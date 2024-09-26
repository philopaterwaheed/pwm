
#include "main.h"
#include "config.h"
#include <X11/X.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>


Atom delete_atom, protocol_atom, name_atom;

Atom netatom[NetLast];
Display *display; // the connection to the X server
int screen;
Window root; // the root window top level window all other windows are children
             // of it and covers all the screen
Window focused_window = None;
XftColor xft_color;
extern Cursor cursors[3];

Monitor *current_monitor = nullptr; // The monitor in focus
Workspace *current_workspace;
//
std::vector<Monitor> monitors; // List of monitors
std::vector<Workspace> *workspaces;
std::vector<Client> *clients;
std::vector<Client> *sticky;
std::set <Window> all_windows;

std::string status = "pwm by philo";
std::vector<XftFont *> fallbackFonts;

extern XftFont *xft_font;

int multi_monitor = false; // to detect if there are multiple monitors to
                           // cancle montion notify if there is not

// to store button widths and utf8 strings
int BUTTONS_WIDTHS[NUM_WORKSPACES + 1];
int BUTTONS_WIDTHS_PRESUM[NUM_WORKSPACES +
                          1]; // a presum array for button widths
XftChar8 *BUTTON_LABEL_UTF8[NUM_WORKSPACES];

int errorHandler(Display *display, XErrorEvent *errorEvent) {
  std::cerr << "Xlib Error: ";
  switch (errorEvent->error_code) {
  case BadWindow:
    std::cerr << "BadWindow error" << std::endl;
    break;
  case BadDrawable:
    std::cerr << "BadDrawable error" << std::endl;
    break;
  // Add cases for other error types as needed
  default:
    std::cerr << "Other error" << std::endl;
  }
  return 0; // Return 0 to indicate the error was handled
}
// Find a client by its window id and return a pointer to it
Client *find_client(Window w) {
  for (auto &m : monitors) {
    for (auto &client : m.workspaces[current_monitor->at]
                            .clients) { // i really don't why
                                        // i need this insted of
                                        // current_workspace but that what works
      if (client.window == w) {
        return &client;
      }
    }
    for (auto &client : m.sticky) {
      if (client.window == w) {
        return &client;
      }
    }
  }

  return nullptr;
}

