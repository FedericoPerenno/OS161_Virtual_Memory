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
#include <kern/fcntl.h>
#include <uio.h>
#include <vfs.h>
#include <vnode.h>

#include <swapfile.h>

static swapfile_t *mySwapfile;

struct vnode* swapfile_node;
char swapfile_name[] = "emu0:SWAPFILE"; //emu0:SWAPFILE";

int swapfile_create (void)
{	
	int i, result;
		
    mySwapfile = (swapfile_t *) kmalloc(sizeof(swapfile_t)*SWAP_TABLE_SIZE);
    if (mySwapfile == NULL){
        return 1;
    }
    
    for (i=0;i<SWAP_TABLE_SIZE;i++)
	{
        mySwapfile[i].pid = -1;
		mySwapfile[i].vaddr = 0;
    }
    
    
	result = vfs_open(swapfile_name, O_RDWR|O_CREAT, 0, &swapfile_node);
    if (result)
	{
		panic ("Can't open the swapfile");
		return 1;
	}

    return 0;
}

int page_is_in_swapfile(pid_t pid, vaddr_t vaddr, int *index)
{
	int i;
	
	for (i = 0; i < SWAP_TABLE_SIZE; i++)
	{
		if (mySwapfile[i].pid == pid && mySwapfile[i].vaddr == vaddr)
		{
			*index = i;
			return 1;
		}
	}
	
	return 0;
}

int read_from_swapfile(int index_sf, int index_pt)
{
	int result;
	struct iovec iov;
	struct uio ku;
	
	paddr_t paddr = index_pt * PAGE_SIZE;
	
	uio_kinit(&iov, &ku, (void*) PADDR_TO_KVADDR(paddr), PAGE_SIZE, index_sf * PAGE_SIZE, UIO_READ);
	result = VOP_READ(swapfile_node, &ku);
	if (result){
		return result;
	}

	if (ku.uio_resid!=0){
		return ENOEXEC;
	}
	
	// clean swapfile_table entry
	mySwapfile[index_sf].pid = -1;
	mySwapfile[index_sf].vaddr = 0;

	return result;
}

int write_to_swapfile (pid_t pid, vaddr_t vaddr, int index_pt)
{
	int i, result;
	int index_sf = 0;
	struct iovec iov;
	struct uio ku;
	int found = 0;
	
	paddr_t paddr = index_pt * PAGE_SIZE;
	int swapfile_offset = 0;
	
	for (i = 0; i < SWAP_TABLE_SIZE; i++)
	{
		if (mySwapfile[i].pid == -1)
		{
			index_sf = i;
			swapfile_offset = i * PAGE_SIZE;
			found = 1;
			break;
		}
	}
	
	if (!found)
		panic ("Swapfile is full");
	
	uio_kinit(&iov, &ku, (void *)PADDR_TO_KVADDR(paddr), PAGE_SIZE, swapfile_offset, UIO_WRITE);
    result = VOP_WRITE(swapfile_node, &ku);
	if (result){
		return result;
	}

	if (ku.uio_resid!=0){
		return ENOEXEC;
	}
	
	// update swapfile table
	mySwapfile[index_sf].pid = pid;
	mySwapfile[index_sf].vaddr = vaddr;

	return result;
}

