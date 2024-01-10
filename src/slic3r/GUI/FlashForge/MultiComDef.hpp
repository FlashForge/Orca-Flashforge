#ifndef slic3r_GUI_MultiComDef_hpp_
#define slic3r_GUI_MultiComDef_hpp_

#include <string>
#include <vector>
#include "FlashNetwork.h"

namespace Slic3r { namespace GUI {

using com_id_t = int;
using com_id_list_t = std::vector<com_id_t>;
constexpr com_id_t ComInvalidId = -1;

enum ComErrno {
    COM_OK,
    COM_ERROR,
    COM_UNINITIALIZED,
    COM_DEVICE_IS_BUSY,
    COM_VERIFY_LAN_DEV_FAILED,  // invalid serialNumber/checkCode
    COM_UNAUTHORIZED,           // invalid accessToken/clientAccessToken
    COM_INVALID_VALIDATION,     // invalid userName/password/SMSCode
};

enum ComConnectMode {
    COM_CONNECT_LAN,
    COM_CONNECT_WAN,
};

struct com_token_data_t {
    int expiresIn;
    std::string accessToken;
    std::string refreshToken;
};

struct com_clinet_token_data_t {
    int expiresIn;
    std::string accessToken;
};

struct com_user_profile_t {
    std::string nickname;
    std::string headImgUrl;
};

struct com_dev_data_t {
    ComConnectMode connectMode;
    fnet_dev_detail_t *devDetail;
};

}} // namespace Slic3r::GUI

#endif
