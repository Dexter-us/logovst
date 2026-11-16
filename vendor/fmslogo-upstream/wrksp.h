/*
 *  wrksp.h      logo workspace management module
 *
 *    Copyright (C) 1993 by the Regents of the University of California
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __WORKSPACE_H_
#define __WORKSPACE_H_

#include <stdio.h> // for fpos_t, FILE*
#include "files.h" // for ENCODING

struct NODE;

// functions
NODE *ltext(NODE *args);
NODE *lfulltext(NODE *args);
NODE *ldefine(NODE *args);
NODE *ldefmacro(NODE *args);
NODE *anonymous_function(NODE *text);
NODE *lto(NODE *args);
NODE *lmacro(NODE *args);
NODE *lmake(NODE *args);
NODE *lcontents(NODE *arg);
NODE *lstepped(NODE *arg);
NODE *ltraced(NODE *arg);
NODE *lburied(NODE *arg);
NODE *lprocedures(NODE *arg);
NODE *lnames(NODE *arg);
NODE *lplists(NODE *arg);
void po(NODE * arg, FILE* output_stream, ENCODING encoding);
bool po_is_ascii(NODE * arg);
NODE *lpo(NODE *arg);
NODE *lpot(NODE *arg);
NODE *lerase(NODE *arg);
NODE *lbury(NODE *arg);
NODE *ltrace(NODE *arg);
NODE *lstep(NODE *arg);
NODE *lunbury(NODE *arg);
NODE *luntrace(NODE *arg);
NODE *lunstep(NODE *arg);
NODE *ledit(NODE *args);
bool endedit();
bool something_is_unburied();
NODE *lthing(NODE *args);
NODE *lnamep(NODE *args);
NODE *lprocedurep(NODE *args);
NODE *lproplistp(NODE *args);
NODE *lprimitivep(NODE *args);
NODE *ldefinedp(NODE *args);
NODE *lmacrop(NODE *args);
NODE *larity(NODE *args);
NODE *lcopydef(NODE *args);
NODE *get_all_proc_names();
NODE *get_all_variable_names();


// global variables
extern bool to_pending;
extern bool yield_flag;
extern bool IsDirty;
extern bool bExpert;

extern long int g_CharactersSuccessfullyParsedInEditor;

#endif // __WORKSPACE_H_
