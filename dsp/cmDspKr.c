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
// Time Line UI Object

enum
{
  kTlFileTlId,
  kAudPathTlId,
  kSelTlId,
  kCursTlId,
  kResetTlId,
  kAudFnTlId,
  kMidiFnTlId,
  kBegAudSmpIdxTlId,
  kEndAudSmpIdxTlId,
  kBegMidiSmpIdxTlId,
  kEndMidiSmpIdxTlId
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
    { "path",    kAudPathTlId,        0, 0, kInDsvFl   | kStrzDsvFl | kReqArgDsvFl, "Audio path"    },
    { "sel",     kSelTlId,            0, 0, kInDsvFl   | kOutDsvFl  | kUIntDsvFl,   "Selected marker id."},
    { "curs",    kCursTlId,           0, 0, kInDsvFl   | kUIntDsvFl,  "Current audio file index."},
    { "reset",   kResetTlId,          0, 0, kInDsvFl   | kSymDsvFl,   "Resend all outputs." },
    { "afn",     kAudFnTlId,          0, 0, kOutDsvFl  | kStrzDsvFl,  "Selected Audio file." },
    { "mfn",     kMidiFnTlId,         0, 0, kOutDsvFl  | kStrzDsvFl,  "Selected MIDI file." },
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
  cmDspSetDefaultStrcz(ctx, &p->inst,  kMidiFnTlId,        NULL, "");
  cmDspSetDefaultInt(  ctx, &p->inst,  kBegAudSmpIdxTlId,  0, cmInvalidIdx);
  cmDspSetDefaultInt(  ctx, &p->inst,  kEndAudSmpIdxTlId,  0, cmInvalidIdx);
  cmDspSetDefaultInt(  ctx, &p->inst,  kBegMidiSmpIdxTlId, 0, cmInvalidIdx);
  cmDspSetDefaultInt(  ctx, &p->inst,  kEndMidiSmpIdxTlId, 0, cmInvalidIdx);

  // create the UI control
  cmDspUiTimeLineCreate(ctx,&p->inst,kTlFileTlId,kAudPathTlId,kSelTlId,kCursTlId);

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
  cmDspTimeLine_t* p  = (cmDspTimeLine_t*)inst;

  switch( evt->dstVarId )
  {
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
            assert(op->typeId == kMarkerTlId);

            p->afIdx = op->begSmpIdx;

            cmDspSetInt(ctx, inst, kBegAudSmpIdxTlId,  op->begSmpIdx );
            cmDspSetInt(ctx, inst, kEndAudSmpIdxTlId,  op->begSmpIdx + op->durSmpCnt );
            
            // locate the audio file assoc'd with the marker
            cmTlAudioFile_t* afp;
            if((afp = cmTimeLineAudioFileAtTime(p->tlH,op->seqId,op->seqSmpIdx)) != NULL)
              cmDspSetStrcz(ctx, inst, kAudFnTlId, afp->fn );

            // locate the midi file assoc'd with the marker
            cmTlMidiFile_t* mfp;
            if((mfp = cmTimeLineMidiFileAtTime(p->tlH,op->seqId,op->seqSmpIdx)) != NULL )
            {
              cmDspSetInt(ctx, inst, kBegMidiSmpIdxTlId, op->seqSmpIdx - mfp->obj.seqSmpIdx );
              cmDspSetInt(ctx, inst, kEndMidiSmpIdxTlId, op->seqSmpIdx + op->durSmpCnt - mfp->obj.seqSmpIdx );

              cmDspSetStrcz(ctx, inst, kMidiFnTlId, mfp->fn );
            }
          }
          
        }
        
      }
      
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

//==========================================================================================================================================
// Score UI Object

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
  kEvtIdxScId,
  kDynScId,
  kValTypeScId,
  kValueScId
};

cmDspClass_t _cmScoreDC;

typedef struct
{
  cmDspInst_t inst;
  cmScH_t     scH;
  cmDspCtx_t* ctx;   // temporary ctx ptr used during cmScore callback in _cmDspScoreRecv()
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
    { "evtidx",  kEvtIdxScId, 0, 0, kOutDsvFl | kUIntDsvFl,                "Performed event index of following dynamcis level."},
    { "dyn",     kDynScId,    0, 0, kOutDsvFl | kUIntDsvFl,                "Dynamic level of previous event index."},
    { "type",    kValTypeScId,0, 0, kOutDsvFl | kUIntDsvFl,                "Output variable type."},
    { "value",   kValueScId,  0, 0, kOutDsvFl | kDoubleDsvFl,              "Output variable value."},
    { NULL, 0, 0, 0, 0 }
  };

  cmDspScore_t* p = cmDspInstAlloc(cmDspScore_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);
  
  cmDspSetDefaultUInt( ctx, &p->inst,  kSelScId,           0, cmInvalidId);

  // create the UI control
  cmDspUiScoreCreate(ctx,&p->inst,kFnScId,kSelScId,kSmpIdxScId,kD0ScId,kD1ScId,kLocIdxScId,kEvtIdxScId,kDynScId,kValTypeScId,kValueScId);

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
    if( cmScoreInitialize(ctx->cmCtx, &p->scH, tlFn, cmDspSampleRate(ctx), dynRefArray, dynRefCnt, _cmDspScoreCb, p, cmSymTblNullHandle ) != kOkTlRC )
      rc = cmErrMsg(&inst->classPtr->err, kInstResetFailDspRC, "Score file open failed.");

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

  }

  return kOkDspRC;
}

