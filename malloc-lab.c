/*
 * 15213 malloc Lab 
 * Name: Xuan Li
 * ID: xuanli1
 *
 * ----------------------------- EXPLANATION -----------------------------
 *
 * 1. Use method 3: segregated free list as base,
 *                      implement 10 buckets of free blocks.
 * 2. Extend heap methods: 
 *                      default size: CHUNKSIZE / 4;
 *                      size would doubled when heap is small;
 *                      size would be set to CHUNKSIZE when heap is large.
 * 3. use modified best fit to find a free block:
 *                      return the best result from the first 3 fits locally,
 *                      or return best fit in a higher bucket if no enough
 *                      space in current bucket.
 * 
 * ------------------------------------------------------------------------                     
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif


/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~7)

#define WSIZE       4      
#define DSIZE       8
#define ALIGNMENT   8
#define SEG_COUNT   10       
#define CHUNKSIZE   1024  

#define MAX(x, y) ((x) > (y) ? (x) : (y))  
#define MIN(x, y) ((x) < (y) ? (x) : (y))  

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc)) 

/* Read and write 8 byte at address p */
#define GET(p)          (*(unsigned int *)(p))            
#define PUT(p, val)     (*(unsigned int *)(p) = (val))    

/* return block size and address at header or footer */
#define GET_SIZE(p)     (GET(p) & ~7)                   
#define GET_ALLOC(p)    (GET(p) & 1)                    

/* return address of header and footer of a block given pointer */
#define HDRP(bp)        ((char *)(bp) - WSIZE)                      
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) 

/* calculate next and previous address of block */
#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) 
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) 

/* calculate next and previous address of free block */
#define NEXT_FREEBLKP(bp)               (*(void **)(bp)) 
#define PREV_FREEBLKP(bp)               (*(void **)(bp + DSIZE)) 

/* minimum block size and compute block size */
#define MIN_BLK_SIZE    (3*DSIZE)
#define BLK_SIZE(size)  (((size)<=2*DSIZE) ? MIN_BLK_SIZE : ALIGN(DSIZE+size))

/* set free block in the free list */
#define SET_NEXT_FREEBLK(bp, ptr)       (NEXT_FREEBLKP(bp) = ptr)
#define SET_PREV_FREEBLK(bp, ptr)       (PREV_FREEBLKP(bp) = ptr)

/* Global variables*/
static char *heap_listp = 0;            /* Pointer to the first block in heap */
static char *seg_listp[SEG_COUNT];      /* Pointers to each segregation */

/* malloc key functions */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *ptr);

/* functions for seg free list */
static void add_to_list(void *ptr);
static void delete_from_list(void *ptr);
static int index_seg(size_t blksize);

/* functions for checking correctness */
static void checkblock(void *ptr);
static void printblock(void *ptr);
static void printfreeblock(void *ptr);
static void checkfreelist(void);
static void printfreelist(void);

/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {
        /* create initial empty heap */
        if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) return -1;

        PUT(heap_listp, 0);                          /* Alignment padding */
        PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); /* Prologue header */
        PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
        PUT(heap_listp + (3*WSIZE), PACK(0, 1));     /* Epilogue header */
        heap_listp += (2*WSIZE);

        /* Create the initial empty seg list */
        for (int i = 0; i < SEG_COUNT; ++i){
                seg_listp[i] = NULL;
        }

        /* Extend the empty heap with a free block of (CHUNKSIZE/4) bytes */
        if (extend_heap(CHUNKSIZE/(WSIZE*4)) == NULL) return -1;

        /* initialization succeeds */
        return 0;
}

/*
 * malloc: return pointer to allocated block 
 */
void *malloc (size_t size) {
        size_t blksize = BLK_SIZE(size);    /* block size */
        size_t extdsize;                        /* extend heap size */
        char *bp;

        /* init heap if heap is empty */
        if (heap_listp == 0) mm_init();

        /* ignore edge case */
        if (size == 0) return NULL;

        /* search free list for fit */
        if ((bp = find_fit(blksize)) != NULL) place(bp, blksize);

        /* allocate memory and place block if fit not found */
        /* extend heap size when needed */
        else {
                extdsize = MAX(blksize, MIN(mem_heapsize(), CHUNKSIZE));
                if ((bp = extend_heap(extdsize/WSIZE)) == NULL)
                        return NULL;
                place(bp, blksize);
        }
        return bp;

}

/*
 * free: free a block
 */
