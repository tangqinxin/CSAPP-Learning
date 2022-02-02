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
    "tommy's team",
    /* First member's full name */
    "tommy",
    /* First member's email address */
    "448373336@qq.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};


/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12) /* Extend heap by this amount (bytes) */
#define MINBLOCKSIZE 16
#define MIN_BLOCK_SIZE 16

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc)) /* Pack a size and allocated bit into a word */

#define GET(p) (*(unsigned int *)(p)) /* read a word at address p */
#define PUT(p, val) (*(unsigned int *)(p) = (val)) /* write a word at address p */

#define GET_SIZE(p) (GET(p) & ~0x7) /* read the size field from address p */
#define GET_ALLOC(p) (GET(p) & 0x1) /* read the alloc field from address p */

#define HDRP(bp) ((char*)(bp) - WSIZE) /* given block ptr bp, compute address of its header */
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) /* given block ptr bp, compute address of its footer */

#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp))) /* given block ptr bp, compute address of next blocks */
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE((char*)(bp)-DSIZE)) /* given block ptr bp, compute address of prev blocks */

static void* extend_heap(size_t words);
static void* coalesce(void *bp);
static void* first_fit(size_t asize);
static void* next_fit(size_t size);
// static void* best_fit(size_t size);
static void place(void* bp, size_t asize);
void mm_check(const char *function);

static char* heap_listp;
static char* pre_listp;

void mm_check(const char *function)
{
    printf("start:%x\n",(unsigned int)heap_listp);
    return;
   printf("---cur function:%s empty blocks:\n",function);
   void* tmpP = heap_listp;
   int count_empty_block = 0;
   int curBlockIndex = 0;
   while(GET_SIZE(HDRP(tmpP)) != 0)
   {
    //    if(GET_ALLOC(HDRP(tmpP))!=0) {
    //         count_empty_block++;
    //    }
    //printf("address：%x size:%d alloc:%d\n", (unsigned int)tmpP, (int)GET_SIZE(HDRP(tmpP)), (int)GET_ALLOC(HDRP(tmpP)));
       unsigned int curhead = tmpP;
       unsigned int curtail = tmpP + GET_SIZE(HDRP(tmpP)) - DSIZE -1;
    //    unsigned int nexthead = (unsigned int)HDRP(NEXT_BLKP(tmpP));
    //    unsigned int nexttail = (unsigned int)FTRP(NEXT_BLKP(tmpP));
    //    if(nexthead != (curtail+4)){
    //        printf("curaddress: %x nextaddress: %x\n",curhead, nexthead);
    //        debug_ok = 0; // break the debuger
    //    }
       printf("Index:%d head:%x tail: %x size:%d asize:%d\n",curBlockIndex, curhead, curtail, GET_SIZE(HDRP(tmpP)) - DSIZE, curtail - curhead+1);
       tmpP = NEXT_BLKP(tmpP);
       curBlockIndex++;
       if(curBlockIndex>80)break;
   }
//    printf("empty_block num: %d\n",count_empty_block);
}

static void* coalesce(void* bp){
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    // case 1
    if(prev_alloc&&next_alloc){
        pre_listp = (char*)bp;
        return bp;
    }
    // case 2
    else if(prev_alloc&&!next_alloc){
        size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp),PACK(size + next_size, 0));
        PUT(FTRP(bp),PACK(size + next_size, 0));
        pre_listp = (char*)bp;
        return bp;
    }
    // case 3
    else if(!prev_alloc&&next_alloc){
        size_t prev_size=GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)),PACK(size + prev_size, 0)); // 2 head then foot?
        PUT(FTRP(PREV_BLKP(bp)),PACK(size + prev_size, 0));
        bp = PREV_BLKP(bp);
    }
    // case 4
    else {
        size_t prev_size=GET_SIZE(HDRP(PREV_BLKP(bp)));
        size_t next_size=GET_SIZE(HDRP(NEXT_BLKP(bp)));
        size_t total=size+prev_size+next_size;
        PUT(HDRP(PREV_BLKP(bp)),PACK(total, 0));
        PUT(FTRP(NEXT_BLKP(bp)),PACK(total, 0));
        bp = PREV_BLKP(bp);
    }

    pre_listp = (char*)bp;
    return bp;
}

