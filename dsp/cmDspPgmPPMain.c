#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmFileSys.h"
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
#include "cmProcObj.h"
#include "cmDspCtx.h"
#include "cmDspClass.h"
#include "cmDspSys.h"
#include "cmDspPgm.h"
#include "cmDspPgmPPMain.h"

#include "cmAudioFile.h"
#include "cmProcObj.h"
#include "cmProc.h"
#include "cmProc3.h"
#include "cmVectOpsTemplateMain.h"

#define COL_WIDTH (150)




cmDspInst_t** allocInstPtrArray( cmDspSysH_t h, unsigned cnt )
{ return cmLhAllocZ(cmDspSysLHeap(h),cmDspInst_t*,cnt); }


#define formLabel( h, lbl, i) cmTsPrintfH( cmDspSysLHeap(h), "%s-%i", lbl, i )



//=====================================================================================================================
cmDspInst_t*  _cmDspSys_PresetMgmt( cmDspSysH_t h, const cmChar_t* preLbl, unsigned presetGroupSymId )
{
  cmLHeapH_t   lhH       = cmDspSysLHeap(h);
  cmDspInst_t* preset    = cmDspSysAllocInst( h, "Preset", NULL, 1, presetGroupSymId );
  cmDspInst_t* presetLbl = cmDspSysAllocInst( h, "Text",   NULL,      2, "", "Preset Label" );
  cmDspInst_t* storeBtn  = cmDspSysAllocInst( h, "Button", cmTsPrintfH(lhH,"%s-store", preLbl),  2, kButtonDuiId, 0.0);
  cmDspInst_t* recallBtn = cmDspSysAllocInst( h, "Button", cmTsPrintfH(lhH,"%s-recall",preLbl),  2, kButtonDuiId, 0.0);
  cmDspInst_t* ptsStore = cmDspSysAllocInst(  h, "PortToSym", NULL, 1, "store" );
  cmDspInst_t* ptsRecall = cmDspSysAllocInst( h, "PortToSym", NULL, 1, "recall" );

  cmDspSysInstallCb(   h, presetLbl, "val", preset, "label",NULL);
  cmDspSysInstallCb(   h, storeBtn,  "out", ptsStore, "store", NULL );
  cmDspSysInstallCb(   h, recallBtn, "out", ptsRecall, "recall", NULL );
  cmDspSysInstallCb(   h, ptsStore,  "out", preset, "cmd", NULL );
  cmDspSysInstallCb(   h, ptsRecall, "out", preset, "cmd", NULL );


  return preset;
}
//=====================================================================================================================
typedef struct
{
  unsigned        circuitSymId;  // enable/disable symId

  cmDspInst_t**   aout;    // aout[ ctx->outChCnt ]
  const cmChar_t* aoutLbl; // "out" 
  
} cmDspPP_Circuit_t;

//=====================================================================================================================
typedef struct
{
  cmDspPP_Circuit_t c;
  
  cmDspInst_t** sfmt;
  cmDspInst_t** rmtr;
  cmDspInst_t** gmtr;
  
} cmDspPP_TestCircuit_t;


cmDspRC_t _cmDspPP_TestCircuitAlloc(
  cmDspSysH_t              h, 
  _cmDspPP_Ctx_t*          ctx,   
  cmDspPP_TestCircuit_t*   p,
  const cmChar_t*          title,
  const cmChar_t*          preLbl,
  cmDspInst_t*             selChk,
  cmDspInst_t**            ain,
  cmDspInst_t*             ptIn )
{
  cmDspRC_t rc;

  p->c.circuitSymId     = cmDspSysRegisterInstAttrSymbolStr(h,title);
  unsigned    presetGroupSymId = cmDspSysPresetRegisterGroup(h,title);
  _cmDspSys_PresetMgmt(h,preLbl,presetGroupSymId);
  cmDspSysNewColumn(h,COL_WIDTH);

  p->c.aout  = allocInstPtrArray(h, ctx->oChCnt);


  // make processors for each input channel
  cmDspInst_t** sfmt = cmDspSysAllocInstArray( h, ctx->iChCnt, "Sprintf", NULL,  NULL, 1, "gate-%i:");
  cmDspInst_t** rmtr = cmDspSysAllocInstArray( h, ctx->iChCnt, "Meter",   "RMS", NULL, 3, 0.0, 0.0, 1.0 );
  cmDspSysNewColumn(h,COL_WIDTH);
  cmDspInst_t** gmtr = cmDspSysAllocInstArray( h, ctx->iChCnt, "Meter",   "Gate",NULL, 3, 0.0, 0.0, 1.0 );


  // make processors for each output channel
  if( ctx->oChCnt )
    p->c.aout[0] = cmDspSysAllocInst( h, "AMix", NULL, 1, ctx->iChCnt );
  p->c.aoutLbl = "out";
  
  
  // check for allocation errors
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    goto errLabel;

  if( ctx->oChCnt )
    cmDspSysConnectAudioN11N(h, ain, "out", p->c.aout[0], "in", ctx->iChCnt );

  cmDspSysInstallCb1NN1(h, ptIn, "f-out", rmtr,    "in",   ctx->iChCnt );  // RMS input
  cmDspSysInstallCb1NN1(h, ptIn, "b-out", gmtr,    "in",   ctx->iChCnt );  // Gate input 
  cmDspSysInstallCb1NN1(h, ptIn, "b-out", sfmt,    "in-0", ctx->iChCnt );  // format string
  //cmDspSysInstallCbN111(h, p->sfmt, "out",   ctx->print, "in",   ctx->iChCnt);// print string
  
 errLabel:

  cmDspSysRemoveInstAttrSymbol(h,p->c.circuitSymId);

  return rc;
}

//=====================================================================================================================

typedef struct
{
  cmDspInst_t** tmop;
  cmDspInst_t** amop;
  cmDspInst_t** cf;
  cmDspInst_t** sg;
  cmDspInst_t** eq;
  cmDspInst_t** env;
  cmDspInst_t** rtr;
  cmDspInst_t** bts;
  cmDspInst_t*  mix; 
} cmDspSys_NShp_t;



cmDspSys_NShp_t*  _cmDspSys_NShpAlloc( cmDspSysH_t h, unsigned pgSymId, const cmChar_t* preLbl, unsigned chCnt, bool seqFl )
{
  cmDspRC_t       rc           = kOkDspRC;

  unsigned        secCnt       = 3;

  double          cntMin       = 1;
  double          cntMax       = secCnt;

  unsigned        sgShapeId    = 2;
  unsigned        sgOtCnt      = 5;

  double          cfMinHz      = 20.0;
  double          cfHz         = 500;
  double          cfAlpha      = 0.9;
  bool            cfFbFl       = true;
  bool            cfBypassFl   = false;

  double          adsrMaxMs    = 20000;
  double          adsrMinMs    = 0;
  double          adsrIncMs    = 1;
  double          adsrMaxLevel = 100.0;
  double          adsrSusLevel = 100.0;
  double          adsrMinLevel =   0.0;
  double          adsrIncLevel = 0.001;

  bool            eqBypassFl = false;
  unsigned        eqModeSymId = cmSymTblRegisterStaticSymbol(cmDspSysSymbolTable(h),"LP");
  double          eqF0hz      = 250;
  double          eqQ         = 1.0;
  double          eqFgain     = 1.0;
  
  bool            mtBypassFl = false;
  double          mtTimeScale= 1.0;
  double          mtFeedback = 0.5;

  double          multMin  = 0.0;
  double          multMax  = 100.0;
  double          multInc  = 0.01;
  double          offsMin  = 0.0;
  double          offsMax  = 100.0;
  double          offsInc  = 0.01;

  cmDspSys_NShp_t* p = cmLhAllocZ( cmDspSysLHeap(h),cmDspSys_NShp_t,1);

  cmDspInst_t** bts   = NULL;
  cmDspInst_t** rtr   = NULL;
  cmDspInst_t** cnt0  = NULL;
  cmDspInst_t** cnt1  = NULL;
  cmDspInst_t** cnt2  = NULL;
  cmDspInst_t** seq0  = NULL;
  cmDspInst_t** seq1  = NULL;
  cmDspInst_t** seq2  = NULL;


  cmDspSysNewColumn(h,COL_WIDTH);
  cmDspInst_t** sg   = p->sg  = cmDspSysAllocInstArray( h, chCnt, "SigGen",    NULL, NULL, 2, 1000.0, sgShapeId, 1.0, sgOtCnt  );
  cmDspInst_t** cf   = p->cf  = cmDspSysAllocInstArray( h, chCnt, "CombFilt",  NULL, NULL, 5, cfBypassFl, cfMinHz, cfFbFl, cfHz, cfAlpha );
  cmDspInst_t** eq   = p->eq  = cmDspSysAllocInstArray( h, chCnt, "BiQuadEq",  NULL, NULL, 5, eqBypassFl, eqModeSymId, eqF0hz, eqQ, eqFgain ); 
  cmDspInst_t** mt   =          cmDspSysAllocInstArray( h, chCnt, "MtDelay",   NULL, NULL, 9, mtBypassFl, mtTimeScale, mtFeedback,   20.0, 0.8,   15.0, 0.9,    12.0, 0.9 );  

  // adsr time/ampl scale/offset 
  cmDspInst_t** tmop = p->tmop= cmDspSysAllocInstArray(h,  chCnt, "ScalarOp",  NULL, NULL, 6, 2, "*", "in-0", 1.0, "in-1", 1.0 );
  cmDspInst_t** toop =          cmDspSysAllocInstArray(h,  chCnt, "ScalarOp",  NULL, NULL, 6, 2, "+", "in-0", 0.0, "in-1", 0.0 ); 
  cmDspInst_t** amop = p->amop= cmDspSysAllocInstArray(h,  chCnt, "ScalarOp",  NULL, NULL, 6, 2, "*", "in-0", 1.0, "in-1", 1.0 );
  cmDspInst_t** aoop =          cmDspSysAllocInstArray(h,  chCnt, "ScalarOp",  NULL, NULL, 6, 2, "+", "in-0", 0.0, "in-1", 0.0 ); 
  cmDspInst_t** env  = p->env = cmDspSysAllocInstArray( h, chCnt, "Adsr",      NULL, NULL, 2, true, adsrMinLevel );
  cmDspInst_t** d2l  =          cmDspSysAllocInstArray( h, chCnt, "DbToLin",   NULL, NULL, 0 );

  if( seqFl )
  {
    bts   = p->bts = cmDspSysAllocInstArray(   h, chCnt, "GateToSym",NULL, NULL, 0 );
    rtr   = p->rtr = cmDspSysAllocInstArray(   h, chCnt, "Router",   NULL, NULL, 2, secCnt, 0 );
    cnt0  =          cmDspSysAllocInstArray(   h, chCnt, "Counter",  NULL, NULL, 3, cntMin, cntMax, 1.0 );
    cnt1  =          cmDspSysAllocInstArray(   h, chCnt, "Counter",  NULL, NULL, 3, cntMin, cntMax, 1.0 );
    cnt2  =          cmDspSysAllocInstArray(   h, chCnt, "Counter",  NULL, NULL, 3, cntMin, cntMax, 1.0 );
    seq0  =          cmDspSysAllocInstArray(   h, chCnt, "MsgList",  NULL, NULL, 1, "ns_seq0");
    seq1  =          cmDspSysAllocInstArray(   h, chCnt, "MsgList",  NULL, NULL, 1, "ns_seq1");
    seq2  =          cmDspSysAllocInstArray(   h, chCnt, "MsgList",  NULL, NULL, 1, "ns_seq2");
  }

  // output mixer
  cmDspInst_t* mix    = p->mix = cmDspSysAllocInst( h, "AMix", NULL, 2, chCnt, 0.0 );

  // comb filter parameters
  cmDspSysNewColumn(h,COL_WIDTH);
  cmDspInst_t* alpha  = cmDspSysAllocScalarP(   h, pgSymId, preLbl, "alpha", -1.0, 1.0, 0.001, cfAlpha );

  // time/amp scale multiply and offset controls
  cmDspInst_t* tmul   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Time Mult",  multMin,   multMax,   multInc, 1.0);
  cmDspInst_t* toff   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Time Offs",  offsMin,   offsMax,   offsInc, 0.0);
  cmDspInst_t* amul   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Ampl Mult",  multMin,   multMax,   multInc, 1.0);
  cmDspInst_t* aoff   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Ampl Offs",  offsMin,   offsMax,   offsInc, 0.0);
 
  // ADSR parameters
  cmDspInst_t* dly   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Dly Ms",  adsrMinMs,   adsrMaxMs,   adsrIncMs, 0.0);
  cmDspInst_t* atk   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Atk Ms",  adsrMinMs,   adsrMaxMs,   adsrIncMs, 5000.0);
  cmDspInst_t* dcy   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Dcy Ms",  adsrMinMs,   adsrMaxMs,   adsrIncMs, 100.0);
  cmDspInst_t* hold  = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Hold Ms", adsrMinMs,   adsrMaxMs,   adsrIncMs, 100.0);
  cmDspInst_t* rls   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Rls Ms",  adsrMinMs,   adsrMaxMs,   adsrIncMs, 9000.0);
  cmDspInst_t* alvl  = cmDspSysAllocScalarP( h, pgSymId, preLbl, "AdsrMax", adsrMinLevel,adsrMaxLevel,adsrIncLevel, adsrMaxLevel);  
  cmDspInst_t* sus   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Sustain", adsrMinLevel,adsrMaxLevel,adsrIncLevel, adsrSusLevel );  

  cmDspSysNewColumn(h,COL_WIDTH);
  cmDspInst_t* eqbyp   = cmDspSysAllocCheckP(  h, pgSymId, preLbl, "Eq Byp", 0 );
  cmDspInst_t* eqmode  = cmDspSysAllocMsgListP(h, pgSymId, preLbl, "Mode", NULL, "biQuadEqMode", 1);
  cmDspInst_t* eqq     = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Q",          -100.0, 100.0, 0.1, eqQ);
  cmDspInst_t* eqfgn   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Filt Gain",   0.0,     1.0, 0.1, eqFgain);

  cmDspInst_t* mtfb   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Mt Feedback",   0.0,     1.0, 0.01, mtFeedback);
  cmDspInst_t* mtscale= cmDspSysAllocScalarP( h, pgSymId, preLbl, "Mt Time Scale", 0.01,   10.0, 0.01, mtTimeScale);

  cmDspInst_t* sgsh   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Sg Shape",   0.0,     8.0, 1.0, ((double)sgShapeId));
  cmDspInst_t* sgot   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Sg OT Cnt",  0.0,    10.0, 1.0, ((double)sgOtCnt));

  // check for allocation errors
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    goto errLabel;

  cmDspSysConnectAudioN1N1( h, sg,  "out",  cf,  "in",   chCnt );  // sg  -> CF
  cmDspSysConnectAudioN1N1( h, cf,  "out",  eq,  "in",   chCnt );  // CF -> EQ
  cmDspSysConnectAudioN1N1( h, eq,  "out",  mt,  "in",   chCnt );  // EQ -> MT
  cmDspSysConnectAudioN11N( h, mt,  "out",  mix, "in",   chCnt );  // MT  -> mix

  cmDspSysInstallCb11N1(    h, tmul, "val", tmop, "in-1",   chCnt );
  cmDspSysInstallCb11N1(    h, toff, "val", toop, "in-1",   chCnt );
  cmDspSysInstallCbN1N1(    h, tmop, "out", toop, "in-0",   chCnt );
  cmDspSysInstallCbN1N1(    h, toop, "out", env,  "tscale", chCnt );

  cmDspSysInstallCb11N1(    h, amul, "val", amop, "in-1",   chCnt );
  cmDspSysInstallCb11N1(    h, aoff, "val", aoop, "in-1",   chCnt );
  cmDspSysInstallCbN1N1(    h, amop, "out", aoop, "in-0",   chCnt );
  cmDspSysInstallCbN1N1(    h, aoop, "out", env,  "ascale", chCnt );

  cmDspSysInstallCbN1N1(   h, env,   "out",  d2l,  "in",   chCnt );   // adsr -> dbtolin
  cmDspSysInstallCbN11N(   h, d2l,   "out",  mix,  "gain", chCnt );   // db2lin -> mix (gain)

  cmDspSysInstallCbN1N1(   h, env,   "out",  d2l,  "in",    chCnt );
  cmDspSysInstallCbN11N(   h, d2l,   "out",  mix,  "gain",  chCnt ); 
  cmDspSysInstallCb11N1(   h, alpha, "val",  cf,   "alpha", chCnt );  // CF alpha

  cmDspSysInstallCb11N1( h, sgsh,  "val",  sg,   "shape",  chCnt );
  cmDspSysInstallCb11N1( h, sgot,  "val",  sg,   "ot",     chCnt );

  cmDspSysInstallCb11N1( h, dly,  "val", env, "dly",  chCnt );
  cmDspSysInstallCb11N1( h, atk,  "val", env, "atk",  chCnt );
  cmDspSysInstallCb11N1( h, dcy,  "val", env, "dcy",  chCnt );
  cmDspSysInstallCb11N1( h, hold, "val", env, "hold", chCnt );
  cmDspSysInstallCb11N1( h, rls,  "val", env, "rls",  chCnt );
  cmDspSysInstallCb11N1( h, alvl, "val", env, "alvl", chCnt );
  cmDspSysInstallCb11N1( h, sus,  "val", env, "sus",  chCnt );


  cmDspSysInstallCb11N1(   h, eqbyp,  "out",  eq,    "bypass", chCnt );
  cmDspSysInstallCb11N1(   h, eqmode, "mode", eq,    "mode",   chCnt );
  cmDspSysInstallCb11N1(   h, eqq,    "val",  eq,    "Q",      chCnt );
  cmDspSysInstallCb11N1(   h, eqfgn,  "val",  eq,    "gain",   chCnt );

  cmDspSysInstallCb11N1(   h, mtfb,   "val",  mt,    "fb",      chCnt );
  cmDspSysInstallCb11N1(   h, mtscale,"val",  mt,    "scale",   chCnt );


  if( seqFl )
  {
    cmDspSysInstallCbN1N1( h, bts, "out",     rtr,  "s-in",   chCnt );
    cmDspSysInstallCbN1N1( h, rtr, "s-out-0", cnt0, "next", chCnt );
    cmDspSysInstallCbN1N1( h, rtr, "s-out-1", cnt1, "next", chCnt );
    cmDspSysInstallCbN1N1( h, rtr, "s-out-2", cnt2, "next", chCnt );

    cmDspSysInstallCbN1N1(h, seq0, "cnt",  cnt0, "max",  chCnt );
    cmDspSysInstallCbN1N1(h, seq1, "cnt",  cnt1, "max",  chCnt );
    cmDspSysInstallCbN1N1(h, seq2, "cnt",  cnt2, "max",  chCnt );

    cmDspSysInstallCbN1N1(h, cnt0, "out",  seq0, "sel",  chCnt );
    cmDspSysInstallCbN1N1(h, cnt1, "out",  seq1, "sel",  chCnt );
    cmDspSysInstallCbN1N1(h, cnt2, "out",  seq2, "sel",  chCnt );

    cmDspSysInstallCbN1N1(h, seq0, "hz",  cf, "hz",  chCnt );
    cmDspSysInstallCbN1N1(h, seq1, "hz",  cf, "hz",  chCnt );
    cmDspSysInstallCbN1N1(h, seq2, "hz",  cf, "hz",  chCnt );

    cmDspSysInstallCbN1N1(h, seq0, "hz",  eq, "f0",  chCnt );
    cmDspSysInstallCbN1N1(h, seq1, "hz",  eq, "f0",  chCnt );
    cmDspSysInstallCbN1N1(h, seq2, "hz",  eq, "f0",  chCnt );

    cmDspSysInstallCbN1N1(h, seq0, "hz",  sg, "hz",  chCnt );
    cmDspSysInstallCbN1N1(h, seq1, "hz",  sg, "hz",  chCnt );
    cmDspSysInstallCbN1N1(h, seq2, "hz",  sg, "hz",  chCnt );
  }


 errLabel:
  if(cmDspSysLastRC(h) != kOkDspRC )
    p = NULL;

  return p;

}



