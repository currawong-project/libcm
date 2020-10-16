//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmStack.h"

typedef struct cmStkBlk_str
{
  char*                base;
  unsigned             curEleCnt;
  unsigned             maxEleCnt;
  struct cmStkBlk_str* link;
} cmStkBlk_t;

typedef struct
{
  cmErr_t     err;
  unsigned    expandBlkEleCnt;  // count of ele's in all blocks after the first block
  unsigned    cnt;              // cur total count of elements in all blocks
  cmStkBlk_t* baseBlkPtr;       // first block    
  cmStkBlk_t* curBlkPtr;        // current block - all blocks are always full except for the current block
  unsigned    eleByteCnt;       // bytes per element
} cmStack_t;

cmStackH_t cmStackNullHandle = cmSTATIC_NULL_HANDLE;

cmStack_t* _cmStHandleToPtr( cmStackH_t h )
{
  cmStack_t* p = (cmStack_t*)h.h;
  assert( p != NULL );
  return p;
}


cmStkBlk_t* _cmStkAllocBlk( cmStack_t* p, unsigned blkEleCnt )
{
  cmStkBlk_t* blkPtr;

  // if a next block has already been created (because an earlier stack was cleared (w/o release)
  if( p->curBlkPtr != NULL && p->curBlkPtr->link != NULL )
    blkPtr = p->curBlkPtr->link;
  else
  {
    // ... otherwise allocate a new next block
    char* cp = cmMemAllocZ(char,sizeof(cmStkBlk_t) + p->eleByteCnt * blkEleCnt );

    blkPtr = (cmStkBlk_t*)cp;

    blkPtr->base      = cp + sizeof(cmStkBlk_t);
    blkPtr->curEleCnt = 0;
    blkPtr->maxEleCnt = blkEleCnt;

    if( p->curBlkPtr != NULL )
      p->curBlkPtr->link = blkPtr;
  }

  if( p->baseBlkPtr == NULL )
    p->baseBlkPtr = blkPtr;

  p->curBlkPtr = blkPtr;

  return blkPtr;
}

cmStkBlk_t*  _cmStackClear( cmStack_t* p, bool releaseFl )
{
  cmStkBlk_t* bp = p->baseBlkPtr;
  while( bp != NULL )
  {
    cmStkBlk_t* nbp = bp->link;
    bp->curEleCnt = 0;

    if( releaseFl )
      cmMemFree(bp);

    bp = nbp;
  }


  p->cnt       = 0;
  
  if( releaseFl )
  {
    p->curBlkPtr  = NULL;
    p->baseBlkPtr = NULL;
  }
  else
  {
    p->curBlkPtr = p->baseBlkPtr;
  }

  return bp;
}

cmStRC_t _cmStackFree( cmStack_t* p )
{
  cmStRC_t rc = kOkStRC;

  _cmStackClear(p,true);
  cmMemFree(p);
  return rc;
}


cmStRC_t cmStackAlloc( cmCtx_t* ctx, cmStackH_t* hp, unsigned initCnt, unsigned expandCnt, unsigned eleByteCnt )
{
  cmStRC_t rc = kOkStRC;

  if((rc = cmStackFree(hp)) != kOkStRC )
    return rc;

  cmStack_t* p = cmMemAllocZ(cmStack_t,1);
  
  cmErrSetup(&p->err,&ctx->rpt,"Stack");

  p->expandBlkEleCnt = expandCnt;
  p->eleByteCnt      = eleByteCnt;
  
  _cmStkAllocBlk(p,initCnt); // allocate the initial block

  hp->h = p;

  return rc;
}

cmStRC_t cmStackFree( cmStackH_t* hp )
{
  cmStRC_t rc = kOkStRC;
  if( hp==NULL || cmStackIsValid(*hp) == false )
    return kOkStRC;

  cmStack_t* p = _cmStHandleToPtr(*hp);

  if((rc = _cmStackFree(p)) != kOkStRC )
    goto errLabel;

  hp->h = NULL;

 errLabel:
  return rc;
}

cmStRC_t cmStackIsValid( cmStackH_t h )
{ return h.h != NULL; }

unsigned cmStackCount( cmStackH_t h )
{
  cmStack_t* p = _cmStHandleToPtr(h);
  return p->cnt;
}

