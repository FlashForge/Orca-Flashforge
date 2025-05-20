#ifndef _FLASHNETWORK_H_
#define _FLASHNETWORK_H_

#ifdef _WIN32
#ifdef _FNET_EXPORT_
#define FNET_API __declspec(dllexport)
#else
#define FNET_API __declspec(dllimport)
#endif
#else
#define FNET_API
#endif


#define MAX_DEVICE_SN_LEN 128
#define MAX_DEVICE_NAME_LEN 128

typedef enum fnet_log_level {
    FNET_LOG_LEVEL_OFF,
    FNET_LOG_LEVEL_ERROR,
    FNET_LOG_LEVEL_WARN,
    FNET_LOG_LEVEL_INFO,
    FNET_LOG_LEVEL_DEBUG,
} fnet_log_level_t;

typedef enum fnet_add_clound_job_error {
    FNET_ADD_CLOUND_JOB_OK,
    FNET_ADD_CLOUND_JOB_DEVICE_BUSY,
    FNET_ADD_CLOUND_JOB_DEVICE_NOT_FOUND,
    FNET_ADD_CLOUND_JOB_SERVER_INTERNAL_ERROR,
    FNET_ADD_CLOUND_JOB_UNKNOWN_ERROR,
} fnet_add_clound_job_error_t;

typedef enum fnet_conn_status {
    FNET_CONN_STATUS_LOGINED,
    FNET_CONN_STATUS_LOGOUT,
    FNET_CONN_STATUS_UNLOGIN,
} fnet_conn_status_t;

typedef enum fnet_conn_write_data_type {
    FNET_CONN_WRITE_SYNC_BIND_DEVICE,   // data, const char *devId
    FNET_CONN_WRITE_SYNC_UNBIND_DEVICE, // data, const char *devId
    FNET_CONN_WRITE_SYNC_DEVICE_UNREGISTER,// data, nullptr
    FNET_CONN_WRITE_UPDATE_DETAIL,      // data, nullptr
    FNET_CONN_WRITE_START_JOB,          // data, fnet_local_job_data_t
    FNET_CONN_WRITE_START_CLOUND_JOB,   // data, fnet_clound_job_data_t
    FNET_CONN_WRITE_TEMP_CTRL,          // data, fnet_temp_ctrl_t
    FNET_CONN_WRITE_LIGHT_CTRL,         // data, fnet_light_ctrl_t
    FNET_CONN_WRITE_AIR_FILTER_CTRL,    // data, fnet_air_filter_ctrl_t
    FNET_CONN_WRITE_CLEAR_FAN_CTRL,     // data, fnet_clear_fan_ctrl_t
    FNET_CONN_WRITE_MOVE_CTRL,          // data, fnet_move_ctrl_t
    FNET_CONN_WRITE_HOMING_CTRL,        // data, nullptr
    FNET_CONN_WRITE_MATL_STATION_CTRL,  // data, fnet_matl_station_ctrl_t
    FNET_CONN_WRITE_INDEP_MATL_CTRL,    // data, fnet_indep_matl_ctrl_t
    FNET_CONN_WRITE_PRINT_CTRL,         // data, fnet_print_ctrl_t
    FNET_CONN_WRITE_JOB_CTRL,           // data, fnet_job_ctrl_t
    FNET_CONN_WRITE_STATE_CTRL,         // data, fnet_state_ctrl_t
    FNET_CONN_WRITE_PLATE_DETECT_CTRL,  // data, fnet_plate_detect_ctrl_t
    FNET_CONN_WRITE_FIRST_LAYER_DETECT_CTRL, // data, fnet_first_layer_detect_ctrl_t
    FNET_CONN_WRITE_CAMERA_STREAM_CTRL, // data, fnet_camera_stream_ctrl_t
    FNET_CONN_WRITE_MATL_STATION_CONFIG,// data, fnet_matl_station_config_t
    FNET_CONN_WRITE_INDEP_MATL_CONFIG,  // data, fnet_indep_matl_config_t
} fnet_conn_write_data_type_t;

