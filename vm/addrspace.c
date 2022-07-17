/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

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
#include <uio.h>
#include <vfs.h>
#include <vnode.h>

#include <addrspace.h>
#include <coremap.h>
#include <vmstats.h>
#include <pt.h>
#include <swapfile.h>
#include <vm_tlb.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */


 void vm_bootstrap(void)
{	
	if (pt_create())
		panic ("Can't create Page Table");
	
	if (swapfile_create())
		panic ("Can't create Swapfile");
		
	coremap_bootstrap();

}

// should be callled from interprocessor interrupt
void vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("vm tried to do tlb shootdown?!\n");
}

void as_zero_region(paddr_t paddr, unsigned npages)
{
	bzero((void *)PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
}

// Loads the needed page on demand from ELF file
int load_page_on_demand(struct vnode* v, paddr_t paddr, size_t memsize, size_t filesize, off_t offset)
{
	int result;
	struct iovec iov;
	struct uio u;
	
	if (filesize > memsize) {
		kprintf("ELF: warning: segment filesize > segment memsize\n");
		filesize = memsize;
	}
	
	else if (filesize < memsize) {
		// load the page with zeros so that not-loaded bits are initialized to zero
		as_zero_region((paddr & PAGE_FRAME), 1);
	}
	
	iov.iov_ubase = (void*) PADDR_TO_KVADDR(paddr);
	iov.iov_len = memsize;
	u.uio_iov = &iov;
	u.uio_iovcnt = 1;
	u.uio_offset = offset;
	u.uio_resid = filesize;
	u.uio_segflg = UIO_SYSSPACE;
	u.uio_rw = UIO_READ;
	u.uio_space = NULL;
	
	result = VOP_READ(v, &u);
	if (result){
		return result;
	}

	if (u.uio_resid!=0){
		return ENOEXEC;
	}

	return result;
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	vaddr_t text_vbase, text_vtop, data_vbase, data_vtop, stacktop; // virtual addresses
	paddr_t paddr; // physical address
	int i;
	uint32_t ehi, elo, old_elo, old_ehi; // tlb entry - high and low
	struct addrspace *as;
	int spl; // used to disable interrupts when accessing the tlb

	faultaddress &= PAGE_FRAME;
	//DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
	    case VM_FAULT_READONLY:
		// In our case text is read-only, so we might get this
		increment_TLB_faults();
		kprintf ("Attempted to read a read-only segment\n");
		as = proc_getas();
		as_destroy(as); // free space used for address space
		thread_exit(); // exit current thread without crashing
		break;
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
	    kprintf ("Can't handle this fault type\n");
		return EINVAL;
	}

	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		kprintf ("Can't find the process\n");
		return EFAULT;
	}

	as = proc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		kprintf ("Can't find address space\n");
		return EFAULT;
	}
	
	/* Assert that the address space has been set up properly. */
	KASSERT(as->text_vbase != 0);
	KASSERT(as->text_npages != 0);
	KASSERT(as->data_vbase != 0);
	KASSERT(as->data_npages != 0);	

	text_vbase = as->text_vbase;
	text_vtop = (text_vbase & PAGE_FRAME) + as->text_npages * PAGE_SIZE; // compute top of text
	data_vbase = as->data_vbase;
	data_vtop = (data_vbase & PAGE_FRAME) + as->data_npages * PAGE_SIZE; // compute top of data
	
	stacktop = USERSTACK;
	
	int index_sf;
	int index_pt;
	pid_t pid = curproc->pid;
	
	
	int IS_TEXT = 0;
	int IS_DATA = 0;
	int IS_STACK = 0;
	
	if (faultaddress >= (text_vbase & PAGE_FRAME) && faultaddress < text_vtop)
		IS_TEXT = 1;
	else if (faultaddress >= (data_vbase & PAGE_FRAME) && faultaddress < data_vtop)
		IS_DATA = 1;
	else if (faultaddress >= data_vtop && faultaddress < stacktop)
		IS_STACK = 1;
	else
		return EFAULT;
	
	increment_TLB_faults();
	
	// page hit
	if(page_is_in_mem(pid, faultaddress, &index_pt))
	{
		increment_TLB_reloads();
		paddr = index_pt * PAGE_SIZE;
	}
	else{
		// find paddr (page replacement if needed)
		index_pt = pt_get_victim(); 
		paddr = (paddr_t) index_pt * PAGE_SIZE; // the ipt covers all pages
		
		// set new entry to pt
		pt_set_entry(pid, faultaddress, index_pt);
		
		if (page_is_in_swapfile(pid, faultaddress, &index_sf))
		{
			increment_PAGE_faults_disk();
			increment_PAGE_faults_swapfile();
			// write from swapfile to memory 
			if (read_from_swapfile(index_sf, index_pt)) 
				panic("Can't read from swapfile\n");
		}
	
		// needs to be loaded from disk exploiting info in ELF file
		else
		{
			// Text segment
			if (IS_TEXT) 
			{		
				size_t amount_to_read;
					

				off_t offset_elf  = as->text_offset + faultaddress - (text_vbase & PAGE_FRAME);
				paddr_t paddr_tmp = paddr;
				vaddr_t text_vbase_off = text_vbase & ~PAGE_FRAME;
				
				// First page of text segment to read from elf
				if (faultaddress == (text_vbase & PAGE_FRAME))
				{
					paddr_tmp += text_vbase_off;
					if (as->text_size >= PAGE_SIZE - text_vbase_off)
						amount_to_read = PAGE_SIZE - text_vbase_off;
					else
						amount_to_read = as->text_size;
					
				}
				// Last page of text segment to read from elf
				else if (faultaddress == ((text_vbase + as->text_size) & PAGE_FRAME))
				{
					amount_to_read = (as->text_size - (PAGE_SIZE - text_vbase_off)) &~ PAGE_FRAME;
					offset_elf -= text_vbase_off;
				}
				// Middle page of text segment to read from elf
				else 
				{
					amount_to_read = PAGE_SIZE;
					offset_elf -= text_vbase_off;
				}
				
				if (load_page_on_demand(as->v, paddr_tmp, PAGE_SIZE, amount_to_read, offset_elf))
					panic ("can't load page on demand\n");
												
				increment_PAGE_faults_disk();
				increment_PAGE_faults_elf();
				
				
				
			}
			
			// Data segment
			else if (IS_DATA) 
			{		
				size_t amount_to_read;
				paddr_t paddr_tmp = paddr;
				
				// Uninitialized global data
				if (faultaddress > ((data_vbase + as->data_size) & PAGE_FRAME))
				{
					as_zero_region(paddr, 1);
					increment_PAGE_faults_zeroed();
				}
				// It's at least partially initialized data
				else
				{
					off_t offset_elf = as->data_offset + faultaddress - (data_vbase & PAGE_FRAME);
					vaddr_t data_vbase_off = data_vbase & ~PAGE_FRAME;
					
					// First page of data segment to read from elf 
					if (faultaddress == (as->data_vbase & PAGE_FRAME))
					{
						paddr_tmp += data_vbase_off;
						if (as->data_size >= PAGE_SIZE - data_vbase_off)
							amount_to_read = PAGE_SIZE - data_vbase_off;
						else
							amount_to_read = as->data_size;
						
					}
					// Last page of data segment to read from elf 
					else if (faultaddress == ((data_vbase + as->data_size) & PAGE_FRAME))
					{
						amount_to_read = (as->data_size - (PAGE_SIZE - data_vbase_off)) &~ PAGE_FRAME;
						offset_elf -= data_vbase_off;
					}
					// Middle page of data segment to read from elf
					else 
					{
						amount_to_read = PAGE_SIZE;
						offset_elf -= data_vbase_off;
					}
					
					if (load_page_on_demand(as->v, paddr_tmp, PAGE_SIZE, amount_to_read, offset_elf))
						panic ("can't load page on demand\n");
						
					increment_PAGE_faults_disk();
					increment_PAGE_faults_elf();
				}	
			}
			
			// Stack segment
			else if (IS_STACK)
			{
				increment_PAGE_faults_zeroed();
				// zero the region we want to use for the stack
				as_zero_region(paddr, 1);		
			}
		}
	}
	
	/* make sure it's page-aligned */
	KASSERT((paddr & PAGE_FRAME) == paddr);

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	// Load an appropriate entry into the TLB (replacing an existing TLB entry if necessary)
	// substitute the following code
	int INVALID_FOUND = 0;
	
	ehi = faultaddress;
	if (IS_TEXT)
		elo = paddr | TLBLO_VALID; //read-only
	else
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
	
	
	// Search for invalid TLB entry
	for (i=0; i<NUM_TLB; i++) 
	{
		tlb_read(&old_ehi, &old_elo, i);
		if (!(old_elo & TLBLO_VALID)) 
		{
			increment_TLB_faults_free();
			tlb_write(ehi, elo, i);
			splx(spl);
			INVALID_FOUND = 1;
			return 0;
		}
	}
	
	// Need to replace a TBL entry
	if (!INVALID_FOUND)
	{
		increment_TLB_faults_replace();
		int index_TLB = tlb_get_rr_victim();
		tlb_write(ehi, elo, index_TLB);
		splx(spl);
	}
	
	return 0;
}

