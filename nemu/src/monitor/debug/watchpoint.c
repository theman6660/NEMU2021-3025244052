#include "monitor/watchpoint.h"
#include "monitor/expr.h"

#define NR_WP 32

static WP wp_pool[NR_WP];
static WP *head, *free_;

void init_wp_pool() {
	int i;
	for(i = 0; i < NR_WP; i ++) {
		wp_pool[i].NO = i;
		wp_pool[i].next = &wp_pool[i + 1];
	}
	wp_pool[NR_WP - 1].next = NULL;

	head = NULL;
	free_ = wp_pool;
}

WP *new_wp(void) {
	if(free_ == NULL) {
		printf("No free watchpoint is available.\n");
		return NULL;
	}

	WP *wp = free_;
	free_ = free_->next;
	wp->next = head;
	wp->expression[0] = '\0';
	wp->value = 0;
	head = wp;
	return wp;
}

void free_wp(WP *wp) {
	if(wp == NULL) {
		return;
	}

	WP **link = &head;
	while(*link != NULL && *link != wp) {
		link = &(*link)->next;
	}
	if(*link == NULL) {
		return;
	}

	*link = wp->next;
	wp->expression[0] = '\0';
	wp->value = 0;
	wp->next = free_;
	free_ = wp;
}

bool delete_watchpoint(int no) {
	WP *wp;
	for(wp = head; wp != NULL; wp = wp->next) {
		if(wp->NO == no) {
			free_wp(wp);
			return true;
		}
	}
	return false;
}

void print_watchpoints(void) {
	WP *wp;
	if(head == NULL) {
		printf("No watchpoints.\n");
		return;
	}

	printf("NO\tValue\t\tExpression\n");
	for(wp = head; wp != NULL; wp = wp->next) {
		printf("%d\t0x%08x\t%s\n", wp->NO, wp->value, wp->expression);
	}
}

bool check_watchpoints(swaddr_t eip) {
	bool triggered = false;
	WP *wp;

	for(wp = head; wp != NULL; wp = wp->next) {
		bool success = true;
		uint32_t new_value = expr(wp->expression, &success);
		if(!success) {
			printf("Unable to evaluate watchpoint %d: %s\n", wp->NO, wp->expression);
			continue;
		}
		if(new_value != wp->value) {
			printf("Hint watchpoint %d at address 0x%08x\n", wp->NO, eip);
			printf("Expression: %s\n", wp->expression);
			printf("Old value: 0x%08x\n", wp->value);
			printf("New value: 0x%08x\n", new_value);
			wp->value = new_value;
			triggered = true;
		}
	}

	return triggered;
}


