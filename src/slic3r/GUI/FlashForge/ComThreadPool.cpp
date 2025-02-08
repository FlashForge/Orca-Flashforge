#include "ComThreadPool.hpp"
#include <chrono>

namespace Slic3r { namespace GUI {

ComTaskThread::ComTaskThread(ComThreadPool *threadPool)
    : m_threadPool(threadPool)
    , m_thread(&ComTaskThread::run, this)
{
}

void ComTaskThread::run()
{
    std::unique_lock<std::mutex> lock(m_threadPool->m_mutex);
    while (!m_threadPool->m_exitThreads && !m_threadPool->m_tasks.empty()) {
        auto task = m_threadPool->m_tasks.front();
        m_threadPool->m_tasks.pop_front();
        m_threadPool->m_activeThreadCnt++;
        lock.unlock();
        task();
        lock.lock();
        m_threadPool->m_activeThreadCnt--;
        m_threadPool->m_finishCondVar.notify_all();
        if (!m_threadPool->m_exitThreads && m_threadPool->m_tasks.empty()) {
            auto expairTimeout = std::chrono::milliseconds(m_threadPool->m_expiryTimeout);
            m_threadPool->m_postCondVar.wait_for(lock, expairTimeout);
        }
    }
    m_thread.detach();
    std::condition_variable &exitCondVar = m_threadPool->m_exitCondVar;
    m_threadPool->m_threads.erase(m_threadsIt);
    exitCondVar.notify_all();
}

ComThreadPool::ComThreadPool(size_t maxThreadCnt, int expiryTimeout)
    : m_maxThreadCnt(maxThreadCnt)
    , m_activeThreadCnt(0)
    , m_expiryTimeout(expiryTimeout)
    , m_exitThreads(false)
{
}

ComThreadPool::~ComThreadPool()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_exitThreads = true;
    m_postCondVar.notify_all();
    while (!m_threads.empty()) {
        m_exitCondVar.wait(lock);
    }
}

void ComThreadPool::post(const std::function<void()> &task)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_tasks.push_back(task);
    if (m_tasks.size() == 1 && m_activeThreadCnt < m_threads.size()) {
        m_postCondVar.notify_one();
    } else if (m_threads.size() < m_maxThreadCnt) {
        m_threads.emplace_back(this);
        m_threads.back().m_threadsIt = --m_threads.end();
    }
}

void ComThreadPool::wait()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    while (!m_tasks.empty() || m_activeThreadCnt > 0) {
        m_finishCondVar.wait(lock);
    }
}

void ComThreadPool::clear()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_tasks.clear();
    m_finishCondVar.notify_all();
}

}} // namespace Slic3r::GUI
