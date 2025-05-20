#include "CleanNimData.hpp"
#include <cstdint>
#include <ctime>
#include <wx/datetime.h>
#include <wx/dir.h>
#include <wx/fileconf.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/MsgDialog.hpp"
#include "slic3r/GUI/FlashForge/ComWanNimConn.hpp"

namespace Slic3r { namespace GUI {

void CleanNimData::run(const std::string &nimAppDir, const std::string &fileLockName)
{
    m_nimAppDir = wxString::FromUTF8(nimAppDir);
    m_flagFilePath = m_nimAppDir + "/ff_clean_flag";
    m_lastShowErrorTime = std::chrono::steady_clock::time_point::min();
    m_blockShowError = false;
    if (checkFlag()) {
        clean(fileLockName);
        setFlag(7);
    }
    setFlagWhenDataBaseError();
}

bool CleanNimData::checkFlag()
{
    if (!wxFileName::FileExists(m_flagFilePath)) {
        return true;
    }
    wxFileInputStream inputStream(m_flagFilePath);
    wxFileConfig fileConfig(inputStream);
    int64_t now = time(nullptr);
    if (fileConfig.ReadLongLong("cleanTime", now) <= now) {
        return true;
    }
    return false;
}

void CleanNimData::setFlagWhenDataBaseError()
{
    ComWanNimConn::inst()->Bind(WAN_CONN_NIM_DATA_BASE_ERROR_EVENT, [this](wxCommandEvent &) {
        std::chrono::duration<double> duration;
        if (m_lastShowErrorTime == std::chrono::steady_clock::time_point::min()) {
            duration = std::chrono::duration<double>::max();
        } else {
            duration = std::chrono::steady_clock::now() - m_lastShowErrorTime;
        }
        if (!m_blockShowError && duration.count() >= 15) {
            m_blockShowError = true;
            wxString msgText = _L("For an improved experience, please restart Orca-Flashforge to load resources.");
            MessageDialog msgDlg(nullptr, msgText, _L("Information"), wxICON_WARNING | wxOK);
            msgDlg.ShowModal();
            setFlag(0);
            m_blockShowError = false;
            m_lastShowErrorTime = std::chrono::steady_clock::now();
        }
    });
}

void CleanNimData::setFlag(int cleanDay)
{
    wxFileConfig fileConfig;
    fileConfig.Write("cleanTime", time(nullptr) + (int64_t)cleanDay * 24 * 3600);

    wxFileOutputStream outputStream(m_flagFilePath);
    fileConfig.Save(outputStream);
}

void CleanNimData::clean(const std::string &fileLockName)
{
    wxDir dir(m_nimAppDir);
    wxString fileName;
    if (dir.GetFirst(&fileName)) {
        do {
            wxString filePath = m_nimAppDir + "/" + fileName;
            if (wxFileName::DirExists(filePath)) {
                if (fileName != "log") {
                    wxFileName::Rmdir(filePath, wxPATH_RMDIR_RECURSIVE);
                } else {
                    cleanNimLog(filePath);
                }
            } else if (fileName != fileLockName) {
                wxRemoveFile(filePath);
            }
        } while (dir.GetNext(&fileName));
    }
}

void CleanNimData::cleanNimLog(const wxString &nimLogDir)
{
    wxDateTime now = wxDateTime::Now();
    wxDir dir(nimLogDir);
    wxString fileName;
    if (dir.GetFirst(&fileName)) {
        do {
            if (fileName.StartsWith("nim_") && fileName.size() >= 12) {
                wxDateTime fileDateTime;
                if (fileDateTime.ParseFormat(fileName.substr(4, 8), "%Y%m%d", now)) {
                    wxTimeSpan span = now - fileDateTime;
                    if (span.GetDays() > 7) {
                        wxRemoveFile(nimLogDir + "/" + fileName);
                    }
                }
            }
        } while (dir.GetNext(&fileName));
    }
}

}} // namespace Slic3r::GUI
