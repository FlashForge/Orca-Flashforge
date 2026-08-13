#include "FFRTMPVideoCtrl.h"

#include <wx/dcbuffer.h>
#include <wx/scrolwin.h>

#include <boost/log/trivial.hpp>
#include <thread>
#include <chrono>
#include <cctype>
#include <algorithm>
#include <cstring>
#include <sstream>

// [FFRTMP] 调试日志总开关：置 false 关闭全部 [FFRTMP] 输出（需要排查时改回 true）。
static const bool g_ffrtmp_log_enabled = false;
// Independent diagnostic switch: visibility/layout logs remain available while
// verbose stream decoding logs are disabled.
static const bool g_camera_panel_state_log_enabled = false;
// Frame diagnostics are deliberately independent from the verbose stream log.
// Normal traffic is summarized once per second; only large gaps/latency are
// logged immediately so diagnostics do not become a new source of frame loss.
static const bool g_ffrtmp_frame_diag_enabled = false;
static constexpr int64_t g_ffrtmp_frame_gap_warn_ms = 100;
static constexpr int64_t g_ffrtmp_ui_latency_warn_ms = 100;
// Independent low-volume timing diagnostic for HTTP/MJPEG stalls.
static const bool g_mjpeg_perf_diag_enabled = false;
static constexpr int64_t g_mjpeg_perf_warn_ms = 100;

static int64_t ffrtmp_frame_now_ms()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

static void ffrtmp_frame_log(const std::string &msg)
{
    if (!g_ffrtmp_frame_diag_enabled) return;
    BOOST_LOG_TRIVIAL(info) << "[FFRTMP_FRAME] " << msg;
#ifdef _WIN32
    OutputDebugStringA(("[FFRTMP_FRAME] " + msg + "\n").c_str());
#endif
}

// Debug output helper — writes to both Boost log and Visual Studio / DebugView on Windows
static void ffrtmp_mjpeg_perf_log(const std::string &msg)
{
    if (!g_mjpeg_perf_diag_enabled) return;
    BOOST_LOG_TRIVIAL(info) << "[MJPEG_PERF] " << msg;
#ifdef _WIN32
    OutputDebugStringA(("[MJPEG_PERF] " + msg + "\n").c_str());
#endif
}

static void ffrtmp_log(const std::string &msg)
{
    if (!g_ffrtmp_log_enabled) return;
    BOOST_LOG_TRIVIAL(info) << "[FFRTMP] " << msg;
#ifdef _WIN32
    OutputDebugStringA(("[FFRTMP] " + msg + "\n").c_str());
#endif
}
static void ffrtmp_err(const std::string &msg)
{
    if (!g_ffrtmp_log_enabled) return;
    BOOST_LOG_TRIVIAL(error) << "[FFRTMP] ERROR: " << msg;
#ifdef _WIN32
    OutputDebugStringA(("[FFRTMP] ERROR: " + msg + "\n").c_str());
#endif
}

// [UNBIND] 解绑耗时诊断：带 steady_clock 毫秒时间戳，与 SingleDeviceState 的 [UNBIND]
// 日志共用同一时间轴，便于把“UI 线程回收”与“后台线程/解码线程退出”对齐排查。
// 定位后把开关置 false 即可关闭。
static const bool g_unbind_timing_enabled = false;
static void ffrtmp_ts(const std::string &tag)
{
    if (!g_unbind_timing_enabled) return;
    int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now().time_since_epoch()).count();
    std::string line = "[UNBIND] t=" + std::to_string(ms) + "ms | " + tag;
    BOOST_LOG_TRIVIAL(info) << line;
#ifdef _WIN32
    OutputDebugStringA((line + "\n").c_str());
#endif
}

#ifdef FFRTMP_USE_FFMPEG
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
#endif

#include "libslic3r/Utils.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/MainFrame.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/FlashForge/MultiComMgr.hpp"
#include "slic3r/GUI/FlashForge/ComCommand.hpp"

