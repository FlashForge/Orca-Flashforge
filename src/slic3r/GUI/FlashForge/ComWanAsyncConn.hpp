#ifndef slic3r_GUI_ComAsyncConn_hpp_
#define slic3r_GUI_ComAsyncConn_hpp_

#include <memory>
#include <boost/thread/thread.hpp>
#include <wx/event.h>
#include "FlashNetworkIntfc.h"
#include "MultiComDef.hpp"

namespace Slic3r { namespace GUI {

struct WanConnReadDataEvent : public wxCommandEvent {
    fnet_conn_read_data_t readData;
};
struct WanConnExitEvent : public wxCommandEvent {
    ComErrno ret;
};
wxDECLARE_EVENT(WAN_CONN_READ_DATA_EVENT, WanConnReadDataEvent);
wxDECLARE_EVENT(WAN_CONN_RECONNECT_EVENT, wxCommandEvent);
wxDECLARE_EVENT(WAN_CONN_EXIT_EVENT, WanConnExitEvent);

class ComWanAsyncConn : public wxEvtHandler
{
public:
    ComWanAsyncConn(fnet::FlashNetworkIntfc *networkIntfc);

    ComErrno createConn(const std::string &uid, const std::string &accessToken);

    void freeConn();

    void postSyncSlicerLogin(const std::string &uid);

    void postSyncBindDev(const std::string &uid, const std::string &devId);

    void postSyncUnbindDev(const std::string &uid,const std::string &devId);

    void postSubscribeDev(const std::vector<std::string> &devIds);

    void postSubscribeAppSlicer(const std::string &uid);

    void postTempCtrl(const std::string &devId, const fnet_temp_ctrl_t &tempCtrl);

    void postLightCtrl(const std::string &devId, const fnet_light_ctrl_t &lightCtrl);

    void postAirFilterCtrl(const std::string &devId, const fnet_air_filter_ctrl_t &airFilterCtrl);

    void postClearFanCtrl(const std::string &devId, const fnet_clear_fan_ctrl_t &clearFanCtrl);

    void postPrintCtrl(const std::string &devId, const fnet_print_ctrl_t &printCtrl);

    void postJobCtrl(const std::string &devId, const fnet_job_ctrl_t &jobCtrl);

    void postCameraStreamCtrl(const std::string &devId, const fnet_camera_stream_ctrl_t &cameraStreamCtrl);

private:
    void run();

    static int readCallback(fnet_conn_read_data_t *readData, void *data);

    static void reconnectCallback(void *data);

private:
    void                          *m_conn;
    std::unique_ptr<boost::thread> m_thread;
    fnet::FlashNetworkIntfc       *m_networkIntfc;
};

}} // namespace Slic3r::GUI

#endif
