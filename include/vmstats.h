#ifndef _VMSTATS_H_
#define _VMSTATS_H_


void increment_TLB_faults (void);
void increment_TLB_faults_free (void);
void increment_TLB_faults_replace (void);
void increment_TLB_invalidations (void);
void increment_TLB_reloads (void);
void increment_PAGE_faults_zeroed (void);
void increment_PAGE_faults_disk (void);
void increment_PAGE_faults_elf (void);
void increment_PAGE_faults_swapfile (void);
void increment_SWAPFILE_writes (void);
void print_vmstats (void);


#endif // _VMSTATS_H_ 
