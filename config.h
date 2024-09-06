#pragma once // #include only once
#include "main.h"

//commands 
const char *term[] = {"st", NULL};
shortcut shortcuts[] = {
    {MOD, XK_q, kill_focused_window,  0},
    {MOD, XK_t, lunch, {.v= term} },
    {MOD|SHIFT, XK_q, kill_focused_window, void(0)}
    {MOD , XK_Right , resize_focused_window_x, {.i = 20}},
    {MOD , XK_Left , resize_focused_window_x, {.i = -20}},
    {MOD , XK_Up , resize_focused_window_y, {.i =  -20}},
    {MOD , XK_Down , resize_focused_window_y, {.i =  20}},

};
