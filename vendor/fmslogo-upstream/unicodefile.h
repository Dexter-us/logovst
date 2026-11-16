// -*- c++ -*-
// Copyright (C) 2019 by David Costanzo
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

#ifndef __UNICODE_FILE_H_
#define __UNICODE_FILE_H_

#include <stdio.h> // for FILE
#include "files.h" // for ENCODING

// function declarations
bool read_unicode_char(FILE* Stream, ENCODING Encoding, wchar_t * NextChar);
ENCODING read_character_encoding(FILE * Stream);
bool write_unicode_char(FILE* Stream, ENCODING Encoding, wchar_t Character);
bool write_unicode_string(FILE* Stream, ENCODING Encoding, const wchar_t * String);

#endif // __UNICODE_FILE_H_