typedef struct 
{
  cmDspPP_Circuit_t c;
  cmDspSys_NShp_t*  ns0p;
  cmDspSys_NShp_t*  ns1p;
} cmDspPP_NSh_Circuit_t;


cmDspRC_t _cmDspPP_NoiseShaperAlloc(
  cmDspSysH_t              h, 
  cmErr_t*                 err,
  cmDspPP_NSh_Circuit_t*   p,
  const cmChar_t*          title,
  const cmChar_t*          preLbl,
  cmDspInst_t*             selChk,
  cmDspInst_t*             ptIn,
  unsigned                 iChCnt,
  unsigned                 oChCnt)
{
  cmDspRC_t       rc             = kOkDspRC;
  double          secCnt         = 3;
  unsigned        nsCnt          = 0;
  unsigned        nsCnt1         = 0;
  const cmChar_t* nsPitchRsrcStr = "nsMidi";
  const cmChar_t* nsChIdxRsrcStr = "nsSelCh";

  // get the active channel list (indexes of the input channels which will be used by the noise shaper);
  if( cmDspRsrcArrayCount( h, &nsCnt1, nsChIdxRsrcStr, NULL ) != kOkDspRC )
    return cmErrMsg(err,kPgmCfgFailDspRC,"The noise shaper channel index list '%s' could not be read.",cmStringNullGuard(nsChIdxRsrcStr));

  // get the pitches associated with each active channel
  if( cmDspRsrcArrayCount( h, &nsCnt, nsPitchRsrcStr, NULL ) != kOkDspRC )
    return cmErrMsg(err,kPgmCfgFailDspRC,"The noise shaper pitch list '%s' could not be read.",cmStringNullGuard(nsPitchRsrcStr));

  // the channel and pitch list must contain the same number of elements
  assert( nsCnt1 == nsCnt );

  p->c.circuitSymId =  cmDspSysRegisterInstAttrSymbolStr(h,title);
  unsigned       presetGrpSymId = cmDspSysPresetRegisterGroup(h,title);
  _cmDspSys_PresetMgmt(h,preLbl,presetGrpSymId);
  cmDspSysNewColumn(h,COL_WIDTH);
  
  //cmDspInst_t* printBtn  = cmDspSysAllocInst( h, "Button", "_print",  2, kButtonDuiId, 0.0);
  //cmDspInst_t* bcast     = cmDspSysAllocInst( h, "BcastSym",   NULL,  1, p->c.circuitSymId );
  //cmDspSysInstallCb( h, printBtn, "sym", bcast, "msg", NULL );

  p->c.aout  = allocInstPtrArray(h,oChCnt);

  // nom - selects active channels from among all possible input channels - coming from the EF's
  cmDspInst_t* nom = cmDspSysAllocInst( h, "NofM", NULL, 2, iChCnt, nsCnt );  // EF's -> Nsh switch
 
  // Allocate the noise shapers
  p->ns0p = _cmDspSys_NShpAlloc( h, presetGrpSymId, "f", nsCnt, false ); 
  p->ns1p = _cmDspSys_NShpAlloc( h, presetGrpSymId, "s", nsCnt, true );

  cmDspSysNewColumn(h,COL_WIDTH);
  cmDspInst_t*  sectn  = cmDspSysAllocScalar(    h, "Section",  0, secCnt-1, 1.0, 0.0);
  cmDspInst_t*  pts    = cmDspSysAllocInst(      h, "PortToSym", NULL, 2, "off", "send" );
  cmDspInst_t*  pitarr = cmDspSysAllocInst(      h, "Array", NULL, 1, nsPitchRsrcStr );
  cmDspInst_t*  charr  = cmDspSysAllocInst(      h, "Array", NULL, 1, nsChIdxRsrcStr );
  cmDspInst_t** eqpc   = cmDspSysAllocInstArray( h, nsCnt, "PitchCvt", NULL, NULL, 0 );
  cmDspInst_t** cfpc   = cmDspSysAllocInstArray( h, nsCnt, "PitchCvt", NULL, NULL, 0 );
  cmDspInst_t** sgpc   = cmDspSysAllocInstArray( h, nsCnt, "PitchCvt", NULL, NULL, 0 );

  // check for allocation errors
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    goto errLabel;

  assert( oChCnt >= 2 );

  // put the 'f' noise in ch:0 and the 's' noise in ch:1
  p->c.aoutLbl = "out";
  p->c.aout[1] = p->ns0p->mix;
  p->c.aout[0] = p->ns1p->mix;


  // selChk is the circuit selector check box in the main window
  cmDspSysInstallCb(       h, selChk, "out",      pts,    "off",  NULL );  // First send 'off' to nom to clear its selector
  cmDspSysInstallCb(       h, selChk, "out",      pts,    "send", NULL );  // Second send 'send' to charr to select active ports on nom.
  cmDspSysInstallCb(       h, pts,    "off",      nom,    "cmd",  NULL );  // Clear the nom (all inputs are turned off)
  cmDspSysInstallCb(       h, pts,    "send",     charr,  "cmd",  NULL );  // Send the active channel indexes to the nom
  cmDspSysInstallCb(       h, pts,    "send",     pitarr, "cmd",  NULL );  // Send the pitch values to the circuits (eq,cf,sg,..)
  cmDspSysInstallCb(       h, charr,  "done",     nom,    "cmd",  NULL );  // Tell the nom when its setup is complete

  cmDspSysInstallCb1N11(   h, charr,  "out",      nom,    "seli", nsCnt );   // EF nom channel index selectors 
  cmDspSysInstallCb1N1N(   h, ptIn,   "b-out",    nom,    "b-in", iChCnt );  // EF -> nom (gates)
  cmDspSysInstallCb1N1N(   h, ptIn,   "f-out",    nom,    "f-in", iChCnt );  // EF -> nom (rms)


  cmDspSysInstallCb1NN1(   h, pitarr, "out",      cfpc,   "midi", nsCnt );   // pitch array -> pitch converter
  cmDspSysInstallCb1NN1(   h, pitarr, "out",      eqpc,   "midi", nsCnt );   
  cmDspSysInstallCb1NN1(   h, pitarr, "out",      sgpc,   "midi", nsCnt );

  cmDspSysInstallCbN1N1(   h, cfpc,   "hz",    p->ns0p->cf,   "hz",   nsCnt );   // pitch cvt -> CF (Hz)
  cmDspSysInstallCbN1N1(   h, eqpc,   "hz",    p->ns0p->eq,   "f0",   nsCnt );   // pitch cvt -> Eq (Hz)
  cmDspSysInstallCbN1N1(   h, sgpc,   "hz",    p->ns0p->sg,   "hz",   nsCnt );   // pitch cvt -> SG (Hz)

  cmDspSysInstallCb1NN1(   h, nom,   "b-out",  p->ns0p->env,  "gate", nsCnt );   // EF  -> adsr  (gate)
  cmDspSysInstallCb1NN1(   h, nom,   "b-out",  p->ns1p->env,  "gate", nsCnt );   // 


  cmDspSysInstallCb1NN1(   h, nom,   "f-out",  p->ns0p->tmop,  "in-0",  nsCnt );   // EF (level) -> adsr time scale
  cmDspSysInstallCb1NN1(   h, nom,   "f-out",  p->ns0p->amop,  "in-0",  nsCnt );   // EF (level) -> adsr ampl scale
  cmDspSysInstallCb1NN1(   h, nom,   "f-out",  p->ns1p->tmop,  "in-0",  nsCnt );   // EF (level) -> adsr time scale
  cmDspSysInstallCb1NN1(   h, nom,   "f-out",  p->ns1p->amop,  "in-0",  nsCnt );   // EF (level) -> adsr ampl scale

  cmDspSysInstallCb11N1(   h, sectn, "val",    p->ns1p->rtr,  "sel",  nsCnt );   // section scalar -> rtr sel input
  cmDspSysInstallCb1NN1(   h, nom,   "b-out",  p->ns1p->bts,  "on",   nsCnt );   // nom gate -> bool-to-sym -> rtr input 

 errLabel:

  cmDspSysRemoveInstAttrSymbol(h,p->c.circuitSymId);
  return rc;
}



