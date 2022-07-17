#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <vm.h>
#include <proc.h>
#include <cpu.h>
#include <current.h>

#include <vmstats.h>

/*
	TLB_faults_free + TLB_faults_replace = TLB_faults;
	TLB_reloads + PAGE_faults_zeroed + PAGE_faults_disk = TLB_faults;
	PAGE_faults_elf + PAGE_faults_swapfile = PAGE_faults_disk;
*/

static int TLB_faults = 0;
static int TLB_faults_free = 0;
static int TLB_faults_replace = 0;
static int TLB_invalidations = 0;
static int TLB_reloads = 0;
static int PAGE_faults_zeroed = 0;
static int PAGE_faults_disk = 0;
static int PAGE_faults_elf = 0;
static int PAGE_faults_swapfile = 0;
static int SWAPFILE_writes = 0;


void increment_TLB_faults (void)
{
	TLB_faults++;
}

void increment_TLB_faults_free (void)
{
	TLB_faults_free++;
}

void increment_TLB_faults_replace (void)
{
	TLB_faults_replace++;
}

void increment_TLB_invalidations (void)
{
	TLB_invalidations++;
}

void increment_TLB_reloads (void)
{
	TLB_reloads++;
}

void increment_PAGE_faults_zeroed (void)
{
	PAGE_faults_zeroed++;
}

void increment_PAGE_faults_disk (void)
{
	PAGE_faults_disk++;
}

void increment_PAGE_faults_elf (void)
{
	PAGE_faults_elf++;
}

void increment_PAGE_faults_swapfile (void)
{
	PAGE_faults_swapfile++;
}

void increment_SWAPFILE_writes (void)
{
	SWAPFILE_writes++;
}

void print_vmstats (void)
{
	kprintf ("\n-------------------STATISTICS-------------------\n\n");
	kprintf ("The number of TLB Faults is: %d\n", TLB_faults);
	kprintf ("The number of TLB Faults with Free is: %d\n", TLB_faults_free);
	kprintf ("The number of TLB Faults with Replace is: %d\n", TLB_faults_replace);
	kprintf ("The number of TLB Invalidations is: %d\n", TLB_invalidations);
	kprintf ("The number of TLB Reloads is: %d\n", TLB_reloads);
	kprintf ("The number of Page Faults (Zeroed) is: %d\n", PAGE_faults_zeroed);
	kprintf ("The number of Page Faults (Disk) is: %d\n", PAGE_faults_disk);
	kprintf ("The number of Page Faults from ELF is: %d\n", PAGE_faults_elf);
	kprintf ("The number of Page Faults from Swapfile is: %d\n", PAGE_faults_swapfile);
	kprintf ("The number of Swapfile Writes is: %d\n", SWAPFILE_writes);
	
	if ((TLB_faults_free + TLB_faults_replace) != TLB_faults)
		kprintf ("WARNING: the sum of TLB Faults with Free and TLB Faults with Reload isn't correct\n");
	
	if ((TLB_reloads + PAGE_faults_zeroed + PAGE_faults_disk) != TLB_faults)
		kprintf ("WARNING: the sum of TLB Reloads, Page Faults (Zeroed) and Page Faults (Disk) isn't correct\n");	
	
	if ((PAGE_faults_elf + PAGE_faults_swapfile) != PAGE_faults_disk)
		kprintf ("WARNING: the sum of Page Faults from ELF and Page Faults from Swapfile isn't correct\n");
	
	kprintf ("\n------------------------------------------------\n\n");
}





