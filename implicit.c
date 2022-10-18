* FILE: implicit.c
 * -----------------
 * This file contains an implementation of an implicit free list heap allocator. Core implementation functions include 
 * mymalloc (an implementation of malloc()), myrealloc (an implementation of realloc()), and myfree (and implementation
 *  of free()). 
 * There also exist functions validate_heap, called inbetween requests to check that the heap is properly aligned and 
 * set up. There is also dump_heap, not called anywhere within the program but can be called when using gdb to analyze 
 * the contents of the heap with useful data. 
 */
#include "./allocator.h"
#include "./debug_break.h"
#include <stdio.h> // for printf
#include <string.h> // for memcpy

// globals taken from bump.c implementation
static void *segment_start;
static size_t segment_size;
static size_t nused;

// how many bytes are printed per line in dump_heap
#define BYTES_PER_LINE 32

// define size of header in bytes
#define HEADER_SIZE 8

/* FUNCTION: placeheader
 * ---------------------
 * This function takes a pointer to the beginning of a block on the heap, and encodes an eight byte header consisting of
 * the size of the block. The free status is encoded in the least significant bit of the header using |=
 */
void placeheader(void *block_start, unsigned long size, int status) {
    unsigned long header = size;
    header |= (unsigned long)status; // adds a 1 to the least significant bit for an in-use block. changes nothing for free block
    *(unsigned long *)block_start = header;
}

/* FUNCTION: myinit
 * -----------------
 * This must be called by a client before making any allocation
 * requests.  The function returns true if initialization was 
 * successful, or false otherwise. The myinit function can be 
 * called to reset the heap to an empty state. When running 
 * against a set of of test scripts, our test harness calls 
 * myinit before starting each new script.
 * The bulk of the body of this function was taken from the bump.c implementation. 
 */
bool myinit(void *heap_start, size_t heap_size) {
    segment_start = heap_start;
    segment_size = heap_size;
    nused = 0;
    placeheader(heap_start, heap_size, 0);
    return true;
}

/* FUNCTION: roundup
 * ------------------
 * This function was taken from the bump.c implementation with no changes being made. 
 * The function, given a size request sz and an alignment mult, returns the closest rounded-up multiple of mult. 
 */
size_t roundup(size_t sz, size_t mult) {
    return (sz + mult - 1) & ~(mult - 1);
}

/* FUNCTION: isfree
 * -----------------
 * This function takes a void * address and returns whether or not the least significant bit is a 0 (signifying that the
 * block is free, and returnvalue = true), or if the lsb is a 1 (signifying that the block is occupied, and 
 * returnvalue=false.
 */
bool isfree(void *header_address) {
    return (*(unsigned long *)header_address & (unsigned long)1) == 0;
}

/* FUNCTION: getsize
 * ------------------
 * This function takes a void * pointer to a block on the heap and returns its size. This is done by casting the pointer
 * to an unsigned long, dereferencing to get the eight byte header, and disregarding the least significant bit (which 
 * holds the status). 
 */
unsigned long getsize(void *header_address) {
    return (*(unsigned long *)header_address & ~((unsigned long)1));
}

/* FUNCTION: mymalloc
 * -------------------
 * This function takes the requested size from the client and returns a pointer to a heap allocated block with at least
 * requested_size space. If requested size is 0, or if the heap has filled up and there is no space reamining, a NULL 
 * pointer is returned. 
 * Within the function, heap blocks are searched via an implicit list, and the first block found which can hold the
 * requested size (at minimum) is utilized in the allocation. As blocks become occupied, their headers are updated. 
 */
void *mymalloc(size_t requested_size) {
    breakpoint();
    if (requested_size <= 0) {
        return NULL;
    }
    size_t needed = roundup(requested_size, ALIGNMENT) + HEADER_SIZE; // is +8 necessary here?
    if (needed + nused > segment_size) {
        return NULL;
    } else if (needed > MAX_REQUEST_SIZE) {
        return NULL;
    }
    // here, must search through headers to find free block
    void *search_headers = segment_start;
    while (!isfree(search_headers) || getsize(search_headers) < needed) {
        search_headers = (char *)search_headers + getsize(search_headers);
        if ((char *)search_headers > (char *)segment_start + segment_size) {
            return NULL;
        }
    }
    // following code marks block as occupied and handles adjacent block re-headering
    unsigned long free_block_size = getsize(search_headers);
    unsigned long remaining_space = free_block_size - needed;
    if (remaining_space >= (HEADER_SIZE + ALIGNMENT)) {
        placeheader((char *)search_headers + needed, remaining_space, 0);
        placeheader(search_headers, needed, 1);
        nused += needed;
    } else {
        placeheader(search_headers, free_block_size, 1);
        nused += free_block_size;
    }
    return (char *)search_headers + HEADER_SIZE;
}

/* FUNCTION: myfree
 * -----------------
 * This function frees a heap allocated block. It does so by changing the least significant bit of the eight byte header
 * to zero. If a NULL ptr is passed, nothing happens. 
 */
void myfree(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    ptr = (char *)ptr - HEADER_SIZE;
    *(unsigned long *)ptr &= ~1; // set least sig bit to 0
}

/* FUNCTION: myrealloc
 * --------------------
 * This function takes in a pointer to an already allocated block, and returns a pointer to the payload of a new block 
 * of size at minimum new_size.
 */
void *myrealloc(void *old_ptr, size_t new_size) {
    // I don't test for new_size < 0 as size_t is unsigned
    breakpoint();
    if (new_size == 0 && old_ptr != NULL) {
        myfree(old_ptr);
        return NULL;
    } else if (old_ptr == NULL) {
        return mymalloc(new_size);
    }
    void *new_ptr = mymalloc(new_size);
    unsigned int old_size = getsize((char *)old_ptr - HEADER_SIZE);
    if (old_size > new_size) {
        memcpy(new_ptr, old_ptr, new_size);
    } else {
        memcpy(new_ptr, old_ptr, old_size);
    }
    myfree(old_ptr);
    return new_ptr;
}

/* FUNCTION: validate_heap
 * ----------------------------
 * Return true if all is ok, or false otherwise.
 * This function is called periodically by the test
 * harness to check the state of the heap allocator.
 * You can also use the breakpoint() function to stop
 * in the debugger - e.g. if (something_is_wrong) breakpoint();
 */
bool validate_heap() {
    if (nused > segment_size) {
        printf("More heap is being used than total available!\n");
        breakpoint();
        return false;
    } return true;
}

/* FUNCTION: dump_heap()
 * ---------------------
 * This function can be called from within the debugger, and will print the contents of the heap along with useful
 * information. This information includes: Heap start location, heap end location, number of bytes in use, and 
 * block size for each block along with its free status. 
 */
void dump_heap() {
    printf("The heap begins at address %p, and ends at %p. There are %lu bytes currently being used.\n", segment_start, (char *)segment_start + segment_size, nused);
    char *address_tracker = (char *)segment_start;
    int counter = 1;
    for (int i = 0; i < nused; i++) {
        unsigned long size = getsize(address_tracker);
        printf("Block #%i has size %lu, and free status is %i\n", counter, size, isfree(address_tracker));
        address_tracker += size;
        i += size - 1;
        counter++;
    }
}
