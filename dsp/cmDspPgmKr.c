#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmText.h"
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
#include "cmDspSys.h"
#include "cmDspPgm.h"


#include "cmAudioFile.h"
#include "cmProcObj.h"
#include "cmProc.h"
#include "cmProc3.h"

#include "cmVectOpsTemplateMain.h"
#include "cmVectOps.h"

typedef struct
{
  const cmChar_t* tlFn;
  const cmChar_t* tlPrefixPath;
  const cmChar_t* scFn;
  const cmChar_t* modFn;
  const cmChar_t* measFn;
  const cmChar_t* recordDir;
  const cmChar_t* midiDevice;
  const cmChar_t* midiOutPort;
} krRsrc_t;

cmDspRC_t krLoadRsrc(cmDspSysH_t h, cmErr_t* err, krRsrc_t* r)
{
  cmDspRC_t rc;
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    return rc;

  cmDspRsrcString(h,&r->tlFn,        "timeLineFn",   NULL);
  cmDspRsrcString(h,&r->tlPrefixPath,"tlPrefixPath", NULL);
  cmDspRsrcString(h,&r->scFn,        "scoreFn",      NULL);
  cmDspRsrcString(h,&r->modFn,       "modFn",        NULL);
  cmDspRsrcString(h,&r->measFn,      "measFn",       NULL);
  cmDspRsrcString(h,&r->recordDir,   "recordDir",    NULL);
  cmDspRsrcString(h,&r->midiDevice,  "midiDevice",   NULL);
  cmDspRsrcString(h,&r->midiOutPort, "midiOutPort",  NULL);

  

  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    cmErrMsg(err,rc,"A KR DSP resource load failed.");
    
  return rc;
}

// connection information for 1 transform channel
typedef struct
{
  cmDspInst_t* achan; 
  cmDspInst_t* kr0;  // audio input
  cmDspInst_t* kr1;
  cmDspInst_t* cmp;  // audio output
} cmDspTlXform_t;

const cmChar_t* _mlbl(const cmChar_t* prefix, unsigned ch )
{
  static char s[128];
  s[127]=0;
  snprintf(s,127,"%s%i",prefix,ch);
  return s;
}

#define mlbl(a)  _mlbl(a,ch)
#define lbl(a) cmDspSysPrintLabel(a,ch)

void _cmDspSys_TlXformChain( cmDspSysH_t h, cmDspTlXform_t* c,  unsigned preGrpSymId, unsigned cmpPreGrpSymId, cmDspInst_t* modp, unsigned ch )
{
  unsigned        measRtrChCnt = 6; // note: router channel 6 is not connected

  int             krWndSmpCnt = 2048;
  int             krHopFact   = 4;

  unsigned        xfadeChCnt  = 2;
  double          xfadeMs     = 50;
  bool            xfadeInitFl = true;
  double          mixGain     = 1.0;

  bool            cmpBypassFl  = false;
  double          cmpInGain    = 3.0;
  double          cmpThreshDb  = -40.0;
  double          cmpRatio_num = 5.0;
  double          cmpAtkMs     = 20.0;
  double          cmpRlsMs     = 100.0;
  double          cmpMakeup    = 1.0;
  double          cmpWndMaxMs  = 1000.0;
  double          cmpWndMs     = 200.0;

  cmDspInst_t* achan = cmDspSysAllocInst(h, "AvailCh",     NULL, 1, xfadeChCnt );
  
  // Measurement scale/range 
  cmDspInst_t* even_sr  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.8,   1.1, 0.0,   1.0 );
  cmDspInst_t* dynm_sr  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0,   4.0, 0.01,  1.0 );
  cmDspInst_t* tmpo_sr  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4, 80.0, 120.0, 0.01,  1.0 );
  cmDspInst_t* cost_sr  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0,   1.0, 0.001, 1.0 );

  // Measurement -> parameter mappers
  cmDspInst_t* even_rt  = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtrChCnt, measRtrChCnt-1 );
  cmDspInst_t* dynm_rt  = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtrChCnt, measRtrChCnt-1 );
  cmDspInst_t* tmpo_rt  = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtrChCnt, measRtrChCnt-1 );
  cmDspInst_t* cost_rt  = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtrChCnt, measRtrChCnt-1 );

  // Scale/ranges applied to incoming measurements.
  cmDspInst_t* thr_sr   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, 0.01, 100.0 );
  cmDspInst_t* upr_sr   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, -1.0, 5.0 );
  cmDspInst_t* lwr_sr   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, -5.0, 5.0 );
  cmDspInst_t* off_sr   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0,  0.0, 100.0 );
  cmDspInst_t* wet_sr   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0,  0.0, 1.0 );

 
  // Parameter -> kr routers
  unsigned paramRtChCnt = 2;
  cmDspInst_t* mod_rt   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );
  cmDspInst_t* wnd_rt   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );
  cmDspInst_t* hop_rt   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );
  cmDspInst_t* thr_rt   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );
  cmDspInst_t* upr_rt   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );
  cmDspInst_t* lwr_rt   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );
  cmDspInst_t* inv_rt   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );
  cmDspInst_t* off_rt   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );
  cmDspInst_t* wet_rt   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );

  // Audio processors
  cmDspInst_t* kr0  = cmDspSysAllocInst(h, "Kr",         NULL,   2, krWndSmpCnt, krHopFact );
  cmDspInst_t* kr1  = cmDspSysAllocInst(h, "Kr",         NULL,   2, krWndSmpCnt, krHopFact );
  cmDspInst_t* xfad = cmDspSysAllocInst(h, "Xfader",     NULL,   3, xfadeChCnt,  xfadeMs, xfadeInitFl ); 
  cmDspInst_t* mix  = cmDspSysAllocInst(h, "AMix",       NULL,   3, xfadeChCnt,  mixGain, mixGain );
  cmDspInst_t* cmp  = cmDspSysAllocInst(h, "Compressor", NULL,   8, cmpBypassFl, cmpThreshDb, cmpRatio_num, cmpAtkMs, cmpRlsMs, cmpMakeup, cmpWndMs, cmpWndMaxMs ); 


  // Internal audio connections
  cmDspSysConnectAudio(h, kr0,  "out",   xfad, "in-0");
  cmDspSysConnectAudio(h, kr1,  "out",   xfad, "in-1");
  cmDspSysConnectAudio(h, xfad, "out-0", mix,  "in-0");
  cmDspSysConnectAudio(h, xfad, "out-1", mix,  "in-1");
  cmDspSysConnectAudio(h, mix,  "out",   cmp,  "in" );

  // active channel <-> cross-fade connections
  cmDspSysInstallCb(h, achan,  "reset",   xfad, "reset", NULL);
  cmDspSysInstallCb(h, achan,  "gate-0",  xfad, "gate-0", NULL );
  cmDspSysInstallCb(h, achan,  "gate-1",  xfad, "gate-1", NULL );
  cmDspSysInstallCb(h, xfad,   "state-0", achan, "dis-0",  NULL );
  cmDspSysInstallCb(h, xfad,   "state-1", achan, "dis-1",  NULL );

  
  //  Measurement Number Controls
  cmDspInst_t* min_dynm_ctl    = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Min In Dyn"),      0.0, 10.0, 1.0, 0.0);
  cmDspInst_t* max_dynm_ctl    = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Max In Dyn"),      0.0, 10.0, 1.0, 4.0);
  cmDspInst_t* dynm_map_menu   = cmDspSysAllocMsgListP(h,preGrpSymId, NULL, lbl("DynSel 0"), NULL, "measMenu", measRtrChCnt-1);

  cmDspInst_t* min_even_ctl   = cmDspSysAllocScalarP(  h,preGrpSymId, NULL, lbl("Min In Even"),    0.0, 1.0, 0.001, 0.75);
  cmDspInst_t* max_even_ctl   = cmDspSysAllocScalarP(  h,preGrpSymId, NULL, lbl("Max In Even"),    0.0, 3.0, 0.001, 1.0);
  cmDspInst_t* even_map_menu  = cmDspSysAllocMsgListP( h,preGrpSymId, NULL, lbl("EvenSel"), NULL, "measMenu", measRtrChCnt-1);

  cmDspSysNewColumn(h,0);
  cmDspInst_t* min_tmpo_ctl  = cmDspSysAllocScalarP(   h,preGrpSymId, NULL, lbl("Min In Tempo"),   0.0, 200.0, 1.0, 80.0);
  cmDspInst_t* max_tmpo_ctl  = cmDspSysAllocScalarP(   h,preGrpSymId, NULL, lbl("Max In Tempo"),   0.0, 200.0, 1.0, 120.0);
  cmDspInst_t* tmpo_map_menu = cmDspSysAllocMsgListP(  h,preGrpSymId, NULL, lbl("TempoSel"), NULL, "measMenu", measRtrChCnt-1);

  cmDspInst_t* min_cost_ctl   = cmDspSysAllocScalarP(  h,preGrpSymId, NULL, lbl("Min In Cost"),      0.0, 1.0, 0.01, 0.0);
  cmDspInst_t* max_cost_ctl   = cmDspSysAllocScalarP(  h,preGrpSymId, NULL, lbl("Max In Cost"),      0.0, 1.0, 0.01, 1.0);
  cmDspInst_t* cost_map_menu  = cmDspSysAllocMsgListP( h,preGrpSymId, NULL, lbl("CostSel"), NULL, "measMenu", measRtrChCnt-1);

  cmDspSysInstallCb(h, min_dynm_ctl, "val",     dynm_sr, "min_in", NULL );
  cmDspSysInstallCb(h, max_dynm_ctl, "val",     dynm_sr, "min_in", NULL );
  cmDspSysInstallCb(h, dynm_map_menu,"out",     dynm_rt, "sel",    NULL );   
  cmDspSysInstallCb(h, dynm_sr,      "val_out", dynm_rt, "f-in",   NULL );

  cmDspSysInstallCb(h, min_even_ctl, "val",     even_sr, "min_in", NULL );
  cmDspSysInstallCb(h, max_even_ctl, "val",     even_sr, "min_in", NULL );
  cmDspSysInstallCb(h, even_map_menu,"out",     even_rt, "sel",    NULL );   
  cmDspSysInstallCb(h, even_sr,      "val_out", even_rt, "f-in",   NULL );

  cmDspSysInstallCb(h, min_tmpo_ctl, "val",     tmpo_sr, "min_in", NULL );
  cmDspSysInstallCb(h, max_tmpo_ctl, "val",     tmpo_sr, "min_in", NULL );
  cmDspSysInstallCb(h, tmpo_map_menu,"out",     tmpo_rt, "sel",    NULL );   
  cmDspSysInstallCb(h, tmpo_sr,      "val_out", tmpo_rt, "f-in",   NULL );
  
  cmDspSysInstallCb(h, min_cost_ctl, "val",     cost_sr, "min_in", NULL );
  cmDspSysInstallCb(h, max_cost_ctl, "val",     cost_sr, "min_in", NULL );
  cmDspSysInstallCb(h, cost_map_menu,"out",     cost_rt, "sel",    NULL );   
  cmDspSysInstallCb(h, cost_sr,      "val_out", cost_rt, "f-in",   NULL );

  cmDspSysNewColumn(h,0);
  cmDspInst_t* min_thr_ctl   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Min Thresh"),       0.0,100.0, 1.0, 30.0);
  cmDspInst_t* max_thr_ctl   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Max Thresh"),       0.0,100.0, 1.0, 80.0);
  cmDspInst_t* min_upr_ctl   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Min Upr"),         -1.0,  1.0, 0.001, -0.5);
  cmDspInst_t* max_upr_ctl   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Max Upr"),         -1.0,  1.0, 0.001, 0.5);
  cmDspInst_t* min_lwr_ctl   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Min Lwr"),          0.0, -1.0, 5.0, 1.0);
  cmDspInst_t* max_lwr_ctl   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Max Lwr"),          0.0, -1.0, 5.0, 3.0);
  cmDspInst_t* min_off_ctl   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Min Off"),          0.0, 50.0, 0.1, 30.0);
  cmDspInst_t* max_off_ctl   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Max Off"),          0.0, 50.0, 0.1, 30.0);
  cmDspInst_t* min_wet_ctl   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Min Wet"),          0.0,  1.0, 0.01, 1.0);
  cmDspInst_t* max_wet_ctl   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Max Wet"),          0.0,  1.0, 0.01, 1.0);


  // Parameter number controls 
  cmDspSysNewColumn(h,0);
  cmDspInst_t* mod_ctl = cmDspSysAllocScalarP( h,preGrpSymId,NULL, lbl("Mode"),      0.0, 4.0, 1.0, 1.0);
  cmDspInst_t* wnd_ctl = cmDspSysAllocMsgListP(h,preGrpSymId,NULL, lbl("WndSmpCnt"), NULL, "wndSmpCnt", 2);
  cmDspInst_t* hop_ctl = cmDspSysAllocMsgListP(h,preGrpSymId,NULL, lbl("HopFact"),   NULL, "hopFact",   2);
  cmDspInst_t* thr_ctl = cmDspSysAllocScalarP( h,preGrpSymId,NULL, lbl("Threshold"), 0.0, 100.0, 1.0,  60.0 );
  cmDspInst_t* upr_ctl = cmDspSysAllocScalarP( h,preGrpSymId,NULL, lbl("Upr slope"), 0.0,  10.0, 0.01,  0.0 ); 
  cmDspInst_t* lwr_ctl = cmDspSysAllocScalarP( h,preGrpSymId,NULL, lbl("Lwr slope"), 0.3,  10.0, 0.01,  2.0 );
  cmDspInst_t* off_ctl = cmDspSysAllocScalarP( h,preGrpSymId,NULL, lbl("Offset"),    0.0, 100.0, 0.01, 20.0 );
  cmDspInst_t* inv_ctl = cmDspSysAllocScalarP( h,preGrpSymId,NULL, lbl("Invert"),    0.0,   1.0, 1.0,   0.0 );  
  cmDspInst_t* wet_ctl = cmDspSysAllocScalarP( h,preGrpSymId,NULL, lbl("Wet Dry"),   0.0,   1.0, 0.001, 1.0 );  

  cmDspSysInstallCb(h, mod_ctl, "val",         mod_rt, "f-in",    NULL );
  cmDspSysInstallCb(h, achan,   "ch",          mod_rt, "sel",     NULL );   // ach->rt sel
  cmDspSysInstallCb(h, mod_rt,  "f-out-0",     kr0,    "mode",    NULL );   // mode->kr
  cmDspSysInstallCb(h, mod_rt,  "f-out-1",     kr1,    "mode",    NULL );   // mode->kr

  cmDspSysInstallCb(h, wnd_ctl, "out",         wnd_rt, "f-in",    NULL );
  cmDspSysInstallCb(h, achan,   "ch",          wnd_rt, "sel",     NULL );   // ach->rt sel
  cmDspSysInstallCb(h, wnd_rt,  "f-out-0",     kr0,    "wndn",    NULL );   // wndn->kr
  cmDspSysInstallCb(h, wnd_rt,  "f-out-1",     kr1,    "wndn",    NULL );   // wndn->kr

  cmDspSysInstallCb(h, hop_ctl, "out",         hop_rt, "f-in",    NULL );
  cmDspSysInstallCb(h, achan,   "ch",          hop_rt, "sel",     NULL );   // ach->rt sel
  cmDspSysInstallCb(h, hop_rt,  "f-out-0",     kr0,    "hopf",    NULL );   // hopf->kr
  cmDspSysInstallCb(h, hop_rt,  "f-out-1",     kr1,    "hopf",    NULL );   // hopf->kr

  cmDspSysInstallCb(h, min_thr_ctl, "val",     thr_sr, "min_out", NULL );
  cmDspSysInstallCb(h, max_thr_ctl, "val",     thr_sr, "max_out", NULL );
  cmDspSysInstallCb(h, even_rt,     "f-out-0", thr_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, dynm_rt,     "f-out-0", thr_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, tmpo_rt,     "f-out-0", thr_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, cost_rt,     "f-out-0", thr_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, thr_sr,      "val_out", thr_ctl,"val",     NULL );
  cmDspSysInstallCb(h, thr_ctl,     "val",     thr_rt, "f-in",    NULL );
  cmDspSysInstallCb(h, achan,       "ch",      thr_rt, "sel",     NULL );   // ach->rt sel
  cmDspSysInstallCb(h, thr_rt,      "f-out-0", kr0,    "thrh",    NULL );   // thr->kr
  cmDspSysInstallCb(h, thr_rt,      "f-out-1", kr1,    "thrh",    NULL );   // thr->kr

  cmDspSysInstallCb(h, min_upr_ctl, "val",     upr_sr, "min_out", NULL );
  cmDspSysInstallCb(h, max_upr_ctl, "val",     upr_sr, "max_out", NULL );
  cmDspSysInstallCb(h, even_rt,     "f-out-1", upr_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, dynm_rt,     "f-out-1", upr_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, tmpo_rt,     "f-out-1", upr_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, cost_rt,     "f-out-1", upr_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, upr_sr,      "val_out", upr_ctl,"val",     NULL );
  cmDspSysInstallCb(h, upr_ctl,     "val",     upr_rt, "f-in",    NULL );
  cmDspSysInstallCb(h, achan,       "ch",      upr_rt, "sel",     NULL );   // ach->rt sel
  cmDspSysInstallCb(h, upr_rt,      "f-out-0", kr0,    "uprs",    NULL );   // upr->kr
  cmDspSysInstallCb(h, upr_rt,      "f-out-1", kr1,    "uprs",    NULL );   // upr->kr

  cmDspSysInstallCb(h, min_lwr_ctl, "val",     lwr_sr, "min_out", NULL );
  cmDspSysInstallCb(h, max_lwr_ctl, "val",     lwr_sr, "max_out", NULL );
  cmDspSysInstallCb(h, even_rt,     "f-out-2", lwr_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, dynm_rt,     "f-out-2", lwr_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, tmpo_rt,     "f-out-2", lwr_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, cost_rt,     "f-out-2", lwr_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, lwr_sr,      "val_out", lwr_ctl,"val",     NULL );
  cmDspSysInstallCb(h, lwr_ctl,     "val",     lwr_rt, "f-in",    NULL );
  cmDspSysInstallCb(h, achan,       "ch",      lwr_rt, "sel",     NULL );   // ach->rt sel
  cmDspSysInstallCb(h, lwr_rt,      "f-out-0", kr0,    "lwrs",    NULL );   // lwr->kr
  cmDspSysInstallCb(h, lwr_rt,      "f-out-1", kr1,    "lwrs",    NULL );   // lwr->kr

  cmDspSysInstallCb(h, min_off_ctl, "val",     off_sr, "min_out", NULL );
  cmDspSysInstallCb(h, max_off_ctl, "val",     off_sr, "max_out", NULL );
  cmDspSysInstallCb(h, even_rt,     "f-out-3", off_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, dynm_rt,     "f-out-3", off_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, tmpo_rt,     "f-out-3", off_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, cost_rt,     "f-out-3", off_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, off_sr,      "val_out", off_ctl,"val",     NULL );
  cmDspSysInstallCb(h, off_ctl,     "val",     off_rt, "f-in",    NULL );
  cmDspSysInstallCb(h, achan,       "ch",      off_rt, "sel",     NULL );   // ach->rt sel
  cmDspSysInstallCb(h, off_rt,      "f-out-0", kr0,    "offs",    NULL );   // off->kr
  cmDspSysInstallCb(h, off_rt,      "f-out-1", kr1,    "offs",    NULL );   // off->kr

  cmDspSysInstallCb(h, inv_ctl,     "val",     inv_rt, "f-in",   NULL );
  cmDspSysInstallCb(h, achan,       "ch",      inv_rt, "sel",    NULL );   // ach->rt sel
  cmDspSysInstallCb(h, inv_rt,      "f-out-0", kr0,    "invt",   NULL );   // inv->kr
  cmDspSysInstallCb(h, inv_rt,      "f-out-1", kr1,    "invt",   NULL );   // inv->kr

  cmDspSysInstallCb(h, min_wet_ctl, "val",     wet_sr, "min_out", NULL );
  cmDspSysInstallCb(h, max_wet_ctl, "val",     wet_sr, "max_out", NULL );
  cmDspSysInstallCb(h, even_rt,     "f-out-4", wet_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, dynm_rt,     "f-out-4", wet_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, tmpo_rt,     "f-out-4", wet_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, cost_rt,     "f-out-4", wet_sr, "val_in",  NULL );

  cmDspSysInstallCb(h, wet_sr,      "val_out", wet_ctl,"val",     NULL );
  cmDspSysInstallCb(h, wet_ctl,     "val",     wet_rt, "f-in",    NULL );
  cmDspSysInstallCb(h, achan,       "ch",      wet_rt, "sel",     NULL );   // ach->rt sel
  cmDspSysInstallCb(h, wet_rt,      "f-out-0", kr0,    "wet",     NULL );   // wet->kr
  cmDspSysInstallCb(h, wet_rt,      "f-out-1", kr1,    "wet",     NULL );   // wet->kr
  

  cmDspSysNewColumn(h,0);
  cmDspInst_t* cmp_byp   = cmDspSysAllocCheckP(  h, cmpPreGrpSymId, NULL, lbl("Bypass"), 1.0 );
  cmDspInst_t* cmp_igain = cmDspSysAllocScalarP( h, cmpPreGrpSymId, NULL, lbl("In Gain"),  0.0,   10.0, 0.1, cmpInGain);
  cmDspInst_t* cmp_thr   = cmDspSysAllocScalarP( h, cmpPreGrpSymId, NULL, lbl("ThreshDb"), -100.0, 0.0, 0.1, cmpThreshDb);
  cmDspInst_t* cmp_rat   = cmDspSysAllocScalarP( h, cmpPreGrpSymId, NULL, lbl("Ratio"),    0.1, 100, 0.1, cmpRatio_num);
  cmDspInst_t* cmp_atk   = cmDspSysAllocScalarP( h, cmpPreGrpSymId, NULL, lbl("Atk Ms"),   0.0, 1000.0, 0.1, cmpAtkMs);
  cmDspInst_t* cmp_rls   = cmDspSysAllocScalarP( h, cmpPreGrpSymId, NULL, lbl("Rls Ms"),   0.0, 1000.0, 0.1, cmpRlsMs);
  cmDspInst_t* cmp_mkup  = cmDspSysAllocScalarP( h, cmpPreGrpSymId, NULL, lbl("Makeup"),   0.0, 10.0,   0.01, cmpMakeup);
  cmDspInst_t* cmp_wnd   = cmDspSysAllocScalarP( h, cmpPreGrpSymId, NULL, lbl("Wnd Ms"),   1.0, cmpWndMaxMs, 1.0, cmpWndMs );
  cmDspInst_t* cmp_mtr   = cmDspSysAllocInst(h,"Meter",lbl("Env"), 3, 0.0, 0.0, 1.0);

  cmDspSysInstallCb(h, cmp_byp,  "out", cmp, "bypass", NULL );
  cmDspSysInstallCb(h, cmp_igain,"val", cmp, "igain", NULL );
  cmDspSysInstallCb(h, cmp_thr,  "val", cmp, "thr", NULL );
  cmDspSysInstallCb(h, cmp_rat,  "val", cmp, "ratio", NULL );
  cmDspSysInstallCb(h, cmp_atk,  "val", cmp, "atk", NULL );
  cmDspSysInstallCb(h, cmp_rls,  "val", cmp, "rls", NULL );
  cmDspSysInstallCb(h, cmp_mkup, "val", cmp, "ogain", NULL );
  cmDspSysInstallCb(h, cmp_wnd,  "val", cmp, "wnd", NULL );
  cmDspSysInstallCb(h, cmp,      "env", cmp_mtr, "in", NULL );

  // 
  cmDspInst_t* xfadMs = cmDspSysAllocInst(h,"Scalar", lbl("Xfade Ms"),     5, kNumberDuiId, 0.0,   1000.0,0.01, 50.0 );  
  cmDspSysInstallCb(h, xfadMs, "val", xfad, "ms", NULL );

  cmDspSysInstallCb(h, modp, mlbl("mod"),  mod_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("thr"),  thr_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("upr"),  upr_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("lwr"),  lwr_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("mint"), min_thr_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("maxt"), max_thr_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("minu"), min_upr_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("maxu"), max_upr_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("minl"), min_lwr_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("maxl"), max_lwr_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("sw"),   achan,       "trig", NULL ); // See also: amp.sfloc->achan.trig

  c->achan = achan; 
  c->kr0   = kr0; 
  c->kr1   = kr1;
  c->cmp   = cmp; 

}

