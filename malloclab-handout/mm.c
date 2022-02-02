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

//获得块中记录后继和前驱的地址. it refers to the pointer where store the prevblock and next block
#define PRED(bp) ((char*)(bp) + WSIZE)
#define SUCC(bp) ((char*)bp)
//获得块的后继和前驱的地址. it refers to the prevblock and next block in the segment list
#define PRED_BLKP(bp) (GET(PRED(bp)))
#define SUCC_BLKP(bp) (GET(SUCC(bp)))

static void* extend_heap(size_t words);
static void* coalesce(void *bp);
static void* first_fit(size_t asize);
static void* next_fit(size_t size);
static void place(void* bp, size_t asize);
static void delete_block(void* bp);
static void* add_block(void* bp);
static int Index(size_t size);
static void *LIFO(void *bp, void *root);
void mm_check(const char *function);

static char* heap_listp;
static char* listp;

void mm_check(const char *function)
{
}

static void delete_block(void* bp) {
	void* prev_blk_in_segmentlist = PRED_BLKP(bp);
	void* next_blk_in_segmentlist = SUCC_BLKP(bp);
	// deal with the prev block's next pointer
	PUT(SUCC(prev_blk_in_segmentlist), next_blk_in_segmentlist); // block has exist, so there must be a previous block connect to current block
	// deal with the next block's previous pointer
	
	if (next_blk_in_segmentlist != NULL) {
		PUT(PRED(next_blk_in_segmentlist), prev_blk_in_segmentlist);
	}
}

static int Index(size_t size) {
	int ind = 0;
	if (size >= 4096)
		return 8;

	size = size >> 5;
	while (size) {
		size = size >> 1;
		ind++;
	}
	return ind;
}

static void *LIFO(void *bp, void *root) {
	// 1 deal with the node after the root
	if (SUCC_BLKP(root) != NULL) {
		PUT(PRED(SUCC_BLKP(root)), bp);	//SUCC->BP
		PUT(SUCC(bp), SUCC_BLKP(root));	//BP->SUCC
	}
	else {
		// 如果没有下一个节点，那么插入节点的next要置为NULL
		PUT(SUCC(bp), NULL);
	}
	// 2 deal with the relation between root and insert node
	// SUCC就是指向bp本身的指针，因此这里本身就指向了表头节点的位置，直接填充进去
	PUT(SUCC(root), bp);	//ROOT->BP
	PUT(PRED(bp), root);	//BP->ROOT
	return bp;
}

static void* add_block(void* bp) {
	size_t size = GET_SIZE(HDRP(bp));
	int index = Index(size);
	void *root = listp + index*WSIZE;

	//LIFO
	return LIFO(bp, root);
}

