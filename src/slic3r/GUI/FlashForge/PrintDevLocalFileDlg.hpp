#ifndef slic3r_PrintDevLocalFileDlg_hpp_
#define slic3r_PrintDevLocalFileDlg_hpp_

#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>
#include "slic3r/GUI/FlashForge/AmsMappingWidgets.hpp"
#include "slic3r/GUI/FlashForge/MultiComDef.hpp"
#include "slic3r/GUI/FlashForge/MultiComEvent.hpp"
#include "slic3r/GUI/GUI_Utils.hpp"
#include "slic3r/GUI/Widgets/FFButton.hpp"
#include "slic3r/GUI/Widgets/FFCheckBox.hpp"

namespace Slic3r { namespace GUI {
    
class PrintDevLocalFileDlg : public DPIDialog
{
public:
    PrintDevLocalFileDlg(wxWindow *parent);

    int ShowModal(com_local_job_data_t &jobData);

    bool setupData(com_id_t comId, const com_gcode_data_t &gcodeData, const wxImage &thumb);

private:
    void on_dpi_changed(const wxRect &suggested_rect) {}

    void onLevellingStateChanged(wxCommandEvent &event);

    void onEnableAmsStateChanged(wxCommandEvent &event);
    
    void onEnterAmsTipWidget(wxMouseEvent &event);

    void onFlowCalibrationStateChanged(wxCommandEvent &event);

    void onFirstLayerInspectionStateChanged(wxCommandEvent& event);
    
    void onTimeLapseVideoStateChanged(wxCommandEvent& event);

    void onPrintButtonClicked(wxCommandEvent &event);

    void onConnectionExit(ComConnectionExitEvent &event);

    void onWanDevInfoUpdate(ComWanDevInfoUpdateEvent &event);

    void onDevDetailUpdate(ComDevDetailUpdateEvent &event);

    bool updateConfigState(const fnet_dev_detail_t *detail, bool isInit = false);

    void updatePrintButtonState();
    
    wxPanel *makeLineSpacer();

private:
    wxBoxSizer      *m_mainSizer;
    wxPanel         *m_topPnl;
    wxStaticText    *m_nameLbl;
    wxStaticBitmap  *m_thumbWxBmp;
    wxStaticBitmap  *m_timeWxBmp;
    wxStaticText    *m_timeLbl;
    wxStaticBitmap  *m_weightWxBmp;
    wxStaticText    *m_weightLbl;
    wxPanel         *m_materialPnl;
    wxGridSizer     *m_materialSizer;
    wxStaticText    *m_amsTipLbl;
    wxFlexGridSizer *m_printConfigSizer;
    FFCheckBox      *m_levelChk;
    wxStaticText    *m_levelLbl;
    FFCheckBox      *m_enableAmsChk;
    wxStaticText    *m_enableAmsLbl;
    wxStaticBitmap  *m_amsTipWxBmp;
    AmsTipWnd       *m_amsTipWnd;
    FFCheckBox      *m_flowCalibrationChk;
    wxStaticText    *m_flowCalibrationLbl;
    FFCheckBox      *m_firstLayerInspectionChk;
    wxStaticText    *m_firstLayerInspectionLbl;
    FFCheckBox      *m_timeLapseVideoChk;
    wxStaticText    *m_timeLapseVideoLbl;
    FFButton        *m_printBtn;
    com_id_t         m_comId;
    bool             m_isSupportLidar;
    bool             m_isSupportCamera;
    std::vector<MaterialMapWgt*> m_materialMapItems;
};

}} // namespace Slic3r::GUI

#endif
