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

static void terminate(int sig) {
  exit(0);
}

int main(int argc, char *argv[]) {
  struct full_sockaddr_ax25 server_address;
  struct full_sockaddr_ax25 client_address;
  int server_address_length, client_address_length;
  int option;
  int interval = 30;
  int socket_file_descriptor;
  char address_buffer[20], *message, *port_call;
  char *port = NULL, *client_call = NULL, *server_call = NULL;

  while ((option = getopt(argc, argv, "d:t:lv")) != -1) {
    switch (option) {
      case 'd':
        server_call = optarg;
        break;
      case 't':
        interval = atoi(optarg);
        break;
      case 's':
        single = TRUE;
        break;
      case 'v':
        printf("client: %s\n", VERSION);
      default:
        fprintf(stderr, "usage: client [-d <srv_call>] [-t interval] [-l] [-v] <message>\n");
        return 1;
    }
  }

  signal(SIGTERM, terminate);
  
  if (optind == argc) {
    fprintf(stderr, "usage: client [-d <srv_call>] [-t interval] [-l] [-v] <message>\n");
    return 1;
  }

  message = argv[optind];

  if (ax25_config_load_ports() == 0) {
    fprintf(stderr, "client: no AX.25 ports defined\n");
    return 1;
  }
  printf("There is an AX.25 port: %s\n", port); // debug

  if (port != NULL) {
    if ((port_call = ax25_config_get_addr(port))) {
      fprintf(stderr, "client: invalid AX.25 port setting - %s\n", port);
      return 1;
    }
    return 1;
  }

  strcpy(address_buffer, server_call);

  if ((server_address_length = ax25_aton(address_buffer, &server_address)) == -1) {
    fprintf(stderr, "client: unable to convert callsign '%s'\n", address_buffer);
    return 1;
  }

  printf("Server address: %s converted to AX.25 format\n", address_buffer); // debug

  printf("Address found is %s\n", port_call); // debug

  if (client_call != NULL && strcmp(client_call, port_call)) {
    sprintf(address_buffer, "%s %s", client_call, port_call);
  }
  else {
    strcpy(address_buffer, port_call);
  }

  printf("Address copied to buffer\n"); // debug

  if (client_address_length = ax25_aton(address_buffer, &client_address) == -1) {
    fprintf(stderr, "client: unable to convert callsign '%s'\n", address_buffer);
    return 1;
  }

  printf("Client address converted to AX.25 format\n"); // debug

  if (!single) {
    if (!daemon_start(FALSE)) {
      fprintf(stderr, "client: cannot become a daemon\n");
      return 1;
    }
  }

  while(1) {
    if ((socket_file_descriptor = socket(AF_AX25, SOCK_SEQPACKET, 0)) == -1) {
      return 1;
    }

    if (bind(socket_file_descriptor, (struct sockaddr *)&client_address, client_address_length) == -1) {
      return 1;
    }

    if (connect(socket_file_descriptor, (struct sockaddr *)&server_address, server_address_length) == -1) {
      return 1;
    }

    write(socket_file_descriptor, message, strlen(message));

    printf("Sent message via connection to %s\n", address_buffer);
    sleep(60);
    close(socket_file_descriptor);

    if (!single)
      sleep(interval * 60);
    else
      break;
  }

  return 0;
}
