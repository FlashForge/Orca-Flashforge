#include "MultiComHelper.hpp"
#include "MultiComMgr.hpp"
#include "MultiComUtils.hpp"
#include "WanDevTokenMgr.hpp"

namespace Slic3r { namespace GUI {

MultiComHelper::MultiComHelper()
    : m_threadPool(5, 30000)
{
}

void MultiComHelper::userClickCount(const std::string &source, int msTimeout)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return;
    }
    m_threadPool.post([=]() {
        ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
        ComErrno ret = MultiComUtils::fnetRet2ComErrno(intfc->userClickCount(
            m_uid.c_str(), token.accessToken().c_str(), source.c_str(), msTimeout));
        if (ret != COM_OK) {
            BOOST_LOG_TRIVIAL(error) << "aiModelClickCount error, " << (int)ret;
        }
    });
}

void MultiComHelper::doBusGetRequest(const std::string &requestId, const std::string &target, int msTimeout)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return;
    }
    m_threadPool.post([=]() {
        ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
        char *responseData;
        ComErrno ret = MultiComUtils::fnetRet2ComErrno(intfc->doBusGetRequest(
            m_uid.c_str(), token.accessToken().c_str(), target.c_str(), &responseData, msTimeout));
        fnet::FreeInDestructor freeResponseData(responseData, intfc->freeString);
        if (responseData != nullptr) {
            QueueEvent(new ComBusGetRequestEvent(COM_BUS_GET_REQUEST_EVENT, requestId, responseData, ret));
        } else {
            QueueEvent(new ComBusGetRequestEvent(COM_BUS_GET_REQUEST_EVENT, requestId, "", ret));
        }
    });
}

ComErrno MultiComHelper::singOut(int msTimeout)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
    return MultiComUtils::fnetRet2ComErrno(intfc->signOut(token.accessToken().c_str(), msTimeout));
}

ComErrno MultiComHelper::getUserAiPointsInfo(com_user_ai_points_info_t &userAiPointsInfo, int msTimeout)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
    fnet_user_ai_points_info_t *fnetUserAiPointsInfo;
    ComErrno ret = MultiComUtils::fnetRet2ComErrno(intfc->getUserAiPointsInfo(
        m_uid.c_str(), token.accessToken().c_str(), &fnetUserAiPointsInfo, msTimeout));
    if (ret != COM_OK) {
        return ret;
    }
    fnet::FreeInDestructor freeDevInfos(fnetUserAiPointsInfo, intfc->freeUserAiPointsInfo);
    userAiPointsInfo.totalPoints = fnetUserAiPointsInfo->totalPoints;
    userAiPointsInfo.modelGenPoints = fnetUserAiPointsInfo->modelGenPoints;
    userAiPointsInfo.img2imgPoints = fnetUserAiPointsInfo->img2imgPoints;
    userAiPointsInfo.txt2txtPoints = fnetUserAiPointsInfo->txt2txtPoints;
    userAiPointsInfo.txt2imgPoints = fnetUserAiPointsInfo->txt2imgPoints;
    userAiPointsInfo.remainingFreeCount = fnetUserAiPointsInfo->remainingFreeCount;
    userAiPointsInfo.freeRetriesPerProcess = fnetUserAiPointsInfo->freeRetriesPerProcess;
    return ret;
}

ComErrno MultiComHelper::uploadAiImageClound(const std::string &filePath, const std::string &saveName,
    std::string &storeUrl, fnet_progress_callback_t callback, void *callbackData, int msTimeout)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
    fnet_upload_file_data_t uploadFileData;
    uploadFileData.filePath = filePath.c_str();
    uploadFileData.saveName = saveName.c_str();
    uploadFileData.callback = callback;
    uploadFileData.callbackData = callbackData;
    fnet_clound_file_data_t *cloundFileData;
    ComErrno ret = MultiComUtils::fnetRet2ComErrno(intfc->uploadAiImageClound(
        m_uid.c_str(), token.accessToken().c_str(), &uploadFileData, &cloundFileData, msTimeout));
    if (ret != COM_OK) {
        return ret;
    }
    fnet::FreeInDestructor freeCloundFileData(cloundFileData, intfc->freeCloundFileData);
    storeUrl = cloundFileData->storageUrl;
    return ret;
}

