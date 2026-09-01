#include "cpu/exec/helper.h"
#include "cpu/decode/modrm.h"

make_helper(nop) {
	print_asm("nop");
	return 1;
}

make_helper(int3) {
	void do_int3();
	do_int3();
	print_asm("int3");

	return 1;
}

make_helper(lea) {
	ModR_M m;
	m.val = instr_fetch(eip + 1, 1);
	int len = load_addr(eip + 1, &m, op_src);
	reg_l(m.reg) = op_src->addr;

	print_asm("leal %s,%%%s", op_src->str, regsl[m.reg]);
	return 1 + len;
}

make_helper(leave) {
	cpu.esp = cpu.ebp;
	cpu.ebp = swaddr_read(cpu.esp, 4);
	cpu.esp += 4;
	print_asm("leave");
	return 1;
}

make_helper(sahf) {
	uint8_t value = reg_b(R_AH);
	cpu.eflags.SF = (value >> 7) & 1;
	cpu.eflags.ZF = (value >> 6) & 1;
	cpu.eflags.AF = (value >> 4) & 1;
	cpu.eflags.PF = (value >> 2) & 1;
	cpu.eflags.CF = value & 1;
	print_asm("sahf");
	return 1;
}

make_helper(lahf) {
	reg_b(R_AH) = (cpu.eflags.SF << 7) | (cpu.eflags.ZF << 6) |
			(cpu.eflags.AF << 4) | (cpu.eflags.PF << 2) | 0x2 | cpu.eflags.CF;
	print_asm("lahf");
	return 1;
}
