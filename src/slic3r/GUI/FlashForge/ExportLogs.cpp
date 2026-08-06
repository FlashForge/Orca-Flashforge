#include "ExportLogs.hpp"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <regex>
#include <thread>
#include <boost/log/trivial.hpp>
#include <wx/datetime.h>
#include <wx/file.h>
#include <wx/filedlg.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/stdpaths.h>
#include <wx/utils.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include "libslic3r/Utils.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/MainFrame.hpp"
#include "slic3r/GUI/MsgDialog.hpp"
#include "slic3r/GUI/Widgets/ProgressDialog.hpp"
#include "slic3r/GUI/Widgets/Label.hpp"
#include "slic3r/GUI/FlashForge/MultiComHelper.hpp"
#include "slic3r/GUI/FlashForge/WaitEvent.hpp"

namespace Slic3r { namespace GUI {

namespace {

std::string sanitize_application_log_line(const std::string& line)
{
    std::string content = line;
    while (!content.empty() && (content.back() == '\r' || content.back() == '\n')) {
        content.pop_back();
    }

    // This is the exact envelope emitted by set_log_path_and_level(). The message is never copied to the archive.
    static const std::regex boostEnvelope(
        R"re(^\[(trace|debug|info|warning|error|fatal)\]\t(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{1,9})\[Thread (?:0x)?[0-9A-F]+\]:.*$)re",
        std::regex_constants::icase);
    std::smatch match;
    if (!std::regex_match(content, match, boostEnvelope)) {
        return {};
    }

    return "[" + match[1].str() + "]\t" + match[2].str() + ":<redacted-message>\n";
}

std::string sanitize_flashnetwork_log_line(const std::string& line)
{
    std::string content = line;
    while (!content.empty() && (content.back() == '\r' || content.back() == '\n')) {
        content.pop_back();
    }

    static const std::regex logHeader(
        R"re(^\s*(\d{1,4}[-/.]\d{1,2}[-/.]\d{1,4}-\d{1,2}:\d{2}:\d{2},\s*(?:ERROR|WARN|INFO|DEBUG):)\s*$)re",
        std::regex_constants::icase);
    static const std::regex requestType(R"re(^\s*requestType\s*:\s*(GET|POST|PUT|DELETE|PATCH|HEAD|OPTIONS)\s*$)re",
                                        std::regex_constants::icase);
    static const std::regex sensitiveField(
        R"re(^\s*(?:url|headers?(?:\s+\d+)?|responseData|response|postFields|payload|playload|body|received|)re"
        R"re(MainboardID|RequestID|Topic|selected_machine|printer_name|)re"
        R"re(materialMappings|serialNumber|checkCode|gcodeFilePath|thumbFilePath|gcodeDstName|materialName|)re"
        R"re(toolMaterialColor|slotMaterialColor|callback|callbackData|userTopic)\b\s*[:=])re",
        std::regex_constants::icase);
    static const std::regex mqttEvent(R"re(^\s*mqtt\b)re", std::regex_constants::icase);
    static const std::regex uploadLogEvent(R"re(\b(?:upload_log|log upload)\b)re", std::regex_constants::icase);
    static const std::regex httpEvent(
        R"re(^\s*(?:HTTP|HttpIO::|curl_|postLanGcode|401 Unauthorized|Secure (?:HTTP|file upload))\b)re",
        std::regex_constants::icase);

    std::smatch match;
    if (std::regex_match(content, match, logHeader)) {
        return match[1].str() + '\n';
    }
    if (std::regex_match(content, match, requestType)) {
        return "requestType: " + match[1].str() + '\n';
    }
    if (std::regex_search(content, sensitiveField)) {
        return "network field: <redacted>\n";
    }
    if (std::regex_search(content, mqttEvent)) {
        return "mqtt event: <redacted>\n";
    }
    if (std::regex_search(content, uploadLogEvent)) {
        return "log upload event: <redacted>\n";
    }
    if (std::regex_search(content, httpEvent)) {
        return "HTTP transport event: <redacted>\n";
    }
    return {};
}

std::filesystem::path filesystem_path(const wxString& path);

struct RegularFileSnapshot
{
    uint64_t  device = 0;
    uint64_t  file   = 0;
    uintmax_t size   = 0;
};

class SafeLogDirectoryGuard
{
public:
    SafeLogDirectoryGuard() = default;
    ~SafeLogDirectoryGuard() { close(); }

    SafeLogDirectoryGuard(const SafeLogDirectoryGuard&)            = delete;
    SafeLogDirectoryGuard& operator=(const SafeLogDirectoryGuard&) = delete;

