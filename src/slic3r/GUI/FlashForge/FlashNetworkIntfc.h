#ifndef _FNET_FLASHNETWORKINTFC_H_
#define _FNET_FLASHNETWORKINTFC_H_

#include "FlashNetwork.h"

#ifdef _WIN32
#include <Windows.h>
typedef HMODULE library_handle_t;
#define INVALID_LIBRARY_HANDLE NULL
#else
typedef void *library_handle_t;
#define INVALID_LIBRARY_HANDLE nullptr
#endif // _WIN32

namespace fnet {

class FlashNetworkIntfc
{
public:
    decltype(&fnet_initlize) initlize;
    decltype(&fnet_uninitlize) uninitlize;
    decltype(&fnet_getVersion) getVersion;
    decltype(&fnet_getHomePageUrl) getHomePageUrl;
    decltype(&fnet_setUserAgent) setUserAgent;
    decltype(&fnet_getLanDevList) getLanDevList;
    decltype(&fnet_freeLanDevInfos) freeLanDevInfos;
    decltype(&fnet_getLanDevProduct) getLanDevProduct;
    decltype(&fnet_freeDevProduct) freeDevProduct;
    decltype(&fnet_getLanDevDetail) getLanDevDetail;
    decltype(&fnet_freeDevDetail) freeDevDetail;
    decltype(&fnet_getLanDevGcodeList) getLanDevGcodeList;
    decltype(&fnet_freeGcodeList) freeGcodeList;
    decltype(&fnet_getLanDevGcodeThumb) getLanDevGcodeThumb;
    decltype(&fnet_lanDevStartJob) lanDevStartJob;
    decltype(&fnet_ctrlLanDevTemp) ctrlLanDevTemp;
    decltype(&fnet_ctrlLanDevLight) ctrlLanDevLight;
    decltype(&fnet_ctrlLanDevAirFilter) ctrlLanDevAirFilter;
    decltype(&fnet_ctrlLanDevClearFan) ctrlLanDevClearFan;
    decltype(&fnet_ctrlLanDevMove) ctrlLanDevMove;
    decltype(&fnet_ctrlLanDevExtrude) ctrlLanDevExtrude;
    decltype(&fnet_ctrlLanDevHoming) ctrlLanDevHoming;
    decltype(&fnet_ctrlLanDevMatlStation) ctrlLanDevMatlStation;
    decltype(&fnet_ctrlLanDevIndepMatl) ctrlLanDevIndepMatl;
    decltype(&fnet_ctrlLanDevPrint) ctrlLanDevPrint;
    decltype(&fnet_ctrlLanDevJob) ctrlLanDevJob;
    decltype(&fnet_ctrlLanDevState) ctrlLanDevState;
    decltype(&fnet_ctrlLanDevErrorCode) ctrlLanDevErrorCode;
    decltype(&fnet_ctrlLanDevPlateDetect) ctrlLanDevPlateDetect;
    decltype(&fnet_ctrlLanDevFirstLayerDetect) ctrlLanDevFirstLayerDetect;
    decltype(&fnet_configLanDevMatlStation) configLanDevMatlStation;
    decltype(&fnet_configLanDevIndepMatl) configLanDevIndepMatl;
    decltype(&fnet_lanDevSendGcode) lanDevSendGcode;
    decltype(&fnet_notifyLanDevWanBind) notifyLanDevWanBind;
    decltype(&fnet_downloadFileMem) downloadFileMem;
    decltype(&fnet_downloadFileDisk) downloadFileDisk;
    decltype(&fnet_freeFileData) freeFileData;
    decltype(&fnet_getTokenByPassword) getTokenByPassword;
    decltype(&fnet_refreshToken) refreshToken;
    decltype(&fnet_freeToken) freeToken;
    decltype(&fnet_sendSMSCode) sendSMSCode;
    decltype(&fnet_getTokenBySMSCode) getTokenBySMSCode;
    decltype(&fnet_signOut) signOut;
    decltype(&fnet_getUserProfile) getUserProfile;
    decltype(&fnet_freeUserProfile) freeUserProfile;
    decltype(&fnet_bindWanDev) bindWanDev;
    decltype(&fnet_freeBindData) freeBindData;
    decltype(&fnet_unbindWanDev) unbindWanDev;
    decltype(&fnet_getWanDevList) getWanDevList;
    decltype(&fnet_freeWanDevList) freeWanDevList;
    decltype(&fnet_getWanDevProductDetail) getWanDevProductDetail;
    decltype(&fnet_getWanDevGcodeList) getWanDevGcodeList;
    decltype(&fnet_getWanDevTimeLapseVideoList) getWanDevTimeLapseVideoList;
    decltype(&fnet_freeTimeLapseVideoList) freeTimeLapseVideoList;
    decltype(&fnet_deleteTimeLapseVideo) deleteTimeLapseVideo;
    decltype(&fnet_wanDevAddJob) wanDevAddJob;
    decltype(&fnet_freeAddJobResult) freeAddJobResult;
    decltype(&fnet_wanDevSendGcodeClound) wanDevSendGcodeClound;
    decltype(&fnet_freeCloundGcodeData) freeCloundGcodeData;
    decltype(&fnet_wanDevAddCloundJob) wanDevAddCloundJob;
    decltype(&fnet_freeAddCloudJobResults) freeAddCloudJobResults;
    decltype(&fnet_bindAccountRelp) bindAccountRelp;
    decltype(&fnet_freeBindAccountRelpResult) freeBindAccountRelpResult;
    decltype(&fnet_uploadAiImageClound) uploadAiImageClound;
    decltype(&fnet_uploadLogFileCloud) uploadLogFileCloud;
    decltype(&fnet_uploadLogFileCloudEx) uploadLogFileCloudEx;
    decltype(&fnet_freeCloundFileData) freeCloundFileData;
    decltype(&fnet_getUserAiPointsInfo) getUserAiPointsInfo;
    decltype(&fnet_freeUserAiPointsInfo) freeUserAiPointsInfo;
    decltype(&fnet_createAiJobPipeline) createAiJobPipeline;
    decltype(&fnet_freeAiJobPipelineInfo) freeAiJobPipelineInfo;
    decltype(&fnet_startAiModelJob) startAiModelJob;
    decltype(&fnet_freeStartAiModelJobResult) freeStartAiModelJobResult;
    decltype(&fnet_getAiModelJobState) getAiModelJobState;
    decltype(&fnet_freeAiModelJobState) freeAiModelJobState;
    decltype(&fnet_abortAiModelJob) abortAiModelJob;
    decltype(&fnet_getExistingAiModelJob) getExistingAiModelJob;
    decltype(&fnet_startAiImg2imgJob) startAiImg2imgJob;
    decltype(&fnet_startAiTxt2txtJob) startAiTxt2txtJob;
    decltype(&fnet_startAiTxt2imgJob) startAiTxt2imgJob;
    decltype(&fnet_freeStartAiGeneralJobResult) freeStartAiGeneralJobResult;
    decltype(&fnet_getAiImg2imgJobState) getAiImg2imgJobState;
    decltype(&fnet_getAiTxt2txtJobState) getAiTxt2txtJobState;
    decltype(&fnet_getAiTxt2imgJobState) getAiTxt2imgJobState;
    decltype(&fnet_freeAiGeneralJobState) freeAiGeneralJobState;
    decltype(&fnet_abortAiImg2imgJob) abortAiImg2imgJob;
    decltype(&fnet_abortAiTxt2txtJob) abortAiTxt2txtJob;
    decltype(&fnet_abortAiTxt2imgJob) abortAiTxt2imgJob;
    decltype(&fnet_userClickCount) userClickCount;
    decltype(&fnet_addPrintListModel) addPrintListModel;
    decltype(&fnet_removePrintListModel) removePrintListModel;
    decltype(&fnet_navLikeModel) navLikeModel;
    decltype(&fnet_reportModel) reportModel;
    decltype(&fnet_reportTrackingData) reportTrackingData;
    decltype(&fnet_reportTrackingDataBatch) reportTrackingDataBatch;
    decltype(&fnet_getSystemMessage)getSystemMessage;
    decltype(&fnet_postReadSystemMessage)postReadSystemMessage;
    decltype(&fnet_freeSystemMessage)freeSystemMessage;
    decltype(&fnet_getMonitorMessage)getMonitorMessage;
    decltype(&fnet_retrySliceTask)retrySliceTask;
    decltype(&fnet_cancelSliceTask)cancelSliceTask;
    decltype(&fnet_freeSliceState)freeSliceState;
    decltype(&fnet_freeJobInfo)freeJobInfo;
    decltype(&fnet_doBusGetRequest) doBusGetRequest;
    decltype(&fnet_doBusPostRequest) doBusPostRequest;
    decltype(&fnet_getSettingList2) getSettingList2;
    decltype(&fnet_getSettingList) getSettingList;
    decltype(&fnet_getUserPresets) getUserPresets;
    decltype(&fnet_requestSettingId) requestSettingId;
    decltype(&fnet_putSetting) putSetting;
    decltype(&fnet_deleteSetting) deleteSetting;
    decltype(&fnet_getMqttConfig) getMqttConfig;
    decltype(&fnet_freeMqttConfig) freeMqttConfig;
    decltype(&fnet_createConnection) createConnection;
    decltype(&fnet_freeConnection) freeConnection;
    decltype(&fnet_connectionStop) connectionStop;
    decltype(&fnet_connectionSend) connectionSend;
    decltype(&fnet_connectionSendMulti) connectionSendMulti;
    decltype(&fnet_connectionSubscribe) connectionSubscribe;
    decltype(&fnet_connectionUnsubscribe) connectionUnsubscribe;
    decltype(&fnet_freeWriteMultiResult) freeWriteMultiResult;
    decltype(&fnet_freeSyncLoginInfo) freeSyncLoginInfo;
    decltype(&fnet_freeSyncBindInfo) freeSyncBindInfo;
    decltype(&fnet_freeSyncOnlineInfo) freeSyncOnlineInfo;
    decltype(&fnet_freeUploadLogInfo) freeUploadLogInfo;
    decltype(&fnet_allocString) allocString;
    decltype(&fnet_freeString) freeString;

public:
    FlashNetworkIntfc(const char *libraryPath, const char *serverSettingsPath,
        const fnet_log_settings_t &logSettings);

    ~FlashNetworkIntfc();

    bool isOk() const { return m_isOk; }

    // True when the loaded library is 3.4.0, the last one shipped for Linux (Flash Studio 1.7.9).
    // It predates the cloud preset and feedback-log functions, so those pointers are null, and it
    // allocates fnet_dev_detail_t without the trailing cumulativePrintTimeSecond fields.
    bool isLegacyLibrary() const { return m_isLegacyLibrary; }

private:
    library_handle_t loadLibrary(const char *libraryPath);

    void *getFuncPtr(library_handle_t libraryHandle, const char *funcName);

private:
    bool m_isOk;
    bool m_isLegacyLibrary;
};

} // namespace fnet

#endif
