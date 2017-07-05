from functools import wraps
from level import *

class BaseLevel:
    """The base level class. Create a subclass and register it using
       set_level(code, class). For multiple connections, each connection
       gets its own object.

       The base class has two class attributes:

       max_conn: Specifies the maximum number of concurrent connections to this
                 level.
       max_time: Specifies the maximum number of seconds that are available
                 to solve the level.

       Subclasses must define these attributes, either as class or object
       attributes:

       x: The current position, x axis (usually an object attribute).
       y: The current position, y axis (usually an object attribute).
       w: Width of the playing field (usually a class attribute).
       h: Height of the playing field (usually a class attribute).

       If a dynamic behavior is needed, use @property. Note though that the
       values of w and h must remain constant.

       In addition, the subclass may define this attribute (again, either as
       a class or object attribute or a property):

       allowed_moves: An iterable (e.g. a string, list or tuple) of
                      characters that are allowed for the move method.
                      Characters other than those will be rejected
                      automatically."""

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


def simpleredraw(func):
    """A decorator for draw method of levels that don't support multiple
       connections or use just the first connection for drawing. Use this
       decorator to turn the redraw class method into an object method.

       Example:

       class Level(BaseLevel):
           @simpleredraw
           def draw(self):
               ..."""
    @wraps(func)
    def wrapper(cls, objs):
        return func(objs[0])
    return wrapper


COLOR_NONE = 0
COLOR_PLAYER = 1
COLOR_WALL = 2
COLOR_TREASURE = 3
COLOR_KEY = 4
COLOR_DOOR = 5
COLOR_CROWN = 6
COLOR_GOLD = 7
COLOR_MONEY = 8
COLOR_CHALICE = 9
