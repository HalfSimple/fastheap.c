/*
a better version of my autoheap.h. defragment is now o(n) with smaller constants, allowing (unrealistic conditions)
a very sharded 2 gb to defragment in : 0.64 sec as compared to the old versions: 300+ sec. the cost is slightly more memory overhead, and ~0.04% slower allocations.

the general idea and usage is we start by handing over some room with initheap.
it is then turned into the heap pointer, and a free block.

room can be allocated with alloch, which internally rounds up your allocation to the closes uintptr_t unit
freeh assumes you pass a real pointer in a heap, and just shifts some values and stapes it to the free list.
extendheap adds new room as another free block.
defragment merges adjacent free blocks into larger ones. avoids falling off the edge to not segfault 
in a far cheaper way now, saving an n iterations per block. 

the heap pointer holds all of the information, you can easily have multiple heaps as long as they do not overlap.
you can even make a heap inside of another with 0 issues. 

DO NOT MULTITHREAD THIS WITHOUT A LOT OF SAFETY.
unless you like assembly debugging. 

the primary goal of this project was to make something that could function as a malloc like interface from raw pages, and this can be done with minimal changes
memory usage was partially prioritized as opposed to raw speed (as opposed to v1, which was 100% for memory usage), leading to something with far less overhead compared to an array allocator.

there was no AI code used in the making of this project.
*/

#include <stdint.h>


void* initheap (void* storage, uintptr_t bytesize){ // NULL if failure, heap pointer if vaild.

	/* we take the memory given, and convert it into the heap pointer and one free block. */

	if(storage == NULL){return NULL;}
	if(bytesize == 0 || bytesize < 4 * sizeof(uintptr_t)){return NULL;}
	uintptr_t* heap = (uintptr_t*) storage;
	heap[0] = (uintptr_t) &heap[1]; // setup the last free block pointer
	heap[1] = (bytesize - 3*sizeof(uintptr_t));
	heap[2] = 2; // bit one = free/alloc, bit two = safe/unsafe, bit 3 = reclaimed 
	heap[3] = 0; // no previous. bottom hit
	return (void*) heap;
}

void* alloch (void* heapaddress, uintptr_t bytesize){ // NULL if failed, void* to room if sucess.

	/* we look at a heap pointer, and traverse it last back. if we find a block that matches the rounded up size
	we allocate and remove it from the free list, reconnecting the ends of the list */ 

	if(bytesize == 0){return NULL;} // not POSIX compliant! for later versions potential change here.
	uintptr_t* heap = (uintptr_t*) heapaddress;
	uintptr_t* traverse = (uintptr_t*)*heap; // start search at the top free
	if(traverse == 0){return NULL;} // there are no free blocks in the list. 
	uintptr_t* from = heap; // pointer we came from on iteration.
	while(1){
		if(bytesize <= *traverse){ // this block can hold the allocation.
			if((*traverse) < ((bytesize - 1) / sizeof(uintptr_t) + 1) * sizeof(uintptr_t) + 3*(sizeof(uintptr_t))){
			// check if it is legal to make a second block and allocate, if not overallocate a bit.
				bytesize = *traverse;
				*(traverse + 1) = *(traverse + 1) | 1; // keep the type and set to allocated
				goto allocate; 
			}
			*(traverse + ( ((bytesize - 1) / sizeof(uintptr_t)) + 3)) = (*traverse) - ((((bytesize - 1) / sizeof(uintptr_t) + 1) * sizeof(uintptr_t)) + 2*sizeof(uintptr_t)); 
			// create a new block start,holding length calulated by (old total length) - (rounded allocation) - (managerial space requirements)
			*(traverse + ( ((bytesize - 1) / sizeof(uintptr_t)) + 4)) = *(traverse + 1) & 2;
			// set the status of the new block to free and unsafe if the old block was.
			*(traverse + ( ((bytesize - 1) / sizeof(uintptr_t)) + 5)) = *(traverse + 2);
			// set the previous pointer hidden in the new block to the previous pointer stored in the old block.
			*(traverse + 2) = (uintptr_t)(traverse + ( ((bytesize - 1) / sizeof(uintptr_t)) + 3));
			// use the old blocks room for a temp variable pointing to our new block as the previous
			// this is because we just connect the list using this slot (works for both cases) to the heap pointer
			*traverse = ((((bytesize-1) / sizeof(uintptr_t)) + 1)) * sizeof(uintptr_t); 
			// set the length of the old block to the rounded allocation
			*(traverse + 1) = 1;
			// due to the fact we created a block at the end of this one, this is no longer at the edge and is now safe.
			allocate:
			*from = *(traverse + 2);
			// reconnect the list, removing the now allocated block from the list (pop a link, connect the chains)
			return((void*) (traverse + 2));
			// return the interal space of the block, now safe to use
		} else { // follow the pointer to the previous block, if it exists.
			if(*(traverse + 2) == 0){return NULL;} 
			from = traverse + 2;
			traverse = (uintptr_t*)*(traverse + 2);
		}
	}	 
}

