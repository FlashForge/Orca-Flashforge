#include "ComWanConn.hpp"
#include "FreeInDestructor.h"
#include "MultiComUtils.hpp"
#include "WanDevTokenMgr.hpp"
#include <wx/thread.h>

namespace Slic3r { namespace GUI {

wxDEFINE_EVENT(WAN_CONN_STATUS_EVENT, WanConnStatusEvent);
wxDEFINE_EVENT(WAN_CONN_READ_EVENT, WanConnReadEvent);
wxDEFINE_EVENT(WAN_CONN_HTTP_UNAUTHORIZED, WanConnUnauthorizedEvent);

ComWanConn::ComWanConn()
    : m_networkIntfc(nullptr)
    , m_conn(nullptr)
    , m_threadPool(5, 30000)
{
}

ComErrno ComWanConn::createConn(fnet::FlashNetworkIntfc *networkIntfc, const char *clientId,
    uint64_t sessionGeneration, uint64_t connectionGeneration)
{
    boost::unique_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn != nullptr) {
        return COM_ERROR;
    }
    std::unique_ptr<ConnectionCallbackContext> callbackContext(new ConnectionCallbackContext {
        this, sessionGeneration, connectionGeneration });
    m_networkIntfc = networkIntfc;
    m_clientId = clientId;
    void *conn;
    fnet_conn_settings_t settings;
    settings.clientId = clientId;
    settings.statusCallback = statusCallback;
    settings.statusCallbackData = callbackContext.get();
    settings.updateCallback = updateCallback;
    settings.updateCallbackData = callbackContext.get();
    settings.readCallback = readCallback;
    settings.readCallbackData = callbackContext.get();
    ComErrno ret = MultiComUtils::fnetRet2ComErrno(networkIntfc->createConnection(&conn, &settings));
    if (ret != COM_OK) {
        return ret;
    }
    m_conn = conn;
    m_callbackContext = std::move(callbackContext);
    m_threadExitEvent.set(false);
    return COM_OK;
}

void ComWanConn::freeConnBlocking()
{
    wxASSERT_MSG(!wxIsMainThread(), "ComWanConn::freeConnBlocking must not run on the wx event thread");
    // Reject newly posted commands before stopping the native connection. A
    // task that is already inside FlashNetwork is interrupted by
    // connectionStop() below.
    m_threadExitEvent.set(true);
    boost::shared_lock<boost::shared_mutex> sharedLock(m_connMutex);
    if (m_conn == nullptr) {
        return;
    }
    BOOST_LOG_TRIVIAL(info) << "ComWanConn::freeConnBlocking begin";
    m_threadPool.clear();
    m_networkIntfc->connectionStop(m_conn);
    BOOST_LOG_TRIVIAL(info) << "ComWanConn::freeConnBlocking connectionStop finished";
    m_threadPool.wait();
    BOOST_LOG_TRIVIAL(info) << "ComWanConn::freeConnBlocking WAN tasks finished";
    sharedLock.unlock();

    boost::unique_lock<boost::shared_mutex> uniqueLock(m_connMutex);
    m_networkIntfc->freeConnection(m_conn);
    m_conn = nullptr;
    m_callbackContext.reset();
    BOOST_LOG_TRIVIAL(info) << "ComWanConn::freeConnBlocking finished";
}

void ComWanConn::syncLogin(const std::string &topic)
{
    m_threadPool.post([this, topic]() {
        boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
        if (m_conn == nullptr || m_threadExitEvent.get()) {
            return;
        }
        fnet_conn_write_data_t writeData;
        writeData.type = FNET_CONN_WRITE_SYNC_LOGIN;
        writeData.data = m_clientId.c_str();
        writeData.topic = topic.c_str();
        writeData.qos = 1;
        if (m_networkIntfc->connectionSend(m_conn, &writeData) != FNET_OK) {
            return;
        }
    });
}

void ComWanConn::updateDetail(const std::vector<std::string> &topics)
{
    m_threadPool.post([this, topics]() {
        boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
        if (m_conn == nullptr || m_threadExitEvent.get()) {
            return;
        }
        const void *dataPtr = nullptr;
        std::vector<const char *> topicPtrs(topics.size());
        for (size_t i = 0; i < topics.size(); ++i) {
            topicPtrs[i] = topics[i].c_str();
        }
        fnet_conn_write_multi_data_t writeData;
        writeData.type = FNET_CONN_WRITE_UPDATE_DETAIL;
        writeData.datas = &dataPtr;
        writeData.topics = topicPtrs.data();
        writeData.dataCnt = 1;
        writeData.topicCnt = topicPtrs.size();
        writeData.qos = 1;
        fnet_conn_write_multi_result_t *writeResult;
        if (m_networkIntfc->connectionSendMulti(m_conn, &writeData, &writeResult) != FNET_OK) {
            return;
        }
        fnet::FreeInDestructor freeWriteData(writeResult, m_networkIntfc->freeWriteMultiResult);
    });
}

void ComWanConn::subscribe(const std::vector<std::string> &topics)
{
    m_threadPool.post([this, topics]() {
        boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
        if (m_conn == nullptr || m_threadExitEvent.get() || topics.empty()) {
            return;
        }
        std::vector<const char *> topicPtrs(topics.size());
        for (size_t i = 0; i < topics.size(); ++i) {
            topicPtrs[i] = topics[i].c_str();
        }
        fnet_conn_subscribe_data subscribeData;
        subscribeData.topics = topicPtrs.data();
        subscribeData.topicCnt = topicPtrs.size();
        for (int i = 0; i < 3 && !m_threadExitEvent.get(); ++i) {
            if (m_networkIntfc->connectionSubscribe(m_conn, &subscribeData) == FNET_OK) {
                break;
            }
            m_threadExitEvent.waitTrue(3000);
        }
    });
}

void ComWanConn::unsubscribe(const std::vector<std::string> &topics)
{
    m_threadPool.post([this, topics]() {
        boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
        if (m_conn == nullptr || m_threadExitEvent.get()) {
            return;
        }
        std::vector<const char *> topicPtrs(topics.size());
        for (size_t i = 0; i < topics.size(); ++i) {
            topicPtrs[i] = topics[i].c_str();
        }
        fnet_conn_subscribe_data subscribeData;
        subscribeData.topics = topicPtrs.data();
        subscribeData.topicCnt = topicPtrs.size();
        for (int i = 0; i < 3 && !m_threadExitEvent.get(); ++i) {
            if (m_networkIntfc->connectionUnsubscribe(m_conn, &subscribeData) == FNET_OK) {
                break;
            }
            m_threadExitEvent.waitTrue(3000);
        }
    });
}

ComErrno ComWanConn::sendStartJob(const char *topic, const fnet_local_job_data_t &jobData)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr || m_threadExitEvent.get()) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_START_JOB, &jobData, topic, 1 };
    return MultiComUtils::fnetRet2ComErrno(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanConn::sendStartCloundJob(const std::vector<std::string> &topics,
    const fnet_clound_job_data_t &jobData, std::set<int> &failedIndices)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr || m_threadExitEvent.get()) {
        return COM_ERROR;
    }
    std::vector<fnet_clound_job_data_t> jobDatas(topics.size(), jobData);
    std::vector<const void *> jobDataPtrs(topics.size());
    std::vector<const char *> topicPtrs(topics.size());
    for (size_t i = 0; i < topics.size(); ++i) {
        jobDatas[i].devIds = &jobData.devIds[i];
        jobDatas[i].devSerialNumbers = &jobData.devSerialNumbers[i];
        jobDatas[i].jobIds = &jobData.jobIds[i];
        jobDatas[i].devCnt = 1;
        jobDataPtrs[i] = &jobDatas[i];
        topicPtrs[i] = topics[i].c_str();
    }
    fnet_conn_write_multi_data_t writeData;
    writeData.type = FNET_CONN_WRITE_START_CLOUND_JOB;
    writeData.datas = jobDataPtrs.data();
    writeData.topics = topicPtrs.data();
    writeData.dataCnt = jobDataPtrs.size();
    writeData.topicCnt = topicPtrs.size();
    writeData.qos = 1;
    fnet_conn_write_multi_result_t *writeResult;
    if (m_networkIntfc->connectionSendMulti(m_conn, &writeData, &writeResult) != FNET_OK) {
        return COM_ERROR;
    }
    fnet::FreeInDestructor freeWriteData(writeResult, m_networkIntfc->freeWriteMultiResult);
    for (int i = 0; i < writeResult->failedCnt; ++i) {
        failedIndices.emplace(writeResult->failedIndices[i]);
    }
    return COM_OK;
}

