#ifndef _COREMAP_H_
#define _COREMAP_H_

#include <types.h>

int isTableActive(void); 
void coremap_bootstrap(void);
paddr_t getppages(unsigned long npages);
paddr_t getfreeppages(unsigned long npages);
int freeppages(paddr_t addr);

#endif // _COREMAP_H_ 
