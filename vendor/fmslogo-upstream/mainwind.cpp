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
   #include "mainwind.h"

   #include <algorithm>

   #include <wx/bitmap.h>
   #include <wx/crt.h>
   #include <wx/dc.h>
   #include <wx/dcmemory.h>
   #include <wx/gdicmn.h>
   #include <wx/image.h>
   #include <wx/imagjpeg.h>
   #include <wx/window.h>
   #include <wx/wfstream.h>
   #ifdef __WXMSW__
     #include <wx/msw/dc.h> // for wxDCTemp
   #endif

   #include "const.h"
   #include "activearea.h"
   #include "screenwindow.h"
   #include "startup.h"
   #include "graphwin.h"
   #include "cursor.h"
   #include "fmslogo.h"
   #include "debugheap.h"

   #ifdef max // MS compilers #define max in windows.h
      #undef max
   #endif

   #ifdef min // MS compilers #define min in windows.h
      #undef min
   #endif

#endif

#ifndef __WXMSW__
    // define the bitmap structures on non-windows platforms
    #include <stdint.h>

    typedef uint32_t RGBQUAD;

    typedef struct tagBITMAPINFOHEADER {
        uint32_t biSize;
        int32_t  biWidth;
        int32_t  biHeight;
        int16_t  biPlanes;
        int16_t  biBitCount;
        uint32_t biCompression;
        uint32_t biSizeImage;
        int32_t  biXPelsPerMeter;
        int32_t  biYPelsPerMeter;
        uint32_t biClrUsed;
        uint32_t biClrImportant;
    } BITMAPINFOHEADER;

    typedef struct tagBITMAPINFO {
        BITMAPINFOHEADER bmiHeader;
        RGBQUAD          bmiColors[1];
    } BITMAPINFO;

    #define BI_RGB 0

#endif

