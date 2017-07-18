#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "app.h"
#include "common.h"
#include "db.h"
#include "draw.h"
#include "event.h"
#include "ipc.h"
#include "log.h"
#include "proto.h"
#include "pybindings.h"
#include "spawn.h"
#include "websocket_data.h"
#include "websocket_http.h"

#define WEBSOCKET_PORT	1234
#define APP_PORT	4000

static void init_master(char *argv0)
{
	check(event_init());
	log_init("<master>");
	spawn_init(argv0);
	check(db_init());
	check(proto_server_init(APP_PORT));
	check(websocket_http_init(WEBSOCKET_PORT));
}

static void init_child(char *arg)
{
	check(event_init());
	log_init(arg);
	check(ipc_client_init());
	proto_client_init(arg, event_quit);
	websocket_init(app_remote_command, proto_cond_close);
	draw_init();
}

void help(char *argv0)
{
	printf(
		"Usage: %s [OPTION]...\n"
		"\n"
		"  -i, --interactive    start interactive Python 3 session\n"
		"  -h, --help           this help\n",
		argv0
	    );
}

int main(int argc, char **argv)
{
	static const struct option longopts[] = {
		{ "interactive", no_argument, NULL, 'i' },
		{ "help", no_argument, NULL, 'h' },
	};
	int opt;
	bool opt_interactive = false;

	while ((opt = getopt_long(argc, argv, "ish", longopts, NULL)) >= 0) {
		switch (opt) {
		case 'i':
			opt_interactive = true;
			break;
		case 'h':
			help(argv[0]);
			return 0;
		case '?':
			return 1;
		}
	}

	if (opt_interactive) {
		log_init("<python>");
		pyb_interactive();
		return 0;
	}

	if (optind < argc)
		init_child(argv[optind]);
	else
		init_master(argv[0]);

	log_info("started");
	check(event_loop());
	log_info("terminating cleanly");
	return 0;
}
