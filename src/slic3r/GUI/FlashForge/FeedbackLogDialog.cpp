#include "FeedbackLogDialog.hpp"

#include <algorithm>
#include <map>
#include <utility>

#include <wx/radiobut.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>

#include "slic3r/GUI/FFUtils.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/MsgDialog.hpp"
#include "slic3r/GUI/Widgets/Button.hpp"
#include "slic3r/GUI/Widgets/Label.hpp"
#include "slic3r/GUI/FlashForge/DeviceData.hpp"

namespace Slic3r { namespace GUI {

namespace {

class FeedbackDeviceSelectDialog final : public wxDialog
{
public:
    FeedbackDeviceSelectDialog(wxWindow* parent, const std::vector<FeedbackDeviceInfo>& devices, const std::string& selectedSerialNumber)
        : wxDialog(parent, wxID_ANY, _L("Select Device"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
        , m_devices(devices)
    {
        SetBackgroundColour(*wxWHITE);
        SetSize(FromDIP(wxSize(520, 650)));
        SetMinSize(FromDIP(wxSize(440, 480)));

        auto* listWindow = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_NONE);
        listWindow->SetBackgroundColour(*wxWHITE);
        listWindow->SetScrollRate(0, FromDIP(10));

        auto* listSizer = new wxBoxSizer(wxVERTICAL);
        for (std::size_t index = 0; index < m_devices.size(); ++index) {
            addDeviceRow(listWindow, listSizer, index);
            if (index + 1 < m_devices.size()) {
                auto* separator = new wxPanel(listWindow, wxID_ANY);
                separator->SetBackgroundColour(wxColour("#eeeeee"));
                separator->SetMinSize(wxSize(-1, FromDIP(1)));
                listSizer->Add(separator, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(28));
            }
        }
        listSizer->AddStretchSpacer();
        listWindow->SetSizer(listSizer);

        auto* footerLine = new wxPanel(this, wxID_ANY);
        footerLine->SetBackgroundColour(wxColour("#e5e5e5"));
        footerLine->SetMinSize(wxSize(-1, FromDIP(1)));

        auto* confirmButton = new Button(this, _L("Confirm"));
        confirmButton->SetMinSize(FromDIP(wxSize(110, 40)));
        confirmButton->SetStyle(ButtonStyle::Confirm, ButtonType::Choice);
        confirmButton->Enable(false);
        confirmButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            if (m_selectedIndex >= 0) {
                EndModal(wxID_OK);
            }
        });
        m_confirmButton = confirmButton;

        auto* cancelButton = new Button(this, _L("Cancel"));
        cancelButton->SetMinSize(FromDIP(wxSize(110, 40)));
        cancelButton->SetStyle(ButtonStyle::Regular, ButtonType::Choice);
        cancelButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_CANCEL); });

        auto* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
        buttonSizer->AddStretchSpacer();
        buttonSizer->Add(confirmButton, 0, wxRIGHT, FromDIP(24));
        buttonSizer->Add(cancelButton, 0);
        buttonSizer->AddStretchSpacer();

        auto* mainSizer = new wxBoxSizer(wxVERTICAL);
        mainSizer->Add(listWindow, 1, wxEXPAND | wxTOP, FromDIP(12));
        mainSizer->Add(footerLine, 0, wxEXPAND);
        mainSizer->Add(buttonSizer, 0, wxEXPAND | wxTOP | wxBOTTOM, FromDIP(22));
        SetSizer(mainSizer);

        if (!selectedSerialNumber.empty()) {
            for (std::size_t index = 0; index < m_devices.size(); ++index) {
                if (m_devices[index].serialNumber == selectedSerialNumber) {
                    selectRow(index);
                    break;
                }
            }
        }

        CentreOnParent();
    }

    std::optional<FeedbackDeviceInfo> selectedDevice() const
    {
        if (m_selectedIndex < 0 || static_cast<std::size_t>(m_selectedIndex) >= m_devices.size()) {
            return std::nullopt;
        }
        return m_devices[static_cast<std::size_t>(m_selectedIndex)];
    }