void free (void *bp) {
        if (bp == 0) return;

        size_t size = GET_SIZE(HDRP(bp));

        if (heap_listp == 0) mm_init();

        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        coalesce(bp);
}

/*
 * realloc - original implementation version 
 */
void *realloc(void *ptr, size_t size) {
        size_t oldsize;
        void *newptr;

        /* oldptr is NULL, then this is just malloc. */
        if (ptr == NULL) {
                return malloc(size);
        }

        /* just free when size == 0 */
        if (size == 0){
                free(ptr);
                return 0;
        }

        newptr = malloc(size);

        /* if realloc() fails the original bolck is left untouched */
        if(!newptr) return 0;
        /* copy the old data */
        oldsize = GET_SIZE(HDRP(ptr));

        /* extend size when needed */
        if(size < oldsize) oldsize = size;
        memcpy(newptr, ptr, oldsize);
        /* free the old block */
        free(ptr);

        return newptr;
}

/*
 * calloc - you may want to look at mm-naive.c
 * This function is not tested by mdriver, but it is
 * needed to run the traces
 */
void *calloc (size_t nmemb, size_t size) {
        size_t bytes = nmemb * size;
        void *newptr;

        newptr = malloc(bytes);
        memset(newptr, 0, bytes);

        return newptr;
}

/*
 * to coalesce boundary tag
 * return pointer to coalesced block
 */
static void *coalesce(void *ptr) {
        char *prev_block = PREV_BLKP(ptr);
        char *next_block = NEXT_BLKP(ptr);
        size_t prev_alloc = GET_ALLOC(FTRP(prev_block));
        size_t next_alloc = GET_ALLOC(HDRP(next_block));
        size_t size = GET_SIZE(HDRP(ptr));

        if (prev_alloc && next_alloc) {            /* Case 1 */
                /* bp stay put */
                add_to_list(ptr);
                return ptr;
        }

        else if (prev_alloc && !next_alloc) {      /* Case 2 */
                /* bp stay put */
                size += GET_SIZE(HDRP(next_block));
                delete_from_list(next_block);
                PUT(HDRP(ptr), PACK(size, 0));
                PUT(FTRP(ptr), PACK(size,0));
              add_to_list(ptr);
                return ptr;
        }

        else if (!prev_alloc && next_alloc) {      /* Case 3 */
                ptr = prev_block;
                size += GET_SIZE(HDRP(prev_block));
                delete_from_list(prev_block);
                PUT(HDRP(ptr), PACK(size, 0));
                PUT(FTRP(ptr), PACK(size, 0));
                add_to_list(ptr);
                return ptr;
        }

        else {                                     /* Case 4 */
                delete_from_list(prev_block);
                delete_from_list(next_block);
                size += (GET_SIZE(HDRP(prev_block)) + GET_SIZE(FTRP(next_block)));
                PUT(HDRP(prev_block), PACK(size, 0));
                PUT(FTRP(next_block), PACK(size, 0));
                ptr = prev_block;
                add_to_list(ptr);
                return ptr;
        }

}

/* 
 * initial block of asize bytes whenfree block bp;
 * split if remainder is at least min[block_size]
 */
static void place(void *ptr, size_t blksize) {
        char *nextptr;
        size_t freedsize = GET_SIZE(HDRP(ptr));

        if (!GET_ALLOC(ptr)) {
                delete_from_list(ptr);
        } else {
                printf("error: attempt to place in allocated block %p.\n", ptr);
                return;
        }

        if ((freedsize - blksize) >= MIN_BLK_SIZE) {
               PUT(HDRP(ptr), PACK(blksize, 1));
                PUT(FTRP(ptr), PACK(blksize, 1));
                /* split free block and put the remainder in seg list */
                nextptr = NEXT_BLKP(ptr);
                PUT(HDRP(nextptr), PACK(freedsize-blksize, 0));
                PUT(FTRP(nextptr), PACK(freedsize-blksize, 0));
                add_to_list(nextptr);
        } else {
        /* allocate all freesize for blksize */
                PUT(HDRP(ptr), PACK(freedsize, 1));
                PUT(FTRP(ptr), PACK(freedsize, 1));
        }

}

/* 
 * find a fit for block given required size .
 * return pointer to that block if found, else return null.
 * search from the lowest segment list index.
 * return the best one of first 5 fits in one bucket.
 */
