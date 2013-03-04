/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * You __MUST__ add your user information here and in the team array bellow.
 * 
 * === User information ===
 * Group: JeeBoy
 * User 1: ingvars11
 * SSN: 2107862529
 * User 2: elisae11
 * SSN: 2807815099
 * === End User Information ===
 */

/*
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
    "JeeBoy",
    /* First member's full name */
    "Ingvar Sigurdsson",
    /* First member's email address */
    "ingvars11@ru.is",
    /* Second member's full name (leave blank if none) */
    "Elísa Erludóttir",
    /* Second member's email address (leave blank if none) */
    "elisae11@ru.is",
    /* Third member's full name (leave blank if none) */
    "",
    /* Third member's email address (leave blank if none) */
    ""
};

/* Basic constant and macros */
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pick size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word ar address p */
#define GET(key) (*(unsigned int *) (key))
#define PUT(key, val) (*(unsigned int *)(key) = (val)) /* key points to this value */

/* Read the size and allocated fields from address key(key is a pointer) */
#define GET_SIZE(key) (GET(key) & ~0x7) /* This will get the size of the block wich we are referencing */
#define GET_ALLOC(key) (GET(key) & 0x1) /* This will check if the block is allocated */

/* Given a block pointer bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given a block pointer bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))
/* These macros and constan are used when manipulating the free list */

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))


/* Using the double linked list implementation of malloc */
struct mm_chunk {
    size_t prev_foot; /* Size of previous chunk (if free). */
    size_t head;      /* Size of chunk allocated and the three flag bits to determine if in use or not */
    struct mm_chunk *next_p; /* pointer to the next block only used in mm_free function */
    struct mm_chunk *prev_p; /* pointer to the previous block only used in mm_free function */
};

typedef struct mm_chunk chunk;     /* defining a memory chunk */
typedef struct mm_chunk *chunkptr; /* defining a ptr type */

static char* heap_list; /* Global variable used as the heap list */ 

/* function prototypes for internal calls */
static void* coalesce(void *bp);
static void* find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void* extend_heap(size_t words);

/*
 * When freeing a list item we need to try to 
 * coalesce other blocks into the block we are freeing 
 * else just free the block we are working with
 */
static void* coalesce(void *bp)
{
    size_t prev_alloc_block = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc_block = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t this_size = GET_SIZE(HDRP(bp));

    if (prev_alloc_block && next_alloc_block) { /* Case 1: No adjencant block is free */
        return bp;
    } 
    else if (prev_alloc_block && !next_alloc_block) { /* Case 2: block behind this block is free and not the block in front */
        this_size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(this_size, 0));
        PUT(FTRP(bp), PACK(this_size, 0));
    }
    else if (!prev_alloc_block && next_alloc_block) { /* Case 3: block behind this block is not free but the block in front of it is free */
        this_size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(this_size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(this_size, 0));
        bp = PREV_BLKP(bp); /* After we have coalsed the block we let bp point to it */
    }
    else { /* Case 4: both blocks are free so lets coales it with this block */
        this_size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(this_size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(this_size, 0));
        bp = PREV_BLKP(bp);
    }
    
    return bp;
}

/*
 * private function extend_heap
 * takes in a size_t word and returns NULL if it does not 
 * need to do anything else it resizes the heap
 */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2 ) ? (words + 1) * WSIZE : words * WSIZE;
    if((long unsigned int)(bp = mem_sbrk(size)) == -1)
        return NULL; /* we dont need to resize the heap */

    /* Initialize free block header/footer and the epilouge header */
    PUT(HDRP(bp), PACK(size, 0)); /* Free block header */
    PUT(FTRP(bp), PACK(size, 0)); /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilouge header */

    return coalesce(bp);
}


/* Perform a first-fit search of the implicit free list */
static void *find_fit(size_t asize)
{   void  *ptr;
    for(ptr = heap_list;ptr != NULL && GET_SIZE(HDRP(ptr)) > 0; ptr = NEXT_BLKP(ptr)) {
        if (!GET_ALLOC(HDRP(ptr)) && (asize <= GET_SIZE(HDRP(ptr)))) { 
            return ptr;
        }
    }
    return NULL; /* If search could not find a block big enough */
}

/* Place the requested block at the beginning of the free block 
 * splitting only if the size of the remainder would equal or exceed 
 * the mininum block size
 */
static void place(void *bp, size_t asize)
{
    size_t size_of_ptr = GET_SIZE(HDRP(bp));
    size_t size_to_split = size_of_ptr - asize; 
    // Min block size == 8, overhead == 8, so total 16 bytes min block
    
    if(size_to_split >= (2 * DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp); /* Getting the next block to split it */
        PUT(HDRP(bp), PACK(size_to_split, 0));
        PUT(FTRP(bp), PACK(size_to_split, 0));
    }
    else {
        PUT(HDRP(bp), PACK(size_of_ptr, 1));
        PUT(FTRP(bp), PACK(size_of_ptr, 1));
    }
}


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the intial empty heap */
    if ((heap_list = mem_sbrk(4 * WSIZE)) == (void *) - 1)
        return -1;
    PUT(heap_list, 0);                              /* Alignment padding */
    PUT(heap_list + (1 * WSIZE), PACK(DSIZE, 1));   /* Prolouge header */
    PUT(heap_list + (2 * WSIZE), PACK(DSIZE, 1));   /* Prolouge footer */
    PUT(heap_list + (3 * WSIZE), PACK(0, 1));       /* Epilogue header */
    heap_list += (2 * WSIZE);
    
    /* Extend the heap with a free block of CHUNKSIZE */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;

    /* On success */
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;       /* Adjusted block size */
    size_t extendsize;  /* Amount to extend heap if no fit */
    void *bp;

    if (heap_list == NULL)
        mm_init();
    /* Ignore spurious requests */
    if (size <= 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs */
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    
    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL; /* No more memory :( */
    
    place(bp, asize);
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
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

