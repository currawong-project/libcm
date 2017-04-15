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
#include "cmText.h"
#include "cmPrefs.h"
#include "cmDspValue.h"
#include "cmMsgProtocol.h"
#include "cmThread.h"
#include "cmUdpPort.h"
#include "cmUdpNet.h"
//( { file_desc:"'snap' audio effects performance analysis units." kw:[snap]}

#include "cmTime.h"
#include "cmAudioSys.h"
#include "cmDspCtx.h"
#include "cmDspClass.h"
#include "cmDspStore.h"
#include "cmDspUi.h"
#include "cmDspSys.h"
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
#include "cmScore.h"
#include "cmProc4.h"
#include "cmProc5.h"
#include "cmSyncRecd.h"
#include "cmTakeSeqBldr.h"

//------------------------------------------------------------------------------------------------------------
//)
//( { label:cmDspKr file_desc:"Spectral non-linear distortion effect." kw:[sunit] }

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
  kBypassKrId,
  kWetKrId,
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
    { "bypass",  kBypassKrId,      0, 0,   kInDsvFl  | kBoolDsvFl   | kOptArgDsvFl,   "Bypass enable flag." },
    { "wet",     kWetKrId,         0, 0,   kInDsvFl  | kSampleDsvFl,                  "Wet mix level."},
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
  cmDspSetDefaultUInt(   ctx,&p->inst, kBypassKrId,   0, 0 );
  cmDspSetDefaultSample( ctx,&p->inst, kWetKrId,      0, 1.0);

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
  unsigned  olaWndTypeId =kHannWndId;

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

  // if no connected
  if( iSmpCnt == 0 )
    return rc;
  
  unsigned          oChIdx  = 0;
  cmSample_t*       op      = cmDspAudioBuf(ctx,inst,kAudioOutKrId,oChIdx);
  unsigned          oSmpCnt = cmDspVarRows(inst,kAudioOutKrId);
  const cmSample_t* sp;

  cmSample_t wet = cmDspSample(inst,kWetKrId);

  cmSpecDistExec(p->sdp,ip,iSmpCnt);
  
  if((sp = cmSpecDistOut(p->sdp)) != NULL )
  {
    cmVOS_MultVVS(op,oSmpCnt,sp,wet);
  }

  if( wet<1.0 )
    cmVOS_MultSumVVS(op,oSmpCnt,ip,1.0-wet);

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

      // THIS IS A HACK
      // WHEN WND OR HOP CHANGE THE RESULTING CHANGES
      // SHOULD BE ISOLATED IN cmSpecDist() AND THE
      // CURRENT STATE OF THE PARAMETERS SHOULD NOT BE
      // LOST - IF THE CHANGES WERE ISOLATED WITHIN PVANL 
      // AND PVSYN IT MIGHT BE POSSIBLE TO DO WITH 
      // MINIMAL AUDIO INTERUPTION.

      p->sdp->mode = cmDspUInt(inst,kModeKrId);
      p->sdp->thresh   = cmDspDouble(inst,kThreshKrId);   
      p->sdp->uprSlope = cmDspDouble(inst,kUprSlopeKrId); 
      p->sdp->lwrSlope = cmDspDouble(inst,kLwrSlopeKrId); 
      p->sdp->offset   = cmDspDouble(inst,kOffsetKrId);   
      p->sdp->invertFl = cmDspUInt(inst,kInvertKrId)!=0;  

      printf("wsn:%i hsn:%i\n",p->sdp->wndSmpCnt,p->sdp->hopSmpCnt);
      break;

    case kModeKrId:
      p->sdp->mode = cmDspUInt(inst,kModeKrId);
      printf("mode:%i\n",p->sdp->mode);
      break;
      
    case kThreshKrId:     
      p->sdp->thresh   = cmDspDouble(inst,kThreshKrId);   
      //printf("thr:p:%p sdp:%p %f\n",p,p->sdp,p->sdp->thresh);
      break;

    case kUprSlopeKrId:   
      p->sdp->uprSlope = cmDspDouble(inst,kUprSlopeKrId); 
      //printf("upr slope:%f\n",p->sdp->uprSlope);
      break;

    case kLwrSlopeKrId:   
      p->sdp->lwrSlope = cmDspDouble(inst,kLwrSlopeKrId); 
      //printf("upr slope:%f\n",p->sdp->lwrSlope);
      break;

    case kOffsetKrId:     
      p->sdp->offset   = cmDspDouble(inst,kOffsetKrId);   
      break;

    case kInvertKrId:     
      p->sdp->invertFl = cmDspUInt(inst,kInvertKrId)!=0;  
      break;

    case kWetKrId:
      break;

    default:
      { assert(0); }
  }

  return rc;
}

cmDspClass_t* cmKrClassCons( cmDspCtx_t* ctx )
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

//------------------------------------------------------------------------------------------------------------
//)
//( { label:cmDspKr2 file_desc:"Spectral non-linear distortion effect." kw:[sunit] }

enum
{
  kWndSmpCntKr2Id,
  kHopFactKr2Id,
  
  kCeilKr2Id,
  kExpoKr2Id,
  
  kThreshKr2Id,
  kLwrSlopeKr2Id,
  kUprSlopeKr2Id,

  kMixKr2Id,

  kWetKr2Id,
  kAudioInKr2Id,
  kAudioOutKr2Id
};

typedef struct
{
  cmDspInst_t   inst;
  cmCtx*        ctx;
  cmSpecDist2_t* sdp;
} cmDspKr2_t;
 
cmDspClass_t _cmKr2DC;


cmDspInst_t*  _cmDspKr2Alloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "wndn",    kWndSmpCntKr2Id,   0, 0,   kInDsvFl  | kUIntDsvFl   | kReqArgDsvFl,   "Window sample count"   },
    { "hopf",    kHopFactKr2Id,     0, 0,   kInDsvFl  | kUIntDsvFl   | kOptArgDsvFl,   "Hop factor" },
    
    { "ceil",    kCeilKr2Id,     0, 0,   kInDsvFl  | kDoubleDsvFl | kOptArgDsvFl,   "Ceiling" },
    { "expo",    kExpoKr2Id,        0, 0,   kInDsvFl  | kDoubleDsvFl | kOptArgDsvFl,   "Exponent" },
    
    { "thrh",    kThreshKr2Id,      0, 0,   kInDsvFl  | kDoubleDsvFl | kOptArgDsvFl,   "Threshold" },
    { "lwrs",    kLwrSlopeKr2Id,    0, 0,   kInDsvFl  | kDoubleDsvFl | kOptArgDsvFl,   "Lower Slope"},
    { "uprs",    kUprSlopeKr2Id,    0, 0,   kInDsvFl  | kDoubleDsvFl | kOptArgDsvFl,   "Upper Slope"},

    { "mix",     kMixKr2Id,         0, 0,   kInDsvFl  | kDoubleDsvFl | kOptArgDsvFl,   "Mix"},
    
    { "wet",     kWetKr2Id,         0, 0,   kInDsvFl  | kSampleDsvFl,                  "Wet mix level."},
    { "in",      kAudioInKr2Id,     0, 0,   kInDsvFl  | kAudioBufDsvFl, "Audio Input" },
    { "out",     kAudioOutKr2Id,    0, 1,   kOutDsvFl | kAudioBufDsvFl, "Audio Output" },
    { NULL, 0, 0, 0, 0 }
  };

  cmDspKr2_t* p           = cmDspInstAlloc(cmDspKr2_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);
  unsigned   defWndSmpCnt = cmDspDefaultUInt(&p->inst,kWndSmpCntKr2Id);
  unsigned   wndSmpCnt    = cmNextPowerOfTwo( defWndSmpCnt );
  
  cmDspSetDefaultUInt(   ctx,&p->inst, kWndSmpCntKr2Id, defWndSmpCnt, wndSmpCnt );
  cmDspSetDefaultUInt(   ctx,&p->inst, kHopFactKr2Id,  0, 4 );
  
  cmDspSetDefaultDouble( ctx,&p->inst, kCeilKr2Id,  0, 20.0 );
  cmDspSetDefaultDouble( ctx,&p->inst, kExpoKr2Id,     0,  2.0 );

  cmDspSetDefaultDouble( ctx,&p->inst, kThreshKr2Id,   0, 60.0 );
  cmDspSetDefaultDouble( ctx,&p->inst, kLwrSlopeKr2Id, 0, 2.0 );
  cmDspSetDefaultDouble( ctx,&p->inst, kUprSlopeKr2Id, 0, 0.0 );
  
  cmDspSetDefaultDouble( ctx,&p->inst, kMixKr2Id,      0, 0.0 );
  
  cmDspSetDefaultSample( ctx,&p->inst, kWetKr2Id,      0, 1.0);

  //_cmDspKr2CmInit(ctx,p); // initialize the cm library

  p->ctx = cmCtxAlloc(NULL,ctx->rpt,ctx->lhH,ctx->stH);

  return &p->inst;
}

cmDspRC_t _cmDspKr2Free(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t rc = kOkDspRC;
  cmDspKr2_t* p = (cmDspKr2_t*)inst;

  cmSpecDist2Free(&p->sdp);

  cmCtxFree(&p->ctx);
  //_cmDspKr2CmFinal(ctx,p);  // finalize the cm library

  return rc;
}


cmDspRC_t _cmDspKr2Setup(cmDspCtx_t* ctx, cmDspKr2_t* p )
{
  cmDspRC_t rc           = kOkDspRC;
  unsigned  wndSmpCnt    = cmDspUInt(&p->inst,kWndSmpCntKr2Id);
  unsigned  hopFact      = cmDspUInt(&p->inst,kHopFactKr2Id);
  unsigned  olaWndTypeId =kHannWndId;

  cmSpecDist2Free(&p->sdp);

  p->sdp = cmSpecDist2Alloc(p->ctx, NULL, cmDspSamplesPerCycle(ctx), cmDspSampleRate(ctx), wndSmpCnt, hopFact, olaWndTypeId);

  assert(p->sdp != NULL );

  if((rc = cmDspZeroAudioBuf(ctx,&p->inst,kAudioOutKr2Id)) != kOkDspRC )
    return rc;
  
  return rc;
}


cmDspRC_t _cmDspKr2Reset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspKr2_t*   p  = (cmDspKr2_t*)inst;
  cmDspRC_t    rc;

  if((rc = cmDspApplyAllDefaults(ctx,inst)) != kOkDspRC )
    return rc;

  return _cmDspKr2Setup(ctx,p);
}

cmDspRC_t _cmDspKr2Exec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspKr2_t* p = (cmDspKr2_t*)inst;
  cmDspRC_t rc = kOkDspRC;

  unsigned          iChIdx  = 0;
  const cmSample_t* ip      = cmDspAudioBuf(ctx,inst,kAudioInKr2Id,iChIdx);
  unsigned          iSmpCnt = cmDspVarRows(inst,kAudioInKr2Id);

  // if no connected
  if( iSmpCnt == 0 )
    return rc;
  
  unsigned          oChIdx  = 0;
  cmSample_t*       op      = cmDspAudioBuf(ctx,inst,kAudioOutKr2Id,oChIdx);
  unsigned          oSmpCnt = cmDspVarRows(inst,kAudioOutKr2Id);
  const cmSample_t* sp;

  cmSample_t wet = cmDspSample(inst,kWetKr2Id);

  cmSpecDist2Exec(p->sdp,ip,iSmpCnt);
  
  if((sp = cmSpecDist2Out(p->sdp)) != NULL )
  {
    cmVOS_MultVVS(op,oSmpCnt,sp,wet);
  }

  if( wet<1.0 )
    cmVOS_MultSumVVS(op,oSmpCnt,ip,1.0-wet);

  return rc;
}

cmDspRC_t _cmDspKr2Recv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspKr2_t* p = (cmDspKr2_t*)inst;
  cmDspRC_t rc = kOkDspRC;


  cmDspSetEvent(ctx,inst,evt);

  switch( evt->dstVarId )
  {
    case kWndSmpCntKr2Id:
    case kHopFactKr2Id:
      _cmDspKr2Setup(ctx,p);

      // THIS IS A HACK
      // WHEN WND OR HOP CHANGE THE RESULTING CHANGES
      // SHOULD BE ISOLATED IN cmSpecDist() AND THE
      // CURRENT STATE OF THE PARAMETERS SHOULD NOT BE
      // LOST - IF THE CHANGES WERE ISOLATED WITHIN PVANL 
      // AND PVSYN IT MIGHT BE POSSIBLE TO DO WITH 
      // MINIMAL AUDIO INTERUPTION.


      p->sdp->ceiling   = cmDspDouble(inst,kCeilKr2Id);   
      p->sdp->expo      = cmDspDouble(inst,kExpoKr2Id);   
      
      p->sdp->thresh   = cmDspDouble(inst,kThreshKr2Id);   
      p->sdp->uprSlope = cmDspDouble(inst,kUprSlopeKr2Id); 
      p->sdp->lwrSlope = cmDspDouble(inst,kLwrSlopeKr2Id); 

      p->sdp->mix      = cmDspDouble(inst,kMixKr2Id);
      
      printf("wsn:%i hsn:%i\n",p->sdp->wndSmpCnt,p->sdp->hopSmpCnt);
      break;

    case kCeilKr2Id:     
      p->sdp->ceiling  = cmDspDouble(inst,kCeilKr2Id);   
      break;
    
    case kExpoKr2Id:     
      p->sdp->expo  = cmDspDouble(inst,kExpoKr2Id);   
      break;
      
    case kThreshKr2Id:     
      p->sdp->thresh   = cmDspDouble(inst,kThreshKr2Id);   
      break;

    case kUprSlopeKr2Id:   
      p->sdp->uprSlope = cmDspDouble(inst,kUprSlopeKr2Id); 
      break;

    case kLwrSlopeKr2Id:   
      p->sdp->lwrSlope = cmDspDouble(inst,kLwrSlopeKr2Id); 
      break;

    case kMixKr2Id:   
      p->sdp->mix = cmDspDouble(inst,kMixKr2Id); 
      break;

      
    case kWetKr2Id:
      break;

    default:
      { assert(0); }
  }

  cmSpecDist2Report(p->sdp);
  
  return rc;
}

cmDspClass_t* cmKr2ClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmKr2DC,ctx,"Kr2",
    NULL,
    _cmDspKr2Alloc,
    _cmDspKr2Free,
    _cmDspKr2Reset,
    _cmDspKr2Exec,
    _cmDspKr2Recv,
    NULL,NULL,
    "Fourier based non-linear transformer two.");

  return &_cmKr2DC;
}


//------------------------------------------------------------------------------------------------------------
//)
//( { label:cmDspTimeLine file_desc:"Time line user interface unit." kw:[sunit] }

enum
{
  kTlFileTlId,
  kPrefixPathTlId,
  kSelTlId,
  kMeasTlId,
  kCursTlId,
  kResetTlId,
  kAudFnTlId,
  kAudLblTlId,
  kMidiFnTlId,
  kMidiLblTlId,
  kBegAudSmpIdxTlId,
  kEndAudSmpIdxTlId,
  kBegMidiSmpIdxTlId,
  kEndMidiSmpIdxTlId,
};

cmDspClass_t _cmTimeLineDC;

typedef struct
{
  cmDspInst_t inst;
  cmTlH_t     tlH;
  unsigned    afIdx;
} cmDspTimeLine_t;

cmDspInst_t*  _cmDspTimeLineAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "tlfile",  kTlFileTlId,         0, 0, kInDsvFl   | kStrzDsvFl | kReqArgDsvFl, "Time line file." },
    { "path",    kPrefixPathTlId,     0, 0, kInDsvFl   | kStrzDsvFl | kReqArgDsvFl, "Time line data file prefix path"    },
    { "sel",     kSelTlId,            0, 0, kInDsvFl   | kOutDsvFl  | kUIntDsvFl,   "Selected marker id."},
    { "meas",    kMeasTlId,           0, 0, kInDsvFl   | kUIntDsvFl,  "Select a bar marker and generate a 'sel' output."},
    { "curs",    kCursTlId,           0, 0, kInDsvFl   | kUIntDsvFl,  "Current audio file index."},
    { "reset",   kResetTlId,          0, 0, kInDsvFl   | kSymDsvFl,   "Resend all outputs." },
    { "afn",     kAudFnTlId,          0, 0, kOutDsvFl  | kStrzDsvFl,  "Selected Audio file." },
    { "albl",    kAudLblTlId,         0, 0, kOutDsvFl  | kStrzDsvFl,  "Select Audio file time line label."},
    { "mfn",     kMidiFnTlId,         0, 0, kOutDsvFl  | kStrzDsvFl,  "Selected MIDI file." },
    { "mlbl",    kMidiLblTlId,        0, 0, kOutDsvFl  | kStrzDsvFl,  "Select MIDI file time line label."},
    { "absi",    kBegAudSmpIdxTlId,   0, 0, kOutDsvFl  | kIntDsvFl,   "Begin audio sample index."},
    { "aesi",    kEndAudSmpIdxTlId,   0, 0, kOutDsvFl  | kIntDsvFl,   "End audio sample index."},
    { "mbsi",    kBegMidiSmpIdxTlId,  0, 0, kOutDsvFl  | kIntDsvFl,   "Begin MIDI sample index."},
    { "mesi",    kEndMidiSmpIdxTlId,  0, 0, kOutDsvFl  | kIntDsvFl,   "End MIDI sample index."},
    { NULL, 0, 0, 0, 0 }
  };

  cmDspTimeLine_t* p = cmDspInstAlloc(cmDspTimeLine_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);
  
  cmDspSetDefaultUInt( ctx, &p->inst,  kSelTlId,           0, cmInvalidId);
  cmDspSetDefaultUInt( ctx, &p->inst,  kCursTlId,          0, 0);
  cmDspSetDefaultStrcz(ctx, &p->inst,  kAudFnTlId,         NULL, "");
  cmDspSetDefaultStrcz(ctx, &p->inst,  kAudLblTlId,        NULL, "");
  cmDspSetDefaultStrcz(ctx, &p->inst,  kMidiFnTlId,        NULL, "");
  cmDspSetDefaultStrcz(ctx, &p->inst,  kMidiLblTlId,       NULL, "");
  cmDspSetDefaultInt(  ctx, &p->inst,  kBegAudSmpIdxTlId,  0, cmInvalidIdx);
  cmDspSetDefaultInt(  ctx, &p->inst,  kEndAudSmpIdxTlId,  0, cmInvalidIdx);
  cmDspSetDefaultInt(  ctx, &p->inst,  kBegMidiSmpIdxTlId, 0, cmInvalidIdx);
  cmDspSetDefaultInt(  ctx, &p->inst,  kEndMidiSmpIdxTlId, 0, cmInvalidIdx);

  // create the UI control
  cmDspUiTimeLineCreate(ctx,&p->inst,kTlFileTlId,kPrefixPathTlId,kSelTlId,kMeasTlId,kCursTlId);

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

  const cmChar_t* tlPrePath = cmDspStrcz(inst,kPrefixPathTlId);

  if((tlFn =  cmDspStrcz(inst, kTlFileTlId )) !=  NULL )
    if( cmTimeLineInitializeFromFile(ctx->cmCtx, &p->tlH, NULL, NULL, tlFn, tlPrePath ) != kOkTlRC )
        rc = cmErrMsg(&inst->classPtr->err, kInstResetFailDspRC, "Time-line file open failed.");

  return rc;
}

