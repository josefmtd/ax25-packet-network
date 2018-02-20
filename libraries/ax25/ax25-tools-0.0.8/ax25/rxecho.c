/*
 * rxecho.c - Copies AX.25 packets from an interface to another interface.
 *            Reads CONFIGFILE (see below) and uses that information to
 *            decide what packets should be copied and where.
 *
 *            CONFIGFILE format is:
 *
 *              # this is a comment
 *              144	kiss0	oh2bns-1,oh2bns-2
 *              kiss0	144	*
 *
 *            This means that packets received on port 144 are copied to port
 *            kiss0 if they are destined to oh2bns-1 or oh2bns-2. Packets
 *            from port kiss0 are all copied to port 144.
 *
 *            There may be empty lines and an arbirary amount of white
 *            space around the tokens but the callsign field must not
 *            have any spaces in it. There can be up to MAXCALLS call-
 *            signs in the callsign field (see below).
 *
 * Copyright (C) 1996 by Tomi Manninen, OH2BNS, <tomi.manninen@hut.fi>.
 *
 * *** Modified 9/9/96 by Heikki Hannikainen, OH7LZB, <hessu@pspt.fi>:
 *
 *            One port can actually be echoed to multiple ports (with a
 *            different recipient callsign, of course). The old behaviour was
 *            to give up on the first matching port, even if the recipient
 *            callsign didn't match (and the frame wasn't echoed anywhere).
 *
 * ***
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <sys/types.h>

#include <config.h>

#include <sys/socket.h>

#ifdef __GLIBC__ 
#include <net/ethernet.h>
#else
#include <linux/if_ether.h>
#endif

#include <netinet/in.h>

#ifdef HAVE_NETAX25_AX25_H
#include <netax25/ax25.h>
#else
#include <netax25/kernel_ax25.h>
#endif
#ifdef HAVE_NETROSE_ROSE_H
#include <netrose/rose.h>
#else 
#include <netax25/kernel_rose.h>
#endif

#include <netax25/axlib.h>
#include <netax25/axconfig.h>
#include <netax25/daemon.h>

#include "../pathnames.h"

#define MAXCALLS	8

struct config {
	char		from[14];	/* sockaddr.sa_data is 14 bytes	*/
	char		to[14];
	ax25_address	calls[MAXCALLS];/* list of calls to echo	*/
	int		ncalls;		/* number of calls to echo	*/

	struct config	*next;
};

static int logging = FALSE;

static void terminate(int sig)
{
	if (logging) {
		syslog(LOG_INFO, "terminating on SIGTERM\n");
		closelog();
	}
	
	exit(0);
}

/*
 * Read string "call1,call2,call3,..." into p.
 */
static int read_calls(struct config *p, char *s)
{
	char *cp, *cp1;

	if (p == NULL || s == NULL)
		return -1;

	p->ncalls = 0;

	if (strcmp(s, "*") == 0)
		return 0;

	cp = s;

	while ((cp1 = strchr(cp, ',')) != NULL && p->ncalls < MAXCALLS) {
		*cp1 = 0;

		if (ax25_aton_entry(cp, p->calls[p->ncalls].ax25_call) == -1)
			return -1;

		p->ncalls++;
		cp = ++cp1;
	}

	if (p->ncalls < MAXCALLS) {
		if (ax25_aton_entry(cp, p->calls[p->ncalls].ax25_call) == -1)
			return -1;

		p->ncalls++;
	}

	return p->ncalls;
}

static struct config *readconfig(void)
{
	FILE *fp;
	char line[80], *cp, *dev;
	struct config *p, *list = NULL;

	if ((fp = fopen(CONF_RXECHO_FILE, "r")) == NULL) {
		fprintf(stderr, "rxecho: cannot open config file\n");
		return NULL;
	}

	while (fgets(line, 80, fp) != NULL) {
		cp = strtok(line, " \t\r\n");

		if (cp == NULL || cp[0] == '#')
			continue;

		if ((p = calloc(1, sizeof(struct config))) == NULL) {
			perror("rxecho: malloc");
			return NULL;
		}

		if ((dev = ax25_config_get_dev(cp)) == NULL) {
			fprintf(stderr, "rxecho: invalid port name - %s\n", cp);
			return NULL;
		}

		strcpy(p->from, dev);

		if ((cp = strtok(NULL, " \t\r\n")) == NULL) {
			fprintf(stderr, "rxecho: config file error.\n");
			return NULL;
		}

		if ((dev = ax25_config_get_dev(cp)) == NULL) {
			fprintf(stderr, "rxecho: invalid port name - %s\n", cp);
			return NULL;
		}

		strcpy(p->to, dev);

		if (read_calls(p, strtok(NULL, " \t\r\n")) == -1) {
			fprintf(stderr, "rxecho: config file error.\n");
			return NULL;
		}

		p->next = list;
		list = p;
	}

	fclose(fp);

	if (list == NULL)
		fprintf(stderr, "rxecho: Empty config file!\n");

	return list;
}

/*
 *	Slightly modified from linux/include/net/ax25.h and 
 *	linux/net/ax25/ax25_subr.c:
 */

