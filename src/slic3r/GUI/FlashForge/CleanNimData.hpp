#ifndef slic3r_GUI_CleanNimData_hpp_
#define slic3r_GUI_CleanNimData_hpp_

#include <chrono>
#include <string>
#include <wx/string.h>
#include "Singleton.hpp"

namespace Slic3r { namespace GUI {

class CleanNimData : public Singleton<CleanNimData>
{
public:
    void run(const std::string &nimAppDir, const std::string &fileLockName);

private:
    bool checkFlag();

    void setFlag(int cleanDay);

    void setFlagWhenDataBaseError();

    void clean(const std::string &fileLockName);

    void cleanNimLog(const wxString &nimLogDir);

private:
    wxString m_nimAppDir;
    wxString m_flagFilePath;
    std::chrono::steady_clock::time_point m_lastShowErrorTime;
    bool m_blockShowError;
};

}} // namespace Slic3r::GUI

#endif
