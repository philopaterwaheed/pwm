
#include "main.h"
#include "config.h"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <cstdlib>

Display *display; // the connection to the X server
Window root; // the root window top level window all other windows are children
             // of it and covers all the screen
Window focused_window = None, master_window = None ;

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

/* void handle_focus_in(XEvent *e) { */
/*   XFocusChangeEvent *ev = &e->xfocus; */
/*   focused_window = ev->window; // Set the focused window */
/* } */

void kill_focused_window(const Arg *arg) {
  (void)arg;
  if (focused_window != None) {
    clients.erase(std::remove_if(clients.begin(), clients.end(),
                                 [](const Client &c) {
                                   return c.window == focused_window;
                                 }),
                  clients.end());
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

  XSelectInput(display, ev->window, StructureNotifyMask | EnterWindowMask);
  XMapWindow(display, ev->window);

  clients.push_back({ev->window, wa.x, wa.y,
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
    warp_pointer_to_window(focused_window);
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
    warp_pointer_to_window(focused_window);
  }
}

void move_focused_window_x(const Arg *arg) {
  if (focused_window != None && focused_window != root) {
    XWindowChanges changes;
    XWindowAttributes wa;
    XSetInputFocus(display, focused_window, RevertToPointerRoot, CurrentTime);
    XGetWindowAttributes(display, focused_window, &wa);

    changes.x = wa.x + arg->i;
    XConfigureWindow(display, focused_window, CWX, &changes);
  }
}
void move_focused_window_y(const Arg *arg) {
  if (focused_window != None && focused_window != root) {
    XWindowChanges changes;
    XWindowAttributes wa;
    XSetInputFocus(display, focused_window, RevertToPointerRoot, CurrentTime);
    XGetWindowAttributes(display, focused_window, &wa);

    changes.y = wa.y + arg->i;
    XConfigureWindow(display, focused_window, CWY, &changes);
  }
}
void swap_window(const Arg *arg) {
  int index1 = get_focused_window_index(), index2 = index1 + arg->i;
  index2 = index2 < 0 ? clients.size() - 1 : index2;
  index2 = index2 >= clients.size() ? 0 : index2;
  if (index1 < clients.size() && index2 < clients.size()) {
    std::swap(clients[index1], clients[index2]);
    /* warp_pointer_to_window(focused_window); */
    tile_windows(); // Rearrange windows after swapping
  }
}

// Get the index of the focused window
int get_focused_window_index() {
  for (unsigned int i = 0; i < clients.size(); ++i) {
    if (clients[i].window == focused_window) {
      return i;
    }
  }
  return -1; // Not found
}

void warp_pointer_to_window(Window win) {
  XWindowAttributes wa;
  XGetWindowAttributes(display, win, &wa);
  int x =  wa.width / 2;
  int y =  wa.height / 2;
  XWarpPointer(display, None, win, 0, 0, 0, 0, x, y);
}
void restack_windows() {
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
    /* case FocusIn: */
    /*   handle_focus_in(&ev); */
    /*   break; */
    case EnterNotify:
      handle_enter_notify(&ev);
      break;
    case KeyPress:
      handle_key_press(&ev);
      break;
    case PropertyNotify:
      break;
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
  run();

  XCloseDisplay(display);
  return 0;
}
void tile_windows() {
  unsigned int num_tiled_clients = 0;
  std::vector<int>
      floating_clients; // for the index of floatings not  to do it twice

  for (auto &client : clients) {
    if (!client.floating) {
      ++num_tiled_clients;
    }
  }

  if (num_tiled_clients == 0)
    return;

  int screen_width = DisplayWidth(display, DefaultScreen(display));
  int screen_height = DisplayHeight(display, DefaultScreen(display));

  int master_width = screen_width * 0.6;

  unsigned int tiled_index = 0;
  for (unsigned int i = 0; i < clients.size(); ++i) {
    Client *c = &clients[i];
    if (c->floating) {
      // Skip floating windows
      floating_clients.push_back(i);
      continue;
    }

    if (tiled_index == 0) { // make a master here
      // Master window
      XMoveResizeWindow(display, c->window, 0, 0, master_width, screen_height);
    } else {
      // Stack windows
      int stack_height = screen_height / (num_tiled_clients - 1);
      XMoveResizeWindow(display, c->window, master_width,
                        (tiled_index - 1) * stack_height,
                        screen_width - master_width, stack_height);
    }
    ++tiled_index;
  }
  for (unsigned int i = 0; i < floating_clients.size(); ++i) {
    if (clients[floating_clients[i]].floating) {
      XRaiseWindow(display, clients[floating_clients[i]].window);
    }
  }
}

void toggle_floating(const Arg *arg) {
  if (focused_window == None)
    return;

  Client *client = find_client(focused_window);
  if (client) {
    client->floating = !client->floating; // Toggle floating
    if (client->floating) {
      XWindowAttributes wa;
      XGetWindowAttributes(display, focused_window, &wa);
      client->x = wa.x;
      client->y = wa.y;
      client->width = wa.width;
      client->height = wa.height;
      XMoveResizeWindow(display, focused_window, 100, 100, 800,
                        600); // not for future me make rule in this
      XRaiseWindow(display, focused_window);
    }
    tile_windows(); // Rearrange windows
  }
}

void exit_pwm(const Arg *arg) {
  free(display);
  exit(0);
}