void freeh (void* heapaddress, void* ptr){
	/* we take in a pointer with the correct structure assumed
	set its previous pointer to the old last pointer
	set its status to the proper free type
	and set the last free pointer to it */

	if(ptr == NULL){return;}
	uintptr_t* heap = (uintptr_t*) heapaddress;
	uintptr_t* target = (uintptr_t*) ptr;
	*target = *heap;
	*(target - 1) = *(target - 1) & 2; // if unsafe, make free unsafe. if safe make safe free. 
	*heap = (uintptr_t) (target - 2);
	return;
}

int extendheap (void* heapaddress, void* room, uintptr_t bytesize){ // 0 = attached, 1 = something invalid

	/* take given memory, attach it to an existing heap as a free block*/

	if(room == NULL){return 1;}
	if(bytesize < 3*sizeof(uintptr_t)){return 1;}
	uintptr_t* heap = (uintptr_t*) heapaddress;
	uintptr_t* block = (uintptr_t*) room;
	*block = bytesize - 2*sizeof(uintptr_t);
	*(block + 1) = 2; // free, unsafe
	*(block + 2) = *heap; // connect to previous last
	*heap = (uintptr_t) block; // become the last
	return 0;
}

void defragment (void* heapaddress){ // :) I found a solution that is not horrid! 50,000%+ improvement is pretty nice in my eyes
	uintptr_t* heap = (uintptr_t*) heapaddress;
	if(*heap == 0){return;} // nothing is free
	uintptr_t* from = (uintptr_t*) *heap; // start at the last free block
	// start our loop at the last free block
	
	while(1){
		if(*(from + 1) == 2 || *(from + 1) & 1 || *(from + 1) & 8){
			goto getnext;
		} // we now know it is safe to jump ahead.
		uintptr_t candidate = ((uintptr_t) from) + 2*sizeof(uintptr_t) + *from;
		// location of the next block, as it must exist
		if(!(*(((uintptr_t*) candidate) + 1) & 1)){
		// the next block is not allocated
		 *from = *from + 2*sizeof(uintptr_t) + *((uintptr_t*) candidate);
		 // the new length is the old length + new block and its control data
		 *(((uintptr_t*) candidate) + 1) = *(((uintptr_t*) candidate) + 1) | 8; // set a mark
		}
		if (!(*(((uintptr_t*) candidate) + 1) & 2)){
		// continue explansion if we did not hit the edge
			if(!(*(((uintptr_t*) candidate) + 1) & 1)){
				continue;
			} else {
				goto getnext; // next block is not free. 
			}
		} else {
			*(from + 1) = *(from + 1) | 2; // we hit the edge, mark that.
		}
		getnext:
		if(*(from+2) == 0){break;} else {from = (uintptr_t*)*(from + 2);}
	}
	
	from = heap;
	uintptr_t* current = (uintptr_t*)*(heap);
	// cleanup pass, clear all dead nodes from the list as they are not safe to allocate. 
	while(1){
		if(current == 0){return;}
		if((*(current + 1) & 8) == 8){
		// marked block found
			*from = *(current + 2);
			current = (uintptr_t*)*(current + 2);
			continue;
		}
		from = current + 2;
		current = (uintptr_t*)*(current + 2);
	}
}



