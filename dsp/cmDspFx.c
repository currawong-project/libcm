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
#include "cmText.h"
#include "cmMath.h"
#include "cmFile.h"
#include "cmFileSys.h"
#include "cmSymTbl.h"
#include "cmJson.h"
#include "cmPrefs.h"
#include "cmDspValue.h"
#include "cmMsgProtocol.h"
#include "cmThread.h" 
#include "cmUdpPort.h"
#include "cmUdpNet.h"
#include "cmAudioSys.h"
#include "cmProcObj.h"
#include "cmDspCtx.h"
#include "cmDspClass.h"
#include "cmDspUi.h"
#include "cmAudioFile.h"

#include "cmProcObj.h"
#include "cmProcTemplateMain.h"
#include "cmProc.h"
#include "cmMidi.h"
#include "cmProc2.h"
#include "cmProc3.h"
#include "cmVectOpsTemplateMain.h"

#include "app/cmPickup.h"

#include "cmDspSys.h"

//==========================================================================================================================================
enum
{
  kBypassDyId,
  kTimeDyId,
  kFbDyId,
  kInDyId,
  kOutDyId

};

cmDspClass_t _cmDelayDC;

typedef struct
{
  cmDspInst_t  inst;
  cmMDelay*    delay;
} cmDspDelay_t;

cmDspInst_t*  _cmDspDelayAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  unsigned chs = 1;

  cmDspVarArg_t args[] =
  {
    { "bypass",kBypassDyId,0,0,   kInDsvFl  | kBoolDsvFl   | kOptArgDsvFl,  "Bypass enable flag." },
    { "time",  kTimeDyId, 0, 0,   kInDsvFl  | kDoubleDsvFl | kOptArgDsvFl,  "Max delay time in milliseconds" },
    { "fb",    kFbDyId,   0, 0,   kInDsvFl  | kDoubleDsvFl | kOptArgDsvFl,  "Feedback"  },
    { "in",    kInDyId,   0, 0,   kInDsvFl  | kAudioBufDsvFl,               "Audio input" },
    { "out",   kOutDyId,  0, chs, kOutDsvFl | kAudioBufDsvFl,               "Audio output." },
    { NULL, 0, 0, 0, 0 }
  };

  cmDspDelay_t* p  = cmDspInstAlloc(cmDspDelay_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);

  // set default values for the parameters that were not explicitely set in the va_arg list
  cmDspSetDefaultBool(   ctx, &p->inst, kBypassDyId,0,   0 );
  cmDspSetDefaultUInt(   ctx, &p->inst, kTimeDyId,  0,   1000 );
  cmDspSetDefaultDouble( ctx, &p->inst ,kFbDyId,    0.0, 0.0 );

  cmReal_t dtimeMs =  cmDspDefaultUInt(&p->inst,kTimeDyId);
  cmReal_t fbCoeff = cmDspDefaultDouble(&p->inst,kFbDyId);

  p->delay = cmMDelayAlloc(ctx->cmProcCtx,NULL, cmDspSamplesPerCycle(ctx), cmDspSampleRate(ctx), fbCoeff,  1, &dtimeMs, NULL  );

  return &p->inst;
}

cmDspRC_t _cmDspDelayFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t     rc = kOkDspRC;
  cmDspDelay_t* p  = (cmDspDelay_t*)inst;

  cmMDelayFree(&p->delay);

  //cmCtxFree(&p->ctx);
  
  return rc;
}

cmDspRC_t _cmDspDelayReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t     rc = kOkDspRC;

  rc = cmDspApplyAllDefaults(ctx,inst);

  return rc;  
}

cmDspRC_t _cmDspDelayExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspDelay_t* p  = (cmDspDelay_t*)inst;
  cmDspRC_t     rc = kOkDspRC;

  unsigned          iChIdx  = 0;
  const cmSample_t* ip      = cmDspAudioBuf(ctx,inst,kInDyId,iChIdx);
  unsigned          iSmpCnt = cmDspVarRows(inst,kInDyId);
  
  unsigned          oChIdx  = 0;
  cmSample_t*       op      = cmDspAudioBuf(ctx,inst,kOutDyId,oChIdx);
  unsigned          oSmpCnt = cmDspVarRows(inst,kOutDyId);
  
  bool              bypassFl= cmDspBool(inst,kBypassDyId);
  
  cmMDelayExec(p->delay,ip,op,cmMin(iSmpCnt,oSmpCnt),bypassFl);

  return rc;
}

cmDspRC_t _cmDspDelayRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspDelay_t* p = (cmDspDelay_t*)inst;
  cmDspRC_t     rc= kOkDspRC;

  cmDspSetEvent(ctx,inst,evt);

  switch( evt->dstVarId )
  {
    case kBypassDyId:
      break;

    case kTimeDyId:
      p->delay->delayArray[0].delayMs = cmDspDouble(inst,kTimeDyId);
      break;

    case kFbDyId:
      p->delay->fbCoeff  = cmDspDouble(inst,kFbDyId);
      break;

    default:
      { assert(0); }
  }
  return rc;
}

struct cmDspClass_str* cmDelayClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmDelayDC,ctx,"Delay",
    NULL,
    _cmDspDelayAlloc,
    _cmDspDelayFree,
    _cmDspDelayReset,
    _cmDspDelayExec,
    _cmDspDelayRecv,
    NULL,NULL,
    "Simple delay.");

  return &_cmDelayDC;
}

//==========================================================================================================================================
enum
{
  kBypassMtId,
  kScaleMtId,
  kFbMtId,
  kInMtId,
  kOutMtId,
  kBaseMsMtId

};

cmDspClass_t _cmMtDelayDC;

typedef struct
{
  cmDspInst_t  inst;
  cmMDelay*    p;
  unsigned     baseGainMtId;
  unsigned     tapCnt;
  cmReal_t*    msV;
  cmReal_t*    gainV;
  unsigned     printSymId;
} cmDspMtDelay_t;

// args: bypassFl, time_scale, feedback, tap_ms0, tap_gain0, tapms1, tap_gain1, ....

cmDspInst_t*  _cmDspMtDelayAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  va_list vl1;

  cmDspVarArg_t args[] =
  {
    { "bypass",kBypassMtId,0, 0,   kInDsvFl  | kBoolDsvFl   | kReqArgDsvFl,  "Bypass enable flag." },
    { "scale", kScaleMtId, 0, 0,   kInDsvFl  | kDoubleDsvFl | kReqArgDsvFl,  "Scale tap times. (0.0 - 1.0)" },
    { "fb",    kFbMtId,    0, 0,   kInDsvFl  | kDoubleDsvFl | kReqArgDsvFl,  "Feedback"  },
    { "in",    kInMtId,    0, 0,   kInDsvFl  | kAudioBufDsvFl,               "Audio input" },
    { "out",   kOutMtId,   0, 1,   kOutDsvFl | kAudioBufDsvFl,               "Audio output." },
  };

  // verify that at least one var arg exists
  if( va_cnt < 5 || cmIsEvenU(va_cnt) )
  {
    cmDspClassErr(ctx,classPtr,kInvalidArgDspRC,"The 'multi-tap delay requires at least 5 arguments. Three fixed arguments and groups of two tap specification arguments.");
    return NULL;
  }    

  va_copy(vl1,vl);

  unsigned      reqArgCnt    = 3;
  unsigned      fixArgCnt    = sizeof(args)/sizeof(args[0]);
  unsigned      tapCnt       = (va_cnt - reqArgCnt)/2;
  cmReal_t*     msV          = cmMemAllocZ(cmReal_t,tapCnt);
  cmReal_t*     gainV        = cmMemAllocZ(cmReal_t,tapCnt);
  
  unsigned      argCnt       = fixArgCnt + 2 * tapCnt;
  unsigned      baseGainMtId = kBaseMsMtId + tapCnt;
  cmDspVarArg_t a[ argCnt+1 ];
  unsigned      i;

  // Get the taps and gains
  va_arg(vl1,int);    // enable
  va_arg(vl1,double);  // time scale
  va_arg(vl1,double);  // feedback
  for(i=0; i<tapCnt; ++i)
  {
    msV[i]   = va_arg(vl1,double);
    gainV[i] = va_arg(vl1,double);
  }


    // setup the output gain args 
  cmDspArgCopy(       a, argCnt, 0, args, fixArgCnt );
  cmDspArgSetupN(ctx, a, argCnt, kBaseMsMtId,  tapCnt, "ms",   kBaseMsMtId,  0, 0, kInDsvFl | kDoubleDsvFl, "Tap delay times in milliseconds.");
  cmDspArgSetupN(ctx, a, argCnt, baseGainMtId, tapCnt, "gain", baseGainMtId, 0, 0, kInDsvFl | kDoubleDsvFl, "Tap delay linear gain.");
  cmDspArgSetupNull(  a+argCnt); // set terminating arg. flag

  cmDspMtDelay_t* p  = cmDspInstAlloc(cmDspMtDelay_t,ctx,classPtr,a,instSymId,id,storeSymId,reqArgCnt,vl1);


  p->p            = cmMDelayAlloc(ctx->cmProcCtx,NULL,0, 0, 0, 0, NULL, NULL  );
  p->baseGainMtId = baseGainMtId;
  p->tapCnt       = tapCnt;
  p->msV          = msV;
  p->gainV        = gainV;
  p->printSymId   = cmSymTblRegisterStaticSymbol(ctx->stH,"_print");

  for(i=0; i<tapCnt; ++i)
  {
    cmDspSetDefaultDouble(ctx,&p->inst,kBaseMsMtId+i,   0.0, msV[i]);
    cmDspSetDefaultDouble(ctx,&p->inst,baseGainMtId+i,  0.0, gainV[i]);
  }
  
  va_end(vl1);
  return &p->inst;
}

cmDspRC_t _cmDspMtDelayFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t     rc = kOkDspRC;
  cmDspMtDelay_t* p  = (cmDspMtDelay_t*)inst;

  cmMemFree(p->msV);
  cmMemFree(p->gainV);

  cmMDelayFree(&p->p);
  
  return rc;
}

cmDspRC_t _cmDspMtDelayReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t     rc = kOkDspRC;
  cmDspMtDelay_t* p  = (cmDspMtDelay_t*)inst;

  if((rc = cmDspApplyAllDefaults(ctx,inst)) == kOkDspRC )
  {
    cmMDelayInit(p->p,cmDspSamplesPerCycle(ctx), cmDspSampleRate(ctx), cmDspDouble(&p->inst,kFbMtId), p->tapCnt, p->msV, p->gainV  );    
  }

  return rc;  
}

cmDspRC_t _cmDspMtDelayExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspMtDelay_t* p  = (cmDspMtDelay_t*)inst;
  cmDspRC_t     rc = kOkDspRC;

  unsigned          iChIdx  = 0;
  const cmSample_t* ip      = cmDspAudioBuf(ctx,inst,kInMtId,iChIdx);
  unsigned          iSmpCnt = cmDspVarRows(inst,kInMtId);
  
  unsigned          oChIdx  = 0;
  cmSample_t*       op      = cmDspAudioBuf(ctx,inst,kOutMtId,oChIdx);
  unsigned          oSmpCnt = cmDspVarRows(inst,kOutMtId);
  
  bool              bypassFl= cmDspBool(inst,kBypassMtId);
  
  if( ip != NULL )
    cmMDelayExec(p->p,ip,op,cmMin(iSmpCnt,oSmpCnt),bypassFl);

  return rc;
}

cmDspRC_t _cmDspMtDelayRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspMtDelay_t* p = (cmDspMtDelay_t*)inst;
  cmDspRC_t     rc= kOkDspRC;

  cmDspSetEvent(ctx,inst,evt);

  // set tap times
  if( kBaseMsMtId <= evt->dstVarId && evt->dstVarId < kBaseMsMtId + p->p->delayCnt )
    cmMDelaySetTapMs(p->p,evt->dstVarId - kBaseMsMtId, cmDspDouble(inst,evt->dstVarId));
  else
    // set tap gains
    if( p->baseGainMtId <= evt->dstVarId && evt->dstVarId < p->baseGainMtId + p->p->delayCnt )
      cmMDelaySetTapGain(p->p,evt->dstVarId - p->baseGainMtId, cmDspDouble(inst,evt->dstVarId));
    else
    {
      switch( evt->dstVarId )
      {
        case kScaleMtId:
          //cmDspDouble(inst,kScaleMtId);
          break;

        case kFbMtId:
          p->p->fbCoeff  = cmDspDouble(inst,kFbMtId);
          break;

      }
    }
  return rc;
}

cmDspRC_t  _cmDspMtDelayRecvFunc(   cmDspCtx_t* ctx, struct cmDspInst_str* inst,  unsigned attrSymId, const cmDspValue_t* value )
{
  cmDspRC_t       rc = kOkDspRC;
  cmDspMtDelay_t* p  = (cmDspMtDelay_t*)inst;

  if( cmDsvIsSymbol(value) && (cmDsvSymbol(value)==p->printSymId) )
  {
    int i;
    cmRptPrintf(ctx->rpt,"taps:%i\n",p->tapCnt);
    for(i=0; i<p->tapCnt; ++i)
      cmRptPrintf(ctx->rpt,"%f %f\n",p->msV[i],p->gainV[i]);

    cmMDelayReport(p->p, ctx->rpt );
  }

  return rc;
}


struct cmDspClass_str* cmMtDelayClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmMtDelayDC,ctx,"MtDelay",
    NULL,
    _cmDspMtDelayAlloc,
    _cmDspMtDelayFree,
    _cmDspMtDelayReset,
    _cmDspMtDelayExec,
    _cmDspMtDelayRecv,
    NULL,
    _cmDspMtDelayRecvFunc,
    "Multi-tap delay.");

  return &_cmMtDelayDC;
}

//==========================================================================================================================================
enum
{
  kBypassPsId,
  kRatioPsId,
  kInPsId,
  kOutPsId
};

cmDspClass_t _cmPShiftDC;

typedef struct
{
  cmDspInst_t   inst;
  //cmCtx*        ctx;
  cmPitchShift* pshift;
} cmDspPShift_t;

cmDspInst_t*  _cmDspPShiftAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  unsigned chs = 1;

  cmDspVarArg_t args[] =
  {
    { "bypass",kBypassPsId, 0, 0,   kInDsvFl  | kBoolDsvFl   | kOptArgDsvFl,  "Bypass enable flag." },
    { "ratio", kRatioPsId,  0, 0,   kInDsvFl  | kDoubleDsvFl | kOptArgDsvFl,  "Ratio"  },
    { "in",    kInPsId,     0, 0,   kInDsvFl  | kAudioBufDsvFl,               "Audio input" },
    { "out",   kOutPsId,    0, chs, kOutDsvFl | kAudioBufDsvFl,               "Audio output." },
    { NULL, 0, 0, 0, 0 }
  };

  cmDspPShift_t* p  = cmDspInstAlloc(cmDspPShift_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);

  // set default values for the parameters that were not explicitely set in the va_arg list
  cmDspSetDefaultBool(   ctx, &p->inst, kBypassPsId,  0,     0 );
  cmDspSetDefaultDouble( ctx, &p->inst ,kRatioPsId,   0.0, 1.0 );

  p->pshift = cmPitchShiftAlloc(ctx->cmProcCtx,NULL,cmDspSamplesPerCycle(ctx), cmDspSampleRate(ctx)  );

  return &p->inst;
}

cmDspRC_t _cmDspPShiftFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t      rc = kOkDspRC;
  cmDspPShift_t* p  = (cmDspPShift_t*)inst;

  cmPitchShiftFree(&p->pshift);

  //cmCtxFree(&p->ctx);
  
  return rc;
}

cmDspRC_t _cmDspPShiftReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t     rc = kOkDspRC;

  rc = cmDspApplyAllDefaults(ctx,inst);

  return rc;  
}

cmDspRC_t _cmDspPShiftExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspPShift_t* p  = (cmDspPShift_t*)inst;
  cmDspRC_t      rc = kOkDspRC;

  unsigned          iChIdx  = 0;
  const cmSample_t* ip      = cmDspAudioBuf(ctx,inst,kInPsId,iChIdx);
  unsigned          iSmpCnt = cmDspVarRows(inst,kInPsId);
  
  unsigned          oChIdx  = 0;
  cmSample_t*       op      = cmDspAudioBuf(ctx,inst,kOutPsId,oChIdx);
  unsigned          oSmpCnt = cmDspVarRows(inst,kOutPsId);
  
  bool              bypassFl= cmDspBool(inst,kBypassPsId);

  cmPitchShiftExec(p->pshift,ip,op,cmMin(iSmpCnt,oSmpCnt),cmDspDouble(inst,kRatioPsId),bypassFl);

  return rc;
}

cmDspRC_t _cmDspPShiftRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  return cmDspSetEvent(ctx,inst,evt);
}

struct cmDspClass_str* cmPShiftClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmPShiftDC,ctx,"PShift",
    NULL,
    _cmDspPShiftAlloc,
    _cmDspPShiftFree,
    _cmDspPShiftReset,
    _cmDspPShiftExec,
    _cmDspPShiftRecv,
    NULL,NULL,
    "Pitch Shifter.");

  return &_cmPShiftDC;
}

//==========================================================================================================================================
enum
{
  kTimeLrId,
  kPGainLrId,
  kRGainLrId,
  kBypassLrId,
  kPlayLrId,
  kRecdLrId,
  kRatioLrId,
  kInLrId,
  kOutLrId

};

cmDspClass_t _cmLoopRecdDC;

typedef struct
{
  cmDspInst_t     inst;
  //cmCtx*          ctx;
  bool            playFl;
  cmLoopRecord*   lrp;
} cmDspLoopRecd_t;

cmDspInst_t*  _cmDspLoopRecdAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  unsigned chs = 1;

  cmDspVarArg_t args[] =
  {
    { "time",  kTimeLrId,  0, 0,   kInDsvFl  | kDoubleDsvFl | kOptArgDsvFl,  "Max record time in seconds" },
    { "pgain", kPGainLrId, 0, 0,   kInDsvFl  | kDoubleDsvFl | kOptArgDsvFl,  "Pass-through gain."},
    { "rgain", kRGainLrId, 0, 0,   kInDsvFl  | kDoubleDsvFl | kOptArgDsvFl,  "Recorder out gain."},
    { "bypass",kBypassLrId,0, 0,   kInDsvFl  | kBoolDsvFl,                   "Bypass flag"},
    { "play",  kPlayLrId,  0, 0,   kInDsvFl  | kBoolDsvFl,                   "Play gate flag"  },
    { "recd",  kRecdLrId,  0, 0,   kInDsvFl  | kBoolDsvFl,                   "Recd gate flag" },
    { "ratio", kRatioLrId, 0, 0,   kInDsvFl  | kDoubleDsvFl,                 "Playback speed ratio"},
    { "in",    kInLrId,    0, 0,   kInDsvFl  | kAudioBufDsvFl,               "Audio input" },
    { "out",   kOutLrId,   0, chs, kOutDsvFl | kAudioBufDsvFl,               "Audio output." },
    { NULL, 0, 0, 0, 0 }
  };

  cmDspLoopRecd_t* p  = cmDspInstAlloc(cmDspLoopRecd_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);

  // set default values for the parameters that were not explicitely set in the va_arg list
  cmDspSetDefaultDouble(   ctx, &p->inst, kTimeLrId,   0,   10 );
  cmDspSetDefaultDouble(   ctx, &p->inst, kPGainLrId,   0,  1.0 );
  cmDspSetDefaultDouble(   ctx, &p->inst, kRGainLrId,   0,  1.0 );
  cmDspSetDefaultBool(     ctx, &p->inst, kBypassLrId, 0,    0 ); 
  cmDspSetDefaultBool(     ctx, &p->inst, kPlayLrId,   0,    0 );
  cmDspSetDefaultBool(     ctx, &p->inst, kRecdLrId,   0,    0 );
  cmDspSetDefaultDouble(   ctx, &p->inst, kRatioLrId,  0,   1.0);

  p->lrp   = cmLoopRecordAlloc(ctx->cmProcCtx,NULL,0,0,0   );

  return &p->inst;
}

cmDspRC_t _cmDspLoopRecdFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t     rc = kOkDspRC;
  cmDspLoopRecd_t* p  = (cmDspLoopRecd_t*)inst;

  cmLoopRecordFree(&p->lrp);

  //cmCtxFree(&p->ctx);
  
  return rc;
}

cmDspRC_t _cmDspLoopRecdReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t     rc = kOkDspRC;
 cmDspLoopRecd_t* p  = (cmDspLoopRecd_t*)inst;

  rc = cmDspApplyAllDefaults(ctx,inst);

  cmReal_t   maxRecdTimeSecs  =  cmDspDefaultDouble(&p->inst,kTimeLrId);
  unsigned   maxRecdTimeSmps  = floor(cmDspSampleRate(ctx) * maxRecdTimeSecs);
  unsigned   xfadeTimeSmps    = floor(cmDspSampleRate(ctx) * 50.0/1000.0);
  
  if( maxRecdTimeSmps != p->lrp->maxRecdSmpCnt || xfadeTimeSmps != p->lrp->xfadeSmpCnt )
    cmLoopRecordInit(p->lrp,cmDspSamplesPerCycle(ctx),maxRecdTimeSmps,xfadeTimeSmps);

  return rc;  
}

cmDspRC_t _cmDspLoopRecdExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspLoopRecd_t* p  = (cmDspLoopRecd_t*)inst;
  cmDspRC_t        rc = kOkDspRC;

  unsigned          iChIdx  = 0;
  const cmSample_t* ip      = cmDspAudioBuf(ctx,inst,kInLrId,iChIdx);
  unsigned          iSmpCnt = cmDspVarRows(inst,kInLrId);
  
  unsigned          oChIdx  = 0;
  cmSample_t*       op      = cmDspAudioBuf(ctx,inst,kOutLrId,oChIdx);
  unsigned          oSmpCnt = cmDspVarRows(inst,kOutLrId);

  bool               recdFl   = cmDspBool(inst,kRecdLrId);
  bool               bypassFl = cmDspBool(inst,kBypassLrId);
  double             ratio    = cmDspDouble(inst,kRatioLrId);

  double           rgain    = cmDspDouble(inst,kRGainLrId); // recorder output gain
  double           pgain    = cmDspDouble(inst,kPGainLrId); // pass through gain

  if( ip != NULL && op != NULL )
    cmLoopRecordExec(p->lrp,ip,op,cmMin(iSmpCnt,oSmpCnt), bypassFl, recdFl, p->playFl, ratio, pgain, rgain );


  p->playFl = false;

  return rc;
}

cmDspRC_t _cmDspLoopRecdRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspLoopRecd_t* p = (cmDspLoopRecd_t*)inst;
  cmDspRC_t       rc = cmDspSetEvent(ctx,inst,evt);

  
  switch(evt->dstVarId)
  {
    case kPlayLrId:
      p->playFl = cmDspBool(inst,kPlayLrId);
      break;
  }

  return rc;
}

struct cmDspClass_str* cmLoopRecdClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmLoopRecdDC,ctx,"LoopRecd",
    NULL,
    _cmDspLoopRecdAlloc,
    _cmDspLoopRecdFree,
    _cmDspLoopRecdReset,
    _cmDspLoopRecdExec,
    _cmDspLoopRecdRecv,
    NULL,NULL,
    "Loop recorder.");

  return &_cmLoopRecdDC;
}

//==========================================================================================================================================
enum
{
  kBypassRcId,
  kCoeffRcId,
  kInRcId,
  kOutRcId
};

cmDspClass_t _cmRectifyDC;

typedef struct
{
  cmDspInst_t  inst;
} cmDspRectify_t;

cmDspInst_t*  _cmDspRectifyAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  unsigned chs = 1;

  cmDspVarArg_t args[] =
  {
    { "bypass",kBypassRcId,0, 0,   kInDsvFl  | kBoolDsvFl   | kOptArgDsvFl,  "Bypass enable flag." },
    { "coeff", kCoeffRcId, 0, 0,   kInDsvFl  | kDoubleDsvFl | kOptArgDsvFl,  "Coefficient" },
    { "in",    kInRcId,    0, 0,   kInDsvFl  | kAudioBufDsvFl,               "Audio input" },
    { "out",   kOutRcId,   0, chs, kOutDsvFl | kAudioBufDsvFl,               "Audio output." },
    { NULL, 0, 0, 0, 0 }
  };

  cmDspRectify_t* p  = cmDspInstAlloc(cmDspRectify_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);

  // set default values for the parameters that were not explicitely set in the va_arg list
  cmDspSetDefaultBool(   ctx, &p->inst, kBypassRcId,0,    0 );
  cmDspSetDefaultDouble( ctx, &p->inst, kCoeffRcId, 0,  0.0 );


  return &p->inst;
}

cmDspRC_t _cmDspRectifyFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  return kOkDspRC;
}

cmDspRC_t _cmDspRectifyReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t     rc = kOkDspRC;

  rc = cmDspApplyAllDefaults(ctx,inst);

  return rc;  
}

cmDspRC_t _cmDspRectifyExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  unsigned          iChIdx  = 0;
  const cmSample_t* ip      = cmDspAudioBuf(ctx,inst,kInRcId,iChIdx);
  unsigned          iSmpCnt = cmDspVarRows(inst,kInRcId);
  
  unsigned          oChIdx  = 0;
  cmSample_t*       op      = cmDspAudioBuf(ctx,inst,kOutRcId,oChIdx);
  unsigned          oSmpCnt = cmDspVarRows(inst,kOutRcId);
  
  bool              bypassFl= cmDspBool(inst,kBypassRcId);
  unsigned          n       = cmMin(iSmpCnt,oSmpCnt);

  unsigned          i;

  if( bypassFl )
    memcpy(op,ip,n*sizeof(cmSample_t));
  else
  {    
    for(i=0; i<n; ++i)
      op[i] = ip[i] > 0 ? ip[i] : 0;
  }

  return kOkDspRC;
}

cmDspRC_t _cmDspRectifyRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  return cmDspSetEvent(ctx,inst,evt);
}

struct cmDspClass_str* cmRectifyClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmRectifyDC,ctx,"Rectify",
    NULL,
    _cmDspRectifyAlloc,
    _cmDspRectifyFree,
    _cmDspRectifyReset,
    _cmDspRectifyExec,
    _cmDspRectifyRecv,
    NULL,NULL,
    "Half-wave rectifier.");

  return &_cmRectifyDC;
}


//==========================================================================================================================================
enum
{
  kWndMsGdId,
  kOnThreshPctGdId,
  kOnThreshDbGdId,
  kOffThreshDbGdId,
  kInGdId,
  kGateGdId,
  kRmsGdId,
  kMeanGdId
};

cmDspClass_t _cmGateDetectDC;

typedef struct
{
  cmDspInst_t  inst;
  //cmCtx*        ctx;
  cmShiftBuf*   sbp;
  cmGateDetect* gdp;
  
  
} cmDspGateDetect_t;

cmDspInst_t*  _cmDspGateDetectAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "wnd",   kWndMsGdId,      0, 0,   kInDsvFl  | kDoubleDsvFl   | kOptArgDsvFl,  "Window length in milliseconds." },
    { "onpct", kOnThreshPctGdId,0, 0,   kInDsvFl  | kDoubleDsvFl   | kOptArgDsvFl,  "Onset slope threshold [0.0 - 1.0]." },
    { "ondb",  kOnThreshDbGdId, 0, 0,   kInDsvFl  | kDoubleDsvFl   | kOptArgDsvFl,  "Onset threshold dB [-Inf to 0]" },
    { "offdb", kOffThreshDbGdId,0, 0,   kInDsvFl  | kDoubleDsvFl   | kOptArgDsvFl,  "Offset threshold dB [-Inf to 0]" },
    { "in",    kInGdId,         0, 0,   kInDsvFl  | kAudioBufDsvFl,                 "Audio input" },
    { "gate",  kGateGdId,       0, 0,   kOutDsvFl | kBoolDsvFl,                     "Gate state output." },
    { "rms",   kRmsGdId,        0, 0,   kOutDsvFl | kDoubleDsvFl,                   "Signal level RMS"},
    { "mean",  kMeanGdId,       0, 0,   kOutDsvFl | kDoubleDsvFl,                   "Derr mean."},
    { NULL, 0, 0, 0, 0 }
  };

  cmDspGateDetect_t* p  = cmDspInstAlloc(cmDspGateDetect_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);

  p->sbp   = cmShiftBufAlloc(ctx->cmProcCtx,NULL,0,0,0);
  p->gdp   = cmGateDetectAlloc(ctx->cmProcCtx,NULL,0,0,0,0);

  // set default values for the parameters that were not explicitely set in the va_arg list
  cmDspSetDefaultDouble( ctx, &p->inst, kWndMsGdId,       0,    42 );
  cmDspSetDefaultDouble( ctx, &p->inst, kOnThreshPctGdId, 0,    0.8 );
  cmDspSetDefaultDouble( ctx, &p->inst, kOnThreshDbGdId,  0,  -30.0 );
  cmDspSetDefaultDouble( ctx, &p->inst, kOffThreshDbGdId, 0,  -60.0 );

  return &p->inst;
}

cmDspRC_t _cmDspGateDetectFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspGateDetect_t* p = (cmDspGateDetect_t*)inst;
  cmGateDetectFree(&p->gdp);
  cmShiftBufFree(&p->sbp);
  //cmCtxFree(&p->ctx);
  return kOkDspRC;
}

cmDspRC_t _cmDspGateDetectReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspGateDetect_t* p = (cmDspGateDetect_t*)inst;
  cmDspRC_t     rc = kOkDspRC;

  if((rc = cmDspApplyAllDefaults(ctx,inst)) != kOkDspRC )
    return rc;

  double   wndMs     = cmDspDouble(inst,kWndMsGdId);
  double   sr        = cmDspSampleRate(ctx);
  unsigned wndSmpCnt = floor(wndMs * sr / 1000.0);
  
  if( cmShiftBufInit(p->sbp, cmDspSamplesPerCycle(ctx), wndSmpCnt, wndSmpCnt/4 ) != cmOkRC )
    return cmErrMsg(&ctx->cmCtx->err,kInstResetFailDspRC,"The gate detector shift buffer initialization failed.");

  if( cmGateDetectInit(p->gdp, cmDspSamplesPerCycle(ctx), cmDspDouble(inst,kOnThreshPctGdId), cmDspDouble(inst,kOnThreshDbGdId), cmDspDouble(inst,kOffThreshDbGdId) ) != cmOkRC )
    return cmErrMsg(&ctx->cmCtx->err,kInstResetFailDspRC,"The gate detector shift buffer initialization failed.");

  return rc;  
}

cmDspRC_t _cmDspGateDetectExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspGateDetect_t* p = (cmDspGateDetect_t*)inst;
  unsigned          iChIdx  = 0;
  const cmSample_t* ip      = cmDspAudioBuf(ctx,inst,kInGdId,iChIdx);
  unsigned          iSmpCnt = cmDspVarRows(inst,kInGdId);
  
  
  while( cmShiftBufExec(p->sbp, ip, iSmpCnt ) )
  {
    cmGateDetectExec(p->gdp,p->sbp->outV, p->sbp->outN );

    cmDspSetDouble(ctx,inst,kRmsGdId,p->gdp->rms);
    cmDspSetDouble(ctx,inst,kMeanGdId,p->gdp->mean);

    if( p->gdp->deltaFl )
      cmDspSetBool(  ctx,inst,kGateGdId,p->gdp->gateFl);
    
  }

  return kOkDspRC;
}

