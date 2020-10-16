//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
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
#include "cmSerialPort.h"
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

#include "cmDspPgmKrChain.h"

#undef KR2


cmDspRC_t krLoadRsrc(cmDspSysH_t h, cmErr_t* err, krRsrc_t* r)
{
  cmDspRC_t rc;
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    return rc;

  cmDspRsrcString(h,&r->tlFn,        "timeLineFn",   NULL);
  cmDspRsrcString(h,&r->tlPrefixPath,"tlPrefixPath", NULL);
  cmDspRsrcString(h,&r->scFn,        "scoreFn",      NULL);
  cmDspRsrcString(h,&r->tksbFn,      "tksbFn",       NULL);
  cmDspRsrcString(h,&r->modFn,       "modFn",        NULL);
  cmDspRsrcString(h,&r->measFn,      "measFn",       NULL);
  cmDspRsrcString(h,&r->recordDir,   "recordDir",    NULL);
  cmDspRsrcString(h,&r->midiDevice,  "midiDevice",   NULL);
  cmDspRsrcString(h,&r->midiOutPort, "midiOutPort",  NULL);
  cmDspRsrcString(h,&r->midiOutPort2,"midiOutPort2", NULL);

  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    cmErrMsg(err,rc,"A KR DSP resource load failed.");
    
  return rc;
}


const cmChar_t* _mlbl(const cmChar_t* prefix, unsigned ch )
{
  static char s[128];
  s[127]=0;
  snprintf(s,127,"%s%i",prefix,ch);
  return s;
}

#define mlbl(a)  _mlbl(a,mch)
#define lbl(a) cmDspSysPrintLabel(a,ach)

