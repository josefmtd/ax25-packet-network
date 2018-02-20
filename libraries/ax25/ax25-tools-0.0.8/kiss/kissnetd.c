/*
 * kissnetd.c : Simple kiss broadcast daemon between several
 *		pseudo-tty devices.
 *		Each kiss frame received on one pty is broadcasted
 *		to each other one.
 *
 * ATEPRA FPAC/Linux Project
 *
 * F1OAT 960804 - Frederic RIBLE
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <time.h>

static char *Version = "1.5";
static int VerboseMode = 0;
static int MaxFrameSize = 512;

#define REOPEN_TIMEOUT	30	/* try tio reopen every 10 s */

struct PortDescriptor {
	char		Name[80];
	int		Fd;
	unsigned char	*FrameBuffer;
	int		BufferIndex;
	time_t		TimeLastOpen;
};

static struct PortDescriptor *PortList[FD_SETSIZE];
static int NbPort = 0;

static void Usage(void)
{
	fprintf(stderr, "\nUsage : kissnetd [-v] [-f size] /dev/pty?? [/dev/pty??]*\n");
	fprintf(stderr, " -v       : Verbose mode, trace on stdout\n");
	fprintf(stderr, " -f size  : Set max frame size to size bytes (default 512)\n");
	exit(1);
} 

static void Banner(int Small)
{
	if (Small) {
		printf("kissnetd V %s by Frederic RIBLE F1OAT - ATEPRA FPAC/Linux Project\n", Version);
	}
	else {
		printf("****************************************\n");
		printf("* Network broadcast between kiss ports *\n");
		printf("*      ATEPRA FPAC/Linux Project       *\n");
		printf("****************************************\n");
		printf("*         kissnetd Version %-4s        *\n", Version); 
		printf("*        by Frederic RIBLE F1OAT       *\n");
		printf("****************************************\n");
	}
}

static void NewPort(char *Name)
{
	struct PortDescriptor *MyPort;
	
	if (VerboseMode) {
		printf("Opening port %s\n", Name);
	}
	
	if (NbPort == FD_SETSIZE) {
		fprintf(stderr, "Cannot handle %s : too many ports\n", Name);
		exit(1);
	}
	
	MyPort = calloc(sizeof(struct PortDescriptor), 1);
	if (MyPort) MyPort->FrameBuffer = calloc(sizeof (unsigned char), MaxFrameSize);
	if (!MyPort || !MyPort->FrameBuffer) {
		perror("cannot allocate port descriptor");
		exit(1);
	}
	
	strncpy(MyPort->Name, Name, sizeof(MyPort->Name));
	MyPort->Fd = -1;
	MyPort->FrameBuffer[0] = 0xC0;
	MyPort->BufferIndex = 1;
	PortList[NbPort++] = MyPort;
}

static void ReopenPort(int PortNumber)
{
	char MyString[80];

	PortList[PortNumber]->TimeLastOpen = time(NULL);
		
	if (VerboseMode) {
		printf("Reopening port %d\n", PortNumber);
	}
	
	syslog(LOG_WARNING, "kissnetd : Opening port %s\n", PortList[PortNumber]->Name);
	
	PortList[PortNumber]->Fd = open(PortList[PortNumber]->Name, O_RDWR | O_NONBLOCK);
	if (PortList[PortNumber]->Fd < 0) {
		syslog(LOG_WARNING, "kissnetd : Error opening port %s : %s\n", 
			PortList[PortNumber]->Name, sys_errlist[errno]);
		if (VerboseMode) {
			sprintf(MyString, "cannot reopen %s", PortList[PortNumber]->Name);
			perror(MyString);
		}
	}
}

static void TickReopen(void)
{
	int i;
	time_t CurrentTime = time(NULL);
	
	for (i=0; i<NbPort; i++) {
		if (PortList[i]->Fd >= 0) continue;
		if ( (CurrentTime - PortList[i]->TimeLastOpen) > REOPEN_TIMEOUT ) ReopenPort(i);
	}
}

