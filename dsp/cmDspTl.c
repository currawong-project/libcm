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
#include "cmMath.h"
#include "cmFile.h"
#include "cmFileSys.h"
#include "cmSymTbl.h"
#include "cmJson.h"
#include "cmPrefs.h"
#include "cmProcObj.h"
#include "cmDspValue.h"
#include "cmDspClass.h"
#include "cmDspFx.h"
#include "cmMsgProtocol.h"
#include "cmThread.h"
#include "cmUdpPort.h"
#include "cmUdpNet.h"
#include "cmAudioSys.h"
#include "cmDspSys.h"


//==========================================================================================================================================
enum
{
  kFnTlId,
  kAudOutTlId,
  kMidOutTlId,
  kEvtOutTlId
};

cmDspClass_t _cmTimeLineDC;

typedef struct
{
  cmDspInst_t inst;
  cmTlH_t     tlH;
  
} cmDspTimeLine_t;


cmDspInst_t*  _cmDspTimeLineAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  unsigned chs = 1;

  cmDspVarArg_t args[] =
  {
    { "fn",     kFnTlId,     0, 0,   kInDsvFl  | kStrzDsvFl,        "Time line file" },
    { "a-out",  kAudOutTlId, 0, chs, kOutDsvFl | kAudioBufDsvFl,    "Audio output."  },
    { "m-out",  kMidOutTlId, 0, 0,   kOutDsvFl | k
    { NULL, 0, 0, 0, 0 }
  };

  // allocate the instance
  cmDspTimeLine_t* p = cmDspInstAlloc(cmDspTimeLine_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);

  // assign default values to any of the the optional arg's which may not have been set from vl.
  cmDspSetDefaultDouble(ctx, &p->inst, kMaxTlId,  0.0, DBL_MAX);
  cmDspSetDefaultDouble(ctx, &p->inst, kMultTlId, 0.0, 1.0);
  cmDspSetDefaultDouble(ctx, &p->inst, kPhsTlId,  0.0, 0.0);

  return &p->inst;
}

cmDspRC_t _cmDspTimeLineReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspApplyAllDefaults(ctx,inst);
  cmDspZeroAudioBuf( ctx, inst, kOutTlId );
  return kOkDspRC;
}

cmDspRC_t _cmDspTimeLineExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmSample_t*       bp   = cmDspAudioBuf(ctx,inst,kOutTlId,0);
  const cmSample_t* ep   = bp + cmDspAudioBufSmpCount(ctx,inst,kOutTlId,0);
  double            mult = cmDspDouble(inst,kMultTlId);
  double            max  = cmDspDouble(inst,kMaxTlId);
  double            phs  = cmDspDouble(inst,kPhsTlId);
  double            inc  = mult;

  for(; bp<ep; ++bp)
  {
    while( phs >= max )
      phs -= max;

    *bp = phs;

    phs += inc;

  }

  cmDspSetDouble(ctx,inst,kPhsTlId,phs);
  
  return kOkDspRC;
}

cmDspRC_t _cmDspTimeLineRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  switch( evt->dstVarId )
  {
    case kMultTlId:
    case kMaxTlId:
    case kPhsTlId:
      cmDspSetEvent(ctx, inst, evt );;
      break;

    default:
      { assert(0); }
  }

  return kOkDspRC;
}

struct cmDspClass_str* cmTimeLineClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmTimeLineDC,ctx,"TimeLine",
    NULL,
    _cmDspTimeLineAlloc,
    NULL,
    _cmDspTimeLineReset,
    _cmDspTimeLineExec,
    _cmDspTimeLineRecv,
    NULL,
    NULL,
    "Ramp wave signal generator.");

  return &_cmTimeLineDC;
}



typedef struct 

typedef struct cmPerfEvt_str
{
  unsigned pitch;
  unsigned dyn;
  unsigned ioiTicks;
} cmPerfEvt_t;



