#include "WebView.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/Utils/MacDarkMode.hpp"
#include "libslic3r/Utils.hpp"

#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>
#include <cctype>

#include <nlohmann/json.hpp>

#include <wx/webviewarchivehandler.h>
#include <wx/webviewfshandler.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/utils.h>
#if wxUSE_WEBVIEW_EDGE
#include <wx/msw/webview_edge.h>
#elif defined(__WXMAC__)
#include <wx/osx/webview_webkit.h>
#endif
#include <wx/uri.h>
#if defined(__WIN32__) || defined(__WXMAC__)
#include "wx/private/jsscriptwrapper.h"
#endif

#ifdef __WIN32__
#include <WebView2.h>
#include <Shellapi.h>
#include <slic3r/Utils/Http.hpp>
#elif defined __linux__
#include "slic3r/GUI/Downloader.hpp"

#include <set>
#include <gtk/gtk.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#define WEBKIT_API
struct WebKitWebView;
struct WebKitWebContext;
struct WebKitJavascriptResult;
struct WebKitDownload;
struct WebKitURIRequest;
extern "C" {
WEBKIT_API void
webkit_web_view_run_javascript                       (WebKitWebView             *web_view,
                                                      const gchar               *script,
                                                      GCancellable              *cancellable,
                                                      GAsyncReadyCallback       callback,
                                                      gpointer                  user_data);
WEBKIT_API WebKitJavascriptResult *
webkit_web_view_run_javascript_finish                (WebKitWebView             *web_view,
                                                      GAsyncResult              *result,
                                                      GError                    **error);
WEBKIT_API void
webkit_javascript_result_unref                       (WebKitJavascriptResult    *js_result);
WEBKIT_API WebKitWebContext *
webkit_web_view_get_context                         (WebKitWebView             *web_view);
WEBKIT_API WebKitURIRequest *
webkit_download_get_request                          (WebKitDownload            *download);
WEBKIT_API const gchar *
webkit_uri_request_get_uri                           (WebKitURIRequest          *request);
WEBKIT_API void
webkit_download_set_destination                      (WebKitDownload            *download,
                                                      const gchar               *destination);
WEBKIT_API double
webkit_download_get_estimated_progress               (WebKitDownload            *download);
WEBKIT_API const gchar *
webkit_download_get_destination                      (WebKitDownload            *download);
}
#endif

namespace {

#ifdef __WIN32__
std::string webview_profile_component(std::string value)
{
    for (char &ch : value) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (!std::isalnum(c) && ch != '-' && ch != '_')
            ch = '_';
    }
    return value.empty() ? "default" : value;
}

boost::filesystem::path edge_webview_data_path()
{
    static const boost::filesystem::path profile_dir = [] {
        const std::string version = webview_profile_component(Orca_Flashforge_VERSION);
        const std::string pid = std::to_string(Slic3r::get_current_pid());

        wxString local_data_dir = wxStandardPaths::Get().GetUserLocalDataDir();
        boost::filesystem::path base_dir;
        if (!local_data_dir.empty()) {
            // Keep the new WebView2 profile outside wxWidgets' default user data
            // folder, which is still used by release_0516.
            base_dir = boost::filesystem::path((local_data_dir + "-webview2-isolated-v2").ToUTF8().data());
        } else {
            wxFileName exe_path(wxStandardPaths::Get().GetExecutablePath());
            base_dir = boost::filesystem::path(exe_path.GetPath().ToUTF8().data()) / "webview2-isolated-v2";
        }

        boost::filesystem::path path = base_dir / version / pid;
        path.make_preferred();
        return path;
    }();
    return profile_dir;
}

wxWebViewConfiguration create_edge_webview_config()
{
    wxWebViewConfiguration config = wxWebView::NewConfiguration(wxWebViewBackendEdge);

    boost::filesystem::path profile_dir = edge_webview_data_path();
    boost::system::error_code ec;
    boost::filesystem::create_directories(profile_dir, ec);
    if (ec) {
        BOOST_LOG_TRIVIAL(warning) << "Failed to create WebView2 user data folder: "
                                   << profile_dir.string() << ", " << ec.message();
    }

    config.SetDataPath(wxString::FromUTF8(profile_dir.string()));
    config.EnablePersistentStorage(true);
    BOOST_LOG_TRIVIAL(info) << "WebView2 user data folder: " << profile_dir.string();

    return config;
}
#endif

} // namespace

