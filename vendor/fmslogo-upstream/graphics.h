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

#ifndef __GRAPHICS_H_
#define __GRAPHICS_H_

#ifdef WX_PURE
   typedef unsigned int DWORD;
   #define SRCCOPY 0xCC0020
#else
   #include <windows.h>
#endif

#include <wx/defs.h>   // wxUint32
#include <wx/gdicmn.h> // wxPoint
#include <wx/dc.h>     // wxRasterOperationMode

#include "logocore.h"
#include "vector.h"
#include "3dsolid.h"   // Point

// types
struct Color
{
    int   red;
    int   green;
    int   blue;
};

struct PENSTATE
{
    int          Width;
    struct Color Color;

    wxRasterOperationMode Mode;
    bool                  IsErasing;
};

struct Segment
{
    wxPoint From;
    wxPoint To;
};

struct Turtle
{
    MATRIX   Matrix;
    VECTOR   Position;
    FLONUM   Heading;
    DWORD    BitmapRasterMode; // 0 if not bitmapped.  Windows raster mode, otherwise.
    bool     IsShown;
    bool     IsPenUp;
    bool     IsSpecial;

    bool     IsSprite; // true if the bitmap rotates with the turtle, false otherwise.

    bool     HasOwnPenState;
    PENSTATE PenState;

    // A cache for the points that make up the turtle's body.
    wxPoint  BodyOutline[3];  // closed path (implied line from BodyOutline[2] to BodyOutline[0])
    bool     BodyOutlineIsValid;

    unsigned char TotalSegments;
    Segment       Segments[4]; // for holding non-continuous segments
};

// function declarations
wxUint32 GetColorArgument(NODE* args);
FIXNUM g_round(FLONUM n);
FLONUM numeric_node_to_flonum(const NODE* numeric_node);
FIXNUM numeric_node_to_fixnum(const NODE* numeric_node);
int GetSelectedTurtleIndex();
void InitializeTurtle(struct Turtle * TurtleToInitialize);
NODE *lscrunch(NODE *arg);
void draw_turtle(bool erase);
void draw_turtles(bool erase);
FLONUM nonnegative_flonum_arg(NODE *argument);
FLONUM positive_flonum_arg(NODE *argument);
NODE *lright(NODE *arg);
NODE *lleft(NODE *arg);
NODE *lrightroll(NODE *arg);
NODE *lleftroll(NODE *arg);
NODE *ldownpitch(NODE *arg);
NODE *luppitch(NODE *arg);
FLONUM rotation_z();
FLONUM rotation_y();
FLONUM rotation_x();
NODE *lforward(NODE *arg);
NODE *lback(NODE *arg);
NODE *lshowturtle(NODE *arg);
NODE *lhideturtle(NODE *arg);
NODE *lbitmapturtle(NODE *arg);
NODE *lnobitmapturtle(NODE *arg);
NODE *lshownp(NODE *arg);
NODE *lsetheading(NODE *arg);
NODE *lsetroll(NODE *arg);
NODE *lsetclip(NODE *arg);
NODE *lsetpitch(NODE *arg);
NODE *lsetturtle(NODE *arg);
NODE *lturtle(NODE *arg);
NODE *lturtles(NODE *arg);
NODE *lhasownpenp(NODE *arg);
NODE *lheading(NODE *arg);
NODE *lroll(NODE *arg);
NODE *lpitch(NODE *arg);
NODE *vector_arg(NODE *args);
NODE *int_vector_4_arg(NODE *args);
NODE *pos_int_vector_arg(NODE *args);
NODE *ltowards(NODE *args);
NODE *ltowardsxyz(NODE *args);
NODE *llight(NODE *arg);
NODE *lpos(NODE *arg);
NODE *lposxyz(NODE *arg);
NODE *lhome(NODE *arg);
NODE *lclearscreen(NODE *arg);
NODE *lclean(NODE *arg);
NODE *lsetlight(NODE *args);
NODE *lsetpos(NODE *args);
NODE *lsetposxyz(NODE *args);
NODE *lsetxy(NODE *args);
NODE *lsetxyz(NODE *args);
NODE *lellipsearc(NODE *args);
NODE *lsetx(NODE *args);
NODE *lsety(NODE *args);
NODE *lsetz(NODE *args);
NODE *lwrap(NODE *arg);
NODE *lfence(NODE *arg);
NODE *lwindow(NODE *arg);
NODE *lpolystart(NODE *arg);
NODE *lpolyend(NODE *arg);
NODE *lpolyview(NODE *arg);
NODE *lperspective(NODE *arg);
NODE *lfill(NODE *arg);
NODE *llabel(NODE *arg);
NODE *llabelsize(NODE *arg);
NODE *ltextscreen(NODE *arg);
NODE *lsplitscreen(NODE *arg);
NODE *lfullscreen(NODE *arg);
NODE *lpendownp(NODE *arg);
NODE *lpenmode(NODE *arg);
NODE *lpencolor(NODE *arg);
NODE *lpixel(NODE *arg);
NODE *lsetpixel(NODE *args);
NODE *lfloodcolor(NODE *arg);
NODE *lscreencolor(NODE *arg);
NODE *lpensize(NODE *arg);
NODE *lpenpattern(NODE *arg);
NODE *lpendown(NODE *arg);
NODE *lpenup(NODE *arg);
NODE *lpenpaint(NODE *arg);
NODE *lpenerase(NODE *arg);
NODE *lpenreverse(NODE *arg);
NODE *lclearpalette(NODE *arg);
NODE *lbitload(NODE *arg);
NODE *lbitloadsize(NODE *arg);
NODE *lbitsize(NODE *arg);
NODE *lbitsave(NODE *arg);
NODE *lgifsave(NODE *arg);
NODE *lgifload(NODE *arg);
NODE *lgifsize(NODE *arg);
NODE *lbitcut(NODE *arg);
NODE *lbitcopy(NODE *arg);
NODE *lbitpaste(NODE *arg);
NODE *lbitpastetoindex(NODE *arg);
NODE *lbitmode(NODE *arg);
NODE *lturtlemode(NODE *arg);
NODE *lmachine(NODE *arg);
NODE *lsetbitmode(NODE *arg);
NODE *lsetturtlemode(NODE *arg);
NODE *lbitindex(NODE *arg);
NODE *lsetbitindex(NODE *arg);
NODE *lbitfit(NODE *arg);
NODE *lbitblock(NODE *arg);
NODE *lscrollx(NODE *arg);
NODE *lscrolly(NODE *arg);
NODE *lsetpencolor(NODE *arg);
NODE *lsetfloodcolor(NODE *arg);
NODE *lsetscreencolor(NODE *arg);
NODE *lsetpensize(NODE *args);
NODE *lsetpenpattern(NODE *args);
NODE *lsetscrunch(NODE *args);
NODE *lmousepos(NODE *args);
NODE *lkeyboardvalue(NODE *args);
NODE *lbuttonp(NODE *args);
void keyboard_uninit();
void mouse_uninit();

// global variables
extern long vector_count;

extern mode_type current_mode;

const int COLORTABLESIZE = 16;
extern const wxUint32 colortable[];

extern Turtle * g_Turtles;
extern int      g_MaxTurtle;        // the index of the largest real turtle
extern size_t   g_TurtlesLimit;     // the physical limit of g_Turtles
extern Turtle   g_SpecialTurtles[TOTAL_SPECIAL_TURTLES];
extern Turtle * g_SelectedTurtle;
extern Point    g_Wanna;

#endif // __GRAPHICS_H_
