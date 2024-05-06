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
        const std::string &language, com_token_data_t &tokenData, std::string &message);

    static ComErrno refreshToken(const std::string &refreshToken, com_token_data_t &tokenData);

    static ComErrno getClientToken(com_clinet_token_data_t &clinetTokenData);

    static ComErrno sendSMSCode(const std::string &clinetAccessToken, const std::string &phoneNumber,
        const std::string &language, std::string &message);

    static ComErrno getTokenBySMSCode(const std::string &userName, const std::string &SMSCode,
        const std::string &language, com_token_data_t &tokenData, std::string &message);

    static ComErrno checkToken(const std::string &accessToken);

    static ComErrno signOut(const std::string &accessToken);

    static ComErrno getUserProfile(const std::string &accessToken, com_user_profile_t &userProfile);

    static ComErrno downloadFile(const std::string &url, std::vector<char> &bytes, int msTimeout);

    static ComErrno fnetRet2ComErrno(int networkRet);

    static com_thread_ptr_t asyncCall(wxEvtHandler *evtHandler, const com_async_call_func_t &func);

    static void killAsyncCall(const com_thread_ptr_t &thread); // wouldn't be safe and would probably leak resources
};

}} // namespace Slic3r::GUI

#endif
