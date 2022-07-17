#ifndef _SWAPFILE_H_
#define _SWAPFILE_H_

#include <types.h>

#define SWAP_SIZE 9*1024*1024 // 9 MB
#define SWAP_TABLE_SIZE (SWAP_SIZE / PAGE_SIZE)

typedef struct sf_entry_t 
{
    pid_t pid;
	vaddr_t vaddr;
}swapfile_t;

int swapfile_create (void);
int page_is_in_swapfile(pid_t pid, vaddr_t vaddr, int *index);
int read_from_swapfile(int index_sf, int index_pt);
int write_to_swapfile (pid_t pid, vaddr_t vaddr, int index_pt);

#endif // _SWAPFILE_H_ 


