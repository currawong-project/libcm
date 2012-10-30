#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmLinkedHeap.h"
#include "cmMallocDebug.h"

typedef struct cmLhBlock_str
{
  char*                 basePtr;     // base of block
  char*                 nextPtr;     // next avail location in block
  char*                 endPtr;      // one past end of block
  struct cmLhBlock_str* prevBlkPtr;  // backward block link
  struct cmLhBlock_str* nextBlkPtr;  // forward block link
  unsigned              freeCnt;     // track orphaned space that is unavailable for reuse
} cmLhBlock_t;

typedef struct
{
  unsigned      dfltBlockByteCnt; // size of each block in chain
  cmLhBlock_t*  first;            // first block in chain
  cmLhBlock_t*  last;             // last block in chain
  cmMmH_t       mmH;
} cmLHeap_t;

cmLHeapH_t cmLHeapNullHandle = { NULL };

cmLHeap_t* _cmLHeapHandleToPtr( cmLHeapH_t h )
{
  cmLHeap_t* lhp = (cmLHeap_t*)h.h;
  assert( lhp != NULL);
  return lhp;
}

cmLhBlock_t* _cmLHeapAllocBlock( cmLHeap_t* lhp, unsigned blockByteCnt )
{
  // note: the entire block (control record and data space) is allocated
  // as one contiguous chunk of memory.

  cmLhBlock_t* lbp = (cmLhBlock_t*)cmMemMallocZ( sizeof(cmLhBlock_t) + blockByteCnt );

  // setup the new block
  lbp->basePtr    = (char*)(lbp+1);
  lbp->nextPtr    = lbp->basePtr;
  lbp->endPtr     = lbp->basePtr + blockByteCnt;
  lbp->prevBlkPtr = lhp->last;
  lbp->nextBlkPtr = NULL;

  // link the new block into the chain
  if( lhp->last != NULL )
    lhp->last->nextBlkPtr = lbp;
  else
  {
    assert( lhp->first == NULL );
    lhp->first = lbp;
  }

  lhp->last = lbp;

  return lbp;
}

void*      _cmLHeapAlloc(  cmLHeap_t* lhp, unsigned dataByteCnt )
{
  void*        retPtr       = NULL;
  cmLhBlock_t* lbp          = lhp->last;
  unsigned     allocByteCnt = dataByteCnt + sizeof(unsigned); 

  // go backwards down the chain looking for the first block with enough
  // free space to hold the allocation (we go backward under the assumption
  // that the last block is most likely to have available space)
  while( (lbp != NULL) && ((lbp->endPtr - lbp->nextPtr) < allocByteCnt) )
    lbp = lbp->prevBlkPtr; 

  // no space large enough to provide the requested memory - allocate a new block
  if( lbp == NULL )
    lbp = _cmLHeapAllocBlock(lhp, cmMax( lhp->dfltBlockByteCnt, allocByteCnt ));

  assert( lbp != NULL );

  // store the sizeof the allocation at the beginning of the allocated block
  *(unsigned*)lbp->nextPtr  = allocByteCnt;

  // the returned block ptr begins just after the block size
  retPtr        = lbp->nextPtr + sizeof(unsigned);

  
  lbp->nextPtr += allocByteCnt;

  return retPtr;
}


void* _cmLHeapAllocCb(void* funcArgPtr, unsigned byteCnt)
{
  cmLHeap_t* p = (cmLHeap_t*)funcArgPtr;
  assert( p != NULL );
  return _cmLHeapAlloc(p,byteCnt);
}

bool     _cmLHeapFree(    cmLHeap_t* lhp, void* dataPtr )
{ 
  if( dataPtr == NULL )
    return true;

  cmLhBlock_t* lbp = lhp->first;

  // locate the block containing the area to free
  while( (lbp != NULL ) && (((char*)dataPtr < lbp->basePtr) || ((char*)dataPtr >= lbp->endPtr)))
    lbp = lbp->nextBlkPtr;

  // the pointer must be in one of the blocks
  if( lbp == NULL )
    return false;

  unsigned* allocPtr     = ((unsigned*)dataPtr)-1;
  unsigned  dataByteCnt  = *allocPtr - sizeof(unsigned);

  // the data to be freed is at the end of the blocks space ...
  if( dataPtr == lbp->nextPtr - dataByteCnt )
    lbp->nextPtr = (char*)allocPtr; // ... then make it the space to alloc
  else           
    lbp->freeCnt += *allocPtr; // ... otherwise increase the free count
  //(freeCnt tracks unused space that is not at the end of the block and therefore cannot be reused.)

  // if all the space for this block has been freed then the
  // next space to allocate must be at the base
  if( lbp->freeCnt == lbp->endPtr - lbp->basePtr )
    lbp->nextPtr = lbp->basePtr;

  return true;
  
}


