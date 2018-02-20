/*
 *
 * $Id: axspawn.c,v 1.1.1.1 2001/04/10 02:09:28 csmall Exp $
 *
 * axspawn.c - run a program from ax25d.
 *
 * Copyright (c) 1996 Jörg Reuter DL1BKE (jreuter@poboxes.com)
 *
 * This program is a hack.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *                
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * It might even kill your cat... ;-) 
 *
 * Status: alpha (still...)
 *
 * usage: change the "default" lines in your /usr/local/etc/ax25d.conf:
 *
 * 	default * * * * * 1 root /usr/local/sbin/axspawn axspawn
 *
 * a line like this would wait for an incoming info frame first.
 *
 * 	default * * * * * 1 root /usr/local/sbin/axspawn axspawn --wait
 *
 * The program will check if the peer is an AX.25 socket, the
 * callsign is a valid amateur radio callsign, strip the SSID, 
 * check if UID/GID are valid, allow a password-less login if the
 * password-entry in /etc/passwd is "+" or empty; in every other case
 * login will prompt for a password.
 *
 * Still on my TODO list: a TheNet compatible or MD5 based 
 * authentication scheme... Won't help you if you changed the "+"-entry
 * in /etc/passwd to a valid passord and login with telnet, though. 
 * A better solution could be a small program called from .profile.
 *
 * Axspawn can create user accounts automatically. You may specify
 * the user shell, first and maximum user id, group ID in the config
 * file and (unlike WAMPES) create a file "/usr/local/etc/ax25.profile" 
 * which will be copied to ~/.profile.
 *
 * This is an example for the config file:
 *
 * # this is /usr/local/etc/axspawn.conf
 * #
 * # allow automatic creation of user accounts
 * create    yes
 * #
 * # guest user if above is 'no' or everything else fails. Disable with "no"
 * guest     ax25
 * #
 * # group id or name for autoaccount
 * group     ax25
 * #
 * # first user id to use
 * first_uid 400
 * #
 * # maximum user id
 * max_uid   2000
 * #
 * # where to add the home directory for the new user
 * home      /home/ax25
 * #
 * # user's shell
 * shell     /bin/bash
 * #
 * # bind user id to callsign for outgoing connects.
 * associate yes
 *
 * SECURITY:
 *
 * Umm... auto accounting is a security problem by definition. Unlike
 * WAMPES, which creates an empty password field, Axspawn adds an
 * "impossible" ('+') password to /etc/passwd. Login gets called with
 * the "-f" option, thus new users have the chance to login without
 * a password. (I guess this won't work with the shadow password system).
 *
 * The "associate" option has to be used with great care. If a user
 * logs on it removes any existing callsign from the translation table
 * for this userid and replaces it with the callsign and SSID of the
 * user. This will happen with multiple connects (same callsign,
 * different SSIDs), too. Unless you want your users to be able
 * to call out from your machine disable "associate".
 *
 * Of course Axspawn does callsign checking: Only letters and numbers
 * are allowed, the callsign must be longer than 4 characters and
 * shorter than 6 characters (without SSID). There must be at least
 * one digit, and max. two digits within the call. The SSID must
 * be within the range of 0 and 15. Please drop me a note if you
 * know a valid Amateur Radio (sic!) callsign that does not fit this 
 * pattern _and_ can be represented correctly in AX.25.
 *
 * It uses the forkpty from libbsd.a (found after analyzing logind)
 * which has no prototype in any of my .h files.
 *
 */

/* removed -h <protocol> from login command as it was causing hostname lookups
   with new login/libc - Terry, vk2ktj. */


#define QUEUE_DELAY 400		/* 400 msec */
#define USERPROFILE ".profile"
#define PASSWDFILE  "/etc/passwd"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <pwd.h>
#include <grp.h>
#include <utmp.h>
#include <paths.h>
#include <errno.h>
#include <syslog.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/file.h>

#include <config.h>

#include <sys/socket.h>

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

#include "../pathnames.h"

#define MAXLEN strlen("DB0PRA-15")
#define MINLEN strlen("KA9Q")

#define AX_PACLEN	256
#define	NETROM_PACLEN	236
#define	ROSE_PACLEN	128

