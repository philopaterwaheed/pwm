
#include "main.h"
#include "config.h"
#include <X11/X.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <cstdlib>
#include <fontconfig/fontconfig.h>
#include <iostream>
#include <string>
#include <unordered_map>

Display *display; // the connection to the X server
Window root; // the root window top level window all other windows are children
             // of it and covers all the screen
Window focused_window = None, master_window = None, bar_window;
XftFont *xft_font;
XftDraw *xft_draw;
XftColor xft_color;
std::unordered_map<FcChar32, XftFont *> font_cache;

short current_workspace = 0;
std::vector<Workspace> workspaces(NUM_WORKSPACES);
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

// Cache to store fonts for each Unicode character or character range

// Function to select and cache the appropriate font for a character
XftFont *select_font_for_char(Display *display, FcChar32 ucs4, int screen) {
  // Check if the font for this character is already cached
  if (font_cache.find(ucs4) != font_cache.end()) {
    return font_cache[ucs4];
  }

  // If not cached, use Fontconfig to find the appropriate font
  FcPattern *pattern = FcPatternCreate();
  FcCharSet *charset = FcCharSetCreate();
  FcCharSetAddChar(charset, ucs4);
  FcPatternAddCharSet(pattern, FC_CHARSET, charset);
  FcPatternAddBool(pattern, FC_SCALABLE, FcTrue);

  FcResult result;
  FcPattern *match = FcFontMatch(nullptr, pattern, &result);
  XftFont *font = nullptr;

  if (match) {
    char *font_name = nullptr;
    FcPatternGetString(match, FC_FAMILY, 0, (FcChar8 **)&font_name);
    if (font_name) {
      font = XftFontOpenName(display, screen, font_name);
    }
    FcPatternDestroy(match);
  }

  FcPatternDestroy(pattern);
  FcCharSetDestroy(charset);

  // Fallback to default font if no match was found
  if (!font) {
    font = XftFontOpenName(display, screen, "fixed");
  }

  // Cache the font for this character
  font_cache[ucs4] = font;

  return font;
}

