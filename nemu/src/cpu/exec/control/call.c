#include "cpu/exec/helper.h"

make_helper(call_rel_l) {
	int32_t displacement = instr_fetch(eip + 1, 4);
	uint32_t return_address = eip + 5;
	cpu.esp -= 4;
	swaddr_write(cpu.esp, 4, return_address);
	cpu.eip += displacement;
	print_asm("call 0x%x", return_address + displacement);
	return 5;
}

make_helper(call_rm_l) {
	int len = decode_rm_l(eip + 1);
	uint32_t return_address = eip + len + 1;
	cpu.esp -= 4;
	swaddr_write(cpu.esp, 4, return_address);
	cpu.eip = op_src->val - (len + 1);
	print_asm("call *%s", op_src->str);
	return len + 1;
}

make_helper(ret) {
	uint32_t target = swaddr_read(cpu.esp, 4);
	cpu.esp += 4;
	cpu.eip = target - 1;
	print_asm("ret");
	return 1;
}

make_helper(ret_i_w) {
	uint16_t bytes_to_release = instr_fetch(eip + 1, 2);
	uint32_t target = swaddr_read(cpu.esp, 4);
	cpu.esp += 4 + bytes_to_release;
	cpu.eip = target - 3;
	print_asm("ret $0x%x", bytes_to_release);
	return 3;
}