struct cmDspClass_str* cmScoreClassCons( cmDspCtx_t* ctx )
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

//==========================================================================================================================================
// MIDI File Player

enum
{
  kFnMfId,
  kSelMfId,    
  kBsiMfId,
  kEsiMfId,
  kStatusMfId,
  kD0MfId,
  kD1MfId,
  kSmpIdxMfId
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

/*
  'bsi' and 'esi' give the starting and ending sample for MIDI file playback.
  These indexes are relative to the start of the file.
  When the player recieves a 'start' msg it sets the current sample index
  'si' to 'bsi' and begins scanning for the next note to play.  
  On each call to the _cmDspMidiFilePlayExec() msgs that fall in the interval
  si:si+sPc-1 will be transmitted.  (where sPc are the number of samples per DSP cycle).
 */

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
    if( a[i]->dtick > smpIdx )
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

  if( cmMidiFileOpen( fn, &p->mfH, ctx->cmCtx ) != kOkFileRC )
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
    cmMidiFileTickToSamples(p->mfH,cmDspSampleRate(ctx),true);

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
  
    for(; p->curMsgIdx < msgN && p->csi <= mpp[p->curMsgIdx]->dtick  &&  mpp[p->curMsgIdx]->dtick < (p->csi + sPc); ++p->curMsgIdx )
    {
      const cmMidiTrackMsg_t* mp = mpp[p->curMsgIdx];
      switch( mp->status )
      {
        case kNoteOnMdId:
        case kCtlMdId:
          cmDspSetUInt(ctx,inst, kSmpIdxMfId, mp->dtick);
          cmDspSetUInt(ctx,inst, kD1MfId,     mp->u.chMsgPtr->d1);
          cmDspSetUInt(ctx,inst, kD0MfId,     mp->u.chMsgPtr->d0);
          cmDspSetUInt(ctx,inst, kStatusMfId, mp->status);
          break;
      }
    }
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

struct cmDspClass_str* cmMidiFilePlayClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmMidiFilePlayDC,ctx,"MidiFilePlay",
    NULL,
    _cmDspMidiFilePlayAlloc,
    _cmDspMidiFilePlayFree,
    _cmDspMidiFilePlayReset,
    _cmDspMidiFilePlayExec,
    _cmDspMidiFilePlayRecv,
    NULL,NULL,
    "Time tagged text file.");

  return &_cmMidiFilePlayDC;
}

//==========================================================================================================================================
enum
{
  kFnSfId,
  kBufCntSfId,
  kMinLkAhdSfId,
  kMaxWndCntSfId,
  kMinVelSfId,
  kIndexSfId,
  kStatusSfId,
  kD0SfId,
  kD1SfId,
  kSmpIdxSfId,
  kCmdSfId,
  kOutSfId,
  kDynSfId,
  kEvenSfId,
  kTempoSfId,
  kCostSfId,
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
} cmDspScFol_t;

