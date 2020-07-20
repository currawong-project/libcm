#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"

  void cmCtxSetup( 
    cmCtx_t*         ctx, 
    const cmChar_t*  title,
    cmRptPrintFunc_t prtFunc, 
    cmRptPrintFunc_t errFunc, 
    void*            cbPtr, 
    unsigned         guardByteCnt,
    unsigned         alignByteCnt,
    unsigned         mmFlags )
  {
    cmRptSetup(&ctx->rpt,prtFunc,errFunc,cbPtr);
    cmErrSetup(&ctx->err,&ctx->rpt,title);
    ctx->guardByteCnt = guardByteCnt;
    ctx->alignByteCnt = alignByteCnt;
    ctx->mmFlags      = mmFlags;
  }
