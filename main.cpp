#include <X11/X.h>
#include <X11/Xlib.h>
#include <vector>
#include <iostream>
#include <X11/keysym.h>  // Add this line for XK_Escape
#include <X11/Xcursor/Xcursor.h>

// client running struct
struct Client 
{
	int x;
	int y;
	Window window;
	unsigned int width , height;
};

std::vector<Client> clients;

Client *get_client(Window window){
    for (auto &client : clients) {
        if (client.window == window)
            return &client;
    }
    return nullptr;
}

void handle_map_request(Display *display, XMapRequestEvent &event) {
	Client client;
	client.window = event.window;
	XGetGeometry(display, event.window, &client.window, &client.x, &client.y, &client.width, &client.height, nullptr, nullptr);
	clients.push_back(client);
	XMapWindow(display, event.window);
}
int main() {
    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        std::cerr << "Error: Unable to open display." << std::endl;
        return 1;
    }

    int screen = DefaultScreen(display);
    Window rootWindow = RootWindow(display, screen);
    // Create the cursor
    Cursor cursor = XCreateFontCursor(display, 69);

    // Set the cursor for the window
    XDefineCursor(display, rootWindow, cursor);

    // Create a new window
    Window window = XCreateSimpleWindow(display, rootWindow, 100, 100, 400, 300, 1,
                                        BlackPixel(display, screen), WhitePixel(display, screen));

    // vector of windows
    std :: vector<Window> windows;
    // Set window properties
    // windows.push_back(window);
    XSelectInput(display, rootWindow, ExposureMask | KeyPressMask);
    // XMapWindow(display, vwindows[0]);
    XSetInputFocus(display, rootWindow, RevertToPointerRoot, CurrentTime);

    XEvent event;
    while (true) {
        XNextEvent(display, &event);
        switch (event.type) {
            case Expose:
                // Handle Expose event (window needs to be redrawn)
                break;
            case KeyPress:
                // Handle KeyPress event (for example, close the window with Escape key)
                if (event.xkey.keycode == XKeysymToKeycode(display, XK_a)) {
		    windows.push_back(XCreateSimpleWindow(display, rootWindow, 100, 100, 400, 300, 1,
                                        BlackPixel(display, screen), WhitePixel(display, screen)));
		    XSelectInput(display, windows[windows.size()-1], ExposureMask | KeyPressMask);
		    XMapWindow(display, windows[windows.size()-1]);
		}
                if (event.xkey.keycode == XKeysymToKeycode(display, XK_q)) {
		    if (windows.size()>0)
		    {
			XDestroyWindow(display,windows[windows.size()-1]);
			windows.pop_back();
		    }
		}
                if (event.xkey.keycode == XKeysymToKeycode(display, XK_Escape)) {
		     XCloseDisplay(display);
                     return 0;
		}
                break;
        }
	// if (windows.size() == 0 )
	    // return 0;
    }

    XCloseDisplay(display);
    return 0;
}
