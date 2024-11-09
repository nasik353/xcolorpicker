#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>

#include <chrono>
#include <thread>
#include <string>
#include <iostream>
#include <sstream>
#include <unistd.h>

std::string rgb2hex(int r, int g, int b);
void getMouseCoords(Display *display, int screen, int *x_coord_ret, int *y_coord_ret);
XColor getPixelColor(Display *display, int x_coord, int y_coord);
void xcopy(Display *display, Window window, Atom selection, unsigned char *text, int size, Atom targets_atom, Atom text_atom, Atom UTF8, Atom XA_ATOM, Atom XA_STRING);

int main()
{
    // init
    // create display
    char *display_name = getenv("DISPLAY");
    if (!display_name)
    {
        std::cerr << "Get display failed\n";
        exit(1);
    }

    Display *display = XOpenDisplay(display_name);
    Display *display_copy = XOpenDisplay(display_name);
    if (!display || !display_copy)
    {
        std::cerr << "Open display failed\n";
        exit(1);
    }

    // get screen
    int screen_num = DefaultScreen(display);
    Screen *screen = ScreenOfDisplay(display, screen_num);

    // create windows
    Window window_root = RootWindow(display, XScreenNumberOfScreen(screen));
    Window window_color = XCreateSimpleWindow(display, window_root, 0, 0, 75, 75, 1, 0, 0);
    Window window_copy = XCreateSimpleWindow(display_copy, window_root, 0, 0, 1, 1, 0, 0, 0);
    XSetStandardProperties(display, window_color, "xcolorpicker", " ", None, nullptr, 0, nullptr);

    // set class name
    XClassHint class_name{"xcolorpicker", "xcolorpicker"};
    XSetClassHint(display, window_color, &class_name);

    XClearWindow(display, window_color);
    XMapRaised(display, window_color);

    // change curser shape
    Cursor cursor = XCreateFontCursor(display, XC_crosshair);

    // grab pointer input
    if ((XGrabPointer(display, window_root, True, ButtonMotionMask | ButtonPressMask | ButtonReleaseMask,
                      GrabModeAsync, GrabModeAsync, window_root, cursor, CurrentTime) != GrabSuccess))
    {
        std::cerr << "Grab pointer failed\n";
    }

    // get targets, text and UTF8 formats
    Atom targets_atom = XInternAtom(display, "TARGETS", 0);
    Atom text_atom = XInternAtom(display, "TEXT", 0);
    Atom UTF8 = XInternAtom(display, "UTF8_STRING", 1);
    Atom XA_ATOM = 4, XA_STRING = 31;
    if (UTF8 == None)
    {
        UTF8 = XA_STRING;
    }

    // get CLIPBOARD selection
    Atom selection = XInternAtom(display, "CLIPBOARD", 0);

    // main loop
    int x_coord, y_coord = 0;
    unsigned short r_value, g_value, b_value;
    XColor screen_pixel_color;
    XEvent event;
    bool running = true;
    while (running)
    {
        if (!XPending(display))
        {
            // clear window
            XClearWindow(display, window_color);

            // get mouse pos
            getMouseCoords(display, screen_num, &x_coord, &y_coord);

            // move window to mouse pos
            XMoveWindow(display, window_color, x_coord + 25, y_coord - 100);
            XSync(display, False);

            // get mouse pos pixel color
            screen_pixel_color = getPixelColor(display, x_coord, y_coord);

            // paint window with color
            XSetWindowBackground(display, window_color, screen_pixel_color.pixel);

            continue;
        }
        if ((XNextEvent(display, &event) >= 0))
        {
            switch (event.type)
            {
            case ButtonPress:
                switch (event.xbutton.button)
                {
                case Button1: // left click
                {
                    r_value = screen_pixel_color.red / 256.0;
                    g_value = screen_pixel_color.green / 256.0;
                    b_value = screen_pixel_color.blue / 256.0;
                    auto hexColor = rgb2hex(r_value, g_value, b_value);

                    // fork and exit current process
                    pid_t pid = fork();
                    if (pid)
                    {
                        XSetSelectionOwner(display, selection, None, CurrentTime);
                        exit(EXIT_SUCCESS);
                    }

                    // destroy color pad window and display to cancel the XGrabPointer effect
                    XDestroyWindow(display, window_color);
                    XCloseDisplay(display);

                    // run the xcopy process. This process will exit when the selection is changed (i.e a user copies any text)
                    xcopy(display_copy, window_copy, selection, (unsigned char *)hexColor.data(), hexColor.size(), targets_atom, text_atom, UTF8, XA_ATOM, XA_STRING);
                    running = false;
                    break;
                }
                case Button3: // right click
                    running = false;
                    break;
                default:
                    break;
                }
                break;
            }
        }
    }

    // finish
    XDestroyWindow(display, window_copy);
    XDestroyWindow(display, window_color);
    XCloseDisplay(display_copy);
    XCloseDisplay(display);
    return 0;
}