void create_bars() {
  for (auto &monitor : monitors) {
    int screen_width = current_monitor->width;
    int screen_height = current_monitor->height;

    // Create the bar windows for each monitor
    monitor.bar = XCreateSimpleWindow(display, root, monitor.x, monitor.y,
                                      screen_width, BAR_HEIGHT, 0, 0, 0x000000);
    if (SHOW_BAR) { // for if show bar is false in the config
      XSelectInput(display, monitor.bar,
                   ExposureMask | KeyPressMask | ButtonPressMask);
      XMapWindow(display,
                 monitor.bar); // we create and don't show for future errors
    }
    // Load the font using Xft

    // Create a drawable for the bar

    // Set the color for drawing text (white in this case)
    XRenderColor render_color = {0xffff, 0xffff, 0xffff, 0xffff}; // White color
    XftColorAllocValue(display, DefaultVisual(display, DefaultScreen(display)),
                       DefaultColormap(display, DefaultScreen(display)),
                       &render_color, &xft_color);
    monitor.xft_draw = XftDrawCreate(
        display, monitor.bar, DefaultVisual(display, DefaultScreen(display)),
        DefaultColormap(display, DefaultScreen(display)));
  }
}
void update_bar() {
  if (!current_workspace->show_bar) {
    // if we change the workspace and it's hidden there if we don't unmap it
    // will still exist in the new work space to not show the bar
    XUnmapWindow(display, current_monitor->bar);
    return;
  }
  // if we were in a workspace that has the bar hidden and changed to a
  // workspace that does we need to remap
  XMapWindow(display, current_monitor->bar);
  int screen_width = current_monitor->width;

  XClearWindow(display, current_monitor->bar);

  // Draw workspace buttons
  for (int i = 0; i < NUM_WORKSPACES; ++i) {
    int x = BUTTONS_WIDTHS_PRESUM[i];
    // Highlight the current workspace button
    if (i == current_workspace->index) {
      XSetForeground(display, DefaultGC(display, DefaultScreen(display)),
                     0x3399FF); // Light blue background
      XFillRectangle(display, current_monitor->bar, DefaultGC(display, screen),
                     x, 0, BUTTONS_WIDTHS[i], current_workspace->bar_height);
    }
    draw_text_with_dynamic_font(display, current_monitor->bar,
                                current_monitor->xft_draw, &xft_color,
                                BUTTON_LABEL_UTF8[i], x + BUTTONS_WIDTHS[i] / 3,
                                BAR_Y, screen, current_monitor->width);
  }
  XftChar8 *status_utf8 =
      reinterpret_cast<XftChar8 *>(const_cast<char *>(status.c_str()));

  draw_text_with_dynamic_font(
      display, current_monitor->bar, current_monitor->xft_draw, &xft_color,
      status_utf8, screen_width - get_utf8_string_width(display, status_utf8),
      BAR_Y, screen, current_monitor->width);

  XftChar8 *layout_name_utf8 = reinterpret_cast<XftChar8 *>(
      const_cast<char *>(LAYOUTS[current_workspace->layout].name.c_str()));
  XSetForeground(display, DefaultGC(display, screen),
                 0x3399FF); // Light blue background
  BUTTONS_WIDTHS[NUM_WORKSPACES] =
      std::max(BUTTON_WIDTH, get_utf8_string_width(display, layout_name_utf8));
  XFillRectangle(display, current_monitor->bar, DefaultGC(display, screen),
                 BUTTONS_WIDTHS_PRESUM[NUM_WORKSPACES], 0,
                 BUTTONS_WIDTHS[NUM_WORKSPACES], current_workspace->bar_height);
  draw_text_with_dynamic_font(
      display, current_monitor->bar, current_monitor->xft_draw, &xft_color,
      layout_name_utf8, BUTTONS_WIDTHS_PRESUM[NUM_WORKSPACES], BAR_Y, screen,
      current_monitor->width); // to show the layout name of the workspaces
}

// Get the index of the focused window
int get_focused_window_index() {
  for (unsigned int i = 0; i < clients->size(); ++i) {
    if ((*clients)[i].window == focused_window) {
      return i;
    }
  }
  return -1; // Not found
}

void warp(const Client *c) {
  Window dummy;
  int x, y, di;
  unsigned int dui;

  if (!c) {
    XWarpPointer(display, None, root, 0, 0, 0, 0,
                 current_monitor->x + current_monitor->width / 2,
                 current_monitor->y + current_monitor->height / 2);
    return;
  }

  XQueryPointer(display, root, &dummy, &dummy, &x, &y, &di, &di, &dui);

  if ((x > c->x && y > c->y && x < c->x + c->window && y < c->y + c->height) ||
      (y > current_monitor->y + current_monitor->height &&
       y < current_monitor->y + current_monitor->height))
    return;

  XWarpPointer(display, None, c->window, 0, 0, 0, 0, c->width / 2,
               c->height / 2);
}

void run() {
  XEvent ev;
  while (!XNextEvent(display, &ev)) {
    if (handler[ev.type])
      handler[ev.type](&ev);
  }
}

