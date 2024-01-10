#include "FlashNetworkIntfc.h"
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

namespace fnet {

FlashNetworkIntfc::FlashNetworkIntfc(const char *libraryPath)
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
    INIT_FUNC_PTR(lanDevSendGcode, fnet_lanDevSendGcode);
    INIT_FUNC_PTR(getLanDevDetail, fnet_getLanDevDetail);
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
    INIT_FUNC_PTR(downloadFile, fnet_downloadFile);
    INIT_FUNC_PTR(freeFileData, fnet_freeFileData);
    INIT_FUNC_PTR(bindWanDev, fnet_bindWanDev);
    INIT_FUNC_PTR(freeBindData, fent_freeBindData);
    INIT_FUNC_PTR(unbindWanDev, fnet_unbindWanDev);
    INIT_FUNC_PTR(getWanDevList, fnet_getWanDevList);
    INIT_FUNC_PTR(freeWanDevList, fnet_freeWanDevList);
    INIT_FUNC_PTR(getWanDevDetail, fnet_getWanDevDetail);
    INIT_FUNC_PTR(freeDevDetail, fnet_freeDevDetail);
    INIT_FUNC_PTR(wanDevSendGcode, fnet_wanDevSendGcode);
    if (initlize() == FNET_OK) {
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
