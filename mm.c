#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Team Cash Money Heroes",
    /* First member's full name */
    "James Schuback",
    /* First member's email address */
    "iiJDSii@gmail.com",
    /* Second member's full name (leave blank if none) */
    "Mohamed Kayed",
    /* Second member's email address (leave blank if none) */
    "kayedmo@gmail.com"
};

typedef int bool;
#define true 1
#define false 0

/*************************************************************************
 * Basic Constants and Macros
 * You are not required to use these macros but may find them helpful.
 *************************************************************************/
#define WSIZE       sizeof(void *)         /* word size (bytes) */
#define DSIZE       (2 * WSIZE)            /* doubleword size (bytes) */
#define CHUNKSIZE   (1<<7)      /* initial heap size (bytes) */
#define OVERHEAD    DSIZE     /* overhead of header and footer (bytes) */

#define MAX(x,y) ((x) > (y)?(x) :(y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)          (*(uintptr_t *)(p))
#define PUT(p,val)      (*(uintptr_t *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)     (GET(p) & ~(DSIZE - 1))
#define GET_ALLOC(p)    (GET(p) & 0x1)


/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)        ((char *)(bp) - WSIZE)
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* alignment */
#define ALIGNMENT 16
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0xf)
void* heap_ptr = NULL;
size_t num_segregated_lists = 10;
void* heap_listp[10];

///////////////////// Additional ///////////////////////////
// Structure of free blocks: [header][next_block_ptr][prev_block_ptr][][][]...[][footer]
// (the block pointer thus points at the location of the next_block_ptr)

//GET EXPLICIT LIST NEXT AND PREV BLOCKS
#define EXP_NEXT_BLKP(bp) GET(bp)
#define EXP_PREV_BLKP(bp) GET((char *)(bp)+WSIZE) //(char*)(bp) casting necessary for raw address manipulation (the bits of the address)

//SET EXPLICIT LIST NEXT AND PREV BLOCKS for a block with block pointer bp
#define EXP_SET_NEXT_BLKP(bp, next_block_ptr) PUT(bp, next_block_ptr)
#define EXP_SET_PREV_BLKP(bp, prev_block_ptr) PUT((char *)(bp)+WSIZE, prev_block_ptr) 

/**********************************************************
 * mm_init
 * Initialize the heap, including "allocation" of the
 * prologue and epilogue
 * Initialize our dynamic memory system: the segregated list 
 * pointers.
 **********************************************************/
int mm_init(void)
{
    if ((heap_ptr = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_ptr, 0);                         // alignment padding
    PUT(heap_ptr + (1 * WSIZE), PACK(OVERHEAD, 1));   // prologue header
    PUT(heap_ptr + (2 * WSIZE), PACK(OVERHEAD, 1));   // prologue footer
    PUT(heap_ptr + (3 * WSIZE), PACK(0, 1));    // epilogue header
    heap_ptr += DSIZE; //block pointer of first block

    int i;
    for (i = 0; i < num_segregated_lists; i++)
        heap_listp[i] = NULL;

    return 0;
}

/**********************************************************
 * list_index
 * return segregated list array index
 **********************************************************/
int list_index(size_t size) {

    if (size > 16384)
        return 9;
    else if (size > 8192)
        return 8;
    else if (size > 4096)
        return 7;
    else if (size > 2048)
        return 6;
    else if (size > 1024)
        return 5;
    else if (size > 512)
        return 4;
    else if (size > 256)
        return 3;
    else if (size > 128)
        return 2;
    else if (size > 64)
        return 1;
    else
        return 0;

}

/**********************************************************
 * extra_realloc_size
 * Upon realloc, return a size larger than requested to save
 * time on likely future realloc requests. Cap the max extra
 * space given at some small number of pages.
 **********************************************************/
size_t extra_realloc_size(size_t size) {

    size_t biggerBuffer = size * 4; 
    //Assuming one would want to realloc into a size class somewhat more than twice the previous size.
    //This is in line with common use cases such as extending an array (e.g. C++ vectors)

    //But we will clamp that extra amount in multiples of page size
    if (biggerBuffer > size+24576) { //currently at 6 pages
        biggerBuffer = size+24576;
    }

    return biggerBuffer;

}

/**********************************************************
 * coalesce
 * Covers the 4 cases discussed in the text:
 * - both neighbours are allocated
 * - the next block is available for coalescing
 * - the previous block is available for coalescing
 * - both neighbours are available for coalescing
 **********************************************************/
void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {       /* Case 1 */
        return bp;
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        return (bp);
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        return (PREV_BLKP(bp));
    }

    else {            /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)))  +
            GET_SIZE(FTRP(NEXT_BLKP(bp)))  ;
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        return (PREV_BLKP(bp));
    }
}

