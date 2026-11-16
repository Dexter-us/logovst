// Copyright (C) 2019 by David Costanzo
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

#include "pch.h"
#ifndef USE_PRECOMPILED_HEADER
    #include <stdlib.h>
    #include <assert.h>
    #ifdef WX_PURE
        #include <cstring> // strlen
        #include <cwchar>  // wcslen
    #else
        #include <windows.h>
    #endif
    #include "debugheap.h"
#endif

// Converts a Unicode string to an ANSI string.
// This has some information loss.
// This exists for compatibility with MSWLogo and version of FMSLogo
// which pre-date Unicode support.
//
// UnicodeString - a NUL-terminated Unicode string.
//
// Returns a buffer with a NUL-terminated ANSI string with characters
// that were representable within the character set.
// This must be free'd with free().
char *
unicode_to_ansi(
    const wchar_t * UnicodeString
    )
{
#ifdef WX_PURE
    // Copy the ASCII part of the Unicode string
    size_t stringLength = wcslen(UnicodeString);
    char * ansiString = static_cast<char*>(malloc(stringLength + 1));
    for (size_t i = 0; i < stringLength; i++)
    {
        wchar_t ch = UnicodeString[i];
        if (0 <= ch && ch <= 0x7F)
        {
            // This character falls within the ASCII-7 range.
            ansiString[i] = static_cast<char>(ch);
        }
        else
        {
            // This character falls outside the ASCII-7 range.
            // Since we don't have access to the code page,
            // assume the worst case, that it has no mapping
            // in the ANSI code page.
            ansiString[i] = '?';
        }
    }

    // NUL-terminate the string.
    ansiString[stringLength] = '\0';
#else
    // Determine the size of the buffer we need to hold the ANSI string.
    int bytesNeeded = WideCharToMultiByte(
        CP_ACP,
        0,      // no flags (convert characters as best as possible)
        UnicodeString,
        -1,     // assume NUL-terminated
        NULL,   // NULL to request output buffer size
        0,      // 0 to request output buffer size
        NULL,   // use system default replacement character
        NULL);  // don't care what the default char is

    // Allocate the buffer.
    char * ansiString = static_cast<char*>(malloc(bytesNeeded));

    // Convert from Unicode to ANSI.
    int bytesUsed = WideCharToMultiByte(
        CP_ACP,
        0,           // no flags (convert characters as best as possible)
        UnicodeString,
        -1,          // assume NUL-terminated
        ansiString,  // output buffer
        bytesNeeded, // output buffer size
        NULL,        // use system default replacement character
        NULL);       // don't care what the default char is

    assert(bytesUsed == bytesNeeded);
#endif
    return ansiString;
}

// Converts an ANSI string to a Unicode string.
//
// AnsiString - a NUL-terminated ANSI string.
//
// Returns a buffer with a NUL-terminated Unicode string with the
// same characters as in AnsiString, mapped to their Unicode code
// points.
// This must be free'd with free().
wchar_t *
ansi_to_unicode(
    const char * AnsiString
    )
{
#ifdef WX_PURE
    // Copy the ASCII part of the Unicode string
    size_t stringLength = strlen(AnsiString);
    size_t bufferLength = (stringLength + 1) * sizeof(wchar_t);
    wchar_t * unicodeString = static_cast<wchar_t*>(malloc(bufferLength));
    for (size_t i = 0; i < stringLength; i++)
    {
        char ch = AnsiString[i];
        if (0 <= ch && ch <= 0x7F)
        {
            // This character falls within the ASCII-7 range.
            unicodeString[i] = static_cast<wchar_t>(ch);
        }
        else
        {
            // This character falls outside the ASCII-7 range.
            // Since we don't have access to the code page,
            // assume the worst case, that it has no mapping
            // in the ANSI code page.
            unicodeString[i] = L'?';
        }
    }

    // NUL-terminate the string.
    unicodeString[stringLength] = L'\0';
#else
    // Determine the size of the buffer we need to hold the Unicode string.
    int charactersNeeded = MultiByteToWideChar(
        CP_ACP,
        MB_PRECOMPOSED,  // don't decompose latin characters outside of ASCII range
        AnsiString,      // string to convert
        -1,              // assume NUL-terminated
        NULL,            // destination buffer
        0);              // request number of bytes

    wchar_t * unicodeString = static_cast<wchar_t*>(malloc(sizeof(wchar_t) * charactersNeeded));

    // Convert from ANSI to Unicode.
    int charactersUsed = MultiByteToWideChar(
        CP_ACP,
        MB_PRECOMPOSED,    // don't decompose latin characters outside of ASCII range
        AnsiString,        // string to convert
        -1,                // assume NUL-terminated
        unicodeString,     // destination buffer
        charactersNeeded); // request number of bytes

    assert(charactersUsed == charactersNeeded);
#endif
    return unicodeString;
}
