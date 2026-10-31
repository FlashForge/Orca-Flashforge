#ifndef slic3r_GUI_AddPrinterDialog_hpp_
#define slic3r_GUI_AddPrinterDialog_hpp_

#include <algorithm>
#include <chrono>
#include <future>
#include <wx/listctrl.h>
#include <wx/sizer.h>
#include <wx/timer.h>
#include "FFTitleLessDialog.hpp"
#include "DeviceData.hpp"
#include "MultiComUtils.hpp"
#include "slic3r/GUI/ConnectPrinter.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/Widgets/Button.hpp"
#include "slic3r/GUI/Widgets/Label.hpp"

namespace Slic3r { namespace GUI {

// The add action is a stable dialog, independent of the account and of the
// transient printer-selection popup. Discovery never runs on the UI thread.
class AddPrinterDialog : public FFTitleLessDialog
{
public:
    explicit AddPrinterDialog(wxWindow *parent)
        : FFTitleLessDialog(parent), m_timer(this)
    {
        SetTitle(_L("Add Device"));
        SetBackgroundColour(*wxWHITE);
        // The shaped dialog paints its background itself. Clip it around the
        // child windows, otherwise the first parent paint can overwrite the
        // native labels/header until a mouse-over makes them repaint.
        SetWindowStyleFlag(GetWindowStyleFlag() | wxCLIP_CHILDREN);
#ifdef __WXMSW__
        SetDoubleBuffered(true);
#endif
        auto *sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(new Label(this, Label::Head_20, _L("Add Device")), 0,
            wxLEFT | wxRIGHT | wxTOP, FromDIP(28));
        auto *description = new Label(this, Label::Body_14,
            _L("Find printers on the same network. A Flashforge account is not required for a LAN connection."), LB_AUTO_WRAP);
        description->Wrap(FromDIP(584));
        sizer->Add(description, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(28));

        auto *searchBar = new wxBoxSizer(wxHORIZONTAL);
        m_status = new Label(this, Label::Body_14,
            _L("Searching the local network..."), wxST_NO_AUTORESIZE);
        m_status->SetMinSize(FromDIP(wxSize(400, 40)));
        m_status->SetForegroundColour(wxColour("#555555"));
        searchBar->Add(m_status, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(16));
        m_refresh = new Button(this, _L("Search again"));
        m_refresh->SetStyle(ButtonStyle::Regular, ButtonType::Choice);
        m_refresh->SetMinSize(FromDIP(wxSize(112, 36)));
        m_refresh->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Search(); });
        searchBar->Add(m_refresh, 0, wxALIGN_CENTER_VERTICAL);
        sizer->Add(searchBar, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(28));

        m_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
            wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_SIMPLE);
        m_list->SetFont(Label::Body_14);
        m_list->SetName(_L("Printers found on the local network"));
        m_list->InsertColumn(0, _L("Printer name"), wxLIST_FORMAT_LEFT, FromDIP(264));
        m_list->InsertColumn(1, _L("IP address"), wxLIST_FORMAT_LEFT, FromDIP(164));
        m_list->InsertColumn(2, _L("Connection"), wxLIST_FORMAT_LEFT, FromDIP(128));
        m_list->Bind(wxEVT_LIST_ITEM_SELECTED, [this](wxListEvent &) { UpdateConnectButton(); });
        m_list->Bind(wxEVT_LIST_ITEM_DESELECTED, [this](wxListEvent &) { UpdateConnectButton(); });
        m_list->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent &) { Connect(); });
        sizer->Add(m_list, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(28));

        auto *hint = new Label(this, Label::Body_12,
            _L("Select a printer and connect using the access code shown on its screen."), LB_AUTO_WRAP);
        hint->SetForegroundColour(wxColour("#666666"));
        hint->Wrap(FromDIP(584));
        sizer->Add(hint, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(28));
        auto *buttons = new wxBoxSizer(wxHORIZONTAL);
        auto *close = new Button(this, _L("Close"), "", 0, 0, wxID_CANCEL);
        close->SetStyle(ButtonStyle::Regular, ButtonType::Choice);
        close->SetMinSize(FromDIP(wxSize(96, 40)));
        close->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });
        buttons->Add(close);
        buttons->AddStretchSpacer();
        m_connect = new Button(this, _L("Connect via LAN"));
        m_connect->SetStyle(ButtonStyle::Confirm, ButtonType::Choice);
        m_connect->SetMinSize(FromDIP(wxSize(208, 40)));
        m_connect->Disable();
        m_connect->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Connect(); });
        buttons->Add(m_connect);
        sizer->Add(buttons, 0, wxEXPAND | wxALL, FromDIP(28));
        SetSizer(sizer);
        SetClientSize(FromDIP(wxSize(640, 496)));
        CentreOnParent();
        Bind(wxEVT_TIMER, [this](wxTimerEvent &) { PollSearch(); });
        // Paint the dialog and its search state before starting discovery.
        Bind(wxEVT_SHOW, [this](wxShowEvent &event) {
            event.Skip();
            if (event.IsShown()) CallAfter([this] {
                Layout();
                Refresh();
                Search();
            });
        });
    }

    ~AddPrinterDialog() override
    {
        m_timer.Stop();
        // The worker owns only its result, never this dialog or GUI objects.
        if (m_scan.valid()) m_scan.wait();
    }

    const std::string &connectedPrinter() const { return m_connectedPrinter; }

