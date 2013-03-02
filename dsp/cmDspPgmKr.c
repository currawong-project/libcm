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
  cmDspInst_t* modp = cmDspSysAllocInst(h,"ScMod",       NULL,  2, r.modFn, "m1" );
  cmDspInst_t* kr0p = cmDspSysAllocInst(h,"Kr",          NULL,   2, krWndSmpCnt, krHopFact );
  cmDspInst_t* kr1p = cmDspSysAllocInst(h,"Kr",          NULL,   2, krWndSmpCnt, krHopFact );
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
  cmDspInst_t* wet0  = cmDspSysAllocInst(h,"Scalar", "wet",      5, kNumberDuiId, 0.0,    1.0,0.001,  1.0 );  
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
  cmDspInst_t* wet1  = cmDspSysAllocInst(h,"Scalar", "wet1",      5, kNumberDuiId, 0.0,    1.0,0.001,  1.0 );  

  cmDspSysNewColumn(h,0);
  cmDspInst_t* ogain = cmDspSysAllocInst(h,"Scalar", "Out Gain",   5, kNumberDuiId, 0.0,   10.0,0.01,   3.0 );  
  //cmDspInst_t* reload = cmDspSysAllocInst(h,"Button", "Reload",     2, kButtonDuiId, 0.0 );


  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    return rc;

  // phasor->wt->aout
  cmDspSysConnectAudio(h, php,  "out", wtp,  "phs" );   // phs -> wt

  if(1)
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
    cmDspSysConnectAudio(h, wtp, "out", ao0p, "in"  );   // wt  -> aout0
    cmDspSysConnectAudio(h, wtp, "out", ao1p, "in" );    // wt  -> aout1
    //cmDspSysConnectAudio(h, wtp, "out", af0p, "in0" );   // wt  -> audio file
  }

  // wave-table to time-line cursor
  cmDspSysInstallCb(   h, wtp, "fidx",tlp,  "curs", NULL); 

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


  // MIDI file play er to score follower
  if(1)
  {
    cmDspSysInstallCb(h, mfp, "smpidx", sfp, "smpidx", NULL );
    cmDspSysInstallCb(h, mfp, "d1",     sfp, "d1",     NULL );
    cmDspSysInstallCb(h, mfp, "d0",     sfp, "d0",     NULL );
    cmDspSysInstallCb(h, mfp, "status", sfp, "status", NULL );
  }

  // score follower to modulator and printers
  cmDspSysInstallCb(h, sfp, "out",  modp, "index", NULL );
  cmDspSysInstallCb(h, sfp, "out",  prp, "in",  NULL );
  cmDspSysInstallCb(h, sfp, "even", pre, "in", NULL );
  cmDspSysInstallCb(h, sfp, "dyn",  prd, "in", NULL );
  cmDspSysInstallCb(h, sfp, "tempo",prt, "in", NULL );

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
  cmDspSysInstallCb(h, modp, "xf0", xfad, "gate-0", NULL );

  cmDspSysInstallCb(h, modp, "mod1", md1p, "val", NULL );
  cmDspSysInstallCb(h, modp, "win1", kr1p, "wndn",NULL );
  cmDspSysInstallCb(h, modp, "thr1", th1p, "val", NULL );
  cmDspSysInstallCb(h, modp, "upr1", us1p, "val", NULL );
  cmDspSysInstallCb(h, modp, "lwr1", ls1p, "val", NULL );
  cmDspSysInstallCb(h, modp, "off1", of1p, "val", NULL );
  cmDspSysInstallCb(h, modp, "inv1", iv1p, "val", NULL );
  cmDspSysInstallCb(h, modp, "wet1", wet1, "val", NULL );
  cmDspSysInstallCb(h, modp, "xf1", xfad, "gate-1", NULL );

  
  return rc;
}

