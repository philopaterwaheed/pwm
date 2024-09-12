
#include "main.h"
#include "config.h"
#include <X11/Xlib.h>

Display *display; // the connection to the X server
Window root; // the root window top level window all other windows are children
             // of it and covers all the screen
Window focused_window = None, bar_window;
XftFont *xft_font;
XftDraw *xft_draw;
XftColor xft_color;

std::vector<Workspace> workspaces(NUM_WORKSPACES);
auto *clients = &workspaces[0].clients;
auto *current_workspace = &workspaces[0];

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

void create_bar() {
  int screen_width = DisplayWidth(display, DefaultScreen(display));
  int screen_height = DisplayHeight(display, DefaultScreen(display));

  // Create the bar window
  bar_window = XCreateSimpleWindow(display, root, 0, 0, screen_width,
                                   BAR_HEIGHT, 0, 0, 0x000000);
  if (SHOW_BAR) { // for if show bar is false in the config
    XSelectInput(display, bar_window,
                 ExposureMask | KeyPressMask | ButtonPressMask);
    XMapWindow(display,
               bar_window); // we create and don't show for future errors
  }
  // Load the font using Xft
  xft_font = XftFontOpenName(display, DefaultScreen(display), BAR_FONT.c_str());

  // Create a drawable for the bar
  xft_draw = XftDrawCreate(display, bar_window,
                           DefaultVisual(display, DefaultScreen(display)),
                           DefaultColormap(display, DefaultScreen(display)));

  // Set the color for drawing text (white in this case)
  XRenderColor render_color = {0xffff, 0xffff, 0xffff, 0xffff}; // White color
  XftColorAllocValue(display, DefaultVisual(display, DefaultScreen(display)),
                     DefaultColormap(display, DefaultScreen(display)),
                     &render_color, &xft_color);
}
void update_bar() {
  if (!current_workspace->show_bar) {
    // if we change the workspace and it's hidden there if we don't unmap it
    // will still exist in the new work space to not show the bar
    XUnmapWindow(display, bar_window);
    return;
  }
  // if we were in a workspace that has the bar hidden and changed to a
  // workspace that does we need to remap
  XMapWindow(display, bar_window);
  int screen_width = DisplayWidth(display, DefaultScreen(display));

  XClearWindow(display, bar_window);

  // Draw workspace buttons
  for (int i = 0; i < NUM_WORKSPACES; ++i) {
    int x = i * BUTTONS_WIDTH;
    // Highlight the current workspace button
    if (i == current_workspace->index) {
      XSetForeground(display, DefaultGC(display, DefaultScreen(display)),
                     0x3399FF); // Light blue background
      XFillRectangle(display, bar_window,
                     DefaultGC(display, DefaultScreen(display)), x, 0,
                     BUTTONS_WIDTH, current_workspace->bar_height);
    }
    std::string button_label = workspaces_names[i];
    draw_text_with_dynamic_font(display, bar_window, xft_draw, &xft_color,
                                button_label.c_str(), x + 10, 15,
                                XDefaultScreen(display));
  }
  draw_text_with_dynamic_font(display, bar_window, xft_draw, &xft_color, status,
                              get_utf8_string_width(display, xft_font, status),
                              15, XDefaultScreen(display));
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
                   KeyPressMask | ExposureMask | PropertyChangeMask);

  Cursor cursor =                    // not for future evry action needs a shape
      XCreateFontCursor(display, 2); // Set the cursor for the window
  XDefineCursor(display, root, cursor);
  // Capture key presses for the mod key (e.g., Mod4Mask) with any key

  grab_keys();
  for (int i = 0; i < NUM_WORKSPACES; ++i) { // setting the workspace index
    workspaces[i].index = i;
  }
  create_bar();
  update_bar();
  run();

  cleanup();
  XCloseDisplay(display);
  return 0;
}
void tile_windows() {
  unsigned int num_tiled_clients = 0;
  std::vector<Client> fullscrren_clients;
  // First, count the number of non-floating (tiled) clients
  if (clients->size() == 1) {
    one_window();
    return;
  }
  for (const auto &client : *clients) {
    if (!client.floating) {
      ++num_tiled_clients;
    }
  }

  // If there are no tiled clients, we don't need to arrange anything
  if (num_tiled_clients == 0)
    return;

  if (current_workspace->master == None) {
    current_workspace->master = clients->front().window;
  }
  int screen_width = DisplayWidth(display, DefaultScreen(display));
  int screen_height = DisplayHeight(display, DefaultScreen(display));

  // Calculate master width (60% of screen width)
  int master_width = screen_width * 0.6;

  unsigned int tiled_index = 1; // To track the position of tiled clients
  for (unsigned int i = 0; i < clients->size(); ++i) {
    Client *c = &(*clients)[i];
    if (c->floating) {
      // Skip floating windows and store their indices for later raising
      continue;
    }
    if (c->is_fullscreen) {
      // Skip floating windows and store their indices for later raising
      continue;
    }

    if (c->window == current_workspace->master) { // First window is the master
      // Master window positioning with border
      XMoveResizeWindow(
          display, c->window, GAP_SIZE,
          current_workspace->bar_height +
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
      int x = master_width + GAP_SIZE;
      int y = current_workspace->bar_height + GAP_SIZE +
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

void one_window() {
  if (clients->size() == 1) {
    int screen_width = DisplayWidth(display, DefaultScreen(display));
    int screen_height = DisplayHeight(display, DefaultScreen(display));

    int height = screen_height - current_workspace->bar_height;

    // Move and resize the window to fit within the calculated region
    current_workspace->master = clients->front().window;
    XMoveResizeWindow(display, clients->front().window, 0,
                      current_workspace->bar_height, screen_width, height);
  }
}
