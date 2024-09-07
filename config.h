#pragma once // #include only once
#include "main.h"
#include <X11/X.h>
#include <vector>

// commands
static const char *term[] = {"st", NULL};
static const char *s_shot[] = {"flameshot", "gui", NULL}; // for screenshots
static std::vector<shortcut> shortcuts = {
    {MOD, XK_q, kill_focused_window, {0}},
    {MOD, XK_t, lunch, {.v = term}},
    {MOD, XK_Print, lunch, {.v = s_shot}},
    {MOD | ShiftMask, XK_q, exit_pwm, {0}},
    {MOD, XK_Right, resize_focused_window_x, {.i = +20}},
    {MOD, XK_Left, resize_focused_window_x, {.i = -20}},
    {MOD, XK_Down, resize_focused_window_y, {.i = +20}},
    {MOD, XK_Up, resize_focused_window_y, {.i = -20}},
    {MOD | SHIFT, XK_Right, move_focused_window_x, {.i = +20}},
    {MOD | SHIFT, XK_Left, move_focused_window_x, {.i = -20}},
    {MOD | SHIFT, XK_Down, move_focused_window_y, {.i = +20}},
    {MOD | SHIFT, XK_Up, move_focused_window_y, {.i = -20}},

};