typedef enum fnet_conn_read_data_type {
    FNET_CONN_READ_SYNC_USER_PROFILE,   // data, nullptr
    FNET_CONN_READ_SYNC_BIND_DEVICE,    // data, nullptr
    FNET_CONN_READ_SYNC_UNBIND_DEVICE,  // data, nullptr
    FNET_CONN_READ_UNREGISTER_USER,     // data, nullptr
    FNET_CONN_READ_DEVICE_DETAIL,       // data, fnet_dev_detail_t
    FNET_CONN_READ_DEVICE_KEEP_ALIVE,   // data, nullptr
} fnet_conn_read_data_type_t;

struct fnet_conn_read_data;
typedef struct fnet_conn_read_data fnet_conn_read_data_t;

// returning a non-zero value from the callback aborts the transfer
typedef int (*fnet_progress_callback_t)(long long now, long long total, void *data);

typedef void (*fnet_conn_status_callback_t)(fnet_conn_status_t status, void *data);

// call fnet_freeXXXX to release readData->data, call fnet_freeString to release readData->devId
typedef void (*fnet_conn_read_callback_t)(fnet_conn_read_data_t *readData, void *data);

// call fnet_freeString to release nimAccountId
typedef void (*fnet_conn_subscribe_callback_t)(const char *nimAccountId, unsigned int status, void *data);


#pragma pack(push, 8)

typedef struct fnet_log_settings {
    const char *fileDir;
    int expireHours;
    fnet_log_level_t level;
} fnet_log_settings_t;

typedef struct fnet_material_mapping {
    int toolId;
    int slotId;
    const char *materialName;
    const char *toolMaterialColor;
    const char *slotMaterialColor;
} fnet_material_mapping_t;

typedef struct fnet_send_gcode_data {
    const char *gcodeFilePath;          // utf-8
    const char *thumbFilePath;          // utf-8, wan only
    const char *gcodeDstName;           // utf-8
    int printNow;                       // 1 true, 0 false
    int levelingBeforePrint;            // 1 true, 0 false
    int flowCalibration;                // 1 true, 0 false
    int firstLayerInspection;           // 1 true, 0 false
    int timeLapseVideo;                 // 1 true, 0 false
    int useMatlStation;                 // 1 true, 0 false
    int gcodeToolCnt;
    const fnet_material_mapping_t *materialMappings;
    fnet_progress_callback_t callback;
    void *callbackData;
} fnet_send_gcode_data_t;

typedef struct fnet_clound_job_data {
    const char **devIds;
    const char **devSerialNumbers;
    const char **jobIds;
    int devCnt;
    const char *gcodeName;
    const char *gcodeType;              // 3mf
    const char *gcodeMd5;
    long long gcodeSize;
    const char *thumbName;
    const char *thumbType;              // png
    const char *thumbMd5;
    long long thumbSize;
    const char *bucketName;
    const char *endpoint;
    const char *gcodeStorageKey;
    const char *gcodeStorageUrl;
    const char *thumbStorageKey;
    const char *thumbStorageUrl;
    int printNow;                       // 1 true, 0 false
    int levelingBeforePrint;            // 1 true, 0 false
    int flowCalibration;                // 1 true, 0 false
    int firstLayerInspection;           // 1 true, 0 false
    int timeLapseVideo;                 // 1 true, 0 false
    int useMatlStation;                 // 1 true, 0 false
    int gcodeToolCnt;
    const fnet_material_mapping_t *materialMappings;
} fnet_clound_job_data_t;

typedef struct fnet_local_job_data {
    const char *jobId;
    const char *thumbUrl;
    const char *fileName;
    int printNow;                       // 1 true, 0 false
    int levelingBeforePrint;            // 1 true, 0 false
    int flowCalibration;                // 1 true, 0 false
    int firstLayerInspection;           // 1 true, 0 false
    int timeLapseVideo;                 // 1 true, 0 false
    int useMatlStation;                 // 1 true, 0 false
    int gcodeToolCnt;
    const fnet_material_mapping_t *materialMappings;
} fnet_local_job_data_t;

