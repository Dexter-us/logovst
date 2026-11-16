#include "pch.h"
#ifndef USE_PRECOMPILED_HEADER
    #include "pausedialog.h"

    #include <wx/sizer.h>
    #include <wx/stattext.h>
    #include <wx/button.h>
    #include <wx/textctrl.h>

    #include "logocodectrl.h"
    #include "internationalizedstrings.h"
#endif

// ----------------------------------------------------------------------------
// CPauseDialog
// ----------------------------------------------------------------------------

// Menu IDs
enum
{
   ID_INSTRUCTION = wxID_HIGHEST,
   ID_EXECUTE,
   ID_HALT,
};


wxButton *
CPauseDialog::AddButtonRow(
    wxSizer       * GridSizer,
    const wchar_t * ExplainText,
    const wchar_t * ButtonText,
    wxWindowID      ButtonId)
{
    // Add the explain text.
    wxStaticText * explainText = new wxStaticText(this, wxID_ANY, ExplainText);
    GridSizer->Add(
        explainText,
        0,
        wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL | wxALL,
        5);

    // Add the button.
    wxButton * button = new wxButton(this, ButtonId, ButtonText);
    GridSizer->Add(
        button,
        0,
        wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL,
        5);

    return button;
}

CPauseDialog::CPauseDialog(wxWindow *Parent)
    : wxDialog(
        Parent,
        wxID_ANY,
        g_LOCALIZED_PAUSE_MODE_TITLE,
        wxDefaultPosition,
        wxDefaultSize,
        wxCAPTION | wxCLOSE_BOX)
{
    wxBoxSizer *topLevelSizer = new wxBoxSizer(wxVERTICAL);

    // Add the text that explains the button choices.
    wxStaticText * message = new wxStaticText(
        this,
        wxID_ANY,
        g_LOCALIZED_PAUSE_MODE_EXPLAIN_TEXT);
    message->Wrap(400);
    topLevelSizer->Add(message, 0, wxALIGN_LEFT | wxALL, 5);

    // Add the row with an instruction input and an "Execute" button.
    wxSizer * gridSizer = new wxFlexGridSizer(2, wxSize(5, 5));
    topLevelSizer->Add(gridSizer, 1, wxEXPAND);

    m_InstructionInput = new wxTextCtrl(
        this,
        static_cast<wxWindowID>(ID_INSTRUCTION));
    gridSizer->Add(
        m_InstructionInput,
        0,
        wxEXPAND | wxALIGN_CENTER_VERTICAL | wxALL,
        5);

    // Give enough horizontal space for a reasonable instruction.
    int instructionInputHeight = m_InstructionInput->GetSize().GetHeight();
    m_InstructionInput->SetMinSize(wxSize(300, instructionInputHeight));
 
    m_ExecuteButton = new wxButton(
        this,
        ID_EXECUTE,
        g_LOCALIZED_PAUSE_MODE_EXECUTE_BUTTON);
    gridSizer->Add(
        m_ExecuteButton,
        0,
        wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL,
        5);

    // Add a row with a "Continue" button.
    // This is mapped to wxID_CANCEL because this is how to
    // dismiss the dialog box and exit pause mode.
    wxButton * continueButton = AddButtonRow(
        gridSizer,
        g_LOCALIZED_PAUSE_MODE_CONTINUE_EXPLAIN_TEXT,
        g_LOCALIZED_PAUSE_MODE_CONTINUE_BUTTON,
        wxID_CANCEL);

    // Add a row with a "Halt" button.
    wxButton * haltButton = AddButtonRow(
        gridSizer,
        g_LOCALIZED_PAUSE_MODE_HALT_EXPLAIN_TEXT,
        g_LOCALIZED_PAUSE_MODE_HALT_BUTTON,
        ID_HALT);

    // Make all the buttons have the same size.
    // This looks nicer.
    int maxWidth = std::max(
        m_ExecuteButton->GetSize().GetWidth(),
        std::max(
            continueButton->GetSize().GetWidth(),
            haltButton->GetSize().GetWidth()));
    wxSize buttonSize(m_ExecuteButton->GetSize());
    buttonSize.SetWidth(maxWidth);
    m_ExecuteButton->SetMinClientSize(buttonSize);
    continueButton->SetMinClientSize(buttonSize);
    haltButton->SetMinClientSize(buttonSize);
 
    SetSizerAndFit(topLevelSizer);

    // Give the instruction input focus.
    // This should be the next thing the user wants to do,
    // otherwise they wouldn't have paused.
    m_ExecuteButton->Enable(false);
    m_ExecuteButton->SetDefault();
    m_InstructionInput->SetFocus();
}

void CPauseDialog::UseButtonTextAsInstruction(const wchar_t * ButtonText)
{
    // Set the text as the instruction to run.
    m_Instruction.assign(ButtonText);

    // ButtonText can have an "&" to indicate a keyboard shortcut.
    // This must be removed before executing it.
    m_Instruction.Replace(L"&", L"");
}

void CPauseDialog::OnClose(wxCloseEvent& WXUNUSED(Event))
{
    // This is called when the dialog's 'X' button is pressed.
    // Treat it the same as continue.
    UseButtonTextAsInstruction(g_LOCALIZED_PAUSE_MODE_CONTINUE_BUTTON);
    Destroy();
}

void CPauseDialog::OnInstructionChange(wxCommandEvent& WXUNUSED(Event))
{
    // Enable the Execute button iff there is an instruction to execute.
    m_ExecuteButton->Enable(!m_InstructionInput->GetValue().empty());
}

void CPauseDialog::OnExecute(wxCommandEvent& WXUNUSED(Event))
{
    m_Instruction.assign(m_InstructionInput->GetValue());
    Destroy();
}

void CPauseDialog::OnContinue(wxCommandEvent& WXUNUSED(Event))
{
    UseButtonTextAsInstruction(g_LOCALIZED_PAUSE_MODE_CONTINUE_BUTTON);
    Destroy();
}

void CPauseDialog::OnHalt(wxCommandEvent& WXUNUSED(Event))
{
    UseButtonTextAsInstruction(g_LOCALIZED_PAUSE_MODE_HALT_BUTTON);
    Destroy();
}

const wxString CPauseDialog::GetInstruction() const
{
    return m_Instruction;
}

BEGIN_EVENT_TABLE(CPauseDialog, wxDialog)
    EVT_CLOSE(CPauseDialog::OnClose)
    EVT_TEXT(ID_INSTRUCTION, CPauseDialog::OnInstructionChange)
    EVT_BUTTON(ID_EXECUTE,   CPauseDialog::OnExecute)
    EVT_BUTTON(wxID_CANCEL,  CPauseDialog::OnContinue)
    EVT_BUTTON(ID_HALT,      CPauseDialog::OnHalt)
END_EVENT_TABLE()
