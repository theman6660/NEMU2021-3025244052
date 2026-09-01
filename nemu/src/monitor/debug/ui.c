#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "monitor/elf.h"
#include "monitor/watchpoint.h"
#include "nemu.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

void cpu_exec(uint32_t);

/* We use the `readline' library to provide more flexibility to read from stdin. */
char* rl_gets() {
	static char *line_read = NULL;

	if(line_read) {
		free(line_read);
		line_read = NULL;
	}

	line_read = readline("(nemu) ");

	if(line_read && *line_read) {
		add_history(line_read);
	}

	return line_read;
}

static char *skip_spaces(char *text) {
	while(text != NULL && (*text == ' ' || *text == '\t')) {
		text ++;
	}
	return text;
}

static bool parse_u32(char *text, int base, uint32_t *value, char **end_out) {
	char *end = NULL;
	unsigned long parsed;

	text = skip_spaces(text);
	if(text == NULL || *text == '\0' || *text == '-') {
		return false;
	}

	errno = 0;
	parsed = strtoul(text, &end, base);
	if(errno == ERANGE || end == text || parsed > UINT32_MAX) {
		return false;
	}

	*value = (uint32_t)parsed;
	if(end_out != NULL) {
		*end_out = end;
	}
	return true;
}

static int cmd_c(char *args) {
	(void)args;
	cpu_exec(UINT32_MAX);
	return 0;
}

static int cmd_q(char *args) {
	(void)args;
	return -1;
}

static int cmd_si(char *args) {
	uint32_t count = 1;
	char *end = NULL;

	if(args != NULL) {
		if(!parse_u32(args, 10, &count, &end) || count == 0 || *skip_spaces(end) != '\0') {
			printf("Usage: si [positive instruction count]\n");
			return 0;
		}
	}

	cpu_exec(count);
	return 0;
}

static void print_registers(void) {
	int i;
	for(i = 0; i < 8; i ++) {
		printf("%-7s 0x%08x %10u\n", regsl[i], reg_l(i), reg_l(i));
	}
	printf("%-7s 0x%08x %10u\n", "eip", cpu.eip, cpu.eip);
	printf("%-7s 0x%08x %10u\n", "eflags", cpu.eflags.val, cpu.eflags.val);
}

static int cmd_info(char *args) {
	char *subcommand;

	args = skip_spaces(args);
	if(args == NULL || *args == '\0') {
		printf("Usage: info r|w\n");
		return 0;
	}

	subcommand = strtok(args, " \t");
	if(strcmp(subcommand, "r") == 0) {
		print_registers();
	}
	else if(strcmp(subcommand, "w") == 0) {
		print_watchpoints();
	}
	else {
		printf("Unknown info subcommand '%s'. Use 'info r' or 'info w'.\n", subcommand);
	}
	return 0;
}

static int cmd_x(char *args) {
	uint32_t count;
	char *end = NULL;
	char *expression;
	bool success = true;
	uint32_t start;
	uint32_t i;

	if(!parse_u32(args, 10, &count, &end) || count == 0) {
		printf("Usage: x N EXPR\n");
		return 0;
	}

	expression = skip_spaces(end);
	if(expression == NULL || *expression == '\0') {
		printf("Usage: x N EXPR\n");
		return 0;
	}

	start = expr(expression, &success);
	if(!success) {
		return 0;
	}

	for(i = 0; i < count; i ++) {
		uint32_t address = start + i * 4;
		printf("0x%08x: 0x%08x\n", address, swaddr_read(address, 4));
	}
	return 0;
}

static int cmd_p(char *args) {
	bool success = true;
	uint32_t value;

	args = skip_spaces(args);
	if(args == NULL || *args == '\0') {
		printf("Usage: p EXPR\n");
		return 0;
	}

	value = expr(args, &success);
	if(success) {
		printf("0x%08x (%u)\n", value, value);
	}
	return 0;
}

static int cmd_w(char *args) {
	bool success = true;
	uint32_t value;
	WP *wp;

	args = skip_spaces(args);
	if(args == NULL || *args == '\0') {
		printf("Usage: w EXPR\n");
		return 0;
	}
	if(strlen(args) >= sizeof(((WP *)0)->expression)) {
		printf("Watchpoint expression is too long.\n");
		return 0;
	}

	value = expr(args, &success);
	if(!success) {
		return 0;
	}

	wp = new_wp();
	if(wp == NULL) {
		return 0;
	}
	strcpy(wp->expression, args);
	wp->value = value;
	printf("Watchpoint %d: %s = 0x%08x\n", wp->NO, wp->expression, wp->value);
	return 0;
}

static int cmd_d(char *args) {
	uint32_t no;
	char *end = NULL;

	if(!parse_u32(args, 10, &no, &end) || *skip_spaces(end) != '\0' || no > INT_MAX) {
		printf("Usage: d N\n");
		return 0;
	}

	if(!delete_watchpoint((int)no)) {
		printf("Watchpoint %u does not exist.\n", no);
	}
	return 0;
}

static int cmd_bt(char *args) {
	if(args != NULL && *skip_spaces(args) != '\0') {
		printf("Usage: bt\n");
		return 0;
	}
	print_backtrace();
	return 0;
}

static int cmd_help(char *args);

static struct {
	char *name;
	char *description;
	int (*handler) (char *);
} cmd_table [] = {
	{ "help", "Display information about all supported commands", cmd_help },
	{ "c", "Continue the execution of the program", cmd_c },
	{ "q", "Exit NEMU", cmd_q },
	{ "si", "Execute N instructions (default: 1)", cmd_si },
	{ "info", "Display registers (r) or watchpoints (w)", cmd_info },
	{ "x", "Examine N four-byte words starting at EXPR", cmd_x },
	{ "p", "Evaluate expression EXPR", cmd_p },
	{ "w", "Set a watchpoint for expression EXPR", cmd_w },
	{ "d", "Delete watchpoint N", cmd_d },
	{ "bt", "Print the current stack backtrace", cmd_bt }
};

#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

static int cmd_help(char *args) {
	char *argument = NULL;
	int i;

	args = skip_spaces(args);
	if(args != NULL && *args != '\0') {
		argument = strtok(args, " \t");
	}

	if(argument == NULL) {
		for(i = 0; i < NR_CMD; i ++) {
			printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
		}
	}
	else {
		for(i = 0; i < NR_CMD; i ++) {
			if(strcmp(argument, cmd_table[i].name) == 0) {
				printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
				return 0;
			}
		}
		printf("Unknown command '%s'\n", argument);
	}
	return 0;
}

void ui_mainloop() {
	while(1) {
		char *str = rl_gets();
		char *str_end;
		char *cmd;
		char *args;
		int i;

		if(str == NULL) {
			return;
		}
		str_end = str + strlen(str);

		cmd = strtok(str, " \t");
		if(cmd == NULL) {
			continue;
		}

		args = cmd + strlen(cmd) + 1;
		if(args >= str_end) {
			args = NULL;
		}
		else {
			args = skip_spaces(args);
			if(*args == '\0') {
				args = NULL;
			}
		}

#ifdef HAS_DEVICE
		extern void sdl_clear_event_queue(void);
		sdl_clear_event_queue();
#endif

		for(i = 0; i < NR_CMD; i ++) {
			if(strcmp(cmd, cmd_table[i].name) == 0) {
				if(cmd_table[i].handler(args) < 0) {
					return;
				}
				break;
			}
		}

		if(i == NR_CMD) {
			printf("Unknown command '%s'\n", cmd);
		}
	}
}
