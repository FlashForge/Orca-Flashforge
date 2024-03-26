#ifndef slic3r_DeviceListPanel_hpp_
#define slic3r_DeviceListPanel_hpp_
#include <map>
#include <set>
#include <wx/simplebook.h>
#include <wx/tglbtn.h>
#include <wx/timer.h>
#include "slic3r/GUI/Widgets/Button.hpp"
#include "slic3r/GUI/Widgets/ComboBox.hpp"
#include "slic3r/GUI/Widgets/PopupWindow.hpp"
#include "MultiComMgr.hpp"
#include "MultiComEvent.hpp"
#include "DeviceData.hpp"

class FFToggleButton;
class FFBitmapToggleButton;
class FFCheckBox;

namespace Slic3r {
namespace GUI {


class DropDownButton : public wxPanel
{
public:
    DropDownButton(wxWindow* parent = nullptr, const wxString& text = wxEmptyString, const wxBitmap& bitmap = wxNullBitmap);
    ~DropDownButton();

    void setText(const wxString& text);

private:
    wxPoint convertEventPoint(const wxMouseEvent& event);
    void leaveWindow();
    void updateMinSize();
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


class FilterItemEvent : public wxCommandEvent
{
public:
    FilterItemEvent(wxEventType type, wxWindow* obj, const std::string& full, const std::string& trimmed, int int_value)
        : wxCommandEvent(type), filterObject(obj), fullStringValue(full), trimmedStringValue(trimmed), intValue(int_value) {}

    FilterItemEvent *Clone() const {
        return new FilterItemEvent(GetEventType(), filterObject, fullStringValue, trimmedStringValue, intValue);
    }

    wxWindow*       filterObject {nullptr};
    std::string     fullStringValue;
    std::string     trimmedStringValue;
    int             intValue;
};


wxDECLARE_EVENT(EVT_FILTER_ITEM_CLICKED, FilterItemEvent);
class FilterPopupWindow : public PopupWindow
{
public:
    class FilterItem : public wxPanel
    {
    public:
        FilterItem(wxWindow* parent, const wxString& text, bool top_corner_round = false, bool bottom_corner_round = false);
        virtual ~FilterItem();


        void setSelect(bool select);
        bool isSelect() const;
        void setTopCornerRound(bool round);
        void setBottomCornerRound(bool round);
        void setValid(bool valid) { m_validFlag = valid; }
        bool isValid() const { return m_validFlag; }

    protected:
        void onPaint(wxPaintEvent& event);
        void onEnter(wxMouseEvent& event);
        void onLeave(wxMouseEvent& event);
        void onMouseDown(wxMouseEvent& event);
        void onMouseUp(wxMouseEvent& event);
        void onMotion(wxMouseEvent& event);
        void onMouseCaptureLost(wxMouseCaptureLostEvent& event);
        bool isPointIn(const wxPoint& pnt);
        void leaveWindow();
        void sendEvent(const wxString& full_data, const wxString& trim_data, int int_data);
        void setText(const wxString& text);
        virtual wxPoint convertEventPoint(wxMouseEvent& event);
        virtual void updateChildrenBackground(const wxColour& color);
        virtual void mouseDownEvent() {};
        virtual void mouseUpEvent();
        virtual void updateMinSize();

    protected:
        bool            m_hoverFlag {false};
        bool            m_selectFlag {false};
        bool            m_pressFlag {false};
        bool            m_topCornerRound {false};
        bool            m_bottomCornerRound {false};
        bool            m_validFlag {true};
        wxBoxSizer*     m_main_sizer {nullptr};
        wxStaticText*   m_text {nullptr};
        wxString        m_text_value;
    };

    class StatusItem final : public FilterItem
    {
    public:
        StatusItem(wxWindow* parent, const std::string& status, bool top_corner_round = false, bool bottom_corner_round = false);

        const std::string& getStatus() const { return m_status; }
        void setStatus(const std::string& status);

    protected:
        void mouseUpEvent() override;

    private:
        std::string     m_status;
    };

    class DeviceTypeItem final : public FilterItem
    {
    public:
        DeviceTypeItem(wxWindow* parent, unsigned short pid, bool checked = false, bool top_corner_round = false, bool bottom_corner_round = false);

        bool isChecked() const;
        void setChecked(bool checked);

    protected:
        wxPoint convertEventPoint(wxMouseEvent& event) override;
        void updateChildrenBackground(const wxColour& color) override;
        void mouseUpEvent() override {};
        void mouseDownEvent() override;
        void updateMinSize() override;

    private:
        unsigned short  m_pid;
        FFCheckBox*     m_check_box { nullptr };
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
    void OnDismiss() override;

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
    void leaveWindow();
    void enterWindow();
    void mouseDown(wxMouseEvent &event);
    void mouseReleased(wxMouseEvent &event);
    void onEnter(wxMouseEvent &event);
    void onLeave(wxMouseEvent &event);
    void onMotion(wxMouseEvent& event);
    void onPaint(wxPaintEvent& event);
    void onMouseCaptionLost(wxMouseCaptureLostEvent& event);

protected:
    virtual wxPoint getEventPoint(wxMouseEvent& event);
    bool isPointIn(const wxPoint& pt);
    virtual void sendEvent() {};

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
        bool lanFlag {true};  // lan or not
        int conn_id {-1};
        unsigned short pid {0};
        std::string name;
        std::string placement;
        std::string status;
    };

    DeviceInfoItemPanel(wxWindow *parent, const DeviceInfo& info, wxWindow* event_handle = nullptr);

