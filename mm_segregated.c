#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

team_t team = {
    "ateam",
    "Harry Bovik",
    "bovik@cs.cmu.edu",
    "",
    ""
};

#define ALIGNMENT           8
#define WSIZE               4
#define DSIZE               8
#define CHUNKSIZE           (1<<12)
#define GROUPSIZE           20

#define MAX(x, y)           ((x) > (y) ? (x) : (y))
#define PACK(size, alloc)   ((size) | (alloc))
#define GET(p)              (*(unsigned int *)(p))
#define PUT(p, val)         (*(unsigned int *)(p) = (val))
#define GET_SIZE(p)         (GET(p) & ~0x7)
#define GET_ALLOC(p)        (GET(p) & 0x1)
#define HDRP(bp)            ((char *)(bp) - WSIZE)
#define FTRP(bp)            ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp)       ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)       ((char *)(bp) - GET_SIZE((char *)bp - DSIZE))
#define PREV_FREE(bp)       (*(char **)(bp))
#define NEXT_FREE(bp)       (*(char **)(bp + WSIZE))
#define ALIGN(size)         (((size) + (ALIGNMENT-1)) & ~0x7)

static char *group_free_listp[GROUPSIZE];

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void add_block(void *bp, size_t size);
static void remove_block(void *bp);

int mm_init(void){
    int group;
    for(group = 0; group < GROUPSIZE; group ++){
        group_free_listp[group] = NULL;
    }

    char * heap_listp;
    if((heap_listp = mem_sbrk(WSIZE*4)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0);    
    PUT(heap_listp + WSIZE,     PACK(DSIZE, 1)); 
    PUT(heap_listp + 2*WSIZE,   PACK(DSIZE, 1));
    PUT(heap_listp + 3*WSIZE,   PACK(0, 1));  
    
    heap_listp += 2*WSIZE;
    
    if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

static void *extend_heap(size_t words){
    char *bp;
    size_t size;

    size = (words & 1) ? (words+1) * WSIZE : words * WSIZE; // 다른점? 
    
    if((size_t)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    PUT(HDRP(bp), PACK(size, 0));           /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));           /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));   /* New epilogue header */
    
    add_block(bp, size);
    // printf("Extend_heap! initial heap size : %d\n", size);
    return coalesce(bp);
}


static void add_block(void *bp, size_t size){
    int group_idx = 0;
    int original_size = size;
    while((group_idx < GROUPSIZE - 1) && (size > 1)){
        size >>= 1;
        group_idx++;
    }

    void *curr = group_free_listp[group_idx];
    void *prev = NULL;

    while(curr != NULL && size > GET_SIZE(HDRP(curr))){
        prev = curr;
        curr = NEXT_FREE(curr);
    }

    if(curr == NULL){
        // case 1. 그룹이 비어있는 경우 
        if(prev == NULL){
            NEXT_FREE(bp) = NULL;
            PREV_FREE(bp) = NULL;
            group_free_listp[group_idx] = bp;
        }
        // case 2. 새로운 블록이 맨 끝에 들어가야 하는 경우 
        else {
            NEXT_FREE(prev) = bp;
            PREV_FREE(bp) = prev;
            NEXT_FREE(bp) = NULL;
        }
    }
    else{
        // case 3. 새로운 블록이 맨 앞 루트가 되는 경우 
        if(prev == NULL){
            NEXT_FREE(bp) = curr;
            PREV_FREE(bp) = NULL;
            PREV_FREE(curr) = bp;
            group_free_listp[group_idx] = bp;
        }
        // case 4. 새로운 블록이 두 블록 중간에 들어가는 경우 
        else {
            NEXT_FREE(bp) = curr;
            PREV_FREE(bp) = prev;
            NEXT_FREE(prev) = bp;
            PREV_FREE(curr) = bp;
        }
    }
    // printf("[add size:%d]GROUP: %d PREV: %p BP: %p, NEXT: %p\n", original_size, group_idx, PREV_FREE(bp), bp, NEXT_FREE(bp));
    // curr = group_free_listp[group_idx];
    // while(curr != NULL){
    //     printf("size:%d(%p) -> ", GET_SIZE(HDRP(curr)), curr);
    //     prev = curr;
    //     curr = NEXT_FREE(curr);
    // }
    // printf("\n");
}


