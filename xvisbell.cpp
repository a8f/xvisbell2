/*
  xvisbell: visual bell for X11

  Copyright 2015 Rian Hunter <rian@alum.mit.edu>

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

#include <iostream>
#include <stdexcept>

#include <cstdlib>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <string.h>

#include <sys/select.h>
#include <sys/time.h>

const struct timeval window_timeout = {0, 100000};

// Custom color for visual bell
char *bell_color;

// Dimensions/position of visual bell
// -1 for w/h means screen width/height
struct {
  int x, y;
  int w, h;
} geometry = {0, 0, -1, -1};

bool operator<(const struct timeval & a,
               const struct timeval & b) {
  return timercmp(&a, &b, <);
}

struct timeval & operator+=(struct timeval & a, const struct timeval & b) {
  timeradd(&a, &b, &a);
  return a;
}

struct timeval operator-(const struct timeval & a,
                         const struct timeval & b) {
  struct timeval toret;
  timersub(&a, &b, &toret);
  return toret;
}

/*
 * Parse a long from string s and store its rounded int value in i
 * If s is a valid long in (INT_MIN, INT_MAX) then i is set to round((long)s) and false is returned
 * Otherwise true is returned and i is not modified
 */
bool long_str_to_int(char *s, int *i) {
  char *end;
  errno = 0;
  long l = strtol(s, &end, 10);
  if (errno == ERANGE && (l == LONG_MAX || l == LONG_MIN)) return true; // long over/underflow
  if (l > INT_MAX || l < INT_MIN) return true; // int over/underflow
  if (*end != '\0') return true; // The string had some non-digit chars after the parsed int
  *i = (int)round(l);
  return false;
}


void print_usage(char *argv[]) {
  printf("Usage: %s [-h <height>] [-w <width] [-x <x position>] [-y <y position>] [-c <colour name>]\n", argv[0]);
}

