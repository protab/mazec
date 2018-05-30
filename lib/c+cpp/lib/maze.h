/* Ochrana proti vícenásobnému #include */
#ifndef LIBMAZE_MAZE_H
#define LIBMAZE_MAZE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct maze maze_t;

/*
 * maze_raw_send: pošli serveru příkaz
 ** při chybě volá error_handler
 *
 * maze_raw_recv: vrátí pointer na poslední přečtenou odpověď serveru
 ** po poslání příkazu MAZE se chová nedefinovaně
 ** možno volat opakovaně a vždy se vrátí stejný pointer
 ** po poslání dalšího příkazu je původní pointer neplatný
 ** při chybě volá error_handler
 * 
 * Uživatel nesmí poslat další příkaz před přečtením odpovědi na předchozí příkaz.
 */
void maze_raw_send(maze_t *m, const char *cmd);
const char *maze_raw_recv(maze_t *m);

// Vyhoď výjimku
void maze_throw(maze_t *m, const char *msg);

// Pošli libovolný příkaz, při chybě zavolej error_handler
int maze_cmd(maze_t *m, const char *cmd);

// Čekej na odkliknutí uživatelem.
void maze_wait(maze_t *m);

// Vrátí šířku levelu. Příkaz se pošle jen jednou, pak čte uloženou hodnotu.
int maze_width(maze_t *m);

// Vrátí výšku levelu. Příkaz se pošle jen jednou, pak čte uloženou hodnotu.
int maze_height(maze_t *m);

// Vrátí souřadnici X aktuální pozice.
int maze_x(maze_t *m);

// Vrátí souřadnici Y aktuální pozice.
int maze_y(maze_t *m);

// Vrátí obsah zadaných souřadnic.
int maze_what(maze_t *m, int x, int y);

// Vrátí obsah zadaných souřadnic z dat uložených pomocí maze_maze().
#define MAZE_WHAT(m, data, x, y) data[maze_width(m) * y + x]

// Vrátí celé bludiště v čerstvě alokovaném poli. Nutno uvolnit pomocí free();
int *maze_maze(maze_t *m);

// Provede pohyb; vrátí text za NOPE, nebo NULL při úspěchu.
const char *maze_move(maze_t *m, char c);

/*
 * Připojení a registrace error_handler-u.
 * Vrátí objekt připojení k serveru, který se pak předává dalším obslužným funkcím.
 * Error handler se může zavolat i z tohoto konstruktoru; v takovém případě dostane jako první argument NULL.
 */
maze_t *maze_new(const char *server, const char *port, const char *user, const char *level, void (*error_handler)(maze_t *m, const char *msg));

/*
 * Zavření připojení a úklid.
 * Po návratu z maze_close() se stává pointer m invalidním.
 */
void maze_close(maze_t *m);

/*
 * Definice pro maze_loop a maze_run.
 */
#define MAZE_DONE	-1
#define MAZE_KEEP	-2

/*
 * Callbacková smyčka pro maze_run.
 */
static inline maze_t *maze_loop(maze_t *m, char (*callback)(maze_t *m, const char *nope)) {
  const char *nope = NULL;
  for (int c; c = callback(m, nope); )
    switch (c) {
      case MAZE_DONE:
	maze_close(m);
	return NULL;
      case MAZE_KEEP:
	return m;
      default:
	nope = maze_move(m, c);
    }

  return m;
}

/*
 * Připojení, registrace error handler-u a obslužného callbacku.
 * Opakovaně volá callback; vstupem callbacku jsou:
 ** objekt připojení
 ** hláška NOPE; pokud přišlo DONE, předává se NULL
 * výstup callbacku se posílá serveru jako příkaz MOVE.
 * Pokud vrátí callback MAZE_DONE, cyklus skončí a připojení uzavře.
 * Pokud vrátí callback MAZE_KEEP, cyklus skončí a vrátí nezavřený objekt připojení.
 */
static inline maze_t *maze_run(const char *server, const char *port, const char *user, const char *level, void (*error_handler)(maze_t *m, const char *msg), char (*callback)(maze_t *m, const char *nope)) {
  maze_t *m = maze_new(server, port, user, level, error_handler);
  return maze_loop(m, callback);
}

#endif
