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
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define PACK(payload_size, block_size, flags) ((((uint64_t)payload_size << 32) | (block_size)) | (flags))
#define GET_PAYLOAD_SIZE(p) (HEADER(p) >> 32)
#define GET_BLOCK_SIZE(p) ((HEADER(p) & 0xffffffff) & ~(0xf))
#define GET_ALLOC(p) (HEADER(p) & THIS_BLOCK_ALLOCATED)
#define GET_PREV_ALLOC(p) (HEADER(p) & PREV_BLOCK_ALLOCATED)
#define IN_QKLST(p) (HEADER(p) & IN_QUICK_LIST)
#define SET_ALLOC(p) (HEADER(p) |= THIS_BLOCK_ALLOCATED)
#define SET_PREV_ALLOC(p) (HEADER(p) |= PREV_BLOCK_ALLOCATED)
#define SET_IN_QKLST(p) (HEADER(p) |= IN_QUICK_LIST)
#define PROLOGUE ((sf_block *)HEAP_START)
#define EPILOGUE ((sf_block *)((char *)HEAP_END - ALIGN_SIZE))
#define NEXT_BLOCK(p) ((sf_block *)((char *)p + GET_BLOCK_SIZE(p)))
#define PREV_BLOCK(p) ((sf_block *)((char *)p - ((((sf_block *)p)->prev_footer & 0xffffffff) & ~(0xf))))

void insert_block_free_list(sf_block *, sf_block *);
void delete_block_free_list(sf_block *);
void insert_block_quick_list(sf_block **, sf_block **);
void *delete_block_quick_list(sf_block **);
int get_free_list_index(sf_size_t);
sf_block *search_free_list(sf_block *, sf_size_t);

void insert_block_free_list(sf_block *sentinel, sf_block *block) {
    if(sentinel == NULL || block == NULL) return;
    sentinel->body.links.next->body.links.prev = block;
    block->body.links.next = sentinel->body.links.next;
    block->body.links.prev = sentinel;
    sentinel->body.links.next = block;
}

void delete_block_free_list(sf_block *block) {
    if(block == NULL) return;
    block->body.links.prev->body.links.next = block->body.links.next;
    block->body.links.next->body.links.prev = block->body.links.prev;
}

void insert_block_quick_list(sf_block **first, sf_block **block) {
    if(first == NULL || block == NULL) return;
    if(*first != NULL) (*block)->body.links.next = *first;
    *first = *block;
}

void *delete_block_quick_list(sf_block **first) {
    if(first == NULL || *first == NULL) return NULL;
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
    if(free_list_head == NULL) return NULL;
    sf_block *free_block = free_list_head->body.links.next;
    while(free_block != free_list_head) {
        if(block_size <= GET_BLOCK_SIZE(free_block))
            return free_block;
        free_block = free_block->body.links.next;
    }
    return NULL;
}

void align(sf_size_t *block_size) {
    if(block_size == NULL) return;
    if(*block_size % ALIGN_SIZE != 0)
        *block_size = ALIGN_SIZE * ((*block_size / ALIGN_SIZE) + 1);
    if(*block_size < MIN_BLOCK_SIZE) *block_size = MIN_BLOCK_SIZE;
}