/**********************************************************
 * remove_from_list
 * Remove block pointer bp from its given segregated free 
 * list, using the size to determine which list.
 **********************************************************/
void remove_from_list (void *bp){
    int index = list_index(GET_SIZE(HDRP(bp)));
    if (EXP_PREV_BLKP(bp) != NULL) //never try setting a proporty on something that's NULL 
        EXP_SET_NEXT_BLKP(EXP_PREV_BLKP(bp),EXP_NEXT_BLKP(bp));
    else heap_listp[index] = EXP_NEXT_BLKP(bp);
    if (EXP_NEXT_BLKP(bp) != NULL) //never try setting a proporty on something that's NULL 
        EXP_SET_PREV_BLKP(EXP_NEXT_BLKP(bp), EXP_PREV_BLKP(bp));
    return;
}

/**********************************************************
 * add_to_list
 * Add block pointer bp to the appropriate segregated free
 * list, using the size to determine which list.
 **********************************************************/
void add_to_list (void *bp){
    int index = list_index(GET_SIZE(HDRP(bp)));
    EXP_SET_NEXT_BLKP(bp, heap_listp[index]); 
    EXP_SET_PREV_BLKP(bp, NULL); 
    if (heap_listp[index] != NULL) //never try setting a proporty on something that's NULL 
            EXP_SET_PREV_BLKP(heap_listp[index], bp);	 
    heap_listp[index] = bp; 
    return; 
}

/**********************************************************
 * explicit_add_lifo_coalesce
 * Added for explicit lists: add the newly freed block to the 
 * LIFO free list and coalesce as necessary (see 4 cases in lecture notes)
 **********************************************************/
void *explicit_add_lifo_coalesce(void *bp) {

    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    
    //case 1: both neighbors allocated
    if (prev_alloc && next_alloc) {
        add_to_list(bp);
    } 

    //case 2: next block available for coaslescing
    //following diagram in lecture 7 slide 54
    if (prev_alloc && !next_alloc) {
        void *next = NEXT_BLKP(bp); //the CONTIGUOUS next block, we're checking to make sure
        remove_from_list(next);
        bp = coalesce(bp);
        add_to_list(bp); 
    }

    //case 3: previous block available for coalescing
    //very similar procedure to case 2, see lecture 7 slide 55
    if (!prev_alloc && next_alloc) {
        void *prev = PREV_BLKP(bp);
        remove_from_list(prev);
        bp = coalesce(bp);
        add_to_list(bp);
    }

    //case 4: both blocks available for coalescing; lecture 7 slide 56
    //the most work of any of the cases because we may need to fix up 2 parts of the list
    //at prev and next (of physical contiguous blocks to bp)
    if (!prev_alloc && !next_alloc) {
        void *prev = PREV_BLKP(bp);
        void *next = NEXT_BLKP(bp);
        remove_from_list(prev);
        remove_from_list(next);
        bp = coalesce(bp);
        add_to_list(bp);
    }
    return bp;

}

/**********************************************************
 * explicit_find_list
 * For explicit lists: find the fit
 * Traverse the heap searching for a block to fit asize
 * Return NULL if no free blocks can handle that size
 * Assumed that asize is aligned
 **********************************************************/
void *explicit_find_fit(size_t asize) {

    int counter = 0; int threshold = 10; 
    //'threshold' is a small optimization that allows for a "pseudo best-fit" so that
    //LIFO lists can try doing better than always returning the first element.
    //This becomes important in larger-sized free lists where freeing and coalescing
    //patterns may create a very large initial block, requiring constant splicing and
    //re-insertions upon allocation.
    size_t lowest_diff = 9999999;
    void *bestSoFar = NULL;

    int index = list_index(asize);
    int i;
    for (i = index; i < num_segregated_lists; i++) { //start from index: if we don't find something in this 
        //size class, it can only be in a larger size class. Ideally could use size to just jump to the right
        //size class (TODO)
        void *bp = heap_listp[i]; //start from the beginning
        while (bp) { //go through the linked list
            size_t curr_block_size = GET_SIZE(HDRP(bp));
            if (!GET_ALLOC(HDRP(bp)) && (asize <= curr_block_size)) {
                counter++; 
                size_t diff = curr_block_size - asize;
                if (diff < lowest_diff) { 
                    lowest_diff = diff;
                    bestSoFar = bp;
                }

                if (counter > threshold)
                    return bestSoFar; 

            }
            bp  = EXP_NEXT_BLKP(bp);
        }
    }
    //Eventually if we don't find a fit, we will reach the end of the list, e.g. explicit list's next
    //will be NULL. Return NULL, find fit failed.
    return bestSoFar; 
}

