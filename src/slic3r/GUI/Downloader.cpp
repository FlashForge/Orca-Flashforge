#include "Downloader.hpp"
#include "GUI_App.hpp"
#include "NotificationManager.hpp"
#include "format.hpp"
#include "MainFrame.hpp"
#include "wxExtensions.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/log/trivial.hpp>
#include <boost/regex.hpp>
#include <algorithm>

#ifdef __linux__
#include <wx/bmpbuttn.h>
#include <wx/button.h>
#include <wx/filename.h>
#include <wx/gauge.h>
#include <wx/popupwin.h>
#include <wx/statline.h>
#include <wx/timer.h>
#endif

namespace Slic3r {
namespace GUI {

namespace {
void open_folder(const std::string& path)
{
	// Code taken from NotificationManager.cpp

	// Execute command to open a file explorer, platform dependent.
#ifdef _WIN32
	const wxString widepath = from_u8(path);
	const wchar_t* argv[] = { L"explorer", widepath.GetData(), nullptr };
	::wxExecute(argv, wxEXEC_ASYNC, nullptr);
#elif __APPLE__
	const char* argv[] = { "open", path.data(), nullptr };
	::wxExecute(argv, wxEXEC_ASYNC, nullptr);
#else
	const char* argv[] = { "xdg-open", path.data(), nullptr };

	// Check if we're running in an AppImage container, if so, we need to remove AppImage's env vars,
	// because they may mess up the environment expected by the file manager.
	// Mostly this is about LD_LIBRARY_PATH, but we remove a few more too for good measure.
	if (wxGetEnv("APPIMAGE", nullptr)) {
		// We're running from AppImage
		wxEnvVariableHashMap env_vars;
		wxGetEnvMap(&env_vars);

		env_vars.erase("APPIMAGE");
		env_vars.erase("APPDIR");
		env_vars.erase("LD_LIBRARY_PATH");
		env_vars.erase("LD_PRELOAD");
		env_vars.erase("UNION_PRELOAD");

		wxExecuteEnv exec_env;
		exec_env.env = std::move(env_vars);

		wxString owd;
		if (wxGetEnv("OWD", &owd)) {
			// This is the original work directory from which the AppImage image was run,
			// set it as CWD for the child process:
			exec_env.cwd = std::move(owd);
		}

		::wxExecute(argv, wxEXEC_ASYNC, nullptr, &exec_env);
	}
	else {
		// Looks like we're NOT running from AppImage, we'll make no changes to the environment.
		::wxExecute(argv, wxEXEC_ASYNC, nullptr, nullptr);
	}
#endif
}

std::string filename_from_url(const std::string& url)
{
	// TODO: can it be done with curl?
	size_t slash = url.find_last_of("/");
	if (slash == std::string::npos && slash != url.size() - 1)
		return {};
    size_t question_mark = url.find_last_of("?");
    if (question_mark != std::string::npos && question_mark > slash)
        return url.substr(slash + 1, question_mark - slash - 1);
	return url.substr(slash + 1, url.size() - slash + 1);
}

#ifdef __linux__
class LinuxWebDownloadPopup : public wxPopupWindow
{
public:
    explicit LinuxWebDownloadPopup(wxWindow *parent)
        : wxPopupWindow(parent, wxBORDER_NONE)
        , m_hide_timer(this)
    {
        SetBackgroundColour(*wxWHITE);

        auto *outer_sizer = new wxBoxSizer(wxVERTICAL);
        auto *header_sizer = new wxBoxSizer(wxHORIZONTAL);

        auto *title = new wxStaticText(this, wxID_ANY, _L("Downloads"));
        wxFont title_font = title->GetFont();
        title_font.SetWeight(wxFONTWEIGHT_BOLD);
        title->SetFont(title_font);

        auto *close_btn = new wxBitmapButton(this, wxID_ANY, create_scaled_bitmap("title_close", this, 12), wxDefaultPosition,
                                             wxDefaultSize, wxBORDER_NONE);
        close_btn->SetBitmapHover(create_scaled_bitmap("title_closeHover", this, 12));
        close_btn->SetBitmapPressed(create_scaled_bitmap("title_closePress", this, 12));
        close_btn->SetBackgroundColour(*wxWHITE);
        close_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Hide(); });

        header_sizer->Add(title, 1, wxALIGN_CENTER_VERTICAL);
        header_sizer->Add(close_btn, 0, wxLEFT, FromDIP(8));