cmDspRC_t _cmDspGateDetectRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t rc;

  if((rc = cmDspSetEvent(ctx,inst,evt)) != kOkDspRC )
    return rc;

  switch(evt->dstVarId)
  {
    case kWndMsGdId:
      break;
    case kOnThreshPctGdId:
      break;
    case kOnThreshDbGdId:
      break;
    case kOffThreshDbGdId:
      break;
    case kInGdId:
      break;
  }

  return rc;
}

struct cmDspClass_str* cmGateDetectClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmGateDetectDC,ctx,"GateDetect",
    NULL,
    _cmDspGateDetectAlloc,
    _cmDspGateDetectFree,
    _cmDspGateDetectReset,
    _cmDspGateDetectExec,
    _cmDspGateDetectRecv,
    NULL,NULL,
    "Gate detector.");

  return &_cmGateDetectDC;
}


//==========================================================================================================================================
/*
  The purpose of this object is to calculate, store and retrieve gain coefficents
  for a set of audio channels.  The gain coefficients are designed to balance the
  volume of each channel relative to the others.  During gain calibration
  a sample of each channel is taken and it's average volume is determined.  
  After an example of all channels has been received a new set of gain coefficients
  is calculated which decreases the volume of loud channels and increases the
  volume of quiet channels.

  The gain coefficents are made available via a set of 'gain-###' output ports.

  This object acts as an interface to the cmAutoGain processor.  

  As input it takes a channel configuration JSON file of the form:
  {
    ch_array : 
    [ ["ch","ssi","pitch","midi","gain"]
      [ 0,    0,    "C4",   60,    1.0 ]
               ....
      [ n     0,    "C5",   72,    1.0 ]
    ] 
  }      

  Each array in 'ch_array' gives the configuration of a channel.
  

  It also requires a JSON resource object of the form
    gdParms: 
    {
      medCnt: 5                   
      avgCnt: 9       
      suprCnt: 6       
      offCnt: 3       
      suprCoeff: 1.400000       
      onThreshDb: -53.000000       
      offThreshDb: -80.000000       
    }    

    These arguments are used to configure the cmAutoGain Proessor gate detector function.

  During runtime the object accepts the following action selector symbol id's:
    a. start  - begin a new calibration  session 
    b. proc   - end a calibration session and calculate new gain coeff's
    c. cancel - cancel a calibration session   
    d. write  - write the channel configuration file 
    e. print  - print the current auto gain calibration state

  After a 'start' msg the object accepts channel id's throught its 'id' input port.
  Each 'id' identifies the channel which it will process next.
  Upon reception of a channel id the object routes subsequent audio to its
  internal cmAutoGain processor until it receives the next channel id 
  or a 'proc' or 'cancel' symbol.


*/
//==========================================================================================================================================
enum
{
  kChCntAgId,
  kHopAgId,
  kMedNAgId,
  kAvgNAgId,
  kSupNAgId,
  kOffNAgId,
  kSupCoefAgId,
  kOnThrAgId,
  kOffThrAgId,
  kSelAgId,
  kIdAgId,
  kInBaseAgId
};

typedef struct
{
  cmDspInst_t        inst;
  cmAutoGain*        agp;
  unsigned           gainBaseAgId;
  unsigned           chCnt;
  unsigned           chIdx;
  unsigned           startSymId;
  unsigned           procSymId;
  unsigned           cancelSymId;
  unsigned           writeSymId;
  unsigned           printSymId;
} cmDspAutoGain_t;

cmDspClass_t _cmAutoGainDC;

cmDspInst_t*  _cmDspAutoGainAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
    {
      { "chCnt",kChCntAgId,  0, 0,  kUIntDsvFl   | kReqArgDsvFl, "Audio channel count."},  
      { "hop",  kHopAgId,    0, 0,  kDoubleDsvFl | kReqArgDsvFl, "RMS hop in milliseconds."},
      { "med",  kMedNAgId,   0, 0,  kUIntDsvFl   | kReqArgDsvFl, "Median filter hop count."},
      { "avg",  kAvgNAgId,   0, 0,  kUIntDsvFl   | kReqArgDsvFl, "Average filter hop count."},
      { "sup",  kSupNAgId,   0, 0,  kUIntDsvFl   | kReqArgDsvFl, "Supression filter hop count."},
      { "off",  kOffNAgId,   0, 0,  kUIntDsvFl   | kReqArgDsvFl, "Offset filter hop count."},
      { "supC", kSupCoefAgId,0, 0,  kInDsvFl     | kDoubleDsvFl | kReqArgDsvFl, "Suppression coefficent."},
      { "onThr",kOnThrAgId,  0, 0,  kInDsvFl     | kDoubleDsvFl | kReqArgDsvFl, "Onset threshold in dB."},
      { "offThr",kOffThrAgId,0, 0,  kInDsvFl     | kDoubleDsvFl | kReqArgDsvFl, "Offset threshold in dB."},
      { "sel",  kSelAgId,    0, 0,  kInDsvFl     | kSymDsvFl    | kNoArgDsvFl,  "Action Selector: start | proc | cancel." },
      { "id",   kIdAgId,     0, 0,  kInDsvFl     | kIntDsvFl    | kNoArgDsvFl,  "Channel id input."},
    };

  va_list             vl1;
  unsigned            i;


  // verify that at least one var arg exists
  if( va_cnt < 1 )
  {
    cmDspClassErr(ctx,classPtr,kInvalidArgDspRC,"The AutoGain constructor must be given the audio channel count as its first argument.");
    return NULL;
  }    
  // copy the va_list so that it can be used again in cmDspInstAlloc()
  va_copy(vl1,vl);

  // get the first var arg which should be a filename
  unsigned chCnt = va_arg(vl,unsigned);

  if( chCnt == 0 )
  {
    cmDspClassErr(ctx,classPtr,kInvalidArgDspRC,"The AutoGain constructor requires at least 1 audio channel.");
    va_end(vl1);
    return NULL;
  }

  unsigned fixArgCnt    = sizeof(args)/sizeof(args[0]);
  unsigned argCnt       = fixArgCnt + 2 * chCnt;
  unsigned gainBaseAgId = kInBaseAgId + chCnt;
  cmDspVarArg_t a[ argCnt+1 ];

  assert( fixArgCnt == kInBaseAgId );

  // setup the output gain args 
  cmDspArgCopy(       a, argCnt, 0, args, fixArgCnt );
  cmDspArgSetupN(ctx, a, argCnt, kInBaseAgId,  chCnt, "in",   kInBaseAgId,  0, 0, kInDsvFl  | kAudioBufDsvFl, "audio in");
  cmDspArgSetupN(ctx, a, argCnt, gainBaseAgId, chCnt, "gain", gainBaseAgId, 0, 0, kOutDsvFl | kDoubleDsvFl,   "calibrated channel gain");
  cmDspArgSetupNull(  a+argCnt); // set terminating arg. flag

  // instantiate the object
  cmDspAutoGain_t* p  = cmDspInstAlloc(cmDspAutoGain_t,ctx,classPtr,a,instSymId,id,storeSymId,va_cnt,vl1);

  // assign the current gain coefficients
  for(i=0; i<chCnt; ++i)
    cmDspSetDefaultDouble( ctx, &p->inst, gainBaseAgId + i, 0.0, 1.0);


  // allocate the auto gain calculation proc
  p->agp         = cmAutoGainAlloc(ctx->cmProcCtx,NULL,0,0,0,0,NULL);
  p->chCnt       = chCnt;
  p->chIdx       = cmInvalidIdx;
  p->gainBaseAgId= gainBaseAgId;
  p->startSymId  = cmSymTblRegisterStaticSymbol(ctx->stH,"start");
  p->procSymId   = cmSymTblRegisterStaticSymbol(ctx->stH,"proc");
  p->cancelSymId = cmSymTblRegisterStaticSymbol(ctx->stH,"cancel");
  p->printSymId  = cmSymTblRegisterStaticSymbol(ctx->stH,"print");

  cmDspSetDefaultSymbol( ctx, &p->inst, kSelAgId, p->cancelSymId );
  cmDspSetDefaultInt(    ctx, &p->inst, kIdAgId,  0, cmInvalidId );

   va_end(vl1);
 
  return &p->inst;
}


cmDspRC_t _cmDspAutoGainFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspAutoGain_t* p = (cmDspAutoGain_t*)inst;

  cmAutoGainFree(&p->agp);

  return kOkDspRC;
}

cmDspRC_t _cmDspAutoGainReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t     rc = kOkDspRC;

  rc = cmDspApplyAllDefaults(ctx,inst);
  

  return rc;  
}

cmDspRC_t _cmDspAutoGainExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspAutoGain_t* p = (cmDspAutoGain_t*)inst;
  
  unsigned curInChIdx = cmDspInt( inst, kIdAgId);

  if( cmDspSymbol( inst, kSelAgId ) == p->startSymId && curInChIdx != cmInvalidId )
  {
    unsigned          inChVarId  = kInBaseAgId+curInChIdx;
    const cmSample_t* ip      = cmDspAudioBuf(ctx,inst,inChVarId,0);
    unsigned          iSmpCnt = cmDspVarRows(inst,inChVarId);

    cmAutoGainProcCh( p->agp, ip, iSmpCnt );
  }

  return kOkDspRC;
}

cmDspRC_t _cmDspAutoGainInit( cmDspCtx_t* ctx, cmDspInst_t* inst )
{
  cmDspAutoGain_t* p  = (cmDspAutoGain_t*)inst;  
  cmGateDetectParams gd;
  unsigned i;

  // collect the params into a cmGateDetectParams recd
  gd.medCnt      = cmDspUInt(   inst, kMedNAgId );
  gd.avgCnt      = cmDspUInt(   inst, kAvgNAgId );
  gd.suprCnt     = cmDspUInt(   inst, kSupNAgId );
  gd.offCnt      = cmDspUInt(   inst, kOffNAgId );
  gd.suprCoeff   = cmDspDouble( inst, kSupCoefAgId );
  gd.onThreshDb  = cmDspDouble( inst, kOnThrAgId );
  gd.offThreshDb = cmDspDouble( inst, kOffThrAgId );
  
  // setup the internal auto-gain object
  if( cmAutoGainInit(p->agp, cmDspSamplesPerCycle(ctx), cmDspSampleRate(ctx), cmDspDouble(inst,kHopAgId), p->chCnt, &gd ) != cmOkRC )
    return cmDspInstErr(ctx,inst,kSubSysFailDspRC,"The internal auto-gain instance could not be initialized.");

  // send out gain's of 1.0 so that the input audio is not 
  // biased by any existing scaling.
  for(i=0; i<p->chCnt; ++i)
    cmDspSetDouble( ctx, inst, p->gainBaseAgId+i, 1.0);

  return kOkDspRC;
}

cmDspRC_t _cmDspAutoGainRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspAutoGain_t* p  = (cmDspAutoGain_t*)inst;
  cmDspRC_t          rc = kOkDspRC;

  switch( evt->dstVarId )
  {
    case kSelAgId:
      {
        // store the current 'sel' state
        unsigned prvSymId = cmDspSymbol(inst,kSelAgId);

        // update it 'sel' to the new state
        cmDspSetEvent(ctx,inst,evt);

        // get the new state
        unsigned newSymId = cmDspSymbol(inst,kSelAgId);

        //
        // if PRINTing was requested
        //
        if( newSymId == p->printSymId )
        {
          cmRptPrintf(&ctx->cmCtx->rpt,"Auto-Gain Report\n");
          cmAutoGainPrint( p->agp, &ctx->cmCtx->rpt );
          goto doneLabel;
        }


        //
        // if calibration was CANCELLED
        //
        if( newSymId == p->cancelSymId )
        {
          cmDspSetInt( ctx, inst, kIdAgId, cmInvalidId );
          cmRptPrintf(&ctx->cmCtx->rpt,"cancelled\n");
          goto doneLabel;
        }

        //
        // if calibration STARTup was requested - initialize the autogain proc 
        //
        if( newSymId == p->startSymId )
        {
          _cmDspAutoGainInit(ctx,inst);
          cmRptPrintf(&ctx->cmCtx->rpt,"started\n");
          goto doneLabel;
        }

        //
        // if calibration PROCessing was requested
        //
        if( newSymId == p->procSymId && prvSymId == p->startSymId )
        {
          cmRptPrintf(&ctx->cmCtx->rpt,"proc\n");

          // set the current channel id to 'cmInvalidId' to stop calls to 
          // cmAutoGainProcCh() in _cmDspAutoGainExec()
          cmDspSetInt( ctx, inst, kIdAgId, cmInvalidId );

          // update the auto gain coefficients
          if( cmAutoGainCalcGains(p->agp) == cmOkRC )
          {
            // send the new auto gain coefficients to the output ports
            unsigned i;
            for(i=0; i<p->chCnt; ++i)
              cmDspSetDouble( ctx, inst, p->gainBaseAgId+i, p->agp->chArray[i].gain);
          }

          goto doneLabel;
        }
      }
      break;

    case kIdAgId:
      cmDspSetEvent(ctx,inst,evt);
      if( cmDspSymbol(inst,kSelAgId) == p->startSymId )
      {
        cmRptPrintf(&ctx->cmCtx->rpt,"id:%i\n", cmDspInt(inst,kIdAgId));
        cmAutoGainStartCh(p->agp, p->chIdx = cmDspInt(inst,kIdAgId));
      }
      break;

    default:
      { assert(0); }
  }

 doneLabel:
  return rc;
}

struct cmDspClass_str* cmAutoGainClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmAutoGainDC,ctx,"AutoGain",
    NULL,
    _cmDspAutoGainAlloc,
    _cmDspAutoGainFree,
    _cmDspAutoGainReset,
    _cmDspAutoGainExec,
    _cmDspAutoGainRecv,
    NULL,NULL,
    "Auto-gain calibrator.");

  return &_cmAutoGainDC;
}

//==========================================================================================================================================
enum
{
  kHopMsEfId,     // RMS window length in milliseconds
  kMedCntEfId,    // 
  kAvgCntEfId,    // 
  kSuprCntEfId,   // 
  kOffCntEfId,    //
  kSuprCoefEfId, //
  kOnThrDbEfId,   //
  kOffThrDbEfId,  //
  kMaxDbEfId,
  kInEfId,     
  kGateEfId,
  kRmsEfId,
  kLevelEfId,
  kOnEfId,
  kOffEfId
};

cmDspClass_t _cmEnvFollowDC;

typedef struct
{
  cmDspInst_t    inst;
  cmShiftBuf*    sbp;
  cmGateDetect2* gdp;
} cmDspEnvFollow_t;

cmDspInst_t*  _cmDspEnvFollowAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "wnd",   kHopMsEfId,      0, 0,   kInDsvFl  | kDoubleDsvFl   | kOptArgDsvFl,  "RMS Window length"},
    { "med",   kMedCntEfId,     0, 0,   kInDsvFl  | kUIntDsvFl     | kOptArgDsvFl,  "Median filter length." },
    { "avg",   kAvgCntEfId,     0, 0,   kInDsvFl  | kUIntDsvFl     | kOptArgDsvFl,  "Averaging filter length." },
    { "sup",   kSuprCntEfId,    0, 0,   kInDsvFl  | kUIntDsvFl     | kOptArgDsvFl,  "Supression filter length." },
    { "off",   kOffCntEfId,     0, 0,   kInDsvFl  | kUIntDsvFl     | kOptArgDsvFl,  "Offset detection window length" },
    { "supc",  kSuprCoefEfId,   0, 0,   kInDsvFl  | kDoubleDsvFl   | kOptArgDsvFl,  "Suppression shape coefficient" },
    { "ondb",  kOnThrDbEfId,    0, 0,   kInDsvFl  | kDoubleDsvFl   | kOptArgDsvFl,  "Onset threshold dB." },
    { "offdb", kOffThrDbEfId,   0, 0,   kInDsvFl  | kDoubleDsvFl   | kOptArgDsvFl,  "Offset threshold dB" },
    { "maxdb", kMaxDbEfId,      0, 0,   kInDsvFl  | kDoubleDsvFl   | kOptArgDsvFl,  "Max reference dB" },
    { "in",    kInEfId,         0, 0,   kInDsvFl  | kAudioBufDsvFl,                 "Audio input" },
    { "gate",  kGateEfId,       0, 0,   kOutDsvFl | kBoolDsvFl,                     "Gate state output." },
    { "rms",   kRmsEfId,        0, 0,   kOutDsvFl | kDoubleDsvFl,                   "Signal level RMS"},
    { "level", kLevelEfId,      0, 0,   kOutDsvFl | kDoubleDsvFl,                   "Signal level 0.0-1.0 as scale between offset thresh dB and max dB"},
    { "ons",   kOnEfId,         0, 0,   kOutDsvFl | kUIntDsvFl,                     "Onset counter"},
    { "offs",  kOffEfId,        0, 0,   kOutDsvFl | kUIntDsvFl,                     "Offset counter"},
    { NULL, 0, 0, 0, 0 }
  };

  cmDspEnvFollow_t* p  = cmDspInstAlloc(cmDspEnvFollow_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);

  p->sbp   = cmShiftBufAlloc(ctx->cmProcCtx,NULL,0,0,0);
  p->gdp   = cmGateDetectAlloc2(ctx->cmProcCtx,NULL,0,NULL);

  // set default values for the parameters that were not explicitely set in the va_arg list
  cmDspSetDefaultDouble( ctx, &p->inst, kHopMsEfId,       0,     12 );
  cmDspSetDefaultUInt(   ctx, &p->inst, kMedCntEfId,      0,      5 );
  cmDspSetDefaultUInt(   ctx, &p->inst, kAvgCntEfId,      0,      9 );
  cmDspSetDefaultUInt(   ctx, &p->inst, kSuprCntEfId,     0,      6 );
  cmDspSetDefaultUInt(   ctx, &p->inst, kOffCntEfId,      0,      3 );
  cmDspSetDefaultDouble( ctx, &p->inst, kSuprCoefEfId,    0,    1.4 );
  cmDspSetDefaultDouble( ctx, &p->inst, kOnThrDbEfId,     0,    -45 );
  cmDspSetDefaultDouble( ctx, &p->inst, kOffThrDbEfId,    0,    -80 );
  cmDspSetDefaultDouble( ctx, &p->inst, kMaxDbEfId,       0,    -10.0 );
  cmDspSetDefaultUInt(   ctx, &p->inst, kOnEfId,          0,      0 );
  cmDspSetDefaultUInt(   ctx, &p->inst, kOffEfId,         0,      0 );

  return &p->inst;
}

cmDspRC_t _cmDspEnvFollowFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspEnvFollow_t* p = (cmDspEnvFollow_t*)inst;
  cmGateDetectFree2(&p->gdp);
  cmShiftBufFree(&p->sbp);
  return kOkDspRC;
}

cmDspRC_t _cmDspEnvFollowReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspEnvFollow_t* p = (cmDspEnvFollow_t*)inst;
  cmDspRC_t     rc = kOkDspRC;

  if((rc = cmDspApplyAllDefaults(ctx,inst)) != kOkDspRC )
    return rc;

  cmGateDetectParams r;
  r.medCnt      = cmDspUInt(   inst, kMedCntEfId );
  r.avgCnt      = cmDspUInt(   inst, kAvgCntEfId );
  r.suprCnt     = cmDspUInt(   inst, kSuprCntEfId );
  r.offCnt      = cmDspUInt(   inst, kOffCntEfId );
  r.suprCoeff   = cmDspDouble( inst, kSuprCoefEfId );
  r.onThreshDb  = cmDspDouble( inst, kOnThrDbEfId );
  r.offThreshDb = cmDspDouble( inst, kOffThrDbEfId );

  double   sr        = cmDspSampleRate(ctx);
  double   hopSmpCnt = floor(sr * cmDspDouble(inst,kHopMsEfId) / 1000 );
  unsigned wndSmpCnt = floor(r.medCnt * hopSmpCnt);

  if( cmShiftBufInit(p->sbp, cmDspSamplesPerCycle(ctx), wndSmpCnt, hopSmpCnt ) != cmOkRC )
    return cmDspInstErr(ctx,inst,kInstResetFailDspRC,"The gate detector shift buffer initialization failed.");

  if( cmGateDetectInit2(p->gdp, cmDspSamplesPerCycle(ctx), &r ) != cmOkRC )
    return cmDspInstErr(ctx,inst,kInstResetFailDspRC,"The gate detector shift buffer initialization failed.");

  return rc;  
}

cmDspRC_t _cmDspEnvFollowExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspEnvFollow_t* p       = (cmDspEnvFollow_t*)inst;
  unsigned          iChIdx  = 0;
  const cmSample_t* ip      = cmDspAudioBuf(ctx,inst,kInEfId,iChIdx);
  unsigned          iSmpCnt = cmDspVarRows(inst,kInEfId);
  double            maxDb   = cmDspDouble(inst,kMaxDbEfId);
  double            offDb   = cmDspDouble(inst,kOffThrDbEfId);
    
  while( cmShiftBufExec(p->sbp, ip, iSmpCnt ) )
  {
    cmGateDetectExec2(p->gdp,p->sbp->outV, p->sbp->outN );

    // RMS is going out at the audio rate - maybe there should be an option
    // to send it only when the gate changes - this could significantly 
    // cut down on unnecessary transmission if the RMS is only used 
    // when the gate changes
    cmDspSetDouble(ctx,inst,kRmsEfId, p->gdp->rms );

    double rmsDb = p->gdp->rms <  0.00001 ? -100.0 : 20.0 * log10(p->gdp->rms);
    double level = maxDb       <= offDb   ?      0 : fabs((offDb - cmMax( offDb, cmMin( maxDb, rmsDb ))) / (maxDb - offDb));

    cmDspSetDouble(ctx,inst,kLevelEfId, level );

    if( p->gdp->onFl || p->gdp->offFl )
    {
      cmDspSetBool(ctx, inst, kGateEfId, p->gdp->gateFl);    

      if( p->gdp->onFl )
        cmDspSetUInt( ctx, inst, kOnEfId, cmDspUInt(inst,kOnEfId) + 1 );

      if( p->gdp->offFl )
        cmDspSetUInt( ctx, inst, kOffEfId, cmDspUInt(inst,kOffEfId) + 1 );
    }
  }

  return kOkDspRC;
}

cmDspRC_t _cmDspEnvFollowRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t rc;
  cmDspEnvFollow_t* p = (cmDspEnvFollow_t*)inst;

  if((rc = cmDspSetEvent(ctx,inst,evt)) == kOkDspRC )
  {
    switch( evt->dstVarId )
    {
      case kOnThrDbEfId:
        cmGateDetectSetOnThreshDb2(p->gdp, cmDspDouble(inst,kOnThrDbEfId) );
        break;

      case kOffThrDbEfId:
        cmGateDetectSetOffThreshDb2(p->gdp, cmDspDouble(inst,kOffThrDbEfId) );
        break;
    }
    
  }
  return rc;
}

struct cmDspClass_str* cmEnvFollowClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmEnvFollowDC,ctx,"EnvFollow",
    NULL,
    _cmDspEnvFollowAlloc,
    _cmDspEnvFollowFree,
    _cmDspEnvFollowReset,
    _cmDspEnvFollowExec,
    _cmDspEnvFollowRecv,
    NULL,NULL,
    "Envelope follower and gate detector.");

  return &_cmEnvFollowDC;
}

//==========================================================================================================================================
// Fade in and out an arbitrary number of audio signals based on gate signals.
// When the gate is high the signal fades in and when the gate is low the signal fades out.
// Constructor Args:
//    Required: Count of input and output channels.
//    Optional: Fade time in milliseconds.
//
// Inputs:
//    bool   Control gates
//    audio  Input audio.
// Outputs:
//    audio  Output audio
//    double Channel gains.

enum
{
  kChCntXfId,
  kFadeTimeMsXfId,
  kMstrGateXfId,
  kFadeInTimeMsXfId,
  kFadeOutTimeMsXfId,
  kResetXfId,
  kOnXfId,
  kOffXfId,
  kGateBaseXfId,
};

cmDspClass_t _cmXfaderDC;

typedef struct
{
  cmDspInst_t  inst;
  cmXfader*    xfdp;
  unsigned     inBaseXfId;
  unsigned     outBaseXfId;
  unsigned     stateBaseXfId;
  unsigned     gainBaseXfId;
  unsigned     chCnt;
  bool*        chGateV;
  unsigned     onSymId;
  unsigned     offSymId;
} cmDspXfader_t;

cmDspInst_t*  _cmDspXfaderAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "chs",   kChCntXfId,         0, 0,               kUIntDsvFl   | kReqArgDsvFl, "Input and Output channel count"},
    { "ms",    kFadeTimeMsXfId,    0, 0,   kInDsvFl  | kDoubleDsvFl | kOptArgDsvFl, "Fade time in milliseonds."},
    { "mgate", kMstrGateXfId,      0, 0,   kInDsvFl  | kBoolDsvFl   | kOptArgDsvFl, "Master gate - can be used to set all gates."},
    { "ims",   kFadeInTimeMsXfId,  0, 0,   kInDsvFl  | kDoubleDsvFl | kOptArgDsvFl, "Fade in time in milliseonds."},
    { "oms",   kFadeOutTimeMsXfId, 0, 0,   kInDsvFl  | kDoubleDsvFl | kOptArgDsvFl, "Fade out time in milliseonds."},
    { "reset", kResetXfId,         0, 0,   kInDsvFl  | kBoolDsvFl,                  "Jump to gate states rather than fade."},
    { "on",    kOnXfId,            0, 0,   kOutDsvFl | kSymDsvFl,                   "Send 'on' when all ch's transition from off to on."},
    { "off",   kOffXfId,           0, 0,   kOutDsvFl | kSymDsvFl,                   "Send 'off' when all ch's transition from on to off."},
  };

  if( va_cnt < 1 )
  {
    cmDspClassErr(ctx,classPtr,kInvalidArgDspRC,"The Xfader object must be given a channel count argument.");
    return NULL;
  }

  va_list vl1;
  va_copy(vl1,vl);

  unsigned chCnt         = va_arg(vl,int);
  unsigned fixArgCnt     = sizeof(args)/sizeof(args[0]);
  unsigned argCnt        = fixArgCnt + 5*chCnt;
  unsigned inBaseXfId    = kGateBaseXfId + chCnt;
  unsigned outBaseXfId   = inBaseXfId + chCnt;
  unsigned stateBaseXfId = outBaseXfId + chCnt;
  unsigned gainBaseXfId  = stateBaseXfId + chCnt;
  cmDspVarArg_t a[ argCnt+1 ];
  

  // setup the input gate detectors and the output gain args 
  cmDspArgCopy(  a, argCnt, 0, args, fixArgCnt );
  cmDspArgSetupN(ctx, a, argCnt, kGateBaseXfId, chCnt, "gate", kGateBaseXfId, 0, 0, kInDsvFl  | kBoolDsvFl,     "gate flags");
  cmDspArgSetupN(ctx, a, argCnt, inBaseXfId,    chCnt, "in",   inBaseXfId,    0, 0, kInDsvFl  | kAudioBufDsvFl, "audio input");
  cmDspArgSetupN(ctx, a, argCnt, outBaseXfId,   chCnt, "out",  outBaseXfId,   0, 1, kOutDsvFl | kAudioBufDsvFl, "audio output"); 
  cmDspArgSetupN(ctx, a, argCnt, stateBaseXfId, chCnt, "state",stateBaseXfId, 0, 0, kOutDsvFl | kBoolDsvFl,     "current fader state"); 
  cmDspArgSetupN(ctx, a, argCnt, gainBaseXfId,  chCnt, "gain", gainBaseXfId,  0, 0, kOutDsvFl | kDoubleDsvFl,   "gain output"); 
  cmDspArgSetupNull( a+argCnt); // set terminating arg. flag

  cmDspXfader_t* p  = cmDspInstAlloc(cmDspXfader_t,ctx,classPtr,a,instSymId,id,storeSymId,va_cnt,vl1);

  double fadeTimeMs = cmDspDouble(&p->inst, kFadeTimeMsXfId );
  p->xfdp           = cmXfaderAlloc(ctx->cmProcCtx,NULL,cmDspSampleRate(ctx), chCnt, fadeTimeMs);
  p->inBaseXfId     = inBaseXfId;
  p->outBaseXfId    = outBaseXfId;
  p->stateBaseXfId  = stateBaseXfId;
  p->gainBaseXfId   = gainBaseXfId;
  p->chCnt          = chCnt;
  p->chGateV        = cmMemAllocZ(bool,p->chCnt);
  p->onSymId        = cmSymTblRegisterStaticSymbol(ctx->stH,"on");
  p->offSymId       = cmSymTblRegisterStaticSymbol(ctx->stH,"off");

  // set default values for the parameters that were not explicitely set in the va_arg list
  cmDspSetDefaultDouble( ctx, &p->inst, kFadeTimeMsXfId,  0,     100 );
  cmDspSetDefaultBool(   ctx, &p->inst, kMstrGateXfId,    false, false);
  cmDspSetDefaultSymbol( ctx, &p->inst, kOnXfId,    p->onSymId );
  cmDspSetDefaultSymbol( ctx, &p->inst, kOffXfId,   p->offSymId );  

  int i;
  for(i=0; i<chCnt; ++i)
    cmDspSetDefaultBool( ctx, &p->inst, stateBaseXfId+i, false, false );

  va_end(vl1);

  return &p->inst;
}

cmDspRC_t _cmDspXfaderFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspXfader_t* p = (cmDspXfader_t*)inst;
  cmMemFree(p->chGateV);
  cmXfaderFree(&p->xfdp);
  return kOkDspRC;
}

cmDspRC_t _cmDspXfaderReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t      rc = kOkDspRC;
  rc = cmDspApplyAllDefaults(ctx,inst);

  cmDspXfader_t* p  = (cmDspXfader_t*)inst;

  // TODO: zeroing of output audio buffers should be built into cmDspApplyAllDefaults().
  unsigned i;
  for(i=0; i<p->chCnt; ++i)
    cmDspZeroAudioBuf(ctx,inst,p->outBaseXfId + i);

  return rc;  
}

cmDspRC_t _cmDspXfaderExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t      rc = cmOkRC;
  cmDspXfader_t* p  = (cmDspXfader_t*)inst;
  unsigned       i;

  // update the internal cross fader by providing it with new gate settings and generate new gain values
  cmXfaderExec( p->xfdp, cmDspSamplesPerCycle(ctx), p->chGateV, p->chCnt );

  for(i=0; i<p->chCnt; ++i)
  {
    unsigned          n  = cmDspAudioBufSmpCount(ctx,inst,p->outBaseXfId+i,0);
    cmSample_t*       op = cmDspAudioBuf(ctx,inst,p->outBaseXfId+i,0);
    const cmSample_t* ip = cmDspAudioBuf(ctx,inst,p->inBaseXfId+i,0);
    cmSample_t        gain = (cmSample_t)p->xfdp->chArray[i].ep_gain;

    if( op != NULL )
    {
      if( ip == NULL )
        cmVOS_Zero(op,n);
      else
        cmVOS_MultVVS(op,n,ip,gain);
    }

    if( p->xfdp->chArray[i].onFl )
      cmDspSetBool(ctx,inst,p->stateBaseXfId+i,true);

    if( p->xfdp->chArray[i].offFl )
      cmDspSetBool(ctx,inst,p->stateBaseXfId+i,false);

    // send the gain output
    cmDspSetDouble(ctx,inst,p->gainBaseXfId+i,gain);

    /*
    if( gain > 0 )
      printf("(%i %f %i %i %i %f)",
        i,
        gain,
        p->chGateV[i],
        cmDspBool(inst,p->stateBaseXfId+i),
        p->xfdp->chArray[i].gateFl,
        p->xfdp->chArray[i].gain);
    */


  }

  if( p->xfdp->onFl )
    cmDspSetSymbol(ctx,inst,kOnXfId,p->onSymId);

  if( p->xfdp->offFl )
    cmDspSetSymbol(ctx,inst,kOffXfId,p->offSymId);
  
  return rc;
}

