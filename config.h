#pragma once // #include only once
#include "main.h"
#include <X11/X.h>
#include <X11/Xutil.h>
#include <vector>

// constatns
#define MOD Mod4Mask    // Usually the Windows key
#define SHIFT ShiftMask // Usually the shift key

static bool SHOW_BAR = true; // Whether to show the bar or not
static std::string BAR_FONT = "NotoMono Nerd Font:5";
static int BORDER_WIDTH = 1; // Width of the window border in pixels
static int  BAR_HEIGHT = 30;   // Height of the bar in pixels
static unsigned long BORDER_COLOR =
    0xd3d3d3; // gray color for borders (hex value)
static unsigned long FOCUSED_BORDER_COLOR =
    0x000000;                  // black color for focused window
static int GAP_SIZE = 2;       // Size of the gap around windows in pixels
static int BUTTONS_WIDTH = 30; // Width of the buttons in pixels

// all about workspaces
#define NUM_WORKSPACES 5 // Number of workspaces
static const std::string workspaces_names[NUM_WORKSPACES] = {
    "_", "", "", "", ""};
#define WORKSPACEKEYS(KEY, WORKSPACENUM)                                       \
  {MOD, KEY, switch_workspace, {.i = WORKSPACENUM}}, {                         \
    MOD | SHIFT, KEY, move_window_to_workspace, { .i = WORKSPACENUM }          \
  }
// commands
static const char *term[] = {"st", NULL};
static const char *s_shot[] = {"flameshot", "gui", NULL}; // for screenshots
                                                          //
// shortcuts
static std::vector<shortcut> shortcuts = {
    {MOD, XK_q, kill_focused_window, {0}},
    {MOD, XK_t, lunch, {.v = term}},
    {MOD, XK_Print, lunch, {.v = s_shot}},
    {MOD, XK_Return, toggle_fullscreen, {0}},
    {MOD, XK_b, toggle_bar, {0}},
    {MOD, XK_Right, resize_focused_window_x, {.i = +20}},
    {MOD, XK_Left, resize_focused_window_x, {.i = -20}},
    {MOD, XK_Down, resize_focused_window_y, {.i = +20}},
    {MOD, XK_Up, resize_focused_window_y, {.i = -20}},
    {MOD | ShiftMask, XK_q, exit_pwm, {0}},
    {MOD | ShiftMask, XK_space, toggle_floating, {0}},
    {MOD | SHIFT, XK_Right, move_focused_window_x, {.i = +20}},
    {MOD | SHIFT, XK_Left, move_focused_window_x, {.i = -20}},
    {MOD | SHIFT, XK_Down, move_focused_window_y, {.i = +20}},
    {MOD | SHIFT, XK_Up, move_focused_window_y, {.i = -20}},
    {MOD | SHIFT, XK_j, swap_window, {.i = +1}},
    {MOD | SHIFT, XK_k, swap_window, {.i = -1}},
    {MOD | SHIFT, XK_f, set_master, {0}},
    WORKSPACEKEYS(XK_1, 1),
    WORKSPACEKEYS(XK_2, 2),
    WORKSPACEKEYS(XK_3, 3),
    WORKSPACEKEYS(XK_4, 4),
    WORKSPACEKEYS(XK_5, 5),

};
