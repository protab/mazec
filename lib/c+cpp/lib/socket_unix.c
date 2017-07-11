#include <maze.h>
#include <maze_socket.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

struct maze_socket {
  maze_t *maze;
  int fd;
};

struct maze_socket *maze_socket_open(maze_t *m, const char *host, const char *port) {
  struct maze_socket *sock = malloc(sizeof(struct maze_socket));
  if (!sock)
    maze_throw(m, "Nelze alokovat paměť pro síťovou komunikaci");

  sock->maze = m;

  struct sockaddr_in addr;
  struct addrinfo hints, *results, *rp;
  int fd;
  int err;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if ((err = getaddrinfo(host, port, &hints, &results)))
    maze_throw(m, gai_strerror(err));

  for (rp = results; rp; rp = rp->ai_next) {
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
      continue;

    if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0)
      break;

    close(fd);
  }

  freeaddrinfo(results);

  if (rp == NULL)
    maze_throw(m, "Nelze se připojit k serveru.");

  sock->fd = fd;
  return sock;
}

void maze_socket_close(struct maze_socket *sock) {
  close(sock->fd);
  free(sock);
}

int maze_socket_read(struct maze_socket *sock, char *msg, size_t size) {
  int rv = read(sock->fd, msg, size);
  if (rv < 0)
    maze_throw(sock->maze, strerror(errno));

  return rv;
}

int maze_socket_write(struct maze_socket *sock, const char *msg, size_t size) {
  int rv = write(sock->fd, msg, size);
  if (rv < 0)
    maze_throw(sock->maze, strerror(errno));

  return rv;
}
