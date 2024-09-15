// for event handler funtions
#include "config.h"
#include "main.h"
#include <X11/Xlib.h>
#include <cstdlib>

extern Display *display; // the connection to the X server
extern Window root; // the root window top level window all other windows are
                    // children of it and covers all the screen
extern Window focused_window;
extern Monitor *current_monitor;
extern std::vector<Monitor> monitors; // List of monitors
extern std::vector<Workspace> *workspaces;
extern Workspace *current_workspace;
extern std::vector<Client> *clients;
extern std::string status;
extern Atom wmDeleteMessage;
void handle_button_press_event(XEvent *e) {
  int x = e->xbutton.x;
  int y = e->xbutton.y;

  // Determine which button was clicked
  int button_index = x / BUTTONS_WIDTH;
  if (button_index >= 0 && y <= BAR_HEIGHT * (current_workspace->show_bar) &&
      button_index < NUM_WORKSPACES) {
    switch_workspace(new (Arg){.i = button_index + 1});
    update_bar();
  } else if (button_index >= 0 &&
             y <= BAR_HEIGHT * (current_workspace->show_bar) &&
             button_index == NUM_WORKSPACES + 1) {
    toggle_layout();
  }
}
void handle_focus_in(XEvent *e) {
  XFocusChangeEvent *ev = &e->xfocus;
  /* if (focused_window != None) */
    /* XSetWindowBorder(display, focused_window, FOCUSED_BORDER_COLOR); */
}

void handle_focus_out(XEvent *e) {
  XFocusChangeEvent *ev = &e->xfocus;
  if (focused_window ==
      None) // that happens when we are killing leaading to program crash
    return;
  /* else */
    /* XSetWindowBorder(display, ev->window, BORDER_COLOR); */
}

// handle the mouse enter event
void handle_enter_notify(XEvent *e) {
  XEnterWindowEvent *ev = &e->xcrossing;
  if (ev->mode == NotifyNormal && ev->detail != NotifyInferior) {
    XSetInputFocus(display, ev->window, RevertToPointerRoot, CurrentTime);
    focused_window = ev->window; // Set the focused window on mouse enter
  }
}
void handle_map_request(XEvent *e) {
  XMapRequestEvent *ev = &e->xmaprequest;
  XWindowAttributes wa;
  Atom wm_delete_window;
  XGetWindowAttributes(display, ev->window, &wa);

  if (wa.override_redirect)
    return;

  XSelectInput(display, ev->window, EnterWindowMask | FocusChangeMask);
  XMapWindow(display, ev->window);
  // Set border width
  /* XSetWindowBorderWidth(display, ev->window, BORDER_WIDTH); */
  XSetWMProtocols(display, ev->window, &wm_delete_window, 1);
  // Set initial border color (unfocused)
  /* XSetWindowBorder(display, ev->window, BORDER_COLOR); */
  Client client = {ev->window, wa.x, wa.y, static_cast<unsigned int>(wa.width),
                   static_cast<unsigned int>(wa.height)};
  clients->push_back(client);
  current_workspace->master = clients->back().window;
  arrange_windows();
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
void handle_key_press(XEvent *e) {
  XKeyEvent *ev = &e->xkey;
  for (auto shortcut : shortcuts) {
    // the state is a bit mask and is true only when the key is mod
    if (ev->keycode == XKeysymToKeycode(display, shortcut.key) &&
        (CLEANMASK(ev->state) == CLEANMASK(shortcut.mask)) && shortcut.func)
      shortcut.func(&(shortcut.arg));
  }
  restack_windows();
}
void handle_motion_notify(XEvent *e) {

  Monitor *m;
  XMotionEvent *ev = &e->xmotion;

  if (ev->window != root)
    return;
  int mouse_x = ev->x_root;
  int mouse_y = ev->y_root;

  // Find the monitor under the mouse
  Monitor *monitor = find_monitor_by_coordinates(mouse_x, mouse_y);

  // If the monitor has changed, update the focus
  if (monitor && monitor != current_monitor) {
    XClearWindow(display, current_monitor->bar);
    focus_monitor(monitor);
  }
}

void handle_destroy_notify(XEvent *e) {
  // Access the XDestroyWindowEvent
  XDestroyWindowEvent *ev = &e->xdestroywindow;

  // Get the window ID that was destroyed
  Window window = ev->window;
  // Update master window if needed

  // Erase the client corresponding to the destroyed window
  // Ensure 'clients' is a valid pointer or directly use it if it's a member
  auto it =
      std::remove_if(clients->begin(), clients->end(),
                     [window](const Client &c) { return c.window == window; });

  // Only erase if an element was found and removed

  if (it != clients->end()) {
    clients->erase(it, clients->end());
  }
  if (window == current_workspace->master) {
    current_workspace->master = None;
    if (!clients->empty()) {
      current_workspace->master = (*clients)[0].window;
    }
  }
  std::string log = "echo 'killed window";
  log += std::to_string(ev->window);
  log += "' >> /tmp/log.txt";
  system(log.c_str());
  focused_window = None;
  arrange_windows();
}
void handle_client_message(XEvent *ev) {
  if (ev->xclient.data.l[0] == wmDeleteMessage) {
    Window window = ev->xclient.window;
    if (window != None) {
      // Remove the focused window from the client list
      clients->erase(std::remove_if(clients->begin(), clients->end(),
                                    [window](const Client &c) {
                                      return c.window == window;
                                    }),
                     clients->end());

      XUnmapWindow(display, window);
      /* XSetWindowBorderWidth(display, window, 0); */
      // Update master window if needed
      if (focused_window == current_workspace->master) {
        current_workspace->master = None;
        if (!clients->empty()) {
          current_workspace->master = (*clients)[0].window;
        }
      }

      focused_window = None;
      arrange_windows();
    }
  }
}
