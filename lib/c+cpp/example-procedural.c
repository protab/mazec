#include <maze.h>
#include <stdio.h>

#define USER	"karel"
#define LEVEL	"bagr"

#define MAZE_SERVER_NAME	"localhost"
#define MAZE_SERVER_PORT	"4000"

int main(void) {
  maze_t *m = maze_new(MAZE_SERVER_NAME, MAZE_SERVER_PORT, USER, LEVEL, NULL);

  printf("Level je vysoký %u a široký %u\n", maze_height(m), maze_width(m));
  printf("Jsem na souřadnicích %d a %d\n", maze_x(m), maze_y(m));

  printf("Na souřadnicích (5, 10) je %d\n", maze_what(m, 5, 10));

  // ... a stáhni si jej celý.
  int *level_data = maze_maze(m);

  // Pomůcka pro získání políčka ze staženého levelu.
  printf("Na souřadnicích (5, 10) je %d\n", MAZE_WHAT(m, level_data, 5, 10));
  // Vrácení paměti. Za tímto řádkem nesmíš přistupovat do proměnné level_data.
  free(level_data);

  // Počkáme, než nás odklikne frontend.
  maze_wait(m);

  // Pošleme pohyb.
  const char *msg = maze_move(m, 'W');
  if (msg) {
    printf("Pohyb odmítnut: %s\n", msg);
  } else {
    printf("Pohyb W proveden v pořádku.\n");
  }

  // Ukončíme spojení.
  maze_close(m);
}
