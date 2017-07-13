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
  int cmd_sent;
  int cmd_recv;
  unsigned int flags;
  int height, width;
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

void maze_raw_send(maze_t *m, const char *cmd) {
  if (m->cmd_sent > m->cmd_recv)
    maze_throw(m, "Nepřečetl sis výstup předchozího příkazu");

  int wb = strlen(cmd), wa = 0;
  while (wa < wb)
    wa += maze_socket_write(m->sock, cmd + wa, wb - wa);

  while (maze_socket_write(m->sock, "\n", 1) < 1);

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

int maze_setw_(maze_t *m, int w) {
  return m->width = w;
}

int maze_getw_(maze_t *m) {
  return m->width;
}

int maze_seth_(maze_t *m, int h) {
  return m->height = h;
}

int maze_geth_(maze_t *m) {
  return m->height;
}
