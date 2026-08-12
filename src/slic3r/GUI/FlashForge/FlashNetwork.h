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
#define DAT_FILE_NAME "FLASHNETWORK9.DAT"

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
    FNET_CONN_STATUS_CONNECTED,
    FNET_CONN_STATUS_DISCONNECTED,
} fnet_conn_status_t;

typedef enum fnet_conn_write_data_type {
    FNET_CONN_WRITE_SYNC_LOGIN,         // data, char *clinetId
    FNET_CONN_WRITE_UPDATE_DETAIL,      // data, nullptr
    FNET_CONN_WRITE_START_JOB,          // data, fnet_local_job_data_t
    FNET_CONN_WRITE_START_CLOUND_JOB,   // data, fnet_clound_job_data_t
    FNET_CONN_WRITE_TEMP_CTRL,          // data, fnet_temp_ctrl_t
    FNET_CONN_WRITE_LIGHT_CTRL,         // data, fnet_light_ctrl_t
    FNET_CONN_WRITE_AIR_FILTER_CTRL,    // data, fnet_air_filter_ctrl_t
    FNET_CONN_WRITE_CLEAR_FAN_CTRL,     // data, fnet_clear_fan_ctrl_t
    FNET_CONN_WRITE_MOVE_CTRL,          // data, fnet_move_ctrl_t
    FNET_CONN_WRITE_EXTRUDE_CTRL,       // data, fnet_extrude_ctrl_t
    FNET_CONN_WRITE_HOMING_CTRL,        // data, nullptr
    FNET_CONN_WRITE_MATL_STATION_CTRL,  // data, fnet_matl_station_ctrl_t
    FNET_CONN_WRITE_INDEP_MATL_CTRL,    // data, fnet_indep_matl_ctrl_t
    FNET_CONN_WRITE_PRINT_CTRL,         // data, fnet_print_ctrl_t
    FNET_CONN_WRITE_JOB_CTRL,           // data, fnet_job_ctrl_t
    FNET_CONN_WRITE_STATE_CTRL,         // data, fnet_state_ctrl_t
    FNET_CONN_WRITE_ERROR_CODE_CTRL,    // data, fnet_error_code_ctrl_t
    FNET_CONN_WRITE_PLATE_DETECT_CTRL,  // data, fnet_plate_detect_ctrl_t
    FNET_CONN_WRITE_FIRST_LAYER_DETECT_CTRL, // data, fnet_first_layer_detect_ctrl_t
    FNET_CONN_WRITE_CAMERA_STREAM_CTRL, // data, fnet_camera_stream_ctrl_t
    FNET_CONN_WRITE_MATL_STATION_CONFIG,// data, fnet_matl_station_config_t
    FNET_CONN_WRITE_INDEP_MATL_CONFIG,  // data, fnet_indep_matl_config_t
} fnet_conn_write_data_type_t;

typedef enum fnet_conn_read_data_type {
    FNET_CONN_READ_SYS_NOTIFY,          // data, char *payload
    FNET_CONN_READ_UPDATE_NOTIFY,       // data, chat *payload
    FNET_CONN_READ_SYNC_USER_PROFILE,   // data, nullptr
    FNET_CONN_READ_SYNC_UNREGISTER_USER,// data, nullptr
    FNET_CONN_READ_SYNC_LOGIN,          // data, fnet_sync_login_info_t
    FNET_CONN_READ_SYNC_BIND_DEVICE,    // data, fnet_sync_bind_info_t
    FNET_CONN_READ_SYNC_UNBIND_DEVICE,  // data, fnet_sync_bind_info_t
    FNET_CONN_READ_SYNC_ONLINE,         // data, fnet_sync_online_info_t
    FNET_CONN_READ_SYNC_OFFLINE,        // data, fnet_sync_online_info_t
    FNET_CONN_READ_DEVICE_DETAIL,       // data, fnet_dev_detail_t
    FNET_CONN_READ_DEVICE_KEEP_ALIVE,   // data, char *devId
    FNET_CONN_READ_SLICE_STATE,         // data, fnet_slice_state_t
    FNET_CONN_READ_JOB_DOWNLOAD,        // data, fnet_job_info_t
    FNET_CONN_READ_JOB_UNZIP,           // data, fnet_job_info_t
    FNET_CONN_READ_UPLOAD_LOG,          // data, fnet_upload_log_info_t
} fnet_conn_read_data_type_t;

