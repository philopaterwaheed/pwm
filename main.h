#pragma once

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <algorithm>
#include <any>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <variant>
#include <vector>
#include <initializer_list>

#define MOD Mod4Mask    // Usually the Windows key
#define SHIFT ShiftMask // Usually the Windows key

struct Client {
  Window window; // the window id
  int x, y;
  unsigned int width, height;
};

// to pass arguments to the functions
union Arg {
    public:
    int i;
    void *v;
};
struct shortcut {
  int mask = MOD;
  KeySym key; 
  void (*func)(void *); // https://stackoverflow.com/questions/2898316/using-a-member-function-pointer-within-a-class
            // what the shortcut would execute
            // https://www.reddit.com/r/learnrust/comments/1680nkq/whats_the_difference_between_mod_and_use/
  Arg arg;
};

Client *find_client(Window w);
void handle_focus_in(XEvent *e);
void kill_focused_window();
void handle_enter_notify(XEvent *e);
void handle_map_request(XEvent *e);
void handle_configure_request(XEvent *e);
void resize_focused_window_x(void* arg);
void resize_focused_window_y(void* arg);
void lunch(void * arg);
void handle_key_press(XEvent *e);