cmDspRC_t _cmDspSysPgm_TimeLine(cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t       rc         = kOkDspRC;
  cmCtx_t*        cmCtx      = cmDspSysPgmCtx(h);
  cmErr_t         err;
  krRsrc_t        r;
  unsigned        wtLoopCnt  = 1;                            // 1=play once (-1=loop forever)
  unsigned        wtInitMode = 0;                            // initial wt mode is 'silence'
  unsigned        wtSmpCnt   = floor(cmDspSysSampleRate(h)); // wt length == srate

  unsigned        sfBufCnt    = 7;     // length of the MIDI event buffer
  unsigned        sfMaxWndCnt = 10;    // length of the score event buffer
  unsigned        sfMinVel    = 5;     // ignore MIDI events below this velocity
  bool            sfEnaMeasFl = false;
  double          recdPlayInitAllocSecs    = 10.0;
  double          recdPlayMaxLaSecs        = 2.0;
  double          recdPlayCurLaSecs        = 0.1;
  double          recdPlayFadeRateDbPerSec = 4.0;

  memset(&r,0,sizeof(r));
  cmErrSetup(&err,&cmCtx->rpt,"Kr Timeline");

  if( krLoadRsrc(h,&err,&r) != kOkDspRC )
    return rc;


  cmDspInst_t* ai0p = cmDspSysAllocInst(h,"AudioIn",     NULL,  1, 0);
  cmDspInst_t* ai1p = cmDspSysAllocInst(h,"AudioIn",     NULL,  1, 1);

  cmDspInst_t* tlp  = cmDspSysAllocInst(h,"TimeLine",    "tl",  2, r.tlFn, r.tlPrefixPath );
  cmDspInst_t* scp  = cmDspSysAllocInst(h,"Score",       "sc",  1, r.scFn );
  cmDspInst_t* php  = cmDspSysAllocInst(h,"Phasor",      NULL,  1, cmDspSysSampleRate(h) );
  cmDspInst_t* wtp  = cmDspSysAllocInst(h,"WaveTable",   NULL,  4, wtSmpCnt, wtInitMode, NULL, wtLoopCnt );
  cmDspInst_t* pts  = cmDspSysAllocInst(h,"PortToSym",   NULL,  2, "on", "off" );
  cmDspInst_t* mip  = cmDspSysAllocInst(h,"MidiIn",      NULL,  0 );
  cmDspInst_t* mfp  = cmDspSysAllocInst(h,"MidiFilePlay",NULL,  0 );
  cmDspInst_t* nmp  = cmDspSysAllocInst(h,"NanoMap",     NULL,  0 );
  cmDspInst_t* mop  = cmDspSysAllocInst(h,"MidiOut",     NULL,  2, r.midiDevice,r.midiOutPort);
  cmDspInst_t* sfp  = cmDspSysAllocInst(h,"ScFol",       NULL,  1, r.scFn, sfBufCnt, sfMaxWndCnt, sfMinVel, sfEnaMeasFl );
  cmDspInst_t* amp  = cmDspSysAllocInst(h,"ActiveMeas",  NULL,  1, 100 );
  cmDspInst_t* rpp  = cmDspSysAllocInst(h,"RecdPlay",    NULL,  6, 2, r.scFn, recdPlayInitAllocSecs, recdPlayMaxLaSecs, recdPlayCurLaSecs, recdPlayFadeRateDbPerSec );
  cmDspInst_t* modp = cmDspSysAllocInst(h,"ScMod",       NULL,  2, r.modFn, "m1" );
 
  unsigned   preGrpSymId     = cmDspSysPresetRegisterGroup(h,"tl");
  unsigned   cmpPreGrpSymId  = cmDspSysPresetRegisterGroup(h,"tl_cmp"); 

  cmDspTlXform_t c0,c1;

  cmDspSysNewPage(h,"Controls-0");
  _cmDspSys_TlXformChain(h, &c0, preGrpSymId, cmpPreGrpSymId, modp, 0 );
  cmDspInst_t* mix0 = cmDspSysAllocInst(h,"AMix",        NULL,   3, 2, 1.0, 1.0 );
  cmDspInst_t* ao0p = cmDspSysAllocInst(h,"AudioOut",    NULL,   1, 0 );

  cmDspSysNewPage(h,"Controls-1");
  _cmDspSys_TlXformChain(h, &c1, preGrpSymId, cmpPreGrpSymId, modp, 1 );
  cmDspInst_t* mix1 = cmDspSysAllocInst(h,"AMix",        NULL,   3, 2, 1.0, 1.0 );
  cmDspInst_t* ao1p = cmDspSysAllocInst(h,"AudioOut",    NULL,   1, 1 );

  cmDspInst_t* ao2p = cmDspSysAllocInst(h,"AudioOut",    NULL,   1, 2 );
  cmDspInst_t* ao3p = cmDspSysAllocInst(h,"AudioOut",    NULL,   1, 3 );

  cmDspSysNewPage(h,"Main");
  cmDspInst_t* liveb= cmDspSysAllocInst(h,"Button", "live",    2, kCheckDuiId,  0.0 );
  cmDspInst_t* simb = cmDspSysAllocInst(h,"Button", "simulate",2, kCheckDuiId,  0.0 );
  cmDspInst_t* ainb = cmDspSysAllocInst(h,"Button", "audio in",2, kCheckDuiId,  0.0 );
  cmDspInst_t* measb= cmDspSysAllocInst(h,"Button", "meas",    2, kCheckDuiId,  0.0 );
  cmDspInst_t* onb  = cmDspSysAllocInst(h,"Button", "start",   2, kButtonDuiId, 1.0 );
  cmDspInst_t* offb = cmDspSysAllocInst(h,"Button", "stop",    2, kButtonDuiId, 1.0 );
  cmDspInst_t* prtb = cmDspSysAllocInst(h,"Button", "print",   2, kButtonDuiId, 1.0 );
  cmDspInst_t* qtb  = cmDspSysAllocInst(h,"Button", "quiet",   2, kButtonDuiId, 1.0 );
  cmDspInst_t* prp  = cmDspSysAllocInst(h,"Printer", NULL,   1, ">" );
  cmDspInst_t* prd  = cmDspSysAllocInst(h,"Printer", NULL,   1, "DYN:" );
  cmDspInst_t* pre  = cmDspSysAllocInst(h,"Printer", NULL,   1, "EVEN:" );
  cmDspInst_t* prt  = cmDspSysAllocInst(h,"Printer", NULL,   1, "TEMPO:");
  cmDspInst_t* prc  = cmDspSysAllocInst(h,"Printer", NULL,   1, "COST:");
  //cmDspInst_t* prv  = cmDspSysAllocInst(h,"Printer", NULL,   1, "Value:");

  // Record <-> Live switches
  cmDspInst_t* tlRt  = cmDspSysAllocInst(h,"Router", NULL, 2, 2, 0);  // time line swich
  cmDspInst_t* wtRt  = cmDspSysAllocInst(h,"Router", NULL, 2, 2, 0);
  cmDspInst_t* mfpRt = cmDspSysAllocInst(h,"Router", NULL, 2, 2, 0);
  cmDspInst_t* amRt  = cmDspSysAllocInst(h,"Router", NULL, 2, 2, 0);
  cmDspInst_t* au0Sw = cmDspSysAllocInst(h,"1ofN",   NULL, 2, 2, 0);
  cmDspInst_t* au1Sw = cmDspSysAllocInst(h,"1ofN",   NULL, 2, 2, 0);

  cmDspInst_t* siRt  = cmDspSysAllocInst(h,"Router", NULL, 2, 2, 0);  
  cmDspInst_t* d0Rt  = cmDspSysAllocInst(h,"Router", NULL, 2, 2, 0);
  cmDspInst_t* d1Rt  = cmDspSysAllocInst(h,"Router", NULL, 2, 2, 0);
  cmDspInst_t* stRt  = cmDspSysAllocInst(h,"Router", NULL, 2, 2, 0);

  cmDspSysNewColumn(h,0);
  cmDspInst_t* igain0 = cmDspSysAllocInst(h,"Scalar", "In Gain-0",    5, kNumberDuiId, 0.0,   10.0,0.01,   1.0 );  
  cmDspInst_t* igain1 = cmDspSysAllocInst(h,"Scalar", "In Gain-1",    5, kNumberDuiId, 0.0,   10.0,0.01,   1.0 );  

  cmDspInst_t* lasecs = cmDspSysAllocInst(h,"Scalar", "LA Secs",      5, kNumberDuiId, 0.0,   recdPlayMaxLaSecs,0.01,   recdPlayCurLaSecs );  
  cmDspInst_t* dbpsec = cmDspSysAllocInst(h,"Scalar", "Fade dBpSec",  5, kNumberDuiId, 0.0,   24.0, 0.01, recdPlayFadeRateDbPerSec);
  cmDspInst_t* ogain0 = cmDspSysAllocInst(h,"Scalar", "Out Gain-0",   5, kNumberDuiId, 0.0,   10.0,0.01,   3.0 );  
  cmDspInst_t* ogain1 = cmDspSysAllocInst(h,"Scalar", "Out Gain-1",   5, kNumberDuiId, 0.0,   10.0,0.01,   3.0 );  
  cmDspInst_t* ogain2 = cmDspSysAllocInst(h,"Scalar", "Out Gain-2",   5, kNumberDuiId, 0.0,   10.0,0.01,   3.0 );  
  cmDspInst_t* ogain3 = cmDspSysAllocInst(h,"Scalar", "Out Gain-3",   5, kNumberDuiId, 0.0,   10.0,0.01,   3.0 );  

  // Audio file recording
  cmDspInst_t* recdGain= cmDspSysAllocInst(h,"Scalar", "Recd Gain",  5, kNumberDuiId, 0.0,   100.0,0.01, 1.5 );  
  cmDspInst_t* recdChk = cmDspSysAllocInst(h,"Button", "Record",     2, kCheckDuiId, 0.0 );
  cmDspInst_t* recdPtS = cmDspSysAllocInst(h,"GateToSym", NULL,      2, cmSymTblRegisterStaticSymbol(cmDspSysSymbolTable(h),"open"),cmSymTblRegisterStaticSymbol(cmDspSysSymbolTable(h),"close"));
  cmDspInst_t* afop    = cmDspSysAllocInst(h,"AudioFileOut",NULL,    2, r.recordDir,2);
  cmDspInst_t* mi0p    = cmDspSysAllocInst(h,"AMeter","In 0",  0);
  cmDspInst_t* mi1p    = cmDspSysAllocInst(h,"AMeter","In 1",  0);

  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    return rc;

  // Output Audio file recording.
  cmDspSysInstallCb(h, recdGain,"val", afop,    "gain0", NULL );
  cmDspSysInstallCb(h, recdGain,"val", afop,    "gain1", NULL );
  cmDspSysInstallCb(h, recdChk, "out", recdPtS, "on",    NULL );
  cmDspSysInstallCb(h, recdChk, "out", recdPtS, "off",   NULL );
  cmDspSysInstallCb(h, recdPtS, "out", afop,    "sel",   NULL );


  // Audio connections
  cmDspSysConnectAudio(h, php,  "out",   wtp,  "phs" );     // phs -> wt

  cmDspSysConnectAudio(h, wtp,    "out",   au0Sw, "a-in-0" ); // wt  -> sw

  cmDspSysConnectAudio(h, ai0p,   "out",   au0Sw, "a-in-1" ); // ain -> sw
  cmDspSysConnectAudio(h, ai0p,   "out",   mi0p,  "in" );     
  cmDspSysConnectAudio(h, au0Sw,  "a-out", rpp,   "in-0");    // sw  -> rcdply
  cmDspSysConnectAudio(h, au0Sw,  "a-out", c0.kr0,"in"  );    // sw  -> kr
  cmDspSysConnectAudio(h, au0Sw,  "a-out", c0.kr1,"in"  );    // sw  -> kr
  cmDspSysConnectAudio(h, c0.cmp, "out",   mix0,  "in-0" );   // cmp -> mix 0
  cmDspSysConnectAudio(h, rpp,    "out-0", mix0,  "in-1");    // rpp -> mix 1
  cmDspSysConnectAudio(h, mix0,   "out",   ao0p,  "in" );     // mix -> aout


  cmDspSysConnectAudio(h, wtp,    "out",   au1Sw, "a-in-0" ); // wt  -> sw
  cmDspSysConnectAudio(h, ai1p,   "out",   au1Sw, "a-in-1" ); // ain -> sw
  cmDspSysConnectAudio(h, ai1p,   "out",   mi1p,  "in" );
  cmDspSysConnectAudio(h, au1Sw,  "a-out", rpp,   "in-1");    // sw  -> rcdply
  cmDspSysConnectAudio(h, au1Sw,  "a-out", c1.kr0,"in"  );    // sw  -> kr
  cmDspSysConnectAudio(h, au1Sw,  "a-out", c1.kr1,"in"  );    // sw  -> kr
  cmDspSysConnectAudio(h, c1.cmp, "out",   mix1,  "in-0" );   // cmp -> mix 0
  cmDspSysConnectAudio(h, rpp,    "out-1", mix1,  "in-1");    // rpp -> mix 1
  cmDspSysConnectAudio(h, mix1,   "out",   ao1p,  "in" );     // mix -> aout

  cmDspSysConnectAudio(h, c0.cmp, "out", afop, "in0" );    // comp -> audio_file_out
  cmDspSysConnectAudio(h, c1.cmp, "out", afop, "in1" );

  cmDspSysConnectAudio(h, ai0p, "out", ao2p, "in" );
  cmDspSysConnectAudio(h, ai1p, "out", ao3p, "in" );


  //--------------- Preset controls
  cmDspSysNewColumn(h,0);
  cmDspInst_t* preset    = cmDspSysAllocInst(   h, "Preset", NULL, 1, preGrpSymId );
  cmDspInst_t* presetLbl = cmDspSysAllocInst(   h, "Text",   "Preset",      1, "" );
  cmDspInst_t* storeBtn  = cmDspSysAllocButton( h, "store",  0);
  cmDspInst_t* recallBtn = cmDspSysAllocButton( h, "recall", 0);
  cmDspSysInstallCb(   h, presetLbl, "val", preset, "label",NULL);
  cmDspSysInstallCb(   h, storeBtn,  "sym", preset, "cmd", NULL );
  cmDspSysInstallCb(   h, recallBtn, "sym", preset, "cmd", NULL );

  cmDspInst_t* prePath    = cmDspSysAllocInst(   h, "Fname",  "prePath",   3, true,NULL,r.tlPrefixPath);
  

  cmDspSysNewColumn(h,0);

  //--------------- Recorded performance evaluation and Active Measurement related controls
  cmDspInst_t* clrBtn  = cmDspSysAllocButton( h, "clear",  0);
  cmDspInst_t* prtBtn  = cmDspSysAllocButton( h, "dump",  0);
  cmDspInst_t* mlst    = cmDspSysAllocInst(   h, "MsgList",   NULL, 3, "meas", r.measFn, 2);
  cmDspInst_t* amCmd   = cmDspSysAllocInst(   h, "PortToSym", NULL, 2, "add", "rewind" );  



  cmDspSysInstallCb( h, clrBtn, "sym",    amp, "cmd",   NULL ); // clear active meas.
  cmDspSysInstallCb( h, prtBtn, "sym",    amp, "cmd",   NULL ); // print active meas
  cmDspSysInstallCb( h, amCmd, "add",     amp, "cmd",   NULL ); // add active meas
  cmDspSysInstallCb( h, amCmd, "rewind",  amp, "cmd",   NULL ); // rewind active meas
  cmDspSysInstallCb( h, mlst,   "loc",    amp, "loc",   NULL ); // recorded meas's list to active meas unit
  cmDspSysInstallCb( h, mlst,   "typeId", amp, "type",  NULL ); //
  cmDspSysInstallCb( h, mlst,   "val",    amp, "val",   NULL ); //
  cmDspSysInstallCb( h, mlst,   "cost",   amp, "cst",   NULL ); // 
  cmDspSysInstallCb( h, mlst,   "typeId", amCmd, "add", NULL ); //
  cmDspSysInstallCb( h, sfp,    "out",    amRt, "f-in", NULL ); // sfp-active meas router (rtr is switched by live btn)
  cmDspSysInstallCb( h, amRt,   "f-out-0",amp, "sfloc", NULL ); 

  cmDspSysInstallCb( h, sfp, "vloc", amp,   "loc",  NULL ); // live meas's to active meas unit
  cmDspSysInstallCb( h, sfp, "vval", amp,   "val",  NULL ); //
  cmDspSysInstallCb( h, sfp, "vcost",amp,   "cst",  NULL ); //
  cmDspSysInstallCb( h, sfp, "vtyp", amp,   "type", NULL ); //
  cmDspSysInstallCb( h, sfp, "vtyp", amCmd, "add",  NULL);  //

  // ***** delete this to prevent the score follower from driving the active-measure unit in 'live' mode
  cmDspSysInstallCb( h, amRt,   "f-out-1",amp, "sfloc", NULL );
  // *****

  // active measure loc to xfad channel trigger
  cmDspSysInstallCb( h, amp,    "scloc",  c0.achan,   "trig", NULL );  // See Also: modp.sw ->achan.trig
  cmDspSysInstallCb( h, amp,    "scloc",  c1.achan,   "trig", NULL );
  cmDspSysInstallCb( h, amp,    "even",   pre,        "in",   NULL );  // active meas output to printers
  cmDspSysInstallCb( h, amp,    "dyn",    prd,        "in",   NULL );
  cmDspSysInstallCb( h, amp,    "tempo",  prt,        "in",   NULL );
  cmDspSysInstallCb( h, amp,    "cost",   prc,        "in",   NULL );

  // wave-table to time-line cursor
  cmDspSysInstallCb(   h, wtp, "fidx",tlp,  "curs", NULL); 

  cmDspSysInstallCb(h, prePath, "out", tlp, "path", NULL );

  // 'live' button -> live router selector switch 
  cmDspSysInstallCb(h, liveb, "out",  wtRt, "sel", NULL );
  cmDspSysInstallCb(h, liveb, "out",  tlRt, "sel", NULL );
  cmDspSysInstallCb(h, liveb, "out",  mfpRt,"sel", NULL );
  cmDspSysInstallCb(h, liveb, "out",  amRt, "sel", NULL );
  cmDspSysInstallCb(h, liveb, "out",  au0Sw, "chidx", NULL );
  cmDspSysInstallCb(h, liveb, "out",  au1Sw, "chidx", NULL );
  cmDspSysInstallCb(h, liveb, "out",  measb, "in",     NULL );
  cmDspSysInstallCb(h, measb, "out",  sfp,   "measfl", NULL );

  // 'simulate' button -> simulate router selector switch
  cmDspSysInstallCb(h, simb,  "out",  ainb,  "in", NULL );
  cmDspSysInstallCb(h, ainb,  "out",  au0Sw, "chidx", NULL );
  cmDspSysInstallCb(h, ainb,  "out",  au1Sw, "chidx", NULL );
  cmDspSysInstallCb(h, simb,  "out",  siRt,  "sel", NULL );
  cmDspSysInstallCb(h, simb,  "out",  d1Rt,  "sel", NULL );
  cmDspSysInstallCb(h, simb,  "out",  d0Rt,  "sel", NULL );
  cmDspSysInstallCb(h, simb,  "out",  stRt,  "sel", NULL );
  
  
  // start connections
  cmDspSysInstallCb(h, onb,  "sym",    tlRt, "s-in",  NULL );
  cmDspSysInstallCb(h, tlRt, "s-out-0",tlp,  "reset", NULL );
  cmDspSysInstallCb(h, onb,  "sym",    scp,  "send",  NULL );
  cmDspSysInstallCb(h, onb,  "sym",    mfpRt,"s-in",  NULL );
  cmDspSysInstallCb(h, mfpRt,"s-out-0",mfp,  "sel",   NULL );

  cmDspSysInstallCb(h, onb, "sym",     pts,   "on",    NULL );
  cmDspSysInstallCb(h, pts, "on",      wtRt,  "s-in",  NULL );
  cmDspSysInstallCb(h, wtRt,"s-out-0", wtp,   "cmd",   NULL );
  cmDspSysInstallCb(h, pts, "on",      modp,  "cmd",   NULL );
  cmDspSysInstallCb(h, pts, "on",      rpp,   "cmd",   NULL );
  cmDspSysInstallCb(h, onb, "sym",     amCmd, "rewind",NULL );
  cmDspSysInstallCb(h, onb, "out",     c0.achan,"reset",  NULL );
  cmDspSysInstallCb(h, onb, "out",     c1.achan,"reset",  NULL );

  // stop connections
  cmDspSysInstallCb(h, wtp,  "done",offb,"in",  NULL ); // 'done' from WT simulates pressing Stop btn.
  cmDspSysInstallCb(h, tlp,  "mfn", pts, "off", NULL ); // Prevents WT start on new audio file from TL.
  cmDspSysInstallCb(h, offb, "sym", mfp, "sel", NULL ); 
  cmDspSysInstallCb(h, offb, "sym", pts, "off", NULL );
  cmDspSysInstallCb(h, pts,  "off", wtp, "cmd", NULL );
  cmDspSysInstallCb(h, pts,  "off", modp,"cmd", NULL );
  cmDspSysInstallCb(h, offb, "sym", mop, "reset", NULL );

  // time-line to wave-table selection 
  cmDspSysInstallCb(h, tlp, "absi", wtp, "beg", NULL );  
  cmDspSysInstallCb(h, tlp, "aesi", wtp, "end", NULL );
  cmDspSysInstallCb(h, tlp, "afn",  wtp, "fn",  NULL );

  // time-line to MIDI file player selection
  cmDspSysInstallCb(h, tlp, "mbsi", mfp, "bsi",   NULL );
  cmDspSysInstallCb(h, tlp, "mesi", mfp, "esi",   NULL );
  cmDspSysInstallCb(h, tlp, "mfn",  mfp, "fn",    NULL );
  
  // score to score follower - to set initial search location
  cmDspSysInstallCb(h, scp, "sel",    sfp, "index",  NULL );
  cmDspSysInstallCb(h, scp, "sel",    modp,"reset", NULL );
  
  //cmDspSysInstallCb(h, reload,"out",  modp, "reload", NULL );


  // MIDI file player to score follower
  cmDspSysInstallCb(h, mfp,  "smpidx",  siRt, "f-in",NULL );
  cmDspSysInstallCb(h, siRt, "f-out-0", sfp,  "smpidx",NULL ); 
  // leave siRt.f-out-1 unconnected because it should be ignored in 'simulate mode'

  cmDspSysInstallCb(h, mfp,  "d1",      d1Rt, "f-in",  NULL );
  cmDspSysInstallCb(h, d1Rt, "f-out-0", sfp,  "d1",    NULL );
  cmDspSysInstallCb(h, d1Rt, "f-out-1", nmp,  "d1",    NULL );
  cmDspSysInstallCb(h, nmp,   "d1",     mop,  "d1",    NULL );

  cmDspSysInstallCb(h, mfp,  "d0",      d0Rt,  "f-in", NULL );
  cmDspSysInstallCb(h, d0Rt, "f-out-0", sfp,   "d0",   NULL );
  cmDspSysInstallCb(h, d0Rt, "f-out-1", nmp,  "d0",   NULL );
  cmDspSysInstallCb(h, nmp,  "d0",      mop,  "d0",   NULL );

  cmDspSysInstallCb(h, mfp, "status",   stRt, "f-in",  NULL );
  cmDspSysInstallCb(h, stRt, "f-out-0", sfp,  "status",NULL );
  cmDspSysInstallCb(h, stRt, "f-out-1", nmp,  "status",NULL );
  cmDspSysInstallCb(h, nmp,  "status",  mop,  "status",NULL );

  // MIDI input port
  cmDspSysInstallCb(h, mip, "smpidx", sfp, "smpidx", NULL );
  cmDspSysInstallCb(h, mip, "d1",     sfp, "d1",     NULL );
  cmDspSysInstallCb(h, mip, "d0",     sfp, "d0",     NULL );
  cmDspSysInstallCb(h, mip, "status", sfp, "status", NULL );

  // score follower to recd_play,modulator and printers
  cmDspSysInstallCb(h, sfp, "out",     rpp,     "index", NULL );
  cmDspSysInstallCb(h, sfp, "out",     modp,    "index", NULL );
  cmDspSysInstallCb(h, sfp, "recent",  prp,     "in",  NULL );  // report 'recent' but only act on 'max' loc index

  cmDspSysInstallCb(h, prtb, "sym", sfp, "cmd", NULL );
  cmDspSysInstallCb(h, qtb,  "sym", sfp, "cmd", NULL );


  cmDspSysInstallCb(   h, lasecs, "val", rpp, "curla", NULL ); // recd/play control
  cmDspSysInstallCb(   h, dbpsec, "val", rpp, "frate", NULL );

  cmDspSysInstallCb(h, igain0, "val", ai0p, "gain", NULL );   // input gain control
  cmDspSysInstallCb(h, igain1, "val", ai1p, "gain", NULL );
  cmDspSysInstallCb(h, ogain0, "val", ao0p, "gain", NULL );   // output gain control
  cmDspSysInstallCb(h, ogain1, "val", ao1p, "gain", NULL );
  cmDspSysInstallCb(h, ogain2, "val", ao2p, "gain", NULL );
  cmDspSysInstallCb(h, ogain3, "val", ao3p, "gain", NULL );

  return rc;
}