cmDspRC_t _cmDspTimeLineRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspTimeLine_t* p  = (cmDspTimeLine_t*)inst;

  switch( evt->dstVarId )
  {
    case kPrefixPathTlId:
      cmDspSetEvent(ctx,inst,evt);
      break;

    case kCursTlId:
      cmDspSetEvent(ctx,inst,evt);
      break;

    case kResetTlId:
    case kSelTlId:
      {
        unsigned markerId;
        cmDspSetEvent(ctx,inst,evt);
        
        // get the id of the selected marker
        if((markerId = cmDspUInt(inst,kSelTlId)) != cmInvalidId )
        {
          // get the marker object
          cmTlObj_t* op;
          if((op = cmTimeLineIdToObj(p->tlH, cmInvalidId, markerId )) != NULL )
          {
            assert(op->typeId == kMarkerTlId || op->typeId == kMidiEvtTlId );

            p->afIdx = op->begSmpIdx;

            cmDspSetInt(ctx, inst, kBegAudSmpIdxTlId,  op->begSmpIdx );
            cmDspSetInt(ctx, inst, kEndAudSmpIdxTlId,  op->begSmpIdx + op->durSmpCnt );
            
            // locate the audio file assoc'd with the marker
            cmTlAudioFile_t* afp;
            if((afp = cmTimeLineAudioFileAtTime(p->tlH,op->seqId,op->seqSmpIdx)) != NULL)
            {
              cmDspSetStrcz(ctx, inst, kAudFnTlId, afp->fn );
              cmDspSetStrcz(ctx, inst, kAudLblTlId, afp->obj.name );              
            }

            // locate the midi file assoc'd with the marker
            cmTlMidiFile_t* mfp;
            if((mfp = cmTimeLineMidiFileAtTime(p->tlH,op->seqId,op->seqSmpIdx)) != NULL )
            {
              cmDspSetInt(ctx, inst, kBegMidiSmpIdxTlId, op->seqSmpIdx - mfp->obj.seqSmpIdx );
              cmDspSetInt(ctx, inst, kEndMidiSmpIdxTlId, op->seqSmpIdx + op->durSmpCnt - mfp->obj.seqSmpIdx );

              cmDspSetStrcz(ctx, inst, kMidiFnTlId, mfp->fn );
              cmDspSetStrcz(ctx, inst, kMidiLblTlId,mfp->obj.name );              

            }
          }
          
        }
        
      }
      
      break;
      
    case kMeasTlId:
      cmDspSetEvent(ctx,inst,evt);
      break;
      
    default:
      {assert(0);}
  }

  return kOkDspRC;
}

cmDspClass_t* cmTimeLineClassCons( cmDspCtx_t* ctx )
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

//------------------------------------------------------------------------------------------------------------
//)
//( { label:cmDspScore file_desc:"Musical score user interface unit." kw:[sunit] }

enum
{
  kFnScId,
  kSelScId,
  kSendScId,
  kStatusScId,
  kD0ScId,
  kD1ScId,
  kSmpIdxScId,
  kLocIdxScId,
  kCmdScId,
  kEvtIdxScId,
  kDynScId,
  kValTypeScId,
  kValueScId,
  kMeasScId,
};

cmDspClass_t _cmScoreDC;

typedef struct
{
  cmDspInst_t inst;
  cmScH_t     scH;
  cmDspCtx_t* ctx;   // temporary ctx ptr used during cmScore callback in _cmDspScoreRecv()
  unsigned printSymId;
} cmDspScore_t;

cmDspInst_t*  _cmDspScoreAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "fn",      kFnScId,     0, 0, kInDsvFl  | kStrzDsvFl | kReqArgDsvFl, "Score file." },
    { "sel",     kSelScId,    0, 0, kInDsvFl  | kOutDsvFl  | kUIntDsvFl,   "Selected score element index input."},
    { "send",    kSendScId,   0, 0, kInDsvFl  | kTypeDsvMask,              "Resend last selected score element."},
    { "status",  kStatusScId, 0, 0, kInDsvFl  | kIntDsvFl,                 "Performed MIDI status value output" },
    { "d0",      kD0ScId,     0, 0, kInDsvFl  | kUIntDsvFl,                "Performed MIDI msg data byte 0" },
    { "d1",      kD1ScId,     0, 0, kInDsvFl  | kUIntDsvFl,                "Performed MIDI msg data byte 1" },
    { "smpidx",  kSmpIdxScId, 0, 0, kInDsvFl  | kUIntDsvFl,                "Performed MIDi msg time tag as a sample index." }, 
    { "loc",     kLocIdxScId, 0, 0, kInDsvFl  | kUIntDsvFl,                "Performance score location."},
    { "cmd",     kCmdScId,    0, 0, kInDsvFl  | kSymDsvFl,                 "cmd: dump "},
    { "evtidx",  kEvtIdxScId, 0, 0, kOutDsvFl | kUIntDsvFl,                "Performed event index of following dynamcis level."},
    { "dyn",     kDynScId,    0, 0, kOutDsvFl | kUIntDsvFl,                "Dynamic level of previous event index."},
    { "type",    kValTypeScId,0, 0, kOutDsvFl | kUIntDsvFl,                "Output variable type."},
    { "value",   kValueScId,  0, 0, kOutDsvFl | kDoubleDsvFl,              "Output variable value."},
    { "meas",    kMeasScId,   0, 0, kInDsvFl  | kUIntDsvFl,                "Trigger this measures location to emit from 'sel'."},
    { NULL, 0, 0, 0, 0 }
  };

  cmDspScore_t* p = cmDspInstAlloc(cmDspScore_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);
  
  cmDspSetDefaultUInt( ctx, &p->inst,  kSelScId,           0, cmInvalidId);

  p->printSymId = cmSymTblRegisterStaticSymbol(ctx->stH,"dump");

  // create the UI control
  cmDspUiScoreCreate(ctx,&p->inst,kFnScId,kSelScId,kSmpIdxScId,kD0ScId,kD1ScId,kLocIdxScId,kEvtIdxScId,kDynScId,kValTypeScId,kValueScId,kMeasScId);

  p->scH = cmScNullHandle;

  return &p->inst;
}

cmDspRC_t _cmDspScoreFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t        rc = kOkDspRC;
  cmDspScore_t* p = (cmDspScore_t*)inst;

  if( cmScoreFinalize(&p->scH) != kOkTlRC )
    return cmErrMsg(&inst->classPtr->err, kInstFinalFailDspRC, "Score finalize failed.");

  return rc;
}

// Callback from cmScore triggered from _cmDspScoreRecv() during call to cmScoreSetPerfEvent().
void _cmDspScoreCb( void* arg, const void* data, unsigned byteCnt )
{
  cmDspInst_t*  inst = (cmDspInst_t*)arg;
  cmDspScore_t* p    = (cmDspScore_t*)inst;
  cmScMsg_t m;
  if( cmScoreDecode(data,byteCnt,&m) == kOkScRC )
  {
    switch( m.typeId )
    {
      case kDynMsgScId:
        cmDspSetUInt( p->ctx,inst, kEvtIdxScId, m.u.dyn.evtIdx );
        cmDspSetUInt( p->ctx,inst, kDynScId,    m.u.dyn.dynLvl );
        break;

      case kVarMsgScId:
        cmDspSetUInt(  p->ctx,inst, kValTypeScId, m.u.meas.varId);
        cmDspSetDouble(p->ctx,inst, kValueScId,   m.u.meas.value);
        break;

      default:
        { assert(0); }
    }
  }
}

cmDspRC_t _cmDspScoreReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t       rc          = kOkDspRC;
  cmDspScore_t*   p           = (cmDspScore_t*)inst;
  const cmChar_t* tlFn        = NULL;
  unsigned*       dynRefArray = NULL;
  unsigned        dynRefCnt   = 0;

  cmDspApplyAllDefaults(ctx,inst);


  if( cmDspRsrcUIntArray(ctx->dspH, &dynRefCnt, &dynRefArray, "dynRef", NULL ) != kOkDspRC )
  {
    rc = cmErrMsg(&inst->classPtr->err, kRsrcNotFoundDspRC, "The dynamics reference array resource was not found.");
    goto errLabel;
  }

  if((tlFn =  cmDspStrcz(inst, kFnScId )) !=  NULL )
  {
    if( cmScoreInitialize(ctx->cmCtx, &p->scH, tlFn, cmDspSampleRate(ctx), dynRefArray, dynRefCnt, _cmDspScoreCb, p, ctx->stH ) != kOkTlRC )
      rc = cmErrMsg(&inst->classPtr->err, kInstResetFailDspRC, "Score file open failed.");
    //else
    //  cmScorePrintLoc(p->scH);
  }
 errLabel:
  return rc;
}

cmDspRC_t _cmDspScoreRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspScore_t* p  = (cmDspScore_t*)inst;

  if( evt->dstVarId == kSendScId )
  {
    unsigned selIdx;
    if((selIdx = cmDspUInt(inst,kSelScId)) != cmInvalidIdx )
    {
      cmDspSetUInt(ctx,inst,kSelScId, selIdx );
      cmScoreClearPerfInfo(p->scH);
    }
    return kOkDspRC;
  }

  cmDspSetEvent(ctx,inst,evt);

  switch( evt->dstVarId )
  {
    case kSelScId:
      cmScoreClearPerfInfo(p->scH);
      break;

    case kStatusScId:
      //printf("st:%x\n",cmDspUInt(inst,kStatusScId));
      break;

    case kLocIdxScId:
      {
        assert( cmDspUInt(inst,kStatusScId ) == kNoteOnMdId );
        p->ctx = ctx; // setup p->ctx for use in _cmDspScoreCb()

        // this call may result in callbacks to _cmDspScoreCb()
        cmScoreExecPerfEvent(p->scH, cmDspUInt(inst,kLocIdxScId), cmDspUInt(inst,kSmpIdxScId), cmDspUInt(inst,kD0ScId), cmDspUInt(inst,kD1ScId) );      
      }
      break;

    case kCmdScId:
      if( cmDspSymbol(inst,kCmdScId) == p->printSymId )
        cmScorePrintLoc(p->scH);
      break;

    case kMeasScId:
      break;

  }

  return kOkDspRC;
}

cmDspClass_t* cmScoreClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmScoreDC,ctx,"Score",
    NULL,
    _cmDspScoreAlloc,
    _cmDspScoreFree,
    _cmDspScoreReset,
    NULL,
    _cmDspScoreRecv,
    NULL,NULL,
    "Score control.");

  return &_cmScoreDC;
}

//------------------------------------------------------------------------------------------------------------
//)
//( { label:cmDspMidiFilePlay file_desc:"MIDI file player." kw:[sunit] }

enum
{
  kFnMfId,
  kSelMfId,    
  kBsiMfId,
  kEsiMfId,
  kStatusMfId,
  kD0MfId,
  kD1MfId,
  kSmpIdxMfId,
  kIdMfId
};

cmDspClass_t _cmMidiFilePlayDC;

typedef struct
{
  cmDspInst_t   inst;
  cmMidiFileH_t mfH;  
  unsigned      curMsgIdx;     // current midi file msg index
  int           csi;           // current sample index
  int           bsi;           // starting sample index
  int           esi;           // ending sample index
  unsigned      startSymId;
  unsigned      stopSymId;
  unsigned      contSymId;
  bool          errFl;
} cmDspMidiFilePlay_t;

//  'bsi' and 'esi' give the starting and ending sample for MIDI file playback.
//  These indexes are relative to the start of the file.
//  When the player recieves a 'start' msg it sets the current sample index
//  'si' to 'bsi' and begins scanning for the next note to play.  
//  On each call to the _cmDspMidiFilePlayExec() msgs that fall in the interval
//  si:si+sPc-1 will be transmitted.  (where sPc are the number of samples per DSP cycle).

cmDspInst_t*  _cmDspMidiFilePlayAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "fn",     kFnMfId,     0, 0, kInDsvFl  | kStrzDsvFl, "File name"},
    { "sel",    kSelMfId,    0, 0, kInDsvFl  | kSymDsvFl,  "start | stop | continue" },
    { "bsi",    kBsiMfId,    0, 0, kInDsvFl  | kIntDsvFl,  "Starting sample." },
    { "esi",    kEsiMfId,    0, 0, kInDsvFl  | kIntDsvFl,  "Ending sample."},
    { "status", kStatusMfId, 0, 0, kOutDsvFl | kIntDsvFl,  "Status value output" },
    { "d0",     kD0MfId,     0, 0, kOutDsvFl | kUIntDsvFl, "Data byte 0" },
    { "d1",     kD1MfId,     0, 0, kOutDsvFl | kUIntDsvFl, "Data byte 1" },
    { "smpidx", kSmpIdxMfId, 0, 0, kOutDsvFl | kUIntDsvFl, "Msg time tag as a sample index." },
    { "id",     kIdMfId,     0, 0, kOutDsvFl | kUIntDsvFl, "MIDI file msg unique id."},
    { NULL, 0, 0, 0, 0 }
  };

  cmDspMidiFilePlay_t* p = cmDspInstAlloc(cmDspMidiFilePlay_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);

  p->startSymId = cmSymTblRegisterStaticSymbol(ctx->stH,"start");
  p->stopSymId  = cmSymTblRegisterStaticSymbol(ctx->stH,"stop");
  p->contSymId  = cmSymTblRegisterStaticSymbol(ctx->stH,"continue");

  p->mfH        = cmMidiFileNullHandle;

  cmDspSetDefaultStrcz( ctx, &p->inst, kFnMfId,   NULL, "");
  cmDspSetDefaultSymbol(ctx, &p->inst,  kSelMfId,    p->stopSymId);
  cmDspSetDefaultInt(   ctx, &p->inst,  kBsiMfId,    0, 0);
  cmDspSetDefaultInt(   ctx, &p->inst,  kEsiMfId,    0, 0);
  cmDspSetDefaultUInt(  ctx, &p->inst,  kStatusMfId, 0, 0);
  cmDspSetDefaultUInt(  ctx, &p->inst,  kD0MfId,     0, 0);
  cmDspSetDefaultUInt(  ctx, &p->inst,  kD1MfId,     0, 0);

  return &p->inst;
}

cmDspRC_t _cmDspMidiFilePlayFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspMidiFilePlay_t* p = (cmDspMidiFilePlay_t*)inst;
  if( cmMidiFileClose(&p->mfH) )
    return cmErrMsg(&inst->classPtr->err, kInstFinalFailDspRC, "MIDI file close failed.");
  return kOkDspRC;
}

// return the index of the msg following smpIdx
unsigned _cmDspMidiFilePlaySeekMsgIdx( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned smpIdx )
{
  cmDspMidiFilePlay_t* p = (cmDspMidiFilePlay_t*)inst;

  if( cmMidiFileIsValid(p->mfH) == false )
  {
    cmErrMsg(&inst->classPtr->err, kInvalidStateDspRC,"The MIDI file player has not been given a valid MIDI file.");
    return cmInvalidIdx;
  }

  unsigned                 i;
  unsigned                 n     = cmMidiFileMsgCount(p->mfH);
  const cmMidiTrackMsg_t** a     = cmMidiFileMsgArray(p->mfH);

  for(i=0; i<n; ++i)
    if( (a[i]->amicro * cmDspSampleRate(ctx) / 1000000.0) >= smpIdx )
      break;

  return i==n ? cmInvalidIdx : i;
}

cmDspRC_t _cmDspMidiFilePlayOpen(cmDspCtx_t* ctx, cmDspInst_t* inst )
{
  cmDspRC_t            rc = kOkDspRC;
  const cmChar_t*      fn = cmDspStrcz(inst,kFnMfId);
  cmDspMidiFilePlay_t* p  = (cmDspMidiFilePlay_t*)inst;

  p->errFl = false;

  if( fn==NULL || strlen(fn)==0 )
    return rc;

  if( cmMidiFileOpen( ctx->cmCtx, &p->mfH, fn ) != kOkFileRC )
    rc = cmErrMsg(&inst->classPtr->err, kInstResetFailDspRC, "MIDI file open failed.");
  else
  {
    p->curMsgIdx = 0;
    p->bsi       = cmDspInt(inst,kBsiMfId);
    p->esi       = cmDspInt(inst,kEsiMfId);
    p->csi       = 0;
    
    // force the first msg to occurr one quarter note into the file
    cmMidiFileSetDelay(p->mfH, cmMidiFileTicksPerQN(p->mfH) );

    // convert midi msg times to absolute time in samples
    //cmMidiFileTickToSamples(p->mfH,cmDspSampleRate(ctx),true);

  }
  return rc;
}

cmDspRC_t _cmDspMidiFilePlayReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspApplyAllDefaults(ctx,inst);

  return _cmDspMidiFilePlayOpen(ctx,inst);
}

cmDspRC_t _cmDspMidiFilePlayExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t            rc  = kOkDspRC;
  cmDspMidiFilePlay_t* p   = (cmDspMidiFilePlay_t*)inst;
  unsigned             sPc = cmDspSamplesPerCycle(ctx);


  if( cmDspSymbol(inst,kSelMfId) != p->stopSymId )
  {
    if( cmMidiFileIsValid(p->mfH) == false )
    {
      if( p->errFl==false )
      {
        rc = cmErrMsg(&inst->classPtr->err, kInvalidStateDspRC,"The MIDI file player has not been given a valid MIDI file.");
        p->errFl = true;
      }
      return rc;
    }

    const cmMidiTrackMsg_t** mpp   = cmMidiFileMsgArray(p->mfH);
    unsigned                 msgN  = cmMidiFileMsgCount(p->mfH);
    
    do
    {
      if( p->curMsgIdx >= msgN )
        break;

      const cmMidiTrackMsg_t* mp = mpp[p->curMsgIdx];

      // convert the absolute time in microseconds to samples
      unsigned  curMsgTimeSmp = round(mp->amicro * cmDspSampleRate(ctx) / 1000000.0);

      // if this midi event falls inside this execution window
      if( p->csi > curMsgTimeSmp  ||  curMsgTimeSmp >= (p->csi + sPc))
        break;
        
      switch( mp->status )
      {
        case kNoteOffMdId:
        case kNoteOnMdId:
        case kCtlMdId:
          cmDspSetUInt(ctx,inst, kSmpIdxMfId, curMsgTimeSmp);
          cmDspSetUInt(ctx,inst, kD1MfId,     mp->u.chMsgPtr->d1);
          cmDspSetUInt(ctx,inst, kD0MfId,     mp->u.chMsgPtr->d0);
          cmDspSetUInt(ctx,inst, kStatusMfId, mp->status);
          cmDspSetUInt(ctx,inst, kIdMfId,     mp->uid);
          break;
      }

      p->curMsgIdx += 1;
      
    }while(1);
  }

  p->csi += sPc;
  
  return rc;
}

cmDspRC_t _cmDspMidiFilePlayRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  cmDspMidiFilePlay_t* p = (cmDspMidiFilePlay_t*)inst;
  
  cmDspSetEvent(ctx,inst,evt);

  switch(evt->dstVarId)
  {
    case kFnMfId:
      _cmDspMidiFilePlayOpen(ctx, inst );
      break;

    case kSelMfId:
      {
        if( cmDspSymbol(inst,kSelMfId)==p->startSymId ) 
        {
          p->csi       = cmDspInt(inst,kBsiMfId);
          p->curMsgIdx = _cmDspMidiFilePlaySeekMsgIdx(ctx, inst, p->csi );
        }
        break;
      }

  }
  return kOkDspRC;
}

cmDspClass_t* cmMidiFilePlayClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmMidiFilePlayDC,ctx,"MidiFilePlay",
    NULL,
    _cmDspMidiFilePlayAlloc,
    _cmDspMidiFilePlayFree,
    _cmDspMidiFilePlayReset,
    _cmDspMidiFilePlayExec,
    _cmDspMidiFilePlayRecv,
    NULL,NULL,
    "MIDI file player.");

  return &_cmMidiFilePlayDC;
}

//------------------------------------------------------------------------------------------------------------
//)
//( { label:cmDspScFol file_desc:"MIDI performance score follower." kw:[sunit] }

