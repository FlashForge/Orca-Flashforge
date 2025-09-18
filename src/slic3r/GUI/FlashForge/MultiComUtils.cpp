#include "MultiComUtils.hpp"
#include "FreeInDestructor.h"
#include "MultiComMgr.hpp"

namespace Slic3r { namespace GUI {

ComErrno MultiComUtils::getLanDevList(std::vector<fnet_lan_dev_info> &devInfos)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    int devCnt;
    fnet_lan_dev_info *fnetDevInfos;
    int fnetRet = intfc->getLanDevList(&fnetDevInfos, &devCnt, 500);
    if (fnetRet != FNET_OK) {
        return fnetRet2ComErrno(fnetRet);
    }
    fnet::FreeInDestructor freeDevInfos(fnetDevInfos, intfc->freeLanDevInfos);
    devInfos.clear();
    for (int i = 0; i < devCnt; ++i) {
        devInfos.push_back(fnetDevInfos[i]);
    }
    return COM_OK;
}

ComErrno MultiComUtils::getTokenByPassword(const std::string &userName, const std::string &password,
    const std::string &language, com_token_data_t &tokenData, std::string &message, int msTimeout)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    time_t startTime = time(nullptr);
    fnet_token_data_t *fnetTokenData;
    char *fnetMessage = nullptr;
    fnet::FreeInDestructor freeFnetMessage(fnetMessage, intfc->freeString);
    int fnetRet = intfc->getTokenByPassword(userName.c_str(), password.c_str(), language.c_str(),
        &fnetTokenData, &fnetMessage, msTimeout);
    if (fnetMessage != nullptr) {
        message = fnetMessage;
    }
    if (fnetRet != FNET_OK) {
        return fnetRet2ComErrno(fnetRet);
    }
    fnet::FreeInDestructor freeTokenInfo(fnetTokenData, intfc->freeToken);
    tokenData.expiresIn = fnetTokenData->expiresIn;
    tokenData.accessToken = fnetTokenData->accessToken;
    tokenData.refreshToken = fnetTokenData->refreshToken;
    tokenData.startTime = startTime;
    return COM_OK;
}

ComErrno MultiComUtils::refreshToken(const std::string &refreshToken, com_token_data_t &tokenData, int msTimeout)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    time_t startTime = time(nullptr);
    fnet_token_data_t *fnetTokenData;
    int fnetRet = intfc->refreshToken(refreshToken.c_str(), "en", &fnetTokenData, nullptr, msTimeout);
    if (fnetRet != FNET_OK) {
        return fnetRet2ComErrno(fnetRet);
    }
    fnet::FreeInDestructor freeTokenData(fnetTokenData, intfc->freeToken);
    tokenData.expiresIn = fnetTokenData->expiresIn;
    tokenData.accessToken = fnetTokenData->accessToken;
    tokenData.refreshToken = fnetTokenData->refreshToken;
    tokenData.startTime = startTime;
    return COM_OK;
}

ComErrno MultiComUtils::getClientToken(com_clinet_token_data_t &clinetTokenData, int msTimeout)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    time_t startTime = time(nullptr);
    fnet_client_token_data *fnetClientTokenData;
    int fnetRet = intfc->getClientToken("en", &fnetClientTokenData, nullptr, msTimeout);
    if (fnetRet != FNET_OK) {
        return fnetRet2ComErrno(fnetRet);
    }
    fnet::FreeInDestructor freeClientTokenData(fnetClientTokenData, intfc->freeClientToken);
    clinetTokenData.accessToken = fnetClientTokenData->accessToken;
    clinetTokenData.expiresIn = fnetClientTokenData->expiresIn;
    clinetTokenData.startTime = startTime;
    return COM_OK;
}

ComErrno MultiComUtils::sendSMSCode(const std::string &clinetAccessToken, const std::string &phoneNumber,
    const std::string &language, std::string &message, int msTimeout)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    char *fnetMessage = nullptr;
    fnet::FreeInDestructor freeFnetMessage(fnetMessage, intfc->freeString);
    int fnetRet = intfc->sendSMSCode(clinetAccessToken.c_str(), phoneNumber.c_str(), language.c_str(),
        &fnetMessage, msTimeout);
    if (fnetMessage != nullptr) {
        message = fnetMessage;
    }
    if (fnetRet != FNET_OK) {
        return fnetRet2ComErrno(fnetRet);
    }
    return COM_OK;
}

ComErrno MultiComUtils::getTokenBySMSCode(const std::string &userName, const std::string &SMSCode,
    const std::string &language, com_token_data_t &tokenData, std::string &message, int msTimeout)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    time_t startTime = time(nullptr);
    fnet_token_data_t *fnetTokenData;
    char *fnetMessage = nullptr;
    fnet::FreeInDestructor freeFnetMessage(fnetMessage, intfc->freeString);
    int fnetRet = intfc->getTokenBySMSCode(userName.c_str(), SMSCode.c_str(), language.c_str(),
        &fnetTokenData, &fnetMessage, msTimeout);
    if (fnetMessage != nullptr) {
        message = fnetMessage;
    }
    if (fnetRet != FNET_OK) {
        return fnetRet2ComErrno(fnetRet);
    }
    fnet::FreeInDestructor freeTokenInfo(fnetTokenData, intfc->freeToken);
    tokenData.expiresIn = fnetTokenData->expiresIn;
    tokenData.accessToken = fnetTokenData->accessToken;
    tokenData.refreshToken = fnetTokenData->refreshToken;
    tokenData.startTime = startTime;
    return COM_OK;
}

