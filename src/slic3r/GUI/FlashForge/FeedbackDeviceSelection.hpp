#ifndef slic3r_FeedbackDeviceSelection_hpp_
#define slic3r_FeedbackDeviceSelection_hpp_

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace Slic3r { namespace GUI {

struct FeedbackDeviceInfo
{
    std::string    serialNumber;
    std::string    name;
    unsigned short pid{0};
    bool           online{false};
};

enum class FeedbackDeviceState { NoDevice, AutoSelected, SelectionRequired, Selected, Offline };

struct FeedbackDeviceResolution
{
    FeedbackDeviceState               state{FeedbackDeviceState::NoDevice};
    std::optional<FeedbackDeviceInfo> device;
    std::size_t                       boundDeviceCount{0};
    std::size_t                       onlineDeviceCount{0};
};

FeedbackDeviceResolution resolve_feedback_device(const std::vector<FeedbackDeviceInfo>& devices,
                                                 const std::string&                     requestedSerialNumber = {});

}} // namespace Slic3r::GUI

#endif
