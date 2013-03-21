#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmSymTbl.h"
#include "cmJson.h"
#include "cmDspValue.h"
#include "cmDspCtx.h"
#include "cmDspClass.h"
#include "cmDspStore.h"

typedef struct
{
  unsigned     symId;
  cmDspValue_t value;
} cmDspStoreVar_t;

typedef struct 
{
  cmErr_t          err;
  cmDspStoreVar_t* array;
  unsigned         allocCnt;
  unsigned         growCnt;
  unsigned         curCnt;
} cmDspStore_t;


cmDspStoreH_t cmDspStoreNullHandle = cmSTATIC_NULL_HANDLE;

cmDspStore_t* _cmDspStoreHandleToPtr( cmDspStoreH_t h )
{
  cmDspStore_t* p = (cmDspStore_t*)h.h;
  assert( p != NULL );
  return p;
}

cmDspRC_t _cmDspStoreFree( cmDspStore_t* p )
{
  cmMemFree(p->array);
  cmMemFree(p);
  return kOkDspRC;
}


cmDspRC_t cmDspStoreAlloc( cmCtx_t* ctx, cmDspStoreH_t* hp, unsigned initStoreCnt, unsigned growStoreCnt )
{
  cmDspRC_t rc;
  if((rc = cmDspStoreFree(hp)) != kOkDspRC )
    return rc;

  cmDspStore_t* p = cmMemAllocZ(cmDspStore_t,1);

  cmErrSetup(&p->err,&ctx->rpt,"cmDspStore");

  p->array    = cmMemAllocZ(cmDspStoreVar_t,initStoreCnt);
  p->allocCnt = initStoreCnt;
  p->curCnt   = 0;
  p->growCnt  = growStoreCnt;

  hp->h = p;

  return rc;
}
  
cmDspRC_t cmDspStoreFree( cmDspStoreH_t *hp )
{
  cmDspRC_t rc = kOkDspRC;
  if(hp==NULL || cmDspStoreIsValid(*hp)==false )
    return rc;

  cmDspStore_t* p = _cmDspStoreHandleToPtr(*hp);

  if((rc = _cmDspStoreFree(p)) != kOkDspRC )
    return rc;

  hp->h = NULL;

  return rc;
}

bool      cmDspStoreIsValid( cmDspStoreH_t h )
{ return h.h; }


cmDspStoreVar_t*  _cmDspStoreSymToPtr( cmDspStore_t* p, unsigned symId )
{
  cmDspStoreVar_t* vp = p->array;
  cmDspStoreVar_t* ep = p->array + p->curCnt;
  for(; vp<ep; ++vp)
    if( vp->symId == symId )
      return vp;
  return NULL;
}

cmDspStoreVar_t*  _cmDspStoreIdToPtr( cmDspStore_t* p, unsigned id )
{
  if( id < p->curCnt )
    return p->array + id;
 
  return NULL;
}

cmDspStoreVar_t* _cmDspStoreAppend( cmDspStore_t* p )
{
  cmDspStoreVar_t* vp = NULL;

  if( p->curCnt >= p->allocCnt )
  {
    p->allocCnt += p->growCnt;
    p->array = cmMemResizePZ(cmDspStoreVar_t,p->array,p->allocCnt);
  }

  vp = p->array + p->curCnt;

  p->curCnt += 1;

  return vp;
}
  
unsigned  cmDspStoreSymToId( cmDspStoreH_t h, unsigned symId )
{
  cmDspStore_t* p = _cmDspStoreHandleToPtr(h);
  const cmDspStoreVar_t* vp;
  if((vp = _cmDspStoreSymToPtr(p,symId)) == NULL )
    return cmInvalidId;
  return vp - p->array;
}

unsigned  cmDspStoreIdToSym( cmDspStoreH_t h, unsigned id )
{
  cmDspStore_t* p = _cmDspStoreHandleToPtr(h);
  const cmDspStoreVar_t* vp;
  if((vp = _cmDspStoreIdToPtr(p,id)) == NULL )
    return cmInvalidId;
  return vp->symId;
}

const cmDspValue_t*  cmDspStoreIdToValue( cmDspStoreH_t h, unsigned id )
{
  cmDspStore_t* p = _cmDspStoreHandleToPtr(h);
  const cmDspStoreVar_t* vp;
  if((vp = _cmDspStoreIdToPtr(p,id)) == NULL )
    return NULL;
  return &vp->value;
}

cmDspRC_t cmDspStoreSetValueViaId(  cmDspStoreH_t h, unsigned id, const cmDspValue_t* val )
{
  cmDspRC_t rc = kOkDspRC;
  cmDspStore_t*     p = _cmDspStoreHandleToPtr(h);
  cmDspStoreVar_t* vp = NULL;

  if((vp = _cmDspStoreIdToPtr(p,id)) == NULL )
    return cmErrMsg(&p->err,kVarNotFoundDspRC,"There is not global variable at with id:%i\n",id);

  cmDsvCopy(&vp->value,val);    
  return rc;
}

unsigned cmDspStoreSetValueViaSym( cmDspStoreH_t h, unsigned symId, const cmDspValue_t* val )
{
  cmDspStore_t*     p = _cmDspStoreHandleToPtr(h);
  cmDspStoreVar_t* vp = NULL;

  if((vp = _cmDspStoreSymToPtr(p,symId)) == NULL )
    vp = _cmDspStoreAppend(p);

  assert(vp != NULL );

  cmDsvCopy(&vp->value,val);    
  vp->symId = symId;

  return vp - p->array;
}
