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
    , m_getDetailClock(clock())
    , m_exitThread(false)
    , m_networkIntfc(networkIntfc)
{
}

ComConnection::ComConnection(com_id_t id, const std::string &uid, const std::string &serialNumber,
    const std::string &devId, fnet::FlashNetworkIntfc *networkIntfc)
    : m_id(id)
    , m_connectMode(COM_CONNECT_WAN)
    , m_serialNumber(serialNumber)
    , m_port(0)
    , m_uid(uid)
    , m_deviceId(devId)
    , m_getDetailClock(clock())
    , m_exitThread(false)
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
    while (!m_exitThread) {
        ComCommandPtr frontCommand = m_commandQue.getFront(100);
        if (frontCommand != nullptr) {
            ComErrno ret;
            if (m_connectMode == COM_CONNECT_LAN) {
                ret = frontCommand->exec(m_networkIntfc, m_ip, m_port, m_serialNumber, m_checkCode);
            } else {
                ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
                ret = frontCommand->exec(m_networkIntfc, m_uid, token.accessToken(), m_deviceId);
            }
            processCommand(frontCommand.get(), ret);
            if (ret == COM_OK || ret == COM_DEVICE_IS_BUSY) {
                errorCnt = 0;
            } else if (ret == COM_VERIFY_LAN_DEV_FAILED || ret == COM_UNAUTHORIZED
                    || ret != COM_ABORTED_BY_USER && ++errorCnt > 5) {
                if (m_connectMode == COM_CONNECT_LAN) {
                    return ret;
                } else {
                    QueueEvent(new CommandFailedEvent(COMMAND_FAILED_EVENT, ret, false));
                    errorCnt = 0;
                }
            }
            m_commandQue.pop(frontCommand->commandId());
        }
        if (m_connectMode == COM_CONNECT_LAN && (clock() - m_getDetailClock) / (double)CLOCKS_PER_SEC > 3) {
            m_commandQue.pushBack(ComCommandPtr(new ComGetDevDetail), 5, true);
            m_getDetailClock = clock();
        }
    }
    return COM_OK;
}

ComErrno ComConnection::initialize(fnet_dev_product_t **product, fnet_dev_detail_t **detail)
{
    ComErrno ret;
    if (m_connectMode == COM_CONNECT_LAN) {
        ComGetDevProduct getDevProduct;
        ret = getDevProduct.exec(m_networkIntfc, m_ip, m_port, m_serialNumber, m_checkCode);
        *product = getDevProduct.devProduct();
        if (ret == COM_OK) {
            ComGetDevDetail getDevDetail;
            ret = getDevDetail.exec(m_networkIntfc, m_ip, m_port, m_serialNumber, m_checkCode);
            *detail = getDevDetail.devDetail();
        }
    } else {
        ComGetDevProductDetail getDevProductDetail;
        int tryCnt = 5;
        for (int i = 0; i < tryCnt; ++i) {
            ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
            ret = getDevProductDetail.exec(m_networkIntfc, m_uid, token.accessToken(), m_deviceId);
            token.releaseToken();
            if (ret == COM_OK || ret == COM_UNAUTHORIZED || m_exitThread) {
                break;
            } else if (i + 1 < tryCnt) {
                int sleepTimes[] = {1, 3, 5};
                boost::this_thread::sleep_for(boost::chrono::seconds(sleepTimes[i < 3 ? i : 2]));
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
