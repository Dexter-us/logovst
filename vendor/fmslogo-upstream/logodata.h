/*
 *  logodata.h      intern header file
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

#ifndef __LOGODATA_H_
#define __LOGODATA_H_

#include "logocore.h"

wchar_t vbar_set(wchar_t ch);
wchar_t vbar_clear(wchar_t ch);
bool is_vbarred(wchar_t ch);
bool is_special_character(wchar_t ch);
void strnzcpy(wchar_t *dst, const wchar_t *src, int len);
void word_strnzcpy(wchar_t * dst, NODE *kludge, int len);
void clearvbar_strnzcpy(wchar_t * dst, const wchar_t * src, int len);
void mend_strnzcpy(wchar_t * dst, const wchar_t * src, int len);
void mend_nosemi(wchar_t * dst, const wchar_t * src, int len);
void quote_strnzcpy(wchar_t * dst, const wchar_t * src, int len);
void colon_strnzcpy(wchar_t * dst, const wchar_t * src, int len);
wchar_t capital(wchar_t ch);
wchar_t uncapital(wchar_t ch);
void low_strnzcpy(wchar_t * dst, const wchar_t * src, int len);
void cap_strnzcpy(wchar_t * dst, const wchar_t * src, int len);
void NormalizeCaseForDisplay(wchar_t *dst, const wchar_t * src, int len);
void clearvbar_low_strnzcpy(wchar_t * dst, const wchar_t * src, int len);
int low_strncmp(const wchar_t * s1, const wchar_t * s2, int len);
int clearvbar_strncmp(const wchar_t * s1, const wchar_t * s2, int len);
int clearvbar_low_strncmp(const wchar_t * s1, const wchar_t * s2, int len);
NODE *make_strnode_no_copy(const wchar_t *strptr, wchar_t *strhead, int len, NODETYPE typ);
NODE *make_strnode(const wchar_t *string, int len, NODETYPE typ, void (*copy_routine) (wchar_t *, const wchar_t *, int));
NODE *make_strnode(const wchar_t *string);
NODE *make_strnode_from_wordlist(NODE * wordlist, int len, NODETYPE typ);
NODE *make_lowercase_strnode_from_strnode(NODE * OriginalNode);
void make_runparse(NODE *ndi);
NODE *make_quote(NODE *qnd);
NODE *maybe_quote(NODE *nd);
NODE *make_caseobj(NODE *cstrnd, NODE *obj);
NODE *make_colon(NODE *cnd);
NODE *make_intnode(FIXNUM i);
NODE *make_floatnode(FLONUM f);
NODE *cnv_node_to_intnode(NODE *ndi);
NODE *cnv_node_to_numnode(NODE *ndi);
NODE *cnv_node_to_strnode(NODE *nd);
NODE *make_static_strnode(const wchar_t *strptr);
NODE *cons(NODE *x, NODE *y);
NODE *cons_list();
NODE *cons_list(NODE * node1);
NODE *cons_list(NODE * node1, NODE * node2);
NODE *cons_list(NODE * node1, NODE * node2, NODE * node3);
NODE *cons_list(NODE * node1, NODE * node2, NODE * node3, NODE * node4);
NODE *cons_list(NODE * node1, NODE * node2, NODE * node3, NODE * node4, NODE * node5);
int list_length(const NODE * List);
NODE *make_array(int len);
NODE *llowercase(NODE *args);
NODE *luppercase(NODE *args);
NODE *getprop(NODE *plist, NODE *name, bool before);
NODE *lgprop(NODE *args);
NODE *lpprop(NODE *args);
NODE *lremprop(NODE *args);
NODE *lplist(NODE *args);
NODE *llogoversion(NODE *args);
bool variableIsTrue(NODE *variable);
bool isCaseIgnored();
bool canRedefinePrimitives();

#endif // __LOGODATA_H_
