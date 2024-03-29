#include "ComWanAsyncConn.hpp"
#include "MultiComEvent.hpp"
#include "MultiComUtils.hpp"

namespace Slic3r { namespace GUI {

wxDEFINE_EVENT(WAN_CONN_READ_DATA_EVENT, WanConnReadDataEvent);
wxDEFINE_EVENT(WAN_CONN_RECONNECT_EVENT, wxCommandEvent);

ComWanAsyncConn::ComWanAsyncConn(fnet::FlashNetworkIntfc *networkIntfc)
    : m_conn(nullptr)
    , m_networkIntfc(networkIntfc)
{
}

ComErrno ComWanAsyncConn::createConn(const std::string &uid, const std::string &accessToken)
{
    fnet_conn_settings_t settings;
    settings.readCallback = readCallback;
    settings.readCallbackData = this;
    settings.reconnectCallback = reconnectCallback;
    settings.reconnectCallbackData = this;
    settings.maxReconnectCnt = 5;
    settings.maxErrorCnt = 3;
    settings.msTimeout = ComTimeoutWan;
    int fnetRet = m_networkIntfc->createConnection(&m_conn, uid.c_str(), accessToken.c_str(), &settings);
    ComErrno ret = MultiComUtils::fnetRet2ComErrno(fnetRet);
    if (ret != COM_OK) {
        return ret;
    }
    m_thread.reset(new boost::thread(boost::bind(&ComWanAsyncConn::run, this)));
    return ret;
}

void ComWanAsyncConn::freeConn()
{
    if (m_thread == nullptr) {
        return;
    }
    m_networkIntfc->connectionStop(m_conn);
    m_thread->join();
    m_thread.reset(nullptr);
    m_networkIntfc->freeConnection(m_conn);
}

void ComWanAsyncConn::postSyncSlicerLogin(const std::string &uid)
{
    if (m_thread == nullptr) {
        return;
    }
    fnet_user_id_t fnetUserId = {uid.c_str()};
    fnet_conn_write_data_t writeData = {FNET_CONN_WRITE_SYNC_SLICER_LOGIN, &fnetUserId, {nullptr, 0}};
    m_networkIntfc->connectionPost(m_conn, &writeData);
}

void ComWanAsyncConn::postSyncBindDev(const std::string &uid, const std::string &devId)
{
    if (m_thread == nullptr) {
        return;
    }
    const char *ids = devId.c_str();
    fnet_user_id_t fnetUserId = {uid.c_str()};
    fnet_conn_write_data_t writeData = {FNET_CONN_WRITE_SYNC_BIND_DEVICE, &fnetUserId, {&ids, 1}};
    m_networkIntfc->connectionPost(m_conn, &writeData);
}

void ComWanAsyncConn::postSyncUnbindDev(const std::string &uid,const std::string &devId)
{
    if (m_thread == nullptr) {
        return;
    }
    const char *ids = devId.c_str();
    fnet_user_id_t fnetUserId = {uid.c_str()};
    fnet_conn_write_data_t writeData = {FNET_CONN_WRITE_SYNC_UNBIND_DEVICE, &fnetUserId, {&ids, 1}};
    m_networkIntfc->connectionPost(m_conn, &writeData);
}

void ComWanAsyncConn::postSubscribeAppSlicer(const std::string &uid)
{
    if (m_thread == nullptr) {
        return;
    }
    fnet_user_id_t fnetUserId = {uid.c_str()};
    fnet_conn_write_data_t writeData  = {FNET_CONN_WRITE_SUB_APP_SLICER_SYNC, &fnetUserId, {nullptr, 0}};
    m_networkIntfc->connectionPost(m_conn, &writeData);
}

void ComWanAsyncConn::postSubscribeDev(const std::vector<std::string> &devIds)
{
    if (m_thread == nullptr || devIds.empty()) {
        return;
    }
    std::vector<const char *> ids;
    for (auto &devId : devIds) {
        ids.push_back(devId.c_str());
    }
    fnet_conn_write_data_t writeData = {FNET_CONN_WRITE_SUB_DEVICE_ACTION, nullptr, {ids.data(), (int)ids.size()}};
    m_networkIntfc->connectionPost(m_conn, &writeData);
}

void ComWanAsyncConn::postTempCtrl(const std::string &devId, const fnet_temp_ctrl_t &tempCtrl)
{
    if (m_thread == nullptr) {
        return;
    }
    const char *ids = devId.c_str();
    fnet_conn_write_data_t writeData = {FNET_CONN_WRITE_TEMP_CTRL, &tempCtrl, {&ids, 1}};
    m_networkIntfc->connectionPost(m_conn, &writeData);
}

void ComWanAsyncConn::postLightCtrl(const std::string &devId, const fnet_light_ctrl_t &lightCtrl)
{
    if (m_thread == nullptr) {
        return;
    }
    const char *ids = devId.c_str();
    fnet_conn_write_data_t writeData = {FNET_CONN_WRITE_LIGHT_CTRL, &lightCtrl, {&ids, 1}};
    m_networkIntfc->connectionPost(m_conn, &writeData);
}

void ComWanAsyncConn::postAirFilterCtrl(const std::string &devId,
    const fnet_air_filter_ctrl_t &airFilterCtrl)
{
    if (m_thread == nullptr) {
        return;
    }
    const char *ids = devId.c_str();
    fnet_conn_write_data_t writeData = {FNET_CONN_WRITE_AIR_FILTER_CTRL, &airFilterCtrl, {&ids, 1}};
    m_networkIntfc->connectionPost(m_conn, &writeData);
}

void ComWanAsyncConn::postPrintCtrl(const std::string &devId, const fnet_print_ctrl_t &printCtrl)
{
    if (m_thread == nullptr) {
        return;
    }
    const char *ids = devId.c_str();
    fnet_conn_write_data_t writeData = {FNET_CONN_WRITE_PRINT_CTRL, &printCtrl, {&ids, 1}};
    m_networkIntfc->connectionPost(m_conn, &writeData);
}

void ComWanAsyncConn::postJobCtrl(const std::string &devId, const fnet_job_ctrl_t &jobCtrl)
{
    if (m_thread == nullptr) {
        return;
    }
    const char *ids = devId.c_str();
    fnet_conn_write_data_t writeData = {FNET_CONN_WRITE_JOB_CTRL, &jobCtrl, {&ids, 1}};
    m_networkIntfc->connectionPost(m_conn, &writeData);
}

void ComWanAsyncConn::postCameraStreamCtrl(const std::string &devId,
    const fnet_camera_stream_ctrl_t &cameraStreamCtrl)
{
    if (m_thread == nullptr) {
        return;
    }
    const char *ids = devId.c_str();
    fnet_conn_write_data_t writeData = {FNET_CONN_WRITE_CAMERA_STREAM_CTRL, &cameraStreamCtrl, {&ids, 1}};
    m_networkIntfc->connectionPost(m_conn, &writeData);
}

void ComWanAsyncConn::run()
{
    ComErrno ret = MultiComUtils::fnetRet2ComErrno(m_networkIntfc->connectionRun(m_conn));
    if (ret != COM_OK) {
        QueueEvent(new ComWanDevMaintainEvent(COM_WAN_DEV_MAINTAIN_EVENT, ret));
    }
}

int ComWanAsyncConn::readCallback(fnet_conn_read_data_t *readData, void *data)
{
    WanConnReadDataEvent *event = new WanConnReadDataEvent;
    event->SetEventType(WAN_CONN_READ_DATA_EVENT);
    event->readData = *readData;
    ((ComWanAsyncConn *)data)->QueueEvent(event);
    return 0;
}

void ComWanAsyncConn::reconnectCallback(void *data)
{
    ((ComWanAsyncConn *)data)->QueueEvent(new wxCommandEvent(WAN_CONN_RECONNECT_EVENT));
}

}} // namespace Slic3r::GUI
