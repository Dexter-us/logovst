#include "pch.h"
#ifndef USE_PRECOMPILED_HEADER
   #include "centerablelink.h"

   #include <wx/hyperlink.h>
#endif

// On windows, CHyperlinkCtrl does not honor wxHL_ALIGN_CENTRE.
// This is a helper class that can center the link.
CCenterableLink::CCenterableLink(
    wxWindow       * Parent,
    const wxString & Text,
    const wxString & Url
    ) :
    wxWindow(Parent, wxID_ANY),
    m_Link(NULL)
{
    m_Link = new wxHyperlinkCtrl(this, wxID_ANY, Text, Url);

    // Set the size of the link to match the size of the text.
    // This helps to give a consistent layout across platforms.
    const wxSize & linkTextSize = m_Link->GetTextExtent(m_Link->GetLabel());
    m_Link->SetSize(linkTextSize);
}

void CCenterableLink::CenterLink()
{
    // Determine the amount of free space on either side of the link text.
    const wxSize & linkTextSize = m_Link->GetTextExtent(m_Link->GetLabel());
    int freeSpace = GetSize().GetWidth() - linkTextSize.GetWidth();

    // Move the link to have an equal amount of free space on either side.
    // This centers it horizontally.
    m_Link->Move(freeSpace / 2, m_Link->GetPosition().y);
}
