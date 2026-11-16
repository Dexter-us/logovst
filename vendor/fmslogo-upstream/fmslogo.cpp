#include "pch.h"
#ifndef USE_PRECOMPILED_HEADER
   #include "fmslogo.h"

   #include <algorithm>
   #include <filesystem>
   #include <iostream>
   #include <stdexcept>
   #include "logo_midi/music_engine.hpp"

   #include <wx/msgdlg.h>
   #include <wx/filename.h>
   #include <wx/string.h>
   #include <wx/dcclient.h>
   #include <wx/dcmemory.h>
   #include <wx/clipbrd.h>
   #include <wx/gdicmn.h>
   #include <wx/settings.h>
   #include <wx/stdpaths.h>
   #include <wx/snglinst.h>

   #ifdef __WXMSW__
      #include "wx/msw/private.h"
   #endif

   #include "init.h"
   #include "internationalizedstrings.h"
   #include "logocore.h" // for ARRAYSIZE
   #include "mainframe.h"
   #include "graphwin.h"
   #include "error.h"
   #include "wrksp.h"
   #include "startup.h"
   #include "utils.h"
   #include "timer.h"
   #include "files.h"
   #include "mainwind.h"
   #include "cursor.h"
   #include "logoeventqueue.h"
   #include "eval.h"
   #include "print.h"
   #include "screenwindow.h"
   #include "workspaceeditor.h"
   #include "minieditor.h"
   #include "dynamicbuffer.h"
   #include "stringprintednode.h"
   #include "screen.h"
   #include "commander.h"
#endif

// global variables declared in fmslogo.h
int  BitMapWidth  = 1000;
int  BitMapHeight = 1000;
bool bFixed       = false;

static wxSingleInstanceChecker * g_SingleInstanceChecker = NULL;

#ifdef WX_PURE
#define MAX_PATH (260)
#endif
static wchar_t g_FileToLoad[MAX_PATH] = L""; // routine to exec on start
static wchar_t g_MidiOutputFile[MAX_PATH] = L"";
static bool g_MidiCliFailed = false;
static logo_midi::MusicEngine g_MidiEngine;
static bool g_EnterPerspectiveMode = false;
static bool g_CustomWidth          = false;
static bool g_CustomHeight         = false;

bool IsMidiRenderMode()
{
    return g_MidiOutputFile[0] != '\0';
}

void SetMidiRenderFailure()
{
    g_MidiCliFailed = true;
}

#ifdef MEM_DEBUG
#ifdef __WXMSW__
typedef DWORD (WINAPI *GETGUIRESOURCES)(HANDLE, DWORD);

static GETGUIRESOURCES g_GetGuiResources     = NULL;
static DWORD           g_OriginalGuiObjects  = 0;
static DWORD           g_OriginalUserObjects = 0;
static HANDLE          g_Fmslogo             = NULL;
static HMODULE         g_User32              = NULL;
#endif // __WXMSW__
#endif // MEM_DEBUG


////////////////////////////////////////////////////////////////////
// CFmsLogo
IMPLEMENT_APP(CFmsLogo)

CFmsLogo::CFmsLogo()
{
}

// Read an integer command-line argument, given as either "-W500" or "-W 500".
//
// CurrentArgument - A pointer in a string just after the switch,
//                   to where the number should begin.
//
// NextArgument    - A reference to an pointer in varg.
//                   This is incremented to the next argument if
//                   the switch is given in the form "-W 500".
static
int
ReadIntArgument(
    wchar_t *    CurrentArgument,
    wchar_t ** & NextArgument
    )
{
    int numericValue;

    if (*CurrentArgument != '\0')
    {
        // The command-line was given as "-w500".
        numericValue = wcstoul(CurrentArgument, &CurrentArgument, 10);
    }
    else
    {
        // The -w isn't immediately followed by a number.
        // Try to get the next argument, as in "-w 500".
        wchar_t * nextArgument = NextArgument[1];
        if (nextArgument != NULL)
        {
            // There was an argument following the -W.
            numericValue = wcstoul(nextArgument, NULL, 10);
            NextArgument++;
        }
        else
        {
            // The command-line ends in something like "-w".
            // In MSWLogo, the width would have silently been
            // taken to be 0.

            // Since this results in an unusable workspace, we
            // now warn user of their mistake.
            wxMessageBox(
                *NextArgument,
                g_LOCALIZED_ERROR_BADCOMMANDLINE,
                wxOK | wxICON_INFORMATION);

            numericValue = 0;
        }
    }

    return numericValue;
}

