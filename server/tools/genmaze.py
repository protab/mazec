#!/usr/bin/python3
import random
import sys

try:
    width = int(sys.argv[1])
    height = int(sys.argv[2])
except (IndexError, ValueError):
    sys.stderr.write("Usage: {} width height\n".format(sys.argv[0]))
    sys.exit(1)

if width % 2 == 0 or height % 2 == 0:
    sys.stderr.write("Error: width and height must be odd\n")
    sys.exit(1)


def coords(x, y):
    return y * width + x

maze = [2] * (width * height)
stack = []
x, y = 1, 1
dirs = [(-1, 0), (1, 0), (0, -1), (0, 1)]
maze[coords(x, y)] = 0

while True:
    random.shuffle(dirs)
    found = False
    for dx, dy in dirs:
        nx = x + 2 * dx
        ny = y + 2 * dy
        if nx >= 0 and nx < width and ny >= 0 and ny < height and maze[coords(nx, ny)] == 2:
            stack.append((x, y))
            maze[coords(x + dx, y + dy)] = 0
            maze[coords(nx, ny)] = 0
            x, y = nx, ny
            found = True
            break
    if not found:
        if not stack:
            break
        x, y = stack.pop()

print("""#define WIDTH\t{}
#define HEIGHT\t{}
static const unsigned char level[WIDTH * HEIGHT] = {{""".format(width, height))
for y in range(width):
    s = "\t"
    for x in range(height):
        s += "{}, ".format(maze[coords(x, y)])
    print(s.rstrip())
print("};")
