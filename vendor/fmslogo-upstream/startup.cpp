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
    #ifndef WX_PURE
        #include <windows.h>

        #ifdef max // MS compilers #define max in windows.h
            #undef max
        #endif
    #endif

    #include <algorithm>

    #include <wx/defs.h>
    #include <wx/string.h>
    #include <wx/filename.h>

    #include "startup.h"
    #include "activearea.h"
    #include "graphwin.h"
    #include "utils.h"
    #include "fmslogo.h"
    #include "debugheap.h"
#endif

Color dfld;                        // Current flood color
Color dscn;                        // Current screen color

wxString * g_LibPathName;             // path to library
wxString * g_HelpFileName;            // path to help file
wchar_t TempPathName[MAX_PATH + 1];   // path to temp edit file
wchar_t TempBmpName[MAX_PATH + 1];    // path to temp bitmap file
wchar_t TempClipName[MAX_PATH + 1];   // path to temp clipboard file

wxUint32 scolor;                   // screen color
wxUint32 fcolor;                   // flood color
wxUint32 pcolor;                   // pen color

wchar_t g_FmslogoBaseDirectory[MAX_PATH+1]; // The directory that contains fmslogo.exe

// Creates path relative to the directory in which FMSLogo is installed.
void MakeHelpPathName(wchar_t *OutBuffer, const wchar_t * TheFileName)
{
    wcsncpy(OutBuffer, g_FmslogoBaseDirectory, MAX_PATH);
    wcsncat(OutBuffer, TheFileName,            MAX_PATH - wcslen(OutBuffer));
}

// Creates a temp filename
static
void MakeTempFilename(wchar_t *OutBuffer, const wchar_t * Prefix)
{
    const wxString & tempFile = wxFileName::CreateTempFileName(Prefix);
    wxRemove(tempFile);
    wcscpy(OutBuffer, tempFile.wc_str());
}

void init_graphics()
{
    // set appropriate default colors
    pcolor = 0x00000000;
    scolor = 0x00FFFFFF;
    fcolor = 0x00000000;

    dfld.red   = 0x00;
    dfld.green = 0x00;
    dfld.blue  = 0x00;

    dscn.red   = 0xFF;
    dscn.green = 0xFF;
    dscn.blue  = 0xFF;

#ifndef WX_PURE
    // init the font structure
    FontRec.lfHeight         = 24;
    FontRec.lfWidth          = 0;
    FontRec.lfOrientation    = 0;
    FontRec.lfWeight         = 400;
    FontRec.lfItalic         = 0;
    FontRec.lfUnderline      = 0;
    FontRec.lfStrikeOut      = 0;
    FontRec.lfCharSet        = DEFAULT_CHARSET; // system default
    FontRec.lfOutPrecision   = OUT_DEFAULT_PRECIS;
    FontRec.lfClipPrecision  = CLIP_DEFAULT_PRECIS;
    FontRec.lfQuality        = DEFAULT_QUALITY;
    FontRec.lfPitchAndFamily = DEFAULT_PITCH;
    wcscpy(FontRec.lfFaceName, L"Arial");
#endif

    // Set handy rectangle of full bitmap
    g_FullRect.SetWidth(BitMapWidth);
    g_FullRect.SetHeight(BitMapHeight);

    // turtle coords are in center
    xoffset = BitMapWidth  / 2;
    yoffset = BitMapHeight / 2;

    // Init active area even if off
    g_PrinterAreaXLow  = -BitMapWidth  / 2;
    g_PrinterAreaXHigh = +BitMapWidth  / 2;
    g_PrinterAreaYLow  = -BitMapHeight / 2;
    g_PrinterAreaYHigh = +BitMapHeight / 2;

    g_PrinterAreaPixels = std::max(BitMapWidth, BitMapHeight) / 8;

    // init paths to library and help files based on location of .EXE
    const wxString & fmslogoDirectory = g_FmslogoBaseDirectory;

    const wxFileName libPathName(fmslogoDirectory + L"logolib/");
    g_LibPathName  = new wxString(libPathName.GetPathWithSep());

    const wxFileName helpFileName(fmslogoDirectory + L"logohelp.chm");
    g_HelpFileName = new wxString(helpFileName.GetFullPath());

    // construct the name of the temporary editor file
    MakeTempFilename(TempPathName, L"fmslogo-editor-");

    // construct the name of the temporary bitmap file
    MakeTempFilename(TempBmpName, L"fmslogo-bitmap-");

    // construct the name of the clipboard file
    MakeTempFilename(TempClipName, L"fmslogo-clipboard-");

    g_PrinterAreaXLow   = GetConfigurationInt(L"Printer.Xlow",  -BitMapWidth  / 2);
    g_PrinterAreaXHigh  = GetConfigurationInt(L"Printer.XHigh", +BitMapWidth  / 2);
    g_PrinterAreaYLow   = GetConfigurationInt(L"Printer.Ylow",  -BitMapHeight / 2);
    g_PrinterAreaYHigh  = GetConfigurationInt(L"Printer.YHigh", +BitMapHeight / 2);
    g_PrinterAreaPixels = GetConfigurationInt(L"Printer.Pixels", std::max(BitMapWidth, BitMapHeight) / 8);
}

void uninit_graphics()
{
    delete g_LibPathName;
    g_LibPathName = NULL;

    delete g_HelpFileName;
    g_HelpFileName = NULL;

    wxRemove(TempPathName);
    wxRemove(TempBmpName);
    wxRemove(TempClipName);
}
