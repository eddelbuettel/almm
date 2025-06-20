## almm: Activate-Linux (based) Market Monitor

[activate-linux][activate linux] is a very neat little application (and pun!) that puts a
(user-selectable) text very discretely in the bottom right of the desktop. Apparently this is
mocking another OS which posts a 'please activate' message there.

It seemed like the perfect vessel to connect the Redis (or Valkey) pub/sub mechanism in order to (in
a very lightweight manner) update display text with current market index levels and changes. 

You can see it in action in the very short little 'video' recorded off my desktop demonstrating it.
(Note that the video shows variable `SYM`; this should actually be `SYMBOL`. As we use the default
value it does not impact the demo.)

### Demos

A shorter and simpler animated gif shows the monitor springing into action once a publisher runs:

![](https://eddelbuettel.github.io/images/2025-06-12/almm_publisher_dot_r_demo_%202025-06-12_08-30.gif)

We also have a slightly longer and more detailed short video with a voice overlay (click on the image to see the video):

[![almm demo](https://img.youtube.com/vi/oCjd_qgMM1U/0.jpg)](https://www.youtube.com/watch?v=oCjd_qgMM1U)

### Background

All required files were taken from the [activate-linux][activate linux] repository and remain under
its license, the GPL-3.  All files are copyright by their respective owners.

My changes (currently) consist solely of connecting the app to the [hiredis][hiredis] library in
order to access the [Redis pub/sub framework][pub sub]. A sibbling repository
[redis-pubsub-examples][pub sub examples] contains the sample producer and (other) client shown in
the demo above and may serve as a start with [Redis pub/sub][pub sub] (and I may add more examples
there).

### Building

It all 'just works' on, _e.g._, Ubuntu if you have all the required packages. On my laptop I needed
to add `libcairo-dev`, `libxfixes-dev`, `libxrandr-dev`, `libxinerama-dev`, `libwayland-bin`,
`libwayland-dev`, `wayland-protocols` in addition to what was already installed (and of course
`libhiredis-dev` and `libevent-dev` for our extension).

It works with either `redis-server` or `valkey-server`.

### Running

See the `activate-linux --help` for available command-line options. Adding `-v` (or `-vv` or `-vvv`)
adds debugging info, while adding font scale or bold font use or ... can aide in tuning the display.

At present, the binary is customized for the personal use case listening to symbols ES1 and SP500
and displaying whichever was most current. That works really well given that SP500 (via symbol
`^GSPC`) updates near real-time but only during standard market hours, whereas ES1 (via symbol
`ES=F` is available almost 24 hours (excluding 15:15h to 17:00h) for five days, each time starting
the prior day (i.e. Sunday afternoon 17:00h open for electrinic trading to Friday 15:15h; all times
Central).

### Author

For the changes in this repo, Dirk Eddelbuettel

For everything in [activate-linux][activate linux], its respective authors (see [contributors][al
authors])

### Licence

For the changes here as well as for everything related to [activate-linux][activate linux]: GPL-3


[activate linux]: https://github.com/MrGlockenspiel/activate-linux
[hiredis]: https://github.com/redis/hiredis
[pub sub]: https://redis.io/docs/latest/develop/interact/pubsub/
[pub sub examples]: https://github.com/eddelbuettel/redis-pubsub-examples
[al authors]: https://github.com/MrGlockenspiel/activate-linux/graphs/contributors
