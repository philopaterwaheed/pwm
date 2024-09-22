// for event handler funtions
#include "config.h"
#include "main.h"
#include <X11/Xlib.h>
#include <string>

extern Display *display; // the connection to the X server
extern Window root; // the root window top level window all other windows are
                    // children of it and covers all the screen
extern Window focused_window;
extern Monitor *current_monitor;
extern std::vector<Monitor> monitors; // List of monitors
extern std::vector<Workspace> *workspaces;
extern Workspace *current_workspace;
extern std::vector<Client> *clients;
extern std::vector<Client> *sticky;
extern std::string status;
extern Atom state_atom; // i don't know why I need those but they don't work in
                        // the array
extern Atom fullscreen_atom;
void handle_button_press_event(XEvent *e) {
  XButtonPressedEvent *ev = &e->xbutton;
  for (auto button : buttons) {
    if (e->xbutton.button == button.id &&
        CLEANMASK(ev->state) == CLEANMASK(button.mask) && button.func) {
      button.func(&(button.arg));
      return;
    }
  }
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
  focused_window = ev->window;
  /* if (focused_window != None) */
  XSetWindowBorder(display, focused_window, FOCUSED_BORDER_COLOR);
}

void handle_focus_out(XEvent *e) {
  XFocusChangeEvent *ev = &e->xfocus;
  if (focused_window ==
      None) // that happens when we are killing leaading to program crash
    return;
  /* else */
  XSetWindowBorder(display, ev->window, BORDER_COLOR);
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
  XGetWindowAttributes(display, ev->window, &wa);

  focused_window = ev->window;
  if (wa.override_redirect)
    return;

  XSetWindowBorder(display, ev->window, BORDER_COLOR);
  Client client = {ev->window,
                   wa.x,
                   wa.y,
                   static_cast<unsigned int>(wa.width),
                   static_cast<unsigned int>(wa.height),
                   .floating = wants_floating(ev->window)};
  clients->push_back(client);
  if (!client.floating)
    current_workspace->master = clients->back().window;
  focused_window = clients->back().window;
  arrange_windows();
  XSelectInput(display, ev->window,
               EnterWindowMask | FocusChangeMask | PropertyChangeMask);
  XMapWindow(display, ev->window);
  // Set border width
  XSetWindowBorderWidth(display, ev->window, BORDER_WIDTH);
  // Set initial border color (unfocused)
  // we map afterarrange for  redusing visual glitchs
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
  set_size_hints(ev->window);
}
void handle_key_press(XEvent *e) {
  XKeyEvent *ev = &e->xkey;
  for (auto shortcut : shortcuts) {
    // the state is a bit mask and is true only when the key is mod
    if (ev->keycode == XKeysymToKeycode(display, shortcut.key) &&
        (CLEANMASK(ev->state) == CLEANMASK(shortcut.mask)) && shortcut.func)
      shortcut.func(&(shortcut.arg));
  }
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
  clients->erase(
      std::remove_if(clients->begin(), clients->end(),
                     [&window](const Client &c) { return c.window == window; }),
      clients->end());
  sticky->erase(
      std::remove_if(sticky->begin(), sticky->end(),
                     [&window](const Client &c) { return c.window == window; }),
      sticky->end());
  if (window == current_workspace->master) {
    current_workspace->master = None;
  }
  XUnmapWindow(display, window);

  focused_window = None;
  arrange_windows();
}
void handle_property_notify(XEvent *e) {

  XPropertyEvent *ev = &e->xproperty;
  if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
    update_status(e);
    update_bar();
  }
  if (ev->atom == netatom[NetWMWindowType]) {
    Client *c = find_client(ev->window);
    updatewindowtype(c);
  }
}
void handle_client_message(XEvent *e) {
  XClientMessageEvent *cm = &e->xclient;
  if (cm->message_type == state_atom) {
    Client *c = find_client(cm->window);
    if (c) { // Ensure the client was found
      if (cm->data.l[1] == fullscreen_atom ||
          cm->data.l[2] == fullscreen_atom) {
        bool should_fullscreen =
            (cm->data.l[0] == 1) || (cm->data.l[0] == 2 && !c->fullscreen);
        set_fullscreen(c, should_fullscreen);
        focused_window = c->window;
      }
    }
  }
}
void nothing(XEvent *e) {
  // just a placehodler for events we don't handle
}