void CFmsLogo::ProcessCommandLine()
{
    // parse the command-line parameters
    g_FileToLoad[0] = '\0';
    bExpert         = false;
    g_CustomWidth   = false;
    g_CustomHeight  = false;
    bFixed          = false;

    // For processing the -L parameter
    bool   copyRemaingArgsAsFilename = false;
    size_t fileToLoadIndex           = 0;

    for (wchar_t ** nextArgument = static_cast<wchar_t**>(argv) + 1;
         *nextArgument != NULL;
         nextArgument++)
    {
        wchar_t * argument = *nextArgument;

        if (!copyRemaingArgsAsFilename)
        {
            if (*argument == '-')
            {
                argument++; // advance beyond the -

                switch (*argument++)
                {
                case 'p':
                case 'P':
                    g_EnterPerspectiveMode = true;
                    break;

                case 'e':
                case 'E':
                    bExpert = true;
                    break;

                case 'f':
                case 'F':
                    bFixed = true;
                    break;

                case 'h':
                case 'H':
                    BitMapHeight   = ReadIntArgument(argument, nextArgument);
                    g_CustomHeight = true;
                    break;

                case 'w':
                case 'W':
                    BitMapWidth   = ReadIntArgument(argument, nextArgument);
                    g_CustomWidth = true;
                    break;

                case 'l':
                case 'L':
                    // the rest of the arguments should be taken as part of a filename
                    copyRemaingArgsAsFilename = true;
                    break;

                case 'm':
                case 'M':
                    if (*argument != '\0')
                    {
                        wcsncpy(g_MidiOutputFile, argument,
                                ARRAYSIZE(g_MidiOutputFile) - 1);
                    }
                    else if (nextArgument[1] != NULL &&
                             nextArgument[1][0] != '-')
                    {
                        nextArgument++;
                        wcsncpy(g_MidiOutputFile, nextArgument[0],
                                ARRAYSIZE(g_MidiOutputFile) - 1);
                    }
                    else
                    {
                        std::wcerr << L"Missing MIDI output file after -M.\n";
                        g_MidiCliFailed = true;
                    }
                    g_MidiOutputFile[ARRAYSIZE(g_MidiOutputFile) - 1] = '\0';
                    break;

                default:
                    // invalid command line: unrecognized switch
                    wxMessageBox(
                        argument - 2,
                        g_LOCALIZED_ERROR_BADCOMMANDLINE,
                        wxOK | wxICON_INFORMATION);
                    break;
                }
            }
            else
            {
                // This was not a '-', so treat it as part of a filename
                copyRemaingArgsAsFilename = true;
            }
        }

        if (copyRemaingArgsAsFilename)
        {
            // Copy the rest of the line into the g_FileToLoad buffer

            // BUG: This is not backward compatible with MSWLogo.
            // This should tolerate multiple spaces, instead of assuming
            // that there's exactly one space between arguments.
            if (fileToLoadIndex != 0 &&
                fileToLoadIndex < ARRAYSIZE(g_FileToLoad))
            {
                g_FileToLoad[fileToLoadIndex++] = ' ';
            }

            wcsncpy(
                &g_FileToLoad[fileToLoadIndex],
                argument,
                ARRAYSIZE(g_FileToLoad) - 1 - fileToLoadIndex);

            fileToLoadIndex += wcslen(argument);
        }
    }
}

