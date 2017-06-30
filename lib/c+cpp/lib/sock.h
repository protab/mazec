/**
 * Sockety se liší na Windows a na UNIXu. Tato pod-knihovnička se stará
 * o jejich rozdíly.
 *
 * Všechny funkce vracející int vrací error code:
 *  = 0 v případě úspěchu,
 * != 0 v případě chyby
 */

typedef struct sock {
	int fd;
	const char *msg;
} *sock_t;


/* Inicializace a očista - před a po programu (kvůli Windows) */
int sock_init(void);
int sock_cleanup(void);

/* otevření socketu, připojení se */
int sock_open(sock_t sock, const char *host, int port);

/* uzavření socketu */
int sock_close(sock_t sock);

/**
 * Bylo by strašně fajn pro čtení a psaní používat
 * read a write (a tím i buffered io),
 * jenže winsockety to neumí pořádně.
 */

/* odeslání zprávy - vrátí počet odeslaných bytů */
ssize_t sock_send(sock_t sock, const void *msg, size_t size);

/* přijetí zprávy - vrátí počet přijatých bytů */
ssize_t sock_recv(sock_t sock, void *msg, size_t size);

#ifndef _WIN32

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

/**
 * Předpokládame, že msg jsou konstantní a není třeba je uvolňovat.
 */
int sock_err(sock_t sock, const char *msg) {
	sock->msg = msg;
	return 1;
}

int sock_init(void) { return 0; }
int sock_cleanup(void) { return 0; }

int sock_open(sock_t sock, const char *host, int port) {
	struct sockaddr_in addr;
	struct addrinfo hints, *results, *rp;
	int fd;
	int err;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if ((err = getaddrinfo(host, NULL, &hints, &results)))
		return sock_err(sock, gai_strerror(err));

	for (rp = results; rp; rp = rp->ai_next) {
		if ((fd = socket(AF_INET, SOCK_STREAM, PROTO_TCP)) < 0)
			continue;

		if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0)
			break;

		close(fd);
	}

	freeaddrinfo(info);

	if (rp == NULL)
		return sock_err(sock, "Nelze se připojit k serveru.");

	sock->fd = fd;
	return 0;
}

int sock_close(sock_t sock) {
	return close(sock->fd) == 0;
}

ssize_t sock_send(sock_t sock, const void *msg, size_t size) {
	return send(sock->fd, msg + sent, size - sent, MSG_NOSIGNAL);
}

ssize_t sock_recv(sock_t sock, void *buf, size_t size) {
	return recv(sock->fd, buf, size, 0);
}

#else

#endif