//=====================================================================================================================
typedef struct
{
  cmDspInst_t*  start;   // circ. sel check -> start  
  cmDspInst_t** tmop;    // level -> time scale input
  cmDspInst_t** amop;    // level -> ampl scale input
  cmDspInst_t** adsr;    // gate  -> adsr input
  cmDspInst_t** mix;     // audio -> input mixer
  cmDspInst_t** ps;      // pitch shifter
  cmDspInst_t** eq;
  cmDspInst_t** mt;      // output prior to the mixer
  cmDspInst_t*  mixo;    // output mixer -> aout
} cmDspFx0_Chain_t;


cmDspRC_t _cmDspFx0_ChainAlloc( 
  cmDspSysH_t       h, 
  cmErr_t*          err,
  unsigned          pgSymId, 
  const cmChar_t*   preLbl, 
  const cmChar_t*   rsrc,
  unsigned          chainChCnt, 
  cmDspFx0_Chain_t* p )
{
  cmDspRC_t       rc           = kOkDspRC;

  double          multMin  = 0.0;
  double          multMax  = 100.0;
  double          multInc  = 0.01;
  double          offsMin  = 0.0;
  double          offsMax  = 100.0;
  double          offsInc  = 0.01;

  double          adsrMaxMs    = 20000;
  double          adsrMinMs    = 0;
  double          adsrIncMs    = 1;
  double          adsrMaxLevel = 100.0;
  double          adsrSusLevel = 100.0;
  double          adsrMinLevel =   0.0;
  double          adsrIncLevel = 0.001;

  unsigned        sgShapeId    = 2;
  unsigned        sgOtCnt      = 5;

  bool            lpBypassFl   = true;

  bool            cmpBypassFl  = false;
  double          cmpInGain     = 1.0;
  double          cmpThreshDb  = -40.0;
  double          cmpRatio_num = 4.0;
  double          cmpAtkMs     = 15.0;
  double          cmpRlsMs     = 50.0;
  double          cmpMakeup    = 4.0;
  double          cmpWndMaxMs  = 1000.0;
  double          cmpWndMs     = 200.0;
  double          cmpMaxGain   = 100.0;

  bool            distBypassFl   = false;
  double          distInGain     = 4.0;
  double          distDsrate     = 44100.0;
  double          distBits       = 24.0;
  bool            distRectifyFl  = false;
  bool            distFullRectFl = false;
  double          distClipDb     = -10.0;

  double          cfMinHz      = 20.0;
  double          cfHz         = 500;
  double          cfAlpha      = 0.9;
  bool            cfFbFl       = true;
  bool            cfBypassFl   = true;

  bool            eqBypassFl   = false;
  unsigned        eqModeSymId  = cmSymTblRegisterStaticSymbol(cmDspSysSymbolTable(h),"LP");
  double          eqF0hz       = 250;
  double          eqQ          = 1.0;
  double          eqFgain      = 1.0;

  bool            psBypassFl   = true;
  double          psMaxRatio   = 10.0;
  double          psRatio      = 1.0;

  double          dlyFb        = 0.0;
  double          dlyMaxMs     = 3000.0;
  double          dlyMs        = 1;

  bool            mtBypassFl   = true;
  double          mtTimeScale  = 1.0;
  double          mtFeedback   = 0.0;

  double          stOffs = 96;

  double          voiceMult = 1.0 / chainChCnt;

  cmDspInst_t** adsr = p->adsr= cmDspSysAllocInstArray(h, chainChCnt, "Adsr",      NULL, NULL, 2, true, adsrMinLevel );
  cmDspInst_t** dtl  =          cmDspSysAllocInstArray(h, chainChCnt, "DbToLin",   NULL, NULL, 0 );
  cmDspInst_t** phs  =          cmDspSysAllocInstArray(h, chainChCnt, "Phasor",    NULL, NULL, 1, cmDspSysSampleRate(h) );
  cmDspInst_t** wt   =          cmDspSysAllocInstArray(h, chainChCnt, "WaveTable", NULL, NULL, 4, ((unsigned)cmDspSysSampleRate(h)), 2, NULL, NULL, -1 ); 

  //cmDspInst_t** sgmul=          cmDspSysAllocScalarA(  h, chainChCnt, pgSymId, preLbl, "Sg Mult",    0.0,    10.0, 0.01, 1.0 );
  //cmDspInst_t** mul  =          cmDspSysAllocInstArray(h, chainChCnt, "ScalarOp",  NULL, NULL, 6, 2, "*", "in-0", 1.0, "in-1", 1.0 );
  cmDspInst_t** mixa = p->mix = cmDspSysAllocInstArray(h, chainChCnt, "AMix",      NULL, NULL, 1, 1);
  cmDspInst_t** mixs =          cmDspSysAllocInstArray(h, chainChCnt, "AMix",      NULL, NULL, 2, 1, 0.0);
  cmDspInst_t** mixm =          cmDspSysAllocInstArray(h, chainChCnt, "AMix",      NULL, NULL, 3, 2, 0.5, 0.5);

  cmDspInst_t** loop =          cmDspSysAllocInstArray(h, chainChCnt, "LoopRecd",  NULL, NULL, 0 );
  cmDspInst_t** ps   = p->ps =  cmDspSysAllocInstArray(h, chainChCnt, "PShift",    NULL, NULL, 2, psBypassFl,   psRatio );

  cmDspInst_t** cmp  =          cmDspSysAllocInstArray(h, chainChCnt, "Compressor",NULL, NULL, 8, cmpBypassFl, cmpThreshDb, cmpRatio_num, cmpAtkMs, cmpRlsMs, cmpInGain, cmpMakeup, cmpWndMs, cmpWndMaxMs ); 
  cmDspInst_t** dist =          cmDspSysAllocInstArray(h, chainChCnt, "DistDs",    NULL, NULL, 3, distBypassFl, distInGain, distDsrate, distBits  ); 
  cmDspInst_t** cf   =          cmDspSysAllocInstArray(h, chainChCnt, "CombFilt",  NULL, NULL, 5, cfBypassFl,   cfMinHz, cfFbFl, cfHz, cfAlpha );
  cmDspInst_t** eq   = p->eq =  cmDspSysAllocInstArray(h, chainChCnt, "BiQuadEq",  NULL, NULL, 5, eqBypassFl,   eqModeSymId, eqF0hz, eqQ, eqFgain );    
  cmDspInst_t** dly  =          cmDspSysAllocInstArray(h, chainChCnt, "Delay",     NULL, NULL, 2, dlyMaxMs,     dlyFb );
  cmDspInst_t** mt   = p->mt  = cmDspSysAllocInstArray(h, chainChCnt, "MtDelay",   NULL, NULL, 5, mtBypassFl,   mtTimeScale, mtFeedback,   20.0, 0.3 );      
  cmDspInst_t*  mixo = p->mixo= cmDspSysAllocInst(     h,             "AMix",      NULL,       1, chainChCnt );

  cmDspSysNewColumn(h,COL_WIDTH);
  cmDspInst_t** dlyms =         cmDspSysAllocScalarA(  h, chainChCnt, pgSymId, preLbl, "Dly ms",    0.0,    10000.0, 1.0, dlyMs);

  cmDspInst_t*  start = p->start = cmDspSysAllocInst(     h,             "PortToSym", NULL, 1, "send" );  // all msgs to start send the 'hz' symbol
  cmDspInst_t** sgpc  =            cmDspSysAllocInstArray(h, chainChCnt, "PitchCvt",  NULL, NULL, 0 );    
  cmDspInst_t** cfpc  =            cmDspSysAllocInstArray(h, chainChCnt, "PitchCvt",  NULL, NULL, 0 );    
  cmDspInst_t** eqpc  =            cmDspSysAllocInstArray(h, chainChCnt, "PitchCvt",  NULL, NULL, 0 );    
  cmDspInst_t*  midiV =            cmDspSysAllocInst(     h,             "Array",     NULL, 1, rsrc );

  cmDspSysNewColumn(h,COL_WIDTH);
  
  cmDspInst_t** tmop = p->tmop= cmDspSysAllocInstArray(h,  chainChCnt, "ScalarOp",  NULL, NULL, 6, 2, "*", "in-0", 1.0, "in-1", 1.0 );
  cmDspInst_t** toop =          cmDspSysAllocInstArray(h,  chainChCnt, "ScalarOp",  NULL, NULL, 6, 2, "+", "in-0", 0.0, "in-1", 0.0 ); 
  cmDspInst_t** amop = p->amop= cmDspSysAllocInstArray(h,  chainChCnt, "ScalarOp",  NULL, NULL, 6, 2, "*", "in-0", 1.0, "in-1", 1.0 );
  cmDspInst_t** aoop =          cmDspSysAllocInstArray(h,  chainChCnt, "ScalarOp",  NULL, NULL, 6, 2, "+", "in-0", 0.0, "in-1", 0.0 ); 

  // time/amp scale multiply and offset controls
  cmDspInst_t* tmul   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Time Mult",  multMin,   multMax,   multInc, 1.0);
  cmDspInst_t* toff   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Time Offs",  offsMin,   offsMax,   offsInc, 0.0);
  cmDspInst_t* amul   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Ampl Mult",  multMin,   multMax,   multInc, 1.0);
  cmDspInst_t* aoff   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Ampl Offs",  offsMin,   offsMax,   offsInc, 0.0);

  // ADSR controls
  cmDspInst_t* adly  = cmDspSysAllocScalarP( h, pgSymId, preLbl, "sg-Dly",  adsrMinMs,   adsrMaxMs,   adsrIncMs, 0.0);
  cmDspInst_t* atk   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "sg-Atk",  adsrMinMs,   adsrMaxMs,   adsrIncMs, 20.0);
  cmDspInst_t* dcy   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "sg-Dcy",  adsrMinMs,   adsrMaxMs,   adsrIncMs, 100.0);
  cmDspInst_t* hold  = cmDspSysAllocScalarP( h, pgSymId, preLbl, "sg-Hold", adsrMinMs,   adsrMaxMs,   adsrIncMs, 100.0);
  cmDspInst_t* rls   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "sg-Rls",  adsrMinMs,   adsrMaxMs,   adsrIncMs, 1000.0);
  cmDspInst_t* alvl  = cmDspSysAllocScalarP( h, pgSymId, preLbl, "sg-AdsrMax", adsrMinLevel,adsrMaxLevel,adsrIncLevel, adsrMaxLevel);  
  cmDspInst_t* sus   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "sg-SusLvl", adsrMinLevel,adsrMaxLevel,adsrIncLevel, adsrSusLevel );  
  //cmDspInst_t* zero  = cmDspSysAllocScalarP( h, pgSymId, preLbl, "zero",  0.0,  0.0, 0.0, 0.0);

  // SigGen controls
  cmDspInst_t* sgsh   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Sg Shape",   0.0,    11.0, 1.0, ((double)sgShapeId));
  cmDspInst_t* sgot   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Sg OT Cnt",  0.0,    20.0, 1.0, ((double)sgOtCnt));
  cmDspInst_t* sgpoffs = cmDspSysAllocScalarP(h, pgSymId, preLbl, "Sg ST off", -stOffs, stOffs, 0.01, 0.0 );


  cmDspSysNewColumn(h,COL_WIDTH);
  // Loop recorder controls
  cmDspInst_t* lpByp  = cmDspSysAllocCheckP(  h, pgSymId, preLbl,  "Lp Byps", lpBypassFl );
  cmDspInst_t* lpRcd  = cmDspSysAllocCheckP(  h, pgSymId, preLbl,  "Lp Recd", false );
  cmDspInst_t* lpRat  = cmDspSysAllocScalarP( h, pgSymId, preLbl,  "Lp Ratio",0.0,  10.0, 0.01, 1.0 );

  // Pitch Shifter controls
  cmDspInst_t* psbyp  = cmDspSysAllocCheckP(   h, pgSymId, preLbl, "PS Byp",   psBypassFl );
  cmDspInst_t* psrat  = cmDspSysAllocScalarP(  h, pgSymId, preLbl, "PS Ratio", 0.0, psMaxRatio, 0.01, psRatio  );


  cmDspSysNewColumn(h,COL_WIDTH);
  // Compressor controls
  cmDspInst_t* cbyp   = cmDspSysAllocCheckP(  h, pgSymId, preLbl, "Byp Cmp",   cmpBypassFl);
  cmDspInst_t* cgain  = cmDspSysAllocScalarP( h, pgSymId, preLbl, "In Gain",  0.0, cmpMaxGain, 0.01,cmpInGain );
  cmDspInst_t* cthr   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "ThreshDb", -100.0, 0.0,     0.1, cmpThreshDb);
  cmDspInst_t* crat   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Ratio",    0.1,  100.0,     0.1, cmpRatio_num);
  cmDspInst_t* catk   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Atk Ms",   0.0, 1000.0,     0.1, cmpAtkMs);
  cmDspInst_t* crls   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Rls Ms",   0.0, 1000.0,     0.1, cmpRlsMs);
  cmDspInst_t* cmkup  = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Makeup",   0.0, cmpMaxGain, 0.01, cmpMakeup);
  cmDspInst_t* cwnd   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Wnd Ms",   1.0, cmpWndMaxMs,1.0, cmpWndMs );

  // Distortion controls
  cmDspInst_t* dbyp   = cmDspSysAllocCheckP(  h, pgSymId, preLbl, "Dist Byp",  distBypassFl);
  cmDspInst_t* drct   = cmDspSysAllocCheckP(  h, pgSymId, preLbl, "Rectify",   distRectifyFl);
  cmDspInst_t* dful   = cmDspSysAllocCheckP(  h, pgSymId, preLbl, "Full/Half", distFullRectFl);
  cmDspInst_t* dsr    = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Dsrate",       0.0, 96000.0, 1.0, distDsrate);
  cmDspInst_t* dbt    = cmDspSysAllocScalarP( h, pgSymId, preLbl, "bits",         2.0,    32.0, 1.0, distBits);
  cmDspInst_t* dclip  = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Clip dB",   -100.0,     0.0, 0.1, distClipDb);
  cmDspInst_t* dogn   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Out Gain",     0.0,    10.0, 0.01, 1.0);

  cmDspSysNewColumn(h,COL_WIDTH);
  // Comb filter controls
  cmDspInst_t* cfbyp   = cmDspSysAllocCheckP(  h, pgSymId, preLbl, "CF Byp",  cfBypassFl);
  cmDspInst_t* cfalpha = cmDspSysAllocScalarP( h, pgSymId, preLbl, "CF alpha", -1.0, 1.0, 0.001, cfAlpha );
  cmDspInst_t* cfpoffs = cmDspSysAllocScalarP( h, pgSymId, preLbl, "CF ST off", -stOffs, stOffs, 0.01, 0.0 );

  // Eq controls
  cmDspInst_t* eqbyp   = cmDspSysAllocCheckP(  h, pgSymId, preLbl, "Eq Byp", 0 );
  cmDspInst_t* eqmode  = cmDspSysAllocMsgListP(h, pgSymId, preLbl, "Mode", NULL, "biQuadEqMode", 1);
  cmDspInst_t* eqq     = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Eq Q",     -100.0, 100.0, 0.1, eqQ);
  cmDspInst_t* eqfgn   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Filt Gain",   0.0,  10.0, 0.1, eqFgain);
  cmDspInst_t* eqpoffs = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Eq ST off", -stOffs, stOffs, 0.01, 0.0 );

  // Delay Controls
  cmDspInst_t* dlybyp = cmDspSysAllocCheckP(   h, pgSymId, preLbl, "Dly Byp", 0 );
  //cmDspInst_t* dlyms  = cmDspSysAllocScalarP(  h, pgSymId, preLbl, "Dly Time", 0.0, dlyMaxMs, 1.0, dlyMs  );
  cmDspInst_t* dlyfb  = cmDspSysAllocScalarP(  h, pgSymId, preLbl, "Dly Fb",   -1.0, 1.0, 0.001, dlyFb );

  // multi-tap controls
  cmDspInst_t* mtbyp  = cmDspSysAllocCheckP(  h, pgSymId, preLbl, "Mt Byp", mtBypassFl);
  cmDspInst_t* mtfb   = cmDspSysAllocScalarP( h, pgSymId, preLbl, "Feedback",   0.0,     1.0, 0.01, mtFeedback);
  cmDspInst_t* mtscale= cmDspSysAllocScalarP( h, pgSymId, preLbl, "Tm Scale",   0.01,     10.0, 0.01, mtTimeScale);

  // voice scaler
  cmDspInst_t* vmult = cmDspSysAllocScalarP(  h, pgSymId, preLbl, "Voc Scale",   0.0,    10.0, 0.001, voiceMult);
  
  // Compression Meters
  cmDspSysNewColumn(h,COL_WIDTH);
  cmDspSysAllocLabel(h,"Compression",kLeftAlignDuiId);

  cmDspInst_t** cmtr =  cmDspSysAllocInstArray(h, chainChCnt, "Meter", NULL, NULL, 3, 0.0, 0.0, 1.0);


  // Check for allocation errors
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    goto errLabel;

  // NOTE:
  // NOTE:  ain goes to mix "in-0"
  // NOTE:

  // audio connections - 
  cmDspSysConnectAudioN1N1(h, phs,  "out", wt,   "phs",  chainChCnt ); //  phs -> wt
  cmDspSysConnectAudioN1N1(h, wt,   "out", mixs, "in",   chainChCnt ); //  wt -> mix
  cmDspSysConnectAudioN1N1(h, mixs, "out", mixm, "in-0", chainChCnt ); //  
  cmDspSysConnectAudioN1N1(h, mixa, "out", mixm, "in-1", chainChCnt ); //  
  cmDspSysConnectAudioN1N1(h, mixm, "out", loop, "in",   chainChCnt ); //  mix -> loop
  cmDspSysConnectAudioN1N1(h, loop, "out", ps,   "in",   chainChCnt ); //  loop -> ps
  cmDspSysConnectAudioN1N1(h, ps,   "out", cmp,  "in",   chainChCnt ); //  ps   -> cmp  
  cmDspSysConnectAudioN1N1(h, cmp,  "out", dist, "in",   chainChCnt ); //  cmp  -> dist
  cmDspSysConnectAudioN1N1(h, dist, "out", cf,   "in",   chainChCnt ); //  dist -> cf  
  cmDspSysConnectAudioN1N1(h, cf,   "out", eq,   "in",   chainChCnt ); //  cf   -> eq  
  cmDspSysConnectAudioN1N1(h, eq,   "out", dly,  "in",   chainChCnt ); //  cf   -> dly
  cmDspSysConnectAudioN1N1(h, dly,  "out", mt,   "in",  chainChCnt );  //  dly -> mt
  cmDspSysConnectAudioN11N(h, mt,   "out", mixo, "in",  chainChCnt );  //  mt -> mixo

  cmDspSysInstallCb(       h, start,      "send", midiV,  "cmd", NULL );        // force the hz value to be sent out the midV
  cmDspSysInstallCb1NN1(   h, midiV,      "out",  sgpc,   "midi", chainChCnt ); // midiV -> pcvt (convert midi to pitch)
  cmDspSysInstallCb1NN1(   h, midiV,      "out",  cfpc,   "midi", chainChCnt ); // midiV -> pcvt (convert midi to pitch)
  cmDspSysInstallCb1NN1(   h, midiV,      "out",  eqpc,   "midi", chainChCnt ); // midiV -> pcvt (convert midi to pitch)

  //cmDspSysInstallCb11N1( h, zero, "val",   mix,  "gain-0", chainChCnt ); 
  //cmDspSysInstallCb11N1( h, zero, "val",   mixs,  "gain", chainChCnt );   // force the osc mixer to start turned off

  // Adssr scale/offset controls
  cmDspSysInstallCb11N1(    h, tmul, "val", tmop, "in-1",   chainChCnt );
  cmDspSysInstallCb11N1(    h, toff, "val", toop, "in-1",   chainChCnt );
  cmDspSysInstallCbN1N1(    h, tmop, "out", toop, "in-0",   chainChCnt );
  cmDspSysInstallCbN1N1(    h, toop, "out", adsr,  "tscale", chainChCnt );

  cmDspSysInstallCb11N1(    h, amul, "val", amop, "in-1",   chainChCnt );
  cmDspSysInstallCb11N1(    h, aoff, "val", aoop, "in-1",   chainChCnt );
  cmDspSysInstallCbN1N1(    h, amop, "out", aoop, "in-0",   chainChCnt );
  cmDspSysInstallCbN1N1(    h, aoop, "out", adsr,  "ascale", chainChCnt );

  // ADSR parameter controls
  cmDspSysInstallCb11N1( h, adly, "val",   adsr, "dly",  chainChCnt );
  cmDspSysInstallCb11N1( h, atk,  "val",   adsr, "atk",  chainChCnt );
  cmDspSysInstallCb11N1( h, dcy,  "val",   adsr, "dcy",  chainChCnt );
  cmDspSysInstallCb11N1( h, hold, "val",   adsr, "hold", chainChCnt );
  cmDspSysInstallCb11N1( h, rls,  "val",   adsr, "rls",  chainChCnt );
  cmDspSysInstallCb11N1( h, alvl, "val",   adsr, "alvl", chainChCnt );
  cmDspSysInstallCb11N1( h, sus,  "val",   adsr, "sus",  chainChCnt );  
  cmDspSysInstallCbN1N1( h, adsr, "out",   dtl,  "in",   chainChCnt );   // adsr    -> dbToLin
  cmDspSysInstallCbN1N1( h, dtl,  "out",   mixs, "gain", chainChCnt );   // dbToLin -> mix (gain)

  // SigGen parameter controls
  //cmDspSysInstallCb11N1(   h, start,      "send", sgmul,  "send",   chainChCnt ); // send cur sgmul value to mul in-0
  //cmDspSysInstallCbN1N1(   h, sgpc,       "hz",   mul,    "in-1",   chainChCnt ); // send hz        value to mul in-1 
  //cmDspSysInstallCbN1N1(   h, sgmul,      "val",  mul,    "in-0",   chainChCnt );  
  cmDspSysInstallCb11N1(   h, sgpoffs,    "val",  sgpc,   "offs",   chainChCnt );
  //cmDspSysInstallCbN1N1(   h, mul,        "out",  phs,     "mult",  chainChCnt );
  cmDspSysInstallCbN1N1(   h, sgpc,       "hz",   phs,    "mult",   chainChCnt );
  cmDspSysInstallCb11N1(   h, sgsh,       "val",  wt,     "shape",  chainChCnt );
  cmDspSysInstallCb11N1(   h, sgot,       "val",  wt,     "ot",     chainChCnt );


  // Pitch Shift parameter controls
  cmDspSysInstallCb11N1(   h, psbyp,  "out",  ps, "bypass", chainChCnt );
  cmDspSysInstallCb11N1(   h, psrat,  "val",  ps, "ratio",   chainChCnt );

  // Loop recorder parameter controls
  cmDspSysInstallCb11N1(h, lpByp, "out", loop, "bypass", chainChCnt );
  cmDspSysInstallCb11N1(h, lpRcd, "out", loop, "recd",   chainChCnt );
  cmDspSysInstallCb11N1(h, lpRat, "val", loop, "ratio",  chainChCnt );

  // Compressor paramter controls
  cmDspSysInstallCb11N1(h, cbyp,  "out", cmp, "bypass", chainChCnt );
  cmDspSysInstallCb11N1(h, cgain, "val", cmp, "igain",  chainChCnt );
  cmDspSysInstallCb11N1(h, cthr,  "val", cmp, "thr",    chainChCnt );
  cmDspSysInstallCb11N1(h, crat,  "val", cmp, "ratio",  chainChCnt );
  cmDspSysInstallCb11N1(h, catk,  "val", cmp, "atk",    chainChCnt );
  cmDspSysInstallCb11N1(h, crls,  "val", cmp, "rls",    chainChCnt );
  cmDspSysInstallCb11N1(h, cmkup, "val", cmp, "ogain",  chainChCnt );
  cmDspSysInstallCb11N1(h, cwnd,  "val", cmp, "wnd",    chainChCnt );
  cmDspSysInstallCbN1N1(h, cmp,   "env", cmtr, "in",    chainChCnt ); // compressor meter

  // Distortion parameter controls
  cmDspSysInstallCb11N1(h, dbyp,  "out", dist, "bypass",chainChCnt );
  cmDspSysInstallCb11N1(h, dsr,   "val", dist, "srate", chainChCnt );
  cmDspSysInstallCb11N1(h, dbt,   "val", dist, "bits",  chainChCnt );
  cmDspSysInstallCb11N1(h, drct,  "out", dist, "rect",  chainChCnt );
  cmDspSysInstallCb11N1(h, dful,  "out", dist, "full",  chainChCnt );
  cmDspSysInstallCb11N1(h, dclip, "val", dist, "clip",  chainChCnt );
  cmDspSysInstallCb11N1(h, dogn,  "val", dist, "ogain", chainChCnt );

  // Comb filter parameter controls
  cmDspSysInstallCb11N1(h, cfbyp,      "out", cf,   "bypass",chainChCnt );
  cmDspSysInstallCb11N1(h, cfalpha,    "val", cf,   "alpha", chainChCnt );
  cmDspSysInstallCb11N1(h, cfpoffs,    "val", cfpc, "offs",  chainChCnt );
  cmDspSysInstallCbN1N1(h, cfpc,       "hz",  cf,   "hz",    chainChCnt ); 
  //cmDspSysInstallCbN1N1(h, mul,        "out", cf,   "hz",    chainChCnt );

  // EQ parameter controls
  cmDspSysInstallCb11N1(   h, eqbyp,      "out",  eq,    "bypass", chainChCnt );
  cmDspSysInstallCb11N1(   h, eqmode,     "mode", eq,    "mode",   chainChCnt );
  cmDspSysInstallCb11N1(   h, eqq,        "val",  eq,    "Q",      chainChCnt );
  cmDspSysInstallCb11N1(   h, eqfgn,      "val",  eq,    "gain",   chainChCnt );
  cmDspSysInstallCb11N1(   h, eqpoffs,    "val",  eqpc,  "offs",   chainChCnt );
  cmDspSysInstallCbN1N1(   h, eqpc,       "hz",   eq,    "f0",     chainChCnt );   // chCfg -> Eq Hz
  //cmDspSysInstallCbN1N1(   h, mul,        "out",  eq,     "f0",    chainChCnt );

  // Delay parameter controls
  cmDspSysInstallCb11N1(   h, dlybyp, "out",  dly, "bypass", chainChCnt );
  //cmDspSysInstallCb11N1(   h, dlyms,  "val",  dly, "time",   chainChCnt );
  cmDspSysInstallCb11N1(   h, dlyfb,  "val",  dly, "fb",     chainChCnt );
  cmDspSysInstallCbN1N1(   h, dlyms,  "val",  dly, "time",   chainChCnt );  

  // Multi-tap parameter controls
  cmDspSysInstallCb11N1(   h, mtbyp,   "out",  mt,    "bypass",  chainChCnt );
  cmDspSysInstallCb11N1(   h, mtfb,    "val",  mt,    "fb",      chainChCnt );
  cmDspSysInstallCb11N1(   h, mtscale, "val",  mt,    "scale",   chainChCnt );

  cmDspSysInstallCb111N(   h, vmult,   "val",  mixo,  "gain",    chainChCnt );

 errLabel:

  return rc;
}