cmDspRC_t _cmDspXfaderRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t rc;
  cmDspXfader_t* p = (cmDspXfader_t*)inst;

  if((rc = cmDspSetEvent(ctx,inst,evt)) != kOkDspRC )
    return rc;

  switch( evt->dstVarId )
  {
    case kFadeTimeMsXfId:
      // if this is an xfade time event then transfer the new xfade time to the xfade proc
      cmXfaderSetXfadeTime(p->xfdp,cmDspDouble(inst,kFadeTimeMsXfId));
      break;

    case kMstrGateXfId:
      {
        bool fl = cmDspBool(inst,kMstrGateXfId);
        unsigned i;
        for(i=0; i<p->chCnt; ++i)
          p->chGateV[i] = fl;
      }
      break;

    case kFadeInTimeMsXfId:
      cmXfaderSetXfadeInTime(p->xfdp,cmDspDouble(inst,kFadeInTimeMsXfId));
      break;

    case kFadeOutTimeMsXfId:
      cmXfaderSetXfadeOutTime(p->xfdp,cmDspDouble(inst,kFadeOutTimeMsXfId));
      break;

    case kResetXfId:
      {
        cmXfaderExec( p->xfdp, cmDspSamplesPerCycle(ctx), p->chGateV, p->chCnt );
        cmXfaderJumpToDestinationGain(p->xfdp);
        // force the chGateV[] to match the xfaders state
        int i;
        for(i=0; i<p->chCnt; ++i)
        {
          bool gateFl = p->xfdp->chArray[i].gateFl;
          p->chGateV[i] = gateFl;
          cmDspSetBool(  ctx,inst,p->stateBaseXfId + i, gateFl);
          cmDspSetDouble(ctx,inst,p->gainBaseXfId  + i, gateFl ? 1.0 : 0.0 );          
        }
      }
      break;

  }
  
  // record gate changes into p->chGateV[] for later use in _cmDspXfaderExec().
  if( kGateBaseXfId <= evt->dstVarId && evt->dstVarId < kGateBaseXfId + p->chCnt )
  {
    p->chGateV[ evt->dstVarId - kGateBaseXfId ] = cmDspBool( inst, evt->dstVarId );
  }

  return rc;
}

struct cmDspClass_str* cmXfaderClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmXfaderDC,ctx,"Xfader",
    NULL,
    _cmDspXfaderAlloc,
    _cmDspXfaderFree,
    _cmDspXfaderReset,
    _cmDspXfaderExec,
    _cmDspXfaderRecv,
    NULL,NULL,
    "Cross fade gain generator.");

  return &_cmXfaderDC;
}


//==========================================================================================================================================

enum
{
  kFnCcId,
  kSelCcId,
  kDoneCcId,
  kGainBaseCcId
};

cmDspClass_t _cmChCfgDC;

typedef struct
{
  cmDspInst_t  inst;
  cmChCfg*     ccp;
  unsigned     midiBaseCcId;
  unsigned     hzBaseCcId;
  unsigned     chBaseCcId;
  unsigned     nsflBaseCcId;
  unsigned     nshzBaseCcId;
  unsigned     printSymId;
  unsigned     writeSymId;
  unsigned     nsCmdSymId;
  unsigned     hzCmdSymId;
  unsigned     resetSymId;
} cmDspChCfg_t;

cmDspInst_t*  _cmDspChCfgAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "fn",   kFnCcId,  0, 0,               kStrzDsvFl| kReqArgDsvFl, "Channel configuration JSON file name."},
    { "sel",  kSelCcId, 0, 0,   kInDsvFl  | kSymDsvFl,                "Action selector: print | write | ns | reset"},
    { "done", kDoneCcId,0, 0,   kOutDsvFl | kSymDsvFl,                "Trigger following action."}
  };

  if( va_cnt < 1 )
  {
    cmDspClassErr(ctx,classPtr,kInvalidArgDspRC,"The channel configuration object must be given a file name argument.");
    return NULL;
  }

  va_list vl1;
  va_copy(vl1,vl);

  const cmChar_t* chCfgFn = va_arg(vl,cmChar_t*);

  cmChCfg* ccp = cmChCfgAlloc( ctx->cmProcCtx, NULL, ctx->cmCtx, chCfgFn );

  if( ccp == NULL || ccp->chCnt==0 )
  {
    cmDspClassErr(ctx,classPtr,kInvalidArgDspRC,"The channel configuration object could not be initialized with the file name '%s'.",cmStringNullGuard(chCfgFn));
    return NULL;
  }

  unsigned       chCnt         = ccp->chCnt;
  unsigned       nsChCnt       = ccp->nsChCnt;
  unsigned       fixArgCnt     = sizeof(args)/sizeof(args[0]);
  unsigned       argCnt        = fixArgCnt + 5*chCnt + nsChCnt;
  unsigned       midiBaseCcId  = kGainBaseCcId + chCnt;
  unsigned       hzBaseCcId    = midiBaseCcId  + chCnt;
  unsigned       chBaseCcId    = hzBaseCcId    + chCnt;
  unsigned       nsflBaseCcId  = chBaseCcId    + chCnt;
  unsigned       nshzBaseCcId  = nsflBaseCcId  + chCnt;
  cmDspChCfg_t*  p             = NULL;
  cmDspVarArg_t  a[ argCnt+1 ];
  unsigned       i,j;

  // setup the input gate detectors and the output gain args 
  cmDspArgCopy(  a, argCnt, 0, args, fixArgCnt );
  cmDspArgSetupN(ctx, a, argCnt, kGainBaseCcId, chCnt,   "gain",  kGainBaseCcId, 0, 0, kSendDfltDsvFl | kInDsvFl   | kOutDsvFl | kDoubleDsvFl,  "Gain input and output.");
  cmDspArgSetupN(ctx, a, argCnt, midiBaseCcId,  chCnt,   "midi",  midiBaseCcId,  0, 0, kSendDfltDsvFl | kOutDsvFl  | kUIntDsvFl,                "MIDI pitch output");
  cmDspArgSetupN(ctx, a, argCnt, hzBaseCcId,    chCnt,   "hz",    hzBaseCcId,    0, 0, kSendDfltDsvFl | kOutDsvFl  | kDoubleDsvFl,              "pitch output in Hz");
  cmDspArgSetupN(ctx, a, argCnt, chBaseCcId,    chCnt,   "ch",    chBaseCcId,    0, 0, kSendDfltDsvFl | kOutDsvFl  | kUIntDsvFl  ,              "Audio channel index");
  cmDspArgSetupN(ctx, a, argCnt, nsflBaseCcId,  chCnt,   "nsfl",  nsflBaseCcId,  0, 0,                  kOutDsvFl  | kBoolDsvFl,                "noise shaper enables");
  cmDspArgSetupN(ctx, a, argCnt, nshzBaseCcId,  nsChCnt, "nshz",  nshzBaseCcId,  0, 0,                  kOutDsvFl  | kDoubleDsvFl,              "noise-shaper pitch output in Hz");
  cmDspArgSetupNull( a+argCnt); // set terminating arg. flag

  if((p = cmDspInstAlloc(cmDspChCfg_t,ctx,classPtr,a,instSymId,id,storeSymId,va_cnt,vl1)) == NULL )
    return NULL;
  
  p->ccp           = ccp;
  p->midiBaseCcId  = midiBaseCcId;
  p->hzBaseCcId    = hzBaseCcId;
  p->chBaseCcId    = chBaseCcId;
  p->nsflBaseCcId  = nsflBaseCcId;
  p->nshzBaseCcId  = nshzBaseCcId;
  p->writeSymId    = cmSymTblRegisterStaticSymbol(ctx->stH,"write");
  p->printSymId    = cmSymTblRegisterStaticSymbol(ctx->stH,"print");
  p->nsCmdSymId    = cmSymTblRegisterStaticSymbol(ctx->stH,"ns");
  p->hzCmdSymId    = cmSymTblRegisterStaticSymbol(ctx->stH,"hz");
  p->resetSymId    = cmSymTblRegisterStaticSymbol(ctx->stH,"reset");

  for(i=0,j=0; i<chCnt; ++i)
  {
    double hz = cmMidiToHz(ccp->chArray[i].midi);
    cmDspSetDefaultDouble(ctx, &p->inst, kGainBaseCcId   + i,   0.0, ccp->chArray[i].gain);
    cmDspSetDefaultUInt(  ctx, &p->inst, p->midiBaseCcId + i,   0,   ccp->chArray[i].midi );
    cmDspSetDefaultDouble(ctx, &p->inst, p->hzBaseCcId   + i,   0.0, hz );
    cmDspSetDefaultUInt(  ctx, &p->inst, p->chBaseCcId   + i,   0,   ccp->chArray[i].ch );
    cmDspSetDefaultBool(  ctx, &p->inst, p->nsflBaseCcId+i, false, false );
    if( ccp->chArray[i].nsFl )
    {
      cmDspSetDefaultDouble(ctx,&p->inst, p->nshzBaseCcId+j, 0.0, hz);
      ++j;
    }

  }


  cmDspSetDefaultSymbol(ctx, &p->inst, kDoneCcId, cmInvalidId );
  cmDspSetDefaultSymbol(ctx, &p->inst, kSelCcId,  cmInvalidId );
  
  return &p->inst;
}

cmDspRC_t _cmDspChCfgFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspChCfg_t* p = (cmDspChCfg_t*)inst;
  cmChCfgFree(&p->ccp);
  return kOkDspRC;
}

cmDspRC_t _cmDspChCfgReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t      rc = kOkDspRC;
  rc = cmDspApplyAllDefaults(ctx,inst);
  return rc;  
}


cmDspRC_t _cmDspChCfgRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t     rc;
  cmDspChCfg_t* p = (cmDspChCfg_t*)inst;

  if((rc = cmDspSetEvent(ctx,inst,evt)) == kOkDspRC )
  {
    if( evt->dstVarId == kSelCcId )
    {
      unsigned selId = cmDspSymbol(inst,kSelCcId);

      if( selId == p->resetSymId )
      {
        _cmDspChCfgReset(ctx,inst,evt);
      }
      else
        if( selId == p->hzCmdSymId )
        {
          unsigned i;
          // snd the  hz
          for(i=0; i<p->ccp->chCnt; ++i)
            cmDspSetDouble(ctx,inst,p->hzBaseCcId+i,cmDspDouble(inst,p->hzBaseCcId+i));

        }
        else
        if( selId == p->nsCmdSymId )
        {
          cmRptPrintf(ctx->rpt,"ChCfg:NS\n");

          unsigned i;
          // send the ns flags
          for(i=0; i<p->ccp->chCnt; ++i)
            cmDspSetBool(ctx,inst,p->nsflBaseCcId+i,p->ccp->chArray[i].nsFl);

          // snd the ns hz
          for(i=0; i<p->ccp->nsChCnt; ++i)
            cmDspSetDouble(ctx,inst,p->nshzBaseCcId+i,cmDspDouble(inst,p->nshzBaseCcId+i));

          cmDspSetSymbol(ctx,inst,kDoneCcId,p->nsCmdSymId);

        }
        else

          if( selId == p->printSymId )
          {
            cmRptPrintf(&ctx->cmCtx->rpt,"Channel Cfg Report\n");
            cmChCfgPrint(p->ccp, ctx->rpt );
          }
          else
          {
            if( selId == p->writeSymId )
            {
              unsigned i;

              cmRptPrintf(&ctx->cmCtx->rpt,"writing\n");

              // copy the gain values into the internal chCfg object ...
              for(i=0; i<p->ccp->chCnt; ++i)
                p->ccp->chArray[i].gain = cmDspDouble(inst,kGainBaseCcId+i);
          
              // ... and write the object
              cmChCfgWrite(p->ccp);
            }
          }
    }
  }

  return rc;
}

struct cmDspClass_str* cmChCfgClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmChCfgDC,ctx,"ChCfg",
    NULL,
    _cmDspChCfgAlloc,
    _cmDspChCfgFree,
    _cmDspChCfgReset,
    NULL,
    _cmDspChCfgRecv,
    NULL,NULL,
    "PP Channel Configuration Object.");

  return &_cmChCfgDC;
}

//==========================================================================================================================================
enum
{
  kRsrcCdId,
  kMaxTimeSpanCdId,
  kMinNoteCntCdId,
  kDetectCdId,
  kCountCdId,
  kGateBaseCdId
};

cmDspClass_t _cmChordDetectDC;

typedef struct
{
  cmDspInst_t    inst;
  cmChordDetect* cdp;
  unsigned       rmsBaseCdId;
  unsigned       chCnt; 
  bool*          chGateV;    // chGateV[ chCnt ]
  cmReal_t*      chRmsV;     // chRmsV[  chCnt ]
  unsigned*      chEnaV;     // chEnaV[  chCnt ]
  unsigned       count;
} cmDspChordDetect_t;

cmDspInst_t*  _cmDspChordDetectAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "rsrc",    kRsrcCdId,        0, 0,              kStrzDsvFl   | kReqArgDsvFl, "Channel enable flag array."},
    { "span",    kMaxTimeSpanCdId, 0, 0, kInDsvFl   | kDoubleDsvFl | kOptArgDsvFl, "Max. onset time span."},
    { "notes",   kMinNoteCntCdId,  0, 0, kInDsvFl   | kUIntDsvFl   | kOptArgDsvFl, "Min. note count per chord."},
    { "detect",  kDetectCdId,      0, 0, kOutDsvFl  | kBoolDsvFl,                  "Chord detect flag."},
    { "count",   kCountCdId,       0, 0, kOutDsvFl  | kUIntDsvFl,                  "Count of chords detected since last reset."}
  };

  if( va_cnt < 1 )
  {
    cmDspClassErr(ctx,classPtr,kInvalidArgDspRC,"The chord detector must be given a channel enable flags array resource argument .");
    return NULL;
  }

  va_list vl1;
  va_copy(vl1,vl);

  const cmChar_t* rsrc  = va_arg(vl,const cmChar_t*);
  unsigned*       enaV  = NULL;
  unsigned        chCnt   = 0;

  if( cmDspRsrcUIntArray( ctx->dspH, &chCnt, &enaV, rsrc, NULL ) != kOkDspRC )
  {
    va_end(vl1);
    cmDspClassErr(ctx,classPtr,kInvalidArgDspRC,"The chord detector channel index resource '%s' could not be read.",cmStringNullGuard(rsrc));
    return NULL;
  }

  //cmRptPrintf(ctx->rpt,"cd %s chs:%i\n",rsrc,chCnt);

  unsigned            fixArgCnt   = sizeof(args)/sizeof(args[0]);
  unsigned            argCnt      = fixArgCnt + 2*chCnt;
  unsigned            rmsBaseCdId = kGateBaseCdId + chCnt;
  cmDspVarArg_t       a[ argCnt+1 ];
  unsigned            i;
  cmDspChordDetect_t* p;

  // setup the input gate detectors and the output gain args 
  cmDspArgCopy(       a, argCnt, 0, args, fixArgCnt );
  cmDspArgSetupN(ctx, a, argCnt, kGateBaseCdId, chCnt, "gate", kGateBaseCdId, 0, 0, kInDsvFl | kOutDsvFl | kBoolDsvFl,   "Channel gate input and output.");
  cmDspArgSetupN(ctx, a, argCnt, rmsBaseCdId,   chCnt, "rms",  rmsBaseCdId,   0, 0, kInDsvFl | kOutDsvFl | kDoubleDsvFl, "Channel RMS input and output");
  cmDspArgSetupNull( a+argCnt); // set terminating arg. flag

  if((p = cmDspInstAlloc(cmDspChordDetect_t,ctx,classPtr,a,instSymId,id,storeSymId,va_cnt,vl1)) == NULL )
    return NULL;

  double   dfltMaxTimeSpanMs = 50.0;
  unsigned dfltMinNoteCnt    = 2;
  cmDspSetDefaultDouble( ctx, &p->inst, kMaxTimeSpanCdId, 0.0,   dfltMaxTimeSpanMs );
  cmDspSetDefaultUInt(   ctx, &p->inst, kMinNoteCntCdId,  0,     dfltMinNoteCnt );
  cmDspSetDefaultBool(   ctx, &p->inst, kDetectCdId,      false, false );
  cmDspSetDefaultUInt(   ctx, &p->inst, kCountCdId,       0,     0 );

  for(i=0; i<chCnt; ++i)
  {
    cmDspSetDefaultBool(  ctx, &p->inst, kGateBaseCdId  + i, false, false );
    cmDspSetDefaultDouble(ctx, &p->inst, rmsBaseCdId    + i, 0.0, 0.0 );
  }

  p->cdp           = cmChordDetectAlloc( ctx->cmProcCtx, NULL, cmDspSampleRate(ctx), chCnt, cmDspDouble(&p->inst,kMaxTimeSpanCdId), cmDspUInt(&p->inst,kMinNoteCntCdId) );
  p->rmsBaseCdId   = rmsBaseCdId;
  p->chCnt         = chCnt;
  p->chGateV       = cmMemAllocZ(bool,     chCnt);
  p->chRmsV        = cmMemAllocZ(cmReal_t, chCnt);
  p->chEnaV        = enaV;

  va_end(vl1);

  return &p->inst;
}

cmDspRC_t _cmDspChordDetectFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspChordDetect_t* p = (cmDspChordDetect_t*)inst;
  cmChordDetectFree(&p->cdp);
  cmMemFree(p->chGateV);
  cmMemFree(p->chRmsV);
  return kOkDspRC;
}

cmDspRC_t _cmDspChordDetectReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t      rc = kOkDspRC;
  rc = cmDspApplyAllDefaults(ctx,inst);
  return rc;  
}

cmDspRC_t _cmDspChordDetectExec( cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t           rc = kOkDspRC;
  cmDspChordDetect_t* p = (cmDspChordDetect_t*)inst;

  cmChordDetectExec(p->cdp, cmDspSamplesPerCycle(ctx), p->chGateV, p->chRmsV, p->chCnt );

  if( p->cdp->detectFl )
  {
    unsigned i;
    for(i=0; i<p->chCnt; ++i)
    {
      bool fl = p->cdp->chArray[i].chordFl;

      cmDspSetBool(   ctx, inst, kGateBaseCdId  + i, fl );
      cmDspSetDouble( ctx, inst, p->rmsBaseCdId + i, fl ? p->cdp->chArray[i].candRMS : 0 );
    }

    cmDspSetBool(ctx, inst, kDetectCdId, true);
    cmDspSetUInt(ctx, inst, kCountCdId, cmDspUInt(inst,kCountCdId) + 1 );
  }

  cmVOB_Zero(p->chGateV,p->chCnt);
  cmVOR_Zero(p->chRmsV,p->chCnt);
  return rc;
}

cmDspRC_t _cmDspChordDetectRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t           rc = kOkDspRC;
  cmDspChordDetect_t* p = (cmDspChordDetect_t*)inst;

  if( kGateBaseCdId <= evt->dstVarId && evt->dstVarId < kGateBaseCdId + p->chCnt )
  {
    unsigned idx = evt->dstVarId - kGateBaseCdId;
    if( p->chEnaV[idx] )
      p->chGateV[ idx ] = cmDsvGetBool(evt->valuePtr);

    //cmRptPrintf(ctx->rpt,"cd gate:%i e:%i v:%i\n",idx,p->chEnaV[idx],p->chGateV[idx]);
    
  }
  else
    if( p->rmsBaseCdId <= evt->dstVarId && evt->dstVarId < p->rmsBaseCdId + p->chCnt )
    {
      unsigned idx = evt->dstVarId - p->rmsBaseCdId;
      if( p->chEnaV[idx] )
        p->chRmsV[ idx ] = cmDsvGetReal( evt->valuePtr );
    }
    else
    {
      if((rc = cmDspSetEvent(ctx,inst,evt)) == kOkDspRC )
      {
        switch( evt->dstVarId )
        {
          case kMaxTimeSpanCdId:
            cmChordDetectSetSpanMs(p->cdp,cmDspDouble(inst,kMaxTimeSpanCdId));
            break;

          case kMinNoteCntCdId:
            p->cdp->minNotesPerChord = cmDspUInt(inst,kMinNoteCntCdId);
            break;
        }
      }
    }

  return rc;
}

struct cmDspClass_str* cmChordDetectClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmChordDetectDC,ctx,"ChordDetect",
    NULL,
    _cmDspChordDetectAlloc,
    _cmDspChordDetectFree,
    _cmDspChordDetectReset,
    _cmDspChordDetectExec,
    _cmDspChordDetectRecv,
    NULL,NULL,
    "Chord detector.");

  return &_cmChordDetectDC;
}

//==========================================================================================================================================
enum
{
  kTimeFaId,
  kGateFaId,
  kInFaId,
  kGainFaId,
  kOutFaId
};

cmDspClass_t _cmFaderDC;

typedef struct
{
  cmDspInst_t    inst;
  cmFader*       fdp;
} cmDspFader_t;

cmDspInst_t*  _cmDspFaderAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "time", kTimeFaId, 0, 0, kDoubleDsvFl | kOptArgDsvFl,   "Fade time in milliseconds."},
    { "gate", kGateFaId, 0, 0, kInDsvFl     | kBoolDsvFl,     "Gate control signal."},
    { "in",   kInFaId,   0, 0, kInDsvFl     | kAudioBufDsvFl, "Audio input."},
    { "gain", kGainFaId, 0, 0, kOutDsvFl    | kDoubleDsvFl,   "gain output."},
    { "out",  kOutFaId,  0, 0, kOutDsvFl    | kAudioBufDsvFl, "Audio out."},
    { NULL,   0,         0, 0, 0, NULL }
  };

  cmDspFader_t* p;

  if((p = cmDspInstAlloc(cmDspFader_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl)) == NULL )
    return NULL;

  double dfltFadeTimeMs = 100.0;
  cmDspSetDefaultDouble( ctx, &p->inst, kTimeFaId, 0.0,   dfltFadeTimeMs );

  p->fdp = cmFaderAlloc(ctx->cmProcCtx, NULL, cmDspSampleRate(ctx), dfltFadeTimeMs );

  return &p->inst;
}

cmDspRC_t _cmDspFaderFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspFader_t* p = (cmDspFader_t*)inst;
  cmFaderFree(&p->fdp);
  return kOkDspRC;
}

cmDspRC_t _cmDspFaderReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t     rc = kOkDspRC;
  cmDspFader_t* p  = (cmDspFader_t*)inst;
  rc               = cmDspApplyAllDefaults(ctx,inst);
  cmDspZeroAudioBuf(ctx,inst,kOutFaId);
  cmFaderSetFadeTime(p->fdp,cmDspDouble(inst,kTimeFaId));
  return rc;  
}

cmDspRC_t _cmDspFaderExec( cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t         rc = kOkDspRC;
  cmDspFader_t*     p  = (cmDspFader_t*)inst;

  unsigned          n  = cmDspAudioBufSmpCount(ctx,inst,kOutFaId,0);
  cmSample_t*       op = cmDspAudioBuf(ctx,inst,kOutFaId,0);
  const cmSample_t* ip = cmDspAudioBuf(ctx,inst,kInFaId,0);

  cmFaderExec(p->fdp,n,cmDspBool(inst,kGateFaId),false,ip,op);


  cmDspSetDouble(ctx,inst,kGainFaId,p->fdp->gain);

  return rc;
}

cmDspRC_t _cmDspFaderRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  cmDspRC_t     rc = kOkDspRC;
  cmDspFader_t* p  = (cmDspFader_t*)inst;

  if((rc = cmDspSetEvent(ctx,inst,evt)) == kOkDspRC )
  {
    if( evt->dstVarId == kTimeFaId )
      cmFaderSetFadeTime(p->fdp,cmDspDouble(inst,kTimeFaId));
  }

  return rc;
}

struct cmDspClass_str* cmFaderClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmFaderDC,ctx,"Fader",
    NULL,
    _cmDspFaderAlloc,
    _cmDspFaderFree,
    _cmDspFaderReset,
    _cmDspFaderExec,
    _cmDspFaderRecv,
    NULL,NULL,
    "Audio fade in/out controller.");

  return &_cmFaderDC;
}

//==========================================================================================================================================
enum
{
  kChCntNsId,
  kTrigNsId,
  kDoneNsId,
  kGateBaseNsId
};

enum
{
  kGroupNonNsId,
  kGroup0NsId,
  kGroup1NsId
};

cmDspClass_t _cmNoteSelectDC;

typedef struct
{
  cmDspInst_t    inst;
  unsigned       chCnt; 
  unsigned       rmsBaseNsId;
  unsigned       gate0BaseNsId;
  unsigned       gate1BaseNsId;
  unsigned       gate2BaseNsId;
  unsigned       gate3BaseNsId;
  unsigned       gate4BaseNsId;
  bool*          chGateV;    // chGateV[chCnt]
  cmReal_t*      chRmsV;     // chRmsV[ chCnt ];
  unsigned*      chGroupV;   // chGroupV[ chCnt ] (0=non-chord 1=low/high 2=middle)
  unsigned       count;
  unsigned       doneSymId;
} cmDspNoteSelect_t;

cmDspInst_t*  _cmDspNoteSelectAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "ch_cnt",  kChCntNsId,       0, 0,             kUIntDsvFl   | kReqArgDsvFl, "Channel count."},
    { "trig",    kTrigNsId,        0, 0, kInDsvFl | kBoolDsvFl,                  "Trigger note selection."},
    { "done",    kDoneNsId,        0, 0, kOutDsvFl | kSymDsvFl,                  "Sends 'done' after new set of outputs have been sent."},
  };

  if( va_cnt < 1 )
  {
    cmDspClassErr(ctx,classPtr,kInvalidArgDspRC,"The note selector must be given a channel count argument .");
    return NULL;
  }

  va_list vl1;

  unsigned        CD0chanN = 0;
  unsigned        CD1chanN = 0;
  unsigned*       CD0chan  = NULL;
  unsigned*       CD1chan  = NULL;
  const cmChar_t* CD0rsrc  = "CD0chan";
  const cmChar_t* CD1rsrc  = "CD1chan";

  if( cmDspRsrcUIntArray( ctx->dspH, &CD0chanN, &CD0chan, CD0rsrc, NULL ) != kOkDspRC )
  {
    cmDspClassErr(ctx,classPtr,kInvalidArgDspRC,"The chord detector channel index resource '%s' could not be read.",cmStringNullGuard(CD0rsrc));
    return NULL;
  }

  if( cmDspRsrcUIntArray( ctx->dspH, &CD1chanN, &CD1chan, CD1rsrc, NULL ) != kOkDspRC )
  {
    cmDspClassErr(ctx,classPtr,kInvalidArgDspRC,"The chord detector channel index resource '%s' could not be read.",cmStringNullGuard(CD1rsrc));
    return NULL;
  }

  va_copy(vl1,vl);

  unsigned chCnt         = va_arg(vl,unsigned);
  unsigned fixArgCnt     = sizeof(args)/sizeof(args[0]);
  unsigned argCnt        = fixArgCnt + 7*chCnt;
  unsigned rmsBaseNsId   = kGateBaseNsId + 1 * chCnt;
  unsigned gate0BaseNsId = kGateBaseNsId + 2 * chCnt;
  unsigned gate1BaseNsId = kGateBaseNsId + 3 * chCnt;
  unsigned gate2BaseNsId = kGateBaseNsId + 4 * chCnt;
  unsigned gate3BaseNsId = kGateBaseNsId + 5 * chCnt;
  unsigned gate4BaseNsId = kGateBaseNsId + 6 * chCnt;

  cmDspVarArg_t       a[ argCnt+1 ];
  unsigned            i;
  cmDspNoteSelect_t* p;

  // setup the input gate detectors and the output gain args 
  cmDspArgCopy(  a, argCnt, 0, args, fixArgCnt );
  cmDspArgSetupN(ctx, a, argCnt, kGateBaseNsId, chCnt, "gate",   kGateBaseNsId, 0, 0, kInDsvFl  | kBoolDsvFl,   "Channel gate input.");
  cmDspArgSetupN(ctx, a, argCnt, rmsBaseNsId,   chCnt, "rms",    rmsBaseNsId,   0, 0, kInDsvFl  | kDoubleDsvFl, "Channel RMS input");
  cmDspArgSetupN(ctx, a, argCnt, gate0BaseNsId, chCnt, "gate-0", gate0BaseNsId, 0, 0, kOutDsvFl | kBoolDsvFl,   "Channel gate set 0 output.");
  cmDspArgSetupN(ctx, a, argCnt, gate1BaseNsId, chCnt, "gate-1", gate1BaseNsId, 0, 0, kOutDsvFl | kBoolDsvFl,   "Channel gate set 1 output.");
  cmDspArgSetupN(ctx, a, argCnt, gate2BaseNsId, chCnt, "gate-2", gate2BaseNsId, 0, 0, kOutDsvFl | kBoolDsvFl,   "Channel gate set 2 output.");
  cmDspArgSetupN(ctx, a, argCnt, gate3BaseNsId, chCnt, "gate-3", gate3BaseNsId, 0, 0, kOutDsvFl | kBoolDsvFl,   "Channel gate set 3 output.");
  cmDspArgSetupN(ctx, a, argCnt, gate4BaseNsId, chCnt, "gate-4", gate4BaseNsId, 0, 0, kOutDsvFl | kBoolDsvFl,   "Channel gate set 4 output.");

  cmDspArgSetupNull( a+argCnt); // set terminating arg. flag

  if((p = cmDspInstAlloc(cmDspNoteSelect_t,ctx,classPtr,a,instSymId,id,storeSymId,va_cnt,vl1)) == NULL )
    return NULL;

  cmDspSetDefaultBool(   ctx, &p->inst, kTrigNsId,      false, false );
  cmDspSetDefaultSymbol( ctx, &p->inst, kDoneNsId,  cmInvalidId );


  p->rmsBaseNsId   = rmsBaseNsId;
  p->gate0BaseNsId = gate0BaseNsId;
  p->gate1BaseNsId = gate1BaseNsId;
  p->gate2BaseNsId = gate2BaseNsId;
  p->gate3BaseNsId = gate3BaseNsId;
  p->gate4BaseNsId = gate4BaseNsId;

  p->chCnt         = chCnt;
  p->chGateV       = cmMemAllocZ(bool,chCnt);
  p->chRmsV        = cmMemAllocZ(cmReal_t,chCnt);
  p->chGroupV      = cmMemAllocZ(unsigned,chCnt);
  p->doneSymId     = cmSymTblRegisterStaticSymbol(ctx->stH,"done");

  for(i=0; i<CD0chanN; ++i)
  {
    if( CD0chan[i] >= chCnt )
      cmDspInstErr(ctx,&p->inst,kInvalidArgDspRC,"The chord detector resource array '%s' value  %i is out of range %i.",cmStringNullGuard(CD0rsrc),CD0chan[i],chCnt);
    else
      p->chGroupV[ CD0chan[i] ] = kGroup0NsId;
  }

  for(i=0; i<CD1chanN; ++i)
  {
    if( CD1chan[i] >= chCnt )
      cmDspInstErr(ctx,&p->inst,kInvalidArgDspRC,"The chord detector resource array '%s' value  %i is out of range %i.",cmStringNullGuard(CD1rsrc),CD1chan[i],chCnt);
    else
      p->chGroupV[ CD1chan[i] ] = kGroup1NsId;
  }

  for(i=0; i<chCnt; ++i)
  {
    cmDspSetDefaultDouble(ctx, &p->inst, rmsBaseNsId+i,   0.0,   0.0 );
    cmDspSetDefaultBool(  ctx, &p->inst, gate0BaseNsId+i, false, false );
    cmDspSetDefaultBool(  ctx, &p->inst, gate1BaseNsId+i, false, false );
    cmDspSetDefaultBool(  ctx, &p->inst, gate2BaseNsId+i, false, false );   
    cmDspSetDefaultBool(  ctx, &p->inst, gate3BaseNsId+i, false, false );
    // the non-chord channel selections should always be on
    cmDspSetDefaultBool(  ctx, &p->inst, gate4BaseNsId+i, false, p->chGroupV[i] == kGroupNonNsId );   
  }

  va_end(vl1);

  return &p->inst;
}

