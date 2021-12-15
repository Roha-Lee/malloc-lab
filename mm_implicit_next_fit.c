/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* Basic constants and macros*/
#define WSIZE 4
#define DSIZE 8

#define CHUNKSIZE (1<<12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)          (*(unsigned int *)(p))
#define PUT(p, val)     (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)     (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)        ((char *)(bp) - WSIZE)
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)        ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)        ((char *)(bp) - GET_SIZE((char *)bp - DSIZE))

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

/* user defined macro & constant */

/* private variables */
static char * heap_listp;
static char * last_bp;  

/* function prototypes */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit_next(size_t asize);
static void place(void *bp, size_t asize);
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if((heap_listp = (char*)mem_sbrk(WSIZE<<2)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);     /* Alignment padding */
    PUT(heap_listp + WSIZE, PACK(DSIZE, 1));    /* Prologue Header */ 
    PUT(heap_listp + 2*WSIZE, PACK(DSIZE, 1));  /* Prologue Footer */ 
    PUT(heap_listp + 3*WSIZE, PACK(0, 1));  /* Epilogue Header */ 
    heap_listp += 2*WSIZE;
    last_bp = heap_listp;
    
    if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}
/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t newsize = ALIGN(size + DSIZE); 
    size_t extendsize = MAX(newsize, CHUNKSIZE);
    char * bp;

    if(!size){
        return NULL;
    }
    
    if((bp = find_fit_next(newsize)) != NULL){
        place(bp, newsize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    if((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, newsize);
    return bp; 
}
/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}
/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size){
    if(size <= 0){ //equivalent to mm_free(ptr).
        mm_free(ptr);
        return 0;
    }
    if(ptr == NULL){
        return mm_malloc(size); //equivalent to mm_malloc(size).
    }
    void *new_ptr = mm_malloc(size); //new pointer.
    if(new_ptr == NULL){
        return 0;
    }
    size_t oldsize = GET_SIZE(HDRP(ptr));
    if(size < oldsize){
    	oldsize = size; //shrink size.
	}
    memcpy(new_ptr, ptr, oldsize); //cover.
    mm_free(ptr);
    return new_ptr;
}

// void *mm_realloc(void *ptr, size_t size)
// {
//     if(!size){
//         mm_free(ptr);
//         return ptr;
//     }
//     if(ptr == NULL){
//         return mm_malloc(size);
//     }
//     else{
//         size_t new_size = ALIGN(size + DSIZE); // Adjusted block size
//         size_t block_size = GET_SIZE(HDRP(ptr));
//         if (new_size <= block_size){
//             place(ptr, new_size);
//             return ptr;
//         }
//         else{
//             void * new_ptr = mm_malloc(size);
//             if (new_ptr == NULL)
//                 return NULL;
//             memcpy(new_ptr, ptr, size);
//             mm_free(ptr);
//             return new_ptr;
//         }
//     }    
// }

/*
 * extend_heap - extend heap by words 
 */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words & 1) ? (words+1) * WSIZE : words * WSIZE;
    if((size_t)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));           /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));           /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));   /* New epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
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
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    //case 3
    else if (!prev_alloc && next_alloc){
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    } 

    //case 4
    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    last_bp = PREV_BLKP(bp);
    return bp;
}


static void *find_fit_next(size_t asize)
{
    char *bp;

    for (bp = NEXT_BLKP(last_bp); GET_SIZE(HDRP(bp)) != 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize) {
            last_bp = bp;
            return bp;
        }
    }
    
    // for better memory utils
    bp = mem_heap_lo() + (WSIZE<<1);
    for(bp = NEXT_BLKP(bp); bp < last_bp; bp=NEXT_BLKP(bp)){
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize) {
            last_bp = bp;
            return bp;
        }
    }
    return NULL;
}


static void place(void *bp, size_t asize)
{
    size_t block_size = GET_SIZE(HDRP(bp));
    
    /* case: block should be splitted */
    if(block_size - asize >= 2*DSIZE){
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(block_size-asize, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(block_size-asize, 0));
    }
    /* case: block is not splitted */
    else{
        PUT(HDRP(bp), PACK(block_size, 1));
        PUT(FTRP(bp), PACK(block_size, 1));
    }
}