static void remove_block(void *bp){
    int group_idx = 0;
    size_t size = GET_SIZE(HDRP(bp));
    
    while ((group_idx < GROUPSIZE - 1) && (size > 1)) {
        size >>= 1;
        group_idx++;
    }
    
    if(NEXT_FREE(bp) == NULL){
        // case 1. 빼면 아무것도 안남는 경우 
        if(PREV_FREE(bp) == NULL){
            group_free_listp[group_idx] = NULL;
        }
        // case 2. 마지막 블록을 빼는 경우 
        else{
            NEXT_FREE(PREV_FREE(bp)) = NULL;
        }
    }
    else{
        // case 3. 맨 앞 블록을 빼는 경우 
        if(PREV_FREE(bp) == NULL){
            PREV_FREE(NEXT_FREE(bp)) = NULL;
            group_free_listp[group_idx] = NEXT_FREE(bp);
        }
        // case 4. 앞 뒤 모두 있는 중간 블록을 빼는 경우 
        else{
            PREV_FREE(NEXT_FREE(bp)) = PREV_FREE(bp);
            NEXT_FREE(PREV_FREE(bp)) = NEXT_FREE(bp);
        }
    }
    // printf("[rmv]group: %d PREV: %p BP: %p, NEXT: %p\n", group_idx, PREV_FREE(bp), bp, NEXT_FREE(bp));
}


static void *coalesce(void * bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // case 1
    if (prev_alloc && next_alloc){
        return bp;
    }
    //case 2
    else if(prev_alloc && !next_alloc){
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        remove_block(bp);
        remove_block(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    //case 3
    else if (!prev_alloc && next_alloc){
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        remove_block(bp);
        remove_block(PREV_BLKP(bp));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    } 

    //case 4
    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        remove_block(bp);
        remove_block(NEXT_BLKP(bp));
        remove_block(PREV_BLKP(bp));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    
    add_block(bp, size);
    return bp;
}


void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    add_block(bp, size);
    coalesce(bp);
}


// void *mm_realloc(void *ptr, size_t size)
// {
//     void *oldptr = ptr;
//     void *newptr;
//     size_t copySize;
    
//     newptr = mm_malloc(size);
//     if (newptr == NULL)
//         return NULL;
//     // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
//     copySize = GET_SIZE(HDRP(oldptr));
//     if (size < copySize)
//         copySize = size;
//     memcpy(newptr, oldptr, copySize);
//     mm_free(oldptr);
//     return newptr;
// }


void *mm_realloc(void *ptr, size_t size)
{
    if(!size){
        mm_free(ptr);
        return ptr;
    }
    if(ptr == NULL){
        return mm_malloc(size);
    }
    else{
        size_t new_size = ALIGN(size + DSIZE); // Adjusted block size
        size_t block_size = GET_SIZE(HDRP(ptr));
        if (new_size <= block_size){
            place(ptr, new_size);
            return ptr;
        }
        else{
            void * new_ptr = mm_malloc(size);
            if (new_ptr == NULL)
                return NULL;
            memcpy(new_ptr, ptr, size);
            mm_free(ptr);
            return new_ptr;
        }
    }    
}



void *mm_malloc(size_t size)
{
    size_t newsize = ALIGN(size + DSIZE); 
    size_t extendsize = MAX(newsize, CHUNKSIZE);
    char * bp;

    if(!size){
        return NULL;
    }

    if((bp = find_fit(newsize)) != NULL){
        // printf("[find size:%d]PREV: %p BP: %p, NEXT: %p\n", GET_SIZE(HDRP(bp)), PREV_FREE(bp), bp, NEXT_FREE(bp));
        place(bp, newsize);
        return bp;
    }

    if((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, newsize);
    return bp; 
    
}


static void place(void *bp, size_t asize)
{
    size_t block_size = GET_SIZE(HDRP(bp));

    remove_block(bp);
    /* case: block should be splitted */
    if(block_size - asize >= 2*DSIZE){
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(block_size-asize, 0));
        PUT(FTRP(bp), PACK(block_size-asize, 0));
        add_block(bp, block_size-asize);
    }
    /* case: block is not splitted */
    else{
        PUT(HDRP(bp), PACK(block_size, 1));
        PUT(FTRP(bp), PACK(block_size, 1));
    }
}


static void *find_fit(size_t asize){
    int group_idx = 0;
    int size = asize;
    void * curr;
    while((group_idx < GROUPSIZE - 1) && (size > 1)){
        size >>= 1;
        group_idx++;
    }
    // printf("[in find]start point: %p, size: %d\n", curr, asize);
    for (;group_idx < GROUPSIZE; group_idx++){
        curr = group_free_listp[group_idx];
        while(curr != NULL && asize > GET_SIZE(HDRP(curr))){
            curr = NEXT_FREE(curr);
        // printf("[find size:%d]GROUP: %d PREV: %p BP: %p, NEXT: %p\n", GET_SIZE(HDRP(curr)), group_idx, PREV_FREE(bp), bp, NEXT_FREE(bp));
        }
        if(curr){
            return curr;           
        }
    }

    return NULL;
}