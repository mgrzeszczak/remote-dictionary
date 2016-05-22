#include <stdio.h>
#include <stdlib.h>
#include "server.h"
#include <string.h>

#define DEFAULT_PORT "12345"

void usage(char* name){
	printf("[USAGE] %s [PORT]\n",name);
	exit(EXIT_FAILURE);
}

int main(int argc, char** argv){
	run_server(argc>=2? argv[1] : DEFAULT_PORT);
	return EXIT_SUCCESS;
}
