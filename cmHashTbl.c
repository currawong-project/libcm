#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmLinkedHeap.h"
#include "cmMallocDebug.h"
#include "cmMath.h"
#include "cmHashTbl.h"
#include "cmText.h"

enum
{
  kFreeHtFl = 0x01,
};

typedef struct cmHtValue_str
{
  unsigned              flags;    // See kXXXHtFl above.
  unsigned              id;       // unique id associated with this value
  void*                 value;    // value blob 
  unsigned              byteCnt;  // size of value blob in bytes
  struct cmHtValue_str* link;     // cmHtBucket_t.list link
} cmHtValue_t;

typedef struct
{
  cmHtValue_t* list;    // value list
  cmHtValue_t* avail;   // available value slots - formed from cmHashTblRemoved() values.
  unsigned     nextIdx; // next unused index for this bucket
} cmHtBucket_t;

typedef struct
{
  cmErr_t       err;
  cmLHeapH_t    lhH;            // memory for hash table buckets, values, value blobs.
  unsigned      bucketCnt;      // hash table bucket cnt
  unsigned      linkCnt;        // max length of collision list for each bucket
  unsigned      mask;           // hash id bucket index mask (masks the MSB's of the hash-id)
  unsigned      maskShift;      // shift required to move the lowest 'mask' bit to the LSB.
  cmHtBucket_t* b;              // b[bucketCnt] bucket array
} cmHt_t;

cmHashTblH_t cmHashTblNullHandle = cmSTATIC_NULL_HANDLE;

#define _cmHtBucketIndex( p, id ) (((id) & (p)->mask) >> (p)->maskShift)

cmHt_t* _cmHtHandleToPtr( cmHashTblH_t h )
{
  cmHt_t* p = (cmHt_t*)h.h;
  assert(p!=NULL);
  return p;
}

// Return the bucket index portion of the hash id.
unsigned _cmHtGenId( cmHt_t* p, const void* v, unsigned byteCnt )
{
  unsigned i,j;
  const char* cv = v;
  unsigned h = 0;

  for(i=0,j=3; i<byteCnt; ++i,++j)
    h += ((unsigned)cv[i]) << ((j&0x3)*8);

  return h & p->mask;
}


// Given an id find the value.
cmHtValue_t* _cmHtIdToValue( cmHt_t* p, unsigned id )
{
  if( id == cmInvalidId )
    return NULL;

  unsigned      bi = _cmHtBucketIndex(p,id);

  assert(bi < p->bucketCnt);

  cmHtValue_t* v = p->b[bi].list;
  for(; v!=NULL; v=v->link)
    if( v->id == id )
      return v;

  return NULL;
}

// Given a value find the id
cmHtValue_t* _cmHtValueToId( cmHt_t* p, const void* value, unsigned byteCnt, unsigned id )
{
  if( id == cmInvalidId )
    id = _cmHtGenId(p,value,byteCnt);

  unsigned bi = _cmHtBucketIndex(p,id);

  assert(bi < p->bucketCnt);

  cmHtValue_t* v = p->b[bi].list;
  for(; v!=NULL; v=v->link)
    if( v->byteCnt==byteCnt && memcmp(value,v->value,byteCnt)==0 )
      return v;

  return NULL;
}

cmHtRC_t _cmHtDestroy( cmHt_t* p )
{
  cmHtRC_t rc = kOkHtRC;
  cmLHeapDestroy(&p->lhH);
  cmMemFree(p->b);
  cmMemFree(p);
  return rc;
}

cmHtRC_t cmHashTblCreate( cmCtx_t* ctx, cmHashTblH_t* hp, unsigned bucketCnt )
{
  cmHtRC_t rc;
  if((rc = cmHashTblDestroy(hp)) != kOkHtRC )
    return rc;

  cmHt_t* p = cmMemAllocZ(cmHt_t,1);

  cmErrSetup(&p->err,&ctx->rpt,"hash table");

  if(cmLHeapIsValid(p->lhH = cmLHeapCreate(8192,ctx)) == false )
  {
    cmErrMsg(&p->err,kLHeapFailHtRC,"Internal linked heap mgr. create failed.");
    goto errLabel;
  }

  // force the bucket count to be a power of two
  p->bucketCnt = cmNextPowerOfTwo(bucketCnt);
  p->mask      = p->bucketCnt - 1;

  // calcluate the hash-id bucket mask
  for(p->maskShift=0; (0x80000000 & p->mask) == 0; ++p->maskShift )
    p->mask <<= 1;
  
  // calculate the maximum collisions per bucket mask
  p->linkCnt = ~p->mask;

  // allocate the bucket array
  p->b = cmMemAllocZ(cmHtBucket_t,p->bucketCnt);
  
  hp->h = p;

 errLabel:
  if( rc != kOkHtRC )
    _cmHtDestroy(p);
  return rc;
}

