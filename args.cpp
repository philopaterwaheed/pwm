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
extern std::vector<Client> *sticky;
extern Cursor cursors[3];

void resize_focused_window_y(const Arg *arg) {
  Client *c = find_client(focused_window);
  if (focused_window != None && focused_window != root && c->floating &&
      !c->fullscreen) {
    XWindowChanges changes;

    // Get the current window attributes to calculate the new size
    XWindowAttributes wa;
    XGetWindowAttributes(display, focused_window, &wa);

    // Adjust width and height based on delta
    changes.height = wa.height + arg->i;
    c->height = wa.height + arg->i;

    // Prev the window from being resized too small
    if (changes.height < 100)
      changes.height = 100;

    // Apply the new size to the window
    XConfigureWindow(display, focused_window, CWHeight, &changes);
    movement_warp(&focused_window);
  }
}
void resize_focused_window_x(const Arg *arg) {
  Client *c = find_client(focused_window);
  if (focused_window != None && focused_window != root && c->floating &&
      !c->fullscreen) {
    XWindowChanges changes;

    // Get the current window attributes to calculate the new size
    XWindowAttributes wa;
    XGetWindowAttributes(display, focused_window, &wa);

    // Adjust width and height based on delta
    changes.width = wa.width + arg->i;
    c->width = wa.width + arg->i;

    // Prev the window from being resized too small
    if (changes.width < 100)
      changes.width = 100;

    // Apply the new size to the window
    XConfigureWindow(display, focused_window, CWWidth, &changes);
    movement_warp(&focused_window);
  }
}

void move_focused_window_x(const Arg *arg) {
  Client *c = find_client(focused_window);
  if (focused_window != None && focused_window != root && c->floating &&
      !c->fullscreen) {
    XWindowChanges changes;
    XWindowAttributes wa;
    XSetInputFocus(display, focused_window, RevertToPointerRoot, CurrentTime);
    XGetWindowAttributes(display, focused_window, &wa);

    Window dummy =
        focused_window; // for if the window changed focus after moving
    changes.x = wa.x + arg->i;
    c->x = wa.x + arg->i;
    XConfigureWindow(display, focused_window, CWX, &changes);
    XConfigureWindow(display, dummy, CWX, &changes);
    movement_warp(&dummy);
  }
}
void move_focused_window_y(const Arg *arg) {
  Client *c = find_client(focused_window);
  if (focused_window != None && focused_window != root && c->floating &&
      !c->fullscreen) {
    XWindowChanges changes;
    XWindowAttributes wa;
    XSetInputFocus(display, focused_window, RevertToPointerRoot, CurrentTime);
    XGetWindowAttributes(display, focused_window, &wa);
    Window dummy =
        focused_window; // for if the window changed focus after moving
    changes.y = wa.y + arg->i;
    c->y = wa.y + arg->i;
    XConfigureWindow(display, focused_window, CWY, &changes);
    XConfigureWindow(display, dummy, CWY, &changes);
    movement_warp(&dummy);
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
    movement_warp(&focused_window);
    arrange_windows(); // Rearrange windows after swapping
  }
}
void kill_focused_window(const Arg *arg) {
  (void)arg;

  if (focused_window != None) {
    // Remove the focused window from the client list
    if (!sendevent(focused_window,
                   XInternAtom(display, "WM_DELETE_WINDOW", False))) {

      XGrabServer(display);
      XSetCloseDownMode(display, DestroyAll);
      XKillClient(display, focused_window);
      XSync(display, False);
      XUngrabServer(display);
      clients->erase(std::remove_if(clients->begin(), clients->end(),
                                    [](const Client &c) {
                                      return c.window == focused_window;
                                    }),
                     clients->end());
      sticky->erase(std::remove_if(sticky->begin(), sticky->end(),
                                   [](const Client &c) {
                                     return c.window == focused_window;
                                   }),
                    clients->end());
      // Update master window if needed
      if (focused_window == current_workspace->master) {
        current_workspace->master = None;
      }

      focused_window = None;
    }
  }
  arrange_windows();
}
void set_master(const Arg *arg) {
  if (focused_window != None) {
    current_workspace->master = focused_window;
    arrange_windows();
    /* warp_pointer_to_window(&focused_window); */
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
  // Re-arrange_windows the windows in the new workspace
  arrange_windows();
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
  if (it != current_clients.end() && !it->sticky) { // prevent sticky windows

    current_clients.erase(it, current_clients.end());

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
    arrange_windows();
  }
}
void toggle_floating(const Arg *arg) {
  if (focused_window == None)
    return;

  Client *client = find_client(focused_window);

  if (client && !client->fullscreen && !client->sticky) {
    // furcing sticky windows to be floating
    //  Toggle the floating status of the focused window
    client->floating = !client->floating;
    if (client->window == current_workspace->master) {
      current_workspace->master = None;
    }
    if (client->floating) { // if we are no resizing the window
      // center the window
      if (arg->i != 21) {
        client->x = current_monitor->x + (current_monitor->width) / 4;
        client->y = current_monitor->y + (current_monitor->height) / 4;
        client->width = current_monitor->width / 2;
        client->height = current_monitor->height / 2;

        XMoveResizeWindow(display, client->window, client->x, client->y,
                          client->width, client->height);
      }
    }
    // and set borders
    XSetWindowBorderWidth(
        display, focused_window,
        BORDER_WIDTH); // Set border width for floating windows */

    // Ensure the floating window is raised above other windows
    XRaiseWindow(display, focused_window);
    // Rearrange windows after toggling floating
    arrange_windows();

    (arg->i != 21) ? movement_warp(&focused_window) : void();
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
                     /* MotionNotify | */ SubstructureRedirectMask |
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
                     /* MotionNotify | */ SubstructureRedirectMask |
                     SubstructureNotifyMask | ButtonPressMask |
                     PointerMotionMask | EnterWindowMask | LeaveWindowMask |
                     StructureNotifyMask);
  }
  arrange_windows();
}
void toggle_fullscreen(const Arg *arg) {
  // todo fix the fullscreen on proebrty change
  auto client = find_client(focused_window);
  if (!client)
    return;

  if (!client->fullscreen) {
    client->fullscreen = true;
    make_fullscreen(client, current_monitor->width, current_monitor->height);
  } else {
    // Exit full-screen and restore the original size and position
    XMoveResizeWindow(display, client->window, client->x, client->y,
                      client->width, client->height);

    // Restore the window border
    XSetWindowBorderWidth(display, client->window, BORDER_WIDTH);

    client->fullscreen = false;
  }
}
/* void lunch(const Arg *arg) { */
/*   char **args = (char **)arg->v; */
/*   struct sigaction sa; */
/**/
/*   if (fork() == 0) { */
/*     if (display) */
/*       close(ConnectionNumber( */
/*           display)); // if the display is open, close it */
/*                      // The close(ConnectionNumber(display)) call closes the
 * file
 */