static void *find_fit(size_t blksize) {
        void *ptr;
        void *bestfit = NULL;
        unsigned int remains = ~0;
        int count = 0;

        /* search from the lowest seg_idx */
        for (int i = index_seg(blksize); i < SEG_COUNT; i++) {

                /* return fit when jump to higher bucket */
                if (bestfit != NULL) return bestfit;

                /* return best fit among first 5 fits in the same bucket */
                for (ptr = seg_listp[i]; ptr != NULL; ptr = NEXT_FREEBLKP(ptr)){
                        if (blksize <= GET_SIZE(HDRP(ptr))) {
                                if ((GET_SIZE(HDRP(ptr)) - blksize) < remains){
                                        bestfit = ptr;
                                        remains = GET_SIZE(HDRP(ptr)) - blksize;
                                }

                        count++;
                       if (count > 3) return bestfit;
                        }
                }
        }
        return bestfit;

}


/* 
 * to extend heap using free block
 * return the extended block pointer
 */
static void *extend_heap(size_t words)
{
        char *bp;
        size_t size;

        /* Allocate an even number of words to maintain alignment */
        size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
        if ((long)(bp = mem_sbrk(size)) == -1) return NULL;
        /* Initialize free block header/footer and the epilogue header */
        PUT(HDRP(bp), PACK(size, 0));         /* Free block header */
        PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */
        PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

        /* Coalesce if the adjacent blocks are free */
        return coalesce(bp);

}

/*
 * judge whether the pointer located in heap
 */
static int in_heap(const void *ptr) {
        return ptr <= mem_heap_hi() && ptr >= mem_heap_lo();

}

/*
 * judge if pointer is aligned
 */
static int aligned(const void *ptr) {
        return (size_t)ALIGN(ptr) == (size_t)ptr;

}

/*
 * add a free block to seg list based on current size
 */
static void add_to_list(void *ptr){
        int index = index_seg(GET_SIZE(HDRP(ptr)));

        /* no free block this bucket */
        if (seg_listp[index] == NULL){
                SET_PREV_FREEBLK(ptr, NULL);
                SET_NEXT_FREEBLK(ptr, NULL);
                seg_listp[index] = ptr;
        }

        /* add block to the head of the bucket */
        else {
                SET_PREV_FREEBLK(ptr, NULL);
                SET_NEXT_FREEBLK(ptr, seg_listp[index]);
                SET_PREV_FREEBLK(seg_listp[index], ptr);
                seg_listp[index] = ptr;
        }
}

/*
 * delete free block free of segment list
 */
static void delete_from_list(void *ptr) {
        int index = index_seg(GET_SIZE(HDRP(ptr)));
        char *prev_block = PREV_FREEBLKP(ptr);
        char *next_block = NEXT_FREEBLKP(ptr);

        /* clear the deleted block */
        SET_PREV_FREEBLK(ptr, NULL);
        SET_NEXT_FREEBLK(ptr, NULL);

        /* rewire connections of the list */
        if (prev_block == NULL){
                if (next_block == NULL){
                        /* delete the only free block */
                        seg_listp[index] = NULL;
                } else {
                        /* delete the first free block */
                        seg_listp[index] = next_block;
                        SET_PREV_FREEBLK(next_block, NULL);
                }
        }

        else if (next_block == NULL){
                /* delete the last free block */
                SET_NEXT_FREEBLK(prev_block, NULL);
        }

        else {
                /* delete an intermediate free block */
                SET_PREV_FREEBLK(next_block, prev_block);
                SET_NEXT_FREEBLK(prev_block, next_block);
        }

}

/*
 * return index for segment list
 * @param block size
 */
static int index_seg(size_t blk_num){
        if      (blk_num < 33)         return 0;
        else if (blk_num < 65)         return 1;
        else if (blk_num < 129)        return 2;
        else if (blk_num < 257)        return 3;
        else if (blk_num < 513)        return 4;
        else if (blk_num < 1025)       return 5;
        else if (blk_num < 2049)       return 6;
        else if (blk_num < 4097)       return 7;
        else if (blk_num < 8193)       return 8;
        else                           return 9;
}

/* 
 * print out block 
 */
static void printblock(void *ptr) {
        size_t hdsize, hdlloc, ftsize, ftlloc;

        hdsize = GET_SIZE(HDRP(ptr));
        hdlloc = GET_ALLOC(HDRP(ptr));
        ftsize = GET_SIZE(FTRP(ptr));
        ftlloc = GET_ALLOC(FTRP(ptr));

        if (hdsize == 0) {
                printf("%p: initial error\n", ptr);
                return;
         }

        printf("%p: header: [%d:%c] footer: [%d:%c]\n", ptr,
                        (int)hdsize, (hdlloc ? 'a' : 'f'),
                        (int)ftsize, (ftlloc ? 'a' : 'f'));
}

