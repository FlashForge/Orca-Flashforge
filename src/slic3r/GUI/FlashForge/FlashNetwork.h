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

struct fnet_conn_read_data;
typedef struct fnet_conn_read_data fnet_conn_read_data_t;

// returning a non-zero value from the callback aborts the transfer
typedef int (*fnet_progress_callback_t)(long long now, long long total, void *data);

// returning a non-zero value from the callback stop the event loop
// the corresponding free function needs to be called to release the readData->data and readData->devId.
typedef int (*fnet_conn_read_callback_t)(fnet_conn_read_data_t *readData, void *data);

// after the connection is reconnected, you need to resubscribe to the fnet_conn_read_data_t in the callback
typedef void (*fnet_conn_reconnect_callback_t)(void *data);

#pragma pack(push, 4)

typedef enum fnet_log_level {
    FNET_LOG_LEVEL_OFF,
    FNET_LOG_LEVEL_ERROR,
    FNET_LOG_LEVEL_WARN,
    FNET_LOG_LEVEL_INFO,
    FNET_LOG_LEVEL_DEBUG,
} fnet_log_level_t;

typedef enum fnet_clound_job_error_type {
    FNET_CLOUND_JOB_DEVICE_BUSY,
    FNET_CLOUND_JOB_DEVICE_NOT_FOUND,
    FNET_CLOUND_JOB_SERVER_INTERNAL_ERROR,
    FNET_CLOUND_JOB_UNKNOWN_ERROR,
} fnet_clound_job_error_type_t;

typedef enum fnet_conn_write_data_type {
    FNET_CONN_WRITE_SUB_DEVICE_ACTION,  // data, nullptr
    FNET_CONN_WRITE_SUB_APP_SLICER_SYNC,// data, fnet_user_id_t
    FNET_CONN_WRITE_SYNC_SLICER_LOGIN,  // data, fnet_user_id_t
    FNET_CONN_WRITE_SYNC_BIND_DEVICE,   // data, fnet_user_id_t
    FNET_CONN_WRITE_SYNC_UNBIND_DEVICE, // data, fnet_user_id_t
    FNET_CONN_WRITE_TEMP_CTRL,          // data, fnet_temp_ctrl_t
    FNET_CONN_WRITE_LIGHT_CTRL,         // data, fnet_light_ctrl_t
    FNET_CONN_WRITE_AIR_FILTER_CTRL,    // data, fnet_air_filter_ctrl_t
    FNET_CONN_WRITE_PRINT_CTRL,         // data, fnet_print_ctrl_t
    FNET_CONN_WRITE_JOB_CTRL,           // data, fnet_job_ctrl_t
    FNET_CONN_WRITE_CAMERA_STREAM_CTRL, // data, fnet_camera_stream_ctrl_t
} fnet_conn_write_data_type_t;

typedef enum fnet_conn_read_data_type {
    FNET_CONN_READ_SYNC_SLICER_LOGIN,   // data, nullptr
    FNET_CONN_READ_SYNC_USER_PROFILE,   // data, nullptr
    FNET_CONN_READ_SYNC_BIND_DEVICE,    // data, nullptr
    FNET_CONN_READ_SYNC_UNBIND_DEVICE,  // data, nullptr
    FNET_CONN_READ_UNREGISTER_USER,     // data, nullptr
    FNET_CONN_READ_DEVICE_DETAIL,       // data, fnet_dev_detail_t
    FNET_CONN_READ_DEVICE_OFFLINE,      // data, nullptr
} fnet_conn_read_data_type_t;

typedef struct fnet_log_settings {
    const char *fileDir;
    int expireHours;
    fnet_log_level_t level;
} fnet_log_settings_t;

typedef struct fnet_send_gcode_data {
    const char *gcodeFilePath;          // utf-8
    const char *thumbFilePath;          // utf-8, wan only
    const char *gcodeDstName;           // utf-8
    int printNow;                       // 1 true, 0 false
    int levelingBeforePrint;            // 1 true, 0 false
    fnet_progress_callback_t callback;
    void *callbackData;
} fnet_send_gcode_data_t;