bool CFmsLogo::OnInit()
{
    bool rval = true;

#ifdef MEM_DEBUG
#ifdef __WXMSW__
    g_Fmslogo = NULL;
    g_User32  = GetModuleHandle(L"user32.dll");
    if (g_User32 != NULL)
    {
        g_GetGuiResources = (GETGUIRESOURCES) GetProcAddress(g_User32, "GetGuiResources");
        if (g_GetGuiResources != NULL)
        {
            g_Fmslogo = OpenProcess(
                PROCESS_QUERY_INFORMATION,
                FALSE,
                GetCurrentProcessId());
            if (g_Fmslogo != NULL)
            {
                g_OriginalGuiObjects  = g_GetGuiResources(g_Fmslogo, GR_GDIOBJECTS);
                g_OriginalUserObjects = g_GetGuiResources(g_Fmslogo, GR_USEROBJECTS);
            }
        }
    }
#endif // __WXMSW__
#endif // MEM_DEBUG

    // Figure out the path that contains fmslogo.exe, which we
    // assume also holds Logolib.
    const wxFileName fmslogoExecutable(
        wxStandardPaths::Get().GetExecutablePath());
    const wxString & fmslogoPath = fmslogoExecutable.GetPath(
        wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR);

    wcsncpy(
        g_FmslogoBaseDirectory,
        fmslogoPath,
        ARRAYSIZE(g_FmslogoBaseDirectory));
    g_FmslogoBaseDirectory[ARRAYSIZE(g_FmslogoBaseDirectory) - 1] = '\0';

    ProcessCommandLine();
    if (g_MidiCliFailed ||
        (g_MidiOutputFile[0] != '\0' && g_FileToLoad[0] == '\0'))
    {
        if (g_MidiOutputFile[0] != '\0' && g_FileToLoad[0] == '\0')
        {
            std::cerr << "logo-midi: a Logo script must be supplied with -L.\n";
        }
        g_MidiCliFailed = true;
        return false;
    }
    if (g_MidiOutputFile[0] != '\0')
    {
        std::wcout << L"Logo MIDI by Dexter U.S.\n";
    }

    // Grab the single instance lock.
    g_SingleInstanceChecker = new wxSingleInstanceChecker();

#ifdef __WXMSW__
    // The Windows installer detects whether FMSLogo is running
    // by creating a mutex with the name LogoForWindowsMutex.
    // In order to stop an installer from running when FMSLogo
    // is running, we do the same.  On other platforms, there's
    // no need for this, so they can use the default name.
    g_SingleInstanceChecker->Create(L"LogoForWindowsMutex");
#endif

    bool isAlreadyRunning = g_SingleInstanceChecker->IsAnotherRunning();
    if (isAlreadyRunning)
    {
        // A copy of Logo is already running.
        if (g_FileToLoad[0] == '\0')
        {
            // No logo scripts were specified on the command-line.
            // We should re-use the existing window instead of creating a new
            // instance of logo, since this was probably just an accident.

            // Find that running copy of Logo and make it visible.
#ifndef WX_PURE
            HWND runningInstance = FindWindow(NULL, g_LOCALIZED_GENERAL_PRODUCTNAME);
            if (runningInstance != NULL)
            {
                // bring running instance to the the foreground
                ::SetForegroundWindow(runningInstance);
                if ( ::IsIconic(runningInstance) ||
                     !::IsWindowVisible(runningInstance))
                {
                    // the running instance is not visible, so restore it
                    ::ShowWindow(runningInstance, SW_SHOWDEFAULT);
                }

                delete g_SingleInstanceChecker;
                g_SingleInstanceChecker = nullptr;
                return false;
            }

            // We can't find the window, so we'll start up another instance.
            // The feature of not running two instances is supposed to make
            // things simpler.  If the other copy of Logo failed to exit
            // cleanly, or if some other application created the mutex,
            // it would more confusing if this Logo didn't start up.
#else
            // Return false to prevent starting another instance.
            delete g_SingleInstanceChecker;
            g_SingleInstanceChecker = nullptr;
            return false;
#endif
        }
        else
        {
            // a Logo script was specified on the command-line.
            // This means that we should open up a new instance
            // of Logo, even if one is already running.
        }
    }

    // Get video mode parameters
    init_videomode();

    srand(time(NULL));

    // init the timer callback array
    init_timers();

    // alloc and init the bitmap cut array
    init_bitmaps();

    // alloc and init the turtles array
    init_turtles();

    // init global pen state
    init_penstate();

    // init logo kernel
    init();

    // initialize the hourglass and arrow cursors
    init_cursors();

    // determine how big a window we would like
    int x = 0;
    int y = 0;
    int w = BitMapWidth;
    int h = BitMapHeight;

    // The main window should not exceed the size of the working area
    int maxWidth;
    int maxHeight;
    wxClientDisplayRect(NULL, NULL, &maxWidth, &maxHeight);

    // if fixed mode
    bool startMaximized;
    if (bFixed)
    {
        // FMSLogo was started with -F on the command-line.  In this case,
        // CMainFrame ignores the window size parameter (width and height)
        // and instead resizes itself to ensure that there is enough space
        // for the bitmap's width/height.  Therefore, this code block only
        // computes correct values for BitMapWidth and BitMapHeight, not
        // w and h.

        // If "-F" is given without "-W" or "-H", then FMSLogo should
        // start maximized.  This ensures the FMSLogo uses the most
        // amount of space available to it.
        startMaximized = !g_CustomHeight && !g_CustomWidth;

        // Reduce the maximum width available to the virtual service
        // that is taken up by the 3D border between the frame and the
        // screen.
        int borderWidth = wxSystemSettings::GetMetric(wxSYS_EDGE_X);
        if (borderWidth != -1)
        {
            maxWidth -= 2 * borderWidth;
        }

        // When an application is not maximized, it has a frame that reduces
        // the amount of space available to the window.
        // CMainFrame uses wxDEFAULT_FRAME_STYLE, which includes
        // wxRESIZE_BORDER.  In wx/toplevel.h, this is #define'd to be the
        // same as wxTHICK_FRAME, which is what wxSYS_FRAMESIZE_X returns.
        if (!startMaximized)
        {
            int frameWidth  = wxSystemSettings::GetMetric(wxSYS_FRAMESIZE_X);
            if (frameWidth != -1)
            {
                maxWidth -= 2 * frameWidth;
            }
        }

        if (g_CustomHeight)
        {
            // if height specified sanitize it against screen height
            BitMapHeight = std::min(h, maxHeight);
        }
        else
        {
            // Go "full screen", leaving space for the commander.
            BitMapHeight = (int) (maxHeight * ScreenSz);
        }

        if (g_CustomWidth)
        {
            // if width specified sanitize it against screen width
            BitMapWidth = std::min(w, maxWidth);
        }
        else
        {
            // Use the full width available to a window with a frame.
            BitMapWidth = maxWidth;
        }
    }
    else
    {
        // sanitize window size we would like against screen size
        w = std::min(w, maxWidth);
        h = std::min(h, (int) (maxHeight * ScreenSz));

        GetConfigurationQuadruple(L"Screen", &x, &y, &w, &h);

        // the smallest reasonable size is 400 x 400.
        h = std::max(h, 400);
        w = std::max(w, 400);

        // sanitize against screen size
        checkwindow(&x, &y, &w, &h);

        // Don't normally start maximized.
        startMaximized = false;
    }

    // initialize the values for some of the graphics-related
    // global variables.
    init_graphics();

    // create and show the main frame
    CMainFrame * frame = new CMainFrame(
        BitMapWidth,
        BitMapHeight,
        wxPoint(x, y),
        wxSize(w, h),
        startMaximized);

    frame->Show(g_MidiOutputFile[0] == '\0');

    // Force the initial screen painting to draw the turtle
    // in its initial position.  This isn't needed on MSW,
    // but is on GTK.
    GetScreenWxWindow()->Refresh(false);

    return rval;
}

