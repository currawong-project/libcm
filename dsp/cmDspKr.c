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
  kMidiFnTlId,
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
    { "mfn",     kMidiFnTlId,    0, 0, kOutDsvFl  | kStrzDsvFl,   "Selected MIDI file." },
    { "bsi",     kBegSmpIdxTlId, 0, 0, kOutDsvFl  | kUIntDsvFl,   "Begin audio sample index."},
    { "esi",     kEndSmpIdxTlId, 0, 0, kOutDsvFl  | kUIntDsvFl,   "End audio sample index."},
    { NULL, 0, 0, 0, 0 }
  };

  cmDspTimeLine_t* p = cmDspInstAlloc(cmDspTimeLine_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);
  
  cmDspSetDefaultUInt(  ctx, &p->inst,  kSelTlId,       0, cmInvalidId);
  cmDspSetDefaultStrcz( ctx, &p->inst,  kAudFnTlId,     NULL, "");
  cmDspSetDefaultStrcz( ctx, &p->inst,  kMidiFnTlId,     NULL, "");
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
  cmDspTimeLine_t* p  = (cmDspTimeLine_t*)inst;

  switch( evt->dstVarId )
  {
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

            cmDspSetUInt(ctx, inst, kBegSmpIdxTlId, op->begSmpIdx );
            cmDspSetUInt(ctx, inst, kEndSmpIdxTlId, op->begSmpIdx + op->durSmpCnt );
            
            // locate the audio file assoc'd with the marker
            cmTlAudioFile_t* afp;
            if((afp = cmTimeLineAudioFileAtTime(p->tlH,op->seqId,op->seqSmpIdx)) != NULL)
              cmDspSetStrcz(ctx, inst, kAudFnTlId, afp->fn );

            // locate the midi file assoc'd with the marker
            cmTlMidiFile_t* mfp;
            if((mfp = cmTimeLineMidiFileAtTime(p->tlH,op->seqId,op->seqSmpIdx)) != NULL )
              cmDspSetStrcz(ctx, inst, kMidiFnTlId, mfp->fn );
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

//
//
//  Read files created by this object with the Octave function cmTextFile().
//
//

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
  unsigned      msgIdx;     // current midi file msg index
  unsigned      bsi;
  unsigned      esi;
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
    { "fn",     kFnMfId,     0, 0, kInDsvFl  | kStrzDsvFl   | kReqArgDsvFl, "File name"},
    { "sel",    kSelMfId,    0, 0, kInDsvFl  | kSymDsvFl,  "start | stop | continue" },
    { "bsi",    kBsiMfId,    0, 0, kInDsvFl  | kUIntDsvFl, "Starting sample." },
    { "esi",    kEsiMfId,    0, 0, kInDsvFl  | kUIntDsvFl, "Ending sample."},
    { "status", kStatusMfId, 0, 0, kOutDsvFl | kUIntDsvFl, "Status value output" },
    { "d0",     kD0MfId,     0, 0, kOutDsvFl | kUIntDsvFl, "Data byte 0" },
    { "d1",     kD1MfId,     0, 0, kOutDsvFl | kUIntDsvFl, "Data byte 1" },
    { NULL, 0, 0, 0, 0 }
  };

  cmDspMidiFilePlay_t* p = cmDspInstAlloc(cmDspMidiFilePlay_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);

  cmDspSetDefaultUInt(  ctx, &p->inst,  kStatusMfId, 0, 0);
  cmDspSetDefaultUInt(  ctx, &p->inst,  kD0MfId,     0, 0);
  cmDspSetDefaultUInt(  ctx, &p->inst,  kD1MfId,     0, 0);

  p->startSymId = cmSymTblRegisterStaticSymbol(ctx->stH,"start");
  p->stopSymId  = cmSymTblRegisterStaticSymbol(ctx->stH,"stop");
  p->contSymId  = cmSymTblRegisterStaticSymbol(ctx->stH,"continue");
  p->mfH        = cmMidiFileNullHandle;
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
  double                   srate = cmDspSampleRate(ctx);

  for(i=0; i<n; ++i)
    if( floor(a[i]->dtick*srate) > smpIdx )
      break;

  return i==n ? cmInvalidIdx : i;
}

cmDspRC_t _cmDspMidiFilePlayOpen(cmDspCtx_t* ctx, cmDspInst_t* inst )
{
  cmDspRC_t        rc = kOkDspRC;
  const cmChar_t*  fn = cmDspStrcz(inst,kFnMfId);
  cmDspMidiFilePlay_t* p  = (cmDspMidiFilePlay_t*)inst;

  if( cmMidiFileOpen( fn, &p->mfH, ctx->cmCtx ) != kOkFileRC )
    rc = cmErrMsg(&inst->classPtr->err, kInstResetFailDspRC, "MIDI file open failed.");
  else
  {
    p->msgIdx    = 0;
    p->bsi       = cmDspUInt(inst,kBsiMfId);
    p->esi       = cmDspUInt(inst,kEsiMfId);
    cmMidiFileTickToMicros(p->mfH);
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
  cmDspRC_t        rc = kOkDspRC;
  cmDspMidiFilePlay_t* p  = (cmDspMidiFilePlay_t*)inst;

  double   srate = cmDspSampleRate(ctx);
  unsigned sPc   = cmDspSamplesPerCycle(ctx);

  
  
    

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
          _cmDspMidiFilePlaySeekMsgIdx(ctx, p, cmDspUInt(inst,kBsiMfId) );
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