private:
    struct ScanResult {
        ComErrno result { COM_ERROR };
        std::vector<fnet_lan_dev_info> devices;
    };

    std::string SelectedPrinter() const
    {
        const long index = m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        return index >= 0 && static_cast<size_t>(index) < m_ids.size() ? m_ids[index] : std::string();
    }

    void UpdateConnectButton()
    {
        m_connect->Enable(!m_scan.valid() && !SelectedPrinter().empty());
    }

    void Search()
    {
        if (m_scan.valid()) return;
        m_status->SetLabel(_L("Searching the local network..."));
        m_refresh->Disable();
        m_connect->Disable();
        m_scan = std::async(std::launch::async, [] {
            ScanResult scan;
            scan.result = MultiComUtils::getLanDevList(scan.devices);
            return scan;
        });
        m_timer.Start(50);
    }

    void PollSearch()
    {
        if (!m_scan.valid() || m_scan.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            return;
        m_timer.Stop();
        ScanResult scan;
        try { scan = m_scan.get(); }
        catch (...) { scan.result = COM_ERROR; }
        const std::string previous = SelectedPrinter();
        m_list->Freeze();
        m_list->DeleteAllItems();
        m_ids.clear();
        if (scan.result == COM_OK) {
            wxGetApp().getDeviceObjectOpr()->update_scan_machine(scan.devices);
            std::sort(scan.devices.begin(), scan.devices.end(), [](const auto &a, const auto &b) {
                return std::string(a.name) < std::string(b.name);
            });
            for (const auto &device : scan.devices) {
                const long index = m_list->InsertItem(m_list->GetItemCount(), wxString::FromUTF8(device.name));
                m_list->SetItem(index, 1, wxString::FromUTF8(device.ip));
                m_list->SetItem(index, 2, "LAN");
                m_ids.emplace_back(device.serialNumber);
                if (m_ids.back() == previous) m_list->SetItemState(index, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
            }
            m_status->SetLabel(m_ids.empty()
                ? _L("No printers found. Check the network and search again.")
                : wxString::Format(_L("Printers found: %d"), static_cast<int>(m_ids.size())));
        } else {
            m_status->SetLabel(_L("Printer discovery failed. Check the network and try again."));
        }
        m_list->Thaw();
        m_status->Wrap(FromDIP(400));
        m_refresh->Enable();
        UpdateConnectButton();
        Layout();
        Refresh();
    }

    void Connect()
    {
        if (m_scan.valid()) return;
        const std::string id = SelectedPrinter();
        if (id.empty()) return;
        std::map<std::string, DeviceObject *> devices;
        wxGetApp().getDeviceObjectOpr()->get_scan_machine(devices);
        const auto found = devices.find(id);
        if (found == devices.end() || !found->second->get_lan_dev_info()) {
            Search();
            return;
        }
        ConnectPrinterDialog dialog;
        dialog.set_device_object(found->second);
        if (dialog.ShowModal() == wxID_OK) {
            m_connectedPrinter = id;
            EndModal(wxID_OK);
        }
    }

    wxTimer m_timer;
    std::future<ScanResult> m_scan;
    wxListCtrl *m_list { nullptr };
    Label *m_status { nullptr };
    Button *m_refresh { nullptr };
    Button *m_connect { nullptr };
    std::vector<std::string> m_ids;
    std::string m_connectedPrinter;
};

}} // namespace Slic3r::GUI
#endif
