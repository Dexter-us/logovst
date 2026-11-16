// -*- c++ -*-
// Copyright (C) 1995 by George Mills
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#ifndef __FILES_H_
#define __FILES_H_

#include <stdio.h>
#include <assert.h>

struct NODE;
class CFileListNode;

// types
enum ENCODING
{
    // The encoding is not applicable because the stream is not a byte stream.
    // This is appropriate for when the "stream" is really a string buffer,
    // the commander history, or some other dialog box.
    //
    // This is used to explicitly declare that the encoding doesn't matter.
    // It's an error to pass this to a function where the encoding does matter.
    ENCODING_NotApplicable,

    // The content is text encoded in the system code page.
    // This is for backward compatibility with how UCBLogo and MSWLogo writes files.
    ENCODING_SystemCodePage,

    // The content is binary data, not text data.
    // The content is encoded as a stream of bytes, from 0 - 255.
    ENCODING_Binary,

    // The content is Unicode text encoded in UTF-8.
    ENCODING_Utf8,

    // The content is Unicode text encoded in UTF-16 little-endian order.
    ENCODING_Utf16Le,
};


// CFileStream is the class which implements the current reader or writer.
// It is not a class for an abritrary open file.
class CFileStream
{
public:
    CFileStream(FILE * DefaultStream);

    void       SetStreamToOpenFile(NODE * FileName);
    void       ResetToDefaultStream();
    bool       IsNamed(NODE * FileName) const;
    ENCODING   GetEncoding() const;
    NODE     * GetName() const;
    NODE     * GetPosition() const;
    void       SetPosition(NODE * Arguments);
    FILE     * GetStream() const;

private:
    bool IsCommander() const;
    void AssertInvariant() const;

    CFileListNode *       m_FileNode;      // real file
    FILE          * const m_DefaultStream; // stdin or stdout
};

// global variables
extern NODE *current_line;

// function declarations
void PrintWorkspaceToFileStream(FILE * FileStream);
NODE *ldribble(NODE *arg);
NODE *lnodribble(NODE *arg);
NODE *lopenread(NODE *arg);
NODE *lopenwrite(NODE *arg);
NODE *lopenappend(NODE *arg);
NODE *lopenupdate(NODE *arg);
NODE *lallopen(NODE *arg);
NODE *lclose(NODE *arg);
NODE *lsetwrite(NODE *arg);
NODE *lsetread(NODE *arg);
NODE *lreader(NODE *arg);
NODE *lwriter(NODE *arg);
NODE *lerasefile(NODE *arg);
NODE *lsave(NODE *arg);
void runstartup(NODE *oldst);
void silent_load(NODE *arg, const wchar_t *prefix);
NODE *lload(NODE *arg);
NODE *lreadlist(NODE *arg);
NODE *lreadword(NODE *arg);
NODE *lreadrawline(NODE *arg);
NODE *lreadchar(NODE *arg);
NODE *lreadchars(NODE *args);
NODE *leofp(NODE *arg);
NODE *lkeyp(NODE *arg);
NODE *lreadpos(NODE *arg);
NODE *lsetreadpos(NODE *arg);
NODE *lwritepos(NODE *arg);
NODE *lsetwritepos(NODE *arg);
NODE *lcloseall(NODE * arg);
void uninitialize_files();

// global variables
extern CFileStream g_Reader;
extern CFileStream g_Writer;

#endif // __FILES_H_
