// -*- c++ -*-
#ifndef __CENTERABLELINK_H_
#define __CENTERABLELINK_H_

#include <wx/window.h>

class wxString;

// On windows, CHyperlinkCtrl does not honor wxHL_ALIGN_CENTRE.
// This is a helper class that can center the link.
class CCenterableLink : public wxWindow
{
public:
    CCenterableLink(
        wxWindow       * parent,
        const wxString & text,
        const wxString & url
        );

    void CenterLink();

private:
    wxWindow * m_Link;
};

#endif // __CENTERABLELINK_H_
