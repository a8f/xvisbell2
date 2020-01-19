/*
   xvisbell: visual bell for X11

   Copyright 2015 Rian Hunter <rian@alum.mit.edu>
   Copyright 2020 Alexander French <a.french@mail.utoronto.ca>

   Derived from pulseaudio/src/modules/x11/module-x11-bell.c
   Copyright 2004-2006 Lennart Poettering

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published
   by the Free Software Foundation; either version 3 of the License,
   or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <X11/XKBlib.h>
#include <X11/Xlib.h>

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/select.h>
#include <sys/time.h>

// Define clock_gettime since it's not implemented on older versions of OS X (< 10.12)
// from https://stackoverflow.com/a/9781275
#if defined(__MACH__) && !defined(CLOCK_MONOTONIC)
#include <sys/time.h>
#define CLOCK_MONOTONIC 0
int clock_gettime(int /*clk_id*/, struct timespec*t) {
    struct timeval now;
    int rv = gettimeofday(&now, NULL);

    if (rv) return rv;
    t->tv_sec = now.tv_sec;
    t->tv_nsec = now.tv_usec * 1000;
    return 0;
}
#endif

// If true then flash one time and exit instead of listening for X's bell
bool flash_once = false;

// Visual bell
struct {
    int x, y; // Position
    long w, h; // Dimensions. -1 means match display size
    unsigned long duration; // Duration in ms
    char *color; // Color as an X11 color name
} bell = {0, 0, -1, -1, 100, NULL};


// Returns the difference between start and end or {0, 0} if end is before start
struct timespec timespec_diff(struct timespec *start, struct timespec *end) {
    if (start->tv_sec > end->tv_sec || (start->tv_sec == end->tv_sec && start->tv_nsec > end->tv_nsec)) {
        return (struct timespec){0, 0};
    }

    struct timespec result;
    if (end->tv_nsec - start->tv_nsec < 0) {
        result.tv_sec = end->tv_sec - start->tv_sec - 1;
        result.tv_nsec = 1.0e9 + end->tv_nsec - start->tv_nsec;
    } else {
        result.tv_sec = end->tv_sec - start->tv_sec;
        result.tv_nsec = end->tv_nsec - start->tv_nsec;
    }
    return result;
}

/*
 * Parse a long from a string
 * If s is a valid long then l is set to the long value of s and false is returned
 * Otherwise true is returned and l is not modified
 */
bool parse_long(char *s, long *l) {
    char *end;

    errno = 0;
    long parsed = strtol(s, &end, 10);
    if (errno == ERANGE && (parsed == LONG_MAX || parsed == LONG_MIN)) return true; // long over/underflow
    if (*end != '\0') return true; // String had non-digit chars after the parsed value
    *l = parsed;
    return false;
}

/*
 * Parse an unsigned long from a string
 * If s is a valid unsigned long then l is set to the long value of s and false is returned
 * Otherwise true is returned and l is not modified
 */
bool parse_ulong(char *s, unsigned long *l) {
    char *end;

    errno = 0;
    long parsed = strtoul(s, &end, 10);
    if (errno == ERANGE && (parsed == LONG_MAX || parsed < 0)) return true; // long over/underflow
    if (*end != '\0') return true; // String had non-digit chars after the parsed value
    *l = parsed;
    return false;
}

void print_usage(char *argv[]) {
    printf("Usage: %s [-h <height>] [-w <width] [-x <x position>] [-y <y position>] [-c <colour name>]\n", argv[0]);
}

