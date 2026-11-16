/*
 *      files.cpp         logo file management module             dvb
 *
 *       Copyright (C) 1995 by the Regents of the University of California
 *       Copyright (C) 1995 by George Mills
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "pch.h"
#ifndef USE_PRECOMPILED_HEADER
   #include <stdlib.h>
   #include <assert.h>
   #include <setjmp.h>
   #ifndef WX_PURE
      #include <windows.h>
   #endif

   #include <wx/crt.h>  // wxFopen
   #include <wx/msgdlg.h>
   #include <wx/clipbrd.h>
   #include <wx/file.h> // OpenMode

   #include "files.h"
   #include "fmslogo.h"
   #include "fileswnd.h"
   #include "logocore.h"
   #include "init.h"
   #include "parse.h"
   #include "error.h"
   #include "logomath.h"
   #include "eval.h"
   #include "coms.h"
   #include "lists.h"
   #include "wrksp.h"
   #include "print.h"
   #include "unix.h"
   #include "startup.h"
   #include "screenwindow.h"
   #include "cursor.h"
   #include "unicodefile.h"
   #include "debugheap.h"

   #include "internationalizedstrings.h"
#endif

// A file opened by OPENREAD, OPENWRITE, etc.
// This is an inline singly-linked list.
class CFileListNode
{
public:
    CFileListNode * m_Next;
    NODE    * const m_FileNameNode;
    const ENCODING  m_Encoding;
    FILE    * const m_FilePtr;

    CFileListNode(
        NODE    * FileNameNode,
        FILE    * FilePtr,
        ENCODING  Encoding
        );
    ~CFileListNode();

    bool IsNamed(NODE * FileName) const;

private:
    CFileListNode();
};

NODE *current_line = NIL;       // current line to be parsed

static CFileListNode * g_OpenFiles;

CFileStream g_Reader(stdin);
CFileStream g_Writer(stdout);

CFileListNode::CFileListNode(
    NODE     * FileNameNode,
    FILE     * FilePtr,
    ENCODING   Encoding
    ) :
    m_Next(NULL),
    m_FileNameNode(vref(FileNameNode)),
    m_Encoding(Encoding),
    m_FilePtr(FilePtr)
{
}

CFileListNode::~CFileListNode()
{
    // close the file
    fclose(m_FilePtr);

    // release the filename
    deref(m_FileNameNode);
}

bool CFileListNode::IsNamed(NODE* FileName) const
{
    return compare_node(FileName, m_FileNameNode, false) == 0;
}

// Converts a wxWidgets OpenMode to a C fopen mode.
// Always uses a binary mode to guarantee correct behavior from fseek().
static
const wchar_t * OpenModeToBinaryFopenMode(wxFile::OpenMode Mode)
{
    switch (Mode)
    {
    case wxFile::read:
        return L"rb";

    case wxFile::write:
        return L"wb";

    case wxFile::read_write:
        return L"r+b";

    case wxFile::write_append:
        return L"ab";

    default:
        assert(false); // unsupported access mode
        return NULL;
    }
}

// Open a file (or the clipboard) for the requested access mode.
// The file is always opened in binary mode.
//
// If an encoding is requested, indicated by a non-null Encoding
// argument, then upon success, the encoding is read from the file
// and the file position is set to after the unicode signature, if any.
//
// FilenameNode - The name of the file to open, converted to a string.
// Mode         - The access mode in which to open the file.
// Encoding     - On success, for non-write modes, this is set to the
//                encoding of the file that was opened.
//
// Returns:
//   A file pointer, if the file could be opened for the requested access.
//   NULL, if the file could not be opened.  In this case, it's the caller's
//   responsibility to throw the appropriate error.
static
FILE *open_file(NODE *FilenameNode, wxFile::OpenMode Mode, ENCODING * Encoding)
{
    assert(FilenameNode == NIL || getrefcnt(FilenameNode) != 0);

    // Convert the argument into a file name.
    NODE * filenameNode = cnv_node_to_strnode(FilenameNode);
    if (filenameNode == Unbound)
    {
        return NULL;
    }

    wchar_t * fnstr = (wchar_t *) malloc(sizeof(wchar_t) * (getstrlen(filenameNode) + 1));
    clearvbar_strnzcpy(fnstr, getstrptr(filenameNode), getstrlen(filenameNode));

    FILE          * tstrm;
    const wchar_t * filename;
    if (wxStricmp(fnstr, L"clipboard") == 0)
    {
        // The caller wants to open the clipboard.
        // We use a temp file that mirrors the contents of the clipboard.
        filename = TempClipName;

        FILE * clipstrm = wxFopen(filename, L"wb");
        if (clipstrm != NULL)
        {
            // Write a UTF-8 BOM
            write_unicode_char(clipstrm, ENCODING_Utf8, BYTE_ORDER_MARK);

            if (Mode != wxFile::write)
            {
                // The caller wants to read from the clipboard or append to it.
                // Copy the contents of the clipboard to the file.
                if (wxTheClipboard->Open())
                {
                    if (wxTheClipboard->IsSupported(wxDF_TEXT))
                    {
                        // The clipboard contains something that could be
                        // converted to text.  Write that to the file.
                        wxTextDataObject data;
                        wxTheClipboard->GetData(data);
                        wxString text(data.GetText());

                        write_unicode_string(clipstrm, ENCODING_Utf8, text);
                    }
                    else
                    {
                        // The clipboard does not contain text.
                        // Write an error message to the file.
                        if (Mode == wxFile::read)
                        {
                            write_unicode_string(
                                clipstrm,
                                ENCODING_Utf8,
                                g_LOCALIZED_ERROR_CLIPBOARDISEMPTY);
                        }
                    }

                    wxTheClipboard->Close();
                }
            }

            fclose(clipstrm);
        }
    }
    else
    {
        // The caller wants to open a normal file.
        filename = fnstr;
    }

    // Open the file.
    tstrm = wxFopen(filename, OpenModeToBinaryFopenMode(Mode));
    if (tstrm != NULL)
    {
        // We succesfully opened the file.
        if (Encoding != NULL)
        {
            switch (Mode)
            {
            case wxFile::read:
                *Encoding = read_character_encoding(tstrm);
                break;

            case wxFile::write:
                // The file was opened for writing, so the caller will
                // determine the encoding they desire.
                break;

            case wxFile::read_write:
                // The file was opened for reading and writing.
                // If the file has content, we respect its current encoding.
                *Encoding = read_character_encoding(tstrm);
                break;

            case wxFile::write_append:
                // The file was opened for appending.
                // If the file has content, we respect its current encoding.
                // Because the file was opened in "ab" mode, we cannot seek
                // back to the beginning of the file to determine its encoding.
                // Instead, we open another file handle in read mode.
                {
                    FILE * readableFile = wxFopen(filename, L"rb");
                    if (readableFile != NULL)
                    {
                        *Encoding = read_character_encoding(readableFile);
                        fclose(readableFile);
                    }
                    else
                    {
                        // If we couldn't determine the encoding of the file,
                        // then throw an error.  If the Logo program continued
                        // with the wrong encoding, it could cause data corruption.
                        fclose(tstrm);
                        tstrm = NULL;
                    }
                }
                break;

            case wxFile::write_excl:
                assert(false); // can't happen
                break;
            }
        }
    }

    gcref(filenameNode);
    free(fnstr);
    return tstrm;
}

NODE *ldribble(NODE *arg)
{
    if (dribblestream != NULL)
    {
        err_logo(ALREADY_DRIBBLING, NIL);
    }
    else
    {
        dribblestream = open_file(car(arg), wxFile::write, NULL);
        if (dribblestream == NULL)
        {
            err_logo(FILE_ERROR, NIL);
        }
        else
        {
            // The dribble stream is always encoded in UTF-8,
            // just in case non-ASCII content is dribbled.
            // For consistency, write a UTF-8 BOM.
            fwrite(UTF8_BOM, 1, sizeof(UTF8_BOM), dribblestream);
        }
    }

    return Unbound;
}

NODE *lnodribble(NODE *)
{
    if (dribblestream != NULL)
    {
        fclose(dribblestream);
        dribblestream = NULL;
    }
    return Unbound;
}

// Searches the list of open files for the node that matches
// the given filename.
//
// FileNameNode - The name of the file stream to look for.
// Remove       - true, if the file should be unlinked from the
//                list of open files.  false, otherwise.
//                In this case, it's the caller's responsibility
//                to delete the returned node.
static
CFileListNode *
FindOrRemoveFile(
    NODE * FileNameNode,
    bool   Remove
    )
{
    CFileListNode * fileNode = NULL;

    for (CFileListNode ** nodePtr = &g_OpenFiles;
         (*nodePtr) != NULL;
         nodePtr = &(*nodePtr)->m_Next)
    {
        CFileListNode * node = *nodePtr;
        if (node->IsNamed(FileNameNode))
        {
            // Found it. Return the node
            fileNode = node;

            if (Remove)
            {
                // Unlink the node from the list
                *nodePtr = node->m_Next;
            }
            break;
        }
    }

    return fileNode;
}

static
CFileListNode *
FindFile(
    NODE * FileNameNode
    )
{
    return FindOrRemoveFile(FileNameNode, false);
}

static
CFileListNode *
RemoveFile(
    NODE * FileNameNode
    )
{
    return FindOrRemoveFile(FileNameNode, true);
}

// Opens a file with the given mode.
// If opening the file fails, throws a Logo error.
//
// Arguments   - The Logo arguments passed into the file operation.
//               The first argument is the filename.
//               The second argument is the optional "BinaryMode" argument.
// Mode        - The mode to use.
static
NODE *
open_helper(
    NODE              * Arguments,
    wxFile::OpenMode    Mode
    )
{
    bool useBinaryMode = false;

    if (cdr(Arguments) != NIL)
    {
        useBinaryMode = boolean_arg(cdr(Arguments));
    }

    if (NOT_THROWING)
    {
        NODE * fileNameNode = car(Arguments);
        if (FindFile(fileNameNode) != NULL)
        {
            err_logo(
                FILE_ERROR,
                make_static_strnode(g_LOCALIZED_ERROR_FILESYSTEM_ALREADYOPEN));
        }
        else
        {
            FILE * newFileStream;
            ENCODING fileEncoding;
            if (useBinaryMode)
            {
                newFileStream = open_file(fileNameNode, Mode, NULL);
                fileEncoding = ENCODING_Binary;
            }
            else
            {
                newFileStream = open_file(fileNameNode, Mode, &fileEncoding);
            }

            if (newFileStream != NULL)
            {
                // Both OPENUPDATE and OPENAPPEND are documented as starting
                // at end of the file.
                if (Mode == wxFile::read_write || Mode == wxFile::write_append)
                {
                    fseek(newFileStream, 0, SEEK_END);
                }

                if (!useBinaryMode && Mode != wxFile::read)
                {
                    // The file was opened for text writing/appending/updating.
                    long position = ftell(newFileStream);
                    if (position == 0)
                    {
                        // The file is empty.
                        // Since the programmer might want to write arbitrary text,
                        // we set the BOM to UTF-8.
                        fwrite(UTF8_BOM, 1, sizeof(UTF8_BOM), newFileStream);
                        fileEncoding = ENCODING_Utf8;
                    }
                }

                // create a new node for this file
                CFileListNode * newNode = new CFileListNode(
                    fileNameNode,
                    newFileStream,
                    fileEncoding);

                // add this to the front of the list
                newNode->m_Next = g_OpenFiles;
                g_OpenFiles     = newNode;
            }
            else
            {
                err_logo(
                    FILE_ERROR,
                    make_static_strnode(g_LOCALIZED_ERROR_FILESYSTEM_CANTOPEN));
            }
        }
    }

    return Unbound;
}

NODE *lopenread(NODE *args)
{
    return open_helper(args, wxFile::read);
}

NODE *lopenwrite(NODE *args)
{
    return open_helper(args, wxFile::write);
}

NODE *lopenappend(NODE *args)
{
    return open_helper(args, wxFile::write_append);
}

NODE *lopenupdate(NODE *args)
{
    return open_helper(args, wxFile::read_write);
}

NODE *lallopen(NODE *)
{
    NODE * allopen = NIL;
    for (CFileListNode * node = g_OpenFiles; node != NULL; node = node->m_Next)
    {
        allopen = cons(node->m_FileNameNode, allopen);
    }

    return allopen;
}

NODE *lclose(NODE *arg)
{
    NODE * filename = string_arg(arg);
    if (stopping_flag == THROWING)
    {
        return Unbound;
    }

    CFileListNode *fileNode = RemoveFile(filename);
    if (fileNode == NULL)
    {
        err_logo(
            FILE_ERROR,
            make_static_strnode(g_LOCALIZED_ERROR_FILESYSTEM_NOTOPEN));

        return Unbound;
    }

    // If we are closing the active read or write stream,
    // then we reset the stream to the default
    // (either stdin or stdout) so that future
    // reads/writes won't crash.
    if (g_Writer.IsNamed(filename))
    {
        g_Writer.ResetToDefaultStream();
    }

    if (g_Reader.IsNamed(filename))
    {
        g_Reader.ResetToDefaultStream();
    }

    // close the file stream
    delete fileNode;

    // Read the filename node into a string.
    wchar_t * fnstr = (wchar_t *) malloc(sizeof(wchar_t) * (getstrlen(filename) + 1));
    strnzcpy(fnstr, getstrptr(filename), getstrlen(filename));

    if (wxStricmp(fnstr, L"clipboard") == 0)
    {
        // They have closed the "clipboard" file.
        // Copy all of the data written to this file to the clipboard.
        if (wxTheClipboard->Open())
        {
            FILE *clipstrm = wxFopen(TempClipName, L"rb");
            if (clipstrm != NULL)
            {
                // skip past the BOM
                ENCODING fileEncoding = read_character_encoding(clipstrm);

                // Read the entire file into a buffer.
                wxString text;
                wchar_t character;
                while (read_unicode_char(clipstrm, fileEncoding, &character))
                {
                    text.Append(character);
                }

                // Copy the buffer to the clipboard (as text)
                wxTextDataObject * data = new wxTextDataObject(text);
                wxTheClipboard->SetData(data);

                fclose(clipstrm);
            }

            wxTheClipboard->Close();
        }

        wxRemove(TempClipName);
    }

    free(fnstr);

    return Unbound;
}

#ifdef NDEBUG
#  define ASSERT_FILE_STREAM_INVARIANT
#else
#  define ASSERT_FILE_STREAM_INVARIANT AssertInvariant()

void
CFileStream::AssertInvariant() const
{
    // The default stream should be the commander.
    assert(m_DefaultStream == stdin || m_DefaultStream == stdout);

    if (m_FileNode != NULL)
    {
        // The current stream is not the commander.
        assert(0 < getrefcnt(m_FileNode->m_FileNameNode));
        assert(m_FileNode->m_FilePtr != m_DefaultStream);

        switch (m_FileNode->m_Encoding)
        {
        case ENCODING_SystemCodePage:
        case ENCODING_Binary:
            break;

        case ENCODING_Utf8:
            assert(3 <= ftell(m_FileNode->m_FilePtr)); // position in BOM
            break;

        case ENCODING_Utf16Le:
            assert(2 <= ftell(m_FileNode->m_FilePtr)); // position in BOM
            break;

        case ENCODING_NotApplicable:
            assert(false); // N/A is only for commander
            break;
        }
    }
}

#endif // NDEBUG

CFileStream::CFileStream(
    FILE * DefaultFileStream
    ) :
    m_FileNode(NULL),
    m_DefaultStream(DefaultFileStream)
{
    ASSERT_FILE_STREAM_INVARIANT;
}

void
CFileStream::ResetToDefaultStream()
{
    ASSERT_FILE_STREAM_INVARIANT;

    m_FileNode = NULL;

    ASSERT_FILE_STREAM_INVARIANT;
}

void
CFileStream::SetStreamToOpenFile(
    NODE * FileName
    )
{
    ASSERT_FILE_STREAM_INVARIANT;

    CFileListNode * fileNode;

    if (FileName == NIL)
    {
        // reset to the default stream
        ResetToDefaultStream();
    }
    else if ((fileNode = FindFile(FileName)) != NULL)
    {
        // Found the file
        m_FileNode = fileNode;
    }
    else
    {
        err_logo(
            FILE_ERROR,
            make_static_strnode(g_LOCALIZED_ERROR_FILESYSTEM_NOTOPEN));
    }

    ASSERT_FILE_STREAM_INVARIANT;
}

bool
CFileStream::IsNamed(
    NODE * FileName
    ) const
{
    ASSERT_FILE_STREAM_INVARIANT;
    return IsCommander() ? false : m_FileNode->IsNamed(FileName);
}

ENCODING
CFileStream::GetEncoding() const
{
    ASSERT_FILE_STREAM_INVARIANT;

    return IsCommander() ? ENCODING_NotApplicable : m_FileNode->m_Encoding;
}

bool
CFileStream::IsCommander() const
{
    ASSERT_FILE_STREAM_INVARIANT;
    return m_FileNode == NULL;
}

NODE *
CFileStream::GetName() const
{
    ASSERT_FILE_STREAM_INVARIANT;
    return IsCommander() ? NIL : m_FileNode->m_FileNameNode;
}

NODE *
CFileStream::GetPosition() const
{
    ASSERT_FILE_STREAM_INVARIANT;

    long position;
    if (IsCommander())
    {
        // The position of the commander is defined to be -1.
        position = -1;
    }
    else
    {
        // Get the position of the file.
        position = ftell(m_FileNode->m_FilePtr);

        // For READPOS and WRITEPOS, position 0 is the start of the file.
        // To hide the initial BOM character from the logo programmer,
        // we pretend that the position just after the BOM is position 0.
        switch (m_FileNode->m_Encoding)
        {
        case ENCODING_Utf8:
            // The UTF-8 BOM is 3 bytes long
            if (position <= 3)
            {
                position = 0;
            }
            break;

        case ENCODING_Utf16Le:
            // The UTF-16 BOM is 2 bytes long
            if (position <= 2)
            {
                position = 0;
            }
            break;

        case ENCODING_NotApplicable:
        case ENCODING_SystemCodePage:
        case ENCODING_Binary:
            // These encodings have no BOM.
            break;
        }
    }

    return make_intnode(position);
}

FILE *
CFileStream::GetStream() const
{
    ASSERT_FILE_STREAM_INVARIANT;

    return IsCommander() ? m_DefaultStream : m_FileNode->m_FilePtr;
}

void
CFileStream::SetPosition(
    NODE * Arguments
    )
{
    ASSERT_FILE_STREAM_INVARIANT;

    NODE *val = nonnegative_int_arg(Arguments);

    if (NOT_THROWING)
    {
        // Seeking is meaningless for the commander.
        if (!IsCommander())
        {
            // Determine the requested position.
            int position = getint(val);

            // Advance past the BOM if the file has one.
            switch (m_FileNode->m_Encoding)
            {
            case ENCODING_Utf8:
                // The UTF-8 BOM is 3 bytes long
                if (position < 3)
                {
                    position = 3;
                }
                break;

            case ENCODING_Utf16Le:
                // The UTF-16 BOM is 2 bytes long
                if (position < 2)
                {
                    position = 2;
                }
                break;

            case ENCODING_NotApplicable:
            case ENCODING_SystemCodePage:
            case ENCODING_Binary:
                // These encodings have no BOM.
                break;
            }

            // Seek to the requested position.
            fseek(m_FileNode->m_FilePtr, position, SEEK_SET);
        }
    }

    ASSERT_FILE_STREAM_INVARIANT;
}


NODE *lsetwrite(NODE *arg)
{
    g_Writer.SetStreamToOpenFile(car(arg));
    return Unbound;
}

NODE *lsetread(NODE *arg)
{
    g_Reader.SetStreamToOpenFile(car(arg));
    return Unbound;
}

NODE *lreader(NODE *)
{
    return g_Reader.GetName();
}

NODE *lwriter(NODE *)
{
    return g_Writer.GetName();
}

NODE *lerasefile(NODE *arg)
{
    arg = cnv_node_to_strnode(car(arg));
    if (arg == Unbound)
    {
        return Unbound;
    }

    wchar_t * fnstr = (wchar_t *) malloc(sizeof(wchar_t) * (getstrlen(arg) + 1));
    strnzcpy(fnstr, getstrptr(arg), getstrlen(arg));
    wxRemove(fnstr);
    free(fnstr);
    gcref(arg);
    return Unbound;
}


// Prints the unburied contents of the workspace to a given file stream,
// which must have been opened for writing.
// Throws a Logo error is FileStream is NULL.
// Always closes FileStream.
void
PrintWorkspaceToFileStream(
    FILE * FileStream
    )
{
    assert(FileStream != stdout);

    if (FileStream != NULL)
    {
        // never yield during workspace loading
        bool saved_yield_flag = yield_flag;
        yield_flag = false;

        NODE * entire_workspace = vref(cons_list(lcontents(NIL)));

        // Only write a BOM if the workspace isn't 100% ASCII.
        // This is done for compatibility with older versions of
        // FMSLogo and other Logos that would try to run the BOM
        // as a procedure if it saw one.
        ENCODING characterEncoding;
        if (po_is_ascii(entire_workspace))
        {
            characterEncoding = ENCODING_SystemCodePage;
        }
        else
        {
            // There is at least one non-ASCII character.
            // Encode the file in UTF-8 and add a BOM.
            characterEncoding = ENCODING_Utf8;
            fwrite(UTF8_BOM, 1, sizeof(UTF8_BOM), FileStream);
        }
        po(entire_workspace, FileStream, characterEncoding);
        deref(entire_workspace);


        // The workspace was saved, so it's no longer dirty.
        IsDirty = false;

        yield_flag = saved_yield_flag; // restore yield_flag

        fclose(FileStream);
    }
    else
    {
        err_logo(
            FILE_ERROR,
            make_static_strnode(g_LOCALIZED_ERROR_FILESYSTEM_CANTOPEN));
    }
}


NODE *lsave(NODE *arg)
{
    if (IsEditorOpen())
    {
        // Notify the user that the editor is open and that
        // the changes made in that editor won't be saved.
        wxMessageBox(
            g_LOCALIZED_EDITORISOPEN,
            g_LOCALIZED_INFORMATION,
            wxOK | wxCENTRE,
            GetParentWindowForDialog());
    }

    lprint(arg);

    PrintWorkspaceToFileStream(open_file(car(arg), wxFile::write, NULL));

    return Unbound;
}

void runstartup(NODE *oldst)
{
    NODE* st = Startup.GetValue();
    if (st != oldst && st != NIL && is_list(st))
    {
        g_ValueStatus = VALUE_STATUS_NotOk;
        eval_driver(st);

        // process special conditions
        process_special_conditions();
    }
}

void silent_load(NODE *arg, const wchar_t *prefix)
{

    /* This procedure is called three ways:
     *    silent_load(NIL,*argv)    loads *argv
     *    silent_load(proc,logolib) loads logolib/proc
     *    silent_load(proc,NULL)    loads proc.lgo
     * The "/" or ".lgo" is supplied by this procedure as needed.
     */
    if (prefix == NULL && arg == NIL)
    {
        return;
    }

    // construct the filename
