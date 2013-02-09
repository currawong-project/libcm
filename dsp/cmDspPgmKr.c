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
  unsigned        wtLoopCnt  = 1;                           // 1=play once (-1=loop forever)
  unsigned        wtInitMode = 0;                            // initial wt mode is 'silence'
  unsigned        wtSmpCnt   = floor(cmDspSysSampleRate(h)); // wt length == srate
  int             krWndSmpCnt = 2048;
  int             krHopFact   = 4;

  memset(&r,0,sizeof(r));
  cmErrSetup(&err,&cmCtx->rpt,"Kr Timeline");

  if( krLoadRsrc(h,&err,&r) != kOkDspRC )
    return rc;

  cmDspInst_t* tlp  = cmDspSysAllocInst(h,"TimeLine",    "tl",  2, r.tlFn, r.audPath );
  cmDspInst_t* scp  = cmDspSysAllocInst(h,"Score",       "sc",  1, r.scFn );
  cmDspInst_t* php  = cmDspSysAllocInst(h,"Phasor",      NULL,  1, cmDspSysSampleRate(h) );
  cmDspInst_t* wtp  = cmDspSysAllocInst(h,"WaveTable",   NULL,  4, wtSmpCnt, wtInitMode, NULL, wtLoopCnt );
  cmDspInst_t* pts  = cmDspSysAllocInst(h,"PortToSym",   NULL,  2, "on", "off" );
  cmDspInst_t* mfp  = cmDspSysAllocInst(h,"MidiFilePlay",NULL,  0 );
  cmDspInst_t* sfp  = cmDspSysAllocInst(h,"ScFol",       NULL,  1, r.scFn );
  cmDspInst_t* kr0p = cmDspSysAllocInst(h,"Kr",          NULL,   2, krWndSmpCnt, krHopFact );
  cmDspInst_t* kr1p = cmDspSysAllocInst(h,"Kr",          NULL,   2, krWndSmpCnt, krHopFact );

  cmDspInst_t* ao0p = cmDspSysAllocInst(h,"AudioOut",    NULL,  1, 0 );
  cmDspInst_t* ao1p = cmDspSysAllocInst(h,"AudioOut",    NULL,  1, 1 );

  cmDspSysNewPage(h,"Controls");
  cmDspInst_t* onb  = cmDspSysAllocInst(h,"Button", "start",  2, kButtonDuiId, 1.0 );
  cmDspInst_t* offb = cmDspSysAllocInst(h,"Button", "stop",   2, kButtonDuiId, 1.0 );
  cmDspInst_t* prtb = cmDspSysAllocInst(h,"Button", "print",  2, kButtonDuiId, 1.0 );
  cmDspInst_t* qtb  = cmDspSysAllocInst(h,"Button", "quiet",  2, kButtonDuiId, 1.0 );
  cmDspInst_t* prp  = cmDspSysAllocInst(h,"Printer", NULL,   1, ">" );
  cmDspInst_t* prd  = cmDspSysAllocInst(h,"Printer", NULL,   1, "DYN:" );
  cmDspInst_t* pre  = cmDspSysAllocInst(h,"Printer", NULL,   1, "EVEN:" );
  cmDspInst_t* prt  = cmDspSysAllocInst(h,"Printer", NULL,   1, "TEMPO:");
  //cmDspInst_t* prv  = cmDspSysAllocInst(h,"Printer", NULL,   1, "Value:");
  cmDspSysNewColumn(h,0);

  cmDspInst_t* md0p = cmDspSysAllocInst(h,"Scalar", "Mode",      5, kNumberDuiId, 0.0, 4.0, 1.0, 1.0);
  cmDspInst_t* ws0p = cmDspSysAllocInst(h,"MsgList","wndSmpCnt", 3, "wndSmpCnt", NULL, 2);
  cmDspInst_t* hf0p = cmDspSysAllocInst(h,"MsgList","hopFact",   3, "hopFact",   NULL, 2);
  cmDspInst_t* th0p = cmDspSysAllocInst(h,"Scalar", "threshold", 5, kNumberDuiId, 0.0, 100.0, 1.0,  60.0 );
  cmDspInst_t* us0p = cmDspSysAllocInst(h,"Scalar", "upr slope", 5, kNumberDuiId, 0.0,  10.0, 0.01,  0.0 ); 
  cmDspInst_t* ls0p = cmDspSysAllocInst(h,"Scalar", "lwr slope", 5, kNumberDuiId, 0.3,  10.0, 0.01,  2.0 );
  cmDspInst_t* of0p = cmDspSysAllocInst(h,"Scalar", "offset",    5, kNumberDuiId, 0.0, 100.0, 0.01, 30.0 );
  cmDspInst_t* iv0p = cmDspSysAllocInst(h,"Scalar", "invert",    5, kNumberDuiId, 0.0,   1.0, 1.0,   0.0 );  
  cmDspSysNewColumn(h,0);

  //cmDspInst_t* al1p = cmDspSysAllocInst(h,"MsgList","audFiles", 2, "audFiles",NULL);
  //cmDspInst_t* fl1p = cmDspSysAllocInst(h,"MsgList","audFrags1", 2, "audFrags",NULL);
  //cmDspInst_t* fn1p = cmDspSysAllocInst(h,"Sprintf","filename", 1, "%s/%s_%02i.wav");
  cmDspInst_t* md1p = cmDspSysAllocInst(h,"Scalar", "Mode1",      5, kNumberDuiId, 0.0, 4.0, 1.0, 1.0);
  cmDspInst_t* ws1p = cmDspSysAllocInst(h,"MsgList","wndSmpCnt1", 3, "wndSmpCnt", NULL, 2);
  cmDspInst_t* hf1p = cmDspSysAllocInst(h,"MsgList","hopFact1",   3, "hopFact",   NULL, 2);
  cmDspInst_t* th1p = cmDspSysAllocInst(h,"Scalar", "threshold1", 5, kNumberDuiId, 0.0, 100.0, 1.0,  60.0 );
  cmDspInst_t* us1p = cmDspSysAllocInst(h,"Scalar", "upr slope1", 5, kNumberDuiId, 0.0,  10.0, 0.01,  0.0 ); 
  cmDspInst_t* ls1p = cmDspSysAllocInst(h,"Scalar", "lwr slope1", 5, kNumberDuiId, 0.3,  10.0, 0.01,  2.0 );
  cmDspInst_t* of1p = cmDspSysAllocInst(h,"Scalar", "offset1",    5, kNumberDuiId, 0.0, 100.0, 0.01, 30.0 );
  cmDspInst_t* iv1p = cmDspSysAllocInst(h,"Scalar", "invert1",    5, kNumberDuiId, 0.0,   1.0, 1.0,   0.0 );  


  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    return rc;

  // phasor->wt->aout
  cmDspSysConnectAudio(h, php, "out", wtp,  "phs" );   // phs -> wt
  //cmDspSysConnectAudio(h, wtp, "out", kr0p,  "in"  );  // wt->kr
  //cmDspSysConnectAudio(h, wtp, "out", kr1p,  "in"  );
  //cmDspSysConnectAudio(h, kr0p, "out", ao0p, "in");  // kr->aout- 0
  //cmDspSysConnectAudio(h, kr1p, "out", ao1p, "in"); 
  cmDspSysConnectAudio(h, wtp, "out", ao0p, "in"  );   // wt  -> aout0
  cmDspSysConnectAudio(h, wtp, "out", ao1p, "in" );    // wt  -> aout1
 
  cmDspSysInstallCb(   h, wtp, "fidx",tlp,  "curs", NULL); 

  // start connections
  cmDspSysInstallCb(h, onb, "sym", tlp, "reset", NULL );
  cmDspSysInstallCb(h, onb, "sym", scp, "send",  NULL );
  cmDspSysInstallCb(h, onb, "sym", mfp, "sel",   NULL );
  cmDspSysInstallCb(h, onb, "sym", pts, "on",    NULL );
  cmDspSysInstallCb(h, pts, "on",  wtp, "cmd",   NULL );

  // stop connections
  cmDspSysInstallCb(h, wtp,  "done",offb,"in",  NULL ); // 'done' from WT simulates pressing Stop btn.
  cmDspSysInstallCb(h, tlp,  "mfn", pts, "off", NULL ); // Prevents WT start on new audio file from TL.
  cmDspSysInstallCb(h, offb, "sym", mfp, "sel", NULL ); 
  cmDspSysInstallCb(h, offb, "sym", pts, "off", NULL );
  cmDspSysInstallCb(h, pts,  "off", wtp, "cmd", NULL );

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
  

  // MIDI file player to score-follower and score - the order of connections is the same
  // as the msg transmision order from MFP
  //cmDspSysInstallCb(h, mfp, "smpidx", scp, "smpidx", NULL );
  cmDspSysInstallCb(h, mfp, "smpidx", sfp, "smpidx", NULL );
  //cmDspSysInstallCb(h, mfp, "d1",     scp, "d1",     NULL );
  cmDspSysInstallCb(h, mfp, "d1",     sfp, "d1",     NULL );
  //cmDspSysInstallCb(h, mfp, "d0",     scp, "d0",     NULL );
  cmDspSysInstallCb(h, mfp, "d0",     sfp, "d0",     NULL );
  //cmDspSysInstallCb(h, mfp, "status", scp, "status", NULL );
  cmDspSysInstallCb(h, mfp, "status", sfp, "status", NULL );


  // score follower to score
  //cmDspSysInstallCb(h, sfp, "out",  modp, "index", NULL );


  cmDspSysInstallCb(h, ws0p, "out", kr0p, "wndn", NULL );   // wndSmpCnt->kr
  cmDspSysInstallCb(h, hf0p, "out", kr0p, "hopf", NULL );   // hopFact->kr
  cmDspSysInstallCb(h, md0p, "val", kr0p, "mode", NULL );   // mode->kr
  cmDspSysInstallCb(h, th0p, "val", kr0p, "thrh", NULL );   // thresh->kr
  cmDspSysInstallCb(h, ls0p, "val", kr0p, "lwrs", NULL );   // lwrSlope->kr
  cmDspSysInstallCb(h, us0p, "val", kr0p, "uprs", NULL );   // uprSlope->kr
  cmDspSysInstallCb(h, of0p, "val", kr0p, "offs", NULL );   // offset->kr
  cmDspSysInstallCb(h, iv0p, "val", kr0p, "invt", NULL );   // invert->kr

  cmDspSysInstallCb(h, ws1p, "out", kr1p, "wndn", NULL );   // wndSmpCnt->kr
  cmDspSysInstallCb(h, hf1p, "out", kr1p, "hopf", NULL );   // hopFact->kr
  cmDspSysInstallCb(h, md1p, "val", kr1p, "mode", NULL );   // mode->kr
  cmDspSysInstallCb(h, th1p, "val", kr1p, "thrh", NULL );   // thresh->kr
  cmDspSysInstallCb(h, ls1p, "val", kr1p, "lwrs", NULL );   // lwrSlope->kr
  cmDspSysInstallCb(h, us1p, "val", kr1p, "uprs", NULL );   // uprSlope->kr
  cmDspSysInstallCb(h, of1p, "val", kr1p, "offs", NULL );   // offset->kr
  cmDspSysInstallCb(h, iv1p, "val", kr1p, "invt", NULL );   // invert->kr

  // Printer connections
  cmDspSysInstallCb(h, tlp, "afn",  prp, "in",  NULL );
  cmDspSysInstallCb(h, tlp, "mfn",  prp, "in",  NULL );
  cmDspSysInstallCb(h, tlp, "sel",  prp, "in",  NULL );
  cmDspSysInstallCb(h, sfp, "out",  prp, "in",     NULL );

  cmDspSysInstallCb(h, sfp, "even", pre, "in", NULL );
  cmDspSysInstallCb(h, sfp, "dyn",  prd, "in", NULL );
  cmDspSysInstallCb(h, sfp, "tempo",prt, "in", NULL );
  //cmDspSysInstallCb(h, modp,"v0",   prv, "in", NULL );
  //cmDspSysInstallCb(h, modp,"v1",   prv, "in", NULL );
  //cmDspSysInstallCb(h, modp,"v2",   prv, "in", NULL );


  cmDspSysInstallCb(h, prtb, "sym", sfp, "cmd", NULL );
  cmDspSysInstallCb(h, qtb,  "sym", sfp, "cmd", NULL );
  
  return rc;
}
