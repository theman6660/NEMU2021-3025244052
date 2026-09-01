#include "cpu/exec/template-start.h"

#define instr adc

static void do_execute () {
	uint32_t carry = cpu.eflags.CF;
	DATA_TYPE dest = op_dest->val;
	DATA_TYPE src = op_src->val;
	uint64_t full = (uint64_t)dest + src + carry;
	DATA_TYPE result = full;
	OPERAND_W(op_dest, result);

	update_eflags_pf_zf_sf((DATA_TYPE_S)result);
	cpu.eflags.CF = full >> (DATA_BYTE << 3);
	cpu.eflags.OF = MSB(~(dest ^ src) & (dest ^ result));

	print_asm_template2();
}

make_instr_helper(r2rm)
make_instr_helper(i2a)
make_instr_helper(i2rm)
#if DATA_BYTE == 2 || DATA_BYTE == 4
make_instr_helper(si2rm)
#endif
make_instr_helper(rm2r)

#include "cpu/exec/template-end.h"