enum
{
  kFnSfId,
  kBufCntSfId,
  kMaxWndCntSfId,
  kMinVelSfId,
  kMeasflSfId,
  kIndexSfId,
  kMuidSfId,
  kStatusSfId,
  kD0SfId,
  kD1SfId,
  kSmpIdxSfId,
  kCmdSfId,
  kOutSfId,
  kRecentSfId,
  kVlocSfId,
  kVtypSfId,
  kVvalSfId,
  kVcostSfId,
  kSymSfId
};

cmDspClass_t _cmScFolDC;
struct cmDspScFol_str;

typedef struct
{
  cmDspCtx_t*            ctx;
  struct cmDspScFol_str* sfp;
} cmDspScFolCbArg_t;

typedef struct cmDspScFol_str
{
  cmDspInst_t       inst;
  cmScMatcher*      sfp;
  cmScMeas*         smp;
  cmScH_t           scH;
  cmDspScFolCbArg_t arg;
  unsigned          printSymId;
  unsigned          quietSymId;
  unsigned          maxScLocIdx;
  bool              liveFl;
} cmDspScFol_t;

cmDspInst_t*  _cmDspScFolAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "fn",    kFnSfId,       0, 0, kInDsvFl | kStrzDsvFl | kReqArgDsvFl,     "Score file." },
    { "bufcnt",kBufCntSfId,   0, 0, kInDsvFl | kUIntDsvFl | kOptArgDsvFl,     "Event buffer element count." },
    { "wndcnt",kMaxWndCntSfId,0, 0, kInDsvFl | kUIntDsvFl | kOptArgDsvFl,     "Maximum window length."},
    { "minvel",kMinVelSfId,   0, 0, kInDsvFl | kUIntDsvFl | kOptArgDsvFl,     "Minimum velocity."},
    { "measfl",kMeasflSfId,   0, 0, kInDsvFl | kBoolDsvFl | kOptArgDsvFl,     "Enable measurements"},
    { "index", kIndexSfId,    0, 0, kInDsvFl | kUIntDsvFl,                    "Tracking start location."},
    { "muid",  kMuidSfId,     0, 0, kInDsvFl | kUIntDsvFl,                    "MIDI msg file unique id"},
    { "status",kStatusSfId,   0, 0, kInDsvFl | kUIntDsvFl,                    "MIDI status byte"},
    { "d0",    kD0SfId,       0, 0, kInDsvFl | kUIntDsvFl,                    "MIDI data byte 0"},
    { "d1",    kD1SfId,       0, 0, kInDsvFl | kUIntDsvFl,                    "MIDI data byte 1"},
    { "smpidx",kSmpIdxSfId,   0, 0, kInDsvFl | kUIntDsvFl,                    "MIDI time tag as a sample index"},
    { "cmd",   kCmdSfId,      0, 0, kInDsvFl | kSymDsvFl,                     "Command input: print | quiet"},
    { "out",   kOutSfId,      0, 0, kOutDsvFl| kUIntDsvFl,                    "Maximum score location index."},
    { "recent",kRecentSfId,   0, 0, kOutDsvFl| kUIntDsvFl,                    "Most recent score location index."},
    { "vloc",  kVlocSfId,     0, 0, kOutDsvFl| kUIntDsvFl,                    "Score location at which the variable value becomes active."},
    { "vtyp",  kVtypSfId,     0, 0, kOutDsvFl| kUIntDsvFl,                    "Variable type: 0=even=kEvenVarScId 1=dyn=kDynVarScId 2=tempo=kTempoVarScId."},
    { "vval",  kVvalSfId,     0, 0, kOutDsvFl| kDoubleDsvFl,                  "Variable value."},
    { "vcost", kVcostSfId,    0, 0, kOutDsvFl| kDoubleDsvFl,                  "Variable match cost value."},
    { "sym",   kSymSfId,      0, 0, kOutDsvFl| kSymDsvFl,                     "Symbol associated with a global variable which has changed value."},
    { NULL,    0,             0, 0, 0, NULL }
  };

  cmDspScFol_t* p;

  if((p = cmDspInstAlloc(cmDspScFol_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl)) == NULL )
    return NULL;
  
  p->sfp        = cmScMatcherAlloc(ctx->cmProcCtx, NULL, 0, cmScNullHandle, 0, 0, NULL, NULL );
  p->smp        = cmScMeasAlloc(   ctx->cmProcCtx, NULL, cmScNullHandle, 0, NULL, 0 );
  p->printSymId = cmSymTblRegisterStaticSymbol(ctx->stH,"print");
  p->quietSymId = cmSymTblRegisterStaticSymbol(ctx->stH,"quiet");
  p->maxScLocIdx= cmInvalidIdx;

  cmDspSetDefaultUInt(   ctx, &p->inst,  kBufCntSfId,     0,     7);
  cmDspSetDefaultUInt(   ctx, &p->inst,  kMaxWndCntSfId,  0,    10);
  cmDspSetDefaultUInt(   ctx, &p->inst,  kMinVelSfId,     0,     5);
  cmDspSetDefaultBool(   ctx, &p->inst,  kMeasflSfId,     0,     0);
  cmDspSetDefaultUInt(   ctx, &p->inst,  kIndexSfId,      0,     0);  
  cmDspSetDefaultUInt(   ctx, &p->inst,  kOutSfId,        0,     0);
  cmDspSetDefaultUInt(   ctx, &p->inst,  kRecentSfId,     0,     0);
  
  cmDspSetDefaultSymbol(ctx,&p->inst,  kCmdSfId, p->quietSymId );

  return &p->inst;
}

cmDspRC_t _cmDspScFolFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspScFol_t* p = (cmDspScFol_t*)inst;
  cmScMatcherFree(&p->sfp);
  cmScMeasFree(&p->smp);
  cmScoreFinalize(&p->scH);
  return kOkDspRC;
}

// This is a callback function from cmScMatcherExec() which is called when
// this cmDspFol object receives a new score location index.
void _cmScFolMatcherCb( cmScMatcher* p, void* arg, cmScMatcherResult_t* rp )
{
  cmDspScFolCbArg_t* ap = (cmDspScFolCbArg_t*)arg;

  
  if( cmScMeasExec(ap->sfp->smp, rp->mni, rp->locIdx, rp->scEvtIdx, rp->flags, rp->smpIdx, rp->pitch, rp->vel ) == cmOkRC )
  {
    cmDspInst_t*  inst = &(ap->sfp->inst);

    // send 'set' values that were calculated on the previous call to cmScMeasExec()
    unsigned i;
    for(i=ap->sfp->smp->vsi; i<ap->sfp->smp->nsi; ++i)
      if(ap->sfp->smp->set[i].value != DBL_MAX )
      {

//        switch( ap->sfp->smp->set[i].sp->varId )
//        {
//          case kEvenVarScId:
//            cmDspSetDouble(ap->ctx,inst,kEvenSfId,ap->sfp->smp->set[i].value);
//            break;
//
//          case kDynVarScId:
//            cmDspSetDouble(ap->ctx,inst,kDynSfId,ap->sfp->smp->set[i].value);
//            break;
//
//          case kTempoVarScId:
//            cmDspSetDouble(ap->ctx,inst,kTempoSfId,ap->sfp->smp->set[i].value);
//            break;
//
//          default:
//            { assert(0); }
//        }           
//
//        cmDspSetDouble(ap->ctx,inst,kCostSfId,ap->sfp->smp->set[i].match_cost);

        // Set the values in the global variable storage
        cmDspValue_t vv,cv;
        unsigned     j;
        cmDsvSetDouble(&vv,ap->sfp->smp->set[i].value);
        cmDsvSetDouble(&cv,ap->sfp->smp->set[i].match_cost);

        for(j=0; j<ap->sfp->smp->set[i].sp->sectCnt; ++j)
        {
          cmDspStoreSetValueViaSym(ap->ctx->dsH, ap->sfp->smp->set[i].sp->symArray[j], &vv );
          cmDspStoreSetValueViaSym(ap->ctx->dsH, ap->sfp->smp->set[i].sp->costSymArray[j], &cv );

          cmDspSetSymbol(ap->ctx,inst,kSymSfId,ap->sfp->smp->set[i].sp->symArray[j]);
          cmDspSetSymbol(ap->ctx,inst,kSymSfId,ap->sfp->smp->set[i].sp->costSymArray[j]);

          if( cmDspBool(inst,kMeasflSfId) )
          {
            cmDspSetUInt(   ap->ctx, inst, kVlocSfId,  ap->sfp->smp->set[i].sp->sectArray[j]->locPtr->index);
            cmDspSetDouble( ap->ctx, inst, kVvalSfId,  ap->sfp->smp->set[i].value);
            cmDspSetDouble( ap->ctx, inst, kVcostSfId, ap->sfp->smp->set[i].match_cost);
            cmDspSetUInt(   ap->ctx, inst, kVtypSfId,  ap->sfp->smp->set[i].sp->varId);
          }
        }


      }

//    // trigger 'section' starts 
//    for(i=ap->sfp->smp->vsli; i<ap->sfp->smp->nsli; ++i)
//    {
//      const cmScoreLoc_t* locPtr = cmScoreLoc(ap->sfp->smp->mp->scH,i);
//      if( locPtr->begSectPtr != NULL )
//        cmDspSetUInt(ap->ctx,inst,kSectIndexSfId,locPtr->begSectPtr->index);
//    }
  }
}


cmDspRC_t _cmDspScFolOpenScore( cmDspCtx_t* ctx, cmDspInst_t* inst )
{
  cmDspRC_t       rc = kOkDspRC;
  cmDspScFol_t*   p  = (cmDspScFol_t*)inst;
  const cmChar_t* fn;

  if((fn = cmDspStrcz(inst,kFnSfId)) == NULL || strlen(fn)==0 )
    return cmErrMsg(&inst->classPtr->err, kInvalidArgDspRC, "No score file name supplied.");

  if( cmScoreInitialize(ctx->cmCtx, &p->scH, fn, cmDspSampleRate(ctx), NULL, 0, NULL, NULL, ctx->stH ) != kOkScRC )
    return cmErrMsg(&inst->classPtr->err, kSubSysFailDspRC, "Unable to open the score '%s'.",fn);

  if( cmScoreIsValid(p->scH) )
  {
    unsigned*       dynRefArray = NULL;
    unsigned        dynRefCnt   = 0;

    // initialize the cmScMatcher
    if( cmScMatcherInit(p->sfp, cmDspSampleRate(ctx),  p->scH, cmDspUInt(inst,kMaxWndCntSfId), cmDspUInt(inst,kBufCntSfId), _cmScFolMatcherCb, p->smp ) != cmOkRC )
      rc = cmErrMsg(&inst->classPtr->err, kSubSysFailDspRC, "Internal score follower allocation failed.");

    // read the dynamics reference array
    if( cmDspRsrcUIntArray(ctx->dspH, &dynRefCnt, &dynRefArray, "dynRef", NULL ) != kOkDspRC )
    {
      rc = cmErrMsg(&inst->classPtr->err, kRsrcNotFoundDspRC, "The dynamics reference array resource was not found.");
      goto errLabel;
    }

    // initialize the cmScMeas object.
    if( cmScMeasInit(p->smp, p->scH, cmDspSampleRate(ctx), dynRefArray, dynRefCnt ) != cmOkRC )
      rc = cmErrMsg(&inst->classPtr->err, kSubSysFailDspRC, "Internal scMeas object initialization failed.");

  }

 errLabel:
  return rc;
}

cmDspRC_t _cmDspScFolReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t     rc;
  if((rc = cmDspApplyAllDefaults(ctx,inst)) != kOkDspRC )
    return rc;

  return _cmDspScFolOpenScore(ctx,inst);
}


cmDspRC_t _cmDspScFolRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  cmDspRC_t     rc = kOkDspRC;
  cmDspScFol_t* p  = (cmDspScFol_t*)inst;

  if((rc = cmDspSetEvent(ctx,inst,evt)) == kOkDspRC && p->sfp != NULL )
  {
    switch( evt->dstVarId )
    {
      case kIndexSfId:
        if( cmScoreIsValid(p->scH) )
        {
          p->maxScLocIdx = cmInvalidIdx;

          if( cmScMeasReset( p->smp ) != cmOkRC )
            cmErrMsg(&inst->classPtr->err, kSubSysFailDspRC, "Score measure unit reset to score index '%i' failed.");

          if( cmScMatcherReset( p->sfp, cmDspUInt(inst,kIndexSfId) ) != cmOkRC )
            cmErrMsg(&inst->classPtr->err, kSubSysFailDspRC, "Score follower reset to score index '%i' failed.");
        }
        break;

      case kStatusSfId:
        if( cmScoreIsValid(p->scH))
        {
          unsigned scLocIdx = cmInvalidIdx;

          // setup the cmScMeas() callback arg.
          p->arg.ctx    = ctx;
          p->arg.sfp    = p;
          p->sfp->cbArg = &p->arg;

          // this call may result in a callback to _cmScFolMatcherCb()
          if( cmScMatcherExec(p->sfp, cmDspUInt(inst,kSmpIdxSfId), cmDspUInt(inst,kMuidSfId), cmDspUInt(inst,kStatusSfId), cmDspUInt(inst,kD0SfId), cmDspUInt(inst,kD1SfId), &scLocIdx) == cmOkRC )
            if( scLocIdx != cmInvalidIdx )
            {
              // It is possible that the internal score follower may go backwards.  
              // In this case it will report a given score location multiple times or out of time order.
              // The 'out' port will only be updated under the circumstances that no later
              // score location has been seen - so the last output from 'out' always reports
              // the furthest possible progress in the score.  THe 'recent' output simply reports
              // the most recent output from the internal score follower which may include 
              // previously reported or out of order score locations.
              cmDspSetUInt(ctx,inst,kRecentSfId,scLocIdx);

              if( p->maxScLocIdx==cmInvalidIdx || p->maxScLocIdx < scLocIdx )
              {
                p->maxScLocIdx = scLocIdx;
                cmDspSetUInt(ctx,inst,kOutSfId,scLocIdx);
              }
            }
        }
        break;

      case kFnSfId:
        _cmDspScFolOpenScore(ctx,inst);
        break;

      case kCmdSfId:
        if( cmDspSymbol(inst,kCmdSfId) == p->printSymId )
          p->sfp->printFl = true;
        else
          if( cmDspSymbol(inst,kCmdSfId) == p->quietSymId )
            p->sfp->printFl = false;

        break;
    }
  }

  return rc;
}

cmDspClass_t* cmScFolClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmScFolDC,ctx,"ScFol",
    NULL,
    _cmDspScFolAlloc,
    _cmDspScFolFree,
    _cmDspScFolReset,
    NULL,
    _cmDspScFolRecv,
    NULL,NULL,
    "Score Follower");

  return &_cmScFolDC;
}

//------------------------------------------------------------------------------------------------------------
//)
//( { label:cmDspScMod file_desc:"Score driven parameter automation." kw:[sunit] }

enum
{
  kScLocIdxMdId,
  kResetIdxMdId,
  kCmdMdId,
  kSelMdId,
  kPostMdId
};

cmDspClass_t _cmModulatorDC;

typedef struct
{
  cmDspInst_t    inst;
  cmScModulator* mp;
  cmDspCtx_t*    tmp_ctx;       // used to temporarily hold the current cmDspCtx during callback
  cmChar_t*      fn;
  cmChar_t*      modLabel;
  unsigned       onSymId;
  unsigned       offSymId;
  unsigned       postSymId;
  unsigned       dumpSymId;
} cmDspScMod_t;

void _cmDspScModCb( void* arg, unsigned varSymId, double value, bool postFl )
{
  cmDspScMod_t* p = (cmDspScMod_t*)arg;

  cmDspVar_t* varPtr;
  if((varPtr = cmDspVarSymbolToPtr( p->tmp_ctx, &p->inst, varSymId, 0 )) == NULL )
    return;

  cmDspSetDouble(p->tmp_ctx,&p->inst,varPtr->constId,value);

  if( postFl )
    cmDspSetSymbol(p->tmp_ctx,&p->inst,kPostMdId,p->postSymId);
  
}

cmDspInst_t*  _cmDspScModAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  va_list vl1;
  va_copy(vl1,vl);

  cmDspVarArg_t args[] =
  {
    { "index",   kScLocIdxMdId, 0,0, kInDsvFl  | kUIntDsvFl,  "Score follower index input."},
    { "reset",   kResetIdxMdId, 0,0, kInDsvFl  | kUIntDsvFl | kOptArgDsvFl, "Reset the modulator and go to the score index."},
    { "cmd",     kCmdMdId,      0,0, kInDsvFl  | kSymDsvFl  | kOptArgDsvFl, "on | off."},
    { "sel",     kSelMdId,      0,0, kInDsvFl  | kSymDsvFl  | kOptArgDsvFl, "Set the next active entry group name."},
    { "post",    kPostMdId,     0,0, kOutDsvFl | kSymDsvFl, "Sends 'post' symbol after a message transmission if the 'post' flag is set in scMod."},
    { NULL, 0, 0, 0, 0 }
  };

  // validate the argument count
  if( va_cnt != 2 )
  {
    cmDspClassErr(ctx,classPtr,kInvalidArgDspRC,"The Modulator requires at least two arguments.");
    return NULL;
  }    

  // read the modulator file and label strings
  const cmChar_t* fn       = va_arg(vl1,const cmChar_t*);
  const cmChar_t* modLabel = va_arg(vl1,const cmChar_t*);

  va_end(vl1);

  // validate the file
  if( fn==NULL || cmFsIsFile(fn)==false )
  {
    cmDspClassErr(ctx,classPtr,kInvalidArgDspRC,"The Modulator file '%s' is not valid.",cmStringNullGuard(fn));
    return NULL;
  }

  // allocate the internal modulator object
  cmScModulator* mp = cmScModulatorAlloc(ctx->cmProcCtx, NULL, ctx->cmCtx, ctx->stH, cmDspSampleRate(ctx), cmDspSamplesPerCycle(ctx), fn, modLabel, _cmDspScModCb, NULL );
  
  if(mp == NULL )
  {
    cmDspClassErr(ctx,classPtr,kInvalidArgDspRC,"The internal modulator object initialization failed.");
    return NULL;
  }
  unsigned      fixArgCnt = sizeof(args)/sizeof(args[0]) - 1;
  unsigned      argCnt    = fixArgCnt + cmScModulatorOutVarCount(mp);
  cmDspVarArg_t a[ argCnt+1 ];
  unsigned      i;

  cmDspArgCopy( a, argCnt, 0, args, fixArgCnt );

  for(i=fixArgCnt; i<argCnt; ++i)
  {
    unsigned            varIdx    = i - fixArgCnt;
    const cmScModVar_t* vp        = cmScModulatorOutVar(mp,varIdx);
    const cmChar_t*     label     = cmSymTblLabel( ctx->stH, vp->varSymId );
    const cmChar_t*     docStr    = cmTsPrintfS("Variable output for %s",label);

    cmDspArgSetup(ctx, a + i, label, cmInvalidId, i, 0, 0, kOutDsvFl | kDoubleDsvFl, docStr ); 
  }
  cmDspArgSetupNull(a+argCnt); // set terminating arg. flags

  cmDspScMod_t* p = cmDspInstAlloc(cmDspScMod_t,ctx,classPtr,a,instSymId,id,storeSymId,va_cnt,vl);


  p->fn       = cmMemAllocStr(fn);
  p->modLabel = cmMemAllocStr(modLabel);
  p->mp       = mp;
  p->onSymId  = cmSymTblId(ctx->stH,"on");
  p->offSymId = cmSymTblId(ctx->stH,"off");
  p->postSymId = cmSymTblRegisterStaticSymbol(ctx->stH,"post");
  p->dumpSymId = cmSymTblId(ctx->stH,"dump");

  mp->cbArg = p;  // set the modulator callback arg

  

  cmDspSetDefaultUInt(ctx,&p->inst,kScLocIdxMdId,0,0);
  cmDspSetDefaultSymbol(ctx,&p->inst,kCmdMdId,p->offSymId);
  cmDspSetDefaultSymbol(ctx,&p->inst,kSelMdId,cmInvalidId);
  return &p->inst;
}

