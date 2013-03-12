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

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define WSIZE 4
#define CHUNKSIZE (1 << 12)


#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))


/* Macro for pointer arithmetic */
#define POINTER_ADD(p, x) ((char *) (p) + (x))
#define POINTER_SUB(p, x) ((char *) (p) - (x))

/* Pointer to the head of the list */
#define FREE_LIST_HEAD *((chunk **)mem_heap_lo())

/* Minimum block size */
#define MIN_BLOCK_SIZE (4 *  WSIZE)

#define GET_SIZE(p) (GET(p) & ~(ALIGNMENT -1))
#define GET_ALLOC(p) (GET(p) & 0x1)
#define GET_BLOCK(p, size) ((chunk *)((char *) (p) + (size))) 

#define HEADER(bp)  ((char *) (bp) - WSIZE)
#define FOOTER(bp)  ((char *) (bp) + GET_SIZE(HEADER(bp)) - ALIGNMENT)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - ALIGNMENT)))
/* TAG_USED is the bit mask used in size_tag to mark a block as used */
#define TAG_USED 1

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)



#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
#define CHUNK_SIZE (ALIGN(sizeof(chunk)))

typedef struct mm_chunk chunk;     /* defining a memory chunk */
/* Using the double linked list implementation of malloc */
struct mm_chunk {
    size_t size_tags;/* Size of chunk allocated and the three flag bits to determine if in use or not */
    chunk *next; /* pointer to the next block only used in mm_free function */
    chunk *prev; /* pointer to the previous block only used in mm_free function */
};




/* function prototypes for the free list */
static void insert_free_block(chunk *node);
static void remove_free_block(chunk *node);
static void init_free_list();
static int in_heap(void *bp);

/* function prototypes for internal calls */
static void coalesce(chunk *oldbp);
static void* find_fit(size_t asize);
static void place(chunk *bp, size_t asize);
static void extend_heap(size_t words);
void print_heap();
void print_free_list();
void heap_cheack();


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{   // Head of the free list
    init_free_list(); 
    extend_heap(CHUNKSIZE/WSIZE);

    return 0;
}

/* (chunk *) 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{  
    size_t req_size;
    size_t extendsize;
    /* Ignore spurious requests */
    if (size <= 0)
        return NULL; 
    
    if(size <= MIN_BLOCK_SIZE)
        req_size = MIN_BLOCK_SIZE;
    else
        req_size = ALIGNMENT * ((size + (ALIGNMENT) +  ALIGNMENT - 1) / ALIGNMENT);
     
    chunk *bp;
    if ((bp = find_fit(req_size)) != NULL) {
        place(bp, req_size);
        return (char *) bp + CHUNK_SIZE;
    }

    extendsize = MAX(req_size, CHUNKSIZE);
    extend_heap(extendsize/WSIZE);
        //return NULL;
    if((bp = find_fit(req_size)) != NULL){       
        place(bp, req_size);
        return  (char *)bp + CHUNK_SIZE;
    }

    return NULL; // Everything failed
}
/*
static void place(chunk *ptr, size_t size) {
    ptr->head_size |= 1;
    ptr->prev_p->next_p = ptr->next_p;
    ptr->next_p->prev_p = ptr->prev_p;
}
*/
/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    if (ptr == NULL)
        return;

    chunk *bp = ptr - CHUNK_SIZE;
    //assert(size == bp->size_tags);
    
    insert_free_block(bp);
    coalesce(bp);
    // Need to implement some coalesing to get
    // utl up more
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 *///
void *mm_realloc(void *ptr, size_t size)
{
   chunk *bp = ptr - CHUNK_SIZE;
   void* newPtr = mm_malloc(size);
    
   if (newPtr == NULL)
       return NULL;

   int copysize = bp->size_tags - CHUNK_SIZE;
   if(size < copysize)
       copysize = size;

   memcpy(newPtr, ptr, copysize);
   mm_free(ptr);
   return newPtr;
}

/*
 * When freeing a list item we need to try to 
 * coalesce other blocks into the block we are freeing 
 * else just free the block we are working with
 */
static void coalesce(chunk *oldbp)
{
    //assert(oldbp != NULL);
    //assert(oldbp->size_tags != (size_t) NULL);

    if (oldbp->next == mem_heap_lo() || oldbp->prev == mem_heap_lo() || oldbp == mem_heap_lo())
        return;
    
    size_t prev_alloc = 0;
    size_t next_alloc = 0;

    if (oldbp->prev != NULL)
        prev_alloc = oldbp->prev->size_tags & 1; //Returns 1 next is alloc, else 0
    if (oldbp->next != NULL)
        next_alloc = oldbp->next->size_tags & 1;
    
    size_t size = oldbp->size_tags;
   
    /* Setting up the logic for coalesing the blocks */
    if (prev_alloc && next_alloc) {
        
        return;/* both prev and next are allocated, or the list is empty */
    }

    else if (prev_alloc && !next_alloc) {
        /* prev is allocated but not the next pointer */
        size += oldbp->next->size_tags;
        oldbp->size_tags = size;
        //insert_free_block(oldbp);;
        //oldbp = oldbp->next;
    }

    else if (!prev_alloc && next_alloc) {
        /* prev is not allocated but next is allocated */
        size += oldbp->prev->size_tags;
        oldbp->size_tags = size;
        //insert_free_block(oldbp);
        //oldbp = oldbp->prev;
    }

    else {
        /* Neither pointers are allocated */
        size += oldbp->next->size_tags + oldbp->prev->size_tags; 
        oldbp->size_tags = size; 
        //insert_free_block(oldbp);
        //oldbp = oldbp->prev;
    }

    //return oldbp;
}

