#ifndef slic3r_GUI_TripoModelApiDialog_h_
#define slic3r_GUI_TripoModelApiDialog_h_

#include <wx/wx.h>
#include <wx/webview.h>
#include <wx/graphics.h>
#include "slic3r/GUI/GUI_Utils.hpp"
#include "slic3r/GUI/Widgets/ProgressBar.hpp"
#include "slic3r/GUI/Widgets/FFButton.hpp"
#include "slic3r/GUI/FlashForge/FFDownloadTool.hpp"
#include "slic3r/GUI/MsgDialog.hpp"
#include "slic3r/GUI/ConvertModel/ConvertModel.hpp"
#include "libslic3r/miniz_extension.hpp"
#include "slic3r/GUI/FlashForge/FFTitleLessDialog.hpp"
#include "slic3r/GUI/FlashForge/FFTransientWindow.hpp"
#include "slic3r/GUI/FlashForge/PromoShareDlg.hpp"
#include "slic3r/GUI/FlashForge/MultiComEvent.hpp"

namespace Slic3r { namespace GUI {

struct ScoreRule {
    int  text_optimize_count{0};
    int  text_trans_image_count{0};
    int  image_process_count{0};
    int  image_real_generate_count{0};
    int  image_generate_count{0};
    int  total_count{0};
    int  reagain_free_count{0};
    int  free_count{0};
    bool isOk{false};
};

typedef enum { TEXT_MODEL, IMAGE_MODEL } ModelType;

class FinishScoreEvent : public wxCommandEvent
{
public:
    FinishScoreEvent();
    int  curCostScore = 0;
    int  totalScore  = 0;
    std::string promoData;
};

class RefreshStateEvent : public wxCommandEvent
{
public:
    RefreshStateEvent();
    int         state = 0;
    int         jobId   = 0;
};

wxDECLARE_EVENT(EVT_LOADED_IMAGE, wxCommandEvent);
wxDECLARE_EVENT(EVT_OPTIMIZED_TEXT, wxCommandEvent);
wxDECLARE_EVENT(EVT_REFRESH_STATE, RefreshStateEvent);
wxDECLARE_EVENT(EVT_FINISH_TASK, wxCommandEvent);
wxDECLARE_EVENT(EVT_UPDATE_ICON, wxCommandEvent);
wxDECLARE_EVENT(EVT_ERROR_MSG, wxCommandEvent);
wxDECLARE_EVENT(EVT_LOGOUT_USER, wxCommandEvent);
wxDECLARE_EVENT(EVT_FINISH_SCORE, FinishScoreEvent);

class ModelHoverWindow : public FFRoundedWindow
{
public:
    ModelHoverWindow(wxWindow* parent = nullptr);

private:
    void onMouseCaptureLost(wxMouseCaptureLostEvent& event);
    void OnMouseLeave(wxMouseEvent& event);
    void OnActivateApp(wxActivateEvent& event);
};

class ModelBaseDialog : public FFTitleLessDialog
{
public:
    ModelBaseDialog(wxWindow* parent = nullptr);
    void EndModal(int retCode) override;
    void drawBackground(wxBufferedPaintDC& dc, wxGraphicsContext* gc);
    ~ModelBaseDialog();

protected:
    void BindMsgDialog(wxDialog* dlg);
    void bindConnEvent(ComWanDevMaintainEvent& event);
    bool m_offline{false};
    int       m_msg_res{wxID_CANCEL};
    int       m_res{wxID_CANCEL};
    bool      m_needClose{false};
    wxDialog* m_msg{nullptr};
};

class ModelApiTask : public wxEvtHandler, public std::enable_shared_from_this<ModelApiTask>
{
public:
    ModelApiTask(wxEvtHandler* parent);
    void setThreadFunc(std::function<void()> func);
    wxSemaphore& Sem();
    void              safeFunc(std::function<void()> func);
    std::atomic_bool& FinishLoop();
    std::mutex&       Lock();
    wxEvtHandler*     Parent();
    void start();

private:
    std::function<void()> m_func;
    wxSemaphore           m_sem;
    std::atomic_bool      m_isFinish;
    std::mutex            m_lock;
    wxEvtHandler*         m_parent{nullptr};
};

class ApiLoadingIcon : public wxEvtHandler
{
public: 
    ApiLoadingIcon(wxDialog* parent);
    void paintInRect(wxGraphicsContext* gc, wxRect rect);
    void Loading(int interval);
    void End();
    bool isLoading();
    ~ApiLoadingIcon() { End(); }

private:
    void                        OnTimer(wxTimerEvent& event);
    int                         m_loadingIdx = 0;
    int                         m_loadingTime = 0;
    wxDialog*                   m_parent{nullptr};
    wxTimer* m_timer{nullptr};
    std::vector<ScalableBitmap> m_loadingIcons;
};

class FFTextCtrl : public wxTextCtrl
{
public:
    FFTextCtrl(wxWindow* parent = nullptr, wxString text = "", wxSize size = wxDefaultSize, int style = 0, wxString hint = "");
    void SetTextHint(const wxString& hint);
    void SetMaxBytes(int max_length);

private:
    wxString m_hint;
    wxString                 m_old_text;
    int                      m_max_length;
    Label*                   m_length_label{nullptr};
    std::vector<std::string> m_vs;
    void     OnPaint(wxPaintEvent& event);
};

class ImageWhatDoingPanel : public wxPanel
{
public:
    typedef enum { HOVER_LINK, FIRST_DLG, RULE_HOVER_LINK } DlgType;
    ImageWhatDoingPanel(wxWindow* parent = nullptr, DlgType type = HOVER_LINK);
    static ModelHoverWindow* createPopup(wxWindow* parent = nullptr, DlgType type = HOVER_LINK);
    static ModelBaseDialog*   createDialog(wxWindow* parent = nullptr);
    FFButton*                 getCancelButton();
    FFButton*                 getConfirmButton();

private:
    FFButton*                                       m_cancel_btn{nullptr};
    FFButton*                                       m_confirm_btn{nullptr};
    DlgType                                            m_type;
    std::unordered_map<std::string, ScalableBitmap> m_bmp_map;
    wxBoxSizer* create_bmp_orders(const std::vector<std::string>& bmp_lists, 
                                  const std::vector<wxString>& up_text_lists, 
                                  const std::vector<wxString>& down_text_lists);
};

class ImageQuestionDialog : public ModelHoverWindow
{
public:
    ImageQuestionDialog(wxWindow* parent = nullptr);
    void SetProcessed(bool processed, bool init = false);

private:
    Label* m_info;
    bool   m_processed{false};
};

class ImageUploadPanel : public wxPanel
{
public: 
    ImageUploadPanel(wxWindow* parent);
    bool     judgeTransImage(wxString& path);
    bool     compressImage(const wxString& inpath, wxString& outpath);
    wxString getPath();
    void     SetProcessed(bool processed, bool init = false);
    wxDialog*     HasDlg();

private:
    wxString m_path;
    wxImage m_img;
    wxFileDialog*            m_file_dialog{nullptr};
    std::vector<std::string> m_vs;
    ScalableBitmap m_upload_icon, m_delete_icon;
    bool                     m_processed{false};

private:
    void onPaint(wxPaintEvent& event);
    void onLeftDown(wxMouseEvent& event);
    void onLeftUp(wxMouseEvent& event);
    void onMouseCaptureLost(wxMouseCaptureLostEvent& event);
    void OnMouseEnter(wxMouseEvent& event);
    void OnMouseLeave(wxMouseEvent& event);
    bool m_isGeneratePressed;
    bool m_isHovered;
};

class ModelApiDialog : public ModelBaseDialog
{
public:
    static void updateCustomModelDir();
    static const std::string& GetDir();
    ModelApiDialog(wxWindow* parent = nullptr);
    wxString getImage();
    wxString  getText();
    ModelType getType();
    bool      IsImageProcess();
    int       getOldJobId();
    void drawBackground(wxBufferedPaintDC& dc, wxGraphicsContext* gc);
    ~ModelApiDialog();

private:
    static std::string                              m_dir_path;
    std::unordered_map<std::string, ScalableBitmap> m_bmp_map;
    wxString m_cost_text;
    wxString m_score_text;
    wxRect                                          m_generate_btn_rect;
    ModelType                                       m_generateType;
    wxRect                                                          m_question_link_rect;
    wxRect                                                          m_pretreat_link_rect;
    wxRect                                                          m_pretreat_btn_rect;
    wxRect                                                          m_rule_link_rect;
    std::unordered_map<int, wxRect>                                             m_model_type_rects;
    ImageUploadPanel*                               m_image_panel{nullptr};
    FFTextCtrl*                                     m_text_ctrl{nullptr};
    wxPanel*                                        m_text_panel{nullptr};
    ImageQuestionDialog*                                 m_question_dialog{nullptr};
    FFRoundedWindow*                                     m_what_doing_dialog{nullptr};
    FFRoundedWindow*                                     m_rule_dialog{nullptr};
    std::shared_ptr<ApiLoadingIcon>                                 m_loadIcon;
    std::shared_ptr<ModelApiTask>                                   m_loadTask;
    void drawCenterText(wxBufferedPaintDC& dc, wxGraphicsContext* gc, const wxString& str, int height, const wxFont& font, wxColour color, wxString iconName = "");
    void onLeftDown(wxMouseEvent& event);
    void onLeftUp(wxMouseEvent& event);
    void onMouseCaptureLost(wxMouseCaptureLostEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void GenerateClicked();
    void RefreshScore();
    void changeModelType(ModelType type);
    int  m_cost_score = 0;
    int  m_total_score = 0;
    int         m_old_job_id{-1};
    std::string m_promoData;
    bool m_isGeneratePressed{false};
    bool m_isGenerateHovered{false};
    bool m_isQuestionHovered{false};
    bool m_isPretreatHovered{false};
    bool m_isRuleHovered{false};
    bool m_can_image_pretreat{false};
    bool        m_first_image{false};
    bool m_isTextTypePressed{false};
    bool m_isImageTypePressed{false};
};

class ModelImageProcessDialog : public ModelBaseDialog
{
public:
    typedef enum {NODO, IMG_TO_IMG, TXT_TO_TXT, TXT_TO_IMG} ProcessState;
    ModelImageProcessDialog(wxWindow* parent = nullptr);
    void drawBackground(wxBufferedPaintDC& dc, wxGraphicsContext* gc);
    wxString                        getProcessedImage();
    wxString                        getProcessedUrl();
    wxString                        getOptimizedText();
    bool                            IsOptimized();
    void                            FirstStep(bool isFirstStep);
    void     setSrcImage(const wxString& path);
    void     setSrcText(const wxString& text, bool isOptimized = false);
    void                            changeModelType(ModelType type, bool init = false);
    ~ModelImageProcessDialog();

private:
    std::shared_ptr<ApiLoadingIcon> m_loadIcon;
    std::shared_ptr<ModelApiTask>   m_processTask;
    wxString                        m_src_image_path;
    wxString                        m_src_text;
    wxString                        m_optimize_text;
    wxString                        m_image_path;
    int                             m_download_id{-1};
    static FFDownloadTool           m_download_tool;
    std::string                     m_download_path;
    wxString                        m_image_url;
    Label*                          m_info_text{nullptr};
    Label*                          m_detail_text{nullptr};
    int                             m_job_id{-1};
    bool                            m_errorExit{false};
    ProcessState                    m_state{NODO};
    ModelType                       m_type{IMAGE_MODEL};
    bool                            m_isOptimized{false};
    bool                            m_isFirstStep{true};
    void                            finishDownloadEvent(FFDownloadFinishedEvent& event);
};

class ZoomOutDialog : public FFTitleLessDialog
{
public:
    ZoomOutDialog(wxWindow* parent, const wxImage& image);
    void drawBackground(wxBufferedPaintDC& dc, wxGraphicsContext* gc);

private:
    wxImage m_image;
};

class ModelSingleImageDialog : public ModelBaseDialog
{
public:
    ModelSingleImageDialog(wxWindow* parent, const wxString& image_path);
    void drawBackground(wxBufferedPaintDC& dc, wxGraphicsContext* gc);
    void SetAgainScore(int score);
    bool IsOffline();

private:
    wxImage m_image;
    std::unordered_map<std::string, ScalableBitmap> m_bmp_map;
    FFButton*                                       m_btn;
    Button*                                         m_again_link_btn;
    Label*                                          m_title;
    wxRect                                          m_again_btn_rect;
    wxRect                                          m_zoom_btn_rect;
    void                                            onLeftDown(wxMouseEvent& event);
    void                                            onLeftUp(wxMouseEvent& event);
    void                                            onMouseCaptureLost(wxMouseCaptureLostEvent& event);
    bool                                            m_isZoomOutPressed{false};
    bool                                            m_isAgainPressed{false};
    int                                             m_againScore{0};
    ModelType                                       m_type{IMAGE_MODEL};
    bool                                            m_selected{false};
};

class ModelImageItemPanel :public wxPanel 
{
public:
    ModelImageItemPanel(wxWindow* parent, const wxImage& image);
    void OnPaint(wxPaintEvent& event);
    void SetChecked(bool checked);
    bool Checked();
    
private:
    wxImage                                         m_image;
    std::unordered_map<std::string, ScalableBitmap> m_bmp_map;
    wxRect                                          m_zoom_btn_rect;
    bool                                            m_checked{false};
    void                                            onLeftDown(wxMouseEvent& event);
    void                                            onLeftUp(wxMouseEvent& event);
    void                                            onMouseCaptureLost(wxMouseCaptureLostEvent& event);
    bool                                            m_isZoomPressed{false};
    bool                                            m_isPressed{false};
};

class ModelFourImageDialog : public FFTitleLessDialog
{
public:
    ModelFourImageDialog(wxWindow* parent, std::vector<wxString> image_path_list);
    void drawBackground(wxBufferedPaintDC& dc, wxGraphicsContext* gc);

private:
    std::vector<ModelImageItemPanel*>                            m_image_panel_list;
    std::unordered_map<std::string, ScalableBitmap> m_bmp_map;
    FFButton*                                       m_btn;
    Button*                                         m_again_link_btn;
    Label*                                          m_title;
    wxRect                                          m_again_btn_rect;
    void                                            onLeftDown(wxMouseEvent& event);
    void                                            onLeftUp(wxMouseEvent& event);
    void                                            onMouseCaptureLost(wxMouseCaptureLostEvent& event);
    bool                                            m_isAgainPressed{false};
    int                                             m_again_btn_y{-1};
};

class ApiSetStateEvent : public wxCommandEvent
{
public:
    ApiSetStateEvent();
    bool isQueuePanel{false};
    bool isShowQueue{false};
    int  remainCount = 0;
    int  totalCount  = 0;
};

class CompleteModelEvent : public wxCommandEvent
{
public:
    CompleteModelEvent();
    std::string path;
    int64_t     job_id;
};

class ChoiceColorEvent : public wxCommandEvent
{
public:
    ChoiceColorEvent();
    std::shared_ptr<convert_model_data_t> data;
    cvt_colors_t                          colors;
};

class CompleteConvertEvent : public wxCommandEvent
{
public:
    CompleteConvertEvent();
    cvt_colors_t                          colors;
    std::string                           obj_path;
    std::string                           mtl_path;
};

wxDECLARE_EVENT(EVT_OLD_TASK, wxCommandEvent);
wxDECLARE_EVENT(EVT_SET_ID, wxCommandEvent);
wxDECLARE_EVENT(EVT_SET_STATE, ApiSetStateEvent);
wxDECLARE_EVENT(EVT_COMPLETE_MODEL, CompleteModelEvent);
wxDECLARE_EVENT(EVT_CHOICE_COLOR, ChoiceColorEvent);
wxDECLARE_EVENT(EVT_COMPLETE_CONVERT, CompleteConvertEvent);
wxDECLARE_EVENT(EVT_REAL_CLOSE, wxCommandEvent);

class ModelGenerateDialog : public ModelBaseDialog
{
public:
    ModelGenerateDialog(wxWindow* parent = nullptr);
    void                                  SetImgPath(wxString path, bool isUpload = true, int oldJobId = -1);
    void drawBackground(wxBufferedPaintDC& dc, wxGraphicsContext* gc);
    std::shared_ptr<convert_model_data_t> getModelData();
    cvt_colors_t                          getCvtColors();
    std::string                           getDownloadPath();
    void            showCurState(bool isQueuePanel, bool isShowQueue = true);
    ~ModelGenerateDialog();

private:
    Label*          m_info_text{nullptr};
    Label*          m_queue_text{nullptr};
    wxPanel*        m_under_queue_sperator{nullptr};
    std::shared_ptr<ApiLoadingIcon> m_loadIcon;
    wxBoxSizer*     m_sizer{nullptr};
    std::shared_ptr<int64_t>        m_job_id;
    bool                            m_errorExit{false};
    bool                            m_isShowQueue{false};
    bool                            m_isQueuePanel{true};
    bool                            m_download_try_angin{false};
    bool                            m_can_cancel{true};
    bool                            m_isFirstStep{false};
    int             m_remainCount = 5, m_totalCount = 20;
    std::string                     m_download_path;
    std::string                     m_src_path;
    std::shared_ptr<ModelApiTask>   m_generateTask;
    std::shared_ptr<ModelApiTask>   m_abortTask;
    static FFDownloadTool                  m_download_tool;
    int                                   m_download_id{-1};
    std::shared_ptr<convert_model_data_t> m_modelData;
    cvt_colors_t                          m_cvt_colors;
    void                                   finishDownloadEvent(FFDownloadFinishedEvent& event);
};

class VerticalCenterTextCtrl : public wxTextCtrl
{
public:
    VerticalCenterTextCtrl(wxWindow* parent);

protected:
    void OnPaint(wxPaintEvent& event);
};

class ModelColorDialog : public FFTitleLessDialog
{
public:
    ModelColorDialog(wxWindow* parent = nullptr);
    void drawBackground(wxBufferedPaintDC& dc, wxGraphicsContext* gc);
    void changeColor(const cvt_colors_t& colors);
    void setModelData(const std::shared_ptr<convert_model_data_t>& data);
    void setDownloadFile(const std::string& path);
    ~ModelColorDialog();

private:
    FFButton*                             m_btn;
    std::shared_ptr<ModelApiTask> m_convertTask;
    std::vector<wxColour> m_color_grids;
    std::shared_ptr<convert_model_data_t> m_modelData;
    std::shared_ptr<ApiLoadingIcon> m_loadIcon;
    int                   m_last_color_count = 4;
    std::string                           m_filepath;
    wxTextCtrl*                       m_text_ctrl{nullptr};
};

class ModelApi
{
public:
    static void ShowModelApi(wxWindow* parent = nullptr);

private:
    static void End();
    static bool m_exist;
};

}} // namespace Slic3r::GUI

#endif