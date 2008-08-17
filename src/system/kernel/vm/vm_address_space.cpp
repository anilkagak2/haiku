/*
 * Copyright 2002-2008, Axel Dörfler, axeld@pinc-software.de. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
 * Distributed under the terms of the NewOS License.
 */


#include <KernelExport.h>

#include <vm.h>
#include <vm_address_space.h>
#include <vm_priv.h>
#include <heap.h>
#include <thread.h>
#include <util/khash.h>

#include <stdlib.h>


//#define TRACE_VM
#ifdef TRACE_VM
#	define TRACE(x) dprintf x
#else
#	define TRACE(x) ;
#endif


static vm_address_space *sKernelAddressSpace;

#define ASPACE_HASH_TABLE_SIZE 1024
static struct hash_table *sAddressSpaceTable;
static rw_lock sAddressSpaceTableLock;


static void
_dump_aspace(vm_address_space *aspace)
{
	vm_area *area;

	kprintf("dump of address space at %p:\n", aspace);
	kprintf("id: 0x%lx\n", aspace->id);
	kprintf("ref_count: %ld\n", aspace->ref_count);
	kprintf("fault_count: %ld\n", aspace->fault_count);
	kprintf("translation_map: %p\n", &aspace->translation_map);
	kprintf("base: 0x%lx\n", aspace->base);
	kprintf("size: 0x%lx\n", aspace->size);
	kprintf("change_count: 0x%lx\n", aspace->change_count);
	kprintf("area_hint: %p\n", aspace->area_hint);
	kprintf("area_list:\n");
	for (area = aspace->areas; area != NULL; area = area->address_space_next) {
		kprintf(" area 0x%lx: ", area->id);
		kprintf("base_addr = 0x%lx ", area->base);
		kprintf("size = 0x%lx ", area->size);
		kprintf("name = '%s' ", area->name);
		kprintf("protection = 0x%lx\n", area->protection);
	}
}


static int
dump_aspace(int argc, char **argv)
{
	vm_address_space *aspace;

	if (argc < 2) {
		kprintf("aspace: not enough arguments\n");
		return 0;
	}

	// if the argument looks like a number, treat it as such

	{
		team_id id = strtoul(argv[1], NULL, 0);

		aspace = (vm_address_space *)hash_lookup(sAddressSpaceTable, &id);
		if (aspace == NULL) {
			kprintf("invalid aspace id\n");
		} else {
			_dump_aspace(aspace);
		}
		return 0;
	}
	return 0;
}


static int
dump_aspace_list(int argc, char **argv)
{
	vm_address_space *space;
	struct hash_iterator iter;

	kprintf("   address      id         base         size   area count   "
		" area size\n");

	hash_open(sAddressSpaceTable, &iter);
	while ((space = (vm_address_space *)hash_next(sAddressSpaceTable,
			&iter)) != NULL) {
		int32 areaCount = 0;
		off_t areaSize = 0;
		for (vm_area* area = space->areas; area != NULL;
				area = area->address_space_next) {
			if (area->id != RESERVED_AREA_ID
				&& area->cache->type != CACHE_TYPE_NULL) {
				areaCount++;
				areaSize += area->size;
			}
		}
		kprintf("%p  %6ld   %#010lx   %#10lx   %10ld   %10lld\n",
			space, space->id, space->base, space->size, areaCount, areaSize);
	}
	hash_close(sAddressSpaceTable, &iter, false);
	return 0;
}


static int
aspace_compare(void *_a, const void *key)
{
	vm_address_space *aspace = (vm_address_space *)_a;
	const team_id *id = (const team_id *)key;

	if (aspace->id == *id)
		return 0;

	return -1;
}


static uint32
aspace_hash(void *_a, const void *key, uint32 range)
{
	vm_address_space *aspace = (vm_address_space *)_a;
	const team_id *id = (const team_id *)key;

	if (aspace != NULL)
		return aspace->id % range;

	return *id % range;
}


/*! When this function is called, all references to this address space
	have been released, so it's safe to remove it.
*/
static void
delete_address_space(vm_address_space *addressSpace)
{
	TRACE(("delete_address_space: called on aspace 0x%lx\n", addressSpace->id));

	if (addressSpace == sKernelAddressSpace)
		panic("tried to delete the kernel aspace!\n");

	rw_lock_write_lock(&addressSpace->lock);

	addressSpace->translation_map.ops->destroy(&addressSpace->translation_map);

	rw_lock_destroy(&addressSpace->lock);
	free(addressSpace);
}


//	#pragma mark -


vm_address_space *
vm_get_address_space(team_id id)
{
	vm_address_space *addressSpace;

	rw_lock_read_lock(&sAddressSpaceTableLock);
	addressSpace = (vm_address_space *)hash_lookup(sAddressSpaceTable, &id);
	if (addressSpace)
		atomic_add(&addressSpace->ref_count, 1);
	rw_lock_read_unlock(&sAddressSpaceTableLock);

	return addressSpace;
}