ComErrno ComWanConn::sendTempCtrl(const char *topic, const fnet_temp_ctrl_t &tempCtrl)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr || m_threadExitEvent.get()) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_TEMP_CTRL, &tempCtrl, topic, 1 };
    return MultiComUtils::fnetRet2ComErrno(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanConn::sendLightCtrl(const char *topic, const fnet_light_ctrl_t &lightCtrl)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr || m_threadExitEvent.get()) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_LIGHT_CTRL, &lightCtrl, topic, 1 };
    return MultiComUtils::fnetRet2ComErrno(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanConn::sendAirFilterCtrl(const char *topic, const fnet_air_filter_ctrl_t &airFilterCtrl)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr || m_threadExitEvent.get()) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_AIR_FILTER_CTRL, &airFilterCtrl, topic, 1 };
    return MultiComUtils::fnetRet2ComErrno(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanConn::sendClearFanCtrl(const char *topic, const fnet_clear_fan_ctrl_t &clearFanCtrl)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr || m_threadExitEvent.get()) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_CLEAR_FAN_CTRL, &clearFanCtrl, topic, 1 };
    return MultiComUtils::fnetRet2ComErrno(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanConn::sendMoveCtrl(const char *topic, const fnet_move_ctrl_t &moveCtrl)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr || m_threadExitEvent.get()) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_MOVE_CTRL, &moveCtrl, topic, 1 };
    return MultiComUtils::fnetRet2ComErrno(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanConn::sendExtrudeCtrl(const char *topic, const fnet_extrude_ctrl_t &extrudeCtrl)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr || m_threadExitEvent.get()) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_EXTRUDE_CTRL, &extrudeCtrl, topic, 1 };
    return MultiComUtils::fnetRet2ComErrno(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanConn::sendHomingCtrl(const char *topic)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr || m_threadExitEvent.get()) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_HOMING_CTRL, nullptr, topic, 1 };
    return MultiComUtils::fnetRet2ComErrno(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanConn::sendMatlStationCtrl(const char *topic, const fnet_matl_station_ctrl_t &matlStationCtrl)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr || m_threadExitEvent.get()) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_MATL_STATION_CTRL, &matlStationCtrl, topic, 1 };
    return MultiComUtils::fnetRet2ComErrno(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanConn::sendIndepMatlCtrl(const char *topic, const fnet_indep_matl_ctrl_t &indepMatlCtrl)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr || m_threadExitEvent.get()) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_INDEP_MATL_CTRL, &indepMatlCtrl, topic, 1 };
    return MultiComUtils::fnetRet2ComErrno(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanConn::sendPrintCtrl(const char *topic, const fnet_print_ctrl_t &printCtrl)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr || m_threadExitEvent.get()) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_PRINT_CTRL, &printCtrl, topic, 1 };
    return MultiComUtils::fnetRet2ComErrno(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanConn::sendJobCtrl(const char *topic, const fnet_job_ctrl_t &jobCtrl)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr || m_threadExitEvent.get()) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_JOB_CTRL, &jobCtrl, topic, 1 };
    return MultiComUtils::fnetRet2ComErrno(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanConn::sendStateCtrl(const char *topic, const fnet_state_ctrl_t &stateCtrl)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr || m_threadExitEvent.get()) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_STATE_CTRL, &stateCtrl, topic, 1 };
    return MultiComUtils::fnetRet2ComErrno(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanConn::sendErrorCodeCtrl(const char *topic, const fnet_error_code_ctrl_t &errorCodeCtrl)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr || m_threadExitEvent.get()) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_ERROR_CODE_CTRL, &errorCodeCtrl, topic, 1 };
    return MultiComUtils::fnetRet2ComErrno(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanConn::sendPlateDetectCtrl(const char *topic, const fnet_plate_detect_ctrl &plateDetectCtrl)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr || m_threadExitEvent.get()) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_PLATE_DETECT_CTRL, &plateDetectCtrl, topic, 1 };
    return MultiComUtils::fnetRet2ComErrno(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanConn::sendFirstLayerDetectCtrl(const char *topic,
    const fnet_first_layer_detect_ctrl_t &firstLayerDetectCtrl)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr || m_threadExitEvent.get()) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_FIRST_LAYER_DETECT_CTRL, &firstLayerDetectCtrl, topic, 1 };
    return MultiComUtils::fnetRet2ComErrno(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanConn::sendCameraStreamCtrl(const char *topic, const fnet_camera_stream_ctrl_t &cameraStreamCtrl)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr || m_threadExitEvent.get()) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_CAMERA_STREAM_CTRL, &cameraStreamCtrl, topic, 1 };
    return MultiComUtils::fnetRet2ComErrno(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanConn::sendMatlStationConfig(const char *topic, const fnet_matl_station_config_t &matlStationConfig)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr || m_threadExitEvent.get()) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_MATL_STATION_CONFIG, &matlStationConfig, topic, 1 };
    return MultiComUtils::fnetRet2ComErrno(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanConn::sendIndepMatlConfig(const char *topic, const fnet_indep_matl_config_t &indepMatlConfig)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr || m_threadExitEvent.get()) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_INDEP_MATL_CONFIG, &indepMatlConfig, topic, 1 };
    return MultiComUtils::fnetRet2ComErrno(m_networkIntfc->connectionSend(m_conn, &writeData));
}

