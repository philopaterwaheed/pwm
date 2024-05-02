#include <X11/Xlib.h>
#include <iostream>

int main() {
    Display *display;
    Window window;
    XEvent event;

    // Open a new display
    display = XOpenDisplay(nullptr);
    if (display == nullptr) {
        std::cerr << "Error: Could not open display." << std::endl;
        return 1;
    }

    // Create a simple window
    int screen_num = DefaultScreen(display);
    window = XCreateSimpleWindow(display, RootWindow(display, screen_num),
                                 10, 10, 200, 200, 1,
                                 BlackPixel(display, screen_num),
                                 WhitePixel(display, screen_num));

    // Select input events to listen for
    XSelectInput(display, window, ExposureMask | KeyPressMask);

    // Map the window on the screen
    XMapWindow(display, window);

    // Main event loop
    while (1) {
        XNextEvent(display, &event);
        if (event.type == Expose) {
            // Redraw the window on expose events
            XFillRectangle(display, window, DefaultGC(display, screen_num),
                           20, 20, 100, 100);
        }
        if (event.type == KeyPress)
            break;
    }

    // Clean up and close the display
    XDestroyWindow(display, window);
    XCloseDisplay(display);

    return 0;
}