ComErrno MultiComUtils::getUserProfile(const std::string &accessToken, com_user_profile_t &userProfile,
    int msTimeout)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    fnet_user_profile_t *fnetProfile;
    int fnetRet = intfc->getUserProfile(accessToken.c_str(), &fnetProfile, msTimeout);
    if (fnetRet != FNET_OK) {
        return fnetRet2ComErrno(fnetRet);
    }
    fnet::FreeInDestructor freeProfile(fnetProfile, intfc->freeUserProfile);
    userProfile.uid = fnetProfile->uid;
    userProfile.nickname = fnetProfile->nickname;
    userProfile.headImgUrl = fnetProfile->headImgUrl;
    userProfile.email = fnetProfile->email;
    return COM_OK;
}

ComErrno MultiComUtils::bindAccountRelp(const std::string &uid, const std::string &accessToken,
    const std::string &email, bool &showUserPoints, int msTimeout)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    fnet_bind_account_relp_result_t *bindResult;
    int fnetRet = intfc->bindAccountRelp(
        uid.c_str(), accessToken.c_str(), email.c_str(), &bindResult, msTimeout);
    if (fnetRet != FNET_OK) {
        return fnetRet2ComErrno(fnetRet);
    }
    fnet::FreeInDestructor freeBindResult(bindResult, intfc->freeBindAccountRelpResult);
    showUserPoints = bindResult->showUserPoints;
    return COM_OK;
}

ComErrno MultiComUtils::getNimData(const std::string &uid, const std::string &accessToken,
    com_nim_data_t &nimData, int msTimeout)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    fnet_nim_data_t * fnetNimData;
    int fnetRet = intfc->getNimData(uid.c_str(), accessToken.c_str(), &fnetNimData, msTimeout);
    if (fnetRet != FNET_OK) {
        return fnetRet2ComErrno(fnetRet);
    }
    fnet::FreeInDestructor freeNimData(fnetNimData, intfc->freeNimData);
    nimData.nimDataId = fnetNimData->nimDataId;
    nimData.appNimAccountId = fnetNimData->appNimAccountId;
    nimData.nimTeamId = fnetNimData->nimTeamId;
    return COM_OK;
}

ComErrno MultiComUtils::downloadFileMem(const std::string &url, std::vector<char> &bytes,
    fnet_progress_callback_t callback, void *callbackData, int msConnectTimeout, int msTimeout)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    fnet_file_data_t *fileData;
    int fnetRet = intfc->downloadFileMem(
        url.c_str(), &fileData, callback, callbackData, msConnectTimeout, msTimeout);
    if (fnetRet != FNET_OK) {
        return fnetRet2ComErrno(fnetRet);
    }
    fnet::FreeInDestructor freeFileData(fileData, intfc->freeFileData);
    bytes.assign(fileData->data, fileData->data + fileData->size);
    return COM_OK;
}

ComErrno MultiComUtils::downloadFileDisk(const std::string &url, const wxString &saveName,
    fnet_progress_callback_t callback, void *callbackData, int msConnectTimeout, int msTimeout)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    int fnetRet = intfc->downloadFileDisk(
        url.c_str(), saveName.ToUTF8().data(), callback, callbackData, msConnectTimeout, msTimeout);
    if (fnetRet != FNET_OK) {
        return fnetRet2ComErrno(fnetRet);
    }
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
    case FNET_GCODE_NOT_FOUND:
        return COM_GCODE_NOT_FOUND;
    case FNET_VERIFY_LAN_DEV_FAILED:
        return COM_VERIFY_LAN_DEV_FAILED;
    case FNET_UNAUTHORIZED:
        return COM_UNAUTHORIZED;
    case FNET_INVALID_VALIDATION:
        return COM_INVALID_VALIDATION;
    case FNET_DEVICE_HAS_BEEN_BOUND:
        return COM_DEVICE_HAS_BEEN_BOUND;
    case FNET_ABORT_AI_JOB_FAILED:
        return COM_ABORT_AI_JOB_FAILED;
    case FENT_AI_JOB_NOT_ENOUGH_POINTS:
        return COM_AI_JOB_NOT_ENOUGH_POINTS;
    case FNET_NO_EXISTING_AI_MODEL_JOB:
        return COM_NO_EXISTING_AI_MODEL_JOB;
    case FNET_INPUT_FAILED_THE_REVIEW:
        return COM_INPUT_FAILED_THE_REVIEW;
    case FNET_NIM_SEND_ERROR:
        return COM_NIM_SEND_ERROR;
    case FNET_NIM_DATA_BASE_ERROR:
        return COM_NIM_DATA_BASE_ERROR;
    default:
        return COM_ERROR;
    }
}

std::vector<fnet_material_mapping_t> MultiComUtils::comMaterialMappings2Fnet(
    const std::vector<com_material_mapping_t> &comMaterialMappings)
{
    std::vector<fnet_material_mapping_t> ret(comMaterialMappings.size());
    for (size_t i = 0; i < ret.size(); ++i) {
        const com_material_mapping_t &comMaterialMapping = comMaterialMappings[i];
        ret[i].toolId = comMaterialMapping.toolId;
        ret[i].slotId = comMaterialMapping.slotId;
        ret[i].materialName = comMaterialMapping.materialName.c_str();
        ret[i].toolMaterialColor = comMaterialMapping.toolMaterialColor.c_str();
        ret[i].slotMaterialColor = comMaterialMapping.slotMaterialColor.c_str();
    }
    return ret;
}

}} // namespace Slic3r::GUI
