// Generic Libraries

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <error.h>

// Socket Libraries

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#ifdef __GLIBC__
  #include <net/if.h>
  #include <net/ethernet.h>
#else
  #include <linux/if_ether.h>
  #include <linux/if.h>
#endif
#include <netinet/in.h>

// AX.25 Libraries
#include <netax25/ax25.h>
#include <netax25/axconfig.h>

#define ax_address_length 7
#define end_address_bit 0x01
#define has_been_repeated_bit 0x80
#define max_ax25_ports 16
#define buffer_size 1500
#define ssid_mask 0x1e

int socket_file_descriptor;
struct sockaddr socket_address;
int address_size = sizeof(socket_address);
int packet_size;
struct ifreq interface_request;
int proto = ETH_P_AX25; // ETH_P_AX25 is defined on netax25/ax25.h

/*
 * Nama fungsi: print_call
 * Argumen:
 * 1. unsigned char *call_sign : pointer yang menunjukkan posisi pertama dari
 *    byte callsign ax.25
 */

void print_call(unsigned char *call_sign){
	printf("%c%c%c%c%c%c-%d", call_sign[0] >> 1, call_sign[1] >> 1,
			call_sign[2] >> 1, call_sign[3] >> 1, call_sign[4] >> 1, call_sign[5] >> 1,
			(call_sign[6] >> 1) & 0xf);
}

/*
 * Nama fungsi: change_digipeat_bit
 * Argumen:
 * 1. unsigned char *packet_buffer : pointer yang menunjukkan lokasi memory
 *    elemen pertama dari string paket yang diterima
 * 2. int packet_buffer_size : panjang string paket yang diterima
 * 3. int digipeater_address_number : urutan digipeater address yang sesuai
 *    callsign port
 */

void change_digipeat_bit(unsigned char *packet_buffer, int packet_buffer_size, int digipeater_address_number) {
  unsigned char *byte_pointer;

  byte_pointer = packet_buffer + 1 + ax_address_length + ax_address_length;

  while(digipeater_address_number != 0){
    byte_pointer += ax_address_length;
    digipeater_address_number--;
  }
  // printf("Digipeater is: ");
  // print_call(byte_pointer);
  // printf("\n");

  byte_pointer[6] |= has_been_repeated_bit;
}

/* Nama fungsi: check_digipeater_address
 * Argumen:
 * 1. unsigned char *packet_buffer : pointer yang menunjukkan lokasi memory
 *    elemen pertama dari string paket yang diterima
 * 2. int packet_buffer_size : panjang string paket yang diterima
 * 3. unsigned char *port : pointer dari port socket yang menerima paket
 */

int check_digipeater_address(unsigned char *packet_buffer, int packet_buffer_size, unsigned char *port_callsign){
  unsigned char *byte_pointer;
  int digipeater_count, iterator;

  byte_pointer = packet_buffer + 1;
  byte_pointer += ax_address_length;

  // Check the end byte of source address to see if digipeater address available
  if (byte_pointer[6] & end_address_bit) {
    return -1;
  }

  byte_pointer += ax_address_length;
  digipeater_count = 0;
  while (digipeater_count < AX25_MAX_DIGIS && ((byte_pointer - packet_buffer) < packet_buffer_size)) {
    if (byte_pointer[6] & has_been_repeated_bit) {
      byte_pointer += ax_address_length;
      digipeater_count++;
      continue;
    }
    if ((bcmp(byte_pointer, port_callsign, 6) == 0) && ((byte_pointer[6] & ssid_mask) == (port_callsign[6] & ssid_mask))) {
      // printf("Found our callsign in a digipeater address\n");
      // printf("Byte_pointer = %.7s and Socket_address = %.7s", byte_pointer, interface_request.ifr_hwaddr.sa_data);
      // printf("Packet digipeater address: ");
      // print_call(byte_pointer);
      // printf(" and my digipeater address: ");
      // print_call(interface_request.ifr_hwaddr.sa_data);
      return digipeater_count;
    }
    else {
      // printf("Found a digipeater address that is not ours\n");
      return -1;
    }
  }
}

int main(int argc, char *argv[]) {
  char *port = NULL, *device = NULL;
  int digipeater_address_number;
  unsigned char buffer[buffer_size];

  if (ax25_config_load_ports() == 0)
    printf("axdigi: no AX.25 port data configured\n");
  if (port != NULL) {
    if ((device = ax25_config_get_dev(port)) == NULL) {
      printf("axdigi: invalid port name - %s\n", port);
      return 1;
    }
  }

  if ((socket_file_descriptor = socket(AF_PACKET, SOCK_PACKET, htons(proto))) == -1) {
    perror("socket");
    return 1;
  }
  // printf("Socket has been created\n");

  while(1) {
    address_size = sizeof(socket_address);
    packet_size = recvfrom(socket_file_descriptor, buffer, buffer_size, 0, &socket_address, &address_size);

    if (packet_size == -1) {
      perror("recv");
      return 1;
    }

    if (device != NULL && strcmp(device, socket_address.sa_data) != 0)
      continue;

    strcpy(interface_request.ifr_name, socket_address.sa_data);
    if (ioctl(socket_file_descriptor, SIOCGIFHWADDR, &interface_request) < 0)
      perror("GIFHWADDR");

    if (interface_request.ifr_hwaddr.sa_family == AF_AX25) {
      digipeater_address_number = check_digipeater_address(buffer, buffer_size, interface_request.ifr_hwaddr.sa_data);
      if (digipeater_address_number == -1){
        printf("Our digipeater callsign: ");
        print_call(interface_request.ifr_hwaddr.sa_data);
        printf("was not found in the next digi hop in the packet\n");
        continue;
      }
      change_digipeat_bit(buffer, buffer_size, digipeater_address_number);
      if (sendto(socket_file_descriptor, buffer, packet_size, 0, &socket_address, address_size) == -1){
        perror("sendto");
        continue;
      }
      else {
        printf("Packet has been digipeated successfully\n");
        continue;
      }
    }
  }
  close(socket_file_descriptor);
  // printf("Socket has been closed\n");
}
