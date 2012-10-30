#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmComplexTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmFile.h"
#include "cmSymTbl.h"
#include "cmJson.h"
#include "cmPrefs.h"
#include "cmDspValue.h"
#include "cmMsgProtocol.h"
#include "cmThread.h"
#include "cmUdpPort.h"
#include "cmUdpNet.h"
#include "cmAudioSys.h"
#include "cmDspCtx.h"
#include "cmDspClass.h"
#include "cmDspUi.h"
#include "cmOp.h"
#include "cmMath.h"

#include "cmAudioFile.h"
#include "cmFileSys.h"
#include "cmProcObj.h"
#include "cmProcTemplateMain.h"
#include "cmProc.h"
#include "cmMidi.h"
#include "cmProc2.h"
#include "cmVectOpsTemplateMain.h"

enum
{
  kWndSmpCntKrId,
  kHopFactKrId,
  kModeKrId,
  kThreshKrId,
  kLwrSlopeKrId,
  kUprSlopeKrId,
  kOffsetKrId,
  kInvertKrId,
  kAudioInKrId,
  kAudioOutKrId
};

typedef struct
{
  cmDspInst_t   inst;
  cmCtx*        ctx;
  cmSpecDist_t* sdp;
} cmDspKr_t;
 
cmDspClass_t _cmKrDC;

// cm console output function
void _cmKrCmRptFunc( void* userDataPtr, const cmChar_t* fmt, va_list vl )
{
  cmCtx_t* p = (cmCtx_t*)userDataPtr;
  if( p == NULL )
    vprintf(fmt,vl);
  else
    cmRptVPrintf(&p->rpt,fmt,vl);
}

// initialize the cm library
/*
cmDspRC_t cmDspKrCmLibInit(cmCtx_t* cmCtx )
{
  bool debugFl = false;
#ifdef NDEBUG
  debugFl = true;
#endif

  unsigned memPadByteCnt       = cmCtx->guardByteCnt;
  unsigned memAlignByteCnt     = cmCtx->alignByteCnt;
  unsigned memAutoBlockByteCnt = 0;
  unsigned memFlags            = cmCtx->mmFlags;

  if( cmMallocDebugIsInit() == false )
    cmMallocDebugInitialize( memPadByteCnt, memAlignByteCnt, memAutoBlockByteCnt, memFlags, _cmKrCmRptFunc, cmCtx  );
  
  return kOkDspRC;
}

// free the cm library
cmDspRC_t cmDspKrCmLibFinal()
{
#ifdef NDEBUG
  cmMallocDebugReport( vPrintF, _rptUserPtr, 0);
#endif

  if( cmMallocDebugIsInit()  )
    cmMallocDebugFinalize();

  return kOkDspRC;
}
*/

  cmDspInst_t*  _cmDspKrAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "wndn",    kWndSmpCntKrId,   0, 0,   kInDsvFl  | kUIntDsvFl   | kReqArgDsvFl,   "Window sample count"   },
    { "hopf",    kHopFactKrId,     0, 0,   kInDsvFl  | kUIntDsvFl   | kOptArgDsvFl,   "Hop factor" },
    { "mode",    kModeKrId,        0, 0,   kInDsvFl  | kUIntDsvFl   | kOptArgDsvFl,   "Mode 0=bypass 1=basic 2=spec cnt 3=amp env" },
    { "thrh",    kThreshKrId,      0, 0,   kInDsvFl  | kDoubleDsvFl | kOptArgDsvFl,   "Threshold" },
    { "lwrs",    kLwrSlopeKrId,    0, 0,   kInDsvFl  | kDoubleDsvFl | kOptArgDsvFl,   "Lower Slope"},
    { "uprs",    kUprSlopeKrId,    0, 0,   kInDsvFl  | kDoubleDsvFl | kOptArgDsvFl,   "Upper Slope"},
    { "offs",    kOffsetKrId,      0, 0,   kInDsvFl  | kDoubleDsvFl | kOptArgDsvFl,   "Offset"},
    { "invt",    kInvertKrId,      0, 0,   kInDsvFl  | kUIntDsvFl   | kOptArgDsvFl,   "Invert"},
    { "in",      kAudioInKrId,     0, 0,   kInDsvFl  | kAudioBufDsvFl, "Audio Input" },
    { "out",     kAudioOutKrId,    0, 1,   kOutDsvFl | kAudioBufDsvFl, "Audio Output" },
    { NULL, 0, 0, 0, 0 }
  };

  cmDspKr_t* p            = cmDspInstAlloc(cmDspKr_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);
  unsigned   defWndSmpCnt = cmDspDefaultUInt(&p->inst,kWndSmpCntKrId);
  unsigned   wndSmpCnt    = cmNextPowerOfTwo( defWndSmpCnt );
  
  cmDspSetDefaultUInt(   ctx,&p->inst, kWndSmpCntKrId, defWndSmpCnt, wndSmpCnt );
  cmDspSetDefaultUInt(   ctx,&p->inst, kHopFactKrId,  0, 4 );
  cmDspSetDefaultUInt(   ctx,&p->inst, kModeKrId,     0, kBasicModeSdId );
  cmDspSetDefaultDouble( ctx,&p->inst, kThreshKrId,   0, 60.0 );
  cmDspSetDefaultDouble( ctx,&p->inst, kLwrSlopeKrId, 0, 2.0 );
  cmDspSetDefaultDouble( ctx,&p->inst, kUprSlopeKrId, 0, 0.0 );
  cmDspSetDefaultDouble( ctx,&p->inst, kOffsetKrId,   0, 30.0);
  cmDspSetDefaultUInt(   ctx,&p->inst, kInvertKrId,   0, 0 );
  //_cmDspKrCmInit(ctx,p); // initialize the cm library

  p->ctx = cmCtxAlloc(NULL,ctx->rpt,ctx->lhH,ctx->stH);

  return &p->inst;
}