#ifdef WX_PURE
#define MAX_PATH (260)
#endif
    wchar_t   filename[MAX_PATH];
    wchar_t * filenamePtr   = filename;
    wchar_t * filenameLimit = filename + ARRAYSIZE(filename) - 1;  // leave room for NUL

    *filenamePtr = '\0';
    if (prefix != NULL)
    {
        size_t prefixLength = wcslen(prefix);
        if (filenameLimit <= filenamePtr + prefixLength)
        {
            // prefix is too long
            return;
        }

        // copy the prefix to the filename
        memcpy(filenamePtr, prefix, prefixLength * sizeof(*prefix));
        filenamePtr += prefixLength;
    }

    if (arg != NIL)
    {
        arg = cnv_node_to_strnode(arg);
        if (arg == Unbound)
        {
            return;
        }

        const wchar_t * argString       = getstrptr(arg);
        int             argStringLength = getstrlen(arg);

        if (filenameLimit <= filenamePtr + argStringLength)
        {
            // prefix is too long
            return;
        }

        // Normalize the case to how we expect it to be on the file system.
        // Using lowercase was inherited from UCBLogo.
        clearvbar_low_strnzcpy(filenamePtr, argString, argStringLength);

        // Replace characters that are legal in procedure names, but illegal
        // in file names (such as '?') with characters that are illegal in
        // procedure names, but legal in file names (such as '+').
        // This permits us to load "?REST" as a Logolib routine.
        for (const wchar_t * limit = filenamePtr + argStringLength;
             filenamePtr < limit;
             filenamePtr++)
        {
            if (*filenamePtr == '?')
            {
                *filenamePtr = '+';
            }
        }

        if (prefix == NULL)
        {
            const size_t EXTENSION_LENGTH = sizeof(".lgo") - 1;
            // this is not coming from Logolib, so append a ".lgo"
            if (filenameLimit <= filenamePtr + EXTENSION_LENGTH)
            {
                // prefix is too long
                return;
            }
            memcpy(filenamePtr, L".lgo", EXTENSION_LENGTH * sizeof(*filenamePtr));
            filenamePtr += EXTENSION_LENGTH;
        }
        gcref(arg);
    }

    // NUL-terminate filename
    *filenamePtr = '\0';

    bool isOk = fileload(filename);
    if (isOk)
    {
        if (stopping_flag == THROWING)
        {
            if (IsMidiRenderMode())
            {
                // A console render cannot open an editor to recover.
                SetMidiRenderFailure();
                stopping_flag = RUN;
            }
            else
            {
                // There was an error parsing this file.
                // Open it in the editor so that it can be debugged.
                stopping_flag = RUN;
                OpenEditorToLocationOfFirstError(filename);
            }
        }
    }
    else
    {
        if (arg == NIL)
        {
            if (IsMidiRenderMode())
            {
                SetMidiRenderFailure();
            }
            // we're loading argv (not from Logolib or current directory)
            // so we should display an error
            ndprintf(
                stdout,
                ENCODING_NotApplicable,
                MESSAGETYPE_Error,
                false, // format for display
                g_LOCALIZED_ERROR_FILESYSTEM_CANTOPEN2,
                prefix);
            print_newline(
                stdout,
                ENCODING_NotApplicable,
                MESSAGETYPE_Error);
        }
    }
}


