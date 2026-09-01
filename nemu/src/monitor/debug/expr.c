#include "nemu.h"

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <sys/types.h>
#include <regex.h>
#include <errno.h>
#include <stdlib.h>
#include <strings.h>

enum {
	NOTYPE = 256,
	TK_DEC,
	TK_HEX,
	TK_REG,
	TK_EQ,
	TK_NEQ,
	TK_AND,
	TK_DEREF,
	TK_NEG
};

static struct rule {
	char *regex;
	int token_type;
} rules[] = {
	{"[ \t]+", NOTYPE},
	{"0[xX][0-9a-fA-F]+", TK_HEX},
	{"[0-9]+", TK_DEC},
	{"\\$[a-zA-Z][a-zA-Z0-9]*", TK_REG},
	{"==", TK_EQ},
	{"!=", TK_NEQ},
	{"&&", TK_AND},
	{"\\+", '+'},
	{"-", '-'},
	{"\\*", '*'},
	{"/", '/'},
	{"\\(", '('},
	{"\\)", ')'}
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]))

static regex_t re[NR_REGEX];

void init_regex() {
	int i;
	char error_msg[128];
	int ret;

	for(i = 0; i < NR_REGEX; i ++) {
		ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
		if(ret != 0) {
			regerror(ret, &re[i], error_msg, 128);
			Assert(ret == 0, "regex compilation failed: %s\n%s", error_msg, rules[i].regex);
		}
	}
}

typedef struct token {
	int type;
	char str[32];
} Token;

#define NR_TOKEN 128

static Token tokens[NR_TOKEN];
static int nr_token;

static bool is_value_token(int type) {
	return type == TK_DEC || type == TK_HEX || type == TK_REG || type == ')';
}

static bool make_token(char *e) {
	int position = 0;
	int i;
	regmatch_t pmatch;

	nr_token = 0;

	while(e[position] != '\0') {
		for(i = 0; i < NR_REGEX; i ++) {
			if(regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
				char *substr_start = e + position;
				int substr_len = pmatch.rm_eo;

				Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
						i, rules[i].regex, position, substr_len, substr_len, substr_start);
				position += substr_len;

				if(rules[i].token_type != NOTYPE) {
					if(nr_token >= NR_TOKEN) {
						printf("Expression contains too many tokens (maximum %d).\n", NR_TOKEN);
						return false;
					}

					tokens[nr_token].type = rules[i].token_type;
					tokens[nr_token].str[0] = '\0';

					if(rules[i].token_type == TK_DEC || rules[i].token_type == TK_HEX ||
							rules[i].token_type == TK_REG) {
						if(substr_len >= (int)sizeof(tokens[nr_token].str)) {
							printf("Token is too long at position %d.\n", position - substr_len);
							return false;
						}
						memcpy(tokens[nr_token].str, substr_start, substr_len);
						tokens[nr_token].str[substr_len] = '\0';
					}

					nr_token ++;
				}

				break;
			}
		}

		if(i == NR_REGEX) {
			printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
			return false;
		}
	}

	for(i = 0; i < nr_token; i ++) {
		if(tokens[i].type == '*' && (i == 0 || !is_value_token(tokens[i - 1].type))) {
			tokens[i].type = TK_DEREF;
		}
		else if(tokens[i].type == '-' && (i == 0 || !is_value_token(tokens[i - 1].type))) {
			tokens[i].type = TK_NEG;
		}
	}

	return true;
}

static bool register_value(const char *name, uint32_t *value) {
	const char *reg_name = name + 1;
	int i;

	for(i = 0; i < 8; i ++) {
		if(strcasecmp(reg_name, regsl[i]) == 0) {
			*value = reg_l(i);
			return true;
		}
		if(strcasecmp(reg_name, regsw[i]) == 0) {
			*value = reg_w(i);
			return true;
		}
		if(strcasecmp(reg_name, regsb[i]) == 0) {
			*value = reg_b(i);
			return true;
		}
	}

	if(strcasecmp(reg_name, "eip") == 0) {
		*value = cpu.eip;
		return true;
	}
	if(strcasecmp(reg_name, "eflags") == 0) {
		*value = cpu.eflags.val;
		return true;
	}

	printf("Unknown register '%s'.\n", name);
	return false;
}

