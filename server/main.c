#include "common.h"
#include "db.h"
#include "draw.h"
#include "event.h"
#include "ipc.h"
#include "log.h"
#include "proto.h"
#include "spawn.h"
#include "websocket_data.h"
#include "websocket_http.h"

#define WEBSOCKET_PORT	1234
#define APP_PORT	4000

static void init_master(int argc, char **argv)
{
	check(event_init());
	log_init("<master>");
	spawn_init(argc, argv);
	check(db_init());
	check(proto_server_init(APP_PORT));
	check(websocket_http_init(WEBSOCKET_PORT));
}

static void ws_debug(struct socket *s, void *buf, size_t len)
{
	char *disp = buf;
	char x;

	x = disp[len - 1];
	disp[len - 1] = '\0';
	log_info("got: %s%c", disp, x);
	websocket_write(s, "ack", 3, false);
}

static void init_child(int argc __unused, char **argv)
{
	check(event_init());
	log_init(argv[1]);
	check(ipc_client_init());
	proto_client_init(event_quit);
	websocket_init(ws_debug, proto_cond_close);
	draw_init();
}

int main(int argc, char **argv)
{
	if (argc > 1)
		init_child(argc, argv);
	else
		init_master(argc, argv);
	log_info("started");
	check(event_loop());
	log_info("terminating cleanly");
	return 0;
}