void parse_args(int argc, char *argv[]) {
    char option;
    struct option long_opts[10] = {
        {"help", no_argument, NULL, 0},
        {"width", required_argument, NULL, 'w'},
        {"height", required_argument, NULL, 'h'},
        // Allow --x and --y for convenience
        {"x", required_argument, NULL, 'x'},
        {"y", required_argument, NULL, 'y'},
        {"color", required_argument, NULL, 'c'},
        {"colour", required_argument, NULL, 'c'},
        {"duration", required_argument, NULL, 'd'},
        {"flash", no_argument, NULL, 'f'},
        {0, 0, 0, 0} // Last element must have all 0s for getopt_long
    };
    long tmp; // buffer for parsing arguments for options

    while ((option = getopt_long(argc, argv, "w:h:x:y:c:d:f", long_opts, NULL)) != -1) {
        switch (option) {
            case 0: // --help
                print_usage(argv);
                exit(0);

            case 'w': // --width
                if (parse_long(optarg, &tmp)) {
                    printf("Invalid width %s\n", optarg);
                    exit(1);
                }
                if (tmp > UINT_MAX) {
                    printf("Invalid width. The maximum width is %d\n", UINT_MAX);
                    exit(1);
                }
                if (tmp < 0) bell.w = -1;
                else bell.w = (long) tmp;
                break;

            case 'h': // --height
                if (parse_long(optarg, &tmp)) {
                    printf("Invalid height %s\n", optarg);
                    exit(1);
                }
                if (tmp > UINT_MAX) {
                    printf("Invalid height. The maximum height is %d\n", UINT_MAX);
                    exit(1);
                }
                if (tmp < 0) bell.h = -1;
                else bell.h = (long) tmp;
                break;

            case 'x': // --x
                if (parse_long(optarg, &tmp)) {
                    printf("Invalid x position %s\n", optarg);
                    exit(1);
                }
                if (tmp < INT_MIN || tmp > INT_MAX) {
                    printf("Invalid x. Must be an integer in the range (%d, %d)\n", INT_MIN, INT_MAX);
                    exit(1);
                }
                bell.x = (int) tmp;
                break;

            case 'y': // --y
                if (parse_long(optarg, &tmp)) {
                    printf("Invalid y position %s\n", optarg);
                    exit(1);
                }
                if (tmp < INT_MIN || tmp > INT_MAX) {
                    printf("Invalid y. Must be an integer in the range (%d, %d)\n", INT_MIN, INT_MAX);
                    exit(1);
                }
                bell.y = (int) tmp;
                break;

            case 'c': // --color, --colour
                errno = 0;
                bell.color = strdup(optarg);
                if (bell.color == NULL) {
                    printf("Error setting color to %s", optarg);
                    if (errno == 0) printf("\n");
                    else printf(" (%d). Make sure you are using a valid X11 color name\n", errno);
                    exit(1);
                }
                break;

            case 'd': // --duration
                if (parse_ulong(optarg, &bell.duration)) {
                    printf("Invalid duration %s. Should be a non-negative number of milliseconds.\n", optarg);
                    exit(1);
                }
                break;

            case 'f': // --flash
                flash_once = true;
                break;

            default:
                // Print error message if getopt didn't already
                if (option != '?') {
                    printf("Invalid option %c\n", option);
                }
                print_usage(argv);
                exit(1);
        }
    }
}

static inline void update_timeout_and_hide(struct timespec *now, struct timespec *end_time, struct timespec *timeout,
                                           Display **display, int *window, bool *visible) {
    clock_gettime(CLOCK_MONOTONIC, now);
    *timeout = timespec_diff(now, end_time);
    if (timeout->tv_sec == 0 && timeout->tv_nsec == 0) {
        XUnmapWindow(*display, *window);
        *visible = false;
    }
}

// Flash the screen once then exit(0)
// Never returns
void flash_once_and_exit(Display *display, Window window, struct timespec *duration) {
    struct timespec now, timeout, end_time;

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    end_time.tv_sec += duration->tv_sec;
    end_time.tv_nsec += duration->tv_nsec;

    // Create and display the window
    XMapRaised(display, window);
    XFlush(display);
    bool visible = true;

    // Wait for duration then destroy the window and return
    // This should only have 2 iterations max in normal circumstances
    while (visible) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        timeout = timespec_diff(&now, &end_time);
        if (timeout.tv_sec == 0 && timeout.tv_nsec == 0) {
            XUnmapWindow(display, window);
            exit(0);
        }
        nanosleep(&timeout, NULL);
    }
    exit(0);
}