// CONSIDER for MAINTAINABILITY:
// CONSIDER for SIZE:
// Refactor the common parts of lload() and fileload() into a helper.
NODE *lload(NODE *arg)
{
    NODE *st = Startup.GetValue();
    FIXNUM saved_value_status     = g_ValueStatus;

    bool       isDirtySave  = IsDirty;
    FILE     * tmp          = loadstream;
    ENCODING   tmp_encoding = loadstream_encoding;
    NODE     * tmp_line     = vref(current_line);

    loadstream = open_file(car(arg), wxFile::read, &loadstream_encoding);
    if (loadstream != NULL)
    {
        bool save_yield_flag = yield_flag;
        yield_flag = false;
        lsetcursorwait(NIL);

        start_execution();

        while (!feof(loadstream) && NOT_THROWING)
        {
            assign(current_line, reader(loadstream, loadstream_encoding, ""));
            NODE * exec_list = parser(current_line, true);
            g_ValueStatus = VALUE_STATUS_NotOk;
            eval_driver(exec_list);
        }
        fclose(loadstream);

        lsetcursorarrow(NIL);
        yield_flag = save_yield_flag;

        runstartup(st);
        g_ValueStatus = saved_value_status;

        stop_execution();
    }
    else
    {
        err_logo(
            FILE_ERROR,
            make_static_strnode(g_LOCALIZED_ERROR_FILESYSTEM_CANTOPEN));
    }

    loadstream          = tmp;
    loadstream_encoding = tmp_encoding;
    deref(current_line);
    current_line = tmp_line;
    IsDirty = isDirtySave;
    return Unbound;
}