std::string rgb2hex(int r, int g, int b)
{
    char hexColor[8];
    std::snprintf(hexColor, sizeof hexColor, "#%02x%02x%02x", r, g, b);
    return std::string(hexColor);
}

void getMouseCoords(Display *display, int screen, int *x_coord_ret, int *y_coord_ret)
{
    // get the mouse cursor position
    int root_x, root_y = 0;
    unsigned int mask = 0;
    Window child_win, root_win;
    XQueryPointer(display, XRootWindow(display, screen),
                  &child_win, &root_win,
                  &root_x, &root_y, x_coord_ret, y_coord_ret, &mask);
}

XColor getPixelColor(Display *display, int x_coord, int y_coord)
{
    XColor pixel_color;
    XImage *screen_image = XGetImage(
        display,
        XRootWindow(display, XDefaultScreen(display)),
        x_coord, y_coord,
        1, 1,
        AllPlanes,
        XYPixmap);

    pixel_color.pixel = XGetPixel(screen_image, 0, 0);
    XFree(screen_image);
    XQueryColor(display, XDefaultColormap(display, XDefaultScreen(display)), &pixel_color);

    return pixel_color;
}

void xcopy(Display *display, Window window, Atom selection, unsigned char *text, int size, Atom targets_atom, Atom text_atom, Atom UTF8, Atom XA_ATOM = 4, Atom XA_STRING = 31)
{

    XSetSelectionOwner(display, selection, window, CurrentTime);
    if (XGetSelectionOwner(display, selection) != window)
    {
        return;
    }

    XEvent event;
    while (1)
    {
        XNextEvent(display, &event);
        switch (event.type)
        {
        case SelectionRequest:
        {

            if (event.xselectionrequest.selection != selection)
            {
                break;
            }

            XSelectionRequestEvent *xsr = &event.xselectionrequest;
            XSelectionEvent ev = {0, 0, 0, nullptr, 0, 0, 0, 0, 0};

            int R = 0;
            ev.type = SelectionNotify;
            ev.display = xsr->display;
            ev.requestor = xsr->requestor;
            ev.selection = xsr->selection;
            ev.time = xsr->time;
            ev.target = xsr->target;
            ev.property = xsr->property;

            if (ev.target == targets_atom)
                R = XChangeProperty(ev.display, ev.requestor, ev.property, XA_ATOM, 32, PropModeReplace, (unsigned char *)&UTF8, 1);
            else if (ev.target == XA_STRING || ev.target == text_atom)
                R = XChangeProperty(ev.display, ev.requestor, ev.property, XA_STRING, 8, PropModeReplace, text, size);
            else if (ev.target == UTF8)
                R = XChangeProperty(ev.display, ev.requestor, ev.property, UTF8, 8, PropModeReplace, text, size);
            else
                ev.property = None;

            if ((R & 2) == 0)
            {
                XSendEvent(display, ev.requestor, 0, 0, (XEvent *)&ev);
            }
            break;
        }

        case SelectionClear:
            exit(EXIT_SUCCESS);
        }
    }
}