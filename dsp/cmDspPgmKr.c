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
} krRsrc_t;

cmDspRC_t krLoadRsrc(cmDspSysH_t h, cmErr_t* err, krRsrc_t* r)
{
  cmDspRC_t rc;
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    return rc;

  cmDspRsrcString(h,&r->tlFn,   "timeLineFn",   NULL);
  cmDspRsrcString(h,&r->tlPrefixPath,"tlPrefixPath",    NULL);
  cmDspRsrcString(h,&r->scFn,   "scoreFn",         NULL);
  cmDspRsrcString(h,&r->modFn,  "modFn",           NULL);
  cmDspRsrcString(h,&r->measFn, "measFn",          NULL);

  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    cmErrMsg(err,rc,"A KR DSP resource load failed.");
    
  return rc;
}

/*
cmDspRC_t _cmDspSysPgm_TimeLine(cmDspSysH_t h, void** userPtrPtr )
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
  unsigned        xfadOutChCnt = 2;
  double          xfadMs      = 200;
  bool            xfadAllOnFl  = true;

  unsigned        measRtChCnt = 4; // note: router channel 4 is not connected

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

  unsigned   preGrpSymId     = cmDspSysPresetRegisterGroup(h,"tl");
  unsigned   compPreGrpSymId = cmDspSysPresetRegisterGroup(h,"tl_cmp"); 

  cmDspInst_t* tlp  = cmDspSysAllocInst(h,"TimeLine",    "tl",  2, r.tlFn, r.tlPrefixPath );
  cmDspInst_t* scp  = cmDspSysAllocInst(h,"Score",       "sc",  1, r.scFn );
  cmDspInst_t* php  = cmDspSysAllocInst(h,"Phasor",      NULL,  1, cmDspSysSampleRate(h) );
  cmDspInst_t* wtp  = cmDspSysAllocInst(h,"WaveTable",   NULL,  4, wtSmpCnt, wtInitMode, NULL, wtLoopCnt );
  cmDspInst_t* pts  = cmDspSysAllocInst(h,"PortToSym",   NULL,  2, "on", "off" );
  cmDspInst_t* mfp  = cmDspSysAllocInst(h,"MidiFilePlay",NULL,  0 );
  //cmDspInst_t* mip  = cmDspSysAllocInst(h,"MidiIn",      NULL,  0 );
  cmDspInst_t* sfp  = cmDspSysAllocInst(h,"ScFol",       NULL,  1, r.scFn );
  cmDspInst_t* amp  = cmDspSysAllocInst(h,"ActiveMeas",  NULL,  1, 100 );
  cmDspInst_t* modp = cmDspSysAllocInst(h,"ScMod",       NULL,  2, r.modFn, "m1" );

  cmDspInst_t* even_sr_0  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.8,   1.1, 0.0, 1.0 );
  cmDspInst_t* even_rt_0  = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtChCnt, measRtChCnt-1 );
  cmDspInst_t* dyn_sr_0   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0,   4.0, 0.01, 1.0 );
  cmDspInst_t* dyn_rt_0   = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtChCnt, measRtChCnt-1 );
  cmDspInst_t* tempo_sr_0 = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4, 80.0, 120.0, 0.01, 1.0 );
  cmDspInst_t* tempo_rt_0 = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtChCnt, measRtChCnt-1 );
  cmDspInst_t* cost_sr_0  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0,    1.0, 0.001, 1.0 );
  cmDspInst_t* cost_rt_0  = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtChCnt, measRtChCnt-1 );

  cmDspInst_t* thrh_sr_0  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, 0.01, 100.0 );
  cmDspInst_t* upr_sr_0   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, -1.0, 5.0 );
  cmDspInst_t* lwr_sr_0   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, -5.0, 5.0 );


  cmDspInst_t* even_sr_1  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.8,   1.1, 0.0, 1.0 );
  cmDspInst_t* even_rt_1  = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtChCnt, measRtChCnt-1 );
  cmDspInst_t* dyn_sr_1   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0,   4.0, 0.01, 1.0 );
  cmDspInst_t* dyn_rt_1   = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtChCnt, measRtChCnt-1 );
  cmDspInst_t* tempo_sr_1 = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4, 80.0, 120.0, 0.01, 1.0 );
  cmDspInst_t* tempo_rt_1 = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtChCnt, measRtChCnt-1 );
  cmDspInst_t* cost_sr_1  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0,    1.0, 0.001, 1.0 );
  cmDspInst_t* cost_rt_1  = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtChCnt, measRtChCnt-1 );

  cmDspInst_t* thrh_sr_1  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, 0.01, 100.0 );
  cmDspInst_t* upr_sr_1   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, -1.0, 5.0 );
  cmDspInst_t* lwr_sr_1   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, -5.0, 5.0 );


  cmDspInst_t* kr0p = cmDspSysAllocInst(h,"Kr",          NULL,   2, krWndSmpCnt, krHopFact );
  cmDspInst_t* kr1p = cmDspSysAllocInst(h,"Kr",          NULL,   2, krWndSmpCnt, krHopFact );

  cmDspInst_t* cmp0 =  cmDspSysAllocInst(h,"Compressor", NULL, 8, cmpBypassFl, cmpThreshDb, cmpRatio_num, cmpAtkMs, cmpRlsMs, cmpMakeup, cmpWndMs, cmpWndMaxMs ); 
  cmDspInst_t* cmp1 =  cmDspSysAllocInst(h,"Compressor", NULL, 8, cmpBypassFl, cmpThreshDb, cmpRatio_num, cmpAtkMs, cmpRlsMs, cmpMakeup, cmpWndMs, cmpWndMaxMs ); 

  cmDspInst_t* xfad = cmDspSysAllocInst(h,"Xfader",      NULL,   3, xfadOutChCnt, xfadMs, xfadAllOnFl );
 

  cmDspInst_t* ao0p = cmDspSysAllocInst(h,"AudioOut",    NULL,  1, 0 );
  cmDspInst_t* ao1p = cmDspSysAllocInst(h,"AudioOut",    NULL,  1, 1 );
  //cmDspInst_t* af0p = cmDspSysAllocInst(h,"AudioFileOut",NULL,  2, "/home/kevin/temp/debug0.wav",1);

  cmDspSysNewPage(h,"Controls");
  cmDspInst_t* onb  = cmDspSysAllocInst(h,"Button", "start",  2, kButtonDuiId, 1.0 );
  cmDspInst_t* offb = cmDspSysAllocInst(h,"Button", "stop",   2, kButtonDuiId, 1.0 );
  cmDspInst_t* prtb = cmDspSysAllocInst(h,"Button", "print",  2, kButtonDuiId, 1.0 );
  cmDspInst_t* qtb  = cmDspSysAllocInst(h,"Button", "quiet",  2, kButtonDuiId, 1.0 );
  cmDspInst_t* prp  = cmDspSysAllocInst(h,"Printer", NULL,   1, ">" );
  cmDspInst_t* prd  = cmDspSysAllocInst(h,"Printer", NULL,   1, "DYN:" );
  cmDspInst_t* pre  = cmDspSysAllocInst(h,"Printer", NULL,   1, "EVEN:" );
  cmDspInst_t* prt  = cmDspSysAllocInst(h,"Printer", NULL,   1, "TEMPO:");
  cmDspInst_t* prc  = cmDspSysAllocInst(h,"Printer", NULL,   1, "COST:");
  //cmDspInst_t* prv  = cmDspSysAllocInst(h,"Printer", NULL,   1, "Value:");

  //--------------- Preset controls
  cmDspInst_t* preset    = cmDspSysAllocInst(   h, "Preset", NULL, 1, preGrpSymId );
  cmDspInst_t* presetLbl = cmDspSysAllocInst(   h, "Text",   "Preset",      1, "" );
  cmDspInst_t* storeBtn  = cmDspSysAllocButton( h, "store",  0);
  cmDspInst_t* recallBtn = cmDspSysAllocButton( h, "recall", 0);
  cmDspSysInstallCb(   h, presetLbl, "val", preset, "label",NULL);
  cmDspSysInstallCb(   h, storeBtn,  "sym", preset, "cmd", NULL );
  cmDspSysInstallCb(   h, recallBtn, "sym", preset, "cmd", NULL );

  cmDspInst_t* prePath    = cmDspSysAllocInst(   h, "Fname",  "prePath",   3, true,NULL,r.tlPrefixPath);
  

  //--------------- Recorded evaluation and Active Measurement related controls
  cmDspInst_t* clrBtn  = cmDspSysAllocButton( h, "clear",  0);
  cmDspInst_t* prtBtn  = cmDspSysAllocButton( h, "dump",  0);
  cmDspInst_t* mlst    = cmDspSysAllocInst(   h, "MsgList",   NULL, 3, "meas", r.measFn, 2);
  cmDspInst_t* amCmd  = cmDspSysAllocInst(   h, "PortToSym", NULL, 2, "add", "rewind" );  

  cmDspSysInstallCb( h, clrBtn, "sym",    amp, "cmd",  NULL );
  cmDspSysInstallCb( h, prtBtn, "sym",    amp, "cmd",  NULL );
  cmDspSysInstallCb( h, amCmd, "add",     amp, "cmd",  NULL );
  cmDspSysInstallCb( h, amCmd, "rewind",  amp, "cmd",  NULL );
  cmDspSysInstallCb( h, mlst,   "loc",    amp, "loc", NULL );
  cmDspSysInstallCb( h, mlst,   "typeId", amp, "type",NULL );
  cmDspSysInstallCb( h, mlst,   "val",    amp, "val", NULL );
  cmDspSysInstallCb( h, mlst,   "cost",   amp, "cst", NULL );
  cmDspSysInstallCb( h, mlst,   "typeId", amCmd, "add", NULL );
  cmDspSysInstallCb( h, sfp,    "out",    amp, "sfloc", NULL );
  cmDspSysInstallCb( h, amp,    "even",   pre, "in", NULL );
  cmDspSysInstallCb( h, amp,    "dyn",    prd, "in", NULL );
  cmDspSysInstallCb( h, amp,    "tempo",  prt, "in", NULL );
  cmDspSysInstallCb( h, amp,    "cost",   prc, "in", NULL );

  cmDspSysNewColumn(h,0);

  double dfltOffset = 2.0; // 30.0;

  // ------   Spectral distortion controls 0
  cmDspInst_t* md0p = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "Mode",      0.0, 4.0, 1.0, 1.0);
  cmDspInst_t* ws0p = cmDspSysAllocMsgListP(h,preGrpSymId,NULL,"wndSmpCnt", NULL, "wndSmpCnt", 2);
  cmDspInst_t* hf0p = cmDspSysAllocMsgListP(h,preGrpSymId,NULL,"hopFact",   NULL, "hopFact",   2);
  cmDspInst_t* th0p = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "threshold", 0.0, 100.0, 1.0,  60.0 );
  cmDspInst_t* us0p = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "upr slope", 0.0,  10.0, 0.01,  0.0 ); 
  cmDspInst_t* ls0p = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "lwr slope", 0.3,  10.0, 0.01,  2.0 );
  cmDspInst_t* of0p = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "offset",    0.0, 100.0, 0.01, dfltOffset );
  cmDspInst_t* iv0p = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "invert",    0.0,   1.0, 1.0,   0.0 );  
  cmDspInst_t* wet0  = cmDspSysAllocScalarP(h,preGrpSymId,NULL, "wet",       0.0,    1.0,0.001,  1.0 );  
  cmDspSysNewColumn(h,0);

  // ------   Spectral distortion controls 1
  cmDspInst_t* md1p  = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "Mode1",      0.0, 4.0, 1.0, 1.0);
  cmDspInst_t* ws1p  = cmDspSysAllocMsgListP(h,preGrpSymId,NULL, "wndSmpCnt1", NULL, "wndSmpCnt", 2);
  cmDspInst_t* hf1p  = cmDspSysAllocMsgListP(h,preGrpSymId,NULL, "hopFact1",   NULL, "hopFact",   2);
  cmDspInst_t* th1p  = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "threshold1", 0.0, 100.0, 1.0,  60.0 );
  cmDspInst_t* us1p  = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "upr slope1", 0.0,  10.0, 0.01,  0.0 ); 
  cmDspInst_t* ls1p  = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "lwr slope1", 0.3,  10.0, 0.01,  2.0 );
  cmDspInst_t* of1p  = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "offset1",    0.0, 100.0, 0.01, dfltOffset );
  cmDspInst_t* iv1p  = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "invert1",    0.0,   1.0, 1.0,   0.0 );  
  cmDspInst_t* wet1  = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "wet1",       0.0,    1.0,0.001,  1.0 );  

  cmDspSysNewColumn(h,0);
  cmDspInst_t* ogain = cmDspSysAllocInst(h,"Scalar", "Out Gain",   5, kNumberDuiId, 0.0,   10.0,0.01,   3.0 );  
  //cmDspInst_t* reload = cmDspSysAllocInst(h,"Button", "Reload",     2, kButtonDuiId, 0.0 );

  cmDspSysNewPage(h,"Sc/Rgn");

  // -------- Measurement Scale/Ranges controls 0
  cmDspInst_t* min_dyn_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min In Dyn 0",      0.0, 10.0, 1.0, 0.0);
  cmDspInst_t* max_dyn_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max In Dyn 0",      0.0, 10.0, 1.0, 4.0);
  cmDspInst_t* menu_dyn_0  = cmDspSysAllocMsgListP(h,preGrpSymId, NULL, "DynSel 0", NULL, "measMenu", measRtChCnt-1);

  cmDspInst_t* min_even_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min In Even 0",    0.0, 1.0, 0.001, 0.75);
  cmDspInst_t* max_even_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max In Even 0",    0.0, 3.0, 0.001, 1.0);
  cmDspInst_t* menu_even_0  = cmDspSysAllocMsgListP(h,preGrpSymId, NULL, "EvenSel 0", NULL, "measMenu", measRtChCnt-1);

  cmDspSysNewColumn(h,0);
  cmDspInst_t* min_tempo_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min In Tempo 0",   0.0, 200.0, 1.0, 80.0);
  cmDspInst_t* max_tempo_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max In Tempo 0",   0.0, 200.0, 1.0, 120.0);
  cmDspInst_t* menu_tempo_0  = cmDspSysAllocMsgListP(h,preGrpSymId, NULL, "TempoSel 0", NULL, "measMenu", measRtChCnt-1);

  cmDspInst_t* min_cost_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min In Cost 0",      0.0, 1.0, 0.01, 0.0);
  cmDspInst_t* max_cost_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max In Cost 0",      0.0, 1.0, 0.01, 1.0);
  cmDspInst_t* menu_cost_0  = cmDspSysAllocMsgListP(h,preGrpSymId, NULL, "CostSel 0", NULL, "measMenu", measRtChCnt-1);

  // -------- Parameter Scale/Ranges controls 0
  cmDspSysNewColumn(h,0);
  cmDspInst_t* min_thrh_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min Thresh 0",      0.0, 100.0, 1.0, 30.0);
  cmDspInst_t* max_thrh_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max Thresh 0",      0.0, 100.0, 1.0, 80.0);

  cmDspInst_t* min_upr_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min Upr 0",          -1.0, 1.0, 0.001, -0.5);
  cmDspInst_t* max_upr_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max Upr 0",          -1.0, 1.0, 0.001, 0.5);

  cmDspInst_t* min_lwr_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min Lwr 0",          0.0, -1.0, 5.0, 1.0);
  cmDspInst_t* max_lwr_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max Lwr 0",          0.0, -1.0, 5.0, 3.0);


  // -------- Measurement Scale/Ranges controls 0
  cmDspSysNewColumn(h,0);
  cmDspInst_t* min_dyn_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min In Dyn 1",      0.0, 10.0, 1.0, 0.0);
  cmDspInst_t* max_dyn_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max In Dyn 1",      0.0, 10.0, 1.0, 4.0);
  cmDspInst_t* menu_dyn_1  = cmDspSysAllocMsgListP(h,preGrpSymId, NULL, "DynSel 1", NULL, "measMenu", measRtChCnt-1);

  cmDspInst_t* min_even_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min In Even 1",    0.0, 1.0, 0.001, 0.75);
  cmDspInst_t* max_even_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max In Even 1",    0.0, 3.0, 0.001, 1.0);
  cmDspInst_t* menu_even_1  = cmDspSysAllocMsgListP(h,preGrpSymId, NULL, "EvenSel 1", NULL, "measMenu", measRtChCnt-1);

  cmDspSysNewColumn(h,0);
  cmDspInst_t* min_tempo_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min In Tempo 1",   0.0, 200.0, 1.0, 80.0);
  cmDspInst_t* max_tempo_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max In Tempo 1",   0.0, 200.0, 1.0, 120.0);
  cmDspInst_t* menu_tempo_1  = cmDspSysAllocMsgListP(h,preGrpSymId, NULL, "TempoSel 1", NULL, "measMenu", measRtChCnt-1);

  cmDspInst_t* min_cost_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min In Cost 1",      0.0, 1.0, 0.01, 0.0);
  cmDspInst_t* max_cost_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max In Cost 1",      0.0, 1.0, 0.01, 1.0);
  cmDspInst_t* menu_cost_1  = cmDspSysAllocMsgListP(h,preGrpSymId, NULL, "CostSel 1", NULL, "measMenu", measRtChCnt-1);

  // -------- Parameter Scale/Ranges controls 0
  cmDspSysNewColumn(h,0);
  cmDspInst_t* min_thrh_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min Thresh 1",      0.0, 100.0, 1.0, 30.0);
  cmDspInst_t* max_thrh_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max Thresh 1",      0.0, 100.0, 1.0, 80.0);

  cmDspInst_t* min_upr_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min Upr 1",          -1.0, 1.0, 0.001, -0.5);
  cmDspInst_t* max_upr_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max Upr 1",          -1.0, 1.0, 0.001, 0.5);

  cmDspInst_t* min_lwr_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min Lwr 1",          0.0, -1.0, 5.0, 1.0);
  cmDspInst_t* max_lwr_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max Lwr 1",          0.0, -1.0, 5.0, 3.0);


  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    return rc;

  // phasor->wt->aout
  cmDspSysConnectAudio(h, php,  "out", wtp,  "phs" );   // phs -> wt

  if(0)
  {
    cmDspSysConnectAudio(h, wtp,  "out", kr0p,  "in"  );  // wt->kr
    cmDspSysConnectAudio(h, wtp,  "out", kr1p,  "in"  );
    cmDspSysConnectAudio(h, kr0p, "out", xfad, "in-0");     // kr->aout
    cmDspSysConnectAudio(h, kr1p, "out", xfad, "in-1");
    cmDspSysConnectAudio(h, xfad, "out-0", ao0p, "in");     // kr->aout
    cmDspSysConnectAudio(h, xfad, "out-1", ao1p, "in");
 
  }
  else
  {
    cmDspSysConnectAudio(h, wtp,  "out", kr0p,  "in"  );  // wt->kr
    cmDspSysConnectAudio(h, wtp,  "out", kr1p,  "in"  );

    cmDspSysConnectAudio(h, kr0p, "out", cmp0, "in"  );   // wt  -> aout0
    cmDspSysConnectAudio(h, cmp0, "out", ao0p, "in" );
    cmDspSysConnectAudio(h, kr1p, "out", cmp1, "in" );    // wt  -> aout1
    cmDspSysConnectAudio(h, cmp1, "out", ao1p, "in" );
  }

  // wave-table to time-line cursor
  cmDspSysInstallCb(   h, wtp, "fidx",tlp,  "curs", NULL); 

  cmDspSysInstallCb(h, prePath, "out", tlp, "path", NULL );

  // start connections
  cmDspSysInstallCb(h, onb, "sym", tlp, "reset", NULL );
  cmDspSysInstallCb(h, onb, "sym", scp, "send",  NULL );
  cmDspSysInstallCb(h, onb, "sym", mfp, "sel",   NULL );
  cmDspSysInstallCb(h, onb, "sym", pts, "on",    NULL );
  cmDspSysInstallCb(h, pts, "on",  wtp, "cmd",   NULL );
  cmDspSysInstallCb(h, pts, "on",  modp,"cmd",   NULL );
  cmDspSysInstallCb(h, onb, "sym", amCmd, "rewind", NULL );

  // stop connections
  cmDspSysInstallCb(h, wtp,  "done",offb,"in",  NULL ); // 'done' from WT simulates pressing Stop btn.
  cmDspSysInstallCb(h, tlp,  "mfn", pts, "off", NULL ); // Prevents WT start on new audio file from TL.
  cmDspSysInstallCb(h, offb, "sym", mfp, "sel", NULL ); 
  cmDspSysInstallCb(h, offb, "sym", pts, "off", NULL );
  cmDspSysInstallCb(h, pts,  "off", wtp, "cmd", NULL );
  cmDspSysInstallCb(h, pts,  "off", modp,"cmd", NULL );

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
  //cmDspSysInstallCb(h, scp, "sel",    prv, "in", NULL );
  cmDspSysInstallCb(h, scp, "sel",    modp,"reset", NULL );
  
  //cmDspSysInstallCb(h, reload,"out",  modp, "reload", NULL );


  // MIDI file player to score follower
  if(1)
  {
    cmDspSysInstallCb(h, mfp, "smpidx", sfp, "smpidx", NULL );
    cmDspSysInstallCb(h, mfp, "d1",     sfp, "d1",     NULL );
    cmDspSysInstallCb(h, mfp, "d0",     sfp, "d0",     NULL );
    cmDspSysInstallCb(h, mfp, "status", sfp, "status", NULL );
  }
  else
  {
    //cmDspSysInstallCb(h, mip, "smpidx", sfp, "smpidx", NULL );
    //cmDspSysInstallCb(h, mip, "d1",     sfp, "d1",     NULL );
    //cmDspSysInstallCb(h, mip, "d0",     sfp, "d0",     NULL );
    //cmDspSysInstallCb(h, mip, "status", sfp, "status", NULL );
  }

  // score follower to modulator and printers
  cmDspSysInstallCb(h, sfp, "out",  modp, "index", NULL );
  cmDspSysInstallCb(h, sfp, "out",  prp, "in",  NULL );
  //cmDspSysInstallCb(h, sfp, "even", pre, "in", NULL );
  //cmDspSysInstallCb(h, sfp, "dyn",  prd, "in", NULL );
  //cmDspSysInstallCb(h, sfp, "tempo",prt, "in", NULL );
  //cmDspSysInstallCb(h, sfp, "cost", prc, "in", NULL );

  cmDspSysInstallCb(h, prtb, "sym", sfp, "cmd", NULL );
  cmDspSysInstallCb(h, qtb,  "sym", sfp, "cmd", NULL );


  cmDspSysInstallCb(h, ws0p, "out", kr0p, "wndn", NULL );   // wndSmpCnt->kr
  cmDspSysInstallCb(h, hf0p, "out", kr0p, "hopf", NULL );   // hopFact->kr
  cmDspSysInstallCb(h, md0p, "val", kr0p, "mode", NULL );   // mode->kr
  cmDspSysInstallCb(h, th0p, "val", kr0p, "thrh", NULL );   // thresh->kr
  cmDspSysInstallCb(h, ls0p, "val", kr0p, "lwrs", NULL );   // lwrSlope->kr
  cmDspSysInstallCb(h, us0p, "val", kr0p, "uprs", NULL );   // uprSlope->kr
  cmDspSysInstallCb(h, of0p, "val", kr0p, "offs", NULL );   // offset->kr
  cmDspSysInstallCb(h, iv0p, "val", kr0p, "invt", NULL );   // invert->kr
  cmDspSysInstallCb(h, wet0, "val", kr0p, "wet", NULL );    //  wet->kr

  cmDspSysInstallCb(h, ws1p, "out", kr1p, "wndn", NULL );   // wndSmpCnt->kr
  cmDspSysInstallCb(h, hf1p, "out", kr1p, "hopf", NULL );   // hopFact->kr
  cmDspSysInstallCb(h, md1p, "val", kr1p, "mode", NULL );   // mode->kr
  cmDspSysInstallCb(h, th1p, "val", kr1p, "thrh", NULL );   // thresh->kr
  cmDspSysInstallCb(h, ls1p, "val", kr1p, "lwrs", NULL );   // lwrSlope->kr
  cmDspSysInstallCb(h, us1p, "val", kr1p, "uprs", NULL );   // uprSlope->kr
  cmDspSysInstallCb(h, of1p, "val", kr1p, "offs", NULL );   // offset->kr
  cmDspSysInstallCb(h, iv1p, "val", kr1p, "invt", NULL );   // invert->kr
  cmDspSysInstallCb(h, wet1, "val", kr1p, "wet", NULL );    //  wet->kr

  cmDspSysInstallCb(h, ogain, "val", ao0p, "gain", NULL );   // output gain control
  cmDspSysInstallCb(h, ogain, "val", ao1p, "gain", NULL );

  // Printer connections
  cmDspSysInstallCb(h, tlp, "afn",  prp, "in",  NULL );
  cmDspSysInstallCb(h, tlp, "mfn",  prp, "in",  NULL );
  cmDspSysInstallCb(h, tlp, "sel",  prp, "in",  NULL );

  //cmDspSysInstallCb(h, modp, "mod0", md0p, "val", NULL );
  //cmDspSysInstallCb(h, modp, "win0", kr0p, "wndn",NULL );
  //cmDspSysInstallCb(h, modp, "thr0", th0p, "val", NULL );
  //cmDspSysInstallCb(h, modp, "upr0", us0p, "val", NULL );
  //cmDspSysInstallCb(h, modp, "lwr0", ls0p, "val", NULL );
  //cmDspSysInstallCb(h, modp, "off0", of0p, "val", NULL );
  //cmDspSysInstallCb(h, modp, "inv0", iv0p, "val", NULL );
  //cmDspSysInstallCb(h, modp, "wet0", wet0, "val", NULL );
  //cmDspSysInstallCb(h, modp, "xf0",  xfad, "gate-0", NULL );

  cmDspSysInstallCb(h, modp, "thr0", th0p, "val", NULL );
  cmDspSysInstallCb(h, modp, "mint0", min_thrh_0, "val", NULL );
  cmDspSysInstallCb(h, modp, "maxt0", max_thrh_0, "val", NULL );
  cmDspSysInstallCb(h, modp, "minu0", min_upr_0, "val", NULL );
  cmDspSysInstallCb(h, modp, "maxu0", max_upr_0, "val", NULL );
  cmDspSysInstallCb(h, modp, "minl0", min_lwr_0, "val", NULL );
  cmDspSysInstallCb(h, modp, "maxl0", max_lwr_0, "val", NULL );


 
  //cmDspSysInstallCb(h, modp, "mod1", md1p, "val", NULL );
  //cmDspSysInstallCb(h, modp, "win1", kr1p, "wndn",NULL );
  //cmDspSysInstallCb(h, modp, "upr1", us1p, "val", NULL );
  //cmDspSysInstallCb(h, modp, "lwr1", ls1p, "val", NULL );
  //cmDspSysInstallCb(h, modp, "off1", of1p, "val", NULL );
  //cmDspSysInstallCb(h, modp, "inv1", iv1p, "val", NULL );
  //cmDspSysInstallCb(h, modp, "wet1", wet1, "val", NULL );
  //cmDspSysInstallCb(h, modp, "xf1",  xfad, "gate-1", NULL );
  
  cmDspSysInstallCb(h, modp, "thr1", th1p, "val", NULL );
  cmDspSysInstallCb(h, modp, "mint1", min_thrh_1, "val", NULL );
  cmDspSysInstallCb(h, modp, "maxt1", max_thrh_1, "val", NULL );
  cmDspSysInstallCb(h, modp, "minu1", min_upr_1, "val", NULL );
  cmDspSysInstallCb(h, modp, "maxu1", max_upr_1, "val", NULL );
  cmDspSysInstallCb(h, modp, "minl1", min_lwr_1, "val", NULL );
  cmDspSysInstallCb(h, modp, "maxl1", max_lwr_1, "val", NULL );

  // =========================================================================
  //  Scale Range Connections 0
  //

  // DYN -> scaleRange -> Router -> var scaleRange
  cmDspSysInstallCb(h, amp,    "dyn",    dyn_sr_0, "val_in", NULL );
  //cmDspSysInstallCb(h, sfp,      "dyn",     dyn_sr_0,  "val_in",  NULL );
  cmDspSysInstallCb(h, min_dyn_0,  "val",     dyn_sr_0,  "min_in",  NULL );
  cmDspSysInstallCb(h, max_dyn_0,  "val",     dyn_sr_0,  "max_in",  NULL );
  cmDspSysInstallCb(h, dyn_sr_0,   "val_out", dyn_rt_0,  "f-in",    NULL );
  cmDspSysInstallCb(h, menu_dyn_0, "out",     dyn_rt_0,  "sel",     NULL );
  cmDspSysInstallCb(h, dyn_rt_0,   "f-out-0", thrh_sr_0, "val_in",  NULL );
  cmDspSysInstallCb(h, dyn_rt_0,   "f-out-1", upr_sr_0,  "val_in",  NULL );
  cmDspSysInstallCb(h, dyn_rt_0,   "f-out-2", lwr_sr_0,  "val_in",  NULL );

  // EVEN -> scaleRange -> Router  -> var scaleRange
  cmDspSysInstallCb(h, amp,    "even",    even_sr_0, "val_in", NULL );
  //cmDspSysInstallCb(h, sfp,      "even",     even_sr_0,  "val_in",  NULL );
  cmDspSysInstallCb(h, min_even_0,  "val",     even_sr_0,  "min_in",  NULL );
  cmDspSysInstallCb(h, max_even_0,  "val",     even_sr_0,  "max_in",  NULL );
  cmDspSysInstallCb(h, even_sr_0,   "val_out", even_rt_0,  "f-in",    NULL );
  cmDspSysInstallCb(h, menu_even_0, "out",     even_rt_0,  "sel",     NULL );
  cmDspSysInstallCb(h, even_rt_0,   "f-out-0", thrh_sr_0, "val_in",  NULL );
  cmDspSysInstallCb(h, even_rt_0,   "f-out-1", upr_sr_0,  "val_in",  NULL );
  cmDspSysInstallCb(h, even_rt_0,   "f-out-2", lwr_sr_0,  "val_in",  NULL );

  // TEMPO -> scaleRange -> Router  -> var scaleRange
  cmDspSysInstallCb(h, amp,    "tempo",    tempo_sr_0, "val_in", NULL );
  //cmDspSysInstallCb(h, sfp,      "tempo",     tempo_sr_0,  "val_in",  NULL );
  cmDspSysInstallCb(h, min_tempo_0,  "val",     tempo_sr_0,  "min_in",  NULL );
  cmDspSysInstallCb(h, max_tempo_0,  "val",     tempo_sr_0,  "max_in",  NULL );
  cmDspSysInstallCb(h, tempo_sr_0,   "val_out", tempo_rt_0,  "f-in",    NULL );
  cmDspSysInstallCb(h, menu_tempo_0, "out",     tempo_rt_0,  "sel",     NULL );
  cmDspSysInstallCb(h, tempo_rt_0,   "f-out-0", thrh_sr_0, "val_in",  NULL );
  cmDspSysInstallCb(h, tempo_rt_0,   "f-out-1", upr_sr_0,  "val_in",  NULL );
  cmDspSysInstallCb(h, tempo_rt_0,   "f-out-2", lwr_sr_0,  "val_in",  NULL );

  // COST -> scaleRange -> Router  -> var scaleRange 
  cmDspSysInstallCb(h, amp,      "cost",    cost_sr_0, "val_in", NULL );
  //cmDspSysInstallCb(h, sfp,      "cost",     cost_sr_0,  "val_in",  NULL );
  cmDspSysInstallCb(h, min_cost_0,  "val",     cost_sr_0,  "min_in",  NULL );
  cmDspSysInstallCb(h, max_cost_0,  "val",     cost_sr_0,  "max_in",  NULL );
  cmDspSysInstallCb(h, cost_sr_0,   "val_out", cost_rt_0,  "f-in",    NULL );
  cmDspSysInstallCb(h, menu_cost_0, "out",     cost_rt_0,  "sel",     NULL );
  cmDspSysInstallCb(h, cost_rt_0,   "f-out-0", thrh_sr_0, "val_in",  NULL );
  cmDspSysInstallCb(h, cost_rt_0,   "f-out-1", upr_sr_0,  "val_in",  NULL );
  cmDspSysInstallCb(h, cost_rt_0,   "f-out-2", lwr_sr_0,  "val_in",  NULL );


  // THRESH scaleRange -> FX
  cmDspSysInstallCb(h, min_thrh_0, "val",     thrh_sr_0, "min_out", NULL );
  cmDspSysInstallCb(h, max_thrh_0, "val",     thrh_sr_0, "max_out", NULL );
  cmDspSysInstallCb(h, thrh_sr_0,  "val_out", th0p,    "val",     NULL );


  // UPR scaleRange -> FX
  cmDspSysInstallCb(h, min_upr_0, "val",     upr_sr_0, "min_out", NULL );
  cmDspSysInstallCb(h, max_upr_0, "val",     upr_sr_0, "max_out", NULL );
  cmDspSysInstallCb(h, upr_sr_0,  "val_out", us0p,    "val",     NULL );


  // LWR scaleRange -> FX
  cmDspSysInstallCb(h, min_lwr_0, "val",     lwr_sr_0, "min_out", NULL );
  cmDspSysInstallCb(h, max_lwr_0, "val",     lwr_sr_0, "max_out", NULL );
  cmDspSysInstallCb(h, lwr_sr_0,  "val_out", ls0p,    "val",     NULL );


  // =========================================================================
  //  Scale Range Connections 1
  //

  // DYN -> scaleRange -> Router -> var scaleRange
  cmDspSysInstallCb(h, amp,    "dyn",    dyn_sr_1, "val_in", NULL );
  //cmDspSysInstallCb(h, sfp,      "dyn",     dyn_sr_1,  "val_in",  NULL );
  cmDspSysInstallCb(h, min_dyn_1,  "val",     dyn_sr_1,  "min_in",  NULL );
  cmDspSysInstallCb(h, max_dyn_1,  "val",     dyn_sr_1,  "max_in",  NULL );
  cmDspSysInstallCb(h, dyn_sr_1,   "val_out", dyn_rt_1,  "f-in",    NULL );
  cmDspSysInstallCb(h, menu_dyn_1, "out",     dyn_rt_1,  "sel",     NULL );
  cmDspSysInstallCb(h, dyn_rt_1,   "f-out-0", thrh_sr_1, "val_in",  NULL );
  cmDspSysInstallCb(h, dyn_rt_1,   "f-out-1", upr_sr_1,  "val_in",  NULL );
  cmDspSysInstallCb(h, dyn_rt_1,   "f-out-2", lwr_sr_1,  "val_in",  NULL );

  // EVEN -> scaleRange -> Router  -> var scaleRange
  cmDspSysInstallCb(h, amp,    "even",    even_sr_1, "val_in", NULL );
  //cmDspSysInstallCb(h, sfp,      "even",     even_sr_1,  "val_in",  NULL );
  cmDspSysInstallCb(h, min_even_1,  "val",     even_sr_1,  "min_in",  NULL );
  cmDspSysInstallCb(h, max_even_1,  "val",     even_sr_1,  "max_in",  NULL );
  cmDspSysInstallCb(h, even_sr_1,   "val_out", even_rt_1,  "f-in",    NULL );
  cmDspSysInstallCb(h, menu_even_1, "out",     even_rt_1,  "sel",     NULL );
  cmDspSysInstallCb(h, even_rt_1,   "f-out-0", thrh_sr_1, "val_in",  NULL );
  cmDspSysInstallCb(h, even_rt_1,   "f-out-1", upr_sr_1,  "val_in",  NULL );
  cmDspSysInstallCb(h, even_rt_1,   "f-out-2", lwr_sr_1,  "val_in",  NULL );

  // TEMPO -> scaleRange -> Router  -> var scaleRange
  cmDspSysInstallCb(h, amp,    "tempo",    tempo_sr_1, "val_in", NULL );
  //cmDspSysInstallCb(h, sfp,      "tempo",     tempo_sr_1,  "val_in",  NULL );
  cmDspSysInstallCb(h, min_tempo_1,  "val",     tempo_sr_1,  "min_in",  NULL );
  cmDspSysInstallCb(h, max_tempo_1,  "val",     tempo_sr_1,  "max_in",  NULL );
  cmDspSysInstallCb(h, tempo_sr_1,   "val_out", tempo_rt_1,  "f-in",    NULL );
  cmDspSysInstallCb(h, menu_tempo_1, "out",     tempo_rt_1,  "sel",     NULL );
  cmDspSysInstallCb(h, tempo_rt_1,   "f-out-0", thrh_sr_1, "val_in",  NULL );
  cmDspSysInstallCb(h, tempo_rt_1,   "f-out-1", upr_sr_1,  "val_in",  NULL );
  cmDspSysInstallCb(h, tempo_rt_1,   "f-out-2", lwr_sr_1,  "val_in",  NULL );

  // COST -> scaleRange -> Router  -> var scaleRange 
  cmDspSysInstallCb(h, amp,      "cost",    cost_sr_1, "val_in", NULL );
  //cmDspSysInstallCb(h, sfp,      "cost",     cost_sr_1,  "val_in",  NULL );
  cmDspSysInstallCb(h, min_cost_1,  "val",     cost_sr_1,  "min_in",  NULL );
  cmDspSysInstallCb(h, max_cost_1,  "val",     cost_sr_1,  "max_in",  NULL );
  cmDspSysInstallCb(h, cost_sr_1,   "val_out", cost_rt_1,  "f-in",    NULL );
  cmDspSysInstallCb(h, menu_cost_1, "out",     cost_rt_1,  "sel",     NULL );
  cmDspSysInstallCb(h, cost_rt_1,   "f-out-0", thrh_sr_1, "val_in",  NULL );
  cmDspSysInstallCb(h, cost_rt_1,   "f-out-1", upr_sr_1,  "val_in",  NULL );
  cmDspSysInstallCb(h, cost_rt_1,   "f-out-2", lwr_sr_1,  "val_in",  NULL );


  // THRESH scaleRange -> FX
  cmDspSysInstallCb(h, min_thrh_1, "val",     thrh_sr_1, "min_out", NULL );
  cmDspSysInstallCb(h, max_thrh_1, "val",     thrh_sr_1, "max_out", NULL );
  cmDspSysInstallCb(h, thrh_sr_1,  "val_out", th1p,    "val",     NULL );


  // UPR scaleRange -> FX
  cmDspSysInstallCb(h, min_upr_1, "val",     upr_sr_1, "min_out", NULL );
  cmDspSysInstallCb(h, max_upr_1, "val",     upr_sr_1, "max_out", NULL );
  cmDspSysInstallCb(h, upr_sr_1,  "val_out", us1p,    "val",     NULL );


  // LWR scaleRange -> FX
  cmDspSysInstallCb(h, min_lwr_1, "val",     lwr_sr_1, "min_out", NULL );
  cmDspSysInstallCb(h, max_lwr_1, "val",     lwr_sr_1, "max_out", NULL );
  cmDspSysInstallCb(h, lwr_sr_1,  "val_out", ls1p,    "val",     NULL );



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
*/


