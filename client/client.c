#include <sys/socket.h>

#include <netax25/ax25.h>
#include <netax25/axlib.h>
#include <netax25/axconfig.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

int main (int argc, char **argv){
	int sockfd;
	struct full_sockaddr_ax25 client_addr;
	struct full_sockaddr_ax25 server_addr;
	int addr_size = sizeof(struct full_sockaddr_ax25);

	sockfd = socket(AF_AX25, SOCK_SEQPACKET, 0);

	// Check if socket is opened (returns -1 if error)

	if (sockfd < 0) {
		perror("Error opening socket");
		exit(1);
	}

	client_addr.fsa_ax25.sax25_family = AF_AX25;
	ax25_aton_entry("YD0SHY-2", client_addr.fsa_ax25.sax25_call.ax25_call);
	client_addr.fsa_ax25.sax25_ndigis = 1;
	ax25_aton_entry("YD0SHY-2", client_addr.fsa_digipeater[0].ax25_call);

	// Check if socket is bound successfully (returns -1 if error)

	if (bind(sockfd, (struct sockaddr *) &client_addr, addr_size) < 0) {
		perror("Error binding socket");
		exit(1);
	}

	server_addr.fsa_ax25.sax25_family = AF_AX25;
	ax25_aton_entry("YD0SHY-10", server_addr.fsa_ax25.sax25_call.ax25_call);
	server_addr.fsa_ax25.sax25_ndigis = 1;
	ax25_aton_entry("YD0SHY-2", server_addr.fsa_ax25.sax25_call.ax25_call);

	if (connect(sockfd, (struct sockaddr *) &server_addr, addr_size) < 0) {
		perror("Error making connection");
		exit(1);
	}

	sleep(5);
	char *message = "Tes\r";
	write(sockfd, message, strlen(message));

	printf("Sent message via connection to YD0SHY-10 through digipeater YD0SHY-2\n");
	sleep(60);
	close(sockfd);

	return 0;
}

