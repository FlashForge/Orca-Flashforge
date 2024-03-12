#ifndef slic3r_DeviceListPanel_hpp_
#define slic3r_DeviceListPanel_hpp_
#include <map>
#include <set>
#include <wx/simplebook.h>
#include <wx/tglbtn.h>
#include "slic3r/GUI/Widgets/Button.hpp"
#include "slic3r/GUI/Widgets/ComboBox.hpp"
#include "slic3r/GUI/Widgets/PopupWindow.hpp"
#include "MultiComMgr.hpp"
#include "MultiComEvent.hpp"

class FFToggleButton;
class FFBitmapToggleButton;
class FFCheckBox;

namespace Slic3r {
namespace GUI {

#define LISTBOX_HEIGHT 40

class DropDownButton : public wxPanel
{
public:
    DropDownButton(wxWindow* parent = nullptr, const wxString& text = wxEmptyString, const wxBitmap& bitmap = wxNullBitmap);
    ~DropDownButton();

    void setText(const wxString& text);

private:
    wxPoint convertEventPoint(const wxMouseEvent& event);
    void leaveWindow();
    bool isPointIn(const wxPoint& pnt);
    void onEnter(wxMouseEvent& event);
    void onLeave(wxMouseEvent& event);
    void onMotion(wxMouseEvent& event);
    void onMouseCaptureLost(wxMouseCaptureLostEvent& event);

private:
    wxStaticText*   m_text;
    wxStaticBitmap* m_bitmap;
    wxBoxSizer*     m_sizer;
};


wxDECLARE_EVENT(EVT_FILTER_ITEM_CLICKED, wxCommandEvent);
class FilterPopupWindow : public PopupWindow
{
public:
    class FilterItem : public wxPanel
    {
    public:
        FilterItem(wxWindow* parent, const wxString& text, bool top_corner_round = false,
            bool bottom_corner_round = false, bool can_checked = false);
        ~FilterItem();

        void setSelect(bool select);
        bool isSelect() const;
        void setChecked(bool check);
        bool isChecked() const;
        wxString getText() const;
        void setText(const wxString& text);
        void setTopCornerRound(bool round);
        void setBottomCornerRound(bool round);
        bool Show(bool show = true) override;

    private:
        void onPaint(wxPaintEvent& event);
        void onEnter(wxMouseEvent& event);
        void onLeave(wxMouseEvent& event);
        void onMouseDown(wxMouseEvent& event);
        void onMouseUp(wxMouseEvent& event);
        void onMotion(wxMouseEvent& event);
        void onMouseCaptureLost(wxMouseCaptureLostEvent& event);
        bool isPointIn(const wxPoint& pnt);
        void sendEvent();
        void leaveWindow();

    private:
        bool            m_hoverFlag {false};
        bool            m_selectFlag {false};
        bool            m_pressFlag {false};
        bool            m_topCornerRound {false};
        bool            m_bottomCornerRound {false};
        bool            m_can_check {false};
        FFCheckBox*     m_check_box {nullptr};
        wxStaticText*   m_text {nullptr};
    };

public:
    FilterPopupWindow(wxWindow* parent);
    ~FilterPopupWindow();

    void Create();
    void Popup(wxWindow* focus = nullptr) override;
    bool ProcessLeftDown(wxMouseEvent &event)override;
    void AddItem(FilterItem* item);
    void ClearItem();

private:
    void onPaint(wxPaintEvent& event);

private:
    wxBoxSizer*     m_sizer;
    std::vector<FilterItem*> m_items;
};


class DeviceItemPanel : public wxPanel
{
public:
    DeviceItemPanel(wxWindow *parent);
    virtual ~DeviceItemPanel();

    void blockMouseEvent(bool block);

protected:
    wxString statusText(const std::string& status, wxColour& color);
    void leaveWindow();
    void mouseDown(wxMouseEvent &event);
    void mouseReleased(wxMouseEvent &event);
    void onEnter(wxMouseEvent &event);
    void onLeave(wxMouseEvent &event);
    void onMotion(wxMouseEvent& event);
    void onPaint(wxPaintEvent& event);
    void onMouseCaptionLost(wxMouseCaptureLostEvent& event);

private:
    bool isPointIn(const wxPoint& pt);
    void sendEvent();

protected:
    bool            m_hovered {false};
    bool            m_pressed {false};
    bool            m_blockFlag {false};    
    wxBoxSizer*     m_main_sizer {nullptr};
    wxColour        m_bg_color = wxColour("#ffffff");
    wxColour        m_border_color = wxColour("#ffffff");
    wxColour        m_border_hover_color = wxColour("#999999");
    wxColour        m_border_press_color = wxColour("#328DFB");