typedef struct fnet_conn_settings {
    const char *nimDataId;
    fnet_conn_status_callback_t statusCallback;
    void *statusCallbackData;
    fnet_conn_read_callback_t readCallback;
    void *readCallbackData;
    fnet_conn_subscribe_callback_t subscribeCallback;
    void *subscribeCallbackData;
} fnet_conn_settings_t;

typedef struct fnet_conn_write_data {
    fnet_conn_write_data_type_t type;
    const void *data;
    int sendTeam;
    const char *nimId;              // nimAccountId/nimTeamId
} fnet_conn_write_data_t;

typedef struct fnet_conn_subscribe_data {
    const char **nimAccountIds;
    int accountCnt;                 // [1, 100]
    int duration;                   // [60, 2592000]
    int immediateSync;              // 1 true, 0 false
} fnet_conn_subscribe_data_t;

typedef struct fnet_conn_unsubscribe_data {
    const char **nimAccountIds;
    int accountCnt;                 // [1, 100]
} fnet_conn_unsubscribe_data_t;

typedef struct fnet_temp_ctrl {
    double platformTemp;
    double rightTemp;
    double leftTemp;
    double chamberTemp;
} fnet_temp_ctrl_t;

typedef struct fnet_light_ctrl {
    const char *lightStatus;        // "open", "close"
} fnet_light_ctrl_t;

typedef struct fnet_air_filter_ctrl {
    const char *internalFanStatus;  // "open", "close"
    const char *externalFanStatus;  // "open", "close"
} fnet_air_filter_ctrl_t;

typedef struct fnet_clear_fan_ctrl {
    const char *clearFanStatus;     // "open", "close"
} fnet_clear_fan_ctrl_t;

typedef struct fnet_move_ctrl {
    const char *axis;
    double delta;
} fnet_move_ctrl_t;

typedef struct fnet_matl_station_ctrl {
    int slotId;
    int action;                     // 0 load filament, 1 unload filament
} fnet_matl_station_ctrl_t;

typedef struct fnet_indep_matl_ctrl {
    int action;                     // 0 load filament, 1 unload filament
} fnet_indep_matl_ctrl_t;

typedef struct fnet_print_ctrl {
    double zAxisCompensation;       // mm
    double printSpeedAdjust;        // percent
    double coolingFanSpeed;         // percent
    double coolingFanLeftSpeed;     // percent
    double chamberFanSpeed;         // percent
} fnet_print_ctrl_t;

typedef struct fnet_job_ctrl {
    const char *jobId;
    const char *action;             // "pause", "continue", "cancel"
} fnet_job_ctrl_t;

typedef struct fnet_state_ctrl {
    const char *action;             // "setClearPlatform"
} fnet_state_ctrl_t;

typedef struct fnet_plate_detect_ctrl {
    const char *action;             // "continue", "stop"
} fnet_plate_detect_ctrl_t;

typedef struct fnet_first_layer_detect_ctrl {
    const char *action;             // "continue", "stop"
} fnet_first_layer_detect_ctrl_t;

typedef struct fnet_camera_stream_ctrl {
    const char *action;             // "open", "close"
} fnet_camera_stream_ctrl_t;

typedef struct fnet_matl_station_config {
    int slotId;
    const char *materialName;
    const char *materialColor;
} fnet_matl_station_config_t;

typedef struct fnet_indep_matl_config {
    const char *materialName;
    const char *materialColor;
} fnet_indep_matl_config_t;

typedef struct fnet_lan_dev_info {
    char serialNumber[MAX_DEVICE_SN_LEN];
    char name[MAX_DEVICE_NAME_LEN];
    char ip[16];
    unsigned short port;
    unsigned short vid;
    unsigned short pid;
    unsigned short connectMode;     // 0 lan mode, 1 wan mode
    unsigned short bindStatus;      // 0 unbound, 1 bound
} fnet_lan_dev_info_t;

typedef struct fnet_file_data {
    char *data;
    unsigned int size;
} fnet_file_data_t;

typedef struct fnet_token_data {
    int expiresIn;
    char *accessToken;
    char *refreshToken;
} fnet_token_data_t;

typedef struct fnet_client_token_data {
    int expiresIn;
    char *accessToken;
} fnet_client_token_data_t;