#ifdef __WIN32__
// Run Download and Install in another thread so we don't block the UI thread
DWORD DownloadAndInstallWV2RT() {

  int returnCode = 2; // Download failed
  // Use fwlink to download WebView2 Bootstrapper at runtime and invoke installation
  // Broken/Invalid Https Certificate will fail to download
  // Use of the download link below is governed by the below terms. You may acquire the link
  // for your use at https://developer.microsoft.com/microsoft-edge/webview2/. Microsoft owns
  // all legal right, title, and interest in and to the WebView2 Runtime Bootstrapper
  // ("Software") and related documentation, including any intellectual property in the
  // Software. You must acquire all code, including any code obtained from a Microsoft URL,
  // under a separate license directly from Microsoft, including a Microsoft download site
  // (e.g., https://developer.microsoft.com/microsoft-edge/webview2/).
  // HRESULT hr = URLDownloadToFileW(NULL, L"https://go.microsoft.com/fwlink/p/?LinkId=2124703",
  //                               L".\\plugin\\MicrosoftEdgeWebview2Setup.exe", 0, 0);
  fs::path target_file_path = (fs::temp_directory_path() / "MicrosoftEdgeWebview2Setup.exe");
  bool downloaded = false;
  Slic3r::Http::get("https://go.microsoft.com/fwlink/p/?LinkId=2124703")
      .on_error([](std::string body, std::string error, unsigned http_status) {

      })
      .on_complete([&downloaded, target_file_path](std::string body, unsigned http_status) {
        fs::fstream file(target_file_path, std::ios::out | std::ios::binary | std::ios::trunc);
        file.write(body.c_str(), body.size());
        file.flush();
        file.close();

        downloaded = true;
      })
      .perform_sync();
  // Sleep for 1 second to wait for the buffer writen into disk
  std::this_thread::sleep_for(1000ms);
  if (downloaded) {
    // Either Package the WebView2 Bootstrapper with your app or download it using fwlink
    // Then invoke install at Runtime.
    SHELLEXECUTEINFOW shExInfo = {0};
    shExInfo.cbSize = sizeof(shExInfo);
    shExInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
    shExInfo.hwnd = 0;
    shExInfo.lpVerb = L"runas";
    shExInfo.lpFile = target_file_path.generic_wstring().c_str();
    shExInfo.lpParameters = L" /install";
    shExInfo.lpDirectory = 0;
    shExInfo.nShow = 0;
    shExInfo.hInstApp = 0;

    if (ShellExecuteExW(&shExInfo)) {
      WaitForSingleObject(shExInfo.hProcess, INFINITE);
      returnCode = 0; // Install successfull
    } else {
      returnCode = 1; // Install failed
    }
  }
  return returnCode;
}

class WebViewEdge : public wxWebViewEdge
{
public:
    WebViewEdge() = default;

    explicit WebViewEdge(const wxWebViewConfiguration &config)
        : wxWebViewEdge(config)
    {
    }

    bool SetUserAgent(const wxString &userAgent)
    {
        bool dark = userAgent.Contains("dark");
        SetColorScheme(dark ? COREWEBVIEW2_PREFERRED_COLOR_SCHEME_DARK : COREWEBVIEW2_PREFERRED_COLOR_SCHEME_LIGHT);

        ICoreWebView2 *webView2 = (ICoreWebView2 *) GetNativeBackend();
        if (webView2) {
            ICoreWebView2Settings *settings;
            HRESULT                hr = webView2->get_Settings(&settings);
            if (hr == S_OK) {
                ICoreWebView2Settings2 *settings2;
                hr = settings->QueryInterface(&settings2);
                if (hr == S_OK) {
                    settings2->put_UserAgent(userAgent.wc_str());
                    settings2->Release();
                    return true;
                }
            }
            settings->Release();
            return false;
        }
        pendingUserAgent = userAgent;
        return true;
    }

