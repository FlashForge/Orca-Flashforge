#include "ComWanNimConn.hpp"
#include "MultiComUtils.hpp"

namespace Slic3r { namespace GUI {

wxDEFINE_EVENT(WAN_CONN_STATUS_EVENT, WanConnStatusEvent);
wxDEFINE_EVENT(WAN_CONN_READ_EVENT, WanConnReadEvent);
wxDEFINE_EVENT(WAN_CONN_SUBSCRIBE_EVENT, WanConnSubscribeEvent);
wxDEFINE_EVENT(WAN_CONN_NIM_DATA_BASE_ERROR_EVENT, wxCommandEvent);

ComWanNimConn::ComWanNimConn()
    : m_networkIntfc(nullptr)
    , m_isInitalizeNim(false)
    , m_conn(nullptr)
    , m_threadPool(5, 30000)
{
}

void ComWanNimConn::initalize(fnet::FlashNetworkIntfc *networkIntfc, const char *nimAppDir)
{
    m_networkIntfc = networkIntfc;
    m_nimAppDir = nimAppDir;
}

void ComWanNimConn::uninitalize()
{
    if (m_isInitalizeNim) {
        m_networkIntfc->uninitlizeNim();
        m_isInitalizeNim = false;
    }
}

ComErrno ComWanNimConn::createConn(const char *nimDataId)
{
    if (m_networkIntfc == nullptr) {
        return COM_ERROR;
    }
    if (!m_isInitalizeNim) {
        ComErrno ret = checkAndConvertError(m_networkIntfc->initlizeNim(nimDataId, m_nimAppDir.c_str()));
        if (ret != COM_OK) {
            return ret;
        }
        m_isInitalizeNim = true;
    }
    m_threadExitEvent.set(false);
    void *conn;
    fnet_conn_settings_t settings;
    settings.nimDataId = nimDataId;
    settings.statusCallback = statusCallback;
    settings.statusCallbackData = this;
    settings.readCallback = readCallback;
    settings.readCallbackData = this;
    settings.subscribeCallback = subscribeCallback;
    settings.subscribeCallbackData = this;
    ComErrno ret = checkAndConvertError(m_networkIntfc->createConnection(&conn, &settings));
    if (ret != COM_OK) {
        return ret;
    }
    boost::unique_lock<boost::shared_mutex> lock(m_connMutex);
    m_conn = conn;
    return COM_OK;
}

void ComWanNimConn::freeConn()
{
    boost::unique_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn != nullptr) {
        m_threadExitEvent.set(true);
        m_threadPool.clear();
        m_threadPool.wait();
        m_networkIntfc->freeConnection(m_conn);
        m_conn = nullptr;
    }
}

void ComWanNimConn::syncBindDev(const std::string &nimAccountId, const std::string &devId)
{
    m_threadPool.post([this, nimAccountId, devId]() {
        boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
        if (m_conn == nullptr) {
            return;
        }
        fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_SYNC_BIND_DEVICE, devId.c_str() };
        writeData.sendTeam = 0;
        writeData.nimId = nimAccountId.c_str();
        for (int i = 0; i < 3 && !m_threadExitEvent.get(); ++i) {
            if (checkError(m_networkIntfc->connectionSend(m_conn, &writeData))) {
                break;
            }
            m_threadExitEvent.waitTrue(3000);
        }
    });
}

void ComWanNimConn::syncUnbindDev(const std::string &nimAccountId, const std::string &devId)
{
    m_threadPool.post([this, nimAccountId, devId]() {
        boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
        if (m_conn == nullptr) {
            return;
        }
        fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_SYNC_UNBIND_DEVICE, devId.c_str() };
        writeData.sendTeam = 0;
        writeData.nimId = nimAccountId.c_str();
        for (int i = 0; i < 3 && !m_threadExitEvent.get(); ++i) {
            if (checkError(m_networkIntfc->connectionSend(m_conn, &writeData))) {
                break;
            }
            m_threadExitEvent.waitTrue(3000);
        }
    });
}