typedef struct fnet_clound_job_data {
    const char **devIds;
    int devCnt;
    const char *gcodeName;
    const char *gcodeType; // 3mf
    const char *gcodeMd5;
    long long gcodeSize;
    const char *thumbName;
    const char *thumbType; // png
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
} fnet_clound_job_data_t;

typedef struct fnet_conn_settings {
    fnet_conn_read_callback_t readCallback;
    void *readCallbackData;
    fnet_conn_reconnect_callback_t reconnectCallback;
    void *reconnectCallbackData;
    int maxReconnectCnt;
    int maxErrorCnt;
    int msTimeout;
} fnet_conn_settings_t;

typedef struct fnet_dev_ids {
    const char **ids;
    int cnt;
} fnet_dev_ids_t;

typedef struct fnet_conn_write_data {
    fnet_conn_write_data_type_t type;
    const void *data;
    fnet_dev_ids_t devIds;
} fnet_conn_write_data_t;

typedef struct fnet_user_id {
    const char *uid;
} fnet_user_id_t;

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

typedef struct fnet_print_ctrl {
    double zAxisCompensation;       // mm
    double printSpeedAdjust;        // percent
    double coolingFanSpeed;         // percent
    double chamberFanSpeed;         // percent
} fnet_print_ctrl_t;

typedef struct fnet_job_ctrl {
    const char *jobId;
    const char *action;             // "pause", "continue", "cancel"
} fnet_job_ctrl_t;

typedef struct fnet_camera_stream_ctrl {
    const char *action;             // "open", "close"
} fnet_camera_stream_ctrl_t;

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
} fnet_wan_dev_bind_data_t;

typedef struct fnet_wan_dev_info {
    char *devId;
    char *name;
    char *model;
    char *imageUrl;
    char *status;               // "ready", "busy", "calibrate_doing", "error", "heating", "printing", "pausing", "pause", "canceling", "cancel", "completed"
    char *location;
    char *serialNumber;
} fnet_wan_dev_info_t;

typedef struct fnet_dev_product {
    int nozzleTempCtrlState;    // 0 disabled, 1 enabled
    int chamberTempCtrlState;
    int platformTempCtrlState;
    int lightCtrlState;
    int internalFanCtrlState;
    int externalFanCtrlState;
} fnet_dev_product_t;

