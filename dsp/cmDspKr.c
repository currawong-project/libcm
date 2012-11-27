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
    cmVOS_Copy(op,oSmpCnt,sp);
  
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
  kSendScId
};

cmDspClass_t _cmScoreDC;

typedef struct
{
  cmDspInst_t inst;
  cmScH_t     scH;
} cmDspScore_t;

cmDspInst_t*  _cmDspScoreAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "fn",      kFnScId,             0, 0, kInDsvFl   | kStrzDsvFl | kReqArgDsvFl, "Score file." },
    { "sel",     kSelScId,            0, 0, kInDsvFl   |  kOutDsvFl | kUIntDsvFl,   "Selected score element index."},
    { "send",    kSendScId,           0, 0, kInDsvFl   | kTypeDsvMask,              "Resend last selected score element."},
    { NULL, 0, 0, 0, 0 }
  };

  cmDspScore_t* p = cmDspInstAlloc(cmDspScore_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);
  
  cmDspSetDefaultUInt( ctx, &p->inst,  kSelScId,           0, cmInvalidId);

  // create the UI control
  cmDspUiScoreCreate(ctx,&p->inst,kFnScId,kSelScId);

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


cmDspRC_t _cmDspScoreReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t        rc = kOkDspRC;
  cmDspScore_t* p  = (cmDspScore_t*)inst;

  cmDspApplyAllDefaults(ctx,inst);

  const cmChar_t* tlFn;
  if((tlFn =  cmDspStrcz(inst, kFnScId )) !=  NULL )
    if( cmScoreInitialize(ctx->cmCtx, &p->scH, tlFn, NULL, NULL ) != kOkTlRC )
      rc = cmErrMsg(&inst->classPtr->err, kInstResetFailDspRC, "Score file open failed.");

  return rc;
}

cmDspRC_t _cmDspScoreRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  switch( evt->dstVarId )
  {
    case kSelScId:
      cmDspSetEvent(ctx,inst,evt);
      break;

    case kSendScId:
      {
        unsigned selIdx;
        if((selIdx = cmDspUInt(inst,kSelScId)) != cmInvalidIdx )
          cmDspSetUInt(ctx,inst,kSelScId, selIdx );
      }
      break;

    default:
      {assert(0);}
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
  kD1MfId
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
    { "bsi",    kBsiMfId,    0, 0, kInDsvFl  | kIntDsvFl, "Starting sample." },
    { "esi",    kEsiMfId,    0, 0, kInDsvFl  | kIntDsvFl, "Ending sample."},
    { "status", kStatusMfId, 0, 0, kOutDsvFl | kIntDsvFl, "Status value output" },
    { "d0",     kD0MfId,     0, 0, kOutDsvFl | kUIntDsvFl, "Data byte 0" },
    { "d1",     kD1MfId,     0, 0, kOutDsvFl | kUIntDsvFl, "Data byte 1" },
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
unsigned _cmDspMidiFilePlaySeekMsgIdx( cmDspCtx_t* ctx, cmDspMidiFilePlay_t* p, unsigned smpIdx )
{
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
    const cmMidiTrackMsg_t** mpp   = cmMidiFileMsgArray(p->mfH);
    unsigned                 msgN  = cmMidiFileMsgCount(p->mfH);
  
    for(; p->curMsgIdx < msgN && p->csi <= mpp[p->curMsgIdx]->dtick  &&  mpp[p->curMsgIdx]->dtick < (p->csi + sPc); ++p->curMsgIdx )
    {
      const cmMidiTrackMsg_t* mp = mpp[p->curMsgIdx];
      switch( mp->status )
      {
        case kNoteOnMdId:
        case kCtlMdId:
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
          p->curMsgIdx = _cmDspMidiFilePlaySeekMsgIdx(ctx, p, p->csi );
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
  kWndCntSfId,
  kWndMsSfId,
  kIndexSfId,
  kStatusSfId,
  kD0SfId,
  kD1SfId,
  kOutSfId
};

cmDspClass_t _cmScFolDC;

typedef struct
{
  cmDspInst_t inst;
  cmScFol*    sfp;
  cmScH_t     scH;
} cmDspScFol_t;

cmDspInst_t*  _cmDspScFolAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "fn",    kFnSfId,     0, 0, kInDsvFl | kStrzDsvFl   | kReqArgDsvFl,   "Score file." },
    { "wndcnt",kWndCntSfId, 0, 0, kInDsvFl | kUIntDsvFl,                    "Event window element count." },
    { "wndms", kWndMsSfId,  0, 0, kInDsvFl | kUIntDsvFl,                    "Event window length milliseconds."},
    { "index", kIndexSfId,  0, 0, kInDsvFl | kUIntDsvFl,                    "Tracking start location."},
    { "status",kStatusSfId, 0, 0, kInDsvFl | kUIntDsvFl,                    "MIDI status byte"},
    { "d0",    kD0SfId,     0, 0, kInDsvFl | kUIntDsvFl,                    "MIDI data byte 0"},
    { "d1",    kD1SfId,     0, 0, kInDsvFl | kUIntDsvFl,                    "MIDI data byte 1"},
    { "out",   kOutSfId,    0, 0, kOutDsvFl| kUIntDsvFl,                    "Current score index."},
    { NULL,    0,           0, 0, 0, NULL }
  };

  cmDspScFol_t* p;

  if((p = cmDspInstAlloc(cmDspScFol_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl)) == NULL )
    return NULL;
  

  p->sfp = cmScFolAlloc(ctx->cmProcCtx, NULL, 0, 0, 0, cmScNullHandle );

  cmDspSetDefaultUInt( ctx, &p->inst,  kWndCntSfId,     0,    10);
  cmDspSetDefaultUInt( ctx, &p->inst,  kWndMsSfId,      0,  5000);
  cmDspSetDefaultUInt( ctx, &p->inst,  kIndexSfId,      0,     0);  
  cmDspSetDefaultUInt( ctx, &p->inst,  kOutSfId,        0,     0);

  return &p->inst;
}