struct fnet_conn_read_data;
typedef struct fnet_conn_read_data fnet_conn_read_data_t;

// returning a non-zero value from the callback aborts the transfer
typedef int (*fnet_progress_callback_t)(long long now, long long total, void *data);
// settingJson is valid only for the duration of the callback and contains one
// complete preset object using the OrcaSlicer setting_id/name/type keys.
typedef int (*fnet_setting_check_callback_t)(const char *settingJson, void *data);

typedef void (*fnet_setting_progress_callback_t)(int progress, void *data);

// Return non-zero to cancel a setting-list transfer.
typedef int (*fnet_setting_cancel_callback_t)(void *data);


typedef void (*fnet_conn_status_callback_t)(fnet_conn_status_t status, void *data);

// return zero for success, non-zero for failure.
// clientId storage with fnet_allocString, this will then be free later by the library
typedef int (*fnet_conn_update_callback_t)(const char **clientId, void *data);

// call fnet_freeXXXX to release readData->data, call fnet_freeString to release readData->devId
typedef void (*fnet_conn_read_callback_t)(fnet_conn_read_data_t *readData, void *data);

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
    const char *unionId;
    int plateNo;
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

typedef struct fnet_upload_file_data {
    const char *filePath;
    const char *saveName;
    fnet_progress_callback_t callback;
    void *callbackData;
} fnet_upload_file_data_t;

typedef struct fnet_upload_log_info {
    char *feedbackId;
    char *clientType;
} fnet_upload_log_info_t;

typedef struct fnet_start_ai_model_job_data {
    int supplier;
    long long pipelineId;
    const char *imageUrl;
    const char *resultFormat;
} fnet_start_ai_model_job_data_t;

typedef struct fnet_start_ai_general_job_data {
    int supplier;
    long long pipelineId;
    const char *prompt;
    const char *imageUrl;
} fnet_start_ai_general_job_data_t;

typedef struct fnet_report_model_data {
    int selectedOptionId;
    const char *modelId;
    const char *extraMessage;
} fnet_report_model_data_t;

typedef struct fnet_tracking_common_data {
    const char *uid;
    const char *did;
    const char *sid;
    const char *netType;
    const char *oper;
    const char *ext;
} fnet_tracking_common_data_t;

typedef struct fnet_tracking_event_data {
    const char *eventType;
    const char *eventId;
    const char *eventName;
    const char *pageId;
    const char *moduleId;
    const char *reqId;
    const char *expIds;
    const char *objectType;
    const char *objectId;
    const char *extend;
    const char *timestamp;
} fnet_tracking_event_data_t;

typedef struct fnet_conn_settings {
    const char *clientId;
    fnet_conn_status_callback_t statusCallback;
    void *statusCallbackData;
    fnet_conn_update_callback_t updateCallback;
    void *updateCallbackData;
    fnet_conn_read_callback_t readCallback;
    void *readCallbackData;
} fnet_conn_settings_t;

typedef struct fnet_conn_write_data {
    fnet_conn_write_data_type_t type;
    const void *data;
    const char *topic;
    int qos;
} fnet_conn_write_data_t;

typedef struct fnet_conn_write_multi_data {
    fnet_conn_write_data_type_t type;
    const void **datas;
    const char **topics;
    int dataCnt;                    // the value must be 1 or the same as topicCnt
    int topicCnt;                   // the value must be 1 or the same as dataCnt
    int qos;
} fnet_conn_write_multi_data_t;

typedef struct fnet_conn_subscribe_data {
    const char **topics;
    int topicCnt;                   // [1, 100]
} fnet_conn_subscribe_data_t;

typedef struct fnet_temp_ctrl {
    double platformTemp;
    double rightTemp;
    double leftTemp;
    double chamberTemp;
    double *nozzlesTemp;
    int nozzlesCnt;
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
    const char *axis;               // "x", "y", "z"
    double delta;
} fnet_move_ctrl_t;

typedef struct fnet_extrue_ctrl {
    const char *axis;               // "e"
    double delta;
} fnet_extrude_ctrl_t;

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

typedef struct fnet_error_code_ctrl {
    const char *action;             // "clearErrorCode"
    const char *errorCode;
} fnet_error_code_ctrl_t;

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
    unsigned short bindType;        // 0 old, 1 mqtt
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

