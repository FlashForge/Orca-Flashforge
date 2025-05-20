#include "TimeLapseVideoPlayDlg.hpp"

#include "slic3r/GUI/I18N.hpp"

#include <wx/uri.h>
#include <wx/dir.h>

const char* htmlTemplate = R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <title>time lapse video</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            overflow: hidden;
        }
        body {
            width: 100vw;
            height: 100vh;
            background: black;
            display: flex;
            justify-content: center;
            align-items: center;
        }
        #video-container {
            position: relative;
            max-width: 100%;
            max-height: 100%;
        }
        video {
            width: 100vw;
            height: 100vh;
            max-width: 100%;
            max-height: 100%;
            vertical-align: middle;
        }
    </style>
</head>
<body>
    <div id="video-container">
        <video controls>
            <source src="%s">
        </video>
    </div>
    <script>
        function resizeVideo() {
            const video = document.querySelector('video');
            if (!video.videoWidth) return;
            const videoRatio = video.videoWidth / video.videoHeight;
            const windowRatio = window.innerWidth / window.innerHeight;
            if (windowRatio > videoRatio) {
                video.style.height = '100vh';
                video.style.width = 'auto';
            } else {
                video.style.width = '100vw';
                video.style.height = 'auto';
            }
        }
        window.addEventListener('pageshow', () => {
            resizeVideo();
        });
        window.addEventListener('resize', resizeVideo);
    </script>
</body>
</html>

)";

namespace Slic3r {
namespace GUI {

    TimeLapseVideoPlayDlg::TimeLapseVideoPlayDlg(wxWindow* parent, const wxString& filepath, const wxString& video_url, int width, int height)
    : wxDialog(parent, wxID_ANY, _L("Time-Lapse Video"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
        , m_filepath(filepath)
        , m_video_url(video_url)
        , m_width(width)
        , m_height(height)
    {
        m_filepath2 = filepath;
#ifdef __WIN32__
        m_filepath.Replace("\\", "/");
        m_filepath2.Replace("\\", "/");
#endif
        if (!m_filepath2.empty())
        {
            m_filepath2 = wxURI(m_filepath2).BuildURI();
        }
        generate_html();

        wxString localUrl = "file://" + m_filepath2;
        m_webview = wxWebView::New(this, wxID_ANY, localUrl);
        m_webview->SetClientSize(FromDIP(m_width), FromDIP(m_height));
        SetClientSize(m_webview->GetSize());

        CentreOnParent();

        m_webview->Bind(wxEVT_WEBVIEW_ERROR, [](wxWebViewEvent& event) {
            wxString errorMsg;
            switch (event.GetInt()) {
            case wxWEBVIEW_NAV_ERR_CONNECTION:  errorMsg = "Connection error"; break;
            case wxWEBVIEW_NAV_ERR_CERTIFICATE: errorMsg = "Certificate error"; break;
            case wxWEBVIEW_NAV_ERR_NOT_FOUND:   errorMsg = "Page not found"; break;
            default: errorMsg = "Unknown error"; break;
            }
            BOOST_LOG_TRIVIAL(error) << "Unable to load html: " << event.GetURL().ToStdString();
        });

    }

    TimeLapseVideoPlayDlg::~TimeLapseVideoPlayDlg()
    {

    }

    bool TimeLapseVideoPlayDlg::generate_html()
    {
        char buffer[4096];
        snprintf(buffer, sizeof(buffer), htmlTemplate, m_video_url.ToUTF8().data());

        wxString content(buffer, strlen(buffer));


        wxString dirPath = wxFileName(m_filepath).GetPath();

        if (!wxDir::Exists(dirPath)) {
            if (!wxDir::Make(dirPath, 0777, wxPATH_MKDIR_FULL)) {
                BOOST_LOG_TRIVIAL(error) << "Unable to create time_lapse_video dir: " << dirPath.ToStdString();
                return false;
            }
        }

        wxFile outFile(m_filepath, wxFile::write);
        if (outFile.IsOpened())
        {
            outFile.Write(content);
            outFile.Close();
        }
        else
        {
            BOOST_LOG_TRIVIAL(error) << "Unable to open the file: " << m_filepath.ToStdString();
            return false;
        }
        return true;
    }

}}
