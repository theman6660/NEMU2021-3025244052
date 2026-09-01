#include "cpu/exec/template-start.h"

#define instr push

static void do_execute() {
	cpu.esp -= DATA_BYTE;
	swaddr_write(cpu.esp, DATA_BYTE, op_src->val);
	print_asm_template1();
}

make_instr_helper(i)
make_instr_helper(r)
make_instr_helper(rm)

#undef instr
#define instr pop

static void concat(do_pop_, SUFFIX)() {
	DATA_TYPE value = swaddr_read(cpu.esp, DATA_BYTE);
	OPERAND_W(op_src, value);
	cpu.esp += DATA_BYTE;
	print_asm_template1();
}

make_helper(concat(pop_r_, SUFFIX)) {
	return idex(eip, concat(decode_r_, SUFFIX), concat(do_pop_, SUFFIX));
}

make_helper(concat(pop_rm_, SUFFIX)) {
	return idex(eip, concat(decode_rm_, SUFFIX), concat(do_pop_, SUFFIX));
}

#undef instr
#include "cpu/exec/template-end.h"
