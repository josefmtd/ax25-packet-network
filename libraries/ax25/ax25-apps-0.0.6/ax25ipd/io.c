/* io.c 		All base I/O routines live here
 *
 * Copyright 1991, Michael Westerhof, Sun Microsystems, Inc.
 * This software may be freely used, distributed, or modified, providing
 * this header is not removed.
 *
 * This is the only module that knows about base level UNIX/SunOS I/O
 * This is also the key dispatching module, so it knows about a lot more
 * than just I/O stuff.
 */

#undef USE_ICMP			/* not implemented yet, sorry */

#include "ax25ipd.h"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#ifdef USE_ICMP
#include <netinet/ip_icmp.h>
#endif
#include <netdb.h>
#include <fcntl.h>
#include <memory.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>

#ifdef __bsdi__
#define USE_TERMIOS
#endif

#ifndef USE_TERMIOS
#ifndef USE_TERMIO
#define USE_SGTTY
#endif
#endif

#ifdef USE_TERMIOS
#include <sys/termios.h>
struct termios nterm;
#endif

#ifdef USE_TERMIO
#include <termio.h>
struct termio nterm;
#endif

#ifdef USE_SGTTY
#include <sys/ioctl.h>
struct sgttyb nterm;
#endif

int ttyfd = -1;
int udpsock = -1;
int sock = -1;
#ifdef USE_ICMP
int icmpsock = -1;
#endif
struct sockaddr_in udpbind;
struct sockaddr_in to;
struct sockaddr_in from;
int fromlen;

time_t last_bc_time;

/*
 * I/O modes for the io_error routine
 */
#define READ_MSG	0x00
#define SEND_MSG	0x01

#define IP_MODE		0x10
#define UDP_MODE	0x20
#define TTY_MODE	0x30
#ifdef USE_ICMP
#define ICMP_MODE	0x40
#endif

#ifndef FNDELAY
#define FNDELAY O_NDELAY
#endif

/*
 * Initialize the io variables
 */

void io_init()
{

/*
 * Close the file descriptors if they are open.  The idea is that we
 * will be able to support a re-initialization if sent a SIGHUP.
 */

	if (ttyfd >= 0) {
		close(ttyfd);
		ttyfd = -1;
	}

	if (sock >= 0) {
		close(sock);
		sock = -1;
	}

	if (udpsock >= 0) {
		close(udpsock);
		udpsock = -1;
	}
#ifdef USE_ICMP
	if (icmpsock >= 0) {
		close(icmpsock);
		icmpsock = -1;
	}
#endif

/*
 * The bzero is not strictly required - it simply zeros out the
 * address structure.  Since both to and from are static, they are
 * already clear.
 */
	bzero((char *) &to, sizeof(struct sockaddr));
	to.sin_family = AF_INET;

	bzero((char *) &from, sizeof(struct sockaddr));
	from.sin_family = AF_INET;

	bzero((char *) &udpbind, sizeof(struct sockaddr));
	udpbind.sin_family = AF_INET;
}

/*
 * open and initialize the IO interfaces
 */