void update_status(XEvent *ev) {

  if (ev->xproperty.atom == name_atom) {
    /* // Update status text from xsetroot */
    XTextProperty name;
    char **list = nullptr;
    int count = 0;

    if (XGetWMName(display, root, &name)) {

      if (XmbTextPropertyToTextList(display, &name, &list, &count) >= Success &&
          count > 0 && list != nullptr) {
        // Only take the first string (most window titles are single string)
        status = list[0];

        // Free the list after use
        XFreeStringList(list);
      } else {
        // If conversion failed or there was no text, return empty string
        status = "pwm by philo";
      }
    }
  }
}
void grab_keys() {
  // it only lets the window manager to listen to the key presses we specify
  for (auto shortcut : shortcuts) {
    XGrabKey(display, XKeysymToKeycode(display, shortcut.key), shortcut.mask,
             root, True, GrabModeAsync, GrabModeAsync);
  }
}
void cleanup() {
  XftColorFree(display, DefaultVisual(display, DefaultScreen(display)),
               DefaultColormap(display, DefaultScreen(display)), &xft_color);

  for (auto font : fallbackFonts) {
    XftFontClose(display, font);
  }
  XftFontClose(display, xft_font);
  for (auto m : monitors) {
    XftDrawDestroy(m.xft_draw);
  }
  XCloseDisplay(display);
}

int main() {
  // future me add atom array to the config
  // and change window name
  // add an array  of events
  //
  display = XOpenDisplay(NULL);
  if (!display) {
    fprintf(stderr, "Cannot open display\n");
    exit(1);
  }

  struct sigaction sa;

  /* do not transform children into zombies when they terminate */
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART;
  sa.sa_handler = SIG_IGN;
  sigaction(SIGCHLD, &sa, NULL);

  /* clean up any zombies (inherited from .xinitrc etc) immediately */
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
  root = DefaultRootWindow(display);
  XSetErrorHandler(errorHandler);

  setup();
  grab_keys();
  grabbuttons();

  create_bars();
  update_bar();
  run();

  cleanup();
  XCloseDisplay(display);
  return 0;
}
void detect_monitors() {
  if (!XineramaIsActive(display)) {
    fprintf(stderr, "Xinerama is not active.\n");
    exit(1);
  }

  int num_monitors;
  XineramaScreenInfo *screens = XineramaQueryScreens(display, &num_monitors);

  multi_monitor = num_monitors > 1;
  for (int i = 0; i < num_monitors; i++) {
    Monitor monitor;
    monitor.screen = screens[i].screen_number;
    monitor.x = screens[i].x_org;
    monitor.y = screens[i].y_org;
    monitor.width = screens[i].width;
    monitor.height = screens[i].height;
    monitor.index = i;
    monitors.push_back(monitor);
  }

  XFree(screens);

  if (monitors.size() > 0) {
    current_monitor = &monitors[0]; // Set first monitor as the current one
    workspaces = &current_monitor->workspaces;
    current_workspace = &(*workspaces)[0];
    clients = &(*workspaces)[0].clients;
    sticky = &(current_monitor->sticky);
  }
}

