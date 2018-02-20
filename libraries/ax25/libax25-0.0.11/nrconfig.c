#define _LINUX_STRING_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <config.h>

#include <sys/types.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/if_arp.h>
#ifdef HAVE_NETAX25_AX25_H
#include <netax25/ax25.h>
#else
#include "kernel_ax25.h"
#endif
#ifdef HAVE_NETROSE_ROSE_H
#include <netrose/rose.h>
#else
#include "kernel_rose.h"
#endif

#include "pathnames.h"
#include "axlib.h"
#include "nrconfig.h"

typedef struct _nrport
{
	struct _nrport *Next;
	char *Name;
	char *Call;
	char *Alias;
	char *Device;
	int  Paclen;
	char *Description;
} NR_Port;

static NR_Port *nr_ports       = NULL;
static NR_Port *nr_port_tail   = NULL;

static int ax25_hw_cmp(unsigned char *callsign, unsigned char *hw_addr)
{
	ax25_address call;

	ax25_aton_entry(callsign, call.ax25_call);
	
	return ax25_cmp(&call, (ax25_address *)hw_addr) == 0;
}

static NR_Port *nr_port_ptr(char *name)
{
	NR_Port *p = nr_ports;

	if (name == NULL)
		return p;

	while (p != NULL) {
		if (strcasecmp(name, p->Name) == 0)
			return p;

		p = p->Next;
	}

	return NULL;
}

char *nr_config_get_next(char *name)
{
	NR_Port *p;
	
	if (nr_ports == NULL)
		return NULL;
		
	if (name == NULL)
		return nr_ports->Name;
		
	if ((p = nr_port_ptr(name)) == NULL)
		return NULL;
		
	p = p->Next;

	if (p == NULL)
		return NULL;
		
	return p->Name;
}

char *nr_config_get_name(char *device)
{
	NR_Port *p = nr_ports;

	while (p != NULL) {
		if (strcmp(device, p->Device) == 0)
			return p->Name;

		p = p->Next;
	}

	return NULL;
}

char *nr_config_get_addr(char *name)
{
	NR_Port *p = nr_port_ptr(name);

	if (p == NULL)
		return NULL;

	return p->Call;
}

char *nr_config_get_dev(char *name)
{
	NR_Port *p = nr_port_ptr(name);

	if (p == NULL)
		return NULL;

	return p->Device;
}

char *nr_config_get_port(ax25_address *callsign)
{
	NR_Port *p = nr_ports;
	ax25_address addr;

	while (p != NULL) {
		ax25_aton_entry(p->Call, (char *)&addr);
	
		if (ax25_cmp(callsign, &addr) == 0)
			return p->Name;

		p = p->Next;
	}

	return NULL;
}

char *nr_config_get_alias(char *name)
{
	NR_Port *p = nr_port_ptr(name);

	if (p == NULL)
		return NULL;

	return p->Alias;
}

int nr_config_get_paclen(char *name)
{
	NR_Port *p = nr_port_ptr(name);

	if (p == NULL)
		return 0;

	return p->Paclen;
}

char *nr_config_get_desc(char *name)
{
	NR_Port *p = nr_port_ptr(name);

	if (p == NULL)
		return NULL;

	return p->Description;
}

