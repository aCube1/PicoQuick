#ifndef _PQP_H
#define _PQP_H

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum {
	PQP_MEMORY_SIZE = 256,
	PQP_REGISTERS_COUNT = 16,
	PQP_PRINTED_PCS_SIZE = PQP_MEMORY_SIZE / 4,
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

typedef struct PQP_Cpu {
	uint32_t regs[PQP_REGISTERS_COUNT]; // General Purpose Register: r0-r15
	uint32_t pc;
	uint8_t flags;

	uint8_t mem[PQP_MEMORY_SIZE];

	uint64_t cycles;
	uint32_t opcode_count[PQP_OPCODE_COUNT];
	bool printed[PQP_PRINTED_PCS_SIZE];
} PQP_Cpu;

bool pqp_init(PQP_Cpu *cpu, const char *filename);

// Run loop and printou trace to stderr
void pqp_run(PQP_Cpu *cpu);

// return:
//  >=0: Instruction interpreted
//	-1: Is halted
int pqp_step(PQP_Cpu *cpu, FILE *outstream);
void pqp_print_statistics(const PQP_Cpu *cpu, FILE *outstream);

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

static bool _pqp_is_hex_file_format(FILE *file) {
	char buf[16];
	size_t bytes = fread(buf, 1, 15, file);
	if (bytes == 0) {
		if (ferror(file))
			fprintf(stderr, "err: failed to read file");

		fseek(file, 0, SEEK_SET);
		return false;
	}

	buf[bytes] = '\0';
	bool is_hex = strstr(buf, "0x") != NULL || strstr(buf, "0X") != NULL;

	fseek(file, 0, SEEK_SET);
	return is_hex;
}

static bool _pqp_load_from_binary(PQP_Cpu *cpu, FILE *file) {
	assert(file);

	// HACK: This should be sufficient, since we can't open files >2GiB anyway
	fseek(file, 0, SEEK_END);
	long fsize = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (fsize != PQP_MEMORY_SIZE) {
		return false;
	}

	fread(cpu->mem, 1, PQP_MEMORY_SIZE, file);
	return true;
}

static bool _pqp_load_from_text(PQP_Cpu *cpu, FILE *file) {
	uint8_t rom[PQP_MEMORY_SIZE];
	size_t romsize = 0;
	memset(rom, 0, sizeof(uint8_t) * PQP_MEMORY_SIZE);

	// HACK: This should be sufficient, since we can't open files >2GiB anyway
	fseek(file, 0, SEEK_END);
	long fsize = ftell(file);
	fseek(file, 0, SEEK_SET);

	char *content = malloc(fsize + 1); // +1 for null-terminator
	if (!content) {
		fprintf(stderr, "err: Failed to allocate memory to read file");
		return false;
	}

	size_t read_bytes = fread(content, 1, fsize, file);
	content[read_bytes] = '\0';

	char *ptr = content;
	while (*ptr != '\0' && romsize < PQP_MEMORY_SIZE) {
		if (*ptr == '0' && (ptr[1] == 'x' || ptr[1] == 'X')) {
			unsigned long value = strtoul(ptr, &ptr, 16);
			rom[romsize] = (uint8_t)value;
			romsize += 1;
		} else {
			ptr += 1;
		}
	}
	free(content);

	// Load rom into RAM
	memcpy(cpu->mem, rom, sizeof(uint8_t) * PQP_MEMORY_SIZE);
	return true;
}

bool pqp_init(PQP_Cpu *cpu, const char *filename) {
	assert(cpu);
	assert(filename);

	// Reset CPU
	memset(cpu, 0, sizeof(PQP_Cpu));
	cpu->pc = 0x0000;

	FILE *in = fopen(filename, "rb");
	if (!in) {
		fprintf(stderr, "err: Unable to open file at %s\n", filename);
		return NULL;
	}

	bool success;
	if (_pqp_is_hex_file_format(in)) {
		success = _pqp_load_from_text(cpu, in);
	} else {
		success = _pqp_load_from_binary(cpu, in);
	}

	fclose(in);
	return success;
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

static void _pqp_print_opcode(PQP_Cpu *cpu, FILE *stream, uint32_t instruction) {
	uint32_t pc_index = cpu->pc / 4;
	bool should_print = (pc_index < PQP_PRINTED_PCS_SIZE) && !cpu->printed[pc_index];

	if (!should_print)
		return;

	cpu->printed[pc_index] = true;

	uint8_t opcode = instruction & 0xff;
	uint8_t reg_y = (instruction >> 8) & 0x0f;
	uint8_t reg_x = (instruction >> 12) & 0x0f;
	int32_t i16 = _pqp_sign_extend((instruction >> 16) & 0xffff);
	uint8_t i5 = (instruction >> 24) & 0x1f;

	uint16_t target_pc = cpu->pc + _pqp_sign_extend(i16) + 4;

	fprintf(stream, "0x%04X->", cpu->pc);

	switch (opcode) {
	case PQP_OP_MOV_IMM: // mov rx, i16
		fprintf(stream, "MOV_R%d=0x%08X\n", reg_x, i16);
		break;
	case PQP_OP_MOV_REG: // mov rx, ry
		fprintf(stream, "MOV_R%d=R%d=0x%08X\n", reg_x, reg_y, cpu->regs[reg_y]);
		break;
	case PQP_OP_MOV_LD: { // mov rx, [ry]
		uint16_t addr = cpu->regs[reg_y] & 0xff;

		uint8_t mem0 = (addr + 0 >= PQP_MEMORY_SIZE) ? 0 : cpu->mem[addr + 0];
		uint8_t mem1 = (addr + 1 >= PQP_MEMORY_SIZE) ? 0 : cpu->mem[addr + 1];
		uint8_t mem2 = (addr + 2 >= PQP_MEMORY_SIZE) ? 0 : cpu->mem[addr + 2];
		uint8_t mem3 = (addr + 3 >= PQP_MEMORY_SIZE) ? 0 : cpu->mem[addr + 3];

		fprintf(
			stream,
			"MOV_R%d=MEM[0x%02X,0x%02X,0x%02X,0x%02X]=[0x%02X,0x%02X,0x%02X,0x%02X]\n",
			reg_x, addr + 0, addr + 1, addr + 2, addr + 3, mem0, mem1, mem2, mem3
		);
	} break;
	case PQP_OP_MOV_STR: { // mov [rx], ry
		uint16_t addr = cpu->regs[reg_x] & 0xff;

		uint8_t byte0 = (cpu->regs[reg_y] >> 0) & 0xff;
		uint8_t byte1 = (cpu->regs[reg_y] >> 8) & 0xff;
		uint8_t byte2 = (cpu->regs[reg_y] >> 16) & 0xff;
		uint8_t byte3 = (cpu->regs[reg_y] >> 24) & 0xff;

		fprintf(
			stream,
			"MOV_MEM[0x%02X,0x%02X,0x%02X,0x%02X]=R%d=[0x%02X,0x%02X,0x%02X,0x%02X]\n",
			addr + 0, addr + 1, addr + 2, addr + 3, reg_y, byte0, byte1, byte2, byte3
		);
	} break;
	case PQP_OP_CMP: { // cmp rx, ry
		int64_t s_reg_x = (int32_t)cpu->regs[reg_x];
		int64_t s_reg_y = (int32_t)cpu->regs[reg_y];

		fprintf(
			stream, "CMP_R%d<=>R%d(G=%d,L=%d,E=%d)\n", //
			reg_x, reg_y,							   //
			s_reg_x > s_reg_y ? 1 : 0,				   //
			s_reg_x < s_reg_y ? 1 : 0,				   //
			s_reg_x == s_reg_y ? 1 : 0
		);
	} break;
	case PQP_OP_JMP: // jmp i16
		fprintf(stream, "JMP_0x%04X\n", target_pc);
		break;
	case PQP_OP_JG: // jg i16
		fprintf(stream, "JG_0x%04X\n", target_pc);
		break;
	case PQP_OP_JL: // jl i16
		fprintf(stream, "JL_0x%04X\n", target_pc);
		break;
	case PQP_OP_JE: // je i16
		fprintf(stream, "JE_0x%04X\n", target_pc);
		break;
	case PQP_OP_ADD: // add rx, ry
		fprintf(
			stream, "ADD_R%d+=R%d=0x%08X+0x%08X=0x%08X\n", //
			reg_x, reg_y,								   //
			cpu->regs[reg_x], cpu->regs[reg_y],			   //
			cpu->regs[reg_x] + cpu->regs[reg_y]
		);
		break;
	case PQP_OP_SUB: // sub rx, ry
		fprintf(
			stream, "SUB_R%d-=R%d=0x%08X-0x%08X=0x%08X\n", //
			reg_x, reg_y,								   //
			cpu->regs[reg_x], cpu->regs[reg_y],			   //
			cpu->regs[reg_x] - cpu->regs[reg_y]
		);
		break;
	case PQP_OP_AND: // and rx, ry
		fprintf(
			stream, "AND_R%d&=R%d=0x%08X&0x%08X=0x%08X\n", //
			reg_x, reg_y,								   //
			cpu->regs[reg_x], cpu->regs[reg_y],			   //
			cpu->regs[reg_x] & cpu->regs[reg_y]
		);
		break;
	case PQP_OP_OR: // or rx, ry
		fprintf(
			stream, "OR_R%d|=R%d=0x%08X|0x%08X=0x%08X\n", //
			reg_x, reg_y,								  //
			cpu->regs[reg_x], cpu->regs[reg_y],			  //
			cpu->regs[reg_x] | cpu->regs[reg_y]
		);
		break;
	case PQP_OP_XOR: // and rx, ry
		fprintf(
			stream, "XOR_R%d^=R%d=0x%08X^0x%08X=0x%08X\n", //
			reg_x, reg_y,								   //
			cpu->regs[reg_x], cpu->regs[reg_y],			   //
			cpu->regs[reg_x] ^ cpu->regs[reg_y]
		);
		break;
	case PQP_OP_SAL: // sal rx, i5
		fprintf(
			stream, "SAL_R%d<<=%d=0x%08X<<%d=0x%08X\n", //
			reg_x, i5,									//
			cpu->regs[reg_x], i5, cpu->regs[reg_x] << i5
		);
		break;
	case PQP_OP_SAR: // sar rx, i5
		fprintf(
			stream, "SAR_R%d>>=%d=0x%08X>>%d=0x%08X\n", //
			reg_x, i5,									//
			cpu->regs[reg_x], i5, cpu->regs[reg_x] >> i5
		);
		break;
	}
}

int pqp_step(PQP_Cpu *cpu, FILE *outstream) {
	if (cpu->pc == 0xf0f0) {
		return -1;
	}

	// Decode instruction
	uint32_t instruction = _pqp_load_immediate(cpu->mem, cpu->pc);

	uint8_t opcode = instruction & 0xff;
	uint8_t reg_y = (instruction >> 8) & 0x0f;
	uint8_t reg_x = (instruction >> 12) & 0x0f;
	int32_t i16 = _pqp_sign_extend((instruction >> 16) & 0xffff);
	uint8_t i5 = (instruction >> 24) & 0x1f;

	uint16_t target_pc = cpu->pc + _pqp_sign_extend(i16);

	FILE *stream = outstream ? outstream : stderr;
	_pqp_print_opcode(cpu, stream, instruction);

	switch (opcode) {
	case 0x00: // mov rx, i16
		cpu->regs[reg_x] = _pqp_sign_extend(i16);
		break;
	case 0x01: // mov rx, ry
		cpu->regs[reg_x] = cpu->regs[reg_y];
		break;
	case 0x02: // mov rx, [ry]
		cpu->regs[reg_x] = _pqp_load_immediate(cpu->mem, cpu->regs[reg_y] & 0xff);
		break;
	case 0x03: // mov [rx], ry
		_pqp_store_immediate(cpu->mem, cpu->regs[reg_x] & 0xff, cpu->regs[reg_y]);
		break;
	case 0x04: { // cmp rx, ry
		// Convert to signed
		int64_t s_reg_x = (int32_t)cpu->regs[reg_x];
		int64_t s_reg_y = (int32_t)cpu->regs[reg_y];

		cpu->flags = 0; // Reset flags
		if (s_reg_x > s_reg_y)
			cpu->flags |= PQP_FLAG_GREATER;
		if (s_reg_x < s_reg_y)
			cpu->flags |= PQP_FLAG_LESS;
		if (s_reg_x == s_reg_y)
			cpu->flags |= PQP_FLAG_EQUAL;

	} break;
	case 0x05: // jmp i16
		cpu->pc = target_pc;
		break;
	case 0x06: // jg i16
		if ((cpu->flags & PQP_FLAG_GREATER) == PQP_FLAG_GREATER)
			cpu->pc = target_pc;
		break;
	case 0x07: // jl i16
		if ((cpu->flags & PQP_FLAG_LESS) == PQP_FLAG_LESS)
			cpu->pc = target_pc;
		break;
	case 0x08: // je i16
		if ((cpu->flags & PQP_FLAG_EQUAL) == PQP_FLAG_EQUAL)
			cpu->pc = target_pc;
		break;
	case 0x09: // add rx, ry
		cpu->regs[reg_x] += cpu->regs[reg_y];
		break;
	case 0x0a: // sub rx, ry
		cpu->regs[reg_x] -= cpu->regs[reg_y];
		break;
	case 0x0b: // and rx, ry
		cpu->regs[reg_x] &= cpu->regs[reg_y];
		break;
	case 0x0c: // or rx, ry
		cpu->regs[reg_x] |= cpu->regs[reg_y];
		break;
	case 0x0d: // xor rx, ry
		cpu->regs[reg_x] ^= cpu->regs[reg_y];
		break;
	case 0x0e: // sal rx, i5
		cpu->regs[reg_x] <<= i5;
		break;
	case 0x0f: // sar rx, i5
		cpu->regs[reg_x] >>= i5;
		break;
	default:
		fprintf(stderr, "FATAL ERROR! Unknown Instruction %08X at %04X", opcode, cpu->pc);
		abort();
	}

	cpu->pc += 4; // Skip instruction
	cpu->opcode_count[opcode] += 1;
	cpu->cycles += 1;

	return opcode;
}

void pqp_print_statistics(const PQP_Cpu *cpu, FILE *outstream) {
	FILE *stream = outstream ? outstream : stderr;

	fprintf(stream, "[");
	for (int i = 0; i < PQP_REGISTERS_COUNT; i += 1) {
		fprintf(stream, "%02X:%u", i, cpu->opcode_count[i]);
		if (i != PQP_REGISTERS_COUNT - 1)
			fprintf(stream, ",");
	}
	fprintf(stream, "]\n");

	fprintf(stream, "[");
	for (int i = 0; i < PQP_REGISTERS_COUNT; i += 1) {
		fprintf(stream, "R%d=0x%08X", i, cpu->regs[i]);
		if (i != PQP_REGISTERS_COUNT - 1)
			fprintf(stream, ",");
	}
	fprintf(stream, "]\n");
}

void pqp_run(PQP_Cpu *cpu) {
	FILE *out = stderr;

	while (true) {
		int res = pqp_step(cpu, out);
		if (res < 0) {
			fprintf(out, "0x%04X->EXIT\n", cpu->pc);
			break;
		}
	}

	pqp_print_statistics(cpu, out);
}

#endif
