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
#include "cmMidi.h"
#include "cmMidiFile.h"
#include "cmAudioFile.h"
#include "cmScore.h"
#include "cmTimeLine.h"
#include "cmScoreProc.h"

enum
{
  kOkSpRC,
  kJsonFailSpRC,
  kScoreFailSpRC,
  kTimeLineFailSpRC
};


typedef struct
{
  cmErr_t         err;
  cmScH_t         scH;
  const cmChar_t* tlFn;
  cmTlH_t         tlH;
  cmJsonH_t       jsH;
  unsigned*       dynArray;
  unsigned        dynCnt;
  double          srate;
} cmSp_t;


cmSpRC_t _cmJsonReadDynArray( cmJsonH_t jsH, unsigned** dynArray, unsigned* dynCnt )
{
  cmJsonNode_t* np;
  int i;

  if( cmJsonPathToArray(jsH, NULL, NULL, "dynRef", &np ) != kOkJsRC )
    return kJsonFailSpRC;

  *dynCnt = cmJsonChildCount(np);

  *dynArray = cmMemAllocZ(unsigned,*dynCnt);
  
  for(i=0; i<*dynCnt; ++i)
    if( cmJsonUIntValue( cmJsonArrayElement(np,i), (*dynArray)+i ) != kOkJsRC )
      return kJsonFailSpRC;

  return kOkSpRC;  
}

cmSpRC_t _cmScoreProcInit( cmCtx_t* ctx, cmSp_t* p, const cmChar_t* rsrcFn  )
{
  cmSpRC_t        rc     = kOkSpRC;
  const cmChar_t* scFn   = NULL;
  const cmChar_t* tlFn   = NULL;

  p->srate = 96000;

  cmErrSetup(&p->err,&ctx->rpt,"ScoreProc");

  // open the resource file
  if( cmJsonInitializeFromFile( &p->jsH, rsrcFn, ctx ) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailSpRC,"Unable to load the main resource file:%s.",cmStringNullGuard(rsrcFn));
    goto errLabel;
  }

  // get the time line fn
  if( cmJsonPathToString( p->jsH, NULL, NULL, "timeLineFn", &tlFn ) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailSpRC,"Unable to locate the time line file name in the main resource file:%s",cmStringNullGuard(rsrcFn));
    goto errLabel;
  }

  // get the score file name
  if( cmJsonPathToString( p->jsH, NULL, NULL, "scoreFn", &scFn ) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailSpRC,"Unable to locate the score file name in the main resource file:%s",cmStringNullGuard(rsrcFn));
    goto errLabel;
  }

  // read the dynamics reference array
  if((rc = _cmJsonReadDynArray( p->jsH, &p->dynArray, &p->dynCnt )) != kOkSpRC )
  {
    rc = cmErrMsg(&p->err,rc,"Unable to read dynamics reference array resource from the main resource file:%s",cmStringNullGuard(rsrcFn));
    goto errLabel;
  }


  // load the score file
  if( cmScoreInitialize(ctx, &p->scH, scFn, p->srate, NULL, 0, NULL, NULL, cmSymTblNullHandle ) != kOkScRC )
  {
    rc = cmErrMsg(&p->err,kScoreFailSpRC,"Score load failed for score file:%s.",cmStringNullGuard(scFn));
    goto errLabel;
  }

  if( cmTimeLineInitialize(ctx, &p->tlH, NULL, NULL ) != kOkTlRC )
  {
    rc = cmErrMsg(&p->err,kTimeLineFailSpRC,"Time line load failed for time line file:%s.",cmStringNullGuard(tlFn));
    goto errLabel;
  }

 errLabel:
  return rc;
}

cmSpRC_t _cmScoreProcFinal( cmSp_t* p )
{
  cmSpRC_t rc = kOkSpRC;

  if( cmScoreFinalize(&p->scH) != kOkScRC )
    cmErrMsg(&p->err,kScoreFailSpRC,"Score finalize failed.");

  if( cmTimeLineFinalize(&p->tlH) != kOkTlRC )
    cmErrMsg(&p->err,kTimeLineFailSpRC,"Time line finalize failed.");

  if( cmJsonFinalize(&p->jsH) != kOkJsRC )
    cmErrMsg(&p->err,kJsonFailSpRC,"JSON finalize failed.");

  cmMemFree(p->dynArray);

  return rc;
}

unsigned cmScoreProc(cmCtx_t* ctx)
{
  cmSpRC_t rc = kOkSpRC;
  const cmChar_t* rsrcFn = "/home/kevin/.kc/time_line.js";
  cmSp_t sp;

  memset(&sp,0,sizeof(sp));

  cmRptPrintf(&ctx->rpt,"Score Proc Start\n");

  if((rc = _cmScoreProcInit(ctx,&sp,rsrcFn)) != kOkSpRC )
    goto errLabel;
  


 
 errLabel:
  _cmScoreProcFinal(&sp);

  cmRptPrintf(&ctx->rpt,"Score Proc End\n");

  return rc;
  
}

