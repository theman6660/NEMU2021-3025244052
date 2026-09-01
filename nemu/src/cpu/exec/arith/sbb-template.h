#include "cpu/exec/template-start.h"

#define instr sbb

static void do_execute () {
	uint32_t borrow = cpu.eflags.CF;
	DATA_TYPE dest = op_dest->val;
	DATA_TYPE src = op_src->val;
	uint64_t subtrahend = (uint64_t)src + borrow;
	DATA_TYPE result = dest - subtrahend;
	OPERAND_W(op_dest, result);

	update_eflags_pf_zf_sf((DATA_TYPE_S)result);
	cpu.eflags.CF = (uint64_t)dest < subtrahend;
	cpu.eflags.OF = MSB((dest ^ src) & (dest ^ result));

	print_asm_template2();
}

make_instr_helper(r2rm)
make_instr_helper(rm2r)
make_instr_helper(i2a)
make_instr_helper(i2rm)
#if DATA_BYTE == 2 || DATA_BYTE == 4
make_instr_helper(si2rm)
#endif

#include "cpu/exec/template-end.h"
