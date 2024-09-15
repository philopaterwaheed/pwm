
#include "main.h"
#include "config.h"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <cmath>
#include <utility>

Display *display; // the connection to the X server
Window root; // the root window top level window all other windows are children
             // of it and covers all the screen
Window focused_window = None;
XftFont *xft_font;
XftDraw *xft_draw;
XftColor xft_color;

std::vector<Monitor> monitors;      // List of monitors
Monitor *current_monitor = nullptr; // The monitor in focus
//
std::vector<Workspace> *workspaces;
Workspace *current_workspace;
std::vector<Client> *clients;

std::string status = "pwm by philo";
std::vector<XftFont *> fallbackFonts;

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
    std::string button_label = workspaces_names[i];
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

void warp_pointer_to_window(Window *win) {
  XWindowAttributes wa;
  XGetWindowAttributes(display, *win, &wa);
  int x = wa.width / 2;
  int y = wa.height / 2;
  XWarpPointer(display, None, *win, 0, 0, 0, 0, x, y);
}
void restack_windows() {}

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
  display = XOpenDisplay(nullptr);
  if (!display) {
    fprintf(stderr, "Cannot open display\n");
    exit(1);
  }

  root = DefaultRootWindow(display);

  // Subscribe to events on the root window
  XSelectInput(display, root,
               SubstructureRedirectMask | SubstructureNotifyMask |
                   KeyPressMask | ExposureMask | PropertyChangeMask |
                   MotionNotify | SubstructureRedirectMask |
                   SubstructureNotifyMask | ButtonPressMask |
                   PointerMotionMask | EnterWindowMask | LeaveWindowMask |
                   StructureNotifyMask | PropertyChangeMask);

  Cursor cursor =                    // not for future evry action needs a shape
      XCreateFontCursor(display, 2); // Set the cursor for the window
  XDefineCursor(display, root, cursor);
  // Capture key presses for the mod key (e.g., Mod4Mask) with any key

  detect_monitors();
  grab_keys();
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
  for (int i = 0; i < clients->size(); ++i) {
    Client *c = (*clients)[i];

    if (c->window == current_workspace->master) { // First window is the master
      // Master window positioning with border
      if (num_tiled_clients == 1) {
        make_fullscreen(c, screen_width, screen_height, false);
        break; // we have no more windows
      }
      XMoveResizeWindow(
          display, c->window, current_monitor->x + GAP_SIZE,
          current_monitor->y + current_workspace->bar_height +
              GAP_SIZE, // Position with gaps and bar height
          master_width -
              2 * (GAP_SIZE +
                   BORDER_WIDTH), // Width adjusted for border and gaps
          screen_height - current_workspace->bar_height -
              2 * (GAP_SIZE +
                   BORDER_WIDTH) // Height adjusted for bar, border, and gaps
      );
    } else {
      // Stack windows positioning with border
      int stack_width =
          screen_width - master_width - 2 * (GAP_SIZE + BORDER_WIDTH);
      int stack_height = (screen_height - current_workspace->bar_height -
                          GAP_SIZE - 2 * BORDER_WIDTH) /
                         (num_tiled_clients - 1);
      int x = current_monitor->x + master_width + GAP_SIZE;
      int y = current_workspace->bar_height + current_monitor->y + GAP_SIZE +
              (tiled_index - 1) * (stack_height + GAP_SIZE);

      XMoveResizeWindow(display, c->window, x, y, // Position with gaps
                        stack_width,              // Width adjusted for border
                        stack_height              // Height adjusted for border
      );
      ++tiled_index; // Only increment for non-floating windows
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
    XMoveResizeWindow(display, client->window,
                      current_monitor->x + (col * win_width) + GAP_SIZE,
                      current_monitor->y + row * win_height +
                          current_workspace->bar_height,
                      win_width - 2 * (GAP_SIZE + BORDER_WIDTH),
                      win_height - 2 * (GAP_SIZE + BORDER_WIDTH));
    XSetWindowBorderWidth(display, client->window,
                          BORDER_WIDTH); // Set border width
    i++;
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

  XMoveResizeWindow(display, client->window, current_monitor->x + GAP_SIZE,
                    current_monitor->y + current_workspace->bar_height +
                        GAP_SIZE,
                    screen_width - 2 * (GAP_SIZE + BORDER_WIDTH),
                    screen_height - current_workspace->bar_height -
                        2 * (GAP_SIZE + BORDER_WIDTH));
  XSetWindowBorderWidth(display, client->window, BORDER_WIDTH);
  // Go full-screen (resize to cover the entire screen)
  (raise) ? XRaiseWindow(display, client->window) : 0;

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
  // First, count the number of non-floating (arranged) clients and store the
  // them
  for (auto &client : *clients) {
    if (!client.floating) {
      arranged_clients.push_back(&client);

    }
    else if (client.fullscreen) {
      fullscreen_clients.push_back(&client);
    }
  }
  int num_arranged_clients = arranged_clients.size();

  // If there are no arranged_clients clients, we don't need to arrange anything
  if (num_arranged_clients == 0)
    return;

  if (current_workspace->master == None) {
    current_workspace->master = clients->front().window;
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
}
void toggle_layout() {
  std::swap(current_workspace->layout,
            current_workspace->layout_index_place_holder);
  arrange_windows();
  update_bar();
}
