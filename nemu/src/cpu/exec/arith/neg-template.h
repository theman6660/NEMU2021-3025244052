#include "cpu/exec/template-start.h"

#define instr neg

static void do_execute() {
	DATA_TYPE value = op_src->val;
	DATA_TYPE result = -value;
	OPERAND_W(op_src, result);

	update_eflags_pf_zf_sf((DATA_TYPE_S)result);
	cpu.eflags.CF = value != 0;
	cpu.eflags.OF = value == ((DATA_TYPE)1 << ((DATA_BYTE << 3) - 1));

	print_asm_template1();
}

make_instr_helper(rm)

#include "cpu/exec/template-end.h"