// Writes the contents of the screen window that is
// within the ACTIVEAREA window to the screen to the given
// file stream as a Windows Bitmap.
//
// Filename - The name of the file to save the bitmap to.
// MaxBitmapBitDepth - the bitcount to use for the bitmap.
//    Must be 1, 4, 8, 16, 24, or 32.
//
// Returns SUCCESS if the bitmap was successfully written.
// Returns an error code, otherwise.
static
ERR_TYPES WriteDIB(const wchar_t * Filename, int MaxBitmapBitDepth)
{
    ERR_TYPES status = SUCCESS;

    // grab a DC
    wxDC * dc = GetWxMemoryDeviceContext();

    // Determine the bit depth of the screen.
    int bitmapBitDepth = dc->GetDepth();

    // If we were asked to save a bitmap with a very low bit-depth
    // (one that uses a palette), then honor the request.
    if (MaxBitmapBitDepth < bitmapBitDepth)
    {
        if ((MaxBitmapBitDepth == 1) || (MaxBitmapBitDepth == 4) || (MaxBitmapBitDepth == 8))
        {
            bitmapBitDepth = MaxBitmapBitDepth;
        }
    }

#ifndef WX_PURE
    // If we're saving a true-color bitmap, use 24bpp.
    // Note: This was copied from MSWLogo.  The intention isn't clear.
    // It's also not clear why MaxBitmapBitDepth is mostly ignored.
    // I suspect that MSWLogo uses 24 for all true-color bitmaps because
    // 16 isn't universally supported and 24 leads to smaller files than 32
    // without any loss of image fidelity.
    if (bitmapBitDepth == 16 || bitmapBitDepth == 32)
    {
        bitmapBitDepth = 24;
    }
#endif

    if (bitmapBitDepth == dc->GetDepth() && 8 < bitmapBitDepth)
    {
        // Note: wxBitmap::SaveFile does not write a palette, so we can
        // only use it when writing true-color bitmaps.
        wxRect activeArea = wxRect(
            g_PrinterAreaXLow  + BitMapWidth / 2,    // x
            g_PrinterAreaYLow  + BitMapHeight / 2,   // y
            g_PrinterAreaXHigh - g_PrinterAreaXLow,  // width
            g_PrinterAreaYHigh - g_PrinterAreaYLow); // height

        wxBitmap originalBitmap = dc->GetAsBitmap(&activeArea);
        bool isOk = originalBitmap.SaveFile(Filename, wxBITMAP_TYPE_BMP);
        if (!isOk)
        {
            status = IMAGE_BMP_WRITE_FAILED;
        }
    }
    else
    {
        // open and check if ok
        FILE* file = wxFopen(Filename, "wb");
        if (file == NULL)
        {
            return IMAGE_BMP_OPEN_FAILED;
        }

        size_t size;
        if (bitmapBitDepth <= 8)
        {
            // Allocate space for a palette of 2^bitmapBitDepth colors
            size = sizeof(BITMAPINFOHEADER) + ((1 << bitmapBitDepth) * sizeof(RGBQUAD));
        }
        else
        {
            size = sizeof(BITMAPINFOHEADER);
        }

        char * bitmapInfoBuffer = new char[size];
        BITMAPINFO * bitmapInfo = reinterpret_cast<BITMAPINFO *>(bitmapInfoBuffer);

        const int bitmapWidth  = g_PrinterAreaXHigh - g_PrinterAreaXLow;
        const int bitmapHeight = g_PrinterAreaYHigh - g_PrinterAreaYLow;

        bitmapInfo->bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
        bitmapInfo->bmiHeader.biWidth         = bitmapWidth;
        bitmapInfo->bmiHeader.biHeight        = bitmapHeight;
        bitmapInfo->bmiHeader.biPlanes        = 1;
        bitmapInfo->bmiHeader.biBitCount      = bitmapBitDepth;
        bitmapInfo->bmiHeader.biCompression   = BI_RGB;
        bitmapInfo->bmiHeader.biXPelsPerMeter = 0;
        bitmapInfo->bmiHeader.biYPelsPerMeter = 0;
        bitmapInfo->bmiHeader.biClrUsed       = 0;
        bitmapInfo->bmiHeader.biClrImportant  = 0;

        const int rowBits = bitmapInfo->bmiHeader.biWidth * bitmapInfo->bmiHeader.biBitCount;
        const int lineBytes = ((rowBits + 31) / 32) * 4; // pad to 32-bit alignment
        bitmapInfo->bmiHeader.biSizeImage = lineBytes * bitmapInfo->bmiHeader.biHeight;

        // If the bitmap is too large, then the call to GetDIBits() below will fail.
        // To account for this, we repeatedly call it for a smaller region until we
        // have extracted the entire bitmap.
        const size_t TARGET_BUFFER_SIZE = 1024 * 1024;
        size_t dibBufferSize;
        size_t areaBitmapHeight;
        wxBitmap bitmapToSave;
        if (8 < bitmapBitDepth && TARGET_BUFFER_SIZE < bitmapInfo->bmiHeader.biSizeImage)
        {
            // Get enough lines so that we reach our target buffer size.
            // Always round up, so that we won't ever end up getting zero
            // bytes in each iteration.
            areaBitmapHeight = (TARGET_BUFFER_SIZE + lineBytes - 1) / lineBytes;
            dibBufferSize    = lineBytes * areaBitmapHeight;

            // GetDiBits() fails if we try to get the bitmap data in a piecemeal
            // fashion, so we must BLT the parts we want to a separate bitmap
            // and then call GetDiBits() on that.
            bitmapToSave.Create(
                bitmapWidth,
                areaBitmapHeight,
                dc->GetDepth());
        }
        else
        {
#ifdef WX_PURE
            status = FILE_ERROR;  // TODO: add NOT_SUPPORTED
#else
            // Put the entire bitmap into a single buffer.
            areaBitmapHeight = bitmapInfo->bmiHeader.biHeight;
            dibBufferSize    = bitmapInfo->bmiHeader.biSizeImage;
            if (IsActiveAreaOneToOneWithScreen())
            {
                // As an optimization, we can extract the bitmap data directly from
                // the memory bitmap without any BLTs.
                bitmapToSave.InitFromHBITMAP(
                    MemoryBitMap,
                    bitmapWidth,
                    areaBitmapHeight,
                    dc->GetDepth());
            }
            else
            {
                // The active area has a different size than the screen,
                // so we must BLT the area we want to a bitmap of the desired
                // size that matches the memory bitmap's format.
                bitmapToSave.Create(
                    bitmapWidth,
                    areaBitmapHeight,
                    dc->GetDepth());
            }
        }

        if (!bitmapToSave.IsOk())
        {
            status = OUT_OF_MEM;
        }
        else
        {
            // Allocate space for the raw DIB data
            void * dibBuffer = malloc(dibBufferSize);
            if (dibBuffer == NULL)
            {
                status = OUT_OF_MEM;
            }
            else
            {
                // build file header
                BITMAPFILEHEADER bitmapFileHeader;
                bitmapFileHeader.bfType = 19778;
                bitmapFileHeader.bfSize = sizeof(bitmapFileHeader) + size + bitmapInfo->bmiHeader.biSizeImage;
                bitmapFileHeader.bfReserved1 = 0;
                bitmapFileHeader.bfReserved2 = 0;
                bitmapFileHeader.bfOffBits = size + sizeof(BITMAPFILEHEADER);

                // Write out the file header.
                fwrite(&bitmapFileHeader, sizeof bitmapFileHeader, 1, file);

                if (bitmapToSave.GetHBITMAP() == MemoryBitMap)
                {
                    // Convert the device-dependent bitmap to raw DIB in dibBuffer.
                    int rval = GetDIBits(
                        dc->GetHDC(),
                        MemoryBitMap,
                        0,
                        bitmapHeight,
                        dibBuffer,
                        bitmapInfo,
                        DIB_RGB_COLORS);
                    if (rval != bitmapHeight)
                    {
                        status = IMAGE_GENERAL;
                    }

                    // Write out the bitmap header (and optional palette)
                    fwrite(bitmapInfo, sizeof(char), size, file);

                    // write out raw DIB data to file
                    fwrite(dibBuffer, sizeof(char), dibBufferSize, file);
                }
                else
                {
                    // Write out the bitmap header.  For large bitmaps, we change the
                    // header below, so we want to write it out while it still reflects
                    // the overall bitmap.
                    fwrite(bitmapInfo, sizeof(bitmapInfo->bmiHeader), 1, file);

                    // Select the correctly-size bitmap.
                    wxMemoryDC areaMemoryDC(dc);
                    areaMemoryDC.SelectObject(bitmapToSave);


                    // Read the bitmap in chunks of areaBitmapHeight scanlines
                    // so that GetDIBits() doesn't fail.
                    for (int linesWritten = 0;
                         linesWritten < bitmapHeight;
                         linesWritten += areaBitmapHeight)
                    {
                        // Figure out how many lines we should read, being
                        // careful to not read past the end of the bitmap.
                        int linesToGet;
                        if (bitmapHeight < linesWritten + areaBitmapHeight)
                        {
                            linesToGet = bitmapHeight - linesWritten;
                        }
                        else
                        {
                            linesToGet = areaBitmapHeight;
                        }

                        // Copy the correct portion from memory to the temporary bitmap.
                        // Because the bits in a bitmap are drawn bottom-up, we request
                        // the bottom chunks first and then move up chunk by chunk.
                        bool isOk = areaMemoryDC.Blit(
                            0,
                            0,
                            bitmapWidth,
                            linesToGet,
                            dc,
                            xoffset + g_PrinterAreaXLow,
                            yoffset - g_PrinterAreaYLow - linesWritten - linesToGet);
                        if (!isOk)
                        {
                            status = IMAGE_GENERAL;
                            break;
                        }

                        bitmapInfo->bmiHeader.biHeight    = linesToGet;
                        bitmapInfo->bmiHeader.biSizeImage = lineBytes * linesToGet;

                        // Convert the device-dependent bitmap to raw DIB in dibBuffer.
                        int rval = GetDIBits(
                            areaMemoryDC.GetHDC(),
                            bitmapToSave.GetHBITMAP(),
                            0,
                            linesToGet,
                            dibBuffer,
                            bitmapInfo,
                            DIB_RGB_COLORS);
                        if (rval != linesToGet)
                        {
                            status = IMAGE_GENERAL;
                            break;
                        }

                        // Write out the palette for paletted bitmaps.
                        // We always write out the entire bitmap in one iteration for paletted
                        // bitmaps, so there's no risk of writing the palette more than once.
                        if (bitmapBitDepth <= 8)
                        {
                            fwrite(&bitmapInfo->bmiColors, sizeof(char), size - sizeof(bitmapInfo->bmiHeader), file);
                        }

                        // write out raw DIB data to file
                        fwrite(dibBuffer, sizeof(char), linesToGet * lineBytes, file);
                    }
                }

                // Don't close the MemoryBitMap when bitmapToSave is deleted.
                if (bitmapToSave.GetHBITMAP() == MemoryBitMap)
                {
                    bitmapToSave.ResetHBITMAP();
                }

                // restore some of the resources
                free(dibBuffer);
            }
#endif // WX_PURE
        }

        delete [] bitmapInfoBuffer;

        int rval = fclose(file);
        if (rval != 0)
        {
            status = IMAGE_BMP_WRITE_FAILED;
        }
    }

    if (status != SUCCESS)
    {
        // The bitmap was not written out correctly.
        // Remove the file that we created, instead
        // of leaving around a corrupt file.
        wxRemoveFile(Filename);
    }

    return status;
}