int CFmsLogo::OnExit()
{
    // cleanup all subsystems
    uninit();

    uninit_bitmaps();

    uninit_turtles();

    uninit_graphics();

    // release the Help subsystem
    HtmlHelpUninitialize();

    delete g_SingleInstanceChecker;
    g_SingleInstanceChecker = nullptr;

#ifdef MEM_DEBUG
#ifdef __WXMSW__
    if (g_Fmslogo != NULL)
    {
        // Check if any GUI objects were leaked
        DWORD currentGuiObjects = g_GetGuiResources(g_Fmslogo, GR_GDIOBJECTS);
        if (g_OriginalGuiObjects < currentGuiObjects)
        {
            TraceOutput(
                L"%d GUI objects were leaked.\n",
                currentGuiObjects - g_OriginalUserObjects);
        }

        // Check if any USER objects were leaked
        DWORD currentUserObjects = g_GetGuiResources(g_Fmslogo, GR_USEROBJECTS);
        if (g_OriginalUserObjects < currentUserObjects)
        {
            TraceOutput(
                L"%d USER objects were leaked.\n",
                currentUserObjects - g_OriginalUserObjects);
        }

        CloseHandle(g_Fmslogo);
    }
#endif // __WXMSW__
#endif // MEM_DEBUG

#if wxUSE_CLIPBOARD
    // wxWidgets clears the clipboard when it exits, presumably to save memory.
    // To me, this behavior is unintuitative and confusing, as it's something
    // that Windows applications typically don't do.  To avoid this, we "flush"
    // the clipboard, which means only that if the clipboard's data came from
    // FMSLogo, it will continue to be available after FMSLogo exits.
    wxClipboard::Get()->Flush();
#endif

    int exitCode = wxApp::OnExit();
    return g_MidiCliFailed ? 1 : exitCode;
}