typedef struct fnet_user_profile {
    char *uid;
    char *nickname;
    char *headImgUrl;
    char *email;
} fnet_user_profile_t;

typedef struct fnet_wan_dev_bind_data {
    char *devId;
    char *serialNumber;
} fnet_wan_dev_bind_data_t;

typedef struct fnet_update_info {
    char *status;
    char *title;
    char *content;
    char *tips;
} fnet_update_info_t;

typedef struct fnet_wan_dev_info {
    char *devId;
    char *name;
    char *model;
    char *imageUrl;
    char *status;               // "ready", "busy", "calibrate_doing", "error", "heating", "printing", "pausing", "pause", "canceling", "cancel", "completed"
    char *location;
    char *serialNumber;
    char *devTopic;
    fnet_update_info_t updateInfo;
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
    char *devId;
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
    int moveCtrl;               // 1 enable, 2 disable, 0 unknown
    int extrudeCtrl;            // 1 enable, 2 disable, 0 unknown
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
    double *nozzleTemps;
    double *nozzleTargetTemps;
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
    double cumulativePrintTimeSecond; // second
    int hasCumulativePrintTimeSecond; // 1 if reported, 0 if unavailable
} fnet_dev_detail_t;

typedef struct fnet_slice_state {
    char* id;
    char* deviceId;
    char* status;
    int waitingCount;
    int percentage;
    int timestamp;
    char* fileName;
    char* thumbImagePath;
    double weight;
} fnet_slice_state_t;

typedef struct fnet_job_info {
    char* jobId;
    char* deviceId;
    char* status;
    int percentage;
    char* errorCode;
} fnet_job_info_t;

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

typedef struct fnet_bind_account_relp_result {
    int showUserPoints;
} fnet_bind_account_relp_result_t;

typedef struct fnet_clound_file_data {
    char *bucketName;
    char *endpoint;
    char *storageKey;
    char *storageUrl;
} fnet_clound_file_data_t;

typedef struct fnet_user_ai_points_info {
    int totalPoints;
    int modelGenPoints;
    int img2imgPoints;
    int txt2txtPoints;
    int txt2imgPoints;
    int remainingFreeCount;
    int freeRetriesPerProcess;
} fnet_user_ai_points_info_t;

typedef struct fnet_ai_job_pipeline_info {
    long long id;
    int isFree;
} fnet_ai_job_pipeline_info_t;

typedef struct fnet_start_ai_model_job_result {
    int status;                 // 0 waiting, 1 running, 2 failed, 3 done, 4 cancelled
    long long jobId;
    int posInQueue;
    int queueLength;
    int isOldJob;
} fnet_start_ai_model_job_result_t;

typedef struct fnet_ai_model_data {
    const char *modelType;
    const char *modelUrl;
} fnet_ai_model_data_t;

typedef struct fnet_ai_model_job_state {
    int status;                 // 0 waiting, 1 running, 2 failed, 3 done, 4 cancelled
    long long jobId;
    int posInQueue;
    int queueLength;
    int modelCnt;
    fnet_ai_model_data_t *models;
    const char *externalJobId;
} fnet_ai_model_job_state_t;

typedef struct fnet_start_ai_general_job_result {
    int status;                 // 0 waiting, 1 running, 2 failed, 3 done, 4 cancelled
    long long jobId;
    int posInQueue;
    int queueLength;
    int remainingFreeRetries;
} fnet_start_ai_general_job_result_t;

typedef struct fnet_ai_general_job_data {
    const char *content;
    const char *imageUrl;
} fnet_ai_general_job_data_t;

typedef struct fnet_ai_general_job_state {
    int status;                 // 0 waiting, 1 running, 2 failed, 3 done, 4 cancelled
    long long jobId;
    int posInQueue;
    int queueLength;
    int dataCnt;
    fnet_ai_general_job_data_t *datas;
    const char *externalJobId;
} fnet_ai_general_job_state_t;

typedef struct fnet_sys_msg_data {
    int id;
    int messageId;
    const char* notifyType;
    const char* title;
    const char* content;
    int linkType;
    const char* linkUrl;
    int duration;
    int showCount;
} fnet_sys_msg_data_t;

typedef struct fnet_mqtt_config {
    char *userTopic;
    char **commonTopics;
    int commonTopicCnt;
} fnet_mqtt_config_t;

