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
#include "cmSerialPort.h"
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
  bool      sfEnaMeasFl              = true;

 
  memset(&r,0,sizeof(r));
  cmErrSetup(&err,&cmCtx->rpt,"Kr TimelineLite");

  if( krLoadRsrc(h,&err,&r) != kOkDspRC )
    return rc;

  cmDspInst_t* ai0 = cmDspSysAllocInst(h,"AudioIn",     NULL,  1, 2);
  cmDspInst_t* ai1 = cmDspSysAllocInst(h,"AudioIn",     NULL,  1, 3);
  //cmDspInst_t* mip = cmDspSysAllocInst(h,"MidiIn",      NULL,  2, "MOTU - Traveler mk3", "MIDI Port");
  //cmDspInst_t* mip = cmDspSysAllocInst(h,"MidiIn",      NULL,  2, "Apple Inc. - IAC Driver", "Bus 1");
  
  cmDspInst_t* tlp  = cmDspSysAllocInst(h,"TimeLine",    "tl",  2, r.tlFn, r.tlPrefixPath );
  cmDspInst_t* scp  = cmDspSysAllocInst(h,"Score",       "sc",  1, r.scFn );
  cmDspInst_t* pts  = cmDspSysAllocInst(h,"PortToSym",   NULL,  2, "on", "off" );

  cmDspInst_t* mfp  = cmDspSysAllocInst(h,"MidiFilePlay",NULL,  0 );
  cmDspInst_t* nmp  = cmDspSysAllocInst(h,"NanoMap",     NULL,  0 );
  cmDspInst_t* pic  = cmDspSysAllocInst(h,"Picadae",     NULL,  0 );
  cmDspInst_t* mop  = cmDspSysAllocInst(h,"MidiOut",     NULL,  2, "Scarlett 18i20 USB","Scarlett 18i20 USB MIDI 1");
  cmDspInst_t* mo2p = cmDspSysAllocInst(h,"MidiOut",     NULL,  2, "picadae","picadae MIDI 1");
  cmDspInst_t* sfp  = cmDspSysAllocInst(h,"ScFol",       NULL,  5, r.scFn, sfBufCnt, sfMaxWndCnt, sfMinVel, sfEnaMeasFl );
  cmDspInst_t* amp  = cmDspSysAllocInst(h,"ActiveMeas",  NULL,  1, 100 );
  cmDspInst_t* modp = cmDspSysAllocInst(h,"ScMod",       NULL,  2, r.modFn, "m1" );
 
  unsigned   preGrpSymId     = cmDspSysPresetRegisterGroup(h,"tl");
  unsigned   cmpPreGrpSymId  = cmDspSysPresetRegisterGroup(h,"tl_cmp"); 

  cmDspTlXform_t c0,c1;

  cmDspSysNewPage(h,"Controls-0");
  _cmDspSys_TlXformChain(h, &c0, preGrpSymId, cmpPreGrpSymId, amp, modp, 0, 0 );

  cmDspSysNewPage(h,"Controls-1");
  _cmDspSys_TlXformChain(h, &c1, preGrpSymId, cmpPreGrpSymId, amp, modp, 1, 1 );


  cmDspInst_t* ao0 = cmDspSysAllocInst(h,"AudioOut",    NULL,   1, 4 ); // 4 Piano     1 Output
  cmDspInst_t* ao1 = cmDspSysAllocInst(h,"AudioOut",    NULL,   1, 5 ); // 5          2
  cmDspInst_t* ao2 = cmDspSysAllocInst(h,"AudioOut",    NULL,   1, 2 ); // 2 Transform 1 OUtput
  cmDspInst_t* ao3 = cmDspSysAllocInst(h,"AudioOut",    NULL,   1, 3 ); // 3          2

  cmDspSysNewPage(h,"Main");
  cmDspInst_t* notesOffb= cmDspSysAllocInst(h,"Button", "notesOff",   2, kButtonDuiId, 1.0 );
  cmDspInst_t* onb     = cmDspSysAllocInst(h,"Button", "start",   2, kButtonDuiId, 1.0 );
  cmDspInst_t* offb    = cmDspSysAllocInst(h,"Button", "stop",    2, kButtonDuiId, 1.0 );
  cmDspInst_t* mod_sel = cmDspSysAllocMsgList(h, NULL, "mod_sel", 1 );
  cmDspInst_t* prp     = cmDspSysAllocInst(h,"Printer", NULL,   1, ">" );
  cmDspInst_t* prd     = cmDspSysAllocInst(h,"Printer", NULL,   1, "DYNM:" );
  cmDspInst_t* pre     = cmDspSysAllocInst(h,"Printer", NULL,   1, "EVEN:" );
  cmDspInst_t* prt     = cmDspSysAllocInst(h,"Printer", NULL,   1, "TMPO:");
  cmDspInst_t* prc     = cmDspSysAllocInst(h,"Printer", NULL,   1, "COST:");

  // Record <-> Live switches
  cmDspInst_t* tlRt  = cmDspSysAllocInst(h,"Router", NULL, 2, 2, 0);  // time line swich
  cmDspInst_t* mfpRt = cmDspSysAllocInst(h,"Router", NULL, 2, 2, 0);
  //cmDspInst_t* amRt  = cmDspSysAllocInst(h,"Router", NULL, 2, 2, 0);

  //cmDspSysNewColumn(h,0);
  cmDspInst_t* igain0 = cmDspSysAllocInst(h,"Scalar", "In Gain-0",    5, kNumberDuiId, 0.0,   100.0,0.01,   1.0 );  
  cmDspInst_t* igain1 = cmDspSysAllocInst(h,"Scalar", "In Gain-1",    5, kNumberDuiId, 0.0,   100.0,0.01,   1.0 );  

  //cmDspSysNewColumn(h,0);
  cmDspInst_t* ogain0 = cmDspSysAllocInst(h,"Scalar", "Dry Out Gain-0",   5, kNumberDuiId, 0.0,   10.0,0.01,   1.0 );  
  cmDspInst_t* ogain1 = cmDspSysAllocInst(h,"Scalar", "Dry Out Gain-1",   5, kNumberDuiId, 0.0,   10.0,0.01,   1.0 );  
  cmDspInst_t* ogain2 = cmDspSysAllocInst(h,"Scalar", "Wet Out Gain-2",   5, kNumberDuiId, 0.0,   10.0,0.01,   1.0 );  
  cmDspInst_t* ogain3 = cmDspSysAllocInst(h,"Scalar", "Wet Out Gain-3",   5, kNumberDuiId, 0.0,   10.0,0.01,   1.0 );  

  // Audio file recording
  cmDspInst_t* recdGain= cmDspSysAllocInst(h,"Scalar", "Recd Gain",  5, kNumberDuiId, 0.0,   100.0,0.01, 1.5 );  
  cmDspInst_t* recdChk = cmDspSysAllocInst(h,"Button", "Record",     2, kCheckDuiId, 0.0 );
  cmDspInst_t* recdPtS = cmDspSysAllocInst(h,"GateToSym", NULL,      2, cmSymTblRegisterStaticSymbol(cmDspSysSymbolTable(h),"open"),cmSymTblRegisterStaticSymbol(cmDspSysSymbolTable(h),"close"));
  cmDspInst_t* afop    = cmDspSysAllocInst(h,"AudioFileOut",NULL,    2, r.recordDir,2);
  cmDspInst_t* mi0p    = cmDspSysAllocInst(h,"AMeter","In 0",  0);
  cmDspInst_t* mi1p    = cmDspSysAllocInst(h,"AMeter","In 1",  0);

  cmDspInst_t* meas    = cmDspSysAllocInst(h,"Scalar", "Meas",    5, kNumberDuiId, 1.0,   59.0,1.0,   1.0 );  
  cmDspSysInstallCb( h, meas, "val", scp, "meas", NULL);
  cmDspSysInstallCb( h, meas, "val", tlp, "meas", NULL);




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
  

  //cmDspSysNewColumn(h,0);

  //--------------- Recorded performance evaluation and Active Measurement related controls
  cmDspInst_t* clrBtn  = cmDspSysAllocButton( h, "clear",  0);
  cmDspInst_t* prtBtn  = cmDspSysAllocButton( h, "dump",  0);
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
  cmDspSysConnectAudio( h, ai0,   "out", ao0,   "in" );  //  dry signal through 
  cmDspSysConnectAudio( h, ai1,   "out", ao1,   "in" );  //
  
  cmDspSysConnectAudio( h, ai0,   "out", mi0p,   "in" );  //
  cmDspSysConnectAudio( h, ai0,   "out", c0.kr0, "in" );  // ain -> ch0.kr0
  cmDspSysConnectAudio( h, ai0,   "out", c0.kr1, "in" );  // ain -> ch0.kr1
  cmDspSysConnectAudio( h, c0.cmp,"out", ao2,    "in" );  // ch0.cmp -> aout
  cmDspSysConnectAudio( h, c0.cmp,"out", afop,   "in0");  // ch0.cmp -> audio_file_out

  cmDspSysConnectAudio( h, ai1,   "out", mi1p,   "in" );  //
  cmDspSysConnectAudio( h, ai1,   "out", c1.kr0, "in" );  // ain -> ch1.kr0
  cmDspSysConnectAudio( h, ai1,   "out", c1.kr1, "in" );  // ain -> ch1.kr1
  cmDspSysConnectAudio( h, c1.cmp,"out", ao3,    "in" );  // ch1.cmp -> aout
  cmDspSysConnectAudio( h, c1.cmp,"out", afop,   "in1");  // ch1.cmp ->audio_file_out
  


  cmDspSysInstallCb( h, clrBtn, "sym",    amp, "cmd",   NULL ); // clear active meas.
  cmDspSysInstallCb( h, prtBtn, "sym",    modp, "cmd",   NULL ); // print active meas
  //cmDspSysInstallCb( h, prtBtn, "sym",    scp, "cmd",   NULL ); // print the score
  cmDspSysInstallCb( h, amCmd,  "add",    amp, "cmd",   NULL ); // add active meas
  cmDspSysInstallCb( h, amCmd,  "rewind", amp, "cmd",   NULL ); // rewind active meas
  //cmDspSysInstallCb( h, sfp,    "out",    amRt, "f-in", NULL ); // sfp-active meas router (rtr is switched by live btn)
  //cmDspSysInstallCb( h, amRt,   "f-out-0",amp, "sfloc", NULL ); 
  cmDspSysInstallCb( h, sfp,    "out",    amp, "sfloc", NULL );
  
  cmDspSysInstallCb( h, sfp, "vloc", amp,   "loc",  NULL ); // live meas's to active meas unit
  cmDspSysInstallCb( h, sfp, "vval", amp,   "val",  NULL ); //
  cmDspSysInstallCb( h, sfp, "vcost",amp,   "cst",  NULL ); //
  cmDspSysInstallCb( h, sfp, "vtyp", amp,   "type", NULL ); //
  cmDspSysInstallCb( h, sfp, "vtyp", amCmd, "add",  NULL);  //

  // ***** delete this to prevent the score follower from driving the active-measure unit in 'live' mode
  //cmDspSysInstallCb( h, amRt,   "f-out-1",amp, "sfloc", NULL );
  // *****

  // active measure loc to xfad channel trigger
  /*
  cmDspSysInstallCb( h, amp,       "scloc",c0.achan, "trig", NULL );  // See Also: modp.sw ->achan.trig
  cmDspSysInstallCb( h, amp,       "scloc",c1.achan, "trig", NULL );
  cmDspSysInstallCb( h, recallBtn, "sym",  c0.achan, "trig", NULL ); 
  cmDspSysInstallCb( h, recallBtn, "sym",  c1.achan, "trig", NULL );
  */
  
  cmDspSysInstallCb( h, amp,    "even",   pre,        "in",   NULL );  // active meas output to printers
  cmDspSysInstallCb( h, amp,    "even",   modp,       "even", NULL );

  cmDspSysInstallCb( h, amp,    "dyn",    prd,        "in",   NULL );
  cmDspSysInstallCb( h, amp,    "dyn",    modp,       "dyn",  NULL );
  
  cmDspSysInstallCb( h, amp,    "tempo",  prt,        "in",   NULL );
  cmDspSysInstallCb( h, amp,    "tempo",  modp,       "tempo",NULL );
  
  cmDspSysInstallCb( h, amp,    "cost",   prc,        "in",   NULL );
  cmDspSysInstallCb( h, amp,    "cost",   modp,       "cost", NULL );

  
  /*
  cmDspSysInstallCb( h, sfp, "vloc", pre,   "in",  NULL ); // live meas's to active meas unit
  cmDspSysInstallCb( h, sfp, "vval", prd,   "in",  NULL ); //
  cmDspSysInstallCb( h, sfp, "vcost",prt,   "in",  NULL ); //
  cmDspSysInstallCb( h, sfp, "vtyp", prc,   "in", NULL ); //
  */
  // wave-table to time-line cursor
  //cmDspSysInstallCb(   h, wtp, "fidx",tlp,  "curs", NULL); 

  cmDspSysInstallCb(h, prePath, "out", tlp, "path", NULL );

  cmDspSysInstallCb(h, notesOffb,  "sym",    pic, "alloff",  NULL );
  
  // start connections
  cmDspSysInstallCb(h, onb,  "sym",    tlRt, "s-in",  NULL );
  cmDspSysInstallCb(h, tlRt, "s-out-0",tlp,  "reset", NULL );
  cmDspSysInstallCb(h, onb,  "sym",    scp,  "send",  NULL );
  cmDspSysInstallCb(h, onb,  "sym",    mfpRt,"s-in",  NULL );
  cmDspSysInstallCb(h, mfpRt,"s-out-0",mfp,  "sel",   NULL );

  cmDspSysInstallCb(h, onb, "sym",     pts,     "on",     NULL );
  cmDspSysInstallCb(h, pts, "on",      modp,    "cmd",    NULL );
  cmDspSysInstallCb(h, onb, "sym",     amCmd,   "rewind", NULL );
  cmDspSysInstallCb(h, onb, "out",     c0.achan,"reset",  NULL );
  cmDspSysInstallCb(h, onb, "out",     c1.achan,"reset",  NULL );

  cmDspSysInstallCb(h, mod_sel,"out",  modp,  "sel", NULL );

  // stop connections
  cmDspSysInstallCb(h, tlp,  "mfn", pts, "off",   NULL ); // Prevents WT start on new audio file from TL.
  cmDspSysInstallCb(h, offb, "sym", mfp, "sel",   NULL ); 
  cmDspSysInstallCb(h, offb, "sym", pts, "off",   NULL );
  cmDspSysInstallCb(h, pts,  "off", modp,"cmd",   NULL );
  cmDspSysInstallCb(h, offb, "sym", mop, "reset", NULL );
  cmDspSysInstallCb(h, offb, "sym", mo2p,"reset", NULL );


  // time-line to MIDI file player selection
  cmDspSysInstallCb(h, tlp, "mbsi", mfp, "bsi",   NULL );
  cmDspSysInstallCb(h, tlp, "mesi", mfp, "esi",   NULL );
  cmDspSysInstallCb(h, tlp, "mfn",  mfp, "fn",    NULL );
  
  // score to score follower - to set initial search location
  cmDspSysInstallCb(h, scp, "sel",    sfp, "index", NULL );
  cmDspSysInstallCb(h, scp, "sel",    modp,"reset", NULL );
  cmDspSysInstallCb(h, scp, "sel",    prp, "in",    NULL );

  cmDspInst_t* msrc = mfp;  // switch MIDI source (mfp or mip)
  
  // MIDI file player to score follower and sampler
  cmDspSysInstallCb(h, msrc,  "smpidx",  sfp, "smpidx",NULL );
  //cmDspSysInstallCb(h, mfp,  "id",      sfp, "muid",  NULL );

  cmDspSysInstallCb(h, msrc,   "d1",     sfp,  "d1",    NULL );
  cmDspSysInstallCb(h, msrc,   "d1",     nmp,  "d1",    NULL );
  cmDspSysInstallCb(h, nmp,   "d1",     mop,  "d1",    NULL );
  cmDspSysInstallCb(h, nmp,   "d1",     pic, "d1",    NULL );
  cmDspSysInstallCb(h, pic,   "d1",     mo2p, "d1",    NULL );

  cmDspSysInstallCb(h, msrc,  "d0",      sfp,  "d0",   NULL );
  cmDspSysInstallCb(h, msrc,  "d0",      nmp,  "d0",   NULL );
  cmDspSysInstallCb(h, nmp,  "d0",      mop,  "d0",   NULL );
  cmDspSysInstallCb(h, nmp,  "d0",      pic, "d0",   NULL );
  cmDspSysInstallCb(h, pic,   "d0",     mo2p, "d0",    NULL );

  cmDspSysInstallCb(h, msrc,  "status",  sfp,  "status",NULL );
  cmDspSysInstallCb(h, msrc,  "status",  nmp,  "status",NULL );
  cmDspSysInstallCb(h, nmp,  "status",  mop,  "status",NULL );
  cmDspSysInstallCb(h, nmp,  "status",  pic, "status",NULL );
  cmDspSysInstallCb(h, pic,   "status",  mo2p, "status",    NULL );


  // score follower to recd_play,modulator and printers
  cmDspSysInstallCb(h, sfp, "out",     modp,    "index", NULL );
  cmDspSysInstallCb(h, sfp, "recent",  prp,     "in",  NULL );  // report 'recent' but only act on 'max' loc index

  cmDspSysInstallCb(h, igain0, "val", ai0, "gain", NULL );   // input gain control
  cmDspSysInstallCb(h, igain1, "val", ai1, "gain", NULL );


  cmDspSysInstallCb(h, modp, "dgain0",  ogain0, "val",  NULL );
  cmDspSysInstallCb(h, modp, "dgain1",  ogain1, "val",  NULL );
  cmDspSysInstallCb(h, modp, "wgain0",  ogain2, "val",  NULL );
  cmDspSysInstallCb(h, modp, "wgain1",  ogain3, "val",  NULL );

  cmDspSysInstallCb(h, ogain0, "val", ao0, "gain", NULL );   // output gain control - dry 0
  cmDspSysInstallCb(h, ogain1, "val", ao1, "gain", NULL );   //                       dry 1
  cmDspSysInstallCb(h, ogain2, "val", ao2, "gain", NULL );   //                       wet 0
  cmDspSysInstallCb(h, ogain3, "val", ao3, "gain", NULL );   //                       wet 1


  return rc;
}


//------------------------------------------------------------------------------
//)
