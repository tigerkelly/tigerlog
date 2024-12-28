
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "tigerlog.h"

/* Program to test the tigerlog API. */

int main(int argc, char *argv[]) {

	int sock = tigerLogSocket();		// Create UDP socket to send log messages with.
	if (sock <= 0) {
		printf("tigerLogSocket fialed.\n");
		return -1;
	}

	tigerLogNew(sock, "Wiles");	// Creates a new log file.

	// Write a message to newly created log file.
	tigerLogMsg(sock, "Wiles", "Kelly writing to %s", "Wiles");

	tigerlogArc(sock, "Wiles");
	tigerlogDel(sock, "Wiles");

	return 0;
}
