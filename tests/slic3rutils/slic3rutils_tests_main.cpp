#include <catch2/catch_all.hpp>

#include "slic3r/Utils/Http.hpp"
#include "slic3r/GUI/FlashForge/FeedbackDeviceSelection.hpp"

TEST_CASE("Feedback device selection policy", "[FeedbackLog]")
{
    using Slic3r::GUI::FeedbackDeviceInfo;
    using Slic3r::GUI::FeedbackDeviceState;
    using Slic3r::GUI::resolve_feedback_device;

    SECTION("No bound device")
    {
        const auto resolution = resolve_feedback_device({});
        REQUIRE(resolution.state == FeedbackDeviceState::NoDevice);
        REQUIRE_FALSE(resolution.device.has_value());
    }

    SECTION("The only online device is selected automatically")
    {
        const std::vector<FeedbackDeviceInfo> devices    = {{"offline-sn", "Offline", 1, false}, {"online-sn", "Online", 2, true}};
        const auto                            resolution = resolve_feedback_device(devices);
        REQUIRE(resolution.state == FeedbackDeviceState::AutoSelected);
        REQUIRE(resolution.device.has_value());
        REQUIRE(resolution.device->serialNumber == "online-sn");
    }

    SECTION("Multiple online devices require an explicit selection")
    {
        const std::vector<FeedbackDeviceInfo> devices    = {{"first-sn", "First", 1, true}, {"second-sn", "Second", 2, true}};
        const auto                            resolution = resolve_feedback_device(devices);
        REQUIRE(resolution.state == FeedbackDeviceState::SelectionRequired);
        REQUIRE_FALSE(resolution.device.has_value());
    }

    SECTION("An explicitly selected offline device remains identifiable")
    {
        const std::vector<FeedbackDeviceInfo> devices    = {{"online-sn", "Online", 1, true}, {"offline-sn", "Offline", 2, false}};
        const auto                            resolution = resolve_feedback_device(devices, "offline-sn");
        REQUIRE(resolution.state == FeedbackDeviceState::Offline);
        REQUIRE(resolution.device.has_value());
        REQUIRE(resolution.device->serialNumber == "offline-sn");
    }

    SECTION("Multiple offline devices resolve to a feedback-level offline state")
    {
        const std::vector<FeedbackDeviceInfo> devices    = {{"first-sn", "First", 1, false}, {"second-sn", "Second", 2, false}};
        const auto                            resolution = resolve_feedback_device(devices);
        REQUIRE(resolution.state == FeedbackDeviceState::Offline);
        REQUIRE(resolution.boundDeviceCount == 2);
        REQUIRE(resolution.onlineDeviceCount == 0);
        REQUIRE_FALSE(resolution.device.has_value());
    }

    SECTION("A stale selection never falls back to another online device")
    {
        const std::vector<FeedbackDeviceInfo> devices    = {{"other-sn", "Other", 1, true}};
        const auto                            resolution = resolve_feedback_device(devices, "removed-sn");
        REQUIRE(resolution.state == FeedbackDeviceState::SelectionRequired);
        REQUIRE_FALSE(resolution.device.has_value());
    }
}

TEST_CASE("Check SSL certificates paths", "[Http][NotWorking]") {
    
    Slic3r::Http g = Slic3r::Http::get("https://github.com/");
    
    unsigned status = 0;
    g.on_error([&status](std::string, std::string, unsigned http_status) {
        status = http_status;
    });
    
    g.on_complete([&status](std::string /* body */, unsigned http_status){
        status = http_status;
    });
    
    g.perform_sync();
    
    REQUIRE(status == 200);
}

TEST_CASE("Http digest authentication", "[Http][NotWorking]") {
    Slic3r::Http g = Slic3r::Http::get("https://httpbingo.org/digest-auth/auth/guest/guest");

    g.auth_digest("guest", "guest");

    unsigned status = 0;
    g.on_error([&status](std::string, std::string, unsigned http_status) {
        status = http_status;
    });

    g.on_complete([&status](std::string /* body */, unsigned http_status){
        status = http_status;
    });

    g.perform_sync();

    REQUIRE(status == 200);
}

TEST_CASE("Http basic authentication", "[Http][NotWorking]") {
    Slic3r::Http g = Slic3r::Http::get("https://httpbingo.org/basic-auth/guest/guest");

    g.auth_basic("guest", "guest");

    unsigned status = 0;
    g.on_error([&status](std::string, std::string, unsigned http_status) {
        status = http_status;
    });

    g.on_complete([&status](std::string /* body */, unsigned http_status){
        status = http_status;
    });

    g.perform_sync();

    REQUIRE(status == 200);
}

