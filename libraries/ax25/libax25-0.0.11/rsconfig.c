#define _LINUX_STRING_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/ioctl.h>

#include <config.h>

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
#include "rsconfig.h"

typedef struct _rsport
{
	struct _rsport *Next;
	char *Name;
	char *Addr;
	char *Device;
	char *Description;
} RS_Port;

static RS_Port *rs_ports       = NULL;
static RS_Port *rs_port_tail   = NULL;

static int rose_hw_cmp(unsigned char *address, unsigned char *hw_addr)
{
	rose_address addr;

	rose_aton(address, addr.rose_addr);

	return rose_cmp(&addr, (rose_address *)hw_addr) == 0;
}

static RS_Port *rs_port_ptr(char *name)
{
	RS_Port *p = rs_ports;

	if (name == NULL)
		return p;

	while (p != NULL) {
		if (strcasecmp(name, p->Name) == 0)
			return p;

		p = p->Next;
	}

	return NULL;
}

char *rs_config_get_next(char *name)
{
	RS_Port *p;
	
	if (rs_ports == NULL)
		return NULL;
		
	if (name == NULL)
		return rs_ports->Name;
		
	if ((p = rs_port_ptr(name)) == NULL)
		return NULL;
		
	p = p->Next;

	if (p == NULL)
		return NULL;
		
	return p->Name;
}

char *rs_config_get_name(char *device)
{
	RS_Port *p = rs_ports;

	while (p != NULL) {
		if (strcmp(device, p->Device) == 0)
			return p->Name;

		p = p->Next;
	}

	return NULL;
}

char *rs_config_get_addr(char *name)
{
	RS_Port *p = rs_port_ptr(name);

	if (p == NULL)
		return NULL;

	return p->Addr;
}

char *rs_config_get_dev(char *name)
{
	RS_Port *p = rs_port_ptr(name);

	if (p == NULL)
		return NULL;

	return p->Device;
}

char *rs_config_get_port(rose_address *address)
{
	RS_Port *p = rs_ports;
	rose_address addr;

	while (p != NULL) {
		rose_aton(p->Addr, addr.rose_addr);
	
		if (rose_cmp(address, &addr) == 0)
			return p->Name;

		p = p->Next;
	}

	return NULL;
}

char *rs_config_get_desc(char *name)
{
	RS_Port *p = rs_port_ptr(name);

	if (p == NULL)
		return NULL;

	return p->Description;
}

int rs_config_get_paclen(char *name)
{
	return 128;
}

static int rs_config_init_port(int fd, int lineno, char *line)
{
	RS_Port *p;
	char buffer[1024];
	char *name, *addr, *desc, *dev = NULL;
	struct ifconf ifc;
	struct ifreq *ifrp;
	struct ifreq ifr;
	int n, found = 0;
	
	name   = strtok(line, " \t");
	addr   = strtok(NULL, " \t");
	desc   = strtok(NULL, "");

	if (name == NULL || addr == NULL || desc == NULL) {
		fprintf(stderr, "rsconfig: unable to parse line %d of config file\n", lineno);
		return FALSE;
	}

	for (p = rs_ports; p != NULL; p = p->Next) {
		if (strcasecmp(name, p->Name) == 0) {
			fprintf(stderr, "rsconfig: duplicate port name %s in line %d of config file\n", name, lineno);
			return FALSE;
		}
		if (strcasecmp(addr, p->Addr) == 0) {
			fprintf(stderr, "rsconfig: duplicate address %s in line %d of config file\n", addr, lineno);
			return FALSE;
		}
	}

	ifc.ifc_len = sizeof(buffer);
	ifc.ifc_buf = buffer;
	
	if (ioctl(fd, SIOCGIFCONF, &ifc) < 0) {
		fprintf(stderr, "rsconfig: SIOCGIFCONF: %s\n", strerror(errno));
		return FALSE;
	}

	for (ifrp = ifc.ifc_req, n = ifc.ifc_len / sizeof(struct ifreq); --n >= 0; ifrp++) {
		strcpy(ifr.ifr_name, ifrp->ifr_name);

		if (strcmp(ifr.ifr_name, "lo") == 0) continue;

		if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
			fprintf(stderr, "rsconfig: SIOCGIFFLAGS: %s\n", strerror(errno));
			return FALSE;
		}

		if (!(ifr.ifr_flags & IFF_UP)) continue;

		if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
			fprintf(stderr, "rsconfig: SIOCGIFHWADDR: %s\n", strerror(errno));
			return FALSE;
		}

		if (ifr.ifr_hwaddr.sa_family != ARPHRD_ROSE) continue;

		if (rose_hw_cmp(addr, ifr.ifr_hwaddr.sa_data)) {
			dev = ifr.ifr_name;
			found = 1;
			break;
		}
	}

	if (!found) {
		fprintf(stderr, "rsconfig: port %s not active\n", name);
		return FALSE;
	}

	if ((p = (RS_Port *)malloc(sizeof(RS_Port))) == NULL) {
		fprintf(stderr, "rsconfig: out of memory!\n");
		return FALSE;
	}

	p->Name        = strdup(name);
	p->Addr        = strdup(addr);
	p->Device      = strdup(dev);
	p->Description = strdup(desc);

	if (rs_ports == NULL)
		rs_ports = p;
	else
		rs_port_tail->Next = p;

	rs_port_tail = p;

	p->Next = NULL;
	
	return TRUE;
}

int rs_config_load_ports(void)
{
	FILE *fp;
	char buffer[256], *s;
	int fd, lineno = 1, n = 0;

	if ((fp = fopen(CONF_RSPORTS_FILE, "r")) == NULL) {
		fprintf(stderr, "rsconfig: unable to open rsports file %s (%s)\n", CONF_RSPORTS_FILE, strerror(errno));
		return 0;
	}

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		fprintf(stderr, "rsconfig: unable to open socket (%s)\n", strerror(errno));
		fclose(fp);
		return 0;
	}

	while (fgets(buffer, 255, fp) != NULL) {
		if ((s = strchr(buffer, '\n')) != NULL)
			*s = '\0';

		if (strlen(buffer) > 0 && buffer[0] != '#')
			if (rs_config_init_port(fd, lineno, buffer))
				n++;

		lineno++;
	}

	fclose(fp);
	close(fd);

	if (rs_ports == NULL)
		return 0;

	return n;
}