    void updateInfo(const DeviceInfo& info);
    const DeviceInfo& deviceInfo() const;
    void blockMouseEvent(bool block);

private:
    wxPoint getEventPoint(wxMouseEvent& event) override;
    void sendEvent() override;
    void updateStatus();
    static wxBitmap machineBitmap(unsigned short pid);

private:
    DeviceInfo      m_info;
    wxStaticText*   m_name_text {nullptr};
    wxStaticBitmap* m_icon {nullptr};
    wxStaticText*   m_placement_text {nullptr};
    wxStaticText*   m_status_text {nullptr};
    wxWindow*       m_event_handle {nullptr};
};
//wxDECLARE_EVENT(EVT_DEVICE_ITEM_SELECTED, wxCommandEvent);


class DeviceStaticItemPanel : public DeviceItemPanel
{
public:
    DeviceStaticItemPanel(wxWindow* parent, std::string status, int count);
    void setStatus(const std::string& status);
    std::string getStatus() const { return m_status; }
    void setCount(int count);
    int getCount() const { return m_count; }
    void blockMouseEvent(bool block);

private:
    wxPoint getEventPoint(wxMouseEvent& event) override;

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
typedef std::map<std::string, FilterPopupWindow::FilterItem*, StringCompareFunc> PlacementItemMap;
typedef std::map<std::string, FilterPopupWindow::StatusItem*, StringCompareFunc> StatusItemMap;
typedef std::map<unsigned short, FilterPopupWindow::DeviceTypeItem*> DeviceTypeItemMap;

//class DeviceListUpdateEvent;
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
            const wxSize& size = wxDefaultSize);
    ~DeviceListPanel();

    void msw_rescale();
    void OnActivate();

private:
    void build();
    void connectEvent();
    void initLocalDevice(std::map<std::string, DeviceInfoItemPanel::DeviceInfo>& deviceInfoMap);
    void initWlanDevice(std::map<std::string, DeviceInfoItemPanel::DeviceInfo>& deviceInfoMap);
    void initDeviceList();
    void updateFilterMap();
    void updatePlacementMap();
    void updateStatusMap();
    void updateTypeMap();
    void updateFilterTitle();
    void updateStaticMap();
    void updateDeviceSizer();
    void filterDeviceList();
    void updateDeviceWindowSize();
    bool getDeviceInfo(DeviceInfoItemPanel::DeviceInfo& info, int conn_id);
    void updateDeviceInfo(const std::string& dev_id, const DeviceInfoItemPanel::DeviceInfo& info);
    void updateDeviceList();
    void copyDeviceInfo(DeviceInfoItemPanel::DeviceInfo& dest, const DeviceInfoItemPanel::DeviceInfo& source);

    void onFilterButtonClicked(wxMouseEvent &event);
    void onNetworkTypeToggled(wxCommandEvent& event);
    void onStaticModeToggled(wxCommandEvent &event);
    void onDeviceListUpdated(DeviceListUpdateEvent& event);
    void onComDevDetailUpdate(ComDevDetailUpdateEvent& event);
    void onComWanDeviceInfoUpdate(ComWanDevInfoUpdateEvent& event);
    void onPopupShow(wxShowEvent& event);
    void onFilterItemClicked(FilterItemEvent& event);
    void onRefreshTimeout(wxTimerEvent& event);

private:
    struct DeviceKey {
        int priority {0};
        std::string dev_id;   // serial number
        std::string dev_name; // sort key

        DeviceKey(const std::string& devID) : dev_id(devID) {}
        DeviceKey(int _priority, const std::string& devID, const std::string& devName)
            : priority(_priority), dev_id(devID), dev_name(devName) {}
        bool operator < (const DeviceKey& other) const {
            return dev_id < other.dev_id;
        }
    };
    
    struct DeviceKeySortFunc {
        bool operator()(const DeviceKey& lhs, const DeviceKey& rhs) const {
            if (lhs.priority != rhs.priority) {
                return lhs.priority > rhs.priority;
            }
            return lhs.dev_name < rhs.dev_name;
        }
    };
    typedef std::map<DeviceKey, DeviceInfoItemPanel*> DeviceItemMap;
    typedef std::set<DeviceKey, DeviceKeySortFunc> DeviceKeySet;
    int generateNewPriorityId();
    void updatePriorityId();

    struct DeviceCacheData {
        std::string dev_id;
        DeviceListUpdateEvent::UpdateType op;
        DeviceInfoItemPanel::DeviceInfo device_info;
    };
    typedef std::map<DeviceKey, DeviceCacheData> DeviceCacheDataMap;

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
    wxScrolledWindow* m_device_scrolled_window {nullptr};
    wxPanel*        m_device_panel {nullptr};
    wxGridSizer*    m_device_sizer {nullptr};
    
    FilterPopupType m_filter_popup_type {Filter_Popup_Type_None};
    FilterPopupWindow* m_filter_popup {nullptr};

    DeviceCacheDataMap  m_device_data_cached;
    wxTimer             m_refresh_timer;
    DeviceItemMap       m_device_map;
    std::map<std::string, DeviceStaticItemPanel*> m_device_stat_map;
    FilterPopupWindow::FilterItem*  m_default_filter_item {nullptr};
    PlacementItemMap    m_placement_item_map;
    StatusItemMap       m_status_item_map;
    DeviceTypeItemMap   m_type_item_map;
    bool                m_filter_placement_default {true};
    std::string         m_filter_placement;
    std::string         m_filter_placement_trimmed;
    bool                m_filter_status_default {true};
    std::string         m_filter_status;
    std::set<unsigned short> m_filter_types;

    static int m_last_priority_id;
};

} // GUI
} // Slic3r

#endif /* slic3r_DeviceListPanel_hpp_ */