void ComWanNimConn::syncDevUnregister(const std::string &nimAccountId)
{
    m_threadPool.post([this, nimAccountId]() {
        boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
        if (m_conn == nullptr) {
            return;
        }
        fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_SYNC_DEVICE_UNREGISTER, nullptr };
        writeData.sendTeam = 0;
        writeData.nimId = nimAccountId.c_str();
        for (int i = 0; i < 3 && !m_threadExitEvent.get(); ++i) {
            if (checkError(m_networkIntfc->connectionSend(m_conn, &writeData))) {
                break;
            }
            m_threadExitEvent.waitTrue(3000);
        }
    });
}

void ComWanNimConn::updateDetail(const std::vector<std::string> &nimAccountIds, const std::string &nimTeamId)
{
    m_threadPool.post([this, nimAccountIds, nimTeamId]() {
        boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
        if (m_conn == nullptr) {
            return;
        }
        fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_UPDATE_DETAIL, nullptr };
        if (nimAccountIds.size() == 1) {
            writeData.sendTeam = 0;
            writeData.nimId = nimAccountIds[0].c_str();
        } else {
            writeData.sendTeam = 1;
            writeData.nimId = nimTeamId.c_str();
        }
        for (int i = 0; i < 3 && !m_threadExitEvent.get(); ++i) {
            if (checkError(m_networkIntfc->connectionSend(m_conn, &writeData))) {
                break;
            }
            m_threadExitEvent.waitTrue(3000);
        }
    });
}

void ComWanNimConn::subscribeDevStatus(const std::vector<std::string> &nimAccountIds, int duration)
{
    m_threadPool.post([this, nimAccountIds, duration]() {
        boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
        if (m_conn == nullptr) {
            return;
        }
        std::vector<const char *> nimAccountIdPtrs;
        for (size_t i = 0; i < nimAccountIds.size(); i += 100) {
            for (size_t j = 0; j < 100 && i + j < nimAccountIds.size(); ++j) {
                if (!nimAccountIds[i + j].empty()) {
                    nimAccountIdPtrs.push_back(nimAccountIds[i + j].c_str());
                }
            }
            fnet_conn_subscribe_data_t subscribeData;
            subscribeData.nimAccountIds = nimAccountIdPtrs.data();
            subscribeData.accountCnt = nimAccountIdPtrs.size();
            subscribeData.duration = duration;
            subscribeData.immediateSync = 1;
            for (int j = 0; j < 3 && !m_threadExitEvent.get(); ++j) {
                if (checkError(m_networkIntfc->connectionSubscribe(m_conn, &subscribeData))) {
                    break;
                }
                m_threadExitEvent.waitTrue(3000);
            }
            nimAccountIdPtrs.clear();
        }
    });
}

void ComWanNimConn::unsubscribeDevStatus(const std::vector<std::string> &nimAccountIds)
{
    m_threadPool.post([this, nimAccountIds]() {
        boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
        if (m_conn == nullptr) {
            return;
        }
        std::vector<const char *> nimAccountIdPtrs;
        for (size_t i = 0; i < nimAccountIds.size(); i += 100) {
            for (size_t j = 0; j < 100 && i + j < nimAccountIds.size(); ++j) {
                if (!nimAccountIds[i + j].empty()) {
                    nimAccountIdPtrs.push_back(nimAccountIds[i + j].c_str());
                }
            }
            fnet_conn_unsubscribe_data_t unsubscribeData;
            unsubscribeData.nimAccountIds = nimAccountIdPtrs.data();
            unsubscribeData.accountCnt = nimAccountIdPtrs.size();
            for (int j = 0; j < 3 && !m_threadExitEvent.get(); ++j) {
                if (checkError(m_networkIntfc->connectionUnsubscribe(m_conn, &unsubscribeData))) {
                    break;
                }
                m_threadExitEvent.waitTrue(3000);
            }
            nimAccountIdPtrs.clear();
        }
    });
}