namespace Slic3r { namespace GUI {

FFRTMPVideoCtrl::FFRTMPVideoCtrl(wxWindow *parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE)
{
    SetBackgroundColour(*wxBLACK);
    // Match the responsive device-page column. 621 DIP makes the two-column
    // layout wider than a 1080p client area when Windows scaling is 125%.
    constexpr int inline_camera_min_width = 580;
    SetMinSize(wxSize(FromDIP(inline_camera_min_width), FromDIP(466)));

    // 记录内联时的父窗口，弹窗（全屏预览）关闭后需归还，否则控件会被孤立、内联区域永久空白
    m_inline_parent = parent;

    m_offline_bitmap.Create(FromDIP(640), FromDIP(360), 24);
    {
        wxMemoryDC dc(m_offline_bitmap);
        dc.SetBackground(*wxBLACK_BRUSH);
        dc.Clear();
    }
    // 缺省图：进设备页/暂停且尚无画面时显示。文件名可替换（等待指定最终图片）。
    // 加载失败则回退为黑底占位（m_offline_bitmap）。
    m_placeholder_bitmap     = ScalableBitmap(this, "camera_offline", 1500).bmp();
    m_playIconMap["offline"] = ScalableBitmap(this, "play_offline", 29);
    m_playIconMap["play"] = ScalableBitmap(this, "play_normal", 29);
    m_playIconMap["pause"] = ScalableBitmap(this, "play_pause", 29);

    Bind(wxEVT_PAINT, &FFRTMPVideoCtrl::OnPaint, this);
    Bind(wxEVT_SIZE,  &FFRTMPVideoCtrl::OnSize,  this);
    Bind(wxEVT_SHOW, [this](wxShowEvent &event) {
        logPanelState(event.IsShown() ? "wxEVT_SHOW shown" : "wxEVT_SHOW hidden");
        event.Skip();
    });
    Bind(wxEVT_LEFT_DOWN, &FFRTMPVideoCtrl::OnLeftDown, this);  // 左下角播放/暂停按钮

    logPanelState("constructor complete");
}

void FFRTMPVideoCtrl::logPanelState(const char *reason) const
{
    if (!g_camera_panel_state_log_enabled) return;

    const wxSize size   = GetSize();
    const wxSize client = GetClientSize();
    const wxSize min    = GetMinSize();
    const wxSize max    = GetMaxSize();
    FFRTMPVideoCtrl *self = const_cast<FFRTMPVideoCtrl *>(this);
    wxSizer *sizer        = self->GetContainingSizer();
    wxSizerItem *item     = sizer ? sizer->GetItem(self) : nullptr;
    wxWindow *parent      = self->GetParent();
    const wxSize parent_size = parent ? parent->GetClientSize() : wxDefaultSize;

    std::ostringstream os;
    os << "[CAMERA_PANEL_STATE] " << (reason ? reason : "")
       << " this=" << static_cast<const void *>(this)
       << " parent=" << static_cast<void *>(parent)
       << " parentShown=" << (parent ? parent->IsShown() : false)
       << " parentClient=" << parent_size.x << 'x' << parent_size.y
       << " shown=" << IsShown()
       << " shownOnScreen=" << IsShownOnScreen()
       << " enabled=" << IsEnabled()
       << " size=" << size.x << 'x' << size.y
       << " client=" << client.x << 'x' << client.y
       << " min=" << min.x << 'x' << min.y
       << " max=" << max.x << 'x' << max.y
       << " hasSizer=" << (sizer != nullptr)
       << " sizerItem=" << static_cast<void *>(item)
       << " itemShown=" << (item ? item->IsShown() : false)
       << " itemMin=" << (item ? std::to_string(item->GetMinSize().x) + "x" + std::to_string(item->GetMinSize().y) : "n/a")
       << " itemRect=" << (item ? std::to_string(item->GetRect().width) + "x" + std::to_string(item->GetRect().height) : "n/a")
       << " popup=" << static_cast<void *>(m_popup_dlg)
       << " forcePlaceholder=" << m_force_default_placeholder.load()
       << " playState=" << static_cast<int>(m_play_state.load());
    BOOST_LOG_TRIVIAL(info) << os.str();
#ifdef _WIN32
    OutputDebugStringA((os.str() + "\n").c_str());
#endif
}

FFRTMPVideoCtrl::~FFRTMPVideoCtrl()
{
    StopStream();

    if (m_popup_dlg) {
        m_popup_dlg->Destroy();
        m_popup_dlg = nullptr;
    }
}

void FFRTMPVideoCtrl::StartStream(const std::string &url)
{
    m_force_default_placeholder = false;

    // Unconditional debug output — always fires regardless of FFRTMP_USE_FFMPEG
    ffrtmp_log("StartStream called, url='" + url + "'");
    // [UNBIND] 若解绑期间意外触发了 StartStream，它内部会同步 StopStream()（见 SS0..SS4），
    // 从而在 UI 线程 join 卡住的解码线程——这是最需要警惕的卡顿路径之一。
    ffrtmp_ts("SN0. StartStream ENTER url=" + url);

#ifdef FFRTMP_USE_FFMPEG
    if (url.empty()) {
        ffrtmp_log("url is empty, stopping stream");
        StopStream();
        return;
    }

    if (m_running && m_paused && m_url == url) {
        ffrtmp_log("paused, ignoring same-url StartStream");
        return;
    }

    // 暂停只是“显示冻结”，与拉流解耦：即使处于暂停显示态，解码/拉流仍照常进行/切换，
    // 因此这里不拦截，正常起流（切设备时会切到新流；用户点播放即恢复显示最新帧）。
    if (m_running && m_url == url) {
        ffrtmp_log("Already streaming this url, ignoring duplicate StartStream");
        return;
    }

    StopStream();

    m_url = url;
    m_running = true;
    m_reconnect_attempts = 0;
    m_offline_shown = false;
    m_ever_got_frame = false;                // 新会话：尚未出过画面（首次加载中不报断开）
    m_last_frame_time_ms = 0;
    m_stream_started_at_ms = ffrtmp_frame_now_ms();
    m_last_frame_diag_summary_ms = 0;
    m_paused = m_display_paused_pref.load();  // 进设备页默认“暂停”(显示缺省图+播放按钮)；
                                              // 用户播放过则保持播放（切设备沿用）。atomic 需 .load()
    // A URL may already be cached before the first user click; that must not be
    // mistaken for an already-warmed stream. Every newly created decoder
    // session gets the initial publisher warm-up delay.
    // 默认暂停 → 直接进入 Paused（显示缺省图 + 播放按钮，等用户点播放）；
    // 否则进入 Initializing（加载阶段）。
    setPlayState(m_paused ? PlayState::Paused : PlayState::Initializing);

    ffrtmp_log("Starting stream: " + m_url);

    // 内联显示：摄像头控件常驻设备状态面板。起流时确保控件可见（此前可能被
    // setOffline/StopStream 隐藏过），并触发一次布局。弹窗模式下由弹窗自行管理显示。
    CallAfter([this]() {
        if (!m_popup_dlg && !IsShown()) {
            Show();
            if (wxSizer *s = GetContainingSizer()) s->Layout();
        }
        Refresh();
    });

    // Notify the printer before FFmpeg starts opening the HLS manifest. The
    // legacy web player used the same order: camera open first, then player
    // startup after a short warm-up. Creator 5 otherwise returns 404 until its
    // publisher has created the first playlist.
    // Send exactly one openstream command for this user-started session.
    sendCameraOpen(m_curComId);

    m_thread = std::make_unique<std::thread>(&FFRTMPVideoCtrl::DecoderThreadFunc, this);
#else
    (void)url;
    ffrtmp_err("FFmpeg not available (FFRTMP_USE_FFMPEG not defined)");
#endif
}

void FFRTMPVideoCtrl::joinStopReaper()
{
    if (m_stop_reaper.joinable()) {
        m_stop_reaper.join();
    }
}

void FFRTMPVideoCtrl::StopStream()
{
    ffrtmp_log("StopStream called");
    // [UNBIND] 同步停流：本函数在调用线程（通常是 UI 线程）上同步 join 解码线程，
    // 若解码线程正卡在 avformat_open_input（见 [decode] 日志），这里会阻塞 UI —— 重点排查。
    // 若解绑期间出现 SS0/SS3 之间的大时间空档，说明卡顿源就是这里（多半由 StartStream 触发）。
    ffrtmp_ts("SS0. StopStream ENTER (同步, 会在本线程 join 解码线程)");
    m_running = false;

    // 先等待上一次异步停止的后台回收结束，避免新旧解码线程并发访问 FFmpeg 上下文。
    ffrtmp_ts("SS1. joinStopReaper BEGIN");
    joinStopReaper();
    ffrtmp_ts("SS2. joinStopReaper END; m_thread->join BEGIN");

    if (m_thread && m_thread->joinable()) {
        m_thread->join();
    }
    m_thread.reset();
    ffrtmp_ts("SS3. m_thread->join END; CloseStream BEGIN");

#ifdef FFRTMP_USE_FFMPEG
    CloseStream();
#endif
    sendCameraClose();
    // A stopped media session is no longer Playing/Paused. Keep the cached URL
    // so the next Play click creates a fresh session and sends one new open.
    // Offline/no-camera callers overwrite Not with their more specific state.
    m_paused = false;
    setPlayState(PlayState::Not);
    ffrtmp_ts("SS4. CloseStream END; StopStream EXIT");

    {
        wxCriticalSectionLocker lock(m_frame_cs);
        m_frame_ready = false;
        m_rgb_buffer.clear();
        m_rgb_bitmap = wxBitmap();
    }
    // 内联控件常驻显示：停止拉流后不隐藏，保持可见并露出黑底占位图 + 状态条
    // （例如“打印机断开连接”）。弹窗模式同样保持显示。
    CallAfter([this]() { Refresh(); });
}

void FFRTMPVideoCtrl::showOfflineImmediate(bool show_default_placeholder)
{
    logPanelState("showOfflineImmediate enter");
    // Connection-exit may arrive before account-maintenance. Select the visible
    // default state here so the result does not depend on event ordering.
    if (show_default_placeholder) m_force_default_placeholder = true;
    // 立即（非阻塞）把画面切到“断开连接”并隐藏，同时置停止标志让解码线程开始退出。
    // 只做这些秒级返回的操作，绝不 join 解码线程——供解绑/登出时优先给用户视觉反馈。
    m_running = false;   // 非阻塞：通知解码线程退出，且 OnFrameReady 会据此停止刷新（避免残帧闪回）
    m_paused  = false;   // 断连清除显示暂停态
    sendCameraClose();
    setPlayState(m_force_default_placeholder.load() ? PlayState::Not : PlayState::Disconnected);
    {
        wxCriticalSectionLocker lock(m_frame_cs);
        m_frame_ready = false;
        m_rgb_buffer.clear();
        m_rgb_bitmap = wxBitmap();
    }
    // 账号登出时内联区域必须保留并显示缺省图；普通解绑仍沿用原有隐藏行为。
    if (!m_popup_dlg) {
        if (m_force_default_placeholder.load())
            Show();
        else
            Hide();
    }
    Refresh();
    logPanelState("showOfflineImmediate exit");
}

void FFRTMPVideoCtrl::reapStoppedStream()
{
    // 回收已停止的解码线程：把 join 丢到后台线程，避免在 UI 线程等待其退出造成卡顿。
    // 解码线程在退出前会自行 CloseStream（见 DecoderThreadFunc 末尾），故这里不在 UI 线程 CloseStream。
    // 前置条件：调用方已通过 showOfflineImmediate()（或 StopStream）置好 m_running=false。

    // 已无正在运行的解码线程可回收：直接返回。
    // 关键——避免重复调用（如登出时 onConnectExit 与 onComWanDevMaintain 都会触发回收）时，
    // 第二次进来又 joinStopReaper() 去等后台回收线程，从而把 UI 线程阻塞住。
    if (!m_thread) {
        ffrtmp_ts("reapStoppedStream: 无解码线程可回收, 直接返回");
        return;
    }

    // 回收上一次后台停止（通常已结束，瞬间返回）。
    // 注意：这一步在 UI 线程等待“上一次”的后台回收线程结束——若上一次那条解码线程
    // 卡在 DNS/连接/关闭上还没死，这里会阻塞 UI（是潜在卡顿点，重点看这段耗时）。
    ffrtmp_ts("reapStoppedStream: joinStopReaper BEGIN (UI线程等上一次回收)");
    joinStopReaper();
    ffrtmp_ts("reapStoppedStream: joinStopReaper END");

    if (m_thread->joinable()) {
        std::thread *old = m_thread.release();
        m_stop_reaper = std::thread([old]() {
            ffrtmp_ts("[reaper] old->join BEGIN (等解码线程真正退出)");
            old->join();   // 可能因 DNS/连接/关闭阻塞——但这是在后台线程，不影响 UI。
            ffrtmp_ts("[reaper] old->join END (解码线程已退出)");
            delete old;
        });
    }
    m_thread.reset();
    ffrtmp_ts("reapStoppedStream: 已把解码线程交后台回收, UI线程返回");
}

bool FFRTMPVideoCtrl::IsStreaming() const { return m_running; }

void FFRTMPVideoCtrl::ShowFullScreenPopup()
{
    logPanelState("ShowFullScreenPopup enter");
    if (m_popup_dlg) return;

    // StartStream() has already restored the display preference for this
    // device. In particular, the first open intentionally remains Paused so
    // the play button is available. Do not force it to Loading here by calling
    // resumeStream() before the user clicks Play.

    // 保存内联时的固定尺寸，弹窗关闭后需恢复（否则会残留 640x480 的约束把内联区域撑大）
    m_inline_size = GetSize();

    // 关键：把本控件从内联 sizer 中分离。
    // 否则设备页每次遥测刷新触发的 SingleDeviceState::Layout() 会级联到 m_monitoring_sizer，
    // 而该 sizer 仍持有指向本控件的 item，会对已经 Reparent 到弹窗的控件调用 SetSize(内联尺寸)，
    // 把弹窗里 640x480 的画面强行缩回内联大小 —— 这就是“画面时大时小”的根因。
    if (wxSizer *s = GetContainingSizer()) s->Detach(this);
    logPanelState("ShowFullScreenPopup after Detach");

    // 弹窗尺寸 = 画面区(640x480 = 4:3，与摄像头分辨率一致) + 状态条(FromDIP(30))。
    // 这样画面区正好是 4:3，Cover 铺满不裁边也不留黑边，状态条在下方拼接。
    wxSize videoSize(FromDIP(640), FromDIP(480) + FromDIP(30));
    m_popup_dlg = new wxDialog(wxGetApp().mainframe, wxID_ANY, "");
    m_popup_dlg->SetClientSize(videoSize);
    m_popup_dlg->CenterOnParent();

    Reparent(m_popup_dlg);
    logPanelState("ShowFullScreenPopup after Reparent popup");
    setSize(videoSize);  // min==max==640x510，弹窗内尺寸固定
    Show();              // 控件此前可能处于 Hidden 状态，必须显式显示，否则弹窗内一片黑
    Refresh();

    m_popup_dlg->Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent &event) {
        logPanelState("popup close enter");
        // 记录关闭时的显示状态（暂停/播放），下次打开弹窗时恢复，实现“保持原状”。
        m_popup_last_paused = m_paused;
        // The popup owns the media session. Closing it must release the
        // FFmpeg/network connection instead of decoding invisibly while the
        // printer control panel is in use.
        StopStream();
        // 弹窗专属：关闭后控件必须先从即将销毁的弹窗中 Reparent 出去（否则会随弹窗一起销毁），
        // 归还给内联父窗口后保持 Hidden —— 设备状态面板上不显示任何视频画面。
        // 解码线程已在上方停止；再次打开弹窗时按缓存地址重新建立连接。
        Reparent(m_inline_parent ? m_inline_parent : wxGetApp().mainframe);
        logPanelState("popup close after Reparent inline");
        setSize(m_inline_size);  // 复位为内联占位尺寸（隐藏状态下不占布局空间）
        if (m_force_default_placeholder.load())
            Show();
        else
            Hide();
        if (m_inline_parent && m_inline_parent->GetSizer()) {
            // 恢复与初始内联布局一致的 EXPAND 标志；否则登出关闭弹窗后会留下
            // 一个有 sizer 占位但没有正确铺开的空白区域，直到窗口尺寸变化才重排。
            m_inline_parent->GetSizer()->Add(this, 0, wxALL | wxEXPAND, 0);
            m_inline_parent->Layout();
        }
        logPanelState("popup close after inline Layout");
        m_popup_dlg->Destroy();
        m_popup_dlg = nullptr;
    });

    m_popup_dlg->Show();
}

#ifdef FFRTMP_USE_FFMPEG

// ============================================================================
//  FFmpeg-specific helpers
// ============================================================================

#define FMT_CTX(p)   ((AVFormatContext *)(p))
#define CDC_CTX(p)   ((AVCodecContext *)(p))
#define SWS_CTX(p)   ((SwsContext *)(p))
#define FRM(p)       ((AVFrame *)(p))
#define PCKT(p)      ((AVPacket *)(p))

// Helper to convert FFmpeg error code to string
static std::string av_err_str(int err)
{
    char buf[256];
    av_strerror(err, buf, sizeof(buf));
    return std::string(buf);
}

static void configure_common_demux_options(AVDictionary **opts)
{
    av_dict_set(opts, "max_delay",     "100000", 0); // Maximum demux/decode delay in microseconds.
    av_dict_set(opts, "flush_packets", "1",      0); // Consume live packets without output-side batching.
}

