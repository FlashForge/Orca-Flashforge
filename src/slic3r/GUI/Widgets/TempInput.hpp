#ifndef slic3r_GUI_TempInput_hpp_
#define slic3r_GUI_TempInput_hpp_

#include "../wxExtensions.hpp"
#include <wx/textctrl.h>
#include "SwitchButton.hpp"
#include "StaticBox.hpp"
#include "Label.hpp"
#include "Button.hpp"
#include "FFButton.hpp"
#include "slic3r/GUI/TitleDialog.hpp"
#include "slic3r/GUI/FlashForge/FlashNetwork.h"
#include "slic3r/GUI/FlashForge/MultiComMgr.hpp"
#include "slic3r/GUI/FlashForge/MultiComUtils.hpp"
#include "slic3r/GUI/FlashForge/ComCommand.hpp"

wxDECLARE_EVENT(wxCUSTOMEVT_SET_TEMP_FINISH, wxCommandEvent);

wxDECLARE_EVENT(EVT_CANCEL_PRINT_CLICKED, wxCommandEvent);
wxDECLARE_EVENT(EVT_CONTINUE_PRINT_CLICKED, wxCommandEvent);
class CancelPrint : public Slic3r::GUI::TitleDialog
{
public:
    CancelPrint(const wxString &info, const wxString &leftBtnTxt, const wxString &rightBtnTxt);

protected:
    void on_dpi_changed(const wxRect &suggested_rect){};

private:
    wxBoxSizer   *m_sizer_main{nullptr};
    wxStaticText *m_info{nullptr};
    FFButton     *m_confirm_btn{nullptr};
    FFButton     *m_cancel_btn{nullptr};
};

class ShowTip : public Slic3r::GUI::TitleDialog
{
public:
    ShowTip(const wxString &info);
    void SetLabel(const wxString &info);

protected:
    void on_dpi_changed(const wxRect &suggested_rect){};

private:
    wxBoxSizer   *m_sizer_main{nullptr};
    wxStaticText *m_info{nullptr};
};

class TempInput : public wxNavigationEnabled<StaticBox>
{
    bool   hover;

    bool           m_read_only{false};
    wxSize         labelSize;
    ScalableBitmap normal_icon;
    ScalableBitmap actice_icon;
    ScalableBitmap degree_icon;

    StateColor   label_color;
    StateColor   text_color;

    wxTextCtrl *  text_ctrl;
    wxStaticText *warning_text;

    int  max_temp     = 0;
    int  min_temp     = 0;
    bool warning_mode = false;

    int              padding_left    = 0;
    static const int TempInputWidth  = 200;
    static const int TempInputHeight = 50;
public:
    enum WarningType {
        WARNING_TOO_HIGH,
        WARNING_TOO_LOW,
        WARNING_UNKNOWN,
    };

    TempInput();

    TempInput(wxWindow *     parent,
              int            type,
              wxString       text,
              wxString       label       = "",
              wxString       normal_icon = "",
              wxString       actice_icon = "",
              const wxPoint &pos         = wxDefaultPosition,
              const wxSize & size        = wxDefaultSize,
              long           style       = 0);

public:
    void Create(wxWindow *     parent,
                wxString       text,
                wxString       label       = "",
                wxString       normal_icon = "",
                wxString       actice_icon = "",
                const wxPoint &pos         = wxDefaultPosition,
                const wxSize & size        = wxDefaultSize,
                long           style       = 0);

	
    wxPopupTransientWindow *wdialog{nullptr};
    int  temp_type;
    bool actice = false;

    
    wxString erasePending(wxString &str);

    void SetTagTemp(int temp);
    void SetTagTemp(wxString temp);
    void SetTagTemp(int temp, bool notifyModify);

    void SetCurrTemp(int temp);
    void SetCurrTemp(wxString temp);
    void SetCurrTemp(int temp, bool notifyModify);
   
    bool AllisNum(std::string str);
    void SetFinish();
    void Warning(bool warn, WarningType type = WARNING_UNKNOWN);
    void SetIconActive();
    void SetIconNormal();

   void SetReadOnly(bool ro) { m_read_only = ro; }
    void SetTextBindInput();

    void SetMaxTemp(int temp);
    void SetMinTemp(int temp);

    int GetType() { return temp_type; }

    wxString GetTagTemp() { return text_ctrl->GetValue(); }
    wxString GetCurrTemp() { return GetLabel(); }

    void SetLabel(const wxString &label);

    void SetTextColor(StateColor const &color);

    void SetLabelColor(StateColor const &color);

    virtual void Rescale();

    virtual bool Enable(bool enable = true) override;

    virtual void SetMinSize(const wxSize &size) override;

    wxTextCtrl *GetTextCtrl() { return text_ctrl; }

    wxTextCtrl const *GetTextCtrl() const { return text_ctrl; }

protected:
    virtual void OnEdit() {}

    virtual void DoSetSize(int x, int y, int width, int height, int sizeFlags = wxSIZE_AUTO);

    void DoSetToolTipText(wxString const &tip) override;

private:
    void paintEvent(wxPaintEvent &evt);

    void render(wxDC &dc);

	void messureMiniSize();
    void messureSize();

    // some useful events
    void mouseMoved(wxMouseEvent &event);
    void mouseWheelMoved(wxMouseEvent &event);
    void mouseEnterWindow(wxMouseEvent &event);
    void mouseLeaveWindow(wxMouseEvent &event);
    void keyPressed(wxKeyEvent &event);
    void keyReleased(wxKeyEvent &event);

    DECLARE_EVENT_TABLE()
};