// Returns a pointer to the main window.
// Returns NULL when the application is exiting.
CMainFrame * CFmsLogo::GetMainFrame()
{
    return static_cast<CMainFrame*>(wxTheApp->GetTopWindow());
}

void CFmsLogo::OnIdle(wxIdleEvent & IdleEvent)
{
    // Don't do anything before the main window exists or after it's closed.
    if (wxTheApp->GetTopWindow() != NULL)
    {
        // If -P was specified on the command line, enter perspective mode.
        if (g_EnterPerspectiveMode)
        {
            g_EnterPerspectiveMode = false;
            lperspective(NIL);
        }

        // run the script that localizes FMSLogo
        static bool hasRunStartup = false;
        if (!hasRunStartup)
        {
            hasRunStartup = true;

            wchar_t startupScript[MAX_PATH + 1];
            MakeHelpPathName(startupScript, L"startup.logoscript");
            silent_load(NIL, startupScript);
        }

        // If a file to load was given on the command line, then execute it.
        static bool hasLoadedFileToLoad = false;
        if (!hasLoadedFileToLoad && g_FileToLoad[0] != '\0')
        {
            // Set that we have loaded the file before we actually do
            // in case loading the file causes and idle event to be sent
            // and re-executes this function.
            hasLoadedFileToLoad = true;
            if (g_MidiOutputFile[0] != '\0')
            {
                try
                {
                    logo_midi::ScopedMusicCommandSink activeSink(&g_MidiEngine);
                    if (!std::filesystem::exists(
                            std::filesystem::path(g_FileToLoad)))
                    {
                        throw std::runtime_error("Logo script file not found.");
                    }
                    silent_load(NIL, g_FileToLoad);
                    if (!g_MidiCliFailed)
                    {
                        g_MidiEngine.write_midi_file(
                            std::filesystem::path(g_MidiOutputFile));
                        std::wcout << L"Wrote MIDI file: " << g_MidiOutputFile
                                   << L"\n";
                    }
                }
                catch (const std::exception & error)
                {
                    std::cerr << "logo-midi: " << error.what() << "\n";
                    g_MidiCliFailed = true;
                }
                wxTheApp->ExitMainLoop();
            }
            else
            {
                silent_load(NIL, g_FileToLoad);
            }
        }

        // Process logo events
        checkqueue();
    }

    // Continue with the default processing
    IdleEvent.Skip();
}

BEGIN_EVENT_TABLE(CFmsLogo, wxApp)
    EVT_IDLE(CFmsLogo::OnIdle)
END_EVENT_TABLE()