static void configure_rtmp_demux_options(AVDictionary **opts)
{
    // RTMP is a continuous low-latency stream, so keep probing and format
    // buffering small.
    av_dict_set(opts, "probesize",       "32768",   0);
    av_dict_set(opts, "analyzeduration", "1000000", 0);
    av_dict_set(opts, "fflags",          "nobuffer", 0);
    av_dict_set(opts, "rtmp_live",       "live",     0);
    av_dict_set(opts, "rtmp_listen",     "0",        0);
    av_dict_set(opts, "rtmp_buffer",     "100",      0);
}

static void configure_hls_demux_options(AVDictionary **opts)
{
    // HLS needs enough data to inspect MPEG-TS/fMP4 segments. Applying the
    // small RTMP probe and nobuffer policy here can cause missing stream info,
    // unstable startup, or repeated manifest reopen cycles.
    av_dict_set(opts, "probesize",       "262144",  0);
    av_dict_set(opts, "analyzeduration", "2000000", 0);
    av_dict_set(opts, "protocol_whitelist",
                "file,crypto,data,http,https,tcp,tls,hls,applehttp", 0);
    av_dict_set(opts, "reconnect",                    "1", 0);
    av_dict_set(opts, "reconnect_streamed",           "1", 0);
    av_dict_set(opts, "reconnect_at_eof",             "1", 0);
    av_dict_set(opts, "reconnect_on_network_error",   "1", 0);
    av_dict_set(opts, "reconnect_delay_max",          "2", 0);
    av_dict_set(opts, "live_start_index",             "-1", 0);
}

static void configure_http_demux_options(AVDictionary **opts)
{
    // Generic HTTP includes HTTP-FLV and multipart MJPEG. Both are continuous
    // streams and benefit from a small probe and minimal format buffering.
    av_dict_set(opts, "probesize",       "32768",   0);
    av_dict_set(opts, "analyzeduration", "1000000", 0);
    av_dict_set(opts, "fflags",          "nobuffer", 0);
    av_dict_set(opts, "protocol_whitelist", "http,https,tcp,tls", 0);
    av_dict_set(opts, "reconnect",           "1", 0);
    av_dict_set(opts, "reconnect_streamed",  "1", 0);
    av_dict_set(opts, "reconnect_delay_max", "2", 0);
}

void FFRTMPVideoCtrl::DecoderThreadFunc()
{
    ffrtmp_log("Decode thread started");

    auto reset_if_no_frames = [this]() -> bool {
        if (!m_running.load()) return false;

        const int64_t now_ms        = ffrtmp_frame_now_ms();
        const int64_t last_frame_ms = m_last_frame_time_ms.load();
        const int64_t reference_ms  = last_frame_ms > 0 ? last_frame_ms : m_stream_started_at_ms.load();
        if (reference_ms <= 0 || now_ms - reference_ms < 20000) return false;

        ffrtmp_log("Stream produced no frame for 20s; resetting player to initial state");
        m_running = false;
        m_paused = false;
        m_display_paused_pref = true;
        m_ever_played = false;
        m_ever_got_frame = false;
        m_stream_started_at_ms = 0;
        sendCameraClose();
        setPlayState(PlayState::Not);
        return true;
    };

    while (m_running) {
        ffrtmp_log("Attempting FFmpeg stream open (attempt " + std::to_string(m_reconnect_attempts + 1)
                   + ") for " + m_url);

        const bool stream_opened = OpenStream(m_url);
        if (stream_opened) {
            m_reconnect_attempts = 0;
            m_offline_shown = false;
            // 显示暂停时（进设备页默认暂停+缺省图）不改动状态，保持“已暂停”显示。
            if (!m_paused) {
                setPlayState(PlayState::Loading);   // 已连接，等待首帧：视频加载中
            }
            int frame_count = 0;
            while (m_running) {
                // 不做人工帧率限制：av_read_frame 会阻塞等待网络数据，天然按源帧率节流。
                // 若加 30fps 睡眠上限，连接时服务端一次性下发的 GOP 缓存无法被快速消费，
                // 会造成延迟不断累积且永不恢复，这是直播画面高延迟的主因。
                ReadStatus st = ReadAndDecodeOneFrame();
                // Audio/metadata packets may keep the demuxer progressing
                // without producing video, so check every read result.
                if (reset_if_no_frames()) break;
                if (st == ReadStatus::Frame) {
                    ++frame_count;
                    if (frame_count == 1) {
                        ffrtmp_log("First frame decoded successfully!");
                        m_ever_got_frame = true;   // 本会话已出过画面
                    }
                    // 显示暂停时（进设备页默认暂停 / 用户暂停）解码照常，但不置 Playing，
                    // 保持“已暂停 + 缺省图”显示；用户点播放(resumeStream)后才转 Playing。
                    if (!m_paused) {
                        setPlayState(PlayState::Playing);   // 视频播放中（内部去重，不会频繁刷新）
                    }
                    continue;
                }
                if (st == ReadStatus::Progress) {
                    continue;
                }
                if (st == ReadStatus::Eof && m_video_protocol == VideoProtocol::Hls) {
                    // EOF is sticky for the current demuxer. Reopen the HLS
                    // manifest instead of polling the exhausted context for
                    // several seconds and freezing the last displayed frame.
                    ffrtmp_log("HLS input reached EOF, reopening manifest (frames="
                               + std::to_string(frame_count) + ")");
                }
                // 非 HLS 的 EOF 视为真正断流；或读取错误 —— 退出内层循环走重连。
                ffrtmp_log("Read ended (status="
                           + std::string(st == ReadStatus::Eof ? "eof" : "error")
                           + "), frames decoded=" + std::to_string(frame_count));
                break;
            }

            ffrtmp_ts("[decode] CloseStream BEGIN (释放FFmpeg上下文)");
            CloseStream();
            ffrtmp_ts("[decode] CloseStream END");
        } else {
            ffrtmp_err("OpenStream failed for " + m_url);
        }

        if (reset_if_no_frames()) break;
        if (!m_running) {
            ffrtmp_ts("[decode] 观察到 m_running=false, 退出重连循环");
            break;
        }

        m_reconnect_attempts++;

        // 关键：区分“首次加载中(从未出过画面)”与“曾经在播又断了”。
        //   · 从未出过画面(m_ever_got_frame=false)：说明流还没就绪(设备刚开始推流，前几次 404)，
        //     此时无论失败多少次都保持“加载中”，绝不误报“打印机断开连接”——
        //     修复“先显示视频加载、随后又闪断开连接”的问题。
        //   · 曾经出过画面又断流(m_ever_got_frame=true)：达到阈值才判定“打印机断开连接”，
        //     并清掉冻结的最后一帧改黑底占位。
        // 两种情况都继续按退避重连；一旦(重新)出画面会自动转回 Playing。
        // 显示暂停时（进设备页默认暂停+缺省图）不改动状态/不清帧，保持“已暂停”显示，
        // 重连仍在后台进行；用户点播放后由 resumeStream + 解码线程接管状态。
        if (m_paused) {
            // keep paused overlay
        } else if (m_ever_got_frame && m_reconnect_attempts >= m_max_reconnect_attempts) {
            setPlayState(PlayState::Disconnected);
            if (!m_offline_shown) {
                m_offline_shown = true;
                ffrtmp_log("Reconnect threshold reached -> Disconnected (clear frozen frame, keep retrying)");
                CallAfter([this]() {
                    wxCriticalSectionLocker lock(m_frame_cs);
                    m_frame_ready = false;
                    m_rgb_buffer.clear();
                    m_rgb_bitmap = wxBitmap();
                    Refresh();  // OnPaint 会绘制离线黑底占位图，而非露出弹窗白底
                });
            }
        } else {
            setPlayState(PlayState::Loading);   // 首次加载中 / 阈值内重连缓冲：统一显示“加载中”
        }

        // 指数退避，封顶 m_reconnect_delay_max_ms，避免网络长时间不可用时高频重连。
        int delay = std::min(m_reconnect_delay_ms * m_reconnect_attempts, m_reconnect_delay_max_ms);
        ffrtmp_log("Reconnecting in " + std::to_string(delay) + "ms (attempt "
                   + std::to_string(m_reconnect_attempts) + ")");

        for (int i = 0; i < delay / 100 && m_running; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (reset_if_no_frames()) break;
        }
    }

    ffrtmp_log("Decode thread exiting");
    ffrtmp_ts("[decode] 解码线程函数返回 (线程即将结束, reaper 的 join 到此才会返回)");

    CallAfter([this]() {
        // 用户暂停时保留最后一帧冻结显示，不清空画面。
        if (m_paused) { Refresh(); return; }
        wxCriticalSectionLocker lock(m_frame_cs);
        m_frame_ready = false;
        m_rgb_buffer.clear();
        m_rgb_bitmap = wxBitmap();
        Refresh();
    });
}

// steady_clock 当前时间（毫秒），用于阻塞 I/O 硬超时判定。
static int64_t ffrtmp_now_ms()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

int FFRTMPVideoCtrl::interruptCb(void *opaque)
{
    FFRTMPVideoCtrl *self = static_cast<FFRTMPVideoCtrl *>(opaque);
    // 返回非 0 会让 FFmpeg 中止当前阻塞的网络调用。
    if (!self) {
        return 0;
    }
    // 主动停止/暂停：立即中止。
    if (!self->m_running) {
        int expected = 0;
        self->m_io_interrupt_reason.compare_exchange_strong(expected, 1);
        return 1;
    }
    // 阻塞 I/O 超时：设备/服务器失联时 open/read 可能无限阻塞，超过截止点即中止，
    // 让 open/read 返回错误、重连计数推进，最终判定“打印机断开连接”。
    int64_t dl = self->m_io_deadline_ms.load();
    if (dl != 0 && ffrtmp_now_ms() > dl) {
        int expected = 0;
        self->m_io_interrupt_reason.compare_exchange_strong(expected, 2);
        return 1;
    }
    return 0;
}

FFRTMPVideoCtrl::VideoProtocol FFRTMPVideoCtrl::DetectVideoProtocol(const std::string &url)
{
    std::string lower_url = url;
    std::transform(lower_url.begin(), lower_url.end(), lower_url.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower_url.rfind("rtmp://", 0) == 0 || lower_url.rfind("rtmpe://", 0) == 0
        || lower_url.rfind("rtmps://", 0) == 0 || lower_url.rfind("rtmpt://", 0) == 0)
        return VideoProtocol::Rtmp;
    if (lower_url.find(".m3u8") != std::string::npos)
        return VideoProtocol::Hls;
    if (lower_url.rfind("http://", 0) == 0 || lower_url.rfind("https://", 0) == 0)
        return VideoProtocol::Http;
    return VideoProtocol::Unknown;
}

bool FFRTMPVideoCtrl::OpenStream(const std::string &url)
{
    m_video_protocol = DetectVideoProtocol(url);
    switch (m_video_protocol) {
    case VideoProtocol::Rtmp: return OpenRtmpStream(url);
    case VideoProtocol::Hls:  return OpenHlsStream(url);
    case VideoProtocol::Http: return OpenHttpStream(url);
    case VideoProtocol::Unknown:
    default:
        ffrtmp_err("Unsupported video protocol: " + url);
        return false;
    }
}

