// -*- c++ -*-

#include <wx/dialog.h>

class wxString;
class wxTextCtrl;
class wxButton;

class CPauseDialog : public wxDialog
{
public:
    CPauseDialog(wxWindow *Parent);
    const wxString GetInstruction() const;

private:
    // Event handlers
    void OnClose(wxCloseEvent& Event);
    void OnInstructionChange(wxCommandEvent& Event);
    void OnContinue(wxCommandEvent& Event);
    void OnExecute(wxCommandEvent& Event);
    void OnHalt(wxCommandEvent& Event);

    wxTextCtrl * m_InstructionInput;
    wxString     m_Instruction;
    wxButton   * m_ExecuteButton;

    // Helper functions
    wxButton *
    AddButtonRow(
        wxSizer       * GridSizer,
        const wchar_t * ExplainText,
        const wchar_t * ButtonText,
        wxWindowID      ButtonId
        );

    void UseButtonTextAsInstruction(const wchar_t * ButtonText);

    DECLARE_NO_COPY_CLASS(CPauseDialog);
    DECLARE_EVENT_TABLE();
};
