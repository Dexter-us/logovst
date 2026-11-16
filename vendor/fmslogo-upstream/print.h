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

#ifndef __PRINT_H_
#define __PRINT_H_

#include <stdio.h> // for FILE*
#include "files.h" // for ENCODING

struct NODE;

enum MESSAGETYPE
{
    MESSAGETYPE_Normal, // normal text
    MESSAGETYPE_Trace,  // a TRACE message
    MESSAGETYPE_Error,  // an error message
};

// function declarations
void print_char(FILE *strm, ENCODING encoding, MESSAGETYPE type, wchar_t ch);
void print_space(FILE *strm, ENCODING encoding, MESSAGETYPE type);
void print_newline(FILE *strm, ENCODING encoding, MESSAGETYPE type);
void print_string(FILE *strm, ENCODING encoding, MESSAGETYPE type, const wchar_t * string);

void
ndprintf(
    FILE          * strm,
    ENCODING        encoding,
    MESSAGETYPE     type,
    bool            print_backslashes,
    const wchar_t * fmt,
    ...
    );

int find_limit(const class CLocalizedNode & Node);

void
print_helper(
    FILE       * strm,
    ENCODING     encoding,
    MESSAGETYPE  type,
    bool         print_backslashes,
    const NODE * ndlist
    );

void
print_node(
    FILE        * strm,
    ENCODING      encoding,
    MESSAGETYPE   type,
    bool          print_backslashes,
    const NODE  * nd
    );

bool is_ascii(const NODE * nd);
NODE *lshow(NODE *args);
NODE *ltype(NODE *args);
NODE *lprint(NODE *args);

void InitializeStringPrintInformation(wchar_t * Buffer, size_t BufferLength);
size_t FinalizeStringPrintInformation();

size_t
PrintNodeToString(
    const NODE * Node,
    wchar_t *    Buffer,
    size_t       BufferLength
    );

size_t
PrintNodeToString(
    const NODE * Node,
    wchar_t *    Buffer,
    size_t       BufferLength,
    int          PrintDepthLimit,
    int          PrintWidthLimit
    );

#endif // __PRINT_H_
