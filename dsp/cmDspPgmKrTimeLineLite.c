//( { file_desc:"'snap' Performance analysis Simplified Time Line program." kw:[snap]}

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
#include "cmDspPgmKrChain.h"
//)

//( { label:cmDspPgm_TimeLineLite file_desc:"Simplified score vs. the performance and generating related audio transforms." kw:[spgm] }

cmDspRC_t _cmDspSysPgm_TimeLineLite(cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t rc                       = kOkDspRC;
  cmCtx_t*  cmCtx                    = cmDspSysPgmCtx(h);
  cmErr_t   err;
  krRsrc_t  r;

  //unsigned  wtLoopCnt                = 1; // 1=play once (-1=loop forever)
  //unsigned  wtInitMode               = 0; // initial wt mode is 'silence'
  //unsigned  wtSmpCnt                 = floor(cmDspSysSampleRate(h)); // wt length == srate

  unsigned  sfBufCnt                 = 7;     // length of the MIDI event buffer
  unsigned  sfMaxWndCnt              = 10;    // length of the score event buffer
  unsigned  sfMinVel                 = 5;     // ignore MIDI events below this velocity
  bool      sfEnaMeasFl              = false;

  double    recdPlayInitAllocSecs    = 10.0;
  double    recdPlayMaxLaSecs        = 2.0;
  double    recdPlayCurLaSecs        = 0.1;
  double    recdPlayFadeRateDbPerSec = 4.0;


  bool            apfBypassFl  = false;
  unsigned        apfModeSymId  = cmSymTblRegisterStaticSymbol(cmDspSysSymbolTable(h),"AP");
  double          apfF0hz       = 100.0;
  double          apfQ          = 1.0;
  double          apfGain       = 1.0;
 
  memset(&r,0,sizeof(r));
  cmErrSetup(&err,&cmCtx->rpt,"Kr TimelineLite");

  if( krLoadRsrc(h,&err,&r) != kOkDspRC )
    return rc;

  cmDspInst_t* ai0p = cmDspSysAllocInst(h,"AudioIn",     NULL,  1, 0);
  cmDspInst_t* ai1p = cmDspSysAllocInst(h,"AudioIn",     NULL,  1, 1);
  cmDspInst_t* ai2p = cmDspSysAllocInst(h,"AudioIn",     NULL,  1, 2);
  cmDspInst_t* ai3p = cmDspSysAllocInst(h,"AudioIn",     NULL,  1, 3);

  cmDspInst_t* mx0p = cmDspSysAllocInst( h, "AMix",      NULL, 3, 2, 0.5, 0.5);
  cmDspInst_t* mx1p = cmDspSysAllocInst( h, "AMix",      NULL, 3, 2, 0.5, 0.5);

  cmDspInst_t* tlp  = cmDspSysAllocInst(h,"TimeLine",    "tl",  2, r.tlFn, r.tlPrefixPath );
  cmDspInst_t* scp  = cmDspSysAllocInst(h,"Score",       "sc",  1, r.scFn );
  //cmDspInst_t* php  = cmDspSysAllocInst(h,"Phasor",      NULL,  1, cmDspSysSampleRate(h) );
  //cmDspInst_t* wtp  = cmDspSysAllocInst(h,"WaveTable",   NULL,  4, wtSmpCnt, wtInitMode, NULL, wtLoopCnt );
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

  cmDspTlXform_t c0,c1;

  cmDspSysNewPage(h,"Controls-0");
  _cmDspSys_TlXformChain(h, &c0, preGrpSymId, cmpPreGrpSymId, modp, 0, 0 );

  cmDspSysNewPage(h,"Controls-1");
  _cmDspSys_TlXformChain(h, &c1, preGrpSymId, cmpPreGrpSymId, modp, 1, 1 );


  cmDspInst_t* apf0 =  cmDspSysAllocInst(h,"BiQuadEq",NULL, 5, apfBypassFl, apfModeSymId, apfF0hz+0, apfQ, apfGain  ); 
  cmDspInst_t* apf1 =  cmDspSysAllocInst(h,"BiQuadEq",NULL, 5, apfBypassFl, apfModeSymId, apfF0hz+100.0, apfQ, apfGain  ); 
  cmDspInst_t* apf2 =  cmDspSysAllocInst(h,"BiQuadEq",NULL, 5, apfBypassFl, apfModeSymId, apfF0hz+200.0, apfQ, apfGain  ); 
  cmDspInst_t* apf3 =  cmDspSysAllocInst(h,"BiQuadEq",NULL, 5, apfBypassFl, apfModeSymId, apfF0hz+300.0, apfQ, apfGain  ); 

  cmDspInst_t* ao0p = cmDspSysAllocInst(h,"AudioOut",    NULL,   1, 2 );
  cmDspInst_t* ao1p = cmDspSysAllocInst(h,"AudioOut",    NULL,   1, 3 );
  cmDspInst_t* ao2p = cmDspSysAllocInst(h,"AudioOut",    NULL,   1, 4 );
  cmDspInst_t* ao3p = cmDspSysAllocInst(h,"AudioOut",    NULL,   1, 5 );

  cmDspSysNewPage(h,"Main");
  cmDspInst_t* liveb= cmDspSysAllocInst(h,"Button", "live",    2, kCheckDuiId,  0.0 );
  cmDspInst_t* simb = cmDspSysAllocInst(h,"Button", "simulate",2, kCheckDuiId,  0.0 );
  cmDspInst_t* ainb = cmDspSysAllocInst(h,"Button", "audio in",2, kCheckDuiId,  0.0 );
  cmDspInst_t* measb= cmDspSysAllocInst(h,"Button", "meas",    2, kCheckDuiId,  0.0 );
  cmDspInst_t* onb  = cmDspSysAllocInst(h,"Button", "start",   2, kButtonDuiId, 1.0 );
  cmDspInst_t* offb = cmDspSysAllocInst(h,"Button", "stop",    2, kButtonDuiId, 1.0 );
  //cmDspInst_t* prtb = cmDspSysAllocInst(h,"Button", "print",   2, kButtonDuiId, 1.0 );
  //cmDspInst_t* qtb  = cmDspSysAllocInst(h,"Button", "quiet",   2, kButtonDuiId, 1.0 );
  cmDspInst_t* prp  = cmDspSysAllocInst(h,"Printer", NULL,   1, ">" );
  cmDspInst_t* prd  = cmDspSysAllocInst(h,"Printer", NULL,   1, "DYN:" );
  cmDspInst_t* pre  = cmDspSysAllocInst(h,"Printer", NULL,   1, "EVEN:" );
  cmDspInst_t* prt  = cmDspSysAllocInst(h,"Printer", NULL,   1, "TEMPO:");
  cmDspInst_t* prc  = cmDspSysAllocInst(h,"Printer", NULL,   1, "COST:");
  //cmDspInst_t* prv  = cmDspSysAllocInst(h,"Printer", NULL,   1, "Value:");

  // Record <-> Live switches
  cmDspInst_t* tlRt  = cmDspSysAllocInst(h,"Router", NULL, 2, 2, 0);  // time line swich
  //cmDspInst_t* wtRt  = cmDspSysAllocInst(h,"Router", NULL, 2, 2, 0);
  cmDspInst_t* mfpRt = cmDspSysAllocInst(h,"Router", NULL, 2, 2, 0);
  cmDspInst_t* amRt  = cmDspSysAllocInst(h,"Router", NULL, 2, 2, 0);
  cmDspInst_t* au0Sw = cmDspSysAllocInst(h,"1ofN",   NULL, 2, 2, 0);
  cmDspInst_t* au1Sw = cmDspSysAllocInst(h,"1ofN",   NULL, 2, 2, 0);

  cmDspInst_t* siRt  = cmDspSysAllocInst(h,"Router", NULL, 2, 2, 0);  
  cmDspInst_t* muRt  = cmDspSysAllocInst(h,"Router", NULL, 2, 2, 0);  
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




  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    return rc;

  // Output Audio file recording.
  cmDspSysInstallCb(h, recdGain,"val", afop,    "gain0", NULL );
  cmDspSysInstallCb(h, recdGain,"val", afop,    "gain1", NULL );
  cmDspSysInstallCb(h, recdChk, "out", recdPtS, "on",    NULL );
  cmDspSysInstallCb(h, recdChk, "out", recdPtS, "off",   NULL );
  cmDspSysInstallCb(h, recdPtS, "out", afop,    "sel",   NULL );


  // Audio connections
  //cmDspSysConnectAudio(h, php,  "out",   wtp,  "phs" );     // phs -> wt

  cmDspSysConnectAudio( h, ai0p, "out", mi0p, "in");
  cmDspSysConnectAudio( h, ai1p, "out", mi1p, "in");
  cmDspSysConnectAudio( h, ai2p, "out", mi2p, "in");
  cmDspSysConnectAudio( h, ai3p, "out", mi3p, "in");


  cmDspSysConnectAudio(h, ai0p, "out", mx0p, "in-0" );  // eqmix -> input mix
  cmDspSysConnectAudio(h, ai1p, "out", mx1p, "in-0" );
  cmDspSysConnectAudio(h, ai2p, "out", mx0p, "in-1" );
  cmDspSysConnectAudio(h, ai3p, "out", mx1p, "in-1" );

  cmDspSysConnectAudio(h, mx0p,  "out", rpp,   "in-0");    // sw  -> rcdply


  cmDspSysConnectAudio(h, mx0p,   "out",   c0.kr0, "in" ); // ain -> sw


  cmDspSysConnectAudio(h, c0.cmp, "out", apf0, "in" );
  cmDspSysConnectAudio(h, c0.cmp, "out", apf2, "in" );
  cmDspSysConnectAudio(h, apf0,  "out", ao0p,  "in" );
  cmDspSysConnectAudio(h, apf2,  "out", ao2p,  "in" );


  cmDspSysConnectAudio(h, mx1p,  "out", rpp,   "in-1");    // sw  -> rcdply

  cmDspSysConnectAudio(h, mx1p,   "out",   c1.kr0, "in" ); // ain -> sw


  cmDspSysConnectAudio(h, c1.cmp, "out",   apf1,  "in" );   // cmp -> mix 0
  cmDspSysConnectAudio(h, c1.cmp, "out",   apf3,  "in" );   // cmp -> mix 0
  cmDspSysConnectAudio(h, apf1,  "out",   ao1p,   "in" );
  cmDspSysConnectAudio(h, apf3,  "out",   ao3p,   "in" );

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

  cmDspSysInstallCb( h, amp,    "scloc",  c1.achan,   "trig", NULL );

  cmDspSysInstallCb( h, amp,    "even",   pre,        "in",   NULL );  // active meas output to printers
  cmDspSysInstallCb( h, amp,    "dyn",    prd,        "in",   NULL );
  cmDspSysInstallCb( h, amp,    "tempo",  prt,        "in",   NULL );
  cmDspSysInstallCb( h, amp,    "cost",   prc,        "in",   NULL );

  // wave-table to time-line cursor
  //cmDspSysInstallCb(   h, wtp, "fidx",tlp,  "curs", NULL); 

  cmDspSysInstallCb(h, prePath, "out", tlp, "path", NULL );

  // 'live' button -> live router selector switch 
  //cmDspSysInstallCb(h, liveb, "out",  wtRt, "sel", NULL );
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
  cmDspSysInstallCb(h, simb,  "out",  muRt,  "sel", NULL );
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
  //cmDspSysInstallCb(h, pts, "on",      wtRt,  "s-in",  NULL );
  //cmDspSysInstallCb(h, wtRt,"s-out-0", wtp,   "cmd",   NULL );
  cmDspSysInstallCb(h, pts, "on",      modp,  "cmd",   NULL );
  cmDspSysInstallCb(h, pts, "on",      modr,  "cmd",   NULL );
  cmDspSysInstallCb(h, pts, "on",      rpp,   "cmd",   NULL );
  cmDspSysInstallCb(h, onb, "sym",     amCmd, "rewind",NULL );
  cmDspSysInstallCb(h, onb, "out",     c0.achan,"reset",  NULL );
  cmDspSysInstallCb(h, onb, "out",     c1.achan,"reset",  NULL );

  // stop connections
  //cmDspSysInstallCb(h, wtp,  "done",offb,"in",  NULL ); // 'done' from WT simulates pressing Stop btn.
  cmDspSysInstallCb(h, tlp,  "mfn", pts, "off", NULL ); // Prevents WT start on new audio file from TL.
  cmDspSysInstallCb(h, offb, "sym", mfp, "sel", NULL ); 
  cmDspSysInstallCb(h, offb, "sym", pts, "off", NULL );
  //cmDspSysInstallCb(h, pts,  "off", wtp, "cmd", NULL );
  cmDspSysInstallCb(h, pts,  "off", modp,"cmd", NULL );
  cmDspSysInstallCb(h, pts,  "off", modr,"cmd", NULL );
  cmDspSysInstallCb(h, offb, "sym", mop, "reset", NULL );
  cmDspSysInstallCb(h, offb, "sym", mo2p, "reset", NULL );


  // time-line to wave-table selection 
  //cmDspSysInstallCb(h, tlp, "absi", wtp, "beg", NULL );  
  //cmDspSysInstallCb(h, tlp, "aesi", wtp, "end", NULL );
  //cmDspSysInstallCb(h, tlp, "afn",  wtp, "fn",  NULL );

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

  cmDspSysInstallCb(h, mfp,  "id",      muRt, "f-in",  NULL );
  cmDspSysInstallCb(h, muRt, "f-out-1", sfp,  "muid",    NULL );
  // leave muRt.f-out-1 unconnected because it should be ignored in 'simulate mode'

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

  cmDspSysInstallCb(h, apfByp, "out", apf0, "bypass", NULL );   // APF bypass
  cmDspSysInstallCb(h, apfByp, "out", apf1, "bypass", NULL );   // 
  cmDspSysInstallCb(h, apfByp, "out", apf2, "bypass", NULL );   // 
  cmDspSysInstallCb(h, apfByp, "out", apf3, "bypass", NULL );   // 


  cmDspSysInstallCb(h, ogain0, "val", ao0p, "gain", NULL );   // output gain control
  cmDspSysInstallCb(h, ogain1, "val", ao1p, "gain", NULL );
  cmDspSysInstallCb(h, ogain2, "val", ao2p, "gain", NULL );
  cmDspSysInstallCb(h, ogain3, "val", ao3p, "gain", NULL );




  return rc;
}


//------------------------------------------------------------------------------
//)
