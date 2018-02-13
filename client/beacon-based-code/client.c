// Generic Libraries

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netax25/ax25.h>
#include <netax25/axlib.h>
#include <netax25/axconfig.h>
#include <netax25/daemon.h>

static int logging = FALSE;
static int mail = FALSE;
static int single = FALSE;

#define VERSION "0.1"

int main(int argc, char *argv[]) {
  struct full_sockaddr_ax25 server_address;
  struct full_sockaddr_ax25 client_address;
  int server_address_length, client_address_length;
  int option;
  int interval = 30;
  char address[20], *port, *device, *message, *port_call;
  char *client_call = NULL, *server_call = NULL;

  while ((option = getopt(argc, argv, "d:t:lv")) != -1) {
    switch (option) {
      case 'd':
        server_call = optarg;
        break;
      case 't':
        interval = atoi(optarg);
        break;
      case 'l':
        logging = TRUE;
        break;
      case 'v':
        printf("client: %s\n", VERSION);
    }

  }
  if (ax25_config_load_ports() == 0) {
    fprintf(stderr, "client: no AX.25 ports defined\n");
    return 1;
  }

  if ((port_call = ax25_config_get_addr(port))) {
    fprintf(stderr, "client: invalid AX.25 port setting - %s\n", port);
    return 1;
  }

  if (client_call != NULL && strcmp(client_call, port_call)) {
    sprintf(address, "%s %s", client_call, port_call);
  }
  else {
    strcpy(address, port_call);
  }

  if (client_address_length = ax25_aton(address, &client_address) == -1) {
    fprintf(stderr, "client: unable to convert callsign '%s'\n", address);
    return 1;
  }
}