    bool SetColorScheme(COREWEBVIEW2_PREFERRED_COLOR_SCHEME colorScheme)
    {
        ICoreWebView2 *webView2 = (ICoreWebView2 *) GetNativeBackend();
        if (webView2) {
            ICoreWebView2_13 * webView2_13;
            HRESULT           hr = webView2->QueryInterface(&webView2_13);
            if (hr == S_OK) {
                ICoreWebView2Profile *profile;
                hr = webView2_13->get_Profile(&profile);
                if (hr == S_OK) {
                    profile->put_PreferredColorScheme(colorScheme);
                    profile->Release();
                    return true;
                }
                webView2_13->Release();
            }
            return false;
        }
        pendingColorScheme = colorScheme;
        return true;
    }

    void DoGetClientSize(int *x, int *y) const override
    {
        if (!pendingUserAgent.empty()) {
            auto thiz = const_cast<WebViewEdge *>(this);
            auto userAgent = std::move(thiz->pendingUserAgent);
            thiz->pendingUserAgent.clear();
            thiz->SetUserAgent(userAgent);
        }
        if (pendingColorScheme) {
            auto thiz      = const_cast<WebViewEdge *>(this);
            auto colorScheme = pendingColorScheme;
            thiz->pendingColorScheme = COREWEBVIEW2_PREFERRED_COLOR_SCHEME_AUTO;
            thiz->SetColorScheme(colorScheme);
        }
        wxWebViewEdge::DoGetClientSize(x, y);
    };
private:
    wxString pendingUserAgent;
    COREWEBVIEW2_PREFERRED_COLOR_SCHEME pendingColorScheme = COREWEBVIEW2_PREFERRED_COLOR_SCHEME_AUTO;
};

#elif defined __WXOSX__

class WebViewWebKit : public wxWebViewWebKit
{
public:
    WebViewWebKit()
        : wxWebViewWebKit(wxWebView::NewConfiguration(wxWebViewBackendWebKit))
    {
    }

    ~WebViewWebKit() override
    {
        RemoveScriptMessageHandler("wx");
    }
};

#endif

class FakeWebView : public wxWebView
{
    virtual bool Create(wxWindow* parent, wxWindowID id, const wxString& url, const wxPoint& pos, const wxSize& size, long style, const wxString& name) override { return false; }
    virtual wxString GetCurrentTitle() const override { return wxString(); }
    virtual wxString GetCurrentURL() const override { return wxString(); }
    virtual bool IsBusy() const override { return false; }
    virtual bool IsEditable() const override { return false; }
    virtual void LoadURL(const wxString& url) override { }
    virtual void Print() override { }
    virtual void RegisterHandler(wxSharedPtr<wxWebViewHandler> handler) override { }
    virtual void Reload(wxWebViewReloadFlags flags = wxWEBVIEW_RELOAD_DEFAULT) override { }
    virtual bool RunScript(const wxString& javascript, wxString* output = NULL) const override { return false; }
    virtual void SetEditable(bool enable = true) override { }
    virtual void Stop() override { }
    virtual bool CanGoBack() const override { return false; }
    virtual bool CanGoForward() const override { return false; }
    virtual void GoBack() override { }
    virtual void GoForward() override { }
    virtual void ClearHistory() override { }
    virtual void EnableHistory(bool enable = true) override { }
    virtual wxVector<wxSharedPtr<wxWebViewHistoryItem>> GetBackwardHistory() override { return {}; }
    virtual wxVector<wxSharedPtr<wxWebViewHistoryItem>> GetForwardHistory() override { return {}; }
    virtual void LoadHistoryItem(wxSharedPtr<wxWebViewHistoryItem> item) override { }
    virtual bool CanSetZoomType(wxWebViewZoomType type) const override { return false; }
    virtual float GetZoomFactor() const override { return 0.0f; }
    virtual wxWebViewZoomType GetZoomType() const override { return wxWebViewZoomType(); }
    virtual void SetZoomFactor(float zoom) override { }
    virtual void SetZoomType(wxWebViewZoomType zoomType) override { }
    virtual bool CanUndo() const override { return false; }
    virtual bool CanRedo() const override { return false; }
    virtual void Undo() override { }
    virtual void Redo() override { }
    virtual void* GetNativeBackend() const override { return nullptr; }
    virtual void DoSetPage(const wxString& html, const wxString& baseUrl) override { }
};

