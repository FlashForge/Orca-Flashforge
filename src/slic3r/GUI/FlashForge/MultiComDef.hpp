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
constexpr int ComTimeoutLanA = 5000;
constexpr int ComTimeoutLanB = 15000;
constexpr int ComTimeoutWanA = 7000;
constexpr int ComTimeoutWanB = 15000;

enum ComErrno {
    COM_OK,
    COM_ERROR,
    COM_UNREGISTER_USER,
    COM_REPEAT_LOGIN,
    COM_ABORTED_BY_USER,
    COM_DEVICE_IS_BUSY,
    COM_GCODE_NOT_FOUND,
    COM_VERIFY_LAN_DEV_FAILED,  // invalid serialNumber/checkCode
    COM_UNAUTHORIZED,           // invalid accessToken/clientAccessToken
    COM_INVALID_VALIDATION,     // invalid userName/password/SMSCode
    COM_DEVICE_HAS_BEEN_BOUND,
    COM_NIM_SEND_ERROR,
};

enum ComConnectMode {
    COM_CONNECT_LAN,
    COM_CONNECT_WAN,
};

enum ComCloundJobErrno {
    COM_CLOUND_JOB_OK,
    COM_CLOUND_JOB_DEVICE_BUSY,
    COM_CLOUND_JOB_DEVICE_NOT_FOUND,
    COM_CLOUND_JOB_SERVER_INTERNAL_ERROR,
    COM_CLOUND_JOB_UNKNOWN_ERROR,
    COM_CLOUND_JOB_NIM_SEND_ERROR,
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
    std::string nimAccountId;
};

struct com_gcode_list_t {
    int gcodeCnt;
    fnet_gcode_data_t *gcodeDatas;
};

struct com_dev_data_t {
    ComConnectMode connectMode;
    fnet_lan_dev_info_t lanDevInfo;
    com_wan_dev_info_t wanDevInfo;
    fnet_dev_product_t *devProduct;
    fnet_dev_detail_t *devDetail;
    com_gcode_list_t lanGcodeList;
    com_gcode_list_t wanGcodeList;
    bool devDetailUpdated;
};

struct com_material_mapping_t {
    int toolId;
    int slotId;
    std::string materialName;
    std::string toolMaterialColor;
    std::string slotMaterialColor;
};

struct com_local_job_data_t {
    std::string fileName;       // utf-8
    bool printNow;
    bool levelingBeforePrint;
    bool flowCalibration;
    bool useMatlStation;
    std::vector<com_material_mapping_t> materialMappings;
};

struct com_send_gcode_data_t {
    std::string gcodeFilePath;  // utf-8
    std::string thumbFilePath;  // utf-8, wan only
    std::string gcodeDstName;   // utf-8
    bool printNow;
    bool levelingBeforePrint;
    bool flowCalibration;
    bool useMatlStation;
    std::vector<com_material_mapping_t> materialMappings;
};

struct com_gcode_tool_data_t {
    int toolId;
    int slotId;
    std::string materialName;
    std::string materialColor;
    double filemanetWeight;     // gram
};

struct com_gcode_data_t {
    std::string fileName;
    std::string thumbUrl;       // Wan only
    double printingTime;        // second
    double totalFilamentWeight; // gram
    bool useMatlStation;
    std::vector<com_gcode_tool_data_t> gcodeToolDatas;
};

struct com_nim_data_t {
    std::string nimDataId;
    std::string appNimAccountId;
    std::string nimTeamId;
};

}} // namespace Slic3r::GUI

#endif