cmDspRC_t _cmDspNoteSelectFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspNoteSelect_t* p = (cmDspNoteSelect_t*)inst;
  cmMemFree(p->chGateV);
  cmMemFree(p->chRmsV);
  cmMemFree(p->chGroupV);
  return kOkDspRC;
}

cmDspRC_t _cmDspNoteSelectReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t      rc = kOkDspRC;
  cmDspNoteSelect_t* p = (cmDspNoteSelect_t*)inst;
  rc = cmDspApplyAllDefaults(ctx,inst);
  cmVOR_Zero(p->chRmsV,p->chCnt);
  return rc;  
}


cmDspRC_t _cmDspNoteSelectRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t           rc = kOkDspRC;
  cmDspNoteSelect_t* p = (cmDspNoteSelect_t*)inst;

  // store incoming gate  values
  if( kGateBaseNsId <= evt->dstVarId && evt->dstVarId < kGateBaseNsId + p->chCnt )
    p->chGateV[ evt->dstVarId - kGateBaseNsId ] = cmDsvGetBool(evt->valuePtr);
  else
    // store incoming RMS values
    if( p->rmsBaseNsId <= evt->dstVarId && evt->dstVarId < p->rmsBaseNsId + p->chCnt )
      p->chRmsV[ evt->dstVarId - p->rmsBaseNsId ] = cmDsvGetReal( evt->valuePtr );
    else
    {
      // if a chord detection was triggered
      if((rc = cmDspSetEvent(ctx,inst,evt)) == kOkDspRC && evt->dstVarId == kTrigNsId )
      {
        unsigned i;
        cmReal_t maxRms = 0;
        unsigned maxIdx = cmInvalidIdx;

        for(i=1; i<p->chCnt; ++i)
        {
          // if this channel had an onset and is a possible chord note and is the max RMS chord note
          if(  p->chGroupV[i] != kGroupNonNsId && p->chGateV[i] &&  (maxIdx==cmInvalidIdx || p->chRmsV[i] > maxRms) )
          {
            maxRms = p->chRmsV[i];
            maxIdx = i;
          }
        }

        for(i=0; i<p->chCnt; ++i)
        {
          bool fl       = p->chGroupV[i] != kGroupNonNsId;
          bool chosenFl = fl && i==maxIdx;
          bool otherFl  = fl && i!=maxIdx && p->chGateV[i];
          bool cd0Fl    = p->chGroupV[i]==kGroup0NsId && (!otherFl) && (!chosenFl);
          bool cd1Fl    = p->chGroupV[i]==kGroup1NsId && (!otherFl) &&  (!chosenFl);

          // gate set 0: set output gate for max chord note
          cmDspSetBool(ctx,inst,p->gate0BaseNsId+i, chosenFl );        

          // gate set 1: set output gate for non-max chord notes
          cmDspSetBool(ctx,inst,p->gate1BaseNsId+i, otherFl );                  

          // gate set 2: set output gate for non-chord notes
          cmDspSetBool(ctx,inst,p->gate2BaseNsId+i, cd0Fl );

          // gate set 3: set output gate for non-chord notes
          cmDspSetBool(ctx,inst,p->gate3BaseNsId+i, cd1Fl);

          // gate set 4: set output gate for non-chord notes
          cmDspSetBool(ctx,inst,p->gate4BaseNsId+i, !fl );

        }

        // send the 'done' symbol to notify the gate receivers that the
        // new set of gates is complete
        cmDspSetSymbol(ctx,inst,kDoneNsId, p->doneSymId);

        // zero the RMS vector
        cmVOR_Zero(p->chRmsV,p->chCnt);
      }
    }

  return rc;
}

struct cmDspClass_str* cmNoteSelectClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmNoteSelectDC,ctx,"NoteSelect",
    NULL,
    _cmDspNoteSelectAlloc,
    _cmDspNoteSelectFree,
    _cmDspNoteSelectReset,
    NULL,
    _cmDspNoteSelectRecv,
    NULL,NULL,
    "Chord detector.");

  return &_cmNoteSelectDC;
}

//==========================================================================================================================================

enum
{
  kTrigNnId,
  kDoneNnId,
  kGateBaseNnId
};


cmDspClass_t _cmNetNoteSelectDC;

#define _cmNetNoteSelPortCnt (10)


typedef struct
{
  cmDspInst_t    inst;
  unsigned       chCnt; 
  unsigned       rmsBaseNnId;

  unsigned       gateBaseNNId[ _cmNetNoteSelPortCnt ];


  bool*     chGateV;            // chGateV[chCnt]
  cmReal_t* chRmsV;             // chRmsV[ chCnt ];
  unsigned* portChCntV;         // portChCntV[ 10 ]
  unsigned* portBaseIdV;        // portBaseIdV[ 10 ]
  unsigned* chPortV;            // chPortV[ chCnt ]
  unsigned* chPortIdxV;         // chPortIdxV[ chCnt ]
  unsigned* ncPortV;            // ncPortV[ chCnt ]
  unsigned* ncPortIdxV;         // ncPortIdxV[ chCnt ]

  unsigned       count;
  unsigned       doneSymId;
} cmDspNetNoteSelect_t;

cmDspInst_t*  _cmDspNetNoteSelectAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{

  unsigned        chCnt         = 0;
  const cmChar_t* label         = NULL;
  const cmChar_t* chPortRsrc    = "nsChSelChV";
  unsigned*       chPortV       = NULL;
  const cmChar_t* chPortIdxRsrc = "nsChSelChIdxV";
  unsigned*       chPortIdxV    = NULL;
  const cmChar_t* ncPortRsrc    = "nsNcSelChV";
  unsigned*       ncPortV       = NULL;
  const cmChar_t* ncPortIdxRsrc = "nsNcSelChIdxV";
  unsigned*       ncPortIdxV    = NULL;

  unsigned i,j,n;

  cmDspVarArg_t args[] =
  {
    { "trig",    kTrigNnId,        0, 0, kInDsvFl  | kBoolDsvFl,                  "Trigger note selection."},
    { "done",    kDoneNnId,        0, 0, kOutDsvFl | kSymDsvFl,                   "Sends 'done' after new set of outputs have been sent."},
  };

 
  if( cmDspRsrcUIntArray( ctx->dspH, &chCnt, &chPortV, label = chPortRsrc, NULL ) != kOkDspRC )
  {
    cmDspClassErr(ctx,classPtr,kRsrcNotFoundDspRC,"The resource '%s' could not be read.",label);
    return NULL;
  }

  if( cmDspRsrcUIntArray( ctx->dspH, &n, &chPortIdxV, label = chPortIdxRsrc, NULL ) != kOkDspRC )
  {
    cmDspClassErr(ctx,classPtr,kRsrcNotFoundDspRC,"The resource '%s' could not be read.",label);
    return NULL;
  }

  assert(n == chCnt );

  if( cmDspRsrcUIntArray( ctx->dspH, &n, &ncPortV, label = ncPortRsrc, NULL ) != kOkDspRC )
  {
    cmDspClassErr(ctx,classPtr,kRsrcNotFoundDspRC,"The resource '%s' could not be read.",label);
    return NULL;
  }

  assert(n == chCnt );

  if( cmDspRsrcUIntArray( ctx->dspH, &n, &ncPortIdxV, label = ncPortIdxRsrc, NULL ) != kOkDspRC )
  {
    cmDspClassErr(ctx,classPtr,kRsrcNotFoundDspRC,"The resource '%s' could not be read.",label);
    return NULL;
  }

  assert(n == chCnt );

  unsigned fixArgCnt     = sizeof(args)/sizeof(args[0]);
  unsigned rmsBaseNnId   = kGateBaseNnId + chCnt;

  // get the count of ch's on each port
  unsigned* portChCntV  = cmLhAllocZ( ctx->lhH, unsigned, _cmNetNoteSelPortCnt );
  unsigned* portBaseIdV = cmLhAllocZ( ctx->lhH, unsigned, _cmNetNoteSelPortCnt );
  for(i=0; i<_cmNetNoteSelPortCnt; ++i)
  {
    // get the count of ch's in the ith gate output port
    portChCntV[i] = cmVOU_Count( chPortV, chCnt, i ) + cmVOU_Count( ncPortV, chCnt, i );

    // ports 1 and 6 are duplicates of ports 0 and 5
    if( i == 1 || i == 6 )
      portChCntV[i] = portChCntV[i-1];

    // set the base port id for this port
    if( i > 0 )
      portBaseIdV[i] = portBaseIdV[i-1] + portChCntV[i-1];
    else
      portBaseIdV[ i ] = rmsBaseNnId + chCnt;

  }


  unsigned argCnt  = fixArgCnt + (2*chCnt) + cmVOU_Sum(portChCntV,_cmNetNoteSelPortCnt );
  cmDspVarArg_t       a[ argCnt+1 ];
  cmDspNetNoteSelect_t* p;

  // setup the input gate detectors and the output gain args 
  cmDspArgCopy(  a, argCnt, 0, args, fixArgCnt );
  cmDspArgSetupN(ctx, a, argCnt, kGateBaseNnId, chCnt,   "gate",   kGateBaseNnId, 0, 0, kInDsvFl  | kBoolDsvFl,   "Channel gate input.");
  cmDspArgSetupN(ctx, a, argCnt, rmsBaseNnId,   chCnt,   "rms",    rmsBaseNnId,   0, 0, kInDsvFl  | kDoubleDsvFl, "Channel RMS input");

  cmDspArgSetupN(ctx, a, argCnt, portBaseIdV[0], portChCntV[0], "gate-0", portBaseIdV[0], 0, 0, kOutDsvFl | kBoolDsvFl,   "Channel gate set 0 output.");
  cmDspArgSetupN(ctx, a, argCnt, portBaseIdV[1], portChCntV[1], "gate-1", portBaseIdV[1], 0, 0, kOutDsvFl | kBoolDsvFl,   "Channel gate set 1 output.");
  cmDspArgSetupN(ctx, a, argCnt, portBaseIdV[2], portChCntV[2], "gate-2", portBaseIdV[2], 0, 0, kOutDsvFl | kBoolDsvFl,   "Channel gate set 2 output.");
  cmDspArgSetupN(ctx, a, argCnt, portBaseIdV[3], portChCntV[3], "gate-3", portBaseIdV[3], 0, 0, kOutDsvFl | kBoolDsvFl,   "Channel gate set 3 output.");
  cmDspArgSetupN(ctx, a, argCnt, portBaseIdV[4], portChCntV[4], "gate-4", portBaseIdV[4], 0, 0, kOutDsvFl | kBoolDsvFl,   "Channel gate set 4 output.");

  cmDspArgSetupN(ctx, a, argCnt, portBaseIdV[5], portChCntV[5], "gate-5", portBaseIdV[5], 0, 0, kOutDsvFl | kBoolDsvFl,   "Channel gate set 5 output.");
  cmDspArgSetupN(ctx, a, argCnt, portBaseIdV[6], portChCntV[6], "gate-6", portBaseIdV[6], 0, 0, kOutDsvFl | kBoolDsvFl,   "Channel gate set 6 output.");
  cmDspArgSetupN(ctx, a, argCnt, portBaseIdV[7], portChCntV[7], "gate-7", portBaseIdV[7], 0, 0, kOutDsvFl | kBoolDsvFl,   "Channel gate set 7 output.");
  cmDspArgSetupN(ctx, a, argCnt, portBaseIdV[8], portChCntV[8], "gate-8", portBaseIdV[8], 0, 0, kOutDsvFl | kBoolDsvFl,   "Channel gate set 8 output.");
  cmDspArgSetupN(ctx, a, argCnt, portBaseIdV[9], portChCntV[9], "gate-9", portBaseIdV[9], 0, 0, kOutDsvFl | kBoolDsvFl,   "Channel gate set 9 output.");

  cmDspArgSetupNull( a+argCnt); // set terminating arg. flag

  if((p = cmDspInstAlloc(cmDspNetNoteSelect_t,ctx,classPtr,a,instSymId,id,storeSymId,0,vl)) == NULL )
    return NULL;

  cmDspSetDefaultBool(   ctx, &p->inst, kTrigNnId,      false, false );
  cmDspSetDefaultSymbol( ctx, &p->inst, kDoneNnId,  cmInvalidId );


  p->rmsBaseNnId   = rmsBaseNnId;

  p->chCnt       = chCnt;
  p->chGateV     = cmMemAllocZ(bool,chCnt);
  p->chRmsV      = cmMemAllocZ(cmReal_t,chCnt);
  p->portChCntV  = portChCntV;
  p->portBaseIdV = portBaseIdV;
  p->doneSymId   = cmSymTblRegisterStaticSymbol(ctx->stH,"done");
  p->chPortV     = chPortV;
  p->chPortIdxV  = chPortIdxV;
  p->ncPortV     = ncPortV;
  p->ncPortIdxV  = ncPortIdxV;
  
  for(i=0; i<chCnt; ++i)
  {
    cmDspSetDefaultBool(  ctx, &p->inst, kGateBaseNnId+i, false, false );
    cmDspSetDefaultDouble(ctx, &p->inst, rmsBaseNnId+i,   0.0,   0.0 );    
  }

  for(i=0; i<_cmNetNoteSelPortCnt; ++i)
    for(j=0; j<p->portChCntV[i]; ++j)
      cmDspSetDefaultBool( ctx, &p->inst, p->portBaseIdV[i]+j, false, false );

  return &p->inst;
}

cmDspRC_t _cmDspNetNoteSelectFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspNetNoteSelect_t* p = (cmDspNetNoteSelect_t*)inst;
  cmMemFree(p->chGateV);
  cmMemFree(p->chRmsV);
  return kOkDspRC;
}

cmDspRC_t _cmDspNetNoteSelectReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t      rc = kOkDspRC;
  cmDspNetNoteSelect_t* p = (cmDspNetNoteSelect_t*)inst;
  rc = cmDspApplyAllDefaults(ctx,inst);
  cmVOR_Zero(p->chRmsV,p->chCnt);
  return rc;  
}


cmDspRC_t _cmDspNetNoteSelectRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t           rc = kOkDspRC;
  cmDspNetNoteSelect_t* p = (cmDspNetNoteSelect_t*)inst;

  // store incoming gate  values
  if( kGateBaseNnId <= evt->dstVarId && evt->dstVarId < kGateBaseNnId + p->chCnt )
  {
    p->chGateV[ evt->dstVarId - kGateBaseNnId ] = cmDsvGetBool(evt->valuePtr);

    //unsigned idx = evt->dstVarId - kGateBaseNnId;
    //cmRptPrintf(ctx->rpt,"ns gate:%i %i\n",idx, p->chGateV[ idx ]);
  }
  else
    // store incoming RMS values
    if( p->rmsBaseNnId <= evt->dstVarId && evt->dstVarId < p->rmsBaseNnId + p->chCnt )
    {
      p->chRmsV[ evt->dstVarId - p->rmsBaseNnId ] = cmDsvGetReal( evt->valuePtr );
    }
    else
    {
      // if a chord detection was triggered
      if((rc = cmDspSetEvent(ctx,inst,evt)) == kOkDspRC && evt->dstVarId == kTrigNnId )
      {
        unsigned i;
        cmReal_t maxRms = 0;
        unsigned maxIdx = cmInvalidIdx;

        for(i=1; i<p->chCnt; ++i)
        {
          // if this channel had an onset and is a possible chord note and is the max RMS chord note
          if(  p->chGateV[i] &&  (maxIdx==cmInvalidIdx || p->chRmsV[i] > maxRms) )
          {
            maxRms = p->chRmsV[i];
            maxIdx = i;
          }
        }

        for(i=0; i<p->chCnt; ++i)
        {
          bool chosenFl = i==maxIdx;
          bool otherFl  = i!=maxIdx && p->chGateV[i];
          bool nonFl    = chosenFl==false && otherFl==false;
          unsigned k;

          // if this is a chord channel
          if( p->chPortV[i] != cmInvalidIdx )
          {
            // get the port associated with this chord note
            k = p->chPortV[i];
            
            assert( k+1 < _cmNetNoteSelPortCnt );
            assert( p->chPortIdxV[i] < p->portChCntV[k] && p->chPortIdxV[i] < p->portChCntV[k+1] );

            // set the chosen and other gate outputs based on the state of 
            // chosenFl and otherFl
            cmDspSetBool(ctx,inst,p->portBaseIdV[ k   ] + p->chPortIdxV[i],chosenFl);
            cmDspSetBool(ctx,inst,p->portBaseIdV[ k+1 ] + p->chPortIdxV[i],otherFl);            
          }

          // all channels have a 'single' note channel
          assert( p->ncPortV[i] != cmInvalidIdx );
          
          k = p->ncPortV[i];

          assert( k < _cmNetNoteSelPortCnt );
          assert( p->ncPortIdxV[i] < p->portChCntV[k] );
          
          cmDspSetBool(ctx,inst,p->portBaseIdV[k] + p->ncPortIdxV[i],nonFl);

        }

        // send the 'done' symbol to notify the gate receivers that the
        // new set of gates is complete
        cmDspSetSymbol(ctx,inst,kDoneNnId, p->doneSymId);

        // zero the RMS vector
        cmVOR_Zero(p->chRmsV,p->chCnt);
      }
    }

  return rc;
}

struct cmDspClass_str* cmNetNoteSelectClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmNetNoteSelectDC,ctx,"NetNoteSelect",
    NULL,
    _cmDspNetNoteSelectAlloc,
    _cmDspNetNoteSelectFree,
    _cmDspNetNoteSelectReset,
    NULL,
    _cmDspNetNoteSelectRecv,
    NULL,NULL,
    "Chord detector.");

  return &_cmNetNoteSelectDC;
}


//==========================================================================================================================================
enum
{
  kBypassCfId,
  kMinHzCfId,
  kFbFlCfId,
  kHzCfId,
  kAlphaCfId,
  kInCfId,
  kOutCfId
};

cmDspClass_t _cmCombFiltDC;

typedef struct
{
  cmDspInst_t    inst;
  cmCombFilt*    cfp;
} cmDspCombFilt_t;

cmDspInst_t*  _cmDspCombFiltAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "bypass",kBypassCfId, 0, 0, kInDsvFl | kBoolDsvFl   | kReqArgDsvFl,   "Bypass enable flag." },
    { "minhz", kMinHzCfId,  0, 0,            kDoubleDsvFl | kReqArgDsvFl,   "Minimum frequency limit."},
    { "fb",    kFbFlCfId,   0, 0, kInDsvFl | kBoolDsvFl   | kReqArgDsvFl,   "Configure the filter in feedback mode."},
    { "hz",    kHzCfId,     0, 0, kInDsvFl | kDoubleDsvFl | kReqArgDsvFl,   "Lowest comb frequency." },
    { "alpha", kAlphaCfId,  0, 0, kInDsvFl | kDoubleDsvFl | kReqArgDsvFl,   "Filter coefficent."},
    { "in",    kInCfId,     0, 0, kInDsvFl | kAudioBufDsvFl,                "Audio input."},
    { "out",   kOutCfId,    0, 1, kOutDsvFl| kAudioBufDsvFl,                "Audio out."},
    { NULL,    0,           0, 0, 0, NULL }
  };

  cmDspCombFilt_t* p;

  if((p = cmDspInstAlloc(cmDspCombFilt_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl)) == NULL )
    return NULL;

  p->cfp = cmCombFiltAlloc(ctx->cmProcCtx, NULL, 
    cmDspSampleRate(ctx), 
    cmDspBool(&p->inst,kFbFlCfId),
    cmDspDouble(&p->inst,kMinHzCfId), 
    cmDspDouble(&p->inst,kAlphaCfId), 
    cmDspDouble(&p->inst,kHzCfId),
    cmDspBool(&p->inst,kBypassCfId));

  return &p->inst;
}

cmDspRC_t _cmDspCombFiltFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspCombFilt_t* p = (cmDspCombFilt_t*)inst;
  cmCombFiltFree(&p->cfp);
  return kOkDspRC;
}

cmDspRC_t _cmDspCombFiltReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t        rc = kOkDspRC;
  cmDspCombFilt_t* p = (cmDspCombFilt_t*)inst;
  rc               = cmDspApplyAllDefaults(ctx,inst);
  cmDspZeroAudioBuf(ctx,inst,kOutCfId);

  cmCombFiltInit(p->cfp, 
    cmDspSampleRate(ctx), 
    cmDspBool(inst,kFbFlCfId),
    cmDspDouble(inst,kMinHzCfId), 
    cmDspDouble(inst,kAlphaCfId), 
    cmDspDouble(inst,kHzCfId),
    cmDspBool(inst,kBypassCfId));

  return rc;  
}

cmDspRC_t _cmDspCombFiltExec( cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t         rc = kOkDspRC;
  cmDspCombFilt_t*  p  = (cmDspCombFilt_t*)inst;

  unsigned          n  = cmDspAudioBufSmpCount(ctx,inst,kOutCfId,0);
  cmSample_t*       op = cmDspAudioBuf(ctx,inst,kOutCfId,0);
  const cmSample_t* ip = cmDspAudioBuf(ctx,inst,kInCfId,0);

  cmCombFiltExec(p->cfp,ip,op,n);

  return rc;
}

cmDspRC_t _cmDspCombFiltRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  cmDspRC_t     rc = kOkDspRC;
  cmDspCombFilt_t* p  = (cmDspCombFilt_t*)inst;

  if((rc = cmDspSetEvent(ctx,inst,evt)) == kOkDspRC )
  {
    switch( evt->dstVarId )
    {
      case kHzCfId:
        cmCombFiltSetHz(p->cfp,cmDspDouble(inst,evt->dstVarId));
        //printf("%s hz:%f\n",cmSymTblLabel(ctx->stH,inst->symId),cmDspDouble(inst,evt->dstVarId));
        break;

      case kAlphaCfId:
        cmCombFiltSetAlpha(p->cfp,cmDspDouble(inst,evt->dstVarId));
        break;

      case kBypassCfId:
        p->cfp->bypassFl = cmDspBool(inst,evt->dstVarId);
        break;
    }
  }

  return rc;
}

struct cmDspClass_str* cmCombFiltClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmCombFiltDC,ctx,"CombFilt",
    NULL,
    _cmDspCombFiltAlloc,
    _cmDspCombFiltFree,
    _cmDspCombFiltReset,
    _cmDspCombFiltExec,
    _cmDspCombFiltRecv,
    NULL,NULL,
    "Comb Filter");

  return &_cmCombFiltDC;
}

//==========================================================================================================================================
enum
{
  kPortCntSoId,
  kOpSoId,
  kOutSoId,
  kBaseOpdSoId
};

cmDspClass_t _cmScalarOpDC;
struct cmDspScalarOp_str;

typedef cmDspRC_t (*_cmDspScalarOpFunc_t)(cmDspCtx_t* ctx, cmDspInst_t* inst );


typedef struct cmDspScalar_str
{
  cmDspInst_t          inst;
  _cmDspScalarOpFunc_t func;
  unsigned             inPortCnt;
} cmDspScalarOp_t;

cmDspRC_t _cmDspScalarOpFuncMult(cmDspCtx_t* ctx, cmDspInst_t* inst )
{
  cmDspScalarOp_t* p = (cmDspScalarOp_t*)inst;
  double val = 1.0;
  unsigned i;
  for(i=0; i<p->inPortCnt; ++i)
    val *= cmDspDouble( inst, kBaseOpdSoId+i );

  cmDspSetDouble( ctx, inst, kOutSoId, val );

  return kOkDspRC;
}

cmDspRC_t _cmDspScalarOpFuncAdd(cmDspCtx_t* ctx, cmDspInst_t* inst )
{
  cmDspScalarOp_t* p = (cmDspScalarOp_t*)inst;
  double val = 0;
  unsigned i;
  for(i=0; i<p->inPortCnt; ++i)
    val += cmDspDouble( inst, kBaseOpdSoId+i );

  cmDspSetDouble( ctx, inst, kOutSoId, val );
  return kOkDspRC;
}

// var args syntax: "<in_port_cnt> <op_string> <opd_label_0> <opd_dflt_val_0>  <opd_label_1> <opd_dflt_val_1> ...  <opd_label_n> <opd_dflt_val_n> 

cmDspInst_t*  _cmDspScalarOpAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =  
    {
      { "cnt",   kPortCntSoId,  0, 0, kUIntDsvFl | kReqArgDsvFl, "Input port count" },
      {  "op",   kOpSoId,       0, 0, kStrzDsvFl | kReqArgDsvFl, "Operation symbol as a string."},
      {  "out",  kOutSoId,      0, 0, kDoubleDsvFl | kOutDsvFl,  "Operation output."},
    };

  cmDspScalarOp_t* p;

  if( va_cnt < 2 )
  {
    cmDspClassErr(ctx,classPtr,kVarArgParseFailDspRC,"The 'ScalarOp' constructor must have a count of input ports and operation identifier string.");
    return NULL;
  }

  va_list vl1;
  va_copy(vl1,vl);

  unsigned           inPortCnt = va_arg(vl,unsigned);
  const cmChar_t*    opIdStr   = va_arg(vl,const cmChar_t*);
  unsigned           fixArgCnt = sizeof(args)/sizeof(args[0]);
  unsigned           argCnt    = fixArgCnt + inPortCnt;
  cmDspVarArg_t      a[ argCnt+1 ];
  double             dfltVal[ inPortCnt ];
  unsigned           i;
  _cmDspScalarOpFunc_t fp = NULL;

  // validate the count of input ports
  if( inPortCnt == 0 )
  {
    cmDspClassErr(ctx,classPtr,kVarNotValidDspRC,"The 'ScalarOp' constructor input port argument must be non-zero.");
    goto errLabel;
  }

  // locate the operation function
  if( strcmp(opIdStr,"*") == 0 )
    fp = _cmDspScalarOpFuncMult;
  else
    if( strcmp(opIdStr,"+") == 0 )
      fp = _cmDspScalarOpFuncAdd;

  // validate the operation function
  if( fp == NULL )
  {
    cmDspClassErr(ctx,classPtr,kVarNotValidDspRC,"The 'ScalarOp' constructor operation string id '%s' did not match a known operation.",cmStringNullGuard(opIdStr));
    goto errLabel;
  }

  // setup the fixed args
  cmDspArgCopy(  a, argCnt, 0, args, fixArgCnt );

  for(i=0; i<inPortCnt; ++i)
  {
    // get the operand label
    const cmChar_t* label = va_arg(vl,const cmChar_t*); 
    
    // get the operand default value
    dfltVal[i]            = va_arg(vl,double);

    // setup the arg recd
    cmDspArgSetup(ctx,a + fixArgCnt + i, label, cmInvalidId, kBaseOpdSoId+i,0,0,kDoubleDsvFl|kInDsvFl,"Operand");
  }
  cmDspArgSetupNull( a+argCnt); // set terminating arg. flag

  if((p = cmDspInstAlloc(cmDspScalarOp_t,ctx,classPtr,a,instSymId,id,storeSymId,2,vl1)) == NULL )
    goto errLabel;

  for(i=0; i<inPortCnt; ++i)
    cmDspSetDefaultDouble(ctx,&p->inst,kBaseOpdSoId+i,0.0,dfltVal[i]);

  p->inPortCnt = inPortCnt;
  p->func      = fp;
  
  va_end(vl1);

  return &p->inst;

 errLabel:
  va_end(vl1);

  return NULL;
}


cmDspRC_t _cmDspScalarOpReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t        rc = kOkDspRC;
  rc               = cmDspApplyAllDefaults(ctx,inst);
  return rc;  
}


cmDspRC_t _cmDspScalarOpRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  cmDspRC_t     rc = kOkDspRC;
  cmDspScalarOp_t* p  = (cmDspScalarOp_t*)inst;

  if((rc = cmDspSetEvent(ctx,inst,evt)) == kOkDspRC )
  {
    if( evt->dstVarId == kBaseOpdSoId )
      p->func(ctx,inst);
  }

  return rc;
}

struct cmDspClass_str* cmScalarOpClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmScalarOpDC,ctx,"ScalarOp",
    NULL,
    _cmDspScalarOpAlloc,
    NULL,
    _cmDspScalarOpReset,
    NULL,
    _cmDspScalarOpRecv,
    NULL,NULL,
    "Scalar Operations");

  return &_cmScalarOpDC;
}

//==========================================================================================================================================

enum
{
  kChCntGsId,
  kGroupCntGsId,
  kChsPerGroupGsId,
  kBaseGateGsId,
};

cmDspClass_t _cmGroupSelDC;

typedef struct
{
  cmDspInst_t  inst;
  unsigned     chCnt;
  unsigned     groupCnt;
  cmGroupSel*  gsp;
  unsigned     baseRmsGsId;
  unsigned     baseOutGsId;
} cmDspGroupSel_t;