static bool is_wrapped_by_parentheses(int p, int q, bool *success) {
	int depth = 0;
	int i;
	bool wrapped = tokens[p].type == '(' && tokens[q].type == ')';

	for(i = p; i <= q; i ++) {
		if(tokens[i].type == '(') {
			depth ++;
		}
		else if(tokens[i].type == ')') {
			depth --;
			if(depth < 0) {
				*success = false;
				return false;
			}
		}

		if(depth == 0 && i < q) {
			wrapped = false;
		}
	}

	if(depth != 0) {
		*success = false;
		return false;
	}

	return wrapped;
}

static int precedence(int type) {
	switch(type) {
		case TK_AND: return 1;
		case TK_EQ:
		case TK_NEQ: return 2;
		case '+':
		case '-': return 3;
		case '*':
		case '/': return 4;
		default: return 0;
	}
}

static int dominant_operator(int p, int q, bool *success) {
	int depth = 0;
	int selected = -1;
	int selected_precedence = 0x7fffffff;
	int i;

	for(i = p; i <= q; i ++) {
		if(tokens[i].type == '(') {
			depth ++;
			continue;
		}
		if(tokens[i].type == ')') {
			depth --;
			if(depth < 0) {
				*success = false;
				return -1;
			}
			continue;
		}
		if(depth == 0) {
			int current_precedence = precedence(tokens[i].type);
			if(current_precedence > 0 && current_precedence <= selected_precedence) {
				selected = i;
				selected_precedence = current_precedence;
			}
		}
	}

	if(depth != 0) {
		*success = false;
		return -1;
	}

	return selected;
}

static uint32_t eval(int p, int q, bool *success) {
	if(p > q) {
		*success = false;
		return 0;
	}

	if(p == q) {
		if(tokens[p].type == TK_DEC || tokens[p].type == TK_HEX) {
			char *end = NULL;
			unsigned long value;

			errno = 0;
			value = strtoul(tokens[p].str, &end, 0);
			if(errno == ERANGE || end == tokens[p].str || *end != '\0' || value > UINT32_MAX) {
				*success = false;
				return 0;
			}
			return (uint32_t)value;
		}
		if(tokens[p].type == TK_REG) {
			uint32_t value = 0;
			if(!register_value(tokens[p].str, &value)) {
				*success = false;
			}
			return value;
		}

		*success = false;
		return 0;
	}

	if(is_wrapped_by_parentheses(p, q, success)) {
		return eval(p + 1, q - 1, success);
	}
	if(!*success) {
		return 0;
	}

	int op = dominant_operator(p, q, success);
	if(!*success) {
		return 0;
	}

	if(op >= 0) {
		uint32_t left = eval(p, op - 1, success);
		if(!*success) {
			return 0;
		}

		if(tokens[op].type == TK_AND && left == 0) {
			return 0;
		}

		uint32_t right = eval(op + 1, q, success);
		if(!*success) {
			return 0;
		}

		switch(tokens[op].type) {
			case '+': return left + right;
			case '-': return left - right;
			case '*': return left * right;
			case '/':
				if(right == 0) {
					printf("Division by zero.\n");
					*success = false;
					return 0;
				}
				return left / right;
			case TK_EQ: return left == right;
			case TK_NEQ: return left != right;
			case TK_AND: return left && right;
			default:
				*success = false;
				return 0;
		}
	}

	if(tokens[p].type == TK_NEG || tokens[p].type == TK_DEREF) {
		uint32_t value = eval(p + 1, q, success);
		if(!*success) {
			return 0;
		}
		if(tokens[p].type == TK_NEG) {
			return 0u - value;
		}
		return swaddr_read(value, 4);
	}

	*success = false;
	return 0;
}

uint32_t expr(char *e, bool *success) {
	*success = true;
	if(e == NULL || !make_token(e) || nr_token == 0) {
		*success = false;
		return 0;
	}

	uint32_t value = eval(0, nr_token - 1, success);
	if(!*success) {
		printf("Invalid expression: %s\n", e);
		return 0;
	}
	return value;
}
