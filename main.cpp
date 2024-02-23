#include <X11/X.h>
#include <X11/Xlib.h>
#include <vector>
#include <iostream>
#include <X11/keysym.h>  // Add this line for XK_Escape

int main() {
    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        std::cerr << "Error: Unable to open display." << std::endl;
        return 1;
    }

    int screen = DefaultScreen(display);
    Window rootWindow = RootWindow(display, screen);

    // Create a new window
    Window window = XCreateSimpleWindow(display, rootWindow, 100, 100, 400, 300, 1,
                                        BlackPixel(display, screen), WhitePixel(display, screen));

    // vector of windows
    std :: vector<Window> windows;
    // Set window properties
    XSelectInput(display, window, ExposureMask | KeyPressMask);
    XMapWindow(display, window);

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
                    // return 0;
}
                break;
        }
    }

    XCloseDisplay(display);
    return 0;
}
