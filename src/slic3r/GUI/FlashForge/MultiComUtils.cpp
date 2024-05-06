#include "MultiComUtils.hpp"
#include <wx/thread.h>
#include "FreeInDestructor.h"
#include "MultiComMgr.hpp"
#include "WaitEvent.hpp"

namespace Slic3r { namespace GUI {

wxDEFINE_EVENT(COM_ASYNC_CALL_FINISH_EVENT, ComAsyncCallFinishEvent);

class ComAsyncThread : public wxThread
{
public:
    ComAsyncThread()
        : wxThread(wxTHREAD_JOINABLE)
    {
    }
    ExitCode Entry()
    {
        ComAsyncCallFinishEvent *event = new ComAsyncCallFinishEvent;
        event->SetEventType(COM_ASYNC_CALL_FINISH_EVENT);
        event->ret = func();
        evtHandler->QueueEvent(event);
        evtHandler->CallAfter([this]() {
            com_thread_ptr_t scopedThreadPtr = std::move(threadPtr);
            Wait();
        });
        return 0;
    }
    wxEvtHandler *evtHandler;
    com_async_call_func_t func;
    com_thread_ptr_t threadPtr;
};

ComErrno MultiComUtils::getLanDevList(std::vector<fnet_lan_dev_info> &devInfos)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    int devCnt;
    fnet_lan_dev_info *fnetDevInfos;
    int ret = intfc->getLanDevList(&fnetDevInfos, &devCnt, 500);
    if (ret != COM_OK) {
        return fnetRet2ComErrno(ret);
    }
    fnet::FreeInDestructor freeDevInfos(fnetDevInfos, intfc->freeLanDevInfos);
    devInfos.clear();
    for (int i = 0; i < devCnt; ++i) {
        devInfos.push_back(fnetDevInfos[i]);
    }
    return COM_OK;
}

ComErrno MultiComUtils::getTokenByPassword(const std::string &userName, const std::string &password,
    const std::string &language, com_token_data_t &tokenData, std::string &message)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    time_t startTime = time(nullptr);
    fnet_token_data_t *fnetTokenData;
    char *fnetMessage = nullptr;
    fnet::FreeInDestructor freeFnetMessage(fnetMessage, intfc->freeString);
    int ret = intfc->getTokenByPassword(userName.c_str(), password.c_str(), language.c_str(),
        &fnetTokenData, &fnetMessage, ComTimeoutWan);
    if (fnetMessage != nullptr) {
        message = fnetMessage;
    }
    if (ret != FNET_OK) {
        return fnetRet2ComErrno(ret);
    }
    fnet::FreeInDestructor freeTokenInfo(fnetTokenData, intfc->freeToken);
    tokenData.expiresIn = fnetTokenData->expiresIn;
    tokenData.accessToken = fnetTokenData->accessToken;
    tokenData.refreshToken = fnetTokenData->refreshToken;
    tokenData.startTime = startTime;
    return COM_OK;
}

ComErrno MultiComUtils::refreshToken(const std::string &refreshToken, com_token_data_t &tokenData)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    time_t startTime = time(nullptr);
    fnet_token_data_t *fnetTokenData;
    int ret = intfc->refreshToken(refreshToken.c_str(), "en", &fnetTokenData, nullptr, ComTimeoutWan);
    if (ret != COM_OK) {
        return fnetRet2ComErrno(ret);
    }
    fnet::FreeInDestructor freeTokenData(fnetTokenData, intfc->freeToken);
    tokenData.expiresIn = fnetTokenData->expiresIn;
    tokenData.accessToken = fnetTokenData->accessToken;
    tokenData.refreshToken = fnetTokenData->refreshToken;
    tokenData.startTime = startTime;
    return COM_OK;
}

ComErrno MultiComUtils::getClientToken(com_clinet_token_data_t &clinetTokenData)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    time_t startTime = time(nullptr);
    fnet_client_token_data *fnetClientTokenData;
    int ret = intfc->getClientToken("en", &fnetClientTokenData, nullptr, ComTimeoutWan);
    if (ret != COM_OK) {
        return fnetRet2ComErrno(ret);
    }
    fnet::FreeInDestructor freeClientTokenData(fnetClientTokenData, intfc->freeClientToken);
    clinetTokenData.accessToken = fnetClientTokenData->accessToken;
    clinetTokenData.expiresIn = fnetClientTokenData->expiresIn;
    clinetTokenData.startTime = startTime;
    return COM_OK;
}

