/* Intel PRO/1000 Family Driver
 * Copyright (C) 2004 Marcus Overhagen <marcus@overhagen.de>. All rights reserved.
 */
#include <Errors.h>
#include <OS.h>
#include <string.h>

//#define DEBUG

#include "debug.h"
#include "util.h"

static inline uint32
round_to_pagesize(uint32 size)
{
	return (size + B_PAGE_SIZE - 1) & ~(B_PAGE_SIZE - 1);
}

area_id
map_mem(void **virt, void *phy, size_t size, uint32 protection, const char *name)
{
	uint32 offset;
	void *phyadr;
	void *mapadr;
	area_id area;

	TRACE("mapping physical address %p with %ld bytes for %s\n", phy, size, name);

	offset = (uint32)phy & (B_PAGE_SIZE - 1);
	phyadr = (char *)phy - offset;
	size = round_to_pagesize(size + offset);
	area = map_physical_memory(name, phyadr, size, B_ANY_KERNEL_BLOCK_ADDRESS, protection, &mapadr);
	if (area < B_OK) {
		ERROR("mapping '%s' failed, error 0x%lx (%s)\n", name, area, strerror(area));
		return area;
	}
	
	*virt = (char *)mapadr + offset;

	TRACE("physical = %p, virtual = %p, offset = %ld, phyadr = %p, mapadr = %p, size = %ld, area = 0x%08lx\n",
		phy, *virt, offset, phyadr, mapadr, size, area);
	
	return area;
}
