#include "FFDownloadTool.hpp"
#include "slic3r/GUI/FlashForge/MultiComUtils.hpp"

namespace Slic3r { namespace GUI {

wxDEFINE_EVENT(EVT_FF_DOWNLOAD_FINISHED, FFDownloadFinishedEvent);

FFDownloadTool::FFDownloadTool(size_t maxThreadCnt, int expiryTimeout)
    : m_baseTaskId(InvalidTaskId + 1)
    , m_threadPool(maxThreadCnt, expiryTimeout)
{
}

FFDownloadTool::~FFDownloadTool()
{
    wait(false);
}

int FFDownloadTool::downloadMem(const std::string &url, int msConnectTimeout, int msTimeout)
{
    int taskId = newTaskId();
    m_threadPool.post([this, taskId, url, msConnectTimeout, msTimeout]() {
        insertAbortFlag(taskId);
        std::vector<char> bytes;
        call_back_data_t callbackData = { this, taskId };
        ComErrno ret = MultiComUtils::downloadFileMem(
            url, bytes, callback, &callbackData, msConnectTimeout, msTimeout);
        removeAbortFlag(taskId);

        FFDownloadFinishedEvent *event = new FFDownloadFinishedEvent;
        event->SetEventType(EVT_FF_DOWNLOAD_FINISHED);
        event->taskId = taskId;
        event->succeed = ret == COM_OK;
        event->data = std::move(bytes);
        QueueEvent(event);
    });
    return taskId;
}

int FFDownloadTool::downloadDisk(const std::string &url, const wxString &saveName,
    int msConnectTimeout, int msTimeout)
{
    int taskId = newTaskId();
    m_threadPool.post([this, taskId, url, saveName, msConnectTimeout, msTimeout]() {
        insertAbortFlag(taskId);
        call_back_data_t callbackData = { this, taskId };
        ComErrno ret = MultiComUtils::downloadFileDisk(
            url, saveName, callback, &callbackData, msConnectTimeout, msTimeout);
        removeAbortFlag(taskId);

        FFDownloadFinishedEvent *event = new FFDownloadFinishedEvent;
        event->SetEventType(EVT_FF_DOWNLOAD_FINISHED);
        event->taskId = taskId;
        event->succeed = ret == COM_OK;
        QueueEvent(event);
    });
    return taskId;
}

bool FFDownloadTool::abort(int taskId)
{
    std::unique_lock lock(m_abortFlagMutex);
    auto it = m_abortFlagMap.find(taskId);
    if (it == m_abortFlagMap.end()) {
        return false;
    }
    it->second = true;
    return true;
}

int FFDownloadTool::newTaskId()
{
    std::unique_lock lock(m_taskIdMutex);
    int taskId = m_baseTaskId++;
    if (m_baseTaskId == InvalidTaskId) {
        m_baseTaskId = InvalidTaskId + 1;
    }
    return taskId;
}

void FFDownloadTool::insertAbortFlag(int taskId)
{
    std::unique_lock lock(m_abortFlagMutex);
    m_abortFlagMap.emplace(taskId, false);
}

void FFDownloadTool::removeAbortFlag(int taskId)
{
    std::unique_lock lock(m_abortFlagMutex);
    m_abortFlagMap.erase(taskId);
}

void FFDownloadTool::wait(bool abortTasks)
{
    if (abortTasks) {
        m_threadPool.clear();
        std::unique_lock lock(m_abortFlagMutex);
        for (auto &item : m_abortFlagMap) {
            item.second = true;
        }
    }
    m_threadPool.wait();
}

int FFDownloadTool::callback(long long now, long long total, void *callbackData)
{
    call_back_data_t *data = (call_back_data_t *)callbackData;
    std::unique_lock lock(data->self->m_abortFlagMutex);
    return data->self->m_abortFlagMap.at(data->taskId);
}

}} // namespace Slic3r::GUI
