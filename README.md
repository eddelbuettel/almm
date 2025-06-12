## 'Activate-Linux' (based) Market Monitor

[activate-linux][activate linux] is a very neat little application (and pun!) that puts a
(user-selectable) text very discretely in the bottom right of the desktop. Apparently this is
mocking another OS which posts a 'please activate' message there.

It seemed like the perfect vessel to connect the Redis (or Valkey) pub/sub mechanism in order to (in
a very lightweight manner) update display text with current market index levels and changes. 

You can see it in action in the very short little 'video' recorded off my desktop demonstrating it.
(Note that the video shows variable `SYM`; this should actually be `SYMBOL`. As we use the default
value it does not impact the demo.)

### Demo

Click on the image to see the video:

[![almm demo](https://img.youtube.com/vi/oCjd_qgMM1U/0.jpg)](https://www.youtube.com/watch?v=oCjd_qgMM1U)

### Background

All required files were taken from the [activate-linux][activate linux] repository and remain under
its license, the GPL-3.  All files are copyright by their respective owners.

My changes (currently) consist solely of connecting the app to the [hiredis][hiredis] library in
order to access the [Redis pub/sub framework][pub sub]. A sibbling repository
[redis-pubsub-examples][pub sub examples] contains the sample producer and (other) client shown in
the demo above and may serve as a start with [Redis pub/sub][pub sub] (and I may add more examples
there).

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
