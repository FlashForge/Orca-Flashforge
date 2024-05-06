#ifndef slic3r_GUI_MultiComDef_hpp_
#define slic3r_GUI_MultiComDef_hpp_

#include <ctime>
#include <string>
#include <vector>
#include "FlashNetwork.h"

namespace Slic3r { namespace GUI {

using com_id_t = int;
using com_id_list_t = std::vector<com_id_t>;
constexpr com_id_t ComInvalidId = -1;
constexpr int ComInvalidCommandId = -1;
constexpr int ComTimeoutLan = 5000;
constexpr int ComTimeoutWan = 5000;

enum ComErrno {
    COM_OK,
    COM_ERROR,
    COM_UNREGISTER_USER,
    COM_REPEAT_LOGIN,
    COM_ABORTED_BY_USER,
    COM_DEVICE_IS_BUSY,
    COM_VERIFY_LAN_DEV_FAILED,  // invalid serialNumber/checkCode
    COM_UNAUTHORIZED,           // invalid accessToken/clientAccessToken
    COM_INVALID_VALIDATION,     // invalid userName/password/SMSCode
    COM_DEVICE_HAS_BEEN_BOUND,
};

enum ComConnectMode {
    COM_CONNECT_LAN,
    COM_CONNECT_WAN,
};

struct com_token_data_t {
    int expiresIn;
    std::string accessToken;
    std::string refreshToken;
    time_t startTime;
};

struct com_clinet_token_data_t {
    int expiresIn;
    std::string accessToken;
    time_t startTime;
};

struct com_user_profile_t {
    std::string uid;
    std::string nickname;
    std::string headImgUrl;
};

struct com_wan_dev_info_t {
    std::string devId;
    std::string name;
    std::string model;
    std::string imageUrl;
    std::string status;
    std::string location;
    std::string serialNumber;
};

struct com_dev_data_t {
    ComConnectMode connectMode;
    fnet_lan_dev_info_t lanDevInfo;
    com_wan_dev_info_t wanDevInfo;
    fnet_dev_product_t *devProduct;
    fnet_dev_detail_t *devDetail;
};

}} // namespace Slic3r::GUI

#endif
