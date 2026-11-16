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

#ifndef __ERROR_H_
#define __ERROR_H_

#include "logocore.h"

void clear_is_running_erract_flag();
void err_print();
NODE *err_logo(ERR_TYPES error_type, NODE *error_desc);
NODE *err_logo(ERR_TYPES error_type, NODE *error_desc, bool uplevel);
NODE *lerror(NODE *arg);
NODE *lpause(NODE *arg);
NODE *lhalt(NODE *arg);
NODE *lyield(NODE *arg);
NODE *lnoyield(NODE *arg);
NODE *lcontinue(NODE *args);
void uninitialize_error();

extern NODE *throw_node;

#endif // __ERROR_H_
