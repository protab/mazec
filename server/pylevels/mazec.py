from level import *

class BaseLevel:
    """The base level class. Create a subclass and register it using
       set_level(code, class). The subclass must define x, y, w and h attributes,
       either as class attributes or as object attributes. Use @property if you
       need a dynamic behavior but note that w and h must be constant.

       For multiple connections, each connection gets its own object.

       The base class has two class attributes:

       max_conn: Specifies the maximum number of concurrent connections to this
                 level.
       max_time: Specifies the maximum number of seconds that are available
                 to solve the level."""

    max_conn = 1
    max_time = 0

    def move(self, key):
        """Called to perform a move. The key parameter is a string containing the key
           that was pressed. Can raise Nope, Win or Lose exceptions."""
        raise Nope("Neni podporovano")

    def what(self, x, y):
        """Should return the item on x, y or raise Nope."""
        raise Nope("Neni podporovano")

    def maze(self):
        """Should return the whole maze as an iterable of w * h integers or raise Nope."""
        raise Nope("Neni podporovano")

    @classmethod
    def redraw(cls, objs):
        """Called to redraw the remote screen. May be called even when the level did
           not indicate the screen is dirty, for example when a new client connects.
           May not be called at all, for example when there's no client connected.
           All current connections are passed as objs tuple.
           This must be a class method.

           The available functions for drawing are:
           draw.dirty()
           draw.bank(id)
           draw.clear()
           draw.origin(x, y)
           draw.item(x, y, angle, color)"""


COLOR_NONE = 0
COLOR_PLAYER = 1
COLOR_WALL = 2
COLOR_TREASURE = 3
COLOR_ON = 4
COLOR_OFF = 5
COLOR_TAIL = 6
COLOR_FOOD = 7
