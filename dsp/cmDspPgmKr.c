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
#include "cmTime.h"
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
  const cmChar_t* midiOutPort2;
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
  cmDspRsrcString(h,&r->midiOutPort2, "midiOutPort2", NULL);

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

#define mlbl(a)  _mlbl(a,mch)
#define lbl(a) cmDspSysPrintLabel(a,ch)


void _cmDspSys_TlXformChain( cmDspSysH_t h, cmDspTlXform_t* c,  unsigned preGrpSymId, unsigned cmpPreGrpSymId, cmDspInst_t* modp, unsigned ch, unsigned mch )
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

 
  // Parameter-> kr routers (routers used to cross-fade between the two kr units)
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
  //cmDspInst_t* kr1  = cmDspSysAllocInst(h, "Kr",         NULL,   2, krWndSmpCnt, krHopFact );
  cmDspInst_t* xfad = cmDspSysAllocInst(h, "Xfader",     NULL,   3, xfadeChCnt,  xfadeMs, xfadeInitFl ); 
  cmDspInst_t* mix  = cmDspSysAllocInst(h, "AMix",       NULL,   3, xfadeChCnt,  mixGain, mixGain );
  cmDspInst_t* cmp  = cmDspSysAllocInst(h, "Compressor", NULL,   8, cmpBypassFl, cmpThreshDb, cmpRatio_num, cmpAtkMs, cmpRlsMs, cmpMakeup, cmpWndMs, cmpWndMaxMs ); 


  // Internal audio connections
  cmDspSysConnectAudio(h, kr0,  "out",   xfad, "in-0");
  //cmDspSysConnectAudio(h, kr1,  "out",   xfad, "in-1");
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
  //cmDspSysInstallCb(h, mod_rt,  "f-out-1",     kr1,    "mode",    NULL );   // mode->kr

  cmDspSysInstallCb(h, wnd_ctl, "out",         wnd_rt, "f-in",    NULL );
  cmDspSysInstallCb(h, achan,   "ch",          wnd_rt, "sel",     NULL );   // ach->rt sel
  cmDspSysInstallCb(h, wnd_rt,  "f-out-0",     kr0,    "wndn",    NULL );   // wndn->kr
  //cmDspSysInstallCb(h, wnd_rt,  "f-out-1",     kr1,    "wndn",    NULL );   // wndn->kr

  cmDspSysInstallCb(h, hop_ctl, "out",         hop_rt, "f-in",    NULL );
  cmDspSysInstallCb(h, achan,   "ch",          hop_rt, "sel",     NULL );   // ach->rt sel
  cmDspSysInstallCb(h, hop_rt,  "f-out-0",     kr0,    "hopf",    NULL );   // hopf->kr
  //cmDspSysInstallCb(h, hop_rt,  "f-out-1",     kr1,    "hopf",    NULL );   // hopf->kr

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
  //cmDspSysInstallCb(h, thr_rt,      "f-out-1", kr1,    "thrh",    NULL );   // thr->kr

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
  //cmDspSysInstallCb(h, upr_rt,      "f-out-1", kr1,    "uprs",    NULL );   // upr->kr

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
  //cmDspSysInstallCb(h, lwr_rt,      "f-out-1", kr1,    "lwrs",    NULL );   // lwr->kr

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
  //cmDspSysInstallCb(h, off_rt,      "f-out-1", kr1,    "offs",    NULL );   // off->kr

  cmDspSysInstallCb(h, inv_ctl,     "val",     inv_rt, "f-in",   NULL );
  cmDspSysInstallCb(h, achan,       "ch",      inv_rt, "sel",    NULL );   // ach->rt sel
  cmDspSysInstallCb(h, inv_rt,      "f-out-0", kr0,    "invt",   NULL );   // inv->kr
  //cmDspSysInstallCb(h, inv_rt,      "f-out-1", kr1,    "invt",   NULL );   // inv->kr

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
  //cmDspSysInstallCb(h, wet_rt,      "f-out-1", kr1,    "wet",     NULL );   // wet->kr
  

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

  cmDspSysInstallCb(h, modp, mlbl("cbyp"),    cmp_byp,  "in", NULL );
  cmDspSysInstallCb(h, modp, mlbl("cigain"),  cmp_igain,"val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("cthrsh"),  cmp_thr,  "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("cratio"),  cmp_rat,  "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("catkms"),  cmp_atk,  "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("crlsms"),  cmp_rls,  "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("cmakeup"), cmp_mkup, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("cwndms"),  cmp_wnd,  "val", NULL );

  // 
  cmDspInst_t* xfadMs = cmDspSysAllocInst(h,"Scalar", lbl("Xfade Ms"),     5, kNumberDuiId, 0.0,   1000.0,0.01, 50.0 );  
  cmDspSysInstallCb(h, xfadMs, "val", xfad, "ms", NULL );
  cmDspSysInstallCb(h, modp, mlbl("xfad"), xfadMs,  "val", NULL);

  cmDspSysInstallCb(h, modp, mlbl("win"),  wnd_ctl, "sel",  NULL );
  cmDspSysInstallCb(h, modp, mlbl("hop"),  hop_ctl, "sel", NULL );
  cmDspSysInstallCb(h, modp, mlbl("mod"),  mod_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("thr"),  thr_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("upr"),  upr_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("lwr"),  lwr_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("off"),  off_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("mint"), min_thr_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("maxt"), max_thr_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("minu"), min_upr_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("maxu"), max_upr_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("minl"), min_lwr_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("maxl"), max_lwr_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("mino"), min_off_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("maxo"), max_off_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("sw"),   achan,       "trig", NULL ); // See also: amp.sfloc->achan.trig

  c->achan = achan; 
  c->kr0   = kr0; 
  //c->kr1   = kr1;
  c->cmp   = cmp; 

}

