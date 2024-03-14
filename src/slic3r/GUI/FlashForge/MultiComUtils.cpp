#include "MultiComUtils.hpp"
#include <boost/thread/thread.hpp>
#include "FreeInDestructor.h"
#include "MultiComMgr.hpp"
#include "WaitEvent.hpp"

namespace Slic3r { namespace GUI {

wxDEFINE_EVENT(COM_ASYNC_CALL_FINISH_EVENT, ComAsyncCallFinishEvent);

ComErrno MultiComUtils::getLanDevList(std::vector<fnet_lan_dev_info> &devInfos)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_UNINITIALIZED;
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
    com_token_data_t &tokenData)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_UNINITIALIZED;
    }
    fnet_token_data_t *fnetTokenData;
    int ret = intfc->getTokenByPassword(userName.c_str(), password.c_str(), &fnetTokenData, ComTimeoutWan);
    if (ret != FNET_OK) {
        return fnetRet2ComErrno(ret);
    }
    fnet::FreeInDestructor freeTokenInfo(fnetTokenData, intfc->freeToken);
    tokenData.expiresIn = fnetTokenData->expiresIn;
    tokenData.accessToken = fnetTokenData->accessToken;
    tokenData.refreshToken = fnetTokenData->refreshToken;
    return COM_OK;
}

ComErrno MultiComUtils::refreshToken(const std::string &refreshToken, com_token_data_t &tokenData)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_UNINITIALIZED;
    }
    fnet_token_data_t *fnetTokenData;
    int ret = intfc->refreshToken(refreshToken.c_str(), &fnetTokenData, ComTimeoutWan);
    if (ret != COM_OK) {
        return fnetRet2ComErrno(ret);
    }
    fnet::FreeInDestructor freeTokenData(fnetTokenData, intfc->freeToken);
    tokenData.expiresIn = fnetTokenData->expiresIn;
    tokenData.accessToken = fnetTokenData->accessToken;
    tokenData.refreshToken = fnetTokenData->refreshToken;
    return COM_OK;
}

ComErrno MultiComUtils::getClientToken(com_clinet_token_data_t &clinetTokenData)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_UNINITIALIZED;
    }
    fnet_client_token_data *fnetClientTokenData;
    int ret = intfc->getClientToken(&fnetClientTokenData, ComTimeoutWan);
    if (ret != COM_OK) {
        return fnetRet2ComErrno(ret);
    }
    fnet::FreeInDestructor freeClientTokenData(fnetClientTokenData, intfc->freeClientToken);
    clinetTokenData.accessToken = fnetClientTokenData->accessToken;
    clinetTokenData.expiresIn = fnetClientTokenData->expiresIn;
    return COM_OK;
}

ComErrno MultiComUtils::sendSMSCode(const std::string &clinetAccessToken, const std::string &phoneNumber)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_UNINITIALIZED;
    }
    int ret = intfc->sendSMSCode(clinetAccessToken.c_str(), phoneNumber.c_str(), "zh", ComTimeoutWan);
    if (ret != COM_OK) {
        return fnetRet2ComErrno(ret);
    }
    return COM_OK;
}

ComErrno MultiComUtils::getTokenBySMSCode(const std::string &userName, const std::string &SMSCode,
    com_token_data_t &tokenData)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_UNINITIALIZED;
    }
    fnet_token_data_t *fnetTokenData;
    int ret = intfc->getTokenBySMSCode(userName.c_str(), SMSCode.c_str(), &fnetTokenData, ComTimeoutWan);
    if (ret != COM_OK) {
        return fnetRet2ComErrno(ret);
    }
    fnet::FreeInDestructor freeTokenInfo(fnetTokenData, intfc->freeToken);
    tokenData.expiresIn = fnetTokenData->expiresIn;
    tokenData.accessToken = fnetTokenData->accessToken;
    tokenData.refreshToken = fnetTokenData->refreshToken;
    return COM_OK;
}

ComErrno MultiComUtils::checkToken(const std::string &accessToken)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_UNINITIALIZED;
    }
    return fnetRet2ComErrno(intfc->checkToken(accessToken.c_str(), ComTimeoutWan));
}

ComErrno MultiComUtils::signOut(const std::string &accessToken)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_UNINITIALIZED;
    }
    return fnetRet2ComErrno(intfc->signOut(accessToken.c_str(), ComTimeoutWan));
}

ComErrno MultiComUtils::getUserProfile(const std::string &accessToken, com_user_profile_t &userProfile)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_UNINITIALIZED;
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
        return COM_UNINITIALIZED;
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

void MultiComUtils::asyncCall(wxEvtHandler *evtHandler, const std::function<ComErrno()> &func)
{
    WaitEvent waitEvent;
    boost::thread *thread = nullptr;
    thread = new boost::thread([evtHandler, func, &waitEvent, &thread]() {
        boost::thread *myThread = thread;
        waitEvent.set(true);
        ComAsyncCallFinishEvent *event = new ComAsyncCallFinishEvent;
        event->SetEventType(COM_ASYNC_CALL_FINISH_EVENT);
        event->ret = func();
        evtHandler->QueueEvent(event);
        evtHandler->CallAfter([myThread]() {
            myThread->join();
            delete myThread;
        });
    });
    waitEvent.waitTrue();
}

}} // namespace Slic3r::GUI