typedef struct fnet_dev_detail {
    int pid;
    int nozzleCnt;
    int nozzleStyle;            // 0 independent, 1 non-independent
    char *measure;
    char *nozzleModel;
    char *firmwareVersion;
    char *macAddr;
    char *ipAddr;
    char *name;
    char *location;
    char *status;               // "ready", "busy", "calibrate_doing", "error", "heating", "printing", "pausing", "pause", "canceling", "cancel", "completed"
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
    double chamberFanSpeed;     // percent
    char *internalFanStatus;    // "open", "close"
    char *externalFanStatus;    // "open", "close"
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

typedef struct fnet_clound_gcode_data {
    const char *bucketName;
    const char *endpoint;
    const char *gcodeStorageKey;
    const char *gcodeStorageUrl;
    const char *thumbStorageKey;
    const char *thumbStorageUrl;
} fnet_clound_gcode_data_t;

typedef struct fnet_clound_job_error {
    fnet_clound_job_error_type_t type;
    const char *devId;
    int code;
} fnet_clound_job_error_t;

typedef struct fnet_conn_read_data {
    fnet_conn_read_data_type_t type;
    void *data;                 // call fent_freeXXXX to release
    char *devId;                // call fnet_freeString to release
} fnet_conn_read_data_t;

#pragma pack(pop)


#define FNET_OK 0
#define FNET_ERROR -1
#define FNET_ABORTED_BY_CALLBACK 1
#define FNET_DIVICE_IS_BUSY 2
#define FNET_VERIFY_LAN_DEV_FAILED 1001 // invalid serialNumber/checkCode
#define FNET_UNAUTHORIZED 2001          // invalid accessToken/clientAccessToken
#define FNET_INVALID_VALIDATION 2002    // invalid userName/password/SMSCode
#define FNET_DEVICE_HAS_BEEN_BOUND 2003

#ifdef __cplusplus
extern "C" {
#endif

FNET_API int fnet_initlize(const char *serverSettingsPath, const fnet_log_settings_t *logSettings);

FNET_API void fnet_uninitlize();

FNET_API const char *fnet_getVersion(); // 1.0.2

FNET_API int fnet_getLanDevList(fnet_lan_dev_info_t **infos, int *devCnt, int msWaitTime);

FNET_API void fnet_freeLanDevInfos(fnet_lan_dev_info_t *infos);

FNET_API int fnet_getLanDevProduct(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, fnet_dev_product_t **product, int msTimeout);

FNET_API void fnet_freeDevProduct(fnet_dev_product_t *product);

FNET_API int fnet_getLanDevDetail(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, fnet_dev_detail_t **detail, int msTimeout);

FNET_API void fnet_freeDevDetail(fnet_dev_detail_t *detail);

FNET_API int fnet_ctrlLanDevTemp(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, const fnet_temp_ctrl_t *tempCtrl, int msTimeout);

FNET_API int fnet_ctrlLanDevLight(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, const fnet_light_ctrl_t *lightCtrl, int msTimeout);

FNET_API int fnet_ctrlLanDevAirFilter(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, const fnet_air_filter_ctrl_t *airFilterCtrl, int msTimeout);

FNET_API int fnet_ctrlLanDevPrint(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, const fnet_print_ctrl_t *printCtrl, int msTimeout);

FNET_API int fnet_ctrlLanDevJob(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, const fnet_job_ctrl_t *jobCtrl, int msTimeout);

FNET_API int fnet_lanDevSendGcode(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, const fnet_send_gcode_data_t *sendGcodeData, int msTimeout);

FNET_API int fnet_downloadFile(const char *url, fnet_file_data_t **fileData,
    fnet_progress_callback_t callback, void *callbackData, int msTimeout);

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

FNET_API void fent_freeBindData(fnet_wan_dev_bind_data_t *bindData);

FNET_API int fnet_unbindWanDev(const char *uid, const char *accessToken, const char *devId,
    int msTimeout);

FNET_API int fnet_getWanDevList(const char *uid, const char *accessToken, fnet_wan_dev_info_t **infos,
    int *devCnt, int msTimeout);

FNET_API void fnet_freeWanDevList(fnet_wan_dev_info_t *infos, int devCnt);

FNET_API int fnet_getWanDevProductDetail(const char *uid, const char *accessToken, const char *devId,
    fnet_dev_product_t **product, fnet_dev_detail_t **detail, int msTimeout);

FNET_API int fnet_wanDevSendGcode(const char *uid, const char *accessToken, const char *devId,
    const fnet_send_gcode_data_t *sendGcodeData, int msTimeout);

FNET_API int fnet_wanDevSendGcodeClound(const char *uid, const char *accessToken,
    const fnet_send_gcode_data_t *sendGcodeData, fnet_clound_gcode_data_t **cloundGcodeData, int msTimeout);

FNET_API void fnet_freeCloundGcodeData(fnet_clound_gcode_data_t *cloundGcodeData);

FNET_API int fnet_wanDevStartCloundJob(const char *uid, const char *accessToken,
    const fnet_clound_job_data_t *jobData, fnet_clound_job_error_t **errors, int *errorCnt, int msTimeout);

FNET_API void fent_freeCloudJobErrors(fnet_clound_job_error_t *errors, int errorCnt);

FNET_API int fnet_createConnection(void **conn, const char *uid, const char *accessToken,
    const fnet_conn_settings_t *settings);

FNET_API void fnet_freeConnection(void *conn);

FNET_API int fnet_connectionRun(void *conn); // run event processing loop

FNET_API void fnet_connectionPost(void *conn, const fnet_conn_write_data_t *writeData); // called in another thread

FNET_API void fnet_connectionStop(void *conn); // called in another thread

FNET_API void fnet_freeString(char *str);

#ifdef __cplusplus
}
#endif

#endif // !_FLASHNETWORK_H_
