
#include "main.h"
#include "config.h"
#include <X11/Xlib.h>
#include <utility>

Display *display; // the connection to the X server
Window root; // the root window top level window all other windows are children
             // of it and covers all the screen
Window focused_window = None, bar_window;
XftFont *xft_font;
XftDraw *xft_draw;
XftColor xft_color;

int bar_hight_place_holder = 0;
short current_workspace = 0;
std::vector<Workspace> workspaces(NUM_WORKSPACES);
/* std::vector<Button> buttons(NUM_WORKSPACES); */
auto *clients = &workspaces[0].clients;

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
  XSelectInput(display, bar_window,
               ExposureMask | KeyPressMask | ButtonPressMask);
  XMapWindow(display, bar_window);
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
  if (!SHOW_BAR)
    return;
  int screen_width = DisplayWidth(display, DefaultScreen(display));

  XClearWindow(display, bar_window);

  // Draw workspace buttons
  for (int i = 0; i < NUM_WORKSPACES; ++i) {

    int x = i * BUTTONS_WIDTH;
    // Highlight the current workspace button
    if (i == current_workspace) {
      XSetForeground(display, DefaultGC(display, DefaultScreen(display)),
                     0x3399FF); // Light blue background
      XFillRectangle(display, bar_window,
                     DefaultGC(display, DefaultScreen(display)), x, 0,
                     BUTTONS_WIDTH, BAR_HEIGHT);
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
    warp_pointer_to_window(&focused_window);
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
    warp_pointer_to_window(&focused_window);
  }
}