#define IS_DIGIT(x)  ( (x >= '0') && (x <= '9') )
#define IS_LETTER(x) ( (x >= 'A') && (x <= 'Z') )

#define MSG_NOCALL	"Sorry, you are not allowed to connect.\n"
#define MSG_CANNOTFORK	"Sorry, system is overloaded.\n"
#define MSG_NOPTY	"Sorry, all channels in use.\n"
#define MSG_NOTINDBF	"Sorry, you are not in my database\n"

#define EXITDELAY	10

char   policy_add_user = 1;
char   policy_guest = 1;
char   policy_associate = 0;

gid_t  user_gid =  400;
char   *user_shell = "/bin/bash";
char   *start_home = "/home/funk";
char   *guest = "guest";
int    start_uid = 400;
int    end_uid   = 65535;
int    paclen    = ROSE_PACLEN;		/* Its the shortest ie safest */

struct write_queue {
	struct write_queue *next;
	char *data;
	int  len;
};

struct write_queue *wqueue_head = NULL;
struct write_queue *wqueue_tail = NULL;
long wqueue_length = 0;

/* This one is in /usr/lib/libbsd.a, but not in bsd.h and fellows... weird. */
/* (found in logind.c)							    */

pid_t forkpty(int *, char *, void *, struct winsize *);

int _write_ax25(const char *s, int len)
{
	int k, m;
	char *p;

	p = (char *) malloc(len+1);

	if (p == NULL)
		return 0;

	m = 0;
	for (k = 0; k < len; k++)
	{
		if ( (s[k] == '\r') && ((k+1) < len) && (s[k+1] == '\n') )
			continue;
		else if (s[k] == '\n')
			p[m++] = '\r';
		else
			p[m++] = s[k];
	}
	
	if (m)
		write(1, p, m);

	free(p);
	return len;
}

int read_ax25(char *s, int size)
{
	int len = read(0, s, size);
	int k;
	
	for (k = 0; k < len; k++)
		if (s[k] == '\r') s[k] = '\n';
		
	return len;
}

/*
 *  We need to buffer the data from the pipe since bash does
 *  a fflush() on every output line. We don't want it, it's
 *  PACKET radio, isn't it?
 */

void kick_wqueue(int dummy)
{
	char *s, *p;
	struct write_queue *buf;
	
	
	if (wqueue_length == 0)
		return;

	s = (char *) malloc(wqueue_length);
	
	p = s;
	
	while (wqueue_head)
	{
		buf = wqueue_head;
		wqueue_head = buf->next;
		
		memcpy(p, buf->data, buf->len);
		p += buf->len;
		free(buf->data);
		free(buf);
	}

	_write_ax25(s, wqueue_length);
	free(s);
	wqueue_tail=NULL;
	wqueue_length=0;
}

int write_ax25(const char *s, int len)
{
	struct itimerval itv, oitv;
	struct write_queue * buf;
	
	signal(SIGALRM, SIG_IGN);

	buf = (struct write_queue *) malloc(sizeof(struct write_queue));
	if (buf == NULL)
		return 0;

	buf->data = (char *) malloc(len);
	if (buf->data == NULL)
		return 0;

	memcpy(buf->data, s, len);
	buf->len = len;
	buf->next = NULL;	
	
	if (wqueue_head == NULL)
	{
		wqueue_head = buf;
		wqueue_tail = buf;
		wqueue_length = len;
	} else {
		wqueue_tail->next = buf;
		wqueue_tail = buf;
		wqueue_length += len;
	}
	
	if (wqueue_length >= paclen)
	{
		kick_wqueue(0);
	} else {
		itv.it_interval.tv_sec = 0;
		itv.it_interval.tv_usec = 0;
		itv.it_value.tv_sec = 0;
		itv.it_value.tv_usec = QUEUE_DELAY;

		setitimer(ITIMER_REAL, &itv, &oitv);
		signal(SIGALRM, kick_wqueue);
	}
	return len;
}