typedef struct fnet_user_profile {
    char *uid;
    char *nickname;
    char *headImgUrl;
} fnet_user_profile_t;

typedef struct fnet_wan_dev_bind_data {
    char *devId;
    char *serialNumber;
    char *nimAccountId;
} fnet_wan_dev_bind_data_t;

typedef struct fnet_wan_dev_info {
    char *devId;
    char *name;
    char *model;
    char *imageUrl;
    char *status;               // "ready", "busy", "calibrate_doing", "error", "heating", "printing", "pausing", "pause", "canceling", "cancel", "completed"
    char *location;
    char *serialNumber;
    char *nimAccountId;
} fnet_wan_dev_info_t;

typedef struct fnet_dev_product {
    int nozzleTempCtrlState;    // 0 disabled, 1 enabled
    int chamberTempCtrlState;
    int platformTempCtrlState;
    int lightCtrlState;
    int internalFanCtrlState;
    int externalFanCtrlState;
} fnet_dev_product_t;

typedef struct fnet_matl_slot_info {
    int slotId;
    int hasFilament;            // 1 true, 0 false
    char *materialName;
    char *materialColor;
} fnet_matl_slot_info_t;

typedef struct fnet_matl_station_info {
    int slotCnt;
    int currentSlot;
    int currentLoadSlot;
    int stateAction;            // 0 idle, 1 load filament, 2 unload filament, 3 cancel load/unload filament, 4 printing, 5 busy, 6 paused
    int stateStep;              // 0 undefined, 1 heating, 2 push filament, 3 purge old filament, 4 cut offf filament, 5 retract filament, 6 complete (load/undload filement)
    fnet_matl_slot_info_t *slotInfos;
} fnet_matl_station_info_t;

typedef struct fnet_indep_matl_info {
    int stateAction;            // 0 idle, 1 load filament, 2 unload filament, 3 cancel load/unload filament, 4 printing, 5 busy, 6 paused
    int stateStep;              // 0 undefined, 1 heating, 2 push filament, 3 purge old filament, 4 cut offf filament, 5 retract filament, 6 complete (load/undload filement)
    char *materialName;
    char *materialColor;
} fnet_indep_matl_info_t;

typedef struct fnet_dev_detail {
    char *protocolVersion;
    int pid;
    int nozzleCnt;
    int nozzleStyle;            // 0 independent, 1 non-independent
    char *measure;
    char *nozzleModel;
    char *firmwareVersion;
    char *macAddr;
    char *ipAddr;
    char *name;
    int lidar;                  // 1 enable, 2 disable, 0 unknown
    int camera;                 // 1 enable, 2 disable, 0 unknown
    char *location;
    char *status;               // "ready", "busy", "calibrate_doing", "error", "heating", "printing", "pausing", "pause", "canceling", "cancel", "completed"
    double coordinate[3];       // mm
    char *jobId;
    char *printFileName;
    char *printFileThumbUrl;
    int printLayer;
    int targetPrintLayer;
    double printProgress;       // [0.0, 1.0]
    double rightTemp;
    double rightTargetTemp;
    double leftTemp;
    double leftTargetTemp;
    double platTemp;
    double platTargetTemp;
    double chamberTemp;
    double chamberTargetTemp;
    double fillAmount;          // percent
    double zAxisCompensation;   // mm
    char *rightFilamentType;
    char *leftFilamentType;
    double currentPrintSpeed;   // mm/s
    double printSpeedAdjust;    // percent
    double printDuration;       // second
    double estimatedTime;       // second
    double estimatedRightLen;   // mm
    double estimatedLeftLen;    // mm
    double estimatedRightWeight;// mm
    double estimatedLeftWeight; // mm
    double coolingFanSpeed;     // percent
    double coolingFanLeftSpeed; // percent
    double chamberFanSpeed;     // percent
    int hasRightFilament;       // 1 true, 0 false
    int hasLeftFilament;        // 1 true, 0 false
    int hasMatlStation;         // 1 true, 0 false
    fnet_matl_station_info_t matlStationInfo;
    fnet_indep_matl_info_t indepMatlInfo;
    char *internalFanStatus;    // "open", "close"
    char *externalFanStatus;    // "open", "close"
    char *clearFanStatus;       // "open", "close"
    char *doorStatus;           // "open", "close"
    char *lightStatus;          // "open", "close"
    char *autoShutdown;         // "open", "close"
    double autoShutdownTime;    // minute
    double tvoc;
    double remainingDiskSpace;  // GB
    double cumulativePrintTime; // minute
    double cumulativeFilament;  // mm
    char *cameraStreamUrl;
    char *polarRegisterCode;
    char *flashRegisterCode;
    char *errorCode;
} fnet_dev_detail_t;

