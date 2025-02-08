#ifndef slic3r_AmsPrintFileDlg_hpp_
#define slic3r_AmsPrintFileDlg_hpp_

#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>
#include "slic3r/GUI/FlashForge/AmsMappingWidgets.hpp"
#include "slic3r/GUI/FlashForge/MultiComDef.hpp"
#include "slic3r/GUI/FlashForge/MultiComEvent.hpp"
#include "slic3r/GUI/TitleDialog.hpp"
#include "slic3r/GUI/Widgets/FFButton.hpp"
#include "slic3r/GUI/Widgets/FFCheckBox.hpp"

namespace Slic3r { namespace GUI {
    
class AmsPrintFileDlg : public TitleDialog
{
public:
    AmsPrintFileDlg(wxWindow *parent);

    int ShowModal(com_local_job_data_t &jobData);

    void setupData(com_id_t comId, const com_gcode_data_t &gcodeData, const wxImage &thumb);

private:
    void onLevellingStateChanged(wxCommandEvent &event);
    
    void onFlowCalibrationStateChanged(wxCommandEvent &event);

    void onEnableAmsStateChanged(wxCommandEvent &event);
    
    void onEnterAmsTipWidget(wxMouseEvent &event);

    void onPrintButtonClicked(wxCommandEvent &event);

    void onConnectionExit(ComConnectionExitEvent &event);

    void updatePrintButtonState();

    wxPanel *makeLineSpacer();

private:
    wxPanel         *m_topPnl;
    wxStaticText    *m_nameLbl;
    wxStaticBitmap  *m_thumbWxBmp;
    wxStaticText    *m_weightLbl;
    wxStaticText    *m_timeLbl;
    wxPanel         *m_materialPnl;
    wxGridSizer     *m_materialSizer;
    wxStaticText    *m_amsTipLbl;
    wxBoxSizer      *m_printConfigSizer;
    FFCheckBox      *m_levelChk;
    wxStaticText    *m_levelLbl;
    FFCheckBox      *m_flowCalibrationChk;
    wxStaticText    *m_flowCalibrationLbl;
    FFCheckBox      *m_enableAmsChk;
    wxStaticText    *m_enableAmsLbl;
    wxStaticBitmap  *m_amsTipWxBmp;
    AmsTipWnd       *m_amsTipWnd;
    FFButton        *m_printBtn;
    com_id_t         m_comId;
    std::vector<MaterialMapWgt*> m_materialMapItems;
};

}} // namespace Slic3r::GUI

#endif