//=====================================================================================================================
typedef struct
{
  cmDspPP_Circuit_t c;
  cmDspFx0_Chain_t  fx;  
} cmDspPP_Fx0_t;


cmDspRC_t _cmDspPP_Fx0Alloc(
  cmDspSysH_t              h, 
  _cmDspPP_Ctx_t*          ctx,   
  cmDspPP_Fx0_t*           p,
  const cmChar_t*          title,
  const cmChar_t*          preLbl,
  cmDspInst_t*             selChk,
  cmDspInst_t**            ain,
  cmDspInst_t*             ptIn )
{
  cmDspRC_t rc;
  unsigned i;

  p->c.circuitSymId     = cmDspSysRegisterInstAttrSymbolStr(h,title);
  unsigned presetGroupSymId = cmDspSysPresetRegisterGroup(h,title);
  _cmDspSys_PresetMgmt(h,preLbl,presetGroupSymId);
  cmDspSysNewColumn(h,COL_WIDTH);

  unsigned fxChCnt = ctx->iChCnt;

  // make processors for each input channel
  if((rc = _cmDspFx0_ChainAlloc(h,ctx->err, presetGroupSymId, preLbl, "midiList", fxChCnt, &p->fx)) != kOkDspRC )
    goto errLabel;

  
  // make mixers for each output channel
  p->c.aout    = cmDspSysAllocInstArray( h, ctx->oChCnt, "AMix", NULL,NULL, 1, fxChCnt );  
  p->c.aoutLbl = "out";


  // check for allocation errors
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    goto errLabel;

  cmDspSysConnectAudioN1N1(h, ain,  "out",    p->fx.mix,   "in",    ctx->iChCnt );   // ain -> sg/ain mixer
  cmDspSysInstallCb1NN1(   h, ptIn, "b-out",  p->fx.adsr,  "gate",  ctx->iChCnt );   // gate -> adsr
  cmDspSysInstallCb1NN1(   h, ptIn, "f-out",  p->fx.tmop,  "in-0",  ctx->iChCnt );   // level ->adsr (tscale)
  cmDspSysInstallCb1NN1(   h, ptIn, "f-out",  p->fx.amop,  "in-0",  ctx->iChCnt );   // level ->adsr (ascale)

  if(0)
  {
    for(i=0; i<ctx->iChCnt; ++i)
    {
      cmDspSysConnectAudio( h, ain[i],   "out",               p->fx.mix[  ctx->iChCnt + i],  "in"    ); // ain -> sg/ain mixer
      cmDspSysInstallCb(    h, ptIn, formLabel(h,"b-out",i),  p->fx.adsr[ ctx->iChCnt + i],  "gate",  NULL );   // gate -> adsr
      cmDspSysInstallCb(    h, ptIn, formLabel(h,"f-out",i),  p->fx.tmop[ ctx->iChCnt + i],  "in-0",  NULL );   // level ->adsr (tscale)
      cmDspSysInstallCb(    h, ptIn, formLabel(h,"f-out",i),  p->fx.amop[ ctx->iChCnt + i],  "in-0",  NULL );   // level ->adsr (ascale)
    }
  }

  for( i=0; i<ctx->oChCnt; ++i)
  {
    cmDspSysConnectAudioN11N(h, p->fx.mt, "out", p->c.aout[i], "in", fxChCnt ); // fx.mix -> mixer
  }


  // reset the chain - called by the master 'reset' button
  cmDspSysInstallCb( h, selChk, "out", p->fx.start, "send", NULL );

 errLabel:

  cmDspSysRemoveInstAttrSymbol(h,p->c.circuitSymId);

  return rc;
}

