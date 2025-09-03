#include "buddy.h"
#include <stdlib.h>
#include <sys/mman.h>
#include <stddef.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>

//this is about memory allocation using the buddy system, a free list implementation

//Define your Constants here
#define MIN 5 //minimum block size is 2^5 = 32 bytes
#define MAX 16 //maximum block size is // 2^16 = 64 KB




// A simple enum to mark whether a block of memory is free or allocated.
enum AEflag { Free, Taken };

struct head {
	enum AEflag status; // Is the block free or taken?
	short int level; // Level in buddy system (controls block size, e.g. 2^(level+MIN))
	struct head* next; // Linked list: pointer to next free block at same level
	struct head* prev; // Linked list: pointer to previous free block at same level
};

struct head* free_lists[MAX - MIN + 1] = { NULL }; // Array of free list heads for each level


//Complete the implementation of new here,Will eventually create the initial block of memory (using mmap() probably), and wrap it in a struct head.
struct head *new() {

	size_t total_size = (1 << (MAX)); // Total size of memory to allocate (2^MAX)

	//request memory from OS
	void* mem = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (mem == MAP_FAILED) {
		perror("mmap failed");
		return NULL;	
	}

	// Initialize the single large free block which is that 64KB block
	struct head* initial_block = (struct head*)mem;
	initial_block->status = Free;
	initial_block->level = MAX - MIN;
	initial_block->next = NULL;
	initial_block->prev = NULL;

	// Add this block to the appropriate free list
	free_lists[initial_block->level] = initial_block;
	return initial_block;
	
}

//Complete the implementation of level here,Will calculate what "level" (i.e., block size power of 2) is needed to fit a requested allocation size.
int level(int req) {

	int total_req = req + sizeof(struct head); // aacount also for the size of the header)
	int lvl = 0;
	while(total_req > (1<<(MIN +lvl))){
		lvl++;
	}
	if(lvl > (MAX-MIN)){
		return -1; // this is to indicate that the request is too large that it cannot be satisfied
	}
	return lvl;

}

//Complete the implementation of balloc here,This will be your malloc equivalent.
// It will:
			//Find or split a free block at the right level.
			//Mark it as Taken.
			//Return a pointer after the header (using hide()).

void* balloc(size_t size) {

	int req_lvl = level(size);
	if(req_lvl == -1){
		  return NULL; // request too large	
	}
	// Find the smallest available block that fits the request
	int i = req_lvl;
	while(i <=(MAX-MIN) && free_lists[i] == NULL){
		i++;
	}
	if(i > (MAX-MIN)){
		return NULL; // No suitable block found
	}

	// take the block from the free list
	struct head* block = free_lists[i];
	free_lists[i] = block->next;
	if(free_lists[i]){
		block->next->prev = NULL;
	}

	// Split blocks down untill we reach the requested level
	while(i>req_lvl){
		i--; //requet is smaller than the block we have so we go one level down

		//compute the buddy of the block we have
		struct head* buddy_block = split(block);

		//initialize the buddy block(right half)
		buddy_block->status = Free;
		buddy_block->level = i;
		buddy_block->next = free_lists[i];
		buddy_block->prev = NULL;
		if(free_lists[i]){
			free_lists[i]->prev = buddy_block;
		}
		free_lists[i] = buddy_block;

		//update the original block
		block->level = i;
		block->next = NULL;
		block->prev = NULL;
	}
	block->status = Taken;
	return hide(block); // return pointer to memory after the header

}

//Complete the implementation of bfree here
// This will be your free equivalent.
// It will:
			//Get the header from the pointer (using magic()).
			//Mark it as Free.
			//Coalesce with buddy if possible.
			//Try to merge it with its buddy if the buddy is also free.
void bfree(void* memory) {
	




}



//Helper Functions
struct head* buddy(struct head* block) {
	int index = block->level;
	long int mask = 0x1 << (index + MIN);
	return (struct head*)((long int)block ^ mask);

}

// ----split(block) doesn’t create memory — it just calculates the address of the other half of the split block.--------
struct head* split(struct head* block) {
	int index = block->level - 1;
	int mask = 0x1 << (index + MIN);
	return (struct head*)((long int)block | mask); //this returns a struct head* pointer to the other half of the block (the buddy).
}

struct head* primary(struct head* block) {
	int index = block->level;
	long int mask = 0xffffffffffffffff << (1 + index + MIN);
	return (struct head*)((long int)block & mask);
}

void* hide(struct head* block) {
	return (void*)(block + 1);
}


struct head* magic(void* memory) {
	return ((struct head*)memory - 1);
}
	


void dispblocklevel(struct head* block){
	printf("block level = %d\n",block->level);
}
void dispblockstatus(struct head* block){
	printf("block status = %d\n",block->status);
}

void blockinfo(struct head* block){
	printf("===================================================================\n");
	dispblockstatus(block);
	dispblocklevel(block);
	printf("start of block in memory: %p\n", block);
	printf("size of block in memory: %ld in bytes\n",sizeof(struct head));
	printf("===================================================================\n");
}






