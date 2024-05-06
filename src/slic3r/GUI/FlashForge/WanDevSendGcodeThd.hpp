#ifndef slic3r_GUI_WanDevSendGcodeThd_hpp_
#define slic3r_GUI_WanDevSendGcodeThd_hpp_

#include <boost/thread/thread.hpp>
#include <wx/event.h>
#include "FlashNetworkIntfc.h"
#include "WaitEvent.hpp"

namespace Slic3r { namespace GUI {

class WanDevSendGcodeThd : public wxEvtHandler
{
public:
    WanDevSendGcodeThd(fnet::FlashNetworkIntfc *networkIntfc);

    void exit();

    bool startSendGcode(const std::string &uid, const std::vector<std::string> &devIds,
        const std::string &gcodeFilePath, const std::string &thumbFilePath,
        const std::string &gcodeDstName, bool printNow, bool levelingBeforePrint);

    bool abortSendGcode();

private:
    void run();

    int startCloundJob(const char *accessToken, const fnet_clound_gcode_data_t *cloundGcodeData,
        fnet_clound_job_error_t **errors, int *errorCnt);

    std::string getFileMd5(const char *filePath);

    static int callback(long long now, long long total, void *callbackData);

private:
    WaitEvent               m_sendGcodeEvent;
    std::string             m_uid;
    std::vector<std::string>m_devIds;
    std::string             m_gcodeFilePath;
    std::string             m_thumbFilePath;
    std::string             m_gcodeDstName;
    fnet_send_gcode_data_t  m_sendGcodeData;
    double                  m_progress;
    int                     m_callbackRet;
    std::atomic_bool        m_exitThread;
    boost::thread           m_thread;
    fnet::FlashNetworkIntfc*m_networkIntfc;
};

}} // namespace Slic3r::GUI

#endif
