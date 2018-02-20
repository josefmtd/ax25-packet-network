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
#include "axlib.h"
#include "pathnames.h"
#include "axconfig.h"

typedef struct _axport
{
	struct _axport *Next;
	char *Name;
	char *Call;
	char *Device;
	int  Baud;
	int  Window;
	int  Paclen;
	char *Description;
} AX_Port;

static AX_Port *ax25_ports     = NULL;
static AX_Port *ax25_port_tail = NULL;

static int ax25_hw_cmp(unsigned char *callsign, unsigned char *hw_addr)
{
	ax25_address call;

	ax25_aton_entry(callsign, call.ax25_call);
	
	return ax25_cmp(&call, (ax25_address *)hw_addr) == 0;
}

static AX_Port *ax25_port_ptr(char *name)
{
	AX_Port *p = ax25_ports;

	if (name == NULL)
		return p;

	while (p != NULL) {
		if (strcasecmp(p->Name, name) == 0)
			return p;

		p = p->Next;
	}

	return NULL;
}

char *ax25_config_get_next(char *name)
{
	AX_Port *p;
	
	if (ax25_ports == NULL)
		return NULL;
		
	if (name == NULL)
		return ax25_ports->Name;
	
	if ((p = ax25_port_ptr(name)) == NULL)
		return NULL;
			
	p = p->Next;

	if (p == NULL)
		return NULL;
		
	return p->Name;
}

char *ax25_config_get_name(char *device)
{
	AX_Port *p = ax25_ports;

	while (p != NULL) {
		if (strcmp(p->Device, device) == 0)
			return p->Name;

		p = p->Next;
	}

	return NULL;
}

char *ax25_config_get_addr(char *name)
{
	AX_Port *p = ax25_port_ptr(name);

	if (p == NULL)
		return NULL;

	return p->Call;
}

char *ax25_config_get_dev(char *name)
{
	AX_Port *p = ax25_port_ptr(name);

	if (p == NULL)
		return NULL;

	return p->Device;
}

char *ax25_config_get_port(ax25_address *callsign)
{
	AX_Port *p = ax25_ports;
	ax25_address addr;

	if (ax25_cmp(callsign, &null_ax25_address) == 0)
		return "*";
		
	while (p != NULL) {
		ax25_aton_entry(p->Call, (char *)&addr);

		if (ax25_cmp(callsign, &addr) == 0)
			return p->Name;

		p = p->Next;
	}

	return NULL;
}

int ax25_config_get_window(char *name)
{
	AX_Port *p = ax25_port_ptr(name);

	if (p == NULL)
		return 0;

	return p->Window;
}

int ax25_config_get_paclen(char *name)
{
	AX_Port *p = ax25_port_ptr(name);

	if (p == NULL)
		return 0;

	return p->Paclen;
}

int ax25_config_get_baud(char *name)
{
	AX_Port *p = ax25_port_ptr(name);

	if (p == NULL)
		return 0;

	return p->Baud;
}

char *ax25_config_get_desc(char *name)
{
	AX_Port *p = ax25_port_ptr(name);

	if (p == NULL)
		return NULL;

	return p->Description;
}