        m_items_sizer = new wxBoxSizer(wxVERTICAL);
        outer_sizer->Add(header_sizer, 0, wxEXPAND | wxALL, FromDIP(12));
        outer_sizer->Add(new wxStaticLine(this), 0, wxEXPAND);
        outer_sizer->Add(m_items_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));
        SetSizer(outer_sizer);

        Bind(wxEVT_TIMER, &LinuxWebDownloadPopup::OnHideTimer, this);
    }

    void Start(size_t id, const wxString &filename, const wxString &folder)
    {
        m_hide_timer.Stop();

        auto iter = m_items.find(id);
        if (iter == m_items.end()) {
            iter = m_items.emplace(id, CreateItem(id)).first;
            TrimItems();
        }

        DownloadItem &item = iter->second;
        item.filename = filename;
        item.folder = folder;
        item.title->SetLabel(Ellipsize(filename));
        item.status->SetLabel(_L("Downloading..."));
        item.gauge->SetValue(0);
        item.gauge->Show();
        item.open_folder_btn->Enable(false);
        item.panel->Show();
        item.active = true;

        LayoutAndShow();
    }

    void SetProgress(size_t id, float percentage)
    {
        auto iter = m_items.find(id);
        if (iter == m_items.end()) {
            return;
        }
        int value = std::max(0, std::min(100, static_cast<int>(percentage * 100.f)));
        iter->second.gauge->SetValue(value);
        iter->second.status->SetLabel(wxString::Format(_L("Downloading... %d%%"), value));
        iter->second.active = true;
        m_hide_timer.Stop();
        LayoutAndShow();
    }

    void Complete(size_t id, const wxString &path)
    {
        auto iter = m_items.find(id);
        if (iter == m_items.end()) {
            wxFileName file_name(path);
            iter = m_items.emplace(id, CreateItem(id)).first;
            TrimItems();
            iter->second.filename = file_name.GetFullName();
        }

        DownloadItem &item = iter->second;
        wxFileName file_name(path);
        item.folder = file_name.GetPath();
        item.title->SetLabel(Ellipsize(item.filename.IsEmpty() ? file_name.GetFullName() : item.filename));
        item.status->SetLabel(_L("Download complete"));
        item.gauge->SetValue(100);
        item.gauge->Hide();
        item.open_folder_btn->Enable(true);
        item.active = false;

        LayoutAndShow();
        ArmAutoHideIfIdle();
    }

    void Error(size_t id, const wxString &message)
    {
        auto iter = m_items.find(id);
        if (iter == m_items.end()) {
            return;
        }
        iter->second.status->SetLabel(message.IsEmpty() ? _L("Download failed") : message);
        iter->second.gauge->Hide();
        iter->second.open_folder_btn->Enable(false);
        iter->second.active = false;
        LayoutAndShow();
        ArmAutoHideIfIdle();
    }

    void Canceled(size_t id)
    {
        auto iter = m_items.find(id);
        if (iter == m_items.end()) {
            return;
        }
        iter->second.status->SetLabel(_L("Download canceled"));
        iter->second.gauge->Hide();
        iter->second.open_folder_btn->Enable(false);
        iter->second.active = false;
        LayoutAndShow();
        ArmAutoHideIfIdle();
    }

private:
    struct DownloadItem
    {
        wxPanel      *panel{nullptr};
        wxStaticText *title{nullptr};
        wxStaticText *status{nullptr};
        wxGauge      *gauge{nullptr};
        wxButton     *open_folder_btn{nullptr};
        wxString      filename;
        wxString      folder;
        bool          active{false};
    };

