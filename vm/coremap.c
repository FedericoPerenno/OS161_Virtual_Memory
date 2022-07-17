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

#include <coremap.h>
#include <pt.h>

// Wrap ram_stealmem and free_mem in a spinlock.
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER; 
static struct spinlock freemem_lock = SPINLOCK_INITIALIZER;


// global variables 
static unsigned char *freeRamFrames = NULL;
static unsigned long *allocSize = NULL;
static int nRamFrames = 0;
static int allocTableActive = 0;


int isTableActive () {
	int active;
	
	spinlock_acquire(&freemem_lock);
	active = allocTableActive;
	spinlock_release(&freemem_lock);
	
	return active;
}

void coremap_bootstrap(void) {
	int i;
	int fullpages;
	paddr_t firstpaddr;
	
	nRamFrames = ((int)ram_getsize())/PAGE_SIZE;
	
	/* alloc freeRamFrame and allocSize */
	freeRamFrames = kmalloc(sizeof(unsigned char)*nRamFrames);
	allocSize = kmalloc(sizeof(unsigned long)*nRamFrames);
	
	if (freeRamFrames==NULL || allocSize==NULL)
	{
		/* reset to disable this vm management */
		freeRamFrames = NULL; 
		allocSize = NULL;
		return;
	}
	
	firstpaddr = ram_getfirstfree();
	
	fullpages = ((int)firstpaddr)/ PAGE_SIZE;
	
	for (i=0; i<nRamFrames; i++) 
	{
		if (i<fullpages)
		{
			freeRamFrames[i] = (unsigned char)0; 
			allocSize[i] = 1;
		}
		else
		{
			freeRamFrames[i] = (unsigned char)1; 
			allocSize[i] = 0;
		}
	}
	
	spinlock_acquire(&freemem_lock);
	allocTableActive = 1;
	spinlock_release(&freemem_lock);
}

// used by kernel to allocate free pages
paddr_t getppages(unsigned long npages)
{
	paddr_t addr;
	
	/* try freed pages first */
	addr = getfreeppages(npages);
	
	// no free pages, take whatever corresponds to the first available virtual address
	// need to change this to get pages according to page replacement ???
	if (addr == 0) {
		if (!isTableActive())
		{
			spinlock_acquire(&stealmem_lock);
			addr = ram_stealmem(npages);
			spinlock_release(&stealmem_lock);
		}
		else
		{
			// in case of a kmalloc being called when memory is fullpages
			// we free the first space of the "over-writable" memory and use
			// it to store the structure/variable addresses by kmalloc
			// we update the page table to flag that page as "not over-writable"
			int index_pt = getFullPages();
			addr = index_pt * PAGE_SIZE;
			// set NULL entry to pt to flag it as "not over-writable"
			pt_set_entry(-1, 0, index_pt);
		}
	}
	
	if (addr != 0 && isTableActive()) {
		spinlock_acquire(&freemem_lock);
		allocSize[addr/PAGE_SIZE] = npages;
		spinlock_release(&freemem_lock);
	}

	return addr;
}


// search for first free page and see if there are any available
paddr_t getfreeppages(unsigned long npages)
{
	paddr_t addr;
	long i, first, found;
	long np = (long)npages;
	
	if (!isTableActive()) return 0;
	
	spinlock_acquire(&freemem_lock);
	// Linear search of free interval
	for (i=0,first=found=-1; i<nRamFrames; i++) 
	{
		if (freeRamFrames[i]) 
		{
			if (i==0 || !freeRamFrames[i-1])
				first = i; /* set first free in an interval */
			if (i-first+1 >= np)
			{
				found = first;
				break;
			}
		}
	}
	
	if (found>=0) 
	{
		// update free frame list masking the frames that have been used
		for (i=found; i<found+np; i++) 
		{
			freeRamFrames[i] = (unsigned char)0;
		}
		allocSize[found] = np;
		addr = (paddr_t) found*PAGE_SIZE;
	}
	else 
	{
		addr = 0;
	}
	spinlock_release(&freemem_lock);
	
	return addr;
}

// update free frame list when freeing the memory
int freeppages(paddr_t addr)
{
	long i, np, first;
	
	if (!isTableActive()) return 0;
	
	first = addr/PAGE_SIZE;
	np = allocSize[first];
	KASSERT(allocSize!=NULL);
	KASSERT(nRamFrames>first);
	
	
	spinlock_acquire(&freemem_lock);
	// free frames list update
	for (i=first; i<first+np; i++) 
	{
		freeRamFrames[i] = (unsigned char)1;
	}
	spinlock_release(&freemem_lock);
	
	return 1;
}
