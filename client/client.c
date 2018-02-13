#include <sys/socket.h>

#include <netax25/ax25.h>
#include <netax25/axlib.h>
#include <netax25/axconfig.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

int main (int argc, char **argv){
	int socket_file_descriptor;
	struct full_sockaddr_ax25 client_address;
	struct full_sockaddr_ax25 server_address;
	int address_size = sizeof(struct full_sockaddr_ax25);

	socket_file_descriptor = socket(AF_AX25, SOCK_SEQPACKET, 0);

	// Check if socket is opened (returns -1 if error)

	if (socket_file_descriptor < 0) {
		perror("Error opening socket");
		exit(1);
	}

	client_address.fsa_ax25.sax25_family = AF_AX25;
	ax25_aton_entry("YD0SHY-3", client_address.fsa_ax25.sax25_call.ax25_call);
//	client_address.fsa_ax25.sax25_ndigis = 1;
//	ax25_aton_entry("YD0SHY-3", client_address.fsa_digipeater[0].ax25_call);

	// Check if socket is bound successfully (returns -1 if error)

	if (bind(socket_file_descriptor, (struct sockaddr *) &client_address, address_size) < 0) {
		perror("Error binding socket");
		exit(1);
	}

	server_address.fsa_ax25.sax25_family = AF_AX25;
	ax25_aton_entry("YD0SHY-5", server_address.fsa_ax25.sax25_call.ax25_call);
	server_address.fsa_ax25.sax25_ndigis = 1;
	ax25_aton_entry("YD0SHY-10", server_address.fsa_digipeater[0].ax25_call);

	if (connect(socket_file_descriptor, (struct sockaddr *) &server_address, address_size) < 0) {
		perror("Error making connection");
		exit(1);
	}

	sleep(5);
	char *message = "Tes\r";
	write(socket_file_descriptor, message, strlen(message));

	printf("Sent message via connection to YD0SHY-10 through digipeater YD0SHY-2\n");
	sleep(60);
	close(socket_file_descriptor);

	return 0;
}
