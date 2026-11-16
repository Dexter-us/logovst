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

#ifndef __GRAPHWIN_H_
#define __GRAPHWIN_H_

#include <wx/gdicmn.h> // wxRect

#include "logocore.h"
#include "graphics.h"

// global variable declarations
extern wxRect g_FullRect;

extern int xoffset;
extern int yoffset;

extern int BaseUnitsx;
extern int BaseUnitsy;

extern bool IsTimeToExit;
extern bool IsTimeToPause;
extern bool IsTimeToHalt;

extern bool zoom_flag;

extern FLONUM the_zoom;

#ifdef __WXMSW__
  extern LOGFONT FontRec;
#else
  extern wxFontInfo g_LabelFontInfo;
#endif

#ifndef WX_PURE
  extern HBITMAP MemoryBitMap;
#endif // WX_PURE

extern wxPen g_NormalPen;
extern wxPen g_ErasePen;

// function declarations
ERR_TYPES gifsave_helper(const wchar_t *GifFileName, int iDelay_, int bAppendMode_, int iLoop_, int iTrans, int iMaxColorDepth);
ERR_TYPES gifload_helper(const wchar_t *GifFileName, unsigned int &dwPixelWidth, unsigned int &dwPixelHeight);
bool HtmlHelpInitialize();
void HtmlHelpUninitialize();
NODE *lstatus(NODE *arg);
NODE *lnostatus(NODE *arg);
void zoom_helper(FLONUM temp_zoom);
void UpdateZoomControlFlag();
NODE *lzoom(NODE *arg);
NODE *lwinhelp(NODE *arg);
NODE *lhelp(NODE *arg);
NODE *lsetlabelfont(NODE *arg);
NODE *lfontfacenames(NODE *arg);
NODE *llabelfont(NODE *arg);
NODE *lsetfocus(NODE *args);
NODE *lgetfocus(NODE *arg);
NODE *lwindowset(NODE *args);
void logofill(bool fillUntilPenColor);
wxSize labelsize(const wchar_t * String);
void label(const wchar_t *s);
void UpdateErasePen(int Width, RGBCOLOR Color);
void UpdateNormalPen(int Width, RGBCOLOR Color);
void ChangeActivePenColor(int Red, int Green, int Blue);
void ChangeActiveFloodColor(int Red, int Green, int Blue);
void ChangeActiveScreenColor(int Red, int Green, int Blue);
void init_penstate();
void init_bitmaps();
void uninit_bitmaps();
void init_turtles();
void uninit_turtles();
void init_videomode();
void ibmturt(bool hide);
void do_help(const wchar_t *arg);
void ibm_clear_screen();
int get_pen_width();
void set_pen_width(int w);
int get_pen_height();
void set_pen_height(int h);
void GetWorkingAreaDimensions(int & workingAreaWidth, int & workingAreaHeight);
struct PENSTATE & GetPenStateForSelectedTurtle();
void paste_all_turtles(class wxDC & DeviceContext, FLONUM zoom);
void exit_program();
void MyMessageScan();
void checkwindow(int *x, int *y, int *w, int *h);

#endif // __GRAPHWIN_H_
