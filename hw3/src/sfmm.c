/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdint.h>
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
#define MAX_BLOCK_SIZE (UINT32_MAX / ALIGN_SIZE)
#define MAX_PAYLOAD_SIZE (MAX_BLOCK_SIZE - ROW_SIZE)
#define HEADER(p) (((sf_block *)p)->header)
#define XOR_MAGIC(content) ((content) ^ (MAGIC))
#define PACK(payload_size, block_size, flags) (((uint64_t)payload_size << 32) | (block_size) | (flags))
#define GET_PAYLOAD_SIZE(p) (XOR_MAGIC(HEADER(p)) >> 32)
#define GET_BLOCK_SIZE(p) ((XOR_MAGIC(HEADER(p)) & 0xffffffff) & ~(0xf))
#define GET_ALLOC(p) (XOR_MAGIC(HEADER(p)) & THIS_BLOCK_ALLOCATED)
#define GET_PREV_ALLOC(p) (XOR_MAGIC(HEADER(p)) & PREV_BLOCK_ALLOCATED)
#define IN_QKLST(p) (XOR_MAGIC(HEADER(p)) & IN_QUICK_LIST)
#define SET_ALLOC(p) (HEADER(p) = XOR_MAGIC(XOR_MAGIC(HEADER(p)) | THIS_BLOCK_ALLOCATED))
#define SET_PREV_ALLOC(p) (HEADER(p) = XOR_MAGIC(XOR_MAGIC(HEADER(p)) | PREV_BLOCK_ALLOCATED))
#define UNSET_PREV_ALLOC(p) (HEADER(p) = XOR_MAGIC(XOR_MAGIC(HEADER(p)) & ~(PREV_BLOCK_ALLOCATED)))
#define SET_IN_QKLST(p) (HEADER(p) = XOR_MAGIC(XOR_MAGIC(HEADER(p)) | IN_QUICK_LIST))
#define FOOTER(p) (*((sf_footer *)((char *)p + GET_BLOCK_SIZE(p))))
#define PREV_FOOTER(p) (((sf_block *)p)->prev_footer)
#define GET_ALLOC_PREV_FOOTER(p) (XOR_MAGIC(PREV_FOOTER(p)) & THIS_BLOCK_ALLOCATED)
#define PROLOGUE ((sf_block *)HEAP_START)
#define EPILOGUE ((sf_block *)((char *)HEAP_END - ALIGN_SIZE))
#define NEXT_BLOCK(p) ((sf_block *)((char *)p + GET_BLOCK_SIZE(p)))
#define PREV_BLOCK(p) ((sf_block *)((char *)p - ((XOR_MAGIC(PREV_FOOTER(p)) & 0xffffffff) & ~(0xf))))

// Helper functions should be defined as static
static void insert_block_free_list(sf_block *, sf_block *);
static void delete_block_free_list(sf_block *);
static void insert_block_quick_list(sf_block **, sf_block **);
static void *delete_block_quick_list(sf_block **);
static int get_free_list_index(sf_size_t);
static sf_block *search_free_list(sf_block *, sf_size_t);
static sf_block *coalesce(sf_block *);
static void *serve_alloc_request(sf_size_t, sf_size_t);
static void init_lists();
static int extend_heap();
static int valid_pointer(void *);
static void flush_quick_list(sf_block **);
static double current_aggregate_payload();
static void set_current_max_aggregate_payload();

static double current_max_aggregate_payload = 0.0;

static void insert_block_free_list(sf_block *sentinel, sf_block *block) {
    sentinel->body.links.next->body.links.prev = block;
    block->body.links.next = sentinel->body.links.next;
    block->body.links.prev = sentinel;
    sentinel->body.links.next = block;
}

static void delete_block_free_list(sf_block *block) {
    if(GET_ALLOC(block) == THIS_BLOCK_ALLOCATED) return;
    block->body.links.prev->body.links.next = block->body.links.next;
    block->body.links.next->body.links.prev = block->body.links.prev;
}

static void insert_block_quick_list(sf_block **first, sf_block **block) {
    if(*first == NULL)
        (*block)->body.links.next = NULL;
    else
        (*block)->body.links.next = *first;
    *first = *block;
}

