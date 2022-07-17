#include <vm_tlb.h>
#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <cpu.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <vm.h>

int tlb_get_rr_victim(void) 
{
	int victim;
	static unsigned int next_victim = 0; // this variable is only initialized once
	victim = next_victim;
	next_victim = (next_victim + 1) % NUM_TLB;
	return victim;
}


