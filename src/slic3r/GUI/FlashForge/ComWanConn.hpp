#ifndef slic3r_GUI_ComWanConn_hpp_
#define slic3r_GUI_ComWanConn_hpp_

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <boost/thread/thread.hpp>
#include <wx/event.h>
#include "ComThreadPool.hpp"
#include "FlashNetworkIntfc.h"
#include "MultiComDef.hpp"
#include "Singleton.hpp"
#include "WaitEvent.hpp"

namespace Slic3r { namespace GUI {

struct WanConnStatusEvent : public wxCommandEvent {
    fnet_conn_status_t status;
    uint64_t sessionGeneration { 0 };
    uint64_t connectionGeneration { 0 };
};
struct WanConnReadEvent : public wxCommandEvent {
    fnet_conn_read_data_t readData;
    uint64_t sessionGeneration { 0 };
    uint64_t connectionGeneration { 0 };
};
struct WanConnUnauthorizedEvent : public wxCommandEvent {
    uint64_t sessionGeneration { 0 };
    uint64_t connectionGeneration { 0 };
};
wxDECLARE_EVENT(WAN_CONN_STATUS_EVENT, WanConnStatusEvent);
wxDECLARE_EVENT(WAN_CONN_READ_EVENT, WanConnReadEvent);
wxDECLARE_EVENT(WAN_CONN_HTTP_UNAUTHORIZED, WanConnUnauthorizedEvent);

class ComWanConn : public wxEvtHandler, public Singleton<ComWanConn>
{
public:
    ComWanConn();

    ComErrno createConn(fnet::FlashNetworkIntfc *networkIntfc, const char *clientId,
        uint64_t sessionGeneration, uint64_t connectionGeneration);

    // Stops the native connection and waits for all outstanding WAN tasks.
    // This may block in third-party network code and must never run on the wx
    // event thread.
    void freeConnBlocking();

    void syncLogin(const std::string &topic);

    void updateDetail(const std::vector<std::string> &topics);

    void subscribe(const std::vector<std::string> &topics);

    void unsubscribe(const std::vector<std::string> &topics);

    ComErrno sendStartJob(const char *topic, const fnet_local_job_data_t &jobData);

    ComErrno sendStartCloundJob(const std::vector<std::string> &topics,
        const fnet_clound_job_data_t &jobData, std::set<int> &failedIndices);

    ComErrno sendTempCtrl(const char *topic, const fnet_temp_ctrl_t &tempCtrl);

    ComErrno sendLightCtrl(const char *topic, const fnet_light_ctrl_t &lightCtrl);

    ComErrno sendAirFilterCtrl(const char *topic, const fnet_air_filter_ctrl_t &airFilterCtrl);

    ComErrno sendClearFanCtrl(const char *topic, const fnet_clear_fan_ctrl_t &clearFanCtrl);

    ComErrno sendMoveCtrl(const char *topic, const fnet_move_ctrl_t &moveCtrl);

    ComErrno sendExtrudeCtrl(const char *topic, const fnet_extrude_ctrl_t &extrudeCtrl);

    ComErrno sendHomingCtrl(const char *topic);

    ComErrno sendMatlStationCtrl(const char *topic, const fnet_matl_station_ctrl_t &matlStationCtrl);

    ComErrno sendIndepMatlCtrl(const char *topic, const fnet_indep_matl_ctrl_t &indepMatlCtrl);

    ComErrno sendPrintCtrl(const char *topic, const fnet_print_ctrl_t &printCtrl);

    ComErrno sendJobCtrl(const char *topic, const fnet_job_ctrl_t &jobCtrl);

    ComErrno sendStateCtrl(const char *topic, const fnet_state_ctrl_t &stateCtrl);

    ComErrno sendErrorCodeCtrl(const char *topic, const fnet_error_code_ctrl_t &errorCodeCtrl);

    ComErrno sendPlateDetectCtrl(const char *topic, const fnet_plate_detect_ctrl &plateDetectCtrl);

    ComErrno sendFirstLayerDetectCtrl(const char *topic,
        const fnet_first_layer_detect_ctrl_t &firstLayerDetectCtrl);

    ComErrno sendCameraStreamCtrl(const char *topic, const fnet_camera_stream_ctrl_t &cameraStreamCtrl);

    ComErrno sendMatlStationConfig(const char *topic, const fnet_matl_station_config_t &matlStationConfig);

    ComErrno sendIndepMatlConfig(const char *topic, const fnet_indep_matl_config_t &indepMatlConfig);

private:
    struct ConnectionCallbackContext {
        ComWanConn *owner;
        uint64_t sessionGeneration;
        uint64_t connectionGeneration;
    };

    static void statusCallback(fnet_conn_status_t status, void *data);

    static int updateCallback(const char **clientId, void *data);

    static void readCallback(fnet_conn_read_data_t *readData, void *data);

private:
    fnet::FlashNetworkIntfc *m_networkIntfc;
    std::string              m_clientId;
    void                    *m_conn;
    std::unique_ptr<ConnectionCallbackContext> m_callbackContext;
    boost::shared_mutex      m_connMutex;
    ComThreadPool            m_threadPool;
    WaitEvent                m_threadExitEvent;
};

}} // namespace Slic3r::GUI

#endif