    bool open(const wxString& path)
    {
        close();
        m_path = path;
#ifdef _WIN32
        const std::filesystem::path fsPath = filesystem_path(path);
        m_handle = CreateFileW(fsPath.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                               FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (m_handle == INVALID_HANDLE_VALUE) {
            return false;
        }
        BY_HANDLE_FILE_INFORMATION info;
        if (!GetFileInformationByHandle(m_handle, &info) || (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
            (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            close();
            return false;
        }
#else
        const std::filesystem::path fsPath = filesystem_path(path);
        m_fd = ::open(fsPath.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (m_fd < 0) {
            return false;
        }
        struct stat info;
        if (::fstat(m_fd, &info) != 0 || !S_ISDIR(info.st_mode)) {
            close();
            return false;
        }
#endif
        return true;
    }

    bool snapshot(const wxString& name, RegularFileSnapshot& snapshot) const
    {
#ifdef _WIN32
        if (m_handle == INVALID_HANDLE_VALUE) {
            return false;
        }
        const std::filesystem::path filePath = filesystem_path(m_path + '/' + name);
        HANDLE fileHandle = CreateFileW(filePath.c_str(), FILE_READ_ATTRIBUTES,
                                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                                        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (fileHandle == INVALID_HANDLE_VALUE) {
            return false;
        }
        BY_HANDLE_FILE_INFORMATION info;
        const bool gotInfo = GetFileInformationByHandle(fileHandle, &info) != 0;
        CloseHandle(fileHandle);
        if (!gotInfo || (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0 ||
            info.nNumberOfLinks != 1) {
            return false;
        }
        snapshot.device = info.dwVolumeSerialNumber;
        snapshot.file   = (static_cast<uint64_t>(info.nFileIndexHigh) << 32) | info.nFileIndexLow;
        snapshot.size   = (static_cast<uintmax_t>(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
#else
        if (m_fd < 0) {
            return false;
        }
        const std::string utf8Name = name.utf8_string();
        struct stat       info;
        if (::fstatat(m_fd, utf8Name.c_str(), &info, AT_SYMLINK_NOFOLLOW) != 0 || !S_ISREG(info.st_mode) || info.st_nlink != 1 ||
            info.st_size < 0) {
            return false;
        }
        snapshot.device = static_cast<uint64_t>(info.st_dev);
        snapshot.file   = static_cast<uint64_t>(info.st_ino);
        snapshot.size   = static_cast<uintmax_t>(info.st_size);
#endif
        return true;
    }

    bool openFile(const wxString& name, const RegularFileSnapshot& snapshot, wxFile& file) const
    {
#ifdef _WIN32
        if (m_handle == INVALID_HANDLE_VALUE) {
            return false;
        }
        const std::filesystem::path filePath = filesystem_path(m_path + '/' + name);
        HANDLE fileHandle = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                        OPEN_EXISTING,
                                        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (fileHandle == INVALID_HANDLE_VALUE) {
            return false;
        }
        BY_HANDLE_FILE_INFORMATION info;
        if (!GetFileInformationByHandle(fileHandle, &info) ||
            (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0 || info.nNumberOfLinks != 1 ||
            snapshot.device != info.dwVolumeSerialNumber ||
            snapshot.file != ((static_cast<uint64_t>(info.nFileIndexHigh) << 32) | info.nFileIndexLow)) {
            CloseHandle(fileHandle);
            return false;
        }
        const int fd = _open_osfhandle(reinterpret_cast<intptr_t>(fileHandle), _O_RDONLY | _O_BINARY);
        if (fd < 0) {
            CloseHandle(fileHandle);
            return false;
        }
        file.Attach(fd);
#else
        if (m_fd < 0) {
            return false;
        }
        const std::string utf8Name = name.utf8_string();
        const int         fd       = ::openat(m_fd, utf8Name.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
        if (fd < 0) {
            return false;
        }
        struct stat info;
        if (::fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_nlink != 1 ||
            snapshot.device != static_cast<uint64_t>(info.st_dev) || snapshot.file != static_cast<uint64_t>(info.st_ino)) {
            ::close(fd);
            return false;
        }
        file.Attach(fd);
#endif
        return true;
    }

private:
    void close()
    {
#ifdef _WIN32
        if (m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
#else
        if (m_fd >= 0) {
            ::close(m_fd);
            m_fd = -1;
        }
#endif
        m_path.clear();
    }

    wxString m_path;
#ifdef _WIN32
    HANDLE m_handle = INVALID_HANDLE_VALUE;
#else
    int m_fd = -1;
#endif
};

bool is_cancelled(WaitEvent* cancelEvent) { return cancelEvent != nullptr && cancelEvent->get(); }

std::filesystem::path filesystem_path(const wxString& path)
{
#ifdef _WIN32
    return std::filesystem::path(path.ToStdWstring());
#else
    return std::filesystem::u8path(path.utf8_string());
#endif
}

bool get_regular_file_size(const wxString& path, uintmax_t& fileSize)
{
    std::error_code                    ec;
    const std::filesystem::path        fsPath = filesystem_path(path);
    const std::filesystem::file_status status = std::filesystem::symlink_status(fsPath, ec);
    if (ec || std::filesystem::is_symlink(status) || !std::filesystem::is_regular_file(status)) {
        return false;
    }
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesW(fsPath.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        return false;
    }
#endif
    fileSize = std::filesystem::file_size(fsPath, ec);
    return !ec;
}

bool is_safe_log_directory(const wxString& path)
{
    std::error_code                    ec;
    const std::filesystem::path        fsPath = filesystem_path(path);
    const std::filesystem::file_status status = std::filesystem::symlink_status(fsPath, ec);
    if (ec || std::filesystem::is_symlink(status) || !std::filesystem::is_directory(status)) {
        return false;
    }
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesW(fsPath.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        return false;
    }
#endif
    return true;
}

bool is_safe_archive_name(const wxString& name)
{
    if (name.empty() || name.length() > 255 || name.Find('/') != wxNOT_FOUND || name.Find('\\') != wxNOT_FOUND ||
        name.Find("..") != wxNOT_FOUND) {
        return false;
    }
    for (wxUniChar ch : name) {
        if (ch.GetValue() < 0x20 || ch.GetValue() == 0x7f) {
            return false;
        }
    }
    return true;
}

bool is_expected_log_name(const wxString& name, const wxString& prefix)
{
    static const std::regex applicationLog(R"re(^debug_[^/\\]+\.log(?:\.\d+)?$)re", std::regex_constants::icase);
    static const std::regex networkLog(R"re(^\d{8}-\d{6}\.log$)re");
    const std::string       utf8Name = name.utf8_string();
    return prefix.empty() ? std::regex_match(utf8Name, networkLog) : std::regex_match(utf8Name, applicationLog);
}

class TemporaryFileGuard
{
public:
    explicit TemporaryFileGuard(wxString path) : m_path(std::move(path)) {}

    ~TemporaryFileGuard()
    {
        if (!m_path.empty() && wxFileName::FileExists(m_path) && !wxRemoveFile(m_path)) {
            BOOST_LOG_TRIVIAL(warning) << "Failed to remove temporary log archive";
        }
    }

private:
    wxString m_path;
};

wxString create_temporary_log_path()
{
    const wxString prefix = wxFileName(wxStandardPaths::Get().GetTempDir(), "flashstudio-log-").GetFullPath();
    return wxFileName::CreateTempFileName(prefix);
}

} // namespace

wxDEFINE_EVENT(EVT_EXPORT_LOGS_FINISHED, ExportLogsFinishedEvent);
wxDEFINE_EVENT(EVT_UPLOAD_LOG_PROGRESS, wxCommandEvent);

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
        bool succeed = saveZip(outputPath, rootPath, fileInfos, false);
        wxQueueEvent(&exportDlg, new ExportLogsFinishedEvent(EVT_EXPORT_LOGS_FINISHED, succeed, outputPath));
    });
    exportDlg.ShowModal();
    thread.join();
}

void ExportLogs::uploadLocal()
{
    wxString defFileName = wxDateTime::Now().Format("%Y-%m-%d_%H-%M-%S.zip");
    flush_logs();
    wxString outputPath = create_temporary_log_path();
    if (outputPath.empty()) {
        ErrorDialog failed_dlg(wxGetApp().mainframe, _L("Upload failed"), _L("Upload log"));
        failed_dlg.ShowModal();
        return;
    }
    TemporaryFileGuard tempFile(outputPath);
    wxString           rootPath = wxString::FromUTF8(data_dir());
    wxString           email;
    if (!wxGetApp().is_flashforge_login()) {
        EmailInputDialog email_dlg(wxGetApp().mainframe, _L("Upload log"));
        if (email_dlg.ShowModal() == wxID_OK) {
            email = email_dlg.getText();
        } else {
            return;
        }
    }
    auto dlg = wxGetApp().mainframe->createLogProgress();
    dlg->Update(10, _L("Save tmp zip file"));
    dlg->Fit();
    auto fileInfos = getRootLatestFiles(rootPath, wxDateTime::Now());
    bool succeed   = saveZip(outputPath, rootPath, fileInfos, false);
    if (!succeed) {
        dlg->Hide();
        ErrorDialog failed_dlg(wxGetApp().mainframe, _L("Upload failed"), _L("Upload log"));
        failed_dlg.ShowModal();
        return;
    }
    dlg->Update(30, _L("Upload zip file"));
    dlg->Fit();

    auto logCallback = [](long long now, long long total, void* callbackData) {
        wxWindow* inst         = (wxWindow*) callbackData;
        const int now_progress = 30;
        if (total > 0) {
            const double ratio    = std::max(0.0, std::min(1.0, static_cast<double>(now) / static_cast<double>(total)));
            const int    progress = now_progress + static_cast<int>(ratio * (100 - now_progress));
            if (inst != nullptr) {
                wxCommandEvent* evt = new wxCommandEvent(EVT_UPLOAD_LOG_PROGRESS);
                evt->SetInt(progress);
                wxQueueEvent(inst, evt);
            }
        }
        return 0;
    };
    auto ret = MultiComHelper::inst()->uploadLogFileCloud(outputPath.utf8_string(), defFileName.utf8_string(), email.utf8_string(),
                                                          logCallback, wxGetApp().mainframe, ComTimeoutWanB);
    if (ret != COM_OK) {
        dlg->Hide();
        ErrorDialog failed_dlg(wxGetApp().mainframe, _L("Upload failed"), _L("Upload log"));
        failed_dlg.ShowModal();
    } else {
        dlg->Update(100, _L("Upload Complete"));
        MessageDialog success_dlg(wxGetApp().mainframe, _L("Upload Successful"), _L("Upload log"));
        success_dlg.ShowModal();
    }
}

bool ExportLogs::uploadFeedback(const std::string& clientId,
                                const std::string& accessToken,
                                const std::string& feedbackId,
                                WaitEvent*         cancelEvent)
{
    if (clientId.empty() || accessToken.empty() || feedbackId.empty()) {
        BOOST_LOG_TRIVIAL(error) << "Cannot upload feedback logs: required upload context is empty";
        return false;
    }

    try {
        if (is_cancelled(cancelEvent)) {
            return false;
        }
        flush_logs();
        if (is_cancelled(cancelEvent)) {
            return false;
        }
        const wxString outputPath = create_temporary_log_path();
        if (outputPath.empty()) {
            BOOST_LOG_TRIVIAL(error) << "Cannot create temporary feedback log archive";
            return false;
        }
        TemporaryFileGuard tempFile(outputPath);

        const wxString rootPath  = wxString::FromUTF8(data_dir());
        const auto     fileInfos = getRootLatestFiles(rootPath, wxDateTime::Now(), cancelEvent);
        if (is_cancelled(cancelEvent)) {
            return false;
        }
        if (!saveZip(outputPath, rootPath, fileInfos, false, cancelEvent)) {
            if (!is_cancelled(cancelEvent)) {
                BOOST_LOG_TRIVIAL(error) << "Failed to package feedback logs";
            }
            return false;
        }
        if (is_cancelled(cancelEvent)) {
            return false;
        }

        const wxString saveName       = wxDateTime::Now().Format("%Y-%m-%d_%H-%M-%S.zip");
        auto           cancelCallback = [](long long, long long, void* data) {
            auto* event = static_cast<WaitEvent*>(data);
            return event != nullptr && event->get() ? 1 : 0;
        };
        // The secure feedback upload API retries each network stage up to
        // three times. Do not retry the whole pipeline here: repeating a
        // successful object upload after a record timeout can orphan files.
        const ComErrno ret = MultiComHelper::inst()->uploadFeedbackLogFileCloud(clientId, accessToken, outputPath.utf8_string(),
                                                                                saveName.utf8_string(), feedbackId, cancelCallback,
                                                                                cancelEvent, ComTimeoutWanB);
        if (ret != COM_OK) {
            BOOST_LOG_TRIVIAL(error) << "Failed to upload feedback logs after transport retries, ret=" << ret;
            return false;
        }
        BOOST_LOG_TRIVIAL(info) << "Feedback logs uploaded";
        return true;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Feedback log upload threw an exception: " << e.what();
    } catch (...) {
        BOOST_LOG_TRIVIAL(error) << "Feedback log upload threw an unknown exception";
    }
    return false;
}

std::vector<std::pair<wxString, std::vector<wxString>>> ExportLogs::getRootLatestFiles(const wxString&   rootPath,
                                                                                       const wxDateTime& now,
                                                                                       WaitEvent*        cancelEvent,
                                                                                       size_t            maxFilesPerDirectory)
{
    std::vector<std::pair<wxString, std::vector<wxString>>> fileInfos;
    fileInfos.emplace_back("log", getDirLatestFiles(rootPath + "/log", "debug_", now, cancelEvent, maxFilesPerDirectory));
    if (!is_cancelled(cancelEvent)) {
        fileInfos.emplace_back(
            "FlashNetwork", getDirLatestFiles(rootPath + "/FlashNetwork", "", now, cancelEvent, maxFilesPerDirectory));
    }
    return fileInfos;
}

std::vector<wxString> ExportLogs::getDirLatestFiles(const wxString&   dirPath,
                                                     const wxString&   prefix,
                                                     const wxDateTime& now,
                                                     WaitEvent*        cancelEvent,
                                                     size_t            maxCandidates)
{
    struct FileCandidate
    {
        wxString   name;
        wxDateTime modified;
    };

    if (!is_safe_log_directory(dirPath)) {
        BOOST_LOG_TRIVIAL(warning) << "Skip an invalid log directory";
        return {};
    }
    wxDir                      dir(dirPath);
    wxString                   fileName;
    std::vector<FileCandidate> candidates;
    const wxDateTime           oldestAllowed = now - wxTimeSpan::Days(7);
    const wxDateTime           newestAllowed = now + wxTimeSpan::Minutes(5);
    if (dir.GetFirst(&fileName, wxEmptyString, wxDIR_FILES)) {
        do {
            if (is_cancelled(cancelEvent)) {
                break;
            }
            if ((prefix.empty() || fileName.StartsWith(prefix)) && is_safe_archive_name(fileName) &&
                is_expected_log_name(fileName, prefix)) {
                wxDateTime fileDateTime = wxFileName(dirPath + '/' + fileName).GetModificationTime();
                if (fileDateTime.IsValid() && fileDateTime >= oldestAllowed && fileDateTime <= newestAllowed) {
                    candidates.push_back({fileName, fileDateTime});
                    if (maxCandidates > 0 && candidates.size() > maxCandidates) {
                        const auto oldest = std::min_element(candidates.begin(), candidates.end(),
                            [](const FileCandidate& left, const FileCandidate& right) {
                                return left.modified.IsEarlierThan(right.modified);
                            });
                        candidates.erase(oldest);
                    }
                }
            }
        } while (dir.GetNext(&fileName));
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const FileCandidate& left, const FileCandidate& right) { return left.modified.IsLaterThan(right.modified); });
    std::vector<wxString> fileNames;
    fileNames.reserve(candidates.size());
    for (const FileCandidate& candidate : candidates) {
        fileNames.push_back(candidate.name);
    }
    return fileNames;
}

bool ExportLogs::saveZip(const wxString&                                                outputPath,
                         const wxString&                                                rootPath,
                         const std::vector<std::pair<wxString, std::vector<wxString>>>& fileInfos,
                         bool                                                           sanitize,
                         WaitEvent*                                                     cancelEvent)
{
    mz_zip_archive zipArchive;
    mz_zip_zero_struct(&zipArchive);
    if (!open_zip_writer(&zipArchive, outputPath.utf8_string())) {
        BOOST_LOG_TRIVIAL(error) << mz_zip_get_error_string(mz_zip_get_last_error(&zipArchive));
        return false;
    }
    std::unique_ptr<mz_zip_archive, decltype(&close_zip_writer)> freeCurl(&zipArchive, close_zip_writer);
    constexpr size_t                                             maxCloudFilesPerDirectory = 16;
    constexpr uintmax_t                                          maxCloudBytesPerDirectory = 32ULL * 1024 * 1024;
    constexpr uintmax_t                                          maxCloudBytesPerFile      = 16ULL * 1024 * 1024;
    const std::filesystem::path                                  outputFsPath              = filesystem_path(outputPath);
    SafeLogDirectoryGuard                                        rootDirectory;
    if (sanitize && !rootDirectory.open(rootPath)) {
        BOOST_LOG_TRIVIAL(error) << "Refuse to package logs from an unsafe data directory";
        return false;
    }
    for (const auto& fileInfo : fileInfos) {
        if (is_cancelled(cancelEvent)) {
            return false;
        }
        const wxString sourceDirPath = rootPath + '/' + fileInfo.first;
        SafeLogDirectoryGuard sourceDirectory;
        if (sanitize && !fileInfo.second.empty() && !sourceDirectory.open(sourceDirPath)) {
            BOOST_LOG_TRIVIAL(error) << "Refuse to package logs from an unsafe log directory";
            return false;
        }
        wxString    dirPath        = fileInfo.first + '/';
        std::string encodedDirPath = encode_path(dirPath.utf8_string().c_str());
        if (!mz_zip_writer_add_mem(&zipArchive, encodedDirPath.c_str(), nullptr, 0, MZ_DEFAULT_COMPRESSION)) {
            BOOST_LOG_TRIVIAL(error) << mz_zip_get_error_string(mz_zip_get_last_error(&zipArchive));
            return false;
        }
        size_t    includedFiles = 0;
        uintmax_t includedBytes = 0;
        for (const wxString& fileName : fileInfo.second) {
            if (is_cancelled(cancelEvent)) {
                return false;
            }
            const wxString srcPath = sourceDirPath + '/' + fileName;
            if (!is_safe_archive_name(fileName)) {
                BOOST_LOG_TRIVIAL(warning) << "Skip an invalid log input file";
                continue;
            }
            uintmax_t          fileSize = 0;
            RegularFileSnapshot snapshot;
            if (sanitize) {
                if (!sourceDirectory.snapshot(fileName, snapshot)) {
                    BOOST_LOG_TRIVIAL(warning) << "Skip an unsafe log input file";
                    continue;
                }
                fileSize = snapshot.size;
            } else if (!get_regular_file_size(srcPath, fileSize)) {
                BOOST_LOG_TRIVIAL(warning) << "Skip an invalid log input file";
                continue;
            }
            std::error_code equivalentError;
            if (std::filesystem::equivalent(outputFsPath, filesystem_path(srcPath), equivalentError) && !equivalentError) {
                BOOST_LOG_TRIVIAL(warning) << "Skip the output archive in log inputs";
                continue;
            }
            const uintmax_t inputBytes = sanitize ? std::min(fileSize, maxCloudBytesPerFile) : fileSize;
            if (sanitize && (includedFiles >= maxCloudFilesPerDirectory || inputBytes > maxCloudBytesPerDirectory - includedBytes)) {
                continue;
            }
            wxFile sanitizedInput;
            if (sanitize && !sourceDirectory.openFile(fileName, snapshot, sanitizedInput)) {
                BOOST_LOG_TRIVIAL(warning) << "Skip a log input file replaced while packaging";
                continue;
            }
            wxString archiveName = fileName;
            if (sanitize) {
                const wxString neutralPrefix = fileInfo.first == "FlashNetwork" ? "network" : "application";
                archiveName = neutralPrefix + wxString::Format("-%02u.log", static_cast<unsigned>(includedFiles + 1));
            }
            const bool  strictNetworkLog = sanitize && fileInfo.first == "FlashNetwork";
            std::string dstPath          = encode_path((dirPath + archiveName).utf8_string().c_str());
            if (!addZipFile(&zipArchive, dstPath, srcPath, sanitize, inputBytes,
                            sanitize ? &sanitizedInput : nullptr, strictNetworkLog, cancelEvent)) {
                return false;
            }
            ++includedFiles;
            includedBytes += inputBytes;
        }
    }
    if (is_cancelled(cancelEvent)) {
        return false;
    }
    if (!mz_zip_writer_finalize_archive(&zipArchive)) {
        BOOST_LOG_TRIVIAL(error) << mz_zip_get_error_string(mz_zip_get_last_error(&zipArchive));
        return false;
    }
    return true;
}

bool ExportLogs::addZipFile(mz_zip_archive*    zipArchive,
                            const std::string& dstPath,
                            const wxString&    srcPath,
                            bool               sanitize,
                            uintmax_t          sanitizedByteLimit,
                            wxFile*            sanitizedInput,
                            bool               strictNetworkLog,
                            WaitEvent*         cancelEvent)
{
    if (sanitize) {
        const wxString sanitizedPath = create_temporary_log_path();
        if (sanitizedPath.empty()) {
            BOOST_LOG_TRIVIAL(error) << "Cannot create temporary sanitized log file";
            return false;
        }
        TemporaryFileGuard tempFile(sanitizedPath);
        wxFile             output;
        if (sanitizedInput == nullptr || !sanitizedInput->IsOpened() || !output.Open(sanitizedPath, wxFile::write)) {
            BOOST_LOG_TRIVIAL(error) << "Cannot open a log file for sanitizing";
            return false;
        }
        wxFile& input = *sanitizedInput;

        constexpr size_t   chunkSize            = 64 * 1024;
        constexpr size_t   maxSanitizedLineSize = 256 * 1024;
        const wxFileOffset fileSize             = input.Length();
        const wxFileOffset maxInputBytes        = static_cast<wxFileOffset>(sanitizedByteLimit);
        if (fileSize == wxInvalidOffset) {
            BOOST_LOG_TRIVIAL(error) << "Cannot determine a log file size";
            return false;
        }
        bool discardPartialLine = false;
        if (fileSize > maxInputBytes) {
            if (input.Seek(fileSize - maxInputBytes, wxFromStart) == wxInvalidOffset) {
                BOOST_LOG_TRIVIAL(error) << "Cannot seek in a log file for sanitizing";
                return false;
            }
            discardPartialLine = true;
        }
        wxFileOffset      remainingBytes          = std::min(fileSize, maxInputBytes);
        std::vector<char> chunk(chunkSize);
        std::string       pending;
        bool              discardOversizedLine = false;
        auto              writeSanitized       = [&output, strictNetworkLog](const std::string& line) {
            const std::string sanitized = strictNetworkLog ? sanitize_flashnetwork_log_line(line) : sanitize_application_log_line(line);
            return sanitized.empty() || output.Write(sanitized.data(), sanitized.size()) == sanitized.size();
        };
        for (;;) {
            if (is_cancelled(cancelEvent)) {
                return false;
            }
            if (remainingBytes == 0) {
                break;
            }
            const size_t readSize = static_cast<size_t>(std::min<wxFileOffset>(remainingBytes, chunk.size()));
            const wxFileOffset bytesRead = input.Read(chunk.data(), readSize);
            if (bytesRead == wxInvalidOffset) {
                BOOST_LOG_TRIVIAL(error) << "Cannot read a log file for sanitizing";
                return false;
            }
            if (bytesRead == 0) {
                break;
            }
            remainingBytes -= bytesRead;
            pending.append(chunk.data(), static_cast<size_t>(bytesRead));
            for (;;) {
                const size_t lineEnd = pending.find('\n');
                if (discardPartialLine || discardOversizedLine) {
                    if (lineEnd == std::string::npos) {
                        pending.clear();
                        break;
                    }
                    pending.erase(0, lineEnd + 1);
                    discardPartialLine   = false;
                    discardOversizedLine = false;
                    continue;
                }
                if (lineEnd == std::string::npos) {
                    if (pending.size() > maxSanitizedLineSize) {
                        static const std::string redactedLine = "<redacted-oversized-line>\n";
                        if (output.Write(redactedLine.data(), redactedLine.size()) != redactedLine.size()) {
                            BOOST_LOG_TRIVIAL(error) << "Cannot write a sanitized log file";
                            return false;
                        }
                        pending.clear();
                        discardOversizedLine = true;
                    }
                    break;
                }
                if (lineEnd + 1 > maxSanitizedLineSize) {
                    static const std::string redactedLine = "<redacted-oversized-line>\n";
                    if (output.Write(redactedLine.data(), redactedLine.size()) != redactedLine.size()) {
                        BOOST_LOG_TRIVIAL(error) << "Cannot write a sanitized log file";
                        return false;
                    }
                } else if (!writeSanitized(pending.substr(0, lineEnd + 1))) {
                    BOOST_LOG_TRIVIAL(error) << "Cannot write a sanitized log file";
                    return false;
                }
                pending.erase(0, lineEnd + 1);
            }
        }
        if (!pending.empty() && !discardPartialLine && !discardOversizedLine) {
            if (!writeSanitized(pending)) {
                BOOST_LOG_TRIVIAL(error) << "Cannot write a sanitized log file";
                return false;
            }
        }
        if (is_cancelled(cancelEvent)) {
            return false;
        }
        output.Close();
        const std::string encodedSanitizedPath = encode_path(sanitizedPath.utf8_string().c_str());
        if (!mz_zip_writer_add_file(zipArchive, dstPath.c_str(), encodedSanitizedPath.c_str(), nullptr, 0, MZ_DEFAULT_COMPRESSION)) {
            BOOST_LOG_TRIVIAL(error) << mz_zip_get_error_string(mz_zip_get_last_error(zipArchive));
            return false;
        }
        return true;
    }

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
    std::vector<char> buf(static_cast<size_t>(fileSize));
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

bool ExportLogs::isVirtualMachine()
{
    wxArrayString output;
#ifdef __WXMSW__
    wxString cmd = "powershell -Command \"Get-WmiObject Win32_ComputerSystem | Select-Object -ExpandProperty Manufacturer\"";
    if (wxExecute(cmd, output, wxEXEC_SYNC) == 0) {
        wxString manu = output[0].Lower().Trim();
        if (manu.Contains("vmware") || manu.Contains("virtualbox") || manu.Contains("microsoft corporation") || manu.Contains("qemu"))
            return true;
    }
#elif __WXMAC__
    wxString cmd = "system_profiler SPHardwareDataType | grep 'Model Identifier'";
    if (wxExecute(cmd, output, wxEXEC_SYNC) == 0) {
        wxString model = output[0].Lower().Trim();
        if (model.Contains("vmware") || model.Contains("parallels") || model.Contains("virtual"))
            return true;
    }
#endif
    return false;
}

wxString ExportLogs::getDeviceHash() 
{ 
    std::vector<wxString> identifiers;
    wxString              cmd;
    wxArrayString         output;
#ifdef __WXMSW__
    cmd = "powershell -Command \"Get-WmiObject Win32_BaseBoard | Select-Object -ExpandProperty SerialNumber\"";
    // 1. MB ID
    if (wxExecute(cmd, output, wxEXEC_SYNC) == 0) {
        wxString sn = output[0].Trim().Trim(false);
        if (!sn.IsEmpty() && sn != "To be filled by O.E.M." && sn != "None")
            identifiers.push_back(sn);
    }
    output.clear();

    // 2. BIOS UUID
    cmd = "powershell -Command \"Get-WmiObject Win32_ComputerSystemProduct | Select-Object -ExpandProperty UUID\"";
    if (wxExecute(cmd, output, wxEXEC_SYNC) == 0) {
        wxString uuid = output[0].Trim().Trim(false);
        if (!uuid.IsEmpty() && uuid != "00000000-0000-0000-0000-000000000000")
            identifiers.push_back(uuid);
    }
    output.clear();

    // 3. DISK ID
    cmd = "powershell -Command \"Get-WmiObject Win32_DiskDrive | Select-Object -ExpandProperty SerialNumber | Select-Object -First 1\"";
    if (wxExecute(cmd, output, wxEXEC_SYNC) == 0) {
        wxString diskSn = output[0].Trim().Trim(false);
        if (!diskSn.IsEmpty() && diskSn != "Unknown")
            identifiers.push_back(diskSn);
    }
    output.clear();

#elif __WXMAC__
    // 1. IOPlatformUUID
    cmd = "ioreg -d2 -c IOPlatformExpertDevice | awk -F\\\" '/IOPlatformUUID/{print $4}'";
    if (wxExecute(cmd, output, wxEXEC_SYNC) == 0) {
        wxString uuid = output[0].Trim().Trim(false);
        if (!uuid.IsEmpty())
            identifiers.push_back(uuid);
    }
    output.clear();

    // 2. MB ID
    cmd = "system_profiler SPHardwareDataType | grep 'Serial Number (system)' | awk '{print $4}'";
    if (wxExecute(cmd, output, wxEXEC_SYNC) == 0) {
        wxString sn = output[0].Trim().Trim(false);
        if (!sn.IsEmpty())
            identifiers.push_back(sn);
    }
    output.clear();

    // 3. MAC ADDR
    cmd = "ifconfig en0 | grep ether | awk '{print $2}'";
    if (wxExecute(cmd, output, wxEXEC_SYNC) == 0) {
        wxString mac = output[0].Trim().Trim(false);
        if (!mac.IsEmpty())
            identifiers.push_back(mac);
    }
    output.clear();

#endif

    if (isVirtualMachine()) {
#ifdef __WXMSW__
        // 1. Hyper-V/VMware ID
        cmd = "powershell -Command \"Get-WmiObject Win32_ComputerSystem | Select-Object -ExpandProperty Name\"";
        if (wxExecute(cmd, output, wxEXEC_SYNC) == 0) {
            wxString vmName = output[0].Trim().Trim(false);
            if (!vmName.IsEmpty())
                identifiers.push_back("VM_" + vmName);
        }
        output.clear();

        // 2. Virtual Disk Path
        cmd = "powershell -Command \"Get-WmiObject Win32_LogicalDisk | Select-Object -ExpandProperty DeviceID | Select-Object -First 1\"";
        if (wxExecute(cmd, output, wxEXEC_SYNC) == 0) {
            wxString diskPath = output[0].Trim().Trim(false);
            if (!diskPath.IsEmpty())
                identifiers.push_back(diskPath);
        }
        output.clear();

#elif __WXMAC__
        // Parallels/VMware Fusion UUID
        cmd = "defaults read /Library/Preferences/Parallels/parallels.desktop.plist 'Virtual Machines' 2>/dev/null | grep uuid | "
                       "head -n1 | awk '{print $3}' | tr -d '\"'";
        if (wxExecute(cmd, output, wxEXEC_SYNC) == 0) {
            wxString vmUUID = output[0].Trim().Trim(false);
            if (!vmUUID.IsEmpty())
                identifiers.push_back("VMUUID_" + vmUUID);
        }
        output.clear();

#endif
    }
    identifiers.push_back("orca-flashforge");

    auto simpleHash64 = [](const wxString& str) {
        uint64_t      hash = 0xCBF29CE484222325LL;
        const wxChar* data = str.wc_str();
        while (*data != '\0') {
            hash ^= (wxUint64) *data;
            hash *= 0x100000001B3LL;
            data++;
        }
        return hash;
    };
    wxString combined;
    for (const auto& id : identifiers) {
        if (!id.IsEmpty())
            combined += id + "|";
    }
    wxString finalID = wxString::Format("%016llX", (unsigned long long)simpleHash64(combined)).Upper();
    return finalID;
}

EmailInputDialog::EmailInputDialog(wxWindow* parent, const wxString& title) : 
    wxDialog(parent, wxID_ANY, title) 
{
    SetBackgroundColour(*wxWHITE);
    auto sizer = new wxBoxSizer(wxVERTICAL);
    auto text_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto label      = new Label(this, _L("Email Address"));
    m_text          = new wxTextCtrl(this, wxID_ANY, "");
    m_text->SetMinSize(wxSize(FromDIP(200), FromDIP(25)));
    text_sizer->Add(label, 0, wxLEFT | wxALIGN_CENTER_HORIZONTAL, FromDIP(10));
    text_sizer->AddSpacer(FromDIP(10));
    text_sizer->Add(m_text, 0, wxRIGHT | wxALIGN_CENTER_HORIZONTAL, FromDIP(10));
    auto btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto btn = new Button(this, _L("Ok"));
    btn->SetMinSize(wxSize(FromDIP(60), FromDIP(22)));
    StateColor bg_color(std::pair<wxColour, int>(wxColour(0, 137, 123), StateColor::Pressed),
                               std::pair<wxColour, int>(wxColour(38, 166, 154), StateColor::Hovered),
                               std::pair<wxColour, int>(wxColour(0, 150, 136), StateColor::Normal));
    btn->SetBackgroundColor(bg_color);
    StateColor text_color = StateColor(std::pair{wxColour(255, 255, 255), (int) StateColor::Normal});
    btn->SetTextColor(text_color);
    btn->SetBorderWidth(0);
    btn->SetCornerRadius(FromDIP(8));
    auto w = new wxPanel(this);
    w->SetBackgroundColour(*wxWHITE);
    w->SetMinSize(wxSize(FromDIP(50), -1));
    btn_sizer->AddStretchSpacer(1);
    btn_sizer->Add(w, 0, wxEXPAND | wxALIGN_RIGHT, 0);
    btn_sizer->Add(btn, 0, wxALIGN_RIGHT | wxRIGHT, FromDIP(5));
    sizer->AddSpacer(FromDIP(10));
    sizer->Add(text_sizer, 0, wxALL, 0);
    sizer->AddSpacer(FromDIP(15));
    sizer->Add(btn_sizer, 0, wxALL | wxALIGN_RIGHT, 0);
    sizer->AddSpacer(FromDIP(10));
    SetSizer(sizer);
    sizer->Fit(this);
    Center();
    Layout();

    btn->Bind(wxEVT_BUTTON, [=](wxCommandEvent& evt) {
        if (m_text->GetValue().empty()) {
            GUI::show_error(this, _L("Email cannot be empty."));
            return;
        }
        if (this->IsModal()) {
            EndModal(wxID_OK);
        } else {
            Close();
        }
    });
}

wxString EmailInputDialog::getText() { return this->m_text->GetValue(); }

}} // namespace Slic3r::GUI