ComErrno MultiComHelper::createAiJobPipeline(const std::string &entryType,
    com_ai_job_pipeline_info_t &pipelineInfo, int msTimeout)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
    fnet_ai_job_pipeline_info_t *fnetPipelineInfo;
    ComErrno ret = MultiComUtils::fnetRet2ComErrno(intfc->createAiJobPipeline(
        m_uid.c_str(), token.accessToken().c_str(), entryType.c_str(), &fnetPipelineInfo, msTimeout));
    if (ret != COM_OK) {
        return ret;
    }
    fnet::FreeInDestructor freePipelineInfo(fnetPipelineInfo, intfc->freeAiJobPipelineInfo);
    pipelineInfo.id = fnetPipelineInfo->id;
    pipelineInfo.isFree = fnetPipelineInfo->isFree;
    return ret;
}

ComErrno MultiComHelper::startAiModelJob(int supplier, int64_t pipelineId, const std::string &imageUrl,
    const std::string &resultFormat, com_ai_model_job_result_t &jobResult, int msTimeout)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
    fnet_start_ai_model_job_data_t jobData;
    jobData.supplier = supplier;
    jobData.pipelineId = pipelineId;
    jobData.imageUrl = imageUrl.c_str();
    jobData.resultFormat = resultFormat.c_str();
    fnet_start_ai_model_job_result *fnetJobResult;
    ComErrno ret = MultiComUtils::fnetRet2ComErrno(intfc->startAiModelJob(
        m_uid.c_str(), token.accessToken().c_str(), &jobData, &fnetJobResult, msTimeout));
    if (ret != COM_OK) {
        return ret;
    }
    fnet::FreeInDestructor freeJobResult(fnetJobResult, intfc->freeStartAiModelJobResult);
    jobResult.status = fnetJobResult->status;
    jobResult.jobId = fnetJobResult->jobId;
    jobResult.posInQueue = fnetJobResult->posInQueue;
    jobResult.queueLength = fnetJobResult->queueLength;
    jobResult.isOldJob = fnetJobResult->isOldJob;
    return ret;
}

ComErrno MultiComHelper::getAiModelJobState(int64_t jobId, com_ai_model_job_state_t &jobState, int msTimeout)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
    fnet_ai_model_job_state_t *fnetJobState;
    ComErrno ret = MultiComUtils::fnetRet2ComErrno(intfc->getAiModelJobState(
        m_uid.c_str(), token.accessToken().c_str(), jobId, &fnetJobState, msTimeout));
    if (ret != COM_OK) {
        return ret;
    }
    fnet::FreeInDestructor freeJobState(fnetJobState, intfc->freeAiModelJobState);
    jobState.status = fnetJobState->status;
    jobState.jobId = fnetJobState->jobId;
    jobState.posInQueue = fnetJobState->posInQueue;
    jobState.queueLength = fnetJobState->queueLength;
    jobState.externalJobId = fnetJobState->externalJobId;
    jobState.models.resize(fnetJobState->modelCnt);
    for (int i = 0; i < fnetJobState->modelCnt; ++i) {
        jobState.models[i].modelType = fnetJobState->models[i].modelType;
        jobState.models[i].modelUrl = fnetJobState->models[i].modelUrl;
    }
    return ret;
}

ComErrno MultiComHelper::abortAiModelJob(int64_t jobId, int msTimeout)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
    ComErrno ret = MultiComUtils::fnetRet2ComErrno(intfc->abortAiModelJob(
        m_uid.c_str(), token.accessToken().c_str(), jobId, msTimeout));
    if (ret != COM_OK) {
        return ret;
    }
    return ret;
}

ComErrno MultiComHelper::getExistingAiModelJob(com_ai_model_job_result_t &jobResult, int msTimeout)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
    fnet_start_ai_model_job_result *fnetJobResult;
    ComErrno ret = MultiComUtils::fnetRet2ComErrno(intfc->getExistingAiModelJob(
        m_uid.c_str(), token.accessToken().c_str(), &fnetJobResult, msTimeout));
    if (ret != COM_OK) {
        return ret;
    }
    fnet::FreeInDestructor freeJobResult(fnetJobResult, intfc->freeStartAiModelJobResult);
    jobResult.status = fnetJobResult->status;
    jobResult.jobId = fnetJobResult->jobId;
    jobResult.posInQueue = fnetJobResult->posInQueue;
    jobResult.queueLength = fnetJobResult->queueLength;
    jobResult.isOldJob = fnetJobResult->isOldJob;
    return ret;
}

