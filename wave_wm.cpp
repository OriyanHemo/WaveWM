#include "wave_wm.hpp"
extern "C" {
#include <X11/Xutil.h>
}
#include <cstring>
#include <algorithm>
#include <glog/logging.h>
#include "util.hpp"

using ::std::max;
using ::std::mutex;
using ::std::string;
using ::std::unique_ptr;

bool WindowManager::wm_detected_;
mutex WindowManager::wm_detected_mutex_;

unique_ptr<WindowManager> WindowManager::Create(const string& display_str) {
  const char* display_c_str =
        display_str.empty() ? nullptr : display_str.c_str();
  Display* display = XOpenDisplay(display_c_str);
  if (display == nullptr) {
    LOG(ERROR) << "Failed to open X display " << XDisplayName(display_c_str);
    return nullptr;
  }
  return unique_ptr<WindowManager>(new WindowManager(display));
}

WindowManager::WindowManager(Display* display)
    : display_(CHECK_NOTNULL(display)),
      root_(DefaultRootWindow(display_)),
      WM_PROTOCOLS(XInternAtom(display_, "WM_PROTOCOLS", false)),
      WM_DELETE_WINDOW(XInternAtom(display_, "WM_DELETE_WINDOW", false)) {
    //SetBackgroundImage(display, root_, "./Untitled.png");

}
// void SetBackgroundImage(Display* display, Window root, const char* image_path) {
//     // Initialize Imlib2
//     Imlib_Image image = imlib_load_image(image_path);
//     if (!image) {
//         fprintf(stderr, "Failed to load image: %s\n", image_path);
//         return;
//     }
    
//     // Set the image for drawing
//     imlib_context_set_image(image);
    
//     // Get the root window's dimensions
//     int screen = DefaultScreen(display);
//     int screen_width = DisplayWidth(display, screen);
//     int screen_height = DisplayHeight(display, screen);

//     // Scale the image to the size of the root window (optional)
//     Imlib_Image scaled_image = imlib_create_cropped_scaled_image(
//         0, 0,
//         imlib_image_get_width(), imlib_image_get_height(),
//         screen_width, screen_height);
//     imlib_free_image();  // Free original image

//     if (!scaled_image) {
//         fprintf(stderr, "Failed to scale image\n");
//         return;
//     }

//     // Set the scaled image in the context and create a pixmap
//     imlib_context_set_image(scaled_image);
//     Pixmap pixmap = XCreatePixmap(display, root, screen_width, screen_height,
//                                   DefaultDepth(display, screen));
//     imlib_context_set_drawable(pixmap);

//     // Render the image to the pixmap
//     imlib_render_image_on_drawable(0, 0);
//     imlib_free_image();  // Free the scaled image

//     // Set the pixmap as the root window background
//     XSetWindowBackgroundPixmap(display, root, pixmap);
//     XClearWindow(display, root);
//     XFlush(display);

//     // Free the pixmap
//     XFreePixmap(display, pixmap);
// }
WindowManager::~WindowManager() {
  XCloseDisplay(display_);
}

void WindowManager::Run() {
  {
    ::std::lock_guard<mutex> lock(wm_detected_mutex_);

    wm_detected_ = false;
    XSetErrorHandler(&WindowManager::OnWMDetected);
    XSelectInput(
        display_,
        root_,
        SubstructureRedirectMask | SubstructureNotifyMask);
    XSync(display_, false);
    if (wm_detected_) {
      LOG(ERROR) << "Detected another window manager on display "
                 << XDisplayString(display_);
      return;
    }
  }
  XSetErrorHandler(&WindowManager::OnXError);
  XGrabServer(display_);
  Window returned_root, returned_parent;
  Window* top_level_windows;
  unsigned int num_top_level_windows;
  CHECK(XQueryTree(
      display_,
      root_,
      &returned_root,
      &returned_parent,
      &top_level_windows,
      &num_top_level_windows));
  CHECK_EQ(returned_root, root_);
  for (unsigned int i = 0; i < num_top_level_windows; ++i) {
    Frame(top_level_windows[i], true);
  }
  XFree(top_level_windows);
  XUngrabServer(display_);

  for (;;) {
    XEvent e;
    XNextEvent(display_, &e);
    LOG(INFO) << "Received event: " << ToString(e);

    switch (e.type) {
      case CreateNotify:
        OnCreateNotify(e.xcreatewindow);
        break;
      case DestroyNotify:
        OnDestroyNotify(e.xdestroywindow);
        break;
      case ReparentNotify:
        OnReparentNotify(e.xreparent);
        break;
      case MapNotify:
        OnMapNotify(e.xmap);
        break;
      case UnmapNotify:
        OnUnmapNotify(e.xunmap);
        break;
      case ConfigureNotify:
        OnConfigureNotify(e.xconfigure);
        break;
      case MapRequest:
        OnMapRequest(e.xmaprequest);
        break;
      case ConfigureRequest:
        OnConfigureRequest(e.xconfigurerequest);
        break;
      case ButtonPress:
        OnButtonPress(e.xbutton);
        break;
      case ButtonRelease:
        OnButtonRelease(e.xbutton);
        break;
      case MotionNotify:
        while (XCheckTypedWindowEvent(
            display_, e.xmotion.window, MotionNotify, &e)) {}
        OnMotionNotify(e.xmotion);
        break;
      case KeyPress:
        OnKeyPress(e.xkey);
        break;
      case KeyRelease:
        OnKeyRelease(e.xkey);
        break;
      default:
        LOG(WARNING) << "Ignored event";
    }
  }
}