class IconText : public wxPanel
{
public:
    IconText();
    IconText(wxWindow* parent,
             wxString icon = "",
             int iconSize = 12,
             wxString text = "",
             int textSize = 12,
             const wxPoint &pos = wxDefaultPosition,
             const wxSize & size = wxDefaultSize,
             long style = wxTAB_TRAVERSAL);
    ~IconText(){};
    void create_panel(wxWindow* parent,wxString icon,int iconSize,wxString text,int textSize);
    void setText(wxString text);
    void setTextForegroundColour(wxColour colour);
    void setTextBackgroundColor(wxColour colour);
private:
    wxBitmap    m_icon;
    wxStaticBitmap*  m_icon_staticbitmap{nullptr};
    Label          *m_text_ctrl{nullptr};
};

class IconBottonText :public wxPanel
{
public:
        IconBottonText(wxWindow* parent,
                       wxString icon = "",
                       int iconSize = 12,
                       wxString text = "",
                       int textSize = 12,
                       wxString secondIcon = "",
                       wxString thirdIcon = "",
                       bool positiveOrder = true,
                       const wxPoint &pos = wxDefaultPosition,
                       const wxSize & size = wxDefaultSize,
                       long style = wxTAB_TRAVERSAL);
    ~IconBottonText(){};
    void create_panel(wxWindow* parent,wxString icon,int iconSize,wxString text,int textSize,wxString secondIcon  = "",wxString thirdIcon = "",bool positiveOrder = true);
    void setLimit(double min,double max);
    void setAdjustValue(double value);
    wxString getTextValue();
    void setText(wxString text);
    void setCurValue(double value);
    void setPoint(int value);
    void checkValue();

private:
    void onTextChange(wxCommandEvent &event);
    void onTextFocusOut(wxFocusEvent &event);
    void onDecBtnClicked(wxMouseEvent &event);
    void onIncBtnClicked(wxMouseEvent &event);

private:
    double        m_min;
    double        m_max;
    double        m_adjust_value;
    double        m_cur_value;
    int           m_point = 0;
    wxBitmap    m_icon;
    FFPushButton *m_dec_btn{nullptr};
    FFPushButton *m_inc_btn{nullptr};
    wxTextCtrl*   m_text_ctrl{nullptr};
    wxStaticText *m_unitLabel{nullptr};
};

class StartFiltering : public wxPanel
{
public:
    StartFiltering(wxWindow* parent);
    ~StartFiltering(){};
    void setCurId(int curId);
    void create_panel(wxWindow* parent);
    void setBtnState(bool internalOpen, bool externalOpen);

private:
    void onAirFilterToggled(wxCommandEvent &event);

private:
    SwitchButton* m_internal_circulate_switch;//内循环过滤
    SwitchButton* m_external_circulate_switch;//外循环过滤

    int m_cur_id;
};

class TempMixDevice :public wxPanel
{
public:
    TempMixDevice(wxWindow* parent,bool idle = false, wxString nozzleTemp = "--", wxString platformTemp = "--", wxString cavityTemp = "--",
                 const wxPoint &pos = wxDefaultPosition,
                 const wxSize & size = wxDefaultSize,
                 long style = wxTAB_TRAVERSAL);
    ~TempMixDevice(){};

    void setState(int state);
    void setCurId(int curId);
    void reInitProductState();
    void reInitPage();
    void setDevProductAuthority(const fnet_dev_product_t &data);
    void lostFocusmodifyTemp();

    void create_panel(wxWindow* parent,bool idle, wxString nozzleTemp,wxString platformTemp,wxString cavityTemp);

    void setupLayoutIdleDeviceState(wxBoxSizer *deviceStateSizer, wxPanel *parent,bool idle);
    void setupLayoutDeviceInfo(wxBoxSizer *deviceStateSizer, wxPanel *parent);

    void connectEvent();
    void onDevInfoBtnClicked(wxMouseEvent &event);
    void onLampBtnClicked(wxMouseEvent &event);
    void onFilterBtnClicked(wxMouseEvent &event);

    void setDeviceInfoBtnIcon(const wxString &icon);

    void modifyTemp(wxString nozzleTemp = "--", wxString platformTemp = "--", wxString cavityTemp = "--", int topTemp = 0, int bottomTemp = 0,int chamberTemp = 0);
    void modifyDeviceInfo(wxString machineType, wxString sprayNozzle,wxString printSize,wxString version,wxString number,wxString material);
    void modifyDeviceLampState(bool bOpen);
    void modifyDeviceFilterState(bool internalOpen, bool externalOpen);

private:
    wxPanel* m_panel_idle_device_state;
    wxPanel* m_panel_idle_device_info;

    Button* m_idle_device_info_button;
    Button* m_idle_lamp_control_button;
    Button* m_idle_filter_button;

    StartFiltering* m_panel_circula_filter; //空闲状态，过滤按钮

    TempInput *m_top_btn{nullptr};
    TempInput *m_bottom_btn{nullptr};
    TempInput *m_mid_btn{nullptr};

    //TempButton *m_top_btn{nullptr};
    //TempButton *m_bottom_btn{nullptr};
    //TempButton *m_mid_btn{nullptr};

    Label *m_machine_type_data{nullptr};
    Label *m_spray_nozzle_data{nullptr};
    Label *m_print_size_data{nullptr};
    Label *m_firmware_version_data{nullptr};
    Label *m_serial_number_data{nullptr};
    Label *m_private_material_data{nullptr};

    int m_cur_id = -1;

    double m_right_target_temp = 0.00;
    double m_plat_target_temp = 0.00;

};

#endif // !slic3r_GUI_TempInput_hpp_