void move_focused_window_x(const Arg *arg) {
  if (focused_window != None && focused_window != root) {
    XWindowChanges changes;
    XWindowAttributes wa;
    XSetInputFocus(display, focused_window, RevertToPointerRoot, CurrentTime);
    XGetWindowAttributes(display, focused_window, &wa);

    Window dummy =
        focused_window; // for if the window changed focus after moving
    changes.x = wa.x + arg->i;
    XConfigureWindow(display, focused_window, CWX, &changes);
    XConfigureWindow(display, dummy, CWX, &changes);
    warp_pointer_to_window(&dummy);
  }
}
void move_focused_window_y(const Arg *arg) {
  if (focused_window != None && focused_window != root) {
    XWindowChanges changes;
    XWindowAttributes wa;
    XSetInputFocus(display, focused_window, RevertToPointerRoot, CurrentTime);
    XGetWindowAttributes(display, focused_window, &wa);
    Window dummy =
        focused_window; // for if the window changed focus after moving
    changes.y = wa.y + arg->i;
    XConfigureWindow(display, focused_window, CWY, &changes);
    XConfigureWindow(display, dummy, CWY, &changes);
    warp_pointer_to_window(&dummy);
  }
}
void swap_window(const Arg *arg) {
  int index1 = get_focused_window_index(), index2 = index1 + arg->i;
  index2 = index2 < 0 ? clients->size() - 1 : index2;
  index2 = index2 >= clients->size() ? 0 : index2;
  if (index1 < clients->size() && index2 < clients->size()) {
    std::swap(clients[index1], clients[index2]);
    /* warp_pointer_to_window(focused_window); */
    tile_windows(); // Rearrange windows after swapping
    warp_pointer_to_window(&(*clients)[index2].window);
  }
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
    exit(1);
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
void kill_focused_window(const Arg *arg) {
  (void)arg;
  if (focused_window != None) {
    clients->erase(std::remove_if(clients->begin(), clients->end(),
                                  [](const Client &c) {
                                    return c.window == focused_window;
                                  }),
                   clients->end());
    XKillClient(display, focused_window);
    if (focused_window == workspaces[current_workspace].master) {
      workspaces[current_workspace].master = None;
      if (workspaces[current_workspace].clients.size() > 0) {
        workspaces[current_workspace].master =
            workspaces[current_workspace].clients[0].window;
      }
    }
    focused_window = None; // Reset focused window
                           //
    tile_windows();
  }
}
void set_master(const Arg *arg) {
  if (focused_window != None) {
    workspaces[current_workspace].master = focused_window;
    tile_windows();
    warp_pointer_to_window(&focused_window);
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
void switch_workspace(const Arg *arg) {
  int new_workspace = arg->i - 1;
  if (new_workspace == current_workspace || new_workspace > NUM_WORKSPACES ||
      new_workspace < 0)
    return;

  // Unmap windows from the current workspace
  for (auto &client : workspaces[current_workspace].clients) {
    XUnmapWindow(display, client.window);
  }

  current_workspace = new_workspace;

  // Map windows from the new workspace
  for (auto &client : workspaces[current_workspace].clients) {
    XMapWindow(display, client.window);
  }

  clients = &workspaces[current_workspace].clients;
  // Re-tile the windows in the new workspace
  tile_windows();
  update_bar();
}
void move_window_to_workspace(const Arg *arg) {
  int target_workspace = arg->i - 1;
  if (focused_window == None || target_workspace == current_workspace ||
      target_workspace < 0 || target_workspace > NUM_WORKSPACES) {
    return; // No window to move or target workspace is the same as current
  }

  // Remove window from the current workspace
  auto &current_clients = workspaces[current_workspace].clients;
  auto it = std::remove_if(
      current_clients.begin(), current_clients.end(),
      [=](Client &client) { return client.window == focused_window; });
  if (it != current_clients.end()) {
    current_clients.erase(it, current_clients.end());
  }

  // Add window to the target workspace
  XWindowAttributes wa;
  XGetWindowAttributes(display, focused_window, &wa);

  workspaces[target_workspace].clients.push_back(
      {focused_window, wa.x, wa.y, static_cast<unsigned int>(wa.width),
       static_cast<unsigned int>(wa.height), false});
  XUnmapWindow(display, focused_window);
  tile_windows();
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
  (SHOW_BAR) ? create_bar() : (void)0;
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

    if (c->window ==
        workspaces[current_workspace].master) { // First window is the master
      // Master window positioning with border
      XMoveResizeWindow(
          display, c->window, GAP_SIZE,
          BAR_HEIGHT + GAP_SIZE, // Position with gaps and bar height
          master_width -
              2 * (GAP_SIZE +
                   BORDER_WIDTH), // Width adjusted for border and gaps
          screen_height - BAR_HEIGHT -
              2 * (GAP_SIZE +
                   BORDER_WIDTH) // Height adjusted for bar, border, and gaps
      );
    } else {
      // Stack windows positioning with border
      int stack_width =
          screen_width - master_width - 2 * (GAP_SIZE + BORDER_WIDTH);
      int stack_height =
          (screen_height - BAR_HEIGHT - GAP_SIZE - 2 * BORDER_WIDTH) /
          (num_tiled_clients - 1);
      int x = master_width + GAP_SIZE;
      int y =
          BAR_HEIGHT + GAP_SIZE + (tiled_index - 1) * (stack_height + GAP_SIZE);

      XMoveResizeWindow(display, c->window, x, y, // Position with gaps
                        stack_width,              // Width adjusted for border
                        stack_height              // Height adjusted for border
      );
      ++tiled_index; // Only increment for non-floating windows
    }

    XLowerWindow(display, c->window); // Lower windows to avoid overlap
    XSetWindowBorderWidth(display, c->window, BORDER_WIDTH); // Set border width
  }
}

void toggle_floating(const Arg *arg) {
  if (focused_window == None)
    return;

  Client *client = find_client(focused_window);
  const int border_width = 2; // Border width in pixels

  if (client) {
    // Toggle the floating status of the focused window
    client->floating = !client->floating;

    if (client->floating) {
      // Get the current window's geometry before making it float
      XWindowAttributes wa;
      XGetWindowAttributes(display, focused_window, &wa);
      client->x = wa.x;
      client->y = wa.y;
      client->width = wa.width;
      client->height = wa.height;

      // Resize and move the window to some default position (e.g., 100, 100)
      // and set borders
      XMoveResizeWindow(display, focused_window, 100, 100,
                        800 - 2 * border_width, 600 - 2 * border_width);
      XSetWindowBorderWidth(
          display, focused_window,
          border_width); // Set border width for floating windows

      // Ensure the floating window is raised above other windows
      XRaiseWindow(display, focused_window);
    }

    // Rearrange windows after toggling floating
    tile_windows();

    // Optionally warp the mouse pointer to the newly floating window
    warp_pointer_to_window(&client->window);
  }
}
void toggle_bar(const Arg *arg) {
  SHOW_BAR = !SHOW_BAR;
  std::swap(bar_hight_place_holder, BAR_HEIGHT);
  if (SHOW_BAR) {
    XMapWindow(display, bar_window);
    XSelectInput(display, root,
                 SubstructureRedirectMask | SubstructureNotifyMask |
                     KeyPressMask | ExposureMask |
                     PropertyChangeMask); // resubscribe
    update_bar();
  } else {
    XUnmapWindow(display, bar_window);
    XSelectInput(display, root,
                 SubstructureRedirectMask | SubstructureNotifyMask |
                     KeyPressMask | ExposureMask); // unsubscibe for more speed
  }
  tile_windows();
}
void toggle_fullscreen(const Arg *arg) {
  auto client = find_client(focused_window);
  if (!client)
    return;

  if (!client->is_fullscreen) {
    // Save the original position and size
    XWindowAttributes wa;
    XGetWindowAttributes(display, client->window, &wa);
    client->x = wa.x;
    client->y = wa.y;
    client->width = wa.width;
    client->height = wa.height;

    // Go full-screen (resize to cover the entire screen)
    XMoveResizeWindow(display, client->window, 0, 0,
                      DisplayWidth(display, DefaultScreen(display)),
                      DisplayHeight(display, DefaultScreen(display)));

    // Remove window borders if needed
    XSetWindowBorderWidth(display, client->window, 0);

    // Raise the window to the top
    XRaiseWindow(display, client->window);

    client->is_fullscreen = true;
  } else {
    // Exit full-screen and restore the original size and position
    XMoveResizeWindow(display, client->window, client->x, client->y,
                      client->width, client->height);

    // Restore the window border
    XSetWindowBorderWidth(display, client->window,
                          2); // Adjust to your border size

    client->is_fullscreen = false;
  }
}
void one_window() {
  if (clients->size() == 1) {
    int screen_width = DisplayWidth(display, DefaultScreen(display));
    int screen_height = DisplayHeight(display, DefaultScreen(display));

    int height = screen_height - BAR_HEIGHT;

    // Move and resize the window to fit within the calculated region
    XMoveResizeWindow(display, clients->front().window, 0, BAR_HEIGHT,
                      screen_width, height);
  }
}

void exit_pwm(const Arg *arg) {
  XCloseDisplay(display);
  cleanup();
  exit(0);
}