bool FFRTMPVideoCtrl::OpenRtmpStream(const std::string &url)
{
    return OpenStreamForProtocol(url, VideoProtocol::Rtmp);
}

bool FFRTMPVideoCtrl::OpenHlsStream(const std::string &url)
{
    return OpenStreamForProtocol(url, VideoProtocol::Hls);
}

bool FFRTMPVideoCtrl::OpenHttpStream(const std::string &url)
{
    return OpenStreamForProtocol(url, VideoProtocol::Http);
}

bool FFRTMPVideoCtrl::OpenStreamForProtocol(const std::string &url, VideoProtocol protocol)
{
    if (protocol == VideoProtocol::Hls) {
        m_hls_clock_initialized = false;
        m_hls_first_media_ms = 0;
        m_hls_first_wall_ms = 0;
        m_hls_last_target_ms = 0;
        m_hls_last_media_ms = 0;
        m_hls_estimated_frame_duration_ms = 33;
        m_hls_playback_speed = 1.0;
        m_hls_catch_up_level = 0;
    }
    AVFormatContext *fmt_ctx = avformat_alloc_context();
    AVDictionary    *opts    = nullptr;
    int              ret;

    if (!fmt_ctx) {
        ffrtmp_err("avformat_alloc_context failed");
        return false;
    }
    // 停止/暂停时（m_running=false）立即中止阻塞的网络 I/O，避免 join 卡住 UI。
    // 该回调对 avformat_open_input 与后续 av_read_frame 均生效。
    fmt_ctx->interrupt_callback.callback = &FFRTMPVideoCtrl::interruptCb;
    fmt_ctx->interrupt_callback.opaque   = this;

    configure_common_demux_options(&opts);
    switch (protocol) {
    case VideoProtocol::Rtmp: configure_rtmp_demux_options(&opts); break;
    case VideoProtocol::Hls:  configure_hls_demux_options(&opts);  break;
    case VideoProtocol::Http: configure_http_demux_options(&opts); break;
    case VideoProtocol::Unknown: break;
    }

    const char *protocol_name = protocol == VideoProtocol::Rtmp ? "rtmp"
                                : protocol == VideoProtocol::Hls ? "hls" : "http";
    ffrtmp_log(std::string("avformat_open_input... (") + protocol_name + ")");

    const bool is_mjpeg_url = protocol == VideoProtocol::Http
                              && url.find("action=stream") != std::string::npos;
    if (is_mjpeg_url) {
        const AVInputFormat *mpjpeg_demuxer = av_find_input_format("mpjpeg");
        const AVCodec       *mjpeg_decoder  = avcodec_find_decoder(AV_CODEC_ID_MJPEG);
        ffrtmp_log(std::string("MJPEG capability: mpjpeg demuxer=")
                   + (mpjpeg_demuxer ? "available" : "MISSING")
                   + ", mjpeg decoder=" + (mjpeg_decoder ? "available" : "MISSING"));
    }

    std::string option_log;
    const AVDictionaryEntry *entry = nullptr;
    while ((entry = av_dict_iterate(opts, entry)) != nullptr) {
        if (!option_log.empty()) option_log += ", ";
        option_log += std::string(entry->key) + "=" + entry->value;
    }
    ffrtmp_log("Open diagnostics: url=" + url
               + ", protocol=" + protocol_name
               + ", mjpeg_candidate=" + (is_mjpeg_url ? "true" : "false")
               + ", running=" + (m_running.load() ? "true" : "false")
               + ", timeout_ms=" + std::to_string(m_open_timeout_ms)
               + ", options={" + option_log + "}");

    // 打开阶段设硬超时：服务器失联时 TCP 连接/握手可能长时间阻塞，
    // 超时后 interruptCb 会让 avformat_open_input 返回错误，避免永久卡住。
    const int64_t open_started_ms = ffrtmp_now_ms();
    m_io_interrupt_reason = 0;
    m_io_deadline_ms = open_started_ms + m_open_timeout_ms;
    // 这是解绑时最可能卡住的地方：DNS 解析 / TCP connect 无法被 interruptCb 打断，
    // 会一直阻塞到 OS 超时。看这两条日志的时间戳差即可判断本次 open 阻塞了多久。
    ffrtmp_ts("[decode] avformat_open_input BEGIN (DNS/连接, 可能长阻塞)");
    // Leave the input format unspecified. In particular, LAN
    // */?action=stream endpoints identify themselves as multipart MJPEG via
    // Content-Type, and libavformat must parse that response (including its
    // boundary) before selecting the mpjpeg demuxer.
    ret = avformat_open_input(&fmt_ctx, url.c_str(), nullptr, &opts);
    const int64_t open_elapsed_ms = ffrtmp_now_ms() - open_started_ms;
    ffrtmp_ts(std::string("[decode] avformat_open_input END ret=") + std::to_string(ret));
    m_io_deadline_ms = 0;
    av_dict_free(&opts);

    if (ret < 0) {
        const int interrupt_reason = m_io_interrupt_reason.exchange(0);
        const char *reason_text = interrupt_reason == 1 ? "stream stopped"
                                  : interrupt_reason == 2 ? "I/O deadline exceeded"
                                                          : "not interrupted by callback";
        ffrtmp_err("avformat_open_input failed: " + av_err_str(ret)
                   + " (code=" + std::to_string(ret)
                   + ", elapsed_ms=" + std::to_string(open_elapsed_ms)
                   + ", interrupt_reason=" + reason_text + ")");
        if (fmt_ctx) avformat_free_context(fmt_ctx);
        return false;
    }

    m_io_interrupt_reason = 0;
    ffrtmp_log("avformat_open_input succeeded: elapsed_ms=" + std::to_string(open_elapsed_ms)
               + ", detected_format="
               + (fmt_ctx->iformat && fmt_ctx->iformat->name ? fmt_ctx->iformat->name : "unknown")
               + ", detected_long_name="
               + (fmt_ctx->iformat && fmt_ctx->iformat->long_name ? fmt_ctx->iformat->long_name : "unknown"));

    ffrtmp_log("avformat_find_stream_info...");

    const int64_t stream_info_started_ms = ffrtmp_now_ms();
    m_io_interrupt_reason = 0;
    m_io_deadline_ms = stream_info_started_ms + m_open_timeout_ms;
    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    const int64_t stream_info_elapsed_ms = ffrtmp_now_ms() - stream_info_started_ms;
    m_io_deadline_ms = 0;
    if (ret < 0) {
        const int interrupt_reason = m_io_interrupt_reason.exchange(0);
        const char *reason_text = interrupt_reason == 1 ? "stream stopped"
                                  : interrupt_reason == 2 ? "I/O deadline exceeded"
                                                          : "not interrupted by callback";
        ffrtmp_err("avformat_find_stream_info failed: " + av_err_str(ret)
                   + " (code=" + std::to_string(ret)
                   + ", elapsed_ms=" + std::to_string(stream_info_elapsed_ms)
                   + ", interrupt_reason=" + reason_text + ")");
        avformat_close_input(&fmt_ctx);
        return false;
    }
    m_io_interrupt_reason = 0;
    ffrtmp_log("avformat_find_stream_info succeeded: elapsed_ms="
               + std::to_string(stream_info_elapsed_ms));

    ffrtmp_log("Stream info: nb_streams=" + std::to_string(fmt_ctx->nb_streams)
               + " format=" + (fmt_ctx->iformat ? fmt_ctx->iformat->name : "unknown")
               + " duration=" + std::to_string(fmt_ctx->duration));

    int video_idx = -1;
    AVCodecParameters *codecpar = nullptr;
    for (unsigned i = 0; i < fmt_ctx->nb_streams; ++i) {
        AVCodecParameters *cp = fmt_ctx->streams[i]->codecpar;
        const char *type_str =
            cp->codec_type == AVMEDIA_TYPE_VIDEO   ? "VIDEO"  :
            cp->codec_type == AVMEDIA_TYPE_AUDIO   ? "AUDIO"  :
            cp->codec_type == AVMEDIA_TYPE_SUBTITLE ? "SUBTITLE" : "OTHER";
        ffrtmp_log("stream[" + std::to_string(i) + "] type=" + type_str
                   + " codec_id=" + std::to_string(cp->codec_id)
                   + " codec_name=" + avcodec_get_name(cp->codec_id)
                   + " width=" + std::to_string(cp->width)
                   + " height=" + std::to_string(cp->height));
        if (cp->codec_type == AVMEDIA_TYPE_VIDEO && video_idx < 0) {
            video_idx = i; codecpar = cp;
        }
    }

    if (video_idx < 0 || !codecpar) {
        ffrtmp_err("No video stream found in " + std::to_string(fmt_ctx->nb_streams) + " streams");
        avformat_close_input(&fmt_ctx);
        return false;
    }

    const AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        ffrtmp_err("avcodec_find_decoder failed for codec_id=" + std::to_string(codecpar->codec_id));
        avformat_close_input(&fmt_ctx);
        return false;
    }

    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        ffrtmp_err("avcodec_alloc_context3 failed");
        avformat_close_input(&fmt_ctx);
        return false;
    }

    avcodec_parameters_to_context(codec_ctx, codecpar);
    codec_ctx->thread_count = 2;

    ret = avcodec_open2(codec_ctx, codec, nullptr);
    if (ret < 0) {
        ffrtmp_err("avcodec_open2 failed: " + av_err_str(ret) + " codec=" + codec->name);
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
        return false;
    }

    AVFrame *frame   = av_frame_alloc();
    AVPacket *packet = av_packet_alloc();
    if (!frame || !packet) {
        ffrtmp_err("av_frame/packet_alloc failed");
        av_frame_free(&frame); av_packet_free(&packet);
        avcodec_free_context(&codec_ctx); avformat_close_input(&fmt_ctx);
        return false;
    }

    m_video_width  = codec_ctx->width;
    m_video_height = codec_ctx->height;
    m_format_ctx   = fmt_ctx;
    m_codec_ctx    = codec_ctx;
    m_sws_ctx      = nullptr;
    m_frame        = frame;
    m_packet       = packet;
    m_video_stream_idx = video_idx;

    ffrtmp_log("Stream opened successfully — "
               + std::to_string(m_video_width) + "x" + std::to_string(m_video_height)
               + " codec=" + codec->name);
    return true;
}

