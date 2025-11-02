#ifndef _PQP_H
#define _PQP_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

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
	PQP_OP_AND,			// and rx, ry
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
//	-1: Invalid Instruction
//	-2: Is halted
int pqp_step(PQP_Interpreter *inter, FILE *outstream);

#endif

#ifdef PQP_IMPL

#include <assert.h>
#include <string.h>

static inline int32_t _pqp_sign_extend(uint16_t value) {
	return (int16_t)value;
}

static inline uint32_t _pqp_load_immediate(const uint8_t *mem, uint32_t addr) {
	if (addr + 3 >= PQP_MEMORY_SIZE) {
		return 0;
	}

	return mem[addr + 0] << 0  //
		 | mem[addr + 1] << 8  //
		 | mem[addr + 2] << 16 //
		 | mem[addr + 3] << 24;
}

static inline void _pqp_store_immediate(uint8_t *mem, uint32_t addr, uint32_t data) {
	if (addr + 3 >= PQP_MEMORY_SIZE) {
		return;
	}

	mem[addr + 0] = (data >> 0) & 0xff;
	mem[addr + 1] = (data >> 8) & 0xff;
	mem[addr + 2] = (data >> 16) & 0xff;
	mem[addr + 3] = (data >> 24) & 0xff;
}

void pqp_init(PQP_Interpreter *inter, uint8_t *rom) {
	assert(inter);
	assert(rom);
	memset(inter, 0, sizeof(PQP_Interpreter));

	// Load rom into RAM
	memcpy(inter->mem, rom, sizeof(uint8_t) * PQP_MEMORY_SIZE);

	inter->pc = 0x0000;
}