cmHtRC_t cmHashTblDestroy( cmHashTblH_t* hp )
{
  cmHtRC_t rc = kOkHtRC;
  if(hp==NULL || cmHashTblIsValid(*hp)==false )
    return rc;

  cmHt_t* p = _cmHtHandleToPtr(*hp);

  if((rc = _cmHtDestroy(p)) != kOkHtRC )
    return rc;

  hp->h = NULL;

  return rc;
}

bool cmHashTblIsValid( cmHashTblH_t h )
{ return h.h!=NULL; }

unsigned cmHashTblStoreBase(      cmHashTblH_t h, void* v, unsigned byteCnt, bool staticFl )
{
  cmHt_t*      p  = _cmHtHandleToPtr(h);
  cmHtValue_t* vp = NULL;
  unsigned     id = _cmHtGenId(p, v, byteCnt );

  // if the value is already stored then there is nothing else to do
  if((vp = _cmHtValueToId(p,v,byteCnt,id)) != NULL )
    return vp->id;

  unsigned bi = _cmHtBucketIndex(p,id);

  assert(bi < p->bucketCnt );

  cmHtBucket_t* b  = p->b + bi;

  if( b->avail != NULL )
  {
    vp       = b->avail;
    b->avail = b->avail->link;
  }
  else
  {
    if( b->nextIdx == p->linkCnt || (id + b->nextIdx) == cmInvalidId )
    {
      cmErrMsg(&p->err,kHashFaultHtRC,"The hash table bucket at index %i is exhaused.",bi);
      return cmInvalidId;
    }

    vp = cmLhAllocZ(p->lhH,cmHtValue_t,1);
    vp->id = id + b->nextIdx++;
  }  
  

  assert( vp->id != cmInvalidId );
  
  vp->link = b->list;
  b->list = vp;
  vp->byteCnt = byteCnt;

  if( staticFl )
    vp->value = v;
  else
  {
    vp->value = cmLhAlloc(p->lhH,char,byteCnt);
    memcpy(vp->value,v,byteCnt);
    vp->flags = cmSetFlag(vp->flags,kFreeHtFl);
  }

  return vp->id;
}

unsigned cmHashTblStore(          cmHashTblH_t h, void* v, unsigned byteCnt )
{ return cmHashTblStoreBase(h,v,byteCnt,false); }

unsigned cmHashTblStoreStatic(    cmHashTblH_t h, void* v, unsigned byteCnt )
{ return cmHashTblStoreBase(h,v,byteCnt,true); }

unsigned _cmHashTblStoreStr(       cmHashTblH_t h, const cmChar_t* s, bool staticFl )
{ 
  unsigned n = cmTextLength(s);
  if( n == 0 )
  {
    s = "";
    n = 1;
  }

return cmHashTblStoreBase(h,(void*)s,n+1,staticFl); 
}

unsigned cmHashTblStoreStr(       cmHashTblH_t h, const cmChar_t* s )
{ return _cmHashTblStoreStr(h,s,false); }

unsigned cmhashTblStoreStaticStr( cmHashTblH_t h, const cmChar_t* s )
{ return _cmHashTblStoreStr(h,s,true); }

unsigned cmHashTblStoreV( cmHashTblH_t h, const cmChar_t* fmt, va_list vl )
{
  cmChar_t* s = NULL;
  s = cmTsVPrintfP(s,fmt,vl);
  unsigned id = _cmHashTblStoreStr(h,s,false); 
  cmMemFree(s);
  return id;
}

unsigned cmHashTblStoreF( cmHashTblH_t h, const cmChar_t* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  unsigned id = cmHashTblStoreV(h,fmt,vl);
  va_end(vl);
  return id;
}

unsigned cmHashTblId( cmHashTblH_t h, const void* value, unsigned byteCnt )
{
  cmHt_t*  p  = _cmHtHandleToPtr(h);
  cmHtValue_t* vp;

  if((vp = _cmHtValueToId(p,value,byteCnt,cmInvalidId)) == NULL )
    return cmInvalidId;

  return vp->id;
}

unsigned cmHashTblStrToId( cmHashTblH_t h, const cmChar_t* str )
{
  if( str == NULL )
    return cmInvalidId;

  return cmHashTblId(h,str,cmTextLength(str)+1);
}


const void* cmHashTblValue( cmHashTblH_t h, unsigned id, unsigned* byteCntRef )
{
  cmHt_t*      p = _cmHtHandleToPtr(h);
  cmHtValue_t* vp;

  if((vp = _cmHtIdToValue(p, id)) != NULL )
  {
    if( byteCntRef != NULL )
      *byteCntRef = vp->byteCnt;

    return vp->value;
  }

  return NULL; 
}


const cmChar_t* cmHashTblStr( cmHashTblH_t h, unsigned id )
{  return (const cmChar_t*)cmHashTblValue(h,id,NULL);  }


