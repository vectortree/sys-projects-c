/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "debug.h"
#include "sfmm.h"

#define ALIGN_SIZE (sizeof(long double))
#define HEAP_START (sf_mem_start())
#define HEAP_END (sf_mem_end())
#define ROW_SIZE (sizeof(sf_header))
#define MIN_BLOCK_SIZE (sizeof(sf_block))
#define HEADER(p) (((sf_block *)p)->header)
#define FOOTER(p) (*((sf_footer *)((char *)p + GET_BLOCK_SIZE(p))))
#define XOR_MAGIC(content) ((content) ^ (MAGIC))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define PACK(payload_size, block_size, flags) (((uint64_t)payload_size << 32) | (block_size) | (flags))
#define GET_PAYLOAD_SIZE(p) (XOR_MAGIC(HEADER(p)) >> 32)
#define GET_BLOCK_SIZE(p) ((XOR_MAGIC(HEADER(p)) & 0xffffffff) & ~(0xf))
#define GET_ALLOC(p) (XOR_MAGIC(HEADER(p)) & THIS_BLOCK_ALLOCATED)
#define GET_PREV_ALLOC(p) (XOR_MAGIC(HEADER(p)) & PREV_BLOCK_ALLOCATED)
#define IN_QKLST(p) (XOR_MAGIC(HEADER(p)) & IN_QUICK_LIST)
#define SET_ALLOC(p) (HEADER(p) = XOR_MAGIC(XOR_MAGIC(HEADER(p)) | THIS_BLOCK_ALLOCATED))
#define SET_PREV_ALLOC(p) (HEADER(p) = XOR_MAGIC(XOR_MAGIC(HEADER(p)) | PREV_BLOCK_ALLOCATED))
#define SET_IN_QKLST(p) (HEADER(p) = XOR_MAGIC(XOR_MAGIC(HEADER(p)) | IN_QUICK_LIST))
#define PROLOGUE ((sf_block *)HEAP_START)
#define EPILOGUE ((sf_block *)((char *)HEAP_END - ALIGN_SIZE))
#define NEXT_BLOCK(p) ((sf_block *)((char *)p + GET_BLOCK_SIZE(p)))
#define PREV_BLOCK(p) ((sf_block *)((char *)p - ((XOR_MAGIC(((sf_block *)p)->prev_footer) & 0xffffffff) & ~(0xf))))

void insert_block_free_list(sf_block *, sf_block *);
void delete_block_free_list(sf_block *);
void insert_block_quick_list(sf_block **, sf_block **);
void *delete_block_quick_list(sf_block **);
int get_free_list_index(sf_size_t);
sf_block *search_free_list(sf_block *, sf_size_t);
sf_block *coalesce(sf_block *);
void *serve_alloc_request(sf_size_t, sf_size_t);
void init_lists();
int extend_heap();

void insert_block_free_list(sf_block *sentinel, sf_block *block) {
    sentinel->body.links.next->body.links.prev = block;
    block->body.links.next = sentinel->body.links.next;
    block->body.links.prev = sentinel;
    sentinel->body.links.next = block;
}

void delete_block_free_list(sf_block *block) {
    if(GET_ALLOC(block) == THIS_BLOCK_ALLOCATED) return;
    block->body.links.prev->body.links.next = block->body.links.next;
    block->body.links.next->body.links.prev = block->body.links.prev;
}

void insert_block_quick_list(sf_block **first, sf_block **block) {
    if(*first != NULL) (*block)->body.links.next = *first;
    *first = *block;
}

void *delete_block_quick_list(sf_block **first) {
    void *return_block = (*first)->body.payload;
    *first = (*first)->body.links.next;
    return return_block;
}

int get_free_list_index(sf_size_t size) {
    // To find the appropriate free list, we need to first
    // divide the size of the block by MIN_BLOCK_SIZE (integer division)
    // Then we need to find the closest power of 2 that is smaller than the result
    // If the result is 1, then return the first index (i.e., 0)
    // We need to return the index that corresponds to its interval
    // The index is i if 2^i is equal to the closest power of 2
    // 0:[M, 2M], 1:(2M, 4M], 2:(4M, 8M], etc.
    uint32_t result = size / MIN_BLOCK_SIZE;
    if(result == 1) return 0;
    uint64_t power = 1;
    int index = -1;
    while(result > power) {
        power *= 2;
        ++index;
    }
    if(index >= NUM_FREE_LISTS) index = NUM_FREE_LISTS - 1;
    return index;
}