/*
 * private function extend_heap
 * takes in a size_t word and returns NULL if it does not 
 * need to do anything else it resizes the heap
 */
static void extend_heap(size_t words)
{
    chunk *bp;
    size_t size;
     
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if((long)(bp = mem_sbrk(size)) == -1) // If true i dont want to live
        exit(0);

    bp->size_tags = size;
    bp->size_tags |= TAG_USED;
    
    insert_free_block(bp);
    coalesce(bp);
}


/* Perform a first-fit search of the implicit free list */
static void* find_fit(size_t asize)
{  
    chunk *p;
    for (p = ((chunk *) mem_heap_lo())->next; 
            p != mem_heap_lo() && p->size_tags < asize;
            p = p->next);
   
    if (p !=  mem_heap_lo())
        return p;
    else 
        return NULL;
}


static void init_free_list()
{
    chunk *flist;
    /* We need to initalize the heap with the list size 
     * and 2 WSIZE that keep track of the header and footer
     */
    size_t init_size = CHUNK_SIZE + WSIZE + WSIZE;
    size_t total_size;

    if ((flist = mem_sbrk(init_size)) == (void *) -1)
        exit(0);

    /* becomes the header of the list, on wordsize in front of the mem_heap_lo() pointer */
    total_size = init_size  - WSIZE - WSIZE;
    
    flist->size_tags = total_size | TAG_USED;
    flist->next = flist;
    flist->prev = flist;
}

/* We need to keep track of all the free blocks */
static void insert_free_block(chunk *node) 
{  
    chunk *head = (chunk *)mem_heap_lo();

    node->size_tags &= ~TAG_USED;
    node->next = head->next;
    node->prev = head;
    head->next = node;
    node->next->prev = node;

    //assert(((chunk *)mem_heap_lo())->next == (FREE_LIST_HEAD)->next);
}


/* When allocated remove free block */
static void remove_free_block(chunk *node) 
{  
    node->size_tags |= TAG_USED;
    node->prev->next = node->next;
    node->next->prev = node->prev;
    
    //assert(((chunk *)mem_heap_lo())->next == (FREE_LIST_HEAD)->next);
}


/* Place the requested block at the beginning of the free block 
 * splitting only if the size of the remainder would equal or exceed 
 * the mininum block size
 */
static void place(chunk *bp, size_t asize)
{
    size_t size_of_bp = bp->size_tags;
    size_t split_size = size_of_bp - asize;
    //printf("size_of_bp = %d, asize = %d, %d - %d = split_size = %d\n", size_of_bp, asize, size_of_bp, asize, split_size);

    if (split_size >= (2 * ALIGNMENT)){
        // This means i can do some splitting
        bp->size_tags = asize;
        chunk *split = bp->next;
        //assert(split != NULL);
        
        //printf("size to split %d and size of return pointer %d\n",split_size, bp->size_tags); 
        split->size_tags = split_size;
        remove_free_block(bp);
        insert_free_block(split);
        coalesce(split);
        
    } else {
        bp->size_tags = asize;
        // Just connect the allocated
        remove_free_block(bp);
    }
}

static int in_heap(void *bp)
{
    if (bp >= mem_heap_lo() && bp < mem_heap_hi())
        return 1; // For true
    else
        return 0; // False not in the heap
}



void heap_cheack()
{
    chunk *bp = ((chunk *)mem_heap_lo())->next;
    printf("Checking the heap for consintency\n");
    printf("Does the heap size align to the 8 byte alignment? %s\n", ((mem_heapsize() % 8) == 0 ? "true": "false")); 
    while(bp < (chunk *) mem_heap_hi()) {
        printf("Pointer %p is %s \n", bp,  (in_heap((char *) bp) == 0 ? "not in the heap" : "is in the heap"));
        printf("Is every block using alignment of 8 bytes? %s\n", ((bp->size_tags % 8) == 0 ? "true" : "false"));
        bp = (chunk *)((char *) bp + (bp->size_tags & ~1));
    }
}

void print_heap(){
    chunk *bp = ((chunk *)mem_heap_lo())->next;
    while(bp <  (chunk *) mem_heap_hi()){
        printf("%s block at %p, size %d and the heap size is %d\n",
                (bp->size_tags & 1) ? "allocated" : "free",
                bp,
                (int)(bp->size_tags & ~1), mem_heapsize()); 
        bp = (chunk *)((char *)bp + (bp->size_tags & ~1));
    }
}

void print_free_list() {
    chunk *bp = ((chunk *)mem_heap_lo())->next;
    int count = 0;
    printf("sizeof chunk %d and minum block size is %d\n", CHUNK_SIZE, MIN_BLOCK_SIZE);
    while(bp != (chunk *) mem_heap_lo()) {
        printf("size of free block %d and pointer %p\n",
                bp->size_tags, bp);
        bp = bp->next;
        count += 1;
    }

    printf("Number of blocks in the free list %d\n", count);
}
