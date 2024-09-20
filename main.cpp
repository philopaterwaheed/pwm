
#include "main.h"
#include "config.h"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <cstdlib>
#include <string>

Display *display; // the connection to the X server
Window root; // the root window top level window all other windows are children
             // of it and covers all the screen
Window focused_window = None;
XftFont *xft_font;
XftDraw *xft_draw;
XftColor xft_color;
extern Cursor cursors[3];

std::vector<Monitor> monitors;      // List of monitors
Monitor *current_monitor = nullptr; // The monitor in focus
//
std::vector<Workspace> *workspaces;
Workspace *current_workspace;
std::vector<Client> *clients;

std::string status = "pwm by philo";
std::vector<XftFont *> fallbackFonts;
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
  for (auto &client : *clients) {
    if (client.window == w) {
      return &client;
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
    xft_font =
        XftFontOpenName(display, DefaultScreen(display), BAR_FONT.c_str());

    // Create a drawable for the bar
    xft_draw = XftDrawCreate(display, monitor.bar,
                             DefaultVisual(display, DefaultScreen(display)),
                             DefaultColormap(display, DefaultScreen(display)));

    // Set the color for drawing text (white in this case)
    XRenderColor render_color = {0xffff, 0xffff, 0xffff, 0xffff}; // White color
    XftColorAllocValue(display, DefaultVisual(display, DefaultScreen(display)),
                       DefaultColormap(display, DefaultScreen(display)),
                       &render_color, &xft_color);
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
    int x = i * BUTTONS_WIDTH;
    // Highlight the current workspace button
    if (i == current_workspace->index) {
      XSetForeground(display, DefaultGC(display, DefaultScreen(display)),
                     0x3399FF); // Light blue background
      XFillRectangle(display, current_monitor->bar,
                     DefaultGC(display, DefaultScreen(display)), x, 0,
                     BUTTONS_WIDTH, current_workspace->bar_height);
    }
    std::string button_label = std::to_string(find_monitor_index(current_monitor)); //workspaces_names[i];
    draw_text_with_dynamic_font(display, current_monitor->bar, xft_draw,
                                &xft_color, button_label.c_str(), x + 10, 15,
                                XDefaultScreen(display),
                                current_monitor->width);
  }
  draw_text_with_dynamic_font(
      display, current_monitor->bar, xft_draw, &xft_color,
      LAYOUTS[current_workspace->layout].name.c_str(),
      (NUM_WORKSPACES)*BUTTONS_WIDTH + 10, 15, XDefaultScreen(display),
      current_monitor->width); // to show the layout name of the workspaces
  draw_text_with_dynamic_font(display, current_monitor->bar, xft_draw,
                              &xft_color, status, 500, 15,
                              XDefaultScreen(display), current_monitor->width);
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
    case FocusOut:
      handle_focus_out(&ev);
      break;
    case EnterNotify:
      handle_enter_notify(&ev);
      break;
    case KeyPress:
      handle_key_press(&ev);
      break;
    case PropertyNotify:
      update_status(&ev);
      update_bar();
      break;
    case ButtonPress:
      handle_button_press_event(&ev);
      break;
    case MotionNotify:
      handle_motion_notify(&ev);
      break;
    case DestroyNotify:
      handle_destroy_notify(&ev);
      break;
    }
  }
}

