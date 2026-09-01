#ifndef __WATCHPOINT_H__
#define __WATCHPOINT_H__

#include "common.h"

typedef struct watchpoint {
	int NO;
	struct watchpoint *next;
	char expression[128];
	uint32_t value;
} WP;

WP *new_wp(void);
void free_wp(WP *wp);
bool delete_watchpoint(int no);
void print_watchpoints(void);
bool check_watchpoints(swaddr_t eip);

#endif