// Function to render text, choosing appropriate font dynamically with caching
void draw_text_with_dynamic_font(Display *display, Window window, XftDraw *draw,
                                 XftColor *color, const std::string &text,
                                 int x, int y, int screen) {
  int x_offset = x;

  for (size_t i = 0; i < text.size();) {
    // Decode the UTF-8 character
    FcChar32 ucs4;
    int bytes = FcUtf8ToUcs4((FcChar8 *)(&text[i]), &ucs4, text.size() - i);
    if (bytes <= 0)
      break;

    // Select and cache the appropriate font for this character
    XftFont *font = select_font_for_char(display, ucs4, screen);
    if (!font) {
      std::cerr << "Failed to load font for character: " << ucs4 << std::endl;
      break;
    }

    // Render the character using the selected (or cached) font
    XftDrawStringUtf8(draw, color, font, x_offset, y, (FcChar8 *)&text[i],
                      bytes);
    XGlyphInfo extents;
    XftTextExtentsUtf8(display, font, (FcChar8 *)&text[i], bytes, &extents);
    x_offset += extents.xOff;

    // Move to the next character
    i += bytes;
  }
}
int get_utf8_string_width(Display* display, XftFont* font, const std::string& text) {
    // Convert std::string to XftChar8 array
    
    XGlyphInfo extents;
    XftChar8* utf8_string = reinterpret_cast<XftChar8*>(const_cast<char*>(text.c_str()));
    
    // Measure the width of the UTF-8 string
    XftTextExtentsUtf8(display, font, utf8_string, text.length(), &extents);

    return extents.width;
}
void create_bar() {
  int screen_width = DisplayWidth(display, DefaultScreen(display));
  int screen_height = DisplayHeight(display, DefaultScreen(display));

  // Create the bar window
  bar_window = XCreateSimpleWindow(display, root, 0, 0, screen_width,
                                   BAR_HEIGHT, 0, 0, 0x000000);
  XSelectInput(display, bar_window, ExposureMask | KeyPressMask | ButtonPressMask);
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
  int screen_width = DisplayWidth(display, DefaultScreen(display));

  XClearWindow(display, bar_window);

  // Workspace names
  std::string workspace_status = "";
  for (int i = 0; i < NUM_WORKSPACES; ++i) {
    if (i == current_workspace) {
      workspace_status += "[*] ";
    } else {
      workspace_status += "[ ] ";
    }
  }

  // Draw workspace names using Xft
  draw_text_with_dynamic_font(display, bar_window, xft_draw, &xft_color,
                              workspace_status, 10, 15,
                              XDefaultScreen(display));

  /* XftDrawStringUtf8(xft_draw, &xft_color, xft_font,  500 + status.size() ,
   * 15, reinterpret_cast<const FcChar8*>(status.c_str()), status.size()); */
  draw_text_with_dynamic_font(display, bar_window, xft_draw, &xft_color, status,
                              get_utf8_string_width(display, xft_font, status), 15, XDefaultScreen(display));
}
void handle_focus_in(XEvent *e) {
  XFocusChangeEvent *ev = &e->xfocus;
  if (focused_window != None)
    XSetWindowBorder(display, focused_window, FOCUSED_BORDER_COLOR);
}
void handle_focus_out(XEvent *e) {
  XFocusChangeEvent *ev = &e->xfocus;
  if (focused_window ==
      None) // that happens when we are killing leaading to program crash
    return;
  else
    XSetWindowBorder(display, ev->window, BORDER_COLOR);
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
    focused_window = None; // Reset focused window
                           //
    tile_windows();
  }
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

  if (wa.override_redirect)
    return;

  XSelectInput(display, ev->window,
               StructureNotifyMask | EnterWindowMask | FocusChangeMask);
  XMapWindow(display, ev->window);
  // Set border width
  XSetWindowBorderWidth(display, ev->window, BORDER_WIDTH);
  // Set initial border color (unfocused)
  XSetWindowBorder(display, ev->window, BORDER_COLOR);
  clients->push_back({ev->window, wa.x, wa.y,
                      static_cast<unsigned int>(wa.width),
                      static_cast<unsigned int>(wa.height)});
  tile_windows();
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
  create_bar();
  update_bar();
  run();

  cleanup();
  XCloseDisplay(display);
  return 0;
}
void tile_windows() {
  unsigned int num_tiled_clients = 0;

  // First, count the number of non-floating (tiled) clients
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

  unsigned int tiled_index = 0; // To track the position of tiled clients
  for (unsigned int i = 0; i < clients->size(); ++i) {
    Client *c = &(*clients)[i];
    if (c->floating) {
      // Skip floating windows and store their indices for later raising
      continue;
    }

    if (tiled_index == 0) { // First window is the master
      // Master window positioning with border
      XMoveResizeWindow(
          display, c->window, GAP_SIZE, BAR_HEIGHT + GAP_SIZE, // Position with gaps and bar height
          master_width - 2 * (GAP_SIZE + BORDER_WIDTH), // Width adjusted for border and gaps
          screen_height - BAR_HEIGHT - 2 * (GAP_SIZE + BORDER_WIDTH) // Height adjusted for bar, border, and gaps
      );
    } else {
      // Stack windows positioning with border
      int stack_width = screen_width - master_width - 2 * (GAP_SIZE + BORDER_WIDTH);
      int stack_height = (screen_height - BAR_HEIGHT - GAP_SIZE - 2 * BORDER_WIDTH) / (num_tiled_clients - 1);
      int x = master_width + GAP_SIZE;
      int y = BAR_HEIGHT + GAP_SIZE + (tiled_index - 1) * (stack_height + GAP_SIZE);

      XMoveResizeWindow(display, c->window, x, y, // Position with gaps
                        stack_width,              // Width adjusted for border
                        stack_height              // Height adjusted for border
      );
    }

    ++tiled_index; // Only increment for non-floating windows

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

void exit_pwm(const Arg *arg) {
  XCloseDisplay(display);
  exit(0);
  void cleanup();
}
