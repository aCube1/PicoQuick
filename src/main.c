#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define PQP_IMPL
#include "pqp.h"

static uint8_t *_decode_file(const char *filepath) {
	assert(filepath);

	FILE *in = fopen(filepath, "rb");
	if (!in) {
		fprintf(stderr, "Unable to open file at %s\n", filepath);
		return NULL;
	}

	uint8_t buf[PQP_MEMORY_SIZE];
	size_t bufsize = 0;
	memset(buf, 0, sizeof(uint8_t) * PQP_MEMORY_SIZE);

	int c;
	while ((c = fgetc(in)) != EOF) {
		if (bufsize >= PQP_MEMORY_SIZE)
			break;

		buf[bufsize] = c;
		bufsize += 1;
	}

	uint8_t *rom = malloc(sizeof(uint8_t) * PQP_MEMORY_SIZE);
	if (rom) {
		memcpy(rom, buf, sizeof(uint8_t) * PQP_MEMORY_SIZE);
	}

	return rom;
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s input\n", argv[0]);
		return EXIT_SUCCESS;
	}

	uint8_t *rom = _decode_file(argv[1]);
	if (!rom) {
		fprintf(stderr, "err: Failed to decode file at %s\n", argv[1]);
		return EXIT_FAILURE;
	}

	PQP_Interpreter inter;
	pqp_init(&inter, rom);
	free(rom);

	// for (size_t i = 0; i < PQP_MEMORY_SIZE; i += 4) {
	// 	fprintf(
	// 		stderr, "%02x%02x %02x%02x%c", inter.mem[i + 0], inter.mem[i + 1],
	// 		inter.mem[i + 2], inter.mem[i + 3], ((i / 4) % 4 == 3) ? '\n' : ' '
	// 	);
	// }
	// fprintf(stderr, "\n");

	while (!inter.halt) {
		int res = pqp_step(&inter, NULL);
		if (res < 0) {
			char *err;
			switch (res) {
			case -1:
				err = "Invalid instruction!";
				break;
			case -2:
				err = "CPU has halted!";
				break;
			}

			fprintf(stderr, "err: Interpreter has found error - %s\n", err);
			break;
		}
	}

	return EXIT_SUCCESS;
}