void FFRTMPVideoCtrl::CloseStream()
{
    AVFormatContext *fmt_ctx  = FMT_CTX(m_format_ctx);
    AVCodecContext  *codec_ctx = CDC_CTX(m_codec_ctx);
    SwsContext      *sws       = SWS_CTX(m_sws_ctx);
    AVFrame         *frame     = FRM(m_frame);
    AVPacket        *packet    = PCKT(m_packet);

    if (sws)       { sws_freeContext(sws);         m_sws_ctx = nullptr; }
    if (frame)     { av_frame_free(&frame);        m_frame   = nullptr; }
    if (packet)    { av_packet_free(&packet);      m_packet  = nullptr; }
    if (codec_ctx) { avcodec_free_context(&codec_ctx); m_codec_ctx = nullptr; }
    if (fmt_ctx)   { avformat_close_input(&fmt_ctx);   m_format_ctx = nullptr; }

    m_video_stream_idx = -1;
    m_video_protocol = VideoProtocol::Unknown;
}

FFRTMPVideoCtrl::ReadStatus FFRTMPVideoCtrl::ReadAndDecodeOneFrame()
{
    switch (m_video_protocol) {
    case VideoProtocol::Rtmp: return ReadAndDecodeRtmpFrame();
    case VideoProtocol::Hls:  return ReadAndDecodeHlsFrame();
    case VideoProtocol::Http: return ReadAndDecodeHttpFrame();
    case VideoProtocol::Unknown:
    default:                  return ReadStatus::Error;
    }
}

FFRTMPVideoCtrl::ReadStatus FFRTMPVideoCtrl::ReadAndDecodeRtmpFrame()
{
    return ReadAndDecodeCommonFrame();
}

FFRTMPVideoCtrl::ReadStatus FFRTMPVideoCtrl::ReadAndDecodeHlsFrame()
{
    return ReadAndDecodeCommonFrame();
}

FFRTMPVideoCtrl::ReadStatus FFRTMPVideoCtrl::ReadAndDecodeHttpFrame()
{
    return ReadAndDecodeCommonFrame();
}

FFRTMPVideoCtrl::ReadStatus FFRTMPVideoCtrl::ReadAndDecodeCommonFrame()
{
    AVFormatContext *fmt_ctx  = FMT_CTX(m_format_ctx);
    AVCodecContext  *codec_ctx = CDC_CTX(m_codec_ctx);
    AVFrame         *frame    = FRM(m_frame);
    AVPacket        *packet   = PCKT(m_packet);

    // 读取阶段设硬超时：连接已建立但设备停止推流（不再产出帧）时，av_read_frame 可能
    // 长时间阻塞等待数据，超时后 interruptCb 令其返回错误，走重连→最终判定断开。
    m_io_deadline_ms = ffrtmp_now_ms() + m_read_timeout_ms;
    const int64_t read_begin_ms = ffrtmp_now_ms();
    int ret = av_read_frame(fmt_ctx, packet);
    const int64_t read_elapsed_ms = ffrtmp_now_ms() - read_begin_ms;
    m_io_deadline_ms = 0;
    if (ret < 0) {
        if (ret == AVERROR_EOF) {
            // 到达流末尾：HLS 直播这是“到达直播边缘”的常态，交由上层做宽限处理。
            return ReadStatus::Eof;
        }
        ffrtmp_err("av_read_frame failed: " + av_err_str(ret));
        return ReadStatus::Error;
    }

    // 读到了一个包（连接存活即算“有进展”），无论它是否属于视频流。
    if (packet->stream_index != m_video_stream_idx) {
        av_packet_unref(packet); return ReadStatus::Progress;
    }

    const int packet_bytes = packet->size;
    const int64_t decode_begin_ms = ffrtmp_now_ms();
    ret = avcodec_send_packet(codec_ctx, packet);
    av_packet_unref(packet);
    if (ret < 0) { return ReadStatus::Progress; }

    bool decoded_frame = false;
    while (ret >= 0) {
        ret = avcodec_receive_frame(codec_ctx, frame);
        if (ret == AVERROR(EAGAIN)) break;
        if (ret < 0) return ReadStatus::Progress;
        const int64_t decode_elapsed_ms = ffrtmp_now_ms() - decode_begin_ms;

        if (m_video_protocol == VideoProtocol::Hls)
            PaceHlsFrame(frame);
        if (!m_running) {
            av_frame_unref(frame);
            return ReadStatus::Error;
        }

        // Lazy-init or re-create scaler when resolution changes
        SwsContext *sws = SWS_CTX(m_sws_ctx);
        if (!sws || codec_ctx->width != m_video_width || codec_ctx->height != m_video_height) {
            if (sws) { sws_freeContext(sws); m_sws_ctx = nullptr; }
            sws = sws_getContext(
                codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
                codec_ctx->width, codec_ctx->height, AV_PIX_FMT_RGB24,
                SWS_BILINEAR, nullptr, nullptr, nullptr);
            m_sws_ctx = sws;
            m_video_width  = codec_ctx->width;
            m_video_height = codec_ctx->height;
            if (!sws) { av_frame_unref(frame); return ReadStatus::Progress; }
            ffrtmp_log("Scaler (re)created: " + std::to_string(m_video_width) + "x" + std::to_string(m_video_height));
        }

        // Scale into raw RGB buffer (thread-safe, no GDI objects in decoder thread)
        const int fw = m_video_width;
        const int fh = m_video_height;
        int      dst_stride = fw * 3;
        // swscale 输出 RGB24 时，末行可能因 SIMD 一次写满寄存器而多写若干字节；
        // 若目标缓冲按 w*h*3 精确分配，末行溢出会踩坏堆相邻内存，导致随后在
        // CRT(ucrtbase) 中随机崩溃(0xC0000005)。这里额外预留一段安全边距。
        std::vector<uint8_t> buffer((size_t)dst_stride * fh + 64);
        uint8_t *dst[1]     = { buffer.data() };

        const int64_t scale_begin_ms = ffrtmp_now_ms();
        sws_scale(sws, frame->data, frame->linesize, 0, codec_ctx->height,
                  dst, &dst_stride);
        const int64_t scale_elapsed_ms = ffrtmp_now_ms() - scale_begin_ms;

        const int64_t pipeline_elapsed_ms =
            read_elapsed_ms + decode_elapsed_ms + scale_elapsed_ms;
        if (m_video_protocol == VideoProtocol::Http
            && pipeline_elapsed_ms >= g_mjpeg_perf_warn_ms) {
            ffrtmp_mjpeg_perf_log(
                "read_ms=" + std::to_string(read_elapsed_ms)
                + " decode_ms=" + std::to_string(decode_elapsed_ms)
                + " scale_ms=" + std::to_string(scale_elapsed_ms)
                + " pipeline_ms=" + std::to_string(pipeline_elapsed_ms)
                + " packet_bytes=" + std::to_string(packet_bytes)
                + " frame=" + std::to_string(codec_ctx->width) + "x"
                + std::to_string(codec_ctx->height));
        }

        const int64_t decoded_at_ms = ffrtmp_now_ms();
        const int64_t previous_frame_ms = m_last_frame_time_ms.exchange(decoded_at_ms);
        const uint64_t frame_sequence = m_decoded_frame_count.fetch_add(1) + 1;
        {
            // buffer 与其尺寸必须一起在锁内更新，主线程读取时才能保证一致，
            // 否则渲染时用到不匹配的宽高会导致画面尺寸错乱（突然变小）甚至内存越界。
            wxCriticalSectionLocker lock(m_frame_cs);
            m_rgb_buffer  = std::move(buffer);
            m_buf_width   = fw;
            m_buf_height  = fh;
            m_buf_frame_sequence = frame_sequence;
            m_frame_ready = true;
            decoded_frame = true;
        }

        // Always overwrite the shared RGB slot with the newest frame, but
        // queue at most one UI callback. This prevents UI event backlog from
        // becoming accumulated live latency.
        const bool post_ui_callback = !m_paused && m_pending_frame_callbacks.exchange(1) == 0;
        if (post_ui_callback) m_ui_callback_posted_count.fetch_add(1);
        const int pending = m_pending_frame_callbacks.load();
        if (previous_frame_ms > 0 && decoded_at_ms - previous_frame_ms >= g_ffrtmp_frame_gap_warn_ms) {
            ffrtmp_frame_log("decode-gap seq=" + std::to_string(frame_sequence)
                             + " gapMs=" + std::to_string(decoded_at_ms - previous_frame_ms)
                             + " pendingUi=" + std::to_string(pending));
        }
        const int64_t previous_summary_ms = m_last_frame_diag_summary_ms.load();
        if (previous_summary_ms == 0 || decoded_at_ms - previous_summary_ms >= 1000) {
            m_last_frame_diag_summary_ms = decoded_at_ms;
            ffrtmp_frame_log("summary decoded=" + std::to_string(m_decoded_frame_count.load())
                             + " uiPosted=" + std::to_string(m_ui_callback_posted_count.load())
                             + " uiDone=" + std::to_string(m_ui_callback_completed_count.load())
                             + " bitmap=" + std::to_string(m_bitmap_frame_count.load())
                             + " painted=" + std::to_string(m_painted_frame_count.load())
                             + " pendingUi=" + std::to_string(pending)
                             + " size=" + std::to_string(fw) + "x" + std::to_string(fh));
        }
        if (post_ui_callback)
            CallAfter([this, frame_sequence, decoded_at_ms]() { OnFrameReady(frame_sequence, decoded_at_ms); });
        av_frame_unref(frame);
    }
    return decoded_frame ? ReadStatus::Frame : ReadStatus::Progress;
}

