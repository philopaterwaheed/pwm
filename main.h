#pragma once

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <variant>
#include <vector>
#include <X11/X.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <cstdlib>
#include <fontconfig/fontconfig.h>
#include <iostream>
#include <string>
#include <unordered_map>

#define CLEANMASK(mask)                                                        \
  (mask & (ShiftMask | ControlMask | Mod1Mask | Mod2Mask | Mod3Mask |          \
           Mod4Mask | Mod5Mask))

static unsigned int numlockmask = 0;

struct Client {
  Window window; // the window id
  int x, y;
  unsigned int width, height;
  bool floating = false; // Indicates whether the window is floating or tiled
    bool is_fullscreen = false;     // Full-screen flag
};

// to pass arguments to the functions
union Arg {
  int i;
  void *v;
};
struct shortcut {
  unsigned int mask = 0;
  KeySym key;
  void (*func)(
      const Arg *
          arg); // https://stackoverflow.com/questions/2898316/using-a-member-function-pointer-within-a-class
                // what the shortcut would execute
                // https://www.reddit.com/r/learnrust/comments/1680nkq/whats_the_difference_between_mod_and_use/
  Arg arg;
};

struct Layout {
  std ::string name;
  void (*arrange)();
};
struct Workspace {
  std::vector<Client> clients = {};
    Window master = None;
  Layout layout;
};
/* union Button { */
/*   std::string name; */
/*   unsigned int id; */
/*   void (*func)(const Arg *arg); */
/*   Arg arg; */
/* }; */

Client *find_client(Window w);
int get_focused_window_index();
void tile_windows();
void update_status(XEvent *ev);
void warp_pointer_to_window(Window *win);
void restack_windows();
void update_bar() ;
void draw_text_with_dynamic_font(Display *display, Window window, XftDraw *draw,
                                 XftColor *color, const std::string &text,
                                 int x, int y, int screen) ;
void one_window() ;
// arg functions to invoke with shortcut
void resize_focused_window_x(const Arg *arg);
void resize_focused_window_y(const Arg *arg);
void move_focused_window_x(const Arg *arg);
void move_focused_window_y(const Arg *arg);
void kill_focused_window(const Arg *arg);
void exit_pwm(const Arg *arg);
void lunch(const Arg *arg);
void toggle_floating(const Arg *arg);
void swap_window(const Arg *arg);
void switch_workspace(const Arg *arg);
void move_window_to_workspace(const Arg *arg);
void toggle_fullscreen(const Arg *arg) ;
void set_master(const Arg *arg);
void toggle_bar(const Arg *arg) ;
// ///
// event handlers
void handle_focus_in(XEvent *e);
void handle_focus_out(XEvent *e);
void handle_enter_notify(XEvent *e);
void handle_map_request(XEvent *e);
void handle_configure_request(XEvent *e);
void handle_key_press(XEvent *e);
void handle_key_press(XEvent *e) ;
void handle_button_press_event(XEvent *e) ;
// font functions
void draw_text_with_dynamic_font(Display *display, Window window, XftDraw *draw,
                                 XftColor *color, const std::string &text,
                                 int x, int y, int screen);
int get_utf8_string_width(Display *display, XftFont *font,
                          const std::string &text) ;
XftFont *select_font_for_char(Display *display, FcChar32 ucs4, int screen) ;
