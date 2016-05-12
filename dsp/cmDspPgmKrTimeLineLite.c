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

  unsigned  sfBufCnt                 = 7;     // length of the MIDI event buffer
  unsigned  sfMaxWndCnt              = 10;    // length of the score event buffer
  unsigned  sfMinVel                 = 5;     // ignore MIDI events below this velocity
  bool      sfEnaMeasFl              = false;

 
  memset(&r,0,sizeof(r));
  cmErrSetup(&err,&cmCtx->rpt,"Kr TimelineLite");

  if( krLoadRsrc(h,&err,&r) != kOkDspRC )
    return rc;

  cmDspInst_t* ai0p = cmDspSysAllocInst(h,"AudioIn",     NULL,  1, 0);
  cmDspInst_t* ai1p = cmDspSysAllocInst(h,"AudioIn",     NULL,  1, 1);
  
  cmDspInst_t* tlp  = cmDspSysAllocInst(h,"TimeLine",    "tl",  2, r.tlFn, r.tlPrefixPath );
  cmDspInst_t* scp  = cmDspSysAllocInst(h,"Score",       "sc",  1, r.scFn );
  cmDspInst_t* pts  = cmDspSysAllocInst(h,"PortToSym",   NULL,  2, "on", "off" );

  cmDspInst_t* mfp  = cmDspSysAllocInst(h,"MidiFilePlay",NULL,  0 );
  cmDspInst_t* nmp  = cmDspSysAllocInst(h,"NanoMap",     NULL,  0 );
  cmDspInst_t* mop  = cmDspSysAllocInst(h,"MidiOut",     NULL,  2, r.midiDevice,r.midiOutPort);
  cmDspInst_t* mo2p = cmDspSysAllocInst(h,"MidiOut",     NULL,  2, r.midiDevice,r.midiOutPort2);
  cmDspInst_t* sfp  = cmDspSysAllocInst(h,"ScFol",       NULL,  1, r.scFn, sfBufCnt, sfMaxWndCnt, sfMinVel, sfEnaMeasFl );
  cmDspInst_t* amp  = cmDspSysAllocInst(h,"ActiveMeas",  NULL,  1, 100 );
  cmDspInst_t* modp = cmDspSysAllocInst(h,"ScMod",       NULL,  2, r.modFn, "m1" );
 
  unsigned   preGrpSymId     = cmDspSysPresetRegisterGroup(h,"tl");
  unsigned   cmpPreGrpSymId  = cmDspSysPresetRegisterGroup(h,"tl_cmp"); 

  cmDspTlXform_t c0,c1;

  cmDspSysNewPage(h,"Controls-0");
  _cmDspSys_TlXformChain(h, &c0, preGrpSymId, cmpPreGrpSymId, modp, 0, 0 );

  cmDspSysNewPage(h,"Controls-1");
  _cmDspSys_TlXformChain(h, &c1, preGrpSymId, cmpPreGrpSymId, modp, 1, 1 );


  cmDspInst_t* ao0p = cmDspSysAllocInst(h,"AudioOut",    NULL,   1, 2 );
  cmDspInst_t* ao1p = cmDspSysAllocInst(h,"AudioOut",    NULL,   1, 3 );

  cmDspSysNewPage(h,"Main");
  cmDspInst_t* liveb= cmDspSysAllocInst(h,"Button", "live",    2, kCheckDuiId,  0.0 );
  cmDspInst_t* simb = cmDspSysAllocInst(h,"Button", "simulate",2, kCheckDuiId,  0.0 );
  cmDspInst_t* ainb = cmDspSysAllocInst(h,"Button", "audio in",2, kCheckDuiId,  0.0 );
  cmDspInst_t* measb= cmDspSysAllocInst(h,"Button", "meas",    2, kCheckDuiId,  0.0 );
  cmDspInst_t* onb  = cmDspSysAllocInst(h,"Button", "start",   2, kButtonDuiId, 1.0 );
  cmDspInst_t* offb = cmDspSysAllocInst(h,"Button", "stop",    2, kButtonDuiId, 1.0 );
  cmDspInst_t* prp  = cmDspSysAllocInst(h,"Printer", NULL,   1, ">" );
  cmDspInst_t* prd  = cmDspSysAllocInst(h,"Printer", NULL,   1, "DYN:" );
  cmDspInst_t* pre  = cmDspSysAllocInst(h,"Printer", NULL,   1, "EVEN:" );
  cmDspInst_t* prt  = cmDspSysAllocInst(h,"Printer", NULL,   1, "TEMPO:");
  cmDspInst_t* prc  = cmDspSysAllocInst(h,"Printer", NULL,   1, "COST:");

  // Record <-> Live switches
  cmDspInst_t* tlRt  = cmDspSysAllocInst(h,"Router", NULL, 2, 2, 0);  // time line swich
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

  cmDspSysNewColumn(h,0);
  cmDspInst_t* ogain0 = cmDspSysAllocInst(h,"Scalar", "Out Gain-0",   5, kNumberDuiId, 0.0,   10.0,0.01,   1.0 );  
  cmDspInst_t* ogain1 = cmDspSysAllocInst(h,"Scalar", "Out Gain-1",   5, kNumberDuiId, 0.0,   10.0,0.01,   1.0 );  

  // Audio file recording
  cmDspInst_t* recdGain= cmDspSysAllocInst(h,"Scalar", "Recd Gain",  5, kNumberDuiId, 0.0,   100.0,0.01, 1.5 );  
  cmDspInst_t* recdChk = cmDspSysAllocInst(h,"Button", "Record",     2, kCheckDuiId, 0.0 );
  cmDspInst_t* recdPtS = cmDspSysAllocInst(h,"GateToSym", NULL,      2, cmSymTblRegisterStaticSymbol(cmDspSysSymbolTable(h),"open"),cmSymTblRegisterStaticSymbol(cmDspSysSymbolTable(h),"close"));
  cmDspInst_t* afop    = cmDspSysAllocInst(h,"AudioFileOut",NULL,    2, r.recordDir,2);
  cmDspInst_t* mi0p    = cmDspSysAllocInst(h,"AMeter","In 0",  0);
  cmDspInst_t* mi1p    = cmDspSysAllocInst(h,"AMeter","In 1",  0);


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
  cmDspSysConnectAudio( h, ai0p,   "out", mi0p,   "in" );  //
  cmDspSysConnectAudio( h, ai0p,   "out", c0.kr0, "in" );  // ain -> ch0.kr0
  cmDspSysConnectAudio( h, ai0p,   "out", c0.kr1, "in" );  // ain -> ch0.kr1
  cmDspSysConnectAudio( h, c0.cmp, "out", ao0p,   "in" );  // ch0.cmp -> aout
  cmDspSysConnectAudio( h, c0.cmp, "out", afop,   "in0");  // ch0.cmp -> audio_file_out

  cmDspSysConnectAudio( h, ai1p,   "out",   mi1p,   "in" );  //
  cmDspSysConnectAudio( h, ai1p,   "out",   c1.kr0, "in" );  // ain -> ch1.kr0
  cmDspSysConnectAudio( h, ai1p,   "out",   c1.kr1, "in" );  // ain -> ch1.kr1
  cmDspSysConnectAudio( h, c1.cmp, "out",   ao1p,   "in" );  // ch1.cmp -> aout
  cmDspSysConnectAudio( h, c1.cmp, "out",   afop,   "in1");  // ch1.cmp ->audio_file_out
  
  //cmDspSysConnectAudio(h, ai0p, "out", afop, "in0" );    // comp -> audio_file_out
  //cmDspSysConnectAudio(h, ai1p, "out", afop, "in1" );

  //cmDspSysConnectAudio(h, ai0p, "out", ao0p, "in" );     // direct through from ain to aout
  //cmDspSysConnectAudio(h, ai1p, "out", ao1p, "in" );     //    


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
  cmDspSysInstallCb(h, pts, "on",      modp,  "cmd",   NULL );
  cmDspSysInstallCb(h, onb, "sym",     amCmd, "rewind",NULL );
  cmDspSysInstallCb(h, onb, "out",     c0.achan,"reset",  NULL );
  cmDspSysInstallCb(h, onb, "out",     c1.achan,"reset",  NULL );

  // stop connections
  cmDspSysInstallCb(h, tlp,  "mfn", pts, "off", NULL ); // Prevents WT start on new audio file from TL.
  cmDspSysInstallCb(h, offb, "sym", mfp, "sel", NULL ); 
  cmDspSysInstallCb(h, offb, "sym", pts, "off", NULL );
  cmDspSysInstallCb(h, pts,  "off", modp,"cmd", NULL );
  cmDspSysInstallCb(h, offb, "sym", mop, "reset", NULL );
  cmDspSysInstallCb(h, offb, "sym", mo2p, "reset", NULL );


  // time-line to MIDI file player selection
  cmDspSysInstallCb(h, tlp, "mbsi", mfp, "bsi",   NULL );
  cmDspSysInstallCb(h, tlp, "mesi", mfp, "esi",   NULL );
  cmDspSysInstallCb(h, tlp, "mfn",  mfp, "fn",    NULL );
  
  // score to score follower - to set initial search location
  cmDspSysInstallCb(h, scp, "sel",    sfp, "index",  NULL );
  cmDspSysInstallCb(h, scp, "sel",    modp,"reset", NULL );
  cmDspSysInstallCb(h, scp, "sel",    prp, "in", NULL );

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
  cmDspSysInstallCb(h, sfp, "out",     modp,    "index", NULL );
  cmDspSysInstallCb(h, sfp, "recent",  prp,     "in",  NULL );  // report 'recent' but only act on 'max' loc index

  cmDspSysInstallCb(h, igain0, "val", ai0p, "gain", NULL );   // input gain control
  cmDspSysInstallCb(h, igain1, "val", ai1p, "gain", NULL );

  cmDspSysInstallCb(h, ogain0, "val", ao0p, "gain", NULL );   // output gain control
  cmDspSysInstallCb(h, ogain1, "val", ao1p, "gain", NULL );


  return rc;
}


//------------------------------------------------------------------------------
//)
