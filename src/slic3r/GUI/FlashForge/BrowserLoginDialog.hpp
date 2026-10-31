#ifndef slic3r_GUI_BrowserLoginDialog_hpp_
#define slic3r_GUI_BrowserLoginDialog_hpp_

#include <wx/dialog.h>
#include <wx/stream.h>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include <wx/button.h>
#include <wx/filename.h>
#include <wx/process.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/timer.h>
#include <wx/utils.h>
#include <chrono>
#include "libslic3r/Utils.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/Widgets/Button.hpp"
#include "slic3r/GUI/Widgets/Label.hpp"
#include "FFTitleLessDialog.hpp"
#include "MultiComDef.hpp"
#include "MultiComMgr.hpp"

namespace Slic3r { namespace GUI {

// Header-only so existing platform project files pick up the integration.
// The helper passes tokens over an anonymous pipe, never command arguments,
// URLs, files, the clipboard or the log. Google pages never enter a WebView.
class BrowserLoginDialog : public FFTitleLessDialog
{
public:
    explicit BrowserLoginDialog(wxWindow *parent)
        : FFTitleLessDialog(parent), m_timer(this)
    {
        SetTitle(_L("Browser login"));
        SetBackgroundColour(*wxWHITE);
        SetFont(Label::Body_14);
        auto *sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(new Label(this, Label::Head_20, _L("Browser login")), 0,
                   wxLEFT | wxRIGHT | wxTOP, FromDIP(28));
        auto *description = new Label(this, Label::Body_14,
            _L("Sign in to Google in your default browser. Your existing browser sign-in can be used. "
               "FlashStudio receives the result automatically. No browser extension is needed."), LB_AUTO_WRAP);
        description->SetForegroundColour(wxColour("#555555"));
        description->SetMinSize(FromDIP(wxSize(424, 64)));
        description->Wrap(FromDIP(424));
        sizer->Add(description, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(28));
        auto *statusPanel = new wxPanel(this);
        statusPanel->SetBackgroundColour(wxColour("#f1f7fc"));
        auto *statusSizer = new wxBoxSizer(wxVERTICAL);
        m_status = new Label(statusPanel, Label::Body_14, wxEmptyString, LB_AUTO_WRAP);
        m_status->SetMinSize(FromDIP(wxSize(392, 72)));
        m_status->SetForegroundColour(wxColour("#333333"));
        m_remaining = new Label(statusPanel, Label::Body_12);
        m_remaining->SetForegroundColour(wxColour("#666666"));
        statusSizer->Add(m_status, 1, wxEXPAND | wxALL, FromDIP(16));
        statusSizer->Add(m_remaining, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, FromDIP(16));
        statusPanel->SetSizer(statusSizer);
        sizer->Add(statusPanel, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(28));
        auto *buttons = new wxBoxSizer(wxHORIZONTAL);
        auto *cancel = new Button(this, _L("Close"), "", 0, 0, wxID_CANCEL);
        cancel->SetStyle(ButtonStyle::Regular, ButtonType::Choice);
        cancel->SetMinSize(FromDIP(wxSize(96, 40)));
        cancel->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });
        m_open = new Button(this, _L("Sign in using default browser"));
        m_open->SetStyle(ButtonStyle::Confirm, ButtonType::Choice);
        m_open->SetMinSize(FromDIP(wxSize(280, 40)));
        m_open->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Start(); });
        buttons->Add(cancel);
        buttons->AddStretchSpacer();
        buttons->Add(m_open);
        sizer->Add(buttons, 0, wxEXPAND | wxALL, FromDIP(28));
        SetSizer(sizer);
        SetClientSize(FromDIP(wxSize(480, 404)));
        SetStatus(_L("Ready to open your browser."));
        CentreOnParent();
        Bind(wxEVT_TIMER, [this](wxTimerEvent &) { Tick(); });
        Bind(wxEVT_END_PROCESS, &BrowserLoginDialog::OnProcessEnd, this);
        wxGetApp().Bind(EVT_LOGIN_FAILED, &BrowserLoginDialog::OnAccountFailed, this);
        wxGetApp().Bind(EVT_LOGIN_SUCCEED, &BrowserLoginDialog::OnAccountSucceeded, this);
        Bind(wxEVT_ACTIVATE, [this](wxActivateEvent &event) {
            event.Skip();
            if (event.GetActive() && m_process && Clock::now() - m_started > std::chrono::seconds(2))
                SetStatus(_L("No authentication result has arrived yet. Finish signing in in your browser, "
                             "or retry if it did not return to this app."));
        });
    }

    ~BrowserLoginDialog() override
    {
        wxGetApp().Unbind(EVT_LOGIN_FAILED, &BrowserLoginDialog::OnAccountFailed, this);
        wxGetApp().Unbind(EVT_LOGIN_SUCCEED, &BrowserLoginDialog::OnAccountSucceeded, this);
        Stop();
    }

