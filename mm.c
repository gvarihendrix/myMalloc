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
#define OVERHEAD 16 // For the footer and header overhead
#define CHUNKSIZE (1 << 12)


#define GET(p) (*(unsigned int *) (p))
#define PUT(p, val) (*(unsigned int *) (p) = (val))

#define PACK(size, val) ((size)|(val))
/* Minimum block size */
#define MIN_BLOCK_SIZE (OVERHEAD)

#define GET_SIZE(p) (GET(p) & ~(ALIGNMENT -1))
#define GET_ALLOC(p) (GET(p) & 0x1)


#define HEADER(bp)  ((char *) (bp) - WSIZE)
#define FOOTER(bp)  ((char *) (bp) + GET_SIZE(HEADER(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))


/* TAG_USED is the bit mask used in size_tag to mark a block as used */
#define TAG_USED 1
#define NOT_USED 0

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
#define BLK_HDR_SIZE ALIGN(sizeof(chunk))

static char *heaplist;
static char *prolouge;

typedef struct mm_block chunk;

struct mm_block {
    size_t size;
    chunk *next;
    chunk *prev;
};

/* function prototypes for the free list */
static void insert_free_block(chunk *, size_t);
static void remove_free_block(chunk *, size_t);
static int in_heap(void *);

/* function prototypes for internal calls */
static void *coalesce(void *);
static void *find_fit(size_t );
static void place(void *, size_t );
static void *extend_heap(size_t );
void print_heap();
void print_free_list();
void heap_cheack();
static int greter_than_zero(size_t);
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
    bp->next = bp;
    bp->prev = bp;
    //insert_free_block(bp, BLK_HDR_SIZE);
    //checkblock(bp);
    //printblock(bp);
    //PUT(bp + WSIZE, PACK(bp->size, TAG_USED));
    //PUT(bp + DSIZE, PACK(bp->size, TAG_USED));
    
    if ((heaplist = mem_sbrk(4 * WSIZE)) == NULL)
        return -1;

    PUT(heaplist, NOT_USED);
    PUT(heaplist + WSIZE , PACK(DSIZE, TAG_USED));
    PUT(heaplist + DSIZE , PACK(DSIZE, TAG_USED));
    PUT(heaplist + WSIZE + DSIZE, PACK(0, TAG_USED));
    prolouge  = heaplist + WSIZE;
     
    heaplist += DSIZE;

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;

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
    char *bp;

    /* Ignore spurious requests */
    if (size <= 0)
        return NULL; 
    
    if(size <= DSIZE)
        req_size = MIN_BLOCK_SIZE + DSIZE; // Need to have the mininum amount of bytes 24
    else
        req_size = DSIZE  * ((size + (DSIZE) +  DSIZE - 1) / DSIZE);

    /*if (mm_check() < 0) {
        print_heap();
        printf("Error in mm_check, in malloc\n");
    }**/

    if ((bp = find_fit(req_size)) != NULL) { 
        place(bp, req_size);
        return bp;
    }
    
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

    if (ptr == NULL)
        return;
    
    /*if (mm_check() < 0) {
        printf("Error in mm_check\n");
    }
    if (free_list_consistency() < 0) {
        printf("Error int free_list_consitency\n");
    }
    */

    size_t size = GET_SIZE(HEADER(ptr));

    PUT(HEADER(ptr), PACK(size, NOT_USED));
    PUT(FOOTER(ptr), PACK(size, NOT_USED));

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

    void* newPtr = mm_malloc(size);
    
    if (newPtr == NULL)
        return NULL;

    int copysize = GET_SIZE(HEADER(ptr));
    if(size < copysize)
        copysize = size;

    memmove(newPtr, ptr, copysize);
    mm_free(ptr);
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
    size_t next_alloc;
    if (!in_heap(PREV_BLKP(bp)))  // || ((char *)(bp)-(3*WSIZE)) == prolouge)
        prev_alloc = 1;
    else 
        prev_alloc = GET_ALLOC(FOOTER(PREV_BLKP(bp))); 

    
    if (!in_heap(NEXT_BLKP(bp)))
        next_alloc = 1;
    else
        next_alloc = GET_ALLOC(HEADER(NEXT_BLKP(bp)));
    
    size_t size = GET_SIZE(HEADER(bp));

    if (prev_alloc && next_alloc) { // If they are both allocated
        insert_free_block((chunk *) bp, size);
        return bp;
    }

    else if (prev_alloc && !next_alloc) {
        
        remove_free_block((chunk *) NEXT_BLKP(bp), GET_SIZE(HEADER(NEXT_BLKP(bp))));
        size += GET_SIZE(HEADER(NEXT_BLKP(bp)));         
        
        PUT(HEADER(bp), PACK(size, NOT_USED));
        PUT(FOOTER(bp), PACK(size, NOT_USED));
    }

    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HEADER(PREV_BLKP(bp)));
        
        PUT(FOOTER(bp), PACK(size, NOT_USED));
        PUT(HEADER(PREV_BLKP(bp)), PACK(size, NOT_USED));
        bp = PREV_BLKP(bp);
        

        remove_free_block((chunk *) bp, GET_SIZE(HEADER(bp)));
    }

    else {
        size += GET_SIZE(HEADER(PREV_BLKP(bp))) + GET_SIZE(FOOTER(NEXT_BLKP(bp)));

        remove_free_block((chunk *) PREV_BLKP(bp), GET_SIZE(HEADER(PREV_BLKP(bp))));
        remove_free_block((chunk *) NEXT_BLKP(bp), GET_SIZE(FOOTER(NEXT_BLKP(bp))));

        PUT(HEADER(PREV_BLKP(bp)), PACK(size, NOT_USED));
        PUT(FOOTER(NEXT_BLKP(bp)), PACK(size, NOT_USED));
        bp = PREV_BLKP(bp);
    }

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

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    if((long)(bp = mem_sbrk(size)) == -1) // If true i dont want to live
        return NULL;
       
    assert(size > 0);

    PUT(HEADER(bp), PACK(size, NOT_USED));
    PUT(FOOTER(bp), PACK(size, NOT_USED));
    PUT(HEADER(NEXT_BLKP(bp)), PACK(0, TAG_USED));
    
    return coalesce(bp);
}


