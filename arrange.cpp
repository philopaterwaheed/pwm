// for the window arrangement functions and theier helper functions
//
#include "config.h"
#include "main.h"

// vectors to control and collect the windows
extern std::vector<Workspace> *workspaces;
extern std::vector<Client> *clients; //  the clients are stored in a vector
                                     //  assosiated with the current workspace
extern std::vector<Client> *sticky;

// to determine the current monitor and workspace
extern Monitor *current_monitor; // The monitor in focus
extern Workspace *current_workspace;

// x server variables
extern Display *display; // the connection to the X server
extern int screen;
extern Window
    root; // the root window top level window all other windows are children

void arrange_windows() {
  std::vector<Client *> fullscreen_clients;
  std::vector<Client *> arranged_clients;
  XWindowAttributes att;
  // First, count the number of non-floating (arranged) clients and store the
  // them

  current_workspace->cfacts = 0;
  for (auto &client : *clients) {
    if (XGetWindowAttributes(display, client.window, &att) == 0) {
      if (client.window == current_workspace->master)
        current_workspace->master = None;
      continue;
    }
    if (!client.floating) {
      if (client.window != current_workspace->master)
        current_workspace->cfacts += client.cfact;
      arranged_clients.push_back(&client);
    }
    if (client.fullscreen) {
      fullscreen_clients.push_back(&client);
    }
  }
  int num_arranged_clients = arranged_clients.size();

  // If there are no arranged_clients clients, we don't need to arrange anything
  if (num_arranged_clients == 0)
    return;

  if (current_workspace->master == None && !arranged_clients.empty()) {
    current_workspace->master = arranged_clients[0]->window;
    current_workspace->cfacts -= arranged_clients[0]->cfact;
  }
  int screen_width =
      current_monitor->width; // DisplayWidth(display, DefaultScreen(display));
  int screen_height =
      current_monitor
          ->height; // DisplayHeight(display, DefaultScreen(display));
  // Calculate master width (60% of screen width)
  int master_width = screen_width * current_workspace->master_persent;
  LAYOUTS[current_workspace->layout].arrange(&arranged_clients, master_width,
                                             screen_height, screen_width);
  if (!(LAYOUTS[current_workspace->layout].index ==
        1)) // monocle_layout // we already did this we don't need to do it
            // twice
    for (auto &client :
         fullscreen_clients) { // the usser won't be able to make
                               // more than one fullscreen
                               // window in the any arranged mood anyways
      make_fullscreen(client, screen_width, screen_height);
    }
}

// the tile windows function for the tile layout
void tile_windows(std::vector<Client *> *clients, int master_width,
                  int screen_height, int screen_width) {
  unsigned int tiled_index = 1; // To track the position of tiled clients
  int num_tiled_clients = clients->size();
  int stack_width = screen_width - master_width - 2 * (GAP_SIZE + BORDER_WIDTH);
  float done_factors = 0;
  int last_y = current_monitor->y + current_workspace->bar_height;
  int last_height = 0;
  for (int i = 0; i < clients->size(); ++i) {
    Client *c = (*clients)[i];

    if (c->window == current_workspace->master) { // First window is the master
      // Master window positioning with border
      if (num_tiled_clients == 1) {
        make_fullscreen(c, screen_width, screen_height, false);
        break; // we have no more windows
      }
      int x = current_monitor->x + GAP_SIZE;
      int y = current_monitor->y + current_workspace->bar_height + GAP_SIZE;
      int w = master_width - 2 * (GAP_SIZE + BORDER_WIDTH);
      int h = screen_height - current_workspace->bar_height -
              2 * (GAP_SIZE + BORDER_WIDTH);
      XMoveResizeWindow(display, c->window, x, y, w, h);
      c->x = x;
      c->y = y;
      c->width = w;
      c->height = h;
    } else {
      // Stack windows positioning with border
      int y = last_y + ((tiled_index != 1) + 1) *
                           (GAP_SIZE + (tiled_index != 1) *
                                           BORDER_WIDTH); // the starting y
      int stack_height = last_height =
          (c->cfact / current_workspace->cfacts) *
          ((screen_height - current_workspace->bar_height) -
           2 * (num_tiled_clients - 1) * (GAP_SIZE + BORDER_WIDTH));
      last_y = y + last_height; // the y of the client
      int x = current_monitor->x + master_width + GAP_SIZE;
      // Move and resize the window with calculated position and size
      XMoveResizeWindow(display, c->window, x, y, // Position with gaps
                        stack_width,              // Width adjusted for border
                        stack_height              // Height adjusted for border
      );

      c->x = x;
      c->y = y;
      c->width = stack_width;
      c->height = stack_height;

      ++tiled_index;
      done_factors += c->cfact;
    }
    XLowerWindow(display,
                 c->window); // Lower windows to avoid overlap with floaters
    XSetWindowBorderWidth(display, c->window, BORDER_WIDTH); // Set border width
  }
}

// full mode all clients
void monocle_windows(std::vector<Client *> *clients, int master_width,
                     int screen_height, int screen_width) {
  for (int i = 0; i < clients->size(); ++i) {
    Client *c = (*clients)[i];
    make_fullscreen(c, screen_width, screen_height);
  }
}

