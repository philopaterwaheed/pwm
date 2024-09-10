//for functions that take Arg as argument, the Arg struct is defined in args.cpp
//functions used in shortcuts 
#include "main.h"
#include "config.h"

extern Display *display; // the connection to the X server
extern Window root; // the root window top level window all other windows are
                    // children of it and covers all the screen
extern Window focused_window, bar_window;
extern std::vector<Workspace> workspaces;

extern short current_workspace;
extern int bar_hight_place_holder;

extern std::vector<Client> *clients;
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