bool  _cmLHeapFreeCb(void* funcArgPtr, void* ptr)
{
  cmLHeap_t*   lhp = (cmLHeap_t*)funcArgPtr;
  return _cmLHeapFree(lhp,ptr);
}


cmLHeapH_t cmLHeapCreate( unsigned dfltBlockByteCnt, cmCtx_t* ctx )
{
  cmLHeapH_t h;
  cmLHeap_t* lhp = cmMemAllocZ( cmLHeap_t, 1 );

  // We are not going to defer freeing each allocation  because it will result in using 
  // a lot of memory.  Commenting out this line however would result in
  // checking all allocations for corruption during cmLHeapDestroy().
  // This may be a good way to hunt down subtle memory corruption problems.
  // See cmLHeapDestroy() and cmLHeapClear() for kDeferFreeMMFl related 
  // items.
  unsigned mmFlags = cmClrFlag(ctx->mmFlags,kDeferFreeMmFl);

  if( cmMmInitialize(&lhp->mmH,_cmLHeapAllocCb,_cmLHeapFreeCb,lhp,ctx->guardByteCnt,ctx->alignByteCnt,mmFlags,&ctx->rpt) != kOkMmRC )
    return cmLHeapNullHandle;
      
  lhp->dfltBlockByteCnt = dfltBlockByteCnt;

  _cmLHeapAllocBlock( lhp, lhp->dfltBlockByteCnt );

  h.h = lhp;
  return h;
}

void cmLHeapDestroy( cmLHeapH_t* hp )
{
  if( hp==NULL || hp->h == NULL )
    return;

  cmLHeap_t* lhp = _cmLHeapHandleToPtr(*hp);

  // check for corruption
  if( cmIsFlag(cmMmInitializeFlags(lhp->mmH),kDeferFreeMmFl))
    cmMmReport(lhp->mmH, kSuppressSummaryMmFl | kIgnoreLeaksMmFl | kIgnoreNormalMmFl );

  cmMmFinalize(&lhp->mmH); 
  
  cmLhBlock_t* lbp = lhp->first;
  while( lbp != NULL )
  {
    cmLhBlock_t* t = lbp;

    lbp = lbp->nextBlkPtr;

    cmMemFree(t);
  }

  cmMemPtrFree(&hp->h);
  
}

void*      cmLHeapAllocate(cmLHeapH_t h, void* orgDataPtr, unsigned eleCnt, unsigned eleByteCnt, unsigned flags, const char* fileStr, const char* funcStr, unsigned fileLine )
{ return cmMmAllocate(_cmLHeapHandleToPtr(h)->mmH,orgDataPtr,eleCnt,eleByteCnt,flags,fileStr,funcStr,fileLine);  }

cmChar_t*  cmLHeapAllocStr(cmLHeapH_t h, void* orgDataPtr, const cmChar_t*  str, unsigned charCnt, unsigned flags, const char* fileStr, const char* funcStr, unsigned fileLine )
{
  if( str == NULL )
    return NULL;

  unsigned  n  = charCnt + 1;
  cmChar_t* cp = cmLHeapAllocate(h, orgDataPtr, n, sizeof(cmChar_t), flags, fileStr, funcStr, fileLine );
  strncpy(cp,str,n);
  cp[n-1] = 0;
  return cp;
}

void       cmLHeapFree(    cmLHeapH_t h, void* dataPtr )
{ 
  cmLHeap_t*   lhp = _cmLHeapHandleToPtr(h);
  cmMmFree(lhp->mmH,dataPtr);
}

void       cmLHeapFreeDebug(    cmLHeapH_t h, void* dataPtr,  const char* fileName, const char* funcName, unsigned fileLine )
{
  cmLHeap_t*   lhp = _cmLHeapHandleToPtr(h);
  cmMmFreeDebug(lhp->mmH,dataPtr,fileName,funcName,fileLine); 
}


void       cmLHeapFreePtr( cmLHeapH_t h, void** ptrPtr )
{  
  assert( ptrPtr != NULL );
  cmLHeapFree(h,*ptrPtr);
  *ptrPtr = NULL;
}

