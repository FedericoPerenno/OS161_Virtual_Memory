#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <vm.h>
#include <proc.h>
#include <spl.h>
#include <cpu.h>
#include <spinlock.h>
#include <current.h>
#include <mips/tlb.h>

#include <pt.h>
#include <swapfile.h>
#include <vmstats.h>
#include <coremap.h>


static struct ipt_t *myIpt;
static int fullPages = 0;


int pt_create(void) 
{
	int i;
	int pt_size = ((int)ram_getsize())/PAGE_SIZE;
	
	// alloc page table
	myIpt = kmalloc(sizeof(struct ipt_t));
	
	if (myIpt==NULL)
	{
		kprintf ("Page table f-ed up\n");
		return 1;
	}
	
	
	myIpt->entry = kmalloc(pt_size*sizeof(struct ipt_t));
	
	if (myIpt->entry == NULL)
	{
		return 1;
	}
	
	myIpt->size = pt_size;
	
	// Initialize page table
	for (i=0; i < myIpt->size; i++) 
	{
		myIpt->entry[i].pid = -1;
		myIpt->entry[i].vaddr = 0;
	}
	
	return 0;
}

// returns 1 if it finds the page in the page table along with the physical address
int page_is_in_mem (pid_t pid, vaddr_t vaddr, int *index)
{
	int i;
	
	for (i = 0; i < myIpt->size; i++)
	{
		if ((pid == myIpt->entry[i].pid) && (vaddr == myIpt->entry[i].vaddr))
		{
			// in ipt each entry corresponds to one physical page
			*index =  i;
			return 1;
		}
	}	

	return 0;
}

// substitutes in order from 0 to myIpt->size
// since entries are also added in order, this corresponds to FIFO
int pt_get_FIFO_victim (void) 
{
	int victim;
	static unsigned int next_victim = 0; // this variable is only initialized once
	
	if (fullPages == 0)
		fullPages = getFullPages();
	
	victim = next_victim + fullPages;
	next_victim = (next_victim + 1) % (myIpt->size - fullPages);
	return victim;
}

// search first invalid entry (pid == -1) 
// if none call pt_get_FIFO_victim
int pt_get_victim (void)
{
	int index_pt;
	paddr_t paddr;
	
	// Search for first free page
	paddr = getfreeppages(1);
	
	if (paddr!=0)
	{
		index_pt = paddr / PAGE_SIZE;
	}
	else // Page Replacement
	{
		index_pt = pt_get_FIFO_victim();
		pid_t old_pid = myIpt->entry[index_pt].pid;
		vaddr_t old_vaddr = myIpt->entry[index_pt].vaddr;
		uint32_t old_elo, old_ehi;
		int i;
		
		increment_SWAPFILE_writes();
		// Save old page in swapfile
		if (write_to_swapfile (old_pid, old_vaddr, index_pt))
			panic ("Can't write to swapfile");
		
		// Invalidate old entry in the TLB if still there
		for (i=0; i<NUM_TLB; i++) {
			tlb_read(&old_ehi, &old_elo, i);
			if (old_ehi == old_vaddr){
				tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
				break;
			}
		}
	}
	
	return index_pt;
}


void pt_set_entry (pid_t pid, vaddr_t vaddr, int index)
{
	myIpt->entry[index].pid = pid;
	myIpt->entry[index].vaddr = vaddr;
}

vaddr_t pt_get_vaddr (int index)
{
	return myIpt->entry[index].vaddr;
}

pid_t pt_get_pid (int index)
{
	return myIpt->entry[index].pid;
}

int getFullPages(void)
{
	int i;
	
	for (i = 0; i < myIpt->size; i++)
	{
		if (myIpt->entry[i].pid >= 0 && myIpt->entry[i-1].pid == -1)
			return i;
	}
	
	return myIpt->size;
	
}
