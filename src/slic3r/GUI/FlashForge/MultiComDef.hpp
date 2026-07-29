#ifndef slic3r_GUI_MultiComDef_hpp_
#define slic3r_GUI_MultiComDef_hpp_

#include <cstdint>
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
    COM_UNSUPPORTED,
    COM_ABORTED_BY_USER,
    COM_DEVICE_IS_BUSY,
    COM_GCODE_NOT_FOUND,
    COM_VERIFY_LAN_DEV_FAILED,  // invalid serialNumber/checkCode
    COM_UNAUTHORIZED,           // invalid accessToken/clientAccessToken
    COM_INVALID_VALIDATION,     // invalid userName/password/SMSCode
    COM_DEVICE_HAS_BEEN_BOUND,
    COM_ABORT_AI_JOB_FAILED,
    COM_AI_JOB_NOT_ENOUGH_POINTS,
    COM_NO_EXISTING_AI_MODEL_JOB,
    COM_INPUT_FAILED_THE_REVIEW,
    COM_PRINT_LIST_MODEL_COUNT_EXCEEDED,
    COM_CONN_SEND_ERROR,
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
    COM_CLOUND_JOB_CONN_SEND_ERROR,
};

struct com_token_data_t {
    int expiresIn;
    std::string accessToken;
    std::string refreshToken;
    time_t startTime;
};

struct com_user_profile_t {
    std::string uid;
    std::string nickname;
    std::string headImgUrl;
    std::string email;
};

struct com_add_wan_dev_data_t {
    com_user_profile_t userProfile;
    bool showUserPoints;
};

struct com_update_info_t {
    std::string status;
    std::string title;
    std::string content;
    std::string tips;
};

struct com_wan_dev_info_t {
    std::string devId;
    std::string name;
    std::string model;
    std::string imageUrl;
    std::string status;
    std::string location;
    std::string serialNumber;
    std::string devTopic;
    com_update_info_t updateInfo;
};

struct com_gcode_list_t {
    int gcodeCnt;
    fnet_gcode_data_t *gcodeDatas;
};

struct com_time_lapse_video_list_t {
    int videoCnt;
    fnet_time_lapse_video_data_t *videoDatas;
};

struct com_dev_data_t {
    ComConnectMode connectMode;
    fnet_lan_dev_info_t lanDevInfo;
    com_wan_dev_info_t wanDevInfo;
    fnet_dev_product_t *devProduct;
    fnet_dev_detail_t *devDetail;
    com_gcode_list_t lanGcodeList;
    com_gcode_list_t wanGcodeList;
    com_time_lapse_video_list_t wanTimeLapseVideoList;
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
    bool firstLayerInspection;
    bool timeLapseVideo;
    bool useMatlStation;
    std::vector<com_material_mapping_t> materialMappings;
};

struct com_send_gcode_data_t {
    std::string gcodeFilePath;  // utf-8
    std::string thumbFilePath;  // utf-8, wan only
    std::string gcodeDstName;   // utf-8
    std::string unionId;
    int  currentPlaterIndex;
    bool printNow;
    bool levelingBeforePrint;
    bool flowCalibration;
    bool firstLayerInspection;
    bool timeLapseVideo;
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

struct com_user_ai_points_info_t {
    int totalPoints;
    int modelGenPoints;
    int img2imgPoints;
    int txt2txtPoints;
    int txt2imgPoints;
    int remainingFreeCount;
    int freeRetriesPerProcess;
};

struct com_ai_job_pipeline_info_t {
    int64_t id;
    int isFree;
};

struct com_ai_model_job_result_t {
    int status;                 // 0 initializing, 1 in the queue, 2 running, 3 completed, 4 failed, 5 canceled
    int64_t jobId;
    int posInQueue;
    int queueLength;
    bool isOldJob;
};

struct com_ai_model_data_t {
    std::string modelType;
    std::string modelUrl;
};

struct com_ai_model_job_state_t {
    int status;                 // 0 initializing, 1 in the queue, 2 running, 3 completed, 4 failed, 5 canceled
    int64_t jobId;
    int posInQueue;
    int queueLength;
    std::vector<com_ai_model_data_t> models;
    std::string externalJobId;
};

struct com_ai_general_job_result_t {
    int status;                 // 0 waiting, 1 running, 2 failed, 3 done, 4 cancelled
    int64_t jobId;
    int posInQueue;
    int queueLength;
    int remainingFreeRetries;
};

struct com_ai_general_job_data_t {
    std::string content;
    std::string imageUrl;
};

struct com_ai_general_job_state_t {
    int status;                 // 0 waiting, 1 running, 2 failed, 3 done, 4 cancelled
    int64_t jobId;
    int posInQueue;
    int queueLength;
    std::vector<com_ai_general_job_data_t> datas;
    std::string externalJobId;
};

struct com_tracking_common_data_t {
    std::string uid;
    std::string did;
    std::string sid;
    std::string netType;
    std::string oper;
    std::string ext;
};

struct com_tracking_event_data_t {
    std::string eventType;
    std::string eventId;
    std::string eventName;
    std::string pageId;
    std::string moduleId;
    std::string reqId;
    std::string expIds;
    std::string objectType;
    std::string objectId;
    std::string extend;
    std::string timestamp;
};

struct com_sys_msg_data_t
{
    int id;
    std::string title;
    std::string content;
    std::string linkUrl;
    int duration;
    int showCount;
};

struct com_mqtt_config_t {
    std::string userTopic;
    std::vector<std::string> commonTopics;
};

}} // namespace Slic3r::GUI

#endif
