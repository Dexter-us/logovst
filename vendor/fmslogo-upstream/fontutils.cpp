#include "pch.h"
#ifndef USE_PRECOMPILED_HEADER
   #ifdef __WXMSW__
   #  include <windows.h>  // for LOGFONT
   #endif

   #include <wx/fontutil.h>

   #include "fontutils.h"
   #include "utils.h"
   #include "graphwin.h"
#endif

void
GetConfigurationFont(
    const wchar_t * ConfigName,
    wxFont &        Font
    )
{
#ifdef __WXMSW__
    // Get the font
    LOGFONT nativeFont;
    GetConfigurationFont(ConfigName, nativeFont);

    // Convert it to a wxFont
    Font = wxFont(nativeFont);
#else
    // TODO: figure out what to do on GNU/Linux
#endif
}


// Set this as the new default font
void
SetConfigurationFont(
    const wchar_t * ConfigName,
    const wxFont  & Font
    )
{
#ifdef __WXMSW__

    // Get the LOGFONT struct from the wxFont
    const wxNativeFontInfo * nativeFontInfo = Font.GetNativeFontInfo();
    if (nativeFontInfo != NULL)
    {
        // save the new font preference to persistent storage
        SetConfigurationFont(ConfigName, nativeFontInfo->lf);
    }
#else
    // TODO: figure out what to do on GNU/Linux
#endif
}

const wxFont
GetLabelFont()
{
#ifdef __WXMSW__
    return wxFont(FontRec);
#else
    return wxFont(g_LabelFontInfo);
#endif
}
