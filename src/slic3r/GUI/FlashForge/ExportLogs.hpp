#ifndef slic3r_GUI_ExportLogs_hpp_
#define slic3r_GUI_ExportLogs_hpp_

#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include <wx/datetime.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/file.h>
#include <wx/string.h>
#include "libslic3r/miniz_extension.hpp"

namespace Slic3r { namespace GUI {

class WaitEvent;

wxDECLARE_EVENT(EVT_UPLOAD_LOG_PROGRESS, wxCommandEvent);

struct ExportLogsFinishedEvent : public wxCommandEvent
{
    ExportLogsFinishedEvent(wxEventType type, bool _succeed, const wxString _outputPath)
        : wxCommandEvent(type), succeed(_succeed), outputPath(_outputPath)
    {}
    bool     succeed;
    wxString outputPath;
};

class EmailInputDialog : public wxDialog
{
public:
    EmailInputDialog(wxWindow* parent, const wxString& title);
    wxString getText();

private:
    wxTextCtrl* m_text;
};

class ExportLogsDlg : public wxDialog
{
public:
    ExportLogsDlg(wxWindow* parent);

    void onExportLogsFinished(ExportLogsFinishedEvent& event);
};

class ExportLogs
{
public:
    static void exportLocal();

    static void uploadLocal();

    static bool uploadFeedback(const std::string& clientId,
                               const std::string& accessToken,
                               const std::string& feedbackId,
                               WaitEvent*         cancelEvent);

private:
    static std::vector<std::pair<wxString, std::vector<wxString>>> getRootLatestFiles(const wxString&   rootPath,
                                                                                      const wxDateTime& now,
                                                                                      WaitEvent*        cancelEvent = nullptr,
                                                                                      size_t            maxFilesPerDirectory = 0);

    static std::vector<wxString> getDirLatestFiles(const wxString&   dirPath,
                                                   const wxString&   prefix,
                                                   const wxDateTime& now,
                                                   WaitEvent*        cancelEvent = nullptr,
                                                   size_t            maxCandidates = 0);

    static bool saveZip(const wxString&                                                outputPath,
                        const wxString&                                                rootPath,
                        const std::vector<std::pair<wxString, std::vector<wxString>>>& fileInfos,
                        bool                                                           sanitize,
                        WaitEvent*                                                     cancelEvent = nullptr);

    static bool addZipFile(mz_zip_archive*    zipArchive,
                           const std::string& dstPath,
                           const wxString&    srcPath,
                           bool               sanitize,
                           uintmax_t          sanitizedByteLimit,
                           wxFile*            sanitizedInput,
                           bool               strictNetworkLog,
                           WaitEvent*         cancelEvent = nullptr);

    static bool isVirtualMachine();

    static wxString getDeviceHash();
};

}} // namespace Slic3r::GUI

#endif
