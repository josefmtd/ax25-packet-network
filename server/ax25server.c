#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[]) {
	setvbuf(stdin, NULL, _IOLBF, 0);
	setvbuf(stdout, NULL, _IONBF,0);

	size_t size = 64 * sizeof(char);
	char *buffer = malloc(size);

	if (getdelim(&buffer, &size, '\r', stdin) < 0) {
		fprintf(stderr, "\nFailed to read line\n");
		exit(1);
	}

	printf("Acknowledged, message sent is: %s", buffer);

	sleep(10);
	free(buffer);

	return 0;
}