cmDspRC_t _cmDspScModFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t        rc = kOkDspRC;
  cmDspScMod_t* p = (cmDspScMod_t*)inst;


  if( cmScModulatorFree(&p->mp) != kOkTlRC )
    return cmErrMsg(&inst->classPtr->err, kInstFinalFailDspRC, "Modulator release failed.");

  cmMemFree(p->fn);
  cmMemFree(p->modLabel);
  return rc;
}


cmDspRC_t _cmDspScModReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t       rc          = kOkDspRC;

  cmDspApplyAllDefaults(ctx,inst);

  return rc;
}

cmDspRC_t _cmDspScModRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspScMod_t* p  = (cmDspScMod_t*)inst;

  cmDspSetEvent(ctx,inst,evt);
  
  switch( evt->dstVarId )
  {
    case kResetIdxMdId:
      cmDspSetUInt(ctx,inst,kScLocIdxMdId,cmDspUInt(inst,kResetIdxMdId));
      break;

    case kCmdMdId:
      {
        unsigned symId = cmDspSymbol(inst,kCmdMdId);
        if( symId == p->onSymId )
          cmScModulatorReset(p->mp, ctx->cmCtx, cmDspUInt(inst,kScLocIdxMdId), cmDspSymbol(inst,kSelMdId));
        
        if( symId == p->dumpSymId )
          cmScModulatorDump(p->mp);
      }
      break;

  }

  return kOkDspRC;
}

cmDspRC_t _cmDspScModExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t         rc = kOkDspRC;
  cmDspScMod_t* p  = (cmDspScMod_t*)inst;
  
  if( cmDspSymbol(inst,kCmdMdId) != p->offSymId )
  {
    p->tmp_ctx = ctx;
    cmScModulatorExec(p->mp,cmDspUInt(inst,kScLocIdxMdId));
  }

  return rc;
}

cmDspClass_t* cmScModClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmModulatorDC,ctx,"ScMod",
    NULL,
    _cmDspScModAlloc,
    _cmDspScModFree,
    _cmDspScModReset,
    _cmDspScModExec,
    _cmDspScModRecv,
    NULL,NULL,
    "Score Driven Variable Modulator.");

  return &_cmModulatorDC;
}

//------------------------------------------------------------------------------------------------------------
//)
//( { label:cmDspGSwitch file_desc:"Route all inputs to one of a group of outputs." kw:[sunit] }

enum
{
  kInChCntGsId,
  kOutGroupCntGsId,
  kGroupSelIdxGsId,
  kBaseInFloatGsId
};

cmDspClass_t _cmGSwitchDC;

typedef struct
{
  cmDspInst_t    inst;

  unsigned iChCnt;
  unsigned oGroupCnt;

  unsigned baseInFloatGsId;
  unsigned baseInSymGsId;
  unsigned baseInBoolGsId;

  unsigned baseOutFloatGsId;
  unsigned baseOutSymGsId;
  unsigned baseOutBoolGsId;

} cmDspGSwitch_t;


cmDspInst_t*  _cmDspGSwitchAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  va_list vl1;
  va_copy(vl1,vl);

  cmDspVarArg_t args[] =
  {
    { "ichs",   kInChCntGsId,     0,0,            kUIntDsvFl | kReqArgDsvFl, "Input channel count."},
    { "ochs",   kOutGroupCntGsId, 0,0,            kUIntDsvFl | kReqArgDsvFl, "Output group count."},
    { "sel",    kGroupSelIdxGsId, 0,0, kInDsvFl | kUIntDsvFl,                "Group select index."},
    { NULL, 0, 0, 0, 0 }
  };

  // validate the argument count
  if( va_cnt != 2 )
  {
    cmDspClassErr(ctx,classPtr,kInvalidArgDspRC,"The GSwitch requires at least two arguments.");
    return NULL;
  }    

  // read the input ch and output group count
  unsigned iChCnt     = va_arg(vl1,unsigned);
  unsigned oGroupCnt  = va_arg(vl1,unsigned);

  va_end(vl1);
  
  // validate the channel counts
  if( iChCnt == 0 || oGroupCnt==0 )
  {
    cmDspClassErr(ctx,classPtr,kInvalidArgDspRC,"The GSwitch input channel count and group count must be greater than zero.");
    return NULL;
  }

  unsigned typeCnt          = 3; // i.e. float,sym,bool
  unsigned baseInFloatGsId  = kBaseInFloatGsId;
  unsigned baseInSymGsId    = baseInFloatGsId  + iChCnt;
  unsigned baseInBoolGsId   = baseInSymGsId    + iChCnt;
  unsigned baseOutFloatGsId = baseInBoolGsId   + iChCnt;
  unsigned baseOutSymGsId   = baseOutFloatGsId + (iChCnt * oGroupCnt);
  unsigned baseOutBoolGsId  = baseOutSymGsId   + (iChCnt * oGroupCnt);

  unsigned      fixArgCnt        = 3;
  unsigned      varArgCnt        = (iChCnt * typeCnt) + (iChCnt * typeCnt * oGroupCnt);
  unsigned      argCnt           = fixArgCnt + varArgCnt;
  cmDspVarArg_t a[ argCnt+1 ];
  unsigned      i;

  cmDspArgCopy( a, argCnt, 0, args, fixArgCnt );
  cmDspArgSetupN( ctx, a, argCnt, baseInFloatGsId, iChCnt, "f-in", baseInFloatGsId, 0, 0, kInDsvFl | kDoubleDsvFl, "Float input");
  cmDspArgSetupN( ctx, a, argCnt, baseInSymGsId,   iChCnt, "s-in", baseInSymGsId,   0, 0, kInDsvFl | kSymDsvFl,    "Symbol input");
  cmDspArgSetupN( ctx, a, argCnt, baseInBoolGsId,  iChCnt, "b-in", baseInBoolGsId,  0, 0, kInDsvFl | kBoolDsvFl,   "Bool input");

  unsigned labelCharCnt = 63;
  cmChar_t label[labelCharCnt+1];
  label[labelCharCnt] = 0;

  unsigned gsid = baseOutFloatGsId;
  for(i=0; i<oGroupCnt; ++i, gsid+=iChCnt)
  {
    snprintf(label,labelCharCnt,"f-out-%i",i);
    cmDspArgSetupN( ctx, a, argCnt, gsid, iChCnt, label, gsid, 0, 0, kInDsvFl | kDoubleDsvFl, "Float output");
  }

  gsid = baseOutSymGsId;
  for(i=0; i<oGroupCnt; ++i, gsid+=iChCnt)
  {
    snprintf(label,labelCharCnt,"s-out-%i",i);
    cmDspArgSetupN( ctx, a, argCnt, gsid, iChCnt, label, gsid, 0, 0, kInDsvFl | kSymDsvFl, "Symbol output");
  }

  gsid = baseOutBoolGsId;
  for(i=0; i<oGroupCnt; ++i, gsid+=iChCnt)
  {
    snprintf(label,labelCharCnt,"b-out-%i",i);
    cmDspArgSetupN( ctx,a, argCnt, gsid, iChCnt, label, gsid, 0, 0, kInDsvFl | kBoolDsvFl, "Bool output");
  }

  cmDspArgSetupNull(a+argCnt); // set terminating arg. flags  

  cmDspGSwitch_t* p = cmDspInstAlloc(cmDspGSwitch_t,ctx,classPtr,a,instSymId,id,storeSymId,va_cnt,vl);

  p->iChCnt           = iChCnt;
  p->oGroupCnt        = oGroupCnt;
  p->baseInFloatGsId  = baseInFloatGsId;
  p->baseInSymGsId    = baseInSymGsId;
  p->baseInBoolGsId   = baseInBoolGsId;
  p->baseOutFloatGsId = baseOutFloatGsId;
  p->baseOutSymGsId   = baseOutSymGsId;
  p->baseOutBoolGsId  = baseOutBoolGsId;

  cmDspSetDefaultUInt(ctx,&p->inst,kGroupSelIdxGsId,0,0);

  return &p->inst;
}

cmDspRC_t _cmDspGSwitchReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t       rc          = kOkDspRC;

  cmDspApplyAllDefaults(ctx,inst);

  return rc;
}

cmDspRC_t _cmDspGSwitchRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t       rc = kOkDspRC;
  cmDspGSwitch_t* p  = (cmDspGSwitch_t*)inst;

  // if this is the group selector
  if( evt->dstVarId == kGroupSelIdxGsId )
  {
    unsigned idx;
    if( (idx = cmDsvGetUInt(evt->valuePtr)) > p->oGroupCnt )
      cmDspInstErr(ctx,inst,kInvalidArgDspRC,"The GSwitch group select index %i is out of range %i.",idx,p->oGroupCnt);
    else
      cmDspSetEvent(ctx,inst,evt);
    return rc;
  }

  // get the group selector
  unsigned groupIdx = cmDspUInt(inst,kGroupSelIdxGsId);
  assert( groupIdx < p->oGroupCnt);


  // if this is a float input
  if( p->baseInFloatGsId <= evt->dstVarId && evt->dstVarId < p->baseInFloatGsId + p->iChCnt )
  {
    unsigned outVarId = p->baseOutFloatGsId + (groupIdx * p->iChCnt) + (evt->dstVarId - p->baseInFloatGsId);
    cmDspValueSet(ctx, inst, outVarId, evt->valuePtr, 0 );
    return rc;
  }

  // if this is a symbol input
  if( p->baseInSymGsId <= evt->dstVarId && evt->dstVarId < p->baseInSymGsId + p->iChCnt )
  {
    unsigned outVarId = p->baseOutSymGsId + (groupIdx * p->iChCnt) + (evt->dstVarId - p->baseInSymGsId);
    cmDspValueSet(ctx, inst, outVarId, evt->valuePtr, 0 );    
    return rc;
  }

  // if this is a bool input
  if( p->baseInBoolGsId <= evt->dstVarId && evt->dstVarId < p->baseInBoolGsId + p->iChCnt )
  {
    unsigned outVarId = p->baseOutBoolGsId + (groupIdx * p->iChCnt) + (evt->dstVarId - p->baseInBoolGsId);
    cmDspValueSet(ctx, inst, outVarId, evt->valuePtr, 0 );        
    return rc;
  }

  return rc;
}


cmDspClass_t* cmGSwitchClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmGSwitchDC,ctx,"GSwitch",
    NULL,
    _cmDspGSwitchAlloc,
    NULL,
    _cmDspGSwitchReset,
    NULL,
    _cmDspGSwitchRecv,
    NULL,NULL,
    "Ganged switch.");

  return &_cmGSwitchDC;
}


//------------------------------------------------------------------------------------------------------------
//)
//( { label:cmDspScaleRange file_desc:"Offset and scale a scalar value." kw:[sunit] }

enum
{
  kMinInSrId,
  kMaxInSrId,
  kMinOutSrId,
  kMaxOutSrId,
  kValInSrId,
  kValOutSrId,
};

cmDspClass_t _cmScaleRangeDC;

typedef struct
{
  cmDspInst_t    inst;

} cmDspScaleRange_t;


cmDspInst_t*  _cmDspScaleRangeAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  va_list vl1;
  va_copy(vl1,vl);

  cmDspVarArg_t args[] =
  {
    { "min_in",   kMinInSrId,  0,0, kInDsvFl | kDoubleDsvFl , "Min Input value."},
    { "max_in",   kMaxInSrId,  0,0, kInDsvFl | kDoubleDsvFl , "Min Input value."},
    { "min_out",  kMinOutSrId, 0,0, kInDsvFl | kDoubleDsvFl , "Min Input value."},
    { "max_out",  kMaxOutSrId, 0,0, kInDsvFl | kDoubleDsvFl , "Min Input value."},
    { "val_in",   kValInSrId,  0,0, kInDsvFl | kDoubleDsvFl,  "Input value."},
    { "val_out",  kValOutSrId, 0,0, kOutDsvFl | kDoubleDsvFl, "Output value"},
    { NULL, 0, 0, 0, 0 }
  };

  cmDspScaleRange_t* p = cmDspInstAlloc(cmDspScaleRange_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);

  cmDspSetDefaultDouble(ctx,&p->inst,kMinInSrId,0,0);
  cmDspSetDefaultDouble(ctx,&p->inst,kMaxInSrId,0,1.0);
  cmDspSetDefaultDouble(ctx,&p->inst,kMinOutSrId,0,0);
  cmDspSetDefaultDouble(ctx,&p->inst,kMaxOutSrId,0,1.0);


  return &p->inst;
}

cmDspRC_t _cmDspScaleRangeReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t       rc          = kOkDspRC;

  cmDspApplyAllDefaults(ctx,inst);

  return rc;
}

cmDspRC_t _cmDspScaleRangeRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t       rc = kOkDspRC;
  //cmDspScaleRange_t* p  = (cmDspScaleRange_t*)inst;

  cmDspSetEvent(ctx,inst,evt);

  if( evt->dstVarId == kValInSrId )
  {
    double val     = cmDspDouble(inst,kValInSrId);
    double min_in  = cmDspDouble(inst,kMinInSrId);
    double max_in  = cmDspDouble(inst,kMaxInSrId);    
    double min_out = cmDspDouble(inst,kMinOutSrId);    
    double max_out = cmDspDouble(inst,kMaxOutSrId);    

    double x = cmMax(min_in,cmMin(max_in,val));
    
    x = (x - min_in)/(max_in - min_in);

    x = min_out + x * (max_out - min_out);

    cmDspSetDouble(ctx,inst,kValOutSrId, x );
    //printf("%f (%f %f) : (%f %f) %f\n",val,min_in,max_in,min_out,max_out,x);
  }
  return rc;
}


cmDspClass_t* cmScaleRangeClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmScaleRangeDC,ctx,"ScaleRange",
    NULL,
    _cmDspScaleRangeAlloc,
    NULL,
    _cmDspScaleRangeReset,
    NULL,
    _cmDspScaleRangeRecv,
    NULL,NULL,
    "Scale a value inside an input range to a value in the output range.");

  return &_cmScaleRangeDC;
}


//------------------------------------------------------------------------------------------------------------
//)
//( { label:cmDspActiveMeas file_desc:"Issue stored parameter values at specified score locations." kw:[sunit] }

enum
{
  kCntAmId,
  kSflocAmId,
  kLocAmId,
  kTypeAmId,
  kValueAmId,
  kCstAmId,
  kCmdAmId,
  kScLocAmId,
  kEvenAmId,
  kDynAmId,
  kTempoAmId,
  kCostAmId
};

cmDspClass_t _cmActiveMeasDC;

typedef struct cmDspAmRecd_str
{
  unsigned loc;
  unsigned type;
  double   value;
  double   cost;
  struct cmDspAmRecd_str* link;
} cmDspAmRecd_t;


//int cmDspActiveMeasRecdCompare(const void * p0, const void * p1)
//{
//  return ((int)((cmDspActiveMeasRecd_t*)p0)->loc) - (int)(((cmDspActiveMeasRecd_t*)p1)->loc);
//}

typedef struct
{
  cmDspInst_t            inst;
  unsigned               addSymId;
  unsigned               clearSymId;
  unsigned               printSymId;
  unsigned               rewindSymId;
  cmDspAmRecd_t*         array;       // array[cnt]
  unsigned               cnt;     
  cmDspAmRecd_t*         list;        // first recd in list sorted on 'loc'.
  cmDspAmRecd_t*         avail;       // next empty recd
  cmDspAmRecd_t*         sent;        // last recd sent
} cmDspActiveMeas_t;

void _cmDspAmAllocList( cmDspActiveMeas_t* p, unsigned cnt )
{
  assert(p->array == NULL );

  cmDspAmRecd_t* r = cmMemAllocZ(cmDspAmRecd_t,cnt);

  p->cnt   = cnt;
  p->array = r;
  p->list  = NULL;
  p->avail = r;
  p->sent  = NULL;
}


cmDspRC_t _cmDspActiveMeasAdd( cmDspCtx_t* ctx, cmDspActiveMeas_t* p, unsigned loc, unsigned type, double value, double cost)
{
  assert( type != kInvalidVarScId );

  cmDspAmRecd_t* rp = p->list;
  cmDspAmRecd_t* pp = NULL;
  

  // search for the location to add the new record
  for(; rp!=NULL; rp=rp->link)
  {
    // if this loc and type already exists then replace the value and cost fields
    if( rp->loc==loc && rp->type==type )
      goto foundLabel;

    // if this loc should be inserted before rp
    if( loc < rp->loc )
      break;
    
    pp = rp;
  }

  // if the pre-allocated list is full
  if( p->avail >= p->array+p->cnt )
    return cmDspInstErr(ctx,&p->inst,kInvalidArgDspRC,"Unable to store new measurement record. All preallocated active measurement slots are in use.");


  // if prepending to the list
  if( pp == NULL )
  {
    rp        = p->avail;
    rp->link  = p->list;
    p->list   = rp;
  }
  else
  {
    // if appending to the list after pp
    if( rp == NULL )
    {
      // nothing to do
    }
    else // if inserting between pp and rp
    {
      p->avail->link = rp;
    }

    rp       = p->avail;
    pp->link = rp;

  }

  p->avail += 1;
  
 foundLabel:
  rp->loc   = loc;
  rp->type  = type;
  rp->value = value;
  rp->cost  = cost;

  return kOkDspRC;
}


cmDspInst_t*  _cmDspActiveMeasAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{

  cmDspVarArg_t args[] =
  {
    { "cnt",      kCntAmId,      0,0, kInDsvFl  | kUIntDsvFl,    "Maximum count of active measurements."},
    { "sfloc",    kSflocAmId,    0,0, kInDsvFl  | kUIntDsvFl,    "Score follower location input." },
    { "loc",      kLocAmId,      0,0, kInDsvFl  | kUIntDsvFl,    "Meas. location." },
    { "type",     kTypeAmId,     0,0, kInDsvFl  | kUIntDsvFl,    "Meas. Type. (even,dyn,...)" },
    { "val",      kValueAmId,    0,0, kInDsvFl  | kDoubleDsvFl,  "Meas. Value."},
    { "cst",      kCstAmId,      0,0, kInDsvFl  | kDoubleDsvFl,  "Meas. Cost."},
    { "cmd",      kCmdAmId,      0,0, kInDsvFl  | kSymDsvFl,     "Commands:add | clear | dump | rewind"}, 
    { "scloc",    kScLocAmId,    0,0, kOutDsvFl | kUIntDsvFl,    "Score location"},
    { "even",     kEvenAmId,     0,0, kOutDsvFl | kDoubleDsvFl,  "Even out"},
    { "dyn",      kDynAmId,      0,0, kOutDsvFl | kDoubleDsvFl,  "Dyn out"},
    { "tempo",    kTempoAmId,    0,0, kOutDsvFl | kDoubleDsvFl,  "Tempo out"},
    { "cost",     kCostAmId,     0,0, kOutDsvFl | kDoubleDsvFl,  "Cost out"},
    { NULL, 0, 0, 0, 0 }
  };



  cmDspActiveMeas_t* p = cmDspInstAlloc(cmDspActiveMeas_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);

  p->addSymId   = cmSymTblRegisterStaticSymbol(ctx->stH,"add");
  p->clearSymId = cmSymTblRegisterStaticSymbol(ctx->stH,"clear");
  p->printSymId = cmSymTblRegisterStaticSymbol(ctx->stH,"dump");
  p->rewindSymId= cmSymTblRegisterStaticSymbol(ctx->stH,"rewind");

  cmDspSetDefaultUInt(  ctx,&p->inst,kCntAmId,  0,256);
  cmDspSetDefaultUInt(  ctx,&p->inst,kScLocAmId,0,0);
  cmDspSetDefaultDouble(ctx,&p->inst,kEvenAmId, 0,0);
  cmDspSetDefaultDouble(ctx,&p->inst,kDynAmId,  0,0);
  cmDspSetDefaultDouble(ctx,&p->inst,kTempoAmId,0,0);
  cmDspSetDefaultDouble(ctx,&p->inst,kTempoAmId,0,0);

  return &p->inst;
}

