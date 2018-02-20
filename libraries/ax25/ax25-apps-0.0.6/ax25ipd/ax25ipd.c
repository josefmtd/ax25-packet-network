/* ax25ipd.c     main entrypoint
 *
 * Copyright 1991, Michael Westerhof, Sun Microsystems, Inc.
 * This software may be freely used, distributed, or modified, providing
 * this header is not removed.
 *
 */

/*
 * cleaned up and prototyped for inclusion into the standard linux ax25
 * toolset in january 1997 by rob mayfield, vk5xxx/vk5zeu
 */

#include <stdio.h>
#include <signal.h>
#include <setjmp.h>
#include <syslog.h>

#include <netax25/daemon.h>
#include <config.h>
#include <getopt.h>

#include "../pathnames.h"
#include "ax25ipd.h"

jmp_buf restart_env;

/* Prototypes */
void hupper(int);

int opt_version = 0;
int opt_loglevel = 0;
int opt_nofork = 0;
int opt_help = 0;
char opt_configfile[1024];

struct option options[] = {
	"version", 0, &opt_version, 1,
	"loglevel", 1, &opt_loglevel, 1,
	"help", 0, &opt_help, 1,
	"configfile", 1, NULL, 0,
        "nofork", 0, &opt_nofork, 1,
	0, 0, 0, 0
};

int main(int argc, char **argv)
{
	if (setjmp(restart_env) == 0) {
		signal(SIGHUP, hupper);
	}

	/* set up the handler for statistics reporting */
	signal(SIGUSR1, usr1_handler);
	signal(SIGINT, int_handler);
	signal(SIGTERM, term_handler);

	while (1) {
		int option_index = 0;
		int c;

		c = getopt_long(argc, argv, "c:fhl:v", options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 0:
			break;
			switch (option_index) {
			case 0:
				break;
			case 1:
				opt_loglevel = atoi(optarg);
				break;
			case 2:
				break;
			case 3:
				strncpy(opt_configfile, optarg, 1023);
				break;
			}
			break;
		case 'c':
			strncpy(opt_configfile, optarg, 1023);
			break;
                case 'f':
                        opt_nofork = 1;
		case 'v':
			opt_version = 1;
			break;
		case 'l':
			opt_loglevel = atoi(optarg);
			break;
		}
	}

	if (optind < argc) {
		printf("config %s\n", argv[optind++]);
	}

	if (opt_version == 1) {
		greet_world();
		exit(0);
	}
	if (opt_help == 1) {
		greet_world();
		printf("Usage:\n");
		printf("%s [flags]\n", argv[0]);
		printf("\nFlags:\n");
		printf
		    ("  --version, -v               Print version of program\n");
		printf("  --help, -h                  This help screen\n");
		printf
		    ("  --loglevel NUM, -l NUM      Set logging level to NUM\n");
		printf
		    ("  --configfile FILE, -c FILE  Set configuration file to FILE\n");
                printf
                    ("  --nofork, -f                Do not put daemon in background\n");
		exit(0);
	}

	/* Initialize all routines */
	config_init();
	kiss_init();
	route_init();
	process_init();
	io_init();

	/* read config file */
	config_read(opt_configfile);

	/* print the current config and route info */
	dump_config();
	dump_routes();
	dump_params();

	/* Open the IO stuff */
	io_open();

	/* if we get this far without error, let's fork off ! :-) */
        if (opt_nofork == 0) {
	    if (!daemon_start(TRUE)) {
	    	    syslog(LOG_DAEMON | LOG_CRIT, "ax25ipd: cannot become a daemon\n");
		    return 1;
	    }
        }

	/* and let the games begin */
	io_start();

	return (0);
}


void greet_world()
{
	printf("\nax25ipd %s / %s\n", VERS2, VERSION);
	printf
	    ("Copyright 1991, Michael Westerhof, Sun Microsystems, Inc.\n");
	printf
	    ("This software may be freely used, distributed, or modified, providing\nthis header is not removed\n\n");
	fflush(stdout);
}

void do_stats()
{
	int save_loglevel;

/* save the old loglevel, and force at least loglevel 1 */
	save_loglevel = loglevel;
	loglevel = 1;

	printf("\nSIGUSR1 signal: statistics and configuration report\n");

	greet_world();

	dump_config();
	dump_routes();
	dump_params();

	printf("\nInput stats:\n");
	printf("KISS input packets:  %d\n", stats.kiss_in);
	printf("           too big:  %d\n", stats.kiss_toobig);
	printf("          bad type:  %d\n", stats.kiss_badtype);
	printf("         too short:  %d\n", stats.kiss_tooshort);
	printf("        not for me:  %d\n", stats.kiss_not_for_me);
	printf("  I am destination:  %d\n", stats.kiss_i_am_dest);
	printf("    no route found:  %d\n", stats.kiss_no_ip_addr);
	printf("UDP  input packets:  %d\n", stats.udp_in);
	printf("IP   input packets:  %d\n", stats.ip_in);
	printf("   failed CRC test:  %d\n", stats.ip_failed_crc);
	printf("         too short:  %d\n", stats.ip_tooshort);
	printf("        not for me:  %d\n", stats.ip_not_for_me);
	printf("  I am destination:  %d\n", stats.ip_i_am_dest);
	printf("\nOutput stats:\n");
	printf("KISS output packets: %d\n", stats.kiss_out);
	printf("            beacons: %d\n", stats.kiss_beacon_outs);
	printf("UDP  output packets: %d\n", stats.udp_out);
	printf("IP   output packets: %d\n", stats.ip_out);
	printf("\n");

	fflush(stdout);

/* restore the old loglevel */
	loglevel = save_loglevel;
}

void hupper(int i)
{
	printf("\nSIGHUP!\n");
	longjmp(restart_env, 1);
}

void usr1_handler(int i)
{
	printf("\nSIGUSR1!\n");
	do_stats();
}

void int_handler(int i)
{
	printf("\nSIGINT!\n");
	do_stats();
	exit(1);
}

void term_handler(int i)
{
	printf("\nSIGTERM!\n");
	do_stats();
	exit(1);
}
