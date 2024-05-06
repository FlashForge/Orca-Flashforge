#ifndef slic3r_GUI_ComConnection_hpp_
#define slic3r_GUI_ComConnection_hpp_

#include <ctime>
#include <atomic>
#include <memory>
#include <string>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <wx/event.h>
#include "ComCommandQue.hpp"
#include "FlashNetworkIntfc.h"
#include "MultiComDef.hpp"
#include "WaitEvent.hpp"

namespace Slic3r { namespace GUI {

struct CommandFailedEvent : public wxCommandEvent {
    CommandFailedEvent(wxEventType type, ComErrno _ret, bool _fatalError)
        : wxCommandEvent(type)
        , ret(_ret)
        , fatalError(_fatalError) {
    }
    ComErrno ret;
    bool fatalError;
};
wxDECLARE_EVENT(COMMAND_FAILED_EVENT, CommandFailedEvent);

class ComConnection : public wxEvtHandler
{
public:
    ComConnection(com_id_t id, const std::string &checkCode, const fnet_lan_dev_info_t &devInfo,
        fnet::FlashNetworkIntfc *networkIntfc);
    
    ComConnection(com_id_t id, const std::string &uid, const std::string &serialNumber,
        const std::string &devId, fnet::FlashNetworkIntfc *networkIntfc);

    com_id_t id() const { return m_id; }

    ComConnectMode connectMode() const { return m_connectMode; }

    const std::string &serialNumber() const { return m_serialNumber; }

    const std::string &deviceId() const { return m_deviceId; }

    bool isDisconnect() { return m_exitThread; }

    void connect();

    void disconnect(unsigned int waitMilliseconds = -1);

    void joinThread();

    void putCommand(const ComCommandPtr &command, int priority = 3);

    bool abortSendGcode(int commandId);

private:
    void run();

    ComErrno commandLoop();

    ComErrno initialize(fnet_dev_product_t **product, fnet_dev_detail_t **detail);

    void processCommand(ComCommand *command, ComErrno ret);

private:
    com_id_t                        m_id;
    ComConnectMode                  m_connectMode;
    std::string                     m_serialNumber;
    std::string                     m_ip;
    unsigned short                  m_port;
    std::string                     m_checkCode;
    std::string                     m_uid;
    std::string                     m_deviceId;
    clock_t                         m_getDetailClock;
    std::atomic_bool                m_exitThread;
    ComCommandQue                   m_commandQue;
    fnet::FlashNetworkIntfc        *m_networkIntfc;
    std::unique_ptr<boost::thread>  m_thread;
};

}} // namespace Slic3r::GUI

#endif