cmDspRC_t _cmDspActiveMeasPrint(cmDspCtx_t* ctx, cmDspActiveMeas_t* p )
{
  cmDspAmRecd_t* rp = p->list;
  for(; rp!=NULL; rp=rp->link)
  {
    const cmChar_t* label = "<null>";
    switch( rp->type )
    {
      case kEvenVarScId:    label="even "; break;
      case kDynVarScId:     label="dyn  "; break;
      case kTempoVarScId:   label="tempo"; break;
      default:
        { assert(0); }
    }

    cmRptPrintf(ctx->rpt,"loc:%i %s %f %f\n",rp->loc,label,rp->value,rp->cost);
  }

  return kOkDspRC;
}

cmDspRC_t _cmDspActiveMeasClear(cmDspCtx_t* ctx, cmDspActiveMeas_t* p )
{
  memset(p->array,0,sizeof(p->array[0])*p->cnt);
  p->avail = p->array;
  p->list  = NULL;
  p->avail = p->array;

  return kOkDspRC;
}

cmDspRC_t _cmDspActiveMeasFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspActiveMeas_t* p  = (cmDspActiveMeas_t*)inst;
  cmMemPtrFree(&p->array);
  return kOkDspRC;
}

cmDspRC_t _cmDspActiveMeasReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t          rc = kOkDspRC;
  cmDspActiveMeas_t* p  = (cmDspActiveMeas_t*)inst;

  cmDspApplyAllDefaults(ctx,inst);

  unsigned cnt = cmMax(100,cmDspUInt(inst,kCntAmId));
  _cmDspActiveMeasFree(ctx,inst,evt);
  _cmDspAmAllocList(p,cnt);

  return rc;
}

cmDspRC_t _cmDspActiveMeasRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t          rc = kOkDspRC;
  cmDspActiveMeas_t* p  = (cmDspActiveMeas_t*)inst;
  cmDspSetEvent(ctx,inst,evt);

  switch( evt->dstVarId )
  {
    case kSflocAmId:
      {
                
        unsigned       sfloc  = cmDspUInt(inst,kSflocAmId);                  // get the recv'd score location
        cmDspAmRecd_t* rp     = p->sent==NULL ? p->list : p->sent->link;     // get the next recd to send
        bool           fl     = false;

        for(; rp!=NULL; rp=rp->link)
          if( rp->loc <= sfloc )
          {
            // deterimine the records type
            unsigned varId = cmInvalidId;
            switch( rp->type )
            {
              case kEvenVarScId:   varId = kEvenAmId;  break;
              case kDynVarScId:    varId = kDynAmId;   break;
              case kTempoVarScId:  varId = kTempoAmId; break;
              default:
                { assert(0); }
            }

            // Sending the location triggers the avail-ch to switch - so the location should only
            //  be sent once.
            if( !fl )
            {
              cmDspSetUInt(ctx,inst,kScLocAmId,rp->loc);
              fl = true;
            }

            // transmit the records value and cost
            cmDspSetDouble(ctx,inst,varId,rp->value);
            cmDspSetDouble(ctx,inst,kCostAmId,rp->cost);
            p->sent = rp;
          }
                
      }
      break;

    case kCmdAmId:
      {
        unsigned cmdSymId = cmDspSymbol(inst,kCmdAmId);

        if( cmdSymId == p->addSymId )
          rc = _cmDspActiveMeasAdd(ctx,p,cmDspUInt(inst,kLocAmId),cmDspUInt(inst,kTypeAmId),cmDspDouble(inst,kValueAmId),cmDspDouble(inst,kCstAmId));
        else          
          if( cmdSymId == p->clearSymId )
            rc = _cmDspActiveMeasClear(ctx,p);
          else
            if( cmdSymId == p->printSymId )
              rc = _cmDspActiveMeasPrint(ctx,p);
            else
              if(cmdSymId == p->rewindSymId )
                p->sent = NULL;
      }
      break;

  }

  //  switch( evt->dstVarId )
  //  {
  //    case kSflocAmId:
  //      if( p->nextFullIdx != cmInvalidIdx )
  //      {
  //        // get the recv'd score location
  //        unsigned sflocIdx = cmDspUInt(inst,kSflocAmId);
  //
  //        unsigned prvLoc = cmInvalidIdx;
  //
  //        // for each remaining avail record
  //        for(; p->nextFullIdx < p->nextEmptyIdx; p->nextFullIdx++)
  //        {
  //          cmDspActiveMeasRecd_t* r = p->array + p->nextFullIdx;
  //
  //          // if this records score location is after the recv'd score loc then we're done
  //          if( r->loc > sflocIdx )
  //            break;
  //
  //          // deterimine the records type
  //          unsigned varId = cmInvalidId;
  //          switch( r->type )
  //          {
  //            case kEvenVarScId:   varId = kEvenAmId;  break;
  //            case kDynVarScId:    varId = kDynAmId;   break;
  //            case kTempoVarScId:  varId = kTempoAmId; break;
  //            default:
  //              { assert(0); }
  //          }
  //
  //          // if this score location has not yet been sent then send it now
  //          if( prvLoc != r->loc )
  //            cmDspSetUInt(ctx,inst,kScLocAmId,r->loc);
  //
  //          // transmit the records value and cost
  //          cmDspSetDouble(ctx,inst,varId,r->value);
  //          cmDspSetDouble(ctx,inst,kCostAmId,r->cost);
  //
  //          prvLoc = r->loc;
  //        } 
  //        
  //
  //      }
  //      break;
  //
  //    case kCmdAmId:
  //      {
  //        unsigned cmdSymId = cmDspSymbol(inst,kCmdAmId);
  //
  //        if( cmdSymId == p->addSymId )
  //        {
  //          if( p->nextEmptyIdx >= p->cnt )
  //            cmDspInstErr(ctx,inst,kProcFailDspRC,"The active measurement list is full cnt=%i.",p->cnt);
  //          else
  //          {
  //            cmDspActiveMeasRecd_t* r = p->array + p->nextEmptyIdx;
  //            r->loc   = cmDspUInt(  inst,kLocAmId);
  //            r->type  = cmDspUInt(  inst,kTypeAmId);
  //            r->value = cmDspDouble(inst,kValueAmId);
  //            r->cost  = cmDspDouble(inst,kCstAmId);
  //            p->nextEmptyIdx += 1;
  //
  //            qsort(p->array,p->nextEmptyIdx,sizeof(p->array[0]),cmDspActiveMeasRecdCompare);
  //
  //            if( p->nextEmptyIdx == 1 && p->nextFullIdx == cmInvalidIdx )
  //              p->nextFullIdx = 0;
  //
  //          }
  //        }
  //          
  //        if( cmdSymId == p->clearSymId )
  //          rc = _cmDspActiveMeasClear(ctx,p);
  //        else
  //          if( cmdSymId == p->printSymId )
  //            rc = _cmDspActiveMeasPrint(ctx,p);
  //          else
  //            if(cmdSymId == p->rewindSymId )
  //              p->nextFullIdx = 0;
  //      }
  //      break;
  //
  //  }

  return rc;
}


cmDspClass_t* cmActiveMeasClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmActiveMeasDC,ctx,"ActiveMeas",
    NULL,
    _cmDspActiveMeasAlloc,
    _cmDspActiveMeasFree,
    _cmDspActiveMeasReset,
    NULL,
    _cmDspActiveMeasRecv,
    NULL,NULL,
    "Scale a value inside an input range to a value in the output range.");

  return &_cmActiveMeasDC;
}

//------------------------------------------------------------------------------------------------------------
//)
//( { label:cmDspAmSync  file_desc:"Calculate MIDI to Audio latency offsets." kw:[sunit] }
//
//
// Usage:
// 1)  In the program resource file setup a list of sync points.
// 'asmp' refers to a sample offset into the audio file 'af'
// which should match to the midi event index 'mid' in the
// midi file 'mf'.
//
//  amSync :
//  [
//   { af:"af-16" asmp:34735276  mf:"mf-10"  mid:350 }
//   { af:"af-16" asmp:71802194  mf:"mf-10"  mid:787 }
//  ]
//
// 2) Feed the 'fidx' output from a wave table loaded with 'af' into the 'asmp' input port of this amSync object.
//    Feed the 'id' output from the MIDI file player loaded with 'mf' into the 'mid' input port of this amSync object.
//
// 3) Run the players. 
// 4) When the run is complete send any message to the 'sel' port of this amSync object.
//    The 'frm:' field of the printed output gives the difference in samples between
//    MIDI and audio sync points.
//
//    If the value is positive then the MIDI point is after the Audio point.
//    If the value is negative then the MIDI point is before the audio point.
//


enum
{
  kSelAmId,    
  kAFnAmId,
  kASmpAmId,
  kMFnAmId,
  kMIdAmId,
};

enum
{
  kAfnAmFl = 0x01,
  kMfnAmFl = 0x02,
  kAsmpAmFl = 0x04,
  kMidAmFl  = 0x08,
};

cmDspClass_t _cmAmSyncDC;

typedef struct cmDspAmSyncEntry_str
{
  const cmChar_t* afn;  // audio file name  
  const cmChar_t* mfn;  // midi file name   
  unsigned        asmp; // Audio sample index to sync to MIDI event
  unsigned        mid;  // MIDI event unique id (cmMidiTrackMsg_t.uid)
  int             afi;  // closest DSP system cycle index to the reference audio sample index (asmp).
  int             mfi;  // DSP system cycle on which the reference MIDI event (mid) arrived.
  unsigned        state; // as incoming msg match this record the state is updated with kXXXAmFl flags 
} cmDspAmSyncEntry_t;

typedef struct
{
  cmDspInst_t         inst;
  cmDspAmSyncEntry_t* array;
  unsigned            arrayCnt;
  cmDspAmSyncEntry_t* acur;
  cmDspAmSyncEntry_t* mcur;
} cmDspAmSync_t;

cmDspInst_t*  _cmDspAmSyncAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "sel",    kSelAmId,    0, 0, kInDsvFl  | kTypeDsvMask,  "Print and reset" },
    { "afn",    kAFnAmId,    0, 0, kInDsvFl  | kStrzDsvFl, "Audio File name"},
    { "asmp",   kASmpAmId,   0, 0, kInDsvFl  | kIntDsvFl,  "Audio sample index"},
    { "mfn",    kMFnAmId,    0, 0, kInDsvFl  | kStrzDsvFl, "MIDI File name"},
    { "mid",    kMIdAmId,    0, 0, kInDsvFl  | kIntDsvFl,  "MIDI Event Unique Id"},
    { NULL, 0, 0, 0, 0 }
  };

  cmDspAmSync_t* p = cmDspInstAlloc(cmDspAmSync_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);

  return &p->inst;
}

cmDspRC_t _cmDspAmSyncFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspAmSync_t* p = (cmDspAmSync_t*)inst;
  
  cmMemFree(p->array);

  return kOkDspRC;
}


cmDspRC_t _cmDspAmSyncReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t      rc = kOkDspRC;
  cmDspAmSync_t* p  = (cmDspAmSync_t*)inst;
 
  cmDspApplyAllDefaults(ctx,inst);

  cmJsonH_t       jsH = cmDspSysPgmRsrcHandle(ctx->dspH);
  cmJsonNode_t*   np;
  const cmChar_t* errLabelPtr;
  unsigned        i;
  cmJsRC_t        jsRC;

  if((np = cmJsonFindValue(jsH, "amSync", NULL, kArrayTId)) == NULL )
  {
    rc = cmDspInstErr(ctx,inst,kRsrcNotFoundDspRC,"The AUDIO MIDI Sync cfg. record was not found.");
    goto errLabel; 
  }
  
  p->arrayCnt = cmJsonChildCount(np);

  p->array    = cmMemResizeZ(cmDspAmSyncEntry_t,p->array,p->arrayCnt);

  for(i=0; i<p->arrayCnt; ++i)
  {
    cmJsonNode_t* anp = cmJsonArrayElement(np,i);

    cmDspAmSyncEntry_t* r = p->array + i;
    if( (jsRC = cmJsonMemberValues(anp,&errLabelPtr,
        "af",  kStringTId,&r->afn,
        "asmp",kIntTId,   &r->asmp,
        "mf",  kStringTId,&r->mfn,
        "mid", kIntTId,   &r->mid, 
          NULL)) != kOkJsRC )
    {
      if( jsRC == kNodeNotFoundJsRC )
        rc = cmDspInstErr(ctx,inst,kRsrcNotFoundDspRC,"The Audio-MIDI Sync cfg. field '%s' was missing in the cfg. record at index %i.",errLabelPtr,i);
      else
        rc = cmDspInstErr(ctx,inst,kInvalidArgDspRC,"The AUDIO MIDI Sync cfg. parse failed on the record at index %i.",i);
      break;
    }
    r->afi = cmInvalidIdx;
    r->mfi = cmInvalidIdx;
    r->state = 0;
  }
  
 errLabel:
  return rc;
}


cmDspRC_t _cmDspAmSyncRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  cmDspAmSync_t* p = (cmDspAmSync_t*)inst;
  unsigned i;

  if( evt->dstVarId != kSelAmId )
    cmDspSetEvent(ctx,inst,evt);

  switch(evt->dstVarId)
  {
    case kSelAmId:    
      {
        double srate = cmDspSysSampleRate(ctx->dspH);
        int    fpc   = cmDspSamplesPerCycle(ctx);

        for(i=0; i<p->arrayCnt; ++i)
        {
          cmDspAmSyncEntry_t* r = p->array + i;

          int dframes = r->mfi-r->afi; 
          cmRptPrintf(ctx->rpt,"0x%x : %s %i %i - %s %i  %i : frm:%i smp:%i sec:%f\n",
            r->state,
            r->afn,
            r->asmp,
            r->afi,
            r->mfn,
            r->mid,
            r->mfi,
            dframes,
            dframes*fpc,dframes*fpc/srate);

          r->afi = cmInvalidIdx;
          r->mfi = cmInvalidIdx;
          r->state = 0;

        }   
        p->acur = NULL;
        p->mcur = NULL;
      }
      break;

    case kAFnAmId:
      {
        // an audio file name just arrived - set p->acur to point to it
        const cmChar_t* fn = cmDspStrcz(inst, kAFnAmId );
        
        for(i=0; i<p->arrayCnt; ++i)
          if( strcmp(fn,p->array[i].afn) == 0 )
          {
            p->array[i].state = cmSetFlag(p->array[i].state,kAfnAmFl);
            p->acur = p->array + i;
          }
      }
      break;

    case kMFnAmId:
      {
        // a midi file name just arrived - set p->mcur to point to it
        const cmChar_t* fn = cmDspStrcz(inst, kMFnAmId );
        for(i=0; i<p->arrayCnt; ++i)
          if( strcmp(fn,p->array[i].mfn) == 0 )
          {
            p->array[i].state = cmSetFlag(p->array[i].state,kMfnAmFl);
            p->mcur = p->array + i;
          }
      }
      break;

    case kASmpAmId:
      {
        // a audio file sample index has just arrived
        int v = cmDspInt(inst,kASmpAmId); 
        
        // if a valid audio file has been set
        if( p->acur != NULL )
          for(i=0; i<p->arrayCnt; ++i)
          {
            // if the audio sync point is before or on the new audio file sample index then 
            // this is the closest audio file index to the audio sync point - record the 
            // associated cycleCnt
            cmDspAmSyncEntry_t* r = p->array + i;
            if( cmIsNotFlag(r->state,kAsmpAmFl)  && r->asmp <= v && strcmp(p->acur->afn,r->afn)==0  )
            {
              r->afi = ctx->cycleCnt;
              r->state = cmSetFlag(r->state,kAsmpAmFl);
              break;
            }
          }
      }
      break;

    case kMIdAmId:
      {
        // a new MIDI event was received 
        int v = cmDspInt(inst,kMIdAmId);
        if( p->mcur != NULL )
          for(i=0; i<p->arrayCnt; ++i)
          {
            // if the new MIDI event matched the MIDI sync point then record the 
            // current cycleCnt.
            cmDspAmSyncEntry_t* r = p->array + i;
            if( cmIsNotFlag(r->state,kMidAmFl)  && r->mid == v && strcmp(p->mcur->mfn,r->mfn)==0  )
            {
              r->mfi = ctx->cycleCnt;
              r->state = cmSetFlag(r->state,kMidAmFl);          
              break;
            }
          }

      }
      break;

  }
  return kOkDspRC;
}

cmDspClass_t* cmAmSyncClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmAmSyncDC,ctx,"AmSync",
    NULL,
    _cmDspAmSyncAlloc,
    _cmDspAmSyncFree,
    _cmDspAmSyncReset,
    NULL,
    _cmDspAmSyncRecv,
    NULL,NULL,
    "Audio - MIDI Sync Object.");

  return &_cmAmSyncDC;
}

//------------------------------------------------------------------------------------------------------------
//)
//( { label:cmDspNanoMap file_desc:"Control a MIDI synth." kw:[sunit] }

enum
{
  kPgmNmId,
  kStatusNmId,
  kD0NmId,
  kD1NmId,
  kThruNmId
};

cmDspClass_t _cmNanoMapDC;

typedef struct
{
  cmDspInst_t inst;

} cmDspNanoMap_t;

cmDspRC_t _cmDspNanoMapSend( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned st, unsigned d0, unsigned d1 )
{
  cmDspSetUInt(ctx,inst,kD1NmId,d1);
  cmDspSetUInt(ctx,inst,kD0NmId,d0);
  cmDspSetUInt(ctx,inst,kStatusNmId,st);
  return kOkDspRC;
}

void _cmDspNanoMapPgm( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned pgm )
{
  //cmDspNanoMap_t* p = (cmDspNanoMap_t*)inst;

  unsigned i;
        
  for(i=0; i<kMidiChCnt; ++i)
  {
    _cmDspNanoMapSend(ctx,inst,kCtlMdId+i,121,0); // reset all controllers
    _cmDspNanoMapSend(ctx,inst,kCtlMdId+i,123,0); // turn all notes off
    _cmDspNanoMapSend(ctx,inst,kCtlMdId+i,0,0);   // switch to bank 0
    _cmDspNanoMapSend(ctx,inst,kPgmMdId+i,pgm,0); // send pgm change
    cmSleepMs(15);
  }
  
}

cmDspInst_t*  _cmDspNanoMapAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "pgm",    kPgmNmId,    0,  0,              kInDsvFl | kUIntDsvFl | kOptArgDsvFl, "Reprogram all channels to this pgm." },
    { "status", kStatusNmId, 0,  0,  kOutDsvFl | kInDsvFl | kUIntDsvFl | kOptArgDsvFl, "MIDI status" },
    { "d0",     kD0NmId,     0,  0,  kOutDsvFl | kInDsvFl | kUIntDsvFl | kOptArgDsvFl, "MIDI channel message d0" },
    { "d1",     kD1NmId,     0,  0,  kOutDsvFl | kInDsvFl | kUIntDsvFl | kOptArgDsvFl, "MIDI channel message d1" },
    { "thru",   kThruNmId,   0,  0,              kInDsvFl | kBoolDsvFl | kOptArgDsvFl, "Enable pass through."},
    { NULL, 0, 0, 0, 0 }
  };

  cmDspNanoMap_t* p = cmDspInstAlloc(cmDspNanoMap_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);
  
  cmDspSetDefaultUInt(ctx,&p->inst, kPgmNmId, 0, 0 );

  return &p->inst;
}

