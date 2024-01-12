#ifndef slic3r_GUI_ComCommandQue_hpp_
#define slic3r_GUI_ComCommandQue_hpp_

#include <list>
#include <memory>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include "ComCommand.hpp"

namespace Slic3r { namespace GUI {

typedef std::shared_ptr<ComCommand> ComCommandPtr;

class ComCommandQue
{
public:
    void clear();

    ComCommandPtr get(int commandId);

    ComCommandPtr getFront(unsigned int milliseconds = -1);

    void pop(int commandId);

    void pushBack(const ComCommandPtr &command, int priority, bool checkDup = false);

private:
    struct QueItem {
        ComCommandPtr command;
        int priority; // The smaller the value, the higher the priority.
    };

    void increasePriority(const std::list<QueItem>::iterator &it, int priority);

private:
    boost::mutex              m_mutex;
    boost::condition_variable m_condition;
    std::list<QueItem>        m_commandList;
};

}} // namespace Slic3r::GUI

#endif
