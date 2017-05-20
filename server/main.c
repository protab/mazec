#include "common.h"
#include "db.h"
#include "event.h"
#include "ipc.h"
#include "log.h"
#include "spawn.h"
#include "websocket.h"

#define WEBSOCKET_PORT	1234

static void init_master(int argc, char **argv)
{
	check(event_init());
	check(log_init("<master>"));
	check(spawn_init(argc, argv));
	check(db_reload());
	check(websocket_init(WEBSOCKET_PORT));
}

static void init_child(int argc __unused, char **argv)
{
	check(event_init());
	check(log_init(argv[1]));
	check(websocket_init(0));
	check(ipc_client_init());
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