private:
    struct DeviceRow
    {
        wxPanel*       panel{nullptr};
        wxRadioButton* radio{nullptr};
        wxStaticText*  name{nullptr};
    };

    void addDeviceRow(wxWindow* parent, wxBoxSizer* listSizer, std::size_t index)
    {
        const FeedbackDeviceInfo& device = m_devices[index];
        auto*                     row    = new wxPanel(parent, wxID_ANY);
        row->SetBackgroundColour(*wxWHITE);
        row->SetMinSize(wxSize(-1, FromDIP(108)));

        auto* radio = new wxRadioButton(row, wxID_ANY, wxEmptyString);
        radio->SetValue(false);

        std::string bitmapName = "printer_thumbnail";
        const auto  presetIt   = FFUtils::printer_preset_map.find(device.pid);
        if (presetIt != FFUtils::printer_preset_map.end()) {
            bitmapName = presetIt->second.bmp_file_name;
        }
        auto* thumbnail = new wxStaticBitmap(row, wxID_ANY, create_scaled_bitmap(bitmapName, row, 80, !device.online));
        thumbnail->SetMinSize(FromDIP(wxSize(88, 88)));

        auto* verticalLine = new wxPanel(row, wxID_ANY);
        verticalLine->SetBackgroundColour(wxColour("#d8d8d8"));
        verticalLine->SetMinSize(wxSize(FromDIP(1), FromDIP(36)));

        wxString label = from_u8(device.name.empty() ? device.serialNumber : device.name);
        if (!device.online) {
            label += "\n" + _L("Device offline");
        }
        auto* name = new wxStaticText(row, wxID_ANY, label, wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
        name->SetFont(Label::Body_14);
        name->SetForegroundColour(device.online ? wxColour("#333333") : wxColour("#8a8a8a"));

        auto* rowSizer = new wxBoxSizer(wxHORIZONTAL);
        rowSizer->Add(radio, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(24));
        rowSizer->Add(thumbnail, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(18));
        rowSizer->Add(verticalLine, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(16));
        rowSizer->Add(name, 1, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(20));
        row->SetSizer(rowSizer);

        const auto select = [this, index]() { selectRow(index); };
        radio->Bind(wxEVT_RADIOBUTTON, [select](wxCommandEvent&) { select(); });
        row->Bind(wxEVT_LEFT_DOWN, [select](wxMouseEvent&) { select(); });
        thumbnail->Bind(wxEVT_LEFT_DOWN, [select](wxMouseEvent&) { select(); });
        name->Bind(wxEVT_LEFT_DOWN, [select](wxMouseEvent&) { select(); });

        m_rows.push_back({row, radio, name});
        listSizer->Add(row, 0, wxEXPAND);
    }

    void selectRow(std::size_t selectedIndex)
    {
        if (selectedIndex >= m_rows.size()) {
            return;
        }
        m_selectedIndex = static_cast<int>(selectedIndex);
        for (std::size_t index = 0; index < m_rows.size(); ++index) {
            const bool selected = index == selectedIndex;
            m_rows[index].radio->SetValue(selected);
            const wxColour background = selected ? wxColour("#f2f9fd") : *wxWHITE;
            m_rows[index].panel->SetBackgroundColour(background);
            m_rows[index].radio->SetBackgroundColour(background);
            m_rows[index].name->SetBackgroundColour(background);
            m_rows[index].panel->Refresh();
        }
        m_confirmButton->Enable(true);
    }

private:
    std::vector<FeedbackDeviceInfo> m_devices;
    std::vector<DeviceRow>          m_rows;
    Button*                         m_confirmButton{nullptr};
    int                             m_selectedIndex{-1};
};

} // namespace