void parse_args(int argc, char *argv[]) {
  char option;
  struct option long_opts[8] = {
    {"help", no_argument, NULL, 0},
    {"width", required_argument, NULL, 'w'},
    {"height", required_argument, NULL, 'h'},
    // Allow --x and --y for convenience
    {"x", required_argument, NULL, 'x'},
    {"y", required_argument, NULL, 'y'},
    {"color", required_argument, NULL, 'c'},
    {"colour", required_argument, NULL, 'c'},
    {0, 0, 0, 0} // Last element must have all 0s for getopt_long
  };

  while ((option = getopt_long(argc, argv, "w:h:x:y:c:", long_opts, NULL)) != -1) {
    switch (option) {
      case 0: // --help
        print_usage(argv);
        exit(0);
      case 'c':
        errno = 0;
        bell_color = strdup(optarg);
        if (bell_color == NULL) {
          printf("Error setting color to %s", optarg);
          if (errno == 0) printf("\n");
          else printf(" (%d)\n", errno);
          exit(1);
        }
        break;
      case 'w':
        if (long_str_to_int(optarg, &geometry.w)) {
          printf("Invalid width %s\n", optarg);
          exit(1);
        }
        break;
      case 'h':
        if (long_str_to_int(optarg, &geometry.h)) {
          printf("Invalid height %s\n", optarg);
          exit(1);
        }
        break;
      case 'x':
        if (long_str_to_int(optarg, &geometry.x)) {
          printf("Invalid x position %s\n", optarg);
          exit(1);
        }
        break;
      case 'y':
        if (long_str_to_int(optarg, &geometry.y)) {
          printf("Invalid y position %s\n", optarg);
          exit(1);
        }
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

int main(int argc, char *argv[]) {
  parse_args(argc, argv);
  auto dpy = XOpenDisplay(nullptr);
  if (!dpy) {
    throw std::runtime_error("XOpenDisplay() error");
  }

  auto scr = XDefaultScreen(dpy);
  auto root = XRootWindow(dpy, scr);
  auto vis = XDefaultVisual(dpy, scr);

  auto major = XkbMajorVersion;
  auto minor = XkbMinorVersion;

  if (!XkbLibraryVersion(&major, &minor)) {
    throw std::runtime_error("XkbLibraryVersion() error");
  }

  major = XkbMajorVersion;
  minor = XkbMinorVersion;

  int xkb_event_base;
  if (!XkbQueryExtension(dpy, nullptr, &xkb_event_base,
                         nullptr, &major, &minor)) {
    throw std::runtime_error("XkbQueryExtension() error");
  }

  XkbSelectEvents(dpy, XkbUseCoreKbd, XkbBellNotifyMask, XkbBellNotifyMask);

  unsigned int auto_ctrls, auto_values;
  auto_ctrls = auto_values = XkbAudibleBellMask;

  XkbSetAutoResetControls(dpy, XkbAudibleBellMask, &auto_ctrls, &auto_values);
  XkbChangeEnabledControls(dpy, XkbUseCoreKbd, XkbAudibleBellMask, 0);

  XSetWindowAttributes attrs;
  attrs.override_redirect = True;
  attrs.save_under = True;
  // Set background colour
  if (bell_color == NULL || strncmp(bell_color, "white", 5) == 0) {
    attrs.background_pixel = WhitePixel(dpy, scr);
  } else {
    XColor rgb, nearest;
    attrs.colormap = XDefaultColormap(dpy, scr);
    if (!XAllocNamedColor(dpy, attrs.colormap, bell_color, &rgb, &nearest)) {
      printf("Colour %s isn't supported\n", bell_color);
      exit(1);
    }
    attrs.background_pixel = nearest.pixel;
  }

  auto x11_fd = ConnectionNumber(dpy);

  struct timeval future_wakeup;
  bool timeout_is_set = false;

  auto width = geometry.w < 0 ? DisplayWidth(dpy, scr) : geometry.w;
  auto height = geometry.h < 0 ? DisplayHeight(dpy, scr) : geometry.h;

  auto win = XCreateWindow(dpy, root, geometry.x, geometry.y,
                           width, height, 0,
                           XDefaultDepth(dpy, scr), InputOutput,
                           vis,
                           CWBackPixel | CWOverrideRedirect | CWSaveUnder,
                           &attrs);

  while (true) {
    struct timeval tv, *wait_tv = nullptr;

    fd_set in_fds;
    FD_ZERO(&in_fds);
    FD_SET(x11_fd, &in_fds);

    if (timeout_is_set) {
      // we really should use a monotonic clock here but
      // there isn't really a portable interface for that yet
      // in the future we can ifdef this for mac/linux
      struct timeval cur_time;
      if (gettimeofday(&cur_time, nullptr) < 0) {
        throw std::runtime_error("getttimeofday() error!");
      }

      // c++ magic, in your face linus!
      tv = (future_wakeup < cur_time
            ? timeval{0, 0}
            : future_wakeup - cur_time);
      wait_tv = &tv;
    }

    while (true) {
      if (select(x11_fd + 1, &in_fds, nullptr, nullptr, wait_tv) < 0) {
        if (errno == EINTR) continue;
        throw std::runtime_error("select() error!");
      }
      break;
    }

    if (timeout_is_set) {
      struct timeval cur_time;
      if (gettimeofday(&cur_time, nullptr) < 0) {
        throw std::runtime_error("getttimeofday() error!");
      }

      if (future_wakeup < cur_time) {
        // timeout fired
        XUnmapWindow(dpy, win);
        timeout_is_set = false;
      }
    }

    while (XPending(dpy)) {
      XEvent ev;
      XNextEvent(dpy, &ev);

      // TODO: handle resize events on root window

      // TODO: this reinterpret cast is not good
      if (reinterpret_cast<XkbEvent *>(&ev)->any.xkb_type != XkbBellNotify) {
        continue;
      }

      XMapRaised(dpy, win);

      // reset timeout
      timeout_is_set = true;
      if (gettimeofday(&future_wakeup, nullptr) < 0) {
        throw std::runtime_error("getttimeofday() error!");
      }
      future_wakeup += window_timeout;

      // ignore for now...
      // auto bne = reinterpret_cast<XkbBellNotifyEvent *>(&ev);
    }
  }
}