static void *delete_block_quick_list(sf_block **first) {
    void *return_block = (*first)->body.payload;
    *first = (*first)->body.links.next;
    return return_block;
}

static int get_free_list_index(sf_size_t size) {
    // If size == MIN_BLOCK_SIZE, return the first index (i.e, 0)
    // To find the appropriate free list, we can divide the
    // size of the block by MIN_BLOCK_SIZE (integer division)
    // If the result is equal to 1, then return 1
    // Then we can find the closest power of 2 that is smaller than the result
    // We need to return the index that corresponds to its interval
    // The index is i if 2^i is equal to the closest power of 2
    // 0:{M}, 1:(M, 2M], 2:(2M, 4M], 3:(4M, 8M], etc.
    if(size == MIN_BLOCK_SIZE) return 0;
    // Reaching this part means that size is greater than MIN_BLOCK_SIZE
    // since the callers of this function MUST pass in a size that is at least MIN_BLOCK_SIZE
    uint32_t result = size / MIN_BLOCK_SIZE;
    if(result == 1) return 1;
    uint64_t power = 1;
    int index = 0;
    while((result > power) && (index < (NUM_FREE_LISTS - 1))) {
        power *= 2;
        ++index;
    }
    return index;
}

static sf_block *search_free_list(sf_block *free_list_head, sf_size_t block_size) {
    sf_block *free_block = free_list_head->body.links.next;
    while(free_block != free_list_head) {
        if(block_size <= GET_BLOCK_SIZE(free_block))
            return free_block;
        free_block = free_block->body.links.next;
    }
    return NULL;
}

static void align(sf_size_t *block_size) {
    // Note: ALIGN_SIZE == sizeof(long double) == 16
    // If block size is not aligned to a 16-byte (double memory row) boundary,
    // then it must be rounded up to the next multiple of 16 (in bytes)
    // If block size is less than the minimum size of a block (i.e., 32 bytes),
    // then set block size to the minimum size of a block
    if(*block_size % ALIGN_SIZE != 0)
        *block_size = ALIGN_SIZE * ((*block_size / ALIGN_SIZE) + 1);
    if(*block_size < MIN_BLOCK_SIZE) *block_size = MIN_BLOCK_SIZE;
}

static sf_block *coalesce(sf_block *free_block) {
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
        UNSET_PREV_ALLOC(NEXT_BLOCK(free_block));
        return free_block;
    }
    // For case 3, merge the previous block with the free block
    // by updating the previous block's header and the free block's footer
    else if(!prev_alloc && next_alloc) {
        delete_block_free_list(PREV_BLOCK(free_block));
        HEADER(PREV_BLOCK(free_block)) = XOR_MAGIC(PACK(0, GET_BLOCK_SIZE(PREV_BLOCK(free_block)) + GET_BLOCK_SIZE(free_block), GET_PREV_ALLOC(PREV_BLOCK(free_block))));
        FOOTER(PREV_BLOCK(free_block)) = HEADER(PREV_BLOCK(free_block));
        UNSET_PREV_ALLOC(NEXT_BLOCK(PREV_BLOCK(free_block)));
        return PREV_BLOCK(free_block);
    }
    // For case 4, merge all three blocks together by updating the
    // previous block's header and the next block's footer
    else {
        delete_block_free_list(PREV_BLOCK(free_block));
        delete_block_free_list(NEXT_BLOCK(free_block));
        HEADER(PREV_BLOCK(free_block)) = XOR_MAGIC(PACK(0, GET_BLOCK_SIZE(PREV_BLOCK(free_block)) + GET_BLOCK_SIZE(free_block) + GET_BLOCK_SIZE(NEXT_BLOCK(free_block)), GET_PREV_ALLOC(PREV_BLOCK(free_block))));
        FOOTER(PREV_BLOCK(free_block)) = HEADER(PREV_BLOCK(free_block));
        UNSET_PREV_ALLOC(NEXT_BLOCK(PREV_BLOCK(free_block)));
        return PREV_BLOCK(free_block);
    }
}