// gird all the windows
// no master
void grid_windows(std::vector<Client *> *clients, int master_width,
                  int screen_height, int screen_width) {

  int num_clients = (*clients).size();
  int rows = (int)std::sqrt(num_clients);
  int cols = (num_clients + rows - 1) /
             rows; // Ensure enough columns to fit all windows

  int win_width = screen_width / cols;
  int win_height = (screen_height - current_workspace->bar_height) / rows;

  int i = 0;
  for (auto &client : *clients) {
    int row = i / cols;
    int col = i % cols;
    // saving for resizing
    client->x = current_monitor->x + (col * win_width) + GAP_SIZE;
    client->y =
        current_monitor->y + row * win_height + current_workspace->bar_height;
    client->width = win_width - 2 * (GAP_SIZE + BORDER_WIDTH);
    client->height = win_height - 2 * (GAP_SIZE + BORDER_WIDTH);
    XMoveResizeWindow(display, client->window, client->x, client->y,
                      client->width, client->height);
    XSetWindowBorderWidth(display, client->window,
                          BORDER_WIDTH); // Set border Width

    XLowerWindow(
        display,
        client->window); // Lower windows to avoid overlap with floaters
    i++;
  }
}

// the master is in the middle of the screen and other clients are arranged on
// it's sides
void center_master_windows(std::vector<Client *> *clients, int master_width,
                           int screen_height, int screen_width) {

  int num_clients = clients->size();
  int tiled_index = 0;

  // Calculate number of clients on each side of the master window
  int right_clients = (num_clients - 1) / 2;
  int left_clients = (num_clients - 1) - right_clients;

  // Calculate available space for stacking windows
  int stack_width =
      (screen_width - master_width) / 2 - 2 * (GAP_SIZE + BORDER_WIDTH);
  int stack_height, x, y;
  float right_factors = 0, left_factors = 0, done_factors_right = 0,
        done_factors_left = 0;
  int last_y_right = current_monitor->y + current_workspace->bar_height;
  int last_y_left = current_monitor->y + current_workspace->bar_height;
  int last_height_right = 0, last_height_left = 0;
  unsigned int left_index = 0, right_index = 0;

  for (int i = 0, x = 0; x < num_clients; x++) {
    if (current_workspace->master == (*clients)[x]->window) {
      continue;
    }
    if (i < left_clients) {
      left_factors += (*clients)[x]->cfact;
      i++;
    } else {
      right_factors += (*clients)[x]->cfact;
      i++;
    }
  }
  for (int i = 0; i < num_clients; ++i) {
    Client *c = (*clients)[i];

    if (c->window == current_workspace->master) { // Master window
      if (num_clients == 1) {
        // If only one window, make it fullscreen
        make_fullscreen(c, screen_width, screen_height, false);
        break; // Exit after handling the single window
      }
      int x =
          current_monitor->x + stack_width + 3 * GAP_SIZE + 2 * BORDER_WIDTH;
      int y = current_monitor->y + current_workspace->bar_height + GAP_SIZE;
      int height = screen_height - current_workspace->bar_height -
                   2 * (GAP_SIZE + BORDER_WIDTH);
      int width = master_width - 2 * (GAP_SIZE + BORDER_WIDTH);
      // Position master window
      XMoveResizeWindow(display, c->window, x, y, width, height);
      c->x = x;
      c->y = y;
      c->width = width;
      c->height = height;
    } else { // Tiled windows
      if (tiled_index < left_clients) {
        // Position for left side of the master
        y = last_y_left +
            ((left_index != 0) + 1) *
                (GAP_SIZE + (left_index != 0) * BORDER_WIDTH); // the starting y
        stack_height = last_height_left =
            (c->cfact / left_factors) *
            ((screen_height - current_workspace->bar_height) -
             2 * (left_clients) * (GAP_SIZE + BORDER_WIDTH));
        last_y_left = y + last_height_left; // the y of the client
        x = current_monitor->x + GAP_SIZE;  // the starting x;
        done_factors_left += c->cfact;
        left_index++;

      } else {
        // Position for right side of the master
        y = last_y_right + ((right_index != 0) + 1) *
                               (GAP_SIZE + (right_index != 0) *
                                               BORDER_WIDTH); // the starting y
        stack_height = last_height_right =
            (c->cfact / right_factors) *
            ((screen_height - current_workspace->bar_height) -
             2 * (right_clients) * (GAP_SIZE + BORDER_WIDTH));
        last_y_right = y + last_height_right; // the y of the client
        x = (current_monitor->x + master_width + stack_width) +
            (3 * GAP_SIZE + 2 * BORDER_WIDTH);
        done_factors_right += c->cfact;

        right_index++;
      }

      XMoveResizeWindow(display, c->window, x, y, stack_width, stack_height);
      tiled_index++;
      // saveing the tiled x,y,hight,widtg for future resizing
      c->x = x;
      c->y = y;
      c->width = stack_width;
      c->height = stack_height;
    }

    XLowerWindow(display, c->window); // Lower windows to avoid overlap
    XSetWindowBorderWidth(display, c->window, BORDER_WIDTH); // Uncomment to
    // set border width if needed
  }
}


// make a window full screen 
void make_fullscreen(Client *client, int screen_width, int screen_height,
                     bool raise) {
  // Save the original position and size
  XWindowAttributes wa;
  XGetWindowAttributes(display, client->window, &wa);
  client->x = wa.x;
  client->y = wa.y;
  client->width = wa.width;
  client->height = wa.height;
  ////saving for future unmake fullscreen

  XMoveResizeWindow(display, client->window, current_monitor->x + GAP_SIZE,
                    current_monitor->y + current_workspace->bar_height +
                        GAP_SIZE,
                    screen_width - 2 * (GAP_SIZE + BORDER_WIDTH),
                    screen_height - current_workspace->bar_height -
                        2 * (GAP_SIZE + BORDER_WIDTH));
  XSetWindowBorderWidth(display, client->window, BORDER_WIDTH);
  // Go full-screen (resize to cover the entire screen)
  (raise) ? XRaiseWindow(display, client->window)
          : XLowerWindow(display, client->window);
}