void cmStackClear( cmStackH_t h, bool releaseFl )
{
  if( cmStackIsValid(h) == false )
    return;

  cmStack_t* p = _cmStHandleToPtr(h);
  _cmStackClear(p,releaseFl);
}


cmStRC_t cmStackPush(  cmStackH_t h, const void* data, unsigned dataEleCnt )
{
  cmStRC_t rc = kOkStRC;
  cmStack_t* p = _cmStHandleToPtr(h);

  while( dataEleCnt )
  {
    if( p->curBlkPtr == NULL || p->curBlkPtr->curEleCnt >= p->curBlkPtr->maxEleCnt )
      _cmStkAllocBlk(p, p->expandBlkEleCnt );
    
    // calc the count of ele's to copy into this block
    unsigned en = cmMin(dataEleCnt, p->curBlkPtr->maxEleCnt - p->curBlkPtr->curEleCnt );

    // calc the count of source bytes to copy into this block
    unsigned bn = en * p->eleByteCnt;

    // copy the source bytes into the block
    memcpy( p->curBlkPtr->base + (p->curBlkPtr->curEleCnt * p->eleByteCnt), data, bn );

    p->curBlkPtr->curEleCnt += en;                 // incr block element count
    p->cnt                  += en;                 // incr the total element count
    data                     = ((char*)data) + bn; // incr the source pointer
    dataEleCnt              -= en;                 // decr the source ele count
  }

  return rc;
}

// Return the block and local index of the element at index *idxPtr.
cmStkBlk_t* _cmStBlockIndex( cmStack_t* p, unsigned* idxPtr )
{
  cmStkBlk_t* blkPtr;

  assert( p->baseBlkPtr != NULL);

  if( *idxPtr < p->baseBlkPtr->maxEleCnt )
    return p->baseBlkPtr;

  *idxPtr -= p->baseBlkPtr->maxEleCnt;

  unsigned bi = *idxPtr / p->expandBlkEleCnt;
  unsigned i;
  blkPtr = p->baseBlkPtr->link;
  for(i=0; i<bi; ++i)
    blkPtr = blkPtr->link;
    
  *idxPtr = *idxPtr - bi * p->expandBlkEleCnt;

  return blkPtr;
}

cmStRC_t cmStackPop(   cmStackH_t h, unsigned eleCnt )
{
  cmStRC_t rc = kOkStRC;
  cmStack_t* p = _cmStHandleToPtr(h);

  if( p->cnt < eleCnt )
    return cmErrMsg(&p->err,kUnderflowStRC,"Stack underflow. Cannot pop %i elements off stack with %i elements.", eleCnt, p->cnt );

  unsigned    idx    = p->cnt - eleCnt;           // index of the first element to remove
  cmStkBlk_t* blkPtr = _cmStBlockIndex(p, &idx ); // get blk and local index of new curBlkPtr

  blkPtr->curEleCnt = idx;    // update the element cnt for the new curBlkPtr
  p->curBlkPtr = blkPtr;      // set the new curBlkPtr 
  blkPtr = blkPtr->link;      // for each blk after the current block ...
  while( blkPtr != NULL )
  {
    blkPtr->curEleCnt = 0;    //   ... set the block empty
    blkPtr = blkPtr->link;
  }

  p->cnt -= eleCnt;

  return rc;
}

const void* cmStackTop(     cmStackH_t h )
{
  unsigned n = cmStackCount(h);
  if( n == 0 )
    return NULL;
  return cmStackGet(h,n-1);
}

cmStRC_t _cmStackSetGet( cmStack_t* p, unsigned index, char* data, unsigned dataEleCnt, bool setFl )
{
  cmStRC_t   rc = kOkStRC;

  if( index + dataEleCnt > p->cnt )
    return cmErrMsg(&p->err,kInvalidIdxStRC,"The element index range:%i to %i falls outside the current index range:%i to %i of the stack.",index,index+dataEleCnt-1,0,p->cnt-1);

  cmStkBlk_t* blkPtr = _cmStBlockIndex(p,&index);
  assert( blkPtr != NULL );

  while( dataEleCnt )
  {
    // calc the count of ele's to copy into this block
    unsigned en = cmMin(dataEleCnt, blkPtr->curEleCnt - index );

    // calc the count of bytes in en elements
    unsigned bn = en * p->eleByteCnt;

    char*    bp = blkPtr->base + (index * p->eleByteCnt);
      
    // copy the source bytes into the block
    memcpy( setFl ? bp:data, setFl ? data:bp, bn );

    data                  = data + bn; // incr the source pointer
    dataEleCnt           -= en;                 // decr the source ele count
    index                 = 0;                  // all blocks after the first use index=0
    blkPtr                = blkPtr->link;
  }
  
  return rc;
}

