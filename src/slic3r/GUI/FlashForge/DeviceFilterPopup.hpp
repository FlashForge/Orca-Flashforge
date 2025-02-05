#ifndef slic3r_DeviceFilterPopup_hpp_
#define slic3r_DeviceFilterPopup_hpp_
#include <vector>
#include <wx/wx.h>
#include "slic3r/GUI/Widgets/PopupWindow.hpp"
#include "slic3r/GUI/Widgets/FFPopupWindow.hpp"

class wxBoxSizer;
class wxStaticText;
class FFToggleButton;
class FFBitmapToggleButton;
class FFCheckBox;

namespace Slic3r {
namespace GUI {

class DeviceFilterEvent : public wxCommandEvent
{
public:
    DeviceFilterEvent(wxEventType type, int id, const std::string& full, const std::string& elide, int int_value, wxWindow* object)
    : wxCommandEvent(type, id), eventObject(object), fullStringValue(full), elidedStringValue(elide), intValue(int_value)
    {}

    wxEvent *Clone() const {
        return new DeviceFilterEvent(GetEventType(), GetId(), fullStringValue, elidedStringValue, intValue, eventObject);
    }

    wxWindow*       eventObject;
    std::string     fullStringValue;
    std::string     elidedStringValue;
    int             intValue;
};
wxDECLARE_EVENT(EVT_DEVICE_FILTER_ITEM_CLICKED, DeviceFilterEvent);

class DeviceFilterItem : public wxWindow
{
public:
    DeviceFilterItem(wxWindow* parent, const wxString& label, bool top_corner_round = false, bool bottom_corner_round = false);
    virtual ~DeviceFilterItem();
    
    void SetValid(bool valid) { m_valid_flag = valid; }
    bool IsValid() const { return m_valid_flag; }
    void SetSelect(bool select);
    bool IsSelect() const { return m_select_flag; }
    void SetHover(bool hover);
    void SetPressed(bool pressed, bool hit);
    bool IsPressed() const { return m_press_flag; }
    void SetTopCornerRound(bool round);
    void SetBottomCornerRound(bool round);
    void SetLabel(const wxString& label) override;

protected:
    virtual void onPaint(wxPaintEvent& event);
    void sendEvent(const wxString& full_data, const wxString& trim_data, int int_data);
    virtual void messureSize();
    virtual void updateChildrenBackground(const wxColour& color);
    virtual void mouseDownEvent() {};
    virtual void mouseUpEvent();

#ifndef __WXMAC__
    void onEnter(wxMouseEvent& event);
    void onLeave(wxMouseEvent& event);
    void onMouseDown(wxMouseEvent& event);
    void onMouseUp(wxMouseEvent& event);
    virtual wxPoint convertEventPoint(const wxMouseEvent& event);
#endif /* __WXMAC__ */

protected:
    bool            m_valid_flag {true};
    bool            m_hover_flag {false};
    bool            m_select_flag {false};
    bool            m_press_flag {false};
    bool            m_top_corner_round {false};
    bool            m_bottom_corner_round {false};
    wxBoxSizer*     m_main_sizer {nullptr};
    wxStaticText*   m_text {nullptr};
};

class DeviceStatusFilterItem final : public DeviceFilterItem
{
public:
    DeviceStatusFilterItem(wxWindow* parent, const std::string& status, bool top_corner_round = false, bool bottom_corner_round = false);

    const std::string& GetStatus() const { return m_status; }
    void SetStatus(const std::string& status);

protected:
    void mouseDownEvent() override {};
    void mouseUpEvent() override;

private:
    std::string     m_status;
};

class DeviceTypeFilterItem final : public DeviceFilterItem
{
public:
    DeviceTypeFilterItem(wxWindow* parent, unsigned short pid, bool checked = false, bool top_corner_round = false, bool bottom_corner_round = false);

    bool IsChecked() const;
    void SetChecked(bool checked);

protected:
    void updateChildrenBackground(const wxColour& color) override;
    void mouseDownEvent() override;
    void mouseUpEvent() override {};
    void messureSize();
    void updateBitmap();

private:
    bool            m_check_flag { false };
    unsigned short  m_pid;
    wxStaticBitmap* m_bitmap { nullptr };
    FFCheckBox*     m_check_box { nullptr };
};


#ifdef __WXMAC__
class DeviceFilterPopupWindow : public FFPopupWindow
{
public:
    DeviceFilterPopupWindow(wxWindow* parent);
    ~DeviceFilterPopupWindow();

    void Create();
    void Popup(wxWindow* focus = nullptr) override;
    void OnDismiss() override;
    void AddItem(DeviceFilterItem* item);
    void ClearItems();

private:
    void ProcessLeftDown(const wxPoint& pnt) override;
    void ProcessLeftUp(const wxPoint& pnt) override;
    void ProcessMotion(const wxPoint& pnt) override;

private:
    wxPoint         m_last_point;
    wxBoxSizer*     m_sizer;
    std::vector<DeviceFilterItem*> m_items;
};
#else

class DeviceFilterPopupWindow : public PopupWindow
{
public:
    DeviceFilterPopupWindow(wxWindow* parent);
    ~DeviceFilterPopupWindow();

    void Create();
    void Popup(wxWindow* focus = nullptr) override;
    void OnDismiss() override;
    void AddItem(DeviceFilterItem* item);
    void ClearItems();

private:
    void onPaint(wxPaintEvent& event);
    bool ProcessLeftDown(wxMouseEvent &event) override;

private:
    wxPoint         m_last_point;
    wxBoxSizer*     m_sizer;
    std::vector<DeviceFilterItem*> m_items;
};


#endif /* __WXMAC__ */

} // GUI
} // Slic3r

#endif /* slic3r_DeviceListPanel_hpp_ */
