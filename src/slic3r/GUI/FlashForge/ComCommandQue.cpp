#include "ComCommandQue.hpp"
#include <utility>

namespace Slic3r { namespace GUI {

void ComCommandQue::clear()
{
    boost::mutex::scoped_lock lock(m_mutex);
    m_commandList.clear();
}

ComCommandPtr ComCommandQue::get(int commandId)
 {
     for (auto &item : m_commandList) {
        if (item.command->commandId() == commandId) {
            return item.command;
        }
     }
     return nullptr;
 }

ComCommandPtr ComCommandQue::getFront(unsigned int milliseconds /* = -1 */)
{
    boost::mutex::scoped_lock lock(m_mutex);
    m_condition.wait_for(lock, boost::chrono::milliseconds(milliseconds));
    if (m_commandList.empty()) {
        return nullptr;
    } else {
        return m_commandList.front().command;
    }
}

void ComCommandQue::pop(int commandId)
{
    boost::mutex::scoped_lock lock(m_mutex);
    for (auto it = m_commandList.begin(); it != m_commandList.end(); ++it) {
        if (it->command->commandId() == commandId) {
            m_commandList.erase(it);
            break;
        }
    }
}

void ComCommandQue::pushBack(const ComCommandPtr &command, int priority, bool checkDup/* = false */)
{
    boost::mutex::scoped_lock lock(m_mutex);
    if (checkDup) {
        for (auto it = m_commandList.begin(); it != m_commandList.end(); ++it) {
            if (command->isDup(it->command.get())) {
                increasePriority(it, priority);
                return;
            }
        }
    }
    auto it = m_commandList.rbegin();
    for (; it != m_commandList.rend(); ++it) {
        if (it->priority <= priority) {
            break;
        }
    }
    QueItem item = { command, priority };
    m_commandList.insert(it.base(), item);
    m_condition.notify_one();
}

void ComCommandQue::increasePriority(const std::list<QueItem>::iterator &it, int priority)
{
    if (it->priority <= priority) {
        return;
    }
    it->priority = priority;
    auto revIt = std::make_reverse_iterator(it);
    auto tmpIt = revIt;
    for (; tmpIt != m_commandList.rend(); ++tmpIt) {
        if (tmpIt->priority <= priority) {
            break;
        }
    }
    if (tmpIt != revIt) {
        m_commandList.splice(tmpIt.base(), m_commandList, it);
    }
}

}} // namespace Slic3r::GUI
