/*
 *  Copyright (C) 1995 by the Regents of the University of California
 *  Copyright (C) 1995 by George Mills
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

#include "pch.h"
#ifndef USE_PRECOMPILED_HEADER
   #include <math.h>
   #include <stdlib.h>
   #include <assert.h>
   #include <algorithm>

   #include <wx/font.h>
   #include <wx/fontutil.h>
   #include <wx/crt.h>
   #include <wx/clipbrd.h>
   #include <wx/window.h>
   #include <wx/dc.h>
   #include <wx/dcmemory.h>
   #include <wx/pen.h>
   #include <wx/brush.h>
   #include <wx/help.h>
   #include <wx/msgdlg.h>
   #include <wx/filefn.h>
   #include <wx/fontenum.h>
   #include <wx/utils.h>
   #include <wx/settings.h>

   #ifdef WX_PURE
      typedef struct __BITMAP      * HBITMAP;
      typedef struct __WND         * HWND;

      typedef int LONG;

      struct RECT {
          int left;
          int top;
          int right;
          int bottom;
      };

      #include <wx/gdicmn.h>

   #else
      #include <windows.h>

      #ifdef max // MS compilers #define max in windows.h
         #undef max
      #endif

      #ifdef min // MS compilers #define min in windows.h
         #undef min
      #endif

   #endif

   #include "screen.h"
   #include "mainframe.h"
   #include "activearea.h"
   #include "graphwin.h"
   #include "mainwind.h"

   #include "devwind.h"
   #include "cursor.h"
   #include "status.h"

   #include "utils.h"
   #include "fontutils.h"
   #include "stringprintednode.h"
   #include "appendablelist.h"
   #include "logomath.h"
   #include "logodata.h"
   #include "vector.h"
   #include "parse.h"
   #include "error.h"
   #include "fmslogo.h" // BitMapWidth, BitMapHeight
   #include "wrksp.h"
   #include "lists.h"
   #include "eval.h"
   #include "init.h"
   #include "coms.h"
   #include "unix.h"
   #include "const.h"
   #include "threed.h"
   #include "screenwindow.h"
   #include "commander.h"
   #include "startup.h" // g_HelpFileName
   #include "debugheap.h"

   #include "internationalizedstrings.h"
#endif

struct CUTMAP
{
    wxBitmap * Bitmap;   // Used to store the bitmap
    RGBCOLOR * Pixels;   // The bitmap in a flat buffer (32bpp)
};


#define X_ROTATED(X, Y, COSINE, SINE) ((X)*(COSINE) + (Y)*(SINE))
#define Y_ROTATED(X, Y, COSINE, SINE) ((Y)*(COSINE) - (X)*(SINE))

extern bool bIndexMode;

int gbmBmpToGif(const wchar_t *BmpName, const wchar_t *GifName);
int gbmGifToBmp(const wchar_t *GifName, const wchar_t *BmpName);

// global variables
int iDelay;
int bAppendMode;
int iLoop;
int iTrans;

wxRect g_FullRect;                     // rectangle of the full screen

#ifdef __WXMSW__
  LOGFONT FontRec;                     // record for label font
#else
  wxFontInfo g_LabelFontInfo;
#endif // __WXMSW__

wxPen g_NormalPen;                     // Handle to "Normal" Pen
wxPen g_ErasePen;                      // Handle to "Erase" Pen

int xoffset = 0;                       // Used to go from logo to windows coords x
int yoffset = 0;                       // Used to go from logo to windows coords y

int BaseUnitsx = 0;                    // X Units Windows uses to for units in dialog
int BaseUnitsy = 0;                    // Y Units Windows uses to for units in dialog

bool IsTimeToExit = false;             // Flag to signal it's time to exit
bool IsTimeToPause = false;            // UCBLOGO? pause flag
bool IsTimeToHalt = false;             // UCBLOGO? halt flag
bool zoom_flag = false;                // flag to signal in zoomed state
FLONUM the_zoom = 1.0;                 // current zoom factor

HBITMAP MemoryBitMap;                  // Backing store bitmap

static wxHelpController * g_HelpController;
static wxWindow         * g_HelpParent;

static CUTMAP * g_SelectedBitmap;
static CUTMAP * g_Bitmaps;               // an array of bitmaps
static int      g_BitmapsLimit;          // number of bitmaps in g_Bitmaps array

static PENSTATE g_PenState;              // The state of the current pen (color, mode, etc.)

struct font_find_t
{
    const wchar_t * fontname;
    bool            found;
};

// The points in the triange which make up the turtle's outline.
// This is a closed shape.
static const VECTOR g_TurtleBodyOutline[] =
{
    {-16.0, 0.0,  0.0},
    { 16.0, 0.0,  0.0},
    {  0.0, 16.0, 0.0},
};

// The line segment drawn on the turtle's right side when
// FMSLogo is in PERSPECTIVE mode.
static const VECTOR g_TurtlePerspectiveModeIndicator[] =
{
    { 8.0, 0.0, 0.0},
    { 8.0, 8.0, 0.0},
};

static DWORD g_BitMode = SRCCOPY;

#ifdef NDEBUG
#  define ASSERT_TURTLE_INVARIANT
#else
#  define ASSERT_TURTLE_INVARIANT CTurtlesInvariant invariant

class CTurtlesInvariant
{
public:

    CTurtlesInvariant()
    {
        AssertInvariant();
    }

    ~CTurtlesInvariant()
    {
        AssertInvariant();
    }

    void
    AssertInvariant(
        void
        )
    {
        // make sure g_MaxTurtle is within range
        assert(0 <= g_MaxTurtle);
        assert(g_MaxTurtle < g_TurtlesLimit);

        // we can never have less than one turtle
        assert(0 < g_TurtlesLimit);

        // g_SelectedTurtle is in either g_Turtles or g_SpecialTurtles
        assert(
            (g_Turtles <= g_SelectedTurtle && g_SelectedTurtle <= &g_Turtles[g_MaxTurtle]) ||
            (g_SpecialTurtles <= g_SelectedTurtle && g_SelectedTurtle < &g_SpecialTurtles[TOTAL_SPECIAL_TURTLES]));

        for (volatile int i = 0; i <= g_MaxTurtle; i++)
        {
            // test that all values are valid (none are uninitialized)
            assert(
                g_Turtles[i].IsShown == true ||
                g_Turtles[i].IsShown == false);

            assert(
                g_Turtles[i].IsPenUp == true ||
                g_Turtles[i].IsPenUp == false);

            assert(
                g_Turtles[i].IsSpecial == true ||
                g_Turtles[i].IsSpecial == false);

#ifndef WX_PURE
            assert(
                g_Turtles[i].BitmapRasterMode == 0           ||
                g_Turtles[i].BitmapRasterMode == SRCCOPY     ||
                g_Turtles[i].BitmapRasterMode == SRCPAINT    ||
                g_Turtles[i].BitmapRasterMode == SRCAND      ||
                g_Turtles[i].BitmapRasterMode == SRCINVERT   ||
                g_Turtles[i].BitmapRasterMode == SRCERASE    ||
                g_Turtles[i].BitmapRasterMode == NOTSRCCOPY  ||
                g_Turtles[i].BitmapRasterMode == NOTSRCERASE ||
                g_Turtles[i].BitmapRasterMode == MERGEPAINT  ||
                g_Turtles[i].BitmapRasterMode == DSTINVERT);
#endif
            if (g_Turtles[i].IsSprite)
            {
                // Even though the raster mode is not used,
                // it is set to indicate that the turtle
                // is drawn with a bitmap.
                assert(g_Turtles[i].BitmapRasterMode != 0);

                // For now, require that all sprites have a
                // default mode.
                assert(g_Turtles[i].BitmapRasterMode == SRCCOPY);
            }

            assert(g_Turtles[i].TotalSegments <= ARRAYSIZE(g_Turtles[i].Segments));
        }

        // g_SelectedBitmap is within the g_Bitmaps array
        assert(g_Bitmaps <= g_SelectedBitmap);
        assert(g_SelectedBitmap < g_Bitmaps + g_BitmapsLimit);

        assert(0 < g_BitmapsLimit);

#ifndef WX_PURE
        assert(
            g_BitMode == SRCCOPY     ||
            g_BitMode == SRCPAINT    ||
            g_BitMode == SRCAND      ||
            g_BitMode == SRCINVERT   ||
            g_BitMode == SRCERASE    ||
            g_BitMode == NOTSRCCOPY  ||
            g_BitMode == NOTSRCERASE ||
            g_BitMode == MERGEPAINT  ||
            g_BitMode == DSTINVERT);
#endif
    }
};

#endif

static
bool
ClipboardIsSelectedBitmap()
{
    return g_SelectedBitmap == &g_Bitmaps[0];
}

static
void
GrowBitmapsArray(int NewSize)
{
    // assert that the array needs to grow
    assert(NewSize > g_BitmapsLimit);

    if (NewSize > (size_t)-1 / sizeof(*g_Bitmaps))
    {
        // this allocation would result in an integer overflow
        err_logo(OUT_OF_MEM, NIL);
        return;
    }

    CUTMAP * newBitmaps = (CUTMAP*) realloc(g_Bitmaps, NewSize * sizeof(*g_Bitmaps));
    if (newBitmaps == NULL)
    {
        // could not grow the bitmaps array to NewSize.
        err_logo(OUT_OF_MEM, NIL);
        return;
    }

    // initialize the newly allocated bitmap structures
    for (int i = g_BitmapsLimit; i < NewSize; i++)
    {
        newBitmaps[i].Bitmap = NULL;
        newBitmaps[i].Pixels = NULL;
    }

    g_Bitmaps      = newBitmaps;
    g_BitmapsLimit = NewSize;
}

void
UpdateErasePen(
    int      Width,
    RGBCOLOR Color
    )
{
    g_ErasePen.SetColour(wxColor(Color));
    g_ErasePen.SetWidth(Width);
}

void
UpdateNormalPen(
    int      Width,
    RGBCOLOR Color
    )
{
    g_NormalPen.SetColour(wxColor(Color));
    g_NormalPen.SetWidth(Width);
}

ERR_TYPES
gifsave_helper(
    const wchar_t * GifFileName,
    int             iDelay_,
    int             bAppendMode_,
    int             iLoop_,
    int             iTrans_,
    int             iMaxColorDepth
    )
{
    iDelay      = iDelay_;
    bAppendMode = bAppendMode_;
    iLoop       = iLoop_;
    iTrans      = iTrans_;

    ERR_TYPES status = DumpBitmapFile(TempBmpName, iMaxColorDepth);
    if (status != SUCCESS)
    {
        // an error occured.
        return status;
    }

    lsetcursorwait(NIL);
    if (gbmBmpToGif(TempBmpName, GifFileName) != 0)
    {
        status = IMAGE_GIF_SAVE_FAILED;
    }
    lsetcursorarrow(NIL);
    wxRemoveFile(TempBmpName);

    return status;
}

PENSTATE & GetPenStateForSelectedTurtle()
{
    if (g_SelectedTurtle->HasOwnPenState)
    {
        return g_SelectedTurtle->PenState;
    }

    // the current turtle uses the global pen state
    return g_PenState;
}

// returns the dimensions of the working area, that is
// the size of the desktop without the task bar.
void GetWorkingAreaDimensions(int & Width, int & Height)
{
#if __WXMSW__
    RECT workingArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workingArea, 0);
    Width  = workingArea.right  - workingArea.left;
    Height = workingArea.bottom - workingArea.top;
#else
    // GTK has no notion of a "working area" as the "task bar" is
    // just another window on the desktop.  Use the desktop
    // width and height as the working area.
    Width  = wxSystemSettings::GetMetric(wxSYS_SCREEN_X, NULL);
    Height = wxSystemSettings::GetMetric(wxSYS_SCREEN_Y, NULL);
#endif
}



NODE *lgifsave(NODE *args)
{
    // same as BITMAP-SAVE but gets file name from logo command
    CStringPrintedNode gifFileName(car(args));
    args = cdr(args);

    // setup default values for the optional inputs
    int  iDelay = -1;
    bool bAppendMode = false;
    int  iLoop = -1;
    int  iMaxColorDepth = 8;
    int  iTrans = -1;

    if (args != NIL)
    {
        iDelay = int_arg(args);
        args = cdr(args);
        if (NOT_THROWING && args != NIL)
        {
            bAppendMode = boolean_arg(args);
            args = cdr(args);
            if (NOT_THROWING && args != NIL)
            {
                iLoop = int_arg(args);
                args = cdr(args);
                if (NOT_THROWING && args != NIL)
                {
                    iMaxColorDepth = int_arg(args);
                    args = cdr(args);
                    if ((iMaxColorDepth != 4) && (iMaxColorDepth != 1))
                    {
                        iMaxColorDepth = 8;
                    }
                    if (NOT_THROWING && args != NIL)
                    {
                        iTrans = GetColorArgument(args);
                    }
                }
            }
        }
    }

    if (NOT_THROWING)
    {
        ERR_TYPES status = gifsave_helper(
            gifFileName,
            iDelay,
            bAppendMode,
            iLoop,
            iTrans,
            iMaxColorDepth);
        if (status != SUCCESS)
        {
            err_logo(status, NIL);
        }
    }

    return Unbound;
}

NODE *lbitsave(NODE *args)
{
    // same as BITMAP-SAVE but gets file name from logo instruction
    CStringPrintedNode bmpFileName(car(args));

    if (stopping_flag == THROWING)
    {
        return Unbound;
    }

    int iMaxBitCount = 32;
    if (cdr(args) != NIL)
    {
        iMaxBitCount = int_arg(cdr(args));
        if (stopping_flag == THROWING)
        {
            return Unbound;
        }
    }

    ERR_TYPES status = DumpBitmapFile(bmpFileName, iMaxBitCount);
    if (status != SUCCESS)
    {
        err_logo(status, NIL);
        return Unbound;
    }

    return Unbound;
}

ERR_TYPES
gifload_helper(
    const wchar_t * GifFileName,
    unsigned int  & dwPixelWidth,
    unsigned int  & dwPixelHeight
    )
{
    // convert the GIF to a DIB
    lsetcursorwait(NIL);

    int rval = gbmGifToBmp(GifFileName, TempBmpName);
    lsetcursorarrow(NIL);

    if (rval != 0)
    {
        return IMAGE_GIF_LOAD_FAILED;
    }

    // load the bitmap
    ERR_TYPES status = LoadBitmapFile(
        TempBmpName,
        dwPixelWidth,
        dwPixelHeight);

    // cleanup
    wxRemoveFile(TempBmpName);
    return status;
}

NODE *lgifload(NODE *arg)
{
    CStringPrintedNode gifFileName(car(arg));

    if (stopping_flag == THROWING)
    {
        return Unbound;
    }

    unsigned int pixelWidth = 1;
    unsigned int pixelHeight = 1;
    ERR_TYPES status = gifload_helper(gifFileName, pixelWidth, pixelHeight);
    if (status != SUCCESS)
    {
        err_logo(status, NIL);
        return Unbound;
    }

    return Unbound;
}

NODE *lgifsize(NODE *args)
{
    CStringPrintedNode gifFileName(car(args));

    if (stopping_flag == THROWING)
    {
        return Unbound;
    }

    unsigned int pixelWidth = 0;
    unsigned int pixelHeight = 0;
    ERR_TYPES status = gifload_helper(gifFileName, pixelWidth, pixelHeight);
    if (status != SUCCESS)
    {
        err_logo(status, NIL);
        return Unbound;
    }

    return cons_list(
        make_intnode((FIXNUM) pixelWidth),
        make_intnode((FIXNUM) pixelHeight));
}

NODE *lbitload(NODE *arg)
{
    // same as BITMAP-LOAD except callable from logo command
    CStringPrintedNode bitmapFileName(car(arg));

    if (stopping_flag == THROWING)
    {
        return Unbound;
    }


    unsigned int pixelWidth = 1;
    unsigned int pixelHeight = 1;
    ERR_TYPES status = LoadBitmapFile(
        bitmapFileName,
        pixelWidth,
        pixelHeight);
    if (status != SUCCESS)
    {
        err_logo(status, NIL);
        return Unbound;
    }

    return Unbound;
}

NODE *lbitloadsize(NODE *arg)
{
    // same as BITMAP-LOAD except callable from logo command
    CStringPrintedNode bitmapFileName(car(arg));

    if (stopping_flag == THROWING)
    {
        return Unbound;
    }

    unsigned int pixelWidth = 0;
    unsigned int pixelHeight = 0;
    ERR_TYPES status = LoadBitmapFile(bitmapFileName, pixelWidth, pixelHeight);
    if (status != SUCCESS)
    {
        err_logo(status, NIL);
        return Unbound;
    }

    return cons_list(
        make_intnode((FIXNUM) pixelWidth),
        make_intnode((FIXNUM) pixelHeight));
}

NODE *lbitsize(NODE *)
{
    int width  = 0;
    int height = 0;

    if (ClipboardIsSelectedBitmap())
    {
        if (wxTheClipboard->Open())
        {
            // Try reading the clipboard as a bitmap.
            if (wxTheClipboard->IsSupported(wxDF_BITMAP))
            {
                wxBitmapDataObject data;
                if (wxTheClipboard->GetData(data))
                {
                    wxBitmap clipboardBitmap(data.GetBitmap());
                    width  = clipboardBitmap.GetWidth();
                    height = clipboardBitmap.GetHeight();
                }
            }

            wxTheClipboard->Close();
        }
    }
    else
    {
        // if we have something fetch its size
        if (g_SelectedBitmap->Bitmap != NULL)
        {
            width  = g_SelectedBitmap->Bitmap->GetWidth();
            height = g_SelectedBitmap->Bitmap->GetHeight();
        }
    }

    return cons_list(make_intnode(width), make_intnode(height));
}

// Fills TurtlePoint with the location of the current
// turtle on the 2D screen (before zooming and scrolling)
//
// returns true on success.
// returns false on error.
static
bool
WorldCoordinateToScreenCoordinate(
    const VECTOR &  TurtleWorldPosition,
    wxPoint &       TurtleScreenPosition
    )
{
    if (current_mode == perspectivemode)
    {
        VECTOR from3d;

        from3d.x = TurtleWorldPosition.x / WorldWidth;
        from3d.y = TurtleWorldPosition.y / WorldHeight;
        from3d.z = TurtleWorldPosition.z / WorldDepth;

        if (!ThreeD.TransformPoint(from3d, TurtleScreenPosition))
        {
            return false;
        }
    }
    else
    {
        TurtleScreenPosition.x = g_round(TurtleWorldPosition.x);
        TurtleScreenPosition.y = g_round(TurtleWorldPosition.y);
    }

    return true;
}

// Invalidates a rectangle on the screen.
// It adjusts for the zoom factor and scroll position.
static void InvalidateRectangleOnScreen(const RECT & ScreenRectangle)
{
    RECT adjustedRectangle;

    // Expand the bounding box a little to make sure that it covers everything
    adjustedRectangle.left   = ScreenRectangle.left   - 2;
    adjustedRectangle.top    = ScreenRectangle.top    - 2;
    adjustedRectangle.right  = ScreenRectangle.right  + 2;
    adjustedRectangle.bottom = ScreenRectangle.bottom + 2;

    // Expand/contract the bounding box based on the zoom
    if (zoom_flag)
    {
        adjustedRectangle.left   *= the_zoom;
        adjustedRectangle.top    *= the_zoom;
        adjustedRectangle.right  *= the_zoom;
        adjustedRectangle.bottom *= the_zoom;
    }

    const int scrollerX = GetScreenHorizontalScrollPosition();
    const int scrollerY = GetScreenVerticalScrollPosition();

    // Move the bounding box based on scroll position.
    adjustedRectangle.left   -= scrollerX;
    adjustedRectangle.right  -= scrollerX;
    adjustedRectangle.top    -= scrollerY;
    adjustedRectangle.bottom -= scrollerY;

#ifdef WX_PURE
    wxRect refreshRegion;
    refreshRegion.SetLeft(adjustedRectangle.left);
    refreshRegion.SetRight(adjustedRectangle.right);
    refreshRegion.SetTop(adjustedRectangle.top);
    refreshRegion.SetBottom(adjustedRectangle.bottom);
    CFmsLogo::GetMainFrame()->GetScreen()->RefreshRect(refreshRegion, false);
#else
    ::InvalidateRect(GetScreenWindow(), &adjustedRectangle, FALSE);
#endif
}

static
void
InvalidateRectangleOnScreen(
    int Left,
    int Top,
    int Right,
    int Bottom)
{
    RECT screenRectangle;
    screenRectangle.left   = Left;
    screenRectangle.top    = Top;
    screenRectangle.right  = Right;
    screenRectangle.bottom = Bottom;
    InvalidateRectangleOnScreen(screenRectangle);
}

static void InvalidateRectangleOnScreen(const wxRect & MemoryRectangle)
{
    InvalidateRectangleOnScreen(
        MemoryRectangle.GetLeft(),
        MemoryRectangle.GetTop(),
        MemoryRectangle.GetRight(),
        MemoryRectangle.GetBottom());
}

static
void
InvalidateTopBottomWrappingMemoryRectangle(
    int Left,
    int Top,
    int Right,
    int Bottom)
{
    // This should only be called after the left-right wrapping
    // has been performed.
    assert(0 <= Left);
    assert(Left <= Right);
    assert(Right < BitMapWidth);

    if (Top < 0 && BitMapHeight <= Bottom)
    {
        // The box wraps around both the top and bottom
        // edges of the memory bitmap, which is to say,
        // it covers the entire height.
        InvalidateRectangleOnScreen(Left, 0, Right, BitMapHeight - 1);
    }
    else if (Top < 0)
    {
        // The box wraps around the top edge of the memory bitmap (only)

        // Invalidate the portion on the top edge.
        InvalidateRectangleOnScreen(Left, 0, Right, Bottom);

        // Invalidate the portion that wrapped to the bottom.
        InvalidateRectangleOnScreen(
            Left,
            BitMapHeight - 1 + Top,
            Right,
            BitMapHeight - 1);
    }
    else if (BitMapHeight <= Bottom)
    {
        // The box wraps around the bottom edge of the memory bitmap (only)

        // Invalidate the portion that wrapped to the top edge.
        InvalidateRectangleOnScreen(Left, 0, Right, Bottom - BitMapHeight + 1);

        // Invalidate the portion on the bottom edge.
        InvalidateRectangleOnScreen(Left, Top, Right, BitMapHeight - 1);
    }
    else
    {
        // The rectangle does not wrap across the left or right edges.
        InvalidateRectangleOnScreen(Left, Top, Right, Bottom);
    }
}

static
void
InvalidateWrappingMemoryRectangle(
    int Left,
    int Top,
    int Right,
    int Bottom)
{
    if (Left < 0 && BitMapWidth <= Right)
    {
        // The box wraps around both the left and right
        // edges of the memory bitmap, which is to say,
        // it covers the entire width.
        InvalidateTopBottomWrappingMemoryRectangle(
            0,
            Top,
            BitMapWidth - 1,
            Bottom);
    }
    else if (Left < 0)
    {
        // The box wraps around the left edge of the memory bitmap (only)

        // Invalidate the portion on the left edge.
        InvalidateTopBottomWrappingMemoryRectangle(0, Top, Right, Bottom);

        // Invalidate the portion that wrapped to the right.
        InvalidateTopBottomWrappingMemoryRectangle(
            BitMapWidth - 1 + Left,
            Top,
            BitMapWidth - 1,
            Bottom);
    }
    else if (BitMapWidth <= Right)
    {
        // The box wraps around the right edge of the memory bitmap (only)

        // Invalidate the portion that wrapped to the left edge.
        InvalidateTopBottomWrappingMemoryRectangle(
            0,
            Top,
            Right - BitMapWidth + 1,
            Bottom);

        // Invalidate the portion on the right edge.
        InvalidateTopBottomWrappingMemoryRectangle(
            Left,
            Top,
            BitMapWidth - 1,
            Bottom);
    }
    else
    {
        // The rectangle does not wrap across the left or right edges.
        InvalidateTopBottomWrappingMemoryRectangle(Left, Top, Right, Bottom);
    }
}

static void expand_rect_to_include_point(RECT & Rectangle, const wxPoint & Point)
{
    Rectangle.left   = std::min(Rectangle.left,   static_cast<LONG>(Point.x));
    Rectangle.top    = std::min(Rectangle.top,    static_cast<LONG>(Point.y));
    Rectangle.right  = std::max(Rectangle.right,  static_cast<LONG>(Point.x));
    Rectangle.bottom = std::max(Rectangle.bottom, static_cast<LONG>(Point.y));
}

static
bool
ibmturt_transform_3d_segment(
    const VECTOR & TurtleCoordinatesFrom,
    const VECTOR & TurtleCoordinatesTo,
    wxPoint      & ScreenCoordinatesFrom,
    wxPoint      & ScreenCoordinatesTo
    )
{
    VECTOR rp = MVxyMultiply(g_SelectedTurtle->Matrix, TurtleCoordinatesFrom);

    VECTOR from3d;
    from3d.x = (g_SelectedTurtle->Position.x + rp.x) / BitMapWidth;
    from3d.y = (g_SelectedTurtle->Position.y + rp.y) / BitMapWidth;
    from3d.z = (g_SelectedTurtle->Position.z + rp.z) / BitMapWidth;

    rp = MVxyMultiply(g_SelectedTurtle->Matrix, TurtleCoordinatesTo);

    VECTOR to3d;
    to3d.x = (g_SelectedTurtle->Position.x + rp.x) / BitMapWidth;
    to3d.y = (g_SelectedTurtle->Position.y + rp.y) / BitMapWidth;
    to3d.z = (g_SelectedTurtle->Position.z + rp.z) / BitMapWidth;

    wxPoint from2d;
    wxPoint to2d;
    if (!ThreeD.TransformSegment(from3d, to3d, from2d, to2d))
    {
        // The line segment is not visible when projected into
        // screen coordinates.
        return false;
    }

    ScreenCoordinatesFrom.x =  from2d.x + xoffset;
    ScreenCoordinatesFrom.y = -from2d.y + yoffset;

    ScreenCoordinatesTo.x   =  to2d.x   + xoffset;
    ScreenCoordinatesTo.y   = -to2d.y   + yoffset;
    return true;
}

// ibmturt() calculates what needs to be done to either draw or erase
// the turte, but it does not actually do either of these operations.
// If draw==true, then the points of the turtle's vertices are computed.
// If draw==false, then the points of the turtle's vertices are simply used
// to invalidate the turtle's bounding box (so it will get erased).
void ibmturt(bool draw)
{
    // special turtles must not be drawn
    assert(!g_SelectedTurtle->IsSpecial);

    if (draw)
    {
        // We are supposed to draw the turtle in a new position, we
        // we need to re-calculate its points.

        // Initialize assuming that all points will fall on the screen.
        g_SelectedTurtle->BodyOutlineIsValid = true;
        g_SelectedTurtle->TotalSegments      = 0;

        if (current_mode == perspectivemode)
        {
            // in 3D mode

            // Map each of point of the turtle's body in screen coordinates.
            for (size_t i = 0; i < ARRAYSIZE(g_TurtleBodyOutline); i++)
            {
                VECTOR rp = MVxyMultiply(g_SelectedTurtle->Matrix, g_TurtleBodyOutline[i]);

                VECTOR point3d;
                point3d.x = (g_SelectedTurtle->Position.x + rp.x) / BitMapWidth;
                point3d.y = (g_SelectedTurtle->Position.y + rp.y) / BitMapWidth;
                point3d.z = (g_SelectedTurtle->Position.z + rp.z) / BitMapWidth;

                wxPoint point2d;
                if (!ThreeD.TransformPoint(point3d, point2d))
                {
                    // Part of the turtle is off-screen.
                    // Mark the BodyOutline as invalid and store the
                    // parts turtle's outline which are on-screen in
                    // g_SelectedTurtle->Segments.
                    g_SelectedTurtle->BodyOutlineIsValid = false;
                    for (i = 0; i < ARRAYSIZE(g_TurtleBodyOutline); i++)
                    {
                        size_t next = (i + 1) % ARRAYSIZE(g_TurtleBodyOutline);

                        if (ibmturt_transform_3d_segment(
                                g_TurtleBodyOutline[i],
                                g_TurtleBodyOutline[next],
                                g_SelectedTurtle->Segments[g_SelectedTurtle->TotalSegments].From,
                                g_SelectedTurtle->Segments[g_SelectedTurtle->TotalSegments].To)) {
                            // Some part of this line segment are on-screen.
                            g_SelectedTurtle->TotalSegments++;
                        }
                    }
                    break;
                }

                g_SelectedTurtle->BodyOutline[i].x =  point2d.x + xoffset;
                g_SelectedTurtle->BodyOutline[i].y = -point2d.y + yoffset;
            }

            // Add the line that is drawn on the turtle's right side.
            if (ibmturt_transform_3d_segment(
                    g_TurtlePerspectiveModeIndicator[0],
                    g_TurtlePerspectiveModeIndicator[1],
                    g_SelectedTurtle->Segments[g_SelectedTurtle->TotalSegments].From,
                    g_SelectedTurtle->Segments[g_SelectedTurtle->TotalSegments].To)) {
                // Some part of this line segment are on-screen.
                g_SelectedTurtle->TotalSegments++;
            }
        }
        else
        {
            // 2D mode
            for (size_t i = 0; i < ARRAYSIZE(g_TurtleBodyOutline); i++)
            {
                FLONUM cosine = cos(g_SelectedTurtle->Heading * rads_per_degree);
                FLONUM sine   = sin(g_SelectedTurtle->Heading * rads_per_degree);

                FLONUM rx = X_ROTATED(g_TurtleBodyOutline[i].x, g_TurtleBodyOutline[i].y, cosine, sine);
                FLONUM ry = Y_ROTATED(g_TurtleBodyOutline[i].x, g_TurtleBodyOutline[i].y, cosine, sine);

                FLONUM oldx = g_SelectedTurtle->Position.x + rx;
                FLONUM oldy = g_SelectedTurtle->Position.y + ry;

                long iOldx = g_round(oldx);
                long iOldy = g_round(oldy);

                long x =  iOldx + xoffset;
                long y = -iOldy + yoffset;

                g_SelectedTurtle->BodyOutline[i].x = x;
                g_SelectedTurtle->BodyOutline[i].y = y;
            }
        }
    }

    if (g_SelectedTurtle->BitmapRasterMode != 0)
    {
        // The turtle is bitmapped, we need to invalidate the bounding box of the bitmap.
        wxPoint dest;
        bool needsInvalidation = WorldCoordinateToScreenCoordinate(
            g_SelectedTurtle->Position,
            dest);

        // If the turtle is on the screen and it has a bitmap, then
        // calculate a bounding box to invalidate.
        int turtleIndex = g_SelectedTurtle - g_Turtles;
        if (needsInvalidation            &&
            turtleIndex < g_BitmapsLimit &&
            g_Bitmaps[turtleIndex].Bitmap != NULL)
        {
            // figure out the index into the bitmaps array.
            const CUTMAP & turtleBitmap = g_Bitmaps[turtleIndex];
            const int turtleBitmapWidth  = turtleBitmap.Bitmap->GetWidth();
            const int turtleBitmapHeight = turtleBitmap.Bitmap->GetHeight();

            FLONUM leftOffset;
            FLONUM rightOffset;
            FLONUM topOffset;
            FLONUM bottomOffset;

            if (g_SelectedTurtle->IsSprite)
            {
                FLONUM cosine = cos(g_SelectedTurtle->Heading * rads_per_degree);
                FLONUM sine   = sin(g_SelectedTurtle->Heading * rads_per_degree);

                // Compute the location of the four corners of the sprite's image
                // rotated about its center.  These values are initially relative
                // to the turtle's location.
                FLONUM x0 = X_ROTATED(-turtleBitmapWidth/2.0, -turtleBitmapHeight/2.0, cosine, sine);
                FLONUM y0 = Y_ROTATED(-turtleBitmapWidth/2.0, -turtleBitmapHeight/2.0, cosine, sine);

                FLONUM x1 = X_ROTATED(-turtleBitmapWidth/2.0, turtleBitmapHeight/2.0, cosine, sine);
                FLONUM y1 = Y_ROTATED(-turtleBitmapWidth/2.0, turtleBitmapHeight/2.0, cosine, sine);

                FLONUM x2 = X_ROTATED(turtleBitmapWidth/2.0, turtleBitmapHeight/2.0, cosine, sine);
                FLONUM y2 = Y_ROTATED(turtleBitmapWidth/2.0, turtleBitmapHeight/2.0, cosine, sine);

                FLONUM x3 = X_ROTATED(turtleBitmapWidth/2.0, -turtleBitmapHeight/2.0, cosine, sine);
                FLONUM y3 = Y_ROTATED(turtleBitmapWidth/2.0, -turtleBitmapHeight/2.0, cosine, sine);

                // Compute the bounding box, again relative to the turtle's location.
                // We grow the box by one to account for rounding errors.
                leftOffset   = std::min(x0,std::min(x1, std::min(x2,x3))) - 1;
                topOffset    = std::min(y0,std::min(y1, std::min(y2,y3))) - 1;
                rightOffset  = std::max(x0,std::max(x1, std::max(x2,x3))) + 1;
                bottomOffset = std::max(y0,std::max(y1, std::max(y2,y3))) + 1;

                // Map from turtle coordinates to memory bitmap coordinates.
                int memoryBoundingBoxLeft   = +dest.x + xoffset + leftOffset;
                int memoryBoundingBoxTop    = -dest.y + yoffset + topOffset;
                int memoryBoundingBoxRight  = +dest.x + xoffset + rightOffset;
                int memoryBoundingBoxBottom = -dest.y + yoffset + bottomOffset;

                if (current_mode == wrapmode)
                {
                    // In "wrapmode" sprite bitmaps wrap around the screen just
                    // like the turtle movement.
                    InvalidateWrappingMemoryRectangle(
                        memoryBoundingBoxLeft,
                        memoryBoundingBoxTop,
                        memoryBoundingBoxRight,
                        memoryBoundingBoxBottom);
                }
                else
                {
                    InvalidateRectangleOnScreen(
                        memoryBoundingBoxLeft,
                        memoryBoundingBoxTop,
                        memoryBoundingBoxRight,
                        memoryBoundingBoxBottom);
                }
            }
            else
            {
                // Compute the bounding box of the non-rotating sprite in screen coordinates.
                // The lower-left corner of this bitmap is the location of the turtle.
                InvalidateRectangleOnScreen(
                    +dest.x + xoffset + 0,                       // left
                    -dest.y + yoffset + LL - turtleBitmapHeight, // top
                    +dest.x + xoffset + turtleBitmapWidth,       // right
                    -dest.y + yoffset + LL);                     // bottom
            }
        }
    }
    else
    {
        // The turtle is drawn with lines, so we need to calculate the bounding box
        // of these lines.

        // Calculate the bounding box independent of zoom and scroll position.
        bool  needsInvalidation = false;
        RECT  invalidationBoundingBox;

        invalidationBoundingBox.left   =  100000;
        invalidationBoundingBox.top    =  100000;
        invalidationBoundingBox.right  = -100000;
        invalidationBoundingBox.bottom = -100000;
        if (g_SelectedTurtle->BodyOutlineIsValid)
        {
            for (size_t i = 0; i < ARRAYSIZE(g_SelectedTurtle->BodyOutline); i++)
            {
                expand_rect_to_include_point(invalidationBoundingBox, g_SelectedTurtle->BodyOutline[i]);
                needsInvalidation = true;
            }
        }
        for (size_t i = 0; i < g_SelectedTurtle->TotalSegments; i++)
        {
            expand_rect_to_include_point(invalidationBoundingBox, g_SelectedTurtle->Segments[i].From);
            expand_rect_to_include_point(invalidationBoundingBox, g_SelectedTurtle->Segments[i].To);
            needsInvalidation = true;
        }

        if (needsInvalidation)
        {
            InvalidateRectangleOnScreen(invalidationBoundingBox);
        }
    }
}

NODE *lsetpixel(NODE *args)
{
    ASSERT_TURTLE_INVARIANT;

    wxPoint dest;
    if (!WorldCoordinateToScreenCoordinate(g_SelectedTurtle->Position, dest))
    {
        return Unbound;
    }

    // get args
    wxUint32 color = GetColorArgument(args);

    if (NOT_THROWING)
    {
        // Draw the pixel on the memory bitmap.
        wxDC * memoryDeviceContext = GetWxMemoryDeviceContext();

        // Set the foreground mix mode in case a previous call had set it
        // to something that would modify the pixel.
        memoryDeviceContext->SetLogicalFunction(wxCOPY);

        const wxPen oldPen(memoryDeviceContext->GetPen());
        memoryDeviceContext->SetPen(wxColor(color));
#ifdef __WXMSW__
        memoryDeviceContext->DrawPoint(
            +dest.x + xoffset,
            -dest.y + yoffset);
#else
        // On non-MSW implementations of wxWidgets, wxDC::DrawPoint
        // draws a line from (x,y) to (x+1,y+1).  To work around
        // this, we set the pixel by drawing a single point.
        // See http://trac.wxwidgets.org/ticket/9674
        memoryDeviceContext->DrawLine(
            +dest.x + xoffset,
            -dest.y + yoffset,
            +dest.x + xoffset,
            -dest.y + yoffset);
#endif
        memoryDeviceContext->SetPen(oldPen);

        //screen
        draw_turtle(false);

        if (zoom_flag)
        {
            // Refresh the pixel on the screen so that pixel on the memory
            // bitmap is immediatly drawn to the screen.
            wxRect pixelRect(+dest.x + xoffset, -dest.y + yoffset, 1, 1);
            InvalidateRectangleOnScreen(pixelRect);
        }
        else
        {
            // We are not zoomed, so the screen is 1:1 with the memory bitmap.
            // This makes it easy to update the screen without an expensive repaint.
            wxDC * screenDeviceContext = GetWxScreenDeviceContext();

            const wxPen oldScreenPen(screenDeviceContext->GetPen());
            screenDeviceContext->SetPen(wxColour(color));

            // Set the foreground mix mode in case a previous call had set it
            // to something that would modify the pixel.
            screenDeviceContext->SetLogicalFunction(wxCOPY);

#ifdef __WXMSW__
        screenDeviceContext->DrawPoint(
            +dest.x - GetScreenHorizontalScrollPosition() + xoffset,
            -dest.y - GetScreenVerticalScrollPosition()   + yoffset);
#else
        screenDeviceContext->DrawLine(
            +dest.x - GetScreenHorizontalScrollPosition() + xoffset,
            -dest.y - GetScreenVerticalScrollPosition()   + yoffset,
            +dest.x - GetScreenHorizontalScrollPosition() + xoffset,
            -dest.y - GetScreenVerticalScrollPosition()   + yoffset);
#endif

            screenDeviceContext->SetPen(oldScreenPen);
        }

        draw_turtle(true);
    }

    return Unbound;
}

static
int
getindexcolor(
    wxUint32 Color
    )
{
    for (size_t i = 0; i < COLORTABLESIZE; i++)
    {
        if (Color == colortable[i])
        {
            return i;
        }
    }
    return -1;
}

static NODE* color_helper(const wxColour & Color)
{
    if (bIndexMode)
    {
        // We're in "index mode", so colors are returned as
        // an index, if possible.
        int colorIndex = getindexcolor(Color.GetRGB());
        if (colorIndex >= 0)
        {
            return make_intnode(colorIndex);
        }
    }

    return cons_list(
        make_intnode((FIXNUM) Color.Red()),
        make_intnode((FIXNUM) Color.Green()),
        make_intnode((FIXNUM) Color.Blue()));
}

static NODE* color_helper(const Color & col)
{
    if (bIndexMode)
    {
        int icolor = getindexcolor(MAKERGB(col.red, col.green, col.blue));
        if (icolor >= 0)
        {
            return make_intnode(icolor);
        }
    }

    return cons_list(
        make_intnode((FIXNUM) col.red),
        make_intnode((FIXNUM) col.green),
        make_intnode((FIXNUM) col.blue));
}



// function that returns the RGB vector of the pixel the turtle is on top of
NODE *lpixel(NODE *)
{
    ASSERT_TURTLE_INVARIANT;

    wxPoint dest;
    if (!WorldCoordinateToScreenCoordinate(g_SelectedTurtle->Position, dest))
    {
        return cons_list(
            make_intnode((FIXNUM) -1),
            make_intnode((FIXNUM) -1),
            make_intnode((FIXNUM) -1));
    }

    // Read the pixel from the memory bitmap.
    wxDC * memoryDeviceContext = GetWxMemoryDeviceContext();

    wxColour pixelColor;
    memoryDeviceContext->GetPixel(
        dest.x + xoffset,
        -dest.y + yoffset,
        &pixelColor);

    // Return the pixel as a NODE
    return color_helper(pixelColor);
}

void logofill(bool fillUntilPenColor)
{
    wxPoint dest;
    if (!WorldCoordinateToScreenCoordinate(g_SelectedTurtle->Position, dest))
    {
        return;
    }

    // memory
    wxDC * memoryDeviceContext = GetWxMemoryDeviceContext();

    // Select the fill brush, saving the old one.
    wxColor fillColor(fcolor);
    wxBrush fillBrush(fillColor);
    const wxBrush oldBrush = memoryDeviceContext->GetBrush();
    memoryDeviceContext->SetBrush(fillBrush);

    if (fillUntilPenColor)
    {
        memoryDeviceContext->FloodFill(
            +dest.x + xoffset,
            -dest.y + yoffset,
            wxColor(pcolor),
            wxFLOOD_BORDER);
    }
    else
    {
        wxColor colorUnderTurtle;
        memoryDeviceContext->GetPixel(
            +dest.x + xoffset,
            -dest.y + yoffset,
            &colorUnderTurtle);

        memoryDeviceContext->FloodFill(
            +dest.x + xoffset,
            -dest.y + yoffset,
            colorUnderTurtle,
            wxFLOOD_SURFACE);
    }

    // Restore the previous brush
    memoryDeviceContext->SetBrush(oldBrush);

    // Since we don't have a way to cacluate how much of the
    // memory region was filled, we mark the entire screen as
    // invalid.
    wxWindow * screen = GetScreenWxWindow();
    screen->Refresh(false);

    // Update the screen immediately so that FILL within
    // a timer event will take place (otherwise the repainting
    // events will wait until FMSLogo is idle).
    screen->Update();
}

NODE *lpencolor(NODE *)
{
    ASSERT_TURTLE_INVARIANT;
    return color_helper(GetPenStateForSelectedTurtle().Color);
}


// Do all of the things necessary when the pen color changes:
// * Sets the pen color on the active turtle
// * Updates the global pen
// * Updates the values in the status window
void ChangeActivePenColor(int Red, int Green, int Blue)
{
    Color & penColor = GetPenStateForSelectedTurtle().Color;
    penColor.red   = Red;
    penColor.green = Green;
    penColor.blue  = Blue;

    pcolor = MAKERGB(Red, Green, Blue);

    UpdateNormalPen(GetPenStateForSelectedTurtle().Width, pcolor);

    update_status_pencolor();
}


// function to return flood color as a RGB list
NODE *lfloodcolor(NODE *)
{
    ASSERT_TURTLE_INVARIANT;
    return color_helper(dfld);
}

// Do all of the things necessary when the flood color changes:
// * Sets the global flood color
// * Updates the global flood color brush
// * Updates the values in the status window
void ChangeActiveFloodColor(int Red, int Green, int Blue)
{
    dfld.red   = Red;
    dfld.green = Green;
    dfld.blue  = Blue;

    fcolor = MAKERGB(dfld.red, dfld.green, dfld.blue);

    update_status_floodcolor();
}

// function to return screen color as a RGB list
NODE *lscreencolor(NODE *)
{
    return color_helper(dscn);
}

// Do all of the things necessary when the screen color changes:
// * Sets the global screen color
// * Updates the global screen brush (and erase brush)
// * Updates the values in the status window
void ChangeActiveScreenColor(int Red, int Green, int Blue)
{
    dscn.red   = Red;
    dscn.green = Green;
    dscn.blue  = Blue;

    scolor = MAKERGB(dscn.red, dscn.green, dscn.blue);

    // When the screen changes we change the erase pen which basically
    // writes the screen color
    UpdateErasePen(GetPenStateForSelectedTurtle().Width, scolor);

    // Clear the screen using the new color
    ibm_clear_screen();

    update_status_screencolor();
}

int get_pen_width()
{
    return GetPenStateForSelectedTurtle().Width;
}

int get_pen_height()
{
    return get_pen_width();
}

void set_pen_width(int w)
{
    GetPenStateForSelectedTurtle().Width = w;

    // we erase with the same pen width as we write
    UpdateNormalPen(w, pcolor);
    UpdateErasePen(w,  scolor);
}

void set_pen_height(int h)
{
    set_pen_width(h);
}

NODE *lclearpalette(NODE *)
{
    // MSWLogo supported display depths of 256 and under.
    // FMSLogo no longer supports these, but retains the
    // procedure as a no-op for compatibility.
    return Unbound;
}

NODE *lstatus(NODE *)
{
    ASSERT_TURTLE_INVARIANT;

    OpenStatusWindow();
    return Unbound;
}

NODE *lnostatus(NODE *)
{
    CloseStatusWindow();
    return Unbound;
}


// The zoom control flag determines whether or not we can use the faster line
// drawing method of drawing directly to the screen buffer, or use the slower
// method of drawing to the backbuffer and invalidating part of the screen.
// This is only faster when there is no zooming.
// And it's only correct to do when there are no widgets on the screen, such
// as ones created with BUTTONCREATE or LISTBOXCREATE.
void UpdateZoomControlFlag()
{
    if ((the_zoom != 1.0) || CheckOnScreenControls())
    {
        zoom_flag = true;
    }
    else
    {
        zoom_flag = false;
    }
}

// The real work of zooming is done in Paint this just adjusts the scroller
// to something reasonable so that what was basically in the center of the
// screen still is. It also readjusts the ranges on the scrollers.
void zoom_helper(FLONUM NewZoomFactor)
{
    if (the_zoom != NewZoomFactor)
    {
        the_zoom = NewZoomFactor;

        AdjustScrollPositionToZoomFactor(NewZoomFactor);

        // hide turtle while we do this
        draw_turtle(false);

        UpdateZoomControlFlag();

        draw_turtle(true);

#ifndef WX_PURE
        // paint the entire window
        InvalidateRect(GetScreenWindow(), NULL, FALSE);
#endif
    }
}

NODE *lzoom(NODE *args)
{
    ASSERT_TURTLE_INVARIANT;

    FLONUM temp_zoom = positive_flonum_arg(car(args));
    if (stopping_flag != THROWING)
    {
        zoom_helper(temp_zoom);
    }

    return Unbound;
}

static
void
FillRectWithColor(
    wxDC         & DeviceContext,
    const wxRect & Rectangle,
    wxUint32       Color
    )
{
    // Select the fill brush, saving the old one.
    wxColor fillColor(Color);
    wxBrush fillBrush(fillColor);
    const wxBrush oldBrush(DeviceContext.GetBrush());
    DeviceContext.SetBrush(fillBrush);

    // Also change the pen to be transparent, or else
    // an outline will be drawn around the filled region.
    const wxPen oldPen(DeviceContext.GetPen());
    DeviceContext.SetPen(*wxTRANSPARENT_PEN);

    // Draw the rectangle
    DeviceContext.DrawRectangle(Rectangle);

    // Restore the brush/pen
    DeviceContext.SetBrush(oldBrush);
    DeviceContext.SetPen(oldPen);
}

NODE *lbitblock(NODE *arg)
{
    ASSERT_TURTLE_INVARIANT;

    wxPoint turtleLocation;
    if (!WorldCoordinateToScreenCoordinate(
            g_SelectedTurtle->Position,
            turtleLocation))
    {
        // the turtle is not on the screen
        return Unbound;
    }

    // get args
    int cutWidth  = getint(nonnegative_int_arg(arg));
    int cutHeight = getint(nonnegative_int_arg(cdr(arg)));

    if (NOT_THROWING)
    {
        // only do the blit if a non-zero area was specified
        if (cutWidth != 0 && cutHeight != 0)
        {
            wxDC * memoryDeviceContext = GetWxMemoryDeviceContext();

            wxRect memoryRect(
                +turtleLocation.x + xoffset,                  // left
                -turtleLocation.y + yoffset - cutHeight + LL, // top
                cutWidth,                                     // width
                cutHeight);                                   // height

            FillRectWithColor(*memoryDeviceContext, memoryRect, fcolor);

            //screen
            if (zoom_flag)
            {
                // It's easier to invalidate the screen and force a
                // repaint from the image memory.
                GetScreenWxWindow()->Refresh(false);
            }
            else
            {
                // Invalidate the portion of the screen that corresponds
                // to the region of memory that we filled.
                // Ever since we stopped calling GetDC() and ReleaseDC()
                // to get the screen DC, simply calling FillRect() on the
                // screen causes repainting problems.
                // I suspect that calling ReleaseDC() implicitly invalidated
                // the screen, which may be why it was so much slower.
                InvalidateRectangleOnScreen(memoryRect);
            }
        }
    }

    return Unbound;
}


#ifndef WX_PURE

// Converts a FMSLogo bitmode to a Microsoft Windows raster mode
static
DWORD
BitModeToRasterMode(
    FIXNUM BitMode
    )
{
    switch (BitMode)
    {
    case 1:
        return SRCCOPY;

    case 2:
        return SRCPAINT;

    case 3:
        return SRCAND;

    case 4:
        return SRCINVERT;

    case 5:
        return SRCERASE;

    case 6:
        return NOTSRCCOPY;

    case 7:
        return NOTSRCERASE;

    case 8:
        return MERGEPAINT;

    case 9:
        return DSTINVERT;

    default:
        // notify the caller that the bitmode was invalid
        return 0;
    }
}

// Converts a Microsoft Windows raster mode to a FMSLogo bitmode.
static
FIXNUM
RasterModeToBitMode(
    DWORD RasterMode
    )
{
    // return the logo "code" for the bit mode
    switch (RasterMode)
    {
    case 0:
    case SRCCOPY:
        return 1;

    case SRCPAINT:
        return 2;

    case SRCAND:
        return 3;

    case SRCINVERT:
        return 4;

    case SRCERASE:
        return 5;

    case NOTSRCCOPY:
        return 6;

    case NOTSRCERASE:
        return 7;

    case MERGEPAINT:
        return 8;

    case DSTINVERT:
        return 9;
    }

    assert(0 && "can't happen");
    return 0;
}

static
DWORD
BitModeArgsToRasterMode(
    NODE * BitModeArguments
    )
{
    NODE * bitModeNode = ranged_integer_arg(BitModeArguments, 1, 9);
    if (stopping_flag == THROWING)
    {
        return 0;
    }

    return BitModeToRasterMode(getint(bitModeNode));
}

#endif // WX_PURE


NODE *lbitmode(NODE *)
{
    ASSERT_TURTLE_INVARIANT;

#ifdef WX_PURE
    FIXNUM temp = 0;
#else
    // return the logo "code" for the bit mode
    FIXNUM temp = RasterModeToBitMode(g_BitMode);
#endif

    return make_intnode(temp);
}

NODE *lsetbitmode(NODE *arg)
{
    ASSERT_TURTLE_INVARIANT;
#ifndef WX_PURE
    // convert from logo "code" to Windows constants
    FIXNUM bitmode = BitModeArgsToRasterMode(arg);
    if (NOT_THROWING)
    {
        g_BitMode = bitmode;
    }
#endif
    return Unbound;
}

NODE *lturtlemode(NODE *)
{
    ASSERT_TURTLE_INVARIANT;
#ifdef WX_PURE
    FIXNUM turtlemode = 0;
#else
    // return the logo "code" for the bit mode
    FIXNUM turtlemode = RasterModeToBitMode(g_SelectedTurtle->BitmapRasterMode);
#endif
    return make_intnode(turtlemode);
}

NODE *lsetturtlemode(NODE *arg)
{
    ASSERT_TURTLE_INVARIANT;
#ifndef WX_PURE
    if (g_SelectedTurtle->BitmapRasterMode)
    {
        if (g_SelectedTurtle->IsSprite)
        {
            // The code for drawing rotating sprites does not
            // honor the turtle's mode.  However, it could
            // someday, so prevent to its introduction from
            // being a breaking change, we prevent anyone
            // from setting it today.
            err_logo(
                INVALID_STATE_FOR_INSTRUCTION,
                cons_list(make_static_strnode(g_LOCALIZED_ERROR_MODE_ON_SPRITE)));
        }

        // convert from logo "code" to Windows constants
        FIXNUM rastermode = BitModeArgsToRasterMode(arg);
        if (NOT_THROWING)
        {
            draw_turtle(false);
            g_SelectedTurtle->BitmapRasterMode = rastermode;
            draw_turtle(true);
        }
    }
#endif
    return Unbound;
}

NODE *lbitindex(NODE *arg)
{
    ASSERT_TURTLE_INVARIANT;
    // return the current bitmap index
    return make_intnode((FIXNUM) (g_SelectedBitmap - g_Bitmaps));
}

NODE *lsetbitindex(NODE *arg)
{
    ASSERT_TURTLE_INVARIANT;

    // set the current bitmap index if within range
    int i = getint(nonnegative_int_arg(arg));
    if (stopping_flag == THROWING)
    {
        return Unbound;
    }

    if (g_BitmapsLimit <= i)
    {
        GrowBitmapsArray(i + 1);
        if (stopping_flag == THROWING)
        {
            return Unbound;
        }
    }

    g_SelectedBitmap = g_Bitmaps + i;

    return Unbound;
}


static
void
CopyFromBitmapArrayToClipboard()
{
    // Open the clipboard, dump what's in there and return the Bitmap
    if (g_Bitmaps[0].Bitmap != NULL)
    {
#ifdef WX_PURE
        if (wxTheClipboard->Open())
        {
            wxBitmap & bitmapToCopy = *(g_Bitmaps[0].Bitmap);
            wxTheClipboard->SetData(new wxBitmapDataObject(bitmapToCopy));
            wxTheClipboard->Flush();
            wxTheClipboard->Close();
        }
#else
        // For reasons I don't understand, the bitmap which the WX_PURE code
        // copies to the clipboard is completely black on MSW.  Therefore,
        // on MSW, we use native code for doing so.
        ::OpenClipboard(GetMainWindow());
        ::EmptyClipboard();
        ::SetClipboardData(CF_BITMAP, g_Bitmaps[0].Bitmap->GetHBITMAP());
        ::CloseClipboard();
#endif
    }
}

static
void
PasteFromClipboardToBitmapArray()
{
    // Delete any bitmap that may exist.
    delete g_Bitmaps[0].Bitmap;
    g_Bitmaps[0].Bitmap = NULL;

    if (wxTheClipboard->Open())
    {
        // Try reading the clipboard as a bitmap.
        if (wxTheClipboard->IsSupported(wxDF_BITMAP))
        {
            wxBitmapDataObject data;
            if (wxTheClipboard->GetData(data))
            {
                g_Bitmaps[0].Bitmap = new wxBitmap(data.GetBitmap());
            }
        }

        wxTheClipboard->Close();
    }
}

static
NODE *
BitCopyOrCut(NODE *arg, bool IsCut)
{
    wxPoint dest;
    if (!WorldCoordinateToScreenCoordinate(g_SelectedTurtle->Position, dest))
    {
        return Unbound;
    }

    int tempWidth  = getint(nonnegative_int_arg(arg));
    int tempHeight = getint(nonnegative_int_arg(cdr(arg)));

    if (NOT_THROWING)
    {
        bool havebitmap = false;

        // if we had a old cut get rid of it, we won't go in for clipboard
        if (g_SelectedBitmap->Bitmap != NULL)
        {
            // if same size reuse the bitmap
            if ((tempWidth  == g_SelectedBitmap->Bitmap->GetWidth()) &&
                (tempHeight == g_SelectedBitmap->Bitmap->GetHeight()))
            {
                havebitmap = true;
            }
            else
            {
                // else get rid of it and make a new one later
                delete g_SelectedBitmap->Bitmap;
                g_SelectedBitmap->Bitmap = NULL;
            }

            // Always free any pixel buffer.  Because this is filled in lazily,
            // we don't want to keep an old image around merely because it's
            // the same size as the one we might allocate in the future.
            free(g_SelectedBitmap->Pixels);
            g_SelectedBitmap->Pixels = NULL;
        }

        // only if we have a surface continue
        if (tempWidth != 0 && tempHeight != 0)
        {
            if (!havebitmap)
            {
                g_SelectedBitmap->Bitmap = new wxBitmap(tempWidth, tempHeight);
            }

            if (!g_SelectedBitmap->Bitmap->IsOk())
            {
                err_logo(BITMAP_CUT_FAILED, NIL);
                return Unbound;
            }


            // Copy the selected portion of the drawing surface to the turtle's bitmap.
            wxDC * memDC = GetWxMemoryDeviceContext();
            wxMemoryDC tempMemDC(*(g_SelectedBitmap->Bitmap));
            tempMemDC.Blit(
                0,
                0,
                tempWidth,
                tempHeight,
                memDC,
                +dest.x + xoffset,
                -dest.y + yoffset + LL - tempHeight);

            if (IsCut)
            {
                // This is a cut operation (as opposed to a copy operation)
                // Replace the cut bitmap with the screen color.
                wxRect memoryRect(
                    +dest.x + xoffset,                   // left
                    -dest.y + yoffset + LL - tempHeight, // top
                    tempWidth,                           // width
                    tempHeight);                         // height

                FillRectWithColor(*memDC, memoryRect, scolor);

                //screen
                if (zoom_flag)
                {
                    // It's difficult to correctly determine the bounding
                    // box when zoomed.  Instead, invalidate the entire screen.
                    GetScreenWxWindow()->Refresh(false);
                }
                else
                {
                    // Refresh the portion of the screen that corresponds to the
                    // area to which the bitmap filled above.
                    wxRect screenRect(
                        memoryRect.GetX() - GetScreenHorizontalScrollPosition(),
                        memoryRect.GetY() - GetScreenVerticalScrollPosition(),
                        memoryRect.GetWidth(),
                        memoryRect.GetHeight());

                    GetScreenWxWindow()->RefreshRect(screenRect, false);
                }
            }

            if (ClipboardIsSelectedBitmap())
            {
                CopyFromBitmapArrayToClipboard();
            }
        }
    }

    return Unbound;
}


NODE *lbitcut(NODE *arg)
{
    ASSERT_TURTLE_INVARIANT;
    return BitCopyOrCut(arg, true);
}

NODE *lbitcopy(NODE *arg)
{
    ASSERT_TURTLE_INVARIANT;
    return BitCopyOrCut(arg, false);
}

NODE *lbitfit(NODE *arg)
{
    ASSERT_TURTLE_INVARIANT;

    FIXNUM newWidth  = getint(nonnegative_int_arg(arg));
    FIXNUM newHeight = getint(nonnegative_int_arg(cdr(arg)));

    if (NOT_THROWING)
    {
        // If clipboard check with clipboard only
        if (ClipboardIsSelectedBitmap())
        {
            PasteFromClipboardToBitmapArray();
        }

        // resize the bitmap if we have a surface to fit to and from
        if (newWidth != 0 && newHeight != 0 && g_SelectedBitmap->Bitmap != NULL)
        {
            wxMemoryDC bitmapDeviceContext(*(g_SelectedBitmap->Bitmap));

            wxBitmap newBitmap(newWidth, newHeight);
            if (newBitmap.IsOk())
            {
                wxMemoryDC newBitmapDeviceContext(newBitmap);

                // Load hour-glass cursor.
                lsetcursorwait(NIL);

#ifdef __WXMSW__
                // Enable anti-aliasing on the strech blit on MSW.
                SetStretchBltMode(newBitmapDeviceContext.GetHDC(), HALFTONE);

                StretchBlt(
                    newBitmapDeviceContext.GetHDC(),
                    0,
                    0,
                    newWidth,
                    newHeight,
                    bitmapDeviceContext.GetHDC(),
                    0,
                    0,
                    g_SelectedBitmap->Bitmap->GetWidth(),
                    g_SelectedBitmap->Bitmap->GetHeight(),
                    SRCCOPY);
#else
                newBitmapDeviceContext.StretchBlit(
                    0,
                    0,
                    newWidth,
                    newHeight,
                    &bitmapDeviceContext,
                    0,
                    0,
                    g_SelectedBitmap->Bitmap->GetWidth(),
                    g_SelectedBitmap->Bitmap->GetHeight());
#endif

                // Restore the arrow cursor
                lsetcursorarrow(NIL);

                // commit to using the new "bitfitted" bitmap
                delete g_SelectedBitmap->Bitmap;
                g_SelectedBitmap->Bitmap = new wxBitmap(newBitmap);
            }
            else
            {
                err_logo(OUT_OF_MEM, NIL);
            }

            if (ClipboardIsSelectedBitmap())
            {
                CopyFromBitmapArrayToClipboard();
            }
        }
    }

    return Unbound;
}

static
void
BlitWithBitMode(
    wxDC       & TargetDeviceContext,
    wxCoord      DestinationX,
    wxCoord      DestinationY,
    wxCoord      Width,
    wxCoord      Height,
    wxDC       & SourceDeviceContext,
    wxCoord      SourceX,
    wxCoord      SourceY,
    DWORD        BitMode
    )
{
#ifdef WX_PURE
    TargetDeviceContext.Blit(
        DestinationX,
        DestinationY,
        Width,
        Height,
        &SourceDeviceContext,
        SourceX,
        SourceY,
        wxCOPY); // TODO: honor BitMode
#else
    BitBlt(
        TargetDeviceContext.GetHDC(),
        DestinationX,
        DestinationY,
        Width,
        Height,
        SourceDeviceContext.GetHDC(),
        SourceX,
        SourceY,
        BitMode);
#endif
}


NODE *lbitpaste(NODE *)
{
    ASSERT_TURTLE_INVARIANT;

    wxPoint dest;
    if (!WorldCoordinateToScreenCoordinate(g_SelectedTurtle->Position, dest))
    {
        return Unbound;
    }

    if (NOT_THROWING)
    {
        // If clipboard check with clipboard only
        if (ClipboardIsSelectedBitmap())
        {
            PasteFromClipboardToBitmapArray();
        }

        // only if we have something to paste
        if (g_SelectedBitmap->Bitmap != NULL)
        {
            // TODO: use device context in CUTMAP.
            wxMemoryDC bitmapDeviceContext(*(g_SelectedBitmap->Bitmap));

            //memory
            wxRect memoryRect(
                +dest.x + xoffset,
                -dest.y + yoffset + LL - g_SelectedBitmap->Bitmap->GetHeight(),
                g_SelectedBitmap->Bitmap->GetWidth(),
                g_SelectedBitmap->Bitmap->GetHeight());

            BlitWithBitMode(
                *GetWxMemoryDeviceContext(),
                memoryRect.GetX(),
                memoryRect.GetY(),
                memoryRect.GetWidth(),
                memoryRect.GetHeight(),
                bitmapDeviceContext,
                0,
                0,
                g_BitMode);

            //screen
            if (zoom_flag)
            {
                // It's difficult to correctly determine the bounding
                // box when zoomed.  Instead, invalidate the entire screen.
                GetScreenWxWindow()->Refresh(false);
            }
            else
            {
                // Refresh the portion of the screen that corresponds to the
                // area to which the bitmap was blit'd above.
                wxRect screenRect(
                    memoryRect.GetX() - GetScreenHorizontalScrollPosition(),
                    memoryRect.GetY() - GetScreenVerticalScrollPosition(),
                    memoryRect.GetWidth(),
                    memoryRect.GetHeight());

                GetScreenWxWindow()->RefreshRect(screenRect, false);
            }
        }
        else
        {
            // notify the user that the clipboard is empty
            err_logo(BITMAP_NOTHING_TO_PASTE, NIL);
        }
    }

    return Unbound;
}

NODE *lbitpastetoindex(NODE *arg)
{
    ASSERT_TURTLE_INVARIANT;

    // set the current bitmap index if within range
    int i = getint(nonnegative_int_arg(arg));
    int x = int_arg(cdr(arg));
    int y = int_arg(cdr(cdr(arg)));

    if (stopping_flag == THROWING)
    {
        return Unbound;
    }

    if (g_BitmapsLimit <= i)
    {
        // notify the user that the bitmap index is out of range
        err_logo(BITMAP_INDEX_OUT_OF_RANGE, NIL);
        return Unbound;
    }

    if (g_Bitmaps[i].Bitmap == NULL)
    {
        // notify the user that there is no bitmap at the target index
        err_logo(BITMAP_NO_BITMAP_AT_INDEX, NIL);
        return Unbound;
    }

    // If ClipBoard check with ClipBoard only
    if (ClipboardIsSelectedBitmap())
    {
        PasteFromClipboardToBitmapArray();
    }

    // only if we have something to paste
    if (g_SelectedBitmap->Bitmap != NULL)
    {
        // TODO: Use device context in CUTMAP.
        wxMemoryDC sourceDeviceContext(*(g_SelectedBitmap->Bitmap));

        // TODO: Use device context in CUTMAP.
        wxMemoryDC destinationDeviceContext(*(g_Bitmaps[i].Bitmap));

        BlitWithBitMode(
            destinationDeviceContext,
            +x,
            g_Bitmaps[i].Bitmap->GetHeight() - y - g_SelectedBitmap->Bitmap->GetHeight(),
            g_SelectedBitmap->Bitmap->GetWidth(),
            g_SelectedBitmap->Bitmap->GetHeight(),
            sourceDeviceContext,
            0,
            0,
            g_BitMode);
    }
    else
    {
        // notify the user that the clipboard is empty
        err_logo(BITMAP_NOTHING_TO_PASTE, NIL);
    }

    return Unbound;
}

NODE *lsetturtle(NODE *args)
{
    ASSERT_TURTLE_INVARIANT;

    NODE * val = ranged_integer_arg(args, -TOTAL_SPECIAL_TURTLES, FIXNUM_MAX);
    if (stopping_flag == THROWING)
    {
        return Unbound;
    }

    int turtleId = getint(val);
    if (turtleId < 0)
    {
        if (cdr(args) != NIL)
        {
            // unique pen modes are meaningless for the special
            // turtles, so we consider this bad input.
            err_logo(TOO_MUCH, NIL);
            return Unbound;
        }

        // this is a special turtle.
        // Remap <-1, -2, -3> to <0, 1, 2>
        g_SelectedTurtle = g_SpecialTurtles - turtleId - 1;
    }
    else
    {
        // This is a normal turtle

        // read the optional "has unique pen" input
        bool hasUniquePen         = false; // by default, share a global pen
        bool overrideHasUniquePen = false; // by default, keep the current setting
        if (cdr(args) != NIL)
        {
            hasUniquePen         = boolean_arg(cdr(args));
            overrideHasUniquePen = true;
            if (stopping_flag == THROWING)
            {
                return Unbound;
            }
        }

        if (g_TurtlesLimit <= turtleId)
        {
            // The physical array is smaller than the requested turtle
            // index, so we must grow the turtle array.

            if (turtleId + 1 > (size_t)-1 / sizeof(*g_Turtles))
            {
                // this allocation would result in an integer overflow
                err_logo(OUT_OF_MEM, NIL);
                return Unbound;
            }

            size_t newSize = (turtleId + 1) * sizeof(*g_Turtles);
            Turtle * newTurtles = (Turtle*) realloc(g_Turtles, newSize);
            if (newTurtles == NULL)
            {
                // could not grow the turtles array to hold turtleId
                err_logo(OUT_OF_MEM, NIL);
                return Unbound;
            }

            // commit to the new array
            g_Turtles      = newTurtles;
            g_TurtlesLimit = turtleId + 1;
        }

        g_SelectedTurtle = &g_Turtles[turtleId];
        if (turtleId > g_MaxTurtle)
        {
            // This turtle has not yet been created.
            // Create this turtle and all of the turtles up to it.
            for (int i = g_MaxTurtle + 1; i <= turtleId; i++)
            {
                InitializeTurtle(&g_Turtles[i]);
                g_Turtles[i].HasOwnPenState = hasUniquePen;
            }

            g_MaxTurtle = turtleId;
        }

        if (overrideHasUniquePen)
        {
            // The caller specified a UseUniquePen input, so
            // we override whatever value was set in the turtle.
            g_SelectedTurtle->HasOwnPenState = hasUniquePen;
        }

        // Update the current brush.
        // ChangeActivePenColor() updates the brushes color and width.
        const Color & activePenColor = GetPenStateForSelectedTurtle().Color;
        ChangeActivePenColor(
            activePenColor.red,
            activePenColor.green,
            activePenColor.blue);
    }

    draw_turtles(false);

    // update everything in the status window that can change
    // when the turtle changes.
    update_status_penstyle();
    update_status_penwidth();
    update_status_turtleposition();
    update_status_pencontact();
    update_status_turtlevisability();
    update_status_turtleheading();
    update_status_turtlepitch();
    update_status_turtleroll();
    update_status_turtlewhich();

    g_Wanna = g_SelectedTurtle->Position;

    draw_turtles(true);

    return Unbound;
}

NODE *lturtle(NODE *)
{
    ASSERT_TURTLE_INVARIANT;

    int active_turtle = GetSelectedTurtleIndex();
    return make_intnode(active_turtle);
}

NODE *lturtles(NODE *)
{
    ASSERT_TURTLE_INVARIANT;
    return make_intnode(g_MaxTurtle);
}

NODE *lhasownpenp(NODE*)
{
    return true_or_false(g_SelectedTurtle->HasOwnPenState);
}

inline unsigned char Interpolate(unsigned char A, unsigned char B, int Alpha, int OneMinusAlpha)
{
    assert(0 <= Alpha);
    assert(Alpha <= 4096);
    assert(Alpha + OneMinusAlpha == 4096);

    if (A == B)
    {
        return A;
    }

    return (A * OneMinusAlpha + B * Alpha) / 4096;
}


// Copies a box from the screen device into a wxImage, accounting for WRAP mode.
static
wxImage
ReadWrappedRectangle(
    wxDC & ScreenDeviceContext,
    int    Left,
    int    Top,
    int    Width,
    int    Height
    )
{
    // Allocate a wxBitmap of the proper dimensions
    wxBitmap bitmap(Width, Height);

    // Wrap the bitmap in a wxMemoryDC so that we can blit from DeviceContext to it.
    wxMemoryDC memoryDc(bitmap);

    // Blit the portion of ScreenDeviceContext that corresponds to the box described
    // by Left, Top, Width, Height to memoryDc, accounting for the wrapping.
    // There are 9 cases: all four corners, all four edges, and center (no wrapping).
    //
    // To handle all of these causes, up to four separate non-continuous regions of
    // the screen must be BLIT'd to resulting image.  This can be visualized by a
    // box that's in the upper-left corner of the screen
    //
    //  +---|---+
    //  | 1 | 2 |
    //  +---+---+-------------------------------+---+
    //  | 3 | 4 |                               | 3 |
    //  +---+---+                               +---+
    //      |                                       |
    //      |                                       |
    //      |                                       |
    //      |                                       |
    //      |---+                               +---+
    //      | 2 |                               | 1 |
    //      +---+-------------------------------+---+
    //
    // The same logic of four BLITs can handle all 9 cases, although for many
    // cases the regions are empty.
    if (current_mode != wrapmode)
    {
        // This mode doesn't wrap, so a single blit is sufficent.
        // Any parts that are off-screen will be black,
        // but this shouldn't matter because they shouldn't
        // be draw back to the screen.
       memoryDc.Blit(0, 0, Width, Height, &ScreenDeviceContext, Left, Top);
    }
    else
    {
        int screenRegion4X;
        int region1Width;
        if (Left < 0)
        {
            // The box extends past the left edge of screen and wraps to the right.
            region1Width   = -Left;
            screenRegion4X = 0;
        }
        else if (BitMapWidth < Left + Width)
        {
            // The box extends past the right edge of the screen and wraps to the left.
            region1Width   = BitMapWidth - Left;
            screenRegion4X = 0;
        }
        else
        {
            // The box somewhere in the horizontal center.
            region1Width   = 0;
            screenRegion4X = Left;
        }

        int region1Height;
        int screenRegion4Y;
        if (Top < 0)
        {
            // The box extends past the top edge of the screen and wraps to the bottom.
            region1Height  = -Top;
            screenRegion4Y = 0;
        }
        else if (BitMapHeight < Top + Height)
        {
            // The box extends past the bottom edge of the screen and wraps to the top.
            region1Height  = BitMapHeight - Top;
            screenRegion4Y = 0;
        }
        else
        {
            // The box is somewhere in the vertical center.
            region1Height  = 0;
            screenRegion4Y = Top;
        }

        // Copy the unwrapped upper-left corner of the screen to lower-right
        // corner of memoryDc (region 4 in the picture above).
        //
        // In the non-wrapped case, this is the only BLIT that's needed.
        memoryDc.Blit(
            region1Width,
            region1Height,
            Width  - region1Width,
            Height - region1Height,
            &ScreenDeviceContext,
            screenRegion4X,
            screenRegion4Y);

        if (region1Width != 0)
        {
            // The box straddles the left/right edge of the screen.

            // Copy the (wrapped) upper-right corner of the screen to the lower-left
            // corner of memoryDc (region 3 in the picture above).
            memoryDc.Blit(
                0,
                region1Height,
                region1Width,
                Height - region1Height,
                &ScreenDeviceContext,
                BitMapWidth - region1Width,
                screenRegion4Y);

            if (region1Height != 0)
            {
                // The box stradles both the left/edge and top/bottom edge of the screen.

                // Copy the (wrapped) lower-left corner of the screen to the upper-right
                // corner of memoryDc (region 1 in the picture above).
                memoryDc.Blit(
                    0,
                    0,
                    region1Width,
                    region1Height,
                    &ScreenDeviceContext,
                    BitMapWidth  - region1Width,
                    BitMapHeight - region1Height);
            }
        }

        if (region1Height != 0)
        {
            // The box straddles the top/bottom edge of the screen.

            // Copy the (wrapped) lower-left corner of the screen to the upper-right
            // corner of memoryDc (region 2 in the picture above).
            memoryDc.Blit(
                region1Width,
                0,
                Width - region1Width,
                region1Height,
                &ScreenDeviceContext,
                screenRegion4X,
                BitMapHeight - region1Height);
        }
    }

    memoryDc.SelectObject(wxNullBitmap);
    return bitmap.ConvertToImage();
}

// Copies an image to the screen device, accounting for WRAP mode.
static
void
WriteWrappedRectangle(
    wxImage & Image,
    wxDC    & ScreenDeviceContext,
    int       Left,
    int       Top,
    int       Width,
    int       Height
    )
{
    // Wrap the image in a wxDC for easy BLITing.
    wxBitmap imageBitmap(Image);
    wxMemoryDC imageDc(imageBitmap);

    // Blit the portion of Image that corresponds to the box described
    // by Left, Top, Width, Height to ScreenDeviceContext, accounting for the wrapping.
    // There are 9 cases: all four corners, all four edges, and center (no wrapping).
    //
    // To handle all of these causes, up to four separate non-continuous regions of
    // the screen must be BLIT'd to resulting image.  This can be visualized by an
    // image that's in the upper-left corner of the screen.
    //
    //  +---|---+
    //  | 1 | 2 |
    //  +---+---+-------------------------------+---+
    //  | 3 | 4 |                               | 3 |
    //  +---+---+                               +---+
    //      |                                       |
    //      |                                       |
    //      |                                       |
    //      |                                       |
    //      |---+                               +---+
    //      | 2 |                               | 1 |
    //      +---+-------------------------------+---+
    if (current_mode != wrapmode)
    {
        // This mode doesn't, so a single BLIT is sufficent.
        // Any parts that are off-screen will truncated.
       ScreenDeviceContext.Blit(Left, Top, Width, Height, &imageDc, 0, 0);
    }
    else
    {
        int screenRegion4X;
        int region1Width;
        if (Left < 0)
        {
            // The image extends past the left edge of screen and wraps to the right.
            region1Width   = -Left;
            screenRegion4X = 0;
        }
        else if (BitMapWidth < Left + Width)
        {
            // The image extends past the right edge of the screen and wraps to the left.
            region1Width   = BitMapWidth - Left;
            screenRegion4X = 0;
        }
        else
        {
            // The image is somewhere in the horizontal center.
            region1Width   = 0;
            screenRegion4X = Left;
        }

        int region1Height;
        int screenRegion4Y;
        if (Top < 0)
        {
            // The image extends past the top edge of the screen and wraps to the bottom.
            region1Height  = -Top;
            screenRegion4Y = 0;
        }
        else if (BitMapHeight < Top + Height)
        {
            // The image extends past the bottom edge of the screen and wraps to the top.
            region1Height  = BitMapHeight - Top;
            screenRegion4Y = 0;
        }
        else
        {
            // The image is somewhere in the vertical center.
            region1Height  = 0;
            screenRegion4Y = Top;
        }

        // Copy the unwrapped lower-left corner of the image to upper-right
        // corner of the screen (region 4 in the picture above).
        //
        // In the non-wrapped case, this is the only BLIT that's needed.
        ScreenDeviceContext.Blit(
            screenRegion4X,
            screenRegion4Y,
            Width  - region1Width,
            Height - region1Height,
            &imageDc,
            region1Width,
            region1Height);

        if (region1Width != 0)
        {
            // The image straddles the left/right edge of the screen.

            // Copy the (wrapped) lower-left corner of the image to the
            // upper-right corner of the screen (region 3 in the picture above).
            ScreenDeviceContext.Blit(
                BitMapWidth - region1Width,
                screenRegion4Y,
                region1Width,
                Height - region1Height,
                &imageDc,
                0,
                region1Height);

            if (region1Height != 0)
            {
                // The image stradles both the left/edge and top/bottom edge of the screen.

                // Copy the (wrapped) upper-right corner of the image to the
                // lower-left corner of the screen (region 1 in the picture above).
                ScreenDeviceContext.Blit(
                    BitMapWidth  - region1Width,
                    BitMapHeight - region1Height,
                    region1Width,
                    region1Height,
                    &imageDc,
                    0,
                    0);
            }
        }

        if (region1Height != 0)
        {
            // The box straddles the top/bottom edge of the screen.

            // Copy the (wrapped) upper-right corner of the image to the
            // lower-left corner of the screen(region 2 in the picture above).
            ScreenDeviceContext.Blit(
                screenRegion4X,
                BitMapHeight - region1Height,
                Width - region1Width,
                region1Height,
                &imageDc,
                region1Width,
                0);
        }
    }
}

inline FLONUM min4(FLONUM n1, FLONUM n2, FLONUM n3, FLONUM n4)
{
    return std::min(n1,std::min(n2, std::min(n3,n4)));
}

inline FLONUM max4(FLONUM n1, FLONUM n2, FLONUM n3, FLONUM n4)
{
    return std::max(n1,std::max(n2, std::max(n3,n4)));
}


// Copies a bitmap that is associated with a given turtle onto a
// device context.
//
// PaintDeviceContext - The device context on which to paste the bitmap.
// TurtleToPaste      - The index of the turtle (and bitmap) that should
//                      be copied to the screen.
//
// 1) This is only called for turtles that are bitmapped.
// 2) This ignores the scroll positions and the zoom.
static void turtlepaste(wxDC & PaintDeviceContext, int TurtleToPaste)
{
    ASSERT_TURTLE_INVARIANT;
    assert(0 <= TurtleToPaste);
    assert(TurtleToPaste <= g_MaxTurtle);

    if (g_BitmapsLimit <= TurtleToPaste)
    {
        // There is no bitmap which corresponds to this turtle.
        // We do nothing, equivalent to if the bitmap size is 0x0.
        return;
    }

    Turtle * const turtle = &g_Turtles[TurtleToPaste];
    CUTMAP * const bitmap = &g_Bitmaps[TurtleToPaste];

    wxPoint dest;
    if (!WorldCoordinateToScreenCoordinate(turtle->Position, dest))
    {
        return;
    }

    // If clipboard check with clipboard only
    if (TurtleToPaste == 0)
    {
        PasteFromClipboardToBitmapArray();
    }

    // only if we have something to paste
    if (bitmap->Bitmap != NULL)
    {
        // Create an HDC that wraps access to the turtle HBITMAP.
        // TODO: use a wxMemoryDC in CUTMAP
        wxMemoryDC turtleBitmapDc(*(bitmap->Bitmap));

        const int bitmapWidth  = bitmap->Bitmap->GetWidth();
        const int bitmapHeight = bitmap->Bitmap->GetHeight();

        if (turtle->IsSprite)
        {
            // TODO: figure out how to merge this with the bounding box
            // computation logic in ibmturt()

            // Compute the sine and cosine of the heading, but add special cases
            // for the multiples of 90 degress so that we get a crisp square
            // rotation instead of a slight deviation due to the imprecision
            // of sin() and cos() functions.
            FLONUM cosine;
            FLONUM sine;

            if (turtle->Heading == 90)
            {
                cosine = 0;
                sine   = 1;
            }
            else if (turtle->Heading == 180)
            {
                cosine = -1;
                sine   = 0;
            }
            else if (turtle->Heading == 270)
            {
                cosine = 0;
                sine   = -1;
            }
            else
            {
                cosine = cos(turtle->Heading * rads_per_degree);
                sine   = sin(turtle->Heading * rads_per_degree);
            }

            const FLONUM sourceWidth  = bitmapWidth;
            const FLONUM sourceHeight = bitmapHeight;

            // The location of the centerpoint (or origin) of the turtle's bitmap about which to rotate
            const FLONUM xOrigin = sourceWidth  / 2.0;
            const FLONUM yOrigin = sourceHeight / 2.0;

            // Compute the bounding box of the image rotated about its center.
            // This is the pre-rotated orientation.
            //
            //  (x1,y1) +-----------+ (x2,y2)
            //          |           |
            //          |     O     |
            //          |           |
            //  (x0,y0) +-----------+ (x3,y3)

            FLONUM x0 = X_ROTATED(-xOrigin, -yOrigin, cosine, sine);
            FLONUM y0 = Y_ROTATED(-xOrigin, -yOrigin, cosine, sine);

            FLONUM x1 = X_ROTATED(-xOrigin, yOrigin, cosine, sine);
            FLONUM y1 = Y_ROTATED(-xOrigin, yOrigin, cosine, sine);

            FLONUM x2 = X_ROTATED(xOrigin, yOrigin, cosine, sine);
            FLONUM y2 = Y_ROTATED(xOrigin, yOrigin, cosine, sine);

            FLONUM x3 = X_ROTATED(xOrigin, -yOrigin, cosine, sine);
            FLONUM y3 = Y_ROTATED(xOrigin, -yOrigin, cosine, sine);

            // Compute the bounding box of the rotated bitmap.
            // We grow the box by one to account for truncation.
            const int minx = static_cast<int>(min4(x0, x1, x2, x3));
            const int miny = static_cast<int>(min4(y0, y1, y2, y3));
            const int maxx = static_cast<int>(max4(x0, x1, x2, x3)) + 1;
            const int maxy = static_cast<int>(max4(y0, y1, y2, y3)) + 1;

            // Figure out where on the target bitmap the turtle belongs
            const int xScreenOffset = (+dest.x + xoffset);
            const int yScreenOffset = (-dest.y + yoffset);

            const RGBCOLOR TRANSPARENT_COLOR = MAKERGB(255, 255, 255);

            // Read the bitmap into a local buffer for faster access.
            // Because this takes a long time, we also cache this in the CUTMAP.
            if (bitmap->Pixels == NULL)
            {
                // TODO: Cache wxImage and call wxImage::GetData()
                bitmap->Pixels = (RGBCOLOR *) malloc(bitmapWidth * bitmapHeight * sizeof(RGBCOLOR));

                RGBCOLOR * nextPixel = bitmap->Pixels;
                for (int y = 0; y < bitmapHeight; y++)
                {
                    for (int x = 0; x < bitmapWidth; x++)
                    {
                        wxColor wxPixel;
                        turtleBitmapDc.GetPixel(x, y, &wxPixel);
                        *nextPixel++ = wxPixel.GetRGB();
                    }
                }
            }
            RGBCOLOR const * const sourceBitmap = bitmap->Pixels;

            // We will need fast access to the pixel data in PaintDeviceContext
            // in order to do transparent pixels.  Getting data pixel-by-pixel
            // is very slow, so we copy all of the screen data in the rotated bitmaps
            // bounding box to a local buffer.
            int boundingBoxWidth  = maxx - minx;
            int boundingBoxHeight = maxy - miny;
            wxImage image = ReadWrappedRectangle(
                PaintDeviceContext,
                xScreenOffset - boundingBoxWidth  / 2,  // left side of box
                yScreenOffset - boundingBoxHeight / 2,  // top of box
                boundingBoxWidth,
                boundingBoxHeight);
            unsigned char * imageBitsPtr = image.GetData();

            // Now do the rotating, one pixel at a time.
            // We walk through each pixel in the target bitmap and find the
            // corresponding pixel(s) within the source bitmap in order to
            // guarantee that each pixel on the target gets covered.
            //
            // This is slower than walking the source bitmap and projecting
            // to the destination bitmap, since there are more pixels in the
            // source bitmap's bounding box than there are pixels in the
            // source bitmap due to the rotation.
            const FLONUM deltaSourceX =  cosine;
            const FLONUM deltaSourceY = -sine;

            // Use bilinear interpolation to make the picture look nice at rough angles.
            for (int y = miny; y < maxy; y++)
            {
                FLONUM sourceX = X_ROTATED(minx, y, cosine, sine) + xOrigin - 0.5;
                FLONUM sourceY = Y_ROTATED(minx, y, cosine, sine) + yOrigin - 0.5;

                for (int x = minx; x < maxx; x++)
                {
                    // sourceX and sourceY look like this if computed in an absolute manner:
                    //   sourceX = X_ROTATED(x, y, cosine, sine) + xOrigin - 0.5;
                    //   sourceY = Y_ROTATED(x, y, cosine, sine) + yOrigin - 0.5;

                    if (-1.0 < sourceX && sourceX < sourceWidth &&
                        -1.0 < sourceY && sourceY < sourceHeight)
                    {
                        // Perform bilinear interpolation across the (potentially)
                        // four pixels on the source bitmap on which this screen
                        // pixel falls.
                        int sourceXFloor = sourceX < 0 ? -1.0 : static_cast<int>(sourceX);
                        int sourceYFloor = sourceY < 0 ? -1.0 : static_cast<int>(sourceY);
                        const RGBCOLOR * x0y0Ptr = sourceBitmap + sourceYFloor * bitmapWidth + sourceXFloor;
                        const RGBCOLOR * x0y1Ptr = x0y0Ptr + bitmapWidth;

                        // Read the four pixels from the bitmap. If any pixel falls
                        // outside the bitmap, then use the screen's pixel, instead.
                        RGBCOLOR pixelx0y0;
                        RGBCOLOR pixelx1y0;
                        RGBCOLOR pixelx0y1;
                        RGBCOLOR pixelx1y1;
                        if (0 <= sourceXFloor && sourceXFloor < bitmapWidth - 1)
                        {
                            // The "X0" and "X1" pixels both fall on the bitmap

                            if (0 <= sourceYFloor && sourceYFloor < bitmapHeight - 1)
                            {
                                // All four source pixels are on the bitmap.
                                pixelx0y0 = x0y0Ptr[0];
                                pixelx1y0 = x0y0Ptr[1];
                                pixelx0y1 = x0y1Ptr[0];
                                pixelx1y1 = x0y1Ptr[1];
                            }
                            else if (sourceYFloor < 0)
                            {
                                // The "Y0" pixels falls below the bitmap.
                                pixelx0y0 = TRANSPARENT_COLOR;
                                pixelx1y0 = TRANSPARENT_COLOR;
                                pixelx0y1 = x0y1Ptr[0];
                                pixelx1y1 = x0y1Ptr[1];
                            }
                            else
                            {
                                // The "Y1" pixels fall above the bitmap.
                                pixelx0y0 = x0y0Ptr[0];
                                pixelx1y0 = x0y0Ptr[1];
                                pixelx0y1 = TRANSPARENT_COLOR;
                                pixelx1y1 = TRANSPARENT_COLOR;
                            }
                        }
                        else if (sourceXFloor < 0)
                        {
                            // The "X0" pixels fall to the left of the bitmap.
                            pixelx0y0 = TRANSPARENT_COLOR;
                            pixelx0y1 = TRANSPARENT_COLOR;

                            if (0 <= sourceYFloor && sourceYFloor < bitmapHeight - 1)
                            {
                                // The Y0 and Y1 are on the bitmap.
                                pixelx1y0 = x0y0Ptr[1];
                                pixelx1y1 = x0y1Ptr[1];
                            }
                            else if (sourceYFloor < 0)
                            {
                                // The "Y0" pixels fall below the bitmap.
                                pixelx1y0 = TRANSPARENT_COLOR;
                                pixelx1y1 = x0y1Ptr[1];
                            }
                            else
                            {
                                // The "Y1" pixels fall above the bitmap.
                                pixelx1y0 = x0y0Ptr[1];
                                pixelx1y1 = TRANSPARENT_COLOR;
                            }
                        }
                        else
                        {
                            // The "X1" pixels fall to the right of the bitmap.
                            pixelx1y0 = TRANSPARENT_COLOR;
                            pixelx1y1 = TRANSPARENT_COLOR;

                            if (0 <= sourceYFloor && sourceYFloor < bitmapHeight - 1)
                            {
                                // The Y0 and Y1 are on the bitmap.
                                pixelx0y0 = x0y0Ptr[0];
                                pixelx0y1 = x0y1Ptr[0];
                            }
                            else if (sourceYFloor < 0)
                            {
                                // The "Y0" pixels fall below the bitmap.
                                pixelx0y0 = TRANSPARENT_COLOR;
                                pixelx0y1 = x0y1Ptr[0];
                            }
                            else
                            {
                                // The "Y1" pixels fall above the bitmap.
                                pixelx0y0 = x0y0Ptr[0];
                                pixelx0y1 = TRANSPARENT_COLOR;
                            }
                        }

                        if (pixelx0y0 == pixelx0y1 &&
                            pixelx0y0 == pixelx1y0 &&
                            pixelx0y0 == pixelx1y1)
                        {
                            // If all of the adjacent pixels are the same, then
                            // bilinear interpolation is unnecessary.  For a typical
                            // sprite image, this optimization yields a 10% speed
                            // improvement.
                            if (pixelx0y0 != TRANSPARENT_COLOR)
                            {
                                imageBitsPtr[0] = RedValue(pixelx0y0);
                                imageBitsPtr[1] = GreenValue(pixelx0y0);
                                imageBitsPtr[2] = BlueValue(pixelx0y0);
                            }
                        }
                        else
                        {
                            // Get the screen's value for each of the transparent pixels.
                            // If there aren't any transparent pixels, then we don't need
                            // to read the screen pixel.
                            if (pixelx0y0 == TRANSPARENT_COLOR ||
                                pixelx1y0 == TRANSPARENT_COLOR ||
                                pixelx0y1 == TRANSPARENT_COLOR ||
                                pixelx1y1 == TRANSPARENT_COLOR)
                            {
                                const RGBCOLOR screenPixel = MAKERGB(
                                    imageBitsPtr[0],  // red
                                    imageBitsPtr[1],  // green
                                    imageBitsPtr[2]); // blue

                                if (pixelx0y0 == TRANSPARENT_COLOR)
                                {
                                    pixelx0y0 = screenPixel;
                                }
                                if (pixelx1y0 == TRANSPARENT_COLOR)
                                {
                                    pixelx1y0 = screenPixel;
                                }
                                if (pixelx0y1 == TRANSPARENT_COLOR)
                                {
                                    pixelx0y1 = screenPixel;
                                }
                                if (pixelx1y1 == TRANSPARENT_COLOR)
                                {
                                    pixelx1y1 = screenPixel;
                                }
                            }

                            float ignored;
                            float xFraction = modff(sourceX, &ignored);
                            if (sourceX < 0)
                            {
                                xFraction = 1.0 + xFraction;
                            }

                            // xFraction Alpha goes from 0 -> 1.
                            // The domain of each color component is only 0-255,
                            // so without a loss of precision, we can change this
                            // to integral math.
                            const int alphaX         = (int)(xFraction * 4096.5);
                            const int oneMinusAlphaX = 4096 - alphaX;


                            // interpolate x0y0 x1y0 -> y0
                            unsigned char y0red;
                            unsigned char y0green;
                            unsigned char y0blue;
                            if (pixelx0y0 == pixelx1y0)
                            {
                                y0red   = RedValue(pixelx0y0);
                                y0green = GreenValue(pixelx0y0);
                                y0blue  = BlueValue(pixelx0y0);
                            }
                            else
                            {
                                y0red   = Interpolate(  RedValue(pixelx0y0),   RedValue(pixelx1y0), alphaX, oneMinusAlphaX);
                                y0green = Interpolate(GreenValue(pixelx0y0), GreenValue(pixelx1y0), alphaX, oneMinusAlphaX);
                                y0blue  = Interpolate( BlueValue(pixelx0y0),  BlueValue(pixelx1y0), alphaX, oneMinusAlphaX);
                            }

                            // interpolate x0y1 x1y1 -> y1
                            unsigned char y1red;
                            unsigned char y1green;
                            unsigned char y1blue;
                            if (pixelx0y1 == pixelx1y1)
                            {
                                y1red   = RedValue(pixelx0y1);
                                y1green = GreenValue(pixelx0y1);
                                y1blue  = BlueValue(pixelx0y1);
                            }
                            else
                            {
                                y1red   = Interpolate(  RedValue(pixelx0y1),   RedValue(pixelx1y1), alphaX, oneMinusAlphaX);
                                y1green = Interpolate(GreenValue(pixelx0y1), GreenValue(pixelx1y1), alphaX, oneMinusAlphaX);
                                y1blue  = Interpolate( BlueValue(pixelx0y1),  BlueValue(pixelx1y1), alphaX, oneMinusAlphaX);
                            }

                            float yFraction = modff(sourceY, &ignored);
                            if (sourceY < 0)
                            {
                                yFraction = 1.0 + yFraction;
                            }

                            // interpolate y0 y1 -> pixel
                            const int alphaY         = (int)(yFraction * 4096.5);
                            const int oneMinusAlphaY = 4096 - alphaY;
                            imageBitsPtr[0] = Interpolate(  y0red, y1red,   alphaY, oneMinusAlphaY);
                            imageBitsPtr[1] = Interpolate(y0green, y1green, alphaY, oneMinusAlphaY);
                            imageBitsPtr[2] = Interpolate( y0blue, y1blue,  alphaY, oneMinusAlphaY);
                        }
                    }

                    sourceX += deltaSourceX;
                    sourceY += deltaSourceY;
                    imageBitsPtr += 3;
                }
            }

            WriteWrappedRectangle(
                image,
                PaintDeviceContext,
                xScreenOffset - boundingBoxWidth / 2,
                yScreenOffset - boundingBoxHeight / 2,
                boundingBoxWidth,
                boundingBoxHeight);
        }
        else
        {
            // Non-rotating bitmaps can be copied to the target context without rotation.
            BlitWithBitMode(
                PaintDeviceContext,
                +dest.x + xoffset,
                -dest.y + yoffset + LL - bitmapHeight,
                bitmapWidth,
                bitmapHeight,
                turtleBitmapDc,
                0,
                0,
                turtle->BitmapRasterMode);
        }
    }
    else
    {
        // notify the user that this turtle has no picture
        wchar_t errorMessage[255];
        swprintf(
            errorMessage,
            ARRAYSIZE(errorMessage),
            g_LOCALIZED_ERROR_TURTLEHASNOPICTURE,
            TurtleToPaste);
        ndprintf(
            stdout,
            ENCODING_NotApplicable,
            MESSAGETYPE_Error,
            false, // format for display
            errorMessage);

        // un-bitmap this turtle to prevent future errors
        turtle->BitmapRasterMode = 0;
        turtle->IsSprite         = false;
    }
}

void paste_all_turtles(wxDC & DeviceContext, FLONUM zoom)
{
    for (int i = 0; i <= g_MaxTurtle; i++)
    {
        if (g_Turtles[i].IsShown)
        {
            if (g_Turtles[i].BitmapRasterMode)
            {
                DeviceContext.SetLogicalFunction(wxCOPY);
                turtlepaste(DeviceContext, i);
            }
            else
            {
#ifndef __WXGTK__
                // wxINVERT seems to be broken in wxWidgets 3.0.2 for GTK.
                // If this were called, then nothing would be drawn.
                DeviceContext.SetLogicalFunction(wxINVERT);
#endif

                if (g_Turtles[i].BodyOutlineIsValid)
                {
                    // Create a closed path for the turtle and scale
                    // the zoom factor.
                    wxPoint points[] =
                    {
                        wxPoint(
                            g_Turtles[i].BodyOutline[0].x * zoom,
                            g_Turtles[i].BodyOutline[0].y * zoom),
                        wxPoint(
                            g_Turtles[i].BodyOutline[1].x * zoom,
                            g_Turtles[i].BodyOutline[1].y * zoom),
                        wxPoint(
                            g_Turtles[i].BodyOutline[2].x * zoom,
                            g_Turtles[i].BodyOutline[2].y * zoom),
                        wxPoint(
                            g_Turtles[i].BodyOutline[0].x * zoom,
                            g_Turtles[i].BodyOutline[0].y * zoom),
                    };

                    DeviceContext.DrawLines(ARRAYSIZE(points), points);
                }

                // Draw the "segments".  In PERSPECTIVE mode, this may contain
                // parts of the turtle's outline which are visible.  It also
                // contains the "right side" indicator.
                for (size_t j = 0; j < g_Turtles[i].TotalSegments; j++)
                {
                    wxPoint points[] =
                    {
                        wxPoint(
                            g_Turtles[i].Segments[j].From.x * zoom,
                            g_Turtles[i].Segments[j].From.y * zoom),
                        wxPoint(
                            g_Turtles[i].Segments[j].To.x * zoom,
                            g_Turtles[i].Segments[j].To.y * zoom),
                    };

                    DeviceContext.DrawLines(ARRAYSIZE(points), points);
                }
            }
        }
    }
}

NODE *lscrollx(NODE *args)
{
    ASSERT_TURTLE_INVARIANT;

    // Read the argument
    FIXNUM delta = g_round(flonum_arg(car(args)));

    if (NOT_THROWING)
    {
        if (delta == 0)
        {
            // SCROLLX 0 centers the scrollbar.
            // The center scroll position is the number of pixels to the center
            // of the drawing surface minus half the width of the screen window.
            wxRect screenRect = GetScreenWxWindow()->GetClientRect();
            SetScreenScrollPosition(
                0.5 * ((BitMapWidth * the_zoom) - screenRect.GetWidth()),
                GetScreenVerticalScrollPosition());
        }
        else
        {
            SetScreenScrollPosition(
                GetScreenHorizontalScrollPosition() + delta,
                GetScreenVerticalScrollPosition());
        }
    }

    return Unbound;
}

NODE *lscrolly(NODE *args)
{
    ASSERT_TURTLE_INVARIANT;

    // Read the argument.
    FIXNUM delta = g_round(flonum_arg(car(args)));

    if (NOT_THROWING)
    {
        if (delta == 0)
        {
            // SCROLLY 0 centers the scrollbar.
            // The center scroll position is the number of pixels to the center
            // of the drawing surface minus half the height of the screen window.
            wxRect screenRect = GetScreenWxWindow()->GetClientRect();
            SetScreenScrollPosition(
                GetScreenHorizontalScrollPosition(),
                0.5 * ((BitMapHeight * the_zoom) - screenRect.GetHeight()));
        }
        else
        {
            SetScreenScrollPosition(
                GetScreenHorizontalScrollPosition(),
                GetScreenVerticalScrollPosition() + delta);
        }
    }

    return Unbound;
}

NODE *lsetfocus(NODE *arg)
{
    ASSERT_TURTLE_INVARIANT;
    CStringPrintedNode windowCaption(car(arg));

    if (0 == wxStricmp(L"FMSLogo", windowCaption) ||
        0 == wxStricmp(L"MSWLogo Screen", windowCaption) ||
        0 == wxStricmp(L"FMSLogo Screen", windowCaption))
    {
        // special-case: set the focus on the screen
        GetScreenWxWindow()->SetFocus();
    }
    else
    {
#ifndef WX_PURE
        // get handle to Window with arg as Caption
        HWND window = FindWindow(NULL, windowCaption);

        // Now set focus to it, if it exists
        if (window != NULL)
        {
            ::SetFocus(window);
        }
#endif
    }

    return Unbound;
}

NODE *lgetfocus(NODE *)
{
    ASSERT_TURTLE_INVARIANT;

    wchar_t textbuf[MAX_BUFFER_SIZE];
    memset(textbuf, 0, sizeof(wchar_t) * MAX_BUFFER_SIZE);

#ifndef WX_PURE
    // Get handle to active window
    HWND TempH = GetActiveWindow();

    // It better exist, get its caption
    if (TempH != NULL)
    {
        ::GetWindowText(TempH, textbuf, ARRAYSIZE(textbuf));
    }
#endif

    // Return caption as a list
    NODE * arg = make_strnode(textbuf);
    NODE * val = arg; // parser(arg, false);
    return val;
}

NODE *lwindowset(NODE *args)
{
    CStringPrintedNode caption(car(args));

    int mode = getint(nonnegative_int_arg(cdr(args)));

    if (NOT_THROWING)
    {
        // For compatibility with other translations of FMSLogo (with
        // possibly alternate names for the commander), we always treat
        // the window "commander" as meaning the commander.
        if (0 == wxStricmp(caption, "commander"))
        {
            // For backward compatibility with MSWLogo, we must undock
            // the commander into its own window so that window operations
            // on it have the expected effect.
            UndockCommanderWindow();

            wxWindow * commander = GetCommanderWxWindow();

            // Set the state of the commander, preferring wxWidgets functions.
            // This makes make wxWindow::IsShown() behave correctly.
            switch (mode)
            {
            case 0: // SW_HIDE
                commander->Show(false);
                break;

            case 5: // SW_SHOW
                commander->Show(true);
                break;
#ifdef __WXMSW__
            default:
                // For all other modes, call ::ShowWindow() on win32.
                // This preserves backward compatibility.
                ShowWindow(static_cast<HWND>(commander->GetHandle()), mode);
                break;
#else
            // The rest of the SW_ flags may not work exactly the same
            // as they do in MSWLogo, but they are close enough for
            // other platforms.

            case 3: // SW_MAXIMIZE
                static_cast<CCommanderDialog*>(commander)->Maximize(true);
                break;

            case 6: // SW_MINIMIZE
                static_cast<CCommanderDialog*>(commander)->Iconize(true);
                break;

            case 9: // SW_RESTORE
                if (static_cast<CCommanderDialog*>(commander)->IsMaximized())
                {
                    // wxWidgets documentation says that Restore()
                    // won't work on GTK if the window is maximized.
                    static_cast<CCommanderDialog*>(commander)->Maximize(false);
                }
                else
                {
                    static_cast<CCommanderDialog*>(commander)->Restore();
                }
                break;
#endif
            }
        }
        else
        {
#ifndef WX_PURE
            // get handle to Window with arg as Caption
            // Note that we can't use wxFindWindow() because that only
            // finds windows which were created by wxWidgets.
            HWND window = FindWindow(NULL, caption);

            // if it exists set its state.
            if (window != NULL)
            {
                ShowWindow(window, mode);
            }
#endif
        }
    }

    return Unbound;
}

void ibm_clear_screen()
{
    ASSERT_TURTLE_INVARIANT;

    wxColour screenColor(dscn.red, dscn.green, dscn.blue);
    wxBrush  screenBrush(screenColor);

    // Reset the on-screen bitmap.
    wxDC * memoryDeviceContext = GetWxMemoryDeviceContext();
    memoryDeviceContext->SetBackground(screenColor);
    memoryDeviceContext->Clear();

    // Refresh the screen window so that it will repainted
    // to match the memory device context.
    GetScreenWxWindow()->Refresh(false);
}


// A class for collecting the font face names that are supported
// by the current system.
class CFontFaceNameCollector : public wxFontEnumerator
{
public:
    CFontFaceNameCollector()
    {
        EnumerateFacenames();
    }

    virtual bool OnFacename(const wxString & CurrentFontFaceName)
    {
        // Create a new node for this font face name.
        // By convention font face names are a list containing a string.
        NODE * fontFaceNode = cons_list(make_strnode(CurrentFontFaceName));

        // Append the font face name to the end of the list.
        m_FontFaceNames.AppendElement(fontFaceNode);

        return true; // keep enumerating fonts
    }

    NODE * GetList()
    {
        return m_FontFaceNames.GetList();
    }

private:
    CAppendableList m_FontFaceNames;
};


#ifndef WX_PURE

static
int CALLBACK
FindFont(
    const LOGFONT    *  LogFont,     // pointer to logical-font data
    const TEXTMETRIC *  TextMetric,  // pointer to physical-font data
    unsigned long       FontType,    // type of font
    LPARAM              lParam       // address of application-defined data
    )
{
    struct font_find_t * context = reinterpret_cast<struct font_find_t *>(lParam);

    // check for a match
    if (wxStricmp(context->fontname, LogFont->lfFaceName) == 0)
    {
        // take a copy, flag one was found and return
        FontRec = *LogFont;
        context->found = true;
        return 0;  // stop enumerating fonts
    }

    return 1; // keep enumerating fonts
}

#endif // WX_PURE

static
void
setfont(
    const wchar_t * fontname
    )
{
    if (wxFontEnumerator::IsValidFacename(fontname))
    {
#ifdef __WXMSW__
    // set the font name that we're looking for (FontRec will get filled if found)
    struct font_find_t context;
    context.found    = false;
    context.fontname = fontname;
    EnumFontFamilies(
        GetScreenDeviceContext(),
        NULL,
        FindFont,
        reinterpret_cast<LPARAM>(&context));
#else
    g_LabelFontInfo.FaceName(fontname);
#endif
    }
    else
    {
        // The font wasn't found.  Print all available font faces.
        printfx(g_LOCALIZED_ERROR_FONTNOTFOUND, fontname);

        // Enumerate all font names on the system.
        CFontFaceNameCollector fontCollector;

        // Print each font name.
        for (NODE * fontNameList = fontCollector.GetList();
             fontNameList != NIL;
             fontNameList = cdr(fontNameList))
        {
            ndprintf(
                stdout, // print directly to the commander (no redirection)
                ENCODING_NotApplicable, //
                MESSAGETYPE_Normal,
                false,  // print without escaping
                L"%s\n",
                car(fontNameList));
        }

        gcref(fontCollector.GetList());
    }
}

bool
HtmlHelpInitialize(
    void
    )
{
    if (g_HelpController != NULL)
    {
        // The Help subsystem has already been initialized
        return true;
    }

#ifdef __WXMSW__
    // For compatibility with MSWLogo, the deskop is the parent of the help.
    // This ensures that the window for the main environment can be positioned
    // over the help and gives the help window a unique icon.
    g_HelpParent = new wxWindow();
    g_HelpParent->SetHWND(GetDesktopWindow());
    g_HelpParent->AdoptAttributesFromHWND();

    g_HelpController = new wxHelpController(g_HelpParent);
#else
    g_HelpController = new wxHelpController();
#endif


    // This is the only place that reads g_HelpFileName,
    // so its allocation should be part of this
    // lazy initialization.
    if (!g_HelpController->Initialize(*g_HelpFileName))
    {
        wxMessageBox(
            g_LOCALIZED_ERROR_HHCTRLNOTLOADED,
            g_LOCALIZED_ERROR);

        HtmlHelpUninitialize();
        return false;
    }

    // success
    return true;
}

void
HtmlHelpUninitialize(
    void
    )
{
    if (g_HelpController != NULL)
    {
        // Close the help.
        g_HelpController->Quit();

        delete g_HelpController;
        g_HelpController = NULL;

#ifdef __WXMSW__
        g_HelpParent->SetHWND(NULL);
        g_HelpParent->Destroy();
        g_HelpParent = NULL;
#endif
    }
}

// if arg is NULL then we jump to index
void do_help(const wchar_t *arg)
{
    if (!HtmlHelpInitialize())
    {
        return;
    }

    if (arg != NULL)
    {
        // A specific help topic was requested.
        g_HelpController->KeywordSearch(arg);
    }
    else
    {
        g_HelpController->DisplaySection(L"index.htm");
    }
}

NODE *lhelp(NODE *arg)
{
    // if arg then pass to do_help
    if (arg != NIL)
    {
        CStringPrintedNode item(car(arg));
        do_help(item);
    }
    else
    {
        // else just pop up help
        do_help(NULL);
    }

    return Unbound;
}

NODE *lwinhelp(NODE *arg)
{
    ASSERT_TURTLE_INVARIANT;
#ifndef WX_PURE
    CStringPrintedNode textbuf(car(arg));

    // if 2nd arg then pass to winhelp
    if (cdr(arg) != NIL)
    {
        CStringPrintedNode textbuf2(car(cdr(arg)));

        WinHelp(
            GetMainWindow(),
            textbuf,
            HELP_PARTIALKEY,
            reinterpret_cast<ULONG_PTR>(textbuf2.GetString()));
    }
    else
    {
        // else just give help on file (arg 1)
        WinHelp(GetMainWindow(), textbuf, HELP_INDEX, 0L);
    }
#endif
    return Unbound;
}

NODE *lfontfacenames(NODE *arg)
{
    // Enumerate all font names on the system.
    CFontFaceNameCollector fontFaceCollector;

    return fontFaceCollector.GetList();
}

class CListArgumentParser
{
private:
    NODE * m_List;
    int    m_ListLength; // -1 means error

public:
    CListArgumentParser(NODE * List) :
        m_List(List),
        m_ListLength(0)
    {
    }

    NODE * GetNodeField()
    {
        if (m_List == NIL || m_ListLength < 0)
        {
            // We already reached the end of the list or
            // we already found an error.
            return Unbound;
        }

        // Get the field.
        NODE * field = car(m_List);

        // Advance to the next field.
        m_List = cdr(m_List);
        m_ListLength++;

        return field;
    }

    FIXNUM GetIntField()
    {
        // Get the next element from the list.
        NODE * node = GetNodeField();
        if (node == Unbound)
        {
            // We already reached the end of the list or
            // we already found an error.
            return 0;
        }

        // Read it as an integer.
        node = cnv_node_to_intnode(node);
        if (node == Unbound)
        {
            // The next element in the list isn't an integer.
            // Return an error.
            m_ListLength = -1;
        }
 
        // Extract the integer from the field.
        FIXNUM value = getint(node);
        gcref(node);

        return value;
    }

    FIXNUM GetNonNegativeIntField()
    {
        FIXNUM value = GetIntField();
        if (value < 0)
        {
            // value is negative, which is an error.
            m_ListLength = -1;
        }

        return value;
    }

    void GetEndOfList()
    {
        if (m_List != NIL)
        {
            // The font vector is too long.
            m_ListLength = -1;
        }
    }

    int GetListLength() const
    {
        return m_ListLength;
    }
};

NODE *lsetlabelfont(NODE *args)
{
    ASSERT_TURTLE_INVARIANT;

    // Fields in the font vector argument.
    NODE * fontname;
    FIXNUM height;
    FIXNUM width;
    FIXNUM orientation;
    FIXNUM weight;
    FIXNUM italic;
    FIXNUM underline;
    FIXNUM strikeout;
    FIXNUM charset;
    FIXNUM out_precision;
    FIXNUM clip_precision;
    FIXNUM quality;
    FIXNUM pitch_and_family;

    NODE * font_vector = car(args);
    int    font_vector_length; // -1 indicates error
    do
    {
        if (is_list(font_vector))
        {
            CListArgumentParser font_vector_parser(font_vector);

            // Anything is legal for the first field, including not having one.
            if (font_vector != NIL)
            {
                fontname = font_vector_parser.GetNodeField();
            }

            height           = font_vector_parser.GetIntField();
            width            = font_vector_parser.GetNonNegativeIntField();
            orientation      = font_vector_parser.GetNonNegativeIntField();
            weight           = font_vector_parser.GetNonNegativeIntField();
            italic           = font_vector_parser.GetNonNegativeIntField();
            underline        = font_vector_parser.GetNonNegativeIntField();
            strikeout        = font_vector_parser.GetNonNegativeIntField();
            charset          = font_vector_parser.GetNonNegativeIntField();
            out_precision    = font_vector_parser.GetNonNegativeIntField();
            quality          = font_vector_parser.GetNonNegativeIntField();
            clip_precision   = font_vector_parser.GetNonNegativeIntField();
            pitch_and_family = font_vector_parser.GetNonNegativeIntField();

            // Check for extranous fields in the list.
            font_vector_parser.GetEndOfList();

            font_vector_length = font_vector_parser.GetListLength();
        }
        else
        {
            // input is not even a list.
            font_vector_length = -1;
        }

        // If the font vector is malformed, try to get a new one from erract.
        if (font_vector_length < 0)
        {
            font_vector = err_logo(BAD_DATA, car(args));
            setcar(args, font_vector); // free font_vector later 
        }
    } while (font_vector_length < 0 && NOT_THROWING);

    if (0 <= font_vector_length)
    {
        if (font_vector_length == 0)
        {
            // no font name was given.
            setfont(L"");
            return Unbound;
        }

        CStringPrintedNode textbuf(fontname);

        // Get the FontRec filled with a match to textbuf
        setfont(textbuf);

#ifndef WX_PURE
        // now fill in the rest of the (optional) fields if given
        if (2 <= font_vector_length)
        {
            FontRec.lfHeight = height;
        }
        if (3 <= font_vector_length)
        {
            FontRec.lfWidth = width;
        }
        if (4 <= font_vector_length)
        {
            FontRec.lfOrientation = orientation;
        }
        if (5 <= font_vector_length)
        {
            FontRec.lfWeight = weight;
        }
        if (6 <= font_vector_length)
        {
            FontRec.lfItalic = italic;
        }
        if (7 <= font_vector_length)
        {
            FontRec.lfUnderline = underline;
        }
        if (8 <= font_vector_length)
        {
            FontRec.lfStrikeOut = strikeout;
        }
        if (9 <= font_vector_length)
        {
            FontRec.lfCharSet = charset;
        }
        if (10 <= font_vector_length)
        {
            FontRec.lfOutPrecision = out_precision;
        }
        if (11 <= font_vector_length)
        {
            FontRec.lfClipPrecision = clip_precision;
        }
        if (12 <= font_vector_length)
        {
            FontRec.lfQuality = quality;
        }
        if (13 <= font_vector_length)
        {
             FontRec.lfPitchAndFamily = pitch_and_family;
        }
#endif
    }

    return Unbound;
}

NODE *llabelfont(NODE *)
{
    ASSERT_TURTLE_INVARIANT;
#ifdef WX_PURE
    return NIL;
#else
    // put the Font name in a list
    NODE *targ = make_strnode(FontRec.lfFaceName);
    NODE *val = parser(targ, false);

    // now return the whole thing as a list

    return
        cons(val,
        cons(make_intnode((FIXNUM) FontRec.lfHeight),
        cons(make_intnode((FIXNUM) FontRec.lfWidth),
        cons(make_intnode((FIXNUM) FontRec.lfOrientation),
        cons(make_intnode((FIXNUM) FontRec.lfWeight),
        cons(make_intnode((FIXNUM) FontRec.lfItalic),
        cons(make_intnode((FIXNUM) FontRec.lfUnderline),
        cons(make_intnode((FIXNUM) FontRec.lfStrikeOut),
        cons(make_intnode((FIXNUM) FontRec.lfCharSet),
        cons(make_intnode((FIXNUM) FontRec.lfOutPrecision),
        cons(make_intnode((FIXNUM) FontRec.lfClipPrecision),
        cons(make_intnode((FIXNUM) FontRec.lfQuality),
        cons(make_intnode((FIXNUM) FontRec.lfPitchAndFamily),
        NIL)))))))))))));
#endif
}

NODE *lmachine(NODE *)
{
    ASSERT_TURTLE_INVARIANT;

    // Get the operating system information
    int majorOsVersion;
    int minorOsVersion;
    wxOperatingSystemId operatingSystemId = wxGetOsVersion(
        &majorOsVersion,
        &minorOsVersion);

    // Get the desktop dimensions
    wxWindow * mainWindow = GetMainWxWindow();
    int ScreenWidth  = wxSystemSettings::GetMetric(wxSYS_SCREEN_X, mainWindow);
    int ScreenHeight = wxSystemSettings::GetMetric(wxSYS_SCREEN_Y, mainWindow);

    // Get the working area dimensions
    int workingAreaWidth;
    int workingAreaHeight;
    GetWorkingAreaDimensions(workingAreaWidth, workingAreaHeight);

    // Get FMSLogo's window dimensions
    wxSize mainWindowSize = mainWindow->GetSize();

    // Get Screen window's dimensions
    wxSize screenWindowSize = GetScreenWxWindow()->GetSize();

    // return a list with system specific information
    return
        cons(make_intnode((FIXNUM) (wxOS_WINDOWS_NT == operatingSystemId)),
        cons(make_intnode((FIXNUM) 32),
        cons(make_intnode((FIXNUM) BitMapWidth),
        cons(make_intnode((FIXNUM) BitMapHeight),
        cons(make_intnode((FIXNUM) 0),
        cons(make_intnode((FIXNUM) majorOsVersion),
        cons(make_intnode((FIXNUM) minorOsVersion),
        cons(make_intnode((FIXNUM) workingAreaWidth),
        cons(make_intnode((FIXNUM) workingAreaHeight),
        cons(make_intnode((FIXNUM) ScreenWidth),
        cons(make_intnode((FIXNUM) ScreenHeight),
        cons(make_intnode((FIXNUM) (mainWindowSize.GetWidth())),
        cons(make_intnode((FIXNUM) (mainWindowSize.GetHeight())),
        cons(make_intnode((FIXNUM) (screenWindowSize.GetWidth())),
        cons(make_intnode((FIXNUM) (screenWindowSize.GetHeight())),
        cons(make_intnode((FIXNUM) GetScreenHorizontalScrollPosition()),
        cons(make_intnode((FIXNUM) GetScreenVerticalScrollPosition()),
        cons(make_floatnode(the_zoom),
        NIL))))))))))))))))));
}

static const wxFont CreateLabelFont()
{
#ifdef __WXMSW__
    // Update the "escapement", which is the angle at which the text
    // is rendered (baseline angle), in 10ths of a degree from positive X.
    // Because the device contexts are in GM_COMPATIBLE mode, the
    // lfEscapement will take priority over the lfOrientation, so that
    // all characters follow the escapement.
    FontRec.lfEscapement = (360.0 - (g_SelectedTurtle->Heading - 90.0)) * 10;
    return wxFont(FontRec);
#else
    // get a handle to the label's font
    return wxFont(g_LabelFontInfo);
#endif
}

wxSize labelsize(const wchar_t * String)
{
    ASSERT_TURTLE_INVARIANT;

    wxCoord width = 0;
    wxCoord height = 0;

    // get a handle to the label's font
    const wxFont & tempFont = CreateLabelFont();
    if (tempFont.IsOk())
    {
        // Get the extents of String if it were written on the screen.
        // For compatibility with MSWLogo, this ignores the descent
        // and external leaning, leading to a tighter fit for the text.
        wxDC * screen = GetWxScreenDeviceContext();
        screen->GetTextExtent(String, &width, &height, NULL, NULL, &tempFont);
    }

    return wxSize(width, height);
}

void label(const wchar_t * String)
{
    ASSERT_TURTLE_INVARIANT;

    wxPoint dest;
    if (!WorldCoordinateToScreenCoordinate(g_SelectedTurtle->Position, dest))
    {
        return;
    }

    const wxFont & tempFont = CreateLabelFont();

    //
    // memory
    //
    wxDC * MemDC = GetWxMemoryDeviceContext();

    MemDC->SetBackgroundMode(wxTRANSPARENT);

    if (GetPenStateForSelectedTurtle().IsErasing)
    {
        MemDC->SetTextForeground(wxColor(scolor));
    }
    else
    {
        MemDC->SetTextForeground(wxColor(pcolor));
    }


    wxFont oldFont = MemDC->GetFont();
    MemDC->SetFont(tempFont);

#if defined __WXMSW__
    // wxDC::DrawRotatedText() doesn't handle line break characters
    // perfectly and is inconsistent with the bounding box that
    // llabelsize() returns.  Rather than only partially fixing how
    // LABEL handles line breaks, this code uses the native win32
    // API, as MSWLogo did.
    SetBkMode(MemDC->GetHDC(), TRANSPARENT);
    SetTextColor(MemDC->GetHDC(), MemDC->GetTextForeground().GetRGB());

    TextOut(
        MemDC->GetHDC(),
        +dest.x + xoffset,
        -dest.y + yoffset,
        String,
        wcslen(String));
#else
    MemDC->DrawRotatedText(
        String,
        +dest.x + xoffset,
        -dest.y + yoffset,
        360.0 - (g_SelectedTurtle->Heading - 90.0));
#endif

    // restore the original font
    MemDC->SetFont(oldFont);


    //
    // screen
    //

    if (zoom_flag)
    {
        GetScreenWxWindow()->Refresh(false);
    }
    else
    {
        // For some unknown reason, DrawRotatedText behaves differently on
        // ScreenDC than on MemDC.  Until this is understood, we always draw
        // on the screen by refreshing it (copying the memory to the screen)
        GetScreenWxWindow()->Refresh(false);
    }
}

void checkwindow(int *x, int *y, int *w, int *h)
{
    const wxRect & displayRect = wxGetClientDisplayRect();

    int minX      = displayRect.GetLeft();
    int minY      = displayRect.GetTop();
    int maxWidth  = displayRect.GetWidth();
    int maxHeight = displayRect.GetHeight();

    // sanity check window coordinates
    if (*x < minX)
    {
        *x = minX;
    }
    if (*y < minY)
    {
        *y = minY;
    }
    if (*w > maxWidth)
    {
        *w = maxWidth;
    }
    if (*h > maxHeight)
    {
        *h = maxHeight;
    }

    // Move the window so that it's on-screen.
    if (*x + *w > displayRect.GetRight())
    {
        *x = displayRect.GetRight() - *w;
    }
    if (*y + *h > displayRect.GetBottom())
    {
        *y = displayRect.GetBottom() - *h;
    }
}

void MyMessageScan()
{
    // depending on yield flag check for messages
    if (yield_flag)
    {
#ifdef __WXMSW__
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            // If this is a keydown event, then it might be a keyboard shortcut.
            // Checking this before calling TranslateKeyboardShortcut is an
            // optimization.
            if (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN)
            {
                if (TranslateKeyboardShortcut(msg))
                {
                    // This message represents a keyboard shortcut and it
                    // has been and dispatched appropriately.
                    // Move on to the next message.
                    continue;
                }
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
#else
        while (wxTheApp->Pending())
        {
            wxTheApp->Dispatch();
        }
#endif
    }
}


void init_videomode()
{
#ifdef WX_PURE
    // Choose dialog units that map 1:2 to pixels
    // This creates dialog close to the way MSWLogo did.
    BaseUnitsx = 8;
    BaseUnitsy = 16;
#else
    // Get Dialog Units for Controls
    BaseUnitsx = LOWORD(GetDialogBaseUnits());
    BaseUnitsy = HIWORD(GetDialogBaseUnits());
#endif
}

void init_bitmaps()
{
    // allocate the array of bitmaps
    g_BitmapsLimit   = 1;
    g_Bitmaps        = (CUTMAP *) calloc(sizeof(*g_Bitmaps), g_BitmapsLimit);
    g_SelectedBitmap = &g_Bitmaps[0];

#if defined(__WXGTK__)
    // On GTK, a PNG handler is required in order to copy bitmaps to the clipboard.
    wxImage::AddHandler(new wxPNGHandler);
#endif
}

void uninit_bitmaps()
{
    for (CUTMAP* bmp = g_Bitmaps; bmp < g_Bitmaps + g_BitmapsLimit; bmp++)
    {
        delete bmp->Bitmap;
        free(bmp->Pixels);
    }

    free(g_Bitmaps);
}

void init_penstate()
{
    g_PenState.Color.red   = 0x00;
    g_PenState.Color.green = 0x00;
    g_PenState.Color.blue  = 0x00;
    g_PenState.Width     = 1;
    g_PenState.Mode      = wxCOPY;
    g_PenState.IsErasing = false;
}


void init_turtles()
{
    g_TurtlesLimit = 1;
    g_MaxTurtle    = 0;
    g_Turtles      = (Turtle *) calloc(sizeof(*g_Turtles), g_TurtlesLimit);
    g_SelectedTurtle = &g_Turtles[g_MaxTurtle];

    InitializeTurtle(g_SelectedTurtle);

    // init the special turtles
    g_SpecialTurtles[SPECIAL_TURTLE_EYE_LOCATION].IsSpecial   = true;
    g_SpecialTurtles[SPECIAL_TURTLE_LIGHT_LOCATION].IsSpecial = true;
    g_SpecialTurtles[SPECIAL_TURTLE_EYE_FIXATION].IsSpecial   = true;
}

void uninit_turtles()
{
    free(g_Turtles);
}

void exit_program()
{
    if (is_executing())
    {
        IsTimeToHalt = true;
    }
    IsTimeToExit = true;
}
