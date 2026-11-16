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

#ifndef __COMS_H_
#define __COMS_H_

#include "logocore.h"

// function variables
NODE *make_cont(enum labels cont, NODE *val);
NODE *loutput(NODE *arg);
NODE *lstop(NODE *arg);
NODE *lthrow(NODE *arg);
NODE *lcatch(NODE *args);
NODE *lgoto(NODE *arg);
NODE *ltag(NODE *arg);
bool  boolean_arg(NODE *args);
NODE *lnot(NODE *args);
NODE *land(NODE *args);
NODE *lor(NODE *args);
NODE *runnable_arg(NODE *args);
NODE *lif(NODE *args);
NODE *lifelse(NODE *args);
NODE *lrun(NODE *args);
NODE *lrunresult(NODE *args);
NODE *lrepeat(NODE *args);
NODE *lrepcount(NODE *args);
NODE *lforever(NODE *args);
NODE *ltest(NODE *args);
NODE *liftrue(NODE *args);
NODE *liffalse(NODE *args);
NODE *lbye(NODE *args);
NODE *lwait(NODE *args);
NODE *ltime(NODE *args);
NODE *ltimemilli(NODE *args);
NODE *lshell(NODE *args);

// global variables
extern FIXNUM ift_iff_flag;


#endif // __COMS_H_