cmDspRC_t _cmDspSysPgm_TimeLine(cmDspSysH_t h, void** userPtrPtr )
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

  unsigned        xfadeChCnt  = 2;
  double          xfadeMs     = 50;
  bool            xfadeInitFl = true;
  double          mixGain     = 1.0;

  unsigned        measRtChCnt = 4; // note: router channel 4 is not connected

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

  unsigned   preGrpSymId     = cmDspSysPresetRegisterGroup(h,"tl");
  unsigned   compPreGrpSymId = cmDspSysPresetRegisterGroup(h,"tl_cmp"); 

  cmDspInst_t* tlp  = cmDspSysAllocInst(h,"TimeLine",    "tl",  2, r.tlFn, r.tlPrefixPath );
  cmDspInst_t* scp  = cmDspSysAllocInst(h,"Score",       "sc",  1, r.scFn );
  cmDspInst_t* php  = cmDspSysAllocInst(h,"Phasor",      NULL,  1, cmDspSysSampleRate(h) );
  cmDspInst_t* wtp  = cmDspSysAllocInst(h,"WaveTable",   NULL,  4, wtSmpCnt, wtInitMode, NULL, wtLoopCnt );
  cmDspInst_t* pts  = cmDspSysAllocInst(h,"PortToSym",   NULL,  2, "on", "off" );
  cmDspInst_t* mfp  = cmDspSysAllocInst(h,"MidiFilePlay",NULL,  0 );
  cmDspInst_t* sfp  = cmDspSysAllocInst(h,"ScFol",       NULL,  1, r.scFn );
  cmDspInst_t* amp  = cmDspSysAllocInst(h,"ActiveMeas",  NULL,  1, 100 );
  cmDspInst_t* modp = cmDspSysAllocInst(h,"ScMod",       NULL,  2, r.modFn, "m1" );

  cmDspInst_t* achan       = cmDspSysAllocInst(h, "AvailCh",     NULL, 1, xfadeChCnt );
  cmDspInst_t* xf_even_rt  = cmDspSysAllocInst(h, "Router",      NULL,  2,  2, 0 );
  cmDspInst_t* xf_dyn_rt   = cmDspSysAllocInst(h, "Router",      NULL,  2,  2, 0 );
  cmDspInst_t* xf_tempo_rt = cmDspSysAllocInst(h, "Router",      NULL,  2,  2, 0 );
  cmDspInst_t* xf_cost_rt  = cmDspSysAllocInst(h, "Router",      NULL,  2,  2, 0 );


  cmDspInst_t* even_sr_00  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.8,   1.1, 0.0, 1.0 );
  cmDspInst_t* even_rt_00  = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtChCnt, measRtChCnt-1 );
  cmDspInst_t* dyn_sr_00   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0,   4.0, 0.01, 1.0 );
  cmDspInst_t* dyn_rt_00   = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtChCnt, measRtChCnt-1 );
  cmDspInst_t* tempo_sr_00 = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4, 80.0, 120.0, 0.01, 1.0 );
  cmDspInst_t* tempo_rt_00 = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtChCnt, measRtChCnt-1 );
  cmDspInst_t* cost_sr_00  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0,    1.0, 0.001, 1.0 );
  cmDspInst_t* cost_rt_00  = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtChCnt, measRtChCnt-1 );
  cmDspInst_t* thrh_sr_00  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, 0.01, 100.0 );
  cmDspInst_t* upr_sr_00   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, -1.0, 5.0 );
  cmDspInst_t* lwr_sr_00   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, -5.0, 5.0 );

  cmDspInst_t* even_sr_01  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.8,   1.1, 0.0, 1.0 );
  cmDspInst_t* even_rt_01  = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtChCnt, measRtChCnt-1 );
  cmDspInst_t* dyn_sr_01   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0,   4.0, 0.01, 1.0 );
  cmDspInst_t* dyn_rt_01   = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtChCnt, measRtChCnt-1 );
  cmDspInst_t* tempo_sr_01 = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4, 80.0, 120.0, 0.01, 1.0 );
  cmDspInst_t* tempo_rt_01 = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtChCnt, measRtChCnt-1 );
  cmDspInst_t* cost_sr_01  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0,    1.0, 0.001, 1.0 );
  cmDspInst_t* cost_rt_01  = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtChCnt, measRtChCnt-1 );
  cmDspInst_t* thrh_sr_01  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, 0.01, 100.0 );
  cmDspInst_t* upr_sr_01   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, -1.0, 5.0 );
  cmDspInst_t* lwr_sr_01   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, -5.0, 5.0 );

  cmDspInst_t* even_sr_10  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.8,   1.1, 0.0, 1.0 );
  cmDspInst_t* even_rt_10  = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtChCnt, measRtChCnt-1 );
  cmDspInst_t* dyn_sr_10   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0,   4.0, 0.01, 1.0 );
  cmDspInst_t* dyn_rt_10   = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtChCnt, measRtChCnt-1 );
  cmDspInst_t* tempo_sr_10 = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4, 80.0, 120.0, 0.01, 1.0 );
  cmDspInst_t* tempo_rt_10 = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtChCnt, measRtChCnt-1 );
  cmDspInst_t* cost_sr_10  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0,    1.0, 0.001, 1.0 );
  cmDspInst_t* cost_rt_10  = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtChCnt, measRtChCnt-1 );
  cmDspInst_t* thrh_sr_10  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, 0.01, 100.0 );
  cmDspInst_t* upr_sr_10   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, -1.0, 5.0 );
  cmDspInst_t* lwr_sr_10   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, -5.0, 5.0 );

  cmDspInst_t* even_sr_11  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.8,   1.1, 0.0, 1.0 );
  cmDspInst_t* even_rt_11  = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtChCnt, measRtChCnt-1 );
  cmDspInst_t* dyn_sr_11   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0,   4.0, 0.01, 1.0 );
  cmDspInst_t* dyn_rt_11   = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtChCnt, measRtChCnt-1 );
  cmDspInst_t* tempo_sr_11 = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4, 80.0, 120.0, 0.01, 1.0 );
  cmDspInst_t* tempo_rt_11 = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtChCnt, measRtChCnt-1 );
  cmDspInst_t* cost_sr_11  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0,    1.0, 0.001, 1.0 );
  cmDspInst_t* cost_rt_11  = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtChCnt, measRtChCnt-1 );
  cmDspInst_t* thrh_sr_11  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, 0.01, 100.0 );
  cmDspInst_t* upr_sr_11   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, -1.0, 5.0 );
  cmDspInst_t* lwr_sr_11   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, -5.0, 5.0 );


  cmDspInst_t* kr00 = cmDspSysAllocInst(h, "Kr",         NULL,   2, krWndSmpCnt, krHopFact );
  cmDspInst_t* kr01 = cmDspSysAllocInst(h, "Kr",         NULL,   2, krWndSmpCnt, krHopFact );
  cmDspInst_t* fad0 = cmDspSysAllocInst(h, "Xfader",     NULL,   3, xfadeChCnt,  xfadeMs, xfadeInitFl ); 
  cmDspInst_t* mix0 = cmDspSysAllocInst(h, "AMix",       NULL,   3, xfadeChCnt, mixGain, mixGain );
  cmDspInst_t* cmp0 =  cmDspSysAllocInst(h,"Compressor", NULL,   8, cmpBypassFl, cmpThreshDb, cmpRatio_num, cmpAtkMs, cmpRlsMs, cmpMakeup, cmpWndMs, cmpWndMaxMs ); 
  cmDspInst_t* ao0p = cmDspSysAllocInst(h,"AudioOut",    NULL,   1, 0 );
  
  cmDspInst_t* kr10 = cmDspSysAllocInst(h, "Kr",         NULL,   2, krWndSmpCnt, krHopFact );
  cmDspInst_t* kr11 = cmDspSysAllocInst(h, "Kr",         NULL,   2, krWndSmpCnt, krHopFact );
  cmDspInst_t* fad1 = cmDspSysAllocInst(h, "Xfader",     NULL,   3, xfadeChCnt,  xfadeMs, xfadeInitFl ); 
  cmDspInst_t* mix1 = cmDspSysAllocInst(h, "AMix",       NULL,   3, xfadeChCnt, mixGain, mixGain );
  cmDspInst_t* cmp1 =  cmDspSysAllocInst(h,"Compressor", NULL,   8, cmpBypassFl, cmpThreshDb, cmpRatio_num, cmpAtkMs, cmpRlsMs, cmpMakeup, cmpWndMs, cmpWndMaxMs ); 
  cmDspInst_t* ao1p = cmDspSysAllocInst(h,"AudioOut",    NULL,   1, 1 );

  //cmDspInst_t* af0p = cmDspSysAllocInst(h,"AudioFileOut",NULL,  2, "/home/kevin/temp/debug0.wav",1);

  cmDspSysNewPage(h,"Controls");
  cmDspInst_t* onb  = cmDspSysAllocInst(h,"Button", "start",  2, kButtonDuiId, 1.0 );
  cmDspInst_t* offb = cmDspSysAllocInst(h,"Button", "stop",   2, kButtonDuiId, 1.0 );
  cmDspInst_t* prtb = cmDspSysAllocInst(h,"Button", "print",  2, kButtonDuiId, 1.0 );
  cmDspInst_t* qtb  = cmDspSysAllocInst(h,"Button", "quiet",  2, kButtonDuiId, 1.0 );
  cmDspInst_t* prp  = cmDspSysAllocInst(h,"Printer", NULL,   1, ">" );
  cmDspInst_t* prd  = cmDspSysAllocInst(h,"Printer", NULL,   1, "DYN:" );
  cmDspInst_t* pre  = cmDspSysAllocInst(h,"Printer", NULL,   1, "EVEN:" );
  cmDspInst_t* prt  = cmDspSysAllocInst(h,"Printer", NULL,   1, "TEMPO:");
  cmDspInst_t* prc  = cmDspSysAllocInst(h,"Printer", NULL,   1, "COST:");
  //cmDspInst_t* prv  = cmDspSysAllocInst(h,"Printer", NULL,   1, "Value:");

  //--------------- Preset controls
  cmDspInst_t* preset    = cmDspSysAllocInst(   h, "Preset", NULL, 1, preGrpSymId );
  cmDspInst_t* presetLbl = cmDspSysAllocInst(   h, "Text",   "Preset",      1, "" );
  cmDspInst_t* storeBtn  = cmDspSysAllocButton( h, "store",  0);
  cmDspInst_t* recallBtn = cmDspSysAllocButton( h, "recall", 0);
  cmDspSysInstallCb(   h, presetLbl, "val", preset, "label",NULL);
  cmDspSysInstallCb(   h, storeBtn,  "sym", preset, "cmd", NULL );
  cmDspSysInstallCb(   h, recallBtn, "sym", preset, "cmd", NULL );

  cmDspInst_t* prePath    = cmDspSysAllocInst(   h, "Fname",  "prePath",   3, true,NULL,r.tlPrefixPath);
  

  //--------------- Recorded evaluation and Active Measurement related controls
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
  cmDspSysInstallCb( h, sfp,    "out",    amp, "sfloc", NULL );
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

  cmDspInst_t* th01p = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "threshold-01", 0.0, 100.0, 1.0,  60.0 );
  cmDspInst_t* us01p = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "upr slope-01", 0.0,  10.0, 0.01,  0.0 ); 
  cmDspInst_t* ls01p = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "lwr slope-01", 0.3,  10.0, 0.01,  2.0 );
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

  cmDspInst_t* th11p = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "threshold-11", 0.0, 100.0, 1.0,  60.0 );
  cmDspInst_t* us11p = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "upr slope-11", 0.0,  10.0, 0.01,  0.0 ); 
  cmDspInst_t* ls11p = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "lwr slope-11", 0.3,  10.0, 0.01,  2.0 );

  cmDspSysNewColumn(h,0);
  cmDspInst_t* ogain0 = cmDspSysAllocInst(h,"Scalar", "Out Gain-0",   5, kNumberDuiId, 0.0,   10.0,0.01,   3.0 );  
  cmDspInst_t* ogain1 = cmDspSysAllocInst(h,"Scalar", "Out Gain-1",   5, kNumberDuiId, 0.0,   10.0,0.01,   3.0 );  

  //cmDspInst_t* reload = cmDspSysAllocInst(h,"Button", "Reload",     2, kButtonDuiId, 0.0 );

  cmDspSysNewPage(h,"Sc/Rgn");

  // -------- Measurement Scale/Ranges controls 0
  cmDspInst_t* min_dyn_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min In Dyn 0",      0.0, 10.0, 1.0, 0.0);
  cmDspInst_t* max_dyn_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max In Dyn 0",      0.0, 10.0, 1.0, 4.0);
  cmDspInst_t* menu_dyn_0  = cmDspSysAllocMsgListP(h,preGrpSymId, NULL, "DynSel 0", NULL, "measMenu", measRtChCnt-1);

  cmDspInst_t* min_even_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min In Even 0",    0.0, 1.0, 0.001, 0.75);
  cmDspInst_t* max_even_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max In Even 0",    0.0, 3.0, 0.001, 1.0);
  cmDspInst_t* menu_even_0  = cmDspSysAllocMsgListP(h,preGrpSymId, NULL, "EvenSel 0", NULL, "measMenu", measRtChCnt-1);

  cmDspSysNewColumn(h,0);
  cmDspInst_t* min_tempo_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min In Tempo 0",   0.0, 200.0, 1.0, 80.0);
  cmDspInst_t* max_tempo_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max In Tempo 0",   0.0, 200.0, 1.0, 120.0);
  cmDspInst_t* menu_tempo_0  = cmDspSysAllocMsgListP(h,preGrpSymId, NULL, "TempoSel 0", NULL, "measMenu", measRtChCnt-1);

  cmDspInst_t* min_cost_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min In Cost 0",      0.0, 1.0, 0.01, 0.0);
  cmDspInst_t* max_cost_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max In Cost 0",      0.0, 1.0, 0.01, 1.0);
  cmDspInst_t* menu_cost_0  = cmDspSysAllocMsgListP(h,preGrpSymId, NULL, "CostSel 0", NULL, "measMenu", measRtChCnt-1);

  // -------- Parameter Scale/Ranges controls 0
  cmDspSysNewColumn(h,0);
  cmDspInst_t* min_thrh_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min Thresh 0",      0.0, 100.0, 1.0, 30.0);
  cmDspInst_t* max_thrh_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max Thresh 0",      0.0, 100.0, 1.0, 80.0);

  cmDspInst_t* min_upr_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min Upr 0",          -1.0, 1.0, 0.001, -0.5);
  cmDspInst_t* max_upr_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max Upr 0",          -1.0, 1.0, 0.001, 0.5);

  cmDspInst_t* min_lwr_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min Lwr 0",          0.0, -1.0, 5.0, 1.0);
  cmDspInst_t* max_lwr_0   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max Lwr 0",          0.0, -1.0, 5.0, 3.0);


  // -------- Measurement Scale/Ranges controls 0
  cmDspSysNewColumn(h,0);
  cmDspInst_t* min_dyn_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min In Dyn 1",      0.0, 10.0, 1.0, 0.0);
  cmDspInst_t* max_dyn_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max In Dyn 1",      0.0, 10.0, 1.0, 4.0);
  cmDspInst_t* menu_dyn_1  = cmDspSysAllocMsgListP(h,preGrpSymId, NULL, "DynSel 1", NULL, "measMenu", measRtChCnt-1);

  cmDspInst_t* min_even_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min In Even 1",    0.0, 1.0, 0.001, 0.75);
  cmDspInst_t* max_even_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max In Even 1",    0.0, 3.0, 0.001, 1.0);
  cmDspInst_t* menu_even_1  = cmDspSysAllocMsgListP(h,preGrpSymId, NULL, "EvenSel 1", NULL, "measMenu", measRtChCnt-1);

  cmDspSysNewColumn(h,0);
  cmDspInst_t* min_tempo_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min In Tempo 1",   0.0, 200.0, 1.0, 80.0);
  cmDspInst_t* max_tempo_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max In Tempo 1",   0.0, 200.0, 1.0, 120.0);
  cmDspInst_t* menu_tempo_1  = cmDspSysAllocMsgListP(h,preGrpSymId, NULL, "TempoSel 1", NULL, "measMenu", measRtChCnt-1);

  cmDspInst_t* min_cost_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min In Cost 1",      0.0, 1.0, 0.01, 0.0);
  cmDspInst_t* max_cost_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max In Cost 1",      0.0, 1.0, 0.01, 1.0);
  cmDspInst_t* menu_cost_1  = cmDspSysAllocMsgListP(h,preGrpSymId, NULL, "CostSel 1", NULL, "measMenu", measRtChCnt-1);

  // -------- Parameter Scale/Ranges controls 0
  cmDspSysNewColumn(h,0);
  cmDspInst_t* min_thrh_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min Thresh 1",      0.0, 100.0, 1.0, 30.0);
  cmDspInst_t* max_thrh_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max Thresh 1",      0.0, 100.0, 1.0, 80.0);

  cmDspInst_t* min_upr_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min Upr 1",          -1.0, 1.0, 0.001, -0.5);
  cmDspInst_t* max_upr_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max Upr 1",          -1.0, 1.0, 0.001, 0.5);

  cmDspInst_t* min_lwr_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min Lwr 1",          0.0, -1.0, 5.0, 1.0);
  cmDspInst_t* max_lwr_1   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max Lwr 1",          0.0, -1.0, 5.0, 3.0);


  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    return rc;

  // phasor->wt->aout
  cmDspSysConnectAudio(h, php,  "out", wtp,  "phs" );   // phs -> wt

  cmDspSysConnectAudio(h, wtp,  "out",   kr00, "in"  );  // wt -> kr
  cmDspSysConnectAudio(h, kr00, "out",   fad0,"in-0");  
  cmDspSysConnectAudio(h, fad0,"out-0", mix0, "in-0");
  cmDspSysConnectAudio(h, wtp,  "out",   kr01, "in"  );  // wt -> kr
  cmDspSysConnectAudio(h, kr01, "out",   fad0,"in-1");  
  cmDspSysConnectAudio(h, fad0,"out-1", mix0, "in-1");
  cmDspSysConnectAudio(h, mix0, "out",   cmp0, "in");
  cmDspSysConnectAudio(h, cmp0, "out",   ao0p, "in" );    // comp -> aout

  cmDspSysConnectAudio(h, wtp,  "out",   kr10,  "in"  );  // wt -> kr
  cmDspSysConnectAudio(h, kr10, "out",   fad1, "in-0");
  cmDspSysConnectAudio(h, fad1,"out-0", mix1,  "in-0");
  cmDspSysConnectAudio(h, wtp,  "out",   kr11,  "in"  );  // wt -> kr
  cmDspSysConnectAudio(h, kr11, "out",   fad1, "in-1");  
  cmDspSysConnectAudio(h, fad1,"out-1", mix1,  "in-1");
  cmDspSysConnectAudio(h, mix1, "out",   cmp1,  "in");
  cmDspSysConnectAudio(h, cmp1, "out",   ao1p,  "in" );   // comp -> aout

  // wave-table to time-line cursor
  cmDspSysInstallCb(   h, wtp, "fidx",tlp,  "curs", NULL); 

  cmDspSysInstallCb(h, prePath, "out", tlp, "path", NULL );

  // start connections
  cmDspSysInstallCb(h, onb, "sym", tlp, "reset", NULL );
  cmDspSysInstallCb(h, onb, "sym", scp, "send",  NULL );
  cmDspSysInstallCb(h, onb, "sym", mfp, "sel",   NULL );
  cmDspSysInstallCb(h, onb, "sym", pts, "on",    NULL );
  cmDspSysInstallCb(h, pts, "on",  wtp, "cmd",   NULL );
  cmDspSysInstallCb(h, pts, "on",  modp,"cmd",   NULL );
  cmDspSysInstallCb(h, onb, "sym", amCmd, "rewind", NULL );

  // stop connections
  cmDspSysInstallCb(h, wtp,  "done",offb,"in",  NULL ); // 'done' from WT simulates pressing Stop btn.
  cmDspSysInstallCb(h, tlp,  "mfn", pts, "off", NULL ); // Prevents WT start on new audio file from TL.
  cmDspSysInstallCb(h, offb, "sym", mfp, "sel", NULL ); 
  cmDspSysInstallCb(h, offb, "sym", pts, "off", NULL );
  cmDspSysInstallCb(h, pts,  "off", wtp, "cmd", NULL );
  cmDspSysInstallCb(h, pts,  "off", modp,"cmd", NULL );

  // time-line to wave-table selection 
  cmDspSysInstallCb(h, tlp, "absi", wtp, "beg", NULL );  
  cmDspSysInstallCb(h, tlp, "aesi", wtp, "end", NULL );
  cmDspSysInstallCb(h, tlp, "afn",  wtp, "fn",  NULL );

  // time-line to MIDI file player selection
  cmDspSysInstallCb(h, tlp, "mbsi", mfp, "bsi",   NULL );
  cmDspSysInstallCb(h, tlp, "mesi", mfp, "esi",   NULL );
  cmDspSysInstallCb(h, tlp, "mfn",  mfp, "fn",    NULL );
  
  cmDspSysInstallCb(h, scp, "sel",    sfp, "index",  NULL ); // score to score follower - to set initial search location
  cmDspSysInstallCb(h, scp, "sel",    modp,"reset", NULL );
  
  //cmDspSysInstallCb(h, reload,"out",  modp, "reload", NULL );


  // MIDI file player to score follower
  cmDspSysInstallCb(h, mfp, "smpidx", sfp, "smpidx", NULL );
  cmDspSysInstallCb(h, mfp, "d1",     sfp, "d1",     NULL );
  cmDspSysInstallCb(h, mfp, "d0",     sfp, "d0",     NULL );
  cmDspSysInstallCb(h, mfp, "status", sfp, "status", NULL );

  // score follower to modulator and printers
  cmDspSysInstallCb(h, sfp, "out",  modp,    "index", NULL );
  cmDspSysInstallCb(h, sfp, "out",  prp,     "in",  NULL );

  cmDspSysInstallCb(h, prtb, "sym", sfp, "cmd", NULL );
  cmDspSysInstallCb(h, qtb,  "sym", sfp, "cmd", NULL );


  cmDspSysInstallCb(h, ws00p, "out", kr00, "wndn", NULL );   // wndSmpCnt->kr
  cmDspSysInstallCb(h, hf00p, "out", kr00, "hopf", NULL );   // hopFact->kr
  cmDspSysInstallCb(h, md00p, "val", kr00, "mode", NULL );   // mode->kr
  cmDspSysInstallCb(h, th00p, "val", kr00, "thrh", NULL );   // thresh->kr
  cmDspSysInstallCb(h, ls00p, "val", kr00, "lwrs", NULL );   // lwrSlope->kr
  cmDspSysInstallCb(h, us00p, "val", kr00, "uprs", NULL );   // uprSlope->kr
  cmDspSysInstallCb(h, of00p, "val", kr00, "offs", NULL );   // offset->kr
  cmDspSysInstallCb(h, iv00p, "val", kr00, "invt", NULL );   // invert->kr
  cmDspSysInstallCb(h, wet00p, "val", kr00, "wet", NULL );    //  wet->kr

  cmDspSysInstallCb(h, ws00p, "out", kr01, "wndn", NULL );   // wndSmpCnt->kr
  cmDspSysInstallCb(h, hf00p, "out", kr01, "hopf", NULL );   // hopFact->kr
  cmDspSysInstallCb(h, md00p, "val", kr01, "mode", NULL );   // mode->kr
  cmDspSysInstallCb(h, th01p, "val", kr01, "thrh", NULL );   // thresh->kr
  cmDspSysInstallCb(h, ls01p, "val", kr01, "lwrs", NULL );   // lwrSlope->kr
  cmDspSysInstallCb(h, us01p, "val", kr01, "uprs", NULL );   // uprSlope->kr
  cmDspSysInstallCb(h, of00p, "val", kr01, "offs", NULL );   // offset->kr
  cmDspSysInstallCb(h, iv00p, "val", kr01, "invt", NULL );   // invert->kr
  cmDspSysInstallCb(h, wet00p, "val", kr01, "wet", NULL );    //  wet->kr

  cmDspSysInstallCb(h, ws10p, "out", kr10, "wndn", NULL );   // wndSmpCnt->kr
  cmDspSysInstallCb(h, hf10p, "out", kr10, "hopf", NULL );   // hopFact->kr
  cmDspSysInstallCb(h, md10p, "val", kr10, "mode", NULL );   // mode->kr
  cmDspSysInstallCb(h, th10p, "val", kr10, "thrh", NULL );   // thresh->kr
  cmDspSysInstallCb(h, ls10p, "val", kr10, "lwrs", NULL );   // lwrSlope->kr
  cmDspSysInstallCb(h, us10p, "val", kr10, "uprs", NULL );   // uprSlope->kr
  cmDspSysInstallCb(h, of10p, "val", kr10, "offs", NULL );   // offset->kr
  cmDspSysInstallCb(h, iv10p, "val", kr10, "invt", NULL );   // invert->kr
  cmDspSysInstallCb(h, wet10p, "val", kr10, "wet", NULL );    //  wet->kr

  cmDspSysInstallCb(h, ws10p, "out", kr11, "wndn", NULL );   // wndSmpCnt->kr
  cmDspSysInstallCb(h, hf10p, "out", kr11, "hopf", NULL );   // hopFact->kr
  cmDspSysInstallCb(h, md10p, "val", kr11, "mode", NULL );   // mode->kr
  cmDspSysInstallCb(h, th11p, "val", kr11, "thrh", NULL );   // thresh->kr
  cmDspSysInstallCb(h, ls11p, "val", kr11, "lwrs", NULL );   // lwrSlope->kr
  cmDspSysInstallCb(h, us11p, "val", kr11, "uprs", NULL );   // uprSlope->kr
  cmDspSysInstallCb(h, of10p, "val", kr11, "offs", NULL );   // offset->kr
  cmDspSysInstallCb(h, iv10p, "val", kr11, "invt", NULL );   // invert->kr
  cmDspSysInstallCb(h, wet10p, "val", kr11, "wet", NULL );    //  wet->kr

  cmDspSysInstallCb(h, ogain0, "val", ao0p, "gain", NULL );   // output gain control
  cmDspSysInstallCb(h, ogain1, "val", ao1p, "gain", NULL );

  // Printer connections
  cmDspSysInstallCb(h, tlp, "afn",  prp, "in",  NULL );
  cmDspSysInstallCb(h, tlp, "mfn",  prp, "in",  NULL );
  cmDspSysInstallCb(h, tlp, "sel",  prp, "in",  NULL );

  cmDspSysInstallCb(h, modp, "thr0", th00p, "val", NULL );
  cmDspSysInstallCb(h, modp, "mint0", min_thrh_0, "val", NULL );
  cmDspSysInstallCb(h, modp, "maxt0", max_thrh_0, "val", NULL );
  cmDspSysInstallCb(h, modp, "minu0", min_upr_0, "val", NULL );
  cmDspSysInstallCb(h, modp, "maxu0", max_upr_0, "val", NULL );
  cmDspSysInstallCb(h, modp, "minl0", min_lwr_0, "val", NULL );
  cmDspSysInstallCb(h, modp, "maxl0", max_lwr_0, "val", NULL );

  cmDspSysInstallCb(h, modp, "thr1", th10p, "val", NULL );
  cmDspSysInstallCb(h, modp, "mint1", min_thrh_1, "val", NULL );
  cmDspSysInstallCb(h, modp, "maxt1", max_thrh_1, "val", NULL );
  cmDspSysInstallCb(h, modp, "minu1", min_upr_1, "val", NULL );
  cmDspSysInstallCb(h, modp, "maxu1", max_upr_1, "val", NULL );
  cmDspSysInstallCb(h, modp, "minl1", min_lwr_1, "val", NULL );
  cmDspSysInstallCb(h, modp, "maxl1", max_lwr_1, "val", NULL );

  // =========================================================================
  //  Cross fade connections for measurments
  //

  // xfade router channel selection
  cmDspSysInstallCb( h, achan, "ch",  xf_even_rt,  "sel", NULL );   
  cmDspSysInstallCb( h, achan, "ch",  xf_dyn_rt,   "sel", NULL );
  cmDspSysInstallCb( h, achan, "ch",  xf_tempo_rt, "sel", NULL );
  cmDspSysInstallCb( h, achan, "ch",  xf_cost_rt,  "sel", NULL );
   
  // cross fade router input 
  cmDspSysInstallCb( h, amp,    "scloc",  achan,   "trig", NULL );
  cmDspSysInstallCb( h, amp,    "even",   xf_even_rt, "f-in", NULL );
  cmDspSysInstallCb( h, amp,    "dyn",    xf_dyn_rt,  "f-in", NULL );
  cmDspSysInstallCb( h, amp,    "tempo",  xf_tempo_rt,"f-in", NULL );
  cmDspSysInstallCb( h, amp,    "cost",   xf_cost_rt, "f-in", NULL );

  // cross fader router output to scale-range chains
  cmDspSysInstallCb(h, xf_even_rt,   "f-out-0",   even_sr_00, "val_in", NULL );
  cmDspSysInstallCb(h, xf_even_rt,   "f-out-1",   even_sr_01, "val_in", NULL );
  cmDspSysInstallCb(h, xf_dyn_rt,    "f-out-0",   dyn_sr_00,  "val_in", NULL );
  cmDspSysInstallCb(h, xf_dyn_rt,    "f-out-1",   dyn_sr_01,  "val_in", NULL );
  cmDspSysInstallCb(h, xf_tempo_rt,  "f-out-0",   tempo_sr_00,"val_in", NULL );
  cmDspSysInstallCb(h, xf_tempo_rt,  "f-out-1",   tempo_sr_01,"val_in", NULL );
  cmDspSysInstallCb(h, xf_cost_rt,   "f-out-0",   cost_sr_00, "val_in", NULL );
  cmDspSysInstallCb(h, xf_cost_rt,   "f-out-1",   cost_sr_01, "val_in", NULL );

  // active-channel to cross-fade connections
  cmDspSysInstallCb(h, achan, "gate-0",  fad0, "gate-0", NULL );
  cmDspSysInstallCb(h, fad0, "state-0", achan, "dis-0",  NULL );
  cmDspSysInstallCb(h, achan, "gate-1",  fad1, "gate-1", NULL );
  cmDspSysInstallCb(h, fad1, "state-1", achan, "dis-1",  NULL );


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


  // THRESH scaleRange -> FX
  cmDspSysInstallCb(h, min_thrh_0, "val",     thrh_sr_00, "min_out", NULL );
  cmDspSysInstallCb(h, max_thrh_0, "val",     thrh_sr_00, "max_out", NULL );
  cmDspSysInstallCb(h, thrh_sr_00,  "val_out", th00p,    "val",     NULL );


  // UPR scaleRange -> FX
  cmDspSysInstallCb(h, min_upr_0, "val",     upr_sr_00, "min_out", NULL );
  cmDspSysInstallCb(h, max_upr_0, "val",     upr_sr_00, "max_out", NULL );
  cmDspSysInstallCb(h, upr_sr_00,  "val_out", us00p,    "val",     NULL );


  // LWR scaleRange -> FX
  cmDspSysInstallCb(h, min_lwr_0, "val",     lwr_sr_00, "min_out", NULL );
  cmDspSysInstallCb(h, max_lwr_0, "val",     lwr_sr_00, "max_out", NULL );
  cmDspSysInstallCb(h, lwr_sr_00,  "val_out", ls00p,    "val",     NULL );

  // =========================================================================
  //  Scale Range Connections 01
  //

  // DYN -> scaleRange -> Router -> var scaleRange
  cmDspSysInstallCb(h, min_dyn_0,  "val",     dyn_sr_01,  "min_in",  NULL );
  cmDspSysInstallCb(h, max_dyn_0,  "val",     dyn_sr_01,  "max_in",  NULL );
  cmDspSysInstallCb(h, dyn_sr_01,   "val_out", dyn_rt_01,  "f-in",    NULL );
  cmDspSysInstallCb(h, menu_dyn_0, "out",     dyn_rt_01,  "sel",     NULL );
  cmDspSysInstallCb(h, dyn_rt_01,   "f-out-0", thrh_sr_01, "val_in",  NULL );
  cmDspSysInstallCb(h, dyn_rt_01,   "f-out-1", upr_sr_01,  "val_in",  NULL );
  cmDspSysInstallCb(h, dyn_rt_01,   "f-out-2", lwr_sr_01,  "val_in",  NULL );

  // EVEN -> scaleRange -> Router  -> var scaleRange
  cmDspSysInstallCb(h, min_even_0,  "val",     even_sr_01,  "min_in",  NULL );
  cmDspSysInstallCb(h, max_even_0,  "val",     even_sr_01,  "max_in",  NULL );
  cmDspSysInstallCb(h, even_sr_01,   "val_out", even_rt_01,  "f-in",    NULL );
  cmDspSysInstallCb(h, menu_even_0, "out",     even_rt_01,  "sel",     NULL );
  cmDspSysInstallCb(h, even_rt_01,   "f-out-0", thrh_sr_01, "val_in",  NULL );
  cmDspSysInstallCb(h, even_rt_01,   "f-out-1", upr_sr_01,  "val_in",  NULL );
  cmDspSysInstallCb(h, even_rt_01,   "f-out-2", lwr_sr_01,  "val_in",  NULL );

  // TEMPO -> scaleRange -> Router  -> var scaleRange
  cmDspSysInstallCb(h, min_tempo_0,  "val",     tempo_sr_01,  "min_in",  NULL );
  cmDspSysInstallCb(h, max_tempo_0,  "val",     tempo_sr_01,  "max_in",  NULL );
  cmDspSysInstallCb(h, tempo_sr_01,   "val_out", tempo_rt_01,  "f-in",    NULL );
  cmDspSysInstallCb(h, menu_tempo_0, "out",     tempo_rt_01,  "sel",     NULL );
  cmDspSysInstallCb(h, tempo_rt_01,   "f-out-0", thrh_sr_01, "val_in",  NULL );
  cmDspSysInstallCb(h, tempo_rt_01,   "f-out-1", upr_sr_01,  "val_in",  NULL );
  cmDspSysInstallCb(h, tempo_rt_01,   "f-out-2", lwr_sr_01,  "val_in",  NULL );

  // COST -> scaleRange -> Router  -> var scaleRange 
  cmDspSysInstallCb(h, min_cost_0,  "val",     cost_sr_01,  "min_in",  NULL );
  cmDspSysInstallCb(h, max_cost_0,  "val",     cost_sr_01,  "max_in",  NULL );
  cmDspSysInstallCb(h, cost_sr_01,   "val_out", cost_rt_01,  "f-in",    NULL );
  cmDspSysInstallCb(h, menu_cost_0, "out",     cost_rt_01,  "sel",     NULL );
  cmDspSysInstallCb(h, cost_rt_01,   "f-out-0", thrh_sr_01, "val_in",  NULL );
  cmDspSysInstallCb(h, cost_rt_01,   "f-out-1", upr_sr_01,  "val_in",  NULL );
  cmDspSysInstallCb(h, cost_rt_01,   "f-out-2", lwr_sr_01,  "val_in",  NULL );

  // THRESH scaleRange -> FX
  cmDspSysInstallCb(h, min_thrh_0, "val",     thrh_sr_01, "min_out", NULL );
  cmDspSysInstallCb(h, max_thrh_0, "val",     thrh_sr_01, "max_out", NULL );
  cmDspSysInstallCb(h, thrh_sr_01,  "val_out", th01p,    "val",     NULL );

  // UPR scaleRange -> FX
  cmDspSysInstallCb(h, min_upr_0, "val",     upr_sr_01, "min_out", NULL );
  cmDspSysInstallCb(h, max_upr_0, "val",     upr_sr_01, "max_out", NULL );
  cmDspSysInstallCb(h, upr_sr_01,  "val_out", us01p,    "val",     NULL );

  // LWR scaleRange -> FX
  cmDspSysInstallCb(h, min_lwr_0, "val",     lwr_sr_01, "min_out", NULL );
  cmDspSysInstallCb(h, max_lwr_0, "val",     lwr_sr_01, "max_out", NULL );
  cmDspSysInstallCb(h, lwr_sr_01,  "val_out", ls01p,    "val",     NULL );

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

  // THRESH scaleRange -> FX
  cmDspSysInstallCb(h, min_thrh_1, "val",     thrh_sr_10, "min_out", NULL );
  cmDspSysInstallCb(h, max_thrh_1, "val",     thrh_sr_10, "max_out", NULL );
  cmDspSysInstallCb(h, thrh_sr_10,  "val_out", th10p,    "val",     NULL );

  // UPR scaleRange -> FX
  cmDspSysInstallCb(h, min_upr_1, "val",     upr_sr_10, "min_out", NULL );
  cmDspSysInstallCb(h, max_upr_1, "val",     upr_sr_10, "max_out", NULL );
  cmDspSysInstallCb(h, upr_sr_10,  "val_out", us10p,    "val",     NULL );

  // LWR scaleRange -> FX
  cmDspSysInstallCb(h, min_lwr_1, "val",     lwr_sr_10, "min_out", NULL );
  cmDspSysInstallCb(h, max_lwr_1, "val",     lwr_sr_10, "max_out", NULL );
  cmDspSysInstallCb(h, lwr_sr_10,  "val_out", ls10p,    "val",     NULL );

  // =========================================================================
  //  Scale Range Connections 11
  //

  // DYN -> scaleRange -> Router -> var scaleRange
  cmDspSysInstallCb(h, min_dyn_1,  "val",     dyn_sr_11,  "min_in",  NULL );
  cmDspSysInstallCb(h, max_dyn_1,  "val",     dyn_sr_11,  "max_in",  NULL );
  cmDspSysInstallCb(h, dyn_sr_11,   "val_out", dyn_rt_11,  "f-in",    NULL );
  cmDspSysInstallCb(h, menu_dyn_1, "out",     dyn_rt_11,  "sel",     NULL );
  cmDspSysInstallCb(h, dyn_rt_11,   "f-out-0", thrh_sr_11, "val_in",  NULL );
  cmDspSysInstallCb(h, dyn_rt_11,   "f-out-1", upr_sr_11,  "val_in",  NULL );
  cmDspSysInstallCb(h, dyn_rt_11,   "f-out-2", lwr_sr_11,  "val_in",  NULL );

  // EVEN -> scaleRange -> Router  -> var scaleRange
  cmDspSysInstallCb(h, min_even_1,  "val",     even_sr_11,  "min_in",  NULL );
  cmDspSysInstallCb(h, max_even_1,  "val",     even_sr_11,  "max_in",  NULL );
  cmDspSysInstallCb(h, even_sr_11,   "val_out", even_rt_11,  "f-in",    NULL );
  cmDspSysInstallCb(h, menu_even_1, "out",     even_rt_11,  "sel",     NULL );
  cmDspSysInstallCb(h, even_rt_11,   "f-out-0", thrh_sr_11, "val_in",  NULL );
  cmDspSysInstallCb(h, even_rt_11,   "f-out-1", upr_sr_11,  "val_in",  NULL );
  cmDspSysInstallCb(h, even_rt_11,   "f-out-2", lwr_sr_11,  "val_in",  NULL );

  // TEMPO -> scaleRange -> Router  -> var scaleRange
  cmDspSysInstallCb(h, min_tempo_1,  "val",     tempo_sr_11,  "min_in",  NULL );
  cmDspSysInstallCb(h, max_tempo_1,  "val",     tempo_sr_11,  "max_in",  NULL );
  cmDspSysInstallCb(h, tempo_sr_11,   "val_out", tempo_rt_11,  "f-in",    NULL );
  cmDspSysInstallCb(h, menu_tempo_1, "out",     tempo_rt_11,  "sel",     NULL );
  cmDspSysInstallCb(h, tempo_rt_11,   "f-out-0", thrh_sr_11, "val_in",  NULL );
  cmDspSysInstallCb(h, tempo_rt_11,   "f-out-1", upr_sr_11,  "val_in",  NULL );
  cmDspSysInstallCb(h, tempo_rt_11,   "f-out-2", lwr_sr_11,  "val_in",  NULL );

  // COST -> scaleRange -> Router  -> var scaleRange 
  cmDspSysInstallCb(h, min_cost_1,  "val",     cost_sr_11,  "min_in",  NULL );
  cmDspSysInstallCb(h, max_cost_1,  "val",     cost_sr_11,  "max_in",  NULL );
  cmDspSysInstallCb(h, cost_sr_11,   "val_out", cost_rt_11,  "f-in",    NULL );
  cmDspSysInstallCb(h, menu_cost_1, "out",     cost_rt_11,  "sel",     NULL );
  cmDspSysInstallCb(h, cost_rt_11,   "f-out-0", thrh_sr_11, "val_in",  NULL );
  cmDspSysInstallCb(h, cost_rt_11,   "f-out-1", upr_sr_11,  "val_in",  NULL );
  cmDspSysInstallCb(h, cost_rt_11,   "f-out-2", lwr_sr_11,  "val_in",  NULL );

  // THRESH scaleRange -> FX
  cmDspSysInstallCb(h, min_thrh_1, "val",     thrh_sr_11, "min_out", NULL );
  cmDspSysInstallCb(h, max_thrh_1, "val",     thrh_sr_11, "max_out", NULL );
  cmDspSysInstallCb(h, thrh_sr_11,  "val_out", th11p,    "val",     NULL );


  // UPR scaleRange -> FX
  cmDspSysInstallCb(h, min_upr_1, "val",     upr_sr_11, "min_out", NULL );
  cmDspSysInstallCb(h, max_upr_1, "val",     upr_sr_11, "max_out", NULL );
  cmDspSysInstallCb(h, upr_sr_11,  "val_out", us11p,    "val",     NULL );

  // LWR scaleRange -> FX
  cmDspSysInstallCb(h, min_lwr_1, "val",     lwr_sr_11, "min_out", NULL );
  cmDspSysInstallCb(h, max_lwr_1, "val",     lwr_sr_11, "max_out", NULL );
  cmDspSysInstallCb(h, lwr_sr_11,  "val_out", ls11p,    "val",     NULL );



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




