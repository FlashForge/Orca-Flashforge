#ifndef slic3r_GUI_ComWanNimConn_hpp_
#define slic3r_GUI_ComWanNimConn_hpp_

#include <string>
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
};
struct WanConnReadEvent : public wxCommandEvent {
    fnet_conn_read_data_t readData;
};
struct WanConnSubscribeEvent : public wxCommandEvent {
    const char *nimAccountId;
    unsigned int status;
};
wxDECLARE_EVENT(WAN_CONN_STATUS_EVENT, WanConnStatusEvent);
wxDECLARE_EVENT(WAN_CONN_READ_EVENT, WanConnReadEvent);
wxDECLARE_EVENT(WAN_CONN_SUBSCRIBE_EVENT, WanConnSubscribeEvent);

class ComWanNimConn : public wxEvtHandler, public Singleton<ComWanNimConn>
{
public:
    ComWanNimConn();

    void initalize(fnet::FlashNetworkIntfc *networkIntfc, const char *nimAppDir);

    void uninitalize();

    ComErrno createConn(const char *nimDataId);

    void freeConn();

    void syncBindDev(const std::string &nimAccountId, const std::string &devId);

    void syncUnbindDev(const std::string &nimAccountId, const std::string &devId);

    void syncDevUnregister(const std::string &nimAccountId);

    void updateDetail(const std::vector<std::string> &nimAcctountIds, const std::string &nimTeamId);

    void subscribeDevStatus(const std::vector<std::string> &nimAccountIds, int duration);

    void unsubscribeDevStatus(const std::vector<std::string> &nimAccountIds);

    ComErrno sendStartJob(const char *nimAccountId, const fnet_local_job_data_t &jobData);

    ComErrno sendStartCloundJob(int sendTeam, const char *nimId, const fnet_clound_job_data_t &jobData);

    ComErrno sendTempCtrl(const char *nimAccountId, const fnet_temp_ctrl_t &tempCtrl);

    ComErrno sendLightCtrl(const char *nimAccountId, const fnet_light_ctrl_t &lightCtrl);

    ComErrno sendAirFilterCtrl(const char *nimAccountId,
        const fnet_air_filter_ctrl_t &airFilterCtrl);

    ComErrno sendClearFanCtrl(const char *nimAccountId, const fnet_clear_fan_ctrl_t &clearFanCtrl);

    ComErrno sendMatlStationCtrl(const char *nimAccountId,
        const fnet_matl_station_ctrl_t &matlStationCtrl);

    ComErrno sendIndepMatlCtrl(const char *nimAccountId,
        const fnet_indep_matl_ctrl_t &indepMatlCtrl);

    ComErrno sendPrintCtrl(const char *nimAccountId, const fnet_print_ctrl_t &printCtrl);

    ComErrno sendJobCtrl(const char *nimAccountId, const fnet_job_ctrl_t &jobCtrl);

    ComErrno sendStateCtrl(const char *nimAccountId, const fnet_state_ctrl_t &stateCtrl);

    ComErrno sendCameraStreamCtrl(const char *nimAccountId,
        const fnet_camera_stream_ctrl_t &cameraStreamCtrl);

    ComErrno sendMatlStationConfig(const char *nimAccountId,
        const fnet_matl_station_config_t &matlStationConfig);

    ComErrno sendIndepMatlConfig(const char *nimAccountId,
        const fnet_indep_matl_config_t &indepMatlConfig);

private:
    static void statusCallback(fnet_conn_status_t status, void *data);

    static void readCallback(fnet_conn_read_data_t *readData, void *data);

    static void subscribeCallback(const char *nimAccountId, unsigned int status, void *data);

private:
    fnet::FlashNetworkIntfc *m_networkIntfc;
    std::string              m_nimAppDir;
    bool                     m_isInitalizeNim;
    void                    *m_conn;
    boost::shared_mutex      m_connMutex;
    ComThreadPool            m_threadPool;
    WaitEvent                m_threadExitEvent;
};

}} // namespace Slic3r::GUI

#endif