sf_block *search_free_list(sf_block *free_list_head, sf_size_t block_size) {
    sf_block *free_block = free_list_head->body.links.next;
    while(free_block != free_list_head) {
        if(block_size <= GET_BLOCK_SIZE(free_block))
            return free_block;
        free_block = free_block->body.links.next;
    }
    return NULL;
}

void align(sf_size_t *block_size) {
    // Note: ALIGN_SIZE == sizeof(long double) == 16
    // If block size is not aligned to a 16-byte (double memory row) boundary,
    // then it must be rounded up to the next multiple of 16 (in bytes)
    // If block size is less than the minimum size of a block (i.e., 32 bytes),
    // then set block size to the minimum size of a block
    if(*block_size % ALIGN_SIZE != 0)
        *block_size = ALIGN_SIZE * ((*block_size / ALIGN_SIZE) + 1);
    if(*block_size < MIN_BLOCK_SIZE) *block_size = MIN_BLOCK_SIZE;
}

sf_block *coalesce(sf_block *free_block) {
    // There are four possible cases to consider when
    // coalescing a free block with its immediately preceding
    // and proceeding blocks:
    // Case 1: Its previous and next blocks are both allocated
    // Case 2: Its previous block is allocated and its next block is free
    // Case 3: Its previous block is free and its next block is allocated
    // Case 4: Its previous and next blocks are both free
    // To check whether the previous block is allocated or not,
    // simply use the free block's prv alloc flag
    // This flag can be checked by the following condition:
    // GET_PREV_ALLOC(free_block) == PREV_BLOCK_ALLOCATED
    // To check whether the next block is allocated or not,
    // use the following condition:
    // GET_ALLOC(NEXT_BLOCK(free_block)) == THIS_BLOCK_ALLOCATED
    char prev_alloc = GET_PREV_ALLOC(free_block) == PREV_BLOCK_ALLOCATED;
    char next_alloc = GET_ALLOC(NEXT_BLOCK(free_block)) == THIS_BLOCK_ALLOCATED;
    // For case 1, simply return the free block without coalescing
    if(prev_alloc && next_alloc) return free_block;
    // For case 2, merge the free block with its next block
    // by updating the free block's header and the next block's footer
    // Make sure to delete pre-existing free blocks in the free lists array
    // and return the appropriate block!
    else if(prev_alloc && !next_alloc) {
        delete_block_free_list(NEXT_BLOCK(free_block));
        HEADER(free_block) = XOR_MAGIC(PACK(0, GET_BLOCK_SIZE(free_block) + GET_BLOCK_SIZE(NEXT_BLOCK(free_block)), GET_PREV_ALLOC(free_block)));
        FOOTER(free_block) = HEADER(free_block);
        return free_block;
    }
    // For case 3, merge the previous block with the free block
    // by updating the previous block's header and the free block's footer
    else if(!prev_alloc && next_alloc) {
        delete_block_free_list(PREV_BLOCK(free_block));
        HEADER(PREV_BLOCK(free_block)) = XOR_MAGIC(PACK(0, GET_BLOCK_SIZE(PREV_BLOCK(free_block)) + GET_BLOCK_SIZE(free_block), GET_PREV_ALLOC(PREV_BLOCK(free_block))));
        FOOTER(PREV_BLOCK(free_block)) = HEADER(PREV_BLOCK(free_block));
        return PREV_BLOCK(free_block);
    }
    // For case 4, merge all three blocks together by updating the
    // previous block's header and the next block's footer
    else {
        delete_block_free_list(PREV_BLOCK(free_block));
        delete_block_free_list(NEXT_BLOCK(free_block));
        HEADER(PREV_BLOCK(free_block)) = XOR_MAGIC(PACK(0, GET_BLOCK_SIZE(PREV_BLOCK(free_block)) + GET_BLOCK_SIZE(free_block) + GET_BLOCK_SIZE(NEXT_BLOCK(free_block)), GET_PREV_ALLOC(PREV_BLOCK(free_block))));
        FOOTER(PREV_BLOCK(free_block)) = HEADER(PREV_BLOCK(free_block));
        return PREV_BLOCK(free_block);
    }
}