std::vector<FeedbackDeviceInfo> get_feedback_device_snapshot()
{
    std::vector<FeedbackDeviceInfo> result;
    DeviceObjectOpr*                deviceOpr = wxGetApp().getDeviceObjectOpr();
    if (deviceOpr == nullptr) {
        return result;
    }

    std::map<std::string, DeviceObject*> devices;
    deviceOpr->get_user_machine(devices);
    result.reserve(devices.size());
    for (const auto& [serialNumber, object] : devices) {
        if (object == nullptr || serialNumber.empty()) {
            continue;
        }
        result.push_back({serialNumber, object->get_dev_name(), object->get_dev_pid(), object->is_online()});
    }

    std::sort(result.begin(), result.end(), [](const FeedbackDeviceInfo& lhs, const FeedbackDeviceInfo& rhs) {
        if (lhs.online != rhs.online) {
            return lhs.online > rhs.online;
        }
        const int nameComparison = from_u8(lhs.name).CmpNoCase(from_u8(rhs.name));
        return nameComparison == 0 ? lhs.serialNumber < rhs.serialNumber : nameComparison < 0;
    });
    return result;
}

FeedbackDeviceResolution resolve_feedback_device(const std::vector<FeedbackDeviceInfo>& devices, const std::string& requestedSerialNumber)
{
    FeedbackDeviceResolution result;
    result.boundDeviceCount  = devices.size();
    result.onlineDeviceCount = static_cast<std::size_t>(
        std::count_if(devices.begin(), devices.end(), [](const FeedbackDeviceInfo& device) { return device.online; }));

    if (devices.empty()) {
        result.state = FeedbackDeviceState::NoDevice;
        return result;
    }

    if (!requestedSerialNumber.empty()) {
        const auto requested = std::find_if(devices.begin(), devices.end(), [&requestedSerialNumber](const FeedbackDeviceInfo& device) {
            return device.serialNumber == requestedSerialNumber;
        });
        if (requested != devices.end()) {
            result.device = *requested;
            result.state  = requested->online ? FeedbackDeviceState::Selected : FeedbackDeviceState::Offline;
            return result;
        }
        // Never silently replace an explicitly selected device after a list
        // refresh. It may have been unbound while the feedback form was open.
        result.state = FeedbackDeviceState::SelectionRequired;
        return result;
    }

    if (result.onlineDeviceCount == 1) {
        result.device = *std::find_if(devices.begin(), devices.end(), [](const FeedbackDeviceInfo& device) { return device.online; });
        result.state  = FeedbackDeviceState::AutoSelected;
        return result;
    }
    if (result.onlineDeviceCount > 1) {
        result.state = FeedbackDeviceState::SelectionRequired;
        return result;
    }

    result.state = FeedbackDeviceState::Offline;
    if (devices.size() == 1) {
        result.device = devices.front();
    }
    return result;
}

std::optional<FeedbackDeviceInfo> show_feedback_device_selector(wxWindow*                              parent,
                                                                const std::vector<FeedbackDeviceInfo>& devices,
                                                                const std::string&                     selectedSerialNumber)
{
    if (devices.empty()) {
        return std::nullopt;
    }
    FeedbackDeviceSelectDialog dialog(parent, devices, selectedSerialNumber);
    if (dialog.ShowModal() != wxID_OK) {
        return std::nullopt;
    }
    return dialog.selectedDevice();
}

bool confirm_feedback_without_device_log(wxWindow* parent)
{
    const wxString message = _L(
        "The current device is offline, so we are unable to retrieve the device logs at this time. You can still submit feedback. "
        "Once the device is back online, you can submit feedback again or go to Settings > Upload Firmware Log to upload the device "
        "logs.");

    RichMessageDialog dialog(parent, message, _L("Tip"), wxOK | wxCANCEL | wxICON_WARNING);
    dialog.SetOKCancelLabels(_L("Confirm"), _L("Cancel"));
    return dialog.ShowModal() == wxID_OK;
}

FeedbackLogLabels get_feedback_log_labels()
{
    return {_L("To help us identify and resolve your issue more quickly, we recommend uploading your device and app logs. The logs are "
               "used solely for troubleshooting and will not collect any of your personal information."),
            _L("Slice Log"),
            _L("Slicing software operation log"),
            _L("App Log"),
            _L("App operation log"),
            _L("Device Log"),
            _L("Device operation log"),
            _L("Add Device"),
            _L("Select Device")};
}

}} // namespace Slic3r::GUI
