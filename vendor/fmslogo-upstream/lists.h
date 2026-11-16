/*
 *  lists.h
 *
 *  Copyright (C) 1993 by the Regents of the University of California
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef __LISTS_H_
#define __LISTS_H_

typedef bool (*IS_VALID_ARGUMENT) (NODE * argument);

NODE *validate_argument(NODE * argument, IS_VALID_ARGUMENT is_valid_argument);
NODE *validate_list_argument(NODE *argument);
NODE *validate_integer_argument(NODE * argument);
NODE *lbutfirst(NODE *args);
NODE *lbutlast(NODE *args);
NODE *lfirst(NODE *args);
NODE *lfirsts(NODE *args);
NODE *lbutfirsts(NODE *args);
NODE *llast(NODE *args);
NODE *llist(NODE *args);
NODE *lemptyp(NODE *arg);
NODE *lascii(NODE *args);
NODE *lrawascii(NODE *args);
NODE *lbackslashedp(NODE *args);
NODE *lchar(NODE *args);
NODE *lcount(NODE *args);
NODE *lfput(NODE *args);
NODE *llput(NODE *args);
NODE *string_arg(NODE *args);
NODE *lword(NODE *args);
NODE *lsentence(NODE *args);
NODE *lwordp(NODE *arg);
NODE *llistp(NODE *arg);
NODE *lnumberp(NODE *arg);
NODE *larrayp(NODE *arg);
NODE *lmemberp(NODE *args);
NODE *lsubstringp(NODE *args);
NODE *lmember(NODE *args);
NODE *ranged_integer_arg(NODE *args, int MinValue, int MaxValue);
NODE *nonnegative_int_arg(NODE *args);
NODE *positive_int_arg(NODE *args);
FIXNUM int_arg(NODE *args);
FLONUM flonum_arg(NODE *argument);
NODE *litem(NODE *args);
NODE *lsetitem(NODE *args);
NODE *l_setitem(NODE *args);
NODE *larray(NODE *args);
NODE *lform(NODE *args);
NODE *l_setfirst(NODE *args);
NODE *l_setbf(NODE *args);

#endif // __LISTS_H_