static void *serve_alloc_request(sf_size_t size, sf_size_t block_size) {
    // Check the quick lists array to see if there exists a block of the appropriate size
    // For each index from 0 to (NUM_QUICK_LISTS - 1):
    // sf_quick_lists[index] -> block_size is equal to MIN_BLOCK_SIZE + index * ALIGN_SIZE
    // This implies that the index is equal to ((block_size - MIN_BLOCK_SIZE) / ALIGN_SIZE)
    // Proposition: The index is always a nonnegative integer
    // Proof:
    // We have that index == ((block_size - MIN_BLOCK_SIZE) / ALIGN_SIZE)
    // and MIN_BLOCK_SIZE == (ALIGN_SIZE * 2) <= block_size == (ALIGN_SIZE * k)
    // for some integer k
    // This means that index == (ALIGN_SIZE * k - ALIGN_SIZE * 2) / ALIGN_SIZE,
    // which equals index == ((ALIGN_SIZE * (k - 2)) / ALIGN_SIZE) == (k - 2)
    // Since MIN_BLOCK_SIZE <= block_size, we have that 2 <= k (because ALIGN_SIZE != 0)
    // or equivalently, 0 <= (k - 2)
    // Hence, index == (k - 2) >= 0
    sf_size_t index = ((block_size - MIN_BLOCK_SIZE) / ALIGN_SIZE);
    if((0 <= index) && (index < NUM_QUICK_LISTS)) {
        if(sf_quick_lists[index].first != NULL) {
            HEADER(sf_quick_lists[index].first) = XOR_MAGIC(PACK(size, block_size, THIS_BLOCK_ALLOCATED | GET_PREV_ALLOC(sf_quick_lists[index].first)));
            SET_PREV_ALLOC(NEXT_BLOCK(sf_quick_lists[index].first));
            return delete_block_quick_list(&sf_quick_lists[index].first);
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
                sf_block *splinter = NEXT_BLOCK(block);
                HEADER(splinter) = XOR_MAGIC(PACK(0, splinter_size, PREV_BLOCK_ALLOCATED));
                FOOTER(splinter) = HEADER(splinter);
                UNSET_PREV_ALLOC(NEXT_BLOCK(splinter));
                splinter = coalesce(splinter);
                insert_block_free_list(sf_free_list_heads + get_free_list_index(GET_BLOCK_SIZE(splinter)), splinter);
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

static void init_lists() {
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

static int extend_heap() {
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

static int valid_pointer(void *pp) {
    // The pointer pp is considered invalid if at least one of the following holds:
    // (1) pp is NULL
    // (2) pp is not aligned on an ALIGN_SIZE (i.e., 16 byte) boundary
    // (3) The content of the block (sf_block *)((char *)pp - ALIGN_SIZE)
    //     satisfies at least one of the following conditions:
    //     1. GET_BLOCK_SIZE(block) < MIN_BLOCK_SIZE
    //     2. (GET_BLOCK_SIZE(block) % ALIGN_SIZE) != 0
    //     3. &HEADER(block) < &(HEADER(NEXT_BLOCK(PROLOGUE)))
    //     4. &FOOTER(block) > &(EPILOGUE->prev_footer)
    //     5. (GET_ALLOC(block) != THIS_BLOCK_ALLOCATED) || (IN_QKLST(block) == IN_QUICK_LIST)
    //     6a. (GET_PREV_ALLOC(block) != PREV_BLOCK_ALLOCATED) && (GET_ALLOC_PREV_FOOTER(block) == THIS_BLOCK_ALLOCATED)
    //     6b. (GET_PREV_ALLOC(block) != PREV_BLOCK_ALLOCATED) && (GET_ALLOC_PREV_FOOTER(block) != THIS_BLOCK_ALLOCATED) && (XOR_MAGIC(HEADER((PREV_BLOCK(block)))) != XOR_MAGIC(PREV_FOOTER(block)))
    // This function returns 1 if the pointer pp is valid,
    // and 0 if the pointer pp is invalid
    if(pp == NULL) return 0;
    if(((uintptr_t)pp % ALIGN_SIZE) != 0) return 0;
    sf_block *block = (sf_block *)((char *)pp - ALIGN_SIZE);
    if(GET_BLOCK_SIZE(block) < MIN_BLOCK_SIZE) return 0;
    if((GET_BLOCK_SIZE(block) % ALIGN_SIZE) != 0) return 0;
    if(&HEADER(block) < &(HEADER(NEXT_BLOCK(PROLOGUE)))) return 0;
    if(&FOOTER(block) > &(EPILOGUE->prev_footer)) return 0;
    // The block cannot be in a free list or quick list!
    // If the block is free (i.e., its alloc bit is not set)
    // OR the block is in a quick list (i.e., its in qklst bit is set),
    // then the pointer is invalid
    if((GET_ALLOC(block) != THIS_BLOCK_ALLOCATED) || (IN_QKLST(block) == IN_QUICK_LIST)) return 0;
    if((GET_PREV_ALLOC(block) != PREV_BLOCK_ALLOCATED) && (GET_ALLOC_PREV_FOOTER(block) == THIS_BLOCK_ALLOCATED)) return 0;
    if((GET_PREV_ALLOC(block) != PREV_BLOCK_ALLOCATED) && (GET_ALLOC_PREV_FOOTER(block) != THIS_BLOCK_ALLOCATED) && (XOR_MAGIC(HEADER((PREV_BLOCK(block)))) != XOR_MAGIC(PREV_FOOTER(block)))) return 0;
    return 1;
}

static void flush_quick_list(sf_block **first) {
    sf_block *block = *first;
    sf_block *next_block;
    while(block != NULL) {
        next_block = block->body.links.next;
        HEADER(block) = XOR_MAGIC(PACK(0, GET_BLOCK_SIZE(block), GET_PREV_ALLOC(block)));
        FOOTER(block) = HEADER(block);
        UNSET_PREV_ALLOC(NEXT_BLOCK(block));
        block = coalesce(block);
        insert_block_free_list(sf_free_list_heads + get_free_list_index(GET_BLOCK_SIZE(block)), block);
        block = next_block;
    }
    // Delete the entire quick list
    *first = NULL;
}

static double current_aggregate_payload() {
    if(HEAP_START == HEAP_END) return 0.0;
    sf_block *block = NEXT_BLOCK(PROLOGUE);
    double aggregate_payload = 0.0;
    while(block != EPILOGUE) {
        // Count only allocated blocks with payloads
        // Blocks in quick lists and free lists are not counted
        // The prologue and epilogue are not counted
        if((GET_ALLOC(block) == THIS_BLOCK_ALLOCATED) && (IN_QKLST(block) != IN_QUICK_LIST)) {
            aggregate_payload += GET_PAYLOAD_SIZE(block);
        }
        block = NEXT_BLOCK(block);
    }
    return aggregate_payload;
}

static void set_current_max_aggregate_payload() {
    double value = current_aggregate_payload();
    current_max_aggregate_payload = ((value > current_max_aggregate_payload) ? value : current_max_aggregate_payload);
}

void *sf_malloc(sf_size_t size) {
    // TO BE IMPLEMENTED
    // If request size is 0, return NULL without setting sf_errno
    if(size == 0) return NULL;
    // If request size exceeds MAX_PAYLOAD_SIZE, set sf_errno
    // to EINVAL and return NULL
    if(size > MAX_PAYLOAD_SIZE) {
        sf_errno = EINVAL;
        return NULL;
    }
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
        // Or equivalently, address of the start of epilogue - address of the end of prologue
        // In this case, we have 1024 - 48 = 976 bytes
        sf_block *remainder = NEXT_BLOCK(prologue);
        sf_size_t remainder_size = (char *)EPILOGUE - (char *)remainder;
        HEADER(remainder) = XOR_MAGIC(PACK(0, remainder_size, PREV_BLOCK_ALLOCATED));
        // Set the prev_footer field appropriately
        // This is the footer of the free block
        FOOTER(remainder) = HEADER(remainder);
        UNSET_PREV_ALLOC(NEXT_BLOCK(remainder));
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
    if(payload != NULL) {
        // Upon serving the allocation request, we need to
        // recalculate the aggregate payload of the heap
        // and update the current_max_aggregate_payload variable
        // accordingly
        set_current_max_aggregate_payload();
        return payload;
    }
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
        UNSET_PREV_ALLOC(NEXT_BLOCK(block_start));
        // Coalesce the free block before inserting it into an appropriate free list
        block_start = coalesce(block_start);
        // Insert free block into appropriate free list
        insert_block_free_list(sf_free_list_heads + get_free_list_index(GET_BLOCK_SIZE(block_start)), block_start);
    }
    // Now we have enough memory to serve the allocation request
    // This means that it should return a non-NULL pointer to the payload
    // of the allocation request back to the client
    // Upon serving the allocation request, we need to
    // recalculate the aggregate payload of the heap
    // and update the current_max_aggregate_payload variable
    // accordingly
    // Make sure to serve the allocation request BEFORE
    // recalculating the aggregate payload of the heap and
    // updating the current_max_aggregate_payload variable!
    payload = serve_alloc_request(size, block_size);
    // Here, the payload variable should always return a
    // non-NULL pointer, but it doesn't hurt to defensively check
    // this before setting the current_max_aggregate_payload variable
    if(payload != NULL)
        set_current_max_aggregate_payload();
    return payload;
}

void sf_free(void *pp) {
    // TO BE IMPLEMENTED
    // Check if the pointer pp is valid
    // Call abort() if pp is invalid
    if(!valid_pointer(pp)) abort();
    // The passed-in pointer pp (if valid) is a pointer to
    // the payload of some block in the heap
    // To access the actual block, use the following:
    // (sf_block *)((char *)pp - ALIGN_SIZE)
    sf_block *block = (sf_block *)((char *)pp - ALIGN_SIZE);
    sf_size_t block_size = GET_BLOCK_SIZE(block);
    // Free the block!
    // Strategy: If for some index (where 0 <= index < NUM_QUICK_LISTS):
    // block_size == (MIN_BLOCK_SIZE + (index * ALIGN_SIZE) holds,
    // then insert the block into sf_quick_lists[index].first with an
    // appropriate quick list header and increment the length field accordingly
    // If sf_quick_lists[index].length == QUICK_LIST_MAX, then
    // flush the quick list before inserting the block into it
    sf_size_t index = ((block_size - MIN_BLOCK_SIZE) / ALIGN_SIZE);
    if((0 <= index) && (index < NUM_QUICK_LISTS)) {
        if(sf_quick_lists[index].length == QUICK_LIST_MAX) {
            flush_quick_list(&sf_quick_lists[index].first);
            sf_quick_lists[index].length = 0;
        }
        HEADER(block) = XOR_MAGIC(PACK(0, block_size, THIS_BLOCK_ALLOCATED | GET_PREV_ALLOC(block) | IN_QUICK_LIST));
        insert_block_quick_list(&sf_quick_lists[index].first, &block);
        ++sf_quick_lists[index].length;
        SET_PREV_ALLOC(NEXT_BLOCK(block));
        return;
    }
    // If no such quick list exists, then coalesce the block (if possible)
    // and insert the resulting block into its appropriate free list
    // with the appropriate free list header (and footer)
    // Make sure to unset the prev alloc bit for the immediately proceeding block
    HEADER(block) = XOR_MAGIC(PACK(0, block_size, GET_PREV_ALLOC(block)));
    FOOTER(block) = HEADER(block);
    UNSET_PREV_ALLOC(NEXT_BLOCK(block));
    block = coalesce(block);
    insert_block_free_list(sf_free_list_heads + get_free_list_index(GET_BLOCK_SIZE(block)), block);
}

void *sf_realloc(void *pp, sf_size_t rsize) {
    // TO BE IMPLEMENTED
    if(!valid_pointer(pp)) {
        sf_errno = EINVAL;
        return NULL;
    }
    if(rsize == 0) {
        sf_free(pp);
        return NULL;
    }
    sf_block *block = (sf_block *)((char *)pp - ALIGN_SIZE);
    sf_size_t payload_size = GET_PAYLOAD_SIZE(block);
    if(rsize == payload_size) return pp;
    else if(rsize > payload_size) {
        void *new_payload = sf_malloc(rsize);
        if(new_payload == NULL) return NULL;
        memcpy(new_payload, pp, payload_size);
        sf_free(pp);
        return new_payload;
    }
    else {
        sf_size_t total_size = GET_BLOCK_SIZE(block);
        sf_size_t new_size = rsize + ROW_SIZE;
        align(&new_size);
        // Split the block if splinter is at least MIN_BLOCK_SIZE (i.e., 32 bytes)
        sf_size_t splinter_size = total_size - new_size;
        if(splinter_size >= MIN_BLOCK_SIZE) {
            HEADER(block) = XOR_MAGIC(PACK(rsize, new_size, THIS_BLOCK_ALLOCATED | GET_PREV_ALLOC(block)));
            sf_block *splinter = NEXT_BLOCK(block);
            HEADER(splinter) = XOR_MAGIC(PACK(0, splinter_size, PREV_BLOCK_ALLOCATED));
            FOOTER(splinter) = HEADER(splinter);
            UNSET_PREV_ALLOC(NEXT_BLOCK(splinter));
            splinter = coalesce(splinter);
            insert_block_free_list(sf_free_list_heads + get_free_list_index(GET_BLOCK_SIZE(splinter)), splinter);
        }
        else {
            HEADER(block) = XOR_MAGIC(PACK(rsize, total_size, THIS_BLOCK_ALLOCATED | GET_PREV_ALLOC(block)));
            SET_PREV_ALLOC(NEXT_BLOCK(block));
        }
        return pp;
    }
}

double sf_internal_fragmentation() {
    // TO BE IMPLEMENTED
    // The current amount of internal fragmentation
    // in the heap is defined to be:
    // Total payload size / total block size (for allocated blocks)
    // If the heap is not initialized, then return 0.0
    if(HEAP_START == HEAP_END) return 0.0;
    double total_payload_size = 0.0, total_block_size = 0.0;
    sf_block *block = NEXT_BLOCK(PROLOGUE);
    while(block != EPILOGUE) {
        // Count only allocated blocks with payloads
        // Blocks in quick lists and free lists are not counted
        // The prologue and epilogue are not counted
        if((GET_ALLOC(block) == THIS_BLOCK_ALLOCATED) && (IN_QKLST(block) != IN_QUICK_LIST)) {
            total_payload_size += GET_PAYLOAD_SIZE(block);
            total_block_size += GET_BLOCK_SIZE(block);
        }
        block = NEXT_BLOCK(block);
    }
    if((total_payload_size == 0.0) || (total_block_size == 0.0))
        return 0.0;
    return (total_payload_size / total_block_size);
}

double sf_peak_utilization() {
    // TO BE IMPLEMENTED
    // The peak memory utilization is defined to be:
    // Current maximum aggregate payload / current heap size
    // If the heap is not initialized, then return 0.0
    if(HEAP_START == HEAP_END) return 0.0;
    // The current maximum aggregate payload is stored
    // in the static variable current_max_aggregate_payload
    // and is updated every time the aggregate payload is increased
    // The aggregate payload can only be increased by a call
    // to sf_malloc() and sf_realloc()
    // For sf_realloc(), the aggregate payload can only increase if
    // the new payload size given by the client is greater than the
    // current payload size of the block (if the new payload size is less than
    // or equal to the current payload size, then the current maximum aggregate
    // payload remains the same)
    // Since a call to sf_malloc() by sf_realloc() is made for this case,
    // setting the current_max_aggregate_payload variable is already handled
    // by sf_malloc(); this means that sf_realloc() will never need to
    // explicitly set this variable (and so can be left unchanged)
    // A call to sf_free() will either decrease the aggregate payload
    // or leave it unchanged, so there is no need to recalculate
    // the aggregate payload and update the variable in sf_free()
    // Current heap size is (HEAP_END - HEAP_START)
    return (current_max_aggregate_payload / (double)(HEAP_END - HEAP_START));
}