int pqp_step(PQP_Interpreter *inter, FILE *outstream) {
	FILE *stream = outstream ? outstream : stderr;
	fprintf(stream, "0x%04X->", inter->pc);

	if (inter->pc == 0xf0f0) {
		inter->halt = true;
		fprintf(outstream, "EXIT\n");
		return -2;
	}

	// Decode instruction
	uint32_t instruction = _pqp_load_immediate(inter->mem, inter->pc);

	uint8_t opcode = instruction & 0xff;
	uint8_t reg_y = (instruction >> 8) & 0x0f;
	uint8_t reg_x = (instruction >> 12) & 0x0f;
	uint16_t i16 = (instruction >> 16) & 0xffff;
	uint8_t i5 = (instruction >> 24) & 0x1f;

	uint32_t *p_reg_x = &inter->gpr[reg_x];
	uint32_t *p_reg_y = &inter->gpr[reg_y];

	switch (opcode) {
	case 0x00: // mov rx, i16
		fprintf(stream, "MOV_R%d=0x%08X\n", reg_x, i16);

		inter->gpr[reg_x] = _pqp_sign_extend(i16);
		break;
	case 0x01: // mov rx, ry
		fprintf(stream, "MOV_R%d=R%d=0x%08X\n", reg_x, reg_y, inter->gpr[reg_y]);

		inter->gpr[reg_x] = inter->gpr[reg_y];
		break;
	case 0x02: { // mov rx, [ry]
		uint16_t addr = inter->gpr[reg_y] & 0xff;

		uint8_t mem0 = (addr + 0 >= PQP_MEMORY_SIZE) ? 0 : inter->mem[addr + 0];
		uint8_t mem1 = (addr + 1 >= PQP_MEMORY_SIZE) ? 0 : inter->mem[addr + 1];
		uint8_t mem2 = (addr + 2 >= PQP_MEMORY_SIZE) ? 0 : inter->mem[addr + 2];
		uint8_t mem3 = (addr + 3 >= PQP_MEMORY_SIZE) ? 0 : inter->mem[addr + 3];

		fprintf(
			stream,
			"MOV_R%d=MEM[0x%02X,0x%02X,0x%02X,0x%02X]=[0x%02X,0x%02X,0x%02X,0x%02X]\n",
			reg_x, addr + 0, addr + 1, addr + 2, addr + 3, mem0, mem1, mem2, mem3
		);

		inter->gpr[reg_x] = _pqp_load_immediate(inter->mem, addr);
	} break;
	case 0x03: { // mov [rx], ry
		uint16_t addr = inter->gpr[reg_x] & 0xff;

		uint8_t byte0 = (inter->gpr[reg_y] >> 0) & 0xff;
		uint8_t byte1 = (inter->gpr[reg_y] >> 8) & 0xff;
		uint8_t byte2 = (inter->gpr[reg_y] >> 16) & 0xff;
		uint8_t byte3 = (inter->gpr[reg_y] >> 24) & 0xff;

		fprintf(
			stream,
			"MOV_MEM[0x%02X,0x%02X,0x%02X,0x%02X]=R%d=[0x%02X,0x%02X,0x%02X,0x%02X]\n",
			addr + 0, addr + 1, addr + 2, addr + 3, reg_y, byte0, byte1, byte2, byte3
		);

		_pqp_store_immediate(inter->mem, addr, inter->gpr[reg_y]);
	} break;
	case 0x04: { // cmp rx, ry

		// Convert signed values
		int32_t s_reg_x = *p_reg_x;
		int32_t s_reg_y = *p_reg_y;

		inter->flags = 0; // Reset flags
		if (s_reg_x > s_reg_y)
			inter->flags |= PQP_FLAG_GREATER;
		if (s_reg_x < s_reg_y)
			inter->flags |= PQP_FLAG_LESS;
		if (s_reg_x == s_reg_y)
			inter->flags |= PQP_FLAG_EQUAL;

		fprintf(
			stream, "CMP_R%d<=>R%d(G=%d,L=%d,E=%d)\n", //
			reg_x, reg_y,							   //
			(inter->flags & PQP_FLAG_GREATER) != 0,	   //
			(inter->flags & PQP_FLAG_LESS) != 0,	   //
			(inter->flags & PQP_FLAG_EQUAL) != 0
		);
	} break;
	case 0x05: { // jmp i16
		uint32_t target_pc = inter->pc + _pqp_sign_extend(i16);
		inter->pc = target_pc;

		fprintf(stream, "JMP_0x%04X\n", target_pc + 4);
	} break;
	case 0x06: { // jg i16
		uint32_t target_pc = inter->pc + _pqp_sign_extend(i16);
		if ((inter->flags & PQP_FLAG_GREATER) == PQP_FLAG_GREATER)
			inter->pc += target_pc;

		fprintf(stream, "JG_0x%04X\n", target_pc + 4);
	} break;
	case 0x07: { // jl i16
		uint32_t target_pc = inter->pc + _pqp_sign_extend(i16);
		if ((inter->flags & PQP_FLAG_LESS) == PQP_FLAG_LESS)
			inter->pc += target_pc;

		fprintf(stream, "JL_0x%04X\n", target_pc + 4);
	} break;
	case 0x08: { // je i16
		uint32_t target_pc = inter->pc + _pqp_sign_extend(i16);
		if ((inter->flags & PQP_FLAG_EQUAL) == PQP_FLAG_EQUAL)
			inter->pc += target_pc;

		fprintf(stream, "JE_0x%04X\n", target_pc + 4);
	} break;
	case 0x09: // add rx, ry
		fprintf(
			stream, "ADD_R%d+=R%d=0x%08X+0x%08X=0x%08X\n", //
			reg_x, reg_y,								   //
			*p_reg_x, *p_reg_y, *p_reg_x + *p_reg_y
		);

		*p_reg_x += *p_reg_y;
		break;
	case 0x0a: // sub rx, ry
		fprintf(
			stream, "SUB_R%d-=R%d=0x%08X-0x%08X=0x%08X\n", //
			reg_x, reg_y,								   //
			*p_reg_x, *p_reg_y, *p_reg_x - *p_reg_y
		);

		*p_reg_x -= *p_reg_y;
		break;
	case 0x0b: // and rx, ry
		fprintf(
			stream, "AND_R%d&=R%d=0x%08X&0x%08X=0x%08X\n", //
			reg_x, reg_y,								   //
			*p_reg_x, *p_reg_y, *p_reg_x & *p_reg_y
		);

		*p_reg_x &= *p_reg_y;
		break;
	case 0x0c: // or rx, ry
		fprintf(
			stream, "OR_R%d|=R%d=0x%08X|0x%08X=0x%08X\n", //
			reg_x, reg_y,								  //
			*p_reg_x, *p_reg_y, *p_reg_x | *p_reg_y
		);

		*p_reg_x |= *p_reg_y;
		break;
	case 0x0d: // xor rx, ry
		fprintf(
			stream, "XOR_R%d^=R%d=0x%08X^0x%08X=0x%08X\n", //
			reg_x, reg_y,								   //
			*p_reg_x, *p_reg_y, *p_reg_x ^ *p_reg_y
		);

		*p_reg_x ^= *p_reg_y;
		break;
	case 0x0e: // sal rx, i5
		fprintf(
			stream, "SAL_R%d<<=%d=0x%08X<<%d=0x%08X\n", //
			reg_x, i5,									//
			*p_reg_x, i5, *p_reg_x << i5
		);

		*p_reg_x <<= i5;
		break;
	case 0x0f: // sar rx, i5
		fprintf(
			stream, "SAR_R%d>>=%d=0x%08X>>%d=0x%08X\n", //
			reg_x, i5,									//
			*p_reg_x, i5, *p_reg_x >> i5
		);

		*p_reg_x >>= i5;
		break;
	default:
		return -1;
	}

	inter->pc += 4; // Skip instruction
	inter->opcode_count[opcode] += 1;
	return opcode;
}

#endif
