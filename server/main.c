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

static void init_master(char *argv0, bool use_syslog)
{
	log_init("<mazec>", use_syslog);
	check(event_init());
	spawn_init(argv0, use_syslog);
	check(db_init());
	check(proto_server_init(APP_PORT));
	check(websocket_http_init(WEBSOCKET_PORT));
}

static void init_child(char *login, bool use_syslog)
{
	log_init(login, use_syslog);
	check(event_init());
	check(ipc_client_init());
	proto_client_init(login, event_quit);
	websocket_init(app_remote_command, proto_cond_close);
	draw_init();
}

void help(char *argv0)
{
	printf(
		"Usage: %s [OPTION]...\n"
		"\n"
		"  -i, --interactive    start interactive Python 3 session\n"
		"  -s, --syslog         log to syslog instead of stderr\n"
		"  -h, --help           this help\n",
		argv0
	    );
}

int main(int argc, char **argv)
{
	static const struct option longopts[] = {
		{ "interactive", no_argument, NULL, 'i' },
		{ "syslog", no_argument, NULL, 's' },
		{ "help", no_argument, NULL, 'h' },
	};
	int opt;
	bool opt_interactive = false, opt_syslog = false;

	while ((opt = getopt_long(argc, argv, "ish", longopts, NULL)) >= 0) {
		switch (opt) {
		case 'i':
			opt_interactive = true;
			break;
		case 's':
			opt_syslog = true;
			break;
		case 'h':
			help(argv[0]);
			return 0;
		case '?':
			return 1;
		}
	}

	if (opt_interactive) {
		log_init("<python>", opt_syslog);
		pyb_interactive();
		return 0;
	}

	if (optind < argc)
		init_child(argv[optind], opt_syslog);
	else
		init_master(argv[0], opt_syslog);

	log_info("started");
	check(event_loop());
	log_info("terminating cleanly");
	return 0;
}