static void* extend_heap(size_t words) {
    char* bp;
    size_t size;
    // 1 deal with the block size for the 
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1){
        return NULL;
    }

    // 2 deal with the header for the block
    PUT(HDRP(bp), PACK(size, 0));
    // 3 deal with the footer for the block
    PUT(FTRP(bp), PACK(size, 0));
    // 4 add the end tag 
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
    // // 5 coalesce the blank block
    return coalesce(bp);
}

static void* first_fit(size_t size){
    for(void* curbp = heap_listp; GET_SIZE(HDRP(curbp))>0; curbp = NEXT_BLKP(curbp)){
        if(GET_ALLOC(HDRP(curbp)) == 0 && GET_SIZE(HDRP(curbp)) >= size){
            return curbp;
        }
    }
    
    return NULL;
}

static void* next_fit(size_t size) {
    for(void* curbp = pre_listp; GET_SIZE(HDRP(curbp))>0; curbp = NEXT_BLKP(curbp)){
        if(GET_ALLOC(HDRP(curbp)) == 0 && GET_SIZE(HDRP(curbp)) >= size){
            pre_listp = curbp; // record the last position
            return curbp;
        }
    }

    for(void* curbp = heap_listp; curbp != pre_listp; curbp = NEXT_BLKP(curbp)){
        if(GET_ALLOC(HDRP(curbp)) == 0 && GET_SIZE(HDRP(curbp)) >= size){
            pre_listp = curbp; // record the last position
            return curbp;
        }
    }
    
    return NULL;
}

static void place(void* bp, size_t size) {
    size_t curBlockSize = GET_SIZE(HDRP(bp));
    if(GET_ALLOC(HDRP(bp)) != 0 || curBlockSize < size) {
        printf("the block to fit is allocated or size is NOT enough\n");
    }
    if(curBlockSize - size >= 2 * DSIZE) {
        // 1 place the header
        PUT(HDRP(bp), PACK(size, 1));
        // 2 place the footer, since the footer will change with the header
        PUT(FTRP(bp), PACK(size, 1));
        // 3 place the next block footer
        PUT(HDRP(NEXT_BLKP(bp)), PACK(curBlockSize - size, 0));
        // 4 place the next block header
        PUT(FTRP(NEXT_BLKP(bp)), PACK(curBlockSize - size, 0)); 
    } else {
        // if left size is less than DSIZE, we align all the left size in the block
        // 1 place the header
        PUT(HDRP(bp), PACK(curBlockSize, 1));
        // 2 place the footer, since the footer will change with the header
        PUT(FTRP(bp), PACK(curBlockSize, 1));
    }
    // pre_listp = (char*)bp; // Question1.seems not
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    heap_listp = mem_sbrk(4*WSIZE);
    if(heap_listp == (void*)(-1)){
        printf("set mem_sbrk failed in mm_init\n");
        return -1;
    }
    
    PUT(heap_listp + WSIZE,PACK(DSIZE, 1));
    PUT(heap_listp + 2 * WSIZE,PACK(DSIZE, 1));
    PUT(heap_listp + 3 * WSIZE,PACK(0, 1));
    heap_listp += 2*WSIZE;
    pre_listp = heap_listp;

    if(extend_heap(CHUNKSIZE/WSIZE)==NULL){
        printf("extend_heap failed in mm_init\n");
        return -1;
    }
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    // printf("malloc size:%u\n",size);
    void* bp = NULL;
    if(size == 0){
        return NULL;
    }
    size_t asize = size;
    if(asize <= DSIZE){
        asize = 2*DSIZE;
    } else {
        // the size should round to the upper 8 Bytes
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE) ;
    }

    if((bp = next_fit(asize)) != NULL){
        place(bp, asize);
        return bp;
    }

    size_t extendsize = MAX(asize, CHUNKSIZE);
    // extend the space and find some place to place the bp
    if((bp = extend_heap(extendsize/WSIZE))!=NULL) {
        place(bp, asize);
        return bp;
    } else {
        return NULL;
    }
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
    size = GET_SIZE(HDRP(oldptr));
    copySize = GET_SIZE(HDRP(newptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize-WSIZE);
    mm_free(oldptr);
    return newptr;
}