cmDspRC_t _cmDspNanoMapReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t      rc = kOkDspRC;

  cmDspApplyAllDefaults(ctx,inst);

  _cmDspNanoMapPgm(ctx,inst,cmDspUInt(inst,kPgmNmId));

  return rc;
} 

cmDspRC_t _cmDspNanoMapRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  //cmDspNanoMap_t* p = (cmDspNanoMap_t*)inst;

  switch( evt->dstVarId )
  {
    case kPgmNmId:
      cmDspSetEvent(ctx,inst,evt);
      _cmDspNanoMapPgm(ctx,inst,cmDspUInt(inst,kPgmNmId));
      break;

    case kStatusNmId:
      {
        unsigned status = cmDsvGetUInt(evt->valuePtr);        
        unsigned stat_no_ch = status & 0xf0;
        if( stat_no_ch == kNoteOnMdId || stat_no_ch == kNoteOffMdId || stat_no_ch == kCtlMdId  )
        {
          //unsigned d0 = cmDspUInt(inst,kD0NmId);
          unsigned ch = 0; //d0 % 8;
          status = (status & 0xf0) + ch;
          cmDspSetUInt(ctx,inst,kStatusNmId,status);
        }
       
      }
      break;


    default:
      cmDspSetEvent(ctx,inst,evt);
      break;
  }


  return kOkDspRC;
}

cmDspClass_t* cmNanoMapClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmNanoMapDC,ctx,"NanoMap",
    NULL,
    _cmDspNanoMapAlloc,
    NULL,
    _cmDspNanoMapReset,
    NULL,
    _cmDspNanoMapRecv,
    NULL,
    NULL,
    "Nanosynth Mapper");

  return &_cmNanoMapDC;
}

//------------------------------------------------------------------------------------------------------------
//)
//( { label:cmDspRecdPlay file_desc:"Record audio segments from a live perfromance and play them back at a later time" kw:[sunit] }

enum
{
  kChCntPrId,
  kFnPrId,
  kSecsPrId,
  kMaxLaSecsPrId,
  kCurLaSecsPrId,
  kFadeRatePrId,
  kScInitLocIdxPrId,
  kScLocIdxPrId,
  kCmdPrId,
  kInAudioBasePrId
};

cmDspClass_t _cmRecdPlayDC;

typedef struct
{
  cmDspInst_t inst;
  cmRecdPlay* rcdply;
  cmScH_t     scH;
  unsigned    onSymId;
  unsigned    offSymId;
  unsigned    audioOutBaseId;
  unsigned    chCnt;
  //unsigned    scLocIdx;
} cmDspRecdPlay_t;

cmDspRC_t _cmDspRecdPlayParseRsrc( cmDspCtx_t* ctx, cmDspInst_t* inst, cmRecdPlay* rcdply )
{
  cmDspRC_t       rc   = kOkDspRC;
  const cmChar_t* path = NULL;

  // read the 'recdplay' audio file path
  if( cmDspRsrcString( ctx->dspH, &path, "recdPlayPath", NULL ) != kOkDspRC )
  {
    cmDspInstErr(ctx,inst,kRsrcNotFoundDspRC,"The 'recdPlayPath' resource string was not found.");
  }

  if( path == NULL )
    path = "";

  cmJsonH_t     jsH = cmDspSysPgmRsrcHandle(ctx->dspH);
  cmJsonNode_t* jnp = cmJsonFindValue(jsH,"recdPlay",NULL, kArrayTId);

  if( jnp == NULL || cmJsonIsArray(jnp)==false )
  {
    // this is really a warning - the object does not require preloaded segments.
    cmDspInstErr(ctx,inst,kRsrcNotFoundDspRC,"Warning: The 'recdPlay' resource used to define pre-loaded segments was not found.");
    return kOkDspRC;
  }

  unsigned n = cmJsonChildCount(jnp);
  unsigned i;

  // for each 'recdplay' segment record
  for(i=0; i<n && rc==kOkDspRC; ++i)
  {
    cmJsonNode_t*   cnp      = cmJsonArrayElement(jnp,i);
    const cmChar_t* label    = NULL;
    unsigned        segSymId = cmInvalidId;
    const cmChar_t* errLabel = NULL;
    const cmChar_t* fn       = NULL;

    // read the ith segment record
    if( cmJsonMemberValues(cnp,&errLabel,
        "label", kStringTId, &label,
        "file",  kStringTId, &fn,
        NULL) != kOkJsRC )
    {
      rc = cmDspInstErr(ctx,inst,kRsrcNotFoundDspRC,"The record at index %i in the 'recdPlay' pre-loaded segment list could not be parsed.",i);
      goto errLabel;
    }

    // find or generate the symbol id for the segment label symbol
    if((segSymId = cmSymTblRegisterSymbol(ctx->stH,label)) == cmInvalidId )
    {
      rc = cmDspInstErr(ctx,inst,kSymNotFoundDspRC,"The 'recdPlay' pre-load segment symbol '%s' could not be found or registered.",cmStringNullGuard(label));
      goto errLabel;
    }

    // create the full path name for the segment audio file
    if((fn = cmFsMakeFn( path, fn, NULL, NULL )) == NULL )
    {
      rc = cmDspInstErr(ctx,inst,kFileSysFailDspRC,"The 'recdPlay' file name '%s/%s' could not be generated.",cmStringNullGuard(path),cmStringNullGuard(fn));
      goto errLabel;
    }
    

    // pre-load the segment
    if( cmRecdPlayInsertRecord(rcdply,segSymId,fn) != cmOkRC )
      rc = cmDspInstErr(ctx,inst,kSubSysFailDspRC,"The 'recdPlay' segment label:'%s'  file:'%s' could not be loaded.",cmStringNullGuard(label),cmStringNullGuard(fn));

   
    cmFsFreeFn(fn);
        
  }
  
 errLabel:
  return rc;
}

cmDspRC_t _cmDspRecdPlayOpenScore( cmDspCtx_t* ctx, cmDspInst_t* inst )
{
  cmDspRC_t rc =kOkDspRC;
  const cmChar_t* fn;

  cmDspRecdPlay_t* p = (cmDspRecdPlay_t*)inst;

  //p->scLocIdx = 0;


  if((fn = cmDspStrcz(inst,kFnPrId)) == NULL || strlen(fn)==0 )
    return cmDspInstErr(ctx,inst, kInvalidArgDspRC, "No score file name supplied.");

  if( cmScoreInitialize(ctx->cmCtx, &p->scH, fn, cmDspSampleRate(ctx), NULL, 0, NULL, NULL, ctx->stH ) != kOkScRC )
    return cmDspInstErr(ctx,inst, kSubSysFailDspRC, "Unable to open the score '%s'.",fn);

  if( cmScoreIsValid(p->scH) )
  {
    unsigned i;
    unsigned markerCnt = cmScoreMarkerLabelCount(p->scH);
    double initFragSecs = cmDspDouble(inst,kSecsPrId);
    double maxLaSecs    = cmDspDouble(inst,kMaxLaSecsPrId);
    double curLaSecs    = cmDspDouble(inst,kCurLaSecsPrId);

    printf("2 max la secs:%f\n",cmDspDouble(inst,kMaxLaSecsPrId));


    if((p->rcdply = cmRecdPlayAlloc(ctx->cmProcCtx, NULL, cmDspSampleRate(ctx), markerCnt, p->chCnt, initFragSecs, maxLaSecs, curLaSecs)) == NULL)
      return cmErrMsg(&inst->classPtr->err,kSubSysFailDspRC,"Unable to create the internal recorder-player object.");    

    for(i=0; i<markerCnt; ++i)
      cmRecdPlayRegisterFrag(p->rcdply,i, cmScoreMarkerLabelSymbolId(p->scH,i ));

    if((rc = _cmDspRecdPlayParseRsrc(ctx,inst,p->rcdply)) != kOkDspRC )
      rc = cmDspInstErr(ctx,inst,kInstResetFailDspRC,"The 'recdplay' segment pre-load failed.");

    //p->scLocIdx = cmDspUInt(inst,kScInitLocIdxPrId);

  }

  return rc;
}


cmDspInst_t*  _cmDspRecdPlayAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{

  if( va_cnt < 1 )
  {
    cmDspClassErr(ctx,classPtr,kVarArgParseFailDspRC,"The 'RecdPlay' constructor must have a count of input ports.");
    return NULL;
  }

  va_list vl1;
  va_copy(vl1,vl);

  int      chCnt         = va_arg(vl,int);
  unsigned audioOutBase  = kInAudioBasePrId + chCnt;    

  cmDspRecdPlay_t* p = cmDspInstAllocV(cmDspRecdPlay_t,ctx,classPtr,instSymId,id,storeSymId,va_cnt,vl1,
    1,         "chs",    kChCntPrId,      0,0, kUIntDsvFl | kReqArgDsvFl,              "channel count.",
    1,         "fn",     kFnPrId,         0,0, kInDsvFl   | kStrzDsvFl   | kReqArgDsvFl, "Score file." ,
    1,         "secs",   kSecsPrId,       0,0, kInDsvFl   | kDoubleDsvFl | kReqArgDsvFl, "Initial fragment allocation in seconds.",
    1,         "maxla",  kMaxLaSecsPrId,  0,0, kInDsvFl   | kDoubleDsvFl | kReqArgDsvFl, "Maximum look-ahead buffer in seconds.",
    1,         "curla",  kCurLaSecsPrId,  0,0, kInDsvFl   | kDoubleDsvFl | kOptArgDsvFl, "Current look-head buffer in seconds.",
    1,         "frate",  kFadeRatePrId,   0,0, kInDsvFl   | kDoubleDsvFl | kOptArgDsvFl, "Fade rate in dB per second.",
    1,         "initIdx",kScInitLocIdxPrId,0,0,kInDsvFl   | kUIntDsvFl,                  "Score search start location.",
    1,         "index",  kScLocIdxPrId,   0,0, kInDsvFl   | kUIntDsvFl,                "Score follower location index.",
    1,         "cmd",    kCmdPrId,        0,0, kInDsvFl   | kSymDsvFl,                 "on=reset off=stop.",
    chCnt,     "in",     kInAudioBasePrId,0,1, kInDsvFl   | kAudioBufDsvFl,            "Audio input",
    chCnt,     "out",    audioOutBase,    0,1, kOutDsvFl  | kAudioBufDsvFl,            "Audio output",
    0 );

  va_end(vl1);

  p->onSymId        = cmSymTblId(ctx->stH,"on");
  p->offSymId       = cmSymTblId(ctx->stH,"off");
  p->audioOutBaseId = audioOutBase;
  p->chCnt          = chCnt;
  //p->scLocIdx       = 0;

  printf("0 max la secs:%f\n",cmDspDouble(&p->inst,kMaxLaSecsPrId));

  cmDspSetDefaultDouble(ctx,&p->inst, kSecsPrId,     0.0, 10.0 );
  cmDspSetDefaultDouble(ctx,&p->inst, kMaxLaSecsPrId,0.0, 2.0);
  cmDspSetDefaultDouble(ctx,&p->inst, kCurLaSecsPrId,0.0, 0.1);
  cmDspSetDefaultDouble(ctx,&p->inst, kFadeRatePrId, 0.0, 1.0);
  cmDspSetDefaultUInt(  ctx,&p->inst, kScInitLocIdxPrId,0,0);

  printf("1 max la secs:%f\n",cmDspDouble(&p->inst,kMaxLaSecsPrId));

  


  return &p->inst;
}

cmDspRC_t _cmDspRecdPlayFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t        rc = kOkDspRC;
  cmDspRecdPlay_t* p = (cmDspRecdPlay_t*)inst;

  cmRecdPlayFree(&p->rcdply);

  cmScoreFinalize(&p->scH);
  return rc;
}

cmDspRC_t _cmDspRecdPlayReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t rc;
  if((rc = _cmDspRecdPlayOpenScore(ctx,inst)) == kOkDspRC )
    cmDspApplyAllDefaults(ctx,inst);

  return rc;
} 

cmDspRC_t _cmDspRecdPlayExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t      rc = kOkDspRC;

  cmDspRecdPlay_t* p = (cmDspRecdPlay_t*)inst;

  const cmSample_t* x[ p->chCnt ];
  cmSample_t*       y[ p->chCnt ];
  unsigned n = 0;
  unsigned i;
  unsigned actChCnt = 0;

  for(i=0; i<p->chCnt; ++i)
  {
    if( cmDspIsAudioInputConnected(ctx,inst,kInAudioBasePrId+i) == false )
    {
      x[i] = NULL;
      y[i] = NULL;
    }
    else
    {
      if( i==0 )
        n  = cmDspAudioBufSmpCount(ctx,inst,kInAudioBasePrId+i,0);
      else
      { 
        
        assert( n == cmDspAudioBufSmpCount(ctx,inst,kInAudioBasePrId+i,0)); 
      }
      
      x[i] = cmDspAudioBuf(ctx,inst,kInAudioBasePrId+i,0);
      
      if( x[i] != NULL )
      {
        y[i] = cmDspAudioBuf(ctx,inst,p->audioOutBaseId+i,0);

        if( y[i] != NULL )
        {
          assert( n == cmDspAudioBufSmpCount(ctx,inst,p->audioOutBaseId+i,0));

          cmVOS_Zero(y[i],n);

          actChCnt += 1;
        }
      
      }
    }
  }

  cmRecdPlayExec(p->rcdply,x,y,actChCnt,n);

  return rc;
}

cmDspRC_t _cmDspRecdPlayRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRecdPlay_t* p = (cmDspRecdPlay_t*)inst;

  if( p->rcdply == NULL )
    return kOkDspRC;

  cmDspSetEvent(ctx,inst,evt);

  switch( evt->dstVarId )
  {
    case kCmdPrId:
      if( cmDspSymbol(inst,kCmdPrId) == p->onSymId )
      {
        printf("rewind\n");
        cmRecdPlayRewind(p->rcdply);
        //p->scLocIdx = cmDspUInt(inst,kScInitLocIdxPrId);
      }
      else
        if( cmDspSymbol(inst,kCmdPrId) == p->offSymId )
        {
        }

      break;

    case kCurLaSecsPrId:
      cmRecdPlaySetLaSecs(p->rcdply, cmDspDouble(inst,kCurLaSecsPrId));
      break;

    case kScInitLocIdxPrId:
      printf("init-idx:%i\n",cmDspUInt(inst,kScInitLocIdxPrId));
      break;

    case kScLocIdxPrId:
      {
        unsigned endScLocIdx = cmDspUInt(inst,kScLocIdxPrId);

        if( endScLocIdx < cmDspUInt(inst,kScInitLocIdxPrId) )
          break;

        cmScoreLoc_t*    loc = cmScoreLoc(p->scH, endScLocIdx );
        if( loc == NULL )
          break;

        cmScoreMarker_t* mp  = loc->markList;

        for(; mp!=NULL; mp=mp->link)
          switch( mp->markTypeId )
          {
            case kRecdBegScMId:
              printf("recd-beg %s\n",cmSymTblLabel(ctx->stH,mp->labelSymId));
              cmRecdPlayBeginRecord(p->rcdply, mp->labelSymId );
              break;
                
            case kRecdEndScMId:
              printf("recd-end %s\n",cmSymTblLabel(ctx->stH,mp->labelSymId));
              cmRecdPlayEndRecord(p->rcdply, mp->labelSymId );
              break;
                
            case kPlayBegScMId:
              printf("play-beg\n");
              cmRecdPlayBeginPlay(p->rcdply, mp->labelSymId );
              break;

            case kPlayEndScMId:
              printf("play-end\n");
              cmRecdPlayEndPlay(p->rcdply, mp->labelSymId );
              break;

            case kFadeScMId:
              printf("fade-beg\n");
              cmRecdPlayBeginFade(p->rcdply, mp->labelSymId, cmDspDouble(inst,kFadeRatePrId) );
              break;

            default:
              break;
          }
        

        //p->scLocIdx = endScLocIdx+1;
      }
      break;
  }

  return kOkDspRC;
}

cmDspClass_t* cmRecdPlayClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmRecdPlayDC,ctx,"RecdPlay",
    NULL,
    _cmDspRecdPlayAlloc,
    _cmDspRecdPlayFree,
    _cmDspRecdPlayReset,
    _cmDspRecdPlayExec,
    _cmDspRecdPlayRecv,
    NULL,
    NULL,
    "Score controlled live recorder/player");

  return &_cmRecdPlayDC;
}

//------------------------------------------------------------------------------------------------------------
//)
//( { label:cmDspGoertzel file_desc:"Goertzel tone detection filter" kw:[sunit] }
enum
{
  kHopFactGrId,
  kInGrId,
  kHzBaseGrId,
};

cmDspClass_t _cmGoertzelDC;

typedef struct
{
  cmDspInst_t inst;
  cmGoertzel* g;
  double      outPhs;
  unsigned    outBaseGrId;
  unsigned    chCnt;
} cmDspGoertzel_t;


cmDspInst_t*  _cmDspGoertzelAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{

  if( va_cnt !=3 )
  {
    cmDspClassErr(ctx,classPtr,kVarArgParseFailDspRC,"The 'Goertzel' constructor must have two arguments: a channel count and frequency array.");
    return NULL;
  }

  va_list vl1;
  va_copy(vl1,vl);

  unsigned hopFact     = va_arg(vl,unsigned);
  int      chCnt       = va_arg(vl,int);
  double*  hzV         = va_arg(vl,double*);
  unsigned outBaseGrId = kHzBaseGrId + chCnt;
  unsigned i;

  cmDspGoertzel_t* p = cmDspInstAllocV(cmDspGoertzel_t,ctx,classPtr,instSymId,id,storeSymId,1,vl1,
    1,         "hop", kHopFactGrId,  0,0, kInDsvFl   | kDoubleDsvFl,    "Hop factor",
    1,         "in",  kInGrId,       0,1, kInDsvFl   | kAudioBufDsvFl,  "Audio input",
    chCnt,     "hz",  kHzBaseGrId,   0,0, kInDsvFl   | kDoubleDsvFl,    "Hz input.",
    chCnt,     "out", outBaseGrId,   0,1, kOutDsvFl  | kDoubleDsvFl,    "Detector output",
    0 );

  va_end(vl1);


  p->outBaseGrId = outBaseGrId;
  p->chCnt       = chCnt;

  p->g = cmGoertzelAlloc(ctx->cmProcCtx, NULL, 0, NULL, 0,0,0,0 );
  cmDspSetDefaultUInt(ctx,&p->inst, kHopFactGrId, 0, cmMax(hopFact,1));

  for(i=0; i<chCnt; ++i)
    cmDspSetDefaultDouble(ctx,&p->inst, kHzBaseGrId+i, 0.0, hzV[i] );

  return &p->inst;
}

cmDspRC_t _cmDspGoertzelSetup( cmDspCtx_t* ctx, cmDspInst_t* inst )
{
  cmDspRC_t        rc         = kOkDspRC;
  cmDspGoertzel_t* p          = (cmDspGoertzel_t*)inst;
  unsigned         hopFact    = cmDspUInt(inst,kHopFactGrId);
  unsigned         procSmpCnt = cmDspAudioBufSmpCount(ctx,inst,kInGrId,0);
  unsigned         wndSmpCnt  = procSmpCnt * hopFact;
  double           fcHzV[ p->chCnt ];
  unsigned         i;

  for(i=0; i<p->chCnt; ++i)
  {
    double hz;
    if( p->g->ch == NULL || p->g->ch[i].hz == 0 )
      hz = cmDspDouble(inst,kHzBaseGrId);
    else
      hz = p->g->ch[i].hz;

    fcHzV[i] = hz;
  }
   
  if( cmGoertzelInit(p->g,cmDspSysSampleRate(ctx->dspH),fcHzV,p->chCnt,procSmpCnt,procSmpCnt,wndSmpCnt) != cmOkRC )
    rc = cmErrMsg(&inst->classPtr->err, kSubSysFailDspRC, "Unable to initialize the internal Goertzel detector.");

  return rc;
}

