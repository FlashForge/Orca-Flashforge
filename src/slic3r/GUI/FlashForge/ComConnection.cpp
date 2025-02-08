#include "ComConnection.hpp"
#include "MultiComEvent.hpp"
#include "WanDevTokenMgr.hpp"

namespace Slic3r { namespace GUI {

wxDEFINE_EVENT(COMMAND_FAILED_EVENT, CommandFailedEvent);

ComConnection::ComConnection(com_id_t id, const std::string &checkCode,
    const fnet_lan_dev_info_t &devInfo, fnet::FlashNetworkIntfc *networkIntfc)
    : m_id(id)
    , m_connectMode(COM_CONNECT_LAN)
    , m_serialNumber(devInfo.serialNumber)
    , m_ip(devInfo.ip)
    , m_port(devInfo.port)
    , m_checkCode(checkCode)
    , m_updateDetailTime(std_precise_clock::now())
    , m_networkIntfc(networkIntfc)
{
    m_cmdExecData.connectMode = m_connectMode;
    m_cmdExecData.networkIntfc = m_networkIntfc;
    m_cmdExecData.ip = m_ip.c_str();
    m_cmdExecData.port = m_port;
    m_cmdExecData.serialNumber = m_serialNumber.c_str();
    m_cmdExecData.checkCode = m_checkCode.c_str();
    m_cmdExecData.uid = m_uid.c_str();
    m_cmdExecData.accessToken = nullptr;
    m_cmdExecData.deviceId = m_deviceId.c_str();
    m_cmdExecData.nimAccountId = m_nimAccountId.c_str();
}

ComConnection::ComConnection(com_id_t id, const std::string &uid, const std::string &serialNumber,
    const std::string &devId, const std::string &nimAccountId, fnet::FlashNetworkIntfc *networkIntfc)
    : m_id(id)
    , m_connectMode(COM_CONNECT_WAN)
    , m_serialNumber(serialNumber)
    , m_port(0)
    , m_uid(uid)
    , m_deviceId(devId)
    , m_nimAccountId(nimAccountId)
    , m_updateDetailTime(std_precise_clock::time_point::max())
    , m_networkIntfc(networkIntfc)
{
    m_cmdExecData.connectMode = m_connectMode;
    m_cmdExecData.networkIntfc = m_networkIntfc;
    m_cmdExecData.ip = m_ip.c_str();
    m_cmdExecData.port = m_port;
    m_cmdExecData.serialNumber = m_serialNumber.c_str();
    m_cmdExecData.checkCode = m_checkCode.c_str();
    m_cmdExecData.uid = m_uid.c_str();
    m_cmdExecData.accessToken = nullptr;
    m_cmdExecData.deviceId = m_deviceId.c_str();
    m_cmdExecData.nimAccountId = m_nimAccountId.c_str();
}

void ComConnection::connect()
{
    m_thread.reset(new boost::thread(boost::bind(&ComConnection::run, this)));
}

void ComConnection::disconnect(unsigned int waitMilliseconds/*= -1*/)
{
    m_exitThreadEvent.set(true);
    if (waitMilliseconds > 0) {
        m_thread->try_join_for(boost::chrono::milliseconds(waitMilliseconds));
    }
}

void ComConnection::joinThread()
{
    m_thread->join();
}

void ComConnection::putCommand(const ComCommandPtr &command, int priority /* = 3 */,
    bool checkDup /* = false */)
{
    ComSendGcode *sendGcode = dynamic_cast<ComSendGcode *>(command.get());
    if (sendGcode != nullptr) {
        sendGcode->setConectionData(m_id, this);
    }
    m_commandQue.pushBack(command, priority, checkDup);
}

bool ComConnection::abortSendGcode(int commandId)
{
    ComCommandPtr command = m_commandQue.get(commandId);
    if (command.get() == nullptr) {
        return false;
    }
    ComSendGcode *sendGcode = dynamic_cast<ComSendGcode *>(command.get());
    if (sendGcode == nullptr) {
        return false;
    }
    sendGcode->abort();
    return true;
}

void ComConnection::run()
{
    fnet_dev_product_t *product;
    fnet_dev_detail_t *detail;
    ComErrno ret = initialize(&product, &detail);
    if (ret != COM_OK) {
        QueueEvent(new ComConnectionExitEvent(COM_CONNECTION_EXIT_EVENT, m_id, ret));
        return;
    }
    QueueEvent(new ComConnectionReadyEvent(COM_CONNECTION_READY_EVENT, m_id, product, detail));
    ret = commandLoop();
    QueueEvent(new ComConnectionExitEvent(COM_CONNECTION_EXIT_EVENT, m_id, ret));
}

ComErrno ComConnection::commandLoop()
{
    int errorCnt = 0;
    while (!m_exitThreadEvent.get()) {
        ComCommandPtr frontCommand = m_commandQue.getFront(100);
        if (frontCommand != nullptr) {
            ComErrno ret;
            if (m_connectMode == COM_CONNECT_LAN) {
                ret = frontCommand->exec(m_cmdExecData);
            } else {
                ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
                m_cmdExecData.accessToken = token.accessToken().c_str();
                ret = frontCommand->exec(m_cmdExecData);
            }
            processCommand(frontCommand.get(), ret);
            if (ret == COM_OK || ret == COM_DEVICE_IS_BUSY) {
                errorCnt = 0;
            } else if (ret == COM_VERIFY_LAN_DEV_FAILED || ret == COM_UNAUTHORIZED
                    || ret != COM_ABORTED_BY_USER && ++errorCnt > 5) {
                if (m_connectMode == COM_CONNECT_LAN) {
                    return ret;
                } else if (ret != COM_NIM_SEND_ERROR) {
                    QueueEvent(new CommandFailedEvent(COMMAND_FAILED_EVENT, ret, false));
                    errorCnt = 0;
                }
            }
            m_commandQue.pop(frontCommand->commandId());
        }
        if (m_connectMode == COM_CONNECT_LAN) {
            std::chrono::duration<double> duration = std_precise_clock::now() - m_updateDetailTime;
            if (duration.count() > 3) {
                m_commandQue.pushBack(ComCommandPtr(new ComGetDevDetail), 5, true);
                m_updateDetailTime = std_precise_clock::now();
            }
        }
    }
    return COM_OK;
}

ComErrno ComConnection::initialize(fnet_dev_product_t **product, fnet_dev_detail_t **detail)
{
    ComErrno ret;
    if (m_connectMode == COM_CONNECT_LAN) {
        ComGetDevProduct getDevProduct;
        ret = getDevProduct.exec(m_cmdExecData);
        *product = getDevProduct.devProduct();
        if (ret == COM_OK) {
            ComGetDevDetail getDevDetail;
            ret = getDevDetail.exec(m_cmdExecData);
            *detail = getDevDetail.devDetail();
        }
    } else {
        ComGetDevProductDetail getDevProductDetail;
        int tryCnt = 3;
        for (int i = 0; i < tryCnt; ++i) {
            ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
            m_cmdExecData.accessToken = token.accessToken().c_str();
            ret = getDevProductDetail.exec(m_cmdExecData);
            token.unlockToken();
            if (ret == COM_OK || ret == COM_UNAUTHORIZED || m_exitThreadEvent.get()) {
                break;
            } else if (i + 1 < tryCnt) {
                int waitTimes[] = { 1000, 3000, 5000 };
                m_exitThreadEvent.waitTrue(waitTimes[i]);
            }
        }
        if (ret != COM_OK) {
            QueueEvent(new CommandFailedEvent(COMMAND_FAILED_EVENT, ret, false));
        }
        *product = getDevProductDetail.devProduct();
        *detail = getDevProductDetail.devDetail();
    }
    return ret;
}

void ComConnection::processCommand(ComCommand *command, ComErrno ret)
{
    auto &commandTypeId = typeid(*command);
    if (commandTypeId == typeid(ComGetDevGcodeList)) {
        ComGetDevGcodeList *getDevGcodeList = (ComGetDevGcodeList *)command;
        int commandId = getDevGcodeList->commandId();
        QueueEvent(new ComGetDevGcodeListEvent(COM_GET_DEV_GCODE_LIST_EVENT, m_id,
            commandId, ret, getDevGcodeList->lanGcodeList(), getDevGcodeList->wanGcodeList()));
        return;
    }
    if (commandTypeId == typeid(ComGetGcodeThumb)) {
        ComGetGcodeThumb *getGcodeThumb = (ComGetGcodeThumb *)command;
        QueueEvent(new ComGetGcodeThumbEvent(COM_GET_GCODE_THUMB_EVENT, m_id,
            getGcodeThumb->commandId(), ret, getGcodeThumb->thumbData()));
        return;
    }
    if (commandTypeId == typeid(ComStartJob)) {
        ComStartJob *getDevGcodeList = (ComStartJob *)command;
        QueueEvent(new ComStartJobEvent(COM_START_JOB_EVENT, m_id, getDevGcodeList->commandId(), ret));
        return;
    }
    if (commandTypeId == typeid(ComSendGcode)) {
        ComSendGcode *sendGcode = (ComSendGcode *)command;
        QueueEvent(new ComSendGcodeFinishEvent(
            COM_SEND_GCODE_FINISH_EVENT, m_id, sendGcode->commandId(), ret));
        return;
    }
    if (ret == COM_OK) {
        if (commandTypeId == typeid(ComGetDevDetail)) {
            ComGetDevDetail *getDevDetail = (ComGetDevDetail *)command;
            QueueEvent(new ComDevDetailUpdateEvent(COM_DEV_DETAIL_UPDATE_EVENT, m_id,
                getDevDetail->commandId(), getDevDetail->devDetail()));
            return;
        }
    }
}

}} // namespace Slic3r::GUI