void       cmLHeapFreePtrDebug(    cmLHeapH_t h, void** ptrPtr,  const char* fileName, const char* funcName, unsigned fileLine )
{ 
  assert( ptrPtr != NULL );
  cmLHeapFreeDebug(h,*ptrPtr,fileName,funcName,fileLine);
  *ptrPtr = NULL;
}


unsigned   cmLHeapDefaultBlockByteCount( cmLHeapH_t h )
{
  cmLHeap_t* p = _cmLHeapHandleToPtr(h);
  return p->dfltBlockByteCnt;
}

unsigned   cmLHeapGuardByteCount(  cmLHeapH_t h )
{
  cmLHeap_t* p = _cmLHeapHandleToPtr(h);
  return cmMmGuardByteCount( p->mmH );
}

unsigned   cmLHeapAlignByteCount(  cmLHeapH_t h )
{
  cmLHeap_t* p = _cmLHeapHandleToPtr(h);
  return cmMmAlignByteCount( p->mmH );
}

unsigned   cmLHeapInitializeFlags( cmLHeapH_t h )
{
  cmLHeap_t* p = _cmLHeapHandleToPtr(h);
  return cmMmInitializeFlags( p->mmH );
}

bool cmLHeapIsValid( cmLHeapH_t h )
{ return h.h != NULL; }

void       cmLHeapClear(   cmLHeapH_t h, bool releaseFl )
{
  cmLHeap_t* p = _cmLHeapHandleToPtr(h);

  // If we are deferring freeing memory until the heap is destroyed
  // then we cannot clear the block list in this function.
  // If the block list was cleared here then later calls to _cmLHeapFreeCb()
  // would fail because the pointers to the deferred allocations
  // would no longer exist in any of the blocks.
  if(   cmIsFlag(cmMmInitializeFlags(p->mmH),kDeferFreeMmFl))
    return;
  
  cmLhBlock_t* bp = p->first;
  while( bp != NULL )
  {
    bp->nextPtr = bp->basePtr;
    bp->freeCnt = bp->endPtr - bp->basePtr;
    
    cmLhBlock_t* nbp = bp->nextBlkPtr;

    if( releaseFl )
      cmMemFree(bp);

    bp = nbp;
  }

  if( releaseFl )
  {
    p->first = NULL;
    p->last  = NULL;
  }
}

cmMmRC_t cmLHeapReportErrors( cmLHeapH_t h, unsigned mmFlags )
{
  cmLHeap_t*   lhp = _cmLHeapHandleToPtr(h);
  return cmMmReport(lhp->mmH, mmFlags );
}

void cmLHeapReport( cmLHeapH_t h )
{
  cmLHeap_t*   lhp = _cmLHeapHandleToPtr(h);
  cmLhBlock_t* lbp = lhp->first;
  unsigned     i;
  for(i=0; lbp != NULL; ++i )
  {
    printf("%u : %li available  %i free\n", i, lbp->endPtr-lbp->nextPtr, lbp->freeCnt );
    lbp = lbp->nextBlkPtr;
  }
  printf("\n");
}

void cmLHeapTestPrint( void* userPtr, const char* text )
{
  fputs(text,stdin);
}

void cmLHeapTest()
{
  int      i            = 0;
  int      n            = 5;
  unsigned guardByteCnt = 8;
  unsigned alignByteCnt = 16;
  unsigned mmFlags      = kTrackMmFl; // | kDeferFreeMmFl;
  cmCtx_t  ctx;
  cmCtxSetup(&ctx,"Heap Test",cmLHeapTestPrint,cmLHeapTestPrint,NULL,guardByteCnt,alignByteCnt,mmFlags);

  cmLHeapH_t h = cmLHeapCreate( 16, &ctx );
  
  void* pArray[ n ];

  cmLHeapReport(h);

  for(i=0; i<n; ++i)
  {
    unsigned byteCnt = (i+1) * 2;
    printf("Allocating:%li\n",byteCnt+sizeof(unsigned));
    pArray[i] = cmLHeapAlloc(h,byteCnt);

    cmLHeapReport(h);
  }

  for(i=n-1; i>=0; i-=2)
  {
    printf("Freeing:%li\n",((i+1) * 2) + sizeof(unsigned));
    cmLHeapFree(h,pArray[i]);
    cmLHeapReport(h);
  }

  cmLHeapReportErrors(h,0);

  cmLHeapDestroy(&h);
}