void io_open()
{
	int baudrate;

	if (ip_mode) {
		sock = socket(AF_INET, SOCK_RAW, IPPROTO_AX25);
		if (sock < 0) {
			perror("opening raw socket");
			exit(1);
		}
		if (fcntl(sock, F_SETFL, FNDELAY) < 0) {
			perror("setting non-blocking I/O on raw socket");
			exit(1);
		}
#ifdef USE_ICMP
		icmpsock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
		if (icmpsock < 0) {
			perror("opening raw ICMP socket");
			exit(1);
		}
		if (fcntl(icmpsock, F_SETFL, FNDELAY) < 0) {
			perror("setting non-blocking I/O on ICMP socket");
			exit(1);
		}
#endif
	}

	if (udp_mode) {
		udpsock = socket(AF_INET, SOCK_DGRAM, 0);
		if (udpsock < 0) {
			perror("opening udp socket");
			exit(1);
		}
		if (fcntl(udpsock, F_SETFL, FNDELAY) < 0) {
			perror("setting non-blocking I/O on UDP socket");
			exit(1);
		}
/*
 * Ok, the udp socket is open.  Now express our interest in receiving
 * data destined for a particular socket.
 */
		udpbind.sin_addr.s_addr = INADDR_ANY;
		udpbind.sin_port = my_udp;
		if (bind(udpsock, (struct sockaddr *) &udpbind, sizeof udpbind) < 0) {
			perror("binding udp socket");
			exit(1);
		}
	}

	ttyfd = open(ttydevice, O_RDWR, 0);
	if (ttyfd < 0) {
		perror("opening tty device");
		exit(1);
	}
	if (fcntl(ttyfd, F_SETFL, FNDELAY) < 0) {
		perror("setting non-blocking I/O on tty device");
		exit(1);
	}
#ifdef USE_TERMIOS
	if (ioctl(ttyfd, TCGETS, &nterm) < 0) {
#endif
#ifdef USE_TERMIO
	if (ioctl(ttyfd, TCGETA, &nterm) < 0) {
#endif
#ifdef USE_SGTTY
	if (ioctl(ttyfd, TIOCGETP, &nterm) < 0) {
#endif
		perror("fetching tty device parameters");
		exit(1);
	}

	if (ttyspeed == 50)
		baudrate = B50;
	else if (ttyspeed == 50)
		baudrate = B50;
	else if (ttyspeed == 75)
		baudrate = B75;
	else if (ttyspeed == 110)
		baudrate = B110;
	else if (ttyspeed == 134)
		baudrate = B134;
	else if (ttyspeed == 150)
		baudrate = B150;
	else if (ttyspeed == 200)
		baudrate = B200;
	else if (ttyspeed == 300)
		baudrate = B300;
	else if (ttyspeed == 600)
		baudrate = B600;
	else if (ttyspeed == 1200)
		baudrate = B1200;
	else if (ttyspeed == 1800)
		baudrate = B1800;
	else if (ttyspeed == 2400)
		baudrate = B2400;
	else if (ttyspeed == 4800)
		baudrate = B4800;
	else if (ttyspeed == 9600)
		baudrate = B9600;
#ifdef B19200
	else if (ttyspeed == 19200)
		baudrate = B19200;
#else
#ifdef EXTA
	else if (ttyspeed == 19200)
		baudrate = EXTA;
#endif	/* EXTA */
#endif	/* B19200 */
#ifdef B38400
	else if (ttyspeed == 38400)
		baudrate = B38400;
#else
#ifdef EXTB
	else if (ttyspeed == 38400)
		baudrate = EXTB;
#endif /* EXTB */
#endif /* B38400 */
	else
		baudrate = B9600;

#ifdef USE_SGTTY
	nterm.sg_flags = (RAW | ANYP);
	nterm.sg_ispeed = baudrate;
	nterm.sg_ospeed = baudrate;
#else
	nterm.c_iflag = 0;
	nterm.c_oflag = 0;
	nterm.c_cflag = baudrate | CS8 | CREAD | CLOCAL;
	nterm.c_lflag = 0;
	nterm.c_cc[VMIN] = 0;
	nterm.c_cc[VTIME] = 0;
#endif /* USE_SGTTY */

#ifdef USE_TERMIOS
	if (ioctl(ttyfd, TCSETS, &nterm) < 0) {
#endif /* USE_TERMIOS */
#ifdef USE_TERMIO
	if (ioctl(ttyfd, TCSETA, &nterm) < 0) {
#endif /* USE_TERMIO */
#ifdef USE_SGTTY
	if (ioctl (ttyfd, TIOCSETP, &nterm) < 0) {
#endif /* USE_SGTTY */
		perror("setting tty device parameters");
		exit(1);
	}

	if (digi)
		send_params();

	last_bc_time = 0;	/* force immediate id */
}

/*
 * Start up and run the I/O mechanisms.
 *  run in a loop, using the select call to handle input.
 */

void io_start() {
	int n, nb, hdr_len;
	fd_set readfds;
	unsigned char buf[MAX_FRAME];
	struct timeval wait;
	struct iphdr *ipptr;
	time_t now;

	for (;;) {

		if ((bc_interval > 0) && digi) {
			now = time(NULL);
			if (last_bc_time + bc_interval < now) {
				last_bc_time = now;
				LOGL4("iostart: BEACON\n");
				do_beacon();
			}
		}

		wait.tv_sec = 10;	/* lets us keep the beacon going */
		wait.tv_usec = 0;

		FD_ZERO(&readfds);

		FD_SET(ttyfd, &readfds);

		if (ip_mode) {
			FD_SET(sock, &readfds);
#ifdef USE_ICMP
			FD_SET(icmpsock, &readfds);
#endif
		}

		if (udp_mode) {
			FD_SET(udpsock, &readfds);
		}

		nb = select(FD_SETSIZE, &readfds, (fd_set *) 0, (fd_set *) 0, &wait);

		if (nb < 0) {
			if (errno == EINTR)
				continue;	/* Ignore */
			perror("select");
			exit(1);
		}

		if (nb == 0) {
			fflush(stdout);
			fflush(stderr);
			/* just so we go back to the top of the loop! */
			continue;
		}

		if (FD_ISSET(ttyfd, &readfds)) {
			do {
				n = read(ttyfd, buf, MAX_FRAME);
			}
			while (io_error(n, buf, n, READ_MSG, TTY_MODE));
			LOGL4("ttydata l=%d\n", n);
			if (n > 0)
				assemble_kiss(buf, n);
/*
 * If we are in "beacon after" mode, reset the "last_bc_time" each time
 * we hear something on the channel.
 */
			if (!bc_every)
				last_bc_time = time(NULL);
		}

		if (udp_mode) {
			if (FD_ISSET(udpsock, &readfds)) {
				do {
					fromlen = sizeof from;
					n = recvfrom(udpsock, buf, MAX_FRAME, 0, (struct sockaddr *) &from, &fromlen);
				}
				while (io_error(n, buf, n, READ_MSG, UDP_MODE));
				LOGL4("udpdata from=%s port=%d l=%d\n", (char *) inet_ntoa(from.  sin_addr), ntohs(from.  sin_port), n);
				stats.udp_in++;
				if (n > 0)
					from_ip(buf, n);
			}
		}
		/* if udp_mode */
		if (ip_mode) {
			if (FD_ISSET(sock, &readfds)) {
				do {
					fromlen = sizeof from;
					n = recvfrom(sock, buf, MAX_FRAME, 0, (struct sockaddr *) &from, &fromlen);
				}
				while (io_error(n, buf, n, READ_MSG, IP_MODE));
				ipptr = (struct iphdr *) buf;
				hdr_len = 4 * ipptr-> ihl;
				LOGL4("ipdata from=%s l=%d, hl=%d\n", (char *) inet_ntoa(from.  sin_addr), n, hdr_len);
				stats.ip_in++;
				if (n > hdr_len)
					from_ip(buf + hdr_len, n - hdr_len);
			}
#ifdef USE_ICMP
			if (FD_ISSET(icmpsock, &readfds)) {
				do {
					fromlen = sizeof from;
					n = recvfrom(icmpsock, buf, MAX_FRAME, 0, (struct sockaddr *) &from, &fromlen);
				}
				while (io_error(n, buf, n, READ_MSG, ICMP_MODE));
				ipptr = (struct iphdr *) buf;
				hdr_len = 4 * ipptr-> ihl;
				LOGL4("icmpdata from=%s l=%d, hl=%d\n", (char *) inet_ntoa(from.  sin_addr), n, hdr_len);
			}
#endif
		}
		/* if ip_mode */
	}	/* for forever */
}

/* Send an IP frame */

void send_ip(buf, l, targetip)
unsigned char *buf;
int l;
unsigned char *targetip;
{
	int n;

	if (l <= 0)
		return;
	memcpy((char *) &to.sin_addr,
	       targetip, 4);
	memcpy((char *) &to.sin_port,
	       &targetip[4], 2);
	LOGL4("sendipdata to=%s %s %d l=%d\n", (char *) inet_ntoa(to.  sin_addr), to.sin_port ? "udp" : "ip", ntohs(to.sin_port), l);
	if (to.sin_port) {
		if (udp_mode) {
			stats.udp_out++;
			do {
				n = sendto(udpsock, buf, l, 0, (struct sockaddr *) &to, sizeof to);
			}
			while (io_error(n, buf, l, SEND_MSG, UDP_MODE));
		}
	} else {
		if (ip_mode) {
			stats.ip_out++;
			do {
				n = sendto(sock, buf, l, 0, (struct sockaddr *) &to, sizeof to);
			}
			while (io_error(n, buf, l, SEND_MSG, IP_MODE));
		}
	}
}

/* Send a kiss frame */

void send_tty(buf, l)
unsigned char *buf;
int l;
{
	int n;
	unsigned char *p;
	int nc;

	if (l <= 0)
		return;
	LOGL4("sendttydata l=%d\tsent: ", l);
	stats.kiss_out++;

	p = buf;
	nc = l;
	n = 0;

/*
 * we have to loop around here because each call to write may write a few
 * characters.  So we simply increment the buffer each time around.  If
 * we ever write no characters, we should get an error code, and io_error
 * will sleep for a fraction of a second.  Note that we are keyed to
 * the BSD 4.2 behaviour... the Sys 5 non-blocking I/O may or may not work
 * in this loop.  We may detect system 5 behaviour (this would result from
 * compile-time options) by having io_error barf when it detects an EAGAIN
 * error code.
 */
	do {
		if ((n > 0) && (n < nc)) {	/* did we put only write a bit? */
			p += n;	/* point to the new data */
			nc -= n;	/* drop the length */
		}
		n = write(ttyfd, p, nc);
		if (n > 0) {
			if (n != nc) {
				LOGL4("%d ", n);	/* no-one said loglevel 4 */
			} else {
				LOGL4("%d\n", n);	/* was efficient!!! */
			}
		}
	}
	while (((n > 0) && (n < nc)) || (io_error(n, p, nc, SEND_MSG, TTY_MODE)));
}

/* process an I/O error; return true if a retry is needed */
int io_error(oops, buf, bufsize, dir, mode)
int oops;	/* the error flag; < 0 indicates a problem */
unsigned char *buf;	/* the data in question */
int bufsize;	/* the size of the data buffer */
int dir;	/* the direction; input or output */
int mode;	/* the fd on which we got the error */
{

	if (oops >= 0)
		return 0;	/* do we have an error ? */

#ifdef EAGAIN
	if (errno == EAGAIN) {
		perror("System 5 I/O error!");
		fprintf(stderr, "A System 5 style I/O error was detected.  This program requires BSD 4.2\n");
		fprintf(stderr, "behaviour.  This is probably a result of compile-time environment.\n");
		exit(3);
	}
#endif

	if (dir == READ_MSG) {
		if (errno == EINTR)
			return 0;	/* never retry read */
		if (errno == EWOULDBLOCK) {
			LOGL4("READ would block (?!), sleeping and retrying!\n");
			usleep(100000);	/* sleep a bit */
			return 1;	/* and retry */
		}
		if (mode == IP_MODE) {
			perror("reading from raw ip socket");
			exit(2);
		} else if (mode == UDP_MODE) {
			perror("reading from udp socket");
			exit(2);
		} else if (mode == TTY_MODE) {
			perror("reading from tty device");
			exit(2);
		} else {
			perror("reading from unknown I/O");
			exit(2);
		}
	} else if (dir == SEND_MSG) {
		if (errno == EINTR)
			return 1;	/* always retry on writes */
		if (mode == IP_MODE) {
			if (errno == EMSGSIZE) {	/* msg too big, drop it */
				perror("message dropped on raw ip socket");
				fprintf (stderr, "message was %d bytes long.\n", bufsize);
				return 0;
			}
			if (errno == ENOBUFS) {	/* congestion; sleep + retry */
				LOGL4("send congestion on raw ip, sleeping and retrying!\n");
				usleep(100000);
				return 1;
			}
			if (errno == EWOULDBLOCK) {
				LOGL4("send on raw ip would block, sleeping and retrying!\n");
				usleep(100000);	/* sleep a bit */
				return 1;	/* and retry */
			}
			perror("writing to raw ip socket");
			exit(2);
		} else if (mode == UDP_MODE) {
			if (errno == EMSGSIZE) {	/* msg too big, drop it */
				perror("message dropped on udp socket");
				fprintf(stderr, "message was %d bytes long.\n", bufsize);
				return 0;
			}
			if (errno == ENOBUFS) {	/* congestion; sleep + retry */
				LOGL4("send congestion on udp, sleeping and retrying!\n");
				usleep(100000);
				return 1;
			}
			if (errno == EWOULDBLOCK) {
				LOGL4("send on udp would block, sleeping and retrying!\n");
				usleep(100000);	/* sleep a bit */
				return 1;	/* and retry */
			}
			perror("writing to udp socket");
			exit(2);
		} else if (mode == TTY_MODE) {
			if (errno == EWOULDBLOCK) {
				LOGL4("write to tty would block, sleeping and retrying!\n");
				usleep(100000);	/* sleep a bit */
				return 1;	/* and retry */
			}
			perror("writing to tty device");
			exit(2);
		} else {
			perror("writing to unknown I/O");
			exit(2);
		}
	} else {
		perror("Unknown direction and I/O");
		exit(2);
	}
	return 0;
}
