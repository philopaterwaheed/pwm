
#include "main.h"
#include "config.h"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <iostream>

Display *display; // the connection to the X server
Window root; // the root window top level window all other windows are children
             // of it and covers all the screen
Window focused_window = None;

std::vector<Client> clients; // the clients vector

// Find a client by its window id and return a pointer to it
Client *find_client(Window w) {
  for (auto &client : clients) {
    if (client.window == w) {
      return &client;
    }
  }
  return nullptr;
}

void handle_focus_in(XEvent *e) {
  XFocusChangeEvent *ev = &e->xfocus;
  focused_window = ev->window; // Set the focused window
}

void kill_focused_window(const Arg *arg) {
  (void)arg;
  if (focused_window != None) {
    XKillClient(display, focused_window);
    focused_window = None; // Reset focused window
    clients.erase(std::remove_if(clients.begin(), clients.end(),
                                 [](const Client &c) {
                                   return c.window == focused_window;
                                 }),
                  clients.end());
  }
}

// handle the mouse enter event
void handle_enter_notify(XEvent *e) {
  XEnterWindowEvent *ev = &e->xcrossing;
  focused_window = ev->window; // Set the focused window on mouse enter
  XSetInputFocus(display, focused_window, RevertToParent, CurrentTime);
}
void handle_map_request(XEvent *e) {
  XMapRequestEvent *ev = &e->xmaprequest;
  XWindowAttributes wa;
  XGetWindowAttributes(display, ev->window, &wa);

  if (wa.override_redirect)
    return;

  XSelectInput(display, ev->window, StructureNotifyMask | EnterWindowMask);
  XMapWindow(display, ev->window);

  clients.push_back({ev->window, wa.x, wa.y,
                     static_cast<unsigned int>(wa.width),
                     static_cast<unsigned int>(wa.height)});
}

void handle_configure_request(XEvent *e) {
  // this event is sent when a window wants to change its size/position
  XConfigureRequestEvent *ev = &e->xconfigurerequest;

  XWindowChanges changes;
  changes.x = ev->x;
  changes.y = ev->y;
  changes.width = ev->width;
  changes.height = ev->height;
  changes.border_width = ev->border_width;
  changes.sibling = ev->above;
  changes.stack_mode = ev->detail;

  XConfigureWindow(display, ev->window, ev->value_mask, &changes);
}

void resize_focused_window_y(const Arg *arg) {
  if (focused_window != None && focused_window != root) {
    XWindowChanges changes;

    // Get the current window attributes to calculate the new size
    XWindowAttributes wa;
    XGetWindowAttributes(display, focused_window, &wa);

    // Adjust width and height based on delta
    changes.height = wa.height + arg->i;
    changes.width = 0;

    // Prevent the window from being resized too small
    if (changes.height < 100)
      changes.height = 100;

    // Apply the new size to the window
    XConfigureWindow(display, focused_window, CWHeight, &changes);
  }
}
void resize_focused_window_x(const Arg *arg) {
  if (focused_window != None && focused_window != root) {
    XWindowChanges changes;

    // Get the current window attributes to calculate the new size
    XWindowAttributes wa;
    XGetWindowAttributes(display, focused_window, &wa);

    // Adjust width and height based on delta
    changes.width = wa.width + arg->i;
    changes.height = 0;

    // Prevent the window from being resized too small
    if (changes.width < 100)
      changes.width = 100;

    // Apply the new size to the window
    XConfigureWindow(display, focused_window, CWWidth, &changes);
  }
}
void lunch(const Arg *arg) {
  auto args = (char **)arg->v;
  if (fork() == 0) {
    if (display)
      close(ConnectionNumber(
          display)); // if the display is open, close it
                     // The close(ConnectionNumber(dpy)) call closes the file
                     // descriptor associated with the display connection in the
                     // child process. This is necessary because the child
                     // process doesn't need to maintain the X connection—it
                     // will execute a new program.
    setsid();
    //    function call creates a new session and sets the child process as the
    //    session leader. This detaches the child process from the terminal (if
    //    any) and makes it independent of the parent process group.
    execvp(args[0], args);
    // The execvp() function replaces the current process image with a new
    // process
    fprintf(stderr, "Failed to launch st\n");
    exit(1);
  }
}

void handle_key_press(XEvent *e) {
  XKeyEvent *ev = &e->xkey;
  for (auto shortcut : shortcuts) {
    // the state is a bit mask and is true only when the key is mod
    if (ev->keycode == XKeysymToKeycode(display, shortcut.key) &&
        (ev->state & shortcut.mask) && shortcut.func)
      shortcut.func(&(shortcut.arg));
  }
}

void run() {
  XEvent ev;
  while (!XNextEvent(display, &ev)) {
    switch (ev.type) {
    case MapRequest:
      // This event is sent when a window want to be mapped
      handle_map_request(&ev);
      break;
    case ConfigureRequest:
      // this event is sent when a window wants to change its size/position
      handle_configure_request(&ev);
      break;
    case FocusIn:
      handle_focus_in(&ev);
      break;
    case EnterNotify:
      handle_enter_notify(&ev);
      break;
    case KeyPress:
      handle_key_press(&ev);
      break;
    }
  }
}

void grab_keys() {
  // it only lets the window manager to listen to the key presses we specify
  for (auto shortcut : shortcuts) {
    XGrabKey(display, XKeysymToKeycode(display, shortcut.key), MOD|SHIFT, root, True,
             GrabModeAsync, GrabModeAsync);
  }
}
int main() {
  display = XOpenDisplay(nullptr);
  if (!display) {
    fprintf(stderr, "Cannot open display\n");
    exit(1);
  }

  root = DefaultRootWindow(display);

  // Subscribe to events on the root window
  XSelectInput(display, root,
               SubstructureRedirectMask | SubstructureNotifyMask |
                   KeyPressMask | ExposureMask);

  Cursor cursor =                    // not for future evry action needs a shape
      XCreateFontCursor(display, 2); // Set the cursor for the window
  XDefineCursor(display, root, cursor);
  // Capture key presses for the mod key (e.g., Mod4Mask) with any key

  grab_keys();
  run();

  XCloseDisplay(display);
  return 0;
}
void exit_pwm(const Arg *arg) {
  free(display);
  exit(0);
}