cmDspRC_t _cmDspSysPgm_Switcher(cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t rc = kOkDspRC;

  const char*     fn0          = "media/audio/20110723-Kriesberg/Audio Files/Piano 3_01.wav";
  const cmChar_t* fn           = cmFsMakeFn(cmFsUserDir(),fn0,NULL,NULL );

  bool   bypassFl   = false;
  double inGain     = 1.0;
  double dsrate     = 96000.0;
  double bits       = 24.0;
  bool   rectifyFl  = false;
  bool   fullRectFl = false;
  double clipDb     = -10.0;

  double cfMinHz    = 20.0;
  double cfHz       = 1000.0;
  double cfAlpha    = 0.9;
  bool   cfFbFl     = true;
  bool   cfBypassFl = false;

  unsigned outChCnt = 2;
  double   xfadeMs  = 250;


  cmDspInst_t* gsw = cmDspSysAllocInst(h,"GSwitch", NULL, 2, 12,2 );

  cmDspInst_t* ofp =  cmDspSysAllocInst(h,"Scalar", "Offset",  5, kNumberDuiId, 0.0,  cmDspSysSampleRate(h)*600.0, 1.0,  6900000.0);
  cmDspInst_t* fnp =  cmDspSysAllocInst(h,"Fname",    NULL,  3, false,"Audio Files (*.wav,*.aiff,*.aif)\tAudio Files (*.{wav,aiff,aif})",fn);
  cmDspInst_t* php =  cmDspSysAllocInst(h,"Phasor",   NULL,  0 );
  cmDspInst_t* wtp =  cmDspSysAllocInst(h,"WaveTable",NULL,  2, ((int)cmDspSysSampleRate(h)), 1 );

  cmDspInst_t* dst =  cmDspSysAllocInst(h,"DistDs",   NULL, 3, bypassFl, inGain, dsrate, bits  ); 
  cmDspInst_t* cf  = cmDspSysAllocInst( h,"CombFilt", NULL, 5, cfBypassFl, cfMinHz, cfFbFl, cfMinHz, cfAlpha );
  
  cmDspInst_t* xfad  = cmDspSysAllocInst(h,"Xfader", NULL,    2, outChCnt, xfadeMs );


  cmDspInst_t* ao0p = cmDspSysAllocInst(h,"AudioOut",NULL,   1, 0 );
  cmDspInst_t* ao1p = cmDspSysAllocInst(h,"AudioOut",NULL,   1, 1 );


  cmDspInst_t* ign   = cmDspSysAllocScalar( h, "In Gain",      0.0, 10.0, 0.01, 1.0);
  cmDspInst_t* rct   = cmDspSysAllocCheck(  h, "Rectify",   rectifyFl);
  cmDspInst_t* ful   = cmDspSysAllocCheck(  h, "Full/Half", fullRectFl);
  cmDspInst_t* dsr   = cmDspSysAllocScalar( h, "Srate",        0.0, 96000, 1.0, dsrate);
  cmDspInst_t* dbt   = cmDspSysAllocScalar( h, "bits",         2.0,  32.0, 1.0, bits);
  cmDspInst_t* clip  = cmDspSysAllocScalar( h, "Clip dB",   -100.0,   0.0, 0.1, clipDb);
  cmDspInst_t* ogn   = cmDspSysAllocScalar( h, "Out Gain",    0.0, 10.0, 0.01, 1.0);

  cmDspInst_t* cfhz    = cmDspSysAllocScalar( h, "CF Hz",     25, 10000, 1, cfHz );
  cmDspInst_t* cfalpha = cmDspSysAllocScalar( h, "CF Alpha",   0.0, 2.0, 0.001, cfAlpha);
  cmDspInst_t* cfgain  = cmDspSysAllocScalar( h, "CF Gain",    0.0, 20.0, 0.001, 1.0);
  cmDspInst_t* cffb    = cmDspSysAllocInst(   h,"Button", "CF Fb",  2, kCheckDuiId, 0.0 );

  cmDspInst_t* dfdb    = cmDspSysAllocInst(   h,"Button", "Dist Fade",  2, kCheckDuiId, 0.0 );
  cmDspInst_t* cfdb    = cmDspSysAllocInst(   h,"Button", "CF Fade",  2, kCheckDuiId, 0.0 );


  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    return rc;
  
  cmDspSysConnectAudio(h, php, "out", wtp,  "phs" );  // phasor -> wave table

  cmDspSysConnectAudio(h, wtp, "out",  dst,  "in" );   // wt   -> dist
  cmDspSysConnectAudio(h, dst, "out",  xfad, "in-0");  // dist -> xfad
  cmDspSysConnectAudio(h, xfad,"out-0",ao0p, "in" );   // xfad -> aout

  cmDspSysConnectAudio(h, wtp, "out",  cf,   "in" );   // wt   -> xfad
  cmDspSysConnectAudio(h, cf,  "out",  xfad, "in-1");  // xfad -> cf
  cmDspSysConnectAudio(h, xfad,"out-1",ao1p, "in" );   // cf   -> aout 

  cmDspSysInstallCb(h, ofp, "val", wtp, "beg", NULL ); // offset -> wavetable
  cmDspSysInstallCb(h, fnp, "out", wtp, "fn", NULL);   // filename -> wavetable  


  // Distortion control connections
  cmDspSysInstallCb(h, ign,  "val", dst, "igain", NULL );
  cmDspSysInstallCb(h, dsr,  "val", dst, "srate", NULL );
  cmDspSysInstallCb(h, dbt,  "val", dst, "bits", NULL );
  cmDspSysInstallCb(h, rct,  "out", dst, "rect", NULL );
  cmDspSysInstallCb(h, ful,  "out", dst, "full", NULL );
  cmDspSysInstallCb(h, clip, "val", dst, "clip", NULL );

  cmDspSysInstallCb(h, ogn,  "val", dst, "ogain", NULL );

  cmDspSysInstallCb(h, cfhz,    "val", cf, "hz",    NULL );  
  cmDspSysInstallCb(h, cfalpha, "val", cf, "alpha", NULL );
  cmDspSysInstallCb(h, cffb,    "out", cf, "fb",    NULL );
  cmDspSysInstallCb(h, cfgain,  "val", ao1p, "gain", NULL );

  cmDspSysInstallCb(h, dfdb, "out", xfad, "gate-0", NULL);
  cmDspSysInstallCb(h, cfdb, "out", xfad, "gate-1", NULL);

  return cmDspSysLastRC(h);
}

