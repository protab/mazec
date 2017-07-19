#include <maze.h>
#include <maze_socket.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAZE_FLAG_CONSTRUCTOR	1
#define MAZE_FLAG_RECV_INT	2
#define MAZE_FLAG_RECV_END	4
#define BUFSIZE	256

struct maze {
  struct maze_socket *sock;
  void (*error_handler)(maze_t *m, const char *msg);
  unsigned cmd_sent;
  unsigned cmd_recv;
  unsigned flags;
  unsigned height, width;
  char *ptr, *end;
  char buf[256];
};

void maze_default_error_handler(maze_t *m, const char *msg) {
  printf("Chyba: %s\n", msg);
  abort();
}

void maze_throw(maze_t *m, const char *msg) {
  void (*error_handler)(maze_t *m, const char *msg) = m->error_handler ? m->error_handler : maze_default_error_handler;
  if (m->flags & MAZE_FLAG_CONSTRUCTOR) {
    free(m);
    error_handler(NULL, msg);
  } else
    error_handler(m, msg);

  abort();
}

#define CHECK(fun) do { const char *e = fun; if (e) maze_throw(m, e); } while (0)


void maze_raw_send(maze_t *m, const char *cmd) {
  if (m->cmd_sent > m->cmd_recv)
    maze_throw(m, "Nepřečetl sis výstup předchozího příkazu");

  int len = strlen(cmd);
  char buf[64];
  memcpy(buf, cmd, len);
  buf[len] = '\n';
  buf[len+1] = '\0';
  int wb = len+1, wa = 0;
  while (wa < wb)
    wa += maze_socket_write(m->sock, buf + wa, wb - wa);

  m->cmd_sent++;
}

const char *maze_raw_recv(maze_t *m) {
  if (m->flags & MAZE_FLAG_RECV_INT)
    maze_throw(m, "Nelze míchat metody čtení");
  if (m->cmd_sent == m->cmd_recv)
    return m->buf;

  /* sesypání bufferu na začátek */
  if (m->ptr == m->end)
    m->ptr = m->end = m->buf;
  else if (m->ptr != m->buf) {
    int amount = m->end - m->ptr;
    memmove(m->buf, m->ptr, amount);
    m->ptr = m->buf;
    m->end = m->buf + amount;
  }

  /* čtení ze socketu */
  while (m->end - m->buf < BUFSIZE - 1) {
    m->end += maze_socket_read(m->sock, m->end, BUFSIZE - 1 - (m->end - m->buf));
    while (m->ptr < m->end) {
      if (*m->ptr == '\n') { /* přečten konec řádku */
	*m->ptr = 0; /* nahradit nulou */
	m->ptr++; /* nyní ukazuje na další řádek, nebo na konec přečtených dat */
	m->cmd_recv++;
	return m->buf;
      }
      m->ptr++; /* zkusit další znak */
    }
  }

  /* pokud jsme se dostali sem, máme přečteno (BUFSIZE - 1) znaků na stále stejném řádku */
  *m->end = 0;
  return m->buf;
}

void maze_raw_recv_start(maze_t *m) {
  m->end += maze_socket_read(m->sock, m->end, BUFSIZE - 1 - (m->end - m->buf));
  *m->end = 0;

  if (strncmp("DATA ", m->ptr, 5) != 0)
    maze_throw(m, "Server neposlal DATA");

  m->flags |= MAZE_FLAG_RECV_INT;
  m->ptr += 5;
}

int maze_raw_recv_int(maze_t *m) {
  if (!(m->flags & MAZE_FLAG_RECV_INT))
    maze_throw(m, "Čtení jednotlivých čísel se musí inicializovat");

  if (m->flags & MAZE_FLAG_RECV_END)
    maze_throw(m, "Už nemám další data");

  for (int i=0; i<16; i++) { /* Limitovaný počet pokusů. */
    /* Mám v bufferu celé číslo, nebo je rozseknuté? */
    for (const char *ws = m->ptr; ws < m->end; ws++) {
      if ((*ws == ' ') || (*ws == '\n')) /* Nalezen konec čísla. */
	goto scan_num;
    }
    /* Nenalezen konec čísla, musím přinačíst data. */

    /* Vejdou se mi tam vůbec další data? */
    if ((m->ptr == m->buf) && (m->end == m->buf + BUFSIZE))
      maze_throw(m, "Absurdně dlouhé číslo na vstupu");

    /* Sesypání dat na začátek a přinačtení dalších */
    if ((m->ptr > m->buf) && (m->end > m->ptr)) {
      int amount = m->end - m->ptr;
      memcpy(m->buf, m->ptr, amount);
      m->ptr = m->buf;
      m->end = m->buf + amount;
    }

    m->end += maze_socket_read(m->sock, m->end, BUFSIZE - 1 - (m->end - m->buf));
    *m->end = 0;
  }

  /* Ani po 16 pokusech o donačtení nenalezen konec čísla. */
  maze_throw(m, "Došla mi trpělivost.");

  /* Teď máme mezi m->ptr a m->end dostatek dat. */
scan_num:
  errno = 0;
  char *ep;
  int out = strtol(m->ptr, &ep, 10);
  if (errno)
    maze_throw(m, strerror(errno));

  /* Kontrola syntaxe */
  if ((*ep == '\n') || (*ep == ' '))
    m->ptr = ep + 1;
  else
    maze_throw(m, "Za číslem není mezera ani konec řádku");

  /* Konec dat */
  if (*ep == '\n')
    m->flags |= MAZE_FLAG_RECV_END;

  return out;
}