#ifndef KR2
void _cmDspSys_TlXformChain( cmDspSysH_t h, cmDspTlXform_t* c,  unsigned preGrpSymId, unsigned cmpPreGrpSymId, cmDspInst_t* modp, unsigned ach, unsigned mch )
{
  unsigned        measRtrChCnt = 9; // note: router channel 8 is not connected
  unsigned        scaleRangeDfltSelId = 8;
    
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


  cmDspInst_t* p_wet     = cmDspSysAllocInst(h,"Printer", NULL,   1, "WET:");

  
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
  cmDspInst_t* cel_sr   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0,  0.0,  100.0 );
  cmDspInst_t* exp_sr   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0,-10.0,   10.0 );
  cmDspInst_t* mix_sr   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0,  0.01,   1.0 );
  cmDspInst_t* thr_sr   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, 0.01, 100.0 );
  cmDspInst_t* upr_sr   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, -1.0, 5.0 );
  cmDspInst_t* lwr_sr   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, -5.0, 5.0 );
  //cmDspInst_t* off_sr   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0,  0.0, 100.0 );
  cmDspInst_t* wet_sr   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0,  0.0, 1.0 );

 
  // Parameter-> kr routers (routers used to cross-fade between the two kr units)
  unsigned paramRtChCnt = 2;
  cmDspInst_t* wnd_rt   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );
  cmDspInst_t* hop_rt   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );
  cmDspInst_t* cel_rt   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );
  cmDspInst_t* exp_rt   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );
  cmDspInst_t* mix_rt   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );
  cmDspInst_t* thr_rt   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );
  cmDspInst_t* upr_rt   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );
  cmDspInst_t* lwr_rt   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );
  cmDspInst_t* wet_rt   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );

  // Audio processors
  cmDspInst_t* kr0  = cmDspSysAllocInst(h, "Kr2",         NULL,   2, krWndSmpCnt, krHopFact );
  cmDspInst_t* kr1  = cmDspSysAllocInst(h, "Kr2",         NULL,   2, krWndSmpCnt, krHopFact );
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
  cmDspInst_t* val_dynm_ctl    = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Dynm Meas"),       0.0, 10.0, 1.0, 1.0);  
  cmDspInst_t* min_dynm_ctl    = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Min In Dyn"),      0.0, 10.0, 1.0, 0.0);
  cmDspInst_t* max_dynm_ctl    = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Max In Dyn"),      0.0, 10.0, 1.0, 4.0);
  cmDspInst_t* dynm_map_menu   = cmDspSysAllocMsgListP(h,preGrpSymId, NULL, lbl("DynSel 0"), NULL, "measMenu", scaleRangeDfltSelId);

  cmDspInst_t* val_even_ctl   = cmDspSysAllocScalarP(  h,preGrpSymId, NULL, lbl("Even Meas"),      0.0, 3.0, 0.001, 0.5);  
  cmDspInst_t* min_even_ctl   = cmDspSysAllocScalarP(  h,preGrpSymId, NULL, lbl("Min In Even"),    0.0, 1.0, 0.001, 0.0);
  cmDspInst_t* max_even_ctl   = cmDspSysAllocScalarP(  h,preGrpSymId, NULL, lbl("Max In Even"),    0.0, 3.0, 0.001, 1.0);
  cmDspInst_t* even_map_menu  = cmDspSysAllocMsgListP( h,preGrpSymId, NULL, lbl("EvenSel"), NULL, "measMenu", 4);

  cmDspSysNewColumn(h,0);
  cmDspInst_t* val_tmpo_ctl   = cmDspSysAllocScalarP(   h,preGrpSymId, NULL, lbl("Tempo Meas"),     0.0, 200.0, 1.0, 100.0);  
  cmDspInst_t* min_tmpo_ctl   = cmDspSysAllocScalarP(   h,preGrpSymId, NULL, lbl("Min In Tempo"),   0.0, 200.0, 1.0, 80.0);
  cmDspInst_t* max_tmpo_ctl   = cmDspSysAllocScalarP(   h,preGrpSymId, NULL, lbl("Max In Tempo"),   0.0, 200.0, 1.0, 120.0);
  cmDspInst_t* tmpo_map_menu  = cmDspSysAllocMsgListP(  h,preGrpSymId, NULL, lbl("TempoSel"), NULL, "measMenu", scaleRangeDfltSelId);

  cmDspInst_t* val_cost_ctl   = cmDspSysAllocScalarP(  h,preGrpSymId, NULL, lbl("Cost Meas"),        0.0, 1.0, 0.01, 0.5);  
  cmDspInst_t* min_cost_ctl   = cmDspSysAllocScalarP(  h,preGrpSymId, NULL, lbl("Min In Cost"),      0.0, 1.0, 0.01, 0.0);
  cmDspInst_t* max_cost_ctl   = cmDspSysAllocScalarP(  h,preGrpSymId, NULL, lbl("Max In Cost"),      0.0, 1.0, 0.01, 1.0);
  cmDspInst_t* cost_map_menu  = cmDspSysAllocMsgListP( h,preGrpSymId, NULL, lbl("CostSel"), NULL, "measMenu", scaleRangeDfltSelId);

  cmDspSysInstallCb(h, val_dynm_ctl, "val",     dynm_sr, "val_in", NULL );  
  cmDspSysInstallCb(h, min_dynm_ctl, "val",     dynm_sr, "min_in", NULL );
  cmDspSysInstallCb(h, max_dynm_ctl, "val",     dynm_sr, "max_in", NULL );
  cmDspSysInstallCb(h, dynm_map_menu,"out",     dynm_rt, "sel",    NULL );   
  cmDspSysInstallCb(h, dynm_sr,      "val_out", dynm_rt, "f-in",   NULL );

  cmDspSysInstallCb(h, val_even_ctl, "val",     even_sr, "val_in", NULL );  
  cmDspSysInstallCb(h, min_even_ctl, "val",     even_sr, "min_in", NULL );
  cmDspSysInstallCb(h, max_even_ctl, "val",     even_sr, "max_in", NULL );
  cmDspSysInstallCb(h, even_map_menu,"out",     even_rt, "sel",    NULL );   
  cmDspSysInstallCb(h, even_sr,      "val_out", even_rt, "f-in",   NULL );

  cmDspSysInstallCb(h, val_tmpo_ctl, "val",     tmpo_sr, "val_in", NULL );
  cmDspSysInstallCb(h, min_tmpo_ctl, "val",     tmpo_sr, "min_in", NULL );
  cmDspSysInstallCb(h, max_tmpo_ctl, "val",     tmpo_sr, "max_in", NULL );
  cmDspSysInstallCb(h, tmpo_map_menu,"out",     tmpo_rt, "sel",    NULL );   
  cmDspSysInstallCb(h, tmpo_sr,      "val_out", tmpo_rt, "f-in",   NULL );
  
  cmDspSysInstallCb(h, val_cost_ctl, "val",     cost_sr, "val_in", NULL );
  cmDspSysInstallCb(h, min_cost_ctl, "val",     cost_sr, "min_in", NULL );
  cmDspSysInstallCb(h, max_cost_ctl, "val",     cost_sr, "max_in", NULL );
  cmDspSysInstallCb(h, cost_map_menu,"out",     cost_rt, "sel",    NULL );   
  cmDspSysInstallCb(h, cost_sr,      "val_out", cost_rt, "f-in",   NULL );

  cmDspSysNewColumn(h,0);
  cmDspInst_t* min_cel_ctl   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Min Ceil"),         0.0,100.0, 0.1,  10.0);
  cmDspInst_t* max_cel_ctl   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Max Ceil"),         0.0,100.0, 0.1,  50.0);
  cmDspInst_t* min_exp_ctl   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Min Exp"),        -10.0, 10.0, 0.01, 1.0);
  cmDspInst_t* max_exp_ctl   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Max Exp"),        -10.0, 10.0, 0.01, 4.0);
  cmDspInst_t* min_mix_ctl   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Min Mix"),          0.0,  1.0, 0.01, 0.0);
  cmDspInst_t* max_mix_ctl   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Max Mix"),          0.0,  1.0, 0.01, 1.0);
  cmDspInst_t* min_thr_ctl   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Min Thresh"),       0.0,100.0, 1.0, 30.0);
  cmDspInst_t* max_thr_ctl   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Max Thresh"),       0.0,100.0, 1.0, 80.0);
  cmDspInst_t* min_upr_ctl   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Min Upr"),         -1.0,  1.0, 0.001, -0.5);
  cmDspInst_t* max_upr_ctl   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Max Upr"),         -1.0,  1.0, 0.001, 0.5);
  cmDspInst_t* min_lwr_ctl   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Min Lwr"),          0.0, -1.0, 5.0, 1.0);
  cmDspInst_t* max_lwr_ctl   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Max Lwr"),          0.0, -1.0, 5.0, 3.0);
  cmDspInst_t* min_off_ctl   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Min Off"),          0.0, 50.0, 0.1, 30.0);
  cmDspInst_t* max_off_ctl   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Max Off"),          0.0, 50.0, 0.1, 30.0);
  cmDspInst_t* min_wet_ctl   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Min Wet"),          0.0,  1.0, 0.01, 0.0);
  cmDspInst_t* max_wet_ctl   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, lbl("Max Wet"),          0.0,  1.0, 0.01, 1.0);


  // Parameter number controls 
  cmDspSysNewColumn(h,0);
  cmDspInst_t* wnd_ctl = cmDspSysAllocMsgListP(h,preGrpSymId,NULL, lbl("WndSmpCnt"), NULL, "wndSmpCnt", 2);
  cmDspInst_t* hop_ctl = cmDspSysAllocMsgListP(h,preGrpSymId,NULL, lbl("HopFact"),   NULL, "hopFact",   2);
  cmDspInst_t* cel_ctl = cmDspSysAllocScalarP( h,preGrpSymId,NULL, lbl("Ceiling"),   0.0, 100.0, 0.1,  30.0 );
  cmDspInst_t* exp_ctl = cmDspSysAllocScalarP( h,preGrpSymId,NULL, lbl("Expo"),    -10.0,  10.0, 0.01,  2.0 );
  cmDspInst_t* mix_ctl = cmDspSysAllocScalarP( h,preGrpSymId,NULL, lbl("Mix"),       0.0,   1.0, 0.01,  0.0 );    
  cmDspInst_t* thr_ctl = cmDspSysAllocScalarP( h,preGrpSymId,NULL, lbl("Threshold"), 0.0, 100.0, 1.0,  60.0 );
  cmDspInst_t* upr_ctl = cmDspSysAllocScalarP( h,preGrpSymId,NULL, lbl("Upr slope"), 0.0,  10.0, 0.01,  0.0 ); 
  cmDspInst_t* lwr_ctl = cmDspSysAllocScalarP( h,preGrpSymId,NULL, lbl("Lwr slope"), 0.3,  10.0, 0.01,  2.0 );
  cmDspInst_t* wet_ctl = cmDspSysAllocScalarP( h,preGrpSymId,NULL, lbl("Wet Dry"),   0.0,   1.0, 0.001, 1.0 );

  
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
  
  cmDspSysInstallCb(h, min_wet_ctl, "val",     wet_sr, "min_out", NULL );
  cmDspSysInstallCb(h, max_wet_ctl, "val",     wet_sr, "max_out", NULL );
  cmDspSysInstallCb(h, even_rt,     "f-out-3", wet_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, dynm_rt,     "f-out-3", wet_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, tmpo_rt,     "f-out-3", wet_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, cost_rt,     "f-out-3", wet_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, wet_sr,      "val_out", wet_ctl,"val",     NULL );
  cmDspSysInstallCb(h, wet_ctl,     "val",     wet_rt, "f-in",    NULL );
  cmDspSysInstallCb(h, achan,       "ch",      wet_rt, "sel",     NULL );   // ach->rt sel
  cmDspSysInstallCb(h, wet_rt,      "f-out-0", kr0,    "wet",     NULL );   // wet->kr
  cmDspSysInstallCb(h, wet_rt,      "f-out-1", kr1,    "wet",     NULL );   // wet->kr


  cmDspSysInstallCb(h, wet_rt, "f-out-0", p_wet, "in", NULL );
  cmDspSysInstallCb(h, wet_rt, "f-out-1", p_wet, "in", NULL );
  
  cmDspSysInstallCb(h, min_cel_ctl, "val",     cel_sr, "min_out", NULL );
  cmDspSysInstallCb(h, max_cel_ctl, "val",     cel_sr, "max_out", NULL );
  cmDspSysInstallCb(h, even_rt,     "f-out-4", cel_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, dynm_rt,     "f-out-4", cel_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, tmpo_rt,     "f-out-4", cel_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, cost_rt,     "f-out-4", cel_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, cel_sr,      "val_out", cel_ctl,"val",     NULL );
  cmDspSysInstallCb(h, cel_ctl,     "val",     cel_rt, "f-in",    NULL );
  cmDspSysInstallCb(h, achan,       "ch",      cel_rt, "sel",     NULL );   // ach->rt sel
  cmDspSysInstallCb(h, cel_rt,      "f-out-0", kr0,    "ceil",    NULL );   // cel->kr
  cmDspSysInstallCb(h, cel_rt,      "f-out-1", kr1,    "ceil",    NULL );   // cel->kr

  cmDspSysInstallCb(h, min_exp_ctl, "val",     exp_sr, "min_out", NULL );
  cmDspSysInstallCb(h, max_exp_ctl, "val",     exp_sr, "max_out", NULL );
  cmDspSysInstallCb(h, even_rt,     "f-out-5", exp_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, dynm_rt,     "f-out-5", exp_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, tmpo_rt,     "f-out-5", exp_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, cost_rt,     "f-out-5", exp_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, exp_sr,      "val_out", exp_ctl,"val",     NULL );
  cmDspSysInstallCb(h, exp_ctl,     "val",     exp_rt, "f-in",    NULL );
  cmDspSysInstallCb(h, achan,       "ch",      exp_rt, "sel",     NULL );   // ach->rt sel
  cmDspSysInstallCb(h, exp_rt,      "f-out-0", kr0,    "expo",    NULL );   // exp->kr
  cmDspSysInstallCb(h, exp_rt,      "f-out-1", kr1,    "expo",    NULL );   // exp->kr

  cmDspSysInstallCb(h, min_mix_ctl, "val",     mix_sr, "min_out", NULL );
  cmDspSysInstallCb(h, max_mix_ctl, "val",     mix_sr, "max_out", NULL );
  cmDspSysInstallCb(h, even_rt,     "f-out-6", mix_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, dynm_rt,     "f-out-6", mix_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, tmpo_rt,     "f-out-6", mix_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, cost_rt,     "f-out-6", mix_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, mix_sr,      "val_out", mix_ctl,"val",     NULL );
  cmDspSysInstallCb(h, mix_ctl,     "val",     mix_rt, "f-in",    NULL );
  cmDspSysInstallCb(h, achan,       "ch",      mix_rt, "sel",     NULL );   // ach->rt sel
  cmDspSysInstallCb(h, mix_rt,      "f-out-0", kr0,    "mix",    NULL );   // mix->kr
  cmDspSysInstallCb(h, mix_rt,      "f-out-1", kr1,    "mix",    NULL );   // mix->kr
  
  

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

  cmDspInst_t* sw_btn = cmDspSysAllocButton( h, lbl("switch"), 0);
  cmDspSysInstallCb( h, sw_btn, "sym",  achan, "trig", NULL ); 

  
  
  // TODO: FIX THIS: can't send window length (e.g. 1024,2048, ... ) to a 'MsgList' with 4 elements
  // since it expects an index betweeen 0 and 3.  CRASH!
  //cmDspSysInstallCb(h, modp, mlbl("win"),  wnd_ctl, "sel",  NULL );

  
  cmDspSysInstallCb(h, modp, mlbl("hop"),  hop_ctl, "sel", NULL );
  cmDspSysInstallCb(h, modp, mlbl("ceil"), cel_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("expo"), exp_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("mix"),  mix_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("thr"),  thr_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("upr"),  upr_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("lwr"),  lwr_ctl, "val", NULL );
  //cmDspSysInstallCb(h, modp, mlbl("wet"),  wet_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("minc"), min_cel_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("maxc"), max_cel_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("mine"), min_exp_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("maxe"), max_exp_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("minm"), min_mix_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("maxm"), max_mix_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("mint"), min_thr_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("maxt"), max_thr_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("minu"), min_upr_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("maxu"), max_upr_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("minl"), min_lwr_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("maxl"), max_lwr_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("mino"), min_off_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("maxo"), max_off_ctl, "val", NULL );
  //cmDspSysInstallCb(h, modp, mlbl("sw"),   achan,       "trig", NULL ); // See also: amp.sfloc->achan.trig

  
  c->achan = achan; 
  c->kr0   = kr0; 
  c->kr1   = kr1;
  c->cmp   = cmp;
  c->even_ctl = val_even_ctl;

}
#endif