static int nr_config_init_port(int fd, int lineno, char *line)
{
	NR_Port *p;
	char buffer[1024];
	char *name, *call, *alias, *paclen, *desc, *dev = NULL;
	struct ifconf ifc;
	struct ifreq *ifrp;
	struct ifreq ifr;
	int n, found = 0;
	
	name   = strtok(line, " \t");
	call   = strtok(NULL, " \t");
	alias  = strtok(NULL, " \t");
	paclen = strtok(NULL, " \t");
	desc   = strtok(NULL, "");

	if (name == NULL || call == NULL || alias == NULL || paclen == NULL ||
	    desc == NULL) {
		fprintf(stderr, "nrconfig: unable to parse line %d of config file\n", lineno);
		return FALSE;
	}

	for (p = nr_ports; p != NULL; p = p->Next) {
		if (strcasecmp(name, p->Name) == 0) {
			fprintf(stderr, "nrconfig: duplicate port name %s in line %d of config file\n", name, lineno);
			return FALSE;
		}
		if (strcasecmp(call, p->Call) == 0) {
			fprintf(stderr, "nrconfig: duplicate callsign %s in line %d of config file\n", call, lineno);
			return FALSE;
		}
		if (strcasecmp(alias, p->Alias) == 0) {
			fprintf(stderr, "nrconfig: duplicate alias %s in line %d of config file\n", alias, lineno);
			return FALSE;
		}
	}

	if (atoi(paclen) <= 0) {
		fprintf(stderr, "nrconfig: invalid packet size %s in line %d of config file\n", paclen, lineno);
		return FALSE;
	}

	strupr(call);
	strupr(alias);

	ifc.ifc_len = sizeof(buffer);
	ifc.ifc_buf = buffer;
	
	if (ioctl(fd, SIOCGIFCONF, &ifc) < 0) {
		fprintf(stderr, "nrconfig: SIOCGIFCONF: %s\n", strerror(errno));
		return FALSE;
	}

	for (ifrp = ifc.ifc_req, n = ifc.ifc_len / sizeof(struct ifreq); --n >= 0; ifrp++) {
		strcpy(ifr.ifr_name, ifrp->ifr_name);

		if (strcmp(ifr.ifr_name, "lo") == 0) continue;

		if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
			fprintf(stderr, "nrconfig: SIOCGIFFLAGS: %s\n", strerror(errno));
			return FALSE;
		}

		if (!(ifr.ifr_flags & IFF_UP)) continue;

		if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
			fprintf(stderr, "nrconfig: SIOCGIFHWADDR: %s\n", strerror(errno));
			return FALSE;
		}

		if (ifr.ifr_hwaddr.sa_family != ARPHRD_NETROM) continue;

		if (ax25_hw_cmp(call, ifr.ifr_hwaddr.sa_data)) {
			dev = ifr.ifr_name;
			found = 1;
			break;
		}
	}

	if (!found) {
		fprintf(stderr, "nrconfig: port %s not active\n", name);
		return FALSE;
	}

	if ((p = (NR_Port *)malloc(sizeof(NR_Port))) == NULL) {
		fprintf(stderr, "nrconfig: out of memory!\n");
		return FALSE;
	}

	p->Name        = strdup(name);
	p->Call        = strdup(call);
	p->Alias       = strdup(alias);
	p->Device      = strdup(dev);
	p->Paclen      = atoi(paclen);
	p->Description = strdup(desc);

	if (nr_ports == NULL)
		nr_ports = p;
	else
		nr_port_tail->Next = p;

	nr_port_tail = p;

	p->Next = NULL;
	
	return TRUE;
}

int nr_config_load_ports(void)
{
	FILE *fp;
	char buffer[256], *s;
	int fd, lineno = 1, n = 0;

	if ((fp = fopen(CONF_NRPORTS_FILE, "r")) == NULL) {
		fprintf(stderr, "nrconfig: unable to open nrports file %s (%s)\n", CONF_NRPORTS_FILE, strerror(errno));
		return 0;
	}

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		fprintf(stderr, "nrconfig: unable to open socket (%s)\n", strerror(errno));
		fclose(fp);
		return 0;
	}

	while (fgets(buffer, 255, fp) != NULL) {
		if ((s = strchr(buffer, '\n')) != NULL)
			*s = '\0';

		if (strlen(buffer) > 0 && buffer[0] != '#')
			if (nr_config_init_port(fd, lineno, buffer))
				n++;

		lineno++;
	}

	fclose(fp);
	close(fd);

	if (nr_ports == NULL)
		return 0;

	return n;
}