cmDspInst_t*  _cmDspGroupSelAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "chCnt",       kChCntGsId,       0, 0, kUIntDsvFl | kReqArgDsvFl,   "Channel count." },
    { "groupCnt",    kGroupCntGsId,    0, 0, kUIntDsvFl | kReqArgDsvFl,   "Group count." },
    { "chsPerGroup", kChsPerGroupGsId, 0, 0, kUIntDsvFl   | kInDsvFl | kReqArgDsvFl, "Channels per group." }
  };

  if( va_cnt < 2 )
  {
    cmDspClassErr(ctx,classPtr,kVarArgParseFailDspRC,"The 'GroupSel' constructor must have a channel and group count.");
    return NULL;
  }

  va_list vl1;
  va_copy(vl1,vl);

  cmDspGroupSel_t* p;
  unsigned         i;
  unsigned         chCnt       = va_arg(vl,unsigned);
  unsigned         groupCnt    = va_arg(vl,unsigned);
  unsigned         outCnt      = chCnt * groupCnt;
  unsigned         fixArgCnt   = sizeof(args)/sizeof(args[0]);
  unsigned         baseRmsGsId = kBaseGateGsId + chCnt;
  unsigned         baseOutGsId = baseRmsGsId   + chCnt;
  unsigned         argCnt      = baseOutGsId   + outCnt;
  cmDspVarArg_t    a[ argCnt + 1 ];


  cmDspArgCopy(  a, argCnt, 0, args, fixArgCnt );
  cmDspArgSetupN(ctx, a, argCnt, kBaseGateGsId, chCnt, "gate",  kBaseGateGsId, 0, 0, kInDsvFl  | kBoolDsvFl,   "Channel gate input.");
  cmDspArgSetupN(ctx, a, argCnt, baseRmsGsId,   chCnt, "rms",   baseRmsGsId,   0, 0, kInDsvFl  | kDoubleDsvFl, "Channel RMS input");

  for(i=0; i<groupCnt; ++i)
  {
    int  labelCharCnt = 31;
    char label[ labelCharCnt + 1 ];
    snprintf(label,labelCharCnt,"gate-%i",i);
    cmDspArgSetupN(ctx, a, argCnt, baseOutGsId + (i*chCnt), chCnt, label, baseOutGsId + (i*chCnt), 0, 0, kOutDsvFl | kBoolDsvFl,   "Output gates");
  }
  cmDspArgSetupNull( a+argCnt); // set terminating arg. flag

  if((p = cmDspInstAlloc(cmDspGroupSel_t,ctx,classPtr,a,instSymId,id,storeSymId,va_cnt,vl1)) == NULL )
  {
    va_end(vl1);
    return NULL;
  }
 
  p->chCnt       = chCnt;
  p->groupCnt    = groupCnt;
  p->gsp         = cmGroupSelAlloc(ctx->cmProcCtx, NULL, 0, 0, 0 );
  p->baseRmsGsId = baseRmsGsId;
  p->baseOutGsId = baseOutGsId;

  for(i=0; i<chCnt; ++i)
  {
    cmDspSetDefaultBool( ctx, &p->inst, kBaseGateGsId, false, false );
    cmDspSetDefaultDouble(ctx,&p->inst, baseRmsGsId,   0.0,   0.0 );
  }
  for(i=0; i<outCnt; ++i)
    cmDspSetDefaultBool( ctx, &p->inst, baseOutGsId, false, false );

  va_end(vl1);

  return &p->inst;
}

cmDspRC_t _cmDspGroupSelFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspGroupSel_t* p = (cmDspGroupSel_t*)inst;
  cmGroupSelFree(&p->gsp);
  return kOkDspRC;
}

cmDspRC_t _cmDspGroupSelReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t        rc = kOkDspRC;
  cmDspGroupSel_t* p  = (cmDspGroupSel_t*)inst;
  rc               = cmDspApplyAllDefaults(ctx,inst);
  cmGroupSelInit(p->gsp,p->chCnt,p->groupCnt,cmDspUInt(&p->inst,kChsPerGroupGsId));
  return rc;  
}

cmDspRC_t _cmDspGroupSelExec( cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t        rc = kOkDspRC;
  cmDspGroupSel_t* p  = (cmDspGroupSel_t*)inst;

  if( cmGroupSelExec(p->gsp) == cmOkRC && p->gsp->updateFl )
  {
    unsigned i,j;
    for(i=0; i<p->groupCnt; ++i)
    {
      cmGroupSelGrp* gp = p->gsp->groupArray + i;

      if( gp->releaseFl )
      {
        for(j=0; j<gp->chIdxCnt; ++j)
          cmDspSetBool(ctx,inst,p->baseOutGsId + (i*p->chCnt) + gp->chIdxArray[j], false);
      }

      if( gp->createFl )
      {
        for(j=0; j<gp->chIdxCnt; ++j)
          cmDspSetBool(ctx,inst,p->baseOutGsId + (i*p->chCnt) + gp->chIdxArray[j], true);
      }
    }
  }

  return rc;
}

cmDspRC_t _cmDspGroupSelRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  cmDspRC_t        rc = kOkDspRC;
  cmDspGroupSel_t* p  = (cmDspGroupSel_t*)inst;

  if((rc = cmDspSetEvent(ctx,inst,evt)) == kOkDspRC )
  {
    if( evt->dstVarId == kChsPerGroupGsId )
      p->gsp->chsPerGroup = cmDspUInt(inst, kChsPerGroupGsId );
    else
      if( kBaseGateGsId <= evt->dstVarId && evt->dstVarId < (kBaseGateGsId + p->chCnt) )
        cmGroupSetChannelGate(p->gsp, evt->dstVarId - kBaseGateGsId, cmDspDouble(inst,evt->dstVarId));
      else
        if( p->baseRmsGsId <= evt->dstVarId && evt->dstVarId < (p->baseRmsGsId + p->chCnt) )
          cmGroupSetChannelRMS(p->gsp, evt->dstVarId - p->baseRmsGsId, cmDspDouble(inst,evt->dstVarId));
  }

  return rc;
}

struct cmDspClass_str* cmGroupSelClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmGroupSelDC,ctx,"GroupSel",
    NULL,
    _cmDspGroupSelAlloc,
    _cmDspGroupSelFree,
    _cmDspGroupSelReset,
    _cmDspGroupSelExec,
    _cmDspGroupSelRecv,
    NULL,NULL,
    "Group selector.");

  return &_cmGroupSelDC;
}


//==========================================================================================================================================

enum
{
  kInChCntNmId,
  kOutChCntNmId,
  kFadeTimeNmId,
  kBaseGateNmId,
};

cmDspClass_t _cmAudioNofM_DC;

typedef struct
{
  cmDspInst_t   inst;
  unsigned      inChCnt;
  unsigned      outChCnt;
  cmAudioNofM*  nmp;
  unsigned      baseInNmId;
  unsigned      baseOutNmId;
  unsigned      baseGainNmId;
} cmDspAudioNofM_t;

cmDspInst_t*  _cmDspAudioNofM_Alloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  if( va_cnt < 2 )
  {
    cmDspClassErr(ctx,classPtr,kVarArgParseFailDspRC,"The 'AudioNofM' constructor must given input and output channel counts.");
    return NULL;
  }

  va_list vl1;
  va_copy(vl1,vl);

  int           inChCnt     = va_arg(vl,int);
  int           outChCnt    = va_arg(vl,int);
  unsigned      baseInNmId  = kBaseGateNmId + inChCnt;
  unsigned      baseOutNmId = baseInNmId    + inChCnt;
  unsigned      baseGainNmId= baseOutNmId   + outChCnt;
  unsigned      i;

  cmDspAudioNofM_t* p = cmDspInstAllocV(cmDspAudioNofM_t,ctx,classPtr,instSymId,id,storeSymId,va_cnt,vl1,
    1,         "ichs",  kInChCntNmId,  0, 0, kUIntDsvFl   | kReqArgDsvFl,            "Input channel count.",
    1,         "ochs",  kOutChCntNmId, 0, 0, kUIntDsvFl   | kReqArgDsvFl,            "Output channel count.", 
    1,         "time",  kFadeTimeNmId, 0, 0, kDoubleDsvFl | kOptArgDsvFl | kInDsvFl, "Fade time in milliseconds.",
    inChCnt,   "gate",  kBaseGateNmId, 0, 0, kBoolDsvFl   | kInDsvFl,                "Gate inputs.",
    inChCnt,   "in",    baseInNmId,    0, 0, kInDsvFl     | kAudioBufDsvFl,          "Audio input",
    outChCnt,  "out",   baseOutNmId,   0, 1, kOutDsvFl    | kAudioBufDsvFl,          "Audio output",
    outChCnt,  "gain",  baseGainNmId,  0, 0, kOutDsvFl    | kDoubleDsvFl,            "Gain output",
    0 );

  cmDspSetDefaultDouble( ctx, &p->inst, kFadeTimeNmId, 0.0, 25.0 );

  p->inChCnt      = inChCnt;
  p->outChCnt     = outChCnt;
  p->nmp          = cmAudioNofMAlloc(ctx->cmProcCtx,NULL,0,0,0,0);
  p->baseInNmId   = baseInNmId;
  p->baseOutNmId  = baseOutNmId;
  p->baseGainNmId = baseGainNmId;

  for(i=0; i<outChCnt; ++i)
    cmDspSetDefaultDouble( ctx, &p->inst, baseGainNmId + i, 0.0, 0.0 );

  va_end(vl1);

  return &p->inst;
}

cmDspRC_t _cmDspAudioNofM_Free(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspAudioNofM_t* p = (cmDspAudioNofM_t*)inst;
  cmAudioNofMFree(&p->nmp);
  return kOkDspRC;
}

cmDspRC_t _cmDspAudioNofM_Reset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t        rc = kOkDspRC;
  cmDspAudioNofM_t* p = (cmDspAudioNofM_t*)inst;
  unsigned i;
  if((rc               = cmDspApplyAllDefaults(ctx,inst)) == kOkDspRC )
  {
    for(i=0; i<p->outChCnt; ++i)
      cmDspZeroAudioBuf(ctx,inst,p->baseOutNmId+i);

    cmAudioNofMInit(p->nmp, cmDspSampleRate(ctx), p->inChCnt, p->outChCnt, cmDspDouble(&p->inst, kFadeTimeNmId ));
  }

  return rc;  
}

cmDspRC_t _cmDspAudioNofM_Exec( cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t         rc = kOkDspRC;
  cmDspAudioNofM_t*  p  = (cmDspAudioNofM_t*)inst;
  unsigned i;
  const cmSample_t* x[ p->inChCnt ];
  cmSample_t*       y[ p->outChCnt ];
  unsigned n;
  
  for(i=0; i<p->inChCnt; ++i)
  {
    if( i==0 )
      n  = cmDspAudioBufSmpCount(ctx,inst,p->baseInNmId+i,0);
    else
    { assert( n == cmDspAudioBufSmpCount(ctx,inst,p->baseInNmId+i,0)); }

    x[i] = cmDspAudioBuf(ctx,inst,p->baseInNmId+i,0);
  }

  for(i=0; i<p->outChCnt; ++i)
  {
    y[i] = cmDspAudioBuf(ctx,inst,p->baseOutNmId+i,0);

    assert( n == cmDspAudioBufSmpCount(ctx,inst,p->baseOutNmId+i,0));

    cmVOS_Zero(y[i],n);
  }

  cmAudioNofMExec(p->nmp,x,p->inChCnt,y,p->outChCnt,n);


  for(i=0; i<p->outChCnt; ++i)
  {
    cmAudioNofM_In* ip = p->nmp->outArray[i].list;
    double         v  = 0;

    for(; ip != NULL; ip=ip->link)
      v += ip->fader->gain;

    cmDspSetDouble(ctx, inst,p->baseGainNmId + i,v );
  }

  return rc;
}

cmDspRC_t _cmDspAudioNofM_Recv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  cmDspRC_t        rc = kOkDspRC;
  cmDspAudioNofM_t* p  = (cmDspAudioNofM_t*)inst;

  if((rc = cmDspSetEvent(ctx,inst,evt)) == kOkDspRC )
  {
    if( kBaseGateNmId <= evt->dstVarId && evt->dstVarId <= kBaseGateNmId + p->inChCnt )
      cmAudioNofMSetChannelGate( p->nmp, evt->dstVarId - kBaseGateNmId, cmDspBool(inst,evt->dstVarId) );
  }

  return rc;
}

struct cmDspClass_str* cmAudioNofMClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmAudioNofM_DC,ctx,"AudioNofM",
    NULL,
    _cmDspAudioNofM_Alloc,
    _cmDspAudioNofM_Free,
    _cmDspAudioNofM_Reset,
    _cmDspAudioNofM_Exec,
    _cmDspAudioNofM_Recv,
    NULL,NULL,
    "Audio N of M Switch");

  return &_cmAudioNofM_DC;
}



//==========================================================================================================================================
enum
{
  kInChCntRmId,
  kBypassRmId,
  kGainRmId,
  kOutRmId,
  kBaseInRmId
};

cmDspClass_t _cmRingModDC;

typedef struct
{
  cmDspInst_t   inst;
  unsigned      inChCnt;
} cmDspRingMod_t;

cmDspInst_t*  _cmDspRingModAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  if( va_cnt < 1 )
  {
    cmDspClassErr(ctx,classPtr,kVarArgParseFailDspRC,"The 'RingMod' constructor must given an input channel counts.");
    return NULL;
  }

  va_list vl1;
  va_copy(vl1,vl);

  int           inChCnt     = va_arg(vl,int);

  cmDspRingMod_t* p = cmDspInstAllocV(cmDspRingMod_t,ctx,classPtr,instSymId,id,storeSymId,va_cnt,vl1,
    1,         "ichs",  kInChCntRmId,  0, 0,                kUIntDsvFl   | kReqArgDsvFl, "Input channel count.",
    1,         "bypass",kBypassRmId,   0, 0, kInDsvFl     | kBoolDsvFl   | kOptArgDsvFl, "Bypass enable",
    1,         "gain",  kGainRmId,     0, 0, kInDsvFl     | kDoubleDsvFl | kOptArgDsvFl, "Output gain (default:1.0)",
    1,         "out",   kOutRmId,      0, 1, kOutDsvFl    | kAudioBufDsvFl,              "Audio output",
    inChCnt,   "in",    kBaseInRmId,   0, 0, kInDsvFl     | kAudioBufDsvFl,              "Audio input",
    0 );

  cmDspSetDefaultBool(   ctx, &p->inst, kBypassRmId, false, false );
  cmDspSetDefaultDouble( ctx, &p->inst, kGainRmId, 0.0, 1.0 );

  p->inChCnt = inChCnt;

  va_end(vl1);

  return &p->inst;
}


cmDspRC_t _cmDspRingModReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t        rc = kOkDspRC;

  if((rc  = cmDspApplyAllDefaults(ctx,inst)) == kOkDspRC )
  {
    cmDspZeroAudioBuf(ctx,inst,kOutRmId);
  }

  return rc;  
}

cmDspRC_t _cmDspRingModExec( cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t         rc       = kOkDspRC;
  cmDspRingMod_t*   p        = (cmDspRingMod_t*)inst;
  unsigned          i,j;
  cmSample_t*       y        = cmDspAudioBuf(ctx,inst,kOutRmId, 0);
  const cmSample_t* x0       = cmDspAudioBuf(ctx,inst,kBaseInRmId,0);
  unsigned          n        = cmDspAudioBufSmpCount(ctx,inst,kOutRmId,0);
  double            gain     = cmDspDouble(inst,kGainRmId);
  bool              bypassFl = cmDspBool(inst,kBypassRmId);

  for(i=1; i<p->inChCnt; ++i)
  {
    assert( n == cmDspAudioBufSmpCount(ctx,inst,kBaseInRmId+i,0)); 

    const cmSample_t* x1 = cmDspAudioBuf(ctx,inst,kBaseInRmId+i,0);

    if( bypassFl )
    {
      for(j=0; j<n; ++j)
        y[j] = x0[j] + x1[j];
    }
    else
    {
      for(j=0; j<n; ++j)
        y[j] = x0[j] * x1[j] * gain;
    }

    x0 = y;
  }


  return rc;
}

cmDspRC_t _cmDspRingModRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  return cmDspSetEvent(ctx,inst,evt);
}

struct cmDspClass_str* cmRingModClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmRingModDC,ctx,"RingMod",
    NULL,
    _cmDspRingModAlloc,
    NULL,
    _cmDspRingModReset,
    _cmDspRingModExec,
    _cmDspRingModRecv,
    NULL,NULL,
    "Ring modulator");

  return &_cmRingModDC;
}

//==========================================================================================================================================
enum
{
  kMaxCntMdId,
  kDelayMdId,
  kInMdId,
  kOutMdId,
};

cmDspClass_t _cmMsgDelayDC;

typedef struct cmDspMsgDelayEle_str
{
  unsigned              outTimeSmp;
  cmDspValue_t          value;
  struct cmDspMsgDelayEle_str* link;
} cmDspMsgDelayEle_t;

typedef struct
{
  cmDspInst_t         inst;
  unsigned            maxCnt;
  cmDspMsgDelayEle_t* array;  // array[maxCnt];
  cmDspMsgDelayEle_t* avail;
  cmDspMsgDelayEle_t* active;
} cmDspMsgDelay_t;

cmDspInst_t*  _cmDspMsgDelayAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspMsgDelay_t* p = cmDspInstAllocV(cmDspMsgDelay_t,ctx,classPtr,instSymId,id,storeSymId,va_cnt,vl,
    1,   "maxcnt",  kMaxCntMdId,   0, 0,              kUIntDsvFl   | kReqArgDsvFl, "Maximum count of elements in the delay",
    1,   "delay",   kDelayMdId,    0, 0, kInDsvFl   | kDoubleDsvFl | kOptArgDsvFl, "Delay time in millisecond.",
    1,   "in",      kInMdId,       0, 0, kInDsvFl   | kTypeDsvMask,                "Msg input",
    1,   "out",     kOutMdId,      0, 0, kOutDsvFl  | kTypeDsvMask,                "Msg input",
    0 );

  if( p == NULL )
    return NULL;

  cmDspSetDefaultDouble( ctx, &p->inst, kDelayMdId, 0.0, 0.0 );
  cmDspSetDefaultBool(   ctx, &p->inst, kOutMdId,   false, false ); 

  p->maxCnt = cmDspUInt(&p->inst,kMaxCntMdId);
  p->array  = cmMemAllocZ(cmDspMsgDelayEle_t, p->maxCnt );
  return &p->inst;
}

cmDspRC_t _cmDspMsgDelayFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspMsgDelay_t* p = (cmDspMsgDelay_t*)inst;
  cmMemFree(p->array);
  return kOkDspRC;
}

// insert a value in the delay list
cmDspRC_t  _cmDspMsgDelayInsert( cmDspCtx_t* ctx, cmDspMsgDelay_t* p, unsigned delayTimeSmp, const cmDspValue_t* valPtr )
{
  cmDspRC_t rc = kOkDspRC;

  // protect against pre-reset calls
  if( p->avail == NULL && p->active == NULL )
    return kOkDspRC;

  // if there are no available delay elements
  if( p->avail == NULL )
    return cmDspInstErr(ctx,&p->inst,kInvalidStateDspRC,"The message delay has exhausted it's internal message store.");

  // we only do the simplest kind of copying to avoid allocating memory
  // TODO: fix this
  if( cmDsvIsMtx(valPtr) ||  cmIsFlag(valPtr->flags,kProxyDsvFl))
    return cmDspInstErr(ctx,&p->inst,kInvalidArgDspRC,"The message delay cannot yet store matrix or proxy types.");

  // get a pointer to the next available element
  cmDspMsgDelayEle_t* np = p->avail;

  // remove the new ele from the avail list
  p->avail = np->link;

  // calc the new ele's exec time
  np->outTimeSmp = ctx->cycleCnt * cmDspSamplesPerCycle(ctx) + delayTimeSmp;

  // copy the msg value into the delay line element
  // TODO: this should be a real copy that supports all types
  np->value      = *valPtr;
 
  cmDspMsgDelayEle_t* ep = p->active;
  cmDspMsgDelayEle_t* pp = NULL;

  // if the active list is empty ...
  if( ep == NULL )
  {
    // ... make the avail element the first on the list
    p->active  = np;
    np->link   = NULL;
  }
  else
  {
    // iterate through the list and find the active links which 
    // the new ele falls between based on its execution time
    while(ep != NULL )
    {
      // ep's exec time is greater than the new ele's exec time
      if( ep->outTimeSmp > np->outTimeSmp )
      {
        
        // insert the new ele in the active list before 'ep'
        if( pp == NULL )
        {
          np->link   = p->active;
          p->active  = np;
        }
        else
        {
          np->link = pp->link;
          pp->link = np;
        }
        break;
      }

      pp = ep;
      ep = ep->link;
    }

    // if the new element is last on the list
    if( ep == NULL )
    {
      assert(pp != NULL && pp->link == NULL);
      pp->link = np;
      np->link = NULL;
    }
  }


  return rc;
}

cmDspRC_t _cmDspMsgDelayReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t        rc = kOkDspRC;
  cmDspMsgDelay_t* p  = (cmDspMsgDelay_t*)inst;

  if((rc  = cmDspApplyDefault(ctx,inst,kDelayMdId)) == kOkDspRC )
  {
    unsigned i;
    unsigned maxCnt = cmDspUInt(inst,kMaxCntMdId);
    p->active = NULL;
    p->avail  = NULL;

    // put all ele's on the available list
    for(i=0; i<maxCnt; ++i)
    {
      p->array[i].link = p->avail;
      p->avail         = p->array + i;      
    }
  }

  return rc;  
}

cmDspRC_t _cmDspMsgDelayExec( cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* e )
{
  cmDspRC_t           rc             = kOkDspRC;
  cmDspMsgDelay_t*    p              = (cmDspMsgDelay_t*)inst;
  unsigned            framesPerCycle = cmDspSamplesPerCycle(ctx);
  unsigned            begTimeSmp     = ctx->cycleCnt * framesPerCycle;
  unsigned            endTimeSmp     = begTimeSmp + framesPerCycle;

  while( p->active != NULL )
  {

    if( p->active->outTimeSmp >= endTimeSmp )
      break;
    
    cmDspMsgDelayEle_t* ep  = p->active;

    // remove the element from the active list and place it on the available list.
    p->active = p->active->link; // advance the active list
    ep->link  = p->avail; // put the cur. element on the avail list
    p->avail  = ep;       // 

    // output the element value
    if((rc = cmDspValueSet(ctx,inst,kOutMdId,&ep->value,0)) != kOkDspRC )
      return cmDspInstErr(ctx,inst,rc,"Message delay output failed.");      

  }

  return rc;
}

cmDspRC_t _cmDspMsgDelayRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  cmDspRC_t rc = kOkDspRC;
  cmDspMsgDelay_t* p = (cmDspMsgDelay_t*)inst;
  switch( evt->dstVarId )
  {
    case kDelayMdId:
      rc =  cmDspSetEvent(ctx,inst,evt);
      break;

    case  kInMdId:
      {
        unsigned delayTimeSmp = floor(cmDspDouble(&p->inst,kDelayMdId) * cmDspSampleRate(ctx) / 1000.0);
        rc = _cmDspMsgDelayInsert(ctx,p,delayTimeSmp,evt->valuePtr);
      }
      break;
  }

  return rc;
}

struct cmDspClass_str* cmMsgDelayClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmMsgDelayDC,ctx,"MsgDelay",
    NULL,
    _cmDspMsgDelayAlloc,
    _cmDspMsgDelayFree,
    _cmDspMsgDelayReset,
    _cmDspMsgDelayExec,
    _cmDspMsgDelayRecv,
    NULL,NULL,
    "Message Delay");

  return &_cmMsgDelayDC;
}

//==========================================================================================================================================
enum
{
  kBegLnId,
  kEndLnId,
  kDurLnId,
  kCmdLnId,
  kOutLnId,
};

cmDspClass_t _cmLineDC;

typedef struct cmDspLineEle_str
{
  unsigned              outTimeSmp;
  cmDspValue_t          value;
  struct cmDspLineEle_str* link;
} cmDspLineEle_t;

typedef struct
{
  cmDspInst_t inst;
  unsigned    onSymId;
  unsigned    offSymId;
  unsigned    resetSymId;
  unsigned    curSmpIdx;
  bool        onFl;
} cmDspLine_t;

cmDspInst_t*  _cmDspLineAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspLine_t* p = cmDspInstAllocV(cmDspLine_t,ctx,classPtr,instSymId,id,storeSymId,va_cnt,vl,
    1,   "beg",   kBegLnId,   0, 0, kInDsvFl   | kDoubleDsvFl | kReqArgDsvFl, "Begin value.",
    1,   "end",   kEndLnId,   0, 0, kInDsvFl   | kDoubleDsvFl | kReqArgDsvFl, "End value.",
    1,   "dur",   kDurLnId,   0, 0, kInDsvFl   | kDoubleDsvFl | kReqArgDsvFl, "Duration (ms)",
    1,   "cmd",   kCmdLnId,   0, 0, kInDsvFl   | kSymDsvFl    | kOptArgDsvFl, "Command: on | off | reset",
    1,   "out",   kOutLnId,   0, 0, kOutDsvFl  | kDoubleDsvFl,                "Output",
    0 );

  if( p == NULL )
    return NULL;

  cmDspSetDefaultDouble( ctx, &p->inst, kOutLnId, 0.0, cmDspDefaultDouble(&p->inst,kBegLnId) );

  p->onSymId    = cmSymTblRegisterStaticSymbol(ctx->stH,"on");
  p->offSymId   = cmSymTblRegisterStaticSymbol(ctx->stH,"off");
  p->resetSymId = cmSymTblRegisterStaticSymbol(ctx->stH,"reset");

  return &p->inst;
}

cmDspRC_t _cmDspLineFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  return kOkDspRC;
}


cmDspRC_t _cmDspLineReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspLine_t* p   = (cmDspLine_t*)inst;
  p->curSmpIdx = 0;
  p->onFl      = false;
  return cmDspApplyAllDefaults(ctx,inst);
}

cmDspRC_t _cmDspLineExec( cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* e )
{
  cmDspRC_t    rc  = kOkDspRC;
  cmDspLine_t* p   = (cmDspLine_t*)inst;
  
  if( p->onFl == false )
    return kOkDspRC;

  double     beg       = cmDspDouble(inst,kBegLnId);
  double     end       = cmDspDouble(inst,kEndLnId);
  double     ms        = cmDspDouble(inst,kDurLnId);
  double     durSmpCnt = floor(ms * cmDspSampleRate(ctx) / 1000);
  double     out       = beg + (end - beg) * p->curSmpIdx / durSmpCnt;
 
  if( beg < end )
  {
    if( out >= end )
    {
      out     = end;
      p->onFl = false;
    }
  }
  else
  {
    if( out <= end )
    {
      out     = end;
      p->onFl = false;
    }
  }

  cmDspSetDouble(ctx,inst,kOutLnId,out);

  p->curSmpIdx += cmDspSamplesPerCycle(ctx);

  return rc;
}

cmDspRC_t _cmDspLineRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  cmDspRC_t rc = kOkDspRC;
  cmDspLine_t* p = (cmDspLine_t*)inst;

  if((rc = cmDspSetEvent(ctx,inst,evt)) == kOkDspRC )
  {
    switch( evt->dstVarId )
    {
      case kCmdLnId:
        {
          unsigned symId = cmDspSymbol(inst,kCmdLnId);

          if( symId == p->onSymId )
            p->onFl = true;
          else
            if( symId == p->offSymId )
              p->onFl = false;
            else
              if( symId == p->resetSymId )
              {
                p->curSmpIdx = 0;
                p->onFl = true;
              }
              
        }
        break;
    }
  }
  return rc;
}

struct cmDspClass_str* cmLineClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmLineDC,ctx,"Line",
    NULL,
    _cmDspLineAlloc,
    _cmDspLineFree,
    _cmDspLineReset,
    _cmDspLineExec,
    _cmDspLineRecv,
    NULL,NULL,
    "Line");

  return &_cmLineDC;
}


//==========================================================================================================================================
enum
{
  kTrigModeAdId,
  kMinLvlAdId,
  kDlyMsAdId,
  kAtkMsAdId,
  kAtkLvlAdId,
  kDcyMsAdId,
  kSusLvlAdId,
  kSusMsAdId,
  kRlsMsAdId,
  kTScaleAdId,
  kAScaleAdId,
  kGateAdId,
  kRmsAdId,
  kOutAdId,
  kCmdAdId
};

cmDspClass_t _cmAdsrDC;

typedef struct
{
  cmDspInst_t  inst;
  cmAdsr*      p;
} cmDspAdsr_t;

cmDspInst_t*  _cmDspAdsrAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspAdsr_t* p = cmDspInstAllocV(cmDspAdsr_t,ctx,classPtr,instSymId,id,storeSymId,va_cnt,vl,
    1,         "trig",  kTrigModeAdId, 0, 0, kBoolDsvFl   | kInDsvFl | kOptArgDsvFl, "Trigger mode (offset ignored).",
    1,         "min",   kMinLvlAdId,   0, 0, kDoubleDsvFl | kInDsvFl | kOptArgDsvFl, "Minimum level (dflt:0.0).",
    1,         "dly",   kDlyMsAdId,    0, 0, kDoubleDsvFl | kInDsvFl | kOptArgDsvFl, "Delay milliseconds.",
    1,         "atk",   kAtkMsAdId,    0, 0, kDoubleDsvFl | kInDsvFl | kOptArgDsvFl, "Attack milliseconds.",
    1,         "alvl",  kAtkLvlAdId,   0, 0, kDoubleDsvFl | kInDsvFl | kOptArgDsvFl, "Attack Level.",
    1,         "dcy",   kDcyMsAdId,    0, 0, kDoubleDsvFl | kInDsvFl | kOptArgDsvFl, "Decay milliseconds.",
    1,         "sus",   kSusLvlAdId,   0, 0, kDoubleDsvFl | kInDsvFl | kOptArgDsvFl, "Sustain Level.",
    1,         "hold",  kSusMsAdId,    0, 0, kDoubleDsvFl | kInDsvFl | kOptArgDsvFl, "Sustain Ms (trig mode only).",
    1,         "rls",   kRlsMsAdId,    0, 0, kDoubleDsvFl | kInDsvFl | kOptArgDsvFl, "Release milliseconds.",
    1,         "tscale",kTScaleAdId,   0, 0, kDoubleDsvFl | kInDsvFl,                "Time scale.",
    1,         "ascale",kAScaleAdId,   0, 0, kDoubleDsvFl | kInDsvFl,                "Amplitude scale.",
    1,         "gate",  kGateAdId,     0, 0, kBoolDsvFl   | kInDsvFl,                "Gate input.",
    1,         "rms",   kRmsAdId,      0, 0, kDoubleDsvFl | kInDsvFl,                "RMS input.",
    1,         "out",   kOutAdId,      0, 0, kDoubleDsvFl | kOutDsvFl,               "Level output.",    
    1,         "cmd",   kCmdAdId,      0, 0, kSymDsvFl    | kInDsvFl,                "Command input.",
    0 );

  cmDspSetDefaultBool(   ctx, &p->inst, kTrigModeAdId, false, false );
  cmDspSetDefaultDouble( ctx, &p->inst, kMinLvlAdId,     0.0, 0.0 );  
  cmDspSetDefaultDouble( ctx, &p->inst, kDlyMsAdId,     0.0, 0.0 );
  cmDspSetDefaultDouble( ctx, &p->inst, kAtkMsAdId,     0.0, 5.0 );
  cmDspSetDefaultDouble( ctx, &p->inst, kAtkLvlAdId,    0.0, 1.0 );
  cmDspSetDefaultDouble( ctx, &p->inst, kDcyMsAdId,     0.0, 10.0 );
  cmDspSetDefaultDouble( ctx, &p->inst, kSusLvlAdId,    0.0, 0.8 );
  cmDspSetDefaultDouble( ctx, &p->inst, kSusMsAdId,    0.0, 50.0);
  cmDspSetDefaultDouble( ctx, &p->inst, kRlsMsAdId,     0.0, 20.0 );
  cmDspSetDefaultDouble( ctx, &p->inst, kTScaleAdId,     0.0, 1.0 );
  cmDspSetDefaultDouble( ctx, &p->inst, kAScaleAdId,     0.0, 1.0 );
  cmDspSetDefaultBool(   ctx, &p->inst, kGateAdId,     false,false);
  cmDspSetDefaultDouble( ctx, &p->inst, kRmsAdId,       0.0, 0.0);
  cmDspSetDefaultDouble( ctx, &p->inst, kOutAdId,       0.0, cmDspDouble(&p->inst,kMinLvlAdId));

  p->p = cmAdsrAlloc(ctx->cmProcCtx,NULL,false,0,0,0,0,0,0,0,0,0); 

  return &p->inst;
}

