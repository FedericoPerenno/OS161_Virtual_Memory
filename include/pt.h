#ifndef _PT_H_
#define _PT_H_

#include <types.h>

struct ipt_entry_t 
{
    pid_t pid;
	vaddr_t vaddr;
};

struct ipt_t 
{
    struct ipt_entry_t * entry;
    int size;
};

int pt_create(void);
int page_is_in_mem(pid_t pid, vaddr_t vaddr, int *index);
int pt_get_FIFO_victim (void);
int pt_get_victim (void);
void pt_set_entry (pid_t pid, vaddr_t vaddr, int index);
vaddr_t pt_get_vaddr (int index);
pid_t pt_get_pid (int index);
int getFullPages(void);

#endif // _PT_H_ 