static void* coalesce(void* bp){
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    // case 1
    if(prev_alloc&&next_alloc){
        return bp;
    }
    // case 2
    else if(prev_alloc&&!next_alloc){
        size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(bp)));
		// delete block in the blank segment list
		delete_block(NEXT_BLKP(bp));
        PUT(HDRP(bp),PACK(size + next_size, 0));
        PUT(FTRP(bp),PACK(size + next_size, 0));
        return bp;
    }
    // case 3
    else if(!prev_alloc&&next_alloc){
        size_t prev_size=GET_SIZE(HDRP(PREV_BLKP(bp)));
		// delete block in the blank segment list
		delete_block(PREV_BLKP(bp));
        PUT(HDRP(PREV_BLKP(bp)),PACK(size + prev_size, 0));
        PUT(FTRP(PREV_BLKP(bp)),PACK(size + prev_size, 0));
        bp = PREV_BLKP(bp);
    }
    // case 4
    else {
        size_t prev_size=GET_SIZE(HDRP(PREV_BLKP(bp)));
        size_t next_size=GET_SIZE(HDRP(NEXT_BLKP(bp)));
        size_t total=size+prev_size+next_size;
		// delete block in the blank segment list
		delete_block(NEXT_BLKP(bp));
		delete_block(PREV_BLKP(bp));
        PUT(HDRP(PREV_BLKP(bp)),PACK(total, 0));
        PUT(FTRP(NEXT_BLKP(bp)),PACK(total, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

static void* extend_heap(size_t words) {
    char* bp = NULL;
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
	// 5 add NULL pointer at the prev pointer and next pointer
	PUT(SUCC(bp), NULL);
	PUT(PRED(bp), NULL);
    // 6 coalesce the blank block
	bp = coalesce(bp);
	bp = add_block(bp);
	return bp;
}

static void* first_fit(size_t asize){
	// my ans
	int index = Index(asize);
	void* cur = NULL;
	while (index <= 8) {
		for (cur = listp + index * WSIZE; (cur=SUCC_BLKP(cur)) != NULL;) {
			if (GET_SIZE(HDRP(cur)) >= asize && !GET_ALLOC(HDRP(cur))) {
				return cur;
			}
		}
		index++;
	}
    return NULL;
}

static void place(void* bp, size_t size) {
    size_t curBlockSize = GET_SIZE(HDRP(bp));
    if(GET_ALLOC(HDRP(bp)) != 0 || curBlockSize < size) {
        printf("the block to fit is allocated or size is NOT enough\n");
    }
    if(curBlockSize - size >= 2 * DSIZE) {
		// 1 先将整个大的块从链表中删去，然后再在大块中加tag分割，然后将小块插入空闲链表
		delete_block(bp);
        // 2 place the header
        PUT(HDRP(bp), PACK(size, 1));
        // 3 place the footer, since the footer will change with the header
        PUT(FTRP(bp), PACK(size, 1));
        // 4 place the next block footer
        PUT(HDRP(NEXT_BLKP(bp)), PACK(curBlockSize - size, 0));
        // 5 place the next block header
        PUT(FTRP(NEXT_BLKP(bp)), PACK(curBlockSize - size, 0)); 
		// 6 fill the prev pointer and next pointer
		PUT(SUCC(bp), NULL);
		PUT(PRED(bp), NULL);
		// 7 add the next block to segment list
		add_block(NEXT_BLKP(bp));
    } else {
        // if remain size is less than DSIZE, we align all the left size in the block
		// 1 delete the whole block, here we use the whole block and NO add block since no space remain
		delete_block(bp);
        // 2 place the header
        PUT(HDRP(bp), PACK(curBlockSize, 1));
        // 3 place the footer, since the footer will change with the header
        PUT(FTRP(bp), PACK(curBlockSize, 1));
    }
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
	heap_listp = mem_sbrk(12 * WSIZE);
    if(heap_listp == (void*)(-1)){
        printf("set mem_sbrk failed in mm_init\n");
        return -1;
    }

    PUT(heap_listp + 0 * WSIZE,NULL); // 16~31, minimum block is 16
    PUT(heap_listp + 1 * WSIZE,NULL); // 32~63
    PUT(heap_listp + 2 * WSIZE,NULL);
    PUT(heap_listp + 3 * WSIZE,NULL);
    PUT(heap_listp + 4 * WSIZE,NULL);
    PUT(heap_listp + 5 * WSIZE,NULL);
    PUT(heap_listp + 6 * WSIZE,NULL);
    PUT(heap_listp + 7 * WSIZE,NULL);
    PUT(heap_listp + 8 * WSIZE,NULL); // 4096~inf
    PUT(heap_listp + 9 * WSIZE,PACK(DSIZE, 1));
    PUT(heap_listp + 10 * WSIZE,PACK(DSIZE, 1));
    PUT(heap_listp + 11 * WSIZE,PACK(0, 1));
	listp = heap_listp;
	heap_listp = heap_listp + 10 * WSIZE;
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

    if((bp = first_fit(asize)) != NULL){
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
    ptr = coalesce(ptr); // 这里必须要更新合并后的块，合并后必须利用返回值对ptr进行更新！不然会出错！
	add_block(ptr); // 这里必须是添加一个合并后的块
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
    memmove(newptr, oldptr, copySize-WSIZE);
    mm_free(oldptr);
    return newptr;
}











