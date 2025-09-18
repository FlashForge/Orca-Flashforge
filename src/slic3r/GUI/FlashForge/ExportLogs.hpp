#ifndef slic3r_GUI_ExportLogs_hpp_
#define slic3r_GUI_ExportLogs_hpp_

#include <utility>
#include <vector>
#include <wx/datetime.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/string.h>
#include "libslic3r/miniz_extension.hpp"

namespace Slic3r { namespace GUI {

struct ExportLogsFinishedEvent : public wxCommandEvent {
    ExportLogsFinishedEvent(wxEventType type, bool _succeed, const wxString _outputPath)
        : wxCommandEvent(type), succeed(_succeed), outputPath(_outputPath) {
    }
    bool succeed;
    wxString outputPath;
};

class ExportLogsDlg : public wxDialog
{
public:
    ExportLogsDlg(wxWindow *parent);

    void onExportLogsFinished(ExportLogsFinishedEvent &event);
};

class ExportLogs
{
public:
    static void exportLocal();

private:
    static std::vector<std::pair<wxString, std::vector<wxString>>> getRootLatestFiles(const wxString &rootPath,
        const wxDateTime &now);

    static std::vector<wxString> getDirLatestFiles(const wxString &dirPath, const wxString &prefix,
        const wxDateTime &now);

    static bool saveZip(const wxString &outputPath, const wxString &rootPath,
        const std::vector<std::pair<wxString, std::vector<wxString>>> &fileInfos);

    static bool addZipFile(mz_zip_archive *zipArchive, const std::string &dstPath, const wxString &srcPath);
};

}} // namespace Slic3r::GUI

#endif