wxDEFINE_EVENT(EVT_WEBVIEW_RECREATED, wxCommandEvent);

static std::vector<wxWebView*> g_webviews;
static std::vector<wxWebView*> g_delay_webviews;

static wxString BuildFlashForgeUserAgent(bool dark)
{
    const char *theme = dark ? "dark" : "light";

#ifdef __WIN32__
    return wxString::Format(
        "Orca-Flashforge/%s (Windows; %s) Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
        "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/107.0.0.0 Safari/537.36 Edg/107.0.1418.52",
        Orca_Flashforge_VERSION, theme);
#elif defined(__WXMAC__)
    return wxString::Format(
        "Orca-Flashforge/%s (macOS; %s) Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
        "AppleWebKit/605.1.15 (KHTML, like Gecko)",
        Orca_Flashforge_VERSION, theme);
#else
    return wxString::Format(
        "Orca-Flashforge/%s (Linux; %s) Mozilla/5.0 (X11; Linux x86_64) "
        "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/107.0.0.0 Safari/537.36",
        Orca_Flashforge_VERSION, theme);
#endif
}

#ifdef __linux__
namespace {

struct NativeDownloadInfo
{
    size_t   id{0};
    wxString filename;
    wxString folder;
    wxString path;
};

wxString sanitize_download_filename(const wxString &filename)
{
    wxString sanitized;
    wxString forbidden_chars = wxFileName::GetForbiddenChars();
    for (auto ch : filename) {
        if (forbidden_chars.find(ch) == wxNOT_FOUND) {
            sanitized.append(ch);
        }
    }
    if (sanitized.empty()) {
        sanitized = "download";
    }
    return sanitized;
}

wxString filename_from_uri(const wxString &uri)
{
    wxString without_query = uri.BeforeFirst('?');
    wxString filename = wxFileName(without_query).GetFullName();
    if (filename.empty()) {
        filename = "download";
    }
    return sanitize_download_filename(filename);
}

wxString configured_download_folder()
{
    if (Slic3r::GUI::wxGetApp().app_config != nullptr) {
        wxString folder = wxString::FromUTF8(Slic3r::GUI::wxGetApp().app_config->get("download_path"));
        if (!folder.empty() && wxFileName::DirExists(folder)) {
            return folder;
        }
    }
    return wxStandardPaths::Get().GetDocumentsDir();
}

wxString unique_download_path(const wxString &folder, const wxString &filename)
{
    wxFileName file_name(folder, sanitize_download_filename(filename));
    wxString base = file_name.GetName();
    wxString ext = file_name.GetExt();

    for (int i = 1; file_name.Exists(); ++i) {
        file_name.SetName(wxString::Format("%s(%d)", base, i));
        file_name.SetExt(ext);
    }
    return file_name.GetFullPath();
}

gboolean on_linux_download_decide_destination(WebKitDownload *download, gchar *suggested_filename, gpointer user_data)
{
    auto *info = static_cast<NativeDownloadInfo *>(user_data);
    info->filename = sanitize_download_filename(wxString::FromUTF8(suggested_filename ? suggested_filename : ""));
    info->path = unique_download_path(info->folder, info->filename);

    char *destination_uri = g_filename_to_uri(info->path.utf8_str(), nullptr, nullptr);
    if (destination_uri == nullptr) {
        Slic3r::GUI::show_linux_web_download_error(info->id, _L("Download failed"));
        return FALSE;
    }

    webkit_download_set_destination(download, destination_uri);
    g_free(destination_uri);
    Slic3r::GUI::show_linux_web_download_start(info->id, info->filename, info->folder);
    return TRUE;
}

void on_linux_download_received_data(WebKitDownload *download, guint64, gpointer user_data)
{
    auto *info = static_cast<NativeDownloadInfo *>(user_data);
    Slic3r::GUI::show_linux_web_download_progress(info->id, static_cast<float>(webkit_download_get_estimated_progress(download)));
}

void on_linux_download_finished(WebKitDownload *download, gpointer user_data)
{
    auto *info = static_cast<NativeDownloadInfo *>(user_data);
    wxString path = info->path;
    if (path.empty()) {
        const gchar *destination = webkit_download_get_destination(download);
        if (destination != nullptr) {
            char *filename = g_filename_from_uri(destination, nullptr, nullptr);
            if (filename != nullptr) {
                path = wxString::FromUTF8(filename);
                g_free(filename);
            }
        }
    }
    Slic3r::GUI::show_linux_web_download_complete(info->id, path);
    delete info;
}

void on_linux_download_failed(WebKitDownload *, GError *error, gpointer user_data)
{
    auto *info = static_cast<NativeDownloadInfo *>(user_data);
    Slic3r::GUI::show_linux_web_download_error(info->id, error != nullptr && error->message != nullptr
        ? wxString::FromUTF8(error->message)
        : _L("Download failed"));
    delete info;
}

void on_linux_download_started(WebKitWebContext *, WebKitDownload *download, gpointer)
{
    static size_t next_download_id = 2000000000;
    auto *info = new NativeDownloadInfo;
    info->id = ++next_download_id;
    info->folder = configured_download_folder();

    WebKitURIRequest *request = webkit_download_get_request(download);
    const gchar *uri = request != nullptr ? webkit_uri_request_get_uri(request) : nullptr;
    info->filename = filename_from_uri(wxString::FromUTF8(uri != nullptr ? uri : ""));
    Slic3r::GUI::show_linux_web_download_start(info->id, info->filename, info->folder);

    g_signal_connect(download, "decide-destination", G_CALLBACK(on_linux_download_decide_destination), info);
    g_signal_connect(download, "received-data", G_CALLBACK(on_linux_download_received_data), info);
    g_signal_connect(download, "finished", G_CALLBACK(on_linux_download_finished), info);
    g_signal_connect(download, "failed", G_CALLBACK(on_linux_download_failed), info);
}

void connect_linux_download_signals(wxWebView *webView)
{
    if (webView == nullptr) {
        return;
    }
    auto *native_webview = static_cast<WebKitWebView *>(webView->GetNativeBackend());
    if (native_webview == nullptr) {
        return;
    }
    WebKitWebContext *context = webkit_web_view_get_context(native_webview);
    if (context == nullptr) {
        return;
    }

    static std::set<WebKitWebContext *> connected_contexts;
    if (!connected_contexts.insert(context).second) {
        return;
    }

    g_signal_connect(context, "download-started", G_CALLBACK(on_linux_download_started), nullptr);
}

} // namespace
#endif