void single_step_box(NODE * the_line)
{
    // A flag which indicates that a "Single Step" dialog box is visible.
    static bool s_SingleStepBoxIsShown = false;

    // Pop up single step box showing the line of code.
    //
    // Only one to show at most one at a time so that, if LOGO
    // instructions are run as callbacks (for example, with SETTIMER)
    // we don't end up flooding the screen and overflowing the stack
    // with single step dialog boxes.  There are other ways to prevent
    // multiple single step dialog boxes from appears, such as not
    // running callbacks when single stepping, but this is the simplest.
    if (!s_SingleStepBoxIsShown)
    {
        s_SingleStepBoxIsShown = true;

        // Print the line into a buffer, honoring the current printing limits.
        CStringPrintedNode printedLine(the_line, CStringPrintedNode::WithPrintLimits);

        if (wxMessageBox(
                printedLine.GetString(),
                g_LOCALIZED_STEPPING,
                wxOK | wxCANCEL) == wxCANCEL)
        {
            if (stepflag)
            {
                // The Step button is depressed.
                // Act like someone pressed the "UnStep" button.
                CFmsLogo::GetMainFrame()->GetCommander()->ToggleStep();
            }
            else
            {
                err_logo(STOP_ERROR, NIL);
            }
        }
        s_SingleStepBoxIsShown = false;
    }
}

wchar_t * promptuser(const wchar_t *prompt)
{
    return CFmsLogo::GetMainFrame()->PromptUserForInput(prompt);
}

wxWindow * GetParentWindowForDialog()
{
    return CFmsLogo::GetMainFrame()->GetParentWindowForDialog();
}

#ifndef WX_PURE
HWND GetScreenWindow()
{
    return GetScreenWxWindow()->GetHandle();
}

HWND GetMainWindow()
{
    return GetMainWxWindow()->GetHandle();
}

#endif // WX_PURE

wxWindow * GetMainWxWindow() {
    return CFmsLogo::GetMainFrame();
}

wxWindow * GetScreenWxWindow() {
    return CFmsLogo::GetMainFrame()->GetScreen();
}

wxWindow * GetCommanderWxWindow()
{
    assert(CFmsLogo::GetMainFrame() != NULL);
    return CFmsLogo::GetMainFrame()->GetTopLevelWindowForCommander();
}

wxWindow * GetEditorWxWindow()
{
    return CFmsLogo::GetMainFrame()->GetWorkspaceEditor();
}

int GetScreenHorizontalScrollPosition()
{
    return CFmsLogo::GetMainFrame()->GetScreen()->GetScrollPos(wxHORIZONTAL);
}

int GetScreenVerticalScrollPosition()
{
    return CFmsLogo::GetMainFrame()->GetScreen()->GetScrollPos(wxVERTICAL);
}

void SetScreenScrollPosition(int X, int Y)
{
    CFmsLogo::GetMainFrame()->GetScreen()->Scroll(X, Y);
}

bool IsEditorOpen()
{
    CMainFrame* mainFrame = CFmsLogo::GetMainFrame();

    if (mainFrame == NULL)
    {
        return false;
    }

    return mainFrame->IsEditorOpen();
}

void OpenEditorToLocationOfFirstError(const wchar_t *FileName)
{
    CFmsLogo::GetMainFrame()->PopupEditorToError(FileName);
}

#ifndef WX_PURE
HDC GetScreenDeviceContext()
{
    CMainFrame* mainFrame = CFmsLogo::GetMainFrame();
    return mainFrame->GetScreen()->GetScreenDeviceContext().GetHDC();
}

HDC GetMemoryDeviceContext()
{
    return GetWxMemoryDeviceContext()->GetHDC();
}

#endif // WX_PURE

wxDC * GetWxMemoryDeviceContext()
{
    return &CFmsLogo::GetMainFrame()->GetScreen()->GetMemoryDeviceContext();
}

wxDC * GetWxScreenDeviceContext()
{
    return &CFmsLogo::GetMainFrame()->GetScreen()->GetScreenDeviceContext();
}

void OpenStatusWindow()
{
    CMainFrame * mainFrame = CFmsLogo::GetMainFrame();
    mainFrame->ShowStatus();
    mainFrame->GetCommander()->UpdateStatusButtonState();
}