typedef struct fnet_conn_write_multi_result {
    int *failedIndices;
    int failedCnt;
} fnet_conn_write_multi_result_t;

typedef struct fnet_sync_login_info {
    char *clientType;
    char *clientId;
} fnet_sync_login_info_t;

typedef struct fnet_sync_bind_info {
    char *fromClinetId;
    char *devId;
} fnet_sync_bind_info_t;

typedef struct fnet_sync_online_info {
    char *uid;
    char *devId;
} fnet_sync_online_info_t;

typedef struct fnet_conn_read_data {
    fnet_conn_read_data_type_t type;
    void *data;                 // call fnet_freeXXXX to release
    char *topic;                // call fnet_freeString to release
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
#define FNET_ABORT_AI_JOB_FAILED 2004
#define FENT_AI_JOB_NOT_ENOUGH_POINTS 2005
#define FNET_NO_EXISTING_AI_MODEL_JOB 2006
#define FNET_INPUT_FAILED_THE_REVIEW 2007
#define FNET_PRINT_LIST_MODEL_COUNT_EXCEEDED 2008
#define FNET_CONN_SEND_ERROR 3001
#define FNET_TIMEOUT 3002

#ifdef __cplusplus
extern "C" {
#endif
#define FNET_INVALID_PARAMETER 4001
#define FNET_GET_SETTING_LIST_FAILED 4002
#define FNET_PUT_SETTING_FAILED 4003
#define FNET_DELETE_SETTING_FAILED 4004

// FlashForge preset service business codes. The HTTP status is returned separately.
#define FNET_PRESET_UNSUPPORTED_TYPE 120000
#define FNET_PRESET_SETTING_ALREADY_EXISTS 120001
#define FNET_PRESET_NAME_ALREADY_EXISTS 120002
#define FNET_PRESET_NOT_FOUND 120003
#define FNET_PRESET_HAS_ACTIVE_CHILDREN 120004
#define FNET_PRESET_CREATE_CONFLICT 120005
#define FNET_PRESET_UPDATE_CONFLICT 120006

FNET_API int fnet_initlize(const char *serverSettingsPath, const fnet_log_settings_t *logSettings);

FNET_API void fnet_uninitlize();

FNET_API const char *fnet_getVersion(); // 3.0.0

FNET_API const char* fnet_getHomePageUrl();

FNET_API void fnet_setUserAgent(const char *userAgent);

FNET_API int fnet_getLanDevList(fnet_lan_dev_info_t **infos, int *devCnt, int msWaitTime);

FNET_API void fnet_freeLanDevInfos(fnet_lan_dev_info_t *infos);

FNET_API int fnet_getLanDevProduct(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, fnet_dev_product_t **product, int msTimeout);

FNET_API void fnet_freeDevProduct(fnet_dev_product_t *product);

FNET_API int fnet_getLanDevDetail(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, fnet_dev_detail_t **detail, int msTimeout);

FNET_API void fnet_freeDevDetail(const fnet_dev_detail_t *detail);

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

FNET_API int fnet_ctrlLanDevExtrude(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, const fnet_extrude_ctrl_t *extrudeCtrl, int msTimeout);

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

FNET_API int fnet_ctrlLanDevErrorCode(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, const fnet_error_code_ctrl_t *errorCodeCtrl, int msTimeout);

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

FNET_API int fnet_sendSMSCode(const char *phoneNumber, const char *language, char **message, int msTimeout);

FNET_API int fnet_getTokenBySMSCode(const char *userName, const char *SMSCode, const char *language,
    fnet_token_data_t **tokenData, char **message, int msTimeout);

FNET_API int fnet_signOut(const char *accessToken, int msTimeout);

FNET_API int fnet_getUserProfile(const char *accessToken, fnet_user_profile_t **profile,
    int msTimeout);

FNET_API void fnet_freeUserProfile(fnet_user_profile_t *profile);

FNET_API int fnet_bindWanDev(const char *clientId, const char *accessToken, const char *serialNumber,
    unsigned short pid, const char *name, unsigned short bindType, fnet_wan_dev_bind_data_t **bindData, int msTimeout);

FNET_API void fnet_freeBindData(fnet_wan_dev_bind_data_t *bindData);

FNET_API int fnet_unbindWanDev(const char *clientId, const char *accessToken, const char *devId,
    int msTimeout);

FNET_API int fnet_getWanDevList(const char *clientId, const char *accessToken, long long appId, long long platId,
    fnet_wan_dev_info_t **infos, int *devCnt, int msTimeout);

FNET_API void fnet_freeWanDevList(fnet_wan_dev_info_t *infos, int devCnt);

FNET_API int fnet_getWanDevProductDetail(const char *clientId, const char *accessToken, const char *devId,
    fnet_dev_product_t **product, fnet_dev_detail_t **detail, int msTimeout);

FNET_API int fnet_getWanDevGcodeList(const char *clientId, const char *accessToken, const char *devId,
    fnet_gcode_data_t **gcodeDatas, int *gcodeCnt, int msTimeout);

FNET_API int fnet_getWanDevTimeLapseVideoList(const char *clientId, const char *accessToken, const char *devId,
    int maxVideoCnt, fnet_time_lapse_video_data_t **videoDatas, int *videoCnt, int msTimeout);

FNET_API void fnet_freeTimeLapseVideoList(fnet_time_lapse_video_data_t *videoDatas, int videoCnt);

FNET_API int fnet_deleteTimeLapseVideo(const char *clientId, const char *accessToken, const char **jobIds,
    int jobCnt, int msTimeout);

FNET_API int fnet_wanDevAddJob(const char *clientId, const char *accessToken, const char *devId,
    const fnet_local_job_data_t *jobData, fnet_add_job_result_t **result, int msTimeout);

FNET_API void fnet_freeAddJobResult(fnet_add_job_result_t *result);

FNET_API int fnet_wanDevSendGcodeClound(const char *clientId, const char *accessToken,
    const fnet_send_gcode_data_t *sendGcodeData, fnet_clound_gcode_data_t **cloundGcodeData, int msTimeout);

FNET_API void fnet_freeCloundGcodeData(fnet_clound_gcode_data_t *cloundGcodeData);

FNET_API int fnet_wanDevAddCloundJob(const char *clientId, const char *accessToken,
    const fnet_clound_job_data_t *jobData, fnet_add_clound_job_result_t **results, int *resultCnt, int msTimeout);

FNET_API void fnet_freeAddCloudJobResults(fnet_add_clound_job_result_t *results, int resultCnt);

FNET_API int fnet_bindAccountRelp(const char *clientId, const char *accessToken, const char *email,
    fnet_bind_account_relp_result_t **bindResult, int msTimeout);

FNET_API void fnet_freeBindAccountRelpResult(fnet_bind_account_relp_result_t *bindResult);

FNET_API int fnet_uploadAiImageClound(const char *clientId, const char *accessToken, const char *uid,
    const fnet_upload_file_data_t *uploadFileData, fnet_clound_file_data_t **cloundFileData, int msTimeout);

FNET_API int fnet_uploadLogFileCloud(const char* clientId, const char* accessToken, const fnet_upload_file_data_t* uploadFileData, const char* connectEmail, int msTimeout);

FNET_API int fnet_uploadLogFileCloudEx(const char *clientId, const char *accessToken,
    const fnet_upload_file_data_t *uploadFileData, const char *connectEmail,
    const char *feedbackId, int msTimeout);

FNET_API void fnet_freeCloundFileData(fnet_clound_file_data_t *cloundFileData);

FNET_API int fnet_getUserAiPointsInfo(const char *clientId, const char *accessToken,
    fnet_user_ai_points_info_t **userAiPointsInfo, int msTimeout);

FNET_API void fnet_freeUserAiPointsInfo(fnet_user_ai_points_info_t *userAiPointsInfo);

FNET_API int fnet_createAiJobPipeline(const char *clientId, const char *accessToken, const char *entryType,
    fnet_ai_job_pipeline_info_t **pipelineInfo, int msTimeout);

FNET_API void fnet_freeAiJobPipelineInfo(fnet_ai_job_pipeline_info_t *pipelineInfo);

FNET_API int fnet_startAiModelJob(const char *clientId, const char *accessToken,
    const fnet_start_ai_model_job_data_t *jobData, fnet_start_ai_model_job_result_t **jobResult, int msTimeout);

FNET_API void fnet_freeStartAiModelJobResult(fnet_start_ai_model_job_result_t *jobResult);

FNET_API int fnet_getAiModelJobState(const char *clientId, const char *accessToken, long long jobId,
    fnet_ai_model_job_state_t **jobState, int msTimeout);

FNET_API void fnet_freeAiModelJobState(fnet_ai_model_job_state_t *jobState);

FNET_API int fnet_abortAiModelJob(const char *clientId, const char *accessToken, long long jobId, int msTimeout);

FNET_API int fnet_getExistingAiModelJob(const char *clientId, const char *accessToken,
    fnet_start_ai_model_job_result_t **jobResult, int msTimeout);

FNET_API int fnet_startAiImg2imgJob(const char *uclientIdid, const char *accessToken,
    const fnet_start_ai_general_job_data_t *jobData, fnet_start_ai_general_job_result_t **jobResult, int msTimeout);

FNET_API int fnet_startAiTxt2txtJob(const char *clientId, const char *accessToken,
    const fnet_start_ai_general_job_data_t *jobData, fnet_start_ai_general_job_result_t **jobResult, int msTimeout);

FNET_API int fnet_startAiTxt2imgJob(const char *clientId, const char *accessToken,
    const fnet_start_ai_general_job_data_t *jobData, fnet_start_ai_general_job_result_t **jobResult, int msTimeout);

FNET_API void fnet_freeStartAiGeneralJobResult(fnet_start_ai_general_job_result_t *jobResult);

FNET_API int fnet_getAiImg2imgJobState(const char *clientId, const char *accessToken, long long jobId,
    fnet_ai_general_job_state_t **jobState, int msTimeout);

FNET_API int fnet_getAiTxt2txtJobState(const char *clientId, const char *accessToken, long long jobId,
    fnet_ai_general_job_state_t **jobState, int msTimeout);

FNET_API int fnet_getAiTxt2imgJobState(const char *clientId, const char *accessToken, long long jobId,
    fnet_ai_general_job_state_t **jobState, int msTimeout);

FNET_API void fnet_freeAiGeneralJobState(fnet_ai_general_job_state_t *jobState);

FNET_API int fnet_abortAiImg2imgJob(const char *clientId, const char *accessToken, long long jobId, int msTimeout);

FNET_API int fnet_abortAiTxt2txtJob(const char *clientId, const char *accessToken, long long jobId, int msTimeout);

FNET_API int fnet_abortAiTxt2imgJob(const char *clientId, const char *accessToken, long long jobId, int msTimeout);

FNET_API int fnet_userClickCount(const char *clientId, const char *accessToken, const char *source, int msTimeout);

FNET_API int fnet_addPrintListModel(const char *clientId, const char *accessToken, const char *language,
    const char *modelId, char **message, int msTimeout); // call fnet_freeString to release message

FNET_API int fnet_removePrintListModel(const char *clientId, const char *accessToken, const char *modelId,
    int msTimeout);

FNET_API int fnet_navLikeModel(const char* clientId, const char* accessToken, const char* modelId, int like, int msTimeout);

FNET_API int fnet_reportModel(const char *clientId, const char *accessToken,
    const fnet_report_model_data_t *reportData, int msTimeout);

FNET_API int fnet_reportTrackingData(const char *clientId, const fnet_tracking_common_data_t *commonData,
    const fnet_tracking_event_data_t *eventData, int msTimeout);

FNET_API int fnet_reportTrackingDataBatch(const char *clientId, const fnet_tracking_common_data_t *commonData,
    const fnet_tracking_event_data_t *eventDatas, int eventCnt, int msTimeout);

FNET_API int fnet_getSystemMessage(const char* clientId, const char* accessToken, const char* language, 
    fnet_sys_msg_data_t **responseData, int msTimeout);

FNET_API int fnet_postReadSystemMessage(const char* clientId, const char* accessToken, int id, int msTimeout);

FNET_API void fnet_freeSystemMessage(fnet_sys_msg_data_t* data);

FNET_API int fnet_getMonitorMessage(const char* clientId, const char* accessToken, const char* language,
    fnet_sys_msg_data_t** responseData, int msTimeout);

FNET_API int fnet_retrySliceTask(const char* clientId, const char* accessToken, const char* taskId, int msTimeout);

FNET_API int fnet_cancelSliceTask(const char* clientId, const char* accessToken, const char* taskId, int msTimeout);

FNET_API void fnet_freeSliceState(const fnet_slice_state_t* detail);

FNET_API void fnet_freeJobInfo(const fnet_job_info_t* detail);

FNET_API int fnet_doBusGetRequest(const char *clientId, const char *accessToken, const char *language,
    const char *target, char **responseData, int msTimeout); // call fnet_freeString to release message

FNET_API int fnet_doBusPostRequest(const char *clientId, const char *accessToken, const char *language,
    const char *target, const char *postFields, int *code, int msTimeout);
// Downloads the complete cloud preset JSON file and replaces the in-memory
// cache. bundleVersion is reserved for compatibility with OrcaSlicer's
// network-plugin ABI.
FNET_API int fnet_getSettingList2(const char *clientId, const char *accessToken,
    const char *bundleVersion, fnet_setting_check_callback_t checkCallback, void *checkCallbackData,
    fnet_setting_progress_callback_t progressCallback, void *progressCallbackData,
    fnet_setting_cancel_callback_t cancelCallback, void *cancelCallbackData, int msTimeout);

FNET_API int fnet_getSettingList(const char *clientId, const char *accessToken,
    const char *bundleVersion, fnet_setting_progress_callback_t progressCallback,
    void *progressCallbackData, fnet_setting_cancel_callback_t cancelCallback,
    void *cancelCallbackData, int msTimeout);

// Returns the cache populated by fnet_getSettingList/fnet_getSettingList2 as a
// JSON object keyed by setting_id. Release with fnet_freeString.
FNET_API int fnet_getUserPresets(const char *clientId, const char *accessToken,
    char **userPresetsJson);

// valuesJson must be a JSON object whose values follow OrcaSlicer's string-map
// convention. Create values must contain setting_id and type. Update values must
// contain type and updated_time; updated_time is sent to the server as ISO-8601 UTC.
// Writes are sent to the user-preset add/update endpoints.
// Server updateTime/updatedTime values are returned as Unix epoch millisecond strings.
// Release resultValuesJson with fnet_freeString.
FNET_API int fnet_requestSettingId(const char *clientId, const char *accessToken,
    const char *name, const char *valuesJson, char **settingId, char **resultValuesJson,
    unsigned int *httpCode, int msTimeout);

FNET_API int fnet_putSetting(const char *clientId, const char *accessToken,
    const char *settingId, const char *name, const char *valuesJson,
    char **resultValuesJson, unsigned int *httpCode, int msTimeout);

FNET_API int fnet_deleteSetting(const char *clientId, const char *accessToken,
    const char *settingId, unsigned int *httpCode, int msTimeout);


FNET_API int fnet_getMqttConfig(const char *clientId, const char *accessToken, fnet_mqtt_config_t **mqttConfig,
    int msTimeout);

FNET_API void fnet_freeMqttConfig(fnet_mqtt_config_t *mqttConfig);

FNET_API int fnet_createConnection(void **conn, const fnet_conn_settings_t *settings);

FNET_API void fnet_freeConnection(void *conn);

FNET_API void fnet_connectionStop(void *conn);

FNET_API int fnet_connectionSend(void *conn, const fnet_conn_write_data_t *writeData);

FNET_API int fnet_connectionSendMulti(void *conn, const fnet_conn_write_multi_data_t *writeData,
    fnet_conn_write_multi_result_t **writeResult);

FNET_API int fnet_connectionSubscribe(void *conn, const fnet_conn_subscribe_data_t *subscribeData);

FNET_API int fnet_connectionUnsubscribe(void *conn, const fnet_conn_subscribe_data_t *subscribeData);

FNET_API void fnet_freeWriteMultiResult(const fnet_conn_write_multi_result_t *writeResult);

FNET_API void fnet_freeSyncLoginInfo(const fnet_sync_login_info_t *loginInfo);

FNET_API void fnet_freeSyncBindInfo(const fnet_sync_bind_info_t *bindInfo);

FNET_API void fnet_freeSyncOnlineInfo(const fnet_sync_online_info_t *onlineInfo);

FNET_API void fnet_freeUploadLogInfo(const fnet_upload_log_info_t *uploadLogInfo);

FNET_API char *fnet_allocString(const char *str, int len);

FNET_API void fnet_freeString(const char *str);

#ifdef __cplusplus
}
#endif

#endif // !_FLASHNETWORK_H_
