#ifndef slic3r_GUI_FFRTMPVideoCtrl_hpp_
#define slic3r_GUI_FFRTMPVideoCtrl_hpp_

#include <wx/panel.h>
#include <wx/bitmap.h>
#include <wx/timer.h>
#include <wx/dialog.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <memory>
#include <thread>
#include <vector>

#include "MultiComDef.hpp"
#include "Slic3r/GUI/wxExtensions.hpp"

namespace Slic3r { namespace GUI {

class FFRTMPVideoCtrl : public wxPanel
{
public:
    // 画面填充方式
    //   Fit   : 等比缩放完整显示，留黑边（letterbox / contain）—— 默认，行为对齐 VLC 等播放器
    //   Cover : 等比缩放铺满控件，超出部分裁掉（铺满/裁边）
    enum class DisplayMode { Fit, Cover };

    FFRTMPVideoCtrl(wxWindow *parent);
    ~FFRTMPVideoCtrl();

    // 设置画面填充方式（默认 Fit）
    void setDisplayMode(DisplayMode mode);

    // Start RTMP stream playback on background thread. Non-blocking.
    void StartStream(const std::string &url);

    // Stop playback and release resources.
    void StopStream();

    // Returns true if a decode thread is currently active.
    bool IsStreaming() const;

    // Show the video in a popup dialog (full-screen).
    void ShowFullScreenPopup();

    // ---- PrinterCameraPanel compatibility API ----

    // Sets panel size and min/max constraints
    void setSize(wxSize size);

    // Records current communication identifier
    void setCurComId(com_id_t comId);

    // Start streaming from the given URL
    void setStreamUrl(const std::string &streamUrl);

    // 设置“下次起流的显示暂停偏好”：true=起流后停在暂停(显示缺省图+播放按钮)，false=直接播放。
    // 切设备时由外部置 true，使每台设备进入都与首次一致（默认暂停）。
    void setStartPausedPreference(bool paused);

    // Stop streaming and show an offline/default placeholder. Account logout uses
    // the default placeholder and keeps the inline panel visible.
    void setOffline(bool show_default_placeholder = false);

    // Show popup (full-screen) dialog
    void showPopup();

    // 关闭弹窗（若已打开）。用于账号登出、离开页面等需要主动收起视频窗的场景。
    void closePopup();

    // 立即把画面切到“断开连接”并隐藏（非阻塞，不回收解码线程）。
    // 用于解绑/登出：先给用户即时的视觉反馈（设备已解绑、摄像头画面已隐藏），
    // 真正的解码线程回收由 reapStoppedStream() 延后完成。
    void showOfflineImmediate(bool show_default_placeholder = false);
    // 回收已停止的解码线程（可能因 DNS/连接/关闭阻塞，交后台线程 join，不阻塞 UI）。
    // 需与 showOfflineImmediate() 配合：先置停止标志+更新 UI，再于后续 tick 调用本函数回收。
    void reapStoppedStream();

    // Diagnostic snapshot for unexpected Hide() or zero-height layout changes.
    void logPanelState(const char *reason) const;

protected:
    void OnPaint(wxPaintEvent &event);
    void OnSize(wxSizeEvent &event);

private:
    // Threading
    void DecoderThreadFunc();
    void OnFrameReady();

    // 等待上一次后台回收结束（供同步 StopStream/析构复用，防止新旧解码线程并发访问 FFmpeg 上下文）。
    void joinStopReaper();

    // 向设备发送 camera "open" 指令：WAN 下经云端让打印机开始/持续推流
    // （LAN 下命令层会自行忽略）。对齐 PrinterCameraPanel 的 rtsp_player_continue 机制。
    void sendCameraOpen();

    // ---- 播放器覆盖层（底部状态条：左下角播放/暂停按钮，右下角状态文字）----
    enum class PlayState { Initializing, Loading, Playing, Paused, Disconnected, NoCamera, Not };
    void     setPlayState(PlayState s);  // 线程安全，可从解码线程调用
    wxString statusText() const;         // 右下角状态文字
    wxRect   playButtonRect();           // 左下角播放/暂停按钮的点击区域
    bool     playButtonAvailable() const;// 播放/暂停按钮是否可用（仅播放中/已暂停时可操作）
    void     drawOverlayBar(wxDC &dc);   // 绘制底部状态条
    void     togglePause();              // 切换 播放/暂停
    void     pauseStream();              // 暂停：停解码/保活，冻结最后一帧
    void     resumeStream();             // 恢复：按最新地址重新开流
    void     OnLeftDown(wxMouseEvent &event);

