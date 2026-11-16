#include "pch.h"
#ifndef USE_PRECOMPILED_HEADER
   #include "aboutfmslogo.h"

   #include <wx/sizer.h>
   #include <wx/stattext.h>
   #include <wx/button.h>

   #include "centerablelink.h"
   #include "version.h"
   #include "internationalizedstrings.h"
   #include "logocore.h" // for ARRAYSIZE
#endif

// Some URLs which should not be localized.
static const wchar_t g_FMSLOGO_URL[]   = L"https://sourceforge.net/projects/fmslogo";
static const wchar_t g_UCBLOGO_URL[]   = L"https://people.eecs.berkeley.edu/~bh/logo.html";
static const wchar_t g_SOFTRONIX_URL[] = L"https://www.softronix.com";
static const wchar_t g_NSIS_URL[]      = L"https://nsis.sourceforge.io";

// A sunken panel in the About box.
class CAboutPanel : public wxWindow {
public:
    CAboutPanel(
        wxWindow       * Parent,
        wxSizer        * ParentSizer,
        const wchar_t  * HeaderText,
        const wchar_t  * Url
        ) :
        wxWindow(
            Parent,
            wxID_ANY,
            wxDefaultPosition,
            wxDefaultSize,
            wxBORDER_SUNKEN),
        m_Link(NULL)

    {
        wxStaticText * headerText = new wxStaticText(
            this,
            wxID_ANY,
            HeaderText,
            wxDefaultPosition,
            wxDefaultSize,
            wxALIGN_CENTRE);

        if (Url != NULL)
        {
            m_Link = new CCenterableLink(this,  Url, Url);
        }

        // Layout the text above the URL.
        wxBoxSizer *panelSizer = new wxBoxSizer(wxVERTICAL);
        panelSizer->Add(headerText, 0, wxLEFT | wxRIGHT | wxEXPAND, 5);
        if (m_Link != NULL)
        {
            panelSizer->Add(m_Link, 0, wxLEFT | wxRIGHT | wxEXPAND, 5);
        }
        SetSizer(panelSizer);

        ParentSizer->Add(this, 0, wxALL | wxEXPAND, 5);
    }

    void CenterLink()
    {
        if (m_Link != NULL)
        {
            m_Link->CenterLink();
        }
    }

private:
    CCenterableLink * m_Link;
};


// ----------------------------------------------------------------------------
// CAboutFmsLogo
// ----------------------------------------------------------------------------

CAboutFmsLogo::CAboutFmsLogo(wxWindow * Parent)
    : wxDialog(Parent, wxID_ANY, g_LOCALIZED_ABOUTFMS)
{
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    // The FMSLogo version number
    CAboutPanel * fmsLogoPanel = new CAboutPanel(
        this,
        sizer,
        wxString::Format(
            L"%ls %ls %ls",
            g_LOCALIZED_GENERAL_PRODUCTNAME,
            g_LOCALIZED_ABOUTFMS_VERSION,
            FMSLOGO_VERSION),
        g_FMSLOGO_URL);

    // GUI by George Mills
    CAboutPanel * guiPanel = new CAboutPanel(
        this,
        sizer,
        g_LOCALIZED_ABOUTFMS_GUI,
        g_SOFTRONIX_URL);

    // Core by Brian Harvey
    CAboutPanel * ucbLogoPanel = new CAboutPanel(
        this,
        sizer,
        g_LOCALIZED_ABOUTFMS_CORE,
        g_UCBLOGO_URL);

    // Installer by NSIS.
    CAboutPanel * installerPanel = new CAboutPanel(
        this,
        sizer,
        g_LOCALIZED_ABOUTFMS_INSTALLER,
        g_NSIS_URL);

    // The Great Logo Adventure
    CAboutPanel * logoAdventurePanel = new CAboutPanel(
        this,
        sizer,
        g_LOCALIZED_ABOUTFMS_ADVENTURE,
        g_LOCALIZED_ABOUTFMS_ADVENTURE_URL);

    // Special Thanks
    new CAboutPanel(this, sizer, g_LOCALIZED_ABOUTFMS_SPECIALTHANKS, NULL);

    // GPL
    new CAboutPanel(this, sizer, g_LOCALIZED_ABOUTFMS_GPL, NULL);

    // Logo forum
    CAboutPanel * forumPanel = new CAboutPanel(
        this,
        sizer,
        g_LOCALIZED_ABOUTFMS_PARTICIPATE,
        g_LOCALIZED_ABOUTFMS_PARTICIPATE_URL);

    // Multimedia Logic + Translators
    new CAboutPanel(this, sizer, g_LOCALIZED_ABOUTFMS_MULTIMEDIALOGIC, NULL);

    wxButton *okButton = new wxButton(
        this,
        wxID_CANCEL,
        g_LOCALIZED_ABOUTFMS_OK);
    sizer->Add(okButton, 0, wxALIGN_CENTER | wxALL, 5);

    SetSizerAndFit(sizer);

    // On windows, the link doesn't center itself automatically so
    // we manually center it.  We must do that after the top-level
    // sizer has fit the contents (and rearranged them).
    fmsLogoPanel->CenterLink();
    guiPanel->CenterLink();
    ucbLogoPanel->CenterLink();
    installerPanel->CenterLink();
    logoAdventurePanel->CenterLink();
    forumPanel->CenterLink();

    // make the "Ok" button the default
    okButton->SetFocus();
    okButton->SetDefault();
}
