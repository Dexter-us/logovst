/*
 *  Copyright (C) 1995 by the Regents of the University of California
 *  Copyright (C) 1995 by George Mills
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include "pch.h"
#ifndef USE_PRECOMPILED_HEADER
   #include <wx/msgdlg.h>

   #include "messagebox.h"
   #include "screenwindow.h"
   #include "error.h"
   #include "debugheap.h"

   #include "internationalizedstrings.h"
#endif

void
ShowMessage(
    const wchar_t * Title,
    const wchar_t * Message
    )
{
    wxMessageBox(
        Message,
        Title,
        wxOK | wxCENTER,
        GetParentWindowForDialog());
}

void
ShowErrorMessage(
    const wchar_t * Message
    )
{
    ShowMessage(g_LOCALIZED_ERROR, Message);
}

// Maps ErrorCode to a string and displays it in a message box
void
ShowErrorMessage(
    ERR_TYPES  ErrorCode
    )
{
    const wchar_t * errorMessage;

    switch (ErrorCode)
    {
    case IMAGE_GENERAL:
        // This happens when one of the Win32 API fails when trying to
        // save a bitmap.  It would be nice to show an error that
        // indicates the problem, but since I don't expect this to happen
        // showing a generic "unknown error happened" is sufficient.
        errorMessage = g_LOCALIZED_ERROR_UNKNOWN;
        break;

    case IMAGE_GIF_LOAD_FAILED:
        errorMessage = g_LOCALIZED_ERROR_GIFREADFAILED;
        break;

    case IMAGE_GIF_SAVE_FAILED:
        errorMessage = g_LOCALIZED_ERROR_GIFSAVEFAILED;
        break;

    case IMAGE_BMP_OPEN_FAILED:
        errorMessage = g_LOCALIZED_COULDNOTOPENBMP;
        break;

    case IMAGE_BMP_CREATE_FAILED:
        errorMessage = g_LOCALIZED_COULDNOTCREATEBMP;
        break;

    case IMAGE_BMP_WRITE_FAILED:
        errorMessage = g_LOCALIZED_COULDNOTWRITEBMP;
        break;

    case IMAGE_BMP_INVALID:
        errorMessage = g_LOCALIZED_NOTVALIDBMP;
        break;

    case OUT_OF_MEM:
        errorMessage = g_LOCALIZED_ERROR_OUTOFMEMORY;
        break;

    default:
        // This should never happen, but if it does, showing a general error
        // message is better than showing no error message.
        assert(0);
        errorMessage = g_LOCALIZED_ERROR_UNKNOWN;
        break;
    }

    ShowErrorMessage(errorMessage);
}
