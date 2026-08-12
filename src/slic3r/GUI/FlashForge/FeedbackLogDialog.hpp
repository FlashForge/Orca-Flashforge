#ifndef slic3r_FeedbackLogDialog_hpp_
#define slic3r_FeedbackLogDialog_hpp_

#include <optional>
#include <string>
#include <vector>

#include <wx/string.h>

#include "FeedbackDeviceSelection.hpp"

class wxWindow;

namespace Slic3r { namespace GUI {

struct FeedbackLogLabels
{
    wxString authorization;
    wxString sliceTitle;
    wxString sliceDescription;
    wxString appTitle;
    wxString appDescription;
    wxString deviceTitle;
    wxString deviceDescription;
    wxString addDevice;
    wxString selectDevice;
};

// These functions are UI-thread-only. DeviceObjectOpr owns its device pointers
// and may replace them during login, logout, or a device-list refresh, so callers
// receive value snapshots and must never retain DeviceObject pointers.
std::vector<FeedbackDeviceInfo> get_feedback_device_snapshot();

std::optional<FeedbackDeviceInfo> show_feedback_device_selector(wxWindow*                              parent,
                                                                const std::vector<FeedbackDeviceInfo>& devices,
                                                                const std::string&                     selectedSerialNumber = {});

bool confirm_feedback_without_device_log(wxWindow* parent);

FeedbackLogLabels get_feedback_log_labels();

}} // namespace Slic3r::GUI

#endif