NODE *lreadlist(NODE *)
{
    input_mode = INPUTMODE_List;
    NODE * val = parser(reader(g_Reader.GetStream(), g_Reader.GetEncoding(), ""), false);
    input_mode = INPUTMODE_None;
    if (feof(g_Reader.GetStream()) && val == NIL)
    {
        gcref(val);
        return Null_Word;
    }
    return val;
}

NODE *lreadword(NODE *)
{
    NODE * val = reader(g_Reader.GetStream(), g_Reader.GetEncoding(), "RW");  // fake prompt flags no auto-continue
    if (feof(g_Reader.GetStream()) && getstrlen(val) == 0)
    {
        gcref(val);
        return NIL;
    }
    return val;
}

NODE *lreadrawline(NODE *)
{
    NODE *val = reader(g_Reader.GetStream(), g_Reader.GetEncoding(), "RAW"); // fake prompt flags no specials
    if (feof(g_Reader.GetStream()) && getstrlen(val) == 0)
    {
        gcref(val);
        return NIL;
    }

    return val;
}

NODE *lreadchar(NODE *)
{
    wchar_t c;
    if (g_Reader.GetStream() == stdin)
    {
        c = (wchar_t) rd_getc(stdin, ENCODING_NotApplicable);
    }
    else
    {
        read_unicode_char(g_Reader.GetStream(), g_Reader.GetEncoding(), &c);
    }

    if (feof(g_Reader.GetStream()))
    {
        return NIL;
    }

    if (g_Reader.GetEncoding() == ENCODING_Binary)
    {
        // cast to unsigned int so that this doesn't return a negative number.
        return make_intnode(static_cast<unsigned int>(c));
    }
    else
    {
        return make_strnode(
            &c,
            1,
            is_vbarred(c) ? STRING : BACKSLASH_STRING,
            strnzcpy);
    }
}

