/*
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
 */

#include "pch.h"
#ifndef USE_PRECOMPILED_HEADER
    #ifndef WX_PURE
      #include <windows.h>
    #endif

    #include <stdio.h>
    #include <errno.h>

    #include <wx/filefn.h>
    #include <wx/dir.h>

    #include "unix.h"
    #include "init.h"
    #include "logodata.h"
    #include "parse.h"
    #include "commander.h"
    #include "stringprintednode.h"
    #include "dynamicbuffer.h"
    #include "appendablelist.h"
    #include "sort.h"
    #include "debugheap.h"
    #include "internationalizedstrings.h"
#endif

static CDynamicBuffer g_ComboBuffer;

static
void
mputcombobox(
    const wchar_t *str
    )
{
    // Append the string to the buffer
    g_ComboBuffer.AppendString(str);

    // process lines
    wchar_t * rawBuffer       = g_ComboBuffer.GetBuffer();
    size_t    rawBufferLength = g_ComboBuffer.GetBufferLength();

    wchar_t * next_line = rawBuffer;
    for (size_t i = 0; i < rawBufferLength; i++)
    {
        if (rawBuffer[i] == '\n')
        {
            // if <lf> pump it out
            rawBuffer[i] = '\0';
            putcombobox(next_line, MESSAGETYPE_Normal);
            rawBuffer[i] = '\n';
            next_line = &rawBuffer[i + 1];
        }
    }

    // flush the last line (which doesn't end in \n)
    if (next_line[0] != '\0')
    {
        putcombobox(next_line, MESSAGETYPE_Normal);
    }

    // clear the contents of the buffer, since we wrote the entire thing
    g_ComboBuffer.Empty();
}

void putcombochar(MESSAGETYPE type, wchar_t ch)
{
    if (ch == '\n')
    {
        // if <lf> pump it out
        putcombobox(g_ComboBuffer.GetBuffer(), type);
        g_ComboBuffer.Empty();
    }
    else
    {
        g_ComboBuffer.AppendChar(ch);
    }
}


void printfx(const wchar_t *str)
{
    mputcombobox(str);
}

void printfx(const wchar_t *fmt, const wchar_t *str)
{
    // Format the string.
    size_t bufferSize = wcslen(fmt) + (str ? wcslen(str) : 0) + 1;
    wchar_t * buff = new wchar_t [bufferSize];
    swprintf(buff, bufferSize, fmt, str);

    mputcombobox(buff);

    delete [] buff;
}

NODE *lchdir(NODE *arg)
{
    CStringPrintedNode directoryName(car(arg));

    if (wxSetWorkingDirectory(directoryName.GetString()))
    {
        // Success.  Print the new directory to the commander history.
        wxString newDirectoryName(wxGetCwd());
        printfx(g_LOCALIZED_FILE_CHDIRSUCCEEDED, newDirectoryName);
    }
    else
    {
        // Failure.  Print an error to the commander history.
        printfx(g_LOCALIZED_FILE_CHDIRFAILED, directoryName);
    }

    return Unbound;
}

NODE *lpopdir(NODE *)
{
    wxSetWorkingDirectory(L"..");

    wxString fname(wxGetCwd());

    printfx(g_LOCALIZED_FILE_POPPEDTO, fname);

    return Unbound;
}

NODE *lmkdir(NODE *arg)
{
    CStringPrintedNode directoryName(car(arg));

    if (wxMkdir(directoryName.GetString()))
    {
        // wxMkdir returns true on success
        wxSetWorkingDirectory(directoryName.GetString());
        printfx(g_LOCALIZED_FILE_MKDIRSUCCEEDED, directoryName);
    }
    else
    {
        // wxMkdir returns false on error
        printfx(g_LOCALIZED_FILE_MKDIRFAILED, directoryName);
    }

    return Unbound;
}

NODE *lrmdir(NODE *arg)
{
    CStringPrintedNode directoryName(car(arg));

    // Use wxRmDir instead of wxRmdir so that it doesn't show a
    // dialog box on error on GTK builds.
    if (0 == wxRmDir(directoryName.GetString()))
    {
        printfx(g_LOCALIZED_FILE_RMDIRSUCCEEDED, directoryName);
    }
    else
    {
        // Save the reason for the error, since printfx can change errno.
        int saved_errno = errno;

        // Print that RMDIR failed.
        printfx(g_LOCALIZED_FILE_RMDIRFAILED, directoryName);

        // Print the reason that RMDIR failed.
        if (saved_errno == EEXIST)
        {
            printfx(g_LOCALIZED_FILE_RMDIRFAILEDNOEXIST);
        }
        else if (saved_errno == EEXIST || saved_errno == EPERM)
        {
            printfx(g_LOCALIZED_FILE_RMDIRFAILEDNOTEMPTY);
        }
        else
        {
            wxString errorMessage(strerror(saved_errno));
            printfx(errorMessage);
        }
    }


    return Unbound;
}

static
NODE *directory_helper(bool OnlyListDirectories)
{
    wxDir directory(wxGetCwd());

    if (!directory.IsOpened())
    {
        // The current directory couldn't be opened.
        // MSWLogo didn't throw an error in this case.
        // Instead it returned an empty list.
        return NIL;
    }

    CAppendableList directoryContents;

    // Flags for whether the special directories "." and ".." have been found.
    // Because these are special entries, we always want them to be first,
    // even if there's an entry that comes before them alphabetically,
    // like "!MyDirectory".
    bool foundDot    = false;
    bool foundDotDot = false;
 
    int flags = OnlyListDirectories ?
        wxDIR_DIRS  | wxDIR_HIDDEN | wxDIR_DOTDOT :
        wxDIR_FILES | wxDIR_HIDDEN;

    wxString filename;
    bool moreToCome = directory.GetFirst(
        &filename,     // the name of the first file/directory
        wxEmptyString, // all files or directories
        flags);        // files or directories, depending OnlyListDirectories
    while (moreToCome)
    {
        if (OnlyListDirectories && 0 == filename.compare(L"."))
        {
            // This is the "." directory.  Flag it to be added later.
            foundDot = true;
        }
        else if (OnlyListDirectories && 0 == filename.compare(L".."))
        {
            // This is the ".." directory.  Flag it to be added later.
            foundDotDot = true;
        }
        else
        {
            // This is a regular directory entry.  Add it to the list.
            NODE* file = make_strnode(filename.wc_str());
            directoryContents.AppendElement(file);
        }

        // Iterate to the next file/directory
        moreToCome = directory.GetNext(&filename);
    }

    // The order of iteration is not defined by wxDir, so we sort the list
    // in a case-insensitive manner.
    NODE * sortedList = mergesort(directoryContents.GetList(), true);

    // Add "." and ".." to the front of the list, if applicable.
    if (foundDotDot)
    {
        sortedList = cons(make_strnode(L".."), sortedList);
    }
    if (foundDot)
    {
        sortedList = cons(make_strnode(L"."), sortedList);
    }

    return sortedList;
}

NODE *lfiles(NODE *)
{
    return directory_helper(false);
}

NODE *ldirectories(NODE *)
{
    return directory_helper(true);
}

void uninitialize_combobox()
{
    g_ComboBuffer.Dispose();
}
