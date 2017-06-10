#include "generator.h"
#include <stdlib.h>
#include <string.h>
#include "../common.h"
#include "../level.h"

static void shuffle(unsigned char *arr, int len)
{
	for (int i = len - 1; i > 0; i--) {
		int n = random() % (i + 1);
		unsigned char x = arr[n];
		arr[n] = arr[i];
		arr[i] = x;
	}
}

#define coords(x, y)	((y) * width + (x))

unsigned char *generate_maze(int width, int height, int holes)
{
	unsigned char *maze;
	int *stack;
	int stack_len;
	int x, y;
	unsigned char dirs[4] = { 0x10, 0x12, 0x01, 0x21 };

	randomize();

	maze = salloc(width * height);
	memset(maze, COLOR_WALL, width * height);
	stack = salloc(width * height / 4 * sizeof(int));
	stack_len = 0;
	x = 1;
	y = 1;
	maze[coords(x, y)] = COLOR_NONE;

	while (true) {
		bool found = false;

		shuffle(dirs, 4);
		for (int i = 0; i < 4; i++) {
			int dx = (int)(dirs[i] & 0xf) - 1;
			int dy = (int)(dirs[i] >> 4) - 1;
			int nx = x + 2 * dx;
			int ny = y + 2 * dy;

			if (nx >= 0 && nx < width && ny >= 0 && ny < height &&
			    maze[coords(nx, ny)] == COLOR_WALL) {
				stack[stack_len++] = coords(x, y);
				maze[coords(x + dx, y + dy)] = COLOR_NONE;
				maze[coords(nx, ny)] = COLOR_NONE;
				x = nx;
				y = ny;
				found = true;
				break;
			}
		}
		if (!found) {
			if (!stack_len)
				break;
			int xy = stack[--stack_len];
			x = xy % width;
			y = xy / width;
		}
	}

	if (holes > 0) {
		for (int i = width * height / 2 * holes / 1000; i > 0; i--) {
			int nx, ny;

			if (random() % 2) {
				nx = (random() % (width / 2 - 1)) * 2 + 2;
				ny = (random() % (height / 2)) * 2 + 1;
			} else {
				nx = (random() % (width / 2)) * 2 + 1;
				ny = (random() % (height / 2 - 1)) * 2 + 2;
			}
			maze[coords(nx, ny)] = COLOR_NONE;
		}
	}

	sfree(stack);
	return maze;
}
