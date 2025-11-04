#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define PQP_IMPL
#include "pqp.h"

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s input\n", argv[0]);
		return EXIT_FAILURE;
	}

	PQP_Cpu cpu;
	if (!pqp_init(&cpu, argv[1])) {
		fprintf(stderr, "err: Failed to initialize CPU interpreter!");
		return EXIT_FAILURE;
	}

	pqp_run(&cpu);

	return EXIT_SUCCESS;
}