cmDspRC_t _cmDspAdsrFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspAdsr_t* p = (cmDspAdsr_t*)inst;
  cmAdsrFree(&p->p);
  return kOkDspRC;
}

cmDspRC_t _cmDspAdsrReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t    rc = kOkDspRC;
  cmDspAdsr_t* p  = (cmDspAdsr_t*)inst;

  if((rc  = cmDspApplyAllDefaults(ctx,inst)) == kOkDspRC )
  {
    bool     trigFl= cmDspBool(   inst, kTrigModeAdId );
    cmReal_t minL  = cmDspDouble( inst, kMinLvlAdId );
    cmReal_t dlyMs = cmDspDouble( inst, kDlyMsAdId );
    cmReal_t atkMs = cmDspDouble( inst, kAtkMsAdId );
    cmReal_t atkL  = cmDspDouble( inst, kAtkLvlAdId );
    cmReal_t dcyMs = cmDspDouble( inst, kDcyMsAdId );
    cmReal_t susMs = cmDspDouble( inst, kSusMsAdId );
    cmReal_t susL  = cmDspDouble( inst, kSusLvlAdId );
    cmReal_t rlsMs = cmDspDouble( inst, kRlsMsAdId );

    cmAdsrInit( p->p, cmDspSampleRate(ctx), trigFl, minL, dlyMs, atkMs, atkL, dcyMs, susMs, susL, rlsMs );
 

  }

  return rc;  
}

cmDspRC_t _cmDspAdsrExec( cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t    rc     = kOkDspRC;
  cmDspAdsr_t* p      = (cmDspAdsr_t*)inst;
  bool         gateFl = cmDspBool( inst, kGateAdId );
  //double       rms    = cmDspDouble(inst,kRmsAdId);
  double       tscale = cmDspDouble(inst,kTScaleAdId);
  double       ascale = cmDspDouble(inst,kAScaleAdId);

  // 
  // HACK HACK HACK HACK
  // HACK HACK HACK HACK
  // HACK HACK HACK HACK  see the accompanying hack in cmProc3.c cmAdsrExec()
  // HACK HACK HACK HACK
  // HACK HACK HACK HACK
  //

  /*
  double db  = rms<0.00001 ? -100.0 :  20.0*log10(rms);
  double dbMax = -15.0;
  double dbMin = -58.0;

  db = cmMin(dbMax,cmMax(dbMin,db));
  double scale =  (db - dbMin) / (dbMax-dbMin);
  */

  cmReal_t     out    = cmAdsrExec( p->p, cmDspSamplesPerCycle(ctx), gateFl, tscale, ascale );

  rc = cmDspSetDouble( ctx, inst, kOutAdId, out );
  
  return rc;
}

cmDspRC_t _cmDspAdsrRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  cmDspRC_t    rc     = kOkDspRC;
  cmDspAdsr_t* p      = (cmDspAdsr_t*)inst;

  if((rc =  cmDspSetEvent(ctx,inst,evt)) != kOkDspRC )
    return rc;

  if( evt->dstVarId == kCmdAdId )
  {
    cmAdsrReport(p->p,ctx->rpt);
    return rc;
  }

  cmReal_t v = cmDspDouble(inst,evt->dstVarId);

  switch( evt->dstVarId )
  {
    case kTrigModeAdId:
      p->p->trigModeFl = cmDspBool(inst, kTrigModeAdId);
      break;

    case kMinLvlAdId:
      cmAdsrSetLevel(p->p, v, kDlyAdsrId );
      break;

    case kDlyMsAdId:
      cmAdsrSetTime(p->p, v, kDlyAdsrId );
      break;

    case kAtkMsAdId:
      cmAdsrSetTime(p->p, v, kAtkAdsrId );
      break;

    case kAtkLvlAdId:
      cmAdsrSetLevel(p->p, v, kAtkAdsrId );
      break;

    case kDcyMsAdId:
      cmAdsrSetTime(p->p, v, kDcyAdsrId );
      break;

    case kSusMsAdId:
      cmAdsrSetTime(p->p, v, kSusAdsrId );
      break;

    case kSusLvlAdId:
      cmAdsrSetLevel(p->p, v, kSusAdsrId );
      break;

    case kRlsMsAdId:
      cmAdsrSetTime(p->p, v, kRlsAdsrId );
      break;

      
  }

  return rc;
}

struct cmDspClass_str* cmAdsrClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmAdsrDC,ctx,"Adsr",
    NULL,
    _cmDspAdsrAlloc,
    _cmDspAdsrFree,
    _cmDspAdsrReset,
    _cmDspAdsrExec,
    _cmDspAdsrRecv,
    NULL,NULL,
    "ADSR Envelope Generator");

  return &_cmAdsrDC;
}

//==========================================================================================================================================
enum
{
  kBypassCmId,
  kThreshDbCmId,
  kRatioCmId,
  kAtkMsCmId,
  kRlsMsCmId,
  kInGainCmId,
  kOutGainCmId,
  kWndMsCmId,
  kMaxWndMsCmId,
  kInCmId,
  kOutCmId,
  kEnvCmId
};

cmDspClass_t _cmCompressorDC;

typedef struct
{
  cmDspInst_t   inst;
  cmCompressor* p;
} cmDspCompressor_t;

cmDspInst_t*  _cmDspCompressorAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspCompressor_t* p = cmDspInstAllocV(cmDspCompressor_t,ctx,classPtr,instSymId,id,storeSymId,va_cnt,vl,
    1,   "bypass",kBypassCmId,   0, 0, kInDsvFl  | kBoolDsvFl    | kReqArgDsvFl, "Bypass enable",
    1,   "thr",   kThreshDbCmId, 0, 0, kInDsvFl  | kDoubleDsvFl  | kReqArgDsvFl, "Threshold in dB.",
    1,   "ratio", kRatioCmId,    0, 0, kInDsvFl  | kDoubleDsvFl  | kReqArgDsvFl, "Ratio numerator.",
    1,   "atk",   kAtkMsCmId,    0, 0, kInDsvFl  | kDoubleDsvFl  | kOptArgDsvFl, "Attack milliseconds",
    1,   "rls",   kRlsMsCmId,    0, 0, kInDsvFl  | kDoubleDsvFl  | kOptArgDsvFl, "Release milliseconds",
    1,   "igain", kInGainCmId,   0, 0, kInDsvFl  | kDoubleDsvFl  | kOptArgDsvFl, "Input gain.",
    1,   "ogain", kOutGainCmId,  0, 0, kInDsvFl  | kDoubleDsvFl  | kOptArgDsvFl, "Makeup Gain",
    1,   "wnd",   kWndMsCmId,    0, 0, kInDsvFl  | kDoubleDsvFl  | kOptArgDsvFl, "RMS window milliseconds.",      
    1,   "maxwnd",kMaxWndMsCmId, 0, 0,             kDoubleDsvFl  | kOptArgDsvFl, "Max. RMS window milliseconds.",
    1,   "in",    kInCmId,       0, 0, kInDsvFl  | kAudioBufDsvFl,               "Audio input",
    1,   "out",   kOutCmId,      0, 1, kOutDsvFl | kAudioBufDsvFl,               "Audio output",
    1,   "env",   kEnvCmId,      0, 0, kOutDsvFl | kDoubleDsvFl,                 "Envelope out",
    0 );

  p->p = cmCompressorAlloc(ctx->cmProcCtx, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false );

  cmDspSetDefaultBool(   ctx, &p->inst, kBypassCmId, false, false );
  cmDspSetDefaultDouble( ctx, &p->inst, kAtkMsCmId,    0.0, 20.0 );
  cmDspSetDefaultDouble( ctx, &p->inst, kRlsMsCmId,    0.0, 20.0 );
  cmDspSetDefaultDouble( ctx, &p->inst, kInGainCmId,   0.0, 1.0 );
  cmDspSetDefaultDouble( ctx, &p->inst, kOutGainCmId,  0.0, 1.0 );
  cmDspSetDefaultDouble( ctx, &p->inst, kWndMsCmId,    0.0, 200.0);
  cmDspSetDefaultDouble( ctx, &p->inst, kMaxWndMsCmId, 0.0, 1000.0);
  cmDspSetDefaultDouble( ctx, &p->inst, kEnvCmId,      0.0, 0.0 );

  return &p->inst;
}

cmDspRC_t _cmDspCompressorFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspCompressor_t* p = (cmDspCompressor_t*)inst;
  cmCompressorFree(&p->p);
  return kOkDspRC;
}

cmDspRC_t _cmDspCompressorReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t        rc = kOkDspRC;
  cmDspCompressor_t* p  = (cmDspCompressor_t*)inst;

  if((rc  = cmDspApplyAllDefaults(ctx,inst)) == kOkDspRC )
  {
    cmDspZeroAudioBuf(ctx,inst,kOutCmId);


    cmReal_t threshDb = cmDspDouble(inst, kThreshDbCmId );
    cmReal_t ratio    = cmDspDouble(inst, kRatioCmId );
    cmReal_t atkMs    = cmDspDouble(inst, kAtkMsCmId );
    cmReal_t rlsMs    = cmDspDouble(inst, kRlsMsCmId );
    cmReal_t inGain   = cmDspDouble(inst, kInGainCmId );
    cmReal_t outGain  = cmDspDouble(inst, kOutGainCmId );    
    cmReal_t wndMs    = cmDspDouble(inst, kWndMsCmId );
    cmReal_t maxWndMs = cmDspDouble(inst, kMaxWndMsCmId);
    bool     bypassFl = cmDspBool(  inst, kBypassCmId );
    cmCompressorInit(p->p,cmDspSampleRate(ctx),cmDspSamplesPerCycle(ctx), inGain, maxWndMs, wndMs, threshDb, ratio, atkMs, rlsMs, outGain, bypassFl );
  }

  return rc;  
}

cmDspRC_t _cmDspCompressorExec( cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t          rc   = kOkDspRC;
  cmDspCompressor_t* p    = (cmDspCompressor_t*)inst;
  cmSample_t*        y    = cmDspAudioBuf(ctx,inst,kOutCmId, 0);
  const cmSample_t*  x    = cmDspAudioBuf(ctx,inst,kInCmId,0);
  unsigned           n    = cmDspAudioBufSmpCount(ctx,inst,kOutCmId,0);

  if( x != NULL )
  {
    cmCompressorExec(p->p,x,y,n);
  
    rc = cmDspSetDouble( ctx, inst, kEnvCmId, p->p->gain );
  }
  return rc;
}

cmDspRC_t _cmDspCompressorRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  cmDspRC_t rc;
  cmDspCompressor_t* p  = (cmDspCompressor_t*)inst;

  if((rc =  cmDspSetEvent(ctx,inst,evt)) != kOkDspRC )
    return rc;

  cmReal_t v = cmDspDouble(inst,evt->dstVarId);

  switch( evt->dstVarId )
  {
    case kThreshDbCmId:
      cmCompressorSetThreshDb(p->p,v);
      break;

    case kRatioCmId:
      p->p->ratio_num = v;
      break;

    case kAtkMsCmId:
      cmCompressorSetAttackMs(p->p,v);
      break;

    case kRlsMsCmId:
      cmCompressorSetReleaseMs(p->p,v);
      break;

    case kInGainCmId:
      p->p->inGain = v;
      break;

    case kOutGainCmId:
      p->p->outGain = v;
      break;
      
    case kWndMsCmId:
      cmCompressorSetRmsWndMs(p->p,v);
      break;

    case kBypassCmId:
      p->p->bypassFl = cmDspBool(inst,kBypassCmId);
      break;
  }

  return rc;
}

struct cmDspClass_str* cmCompressorClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmCompressorDC,ctx,"Compressor",
    NULL,
    _cmDspCompressorAlloc,
    _cmDspCompressorFree,
    _cmDspCompressorReset,
    _cmDspCompressorExec,
    _cmDspCompressorRecv,
    NULL,NULL,
    "Compressor");

  return &_cmCompressorDC;
}

//==========================================================================================================================================
enum
{
  kBypassBqId,
  kModeBqId,
  kF0HzBqId,
  kQBqId,
  kGainDbBqId,
  kInBqId,
  kOutBqId
};

cmDspClass_t _cmBiQuadEqDC;

typedef struct
{
  const cmChar_t* label;
  unsigned        mode;
  unsigned        symbol;
} cmDspBiQuadMap_t;

cmDspBiQuadMap_t _cmDspBiQuadMap[] = 
{
  {"LP",    kLpfBqId, cmInvalidId },
  {"HP",    kHpFBqId, cmInvalidId },
  {"BP",    kBpfBqId, cmInvalidId },
  {"Notch", kNotchBqId, cmInvalidId },
  {"AP",    kAllpassBqId, cmInvalidId },
  {"Peak",  kPeakBqId, cmInvalidId },
  {"LSh",   kLowShelfBqId, cmInvalidId },
  {"HSh",   kHighShelfBqId, cmInvalidId },
  { NULL,   cmInvalidId, cmInvalidId }
};

typedef struct
{
  cmDspInst_t   inst;
  cmBiQuadEq*   p;
} cmDspBiQuadEq_t;

cmDspInst_t*  _cmDspBiQuadEqAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspBiQuadEq_t* p = cmDspInstAllocV(cmDspBiQuadEq_t,ctx,classPtr,instSymId,id,storeSymId,va_cnt,vl,
    1,   "bypass",kBypassBqId,   0, 0, kInDsvFl  | kBoolDsvFl    | kReqArgDsvFl, "Bypass enable.",
    1,   "mode",  kModeBqId,     0, 0, kInDsvFl  | kSymDsvFl     | kReqArgDsvFl, "Mode Symbol: LP|HP|BP|AP|Notch|Pk|LSh|HSh.",
    1,   "f0",    kF0HzBqId,     0, 0, kInDsvFl  | kDoubleDsvFl  | kReqArgDsvFl, "Center or edge frequecy in Hz.",
    1,   "Q",     kQBqId,        0, 0, kInDsvFl  | kDoubleDsvFl  | kReqArgDsvFl, "Q",
    1,   "gain",  kGainDbBqId,   0, 0, kInDsvFl  | kDoubleDsvFl  | kOptArgDsvFl, "Gain Db (Pk,LSh,Hsh only)",
    1,   "in",    kInBqId,       0, 0, kInDsvFl  | kAudioBufDsvFl,               "Audio input",
    1,   "out",   kOutBqId,      0, 1, kOutDsvFl | kAudioBufDsvFl,               "Audio output",
    0 );

  cmDspSetDefaultDouble( ctx, &p->inst, kGainDbBqId, 0.0, 1.0 );

  p->p = cmBiQuadEqAlloc(ctx->cmProcCtx,NULL,0,0,0,0,0,false);

  // register the filter mode symbols
  unsigned i;
  for(i=0; _cmDspBiQuadMap[i].label != NULL; ++i)
    if( _cmDspBiQuadMap[i].symbol == cmInvalidId )
      _cmDspBiQuadMap[i].symbol = cmSymTblRegisterStaticSymbol(ctx->stH,_cmDspBiQuadMap[i].label);

  return &p->inst;
}

cmDspRC_t _cmDspBiQuadEqFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspBiQuadEq_t* p = (cmDspBiQuadEq_t*)inst;
  cmBiQuadEqFree(&p->p);
  return kOkDspRC;
}

unsigned _cmDspBiQuadEqModeId( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned modeSymId )
{
  unsigned i;

  for(i=0; _cmDspBiQuadMap[i].label!=NULL; ++i)
    if( _cmDspBiQuadMap[i].symbol == modeSymId )
      return _cmDspBiQuadMap[i].mode;

  cmDspInstErr(ctx,inst,kVarNotValidDspRC,"The mode string '%s' is not valid.",cmStringNullGuard(cmSymTblLabel(ctx->stH,modeSymId)));

  return cmInvalidId;
}

cmDspRC_t _cmDspBiQuadEqReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t        rc = kOkDspRC;
  cmDspBiQuadEq_t* p  = (cmDspBiQuadEq_t*)inst;

  if((rc  = cmDspApplyAllDefaults(ctx,inst)) == kOkDspRC )
  {
    cmDspZeroAudioBuf(ctx,inst,kOutBqId);

    unsigned modeSymId = cmDspSymbol(inst, kModeBqId );
    cmReal_t f0Hz      = cmDspDouble(inst, kF0HzBqId );
    cmReal_t Q         = cmDspDouble(inst, kQBqId );
    cmReal_t gainDb    = cmDspDouble(inst, kGainDbBqId );
    unsigned mode      = _cmDspBiQuadEqModeId(ctx,inst,modeSymId );
    bool     bypassFl  = cmDspBool(inst, kBypassBqId );

    if( mode != cmInvalidId )
      cmBiQuadEqInit(p->p, cmDspSampleRate(ctx), mode, f0Hz, Q, gainDb, bypassFl );
  }

  return rc;  
}

cmDspRC_t _cmDspBiQuadEqExec( cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t          rc   = kOkDspRC;
  cmDspBiQuadEq_t* p    = (cmDspBiQuadEq_t*)inst;
  cmSample_t*        y    = cmDspAudioBuf(ctx,inst,kOutBqId, 0);
  const cmSample_t*  x    = cmDspAudioBuf(ctx,inst,kInBqId,0);
  unsigned           n    = cmDspAudioBufSmpCount(ctx,inst,kOutBqId,0);

  cmBiQuadEqExec(p->p,x,y,n);

  return rc;
}

cmDspRC_t _cmDspBiQuadEqRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  cmDspRC_t rc;
  cmDspBiQuadEq_t* p  = (cmDspBiQuadEq_t*)inst;
  
  if((rc =  cmDspSetEvent(ctx,inst,evt)) != kOkDspRC )
    return rc;
  
  cmReal_t        f0Hz    = p->p->f0Hz;
  cmReal_t        Q       = p->p->Q;
  cmReal_t        gainDb  = p->p->gainDb;
  unsigned        mode    = p->p->mode;

  if( evt->dstVarId == kModeBqId )
  {
    if((mode = _cmDspBiQuadEqModeId(ctx,inst,cmDspSymbol(inst,kModeBqId) )) == cmInvalidId )
      rc = kVarNotValidDspRC;
  }
  else
  {
    cmReal_t v = cmDspDouble(inst,evt->dstVarId);

    switch( evt->dstVarId )
    {
      case kF0HzBqId:
        f0Hz = v;
        break;

      case kQBqId:
        Q = v;
        break;

      case kGainDbBqId:
        gainDb = v;
        break;

      case kBypassBqId:
        p->p->bypassFl = cmDspBool(inst,kBypassBqId);
        break;

    }
  }

  cmBiQuadEqSet(p->p,mode,f0Hz,Q,gainDb);

  return rc;
}

struct cmDspClass_str* cmBiQuadEqClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmBiQuadEqDC,ctx,"BiQuadEq",
    NULL,
    _cmDspBiQuadEqAlloc,
    _cmDspBiQuadEqFree,
    _cmDspBiQuadEqReset,
    _cmDspBiQuadEqExec,
    _cmDspBiQuadEqRecv,
    NULL,NULL,
    "Bi-Quad EQ Filters");

  return &_cmBiQuadEqDC;
}

//==========================================================================================================================================
enum
{
  kBypassDsId,
  kInGainDsId,
  kSrateDsId,
  kBitsDsId,
  kRectDsId,
  kFullDsId,
  kClipDbDsId,
  kOutGainDsId,
  kInDsId,
  kOutDsId
};

cmDspClass_t _cmDistDsDC;

typedef struct
{
  cmDspInst_t  inst;
  cmDistDs*    p;
} cmDspDistDs_t;

cmDspInst_t*  _cmDspDistDsAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{

  cmDspDistDs_t* p = cmDspInstAllocV(cmDspDistDs_t,ctx,classPtr,instSymId,id,storeSymId,va_cnt,vl,
    1,   "bypass", kBypassDsId,   0, 0, kInDsvFl  | kBoolDsvFl     | kOptArgDsvFl, "Bypass enable.",
    1,   "igain",  kInGainDsId,   0, 0, kInDsvFl  | kDoubleDsvFl   | kOptArgDsvFl, "Input gain.",
    1,   "srate",  kSrateDsId,    0, 0, kInDsvFl  | kDoubleDsvFl   | kOptArgDsvFl, "Down-sample rate.",
    1,   "bits",   kBitsDsId,     0, 0, kInDsvFl  | kDoubleDsvFl   | kOptArgDsvFl, "Bits per sample",
    1,   "rect",   kRectDsId,     0, 0, kInDsvFl  | kBoolDsvFl     | kOptArgDsvFl, "Rectify flag",
    1,   "full",   kFullDsId,     0, 0, kInDsvFl  | kBoolDsvFl     | kOptArgDsvFl, "1=Full 0=Half rectify",
    1,   "clip",   kClipDbDsId,   0, 0, kInDsvFl  | kDoubleDsvFl   | kOptArgDsvFl, "Clip dB",
    1,   "ogain",  kOutGainDsId,  0, 0, kInDsvFl  | kDoubleDsvFl   | kOptArgDsvFl, "Output gain",
    1,   "in",     kInDsId,       0, 0, kInDsvFl  | kAudioBufDsvFl,                "Audio input",
    1,   "out",    kOutDsId,      0, 1, kOutDsvFl | kAudioBufDsvFl,                "Audio output",
    0 );

  cmDspSetDefaultBool(   ctx, &p->inst, kBypassDsId, false, false );
  cmDspSetDefaultDouble( ctx, &p->inst, kInGainDsId, 0.0, 1.0 );
  cmDspSetDefaultDouble( ctx, &p->inst, kSrateDsId, 0.0, cmDspSampleRate(ctx));
  cmDspSetDefaultDouble( ctx, &p->inst, kBitsDsId,  0.0, 24.0 );
  cmDspSetDefaultBool(   ctx, &p->inst, kRectDsId,  false, false );
  cmDspSetDefaultBool(   ctx, &p->inst, kFullDsId,  false, false );
  cmDspSetDefaultDouble( ctx, &p->inst, kClipDbDsId,    0.0, 0.0 );
  cmDspSetDefaultDouble( ctx, &p->inst, kOutGainDsId, 0.0, 1.0 );

  p->p = cmDistDsAlloc(ctx->cmProcCtx, NULL, 0, 0, 0, 0, false, false, 0, 0, false ); 

  return &p->inst;
}

cmDspRC_t _cmDspDistDsFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspDistDs_t* p = (cmDspDistDs_t*)inst;
  cmDistDsFree(&p->p);
  return kOkDspRC;
}

cmDspRC_t _cmDspDistDsReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t        rc = kOkDspRC;
  cmDspDistDs_t* p = (cmDspDistDs_t*)inst;

  if((rc = cmDspApplyAllDefaults(ctx,inst)) == kOkDspRC )
  {
    cmDspZeroAudioBuf(ctx,inst,kOutDsId);

    bool     bypassFl  = cmDspBool(   inst, kBypassDsId );
    cmReal_t inGain    = cmDspDouble( inst, kInGainDsId );
    cmReal_t downSrate = cmDspDouble( inst, kSrateDsId );
    cmReal_t bits      = cmDspDouble( inst, kBitsDsId );
    bool     rectFl    = cmDspBool(   inst, kRectDsId );
    bool     fullFl    = cmDspBool(   inst, kFullDsId );
    cmReal_t clipDb    = cmDspDouble( inst, kClipDbDsId );
    cmReal_t outGain   = cmDspDouble( inst, kOutGainDsId );
    cmDistDsInit(p->p, cmDspSampleRate(ctx), inGain, downSrate, bits, rectFl, fullFl, clipDb, outGain, bypassFl ); 
  }

  return rc;  
}

cmDspRC_t _cmDspDistDsExec( cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t         rc = kOkDspRC;
  cmDspDistDs_t*    p  = (cmDspDistDs_t*)inst;
  unsigned          n  = cmDspAudioBufSmpCount(ctx,inst,kOutDsId,0);
  cmSample_t*       y  = cmDspAudioBuf(ctx,inst,kOutDsId,0);
  const cmSample_t* x  = cmDspAudioBuf(ctx,inst,kInDsId,0);
    
  cmDistDsExec(p->p,x,y,n);

  return rc;
}

cmDspRC_t _cmDspDistDsRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  cmDspRC_t     rc = kOkDspRC;
  cmDspDistDs_t* p  = (cmDspDistDs_t*)inst;

  if((rc = cmDspSetEvent(ctx,inst,evt)) == kOkDspRC )
  {
    switch( evt->dstVarId )
    {
      case kInGainDsId:
        p->p->inGain = cmDspDouble(inst,kInGainDsId);
        break;

      case kSrateDsId:        
        p->p->downSrate = cmDspDouble(inst,kSrateDsId);
        break;

      case kBitsDsId:
        p->p->bits = cmDspDouble(inst,kBitsDsId);
        break;

      case kRectDsId:
        p->p->rectFl = cmDspBool(inst,kRectDsId);
        break;

      case kFullDsId:
        p->p->fullFl = cmDspBool(inst,kFullDsId);
        break;

      case kClipDbDsId:
        p->p->clipDb = cmDspDouble(inst,kClipDbDsId);
        break;

      case kOutGainDsId:
        p->p->outGain = cmDspDouble(inst,kOutGainDsId);
        break;
        
      case kBypassDsId:
        p->p->bypassFl = cmDspBool(inst,kBypassDsId);
        break;

    }
  }

  return rc;
}

struct cmDspClass_str* cmDistDsClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmDistDsDC,ctx,"DistDs",
    NULL,
    _cmDspDistDsAlloc,
    _cmDspDistDsFree,
    _cmDspDistDsReset,
    _cmDspDistDsExec,
    _cmDspDistDsRecv,
    NULL,NULL,
    "Distortion and Downsampler");

  return &_cmDistDsDC;
}

//==========================================================================================================================================
enum
{
  kInDlId,
  kOutDlId
};

cmDspClass_t _cmDbToLinDC;

typedef struct
{
  cmDspInst_t  inst;
} cmDspDbToLin_t;

cmDspInst_t*  _cmDspDbToLinAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{

  cmDspDbToLin_t* p = cmDspInstAllocV(cmDspDbToLin_t,ctx,classPtr,instSymId,id,storeSymId,va_cnt,vl,
    1,   "in",   kInDlId,   0, 0, kInDsvFl   | kDoubleDsvFl, "Input",
    1,   "out",  kOutDlId,  0, 0, kOutDsvFl  | kDoubleDsvFl, "Output",
    0 );

  cmDspSetDefaultDouble( ctx, &p->inst, kInDlId,   0.0, -1000.0);
  cmDspSetDefaultDouble( ctx, &p->inst, kOutDlId,  0.0, 0.0 );

  return &p->inst;
}


cmDspRC_t _cmDspDbToLinReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  return cmDspApplyAllDefaults(ctx,inst);
}


cmDspRC_t _cmDspDbToLinRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  cmDspRC_t     rc = kOkDspRC;

  if((rc = cmDspSetEvent(ctx,inst,evt)) == kOkDspRC )
  {
    if( evt->dstVarId == kInDlId )
    {
      double db  = cmMax(0.0,cmMin(100.0,cmDspDouble(inst,kInDlId)));
      double lin = db==0 ? 0.0 : pow(10.0, (db-100.0)/20.0);
      cmDspSetDouble(ctx,inst,kOutDlId,lin);
    }
  }

  return rc;
}

struct cmDspClass_str* cmDbToLinClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmDbToLinDC,ctx,"DbToLin",
    NULL,
    _cmDspDbToLinAlloc,
    NULL,
    _cmDspDbToLinReset,
    NULL,
    _cmDspDbToLinRecv,
    NULL,NULL,
    "dB to Linear converter");

  return &_cmDbToLinDC;
}

//==========================================================================================================================================
// Pass any N of M inputs
enum
{
  kInChCntNoId,
  kOutChCntNoId,
  kXfadeMsNoId,
  kCmdNoId,
  kSelIdxNoId,
  kBaseGateNoId
};

cmDspClass_t _cmNofM_DC;

typedef struct
{
  cmDspInst_t   inst;

  unsigned      iChCnt;
  unsigned      oChCnt;

  unsigned*     map;             // map[ oChCnt ]
  cmXfader**    xf;              // xf[ oChCnt ];

  unsigned      cfgSymId;
  unsigned      onSymId;
  unsigned      offSymId;
  bool          verboseFl;

  unsigned      baseBaseInNoId;  // first data input port id
  unsigned      baseBaseOutNoId; // first data output port id

  unsigned      baseInFloatNoId;
  unsigned      baseInBoolNoId;
  unsigned      baseInSymNoId;
  unsigned      baseInAudioNoId;

  unsigned      baseOutFloatNoId;
  unsigned      baseOutBoolNoId;
  unsigned      baseOutSymNoId;
  unsigned      baseOutAudioNoId;

  bool printFl;

} cmDspNofM_t;