/*                      // descriptor associated with the display connection in
 * the */
/*                      // child process. This is necessary because the child */
/*                      // process doesn't need to maintain the X connection—it
 */
/*                      // will execute a new program. */
/*     setsid(); */
/*     //    function call creates a new session and sets the child process as
 * the */
/*     //    session leader. This detaches the child process from the terminal
 * (if */
/*     //    anew_y) and makes it independent of the parent process group. */
/*     sigemptyset(&sa.sa_mask); */
/*     sa.sa_flags = 0; */
/*     sa.sa_handler = SIG_DFL; */
/*     sigaction(SIGCHLD, &sa, NULL); */
/*     execvp(args[0], args); */
/*     // The execvp() function replaces the current process image with a new */
/*     // process */
/*     exit(1); */
/*   } */
/* } */
void lunch(const Arg *arg) {
  struct sigaction sa;

  if (fork() == 0) {
    if (display)
      close(ConnectionNumber(display));
    setsid();

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = SIG_DFL;
    sigaction(SIGCHLD, &sa, NULL);

    execvp(((char **)arg->v)[0], (char **)arg->v);
    exit(1);
  }
}
void exit_pwm(const Arg *arg) {
  XCloseDisplay(display);
  cleanup();
  exit(0);
}
int sendevent(Window window, Atom proto) {
  // borrowed from dwm like a lot of stuff :)
  XEvent ev;
  Atom wm_protocols = XInternAtom(display, "WM_PROTOCOLS", false);
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
    errx(1, "could not send ev");
  }
  // from https://github.com/phillbush/shod
  // thanks the only message that helped me in killing windows
}
void change_layout(const Arg *arg) {
  if (arg->i < 0 || arg->i >= NUM_LAYOUTS)
    return;
  current_workspace->layout = arg->i;
  arrange_windows();
  update_bar();
}