void FFRTMPVideoCtrl::PaceHlsFrame(void *opaque_frame)
{
    AVFrame *frame = static_cast<AVFrame *>(opaque_frame);
    AVFormatContext *fmt_ctx = FMT_CTX(m_format_ctx);
    if (!frame || !fmt_ctx || m_video_stream_idx < 0
        || m_video_stream_idx >= static_cast<int>(fmt_ctx->nb_streams))
        return;

    AVStream *stream = fmt_ctx->streams[m_video_stream_idx];
    int64_t pts = frame->best_effort_timestamp;
    if (pts == AV_NOPTS_VALUE)
        pts = frame->pts;

    const int64_t now_ms = ffrtmp_now_ms();
    int64_t media_ms = 0;
    bool has_timestamp = pts != AV_NOPTS_VALUE;
    if (has_timestamp)
        media_ms = av_rescale_q(pts, stream->time_base, AVRational{1, 1000});

    auto reset_clock = [this, now_ms, media_ms, has_timestamp](const char *reason) {
        m_hls_clock_initialized = true;
        m_hls_first_media_ms = has_timestamp ? media_ms : 0;
        m_hls_first_wall_ms = now_ms;
        m_hls_last_target_ms = now_ms;
        m_hls_last_media_ms = has_timestamp ? media_ms : 0;
        m_hls_playback_speed = 1.0;
        m_hls_catch_up_level = 0;
        ffrtmp_frame_log(std::string("hls-clock-reset reason=") + reason
                         + " ptsMs=" + std::to_string(m_hls_first_media_ms));
    };

    if (!m_hls_clock_initialized) {
        reset_clock("start");
        return;
    }

    int64_t source_target_ms;
    int64_t frame_duration_ms = m_hls_estimated_frame_duration_ms;
    if (has_timestamp) {
        const int64_t media_delta_ms = media_ms - m_hls_last_media_ms;
        // HLS discontinuities and reconnects commonly reset PTS. Rebase the
        // local clock instead of sleeping for a forward jump or bursting
        // through frames after a backward jump.
        if (media_delta_ms < -500) {
            reset_clock("pts-backward");
            return;
        }
        if (media_delta_ms > 10000) {
            reset_clock("pts-forward");
            return;
        }

        if (media_delta_ms > 0 && media_delta_ms < 1000)
            m_hls_estimated_frame_duration_ms = media_delta_ms;
        if (media_delta_ms > 0 && media_delta_ms < 1000)
            frame_duration_ms = media_delta_ms;
        m_hls_last_media_ms = media_ms;
        source_target_ms = m_hls_first_wall_ms + media_ms - m_hls_first_media_ms;
    } else {
        AVRational rate = av_guess_frame_rate(fmt_ctx, stream, frame);
        if (rate.num > 0 && rate.den > 0)
            m_hls_estimated_frame_duration_ms = std::max<int64_t>(
                1, av_rescale_q(1, av_inv_q(rate), AVRational{1, 1000}));
        frame_duration_ms = m_hls_estimated_frame_duration_ms;
        source_target_ms = m_hls_last_target_ms + frame_duration_ms;
    }

    // Smooth catch-up without dropping frames. A modest playback speed-up
    // shortens presentation intervals until the source clock catches up, then
    // steps back down to normal speed. Hysteresis avoids oscillation near a
    // threshold and the 1.15 cap keeps motion visually continuous.
    const int64_t lateness_ms = now_ms - source_target_ms;
    int catch_up_level;
    if (lateness_ms > 2000)
        catch_up_level = 3;
    else if (lateness_ms > 1000)
        catch_up_level = 2;
    else if (lateness_ms > 400)
        catch_up_level = 1;
    else if (lateness_ms < 200)
        catch_up_level = 0;
    else
        catch_up_level = m_hls_catch_up_level;

    static constexpr double catch_up_speeds[] = {1.0, 1.05, 1.10, 1.15};
    if (catch_up_level != m_hls_catch_up_level) {
        m_hls_catch_up_level = catch_up_level;
        ffrtmp_frame_log("hls-catch-up-level=" + std::to_string(catch_up_level)
                         + " lateMs=" + std::to_string(lateness_ms));
    }
    const double desired_speed = catch_up_speeds[m_hls_catch_up_level];
    constexpr double speed_step_per_frame = 0.002;
    if (m_hls_playback_speed < desired_speed)
        m_hls_playback_speed = std::min(desired_speed, m_hls_playback_speed + speed_step_per_frame);
    else if (m_hls_playback_speed > desired_speed)
        m_hls_playback_speed = std::max(desired_speed, m_hls_playback_speed - speed_step_per_frame);

    const int64_t paced_duration_ms = std::max<int64_t>(
        1, static_cast<int64_t>(frame_duration_ms / m_hls_playback_speed + 0.5));
    // Never schedule into the past: doing so would release a burst of frames
    // and recreate the visible stutter that frame-dropping catch-up caused.
    const int64_t target_ms = std::max(m_hls_last_target_ms, now_ms) + paced_duration_ms;

    m_hls_last_target_ms = target_ms;
    while (m_running) {
        const int64_t remaining_ms = target_ms - ffrtmp_now_ms();
        if (remaining_ms <= 0)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(std::min<int64_t>(remaining_ms, 10)));
    }
}

#else  // !FFRTMP_USE_FFMPEG

// Stubs when FFmpeg is not available
void   FFRTMPVideoCtrl::DecoderThreadFunc()                           {}
FFRTMPVideoCtrl::VideoProtocol FFRTMPVideoCtrl::DetectVideoProtocol(const std::string &) { return VideoProtocol::Unknown; }
bool   FFRTMPVideoCtrl::OpenStream(const std::string &)              { return false; }
bool   FFRTMPVideoCtrl::OpenRtmpStream(const std::string &)          { return false; }
bool   FFRTMPVideoCtrl::OpenHlsStream(const std::string &)           { return false; }
bool   FFRTMPVideoCtrl::OpenHttpStream(const std::string &)          { return false; }
bool   FFRTMPVideoCtrl::OpenStreamForProtocol(const std::string &, VideoProtocol) { return false; }
void   FFRTMPVideoCtrl::CloseStream()                                {}
FFRTMPVideoCtrl::ReadStatus FFRTMPVideoCtrl::ReadAndDecodeOneFrame() { return ReadStatus::Error; }
FFRTMPVideoCtrl::ReadStatus FFRTMPVideoCtrl::ReadAndDecodeRtmpFrame() { return ReadStatus::Error; }
FFRTMPVideoCtrl::ReadStatus FFRTMPVideoCtrl::ReadAndDecodeHlsFrame() { return ReadStatus::Error; }
FFRTMPVideoCtrl::ReadStatus FFRTMPVideoCtrl::ReadAndDecodeHttpFrame() { return ReadStatus::Error; }
FFRTMPVideoCtrl::ReadStatus FFRTMPVideoCtrl::ReadAndDecodeCommonFrame() { return ReadStatus::Error; }
void FFRTMPVideoCtrl::PaceHlsFrame(void *) {}

#endif // FFRTMP_USE_FFMPEG

// ============================================================================
//  PrinterCameraPanel compatibility API (always compiled)
// ============================================================================

void FFRTMPVideoCtrl::setSize(wxSize size)
{
    logPanelState("setSize before");
    SetSize(size);
    SetMinSize(size);
    logPanelState("setSize after");
}

void FFRTMPVideoCtrl::setDisplayMode(DisplayMode mode)
{
    if (m_display_mode == mode) return;
    m_display_mode = mode;
    CallAfter([this]() { Refresh(); });
}

void FFRTMPVideoCtrl::setCurComId(com_id_t comId)
{
    if (m_curComId == comId) return;

    // Device detail may not be populated yet at switch time. Clear the old
    // device's stream and wait for the new URL instead of treating the camera
    // field's initialization value as a final capability result.
    StopStream();
    m_curComId = comId;
    bool valid = false;
    const com_dev_data_t &data = MultiComMgr::inst()->devData(comId, &valid);
    m_is_wan_stream = valid && data.connectMode == COM_CONNECT_WAN;
    m_url.clear();
    m_force_default_placeholder = false;
    m_paused = false;
    setPlayState(PlayState::Not);
}

void FFRTMPVideoCtrl::setCameraAvailable(bool available)
{
    const bool was_available = m_camera_available.exchange(available);
    if (was_available != available) {
        BOOST_LOG_TRIVIAL(error) << "[CAMERA_CAPABILITY] comId=" << m_curComId
                                << ", available: " << was_available << " -> " << available;
    }
    if (!available) {
        if (was_available) StopStream();
        m_url.clear();
        m_paused = false;
        setPlayState(PlayState::NoCamera);
        return;
    }
    if (!was_available && m_play_state.load() == PlayState::NoCamera) {
        setPlayState(PlayState::Not);
    }
}

void FFRTMPVideoCtrl::sendCameraOpen(com_id_t comId)
{
    if (comId == ComInvalidId) {
        ffrtmp_err("sendCameraOpen skipped: invalid comId");
        return;
    }

    if (m_open_camera_com_id != ComInvalidId) {
        ffrtmp_log("sendCameraOpen skipped: camera session already open, comId="
                   + std::to_string(m_open_camera_com_id));
        return;
    }
    // 无条件下发，与 PrinterCameraPanel::onScriptMessage 一致：
    //   · WAN → ComCameraStreamCtrl::exec 经 ComWanConn 把 "open" 发到云端，
    //           云端通知打印机启动摄像头并推流；
    //   · LAN → exec 返回 COM_UNSUPPORTED，自动忽略，无副作用。
    // putCommand 内部对 WAN 未在线的情况会拒绝并记录日志，无需在此判断。
    bool ok = MultiComMgr::inst()->putCommand(comId, new ComCameraStreamCtrl("open"));
    if (!ok) {
        // 仅在失败时记录（通常意味着 WAN 未在线）。
        ffrtmp_err("[CAMERA_STREAM_SESSION] OPENSTREAM rejected, comId="
                   + std::to_string(comId));
    } else {
        m_open_camera_com_id = comId;
        m_active_camera_session_id = ++m_camera_session_sequence;
        ffrtmp_log("[CAMERA_STREAM_SESSION] OPENSTREAM queued, sessionId="
                   + std::to_string(m_active_camera_session_id)
                   + ", comId=" + std::to_string(comId));
    }
}

void FFRTMPVideoCtrl::sendCameraClose()
{
    const com_id_t com_id = m_open_camera_com_id;
    if (com_id == ComInvalidId) return;
    const uint64_t session_id = m_active_camera_session_id;

    // Clear first so repeated stop/offline/destructor paths cannot enqueue a
    // second close for the same media session.
    m_open_camera_com_id = ComInvalidId;
    m_active_camera_session_id = 0;
    if (!MultiComMgr::inst()->putCommand(com_id, new ComCameraStreamCtrl("close"))) {
        ffrtmp_err("[CAMERA_STREAM_SESSION] CLOSESTREAM rejected, sessionId="
                   + std::to_string(session_id)
                   + ", comId=" + std::to_string(com_id));
    } else {
        ffrtmp_log("[CAMERA_STREAM_SESSION] CLOSESTREAM queued, sessionId="
                   + std::to_string(session_id)
                   + ", comId=" + std::to_string(com_id));
    }
}

void FFRTMPVideoCtrl::setStreamUrl(const std::string &streamUrl)
{
    ffrtmp_log("setStreamUrl: popup=" + std::string(m_popup_dlg ? "open" : "closed")
               + ", running=" + std::string(m_running ? "true" : "false")
               + ", urlEmpty=" + std::string(streamUrl.empty() ? "true" : "false"));
    // A non-empty stream URL is authoritative evidence that this device has a
    // camera. The capability field may still contain its initialization value
    // when setCurComId() runs, so do not leave a stale NoCamera state behind.
    if (!m_camera_available.load()) {
        m_url.clear();
        setPlayState(PlayState::NoCamera);
        return;
    }
    if (!streamUrl.empty() && m_play_state.load() == PlayState::NoCamera) {
        setPlayState(PlayState::Not);
    }

    // URL updates never start a printer/media session. Playback is owned by an
    // explicit click on the Play button.
    m_url = streamUrl;
}

void FFRTMPVideoCtrl::setStartPausedPreference(bool paused)
{
    // 下次 StartStream 的显示暂停偏好。切设备时置 true，使每台设备进入都默认暂停(与首次一致)。
    m_display_paused_pref = paused;
    // 进设备页/切设备视为“首次进入”：复位播放标志，使默认暂停不显示“视频已暂停”文字。
    m_ever_played = false;
}