cmDspRC_t _cmDspScFolFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspScFol_t* p = (cmDspScFol_t*)inst;
  cmScFolFree(&p->sfp);
  cmScoreFinalize(&p->scH);
  return kOkDspRC;
}

cmDspRC_t _cmDspScFolOpenScore( cmDspCtx_t* ctx, cmDspInst_t* inst )
{
  const cmChar_t* fn;
  cmDspScFol_t* p  = (cmDspScFol_t*)inst;

  if((fn = cmDspStrcz(inst,kFnSfId)) == NULL || strlen(fn)==0 )
    return cmErrMsg(&inst->classPtr->err, kInvalidArgDspRC, "No score file name supplied.");

  if( cmScoreInitialize(ctx->cmCtx, &p->scH, fn, NULL, NULL ) != kOkScRC )
    return cmErrMsg(&inst->classPtr->err, kSubSysFailDspRC, "Unable to open the score '%s'.",fn);

  return kOkDspRC;
}

cmDspRC_t _cmDspScFolReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t     rc = kOkDspRC;
  cmDspScFol_t* p  = (cmDspScFol_t*)inst;
  rc               = cmDspApplyAllDefaults(ctx,inst);

  if((rc = _cmDspScFolOpenScore(ctx,inst)) != kOkDspRC )
    return rc;

  if( cmScoreIsValid(p->scH) )
    if( cmScFolInit(p->sfp, cmDspSampleRate(ctx), cmDspUInt(inst,kWndCntSfId), cmDspUInt(inst,kWndMsSfId), p->scH) != cmOkRC )
      rc = cmErrMsg(&inst->classPtr->err, kSubSysFailDspRC, "Internal score follower allocation failed.");

  return rc;  
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
          if( cmScFolReset( p->sfp, cmDspUInt(inst,kIndexSfId) ) != cmOkRC )
            cmErrMsg(&inst->classPtr->err, kSubSysFailDspRC, "Score follower reset to score index '%i' failed.");
        break;

      case kStatusSfId:
        if( cmScoreIsValid(p->scH))
        {
          unsigned idx = cmScFolExec(p->sfp, ctx->cycleCnt, cmDspUInt(inst,kStatusSfId), cmDspUInt(inst,kD0SfId), cmDspUInt(inst,kD1SfId));
          if( idx != cmInvalidIdx )
            cmDspSetUInt(ctx,inst,kOutSfId,idx);
        }
        break;

      case kFnSfId:
        _cmDspScFolOpenScore(ctx,inst);
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