//=====================================================================================================================
#define cdChainCnt (5)

typedef struct 
{
  const cmChar_t* preLbl;
  const cmChar_t* title;
  const cmChar_t* midiRsrc;
  const cmChar_t* chanRsrc;
  unsigned        chCnt;
  cmDspInst_t*    nom;
  cmDspInst_t*    ogain;
  cmDspInst_t**   mtr;
  cmDspInst_t**   envm;
  unsigned*       map;
} cmDspPP_Cd0_Desc_t;


typedef struct
{
  cmDspPP_Circuit_t   c;
  cmDspFx0_Chain_t    ch[ cdChainCnt ];
} cmDspPP_Cd0_t;



cmDspRC_t _cmDspPP_Cd0Alloc(
  cmDspSysH_t              h, 
  _cmDspPP_Ctx_t*          ctx,   
  cmDspPP_Cd0_t*           p,
  const cmChar_t*          title,
  const cmChar_t*          preLbl,
  cmDspInst_t*             selChk,
  cmDspInst_t**            ain,
  cmDspInst_t*             ptIn )
{
  cmDspRC_t       rc              = kOkDspRC;
  cmReal_t        cdMaxTimeSpanMs = 50;
  cmReal_t        cdMinNoteCnt    = 2;
  cmReal_t        nomXfadeMs      = 25.0;

  unsigned i,j;

  cmDspPP_Cd0_Desc_t desc[] = 
  {
    { "ch","Chose", "CDmidi",  "CDchan",  0, NULL, NULL, NULL, NULL, NULL },
    { "ot","Other", "CDmidi",  "CDchan",  0, NULL, NULL, NULL, NULL, NULL },
    { "g0","Grp0",  "CD0midi", "CD0chan", 0, NULL, NULL, NULL, NULL, NULL },
    { "g1","Grp1",  "CD1midi", "CD1chan", 0, NULL, NULL, NULL, NULL, NULL },
    { "ee","Else",  "NONmidi", "NONchan", 0, NULL, NULL, NULL, NULL, NULL }
  };

  assert( sizeof(desc)/sizeof(desc[0]) == cdChainCnt );

  for(i=0; i<cdChainCnt; ++i)
  {
    // read channel index resource array
    if( cmDspRsrcUIntArray( h, &desc[i].chCnt, &desc[i].map, desc[i].chanRsrc, NULL ) != kOkDspRC )
      return cmErrMsg(ctx->err,kInvalidArgDspRC,"The chord detector channel index resource '%s' could not be read.",cmStringNullGuard(desc[i].chanRsrc));
  }


  p->c.circuitSymId = cmDspSysRegisterInstAttrSymbolStr(h,title);

  /*
  cmDspInst_t* printBtn  = cmDspSysAllocInst( h, "Button", "_print",  2, kButtonDuiId, 0.0);
  cmDspInst_t* bcast     = cmDspSysAllocInst( h, "BcastSym",   NULL,  1, p->c.circuitSymId );
  cmDspSysInstallCb( h, printBtn, "sym", bcast, "msg", NULL );
  */

  cmDspInst_t* cd = cmDspSysAllocInst(h, "ChordDetect", NULL, 1, "cdSel" );
  cmDspInst_t* ns = cmDspSysAllocInst(h, "NoteSelect",  NULL, 1, ctx->iChCnt );
  
  // allocate the switching noms and gain controls
  for(i=0; i<cdChainCnt; ++i)
  {
    desc[i].nom    = cmDspSysAllocInst(  h, "NofM",  NULL, 3, desc[i].chCnt, desc[i].chCnt, nomXfadeMs );
    desc[i].ogain  = cmDspSysAllocScalar(h,formLabel(h,"Out Gn",i),0.0,10.0,0.01,1.0); 
  }

  // chord detector parameters
  cmDspInst_t* cdSpanMs  = cmDspSysAllocScalar(h,"Span Ms",  10.0,1000.0,1.0,cdMaxTimeSpanMs);
  cmDspInst_t* cdNoteCnt = cmDspSysAllocScalar(h,"Note Cnt",  1.0, 100.0,1.0,cdMinNoteCnt );
  cmDspInst_t* cdCount   = cmDspSysAllocScalar(h,"Ch. Count",   0, 1, 0, 0);
  cmDspInst_t* nomXfade  = cmDspSysAllocScalar(h,"Xfade ms",    0, 1000, 0, nomXfadeMs);

  cmDspInst_t* btn       = cmDspSysAllocButton(h,"print",0);

  // note select gate meters
  for(j=0; j<cdChainCnt; ++j)
  {
    cmDspSysNewColumn(h,50);
    cmDspSysAllocLabel(h,desc[j].title,kLeftAlignDuiId );
    desc[j].mtr = cmDspSysAllocInstArray(h, ctx->iChCnt, "Meter", NULL, NULL, 3, 0.0, 0.0, 1.0 );
  }

  //  audio output meters
  for(j=0; j<cdChainCnt; ++j)
  {                                                                     
    cmDspSysNewColumn(h,COL_WIDTH);
    cmDspSysAllocLabel(h,desc[j].title,kLeftAlignDuiId );
    desc[j].envm = cmDspSysAllocInstArray(h,desc[j].chCnt,"AMeter", NULL, NULL, 3, 0.0, 0.0, 1.0 );
  }

  // create the fx chains
  for(i=0; i<cdChainCnt; ++i )
  {
    cmDspSysNewPage(h,desc[i].title);
    unsigned presetGroupSymId = cmDspSysPresetRegisterGroup(h,desc[i].title);

    _cmDspSys_PresetMgmt(h,desc[i].preLbl,presetGroupSymId);
    cmDspSysNewColumn(h,COL_WIDTH);

    // create an fx chain with desc[i].chCnt channels
    if((rc = _cmDspFx0_ChainAlloc(h, ctx->err, presetGroupSymId, desc[i].preLbl, desc[i].midiRsrc, desc[i].chCnt, p->ch + i)) != kOkDspRC )
      goto errLabel;

  }

  // make mixers for each chain
  p->c.aout    = cmDspSysAllocInstArray( h, ctx->oChCnt, "AMix", NULL, NULL, 1, cdChainCnt );  
  p->c.aoutLbl = "out";

  // check for allocation errors
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    goto errLabel;

  cmDspSysInstallCb1N1N(h, ptIn, "b-out",  cd,   "gate", ctx->iChCnt ); // EF -> CD gate
  cmDspSysInstallCb1N1N(h, ptIn, "f-out",  cd,   "rms",  ctx->iChCnt ); // EF -> CD rms
  cmDspSysInstallCb1N1N(h, cd,   "gate",   ns,   "gate", ctx->iChCnt ); // CD -> NS gate
  cmDspSysInstallCb1N1N(h, cd,   "rms",    ns,   "rms",  ctx->iChCnt ); // CD -> NS rms
  cmDspSysInstallCb(h,     cd,   "detect", ns,   "trig",  NULL );       // CD -> NS trigger

  // chord detector paramter controls
  cmDspSysInstallCb(h, cdSpanMs,  "val",   cd,      "span",  NULL );
  cmDspSysInstallCb(h, cdNoteCnt, "val",   cd,      "notes", NULL );
  cmDspSysInstallCb(h, cd,       "count",  cdCount, "val",   NULL );

  for(i=0; i<cdChainCnt; ++i)
  {
    cmDspSysInstallCb(         h, nomXfade, "val", desc[i].nom, "ms", NULL );

    cmDspSysInstallCb1N1NM(    h, ns,    formLabel(h,"gate",i), ctx->iChCnt, desc[i].nom, "sel",  desc[i].chCnt, desc[i].map ); // NS -> nom's (selectors)
    cmDspSysInstallCb1NN1 (    h, ns,    formLabel(h,"gate",i),              desc[i].mtr, "in",   ctx->iChCnt                ); // NS -> Meter 
    cmDspSysInstallCb(         h, ns,    "done",                desc[i].nom, "cmd",  NULL );        // NS -> nom's (done triggers)

    cmDspSysConnectAudioN11NM( h, ain,   "out",                 ctx->iChCnt, desc[i].nom, "a-in", desc[i].chCnt, desc[i].map ); // ain -> nom's

    cmDspSysInstallCb1N1NM(    h, ptIn,  "b-out",               ctx->iChCnt, desc[i].nom, "b-in", desc[i].chCnt, desc[i].map ); // NS -> nom (gate)
    cmDspSysInstallCb1N1NM(    h, ptIn,  "f-out",               ctx->iChCnt, desc[i].nom, "f-in", desc[i].chCnt, desc[i].map ); // NS -> nom (rms)

    cmDspSysConnectAudio1NN1(h, desc[i].nom, "a-out", p->ch[i].mix,  "in",   desc[i].chCnt );   // nom -> chain sg/ain mixer (audio)
    cmDspSysInstallCb1NN1(   h, desc[i].nom, "b-out", p->ch[i].adsr, "gate", desc[i].chCnt );   // nom -> chain adsr (gate)
    cmDspSysInstallCb1NN1(   h, desc[i].nom, "f-out", p->ch[i].tmop, "in-0", desc[i].chCnt );   // nom -> chain adsr (tscale)
    cmDspSysInstallCb1NN1(   h, desc[i].nom, "f-out", p->ch[i].amop, "in-0", desc[i].chCnt );   // nom -> chain adsr (ascale)

    cmDspSysInstallCb(       h, btn, "sym",   desc[i].nom,   "cmd",  NULL );

    cmDspSysConnectAudioN1N1( h, p->ch[i].mt,   "out", desc[i].envm, "in",                  desc[i].chCnt ); // chain aout -> mtr
    cmDspSysConnectAudio11N1( h, p->ch[i].mixo, "out", p->c.aout,     formLabel(h,"in", i), ctx->oChCnt);    // chain aout -> mix out


    // reset the chain - (called by the master 'reset' button)
    cmDspSysInstallCb( h, selChk, "out", p->ch[i].start, "send", NULL );

  }

 errLabel:

  cmDspSysRemoveInstAttrSymbol(h,p->c.circuitSymId);

  return rc;
}