void maze_raw_recv_flush(maze_t *m) {
  if (m->cmd_sent == m->cmd_recv)
    maze_throw(m, "Nemám co uklízet");

  if (m->flags & MAZE_FLAG_RECV_END) {
clean:
    m->flags &= ~(MAZE_FLAG_RECV_INT | MAZE_FLAG_RECV_END);
    m->cmd_recv++;
    return;
  }

  while (1) {
    for (char *ws = m->ptr; ws < m->end; ws++)
      if (*ws == '\n') {
	m->ptr = ws+1;
	goto clean;
      }

    m->end = m->buf + maze_socket_read(m->sock, m->buf, BUFSIZE - 1);
  }
}

/*
 * Pomocné funkce pro zkontrolování, jestli přišlo DONE nebo DATA.
 */
static void maze_check_done(maze_t *m) {
  if (strcmp("DONE", m->buf))
    maze_throw(m, "Očekávám DONE");
}

static void maze_check_data(maze_t *m) {
  if (strncmp("DATA ", m->buf, 5))
    maze_throw(m, "Server neposlal DATA");
}

/*
 * Pomocná funkce pro přečtení řádku ve formátu "DATA <int>"
 */
static int maze_get_int(maze_t *m) {
  maze_check_data(m);

  int num;
  int res = sscanf(m->buf+5, "%d", &num);

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
int maze_cmd(maze_t *m, const char *cmd) {
  maze_raw_send(m, cmd);
  maze_raw_recv(m);

  if (strncmp("OVER ", m->buf, 5) == 0)
    maze_throw(m, m->buf + 5);

  return 0;
}

/*
 * Nastavuje uživatele. Normálně se tohle volá z maze_new() nebo maze_run().
 */
static void maze_user(maze_t *m, const char *user) {
  char buf[64];
  if (snprintf(buf, sizeof(buf), "USER %s", user) >= sizeof(buf))
    maze_throw(m, "Moc dlouhé jméno uživatele");

  maze_cmd(m, buf);
  maze_check_done(m);
}

/*
 * Nastavuje level. Normálně se tohle volá z maze_new() nebo maze_run().
 */
static void maze_level(maze_t *m, const char *level) {
  char buf[64];
  if (snprintf(buf, sizeof(buf), "LEVL %s", level) >= sizeof(buf))
    maze_throw(m, "Moc dlouhé jméno levelu");

  maze_cmd(m, buf);
  maze_check_done(m);
}

void maze_wait(maze_t *m) {
  maze_cmd(m, "WAIT");
  maze_check_done(m);
}

int maze_width(maze_t *m) {
  if (m->width > 0)
    return m->width;

  maze_cmd(m, "GETW");
  m->width = maze_get_int(m);
  return m->width;
}

int maze_height(maze_t *m) {
  if (m->height > 0)
    return m->height;

  maze_cmd(m, "GETH");
  m->height = maze_get_int(m);
  return m->height;
}

int maze_x(maze_t *m) {
  maze_cmd(m, "GETX");
  return maze_get_int(m);
}

int maze_y(maze_t *m) {
  maze_cmd(m, "GETY");
  return maze_get_int(m);
}

int maze_what(maze_t *m, int x, int y) {
  char buf[64];
  sprintf(buf, "WHAT %d %d", x, y);
  maze_cmd(m, buf);
  return maze_get_int(m);
}

int *maze_maze(maze_t *m) {
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

const char *maze_move(maze_t *m, char c) {
  if (c == 0 || c == '\n')
    maze_throw(m, "Tento pohyb neumím poslat.");

  char buf[7];
  strcpy(buf, "MOVE ");
  buf[5] = c;
  buf[6] = 0;
  maze_cmd(m, buf);

  if (!strncmp("NOPE ", m->buf, 5))
    return m->buf+5;

  maze_check_done(m);
  return NULL;
}

maze_t *maze_new(const char *server, const char *port, const char *user, const char *level, void (*error_handler)(maze_t *m, const char *msg)) {
  maze_t *m = malloc(sizeof(struct maze));
  if (!m)
    if (error_handler)
      error_handler(NULL, "Nelze alokovat paměť");
    else {
      printf("Chyba: Nelze alokovat paměť\n");
      exit(1);
    }

  memset(m, 0, sizeof(struct maze));
  m->ptr = m->end = m->buf;
  
  m->flags |= MAZE_FLAG_CONSTRUCTOR;

  m->error_handler = error_handler;
  m->sock = maze_socket_open(m, server, port);

  maze_user(m, user);
  maze_level(m, level);

  m->flags &= ~MAZE_FLAG_CONSTRUCTOR;

  return m;
}

void maze_close(maze_t *m) {
  maze_socket_close(m->sock);
  free(m);
}

