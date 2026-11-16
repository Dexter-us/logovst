// -*- c++ -*-
#include "pch.h"
#ifndef USE_PRECOMPILED_HEADER

  #include "aboutmultiplesclerosis.h"

  #include <wx/sizer.h>
  #include <wx/stattext.h>
  #include <wx/button.h>

  #include "centerablelink.h"
  #include "internationalizedstrings.h"
#endif

// ----------------------------------------------------------------------------
// CAboutMultipleSclerosis
// ----------------------------------------------------------------------------

CAboutMultipleSclerosis::CAboutMultipleSclerosis(wxWindow * Parent)
    : wxDialog(Parent, wxID_ANY, g_LOCALIZED_ABOUTMS)
{
    wxStaticText * header = new wxStaticText(
        this,
        wxID_ANY,
        g_LOCALIZED_ABOUTMS_FMS,
        wxDefaultPosition,
        wxDefaultSize,
        wxALIGN_CENTRE);

    wxStaticText * donate = new wxStaticText(
        this,
        wxID_ANY,
        g_LOCALIZED_ABOUTMS_CONSIDERDONATING,
        wxDefaultPosition,
        wxDefaultSize,
        wxALIGN_CENTRE);

    wxStaticText * phone = new wxStaticText(
        this,
        wxID_ANY,
        g_LOCALIZED_ABOUTMS_PHONE,
        wxDefaultPosition,
        wxDefaultSize,
        wxALIGN_CENTRE);

    CCenterableLink * link = new CCenterableLink(
        this,
        g_LOCALIZED_ABOUTMS_URL,
        g_LOCALIZED_ABOUTMS_URL);

    wxButton *okButton = new wxButton(
        this,
        wxID_CANCEL,
        g_LOCALIZED_ABOUTMS_OK);

    wxBoxSizer *sizerTop = new wxBoxSizer(wxVERTICAL);
    sizerTop->Add(header,   0, wxALL | wxEXPAND, 10);
    sizerTop->Add(donate,   0, wxLEFT | wxRIGHT | wxEXPAND, 10);
    sizerTop->Add(link,     0, wxLEFT | wxRIGHT | wxEXPAND, 10);
    sizerTop->Add(phone,    0, wxLEFT | wxRIGHT | wxEXPAND, 10);
    sizerTop->Add(okButton, 0, wxALIGN_CENTER | wxALL, 10);

    SetSizer(sizerTop);

    sizerTop->SetSizeHints(this);
    sizerTop->Fit(this);

    // On windows, the link doesn't center itself automatically so
    // we manually center it.  We must do that after the top-level
    // sizer has fit the contents (and rearranged them).
    link->CenterLink();

    // make the "Ok" button the default
    okButton->SetFocus();
    okButton->SetDefault();
}
