#ifndef slic3r_GUI_MultiComHelper_hpp_
#define slic3r_GUI_MultiComHelper_hpp_

#include "ComThreadPool.hpp"
#include "MultiComDef.hpp"
#include "Singleton.hpp"

namespace Slic3r { namespace GUI {

class MultiComHelper : public wxEvtHandler, public Singleton<MultiComHelper>
{
public:
    MultiComHelper();

    void setUid(const std::string &uid) { m_uid = uid; }

    void userClickCount(const std::string &source, int msTimeout);

    void doBusGetRequest(const std::string &requestId, const std::string &target, int msTimeout);

    ComErrno singOut(int msTimeout);

    ComErrno getUserAiPointsInfo(com_user_ai_points_info_t &userAiPointsInfo, int msTimeout);

    ComErrno uploadAiImageClound(const std::string &filePath, const std::string &saveName,
        std::string &storeUrl, fnet_progress_callback_t callback, void *callbackData, int msTimeout);

    ComErrno createAiJobPipeline(const std::string &entryType,
        com_ai_job_pipeline_info_t &pipelineInfo, int msTimeout); // entryType: text2text/img2img

    ComErrno startAiModelJob(int supplier, int64_t pipelineId, const std::string &imageUrl,
        const std::string &resultFormat, com_ai_model_job_result_t &jobResult, int msTimeout);

    ComErrno getAiModelJobState(int64_t jobId, com_ai_model_job_state_t &jobState,
        int msTimeout);

    ComErrno abortAiModelJob(int64_t jobId, int msTimeout);

    ComErrno getExistingAiModelJob(com_ai_model_job_result_t &jobResult, int msTimeout);

    ComErrno startAiImg2imgJob(int supplier, int64_t pipelineId, const std::string &imageUrl,
        com_ai_general_job_result_t &jobResult, int msTimeout);

    ComErrno startAiTxt2txtJob(int supplier, int64_t pipelineId, const std::string &prompt,
        com_ai_general_job_result_t &jobResult, int msTimeout);

    ComErrno startAiTxt2imgJob(int supplier, int64_t pipelineId, const std::string &prompt,
        com_ai_general_job_result_t &jobResult, int msTimeout);

    ComErrno getAiImg2imgJobState(int64_t jobId, com_ai_general_job_state_t &jobState, int msTimeout);

    ComErrno getAiTxt2txtJobState(int64_t jobId, com_ai_general_job_state_t &jobState, int msTimeout);

    ComErrno getAiTxt2imgJobState(int64_t jobId, com_ai_general_job_state_t &jobState, int msTimeout);

    ComErrno abortAiImg2imgJob(int64_t jobId, int msTimeout);

    ComErrno abortAiTxt2txtJob(int64_t jobId, int msTimeout);

    ComErrno abortAiTxt2imgJob(int64_t jobId, int msTimeout);

private:
    std::string m_uid;
    ComThreadPool m_threadPool;
};

}} // namespace Slic3r::GUI

#endif
