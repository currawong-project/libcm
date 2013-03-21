#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmArray.h"

typedef struct
{
  cmErr_t  err;
  char*    base;
  unsigned expand_cnt;
  unsigned alloc_cnt;
  unsigned cur_cnt;
  unsigned ele_byte_cnt;
} cmAr_t;

cmArrayH_t cmArrayNullHandle = cmSTATIC_NULL_HANDLE;

cmAr_t* _cmArHandleToPtr( cmArrayH_t h )
{
  cmAr_t* p = (cmAr_t*)h.h;
  assert(p!=NULL);
  return p;
}

cmArRC_t _cmArFree( cmAr_t* p )
{
  cmArRC_t rc = kOkArRC;
  cmMemFree(p->base);
  cmMemFree(p);
  return rc;
}



cmArRC_t    cmArrayAlloc0(  cmCtx_t* ctx, cmArrayH_t* hp, unsigned eleByteCnt, unsigned initCnt, unsigned expandCnt )
{
  cmArRC_t rc;
  if((rc = cmArrayRelease(hp)) != kOkArRC )
    return rc;

  cmAr_t* p = cmMemAllocZ(cmAr_t,1);
  cmErrSetup(&p->err,&ctx->rpt,"Array");

  p->alloc_cnt    = initCnt;
  p->expand_cnt   = expandCnt;
  p->cur_cnt      = 0;
  p->ele_byte_cnt = eleByteCnt;

  if( p->alloc_cnt > 0 )
    p->base = cmMemAllocZ(char,p->alloc_cnt*eleByteCnt);

  hp->h = p;
  
  return rc;
}

cmArRC_t    cmArrayAlloc(  cmCtx_t* ctx, cmArrayH_t* hp, unsigned eleByteCnt )
{ return cmArrayAlloc0(ctx,hp,eleByteCnt,10,10); }


cmArRC_t    cmArrayRelease(   cmArrayH_t* hp )
{
  cmArRC_t rc = kOkArRC;

  if(hp==NULL || cmArrayIsValid(*hp)==false )
    return rc;

  cmAr_t* p = _cmArHandleToPtr(*hp);

  if((rc = _cmArFree(p)) != kOkArRC )
    return rc;

  hp->h = NULL;

  return rc;
}

cmArRC_t    cmArrayIsValid(cmArrayH_t h )
{  return h.h != NULL; }

void    cmArraySetExpandCount( cmArrayH_t h, unsigned expandCnt )
{
  cmAr_t* p = _cmArHandleToPtr(h);
  p->expand_cnt = expandCnt;
}

unsigned    cmArrayExpandCount( cmArrayH_t h )
{
  cmAr_t* p = _cmArHandleToPtr(h);
  return p->expand_cnt;
}

unsigned    cmArrayCount(  cmArrayH_t h )
{
  cmAr_t* p = _cmArHandleToPtr(h);
  return p->cur_cnt;
}

cmArRC_t    cmArrayClear(  cmArrayH_t h, bool releaseFl )
{
  cmAr_t* p = _cmArHandleToPtr(h);
  if( releaseFl )
  {
    cmMemPtrFree(&p->base);
    p->alloc_cnt = 0;
  }
 
  p->cur_cnt   = 0;
  return kOkArRC;
}

void*    _cmArraySet( cmAr_t* p, unsigned idx, const void* data, unsigned dataEleCnt )
{
  if( idx+dataEleCnt > p->alloc_cnt )
  {
    unsigned add_cnt = (idx + dataEleCnt) - p->alloc_cnt;

    if( add_cnt < p->expand_cnt )
      add_cnt = p->expand_cnt;
    else
      add_cnt = ((add_cnt / p->expand_cnt) + 1) * p->expand_cnt;

    p->alloc_cnt += add_cnt;

    p->base = cmMemResizePZ(char,p->base,p->alloc_cnt*p->ele_byte_cnt);    
  }

  char* bp = p->base + (idx*p->ele_byte_cnt);

  if( data == NULL )
    memset(bp, 0,    p->ele_byte_cnt * dataEleCnt );
  else
    memcpy(bp, data, p->ele_byte_cnt * dataEleCnt );
    
  if( idx+dataEleCnt > p->cur_cnt )
    p->cur_cnt = idx + dataEleCnt;

  return bp;
}

void*    cmArrayPush(   cmArrayH_t h, const void* data, unsigned dataEleCnt )
{
  cmAr_t* p = _cmArHandleToPtr(h);
  return _cmArraySet(p,p->cur_cnt,data,dataEleCnt);
}

cmArRC_t    cmArrayPop(    cmArrayH_t h, unsigned eleCnt )
{
  cmAr_t* p = _cmArHandleToPtr(h);
  if( eleCnt > p->cur_cnt )
    return cmErrMsg(&p->err,kUnderflowArRC,"Cannot pop %i element(s). Array contains %i element(s).",eleCnt,p->cur_cnt);
  p->cur_cnt -= eleCnt;
  return kOkArRC;
}

void*    cmArraySet( cmArrayH_t h, unsigned index, const void* data, unsigned dataEleCnt )
{
  cmAr_t* p = _cmArHandleToPtr(h);
  return  _cmArraySet(p,index,data,dataEleCnt );
}

const void* cmArrayGet(    cmArrayH_t h, unsigned index )
{
  cmAr_t* p = _cmArHandleToPtr(h);
  return p->base + (index * p->ele_byte_cnt);
}

