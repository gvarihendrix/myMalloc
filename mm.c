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
#define DSIZE 8
#define CHUNKSIZE (1 << 12)


#define GET(p) (*(unsigned int *) (p))
#define PUT(p, val) (*(unsigned int *) (p) = (val))

#define PACK(size, val) ((size)|(val))
/* Minimum block size */
#define MIN_BLOCK_SIZE 24

#define GET_SIZE(p) (GET(p) & ~(ALIGNMENT -1))
#define GET_ALLOC(p) (GET(p) & 0x1)


#define HEADER(bp)  ((char *) (bp) - WSIZE)
#define FOOTER(bp)  ((char *) (bp) + GET_SIZE(HEADER(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))


/* TAG_USED is the bit mask used in size_tag to mark a block as used, 
 * and NOT_USED that marks block as not in use
 */
#define TAG_USED 1
#define NOT_USED 0

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
#define BLK_HDR_SIZE ALIGN(sizeof(chunk))

// Global variable to the beginning of the heap list
static char *heaplist;

// My memeory block, only used in the free list
typedef struct mm_block chunk;

// What i store inside a free block, was done 
// just for the ease of pointer manipulation
struct mm_block {
    size_t size;
    chunk *next;
    chunk *prev;
};

/* function prototypes for the free list */
static void insert_free_block(chunk *, size_t);
static void remove_free_block(chunk *, size_t);


/* function prototypes for internal calls */
static void *coalesce(void *);
static void *find_fit(size_t );
static void place(void *, size_t );
static void *extend_heap(size_t );
static int in_heap(void *);

/* Functions used to debug the shit out of this */
void print_heap();
void print_free_list();
void heap_cheack();
static int checkblock(void *);
static void printblock(void *);
int mm_check();
int free_list_consistency();



