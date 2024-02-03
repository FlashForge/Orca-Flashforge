#ifndef slic3r_GUI_ComAsyncConn_hpp_
#define slic3r_GUI_ComAsyncConn_hpp_

#include <memory>
#include <boost/thread/thread.hpp>
#include <wx/event.h>
#include "MultiComDef.hpp"
#include "UserDataUpdateThd.hpp"

namespace Slic3r { namespace GUI {

struct WanConnReadDataEvent : public wxCommandEvent {
    fnet_conn_read_data_t readData;
};
wxDECLARE_EVENT(WAN_CONN_READ_DATA_EVENT, WanConnReadDataEvent);

class ComWanAsyncConn : public wxEvtHandler
{
public:
    ComWanAsyncConn(fnet::FlashNetworkIntfc *networkIntfc);

    ComErrno createConn(const std::string &accessToken);

    void freeConn();

    void postSyncBindDev(const std::string &devId);

    void postSyncUnbindDev(const std::string &devId);

    void postSubscribeDev(const std::vector<std::string> &devIds);

private:
    void run();

    static int readCallback(fnet_conn_read_data_t *readData, void *data);

private:
    void                          *m_conn;
    std::unique_ptr<boost::thread> m_thread;
    fnet::FlashNetworkIntfc       *m_networkIntfc;
};

}} // namespace Slic3r::GUI

#endif