class WebViewRef : public wxObjectRefData
{
public:
    WebViewRef(wxWebView *webView) : m_webView(webView) {}
    ~WebViewRef() {
        auto iter = std::find(g_webviews.begin(), g_webviews.end(), m_webView);
        assert(iter != g_webviews.end());
        if (iter != g_webviews.end())
            g_webviews.erase(iter);
    }
    wxWebView *m_webView;
};

wxWebView* WebView::CreateWebView(wxWindow * parent, wxString const & url)
{
#if wxUSE_WEBVIEW_EDGE
    // Check if a fixed version of edge is present in
    // $executable_path/edge_fixed and use it
    wxFileName edgeFixedDir(wxStandardPaths::Get().GetExecutablePath());
    edgeFixedDir.SetFullName("");
    edgeFixedDir.AppendDir("edge_fixed");
    if (edgeFixedDir.DirExists()) {
        wxWebViewEdge::MSWSetBrowserExecutableDir(edgeFixedDir.GetFullPath());
        wxLogMessage("Using fixed edge version");
    }
#endif
    auto url2  = url;
#ifdef __WIN32__
    url2.Replace("\\", "/");
#endif
    if (!url2.empty()) { url2 = wxURI(url2).BuildURI(); }
    BOOST_LOG_TRIVIAL(trace) << __FUNCTION__ << ": " << url2.ToUTF8();

#ifdef __WIN32__
    wxWebView* webView = new WebViewEdge(create_edge_webview_config());
#elif defined(__WXOSX__)
    wxWebView* webView = new WebViewWebKit;
#else
    auto webView = wxWebView::New();
#endif
    if (webView) {
        webView->SetBackgroundColour(StateColor::darkModeColorFor(*wxWHITE));
#ifdef __WIN32__
        webView->SetUserAgent(BuildFlashForgeUserAgent(Slic3r::GUI::wxGetApp().dark_mode()));
        webView->Create(parent, wxID_ANY, url2, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
        // We register the wxfs:// protocol for testing purposes
        webView->RegisterHandler(wxSharedPtr<wxWebViewHandler>(new wxWebViewArchiveHandler("bbl")));
        // And the memory: file system
        webView->RegisterHandler(wxSharedPtr<wxWebViewHandler>(new wxWebViewFSHandler("memory")));
#else
        // With WKWebView handlers need to be registered before creation.
        // On Linux (WebKit2GTK), URI schemes are registered globally and can only
        // be registered once, so guard against multiple registrations.
        static bool s_schemes_registered = false;
        if (!s_schemes_registered) {
            webView->RegisterHandler(wxSharedPtr<wxWebViewHandler>(new wxWebViewArchiveHandler("wxfs")));
            webView->RegisterHandler(wxSharedPtr<wxWebViewHandler>(new wxWebViewFSHandler("memory")));
            s_schemes_registered = true;
        }
        webView->Create(parent, wxID_ANY, url2, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
        webView->SetUserAgent(BuildFlashForgeUserAgent(Slic3r::GUI::wxGetApp().dark_mode()));
#ifdef __linux__
        connect_linux_download_signals(webView);
#endif
#endif
#ifdef __WXMAC__
        WKWebView * wkWebView = (WKWebView *) webView->GetNativeBackend();
        Slic3r::GUI::WKWebView_setTransparentBackground(wkWebView);
#endif
        auto addScriptMessageHandler = [] (wxWebView *webView) {
            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ": begin to add script message handler for wx.";
            Slic3r::GUI::wxGetApp().set_adding_script_handler(true);
            if (!webView->AddScriptMessageHandler("wx"))
                wxLogError("Could not add script message handler");
            Slic3r::GUI::wxGetApp().set_adding_script_handler(false);
            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ": finished add script message handler for wx.";
        };
#ifndef __WIN32__
        webView->CallAfter([webView, addScriptMessageHandler] {
#endif
            if (Slic3r::GUI::wxGetApp().is_adding_script_handler()) {
                g_delay_webviews.push_back(webView);
            } else {
                addScriptMessageHandler(webView);
                while (!g_delay_webviews.empty()) {
                    auto views = std::move(g_delay_webviews);
                    for (auto wv : views)
                        addScriptMessageHandler(wv);
                }
            }
#ifndef __WIN32__
        });
#endif
        webView->EnableContextMenu(true);
    } else {
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ": failed. Use fake web view.";
        webView = new FakeWebView;
    }
    webView->SetRefData(new WebViewRef(webView));
    g_webviews.push_back(webView);
    return webView;
}
#if wxUSE_WEBVIEW_EDGE
void WebView::InitWebViewDataPath()
{
#ifdef __WIN32__
    const wxString data_path = wxString::FromUTF8(edge_webview_data_path().string());
    wxString existing_path;
    if (wxGetEnv("WEBVIEW2_USER_DATA_FOLDER", &existing_path) && existing_path != data_path) {
        BOOST_LOG_TRIVIAL(info) << "Override WEBVIEW2_USER_DATA_FOLDER for this process: "
                                << existing_path.ToUTF8().data() << " -> " << data_path.ToUTF8().data();
    }
    if (!wxSetEnv("WEBVIEW2_USER_DATA_FOLDER", data_path)) {
        BOOST_LOG_TRIVIAL(warning) << "Failed to set WEBVIEW2_USER_DATA_FOLDER: "
                                   << data_path.ToUTF8().data();
    } else {
        BOOST_LOG_TRIVIAL(info) << "WEBVIEW2_USER_DATA_FOLDER=" << data_path.ToUTF8().data();
    }
#endif
}

bool WebView::CheckWebViewRuntime()
{
    wxWebViewFactoryEdge factory;
    auto wxVersion = factory.GetVersionInfo(wxVersionContext::RunTime);
    return wxVersion.GetMajor() != 0;
}

bool WebView::DownloadAndInstallWebViewRuntime()
{
    return DownloadAndInstallWV2RT() == 0;
}
#endif
void WebView::LoadUrl(wxWebView * webView, wxString const &url)
{
    auto url2  = url;
#ifdef __WIN32__
    url2.Replace("\\", "/");
#endif
    if (!url2.empty()) { url2 = wxURI(url2).BuildURI(); }
    BOOST_LOG_TRIVIAL(trace) << __FUNCTION__ << url2.ToUTF8();
    webView->LoadURL(url2);
}

bool WebView::RunScript(wxWebView *webView, wxString const &javascript)
{
    if (Slic3r::GUI::wxGetApp().app_config->get("internal_developer_mode") == "true"
            && javascript.find("studio_userlogin") == wxString::npos)
        wxLogMessage("Running JavaScript:\n%s\n", javascript);

    try {
#ifdef __WIN32__
        ICoreWebView2 *   webView2 = (ICoreWebView2 *) webView->GetNativeBackend();
        if (webView2 == nullptr)
            return false;
        return webView2->ExecuteScript(javascript, NULL) == 0;
#elif defined __WXMAC__
        WKWebView * wkWebView = (WKWebView *) webView->GetNativeBackend();
        Slic3r::GUI::WKWebView_evaluateJavaScript(wkWebView, javascript, nullptr);
        return true;
#else
        WebKitWebView *wkWebView = (WebKitWebView *) webView->GetNativeBackend();
        webkit_web_view_run_javascript(
            wkWebView, javascript.utf8_str(), NULL,
            [](GObject *wkWebView, GAsyncResult *res, void *) {
                GError * error = NULL;
                auto result = webkit_web_view_run_javascript_finish((WebKitWebView*)wkWebView, res, &error);
                if (!result)
                    g_error_free (error);
                else
                    webkit_javascript_result_unref (result);
        }, NULL);
        return true;
#endif
    } catch (std::exception &) {
        return false;
    }
}

void WebView::AddOpenNewWindowScript(wxWebView *webView)
{
    if (webView == nullptr) {
        return;
    }

    // WebKitGTK 2.50.x aborts when wx handles window.open through the
    // new-window signal. Route real new-window requests through script
    // messages, but keep same-context navigation inside the WebView.
    webView->AddUserScript(R"JS((function(){
  window.open = function(url, target){
    try {
      if (!url) {
        return null;
      }

      var normalizedTarget = target == null ? "" : ("" + target).toLowerCase();
      if (normalizedTarget === "_self") {
        window.location.assign(url);
        return window;
      }
      if (normalizedTarget === "_parent") {
        window.parent.location.assign(url);
        return window.parent;
      }
      if (normalizedTarget === "_top") {
        window.top.location.assign(url);
        return window.top;
      }

      var resolvedUrl = "" + url;
      try {
        resolvedUrl = new URL(resolvedUrl, document.baseURI).href;
      } catch(e) {}

      var message = JSON.stringify({
        command: "ff_open_new_window",
        url: resolvedUrl,
        target: target ? "" + target : ""
      });

      // wxWebView exposes different message bridges depending on the backend.
      if (window.wx && typeof window.wx.postMessage === "function") {
        window.wx.postMessage(message);
      } else if (window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.wx) {
        window.webkit.messageHandlers.wx.postMessage(message);
      }
    } catch(e) {}
    return null;
  };
})();)JS");
}

bool WebView::TryGetOpenNewWindowUrl(wxWebViewEvent &event, wxString *url)
{
    try {
        nlohmann::json json = nlohmann::json::parse(event.GetString().utf8_string());
        if (json.value("command", std::string()) != "ff_open_new_window") {
            return false;
        }

        wxString parsed_url = wxString::FromUTF8(json.value("url", std::string()));
        if (parsed_url.empty()) {
            return false;
        }

        if (url != nullptr) {
            *url = parsed_url;
        }
        return true;
    } catch (const std::exception &) {
        return false;
    }
}

void WebView::RecreateAll()
{
    auto dark = Slic3r::GUI::wxGetApp().dark_mode();
    for (auto webView : g_webviews) {
        webView->SetUserAgent(BuildFlashForgeUserAgent(dark));
        webView->Reload();
    }
}
