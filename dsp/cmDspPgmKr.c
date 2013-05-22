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
  const cmChar_t* audPath;
  const cmChar_t* scFn;
  const cmChar_t* modFn;
} krRsrc_t;

cmDspRC_t krLoadRsrc(cmDspSysH_t h, cmErr_t* err, krRsrc_t* r)
{
  cmDspRC_t rc;
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    return rc;

  cmDspRsrcString(h,&r->tlFn,   "timeLineFn",      NULL);
  cmDspRsrcString(h,&r->audPath,"tlAudioFilePath", NULL);
  cmDspRsrcString(h,&r->scFn,   "scoreFn",         NULL);
  cmDspRsrcString(h,&r->modFn,  "modFn",           NULL);

  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    cmErrMsg(err,rc,"A KR DSP resource load failed.");
    
  return rc;
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

  unsigned   preGrpSymId  = cmDspSysPresetRegisterGroup(h,"TimeLine");


  cmDspInst_t* tlp  = cmDspSysAllocInst(h,"TimeLine",    "tl",  2, r.tlFn, r.audPath );
  cmDspInst_t* scp  = cmDspSysAllocInst(h,"Score",       "sc",  1, r.scFn );
  cmDspInst_t* php  = cmDspSysAllocInst(h,"Phasor",      NULL,  1, cmDspSysSampleRate(h) );
  cmDspInst_t* wtp  = cmDspSysAllocInst(h,"WaveTable",   NULL,  4, wtSmpCnt, wtInitMode, NULL, wtLoopCnt );
  cmDspInst_t* pts  = cmDspSysAllocInst(h,"PortToSym",   NULL,  2, "on", "off" );
  cmDspInst_t* mfp  = cmDspSysAllocInst(h,"MidiFilePlay",NULL,  0 );
  //cmDspInst_t* mip  = cmDspSysAllocInst(h,"MidiIn",      NULL,  0 );
  cmDspInst_t* sfp  = cmDspSysAllocInst(h,"ScFol",       NULL,  1, r.scFn );
  cmDspInst_t* modp = cmDspSysAllocInst(h,"ScMod",       NULL,  2, r.modFn, "m1" );

  cmDspInst_t* even_sr  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.8,   1.1, 0.0, 1.0 );
  cmDspInst_t* even_rt  = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtChCnt, measRtChCnt-1 );

  cmDspInst_t* dyn_sr   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0,   4.0, 0.01, 1.0 );
  cmDspInst_t* dyn_rt   = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtChCnt, measRtChCnt-1 );

  cmDspInst_t* tempo_sr = cmDspSysAllocInst(h,"ScaleRange",  NULL,  4, 80.0, 120.0, 0.01, 1.0 );
  cmDspInst_t* tempo_rt = cmDspSysAllocInst(h,"Router",      NULL,  2,  measRtChCnt, measRtChCnt-1 );

  cmDspInst_t* cost_sr  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0,    1.0, 0.001, 1.0 );
  cmDspInst_t* cost_rt  = cmDspSysAllocInst(h, "Router",      NULL,  2,  measRtChCnt, measRtChCnt-1 );

  cmDspInst_t* thrh_sr  = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, 0.01, 100.0 );
  cmDspInst_t* upr_sr   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, -1.0, 5.0 );
  cmDspInst_t* lwr_sr   = cmDspSysAllocInst(h, "ScaleRange",  NULL,  4,  0.0, 1.0, -5.0, 5.0 );

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
  

  cmDspInst_t* adir = cmDspSysAllocInst(h,"Fname",  "audDir",   3, true,NULL,r.audPath);

  cmDspSysNewColumn(h,0);

  cmDspInst_t* md0p = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "Mode",      0.0, 4.0, 1.0, 1.0);
  cmDspInst_t* ws0p = cmDspSysAllocMsgListP(h,preGrpSymId,NULL,"wndSmpCnt", NULL, "wndSmpCnt", 2);
  cmDspInst_t* hf0p = cmDspSysAllocMsgListP(h,preGrpSymId,NULL,"hopFact",   NULL, "hopFact",   2);
  cmDspInst_t* th0p = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "threshold", 0.0, 100.0, 1.0,  60.0 );
  cmDspInst_t* us0p = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "upr slope", 0.0,  10.0, 0.01,  0.0 ); 
  cmDspInst_t* ls0p = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "lwr slope", 0.3,  10.0, 0.01,  2.0 );
  cmDspInst_t* of0p = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "offset",    0.0, 100.0, 0.01, 30.0 );
  cmDspInst_t* iv0p = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "invert",    0.0,   1.0, 1.0,   0.0 );  
  cmDspInst_t* wet0  = cmDspSysAllocScalarP(h,preGrpSymId,NULL, "wet",       0.0,    1.0,0.001,  1.0 );  
  cmDspSysNewColumn(h,0);

  //cmDspInst_t* al1p = cmDspSysAllocInst(h,"MsgList","audFiles", 2, "audFiles",NULL);
  //cmDspInst_t* fl1p = cmDspSysAllocInst(h,"MsgList","audFrags1", 2, "audFrags",NULL);
  //cmDspInst_t* fn1p = cmDspSysAllocInst(h,"Sprintf","filename", 1, "%s/%s_%02i.wav");
  cmDspInst_t* md1p  = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "Mode1",      0.0, 4.0, 1.0, 1.0);
  cmDspInst_t* ws1p  = cmDspSysAllocMsgListP(h,preGrpSymId,NULL, "wndSmpCnt1", NULL, "wndSmpCnt", 2);
  cmDspInst_t* hf1p  = cmDspSysAllocMsgListP(h,preGrpSymId,NULL, "hopFact1",   NULL, "hopFact",   2);
  cmDspInst_t* th1p  = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "threshold1", 0.0, 100.0, 1.0,  60.0 );
  cmDspInst_t* us1p  = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "upr slope1", 0.0,  10.0, 0.01,  0.0 ); 
  cmDspInst_t* ls1p  = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "lwr slope1", 0.3,  10.0, 0.01,  2.0 );
  cmDspInst_t* of1p  = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "offset1",    0.0, 100.0, 0.01, 30.0 );
  cmDspInst_t* iv1p  = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "invert1",    0.0,   1.0, 1.0,   0.0 );  
  cmDspInst_t* wet1  = cmDspSysAllocScalarP( h,preGrpSymId,NULL, "wet1",       0.0,    1.0,0.001,  1.0 );  

  cmDspSysNewColumn(h,0);
  cmDspInst_t* ogain = cmDspSysAllocInst(h,"Scalar", "Out Gain",   5, kNumberDuiId, 0.0,   10.0,0.01,   3.0 );  
  //cmDspInst_t* reload = cmDspSysAllocInst(h,"Button", "Reload",     2, kButtonDuiId, 0.0 );

  cmDspInst_t* min_dyn   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min In Dyn",      0.0, 10.0, 1.0, 0.0);
  cmDspInst_t* max_dyn   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max In Dyn",      0.0, 10.0, 1.0, 4.0);
  cmDspInst_t* menu_dyn  = cmDspSysAllocMsgListP(h,preGrpSymId, NULL, "DynSel", NULL, "measMenu", measRtChCnt-1);

  cmDspInst_t* min_even   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min In Even",    0.0, 1.0, 0.001, 0.75);
  cmDspInst_t* max_even   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max In Even",    0.0, 3.0, 0.001, 1.0);
  cmDspInst_t* menu_even  = cmDspSysAllocMsgListP(h,preGrpSymId, NULL, "EvenSel", NULL, "measMenu", measRtChCnt-1);

  cmDspSysNewColumn(h,0);
  cmDspInst_t* min_tempo   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min In Tempo",   0.0, 200.0, 1.0, 80.0);
  cmDspInst_t* max_tempo   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max In Tempo",   0.0, 200.0, 1.0, 120.0);
  cmDspInst_t* menu_tempo  = cmDspSysAllocMsgListP(h,preGrpSymId, NULL, "TempoSel", NULL, "measMenu", measRtChCnt-1);

  cmDspInst_t* min_cost   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min In Cost",      0.0, 1.0, 0.01, 0.0);
  cmDspInst_t* max_cost   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max In Cost",      0.0, 1.0, 0.01, 1.0);
  cmDspInst_t* menu_cost  = cmDspSysAllocMsgListP(h,preGrpSymId, NULL, "CostSel", NULL, "measMenu", measRtChCnt-1);

  cmDspSysNewColumn(h,0);
  cmDspInst_t* min_thrh   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min Thresh",      0.0, 100.0, 1.0, 30.0);
  cmDspInst_t* max_thrh   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max Thresh",      0.0, 100.0, 1.0, 80.0);

  cmDspInst_t* min_upr   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min Upr",          -1.0, 1.0, 0.001, -0.5);
  cmDspInst_t* max_upr   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max Upr",          -1.0, 1.0, 0.001, 0.5);

  cmDspInst_t* min_lwr   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Min Lwr",          0.0, -1.0, 5.0, 1.0);
  cmDspInst_t* max_lwr   = cmDspSysAllocScalarP( h,preGrpSymId, NULL, "Max Lwr",          0.0, -1.0, 5.0, 3.0);


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

  cmDspSysInstallCb(h, adir, "out", tlp, "path", NULL );

  // start connections
  cmDspSysInstallCb(h, onb, "sym", tlp, "reset", NULL );
  cmDspSysInstallCb(h, onb, "sym", scp, "send",  NULL );
  cmDspSysInstallCb(h, onb, "sym", mfp, "sel",   NULL );
  cmDspSysInstallCb(h, onb, "sym", pts, "on",    NULL );
  cmDspSysInstallCb(h, pts, "on",  wtp, "cmd",   NULL );
  cmDspSysInstallCb(h, pts, "on",  modp,"cmd",   NULL );

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
  cmDspSysInstallCb(h, sfp, "even", pre, "in", NULL );
  cmDspSysInstallCb(h, sfp, "dyn",  prd, "in", NULL );
  cmDspSysInstallCb(h, sfp, "tempo",prt, "in", NULL );
  cmDspSysInstallCb(h, sfp, "cost", prc, "in", NULL );

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

  cmDspSysInstallCb(h, modp, "mod0", md0p, "val", NULL );
  cmDspSysInstallCb(h, modp, "win0", kr0p, "wndn",NULL );
  cmDspSysInstallCb(h, modp, "thr0", th0p, "val", NULL );
  cmDspSysInstallCb(h, modp, "upr0", us0p, "val", NULL );
  cmDspSysInstallCb(h, modp, "lwr0", ls0p, "val", NULL );
  cmDspSysInstallCb(h, modp, "off0", of0p, "val", NULL );
  cmDspSysInstallCb(h, modp, "inv0", iv0p, "val", NULL );
  cmDspSysInstallCb(h, modp, "wet0", wet0, "val", NULL );
  cmDspSysInstallCb(h, modp, "xf0",  xfad, "gate-0", NULL );

  cmDspSysInstallCb(h, modp, "mod1", md1p, "val", NULL );
  cmDspSysInstallCb(h, modp, "win1", kr1p, "wndn",NULL );
  cmDspSysInstallCb(h, modp, "thr1", th1p, "val", NULL );
  cmDspSysInstallCb(h, modp, "upr1", us1p, "val", NULL );
  cmDspSysInstallCb(h, modp, "lwr1", ls1p, "val", NULL );
  cmDspSysInstallCb(h, modp, "off1", of1p, "val", NULL );
  cmDspSysInstallCb(h, modp, "inv1", iv1p, "val", NULL );
  cmDspSysInstallCb(h, modp, "wet1", wet1, "val", NULL );
  cmDspSysInstallCb(h, modp, "xf1",  xfad, "gate-1", NULL );

  // DYN -> scaleRange -> Router -> var scaleRange
  cmDspSysInstallCb(h, sfp,      "dyn",     dyn_sr,  "val_in",  NULL );
  cmDspSysInstallCb(h, min_dyn,  "val",     dyn_sr,  "min_in",  NULL );
  cmDspSysInstallCb(h, max_dyn,  "val",     dyn_sr,  "max_in",  NULL );
  cmDspSysInstallCb(h, dyn_sr,   "val_out", dyn_rt,  "f-in",    NULL );
  cmDspSysInstallCb(h, menu_dyn, "out",     dyn_rt,  "sel",     NULL );
  cmDspSysInstallCb(h, dyn_rt,   "f-out-0", thrh_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, dyn_rt,   "f-out-1", upr_sr,  "val_in",  NULL );
  cmDspSysInstallCb(h, dyn_rt,   "f-out-2", lwr_sr,  "val_in",  NULL );

  // EVEN -> scaleRange -> Router  -> var scaleRange
  cmDspSysInstallCb(h, sfp,      "even",     even_sr,  "val_in",  NULL );
  cmDspSysInstallCb(h, min_even,  "val",     even_sr,  "min_in",  NULL );
  cmDspSysInstallCb(h, max_even,  "val",     even_sr,  "max_in",  NULL );
  cmDspSysInstallCb(h, even_sr,   "val_out", even_rt,  "f-in",    NULL );
  cmDspSysInstallCb(h, menu_even, "out",     even_rt,  "sel",     NULL );
  cmDspSysInstallCb(h, even_rt,   "f-out-0", thrh_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, even_rt,   "f-out-1", upr_sr,  "val_in",  NULL );
  cmDspSysInstallCb(h, even_rt,   "f-out-2", lwr_sr,  "val_in",  NULL );

  // TEMPO -> scaleRange -> Router  -> var scaleRange
  cmDspSysInstallCb(h, sfp,      "tempo",     tempo_sr,  "val_in",  NULL );
  cmDspSysInstallCb(h, min_tempo,  "val",     tempo_sr,  "min_in",  NULL );
  cmDspSysInstallCb(h, max_tempo,  "val",     tempo_sr,  "max_in",  NULL );
  cmDspSysInstallCb(h, tempo_sr,   "val_out", tempo_rt,  "f-in",    NULL );
  cmDspSysInstallCb(h, menu_tempo, "out",     tempo_rt,  "sel",     NULL );
  cmDspSysInstallCb(h, tempo_rt,   "f-out-0", thrh_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, tempo_rt,   "f-out-1", upr_sr,  "val_in",  NULL );
  cmDspSysInstallCb(h, tempo_rt,   "f-out-2", lwr_sr,  "val_in",  NULL );

  // COST -> scaleRange -> Router  -> var scaleRange 
  cmDspSysInstallCb(h, sfp,      "cost",     cost_sr,  "val_in",  NULL );
  cmDspSysInstallCb(h, min_cost,  "val",     cost_sr,  "min_in",  NULL );
  cmDspSysInstallCb(h, max_cost,  "val",     cost_sr,  "max_in",  NULL );
  cmDspSysInstallCb(h, cost_sr,   "val_out", cost_rt,  "f-in",    NULL );
  cmDspSysInstallCb(h, menu_cost, "out",     cost_rt,  "sel",     NULL );
  cmDspSysInstallCb(h, cost_rt,   "f-out-0", thrh_sr, "val_in",  NULL );
  cmDspSysInstallCb(h, cost_rt,   "f-out-1", upr_sr,  "val_in",  NULL );
  cmDspSysInstallCb(h, cost_rt,   "f-out-2", lwr_sr,  "val_in",  NULL );


  // THRESH scaleRange -> FX
  cmDspSysInstallCb(h, min_thrh, "val",     thrh_sr, "min_out", NULL );
  cmDspSysInstallCb(h, max_thrh, "val",     thrh_sr, "max_out", NULL );
  cmDspSysInstallCb(h, thrh_sr,  "val_out", th0p,    "val",     NULL );
  cmDspSysInstallCb(h, thrh_sr,  "val_out", th1p,    "val",     NULL );


  // UPR scaleRange -> FX
  cmDspSysInstallCb(h, min_upr, "val",     upr_sr, "min_out", NULL );
  cmDspSysInstallCb(h, max_upr, "val",     upr_sr, "max_out", NULL );
  cmDspSysInstallCb(h, upr_sr,  "val_out", us0p,    "val",     NULL );
  cmDspSysInstallCb(h, upr_sr,  "val_out", us1p,    "val",     NULL );


  // LWR scaleRange -> FX
  cmDspSysInstallCb(h, min_lwr, "val",     lwr_sr, "min_out", NULL );
  cmDspSysInstallCb(h, max_lwr, "val",     lwr_sr, "max_out", NULL );
  cmDspSysInstallCb(h, lwr_sr,  "val_out", ls0p,    "val",     NULL );
  cmDspSysInstallCb(h, lwr_sr,  "val_out", ls1p,    "val",     NULL );


  cmDspSysNewPage(h,"Compressor");
  cmDspInst_t* cmp0_byp   = cmDspSysAllocCheck(  h, "Bypass0", 1.0 );
  cmDspInst_t* cmp0_igain = cmDspSysAllocScalar( h, "In Gain0",  0.0,   10.0, 0.1, cmpInGain);
  cmDspInst_t* cmp0_thr   = cmDspSysAllocScalar( h, "ThreshDb0", -100.0, 0.0, 0.1, cmpThreshDb);
  cmDspInst_t* cmp0_rat   = cmDspSysAllocScalar( h, "Ratio0",    0.1, 100, 0.1, cmpRatio_num);
  cmDspInst_t* cmp0_atk   = cmDspSysAllocScalar( h, "Atk Ms0",   0.0, 1000.0, 0.1, cmpAtkMs);
  cmDspInst_t* cmp0_rls   = cmDspSysAllocScalar( h, "Rls Ms0",   0.0, 1000.0, 0.1, cmpRlsMs);
  cmDspInst_t* cmp0_mkup  = cmDspSysAllocScalar( h, "Makeup0",   0.0, 10.0,   0.01, cmpMakeup);
  cmDspInst_t* cmp0_wnd   = cmDspSysAllocScalar( h, "Wnd Ms0",   1.0, cmpWndMaxMs, 1.0, cmpWndMs );
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
  cmDspInst_t* cmp1_byp   = cmDspSysAllocCheck(  h, "Bypass1", 1.0 );
  cmDspInst_t* cmp1_igain = cmDspSysAllocScalar( h, "In Gain1",  0.0,   10.0, 0.1, cmpInGain);
  cmDspInst_t* cmp1_thr   = cmDspSysAllocScalar( h, "ThreshDb1", -100.0, 0.0, 0.1, cmpThreshDb);
  cmDspInst_t* cmp1_rat   = cmDspSysAllocScalar( h, "Ratio1",    0.1, 100, 0.1, cmpRatio_num);
  cmDspInst_t* cmp1_atk   = cmDspSysAllocScalar( h, "Atk Ms1",   0.0, 1000.0, 0.1, cmpAtkMs);
  cmDspInst_t* cmp1_rls   = cmDspSysAllocScalar( h, "Rls Ms1",   0.0, 1000.0, 0.1, cmpRlsMs);
  cmDspInst_t* cmp1_mkup  = cmDspSysAllocScalar( h, "Makeup1",   0.0, 10.0,   0.01, cmpMakeup);
  cmDspInst_t* cmp1_wnd   = cmDspSysAllocScalar( h, "Wnd Ms1",   1.0, cmpWndMaxMs, 1.0, cmpWndMs );
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


  return rc;
}