/* Perform a first-fit search of the implicit free list */
static void* find_fit(size_t asize)
{  
    chunk *p;

    for (p = ((chunk *)mem_heap_lo())->next; 
            p != mem_heap_lo() && p->size < asize; p = p->next);

    if(p != mem_heap_lo()){
        return  (void *) p;
    }
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


/* When allocated remove free block */
static void remove_free_block(chunk *bp, size_t size) 
{

    bp->size = size | TAG_USED;
    assert(bp->size > 0);
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

    if (split_size >= (DSIZE * 3)) { // I need to have it 24 bytes min to split, else it will crash on a segmentation fault
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

static int greter_than_zero(size_t size)
{
    if (size < 0)
        return 0;
    else 
        return 1;
}

static int in_heap(void *bp)
{
    if (bp >= mem_heap_lo() && bp <= mem_heap_hi())
        return 1; // For true
    else
        return 0; // False not in the heap
}


static int checkblock(void *bp)
{
    if ((size_t)bp % 8) {
        printf("Error: %p is not doubleword aligned\n", bp);
        return -1;
    }

    if (GET(HEADER(bp)) != GET(FOOTER(bp))) {
        printf("Error: header does not match footer %p\n", bp);
        return -1;
    }

    return 0;
}

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
        //printblock(bp);
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


void heap_check()
{
    void *bp = heaplist;
    printf("Checking the heap for consintency\n");
    printf("Does the heap size align to the 8 byte alignment? %s\n", ((mem_heapsize() % 8) == 0 ? "true": "false")); 
    while(in_heap(bp)) {
        printf("Pointer %p is %s \n", bp,  (in_heap( bp) == 0 ? "not in the heap" : "is in the heap"));
        printf("Is every block using alignment of 8 bytes? %s\n", ((GET_SIZE(HEADER(bp)) % 8) == 0 ? "true" : "false"));
        bp = NEXT_BLKP(bp);
    }
}

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

int free_list_consistency() {
    chunk *bp =  (chunk *)mem_heap_lo();
    do {
        if (checkblock(bp) < 0){
            printblock(bp);
            printf("This pointer %p is not doing its job\n", bp);
            return -1;;
        }
        bp = bp->next;

        assert(bp != NULL);
        
    } while(bp != mem_heap_lo());
    
    return 0;
    
}
