#include <maze.h>
#include <maze_socket.h>

#include <WinSock2.h>
#include <WS2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

//#include <errno.h>
//#include <sys/types.h>
//#include <sys/socket.h>
//#include <netdb.h>
//#include <unistd.h>

struct maze_socket {
  maze_t *maze;
  SOCKET sock;
};

static int maze_socket_init = 0;
static WSADATA wsaData;

static void maze_socket_do_init(maze_t *m) {
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		maze_throw(m, "Nelze inicializovat WinSock");
	maze_socket_init = 1;
}

struct maze_socket *maze_socket_open(maze_t *m, const char *host, const char *port) {
	if (!maze_socket_init)
		maze_socket_do_init(m);
	struct maze_socket *sock = malloc(sizeof(struct maze_socket));
	if (!sock)
		maze_throw(m, "Nelze alokovat paměť pro síťovou komunikaci");

	sock->maze = m;

	struct addrinfo hints, *results, *rp;
	ZeroMemory(&hints, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	int err;
	if ((err = getaddrinfo(host, port, &hints, &results)))
		maze_throw(m, gai_strerror(err));

	for (rp = results; rp; rp = rp->ai_next) {
		if ((sock->sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
			continue;

		if (connect(sock->sock, rp->ai_addr, rp->ai_addrlen) != SOCKET_ERROR)
			break;

		closesocket(sock->sock);
	}

	freeaddrinfo(results);

	if (sock->sock == INVALID_SOCKET)
		maze_throw(m, "Nelze se připojit k serveru.");

	return sock;
}

void maze_socket_close(struct maze_socket *sock) {
	shutdown(sock->sock, SD_BOTH);
	closesocket(sock->sock);
	free(sock);
}

int maze_socket_read(struct maze_socket *sock, char *msg, size_t size) {
	int rv = recv(sock->sock, msg, size, 0);
	if (rv < 0)
		maze_throw(sock->maze, strerror(errno));

	return rv;
}

int maze_socket_write(struct maze_socket *sock, const char *msg, size_t size) {
	int rv = send(sock->sock, msg, size, 0);
	if (rv < 0)
		maze_throw(sock->maze, strerror(errno));

	return rv;
}
