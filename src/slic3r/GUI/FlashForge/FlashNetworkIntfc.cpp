#include "FlashNetworkIntfc.h"
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

namespace fnet {

FlashNetworkIntfc::FlashNetworkIntfc(const char *libraryPath, const char *serverSettingsPath,
    const fnet_log_settings_t &logSettings)
    : m_isOk(false)
{
    library_handle_t libraryHandle = loadLibrary(libraryPath);
    if (libraryHandle == INVALID_LIBRARY_HANDLE) {
        return;
    }
#define INIT_FUNC_PTR(ptr, func)\
        ptr = (decltype(&func))getFuncPtr(libraryHandle, #func);\
        if (ptr == nullptr) {\
            return;\
        }
    INIT_FUNC_PTR(initlize, fnet_initlize);
    INIT_FUNC_PTR(uninitlize, fnet_uninitlize);
    INIT_FUNC_PTR(getVersion, fnet_getVersion);
    INIT_FUNC_PTR(getLanDevList, fnet_getLanDevList);
    INIT_FUNC_PTR(freeLanDevInfos, fnet_freeLanDevInfos);
    INIT_FUNC_PTR(getLanDevProduct, fnet_getLanDevProduct);
    INIT_FUNC_PTR(freeDevProduct, fnet_freeDevProduct);
    INIT_FUNC_PTR(getLanDevDetail, fnet_getLanDevDetail);
    INIT_FUNC_PTR(freeDevDetail, fnet_freeDevDetail);
    INIT_FUNC_PTR(getLanDevGcodeList, fnet_getLanDevGcodeList);
    INIT_FUNC_PTR(freeGcodeList, fnet_freeGcodeList);
    INIT_FUNC_PTR(getLanDevGcodeThumb, fnet_getLanDevGcodeThumb);
    INIT_FUNC_PTR(lanDevStartJob, fnet_lanDevStartJob);
    INIT_FUNC_PTR(ctrlLanDevTemp, fnet_ctrlLanDevTemp);
    INIT_FUNC_PTR(ctrlLanDevLight, fnet_ctrlLanDevLight);
    INIT_FUNC_PTR(ctrlLanDevAirFilter, fnet_ctrlLanDevAirFilter);
    INIT_FUNC_PTR(ctrlLanDevClearFan, fnet_ctrlLanDevClearFan);
    INIT_FUNC_PTR(ctrlLanDevMove, fnet_ctrlLanDevMove);
    INIT_FUNC_PTR(ctrlLanDevExtrude, fnet_ctrlLanDevExtrude);
    INIT_FUNC_PTR(ctrlLanDevHoming, fnet_ctrlLanDevHoming);
    INIT_FUNC_PTR(ctrlLanDevMatlStation, fnet_ctrlLanDevMatlStation);
    INIT_FUNC_PTR(ctrlLanDevIndepMatl, fnet_ctrlLanDevIndepMatl);
    INIT_FUNC_PTR(ctrlLanDevPrint, fnet_ctrlLanDevPrint);
    INIT_FUNC_PTR(ctrlLanDevJob, fnet_ctrlLanDevJob);
    INIT_FUNC_PTR(ctrlLanDevState, fnet_ctrlLanDevState);
    INIT_FUNC_PTR(ctrlLanDevErrorCode, fnet_ctrlLanDevErrorCode);
    INIT_FUNC_PTR(ctrlLanDevPlateDetect, fnet_ctrlLanDevPlateDetect);
    INIT_FUNC_PTR(ctrlLanDevFirstLayerDetect, fnet_ctrlLanDevFirstLayerDetect);
    INIT_FUNC_PTR(configLanDevMatlStation, fnet_configLanDevMatlStation);
    INIT_FUNC_PTR(configLanDevIndepMatl, fnet_configLanDevIndepMatl);
    INIT_FUNC_PTR(lanDevSendGcode, fnet_lanDevSendGcode);
    INIT_FUNC_PTR(notifyLanDevWanBind, fnet_notifyLanDevWanBind);
    INIT_FUNC_PTR(downloadFileMem, fnet_downloadFileMem);
    INIT_FUNC_PTR(downloadFileDisk, fnet_downloadFileDisk);
    INIT_FUNC_PTR(freeFileData, fnet_freeFileData);
    INIT_FUNC_PTR(getTokenByPassword, fnet_getTokenByPassword);
    INIT_FUNC_PTR(refreshToken, fnet_refreshToken);
    INIT_FUNC_PTR(freeToken, fnet_freeToken);
    INIT_FUNC_PTR(getClientToken, fnet_getClientToken);
    INIT_FUNC_PTR(freeClientToken, fnet_freeClientToken);
    INIT_FUNC_PTR(sendSMSCode, fnet_sendSMSCode);
    INIT_FUNC_PTR(getTokenBySMSCode, fnet_getTokenBySMSCode);
    INIT_FUNC_PTR(checkToken, fnet_checkToken);
    INIT_FUNC_PTR(signOut, fnet_signOut);
    INIT_FUNC_PTR(getUserProfile, fnet_getUserProfile);
    INIT_FUNC_PTR(freeUserProfile, fnet_freeUserProfile);
    INIT_FUNC_PTR(bindWanDev, fnet_bindWanDev);
    INIT_FUNC_PTR(freeBindData, fnet_freeBindData);
    INIT_FUNC_PTR(unbindWanDev, fnet_unbindWanDev);
    INIT_FUNC_PTR(getWanDevList, fnet_getWanDevList);
    INIT_FUNC_PTR(freeWanDevList, fnet_freeWanDevList);
    INIT_FUNC_PTR(getWanDevProductDetail, fnet_getWanDevProductDetail);
    INIT_FUNC_PTR(getWanDevGcodeList, fnet_getWanDevGcodeList);
    INIT_FUNC_PTR(getWanDevTimeLapseVideoList, fnet_getWanDevTimeLapseVideoList);
    INIT_FUNC_PTR(freeTimeLapseVideoList, fnet_freeTimeLapseVideoList);
    INIT_FUNC_PTR(deleteTimeLapseVideo, fnet_deleteTimeLapseVideo);
    INIT_FUNC_PTR(wanDevAddJob, fnet_wanDevAddJob);
    INIT_FUNC_PTR(freeAddJobResult, fnet_freeAddJobResult);
    INIT_FUNC_PTR(wanDevSendGcodeClound, fnet_wanDevSendGcodeClound);
    INIT_FUNC_PTR(freeCloundGcodeData, fnet_freeCloundGcodeData);
    INIT_FUNC_PTR(wanDevAddCloundJob, fnet_wanDevAddCloundJob);
    INIT_FUNC_PTR(freeAddCloudJobResults, fnet_freeAddCloudJobResults);
    INIT_FUNC_PTR(bindAccountRelp, fnet_bindAccountRelp);
    INIT_FUNC_PTR(freeBindAccountRelpResult, fnet_freeBindAccountRelpResult);
    INIT_FUNC_PTR(uploadAiImageClound, fnet_uploadAiImageClound);
    INIT_FUNC_PTR(freeCloundFileData, fnet_freeCloundFileData);
    INIT_FUNC_PTR(getUserAiPointsInfo, fnet_getUserAiPointsInfo);
    INIT_FUNC_PTR(freeUserAiPointsInfo, fnet_freeUserAiPointsInfo);
    INIT_FUNC_PTR(createAiJobPipeline, fnet_createAiJobPipeline);
    INIT_FUNC_PTR(freeAiJobPipelineInfo, fnet_freeAiJobPipelineInfo);
    INIT_FUNC_PTR(startAiModelJob, fnet_startAiModelJob);
    INIT_FUNC_PTR(freeStartAiModelJobResult, fnet_freeStartAiModelJobResult);
    INIT_FUNC_PTR(getAiModelJobState, fnet_getAiModelJobState);
    INIT_FUNC_PTR(freeAiModelJobState, fnet_freeAiModelJobState);
    INIT_FUNC_PTR(abortAiModelJob, fnet_abortAiModelJob);
    INIT_FUNC_PTR(getExistingAiModelJob, fnet_getExistingAiModelJob);
    INIT_FUNC_PTR(startAiImg2imgJob, fnet_startAiImg2imgJob);
    INIT_FUNC_PTR(startAiTxt2txtJob, fnet_startAiTxt2txtJob);
    INIT_FUNC_PTR(startAiTxt2imgJob, fnet_startAiTxt2imgJob);
    INIT_FUNC_PTR(freeStartAiGeneralJobResult, fnet_freeStartAiGeneralJobResult);
    INIT_FUNC_PTR(getAiImg2imgJobState, fnet_getAiImg2imgJobState);
    INIT_FUNC_PTR(getAiTxt2txtJobState, fnet_getAiTxt2txtJobState);
    INIT_FUNC_PTR(getAiTxt2imgJobState, fnet_getAiTxt2imgJobState);
    INIT_FUNC_PTR(freeAiGeneralJobState, fnet_freeAiGeneralJobState);
    INIT_FUNC_PTR(abortAiImg2imgJob, fnet_abortAiImg2imgJob);
    INIT_FUNC_PTR(abortAiTxt2txtJob, fnet_abortAiTxt2txtJob);
    INIT_FUNC_PTR(abortAiTxt2imgJob, fnet_abortAiTxt2imgJob);
    INIT_FUNC_PTR(userClickCount, fnet_userClickCount);
    INIT_FUNC_PTR(doBusGetRequest, fnet_doBusGetRequest);
    INIT_FUNC_PTR(getNimData, fnet_getNimData);
    INIT_FUNC_PTR(freeNimData, fnet_freeNimData);
    INIT_FUNC_PTR(initlizeNim, fnet_initlizeNim);
    INIT_FUNC_PTR(uninitlizeNim, fnet_uninitlizeNim);
    INIT_FUNC_PTR(createConnection, fnet_createConnection);
    INIT_FUNC_PTR(freeConnection, fnet_freeConnection);
    INIT_FUNC_PTR(connectionSend, fnet_connectionSend);
    INIT_FUNC_PTR(connectionSubscribe, fnet_connectionSubscribe);
    INIT_FUNC_PTR(connectionUnsubscribe, fnet_connectionUnsubscribe);
    INIT_FUNC_PTR(freeString, fnet_freeString);
    if (initlize(serverSettingsPath, &logSettings) == FNET_OK && strcmp(getVersion(), "2.3.0") == 0) {
        m_isOk = true;
    }
}

FlashNetworkIntfc::~FlashNetworkIntfc()
{
    if (m_isOk) {
        uninitlize();
    }
}

library_handle_t FlashNetworkIntfc::loadLibrary(const char *libraryPath)
{
#ifdef _WIN32
    std::vector<wchar_t> wpath(256, 0);
    ::MultiByteToWideChar(CP_UTF8, NULL, libraryPath, (int)strlen(libraryPath), wpath.data(), (int)wpath.size());
    library_handle_t handle = LoadLibraryW(wpath.data());
    if (handle == INVALID_LIBRARY_HANDLE) {
        printf("load network module error: %x\n", GetLastError());
    }
    return handle;
#else
    library_handle_t handle = dlopen(libraryPath, RTLD_LAZY);
    if (handle == nullptr) {
        const char *dllError = dlerror();
        printf("load network module error: %s\n", dllError);
    }
    return handle;
#endif
}

void *FlashNetworkIntfc::getFuncPtr(library_handle_t libraryHandle, const char *funcName)
{
#ifdef _WIN32
    void *funcPtr = GetProcAddress(libraryHandle, funcName);
    if (funcPtr == nullptr) {
        printf("can't find function %s\n", funcName);
    }
    return funcPtr;
#else
    void *funcPtr = dlsym(libraryHandle, funcName);
    if (funcPtr == nullptr) {
        printf("can't find function %s\n", funcName);
    }
    return funcPtr;
#endif
}

} // namespace fnet