ComErrno MultiComHelper::startAiImg2imgJob(int supplier, int64_t pipelineId, const std::string &imageUrl,
    com_ai_general_job_result_t &jobResult, int msTimeout)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
    fnet_start_ai_general_job_data_t jobData;
    jobData.supplier = supplier;
    jobData.pipelineId = pipelineId;
    jobData.prompt = "";
    jobData.imageUrl = imageUrl.c_str();
    fnet_start_ai_general_job_result_t *fnetJobResult;
    ComErrno ret = MultiComUtils::fnetRet2ComErrno(intfc->startAiImg2imgJob(
        m_uid.c_str(), token.accessToken().c_str(), &jobData, &fnetJobResult, msTimeout));
    if (ret != COM_OK) {
        return ret;
    }
    fnet::FreeInDestructor freeJobResult(fnetJobResult, intfc->freeStartAiGeneralJobResult);
    jobResult.status = fnetJobResult->status;
    jobResult.jobId = fnetJobResult->jobId;
    jobResult.posInQueue = fnetJobResult->posInQueue;
    jobResult.queueLength = fnetJobResult->queueLength;
    jobResult.remainingFreeRetries = fnetJobResult->remainingFreeRetries;
    return ret;
}

ComErrno MultiComHelper::startAiTxt2txtJob(int supplier, int64_t pipelineId, const std::string &prompt,
    com_ai_general_job_result_t &jobResult, int msTimeout)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
    fnet_start_ai_general_job_data_t jobData;
    jobData.supplier = supplier;
    jobData.pipelineId = pipelineId;
    jobData.prompt = prompt.c_str();
    jobData.imageUrl = "";
    fnet_start_ai_general_job_result_t *fnetJobResult;
    ComErrno ret = MultiComUtils::fnetRet2ComErrno(intfc->startAiTxt2txtJob(
        m_uid.c_str(), token.accessToken().c_str(), &jobData, &fnetJobResult, msTimeout));
    if (ret != COM_OK) {
        return ret;
    }
    fnet::FreeInDestructor freeJobResult(fnetJobResult, intfc->freeStartAiGeneralJobResult);
    jobResult.status = fnetJobResult->status;
    jobResult.jobId = fnetJobResult->jobId;
    jobResult.posInQueue = fnetJobResult->posInQueue;
    jobResult.queueLength = fnetJobResult->queueLength;
    jobResult.remainingFreeRetries = fnetJobResult->remainingFreeRetries;
    return ret;
}

ComErrno MultiComHelper::startAiTxt2imgJob(int supplier, int64_t pipelineId, const std::string &prompt,
    com_ai_general_job_result_t &jobResult, int msTimeout)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
    fnet_start_ai_general_job_data_t jobData;
    jobData.supplier = supplier;
    jobData.pipelineId = pipelineId;
    jobData.prompt = prompt.c_str();
    jobData.imageUrl = "";
    fnet_start_ai_general_job_result_t *fnetJobResult;
    ComErrno ret = MultiComUtils::fnetRet2ComErrno(intfc->startAiTxt2imgJob(
        m_uid.c_str(), token.accessToken().c_str(), &jobData, &fnetJobResult, msTimeout));
    if (ret != COM_OK) {
        return ret;
    }
    fnet::FreeInDestructor freeJobResult(fnetJobResult, intfc->freeStartAiGeneralJobResult);
    jobResult.status = fnetJobResult->status;
    jobResult.jobId = fnetJobResult->jobId;
    jobResult.posInQueue = fnetJobResult->posInQueue;
    jobResult.queueLength = fnetJobResult->queueLength;
    jobResult.remainingFreeRetries = fnetJobResult->remainingFreeRetries;
    return ret;
}