cmDspRC_t _cmDspGoertzelFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t        rc = kOkDspRC;
  cmDspGoertzel_t* p = (cmDspGoertzel_t*)inst;

  cmGoertzelFree(&p->g);

  return rc;
}

cmDspRC_t _cmDspGoertzelReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspGoertzel_t* p = (cmDspGoertzel_t*)inst;

  cmDspApplyAllDefaults(ctx,inst);

  p->outPhs = 0;

  return _cmDspGoertzelSetup(ctx, inst );
} 

cmDspRC_t _cmDspGoertzelExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t         rc        = kOkDspRC;
  cmDspGoertzel_t*  p         = (cmDspGoertzel_t*)inst;
  const cmSample_t* x         = cmDspAudioBuf(ctx,inst,kInGrId,0);
  unsigned          n         = cmDspAudioBufSmpCount(ctx,inst,kInGrId,0);
  double            outMs     = 50.0;
  double            outPhsMax = outMs * cmDspSysSampleRate(ctx->dspH)  / 1000.0;
  double            outV[ p->chCnt ];
  unsigned          i;

  if( x != NULL )
  {
    cmGoertzelExec(p->g,x,n,outV,p->chCnt);

    p->outPhs += n;
    if( p->outPhs > outPhsMax )
    {
      while( p->outPhs > outPhsMax )
        p->outPhs -= outPhsMax;

      for(i=0; i<p->chCnt; ++i)
      {
        cmDspSetDouble(ctx,inst,p->outBaseGrId+i,outV[i]);
        //printf("%f ",outV[i]);
      }
      //printf("\n");
    }
  }

  return rc;
}

cmDspRC_t _cmDspGoertzelRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspGoertzel_t*  p         = (cmDspGoertzel_t*)inst;

  cmDspSetEvent(ctx,inst,evt);

  if( kHzBaseGrId <= evt->dstVarId && evt->dstVarId < kHzBaseGrId+p->chCnt )
    cmGoertzelSetFcHz(p->g, evt->dstVarId - kHzBaseGrId, cmDspDouble(inst,evt->dstVarId));
  else
  {
    if( evt->dstVarId==kHopFactGrId )
    {
      _cmDspGoertzelSetup(ctx,inst);
    }
  }
  return kOkDspRC;
}

cmDspClass_t* cmGoertzelClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmGoertzelDC,ctx,"Goertzel",
    NULL,
    _cmDspGoertzelAlloc,
    _cmDspGoertzelFree,
    _cmDspGoertzelReset,
    _cmDspGoertzelExec,
    _cmDspGoertzelRecv,
    NULL,
    NULL,
    "Goertzel Tone Detector Filter");

  return &_cmGoertzelDC;
}

//------------------------------------------------------------------------------------------------------------
//)
//( { label:cmDspSyncRecd file_desc:"Time align a MIDI and associated audio recording" kw:[sunit] }

enum
{
  kRecdDirSrId,
  kSrFnSrId,
  kAfSrId,
  kBitsSrId,
  kCmdSrId,
  kStatusSrId,
  kD0SrId,
  kD1SrId,
  kSecSrId,
  kNSecSrId,
  kAinBaseSrId
};

cmDspClass_t _cmSyncRecdDC;

typedef struct
{
  cmDspInst_t     inst;
  unsigned        chCnt;
  cmTimeSpec_t    ats;
  cmSyncRecdH_t   srH;
  unsigned        openSymId;
  unsigned        closeSymId;
  const cmChar_t* aFn;
  const cmChar_t* srFn;
  unsigned        smpIdx;
} cmDspSyncRecd_t;

cmDspRC_t _cmDspSyncRecdCreateFile( cmDspCtx_t* ctx, cmDspInst_t* inst )
{
  cmDspSyncRecd_t* p = (cmDspSyncRecd_t*)inst;

  const cmChar_t* aFn  = cmDspStrcz(inst,kAfSrId);
  const cmChar_t* srFn = cmDspStrcz(inst,kSrFnSrId);
  const cmChar_t* dir  = cmDspStrcz(inst,kRecdDirSrId);

  if( !cmFsIsDir(dir) )
    return cmDspInstErr(ctx,&p->inst,kInvalidArgDspRC,"'%s' is not a valid directory.",cmStringNullGuard(dir));

  cmMemPtrFree(&p->aFn);
  if( cmFsGenFn(dir,aFn,"aiff",&p->aFn) != kOkFsRC )
    return cmDspInstErr(ctx,&p->inst,kFileSysFailDspRC,"Audio file name generation failed for dir='%s' and prefix='%s'.",cmStringNullGuard(dir),cmStringNullGuard(aFn));

  cmMemPtrFree(&p->srFn);
  if( cmFsGenFn(dir,srFn,"sr",&p->srFn) != kOkFsRC )
    return cmDspInstErr(ctx,&p->inst,kFileSysFailDspRC,"Sync-recd file name generation failed for dir='%s' and prefix='%s'.",cmStringNullGuard(dir),cmStringNullGuard(srFn));

  unsigned bits = cmDspUInt(inst,kBitsSrId);
  if( cmSyncRecdCreate(  ctx->cmCtx, &p->srH, p->srFn, p->aFn, cmDspSampleRate(ctx), p->chCnt, bits ) != kOkSyRC )
    return cmDspInstErr(ctx,&p->inst,kSubSysFailDspRC,"Sync-recd file create failed for '%s'.",p->srFn);

  p->smpIdx = 0;

  return kOkDspRC;
}

cmDspInst_t*  _cmDspSyncRecdAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspSyncRecd_t* p = cmDspInstAllocV(cmDspSyncRecd_t,ctx,classPtr,instSymId,id,storeSymId,va_cnt,vl,
    1,         "dir",    kRecdDirSrId,  0,0, kInDsvFl | kStrzDsvFl | kReqArgDsvFl, "Recording  directory.",
    1,         "srfn",   kSrFnSrId,     0,0, kInDsvFl | kStrzDsvFl | kReqArgDsvFl, "SyncRecd file prefix.",
    1,         "afn",    kAfSrId,       0,0, kInDsvFl | kStrzDsvFl | kReqArgDsvFl, "Audio file prefix.",
    1,         "bits",   kBitsSrId,     0,0, kInDsvFl | kUIntDsvFl | kOptArgDsvFl, "Audio file bits per sample.",
    1,         "cmd",    kCmdSrId,      0,0, kInDsvFl | kSymDsvFl,                 "Command: open | close",
    1,         "status", kStatusSrId,   0,0, kInDsvFl | kUIntDsvFl,                "MIDI status",
    1,         "d0",     kD0SrId,       0,0, kInDsvFl | kUIntDsvFl,                "MIDI d0",
    1,         "d1",     kD1SrId,       0,0, kInDsvFl | kUIntDsvFl,                "MIDI d1", 
    1,         "sec",    kSecSrId,      0,0, kInDsvFl | kUIntDsvFl,                "MIDI Timestamp Seconds",
    1,         "nsec",   kNSecSrId,     0,0, kInDsvFl | kUIntDsvFl,                "MIDI Timestamp Nanoseconds",
    2,         "ain",    kAinBaseSrId,  0,1, kInDsvFl | kAudioBufDsvFl,            "Audio Input",    
    0 );

  p->chCnt = 2;

  p->openSymId  = cmSymTblRegisterStaticSymbol(ctx->stH,"open");
  p->closeSymId = cmSymTblRegisterStaticSymbol(ctx->stH,"close");

  cmDspSetDefaultUInt(ctx,&p->inst,kBitsSrId,0,16);

  return &p->inst;
}


cmDspRC_t _cmDspSyncRecdFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t        rc = kOkDspRC;
  cmDspSyncRecd_t* p = (cmDspSyncRecd_t*)inst;

  cmMemPtrFree(&p->aFn);
  cmMemPtrFree(&p->srFn);
  cmSyncRecdFinal(&p->srH);

  return rc;
}

cmDspRC_t _cmDspSyncRecdReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t rc = kOkDspRC;

  cmDspApplyAllDefaults(ctx,inst);

  return rc;
} 

cmDspRC_t _cmDspSyncRecdExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{

  cmDspRC_t      rc = kOkDspRC;

  cmDspSyncRecd_t* p = (cmDspSyncRecd_t*)inst;

  const cmSample_t* x[ p->chCnt ];
  unsigned n = 0;
  unsigned i;

  //const cmTimeSpec_t* ts  = &ctx->ctx->oTimeStamp;
  //printf("SR: %ld %ld\n",ts->tv_sec,ts->tv_nsec);
  p->ats = ctx->ctx->iTimeStamp;

  for(i=0; i<p->chCnt; ++i)
  {
    if( i==0 )
      n  = cmDspAudioBufSmpCount(ctx,inst,kAinBaseSrId+i,0);
    else
    { assert( n == cmDspAudioBufSmpCount(ctx,inst,kAinBaseSrId+i,0)); }

    x[i] = cmDspAudioBuf(ctx,inst,kAinBaseSrId+i,0);
  }

  if( n>0 && cmSyncRecdIsValid(p->srH ) )
    if( cmSyncRecdAudioWrite( p->srH, &ctx->ctx->iTimeStamp, p->smpIdx, x, p->chCnt, n ) != kOkSyRC )
      return cmDspInstErr(ctx,&p->inst,kSubSysFailDspRC,"Sync-recd audio update failed.");

  p->smpIdx += n;

  return rc;
}

cmDspRC_t _cmDspSyncRecdRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t rc = kOkDspRC;
  cmDspSyncRecd_t*  p         = (cmDspSyncRecd_t*)inst;

  cmDspSetEvent(ctx,inst,evt);

  switch( evt->dstVarId )
  {
    case kStatusSrId:
      if(cmMidiIsChStatus( cmDspUInt(inst,kStatusSrId) ) )
      {
        cmTimeSpec_t ts;
        ts.tv_sec = cmDspUInt(inst,kSecSrId);
        ts.tv_nsec = cmDspUInt(inst,kNSecSrId);

        //printf("%i %i\n",cmDspUInt(inst,kD1SrId),cmTimeElapsedMicros(&ts,&p->ats));

        if( cmSyncRecdIsValid(p->srH ) )
          if( cmSyncRecdMidiWrite(p->srH, &ts, cmDspUInt(inst,kStatusSrId), cmDspUInt(inst,kD0SrId), cmDspUInt(inst,kD1SrId) ) != kOkSyRC )
            return cmDspInstErr(ctx,&p->inst,kSubSysFailDspRC,"Sync-recd MIDI update failed.");
      }
      break;

    case kCmdSrId:
      {
        unsigned cmdId = cmDspSymbol(inst,kCmdSrId);
        if( cmdId == p->openSymId )
          rc = _cmDspSyncRecdCreateFile(ctx,inst);
        else
          if( cmdId == p->closeSymId && cmSyncRecdIsValid(p->srH))
          {
            cmSyncRecdFinal(&p->srH);
            //cmSyncRecdTest(ctx->cmCtx);
            
          }
          
      }
      break;
  }

  return rc;
}

cmDspClass_t* cmSyncRecdClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmSyncRecdDC,ctx,"SyncRecd",
    NULL,
    _cmDspSyncRecdAlloc,
    _cmDspSyncRecdFree,
    _cmDspSyncRecdReset,
    _cmDspSyncRecdExec,
    _cmDspSyncRecdRecv,
    NULL,
    NULL,
    "Synchronized Audio and MIDI recorder.");

  return &_cmSyncRecdDC;
}


//------------------------------------------------------------------------------------------------------------
//)
//( { label:cmDspTakeSeqBldr file_desc:"User interface unit for creating a single sequence from multiple, score aligned, MIDI fragments." kw:[sunit] }

enum
{
  kFnTsbId,
  kBldrTsbId,
  kSelTsbId,
  kRefreshTsbId,
  kSendTsbId
};

cmDspClass_t _cmTakeSeqBldrDC;

typedef struct
{
  cmDspInst_t      inst;
  cmTakeSeqBldrH_t h;
  bool             errFl;
} cmDspTakeSeqBldr_t;


cmDspInst_t*  _cmDspTakeSeqBldrAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspTakeSeqBldr_t* p = cmDspInstAllocV(cmDspTakeSeqBldr_t,ctx,classPtr,instSymId,id,storeSymId,va_cnt,vl,
    1,       "fn",      kFnTsbId, 0, 0, kInDsvFl  | kStrzDsvFl | kReqArgDsvFl, "Score Tracking file.",
    1,      "bldr",   kBldrTsbId, 0, 0, kOutDsvFl | kPtrDsvFl, "Bldr Ref",
    1,       "sel",   kSelTsbId,  0, 0, kOutDsvFl | kUIntDsvFl,   "Selected score element location index.",
    1,   "refresh",kRefreshTsbId, 0, 0, kOutDsvFl | kUIntDsvFl,   "Refresh",
    1,      "send",   kSendTsbId, 0, 0, kInDsvFl  | kTypeDsvMask,  "Resend last selected score element location.",
    0 );

  p->errFl      = false;

  cmDspSetDefaultInt(   ctx, &p->inst,  kRefreshTsbId, 0, 0);

  if( cmTakeSeqBldrAlloc(ctx->cmCtx, &p->h ) != kOkTsbRC )
    cmErrMsg(&p->inst.classPtr->err, kSubSysFailDspRC, "Allocate TaskSeqBldr object.");
  else
  {
    cmDspUiTakeSeqBldrCreate(ctx,&p->inst,kFnTsbId,kBldrTsbId,kSelTsbId,kRefreshTsbId);
  }

  return &p->inst;
}

cmDspRC_t _cmDspTakeSeqBldrSetup( cmDspCtx_t* ctx, cmDspInst_t* inst )
{
  cmDspRC_t           rc = kOkDspRC;
  cmDspTakeSeqBldr_t* p  = (cmDspTakeSeqBldr_t*)inst;
  
  if( cmTakeSeqBldrInitialize(p->h, cmDspStrcz(inst,kFnTsbId) ) != kOkTsbRC )
  {
    rc = cmErrMsg(&inst->classPtr->err, kSubSysFailDspRC, "Unable to initialize the internal TakeSeqBldr object with %s.",cmStringNullGuard(cmDspStrcz(inst,kFnTsbId)));
  }
  else
  {
    cmDspSetPtr(ctx,inst,kBldrTsbId,p->h.h);
    p->errFl = false;
  }
  return rc;
}

cmDspRC_t _cmDspTakeSeqBldrFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t           rc = kOkDspRC;
  cmDspTakeSeqBldr_t* p  = (cmDspTakeSeqBldr_t*)inst;

  cmTakeSeqBldrFree(&p->h);

  return rc;
}

cmDspRC_t _cmDspTakeSeqBldrReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  //cmDspTakeSeqBldr_t* p = (cmDspTakeSeqBldr_t*)inst;

  cmDspApplyAllDefaults(ctx,inst);

  return _cmDspTakeSeqBldrSetup(ctx, inst );
} 

cmDspRC_t _cmDspTakeSeqBldrExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t            rc  = kOkDspRC;
  return rc;
}

cmDspRC_t _cmDspTakeSeqBldrRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{

  // not matter what arrives at the 'send' input ....
  if( evt->dstVarId == kSendTsbId )
  {
    // send the last score loc 
    unsigned selIdx;
    if((selIdx = cmDspUInt(inst,kSelTsbId)) != cmInvalidIdx )
      cmDspSetUInt(ctx,inst,kSelTsbId, selIdx );
    
    return kOkDspRC;
  }

  cmDspSetEvent(ctx,inst,evt);

  switch(evt->dstVarId)
  {
    case kFnTsbId:
      _cmDspMidiFilePlayOpen(ctx, inst );
      break;

  }
  return kOkDspRC;
}

cmDspClass_t* cmTakeSeqBldrClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmTakeSeqBldrDC,ctx,"TakeSeqBldr",
    NULL,
    _cmDspTakeSeqBldrAlloc,
    _cmDspTakeSeqBldrFree,
    _cmDspTakeSeqBldrReset,
    _cmDspTakeSeqBldrExec,
    _cmDspTakeSeqBldrRecv,
    NULL,
    NULL,
    "TakeSeqBldr");

  return &_cmTakeSeqBldrDC;
}




//------------------------------------------------------------------------------------------------------------
//)
//( { label:cmDspTakeSeqRend file_desc:"User interface unit for graphically rendering the MIDI sequences created by cmDspTakeSeqBldr." kw:[sunit] }
enum
{
  kBldrTsrId,
  kRefreshTsrId,
  kCmdTsrId,
  kSelTsrId,
  kStatusTsrId,
  kD0TsrId,
  kD1TsrId,
  kSmpIdxTsrId
};

cmDspClass_t _cmTakeSeqRendDC;

typedef struct
{
  cmDspInst_t      inst;
  cmTakeSeqBldrH_t h;
  unsigned         startSymId;
  unsigned         stopSymId;
  unsigned         contSymId;
  unsigned         onSymId;
  unsigned         offSymId;
  bool             errFl;
} cmDspTakeSeqRend_t;


cmDspInst_t*  _cmDspTakeSeqRendAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspTakeSeqRend_t* p = cmDspInstAllocV(cmDspTakeSeqRend_t,ctx,classPtr,instSymId,id,storeSymId,va_cnt,vl,
    1,      "bldr",   kBldrTsrId, 0, 0, kInDsvFl  | kPtrDsvFl  | kOptArgDsvFl, "Take Sequene Builder Ref",
    1,   "refresh",kRefreshTsrId, 0, 0, kInDsvFl  | kUIntDsvFl | kOptArgDsvFl, "Refresh",
    1,       "cmd",    kCmdTsrId, 0, 0, kInDsvFl  | kSymDsvFl,  "start | stop | continue" ,
    1,       "sel",    kSelTsrId, 0, 0, kInDsvFl  | kUIntDsvFl, "Selected score element location index input.",
    1,    "status", kStatusTsrId, 0, 0, kOutDsvFl | kIntDsvFl,  "Status value output",
    1,       "d0",      kD0TsrId, 0, 0, kOutDsvFl | kUIntDsvFl, "Data byte 0" ,
    1,       "d1",      kD1TsrId, 0, 0, kOutDsvFl | kUIntDsvFl, "Data byte 1",
    1,    "smpidx", kSmpIdxTsrId, 0, 0, kOutDsvFl | kUIntDsvFl, "Msg time tag as a sample index.",
    0 );

  p->startSymId = cmSymTblRegisterStaticSymbol(ctx->stH,"start");
  p->stopSymId  = cmSymTblRegisterStaticSymbol(ctx->stH,"stop");
  p->contSymId  = cmSymTblRegisterStaticSymbol(ctx->stH,"continue");
  p->onSymId    = cmSymTblRegisterStaticSymbol(ctx->stH,"on");
  p->offSymId   = cmSymTblRegisterStaticSymbol(ctx->stH,"off");

  p->errFl      = false;

  cmDspSetDefaultInt(   ctx, &p->inst,  kRefreshTsrId, 0, 0);
  cmDspSetDefaultSymbol(ctx, &p->inst,  kCmdTsrId,    p->stopSymId);
  cmDspSetDefaultInt(   ctx, &p->inst,  kSmpIdxTsrId, 0, 0);
  cmDspSetDefaultUInt(  ctx, &p->inst,  kStatusTsrId, 0, 0);
  cmDspSetDefaultUInt(  ctx, &p->inst,  kD0TsrId,     0, 0);
  cmDspSetDefaultUInt(  ctx, &p->inst,  kD1TsrId,     0, 0);

  cmDspUiTakeSeqRendCreate(ctx,&p->inst,kBldrTsrId,kRefreshTsrId,kSelTsrId);

  return &p->inst;
}

cmDspRC_t _cmDspTakeSeqRendFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t           rc = kOkDspRC;
  return rc;
}