void WindowManager::Frame(Window w, bool was_created_before_window_manager) {
  const unsigned int BORDER_WIDTH = 2;
  const unsigned long BORDER_COLOR = 0x1d2021;
  const unsigned long BG_COLOR = 0x2c3f53;

  CHECK(!clients_.count(w));

  XWindowAttributes x_window_attrs;
  CHECK(XGetWindowAttributes(display_, w, &x_window_attrs));

  if (was_created_before_window_manager) {
    if (x_window_attrs.override_redirect ||
        x_window_attrs.map_state != IsViewable) {
      return;
    }
  }

  const Window frame = XCreateSimpleWindow(
      display_,
      root_,
      x_window_attrs.x,
      x_window_attrs.y,
      x_window_attrs.width,
      x_window_attrs.height,
      BORDER_WIDTH,
      BORDER_COLOR,
      BG_COLOR);
  XSelectInput(
      display_,
      frame,
      SubstructureRedirectMask | SubstructureNotifyMask);
  XAddToSaveSet(display_, w);
  XReparentWindow(
      display_,
      w,
      frame,
      0, 0);  // Offset of client window within frame.
  XMapWindow(display_, frame);
  clients_[w] = frame;
  XGrabButton(
      display_,
      Button1,
      Mod1Mask | Mod2Mask,
      w,
      false,
      ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
      GrabModeAsync,
      GrabModeAsync,
      None,
      None);
  XGrabButton(
        display_,
        Button2,
        Mod1Mask | Mod2Mask,
        w,
        false,
        ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
        GrabModeAsync,
        GrabModeAsync,
        None,
        None);
  XGrabButton(
      display_,
      Button3,
      Mod1Mask | Mod2Mask,
      w,
      false,
      ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
      GrabModeAsync,
      GrabModeAsync,
      None,
      None);
  XGrabKey(
      display_,
      XKeysymToKeycode(display_, XK_Control_L),
      Mod1Mask | Mod2Mask,
      w,
      false,
      GrabModeAsync,
      GrabModeAsync);
  XGrabKey(
      display_,
      XKeysymToKeycode(display_, XK_Shift_L),
      Mod1Mask | Mod2Mask,
      w,
      false,
      GrabModeAsync,
      GrabModeAsync);


  LOG(INFO) << "Framed window " << w << " [" << frame << "]";
}

void WindowManager::Unframe(Window w) {
  CHECK(clients_.count(w));

  // We reverse the steps taken in Frame().
  const Window frame = clients_[w];
  XUnmapWindow(display_, frame);
  XReparentWindow(
      display_,
      w,
      root_,
      0, 0);  // Offset of client window within root.
  XRemoveFromSaveSet(display_, w);
  XDestroyWindow(display_, frame);
  clients_.erase(w);

  LOG(INFO) << "Unframed window " << w << " [" << frame << "]";
}

void WindowManager::OnCreateNotify(const XCreateWindowEvent& e) {}

void WindowManager::OnDestroyNotify(const XDestroyWindowEvent& e) {}

void WindowManager::OnReparentNotify(const XReparentEvent& e) {}

void WindowManager::OnMapNotify(const XMapEvent& e) {}

void WindowManager::OnUnmapNotify(const XUnmapEvent& e) {
  if (!clients_.count(e.window)) {
    LOG(INFO) << "Ignore UnmapNotify for non-client window " << e.window;
    return;
  }

  if (e.event == root_) {
    LOG(INFO) << "Ignore UnmapNotify for reparented pre-existing window "
              << e.window;
    return;
  }

  Unframe(e.window);
}