void *sf_malloc(sf_size_t size) {
    // TO BE IMPLEMENTED
    // To be removed!
    sf_set_magic(0x0);
    // If request size is 0, return NULL without setting sf_errno
    if(size == 0) return NULL;
    // This is the first allocation request if:
    // Start address of heap == end address of heap
    if(HEAP_START == HEAP_END) {
        // Extend the heap by one memory page (1024 bytes)
        if(sf_mem_grow() == NULL) {
            sf_errno = ENOMEM;
            return NULL;
        }
        // Initialize the prologue and epilogue
        // The prologue has a size of MIN_BLOCK_SIZE (i.e., 32 bytes)
        // The epilogue has a size of ALIGN_SIZE (i.e., 16 bytes)
        // Note: 32 + 16 = 48 bytes < 1024 bytes (one memory page), so
        // the prologue and epilogue fit nicely into one memory page
        sf_block *prologue = PROLOGUE;
        HEADER(prologue) = PACK(0, sizeof(sf_block), THIS_BLOCK_ALLOCATED);
        sf_block *epilogue = EPILOGUE;
        HEADER(epilogue) = PACK(0, 0, THIS_BLOCK_ALLOCATED);
        // Initialize the free lists array (sf_free_list_heads)
        for(int i = 0; i < NUM_FREE_LISTS; ++i) {
            sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
            sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
        }
        // Remainder of first memory page has size 1024 - 48 = 976 bytes
        sf_block *remainder = NEXT_BLOCK(prologue);
        sf_size_t remainder_size = (char *)EPILOGUE - (char *)remainder;
        HEADER(remainder) = PACK(0, remainder_size, PREV_BLOCK_ALLOCATED);
        // Set the prev_footer field appropriately
        // This is the footer of the free block
        FOOTER(remainder) = HEADER(remainder);
        // Insert remainder of first memory page into appropriate free list
        insert_block_free_list(sf_free_list_heads + get_free_list_index(remainder_size), remainder);
    }
    // Block size = header size + payload size (in bytes)
    sf_size_t block_size = size + ROW_SIZE;
    // Note: ALIGN_SIZE == sizeof(long double) == 16
    // If block size is not aligned to a 16-byte (double memory row) boundary,
    // then it must be rounded up to the next multiple of 16 (in bytes)
    // If block size is less than the minimum size of a block (i.e., 32 bytes),
    // then set block size to 32 bytes
    align(&block_size);
    // Check the quick lists array to see if there exists a block of the appropriate size
    // For each index from 0 to (NUM_QUICK_LISTS - 1):
    // sf_quick_lists[index] -> size: MIN_BLOCK_SIZE + index * ALIGN_SIZE
    for(int i = 0; i < NUM_QUICK_LISTS; ++i) {
        if((block_size == (MIN_BLOCK_SIZE + (i * ALIGN_SIZE))) && (sf_quick_lists[i].first != NULL)) {
            HEADER(sf_quick_lists[i].first) = PACK(size, block_size, THIS_BLOCK_ALLOCATED | GET_PREV_ALLOC(sf_quick_lists[i].first));
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
                HEADER(block) = PACK(size, block_size, THIS_BLOCK_ALLOCATED | GET_PREV_ALLOC(block));
                HEADER(NEXT_BLOCK(block)) = PACK(0, splinter_size, PREV_BLOCK_ALLOCATED);
                FOOTER(NEXT_BLOCK(block)) = HEADER(NEXT_BLOCK(block));
                insert_block_free_list(sf_free_list_heads + get_free_list_index(splinter_size), NEXT_BLOCK(block));
            }
            else {
                HEADER(block) = PACK(size, GET_BLOCK_SIZE(block), THIS_BLOCK_ALLOCATED | GET_PREV_ALLOC(block));
                SET_PREV_ALLOC(NEXT_BLOCK(block));
            }

            return block->body.payload;
        }
    }
    // If no block is available to satisfy the allocation request,
    // call sf_mem_grow to extend the heap until the heap has enough
    // free memory to serve the allocation request
    // If sf_mem_grow returns NULL, set sf_errno to ENOMEM and return NULL immediately
    // Coalesce each page with any free block that immediately preceeds it
    void *new_page = sf_mem_grow();
    if(new_page == NULL) {
        sf_errno = ENOMEM;
        return NULL;
    }
    // Set up new epilogue
    /*HEADER(EPILOGUE) = PACK(0, 0, THIS_BLOCK_ALLOCATED);
    sf_size_t remainder_size = (char *)EPILOGUE - (char *)new_page;
    HEADER(new_page) = PACK(0, remainder_size, GET_PREV_ALLOC(PREV_BLOCK(new_page)));
    // Set the prev_footer field appropriately
    // This is the footer of the free block
    FOOTER(new_page) = HEADER(new_page);
    // Insert remainder of first memory page into appropriate free list
    insert_block_free_list(sf_free_list_heads + get_free_list_index(remainder_size), new_page);
    */
    return NULL;
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
