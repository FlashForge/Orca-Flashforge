#ifndef slic3r_GUI_ComThreadPool_hpp_
#define slic3r_GUI_ComThreadPool_hpp_

#include <condition_variable>
#include <functional>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <list>

namespace Slic3r { namespace GUI {

class ComThreadPool;

class ComTaskThread
{
public:
    ComTaskThread(ComThreadPool *threadPool);

private:
    friend ComThreadPool;

    typedef std::list<ComTaskThread> task_threads_t;

    void run();

private:
    task_threads_t::iterator m_threadsIt;
    ComThreadPool *m_threadPool;
    std::thread m_thread;
};

class ComThreadPool
{
public:
    ComThreadPool(size_t maxThreadCnt, int expiryTimeout);

    ~ComThreadPool();

    void post(const std::function<void()> &task);

    void wait();

    void clear();

private:
    friend ComTaskThread;

    typedef std::deque<std::function<void()>> task_queue_t;

private:
    task_queue_t                  m_tasks;
    ComTaskThread::task_threads_t m_threads;
    std::mutex                    m_mutex;
    std::condition_variable       m_postCondVar;
    std::condition_variable       m_finishCondVar;
    std::condition_variable       m_exitCondVar;
    size_t                        m_maxThreadCnt;
    size_t                        m_activeThreadCnt;
    int                           m_expiryTimeout;
    bool                          m_exitThreads;
};

}} // namespace Slic3r::GUI

#endif
