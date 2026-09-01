#ifndef __MONITOR_ELF_H__
#define __MONITOR_ELF_H__

#include "common.h"

void load_elf_tables(int argc, char *argv[]);
bool lookup_symbol(const char *name, uint32_t *address);
const char *lookup_function(swaddr_t address, uint32_t *start);
void print_backtrace(void);

#endif
