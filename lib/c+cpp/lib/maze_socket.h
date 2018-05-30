/* Ochrana proti vícenásobnému #include */
#ifndef LIBMAZE_SOCKET_H
#define LIBMAZE_SOCKET_H

#include <maze.h>

/**
 * Sockety se liší na Windows a na UNIXu. Tato pod-knihovnička se stará
 * o jejich rozdíly.
 */

struct maze_socket;

/* Inicializace a očista - před a po programu (kvůli Windows). Při úspěchu vrací 0. */
int sock_init(void);
int sock_cleanup(void);

/* otevření socketu, připojení se */
struct maze_socket *maze_socket_open(maze_t *m, const char *host, const char *port);

/* uzavření socketu */
void maze_socket_close(struct maze_socket *sock);

/* odeslání zprávy */
int maze_socket_write(struct maze_socket *sock, const char *msg, size_t size);

/* přijetí zprávy */
int maze_socket_read(struct maze_socket *sock, char *msg, size_t size);

#endif