cmDspRC_t _cmDspKrFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t rc = kOkDspRC;
  cmDspKr_t* p = (cmDspKr_t*)inst;

  cmSpecDistFree(&p->sdp);

  cmCtxFree(&p->ctx);
  //_cmDspKrCmFinal(ctx,p);  // finalize the cm library

  return rc;
}


cmDspRC_t _cmDspKrSetup(cmDspCtx_t* ctx, cmDspKr_t* p )
{
  cmDspRC_t rc           = kOkDspRC;
  unsigned  wndSmpCnt    = cmDspUInt(&p->inst,kWndSmpCntKrId);
  unsigned  hopFact      = cmDspUInt(&p->inst,kHopFactKrId);
  unsigned  olaWndTypeId = kHannWndId;

  cmSpecDistFree(&p->sdp);
  p->sdp = cmSpecDistAlloc(p->ctx, NULL, cmDspSamplesPerCycle(ctx), cmDspSampleRate(ctx), wndSmpCnt, hopFact, olaWndTypeId);

  assert(p->sdp != NULL );

  if((rc = cmDspZeroAudioBuf(ctx,&p->inst,kAudioOutKrId)) != kOkDspRC )
    return rc;
  
  return rc;
}


cmDspRC_t _cmDspKrReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspKr_t*   p  = (cmDspKr_t*)inst;
  cmDspRC_t    rc;

  if((rc = cmDspApplyAllDefaults(ctx,inst)) != kOkDspRC )
    return rc;

  return _cmDspKrSetup(ctx,p);
}

cmDspRC_t _cmDspKrExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspKr_t* p = (cmDspKr_t*)inst;
  cmDspRC_t rc = kOkDspRC;

  unsigned          iChIdx  = 0;
  const cmSample_t* ip      = cmDspAudioBuf(ctx,inst,kAudioInKrId,iChIdx);
  unsigned          iSmpCnt = cmDspVarRows(inst,kAudioInKrId);
  
  unsigned          oChIdx  = 0;
  cmSample_t*       op      = cmDspAudioBuf(ctx,inst,kAudioOutKrId,oChIdx);
  unsigned          oSmpCnt = cmDspVarRows(inst,kAudioOutKrId);
  const cmSample_t* sp;

  cmSpecDistExec(p->sdp,ip,iSmpCnt);
  
  if((sp = cmSpecDistOut(p->sdp)) != NULL )
    vs_Copy(op,sp,oSmpCnt);
  
  return rc;
}

cmDspRC_t _cmDspKrRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspKr_t* p = (cmDspKr_t*)inst;
  cmDspRC_t rc = kOkDspRC;

  cmDspSetEvent(ctx,inst,evt);

  switch( evt->dstVarId )
  {
    case kWndSmpCntKrId:
    case kHopFactKrId:
      _cmDspKrSetup(ctx,p);
      printf("wsn:%i hsn:%i\n",p->sdp->wndSmpCnt,p->sdp->hopSmpCnt);
      break;

    case kModeKrId:
      p->sdp->mode = cmDspUInt(inst,kModeKrId);
      printf("mode:%i\n",p->sdp->mode);
      break;
      
    case kThreshKrId:     
      p->sdp->thresh   = cmDspDouble(inst,kThreshKrId);   
      break;

    case kUprSlopeKrId:   
      p->sdp->uprSlope = cmDspDouble(inst,kUprSlopeKrId); 
      printf("upr slope:%f\n",p->sdp->uprSlope);
      break;

    case kLwrSlopeKrId:   
      p->sdp->lwrSlope = cmDspDouble(inst,kLwrSlopeKrId); 
      printf("upr slope:%f\n",p->sdp->lwrSlope);
      break;

    case kOffsetKrId:     
      p->sdp->offset   = cmDspDouble(inst,kOffsetKrId);   
      break;

    case kInvertKrId:     
      p->sdp->invertFl = cmDspUInt(inst,kInvertKrId)!=0;  
      break;

    default:
      { assert(0); }
  }

  return rc;
}

struct cmDspClass_str* cmKrClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmKrDC,ctx,"Kr",
    NULL,
    _cmDspKrAlloc,
    _cmDspKrFree,
    _cmDspKrReset,
    _cmDspKrExec,
    _cmDspKrRecv,
    NULL,NULL,
    "Fourier based non-linear transformer.");

  return &_cmKrDC;
}


