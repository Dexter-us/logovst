// -*- c++ -*-
// Copyright (C) 1995 by the Regents of the University of California
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

#ifndef __DLGWND_H_
#define __DLGWND_H_

#include "logocore.h" // for MAX_BUFFER_SIZE

struct NODE;

extern wchar_t edit_editexit[MAX_BUFFER_SIZE]; // editor callback instruction list

NODE *lmessagebox(NODE *args);
NODE *lquestionbox(NODE *args);
NODE *lyesnobox(NODE *args);
NODE *lselectbox(NODE *args);
NODE *ldialogcreate(NODE *args);
NODE *ldialogenable(NODE *args);
NODE *ldialogdelete(NODE *arg);
NODE *lwindowcreate(NODE *args);
NODE *lwindowenable(NODE *args);
NODE *lwindowdelete(NODE *arg);
NODE *lbuttoncreate(NODE *args);
NODE *lbuttonupdate(NODE *args);
NODE *lbuttonenable(NODE *args);
NODE *lbuttondelete(NODE *arg);
NODE *lscrollbarcreate(NODE *args);
NODE *lscrollbarenable(NODE *args);
NODE *lscrollbardelete(NODE *arg);
NODE *lscrollbarset(NODE *args);
NODE *lscrollbarget(NODE *arg);
NODE *llistboxcreate(NODE *args);
NODE *llistboxenable(NODE *args);
NODE *llistboxdelete(NODE *arg);
NODE *llistboxgetselect(NODE *arg);
NODE *llistboxaddstring(NODE *args);
NODE *llistboxdeletestring(NODE *args);
NODE *lcomboboxcreate(NODE *args);
NODE *lcomboboxenable(NODE *args);
NODE *lcomboboxdelete(NODE *arg);
NODE *lcomboboxgettext(NODE *arg);
NODE *lcomboboxsettext(NODE *arg);
NODE *lcomboboxaddstring(NODE *args);
NODE *lcomboboxdeletestring(NODE *args);
NODE *lstaticcreate(NODE *args);
NODE *lstaticupdate(NODE *args);
NODE *lstaticdelete(NODE *arg);
NODE *lgroupboxcreate(NODE *args);
NODE *lgroupboxdelete(NODE *arg);
NODE *lradiobuttoncreate(NODE *args);
NODE *lradiobuttonenable(NODE *args);
NODE *lradiobuttondelete(NODE *arg);
NODE *lradiobuttonset(NODE *args);
NODE *lradiobuttonget(NODE *arg);
NODE *lcheckboxcreate(NODE *args);
NODE *lcheckboxenable(NODE *args);
NODE *lcheckboxdelete(NODE *arg);
NODE *lcheckboxset(NODE *args);
NODE *lcheckboxget(NODE *arg);
NODE *ldialogfileopen(NODE *arg);
NODE *ldialogfilesave(NODE *arg);
NODE *lwindowfileedit(NODE *arg);
void uninitialize_windows();

#endif // __DLGWND_H_