static int ax25_config_init_port(int fd, int lineno, char *line)
{
	AX_Port *p;
	char buffer[1024];
	char *name, *call, *baud, *paclen, *window, *desc, *dev = NULL;
	struct ifconf ifc;
	struct ifreq *ifrp;
	struct ifreq ifr;
	int n, found = 0;

	name   = strtok(line, " \t");
	call   = strtok(NULL, " \t");
	baud   = strtok(NULL, " \t");
	paclen = strtok(NULL, " \t");
	window = strtok(NULL, " \t");
	desc   = strtok(NULL, "");

	if (name == NULL   || call == NULL   || baud == NULL ||
	    paclen == NULL || window == NULL || desc == NULL) {
		fprintf(stderr, "axconfig: unable to parse line %d of axports file\n", lineno);
		return FALSE;
	}

	for (p = ax25_ports; p != NULL; p = p->Next) {
		if (strcasecmp(name, p->Name) == 0) {
			fprintf(stderr, "axconfig: duplicate port name %s in line %d of axports file\n", name, lineno);
			return FALSE;
		}
		if (strcasecmp(call, p->Call) == 0) {
			fprintf(stderr, "axconfig: duplicate callsign %s in line %d of axports file\n", call, lineno);
			return FALSE;
		}
	}

	if (atoi(baud) < 0) {
		fprintf(stderr, "axconfig: invalid baud rate setting %s in line %d of axports file\n", baud, lineno);
		return FALSE;
	}

	if (atoi(paclen) <= 0) {
		fprintf(stderr, "axconfig: invalid packet size setting %s in line %d of axports file\n", paclen, lineno);
		return FALSE;
	}

	if (atoi(window) <= 0) {
		fprintf(stderr, "axconfig: invalid window size setting %s in line %d of axports file\n", window, lineno);
		return FALSE;
	}

	strupr(call);

	ifc.ifc_len = sizeof(buffer);
	ifc.ifc_buf = buffer;
	
	if (ioctl(fd, SIOCGIFCONF, &ifc) < 0) {
		fprintf(stderr, "axconfig: SIOCGIFCONF: %s\n", strerror(errno));
		return FALSE;
	}

	for (ifrp = ifc.ifc_req, n = ifc.ifc_len / sizeof(struct ifreq); --n >= 0; ifrp++) {
		strcpy(ifr.ifr_name, ifrp->ifr_name);

		if (strcmp(ifr.ifr_name, "lo") == 0) continue;

		if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
			fprintf(stderr, "axconfig: SIOCGIFFLAGS: %s\n", strerror(errno));
			return FALSE;
		}

		if (!(ifr.ifr_flags & IFF_UP)) continue;

		if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
			fprintf(stderr, "axconfig: SIOCGIFHWADDR: %s\n", strerror(errno));
			return FALSE;
		}

		if (ifr.ifr_hwaddr.sa_family != ARPHRD_AX25) continue;

		if (ax25_hw_cmp(call, ifr.ifr_hwaddr.sa_data)) {
			dev = ifr.ifr_name;
			found = 1;
			break;
		}
	}

	if (!found) {
		fprintf(stderr, "axconfig: port %s not active\n", name);
		return FALSE;
	}

	if ((p = (AX_Port *)malloc(sizeof(AX_Port))) == NULL) {
		fprintf(stderr, "axconfig: out of memory!\n");
		return FALSE;
	}

	p->Name        = strdup(name);
	p->Call        = strdup(call);
	p->Device      = strdup(dev);
	p->Baud        = atoi(baud);
	p->Window      = atoi(window);
	p->Paclen      = atoi(paclen);
	p->Description = strdup(desc);

	if (ax25_ports == NULL)
		ax25_ports = p;
	else
		ax25_port_tail->Next = p;

	ax25_port_tail = p;

	p->Next = NULL;

	return TRUE;
}

int ax25_config_load_ports(void)
{
	FILE *fp;
	char buffer[256], *s;
	int fd, lineno = 1, n = 0;

	if ((fp = fopen(CONF_AXPORTS_FILE, "r")) == NULL) {
		fprintf(stderr, "axconfig: unable to open axports file %s (%s)\n", CONF_AXPORTS_FILE, strerror(errno));
		return 0;
	}

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		fprintf(stderr, "axconfig: unable to open socket (%s)\n", strerror(errno));
		fclose(fp);
		return 0;
	}

	while (fgets(buffer, 255, fp) != NULL) {
		if ((s = strchr(buffer, '\n')) != NULL)
			*s = '\0';

		if (strlen(buffer) > 0 && *buffer != '#')
			if (ax25_config_init_port(fd, lineno, buffer))
				n++;

		lineno++;
	}

	fclose(fp);
	close(fd);

	if (ax25_ports == NULL)
		return 0;

	return n;
}