private:
    using Clock = std::chrono::steady_clock;

    void SetStatus(const wxString &message, bool error = false)
    {
        m_status->SetForegroundColour(error ? wxColour("#b3261e") : wxColour("#333333"));
        m_status->SetLabel(message);
        m_status->Wrap(FromDIP(392));
        Layout();
    }

    void Stop()
    {
        m_timer.Stop();
        if (m_process) {
            auto *process = m_process;
            m_process = nullptr; // Ignore any queued event from the old attempt.
            process->Detach();
            wxProcess::Kill(m_pid, wxSIGTERM, wxKILL_CHILDREN);
        }
        m_pid = 0;
        m_checkingSession = false;
        m_output.clear();
        m_remaining->SetLabel(wxEmptyString);
    }

    void Fail(const wxString &message)
    {
        Stop();
        m_open->SetLabel(_L("Try again in browser"));
        m_open->Enable();
        SetStatus(message, true);
    }

    void Start()
    {
        // Retry also cancels a browser attempt that never called back.
        Stop();
        if (MultiComMgr::inst()->networkIntfc() == nullptr) {
            Fail(_L("The printer communication module is unavailable. Restart the application using the complete build folder."));
            return;
        }
        if (m_sessionStarted) {
            CheckAccount();
            return;
        }
        const wxString helper = wxString::FromUTF8(resources_dir()) +
                                "/browser-auth/flashstudio-browser-auth.exe";
        if (!wxFileExists(helper)) {
            Fail(_L("Browser login helper is missing. Build and package the helper before running this fork."));
            return;
        }
        m_output.clear();
        m_process = new wxProcess(this);
        m_process->Redirect();
        const wxString region = wxGetApp().app_config->get("region") == "China" ? "cn" : "global";
        const wxChar *argv[] = {helper.c_str(), wxT("--region"), region.c_str(), nullptr};
        m_pid = wxExecute(argv, wxEXEC_ASYNC | wxEXEC_HIDE_CONSOLE | wxEXEC_MAKE_GROUP_LEADER, m_process);
        if (m_pid <= 0) {
            delete m_process;
            m_process = nullptr;
            Fail(_L("Could not start browser login."));
            return;
        }
        m_started = Clock::now();
        m_deadline = m_started + std::chrono::seconds(300);
        m_open->SetLabel(_L("Try again in browser"));
        SetStatus(_L("Waiting for authentication from your browser. You can close this window or retry at any time."));
        m_timer.Start(100);
        Tick();
    }

    bool ReadPipe()
    {
        if (!m_process) return true;
        auto *stream = m_process->GetInputStream();
        while (stream && stream->CanRead()) {
            char buffer[1024];
            stream->Read(buffer, sizeof(buffer));
            m_output.append(buffer, stream->LastRead());
            if (m_output.size() > 24576) {
                Fail(_L("The authentication response was invalid. Please try again."));
                return false;
            }
        }
        return true;
    }

    void Tick()
    {
        if (m_checkingSession) {
            if (wxGetApp().is_flashforge_login()) {
                m_checkingSession = false;
                m_timer.Stop();
                if (wxGetApp().mainframe) wxGetApp().mainframe->Raise();
                EndModal(wxID_OK);
                return;
            }
            if (Clock::now() >= m_deadline) {
                Fail(_L("Google authentication finished, but the app could not confirm the account. Please try again."));
                return;
            }
        }
        if (!ReadPipe()) return;
        if (!m_process && !m_checkingSession) return;
        const auto now = Clock::now();
        if (now >= m_deadline) {
            Fail(_L("No authentication result was received within five minutes. Please try again."));
            return;
        }
        const int seconds = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(m_deadline - now).count()) + 1;
        m_remaining->SetLabel(wxString::Format(_L("Time remaining: %d:%02d"), seconds / 60, seconds % 60));
    }

    void OnProcessEnd(wxProcessEvent &event)
    {
        if (!m_process || event.GetPid() != m_pid) return;
        m_timer.Stop();
        if (!ReadPipe()) return;
        delete m_process;
        m_process = nullptr;
        m_open->Enable();
        try {
            if (event.GetExitCode() != 0) {
                switch (event.GetExitCode()) {
                case 3: Fail(_L("No authentication result was received within five minutes. Please try again.")); break;
                case 5: Fail(_L("Sign-in was cancelled in the browser. You can try again.")); break;
                case 6: Fail(_L("The login server rejected the authentication result. Please try again.")); break;
                case 7: Fail(_L("The browser return address is in use. Close the other browser login attempt or application using port 3000, then try again.")); break;
                default: Fail(_L("Browser login did not complete. The login server must allow returning to this desktop app.")); break;
                }
                return;
            }
            const auto data = nlohmann::json::parse(m_output);
            com_token_data_t token{};
            token.accessToken = data.at("access_token").get<std::string>();
            token.refreshToken = data.at("refresh_token").get<std::string>();
            token.expiresIn = data.at("expires_in").get<int>();
            token.startTime = data.at("start_time").get<long long>();
            if (token.accessToken.empty() || token.refreshToken.empty() || token.expiresIn <= 0)
                throw std::runtime_error("invalid token response");
            m_output.clear();
            if (!wxGetApp().begin_flashforge_login(token))
                throw std::runtime_error("session initialization rejected");
            m_sessionStarted = true;
            CheckAccount();
        } catch (...) {
            m_output.clear();
            Fail(_L("The authentication response was invalid. Please try again."));
        }
    }

    void CheckAccount()
    {
        m_checkingSession = true;
        m_started = Clock::now();
        m_deadline = m_started + std::chrono::seconds(90);
        m_open->Disable();
        SetStatus(_L("Google authentication finished. Confirming your account in FlashStudio..."));
        m_timer.Start(100);
    }

    void OnAccountFailed(wxCommandEvent &event)
    {
        event.Skip();
        if (!m_sessionStarted) return;
        m_sessionStarted = false;
        Fail(_L("Google authentication finished, but the app could not confirm the account. Please try again."));
    }

    void OnAccountSucceeded(wxCommandEvent &event)
    {
        event.Skip();
        if (m_sessionStarted && !m_checkingSession) CheckAccount();
    }

    wxTimer m_timer;
    Clock::time_point m_started{}, m_deadline{};
    wxProcess *m_process{nullptr};
    long m_pid{0};
    bool m_checkingSession{false};
    bool m_sessionStarted{false};
    std::string m_output;
    Label *m_status;
    Label *m_remaining;
    Button *m_open;
};

}}
#endif
