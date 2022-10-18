* FILE: explicit.c
 * -----------------
 * This file contains an implementation of an explicit free list heap allocator. Core implementation functions include 
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

// size of headers in bytes
#define HEADER_SIZE 8

// size of payload in free block
#define PAYLOAD_SIZE 160

/* FUNCTION: roundup
 * ------------------
 * This function was taken from the bump.c implementation with no changes being made. 
 * The function, given a size request sz and an alignment mult, returns the closest rounded-up multiple of mult. 
 */
size_t roundup(size_t sz, size_t mult) {
    size_t roundup = (sz + mult - 1) & ~(mult - 1);
    if (roundup > 0 && roundup < HEADER_SIZE + 16) {
        return HEADER_SIZE + 16; // header size and linked list must be accounted for
    }
    return roundup + 8;
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

/* FUNCTION: getprevptr
 * ---------------------
 * This function is passed the address for a block header and returns the first half of the linked list, the prevptr, 
 * located immediately after the header in heap memory. 
 */
unsigned long getprevptr(void *block_header) {
    return *(unsigned long *)((char *)block_header + HEADER_SIZE);
}

/* FUNCTION: getnextptr
 * ---------------------
 * This function is passed the address for a block header and returns the second half of the linked list, the nextptr, 
 * located immediately after the prevptr in heap memory. 
 */
unsigned long getnextptr(void *block_header) {
    return *(unsigned long *)((char *)block_header + HEADER_SIZE + 8);
}

/* FUNCTION: setprevptr
 * ---------------------
 * This function takes a pointer to the header of a heap allocated block, and an address. The address is placed
 * in memory in the prevptr place, located immediately after the header. 
 */
void setprevptr(void *block_header, unsigned long prev_address) {
    memcpy((char *)block_header + HEADER_SIZE, &prev_address, 8);
}

/* FUNCTION: setnextptr
 * ---------------------
 * This function takes a pointer to the header of a heap allocated block, and an address. The address is placed
 * in memory in the nextptr place, located immediately after the prevptr.
 */
void setnextptr(void *block_header, unsigned long next_address) {
    memcpy((char *)block_header + HEADER_SIZE + 8, &next_address, 8);
}

/* FUNCTION: getfirstfree
 * ----------------------
 * This function begins at the heap start and loops through blocks one by one until it comes across a free block. 
 * The address to the beginning of this block is then returned. 
 */
void *getfirstfree() {
    void *traverse_blocks = segment_start;
    while (!isfree(traverse_blocks)) {
        traverse_blocks = (char *)traverse_blocks + getsize(traverse_blocks);
        if ((char *)traverse_blocks > (char *)segment_start + segment_size) {
            return NULL;
        }
    }
    return traverse_blocks;
}

/* FUNCTION: updatelinkedlist
 * --------------------------
 * This function is called anytime a node is freed or made occupied, and updates the explicit free linked list. 
 */
void updatelinkedlist() {
    void *traverse_blocks = segment_start;
    unsigned long prev_ptr = 0;
    while ((char *)traverse_blocks < (char *)segment_start + segment_size) {
        if (isfree(traverse_blocks)) {
            // handle prevptr
            setprevptr(traverse_blocks, prev_ptr);
            // handle nextptr
            if (getprevptr(traverse_blocks) != 0) {
                void *go_back = (void *)getprevptr(traverse_blocks);
                // printf("\n\nPrevious address is %lu\n\n", *(unsigned long *)go_back);
                setnextptr(go_back, (unsigned long)traverse_blocks);
            }
            setnextptr(traverse_blocks, 0);
            prev_ptr = (unsigned long)traverse_blocks;
        }
        traverse_blocks = (char *)traverse_blocks + getsize(traverse_blocks); // go to next block
    }
}

/* FUNCTION: placeheader
 * ---------------------
 * This function takes a pointer to the beginning of a block on the heap, and encodes an eight byte header consisting of
 * the size of the block. The free status is encoded in the least significant bit of the header using |=
 */
void placeheader( void *block_start, unsigned long size, int status) {
    if (status != 1 && status != 0) {
        breakpoint();
    }
    unsigned long header = size;
    header |= (unsigned long)status;
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

/* FUNCTION: mymalloc
 * -------------------
 * This function takes the requested size from the client and returns a pointer to a heap allocated block with at least
 * requested_size space. If requested size is 0, or if the heap has filled up and there is no space reamining, a NULL 
 * pointer is returned. 
 * Within the function, heap blocks are searched via an explicit free list, and located using the linked list located 
 * within each free block's payload. Te first block found which can hold the requested size (at minimum) is utilized in
 *  the allocation. As blocks become occupied, their headers are updated and the free block linked list is updated. 
 */
void *mymalloc(size_t requested_size) {
    if (requested_size <= 0) {
        return NULL;
    }
    size_t needed = roundup(requested_size, ALIGNMENT);
    if (needed + nused > segment_size) {
        return NULL;
    } else if (needed > MAX_REQUEST_SIZE) {
        return NULL;
    }
    // here, must search through headers to find free block
    void *traverse_free = getfirstfree(); // look implicitly only for first free block. After this mymalloc will run explicitly
    if (traverse_free == NULL) {
        printf("\n\nNO FREE BLOCKS REMAINING\n\n");
        return NULL;
    }
    while (getsize(traverse_free) < needed) {
        traverse_free = (void *)getnextptr(traverse_free);
        if ((char *)traverse_free == 0) {
            return NULL;
        }
    }
    // following code marks block as occupied and handles adjacent block re-headering
    unsigned long free_block_size = getsize(traverse_free);
    unsigned long remaining_space = free_block_size - needed;
    if (remaining_space >= (40)) {
        placeheader((char *)traverse_free + needed, remaining_space, 0);
        placeheader(traverse_free, needed, 1);
        nused += needed;
    } else {
        placeheader(traverse_free, free_block_size, 1);
        nused += free_block_size;
    }
    updatelinkedlist();
    return (char *)traverse_free + HEADER_SIZE;
}

/* FUNCTION: myfree
 * -----------------
 * This function frees a heap allocated block. It does so by changing the least significant bit of the eight byte header
 * to zero. If a NULL ptr is passed, nothing happens. 
 * After freeing the block, the explicit free linked list is updated before any coalescing of right-adjacent blocks is 
 * handled. After coalesing, the linked list is once again updated. 
 */
void myfree(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    ptr = (char *)ptr - HEADER_SIZE;
    *(unsigned long *)ptr &= ~1;
    // coalesce while possible
    void *next_block = (char *)ptr + getsize(ptr);
    while ((char *)ptr + getsize(ptr) < (char *)segment_start + segment_size && isfree(next_block)) {
        placeheader(ptr, getsize(ptr) + getsize(next_block), 0);
        setnextptr(ptr, getnextptr(next_block));
        next_block = (char *)ptr + getsize(ptr);
    }
    // update linked list
    updatelinkedlist();
}

/* FUNCTION: myrealloc
 * --------------------
 * This function takes in a pointer to an already allocated block, and returns a pointer to the payload of a new block 
 * of size at minimum new_size. If a block can be reallocated in place (with the original block resized) then this 
 * is done. 
 */
void *myrealloc(void *old_ptr, size_t new_size) {
    // I don't test for new_size < 0 as size_t is unsigned
    if (new_size == 0 && old_ptr != NULL) {
        myfree(old_ptr);
        return NULL;
    } else if (old_ptr == NULL) {
        return mymalloc(new_size);
    }
    size_t needed = roundup(new_size, ALIGNMENT);
    size_t old_size = getsize((char *)old_ptr - HEADER_SIZE);
    void *next_block = (char *)old_ptr + old_size  - HEADER_SIZE;
    if (isfree(next_block) && (signed long)(old_size + getsize(next_block) - new_size) > 32) { // +32 to account for linked list space
        // realloc in place
        if (new_size == 979) {
            breakpoint();
        }
        if (needed < old_size) {
            placeheader((char *)next_block + (needed - old_size), getsize(next_block) + (old_size - needed), 0);
        } else {
            placeheader((char *)next_block + (needed - old_size), getsize(next_block) + (old_size - needed), 0);
        }
        placeheader((char *)old_ptr - HEADER_SIZE, needed, 1);
        updatelinkedlist(next_block, 0);
        return old_ptr;
    } else {
        void *new_ptr = mymalloc(new_size);
        if (old_size > new_size) {
            memmove(new_ptr, old_ptr, new_size);
        } else {
            memmove(new_ptr, old_ptr, old_size);
        }
        myfree(old_ptr);
        return new_ptr;
    }
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
        printf("More heap is being used than total available!");
        breakpoint();
        return false;
    }
    void *address_tracker = segment_start;
    for (int i = 0; i < nused; i++) {
        if ((unsigned long)address_tracker % 8 != 0) {
            printf("Bad address\n");
            return false;
        }
        if ((char *)address_tracker >= (char *)segment_start + segment_size) {
            return true;
        }
        unsigned long size = getsize(address_tracker);
        if (size % 8 != 0) {
            return false;
        }
        address_tracker = (char *)address_tracker + size;
        i += size + 1;
    }
    return true;
}

/* FUNCTION: dump_heap()
 * ---------------------
 * This function can be called from within the debugger, and will print the contents of the heap along with useful
 * information. This information includes: Heap start location, heap end location, number of bytes in use, and 
 * block size for each block along with its free status. 
 * Additionally, the function loops through the explicit free list and prints each block's location, along with the
 * address locations encoded for its prevptr and nextptr. 
 */
void dump_heap() {
    printf("The heap begins at address %p, and ends at %p. There are %lu bytes currently being used.\n", segment_start, (char *)segment_start + segment_size, nused);
    char *address_tracker = (char *)segment_start;
    int counter = 1;

    for (int i = 0; i < nused; i++) {
        if ((char *)address_tracker >= (char *)segment_start + segment_size) {
            break;
        }
        unsigned long size = getsize(address_tracker);
        printf("Block #%i has size %lu, and free status is %i -- block begins at %lu and ends at %lu\n", counter, size, isfree(address_tracker), (unsigned long)address_tracker, (unsigned long)address_tracker + getsize(address_tracker));
        address_tracker += size;
        i += size + 1;
        counter++;
    }
    printf("\n\n\n\n");
    address_tracker = (char *)getfirstfree(segment_start);
    if (address_tracker == NULL) {
        printf("There are no free blocks\n\n");
        return;
    }
    for (int i = 1; address_tracker > (char *)segment_start && address_tracker < (char *)segment_start + segment_size; i++) {
        unsigned long size = getsize(address_tracker);
        printf("Free block #%i has size %lu. Previous ptr is %lu and next ptr is %lu. It exists at address %lu\n", i, size, getprevptr(address_tracker), getnextptr(address_tracker), (unsigned long)address_tracker);
        address_tracker = (void *)getnextptr(address_tracker);
    }
}
                                                                                                                                                                                          352,1         Bot
