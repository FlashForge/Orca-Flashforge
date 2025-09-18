#ifndef _Slic3r_GUI_TimeLapseVideoPanel_hpp_
#define _Slic3r_GUI_TimeLapseVideoPanel_hpp_

#include <map>
#include <set>
#include <wx/bitmap.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/graphics.h>
#include <wx/panel.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/wx.h>
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/Widgets/FFButton.hpp"
#include "slic3r/GUI/wxExtensions.hpp"
#include "slic3r/GUI/FlashForge/FFDownloadTool.hpp"
#include "slic3r/GUI/FlashForge/MultiComEvent.hpp"

namespace Slic3r { namespace GUI {

class TimeLapseVideoItem : public wxPanel
{
public:
    TimeLapseVideoItem(wxWindow *parent, int itemIdx);

    int getItemIdx() { return m_itemIdx; }

    const std::string &getJobId() { return m_jobId; }

    const wxString getFileName() const { return m_fileName; }

    const std::string &getVideoUrl() { return m_videoUrl; }

    int getVideoWidth() { return m_videoWidth; }

    int getVideoHeight() { return m_videoHeight; }

    bool getSelect() const { return m_select; }

    void setData(const fnet_time_lapse_video_data_t &videoData);

    void setThumbImage(const std::vector<char> &data);

private:
    void onPaint(wxPaintEvent &event);

    void onLeave(wxEvent &event);

    void onMotion(wxMouseEvent &event);

    void onLeftDown(wxMouseEvent &event);

    void onLeftUp(wxMouseEvent &event);

    void onMouseCaptureLost(wxMouseCaptureLostEvent &event);

private:
    int            m_itemIdx;
    std::string    m_jobId;
    wxString       m_fileName;
    std::string    m_videoUrl;
    int            m_videoWidth;
    int            m_videoHeight;
    bool           m_drawThumbImg;
    wxBitmap       m_thumbWxBmp;
    ScalableBitmap m_loadingBmp;
    ScalableBitmap m_flashforgeBmp;
    bool           m_hoverPlay;
    bool           m_pressPlay;
    wxRect         m_thumbRect;
    bool           m_select;
    bool           m_hoverSelRect;
    bool           m_pressSelRect;
    wxRect         m_selRect;
    ScalableBitmap m_selOnNormalBmp;
    ScalableBitmap m_selOnHoverBmp;
    ScalableBitmap m_selOffNormalBmp;
    ScalableBitmap m_selOffHoverBmp;
    ScalableBitmap m_playHoverBmp;
};

struct download_video_data_t {
    int sequence;
    bool succeed;
    wxString tmpSaveName;
    wxString fileName;
};

class TimeLapseVideoPanel : public wxPanel
{
public:
    TimeLapseVideoPanel(wxWindow *parent);

    ~TimeLapseVideoPanel();

    void setComId(com_id_t comId);

    void updateVideoList();

private:
    void onGetVideoList(ComGetTimeLapseVideoListEvent &event);

    void onSelectChange(wxCommandEvent &event);

    void onDelete(wxCommandEvent &event);

    void onDeleteFinish(ComDeleteTimeLapseVideoEvent &event);

    void onDownload(wxCommandEvent &event);

    void onDownloadFinish(FFDownloadFinishedEvent &event);

    void onPlayVideo(wxCommandEvent &event);

    void clearVideoList();

    void updateButtonState();

    void showDownloadResult();

    wxString getSaveName(const wxString &dirName, const wxString &fileName, bool tmp);

    using download_thumb_item_map_t = std::map<int, int>;

    using download_video_data_map_t = std::map<int, download_video_data_t>;

private:
    com_id_t                  m_comId;
    wxGridSizer              *m_itemSizer;
    wxScrolledWindow         *m_scr;
    wxBoxSizer               *m_btnSizer;
    FFButton                 *m_deleteBtn;
    FFButton                 *m_downloadBtn;
    FFDownloadTool            m_downloadTool;
    download_thumb_item_map_t m_downloadThumbItemMap;
    int                       m_downloadingVideoComId;
    std::set<int>             m_downloadingVideoTaskSet;
    wxString                  m_downloadVideoSaveDir;
    download_video_data_map_t m_downloadVideoDataMap;
    std::set<wxString>        m_downloadSaveNameSet;
};

}} // namespace Slic3r::GUI

#endif
