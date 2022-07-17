#ifndef _VM_TLB_H_
#define _VM_TLB_H_


int tlb_get_rr_victim(void); 

/* We only need this one function, which allows to select a TLB
entry to be replaced, because in <mips/tlb.h> the functions needed to 
write, read and probe the tlb, as well as constants allowing to handle 
the tlb are already defined */ 

#endif // _VM_TLB_H_ 