    // FFmpeg helpers (only called from decode thread)
    bool OpenStream(const std::string &url);
    void CloseStream();

    // FFmpeg 阻塞 I/O 中断回调：m_running 变 false 时立即中止 open/read，
    // 让 StopStream/暂停 秒级停止，避免主线程 join 阻塞导致 UI 卡顿。
    static int interruptCb(void *opaque);

    // 单次读取结果：区分“读到数据/直播到达边缘(EOF)/真正错误”，
    // 以便对 HLS 等分段协议的 EOF 做容忍处理，而不是简单断流重连。
    enum class ReadStatus { Frame, Eof, Error };
    ReadStatus ReadAndDecodeOneFrame();

    // Synchronisation
    std::atomic<bool>              m_running{false};
    std::atomic<bool>              m_frame_ready{false};
    std::atomic<bool>              m_skip_initial_delay{false}; // 同一路流重启时跳过预热等待
    // 阻塞 I/O 硬超时截止点（steady_clock 毫秒，0 表示无截止）。设备/服务器失联时，
    // avformat_open_input / av_read_frame 可能无限阻塞——interruptCb 依据此值超时中止，
    // 让重连计数得以推进、最终判定“打印机断开连接”，而不是永远卡住冻结最后一帧。
    std::atomic<int64_t>           m_io_deadline_ms{0};
    wxCriticalSection              m_frame_cs;
    std::unique_ptr<std::thread> m_thread;
    // 后台回收线程：异步停止时用它 join 旧的解码线程，避免阻塞 UI 线程。
    std::thread                  m_stop_reaper;

    // Latest decoded RGB frame
    std::vector<uint8_t> m_rgb_buffer;  // raw RGB data from decoder thread
    int                  m_buf_width{0};   // m_rgb_buffer 对应的帧宽（与 buffer 一起在锁内更新，避免与解码线程竞争）
    int                  m_buf_height{0};  // m_rgb_buffer 对应的帧高
    wxBitmap             m_rgb_bitmap;  // created on main thread in OnFrameReady
    int                  m_video_width{1280};  // 解码线程内部使用（scaler 尺寸），非渲染依据
    int                  m_video_height{720};

    // 画面填充方式：默认 Cover（等比铺满控件、裁掉超出部分）。配合下方"控件按摄像头
    // 宽高比 + 状态条高"自适应尺寸后，画面区正好等于摄像头比例，Cover 不再裁边也不留黑边。
    DisplayMode          m_display_mode{DisplayMode::Cover};

    // 摄像头画面宽高比（固定分辨率 640x480 = 4:3）。内联时据此让控件总高 =
    // 画面区(宽/比例) + 状态条高，使画面区正好是摄像头比例 —— 既不裁也不留黑边。
    double               m_camera_aspect{4.0 / 3.0};
    bool                 m_in_on_size{false};   // OnSize 动态调高的防重入标志

    // FFmpeg objects (opaque via void*, only valid while m_running)
    void *m_format_ctx{nullptr};
    void *m_codec_ctx{nullptr};
    void *m_sws_ctx{nullptr};
    void *m_frame{nullptr};
    void *m_packet{nullptr};
    int   m_video_stream_idx{-1};

    // 当前流是否为 HLS(.m3u8)：仅解码线程读写，用于对直播边缘 EOF 做容忍。
    bool  m_is_hls{false};

    // Offline / placeholder image
    wxBitmap m_offline_bitmap;
    // 缺省图（进设备页 / 暂停且尚无画面时显示）。加载失败则回退为黑底 m_offline_bitmap。
    wxBitmap m_placeholder_bitmap;
    // Account logout must keep the inline camera area visible and paint the
    // default image, even if a queued connection-exit callback arrives later.
    std::atomic<bool> m_force_default_placeholder{false};

