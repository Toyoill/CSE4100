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
#include "mm.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your information in the following struct.
 ********************************************************/
team_t team = {
    /* Your student ID */
    "20181669",
    /* Your full name*/
    "Woojin Lee",
    /* Your email address */
    "hs6113wj@gmail.com",
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/*********************/

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (unsigned int)(val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp + GET_SIZE(HDRP(bp))) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))

#define PREV_LINKP(bp) ((char *)(bp))
#define NEXT_LINKP(bp) ((char *)(bp) + WSIZE)

#define USER_TO_PACK(bp) ((char *)bp - DSIZE)
#define PACK_TO_USER(bp) ((char *)bp + DSIZE)

#define MAX_IDX 13

static char *heap_listp = 0;
static char *heap_freep = 0;

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
void REMOVE_LINK(void *bp);

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
  // printf("\nMM_INIT START\n");
  if ((heap_listp = mem_sbrk((MAX_IDX + 7) * WSIZE)) == (void *)-1) return -1;
  heap_freep = heap_listp;
  for (int i = 0; i < MAX_IDX + 1; i++) PUT(heap_listp + (i * WSIZE), 0);

  PUT(heap_listp + (MAX_IDX + 1), 0);
  PUT(heap_listp + ((MAX_IDX + 2) * WSIZE),
      PACK(2 * DSIZE, 1));                       // Prologue header
  PUT(heap_listp + ((MAX_IDX + 3) * WSIZE), 0);  // Prologue header
  PUT(heap_listp + ((MAX_IDX + 4) * WSIZE), 0);  // Prologue header
  PUT(heap_listp + ((MAX_IDX + 5) * WSIZE),
      PACK(2 * DSIZE, 1));  // Prolouge footer

  PUT(heap_listp + ((MAX_IDX + 6) * WSIZE), PACK(0, 1));  // Epilogue header

  heap_listp += ((MAX_IDX + 5) * WSIZE);

  if (extend_heap(CHUNKSIZE / WSIZE) == NULL) return -1;
  return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
  size_t asize;
  size_t extendsize;
  char *bp;
  if (size == 0) return NULL;
  if (size <= DSIZE)
    asize = 3 * DSIZE;
  else {
    asize = DSIZE * ((size + (2 * DSIZE) + (DSIZE - 1)) / DSIZE);
  }
  if ((bp = find_fit(asize)) != NULL) {
    place(bp, asize);
    return PACK_TO_USER(bp);
  }
  extendsize = MAX(asize, CHUNKSIZE);
  if ((bp = extend_heap(extendsize / WSIZE)) == NULL) return NULL;
  place(bp, asize);
  return PACK_TO_USER(bp);
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp) {
  bp = USER_TO_PACK(bp);
  size_t size = GET_SIZE(HDRP(bp));
  PUT(HDRP(bp), PACK(size, 0));
  PUT(FTRP(bp), PACK(size, 0));
  coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) {
  size_t oldsize;
  void *newptr;

  if (size == 0) {
    mm_free(ptr);
    return 0;
  }

  if (ptr == NULL) {
    return mm_malloc(size);
  }

  newptr = mm_malloc(size);

  if (!newptr) {
    return 0;
  }

  newptr = USER_TO_PACK(newptr);
  ptr = USER_TO_PACK(ptr);

  oldsize = GET_SIZE(HDRP(ptr));
  if (size < oldsize) oldsize = size;
  memcpy(newptr, ptr, oldsize + 2 * WSIZE);

  mm_free(PACK_TO_USER(ptr));

  return PACK_TO_USER(newptr);
}

/* helper functions */
static void *extend_heap(size_t words) {
  // printf("extend_heap START\n");
  char *bp;
  size_t size;

  size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
  if ((long)(bp = mem_sbrk(size)) == -1) {
    return NULL;
  }

  PUT(HDRP(bp), PACK(size, 0));
  PUT(PREV_LINKP(bp), 0);
  PUT(NEXT_LINKP(bp), 0);
  PUT(FTRP(bp), PACK(size, 0));

  PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

  return coalesce(bp);
}