cmDspInst_t*  _cmDspNofM_Alloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  if( va_cnt < 2 )
  {
    cmDspClassErr(ctx,classPtr,kVarArgParseFailDspRC,"The 'NofM' constructor must given input and output channel counts.");
    return NULL;
  }

  va_list vl1;
  va_copy(vl1,vl);

  int      iChCnt           = va_arg(vl,int);
  int      oChCnt           = va_arg(vl,int);

  if( oChCnt > iChCnt )
  {
    va_end(vl1);
    cmDspClassErr(ctx,classPtr,kVarArgParseFailDspRC,"The 'NofM' output count must be less than or equal to the input count.");
    return NULL;
  }


  unsigned baseInFloatNoId  = kBaseGateNoId    + iChCnt;
  unsigned baseInBoolNoId   = baseInFloatNoId  + iChCnt;
  unsigned baseInSymNoId    = baseInBoolNoId   + iChCnt;
  unsigned baseInAudioNoId  = baseInSymNoId    + iChCnt;
  unsigned baseOutFloatNoId = baseInAudioNoId  + iChCnt;
  unsigned baseOutBoolNoId  = baseOutFloatNoId + oChCnt;
  unsigned baseOutSymNoId   = baseOutBoolNoId  + oChCnt;
  unsigned baseOutAudioNoId = baseOutSymNoId   + oChCnt;

  unsigned      i;

  cmDspNofM_t* p = cmDspInstAllocV(cmDspNofM_t,ctx,classPtr,instSymId,id,storeSymId,va_cnt,vl1,
    1,         "ichs",  kInChCntNoId,     0, 0, kUIntDsvFl     | kReqArgDsvFl,"Input channel count.",
    1,         "ochs",  kOutChCntNoId,    0, 0, kUIntDsvFl     | kReqArgDsvFl,"Output channel count.", 
    1,         "ms",    kXfadeMsNoId,     0, 0, kDoubleDsvFl   | kInDsvFl | kOptArgDsvFl,"Audio Cross-fade time in milliseconds.",
    1,         "cmd",   kCmdNoId,         0, 0, kSymDsvFl      | kInDsvFl,    "Command input.",
    1,         "seli",  kSelIdxNoId,      0, 0, kUIntDsvFl     | kInDsvFl,    "Enable gate at index.",
    iChCnt,    "sel",   kBaseGateNoId,    0, 0, kBoolDsvFl     | kInDsvFl,    "Selector Gate inputs.",
    iChCnt,    "f-in",  baseInFloatNoId,  0, 0, kDoubleDsvFl   | kInDsvFl,    "Float input",
    iChCnt,    "b-in",  baseInBoolNoId,   0, 0, kBoolDsvFl     | kInDsvFl,    "Bool input",
    iChCnt,    "s-in",  baseInSymNoId,    0, 0, kSymDsvFl      | kInDsvFl,    "Symbol input",
    iChCnt,    "a-in",  baseInAudioNoId,  0, 0, kAudioBufDsvFl | kInDsvFl,    "Audio input",
    oChCnt,    "f-out", baseOutFloatNoId, 0, 0, kDoubleDsvFl   | kOutDsvFl,   "Float output",
    oChCnt,    "b-out", baseOutBoolNoId,  0, 0, kBoolDsvFl     | kOutDsvFl,   "Bool output",
    oChCnt,    "s-out", baseOutSymNoId,   0, 0, kSymDsvFl      | kOutDsvFl,   "Symbol output",
    oChCnt,    "a-out", baseOutAudioNoId, 0, 1, kAudioBufDsvFl | kOutDsvFl,   "Audio output",
    0 );

  p->iChCnt     = iChCnt;
  p->oChCnt     = oChCnt;
  p->map        = cmMemAllocZ(unsigned,oChCnt);
  p->xf         = cmMemAllocZ(cmXfader*,oChCnt);
  p->cfgSymId   = cmSymTblRegisterStaticSymbol(ctx->stH,"cfg");
  p->onSymId    = cmSymTblRegisterStaticSymbol(ctx->stH,"on");
  p->offSymId   = cmSymTblRegisterStaticSymbol(ctx->stH,"off");

  p->verboseFl  = false;

  p->baseBaseInNoId   =  baseInFloatNoId;
  p->baseBaseOutNoId  =  baseOutFloatNoId;

  p->baseInFloatNoId  =  baseInFloatNoId; 
  p->baseInBoolNoId   =  baseInBoolNoId;
  p->baseInSymNoId    =  baseInSymNoId;
  p->baseInAudioNoId  =  baseInAudioNoId;
  p->baseOutFloatNoId =  baseOutFloatNoId;
  p->baseOutBoolNoId  =  baseOutBoolNoId;
  p->baseOutSymNoId   =  baseOutSymNoId;
  p->baseOutAudioNoId =  baseOutAudioNoId;

  for(i=0; i<oChCnt; ++i)
  {
    cmDspSetDefaultDouble( ctx, &p->inst, baseOutFloatNoId + i, 0.0, 0.0 );
    cmDspSetDefaultBool(   ctx, &p->inst, baseOutBoolNoId  + i, false, false );
    cmDspSetDefaultSymbol( ctx, &p->inst, baseOutSymNoId   + i, cmInvalidId );
    p->xf[i] = cmXfaderAlloc( ctx->cmProcCtx, NULL, 0, 0, 0 );
  }

  cmDspSetDefaultDouble( ctx, &p->inst, kXfadeMsNoId, 0.0, 15.0 );

  va_end(vl1);

  return &p->inst;
}

cmDspRC_t _cmDspNofM_Free(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspNofM_t* p = (cmDspNofM_t*)inst;
  unsigned i;
  for(i=0; i<p->oChCnt; ++i)
    cmXfaderFree(&p->xf[i]);

  cmMemFree(p->map);
  cmMemFree(p->xf);
  return kOkDspRC;
}

cmDspRC_t _cmDspNofM_Reset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t        rc = kOkDspRC;
  cmDspNofM_t* p = (cmDspNofM_t*)inst;
  unsigned i;
  if((rc   = cmDspApplyAllDefaults(ctx,inst)) == kOkDspRC )
  {
    for(i=0; i<p->oChCnt; ++i)
    {
      cmDspZeroAudioBuf(ctx,inst,p->baseOutAudioNoId+i);
      p->map[i] = cmInvalidIdx;
      double xfadeMs = cmDspDouble(inst,kXfadeMsNoId);
      cmXfaderInit(p->xf[i], cmDspSampleRate(ctx), p->iChCnt, xfadeMs );
    }
  }

  return rc;  
}

cmDspRC_t _cmDspNofM_Exec( cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t    rc = kOkDspRC;

  cmDspNofM_t* p  = (cmDspNofM_t*)inst;
  unsigned     i;
  const cmSample_t* x[ p->iChCnt ];

  for(i=0; i<p->iChCnt; ++i)
    x[i] = cmDspAudioBuf(ctx,inst,p->baseInAudioNoId + i ,0);

  // for each valid p->map[] element
  for(i=0; i<p->oChCnt; ++i)
  {
    cmSample_t* y = cmDspAudioBuf(ctx,inst,p->baseOutAudioNoId+ i,0);
    unsigned    n = cmDspAudioBufSmpCount(ctx,inst,p->baseOutAudioNoId+i,0);

    if( y != NULL )
    {
      y = cmVOS_Zero(y,n);      
      cmXfaderExecAudio(p->xf[i],n,NULL,p->iChCnt,x,y);
    }
  }

  return rc;
}

cmDspRC_t _cmDspNofM_Recv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  cmDspRC_t    rc = kOkDspRC;
  cmDspNofM_t* p  = (cmDspNofM_t*)inst;
  unsigned     i,j;

  assert( evt->dstVarId < p->baseBaseOutNoId );
  
  // store the incoming value
  if((rc = cmDspSetEvent(ctx,inst,evt)) != kOkDspRC )
    return rc;

  // if this is a fade time
  if( evt->dstVarId == kXfadeMsNoId )
  {
    for(i=0; i<p->oChCnt; ++i)
      cmXfaderSetXfadeTime( p->xf[i], cmDspDouble(inst,kXfadeMsNoId) );   

    return rc;
  }

  // if this is an index selector
  if( kSelIdxNoId == evt->dstVarId )
  {
    unsigned idx;
    if((idx = cmDspUInt(inst,kSelIdxNoId)) >= p->iChCnt )
      rc = cmDspInstErr(ctx,inst,kInvalidArgDspRC,"The selection index ('%i') is out of the channel range, (%i).",idx,p->iChCnt); 
    else
    {
      cmDspSetBool( ctx, inst, kBaseGateNoId+idx , true );

      if( p->verboseFl )
        cmRptPrintf(ctx->rpt,"nom seli:%i\n",idx);
    }
    return rc;
  }

  // NOTE: the internal state DOES NOT CHANGE until a message arrives
  // on the 'cmd' port

  // if anything arrives on the command port - then read the gate states and rebuild the map
  if( evt->dstVarId == kCmdNoId )
  {
    
    unsigned cmdSymId = cmDspSymbol(inst,kCmdNoId);

    // if cmdSymId == 'on' then turn on all selection gates
    if(cmdSymId  == p->onSymId )
    {
      for(i=0; i<p->oChCnt; ++i)
        cmDspSetBool(ctx,inst,kBaseGateNoId+i,true);
    }
    else
      // if cmdSymId == 'off' then turn off all selection  gates
      if( cmdSymId == p->offSymId )
      {
        if( p->verboseFl )
          cmRptPrintf(ctx->rpt,"nom: off\n");

        for(i=0; i<p->oChCnt; ++i)
          cmDspSetBool(ctx,inst,kBaseGateNoId+i,false);
      }
      else
        // cmdSymId == 'print' then print the in/out map[]
        if( cmdSymId == cmSymTblId(ctx->stH,"print") )
        {
          for(i=0; i<p->oChCnt; ++i)
            cmRptPrintf(ctx->rpt,"%i:%i ",i,p->map[i]);
          cmRptPrintf(ctx->rpt,"\n");
          cmRptPrintf(ctx->rpt,"%i usecs\n",ctx->execDurUsecs);
          p->printFl = !p->printFl;
        }

    if( p->verboseFl )
      cmRptPrintf(ctx->rpt,"Nom: %s ", inst->symId != cmInvalidId ? cmSymTblLabel(ctx->stH,inst->symId) : "");

    // 5/26
    if( p->iChCnt == p->oChCnt )
      cmVOU_Fill(p->map,p->oChCnt,cmInvalidIdx);

    // for each input 
    for(i=0,j=0; i<p->iChCnt; ++i)
    {
      
      // if this input is switched on
      if( cmDspBool(inst,kBaseGateNoId+i) )
      {
        if( j >= p->oChCnt )
        {
          cmDspInstErr(ctx,inst,kVarNotValidDspRC,"To many inputs have been turned on for %i outputs.",p->oChCnt);
          break;
        }

        // assign input i to output j
        p->map[j] = i;

        // fade in ch i and fade out all others
        cmXfaderSelectOne(p->xf[j],i);

        ++j;

        if( p->verboseFl )
          cmRptPrintf(ctx->rpt,"%i ",i);

      }
      else // 5/26
      {
        if( p->iChCnt == p->oChCnt )
          ++j;
      }
    }

    // deselect all other output channels
    //for(; j<p->oChCnt; ++j)
    //{
    //  p->map[j] = cmInvalidIdx;
    //  cmXfaderAllOff(p->xf[j]);
    //}
    // 5/26
    if( p->iChCnt == p->oChCnt )
    {
      for(j=0; j<p->oChCnt; ++j)
        if( p->map[j] == cmInvalidIdx )
          cmXfaderAllOff(p->xf[j]);
    }
    else
    {
      for(; j<p->oChCnt; ++j)
      {
        p->map[j] = cmInvalidIdx;
        cmXfaderAllOff(p->xf[j]);
      }
    }


    if( p->verboseFl )
      cmRptPrintf(ctx->rpt,"\n");

    // zero the audio buffers of unused output channels
    for(i=0; i<p->oChCnt; ++i)
      if( p->map[i] == cmInvalidIdx )
        cmDspZeroAudioBuf(ctx,inst,p->baseOutAudioNoId+i); 

  }


  // if this is an input data event
  if( p->baseBaseInNoId <= evt->dstVarId && evt->dstVarId < p->baseBaseOutNoId  )
  {
    // get the input channel this event occurred on
    unsigned iChIdx = (evt->dstVarId - p->baseBaseInNoId) % p->iChCnt;

    // is iChIdx mapped to an output ...
    for(i=0; i<p->oChCnt; ++i)
      if( p->map[i] == iChIdx )
        break;

    // ... no - nothing else to do
    if( i==p->oChCnt )
      return kOkDspRC;

    // ... yes set the output ...

    // double
    if( p->baseInFloatNoId <= evt->dstVarId && evt->dstVarId < p->baseInFloatNoId + p->iChCnt )
    {
      cmDspSetDouble(ctx,inst,p->baseOutFloatNoId + i, cmDspDouble(inst,evt->dstVarId));
    }
    else

      // bool  
    if( p->baseInBoolNoId <= evt->dstVarId && evt->dstVarId < p->baseInBoolNoId + p->iChCnt )
    {
      cmDspSetBool(ctx,inst,p->baseOutBoolNoId + i, cmDspBool(inst,evt->dstVarId));
      if(p->printFl)
        cmRptPrintf(ctx->rpt,"%i %i\n",p->baseOutBoolNoId + i, cmDspBool(inst,evt->dstVarId));
    }
    else
      
      // symbol
    if( p->baseInSymNoId <= evt->dstVarId && evt->dstVarId < p->baseInSymNoId + p->iChCnt )
      cmDspSetSymbol(ctx,inst,p->baseOutSymNoId + i, cmDspSymbol(inst,evt->dstVarId));
    
  }


  return rc;
}

struct cmDspClass_str* cmNofMClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmNofM_DC,ctx,"NofM",
    NULL,
    _cmDspNofM_Alloc,
    _cmDspNofM_Free,
    _cmDspNofM_Reset,
    _cmDspNofM_Exec,
    _cmDspNofM_Recv,
    NULL,NULL,
    "N of M Switch");

  return &_cmNofM_DC;
}


//==========================================================================================================================================

enum
{
  kInChCnt1oId,
  kInChIdx1oId,
  kOutFloat1oId,
  kOutBool1oId,
  kOutSym1oId,
  kOutAudio1oId,
  kBaseInFloat1oId
};

cmDspClass_t _cm1ofN_DC;

typedef struct
{
  cmDspInst_t   inst;

  unsigned      iChCnt;
  unsigned      oChCnt;

  unsigned      iChIdx;

  unsigned      baseBaseIn1oId;  // first data input port id

  unsigned      baseInFloat1oId;
  unsigned      baseInBool1oId;
  unsigned      baseInSym1oId;
  unsigned      baseInAudio1oId;

  unsigned      baseOutFloat1oId;
  unsigned      baseOutBool1oId;
  unsigned      baseOutSym1oId;
  unsigned      baseOutAudio1oId;


} cmDsp1ofN_t;

cmDspInst_t*  _cmDsp1ofN_Alloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  if( va_cnt < 1 )
  {
    cmDspClassErr(ctx,classPtr,kVarArgParseFailDspRC,"The '1ofN' constructor must given input channel count.");
    return NULL;
  }

  va_list vl1;
  va_copy(vl1,vl);

  int      iChCnt           = va_arg(vl,int);

  unsigned baseInFloat1oId  = kBaseInFloat1oId;
  unsigned baseInBool1oId   = baseInFloat1oId  + iChCnt;
  unsigned baseInSym1oId    = baseInBool1oId   + iChCnt;
  unsigned baseInAudio1oId  = baseInSym1oId    + iChCnt;

  cmDsp1ofN_t* p = cmDspInstAllocV(cmDsp1ofN_t,ctx,classPtr,instSymId,id,storeSymId,va_cnt,vl1,
    1,         "ichs",  kInChCnt1oId,     0, 0, kUIntDsvFl   | kReqArgDsvFl,"Input channel count.",
    1,         "chidx", kInChIdx1oId,     0, 0, kUIntDsvFl   | kReqArgDsvFl | kInDsvFl,    "Input channel selector index.",
    1,         "f-out", kOutFloat1oId,    0, 0, kDoubleDsvFl | kOutDsvFl,   "Float output",
    1,         "b-out", kOutBool1oId,     0, 0, kBoolDsvFl   | kOutDsvFl,   "Bool output",
    1,         "s-out", kOutSym1oId,      0, 0, kSymDsvFl    | kOutDsvFl,   "Symbol output",
    1,         "a-out", kOutAudio1oId,    0, 1, kAudioBufDsvFl  | kOutDsvFl,   "Audio output",
    iChCnt,    "f-in",  baseInFloat1oId,  0, 0, kDoubleDsvFl | kInDsvFl,    "Float input",
    iChCnt,    "b-in",  baseInBool1oId,   0, 0, kBoolDsvFl   | kInDsvFl,    "Bool input",
    iChCnt,    "s-in",  baseInSym1oId,    0, 0, kSymDsvFl    | kInDsvFl,    "Symbol input",
    iChCnt,    "a-in",  baseInAudio1oId,  0, 0, kAudioBufDsvFl  | kInDsvFl,    "Audio input",
    0 );

  p->iChCnt     = iChCnt;

  p->baseBaseIn1oId   =  kBaseInFloat1oId;

  p->baseInFloat1oId  =  baseInFloat1oId; 
  p->baseInBool1oId   =  baseInBool1oId;
  p->baseInSym1oId    =  baseInSym1oId;
  p->baseInAudio1oId  =  baseInAudio1oId;


  cmDspSetDefaultDouble( ctx, &p->inst, kOutFloat1oId, 0.0, 0.0 );
  cmDspSetDefaultBool(   ctx, &p->inst, kOutBool1oId, false, false );
  cmDspSetDefaultSymbol( ctx, &p->inst, kOutSym1oId, cmInvalidId );

  va_end(vl1);

  return &p->inst;
}

cmDspRC_t _cmDsp1ofN_Free(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  return kOkDspRC;
}

cmDspRC_t _cmDsp1ofN_Reset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t    rc = kOkDspRC;

  if((rc   = cmDspApplyAllDefaults(ctx,inst)) == kOkDspRC )
  {
    cmDspZeroAudioBuf(ctx,inst,kOutAudio1oId);
  }

  return rc;  
}

cmDspRC_t _cmDsp1ofN_Exec( cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t         rc     = kOkDspRC;
  cmDsp1ofN_t*      p      = (cmDsp1ofN_t*)inst;
  unsigned          iChIdx = cmDspUInt(inst,kInChIdx1oId);
  cmSample_t*       dp     = cmDspAudioBuf(ctx,inst,kOutAudio1oId,0);
  const cmSample_t* sp     = cmDspAudioBuf(ctx,inst,p->baseInAudio1oId + iChIdx ,0);
  unsigned          n      = cmDspAudioBufSmpCount(ctx,inst,kOutAudio1oId,0);

  if( dp != NULL )
  {
    if( sp == NULL )
      cmVOS_Zero(dp,n);
    else
      cmVOS_Copy(dp,n,sp);
  }

  return rc;
}

cmDspRC_t _cmDsp1ofN_Recv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  cmDspRC_t    rc = kOkDspRC;
  cmDsp1ofN_t* p  = (cmDsp1ofN_t*)inst;


  // ignore out of range input channel
  if( evt->dstVarId == kInChIdx1oId && cmDsvGetUInt(evt->valuePtr) >= p->iChCnt )
  {
    cmDspInstErr(ctx,inst,kVarNotValidDspRC,"The selector channel index %i is out of range.",cmDsvGetUInt(evt->valuePtr));
    return kOkDspRC;
  } 
  
  // store the incoming value
  if((rc = cmDspSetEvent(ctx,inst,evt)) != kOkDspRC )
    return rc;


  // if this is an input data event
  if( p->baseBaseIn1oId <= evt->dstVarId  )
  {
    // get the input channel this event occurred on
    unsigned iChIdx = (evt->dstVarId - p->baseBaseIn1oId) % p->iChCnt;

    // if the event did not arrive on the selected input channel - there is nothing else to do
    if( iChIdx != cmDspUInt(inst,kInChIdx1oId) )
      return kOkDspRC;

    // The event arrived on the input channel - send it out the output

    // double
    if( p->baseInFloat1oId <= evt->dstVarId && evt->dstVarId < p->baseInFloat1oId + p->iChCnt )
      cmDspSetDouble(ctx,inst,kOutFloat1oId, cmDspDouble(inst,evt->dstVarId));
    else

      // bool  
    if( p->baseInBool1oId <= evt->dstVarId && evt->dstVarId < p->baseInBool1oId + p->iChCnt )
      cmDspSetBool(ctx,inst,kOutBool1oId, cmDspBool(inst,evt->dstVarId));
    else
      
      // symbol
    if( p->baseInSym1oId <= evt->dstVarId && evt->dstVarId < p->baseInSym1oId + p->iChCnt )
      cmDspSetSymbol(ctx,inst,kOutSym1oId, cmDspSymbol(inst,evt->dstVarId));
    
  }


  return rc;
}

struct cmDspClass_str* cm1ofNClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cm1ofN_DC,ctx,"1ofN",
    NULL,
    _cmDsp1ofN_Alloc,
    _cmDsp1ofN_Free,
    _cmDsp1ofN_Reset,
    _cmDsp1ofN_Exec,
    _cmDsp1ofN_Recv,
    NULL,NULL,
    " 1 of N Switch");

  return &_cm1ofN_DC;
}

//==========================================================================================================================================
// Send a 'true' out on the selected channel.
// Send a 'false' out on the deselected channel.
enum
{
  kChCnt1uId,
  kSel1uId,
  kBaseOut1uId
};

cmDspClass_t _cm1Up_DC;

typedef struct
{
  cmDspInst_t   inst;

} cmDsp1Up_t;

cmDspInst_t*  _cmDsp1Up_Alloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  if( va_cnt < 1 )
  {
    cmDspClassErr(ctx,classPtr,kVarArgParseFailDspRC,"The '1Up' constructor must given output channel count.");
    return NULL;
  }

  va_list vl1;
  va_copy(vl1,vl);

  int      chCnt       = va_arg(vl,int);

  cmDsp1Up_t* p = cmDspInstAllocV(cmDsp1Up_t,ctx,classPtr,instSymId,id,storeSymId,va_cnt,vl1,
    1,         "chcnt",  kChCnt1uId,      0, 0, kUIntDsvFl   | kReqArgDsvFl,               "Output channel count.",
    1,         "sel",    kSel1uId,        0, 0, kUIntDsvFl   | kOptArgDsvFl | kInDsvFl,    "Channel index selector.",
    chCnt,     "out",    kBaseOut1uId,    0, 0, kBoolDsvFl   | kOutDsvFl,                  "Gate outputs",
    0 );


  unsigned i;
  
  cmDspSetDefaultUInt( ctx, &p->inst, kSel1uId, 0.0, 0.0 );

  for(i=0; i<chCnt; ++i)
    cmDspSetDefaultBool(   ctx, &p->inst, kBaseOut1uId + i, false, false );

  va_end(vl1);

  return &p->inst;
}


cmDspRC_t _cmDsp1Up_Reset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t    rc = kOkDspRC;

  if((rc   = cmDspApplyAllDefaults(ctx,inst)) == kOkDspRC )
  {
    unsigned chIdx = cmDspUInt(inst,kSel1uId);
    unsigned chCnt = cmDspUInt(inst,kChCnt1uId);
    unsigned i;
    for(i=0; i<chCnt; ++i)
      cmDspSetBool(ctx,inst,kBaseOut1uId+i, i == chIdx );
  }

  return rc;  
}


cmDspRC_t _cmDsp1Up_Recv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  cmDspRC_t    rc = kOkDspRC;

  if( evt->dstVarId == kSel1uId)
  {
    unsigned chIdx = cmDspUInt(inst,kSel1uId);
    unsigned chCnt = cmDspUInt(inst,kChCnt1uId);
    
    // turn off the previously selected channel
    if( chIdx != cmInvalidIdx && chIdx < chCnt )
      cmDspSetBool(ctx,inst,kBaseOut1uId+chIdx,false);
    
    // set the new channel index
    cmDspSetEvent(ctx,inst,evt);

    // get the new channel index
    chIdx = cmDspUInt(inst,kSel1uId);

    // send the new channel index
    if( chIdx != cmInvalidIdx && chIdx < chCnt )
      cmDspSetBool(ctx,inst,kBaseOut1uId+chIdx,true);
    
  }

  return rc;
}

struct cmDspClass_str* cm1UpClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cm1Up_DC,ctx,"1Up",
    NULL,
    _cmDsp1Up_Alloc,
    NULL,
    _cmDsp1Up_Reset,
    NULL,
    _cmDsp1Up_Recv,
    NULL,NULL,
    "Set one input high and all others low.");

  return &_cm1Up_DC;
}

//==========================================================================================================================================
// Convert a 'true'/'false' gate to an 'on'/'off' symbol
enum
{
  kOnSymGsId,
  kOffSymGsId,
  kOnGsId,
  kOffGsId,
  kBothGsId,
  kOutGsId
};

cmDspClass_t _cmGateToSym_DC;

typedef struct
{
  cmDspInst_t   inst;
} cmDspGateToSym_t;

cmDspInst_t*  _cmDspGateToSym_Alloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspGateToSym_t* p = cmDspInstAllocV(cmDspGateToSym_t,ctx,classPtr,instSymId,id,storeSymId,va_cnt,vl,
    1,     "on_sym", kOnSymGsId,   0, 0, kSymDsvFl    | kInDsvFl | kOptArgDsvFl,"'on' symbol id (default:'on')",
    1,     "off_sym",kOffSymGsId,  0, 0, kSymDsvFl    | kInDsvFl | kOptArgDsvFl,"'off' symbol id (default:'off')",
    1,     "on",    kOnGsId,       0, 0, kBoolDsvFl   | kInDsvFl,                "On - send out 'on' symbol when a 'true' is received.",
    1,     "off",   kOffGsId,      0, 0, kBoolDsvFl   | kInDsvFl,                "Off - send out 'off' symbol when a 'false' is received.",
    1,     "both",  kBothGsId,     0, 0, kBoolDsvFl   | kInDsvFl,                "Send 'on' on 'true' and 'off' on 'false'.",
    1,     "out",   kOutGsId,      0, 0, kSymDsvFl    | kOutDsvFl,               "Output",
    0 );


  unsigned onSymId  = cmSymTblRegisterStaticSymbol(ctx->stH,"on");
  unsigned offSymId = cmSymTblRegisterStaticSymbol(ctx->stH,"off");

  cmDspSetDefaultSymbol( ctx, &p->inst, kOnSymGsId,  onSymId );
  cmDspSetDefaultSymbol( ctx, &p->inst, kOffSymGsId, offSymId );
  cmDspSetDefaultSymbol( ctx, &p->inst, kOutGsId, cmInvalidId );
  return &p->inst;
}


cmDspRC_t _cmDspGateToSym_Reset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t    rc = kOkDspRC;

  if((rc   = cmDspApplyAllDefaults(ctx,inst)) == kOkDspRC )
  {
  }

  return rc;  
}


cmDspRC_t _cmDspGateToSym_Recv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  cmDspRC_t    rc = kOkDspRC;

  if((rc = cmDspSetEvent(ctx,inst,evt)) == kOkDspRC )
  {
    unsigned onSymId  = cmDspSymbol(inst,kOnSymGsId);
    unsigned offSymId = cmDspSymbol(inst,kOffSymGsId);

    switch( evt->dstVarId )
    {
      case kOnGsId:
        if( cmDspBool(inst,kOnGsId) )
          cmDspSetSymbol(ctx,inst,kOutGsId,onSymId);
        break;

      case kOffGsId:
        if( !cmDspBool(inst,kOffGsId) )
          cmDspSetSymbol(ctx,inst,kOutGsId,offSymId);
        break;

      case kBothGsId:
        cmDspSetSymbol(ctx,inst, kOutGsId, cmDspBool(inst,kBothGsId) ? onSymId : offSymId);
        break;
    }

  }
  return rc;
}

struct cmDspClass_str* cmGateToSymClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmGateToSym_DC,ctx,"GateToSym",
    NULL,
    _cmDspGateToSym_Alloc,
    NULL,
    _cmDspGateToSym_Reset,
    NULL,
    _cmDspGateToSym_Recv,
    NULL,NULL,
    "Convert a 'true'/'false' gate to an 'on'/'off' symbol.");

  return &_cmGateToSym_DC;
}

//==========================================================================================================================================
enum
{
  kOutPtsId,
  kBaseInPtsId
};

cmDspClass_t _cmPortToSym_DC;

typedef struct
{
  cmDspInst_t inst;
  unsigned*   symIdArray;
  unsigned    symIdCnt;
  unsigned    baseOutPtsId;
} cmDspPortToSym_t;

cmDspInst_t*  _cmDspPortToSym_Alloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  va_list       vl1;
  va_copy(vl1,vl);

  if( va_cnt < 1 )
  {
    va_end(vl1);
    cmDspClassErr(ctx,classPtr,kVarArgParseFailDspRC,"The 'PortToSym' constructor argument list must contain at least one symbol label.");
    return NULL;
  }

  unsigned      symCnt    = va_cnt;
  unsigned      argCnt    = 1 + 2*symCnt;
  cmDspVarArg_t args[argCnt+1];

  unsigned* symIdArray   = cmMemAllocZ(unsigned,symCnt);
  unsigned  baseOutPtsId = kBaseInPtsId + symCnt;

  // setup the output port arg recd
  cmDspArgSetup(ctx,args,"out",cmInvalidId,kOutPtsId,0,0,kOutDsvFl | kSymDsvFl, "Output" );

  unsigned i;

  for(i=0; i<symCnt; ++i)
  {
    // get the symbol label
    const cmChar_t* symLabel = va_arg(vl,const char*);
    assert( symLabel != NULL );

    // register the symbol
    symIdArray[i] = cmSymTblRegisterSymbol(ctx->stH,symLabel);

    cmDspArgSetup(ctx, args+kBaseInPtsId+i, symLabel, cmInvalidId, kBaseInPtsId+i, 0, 0, kInDsvFl  | kTypeDsvMask, cmTsPrintfH(ctx->lhH,"%s Input.",symLabel) );

    cmDspArgSetup(ctx, args+baseOutPtsId+i, symLabel, cmInvalidId, baseOutPtsId+i, 0, 0, kOutDsvFl | kSymDsvFl,    cmTsPrintfH(ctx->lhH,"%s Output.",symLabel) );

  }

  cmDspArgSetupNull(args + argCnt);

  cmDspPortToSym_t* p = cmDspInstAlloc(cmDspPortToSym_t,ctx,classPtr,args,instSymId,id,storeSymId,0,vl1);

  p->symIdCnt     = symCnt;
  p->symIdArray   = symIdArray;
  p->baseOutPtsId = baseOutPtsId;

  cmDspSetDefaultSymbol(ctx,&p->inst,kOutPtsId,cmInvalidId);

  va_end(vl1);

  return &p->inst;
}
cmDspRC_t _cmDspPortToSym_Free(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspPortToSym_t* p = (cmDspPortToSym_t*)inst;
  cmMemFree(p->symIdArray);
  return kOkDspRC;
}

cmDspRC_t _cmDspPortToSym_Reset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  return cmDspApplyAllDefaults(ctx,inst);
}


cmDspRC_t _cmDspPortToSym_Recv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  cmDspRC_t    rc = kOkDspRC;
  cmDspPortToSym_t* p = (cmDspPortToSym_t*)inst;
    
  // if a msg of any type is recieved on an input port - send out the associated symbol
  if( kBaseInPtsId <= evt->dstVarId && evt->dstVarId < kBaseInPtsId + p->symIdCnt )
  {
    unsigned idx = evt->dstVarId - kBaseInPtsId;
    assert( idx < p->symIdCnt );
    cmDspSetSymbol(ctx,inst,p->baseOutPtsId + idx, p->symIdArray[idx]);
    return cmDspSetSymbol(ctx,inst,kOutPtsId,p->symIdArray[ evt->dstVarId - kBaseInPtsId ]);
  }

  return rc;
}

struct cmDspClass_str* cmPortToSymClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmPortToSym_DC,ctx,"PortToSym",
    NULL,
    _cmDspPortToSym_Alloc,
    _cmDspPortToSym_Free,
    _cmDspPortToSym_Reset,
    NULL,
    _cmDspPortToSym_Recv,
    NULL,NULL,
    "If a message of any kind is received on a port then send the symbol associated with the port.");

  return &_cmPortToSym_DC;
}

//==========================================================================================================================================
 
enum
{
  kOutChCntRtId,
  kOutChIdxRtId,
  kInFloatRtId,
  kInBoolRtId,
  kInSymRtId,
  kInAudioRtId,
  kBaseOutFloatRtId
};

cmDspClass_t _cmRouter_DC;

typedef struct
{
  cmDspInst_t   inst;

  unsigned      oChCnt;
  unsigned      oChIdx;

  unsigned      baseBaseOutRtId;  // first data input port id

  unsigned      baseInFloatRtId;
  unsigned      baseInBoolRtId;
  unsigned      baseInSymRtId;
  unsigned      baseInAudioRtId;

  unsigned      baseOutFloatRtId;
  unsigned      baseOutBoolRtId;
  unsigned      baseOutSymRtId;
  unsigned      baseOutAudioRtId;


} cmDspRouter_t;

