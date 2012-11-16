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

#include "cmAudioFile.h"
#include "cmMidiFile.h"
#include "cmTimeLine.h"

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


//==========================================================================================================================================

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


//==========================================================================================================================================
enum
{
  kTlFileTlId,
  kAudPathTlId,
  kSelTlId,
  kAudFnTlId,
  kBegSmpIdxTlId,
  kEndSmpIdxTlId
};

cmDspClass_t _cmTimeLineDC;

typedef struct
{
  cmDspInst_t inst;
  cmTlH_t     tlH;
} cmDspTimeLine_t;

cmDspInst_t*  _cmDspTimeLineAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "tlfile",  kTlFileTlId,    0, 0, kInDsvFl   | kStrzDsvFl | kReqArgDsvFl,   "Time line file." },
    { "path",    kAudPathTlId,   0, 0, kInDsvFl   | kStrzDsvFl | kReqArgDsvFl,   "Audio path"    },
    { "sel",     kSelTlId,       0, 0, kInDsvFl   | kInDsvFl   | kOutDsvFl  | kUIntDsvFl,   "Selected marker id."},
    { "afn",     kAudFnTlId,     0, 0, kOutDsvFl  | kStrzDsvFl,   "Selected Audio file." },
    { "bsi",     kBegSmpIdxTlId, 0, 0, kOutDsvFl  | kUIntDsvFl,   "Begin audio sample index."},
    { "esi",     kEndSmpIdxTlId, 0, 0, kOutDsvFl  | kUIntDsvFl,   "End audio sample index."},
    { NULL, 0, 0, 0, 0 }
  };

  cmDspTimeLine_t* p = cmDspInstAlloc(cmDspTimeLine_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);
  
  cmDspSetDefaultUInt(  ctx, &p->inst,  kSelTlId,       0, cmInvalidId);
  cmDspSetDefaultStrcz( ctx, &p->inst,  kAudFnTlId,     NULL, "");
  cmDspSetDefaultUInt(  ctx, &p->inst,  kBegSmpIdxTlId, 0, cmInvalidIdx);
  cmDspSetDefaultUInt(  ctx, &p->inst,  kEndSmpIdxTlId, 0, cmInvalidIdx);

  // create the UI control
  cmDspUiTimeLineCreate(ctx,&p->inst,kTlFileTlId,kAudPathTlId,kSelTlId);

  p->tlH = cmTimeLineNullHandle;

  return &p->inst;
}

cmDspRC_t _cmDspTimeLineFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t        rc = kOkDspRC;
  cmDspTimeLine_t* p = (cmDspTimeLine_t*)inst;

  if( cmTimeLineFinalize(&p->tlH) != kOkTlRC )
    return cmErrMsg(&inst->classPtr->err, kInstFinalFailDspRC, "Time-line finalize failed.");

  return rc;
}


cmDspRC_t _cmDspTimeLineReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t        rc = kOkDspRC;
  cmDspTimeLine_t* p  = (cmDspTimeLine_t*)inst;

  cmDspApplyAllDefaults(ctx,inst);

  const cmChar_t* tlFn;
  if((tlFn =  cmDspStrcz(inst, kTlFileTlId )) !=  NULL )
    if( cmTimeLineInitializeFromFile(ctx->cmCtx, &p->tlH, NULL, NULL, tlFn ) != kOkTlRC )
      rc = cmErrMsg(&inst->classPtr->err, kInstResetFailDspRC, "Time-line file open failed.");

  return rc;
}

cmDspRC_t _cmDspTimeLineRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{

  switch( evt->dstVarId )
  {
    case kSelTlId:
      cmDspSetEvent(ctx,inst,evt);
      
      break;

    default:
      {assert(0);}
  }

  return kOkDspRC;
}

struct cmDspClass_str* cmTimeLineClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmTimeLineDC,ctx,"TimeLine",
    NULL,
    _cmDspTimeLineAlloc,
    _cmDspTimeLineFree,
    _cmDspTimeLineReset,
    NULL,
    _cmDspTimeLineRecv,
    NULL,NULL,
    "Time Line control.");

  return &_cmTimeLineDC;
}