// local function
void vm_can_sleep(void)
{
	if (CURCPU_EXISTS()) {
		/* must not hold spinlocks */
		KASSERT(curcpu->c_spinlocks == 0);

		/* must not be in an interrupt handler */
		KASSERT(curthread->t_in_interrupt == 0);
	}
}

// Allocate/free some kernel-space virtual pages, called by kmalloc
vaddr_t alloc_kpages(unsigned npages)
{
	paddr_t pa;

	vm_can_sleep();
	pa = getppages(npages);
	if (pa==0) {
		return 0;
	}
	return PADDR_TO_KVADDR(pa);
}

// free kernel heap pages, called by kfree
void free_kpages(vaddr_t addr) 
{
	if (isTableActive()) {
		paddr_t paddr = addr - MIPS_KSEG0;
		//KASSERT(nRamFrames>first);
		freeppages(paddr); // update free-fame list
	}
}

struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	as->text_vbase = 0;
	as->text_offset = 0;
	as->text_npages = 0;
	as->text_size = 0;
	
	as->data_vbase = 0;
	as->data_offset = 0;
	as->data_npages = 0;
	as->data_size = 0;
	
	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;

	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}

	/*
	 * Write this.
	 */

	(void)old;

	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	vm_can_sleep();
	kfree(as);
}