void focus_next_monitor(const Arg *arg) {
  if (monitors.empty())
    return;

  // Move to the next monitor
  unsigned int focused_monitor_index = find_monitor_index(current_monitor);
  unsigned int new_index = (focused_monitor_index + 1) % monitors.size();

  Monitor *monitor = &monitors[new_index];
  XClearWindow(display, current_monitor->bar);
  focus_monitor(monitor);
}

void focus_previous_monitor(const Arg *arg) {
  if (monitors.empty())
    return;

  // Move to the previous monitor
  unsigned int focused_monitor_index = find_monitor_index(current_monitor);
  unsigned int new_index = (focused_monitor_index == 0)
                               ? monitors.size() - 1
                               : focused_monitor_index - 1;

  Monitor *monitor = &monitors[new_index];
  XClearWindow(display, current_monitor->bar);
  focus_monitor(monitor);
}

void change_focused_window_cfact(const Arg *arg) {
  Client *client = find_client(focused_window);
  float temp = client->cfact;
  client->cfact += arg->f;
  if (client->cfact < 0.1)
    client->cfact = 0.1;
  if (client->cfact > 1)
    client->cfact = 1;
  if (temp != client->cfact) // it will be an overhead if arrang every time even
                             // if did't change
    arrange_windows();
}
void change_master_width(const Arg *arg) {
  float temp = current_workspace->master_persent;
  current_workspace->master_persent += arg->f;
  if (current_workspace->master_persent < 0.1)
    current_workspace->master_persent = 0.1;
  if (current_workspace->master_persent > 0.9)
    current_workspace->master_persent = 0.9;
  if (temp != current_workspace->master_persent)
    arrange_windows();
}
void movemouse(const Arg *arg) {
  int x, y, ocx, ocy, new_x, new_y;
  XEvent ev;
  Time lasttime = 0;
  Client *client = find_client(focused_window);
  if (!client || client->fullscreen ||
      current_workspace->layout ==
          1) // no support for resizing fullscreen windows by mouse or moncole
    return;
  if (!client->floating)
    toggle_floating(arg);

  ocx = client->x, ocy = client->y;
  if (XGrabPointer(display, root, False, MOUSEMASK, GrabModeAsync,
                   GrabModeAsync, None, cursors[CurMove],
                   CurrentTime) != GrabSuccess)
    return;
  if (!getrootptr(&x, &y))
    return;
  do {
    XMaskEvent(display, MOUSEMASK | ExposureMask | SubstructureRedirectMask,
               &ev);
    switch (ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      handle_map_request(&ev);
      break;
    case MotionNotify:
      if ((ev.xmotion.time - lasttime) <= (1000 / 60))
        continue;
      lasttime = ev.xmotion.time;

      new_x = ocx + (ev.xmotion.x - x);
      new_y = ocy + (ev.xmotion.y - y);
      if (abs(current_monitor->x - new_x) < snap)
        new_x = current_monitor->x;
      else if (((current_monitor->x + current_monitor->width) -
                (new_x + WIDTH(client))) < snap)
        new_x = current_monitor->x + current_monitor->width - WIDTH(client);
      if (abs(current_monitor->y + current_workspace->bar_height - new_y) <
          snap)
        new_y = current_monitor->y + current_workspace->bar_height;
      else if (((current_monitor->y + current_monitor->height) -
                (new_y + HEIGHT(client))) < snap)
        new_y = current_monitor->y + current_monitor->height - HEIGHT(client);
      /* if (!c->isfloating && selmon->lt[selmon->sellt]->arrange && */
      /*     (abs(new_x - c->x) > snap || abs(new_y - c->y) > snap)) */
      /*   togglefloating(NULL); */
      /* if (!selmon->lt[selmon->sellt]->arrange || c->isfloating) */

      client->x = new_x;
      client->y = new_y;
      XMoveResizeWindow(display, client->window, new_x, new_y, client->width,
                        client->height);
      break;
    }
  } while (ev.type != ButtonRelease);
  XUngrabPointer(display, CurrentTime);
  /* if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) { */
  /*   sendmon(c, m); */
  /*   selmon = m; */
  /*   focus(NULL); */
  /* } */
  // code for monitors check it
}
void resizemouse(const Arg *arg) {
  int originalClientX, originalClientY, newWidth, newHeight;
  Client *client = find_client(focused_window);
  XEvent ev;
  Time lastMotionTime = 0;

  if (!client || client->fullscreen ||
      current_workspace->layout ==
          1) // no support for resizing fullscreen windows by mouse or moncole
    return;
  if (client->fullscreen ||
      current_workspace->layout ==
          1) // no support for resizing fullscreen windows by mouse or moncole

    return;

  originalClientX = client->x;
  originalClientY = client->y;

  if (XGrabPointer(display, root, False, MOUSEMASK, GrabModeAsync,
                   GrabModeAsync, None, cursors[CurResize],
                   CurrentTime) != GrabSuccess)
    return;

  if (arg->i != 1)
    XWarpPointer(display, None, client->window, 0, 0, 0, 0,
                 client->width + BORDER_WIDTH - 1,
                 client->height + BORDER_WIDTH - 1);

  do {
    XMaskEvent(display, MOUSEMASK | ExposureMask | SubstructureRedirectMask,
               &ev);
    switch (ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      handle_map_request(&ev);
      break;

    case MotionNotify:
      if ((ev.xmotion.time - lastMotionTime) <= (1000 / 60))
        continue;
      lastMotionTime = ev.xmotion.time;

      newWidth =
          std::max(ev.xmotion.x - originalClientX - 2 * BORDER_WIDTH + 1, 1);
      newHeight =
          std::max(ev.xmotion.y - originalClientY - 2 * BORDER_WIDTH + 1, 1);

      if (current_monitor->x + newWidth >= current_monitor->x &&
          current_monitor->x + newWidth <=
              current_monitor->x + current_monitor->width &&
          current_monitor->y + newHeight >= current_monitor->y &&
          current_monitor->y + newHeight <=
              current_monitor->y + current_monitor->height) {

        if (!client->floating && ((newWidth - client->width) > snap ||
                                  (newHeight - client->height) > snap)) {
          toggle_floating(arg);
        }
      }

      if (client->floating)
        XMoveResizeWindow(display, client->window, client->x, client->y,
                          newWidth, newHeight);
      break;
    }
  } while (ev.type != ButtonRelease);
  client->width = newWidth;
  client->height = newHeight;

  XWarpPointer(display, None, client->window, 0, 0, 0, 0,
               client->width + BORDER_WIDTH - 1,
               client->height + BORDER_WIDTH - 1);

  XUngrabPointer(display, CurrentTime);

  while (XCheckMaskEvent(display, EnterWindowMask, &ev))
    ;
  /* if ((activeMonitor = recttomon(client->x, client->y, client->w, client->h))
   * != */
  /*     selmon) { */
  /*   sendmon(client, activeMonitor); */
  /*   selmon = activeMonitor; */
  /*   focus(NULL); */
  /* } */
}
void toggle_sticky(const Arg *arg) {

  if (focused_window == None)
    return;
  Client *c = find_client(focused_window);
  if (c) {
    Client client = *c;
    if (client.sticky) {
      auto it = std::remove_if(sticky->begin(), sticky->end(), [&](Client &c) {
        return c.window == focused_window;
      });
      client.sticky = false;
      client.floating = false;
      current_workspace->clients.push_back(client);
      if (it != sticky->end())
        sticky->erase(it);
      /* XUnmapWindow(display, focused_window); */
    } else {
      auto it =
          std::remove_if(clients->begin(), clients->end(),
                         [&](Client &c) { return c.window == focused_window; });
      if (!client.floating) { // if we are no resizing the window
                              // center the window
        client.floating = true;
        client.x = current_monitor->x + (current_monitor->width) / 4;
        client.y = current_monitor->y + (current_monitor->height) / 4;
        client.width = current_monitor->width / 2;
        client.height = current_monitor->height / 2;

        XMoveResizeWindow(display, client.window, client.x, client.y,
                          client.width, client.height);
      }
      client.sticky = true;
      // and set borders
      XSetWindowBorderWidth(
          display, focused_window,
          BORDER_WIDTH); // Set border width for floating windows */

      // Ensure the floating window is raised above other windows
      XRaiseWindow(display, focused_window);
      // Rearrange windows after toggling floating

      movement_warp(&focused_window);
      if (current_workspace->master == focused_window)
        current_workspace->master = None;
      sticky->push_back(client);
      if (it != clients->end())
        clients->erase(it);
    }
    arrange_windows();
  }
}