void WindowManager::OnConfigureNotify(const XConfigureEvent& e) {}

void WindowManager::OnMapRequest(const XMapRequestEvent& e) {
  // 1. Frame or re-frame window.
  Frame(e.window, false);
  // 2. Actually map window.
  XMapWindow(display_, e.window);
}

void WindowManager::OnConfigureRequest(const XConfigureRequestEvent& e) {
  XWindowChanges changes;
  changes.x = e.x;
  changes.y = e.y;
  changes.width = e.width;
  changes.height = e.height;
  changes.border_width = e.border_width;
  changes.sibling = e.above;
  changes.stack_mode = e.detail;
  if (clients_.count(e.window)) {
    const Window frame = clients_[e.window];
    XConfigureWindow(display_, frame, e.value_mask, &changes);
    LOG(INFO) << "Resize [" << frame << "] to " << Size<int>(e.width, e.height);
  }
  XConfigureWindow(display_, e.window, e.value_mask, &changes);
  LOG(INFO) << "Resize " << e.window << " to " << Size<int>(e.width, e.height);
}

void WindowManager::OnButtonPress(const XButtonEvent& e) {
  CHECK(clients_.count(e.window));
  const Window frame = clients_[e.window];
  drag_start_pos_ = Position<int>(e.x_root, e.y_root);

  Window returned_root;
  int x, y;
  unsigned width, height, border_width, depth;
  CHECK(XGetGeometry(
      display_,
      frame,
      &returned_root,
      &x, &y,
      &width, &height,
      &border_width,
      &depth));
  drag_start_frame_pos_ = Position<int>(x, y);
  drag_start_frame_size_ = Size<int>(width, height);

  XRaiseWindow(display_, frame);
}

void WindowManager::OnButtonRelease(const XButtonEvent& e) {
    // alt + right button: Resize window.
    // Window dimensions cannot be negative.
    //        -screen 800x600 
    //change location to 0, 0
  CHECK(clients_.count(e.window));
  const Window frame = clients_[e.window];
  if (e.state & Button2Mask) {
    const Position<int> drag_pos(e.x_root, e.y_root);

    if(drag_start_frame_pos_.x == 0 && drag_start_frame_pos_.y == 0 && drag_start_frame_size_.width == 800 &&  drag_start_frame_size_.height ==600){
      const Position<int> dest_frame_pos = {200, 100};
      XMoveWindow(
          display_,
          frame,
          dest_frame_pos.x, dest_frame_pos.y);
      //change size to 800, 600

      const Size<int> dest_frame_size = {400, 200};
      // 1. Resize frame.
      XResizeWindow(
          display_,
          frame,
          dest_frame_size.width, dest_frame_size.height);
      // 2. Resize client window.
      XResizeWindow(
          display_,
          e.window,
          dest_frame_size.width, dest_frame_size.height);
    }else{
      const Position<int> dest_frame_pos = {0, 0};
      XMoveWindow(
          display_,
          frame,
          dest_frame_pos.x, dest_frame_pos.y);
      //change size to 800, 600

      const Size<int> dest_frame_size = {800, 600};
      XResizeWindow(
          display_,
          frame,
          dest_frame_size.width, dest_frame_size.height);
      XResizeWindow(
          display_,
          e.window,
          dest_frame_size.width, dest_frame_size.height);
    }
  } else if(e.state & Button1Mask){
    if(e.x_root <= 0){
      const Position<int> dest_frame_pos = {0, 0};
      XMoveWindow(
          display_,
          frame,
          dest_frame_pos.x, dest_frame_pos.y);

      const Size<int> dest_frame_size = {400, 600};
      XResizeWindow(
          display_,
          frame,
          dest_frame_size.width, dest_frame_size.height);
      XResizeWindow(
          display_,
          e.window,
          dest_frame_size.width, dest_frame_size.height);
    }else if(e.x_root >= 799){
      const Position<int> dest_frame_pos = {400, 0};
      XMoveWindow(
          display_,
          frame,
          dest_frame_pos.x, dest_frame_pos.y);

      const Size<int> dest_frame_size = {400, 600};
      XResizeWindow(
          display_,
          frame,
          dest_frame_size.width, dest_frame_size.height);
      XResizeWindow(
          display_,
          e.window,
          dest_frame_size.width, dest_frame_size.height);
    }else if(e.y_root <= 0){
      const Position<int> dest_frame_pos = {0, 0};
      XMoveWindow(
          display_,
          frame,
          dest_frame_pos.x, dest_frame_pos.y);
      //change size to 800, 600

      const Size<int> dest_frame_size = {800, 300};
      // 1. Resize frame.
      XResizeWindow(
          display_,
          frame,
          dest_frame_size.width, dest_frame_size.height);
      // 2. Resize client window.
      XResizeWindow(
          display_,
          e.window,
          dest_frame_size.width, dest_frame_size.height);
    }else if(e.y_root >= 599){
      const Position<int> dest_frame_pos = {0, 300};
      XMoveWindow(
          display_,
          frame,
          dest_frame_pos.x, dest_frame_pos.y);

      const Size<int> dest_frame_size = {800, 300};
      XResizeWindow(
          display_,
          frame,
          dest_frame_size.width, dest_frame_size.height);
      XResizeWindow(
          display_,
          e.window,
          dest_frame_size.width, dest_frame_size.height);
    }
  }
}