static void *coalesce(void *bp) {
  // printf("coalesce start\n");
  void *free_idx_ptr;
  int idx = -1;

  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));

  REMOVE_LINK(bp);

  if (prev_alloc && next_alloc) {
    while (size > 8 && idx <= MAX_IDX) {
      size >>= 1;
      idx++;
    }
    if (idx > MAX_IDX) idx = MAX_IDX;
    free_idx_ptr = heap_freep + (WSIZE * idx);

    PUT(NEXT_LINKP(bp), GET(free_idx_ptr));
    PUT(PREV_LINKP(bp), free_idx_ptr);
    PUT(free_idx_ptr, bp);
    if (GET(NEXT_LINKP(bp))) PUT(PREV_LINKP(GET(NEXT_LINKP(bp))), bp);

    return bp;
  } else if (prev_alloc && !next_alloc) {
    REMOVE_LINK(NEXT_BLKP(bp));

    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
  } else if (!prev_alloc && next_alloc) {
    REMOVE_LINK(PREV_BLKP(bp));

    size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
  } else {
    REMOVE_LINK(NEXT_BLKP(bp));
    REMOVE_LINK(PREV_BLKP(bp));

    size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
  }

  while (size > 8 && idx <= MAX_IDX) {
    size >>= 1;
    idx++;
  }
  if (idx > MAX_IDX) idx = MAX_IDX;
  free_idx_ptr = heap_freep + (WSIZE * idx);
  PUT(NEXT_LINKP(bp), GET(free_idx_ptr));
  PUT(PREV_LINKP(bp), free_idx_ptr);
  PUT(free_idx_ptr, bp);
  if (GET(NEXT_LINKP(bp))) PUT(PREV_LINKP(GET(NEXT_LINKP(bp))), bp);
  return bp;
}

static void *find_fit(size_t asize) {
  int size = asize, idx = -1;
  void *bp;
  while (size > 8 && idx <= MAX_IDX) {
    size >>= 1;
    idx++;
  }
  if (idx > MAX_IDX) idx = MAX_IDX;

  while (idx <= MAX_IDX) {
    if ((bp = (char *)GET(heap_freep + WSIZE * idx))) {
      while (bp && asize > GET_SIZE(HDRP(bp))) {
        bp = (char *)GET(NEXT_LINKP(bp));
      }
      if (bp) return bp;
    }
    idx++;
  }
  return NULL;
}

static void place(void *bp, size_t asize) {
  // printf("place start\n");
  size_t csize = GET_SIZE(HDRP(bp));
  void *free_idx_ptr;
  int new_size, idx = -1;

  if ((csize - asize) >= (2 * DSIZE)) {
    PUT(HDRP(bp), PACK(asize, 1));
    REMOVE_LINK(bp);
    PUT(FTRP(bp), PACK(asize, 1));

    bp = NEXT_BLKP(bp);
    new_size = csize - asize;
    PUT(HDRP(bp), PACK(new_size, 0));
    PUT(FTRP(bp), PACK(new_size, 0));

    while (new_size > 8 && idx <= MAX_IDX) {
      new_size >>= 1;
      idx++;
    }
    if (idx > MAX_IDX) idx = MAX_IDX;
    free_idx_ptr = heap_freep + (WSIZE * idx);
    PUT(NEXT_LINKP(bp), GET(free_idx_ptr));
    PUT(PREV_LINKP(bp), free_idx_ptr);
    PUT(free_idx_ptr, bp);
    if (GET(NEXT_LINKP(bp))) PUT(PREV_LINKP(GET(NEXT_LINKP(bp))), bp);
  } else {
    REMOVE_LINK(bp);
    PUT(HDRP(bp), PACK(csize, 1));
    PUT(FTRP(bp), PACK(csize, 1));
  }
  if (bp == (char *)GET(NEXT_LINKP(bp))) {
    getchar();
  }
}

void REMOVE_LINK(void *bp) {
  // printf("remove_link start\n");
  void *prev_ptr, *next_ptr;
  prev_ptr = (char *)GET(PREV_LINKP(bp));
  next_ptr = (char *)GET(NEXT_LINKP(bp));
  if (prev_ptr) {
    if ((char *)prev_ptr < (char *)heap_listp)
      PUT(prev_ptr, next_ptr);
    else
      PUT(NEXT_LINKP(prev_ptr), next_ptr);
  }
  if (next_ptr) {
    PUT(PREV_LINKP(next_ptr), prev_ptr);
  }
  PUT(PREV_LINKP(bp), 0);
  PUT(NEXT_LINKP(bp), 0);
}