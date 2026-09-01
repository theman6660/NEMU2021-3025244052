#include "cpu/exec/helper.h"

bool condition_passed(uint8_t condition) {
	switch(condition & 0xf) {
		case 0x0: return cpu.eflags.OF;
		case 0x1: return !cpu.eflags.OF;
		case 0x2: return cpu.eflags.CF;
		case 0x3: return !cpu.eflags.CF;
		case 0x4: return cpu.eflags.ZF;
		case 0x5: return !cpu.eflags.ZF;
		case 0x6: return cpu.eflags.CF || cpu.eflags.ZF;
		case 0x7: return !cpu.eflags.CF && !cpu.eflags.ZF;
		case 0x8: return cpu.eflags.SF;
		case 0x9: return !cpu.eflags.SF;
		case 0xa: return cpu.eflags.PF;
		case 0xb: return !cpu.eflags.PF;
		case 0xc: return cpu.eflags.SF != cpu.eflags.OF;
		case 0xd: return cpu.eflags.SF == cpu.eflags.OF;
		case 0xe: return cpu.eflags.ZF || (cpu.eflags.SF != cpu.eflags.OF);
		case 0xf: return !cpu.eflags.ZF && (cpu.eflags.SF == cpu.eflags.OF);
		default: return false;
	}
}

make_helper(jcc_b) {
	int8_t displacement = instr_fetch(eip + 1, 1);
	if(condition_passed(ops_decoded.opcode)) {
		cpu.eip += displacement;
	}
	print_asm("jcc 0x%x", eip + 2 + displacement);
	return 2;
}

make_helper(jcc_l) {
	int32_t displacement = instr_fetch(eip + 1, 4);
	if(condition_passed(ops_decoded.opcode)) {
		cpu.eip += displacement;
	}
	print_asm("jcc 0x%x", eip + 5 + displacement);
	return 5;
}
