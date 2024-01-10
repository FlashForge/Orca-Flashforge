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

typedef void (*fnet_progress_call_back_t)(long long now, long long total, void *data);

#pragma pack(push, 4)

typedef struct fnet_send_gcode_data {
    const char *gcodeFileName;  // utf-8
    const char *thumbFileName;  // utf-8, wan only
    int printNow;               // 1 true, 0 false
    int levelingBeforePrint;    // 1 true, 0 false
    fnet_progress_call_back_t callback;
    void *callbackData;
} fnet_send_gcode_data_t;

typedef struct fnet_lan_dev_info {
    char serialNumber[MAX_DEVICE_SN_LEN];
    char name[MAX_DEVICE_NAME_LEN];
    char ip[16];
    unsigned short port;
    unsigned short vid;
    unsigned short pid;
    unsigned short connectMode; // 0 lan mode, 1 wan mode
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
    char *status;
    char *location;
    char *serialNumber;
} fnet_wan_dev_info_t;

typedef struct fnet_dev_detail {
    char *model;
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
    char *printStatus;
    char *printFileName;
    char *printFileThumbUrl;
    int printLayer;
    int targetPrintLayer;
    double printProgress;       // percent
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
    double coolingFanSpeed;     // percent
    double chamberFanSpeed;     // percent
    char *internalFanStatus;
    char *externalFanStatus;
    char *doorStatus;
    char *lightStatus;
    char *autoShutdown;
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

#pragma pack(pop)


#define FNET_OK 0
#define FNET_ERROR -1
#define FNET_DIVICE_IS_BUSY 2
#define FNET_VERIFY_LAN_DEV_FAILED 1001 // invalid serialNumber/checkCode
#define FNET_UNAUTHORIZED 2001          // invalid accessToken/clientAccessToken
#define FNET_INVALID_VALIDATION 2002    // invalid userName/password/SMSCode

#ifdef __cplusplus
extern "C" {
#endif

FNET_API int fnet_initlize();

FNET_API void fnet_uninitlize();

FNET_API const char *fnet_getVersion();

FNET_API int fnet_getLanDevList(fnet_lan_dev_info_t **infos, int *devCnt);

FNET_API void fnet_freeLanDevInfos(fnet_lan_dev_info_t *infos);

FNET_API int fnet_getLanDevDetail(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, fnet_dev_detail_t **detail);

FNET_API void fnet_freeDevDetail(fnet_dev_detail_t *detail);

FNET_API int fnet_lanDevSendGcode(const char *ip, unsigned short port, const char *serialNumber,
    const char *checkCode, const fnet_send_gcode_data_t *sendGcodeData);

FNET_API int fnet_downloadFile(const char *url, fnet_file_data_t **fileData,
    fnet_progress_call_back_t callback, void *callbackData);

FNET_API void fnet_freeFileData(fnet_file_data_t *fileData);

FNET_API int fnet_getTokenByPassword(const char *userName, const char *password,
    fnet_token_data_t **tokenData);

FNET_API int fnet_refreshToken(const char *refreshToken, fnet_token_data_t **tokenData);

FNET_API void fnet_freeToken(fnet_token_data_t *tokenData);

FNET_API int fnet_getClientToken(fnet_client_token_data_t **clientTokenData);

FNET_API void fnet_freeClientToken(fnet_client_token_data_t *clientTokenData);

FNET_API int fnet_sendSMSCode(const char *clientAccessToken, const char *phoneNumber,
    const char *language); // en/zh

FNET_API int fnet_getTokenBySMSCode(const char *userName, const char *SMSCode,
    fnet_token_data_t **tokenData);

FNET_API int fnet_checkToken(const char *accessToken);

FNET_API int fnet_signOut(const char *accessToken);

FNET_API int fnet_getUserProfile(const char *accessToken, fnet_user_profile_t **profile);

FNET_API void fnet_freeUserProfile(fnet_user_profile_t *profile);

FNET_API int fnet_bindWanDev(const char *accessToken, const char *serialNumber,
    unsigned short pid, const char *name, fnet_wan_dev_bind_data_t **bindData);

FNET_API void fent_freeBindData(fnet_wan_dev_bind_data_t *bindData);

FNET_API int fnet_unbindWanDev(const char *accessToken, const char *devId);

FNET_API int fnet_getWanDevList(const char *accessToken, fnet_wan_dev_info_t **infos,
    int *devCnt);

FNET_API void fnet_freeWanDevList(fnet_wan_dev_info_t *infos, int devCnt);

FNET_API int fnet_getWanDevDetail(const char *accessToken, const char *devId,
    fnet_dev_detail_t **detail);

FNET_API int fnet_wanDevSendGcode(const char *accessToken, const char *devId,
    const fnet_send_gcode_data_t *sendGcodeData);

#ifdef __cplusplus
}
#endif

#endif // !_FLASHNETWORK_H_
