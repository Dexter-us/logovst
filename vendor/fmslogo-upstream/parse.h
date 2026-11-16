// Copyright (C) 2005 by David Costanzo

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

#ifndef __PARSER_H_
#define __PARSER_H_

#include <stdio.h> // for FILE
#include "files.h" // for ENCODING

// types
struct NODE;

enum INPUTMODE
{
    INPUTMODE_None,
    INPUTMODE_To,
    INPUTMODE_List,
    INPUTMODE_Pause,
};

// function variables
int rd_getc(FILE * Stream, ENCODING CharacterEncoding);
void rd_clearbuffer(FILE * Stream);
NODE *reader(FILE * Stream, ENCODING CharacterEncoding, const char * Prompt);
NODE *parser(NODE *nd, bool semi);
NODE *lparse(NODE *args);
NODE *runparse(NODE *ndlist);
NODE *lrunparse(NODE *args);
void uninitialize_parser();

// global variables
extern FILE    * loadstream;
extern ENCODING  loadstream_encoding;
extern FILE *dribblestream;
extern enum INPUTMODE input_mode;
extern NODE *deepend_proc_name;
extern NODE *g_ToLine;

#endif // __PARSER_H_
