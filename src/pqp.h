#ifndef _PQP_H
#define _PQP_H

#include <stdint.h>

enum {
	PQP_MEMORY_SIZE = 256,
	PQP_REGISTERS_COUNT = 16
};

enum {
	PQP_OP_MOV_IMM = 0, // mov rx, i16
	PQP_OP_MOV_REG,		// mov rx, ry
	PQP_OP_MOV_LD,		// mov rx, [ry]
	PQP_OP_MOV_STR,		// mov [rx], ry
	PQP_OP_CMP,			// cmp rx, ry
	PQP_OP_JMP,			// jmp i16
	PQP_OP_JG,			// jg i16
	PQP_OP_JL,			// jl i16
	PQP_OP_JE,			// je i16
	PQP_OP_ADD,			// add rx, ry
	PQP_OP_SUB,			// sub rx, ry
	PQP_OP_OR,			// or rx, ry
	PQP_OP_XOR,			// xor rx, ry
	PQP_OP_SAL,			// sal rx, i5
	PQP_OP_SAR,			// sar rx, i5
};

typedef struct PQP_Interpreter {
	uint32_t mem[PQP_MEMORY_SIZE];

	uint32_t gpr[PQP_REGISTERS_COUNT]; // General Purpose Register: r0-r15
	uint32_t pc;

	// Flags
	bool greater;
	bool less;
	bool equal;

	uint64_t cycles;
	uint32_t opcode_count;
} PQP_Interpreter;

void pqp_init(PQP_Interpreter *inter, uint8_t *rom);

int pqp_step(PQP_Interpreter *inter);

#endif

#ifdef PQP_IMPL

#include <assert.h>
#include <string.h>

static inline int32_t _pqp_sign_extend(uint16_t value) {
	return (int16_t)value;
}

void pqp_init(PQP_Interpreter *inter, uint8_t *rom) {
	assert(inter);
	assert(rom);
	memset(inter, 0, sizeof(PQP_Interpreter));

	// Load rom into RAM
	memcpy(inter->mem, rom, PQP_MEMORY_SIZE);

	inter->pc = 0x0000;
}

int pqp_step(PQP_Interpreter *inter) {
	if (inter->pc >= 0x00ff) {
		return -1;
	}

	uint8_t opcode = inter->mem[inter->pc + 0];
	uint8_t data0 = inter->mem[inter->pc + 1];
	uint8_t data1 = inter->mem[inter->pc + 2];
	uint8_t data2 = inter->mem[inter->pc + 3];

	uint16_t imm = (data2 << 8) | data1;

	inter->pc += 4; // Skip instruction

	switch (opcode) {
	case 0x00:
		// TODO: mov rx, i16
		break;
	case 0x01:
		// TODO: mov rx, ry
		break;
	case 0x02:
		// TODO: mov rx, [ry]
		break;
	case 0x03:
		// TODO: mov [rx], ry
		break;
	case 0x04:
		// TODO: cmp rx, ry
		break;
	case 0x05:
		// TODO: jmp i16
		break;
	case 0x06:
		// TODO: jg i16
		break;
	case 0x07:
		// TODO: jl i16
		break;
	case 0x08:
		// TODO: je i16
		break;
	case 0x09:
		// TODO: add rx, ry
		break;
	case 0x0a:
		// TODO: sub rx, ry
		break;
	case 0x0b:
		// TODO: and rx, ry
		break;
	case 0x0c:
		// TODO: or rx, ry
		break;
	case 0x0d:
		// TODO: xor rx, ry
		break;
	case 0x0e:
		// TODO: sal rx, i5
		break;
	case 0x0f:
		// TODO: sar rx, i5
		break;
	default:
		return -1;
	}

	return opcode;
}

#endif
