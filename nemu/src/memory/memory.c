#include "common.h"
#include "memory/cache.h"

#define CACHE_BLOCK 64

/* Memory accessing interfaces */

uint32_t hwaddr_read(hwaddr_t addr, size_t len) {
	uint32_t offset = addr & (CACHE_BLOCK - 1);
	uint32_t value;

	if(offset + len > CACHE_BLOCK) {
		size_t first = CACHE_BLOCK - offset;
		uint32_t low = cache_read(addr, first);
		uint32_t high = cache_read(addr + first, len - first);
		value = low | (high << (first << 3));
	}
	else {
		value = cache_read(addr, len);
	}

	return value & (~0u >> ((4 - len) << 3));
}

void hwaddr_write(hwaddr_t addr, size_t len, uint32_t data) {
	uint32_t offset = addr & (CACHE_BLOCK - 1);

	if(offset + len > CACHE_BLOCK) {
		size_t first = CACHE_BLOCK - offset;
		cache_write(addr, first, data);
		cache_write(addr + first, len - first, data >> (first << 3));
		return;
	}

	cache_write(addr, len, data);
}

uint32_t lnaddr_read(lnaddr_t addr, size_t len) {
	return hwaddr_read(addr, len);
}

void lnaddr_write(lnaddr_t addr, size_t len, uint32_t data) {
	hwaddr_write(addr, len, data);
}

uint32_t swaddr_read(swaddr_t addr, size_t len) {
#ifdef DEBUG
	assert(len == 1 || len == 2 || len == 4);
#endif
	return lnaddr_read(addr, len);
}

void swaddr_write(swaddr_t addr, size_t len, uint32_t data) {
#ifdef DEBUG
	assert(len == 1 || len == 2 || len == 4);
#endif
	lnaddr_write(addr, len, data);
}