cmDspRC_t _cmDspSysPgm_Switcher( cmDspSysH_t h, void** userPtrPtr )
{
  bool            useBuiltInFl = true;
  const char*     fn0          = "media/audio/20110723-Kriesberg/Audio Files/Piano 3_01.wav";
  const cmChar_t* fn           = cmFsMakeFn(cmFsUserDir(),fn0,NULL,NULL );

  bool   dsBypassFl   = false;
  double dsInGain     = 1.0;
  double dsSrate      = 96000.0;
  double dsBits       = 24.0;
  bool   dsRectifyFl  = false;
  bool   dsFullRectFl = false;
  double dsClipDb     = -10.0;

  double cfMinHz      = 20.0;
  double cfHz         = 1000.0;
  double cfAlpha      = 0.9;
  bool   cfFbFl       = true;
  bool   cfBypassFl   = false;

  unsigned xfChCnt = 2;
  double   xfMs    = 250;

  cmDspInst_t* ofp =  cmDspSysAllocInst(h,"Scalar", "Offset",  5, kNumberDuiId, 0.0,  cmDspSysSampleRate(h)*600.0, 1.0,  0.0);
  cmDspInst_t* fnp =  cmDspSysAllocInst(h,"Fname",    NULL,  3, false,"Audio Files (*.wav,*.aiff,*.aif)\tAudio Files (*.{wav,aiff,aif})",fn);
  cmDspInst_t* php =  cmDspSysAllocInst(h,"Phasor",   NULL,  0 );
  cmDspInst_t* wtp =  cmDspSysAllocInst(h,"WaveTable",NULL,  2, ((int)cmDspSysSampleRate(h)), 1 );
  
  cmDspInst_t* ds  =  cmDspSysAllocInst(h,"DistDs",   NULL, 3, dsBypassFl, dsInGain, dsSrate, dsBits  ); 
  cmDspInst_t* cf  = cmDspSysAllocInst( h,"CombFilt", NULL, 5, cfBypassFl, cfMinHz, cfFbFl, cfMinHz, cfAlpha );

  cmDspInst_t* xfad  = cmDspSysAllocInst(h,"Xfader", NULL,    2, xfChCnt, xfMs );

  cmDspInst_t* ao0p = cmDspSysAllocInst(h,"AudioOut",NULL,   1, useBuiltInFl ? 0 : 2 );
  cmDspInst_t* ao1p = cmDspSysAllocInst(h,"AudioOut",NULL,   1, useBuiltInFl ? 1 : 3 );

  
  cmDspInst_t* ign   = cmDspSysAllocScalar( h, "In Gain",      0.0, 10.0, 0.01, 1.0);
  cmDspInst_t* rct   = cmDspSysAllocCheck(  h, "Rectify",     dsRectifyFl);
  cmDspInst_t* ful   = cmDspSysAllocCheck(  h, "Full/Half",   dsFullRectFl);
  cmDspInst_t* dsr   = cmDspSysAllocScalar( h, "Srate",        0.0, 96000, 1.0, dsSrate);
  cmDspInst_t* dbt   = cmDspSysAllocScalar( h, "bits",         2.0,  32.0, 1.0, dsBits);
  cmDspInst_t* clip  = cmDspSysAllocScalar( h, "Clip dB",   -100.0,   0.0, 0.1, dsClipDb);
  cmDspInst_t* ogn   = cmDspSysAllocScalar( h, "Out Gain",    0.0, 10.0, 0.01, 1.0);

  cmDspInst_t* cfhz    = cmDspSysAllocScalar( h, "CF Hz",     25, 10000, 1, cfHz );
  cmDspInst_t* cfalpha = cmDspSysAllocScalar( h, "CF Alpha",   0.0, 2.0, 0.001, cfAlpha);
  cmDspInst_t* cfgain  = cmDspSysAllocScalar( h, "CF Gain",    0.0, 20.0, 0.001, 1.0);
  cmDspInst_t* cffb    = cmDspSysAllocInst(   h,"Button", "CF Fb",  2, kCheckDuiId, 0.0 );

  cmDspInst_t* dfdb    = cmDspSysAllocInst(   h,"Button", "Dist Fader",  2, kCheckDuiId, 0.0 );
  cmDspInst_t* cfdb    = cmDspSysAllocInst(   h,"Button", "CF Fade",     2, kCheckDuiId, 0.0 );


  cmDspSysConnectAudio(h, php, "out",   wtp,  "phs" );  // phasor -> wave table
  cmDspSysConnectAudio(h, wtp, "out",   cf,   "in" );
  cmDspSysConnectAudio(h, cf,  "out",   xfad, "in-0" ); 
  cmDspSysConnectAudio(h, xfad,"out-0", ao0p, "in");
  cmDspSysConnectAudio(h, wtp, "out",   ds,   "in" );
  cmDspSysConnectAudio(h, ds,  "out",   xfad, "in-1" );   // 
  cmDspSysConnectAudio(h, xfad,"out-1", ao1p, "in");

  cmDspSysInstallCb(h, ofp, "val", wtp, "beg", NULL ); 
  cmDspSysInstallCb(h, fnp, "out", wtp, "fn", NULL);    

  // Distortion control connections
  cmDspSysInstallCb(h, ign,  "val", ds, "igain", NULL );
  cmDspSysInstallCb(h, dsr,  "val", ds, "srate", NULL );
  cmDspSysInstallCb(h, dbt,  "val", ds, "bits",  NULL );
  cmDspSysInstallCb(h, rct,  "out", ds, "rect",  NULL );
  cmDspSysInstallCb(h, ful,  "out", ds, "full",  NULL );
  cmDspSysInstallCb(h, clip, "val", ds, "clip",  NULL );
  cmDspSysInstallCb(h, ogn,  "val", ds, "ogain", NULL );

  cmDspSysInstallCb(h, cfhz,    "val", cf,   "hz",    NULL );  
  cmDspSysInstallCb(h, cfalpha, "val", cf,   "alpha", NULL );
  cmDspSysInstallCb(h, cffb,    "out", cf,   "fb",    NULL );
  cmDspSysInstallCb(h, cfgain,  "val", ao1p, "gain",  NULL );

  cmDspSysInstallCb(h, cfdb, "out", xfad, "gate-0", NULL);
  cmDspSysInstallCb(h, dfdb, "out", xfad, "gate-1", NULL);

  return kOkDspRC;
}


