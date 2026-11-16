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

#ifndef __MULTIMEDIA_WINDOW_H_
#define __MULTIMEDIA_WINDOW_H_

// types
struct NODE;

// global variables
extern wchar_t mci_callback[];

// function declarations
NODE *lmci(NODE *arg);
NODE *lmidiopen(NODE *arg);
NODE *lmidiclose(NODE *arg);
NODE *lmidimessage(NODE *arg);
NODE *lsound(NODE *arg);
NODE *ltone(NODE *args);
NODE *lplaywave(NODE *args);

#endif // __MULTIMEDIA_WINDOW_H_
