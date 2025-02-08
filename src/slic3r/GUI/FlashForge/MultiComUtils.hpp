#ifndef slic3r_GUI_MultiComUtils_hpp_
#define slic3r_GUI_MultiComUtils_hpp_

#include <functional>
#include <memory>
#include <string>
#include <wx/event.h>
#include "FlashNetworkIntfc.h"
#include "MultiComDef.hpp"

namespace Slic3r { namespace GUI {

struct ComAsyncCallFinishEvent : public wxCommandEvent {
    ComErrno ret;
};
wxDECLARE_EVENT(COM_ASYNC_CALL_FINISH_EVENT, ComAsyncCallFinishEvent);

class ComAsyncThread;
typedef std::shared_ptr<ComAsyncThread> com_thread_ptr_t;
typedef std::function<ComErrno()> com_async_call_func_t;

class MultiComUtils
{
public:
    static ComErrno getLanDevList(std::vector<fnet_lan_dev_info> &devInfos);

    static ComErrno getTokenByPassword(const std::string &userName, const std::string &password,
        const std::string &language, com_token_data_t &tokenData, std::string &message, int msTimeout);

    static ComErrno refreshToken(const std::string &refreshToken, com_token_data_t &tokenData, int msTimeout);

    static ComErrno getClientToken(com_clinet_token_data_t &clinetTokenData, int msTimeout);

    static ComErrno sendSMSCode(const std::string &clinetAccessToken, const std::string &phoneNumber,
        const std::string &language, std::string &message, int msTimeout);

    static ComErrno getTokenBySMSCode(const std::string &userName, const std::string &SMSCode,
        const std::string &language, com_token_data_t &tokenData, std::string &message, int msTimeout);

    static ComErrno checkToken(const std::string &accessToken, int msTimeout);

    static ComErrno signOut(const std::string &accessToken, int msTimeout);

    static ComErrno getUserProfile(const std::string &accessToken, com_user_profile_t &userProfile,
        int msTimeout);

    static ComErrno getNimData(const std::string &uid, const std::string &accessToken,
        com_nim_data_t &nimData, int msTimeout);

    static ComErrno downloadFile(const std::string &url, std::vector<char> &bytes, int msTimeout);

    static ComErrno fnetRet2ComErrno(int networkRet);

    static com_thread_ptr_t asyncCall(wxEvtHandler *evtHandler, const com_async_call_func_t &func);

    static void killAsyncCall(const com_thread_ptr_t &thread); // wouldn't be safe and would probably leak resources

    static std::vector<fnet_material_mapping_t> comMaterialMappings2Fnet(
        const std::vector<com_material_mapping_t> &comMaterialMappings);
};

}} // namespace Slic3r::GUI

#endif
