// for functions that take Arg as argument, the Arg struct is defined in
// args.cpp functions used in shortcuts
#include "config.h"
#include "main.h"
#include <X11/Xlib.h>

extern Display *display; // the connection to the X server
extern Window root; // the root window top level window all other windows are
                    // children of it and covers all the screen
extern Window focused_window;
extern std::vector<Workspace> *workspaces;
extern Workspace *current_workspace;
extern std::vector<Client> *clients;
extern std::vector<Monitor> monitors; // List of monitors
extern Monitor *current_monitor;
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
    // if one of them is the master and we didn't ccheck the master will not
    // change postions
    if ((*clients)[index2].window == current_workspace->master) {
      current_workspace->master = (*clients)[index1].window;
    } else if ((*clients)[index1].window == current_workspace->master)
      current_workspace->master = (*clients)[index2].window;

    std::swap((*clients)[index1], (*clients)[index2]);
    /* warp_pointer_to_window(focused_window); */
    tile_windows(); // Rearrange windows after swapping
    warp_pointer_to_window(&(*clients)[index2].window);
  }
}
void kill_focused_window(const Arg *arg) {
  (void)arg;

  if (focused_window != None) {
    // Remove the focused window from the client list
    clients->erase(std::remove_if(clients->begin(), clients->end(),
                                  [](const Client &c) {
                                    return c.window == focused_window;
                                  }),
                   clients->end());

    XUnmapWindow(display, focused_window);
    clientmsg(focused_window, XInternAtom(display, "WM_DELETE_WINDOW", False),
              CurrentTime, 2, 0, 0, 0);

    // Update master window if needed
    if (focused_window == current_workspace->master) {
      current_workspace->master = None;
      if (!clients->empty()) {
        current_workspace->master = (*clients)[0].window;
      }
    }

    focused_window = None;
    tile_windows();
  }
}
void set_master(const Arg *arg) {
  if (focused_window != None) {
    current_workspace->master = focused_window;
    tile_windows();
    warp_pointer_to_window(&focused_window);
  }
}
void switch_workspace(const Arg *arg) {
  int new_workspace = arg->i - 1;
  if (new_workspace == current_workspace->index ||
      new_workspace > NUM_WORKSPACES || new_workspace < 0)
    return;

  // Unmap windows from the current workspace
  for (auto &client : *clients) {
    XUnmapWindow(display, client.window);
  }

  current_workspace = &(*workspaces)[new_workspace];
  current_monitor->at = new_workspace;

  // Map windows from the new workspace
  for (auto &client : current_workspace->clients) {
    XMapWindow(display, client.window);
  }

  clients = &current_workspace->clients;
  // Re-tile the windows in the new workspace
  tile_windows();
  update_bar();
}
void move_window_to_workspace(const Arg *arg) {
  int target_workspace = arg->i - 1;
  if (focused_window == None || target_workspace == current_workspace->index ||
      target_workspace < 0 || target_workspace > NUM_WORKSPACES) {
    return; // No window to move or target workspace is the same as current
  }

  // Remove window from the current workspace
  auto &current_clients = current_workspace->clients;
  auto it = std::remove_if(
      current_clients.begin(), current_clients.end(),
      [=](Client &client) { return client.window == focused_window; });
  if (it != current_clients.end()) {
    current_clients.erase(it, current_clients.end());
  }

  // Add window to the target workspace
  XWindowAttributes wa;
  XGetWindowAttributes(display, focused_window, &wa);

  (*workspaces)[target_workspace].clients.push_back(
      {focused_window, wa.x, wa.y, static_cast<unsigned int>(wa.width),
       static_cast<unsigned int>(wa.height), false});
  XUnmapWindow(display, focused_window);
  if (focused_window == current_workspace->master) {
    current_workspace->master = None;
    if (current_clients.size() > 0) {
      current_workspace->master = current_clients[0].window;
    }
  }
  focused_window = None;
  tile_windows();
}
void toggle_floating(const Arg *arg) {
  if (focused_window == None)
    return;

  Client *client = find_client(focused_window);
  const int border_width = 2; // Border width in pixels

  if (client) {
    // Toggle the floating status of the focused window
    client->floating = !client->floating;
    if (client->window == current_workspace->master) {
      current_workspace->master = None;
      if (clients->size() > 0) {
        current_workspace->master = (*clients)[0].window;
      }
    }
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
  current_workspace = &(*workspaces)[current_monitor->at];
  current_workspace->show_bar = !current_workspace->show_bar;
  /* std::swap(bar_height_place_holder, bar_hight); */
  if (current_workspace->show_bar) {
    std::swap(current_workspace->bar_height,
              current_workspace->bar_height_place_holder);
    XMapWindow(display, current_monitor->bar);
  XSelectInput(display, root,
               SubstructureRedirectMask | SubstructureNotifyMask |
                   KeyPressMask | ExposureMask | PropertyChangeMask |
                   MotionNotify | SubstructureRedirectMask |
                   SubstructureNotifyMask | ButtonPressMask |
                   PointerMotionMask | EnterWindowMask | LeaveWindowMask |
                   StructureNotifyMask | PropertyChangeMask);
    update_bar();
  } else {
    std::swap(current_workspace->bar_height,
              current_workspace->bar_height_place_holder);
    XUnmapWindow(display, current_monitor->bar);
  XSelectInput(display, root,
               SubstructureRedirectMask | SubstructureNotifyMask |
                   KeyPressMask | ExposureMask | PropertyChangeMask |
                   MotionNotify | SubstructureRedirectMask |
                   SubstructureNotifyMask | ButtonPressMask |
                   PointerMotionMask | EnterWindowMask | LeaveWindowMask |
                   StructureNotifyMask);
  }
  tile_windows();
}
void toggle_fullscreen(const Arg *arg) {
  auto client = find_client(focused_window);
  if (!client)
    return;

  if (!client->is_fullscreen) {
    make_fullscreen(client);
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
void exit_pwm(const Arg *arg) {
  XCloseDisplay(display);
  cleanup();
  exit(0);
}
int sendevent(Window window, Atom proto) {
  XEvent ev;
  Atom wm_protocols = XInternAtom(display, "WM_PROTOCOLS", True);
  Atom *protocols;
  int n;
  int exists = 0;

  if (XGetWMProtocols(display, window, &protocols, &n)) {
    for (int i = 0; i < n; ++i) {
      if (protocols[i] == proto) {
        exists = 1;
        break;
      }
    }
    XFree(protocols);
  }

  if (exists) {
    ev.type = ClientMessage;
    ev.xclient.window = window;
    ev.xclient.message_type = wm_protocols;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = proto;
    ev.xclient.data.l[1] = CurrentTime;
    XSendEvent(display, window, False, NoEventMask, &ev);
  }

  return exists;
}
static void clientmsg(Window win, Atom atom, unsigned long d0, unsigned long d1,
                      unsigned long d2, unsigned long d3, unsigned long d4) {
  XEvent ev;
  long mask = SubstructureRedirectMask | SubstructureNotifyMask;

  ev.xclient.type = ClientMessage;
  ev.xclient.serial = 0;
  ev.xclient.send_event = True;
  ev.xclient.message_type = atom;
  ev.xclient.window = win;
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = d0;
  ev.xclient.data.l[1] = d1;
  ev.xclient.data.l[2] = d2;
  ev.xclient.data.l[3] = d3;
  ev.xclient.data.l[4] = d4;
  if (!XSendEvent(display, root, False, mask, &ev)) {
    errx(1, "could not send event");
  }
}
void focus_next_monitor(const Arg *arg) {
  if (monitors.empty())
    return;

  // Move to the previous monitor
  unsigned int focused_monitor_index = find_monitor_index(current_monitor);
  unsigned int new_index = focused_monitor_index + 1;
  if (focused_monitor_index >= monitors.size()) {
    new_index = 0;
  }
  Monitor *monitor = &monitors[new_index];
  XWarpPointer(display, None, None, 0, 0, 0, 0, monitor->x, monitor->y);
}

void focus_previous_monitor(const Arg *arg) {
  if (monitors.empty())
    return;

  // Move to the previous monitor
  unsigned int focused_monitor_index = find_monitor_index(current_monitor);
  unsigned int new_index = focused_monitor_index - 1;
  if (focused_monitor_index < 0) {
    new_index = monitors.size() - 1;
  }
  Monitor *monitor = &monitors[new_index];
  XWarpPointer(display, None, None, 0, 0, 0, 0, monitor->width / 2,
               monitor->height / 2);
}