void WindowManager::OnMotionNotify(const XMotionEvent& e) {
  CHECK(clients_.count(e.window));
  const Window frame = clients_[e.window];
  const Position<int> drag_pos(e.x_root, e.y_root);
  const Vector2D<int> delta = drag_pos - drag_start_pos_;

  if (e.state & Button1Mask ) {
    // alt + left button: Move window.
    const Position<int> dest_frame_pos = drag_start_frame_pos_ + delta;
    XMoveWindow(
        display_,
        frame,
        dest_frame_pos.x, dest_frame_pos.y);
  } else if (e.state & Button3Mask) {
    const Vector2D<int> size_delta(
        max(delta.x, -drag_start_frame_size_.width),
        max(delta.y, -drag_start_frame_size_.height));
    const Size<int> dest_frame_size = drag_start_frame_size_ + size_delta;
    XResizeWindow(
        display_,
        frame,
        dest_frame_size.width, dest_frame_size.height);
    XResizeWindow(
        display_,
        e.window,
        dest_frame_size.width, dest_frame_size.height);
  }
}

void WindowManager::OnKeyPress(const XKeyEvent& e) {
  if ((e.state & Mod1Mask) &&
      (e.keycode == XKeysymToKeycode(display_, XK_Control_L))) {
    Atom* supported_protocols;
    int num_supported_protocols;
    if (XGetWMProtocols(display_,
                        e.window,
                        &supported_protocols,
                        &num_supported_protocols) &&
        (::std::find(supported_protocols,
                     supported_protocols + num_supported_protocols,
                     WM_DELETE_WINDOW) !=
         supported_protocols + num_supported_protocols)) {
      LOG(INFO) << "Gracefully deleting window " << e.window;
      XEvent msg;
      memset(&msg, 0, sizeof(msg));
      msg.xclient.type = ClientMessage;
      msg.xclient.message_type = WM_PROTOCOLS;
      msg.xclient.window = e.window;
      msg.xclient.format = 32;
      msg.xclient.data.l[0] = WM_DELETE_WINDOW;
      CHECK(XSendEvent(display_, e.window, false, 0, &msg));
    } else {
      LOG(INFO) << "Killing window " << e.window;
      XKillClient(display_, e.window);
    }
  } else if ((e.state & Mod1Mask) &&
             (e.keycode == XKeysymToKeycode(display_, XK_Shift_L))) {
    auto i = clients_.find(e.window);
    CHECK(i != clients_.end());
    ++i;
    if (i == clients_.end()) {
      i = clients_.begin();
    }
    XRaiseWindow(display_, i->second);
    XSetInputFocus(display_, i->first, RevertToPointerRoot, CurrentTime);
  } 
}

void WindowManager::OnKeyRelease(const XKeyEvent& e) {}

int WindowManager::OnXError(Display* display, XErrorEvent* e) {
  const int MAX_ERROR_TEXT_LENGTH = 1024;
  char error_text[MAX_ERROR_TEXT_LENGTH];
  XGetErrorText(display, e->error_code, error_text, sizeof(error_text));
  LOG(ERROR) << "Received X error:\n"
             << "    Request: " << int(e->request_code)
             << " - " << XRequestCodeToString(e->request_code) << "\n"
             << "    Error code: " << int(e->error_code)
             << " - " << error_text << "\n"
             << "    Resource ID: " << e->resourceid;
  return 0;
}

int WindowManager::OnWMDetected(Display* display, XErrorEvent* e) {
  CHECK_EQ(static_cast<int>(e->error_code), BadAccess);
  wm_detected_ = true;
  return 0;
}
