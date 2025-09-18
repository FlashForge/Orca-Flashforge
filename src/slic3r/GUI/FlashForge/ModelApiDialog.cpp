#include "ModelApiDialog.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/Utils/Http.hpp"
#include "slic3r/GUI/Plater.hpp"
#include "slic3r/GUI/FFUtils.hpp"
#include "slic3r/GUI/FlashForge/MultiComHelper.hpp"
#include "slic3r/GUI/FlashForge/MultiComMgr.hpp"
#include <wx/base64.h>
#include <curl/curl.h>

#define HUNYUAN 1
#define TRIPO   2
#define AI_SUPPLIER HUNYUAN
#define TIMEOUT_LIMIT 30000
namespace Slic3r {
namespace GUI {

std::shared_ptr<ScoreRule> g_scoreRule;
std::shared_ptr<int>       g_pipeline;

wxDEFINE_EVENT(EVT_LOADED_IMAGE, wxCommandEvent);
wxDEFINE_EVENT(EVT_OPTIMIZED_TEXT, wxCommandEvent);
wxDEFINE_EVENT(EVT_REFRESH_STATE, RefreshStateEvent);
wxDEFINE_EVENT(EVT_FINISH_TASK, wxCommandEvent);
wxDEFINE_EVENT(EVT_UPDATE_ICON, wxCommandEvent);
wxDEFINE_EVENT(EVT_ERROR_MSG, wxCommandEvent);
wxDEFINE_EVENT(EVT_FINISH_SCORE, FinishScoreEvent);
wxDEFINE_EVENT(EVT_STORE_PROMO, wxCommandEvent);
wxDEFINE_EVENT(EVT_LOGOUT_USER, wxCommandEvent);

std::string ModelApiDialog::m_dir_path = "";

ModelHoverWindow::ModelHoverWindow(wxWindow* parent) : FFRoundedWindow(parent)
{ 
    Bind(wxEVT_LEAVE_WINDOW, &ModelHoverWindow::OnMouseLeave, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &ModelHoverWindow::onMouseCaptureLost, this);
    wxGetApp().Bind(wxEVT_ACTIVATE_APP, &ModelHoverWindow::OnActivateApp, this); 
}

void ModelHoverWindow::onMouseCaptureLost(wxMouseCaptureLostEvent& event) 
{
    Show(false);
    event.Skip();
}

void ModelHoverWindow::OnMouseLeave(wxMouseEvent& event) 
{ 
    Show(false);
    event.Skip(); 
}

void ModelHoverWindow::OnActivateApp(wxActivateEvent& event)
{
    event.Skip();
    if (event.GetActive()) {
        return;
    }
    Show(false);
}

ImageWhatDoingPanel::ImageWhatDoingPanel(wxWindow* parent, DlgType type) : 
    wxPanel(parent), m_type(type)
{
    m_bmp_map["origin_image"]           = ScalableBitmap(this, "model_image_origin", 80);
    m_bmp_map["processing_image"]       = ScalableBitmap(this, "model_image_processing", 80);
    m_bmp_map["processed_image"]        = ScalableBitmap(this, "model_image_processed", 80);
    m_bmp_map["unprocess_image"]        = ScalableBitmap(this, "model_image_unprocess", 80);
    m_bmp_map["origin_text"]           = ScalableBitmap(this, "model_text_origin", 80);
    m_bmp_map["text_to_text"]       = ScalableBitmap(this, "model_text_to_text", 80);
    m_bmp_map["text_to_img"]        = ScalableBitmap(this, "model_text_to_img", 80);
    m_bmp_map["processed_text"]        = ScalableBitmap(this, "model_text_processed", 80);
    m_bmp_map["arrow"]      = ScalableBitmap(this, "long_arrow_line", 5);
    m_bmp_map["arrow_dark"] = ScalableBitmap(this, "long_arrow_line_dark", 5);
    wxColour          font_color;
    const std::string arrow_bmp = type != FIRST_DLG ? "arrow" : "arrow_dark";
    if (type != FIRST_DLG) {
        SetBackgroundColour(wxColor("#333333"));
        font_color = *wxWHITE;
    } else {
        SetBackgroundColour(*wxWHITE);
        font_color = *wxBLACK;
    }
    auto sizer = new wxBoxSizer(wxVERTICAL);
    if (type == RULE_HOVER_LINK) {
        auto title = new Label(this, Label::Body_13, _L("Point Consumption Rules"));
        title->SetForegroundColour(font_color);
        auto h = create_bmp_orders({"origin_text", "text_to_text", "text_to_img", "processed_text"}, 
                                   {_L("Text optimization"), _L("Generate image"), _L("Generate model")},
                                   {wxString::Format(wxT("%d ") + _L("Points"), g_scoreRule->text_optimize_count), 
                                    wxString::Format(wxT("%d ") + _L("Points"), g_scoreRule->text_trans_image_count),
                                    wxString::Format(wxT("%d ") + _L("Points"), g_scoreRule->image_generate_count)});
        sizer->AddSpacer(FromDIP(20));
        sizer->Add(title, 0, wxALIGN_CENTER, 0);
        sizer->AddSpacer(FromDIP(13));
        sizer->Add(h, 0, wxLEFT, FromDIP(32));
        sizer->AddSpacer(FromDIP(22));
        SetSize(FromDIP(680), -1);
        SetMinSize(wxSize(FromDIP(680), -1));
    } else {
        auto title = new Label(this, Label::Body_13, _L("What we will do?"));
        title->SetForegroundColour(font_color);
        auto info = new Label(this, Label::Body_11,
                              _L("We will preprocess the image, including but not limited"
                                 " to removing the background, reducing complexity, and e"
                                 "nhancing colors, to ensure higher-quality model generation results"));
        info->SetForegroundColour(font_color);
        info->Wrap(FromDIP(438));
        auto tips = new Label(this, Label::Body_11,
                              _L("Based on our tests, the generation results are excellent. "
                                 "We highly recommend enabling this feature!"));
        tips->SetForegroundColour(font_color);
        tips->Wrap(FromDIP(438));

        sizer->AddSpacer(FromDIP(20));
        sizer->Add(title, 0, wxALIGN_CENTER, 0);
        sizer->AddSpacer(FromDIP(6));
        sizer->Add(info, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT, FromDIP(16));
        sizer->AddSpacer(13);
        auto h1 = create_bmp_orders({"origin_image", "unprocess_image"}, {_L("Generate without preprocessing")}, {""});
        sizer->Add(h1, 0, wxLEFT, FromDIP(16));
        sizer->AddSpacer(FromDIP(13));
        auto h2 = create_bmp_orders({"origin_image", "processing_image", "processed_image"}, {_L("Image preprocessing"), _L("Generate model")}, {"", ""});
        sizer->Add(h2, 0, wxLEFT, FromDIP(16));
        sizer->AddSpacer(FromDIP(6));
        sizer->Add(tips, 0, wxLEFT, FromDIP(16));
        sizer->AddSpacer(FromDIP(16));
        if (type == HOVER_LINK) {
            auto line = new wxPanel(this, wxID_ANY);
            line->SetBackgroundColour(*wxWHITE);
            line->SetMinSize(wxSize(-1, FromDIP(1)));
            line->SetMaxSize(wxSize(-1, FromDIP(1)));
            sizer->Add(line, 0, wxLEFT | wxRIGHT | wxEXPAND, FromDIP(16));
            sizer->AddSpacer(FromDIP(16));
            auto rule_title = new Label(this, Label::Body_13, _L("Point Consumption Rules"));
            rule_title->SetForegroundColour(font_color);
            sizer->Add(rule_title, 0, wxALIGN_CENTER, 0);
            sizer->AddSpacer(FromDIP(13));
            auto h3 = create_bmp_orders({"origin_image", "processing_image", "processed_image"},
                                        {_L("Image preprocessing"), _L("Generate model")},
                                        {wxString::Format(wxT("%d ") + _L("Points"), g_scoreRule->image_process_count),
                                         wxString::Format(wxT("%d ") + _L("Points"), g_scoreRule->image_generate_count)});

            sizer->Add(h3, 0, wxLEFT, FromDIP(16));
            sizer->AddSpacer(FromDIP(19));
        } else {
            auto h_btns  = new wxBoxSizer(wxHORIZONTAL);
            m_cancel_btn = new FFButton(this, wxID_ANY, _L("Not Now"), FromDIP(4));
            m_cancel_btn->SetMinSize(wxSize(FromDIP(182), FromDIP(35)));
            m_cancel_btn->SetBGUniformColor(*wxWHITE);
            m_cancel_btn->SetBorderColor(wxColor("#419488"));
            m_cancel_btn->SetFontColor(wxColor("#419488"));
            m_cancel_btn->SetBorderHoverColor(wxColor("#65A79E"));
            m_cancel_btn->SetFontHoverColor(wxColor("#65A79E"));
            m_cancel_btn->SetBorderPressColor(wxColor("#1A8676"));
            m_cancel_btn->SetFontPressColor(wxColor("#1A8676"));
            m_cancel_btn->SetFont(Label::Body_13);
            m_confirm_btn = new FFButton(this, wxID_ANY, _L("Enable"), FromDIP(4), false);
            m_confirm_btn->SetMinSize(wxSize(FromDIP(182), FromDIP(35)));
            m_confirm_btn->SetFontUniformColor(*wxWHITE);
            m_confirm_btn->SetBGColor(wxColor("#419488"));
            m_confirm_btn->SetBGHoverColor(wxColor("#65A79E"));
            m_confirm_btn->SetBGPressColor(wxColor("#1A8676"));
            m_confirm_btn->SetFont(Label::Body_13);
            h_btns->Add(m_cancel_btn, 0, wxALL, 0);
            h_btns->AddSpacer(FromDIP(16));
            h_btns->Add(m_confirm_btn, 0, wxALL, 0);
            sizer->Add(h_btns, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT, FromDIP(16));
            sizer->AddSpacer(FromDIP(19));
        }
        SetSize(FromDIP(470), -1);
        SetMinSize(wxSize(FromDIP(470), -1));
        SetMaxSize(wxSize(FromDIP(470), -1));
    }

    sizer->Fit(this);
    SetSizer(sizer);
    Layout();
    Center();
}

ModelHoverWindow* ImageWhatDoingPanel::createPopup(wxWindow* parent, DlgType type)
{
    ModelHoverWindow*   popup = new ModelHoverWindow(parent);
    ImageWhatDoingPanel* panel = new ImageWhatDoingPanel(popup, type);
    auto                 sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(panel, 0, wxALL, 0);
    popup->SetSizerAndFit(sizer);
    popup->Layout();
    popup->Center();
    return popup; 
}

ModelBaseDialog* ImageWhatDoingPanel::createDialog(wxWindow* parent)
{
    ModelBaseDialog*     dlg   = new ModelBaseDialog(parent);
    ImageWhatDoingPanel* panel = new ImageWhatDoingPanel(dlg, FIRST_DLG);
    auto                 sizer = new wxBoxSizer(wxVERTICAL);
    sizer->SetMinSize(wxSize(dlg->FromDIP(530), -1));
    sizer->AddSpacer(dlg->FromDIP(10));
    sizer->Add(panel, 0, wxALIGN_CENTER, 0);
    sizer->AddSpacer(dlg->FromDIP(10));
    panel->getCancelButton()->Bind(wxEVT_BUTTON, [=](wxCommandEvent& event) { 
        dlg->EndModal(wxID_CANCEL);
    });
    panel->getConfirmButton()->Bind(wxEVT_BUTTON, [=](wxCommandEvent& event) { 
        dlg->EndModal(wxID_OK); 
    });

    dlg->SetSizerAndFit(sizer);
    dlg->Layout();
    dlg->Center();
    return dlg;
}

FFButton* ImageWhatDoingPanel::getCancelButton() { return m_cancel_btn; }

FFButton* ImageWhatDoingPanel::getConfirmButton() { return m_confirm_btn; }

wxBoxSizer* ImageWhatDoingPanel::create_bmp_orders(const std::vector<std::string>& bmp_lists, 
                                  const std::vector<wxString>& up_text_lists, 
                                  const std::vector<wxString>& down_text_lists)
{
    if (bmp_lists.size() - up_text_lists.size() != 1 || 
        up_text_lists.size() != down_text_lists.size()) {
        return nullptr;
    }
    wxColour          font_color;
    const std::string arrow_bmp = m_type != FIRST_DLG ? "arrow" : "arrow_dark";
    if (m_type != FIRST_DLG) {
        font_color = *wxWHITE;
    } else {
        font_color = *wxBLACK;
    }
    const int sper = FromDIP(10);
    auto h     = new wxBoxSizer(wxHORIZONTAL);
    for (int i = 0; i < up_text_lists.size(); i++) {
        auto bmp      = new wxStaticBitmap(this, wxID_ANY, m_bmp_map[bmp_lists[i]].bmp());
        auto v_sizer     = new wxBoxSizer(wxVERTICAL);
        auto arrow = new wxStaticBitmap(this, wxID_ANY, m_bmp_map[arrow_bmp].bmp());
        if (!up_text_lists[i].IsEmpty()) {
            auto text = new Label(this, Label::Body_10, up_text_lists[i], wxALIGN_CENTER);
            text->SetForegroundColour(font_color);
            text->Wrap(FromDIP(80));
            v_sizer->Add(text, 0, wxALL | wxALIGN_CENTER, 0);
        }
        v_sizer->Add(arrow, 0, wxALL, 0);
        if (!down_text_lists[i].IsEmpty()) {
            auto text1 = new Label(this, Label::Body_10, down_text_lists[i], wxALIGN_CENTER);
            text1->SetForegroundColour(font_color);
            text1->Wrap(FromDIP(80));
            v_sizer->Add(text1, 0, wxALL | wxALIGN_CENTER, 0);
        }
        h->Add(bmp, 0, wxALL, 0);
        h->AddSpacer(sper);
        h->Add(v_sizer, 0, wxUP | wxDOWN | wxALIGN_CENTER, 0);
        h->AddSpacer(sper);
    }
    auto bmp = new wxStaticBitmap(this, wxID_ANY, m_bmp_map[bmp_lists[bmp_lists.size() - 1]].bmp());
    h->Add(bmp, 0, wxALL, 0);
    return h;
}

ImageQuestionDialog::ImageQuestionDialog(wxWindow* parent) : ModelHoverWindow(parent)
{
    SetSize(FromDIP(320), -1);
    SetMinSize(wxSize(FromDIP(320), -1));
    SetBackgroundColour(wxColour("#333333"));
    auto title = new Label(this, Label::Body_14, _L("Image Upload Tips"));
    title->SetForegroundColour(*wxWHITE);
    m_info = new Label(this, Label::Body_12, "");
    m_info->SetForegroundColour(*wxWHITE);
    auto text1   = new Label(this, Label::Body_12, _L("Simple background (preferably solid color)"));
    text1->SetForegroundColour(*wxWHITE);
    auto text2   = new Label(this, Label::Body_12, _L("No text included"));
    text2->SetForegroundColour(*wxWHITE);
    auto text3   = new Label(this, Label::Body_12, _L("Single model"));
    text3->SetForegroundColour(*wxWHITE);
    auto text4   = new Label(this, Label::Body_12, _L("The model should not be too small."));
    text4->SetForegroundColour(*wxWHITE);
    auto v_sizer = new wxBoxSizer(wxVERTICAL);
    v_sizer->AddSpacer(FromDIP(16));
    v_sizer->Add(title, 0, wxALIGN_CENTER, 0);
    v_sizer->AddSpacer(FromDIP(6));
    
    v_sizer->Add(m_info, 0, wxLEFT | wxRIGHT | wxEXPAND, FromDIP(18));
    v_sizer->AddSpacer(FromDIP(12));
    auto h_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto v_sizer0 = new wxBoxSizer(wxVERTICAL);
    text1->Wrap(FromDIP(168));
    v_sizer0->Add(text1, 0, wxALL, 0);
    v_sizer0->AddSpacer(FromDIP(6));
    text2->Wrap(FromDIP(168));
    v_sizer0->Add(text2, 0, wxALL, 0);
    v_sizer0->AddSpacer(FromDIP(6));
    text3->Wrap(FromDIP(168));
    v_sizer0->Add(text3, 0, wxALL, 0);
    v_sizer0->AddSpacer(FromDIP(6));
    text4->Wrap(FromDIP(168));
    v_sizer0->Add(text4, 0, wxALL, 0);
    auto       text_height = FromDIP(6) * 3;
    wxCoord    w, h;
    wxMemoryDC dc;
    dc.SetFont(Label::Body_12);
    dc.GetMultiLineTextExtent(text1->GetLabel(), &w, &h);
    text_height += h;
    dc.GetMultiLineTextExtent(text2->GetLabel(), &w, &h);
    text_height += h;
    dc.GetMultiLineTextExtent(text3->GetLabel(), &w, &h);
    text_height += h;
    dc.GetMultiLineTextExtent(text4->GetLabel(), &w, &h);
    text_height += h;
    ScalableBitmap bmp(this, "question_tip_image", ToDIP(text_height));
    auto           img = new wxStaticBitmap(this, wxID_ANY, bmp.bmp());
    h_sizer->Add(img, 0, wxEXPAND | wxALL, 0);
    h_sizer->AddSpacer(FromDIP(16));
    h_sizer->Add(v_sizer0, 0, wxALL, 0);
    v_sizer->Add(h_sizer, 0, wxLEFT | wxRIGHT | wxEXPAND, FromDIP(18));
    v_sizer->AddSpacer(FromDIP(25));
    SetSizer(v_sizer);
    SetProcessed(false, true);
}

void ImageQuestionDialog::SetProcessed(bool processed, bool init)
{
    if (!init && m_processed == processed) {
        return;
    }
    m_processed = processed;
    wxString str;
    if (processed) {
        str = _L("It supports PNG, JPG, JPEG. Images should be no larger than 6MB with a minimum resolution of 128*128.") +
              _L("Uploaded images must be square (same width and height).");
    } else {
        str = _L("It supports PNG, JPG, JPEG. Images should be no larger than 6MB with a minimum resolution of 128*128.");
    }
    m_info->SetLabel(str);
    m_info->Wrap(FromDIP(282));
    Fit();
    Layout();
    //if (!init) {
    //    Refresh();
    //}
}

ApiLoadingIcon::ApiLoadingIcon(wxDialog* parent) : wxEvtHandler()
{
    this->m_parent = parent;
    m_timer  = new wxTimer(this);
    Bind(wxEVT_TIMER, &ApiLoadingIcon::OnTimer, this);
    for (int i = 0; i < 4; i++) {
        auto           str = boost::format("api_loading_%1%") % (i + 1);
        ScalableBitmap bmp(parent, str.str(), 60);
        m_loadingIcons.emplace_back(std::move(bmp));
    }
}

void ApiLoadingIcon::paintInRect(wxGraphicsContext* gc, wxRect rect)
{
    wxBitmap& bmp = m_loadingIcons[m_loadingIdx].bmp();
    gc->DrawBitmap(bmp, rect.x, rect.y, rect.width, rect.height);
}

void ApiLoadingIcon::Loading(int interval)
{
    if (m_timer->IsRunning()) {
        m_timer->Stop();
    }
    m_loadingIdx  = 0;
    m_loadingTime = 0;
    m_timer->Start(interval);
}

void ApiLoadingIcon::End()
{
    if (m_timer->IsRunning()) {
        m_timer->Stop();
    }
}

bool ApiLoadingIcon::isLoading() { return m_timer->IsRunning(); }

void ApiLoadingIcon::OnTimer(wxTimerEvent& event)
{
    m_loadingTime++;
    m_loadingIdx = (m_loadingIdx + 1) % m_loadingIcons.size();
    this->QueueEvent(new wxCommandEvent(EVT_UPDATE_ICON));
}

ModelApiTask::ModelApiTask(wxEvtHandler* parent) : 
    wxEvtHandler(), m_sem(1) 
{
    m_parent = parent;
    m_isFinish.store(false);
}

void ModelApiTask::setThreadFunc(std::function<void()> func) 
{ 
    this->m_func = func; 
}

wxSemaphore& ModelApiTask::Sem() { 
    return m_sem; 
}

std::mutex& ModelApiTask::Lock()
{ 
    return m_lock; 
}

wxEvtHandler* ModelApiTask::Parent() { return m_parent; }

void ModelApiTask::safeFunc(std::function<void()> func) 
{ 
    std::lock_guard<std::mutex> lock(m_lock);
    if (!m_isFinish.load()) func();
}

std::atomic_bool& ModelApiTask::FinishLoop()
{ 
    return m_isFinish; 
}

void ModelApiTask::start() 
{
    std::thread([self = shared_from_this()]() {
        self->m_func();
        self->safeFunc([self]() {
            auto e = new wxCommandEvent(EVT_FINISH_TASK);
            //e->SetString(id);
            wxQueueEvent(self->m_parent, e);
        });
    }).detach();
}

FFTextCtrl::FFTextCtrl(wxWindow* parent, wxString text, wxSize size, int style, wxString hint) : 
    wxTextCtrl(parent, wxID_ANY, text, wxDefaultPosition, size, style)
{
    SetDoubleBuffered(true);
    SetTextHint(hint);
    Bind(wxEVT_PAINT, &FFTextCtrl::OnPaint, this);
    m_length_label = new Label(this, "0/150");
    m_length_label->SetFont(Label::Body_10);
    m_length_label->SetForegroundColour("#999999");
    m_length_label->SetBackgroundColour(*wxWHITE);
    auto sizer = new wxBoxSizer(wxVERTICAL);
    sizer->AddStretchSpacer();
    sizer->Add(m_length_label, 0, wxALIGN_RIGHT | wxRIGHT, FromDIP(16));
    sizer->AddSpacer(FromDIP(16));
    SetSizer(sizer);
    sizer->Fit(this);
    Layout();
    Bind(wxEVT_TEXT, [=](wxCommandEvent& event) { 
        FFTextCtrl* textCtrl = dynamic_cast<FFTextCtrl*>(event.GetEventObject());
        if (!textCtrl) {
            return;
        }
        auto text = textCtrl->GetValue();
        if (text.Length() > m_max_length) {
            textCtrl->ChangeValue(m_old_text.Mid(0, wxMin(m_max_length, m_old_text.Length())));
            textCtrl->SetInsertionPointEnd();
            event.Skip();
            return;
        }
        auto str  = wxString::Format(wxT("%d/%d"), text.Length(), m_max_length);
        m_length_label->SetLabel(str);
        m_old_text = textCtrl->GetValue();
        event.Skip();
    });
    Bind(wxEVT_TEXT_PASTE, [=](wxCommandEvent& event) {
        wxTextCtrl* textCtrl = dynamic_cast<wxTextCtrl*>(event.GetEventObject());
        if (!textCtrl) {
            return;
        }

        if (wxTheClipboard->Open()) {
            wxTextDataObject data;
            if (wxTheClipboard->GetData(data)) {
                wxString pastedText = data.GetText();
                auto     old_size   = textCtrl->GetValue().Length();
                auto     ins        = pastedText.Mid(0, wxMin(pastedText.size(), m_max_length - old_size));
                int pos             = textCtrl->GetInsertionPoint();
                textCtrl->WriteText(ins);
                textCtrl->SetInsertionPoint(pos + ins.Length());
            }
            wxTheClipboard->Close();
        }
        event.Skip(false);
    });
}

void FFTextCtrl::SetTextHint(const wxString& hint) 
{ 
    m_hint = hint;
    Refresh();
}

void FFTextCtrl::SetMaxBytes(int max_length) { 
    m_max_length = max_length; 
    auto str     = wxString::Format(wxT("%d/%d"), GetValue().ToStdString().size(),
        m_max_length);
    m_length_label->SetLabel(str);
}

void FFTextCtrl::OnPaint(wxPaintEvent& event) 
{
    wxPaintDC dc(this);
    auto      size = GetClientSize();
    if (GetValue().IsEmpty()) {
        wxBitmap   bitmap(size);
        wxMemoryDC memDC;
        memDC.SelectObject(bitmap);
        memDC.SetFont(GetFont());
        wxString sstr;
        const int hint_sper = FromDIP(5);
        Label::split_lines(memDC, size.x - hint_sper * 2, m_hint, sstr);
        boost::algorithm::split(m_vs, sstr.utf8_string(), boost::is_any_of("\n"));
        memDC.SelectObject(wxNullBitmap);
        dc.SetTextForeground(wxColour(150, 150, 150));
        dc.SetFont(GetFont());
        for (int i = 0; i < m_vs.size(); i++) {
            auto text_size = dc.GetTextExtent(wxString::FromUTF8(m_vs[i]));
            dc.DrawText(wxString::FromUTF8(m_vs[i]), hint_sper, i * text_size.y);
        }
        return;
    }
    event.Skip();
}

bool ImageUploadPanel::judgeTransImage(wxString& path)
{
    if (path.IsEmpty()) {
        return false;
    }
    fstream fs;
    fs.open(path.ToStdString(), ios::binary | ios::in);
    fs.seekg(0, ios::end);
    int size = fs.tellg();
    if (size == -1) {
        GUI::show_error(this, _L("Failed to load image"));
        return false;
    }
    if (size > 6 * 1024 * 1024) {
        GUI::show_error(this, _L("Maximum image size: ") + "6MB");
        return false;
    }
    fs.close();
    string  buf;
    wxImage img;
    bool    flag = img.LoadFile(wxString::FromUTF8(path.utf8_string()), wxBITMAP_TYPE_ANY);
    if (!flag) {
        GUI::show_error(this, _L("Failed to load image"));
        return false;
    }

    if (m_processed) {
        if (img.GetWidth() != img.GetHeight()) {
            GUI::show_error(this, _L("Uploaded images must be square (same width and height)."));
            return false;
        }
        return true;
    }

    int    min_size     = min(img.GetHeight(), img.GetWidth());
    int    max_size     = max(img.GetHeight(), img.GetWidth());
    if (min_size < 128) {
        GUI::show_error(this, _L("Minimum image resolution: 128*128"));
        return false;
    }
    if (max_size > 5000) {
        GUI::show_error(this, _L("Maximum image resolution: 5000*5000"));
        return false;
    }
    return true;
}

bool ImageUploadPanel::compressImage(const wxString& inpath, wxString& outpath)
{
    wxImage image;
    if (!image.LoadFile(inpath)) {
        return false;
    }

    const int maxDimension = 1920;
    int       origWidth    = image.GetWidth();
    int       origHeight   = image.GetHeight();
    double    scale        = 1.0;

    if (origWidth > maxDimension || origHeight > maxDimension) {
        scale         = maxDimension / static_cast<double>(std::max(origWidth, origHeight));
        int newWidth  = static_cast<int>(origWidth * scale);
        int newHeight = static_cast<int>(origHeight * scale);
        image.Rescale(newWidth, newHeight, wxIMAGE_QUALITY_HIGH);
    }

    if (!image.HasAlpha()) {
        image.InitAlpha();
    }

    wxFileOutputStream output(outpath);
    if (!output.IsOk()) {
        return false;
    }
    return image.SaveFile(output, wxBITMAP_TYPE_PNG);
}

ImageUploadPanel::ImageUploadPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    SetMinSize(FromDIP(wxSize(518, 344)));
    SetMaxSize(FromDIP(wxSize(518, 344)));
    SetSize(FromDIP(wxSize(518, 344)));
    SetDoubleBuffered(true);
    m_upload_icon = ScalableBitmap(this, "model_api_upload_image", 24);
    m_delete_icon = ScalableBitmap(this, "model_api_delete_image", 32);
    SetProcessed(false, true);
    Bind(wxEVT_PAINT, &ImageUploadPanel::onPaint, this);
    Bind(wxEVT_LEFT_DOWN, &ImageUploadPanel::onLeftDown, this);
    Bind(wxEVT_LEFT_UP, &ImageUploadPanel::onLeftUp, this);
    Bind(wxEVT_ENTER_WINDOW, &ImageUploadPanel::OnMouseEnter, this);
    Bind(wxEVT_LEAVE_WINDOW, &ImageUploadPanel::OnMouseLeave, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &ImageUploadPanel::onMouseCaptureLost, this);
}

wxString ImageUploadPanel::getPath() { return m_path; }

void ImageUploadPanel::SetProcessed(bool processed, bool init) 
{ 
    if (!init && m_processed == processed) {
        return;
    }
    m_processed = processed;
    wxString str;
    if (m_processed) {
        str = _L("It supports PNG, JPG, JPEG. Images should be no larger than 6MB with a minimum resolution of 128*128.") + 
            _L("Uploaded images must be square (same width and height).");
    } else {
        str = _L("It supports PNG, JPG, JPEG. Images should be no larger than 6MB with a minimum resolution of 128*128."); 
    }
    {
        m_vs.clear();
        wxBitmap   bitmap(GetSize());
        wxMemoryDC memDC;
        memDC.SelectObject(bitmap);
        memDC.SetFont(Label::Body_10);
        wxString sstr;
        Label::split_lines(memDC, FromDIP(400), str, sstr);
        boost::algorithm::split(m_vs, sstr.utf8_string(), boost::is_any_of("\n"));
        memDC.SelectObject(wxNullBitmap);
    }
    m_path = "";
    if (m_img.IsOk()) {
        m_img.Clear();
    }
    if (!init) {
        Refresh();
    }
}

wxDialog* ImageUploadPanel::HasDlg() { 
    return m_file_dialog; 
}

void ImageUploadPanel::onPaint(wxPaintEvent& event) 
{
    auto size = GetClientSize();
    wxPaintDC                          dc(this);
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (gc == nullptr) {
        return;
    }
    gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
    if (m_path.empty() || !m_img.IsOk()) {
        gc->SetBrush(wxBrush(*wxWHITE));
        gc->DrawRectangle(0, 0, size.x, size.y);
        gc->DrawBitmap(m_upload_icon.bmp(), (size.x - m_upload_icon.GetBmpWidth()) / 2, FromDIP(142), m_upload_icon.GetBmpWidth(), m_upload_icon.GetBmpHeight());
        dc.SetFont(Label::Body_12);
        dc.SetTextForeground(wxColor("#328DF8"));
        auto text_size0 = dc.GetTextExtent(_L("Please upload the image."));
        dc.DrawText(_L("Please upload the image."), (size.x - text_size0.x) / 2, FromDIP(184));
        dc.SetFont(Label::Body_10);
        dc.SetTextForeground(wxColor("#B3B3B3"));
        int text_y = size.y - m_vs.size() * dc.GetTextExtent("0").y - FromDIP(32);
        for (int i = 0; i < m_vs.size(); i++){
            auto text_size = dc.GetTextExtent(wxString::FromUTF8(m_vs[i]));
            dc.DrawText(wxString::FromUTF8(m_vs[i]), (size.x - text_size.x) / 2, text_y+ i * text_size.y);
        }
    }
    else {
        gc->SetBrush(wxBrush(*wxBLACK));
        gc->DrawRectangle(0, 0, size.x, size.y);
        auto rect = FFUtils::calcContainedRect(size, m_img.GetSize(), false);
        gc->DrawBitmap(m_img, rect.x, rect.y, rect.width, rect.height);
        if (m_isHovered) {    
            gc->SetBrush(wxBrush(wxColour(0, 0, 0, 128)));
            gc->DrawRectangle(0, 0, size.x, size.y);
            gc->DrawBitmap(m_delete_icon.bmp(), (size.x - m_delete_icon.GetBmpWidth()) / 2, (size.y - m_delete_icon.GetBmpHeight()) / 2,
                           m_delete_icon.GetBmpWidth(), m_delete_icon.GetBmpHeight());
        }
    }
}

void ImageUploadPanel::onLeftDown(wxMouseEvent& event) 
{
    m_isGeneratePressed = true;
    if (!HasCapture()) {
        CaptureMouse();
    }
    event.Skip();
}

void ImageUploadPanel::onLeftUp(wxMouseEvent& event) 
{
    if (!m_isGeneratePressed) {    
        event.Skip();
        return;
    }
    m_isGeneratePressed = false;
    if (HasCapture()) {
        ReleaseMouse();
    }
    
    if (!this->IsEnabled()) {
        event.Skip();
        return;
    }

    bool isFunc = true;
    if (m_path.empty() || !m_img.IsOk()) {
        const char*   filter_str = "*.jpeg;*jpg;*.png";
        m_file_dialog = new wxFileDialog(this, _L("Select Image"), wxGetApp().app_config->get_last_dir(), "",
                          wxString::Format(_L("Image files") + wxT(" (%s)|%s"), filter_str, filter_str), 
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        wxArrayString files;
        if (m_file_dialog->ShowModal() != wxID_OK) {
            event.Skip();
            return;
        }
        wxGetApp().app_config->update_skein_dir(m_file_dialog->GetDirectory().utf8_string());
        m_file_dialog->GetPaths(files);
        delete m_file_dialog;
        m_file_dialog = nullptr;
        m_path = files[0];
        if (judgeTransImage(m_path)) {
            if (!m_img.LoadFile(m_path, wxBITMAP_TYPE_ANY)) {
                GUI::show_error(this, _L("Image Load Failed"));
                isFunc = false;
            }
        }
        else {
            m_path = "";
            isFunc = false;
        }
        if (m_processed) {
            fs::path file_path(m_path.utf8_string());
            wxString outpath = wxString::FromUTF8(ModelApiDialog::GetDir() + "/" + 
                file_path.stem().string() + ".png");
            if (!compressImage(m_path, outpath)) {
                m_path = "";
                isFunc = false;
            } else {
                m_path = outpath;
            }
        }

    }
    else {
        m_path = "";
        m_img.Clear();
    }

    if (isFunc) {
        Refresh();
        wxQueueEvent(this, new wxCommandEvent(EVT_LOADED_IMAGE));
    }
    event.Skip();
}

void ImageUploadPanel::onMouseCaptureLost(wxMouseCaptureLostEvent& event) 
{
    m_isGeneratePressed = false;
    Refresh();
    event.Skip();
}

void ImageUploadPanel::OnMouseEnter(wxMouseEvent& event) 
{
    m_isHovered = true;
    SetCursor(wxCURSOR_HAND);
    Refresh();
    event.Skip();
}

void ImageUploadPanel::OnMouseLeave(wxMouseEvent& event) 
{
    m_isHovered = false;
    SetCursor(wxCURSOR_ARROW);
    Refresh();
    event.Skip();
}

ModelBaseDialog::ModelBaseDialog(wxWindow* parent) : FFTitleLessDialog(parent)
{
    Bind(EVT_LOGOUT_USER, [=](wxCommandEvent& event) { 
        EndModal(m_res);
        event.Skip();
    });
    MultiComMgr::inst()->Bind(COM_WAN_DEV_MAINTAIN_EVENT, &ModelBaseDialog::bindConnEvent, this);
}

void ModelBaseDialog::EndModal(int retCode)
{
    if (m_needClose) {
        FFTitleLessDialog::EndModal(retCode);
        return;
    }
    m_needClose = true;
    m_res       = retCode;
    if (m_msg) {
        if (m_offline) {
            m_msg->EndModal(wxID_OK);
        } else {
            m_msg->EndModal(m_msg_res);
        }
        return;
    }
    FFTitleLessDialog::EndModal(retCode);
};

void ModelBaseDialog::drawBackground(wxBufferedPaintDC& dc, wxGraphicsContext* gc) 
{
    gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
    gc->SetBrush(*wxWHITE);
    gc->DrawRectangle(0, 0, GetClientSize().x, GetClientSize().y);
}

ModelBaseDialog::~ModelBaseDialog() 
{ 
    MultiComMgr::inst()->Unbind(COM_WAN_DEV_MAINTAIN_EVENT, &ModelBaseDialog::bindConnEvent, this); 
}

void ModelBaseDialog::BindMsgDialog(wxDialog* dlg) 
{
    dlg->Bind(wxEVT_SHOW, [=](wxShowEvent& event) { 
        if (event.IsShown()) {
            m_msg = dlg;  
        } else {
            m_msg = nullptr;
        }
        event.Skip();
    });
    dlg->Bind(wxEVT_DESTROY, [=](wxWindowDestroyEvent& event) { 
        if (m_offline || m_needClose) {
            GetEventHandler()->AddPendingEvent(wxCommandEvent(EVT_LOGOUT_USER));
        } 
        event.Skip();
    });
}

void ModelBaseDialog::bindConnEvent(ComWanDevMaintainEvent& event)
{
    if (event.login) {
        event.Skip();
        return;
    }
    if (!IsShown()) {
        event.Skip();
        return;
    }
    if (!m_offline) {
        m_offline = true;
        if (!m_msg) {
            Close(true);
        }
    }
    event.Skip();
}

void ModelApiDialog::updateCustomModelDir() 
{
    wxString dir_path = wxString::FromUTF8(data_dir()) + "/AiModel";
    wxDir    dir(dir_path);
    if (!dir.IsOpened()) {
        bool ret = wxFileName::Mkdir(dir_path, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
        if (!ret) {
            BOOST_LOG_TRIVIAL(error) << "Create Directory Failed: " << dir_path.utf8_string();
            m_dir_path = wxStandardPaths::Get().GetTempDir().utf8_string();
            return;
        }
    }
    m_dir_path = dir_path.utf8_string();

    wxDateTime cutoff = wxDateTime::Now();
    cutoff.Subtract(wxDateSpan::Month());

    wxString filename;
    bool     cont = dir.GetFirst(&filename, wxEmptyString, wxDIR_FILES);
    while (cont) {
        wxFileName file(dir_path, filename);
        if (file.FileExists()) {
            wxDateTime modTime = file.GetModificationTime();
            if (modTime.IsValid() && modTime.IsEarlierThan(cutoff)) {
                if (!wxRemoveFile(file.GetFullPath())) {
                    BOOST_LOG_TRIVIAL(error) << "Delete File Failed: " << file.GetFullPath();
                }
            }
        }
        cont = dir.GetNext(&filename);
    }
}

const std::string& ModelApiDialog::GetDir()
{ 
    return m_dir_path; 
}

ModelApiDialog::ModelApiDialog(wxWindow* parent)
    : 
    ModelBaseDialog(parent), m_generate_btn_rect(0, 0, 0, 0), 
    m_question_link_rect(0, 0, 0, 0), m_isGeneratePressed(false), 
    m_generateType(IMAGE_MODEL), 
    m_pretreat_link_rect(0, 0, 0, 0), m_pretreat_btn_rect(0, 0, 0, 0)
{
    this->SetSize(FromDIP(wxSize(589, 658)));
    this->SetMinSize(FromDIP(wxSize(589, 658)));
    this->SetDoubleBuffered(true);
    m_loadIcon        = std::make_shared<ApiLoadingIcon>(this);
    m_loadIcon->Bind(EVT_UPDATE_ICON, [=](wxCommandEvent& event) { 
        this->Refresh();
    });
    m_loadTask                 = std::make_shared<ModelApiTask>(this);
    m_loadTask->setThreadFunc([task = this->m_loadTask, scoreRule = g_scoreRule]() {
        com_user_ai_points_info_t data;
        auto                      ret = COM_OK;
        ret = MultiComHelper::inst()->getUserAiPointsInfo(data, TIMEOUT_LIMIT);
        if (ret != COM_OK) {
            task->safeFunc([task, ret]() {
                auto event = new wxCommandEvent(EVT_ERROR_MSG);
                event->SetString(_L("Network Error"));
                event->SetInt(1);
                wxQueueEvent(task->Parent(), event);
            });
            return;
        }
        std::string promoData;
        /*scoreRule->image_generate_count   = 30;
        scoreRule->image_process_count         = 20;
        scoreRule->image_real_generate_count   = 0;
        scoreRule->text_optimize_count         = 10;
        scoreRule->text_trans_image_count      = 15;
        scoreRule->total_count                 = 30;
        scoreRule->free_count                  = 3;
        scoreRule->reagain_free_count          = 3;
        scoreRule->isOk                        = true;*/
        scoreRule->image_generate_count        = data.modelGenPoints;
        scoreRule->image_process_count         = data.img2imgPoints;
        scoreRule->image_real_generate_count   = data.modelGenPoints;
        scoreRule->text_optimize_count         = data.txt2txtPoints;
        scoreRule->text_trans_image_count      = data.txt2imgPoints;
        scoreRule->total_count                 = data.totalPoints;
        scoreRule->free_count                  = data.remainingFreeCount;
        scoreRule->reagain_free_count          = data.freeRetriesPerProcess;
        scoreRule->isOk                        = true;
        com_ai_model_job_result_t res;
        //res.isOldJob = false;
        ret = MultiComHelper::inst()->getExistingAiModelJob(res, TIMEOUT_LIMIT);
        if (ret != COM_OK) {
            if (ret != COM_NO_EXISTING_AI_MODEL_JOB) {
                task->safeFunc([task, ret]() {
                    auto event = new wxCommandEvent(EVT_ERROR_MSG);
                    event->SetString(_L("Network Error"));
                    event->SetInt(1);
                    wxQueueEvent(task->Parent(), event);
                });
                return;
            }
        } else {
            if (res.isOldJob) {
                task->safeFunc([task, res]() {
                    auto event = new wxCommandEvent(EVT_OLD_TASK);
                    event->SetInt(res.jobId);
                    wxQueueEvent(task->Parent(), event);
                });
            }
        }
        task->safeFunc([task, data, promoData]() {
            auto event       = new FinishScoreEvent();
            event->promoData = promoData;
            wxQueueEvent(task->Parent(), event);
        });
    });
    Bind(EVT_ERROR_MSG, [=](wxCommandEvent& event) {
        MessageDialog edlg(this, event.GetString(), _L("Error"));
        BindMsgDialog(&edlg);
        edlg.ShowModal();
        if (event.GetInt() == 1) {
            Close();
        }
    });
    Bind(EVT_OLD_TASK, [=](wxCommandEvent& event) {
        MessageDialog dlg(this, _L("A model is currently being generated. Please wait."), _L("Warning"));
        BindMsgDialog(&dlg);
        dlg.ShowModal();
        m_old_job_id = event.GetInt();
        EndModal(wxID_LAST);
    });
    Bind(EVT_FINISH_SCORE, [=](FinishScoreEvent& event) { 
        this->RefreshScore();
        this->m_promoData = event.promoData;
        this->m_image_panel->Enable(true);
        m_what_doing_dialog = ImageWhatDoingPanel::createPopup(this);
        m_what_doing_dialog->Hide();
        m_rule_dialog     = ImageWhatDoingPanel::createPopup(this, ImageWhatDoingPanel::RULE_HOVER_LINK);
        m_rule_dialog->Hide();
    });
    m_loadTask->start();
    m_question_dialog          = new ImageQuestionDialog(this);
    m_question_dialog->Hide();
    m_bmp_map["bg"] = ScalableBitmap(this, "model_api_dlg_bg", ToDIP(GetSize().y));
    m_bmp_map["question_mark"] = ScalableBitmap(this, "model_api_question_mark", 12);
    m_bmp_map["sw_off"] = ScalableBitmap(this, "switch_button_disabled", 16);
    m_bmp_map["sw_on"] = ScalableBitmap(this, "switch_button_enabled", 16);

    auto sizer      = new wxBoxSizer(wxVERTICAL);
    m_image_panel              = new ImageUploadPanel(this);
    m_image_panel->Enable(false);
    m_image_panel->Bind(EVT_LOADED_IMAGE, [=](wxCommandEvent& event) { Refresh(); });
    m_text_panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(518), FromDIP(344)));
    m_text_panel->SetBackgroundColour(*wxWHITE);
    m_text_panel->SetMinSize(wxSize(FromDIP(518), FromDIP(344)));
    m_text_panel->SetMaxSize(wxSize(FromDIP(518), FromDIP(344)));
    auto btn    = new Button(m_text_panel, "");
    btn->SetMaxSize(wxSize(0, 0));
    btn->Hide();
    m_text_ctrl = new FFTextCtrl(m_text_panel, "", wxSize(FromDIP(482), FromDIP(308)), 
        wxBORDER_NONE | wxTE_MULTILINE | wxTE_NO_VSCROLL, _L("Please enter the content you want to generate. "
        "We recommend focusing on a single subject. For example: A brown cat sculpture with a curled tail, in"
        " a cartoon style"));
    m_text_ctrl->SetMinSize(wxSize(FromDIP(482), FromDIP(308)));
    m_text_ctrl->SetBackgroundColour(*wxWHITE);
    m_text_ctrl->SetFont(Label::Body_12);
    m_text_ctrl->SetMaxBytes(500);
    m_text_ctrl->Bind(wxEVT_TEXT, [=](wxCommandEvent& event) { 
        Refresh(); 
        event.Skip();
    });
    auto text_sizer = new wxBoxSizer(wxHORIZONTAL);
    text_sizer->AddSpacer(FromDIP(18));
    text_sizer->Add(m_text_ctrl, 0, wxALIGN_CENTER, 0);
    text_sizer->Add(btn, 0, wxALL, 0);
    text_sizer->AddSpacer(FromDIP(18));
    m_text_panel->SetSizer(text_sizer);
    text_sizer->Fit(m_text_panel);
    m_text_panel->Layout();

    sizer->AddSpacer(FromDIP(158));
    sizer->Add(m_image_panel, 0, wxALIGN_CENTER, 0);
    sizer->Add(m_text_panel, 0, wxALIGN_CENTER, 0);
    sizer->AddSpacer(FromDIP(156));
    sizer->Fit(this);
    SetSizer(sizer);
    m_text_panel->Hide();
    if (wxGetApp().app_config->get("model_default_text").empty()) {
        wxGetApp().app_config->set_bool("model_default_text", true); 
        changeModelType(TEXT_MODEL);
        btn->SetFocus();
    } else {
        auto b = wxGetApp().app_config->get_bool("model_default_text");
        changeModelType(b ? TEXT_MODEL : IMAGE_MODEL);
        btn->SetFocus();
    }
    if (wxGetApp().app_config->get("model_image_pretreat").empty()) {
        m_first_image = true;
        m_can_image_pretreat = true;
    } else {
        m_can_image_pretreat = wxGetApp().app_config->get_bool("model_image_pretreat");
    }
    m_image_panel->SetProcessed(false);
    Layout();
    Center();

    Bind(wxEVT_LEFT_DOWN, &ModelApiDialog::onLeftDown, this);
    Bind(wxEVT_LEFT_UP, &ModelApiDialog::onLeftUp, this);
    Bind(wxEVT_MOTION, &ModelApiDialog::OnMouseMove, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &ModelApiDialog::onMouseCaptureLost, this);
    Bind(wxEVT_SHOW, [=](wxShowEvent& event) { 
        if (event.IsShown() && m_first_image && m_generateType == IMAGE_MODEL) {
            CallAfter([=] {
                m_first_image = false;
                ModelBaseDialog* dlg = ImageWhatDoingPanel::createDialog(this);
                BindMsgDialog(dlg);
                int ret = dlg->ShowModal();
                if (ret == wxID_OK) {
                    m_can_image_pretreat = true;
                }
                else {
                    m_can_image_pretreat = false;
                }
                wxGetApp().app_config->set_bool("model_image_pretreat", m_can_image_pretreat);
                delete dlg;
                Refresh();
            });
            
        }
        event.Skip();
    });
    m_loadIcon->Loading(200);
}

wxString ModelApiDialog::getImage() { 
    return this->m_image_panel->getPath(); 
}

wxString ModelApiDialog::getText() {
    return this->m_text_ctrl->GetValue(); 
}

ModelType ModelApiDialog::getType() { return m_generateType; }

bool ModelApiDialog::IsImageProcess() 
{ 
    return m_can_image_pretreat; 
}

int ModelApiDialog::getOldJobId() { return m_old_job_id; }

void ModelApiDialog::drawBackground(wxBufferedPaintDC& dc, wxGraphicsContext* gc)
{
    FFTitleLessDialog::drawBackground(dc, gc);
    gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
    auto size = this->GetClientSize();
    gc->DrawBitmap(m_bmp_map["bg"].bmp(), 0, 0, size.x, size.y);
    drawCenterText(dc, gc, _L("AI 3D Model Generator"), FromDIP(48), Label::sysFont(16, true), wxColor("#333333"));
    const int type_center_sper = FromDIP(120);
    gc->SetPen(wxPen(wxColor("#C8C8C8"), 1));
    gc->StrokeLine(size.x / 2, FromDIP(91), size.x / 2, FromDIP(99));
    dc.SetFont(Label::Head_14);
    auto text_type_str = _L("Text to 3D Model");
    auto image_type_str = _L("Image to 3D Model");
    auto text_type_size = dc.GetTextExtent(text_type_str);
    auto image_type_size = dc.GetTextExtent(image_type_str);
    if (m_model_type_rects.empty()) {
        m_model_type_rects[TEXT_MODEL] = wxRect((size.x - text_type_size.x) / 2 - type_center_sper, FromDIP(89), text_type_size.x,
                                                FromDIP(20));
        m_model_type_rects[IMAGE_MODEL] = wxRect((size.x - image_type_size.x) / 2 + type_center_sper, FromDIP(89), image_type_size.x,
                                                FromDIP(20));
    }
    std::unordered_map<int, wxString> type_strs;
    type_strs[TEXT_MODEL] = text_type_str;
    type_strs[IMAGE_MODEL] = image_type_str;
    for (auto it : m_model_type_rects) {
        if (m_generateType == it.first) {
            dc.SetFont(Label::Head_14);
            dc.SetTextForeground(wxColor("#328DFB"));
            dc.DrawText(type_strs[it.first], it.second.x, it.second.y);
            gc->SetPen(wxPen(wxColor("#328DF8"), 2));
            gc->StrokeLine(it.second.x, it.second.y + it.second.height, it.second.x + it.second.width,
                           it.second.y + it.second.height);
        }
        else {
            dc.SetFont(Label::Body_14);
            dc.SetTextForeground(wxColor("#000000"));
            dc.DrawText(type_strs[it.first], it.second.x, it.second.y);
        }
    }
    gc->SetPen(*wxTRANSPARENT_PEN);
    if (m_generateType == IMAGE_MODEL) {
        dc.SetTextForeground(wxColor("#333333"));
        dc.SetFont(Label::Body_11);
        auto      text_size = dc.GetTextExtent(_L("Image Upload Tips"));
        auto      size      = this->GetClientSize();
        auto&     bmp       = m_bmp_map["question_mark"];
        const int icon_sper = 5;
        int       startPos  = m_image_panel->GetPosition().x;
        gc->DrawBitmap(bmp.bmp(), startPos, FromDIP(131), bmp.GetBmpWidth(), bmp.GetBmpHeight());
        dc.DrawText(_L("Image Upload Tips"), startPos + icon_sper + bmp.GetBmpWidth(), FromDIP(131));
        if (m_question_link_rect.IsEmpty()) {
            m_question_link_rect = wxRect(startPos, FromDIP(131), text_size.x + bmp.GetBmpWidth() + icon_sper, bmp.GetBmpHeight());
        }
        auto      pretreat_text_size = dc.GetTextExtent(_L("Image preprocessing"));
        auto&     pretreat_btn = m_can_image_pretreat ? m_bmp_map["sw_on"] : m_bmp_map["sw_off"];
        int       startPos0  = m_image_panel->GetPosition().x + m_image_panel->GetClientSize().x - 
            (icon_sper * 2 + pretreat_btn.GetBmpWidth() + bmp.GetBmpWidth() + pretreat_text_size.x);
        gc->DrawBitmap(bmp.bmp(), startPos0, FromDIP(131), bmp.GetBmpWidth(), bmp.GetBmpHeight());
        dc.DrawText(_L("Image preprocessing"), startPos0 + icon_sper + bmp.GetBmpWidth(), FromDIP(131));
        gc->DrawBitmap(pretreat_btn.bmp(), startPos0 + icon_sper * 2 + pretreat_text_size.x + bmp.GetBmpWidth(), 
            FromDIP(131) + (pretreat_text_size.y - pretreat_btn.GetBmpHeight()) / 2,
                       pretreat_btn.GetBmpWidth(), pretreat_btn.GetBmpHeight());
        if (m_pretreat_link_rect.IsEmpty()) {
            m_pretreat_link_rect = wxRect(startPos0, FromDIP(131), bmp.GetBmpWidth(), bmp.GetBmpHeight());
        }
        if (m_pretreat_btn_rect.IsEmpty()) {
            m_pretreat_btn_rect = wxRect(startPos0 + bmp.GetBmpWidth() + icon_sper, FromDIP(131), pretreat_text_size.x + icon_sper + 
                pretreat_btn.GetBmpWidth(), pretreat_btn.GetBmpHeight());
        }
    } 
    else if (m_generateType == TEXT_MODEL) {
        auto      size      = this->GetClientSize();
        auto&     bmp       = m_bmp_map["question_mark"];
        const int icon_sper = 5;
        int       startPos  = m_text_panel->GetPosition().x;
        dc.SetFont(Label::Head_12);
        dc.SetTextForeground(*wxBLACK);
        dc.DrawText(_L("Please upload your text"), startPos, FromDIP(131));
        auto rule_str = _L("Point Consumption Rules");
        dc.SetFont(Label::Body_11);
        auto rule_size = dc.GetTextExtent(rule_str);
        int  startPos0 = m_text_panel->GetPosition().x + m_text_panel->GetClientSize().x -
                        (icon_sper + bmp.GetBmpWidth() + rule_size.x);
        gc->DrawBitmap(bmp.bmp(), startPos0, FromDIP(131), bmp.GetBmpWidth(), bmp.GetBmpHeight());
        dc.SetTextForeground(wxColor("#333333"));
        dc.DrawText(rule_str, startPos0 + icon_sper + bmp.GetBmpWidth(), FromDIP(131));
        if (m_rule_link_rect.IsEmpty()) {
            m_rule_link_rect = wxRect(startPos0, FromDIP(131), rule_size.x + bmp.GetBmpWidth() + icon_sper, bmp.GetBmpHeight());
        }
    }
    if (m_loadIcon->isLoading()) {
        const int loadSize = FromDIP(40);
        m_loadIcon->paintInRect(gc, wxRect((size.x - loadSize) / 2, FromDIP(521), loadSize, loadSize));
    } else {
        drawCenterText(dc, gc, m_cost_text, FromDIP(521), Label::Body_12, wxColor("#333333"));
        drawCenterText(dc, gc, m_score_text, FromDIP(544), Label::Body_12, wxColor("#419488"));
    }
    if (m_loadIcon->isLoading() || ((m_image_panel->getPath().empty() && m_generateType == IMAGE_MODEL) ||
        (m_text_ctrl->GetValue().empty() && m_generateType == TEXT_MODEL))) {
        gc->SetBrush(wxColor("#D2D2D2"));
    }
    else {
        gc->SetBrush(wxColor("#419488"));
        if (m_isGenerateHovered) {
            gc->SetBrush(wxColor("#65A79E"));
        }
        if (m_isGeneratePressed) {
            gc->SetBrush(wxColor("#1A8676"));
        }
    }
    wxString btn_text(_L("Start generating"));
    dc.SetFont(Label::Body_12);
    auto btn_text_size = dc.GetTextExtent(btn_text);
    wxSize btn_size(FromDIP(518), FromDIP(30));
    if (m_generate_btn_rect.IsEmpty()) {
        m_generate_btn_rect = wxRect((size.x - btn_size.x) / 2, FromDIP(580), btn_size.x, btn_size.y);
    }
    gc->DrawRoundedRectangle((size.x - btn_size.x) / 2, FromDIP(580), btn_size.x, btn_size.y, 4);
    dc.SetTextForeground(*wxWHITE);
    dc.DrawText(btn_text, (size.x - btn_text_size.x) / 2, FromDIP(586));
}

ModelApiDialog::~ModelApiDialog() 
{ 
    wxEventBlocker              block(this);
    std::lock_guard<std::mutex> lock(m_loadTask->Lock());
    m_loadTask->FinishLoop().store(true);
    m_loadTask.reset();
}

void ModelApiDialog::drawCenterText(wxBufferedPaintDC& dc, wxGraphicsContext* gc, const wxString& str, int height, const wxFont& font, wxColour color, wxString iconName /* = "" */)
{ 
    dc.SetFont(font);
    auto text_size = dc.GetTextExtent(str);
    auto size = this->GetClientSize();
    dc.SetTextForeground(color);
    if (iconName.empty()) {
        dc.DrawText(str, (size.x - text_size.x) / 2, height);
    }
    else {
        auto& bmp = m_bmp_map[iconName.utf8_string()];
        const int icon_sper = 5;
        gc->DrawBitmap(bmp.bmp(), (size.x - text_size.x - bmp.GetBmpWidth() - icon_sper) / 2, height, bmp.GetBmpWidth(), bmp.GetBmpHeight());
        dc.DrawText(str, (size.x - text_size.x - bmp.GetBmpWidth() - icon_sper) / 2 + icon_sper + bmp.GetBmpWidth(), height);
        if (iconName == "question_mark" && m_question_link_rect.IsEmpty()) {
            m_question_link_rect = wxRect((size.x - text_size.x - bmp.GetBmpWidth() - icon_sper) / 2, height, 
                text_size.x + bmp.GetBmpWidth() + icon_sper, bmp.GetBmpHeight());
        }
    }
}

void ModelApiDialog::onLeftDown(wxMouseEvent& event) 
{
    if (!m_generate_btn_rect.IsEmpty() && m_generate_btn_rect.Contains(event.GetPosition())) {
        m_isGeneratePressed = true;
        Refresh();
        if (!HasCapture()) {
            CaptureMouse();
        }
        event.Skip();
        return;
    }
    if (m_generateType == IMAGE_MODEL) {
        if (!m_model_type_rects.empty() && m_model_type_rects[TEXT_MODEL].Contains(event.GetPosition())) {
            m_isTextTypePressed = true;
        }
        
        if (!m_pretreat_btn_rect.IsEmpty() && m_pretreat_btn_rect.Contains(event.GetPosition())) {
            m_can_image_pretreat = !m_can_image_pretreat;
            RefreshScore();
            m_image_panel->SetProcessed(false);
            wxGetApp().app_config->set_bool("model_image_pretreat", m_can_image_pretreat);
        }
        Refresh();
        if (!HasCapture()) {
            CaptureMouse();
        }
        event.Skip();
        return;
    }
    else if (m_generateType == TEXT_MODEL) {
        if (!m_model_type_rects.empty() && m_model_type_rects[IMAGE_MODEL].Contains(event.GetPosition())) {
            m_isImageTypePressed = true;
        }
        Refresh();
        if (!HasCapture()) {
            CaptureMouse();
        }
        event.Skip();
        return;
    }
    event.Skip();
}

void ModelApiDialog::onLeftUp(wxMouseEvent& event) 
{
    if (m_generateType == IMAGE_MODEL) {
        if (!m_generate_btn_rect.IsEmpty() && m_isGeneratePressed && !m_image_panel->getPath().empty() &&
            m_generate_btn_rect.Contains(event.GetPosition())) {
            GenerateClicked(); 
            m_isGeneratePressed = false;
        }
        if (!m_model_type_rects.empty() && m_isTextTypePressed) {
            changeModelType(TEXT_MODEL);
            wxGetApp().app_config->set_bool("model_default_text", true);       
            m_isTextTypePressed = false;
        }
       
        Refresh();
        if (HasCapture()) {
            ReleaseMouse();
        }
        event.Skip();
        return;
    } 
    else if (m_generateType == TEXT_MODEL) {
        if (!m_generate_btn_rect.IsEmpty() && m_isGeneratePressed && !m_text_ctrl->GetValue().empty() &&
            m_generate_btn_rect.Contains(event.GetPosition())) {
            GenerateClicked();
            m_isGeneratePressed = false;
        }
        if (!m_model_type_rects.empty() && m_isImageTypePressed) {
            changeModelType(IMAGE_MODEL);
            wxGetApp().app_config->set_bool("model_default_text", false);       
            m_isImageTypePressed = false;
        }

        Refresh();
        if (HasCapture()) {
            ReleaseMouse();
        }
        event.Skip();
        return;
    }
    if (HasCapture()) {
        ReleaseMouse();
    }
    event.Skip();
}

void ModelApiDialog::onMouseCaptureLost(wxMouseCaptureLostEvent& event) 
{ 
    m_isGeneratePressed = false;
    m_isTextTypePressed = false;
    m_isImageTypePressed = false;
    m_isGenerateHovered  = false;
    m_isQuestionHovered  = false;
    m_isPretreatHovered  = false;
    m_isRuleHovered      = false;
    Refresh();
    event.Skip();
}

void ModelApiDialog::OnMouseMove(wxMouseEvent& event) 
{
    if (m_isGenerateHovered && !m_generate_btn_rect.Contains(event.GetPosition())) {
        m_isGenerateHovered = false;
        SetCursor(wxCURSOR_ARROW);
        Refresh();
    } else if (!m_isGenerateHovered && m_generate_btn_rect.Contains(event.GetPosition())) {
        m_isGenerateHovered = true;
        SetCursor(wxCURSOR_HAND);
        Refresh();
    }
    if (m_generateType == IMAGE_MODEL) {
        if (m_isQuestionHovered && !m_question_link_rect.Contains(event.GetPosition())) {
            m_isQuestionHovered = false;
            SetCursor(wxCURSOR_ARROW);
            m_question_dialog->Show(false);
        } else if (!m_isQuestionHovered && m_question_link_rect.Contains(event.GetPosition())) {
            m_isQuestionHovered = true;
            SetCursor(wxCURSOR_HAND);
            m_question_dialog->Move(this->ClientToScreen(wxPoint(event.GetPosition().x - 
                m_question_dialog->GetSize().x / 2, FromDIP(151))));
            m_question_dialog->SetProcessed(false);
            m_question_dialog->Show(true);
        }
        if (m_isPretreatHovered && !m_pretreat_link_rect.Contains(event.GetPosition())) {
            m_isPretreatHovered = false;
            SetCursor(wxCURSOR_ARROW);
            if (m_what_doing_dialog) {
                m_what_doing_dialog->Show(false);
            }
        } else if (!m_isPretreatHovered && m_pretreat_link_rect.Contains(event.GetPosition())) {
            m_isPretreatHovered = true;
            SetCursor(wxCURSOR_HAND);
            if (m_what_doing_dialog) {
                m_what_doing_dialog->Move(
                    this->ClientToScreen(wxPoint(event.GetPosition().x - 
                        m_what_doing_dialog->GetSize().x / 2, FromDIP(151))));
                m_what_doing_dialog->Show(true);
            }
        }
    } 
    else if (m_generateType == TEXT_MODEL) {
        if (m_isRuleHovered && !m_rule_link_rect.Contains(event.GetPosition())) {
            m_isRuleHovered = false;
            SetCursor(wxCURSOR_ARROW);
            if (m_rule_dialog) {
                m_rule_dialog->Show(false);
            }
        } else if (!m_isRuleHovered && m_rule_link_rect.Contains(event.GetPosition())) {
            m_isRuleHovered = true;
            SetCursor(wxCURSOR_HAND);
            if (m_rule_dialog) {
                m_rule_dialog->Move(this->ClientToScreen(wxPoint(event.GetPosition().x - 
                    m_rule_dialog->GetSize().x / 2, FromDIP(151))));
                m_rule_dialog->Show(true);
            }
        }
    }
    event.Skip();
}

void ModelApiDialog::GenerateClicked() 
{ 
    if (m_offline) {
        Close(true);
        return;
    }
    if (m_total_score < 0 || m_cost_score < 0 || m_total_score - m_cost_score < 0) {
        MessageDialog dlg(this, _L("Not enough points. Please earn more points."), _L("Info"));
        BindMsgDialog(&dlg);
        dlg.SetButtonLabel(wxID_OK, _L("Get Now"));
        if (dlg.ShowModal() == wxID_OK) {
            Close();
            wxGetApp().jump_to_user_points();
        }
        return;
    }

    if (m_generateType == IMAGE_MODEL && !ifstream(this->m_image_panel->getPath().ToStdString()).good()) {
        GUI::show_error(this, _L("Failed to load image"));
        return;
    }

    EndModal(wxID_OK);
}

void ModelApiDialog::RefreshScore() 
{
    if (!g_scoreRule->isOk) {
        return;
    }
    this->m_loadIcon->End();
    m_total_score = g_scoreRule->total_count;
    if (g_scoreRule->free_count > 0) {
        m_cost_score = 0;
        m_cost_text = wxString(_L("This generation is free"));
    } else {
        if (m_generateType == TEXT_MODEL) {
            m_cost_score = g_scoreRule->text_optimize_count + g_scoreRule->text_trans_image_count + g_scoreRule->image_generate_count;
        } else {
            m_cost_score = g_scoreRule->image_real_generate_count;
            if (m_can_image_pretreat) {
                m_cost_score = g_scoreRule->image_process_count + g_scoreRule->image_generate_count;
            }
        }
        m_cost_text = wxString(_L("Points consumed")) + wxString::Format(wxT(":  %d"), m_cost_score);
    }
 
    m_score_text = wxString(_L("Remaining points") + wxString::Format(wxT(":  %d"), m_total_score));
    Refresh();
}

void ModelApiDialog::changeModelType(ModelType type) 
{ 
    m_generateType = type; 
    if (type == TEXT_MODEL) {
        m_image_panel->Hide();
        m_text_panel->Show();  
    }
    else if (type == IMAGE_MODEL) {
        m_image_panel->Show();
        m_text_panel->Hide();
    }
    Layout();
    RefreshScore();
}

FFDownloadTool ModelImageProcessDialog::m_download_tool{4, 30000};

ModelImageProcessDialog::ModelImageProcessDialog(wxWindow* parent): 
    ModelBaseDialog(parent)
{
    this->SetSize(wxSize(FromDIP(393), -1));
    this->SetMinSize(wxSize(FromDIP(393), -1));
    this->SetDoubleBuffered(true);
    m_loadIcon = std::make_shared<ApiLoadingIcon>(this);
    m_loadIcon->Bind(EVT_UPDATE_ICON, [=](wxCommandEvent& event) { this->Refresh(); });
    m_processTask = std::make_shared<ModelApiTask>(this);
    Bind(EVT_ERROR_MSG, [=](wxCommandEvent& event) {
        if (event.GetString().ToStdString() == "NOT_ENOUGH_POINTS") {
            MessageDialog dlg(this, _L("Not enough points. Please earn more points."), _L("Info"));
            BindMsgDialog(&dlg);
            dlg.SetButtonLabel(wxID_OK, _L("Get Now"));
            if (dlg.ShowModal() == wxID_OK) {
                wxGetApp().jump_to_user_points();
            }
            m_errorExit = true;
            Close(true);
            return;
        }
        MessageDialog edlg(this, event.GetString(), _L("Error"));
        BindMsgDialog(&edlg);
        edlg.ShowModal();
        if (event.GetInt() == 1) {
            m_errorExit = true;
            Close(true);
        }
    });
    Bind(EVT_LOADED_IMAGE, [=](wxCommandEvent& event) {
        if (m_job_id < 0) {
            m_errorExit = true;
            Close(true);
            return;
        }
        m_image_url              = event.GetString();
        const std::string prefix = m_state == IMG_TO_IMG ? "imgtoimg_" : 
            m_state == TXT_TO_TXT ? "txttotxt_" : "txttoimg_";
        m_download_path = (boost::filesystem::path(ModelApiDialog::GetDir()) / 
            (prefix + std::to_string(m_job_id) + ".png")).string();
        m_download_id = m_download_tool.downloadDisk(event.GetString().ToStdString(), m_download_path, 100000, 6000000);
    });
    Bind(EVT_OPTIMIZED_TEXT, [=](wxCommandEvent& event) { 
        m_optimize_text = event.GetString();
    });
    Bind(EVT_REFRESH_STATE, [=](RefreshStateEvent& event) { 
        m_state = (ProcessState) event.state;
        m_job_id = event.jobId;
    });
    m_download_tool.Bind(EVT_FF_DOWNLOAD_FINISHED, &ModelImageProcessDialog::finishDownloadEvent, this);
    Bind(wxEVT_CLOSE_WINDOW, [=](wxCloseEvent& event) {
         if (m_errorExit) {
            event.Skip();
            return;
        }
        int ret = wxID_OK;
        if (!m_offline) {
            if (g_scoreRule->free_count <= 0) {
                MessageDialog dlg(this, _L("Points have been consumed. Are you sure you want to stop this generation?"), _L("Warning"),
                                  wxOK | wxCANCEL);
                BindMsgDialog(&dlg);
                ret = dlg.ShowModal();
            } else {
                MessageDialog dlg(this, _L("You've used your free generation. Stop this generation?"), _L("Warning"),
                    wxOK | wxCANCEL);
                BindMsgDialog(&dlg);
                ret = dlg.ShowModal();
            }
        }
        if (ret == wxID_OK) {
            if (m_job_id >= 0) {
                if (m_state == IMG_TO_IMG) {
                    std::thread([jobId = m_job_id]() { MultiComHelper::inst()->abortAiImg2imgJob(jobId, TIMEOUT_LIMIT); }).detach();
                } else if (m_state == TXT_TO_TXT) {
                    std::thread([jobId = m_job_id]() { MultiComHelper::inst()->abortAiTxt2txtJob(jobId, TIMEOUT_LIMIT); }).detach();
                } else {
                    std::thread([jobId = m_job_id]() { MultiComHelper::inst()->abortAiTxt2imgJob(jobId, TIMEOUT_LIMIT); }).detach();
                }
            }
            event.Skip();
        }
    });

    m_info_text = new Label(this, Label::Head_16, "", wxALIGN_CENTER);
    m_info_text->SetBackgroundColour(*wxWHITE);
    m_detail_text = new Label(this, Label::Body_13, "", wxALIGN_CENTER);
    m_detail_text->SetBackgroundColour(*wxWHITE);
    auto m_sizer = new wxBoxSizer(wxVERTICAL);
    m_sizer->AddSpacer(FromDIP(106));
    m_sizer->Add(m_info_text, 0, wxALIGN_CENTER | wxALL, 0);
    m_sizer->AddSpacer(FromDIP(10));
    m_sizer->Add(m_detail_text, 0, wxALIGN_CENTER | wxALL, 0);
    m_sizer->AddSpacer(FromDIP(38));
    SetSizer(m_sizer);
    changeModelType(IMAGE_MODEL, true);
    m_loadIcon->Loading(200);
}

void ModelImageProcessDialog::drawBackground(wxBufferedPaintDC& dc, wxGraphicsContext* gc) 
{
    gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
    gc->SetBrush(*wxWHITE);
    gc->DrawRectangle(0, 0, GetClientSize().x, GetClientSize().y);
    const int img_size = FromDIP(48);
    auto      size     = GetClientSize();
    m_loadIcon->paintInRect(gc, wxRect((size.x - img_size) / 2, FromDIP(38), img_size, img_size));
}

wxString ModelImageProcessDialog::getProcessedImage() 
{ 
    if (wxImage().LoadFile(m_image_path, wxBITMAP_TYPE_ANY)) {
        return m_image_path;
    } else {
        BOOST_LOG_TRIVIAL(error) << "AI MODEL: image process failed, already return old image";
        if (m_type == IMAGE_MODEL) {
            return m_src_image_path;
        } else {
            return ModelApiDialog::GetDir() + "/a.jpg";
        }
    }
}

wxString ModelImageProcessDialog::getProcessedUrl() { return m_image_url; }

wxString ModelImageProcessDialog::getOptimizedText() { return m_optimize_text; }

bool ModelImageProcessDialog::IsOptimized() { return m_isOptimized; }

void ModelImageProcessDialog::FirstStep(bool isFirstStep) 
{ 
    m_isFirstStep = isFirstStep; 
}

void ModelImageProcessDialog::setSrcImage(const wxString& path) 
{ 
    m_src_image_path = path; 
    changeModelType(IMAGE_MODEL);
    m_processTask->setThreadFunc([task = this->m_processTask, path = this->m_src_image_path,
        pipeline = g_pipeline, isFirstStep = m_isFirstStep, scoreRule = g_scoreRule]() {
        ComErrno ret = COM_OK;
        auto              imgName              = fs::path(path.utf8_string()).extension().string();
        std::string       img_url              = "";
        auto              callback_func        = [](long long now, long long total, void* data) {
            std::atomic_bool* isFinish = static_cast<std::atomic_bool*>(data);
            if (isFinish->load()) {
                return -1;
            }
            return 0;
        };
        BOOST_LOG_TRIVIAL(warning) << "AI IMAGE PATH: " << path.utf8_string();
        ret = MultiComHelper::inst()->uploadAiImageClound(path.utf8_string(), imgName, img_url, callback_func, &task->FinishLoop(),
                                                          TIMEOUT_LIMIT);
        if (ret != COM_OK) {
            task->safeFunc([task, ret]() {
                auto event = new wxCommandEvent(EVT_ERROR_MSG);
                event->SetString(_L("Network Error"));
                event->SetInt(1);
                wxQueueEvent(task->Parent(), event);
            });
            return;
        }
        if (isFirstStep) {
            com_user_ai_points_info_t data;
            ret = MultiComHelper::inst()->getUserAiPointsInfo(data, TIMEOUT_LIMIT);
            if (ret != COM_OK) {
                task->safeFunc([task, ret]() {
                    auto event = new wxCommandEvent(EVT_ERROR_MSG);
                    event->SetString(_L("Network Error"));
                    event->SetInt(1);
                    wxQueueEvent(task->Parent(), event);
                });
                return;
            }
            scoreRule->image_generate_count = data.modelGenPoints;
            scoreRule->image_process_count  = data.img2imgPoints;
            scoreRule->image_real_generate_count = data.modelGenPoints;
            scoreRule->text_optimize_count       = data.txt2txtPoints;
            scoreRule->text_trans_image_count    = data.txt2imgPoints;
            scoreRule->total_count               = data.totalPoints;
            scoreRule->free_count                = data.remainingFreeCount;
            scoreRule->reagain_free_count        = data.freeRetriesPerProcess;
            scoreRule->isOk                      = true;
            if (scoreRule->free_count <= 0 && scoreRule->image_generate_count + scoreRule->image_process_count > scoreRule->total_count) {
                task->safeFunc([task, ret]() {
                    auto event = new wxCommandEvent(EVT_ERROR_MSG);
                    event->SetString("NOT_ENOUGH_POINTS");
                    event->SetInt(1);
                    wxQueueEvent(task->Parent(), event);
                });
                return;
            }
            com_ai_job_pipeline_info_t pipe_ret;
            ret = MultiComHelper::inst()->createAiJobPipeline("img2img", pipe_ret, TIMEOUT_LIMIT);
            if (ret != COM_OK) {
                task->safeFunc([task, ret]() {
                    auto event = new wxCommandEvent(EVT_ERROR_MSG);
                    event->SetString(_L("Network Error"));
                    event->SetInt(1);
                    wxQueueEvent(task->Parent(), event);
                });
                return;
            }
            *pipeline = pipe_ret.id;
        }
        com_ai_general_job_result_t result;
        result.jobId = 0;
        ret = MultiComHelper::inst()->startAiImg2imgJob(4, *pipeline, img_url, result, TIMEOUT_LIMIT);
        if (ret != COM_OK) {
            task->safeFunc([task, ret]() {
                auto event = new wxCommandEvent(EVT_ERROR_MSG);
                if (ret == COM_AI_JOB_NOT_ENOUGH_POINTS) {
                    event->SetString("NOT_ENOUGH_POINTS");
                } else {
                    event->SetString(_L("Network Error"));
                }
                event->SetInt(1);
                wxQueueEvent(task->Parent(), event);
            });
            return;
        }
        const int64_t job_id = result.jobId;
        BOOST_LOG_TRIVIAL(info) << "AI MODEL:IMG_TO_IMG CURRENT JOB ID ------ " << job_id;
        task->safeFunc([=]() {
            auto event = new RefreshStateEvent();
            event->state = IMG_TO_IMG;
            event->jobId = job_id;
            wxQueueEvent(task->Parent(), event);
        });
        bool isFirstLoop       = true;
        int  networkErrorCount = 0;
        bool isOk              = false;
        com_ai_general_job_state_t state;
        while (!task->FinishLoop().load()) {    
            //state.status = 3;
            ret = MultiComHelper::inst()->getAiImg2imgJobState(job_id, state, TIMEOUT_LIMIT);
            if (ret != COM_OK) {
                if (ret == COM_INPUT_FAILED_THE_REVIEW) {
                    task->safeFunc([task, ret]() {
                        auto event = new wxCommandEvent(EVT_ERROR_MSG);
                        event->SetString(_L("No copyright"));
                        event->SetInt(1);
                        wxQueueEvent(task->Parent(), event);
                    });
                    return;
                }
                if (networkErrorCount < 3) {
                    networkErrorCount++;
                } else {
                    task->safeFunc([task, ret]() {
                        auto event = new wxCommandEvent(EVT_ERROR_MSG);
                        event->SetString(_L("Network Error"));
                        event->SetInt(1);
                        wxQueueEvent(task->Parent(), event);
                    });
                    return;
                }
            } else {
                networkErrorCount = 0;
            }
            if (isFirstLoop) {
                BOOST_LOG_TRIVIAL(info) << "AI MODEL: CURRENT IMG_TO_IMG JOB_ID ------ " << state.externalJobId;
                isFirstLoop = false;
            }
            if (state.status == 2) { // generating failed
                task->safeFunc([task]() {
                    auto event = new wxCommandEvent(EVT_ERROR_MSG);
                    event->SetString(_L("Generation failed"));
                    event->SetInt(1);
                    wxQueueEvent(task->Parent(), event);
                });
                return;
            }
            if (state.status == 4) { // canceled
                task->safeFunc([task]() {
                    auto event = new wxCommandEvent(EVT_REAL_CLOSE);
                    wxQueueEvent(task->Parent(), event);
                });
                return;
            }
            if (state.status == 3) { // completed
                isOk = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
        if (!isOk) {
            return;
        }
        task->safeFunc([task, state]() {
            auto event = new wxCommandEvent(EVT_LOADED_IMAGE);
            for (auto it : state.datas) {
                event->SetString(it.imageUrl);
                break;
            }
            wxQueueEvent(task->Parent(), event);
        });
    });
    m_processTask->start();
}

void ModelImageProcessDialog::setSrcText(const wxString& text, bool isOptimized)
{
    m_isOptimized = isOptimized;
    if (m_isOptimized) {
        m_optimize_text = text;
    }
    m_src_text = text;
    changeModelType(TEXT_MODEL);
    m_processTask->setThreadFunc([task = this->m_processTask, text = this->m_src_text, 
        isOptimized, pipeline = g_pipeline, isFirstStep = m_isFirstStep, scoreRule = g_scoreRule]() {
        ComErrno ret = COM_OK;
        com_ai_general_job_result_t result;
        //result.jobId = 0;
        wxString                    optimize_text = text;
        if (!isOptimized) {   
            if (isFirstStep) {
                com_user_ai_points_info_t data;
                ret = MultiComHelper::inst()->getUserAiPointsInfo(data, TIMEOUT_LIMIT);
                if (ret != COM_OK) {
                    task->safeFunc([task, ret]() {
                        auto event = new wxCommandEvent(EVT_ERROR_MSG);
                        event->SetString(_L("Network Error"));
                        event->SetInt(1);
                        wxQueueEvent(task->Parent(), event);
                    });
                    return;
                }
                scoreRule->image_generate_count      = data.modelGenPoints;
                scoreRule->image_process_count       = data.img2imgPoints;
                scoreRule->image_real_generate_count = data.modelGenPoints;
                scoreRule->text_optimize_count       = data.txt2txtPoints;
                scoreRule->text_trans_image_count    = data.txt2imgPoints;
                scoreRule->total_count               = data.totalPoints;
                scoreRule->free_count                = data.remainingFreeCount;
                scoreRule->reagain_free_count        = data.freeRetriesPerProcess;
                scoreRule->isOk                      = true;
                if (scoreRule->free_count <= 0 &&
                    scoreRule->text_optimize_count + scoreRule->text_trans_image_count + 
                    scoreRule->image_generate_count > scoreRule->total_count) {
                    task->safeFunc([task, ret]() {
                        auto event = new wxCommandEvent(EVT_ERROR_MSG);
                        event->SetString("NOT_ENOUGH_POINTS");
                        event->SetInt(1);
                        wxQueueEvent(task->Parent(), event);
                    });
                    return;
                }
                com_ai_job_pipeline_info_t pipe_ret;
                ret = MultiComHelper::inst()->createAiJobPipeline("text2text", pipe_ret, TIMEOUT_LIMIT);
                if (ret != COM_OK) {
                    task->safeFunc([task, ret]() {
                        auto event = new wxCommandEvent(EVT_ERROR_MSG);
                        if (ret == COM_AI_JOB_NOT_ENOUGH_POINTS) {
                            event->SetString("NOT_ENOUGH_POINTS");
                        } else {
                            event->SetString(_L("Network Error"));
                        }
                        event->SetInt(1);
                        wxQueueEvent(task->Parent(), event);
                    });
                    return;
                }
                *pipeline = pipe_ret.id;
            }
            ret          = MultiComHelper::inst()->startAiTxt2txtJob(4, *pipeline, text.utf8_string(), result, TIMEOUT_LIMIT);
            if (ret != COM_OK) {
                task->safeFunc([task, ret]() {
                    auto event = new wxCommandEvent(EVT_ERROR_MSG);
                    if (ret == COM_AI_JOB_NOT_ENOUGH_POINTS) {
                        event->SetString("NOT_ENOUGH_POINTS");
                    } else {
                        event->SetString(_L("Network Error"));
                    }

                    event->SetInt(1);
                    wxQueueEvent(task->Parent(), event);
                });
                return;
            }
            const int64_t job_id = result.jobId;
            BOOST_LOG_TRIVIAL(info) << "AI MODEL:TXT_TO_TXT CURRENT JOB ID ------ " << job_id;
            task->safeFunc([=]() {
                auto event   = new RefreshStateEvent();
                event->state = TXT_TO_TXT;
                event->jobId = job_id;
                wxQueueEvent(task->Parent(), event);
            });
            bool                       isFirstLoop       = true;
            int                        networkErrorCount = 0;
            bool                       isOk              = false;
            com_ai_general_job_state_t state;
            while (!task->FinishLoop().load()) {
                // state.status = 3;
                ret = MultiComHelper::inst()->getAiTxt2txtJobState(job_id, state, TIMEOUT_LIMIT);
                if (ret != COM_OK) {
                    if (ret == COM_INPUT_FAILED_THE_REVIEW) {
                        task->safeFunc([task, ret]() {
                            auto event = new wxCommandEvent(EVT_ERROR_MSG);
                            event->SetString(_L("No copyright"));
                            event->SetInt(1);
                            wxQueueEvent(task->Parent(), event);
                        });
                        return;
                    }
                    if (networkErrorCount < 15) {
                        networkErrorCount++;
                    } else {
                        task->safeFunc([task, ret]() {
                            auto event = new wxCommandEvent(EVT_ERROR_MSG);
                            event->SetString(_L("Network Error"));
                            event->SetInt(1);
                            wxQueueEvent(task->Parent(), event);
                        });
                        return;
                    }
                } else {
                    networkErrorCount = 0;
                }
                if (isFirstLoop) {
                    BOOST_LOG_TRIVIAL(info) << "AI MODEL: CURRENT TXT_TO_TXT JOB_ID ------ " << state.externalJobId;
                    isFirstLoop = false;
                }
                if (state.status == 2) { // generating failed
                    task->safeFunc([task]() {
                        auto event = new wxCommandEvent(EVT_ERROR_MSG);
                        event->SetString(_L("Generation failed"));
                        event->SetInt(1);
                        wxQueueEvent(task->Parent(), event);
                    });
                    return;
                }
                if (state.status == 4) { // canceled
                    task->safeFunc([task]() {
                        auto event = new wxCommandEvent(EVT_REAL_CLOSE);
                        wxQueueEvent(task->Parent(), event);
                    });
                    return;
                }
                if (state.status == 3) { // completed
                    isOk = true;
                    for (auto it : state.datas) {
                        optimize_text = wxString::FromUTF8(it.content);
                        break;
                    }
                    break;
                }
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
            if (!isOk) {
                return;
            }
            task->safeFunc([task, optimize_text]() {
                auto event = new wxCommandEvent(EVT_OPTIMIZED_TEXT);
                event->SetString(optimize_text);
                wxQueueEvent(task->Parent(), event);
            });
        }

        result.jobId = 0;
        ret          = MultiComHelper::inst()->startAiTxt2imgJob(3, *pipeline, optimize_text.utf8_string(), result, TIMEOUT_LIMIT);
        if (ret != COM_OK) {
            task->safeFunc([task, ret]() {
                auto event = new wxCommandEvent(EVT_ERROR_MSG);
                if (ret == COM_AI_JOB_NOT_ENOUGH_POINTS) {
                    event->SetString("NOT_ENOUGH_POINTS");
                } else {
                    event->SetString(_L("Network Error"));
                }
                event->SetInt(1);
                wxQueueEvent(task->Parent(), event);
            });
            return;
        }
        const int64_t job_id = result.jobId;
        BOOST_LOG_TRIVIAL(info) << "AI MODEL:TXT_TO_IMG CURRENT JOB ID ------ " << job_id;
        task->safeFunc([=]() {
            auto event   = new RefreshStateEvent();
            event->state = TXT_TO_IMG;
            event->jobId = job_id;
            wxQueueEvent(task->Parent(), event);
        });
        bool                       isFirstLoop       = true;
        int                        networkErrorCount = 0;
        bool                       isOk              = false;
        com_ai_general_job_state_t state;
        while (!task->FinishLoop().load()) {
            // state.status = 3;
            ret = MultiComHelper::inst()->getAiTxt2imgJobState(job_id, state, TIMEOUT_LIMIT);
            if (ret != COM_OK) {
                if (ret == COM_INPUT_FAILED_THE_REVIEW) {
                    task->safeFunc([task, ret]() {
                        auto event = new wxCommandEvent(EVT_ERROR_MSG);
                        event->SetString(_L("No copyright"));
                        event->SetInt(1);
                        wxQueueEvent(task->Parent(), event);
                    });
                    return;
                }
                if (networkErrorCount < 6) {
                    networkErrorCount++;
                } else {
                    task->safeFunc([task, ret]() {
                        auto event = new wxCommandEvent(EVT_ERROR_MSG);
                        event->SetString(_L("Network Error"));
                        event->SetInt(1);
                        wxQueueEvent(task->Parent(), event);
                    });
                    return;
                }
            } else {
                networkErrorCount = 0;
            }
            if (isFirstLoop) {
                BOOST_LOG_TRIVIAL(info) << "AI MODEL: CURRENT TXT_TO_TXT JOB_ID ------ " << state.externalJobId;
                isFirstLoop = false;
            }
            if (state.status == 2) { // generating failed
                task->safeFunc([task]() {
                    auto event = new wxCommandEvent(EVT_ERROR_MSG);
                    event->SetString(_L("Generation failed"));
                    event->SetInt(1);
                    wxQueueEvent(task->Parent(), event);
                });
                return;
            }
            if (state.status == 4) { // canceled
                task->safeFunc([task]() {
                    auto event = new wxCommandEvent(EVT_REAL_CLOSE);
                    wxQueueEvent(task->Parent(), event);
                });
                return;
            }
            if (state.status == 3) { // completed
                isOk = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        if (!isOk) {
            return;
        }
        task->safeFunc([task, state]() {
            auto event = new wxCommandEvent(EVT_LOADED_IMAGE);
            for (auto it : state.datas) {
                event->SetString(it.imageUrl);
                break;
            }
            wxQueueEvent(task->Parent(), event);
        });
    });
    m_processTask->start();
}
void ModelImageProcessDialog::changeModelType(ModelType type, bool init) 
{ 
    if (!init && m_type == type) {
        return;
    }
    m_type = type;
    if (type == TEXT_MODEL) {
        m_info_text->Hide();
        m_detail_text->SetLabel(_L("Generating a model image based on your text, please wait"));
        m_detail_text->Wrap(FromDIP(320));
    } else {
        m_info_text->SetLabel(_L("Processing image, please wait"));
        m_info_text->Wrap(FromDIP(320));
        m_info_text->Show();
        m_detail_text->SetLabel(_L("We will preprocess the image to ensure the best AI model generation results"));
        m_detail_text->Wrap(FromDIP(320));
    }
    Layout();
    Fit();
    Center();
}

ModelImageProcessDialog::~ModelImageProcessDialog() 
{
    if (m_download_id != -1) {
        m_download_tool.abort(m_download_id);
    }
    m_download_tool.Unbind(EVT_FF_DOWNLOAD_FINISHED, &ModelImageProcessDialog::finishDownloadEvent, this);
    m_loadIcon->End();
    wxEventBlocker block(this);
    {
        std::lock_guard<std::mutex> lock(m_processTask->Lock());
        m_processTask->FinishLoop().store(true);
        m_processTask.reset();
    }
}

void ModelImageProcessDialog::finishDownloadEvent(FFDownloadFinishedEvent& event) 
{
    if (m_offline) {
        return;
    }
    if (!event.succeed) {
        GUI::show_error(this, _L("Generation failed"));
        Close(true);
        return;
    }
    m_image_path = this->m_download_path;
    EndModal(wxID_OK);
}

ZoomOutDialog::ZoomOutDialog(wxWindow* parent, const wxImage& image) : 
    FFTitleLessDialog(parent) 
{ 
    this->m_image = image; 
    SetSize(FromDIP(wxSize(750, 750)));
    SetMinSize(FromDIP(wxSize(750, 750)));
    SetDoubleBuffered(true);
    Layout();
    Center();
}

void ZoomOutDialog::drawBackground(wxBufferedPaintDC& dc, wxGraphicsContext* gc) 
{ 
    gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
    gc->SetBrush(*wxWHITE);
    gc->DrawRectangle(0, 0, GetClientSize().x, GetClientSize().y);
    if (m_image.IsOk()) {
        auto     size = GetClientSize();
        wxBitmap bmp(m_image);
        gc->DrawBitmap(bmp, 0, 0, size.x, size.y);
    }
}

ModelSingleImageDialog::ModelSingleImageDialog(wxWindow* parent, const wxString& image_path) : 
    ModelBaseDialog(parent), 
    m_again_btn_rect(0, 0, 0, 0), m_zoom_btn_rect(0, 0, 0, 0)
{
    SetSize(FromDIP(wxSize(381, 393)));
    SetMinSize(FromDIP(wxSize(381, 393)));
    SetDoubleBuffered(true);
    m_bmp_map["zoom_out"] = ScalableBitmap(this, "zoom_out", 20);
    m_bmp_map["generate_again"] = ScalableBitmap(this, "generate_again", 16);
    m_image.LoadFile(image_path, wxBITMAP_TYPE_ANY);
    if (!m_image.IsOk()) {
        BOOST_LOG_TRIVIAL(error) << "AI MODEL: single image load failed:  " << image_path.ToStdString();
    }
    m_title = new Label(this, Label::Head_16, _L("Please confirm if the image meets your expectations"), wxALIGN_CENTER);
    m_title->SetBackgroundColour(*wxWHITE);
    m_title->Wrap(FromDIP(320));
    m_btn      = new FFButton(this, wxID_ANY, _L("Confirm"), FromDIP(4), false);
    m_btn->SetFontUniformColor(*wxWHITE);
    m_btn->SetFont(Label::Body_13);
    m_btn->SetBGDisableColor(wxColor("#419488"));
    m_btn->SetBGColor(wxColor("#419488"));
    m_btn->SetBGHoverColor(wxColor("#65A79E"));
    m_btn->SetBGPressColor(wxColor("#1A8676"));
    m_btn->SetMinSize(FromDIP(wxSize(320, 30)));
    m_btn->Bind(wxEVT_BUTTON, [=](wxCommandEvent& event) {
        if (m_offline) {
            Close(true);
            return;
        }
        m_selected = true;
        EndModal(wxID_OK);
    });
    Bind(wxEVT_CLOSE_WINDOW, [=](wxCloseEvent& event) {
        if (m_selected) {
            event.Skip();
            return;
        }
        int ret = wxID_OK;
        if (!m_offline) {
            if (g_scoreRule->free_count <= 0) {
                MessageDialog dlg(this, _L("Points have been consumed. Are you sure you want to stop this generation?"), _L("Warning"),
                                  wxOK | wxCANCEL);
                BindMsgDialog(&dlg);
                ret = dlg.ShowModal();
            } else {
                MessageDialog dlg(this, _L("You've used your free generation. Stop this generation?"), _L("Warning"), wxID_OK | wxID_CANCEL);
                BindMsgDialog(&dlg);
                ret = dlg.ShowModal();
            }
        }
        if (ret == wxID_OK) {
            event.Skip();
        }
    });

    auto sizer = new wxBoxSizer(wxVERTICAL);
    sizer->AddSpacer(FromDIP(38));
    sizer->Add(m_title, 0, wxALL | wxALIGN_CENTER, 0);
    sizer->AddSpacer(FromDIP(266));
    sizer->Add(m_btn, 0, wxALL | wxALIGN_CENTER, 0);
    sizer->AddSpacer(FromDIP(38));
    SetSizer(sizer);
    Layout();
    Center();
    Bind(wxEVT_LEFT_DOWN, &ModelSingleImageDialog::onLeftDown, this);
    Bind(wxEVT_LEFT_UP, &ModelSingleImageDialog::onLeftUp, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &ModelSingleImageDialog::onMouseCaptureLost, this);
}

void ModelSingleImageDialog::drawBackground(wxBufferedPaintDC& dc, wxGraphicsContext* gc)
{
    auto size = GetClientSize();
    gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
    gc->SetBrush(*wxWHITE);
    gc->DrawRectangle(0, 0, GetClientSize().x, GetClientSize().y);
    if (m_image.IsOk()) {
        wxBitmap bmp(m_image);
        int      bmp_size = FromDIP(192);
        auto&    zoom_bmp = m_bmp_map["zoom_out"];
        auto     bmp_y    = FromDIP(38) + FromDIP(16) + m_title->GetClientSize().y;
        gc->DrawBitmap(bmp, (size.x - bmp_size) / 2, bmp_y, bmp_size, bmp_size);
        gc->SetPen(wxPen(wxColor("#CCCCCC"), 1));
        gc->SetBrush(*wxTRANSPARENT_BRUSH);
        gc->DrawRectangle((size.x - bmp_size) / 2, bmp_y, bmp_size, bmp_size);
        int zoom_out_start_pos = size.x / 2 + bmp_size / 2 - zoom_bmp.GetBmpWidth();
        gc->DrawBitmap(zoom_bmp.bmp(), zoom_out_start_pos - 2, bmp_y + 2, zoom_bmp.GetBmpWidth(), zoom_bmp.GetBmpHeight());
        if (m_zoom_btn_rect.IsEmpty()) {
            m_zoom_btn_rect = wxRect(zoom_out_start_pos - 2, bmp_y + 2, zoom_bmp.GetBmpWidth(), zoom_bmp.GetBmpHeight());
        }
        auto again_y = bmp_y + bmp_size + FromDIP(16);
        auto again_str = _L("Regenerate");
        dc.SetFont(Label::Body_13);
        auto again_size = dc.GetTextExtent(again_str);
        auto& again_bmp  = m_bmp_map["generate_again"];
        auto  icon_sper  = FromDIP(6);
        dc.SetTextForeground(wxColor("#419488"));
        gc->DrawBitmap(again_bmp.bmp(), (size.x - again_bmp.GetBmpWidth() - again_size.x - icon_sper) / 2, again_y,
            again_bmp.GetBmpWidth(), again_bmp.GetBmpHeight());
        dc.DrawText(again_str, again_bmp.GetBmpWidth() + icon_sper + (size.x - again_bmp.GetBmpWidth() - again_size.x - icon_sper) / 2,
                    again_y);
        if (m_again_btn_rect.IsEmpty()) {
            m_again_btn_rect = wxRect((size.x - again_bmp.GetBmpWidth() - again_size.x - icon_sper) / 2, again_y,
                                      again_bmp.GetBmpWidth() + again_size.x + icon_sper, again_bmp.GetBmpHeight());
        }
    }
}

void ModelSingleImageDialog::SetAgainScore(int score) 
{
    m_againScore = score; 
}

bool ModelSingleImageDialog::IsOffline() { return m_offline; }

void ModelSingleImageDialog::onLeftDown(wxMouseEvent& event) 
{
    if (!m_image.IsOk()) {
        event.Skip();
        return;
    }
    if (!m_again_btn_rect.IsEmpty() && m_again_btn_rect.Contains(event.GetPosition()))
    {
        m_isAgainPressed = true;
        Refresh();
        if (!HasCapture()) {
            CaptureMouse();
        }
        event.Skip();
        return;
    }
    if (!m_zoom_btn_rect.IsEmpty() && m_zoom_btn_rect.Contains(event.GetPosition())) {
        m_isZoomOutPressed = true;
        Refresh();
        if (!HasCapture()) {
            CaptureMouse();
        }
        event.Skip();
        return;
    }
    event.Skip();
}

void ModelSingleImageDialog::onLeftUp(wxMouseEvent& event) 
{
    if (!m_image.IsOk()) {
        event.Skip();
        return;
    }
    if (m_offline) {
        event.Skip();
        return;
    }
    if (!m_again_btn_rect.IsEmpty() && m_isAgainPressed) {
        m_isAgainPressed = false;
        int ret          = wxID_OK;
        if (g_scoreRule->free_count <= 0) {
            if (g_scoreRule->total_count < 0 || g_scoreRule->total_count - m_againScore - g_scoreRule->image_real_generate_count < 0) {
                MessageDialog dlg0(this, _L("Cannot regenerate. Insufficient points for image or model generation."), _L("Warning"));
                BindMsgDialog(&dlg0);
                dlg0.ShowModal();
                if (HasCapture()) {
                    ReleaseMouse();
                }
                return;
            }
            MessageDialog dlg(this,
                              wxString::Format(_L("This will cost you %d points. Are you sure you want to regenerate?"), m_againScore),
                              _L("Warning"), wxOK | wxCANCEL);
            BindMsgDialog(&dlg);
            ret = dlg.ShowModal();
        } else {
            if (g_scoreRule->reagain_free_count <= 0) {
                MessageDialog dlg0(this, _L("Not enough regenerations. Cannot regenerate."), _L("Warning"));
                BindMsgDialog(&dlg0);
                dlg0.ShowModal();
                if (HasCapture()) {
                    ReleaseMouse();
                }
                return;
            }
            MessageDialog dlg(this,
                              wxString::Format(_L("You have %d regenerations left for this model generation. Regenerate?"),
                                  g_scoreRule->reagain_free_count),
                              _L("Warning"), wxOK | wxCANCEL);
            BindMsgDialog(&dlg);
            ret = dlg.ShowModal();
        }
        if (ret == wxID_OK) {
            if (HasCapture()) {
                ReleaseMouse();
            }
            EndModal(wxID_RESET);
            return;
        }
    }
    if (!m_zoom_btn_rect.IsEmpty() && m_isZoomOutPressed) {
        m_isZoomOutPressed = false;
        ZoomOutDialog dlg(this, m_image);
        BindMsgDialog(&dlg);
        dlg.ShowModal();
    }

    Refresh();
    if (HasCapture()) {
        ReleaseMouse();
    }
    event.Skip();
}

void ModelSingleImageDialog::onMouseCaptureLost(wxMouseCaptureLostEvent& event) 
{
    m_isAgainPressed  = false;
    m_isZoomOutPressed = false;
    Refresh();
    event.Skip();
}

ModelImageItemPanel::ModelImageItemPanel(wxWindow* parent, const wxImage& image) : 
    wxPanel(parent), m_zoom_btn_rect(0, 0, 0, 0) 
{ 
    SetDoubleBuffered(true);
    m_image               = image;
    m_bmp_map["zoom_out"] = ScalableBitmap(this, "zoom_out", 16);
    m_bmp_map["check_on"] = ScalableBitmap(this, "model_image_check_on", 24);
    m_bmp_map["check_off"] = ScalableBitmap(this, "model_image_check_off", 24);
    Bind(wxEVT_PAINT, &ModelImageItemPanel::OnPaint, this); 
    Bind(wxEVT_LEFT_DOWN, &ModelImageItemPanel::onLeftDown, this);
    Bind(wxEVT_LEFT_UP, &ModelImageItemPanel::onLeftUp, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &ModelImageItemPanel::onMouseCaptureLost, this);
}

void ModelImageItemPanel::OnPaint(wxPaintEvent& event) 
{
    wxBufferedPaintDC                  dc(this);
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (gc == nullptr) {
        return;
    }
    auto size = GetClientSize();
    if (m_image.IsOk()) {
        wxBitmap bmp(m_image);
        auto&    zoom_bmp = m_bmp_map["zoom_out"];
        auto&    check_bmp = m_checked ? m_bmp_map["check_on"] : m_bmp_map["check_off"];
        gc->DrawBitmap(bmp, 0, 0, size.x, size.y);
        int check_x            = size.x - check_bmp.GetBmpWidth();
        int check_y            = size.y - check_bmp.GetBmpHeight();
        gc->DrawBitmap(check_bmp.bmp(), check_x, check_y, check_bmp.GetBmpWidth(), check_bmp.GetBmpHeight());
        int zoom_out_start_pos = size.x - zoom_bmp.GetBmpWidth();
        gc->DrawBitmap(zoom_bmp.bmp(), zoom_out_start_pos - 2, 2, zoom_bmp.GetBmpWidth(), zoom_bmp.GetBmpHeight());
        if (m_zoom_btn_rect.IsEmpty()) {
            m_zoom_btn_rect = wxRect(zoom_out_start_pos - 2, 2, zoom_bmp.GetBmpWidth(), zoom_bmp.GetBmpHeight());
        }
    }
    else {
        gc->SetBrush(*wxWHITE);
        gc->DrawRectangle(0, 0, size.x, size.y);
    }
    gc->SetPen(wxPen(wxColor("#CCCCCC"), 1));
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    gc->DrawRectangle(0, 0, size.x - 1, size.y - 1);
}

void ModelImageItemPanel::SetChecked(bool checked) 
{
    m_checked = checked;
    Refresh();
}

bool ModelImageItemPanel::Checked() { return m_checked; }

void ModelImageItemPanel::onLeftDown(wxMouseEvent& event) 
{ 
    if (!m_image.IsOk()) {
        event.Skip();
        return;
    }
    if (!m_zoom_btn_rect.IsEmpty() && m_zoom_btn_rect.Contains(event.GetPosition())) {
        m_isZoomPressed = true;
    }
    else {
        m_isPressed = true;
    }
    Refresh();
    if (!HasCapture()) {
        CaptureMouse();
    }
    event.Skip();
}

void ModelImageItemPanel::onLeftUp(wxMouseEvent& event) 
{ 
    if (!m_image.IsOk()) {
        event.Skip();
        return;
    }
    if (!m_zoom_btn_rect.IsEmpty() && m_isZoomPressed) {
        m_isZoomPressed = false;
        ZoomOutDialog dlg(this, m_image);
        dlg.ShowModal();
    }
    if (m_isPressed) {
        m_isPressed = false;
        auto e      = new wxCommandEvent(wxEVT_CHECKBOX);
        e->SetEventObject(this);
        wxQueueEvent(this, e);
    }
    Refresh();
    if (HasCapture()) {
        ReleaseMouse();
    }
}

void ModelImageItemPanel::onMouseCaptureLost(wxMouseCaptureLostEvent& event) 
{ 
    m_isPressed = false;
    m_isZoomPressed = false;
    Refresh();
}

ModelFourImageDialog::ModelFourImageDialog(wxWindow* parent, std::vector<wxString> image_path_list)
    : FFTitleLessDialog(parent), m_again_btn_rect(0, 0, 0, 0)
{
    SetSize(FromDIP(wxSize(381, 264)));
    SetMinSize(FromDIP(wxSize(381, 264)));
    SetDoubleBuffered(true);
    if (image_path_list.size() == 0 || image_path_list.size() > 4) {
        BOOST_LOG_TRIVIAL(error) << "AI MODEL: four image dialog input failed!";
        return;
    }
    m_bmp_map["generate_again"] = ScalableBitmap(this, "generate_again", 16);
    for (auto path : image_path_list) {
        wxImage img;
        img.LoadFile(path, wxBITMAP_TYPE_ANY);
        if (!img.IsOk()) {
            BOOST_LOG_TRIVIAL(error) << "AI MODEL: single image load failed:  " << path.ToStdString();
        }
        auto panel = new ModelImageItemPanel(this, img);
        panel->SetMinSize(FromDIP(wxSize(72, 72)));
        panel->SetMaxSize(FromDIP(wxSize(72, 72)));
        panel->Bind(wxEVT_CHECKBOX, [=](wxCommandEvent& event) { 
            ModelImageItemPanel* p = dynamic_cast<ModelImageItemPanel*>(event.GetEventObject());
            if (!p) {
                return;
            }
            if (p->Checked()) {
                return;
            }
            for (auto it : m_image_panel_list) {
                if (it == p) {
                    it->SetChecked(true);
                } else {
                    it->SetChecked(false);
                }
            }
        });
        m_image_panel_list.emplace_back(panel);
    }
    m_image_panel_list[0]->SetChecked(true);

    m_title = new Label(this, Label::Head_16, _L("Please select an image to generate a 3D model"), wxALIGN_CENTER);
    m_title->SetBackgroundColour(*wxWHITE);
    m_title->Wrap(FromDIP(320));
    m_btn = new FFButton(this, wxID_ANY, _L("Confirm"), FromDIP(4), false);
    m_btn->SetFontUniformColor(*wxWHITE);
    m_btn->SetFont(Label::Body_13);
    m_btn->SetBGColor(wxColor("#419488"));
    m_btn->SetBGHoverColor(wxColor("#65A79E"));
    m_btn->SetBGPressColor(wxColor("#1A8676"));
    m_btn->SetMinSize(FromDIP(wxSize(320, 30)));
    m_btn->Bind(wxEVT_BUTTON, [=](wxCommandEvent& event) { EndModal(wxID_OK); });

    auto sizer = new wxBoxSizer(wxVERTICAL);
    sizer->AddSpacer(FromDIP(38));
    sizer->Add(m_title, 0, wxALL | wxALIGN_CENTER, 0);
    sizer->AddSpacer(FromDIP(16));
    auto h = new wxBoxSizer(wxHORIZONTAL);
    for (int i = 0; i < m_image_panel_list.size(); i++) {
        h->Add(m_image_panel_list[i], 0, wxALIGN_CENTER, 0);
        if (i != m_image_panel_list.size() - 1) {
            h->AddSpacer(FromDIP(10));
        }
    }
    sizer->Add(h, 0, wxALIGN_CENTER, 0);
    sizer->AddSpacer(FromDIP(48));
    m_again_btn_y = FromDIP(181);
    sizer->Add(m_btn, 0, wxALL | wxALIGN_CENTER, 0);
    sizer->AddSpacer(FromDIP(38));
    SetSizer(sizer);
    Layout();
    Center();
    Bind(wxEVT_LEFT_DOWN, &ModelFourImageDialog::onLeftDown, this);
    Bind(wxEVT_LEFT_UP, &ModelFourImageDialog::onLeftUp, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &ModelFourImageDialog::onMouseCaptureLost, this);
}

void ModelFourImageDialog::drawBackground(wxBufferedPaintDC& dc, wxGraphicsContext* gc)
{
    auto size = GetClientSize();
    gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
    gc->SetBrush(*wxWHITE);
    gc->DrawRectangle(0, 0, GetClientSize().x, GetClientSize().y);
    if (!m_image_panel_list.empty()) {
        auto again_y   = m_again_btn_y;
        auto again_str = _L("Regenerate");
        dc.SetFont(Label::Body_13);
        auto  again_size = dc.GetTextExtent(again_str);
        auto& again_bmp  = m_bmp_map["generate_again"];
        auto  icon_sper  = FromDIP(6);
        dc.SetTextForeground(wxColor("#419488"));
        gc->DrawBitmap(again_bmp.bmp(), (size.x - again_bmp.GetBmpWidth() - again_size.x - icon_sper) / 2, again_y, again_bmp.GetBmpWidth(),
                       again_bmp.GetBmpHeight());
        dc.DrawText(again_str, again_bmp.GetBmpWidth() + icon_sper + (size.x - again_bmp.GetBmpWidth() - again_size.x - icon_sper) / 2,
                    again_y);
        if (m_again_btn_rect.IsEmpty()) {
            m_again_btn_rect = wxRect((size.x - again_bmp.GetBmpWidth() - again_size.x - icon_sper) / 2, again_y,
                                      again_bmp.GetBmpWidth() + again_size.x + icon_sper, again_bmp.GetBmpHeight());
        }
    }
}

void ModelFourImageDialog::onLeftDown(wxMouseEvent& event)
{
    if (m_image_panel_list.empty()) {
        event.Skip();
        return;
    }
    if (!m_again_btn_rect.IsEmpty() && m_again_btn_rect.Contains(event.GetPosition())) {
        m_isAgainPressed = true;
        Refresh();
        if (!HasCapture()) {
            CaptureMouse();
        }
        event.Skip();
        return;
    }
    event.Skip();
}

void ModelFourImageDialog::onLeftUp(wxMouseEvent& event)
{
    if (m_image_panel_list.empty()) {
        event.Skip();
        return;
    }
    if (!m_again_btn_rect.IsEmpty() && m_isAgainPressed) {
        m_isAgainPressed = false;
        MessageDialog dlg(this, wxString::Format(
            _L("This will cost you %d points. Are you sure you want to regenerate?"), 20),
            _L("Warning"),
            wxOK | wxCANCEL);
        if (dlg.ShowModal() == wxID_OK) {
            if (HasCapture()) {
                ReleaseMouse();
            }
            EndModal(wxID_RESET);
            return;
        }
    }

    Refresh();
    if (HasCapture()) {
        ReleaseMouse();
    }
    event.Skip();
}

void ModelFourImageDialog::onMouseCaptureLost(wxMouseCaptureLostEvent& event)
{
    m_isAgainPressed   = false;
    Refresh();
    event.Skip();
}

wxDEFINE_EVENT(EVT_OLD_TASK, wxCommandEvent);
wxDEFINE_EVENT(EVT_SET_ID, wxCommandEvent);
wxDEFINE_EVENT(EVT_SET_STATE, ApiSetStateEvent);
wxDEFINE_EVENT(EVT_COMPLETE_MODEL, CompleteModelEvent);
wxDEFINE_EVENT(EVT_CHOICE_COLOR, ChoiceColorEvent);
wxDEFINE_EVENT(EVT_COMPLETE_CONVERT, CompleteConvertEvent);
wxDEFINE_EVENT(EVT_REAL_CLOSE, wxCommandEvent);

FFDownloadTool ModelGenerateDialog::m_download_tool{4, 30000};

ModelGenerateDialog::ModelGenerateDialog(wxWindow* parent) : 
    ModelBaseDialog(parent)
{
    this->SetSize(wxSize(FromDIP(393), -1));
    this->SetMinSize(wxSize(FromDIP(393), -1));
    this->SetDoubleBuffered(true);
    m_job_id   = std::make_shared<int64_t>(-1);
    m_loadIcon = std::make_shared<ApiLoadingIcon>(this);
    m_loadIcon->Bind(EVT_UPDATE_ICON, [=](wxCommandEvent& event) { this->Refresh(); });
    m_generateTask = std::make_shared<ModelApiTask>(this);
    m_abortTask = std::make_shared<ModelApiTask>(this);
    Bind(EVT_SET_STATE, [=](ApiSetStateEvent& event) {
        if (!event.isQueuePanel && m_isQueuePanel) {
            wxGetApp().update_user_points();
        }
        m_remainCount = event.remainCount;
        m_totalCount  = event.totalCount;
        showCurState(event.isQueuePanel, event.isShowQueue);
    });
    Bind(EVT_ERROR_MSG, [=](wxCommandEvent& event) {
        if (event.GetString().ToStdString() == "NOT_ENOUGH_POINTS") {
            MessageDialog dlg(this, _L("Not enough points. Please earn more points."), _L("Info"));
            BindMsgDialog(&dlg);
            dlg.SetButtonLabel(wxID_OK, _L("Get Now"));
            if (dlg.ShowModal() == wxID_OK) {    
                wxGetApp().jump_to_user_points();
            }
            m_errorExit = true;
            Close(true);
            return;
        }
        MessageDialog edlg(this, event.GetString(), _L("Error"));
        BindMsgDialog(&edlg);
        edlg.ShowModal();
        m_can_cancel = true;
        if (event.GetInt() == 1) {
            Close();
        } else if (event.GetInt() == 2) {
            m_errorExit = true;
            Close(true);
        }
    });
    Bind(EVT_COMPLETE_MODEL, [=](CompleteModelEvent& event) { 
        m_download_path = (boost::filesystem::path(ModelApiDialog::GetDir()) /
            ("hunyuan_" + std::to_string(event.job_id) + ".glb")).string();
        m_src_path = event.path;
        m_download_id = m_download_tool.downloadDisk(m_src_path, m_download_path, 100000, 6000000);
    });
    m_download_tool.Bind(EVT_FF_DOWNLOAD_FINISHED, &ModelGenerateDialog::finishDownloadEvent, this);
    Bind(EVT_CHOICE_COLOR, [=](ChoiceColorEvent& event) {
        this->m_modelData = event.data;
        this->m_cvt_colors = event.colors;
        EndModal(wxID_OK);
    });
    Bind(EVT_SET_ID, [job_id = this->m_job_id](wxCommandEvent& event) { 
        *job_id = event.GetInt();
    });
    m_abortTask->setThreadFunc([task = this->m_abortTask, job_id = this->m_job_id]() {
        auto ret = MultiComHelper::inst()->abortAiModelJob(*job_id, 10000);
        if (ret != COM_OK) {
            task->safeFunc([task]() {
                auto event = new wxCommandEvent(EVT_ERROR_MSG);
                event->SetString(_L("Failed to cancel the task"));
                event->SetInt(0);
                wxQueueEvent(task->Parent(), event);
            });
        }
        else {
            task->safeFunc([task]() {
                wxQueueEvent(task->Parent(), new wxCommandEvent(EVT_REAL_CLOSE));
            });
        }
    });
    Bind(wxEVT_CLOSE_WINDOW, [=](wxCloseEvent& event) {
        if (m_errorExit) {
            event.Skip();
            return;
        }
        if (m_offline) {
            if (*m_job_id < 0 || !m_isQueuePanel) {
                event.Skip();
                return;
            }
            if (m_can_cancel) {
                m_can_cancel = false;
                m_abortTask->start();
            }
            event.Skip();
            return;
        }
        if (m_isFirstStep) {
            if (*m_job_id < 0) {
                event.Skip();
                return;
            }
        }
        if (m_can_cancel) {
            m_can_cancel = false;
        }
        int ret = wxID_OK;
        if (g_scoreRule->free_count <= 0) {
            MessageDialog dlg(this, _L("Points have been consumed. Are you sure you want to stop this generation?"), _L("Warning"),
                              wxOK | wxCANCEL);
            BindMsgDialog(&dlg);
            ret = dlg.ShowModal();
        } else {
            MessageDialog dlg(this, _L("You've used your free generation. Stop this generation?"), _L("Warning"),
                wxOK | wxCANCEL);
            BindMsgDialog(&dlg);
            ret = dlg.ShowModal();
        }
        if (ret == wxID_OK) {
            if (!m_isQueuePanel || *m_job_id < 0) {
                event.Skip();
                return;
            }
            m_abortTask->start();
        }
    });
    Bind(EVT_REAL_CLOSE, [=](wxCommandEvent& event) { 
        m_errorExit  = true;
        m_can_cancel = true;
        Close(true);
    });

    m_info_text = new Label(this, Label::Head_16, "");
    m_info_text->SetBackgroundColour(*wxWHITE);
    m_queue_text = new Label(this, Label::Body_13, "");
    m_queue_text->SetBackgroundColour(*wxWHITE);
    m_sizer = new wxBoxSizer(wxVERTICAL);
    m_sizer->AddSpacer(FromDIP(106));
    m_sizer->Add(m_info_text, 0, wxALIGN_CENTER | wxALL, 0);
    m_sizer->AddSpacer(FromDIP(10));
    m_sizer->Add(m_queue_text, 0, wxALIGN_CENTER | wxALL, 0);
    m_sizer->AddSpacer(FromDIP(38));
    SetSizer(m_sizer);
    m_isShowQueue = true;
    showCurState(true, false);// init state
    m_loadIcon->Loading(200);
}

void ModelGenerateDialog::SetImgPath(wxString path, bool isFirstStep, int oldJobId)
{ 
    m_isFirstStep = isFirstStep;
    m_generateTask->setThreadFunc([task = this->m_generateTask, img_path = path, oldJobId, 
        isFirstStep = this->m_isFirstStep, pipeline = g_pipeline, scoreRule = g_scoreRule]() {
        const std::string generateFormat       = "GLB";
        const int         maxNetworkErrorCount = 3;
        const int         msTimeout            = TIMEOUT_LIMIT;
        auto        imgName       = fs::path(img_path.utf8_string()).extension().string();
        std::string img_url       = "";
        auto        callback_func = [](long long now, long long total, void* data) {
            std::atomic_bool* isFinish = static_cast<std::atomic_bool*>(data);
            if (isFinish->load()) {
                return -1;
            }
            return 0;
        };
        ComErrno ret = COM_OK;
        int64_t  job_id;
        if (oldJobId < 0) {
            if (isFirstStep) {
                com_user_ai_points_info_t data;
                ret = MultiComHelper::inst()->getUserAiPointsInfo(data, TIMEOUT_LIMIT);
                if (ret != COM_OK) {
                    task->safeFunc([task, ret]() {
                        auto event = new wxCommandEvent(EVT_ERROR_MSG);
                        event->SetString(_L("Network Error"));
                        event->SetInt(2);
                        wxQueueEvent(task->Parent(), event);
                    });
                    return;
                }
                scoreRule->image_generate_count      = data.modelGenPoints;
                scoreRule->image_process_count       = data.img2imgPoints;
                scoreRule->image_real_generate_count = data.modelGenPoints;
                scoreRule->text_optimize_count       = data.txt2txtPoints;
                scoreRule->text_trans_image_count    = data.txt2imgPoints;
                scoreRule->total_count               = data.totalPoints;
                scoreRule->free_count                = data.remainingFreeCount;
                scoreRule->reagain_free_count        = data.freeRetriesPerProcess;
                scoreRule->isOk                      = true;
                if (scoreRule->free_count <= 0 && scoreRule->image_generate_count > scoreRule->total_count) {
                    task->safeFunc([task, ret]() {
                        auto event = new wxCommandEvent(EVT_ERROR_MSG);
                        event->SetString("NOT_ENOUGH_POINTS");
                        event->SetInt(2);
                        wxQueueEvent(task->Parent(), event);
                    });
                    return;
                }
            }
            BOOST_LOG_TRIVIAL(warning) << "AI IMAGE PATH: " << img_path.utf8_string();
            ret = MultiComHelper::inst()->uploadAiImageClound(img_path.utf8_string(), imgName, img_url, callback_func, &task->FinishLoop(),
                                                              msTimeout);
            if (ret != COM_OK) {
                task->safeFunc([task, ret]() {
                    auto event = new wxCommandEvent(EVT_ERROR_MSG);
                    event->SetString(_L("Network Error"));
                    event->SetInt(2);
                    wxQueueEvent(task->Parent(), event);
                });
                return;
            }
            com_ai_model_job_result_t result;
            // result.jobId = 0;
            // result.isOldJob = false;
            ret = MultiComHelper::inst()->startAiModelJob(AI_SUPPLIER, *pipeline, img_url, generateFormat, result, msTimeout);
            if (ret != COM_OK) {
                task->safeFunc([task, ret]() {
                    auto event = new wxCommandEvent(EVT_ERROR_MSG);
                    if (ret == COM_AI_JOB_NOT_ENOUGH_POINTS) {
                        event->SetString("NOT_ENOUGH_POINTS");
                    } else {
                        event->SetString(_L("Network Error"));
                    }
                    event->SetInt(2);
                    wxQueueEvent(task->Parent(), event);
                });
                return;
            }
            job_id = result.jobId;
        } else {
            job_id = oldJobId;
        }
        BOOST_LOG_TRIVIAL(info) << "AI MODEL: CURRENT JOB ID ------ " << job_id;
        task->safeFunc([=]() {
            auto event = new wxCommandEvent(EVT_SET_ID);
            event->SetInt(job_id);
            wxQueueEvent(task->Parent(), event);
        });
        bool isFirstLoop       = true;
        int  networkErrorCount = 0;
        while (!task->FinishLoop().load()) {
            com_ai_model_job_state_t state;
            //state.status = 3;
            ret = MultiComHelper::inst()->getAiModelJobState(job_id, state, msTimeout);
            if (ret != COM_OK) {
                if (ret == COM_INPUT_FAILED_THE_REVIEW) {
                    task->safeFunc([task, ret]() {
                        auto event = new wxCommandEvent(EVT_ERROR_MSG);
                        event->SetString(_L("No copyright"));
                        event->SetInt(2);
                        wxQueueEvent(task->Parent(), event);
                    });
                    return;
                }
                if (networkErrorCount < maxNetworkErrorCount) {
                    networkErrorCount++;
                } else {
                    task->safeFunc([task, ret]() {
                        auto event = new wxCommandEvent(EVT_ERROR_MSG);
                        event->SetString(_L("Network Error"));
                        event->SetInt(2);
                        wxQueueEvent(task->Parent(), event);
                    });
                    return;
                }
            }
            else {
                networkErrorCount = 0;
            }
            if (isFirstLoop) {
                BOOST_LOG_TRIVIAL(info) << "AI MODEL: CURRENT HUNYUAN JOB_ID ------ " << state.externalJobId;
                isFirstLoop = false;
            }
            if (state.status == 2) { // generating failed
                task->safeFunc([task]() {
                    auto event = new wxCommandEvent(EVT_ERROR_MSG);
                    event->SetString(_L("AI Model Generation Failed"));
                    event->SetInt(2);
                    wxQueueEvent(task->Parent(), event);
                });
                return;
            }
            if (state.status == 4) { // canceled
                task->safeFunc([task]() {
                    auto event = new wxCommandEvent(EVT_REAL_CLOSE);
                    wxQueueEvent(task->Parent(), event);
                });
                return;
            }
            if (state.status == 3) { // completed
                auto event = new CompleteModelEvent();
                for (auto it : state.models) {
                    if (it.modelType == generateFormat) {
                        event->path   = it.modelUrl;
                        event->job_id = job_id;
                        break;
                    }
                }
                wxQueueEvent(task->Parent(), event);
                return;
            }

            task->safeFunc([=]() {
                auto e = new ApiSetStateEvent();
                if (state.posInQueue == 0) {
                    e->isQueuePanel = false;
                } else {
                    e->isShowQueue  = true;
                    e->isQueuePanel = true;
                    e->remainCount  = state.posInQueue;
                    e->totalCount   = state.queueLength;
                }
                wxQueueEvent(task->Parent(), e);
            });

            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    });
    m_generateTask->start();
}

void ModelGenerateDialog::drawBackground(wxBufferedPaintDC& dc, wxGraphicsContext* gc)
{
    gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
    gc->SetBrush(*wxWHITE);
    gc->DrawRectangle(0, 0, GetClientSize().x, GetClientSize().y);
    const int img_size = FromDIP(48);
    auto      size     = GetClientSize();
    m_loadIcon->paintInRect(gc, wxRect((size.x - img_size) / 2, FromDIP(38), img_size, img_size));
}

std::shared_ptr<convert_model_data_t> ModelGenerateDialog::getModelData() 
{ 
    return this->m_modelData; 
}

cvt_colors_t ModelGenerateDialog::getCvtColors() 
{ 
    return m_cvt_colors; 
}

std::string ModelGenerateDialog::getDownloadPath() 
{ 
    return m_download_path; 
}

void ModelGenerateDialog::showCurState(bool isQueuePanel, bool isShowQueue) 
{
    if (isQueuePanel) {
        m_info_text->SetLabel(_L("We're currently experiencing high demand. Please wait..."));
        m_info_text->Wrap(FromDIP(360));
        if (isShowQueue) {
            m_queue_text->SetLabel(_L("Current queue") + wxString::Format(wxT(" %d/%d"), m_remainCount, m_totalCount));
            m_queue_text->Show();
        }
        else {
            m_queue_text->Hide();
        }
    }
    else {
        m_info_text->SetLabel(_L("Generating, please wait..."));
        m_info_text->Wrap(FromDIP(360));
        m_queue_text->Hide();
    }
    Layout();
    m_isQueuePanel = isQueuePanel;
    if (m_isShowQueue != isShowQueue) {
        Fit();
    }
    Center();
    m_isShowQueue = isShowQueue;
}

ModelGenerateDialog::~ModelGenerateDialog() 
{
    if (m_download_id != -1) {
        m_download_tool.abort(m_download_id);
    }
    m_download_tool.Unbind(EVT_FF_DOWNLOAD_FINISHED, &ModelGenerateDialog::finishDownloadEvent, this);

    m_loadIcon->End();
    wxEventBlocker              block(this);
    {
        std::lock_guard<std::mutex> lock(m_generateTask->Lock());
        m_generateTask->FinishLoop().store(true);
        m_generateTask.reset();
    }
    {
        std::lock_guard<std::mutex> lock(m_abortTask->Lock());
        m_abortTask->FinishLoop().store(true);
        m_abortTask.reset();
    }
}

void ModelGenerateDialog::finishDownloadEvent(FFDownloadFinishedEvent& event) 
{
    if (!event.succeed) {
        if (m_download_try_angin) {
            GUI::show_error(this, _L("AI Model Generation Failed"));
            m_errorExit = true;
            Close(true);
        } else {
            m_download_try_angin = true;
            m_download_tool.downloadDisk(m_src_path, m_download_path, 100000, 6000000);
        }
        return;
    }
    m_download_try_angin = false;
    auto        task     = this->m_generateTask;
    std::string path     = this->m_download_path;
    /*path             = (boost::filesystem::path(ModelApiDialog::GetDir()) /
            ("hunyuan_" + std::to_string(191) + ".glb"))
               .string();*/
    m_generateTask->setThreadFunc([task, path]() {
        ConvertModel    cm;
        auto            area = wxGetApp().plater()->build_volume().printable_area();
        in_cvt_params_t params;
        params.transCoordSys   = true;
        params.maxPrintSize[0] = fabs(area[2].x() - area[0].x());
        params.maxPrintSize[1] = fabs(area[2].y() - area[0].y());
        params.maxPrintSize[2] = wxGetApp().plater()->build_volume().printable_height();
        auto model_data        = std::make_shared<convert_model_data_t>();
        cm.initConvertGlb(path, params, *model_data);
        // cm.initConvertObj(path0, params, *model_data);
        auto colors = cm.clusterColors(*model_data, 4);
        task->safeFunc([=]() {
            auto e    = new ChoiceColorEvent();
            e->data   = model_data;
            e->colors = colors;
            wxQueueEvent(task->Parent(), e);
        });
    });
    m_generateTask->start();
}

ApiSetStateEvent::ApiSetStateEvent(): wxCommandEvent(EVT_SET_STATE) {}

ModelColorDialog::ModelColorDialog(wxWindow* parent) : 
    FFTitleLessDialog(parent)
{ 
    this->SetSize(wxSize(FromDIP(393), FromDIP(233)));
    this->SetMinSize(wxSize(FromDIP(393), FromDIP(233)));
    this->SetDoubleBuffered(true);
    auto title = new Label(this, Label::Body_14, _L("Generation successful!"));
    title->SetBackgroundColour(*wxWHITE);
    auto inputLabel = new Label(this, Label::Body_13, _L("You can specify the number of colors for the model."));
    inputLabel->SetBackgroundColour(*wxWHITE);
    m_text_ctrl = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE | wxTE_CENTRE);
    m_text_ctrl->SetMaxSize(FromDIP(wxSize(20, 20)));
    m_text_ctrl->SetMinSize(FromDIP(wxSize(20, 20)));
    m_text_ctrl->SetValue("4");
    m_text_ctrl->SetBackgroundColour(*wxWHITE);
    m_text_ctrl->SetFont(Label::Body_13);
    m_text_ctrl->SetMaxLength(1);
    wxTextValidator validator(wxFILTER_DIGITS, nullptr);
    m_text_ctrl->SetValidator(validator);
    m_text_ctrl->Bind(wxEVT_TEXT, [=](wxCommandEvent& event) {
        wxTextCtrl* textCtrl = dynamic_cast<wxTextCtrl*>(event.GetEventObject());
        wxString    str      = textCtrl->GetValue();
        if (str.empty()) {
            return;
        }
        int         number   = wxAtoi(str);
        const int min_num        = 1;
        const int max_num        = 4;
        if (number > max_num || number < min_num) {
            number = number < min_num ? min_num : max_num;
            str    = wxString::Format(("%d"), number);
            textCtrl->SetValue(str);
            textCtrl->SetInsertionPointEnd();
        }
        if (m_last_color_count != number) {
            m_last_color_count = number;
            auto color = ConvertModel().clusterColors(*m_modelData, number);
            changeColor(color);
        }
    });
    m_text_ctrl->Bind(wxEVT_CHAR, [this](wxKeyEvent& e) {
        int      keycode    = e.GetKeyCode();
        wxString input_char = wxString::Format("%c", keycode);
        long     value;
        if (!input_char.ToLong(&value) && input_char.ToStdString() != "\b")
            return;
        e.Skip();
    });
    m_btn   = new FFButton(this, wxID_ANY, _L("Import"), FromDIP(4), false); 
    m_btn->SetSize(FromDIP(wxSize(134, 30)));
    m_btn->SetMinSize(FromDIP(wxSize(134, 30)));
    m_btn->SetFontUniformColor(*wxWHITE);
    m_btn->SetBGColor(wxColour("#419488"));
    m_btn->SetBGHoverColor(wxColor("#65A79E"));
    m_btn->SetBGPressColor(wxColor("#1A8676"));
    m_btn->Bind(wxEVT_BUTTON, [=](wxCommandEvent& event) { 
        m_text_ctrl->SetEditable(false);
        m_btn->Hide();
        m_loadIcon->Loading(200);
        m_convertTask->setThreadFunc([task = m_convertTask, data = m_modelData, count = m_last_color_count, path = m_filepath]() {
            ConvertModel cm;
            auto         color            = cm.clusterColors(*data, count);
            std::string  convert_obj_file = path;
            std::string  extension        = fs::path(path).extension().string();
            auto         just_filename    = path.substr(0, path.size() - extension.size()) + "_convert";
            size_t       version          = 0;
            convert_obj_file              = just_filename;
            auto tempdir                  = ModelApiDialog::GetDir();
            while (fs::exists(boost::filesystem::path(tempdir) / (convert_obj_file + ".obj"))) {
                ++version;
                convert_obj_file = just_filename + "(" + std::to_string(version) + ")";
            }
            std::string mtl_path = convert_obj_file + ".mtl";
            std::string obj_path = convert_obj_file + ".obj";
            cm.doConvert(*data, color, obj_path, mtl_path);
            task->safeFunc([=]() {
                auto event      = new CompleteConvertEvent();
                event->colors   = color;
                event->obj_path = obj_path;
                event->mtl_path = mtl_path;
                wxQueueEvent(task->Parent(), event);
            });
        });
        m_convertTask->start();
    });
    Bind(EVT_COMPLETE_CONVERT, [=](CompleteConvertEvent& event) {
        Close();
        std::vector<std::string> arr;
        arr.emplace_back(event.obj_path);
        auto origin_path = wxGetApp().app_config->get_last_dir();
        wxGetApp().plater()->load_files(arr, LoadStrategy::LoadModel, false, event.colors);
        wxGetApp().app_config->update_skein_dir(origin_path);
    });
    m_loadIcon = std::make_shared<ApiLoadingIcon>(this);
    m_loadIcon->Bind(EVT_UPDATE_ICON, [=](wxCommandEvent& event) { this->Refresh(); });
    auto sizer = new wxBoxSizer(wxVERTICAL);
    sizer->AddSpacer(FromDIP(32));
    sizer->Add(title, 0, wxALIGN_CENTER, 0);
    sizer->AddSpacer(FromDIP(16));
    auto h_sizer = new wxBoxSizer(wxHORIZONTAL);
    h_sizer->AddSpacer(FromDIP(20));
    h_sizer->Add(inputLabel, 0, wxALL | wxALIGN_CENTER, 0);
    h_sizer->AddSpacer(FromDIP(10));
    h_sizer->Add(m_text_ctrl, 0, wxALIGN_CENTER, 0);
    h_sizer->AddSpacer(FromDIP(20));
    sizer->Add(h_sizer, 0, wxALIGN_CENTER, 0);
    sizer->AddSpacer(FromDIP(83));
    sizer->Add(m_btn, 0, wxALIGN_CENTER, 0);
    sizer->AddSpacer(FromDIP(32));
    sizer->SetMinSize(wxSize(FromDIP(393), FromDIP(233)));
    SetSizerAndFit(sizer);
    Layout(); 
    Center();
}

void ModelColorDialog::drawBackground(wxBufferedPaintDC& dc, wxGraphicsContext* gc)
{ 
    gc->SetAntialiasMode(wxANTIALIAS_DEFAULT); 
    gc->SetBrush(*wxWHITE);
    gc->DrawRectangle(0, 0, GetClientSize().x, GetClientSize().y);
    dc.SetFont(Label::Body_13);
    wxPoint start_pos(FromDIP(70), FromDIP(104));
    const int     grid_sper = FromDIP(16);
    const int size = FromDIP(51);
    
    start_pos.x             = GetClientSize().x / 2 - (size * m_last_color_count + (m_last_color_count - 1) * grid_sper) / 2;
    for (int i = 0; i < m_color_grids.size(); i++) {
        gc->SetBrush(m_color_grids[i]);
        gc->DrawRectangle(start_pos.x + i * (grid_sper + size), start_pos.y, size, size);
        auto luminance = m_color_grids[i].GetLuminance();
        dc.SetTextForeground(luminance > 0.6 ? wxColor("#333333") : *wxWHITE);
        auto num_str   = wxString::Format(wxT("%d"), i + 1);
        auto text_size = dc.GetTextExtent(num_str);
        dc.DrawText(num_str, start_pos.x + i * (grid_sper + size) + (size - text_size.x) / 2, start_pos.y + (size - text_size.y) / 2);
    }
    if (m_loadIcon->isLoading()) {
        dc.SetTextForeground(*wxBLACK);
        dc.SetFont(Label::Body_12);
        auto text      = _L("Importing...");
        auto text_size = dc.GetTextExtent(text);
        dc.DrawText(text, (GetClientSize().x - text_size.x) / 2, FromDIP(171));
        auto load_size = FromDIP(22);
        m_loadIcon->paintInRect(gc, wxRect((GetClientSize().x - load_size) / 2, FromDIP(194), load_size, load_size));
    }
}

void ModelColorDialog::changeColor(const cvt_colors_t& colors) 
{
    m_color_grids.clear();
    for (auto color : colors) {
        wxColour c;
        c.SetRGB((color[2] << 16) + (color[1] << 8) + color[0]);
        m_color_grids.emplace_back(c);
    }
    m_last_color_count = m_color_grids.size();
    m_text_ctrl->ChangeValue(wxString::Format(wxT("%d"), m_last_color_count));
    Refresh();
}

void ModelColorDialog::setModelData(const std::shared_ptr<convert_model_data_t>& data) 
{ 
    this->m_modelData = data; 
    m_convertTask     = std::make_shared<ModelApiTask>(this);
}

void ModelColorDialog::setDownloadFile(const std::string& path) 
{ 
    this->m_filepath = path; 
}

ModelColorDialog::~ModelColorDialog() 
{
    wxEventBlocker              block(this);
    std::lock_guard<std::mutex> lock(m_convertTask->Lock());
    m_convertTask->FinishLoop().store(true);
    m_convertTask.reset();
}

VerticalCenterTextCtrl::VerticalCenterTextCtrl(wxWindow* parent): 
    wxTextCtrl(parent, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE | wxTE_CENTRE)
{
    Bind(wxEVT_PAINT, &VerticalCenterTextCtrl::OnPaint, this);
}

void VerticalCenterTextCtrl::OnPaint(wxPaintEvent& event)
{
    wxPaintDC dc(this);
    wxSize    size = GetClientSize();
    wxString  text = GetValue();
    dc.SetPen(wxPen(*wxBLACK, 1));
    dc.DrawRectangle(GetClientRect());
    dc.SetFont(this->GetFont());
    auto tsize = dc.GetTextExtent(text);
    int yPos = (size.y - tsize.y) / 2;
    dc.DrawText(text, (size.x - tsize.x) / 2, (size.y - tsize.y) / 2);
}

FinishScoreEvent::FinishScoreEvent() : wxCommandEvent(EVT_FINISH_SCORE) {}

CompleteModelEvent::CompleteModelEvent() : wxCommandEvent(EVT_COMPLETE_MODEL) {}

ChoiceColorEvent::ChoiceColorEvent() : wxCommandEvent(EVT_CHOICE_COLOR) {}

CompleteConvertEvent::CompleteConvertEvent() : wxCommandEvent(EVT_COMPLETE_CONVERT) {}

RefreshStateEvent::RefreshStateEvent() : wxCommandEvent(EVT_REFRESH_STATE) {}


bool ModelApi::m_exist = false;

void ModelApi::ShowModelApi(wxWindow* parent) 
{
    if (m_exist) {
        return;
    }
    try {
        MultiComHelper::inst()->userClickCount("ai", ComTimeoutWanB);
        m_exist            = true;
        g_scoreRule        = std::make_shared<ScoreRule>();
        g_pipeline                      = std::make_shared<int>(0);
        bool           isDirectGenerate = true;
        bool           isFirstGenerate  = true;
        int            ret = -1;
        ModelApiDialog model_dlg(parent);
        ret = model_dlg.ShowModal();
        if (ret == wxID_OK) { 
        } else if (ret == wxID_LAST) {
            int id = model_dlg.getOldJobId();
            ModelGenerateDialog gen_dlg(parent);
            gen_dlg.SetImgPath("", isDirectGenerate, id);
            ret = gen_dlg.ShowModal();
            if (ret != wxID_OK) {
                End();
                return;
            }
            ModelColorDialog color_dlg(parent);
            color_dlg.setDownloadFile(gen_dlg.getDownloadPath());
            color_dlg.setModelData(gen_dlg.getModelData());
            color_dlg.changeColor(gen_dlg.getCvtColors());
            color_dlg.ShowModal();
            End();
            return;
        } else {
            End();
            return;
        }
        auto image_path = model_dlg.getImage();
        int  processFlag = -1;
        if (model_dlg.getType() == IMAGE_MODEL && model_dlg.IsImageProcess()) {
            processFlag = 1;
        } else if (model_dlg.getType() == TEXT_MODEL) {
            processFlag = 2;            
        }
        if (processFlag > 0) {
            isDirectGenerate      = false;
            int ret0 = -1;
            bool is_optimized = false;
            wxString optimized_text;
            while (ret0 != wxID_OK) {
                ModelImageProcessDialog process_dlg(parent);
                process_dlg.FirstStep(isFirstGenerate);
                if (processFlag == 1) {
                    process_dlg.setSrcImage(model_dlg.getImage());
                } else {
                    if (is_optimized) {
                        process_dlg.setSrcText(optimized_text, true);
                    } else {
                        process_dlg.setSrcText(model_dlg.getText(), false);
                    }
                }
                ret = process_dlg.ShowModal();
                if (ret != wxID_OK) {
                    End();
                    return;
                }
                
                image_path = process_dlg.getProcessedImage();
                //image_path     = process_dlg.getProcessedUrl();
                int text_count = g_scoreRule->text_trans_image_count + (!is_optimized) * g_scoreRule->text_optimize_count;
                if (optimized_text.empty()) {
                    is_optimized = true;
                    optimized_text = process_dlg.getOptimizedText();
                }
                if (g_scoreRule->free_count <= 0) {
                    int score = processFlag == 1 ? g_scoreRule->image_process_count : text_count;
                    g_scoreRule->total_count -= score;
                } else {
                    if (!isFirstGenerate) {
                        if (g_scoreRule->reagain_free_count > 0) {
                            g_scoreRule->reagain_free_count--;
                        }
                    }
                }
                int again_score = processFlag == 1 ? g_scoreRule->image_process_count : g_scoreRule->text_trans_image_count;
                ModelSingleImageDialog single_image_dlg(parent, image_path);
                single_image_dlg.SetAgainScore(again_score);
                ret0 = single_image_dlg.ShowModal();
                if (ret0 == wxID_CANCEL) {
                    End();
                    return;
                }
                isFirstGenerate = false;
            }
        }
        ModelGenerateDialog generate_dlg(parent);
        generate_dlg.SetImgPath(image_path, isDirectGenerate);
        ret = generate_dlg.ShowModal();
        if (ret != wxID_OK) {
            End();
            return;
        }
        ModelColorDialog color_dlg(parent);
        color_dlg.setDownloadFile(generate_dlg.getDownloadPath());
        color_dlg.setModelData(generate_dlg.getModelData());
        color_dlg.changeColor(generate_dlg.getCvtColors());
        color_dlg.ShowModal();
        End();
    } catch (std::exception& e) {
        wxMessageBox(e.what());
        End();
    }
}

void ModelApi::End() 
{ 
    m_exist = false;
    g_scoreRule.reset();
    g_pipeline.reset();
}

} // namespace GUI
} // namespace Slic3r::GUI