    DownloadItem CreateItem(size_t id)
    {
        DownloadItem item;
        item.panel = new wxPanel(this);
        item.panel->SetBackgroundColour(*wxWHITE);

        auto *sizer = new wxBoxSizer(wxVERTICAL);
        item.title = new wxStaticText(item.panel, wxID_ANY, wxEmptyString);
        item.status = new wxStaticText(item.panel, wxID_ANY, wxEmptyString);
        item.gauge = new wxGauge(item.panel, wxID_ANY, 100, wxDefaultPosition, wxSize(FromDIP(320), FromDIP(6)));
        item.open_folder_btn = new wxButton(item.panel, wxID_ANY, _L("Open folder"));
        item.open_folder_btn->Enable(false);
        item.open_folder_btn->Bind(wxEVT_BUTTON, [this, id](wxCommandEvent &) {
            auto iter = m_items.find(id);
            if (iter != m_items.end() && !iter->second.folder.IsEmpty()) {
                open_folder(iter->second.folder.utf8_string());
            }
        });

        sizer->Add(item.title, 0, wxEXPAND | wxTOP, FromDIP(10));
        sizer->Add(item.status, 0, wxEXPAND | wxTOP, FromDIP(4));
        sizer->Add(item.gauge, 0, wxEXPAND | wxTOP, FromDIP(8));
        sizer->Add(item.open_folder_btn, 0, wxALIGN_RIGHT | wxTOP, FromDIP(8));
        item.panel->SetSizer(sizer);

        if (!m_items.empty()) {
            m_items_sizer->Add(new wxStaticLine(this), 0, wxEXPAND | wxTOP, FromDIP(10));
        }
        m_items_sizer->Add(item.panel, 0, wxEXPAND);
        return item;
    }

    wxString Ellipsize(const wxString &text) const
    {
        constexpr size_t max_chars = 42;
        if (text.length() <= max_chars) {
            return text;
        }
        return text.Left(max_chars - 3) + "...";
    }

    void LayoutAndShow()
    {
        wxWindow *parent = GetParent();
        if (parent == nullptr) {
            return;
        }

        SetSize(wxSize(parent->FromDIP(360), wxDefaultCoord));
        Layout();
        Fit();

        wxSize size = GetSize();
        wxPoint pos = parent->ClientToScreen(wxPoint(
            parent->GetClientSize().x - size.x - parent->FromDIP(18),
            parent->FromDIP(58)));
        Move(pos);

        if (!IsShown()) {
            Show();
        }
        Raise();
    }

    void ArmAutoHideIfIdle()
    {
        for (const auto &item : m_items) {
            if (item.second.active) {
                return;
            }
        }
        m_hide_timer.StartOnce(6000);
    }

    void OnHideTimer(wxTimerEvent &)
    {
        Hide();
    }

    void TrimItems()
    {
        constexpr size_t max_items = 5;
        while (m_items.size() > max_items) {
            auto iter = m_items.begin();
            if (iter->second.panel != nullptr) {
                iter->second.panel->Hide();
            }
            m_items.erase(iter);
        }
    }

    wxBoxSizer *m_items_sizer{nullptr};
    std::map<size_t, DownloadItem> m_items;
    wxTimer m_hide_timer;
};

LinuxWebDownloadPopup *linux_web_download_popup()
{
    static LinuxWebDownloadPopup *popup = nullptr;
    wxWindow *parent = wxGetApp().mainframe;
    if (parent == nullptr) {
        return nullptr;
    }
    if (popup == nullptr || popup->GetParent() != parent) {
        popup = new LinuxWebDownloadPopup(parent);
    }
    return popup;
}
#endif
}

#ifdef __linux__
void show_linux_web_download_start(size_t id, const wxString &filename, const wxString &folder)
{
    if (auto *popup = linux_web_download_popup()) {
        popup->Start(id, filename, folder);
    }
}

void show_linux_web_download_progress(size_t id, float percentage)
{
    if (auto *popup = linux_web_download_popup()) {
        popup->SetProgress(id, percentage);
    }
}

void show_linux_web_download_complete(const wxString &path)
{
    static size_t synthetic_download_id = 1000000000;
    if (auto *popup = linux_web_download_popup()) {
        popup->Complete(++synthetic_download_id, path);
    }
}

void show_linux_web_download_complete(size_t id, const wxString &path)
{
    if (auto *popup = linux_web_download_popup()) {
        popup->Complete(id, path);
    }
}

void show_linux_web_download_error(size_t id, const wxString &message)
{
    if (auto *popup = linux_web_download_popup()) {
        popup->Error(id, message);
    }
}
#endif

Download::Download(int ID, std::string url, wxEvtHandler* evt_handler, const boost::filesystem::path& dest_folder)
    : m_id(ID)
	, m_filename(filename_from_url(FileGet::escape_url(url)))
	, m_dest_folder(dest_folder)
{
	assert(boost::filesystem::is_directory(dest_folder));
	m_final_path = dest_folder / m_filename;
    m_file_get = std::make_shared<FileGet>(ID, std::move(url), m_filename, evt_handler, dest_folder);
}