cmStRC_t cmStackSet(  cmStackH_t h, unsigned index, const void* data, unsigned dataEleCnt )
{ return _cmStackSetGet( _cmStHandleToPtr(h), index, (char*) data, dataEleCnt,true ); }
  

cmStRC_t cmStackGetN( cmStackH_t h, unsigned index, void* data, unsigned dataEleCnt )
{ return _cmStackSetGet( _cmStHandleToPtr(h), index, (char*)data, dataEleCnt, false ); }

const void* cmStackGet( cmStackH_t h, unsigned index )
{ 
  cmStack_t* p = _cmStHandleToPtr(h);

  if( index >= p->cnt )
  {
    cmErrMsg(&p->err,kInvalidIdxStRC,"The index %i is outside the valid range 0:%i.",index,p->cnt-1);
    return NULL;
  }

  cmStkBlk_t* blkPtr = _cmStBlockIndex(p,&index);
  assert( blkPtr != NULL );
  return blkPtr->base + (index * p->eleByteCnt);
}

void* cmStackFlatten( cmStackH_t h )
{
  cmStack_t*  p   = _cmStHandleToPtr(h);
  cmStkBlk_t* bbp = p->baseBlkPtr;
  cmStkBlk_t* cbp = p->curBlkPtr;
  unsigned    cnt = p->cnt;

  // pretend the stack is unallocated
  p->baseBlkPtr = NULL;
  p->curBlkPtr  = NULL;
 
  // allocate a new initial block large enough to hold all the data
  cmStkBlk_t* nbp =  _cmStkAllocBlk(p,cnt);
  cmStkBlk_t* bp  = bbp;
  unsigned    n   = 0;

  // copy the existing stack values into the new initial block
  while( bp != NULL && bp->curEleCnt > 0 )
  {
    unsigned bn = bp->curEleCnt * p->eleByteCnt;

    assert( n < cnt*p->eleByteCnt );

    memcpy( nbp->base + n, bp->base, bn );
    n += bn;
    bp = bp->link;
  }
  
  // restore the stack to it's original state, clear and release
  p->baseBlkPtr = bbp;
  p->curBlkPtr  = cbp;
  _cmStackClear(p,true);

  // setup new stack with the new initial block 
  // (the data is now linearly arranged in nbp->base)
  p->baseBlkPtr = nbp; 
  p->curBlkPtr  = nbp;
  p->cnt        = cnt;

  return p->baseBlkPtr->base;
}

void        cmStackTest(    cmCtx_t* ctx )
{
  cmStackH_t h = cmStackNullHandle;
  int i;
  int v[20];

  if( cmStackAlloc(ctx,&h,10,5,sizeof(int)) != kOkStRC )
    goto errLabel;

  for(i=0; i<9; ++i)
    v[i] = i;

  cmStackPush(h,v,i);

  cmRptPrintf(&ctx->rpt,"count:%i %i\n",cmStackCount(h),i);

  for(; i<18; ++i)
    v[i] = i;

  cmStackPush(h,v,i);  

  cmRptPrintf(&ctx->rpt,"count:%i %i\n",cmStackCount(h),i);  

  cmStackPop(h,10);

  cmRptPrintf(&ctx->rpt,"count:%i %i\n",cmStackCount(h),i);  

  cmStackPop(h,10);

  cmRptPrintf(&ctx->rpt,"count:%i %i\n",cmStackCount(h),i);  

  cmStackPush(h,v,10);

  cmRptPrintf(&ctx->rpt,"count:%i %i\n",cmStackCount(h),i);  

  for(i=0; i<cmStackCount(h); ++i)
    cmRptPrintf(&ctx->rpt,"%i ",cmStackEle(h,int,i));

  cmRptPrintf(&ctx->rpt,"\n");

  int* ip = (int*)cmStackFlatten(h);
  for(i=0; i<cmStackCount(h); ++i)
    cmRptPrintf(&ctx->rpt,"%i ",ip[i]);

  cmRptPrintf(&ctx->rpt,"\n");

 errLabel:
  cmStackFree(&h);
}