ComErrno ComWanNimConn::sendStartJob(const char *nimAccountId, const fnet_local_job_data_t &jobData)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_START_JOB, &jobData };
    writeData.sendTeam = 0;
    writeData.nimId = nimAccountId;
    return checkAndConvertError(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanNimConn::sendStartCloundJob(int sendTeam, const char *nimId, const fnet_clound_job_data_t &jobData)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_START_CLOUND_JOB, &jobData };
    writeData.sendTeam = sendTeam;
    writeData.nimId = nimId;
    return checkAndConvertError(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanNimConn::sendTempCtrl(const char *nimAccountId, const fnet_temp_ctrl_t &tempCtrl)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_TEMP_CTRL, &tempCtrl };
    writeData.sendTeam = 0;
    writeData.nimId = nimAccountId;
    return checkAndConvertError(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanNimConn::sendLightCtrl(const char *nimAccountId, const fnet_light_ctrl_t &lightCtrl)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_LIGHT_CTRL, &lightCtrl };
    writeData.sendTeam = 0;
    writeData.nimId = nimAccountId;
    return checkAndConvertError(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanNimConn::sendAirFilterCtrl(const char *nimAccountId,
    const fnet_air_filter_ctrl_t &airFilterCtrl)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_AIR_FILTER_CTRL, &airFilterCtrl };
    writeData.sendTeam = 0;
    writeData.nimId = nimAccountId;
    return checkAndConvertError(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanNimConn::sendClearFanCtrl(const char *nimAccountId,
    const fnet_clear_fan_ctrl_t &clearFanCtrl)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_CLEAR_FAN_CTRL, &clearFanCtrl };
    writeData.sendTeam = 0;
    writeData.nimId = nimAccountId;
    return checkAndConvertError(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanNimConn::sendMoveCtrl(const char *nimAccountId, const fnet_move_ctrl_t &moveCtrl)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_MOVE_CTRL, &moveCtrl };
    writeData.sendTeam = 0;
    writeData.nimId = nimAccountId;
    return checkAndConvertError(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanNimConn::sendHomingCtrl(const char *nimAccountId)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_HOMING_CTRL, nullptr };
    writeData.sendTeam = 0;
    writeData.nimId = nimAccountId;
    return checkAndConvertError(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanNimConn::sendMatlStationCtrl(const char *nimAccountId,
    const fnet_matl_station_ctrl_t &matlStationCtrl)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_MATL_STATION_CTRL, &matlStationCtrl };
    writeData.sendTeam = 0;
    writeData.nimId = nimAccountId;
    return checkAndConvertError(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanNimConn::sendIndepMatlCtrl(const char *nimAccountId,
    const fnet_indep_matl_ctrl_t &indepMatlCtrl)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_INDEP_MATL_CTRL, &indepMatlCtrl };
    writeData.sendTeam = 0;
    writeData.nimId = nimAccountId;
    return checkAndConvertError(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanNimConn::sendPrintCtrl(const char *nimAccountId, const fnet_print_ctrl_t &printCtrl)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_PRINT_CTRL, &printCtrl };
    writeData.sendTeam = 0;
    writeData.nimId = nimAccountId;
    return checkAndConvertError(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanNimConn::sendJobCtrl(const char *nimAccountId, const fnet_job_ctrl_t &jobCtrl)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_JOB_CTRL, &jobCtrl };
    writeData.sendTeam = 0;
    writeData.nimId = nimAccountId;
    return checkAndConvertError(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanNimConn::sendStateCtrl(const char *nimAccountId, const fnet_state_ctrl_t &stateCtrl)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_STATE_CTRL, &stateCtrl };
    writeData.sendTeam = 0;
    writeData.nimId = nimAccountId;
    return checkAndConvertError(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanNimConn::sendPlateDetectCtrl(const char *nimAccountId, const fnet_plate_detect_ctrl &plateDetectCtrl)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_PLATE_DETECT_CTRL, &plateDetectCtrl};
    writeData.sendTeam = 0;
    writeData.nimId = nimAccountId;
    return checkAndConvertError(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanNimConn::sendFirstLayerDetectCtrl(const char *nimAccountId,
    const fnet_first_layer_detect_ctrl_t &firstLayerDetectCtrl)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_FIRST_LAYER_DETECT_CTRL, &firstLayerDetectCtrl };
    writeData.sendTeam = 0;
    writeData.nimId = nimAccountId;
    return checkAndConvertError(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanNimConn::sendCameraStreamCtrl(const char *nimAccountId,
    const fnet_camera_stream_ctrl_t &cameraStreamCtrl)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = {FNET_CONN_WRITE_CAMERA_STREAM_CTRL, &cameraStreamCtrl};
    writeData.sendTeam = 0;
    writeData.nimId = nimAccountId;
    return checkAndConvertError(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanNimConn::sendMatlStationConfig(const char *nimAccountId,
    const fnet_matl_station_config_t &matlStationConfig)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_MATL_STATION_CONFIG, &matlStationConfig };
    writeData.sendTeam = 0;
    writeData.nimId = nimAccountId;
    return checkAndConvertError(m_networkIntfc->connectionSend(m_conn, &writeData));
}

