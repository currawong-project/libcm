//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmStrStream.h"
#include "cmText.h"

typedef struct cmSsBlk_str
{
  char*               blk;
  unsigned            i;
  struct cmSsBlk_str* link;
} cmSsBlk_t;

typedef struct
{
  cmErr_t     err;
  cmLHeapH_t  lhH;  
  unsigned    blkByteCnt;
  cmSsBlk_t*  blp;
  cmSsBlk_t*  elp;
} cmOss_t;

cmStrStreamH_t cmStrStreamNullHandle = cmSTATIC_NULL_HANDLE;

cmOss_t* _cmOssHandleToPtr( cmStrStreamH_t h )
{
  cmOss_t* p = (cmOss_t*)h.h;
  assert(p != NULL );
  return p;
}

cmSsRC_t _cmOssDestroy( cmOss_t* p )
{
  cmSsRC_t rc = kOkSsRC;
  cmLHeapDestroy(&p->lhH);
  cmMemFree(p);
  return rc;
}

cmSsRC_t cmOStrStreamCreate( cmCtx_t* ctx, cmStrStreamH_t* hp, unsigned dfltBlkByteCnt )
{
  cmSsRC_t rc;
  if((rc = cmOStrStreamDestroy(hp)) != kOkSsRC )
    return rc;

  cmOss_t* p = cmMemAllocZ(cmOss_t,1);

  p->blkByteCnt = dfltBlkByteCnt==0 ? 4096 : dfltBlkByteCnt;

  cmErrSetup(&p->err,&ctx->rpt,"OStrStream");
  
  if( cmLHeapIsValid(p->lhH = cmLHeapCreate(p->blkByteCnt+sizeof(cmSsBlk_t),ctx)) == false )
  {
    rc = cmErrMsg(&p->err,kLHeapMemFailSsRC,"Linked heap allocation failed.");
    goto errLabel;
  }
  
  hp->h = p;

 errLabel:
  if(rc != kOkSsRC )
    _cmOssDestroy(p);

  return rc;
}

cmSsRC_t cmOStrStreamDestroy(cmStrStreamH_t* hp )
{
  cmSsRC_t rc = kOkSsRC;

  if( hp==NULL || cmOStrStreamIsValid(*hp)==false )
    return rc;

  cmOss_t* p = _cmOssHandleToPtr(*hp);

  if((rc = _cmOssDestroy(p)) != kOkSsRC )
    return rc;

  hp->h = NULL;

  return rc;
}

bool     cmOStrStreamIsValid( cmStrStreamH_t h )
{ return h.h != NULL; }


cmSsRC_t cmOStrStreamWrite(     cmStrStreamH_t h, const void* vp, unsigned byteCnt )
{
  cmSsRC_t rc = kOkSsRC;

  if( vp==NULL || byteCnt == 0 )
    return rc;

  cmOss_t* p  = _cmOssHandleToPtr(h);
  char*    cp = (char*)vp;
  unsigned j  = 0;

  do
  {
    // if a blk exists
    if( p->elp != NULL )
    {
      // copy as much of vp[] as possible into the current end block
      unsigned n = cmMin(byteCnt, p->blkByteCnt -  p->elp->i);
      memcpy(p->elp->blk + p->elp->i,cp + j,n);
      byteCnt    -= n;
      p->elp->i  += n;   
      j          += n;
    }
    
    // if all of vp[] has been copied then we are done
    if( byteCnt == 0 )
      break;

    assert( p->elp==NULL || p->elp->i == p->blkByteCnt );

    // allocate a new block
    cmSsBlk_t* nbp = (cmSsBlk_t*)cmLHeapAlloc(p->lhH,p->blkByteCnt+sizeof(cmSsBlk_t));
    nbp->blk  = (char*)(nbp + 1);
    nbp->i    = 0;
    nbp->link = NULL;

    // append the new blk onto the end of the list
    if( p->elp == NULL )
      p->blp = nbp;
    else
      p->elp->link = nbp;

    p->elp = nbp;
    
  }while(1);

  return rc;
}

cmSsRC_t cmOStrStreamWriteStr(  cmStrStreamH_t h, const cmChar_t* str )
{
  if( str == NULL )
    return kOkSsRC;

  return cmOStrStreamWrite(h,str,strlen(str));
}

cmSsRC_t cmOStrStreamWriteStrN( cmStrStreamH_t h, const cmChar_t* str, unsigned n )
{ return cmOStrStreamWrite(h,str,n); }

cmSsRC_t cmOStrStreamVPrintf(   cmStrStreamH_t h, const cmChar_t* fmt, va_list vl )  
{
  cmChar_t* s = cmTsVPrintfP(NULL,fmt,vl);
  cmSsRC_t rc = cmOStrStreamWriteStr(h,s);
  cmMemFree(s);
  return rc;
}

cmSsRC_t cmOStrStreamPrintf(    cmStrStreamH_t h, const cmChar_t* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  cmSsRC_t rc = cmOStrStreamVPrintf(h,fmt,vl);
  va_end(vl);
  return rc;
}

unsigned  cmOStrStreamByteCount( cmStrStreamH_t h )
{
  unsigned   n  = 0;
  cmOss_t*   p  = _cmOssHandleToPtr(h);
  cmSsBlk_t* bp = p->blp;

  for(; bp!=NULL; bp=bp->link)
    n += bp->i;

  return n;
}

unsigned  _cmOssCopyBuf( cmOss_t* p, char* buf, unsigned n )
{
  unsigned i   = 0;
  cmSsBlk_t* bp = p->blp;

  for(; bp!=NULL; bp=bp->link)
  {
    assert( i + bp->i <= n );

    memcpy(buf+i,bp->blk,bp->i);
    i += bp->i;
  }

  return i;
}

void*     cmOStrStreamAllocBuf(  cmStrStreamH_t h )
{
  unsigned   n  = cmOStrStreamByteCount(h);
  cmOss_t*   p  = _cmOssHandleToPtr(h);

  if( n == 0 )
    return NULL;

  char*    buf = cmMemAlloc(char,n);

  unsigned i = _cmOssCopyBuf(p,buf,n);

  assert(i==n);

  return buf;
}

cmChar_t* cmOStrStreamAllocText( cmStrStreamH_t h )
{
  unsigned   n  = cmOStrStreamByteCount(h);
  cmOss_t*   p  = _cmOssHandleToPtr(h);

  if( n == 0 )
    return NULL;

  char*    buf = cmMemAlloc(char,n+1);

  unsigned i = _cmOssCopyBuf(p,buf,n);

  assert(i==n);

  buf[n] = 0;

  return buf;
}
