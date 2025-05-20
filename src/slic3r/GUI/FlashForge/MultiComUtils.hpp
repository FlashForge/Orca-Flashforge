#ifndef slic3r_GUI_MultiComUtils_hpp_
#define slic3r_GUI_MultiComUtils_hpp_

#include <string>
#include "FlashNetworkIntfc.h"
#include "MultiComDef.hpp"

namespace Slic3r { namespace GUI {

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

    static ComErrno downloadFileMem(const std::string &url, std::vector<char> &bytes,
        fnet_progress_callback_t callback, void *callbackData, int msConnectTimeout, int msTimeout);

    static ComErrno downloadFileDisk(const std::string &url, const wxString &saveName,
        fnet_progress_callback_t callback, void *callbackData, int msConnectTimeout, int msTimeout);

    static ComErrno fnetRet2ComErrno(int networkRet);

    static std::vector<fnet_material_mapping_t> comMaterialMappings2Fnet(
        const std::vector<com_material_mapping_t> &comMaterialMappings);
};

}} // namespace Slic3r::GUI

#endif
