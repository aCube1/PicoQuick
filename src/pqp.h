#ifndef _PQP_H
#define _PQP_H

#include <stdbool.h>
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
	PQP_OPCODE_COUNT,
};

enum {
	PQP_FLAG_GREATER = 1 << 0,
	PQP_FLAG_LESS = 1 << 1,
	PQP_FLAG_EQUAL = 1 << 2,
};

typedef struct PQP_Interpreter {
	uint32_t gpr[PQP_REGISTERS_COUNT]; // General Purpose Register: r0-r15
	uint32_t pc;
	uint8_t flags;

	uint8_t mem[PQP_MEMORY_SIZE];

	uint64_t cycles;
	uint32_t opcode_count[PQP_OPCODE_COUNT];
	bool halt;
} PQP_Interpreter;

void pqp_init(PQP_Interpreter *inter, uint8_t *rom);

// return:
//  >=0: Instruction interpreted
//	-1: Invalid register
//	-2: PC is out-of-bounds
//	-3: Is halted
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
		return -2;
	}

	if (inter->halt) {
		return -3;
	}

	// Instruction format:
	//  0x00   0x01    0x02     0x03
	// OPCODE  DATA0   DATA1    DATA2
	// OPCODE  RX_3-0  IMM_7-0  IMM_15-8
	// OPCODE  RX_3-0  RY_3-0    xxx
	// OPCODE   xxx    IMM_7-0  IMM_15-8
	// OPCODE  RX_3-0   xxx     IMM_4-0
	uint8_t opcode = inter->mem[inter->pc + 0];
	uint8_t data0 = inter->mem[inter->pc + 1];
	uint8_t data1 = inter->mem[inter->pc + 2];
	uint8_t data2 = inter->mem[inter->pc + 3];

	uint16_t imm = (data2 << 8) | data1;
	uint32_t *reg_x = data0 > 0x0f ? NULL : &inter->gpr[data0];
	uint32_t *reg_y = data1 > 0x0f ? NULL : &inter->gpr[data1];

	inter->pc += 4; // Skip instruction

	switch (opcode) {
	case 0x00: // mov rx, i16
		if (!reg_x) {
			return -1;
		}

		*reg_x = imm;
		break;
	case 0x01: // mov rx, ry
		if (!reg_x || !reg_y) {
			return -1;
		}

		*reg_x = *reg_y;
		break;
	case 0x02: // mov rx, [ry]
		if (!reg_x || !reg_y) {
			return -1;
		}
		// Load 4 bytes in little-endian order
		*reg_x = ((uint32_t)inter->mem[*reg_y + 0]) << 0  //
			   | ((uint32_t)inter->mem[*reg_y + 1]) << 8  //
			   | ((uint32_t)inter->mem[*reg_y + 2]) << 16 //
			   | ((uint32_t)inter->mem[*reg_y + 3]) << 24;
		break;
	case 0x03: // mov [rx], ry
		if (!reg_x || !reg_y) {
			return -1;
		}

		inter->mem[*reg_x + 0] = (*reg_y & 0xff) >> 0;
		inter->mem[*reg_x + 1] = (*reg_y & 0xff) >> 8;
		inter->mem[*reg_x + 2] = (*reg_y & 0xff) >> 16;
		inter->mem[*reg_x + 3] = (*reg_y & 0xff) >> 24;
		break;
	case 0x04: { // cmp rx, ry
		if (!reg_x || !reg_y) {
			return -1;
		}

		inter->flags = 0; // Reset flags

		// Convert signed values
		int32_t s_reg_x = *reg_x;
		int32_t s_reg_y = *reg_y;

		if (s_reg_x > s_reg_y)
			inter->flags |= PQP_FLAG_GREATER;
		if (s_reg_x < s_reg_y)
			inter->flags |= PQP_FLAG_LESS;
		if (s_reg_x == s_reg_y)
			inter->flags |= PQP_FLAG_EQUAL;
	} break;
	case 0x05: // jmp i16
		inter->pc += _pqp_sign_extend(imm);
		if (inter->pc == 0xf0f0) {
			inter->halt = true;
		}

		break;
	case 0x06: // jg i16
		if ((inter->flags & PQP_FLAG_GREATER) == PQP_FLAG_GREATER)
			inter->pc += _pqp_sign_extend(imm);

		break;
	case 0x07: // jl i16
		if ((inter->flags & PQP_FLAG_LESS) == PQP_FLAG_LESS)
			inter->pc += _pqp_sign_extend(imm);

		break;
	case 0x08: // je i16
		if ((inter->flags & PQP_FLAG_EQUAL) == PQP_FLAG_EQUAL)
			inter->pc += _pqp_sign_extend(imm);

		break;
	case 0x09: // add rx, ry
	case 0x0a: // sub rx, ry
	case 0x0b: // and rx, ry
	case 0x0c: // or rx, ry
	case 0x0d: // xor rx, ry
		if (!reg_x || !reg_y) {
			return -1;
		}

		if (opcode == 0x09) // add rx, ry
			*reg_x += *reg_y;
		if (opcode == 0x0a) // sub rx, ry
			*reg_x -= *reg_y;
		if (opcode == 0x0b) // and rx, ry
			*reg_x &= *reg_y;
		if (opcode == 0x0c) // or rx, ry
			*reg_x |= *reg_y;
		if (opcode == 0x0d) // xor rx, ry
			*reg_x ^= *reg_y;

		break;
	case 0x0e: // sal rx, i5
	case 0x0f: // sar rx, i5
		if (!reg_x) {
			return -1;
		}

		if (opcode == 0x0e) // sal rx, i5
			*reg_x <<= imm & 0x1f;
		if (opcode == 0x0f) // sar rx, i5
			*reg_x >>= imm & 0x1f;

		break;
	default:
		return -1;
	}

	inter->opcode_count[opcode] += 1;
	return opcode;
}

#endif