    // 显示暂停偏好：初始为 true —— 进设备页默认“暂停 + 缺省图”，后台照常拉流仅冻结显示。
    // 用户点播放置 false、点暂停置 true；StartStream（含切设备）沿用此偏好，实现状态保持。
    std::atomic<bool> m_display_paused_pref{true};

    // 本视图会话内用户是否曾主动点过播放。用于区分“首次进页面的默认暂停”与“用户主动暂停”：
    //   · 首次进页面（默认暂停、从未播放）：状态条不显示“视频已暂停”文字（仅缺省图 + 播放按钮）；
    //   · 用户播放后再暂停：状态条显示“视频已暂停”。
    // resumeStream() 置 true；setStartPausedPreference()（切设备/进设备页）复位为 false。
    std::atomic<bool> m_ever_played{false};
    std::unordered_map<std::string, ScalableBitmap> m_playIconMap;

    // Com ID for this camera (from PrinterCameraPanel API)
    com_id_t m_curComId{ComInvalidId};

    // 推流保活：播放期间周期性重发 camera "open"，维持打印机推流会话，
    // 避免云端会话超时（约 10s）后停止产出分片。
    wxTimer m_keepalive_timer;
    int     m_keepalive_interval_ms{5000};

    // Popup dialog for full-screen view
    wxDialog *m_popup_dlg{nullptr};
    wxWindow *m_inline_parent{nullptr}; // 内联时的父窗口，弹窗关闭后需归还，避免控件被孤立到 mainframe
    wxSize    m_inline_size;            // 内联时的固定尺寸，弹窗关闭后恢复

    // Current stream URL (for reconnect)
    std::string m_url;

    // Reconnect
    // 首次连接前的等待时间。设为 0 以最小化首帧延迟；若摄像头尚未开始推流，
    // OpenStream 会失败并由重连逻辑自动重试，无需在此白等。
    int m_initial_delay_ms{0};
    int m_reconnect_delay_ms{1000};  // 首连失败时快速重试，缩短首帧出现前的等待
    int m_reconnect_delay_max_ms{5000}; // 退避上限：直播源持续重连，不永久放弃
    int m_reconnect_attempts{0};
    // 达到该次数后判定推流已停止：状态切“打印机断开连接”、清掉冻结的最后一帧（黑底占位），
    // 但仍以退避间隔持续重连，直到 StopStream/setOffline 主动结束或推流恢复。
    // 取 3：约几秒内失败即判定断开，避免画面长时间冻结在最后一帧。
    int m_max_reconnect_attempts{3};

    // 阻塞 I/O 硬超时（毫秒）：设备/服务器失联时防止 open/read 无限阻塞。
    // open 略长以容忍慢但可用的连接；read 覆盖“连接建立但不再产出帧”的情况。
    int m_open_timeout_ms{4000};
    int m_read_timeout_ms{6000};

    // 离线占位图是否已显示，避免在持续重连期间反复 CallAfter 刷新。
    std::atomic<bool> m_offline_shown{false};

    // 本次拉流会话是否曾成功出过画面（解码到过至少一帧）。
    // 用于区分“首次加载中(还没出过画面)”与“曾经在播又断了”：
    //   · 从未出画面时，即便多次 OpenStream 失败也只显示“加载中”，不误报“打印机断开连接”；
    //   · 出过画面后再断流且重连失败，才判定“打印机断开连接”。
    // StartStream 时重置为 false，解码到首帧时置 true。
    std::atomic<bool> m_ever_got_frame{false};

    // 播放器状态（右下角状态文字），可从解码线程原子更新。
    std::atomic<PlayState> m_play_state{PlayState::Not};
    // 显示暂停标志（暂停与拉流解耦）：为 true 时解码/拉流照常，但 OnFrameReady 不更新
    // 显示位图，画面冻结。解码线程会读取它，故用原子类型。
    std::atomic<bool>      m_paused{false};

    // 弹窗关闭时记录当时的显示状态（暂停/播放），下次打开时恢复：
    // 关闭前暂停则重开仍暂停，关闭前播放则重开仍播放。
    // 初值 false：首次打开默认播放（自动进入播放状态）。
    bool                   m_popup_last_paused{false};
};

}} // namespace Slic3r::GUI

#endif