ComErrno MultiComHelper::getAiImg2imgJobState(int64_t jobId, com_ai_general_job_state_t &jobState, int msTimeout)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
    fnet_ai_general_job_state_t *fnetJobState;
    ComErrno ret = MultiComUtils::fnetRet2ComErrno(intfc->getAiImg2imgJobState(
        m_uid.c_str(), token.accessToken().c_str(), jobId, &fnetJobState, msTimeout));
    if (ret != COM_OK) {
        return ret;
    }
    fnet::FreeInDestructor freeJobState(fnetJobState, intfc->freeAiGeneralJobState);
    jobState.status = fnetJobState->status;
    jobState.jobId = fnetJobState->jobId;
    jobState.posInQueue = fnetJobState->posInQueue;
    jobState.queueLength = fnetJobState->queueLength;
    jobState.externalJobId = fnetJobState->externalJobId;
    jobState.datas.resize(fnetJobState->dataCnt);
    for (int i = 0; i < fnetJobState->dataCnt; ++i) {
        jobState.datas[i].content = fnetJobState->datas[i].content;
        jobState.datas[i].imageUrl = fnetJobState->datas[i].imageUrl;
    }
    return ret;
}

ComErrno MultiComHelper::getAiTxt2txtJobState(int64_t jobId, com_ai_general_job_state_t &jobState, int msTimeout)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
    fnet_ai_general_job_state_t *fnetJobState;
    ComErrno ret = MultiComUtils::fnetRet2ComErrno(intfc->getAiTxt2txtJobState(
        m_uid.c_str(), token.accessToken().c_str(), jobId, &fnetJobState, msTimeout));
    if (ret != COM_OK) {
        return ret;
    }
    fnet::FreeInDestructor freeJobState(fnetJobState, intfc->freeAiGeneralJobState);
    jobState.status = fnetJobState->status;
    jobState.jobId = fnetJobState->jobId;
    jobState.posInQueue = fnetJobState->posInQueue;
    jobState.queueLength = fnetJobState->queueLength;
    jobState.externalJobId = fnetJobState->externalJobId;
    jobState.datas.resize(fnetJobState->dataCnt);
    for (int i = 0; i < fnetJobState->dataCnt; ++i) {
        jobState.datas[i].content = fnetJobState->datas[i].content;
        jobState.datas[i].imageUrl = fnetJobState->datas[i].imageUrl;
    }
    return ret;
}

ComErrno MultiComHelper::getAiTxt2imgJobState(int64_t jobId, com_ai_general_job_state_t &jobState, int msTimeout)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
    fnet_ai_general_job_state_t *fnetJobState;
    ComErrno ret = MultiComUtils::fnetRet2ComErrno(intfc->getAiTxt2imgJobState(
        m_uid.c_str(), token.accessToken().c_str(), jobId, &fnetJobState, msTimeout));
    if (ret != COM_OK) {
        return ret;
    }
    fnet::FreeInDestructor freeJobState(fnetJobState, intfc->freeAiGeneralJobState);
    jobState.status = fnetJobState->status;
    jobState.jobId = fnetJobState->jobId;
    jobState.posInQueue = fnetJobState->posInQueue;
    jobState.queueLength = fnetJobState->queueLength;
    jobState.externalJobId = fnetJobState->externalJobId;
    jobState.datas.resize(fnetJobState->dataCnt);
    for (int i = 0; i < fnetJobState->dataCnt; ++i) {
        jobState.datas[i].content = fnetJobState->datas[i].content;
        jobState.datas[i].imageUrl = fnetJobState->datas[i].imageUrl;
    }
    return ret;
}

ComErrno MultiComHelper::abortAiImg2imgJob(int64_t jobId, int msTimeout)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
    ComErrno ret = MultiComUtils::fnetRet2ComErrno(intfc->abortAiImg2imgJob(
        m_uid.c_str(), token.accessToken().c_str(), jobId, msTimeout));
    if (ret != COM_OK) {
        return ret;
    }
    return ret;
}

ComErrno MultiComHelper::abortAiTxt2txtJob(int64_t jobId, int msTimeout)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
    ComErrno ret = MultiComUtils::fnetRet2ComErrno(intfc->abortAiTxt2txtJob(
        m_uid.c_str(), token.accessToken().c_str(), jobId, msTimeout));
    if (ret != COM_OK) {
        return ret;
    }
    return ret;
}

ComErrno MultiComHelper::abortAiTxt2imgJob(int64_t jobId, int msTimeout)
{
    fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc == nullptr) {
        return COM_ERROR;
    }
    ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
    ComErrno ret = MultiComUtils::fnetRet2ComErrno(intfc->abortAiTxt2imgJob(
        m_uid.c_str(), token.accessToken().c_str(), jobId, msTimeout));
    if (ret != COM_OK) {
        return ret;
    }
    return ret;
}

}} // namespace Slic3r::GUI