void update_status(XEvent *ev) {

  if (ev->xproperty.atom == XInternAtom(display, "WM_NAME", False)) {
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
        status = "pwm";
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
  XftFontClose(display, xft_font);
  XftDrawDestroy(xft_draw);
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

  // Subscribe to events on the root window
  XSelectInput(display, root,
               SubstructureRedirectMask | SubstructureNotifyMask |
                   KeyPressMask | ExposureMask | PropertyChangeMask |
                   /* MotionNotify | */ SubstructureRedirectMask |
                   SubstructureNotifyMask | ButtonPressMask |
                   PointerMotionMask | EnterWindowMask | LeaveWindowMask |
                   StructureNotifyMask | PropertyChangeMask);

  Cursor cursor =                    // not for future evry action needs a shape
      XCreateFontCursor(display, 2); // Set the cursor for the window
  XDefineCursor(display, root, cursor);
  // Capture key presses for the mod key (e.g., Mod4Mask) with any key

  setup();
  detect_monitors();
  grab_keys();
  grabbuttons();

  for (auto &monitor : monitors) {
    for (int i = 0; i < NUM_WORKSPACES; ++i) { // setting the workspace index
      monitor.workspaces[i].index = i;
    }
    monitor.current_workspace = &monitor.workspaces[0];
  }
  create_bars();
  update_bar();
  run();

  cleanup();
  XCloseDisplay(display);
  return 0;
}
void tile_windows(std::vector<Client *> *clients, int master_width,
                  int screen_height, int screen_width) {
  unsigned int tiled_index = 1; // To track the position of tiled clients
  int num_tiled_clients = clients->size();
  int stack_width = screen_width - master_width - 2 * (GAP_SIZE + BORDER_WIDTH);
  float done_factors = 0;
  int last_y = current_monitor->y + current_workspace->bar_height;
  int last_height = 0;
  for (int i = 0; i < clients->size(); ++i) {
    Client *c = (*clients)[i];

    if (c->window == current_workspace->master) { // First window is the master
      // Master window positioning with border
      if (num_tiled_clients == 1) {
        make_fullscreen(c, screen_width, screen_height, false);
        break; // we have no more windows
      }
      int x = current_monitor->x + GAP_SIZE;
      int y = current_monitor->y + current_workspace->bar_height + GAP_SIZE;
      int w = master_width - 2 * (GAP_SIZE + BORDER_WIDTH);
      int h = screen_height - current_workspace->bar_height -
              2 * (GAP_SIZE + BORDER_WIDTH);
      XMoveResizeWindow(display, c->window, x, y, w, h);
      c->x = x;
      c->y = y;
      c->width = w;
      c->height = h;
    } else {
      // Stack windows positioning with border
      int y = last_y + ((tiled_index != 1) + 1) *
                           (GAP_SIZE + (tiled_index != 1) *
                                           BORDER_WIDTH); // the starting y
      int stack_height = last_height =
          (c->cfact / current_workspace->cfacts) *
          ((screen_height - current_workspace->bar_height) -
           2 * (num_tiled_clients - 1) * (GAP_SIZE + BORDER_WIDTH));
      last_y = y + last_height; // the y of the client
      int x = current_monitor->x + master_width + GAP_SIZE;
      // Move and resize the window with calculated position and size
      XMoveResizeWindow(display, c->window, x, y, // Position with gaps
                        stack_width,              // Width adjusted for border
                        stack_height              // Height adjusted for border
      );

      c->x = x;
      c->y = y;
      c->width = stack_width;
      c->height = stack_height;

      ++tiled_index;
      done_factors += c->cfact;
    }
    XLowerWindow(display,
                 c->window); // Lower windows to avoid overlap with floaters
    XSetWindowBorderWidth(display, c->window, BORDER_WIDTH); // Set border width
  }
}

void monocle_windows(std::vector<Client *> *clients, int master_width,
                     int screen_height, int screen_width) {
  for (int i = 0; i < clients->size(); ++i) {
    Client *c = (*clients)[i];
    make_fullscreen(c, screen_width, screen_height);
  }
}
void grid_windows(std::vector<Client *> *clients, int master_width,
                  int screen_height, int screen_width) {

  int num_clients = (*clients).size();
  int rows = (int)std::sqrt(num_clients);
  int cols = (num_clients + rows - 1) /
             rows; // Ensure enough columns to fit all windows

  int win_width = screen_width / cols;
  int win_height = screen_height / rows;

  int i = 0;
  for (auto &client : *clients) {
    int row = i / cols;
    int col = i % cols;
    // saving for resizing
    client->x = current_monitor->x + (col * win_width) + GAP_SIZE;
    client->y =
        current_monitor->y + row * win_height + current_workspace->bar_height;
    client->width = win_width - 2 * (GAP_SIZE + BORDER_WIDTH);
    client->height = win_height - 2 * (GAP_SIZE + BORDER_WIDTH);
    XMoveResizeWindow(display, client->window, client->x, client->y,
                      client->width, client->height);
    XSetWindowBorderWidth(display, client->window,
                          BORDER_WIDTH); // Set border Width

    XLowerWindow(display,
                 client->window); // Lower windows to avoid overlap with floaters
    i++;
  }
}
void center_master_windows(std::vector<Client *> *clients, int master_width,
                           int screen_height, int screen_width) {

  int num_clients = clients->size();
  int tiled_index = 0;

  // Calculate number of clients on each side of the master window
  int right_clients = (num_clients - 1) / 2;
  int left_clients = (num_clients - 1) - right_clients;

  // Calculate available space for stacking windows
  int stack_width =
      (screen_width - master_width) / 2 - 2 * (GAP_SIZE + BORDER_WIDTH);
  int stack_height, x, y;
  float right_factors = 0, left_factors = 0, done_factors_right = 0,
        done_factors_left = 0;
  int last_y_right = current_monitor->y + current_workspace->bar_height;
  int last_y_left = current_monitor->y + current_workspace->bar_height;
  int last_height_right = 0, last_height_left = 0;
  unsigned int left_index = 0, right_index = 0;

  for (int i = 0, x = 0; x < num_clients; x++) {
    if (current_workspace->master == (*clients)[x]->window) {
      continue;
    }
    if (i < left_clients) {
      left_factors += (*clients)[x]->cfact;
      i++;
    } else {
      right_factors += (*clients)[x]->cfact;
      i++;
    }
  }
  for (int i = 0; i < num_clients; ++i) {
    Client *c = (*clients)[i];

    if (c->window == current_workspace->master) { // Master window
      if (num_clients == 1) {
        // If only one window, make it fullscreen
        make_fullscreen(c, screen_width, screen_height, false);
        break; // Exit after handling the single window
      }
      // Position master window
      XMoveResizeWindow(display, c->window,
                        current_monitor->x + (screen_width - master_width) /
                                                 2, // Centered horizontally
                        current_monitor->y + current_workspace->bar_height +
                            GAP_SIZE, // Position adjusted for bar and gap
                        master_width, // Master width
                        screen_height - current_workspace->bar_height -
                            2 * GAP_SIZE // Height adjusted for bar and gaps
      );
    } else { // Tiled windows
      if (tiled_index < left_clients) {
        // Position for left side of the master
        y = last_y_left +
            ((left_index != 0) + 1) *
                (GAP_SIZE + (left_index != 0) * BORDER_WIDTH); // the starting y
        stack_height = last_height_left =
            (c->cfact / left_factors) *
            ((screen_height - current_workspace->bar_height) -
             2 * (left_clients) * (GAP_SIZE + BORDER_WIDTH));
        last_y_left = y + last_height_left;               // the y of the client
        x = current_monitor->x + GAP_SIZE + BORDER_WIDTH; // the starting x;
        done_factors_left += c->cfact;
        left_index++;

      } else {
        // Position for right side of the master
        y = last_y_right +
            ((left_index != 0) + 1) *
                (GAP_SIZE + (left_index != 0) * BORDER_WIDTH); // the starting y
        stack_height = last_height_right =
            (c->cfact / right_factors) *
            ((screen_height - current_workspace->bar_height) -
             2 * (right_clients) * (GAP_SIZE + BORDER_WIDTH));
        last_y_right = y + last_height_right; // the y of the client
        x = (current_monitor->x + master_width +
             (screen_width - master_width) / 2) +
            GAP_SIZE + BORDER_WIDTH * 2;
        done_factors_right += c->cfact;

        right_index++;
      }

      XMoveResizeWindow(display, c->window, x, y, stack_width, stack_height);
      tiled_index++;
      // saveing the tiled x,y,hight,widtg for future resizing
      c->x = x;
      c->y = y;
      c->width = stack_width;
      c->height = stack_height;
    }

    XLowerWindow(display, c->window); // Lower windows to avoid overlap
    XSetWindowBorderWidth(display, c->window, BORDER_WIDTH); // Uncomment to
    // set border width if needed
  }
}
void make_fullscreen(Client *client, int screen_width, int screen_height,
                     bool raise) {
  // Save the original position and size
  XWindowAttributes wa;
  XGetWindowAttributes(display, client->window, &wa);
  client->x = wa.x;
  client->y = wa.y;
  client->width = wa.width;
  client->height = wa.height;
  ////saving for future unmake fullscreen

  XMoveResizeWindow(display, client->window, current_monitor->x + GAP_SIZE,
                    current_monitor->y + current_workspace->bar_height +
                        GAP_SIZE,
                    screen_width - 2 * (GAP_SIZE + BORDER_WIDTH),
                    screen_height - current_workspace->bar_height -
                        2 * (GAP_SIZE + BORDER_WIDTH));
  XSetWindowBorderWidth(display, client->window, BORDER_WIDTH);
  // Go full-screen (resize to cover the entire screen)
  (raise) ? XRaiseWindow(display, client->window)
          : XLowerWindow(display, client->window);
}
void detect_monitors() {
  if (!XineramaIsActive(display)) {
    fprintf(stderr, "Xinerama is not active.\n");
    exit(1);
  }

  int num_monitors;
  XineramaScreenInfo *screens = XineramaQueryScreens(display, &num_monitors);

  for (int i = 0; i < num_monitors; i++) {
    Monitor monitor;
    monitor.screen = screens[i].screen_number;
    monitor.x = screens[i].x_org;
    monitor.y = screens[i].y_org;
    monitor.width = screens[i].width;
    monitor.height = screens[i].height;
    monitors.push_back(monitor);
  }

  XFree(screens);

  if (monitors.size() > 0) {
    current_monitor = &monitors[0]; // Set first monitor as the current one
    workspaces = &current_monitor->workspaces;
    current_workspace = &(*workspaces)[0];
    clients = &(*workspaces)[0].clients;
  }
}
Monitor *find_monitor_for_window(int x, int y) {
  for (auto &monitor : monitors) {
    if (x >= monitor.x && x < monitor.x + monitor.width && y >= monitor.y &&
        y < monitor.y + monitor.height) {
      return &monitor;
    }
  }
  return nullptr; // Default to no monitor
}

void assign_client_to_monitor(Client *client) {
  XWindowAttributes wa;
  XGetWindowAttributes(display, client->window, &wa);

  Monitor *monitor = find_monitor_for_window(wa.x, wa.y);
  if (!monitor->current_workspace) {
    monitor->current_workspace = &(*workspaces)[0];
  }
  if (monitor) {
    monitor->current_workspace->clients.push_back(*client);
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
  focused_window = None;
  update_bar();
  warp(NULL);
}
unsigned int find_monitor_index(Monitor *monitor) {
  for (unsigned int i = 0; i < monitors.size(); ++i) {
    if (&monitors[i] == monitor) {
      return i;
    }
  }
  return -1; // Not found
}
void arrange_windows() {
  std::vector<Client *> fullscreen_clients;
  std::vector<Client *> arranged_clients;
  XWindowAttributes att;
  // First, count the number of non-floating (arranged) clients and store the
  // them

  current_workspace->cfacts = 0;
  for (auto &client : *clients) {
    if (XGetWindowAttributes(display, client.window, &att) == 0) {
      if (client.window == current_workspace->master)
        current_workspace->master = None;
      continue;
    }
    if (!client.floating) {
      if (client.window != current_workspace->master)
        current_workspace->cfacts += client.cfact;
      arranged_clients.push_back(&client);

    } else if (client.fullscreen) {
      fullscreen_clients.push_back(&client);
    }
  }
  int num_arranged_clients = arranged_clients.size();

  // If there are no arranged_clients clients, we don't need to arrange anything
  if (num_arranged_clients == 0)
    return;

  if (current_workspace->master == None && !arranged_clients.empty()) {
    current_workspace->master = arranged_clients[0]->window;
    current_workspace->cfacts -= arranged_clients[0]->cfact;
  }
  int screen_width =
      current_monitor->width; // DisplayWidth(display, DefaultScreen(display));
  int screen_height =
      current_monitor
          ->height; // DisplayHeight(display, DefaultScreen(display));
  // Calculate master width (60% of screen width)
  int master_width = screen_width * current_workspace->master_persent;
  LAYOUTS[current_workspace->layout].arrange(&arranged_clients, master_width,
                                             screen_height, screen_width);
  if (!(LAYOUTS[current_workspace->layout].index ==
        1)) // monocle_layout // we already did this we don't need to do it
            // twice
    for (auto &client :
         fullscreen_clients) { // the usser won't be able to make
                               // more than one fullscreen
                               // window in the any arranged mood anyways
      make_fullscreen(client, screen_width, screen_height);
    }
  if (focused_window == None) {
    focused_window = current_workspace->master;
  }
  warp(find_client(focused_window));
}
void toggle_layout() {
  if (current_workspace->layout == 1 &&
      current_workspace->layout_index_place_holder == 1) {
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

  Atom net_wm_window_type = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
  Atom net_wm_window_type_dialog =
      XInternAtom(display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
  Atom net_wm_window_type_toolbar =
      XInternAtom(display, "_NET_WM_WINDOW_TYPE_TOOLBAR", False);
  Atom net_wm_window_type_utility =
      XInternAtom(display, "_NET_WM_WINDOW_TYPE_UTILITY", False);

  // Get the _NET_WM_WINDOW_TYPE property from the window
  if (XGetWindowProperty(display, win, net_wm_window_type, 0, (~0L), False,
                         XA_ATOM, &type, &format, &nitems, &bytes_after,
                         &data) == Success) {
    Atom *atoms = (Atom *)data;
    for (unsigned long i = 0; i < nitems; i++) {
      if (atoms[i] == net_wm_window_type_dialog ||
          atoms[i] == net_wm_window_type_toolbar ||
          atoms[i] == net_wm_window_type_utility) {
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
/* 	XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask); */
/* 	XSync(dpy, False); */
/* 	XSetErrorHandler(xerror); */
/* 	XSync(dpy, False); */
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
  cursors[CurNormal] = cur_create(XC_left_ptr);
  cursors[CurResize] = cur_create(XC_sizing);
  cursors[CurMove] = cur_create(XC_fleur);
}
int getrootptr(int *x, int *y) {
  int di;
  unsigned int dui;
  Window dummy;

  return XQueryPointer(display, root, &dummy, &dummy, x, y, &di, &di, &dui);
}
void resizeclient(Client *c, int x, int y, int w, int h) {
  XWindowChanges wc;

  c->x = wc.x = x;
  c->y = wc.y = y;
  c->width = wc.width = w;
  c->height = wc.height = h;
  XConfigureWindow(display, c->window,
                   CWX | CWY | CWWidth | CWHeight | CWBorderWidth, &wc);
  configure(c);
  XSync(display, False);
}
void configure(Client *c) {
  XConfigureEvent ce;

  ce.type = ConfigureNotify;
  ce.display = display;
  ce.event = c->window;
  ce.window = c->window;
  ce.x = c->x;
  ce.y = c->y;
  ce.width = c->width;
  ce.height = c->height;
  ce.border_width = BORDER_WIDTH;
  ce.above = None;
  ce.override_redirect = False;
  XSendEvent(display, c->window, False, StructureNotifyMask, (XEvent *)&ce);
}