int main(int argc, char *argv[]) {
    parse_args(argc, argv);

    Display *display = XOpenDisplay(NULL);
    if (!display) {
        printf("Error opening display\n");
        return 1;
    }

    int screen = XDefaultScreen(display);
    Window root = XRootWindow(display, screen);
    Visual *visual = XDefaultVisual(display, screen);

    int major = XkbMajorVersion;
    int minor = XkbMinorVersion;

    if (!XkbLibraryVersion(&major, &minor)) {
        printf("X server doesn't support Xkb extension\n");
        return 1;
    }

    major = XkbMajorVersion;
    minor = XkbMinorVersion;

    int xkb_event_base;
    if (!XkbQueryExtension(display, NULL, &xkb_event_base,
                           NULL, &major, &minor)) {
        printf("X server has wrong version of Xkb extension (try rebuilding xvisbell)\n");
        return 1;
    }

    XkbSelectEvents(display, XkbUseCoreKbd, XkbBellNotifyMask, XkbBellNotifyMask);

    unsigned int auto_ctrls, auto_values;
    auto_ctrls = auto_values = XkbAudibleBellMask;

    XkbSetAutoResetControls(display, XkbAudibleBellMask, &auto_ctrls, &auto_values);
    XkbChangeEnabledControls(display, XkbUseCoreKbd, XkbAudibleBellMask, 0);

    XSetWindowAttributes attrs;
    attrs.override_redirect = True;
    attrs.save_under = True;
    // Set background colour
    if (bell.color == NULL || strncmp(bell.color, "white", 5) == 0) {
        attrs.background_pixel = WhitePixel(display, screen);
    } else {
        XColor rgb, nearest;
        attrs.colormap = XDefaultColormap(display, screen);
        if (!XAllocNamedColor(display, attrs.colormap, bell.color, &rgb, &nearest)) {
            printf("Colour %s isn't supported\n", bell.color);
            exit(1);
        }
        attrs.background_pixel = nearest.pixel;
    }

    int x11_fd = ConnectionNumber(display);

    // When to hide the window (from CLOCK_MONOTONIC)
    struct timespec end_time;

    // Whether the window is visible
    bool visible = false;

    // How long to show the window for
    struct timespec duration = {bell.duration / 1000, (bell.duration % 1000) * 1000000};

    // Window shape
    int width = bell.w < 0 ? DisplayWidth(display, screen) : bell.w;
    int height = bell.h < 0 ? DisplayHeight(display, screen) : bell.h;

    int window = XCreateWindow(display, root, bell.x, bell.y,
                               width, height, 0,
                               XDefaultDepth(display, screen), InputOutput,
                               visual,
                               CWBackPixel | CWOverrideRedirect | CWSaveUnder,
                               &attrs);

    if (flash_once) flash_once_and_exit(display, window, &duration);

    for (;;) {
        struct timespec now, timeout = {0, 0};

        fd_set in_fds;
        FD_ZERO(&in_fds);
        FD_SET(x11_fd, &in_fds);

        if (visible) update_timeout_and_hide(&now, &end_time, &timeout, &display, &window, &visible);

x11select:
        if (pselect(x11_fd + 1, &in_fds, NULL, NULL, &timeout, NULL) < 0) {
            if (errno == EINTR) goto x11select;
            printf("Error in select() (errno %d)\n", errno);
            return 1;
        }

        if (visible) update_timeout_and_hide(&now, &end_time, &timeout, &display, &window, &visible);

        while (XPending(display)) {
            XEvent ev;
            XNextEvent(display, &ev);

            if (((XkbEvent *) &ev)->any.xkb_type != XkbBellNotify) continue;

            XMapRaised(display, window);

            visible = true;
            clock_gettime(CLOCK_MONOTONIC, &end_time);
            end_time.tv_sec += duration.tv_sec;
            end_time.tv_nsec += duration.tv_nsec;
        }
    }
}