//=======================================================================================================================
//=======================================================================================================================
//  KR2 Modeless Transform based on cmDspKr2 and cmSpecDist2
//=======================================================================================================================
//=======================================================================================================================
/*
#ifdef KR2
void _cmDspSys_TlXformChain( cmDspSysH_t h, cmDspTlXform_t* c,  unsigned preGrpSymId, unsigned cmpPreGrpSymId, cmDspInst_t* modp, unsigned ach, unsigned mch )
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

 
  // Parameter-> kr routers (routers used to cross-fade between the two kr units)
  unsigned paramRtChCnt = 2;
  cmDspInst_t* wnd_rt   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );
  cmDspInst_t* hop_rt   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );
  cmDspInst_t* ceil_rt  = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );
  cmDspInst_t* expo_rt  = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );
  cmDspInst_t* mix_rt   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );
  cmDspInst_t* thr_rt   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );
  cmDspInst_t* upr_rt   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );
  cmDspInst_t* lwr_rt   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );
  cmDspInst_t* wet_rt   = cmDspSysAllocInst(h, "Router",      NULL,  2,  paramRtChCnt, paramRtChCnt-1 );

  // Audio processors
  cmDspInst_t* kr0  = cmDspSysAllocInst(h, "Kr2",        NULL,   2, krWndSmpCnt, krHopFact );
  cmDspInst_t* kr1  = cmDspSysAllocInst(h, "Kr2",        NULL,   2, krWndSmpCnt, krHopFact );
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

  
  // Parameter number controls 
  cmDspInst_t* wnd_ctl = cmDspSysAllocMsgListP(h,preGrpSymId,NULL, lbl("WndSmpCnt"), NULL, "wndSmpCnt", 2);
  cmDspInst_t* hop_ctl = cmDspSysAllocMsgListP(h,preGrpSymId,NULL, lbl("HopFact"),   NULL, "hopFact",   2);
  cmDspInst_t* ceil_ctl= cmDspSysAllocScalarP( h,preGrpSymId,NULL, lbl("Ceiling"),     0.0, 100.0, 0.1,  30.0 );
  cmDspInst_t* expo_ctl= cmDspSysAllocScalarP( h,preGrpSymId,NULL, lbl("Expo"),      -10.0,  10.0, 0.01,  2.0 );
  cmDspInst_t* mix_ctl = cmDspSysAllocScalarP( h,preGrpSymId,NULL, lbl("Mix"),         0.0,   1.0, 0.01,  0.0 );
  cmDspInst_t* thr_ctl = cmDspSysAllocScalarP( h,preGrpSymId,NULL, lbl("Threshold"),   0.0, 100.0, 0.1,  60.0 );
  cmDspInst_t* upr_ctl = cmDspSysAllocScalarP( h,preGrpSymId,NULL, lbl("Upr slope"), -10.0,  10.0, 0.01,  0.0 ); 
  cmDspInst_t* lwr_ctl = cmDspSysAllocScalarP( h,preGrpSymId,NULL, lbl("Lwr slope"), -10.0,  10.0, 0.01,  2.0 );
  cmDspInst_t* wet_ctl = cmDspSysAllocScalarP( h,preGrpSymId,NULL, lbl("Wet Dry"),     0.0,   1.0, 0.001, 1.0 );

  cmDspSysInstallCb(h, wnd_ctl, "out",         wnd_rt, "f-in",    NULL );
  cmDspSysInstallCb(h, achan,   "ch",          wnd_rt, "sel",     NULL );   // ach->rt sel
  cmDspSysInstallCb(h, wnd_rt,  "f-out-0",     kr0,    "wndn",    NULL );   // wndn->kr
  cmDspSysInstallCb(h, wnd_rt,  "f-out-1",     kr1,    "wndn",    NULL );   // wndn->kr

  cmDspSysInstallCb(h, hop_ctl, "out",         hop_rt, "f-in",    NULL );
  cmDspSysInstallCb(h, achan,   "ch",          hop_rt, "sel",     NULL );   // ach->rt sel
  cmDspSysInstallCb(h, hop_rt,  "f-out-0",     kr0,    "hopf",    NULL );   // hopf->kr
  cmDspSysInstallCb(h, hop_rt,  "f-out-1",     kr1,    "hopf",    NULL );   // hopf->kr

  cmDspSysInstallCb(h, ceil_ctl,     "val",     ceil_rt, "f-in",  NULL );
  cmDspSysInstallCb(h, achan,        "ch",      ceil_rt, "sel",   NULL );   // ach->rt sel
  cmDspSysInstallCb(h, ceil_rt,      "f-out-0", kr0,     "ceil",  NULL );   // ceil->kr
  cmDspSysInstallCb(h, ceil_rt,      "f-out-1", kr1,     "ceil",  NULL );   // ceil->kr

  cmDspSysInstallCb(h, expo_ctl,     "val",     expo_rt, "f-in",  NULL );
  cmDspSysInstallCb(h, achan,        "ch",      expo_rt, "sel",   NULL );   // ach->rt sel
  cmDspSysInstallCb(h, expo_rt,      "f-out-0", kr0,     "expo",  NULL );   // expo->kr
  cmDspSysInstallCb(h, expo_rt,      "f-out-1", kr1,     "expo",  NULL );   // expo->kr

  cmDspSysInstallCb(h, mix_ctl,     "val",     mix_rt,  "f-in",   NULL );
  cmDspSysInstallCb(h, achan,       "ch",      mix_rt,  "sel",    NULL );   // ach->rt sel
  cmDspSysInstallCb(h, mix_rt,      "f-out-0", kr0,     "mix",    NULL );   // mix->kr
  cmDspSysInstallCb(h, mix_rt,      "f-out-1", kr1,     "mix",    NULL );   // mix->kr
  
  
  cmDspSysInstallCb(h, thr_ctl,     "val",     thr_rt, "f-in",    NULL );
  cmDspSysInstallCb(h, achan,       "ch",      thr_rt, "sel",     NULL );   // ach->rt sel
  cmDspSysInstallCb(h, thr_rt,      "f-out-0", kr0,    "thrh",    NULL );   // thr->kr
  cmDspSysInstallCb(h, thr_rt,      "f-out-1", kr1,    "thrh",    NULL );   // thr->kr

  cmDspSysInstallCb(h, upr_ctl,     "val",     upr_rt, "f-in",    NULL );
  cmDspSysInstallCb(h, achan,       "ch",      upr_rt, "sel",     NULL );   // ach->rt sel
  cmDspSysInstallCb(h, upr_rt,      "f-out-0", kr0,    "uprs",    NULL );   // upr->kr
  cmDspSysInstallCb(h, upr_rt,      "f-out-1", kr1,    "uprs",    NULL );   // upr->kr

  cmDspSysInstallCb(h, lwr_ctl,     "val",     lwr_rt, "f-in",    NULL );
  cmDspSysInstallCb(h, achan,       "ch",      lwr_rt, "sel",     NULL );   // ach->rt sel
  cmDspSysInstallCb(h, lwr_rt,      "f-out-0", kr0,    "lwrs",    NULL );   // lwr->kr
  cmDspSysInstallCb(h, lwr_rt,      "f-out-1", kr1,    "lwrs",    NULL );   // lwr->kr

  
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
  
  cmDspInst_t* sw_btn = cmDspSysAllocButton( h, lbl("switch"), 0);
  cmDspSysInstallCb( h, sw_btn, "sym",  achan, "trig", NULL ); 

  cmDspSysInstallCb(h, modp, mlbl("win"),  wnd_ctl, "sel",  NULL );
  cmDspSysInstallCb(h, modp, mlbl("hop"),  hop_ctl, "sel",  NULL );
  cmDspSysInstallCb(h, modp, mlbl("ceil"), ceil_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("expo"), expo_ctl, "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("mix"),  mix_ctl,  "val", NULL );
  cmDspSysInstallCb(h, modp, mlbl("thr"),  thr_ctl, "val",  NULL );
  cmDspSysInstallCb(h, modp, mlbl("upr"),  upr_ctl, "val",  NULL );
  cmDspSysInstallCb(h, modp, mlbl("lwr"),  lwr_ctl, "val",  NULL );
  cmDspSysInstallCb(h, modp, mlbl("wet"),  wet_ctl, "val",  NULL );

  cmDspSysInstallCb(h, modp, mlbl("sw"),   achan,       "trig", NULL ); // See also: amp.sfloc->achan.trig

  
  c->achan = achan; 
  c->kr0   = kr0; 
  c->kr1   = kr1;
  c->cmp   = cmp; 

}

#endif
*/
