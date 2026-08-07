#include "LinuxXErrorHandler.hpp"

#if defined(__linux__) && (defined(__WXGTK3__) || defined(__WXGTK20__))

#include <gtk/gtk.h>

#ifdef GDK_WINDOWING_X11

#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <boost/log/trivial.hpp>
#include <boost/format.hpp>

namespace Slic3r { namespace GUI {

namespace {

int (*s_prev_x_error_handler)(Display *, XErrorEvent *) = nullptr;
int s_glx_major_opcode = -1;

int ff_x_error_handler(Display *display, XErrorEvent *event)
{
    // GLX errors raised while a wxGLCanvas is being shown/hidden (e.g. BadMatch on
    // glXSwapBuffers when switching the 3D / assembly-view panels) are transient and
    // non-fatal. Swallow GLX errors; keep GDK's behaviour for everything else.
    if (s_glx_major_opcode >= 0 && event->request_code == s_glx_major_opcode) {
        char buf[256] = {0};
        XGetErrorText(display, event->error_code, buf, sizeof(buf) - 1);
        BOOST_LOG_TRIVIAL(warning) << boost::format(
            "Ignored GLX X error: %1% (error_code=%2%, request_code=%3%, minor_code=%4%)")
            % buf % int(event->error_code) % int(event->request_code) % int(event->minor_code);
        return 0;
    }
    if (s_prev_x_error_handler != nullptr)
        return s_prev_x_error_handler(display, event);
    return 0;
}

} // namespace

void install_linux_x_error_handler()
{
    GdkDisplay *gdk_display = gdk_display_get_default();
    if (gdk_display == nullptr || !GDK_IS_X11_DISPLAY(gdk_display))
        return;

    Display *xdisplay = GDK_DISPLAY_XDISPLAY(gdk_display);
    int first_event = 0, first_error = 0;
    if (!XQueryExtension(xdisplay, "GLX", &s_glx_major_opcode, &first_event, &first_error))
        s_glx_major_opcode = -1;

    s_prev_x_error_handler = XSetErrorHandler(ff_x_error_handler);
}

}} // namespace Slic3r::GUI

#else // !GDK_WINDOWING_X11

namespace Slic3r { namespace GUI {
void install_linux_x_error_handler() {}
}} // namespace Slic3r::GUI

#endif // GDK_WINDOWING_X11

#else // !linux/gtk

namespace Slic3r { namespace GUI {
void install_linux_x_error_handler() {}
}} // namespace Slic3r::GUI

#endif