cmHtRC_t cmHashTblRemove( cmHashTblH_t h, unsigned id )
{
  cmHt_t*       p  = _cmHtHandleToPtr(h);
  unsigned      bi = _cmHtBucketIndex(p,id);



  assert(bi < p->bucketCnt);

  cmHtBucket_t* b  = p->b + bi;

  cmHtValue_t*  vp = b->list;
  cmHtValue_t*  pp = NULL;

  for(; vp!=NULL; vp=vp->link)
  {
    if( vp->id == id )
    {
      if( pp == NULL )
        b->list = vp->link;
      else
        pp->link = vp->link;

      break;
    }

    pp = vp;
  }

  if( vp == NULL )
    return cmErrMsg(&p->err,kInvalidIdHtRC,"A value could not be found for the hash id 0x%x.",id);
  
  if( cmIsFlag(vp->flags,kFreeHtFl ) )
    cmLhFree(p->lhH,vp->value);
  

  vp->flags   = 0;
  vp->value   = NULL;
  vp->byteCnt = 0;

  // Note: Do not set the id to zero since we want to consert id's 
  // and this recd will be reused by the next call to cmHashTblStoreBase().

  return kOkHtRC;
  
}


cmHtRC_t cmHashTblLastRC( cmHashTblH_t h )
{
  cmHt_t* p = _cmHtHandleToPtr(h);
  return cmErrLastRC(&p->err);
}

void _cmHashTblBucketReport( cmHtBucket_t* b, cmRpt_t* rpt )
{
  cmHtValue_t* vp = b->list;
  unsigned i;
  for(i=0; vp!=NULL && i<10; vp=vp->link,++i)
    cmRptPrintf(rpt,"0x%x : %s\n",vp->id,((const cmChar_t*)vp->value));

  cmRptPrintf(rpt,"\n");
}

void cmHashTblReport( cmHashTblH_t h, cmRpt_t* rpt )
{
  cmHt_t* p = _cmHtHandleToPtr(h);
  unsigned i;
  for(i=0; i<p->bucketCnt; ++i)
  {
    //if( p->b[i].nextIdx > 0 )
    //  cmRptPrintf(rpt,"%i,%i\n",i,p->b[i].nextIdx);

    if( p->b[i].nextIdx > 100 )
      _cmHashTblBucketReport(p->b + i,rpt);
  }
}


cmHtRC_t cmHashTblTest( cmCtx_t* ctx )
{
  cmHtRC_t     rc = kOkHtRC;
  cmHashTblH_t h  = cmHashTblNullHandle;
  cmErr_t err;
  cmErrSetup(&err,&ctx->rpt,"hash table test");

  if((rc = cmHashTblCreate(ctx,&h,8192)) != kOkHtRC )
    return cmErrMsg(&err,rc,"Hash table create failed.");

  const cmChar_t* arr[] = 
  {
    "1",
    "12",
    "123",
    "1234",
    "12345",
    "123456",
    "123456",
    "123456",
    NULL
  };

  unsigned n = sizeof(arr)/sizeof(arr[0]);
  unsigned ids[ n ];
  int i = 0;

  // store the values from arr[]
  for(; arr[i]!=NULL; ++i)
    if((ids[i] = cmHashTblStoreStr(h,arr[i])) == cmInvalidId )
    {
      rc = cmErrMsg(&err,cmHashTblLastRC(h),"Hash store failed on: '%s.",cmStringNullGuard(arr[i]));
      goto errLabel;
    }

  /*
  // remove a value
  unsigned rem_idx = 3;
  if((rc = cmHashTblRemove(h, ids[rem_idx] )) != kOkHtRC )
  {
    rc = cmErrMsg(&err,rc,"Hash removed failed.");
    goto errLabel;
  }

  // insert the same value - which should restore the removed value
  if((ids[rem_idx] = cmHashTblStoreStr(h,arr[rem_idx])) == cmInvalidId )
  {
    rc = cmErrMsg(&err,cmHashTblLastRC(h),"Hash store failed on: '%s.",cmStringNullGuard(arr[rem_idx]));
    goto errLabel;
  }
  */
  
  // lookup all the stored values by id
  for(--i; i>=0; --i)
  {
    const cmChar_t* s;

    if((s = cmHashTblStr(h,ids[i])) == NULL )
      rc = cmErrMsg(&err,kInvalidIdHtRC,"The value associated with hash-id:0x%x could not be found.",ids[i]);
    else
      printf("%i : %s\n",i,cmStringNullGuard(s));
  }


  for(i=0; arr[i]!=NULL; ++i)
  {
    unsigned id = cmHashTblStrToId(h, arr[i]);
    printf("%i : 0x%x : %s\n",i, id, cmStringNullGuard(cmHashTblStr(h, id)));
  }


  cmHashTblReport(h, &ctx->rpt );


 errLabel:
  cmHashTblDestroy(&h);
  return rc;

}