int get_assoc(struct sockaddr_ax25 *sax25)
{
	FILE *fp;
	int  uid;
	char buf[81];
	
	fp = fopen(PROC_AX25_CALLS_FILE, "r");
	if (!fp) return -1;
	
	fgets(buf, sizeof(buf)-1, fp);
	
	while(!feof(fp))
	{
		if (fscanf(fp, "%d %s", &uid, buf) == 2)
			if (sax25->sax25_uid == uid)
			{
				ax25_aton_entry(buf, (char *) &sax25->sax25_call);
				return 0;
			}
	}
	
	return -1;
}


void cleanup(char *tty)
{
	struct utmp ut, *ut_line;
	FILE *fp;

	setutent();
	ut.ut_type = LOGIN_PROCESS;
	strncpy(ut.ut_id, tty + 3, sizeof(ut.ut_line));
	ut_line = getutid(&ut);

	if (ut_line != NULL) {
		ut_line->ut_type = DEAD_PROCESS;
		ut_line->ut_host[0] = '\0';
		ut_line->ut_user[0] = '\0';
		time(&ut_line->ut_time);
		pututline(ut_line);
		if ((fp = fopen(_PATH_WTMP, "r+")) != NULL) {
			fseek(fp, 0L, SEEK_END);
			if (fwrite(ut_line, sizeof(ut), 1, fp) != 1)
				syslog(LOG_ERR, "Ooops, I think I've just barbecued your wtmp file\n");
			fclose(fp);
		}
	}

	endutent();
}


/* 
 * add a new user to /etc/passwd and do some init
 */