static void Broadcast(int InputPort)
{
	int i;
	int rc;
	
	/* Broadcast only info frames */
	
	if (PortList[InputPort]->FrameBuffer[1] != 0x00) return;
	
	for (i=0; i<NbPort; i++) {
		if (i == InputPort) continue;
		if (PortList[i]->Fd < 0) continue;
		rc = write(PortList[i]->Fd, 
			   PortList[InputPort]->FrameBuffer, 
			   PortList[InputPort]->BufferIndex);
		if (VerboseMode) {
			printf("Sending %d bytes on port %d : rc=%d\n",
				PortList[InputPort]->BufferIndex,
				i, rc);
		}	   
	}
}

static void ProcessInput(int PortNumber)
{
	static unsigned char MyBuffer[2048];
	int Length;
	int i;
	struct PortDescriptor *MyPort = PortList[PortNumber];
	
	Length = read(MyPort->Fd, MyBuffer, sizeof(MyBuffer));
	if (VerboseMode) {
		printf("Read port %d : rc=%d\n", PortNumber, Length);
	}
	if (!Length) return;
	if (Length < 0) {
		syslog(LOG_WARNING, "kissnetd : Error reading port %s : %s\n", 
			PortList[PortNumber]->Name, sys_errlist[errno]);
		if (VerboseMode) perror("read");
		close(MyPort->Fd);
		MyPort->Fd = -1;
		return;
	}
	for (i=0; i<Length; i++) {
		if (MyPort->BufferIndex == MaxFrameSize) {
			if (MyBuffer[i] == 0xC0) {
				if (VerboseMode) printf("Drop frame too long\n");
				MyPort->BufferIndex = 1;
			}
		}
		else {		
			MyPort->FrameBuffer[MyPort->BufferIndex++] = MyBuffer[i];
			if (MyBuffer[i] == 0xC0) {
				Broadcast(PortNumber);
				MyPort->BufferIndex = 1; 
			}
		}
	}
}

static void ProcessPortList(void)
{
	static fd_set MyFdSet;
	int i, rc;
	struct timeval Timeout;
	
	Timeout.tv_sec = 1;
	Timeout.tv_usec = 0;
	
	FD_ZERO(&MyFdSet);
	for (i=0; i<NbPort; i++) {
		if (PortList[i]->Fd >= 0) FD_SET(PortList[i]->Fd, &MyFdSet);
	}
	rc = select(FD_SETSIZE, &MyFdSet, NULL, NULL, &Timeout);
	
	if (VerboseMode) printf("select : rc=%d\n", rc);
	if (!rc ) TickReopen();
	
	if (rc > 0) {
		for (i=0; i<NbPort && rc; i++) {
			if (PortList[i]->Fd < 0) continue;
			if (FD_ISSET(PortList[i]->Fd, &MyFdSet)) {
				ProcessInput(i);
				rc--;
			}
		}
	}	
}

static void ProcessArgv(char *argv[])
{
	char *Opt;
	int ArgvIndex = 0;
	
	while (argv[ArgvIndex]) {
		if (argv[ArgvIndex][0] != '-') {
			NewPort(argv[ArgvIndex++]);
			continue;
		}
		for (Opt = &argv[ArgvIndex++][1]; *Opt; Opt++) {
			switch (*Opt) {
			case 'v':
				VerboseMode = 1;
				break;
			case 'f':
				MaxFrameSize = atoi(argv[ArgvIndex++]);
				break;	
			default:
				fprintf(stderr, "Invalid option %c\n", *Opt);
				Usage();
				break;
			}
		}
		
	}
}


int main(int argc, char *argv[]) 
{
	if (argc < 2) {
		Banner(0);
		Usage();
	}
	else {
		Banner(1);
	}
	
	ProcessArgv(argv+1);
	while (1) ProcessPortList();
	return 0;	
}
