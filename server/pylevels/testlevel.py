from mazec import *

class Level(BaseLevel):
    w = draw.MOD_WIDTH
    h = draw.MOD_HEIGHT

    def __init__(self):
        self.x = draw.MOD_WIDTH // 2
        self.y = draw.MOD_HEIGHT // 2

    def move(self, key):
        if key == 'w':
            self.y -= 1
        elif key == 's':
            self.y += 1
        elif key == 'a':
            self.x -= 1
        elif key == 'd':
            self.x += 1
        else:
            raise Nope('neznamy smer')
        draw.dirty()

    def redraw(cls, objs):
        draw.clear()
        draw.origin(0, 0)
        for o in objs:
            draw.item(o.x * draw.MOD, o.y * draw.MOD, 0, COLOR_PLAYER)

set_level('aaa', Level)