void UpdateUiExecutionState()
{
    // Enable/Disable the Halt/Pause buttons according to the execution state.

    // The main frame can be null if FMSLogo is shutting down.
    CMainFrame * mainFrame = CFmsLogo::GetMainFrame();
    if (mainFrame != NULL)
    {
        mainFrame->GetCommander()->UpdateHaltButtonState();
        mainFrame->GetCommander()->UpdatePauseButtonState();
    }
}

CStatusDialog * GetStatusDialog()
{
    CStatusDialog * statusDialog = CFmsLogo::GetMainFrame()->GetStatusDialog();
    assert(statusDialog != NULL);
    return statusDialog;
}

void CloseStatusWindow()
{
    CMainFrame * mainFrame = CFmsLogo::GetMainFrame();
    mainFrame->HideStatus();
    mainFrame->GetCommander()->UpdateStatusButtonState();
}

void AdjustScrollPositionToZoomFactor(FLONUM NewZoomFactor)
{
    CMainFrame * mainFrame = CFmsLogo::GetMainFrame();
    mainFrame->GetScreen()->AdjustScrollPositionToZoomFactor(NewZoomFactor);
}

void UndockCommanderWindow()
{
    CFmsLogo::GetMainFrame()->UndockCommanderWindow();
}

void DockCommanderWindow()
{
    CFmsLogo::GetMainFrame()->DockCommanderWindow();
}

#ifndef WX_PURE
#ifdef __WXMSW__
bool TranslateKeyboardShortcut(MSG & Message)
{
    return CFmsLogo::GetMainFrame()->TranslateKeyboardShortcut(Message);
}
#endif
#endif

// Returns non-zero if the editor could not be opened.
// Returns zero if the editor was successfully opened on FileName.
// The editor remains open after this function returns.
int ShowEditorForFile(const wchar_t *FileName, NODE * EditArguments)
{
    return CMainFrame::PopupEditorForFile(FileName, EditArguments);
}

void
ShowProcedureMiniEditor(
    const wchar_t  * ToLine,
    CDynamicBuffer & ReadBuffer
    )
{
    CMiniEditor miniEditor(CFmsLogo::GetMainFrame(), ToLine);

    if (wxID_OK != miniEditor.ShowModal())
    {
        // The user cancelled the definition
        err_logo(STOP_ERROR, NIL);
    }
    else
    {
        // copy the new definition into the read buffer.
        bool haveCarriageReturn = false;
        const wxString & body = miniEditor.GetProcedureBody();
        for (wxString::const_iterator i = body.begin(); i != body.end(); ++i)
        {
            if (haveCarriageReturn && *i != '\n')
            {
                // The carriage return was not followed by a newline,
                // so it was not part of an EOL sequence.
                ReadBuffer.AppendChar('\r');
            }
            if (*i == '\r')
            {
                // Delay writing the CR in case the next character is an LF.
                // This will map the CRLF into the UNIX EOL sequence.
                haveCarriageReturn = true;
                continue;
            }

            ReadBuffer.AppendChar(*i);
            haveCarriageReturn = false;
        }

        ReadBuffer.AppendChar('\n');
        ReadBuffer.AppendString(End.GetName());
    }
}

void
TraceOutput(
    const wchar_t * FormatString,
    ...
    )
{
    // Format and print the message to stderr
    va_list args;
    va_start(args, FormatString);
    int rval = vfwprintf(stderr, FormatString, args);
    va_end(args);

    // On GNU/Linux, stderr is unavailable from an atexit() handler
    // when the debug heap prints leaked memory.  If vfwprintf()
    // returns an error, then try again on stdout, which is still
    // writable at that time.
    if (rval < 0)
    {
        va_start(args, FormatString);
        vfwprintf(stdout, FormatString, args);
        va_end(args);
    }

#ifdef __WXMSW__
    wchar_t formattedString[256] = {0};

    va_start(args, FormatString);
    int bytesNeeded = vswprintf(
        formattedString,
        ARRAYSIZE(formattedString) - 1,
        FormatString,
        args);
    va_end(args);

    if (0 <= bytesNeeded)
    {
        OutputDebugString(formattedString);
    }
#endif
}
