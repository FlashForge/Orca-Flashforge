#include "AmsMappingWidgets.hpp"
#include <memory>
#include <string>
#include <wx/dcgraph.h>
#include <wx/stattext.h>
#include "slic3r/GUI/FFUtils.hpp"
#include "slic3r/GUI/FlashForge/MultiComMgr.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/Widgets/Label.hpp"

namespace Slic3r { namespace GUI {

SlotInfoWgt::SlotInfoWgt(wxWindow *parent)
    : wxPanel(parent)
    , m_type(TYPE_GENERAL)
    , m_slotId(0)
    , m_color(*wxWHITE)
    , m_empty(true)
    , m_hover(false)
    , m_transBmp(this, "filament_reel_trans", 68)
    , m_transStrokeBmp(this, "filament_reel_trans_stroke", 68)
    , m_unknownBmp(this, "filament_reel_unknown", 68)
    , m_emptyBmp(this, "filament_reel_empty", 68)
    , m_u1EmptyNozzle(true)
    , m_u1UnknowBmp(this, "unknown_u1_mat", 68)
    , m_u1EmptyBmp(this, "empty_u1_mat", 68)
    , m_u1EmptyNozzleBmp(this, "empty_u1_nozzle", 68)
{
    SetDoubleBuffered(true);
    SetSize(wxSize(FromDIP(61), FromDIP(102)));
    SetMinSize(GetSize());
    SetMaxSize(GetSize());
    Enable(false);
    Bind(wxEVT_PAINT, &SlotInfoWgt::onPaint, this);
}

void SlotInfoWgt::setInfo(int slotId, wxColour color, wxString name, bool empty, wxString mappingName)
{
    m_slotId = slotId;
    m_color = color;
    m_name = name.Strip();
    m_empty = empty;
    Enable(!m_empty && !m_name.empty() && m_name.IsSameAs(mappingName, false));
    if (!IsEnabled()) {
        m_hover = false;
    }
    Refresh();
    Update();
}

void SlotInfoWgt::setHover(bool hover)
{
    if (hover != m_hover) {
        m_hover = hover;
        Refresh();
        Update();
    }
}

void SlotInfoWgt::onPaint(wxPaintEvent &evt)
{
    if (m_type == TYPE_GENERAL) {
        renderGeneral();
    } else if (m_type == TYPE_U1) {
        renderU1();
    }
}

void SlotInfoWgt::renderGeneral()
{
    wxPaintDC dc(this);
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (gc == nullptr) {
        return;
    }
    // slot
    if (m_hover) {
        gc->SetPen(*wxTRANSPARENT_PEN);
        gc->SetBrush(wxColour("#95c5ff"));
    } else {
        gc->SetPen(*wxTRANSPARENT_PEN);
        gc->SetBrush(wxColour("#dddddd"));
    }
    wxSize size = GetSize();
    int slotCircleSize = FromDIP(24);
    gc->DrawEllipse((size.x - slotCircleSize) / 2, FromDIP(0), slotCircleSize, slotCircleSize);
    wxString slotTxt = std::to_string(m_slotId);
    wxSize slotTxtSize = dc.GetTextExtent(slotTxt);
    dc.SetFont(::Label::Body_13);
    dc.SetTextForeground(*wxWHITE);
    dc.DrawText(slotTxt, (size.x - slotTxtSize.x) / 2, (slotCircleSize - slotTxtSize.y) / 2);

    // filament reel
    int  filamentReelHeight = FromDIP(68);
    bool useStrokeBmp = m_color.GetLuminance() > 0.95;
    if (m_empty) {
        gc->DrawBitmap(m_emptyBmp.bmp(), 0, size.y - filamentReelHeight, size.x, filamentReelHeight);
    } else if (m_name.empty()) {
        gc->DrawBitmap(m_unknownBmp.bmp(), 0, size.y - filamentReelHeight, size.x, filamentReelHeight);
    } else {
        const wxBitmap& bmp = useStrokeBmp ? m_transStrokeBmp.bmp() : m_transBmp.bmp();
        gc->SetPen(*wxTRANSPARENT_PEN);
        gc->SetBrush(wxBrush(m_color));
        gc->DrawRectangle(0, size.y - filamentReelHeight, size.x, filamentReelHeight);
        gc->DrawBitmap(bmp, 0, size.y - filamentReelHeight, size.x, filamentReelHeight);
    }

    // name
    if (!m_empty && !m_name.empty()) {
        int nameMaxWidth = useStrokeBmp ? FromDIP(28) : FromDIP(30);
        wxString showName = FFUtils::wrapString(dc, m_name, nameMaxWidth);
        wxSize nameTxtSize = dc.GetMultiLineTextExtent(showName);
        int nameTxtX = (nameMaxWidth - nameTxtSize.x) / 2;
        int nameTxtY = size.y - filamentReelHeight + (filamentReelHeight - nameTxtSize.y) / 2;
        if (!showName.empty() && m_color.GetLuminance() < 0.6) {
            dc.SetTextForeground(*wxWHITE);
        } else {
            dc.SetTextForeground(wxColour("#434343"));
        }
        dc.SetFont(::Label::Body_10);
        dc.DrawText(showName, nameTxtX + FromDIP(30), nameTxtY + FromDIP(4));
    }
}

void SlotInfoWgt::renderU1()
{
    wxPaintDC dc(this);
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (gc == nullptr) {
        return;
    }
    // slot
    wxSize size = GetSize();
    int slotNumberSize = FromDIP(24);
    wxString slotTxt = std::to_string(m_slotId);
    wxSize slotTxtSize = dc.GetTextExtent(slotTxt);
    dc.SetFont(::Label::Body_13);
    dc.SetTextForeground(*wxBLACK);
    dc.DrawText(slotTxt, (size.x - slotTxtSize.x) / 2, (slotNumberSize - slotTxtSize.y) / 2);

    // filament reel
    int filamentReelHeight = FromDIP(68);
    if (m_empty) {
        gc->DrawBitmap(m_u1EmptyBmp.bmp(), 0, size.y - filamentReelHeight, size.x, filamentReelHeight);
    } else if (m_name.empty()) {
        gc->DrawBitmap(m_u1UnknowBmp.bmp(), 0, size.y - filamentReelHeight, size.x, filamentReelHeight);
    } else if (m_u1EmptyNozzle) {
        gc->DrawBitmap(m_u1EmptyNozzleBmp.bmp(), 0, size.y - filamentReelHeight, size.x, filamentReelHeight);
    } else {
        dc.SetBrush(wxBrush(m_color));
        dc.SetPen(wxPen(wxColor(196, 196, 196), 1));
        dc.DrawRoundedRectangle(0, 0, filamentReelHeight, filamentReelHeight, 4.0);
    }

    // name
    if (!m_empty && !m_name.empty() && !m_u1EmptyNozzle) {
        int nameMaxWidth = FromDIP(30);
        wxString showName = FFUtils::wrapString(dc, m_name, nameMaxWidth);
        wxSize nameTxtSize = dc.GetMultiLineTextExtent(showName);
        int nameTxtX = (nameMaxWidth - nameTxtSize.x) / 2;
        int nameTxtY = size.y - filamentReelHeight + (filamentReelHeight - nameTxtSize.y) / 2;
        if (!showName.empty() && m_color.GetLuminance() < 0.6) {
            dc.SetTextForeground(*wxWHITE);
        } else {
            dc.SetTextForeground(wxColour("#434343"));
        }
        dc.SetFont(::Label::Body_10);
        dc.DrawText(showName, nameTxtX + FromDIP(30), nameTxtY + FromDIP(4));
    }
}

wxDEFINE_EVENT(SOLT_SELECT_EVENT, SlotSelectEvent);

SlotSelectWnd::SlotSelectWnd(wxWindow *parent, wxString mappingName)
    : FFTransientWindow(parent, _L("Material in the material station"))
    , m_mappingName(mappingName)
    , m_comId(ComInvalidId)
    , m_slotInfoWgtsSizer(new wxGridSizer(1, 4, FromDIP(10), FromDIP(20)))
{
    wxStaticText *tipLbl = new wxStaticText(this, wxID_ANY, _L("Only support selecting materials of the same type"));
    tipLbl->SetForegroundColour(wxColour("#f59a23"));

    wxBoxSizer *centralSizer = new wxBoxSizer(wxHORIZONTAL);
    centralSizer->AddSpacer(FromDIP(72));
    centralSizer->Add(m_slotInfoWgtsSizer);
    centralSizer->AddSpacer(FromDIP(72));

    MainSizer()->AddSpacer(FromDIP(9));
    MainSizer()->Add(centralSizer);
    MainSizer()->AddSpacer(FromDIP(10));
    MainSizer()->Add(tipLbl, 0, wxALIGN_CENTER);
    MainSizer()->AddSpacer(FromDIP(16));
    Layout();
    Fit();

    Bind(wxEVT_LEFT_DOWN, &SlotSelectWnd::onLeftDown, this);
    Bind(wxEVT_MOTION, &SlotSelectWnd::onMotion, this);
    MultiComMgr::inst()->Bind(COM_CONNECTION_EXIT_EVENT, &SlotSelectWnd::onComConnectionExit, this);
    MultiComMgr::inst()->Bind(COM_DEV_DETAIL_UPDATE_EVENT, &SlotSelectWnd::onComDevDetailUpdate, this);
}

void SlotSelectWnd::setComId(com_id_t id)
{
    if (id == m_comId) {
        return;
    }
    m_comId = id;
    setupSlotInfoWgts();
}

// TODO: setup U1 info
void SlotSelectWnd::setupSlotInfoWgts()
{
    bool valid;
    const fnet_dev_detail_t *devDetail = MultiComMgr::inst()->devData(m_comId, &valid).devDetail;
    if (!valid) {
        return;
    }
    m_slotInfoWgtsSizer->Clear(true);
    m_slotInfoWgts.resize(4);
    for (size_t i = 0; i < m_slotInfoWgts.size(); ++i) {
        SlotInfoWgt *slotInfoWgt = new SlotInfoWgt(this);
        if (devDetail->hasMatlStation != 0 && i < devDetail->matlStationInfo.slotCnt) {
            const fnet_matl_slot_info_t &slotInfo = devDetail->matlStationInfo.slotInfos[i];
            slotInfoWgt->setInfo(slotInfo.slotId, slotInfo.materialColor, slotInfo.materialName,
                !slotInfo.hasFilament, m_mappingName);
        } else {
            slotInfoWgt->setInfo(i + 1, *wxWHITE, wxEmptyString, true, m_mappingName);
        }
        m_slotInfoWgtsSizer->Add(slotInfoWgt);
        m_slotInfoWgts[i] = slotInfoWgt;
    }
    Layout();
    Fit();
}

void SlotSelectWnd::onLeftDown(wxMouseEvent &evt)
{
    evt.Skip();
    wxPoint pos = evt.GetPosition();
    for (auto slotInfoWgt : m_slotInfoWgts) {
        if (slotInfoWgt->IsEnabled()) {
            wxPoint pos1 = slotInfoWgt->ScreenToClient(ClientToScreen(pos));
            if (slotInfoWgt->HitTest(pos1) == wxHT_WINDOW_INSIDE) {
                SlotSelectEvent *event = new SlotSelectEvent(
                    SOLT_SELECT_EVENT, slotInfoWgt->slotId(), slotInfoWgt->color());
                QueueEvent(event);
                Show(false);
                slotInfoWgt->setHover(false);
            }
        }
    }
}

void SlotSelectWnd::onMotion(wxMouseEvent &evt)
{
    evt.Skip();
    wxPoint pos = evt.GetPosition();
    if (HitTest(pos) == wxHT_WINDOW_OUTSIDE) {
        return;
    }
    wxStockCursor cursor = wxCURSOR_ARROW;
    for (auto slotInfoWgt : m_slotInfoWgts) {
        wxPoint pos1 = slotInfoWgt->ScreenToClient(ClientToScreen(pos));
        bool isHit = slotInfoWgt->HitTest(pos1) == wxHT_WINDOW_INSIDE;
        if (slotInfoWgt->IsEnabled()) {
            slotInfoWgt->setHover(isHit);
        }
        if (isHit) {
            cursor = slotInfoWgt->IsEnabled() ? wxCURSOR_HAND : wxCURSOR_NO_ENTRY;
        }
    }
    SetCursor(wxCursor(cursor));
}

void SlotSelectWnd::onComConnectionExit(ComConnectionExitEvent &evt)
{
    evt.Skip();
    if (evt.id != m_comId) {
        return;
    }
    Show(false);
}

void SlotSelectWnd::onComDevDetailUpdate(ComDevDetailUpdateEvent &evt)
{
    evt.Skip();
    if (evt.id != m_comId) {
        return;
    }
    bool valid;
    const fnet_dev_detail_t *devDetail = MultiComMgr::inst()->devData(m_comId, &valid).devDetail;
    if (!valid) {
        return;
    }
    for (size_t i = 0; i < m_slotInfoWgts.size(); ++i) {
        if (devDetail->hasMatlStation != 0 && i < devDetail->matlStationInfo.slotCnt) {
            const fnet_matl_slot_info_t &slotInfo = devDetail->matlStationInfo.slotInfos[i];
            m_slotInfoWgts[i]->setInfo(slotInfo.slotId, slotInfo.materialColor,
                slotInfo.materialName, !slotInfo.hasFilament, m_mappingName);
        }
    }
}

wxDEFINE_EVENT(SOLT_RESET_EVENT, SlotResetEvent);

wxColour MaterialMapWgt::DisbaleColor(0xdd, 0xdd, 0xdd);

MaterialMapWgt::MaterialMapWgt(wxWindow *parent, int toolId, wxColour color, wxString name)
    : wxPanel(parent)
    , m_toolId(toolId)
    , m_color(color)
    , m_name(name.Strip())
    , m_amsColor(DisbaleColor)
    , m_amsSlotId(0)
    , m_selected(false)
    , m_size(FromDIP(70), FromDIP(58))
    , m_radius(FromDIP(3))
    , m_arrawWhiteBmp(this, "ff_drop_down_white", 6)
    , m_arrawBlackBmp(this, "ff_drop_down_black", 6)
    , m_soltSelectWnd(new SlotSelectWnd(parent, m_name))
 {
    SetDoubleBuffered(true);
    SetSize(m_size);
    SetMinSize(m_size);
    SetMaxSize(m_size);

    Bind(wxEVT_PAINT, &MaterialMapWgt::onPaint, this);
    Bind(wxEVT_LEFT_DOWN, &MaterialMapWgt::onLeftDown, this);
    m_soltSelectWnd->Bind(wxEVT_SHOW, &MaterialMapWgt::onSlotSelectWndShow, this);
    m_soltSelectWnd->Bind(SOLT_SELECT_EVENT, &MaterialMapWgt::onSlotSelected, this);
    MultiComMgr::inst()->Bind(COM_DEV_DETAIL_UPDATE_EVENT, &MaterialMapWgt::onComDevDetailUpdate, this);
}

void MaterialMapWgt::setEnable(bool enable)
{
    if (IsEnabled() == enable) {
        return;
    }
    if (!enable) {
        m_amsColor = DisbaleColor;
        m_amsSlotId = 0;
    }
    Enable(enable);
    Refresh();
    Update();
}

void MaterialMapWgt::setupSlot(int comId, int slotId)
{
    bool valid;
    const fnet_dev_detail_t *devDetail = MultiComMgr::inst()->devData(comId, &valid).devDetail;
    if (!valid || devDetail->hasMatlStation == 0) {
        return;
    }
    for (int i = 0; i < devDetail->matlStationInfo.slotCnt; ++i) {
        const fnet_matl_slot_info_t &slotInfo = devDetail->matlStationInfo.slotInfos[i];
        if (slotId == slotInfo.slotId) {
            wxString materialName = wxString::FromUTF8(slotInfo.materialName).Strip();
            if (slotInfo.hasFilament && m_name.IsSameAs(materialName, false)) {
                m_amsColor = slotInfo.materialColor;
                m_amsSlotId = slotId;
                Refresh();
                Update();
            }
            break;
        }
    }
}

void MaterialMapWgt::resetSlot()
{
    m_amsColor = DisbaleColor;
    m_amsSlotId = 0;
    Refresh();
    Update();
}

com_material_mapping_t MaterialMapWgt::getMaterialMapping()
{
    com_material_mapping_t materialMapping;
    materialMapping.toolId = m_toolId;
    materialMapping.slotId = m_amsSlotId;
    materialMapping.materialName = m_name.ToUTF8().data();
    materialMapping.toolMaterialColor = m_color.GetAsString(wxC2S_HTML_SYNTAX).c_str();
    materialMapping.slotMaterialColor = m_amsColor.GetAsString(wxC2S_HTML_SYNTAX).c_str();
    return materialMapping;
}

void MaterialMapWgt::onPaint(wxPaintEvent &evt)
{
    wxPaintDC dc(this);
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (gc != nullptr) {
        draw(dc, gc.get());
    }
}

void MaterialMapWgt::onLeftDown(wxMouseEvent &evt)
{
    if (!m_selected) {
        wxPoint pos = ClientToScreen(wxPoint(0, GetRect().height + FromDIP(1)));
        m_soltSelectWnd->Move(pos);
        m_soltSelectWnd->Show(true);
    } else {
        m_soltSelectWnd->Show(false);
    }
}

void MaterialMapWgt::onSlotSelectWndShow(wxShowEvent &evt)
{
    CallAfter([this, isShown = evt.IsShown()]() {
        m_selected = isShown;
        Refresh();
        Update();
    });
}

void MaterialMapWgt::onSlotSelected(SlotSelectEvent &evt)
{
    if (m_amsColor == evt.color && m_amsSlotId == evt.slotId) {
        return;
    }
    m_amsColor = evt.color;
    m_amsSlotId = evt.slotId;
    Refresh();
    Update();
    QueueEvent(evt.Clone());
}

void MaterialMapWgt::onComDevDetailUpdate(ComDevDetailUpdateEvent &evt)
{
    evt.Skip();
    if (evt.id != m_soltSelectWnd->getComId()) {
        return;
    }
    bool valid;
    const fnet_dev_detail_t *devDetail = MultiComMgr::inst()->devData(evt.id, &valid).devDetail;
    if (!valid || devDetail->hasMatlStation == 0) {
        return;
    }
    for (int i = 0; i < devDetail->matlStationInfo.slotCnt; ++i) {
        const fnet_matl_slot_info_t &slotInfo = devDetail->matlStationInfo.slotInfos[i];
        if (m_amsSlotId == slotInfo.slotId) {
            wxString materialName = wxString::FromUTF8(slotInfo.materialName).Strip();
            if (slotInfo.hasFilament && m_name.IsSameAs(materialName, false)) {
                m_amsColor = slotInfo.materialColor;
                Refresh();
                Update();
            } else {
                resetSlot();
                QueueEvent(new SlotResetEvent(SOLT_RESET_EVENT));
            }
            break;
        }
    }
}

void MaterialMapWgt::draw(wxPaintDC &dc, wxGraphicsContext *gc)
{
    // top
    int halfHeight = m_size.y / 2;
    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->SetBrush(wxBrush(m_color));
    gc->DrawRoundedRectangle(0, 0, m_size.x, halfHeight, m_radius);
    gc->DrawRectangle(0, halfHeight - m_radius, m_size.x, m_radius);

    // bottom
    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->SetBrush(wxBrush(m_amsColor));
    gc->DrawRoundedRectangle(0, halfHeight, m_size.x, halfHeight, m_radius);
    gc->DrawRectangle(0, halfHeight, m_size.x, m_radius);

    // border
    if (m_selected) {
        gc->SetPen(wxColour(0x00, 0xAE, 0x42));
        gc->SetBrush(*wxTRANSPARENT_BRUSH);
        gc->DrawRoundedRectangle(0, 0, m_size.x - 1, m_size.y - 1, m_radius);
    } else if (m_color.GetLuminance() > 0.95 || m_amsColor.GetLuminance() > 0.95) {
        gc->SetPen(wxColour(0xAC, 0xAC, 0xAC));
        gc->SetBrush(*wxTRANSPARENT_BRUSH);
        gc->DrawRoundedRectangle(0, 0, m_size.x - 1, m_size.y - 1, m_radius);
    }

    // material name
    if (m_color.GetLuminance() < 0.6) {
        dc.SetTextForeground(*wxWHITE);
    } else {
        dc.SetTextForeground(wxColour("#404040"));
    }
    dc.SetFont(::Label::Body_13);
    wxSize nameSize = dc.GetTextExtent(m_name);
    if (nameSize.x > GetSize().x - FromDIP(10)) {
        dc.SetFont(::Label::Body_10);
        nameSize = dc.GetTextExtent(m_name);
    }
    dc.DrawText(m_name, (m_size.x - nameSize.x) / 2, (halfHeight - nameSize.y) / 2);

    // mapping slot
    if (m_amsColor.GetLuminance() < 0.6) {
        dc.SetTextForeground(*wxWHITE);
    } else {
        dc.SetTextForeground(wxColour("#404040"));
    }
    dc.SetFont(::Label::Body_13);
    wxString slotTxt;
    if (!isSlotSelected()) {
        slotTxt = "-";
    } else {
        slotTxt = std::to_string(m_amsSlotId);
    }
    wxSize arrowBmpSize = m_arrawWhiteBmp.GetBmpSize();
    wxSize slotSize = dc.GetTextExtent(slotTxt);
    int slotArrowSpace = FromDIP(4);
    int slotTxtX = FromDIP(2) + (m_size.x - slotSize.x - arrowBmpSize.x - slotArrowSpace) / 2;
    int slotTxtY = halfHeight + (halfHeight - slotSize.y) / 2;
    dc.DrawText(slotTxt, slotTxtX, slotTxtY);

    //arrow
    int arrowX = slotTxtX + slotSize.x + slotArrowSpace;
    int arrowY = halfHeight + (halfHeight - arrowBmpSize.y) / 2;
    if (m_amsColor.GetLuminance() < 0.6) {
        dc.DrawBitmap(m_arrawWhiteBmp.bmp(), arrowX, arrowY);
    } else {
        dc.DrawBitmap(m_arrawBlackBmp.bmp(), arrowX, arrowY);
    }
}

AmsTipWnd::AmsTipWnd(wxWindow *parent)
    : FFRoundedWindow(parent)
{
    SetSize(wxSize(FromDIP(400), FromDIP(125)));
    Bind(wxEVT_PAINT, &AmsTipWnd::onPaint, this);
}

void AmsTipWnd::onPaint(wxPaintEvent &evt)
{
    FFRoundedWindow::OnPaint(evt);
    
    wxPaintDC dc(this);
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (gc == nullptr) {
        return;
    }
    wxSize size = GetSize();
    int iconLeft = size.x * 0.086;
    int iconVertMid = size.y * 0.5;
    int iconWidth = FromDIP(70);
    int iconHalfHeight = FromDIP(29);
    int iconRadius = FromDIP(3);
    int iconRight = iconLeft + iconWidth;
    int iconTop = iconVertMid - iconHalfHeight;
    int iconBottom = iconVertMid + iconHalfHeight;

    // icon
    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->SetBrush(wxColour("#d9001b"));
    gc->DrawRoundedRectangle(iconLeft, iconTop, iconWidth, iconHalfHeight, iconRadius);
    gc->DrawRectangle(iconLeft, iconVertMid - iconRadius, iconWidth, iconRadius);

    gc->SetBrush(wxColour("#f59a23"));
    gc->DrawRoundedRectangle(iconLeft, iconVertMid, iconWidth, iconHalfHeight, iconRadius);
    gc->DrawRectangle(iconLeft, iconVertMid, iconWidth, iconRadius);

    // lines
    int topLineY = iconTop + FromDIP(6);
    int bottomLineY = iconBottom - FromDIP(6);
    int lineRight = iconRight + FromDIP(27);
    wxGraphicsPath path = gc->CreatePath();
    path.MoveToPoint(iconRight, topLineY);
    path.AddLineToPoint(lineRight, topLineY);
    path.MoveToPoint(iconRight, bottomLineY);
    path.AddLineToPoint(lineRight, bottomLineY);
    gc->SetPen(wxColour("#c1c1c1"));
    gc->StrokePath(path);

    // texts
    dc.SetFont(::Label::Body_13);
    dc.SetTextForeground(*wxWHITE);
    drawIconText(dc, "PLA", wxRect(iconLeft, iconTop, iconWidth, iconHalfHeight));
    drawIconText(dc, "1", wxRect(iconLeft, iconVertMid, iconWidth, iconHalfHeight));

    int tutotrialLeft = lineRight + FromDIP(5);
    dc.SetTextForeground(*wxBLACK);
    drawTutorialText(dc, _L("Filament type and color set in the slicer"), tutotrialLeft, topLineY);
    drawTutorialText(dc, _L("Slot number and the color of the\nfilament in the current slot"), tutotrialLeft, bottomLineY);
}

void AmsTipWnd::drawIconText(wxPaintDC &dc, wxString text, wxRect rt)
{
    wxCoord width, height;
    dc.GetTextExtent(text, &width, &height);

    int x = rt.GetLeft() + (rt.width - width) / 2;
    int y = rt.GetTop() + (rt.height - height) / 2;
    dc.DrawText(text, x, y);
}

void AmsTipWnd::drawTutorialText(wxPaintDC &dc, wxString text, int left, int vertMid)
{
    wxCoord width, height;
    dc.GetTextExtent(text, &width, &height);
    dc.DrawText(text, left, vertMid - height / 2);
}

}} // namespace Slic3r::GUI