void *serve_alloc_request(sf_size_t size, sf_size_t block_size) {
    // Check the quick lists array to see if there exists a block of the appropriate size
    // For each index from 0 to (NUM_QUICK_LISTS - 1):
    // sf_quick_lists[index] -> size: MIN_BLOCK_SIZE + index * ALIGN_SIZE
    for(int i = 0; i < NUM_QUICK_LISTS; ++i) {
        if((block_size == (MIN_BLOCK_SIZE + (i * ALIGN_SIZE))) && (sf_quick_lists[i].first != NULL)) {
            HEADER(sf_quick_lists[i].first) = XOR_MAGIC(PACK(size, block_size, THIS_BLOCK_ALLOCATED | GET_PREV_ALLOC(sf_quick_lists[i].first)));
            SET_PREV_ALLOC(NEXT_BLOCK(sf_quick_lists[i].first));
            return delete_block_quick_list(&sf_quick_lists[i].first);
        }
    }
    // If no blocks are available from the quick lists array of the appropriate size,
    // then search the free lists array for a sufficiently large block
    // Determine the index of the free list corresponding to the interval of the block size
    // Search that free list (from the start) until a large enough free block is found
    // If no such block exists, search the next free list until a large enough block is found
    // to satisfy the allocation request
    // When such a block is found, set its header appropriately
    // and set the prev alloc bit in the next block to be 1 (since this block is marked allocated)
    int first_index = get_free_list_index(block_size);
    for(int i = first_index; i < NUM_FREE_LISTS; ++i) {
        sf_block *block = search_free_list(sf_free_list_heads + i, block_size);
        if(block != NULL) {
            delete_block_free_list(block);
            // Split the block if splinter is at least MIN_BLOCK_SIZE (i.e., 32 bytes)
            sf_size_t splinter_size = GET_BLOCK_SIZE(block) - block_size;
            if(splinter_size >= MIN_BLOCK_SIZE) {
                HEADER(block) = XOR_MAGIC(PACK(size, block_size, THIS_BLOCK_ALLOCATED | GET_PREV_ALLOC(block)));
                HEADER(NEXT_BLOCK(block)) = XOR_MAGIC(PACK(0, splinter_size, PREV_BLOCK_ALLOCATED));
                FOOTER(NEXT_BLOCK(block)) = HEADER(NEXT_BLOCK(block));
                insert_block_free_list(sf_free_list_heads + get_free_list_index(splinter_size), NEXT_BLOCK(block));
            }
            else {
                HEADER(block) = XOR_MAGIC(PACK(size, GET_BLOCK_SIZE(block), THIS_BLOCK_ALLOCATED | GET_PREV_ALLOC(block)));
                SET_PREV_ALLOC(NEXT_BLOCK(block));
            }

            return block->body.payload;
        }
    }
    return NULL;
}

void init_lists() {
    // Initialize the quick lists array (sf_quick_lists)
    for(int i = 0; i < NUM_QUICK_LISTS; ++i) {
        sf_quick_lists[i].length = 0;
        sf_quick_lists[i].first = NULL;
    }
    // Initialize the free lists array (sf_free_list_heads)
    for(int i = 0; i < NUM_FREE_LISTS; ++i) {
        sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
        sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
    }
}

int extend_heap() {
    // Extends the heap by one memory page of size PAGE_SZ
    // If unsuccessful, sf_errno is set to ENOMEM, and a value of 0 is returned
    // Otherwise, it returns 1 (successful)
    // Note: The caller of this function should return NULL
    // immediately if it gets a return value of 0
    if(sf_mem_grow() == NULL) {
        sf_errno = ENOMEM;
        return 0;
    }
    return 1;
}

