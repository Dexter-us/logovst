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

#ifndef __STATUS_H_
#define __STATUS_H_

// functions
void update_status_evals();
void update_status_floodcolor();
void update_status_memory();
void update_status_pencolor();
void update_status_pencontact();
void update_status_penstyle();
void update_status_penwidth();
void update_status_screencolor();
void update_status_turtleheading();
void update_status_turtlepitch();
void update_status_turtleposition();
void update_status_turtleroll();
void update_status_turtlevisability();
void update_status_turtlewhich();
void update_status_vectors();

// global variables
extern bool status_flag;

#endif // __STATUS_H_