void new_user(char *newuser)
{
	struct passwd pw, *pwp;
	uid_t uid;
	FILE *fp;
	char username[80];
	char homedir[256], userdir[256];
	char buf[4096];
	char subdir[4];
	int cnt;
	unsigned char *p, *q;
	struct stat fst;
	int fd_a, fd_b, fd_l;
	
	/*
	 * build path for home directory
	 */

	strncpy(subdir, newuser, 3);
	subdir[3] = '\0';
	sprintf(username, "%s", newuser);
	sprintf(homedir, "%s/%s.../%s", start_home, subdir, newuser);
	strcpy(userdir, homedir);

	fd_l = open(LOCK_AXSPAWN_FILE, O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
	flock(fd_l, LOCK_EX);

retry:
	/*
	 * find first free UID
	 */

	for (uid = start_uid; uid < 65535; uid++)
	{
		pwp = getpwuid(uid);
		if (pwp == NULL)
			break;
	}

	if (uid >= 65535 || uid < start_uid)
		return;

	/*
	 * build directories for home
	 */
	 
	p = homedir;
		
	while (*p == '/') p++;

	chdir("/");

	while(p)
	{
		q = strchr(p, '/');
		if (q)
		{
			*q = '\0';
			q++;
			while (*q == '/') q++;
			if (*q == 0) q = NULL;
		}

		if (stat(p, &fst) < 0)
		{
			if (errno == ENOENT)
			{
				mkdir(p, S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);

				if (q == NULL)
				{
					chown(p, uid, user_gid);
					chmod(p, S_IRUSR|S_IWUSR|S_IXUSR);
				} 
			}
			else
				return;
		}
		
		if (chdir(p) < 0)
			return;
		p = q;
	}

	/*
	 * add the user now
	 */

	fp = fopen(PASSWDFILE, "a+");
	if (fp == NULL)
		return;
	
	pw.pw_name   = newuser;
	pw.pw_passwd = "+";
	pw.pw_uid    = uid;
	pw.pw_gid    = user_gid;
	pw.pw_gecos  = username;
	pw.pw_dir    = userdir;
	pw.pw_shell  = user_shell;
	
	if (getpwuid(uid) != NULL) goto retry;	/* oops?! */

	if (putpwent(&pw, fp) < 0)
		return;

	flock(fd_l, LOCK_UN);
	fclose(fp);

	/*
	 * copy ax25.profile
	 */
	
	fd_a = open(CONF_AXSPAWN_PROF_FILE, O_RDONLY);
	
	if (fd_a > 0)
	{
		fd_b = open(USERPROFILE, O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR|S_IXUSR);

		if (fd_b < 0)
			return;
		
		while ( (cnt = read(fd_a, &buf, sizeof(buf))) > 0 )
			write(fd_b, &buf, cnt);
		close(fd_b);
		close(fd_a);
		chown(USERPROFILE, uid, user_gid);
	}
}

void read_config(void)
{
	FILE *fp = fopen(CONF_AXSPAWN_FILE, "r");
	char buf[512];
	char cmd[40], param[80];
	char *p;
	
	if (fp == NULL)
		return;
		
	while (!feof(fp))
	{
		fgets(buf, sizeof(buf), fp);
		p = strchr(buf, '#');
		if (p) *p='\0';
		
		if (buf[0] != '\0')
		{
			sscanf(buf, "%s %s", cmd, param);

			if (!strncmp(cmd, "create", 5))
			{
				policy_add_user = (param[0] == 'y');
			} else
			if (!strncmp(cmd, "guest", 5))
			{
				if (!strcmp(param, "no"))
				{
					policy_guest = 0;
				} else {
					policy_guest = 1;
					guest = (char *) malloc(strlen(param)+1);
					strcpy(guest, param);
				}
			} else
			if (!strncmp(cmd, "group", 5))
			{
				user_gid = strtol(param, &p, 0);
				if (*p != '\0')
				{
					struct group * gp = getgrnam(param);
					if (gp != NULL)
						user_gid = gp->gr_gid;
					else
						user_gid = 400;
					endgrent();
				}
			} else
			if (!strncmp(cmd, "first", 5))
			{
				start_uid = strtol(param, &p, 0);
				if (*p != '\0')
					start_uid = 400;
			} else
			if (!strncmp(cmd, "max", 3))
			{
				end_uid = strtol(param, &p, 0);
				if (*p != '\0')
					end_uid = 0;
			} else
			if (!strncmp(cmd, "home", 4))
			{
				start_home = (char *) malloc(strlen(param)+1);
				strcpy(start_home, param);
			} else
			if (!strncmp(cmd, "assoc", 5))
			{
				if (!strcmp(param, "yes"))
					policy_associate = 1;
				else 
					policy_associate = 0;
			} else
			if (!strncmp(cmd, "shell", 5))
			{
				user_shell = (char *) malloc(strlen(param)+1);
				strcpy(user_shell, param);
			} else
			{
				printf("error in config: ->%s %s<-\n", cmd, param);
			}
		}
	}
	
	fclose(fp);
}

char ptyslave[20];
int child_pid;

void signal_handler(int dummy)
{
	kill(child_pid, SIGHUP);
	cleanup(ptyslave+5);
	exit(1);
}
			
int main(int argc, char **argv)
{
	char call[20], user[20], real_user[20];
	char buf[2048];
	int  k, cnt, digits, letters, invalid, ssid, ssidcnt, addrlen;
	int  fdmaster;
	pid_t pid = -1;
	char *p;
	fd_set fds_read, fds_err;
	struct passwd *pw;
	int  chargc;
	char *chargv[20];
	int envc;
	char *envp[20];
	char wait_for_tcp;
	struct utmp ut_line;
	struct winsize win = { 0, 0, 0, 0};
	struct sockaddr_ax25 sax25;
	union {
		struct full_sockaddr_ax25 fsax25;
		struct sockaddr_rose      rose;
	} sockaddr;
	char *protocol;

	digits = letters = invalid = ssid = ssidcnt = 0;

	if (argc > 1 && (!strcmp(argv[1],"-w") || !strcmp(argv[1],"--wait")))
		wait_for_tcp = 1;
	else
		wait_for_tcp = 0;
		
	read_config();
	
	openlog("axspawn", LOG_PID, LOG_DAEMON);

	if (getuid() != 0) {
		printf("permission denied\n");
		syslog(LOG_NOTICE, "user %d tried to run axspawn\n", getuid());
		return 1;
	}

	addrlen = sizeof(struct full_sockaddr_ax25);
	k = getpeername(0, (struct sockaddr *) &sockaddr, &addrlen);
	
	if (k < 0) {
		syslog(LOG_NOTICE, "getpeername: %m\n");
		return 1;
	}

	switch (sockaddr.fsax25.fsa_ax25.sax25_family) {
		case AF_AX25:
			strcpy(call, ax25_ntoa(&sockaddr.fsax25.fsa_ax25.sax25_call));
			protocol = "AX.25";
			paclen   = AX_PACLEN;
			break;

		case AF_NETROM:
			strcpy(call, ax25_ntoa(&sockaddr.fsax25.fsa_ax25.sax25_call));
			protocol = "NET/ROM";
			paclen   = NETROM_PACLEN;
			break;

		case AF_ROSE:
			strcpy(call, ax25_ntoa(&sockaddr.rose.srose_call));
			protocol = "Rose";
			paclen   = ROSE_PACLEN;
			break;

		default:
			syslog(LOG_NOTICE, "peer is not an AX.25, NET/ROM or Rose socket\n");
			return 1;
	}

	for (k = 0; k < strlen(call); k++)
	{
		if (ssidcnt)
		{
			if (!IS_DIGIT(call[k]))
				invalid++;
			else
			{
				if (ssidcnt > 2)
					invalid++;
				else if (ssidcnt == 1)
					ssid = (int) (call[k] - '0');
				else
				{
					ssid *= 10;
					ssid += (int) (call[k] - '0');
					
					if (ssid > 15) invalid++;
				}
				ssidcnt++;
			}
		} else
		if (IS_DIGIT(call[k]))
		{
			digits++;
			if (k > 3) invalid++;
		} else
		if (IS_LETTER(call[k]))
			letters++;
		else
		if (call[k] == '-')
		{
			if (k < MINLEN)
				invalid++;
			else
				ssidcnt++;
		}
		else
			invalid++;
	}
		
	if ( invalid || (k < MINLEN) || (digits > 2) || (digits < 1) )
	{
		write_ax25(MSG_NOCALL, sizeof(MSG_NOCALL));
		syslog(LOG_NOTICE, "%s is not an Amateur Radio callsign\n", call);
		sleep(EXITDELAY);
		return 1;
	}
	
	strcpy(user, call);
	strlwr(user);
	p = strchr(user, '-');
	if (p) *p = '\0';
	strcpy(real_user, user);
	
	if (wait_for_tcp)
		read_ax25(buf, sizeof(buf));	/* incoming TCP/IP connection? */
	
	pw = getpwnam(user);

	if (pw == NULL)
	{
		if (policy_add_user) 
		{
			new_user(user);
			pw = getpwnam(user);
		}
		
		if (pw == NULL && policy_guest)
		{
			strcpy(real_user,guest);
			pw = getpwnam(guest);
		
			if (! (pw && pw->pw_uid && pw->pw_gid) )
			{
				write_ax25(MSG_NOTINDBF, sizeof(MSG_NOTINDBF));
				syslog(LOG_NOTICE, "%s (callsign: %s) not found in /etc/passwd\n", user, call);
				sleep(EXITDELAY);
				return 1;
			}
		}
	}
	
	endpwent();

	if (pw->pw_uid == 0 || pw->pw_gid == 0)
	{
		write_ax25(MSG_NOCALL, sizeof(MSG_NOCALL));
		syslog(LOG_NOTICE, "root login of %s (callsign: %s) denied\n", user, call);
		sleep(EXITDELAY);
		return 1;
	}
	
	/*
	 * associate UID with callsign (or vice versa?)
	 */

	if (policy_associate)
	{
		int fds = socket(AF_AX25, SOCK_SEQPACKET, 0);
		if (fds != -1)
		{
			sax25.sax25_uid = pw->pw_uid;
			if (get_assoc(&sax25) != -1)
				ioctl(fds, SIOCAX25DELUID, &sax25);
			switch (sockaddr.fsax25.fsa_ax25.sax25_family) {
				case AF_AX25:
				case AF_NETROM:
					sax25.sax25_call = sockaddr.fsax25.fsa_ax25.sax25_call;
					break;
				case AF_ROSE:
					sax25.sax25_call = sockaddr.rose.srose_call;
                                        break;
			}
			ioctl(fds, SIOCAX25ADDUID, &sax25);
			close(fds);
		}
	}
	
	fcntl(1, F_SETFL, O_NONBLOCK);
	
	pid = forkpty(&fdmaster, ptyslave, NULL, &win);
	
	if (pid == 0)
	{
		struct termios termios;

        	memset((char *) &termios, 0, sizeof(termios));
        	
        	ioctl(0, TIOCSCTTY, (char *) 0);

		termios.c_iflag = ICRNL | IXOFF;
            	termios.c_oflag = OPOST | ONLCR;
                termios.c_cflag = CS8 | CREAD | CLOCAL;
                termios.c_lflag = ISIG | ICANON;
                termios.c_cc[VINTR]  = 127;
                termios.c_cc[VQUIT]  =  28;
                termios.c_cc[VERASE] =   8;
                termios.c_cc[VKILL]  =  24;
                termios.c_cc[VEOF]   =   4;
                cfsetispeed(&termios, B19200);
                cfsetospeed(&termios, B19200);
                tcsetattr(0, TCSANOW, &termios);

		setutent();
                ut_line.ut_type = LOGIN_PROCESS;
                ut_line.ut_pid  = getpid();
                strncpy(ut_line.ut_line, ptyslave + 5, sizeof(ut_line.ut_line));
                strncpy(ut_line.ut_id,   ptyslave + 8, sizeof(ut_line.ut_id));
                strncpy(ut_line.ut_user, "LOGIN",      sizeof(ut_line.ut_user));
                strncpy(ut_line.ut_host, protocol,     sizeof(ut_line.ut_host));
                time(&ut_line.ut_time);
                ut_line.ut_addr = 0;                
                pututline(&ut_line);
                endutent();

                chargc = 0;
                chargv[chargc++] = "/bin/login";
                chargv[chargc++] = "-p";
                if (!strcmp(pw->pw_passwd, "+"))
	        	chargv[chargc++] = "-f";
                chargv[chargc++] = real_user;
                chargv[chargc]   = NULL;
                
                envc = 0;
                envp[envc] = (char *) malloc(30);
                sprintf(envp[envc++], "AXCALL=%s", call);
                envp[envc] = (char *) malloc(30);
                sprintf(envp[envc++], "CALL=%s", user);
                envp[envc] = (char *) malloc(30);
                sprintf(envp[envc++], "PROTOCOL=%s", protocol);
		envp[envc] = NULL;

                execve(chargv[0], chargv, envp);
        }
        else if (pid > 0)
        {
        	child_pid = 0;
        	signal(SIGHUP, signal_handler);
        	signal(SIGTERM, signal_handler);
        	signal(SIGINT, signal_handler);
        	signal(SIGQUIT, signal_handler);
 
        	while(1)
        	{
        		FD_ZERO(&fds_read);
        		FD_ZERO(&fds_err);
        		FD_SET(0, &fds_read);
        		FD_SET(0, &fds_err);
        		FD_SET(fdmaster, &fds_read);
        		FD_SET(fdmaster, &fds_err);
        		
        		k = select(fdmaster+1, &fds_read, NULL, &fds_err, NULL);
  
        		if (k > 0)
        		{
        			if (FD_ISSET(0, &fds_err))
        			{
        				kill(pid, SIGHUP);
        				cleanup(ptyslave+5);
        				return 1;
        			}
        			
        			if (FD_ISSET(fdmaster, &fds_err))
        			{
        				cleanup(ptyslave+5);
        				return 1;
        			}

        			if (FD_ISSET(0, &fds_read))
        			{
        				cnt = read_ax25(buf, sizeof(buf));
        				if (cnt < 0)	/* Connection died */
        				{
        					kill(pid, SIGHUP);
        					cleanup(ptyslave+5);
        					return 1;
        				} else
        					write(fdmaster, buf, cnt);
        			}
        			
        			if (FD_ISSET(fdmaster, &fds_read))
        			{
        				cnt = read(fdmaster, buf, sizeof(buf));
        				if (cnt < 0) 
        				{
	        				cleanup(ptyslave+5);
        					return 1;	/* Child died */
        				}
        				write_ax25(buf, cnt);
        			}
        		} else 
        		if (k < 0 && errno != EINTR)
        		{
        			kill(pid, SIGHUP);	/* just in case... */
        			cleanup(ptyslave+5);
        			return 0;
        		}
        	}
        } 
        else
        {
        	syslog(LOG_ERR, "cannot fork %m, closing connection to %s\n", call);
        	write_ax25(MSG_CANNOTFORK, sizeof(MSG_CANNOTFORK));
        	sleep(EXITDELAY);
        	return 1;
        }
        
        sleep(EXITDELAY);

	return 0;
}
