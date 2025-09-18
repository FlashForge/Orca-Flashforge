#ifndef _Slic3r_GUI_PrinterErrorMsgDlg_hpp_
#define _Slic3r_GUI_PrinterErrorMsgDlg_hpp_

#include <string>
#include <wx/event.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include "slic3r/GUI/FlashForge/MultiComDef.hpp"
#include "slic3r/GUI/FlashForge/MultiComEvent.hpp"
#include "slic3r/GUI/Widgets/FFButton.hpp"

namespace Slic3r { namespace GUI {

class PrinterErrorMsgDlg : public wxDialog
{
public:
    PrinterErrorMsgDlg(wxWindow *parent, com_id_t comId, const std::string &errorCode);

private:
    void setupErrorCode(const std::string &errorCode);
    void onContinue(wxCommandEvent &event);
    void onStop(wxCommandEvent &event);
    void onConnectionExit(ComConnectionExitEvent &event);
    void onDevDetailUpdate(ComDevDetailUpdateEvent &event);

private:
    com_id_t m_comId;
    std::string m_errorCode;
    wxStaticText *m_titleLbl;
    wxStaticText *m_msgLbl;
    FFButton *m_continueBtn;
    FFButton *m_stopBtn;
};

}} // namespace Slic3r::GUI

#endif
