#ifndef slic3r_GUI_WanDevSendGcodeThd_hpp_
#define slic3r_GUI_WanDevSendGcodeThd_hpp_

#include <map>
#include <boost/thread/thread.hpp>
#include <wx/event.h>
#include "FlashNetworkIntfc.h"
#include "MultiComDef.hpp"
#include "WaitEvent.hpp"

namespace Slic3r { namespace GUI {

class WanDevSendGcodeThd : public wxEvtHandler
{
public:
    WanDevSendGcodeThd(fnet::FlashNetworkIntfc *networkIntfc);

    void exit();

    bool startSendGcode(const std::string &uid, const std::vector<std::string> &devIds,
        const std::vector<std::string> &devSerialNumbers, const std::string &nimTeamId,
        const std::vector<std::string> &nimAccountIds, const com_send_gcode_data_t &sendGocdeData);

    bool abortSendGcode();

private:
    void run();

    int startCloundJob(const char *accessToken, const fnet_clound_gcode_data_t *cloundGcodeData,
        std::map<std::string, ComCloundJobErrno> &errorMap);

    std::string getFileMd5(const char *filePath);

    void sendStartCloundJob(const fnet_add_clound_job_result_t *results, int resultCnt,
        fnet_clound_job_data_t &jobData, std::map<std::string, ComCloundJobErrno> &errorMap);

    ComCloundJobErrno convertError(fnet_add_clound_job_error_t error);

    static int callback(long long now, long long total, void *callbackData);

    typedef std::map<std::string, std::string> dev_id_str_map_t;

private:
    WaitEvent               m_sendGcodeEvent;
    std::string             m_uid;
    std::vector<std::string>m_devIds;
    std::string             m_nimTeamId;
    dev_id_str_map_t        m_devSerialNumberMap;
    dev_id_str_map_t        m_nimAccountIdMap;
    com_send_gcode_data_t   m_comSendGcodeData;
    fnet_send_gcode_data_t  m_sendGcodeData;
    double                  m_progress;
    int                     m_callbackRet;
    std::atomic_bool        m_exitThread;
    boost::thread           m_thread;
    fnet::FlashNetworkIntfc*m_networkIntfc;
    std::vector<fnet_material_mapping_t> m_materialMappings;
};

}} // namespace Slic3r::GUI

#endif
