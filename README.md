xvisbell2
=========

Flash a window on the screen whenever the X11 bell is rung. Based on [rianhunter's xvisbell](https://github.com/rianhunter/xvisbell).


The typical way to use this program is to run it in the background when X starts. If you are using a desktop environment, it probably includes a way to run applications at startup. Otherwise this can be done by adding the line `xvisbell` to `~/.xinitrc` (assuming you did `make install`, otherwise the path to `xvisbell` must be specified). Alternatively, you can start it in your window manager's startup script, e.g. for i3 add the line `exec --no-startup-id xvisbell &` to `~/.i3/config`.


Usage
-----
`xvisbell [-h <height>] [-w <width] [-x <x position>] [-y <y position>] [-c <colour name>] [-d <ms duration>] [-f]`


`--help` prints the above usage information and exits.


`-x` and `-y` set the x and y position of the top left of the flashed window (default 0).
You can equivalently use `--x` or `--y`.


`-h` and `-w` set the height and width of the flashed window (default height/width of the screen).
If a negative value is given then the height/width of the screen is used.
You can equivalently use `--height` and `--width`.


`-c` sets the color of the flashed window by its X11 colour name.
You can equivalently use `--colour` or `--color`.
To find a list of supported colour names for a given machine, see `<X11root>/lib/X11/rgb.txt` (`<X11root>` is usually `/usr/` or `/etc/`).
There is also a visualization of commonly supported colour names available on [Wikipedia](https://en.wikipedia.org/wiki/X11_color_names#Color_name_chart).


`-d` sets the duration of the flash in milliseconds. You can equivalently use `--duration`.


`-f` flashes once and then exits. You can equivalently use `--flash`. This is generally used if using an external program to start `xvisbell` when the bell rings. Note that it is usually more efficient to let `xvisbell` listen for bell rings itself instead of using another program since it uses the `select` syscall on an IPC socket from X11 to wait for the bell to ring, thereby preventing busy-waiting.