cmDspRC_t _cmDspSysPgm_TimeLine(cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t       rc         = kOkDspRC;
  cmCtx_t*        cmCtx      = cmDspSysPgmCtx(h);
  cmErr_t         err;
  krRsrc_t        r;
  bool     fragFl       = false;
  bool     useWtFl      = false;
  bool     useChain1Fl  = true;
  bool     useInputEqFl = false;
  bool     useInCompFl  = true;

  unsigned wtLoopCnt    = 1;    // 1=play once (-1=loop forever)
  unsigned wtInitMode   = 0;    // initial wt mode is 'silence'
  unsigned wtSmpCnt     = floor(cmDspSysSampleRate(h)); // wt length == srate

  unsigned        sfBufCnt    = 7;     // length of the MIDI event buffer
  unsigned        sfMaxWndCnt = 10;    // length of the score event buffer
  unsigned        sfMinVel    = 5;     // ignore MIDI events below this velocity
  bool            sfEnaMeasFl = false;
  double          recdPlayInitAllocSecs    = 10.0;
  double          recdPlayMaxLaSecs        = 2.0;
  double          recdPlayCurLaSecs        = 0.1;
  double          recdPlayFadeRateDbPerSec = 4.0;


  bool            eqBypassFl  = false;

  unsigned        eqLpSymId   = cmSymTblRegisterStaticSymbol(cmDspSysSymbolTable(h),"LP");
  double          eqLpF0hz    = 200.0;
  double          eqLpQ       = 0.5;
  double          eqLpGain    = 0.9;

  unsigned        eqBpSymId   = cmSymTblRegisterStaticSymbol(cmDspSysSymbolTable(h),"BP");
  double          eqBpF0hz    = 350.0;
  double          eqBpQ       = 0.6;
  double          eqBpGain    = 1.0;

  unsigned        eqHpSymId = cmSymTblRegisterStaticSymbol(cmDspSysSymbolTable(h),"HP");
  double          eqHpF0hz    = 600.0;
  double          eqHpQ       = 0.5;
  double          eqHpGain    = 0.9;

  bool            apfBypassFl  = false;
  unsigned        apfModeSymId  = cmSymTblRegisterStaticSymbol(cmDspSysSymbolTable(h),"AP");
  double          apfF0hz       = 100.0;
  double          apfQ          = 1.0;
  double          apfGain       = 1.0;


  // input compressor default parameters
  bool            cmpBypassFl  = false;
  double          cmpInGain    = 3.0;
  double          cmpThreshDb  = -40.0;
  double          cmpRatio_num = 5.0;
  double          cmpAtkMs     = 20.0;
  double          cmpRlsMs     = 100.0;
  double          cmpMakeup    = 1.0;
  double          cmpWndMaxMs  = 1000.0;
  double          cmpWndMs     = 200.0;
 
  memset(&r,0,sizeof(r));
  cmErrSetup(&err,&cmCtx->rpt,"Kr Timeline");

  if( krLoadRsrc(h,&err,&r) != kOkDspRC )
    return rc;

  cmDspInst_t* ai0p = cmDspSysAllocInst(h,"AudioIn",     NULL,  1, 2);
  cmDspInst_t* ai1p = cmDspSysAllocInst(h,"AudioIn",     NULL,  1, 3);
  cmDspInst_t* ai2p = cmDspSysAllocInst(h,"AudioIn",     NULL,  1, 4);
  cmDspInst_t* ai3p = cmDspSysAllocInst(h,"AudioIn",     NULL,  1, 5);

  cmDspInst_t* eqLpf_0 =  useInputEqFl ? cmDspSysAllocInst(h,"BiQuadEq",NULL, 5, eqBypassFl, eqLpSymId,eqLpF0hz, eqLpQ, eqLpGain  ) : NULL; 
  cmDspInst_t* eqLpf_1 =  useInputEqFl ? cmDspSysAllocInst(h,"BiQuadEq",NULL, 5, eqBypassFl, eqLpSymId,eqLpF0hz, eqLpQ, eqLpGain  ) : NULL; 
  cmDspInst_t* eqLpf_2 =  useInputEqFl ? cmDspSysAllocInst(h,"BiQuadEq",NULL, 5, eqBypassFl, eqLpSymId,eqLpF0hz, eqLpQ, eqLpGain  ) : NULL; 
  cmDspInst_t* eqLpf_3 =  useInputEqFl ? cmDspSysAllocInst(h,"BiQuadEq",NULL, 5, eqBypassFl, eqLpSymId,eqLpF0hz, eqLpQ, eqLpGain  ) : NULL; 

  cmDspInst_t* eqBpf_0 =  useInputEqFl ? cmDspSysAllocInst(h,"BiQuadEq",NULL, 5, eqBypassFl, eqBpSymId,eqBpF0hz, eqBpQ, eqBpGain  ) : NULL; 
  cmDspInst_t* eqBpf_1 =  useInputEqFl ? cmDspSysAllocInst(h,"BiQuadEq",NULL, 5, eqBypassFl, eqBpSymId,eqBpF0hz, eqBpQ, eqBpGain  ) : NULL; 
  cmDspInst_t* eqBpf_2 =  useInputEqFl ? cmDspSysAllocInst(h,"BiQuadEq",NULL, 5, eqBypassFl, eqBpSymId,eqBpF0hz, eqBpQ, eqBpGain  ) : NULL; 
  cmDspInst_t* eqBpf_3 =  useInputEqFl ? cmDspSysAllocInst(h,"BiQuadEq",NULL, 5, eqBypassFl, eqBpSymId,eqBpF0hz, eqBpQ, eqBpGain  ) : NULL; 

  cmDspInst_t* eqHpf_0 =  useInputEqFl ? cmDspSysAllocInst(h,"BiQuadEq",NULL, 5, eqBypassFl, eqHpSymId,eqHpF0hz, eqHpQ, eqHpGain  ) : NULL; 
  cmDspInst_t* eqHpf_1 =  useInputEqFl ? cmDspSysAllocInst(h,"BiQuadEq",NULL, 5, eqBypassFl, eqHpSymId,eqHpF0hz, eqHpQ, eqHpGain  ) : NULL; 
  cmDspInst_t* eqHpf_2 =  useInputEqFl ? cmDspSysAllocInst(h,"BiQuadEq",NULL, 5, eqBypassFl, eqHpSymId,eqHpF0hz, eqHpQ, eqHpGain  ) : NULL; 
  cmDspInst_t* eqHpf_3 =  useInputEqFl ? cmDspSysAllocInst(h,"BiQuadEq",NULL, 5, eqBypassFl, eqHpSymId,eqHpF0hz, eqHpQ, eqHpGain  ) : NULL; 

  double eqMixGain = 1.0/3.0;
  cmDspInst_t* eqMx_0 = useInputEqFl ? cmDspSysAllocInst( h, "AMix",      NULL, 4, 3, eqMixGain, eqMixGain, eqMixGain) : NULL;
  cmDspInst_t* eqMx_1 = useInputEqFl ? cmDspSysAllocInst( h, "AMix",      NULL, 4, 3, eqMixGain, eqMixGain, eqMixGain) : NULL;
  cmDspInst_t* eqMx_2 = useInputEqFl ? cmDspSysAllocInst( h, "AMix",      NULL, 4, 3, eqMixGain, eqMixGain, eqMixGain) : NULL;
  cmDspInst_t* eqMx_3 = useInputEqFl ? cmDspSysAllocInst( h, "AMix",      NULL, 4, 3, eqMixGain, eqMixGain, eqMixGain) : NULL;


  cmDspInst_t* mx0p = cmDspSysAllocInst( h, "AMix",      NULL, 3, 2, 0.5, 0.5);
  cmDspInst_t* mx1p = cmDspSysAllocInst( h, "AMix",      NULL, 3, 2, 0.5, 0.5);

  cmDspInst_t* ci0p = NULL;
  cmDspInst_t* ci1p = NULL;

  if( useInCompFl )
  {
    ci0p = cmDspSysAllocInst(h,"Compressor",  NULL,  8, cmpBypassFl, cmpThreshDb, cmpRatio_num, cmpAtkMs, cmpRlsMs, cmpMakeup, cmpWndMs, cmpWndMaxMs ); 
    ci1p = cmDspSysAllocInst(h,"Compressor",  NULL,  8, cmpBypassFl, cmpThreshDb, cmpRatio_num, cmpAtkMs, cmpRlsMs, cmpMakeup, cmpWndMs, cmpWndMaxMs ); 
  }

  cmDspInst_t* tlp  = cmDspSysAllocInst(h,"TimeLine",    "tl",  2, r.tlFn, r.tlPrefixPath );
  cmDspInst_t* scp  = cmDspSysAllocInst(h,"Score",       "sc",  1, r.scFn );
  cmDspInst_t* php  = cmDspSysAllocInst(h,"Phasor",      NULL,  1, cmDspSysSampleRate(h) );
  cmDspInst_t* wtp  = cmDspSysAllocInst(h,"WaveTable",   NULL,  4, wtSmpCnt, wtInitMode, NULL, wtLoopCnt );
  cmDspInst_t* pts  = cmDspSysAllocInst(h,"PortToSym",   NULL,  2, "on", "off" );
  //cmDspInst_t* mip  = cmDspSysAllocInst(h,"MidiIn",      NULL,  0 );

  cmDspInst_t* mfp  = cmDspSysAllocInst(h,"MidiFilePlay",NULL,  0 );
  cmDspInst_t* nmp  = cmDspSysAllocInst(h,"NanoMap",     NULL,  0 );
  cmDspInst_t* mop  = cmDspSysAllocInst(h,"MidiOut",     NULL,  2, r.midiDevice,r.midiOutPort);
  cmDspInst_t* mo2p = cmDspSysAllocInst(h,"MidiOut",     NULL,  2, r.midiDevice,r.midiOutPort2);
  cmDspInst_t* sfp  = cmDspSysAllocInst(h,"ScFol",       NULL,  1, r.scFn, sfBufCnt, sfMaxWndCnt, sfMinVel, sfEnaMeasFl );
  cmDspInst_t* amp  = cmDspSysAllocInst(h,"ActiveMeas",  NULL,  1, 100 );
  cmDspInst_t* rpp  = cmDspSysAllocInst(h,"RecdPlay",    NULL,  6, 2, r.scFn, recdPlayInitAllocSecs, recdPlayMaxLaSecs, recdPlayCurLaSecs, recdPlayFadeRateDbPerSec);
  cmDspInst_t* modp = cmDspSysAllocInst(h,"ScMod",       NULL,  2, r.modFn, "m1" );
  cmDspInst_t* modr = cmDspSysAllocInst(h,"ScMod",       NULL,  2, r.modFn, "m1" );
 
  unsigned   preGrpSymId     = cmDspSysPresetRegisterGroup(h,"tl");
  unsigned   cmpPreGrpSymId  = cmDspSysPresetRegisterGroup(h,"tl_cmp"); 

  cmDspTlXform_t c0,c1,c2,c3;

  cmDspSysNewPage(h,"Controls-0");
  _cmDspSys_TlXformChain(h, &c0, preGrpSymId, cmpPreGrpSymId, modp, 0, 0 );

  if( useChain1Fl )
  {
    cmDspSysNewPage(h,"Controls-1");
    _cmDspSys_TlXformChain(h, &c1, preGrpSymId, cmpPreGrpSymId, modp, 1, 1 );
  }

  cmDspInst_t* mix0 = NULL;
  cmDspInst_t* mix1 = NULL;

  if( fragFl )
  {
    cmDspSysNewPage(h,"Ctl-R/P-0");
    _cmDspSys_TlXformChain(h, &c2, preGrpSymId, cmpPreGrpSymId, modr, 2, 0 );

    cmDspSysNewPage(h,"Ctl-R/P-1");
    _cmDspSys_TlXformChain(h, &c3, preGrpSymId, cmpPreGrpSymId, modr, 3, 1 );

    mix0 = cmDspSysAllocInst(h,"AMix",        NULL,   3, 2, 1.0, 1.0 );
    mix1 = cmDspSysAllocInst(h,"AMix",        NULL,   3, 2, 1.0, 1.0 );
  }

  if( useInCompFl )
  {
    cmDspSysNewPage(h,"InComp");

    cmDspInst_t* cmp0_byp   = cmDspSysAllocCheckP(  h, cmpPreGrpSymId, NULL, ("0-Bypass"), 0.0 );
    cmDspInst_t* cmp0_igain = cmDspSysAllocScalarP( h, cmpPreGrpSymId, NULL, ("0-In Gain"),  0.0,   10.0, 0.1, cmpInGain);
    cmDspInst_t* cmp0_thr   = cmDspSysAllocScalarP( h, cmpPreGrpSymId, NULL, ("0-ThreshDb"), -100.0, 0.0, 0.1, cmpThreshDb);
    cmDspInst_t* cmp0_rat   = cmDspSysAllocScalarP( h, cmpPreGrpSymId, NULL, ("0-Ratio"),    0.1, 100, 0.1, cmpRatio_num);
    cmDspInst_t* cmp0_atk   = cmDspSysAllocScalarP( h, cmpPreGrpSymId, NULL, ("0-Atk Ms"),   0.0, 1000.0, 0.1, cmpAtkMs);
    cmDspInst_t* cmp0_rls   = cmDspSysAllocScalarP( h, cmpPreGrpSymId, NULL, ("0-Rls Ms"),   0.0, 1000.0, 0.1, cmpRlsMs);
    cmDspInst_t* cmp0_mkup  = cmDspSysAllocScalarP( h, cmpPreGrpSymId, NULL, ("0-Makeup"),   0.0, 10.0,   0.01, cmpMakeup);
    cmDspInst_t* cmp0_wnd   = cmDspSysAllocScalarP( h, cmpPreGrpSymId, NULL, ("0-Wnd Ms"),   1.0, cmpWndMaxMs, 1.0, cmpWndMs );
    cmDspInst_t* cmp0_mtr   = cmDspSysAllocInst(h,"Meter",("0-Env"), 3, 0.0, 0.0, 1.0);

    cmDspSysInstallCb(h, cmp0_byp,  "out", ci0p, "bypass", NULL );
    cmDspSysInstallCb(h, cmp0_igain,"val", ci0p, "igain", NULL );
    cmDspSysInstallCb(h, cmp0_thr,  "val", ci0p, "thr", NULL );
    cmDspSysInstallCb(h, cmp0_rat,  "val", ci0p, "ratio", NULL );
    cmDspSysInstallCb(h, cmp0_atk,  "val", ci0p, "atk", NULL );
    cmDspSysInstallCb(h, cmp0_rls,  "val", ci0p, "rls", NULL );
    cmDspSysInstallCb(h, cmp0_mkup, "val", ci0p, "ogain", NULL );
    cmDspSysInstallCb(h, cmp0_wnd,  "val", ci0p, "wnd", NULL );
    cmDspSysInstallCb(h, ci0p,      "env", cmp0_mtr, "in", NULL );

    cmDspSysNewColumn(h,0);
    cmDspInst_t* cmp1_byp   = cmDspSysAllocCheckP(  h, cmpPreGrpSymId, NULL, ("1-Bypass"), 0.0 );
    cmDspInst_t* cmp1_igain = cmDspSysAllocScalarP( h, cmpPreGrpSymId, NULL, ("1-In Gain"),  0.0,   10.0, 0.1, cmpInGain);
    cmDspInst_t* cmp1_thr   = cmDspSysAllocScalarP( h, cmpPreGrpSymId, NULL, ("1-ThreshDb"), -100.0, 0.0, 0.1, cmpThreshDb);
    cmDspInst_t* cmp1_rat   = cmDspSysAllocScalarP( h, cmpPreGrpSymId, NULL, ("1-Ratio"),    0.1, 100, 0.1, cmpRatio_num);
    cmDspInst_t* cmp1_atk   = cmDspSysAllocScalarP( h, cmpPreGrpSymId, NULL, ("1-Atk Ms"),   0.0, 1000.0, 0.1, cmpAtkMs);
    cmDspInst_t* cmp1_rls   = cmDspSysAllocScalarP( h, cmpPreGrpSymId, NULL, ("1-Rls Ms"),   0.0, 1000.0, 0.1, cmpRlsMs);
    cmDspInst_t* cmp1_mkup  = cmDspSysAllocScalarP( h, cmpPreGrpSymId, NULL, ("1-Makeup"),   0.0, 10.0,   0.01, cmpMakeup);
    cmDspInst_t* cmp1_wnd   = cmDspSysAllocScalarP( h, cmpPreGrpSymId, NULL, ("1-Wnd Ms"),   1.0, cmpWndMaxMs, 1.0, cmpWndMs );
    cmDspInst_t* cmp1_mtr   = cmDspSysAllocInst(h,"Meter",("1-Env"), 3, 0.0, 0.0, 1.0);

    cmDspSysInstallCb(h, cmp1_byp,  "out", ci1p, "bypass", NULL );
    cmDspSysInstallCb(h, cmp1_igain,"val", ci1p, "igain", NULL );
    cmDspSysInstallCb(h, cmp1_thr,  "val", ci1p, "thr", NULL );
    cmDspSysInstallCb(h, cmp1_rat,  "val", ci1p, "ratio", NULL );
    cmDspSysInstallCb(h, cmp1_atk,  "val", ci1p, "atk", NULL );
    cmDspSysInstallCb(h, cmp1_rls,  "val", ci1p, "rls", NULL );
    cmDspSysInstallCb(h, cmp1_mkup, "val", ci1p, "ogain", NULL );
    cmDspSysInstallCb(h, cmp1_wnd,  "val", ci1p, "wnd", NULL );
    cmDspSysInstallCb(h, ci1p,      "env", cmp1_mtr, "in", NULL );

  }

  cmDspInst_t* apf0 =  cmDspSysAllocInst(h,"BiQuadEq",NULL, 5, apfBypassFl, apfModeSymId, apfF0hz+0, apfQ, apfGain  ); 
  cmDspInst_t* apf1 =  cmDspSysAllocInst(h,"BiQuadEq",NULL, 5, apfBypassFl, apfModeSymId, apfF0hz+100.0, apfQ, apfGain  ); 
  cmDspInst_t* apf2 =  cmDspSysAllocInst(h,"BiQuadEq",NULL, 5, apfBypassFl, apfModeSymId, apfF0hz+200.0, apfQ, apfGain  ); 
  cmDspInst_t* apf3 =  cmDspSysAllocInst(h,"BiQuadEq",NULL, 5, apfBypassFl, apfModeSymId, apfF0hz+300.0, apfQ, apfGain  ); 

  cmDspInst_t* ao0p = cmDspSysAllocInst(h,"AudioOut",    NULL,   1, 0 );
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
  //cmDspInst_t* prtb = cmDspSysAllocInst(h,"Button", "print",   2, kButtonDuiId, 1.0 );
  //cmDspInst_t* qtb  = cmDspSysAllocInst(h,"Button", "quiet",   2, kButtonDuiId, 1.0 );
  cmDspInst_t* mutm = cmDspSysAllocInst(h,"Checkbox","main",   1, "main","on","off",1.0,0.0,1.0 );
  cmDspInst_t* mutr = cmDspSysAllocInst(h,"Checkbox","frag",   1, "frag","on","off",1.0,0.0,1.0 );
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
  cmDspInst_t* igain0 = cmDspSysAllocInst(h,"Scalar", "In Gain-0",    5, kNumberDuiId, 0.0,   100.0,0.01,   1.0 );  
  cmDspInst_t* igain1 = cmDspSysAllocInst(h,"Scalar", "In Gain-1",    5, kNumberDuiId, 0.0,   100.0,0.01,   1.0 );  
  cmDspInst_t* igain2 = cmDspSysAllocInst(h,"Scalar", "In Gain-2",    5, kNumberDuiId, 0.0,   100.0,0.01,   1.0 );  
  cmDspInst_t* igain3 = cmDspSysAllocInst(h,"Scalar", "In Gain-3",    5, kNumberDuiId, 0.0,   100.0,0.01,   1.0 );  

  cmDspInst_t* lasecs = cmDspSysAllocInst(h,"Scalar", "LA Secs",      5, kNumberDuiId, 0.0,   recdPlayMaxLaSecs,0.01,   recdPlayCurLaSecs );  
  cmDspInst_t* dbpsec = cmDspSysAllocInst(h,"Scalar", "Fade dBpSec",  5, kNumberDuiId, 0.0,   24.0, 0.01, recdPlayFadeRateDbPerSec);
  cmDspInst_t* apfByp = cmDspSysAllocCheck(  h, "APF-Bypass", 0.0 );



  cmDspSysNewColumn(h,0);
  cmDspInst_t* ogain0 = cmDspSysAllocInst(h,"Scalar", "Out Gain-0",   5, kNumberDuiId, 0.0,   10.0,0.01,   1.0 );  
  cmDspInst_t* ogain1 = cmDspSysAllocInst(h,"Scalar", "Out Gain-1",   5, kNumberDuiId, 0.0,   10.0,0.01,   1.0 );  
  cmDspInst_t* ogain2 = cmDspSysAllocInst(h,"Scalar", "Out Gain-2",   5, kNumberDuiId, 0.0,   10.0,0.01,   1.0 );  
  cmDspInst_t* ogain3 = cmDspSysAllocInst(h,"Scalar", "Out Gain-3",   5, kNumberDuiId, 0.0,   10.0,0.01,   1.0 );  

  //cmDspInst_t* scLoc = cmDspSysAllocInst(h,"Scalar", "Sc Loc",   5, kNumberDuiId, 0.0,   3000.0, 1.0,   0.0 );  

  // Audio file recording
  cmDspInst_t* recdGain= cmDspSysAllocInst(h,"Scalar", "Recd Gain",  5, kNumberDuiId, 0.0,   100.0,0.01, 1.5 );  
  cmDspInst_t* recdChk = cmDspSysAllocInst(h,"Button", "Record",     2, kCheckDuiId, 0.0 );
  cmDspInst_t* recdPtS = cmDspSysAllocInst(h,"GateToSym", NULL,      2, cmSymTblRegisterStaticSymbol(cmDspSysSymbolTable(h),"open"),cmSymTblRegisterStaticSymbol(cmDspSysSymbolTable(h),"close"));
  cmDspInst_t* afop    = cmDspSysAllocInst(h,"AudioFileOut",NULL,    2, r.recordDir,2);
  cmDspInst_t* mi0p    = cmDspSysAllocInst(h,"AMeter","In 0",  0);
  cmDspInst_t* mi1p    = cmDspSysAllocInst(h,"AMeter","In 1",  0);
  cmDspInst_t* mi2p    = cmDspSysAllocInst(h,"AMeter","In 2",  0);
  cmDspInst_t* mi3p    = cmDspSysAllocInst(h,"AMeter","In 3",  0);


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



  if( useInputEqFl ) cmDspSysNewPage(h,"In EQ");
  cmDspInst_t* eqLpByp0  = useInputEqFl ? cmDspSysAllocCheck( h,"Eq-LPF-Bypass-0", 0.0 ) : NULL;
  cmDspInst_t* eqLpMode0 = useInputEqFl ? cmDspSysAllocInst(  h,"MsgList","Eq-LPF-Mode-0",  3, "biQuadEqMode", NULL, 1) : NULL;
  cmDspInst_t* eqLpFc0   = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-LPF-Hz-0",    5, kNumberDuiId, 0.0, 20000.0, 0.5,  eqLpF0hz ) : NULL;  
  cmDspInst_t* eqLpQ0    = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-LPF-Q-0",     5, kNumberDuiId, 0.0,  100.0,  0.01,  eqLpQ ) : NULL;  
  cmDspInst_t* eqLpGain0 = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-LPF-Gain-0",  5, kNumberDuiId, -100.0, 100.0, 0.5,  eqLpGain ) : NULL;  

  cmDspInst_t* eqBpByp0  = useInputEqFl ? cmDspSysAllocCheck( h,"Eq-BPF-Bypass-0", 0.0 ) : NULL;
  cmDspInst_t* eqBpMode0 = useInputEqFl ? cmDspSysAllocInst(  h,"MsgList","Eq-BPF-Mode-0",  3, "biQuadEqMode", NULL, 3) : NULL;
  cmDspInst_t* eqBpFc0   = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-BPF-Hz-0",    5, kNumberDuiId, 0.0, 20000.0, 0.5,  eqBpF0hz ) : NULL;  
  cmDspInst_t* eqBpQ0    = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-BPF-Q-0",     5, kNumberDuiId, 0.0,  100.0,  0.01,  eqBpQ ) : NULL;  
  cmDspInst_t* eqBpGain0 = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-BPF-Gain-0",  5, kNumberDuiId, -100.0, 100.0, 0.5,  eqBpGain ) : NULL;  

  cmDspInst_t* eqHpByp0  = useInputEqFl ? cmDspSysAllocCheck( h,"Eq-HPF-Bypass-0", 0.0 ) : NULL;
  cmDspInst_t* eqHpMode0 = useInputEqFl ? cmDspSysAllocInst(  h,"MsgList","Eq-HPF-Mode-0",  3, "biQuadEqMode", NULL, 2) : NULL;
  cmDspInst_t* eqHpFc0   = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-HPF-Hz-0",    5, kNumberDuiId, 0.0, 20000.0, 0.5,  eqHpF0hz ) : NULL;  
  cmDspInst_t* eqHpQ0    = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-HPF-Q-0",     5, kNumberDuiId, 0.0,  100.0,  0.01,  eqHpQ ) : NULL;  
  cmDspInst_t* eqHpGain0 = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-HPF-Gain-0",  5, kNumberDuiId, -100.0, 100.0, 0.5,  eqHpGain ) : NULL;  

  if( useInputEqFl ) cmDspSysNewColumn(h,0);
  cmDspInst_t* eqLpByp1  = useInputEqFl ? cmDspSysAllocCheck( h,"Eq-LPF-Bypass-1", 0.0 ) : NULL;
  cmDspInst_t* eqLpMode1 = useInputEqFl ? cmDspSysAllocInst(  h,"MsgList","Eq-LPF-Mode-1",  3, "biQuadEqMode", NULL, 1) : NULL;
  cmDspInst_t* eqLpFc1   = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-LPF-Hz-1",    5, kNumberDuiId, 0.0, 20000.0, 0.5,  eqLpF0hz ) : NULL;  
  cmDspInst_t* eqLpQ1    = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-LPF-Q-1",     5, kNumberDuiId, 0.0,  100.0,  0.01,  eqLpQ ) : NULL;  
  cmDspInst_t* eqLpGain1 = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-LPF-Gain-1",  5, kNumberDuiId, -100.0, 100.0, 0.5,  eqLpGain ) : NULL;  

  cmDspInst_t* eqBpByp1  = useInputEqFl ? cmDspSysAllocCheck( h,"Eq-BPF-Bypass-1", 0.0 ) : NULL;
  cmDspInst_t* eqBpMode1 = useInputEqFl ? cmDspSysAllocInst(  h,"MsgList","Eq-BPF-Mode-1",  3, "biQuadEqMode", NULL, 3) : NULL;
  cmDspInst_t* eqBpFc1   = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-BPF-Hz-1",    5, kNumberDuiId, 0.0, 20000.0, 0.5,  eqBpF0hz ) : NULL;  
  cmDspInst_t* eqBpQ1    = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-BPF-Q-1",     5, kNumberDuiId, 0.0,  100.0,  0.01,  eqBpQ ) : NULL;  
  cmDspInst_t* eqBpGain1 = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-BPF-Gain-1",  5, kNumberDuiId, -100.0, 100.0, 0.5,  eqBpGain ) : NULL;  

  cmDspInst_t* eqHpByp1  = useInputEqFl ? cmDspSysAllocCheck( h,"Eq-HPF-Bypass-1", 0.0 ) : NULL;
  cmDspInst_t* eqHpMode1 = useInputEqFl ? cmDspSysAllocInst(  h,"MsgList","Eq-HPF-Mode-1",  3, "biQuadEqMode", NULL, 2) : NULL;
  cmDspInst_t* eqHpFc1   = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-HPF-Hz-1",    5, kNumberDuiId, 0.0, 20000.0, 0.5,  eqHpF0hz ) : NULL;  
  cmDspInst_t* eqHpQ1    = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-HPF-Q-1",     5, kNumberDuiId, 0.0,  100.0,  0.01,  eqHpQ ) : NULL;  
  cmDspInst_t* eqHpGain1 = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-HPF-Gain-1",  5, kNumberDuiId, -100.0, 100.0, 0.5,  eqHpGain ) : NULL;  


  if( useInputEqFl ) cmDspSysNewColumn(h,0);
  cmDspInst_t* eqLpByp2  = useInputEqFl ? cmDspSysAllocCheck( h,"Eq-LPF-Bypass-2", 0.0 ) : NULL;
  cmDspInst_t* eqLpMode2 = useInputEqFl ? cmDspSysAllocInst(  h,"MsgList","Eq-LPF-Mode-2",  3, "biQuadEqMode", NULL, 1) : NULL;
  cmDspInst_t* eqLpFc2   = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-LPF-Hz-2",    5, kNumberDuiId, 0.0, 20000.0, 0.5,  eqLpF0hz ) : NULL;  
  cmDspInst_t* eqLpQ2    = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-LPF-Q-2",     5, kNumberDuiId, 0.0,  100.0,  0.01,  eqLpQ ) : NULL;  
  cmDspInst_t* eqLpGain2 = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-LPF-Gain-2",  5, kNumberDuiId, -100.0, 100.0, 0.5,  eqLpGain ) : NULL;  

  cmDspInst_t* eqBpByp2  = useInputEqFl ? cmDspSysAllocCheck( h,"Eq-BPF-Bypass-2", 0.0 ) : NULL;
  cmDspInst_t* eqBpMode2 = useInputEqFl ? cmDspSysAllocInst(  h,"MsgList","Eq-BPF-Mode-2",  3, "biQuadEqMode", NULL, 3) : NULL;
  cmDspInst_t* eqBpFc2   = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-BPF-Hz-2",    5, kNumberDuiId, 0.0, 20000.0, 0.5,  eqBpF0hz ) : NULL;  
  cmDspInst_t* eqBpQ2    = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-BPF-Q-2",     5, kNumberDuiId, 0.0,  100.0,  0.01,  eqBpQ ) : NULL;  
  cmDspInst_t* eqBpGain2 = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-BPF-Gain-2",  5, kNumberDuiId, -100.0, 100.0, 0.5,  eqBpGain ) : NULL;  

  cmDspInst_t* eqHpByp2  = useInputEqFl ? cmDspSysAllocCheck( h,"Eq-HPF-Bypass-2", 0.0 ) : NULL;
  cmDspInst_t* eqHpMode2 = useInputEqFl ? cmDspSysAllocInst(  h,"MsgList","Eq-HPF-Mode-2",  3, "biQuadEqMode", NULL, 2) : NULL;
  cmDspInst_t* eqHpFc2   = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-HPF-Hz-2",    5, kNumberDuiId, 0.0, 20000.0, 0.5,  eqHpF0hz ) : NULL;  
  cmDspInst_t* eqHpQ2    = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-HPF-Q-2",     5, kNumberDuiId, 0.0,  100.0,  0.01,  eqHpQ ) : NULL;  
  cmDspInst_t* eqHpGain2 = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-HPF-Gain-2",  5, kNumberDuiId, -100.0, 100.0, 0.5,  eqHpGain ) : NULL;  

  if( useInputEqFl ) cmDspSysNewColumn(h,0);
  cmDspInst_t* eqLpByp3  = useInputEqFl ? cmDspSysAllocCheck( h,"Eq-LPF-Bypass-3", 0.0 ) : NULL;
  cmDspInst_t* eqLpMode3 = useInputEqFl ? cmDspSysAllocInst(  h,"MsgList","Eq-LPF-Mode-3", 3, "biQuadEqMode", NULL, 1) : NULL;
  cmDspInst_t* eqLpFc3   = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-LPF-Hz-3",    5, kNumberDuiId, 0.0, 20000.0, 0.5,  eqLpF0hz ) : NULL;  
  cmDspInst_t* eqLpQ3    = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-LPF-Q-3",     5, kNumberDuiId, 0.0,  100.0,  0.01,  eqLpQ ) : NULL;  
  cmDspInst_t* eqLpGain3 = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-LPF-Gain-3",  5, kNumberDuiId, -300.0, 100.0, 0.5,  eqLpGain ) : NULL;  

  cmDspInst_t* eqBpByp3  = useInputEqFl ? cmDspSysAllocCheck( h,"Eq-BPF-Bypass-3", 0.0 ) : NULL;
  cmDspInst_t* eqBpMode3 = useInputEqFl ? cmDspSysAllocInst(  h,"MsgList","Eq-BPF-Mode-3", 3, "biQuadEqMode", NULL, 3) : NULL;
  cmDspInst_t* eqBpFc3   = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-BPF-Hz-3",    5, kNumberDuiId, 0.0, 20000.0, 0.5,  eqBpF0hz ) : NULL;  
  cmDspInst_t* eqBpQ3    = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-BPF-Q-3",     5, kNumberDuiId, 0.0,  100.0,  0.01,  eqBpQ ) : NULL;  
  cmDspInst_t* eqBpGain3 = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-BPF-Gain-3",  5, kNumberDuiId, -300.0, 100.0, 0.5,  eqBpGain ) : NULL;  

  cmDspInst_t* eqHpByp3  = useInputEqFl ? cmDspSysAllocCheck( h,"Eq-HPF-Bypass-3", 0.0 ) : NULL;
  cmDspInst_t* eqHpMode3 = useInputEqFl ? cmDspSysAllocInst(  h,"MsgList","Eq-HPF-Mode-3", 3, "biQuadEqMode", NULL, 2) : NULL;
  cmDspInst_t* eqHpFc3   = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-HPF-Hz-3",    5, kNumberDuiId, 0.0, 20000.0, 0.5,  eqHpF0hz ) : NULL;  
  cmDspInst_t* eqHpQ3    = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-HPF-Q-3",     5, kNumberDuiId, 0.0,  100.0,  0.01,  eqHpQ ) : NULL;  
  cmDspInst_t* eqHpGain3 = useInputEqFl ? cmDspSysAllocInst(  h,"Scalar", "Eq-HPF-Gain-3",  5, kNumberDuiId, -100.0, 100.0, 0.5,  eqHpGain ) : NULL;  

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

  cmDspSysConnectAudio( h, ai0p, "out", mi0p, "in");
  cmDspSysConnectAudio( h, ai1p, "out", mi1p, "in");
  cmDspSysConnectAudio( h, ai2p, "out", mi2p, "in");
  cmDspSysConnectAudio( h, ai3p, "out", mi3p, "in");

  if( useInputEqFl )
  {
    cmDspSysConnectAudio(h, ai0p,    "out", eqLpf_0, "in" );     // ain->eq
    cmDspSysConnectAudio(h, ai0p,    "out", eqBpf_0, "in" );
    cmDspSysConnectAudio(h, ai0p,    "out", eqHpf_0, "in" );
    cmDspSysConnectAudio(h, eqLpf_0, "out", eqMx_0,  "in-0");  // eq -> eqmix
    cmDspSysConnectAudio(h, eqBpf_0, "out", eqMx_0,  "in-1");
    cmDspSysConnectAudio(h, eqHpf_0, "out", eqMx_0,  "in-2");

    cmDspSysConnectAudio(h, ai1p,    "out", eqLpf_1, "in" );     // ain->eq
    cmDspSysConnectAudio(h, ai1p,    "out", eqBpf_1, "in" );
    cmDspSysConnectAudio(h, ai1p,    "out", eqHpf_1, "in" );
    cmDspSysConnectAudio(h, eqLpf_1, "out", eqMx_1,  "in-0");  // eq -> eqmix
    cmDspSysConnectAudio(h, eqBpf_1, "out", eqMx_1,  "in-1");
    cmDspSysConnectAudio(h, eqHpf_1, "out", eqMx_1,  "in-2");

    cmDspSysConnectAudio(h, ai2p,    "out", eqLpf_2, "in" );     // ain->eq
    cmDspSysConnectAudio(h, ai2p,    "out", eqBpf_2, "in" );
    cmDspSysConnectAudio(h, ai2p,    "out", eqHpf_2, "in" );
    cmDspSysConnectAudio(h, eqLpf_2, "out", eqMx_2,  "in-0");  // eq -> eqmix
    cmDspSysConnectAudio(h, eqBpf_2, "out", eqMx_2,  "in-1");
    cmDspSysConnectAudio(h, eqHpf_2, "out", eqMx_2,  "in-2");

    cmDspSysConnectAudio(h, ai3p,    "out", eqLpf_3, "in" );     // ain->eq
    cmDspSysConnectAudio(h, ai3p,    "out", eqBpf_3, "in" );
    cmDspSysConnectAudio(h, ai3p,    "out", eqHpf_3, "in" );
    cmDspSysConnectAudio(h, eqLpf_3, "out", eqMx_3,  "in-0");  // eq -> eqmix
    cmDspSysConnectAudio(h, eqBpf_3, "out", eqMx_3,  "in-1");
    cmDspSysConnectAudio(h, eqHpf_3, "out", eqMx_3,  "in-2");
  }

  cmDspSysConnectAudio(h, useInputEqFl ? eqMx_0 : ai0p, "out", mx0p, "in-0" );  // eqmix -> input mix
  cmDspSysConnectAudio(h, useInputEqFl ? eqMx_1 : ai1p, "out", mx1p, "in-0" );
  cmDspSysConnectAudio(h, useInputEqFl ? eqMx_2 : ai2p, "out", mx0p, "in-1" );
  cmDspSysConnectAudio(h, useInputEqFl ? eqMx_3 : ai3p, "out", mx1p, "in-1" );

  if( useWtFl )
  {
    cmDspSysConnectAudio(h, wtp,    "out",   au0Sw, "a-in-0" ); // wt  -> sw
    cmDspSysConnectAudio(h, mx0p,   "out",   au0Sw, "a-in-1" );      // ain -> sw
    //cmDspSysConnectAudio(h, ci0p,   "out",   au0Sw, "a-in-1" );
    cmDspSysConnectAudio(h, au0Sw,  "a-out", rpp,   "in-0");    // sw  -> rcdply
    cmDspSysConnectAudio(h, au0Sw,  "a-out", c0.kr0,"in"  );    // sw  -> kr
    //cmDspSysConnectAudio(h, au0Sw,  "a-out", c0.kr1,"in"  );    // sw  -> kr
    //cmDspSysConnectAudio(h, au0Sw,  "a-out",  mi0p,  "in" );    // sw  -> meter  
  }
  else
  {
    cmDspSysConnectAudio(h, mx0p,  "out", rpp,   "in-0");    // sw  -> rcdply


    if( useInCompFl )
    {
      cmDspSysConnectAudio(h, mx0p,   "out",   ci0p,   "in" ); // ain -> in compress
      cmDspSysConnectAudio(h, ci0p,   "out",   c0.kr0, "in" );
    }
    else
    {
      cmDspSysConnectAudio(h, mx0p,   "out",   c0.kr0, "in" ); // ain -> sw
    }

    //cmDspSysConnectAudio(h, mx0p,   "out",   mi0p,  "in" );     
  }

  if( fragFl )
  {
    cmDspSysConnectAudio(h, c0.cmp, "out",   mix0,  "in-0" );   // cmp -> mix 0
    cmDspSysConnectAudio(h, rpp,    "out-0", c2.kr0,"in" );
    //cmDspSysConnectAudio(h, rpp,    "out-0", c2.kr1,"in" );
    cmDspSysConnectAudio(h, c2.cmp, "out",   mix0,  "in-1");    // rpp -> mix 1
    cmDspSysConnectAudio(h, mix0,   "out",   apf0,  "in" );     // mix -> aout
    cmDspSysConnectAudio(h, apf0,  "out",   ao0p,  "in" );

  }
  else
  {
    cmDspSysConnectAudio(h, c0.cmp, "out", apf0, "in" );
    cmDspSysConnectAudio(h, c0.cmp, "out", apf2, "in" );
    cmDspSysConnectAudio(h, apf0,  "out", ao0p,  "in" );
    cmDspSysConnectAudio(h, apf2,  "out", ao2p,  "in" );

    //cmDspSysConnectAudio(h, wtp, "out", apf0, "in" );
  }


  if( useChain1Fl )
  {
    if( useWtFl )
    {
      cmDspSysConnectAudio(h, wtp,    "out",   au1Sw, "a-in-0" ); // wt  -> sw
      cmDspSysConnectAudio(h, mx1p,   "out",   au1Sw, "a-in-1" ); // ain -> sw
      //cmDspSysConnectAudio(h, ci1p,   "out",   au1Sw, "a-in-1" ); 
      cmDspSysConnectAudio(h, au1Sw,  "a-out", rpp,   "in-1");    // sw  -> rcdply
      cmDspSysConnectAudio(h, au1Sw,  "a-out", c1.kr0,"in"  );    // sw  -> kr
      //cmDspSysConnectAudio(h, au1Sw,  "a-out", c1.kr1,"in"  );    // sw  -> kr
      //cmDspSysConnectAudio(h, au1Sw,  "a-out",  mi1p,  "in" );    // sw  -> meter 
    }
    else
    {
      cmDspSysConnectAudio(h, mx1p,  "out", rpp,   "in-1");    // sw  -> rcdply

      if( useInCompFl )
      {
        cmDspSysConnectAudio(h, mx1p,   "out",   ci1p,   "in" ); // ain -> in compress
        cmDspSysConnectAudio(h, ci1p,   "out",   c1.kr0, "in" );
      }
      else
      {
        cmDspSysConnectAudio(h, mx1p,   "out",   c1.kr0, "in" ); // ain -> sw
      }

      //cmDspSysConnectAudio(h, mx1p,   "out",   mi1p,  "in" );
    }

    if( fragFl )
    {
      cmDspSysConnectAudio(h, c1.cmp, "out",   mix1,  "in-0" );   // cmp -> mix 0
      cmDspSysConnectAudio(h, rpp,    "out-1", c3.kr0, "in" );
      //cmDspSysConnectAudio(h, rpp,    "out-1", c3.kr1, "in" );
      cmDspSysConnectAudio(h, c3.cmp, "out",   mix1,   "in-1");    // rpp -> mix 1
      cmDspSysConnectAudio(h, mix1,   "out",   apf1,   "in" );     // mix -> aout
      cmDspSysConnectAudio(h, apf1,  "out",   ao1p,   "in" );

    }
    else
    {
      cmDspSysConnectAudio(h, c1.cmp, "out",   apf1,  "in" );   // cmp -> mix 0
      cmDspSysConnectAudio(h, c1.cmp, "out",   apf3,  "in" );   // cmp -> mix 0
      cmDspSysConnectAudio(h, apf1,  "out",   ao1p,   "in" );
      cmDspSysConnectAudio(h, apf3,  "out",   ao3p,   "in" );
      
      //cmDspSysConnectAudio(h, wtp, "out", apf1, "in" );

    }
  }

  cmDspSysConnectAudio(h, c0.cmp, "out", afop, "in0" );    // comp -> audio_file_out
  cmDspSysConnectAudio(h, c1.cmp, "out", afop, "in1" );

  //cmDspSysConnectAudio(h, ai0p, "out", afop, "in0" );    // comp -> audio_file_out
  //cmDspSysConnectAudio(h, ai1p, "out", afop, "in1" );

  //cmDspSysConnectAudio(h, ai0p, "out", ao2p, "in" );     // direct through from input to 
  //cmDspSysConnectAudio(h, ai1p, "out", ao3p, "in" );     //    output chs 2&3




  cmDspSysInstallCb( h, clrBtn, "sym",    amp, "cmd",   NULL ); // clear active meas.
  cmDspSysInstallCb( h, prtBtn, "sym",    amp, "cmd",   NULL ); // print active meas
  cmDspSysInstallCb( h, prtBtn, "sym",    scp, "cmd",   NULL ); 
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

  if( useChain1Fl )
  {
    cmDspSysInstallCb( h, amp,    "scloc",  c1.achan,   "trig", NULL );
  }

  if( fragFl )
  {
    cmDspSysInstallCb( h, amp,    "scloc",  c2.achan,   "trig", NULL ); 
    cmDspSysInstallCb( h, amp,    "scloc",  c3.achan,   "trig", NULL );
  }

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
  cmDspSysInstallCb(h, pts, "on",      modr,  "cmd",   NULL );
  cmDspSysInstallCb(h, pts, "on",      rpp,   "cmd",   NULL );
  cmDspSysInstallCb(h, onb, "sym",     amCmd, "rewind",NULL );
  cmDspSysInstallCb(h, onb, "out",     c0.achan,"reset",  NULL );
  if( useChain1Fl )
  {
    cmDspSysInstallCb(h, onb, "out",     c1.achan,"reset",  NULL );
  }

  // stop connections
  cmDspSysInstallCb(h, wtp,  "done",offb,"in",  NULL ); // 'done' from WT simulates pressing Stop btn.
  cmDspSysInstallCb(h, tlp,  "mfn", pts, "off", NULL ); // Prevents WT start on new audio file from TL.
  cmDspSysInstallCb(h, offb, "sym", mfp, "sel", NULL ); 
  cmDspSysInstallCb(h, offb, "sym", pts, "off", NULL );
  cmDspSysInstallCb(h, pts,  "off", wtp, "cmd", NULL );
  cmDspSysInstallCb(h, pts,  "off", modp,"cmd", NULL );
  cmDspSysInstallCb(h, pts,  "off", modr,"cmd", NULL );
  cmDspSysInstallCb(h, offb, "sym", mop, "reset", NULL );
  cmDspSysInstallCb(h, offb, "sym", mo2p, "reset", NULL );


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
  cmDspSysInstallCb(h, scp, "sel",    modr,"reset", NULL );
  cmDspSysInstallCb(h, scp, "sel",    rpp, "initIdx", NULL );
  cmDspSysInstallCb(h, scp, "sel",    prp, "in", NULL );

  // NOTE: THIS IS A DUPLICATE OF THE scp.sel CONNECTIONS
  /*
  cmDspSysInstallCb(h, scLoc, "val",    sfp, "index",  NULL );
  cmDspSysInstallCb(h, scLoc, "val",    modp,"reset", NULL );
  cmDspSysInstallCb(h, scLoc, "val",    modr,"reset", NULL );
  cmDspSysInstallCb(h, scLoc, "val",    rpp, "initIdx", NULL );
  cmDspSysInstallCb(h, scLoc, "val",    prp, "in", NULL );
  */

  //cmDspSysInstallCb(h, reload,"out",  modp, "reload", NULL );


  // MIDI file player to score follower
  cmDspSysInstallCb(h, mfp,  "smpidx",  siRt, "f-in",NULL );
  cmDspSysInstallCb(h, siRt, "f-out-1", sfp,  "smpidx",NULL ); 
  // leave siRt.f-out-1 unconnected because it should be ignored in 'simulate mode'

  cmDspSysInstallCb(h, mfp,  "d1",      d1Rt, "f-in",  NULL );
  cmDspSysInstallCb(h, d1Rt, "f-out-1", sfp,  "d1",    NULL );
  cmDspSysInstallCb(h, d1Rt, "f-out-1", nmp,  "d1",    NULL );
  cmDspSysInstallCb(h, nmp,   "d1",     mop,  "d1",    NULL );
  cmDspSysInstallCb(h, nmp,   "d1",     mo2p,  "d1",    NULL );

  cmDspSysInstallCb(h, mfp,  "d0",      d0Rt,  "f-in", NULL );
  cmDspSysInstallCb(h, d0Rt, "f-out-1", sfp,   "d0",   NULL );
  cmDspSysInstallCb(h, d0Rt, "f-out-1", nmp,  "d0",   NULL );
  cmDspSysInstallCb(h, nmp,  "d0",      mop,  "d0",   NULL );
  cmDspSysInstallCb(h, nmp,  "d0",      mo2p,  "d0",   NULL );

  cmDspSysInstallCb(h, mfp, "status",   stRt, "f-in",  NULL );
  cmDspSysInstallCb(h, stRt, "f-out-1", sfp,  "status",NULL );
  cmDspSysInstallCb(h, stRt, "f-out-1", nmp,  "status",NULL );
  cmDspSysInstallCb(h, nmp,  "status",  mop,  "status",NULL );
  cmDspSysInstallCb(h, nmp,  "status",  mo2p,  "status",NULL );


  // MIDI input port
  //cmDspSysInstallCb(h, mip, "smpidx", sfp, "smpidx", NULL );
  //cmDspSysInstallCb(h, mip, "d1",     sfp, "d1",     NULL );
  //cmDspSysInstallCb(h, mip, "d0",     sfp, "d0",     NULL );
  //cmDspSysInstallCb(h, mip, "status", sfp, "status", NULL );

  // score follower to recd_play,modulator and printers
  //cmDspSysInstallCb(h, sfp, "out",     rpp,     "index", NULL );
  cmDspSysInstallCb(h, sfp, "out",     modp,    "index", NULL );
  //cmDspSysInstallCb(h, sfp, "out",     modr,    "index", NULL );
  cmDspSysInstallCb(h, sfp, "recent",  prp,     "in",  NULL );  // report 'recent' but only act on 'max' loc index

  //cmDspSysInstallCb(h, prtb, "sym", sfp, "cmd", NULL );
  //cmDspSysInstallCb(h, qtb,  "sym", sfp, "cmd", NULL );


  cmDspSysInstallCb(   h, lasecs, "val", rpp, "curla", NULL ); // recd/play control
  cmDspSysInstallCb(   h, dbpsec, "val", rpp, "frate", NULL );

  cmDspSysInstallCb(h, igain0, "val", ai0p, "gain", NULL );   // input gain control
  cmDspSysInstallCb(h, igain1, "val", ai1p, "gain", NULL );
  cmDspSysInstallCb(h, igain2, "val", ai2p, "gain", NULL );   // input gain control
  cmDspSysInstallCb(h, igain3, "val", ai3p, "gain", NULL );

  if( fragFl )
  {
    cmDspSysInstallCb(h, mutm,   "out", mix0, "gain-0", NULL );
    cmDspSysInstallCb(h, mutm,   "out", mix1, "gain-0", NULL );
    cmDspSysInstallCb(h, mutr,   "out", mix0, "gain-1", NULL );
    cmDspSysInstallCb(h, mutr,   "out", mix1, "gain-1", NULL );
  }

  cmDspSysInstallCb(h, apfByp, "out", apf0, "bypass", NULL );   // APF bypass
  cmDspSysInstallCb(h, apfByp, "out", apf1, "bypass", NULL );   // 
  cmDspSysInstallCb(h, apfByp, "out", apf2, "bypass", NULL );   // 
  cmDspSysInstallCb(h, apfByp, "out", apf3, "bypass", NULL );   // 


  cmDspSysInstallCb(h, ogain0, "val", ao0p, "gain", NULL );   // output gain control
  cmDspSysInstallCb(h, ogain1, "val", ao1p, "gain", NULL );
  cmDspSysInstallCb(h, ogain2, "val", ao2p, "gain", NULL );
  cmDspSysInstallCb(h, ogain3, "val", ao3p, "gain", NULL );




  if( useInputEqFl )
  {
    cmDspSysInstallCb(h, eqLpByp0,  "out", eqLpf_0, "bypass", NULL);
    cmDspSysInstallCb(h, eqLpMode0, "mode",eqLpf_0, "mode",   NULL );
    cmDspSysInstallCb(h, eqLpFc0,   "val", eqLpf_0, "f0",     NULL );
    cmDspSysInstallCb(h, eqLpQ0,    "val", eqLpf_0, "Q",      NULL );
    cmDspSysInstallCb(h, eqLpGain0, "val", eqLpf_0, "gain",   NULL );

    cmDspSysInstallCb(h, eqBpByp0,  "out", eqBpf_0, "bypass", NULL);
    cmDspSysInstallCb(h, eqBpMode0, "mode",eqBpf_0, "mode",   NULL );
    cmDspSysInstallCb(h, eqBpFc0,   "val", eqBpf_0, "f0",     NULL );
    cmDspSysInstallCb(h, eqBpQ0,    "val", eqBpf_0, "Q",      NULL );
    cmDspSysInstallCb(h, eqBpGain0, "val", eqBpf_0, "gain",   NULL );

    cmDspSysInstallCb(h, eqHpByp0,  "out", eqHpf_0, "bypass", NULL);
    cmDspSysInstallCb(h, eqHpMode0, "mode",eqHpf_0, "mode",   NULL );
    cmDspSysInstallCb(h, eqHpFc0,   "val", eqHpf_0, "f0",     NULL );
    cmDspSysInstallCb(h, eqHpQ0,    "val", eqHpf_0, "Q",      NULL );
    cmDspSysInstallCb(h, eqHpGain0, "val", eqHpf_0, "gain",   NULL );


    cmDspSysInstallCb(h, eqLpByp1,  "out", eqLpf_1, "bypass", NULL);
    cmDspSysInstallCb(h, eqLpMode1, "mode",eqLpf_1, "mode",   NULL );
    cmDspSysInstallCb(h, eqLpFc1,   "val", eqLpf_1, "f0",     NULL );
    cmDspSysInstallCb(h, eqLpQ1,    "val", eqLpf_1, "Q",      NULL );
    cmDspSysInstallCb(h, eqLpGain1, "val", eqLpf_1, "gain",   NULL );

    cmDspSysInstallCb(h, eqBpByp1,  "out", eqBpf_1, "bypass", NULL);
    cmDspSysInstallCb(h, eqBpMode1, "mode",eqBpf_1, "mode",   NULL );
    cmDspSysInstallCb(h, eqBpFc1,   "val", eqBpf_1, "f0",     NULL );
    cmDspSysInstallCb(h, eqBpQ1,    "val", eqBpf_1, "Q",      NULL );
    cmDspSysInstallCb(h, eqBpGain1, "val", eqBpf_1, "gain",   NULL );

    cmDspSysInstallCb(h, eqHpByp1,  "out", eqHpf_1, "bypass", NULL);
    cmDspSysInstallCb(h, eqHpMode1, "mode",eqHpf_1, "mode",   NULL );
    cmDspSysInstallCb(h, eqHpFc1,   "val", eqHpf_1, "f0",     NULL );
    cmDspSysInstallCb(h, eqHpQ1,    "val", eqHpf_1, "Q",      NULL );
    cmDspSysInstallCb(h, eqHpGain1, "val", eqHpf_1, "gain",   NULL );


    cmDspSysInstallCb(h, eqLpByp2,  "out", eqLpf_2, "bypass", NULL);
    cmDspSysInstallCb(h, eqLpMode2, "mode",eqLpf_2, "mode",   NULL );
    cmDspSysInstallCb(h, eqLpFc2,   "val", eqLpf_2, "f0",     NULL );
    cmDspSysInstallCb(h, eqLpQ2,    "val", eqLpf_2, "Q",      NULL );
    cmDspSysInstallCb(h, eqLpGain2, "val", eqLpf_2, "gain",   NULL );

    cmDspSysInstallCb(h, eqBpByp2,  "out", eqBpf_2, "bypass", NULL);
    cmDspSysInstallCb(h, eqBpMode2, "mode",eqBpf_2, "mode",   NULL );
    cmDspSysInstallCb(h, eqBpFc2,   "val", eqBpf_2, "f0",     NULL );
    cmDspSysInstallCb(h, eqBpQ2,    "val", eqBpf_2, "Q",      NULL );
    cmDspSysInstallCb(h, eqBpGain2, "val", eqBpf_2, "gain",   NULL );

    cmDspSysInstallCb(h, eqHpByp2,  "out", eqHpf_2, "bypass", NULL);
    cmDspSysInstallCb(h, eqHpMode2, "mode",eqHpf_2, "mode",   NULL );
    cmDspSysInstallCb(h, eqHpFc2,   "val", eqHpf_2, "f0",     NULL );
    cmDspSysInstallCb(h, eqHpQ2,    "val", eqHpf_2, "Q",      NULL );
    cmDspSysInstallCb(h, eqHpGain2, "val", eqHpf_2, "gain",   NULL );

    cmDspSysInstallCb(h, eqLpByp3,  "out", eqLpf_3, "bypass", NULL);
    cmDspSysInstallCb(h, eqLpMode3, "mode",eqLpf_3, "mode",   NULL );
    cmDspSysInstallCb(h, eqLpFc3,   "val", eqLpf_3, "f0",     NULL );
    cmDspSysInstallCb(h, eqLpQ3,    "val", eqLpf_3, "Q",      NULL );
    cmDspSysInstallCb(h, eqLpGain3, "val", eqLpf_3, "gain",   NULL );

    cmDspSysInstallCb(h, eqBpByp3,  "out", eqBpf_3, "bypass", NULL);
    cmDspSysInstallCb(h, eqBpMode3, "mode",eqBpf_3, "mode",   NULL );
    cmDspSysInstallCb(h, eqBpFc3,   "val", eqBpf_3, "f0",     NULL );
    cmDspSysInstallCb(h, eqBpQ3,    "val", eqBpf_3, "Q",      NULL );
    cmDspSysInstallCb(h, eqBpGain3, "val", eqBpf_3, "gain",   NULL );

    cmDspSysInstallCb(h, eqHpByp3,  "out", eqHpf_3, "bypass", NULL);
    cmDspSysInstallCb(h, eqHpMode3, "mode",eqHpf_3, "mode",   NULL );
    cmDspSysInstallCb(h, eqHpFc3,   "val", eqHpf_3, "f0",     NULL );
    cmDspSysInstallCb(h, eqHpQ3,    "val", eqHpf_3, "Q",      NULL );
    cmDspSysInstallCb(h, eqHpGain3, "val", eqHpf_3, "gain",   NULL );
  }

  return rc;
}




cmDspRC_t _cmDspSysPgm_KrLive(cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t       rc         = kOkDspRC;
  return rc;

}