cmDspInst_t*  _cmDspScFolAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "fn",    kFnSfId,       0, 0, kInDsvFl | kStrzDsvFl   | kReqArgDsvFl,   "Score file." },
    { "bufcnt",kBufCntSfId,   0, 0, kInDsvFl | kUIntDsvFl,                    "Event buffer element count." },
    { "lkahd", kMinLkAhdSfId, 0, 0, kInDsvFl | kUIntDsvFl,                    "Minimum window look-ahead."},
    { "wndcnt",kMaxWndCntSfId,0, 0, kInDsvFl | kUIntDsvFl,                    "Maximum window length."},
    { "minvel",kMinVelSfId,   0, 0, kInDsvFl | kUIntDsvFl,                    "Minimum velocity."},
    { "index", kIndexSfId,    0, 0, kInDsvFl | kUIntDsvFl,                    "Tracking start location."},
    { "status",kStatusSfId,   0, 0, kInDsvFl | kUIntDsvFl,                    "MIDI status byte"},
    { "d0",    kD0SfId,       0, 0, kInDsvFl | kUIntDsvFl,                    "MIDI data byte 0"},
    { "d1",    kD1SfId,       0, 0, kInDsvFl | kUIntDsvFl,                    "MIDI data byte 1"},
    { "smpidx",kSmpIdxSfId,   0, 0, kInDsvFl | kUIntDsvFl,                    "MIDI time tag as a sample index"},
    { "cmd",   kCmdSfId,      0, 0, kInDsvFl | kSymDsvFl,                     "Command input: print | quiet"},
    { "out",   kOutSfId,      0, 0, kOutDsvFl| kUIntDsvFl,                    "Current score index."},
    { "dyn",   kDynSfId,      0, 0, kOutDsvFl| kDoubleDsvFl,                  "Dynamic value."},
    { "even",  kEvenSfId,     0, 0, kOutDsvFl| kDoubleDsvFl,                  "Evenness value."},
    { "tempo", kTempoSfId,    0, 0, kOutDsvFl| kDoubleDsvFl,                  "Tempo value."},
    { "cost",  kCostSfId,     0, 0, kOutDsvFl| kDoubleDsvFl,                  "Match cost value."},
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

  cmDspSetDefaultUInt(   ctx, &p->inst,  kBufCntSfId,     0,     7);
  cmDspSetDefaultUInt(   ctx, &p->inst,  kMaxWndCntSfId,  0,    10);
  cmDspSetDefaultUInt(   ctx, &p->inst,  kMinLkAhdSfId,   0,     3);
  cmDspSetDefaultUInt(   ctx, &p->inst,  kMinVelSfId,     0,     5);
  cmDspSetDefaultUInt(   ctx, &p->inst,  kIndexSfId,      0,     0);  
  cmDspSetDefaultUInt(   ctx, &p->inst,  kOutSfId,        0,     0);
  cmDspSetDefaultDouble( ctx, &p->inst,  kDynSfId,        0,     0);
  cmDspSetDefaultDouble( ctx, &p->inst,  kEvenSfId,       0,     0);
  cmDspSetDefaultDouble( ctx, &p->inst,  kTempoSfId,      0,     0);
  cmDspSetDefaultDouble( ctx, &p->inst,  kCostSfId,       0,     0);
  
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
    {

      switch( ap->sfp->smp->set[i].sp->varId )
      {
        case kEvenVarScId:
          cmDspSetDouble(ap->ctx,inst,kEvenSfId,ap->sfp->smp->set[i].value);
          break;

        case kDynVarScId:
          cmDspSetDouble(ap->ctx,inst,kDynSfId,ap->sfp->smp->set[i].value);
          break;

        case kTempoVarScId:
          cmDspSetDouble(ap->ctx,inst,kTempoSfId,ap->sfp->smp->set[i].value);
          break;

        default:
          { assert(0); }
      }           

      cmDspSetDouble(ap->ctx,inst,kCostSfId,ap->sfp->smp->set[i].match_cost);


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
      }


    }

    /*
    // trigger 'section' starts 
    for(i=ap->sfp->smp->vsli; i<ap->sfp->smp->nsli; ++i)
    {
      const cmScoreLoc_t* locPtr = cmScoreLoc(ap->sfp->smp->mp->scH,i);
      if( locPtr->begSectPtr != NULL )
        cmDspSetUInt(ap->ctx,inst,kSectIndexSfId,locPtr->begSectPtr->index);
    }
    */
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
          if( cmScMatcherExec(p->sfp, cmDspUInt(inst,kSmpIdxSfId), cmDspUInt(inst,kStatusSfId), cmDspUInt(inst,kD0SfId), cmDspUInt(inst,kD1SfId), &scLocIdx) == cmOkRC )
            if( scLocIdx != cmInvalidIdx )
              cmDspSetUInt(ctx,inst,kOutSfId,scLocIdx);
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

struct cmDspClass_str* cmScFolClassCons( cmDspCtx_t* ctx )
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

//==========================================================================================================================================

enum
{
  kScLocIdxMdId,
  kResetIdxMdId,
  kCmdMdId
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
} cmDspScMod_t;

void _cmDspScModCb( void* arg, unsigned varSymId, double value )
{
  cmDspScMod_t* p = (cmDspScMod_t*)arg;

  cmDspVar_t* varPtr;
  if((varPtr = cmDspVarSymbolToPtr( p->tmp_ctx, &p->inst, varSymId, 0 )) == NULL )
    return;

  cmDspSetDouble(p->tmp_ctx,&p->inst,varPtr->constId,value);
  
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

  mp->cbArg = p;  // set the modulator callback arg

  

  cmDspSetDefaultUInt(ctx,&p->inst,kScLocIdxMdId,0,0);
  cmDspSetDefaultSymbol(ctx,&p->inst,kCmdMdId,p->offSymId);
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
          cmScModulatorReset(p->mp, ctx->cmCtx, cmDspUInt(inst,kScLocIdxMdId));
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

struct cmDspClass_str* cmScModClassCons( cmDspCtx_t* ctx )
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

//==========================================================================================================================================

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


struct cmDspClass_str* cmGSwitchClassCons( cmDspCtx_t* ctx )
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


//==========================================================================================================================================

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
  cmDspScaleRange_t* p  = (cmDspScaleRange_t*)inst;

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


struct cmDspClass_str* cmScaleRangeClassCons( cmDspCtx_t* ctx )
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