Download::Download(int ID, const std::string &url, const std::string &fileName, wxEvtHandler* evt_handler,
	const boost::filesystem::path& dest_folder)
    : m_id(ID)
    , m_filename(fileName.empty() ? filename_from_url(FileGet::escape_url(url)) : fileName)
    , m_dest_folder(dest_folder)
{
    assert(boost::filesystem::is_directory(dest_folder));
    m_final_path = dest_folder / m_filename;
    m_file_get = std::make_shared<FileGet>(ID, url, m_filename, evt_handler, dest_folder);
}

void Download::start()
{
	m_state = DownloadState::DownloadOngoing;
	m_file_get->get();
}
void Download::cancel()
{
	m_state = DownloadState::DownloadStopped;
	m_file_get->cancel();
}
void Download::pause()
{
	//assert(m_state == DownloadState::DownloadOngoing);
	// if instead of assert - it can happen that user clicks on pause several times before the pause happens
	if (m_state != DownloadState::DownloadOngoing)
		return;
	m_state = DownloadState::DownloadPaused;
	m_file_get->pause();
}
void Download::resume()
{
	//assert(m_state == DownloadState::DownloadPaused);
	if (m_state != DownloadState::DownloadPaused)
		return;
	m_state = DownloadState::DownloadOngoing;
	m_file_get->resume();
}


Downloader::Downloader()
	: wxEvtHandler()
{
	//Bind(EVT_DWNLDR_FILE_COMPLETE, [](const wxCommandEvent& evt) {});
	//Bind(EVT_DWNLDR_FILE_PROGRESS, [](const wxCommandEvent& evt) {});
	//Bind(EVT_DWNLDR_FILE_ERROR, [](const wxCommandEvent& evt) {});
	//Bind(EVT_DWNLDR_FILE_NAME_CHANGE, [](const wxCommandEvent& evt) {});

	Bind(EVT_DWNLDR_FILE_COMPLETE, &Downloader::on_complete, this);
	Bind(EVT_DWNLDR_FILE_PROGRESS, &Downloader::on_progress, this);
	Bind(EVT_DWNLDR_FILE_ERROR, &Downloader::on_error, this);
	Bind(EVT_DWNLDR_FILE_NAME_CHANGE, &Downloader::on_name_change, this);
	Bind(EVT_DWNLDR_FILE_PAUSED, &Downloader::on_paused, this);
	Bind(EVT_DWNLDR_FILE_CANCELED, &Downloader::on_canceled, this);
}

void Downloader::start_download(const std::string& full_url, const std::string& fileName /* = "" */, bool need_escape /* = false */)
{
	assert(m_initialized);

    // Orca: Move to the 3D view
    MainFrame* mainframe = wxGetApp().mainframe;
    Plater* plater = wxGetApp().plater();

    mainframe->Freeze();
    mainframe->select_tab((size_t)MainFrame::TabPosition::tp3DEditor);
    plater->select_view_3D("3D");
    plater->select_view("plate");
    plater->get_current_canvas3D()->zoom_to_bed();
    mainframe->Thaw();

    // Orca: Replace PS workaround for "mysterious slash" with a more dynamic approach
    // Windows seems to have fixed the issue and this provides backwards compatability for those it still affects
	boost::regex re(R"(^(orcaflashforge|orcaslicer|prusaslicer|bambustudio|cura):\/\/open[\/]?\?file=)", boost::regbase::icase);
	boost::regex re2(R"(^(orcaflashforgeopen|bambustudioopen):\/\/)", boost::regex::icase);
    boost::smatch results;

	if (!boost::regex_search(full_url, results, re) && !boost::regex_search(full_url, results, re2)) {
		BOOST_LOG_TRIVIAL(error) << "Could not start download due to wrong URL: " << full_url;
        // Orca: show error
        NotificationManager* ntf_mngr = wxGetApp().notification_manager();
        ntf_mngr->push_notification(NotificationType::CustomNotification, NotificationManager::NotificationLevel::ErrorNotificationLevel,
                                    "Could not start download due to malformed URL");
		return;
	}
    size_t id = get_next_id();
	std::string result_url = full_url.substr(results.length());
    std::string real_url    = result_url; //final url
    if (need_escape) {
        std::string escaped_url = FileGet::escape_url(full_url.substr(results.length()));
        real_url                = escaped_url;
    }
	if (is_bambustudio_open(full_url) || (is_orca_open(full_url) && is_makerworld_link(full_url))) {
        plater->request_model_download(wxString::FromUTF8(real_url));
	} else {
        m_downloads.emplace_back(std::make_unique<Download>(id, real_url, fileName, this, m_dest_folder));
		push_dest_folder_cache(id, m_dest_folder.string());
        NotificationManager* ntf_mngr = wxGetApp().notification_manager();
        ntf_mngr->push_download_URL_progress_notification(id, m_downloads.back()->get_filename(),
                                                          std::bind(&Downloader::user_action_callback, this, std::placeholders::_1,
                                                                    std::placeholders::_2));
        m_downloads.back()->start();
    }
    BOOST_LOG_TRIVIAL(debug) << "started download";
}