cmDspRC_t _cmDspTakeSeqRendReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  //cmDspTakeSeqRend_t* p = (cmDspTakeSeqRend_t*)inst;

  cmDspApplyAllDefaults(ctx,inst);
  return kOkDspRC;
} 

typedef struct
{
  cmDspCtx_t*  ctx;
  cmDspInst_t* inst;
} _cmDspTakeSeqRendCbArg_t;

// Called from cmDspTakeSeqRendExec() -> cmTakeSeqRendPlayExec() to
// deliver MIDI messages which need to be transmitted.
void _cmDspTakeSeqRendMidiCb( void* arg, const cmTksbEvent_t* e )
{
  _cmDspTakeSeqRendCbArg_t* a = (_cmDspTakeSeqRendCbArg_t*)arg;

  switch( e->status )
  {
    case kNoteOffMdId:
    case kNoteOnMdId:
    case kCtlMdId:
      //if( !cmMidiIsPedal(e->status,e->d0))
      {
        cmDspSetUInt(a->ctx,a->inst, kSmpIdxTsrId, e->smpIdx);
        cmDspSetUInt(a->ctx,a->inst, kD1TsrId,     e->d1);
        cmDspSetUInt(a->ctx,a->inst, kD0TsrId,     e->d0);
        cmDspSetUInt(a->ctx,a->inst, kStatusTsrId, e->status);
      }
      break;
  }
  
}

void _cmDspTakeSeqRendPedalsUp( cmDspCtx_t* ctx, cmDspInst_t* inst )
{
  _cmDspTakeSeqRendCbArg_t a;
  a.ctx = ctx;
  a.inst = inst;

  cmTksbEvent_t e[] =
  {
    { 0, kCtlMdId, kSustainCtlMdId,   0 },
    { 0, kCtlMdId, kPortamentoCtlMdId,0 },
    { 0, kCtlMdId, kSostenutoCtlMdId, 0 },
    { 0, kCtlMdId, kSoftPedalCtlMdId, 0 },
    { 0, kCtlMdId, kLegatoCtlMdId,    0 }
  };

  unsigned n = sizeof(e)/sizeof(e[0]);
  unsigned i;
  for(i=0; i<n; ++i)
    _cmDspTakeSeqRendMidiCb(&a,e+i);
}


cmDspRC_t _cmDspTakeSeqRendExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t            rc  = kOkDspRC;
  cmDspTakeSeqRend_t*  p   = (cmDspTakeSeqRend_t*)inst;

  if( cmDspSymbol(inst,kCmdTsrId) != p->stopSymId )
  {
    if( cmTakeSeqBldrIsValid(p->h) == false )
    {
      if( p->errFl==false )
      {
        rc = cmErrMsg(&inst->classPtr->err, kInvalidStateDspRC,"The Take Sequence Builder not been given a valid file.");
        p->errFl = true;
      }
      return rc;
    }

    unsigned                 sPc = cmDspSamplesPerCycle(ctx);
    _cmDspTakeSeqRendCbArg_t arg;
    arg.inst = inst;
    arg.ctx  = ctx;

    // This call may result in multiple callbacks  
    // to _cmDspTakeSeqRendMidiCb() from within the function.
    cmTakeSeqBldrPlayExec(p->h,sPc,_cmDspTakeSeqRendMidiCb, &arg );
  }

  return rc;
}


cmDspRC_t _cmDspTakeSeqRendRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspTakeSeqRend_t* p = (cmDspTakeSeqRend_t*)inst;
  
  printf("Recv:%s\n",cmStringNullGuard(cmDspVarLabel(ctx, inst, evt->dstVarId)) );
  
  cmDspSetEvent(ctx,inst,evt);

  switch(evt->dstVarId)
  {
    case kBldrTsrId:
      p->h.h = cmDspPtr(inst,kBldrTsrId);
      break;

    case kCmdTsrId:
      {
        unsigned symId = cmDspSymbol(inst,kCmdTsrId);

        if( symId == p->onSymId )
          cmDspSetSymbol( ctx, inst, kCmdTsrId, p->startSymId );
        else
          if( symId == p->offSymId )
            cmDspSetSymbol( ctx, inst, kCmdTsrId, p->stopSymId );

        if( cmTakeSeqBldrIsValid(p->h) && cmDspSymbol(inst,kCmdTsrId)==p->startSymId ) 
        {
          _cmDspTakeSeqRendPedalsUp( ctx, inst );
          cmTakeSeqBldrPlaySeekLoc(p->h, cmInvalidId );
        }
      }
      break;

    case kSelTsrId:
      {
        // seek the playback position to the scLocIdx.
        unsigned scLocIdx = cmDspUInt(inst,kSelTsrId);        
        if( cmTakeSeqBldrIsValid(p->h)  && cmTakeSeqBldrPlaySeekLoc(p->h, scLocIdx ) != kOkTsbRC )
          return cmDspInstErr(ctx,&p->inst,kSubSysFailDspRC,"Take Sequence Bldr Seek failed on score location index %i.", scLocIdx);
      }
      break;

  }
  return kOkDspRC;
}

cmDspClass_t* cmTakeSeqRendClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmTakeSeqRendDC,ctx,"TakeSeqRend",
    NULL,
    _cmDspTakeSeqRendAlloc,
    _cmDspTakeSeqRendFree,
    _cmDspTakeSeqRendReset,
    _cmDspTakeSeqRendExec,
    _cmDspTakeSeqRendRecv,
    NULL,
    NULL,
    "TakeSeqRend");

  return &_cmTakeSeqRendDC;
}


//------------------------------------------------------------------------------------------------------------
//)
//( { label:cmDspReflectCalc file_desc:"Estimate the time-of-flight of from an acoustic signal from a speaker to a microphone." kw:[sunit] }
enum
{
  kLfsrN_RcId,
  kMlsCoeff0RcId,
  kMlsCoeff1RcId,
  kSmpPerChipRcId,
  kRcosBetaRcId,
  kRcosOS_RcId,
  kCarrierHzRcId,
  kAtkDcyMsRcId,
  kPhatAlphaRcId,
  kPhatMultRcId,
  kInRcId,
  kOutRcId,
};

cmDspClass_t _cmReflectCalcDC;

typedef struct
{
  cmDspInst_t    inst;
  cmReflectCalc_t* r;
} cmDspReflectCalc_t;


cmDspInst_t*  _cmDspReflectCalcAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{

  //  if( va_cnt !=3 )
  //  {
  //    cmDspClassErr(ctx,classPtr,kVarArgParseFailDspRC,"The 'ReflectCalc' constructor must have two arguments: a channel count and frequency array.");
  //    return NULL;
  //  }

  cmDspReflectCalc_t* p = cmDspInstAllocV(cmDspReflectCalc_t,ctx,classPtr,instSymId,id,storeSymId,va_cnt,vl,
    1,         "lfsrN", kLfsrN_RcId,    0,0, kInDsvFl   | kUIntDsvFl,    "Gold code generator LFSR length",
    1,         "mlsc0", kMlsCoeff0RcId, 0,0, kInDsvFl   | kUIntDsvFl,    "LFSR coefficient 0",
    1,         "mlsc1", kMlsCoeff1RcId, 0,0, kInDsvFl   | kUIntDsvFl,    "LFSR coefficient 0",
    1,         "spchip",kSmpPerChipRcId,0,0, kInDsvFl   | kUIntDsvFl,    "Samples per spreading code bit.",
    1,         "rcosb", kRcosBetaRcId,  0,0, kInDsvFl   | kDoubleDsvFl,  "Raised cosine beta",
    1,         "rcosos",kRcosOS_RcId,   0,0, kInDsvFl   | kUIntDsvFl,    "Raised cosine oversample factor.",
    1,         "carhz", kCarrierHzRcId, 0,0, kInDsvFl   | kDoubleDsvFl,  "Carrier frequency in Hertz.",
    1,         "envms", kAtkDcyMsRcId,  0,0, kInDsvFl   | kDoubleDsvFl,  "Signal Attack/Decay milliseconds.",
    1,         "alpha", kPhatAlphaRcId, 0,0, kInDsvFl   | kDoubleDsvFl,  "PHAT alpha coefficient.",
    1,         "mult",  kPhatMultRcId,  0,0, kInDsvFl   | kUIntDsvFl,    "PHAT multiplier coefficient.",
    1,          "in",   kInRcId,        0,1, kInDsvFl   | kAudioBufDsvFl,"Audio input",
    1,          "out",  kOutRcId,       0,1, kOutDsvFl  | kAudioBufDsvFl,"Audio output",
    0 );


p->r = cmReflectCalcAlloc(ctx->cmProcCtx, NULL, NULL, 0, 0 );
  
  cmDspSetDefaultUInt(   ctx, &p->inst, kLfsrN_RcId,     0,  8);
  cmDspSetDefaultUInt(   ctx, &p->inst, kMlsCoeff0RcId,  0, 0x8e);
  cmDspSetDefaultUInt(   ctx, &p->inst, kMlsCoeff1RcId,  0, 0x96);
  cmDspSetDefaultUInt(   ctx, &p->inst, kSmpPerChipRcId, 0, 64);
  cmDspSetDefaultDouble( ctx, &p->inst, kRcosBetaRcId,   0, 0.5);
  cmDspSetDefaultUInt(   ctx, &p->inst, kRcosOS_RcId,    0, 4);
  cmDspSetDefaultDouble( ctx, &p->inst, kCarrierHzRcId,  0, 2500.0);
  cmDspSetDefaultDouble( ctx, &p->inst, kAtkDcyMsRcId,   0, 50.0);
  cmDspSetDefaultDouble( ctx, &p->inst, kPhatAlphaRcId,  0, 0.5);
  cmDspSetDefaultUInt(   ctx, &p->inst, kPhatMultRcId,   0, 1);

  return &p->inst;
}

cmDspRC_t _cmDspReflectCalcSetup( cmDspCtx_t* ctx, cmDspInst_t* inst )
{
  cmDspRC_t   rc = kOkDspRC;
  cmDspReflectCalc_t* p  = (cmDspReflectCalc_t*)inst;
  cmGoldSigArg_t gsa;

  gsa.chN            = 1;
  gsa.srate          = cmDspSampleRate(ctx);
  gsa.lfsrN          = cmDspUInt(inst,kLfsrN_RcId);
  gsa.mlsCoeff0      = cmDspUInt(inst,kMlsCoeff0RcId);
  gsa.mlsCoeff1      = cmDspUInt(inst,kMlsCoeff1RcId);
  gsa.samplesPerChip = cmDspUInt(inst,kSmpPerChipRcId);
  gsa.rcosBeta       = cmDspDouble(inst,kRcosBetaRcId);
  gsa.rcosOSFact     = cmDspUInt(inst,kRcosOS_RcId);
  gsa.carrierHz      = cmDspDouble(inst,kCarrierHzRcId);
  gsa.envMs          = cmDspDouble(inst,kAtkDcyMsRcId);

  double   phatAlpha = cmDspDouble(inst,kPhatAlphaRcId);
  unsigned phatMult  = cmDspUInt(inst,kPhatMultRcId);

  if( cmReflectCalcInit(p->r,&gsa,phatAlpha,phatMult) != cmOkRC )
    rc = cmErrMsg(&inst->classPtr->err, kSubSysFailDspRC, "Unable to initialize the internal ReflectCalc detector.");

  return rc;
}

cmDspRC_t _cmDspReflectCalcFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t           rc = kOkDspRC;
  cmDspReflectCalc_t* p  = (cmDspReflectCalc_t*)inst;

  cmReflectCalcFree(&p->r);

  return rc;
}

cmDspRC_t _cmDspReflectCalcReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
//cmDspReflectCalc_t* p = (cmDspReflectCalc_t*)inst;

  cmDspApplyAllDefaults(ctx,inst);

  return _cmDspReflectCalcSetup(ctx, inst );
} 

cmDspRC_t _cmDspReflectCalcExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t           rc = kOkDspRC;
  cmDspReflectCalc_t* p  = (cmDspReflectCalc_t*)inst;
  const cmSample_t*   xV = cmDspAudioBuf(ctx,inst,kInRcId,0);
  unsigned            xN = cmDspAudioBufSmpCount(ctx,inst,kInRcId,0);
  cmSample_t*         yV = cmDspAudioBuf(ctx,inst,kOutRcId,0);
  unsigned            yN = cmDspAudioBufSmpCount(ctx,inst,kOutRcId,0);

  if( xV != NULL && yV != NULL )
  {
    assert( xN == yN );
    cmReflectCalcExec(p->r,xV,yV,xN);
  }
  
  return rc;
}

cmDspRC_t _cmDspReflectCalcRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
//cmDspReflectCalc_t*  p         = (cmDspReflectCalc_t*)inst;

  cmDspSetEvent(ctx,inst,evt);

  return kOkDspRC;
}

cmDspClass_t* cmReflectCalcClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmReflectCalcDC,ctx,"ReflectCalc",
    NULL,
    _cmDspReflectCalcAlloc,
    _cmDspReflectCalcFree,
    _cmDspReflectCalcReset,
    _cmDspReflectCalcExec,
    _cmDspReflectCalcRecv,
    NULL,
    NULL,
    "Reflecttion time calculator");

  return &_cmReflectCalcDC;
}


//------------------------------------------------------------------------------------------------------------
//)
//( { label:cmDspEchoCancel file_desc:"Normalized least mean squares echo canceller." kw:[sunit] }
enum
{
  kMuEcId,
  kImpRespN_EcId,
  kDelayN_EcId,
  kBypassEcId,
  kUnfiltInEcId,
  kFiltInEcId,
  kOutEcId
};

cmDspClass_t _cmEchoCancelDC;

typedef struct
{
  cmDspInst_t    inst;
  cmNlmsEc_t*    r;
} cmDspEchoCancel_t;


cmDspInst_t*  _cmDspEchoCancelAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{

  //  if( va_cnt !=3 )
  //  {
  //    cmDspClassErr(ctx,classPtr,kVarArgParseFailDspRC,"The 'EchoCancel' constructor must have two arguments: a channel count and frequency array.");
  //    return NULL;
  //  }

  cmDspEchoCancel_t* p = cmDspInstAllocV(cmDspEchoCancel_t,ctx,classPtr,instSymId,id,storeSymId,va_cnt,vl,
    1,   "mu",     kMuEcId,        0,0, kInDsvFl   | kDoubleDsvFl,   "NLSM mu coefficient.",
    1,   "irN",    kImpRespN_EcId, 0,0, kInDsvFl   | kUIntDsvFl,     "Filter impulse response length in samples.",
    1,   "delayN", kDelayN_EcId,   0,0, kInDsvFl   | kUIntDsvFl,     "Fixed feedback delay in samples.",
    1,   "bypass", kBypassEcId,    0,0, kInDsvFl   | kBoolDsvFl,     "Bypass enable flag.",
    1,   "uf_in",  kUnfiltInEcId,  0,1, kInDsvFl   | kAudioBufDsvFl, "Unfiltered audio input",
    1,   "f_in",   kFiltInEcId,    0,1, kInDsvFl   | kAudioBufDsvFl, "Filtered audio input",
    1,   "out",    kOutEcId,       0,1, kOutDsvFl  | kAudioBufDsvFl, "Audio output",
    0 );


  p->r = cmNlmsEcAlloc(ctx->cmProcCtx, NULL, 0, 0,0,0 );
  
  cmDspSetDefaultDouble( ctx, &p->inst, kMuEcId,        0, 0.1);
  cmDspSetDefaultUInt(   ctx, &p->inst, kImpRespN_EcId, 0, 2048);
  cmDspSetDefaultUInt(   ctx, &p->inst, kDelayN_EcId,   0, 1765);
  cmDspSetDefaultBool(   ctx, &p->inst, kBypassEcId,    0, false);

  return &p->inst;
}

cmDspRC_t _cmDspEchoCancelSetup( cmDspCtx_t* ctx, cmDspInst_t* inst )
{
  cmDspRC_t          rc     = kOkDspRC;
  cmDspEchoCancel_t* p      = (cmDspEchoCancel_t*)inst;
  double             mu     = cmDspDouble(inst,kMuEcId);
  unsigned           hN     = cmDspUInt(inst,kImpRespN_EcId);
  unsigned           delayN = cmDspUInt(inst,kDelayN_EcId);

  if( cmNlmsEcInit(p->r,cmDspSampleRate(ctx),mu,hN,delayN) != cmOkRC )
    rc = cmErrMsg(&inst->classPtr->err, kSubSysFailDspRC, "Unable to initialize the internal echo canceller.");

  return rc;
}

cmDspRC_t _cmDspEchoCancelFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t           rc = kOkDspRC;
  cmDspEchoCancel_t* p  = (cmDspEchoCancel_t*)inst;

  cmNlmsEcWrite(p->r, "/Users/kevin/temp/kc");
  
  cmNlmsEcFree(&p->r);

  return rc;
}

cmDspRC_t _cmDspEchoCancelReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
//cmDspEchoCancel_t* p = (cmDspEchoCancel_t*)inst;

  cmDspApplyAllDefaults(ctx,inst);

  return _cmDspEchoCancelSetup(ctx, inst );
} 

cmDspRC_t _cmDspEchoCancelExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t          rc       = kOkDspRC;
  cmDspEchoCancel_t* p        = (cmDspEchoCancel_t*)inst;
  bool               bypassFl = true; //cmDspBool(inst,kBypassEcId);
  
  const cmSample_t*   fV = cmDspAudioBuf(ctx,inst,kFiltInEcId,0);
  unsigned            fN = cmDspAudioBufSmpCount(ctx,inst,kFiltInEcId,0);
  
  const cmSample_t*   uV = cmDspAudioBuf(ctx,inst,kUnfiltInEcId,0);
  unsigned            uN = cmDspAudioBufSmpCount(ctx,inst,kUnfiltInEcId,0);
  
  cmSample_t*         yV = cmDspAudioBuf(ctx,inst,kOutEcId,0);
  unsigned            yN = cmDspAudioBufSmpCount(ctx,inst,kOutEcId,0);

  assert( fN==uN && fN==yN );
  
  if( bypassFl )
  {
    cmVOS_Copy(yV,yN,uV);
  }
  else
  {
    if( fV !=NULL && uV != NULL && yV != NULL )
    {
      assert( uN == yN && fN == yN );
      cmNlmsEcExec(p->r,uV,fV,yV,yN);
    }
  }
  
  return rc;
}

cmDspRC_t _cmDspEchoCancelRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspEchoCancel_t*  p = (cmDspEchoCancel_t*)inst;

  cmDspSetEvent(ctx,inst,evt);

  if( p->r != NULL )
  {
    switch(evt->dstVarId)
    {
      case kMuEcId:
        cmNlmsEcSetMu( p->r, cmDspReal(inst,kMuEcId));
        break;
        
      case kImpRespN_EcId:
        cmNlmsEcSetIrN( p->r, cmDspUInt(inst,kImpRespN_EcId));
        break;
      
      case kDelayN_EcId:
        cmNlmsEcSetDelayN( p->r, cmDspUInt(inst,kDelayN_EcId));
        break;

      case kBypassEcId:
        printf("EC bypass:%i\n",cmDspBool(inst,kBypassEcId));
        break;
    }
  }

  printf("mu:%f dN:%i hN:%i\n",p->r->mu,p->r->delayN,p->r->hN);
  
  return kOkDspRC;
}

cmDspClass_t* cmEchoCancelClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmEchoCancelDC,ctx,"EchoCancel",
    NULL,
    _cmDspEchoCancelAlloc,
    _cmDspEchoCancelFree,
    _cmDspEchoCancelReset,
    _cmDspEchoCancelExec,
    _cmDspEchoCancelRecv,
    NULL,
    NULL,
    "Echo canceller");

  return &_cmEchoCancelDC;
}
//)