void FFRTMPVideoCtrl::setOffline(bool show_default_placeholder)
{
    logPanelState("setOffline enter");
    ffrtmp_log("setOffline called");

    m_force_default_placeholder = show_default_placeholder;
    m_paused = false;                          // 断连清除显示暂停态
    StopStream();
    setPlayState(show_default_placeholder ? PlayState::Not : PlayState::Disconnected);
    CallAfter([this]() {
        logPanelState("setOffline CallAfter before clear/show");
        wxCriticalSectionLocker lock(m_frame_cs);
        m_frame_ready = false;
        m_rgb_buffer.clear();
        m_rgb_bitmap = wxBitmap();
        // 登出可能与连接退出事件交错；这里再次恢复可见性，确保延迟回调后仍显示缺省图。
        if (m_force_default_placeholder.load() && !m_popup_dlg) {
            Show();
            if (wxSizer *sizer = GetContainingSizer()) sizer->Layout();
        }
        Refresh();
        logPanelState("setOffline CallAfter after Layout/Refresh");
    });
}

void FFRTMPVideoCtrl::handleDeviceUnbound()
{
    ffrtmp_log("handleDeviceUnbound ENTER: comId=" + std::to_string(m_curComId)
               + ", running=" + std::string(m_running ? "true" : "false")
               + ", urlEmpty=" + std::string(m_url.empty() ? "true" : "false"));
    // Give immediate UI feedback without joining the decoder thread here.
    // reapStoppedStream() is scheduled by the caller on the following UI tick.
    m_running = false;
    sendCameraClose();
    m_paused = false;
    m_display_paused_pref = true;
    m_ever_played = false;
    m_camera_available = false;
    m_curComId = ComInvalidId;
    m_url.clear();
    m_force_default_placeholder = false;
    setPlayState(PlayState::Disconnected);

    {
        wxCriticalSectionLocker lock(m_frame_cs);
        m_frame_ready = false;
        m_rgb_buffer.clear();
        m_rgb_bitmap = wxBitmap();
    }

    // Keep whichever camera surface is currently in use (inline or popup)
    // visible and repaint it with the disconnected placeholder/status.
    if (!m_popup_dlg) Show();
    Refresh();
    ffrtmp_log("handleDeviceUnbound EXIT: state=Disconnected, source cleared");
}

void FFRTMPVideoCtrl::showPopup()
{
    ffrtmp_log("showPopup ENTER: popup=" + std::string(m_popup_dlg ? "open" : "closed")
               + ", running=" + std::string(m_running ? "true" : "false")
               + ", urlEmpty=" + std::string(m_url.empty() ? "true" : "false")
               + ", playState=" + std::to_string(static_cast<int>(m_play_state.load())));
    if (!m_popup_dlg && m_url.empty()) {
        bool valid = false;
        const com_dev_data_t &data = MultiComMgr::inst()->devData(m_curComId, &valid);
        if (valid && data.devDetail != nullptr && data.devDetail->camera != 1) {
            setPlayState(PlayState::NoCamera);
        }
    }
    ShowFullScreenPopup();
    ffrtmp_log("showPopup EXIT: popup=" + std::string(m_popup_dlg ? "open" : "closed")
               + ", running=" + std::string(m_running ? "true" : "false")
               + ", paused=" + std::string(m_paused ? "true" : "false")
               + ", playState=" + std::to_string(static_cast<int>(m_play_state.load())));
}

void FFRTMPVideoCtrl::closePopup()
{
    // 主动关闭视频弹窗（若已打开）。走 wxEVT_CLOSE_WINDOW 的关闭处理：
    // 把控件 Reparent 回内联父窗口、Hide，并 Destroy 弹窗、置空 m_popup_dlg。
    if (m_popup_dlg) {
        m_popup_dlg->Close();
    }
}

// ============================================================================
//  Main-Thread Callbacks (always compiled)
// ============================================================================

void FFRTMPVideoCtrl::OnFrameReady(uint64_t posted_frame_sequence, int64_t posted_at_ms)
{
    m_pending_frame_callbacks.fetch_sub(1);
    m_ui_callback_completed_count.fetch_add(1);
    const int64_t ui_now_ms = ffrtmp_frame_now_ms();
    const int64_t callback_latency_ms = ui_now_ms - posted_at_ms;
    // Called on main thread via CallAfter — safe to create GDI objects here
    // 已停止（如解绑/登出触发的异步停流）：解码线程退出前可能仍有若干帧回调在队列里，
    // 此时不再更新显示位图/触发重绘，避免在页面拆除期间做无谓渲染、加重卡顿。
    if (!m_running) {
        if (callback_latency_ms >= g_ffrtmp_ui_latency_warn_ms)
            ffrtmp_frame_log("ui-stale-callback postedSeq=" + std::to_string(posted_frame_sequence)
                             + " latencyMs=" + std::to_string(callback_latency_ms));
        return;
    }
    // 显示暂停：后台解码仍在把最新帧写入 m_rgb_buffer，但这里不更新显示位图，
    // 画面冻结在暂停时刻（暂停与拉流解耦）。
    if (m_paused) {
        return;
    }
    wxCriticalSectionLocker lock(m_frame_cs);
    // 缓冲尾部含安全边距，可能比有效像素大，故用 >= 判断，并只拷贝有效像素 w*h*3。
    const size_t need = (size_t)m_buf_width * m_buf_height * 3;
    if (m_frame_ready && m_buf_width > 0 && m_buf_height > 0 && m_rgb_buffer.size() >= need) {
        const uint64_t source_sequence = m_buf_frame_sequence;
        wxImage img(m_buf_width, m_buf_height, false);
        memcpy(img.GetData(), m_rgb_buffer.data(), need);
        m_rgb_bitmap = wxBitmap(img);
        const uint64_t skipped = source_sequence > m_bitmap_frame_sequence + 1
                                     ? source_sequence - m_bitmap_frame_sequence - 1
                                     : 0;
        m_bitmap_frame_sequence = source_sequence;
        m_bitmap_frame_count.fetch_add(1);
        const int64_t ui_gap_ms = m_last_ui_frame_time_ms == 0 ? 0 : ui_now_ms - m_last_ui_frame_time_ms;
        m_last_ui_frame_time_ms = ui_now_ms;
        if (callback_latency_ms >= g_ffrtmp_ui_latency_warn_ms || skipped > 0 || ui_gap_ms >= g_ffrtmp_frame_gap_warn_ms) {
            ffrtmp_frame_log("ui-frame postedSeq=" + std::to_string(posted_frame_sequence)
                             + " sourceSeq=" + std::to_string(source_sequence)
                             + " skippedDecoded=" + std::to_string(skipped)
                             + " callbackLatencyMs=" + std::to_string(callback_latency_ms)
                             + " uiGapMs=" + std::to_string(ui_gap_ms)
                             + " pendingUi=" + std::to_string(m_pending_frame_callbacks.load()));
        }
    }
    Refresh(false);
}

void FFRTMPVideoCtrl::OnPaint(wxPaintEvent & /*event*/)
{
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(*wxBLACK_BRUSH);
    dc.Clear();

    wxCriticalSectionLocker lock(m_frame_cs);
    wxSize client = GetClientSize();

    // 整个控件 = [上：摄像头画面区] + [下：状态条(固定高)]，两者纵向拼接、不重叠。
    // 画面只画在 videoH 高度内，底部 barH 留给状态条，避免状态条遮挡视频。
    const int barH   = FromDIP(30);
    const int videoH = std::max(1, client.y - barH);

    if (m_frame_ready && m_rgb_bitmap.IsOk() && client.x > 0 && videoH > 0) {
        const int64_t paint_now_ms = ffrtmp_frame_now_ms();
        if (m_bitmap_frame_sequence != m_last_painted_frame_sequence) {
            const uint64_t skipped = m_bitmap_frame_sequence > m_last_painted_frame_sequence + 1
                                         ? m_bitmap_frame_sequence - m_last_painted_frame_sequence - 1
                                         : 0;
            const int64_t paint_gap_ms = m_last_paint_frame_time_ms == 0 ? 0 : paint_now_ms - m_last_paint_frame_time_ms;
            m_last_painted_frame_sequence = m_bitmap_frame_sequence;
            m_last_paint_frame_time_ms = paint_now_ms;
            m_painted_frame_count.fetch_add(1);
            if (skipped > 0 || paint_gap_ms >= g_ffrtmp_frame_gap_warn_ms) {
                ffrtmp_frame_log("paint-frame seq=" + std::to_string(m_bitmap_frame_sequence)
                                 + " skippedBitmap=" + std::to_string(skipped)
                                 + " paintGapMs=" + std::to_string(paint_gap_ms)
                                 + " pendingUi=" + std::to_string(m_pending_frame_callbacks.load()));
            }
        }
        wxMemoryDC memDC;
        memDC.SelectObject(m_rgb_bitmap);

        // 以位图自身尺寸为准，而不是解码线程异步写入的 m_video_width/height，
        // 避免两者不一致导致画面尺寸计算错误（突然变小）。
        const int vw = m_rgb_bitmap.GetWidth();
        const int vh = m_rgb_bitmap.GetHeight();

        if (m_display_mode == DisplayMode::Cover) {
            // ---- 铺满(裁边) / cover ----
            // 等比缩放铺满“画面区”(client.x × videoH)，裁剪源图保持画面比例，超出部分裁掉，不拉伸变形。
            const double client_aspect = (double)client.x / (double)videoH;
            const double video_aspect  = (double)vw / (double)vh;

            int src_x, src_y, src_w, src_h;
            if (video_aspect > client_aspect) {
                // 视频比画面区更宽 —— 裁掉左右
                src_h = vh;
                src_w = std::max(1, (int)(vh * client_aspect + 0.5));
                src_x = (vw - src_w) / 2;
                src_y = 0;
            } else {
                // 视频比画面区更高（或等宽）—— 裁掉上下
                src_w = vw;
                src_h = std::max(1, (int)(vw / client_aspect + 0.5));
                src_x = 0;
                src_y = (vh - src_h) / 2;
            }

            dc.StretchBlit(0, 0, client.x, videoH,
                           &memDC, src_x, src_y, src_w, src_h);
        } else {
            // ---- 完整显示(留黑边) / fit ----
            double scale = std::min((double)client.x / vw,
                                    (double)videoH / vh);
            int w = std::max(1, (int)(vw * scale));
            int h = std::max(1, (int)(vh * scale));
            int x = (client.x - w) / 2;
            int y = (videoH - h) / 2;   // 居中于“画面区”，不进入底部状态条
            dc.StretchBlit(x, y, w, h, &memDC, 0, 0, vw, vh);
        }

        memDC.SelectObject(wxNullBitmap);
    } else if (client.x > 0 && videoH > 0) {
        // 无可显示视频帧时统一显示缺省图，包括设备/视频流断开连接的状态。
        // 缺省图未加载成功时回退为黑底占位。同样只画在“画面区”(videoH)内，不进入状态条。
        // Use the default camera image whenever no video frame is available,
        // including after a connected device/stream disconnects. The generated
        // black bitmap remains only as a fallback if the resource cannot load.
        wxBitmap &bg = m_placeholder_bitmap.IsOk() ? m_placeholder_bitmap : m_offline_bitmap;
        if (bg.IsOk()) {
            const int w = bg.GetWidth();
            const int h = bg.GetHeight();
            // Cover：等比铺满“画面区”、裁掉超出部分（与视频一致），保持比例不变、不留黑边。
            const double client_aspect = (double)client.x / (double)videoH;
            const double img_aspect    = (double)w / (double)h;
            int src_x, src_y, src_w, src_h;
            if (img_aspect > client_aspect) {
                src_h = h;
                src_w = std::max(1, (int)(h * client_aspect + 0.5));
                src_x = (w - src_w) / 2;
                src_y = 0;
            } else {
                src_w = w;
                src_h = std::max(1, (int)(w / client_aspect + 0.5));
                src_x = 0;
                src_y = (h - src_h) / 2;
            }
            wxMemoryDC memDC;
            memDC.SelectObject(bg);
            dc.StretchBlit(0, 0, client.x, videoH, &memDC, src_x, src_y, src_w, src_h);
            memDC.SelectObject(wxNullBitmap);
        }
    }

    // 底部状态条（播放/暂停 + 状态文字），覆盖在画面之上。
    drawOverlayBar(dc);
}