void *sf_malloc(sf_size_t size) {
    // TO BE IMPLEMENTED
    // If request size is 0, return NULL without setting sf_errno
    if(size == 0) return NULL;
    // This is the first allocation request if:
    // Start address of heap == end address of heap
    if(HEAP_START == HEAP_END) {
        // Extend the heap by one memory page (PAGE_SZ (1024) bytes)
        // Return NULL immediately if the operation was unsucessful
        if(!extend_heap()) return NULL;
        // Initialize the prologue and epilogue
        // The prologue has a size of MIN_BLOCK_SIZE (i.e., 32 bytes)
        // The epilogue has a size of ALIGN_SIZE (i.e., 16 bytes)
        // Note: 32 + 16 = 48 bytes < 1024 bytes (one memory page), so
        // the prologue and epilogue fit nicely into one memory page
        sf_block *prologue = PROLOGUE;
        HEADER(prologue) = XOR_MAGIC(PACK(0, sizeof(sf_block), THIS_BLOCK_ALLOCATED));
        sf_block *epilogue = EPILOGUE;
        HEADER(epilogue) = XOR_MAGIC(PACK(0, 0, THIS_BLOCK_ALLOCATED));
        // Initialize the quick and free lists arrays
        init_lists();
        // Remainder of first memory page has size:
        // PAGE_SZ - (size of EPILOGUE + size of PROLOGUE)
        // Or equivalently, address of the start of epilogue - address of end of prologue
        // In this case, we have 1024 - 48 = 976 bytes
        sf_block *remainder = NEXT_BLOCK(prologue);
        sf_size_t remainder_size = (char *)EPILOGUE - (char *)remainder;
        HEADER(remainder) = XOR_MAGIC(PACK(0, remainder_size, PREV_BLOCK_ALLOCATED));
        // Set the prev_footer field appropriately
        // This is the footer of the free block
        FOOTER(remainder) = HEADER(remainder);
        // Insert remainder of first memory page into appropriate free list
        insert_block_free_list(sf_free_list_heads + get_free_list_index(remainder_size), remainder);
    }
    // Block size = payload size + header size (in bytes)
    sf_size_t block_size = size + ROW_SIZE;
    // Add padding (if necessary) to the block size so that it is properly aligned
    align(&block_size);
    // Serve the allocation request using the quick and free lists
    void *payload = serve_alloc_request(size, block_size);
    // If the pointer to the payload is not null, then there exists an available block
    // to serve the allocation request; in this case, return the pointer to the payload
    // Otherwise, no block is available to satisfy the allocation request
    if(payload != NULL) return payload;
    // If no block is available to satisfy the allocation request,
    // call sf_mem_grow to extend the heap until the heap has enough
    // free memory to serve the allocation request
    // If sf_mem_grow returns NULL, set sf_errno to ENOMEM and return NULL immediately
    int num_of_extends = 0;
    sf_size_t supremum_size = 0;
    // If the previous block of epilogue is free, then set supremum_size to
    // the block size of the previous block of epilogue
    // Note that a free block has a footer that can be used to obtain
    // a pointer to it
    sf_block *block_start = EPILOGUE;
    if(GET_PREV_ALLOC(block_start) != PREV_BLOCK_ALLOCATED) {
        supremum_size = GET_BLOCK_SIZE(PREV_BLOCK(block_start));
    }
    while(supremum_size < block_size) {
        supremum_size += PAGE_SZ;
        ++num_of_extends;
    }
    for(int i = 0; i < num_of_extends; ++i) {
        if(!extend_heap()) return NULL;
        // If the block is free, make sure to delete the block
        // from its free list so we can add it back into
        // an appropriate free list after setting up a new
        // epilogue, setting up its footer as a free block,
        // and coalescing it with any immediately preceding free block
        delete_block_free_list(block_start);
        // Set up a new epilogue each time the heap is extended
        HEADER(EPILOGUE) = XOR_MAGIC(PACK(0, 0, THIS_BLOCK_ALLOCATED));
        HEADER(block_start) = XOR_MAGIC(PACK(0, (char *)EPILOGUE - (char *)block_start, GET_PREV_ALLOC(block_start)));
        // Set the prev_footer field appropriately
        // This is the footer of the free block
        FOOTER(block_start) = HEADER(block_start);
        // Coalesce the free block before inserting it into an appropriate free list
        block_start = coalesce(block_start);
        // Insert free block into appropriate free list
        insert_block_free_list(sf_free_list_heads + get_free_list_index(GET_BLOCK_SIZE(block_start)), block_start);
    }
    // Now we have enough memory to serve the allocation request
    // This means that it should return a non-NULL pointer to the payload
    // of the allocation request back to the client
    return serve_alloc_request(size, block_size);
}

void sf_free(void *pp) {
    // TO BE IMPLEMENTED
    abort();
}

void *sf_realloc(void *pp, sf_size_t rsize) {
    // TO BE IMPLEMENTED
    abort();
}

double sf_internal_fragmentation() {
    // TO BE IMPLEMENTED
    abort();
}

double sf_peak_utilization() {
    // TO BE IMPLEMENTED
    abort();
}
