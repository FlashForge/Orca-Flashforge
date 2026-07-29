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

// Debug output helper — writes to both Boost log and Visual Studio / DebugView on Windows
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
    SetMinSize(wxSize(FromDIP(621), FromDIP(466)));

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

    // 保活定时器：播放期间周期性重发 camera "open"，维持打印机推流。
    m_keepalive_timer.Bind(wxEVT_TIMER, [this](wxTimerEvent &) { sendCameraOpen(); });
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

    bool valid = false;
    const com_dev_data_t &data = MultiComMgr::inst()->devData(m_curComId, &valid);
    if (valid && data.devDetail != nullptr && data.devDetail->camera != 1) {
        // Ignore delayed/stale stream URL updates for devices without a camera.
        m_paused = false;
        StopStream();
        setPlayState(PlayState::NoCamera);
        return;
    }

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

    if (m_paused && m_url == url) {
        ffrtmp_log("paused, ignoring same-url StartStream");
        return;
    }

    // 暂停只是“显示冻结”，与拉流解耦：即使处于暂停显示态，解码/拉流仍照常进行/切换，
    // 因此这里不拦截，正常起流（切设备时会切到新流；用户点播放即恢复显示最新帧）。
    if (m_running && m_url == url) {
        ffrtmp_log("Already streaming this url, ignoring duplicate StartStream");
        return;
    }

    // 是否为同一路流的重启（用于跳过 2s 预热等待，实现快速恢复）
    const bool same_url = (!m_url.empty() && m_url == url);

    StopStream();

    m_url = url;
    m_running = true;
    m_reconnect_attempts = 0;
    m_offline_shown = false;
    m_ever_got_frame = false;                // 新会话：尚未出过画面（首次加载中不报断开）
    m_paused = m_display_paused_pref.load();  // 进设备页默认“暂停”(显示缺省图+播放按钮)；
                                              // 用户播放过则保持播放（切设备沿用）。atomic 需 .load()
    m_skip_initial_delay = same_url;
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

    m_thread = std::make_unique<std::thread>(&FFRTMPVideoCtrl::DecoderThreadFunc, this);

    // 立即通知设备开流，并启动定时保活 —— 让打印机（经云端）开始并持续推流。
    // 这条 open 指令走 WAN 云链路，是画面真正出现的前提；仅拉流而不开流拿到的是空地址。
    sendCameraOpen();
    m_keepalive_timer.Start(m_keepalive_interval_ms);
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
    m_keepalive_timer.Stop();  // 停止推流保活（wxTimer 仅可在主线程操作）
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
    m_keepalive_timer.Stop();
    m_running = false;   // 非阻塞：通知解码线程退出，且 OnFrameReady 会据此停止刷新（避免残帧闪回）
    m_paused  = false;   // 断连清除显示暂停态
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

    // 打开摄像头窗口：恢复上次关闭时的显示状态（首次打开默认播放，见 m_popup_last_paused 初值）。
    // 后台解码/拉流始终在跑（暂停与拉流解耦），这里仅决定是否冻结显示。
    // 断连时保持“断开连接”状态条，不改播放/暂停显示。
    if (m_play_state.load() == PlayState::Disconnected || m_play_state.load() == PlayState::NoCamera) {
        // Keep unavailable-state overlay.
    } else if (m_popup_last_paused) {
        pauseStream();
    } else {
        resumeStream();
    }

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
        // 弹窗专属：关闭后控件必须先从即将销毁的弹窗中 Reparent 出去（否则会随弹窗一起销毁），
        // 归还给内联父窗口后保持 Hidden —— 设备状态面板上不显示任何视频画面。
        // 解码线程不停止，保证再次打开弹窗时能立即出画面。
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

