#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmSymTbl.h"
#include "cmFloatTypes.h"
#include "cmComplexTypes.h"
#include "cmProcObj.h"


//------------------------------------------------------------------------------------------------------------
void* _cmObjAlloc( void* ap, const char* label, struct cmCtx_str* c, unsigned objByteCnt )
{
  unsigned flags = (ap == NULL) ? kDynObjFl : 0;

  if( ap == NULL )
    ap = cmMemMalloc(objByteCnt); 

  memset(ap,0,objByteCnt);
  
  assert( objByteCnt >= sizeof(cmObj));

  // the first field in the object must be an cmObj field
  cmObj* p  = (cmObj*)ap;
  p->flags  = flags;
  p->ctx = c;

  if( c != NULL )
    cmErrSetup(&p->err,c->obj.err.rpt,label);

  return ap;
}

// if pp is non-NULL then *pp is set to NULL on return
void  _cmObjFree(  void** pp, cmObj* op )
{
  // pp and op are both NULL or neither is NULL
  assert( (pp==NULL) == (op==NULL) );

  if( op != NULL )
    if( cmIsFlag(op->flags,kDynObjFl) )
      cmMemPtrFree( pp ); 

}



//------------------------------------------------------------------------------------------------------------

cmCtx* cmCtxAlloc( cmCtx* p,  cmRpt_t* rpt, cmLHeapH_t lhH, cmSymTblH_t stH )
{
  cmRC_t rc;
  
  cmCtx* op = cmObjAlloc( cmCtx, NULL, p );

  op->obj.ctx = op;

  if( (rc = cmCtxInit( op, rpt, lhH, stH )) != cmOkRC )
    cmCtxFree(&op);

  return op;

}

cmRC_t cmCtxFree(  cmCtx** pp )
{

  if( pp != NULL && *pp != NULL )
  {
    cmCtxFinal(*pp);
    cmObjFree(pp);
  }

  return cmOkRC;
}

cmRC_t cmCtxInit(  cmCtx* c,  cmRpt_t* rpt, cmLHeapH_t lhH, cmSymTblH_t stH )
{
  cmRC_t rc;

  if((rc = cmCtxFinal(c)) != cmOkRC )
    return rc;
  
  c->obj.err.rpt = rpt;
  c->lhH         = lhH;
  c->stH         = stH;

  return rc;
}

cmRC_t cmCtxFinal( cmCtx* c )
{ return cmOkRC;  }


cmRC_t  cmCtxPrint( cmCtx* c, const char* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  cmRptVPrintf(c->obj.err.rpt,fmt,vl);
  va_end(vl);
  return cmOkRC;
}


cmRC_t  cmCtxRtCondition(   cmObj* p, unsigned code, const char* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  cmErrVMsg(&p->err,code,fmt,vl);
  va_end(vl);
  return code;
}

cmRC_t  cmCtxRtAssertFailed( cmObj* p, unsigned code, const char* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  cmErrVMsg(&p->err, code,fmt,vl);
  va_end(vl);
  return code;
}


//------------------------------------------------------------------------------------------------------------
cmMtxFile* cmMtxFileAlloc( cmCtx* c, cmMtxFile* p, const char* fn )
{
  cmMtxFile* op = cmObjAlloc( cmMtxFile, c, p );

  if( fn != NULL )
    if( cmMtxFileCreate( op, fn ) != cmOkRC && p == NULL )
      cmMtxFileFree(&op);
  return op;
}

cmRC_t      cmMtxFileFree(  cmMtxFile** pp )
{
  cmRC_t rc = cmOkRC;
  if( pp != NULL && *pp != NULL)
  {
    cmLHeapFree((*pp)->obj.ctx->lhH,*pp);

    if((rc = cmMtxFileClose(*pp)) == cmOkRC )
      cmObjFree(pp);
  }

  return rc;
}

cmRC_t      cmMtxFileCreate(cmMtxFile* p, const char* fn )
{
  assert(fn != NULL );

  cmRC_t rc;
  if((rc = cmMtxFileClose(p)) != cmOkRC )
    return rc;

  if((p->fp = fopen(fn,"wt")) == NULL )
    return cmCtxRtCondition( &p->obj, cmSystemErrorRC, "Unable to create the matrix text file:'%s'.",fn);

  unsigned n = strlen(fn)+1;
  p->fn = cmLhResizeN(p->obj.ctx->lhH,cmChar_t,p->fn,n);
  strncpy(p->fn,fn,n);

  return rc;
}

cmRC_t      cmMtxFileClose( cmMtxFile* p )
{
  if( p != NULL )
  {
    if( p->fp != NULL )
    {
      fclose(p->fp);
      p->fp = NULL;
    }      
  }

  return cmOkRC;
}

cmRC_t      cmMtxFileFloatExec(  cmMtxFile* p, const float* inPtr, unsigned inCnt, unsigned inStride )
{
  const float* ep = inPtr + (inCnt * inStride);
  for(; inPtr < ep; inPtr+=inStride )
    fprintf(p->fp,"%e ",*inPtr);
  fprintf(p->fp,"\n");
  return cmOkRC;
}

cmRC_t      cmMtxFileDoubleExec( cmMtxFile* p, const double* inPtr, unsigned inCnt, unsigned inStride )
{
  const double* ep = inPtr + (inCnt * inStride);
  for(; inPtr < ep; inPtr+=inStride )
    fprintf(p->fp,"%e ",*inPtr);
  fprintf(p->fp,"\n");

  return cmOkRC;
}

cmRC_t      cmMtxFileComplexExec( cmMtxFile* p, const cmComplexR_t* inPtr, unsigned inCnt, unsigned inStride )
{
  const cmComplexR_t* sp = inPtr;
  const cmComplexR_t* ep = inPtr + (inCnt * inStride);

  for(; sp < ep; sp += inStride )
    fprintf(p->fp,"%e ",cmCrealR(*sp));
  fprintf(p->fp,"\n");

  sp = inPtr;

  for(; sp < ep; sp += inStride )
    fprintf(p->fp,"%e ",cmCimagR(*sp));
  fprintf(p->fp,"\n");

  return cmOkRC;
}