Monitor *find_monitor_by_coordinates(int x, int y) {
  for (auto &monitor : monitors) {
    if (x >= monitor.x && x < monitor.x + monitor.width && y >= monitor.y &&
        y < monitor.y + monitor.height) {
      return &monitor;
    }
  }
  return nullptr;
}
void focus_monitor(Monitor *monitor) {
  if (!monitor)
    return;

  // Focus the monitor's active workspace
  current_monitor = monitor;
  workspaces = &(current_monitor->workspaces);
  current_workspace =
      &current_monitor
           ->workspaces[current_monitor->at]; // really don't know wht i need
                                              // this but it's what it is
  clients = &current_workspace->clients;
  sticky = &(current_monitor->sticky);
  focused_window = None;
  update_bar();
}
void toggle_layout() {
  if (!(current_workspace->layout == 1 &&
        current_workspace->layout_index_place_holder == 1)) {
    std::swap(current_workspace->layout,
              current_workspace->layout_index_place_holder);
    arrange_windows();
    update_bar();
  }
}
void set_size_hints(Window win) {
  XSizeHints hints;
  hints.flags = PSize | PMinSize; // Enforce both min and max sizes
  hints.min_width = 100;          // Force width
  hints.min_height = 100;         // Force height

  XSetWMNormalHints(display, win, &hints);
}
bool wants_floating(Window win) {
  Atom type;
  int format;
  unsigned long nitems, bytes_after;
  unsigned char *data = NULL;

  // Get the _NET_WM_WINDOW_TYPE property from the window
  if (XGetWindowProperty(display, win, netatom[NetWMWindowType], 0, (~0L),
                         False, XA_ATOM, &type, &format, &nitems, &bytes_after,
                         &data) == Success) {
    Atom *atoms = (Atom *)data;
    for (unsigned long i = 0; i < nitems; i++) {
      if (atoms[i] == netatom[NetWMWindowTypeDialog] ||
          atoms[i] == netatom[NetToolBar] ||
          atoms[i] == netatom[NetClientInfo] || atoms[i] == NetWMFullscreen) {
        XFree(data);
        return true; // Floating window (e.g., dialog, toolbar, or utility)
      }
    }
    XFree(data);
  }
  return false;
}
/* void */
/* checkotherwm(void) */
/* { */
/* 	xerrorxlib = XSetErrorHandler(xerrorstart); */
/* 	 this causes an error if some other window manager is running */
/* 	XSelectInput(display, DefaultRootWindow(display),
 * SubstructureRedirectMask); */