/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{   // Head of the free list
    chunk *bp = mem_sbrk(BLK_HDR_SIZE);
    bp->size = BLK_HDR_SIZE;
    bp->next = bp;  // Point to itself so we dont get segemntation fault
    bp->prev = bp;  // My prev pointer has no footer size, so this is like my 
                    // epilouge header that can not and will not work with
    
    if ((heaplist = mem_sbrk(4 * WSIZE)) == NULL)
        return -1;

    PUT(heaplist, NOT_USED); /* Alignment padding */                   
    PUT(heaplist + WSIZE , PACK(DSIZE, TAG_USED)); /* Prolouge header */
    PUT(heaplist + DSIZE , PACK(DSIZE, TAG_USED)); /* Prolouge footer */
    PUT(heaplist + WSIZE + DSIZE, PACK(0, TAG_USED)); /* Epilouge header */
     
    heaplist += DSIZE;
    /* Extend the empty heap with a free block of chunksize bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;

    return 0;
}

/*
 * mm_malloc 
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{  
    size_t req_size;
    size_t extendsize;
    char *bp;
    /* Ignore spurious requests */
    if (size <= 0)
        return NULL; 
    
    if(size <= DSIZE)
        req_size = MIN_BLOCK_SIZE; // Need to have the mininum amount of bytes 24, or i will run into problems :P
    else
        req_size = DSIZE  * ((size + (DSIZE) +  DSIZE - 1) / DSIZE);

    if ((bp = find_fit(req_size)) != NULL) { 
        place(bp, req_size);
        return bp;
    }
    
    /* Will be called if the heap is not big enough for the req_size */
    extendsize = MAX(req_size, CHUNKSIZE);
    if((bp  =  extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
          
    place(bp, req_size);
    return  bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{   
    /* Just basic checks, if the pointer is NULL or not in the heap
     * or its free then return 
     */
    if (ptr == NULL || !in_heap(HEADER(ptr)) || GET_ALLOC(HEADER(ptr)) == 0)
        return;
    
    /* Takes in the pointer and gets its header size */
    size_t size = GET_SIZE(HEADER(ptr));
    /* Set its header and footer in the heaplist to be not used */
    PUT(HEADER(ptr), PACK(size, NOT_USED));
    PUT(FOOTER(ptr), PACK(size, NOT_USED));
    // Try to coalesce it
    coalesce(ptr);

}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 *///
void *mm_realloc(void *ptr, size_t size)
{
    if (size <= 0) {
        mm_free(ptr);
        return NULL;
    }
    
    if (ptr == NULL)
        return mm_malloc(size);

    if(!in_heap(ptr))
        return mm_malloc(size);
    
    size_t size_ptr = GET_SIZE(HEADER(ptr));

    void *newPtr= mm_malloc(size);
    
    if (newPtr != NULL) {

        if(size < size_ptr)
            size_ptr = size;

        memmove(newPtr, ptr, size_ptr);
        mm_free(ptr);
    }
    return newPtr;

   
}

/*
 * When freeing a list item we need to try to 
 * coalesce other blocks into the block we are freeing 
 * else just free the block we are working with
 */
static void* coalesce(void *bp)
{

    size_t prev_alloc;
    size_t next_alloc = GET_ALLOC(HEADER(NEXT_BLKP(bp)));
    /* Need to check of prev block is in the heap
     * if not prev alloc is assigned the value 1 wich
     * meens its taken, else its in the heap list and its
     * safe to get it 
     */
    if (!in_heap(PREV_BLKP(bp)))
        prev_alloc = 1;
    else 
        prev_alloc = GET_ALLOC(FOOTER(PREV_BLKP(bp)));     
    
    size_t size = GET_SIZE(HEADER(bp));

    if (prev_alloc && next_alloc) { // If they are both allocated
        insert_free_block((chunk *) bp, size);
        return bp;
    }

    else if (prev_alloc && !next_alloc) { // If next is not alloc, but prev is alloc
        
        remove_free_block((chunk *) NEXT_BLKP(bp), GET_SIZE(HEADER(NEXT_BLKP(bp))));
        size += GET_SIZE(HEADER(NEXT_BLKP(bp)));         
        
        PUT(HEADER(bp), PACK(size, NOT_USED));
        PUT(FOOTER(bp), PACK(size, NOT_USED));
    }

    else if (!prev_alloc && next_alloc) { // If prev is not alloc, but next is alloc
        size += GET_SIZE(HEADER(PREV_BLKP(bp)));
        
        PUT(FOOTER(bp), PACK(size, NOT_USED));
        PUT(HEADER(PREV_BLKP(bp)), PACK(size, NOT_USED));
        bp = PREV_BLKP(bp);
        
        // We romeve the block after point bp to its prev block
        remove_free_block((chunk *) bp, GET_SIZE(HEADER(bp)));
    }

    else {
        /* Prev and next are not allocated its safe to free them */

        size += GET_SIZE(HEADER(PREV_BLKP(bp))) + GET_SIZE(FOOTER(NEXT_BLKP(bp)));
        // We need to remove the prev and next from the free list so we can join them together 
        remove_free_block((chunk *) PREV_BLKP(bp), GET_SIZE(HEADER(PREV_BLKP(bp))));
        remove_free_block((chunk *) NEXT_BLKP(bp), GET_SIZE(FOOTER(NEXT_BLKP(bp))));
        
        // Assign the the heaplist that these blocks are free
        PUT(HEADER(PREV_BLKP(bp)), PACK(size, NOT_USED));
        PUT(FOOTER(NEXT_BLKP(bp)), PACK(size, NOT_USED));
        bp = PREV_BLKP(bp);
    }
    // insert the newly created bigger block into the freelist 
    insert_free_block((chunk *) bp, size);
    return bp;

}

/*
 * private function extend_heap
 * takes in a size_t word and returns NULL if it does not 
 * need to do anything else it resizes the heap
 */
static void* extend_heap(size_t words)
{
    size_t size;
    char *bp;
    // Just to keep everything aligned like in the book
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    if((long)(bp = mem_sbrk(size)) == -1) // If true i dont want to live
        return NULL;
    
    /* Initialize free block header/footer and the epilouge header */
    PUT(HEADER(bp), PACK(size, NOT_USED)); // Free block header
    PUT(FOOTER(bp), PACK(size, NOT_USED)); // Free block footer
    PUT(HEADER(NEXT_BLKP(bp)), PACK(0, TAG_USED)); // New epilouge header
    
    return coalesce(bp); // Coalesce with any adjecant block thats free
}


/* Perform a first-fit search of the implicit free list */
static void* find_fit(size_t asize)
{  
    chunk *p;
    /* Simple first fit search 
     * going trough the free list and if we find a block big 
     * enough for the request size we stop or if we go a hole circle 
     * trough the explicit list
     */
    for (p = ((chunk *)mem_heap_lo())->next; 
            p != mem_heap_lo() && p->size < asize; p = p->next);
    
    // If p has not gone the full circle return it 
    // else we did not find anything
    if(p != mem_heap_lo())
        return   p;
    else
        return NULL;
}


/* We need to keep track of all the free blocks */
static void insert_free_block(chunk *bp, size_t size) 
{
    chunk *head = (chunk *)mem_heap_lo();

    bp->size = size | NOT_USED;  
    bp->next = head->next;
    bp->prev = head;
    head->next = bp;
    bp->next->prev = bp;
}


/* When allocated, remove free block */
static void remove_free_block(chunk *bp, size_t size) 
{
    bp->size = size | TAG_USED; 
    bp->prev->next = bp->next;
    bp->next->prev = bp->prev;
}


/* Place the requested block at the beginning of the free block 
 * splitting only if the size of the remainder would equal or exceed 
 * the mininum block size
 */
static void place(void *bp, size_t asize)
{
    size_t size_of_bp = GET_SIZE(HEADER(bp));
    size_t split_size = size_of_bp - asize; 

    if (split_size >= (MIN_BLOCK_SIZE)) { // I need to have it 24 bytes min to split, else it will crash on a segmentation fault
                                     // Could not find out how to fix it :(.
        PUT(HEADER(bp), PACK(asize, TAG_USED)); 
        PUT(FOOTER(bp), PACK(asize, TAG_USED));
        remove_free_block((chunk*) bp, asize);

        bp = NEXT_BLKP(bp);

        PUT(HEADER(bp), PACK(split_size, NOT_USED));
        PUT(FOOTER(bp), PACK(split_size, NOT_USED));
        coalesce(bp);

    } else {

        PUT(HEADER(bp), PACK(size_of_bp, TAG_USED));
        PUT(FOOTER(bp), PACK(size_of_bp, TAG_USED));
        remove_free_block((chunk *) bp, size_of_bp);
        // Delete from free list
    }
}


static int in_heap(void *bp)
{
    if (bp >= mem_heap_lo() && bp <= mem_heap_hi())
        return 1; // For true
    else
        return 0; // False not in the heap
}

// Debugging function 
static int checkblock(void *bp)
{   
    if (GET_SIZE(HEADER(bp)) % 8) {
        printf("Error: %p is not doubleword aligned\n", bp);
        return -1;
    }

    if (GET(HEADER(bp)) != GET(FOOTER(bp))) {
        printf("Error: header does not match footer %p\n", bp);
        return -1;
    }

    return 0;
}

// Debugging function
static void printblock(void *bp)
{
    size_t hsize, halloc, fsize, falloc;

    hsize  = GET_SIZE(HEADER(bp));
    halloc = GET_ALLOC(HEADER(bp));
    fsize  = GET_SIZE(FOOTER(bp));
    falloc = GET_ALLOC(FOOTER(bp));

    printf("%p: header: [%d:%s] footer: [%d:%s]\n", bp,
            hsize, (halloc ? "alloc" : "free"), fsize, (falloc ? "alloc" : "free"));
}


// Debugging function
int mm_check()
{
    void *bp = heaplist;
    
    if ((GET_SIZE(HEADER(heaplist)) != DSIZE) || !GET_ALLOC(HEADER(heaplist))) {
        printf("Bad prolouge header\n");
        return -1;
    }

    if ( checkblock(heaplist) < 0) {
        printf("heap list not right\n");
        return -1;
    }

    for ( bp = heaplist; GET_SIZE(HEADER(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if ( checkblock(bp) < 0) {
            printblock(bp);
            printf("this block is not right\n");
            return -1;
        }
    }

    if ((GET_SIZE(HEADER(bp)) != 0 || !GET_ALLOC(HEADER(bp)))) {
            printf("Bad epilouge header\n");
            return -1;
    }

    return 0;
}

// Debbuging function
void heap_check()
{
    void *bp = heaplist;
    printf("Checking the heap for consintency\n");
    printf("Does the heap size align to the 8 byte alignment? %s\n", ((mem_heapsize() % 8) == 0 ? "true": "false")); 
    while(in_heap(bp)) {
        printf("Pointer %p is %s \n", bp,  (in_heap(bp) == 0 ? "not in the heap" : "is in the heap"));
        printf("Is every block using alignment of 8 bytes? %s\n", ((GET_SIZE(HEADER(bp)) % 8) == 0 ? "true" : "false"));
        bp = NEXT_BLKP(bp);
    }
}

// Debugging function
void print_heap(){
    void *bp = heaplist;
    while(in_heap(bp)){
        printf("%s block at %p, size %d and the heap size is %d\n",
                (GET_ALLOC(HEADER(bp))) ? "allocated" : "free",
                bp,
                GET_SIZE(HEADER(bp)), mem_heapsize()); 
        bp = NEXT_BLKP(bp);
    }
}

// Debugging function 
void print_free_list() {
    chunk *bp = ((chunk *)mem_heap_lo());
    int count = 0;
    do {
        printf("size of free block %d and pointer %p\n",
                bp->size, bp);
        bp = bp->next;
        count += 1;
    } while(bp != mem_heap_lo());

    printf("Number of blocks in the free list %d\n", count);
}

// Debugging function
int free_list_consistency() {
    chunk *bp =  (chunk *)mem_heap_lo();
    do {
        
        if (checkblock(bp) < 0){
            printblock(bp);
            printf("This pointer %p is not doing its job\n", bp);
            return -1;;
        }
        bp = bp->next;
        
    } while(bp != mem_heap_lo());
    
    return 0;
    
}
