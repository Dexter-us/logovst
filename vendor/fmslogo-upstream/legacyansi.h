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

#ifndef __LEGACY_ANSI_H_
#define __LEGACY_ANSI_H_

#include <stddef.h>

// function declarations
char *    unicode_to_ansi(const wchar_t * UnicodeString);
wchar_t * ansi_to_unicode(const char * AnsiString);

#endif // __LEGACY_ANSI_H_