#if 0
#define C_COMMAND	1
#define C_RESPONSE	2
#define LAPB_C		0x80
#endif
#define LAPB_E		0x01
#define AX25_ADDR_LEN	7
#define AX25_REPEATED	0x80

typedef struct {
	ax25_address calls[AX25_MAX_DIGIS];
	unsigned char repeated[AX25_MAX_DIGIS];
	char ndigi;
	char lastrepeat;
} ax25_digi;

/*
 *	Given an AX.25 address pull of to, from, digi list, and the start of data.
 */
static unsigned char *ax25_parse_addr(unsigned char *buf, int len, ax25_address *src, ax25_address *dest, ax25_digi *digi)
{
	int d = 0;
	
	if (len < 14) return NULL;
		
#if 0
	if (flags != NULL) {
		*flags = 0;
	
		if (buf[6] & LAPB_C) {
			*flags = C_COMMAND;
		}

		if (buf[13] & LAPB_C) {
			*flags = C_RESPONSE;
		}
	}
		
	if (dama != NULL) 
		*dama = ~buf[13] & DAMA_FLAG;
#endif
		
	/* Copy to, from */
	if (dest != NULL) 
		memcpy(dest, buf + 0, AX25_ADDR_LEN);

	if (src != NULL)  
		memcpy(src,  buf + 7, AX25_ADDR_LEN);

	buf += 2 * AX25_ADDR_LEN;
	len -= 2 * AX25_ADDR_LEN;

	digi->lastrepeat = -1;
	digi->ndigi      = 0;
	
	while (!(buf[-1] & LAPB_E)) {
		if (d >= AX25_MAX_DIGIS)  return NULL;	/* Max of 6 digis */
		if (len < 7) return NULL;		/* Short packet */

		if (digi != NULL) {
			memcpy(&digi->calls[d], buf, AX25_ADDR_LEN);
			digi->ndigi = d + 1;

			if (buf[6] & AX25_REPEATED) {
				digi->repeated[d] = 1;
				digi->lastrepeat  = d;
			} else {
				digi->repeated[d] = 0;
			}
		}

		buf += AX25_ADDR_LEN;
		len -= AX25_ADDR_LEN;
		d++;
	}

	return buf;
}

/*
 * Check if frame should be echoed. Return 0 if it should and -1 if not.
 */
static int check_calls(struct config *cfg, unsigned char *buf, int len)
{
	ax25_address dest;
	ax25_digi digi;
	ax25_address *axp;
	int i;

	if ((buf[0] & 0x0F) != 0)
		return -1;			/* don't echo non-data	*/

	if (cfg->ncalls == 0)
		return 0;			/* copy everything	*/

	if (ax25_parse_addr(++buf, --len, NULL, &dest, &digi) == NULL)
		return -1;			/* invalid ax.25 header	*/

	/*
	 * If there are no digis or all digis are already repeated
	 * use destination address. Else use first non-repeated digi.
	 */
	if (digi.ndigi == 0 || digi.ndigi == digi.lastrepeat + 1)
		axp = &dest;
	else
		axp = &digi.calls[digi.lastrepeat + 1];

	for (i = 0; i < cfg->ncalls; i++)
		if (ax25_cmp(&cfg->calls[i], axp) == 0)
			return 0;

	return -1;
}

int main(int argc, char **argv)
{
	struct sockaddr sa;
	int s, size, alen;
	unsigned char buf[1500];
	struct config *p, *list;

	while ((s = getopt(argc, argv, "lv")) != -1) {
		switch (s) {
			case 'l':
				logging = TRUE;
				break;
			case 'v':
				printf("rxecho: %s\n", VERSION);
				return 0;
			default:
				fprintf(stderr, "usage: rxecho [-l] [-v]\n");
				return 1;
		}
	}

	signal(SIGTERM, terminate);

	if (ax25_config_load_ports() == 0) {
		fprintf(stderr, "rxecho: no AX.25 port data configured\n");
		return 1;
	}

	if ((list = readconfig()) == NULL)
		return 1;

	if ((s = socket(AF_INET, SOCK_PACKET, htons(ETH_P_AX25))) == -1) {
		perror("rxecho: socket:");
		return 1;
	}

	if (!daemon_start(FALSE)) {
		fprintf(stderr, "rxecho: cannot become a daemon\n");
		close(s);
		return 1;
	}

	if (logging) {
		openlog("rxecho", LOG_PID, LOG_DAEMON);
		syslog(LOG_INFO, "starting");
	}

	for (;;) {
		alen = sizeof(sa);

		if ((size = recvfrom(s, buf, 1500, 0, &sa, &alen)) == -1) {
			if (logging) {
				syslog(LOG_ERR, "recvfrom: %m");
				closelog();
			}
			return 1;
		}

		for (p = list; p != NULL; p = p->next)
 			if ((strcmp(p->from, sa.sa_data) == 0) && (check_calls(p, buf, size) == 0)) {
 				strcpy(sa.sa_data, p->to);
				if (sendto(s, buf, size, 0, &sa, alen) == -1) {
					if (logging) {
						syslog(LOG_ERR, "sendto: %m");
						closelog();
					}
				}
			}
	}
}
