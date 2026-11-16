/*
 *  math.h
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
 */

#ifndef __MATH_H_
#define __MATH_H_

int numberp(NODE *snd);
NODE *lrandom(NODE *arg);
NODE *lrerandom(NODE *arg);
NODE *ladd(NODE *args);
NODE *lsub(NODE *args);
NODE *lmul(NODE *args);
NODE *ldivide(NODE *args);
NODE *lremainder(NODE *args);
NODE *lmodulo(NODE *args);
NODE *lbitand(NODE *args);
NODE *lbitor(NODE *args);
NODE *lbitxor(NODE *args);
NODE *lashift(NODE *args);
NODE *llshift(NODE *args);
NODE *lbitnot(NODE *args);
NODE *lsin(NODE *args);
NODE *larcsin(NODE *args);
NODE *lcos(NODE *args);
NODE *larccos(NODE *args);
NODE *ltan(NODE *args);
NODE *larctan(NODE *args);
NODE *lradsin(NODE *args);
NODE *lradarcsin(NODE *args);
NODE *lradcos(NODE *args);
NODE *lradarccos(NODE *args);
NODE *lradtan(NODE *args);
NODE *lradarctan(NODE *args);
NODE *lsqrt(NODE *args);
NODE *linteg(NODE *args);
NODE *lround(NODE *args);
NODE *lexp(NODE *args);
NODE *llog10(NODE *args);
NODE *lln(NODE *args);
NODE *lpower(NODE *args);
NODE *true_or_false(bool tf);
NODE *llessp(NODE *args);
NODE *llessequalp(NODE *args);
NODE *lgreaterp(NODE *args);
NODE *lgreaterequalp(NODE *args);
int compare_node(NODE *n1, NODE *n2, bool ignorecase);
bool equalp_help(NODE *arg1, NODE *arg2, bool ingc);
NODE *lequalp(NODE *args);
NODE *lnotequalp(NODE *args);
NODE *l_eq(NODE *args);
NODE *lbeforep(NODE *args);

#endif // __MATH_H_