ComErrno MultiComUtils::sendSMSCode(const std::string &clinetAccessToken, const std::string &phoneNumber,
    const std::string &language, std::string &message)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    char *fnetMessage = nullptr;
    fnet::FreeInDestructor freeFnetMessage(fnetMessage, intfc->freeString);
    int ret = intfc->sendSMSCode(clinetAccessToken.c_str(), phoneNumber.c_str(), language.c_str(),
        &fnetMessage, ComTimeoutWan);
    if (fnetMessage != nullptr) {
        message = fnetMessage;
    }
    if (ret != COM_OK) {
        return fnetRet2ComErrno(ret);
    }
    return COM_OK;
}

ComErrno MultiComUtils::getTokenBySMSCode(const std::string &userName, const std::string &SMSCode,
    const std::string &language, com_token_data_t &tokenData, std::string &message)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    time_t startTime = time(nullptr);
    fnet_token_data_t *fnetTokenData;
    char *fnetMessage = nullptr;
    fnet::FreeInDestructor freeFnetMessage(fnetMessage, intfc->freeString);
    int ret = intfc->getTokenBySMSCode(userName.c_str(), SMSCode.c_str(), language.c_str(),
        &fnetTokenData, &fnetMessage, ComTimeoutWan);
    if (fnetMessage != nullptr) {
        message = fnetMessage;
    }
    if (ret != COM_OK) {
        return fnetRet2ComErrno(ret);
    }
    fnet::FreeInDestructor freeTokenInfo(fnetTokenData, intfc->freeToken);
    tokenData.expiresIn = fnetTokenData->expiresIn;
    tokenData.accessToken = fnetTokenData->accessToken;
    tokenData.refreshToken = fnetTokenData->refreshToken;
    tokenData.startTime = startTime;
    return COM_OK;
}

ComErrno MultiComUtils::checkToken(const std::string &accessToken)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    return fnetRet2ComErrno(intfc->checkToken(accessToken.c_str(), ComTimeoutWan));
}

ComErrno MultiComUtils::signOut(const std::string &accessToken)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    return fnetRet2ComErrno(intfc->signOut(accessToken.c_str(), ComTimeoutWan));
}

ComErrno MultiComUtils::getUserProfile(const std::string &accessToken, com_user_profile_t &userProfile)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    fnet_user_profile_t *fnetProfile;
    int fnetRet = intfc->getUserProfile(accessToken.c_str(), &fnetProfile, ComTimeoutWan);
    if (fnetRet != FNET_OK) {
        return fnetRet2ComErrno(fnetRet);
    }
    fnet::FreeInDestructor freeProfile(fnetProfile, intfc->freeUserProfile);
    userProfile.uid = fnetProfile->uid;
    userProfile.nickname = fnetProfile->nickname;
    userProfile.headImgUrl = fnetProfile->headImgUrl;
    return COM_OK;
}

ComErrno MultiComUtils::downloadFile(const std::string &url, std::vector<char> &bytes, int msTimeout)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    fnet_file_data_t *fileData;
    int fnetRet = intfc->downloadFile(url.c_str(), &fileData, nullptr, nullptr, msTimeout);
    if (fnetRet != FNET_OK) {
        return fnetRet2ComErrno(fnetRet);
    }
    fnet::FreeInDestructor freeFileData(fileData, intfc->freeFileData);
    bytes.resize(fileData->size);
    memcpy(bytes.data(), fileData->data, fileData->size);
    return COM_OK;
}

ComErrno MultiComUtils::fnetRet2ComErrno(int networkRet)
{
    switch (networkRet) {
    case FNET_OK:
        return COM_OK;
    case FNET_ABORTED_BY_CALLBACK:
        return COM_ABORTED_BY_USER;
    case FNET_DIVICE_IS_BUSY:
        return COM_DEVICE_IS_BUSY;
    case FNET_VERIFY_LAN_DEV_FAILED:
        return COM_VERIFY_LAN_DEV_FAILED;
    case FNET_UNAUTHORIZED:
        return COM_UNAUTHORIZED;
    case FNET_INVALID_VALIDATION:
        return COM_INVALID_VALIDATION;
    case FNET_DEVICE_HAS_BEEN_BOUND:
        return COM_DEVICE_HAS_BEEN_BOUND;
    default:
        return COM_ERROR;
    }
}

com_thread_ptr_t MultiComUtils::asyncCall(wxEvtHandler *evtHandler, const com_async_call_func_t &func)
{
    ComAsyncThread *thread = new ComAsyncThread;
    thread->evtHandler = evtHandler;
    thread->func = func;
    thread->threadPtr.reset(thread);
    thread->Run();
    return thread->threadPtr;
}

void MultiComUtils::killAsyncCall(const com_thread_ptr_t &thread)
{
    thread->Kill();
    thread->threadPtr.reset();
}

}} // namespace Slic3r::GUI
