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
    DropDownButton(wxWindow* parent = nullptr, const wxString& name = wxEmptyString, const wxBitmap& bitmap = wxNullBitmap);

private:
    wxPoint convertEventPoint(const wxMouseEvent& event);
    bool isPointIn(const wxPoint& pnt);
    void onEnter(wxMouseEvent& event);
    void onLeave(wxMouseEvent& event);

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
        ~FilterItem() {};

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
        bool isPointIn(const wxPoint& pnt);
        void sendEvent();

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
    struct DeviceInfo {
        bool lanFlag {false};  // lan or not
        int conn_id {-1};
        unsigned short pid {0};
        std::string name;
        std::string placement;
        std::string status;
    };
    DeviceItemPanel(wxWindow *parent, const DeviceInfo& info);

    void updateInfo(const DeviceInfo& info);
    const DeviceInfo& deviceInfo() const;
    void blockMouseEvent(bool block);

protected:
    void mouseDown(wxMouseEvent &event);
    void mouseReleased(wxMouseEvent &event);
    void onEnter(wxMouseEvent &event);
    void onLeave(wxMouseEvent &event);
    void onPaint(wxPaintEvent& event);

private:
    void build();
    void connectEvent();
    bool isPointIn(const wxPoint& pt);
    void sendEvent();
    void updateStatus();

    static wxBitmap machineBitmap(unsigned short pid);

private:
    bool            m_hovered {false};
    bool            m_pressed {false};
    bool            m_blockFlag {false};
    DeviceInfo      m_info;
    wxStaticText*   m_name_text {nullptr};
    wxStaticBitmap* m_icon {nullptr};
    wxStaticText*   m_placement_text {nullptr};
    wxStaticText*   m_status_text {nullptr};
    wxBoxSizer*     m_main_sizer {nullptr};
    wxColour        m_bg_color = wxColour("#ffffff");
    wxColour        m_border_color = wxColour("#ffffff");
    wxColour        m_border_hover_color = wxColour("#999999");
    wxColour        m_border_press_color = wxColour("#328DFB");

    static std::map<unsigned short, wxBitmap> m_machineBitmapMap;
};


struct StringCompareFunc {
    bool operator()(const std::string& lhs, const std::string& rhs) const {
        return lhs < rhs;
    }
};
typedef std::map<std::string, FilterPopupWindow::FilterItem*, StringCompareFunc> FilterItemMap;

class DeviceListUpdateEvent;
class DeviceListPanel : public wxPanel
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
            const wxSize& size = wxDefaultSize, long style = wxTAB_TRAVERSAL, const wxString& name = wxEmptyString);
    ~DeviceListPanel();

    void msw_rescale();

private:
    void build();
    void initAllDeviceStatus(wxArrayString& names);
    void connectEvent();
    void initLocalDevice(std::map<std::string, DeviceItemPanel::DeviceInfo>& deviceInfoMap);
    void initDeviceList();
    void updateFilterMap();
    void updatePlacementMap();
    void updateStatusMap();
    void updateTypeMap();
    void filterDeviceList();

    void onFilterButtonClicked(wxMouseEvent &event);
    void onNetworkTypeToggled(wxCommandEvent& event);
    void on_static_mode_toggled(wxCommandEvent &event);
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
    wxScrolledWindow* m_device_window {nullptr};
    wxGridSizer*    m_device_sizer {nullptr};
    
    FilterPopupType m_filter_popup_type {Filter_Popup_Type_None};
    FilterPopupWindow* m_filter_popup {nullptr};
    std::map<std::string, DeviceItemPanel*> m_device_map;
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
