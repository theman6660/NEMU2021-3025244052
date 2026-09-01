#include "cpu/exec/helper.h"

#define DATA_BYTE 2
#include "stack-template.h"
#undef DATA_BYTE

#define DATA_BYTE 4
#include "stack-template.h"
#undef DATA_BYTE

make_helper_v(push_i)
make_helper_v(push_r)
make_helper_v(push_rm)
make_helper_v(pop_r)
make_helper_v(pop_rm)

make_helper(push_si_b) {
	int32_t value = (int8_t)instr_fetch(eip + 1, 1);
	size_t size = ops_decoded.is_operand_size_16 ? 2 : 4;
	cpu.esp -= size;
	swaddr_write(cpu.esp, size, value);
	print_asm("push $0x%x", value);
	return 2;
}
