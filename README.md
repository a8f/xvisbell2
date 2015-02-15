xvisbell
========

This is a simple program that flashes a white window on the screen whenever the
X11 bell is rung.

The typical way to use this program is to run it in the background in your
.xsession/.xinitrc file. Works best when used with dwm & xmonad & friends.

Why would I use this?
----------------------

If you're like me and you are constantly pressing ^G in Emacs out of (possibly
bad) habit, audio bells can be annoying. Visual bells are less annoying.

How do I configure this?
------------------------

Edit xvisbell.cpp. If you don't know how to program, then learn how to program.

Why is this written in C++?
-----------------------------

C++ nails the high-level/efficient tradeoff for me. It's also mature,
ubiquitous, has lots of tool support and is still evolving.

Why is this licensed under the GPL?
------------------------

The GPL and copyleft in general are necessary tools in the fight for a free
technological society.

In the same vein that free speech was a necessary tool in the fight for the
free societies of olde.

Watch this: https://www.youtube.com/watch?v-Ag1AKIl_2GM

I heard that Clang is better than GCC
-------------------------------------

GCC is here to stay. It optimizes better and Clang has more bugs. Don't take
my word for it. Use both tools on a daily basis like I do and find out for
yourself.