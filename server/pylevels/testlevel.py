from mazec import *

class FallTimer(Timer):
    def __init__(self, obj):
        self.obj = obj

    def fired(self, count):
        self.obj.y += count
        draw.dirty()


class Level(BaseLevel):
    max_conn = 10
    w = draw.MOD_WIDTH
    h = draw.MOD_HEIGHT
    allowed_moves = 'wasd'

    def __init__(self):
        self.x = draw.MOD_WIDTH // 2
        self.y = 0
        self.timer = FallTimer(self)
        self.timer.arm(1000, True)

    def move(self, key):
        if key == 'w':
            self.y -= 1
        elif key == 's':
            self.y += 1
        elif key == 'a':
            self.x -= 1
        elif key == 'd':
            self.x += 1
        draw.dirty()

    def redraw(cls, objs):
        draw.clear()
        draw.origin(0, 0)
        for o in objs:
            draw.item(o.x * draw.MOD, o.y * draw.MOD, 0, COLOR_PLAYER)

set_level('aaa', Level)
