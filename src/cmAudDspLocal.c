//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmJson.h"
#include "dsp/cmDspValue.h"
#include "cmMsgProtocol.h"
#include "cmAudDsp.h"
#include "cmAudDspIF.h"
#include "cmAudDspLocal.h"


cmAdlH_t cmAdlNullHandle = cmSTATIC_NULL_HANDLE;

typedef struct
{
  cmErr_t err;
  cmAiH_t aiH;
  cmAdH_t adH;
} cmAdl_t;

cmAdl_t* _cmAdlHandleToPtr( cmAdlH_t h )
{
  cmAdl_t* p = (cmAdl_t*)h.h;
  assert( p != NULL );
  return p;
}

// Forward messages coming from the audio DSP system to the audio DSP IF
// for later dispatch to the client application.
cmMsgRC_t _cmAudDspLocalCallback(void* cbDataPtr, unsigned msgByteCnt, const void* msg )
{
  cmMsgRC_t rc = kOkMsgRC;
  cmAdl_t*  p  = (cmAdl_t*)cbDataPtr;

  if( cmAdIfRecvAudDspMsg(p->aiH, msgByteCnt, msg ) != kOkAiRC )
  {
    cmErrMsg(&p->err,kAudDspIfFailAdlRC,"Message transmission to the audio DSP interface failed.");
    rc =  kSendFailMsgRC;
  }

  return rc;
}

// Forward messages from the audio DSP interface to the audio DSP system.
cmMsgRC_t  _cmAdlAudDspSendFunc( void* cbDataPtr, unsigned msgByteCnt, const void* msg )
{
  cmMsgRC_t rc = kOkMsgRC;
  cmAdl_t*  p  = (cmAdl_t*)cbDataPtr;

  if( cmAudDspReceiveClientMsg( p->adH, msgByteCnt, msg ) != kOkAdRC )
  {
    cmErrMsg(&p->err,kAudDspFailAdlRC,"Message transmission the audio DSP system failed.");
    rc = kSendFailMsgRC;
  }

  return rc;

}

cmAdlRC_t _cmAudDspLocalFree( cmAdl_t* p )
{
  cmAdlRC_t rc = kOkAdlRC;

  if( cmAdIfFree(&p->aiH) != kOkAiRC )
  {
    rc = cmErrMsg(&p->err,kAudDspIfFailAdlRC,"The audio DSP interface release failed.");
    goto errLabel;
  }

  if( cmAudDspFree(&p->adH) != kOkAdRC )
  {
    rc = cmErrMsg(&p->err,kAudDspFailAdlRC,"The audio DSP release failed.");
    goto errLabel;
  }

  cmMemFree(p);
 errLabel:
  return rc;
}



cmAdlRC_t cmAudDspLocalAllocate( cmCtx_t* ctx, cmAdlH_t* hp, const cmAdIfDispatch_t* recd )
{
  cmAdlRC_t rc;
  if((rc = cmAudDspLocalFree(hp)) != kOkAdlRC )
    return rc;

  cmAdl_t* p = cmMemAllocZ(cmAdl_t,1);
  cmErrSetup(&p->err,&ctx->rpt,"Audio DSP Local");

  cmAdIfParm_t parms;
  parms.dispatchRecd      = *recd;
  parms.audDspFunc        = _cmAdlAudDspSendFunc;
  parms.audDspFuncDataPtr = p;

  if( cmAdIfAllocate(ctx, &p->aiH, &parms  ) != kOkAiRC )
  {
    rc = cmErrMsg(&p->err,kAudDspIfFailAdlRC,"The audio DSP interface system allocation failed.");
    goto errLabel;
  }

  if( cmAudDspAlloc(ctx, &p->adH, _cmAudDspLocalCallback, p ) != kOkAdRC )
  {
    rc = cmErrMsg(&p->err,kAudDspFailAdlRC,"The audio DSP system allocation failed.");
    goto errLabel;
  }

  hp->h = p;

 errLabel:
  if( rc != kOkAdlRC )
    _cmAudDspLocalFree(p);

  return rc;
}

cmAdlRC_t cmAudDspLocalFree( cmAdlH_t* hp )
{
  cmAdlRC_t rc = kOkAdlRC;

  if( hp == NULL || cmAudDspLocalIsValid(*hp) == false )
    return kOkAdlRC;

  cmAdl_t* p = _cmAdlHandleToPtr(*hp);

  if((rc = _cmAudDspLocalFree(p)) != kOkAdlRC )
    return rc;

  hp->h = NULL;
  return rc;
}

cmAdlRC_t cmAudDspLocalSendSetup( cmAdlH_t h )
{
  cmAdlRC_t rc = kOkAdlRC;
  cmAdl_t* p = _cmAdlHandleToPtr(h);
  if( cmAudDspSendSetup(p->adH) != kOkAdRC )
  {
    rc = cmErrMsg(&p->err,kAudDspFailAdlRC,"The audio DSP system setup request failed.");
    goto errLabel;
  }

 errLabel:
  return rc;
}


bool      cmAudDspLocalIsValid( cmAdlH_t h )
{ return h.h != NULL; }

cmAiH_t   cmAudDspLocalIF_Handle( cmAdlH_t h )
{ 
  cmAdl_t* p = _cmAdlHandleToPtr(h);
  return p->aiH;
}