vm_address_space *
vm_get_kernel_address_space(void)
{
	/* we can treat this one a little differently since it can't be deleted */
	atomic_add(&sKernelAddressSpace->ref_count, 1);
	return sKernelAddressSpace;
}


vm_address_space *
vm_kernel_address_space(void)
{
	return sKernelAddressSpace;
}


team_id
vm_kernel_address_space_id(void)
{
	return sKernelAddressSpace->id;
}


vm_address_space *
vm_get_current_user_address_space(void)
{
	struct thread *thread = thread_get_current_thread();

	if (thread != NULL) {
		vm_address_space *addressSpace = thread->team->address_space;
		if (addressSpace != NULL) {
			atomic_add(&addressSpace->ref_count, 1);
			return addressSpace;
		}
	}

	return NULL;
}


team_id
vm_current_user_address_space_id(void)
{
	struct thread *thread = thread_get_current_thread();

	if (thread != NULL && thread->team->address_space != NULL)
		return thread->team->id;

	return B_ERROR;
}


void
vm_put_address_space(vm_address_space *addressSpace)
{
	bool remove = false;

	rw_lock_write_lock(&sAddressSpaceTableLock);
	if (atomic_add(&addressSpace->ref_count, -1) == 1) {
		hash_remove(sAddressSpaceTable, addressSpace);
		remove = true;
	}
	rw_lock_write_unlock(&sAddressSpaceTableLock);

	if (remove)
		delete_address_space(addressSpace);
}


/*! Deletes all areas in the specified address space, and the address
	space by decreasing all reference counters. It also marks the
	address space of being in deletion state, so that no more areas
	can be created in it.
	After this, the address space is not operational anymore, but might
	still be in memory until the last reference has been released.
*/
void
vm_delete_address_space(vm_address_space *addressSpace)
{
	rw_lock_write_lock(&addressSpace->lock);
	addressSpace->state = VM_ASPACE_STATE_DELETION;
	rw_lock_write_unlock(&addressSpace->lock);

	vm_delete_areas(addressSpace);
	vm_put_address_space(addressSpace);
}


status_t
vm_create_address_space(team_id id, addr_t base, addr_t size,
	bool kernel, vm_address_space **_addressSpace)
{
	vm_address_space *addressSpace;
	status_t status;

	addressSpace = (vm_address_space *)malloc_nogrow(sizeof(vm_address_space));
	if (addressSpace == NULL)
		return B_NO_MEMORY;

	TRACE(("vm_create_aspace: team %ld (%skernel):"
			" %lx bytes starting at 0x%lx => %p\n",
			id, kernel ? "!" : "", size, base, addressSpace));

	addressSpace->base = base;
	addressSpace->size = size;
	addressSpace->areas = NULL;
	addressSpace->area_hint = NULL;
	addressSpace->change_count = 0;
	rw_lock_init(&addressSpace->lock,
		kernel ? "kernel address space" : "address space");

	addressSpace->id = id;
	addressSpace->ref_count = 1;
	addressSpace->state = VM_ASPACE_STATE_NORMAL;
	addressSpace->fault_count = 0;

	// initialize the corresponding translation map
	status = arch_vm_translation_map_init_map(&addressSpace->translation_map,
		kernel);
	if (status < B_OK) {
		free(addressSpace);
		return status;
	}

	// add the aspace to the global hash table
	rw_lock_write_lock(&sAddressSpaceTableLock);
	hash_insert(sAddressSpaceTable, addressSpace);
	rw_lock_write_unlock(&sAddressSpaceTableLock);

	*_addressSpace = addressSpace;
	return B_OK;
}


status_t
vm_address_space_init(void)
{
	rw_lock_init(&sAddressSpaceTableLock, "address spaces table");

	// create the area and address space hash tables
	{
		vm_address_space *aspace;
		sAddressSpaceTable = hash_init(ASPACE_HASH_TABLE_SIZE,
			(addr_t)&aspace->hash_next - (addr_t)aspace, &aspace_compare,
			&aspace_hash);
		if (sAddressSpaceTable == NULL)
			panic("vm_init: error creating aspace hash table\n");
	}

	// create the initial kernel address space
	if (vm_create_address_space(1, KERNEL_BASE, KERNEL_SIZE,
			true, &sKernelAddressSpace) != B_OK)
		panic("vm_init: error creating kernel address space!\n");

	add_debugger_command("aspaces", &dump_aspace_list,
		"Dump a list of all address spaces");
	add_debugger_command("aspace", &dump_aspace,
		"Dump info about a particular address space");

	return B_OK;
}


status_t
vm_address_space_init_post_sem(void)
{
	status_t status = arch_vm_translation_map_init_kernel_map_post_sem(
		&sKernelAddressSpace->translation_map);
	if (status < B_OK)
		return status;

	return B_OK;
}