/**********************************************************
 * explicit_print_list
 * FOR DEBUG PURPOSES ONLY
 **********************************************************/
void explicit_print_list() {

    printf("====== FREE LIST: =========\n");
    int i;
    for(i = 0; i < num_segregated_lists; i++) {
        printf("\n---Free list #%d\n",i);
        void *bp = heap_listp[i]; //start from the beginning
        while (bp) { //go through the linked list
            printf("curr_block: %p @size: %d <-->", bp, GET_SIZE(HDRP(bp)));
            bp  = EXP_NEXT_BLKP(bp);
        }
    }
    printf("======== </FREE_LIST> =======\n");

}

/**********************************************************
 * extend_heap
 * Extend the heap by "words" words, maintaining alignment
 * requirements of course. Free the former epilogue block
 * and reallocate its new header
 **********************************************************/
void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignments */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;

    if ( (bp = mem_sbrk(size)) == (void *)-1 )
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));                // free block header
    PUT(FTRP(bp), PACK(size, 0));                // free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));        // new epilogue header

    return bp;

}


/**********************************************************
 * place
 * Mark the block as allocated. Handles splicing of under-
 * allocated blocks, placing the smaller splice into the
 * appropriate free list.
 **********************************************************/
void place(void* bp, size_t asize, bool heapExtended)
{
    /* Get the current block size */
    size_t bsize = GET_SIZE(HDRP(bp));

    //TODO: fix next and prev pointers if heapExtended == false (i.e. we're removing an entry from free list)
    //printf("PLACE %p of size %d\n", bp, asize);
    if (heapExtended == true) {
        if (asize <= bsize-4*WSIZE) { //not sure about bounds
            //splice: actual size is less than the block size we were given
            //
            PUT(HDRP(bp), PACK(asize, 1));
            PUT(FTRP(bp), PACK(asize, 1));
            size_t excess_size = bsize - asize;
            void *new_spliced = NEXT_BLKP(bp);
            PUT(HDRP(new_spliced), PACK(excess_size, 0));
            PUT(FTRP(new_spliced), PACK(excess_size, 0)); 

            add_to_list(new_spliced);
        }
        else {
            //no splicing necessary, they are getting the whole block
            PUT(HDRP(bp), PACK(bsize, 1));
            PUT(FTRP(bp), PACK(bsize, 1));
		}
    } else { //found a block from the free list; need to re-do pointers, possibly splice
        //redo pointers:
		if (asize <= bsize-4*WSIZE) {
            //fix free list pointers when removing block:
            remove_from_list(bp);      
            PUT(HDRP(bp), PACK(asize, 1));
            PUT(FTRP(bp), PACK(asize, 1));
            //splice block
            size_t excess_size = bsize - asize;
            void *new_spliced = NEXT_BLKP(bp);

            PUT(HDRP(new_spliced), PACK(excess_size, 0));
            PUT(FTRP(new_spliced), PACK(excess_size, 0)); 
            //Put the newly spliced free block at front of the free list
            add_to_list(new_spliced);
        }
		else {
            PUT(HDRP(bp), PACK(bsize, 1));
            PUT(FTRP(bp), PACK(bsize, 1));
            remove_from_list(bp);
        }
	
    }
}

/**********************************************************
 * round_size
 * If the size is at least 85% of the way to a nice multiple 
 * of 2^N, return the nearest 2^N
 **********************************************************/
size_t round_size(size_t size) {

    if (size > 435 && size < 512)
        return 512;

    if (size > 870 && size < 1024)
        return 1024;

    if (size > 1741 && size < 2048)
        return 2048;

    if (size > 3482 && size < 4096)
        return 4096;

    if (size > 6963 && size < 8192)
        return 8192;

    return size;
}

/**********************************************************
 * create_extras
 * Create extra unallocated blocks of a certain asize.
 **********************************************************/
void create_extras(size_t asize, int numExtras) {

    size_t extendsize = MAX(asize, CHUNKSIZE);
    int i;
    for (i = 0; i < numExtras; i++) {
        void *bp;
        if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
            return; //can't do it boss

        add_to_list(bp);
    }

}