cmDspRC_t _cmDspSysPgm_TimeLine2(cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t       rc         = kOkDspRC;
  cmCtx_t*        cmCtx      = cmDspSysPgmCtx(h);
  cmErr_t         err;
  krRsrc_t        r;
  unsigned        wtLoopCnt  = 1;                            // 1=play once (-1=loop forever)
  unsigned        wtInitMode = 0;                            // initial wt mode is 'silence'
  unsigned        wtSmpCnt   = floor(cmDspSysSampleRate(h)); // wt length == srate
  int             krWndSmpCnt = 2048;
  int             krHopFact   = 4;

  unsigned        sfBufCnt    = 7;     // length of the MIDI event buffer
  unsigned        sfMaxWndCnt = 10;    // length of the score event buffer
  unsigned        sfMinVel    = 5;     // ignore MIDI events below this velocity
  bool            sfEnaMeasFl = false;

  unsigned        xfadeChCnt  = 2;
  double          xfadeMs     = 50;
  bool            xfadeInitFl = true;
  double          mixGain     = 1.0;

  unsigned        measRtrChCnt = 4; // note: router channel 4 is not connected

  bool            cmpBypassFl  = false;
  double          cmpInGain    = 3.0;
  double          cmpThreshDb  = -40.0;
  double          cmpRatio_num = 5.0;
  double          cmpAtkMs     = 20.0;
  double          cmpRlsMs     = 100.0;
  double          cmpMakeup    = 1.0;
  double          cmpWndMaxMs  = 1000.0;
  double          cmpWndMs     = 200.0;

  bool            splitFragFl = true;  // send fragments to separate audio outputs
  double          recdPlayInitAllocSecs    = 10.0;
  double          recdPlayMaxLaSecs        = 2.0;
  double          recdPlayCurLaSecs        = 0.1;
  double          recdPlayFadeRateDbPerSec = 4.0;
  double          fragMixGain              = splitFragFl ? 0.0 : mixGain;


  memset(&r,0,sizeof(r));
  cmErrSetup(&err,&cmCtx->rpt,"Kr Timeline");

  if( krLoadRsrc(h,&err,&r) != kOkDspRC )
    return rc;

  unsigned   preGrpSymId     = cmDspSysPresetRegisterGroup(h,"tl");
  unsigned   compPreGrpSymId = cmDspSysPresetRegisterGroup(h,"tl_cmp"); 

  cmDspInst_t* ai0p = cmDspSysAllocInst(h,"AudioIn",     NULL,  1, 0);
  cmDspInst_t* ai1p = cmDspSysAllocInst(h,"AudioIn",     NULL,  1, 1);

  cmDspInst_t* tlp  = cmDspSysAllocInst(h,"TimeLine",    "tl",  2, r.tlFn, r.tlPrefixPath );
  cmDspInst_t* scp  = cmDspSysAllocInst(h,"Score",       "sc",  1, r.scFn );
  cmDspInst_t* php  = cmDspSysAllocInst(h,"Phasor",      NULL,  1, cmDspSysSampleRate(h) );
  cmDspInst_t* wtp  = cmDspSysAllocInst(h,"WaveTable",   NULL,  4, wtSmpCnt, wtInitMode, NULL, wtLoopCnt );
  cmDspInst_t* pts  = cmDspSysAllocInst(h,"PortToSym",   NULL,  2, "on", "off" );
  cmDspInst_t* mip  = cmDspSysAllocInst(h,"MidiIn",      NULL,  0 );
  cmDspInst_t* mfp  = cmDspSysAllocInst(h,"MidiFilePlay",NULL,  0 );
  cmDspInst_t* nmp  = cmDspSysAllocInst(h,"NanoMap",     NULL,  0 );
  cmDspInst_t* mop  = cmDspSysAllocInst(h,"MidiOut",     NULL,  2, r.midiDevice,r.midiOutPort);
  cmDspInst_t* sfp  = cmDspSysAllocInst(h,"ScFol",       NULL,  1, r.scFn, sfBufCnt, sfMaxWndCnt, sfMinVel, sfEnaMeasFl );
  cmDspInst_t* amp  = cmDspSysAllocInst(h,"ActiveMeas",  NULL,  1, 100 );
  cmDspInst_t* rpp  = cmDspSysAllocInst(h,"RecdPlay",    NULL,  6, 2, r.scFn, recdPlayInitAllocSecs, recdPlayMaxLaSecs, recdPlayCurLaSecs, recdPlayFadeRateDbPerSec );
  cmDspInst_t* modp = cmDspSysAllocInst(h,"ScMod",       NULL,  2, r.modFn, "m1" );
  //cmDspInst_t* asp  = cmDspSysAllocInst(h,"AmSync",      NULL,  0 );



  cmDspInst_t* achan0      = cmDspSysAllocInst(h, "AvailCh",     NULL, 1, xfadeChCnt );
  cmDspInst_t* achan1      = cmDspSysAllocInst(h, "AvailCh",     NULL, 1, xfadeChCnt );


  cmDspInst_t* even_sr_00  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.8,   1.1, 0.0, 1.0 );
  cmDspInst_t* even_rt_00  = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtrChCnt, measRtrChCnt-1 );
  cmDspInst_t* dyn_sr_00   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0,   4.0, 0.01, 1.0 );
  cmDspInst_t* dyn_rt_00   = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtrChCnt, measRtrChCnt-1 );
  cmDspInst_t* tempo_sr_00 = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4, 80.0, 120.0, 0.01, 1.0 );
  cmDspInst_t* tempo_rt_00 = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtrChCnt, measRtrChCnt-1 );
  cmDspInst_t* cost_sr_00  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0,    1.0, 0.001, 1.0 );
  cmDspInst_t* cost_rt_00  = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtrChCnt, measRtrChCnt-1 );

  cmDspInst_t* thrh_sr_00  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, 0.01, 100.0 );
  cmDspInst_t* upr_sr_00   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, -1.0, 5.0 );
  cmDspInst_t* lwr_sr_00   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, -5.0, 5.0 );


  cmDspInst_t* even_sr_10  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.8,   1.1, 0.0, 1.0 );
  cmDspInst_t* even_rt_10  = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtrChCnt, measRtrChCnt-1 );
  cmDspInst_t* dyn_sr_10   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0,   4.0, 0.01, 1.0 );
  cmDspInst_t* dyn_rt_10   = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtrChCnt, measRtrChCnt-1 );
  cmDspInst_t* tempo_sr_10 = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4, 80.0, 120.0, 0.01, 1.0 );
  cmDspInst_t* tempo_rt_10 = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtrChCnt, measRtrChCnt-1 );
  cmDspInst_t* cost_sr_10  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0,    1.0, 0.001, 1.0 );
  cmDspInst_t* cost_rt_10  = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtrChCnt, measRtrChCnt-1 );
  cmDspInst_t* thrh_sr_10  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, 0.01, 100.0 );
  cmDspInst_t* upr_sr_10   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, -1.0, 5.0 );
  cmDspInst_t* lwr_sr_10   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, -5.0, 5.0 );


  unsigned paramRtChCnt = 2;
  cmDspInst_t* mod_rt_00   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );
  cmDspInst_t* thr_rt_00   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );
  cmDspInst_t* upr_rt_00   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );
  cmDspInst_t* lwr_rt_00   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );

  cmDspInst_t* mod_rt_10   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );
  cmDspInst_t* thr_rt_10   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );
  cmDspInst_t* upr_rt_10   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );
  cmDspInst_t* lwr_rt_10   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );



  cmDspInst_t* kr00 = cmDspSysAllocInst(h, "Kr",         NULL,   2, krWndSmpCnt, krHopFact );
  cmDspInst_t* kr01 = cmDspSysAllocInst(h, "Kr",         NULL,   2, krWndSmpCnt, krHopFact );
  cmDspInst_t* fad0 = cmDspSysAllocInst(h, "Xfader",     NULL,   3, xfadeChCnt,  xfadeMs, xfadeInitFl ); 
  cmDspInst_t* mix0 = cmDspSysAllocInst(h, "AMix",       NULL,   4, xfadeChCnt+1, mixGain, mixGain, fragMixGain );
  cmDspInst_t* cmp0 =  cmDspSysAllocInst(h,"Compressor", NULL,   8, cmpBypassFl, cmpThreshDb, cmpRatio_num, cmpAtkMs, cmpRlsMs, cmpMakeup, cmpWndMs, cmpWndMaxMs ); 
  cmDspInst_t* ao0p = cmDspSysAllocInst(h,"AudioOut",    NULL,   1, 0 );
  
  cmDspInst_t* kr10 = cmDspSysAllocInst(h, "Kr",         NULL,   2, krWndSmpCnt, krHopFact );
  cmDspInst_t* kr11 = cmDspSysAllocInst(h, "Kr",         NULL,   2, krWndSmpCnt, krHopFact );
  cmDspInst_t* fad1 = cmDspSysAllocInst(h, "Xfader",     NULL,   3, xfadeChCnt,  xfadeMs, xfadeInitFl ); 
  cmDspInst_t* mix1 = cmDspSysAllocInst(h, "AMix",       NULL,   4, xfadeChCnt+1, mixGain, mixGain, fragMixGain );
  cmDspInst_t* cmp1 =  cmDspSysAllocInst(h,"Compressor", NULL,   8, cmpBypassFl, cmpThreshDb, cmpRatio_num, cmpAtkMs, cmpRlsMs, cmpMakeup, cmpWndMs, cmpWndMaxMs ); 
  cmDspInst_t* ao1p = cmDspSysAllocInst(h,"AudioOut",    NULL,   1, 1 );

  cmDspInst_t* ao2p = NULL;
  cmDspInst_t* ao3p = NULL;
  if( splitFragFl )
  {
    ao2p = cmDspSysAllocInst(h,"AudioOut",    NULL,   1, 2 );
    ao3p = cmDspSysAllocInst(h,"AudioOut",    NULL,   1, 3 );
  }

  cmDspSysNewPage(h,"Controls");
  cmDspInst_t* liveb= cmDspSysAllocInst(h,"Button", "live",    2, kCheckDuiId,  0.0 );
  cmDspInst_t* simb = cmDspSysAllocInst(h,"Button", "simulate",2, kCheckDuiId,  0.0 );
  cmDspInst_t* ainb = cmDspSysAllocInst(h,"Button", "audio in",2, kCheckDuiId,  0.0 );
  cmDspInst_t* onb  = cmDspSysAllocInst(h,"Button", "start",   2, kButtonDuiId, 1.0 );
  cmDspInst_t* offb = cmDspSysAllocInst(h,"Button", "stop",    2, kButtonDuiId, 1.0 );
  cmDspInst_t* prtb = cmDspSysAllocInst(h,"Button", "print",   2, kButtonDuiId, 1.0 );
  cmDspInst_t* qtb  = cmDspSysAllocInst(h,"Button", "quiet",   2, kButtonDuiId, 1.0 );
  cmDspInst_t* measb= cmDspSysAllocInst(h,"Button", "meas",    2, kCheckDuiId,  0.0 );
  cmDspInst_t* prp  = cmDspSysAllocInst(h,"Printer", NULL,   1, ">" );
  cmDspInst_t* prd  = cmDspSysAllocInst(h,"Printer", NULL,   1, "DYN:" );
  cmDspInst_t* pre  = cmDspSysAllocInst(h,"Printer", NULL,   1, "EVEN:" );
  cmDspInst_t* prt  = cmDspSysAllocInst(h,"Printer", NULL,   1, "TEMPO:");
  cmDspInst_t* prc  = cmDspSysAllocInst(h,"Printer", NULL,   1, "COST:");
  //cmDspInst_t* prv  = cmDspSysAllocInst(h,"Printer", NULL,   1, "Value:");

  // Record <-> Live switches
  cmDspInst_t* tlRt  = cmDspSysAllocInst(h,"Router", NULL, 2, 2, 0);  // time line reset
  cmDspInst_t* wtRt  = cmDspSysAllocInst(h,"Router", NULL, 2, 2, 0);  // wave table output enable
  cmDspInst_t* mfpRt = cmDspSysAllocInst(h,"Router", NULL, 2, 2, 0);  // midi file player enable
  cmDspInst_t* amRt  = cmDspSysAllocInst(h,"Router", NULL, 2, 2, 0);  // active meas input
  cmDspInst_t* au0Sw = cmDspSysAllocInst(h,"1ofN",   NULL, 2, 2, 0);  // left audio input switch
  cmDspInst_t* au1Sw = cmDspSysAllocInst(h,"1ofN",   NULL, 2, 2, 0);  // right audio input switch

  cmDspInst_t* siRt  = cmDspSysAllocInst(h,"Router", NULL, 2, 2, 0);  // midi file player sample index
  cmDspInst_t* d0Rt  = cmDspSysAllocInst(h,"Router", NULL, 2, 2, 0);  // midi file player D0
  cmDspInst_t* d1Rt  = cmDspSysAllocInst(h,"Router", NULL, 2, 2, 0);  // midi file player D1
  cmDspInst_t* stRt  = cmDspSysAllocInst(h,"Router", NULL, 2, 2, 0);  // midi file player Status

  //--------------- Preset controls
  cmDspInst_t* preset    = cmDspSysAllocInst(   h, "Preset", NULL, 1, preGrpSymId );
  cmDspInst_t* presetLbl = cmDspSysAllocInst(   h, "Text",   "Preset",      1, "" );
  cmDspInst_t* storeBtn  = cmDspSysAllocButton( h, "store",  0);
  cmDspInst_t* recallBtn = cmDspSysAllocButton( h, "recall", 0);
  cmDspSysInstallCb(   h, presetLbl, "val", preset, "label",NULL);
  cmDspSysInstallCb(   h, storeBtn,  "sym", preset, "cmd", NULL );
  cmDspSysInstallCb(   h, recallBtn, "sym", preset, "cmd", NULL );

  cmDspInst_t* prePath    = cmDspSysAllocInst(   h, "Fname",  "prePath",   3, true,NULL,r.tlPrefixPath);
  

  cmDspSysNewColumn(h,0);

  //--------------- Recorded performance evaluation and Active Measurement related controls
  cmDspInst_t* clrBtn  = cmDspSysAllocButton( h, "clear",  0);
  cmDspInst_t* prtBtn  = cmDspSysAllocButton( h, "dump",  0);
  cmDspInst_t* mlst    = cmDspSysAllocInst(   h, "MsgList",   NULL, 3, "meas", r.measFn, 2);
  cmDspInst_t* amCmd   = cmDspSysAllocInst(   h, "PortToSym", NULL, 2, "add", "rewind" );  

  cmDspSysInstallCb( h, clrBtn, "sym",    amp, "cmd",  NULL );
  cmDspSysInstallCb( h, prtBtn, "sym",    amp, "cmd",  NULL );
  cmDspSysInstallCb( h, amCmd, "add",     amp, "cmd",  NULL );
  cmDspSysInstallCb( h, amCmd, "rewind",  amp, "cmd",  NULL );
  cmDspSysInstallCb( h, mlst,   "loc",    amp, "loc", NULL );
  cmDspSysInstallCb( h, mlst,   "typeId", amp, "type",NULL );
  cmDspSysInstallCb( h, mlst,   "val",    amp, "val", NULL );
  cmDspSysInstallCb( h, mlst,   "cost",   amp, "cst", NULL );
  cmDspSysInstallCb( h, mlst,   "typeId", amCmd, "add", NULL );
  cmDspSysInstallCb( h, sfp,    "out",    amRt, "f-in", NULL );
  cmDspSysInstallCb( h, amRt,   "f-out-0",amp, "sfloc", NULL );

  cmDspSysInstallCb( h, sfp, "vloc", amp, "loc", NULL );
  cmDspSysInstallCb( h, sfp, "vval", amp, "val",  NULL );
  cmDspSysInstallCb( h, sfp, "vcost",amp, "cst",  NULL );
  cmDspSysInstallCb( h, sfp, "vtyp", amp, "type", NULL );
  cmDspSysInstallCb( h, sfp, "vtyp", amCmd, "add", NULL);

  // ***** delete this to prevent the score follower from driving the active-measure unit in 'live' mode
  cmDspSysInstallCb( h, amRt,   "f-out-1",amp, "sfloc", NULL );
  // *****

  cmDspSysInstallCb( h, amp,    "even",   pre, "in", NULL );
  cmDspSysInstallCb( h, amp,    "dyn",    prd, "in", NULL );
  cmDspSysInstallCb( h, amp,    "tempo",  prt, "in", NULL );
  cmDspSysInstallCb( h, amp,    "cost",   prc, "in", NULL );

  cmDspSysNewColumn(h,0);

  double dfltOffset = 2.0; // 30.0;

  // ------   Spectral distortion controls 0
  cmDspInst_t* md00p = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "Mode-00",      0.0, 4.0, 1.0, 1.0);
  cmDspInst_t* ws00p = cmDspSysAllocMsgListP(h,preGrpSymId,NULL, "wndSmpCnt-00", NULL, "wndSmpCnt", 2);
  cmDspInst_t* hf00p = cmDspSysAllocMsgListP(h,preGrpSymId,NULL, "hopFact-00",   NULL, "hopFact",   2);
  cmDspInst_t* th00p = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "threshold-00", 0.0, 100.0, 1.0,  60.0 );
  cmDspInst_t* us00p = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "upr slope-00", 0.0,  10.0, 0.01,  0.0 ); 
  cmDspInst_t* ls00p = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "lwr slope-00", 0.3,  10.0, 0.01,  2.0 );
  cmDspInst_t* of00p = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "offset-00",    0.0, 100.0, 0.01, dfltOffset );
  cmDspInst_t* iv00p = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "invert-00",    0.0,   1.0, 1.0,   0.0 );  
  cmDspInst_t* wet00p  = cmDspSysAllocScalarP(h,preGrpSymId,NULL, "wet-00",       0.0,   1.0,0.001,  1.0 );  


  cmDspSysNewColumn(h,0);


  // ------   Spectral distortion controls 1
  cmDspInst_t* md10p  = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "Mode-10",      0.0, 4.0, 1.0, 1.0);
  cmDspInst_t* ws10p  = cmDspSysAllocMsgListP(h,preGrpSymId,NULL, "wndSmpCnt-10", NULL, "wndSmpCnt", 2);
  cmDspInst_t* hf10p  = cmDspSysAllocMsgListP(h,preGrpSymId,NULL, "hopFact-10",   NULL, "hopFact",   2);
  cmDspInst_t* th10p  = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "threshold-10", 0.0, 100.0, 1.0,  60.0 );
  cmDspInst_t* us10p  = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "upr slope-10", 0.0,  10.0, 0.01,  0.0 ); 
  cmDspInst_t* ls10p  = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "lwr slope-10", 0.3,  10.0, 0.01,  2.0 );
  cmDspInst_t* of10p  = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "offset-10",    0.0, 100.0, 0.01, dfltOffset );
  cmDspInst_t* iv10p  = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "invert-10",    0.0,   1.0, 1.0,   0.0 );  
  cmDspInst_t* wet10p  = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "wet-10",       0.0,    1.0,0.001,  1.0 );  

  cmDspSysNewColumn(h,0);

  cmDspInst_t* igain0 = cmDspSysAllocInst(h,"Scalar", "In Gain-0",    5, kNumberDuiId, 0.0,   10.0,0.01,   1.0 );  
  cmDspInst_t* igain1 = cmDspSysAllocInst(h,"Scalar", "In Gain-1",    5, kNumberDuiId, 0.0,   10.0,0.01,   1.0 );  

  cmDspInst_t* lasecs = cmDspSysAllocInst(h,"Scalar", "LA Secs",      5, kNumberDuiId, 0.0,   recdPlayMaxLaSecs,0.01,   recdPlayCurLaSecs );  
  cmDspInst_t* dbpsec = cmDspSysAllocInst(h,"Scalar", "Fade dBpSec",  5, kNumberDuiId, 0.0,   24.0, 0.01, recdPlayFadeRateDbPerSec);
  cmDspInst_t* ogain0 = cmDspSysAllocInst(h,"Scalar", "Out Gain-0",   5, kNumberDuiId, 0.0,   10.0,0.01,   3.0 );  
  cmDspInst_t* ogain1 = cmDspSysAllocInst(h,"Scalar", "Out Gain-1",   5, kNumberDuiId, 0.0,   10.0,0.01,   3.0 );  
  cmDspInst_t* ogain2 = cmDspSysAllocInst(h,"Scalar", "Out Gain-2",   5, kNumberDuiId, 0.0,   10.0,0.01,   3.0 );  
  cmDspInst_t* ogain3 = cmDspSysAllocInst(h,"Scalar", "Out Gain-3",   5, kNumberDuiId, 0.0,   10.0,0.01,   3.0 );  
  cmDspInst_t* xfadMs = cmDspSysAllocInst(h,"Scalar", "Xfade Ms",     5, kNumberDuiId, 0.0,   1000.0,0.01, 50.0 );  

  // Audio file recording
  cmDspInst_t* recdGain= cmDspSysAllocInst(h,"Scalar", "Recd Gain",  5, kNumberDuiId, 0.0,   100.0,0.01, 1.5 );  
  cmDspInst_t* recdChk = cmDspSysAllocInst(h,"Button", "Record",     2, kCheckDuiId, 0.0 );
  cmDspInst_t* recdPtS = cmDspSysAllocInst(h,"GateToSym", NULL,      2, cmSymTblRegisterStaticSymbol(cmDspSysSymbolTable(h),"open"),cmSymTblRegisterStaticSymbol(cmDspSysSymbolTable(h),"close"));
  cmDspInst_t* afop    = cmDspSysAllocInst(h,"AudioFileOut",NULL,    2, r.recordDir,2);
  cmDspInst_t* mi0p    = cmDspSysAllocInst(h,"AMeter","In 0",  0);
  cmDspInst_t* mi1p    = cmDspSysAllocInst(h,"AMeter","In 1",  0);
  
  cmDspSysInstallCb(h, recdGain,"val", afop,    "gain0", NULL );
  cmDspSysInstallCb(h, recdGain,"val", afop,    "gain1", NULL );
  cmDspSysInstallCb(h, recdChk, "out", recdPtS, "on",    NULL );
  cmDspSysInstallCb(h, recdChk, "out", recdPtS, "off",   NULL );
  cmDspSysInstallCb(h, recdPtS, "out", afop,    "sel",   NULL );


  cmDspSysNewPage(h,"Sc/Rgn");

  // -------- Measurement Scale/Ranges controls 0
  cmDspInst_t* min_dyn_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min In Dyn 0",      0.0, 10.0, 1.0, 0.0);
  cmDspInst_t* max_dyn_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max In Dyn 0",      0.0, 10.0, 1.0, 4.0);
  cmDspInst_t* menu_dyn_0  = cmDspSysAllocMsgListP(h,preGrpSymId, NULL, "DynSel 0", NULL, "measMenu", measRtrChCnt-1);

  cmDspInst_t* min_even_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min In Even 0",    0.0, 1.0, 0.001, 0.75);
  cmDspInst_t* max_even_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max In Even 0",    0.0, 3.0, 0.001, 1.0);
  cmDspInst_t* menu_even_0  = cmDspSysAllocMsgListP(h,preGrpSymId, NULL, "EvenSel 0", NULL, "measMenu", measRtrChCnt-1);

  cmDspSysNewColumn(h,0);
  cmDspInst_t* min_tempo_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min In Tempo 0",   0.0, 200.0, 1.0, 80.0);
  cmDspInst_t* max_tempo_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max In Tempo 0",   0.0, 200.0, 1.0, 120.0);
  cmDspInst_t* menu_tempo_0  = cmDspSysAllocMsgListP(h,preGrpSymId, NULL, "TempoSel 0", NULL, "measMenu", measRtrChCnt-1);

  cmDspInst_t* min_cost_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min In Cost 0",      0.0, 1.0, 0.01, 0.0);
  cmDspInst_t* max_cost_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max In Cost 0",      0.0, 1.0, 0.01, 1.0);
  cmDspInst_t* menu_cost_0  = cmDspSysAllocMsgListP(h,preGrpSymId, NULL, "CostSel 0", NULL, "measMenu", measRtrChCnt-1);

  // -------- Parameter Scale/Ranges controls 0
  cmDspSysNewColumn(h,0);
  cmDspInst_t* min_thrh_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min Thresh 0",      0.0, 100.0, 1.0, 30.0);
  cmDspInst_t* max_thrh_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max Thresh 0",      0.0, 100.0, 1.0, 80.0);

  cmDspInst_t* min_upr_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min Upr 0",          -1.0, 1.0, 0.001, -0.5);
  cmDspInst_t* max_upr_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max Upr 0",          -1.0, 1.0, 0.001, 0.5);

  cmDspInst_t* min_lwr_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min Lwr 0",          0.0, -1.0, 5.0, 1.0);
  cmDspInst_t* max_lwr_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max Lwr 0",          0.0, -1.0, 5.0, 3.0);


  // -------- Measurement Scale/Ranges controls 1
  cmDspSysNewColumn(h,0);
  cmDspInst_t* min_dyn_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min In Dyn 1",      0.0, 10.0, 1.0, 0.0);
  cmDspInst_t* max_dyn_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max In Dyn 1",      0.0, 10.0, 1.0, 4.0);
  cmDspInst_t* menu_dyn_1  = cmDspSysAllocMsgListP(h,preGrpSymId, NULL, "DynSel 1", NULL, "measMenu", measRtrChCnt-1);

  cmDspInst_t* min_even_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min In Even 1",    0.0, 1.0, 0.001, 0.75);
  cmDspInst_t* max_even_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max In Even 1",    0.0, 3.0, 0.001, 1.0);
  cmDspInst_t* menu_even_1  = cmDspSysAllocMsgListP(h,preGrpSymId, NULL, "EvenSel 1", NULL, "measMenu", measRtrChCnt-1);

  cmDspSysNewColumn(h,0);
  cmDspInst_t* min_tempo_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min In Tempo 1",   0.0, 200.0, 1.0, 80.0);
  cmDspInst_t* max_tempo_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max In Tempo 1",   0.0, 200.0, 1.0, 120.0);
  cmDspInst_t* menu_tempo_1  = cmDspSysAllocMsgListP(h,preGrpSymId, NULL, "TempoSel 1", NULL, "measMenu", measRtrChCnt-1);

  cmDspInst_t* min_cost_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min In Cost 1",      0.0, 1.0, 0.01, 0.0);
  cmDspInst_t* max_cost_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max In Cost 1",      0.0, 1.0, 0.01, 1.0);
  cmDspInst_t* menu_cost_1  = cmDspSysAllocMsgListP(h,preGrpSymId, NULL, "CostSel 1", NULL, "measMenu", measRtrChCnt-1);

  // -------- Parameter Scale/Ranges controls 1
  cmDspSysNewColumn(h,0);
  cmDspInst_t* min_thrh_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min Thresh 1",      0.0, 100.0, 1.0, 30.0);
  cmDspInst_t* max_thrh_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max Thresh 1",      0.0, 100.0, 1.0, 80.0);

  cmDspInst_t* min_upr_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min Upr 1",          -1.0, 1.0, 0.001, -0.5);
  cmDspInst_t* max_upr_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max Upr 1",          -1.0, 1.0, 0.001, 0.5);

  cmDspInst_t* min_lwr_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min Lwr 1",          0.0, -1.0, 5.0, 1.0);
  cmDspInst_t* max_lwr_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max Lwr 1",          0.0, -1.0, 5.0, 3.0);


  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    return rc;

  cmDspSysConnectAudio(h, php,  "out",   wtp,  "phs" );     // phs -> wt

  cmDspSysConnectAudio(h, wtp,  "out",   au0Sw, "a-in-0" ); // wt  -> sw
  cmDspSysConnectAudio(h, ai0p, "out",   au0Sw, "a-in-1" ); // ain -> sw
  cmDspSysConnectAudio(h, ai0p, "out",   mi0p,  "in" );
  cmDspSysConnectAudio(h, au0Sw,"a-out", rpp,   "in-0");    // sw  -> rcdply
  cmDspSysConnectAudio(h, au0Sw,"a-out", kr00, "in"  );     // sw  -> kr
  cmDspSysConnectAudio(h, kr00, "out",   fad0, "in-0");     // kr  -> fad
  cmDspSysConnectAudio(h, fad0, "out-0", mix0, "in-0");     // fad -> mix
  cmDspSysConnectAudio(h, au0Sw,"a-out", kr01, "in"  );     // sw  -> kr
  cmDspSysConnectAudio(h, kr01, "out",   fad0, "in-1");     // kr  -> fad
  cmDspSysConnectAudio(h, fad0, "out-1", mix0, "in-1");     // fad -> mix
  cmDspSysConnectAudio(h, rpp,  "out-0", mix0, "in-2");
  cmDspSysConnectAudio(h, mix0, "out",   cmp0, "in");       // mix -> cmp
  cmDspSysConnectAudio(h, cmp0, "out",   ao0p, "in" );      // cmp -> aout


  cmDspSysConnectAudio(h, wtp,  "out",   au1Sw, "a-in-0" );  // wt -> kr
  cmDspSysConnectAudio(h, ai1p, "out",   au1Sw, "a-in-1" );
  cmDspSysConnectAudio(h, ai1p, "out",   mi1p,  "in" );
  cmDspSysConnectAudio(h, au1Sw,"a-out", rpp,   "in-1");    // sw  -> rcdply
  cmDspSysConnectAudio(h, au1Sw,"a-out", kr10, "in"  );  
  cmDspSysConnectAudio(h, kr10, "out",   fad1, "in-0");
  cmDspSysConnectAudio(h, fad1, "out-0", mix1, "in-0");
  cmDspSysConnectAudio(h, au1Sw,"a-out", kr11, "in"  );  // wt -> kr
  cmDspSysConnectAudio(h, kr11, "out",   fad1, "in-1");  
  cmDspSysConnectAudio(h, fad1, "out-1", mix1, "in-1");
  cmDspSysConnectAudio(h, rpp,  "out-0", mix1, "in-2");
  cmDspSysConnectAudio(h, mix1, "out",   cmp1, "in");
  cmDspSysConnectAudio(h, cmp1, "out",   ao1p, "in" );   // comp -> aout

  cmDspSysConnectAudio(h, cmp0, "out", afop, "in0" );    // comp -> audio_file_out
  cmDspSysConnectAudio(h, cmp1, "out", afop, "in1" );

  if( splitFragFl )
  {
    cmDspSysConnectAudio(h, rpp, "out-0", ao2p, "in" );
    cmDspSysConnectAudio(h, rpp, "out-1", ao3p, "in" );
  }

  // wave-table to time-line cursor
  cmDspSysInstallCb(   h, wtp, "fidx",tlp,  "curs", NULL); 

  cmDspSysInstallCb(h, prePath, "out", tlp, "path", NULL );

  // 'live' button -> live router selector switch 
  cmDspSysInstallCb(h, liveb, "out",  wtRt, "sel", NULL );
  cmDspSysInstallCb(h, liveb, "out",  tlRt, "sel", NULL );
  cmDspSysInstallCb(h, liveb, "out",  mfpRt,"sel", NULL );
  cmDspSysInstallCb(h, liveb, "out",  amRt, "sel", NULL );
  cmDspSysInstallCb(h, liveb, "out",  au0Sw, "chidx", NULL );
  cmDspSysInstallCb(h, liveb, "out",  au1Sw, "chidx", NULL );
  cmDspSysInstallCb(h, liveb, "out",  measb, "in",     NULL );
  cmDspSysInstallCb(h, measb, "out",  sfp,   "measfl", NULL );

  // 'simulate' button -> simulate router selector switch
  cmDspSysInstallCb(h, simb,  "out",  ainb,  "in", NULL );
  cmDspSysInstallCb(h, ainb,  "out",  au0Sw, "chidx", NULL );
  cmDspSysInstallCb(h, ainb,  "out",  au1Sw, "chidx", NULL );
  cmDspSysInstallCb(h, simb,  "out",  siRt,  "sel", NULL );
  cmDspSysInstallCb(h, simb,  "out",  d1Rt,  "sel", NULL );
  cmDspSysInstallCb(h, simb,  "out",  d0Rt,  "sel", NULL );
  cmDspSysInstallCb(h, simb,  "out",  stRt,  "sel", NULL );
  
  
  // start connections
  cmDspSysInstallCb(h, onb,  "sym",    tlRt, "s-in",  NULL );
  cmDspSysInstallCb(h, tlRt, "s-out-0",tlp,  "reset", NULL );
  cmDspSysInstallCb(h, onb,  "sym",    scp,  "send",  NULL );
  cmDspSysInstallCb(h, onb,  "sym",    mfpRt,"s-in",  NULL );
  cmDspSysInstallCb(h, mfpRt,"s-out-0",mfp,  "sel",   NULL );

  cmDspSysInstallCb(h, onb, "sym",     pts,   "on",    NULL );
  cmDspSysInstallCb(h, pts, "on",      wtRt,  "s-in",  NULL );
  cmDspSysInstallCb(h, wtRt,"s-out-0", wtp,   "cmd",   NULL );
  cmDspSysInstallCb(h, pts, "on",      modp,  "cmd",   NULL );
  cmDspSysInstallCb(h, pts, "on",      rpp,   "cmd",   NULL );
  cmDspSysInstallCb(h, onb, "sym",     amCmd, "rewind",NULL );
  cmDspSysInstallCb(h, onb, "out",     achan0,"reset",  NULL );
  cmDspSysInstallCb(h, onb, "out",     achan1,"reset",  NULL );

  // stop connections
  cmDspSysInstallCb(h, wtp,  "done",offb,"in",  NULL ); // 'done' from WT simulates pressing Stop btn.
  cmDspSysInstallCb(h, tlp,  "mfn", pts, "off", NULL ); // Prevents WT start on new audio file from TL.
  cmDspSysInstallCb(h, offb, "sym", mfp, "sel", NULL ); 
  cmDspSysInstallCb(h, offb, "sym", pts, "off", NULL );
  cmDspSysInstallCb(h, pts,  "off", wtp, "cmd", NULL );
  cmDspSysInstallCb(h, pts,  "off", modp,"cmd", NULL );
  cmDspSysInstallCb(h, offb, "sym", mop, "reset", NULL );

  // time-line to wave-table selection 
  cmDspSysInstallCb(h, tlp, "absi", wtp, "beg", NULL );  
  cmDspSysInstallCb(h, tlp, "aesi", wtp, "end", NULL );
  cmDspSysInstallCb(h, tlp, "afn",  wtp, "fn",  NULL );

  // time-line to MIDI file player selection
  cmDspSysInstallCb(h, tlp, "mbsi", mfp, "bsi",   NULL );
  cmDspSysInstallCb(h, tlp, "mesi", mfp, "esi",   NULL );
  cmDspSysInstallCb(h, tlp, "mfn",  mfp, "fn",    NULL );
  
  // score to score follower - to set initial search location
  cmDspSysInstallCb(h, scp, "sel",    sfp, "index",  NULL );
  cmDspSysInstallCb(h, scp, "sel",    modp,"reset", NULL );
  
  //cmDspSysInstallCb(h, reload,"out",  modp, "reload", NULL );


  // MIDI file player to score follower
  cmDspSysInstallCb(h, mfp,  "smpidx",  siRt, "f-in",NULL );
  cmDspSysInstallCb(h, siRt, "f-out-0", sfp,  "smpidx",NULL ); 
  // leave siRt.f-out-1 unconnected because it should be ignored in 'simulate mode'

  cmDspSysInstallCb(h, mfp,  "d1",      d1Rt, "f-in",  NULL );
  cmDspSysInstallCb(h, d1Rt, "f-out-0", sfp,  "d1",    NULL );
  cmDspSysInstallCb(h, d1Rt, "f-out-1", nmp,  "d1",    NULL );
  cmDspSysInstallCb(h, nmp,   "d1",     mop,  "d1",    NULL );

  cmDspSysInstallCb(h, mfp,  "d0",      d0Rt,  "f-in", NULL );
  cmDspSysInstallCb(h, d0Rt, "f-out-0", sfp,   "d0",   NULL );
  cmDspSysInstallCb(h, d0Rt, "f-out-1", nmp,  "d0",   NULL );
  cmDspSysInstallCb(h, nmp,  "d0",      mop,  "d0",   NULL );

  cmDspSysInstallCb(h, mfp, "status",   stRt, "f-in",  NULL );
  cmDspSysInstallCb(h, stRt, "f-out-0", sfp,  "status",NULL );
  cmDspSysInstallCb(h, stRt, "f-out-1", nmp,  "status",NULL );
  cmDspSysInstallCb(h, nmp,  "status",  mop,  "status",NULL );

  // MIDI input port
  cmDspSysInstallCb(h, mip, "smpidx", sfp, "smpidx", NULL );
  cmDspSysInstallCb(h, mip, "d1",     sfp, "d1",     NULL );
  cmDspSysInstallCb(h, mip, "d0",     sfp, "d0",     NULL );
  cmDspSysInstallCb(h, mip, "status", sfp, "status", NULL );

  // score follower to recd_play,modulator and printers
  cmDspSysInstallCb(h, sfp, "out",     rpp,     "index", NULL );
  cmDspSysInstallCb(h, sfp, "out",     modp,    "index", NULL );
  cmDspSysInstallCb(h, sfp, "recent",  prp,     "in",  NULL );  // report 'recent' but only act on 'max' loc index

  cmDspSysInstallCb(h, prtb, "sym", sfp, "cmd", NULL );
  cmDspSysInstallCb(h, qtb,  "sym", sfp, "cmd", NULL );

  // audio-midi sync connections
  //cmDspSysInstallCb(h, tlp, "albl", asp, "afn", NULL );
  //cmDspSysInstallCb(h, tlp, "mlbl", asp, "mfn", NULL );
  //cmDspSysInstallCb(h, wtp, "fidx", asp, "asmp",NULL );
  //cmDspSysInstallCb(h, mfp, "id",   asp, "mid", NULL );
  //cmDspSysInstallCb(h, offb,"sym",  asp, "sel", NULL ); 
  //cmDspSysInstallCb(h, tlp, "absi", prp, "in",  NULL );

  cmDspSysInstallCb(h, ws00p,     "out",   kr00, "wndn", NULL );   // wndSmpCnt->kr
  cmDspSysInstallCb(h, hf00p,     "out",   kr00, "hopf", NULL );   // hopFact->kr
  cmDspSysInstallCb(h, of00p,     "val",   kr00, "offs", NULL );   // offset->kr
  cmDspSysInstallCb(h, iv00p,     "val",   kr00, "invt", NULL );   // invert->kr
  cmDspSysInstallCb(h, wet00p,    "val",   kr00, "wet", NULL );    //  wet->kr

  cmDspSysInstallCb(h, ws00p,     "out",   kr01, "wndn", NULL );   // wndSmpCnt->kr
  cmDspSysInstallCb(h, hf00p,     "out",   kr01, "hopf", NULL );   // hopFact->kr
  cmDspSysInstallCb(h, of00p,     "val",   kr01, "offs", NULL );   // offset->kr
  cmDspSysInstallCb(h, iv00p,     "val",   kr01, "invt", NULL );   // invert->kr
  cmDspSysInstallCb(h, wet00p,    "val",   kr01, "wet", NULL );    //  wet->kr

  cmDspSysInstallCb(h, ws10p,     "out",   kr10, "wndn", NULL );   // wndSmpCnt->kr
  cmDspSysInstallCb(h, hf10p,     "out",   kr10, "hopf", NULL );   // hopFact->kr
  cmDspSysInstallCb(h, of10p,     "val",   kr10, "offs", NULL );   // offset->kr
  cmDspSysInstallCb(h, iv10p,     "val",   kr10, "invt", NULL );   // invert->kr
  cmDspSysInstallCb(h, wet10p,    "val",   kr10, "wet", NULL );    //  wet->kr

  cmDspSysInstallCb(h, ws10p,     "out",   kr11, "wndn", NULL );   // wndSmpCnt->kr
  cmDspSysInstallCb(h, hf10p,     "out",   kr11, "hopf", NULL );   // hopFact->kr
  cmDspSysInstallCb(h, of10p,     "val",   kr11, "offs", NULL );   // offset->kr
  cmDspSysInstallCb(h, iv10p,     "val",   kr11, "invt", NULL );   // invert->kr
  cmDspSysInstallCb(h, wet10p,    "val",   kr11, "wet", NULL );    //  wet->kr

 
  cmDspSysInstallCb(   h, lasecs, "val", rpp, "curla", NULL ); // recd/play control
  cmDspSysInstallCb(   h, dbpsec, "val", rpp, "frate", NULL );

  cmDspSysInstallCb(h, igain0, "val", ai0p, "gain", NULL );   // input gain control
  cmDspSysInstallCb(h, igain1, "val", ai1p, "gain", NULL );
  cmDspSysInstallCb(h, ogain0, "val", ao0p, "gain", NULL );   // output gain control
  cmDspSysInstallCb(h, ogain1, "val", ao1p, "gain", NULL );
  cmDspSysInstallCb(h, ogain2, "val", ao2p, "gain", NULL );  
  cmDspSysInstallCb(h, ogain3, "val", ao3p, "gain", NULL );
  cmDspSysInstallCb(h, xfadMs, "val", fad0, "ms", NULL );
  cmDspSysInstallCb(h, xfadMs, "val", fad1, "ms", NULL );

  // Printer connections
  cmDspSysInstallCb(h, tlp, "afn",  prp, "in",  NULL );
  cmDspSysInstallCb(h, tlp, "mfn",  prp, "in",  NULL );
  cmDspSysInstallCb(h, tlp, "sel",  prp, "in",  NULL );

  cmDspSysInstallCb(h, modp, "mod0",  md00p, "val", NULL );
  cmDspSysInstallCb(h, modp, "thr0",  th00p, "val", NULL );
  cmDspSysInstallCb(h, modp, "upr0",  us00p, "val", NULL );
  cmDspSysInstallCb(h, modp, "lwr0",  ls00p, "val", NULL );
  cmDspSysInstallCb(h, modp, "mint0", min_thrh_0, "val", NULL );
  cmDspSysInstallCb(h, modp, "maxt0", max_thrh_0, "val", NULL );
  cmDspSysInstallCb(h, modp, "minu0", min_upr_0, "val", NULL );
  cmDspSysInstallCb(h, modp, "maxu0", max_upr_0, "val", NULL );
  cmDspSysInstallCb(h, modp, "minl0", min_lwr_0, "val", NULL );
  cmDspSysInstallCb(h, modp, "maxl0", max_lwr_0, "val", NULL );

  cmDspSysInstallCb(h, modp, "mod1",  md10p, "val", NULL );
  cmDspSysInstallCb(h, modp, "thr1",  th10p, "val", NULL );
  cmDspSysInstallCb(h, modp, "upr1",  us10p, "val", NULL );
  cmDspSysInstallCb(h, modp, "lwr1",  ls10p, "val", NULL );
  cmDspSysInstallCb(h, modp, "mint1", min_thrh_1, "val", NULL );
  cmDspSysInstallCb(h, modp, "maxt1", max_thrh_1, "val", NULL );
  cmDspSysInstallCb(h, modp, "minu1", min_upr_1, "val", NULL );
  cmDspSysInstallCb(h, modp, "maxu1", max_upr_1, "val", NULL );
  cmDspSysInstallCb(h, modp, "minl1", min_lwr_1, "val", NULL );
  cmDspSysInstallCb(h, modp, "maxl1", max_lwr_1, "val", NULL );

  cmDspSysInstallCb(h, modp, "xfad",  xfadMs, "val", NULL );

  // =========================================================================
  //  Cross fade connections for measurments
  //

  // active measure loc to xfad channel trigger
  cmDspSysInstallCb( h, amp,    "scloc",  achan0,   "trig", NULL );
  cmDspSysInstallCb( h, amp,    "scloc",  achan1,   "trig", NULL );
  //cmDspSysInstallCb( h, modp,   "post",   achan0,   "trig", NULL );
  //cmDspSysInstallCb( h, modp,   "post",   achan1,   "trig", NULL );

  // xfade router channel selection 
  cmDspSysInstallCb( h, achan0, "ch",  mod_rt_00, "sel", NULL );   
  cmDspSysInstallCb( h, achan0, "ch",  thr_rt_00, "sel", NULL );
  cmDspSysInstallCb( h, achan0, "ch",  upr_rt_00, "sel", NULL );
  cmDspSysInstallCb( h, achan0, "ch",  lwr_rt_00, "sel", NULL );

  cmDspSysInstallCb( h, achan1, "ch",  mod_rt_10, "sel", NULL );   
  cmDspSysInstallCb( h, achan1, "ch",  thr_rt_10, "sel", NULL );
  cmDspSysInstallCb( h, achan1, "ch",  upr_rt_10, "sel", NULL );
  cmDspSysInstallCb( h, achan1, "ch",  lwr_rt_10, "sel", NULL );

   

  // active measure to meas->param mapping routers
  cmDspSysInstallCb(h, amp,  "even",  even_sr_00, "val_in", NULL );
  cmDspSysInstallCb(h, amp,  "even",  even_sr_10, "val_in", NULL );
  cmDspSysInstallCb(h, amp,  "dyn",   dyn_sr_00,  "val_in", NULL );
  cmDspSysInstallCb(h, amp,  "dyn",   dyn_sr_10,  "val_in", NULL );
  cmDspSysInstallCb(h, amp,  "tempo", tempo_sr_00,"val_in", NULL );
  cmDspSysInstallCb(h, amp,  "tempo", tempo_sr_10,"val_in", NULL );
  cmDspSysInstallCb(h, amp,  "cost",  cost_sr_00, "val_in", NULL );
  cmDspSysInstallCb(h, amp,  "cost",  cost_sr_10, "val_in", NULL );

  // active-channel to cross-fade connections
  cmDspSysInstallCb(h, achan0, "reset",   fad0, "reset", NULL);
  cmDspSysInstallCb(h, achan0, "gate-0",  fad0, "gate-0", NULL );
  cmDspSysInstallCb(h, achan0, "gate-1",  fad0, "gate-1", NULL );
  cmDspSysInstallCb(h, fad0,   "state-0", achan0, "dis-0",  NULL );
  cmDspSysInstallCb(h, fad0,   "state-1", achan0, "dis-1",  NULL );

  cmDspSysInstallCb(h, achan1, "reset",   fad1, "reset", NULL);
  cmDspSysInstallCb(h, achan1, "gate-0",  fad1, "gate-0", NULL );
  cmDspSysInstallCb(h, achan1, "gate-1",  fad1, "gate-1", NULL );
  cmDspSysInstallCb(h, fad1,   "state-0", achan1, "dis-0",  NULL );
  cmDspSysInstallCb(h, fad1,   "state-1", achan1, "dis-1",  NULL );


  // =========================================================================
  //  Scale Range Connections 00
  //

  // DYN -> scaleRange -> Router -> var scaleRange
  cmDspSysInstallCb(h, min_dyn_0,  "val",     dyn_sr_00,  "min_in",  NULL );
  cmDspSysInstallCb(h, max_dyn_0,  "val",     dyn_sr_00,  "max_in",  NULL );
  cmDspSysInstallCb(h, dyn_sr_00,   "val_out", dyn_rt_00,  "f-in",    NULL );
  cmDspSysInstallCb(h, menu_dyn_0, "out",     dyn_rt_00,  "sel",     NULL );
  cmDspSysInstallCb(h, dyn_rt_00,   "f-out-0", thrh_sr_00, "val_in",  NULL );
  cmDspSysInstallCb(h, dyn_rt_00,   "f-out-1", upr_sr_00,  "val_in",  NULL );
  cmDspSysInstallCb(h, dyn_rt_00,   "f-out-2", lwr_sr_00,  "val_in",  NULL );

  // EVEN -> scaleRange -> Router  -> var scaleRange
  cmDspSysInstallCb(h, min_even_0,  "val",     even_sr_00,  "min_in",  NULL );
  cmDspSysInstallCb(h, max_even_0,  "val",     even_sr_00,  "max_in",  NULL );
  cmDspSysInstallCb(h, even_sr_00,   "val_out", even_rt_00,  "f-in",    NULL );
  cmDspSysInstallCb(h, menu_even_0, "out",     even_rt_00,  "sel",     NULL );
  cmDspSysInstallCb(h, even_rt_00,   "f-out-0", thrh_sr_00, "val_in",  NULL );
  cmDspSysInstallCb(h, even_rt_00,   "f-out-1", upr_sr_00,  "val_in",  NULL );
  cmDspSysInstallCb(h, even_rt_00,   "f-out-2", lwr_sr_00,  "val_in",  NULL );

  // TEMPO -> scaleRange -> Router  -> var scaleRange
  cmDspSysInstallCb(h, min_tempo_0,  "val",     tempo_sr_00,  "min_in",  NULL );
  cmDspSysInstallCb(h, max_tempo_0,  "val",     tempo_sr_00,  "max_in",  NULL );
  cmDspSysInstallCb(h, tempo_sr_00,   "val_out", tempo_rt_00,  "f-in",    NULL );
  cmDspSysInstallCb(h, menu_tempo_0, "out",     tempo_rt_00,  "sel",     NULL );
  cmDspSysInstallCb(h, tempo_rt_00,   "f-out-0", thrh_sr_00, "val_in",  NULL );
  cmDspSysInstallCb(h, tempo_rt_00,   "f-out-1", upr_sr_00,  "val_in",  NULL );
  cmDspSysInstallCb(h, tempo_rt_00,   "f-out-2", lwr_sr_00,  "val_in",  NULL );

  // COST -> scaleRange -> Router  -> var scaleRange 
  cmDspSysInstallCb(h, min_cost_0,  "val",     cost_sr_00,  "min_in",  NULL );
  cmDspSysInstallCb(h, max_cost_0,  "val",     cost_sr_00,  "max_in",  NULL );
  cmDspSysInstallCb(h, cost_sr_00,   "val_out", cost_rt_00,  "f-in",    NULL );
  cmDspSysInstallCb(h, menu_cost_0, "out",     cost_rt_00,  "sel",     NULL );
  cmDspSysInstallCb(h, cost_rt_00,   "f-out-0", thrh_sr_00, "val_in",  NULL );
  cmDspSysInstallCb(h, cost_rt_00,   "f-out-1", upr_sr_00,  "val_in",  NULL );
  cmDspSysInstallCb(h, cost_rt_00,   "f-out-2", lwr_sr_00,  "val_in",  NULL );

  // MODE -> FX
  cmDspSysInstallCb(h, md00p,      "val",  mod_rt_00,  "f-in",      NULL );
  cmDspSysInstallCb(h, mod_rt_00, "f-out-0", kr00, "mode", NULL );   // mode->kr
  cmDspSysInstallCb(h, mod_rt_00, "f-out-1", kr01, "mode", NULL );   // mode->kr


  // THRESH scaleRange -> FX
  cmDspSysInstallCb(h, min_thrh_0, "val",     thrh_sr_00, "min_out", NULL );
  cmDspSysInstallCb(h, max_thrh_0, "val",     thrh_sr_00, "max_out", NULL );
  cmDspSysInstallCb(h, thrh_sr_00, "val_out", th00p,      "val",     NULL );
  cmDspSysInstallCb(h, th00p,      "val",     thr_rt_00,  "f-in",      NULL );
  cmDspSysInstallCb(h, thr_rt_00,  "f-out-0",   kr00,       "thrh",    NULL );   // thresh->kr
  cmDspSysInstallCb(h, thr_rt_00,  "f-out-1",   kr01,       "thrh",    NULL );   // thresh->kr

  // UPR scaleRange -> FX
  cmDspSysInstallCb(h, min_upr_0, "val",     upr_sr_00, "min_out", NULL );
  cmDspSysInstallCb(h, max_upr_0, "val",     upr_sr_00, "max_out", NULL );
  cmDspSysInstallCb(h, upr_sr_00, "val_out", us00p,     "val",     NULL );
  cmDspSysInstallCb(h, us00p,     "val",     upr_rt_00, "f-in",      NULL );
  cmDspSysInstallCb(h, upr_rt_00, "f-out-0",   kr00,      "uprs",    NULL );   // uprSlope->kr
  cmDspSysInstallCb(h, upr_rt_00, "f-out-1",   kr01,      "uprs",    NULL );   // uprSlope->kr


  // LWR scaleRange -> FX
  cmDspSysInstallCb(h, min_lwr_0, "val",     lwr_sr_00, "min_out", NULL );
  cmDspSysInstallCb(h, max_lwr_0, "val",     lwr_sr_00, "max_out", NULL );
  cmDspSysInstallCb(h, lwr_sr_00, "val_out", ls00p,     "val",     NULL );
  cmDspSysInstallCb(h, ls00p,     "val",     lwr_rt_00, "f-in",      NULL );
  cmDspSysInstallCb(h, lwr_rt_00, "f-out-0",   kr00,      "lwrs",    NULL );   // lwrSlope->kr
  cmDspSysInstallCb(h, lwr_rt_00, "f-out-1",   kr01,      "lwrs",    NULL );   // lwrSlope->kr



  // =========================================================================
  //  Scale Range Connections 10
  //

  // DYN -> scaleRange -> Router -> var scaleRange
  cmDspSysInstallCb(h, min_dyn_1,  "val",     dyn_sr_10,  "min_in",  NULL );
  cmDspSysInstallCb(h, max_dyn_1,  "val",     dyn_sr_10,  "max_in",  NULL );
  cmDspSysInstallCb(h, dyn_sr_10,   "val_out", dyn_rt_10,  "f-in",    NULL );
  cmDspSysInstallCb(h, menu_dyn_1, "out",     dyn_rt_10,  "sel",     NULL );
  cmDspSysInstallCb(h, dyn_rt_10,   "f-out-0", thrh_sr_10, "val_in",  NULL );
  cmDspSysInstallCb(h, dyn_rt_10,   "f-out-1", upr_sr_10,  "val_in",  NULL );
  cmDspSysInstallCb(h, dyn_rt_10,   "f-out-2", lwr_sr_10,  "val_in",  NULL );

  // EVEN -> scaleRange -> Router  -> var scaleRange
  cmDspSysInstallCb(h, min_even_1,  "val",     even_sr_10,  "min_in",  NULL );
  cmDspSysInstallCb(h, max_even_1,  "val",     even_sr_10,  "max_in",  NULL );
  cmDspSysInstallCb(h, even_sr_10,   "val_out", even_rt_10,  "f-in",    NULL );
  cmDspSysInstallCb(h, menu_even_1, "out",     even_rt_10,  "sel",     NULL );
  cmDspSysInstallCb(h, even_rt_10,   "f-out-0", thrh_sr_10, "val_in",  NULL );
  cmDspSysInstallCb(h, even_rt_10,   "f-out-1", upr_sr_10,  "val_in",  NULL );
  cmDspSysInstallCb(h, even_rt_10,   "f-out-2", lwr_sr_10,  "val_in",  NULL );

  // TEMPO -> scaleRange -> Router  -> var scaleRange
  cmDspSysInstallCb(h, min_tempo_1,  "val",     tempo_sr_10,  "min_in",  NULL );
  cmDspSysInstallCb(h, max_tempo_1,  "val",     tempo_sr_10,  "max_in",  NULL );
  cmDspSysInstallCb(h, tempo_sr_10,   "val_out", tempo_rt_10,  "f-in",    NULL );
  cmDspSysInstallCb(h, menu_tempo_1, "out",     tempo_rt_10,  "sel",     NULL );
  cmDspSysInstallCb(h, tempo_rt_10,   "f-out-0", thrh_sr_10, "val_in",  NULL );
  cmDspSysInstallCb(h, tempo_rt_10,   "f-out-1", upr_sr_10,  "val_in",  NULL );
  cmDspSysInstallCb(h, tempo_rt_10,   "f-out-2", lwr_sr_10,  "val_in",  NULL );

  // COST -> scaleRange -> Router  -> var scaleRange 
  cmDspSysInstallCb(h, min_cost_1,  "val",     cost_sr_10,  "min_in",  NULL );
  cmDspSysInstallCb(h, max_cost_1,  "val",     cost_sr_10,  "max_in",  NULL );
  cmDspSysInstallCb(h, cost_sr_10,   "val_out", cost_rt_10,  "f-in",    NULL );
  cmDspSysInstallCb(h, menu_cost_1, "out",     cost_rt_10,  "sel",     NULL );
  cmDspSysInstallCb(h, cost_rt_10,   "f-out-0", thrh_sr_10, "val_in",  NULL );
  cmDspSysInstallCb(h, cost_rt_10,   "f-out-1", upr_sr_10,  "val_in",  NULL );
  cmDspSysInstallCb(h, cost_rt_10,   "f-out-2", lwr_sr_10,  "val_in",  NULL );

  // MODE -> FX
  cmDspSysInstallCb(h, md10p,      "val",      mod_rt_10,  "f-in",      NULL );
  cmDspSysInstallCb(h, mod_rt_10, "f-out-0", kr10, "mode", NULL );   // mode->kr
  cmDspSysInstallCb(h, mod_rt_10, "f-out-1", kr11, "mode", NULL );   // mode->kr

  // THRESH scaleRange -> FX
  cmDspSysInstallCb(h, min_thrh_1, "val",      thrh_sr_10, "min_out", NULL );
  cmDspSysInstallCb(h, max_thrh_1, "val",      thrh_sr_10, "max_out", NULL );
  cmDspSysInstallCb(h, thrh_sr_10, "val_out",  th10p,      "val",     NULL );
  cmDspSysInstallCb(h, th10p,      "val",      thr_rt_10,  "f-in",      NULL );
  cmDspSysInstallCb(h, thr_rt_10, "f-out-0", kr10, "thrh", NULL );   // thresh->kr
  cmDspSysInstallCb(h, thr_rt_10, "f-out-1", kr11, "thrh", NULL );   // thresh->kr

  // UPR scaleRange -> FX
  cmDspSysInstallCb(h, min_upr_1, "val",      upr_sr_10,  "min_out", NULL );
  cmDspSysInstallCb(h, max_upr_1, "val",      upr_sr_10,  "max_out", NULL );
  cmDspSysInstallCb(h, upr_sr_10,  "val_out", us10p,      "val",     NULL );
  cmDspSysInstallCb(h, us10p,      "val",     upr_rt_10,  "f-in",      NULL );
  cmDspSysInstallCb(h, upr_rt_10, "f-out-0", kr10, "uprs", NULL );   // uprSlope->kr
  cmDspSysInstallCb(h, upr_rt_10, "f-out-1", kr11, "uprs", NULL );   // uprSlope->kr

  // LWR scaleRange -> FX
  cmDspSysInstallCb(h, min_lwr_1, "val",     lwr_sr_10, "min_out", NULL );
  cmDspSysInstallCb(h, max_lwr_1, "val",     lwr_sr_10, "max_out", NULL );
  cmDspSysInstallCb(h, lwr_sr_10, "val_out", ls10p,     "val",     NULL );
  cmDspSysInstallCb(h, ls10p,     "val",     lwr_rt_10, "f-in",      NULL );
  cmDspSysInstallCb(h, lwr_rt_10, "f-out-0", kr10, "lwrs", NULL );   // lwrSlope->kr
  cmDspSysInstallCb(h, lwr_rt_10, "f-out-1", kr11, "lwrs", NULL );   // lwrSlope->kr

  


  cmDspSysNewPage(h,"Compressor");

  cmDspInst_t* cmp0_byp   = cmDspSysAllocCheckP(  h,  compPreGrpSymId, NULL, "Bypass0", 1.0 );
  cmDspInst_t* cmp0_igain = cmDspSysAllocScalarP( h,  compPreGrpSymId, NULL, "In Gain0", 0.0,   10.0, 0.1, cmpInGain );
  cmDspInst_t* cmp0_thr   = cmDspSysAllocScalarP( h,  compPreGrpSymId, NULL, "ThreshDb0", -100.0, 0.0, 0.1, cmpThreshDb);
  cmDspInst_t* cmp0_rat   = cmDspSysAllocScalarP( h,  compPreGrpSymId, NULL, "Ratio0",    0.1, 100, 0.1, cmpRatio_num);
  cmDspInst_t* cmp0_atk   = cmDspSysAllocScalarP( h,  compPreGrpSymId, NULL, "Atk Ms0",   0.0, 1000.0, 0.1, cmpAtkMs);
  cmDspInst_t* cmp0_rls   = cmDspSysAllocScalarP( h,  compPreGrpSymId, NULL, "Rls Ms0",   0.0, 1000.0, 0.1, cmpRlsMs);
  cmDspInst_t* cmp0_mkup  = cmDspSysAllocScalarP( h,  compPreGrpSymId, NULL, "Makeup0",   0.0, 10.0,   0.01, cmpMakeup);
  cmDspInst_t* cmp0_wnd   = cmDspSysAllocScalarP( h,  compPreGrpSymId, NULL, "Wnd Ms0",   1.0, cmpWndMaxMs, 1.0, cmpWndMs );
  cmDspInst_t* cmp0_mtr   = cmDspSysAllocInst(h,"Meter","Env0", 3, 0.0, 0.0, 1.0);

  cmDspSysInstallCb(h, cmp0_byp,  "out", cmp0, "bypass", NULL );
  cmDspSysInstallCb(h, cmp0_igain,"val", cmp0, "igain", NULL );
  cmDspSysInstallCb(h, cmp0_thr,  "val", cmp0, "thr", NULL );
  cmDspSysInstallCb(h, cmp0_rat,  "val", cmp0, "ratio", NULL );
  cmDspSysInstallCb(h, cmp0_atk,  "val", cmp0, "atk", NULL );
  cmDspSysInstallCb(h, cmp0_rls,  "val", cmp0, "rls", NULL );
  cmDspSysInstallCb(h, cmp0_mkup, "val", cmp0, "ogain", NULL );
  cmDspSysInstallCb(h, cmp0_wnd,  "val", cmp0, "wnd", NULL );
  cmDspSysInstallCb(h, cmp0,      "env", cmp0_mtr, "in", NULL );

  cmDspSysNewColumn(h,0);
  cmDspInst_t* cmp1_byp   = cmDspSysAllocCheckP(  h, compPreGrpSymId, NULL, "Bypass1", 1.0 );
  cmDspInst_t* cmp1_igain = cmDspSysAllocScalarP( h, compPreGrpSymId, NULL, "In Gain1",  0.0,   10.0, 0.1, cmpInGain);
  cmDspInst_t* cmp1_thr   = cmDspSysAllocScalarP( h, compPreGrpSymId, NULL, "ThreshDb1", -100.0, 0.0, 0.1, cmpThreshDb);
  cmDspInst_t* cmp1_rat   = cmDspSysAllocScalarP( h, compPreGrpSymId, NULL, "Ratio1",    0.1, 100, 0.1, cmpRatio_num);
  cmDspInst_t* cmp1_atk   = cmDspSysAllocScalarP( h, compPreGrpSymId, NULL, "Atk Ms1",   0.0, 1000.0, 0.1, cmpAtkMs);
  cmDspInst_t* cmp1_rls   = cmDspSysAllocScalarP( h, compPreGrpSymId, NULL, "Rls Ms1",   0.0, 1000.0, 0.1, cmpRlsMs);
  cmDspInst_t* cmp1_mkup  = cmDspSysAllocScalarP( h, compPreGrpSymId, NULL, "Makeup1",   0.0, 10.0,   0.01, cmpMakeup);
  cmDspInst_t* cmp1_wnd   = cmDspSysAllocScalarP( h, compPreGrpSymId, NULL, "Wnd Ms1",   1.0, cmpWndMaxMs, 1.0, cmpWndMs );
  cmDspInst_t* cmp1_mtr   = cmDspSysAllocInst(h,"Meter","Env1", 3, 0.0, 0.0, 1.0);

  cmDspSysInstallCb(h, cmp1_byp,  "out", cmp1, "bypass", NULL );
  cmDspSysInstallCb(h, cmp1_igain,"val", cmp1, "igain", NULL );
  cmDspSysInstallCb(h, cmp1_thr,  "val", cmp1, "thr", NULL );
  cmDspSysInstallCb(h, cmp1_rat,  "val", cmp1, "ratio", NULL );
  cmDspSysInstallCb(h, cmp1_atk,  "val", cmp1, "atk", NULL );
  cmDspSysInstallCb(h, cmp1_rls,  "val", cmp1, "rls", NULL );
  cmDspSysInstallCb(h, cmp1_mkup, "val", cmp1, "ogain", NULL );
  cmDspSysInstallCb(h, cmp1_wnd,  "val", cmp1, "wnd", NULL );
  cmDspSysInstallCb(h, cmp1,      "env", cmp1_mtr, "in", NULL );

  //--------------- Compressor Preset controls
  cmDspSysNewColumn(h,0);
  cmDspInst_t* comp_preset    = cmDspSysAllocInst(   h, "Preset", NULL, 1, compPreGrpSymId );
  cmDspInst_t* comp_presetLbl = cmDspSysAllocInst(   h, "Text",   "Comp_Preset",      1, "" );
  cmDspInst_t* comp_storeBtn  = cmDspSysAllocButton( h, "comp_store",  0);
  cmDspInst_t* comp_recallBtn = cmDspSysAllocButton( h, "comp_recall", 0);
  cmDspInst_t* comp_pts       = cmDspSysAllocInst(   h, "PortToSym", NULL, 2, "store", "recall");

  cmDspSysInstallCb(   h, comp_presetLbl, "val",    comp_preset, "label",NULL);
  cmDspSysInstallCb(   h, comp_storeBtn,  "out",    comp_pts,    "store", NULL );
  cmDspSysInstallCb(   h, comp_recallBtn, "out",    comp_pts,    "recall", NULL );
  cmDspSysInstallCb(   h, comp_pts,       "store",  comp_preset, "cmd", NULL );
  cmDspSysInstallCb(   h, comp_pts,       "recall", comp_preset, "cmd", NULL );
  
  return rc;
}




cmDspRC_t _cmDspSysPgm_KrLive(cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t       rc         = kOkDspRC;
  return rc;

}
