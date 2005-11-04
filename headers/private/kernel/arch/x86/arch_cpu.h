/*
 * Copyright 2002-2005, Axel Dörfler, axeld@pinc-software.de. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
 * Distributed under the terms of the NewOS License.
 */
#ifndef _KERNEL_ARCH_x86_CPU_H
#define _KERNEL_ARCH_x86_CPU_H


#include <SupportDefs.h>
#include <arch/x86/descriptors.h>


// MSR registers (possibly Intel specific)
#define IA32_MSR_APIC_BASE	0x1b

struct tss {
	uint16 prev_task;
	uint16 unused0;
	uint32 sp0;
	uint32 ss0;
	uint32 sp1;
	uint32 ss1;
	uint32 sp2;
	uint32 ss2;
	uint32 cr3;
	uint32 eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
	uint32 es, cs, ss, ds, fs, gs;
	uint32 ldt_seg_selector;
	uint16 unused1;
	uint16 io_map_base;
};

struct iframe {
	uint32 gs;
	uint32 fs;
	uint32 es;
	uint32 ds;
	uint32 edi;
	uint32 esi;
	uint32 ebp;
	uint32 esp;
	uint32 ebx;
	uint32 edx;
	uint32 ecx;
	uint32 eax;
	uint32 orig_eax;
	uint32 orig_edx;
	uint32 vector;
	uint32 error_code;
	uint32 eip;
	uint32 cs;
	uint32 flags;
	uint32 user_esp;
	uint32 user_ss;
};


#ifdef __cplusplus
extern "C" {
#endif

#define nop() __asm__ ("nop"::)

struct arch_thread;

void setup_system_time(uint32 cv_factor);
void i386_context_switch(struct arch_thread *old_state, struct arch_thread *new_state, addr_t new_pgdir);
void i386_enter_uspace(addr_t entry, void *args1, void *args2, addr_t ustack_top);
void i386_set_tss_and_kstack(addr_t kstack);
void i386_switch_stack_and_call(addr_t stack, void (*func)(void *), void *arg);
void i386_swap_pgdir(addr_t new_pgdir);
void i386_fsave(void *fpu_state);
void i386_fxsave(void *fpu_state);
void i386_frstor(const void *fpu_state);
void i386_fxrstor(const void *fpu_state);
void i386_fsave_swap(void *old_fpu_state, const void *new_fpu_state);
void i386_fxsave_swap(void *old_fpu_state, const void *new_fpu_state);
uint64 x86_read_msr(uint32 register);
void x86_write_msr(uint32 register, uint64 value);
void x86_set_task_gate(int32 n, int32 segment);
struct tss *x86_get_main_tss(void);

#define read_ebp(value) \
	__asm__("movl	%%ebp,%0" : "=r" (value))

#define read_cr3(value) \
	__asm__("movl	%%cr3,%0" : "=r" (value))

#define write_cr3(value) \
	__asm__("movl	%0,%%cr3" : : "r" (value))

#define read_dr3(value) \
	__asm__("movl	%%dr3,%0" : "=r" (value))

#define write_dr3(value) \
	__asm__("movl	%0,%%dr3" : : "r" (value))

#define invalidate_TLB(va) \
	__asm__("invlpg (%0)" : : "r" (va))

#define out8(value,port) \
__asm__ ("outb %%al,%%dx" : : "a" (value), "d" (port))

#define out16(value,port) \
__asm__ ("outw %%ax,%%dx" : : "a" (value), "d" (port))

#define out32(value,port) \
__asm__ ("outl %%eax,%%dx" : : "a" (value), "d" (port))

#define in8(port) ({ \
uint8 _v; \
__asm__ volatile ("inb %%dx,%%al" : "=a" (_v) : "d" (port)); \
_v; \
})

#define in16(port) ({ \
uint16 _v; \
__asm__ volatile ("inw %%dx,%%ax":"=a" (_v) : "d" (port)); \
_v; \
})

#define in32(port) ({ \
uint32 _v; \
__asm__ volatile ("inl %%dx,%%eax":"=a" (_v) : "d" (port)); \
_v; \
})

#define out8_p(value,port) \
__asm__ ("outb %%al,%%dx\n" \
		"\tjmp 1f\n" \
		"1:\tjmp 1f\n" \
		"1:" : : "a" (value), "d" (port))

#define in8_p(port) ({ \
uint8 _v; \
__asm__ volatile ("inb %%dx,%%al\n" \
	"\tjmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:" : "=a" (_v) : "d" (port)); \
_v; \
})

extern segment_descriptor *gGDT;


#ifdef __cplusplus
}	// extern "C" {
#endif


#endif	/* _KERNEL_ARCH_x86_CPU_H */
