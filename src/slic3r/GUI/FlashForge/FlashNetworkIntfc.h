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
    decltype(&fnet_lanDevSendGcode) lanDevSendGcode;
    decltype(&fnet_getLanDevDetail) getLanDevDetail;
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
    decltype(&fnet_downloadFile) downloadFile;
    decltype(&fnet_freeFileData) freeFileData;
    decltype(&fnet_bindWanDev) bindWanDev;
    decltype(&fent_freeBindData) freeBindData;
    decltype(&fnet_unbindWanDev) unbindWanDev;
    decltype(&fnet_getWanDevList) getWanDevList;
    decltype(&fnet_freeWanDevList) freeWanDevList;
    decltype(&fnet_getWanDevDetail) getWanDevDetail;
    decltype(&fnet_freeDevDetail) freeDevDetail;
    decltype(&fnet_wanDevSendGcode) wanDevSendGcode;

public:
    FlashNetworkIntfc(const char *libraryPath);

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
