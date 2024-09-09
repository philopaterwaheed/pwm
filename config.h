#pragma once // #include only once
#include "main.h"
#include <X11/X.h>
#include <X11/Xutil.h>
#include <vector>

//constatns

const int BORDER_WIDTH = 1; // Width of the window border in pixels
const unsigned long BORDER_COLOR = 0xd3d3d3; // gray color for borders (hex value)
const unsigned long FOCUSED_BORDER_COLOR = 0x000000; // black color for focused window
const int GAP_SIZE = 1; // Size of the gap around windows in pixels

// commands
static const char *term[] = {"st", NULL};
static const char *s_shot[] = {"flameshot", "gui", NULL}; // for screenshots
							  //
// shortcuts
static std::vector<shortcut> shortcuts = {
    {MOD, XK_q, kill_focused_window, {0}},
    {MOD, XK_t, lunch, {.v = term}},
    {MOD, XK_Print, lunch, {.v = s_shot}},
    {MOD | ShiftMask, XK_q, exit_pwm, {0}},
    {MOD | ShiftMask, XK_space, toggle_floating, {0}},
    {MOD, XK_Right, resize_focused_window_x, {.i = +20}},
    {MOD, XK_Left, resize_focused_window_x, {.i = -20}},
    {MOD, XK_Down, resize_focused_window_y, {.i = +20}},
    {MOD, XK_Up, resize_focused_window_y, {.i = -20}},
    {MOD | SHIFT, XK_Right, move_focused_window_x, {.i = +20}},
    {MOD | SHIFT, XK_Left, move_focused_window_x, {.i = -20}},
    {MOD | SHIFT, XK_Down, move_focused_window_y, {.i = +20}},
    {MOD | SHIFT, XK_Up, move_focused_window_y, {.i = -20}},
    {MOD | SHIFT, XK_j, swap_window, {.i = +1}},
    {MOD | SHIFT, XK_k, swap_window, {.i = -1}},

};