//=====================================================================================================================

#define cd1ChainCnt (10)

typedef struct 
{
  const cmChar_t* preLbl;
  const cmChar_t* title;
  unsigned        chCnt;
  cmDspInst_t*    nom;
  cmDspInst_t*    ogain;
  cmDspInst_t**   mtr;
  cmDspInst_t**   envm;
  const cmChar_t* midiRsrc;
  unsigned*       midi;
  const cmChar_t* chanRsrc;
  unsigned*       chan;
} cmDspPP_Cd1_Desc_t;


typedef struct
{
  cmDspPP_Circuit_t   c;
  cmDspFx0_Chain_t    ch[ cd1ChainCnt ];
} cmDspPP_Cd1_t;



cmDspRC_t _cmDspPP_Cd1Alloc(
  cmDspSysH_t              h, 
  _cmDspPP_Ctx_t*          ctx,   
  cmDspPP_Cd1_t*           p,
  const cmChar_t*          title,
  const cmChar_t*          preLbl,
  cmDspInst_t*             selChk,
  cmDspInst_t**            ain,
  cmDspInst_t*             ptIn )
{
  cmDspRC_t       rc              = kOkDspRC;
  cmReal_t        cdMaxTimeSpanMs = 50;
  cmReal_t        cdMinNoteCnt    = 2;
  cmReal_t        nomXfadeMs      = 25.0;
  unsigned        cdNetNodeId     = cmInvalidId;
  const cmChar_t* cdNetNodeLabel  = NULL;
  const cmChar_t* nonCdNetNodeLabel = NULL;
  unsigned        cdChCnt         = 0;
  cmDspInst_t*    cd              = NULL;
  cmDspInst_t*    ns              = NULL;
  cmDspInst_t*    cdSpanMs        = NULL;
  cmDspInst_t*    cdNoteCnt       = NULL;
  cmDspInst_t*    cdCount         = NULL;
  cmDspInst_t*    btn             = NULL;
  unsigned        cd1ChainCnto2   = cd1ChainCnt/2;
  unsigned        pgSymId         = cmInvalidId;

  unsigned i,j;

  cmDspPP_Cd1_Desc_t desc[] = 
  {
    // resources for local (this) machine
    { "ch0","Ch0",   0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { "ot0","Oth0",  0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { "g00","Gr00",  0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { "g01","Gr01",  0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { "ee0","Ee00",  0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // resources for remote (other) machine
    { "ch1","Ch1",   0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { "ot1","Oth1",  0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { "g10","Gr10",  0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { "g11","Gr11",  0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { "ee1","Ee01",  0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }

  };

  assert( (sizeof(desc)/sizeof(desc[0])) == cd1ChainCnt );

  // get the chord detector network node label of this machine
  if( cmDspRsrcString(h, &cdNetNodeLabel, "cdLocalNetNode", NULL ) != kOkDspRC )
  {
    rc = cmErrMsg(ctx->err,kRsrcNotFoundDspRC,"The resource 'cdLocalNetNode' could not be read.");
    goto errLabel;
  }

  // convert the chord detector network node label to an id
  if( (cdNetNodeId = cmDspSysNetNodeLabelToId(h,cdNetNodeLabel)) == cmInvalidId )
  {
    rc = cmErrMsg(ctx->err,kInvalidArgDspRC,"The chord detector network node label '%s' is not valid.",cmStringNullGuard(cdNetNodeLabel));
    goto errLabel;
  }

  // get the label of the network node which is not running the chord detector
  if( cmDspRsrcString(h, &nonCdNetNodeLabel, "cdRemoteNetNode", NULL ) != kOkDspRC )
  {
    rc = cmErrMsg(ctx->err,kRsrcNotFoundDspRC,"The resource 'cdRemoteNetNode' could not be read.");
    goto errLabel;
  }

  // validate the non-chord detector network node label
  if( cmDspSysNetNodeLabelToId(h,nonCdNetNodeLabel) == cmInvalidId )
  {
    rc = cmErrMsg(ctx->err,kInvalidArgDspRC,"The net node label '%s' is not valid.",cmStringNullGuard(nonCdNetNodeLabel));
    goto errLabel;    
  }

  // is this the chord detector node
  bool     cdIsLocalFl = cdNetNodeId == cmDspSysNetNodeId(h);
  unsigned ii          = cdIsLocalFl ? 0 : cd1ChainCnto2;
  unsigned nn          = ii + cd1ChainCnto2;

  // read all the resource arrays 
  for(i=0; i<cd1ChainCnt; ++i)
  {
    desc[i].midiRsrc = cmTsPrintfH( cmDspSysLHeap(h), "nsMidi-%i",i);
    desc[i].chanRsrc = cmTsPrintfH( cmDspSysLHeap(h), "nsChan-%i",i);

    if( cmDspRsrcUIntArray(h,&desc[i].chCnt,&desc[i].midi,desc[i].midiRsrc,NULL) != kOkDspRC )
    {
      rc = cmErrMsg(ctx->err,kRsrcNotFoundDspRC,"The note selector MIDI pitch resource '%s' could not be read.",cmStringNullGuard(desc[i].midiRsrc));
      goto errLabel;
    }

    if( cmDspRsrcUIntArray(h,&desc[i].chCnt,&desc[i].chan,desc[i].chanRsrc,NULL) != kOkDspRC )
    {
      rc = cmErrMsg(ctx->err,kRsrcNotFoundDspRC,"The note selector channel map resource '%s' could not be read.",cmStringNullGuard(desc[i].chanRsrc));
      goto errLabel;
    }

  }

  p->c.circuitSymId = cmDspSysRegisterInstAttrSymbolStr(h,title);

  if( cdIsLocalFl )
  {
    // get the count of chord detector input channels
    if( cmDspRsrcArrayCount(h,&cdChCnt,"ChordList",NULL) != kOkDspRC )
      return cmErrMsg(ctx->err,kInvalidArgDspRC,"The 'ChordList' resource could not be read.");
    
    cd = cmDspSysAllocInst(h, "ChordDetect",    "cd", 1, "ChordList" );
    ns = cmDspSysAllocInst(h, "NetNoteSelect",  "ns", 1, cdChCnt );


    // chord detector parameters
    cdSpanMs  = cmDspSysAllocScalarP(h, pgSymId, preLbl, "Span Ms",  10.0,1000.0,1.0,cdMaxTimeSpanMs);
    cdNoteCnt = cmDspSysAllocScalarP(h, pgSymId, preLbl, "Note Cnt",  1.0, 100.0,1.0,cdMinNoteCnt );
    cdCount   = cmDspSysAllocScalarP(h, pgSymId, preLbl, "Ch. Count",   0, 1,0,0);
  }

  cmDspInst_t* nomXfade  = cmDspSysAllocScalarP(h, pgSymId, preLbl, "Xfade ms",  0, 1000, 0, nomXfadeMs);


  btn  = cmDspSysAllocButtonP(h,preLbl,"print",0);

  // allocate the switching noms and gain controls
  for(i=ii; i<nn; ++i)
  {
    desc[i].nom    = cmDspSysAllocInst(  h, "NofM",  formLabel(h,"nom",i), 3, desc[i].chCnt, desc[i].chCnt, nomXfadeMs );
    desc[i].ogain  = cmDspSysAllocScalarP(h, pgSymId, preLbl, formLabel(h,"Out Gn",i),0.0,10.0,0.01,1.0); 
  }

  // note-select gate meters
  for(j=ii; cdIsLocalFl && j<nn; ++j)
  {
    cmDspSysNewColumn(h,50);
    cmDspSysAllocLabel(h,desc[j].title,kLeftAlignDuiId );
    desc[j].mtr = cmDspSysAllocInstArray(h, cdChCnt, "Meter", NULL, NULL, 3, 0.0, 0.0, 1.0 );
  }

  //  audio output meters
  for(j=ii; j<nn; ++j)
  {                                                                     
    cmDspSysNewColumn(h,50);
    cmDspSysAllocLabel(h,desc[j].title,kLeftAlignDuiId );
    desc[j].envm = cmDspSysAllocInstArray(h,desc[j].chCnt,"AMeter", NULL, NULL, 3, 0.0, 0.0, 1.0 );
  }

  // create the fx chains
  for(i=ii; i<nn; ++i )
  {
    cmDspSysNewPage(h,desc[i].title);
    unsigned presetGroupSymId = cmDspSysPresetRegisterGroup(h,desc[i].title);

    _cmDspSys_PresetMgmt(h,desc[i].preLbl,presetGroupSymId);
    cmDspSysNewColumn(h,200);

    // create an fx chain with desc[i].chCnt channels
    if((rc = _cmDspFx0_ChainAlloc(h, ctx->err, presetGroupSymId, desc[i].preLbl, desc[i].midiRsrc, desc[i].chCnt, p->ch + i)) != kOkDspRC )
      goto errLabel;

  }

  // make mixers for each chain
  p->c.aout    = cmDspSysAllocInstArray( h, ctx->oChCnt, "AMix", NULL, NULL, 1, cd1ChainCnto2 );  
  p->c.aoutLbl = "out";

  // check for allocation errors
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    goto errLabel;

  if( cdIsLocalFl )
  {
    cmDspSysInstallCb1N1N(h, ptIn, "b-out",  cd,   "gate", ctx->iChCnt ); // EF -> CD gate
    cmDspSysInstallCb1N1N(h, ptIn, "f-out",  cd,   "rms",  ctx->iChCnt ); // EF -> CD rms
    cmDspSysInstallCb1N1N(h, cd,   "gate",   ns,   "gate", cdChCnt     ); // CD -> NS gate
    cmDspSysInstallCb1N1N(h, cd,   "rms",    ns,   "rms",  cdChCnt     ); // CD -> NS rms
    cmDspSysInstallCb(h,     cd,   "detect", ns,   "trig",  NULL );       // CD -> NS trigger

    // chord detector paramter controls
    cmDspSysInstallCb(h, cdSpanMs,  "val",   cd,      "span",  NULL );
    cmDspSysInstallCb(h, cdNoteCnt, "val",   cd,      "notes", NULL );
    cmDspSysInstallCb(h, cd,       "count",  cdCount, "val",   NULL );
  }
  else
  {
    cmDspSysInstallNetCb1N1N(h, ptIn,  "b-out",  cdNetNodeLabel, "cd", "gate", 16, ctx->iChCnt  );
    cmDspSysInstallNetCb1N1N(h, ptIn,  "f-out",  cdNetNodeLabel, "cd", "rms",  16, ctx->iChCnt  );
  }

  for(i=ii; i<nn; ++i)
  {

    if( cdIsLocalFl )
    {
      // local NS -> nom's (selectors) 
      cmDspSysInstallCb1N1N(    h, ns,    formLabel(h,"gate",i), desc[i].nom, "sel",  desc[i].chCnt ); 
      cmDspSysInstallCb(        h, ns,    "done",                desc[i].nom, "cmd",  NULL );        // NS -> nom's (done triggers)

      // local NS -> meters
      cmDspSysInstallCb1NN1M2(  h, ns,    formLabel(h,"gate",i), desc[i].chCnt, desc[i].chan, desc[i].mtr, "in", cdChCnt ); 

    }


    cmDspSysConnectAudioN11NM( h, ain, "out", ctx->iChCnt,   desc[i].nom, "a-in", desc[i].chCnt, desc[i].chan ); // ain -> nom's

    cmDspSysInstallCb1N1NM(   h, ptIn, "b-out", ctx->iChCnt, desc[i].nom, "b-in", desc[i].chCnt, desc[i].chan ); // EF -> nom (gate)
    cmDspSysInstallCb1N1NM(   h, ptIn, "f-out", ctx->iChCnt, desc[i].nom, "f-in", desc[i].chCnt, desc[i].chan ); // EF -> nom (level)

    cmDspSysConnectAudio1NN1(h, desc[i].nom, "a-out", p->ch[i].mix,  "in",   desc[i].chCnt );   // nom -> chain sg/ain mixer (audio)
    cmDspSysInstallCb1NN1(   h, desc[i].nom, "b-out", p->ch[i].adsr, "gate", desc[i].chCnt );   // nom -> chain adsr (gate)
    cmDspSysInstallCb1NN1(   h, desc[i].nom, "f-out", p->ch[i].tmop, "in-0", desc[i].chCnt );   // nom -> chain adsr (tscale)
    cmDspSysInstallCb1NN1(   h, desc[i].nom, "f-out", p->ch[i].amop, "in-0", desc[i].chCnt );   // nom -> chain adsr (ascale)

    cmDspSysInstallCb(       h, btn,      "sym", desc[i].nom, "cmd",  NULL );
    cmDspSysInstallCb(       h, nomXfade, "val", desc[i].nom, "ms",   NULL );

    cmDspSysConnectAudioN1N1( h, p->ch[i].mt,   "out", desc[i].envm, "in",                     desc[i].chCnt ); // chain aout  -> mtr
    cmDspSysConnectAudio11N1( h, p->ch[i].mixo, "out", p->c.aout,    formLabel(h,"in",  i-ii), ctx->oChCnt);    // chain aout  -> mix out
    cmDspSysInstallCb11N1(    h, desc[i].ogain, "val", p->c.aout,    formLabel(h,"gain",i-ii), ctx->oChCnt);    // chain ogain -> mix gain

    // reset the chain - (called by the master 'reset' button)
    cmDspSysInstallCb( h, selChk, "out", p->ch[i].start, "send", NULL );

  }

  // local note-select to remote 'nom' connections
  if( cdIsLocalFl )
  {
    for(i=cd1ChainCnto2; i<cd1ChainCnt; ++i)
    {
      // NS -> nom's (selectors and done trigger)
      cmDspSysInstallNetCb1N1N( h, ns,  formLabel(h,"gate",i), nonCdNetNodeLabel, cmTsPrintfS("nom-%i",i), "sel",  0, desc[i].chCnt );
      cmDspSysInstallNetCb(     h, ns,  "done",                nonCdNetNodeLabel, cmTsPrintfS("nom-%i",i), "cmd"  );        

      // NS -> meters 
      unsigned map[ ctx->iChCnt ];
      cmVOU_AddVVS(map,ctx->iChCnt,desc[i].chan,ctx->iChCnt );
      cmDspSysInstallCb1NN1M2(   h, ns,  formLabel(h,"gate",i), desc[i].chCnt, map, desc[i-cd1ChainCnto2].mtr, "in", cdChCnt ); 

    }
    
  }
 errLabel:

  cmDspSysRemoveInstAttrSymbol(h,p->c.circuitSymId);

  return rc;
}

//=====================================================================================================================
typedef struct
{
  cmDspInst_t* phs;
  cmDspInst_t* wt;
  cmDspInst_t* sel;
  cmDspInst_t* bts;
  cmDspInst_t* gain;
} cmDspPP_Fp_t;

cmDspRC_t _cmDspPP_FilePlayer( 
    cmDspSysH_t     h, 
    const cmChar_t* fn,
    const cmChar_t* label,
    cmDspPP_Fp_t*   p
)
{
  cmDspRC_t rc         = kOkDspRC;
  int       wtPlayCnt  = 1;
  int       wtMode     = 1;     // file
  int       wtBeg      = 0;
  int       wtEnd      = -1;
  unsigned  wtOffSymId = cmDspSysRegisterStaticSymbol(h,"off");
  bool      selFl      = false;

  p->sel  =  cmDspSysAllocCheck( h,label,selFl);

  p->phs  =  cmDspSysAllocInst(  h,"Phasor",   NULL,  0 );
  p->wt   =  cmDspSysAllocInst(  h,"WaveTable",NULL,  7, ((int)cmDspSysSampleRate(h)), wtMode, fn, wtPlayCnt, wtBeg, wtEnd, wtOffSymId );
  p->bts  =  cmDspSysAllocInst(  h,"GateToSym", NULL, 0 );
  p->gain =  cmDspSysAllocScalar(h,NULL,0.0,10.0,0.01,1.0);

  // check for allocation errors
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    goto errLabel;

  cmDspSysConnectAudio(h,p->phs,"out",p->wt,"phs");

  cmDspSysInstallCb( h, p->sel, "out", p->bts, "both", NULL );
  cmDspSysInstallCb( h, p->bts, "out", p->wt,  "cmd", NULL );
  cmDspSysInstallCb( h, p->sel, "out", p->phs, "phs", NULL );

 errLabel:
  return rc;
}

typedef struct
{
  cmDspPP_Circuit_t   c;
} cmDspPP_Sp0_t;


cmDspRC_t _cmDspPP_SmpPlayAlloc(
  cmDspSysH_t     h, 
  _cmDspPP_Ctx_t* ctx,   
  cmDspPP_Sp0_t*  p,
  const cmChar_t* title,
  const cmChar_t* preLbl,
  cmDspInst_t*    selChk,
  cmDspInst_t**   ain,
  cmDspInst_t*    ptIn,
  const cmChar_t** fnArray,
  const cmChar_t** titleArray,
  unsigned         fnCnt )
{
  cmDspRC_t rc;
  unsigned i,j;

  cmDspPP_Fp_t fp[ fnCnt ];

  p->c.circuitSymId     = cmDspSysRegisterInstAttrSymbolStr(h,title);
  unsigned    presetGroupSymId = cmDspSysPresetRegisterGroup(h,title);
  _cmDspSys_PresetMgmt(h,preLbl,presetGroupSymId);
  cmDspSysNewColumn(h,COL_WIDTH);

  // make sample players
  for(i=0; i<fnCnt; ++i)
  {
    if((rc = _cmDspPP_FilePlayer(h, fnArray[i], titleArray[i], fp + i )) != kOkDspRC )
      break;
  }

  // make processors for each output channel
  p->c.aout = cmDspSysAllocInstArray( h, ctx->oChCnt, "AMix", NULL, NULL, 1, fnCnt );
  p->c.aoutLbl = "out";
  
  // check for allocation errors
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    goto errLabel;

  for(i=0; i<ctx->oChCnt; ++i)
    for(j=0; j<fnCnt; ++j)
    {
      cmDspSysConnectAudio(h, fp[j].wt,   "out", p->c.aout[i], formLabel(h,"in",j) );
      cmDspSysInstallCb(   h, fp[j].gain, "val", p->c.aout[i], formLabel(h,"gain",j), NULL);
    }

 errLabel:

  cmDspSysRemoveInstAttrSymbol(h,p->c.circuitSymId);

  return rc;

}

//=====================================================================================================================
typedef struct
{
  cmDspPP_Circuit_t   c;
  cmDspFx0_Chain_t    fx0;
  cmDspFx0_Chain_t    fx1;
} cmDspPP_Gliss_t;


cmDspRC_t _cmDspPP_GlissAlloc(
  cmDspSysH_t              h, 
  _cmDspPP_Ctx_t*          ctx,   
  cmDspPP_Gliss_t*   p,
  const cmChar_t*          title,
  const cmChar_t*          preLbl,
  cmDspInst_t*             selChk,
  cmDspInst_t**            ain,
  cmDspInst_t*             ptIn )
{
  cmDspRC_t rc;
  const cmChar_t* glissChIdxRsrcStr = "glissChIdx";
  const cmChar_t* glissPitchRsrcStr = "glissPitch";
  const cmChar_t* glissSlineRsrcStr = "glissSline";
  unsigned        chCnt = 0;
  unsigned        i ;

  p->c.circuitSymId     = cmDspSysRegisterInstAttrSymbolStr(h,title);
  unsigned    presetGroupSymId = cmDspSysPresetRegisterGroup(h,title);
  _cmDspSys_PresetMgmt(h,preLbl,presetGroupSymId);
  cmDspSysNewColumn(h,COL_WIDTH);

  p->c.aout  = allocInstPtrArray(h, ctx->oChCnt);

  // get the active channel list (indexes of the input channels which will be used by glisser);
  if( cmDspRsrcArrayCount( h, &chCnt, glissChIdxRsrcStr, NULL ) != kOkDspRC )
    return cmErrMsg(ctx->err,kPgmCfgFailDspRC,"The noise shaper channel index list '%s' could not be read.",cmStringNullGuard(glissChIdxRsrcStr));

  cmDspInst_t*  print  = cmDspSysAllocInst(      h, "Printer",   NULL, 0 );
  cmDspInst_t*  nom    = cmDspSysAllocInst(      h, "NofM",      NULL, 2, ctx->iChCnt, chCnt );  
  cmDspInst_t*  pts    = cmDspSysAllocInst(      h, "PortToSym", NULL, 2, "off", "send" );
  cmDspInst_t*  pitarr = cmDspSysAllocInst(      h, "Array",     NULL, 1, glissPitchRsrcStr );
  cmDspInst_t*  charr  = cmDspSysAllocInst(      h, "Array",     NULL, 1, glissChIdxRsrcStr );
  cmDspInst_t** gts    = cmDspSysAllocInstArray( h, chCnt, "GateToSym", NULL, NULL, 0 );
  cmDspInst_t** pcvt   = cmDspSysAllocInstArray( h, chCnt, "PitchCvt",  NULL, NULL, 0 );
  

  cmDspInst_t** sline = allocInstPtrArray(h, chCnt);
  for(i=0; i<chCnt; ++i)
    sline[i] = cmDspSysAllocInst( h, "SegLine", NULL, 1, formLabel(h,glissSlineRsrcStr,i) );


  if((rc = _cmDspFx0_ChainAlloc(h,ctx->err, presetGroupSymId, formLabel(h,preLbl,0), glissPitchRsrcStr, chCnt, &p->fx0)) != kOkDspRC )
    goto errLabel;

  cmDspSysNewPage(h,"G-Thru");

  if((rc = _cmDspFx0_ChainAlloc(h,ctx->err, presetGroupSymId, formLabel(h,preLbl,1), glissPitchRsrcStr, chCnt, &p->fx1)) != kOkDspRC )
    goto errLabel;

  cmDspInst_t** mix  = cmDspSysAllocInstArray( h, chCnt, "AMix", NULL, NULL, 1, 2 );

  p->c.aout = cmDspSysAllocInstArray( h, ctx->iChCnt, "AMix", NULL, NULL, 1, chCnt );
  p->c.aoutLbl = "out";

  // check for allocation errors
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    goto errLabel;

  // selChk is the circuit selector check box in the main window
  cmDspSysInstallCb(       h, selChk, "out",      pts,    "off",  NULL );  // First send 'off' to nom to clear its selector
  cmDspSysInstallCb(       h, selChk, "out",      pts,    "send", NULL );  // Second send 'send' to charr to select active ports on nom.
  cmDspSysInstallCb(       h, pts,    "off",      nom,    "cmd",  NULL );  // Clear the nom (all inputs are turned off)
  cmDspSysInstallCb(       h, pts,    "send",     charr,  "cmd",  NULL );  // Send the active channel indexes to the nom
  cmDspSysInstallCb(       h, pts,    "send",     pitarr, "cmd",  NULL );  // Send the pitch values to the circuits (eq,cf,sg,..)
  cmDspSysInstallCb11N1(   h, pts,    "send",     sline,  "cmd",  chCnt );
  cmDspSysInstallCb(       h, charr,  "done",     nom,    "cmd",  NULL );  // Tell the nom when its setup is complete

  cmDspSysInstallCb1N11(   h, charr,  "out",      nom,    "seli", chCnt );        // EF nom channel index selectors 
  cmDspSysConnectAudioN11N(h, ain,    "out",      nom,    "a-in", ctx->iChCnt );  // ain -> nom 
  cmDspSysInstallCb1N1N(   h, ptIn,   "b-out",    nom,    "b-in", ctx->iChCnt );  // EF -> nom (gates)
  cmDspSysInstallCb1N1N(   h, ptIn,   "f-out",    nom,    "f-in", ctx->iChCnt );  // EF -> nom (level)
  cmDspSysInstallCb1NN1(   h, pitarr, "out",      pcvt,   "midi", chCnt );        // pitch array -> pitch converter

  cmDspSysInstallCb1NN1(   h, nom,    "b-out",    gts,      "on",   chCnt );      
  cmDspSysInstallCbN1N1(   h, gts,    "out",      sline,    "trig", chCnt );
  cmDspSysInstallCbN1N1(   h, sline,  "out",      pcvt,     "offs", chCnt );
  cmDspSysInstallCbN1N1(   h, pcvt,   "ratio",    p->fx0.ps,"ratio",chCnt );

  cmDspSysInstallCbN111(   h, sline, "out", print, "in", chCnt );


  cmDspSysConnectAudio1NN1( h, nom, "a-out",  p->fx0.mix,   "in",    chCnt );   // ain -> sg/ain mixer
  cmDspSysInstallCb1NN1(    h, nom, "b-out",  p->fx0.adsr,  "gate",  chCnt );   // gate -> adsr
  cmDspSysInstallCb1NN1(    h, nom, "f-out",  p->fx0.tmop,  "in-0",  chCnt );   // level ->adsr (tscale)
  cmDspSysInstallCb1NN1(    h, nom, "f-out",  p->fx0.amop,  "in-0",  chCnt );   // level ->adsr (ascale)

  cmDspSysConnectAudio1NN1( h, nom, "a-out",  p->fx1.mix,   "in",    chCnt );   // ain -> sg/ain mixer
  cmDspSysInstallCb1NN1(    h, nom, "b-out",  p->fx1.adsr,  "gate",  chCnt );   // gate -> adsr
  cmDspSysInstallCb1NN1(    h, nom, "f-out",  p->fx1.tmop,  "in-0",  chCnt );   // level ->adsr (tscale)
  cmDspSysInstallCb1NN1(    h, nom, "f-out",  p->fx1.amop,  "in-0",  chCnt );   // level ->adsr (ascale)

  cmDspSysConnectAudioN1N1( h, p->fx0.mt, "out", mix, "in-0", chCnt );
  cmDspSysConnectAudioN1N1( h, p->fx1.mt, "out", mix, "in-1", chCnt );

  for( i=0; i<ctx->oChCnt; ++i)
  {
    cmDspSysConnectAudioN11N(h, mix, "out", p->c.aout[i], "in", chCnt ); 
  }

  // reset the chain - called by the master 'reset' button
  cmDspSysInstallCb( h, selChk, "out", p->fx0.start, "send", NULL );
  cmDspSysInstallCb( h, selChk, "out", p->fx1.start, "send", NULL );
  
 errLabel:

  cmDspSysRemoveInstAttrSymbol(h,p->c.circuitSymId);

  return rc;

}

//=====================================================================================================================
_cmDspPP_CircDesc_t _cmDspPP_CircDescArray[] =
{
  { "Test","      tc"  },
  { "N Shaper",   "ns"  },
  { "Fx0",            "fx0" },
  { "Smp Play",    "sp"  },
  { "CD",             "cd"  },
  { "Net CD", "ncd" },
  { "Gliss",          "gss" }
};

unsigned                  _cmDspPP_CircuitDescCount()
{
  return sizeof(_cmDspPP_CircDescArray)/sizeof(_cmDspPP_CircDescArray[0]);
}

const _cmDspPP_CircDesc_t* _cmDspPP_CircuitDesc( unsigned idx )
{ 
  assert(idx < _cmDspPP_CircuitDescCount());
  return _cmDspPP_CircDescArray + idx;
}


cmDspRC_t _cmDspPP_CircuitSwitchAlloc( 
  cmDspSysH_t              h, 
  _cmDspPP_Ctx_t*          ctx, 
  cmDspPP_CircuitSwitch_t* p, 
  cmDspInst_t*             reset,
  cmDspInst_t**            sel,
  cmDspInst_t**            ain, 
  cmDspInst_t**            ef  )
{
  cmDspRC_t rc      = kOkDspRC;
  double    xfadeMs = 50.0;
  unsigned  enaSym  = cmDspSysRegisterStaticSymbol(h,"_enable");
  unsigned  disSym  = cmDspSysRegisterStaticSymbol(h,"_disable");
  unsigned  i;

  cmDspPP_TestCircuit_t Tst; memset(&Tst,0,sizeof(Tst));
  cmDspPP_NSh_Circuit_t NSh; memset(&NSh,0,sizeof(NSh));
  cmDspPP_Fx0_t         Fx0; memset(&Fx0,0,sizeof(Fx0));
  cmDspPP_Cd0_t         Cd0; memset(&Cd0,0,sizeof(Cd0));
  cmDspPP_Cd1_t         Cd1; memset(&Cd1,0,sizeof(Cd1));
  cmDspPP_Sp0_t         Sp0; memset(&Sp0,0,sizeof(Sp0));
  cmDspPP_Gliss_t       Gls; memset(&Gls,0,sizeof(Gls));

  const cmChar_t* spFnArr[] = 
  {
    "/home/kevin/media/audio/PP/Siren Blast 2 Peaking.wav",
    "/home/kevin/media/audio/PP/Siren Blast 2.wav"
  };

  const cmChar_t* spLblArr[] = 
  {
    "File 0",
    "File 1"
  };

  unsigned spCnt  = sizeof(spFnArr)  / sizeof(spFnArr[0]);
  unsigned splCnt = sizeof(spLblArr) / sizeof(spLblArr[0]);
  assert( spCnt == splCnt );


  p->circuitCnt = _cmDspPP_CircuitDescCount();

  cmDspInst_t*       ofd[  p->circuitCnt ];
  cmDspPP_Circuit_t* carr[ p->circuitCnt ];

   cmDspInst_t** mxm     = cmDspSysAllocInstArray(h,ctx->oChCnt,"AMeter", NULL, NULL, 3, 0.0, 0.0, 1.0 );

  for(i = 0; rc==kOkDspRC && i<p->circuitCnt; ++i)
  {

    const cmChar_t* title  =  _cmDspPP_CircDescArray[i].title;
    const cmChar_t* preLbl =  _cmDspPP_CircDescArray[i].preLbl;

    cmDspSysNewPage(h, title );

    carr[i] = NULL;

    // allocate the input switches for this circuit
    cmDspInst_t* bts      = cmDspSysAllocInst(h,"GateToSym", NULL, 0 );
    cmDspInst_t* bts_ena  = cmDspSysAllocInst(h,"GateToSym", NULL, 2, enaSym, disSym );
    cmDspInst_t* ipt      = cmDspSysAllocInst(h,"NofM",      NULL, 2, ctx->iChCnt, ctx->iChCnt   );

    // allocate the circuits
    switch( i )
    {
      case 0:
        if((rc = _cmDspPP_TestCircuitAlloc(h,ctx,&Tst,title,preLbl,sel[i],ain,ipt)) == kOkDspRC )
          carr[i] = &Tst.c;
        break;

      case 1:
        if((rc =  _cmDspPP_NoiseShaperAlloc(h,ctx->err,&NSh,title,preLbl,sel[i],ipt,ctx->iChCnt,ctx->oChCnt)) == kOkDspRC )
          carr[i] = &NSh.c;
        break;

      case 2:
        if((rc =_cmDspPP_Fx0Alloc(h,ctx,&Fx0,title,preLbl,sel[i],ain,ipt)) == kOkDspRC )
          carr[i] = &Fx0.c;
        break;

      case 3:
        if((rc =_cmDspPP_SmpPlayAlloc(h,ctx,&Sp0,title,preLbl,sel[i],ain,ipt,spFnArr,spLblArr,spCnt)) == kOkDspRC )
          carr[i] = &Sp0.c;
        break;

      case 4:
        if((rc =_cmDspPP_Cd0Alloc(h,ctx,&Cd0,title,preLbl,sel[i],ain,ipt)) == kOkDspRC )
          carr[i] = &Cd0.c;
        break;
        
      case 5:
        if((rc = _cmDspPP_Cd1Alloc(h,ctx,&Cd1,title,preLbl,sel[i],ain,ipt)) == kOkDspRC )
          carr[i] = &Cd1.c;
        break;

      case 6:
        if((rc = _cmDspPP_GlissAlloc(h,ctx,&Gls,title,preLbl,sel[i],ain,ipt)) == kOkDspRC )
          carr[i] = &Gls.c;
        break;
        
      default:
        { assert(0); }
    }

    if( rc == kOkDspRC && carr[i] != NULL )
    {
      printf("title:%s circuit sym id:%i\n",title,carr[i]->circuitSymId);

      cmDspInst_t* pts_dis  = cmDspSysAllocInst( h, "PortToSym", NULL,  1, "_disable");
      cmDspInst_t* bcast    = cmDspSysAllocInst( h, "BcastSym",   NULL, 1, carr[i]->circuitSymId );
      ofd[i]                = cmDspSysAllocInst( h, "Xfader",     NULL, 2, ctx->oChCnt, xfadeMs );
      cmDspSysNewColumn(h,COL_WIDTH);
      cmDspInst_t** fdm     = cmDspSysAllocInstArray(h,ctx->oChCnt,"AMeter", NULL, NULL, 3, 0.0, 0.0, 1.0 );
      
      // check for allocation errors
      if((rc = cmDspSysLastRC(h)) != kOkDspRC )
        goto errLabel;

      cmDspSysConnectAudio1NN1( h, ofd[i], "out", fdm, "in", ctx->oChCnt ); // fdr -> meter
      
      cmDspSysInstallCb(    h, sel[i], "out", bts,       "both",    NULL );  // Convert circuit selection gate to 'on'/'off' symbols
      cmDspSysInstallCb(    h, bts,    "out", ipt,       "cmd",     NULL );  // send 'on'/'off' to this circuits Gate/RMS switch

      cmDspSysInstallCb(    h, sel[i], "out", bts_ena,   "both",    NULL );  // Convert circuit selection gate to '_enable/_disable' symbols
      cmDspSysInstallCb(    h, bts_ena,"out", bcast,     "msg",     NULL );  // send '_enable/_disable' to this circuits instances.

      cmDspSysInstallCb(    h, reset,  "out", pts_dis,   "_disable",NULL );  // Convert 'reset' button output to '_disable' symbol
      cmDspSysInstallCb(    h, ofd[i], "off", pts_dis,   "_disable",NULL );  // Convert circuit output fader 'off' event to '_disable' symbol.
      cmDspSysInstallCb(    h, pts_dis,"out", bcast,     "msg",     NULL );  // Bcast _disable symbol generated from 'reset' btn or fader 'off'
    
      cmDspSysInstallCb(    h, sel[i], "out", ofd[i],    "mgate", NULL );   // Convert circuit selection gate to master fader gate 'on'/'off' msg's

      cmDspSysInstallCbN11N( h, ef,   "gate",  ipt,      "b-in", ctx->iChCnt );  // EF -> circuit (gate) pass-through
      cmDspSysInstallCbN11N( h, ef,   "level", ipt,      "f-in", ctx->iChCnt );  // EF -> circuit (RMS)  pass-through

    }

  }

  p->omix = cmDspSysAllocInstArray(h,ctx->oChCnt,"AMix",NULL,NULL,1,p->circuitCnt);

   // check for allocation errors
  if(rc!=kOkDspRC || (rc = cmDspSysLastRC(h)) != kOkDspRC )
    goto errLabel;

  for(i=0; i<p->circuitCnt; ++i)
  {
    unsigned j;
    for(j=0; j<ctx->oChCnt; ++j)
      if( carr[i] != NULL && carr[i]->aout != NULL && carr[i]->aout[j] != NULL )
      {
        // circuit-> out fader
        cmDspSysConnectAudio( h, carr[i]->aout[j], carr[i]->aoutLbl, ofd[i], cmDspSysPrintLabel("in",j) ); 

        // out fader -> mixer
        cmDspSysConnectAudio( h, ofd[i], cmDspSysPrintLabel("out",j), p->omix[j], cmDspSysPrintLabel2("in",i) );
      }

  }

  cmDspSysConnectAudioN1N1( h, p->omix, "out", mxm, "in", ctx->oChCnt );  // mix -> meters

 errLabel:


  return rc;
}

cmDspRC_t _cmDspPP_CircuitSwitchFree(  cmDspSysH_t h, cmDspPP_CircuitSwitch_t* p)
{
  return kOkDspRC;
}