/* 
 * print details of free block
 */
static void printfreeblock(void *ptr){
        printblock(ptr);
        printf("PREV_FREEBLKP: %p\n", PREV_FREEBLKP(ptr));
        printf("NEXT_FREEBLKP: %p\n", NEXT_FREEBLKP(ptr));
}

/*
 * print out segment free block in bucket order
 */
static void printfreelist(void){
        char *ptr;
        printf("-> head of segment list:\n");
        for (int i = 0; i < SEG_COUNT; i++){
                printf("-> Seg_listp[%d]:\n", i);
                for (ptr = seg_listp[i]; ptr != NULL ; ptr = NEXT_FREEBLKP(ptr)) {
                        printfreeblock(ptr);
            }
        }
        printf("<- End of segment list.\n");
}

/* 
 * check if segment list is correct in:
 *     block correctness
 *         block-bucket consistency
 *     # of free blocks consistent with heap
 * return error message
 */
static void checkfreelist(void){
        char *ptr;
        int listfreecount = 0;
        int heapfreecount = 0;

        for (int i = 0; i < SEG_COUNT; ++i){
                for (ptr = seg_listp[i]; ptr != NULL; ptr = NEXT_FREEBLKP(ptr)) {
                        listfreecount++;        /* count free blocks in list */
                        checkblock(ptr);        /* check block correctness */

                        /* check if block is free */
                        if (GET_ALLOC(HDRP(ptr))) {
                                printf("ERROR %p: block allocated in free list.\n", ptr);
                                printfreelist();
                                exit(-1);
                        }

                        /* check block-bucket consistency */
                        if (index_seg(GET_SIZE(HDRP(ptr))) != i) {
                                printf("ERROR %p: block located in wrong bucket.\n", ptr);
                                printfreelist();
                                exit(-1);
                        }
                }
        }

        /* check total number of free blocks in heap */
        for (ptr = heap_listp; GET_SIZE(HDRP(ptr)) > 0; ptr = NEXT_BLKP(ptr)) {
                if (!GET_ALLOC(HDRP(ptr))) {
                        printf("Found free in heap:%p\n", ptr);
                        printblock(ptr);
                        heapfreecount++;
            }
        }

        if (listfreecount != heapfreecount){
                printf("ERROR: numbers of free blocks inconsistent with heap.\n");
                exit(-1);
        }

}

/*
 * to check if a single block whether:
 *      pointer is in the heap;
 *          aligned;
 *              consistent of header and footer;
 *                  correct when coalesed.
 */
static void checkblock(void *ptr)
{
        /* check if the block is in the heap */
        if(!in_heap(ptr)) {
                printf("error: %p is not in heap \n", ptr);
                exit(-1);
        }

        /* check alignment */
        if (!aligned(ptr)){
                printf("error: %p is not aligned\n", ptr);
                exit(-1);
        }

        /* check header-footer consistency */
        if (GET(HDRP(ptr)) != GET(FTRP(ptr))){
                printf("error: header and footer mismatch\n");
                exit(-1);
        }

        /* check coalescing correctness */
        if (GET_ALLOC(HDRP(ptr)) == 0
                && NEXT_BLKP(ptr) != NULL
                        && GET_ALLOC(HDRP(NEXT_BLKP(ptr))) == 0) {
                printf("error: %p is not coalesced with next block\n", ptr);
                exit(-1);
        }

}

/*
 * mm_checkheap
 */
void mm_checkheap(int verbose) {
        char *ptr = heap_listp;

        if (verbose) printf("Heap (%p):\n", heap_listp);

        /* check prologue */
        if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp))) {
            printf("Bad prologue header\n");
            exit(-1);
        }
        checkblock(heap_listp);

        /* check each block in the heap */
        for (ptr = heap_listp; GET_SIZE(HDRP(ptr)) > 0; ptr = NEXT_BLKP(ptr)) {
                if (verbose)
                        printblock(ptr);
                        checkblock(ptr);
        }

        /* check epilougue */
        if ((GET_SIZE(HDRP(ptr)) != 0) || !(GET_ALLOC(HDRP(ptr)))){
                printf("Bad epilogue header\n");
                exit(-1);
        }

        /* check correctness of the segregated free list */
        checkfreelist();

}

