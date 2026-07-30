#include "ComCommand.hpp"

#include <boost/log/trivial.hpp>
#include <nlohmann/json.hpp>

#include "slic3r/Utils/Http.hpp"

namespace Slic3r { namespace GUI {

int ComCommand::s_commandNum = ComInvalidCommandId + 1;

ComErrno ComCameraStreamCtrl::sendLanCameraStreamCtrl(const com_command_exec_data_t &data,
                                                      const std::string &action)
{
    if (data.ip == nullptr || data.serialNumber == nullptr || data.checkCode == nullptr) {
        return COM_ERROR;
    }
    // Same request shape the network library uses for the other LAN control
    // commands (see fnet_ctrlLanDevLight): a POST to /control on the printer's
    // local HTTP port, authenticated by serial number plus check code, wrapping
    // the command and its arguments in a "payload" object.
    nlohmann::json body;
    body["serialNumber"]     = data.serialNumber;
    body["checkCode"]        = data.checkCode;
    body["payload"]["cmd"]   = "streamCtrl_cmd";
    body["payload"]["args"]  = {{"action", action}};

    std::string url = std::string("http://") + data.ip + ":" + std::to_string(data.port) + "/control";

    ComErrno result = COM_ERROR;
    // Deliberately not logging the body: it carries the serial number and check
    // code, and these lines end up pasted into bug reports.
    BOOST_LOG_TRIVIAL(info) << "sendLanCameraStreamCtrl: streamCtrl_cmd action=" << action;

    Http::post(url)
        .header("Content-Type", "application/json")
        .timeout_connect(ComTimeoutLanA / 1000)
        .timeout_max(ComTimeoutLanA / 1000)
        .set_post_body(body.dump())
        .on_complete([&result](std::string reply, unsigned status) {
            if (status < 200 || status >= 300) {
                BOOST_LOG_TRIVIAL(warning) << "sendLanCameraStreamCtrl: http status " << status;
                return;
            }
            // The printer answers {"code":0,...} on success. Treat a missing or
            // unparsable code as success so a firmware that only varies its
            // reply shape does not disable the camera, since a 2xx already means
            // the command was accepted.
            try {
                nlohmann::json json = nlohmann::json::parse(reply);
                if (json.contains("code") && json["code"].is_number_integer() &&
                    json["code"].get<int>() != 0) {
                    BOOST_LOG_TRIVIAL(warning) << "sendLanCameraStreamCtrl: printer returned code "
                                               << json["code"].get<int>();
                    return;
                }
            } catch (const std::exception &) {
                BOOST_LOG_TRIVIAL(debug) << "sendLanCameraStreamCtrl: reply was not json";
            }
            result = COM_OK;
        })
        .on_error([](std::string, std::string error, unsigned status) {
            BOOST_LOG_TRIVIAL(warning) << "sendLanCameraStreamCtrl failed, status " << status
                                       << ", " << error;
        })
        .perform_sync();

    return result;
}

}} // namespace Slic3r::GUI