typedef struct fnet_gcode_tool_data {
    int toolId;
    int slotId;
    char *materialName;
    char *materialColor;
    double filemanetWeight;     // gram
} fnet_gcode_tool_data_t;

typedef struct fnet_gcode_data {
    char *fileName;
    char *thumbUrl;             // Wan only
    double printingTime;        // second
    double totalFilamentWeight; // gram
    int useMatlStation;         // 1 true, 0 false
    int gcodeToolCnt;
    fnet_gcode_tool_data_t *gcodeToolDatas;
} fnet_gcode_data_t;

typedef struct fnet_time_lapse_video_data {
    char *jobId;
    char *fileName;
    char *videoUrl;
    char *thumbUrl;
    int width;
    int height;
} fnet_time_lapse_video_data_t;

typedef struct fnet_add_job_result {
    char *jobId;
    char *thumbUrl;
} fnet_add_job_result_t;

typedef struct fnet_clound_gcode_data {
    char *bucketName;
    char *endpoint;
    char *gcodeStorageKey;
    char *gcodeStorageUrl;
    char *thumbStorageKey;
    char *thumbStorageUrl;
} fnet_clound_gcode_data_t;

typedef struct fnet_add_clound_job_result {
    fnet_add_clound_job_error_t error;
    char *devId;
    char *jobId;
} fnet_add_clound_job_result_t;

typedef struct fnet_nim_data {
    char *nimDataId;
    char *appNimAccountId;
    char *nimTeamId;
} fnet_nim_data_t;

typedef struct fnet_conn_read_data {
    fnet_conn_read_data_type_t type;
    void *data;                 // call fnet_freeXXXX to release
    char *nimAccountId;         // call fnet_freeString to release
} fnet_conn_read_data_t;

#pragma pack(pop)


#define FNET_OK 0
#define FNET_ERROR -1
#define FNET_ABORTED_BY_CALLBACK 1
#define FNET_DIVICE_IS_BUSY 2
#define FNET_GCODE_NOT_FOUND 3
#define FNET_VERIFY_LAN_DEV_FAILED 1001 // invalid serialNumber/checkCode
#define FNET_UNAUTHORIZED 2001          // invalid accessToken/clientAccessToken
#define FNET_INVALID_VALIDATION 2002    // invalid userName/password/SMSCode
#define FNET_DEVICE_HAS_BEEN_BOUND 2003
#define FNET_NIM_SEND_ERROR 3001
#define FNET_NIM_DATA_BASE_ERROR 3002

