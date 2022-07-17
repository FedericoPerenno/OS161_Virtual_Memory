#include <types.h>
#include <clock.h>
#include <copyinout.h>
#include <syscall.h>
#include <current.h>
#include <thread.h>
#include <addrspace.h>
#include <kern/unistd.h>
#include <proc.h>
#include <lib.h>

int sys_read(int fd, userptr_t buf, int size)
{
	if (fd!=STDIN_FILENO) 
	{
		kprintf("sys_read functionality not yet implemented\n");
		return -1;
	}
	
	char *p = (char *)buf;
	int i;

	for (i = 0; i < size; i++) {
		int c = getch();
		if (p[i] < 0) 
	  		return i; // EOF
	  	p[i] = (char) c;
	}

	return size;	
}

int sys_write(int fd, userptr_t buf, int size)
{
	if (fd!=STDOUT_FILENO && fd!=STDERR_FILENO) 
	{
		kprintf("sys_write functionality not yet implemented\n");
		return -1;
	}
	
	int i;
	char *p = (char *)buf;
	
	for (i = 0; i < size; i++) 
	{
		putch(p[i]);
	}

	return size;
}

void sys__exit(int status)
{
	struct addrspace *as;
	as = proc_getas();
	as_destroy(as);
	thread_exit();
	(void) status;
}
