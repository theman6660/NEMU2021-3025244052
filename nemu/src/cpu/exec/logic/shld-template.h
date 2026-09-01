#include "cpu/exec/template-start.h"

#define instr shld

#if DATA_BYTE == 2 || DATA_BYTE == 4
static void do_execute() {
	DATA_TYPE source = op_dest->val;
	DATA_TYPE destination = op_src2->val;
	uint8_t count = op_src->val & 0x1f;

	while(count != 0) {
		destination <<= 1;
		destination |= MSB(source);
		source <<= 1;
		count --;
	}

	OPERAND_W(op_src2, destination);
	print_asm("shld" str(SUFFIX) " %s,%s,%s", op_src->str, op_dest->str, op_src2->str);
}

make_helper(concat(shldi_, SUFFIX)) {
	int len = concat(decode_si_rm2r_, SUFFIX)(eip + 1);
	do_execute();
	return len + 1;
}

make_helper(concat(shldc_, SUFFIX)) {
	int len = concat(decode_rm2r_, SUFFIX)(eip + 1);
	op_src->type = OP_TYPE_REG;
	op_src->reg = R_CL;
	op_src->val = reg_b(R_CL);
	sprintf(op_src->str, "%%cl");
	do_execute();
	return len + 1;
}
#endif

#undef instr
#include "cpu/exec/template-end.h"