NODE *lreadchars(NODE *args)
{
    NODETYPE type = STRING;

    int totalCharactersRequested = getint(nonnegative_int_arg(args));
    if (stopping_flag == THROWING)
    {
        return Unbound;
    }

    // READCHARS is documented to always return [] when the read
    // stream is set to the commander.
    if (g_Reader.GetStream() == stdin)
    {
        return NIL;
    }

    // To hold all requested chars, a null terminator, and the header, we need
    // to allocate
    //   sizeof(wchar_t) * (totalCharactersRequested + 1) + sizeof(short) bytes.
    //
    // On a 32-bit machine, if X is a signed 32-bit integer, then computing
    // "sizeof(wchar_t)*(X+1) + sizeof(short)" can overflow when:
    //
    //   INT_MAX                                         < sizeof(wchar_t)*(X+1) + sizeof(short)
    //   INT_MAX - sizeof(short)                         < sizeof(wchar_t)*(X+1)
    //  (INT_MAX - sizeof(short)) / sizeof(wchar_t)      <                 (X+1)
    // ((INT_MAX - sizeof(short)) / sizeof(wchar_t)) - 1 < X
    const int OVERFLOW_THRESHOLD = ((INT_MAX - sizeof(short)) / sizeof(wchar_t)) - 1;
    if (OVERFLOW_THRESHOLD < totalCharactersRequested)
    {
        // An overflow would occur when determining the buffer size.
        err_logo(OUT_OF_MEM, NIL);
        return Unbound;
    }

    int totalCharactersRead = 0;
    wchar_t *strptr;
    wchar_t *strhead;

    // TODO: Don't allocate more bytes than the file contains.
    // This would allocate more memory that needed when given a large
    // number of requested characters.
    // Allocate just enough space to hold the "header character", the
    // string, and the NUL terminator.
    void * buffer = malloc(sizeof(wchar_t) * (totalCharactersRequested + 2));
    if (buffer == NULL)
    {
        err_logo(OUT_OF_MEM, NIL);
        return Unbound;
    }

    strhead = static_cast<wchar_t*>(buffer);
    strptr = strhead + 1;
    setstrrefcnt(strhead, 0); // is set to 1 in make_strnode_no_copy

    // Reading a requested number of characters requires reading an
    // unknown number of bytes.  We read one character at a time and
    // fill the buffer.
    while (totalCharactersRead < totalCharactersRequested)
    {
        wchar_t nextChar;
        if (!read_unicode_char(
                g_Reader.GetStream(),
                g_Reader.GetEncoding(),
                &nextChar))
        {
            // There was an error reading the character.
            // Possibly the file ended.
            break;
        }

        strptr[totalCharactersRead++] = nextChar;
    }

    if (stopping_flag == THROWING)
    {
        free(strhead);
        return Unbound;
    }

    if (totalCharactersRead == 0)
    {
        // We read zero characters.  This may be because we hit EOF or
        // because we requested zero characters.  If both are true, we
        // want to return [] when we're at EOF.
        if (totalCharactersRequested == 0)
        {
            // Probe to see if we're at the end of the file.
            // This will update feof().
            ungetc(getc(g_Reader.GetStream()), g_Reader.GetStream());
        }

        if (feof(g_Reader.GetStream()))
        {
            // We reached the end of the file.
            free(strhead);
            return NIL;
        }
    }

    // Check if this string has special characters in it
    for (int i = 0; i < totalCharactersRead; i++)
    {
        if (is_vbarred(strptr[i]))
        {
            type = BACKSLASH_STRING;
            break;
        }
    }

    strptr[totalCharactersRead] = '\0';
    return make_strnode_no_copy(strptr, strhead, totalCharactersRead, type);
}

