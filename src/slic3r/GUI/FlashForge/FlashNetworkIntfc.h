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
    decltype(&fnet_getLanDevList) getLanDevList;
    decltype(&fnet_freeLanDevInfos) freeLanDevInfos;
    decltype(&fnet_getLanDevProduct) getLanDevProduct;
    decltype(&fnet_freeDevProduct) freeDevProduct;
    decltype(&fnet_getLanDevDetail) getLanDevDetail;
    decltype(&fnet_freeDevDetail) freeDevDetail;
    decltype(&fnet_ctrlLanDevTemp) ctrlLanDevTemp;
    decltype(&fnet_ctrlLanDevLight) ctrlLanDevLight;
    decltype(&fnet_ctrlLanDevAirFilter) ctrlLanDevAirFilter;
    decltype(&fnet_ctrlLanDevPrint) ctrlLanDevPrint;
    decltype(&fnet_ctrlLanDevJob) ctrlLanDevJob;
    decltype(&fnet_lanDevSendGcode) lanDevSendGcode;
    decltype(&fnet_downloadFile) downloadFile;
    decltype(&fnet_freeFileData) freeFileData;
    decltype(&fnet_getTokenByPassword) getTokenByPassword;
    decltype(&fnet_refreshToken) refreshToken;
    decltype(&fnet_freeToken) freeToken;
    decltype(&fnet_getClientToken) getClientToken;
    decltype(&fnet_freeClientToken) freeClientToken;
    decltype(&fnet_sendSMSCode) sendSMSCode;
    decltype(&fnet_getTokenBySMSCode) getTokenBySMSCode;
    decltype(&fnet_checkToken) checkToken;
    decltype(&fnet_signOut) signOut;
    decltype(&fnet_getUserProfile) getUserProfile;
    decltype(&fnet_freeUserProfile) freeUserProfile;
    decltype(&fnet_bindWanDev) bindWanDev;
    decltype(&fent_freeBindData) freeBindData;
    decltype(&fnet_unbindWanDev) unbindWanDev;
    decltype(&fnet_getWanDevList) getWanDevList;
    decltype(&fnet_freeWanDevList) freeWanDevList;
    decltype(&fnet_getWanDevProductDetail) getWanDevProductDetail;
    decltype(&fnet_wanDevSendGcode) wanDevSendGcode;
    decltype(&fnet_wanDevSendGcodeClound) wanDevSendGcodeClound;
    decltype(&fnet_wanDevStartCloundJob) wanDevStartCloundJob;
    decltype(&fent_freeCloudJobErrors) freeCloudJobErrors;
    decltype(&fnet_freeCloundGcodeData) freeCloundGcodeData;
    decltype(&fnet_createConnection) createConnection;
    decltype(&fnet_freeConnection) freeConnection;
    decltype(&fnet_connectionRun) connectionRun;
    decltype(&fnet_connectionPost) connectionPost;
    decltype(&fnet_connectionStop) connectionStop;
    decltype(&fnet_freeString) freeString;

public:
    FlashNetworkIntfc(const char *libraryPath, const char *serverSettingsPath,
        const fnet_log_settings_t &logSettings);

    ~FlashNetworkIntfc();

    bool isOk() const { return m_isOk; }

private:
    library_handle_t loadLibrary(const char *libraryPath);

    void *getFuncPtr(library_handle_t libraryHandle, const char *funcName);

private:
    bool m_isOk;
};

} // namespace fnet

#endif