cmDspInst_t*  _cmDspRouter_Alloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  if( va_cnt < 1 )
  {
    cmDspClassErr(ctx,classPtr,kVarArgParseFailDspRC,"The 'Router' constructor must be given an output channel count.");
    return NULL;
  }

  va_list vl1;
  va_copy(vl1,vl);

  int      oChCnt            = va_arg(vl,int);

  unsigned baseOutFloatRtId  = kBaseOutFloatRtId;
  unsigned baseOutBoolRtId   = baseOutFloatRtId  + oChCnt;
  unsigned baseOutSymRtId    = baseOutBoolRtId   + oChCnt;
  unsigned baseOutAudioRtId  = baseOutSymRtId    + oChCnt;

  cmDspRouter_t* p = cmDspInstAllocV(cmDspRouter_t,ctx,classPtr,instSymId,id,storeSymId,va_cnt,vl1,
    1,         "ochs",   kOutChCntRtId,     0, 0, kUIntDsvFl     | kReqArgDsvFl,"Output channel count.",
    1,         "sel",    kOutChIdxRtId,     0, 0, kUIntDsvFl     | kReqArgDsvFl | kInDsvFl,    "Output channel index selector.",
    1,         "f-in",   kInFloatRtId,      0, 0, kDoubleDsvFl   | kInDsvFl,     "Float input",
    1,         "b-in",   kInBoolRtId,       0, 0, kBoolDsvFl     | kInDsvFl,     "Bool input",
    1,         "s-in",   kInSymRtId,        0, 0, kSymDsvFl      | kInDsvFl,     "Symbol input",
    1,         "a-in",   kInAudioRtId,      0, 0, kAudioBufDsvFl | kInDsvFl,     "Audio input",
    oChCnt,    "f-out",  baseOutFloatRtId,  0, 0, kDoubleDsvFl   | kOutDsvFl,    "Float output",
    oChCnt,    "b-out",  baseOutBoolRtId,   0, 0, kBoolDsvFl     | kOutDsvFl,    "Bool output",
    oChCnt,    "s-out",  baseOutSymRtId,    0, 0, kSymDsvFl      | kOutDsvFl,    "Symbol output",
    oChCnt,    "a-out",  baseOutAudioRtId,  0, 1, kAudioBufDsvFl | kOutDsvFl,    "Audio output",
    0 );

  p->oChCnt     = oChCnt;

  p->baseBaseOutRtId   =  kBaseOutFloatRtId;
  p->baseOutFloatRtId  =  baseOutFloatRtId; 
  p->baseOutBoolRtId   =  baseOutBoolRtId;
  p->baseOutSymRtId    =  baseOutSymRtId;
  p->baseOutAudioRtId  =  baseOutAudioRtId;

  unsigned i;
  for(i=0; i<oChCnt; ++i)
  {
    cmDspSetDefaultDouble( ctx, &p->inst, baseOutFloatRtId+i, 0.0, 0.0 );
    cmDspSetDefaultBool(   ctx, &p->inst, baseOutBoolRtId+i, false, false );
    cmDspSetDefaultSymbol( ctx, &p->inst, baseOutSymRtId+i, cmInvalidId );
  }

  va_end(vl1);

  return &p->inst;
}

cmDspRC_t _cmDspRouter_Free(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  return kOkDspRC;
}

cmDspRC_t _cmDspRouter_Reset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t      rc = kOkDspRC;
  cmDspRouter_t* p  = (cmDspRouter_t*)inst;

  if((rc   = cmDspApplyAllDefaults(ctx,inst)) == kOkDspRC )
  {
    unsigned i;
    for(i=0; i<p->oChCnt; ++i)
      cmDspZeroAudioBuf(ctx,inst,p->baseOutAudioRtId+i);
  }

  return rc;  
}

cmDspRC_t _cmDspRouter_Exec( cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t         rc     = kOkDspRC;
  cmDspRouter_t*    p      = (cmDspRouter_t*)inst;
  unsigned          oChIdx = cmDspUInt(inst,kOutChIdxRtId);
  cmSample_t*       dp     = cmDspAudioBuf(ctx,inst,p->baseOutAudioRtId + oChIdx,0);
  const cmSample_t* sp     = cmDspAudioBuf(ctx,inst,kInAudioRtId ,0);
  unsigned          n      = cmDspAudioBufSmpCount(ctx,inst,p->baseOutAudioRtId,0);

  if( dp != NULL )
  {
    if( sp == NULL )
      cmVOS_Zero(dp,n);
    else
      cmVOS_Copy(dp,n,sp);
  }

  return rc;
}

cmDspRC_t _cmDspRouter_Recv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  cmDspRC_t    rc = kOkDspRC;
  cmDspRouter_t* p  = (cmDspRouter_t*)inst;


  // ignore out of range output channel selection
  if( evt->dstVarId == kOutChIdxRtId && cmDsvGetUInt(evt->valuePtr) >= p->oChCnt )
  {
    cmDspInstErr(ctx,inst,kVarNotValidDspRC,"The selector channel index %i  is out of of range.",cmDsvGetUInt(evt->valuePtr));
    return kOkDspRC;
  } 


  // store the incoming value
  if( evt->dstVarId < p->baseBaseOutRtId )
    if((rc = cmDspSetEvent(ctx,inst,evt)) != kOkDspRC )
      return rc;

  unsigned chIdx = cmDspUInt(inst,kOutChIdxRtId);


  switch( evt->dstVarId )
  {
    case kInFloatRtId:
      cmDspSetDouble(ctx,inst,p->baseOutFloatRtId + chIdx, cmDspDouble(inst,kInFloatRtId) );
      break;

    case kInBoolRtId:
      cmDspSetBool(ctx,inst,p->baseOutBoolRtId + chIdx, cmDspBool(inst,kInBoolRtId) );
      break;

    case kInSymRtId:
      cmDspSetSymbol(ctx,inst,p->baseOutSymRtId + chIdx, cmDspSymbol(inst,kInSymRtId));
      break;
  }

  return rc;
}

struct cmDspClass_str* cmRouterClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmRouter_DC,ctx,"Router",
    NULL,
    _cmDspRouter_Alloc,
    _cmDspRouter_Free,
    _cmDspRouter_Reset,
    _cmDspRouter_Exec,
    _cmDspRouter_Recv,
    NULL,NULL,
    "1 to N Router");

  return &_cmRouter_DC;
}

//==========================================================================================================================================
// Purpose: AvailCh can be used to implement a channel switching circuit.
// 
// Inputs:
//   chs         - The count of channels. Constructor only argument. 
//   trig        - Any input causes the next available channel, i, to be enabled.
//                 gate[i] transmits 'true'.  In 'exclusive (0) mode all active
//                 channels are then requested to shutdown by transmitting 'false' on
//                 gate[] - only the new channel will be active.  In 'multi' (1) mode
//                 no signal is sent out the gate[].
//   dis[chCnt]  - Recieves a gate signal from an external object which indicates 
//                 when a channel is no longer active. When a 'false' is received on dis[i]
//                 the channel i is marked as available.  In 'multi' mode 'false' is 
//                 then transmitted on gate[i].
// Outputs:
//   gate[chCnt] - 'true' is transmitted when a channel is made active (see trig)
//                 'false' is transmitted to notify the channel that it should shutdown.
//                 The channel is not considered actually shutdown until dis[i]
//                 recieves a 'false'.
// Notes:
//   The gate[] output is designed to work with the gate[] input of Xfader.  When
//   availCh.gate[] goes high Xfader fades in, when availCh.gate[] goes low
//   Xfader fades out.  The dis[] channel is designed to connect from Xfader.state[].
//   Xfader.state[] goes low when a fade-out is complete, the connected AvailCh 
//   is then marked as available. 
enum
{
  kChCntAvId,
  kModeAvId,
  kTrigAvId,
  kResetAvId,
  kChIdxAvId,  
  kBaseDisInAvId,

  kExclusiveModeAvId=0,
  kMultiModeAvId=1
};

cmDspClass_t _cmAvailCh_DC;

typedef struct
{
  cmDspInst_t   inst;
  unsigned      chCnt;
  unsigned      baseDisInAvId;
  unsigned      baseGateOutAvId;
  bool*         stateArray;
} cmDspAvailCh_t;

cmDspInst_t*  _cmDspAvailCh_Alloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  if( va_cnt < 1 )
  {
    cmDspClassErr(ctx,classPtr,kVarArgParseFailDspRC,"The 'AvailCh' constructor must be given an channel count.");
    return NULL;
  }

  va_list vl1;
  va_copy(vl1,vl);

  int     chCnt            = va_arg(vl,int);

  if( chCnt <= 0 )
  {
    va_end(vl1);
    cmDspClassErr(ctx,classPtr,kInvalidArgDspRC,"The 'AvailCh' constructor must be given a positive channel count.");
    return NULL;
  }

  unsigned baseDisInAvId    = kBaseDisInAvId;
  unsigned baseGateOutAvId  = baseDisInAvId + chCnt;

  cmDspAvailCh_t* p = cmDspInstAllocV(cmDspAvailCh_t,ctx,classPtr,instSymId,id,storeSymId,va_cnt,vl1,
    1,         "chs",    kChCntAvId,        0, 0, kUIntDsvFl     | kReqArgDsvFl, "Channel count.",
    1,         "mode",   kModeAvId,         0, 0, kUIntDsvFl     | kInDsvFl,     "Mode: 0=exclusive (dflt) 1=multi",
    1,         "trig",   kTrigAvId,         0, 0, kTypeDsvMask   | kInDsvFl,     "Trigger the unit to select the next available channel.", 
    1,         "reset",  kResetAvId,        0, 0, kBoolDsvFl     | kInDsvFl | kOutDsvFl,     "Reset to default state",
    1,         "ch",     kChIdxAvId,        0, 0, kUIntDsvFl     | kOutDsvFl,    "Currently selected channel.",
    chCnt,     "dis",    baseDisInAvId,     0, 0, kBoolDsvFl     | kInDsvFl,     "Disable channel gate",
    chCnt,     "gate",   baseGateOutAvId,   0, 0, kBoolDsvFl     | kOutDsvFl,    "Active channel gate",
    0 );

  p->chCnt     = chCnt;

  p->baseDisInAvId   =  baseDisInAvId;
  p->baseGateOutAvId =  baseGateOutAvId;

  unsigned i;
  for(i=0; i<chCnt; ++i)
  {
    cmDspSetDefaultBool( ctx, &p->inst, baseDisInAvId+i,  false, false );
    cmDspSetDefaultBool( ctx, &p->inst, baseGateOutAvId+i, false, false );
  }
  cmDspSetDefaultUInt( ctx, &p->inst, kModeAvId,  0, kExclusiveModeAvId );
  cmDspSetDefaultUInt( ctx, &p->inst, kChIdxAvId, 0, cmInvalidIdx );
  
  va_end(vl1);

  return &p->inst;
}

cmDspRC_t _cmDspAvailCh_Free(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  return kOkDspRC;
}

cmDspRC_t _cmDspAvailCh_Reset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  return cmDspApplyAllDefaults(ctx,inst);
}


cmDspRC_t _cmDspAvailCh_Recv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  cmDspRC_t    rc = kOkDspRC;
  cmDspAvailCh_t* p  = (cmDspAvailCh_t*)inst;

  bool exclModeFl  = cmDspUInt(inst, kModeAvId ) == kExclusiveModeAvId;

  if( evt->dstVarId == kResetAvId )
  {
    unsigned i;      

    cmDspSetUInt(ctx,inst,kChIdxAvId,0);

    for(i=0; i<p->chCnt; ++i)
    {
      bool fl = i==0;
      cmDspSetBool(ctx, inst, p->baseDisInAvId   + i, !fl);
      cmDspSetBool(ctx, inst, p->baseGateOutAvId + i, fl);
    }

    cmDspSetBool(ctx,inst, kResetAvId, false );

    return rc;
  }

  // if this is a trigger
  if( evt->dstVarId == kTrigAvId )
  {
    unsigned i,j=-1;
    bool     fl = true;
    for(i=0; i<p->chCnt; ++i)
    {
      // the channel's active state is held in the 'dis' variable.
      bool activeFl = cmDspBool(inst,p->baseDisInAvId+i);

      // if ch[i] is the first avail inactive channel
      if( fl && !activeFl )
      {
        // activate the available channel
        //cmDspSetUInt(ctx,inst,kChIdxAvId,j);
        //cmDspSetBool(ctx, inst, p->baseDisInAvId   + j, true);
        //cmDspSetBool(ctx, inst, p->baseGateOutAvId + j, true);

        j  = i;
        fl = false;
      }

      // if ch[i] is active - then request that it shutdown 
      //if( activeFl && exclModeFl)
      //  cmDspSetBool(ctx, inst, p->baseGateOutAvId + i, false);

    }

    if( j==-1 )
      cmDspInstErr(ctx,inst,kInvalidStateDspRC,"No available channels exist.");
    else
    {
      // activate the available channel
      cmDspSetUInt(ctx,inst,kChIdxAvId,j);
      cmDspSetBool(ctx, inst, p->baseDisInAvId   + j, true);
      cmDspSetBool(ctx, inst, p->baseGateOutAvId + j, true);

      if( exclModeFl )
      {
        for(i=0; i<p->chCnt; ++i)
          if( i!=j && cmDspBool(inst,p->baseDisInAvId+i) )
            cmDspSetBool(ctx,inst, p->baseGateOutAvId+i, false );
      }
    }

    
    return rc;
  }

  

  // if this is an incoming disable message.
  if( p->baseDisInAvId <= evt->dstVarId && evt->dstVarId < p->baseDisInAvId+p->chCnt && cmDsvGetBool(evt->valuePtr) == false)
  {
    cmDspSetEvent(ctx,inst,evt);
    if( !exclModeFl )
      cmDspSetBool(ctx, inst, p->baseGateOutAvId + (evt->dstVarId - p->baseDisInAvId), false);
  }

  return rc;
}

struct cmDspClass_str* cmAvailChClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmAvailCh_DC,ctx,"AvailCh",
    NULL,
    _cmDspAvailCh_Alloc,
    _cmDspAvailCh_Free,
    _cmDspAvailCh_Reset,
    NULL,
    _cmDspAvailCh_Recv,
    NULL,NULL,
    "Enable the next availabled channel");

  return &_cmAvailCh_DC;
}

 
//==========================================================================================================================================
 
enum
{
  kGroupSymPrId,
  kLabelPrId,
  kCmdPrId,
  kDonePrId,
  kListPrId,
  kSelPrId
};

cmDspClass_t _cmPreset_DC;

typedef struct
{
  cmDspInst_t   inst;
  unsigned      storeCmdSymId;
  unsigned      recallCmdSymId;
  unsigned      doneSymId;
  cmJsonH_t     jsH;
  cmJsonNode_t* np;
} cmDspPreset_t;

cmDspRC_t  _cmDspPresetUpdateList( cmDspCtx_t* ctx, cmDspPreset_t* p, unsigned groupSymId )
{
  cmDspRC_t rc = kOkDspRC;

  // initialize the JSON tree
  if( cmJsonInitialize(&p->jsH,ctx->cmCtx) != kOkJsRC )
  {
    rc = cmDspInstErr(ctx,&p->inst,kJsonFailDspRC,"JSON preset list handle initialization failed.");
    goto errLabel;
  }

  // create the JSON tree root container
  if( cmJsonCreateObject(p->jsH,NULL) == NULL )
  {
    rc = cmDspInstErr(ctx,&p->inst,kJsonFailDspRC,"JSON preset list root object create failed.");
    goto errLabel;    
  }

  // if a valid preset group symbol was given
  if( groupSymId != cmInvalidId )
  {
    // get the JSON list containing the preset labels and symId's for this preset group
    if( cmDspSysPresetPresetJsonList(ctx->dspH, groupSymId, &p->jsH ) != kOkDspRC )
    {
      rc = cmDspInstErr(ctx,&p->inst,kSubSysFailDspRC,"Request for a preset list failed.");
      goto errLabel;
    }

    // get a pointer to the JSON 'presetArray' array node
    if(( p->np = cmJsonFindValue(p->jsH,"presetArray",NULL,kArrayTId)) == NULL )
    {
      rc = cmDspInstErr(ctx,&p->inst,kJsonFailDspRC,"Preset list is empty or synatax is not recognized.");
      goto errLabel;      
    }
    
    // set the JSON list
    if((rc = cmDspSetJson(ctx,&p->inst,kListPrId,p->np)) != kOkDspRC )
    {
      rc = cmDspInstErr(ctx,&p->inst,rc,"Preset list set failed.");
      goto errLabel;
    }
  }
 errLabel:
  return rc;
}

cmDspRC_t _cmDspPresetDoRecall( cmDspCtx_t* ctx, cmDspPreset_t* p, const cmChar_t* groupLabel, const cmChar_t* presetLabel )
{
  cmDspRC_t rc;

  // recall the preset
  if(( rc = cmDspSysPresetRecall(ctx->dspH, groupLabel, presetLabel )) != kOkDspRC )
    return cmDspInstErr(ctx,&p->inst,kSubSysFailDspRC,"Preset recall failed for group:'%s' preset:'%s'.",cmStringNullGuard(groupLabel),cmStringNullGuard(presetLabel));
 
 // send out a notification that a new preset has been loaded
  return cmDspSetSymbol(ctx,&p->inst,kDonePrId,p->doneSymId);
}

// selIdx is base 1, not base 0, because it references the JSON tree rows where the
// first row contains the titles.
cmDspRC_t _cmDspPresetListSelectRecall( cmDspCtx_t* ctx, cmDspPreset_t* p, unsigned selIdx )
{
  cmDspRC_t rc = kOkDspRC;
  const cmChar_t* presetLabel;
  const cmChar_t* groupLabel;
  unsigned        groupSymId;
  unsigned        presetSymId;
  const cmJsonNode_t* rnp;

  // validate the JSON tree
  if( cmJsonIsValid(p->jsH) == false || p->np == NULL )
  {
    rc = cmDspInstErr(ctx,&p->inst,kJsonFailDspRC,"Preset recall failed. The preset JSON tree does not exist.");
    goto errLabel;
  }

  // validate the group id
  if( (groupSymId = cmDspSymbol(&p->inst,kGroupSymPrId)) == cmInvalidId ) 
  {
    rc = cmDspInstErr(ctx,&p->inst,kVarNotValidDspRC,"Preset recall failed. The preset group symbol has not been set.");
    goto errLabel;
  }

  // validate the selection index
  if( selIdx >= cmJsonChildCount(p->np) )
  {
    rc = cmDspInstErr(ctx,&p->inst,kVarNotValidDspRC,"Preset recall failed. The preset index: %i is out of range 0-%i", selIdx, cmJsonChildCount(p->np));
    goto errLabel;    
  } 

  // get the preset element
  if(( rnp = cmJsonArrayElementC(p->np, selIdx )) == NULL )
  {
    rc = cmDspInstErr(ctx,&p->inst,kJsonFailDspRC,"Preset recall failed. Unable to retrieve preset JSON element.");
    goto errLabel;    
  }

  // verify the JSON syntax
  assert( rnp->typeId==kArrayTId && cmJsonChildCount(rnp)==2 && cmJsonArrayElementC(rnp,1)->typeId == kIntTId );

  // get the preset symbol id
  if( cmJsonUIntValue( cmJsonArrayElementC(rnp,1), &presetSymId ) != kOkJsRC )
  {
    rc = cmDspInstErr(ctx,&p->inst,kJsonFailDspRC,"Preset recall failed. Unable to retrieve preset symbol id.");
    goto errLabel;    
  }

  // convert symbols to strings
  groupLabel  = cmSymTblLabel(ctx->stH,groupSymId);
  presetLabel = cmSymTblLabel(ctx->stH,presetSymId);
  
  rc = _cmDspPresetDoRecall(ctx,p,groupLabel,presetLabel);

 errLabel:
  return rc;
}

cmDspInst_t*  _cmDspPreset_Alloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{

  cmDspPreset_t* p = cmDspInstAllocV(cmDspPreset_t,ctx,classPtr,instSymId,id,storeSymId,va_cnt,vl,
    1,         "sym",    kGroupSymPrId,     0, 0, kInDsvFl  | kSymDsvFl  | kReqArgDsvFl, "Preset group symbol.",
    1,         "label",  kLabelPrId,        0, 0, kInDsvFl  | kStrzDsvFl | kOptArgDsvFl, "Preset label",
    1,         "cmd",    kCmdPrId,          0, 0, kInDsvFl  | kSymDsvFl,                 "Command input",
    1,         "done",   kDonePrId,         0, 0, kOutDsvFl | kSymDsvFl,                 "Send 'done' symbol after preset recall.",
    1,         "list",   kListPrId,         0, 0, kInDsvFl  | kJsonDsvFl,                "Preset list as a JSON array.",
    1,         "sel",    kSelPrId,          0, 0, kInDsvFl  | kUIntDsvFl,                "Preset index selection index",
    0 );

  p->jsH            = cmJsonNullHandle;
  p->np             = NULL;
  p->storeCmdSymId  = cmSymTblRegisterStaticSymbol(ctx->stH,"store");
  p->recallCmdSymId = cmSymTblRegisterStaticSymbol(ctx->stH,"recall"); 
  p->doneSymId      = cmSymTblRegisterStaticSymbol(ctx->stH,"done"); 
  
  cmDspSetDefaultBool(   ctx, &p->inst, kDonePrId,false,false);
  cmDspSetDefaultSymbol( ctx, &p->inst, kGroupSymPrId, cmInvalidId);
  cmDspSetDefaultStrcz(  ctx, &p->inst, kLabelPrId,    NULL,"");

  unsigned height = 5;
  cmDspUiMsgListCreate(ctx, &p->inst, height, kListPrId, kSelPrId );

  return &p->inst;
}

cmDspRC_t _cmDspPreset_Free(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspPreset_t* p = (cmDspPreset_t*)inst;

  cmJsonFinalize(&p->jsH);
  return kOkDspRC;
}

cmDspRC_t _cmDspPreset_Reset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t rc;
  cmDspPreset_t* p  = (cmDspPreset_t*)inst;

  if((rc = cmDspApplyAllDefaults(ctx,inst)) == kOkDspRC )
  {
    _cmDspPresetUpdateList(ctx, p, cmDspSymbol(inst,kGroupSymPrId) );
  }

  return rc;
}

cmDspRC_t _cmDspPreset_Recv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  cmDspRC_t    rc = kOkDspRC;
  cmDspPreset_t* p  = (cmDspPreset_t*)inst;

  switch( evt->dstVarId )
  {
    case kListPrId:
      return rc;  // we don't yet handle lists arriving by the input port

    case kSelPrId:
      {
        // sel idx is base 1 because the first row in the msg list contains the titles
        unsigned  selIdx = cmDsvGetUInt(evt->valuePtr);
        if((rc = _cmDspPresetListSelectRecall(ctx,p,selIdx)) != kOkDspRC)
          return rc;
      }
      break;
  }

  if((rc = cmDspSetEvent(ctx,inst,evt)) != kOkDspRC )
    return rc;
  
  // if this is a store or recall command
  if( evt->dstVarId == kCmdPrId )
  {
    unsigned cmdSymId = cmDspSymbol(inst,kCmdPrId);

    if(  cmdSymId == p->storeCmdSymId || cmdSymId==p->recallCmdSymId )
    {
      unsigned groupSymId;
      const cmChar_t* groupLabel;
      const cmChar_t* presetLabel;
      
      // get the group symbol
      if((groupSymId = cmDspSymbol(inst,kGroupSymPrId)) == cmInvalidId )
        return cmDspInstErr(ctx,inst,kVarNotValidDspRC,"The preset group symbol id is not set.");
     
      // get the group label
      if((groupLabel = cmSymTblLabel(ctx->stH,groupSymId)) == NULL )
        return cmDspInstErr(ctx,inst,kVarNotValidDspRC,"The preset group label was not found.");

      // get the preset label
      if(( presetLabel = cmDspStrcz(inst,kLabelPrId)) == NULL || strlen(presetLabel)==0 )
        return cmDspInstErr(ctx,inst,kVarNotValidDspRC,"The preset label was not set.");

      // if this is a store command
      if( cmdSymId == p->storeCmdSymId )
      {
        // create a new preset
        if((rc = cmDspSysPresetCreate(ctx->dspH,groupLabel, presetLabel)) != kOkDspRC )
          return cmDspInstErr(ctx,inst,kSubSysFailDspRC,"Preset create failed for group:'%s' preset:'%s'.",cmStringNullGuard(groupLabel),cmStringNullGuard(presetLabel));
        
        // update the list with the new preset
        rc =  _cmDspPresetUpdateList(ctx, p, groupSymId );
      }
      else // otherwise this must be a recall command
      {
        rc = _cmDspPresetDoRecall(ctx,p,groupLabel, presetLabel);
      }
    }
  }

  return rc;
}

struct cmDspClass_str* cmPresetClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmPreset_DC,ctx,"Preset",
    NULL,
    _cmDspPreset_Alloc,
    _cmDspPreset_Free,
    _cmDspPreset_Reset,
    NULL,
    _cmDspPreset_Recv,
    NULL,NULL,
    "Preset Manager");

  return &_cmPreset_DC;
}

 
//==========================================================================================================================================

enum
{
  kAttrBcId,
  kMsgBcId
};

cmDspClass_t _cmBcastSym_DC;

typedef struct
{
  cmDspInst_t   inst;
  unsigned onSymId;
  unsigned offSymId;
} cmDspBcastSym_t;

cmDspInst_t*  _cmDspBcastSym_Alloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspBcastSym_t* p = cmDspInstAllocV(cmDspBcastSym_t,ctx,classPtr,instSymId,id,storeSymId,va_cnt,vl,
    1,     "attr",  kAttrBcId,    0, 0, kSymDsvFl    | kInDsvFl | kOptArgDsvFl,    "Instance which have this attribute symbol will receive the message.",
    1,     "msg",   kMsgBcId,     0, 0, kTypeDsvMask | kInDsvFl,    "Msg to broadcast.",
    0 );

  cmDspSetDefaultSymbol( ctx, &p->inst, kAttrBcId, cmInvalidId );

  return &p->inst;
}


cmDspRC_t _cmDspBcastSym_Reset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t    rc = kOkDspRC;

  if((rc   = cmDspApplyAllDefaults(ctx,inst)) == kOkDspRC )
  {
  }

  return rc;  
}


cmDspRC_t _cmDspBcastSym_Recv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  cmDspRC_t    rc = kOkDspRC;

  if( evt->dstVarId == kAttrBcId )
    return cmDspSetEvent(ctx,inst,evt);

  if( evt->dstVarId == kMsgBcId )
  {
    unsigned attrSymId = cmDspSymbol(inst,kAttrBcId);
    if( cmDsvIsSymbol(evt->valuePtr) )
    {
      printf("bcast: %i %s\n",attrSymId,cmSymTblLabel(ctx->stH,cmDsvSymbol(evt->valuePtr)));
    }

    return cmDspSysBroadcastValue(ctx->dspH, attrSymId, evt->valuePtr );
  }

  return rc;
}

struct cmDspClass_str* cmBcastSymClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmBcastSym_DC,ctx,"BcastSym",
    NULL,
    _cmDspBcastSym_Alloc,
    NULL,
    _cmDspBcastSym_Reset,
    NULL,
    _cmDspBcastSym_Recv,
    NULL,NULL,
    "Set one input high and all others low.");

  return &_cmBcastSym_DC;
}

//==========================================================================================================================================
enum
{
  kRsrcSlId,
  kCmdSlId,
  kTrigSlId,
  kOutSlId,
};

cmDspClass_t _cmSegLineDC;


typedef struct
{
  cmDspInst_t inst;
  double*     x;   // x[n]
  unsigned    n;
  unsigned    i;   // current segment 
  unsigned    m;   // cur cnt
} cmDspSegLine_t;

cmDspInst_t*  _cmDspSegLineAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{

  cmDspSegLine_t* p = cmDspInstAllocV(cmDspSegLine_t,ctx,classPtr,instSymId,id,storeSymId,va_cnt,vl,
    1,   "rsrc",  kRsrcSlId,  0, 0, kInDsvFl   | kStrzDsvFl | kReqArgDsvFl, "Name of resource array.",
    1,   "cmd",   kCmdSlId,   0, 0, kInDsvFl   | kSymDsvFl,                 "Command",
    1,   "trig",  kTrigSlId,  0, 0, kInDsvFl   | kTypeDsvMask,              "Trigger",
    1,   "out",   kOutSlId,   0, 0, kOutDsvFl  | kDoubleDsvFl,              "Output",
    0 );

  if( p == NULL )
    return NULL;
  
  // The array is expected to contain interleaved values:
  // cnt_0, val_0, cnt_1, val_1 .... cnt_n val_n
  // The 'cnt_x' values give the count of trigger values upon which the output will be 'val_x'.

  if( cmDspRsrcDblArray(ctx->dspH,&p->n,&p->x,cmDspDefaultStrcz(&p->inst,kRsrcSlId),NULL) != kOkDspRC )
  {
    cmDspClassErr(ctx,classPtr,kVarArgParseFailDspRC,"The 'SegLine' constructor resource array could not be read.");
    return NULL;
  }


  cmDspSetDefaultDouble( ctx, &p->inst, kOutSlId, 0.0, p->n >= 2 ? p->x[1] : 0.0 );

  p->i = 0;
  p->m = 0;

  return &p->inst;
}


cmDspRC_t _cmDspSegLineReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspSegLine_t* p   = (cmDspSegLine_t*)inst;
  
  p->i = 0;
  p->m = 0;

  return cmDspApplyAllDefaults(ctx,inst);
}


cmDspRC_t _cmDspSegLineRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  cmDspRC_t rc = kOkDspRC;
  cmDspSegLine_t* p = (cmDspSegLine_t*)inst;

  if((rc = cmDspSetEvent(ctx,inst,evt)) == kOkDspRC )
  {
    switch( evt->dstVarId )
    {
      case kTrigSlId:
        {
          double val = cmDspDouble(inst,kOutSlId);
          if( p->i < p->n )
          {
            if( p->m >= p->x[p->i] )
              p->i += 2;

            if( p->i < p->n )
            {
              double x0 = p->x[p->i-2];
              double y0 = p->x[p->i-1];
              double x1 = p->x[p->i+0];
              double y1 = p->x[p->i+1];

              double dx = x1 - x0;
              double dy = y1 - y0;
              
              val = y0 + (p->m - x0) * dy / dx;

              printf("i:%i m=%i x0:%f y0:%f x1:%f y1:%f : %f\n",p->i,p->m,x0,y0,x1,y1,val);

            }
            else
            {
              val = p->x[p->n-1];
            }

            ++p->m;
                        
          }
          cmDspSetDouble(ctx,inst,kOutSlId,val);
        }
        break;

      case kCmdSlId:
        p->i = 0;
        p->m = 0;
        break;
    }
  }
  return rc;
}

struct cmDspClass_str* cmSegLineClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmSegLineDC,ctx,"SegLine",
    NULL,
    _cmDspSegLineAlloc,
    NULL,
    _cmDspSegLineReset,
    NULL,
    _cmDspSegLineRecv,
    NULL,NULL,
    "SegLine");

  return &_cmSegLineDC;
}

