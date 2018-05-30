import sys, readchar, maze

if len(sys.argv) < 3:
    print('Usage: {} login code [server]'.format(sys.argv[0]))
    sys.exit(1)

try:
    if len(sys.argv) >= 4:
        c = maze.Connect(sys.argv[1], sys.argv[2], sys.argv[3])
    else:
        c = maze.Connect(sys.argv[1], sys.argv[2])

    while True:
        key = readchar.readchar()
        if key == '\x03':
            break
        if not c.move(key):
            print(c.error)

except maze.MazeException as e:
    print(e)
