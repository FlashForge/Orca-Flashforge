#include "ExportLogs.hpp"
#include <thread>
#include <boost/log/trivial.hpp>
#include <wx/datetime.h>
#include <wx/file.h>
#include <wx/filedlg.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/utils.h>
#include "libslic3r/Utils.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/MainFrame.hpp"
#include "slic3r/GUI/MsgDialog.hpp"
#include "slic3r/GUI/Widgets/Label.hpp"

namespace Slic3r { namespace GUI {

wxDEFINE_EVENT(EVT_EXPORT_LOGS_FINISHED, ExportLogsFinishedEvent);

ExportLogsDlg::ExportLogsDlg(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, _L("Export log"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxSYSTEM_MENU)
{
    SetBackgroundColour(*wxWHITE);
    SetSize(FromDIP(wxSize(400, 200)));
    SetMinSize(FromDIP(wxSize(400, 200)));
    SetMaxSize(FromDIP(wxSize(400, 200)));

    wxStaticText *msgStatText = new wxStaticText(this, wxID_ANY, _L("Exporting, please wait"));
    msgStatText->SetFont(Label::Body_14);

    wxSizer *sizer = new wxBoxSizer(wxVERTICAL);
    sizer->AddStretchSpacer(3);
    sizer->Add(msgStatText, 0, wxALIGN_CENTER);
    sizer->AddStretchSpacer(4);

    SetSizer(sizer);
    Layout();
    CenterOnParent();
}

void ExportLogsDlg::onExportLogsFinished(ExportLogsFinishedEvent &event)
{
    EndModal(wxID_OK);
    if (event.succeed) {
        CallAfter([outputPath = event.outputPath]() {
            MessageDialog dlg(wxGetApp().mainframe, _CTX("Export successful", "Flashforge"), _L("Export log"));
            dlg.ShowModal();
            wxLaunchDefaultApplication(wxFileName(outputPath).GetPath());
        });
    } else {
        CallAfter([outputPath = event.outputPath]() {
            MessageDialog dlg(wxGetApp().mainframe, _L("Export failed, please try again"), _L("Export log"));
            dlg.ShowModal();
            if (wxFileName::FileExists(outputPath)) {
                wxRemoveFile(outputPath);
            }
        });
    }
}

void ExportLogs::exportLocal()
{
    wxString defFileName = wxDateTime::Now().Format("%Y-%m-%d_%H-%M-%S.zip");
    wxFileDialog fileDlg(wxGetApp().mainframe, "", "", defFileName, "*.zip", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (fileDlg.ShowModal() != wxID_OK) {
        return;
    }
    flush_logs();
    wxString outputPath = fileDlg.GetPath();
    wxString rootPath = wxString::FromUTF8(data_dir());
    ExportLogsDlg exportDlg(wxGetApp().mainframe);
    exportDlg.Bind(EVT_EXPORT_LOGS_FINISHED, &ExportLogsDlg::onExportLogsFinished, &exportDlg);
    auto thread = std::thread([&]() {
        auto fileInfos = getRootLatestFiles(rootPath, wxDateTime::Now());
        bool succeed = saveZip(outputPath, rootPath, fileInfos);
        wxQueueEvent(&exportDlg, new ExportLogsFinishedEvent(EVT_EXPORT_LOGS_FINISHED, succeed, outputPath));
    });
    exportDlg.ShowModal();
    thread.join();
}

std::vector<std::pair<wxString, std::vector<wxString>>> ExportLogs::getRootLatestFiles(const wxString &rootPath,
    const wxDateTime &now)
{
    std::vector<std::pair<wxString, std::vector<wxString>>> fileInfos;
    fileInfos.emplace_back("log", getDirLatestFiles(rootPath + "/log", "debug_", now));
    fileInfos.emplace_back("FlashNetwork", getDirLatestFiles(rootPath + "/FlashNetwork", "", now));
    wxDir dir(rootPath);
    wxString dirName;
    if (dir.GetFirst(&dirName, wxEmptyString, wxDIR_DIRS)) {
        do {
            if (dirName.StartsWith("nimData")) {
                wxString dstDirPath = wxString::Format("%s/%s/log", rootPath, dirName);
                fileInfos.emplace_back(dirName + "/log", getDirLatestFiles(dstDirPath, "nim_", now));
            }
        } while (dir.GetNext(&dirName));
    }
    return fileInfos;
}

std::vector<wxString> ExportLogs::getDirLatestFiles(const wxString &dirPath, const wxString &prefix,
    const wxDateTime &now)
{
    wxDir dir(dirPath);
    wxString fileName;
    std::vector<wxString> fileNames;
    if (dir.GetFirst(&fileName, wxEmptyString, wxDIR_FILES)) {
        do {
            if (prefix.empty() || fileName.StartsWith(prefix)) {
                wxDateTime fileDateTime = wxFileName(dirPath + '/' + fileName).GetModificationTime();
                if (fileDateTime.IsValid()) {
                    if ((now - fileDateTime).GetHours() <= 7 * 24) {
                        fileNames.push_back(fileName);
                    }
                }
            }
        } while (dir.GetNext(&fileName));
    }
    return fileNames;
}

bool ExportLogs::saveZip(const wxString &outputPath, const wxString &rootPath,
    const std::vector<std::pair<wxString, std::vector<wxString>>> &fileInfos)
{
    mz_zip_archive zipArchive;
    mz_zip_zero_struct(&zipArchive);
    if (!open_zip_writer(&zipArchive, outputPath.utf8_string())) {
        BOOST_LOG_TRIVIAL(error) << mz_zip_get_error_string(mz_zip_get_last_error(&zipArchive));
        return false;
    }
    std::unique_ptr<mz_zip_archive, decltype(&close_zip_writer)> freeCurl(&zipArchive, close_zip_writer);
    for (auto &fileInfo : fileInfos) {
        wxString dirPath = fileInfo.first + '/';
        std::string encodedDirPath = encode_path(dirPath.utf8_string().c_str());
        if (!mz_zip_writer_add_mem(&zipArchive, encodedDirPath.c_str(), nullptr, 0, MZ_DEFAULT_COMPRESSION)) {
            BOOST_LOG_TRIVIAL(error) << mz_zip_get_error_string(mz_zip_get_last_error(&zipArchive));
            return false;
        }
        for (auto &fileName : fileInfo.second) {
            std::string dstPath = encode_path((dirPath + fileName).utf8_string().c_str());
            std::string srcPath = encode_path((rootPath + '/' + dirPath + fileName).utf8_string().c_str());
            if (!addZipFile(&zipArchive, dstPath, srcPath)) {
                return false;
            }
        }
    }
    if (!mz_zip_writer_finalize_archive(&zipArchive)) {
        BOOST_LOG_TRIVIAL(error) << mz_zip_get_error_string(mz_zip_get_last_error(&zipArchive));
        return false;
    }
    return true;
}

bool ExportLogs::addZipFile(mz_zip_archive *zipArchive, const std::string &dstPath, const wxString &srcPath)
{
    std::string encodedSrcPath = encode_path(srcPath.utf8_string().c_str());
    if (mz_zip_writer_add_file(zipArchive, dstPath.c_str(), encodedSrcPath.c_str(), nullptr, 0,
        MZ_DEFAULT_COMPRESSION)) {
        return true;
    }
    mz_zip_error error = mz_zip_get_last_error(zipArchive);
    if (error != MZ_ZIP_FILE_OPEN_FAILED) {
        BOOST_LOG_TRIVIAL(error) << mz_zip_get_error_string(error);
        return false;
    }
    wxFile file;
    if (!file.Open(srcPath, wxFile::read)) {
        BOOST_LOG_TRIVIAL(error) << "wxFile::Open error";
        return false;
    }
    wxFileOffset fileSize = file.Length();
    if (fileSize == wxInvalidOffset) {
        BOOST_LOG_TRIVIAL(error) << "wxFile::Length error";
        return false;
    }
    std::vector<char> buf(fileSize);
    if (file.Read(buf.data(), fileSize) != fileSize) {
        BOOST_LOG_TRIVIAL(error) << "wxFile::Read error";
        return false;
    }
    if (!mz_zip_writer_add_mem(zipArchive, dstPath.c_str(), buf.data(), fileSize, MZ_DEFAULT_COMPRESSION)) {
        BOOST_LOG_TRIVIAL(error) << mz_zip_get_error_string(mz_zip_get_last_error(zipArchive));
        return false;
    }
    return true;
}

}} // namespace Slic3r::GUI
