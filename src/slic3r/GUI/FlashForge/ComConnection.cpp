#include "ComConnection.hpp"
#include "MultiComEvent.hpp"

namespace Slic3r { namespace GUI {

ComConnection::ComConnection(com_id_t id, const std::string &checkCode,
    const fnet_lan_dev_info_t &devInfo, fnet::FlashNetworkIntfc *networkIntfc)
    : m_id(id)
    , m_connectMode(COM_CONNECT_LAN)
    , m_serialNumber(devInfo.serialNumber)
    , m_ip(devInfo.ip)
    , m_port(devInfo.port)
    , m_checkCode(checkCode)
    , m_getDetailClock(clock())
    , m_networkIntfc(networkIntfc)
{
}

ComConnection::ComConnection(com_id_t id, const std::string &accessToken,
    const std::string &serialNumber, const std::string &devId, fnet::FlashNetworkIntfc *networkIntfc)
    : m_id(id)
    , m_connectMode(COM_CONNECT_WAN)
    , m_serialNumber(serialNumber)
    , m_port(0)
    , m_accessToken(accessToken)
    , m_deviceId(devId)
    , m_getDetailClock(clock())
    , m_networkIntfc(networkIntfc)
{
}

void ComConnection::connect()
{
    m_thread.reset(new boost::thread(boost::bind(&ComConnection::run, this)));
}

void ComConnection::disconnect(unsigned int waitMilliseconds/*= -1*/)
{
    m_exitThread = true;
    if (waitMilliseconds > 0) {
        m_thread->try_join_for(boost::chrono::milliseconds(waitMilliseconds));
    }
}

void ComConnection::joinThread()
{
    m_thread->join();
}

void ComConnection::setAccessToken(const std::string &accessToken)
{
    boost::mutex::scoped_lock lock(m_tokenMutex);
    m_accessToken = accessToken;
}

void ComConnection::putCommand(const ComCommandPtr &command, int priority/* =3 */)
{
    ComSendGcode *sendGcode = dynamic_cast<ComSendGcode *>(command.get());
    if (sendGcode != nullptr) {
        sendGcode->setConectionData(m_id, this);
    }
    m_commandQue.pushBack(command, priority, false);
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
    ComGetDevDetail getDevDetail;
    ComErrno ret;
    if (m_connectMode == COM_CONNECT_LAN) {
        ret = getDevDetail.exec(m_networkIntfc, m_ip, m_port, m_serialNumber, m_checkCode);
    } else {
        ret = getDevDetail.exec(m_networkIntfc, m_accessToken, m_deviceId);
    }
    if (ret != COM_OK) {
        QueueEvent(new ComConnectionExitEvent(COM_CONNECTION_EXIT_EVENT, m_id, ret));
        return;
    }
    QueueEvent(new ComConnectionReadyEvent(COM_CONNECTION_READY_EVENT, m_id, getDevDetail.devDetail()));
    ret = commandLoop();
    QueueEvent(new ComConnectionExitEvent(COM_CONNECTION_EXIT_EVENT, m_id, ret));
}

ComErrno ComConnection::commandLoop()
{
    int errorCnt = 0;
    while (!m_exitThread) {
        ComCommandPtr frontCommand = m_commandQue.getFront(100);
        if (frontCommand != nullptr) {
            ComErrno ret;
            if (m_connectMode == COM_CONNECT_LAN) {
                ret = frontCommand->exec(m_networkIntfc, m_ip, m_port, m_serialNumber, m_checkCode);
            } else {
                ret = frontCommand->exec(m_networkIntfc, m_accessToken, m_deviceId);
            }
            processCommand(frontCommand.get(), ret);
            if (m_connectMode == COM_CONNECT_LAN) {
                if (ret == COM_OK || ret == COM_DEVICE_IS_BUSY) {
                    errorCnt = 0;
                } else if (ret == COM_VERIFY_LAN_DEV_FAILED || ++errorCnt > 5) {
                    return ret;
                }
            }
            m_commandQue.pop(frontCommand->commandId());
        }
        if ((clock() - m_getDetailClock) / (double)CLOCKS_PER_SEC > 3) {
            m_commandQue.pushBack(ComCommandPtr(new ComGetDevDetail), 5, true);
            m_getDetailClock = clock();
        }
    }
    return COM_OK;
}

std::string ComConnection::getAccessToken()
{
    boost::mutex::scoped_lock lock(m_tokenMutex);
    return m_accessToken;
}

void ComConnection::processCommand(ComCommand *command, ComErrno ret)
{
    ComSendGcode *sendGcode = dynamic_cast<ComSendGcode *>(command);
    if (sendGcode != nullptr) {
        QueueEvent(new ComSendGcodeFinishEvent(
            COM_SEND_GCODE_FINISH_EVENT, m_id, sendGcode->commandId(), ret));
    }
    if (ret == COM_OK) {
        ComGetDevDetail *getDevDetail = dynamic_cast<ComGetDevDetail *>(command);
        if (getDevDetail != nullptr) {
            QueueEvent(new ComDevDetailUpdateEvent(COM_DEV_DETAIL_UPDATE_EVENT, m_id,
                getDevDetail->commandId(), getDevDetail->devDetail()));
        }
    }
}

}} // namespace Slic3r::GUI