void Downloader::on_progress(wxCommandEvent& event)
{
	size_t id = event.GetInt();
	float percent = (float)std::stoi(into_u8(event.GetString())) / 100.f;
	//BOOST_LOG_TRIVIAL(error) << "progress " << id << ": " << percent;
	NotificationManager* ntf_mngr = wxGetApp().notification_manager();
	BOOST_LOG_TRIVIAL(trace) << "Download "<< id << ": " << percent;
	ntf_mngr->set_download_URL_progress(id, percent);
}
void Downloader::on_error(wxCommandEvent& event)
{
	size_t id = event.GetInt();
    set_download_state(event.GetInt(), DownloadState::DownloadError);
    BOOST_LOG_TRIVIAL(error) << "Download error: " << event.GetString();
	NotificationManager* ntf_mngr = wxGetApp().notification_manager();
    ntf_mngr->set_download_URL_error(id, into_u8(event.GetString()));
	show_error(nullptr, format_wxstr(L"%1%\n%2%", _L("The download has failed") + ":", event.GetString()));
	remove_download(id);
}
void Downloader::on_complete(wxCommandEvent& event)
{
	// TODO: is this always true? :
	// here we open the file itself, notification should get 1.f progress from on progress.
	set_download_state(event.GetInt(), DownloadState::DownloadDone);
	wxArrayString paths;
	paths.Add(event.GetString());
	wxGetApp().plater()->load_files(paths);
	remove_download(event.GetInt());
}
bool Downloader::user_action_callback(DownloaderUserAction action, int id)
{
	if (action == DownloadUserOpenedFolder) {
		auto it = m_dest_folder_cache_map.find(id);
		if (it != m_dest_folder_cache_map.end()) {
			open_folder(it->second);
			return true;
		}
		return false;
	}
	for (size_t i = 0; i < m_downloads.size(); ++i) {
		if (m_downloads[i]->get_id() == id) {
			switch (action) {
			case DownloadUserCanceled:
				m_downloads[i]->cancel();
				return true;
			case DownloadUserPaused:
				m_downloads[i]->pause();
				return true;
			case DownloadUserContinued:
				m_downloads[i]->resume();
				return true;
			case DownloadUserOpenedFolder:
				open_folder(m_downloads[i]->get_dest_folder());
				return true;
			default:
				return false;
			}
		}
	}
	return false;
}

void Downloader::on_name_change(wxCommandEvent& event)
{
   
}

void Downloader::on_paused(wxCommandEvent& event)
{
	size_t id = event.GetInt();
	NotificationManager* ntf_mngr = wxGetApp().notification_manager();
	ntf_mngr->set_download_URL_paused(id);
}

void Downloader::on_canceled(wxCommandEvent& event)
{
	size_t id = event.GetInt();
	NotificationManager* ntf_mngr = wxGetApp().notification_manager();
	ntf_mngr->set_download_URL_canceled(id);
	remove_download(id);
}

void Downloader::set_download_state(int id, DownloadState state)
{
    for (size_t i = 0; i < m_downloads.size(); ++i) {
        if (m_downloads[i]->get_id() == id) {
            m_downloads[i]->set_state(state);
            return;
        }
    }
}

void Downloader::remove_download(int id)
{
	auto pred = [id](const std::unique_ptr<Download> &download) {
		return download->get_id() == id;
	};
	m_downloads.erase(std::remove_if(m_downloads.begin(), m_downloads.end(), pred), m_downloads.end());
}

void Downloader::push_dest_folder_cache(int id, const std::string &dest_folder)
{
    m_dest_folder_cache_que.push_back(id);
    m_dest_folder_cache_map.emplace(id, m_dest_folder.string());
	if (m_dest_folder_cache_que.size() > 100) {
		m_dest_folder_cache_map.erase(m_dest_folder_cache_que.front());
		m_dest_folder_cache_que.pop_front();
	}
}

}
}
