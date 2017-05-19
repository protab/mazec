#include "common.h"
#include "db.h"
#include "event.h"
#include "log.h"
#include "spawn.h"

static void init_master(int argc, char **argv)
{
	check(event_init());
	check(log_init("<master>"));
	check(spawn_init(argc, argv));
	check(db_reload());
}

static void init_child(int argc __unused, char **argv)
{
	check(event_init());
	check(log_init(argv[1]));
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