/**********************************************************
 * mm_free
 * Free the block and coalesce with neighbouring blocks
 **********************************************************/
void mm_free(void *bp)
{
    if(bp == NULL){
        return;
    }

    
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));

    explicit_add_lifo_coalesce(bp);

    //DEBUG ONLY
    //if (mm_check() == 0)
    //    printf("HEAP CONSISTENCY ERROR!\n");

}

/**********************************************************
 * mm_malloc
 * Allocate a block of size bytes.
 * The type of search is determined by find_fit
 * The decision of splitting the block, or not is determined
 *   in place(..)
 * If no block satisfies the request, the heap is extended
 **********************************************************/
void *mm_malloc(size_t size)
{
    size_t asize; /* adjusted block size */
    size_t extendsize; /* amount to extend heap if no fit */
    char * bp;

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    //Try adjusting the size to the nearest 2^N if it's at least 85% of the way there:
    size = round_size(size);

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = DSIZE + OVERHEAD;
    else
        asize = DSIZE * ((size + (OVERHEAD) + (DSIZE-1))/ DSIZE);
 
    /* Search the free list for a fit */
    if ((bp = explicit_find_fit(asize)) != NULL) { //TODO: change to use explicit_find_fit function
        place(bp, asize, false);
        return bp;
    }

    //if small size, try creating some extras:
    if (size <= 512) { //wont be affected by round_size()
            create_extras(asize, 7); //constants heuristically chosen
    }

    extendsize = MAX(asize, CHUNKSIZE);
    
    /* No fit found. Get more memory and place the block */
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize, true);

    return bp;

}

/**********************************************************
 * mm_realloc
 * Implemented simply in terms of mm_malloc and mm_free.
 * Additional optimizations: 
 * - if the size fits within ptr's current block, return the
 *   same block.
 * - if we can coalesce on the right and create a resultant
 *   block with enough size, do that
 *   (TODO - try coalescing on the left as well)
 * - if a new block really is needed, make it a few multiples
 *   bigger than requested to avoid wasting time in the likely
 *   case that realloc will be called again.
 *********************************************************/
void *mm_realloc(void *ptr, size_t size)
{
    /* If size == 0 then this is just free, and we return NULL. */
    if (size == 0){
        mm_free(ptr);
        return NULL;
    }

    //If the realloc'd block has previously been given more size than it needs, perhaps
    //this realloc request can be serviced within the same block:
    size_t curSize = GET_SIZE(HDRP(ptr));
    if (size < curSize-2*WSIZE) {
        return ptr;
    }
   
    void *next = NEXT_BLKP(ptr);
    int next_alloc = GET_ALLOC(HDRP(next));

    size_t coalesce_size = (GET_SIZE(HDRP(next)) + GET_SIZE(HDRP(ptr)));
    if (!next_alloc && size <= coalesce_size-2*WSIZE){
        remove_from_list(next);
        PUT(HDRP(ptr), PACK(coalesce_size, 1));
        PUT(FTRP(ptr), PACK(coalesce_size, 1));
        return ptr;
    }

    //Assuming realloc will be called again in the future, try giving a bigger block to use:
    size_t newSize = extra_realloc_size(size);
    
    /* If old ptr is NULL, then this is just malloc. */
    if (ptr == NULL)
        return (mm_malloc(newSize));

    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(newSize);
    if (newptr == NULL)
        return NULL;

    /* Copy the old data. */
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

/**********************************************************
 * mm_check
 * Check the consistency of the memory heap
 * Return nonzero if the heap is consistant.
 *********************************************************/
int mm_check(void){

    int i;
    for(i = 0; i < num_segregated_lists; i++) {
        void *bp = heap_listp[i]; //start from the beginning
        while (bp) { //go through the linked list

            //check - is the block marked as free?
            if (GET_ALLOC(HDRP(bp)) == 1 || GET_ALLOC(FTRP(bp)) == 1)
                return 0; //inconsistent, if in free list, should be marked free.

            //check - can we grab valid sizes from neighbouring contiguous blocks?
            size_t left_size = GET_SIZE(HDRP(PREV_BLKP(bp)));
            size_t right_size = GET_SIZE(HDRP(NEXT_BLKP(bp)));
            if (left_size < 0 || right_size < 0)
                return 0; //invalid size somewhere in our list

            bp  = EXP_NEXT_BLKP(bp);
        }
    }

    return 1; //made it to the end, heap consistent. 
}