void FFRTMPVideoCtrl::OnSize(wxSizeEvent & /*event*/)
{
    logPanelState("OnSize enter");
    // 内联模式：控件宽度随左栏横向拉伸，这里让总高 = 画面区(宽/摄像头宽高比) + 状态条高，
    // 随宽度动态调整，使"画面区"正好等于摄像头宽高比 —— 既不裁也不留黑边，画面与状态条纵向拼接。
    // 弹窗模式尺寸固定，不参与。设备遥测刷新不改变左栏宽度，故 desiredH 稳定、不会抖动。
    if (!m_popup_dlg && !m_in_on_size && m_camera_aspect > 0.0) {
        const int w = GetSize().GetWidth();
        if (w > 0) {
            const int barH     = FromDIP(30);
            const int desiredH = (int)(w / m_camera_aspect + 0.5) + barH;
            if (GetSize().GetHeight() != desiredH) {
                m_in_on_size = true;
                // 只约束高度（宽度留 -1 不限制，保持横向 EXPAND）。
                SetMinSize(wxSize(-1, desiredH));
                SetMaxSize(wxSize(-1, desiredH));
                // 关键：把高度变化“向上传播”到整页，使摄像头区变大时下方“信息与控制”
                // 窗口被顶下去，并更新最外层滚动窗的虚拟尺寸（否则区域不增大、下方窗口不动）。
                for (wxWindow *anc = GetParent(); anc != nullptr; anc = anc->GetParent()) {
                    anc->Layout();
                    if (wxScrolledWindow *sw = dynamic_cast<wxScrolledWindow *>(anc)) {
                        sw->FitInside();   // 更新滚动窗虚拟尺寸，内容随之下移/可滚动
                        break;
                    }
                    if (anc->IsTopLevel()) {
                        break;
                    }
                }
                m_in_on_size = false;
            }
        }
    }
    Refresh();
    logPanelState("OnSize exit");
}

// ============================================================================
//  播放器覆盖层：底部状态条（左下角播放/暂停，右下角状态文字）
// ============================================================================

void FFRTMPVideoCtrl::setPlayState(PlayState s)
{
    // 登出/连接退出已要求显示初始不可用状态后，解码线程可能仍从一次正在
    // 进行的阻塞读取中返回，并尝试写入 Loading/Playing。在线程完全退出前
    // 拒绝这些迟到状态，避免右下角重新显示“视频播放中”。StartStream()
    // 会先清除 m_force_default_placeholder，正常起流不受影响。
    if (m_force_default_placeholder.load() && s != PlayState::Not) {
        return;
    }
    // Once the selected device has no valid camera capability (notably after
    // unbind), reject late decoder callbacks that try to restore active states.
    if (!m_camera_available.load()
        && (s == PlayState::Initializing || s == PlayState::Loading
            || s == PlayState::Playing || s == PlayState::Paused)) {
        return;
    }
    // 仅在状态变化时刷新，避免解码线程每帧 setPlayState(Playing) 造成刷新风暴。
    PlayState old = m_play_state.exchange(s);
    if (old == s) {
        return;
    }
    ffrtmp_log("play state " + std::to_string(static_cast<int>(old)) + " -> "
               + std::to_string(static_cast<int>(s))
               + ", running=" + std::string(m_running ? "true" : "false")
               + ", paused=" + std::string(m_paused ? "true" : "false")
               + ", frameReady=" + std::string(m_frame_ready ? "true" : "false"));
    CallAfter([this]() { Refresh(false); });
}

wxString FFRTMPVideoCtrl::statusText() const
{
    // A removed/unbound device may still retain the previous stream type until
    // the decoder is fully reaped. It is not a reconnecting WAN stream, so do
    // not expose a stale "Loading video..." status in the no-device state.
    if (m_curComId == ComInvalidId)
        return wxString();

    // 多语言：msgid 用英文，各语言译文见 localization/flashforge/{lang}/flashforge_{lang}.po。
    switch (m_play_state.load()) {
    case PlayState::Initializing: return _L("Initializing camera...");
    case PlayState::Loading:      return _L("Loading video...");
    case PlayState::Playing:      return _L("Video is playing");
    // 首次进页面的默认暂停（从未主动播放过）不显示“视频已暂停”文字，仅露出缺省图 + 播放按钮；
    // 用户主动播放过后再暂停才显示。
    case PlayState::Paused:       return m_ever_played.load() ? _L("Video is paused") : wxString();
    case PlayState::Disconnected:
        return m_is_wan_stream.load() ? _L("Loading video...") : _L("Printer disconnected");
    case PlayState::NoCamera:     return _L("No camera detected");
    // 初始不可用状态：左侧显示不可播放图标，右侧不显示任何状态文字。
    case PlayState::Not:          return wxString();
    default:                      return wxString();
    }
}

bool FFRTMPVideoCtrl::playButtonAvailable() const
{
    // A cached URL in the initial state means the stream is ready to be
    // started by the user's first click. This is the inline control's actual
    // entry point because the legacy camera-toolbar popup binding is disabled.
    PlayState s = m_play_state.load();
    if (!m_camera_available.load()) return false;
    return s == PlayState::Playing || s == PlayState::Paused
        || (s == PlayState::Not && !m_url.empty());
}

wxRect FFRTMPVideoCtrl::playButtonRect()
{
    // 左下角一块点击区域（比图标略大，便于点击）。
    wxSize client = GetClientSize();
    const int barH = FromDIP(30);
    return wxRect(0, client.y - barH, FromDIP(44), barH);
}

void FFRTMPVideoCtrl::drawOverlayBar(wxDC &dc)
{
    wxSize client = GetClientSize();
    if (client.x <= 0 || client.y <= 0) {
        return;
    }

    const int barH = FromDIP(45);
    const int barY = client.y - barH;

    // 底部状态条背景（深色）
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(*wxWHITE));
    dc.DrawRectangle(0, barY, client.x, barH);

    // 左下角 播放/暂停 图标：暂停/断连时显示“播放三角”，播放中显示“暂停双竖条”。
    auto state = m_play_state.load();
    const int  icon = FromDIP(29);
    const int  ix   = FromDIP(20);
    const int  iy   = barY + (barH - icon) / 2;
    std::string selectIcon = "";
    if (!playButtonAvailable()) {
        selectIcon = "offline";
    } else if (m_paused || m_play_state.load() == PlayState::Not) {
        selectIcon = "play";
    } else {
        selectIcon = "pause";
    }
    dc.DrawBitmap(m_playIconMap[selectIcon].bmp(), ix, iy);
    // 右下角 状态文字
    wxString txt = statusText();
    dc.SetFont(Label::Body_13);
    dc.SetTextForeground(wxColour("#666666"));
    wxSize ts = dc.GetTextExtent(txt);
    dc.DrawText(txt, client.x - ts.x - FromDIP(12), barY + (barH - ts.y) / 2);
}

void FFRTMPVideoCtrl::pauseStream()
{
    if (m_paused) {
        return;
    }
    // 暂停与拉流解耦：后台解码/拉流继续运行，只是“冻结显示”——
    // OnFrameReady 不再把最新帧刷到显示位图，画面停在当前帧。
    ffrtmp_log("pauseStream (display-only, decode keeps running)");
    m_paused = true;
    m_display_paused_pref = true;   // 记住暂停偏好：切设备/重开流沿用
    setPlayState(PlayState::Paused);
    Refresh();
}

void FFRTMPVideoCtrl::resumeStream()
{
    if (!m_paused) {
        return;
    }
    // 恢复显示：解码一直在跑，这里只是重新允许把最新帧刷到显示。
    ffrtmp_log("resumeStream (display-only)");
    m_paused = false;
    m_display_paused_pref = false;  // 记住播放偏好：切设备/重开流沿用
    m_ever_played = true;           // 用户已主动播放：此后暂停才显示“视频已暂停”
    // 不能仅凭 m_running 就报“播放中”：m_running 只表示解码线程在跑，
    // 此时可能仍在连接/等待首帧（画面为黑）。只有确实已解码出帧才报“播放中”，
    // 否则报“加载中”，等首帧到达后由解码线程置为“播放中”，
    // 避免出现“黑屏却显示正在播放”的状态不一致。
    setPlayState((m_running && m_frame_ready) ? PlayState::Playing : PlayState::Loading);
    Refresh();
}

void FFRTMPVideoCtrl::togglePause()
{
    // 断连状态下按钮不响应。
    if (m_play_state.load() == PlayState::Disconnected || m_play_state.load() == PlayState::Not) {
        return;
    }
    if (!m_paused) {
        pauseStream();
    } else {
        resumeStream();
    }
}

void FFRTMPVideoCtrl::OnLeftDown(wxMouseEvent &event)
{
    const bool in_play_button = playButtonRect().Contains(event.GetPosition());
    ffrtmp_log("OnLeftDown: inPlayButton=" + std::string(in_play_button ? "true" : "false")
               + ", playState=" + std::to_string(static_cast<int>(m_play_state.load()))
               + ", urlEmpty=" + std::string(m_url.empty() ? "true" : "false"));

    if (in_play_button && m_play_state.load() == PlayState::Not && !m_url.empty()) {
        // First user click owns stream startup. StartStream restores the
        // initial Paused preference; resume immediately because this click is
        // the explicit Play action.
        const std::string url = m_url;
        StartStream(url);
        resumeStream();
    } else if (in_play_button && playButtonAvailable()) {
        togglePause();
    }
    event.Skip();
}

}} // namespace Slic3r::GUI