void ComWanConn::statusCallback(fnet_conn_status_t status, void *data)
{
    ConnectionCallbackContext *context = static_cast<ConnectionCallbackContext *>(data);
    WanConnStatusEvent *event = new WanConnStatusEvent;
    event->SetEventType(WAN_CONN_STATUS_EVENT);
    event->status = status;
    event->sessionGeneration = context->sessionGeneration;
    event->connectionGeneration = context->connectionGeneration;
    context->owner->QueueEvent(event);
}

int ComWanConn::updateCallback(const char **clientId, void *data)
{
    ConnectionCallbackContext *context = static_cast<ConnectionCallbackContext *>(data);
    ComWanConn *self = context->owner;
    ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
    com_mqtt_config_t mqttConfig;
    ComErrno ret = MultiComUtils::getMqttConfig(self->m_clientId, token.accessToken(), mqttConfig, ComTimeoutWanB);
    if (ret == COM_UNAUTHORIZED) {
        WanConnUnauthorizedEvent *event = new WanConnUnauthorizedEvent;
        event->SetEventType(WAN_CONN_HTTP_UNAUTHORIZED);
        event->sessionGeneration = context->sessionGeneration;
        event->connectionGeneration = context->connectionGeneration;
        self->QueueEvent(event);
        return 1;
    } else if (ret != COM_OK) {
        return 1;
    }
    *clientId = self->m_networkIntfc->allocString(self->m_clientId.c_str(), self->m_clientId.size());
    if (*clientId == nullptr) {
        return 1;
    }
    return 0;
}

void ComWanConn::readCallback(fnet_conn_read_data_t *readData, void *data)
{
    ConnectionCallbackContext *context = static_cast<ConnectionCallbackContext *>(data);
    WanConnReadEvent *event = new WanConnReadEvent;
    event->SetEventType(WAN_CONN_READ_EVENT);
    event->readData = *readData;
    event->sessionGeneration = context->sessionGeneration;
    event->connectionGeneration = context->connectionGeneration;
    context->owner->QueueEvent(event);
}

}} // namespace Slic3r::GUI
