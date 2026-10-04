#ifndef FASTHEAP
#include <stdint.h>

void* initheap (void* storage, uintptr_t bytesize);
// returns NULL if failed, otherwise guaranteed to work
/* takes the memory given, and convert it into the heap pointer and one free block. */

void* alloch (void* heapaddress, uintptr_t bytesize);
// returns NULL on failure, otherwise always valid.
/* we look at a heap pointer, and traverse it last back. if we find a block that matches the rounded up size
we allocate and remove it from the free list, reconnecting the frayed ends of the list */ 

void freeh (void* heapaddress, void* ptr);
/* we take in a pointer with the correct structure assumed
set its previous pointer to the old last pointer
set its status to the proper free type
and set the last free pointer to it 
very bad corruption if you free something that does not exist.*/

int extendheap (void* heapaddress, void* room, uintptr_t bytesize);
//0 = sucess, 1 = too small to make a block
/* take given memory, attach it to an existing heap as a free block*/

void defragment (void* heapaddress);
/*defragments a heap, now o(2n) instead of o(n^2). does not defragment between page boundaries, even if contigious*/

#define FASTHEAP
#endif