ERR_TYPES DumpBitmapFile(const wchar_t * Filename, int MaxBitCount)
{
    // Load hour-glass cursor.
    lsetcursorwait(NIL);

    // do it and if error then let user know
    ERR_TYPES status = WriteDIB(Filename, MaxBitCount);

    // Restore the arrow cursor
    lsetcursorarrow(NIL);

    return status;
}

// Attempt to open load an image to the screen.
// File         - an open file handle to the image file.
// dwPixelWidth - if 0, then the image is not drawn to the screen.
//                On success, this is set to the width of the image.
// dwPixelWidth - On success, this is set to the height of the image.
static
bool
LoadImage(
    wxFFile      & File,
    unsigned int & dwPixelWidth,
    unsigned int & dwPixelHeight
    )
{
    wxFFileInputStream inputStream(File);

    try
    {
        // First, try loading as a Windows Device Independent Bitmap.
        wxImage image;
        image.LoadFile(inputStream, wxBITMAP_TYPE_BMP);
        if (!image.IsOk())
        {
            // It's not a Windows Bitmap.
            // Try loading it as a JPEG.
            wxJPEGHandler jpegHandler;
            jpegHandler.LoadFile(&image, inputStream, false);
            if (!image.IsOk())
            {
                return false;
            }
        }

        // save some typing
        DWORD NewPixelWidth  = image.GetWidth();
        DWORD NewPixelHeight = image.GetHeight();
        if (dwPixelWidth)
        {
            wxBitmap bitmap(image);

            // Get the drawing surface's device context.
            wxDC* memoryDC = GetWxMemoryDeviceContext();

            /* if either dimension is more than half then put in corner */
            if ((NewPixelWidth  > (BitMapWidth / 2)) ||
                (NewPixelHeight > (BitMapHeight / 2)))
            {
                memoryDC->DrawBitmap(bitmap, 0, 0);

                // Invalidate the entire screen to repaint it.
                GetScreenWxWindow()->Refresh(false);
            }
            else
            {
                /* else put at turtle */
                wxPoint dest;

                if (current_mode == perspectivemode)
                {
                    VECTOR from3d;

                    from3d.x = g_SelectedTurtle->Position.x / WorldWidth;
                    from3d.y = g_SelectedTurtle->Position.y / WorldHeight;
                    from3d.z = g_SelectedTurtle->Position.z / WorldDepth;

                    if (!ThreeD.TransformPoint(from3d, dest))
                    {
                        // The turtle is entirely off-screen.
                        // In this case, we don't draw the image on the
                        // screen, even if part of it could be shown.
                        return true;
                    }
                }
                else
                {
                    dest.x = g_round(g_SelectedTurtle->Position.x);
                    dest.y = g_round(g_SelectedTurtle->Position.y);
                }

                memoryDC->DrawBitmap(
                    bitmap,
                    +dest.x + xoffset,
                    -dest.y + yoffset + LL - NewPixelHeight);

                // Invalidate the entire screen to repaint it.
                GetScreenWxWindow()->Refresh(false);
            }
        }

        dwPixelWidth  = NewPixelWidth;
        dwPixelHeight = NewPixelHeight;
        return true;
    }
    catch (std::bad_alloc &)
    {
        // wxImage sometimes tries to allocate very large blocks of
        // memory when given a corrupt image.  For example, if the
        // biClrUsed is large or negative, then wxImage attempts to
        // allocate a large array to hold the palette.
        return false;
    }
}