/* 	XSync(display, False); */
/* 	XSetErrorHandler(xerror); */
/* 	XSync(display, False); */
/* } */
void movement_warp(Window *win) {
  XWindowAttributes wa;
  XGetWindowAttributes(display, *win, &wa);
  int x = wa.width / 2;
  int y = wa.height / 2;
  XWarpPointer(display, None, *win, 0, 0, 0, 0, x, y);
}
void grabbuttons() {
  for (int i = 0; i < 3; i++)
    XGrabButton(display, buttons[i].id, buttons[i].mask, root, False,
                BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
}
void setup() {
  XSetWindowAttributes wa;
  // init atoms
  name_atom = XInternAtom(display, "WM_NAME", False);
  protocol_atom = XInternAtom(display, "WM_PROTOCOLS", False);
  delete_atom = XInternAtom(display, "WM_DELETE_WINDOW", False);
  netatom[NetSupported] = XInternAtom(display, "_NET_SUPPORTED", False);
  netatom[NetWMName] = XInternAtom(display, "_NET_WM_NAME", False);
  netatom[NetWMState] = XInternAtom(display, "_NET_WM_STATE", False);
  netatom[NetWMCheck] = XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", False);
  netatom[NetWMFullscreen] =
      XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);
  netatom[NetWMWindowType] = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
  netatom[NetWMWindowTypeDialog] =
      XInternAtom(display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
  netatom[NetClientList] = XInternAtom(display, "_NET_CLIENT_LIST", False);
  netatom[NetClientInfo] = XInternAtom(display, "_NET_CLIENT_INFO", False);
  netatom[NetToolBar] = XInternAtom(display, "_NET_TOOLBAR", False);
  netatom[NetUtility] = XInternAtom(display, "_NET_UTILITY", False);

  // initing cursors
  cursors[CurNormal] = cur_create(XC_left_ptr);
  cursors[CurResize] = cur_create(XC_sizing);
  cursors[CurMove] = cur_create(XC_fleur);
  // initing handlers
  handler[ButtonPress] = handle_button_press_event;
  handler[ClientMessage] = handle_client_message;
  handler[ConfigureRequest] = handle_configure_request;
  handler[ConfigureNotify] = nothing; // Assuming nothing is defined correctly
  handler[DestroyNotify] = handle_destroy_notify;
  handler[EnterNotify] = handle_enter_notify;
  handler[Expose] = nothing; // Assuming nothing is defined correctly
  handler[FocusIn] = handle_focus_in;
  handler[FocusOut] = handle_focus_out;
  handler[KeyPress] = handle_key_press;
  handler[MappingNotify] = nothing; // Assuming nothing is defined correctly
  handler[MapRequest] = handle_map_request;
  handler[MotionNotify] = handle_motion_notify;
  handler[PropertyNotify] = handle_property_notify;
  handler[UnmapNotify] = nothing; // Fixed spelling from "nothin" to "nothing"
                                  //
                                  //
  // initing the font

  screen = DefaultScreen(display);

  xft_font = XftFontOpenName(display, screen, BAR_FONT.c_str());

  for (int i = 0; i < NUM_WORKSPACES; i++) {
    BUTTON_LABEL_UTF8[i] =
        reinterpret_cast<XftChar8 *>(const_cast<char *>(workspaces_names[i]));
    // determine button widths
    BUTTONS_WIDTHS[i] = std::max(
        BUTTON_WIDTH, get_utf8_string_width(display, BUTTON_LABEL_UTF8[i]));
  }
  for (int i = 1; i < NUM_WORKSPACES + 1; i++) {

    BUTTONS_WIDTHS_PRESUM[i] =
        BUTTONS_WIDTHS_PRESUM[i - 1] + BUTTONS_WIDTHS[i - 1];
  }
  detect_monitors();
  for (auto &monitor : monitors) {
    for (int i = 0; i < NUM_WORKSPACES; ++i) { // setting the workspace index
      monitor.workspaces[i].index = i;
    }
    monitor.current_workspace = &monitor.workspaces[0];
  }
  Atom utf8string = utf8string = XInternAtom(display, "UTF8_STRING", False);
  Window wmcheckwin = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, 0, 0);
  XChangeProperty(display, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
                  PropModeReplace, (unsigned char *)&wmcheckwin, 1);
  XChangeProperty(display, wmcheckwin, netatom[NetWMName], utf8string, 8,
                  PropModeReplace, (unsigned char *)"pwm", 3);
  XChangeProperty(display, root, netatom[NetWMCheck], XA_WINDOW, 32,
                  PropModeReplace, (unsigned char *)&wmcheckwin, 1);
  XChangeProperty(display, root, netatom[NetSupported], XA_ATOM, 32,
                  PropModeReplace, (unsigned char *)netatom, NetLast);
  wa.cursor = cursors[CurNormal];
  wa.event_mask = SubstructureRedirectMask | SubstructureNotifyMask |
                  ButtonPressMask | EnterWindowMask | LeaveWindowMask |
                  StructureNotifyMask | PropertyChangeMask | PointerMotionMask;
  XChangeWindowAttributes(display, root, CWEventMask | CWCursor, &wa);
  XSelectInput(display, root, wa.event_mask);
  add_existing_windows();
}
int getrootptr(int *x, int *y) {
  int di;
  unsigned int dui;
  Window dummy;

  return XQueryPointer(display, root, &dummy, &dummy, x, y, &di, &di, &dui);
}
void resizeclient(Client *c, int x, int y, int w, int h, int border_width) {
  XWindowChanges wc;

  c->x = wc.x = x;
  c->y = wc.y = y;
  c->width = wc.width = w;
  c->height = wc.height = h;
  wc.border_width = border_width;
  XConfigureWindow(display, c->window,
                   CWX | CWY | CWWidth | CWHeight | CWBorderWidth, &wc);
  configure(c, border_width);
  XSync(display, False);
}
void configure(Client *c, int border_width) {
  XConfigureEvent ce;

  ce.type = ConfigureNotify;
  ce.display = display;
  ce.event = c->window;
  ce.window = c->window;
  ce.x = c->x;
  ce.y = c->y;
  ce.width = c->width;
  ce.height = c->height;
  ce.border_width = border_width;
  ce.above = None;
  ce.override_redirect = False;
  XSendEvent(display, c->window, False, StructureNotifyMask, (XEvent *)&ce);
}
void set_fullscreen(Client *client, bool full_screen) {
  if (!client)
    return;

  if (!client->fullscreen && full_screen) {
    auto f = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", true);
    auto a = XInternAtom(display, "_NET_WM_STATE", true);

    client->fullscreen = true;
    XChangeProperty(display, client->window, a, XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)&f, 1);
    if (client->window == current_workspace->master) {
      current_workspace->master = None;
    }
  } else if (client->fullscreen && !full_screen) {
    // Exit full-screen and restore the original size and position
    XChangeProperty(display, client->window, netatom[NetWMState], XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)0, 0);
    client->fullscreen = false;
    client->floating = false;
  }
  focused_window = client->window;
  movement_warp(&client->window);
  arrange_windows();
  // idon't know why i need this here if I but if at the toggle fullscreen it
  // makes the browser video player fullscreen sheft down by the bar height
}
Atom getatomprop(Client *c, Atom prop) {
  int di;
  unsigned long dl;
  unsigned char *p = NULL;
  Atom da, atom = None;

  if (XGetWindowProperty(display, c->window, prop, 0L, sizeof atom, False,
                         XA_ATOM, &da, &di, &dl, &dl, &p) == Success &&
      p) {
    atom = *(Atom *)p;
    XFree(p);
  }
  return atom;
}

