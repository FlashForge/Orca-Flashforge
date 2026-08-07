#ifndef slic3r_LinuxXErrorHandler_hpp_
#define slic3r_LinuxXErrorHandler_hpp_

namespace Slic3r { namespace GUI {

// Installs a non-fatal X11 error handler that swallows GLX errors.
//
// On Linux the app runs on the X11/GLX backend (wxWidgets is built without
// wxUSE_GLCANVAS_EGL, so the GL canvas forces the X11 backend even on Wayland
// sessions). Showing/hiding the 3D and assembly-view GL canvases can raise a
// transient GLX BadMatch on glXSwapBuffers: the offending frame is simply
// dropped and the next render recovers. GDK's default X error handler instead
// treats every X error as fatal and aborts the process. This handler logs and
// ignores GLX errors while delegating everything else to GDK's handler.
//
// No-op on non-Linux / non-X11 builds. Call once after GTK is initialized.
void install_linux_x_error_handler();

}} // namespace Slic3r::GUI

#endif // slic3r_LinuxXErrorHandler_hpp_
