#include "common.h"
#include "memory/cache.h"

#include <stdlib.h>

uint32_t dram_read(hwaddr_t, size_t);
void dram_write(hwaddr_t, size_t, uint32_t);

#define BLOCK_SIZE 64
#define BLOCK_MASK (BLOCK_SIZE - 1)

#define L1_SIZE (64 * 1024)
#define L1_WAYS 8
#define L1_SETS (L1_SIZE / (BLOCK_SIZE * L1_WAYS))
#define L1_INDEX_MASK (L1_SETS - 1)

#define L2_SIZE (4 * 1024 * 1024)
#define L2_WAYS 16
#define L2_SETS (L2_SIZE / (BLOCK_SIZE * L2_WAYS))
#define L2_INDEX_MASK (L2_SETS - 1)

typedef struct {
	bool valid;
	uint32_t tag;
	uint8_t data[BLOCK_SIZE];
} L1Line;

typedef struct {
	bool valid;
	bool dirty;
	uint32_t tag;
	uint8_t data[BLOCK_SIZE];
} L2Line;

static L1Line l1[L1_SETS][L1_WAYS];
static L2Line l2[L2_SETS][L2_WAYS];

static uint32_t block_addr(hwaddr_t addr) {
	return addr & ~BLOCK_MASK;
}

static void dram_load_block(hwaddr_t addr, uint8_t *dst) {
	int i;
	addr = block_addr(addr);
	for(i = 0; i < BLOCK_SIZE; i += 4) {
		uint32_t value = dram_read(addr + i, 4);
		memcpy(dst + i, &value, 4);
	}
}

static void dram_store_block(hwaddr_t addr, const uint8_t *src) {
	int i;
	addr = block_addr(addr);
	for(i = 0; i < BLOCK_SIZE; i += 4) {
		uint32_t value = 0;
		memcpy(&value, src + i, 4);
		dram_write(addr + i, 4, value);
	}
}

static int l2_choose(uint32_t set) {
	int i;
	for(i = 0; i < L2_WAYS; i ++) {
		if(!l2[set][i].valid) {
			return i;
		}
	}
	return rand() % L2_WAYS;
}

static L2Line *l2_find(hwaddr_t addr, bool allocate) {
	uint32_t set = (addr / BLOCK_SIZE) & L2_INDEX_MASK;
	uint32_t tag = addr / (BLOCK_SIZE * L2_SETS);
	int i, victim;

	for(i = 0; i < L2_WAYS; i ++) {
		if(l2[set][i].valid && l2[set][i].tag == tag) {
			return &l2[set][i];
		}
	}

	if(!allocate) {
		return NULL;
	}

	victim = l2_choose(set);
	if(l2[set][victim].valid && l2[set][victim].dirty) {
		hwaddr_t writeback = (l2[set][victim].tag * L2_SETS + set) * BLOCK_SIZE;
		dram_store_block(writeback, l2[set][victim].data);
	}

	dram_load_block(addr, l2[set][victim].data);
	l2[set][victim].valid = true;
	l2[set][victim].dirty = false;
	l2[set][victim].tag = tag;
	return &l2[set][victim];
}

static int l1_choose(uint32_t set) {
	int i;
	for(i = 0; i < L1_WAYS; i ++) {
		if(!l1[set][i].valid) {
			return i;
		}
	}
	return rand() % L1_WAYS;
}

static L1Line *l1_find(hwaddr_t addr, bool allocate) {
	uint32_t set = (addr / BLOCK_SIZE) & L1_INDEX_MASK;
	uint32_t tag = addr / (BLOCK_SIZE * L1_SETS);
	int i, victim;
	L2Line *l2_line;

	for(i = 0; i < L1_WAYS; i ++) {
		if(l1[set][i].valid && l1[set][i].tag == tag) {
			return &l1[set][i];
		}
	}

	if(!allocate) {
		return NULL;
	}

	victim = l1_choose(set);
	l2_line = l2_find(addr, true);
	memcpy(l1[set][victim].data, l2_line->data, BLOCK_SIZE);
	l1[set][victim].valid = true;
	l1[set][victim].tag = tag;
	return &l1[set][victim];
}

static uint32_t load_bytes(const uint8_t *data, size_t len) {
	uint32_t value = 0;
	memcpy(&value, data, len);
	return value;
}

static void store_bytes(uint8_t *data, size_t len, uint32_t value) {
	memcpy(data, &value, len);
}

void init_cache(void) {
	memset(l1, 0, sizeof(l1));
	memset(l2, 0, sizeof(l2));
	srand(1);
}

uint32_t cache_read(hwaddr_t addr, size_t len) {
	L1Line *line = l1_find(addr, true);
	return load_bytes(line->data + (addr & BLOCK_MASK), len);
}

void cache_write(hwaddr_t addr, size_t len, uint32_t data) {
	uint32_t offset = addr & BLOCK_MASK;
	L1Line *l1_line = l1_find(addr, false);
	L2Line *l2_line;

	/* L1: write through, no write allocate. */
	if(l1_line != NULL) {
		store_bytes(l1_line->data + offset, len, data);
	}

	/* L2: write back, write allocate. */
	l2_line = l2_find(addr, true);
	store_bytes(l2_line->data + offset, len, data);
	l2_line->dirty = true;
}