    static std::map<unsigned short, wxBitmap> m_machineBitmapMap;
};


class DeviceInfoItemPanel : public DeviceItemPanel
{
public:
    struct DeviceInfo {
        bool lanFlag {false};  // lan or not
        int conn_id {-1};
        unsigned short pid {0};
        std::string name;
        std::string placement;
        std::string status;
    };

    DeviceInfoItemPanel(wxWindow *parent, const DeviceInfo& info);

    void updateInfo(const DeviceInfo& info);
    const DeviceInfo& deviceInfo() const;

private:
    void updateStatus();
    static wxBitmap machineBitmap(unsigned short pid);

private:
    DeviceInfo      m_info;
    wxStaticText*   m_name_text {nullptr};
    wxStaticBitmap* m_icon {nullptr};
    wxStaticText*   m_placement_text {nullptr};
    wxStaticText*   m_status_text {nullptr};
};


class DeviceStaticItemPanel : public DeviceItemPanel
{
public:
    DeviceStaticItemPanel(wxWindow* parent, std::string status, int count);
    void setStatus(const std::string& status);
    std::string getStatus() const { return m_status; }
    void setCount(int count);
    int getCount() const { return m_count; }

private:
    int             m_count {0};
    std::string     m_status;
    wxStaticText*   m_status_text {nullptr};
    wxStaticText*   m_count_text {nullptr};

};


struct StringCompareFunc {
    bool operator()(const std::string& lhs, const std::string& rhs) const {
        return lhs < rhs;
    }
};
typedef std::map<std::string, FilterPopupWindow::FilterItem*, StringCompareFunc> FilterItemMap;

class DeviceListUpdateEvent;
class DeviceListPanel : public wxScrolledWindow
{
public:
    enum FilterPopupType {
        Filter_Popup_Type_None = 0,     // no popup
        Filter_Popup_Type_Placement,    // placement popup
        Filter_Popup_Type_Status,       // status popup
        Filter_Popup_Type_Device_Type,  // device type popup
    };

public:
    DeviceListPanel(wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, 
            const wxSize& size = wxDefaultSize);
    ~DeviceListPanel();

    void msw_rescale();

private:
    void build();
    void connectEvent();
    void initLocalDevice(std::map<std::string, DeviceInfoItemPanel::DeviceInfo>& deviceInfoMap);
    void initDeviceList();
    void updateFilterMap();
    void updatePlacementMap();
    void updateStatusMap();
    void updateTypeMap();
    void updateFilterTitle();
    void updateStaticMap();
    void updateSizer();
    void filterDeviceList();
    void updateDeviceWindowSize();

    void onFilterButtonClicked(wxMouseEvent &event);
    void onNetworkTypeToggled(wxCommandEvent& event);
    void onStaticModeToggled(wxCommandEvent &event);
    void onDeviceListUpdated(DeviceListUpdateEvent& event);
    void onComDevDetailUpdate(ComDevDetailUpdateEvent& event);
    void onPopupShow(wxShowEvent& event);
    void onFilterItemClicked(wxCommandEvent& event);

private: 
    DropDownButton* m_placement_btn {nullptr};
    DropDownButton *m_status_btn {nullptr};
    DropDownButton *m_type_btn {nullptr};
    FFToggleButton* m_wlan_btn {nullptr};
    FFToggleButton* m_lan_btn {nullptr};
    FFBitmapToggleButton* m_static_btn {nullptr};
    
    wxSimplebook*   m_simple_book {nullptr};
    wxPanel*        m_no_device_panel {nullptr};
    wxStaticBitmap* m_no_device_bitmap {nullptr};
    wxStaticText*   m_no_device_staticText {nullptr};
    wxBoxSizer     *m_no_device_sizer {nullptr};
    wxPanel*        m_device_window {nullptr};
    wxGridSizer*    m_device_sizer {nullptr};
    
    FilterPopupType m_filter_popup_type {Filter_Popup_Type_None};
    FilterPopupWindow* m_filter_popup {nullptr};
    std::map<std::string, DeviceInfoItemPanel*> m_device_map;
    std::map<std::string, DeviceStaticItemPanel*> m_device_stat_map;
    FilterPopupWindow::FilterItem*          m_default_filter_item {nullptr};
    FilterItemMap       m_placement_item_map;
    FilterItemMap       m_status_item_map;
    FilterItemMap       m_type_item_map;
    bool                m_filter_placement_default {true};
    std::string         m_filter_placement;
    bool                m_filter_status_default {true};
    std::string         m_filter_status;
    std::set<std::string> m_filter_types;
};

} // GUI
} // Slic3r

#endif /* slic3r_DeviceListPanel_hpp_ */
