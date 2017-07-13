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

/*
 * Pomocné funkce pro čtení celého bludiště.
 * Nekombinovat s maze_raw_recv()!
 */
void maze_raw_recv_start(maze_t *m);
int maze_raw_recv_int(maze_t *m);
void maze_raw_recv_flush(maze_t *m);

/*
 * Funkce maze_throw zavolá error_handler se správnými argumenty.
 * Nikdy se nevrátí -- pokud by se error_handler vrátil, skončí celý program
 */
void maze_throw(maze_t *m, const char *msg);

/*
 * Pomocné funkce pro zkontrolování, jestli přišlo DONE nebo DATA.
 */
static inline void maze_check_done(maze_t *m) {
  if (strcmp("DONE", maze_raw_recv(m)))
    maze_throw(m, "Očekávám DONE");
}

static inline void maze_check_data(maze_t *m) {
  if (strncmp("DATA ", maze_raw_recv(m), 5))
    maze_throw(m, "Server neposlal DATA");
}
/*
 * Pomocná funkce pro přečtení řádku ve formátu "DATA <int>"
 */
static inline int maze_get_int(maze_t *m) {
  maze_check_data(m);

  int num;
  int res = sscanf(maze_raw_recv(m)+5, "%d", &num);

  if (res == 1)
    return num;
  if (res == 0)
    maze_throw(m, "Server neposlal číslo");
  if (res == EOF)
    maze_throw(m, "Server neposlal nic");

  maze_throw(m, "Tohle se nikdy nemá stát");
}

/*
 * Pošli libovolný příkaz, při chybě zavolej error_handler
 */
static inline int maze_cmd(maze_t *m, const char *cmd) {
  maze_raw_send(m, cmd);

  const char *response = maze_raw_recv(m);
  if (strncmp("OVER ", response, 5) == 0)
    maze_throw(m, response + 5);

  return 0;
}

/*
 * Nastavuje uživatele. Normálně se tohle volá z maze_new() nebo maze_run().
 */
static inline void maze_user(maze_t *m, const char *user) {
  char buf[64];
  if (snprintf(buf, sizeof(buf), "USER %s", user) >= sizeof(buf))
    maze_throw(m, "Moc dlouhé jméno uživatele");

  maze_cmd(m, buf);
  maze_check_done(m);
}

/*
 * Nastavuje level. Normálně se tohle volá z maze_new() nebo maze_run().
 */
static inline void maze_level(maze_t *m, const char *level) {
  char buf[64];
  if (snprintf(buf, sizeof(buf), "LEVL %s", level) >= sizeof(buf))
    maze_throw(m, "Moc dlouhé jméno levelu");

  maze_cmd(m, buf);
  maze_check_done(m);
}

/*
 * Čeká na odkliknutí uživatelem.
 */
static inline void maze_wait(maze_t *m) {
  maze_cmd(m, "WAIT");
  maze_check_done(m);
}

int maze_setw_(maze_t *m, int w);
int maze_getw_(maze_t *m);
int maze_seth_(maze_t *m, int h);
int maze_geth_(maze_t *m);

static inline int maze_getw(maze_t *m) {
  maze_cmd(m, "GETW");
  return maze_setw_(m, maze_get_int(m));
}

/* 
 * Vrátí šířku levelu. Příkaz se pošle jen jednou, pak čte uloženou hodnotu.
 */
static inline int maze_width(maze_t *m) {
  int out = maze_getw_(m);
  if (out > 0)
    return out;
  else
    return maze_getw(m);
}

static inline int maze_geth(maze_t *m) {
  maze_cmd(m, "GETH");
  return maze_seth_(m, maze_get_int(m));
}

/*
 * Vrátí výšku levelu. Příkaz se pošle jen jednou, pak čte uloženou hodnotu.
 */
static inline int maze_height(maze_t *m) {
  int out = maze_geth_(m);
  if (out > 0)
    return out;
  else
    return maze_geth(m);
}

/*
 * Vrátí souřadnici X aktuální pozice.
 */
static inline int maze_x(maze_t *m) {
  maze_cmd(m, "GETX");
  return maze_get_int(m);
}

/*
 * Vrátí souřadnici Y aktuální pozice.
 */
static inline int maze_y(maze_t *m) {
  maze_cmd(m, "GETY");
  return maze_get_int(m);
}

/*
 * Vrátí obsah zadaných souřadnic.
 */
static inline int maze_what(maze_t *m, int x, int y) {
  char buf[64];
  sprintf(buf, "WHAT %d %d", x, y);
  maze_cmd(m, buf);
  return maze_get_int(m);
}

#define MAZE_WHAT(m, data, x, y) data[maze_width(m) * y + x]

/*
 * Vrátí celé bludiště v čerstvě alokovaném poli. Nutno uvolnit pomocí free();
 */
static inline int *maze_maze(maze_t *m) {
  int size = maze_height(m) * maze_width(m);
  int *data = malloc(sizeof(int) * size);
  if (!data)
    maze_throw(m, "Nelze alokovat paměť pro uložení bludiště!");

  maze_raw_send(m, "MAZE");
  maze_raw_recv_start(m);
  for (int i=0; i<size; i++)
    data[i] = maze_raw_recv_int(m);
  maze_raw_recv_flush(m);

  return data;
}

static inline const char *maze_move(maze_t *m, char c) {
  if (c == 0 || c == '\n')
    maze_throw(m, "Tento pohyb neumím poslat.");

  char buf[7];
  strcpy(buf, "MOVE ");
  buf[5] = c;
  buf[6] = 0;
  maze_cmd(m, buf);

  const char *str = maze_raw_recv(m);
  if (!strncmp("NOPE ", str, 5))
    return str+5;

  maze_check_done(m);
  return NULL;
}

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