NODE *leofp(NODE *)
{
    ungetc(getc(g_Reader.GetStream()), g_Reader.GetStream());
    int isEof = feof(g_Reader.GetStream());
    return true_or_false(isEof);
}

NODE *lkeyp(NODE *)
{
    if (g_Reader.GetStream() == stdin)
    {
        return Truex.GetNode();
    }

    return leofp(NIL);
}

NODE *lreadpos(NODE *)
{
    return g_Reader.GetPosition();
}

NODE *lsetreadpos(NODE *arg)
{
    g_Reader.SetPosition(arg);
    return Unbound;
}

NODE *lwritepos(NODE *)
{
    return g_Writer.GetPosition();
}

NODE *lsetwritepos(NODE *arg)
{
    g_Writer.SetPosition(arg);
    return Unbound;
}

NODE *lcloseall(NODE *)
{
    // Reset reader/writer streams to commander, as in lclose().
    g_Reader.ResetToDefaultStream();
    g_Writer.ResetToDefaultStream();

    // close all open file pointers
    while (g_OpenFiles != NULL)
    {
        CFileListNode * nextNode = g_OpenFiles->m_Next;
        delete g_OpenFiles;
        g_OpenFiles = nextNode;
    }

    return Unbound;
}

void uninitialize_files()
{
    lcloseall(NIL);
}
