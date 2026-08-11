#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#endif

#include <catch2/catch_all.hpp>

#include "slic3r/Utils/Http.hpp"
#include "slic3r/Utils/OrcaCloudServiceAgent.hpp"
#include "slic3r/GUI/DeviceCore/DevFilaSystem.h"

namespace {

nlohmann::json flat_session_json(const nlohmann::json& fields)
{
    nlohmann::json session = {
        {"access_token", "test-token"},
        {"user_id", "test-user-id"}
    };
    session.update(fields);
    return session;
}

nlohmann::json nested_session_json(const nlohmann::json& metadata)
{
    return {
        {"access_token", "test-token"},
        {"user", {
            {"id", "test-user-id"},
            {"user_metadata", metadata}
        }}
    };
}

std::string resolved_display_name(const nlohmann::json& session)
{
    Slic3r::OrcaCloudServiceAgent agent("");
    REQUIRE(agent.set_user_session(session, false));
    return agent.get_user_nickname();
}

} // namespace

TEST_CASE("Reported virtual trays are present non-placeholder slots", "[DeviceManager][VirtualTray]")
{
    const auto tray = Slic3r::DevAmsTray::reported_virtual(std::to_string(VIRTUAL_TRAY_MAIN_ID));

    CHECK(tray.is_exists);
    CHECK_FALSE(tray.is_slot_placeholder);
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

TEST_CASE("Orca cloud flat session resolves display name consistently", "[OrcaCloudServiceAgent]")
{
    CHECK(resolved_display_name(flat_session_json({
        {"username", "orca_username"},
        {"display_name", "Display Name"},
        {"nickname", "Nickname"}
    })) == "Display Name");

    CHECK(resolved_display_name(flat_session_json({
        {"username", "orca_username"},
        {"nickname", "Nickname"}
    })) == "Nickname");

    CHECK(resolved_display_name(flat_session_json({
        {"username", "orca_username"},
        {"full_name", "Full Name"}
    })) == "Full Name");

    CHECK(resolved_display_name(flat_session_json({
        {"username", "orca_username"},
        {"name", "Provider Name"}
    })) == "Provider Name");

    CHECK(resolved_display_name(flat_session_json({
        {"username", "orca_username"}
    })) == "orca_username");
}

TEST_CASE("Orca cloud nested session resolves display name consistently", "[OrcaCloudServiceAgent]")
{
    CHECK(resolved_display_name(nested_session_json({
        {"username", "orca_username"},
        {"display_name", "Display Name"},
        {"nickname", "Nickname"}
    })) == "Display Name");

    CHECK(resolved_display_name(nested_session_json({
        {"username", "orca_username"},
        {"nickname", "Nickname"}
    })) == "Nickname");

    CHECK(resolved_display_name(nested_session_json({
        {"username", "orca_username"},
        {"full_name", "Full Name"}
    })) == "Full Name");

    CHECK(resolved_display_name(nested_session_json({
        {"username", "orca_username"},
        {"name", "Provider Name"}
    })) == "Provider Name");

    CHECK(resolved_display_name(nested_session_json({
        {"username", "orca_username"}
    })) == "orca_username");
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