// Attempt to open load an image to the screen.
// Filename     - the name of the image file.
// dwPixelWidth - if 0, then the image is not drawn to the screen.
//                On success, this is set to the width of the image.
// dwPixelWidth - On success, this is set to the height of the image.
ERR_TYPES
LoadBitmapFile(
    const wchar_t * Filename,
    unsigned int  & dwPixelWidth,
    unsigned int  & dwPixelHeight
    )
{
    // Test if Filename is a Windows 3.0 DIB bitmap and if so read it

    // open then check if open
    wxFFile file(Filename, "rb");
    if (!file.IsOpened())
    {
        return IMAGE_BMP_OPEN_FAILED;
    }

    // Show the hour-glass cursor.
    lsetcursorwait(NIL);

    // Load the image.
    ERR_TYPES status;
    if (LoadImage(file, dwPixelWidth, dwPixelHeight))
    {
        status = SUCCESS;
    }
    else
    {
        status = IMAGE_BMP_INVALID;
    }

    // Restore the arrow cursor.
    lsetcursorarrow(NIL);

    return status;
}

static wxCoord ClipToRange(FLONUM Min, FLONUM Value, FLONUM Max)
{
    return static_cast<wxCoord>(std::min(std::max(Min, Value), Max));
}

void
PaintToScreen(
    wxDC     & PaintDeviceContext,
    wxRegion   UpdateRegion,
    int        ScrollX,
    int        ScrollY
    )
{
    // This is a compromise between speed and memory (as is most code).
    // All drawing is written to the backing store 1 to 1 even when zoomed.
    // When zoomed all drawing and painting is scaled to the display on the fly.
    // Painting can be a bit slow while zoomed. It also can be inaccurate when
    // mixing scaled painting and scaled drawing. Printing is never zoomed.
    // User can use Bitfit if he/she wants data scaled.

    // Use a back buffer if we are drawing bitmapped turtles.
    bool useBackBuffer = false;
    for (int j = 0; j <= g_MaxTurtle; j++)
    {
        if (g_Turtles[j].IsShown && g_Turtles[j].BitmapRasterMode != 0)
        {
            useBackBuffer = true;
            break;
        }
    }

    wxBitmap * backBuffer;
    wxDC     * sourceDeviceContext;
    if (useBackBuffer)
    {
        // Allocate a back buffer whose size matches the portion of
        // the drawing surface that corresponds to the size of the
        // window and the zoom factor.
        //
        // This is likely to be smaller than the entire drawing surface,
        // which is important when BitMapWidth*BitMapHeight is large.
        const wxSize & screenSize = PaintDeviceContext.GetSize();
        int backBufferWidth  = screenSize.GetWidth();
        int backBufferHeight = screenSize.GetHeight();
        if (zoom_flag)
        {
            backBufferWidth  = backBufferWidth / the_zoom + 1;
            backBufferHeight = backBufferHeight / the_zoom + 1;
        }
        backBufferWidth  = std::min(backBufferWidth,  BitMapWidth);
        backBufferHeight = std::min(backBufferHeight, BitMapHeight);

        // Add a pixel on either side of the bitmap to account for rounding
        // errors when zooming.  If we don't do this, then scrolling a zoomed
        // image can end up with white stripes being added at the edges.
        backBuffer = new wxBitmap(backBufferWidth + 2, backBufferHeight + 2);
        sourceDeviceContext = new wxMemoryDC(*backBuffer);
        sourceDeviceContext->SetDeviceOrigin(
            g_round(-ScrollX / the_zoom) + 1,
            g_round(-ScrollY / the_zoom) + 1);

        // Initialize the back buffer to white before painting to it.
        // This enables us to be a little sloppy when calcuating the
        // extents because copying a little more from the back buffer
        // to the paint context results in white being drawn just off
        // the edge of the drawing surface, which will match the
        // white that is normally drawn just past the edge of the
        // drawing surface.
        sourceDeviceContext->SetBrush(*wxWHITE_BRUSH);
        sourceDeviceContext->Clear();

        // Copy the portion of the memory image to the back buffer
        // that corresponds to the portion of the screen that is
        // being be painted.
        for (wxRegionIterator regionIterator(UpdateRegion);
             regionIterator;
             regionIterator++)
        {
            // regionIterator gives coordinates in terms of the client area.
            // We need to determine what rectangle in memoryBitmap corresponds
            // to this rectangle.
            FLONUM sourceRectLeft   = regionIterator.GetX();
            FLONUM sourceRectTop    = regionIterator.GetY();
            FLONUM sourceRectRight  = sourceRectLeft + regionIterator.GetWidth();
            FLONUM sourceRectBottom = sourceRectTop  + regionIterator.GetHeight();

            // Shift the rectangle based on the scroll position.
            sourceRectLeft   += ScrollX;
            sourceRectRight  += ScrollX;
            sourceRectTop    += ScrollY;
            sourceRectBottom += ScrollY;

            // If the screen is zoomed, then we must do some additional
            // calculation to determine what part of the source image
            // needs repainting.
            if (zoom_flag)
            {
                // Expand the source rectangle a little bit based zoom factor,
                // It doesn't hurt to copy a little more than necessary, but copying
                // less can result in jaggies.
                const FLONUM inflateIncrement = (the_zoom+0.5)*2.0;
                sourceRectLeft   -= inflateIncrement;
                sourceRectTop    -= inflateIncrement;
                sourceRectRight  += inflateIncrement;
                sourceRectBottom += inflateIncrement;

                // Second, scale based on the zoom factor.
                // We divide by the zoom factor to convert from screen coordinates
                // to turtle coordinates.  If we're zoomed in, the_zoom greater
                // than 1, so dividing by it will shrink the rectangle that
                // we're going to copy from.
                FLONUM scaledSourceRectLeft   = sourceRectLeft   / the_zoom;
                FLONUM scaledSourceRectTop    = sourceRectTop    / the_zoom;
                FLONUM scaledSourceRectRight  = sourceRectRight  / the_zoom;
                FLONUM scaledSourceRectBottom = sourceRectBottom / the_zoom;

                // Make sure that none of rectangle's borders are off-screen
                // after we inflated it.
                sourceRectLeft   = ClipToRange(0, scaledSourceRectLeft,   BitMapWidth);
                sourceRectTop    = ClipToRange(0, scaledSourceRectTop,    BitMapHeight);
                sourceRectRight  = ClipToRange(0, scaledSourceRectRight,  BitMapWidth);
                sourceRectBottom = ClipToRange(0, scaledSourceRectBottom, BitMapHeight);
            }

            sourceDeviceContext->Blit(
                sourceRectLeft,
                sourceRectTop,
                sourceRectRight - sourceRectLeft + 1,
                sourceRectBottom - sourceRectTop + 1,
                GetWxMemoryDeviceContext(), // copy from memory buffer
                sourceRectLeft,
                sourceRectTop);
        }

        // draw the turtles on top of the image
        paste_all_turtles(*sourceDeviceContext, 1.0);
    }
    else
    {
        sourceDeviceContext = GetWxMemoryDeviceContext();
    }


    if (!zoom_flag)
    {
        // The image is not zoomed.  Do a normal BLIT
        for (wxRegionIterator regionIterator(UpdateRegion);
             regionIterator;
             regionIterator++)
        {
            // The x,y coordinates are on the client area
            wxCoord x      = regionIterator.GetX();
            wxCoord y      = regionIterator.GetY();
            wxCoord width  = regionIterator.GetWidth();
            wxCoord height = regionIterator.GetHeight();

            // Fill the part to the right of the screen.
            if (BitMapWidth <= x + width)
            {
                PaintDeviceContext.SetBrush(*wxWHITE_BRUSH);
                PaintDeviceContext.SetPen(*wxWHITE_PEN);

                PaintDeviceContext.DrawRectangle(
                    BitMapWidth,              // x
                    0,                        // y
                    x + width - BitMapWidth,  // width
                    BitMapHeight);            // height
            }

            // Fill the part below the screen.
            if (BitMapHeight <= y + height)
            {
                PaintDeviceContext.SetBrush(*wxWHITE_BRUSH);
                PaintDeviceContext.SetPen(*wxWHITE_PEN);

                PaintDeviceContext.DrawRectangle(
                    x,                           // x
                    BitMapHeight,                // y
                    BitMapWidth,                 // width
                    y + height - BitMapHeight);  // height
            }

            PaintDeviceContext.Blit(
                x + ScrollX,
                y + ScrollY,
                width,
                height,
                sourceDeviceContext,
                x + ScrollX,
                y + ScrollY);
        }
    }
    else
    {
        // We are zoomed.  Compute scaling and then display.

        // Enable anti-aliasing on platforms which support it.
#ifdef __WXMSW__
        SetStretchBltMode(static_cast<HDC>(PaintDeviceContext.GetHDC()), HALFTONE);
#endif

        for (wxRegionIterator regionIterator(UpdateRegion);
             regionIterator;
             regionIterator++)
        {
            // regionIterator gives coordinates in terms of the
            // client area.
            // We need to determine what rectangle in memoryBitmap corresponds
            // to this rectangle.
            FLONUM sourceRectLeft   = regionIterator.GetX();
            FLONUM sourceRectTop    = regionIterator.GetY();
            FLONUM sourceRectRight  = sourceRectLeft + regionIterator.GetWidth();
            FLONUM sourceRectBottom = sourceRectTop  + regionIterator.GetHeight();

            // Shift the rectangle based on the scroll position.
            sourceRectLeft   += ScrollX;
            sourceRectRight  += ScrollX;
            sourceRectTop    += ScrollY;
            sourceRectBottom += ScrollY;

            // First, expand the source rectangle a little bit based zoom factor,
            // It doesn't hurt to copy a little more than necessary, but copying
            // less can result in jaggies.
            const FLONUM inflateIncrement = (the_zoom+0.5)*2.0;
            sourceRectLeft   -= inflateIncrement;
            sourceRectTop    -= inflateIncrement;
            sourceRectRight  += inflateIncrement;
            sourceRectBottom += inflateIncrement;

            // Second, scale based on the zoom factor.
            // We divide by the zoom factor to convert from screen coordinates
            // to turtle coordinates.  If we're zoomed in, the_zoom is greater
            // than 1, so dividing by it will shrink the rectangle that
            // we're going to copy from.
            FLONUM scaledSourceRectLeft   = sourceRectLeft   / the_zoom;
            FLONUM scaledSourceRectTop    = sourceRectTop    / the_zoom;
            FLONUM scaledSourceRectRight  = sourceRectRight  / the_zoom;
            FLONUM scaledSourceRectBottom = sourceRectBottom / the_zoom;

            // fill the part to the right of the drawing area.
            if (BitMapWidth < scaledSourceRectRight)
            {
                PaintDeviceContext.SetBrush(*wxWHITE_BRUSH);
                PaintDeviceContext.SetPen(*wxWHITE_PEN);

                PaintDeviceContext.DrawRectangle(
                    BitMapWidth * the_zoom,
                    0,
                    sourceRectRight,
                    BitMapHeight * the_zoom);
            }

            // fill the part below the drawing area.
            if (BitMapHeight < scaledSourceRectBottom)
            {
                PaintDeviceContext.SetBrush(*wxWHITE_BRUSH);
                PaintDeviceContext.SetPen(*wxWHITE_PEN);

                PaintDeviceContext.DrawRectangle(
                    sourceRectLeft,                              // x
                    BitMapHeight * the_zoom,                     // y
                    sourceRectRight - sourceRectLeft,            // width
                    sourceRectBottom - BitMapHeight * the_zoom); // height
            }

            // Make sure that none of rectangle's borders are off-screen
            // after we inflated it.
            wxCoord clippedSourceRectLeft   = ClipToRange(0, scaledSourceRectLeft,   BitMapWidth);
            wxCoord clippedSourceRectTop    = ClipToRange(0, scaledSourceRectTop,    BitMapHeight);
            wxCoord clippedSourceRectRight  = ClipToRange(0, scaledSourceRectRight,  BitMapWidth);
            wxCoord clippedSourceRectBottom = ClipToRange(0, scaledSourceRectBottom, BitMapHeight);

            // Now project the source rectangle back onto the screen.
            // This should be roughly the same as the original regionIterator,
            // but may be larger due to rounding and may be smaller due to
            // trucation of the off-world areas that still fit on the screen.
            wxCoord destRectLeft   = clippedSourceRectLeft   * the_zoom;
            wxCoord destRectTop    = clippedSourceRectTop    * the_zoom;
            wxCoord destRectRight  = clippedSourceRectRight  * the_zoom;
            wxCoord destRectBottom = clippedSourceRectBottom * the_zoom;

            // Copy the corresponding part of the image to the screen
            PaintDeviceContext.StretchBlit(
                destRectLeft,
                destRectTop,
                destRectRight  - destRectLeft,
                destRectBottom - destRectTop,
                sourceDeviceContext,
                clippedSourceRectLeft,
                clippedSourceRectTop,
                clippedSourceRectRight  - clippedSourceRectLeft,
                clippedSourceRectBottom - clippedSourceRectTop);
        }
    }

    if (!useBackBuffer)
    {
        // draw the turtles on top of the image
        paste_all_turtles(PaintDeviceContext, the_zoom);
    }
    else
    {
        // Delete the back buffer
        delete sourceDeviceContext;
        delete backBuffer;
    }
}