void
as_activate(void)
{
	int i, spl;
	struct addrspace *as;
	
	as = proc_getas();
	if (as == NULL) {
		/*
		 * Kernel thread without an address space; leave the
		 * prior address space in place.
		 */
		return;
	}

	// Disable interrupts on this CPU while frobbing the TLB. 
	spl = splhigh();
	
	// Loading invalid entries in the TLB
	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
	
	
	increment_TLB_invalidations();
	
}

void
as_deactivate(void)
{
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz, size_t file_sz, off_t offset, 
	struct vnode *v, int readable, int writeable, int executable)
{
	size_t npages;
	
	vm_can_sleep();

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	//vaddr &= PAGE_FRAME; // removed to provide aligmnement between vaddr e paddr

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	/* We don't use these - all pages are read-write */
	// Actually might want to use this since text should be read-only
	(void)readable;
	(void)writeable;
	(void)executable;
	
	as->v = v;
	
	if (as->text_vbase == 0) {
		as->text_vbase = vaddr;
		as->text_npages = npages;
		as->text_offset = offset;
		as->text_size = file_sz;
		return 0;
	}

	if (as->data_vbase == 0) {
		as->data_vbase = vaddr;
		as->data_npages = npages;
		as->data_offset = offset;
		as->data_size = file_sz;
		return 0;
	}
	
	// can't handle more than two regions
	kprintf("vm: Warning: too many regions\n");
	return ENOSYS;
}

int
as_prepare_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	/*
	 * Write this.
	 */

	(void)as;

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	return 0;
}