ComErrno ComWanNimConn::sendIndepMatlConfig(const char *nimAccountId,
    const fnet_indep_matl_config_t &indepMatlConfig)
{
    boost::shared_lock<boost::shared_mutex> lock(m_connMutex);
    if (m_conn == nullptr) {
        return COM_ERROR;
    }
    fnet_conn_write_data_t writeData = { FNET_CONN_WRITE_INDEP_MATL_CONFIG, &indepMatlConfig };
    writeData.sendTeam = 0;
    writeData.nimId = nimAccountId;
    return checkAndConvertError(m_networkIntfc->connectionSend(m_conn, &writeData));
}

bool ComWanNimConn::checkError(int fnetError)
{
    if (fnetError == FNET_NIM_DATA_BASE_ERROR) {
        QueueEvent(new wxCommandEvent(WAN_CONN_NIM_DATA_BASE_ERROR_EVENT));
    }
    return fnetError == FNET_OK;
}

ComErrno ComWanNimConn::checkAndConvertError(int fnetError)
{
    if (fnetError == FNET_NIM_DATA_BASE_ERROR) {
        QueueEvent(new wxCommandEvent(WAN_CONN_NIM_DATA_BASE_ERROR_EVENT));
    }
    return MultiComUtils::fnetRet2ComErrno(fnetError);
}

void ComWanNimConn::statusCallback(fnet_conn_status_t status, void *data)
{
    WanConnStatusEvent *event = new WanConnStatusEvent;
    event->SetEventType(WAN_CONN_STATUS_EVENT);
    event->status = status;
    ((ComWanNimConn *)data)->QueueEvent(event);
}

void ComWanNimConn::readCallback(fnet_conn_read_data_t *readData, void *data)
{
    WanConnReadEvent *event = new WanConnReadEvent;
    event->SetEventType(WAN_CONN_READ_EVENT);
    event->readData = *readData;
    ((ComWanNimConn *)data)->QueueEvent(event);
}

void ComWanNimConn::subscribeCallback(const char *nimAccountId, unsigned int status, void *data)
{
    WanConnSubscribeEvent *event = new WanConnSubscribeEvent;
    event->SetEventType(WAN_CONN_SUBSCRIBE_EVENT);
    event->nimAccountId = nimAccountId;
    event->status = status;
    ((ComWanNimConn *)data)->QueueEvent(event);
}

}} // namespace Slic3r::GUI