#ifdef __cplusplus
extern "C" {
#endif

FNET_API int fnet_initlize(const char *serverSettingsPath, const fnet_log_settings_t *logSettings);

FNET_API void fnet_uninitlize();

FNET_API const char *fnet_getVersion(); // 2.0.1

FNET_API int fnet_getLanDevList(fnet_lan_dev_info_t **infos, int *devCnt, int msWaitTime);

FNET_API void fnet_freeLanDevInfos(fnet_lan_dev_info_t *infos);

FNET_API int fnet_getLanDevProduct(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, fnet_dev_product_t **product, int msTimeout);

FNET_API void fnet_freeDevProduct(fnet_dev_product_t *product);

FNET_API int fnet_getLanDevDetail(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, fnet_dev_detail_t **detail, int msTimeout);

FNET_API void fnet_freeDevDetail(fnet_dev_detail_t *detail);

FNET_API int fnet_getLanDevGcodeList(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, fnet_gcode_data_t **gcodeDatas, int *gcodeCnt, int msTimeout);

FNET_API void fnet_freeGcodeList(fnet_gcode_data_t *gcodeDatas, int gcodeCnt);

FNET_API int fnet_getLanDevGcodeThumb(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, const char *fileName, fnet_file_data_t **fileData, int msTimeout);

FNET_API int fnet_lanDevStartJob(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, const fnet_local_job_data_t *jobData, int msTimeout);

FNET_API int fnet_ctrlLanDevTemp(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, const fnet_temp_ctrl_t *tempCtrl, int msTimeout);

FNET_API int fnet_ctrlLanDevLight(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, const fnet_light_ctrl_t *lightCtrl, int msTimeout);

FNET_API int fnet_ctrlLanDevAirFilter(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, const fnet_air_filter_ctrl_t *airFilterCtrl, int msTimeout);

FNET_API int fnet_ctrlLanDevClearFan(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, const fnet_clear_fan_ctrl_t *clearFanCtrl, int msTimeout);

FNET_API int fnet_ctrlLanDevMove(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, const fnet_move_ctrl_t *moveCtrl, int msTimeout);

FNET_API int fnet_ctrlLanDevHoming(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, int msTimeout);

FNET_API int fnet_ctrlLanDevMatlStation(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, const fnet_matl_station_ctrl_t *matlStationCtrl, int msTimeout);

FNET_API int fnet_ctrlLanDevIndepMatl(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, const fnet_indep_matl_ctrl_t *indepMatlCtrl, int msTimeout);

FNET_API int fnet_ctrlLanDevPrint(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, const fnet_print_ctrl_t *printCtrl, int msTimeout);

FNET_API int fnet_ctrlLanDevJob(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, const fnet_job_ctrl_t *jobCtrl, int msTimeout);

FNET_API int fnet_ctrlLanDevState(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, const fnet_state_ctrl_t *stateCtrl, int msTimeout);

FNET_API int fnet_ctrlLanDevPlateDetect(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, const fnet_plate_detect_ctrl_t *plateDetectCtrl, int msTimeout);

FNET_API int fnet_ctrlLanDevFirstLayerDetect(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, const fnet_first_layer_detect_ctrl_t *firstLayerDetectCtrl, int msTimeout);

FNET_API int fnet_configLanDevMatlStation(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, const fnet_matl_station_config_t *matlStatoinConfig, int msTimeout);

FNET_API int fnet_configLanDevIndepMatl(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, const fnet_indep_matl_config_t *indepMatlConfig, int msTimeout);

FNET_API int fnet_lanDevSendGcode(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, const fnet_send_gcode_data_t *sendGcodeData, int msConnectTimeout);

FNET_API int fnet_notifyLanDevWanBind(const char *ip, unsigned short port, const char *serialNumber,
    int msTimeout);

FNET_API int fnet_downloadFileMem(const char *url, fnet_file_data_t **fileData,
    fnet_progress_callback_t callback, void *callbackData, int msConnectTimeout, int msTimeout);

FNET_API int fnet_downloadFileDisk(const char *url, const char *saveName,
    fnet_progress_callback_t callback, void *callbackData, int msConnectTimeout, int msTimeout);

FNET_API void fnet_freeFileData(fnet_file_data_t *fileData);

FNET_API int fnet_getTokenByPassword(const char *userName, const char *password, const char *language,
    fnet_token_data_t **tokenData, char **message, int msTimeout); // en/zh/de/fr/es/ja/ko, call fnet_freeString to release message

FNET_API int fnet_refreshToken(const char *refreshToken, const char *language,
    fnet_token_data_t **tokenData, char **message, int msTimeout);

FNET_API void fnet_freeToken(fnet_token_data_t *tokenData);

FNET_API int fnet_getClientToken(const char *language, fnet_client_token_data_t **clientTokenData,
    char **message, int msTimeout);

FNET_API void fnet_freeClientToken(fnet_client_token_data_t *clientTokenData);

FNET_API int fnet_sendSMSCode(const char *clientAccessToken, const char *phoneNumber,
    const char *language, char **message, int msTimeout);

FNET_API int fnet_getTokenBySMSCode(const char *userName, const char *SMSCode, const char *language,
    fnet_token_data_t **tokenData, char **message, int msTimeout);

FNET_API int fnet_checkToken(const char *accessToken, int msTimeout);

FNET_API int fnet_signOut(const char *accessToken, int msTimeout);

FNET_API int fnet_getUserProfile(const char *accessToken, fnet_user_profile_t **profile,
    int msTimeout);

FNET_API void fnet_freeUserProfile(fnet_user_profile_t *profile);

FNET_API int fnet_bindWanDev(const char *uid, const char *accessToken, const char *serialNumber,
    unsigned short pid, const char *name, fnet_wan_dev_bind_data_t **bindData, int msTimeout);

FNET_API void fnet_freeBindData(fnet_wan_dev_bind_data_t *bindData);

FNET_API int fnet_unbindWanDev(const char *uid, const char *accessToken, const char *devId,
    int msTimeout);

FNET_API int fnet_getWanDevList(const char *uid, const char *accessToken, fnet_wan_dev_info_t **infos,
    int *devCnt, int msTimeout);

FNET_API void fnet_freeWanDevList(fnet_wan_dev_info_t *infos, int devCnt);

FNET_API int fnet_getWanDevProductDetail(const char *uid, const char *accessToken, const char *devId,
    fnet_dev_product_t **product, fnet_dev_detail_t **detail, int msTimeout);

FNET_API int fnet_getWanDevGcodeList(const char *uid, const char *accessToken, const char *devId,
    fnet_gcode_data_t **gcodeDatas, int *gcodeCnt, int msTimeout);

FNET_API int fnet_getWanDevTimeLapseVideoList(const char *uid, const char *accessToken, const char *devId,
    int maxVideoCnt, fnet_time_lapse_video_data_t **videoDatas, int *videoCnt, int msTimeout);

FNET_API void fnet_freeTimeLapseVideoList(fnet_time_lapse_video_data_t *videoDatas, int videoCnt);

FNET_API int fnet_deleteTimeLapseVideo(const char *uid, const char *accessToken, const char **jobIds,
    int jobCnt, int msTimeout);

FNET_API int fnet_wanDevAddJob(const char *uid, const char *accessToken, const char *devId,
    const fnet_local_job_data_t *jobData, fnet_add_job_result_t **result, int msTimeout);

FNET_API void fnet_freeAddJobResult(fnet_add_job_result_t *result);

FNET_API int fnet_wanDevSendGcodeClound(const char *uid, const char *accessToken,
    const fnet_send_gcode_data_t *sendGcodeData, fnet_clound_gcode_data_t **cloundGcodeData, int msTimeout);

FNET_API void fnet_freeCloundGcodeData(fnet_clound_gcode_data_t *cloundGcodeData);

FNET_API int fnet_wanDevAddCloundJob(const char *uid, const char *accessToken,
    const fnet_clound_job_data_t *jobData, fnet_add_clound_job_result_t **results, int *resultCnt, int msTimeout);

FNET_API void fnet_freeAddCloudJobResults(fnet_add_clound_job_result_t *results, int resultCnt);

FNET_API int fnet_getNimData(const char *uid, const char *accessToken, fnet_nim_data_t **nimData,
    int msTimeout);

FNET_API void fnet_freeNimData(fnet_nim_data_t *nimData);

FNET_API int fnet_initlizeNim(const char *nimDataId, const char *appDataDir);

FNET_API void fnet_uninitlizeNim();

FNET_API int fnet_createConnection(void **conn, const fnet_conn_settings_t *settings);

FNET_API void fnet_freeConnection(void *conn);

FNET_API int fnet_connectionSend(void *conn, const fnet_conn_write_data_t *writeData);

FNET_API int fnet_connectionSubscribe(void *conn, const fnet_conn_subscribe_data_t *subscribeData);

FNET_API int fnet_connectionUnsubscribe(void *conn, const fnet_conn_unsubscribe_data_t *unsubscribeData);

FNET_API void fnet_freeString(char *str);

#ifdef __cplusplus
}
#endif

#endif // !_FLASHNETWORK_H_