void FFRTMPVideoCtrl::DecoderThreadFunc()
{
    ffrtmp_log("Decode thread started");

    // 首次连接前等待，给摄像头时间启动推流（RTMP 直播流需要几秒准备）。
    // 同一路流重启（m_skip_initial_delay）时跳过，避免每次重连都白等 2 秒。
    if (m_reconnect_attempts == 0 && m_initial_delay_ms > 0 && !m_skip_initial_delay) {
        ffrtmp_log("Waiting " + std::to_string(m_initial_delay_ms) + "ms for camera stream to become ready...");
        std::this_thread::sleep_for(std::chrono::milliseconds(m_initial_delay_ms));
    }
    m_skip_initial_delay = false;

    while (m_running) {
        ffrtmp_log("Attempting OpenStream (attempt " + std::to_string(m_reconnect_attempts + 1)
                   + ") for " + m_url);

        if (OpenStream(m_url)) {
            m_reconnect_attempts = 0;
            m_offline_shown = false;
            // 显示暂停时（进设备页默认暂停+缺省图）不改动状态，保持“已暂停”显示。
            if (!m_paused) {
                setPlayState(PlayState::Loading);   // 已连接，等待首帧：视频加载中
            }
            int frame_count = 0;
            // 最近一次“有数据”的时间点，用于 HLS 到达直播边缘时的宽限判定。
            auto last_progress = std::chrono::steady_clock::now();
            // HLS 直播边缘 EOF 宽限期：期间在同一连接上原地重试读取，
            // 等待设备（经保活）产出新分片，避免为了一次边缘 EOF 就整路重连。
            // 取 8s：既能容忍正常直播分片间隔（通常 2~4s），又能在推流真正停止时
            // 较快进入重连/断开流程（避免画面长时间冻结在最后一帧）。可按分片时长调整。
            const auto hls_eof_grace = std::chrono::seconds(8);

            while (m_running) {
                // 不做人工帧率限制：av_read_frame 会阻塞等待网络数据，天然按源帧率节流。
                // 若加 30fps 睡眠上限，连接时服务端一次性下发的 GOP 缓存无法被快速消费，
                // 会造成延迟不断累积且永不恢复，这是直播画面高延迟的主因。
                ReadStatus st = ReadAndDecodeOneFrame();
                if (st == ReadStatus::Frame) {
                    ++frame_count;
                    last_progress = std::chrono::steady_clock::now();
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
                if (st == ReadStatus::Eof && m_is_hls) {
                    // 直播到达边缘：不断流。短暂等待后在同一连接上继续读，
                    // 等待新分片到来；仅当超过宽限期仍无数据才重开连接。
                    if (std::chrono::steady_clock::now() - last_progress > hls_eof_grace) {
                        ffrtmp_log("HLS EOF grace exceeded, reopening stream (frames="
                                   + std::to_string(frame_count) + ")");
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(300));
                    continue;
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
        return 1;
    }
    // 阻塞 I/O 超时：设备/服务器失联时 open/read 可能无限阻塞，超过截止点即中止，
    // 让 open/read 返回错误、重连计数推进，最终判定“打印机断开连接”。
    int64_t dl = self->m_io_deadline_ms.load();
    if (dl != 0 && ffrtmp_now_ms() > dl) {
        return 1;
    }
    return 0;
}

bool FFRTMPVideoCtrl::OpenStream(const std::string &url)
{
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

    // 按 URL 协议/扩展名选择解复用参数，实现多协议支持：
    //   rtmp(e/s/t):// —— RTMP 直播流
    //   *.m3u8         —— HLS 直播/点播（Apple HTTP Live Streaming）
    //   其它 http(s)   —— HTTP-FLV 等
    // 之前无条件设置 RTMP 专属选项会让 HLS(.m3u8) 无法正常播放，这里分协议配置。
    std::string lurl = url;
    std::transform(lurl.begin(), lurl.end(), lurl.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    const bool is_rtmp = lurl.rfind("rtmp://", 0) == 0 || lurl.rfind("rtmpe://", 0) == 0
                      || lurl.rfind("rtmps://", 0) == 0 || lurl.rfind("rtmpt://", 0) == 0;
    const bool is_hls  = lurl.find(".m3u8") != std::string::npos;
    m_is_hls = is_hls;

    // 通用低延迟解复用选项（对所有协议生效）
    av_dict_set(&opts, "max_delay",       "100000",   0);  // max demux-decode delay (us)
    av_dict_set(&opts, "probesize",       "32768",    0);  // smaller probe for faster start
    av_dict_set(&opts, "fflags",          "nobuffer", 0);  // disable format-level buffering
    av_dict_set(&opts, "analyzeduration", "1000000",  0);  // 1s analysis (default 5s)
    av_dict_set(&opts, "flush_packets",   "1",        0);  // flush packets immediately

    if (is_rtmp) {
        // RTMP live stream options — tuned for low latency
        av_dict_set(&opts, "rtmp_live",   "live", 0);
        av_dict_set(&opts, "rtmp_listen", "0",    0);  // 显式关闭 listen 模式（timeout 选项会导致 listen 被强制开启，故移除 timeout）
        av_dict_set(&opts, "rtmp_buffer", "100",  0);  // 客户端向 RTMP 服务端请求的缓冲时长，FFmpeg 默认 3000ms 是直播高延迟的主因，改小以降低延迟
    } else {
        // HLS(.m3u8) / HTTP(-FLV) options
        // 允许 HLS 播放列表内嵌 https/tls 加密分片，否则会因协议白名单拒绝加载子分片而失败。
        av_dict_set(&opts, "protocol_whitelist",
                    "file,crypto,data,http,https,tcp,tls,hls,applehttp", 0);
        // HTTP 断流自动重连：分片偶发 404/超时时不至于整路失败。
        av_dict_set(&opts, "reconnect",           "1", 0);
        av_dict_set(&opts, "reconnect_streamed",  "1", 0);
        av_dict_set(&opts, "reconnect_delay_max", "2", 0);
        if (is_hls) {
            // 直播 HLS 从最新分片开始，贴近直播边缘、降低起播延迟。
            av_dict_set(&opts, "live_start_index", "-1", 0);
        }
    }

    ffrtmp_log(std::string("avformat_open_input... (") + (is_rtmp ? "rtmp" : is_hls ? "hls" : "http") + ")");

    // 打开阶段设硬超时：服务器失联时 TCP 连接/握手可能长时间阻塞，
    // 超时后 interruptCb 会让 avformat_open_input 返回错误，避免永久卡住。
    m_io_deadline_ms = ffrtmp_now_ms() + m_open_timeout_ms;
    // 这是解绑时最可能卡住的地方：DNS 解析 / TCP connect 无法被 interruptCb 打断，
    // 会一直阻塞到 OS 超时。看这两条日志的时间戳差即可判断本次 open 阻塞了多久。
    ffrtmp_ts("[decode] avformat_open_input BEGIN (DNS/连接, 可能长阻塞)");
    ret = avformat_open_input(&fmt_ctx, url.c_str(), nullptr, &opts);
    ffrtmp_ts(std::string("[decode] avformat_open_input END ret=") + std::to_string(ret));
    m_io_deadline_ms = 0;
    av_dict_free(&opts);

    if (ret < 0) {
        ffrtmp_err("avformat_open_input failed: " + av_err_str(ret)
                   + " (code=" + std::to_string(ret) + ")");
        return false;
    }

    ffrtmp_log("avformat_find_stream_info...");

    m_io_deadline_ms = ffrtmp_now_ms() + m_open_timeout_ms;
    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    m_io_deadline_ms = 0;
    if (ret < 0) {
        ffrtmp_err("avformat_find_stream_info failed: " + av_err_str(ret));
        avformat_close_input(&fmt_ctx);
        return false;
    }

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
                   + " codec_id=" + std::to_string(cp->codec_id));
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
}

FFRTMPVideoCtrl::ReadStatus FFRTMPVideoCtrl::ReadAndDecodeOneFrame()
{
    AVFormatContext *fmt_ctx  = FMT_CTX(m_format_ctx);
    AVCodecContext  *codec_ctx = CDC_CTX(m_codec_ctx);
    AVFrame         *frame    = FRM(m_frame);
    AVPacket        *packet   = PCKT(m_packet);

    // 读取阶段设硬超时：连接已建立但设备停止推流（不再产出帧）时，av_read_frame 可能
    // 长时间阻塞等待数据，超时后 interruptCb 令其返回错误，走重连→最终判定断开。
    m_io_deadline_ms = ffrtmp_now_ms() + m_read_timeout_ms;
    int ret = av_read_frame(fmt_ctx, packet);
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
        av_packet_unref(packet); return ReadStatus::Frame;
    }

    ret = avcodec_send_packet(codec_ctx, packet);
    av_packet_unref(packet);
    if (ret < 0) { return ReadStatus::Frame; }

    while (ret >= 0) {
        ret = avcodec_receive_frame(codec_ctx, frame);
        if (ret == AVERROR(EAGAIN)) break;
        if (ret < 0) return ReadStatus::Frame;

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
            if (!sws) { av_frame_unref(frame); return ReadStatus::Frame; }
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

        sws_scale(sws, frame->data, frame->linesize, 0, codec_ctx->height,
                  dst, &dst_stride);

        {
            // buffer 与其尺寸必须一起在锁内更新，主线程读取时才能保证一致，
            // 否则渲染时用到不匹配的宽高会导致画面尺寸错乱（突然变小）甚至内存越界。
            wxCriticalSectionLocker lock(m_frame_cs);
            m_rgb_buffer  = std::move(buffer);
            m_buf_width   = fw;
            m_buf_height  = fh;
            m_frame_ready = true;
        }

        CallAfter(&FFRTMPVideoCtrl::OnFrameReady);
        av_frame_unref(frame);
    }
    return ReadStatus::Frame;
}

#else  // !FFRTMP_USE_FFMPEG

// Stubs when FFmpeg is not available
void   FFRTMPVideoCtrl::DecoderThreadFunc()                           {}
bool   FFRTMPVideoCtrl::OpenStream(const std::string &)              { return false; }
void   FFRTMPVideoCtrl::CloseStream()                                {}
FFRTMPVideoCtrl::ReadStatus FFRTMPVideoCtrl::ReadAndDecodeOneFrame() { return ReadStatus::Error; }

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
    // 切换设备：摄像头画面跟随切换（新地址由后续 setStreamUrl 触发 StartStream 正常播放）。
    // 暂停只是显示层面的冻结，不在这里干预。
    m_curComId = comId;

    bool valid = false;
    const com_dev_data_t &data = MultiComMgr::inst()->devData(comId, &valid);
    if (valid && data.devDetail != nullptr && data.devDetail->camera != 1) {
        // 与设备页摄像头按钮使用相同的能力判断。切到无摄像头设备时停止旧流，
        // 清除旧设备残帧，并在缺省图上显示明确的“未检测到摄像头”状态。
        m_force_default_placeholder = false;
        m_paused = false;
        StopStream();
        setPlayState(PlayState::NoCamera);
    }
}

void FFRTMPVideoCtrl::sendCameraOpen()
{
    if (m_curComId == ComInvalidId) {
        return;
    }
    // 无条件下发，与 PrinterCameraPanel::onScriptMessage 一致：
    //   · WAN → ComCameraStreamCtrl::exec 经 ComWanConn 把 "open" 发到云端，
    //           云端通知打印机启动摄像头并推流；
    //   · LAN → exec 返回 COM_UNSUPPORTED，自动忽略，无副作用。
    // putCommand 内部对 WAN 未在线的情况会拒绝并记录日志，无需在此判断。
    bool ok = MultiComMgr::inst()->putCommand(m_curComId, new ComCameraStreamCtrl("open"));
    if (!ok) {
        // 仅在失败时记录（通常意味着 WAN 未在线）；成功的保活不打日志，避免刷屏。
        ffrtmp_err("sendCameraOpen putCommand rejected, comId=" + std::to_string(m_curComId));
    }
}

void FFRTMPVideoCtrl::setStreamUrl(const std::string &streamUrl)
{
    StartStream(streamUrl);
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

void FFRTMPVideoCtrl::showPopup()
{
    ShowFullScreenPopup();
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

void FFRTMPVideoCtrl::OnFrameReady()
{
    // Called on main thread via CallAfter — safe to create GDI objects here
    // 已停止（如解绑/登出触发的异步停流）：解码线程退出前可能仍有若干帧回调在队列里，
    // 此时不再更新显示位图/触发重绘，避免在页面拆除期间做无谓渲染、加重卡顿。
    if (!m_running) {
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
        wxImage img(m_buf_width, m_buf_height, false);
        memcpy(img.GetData(), m_rgb_buffer.data(), need);
        m_rgb_bitmap = wxBitmap(img);
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
    // 仅在状态变化时刷新，避免解码线程每帧 setPlayState(Playing) 造成刷新风暴。
    if (m_play_state.exchange(s) == s) {
        return;
    }
    CallAfter([this]() { Refresh(false); });
}

wxString FFRTMPVideoCtrl::statusText() const
{
    // 多语言：msgid 用英文，各语言译文见 localization/flashforge/{lang}/flashforge_{lang}.po。
    switch (m_play_state.load()) {
    case PlayState::Initializing: return _L("Initializing camera...");
    case PlayState::Loading:      return _L("Loading video...");
    case PlayState::Playing:      return _L("Video is playing");
    // 首次进页面的默认暂停（从未主动播放过）不显示“视频已暂停”文字，仅露出缺省图 + 播放按钮；
    // 用户主动播放过后再暂停才显示。
    case PlayState::Paused:       return m_ever_played.load() ? _L("Video is paused") : wxString();
    case PlayState::Disconnected: return _L("Printer disconnected");
    case PlayState::NoCamera:     return _L("No camera detected");
    // 初始不可用状态：左侧显示不可播放图标，右侧不显示任何状态文字。
    case PlayState::Not:          return wxString();
    default:                      return wxString();
    }
}

bool FFRTMPVideoCtrl::playButtonAvailable() const
{
    // 仅当视频真正在播放或已暂停时，播放/暂停按钮才可用。
    // 初始化中(Initializing)、加载中(Loading)、断开连接(Disconnected) 等状态下
    // 不显示按钮，用户无法操作。
    PlayState s = m_play_state.load();
    return s == PlayState::Playing || s == PlayState::Paused;
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
    } else if (m_paused){
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
    // 暂停与拉流解耦：后台解码/拉流/保活继续运行，只是“冻结显示”——
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
    // 仅在按钮可用（播放中/已暂停）时响应点击；加载中/断连等状态忽略。
    if (playButtonAvailable() && playButtonRect().Contains(event.GetPosition())) {
        togglePause();
    }
    event.Skip();
}

}} // namespace Slic3r::GUI