void updatewindowtype(Client *c) {
  Atom state = getatomprop(c, netatom[NetWMState]);
  Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

  if (state == netatom[NetWMFullscreen])
    set_fullscreen(c, 1);
  if (wtype == netatom[NetWMWindowTypeDialog])
    c->floating = 1;
}
Monitor *rect_to_mon(int x, int y, int w, int h) {
  Monitor *monitor;
  int a, area = 0;

  for (auto &m : monitors) {
    a = INTERSECT(x, y, w, h, m);
    if (a > area) {
      area = a;
      monitor = &m;
    }
  }

  return monitor;
}

void send_to_monitor(Client *client, Monitor *prev_monitor,
                     Monitor *next_monitor, bool rearrange) {
  if (!client || !prev_monitor || !next_monitor)
    return;
  client->monitor = next_monitor->index;
  if (client->sticky) {
    next_monitor->sticky.push_back(*client);
    prev_monitor->sticky.erase(
        std::remove_if(
            prev_monitor->sticky.begin(), prev_monitor->sticky.end(),
            [&client](const Client &c) { return c.window == client->window; }),
        prev_monitor->sticky.end());
    return;
  } else {
    next_monitor->workspaces[next_monitor->at].clients.push_back(*client);
    if (client->window == current_workspace->master) {
      current_workspace->master = None;
      prev_monitor->workspaces[prev_monitor->at].clients.erase(
          std::remove_if(
              prev_monitor->workspaces[prev_monitor->at].clients.begin(),
              prev_monitor->workspaces[prev_monitor->at].clients.end(),
              [&client](const Client &c) {
                return c.window == client->window;
              }),
          prev_monitor->workspaces[prev_monitor->at].clients.end());
    }
    (rearrange) ? arrange_windows()
                : void(); // arrange windows for next monitor
    // the bool for if we sending a float client we don't need the overhead of
    // rearrange
  }
}
void add_existing_windows() {
  Window root_return, parent_return;
  Window *children;
  unsigned int nchildren;

  // Get the list of child windows under the root window
  if (XQueryTree(display, root, &root_return, &parent_return, &children,
                 &nchildren)) {
    for (unsigned int i = 0; i < nchildren; i++) {
      XWindowAttributes attr;
      XGetWindowAttributes(display, children[i], &attr);

      if (attr.map_state == IsViewable && !attr.override_redirect) {
        clients->push_back({children[i]});
	all_windows.insert(children[i]);
        XSelectInput(display, children[i],
                     EnterWindowMask | FocusChangeMask | PropertyChangeMask |
                         StructureNotifyMask);
      }
    }

    if (children)
      XFree(children);
  }
}
