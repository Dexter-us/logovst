#include "pch.h"
#ifndef USE_PRECOMPILED_HEADER
   #include "minieditortextctrl.h"

   #include <wx/event.h>

   #include "guiutils.h"
   #include "logocore.h" // for ARRAYSIZE
   #include "helputils.h" // for ContextHelp()
#endif

// Menu IDs
enum
{
    ID_FINDMATCHINGPAREN = wxID_HIGHEST + 1,
    ID_SELECTMATCHINGPAREN,
    ID_AUTOCOMPLETE,
};

// ----------------------------------------------------------------------------
// CMiniEditorTextCtrl
// ----------------------------------------------------------------------------

CMiniEditorTextCtrl::CMiniEditorTextCtrl(
    wxWindow*      Parent,
    wxWindowID     Id
    ) : CLogoCodeCtrl(Parent, Id)
{
    // Configure the keyboard shortcuts
    static const ACCELERATORITEM keyboardShortcuts[] = {
        {wxACCEL_CTRL, 'A', wxID_SELECTALL},
        {wxACCEL_CTRL, 'Z', wxID_UNDO     },
        {wxACCEL_CTRL, 'Y', wxID_REDO     },
        {wxACCEL_CTRL, 'X', wxID_CUT      },
        {wxACCEL_CTRL, 'C', wxID_COPY     },
        {wxACCEL_CTRL, 'V', wxID_PASTE    },

        // Ctrl+] moves to matching paren
        {wxACCEL_CTRL,                 ']',       ID_FINDMATCHINGPAREN},

        // Ctrl+Shift+] selects to matching paren
        {wxACCEL_CTRL | wxACCEL_SHIFT, ']',       ID_SELECTMATCHINGPAREN},

        // Ctrl+Space starts auto-complete
        {wxACCEL_CTRL,                 WXK_SPACE, ID_AUTOCOMPLETE},
    };
    SetAccelerators(this, keyboardShortcuts, ARRAYSIZE(keyboardShortcuts));
}

CMiniEditorTextCtrl::~CMiniEditorTextCtrl()
{
}

void CMiniEditorTextCtrl::OnKeyDown(wxKeyEvent& Event)
{
    int keyCode = Event.GetKeyCode();

    switch (keyCode)
    {
    case WXK_F1:
        // F1 displays the help
        ContextHelp(GetStringSelection());
        break;

    case WXK_TAB:
        // Use TAB and SHIFT+TAB as a navigational key events
        if (Event.ShiftDown())
        {
            // Shift+Tab means navigate backward.
            Navigate(wxNavigationKeyEvent::IsBackward);
        }
        else
        {
            // Tab means navigate forward.
            Navigate(wxNavigationKeyEvent::IsForward);
        }
        break;

    default:
        Event.Skip();
        break;
    }
}

void CMiniEditorTextCtrl::OnFindMatchingParen(wxCommandEvent& WXUNUSED(Event))
{
    FindMatchingParen();
}

void CMiniEditorTextCtrl::OnSelectMatchingParen(wxCommandEvent& WXUNUSED(Event))
{
    SelectMatchingParen();
}

void CMiniEditorTextCtrl::OnAutoComplete(wxCommandEvent& WXUNUSED(Event))
{
    AutoComplete();
}

BEGIN_EVENT_TABLE(CMiniEditorTextCtrl, CLogoCodeCtrl)
    EVT_KEY_DOWN(CMiniEditorTextCtrl::OnKeyDown)
    EVT_MENU(ID_FINDMATCHINGPAREN,   CMiniEditorTextCtrl::OnFindMatchingParen)
    EVT_MENU(ID_SELECTMATCHINGPAREN, CMiniEditorTextCtrl::OnSelectMatchingParen)
    EVT_MENU(ID_AUTOCOMPLETE,        CMiniEditorTextCtrl::OnAutoComplete)
END_EVENT_TABLE()
