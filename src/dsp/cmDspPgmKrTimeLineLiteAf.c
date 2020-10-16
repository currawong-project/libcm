//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
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

cmDspRC_t _cmDspSysPgm_TimeLineLiteAf(cmDspSysH_t h, void** userPtrPtr )
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

  //int baseAudioInCh =  0; // 2;
  int baseAudioOutCh = 0;//  2;

  
  cmDspInst_t* tlp  = cmDspSysAllocInst(h,"TimeLine",    "tl",  2, r.tlFn, r.tlPrefixPath );
  cmDspInst_t* scp  = cmDspSysAllocInst(h,"Score",       "sc",  1, r.scFn );
  cmDspInst_t* pts  = cmDspSysAllocInst(h,"PortToSym",   NULL,  2, "on", "off" );

  cmDspInst_t* php =  cmDspSysAllocInst(h,"Phasor",    NULL,  0 );
  cmDspInst_t* wt0 =  cmDspSysAllocInst(h,"WaveTable", NULL,  7, ((int)cmDspSysSampleRate(h)), 1, NULL, -1, 0, -1, 0 );
  cmDspInst_t* wt1 =  cmDspSysAllocInst(h,"WaveTable", NULL,  7, ((int)cmDspSysSampleRate(h)), 1, NULL, -1, 0, -1, 1 );

  
  cmDspInst_t* mfp  = cmDspSysAllocInst(h,"MidiFilePlay",NULL,  0 );
  cmDspInst_t* nmp  = cmDspSysAllocInst(h,"NanoMap",     NULL,  0 );
  cmDspInst_t* sfp  = cmDspSysAllocInst(h,"ScFol",       NULL,  5, r.scFn, sfBufCnt, sfMaxWndCnt, sfMinVel, sfEnaMeasFl );
  cmDspInst_t* amp  = cmDspSysAllocInst(h,"ActiveMeas",  NULL,  1, 100 );
  cmDspInst_t* modp = cmDspSysAllocInst(h,"ScMod",       NULL,  2, r.modFn, "m1" );
  cmDspInst_t* its  = cmDspSysAllocInst(h,"IntToSym",    NULL,  2, 0, "off");
 
  unsigned   preGrpSymId     = cmDspSysPresetRegisterGroup(h,"tl");
  unsigned   cmpPreGrpSymId  = cmDspSysPresetRegisterGroup(h,"tl_cmp"); 

  cmDspTlXform_t c0,c1;
  memset(&c0,0,sizeof(c0));
  memset(&c1,0,sizeof(c1));
    

  cmDspSysNewPage(h,"Controls-0");
  _cmDspSys_TlXformChain(h, &c0, preGrpSymId, cmpPreGrpSymId, amp, modp, 0, 0 );

  cmDspSysNewPage(h,"Controls-1");
  _cmDspSys_TlXformChain(h, &c1, preGrpSymId, cmpPreGrpSymId, amp, modp, 1, 1 );

  cmDspInst_t* lmix = cmDspSysAllocInst(h, "AMix",      NULL, 1, 2 );
  cmDspInst_t* rmix = cmDspSysAllocInst(h, "AMix",      NULL, 1, 2 );

  cmDspInst_t* ao0 = cmDspSysAllocInst(h,"AudioOut",    NULL,   1, baseAudioOutCh+0 ); 
  cmDspInst_t* ao1 = cmDspSysAllocInst(h,"AudioOut",    NULL,   1, baseAudioOutCh+1 ); 

  cmDspSysNewPage(h,"Main");
  //cmDspInst_t* notesOffb= cmDspSysAllocInst(h,"Button", "notesOff",   2, kButtonDuiId, 1.0 );
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


  //cmDspSysNewColumn(h,0);
  cmDspInst_t* ogain0 = cmDspSysAllocInst(h,"Scalar", "Dry Out Gain-0",   5, kNumberDuiId, 0.0,   10.0,0.01,   1.0 );  
  cmDspInst_t* ogain1 = cmDspSysAllocInst(h,"Scalar", "Dry Out Gain-1",   5, kNumberDuiId, 0.0,   10.0,0.01,   1.0 );  
  cmDspInst_t* ogain2 = cmDspSysAllocInst(h,"Scalar", "Wet Out Gain-2",   5, kNumberDuiId, 0.0,   10.0,0.01,   1.0 );  
  cmDspInst_t* ogain3 = cmDspSysAllocInst(h,"Scalar", "Wet Out Gain-3",   5, kNumberDuiId, 0.0,   10.0,0.01,   1.0 );  

  cmDspInst_t* ogainW = cmDspSysAllocInst(h,"Scalar", "Wet Master",   5, kNumberDuiId, 0.0,   10.0,0.01,   1.0 );  
  cmDspInst_t* ogainD = cmDspSysAllocInst(h,"Scalar", "Dry Master",   5, kNumberDuiId, 0.0,   10.0,0.01,   1.0 );  

  cmDspInst_t* gmult0  = cmDspSysAllocInst(h,"ScalarOp", NULL, 6, 2, "*$", "in-0", 1.0, "in-1", 1.0 );
  cmDspInst_t* gmult1  = cmDspSysAllocInst(h,"ScalarOp", NULL, 6, 2, "*$", "in-0", 1.0, "in-1", 1.0 );
  cmDspInst_t* gmult2  = cmDspSysAllocInst(h,"ScalarOp", NULL, 6, 2, "*$", "in-0", 1.0, "in-1", 1.0 );
  cmDspInst_t* gmult3  = cmDspSysAllocInst(h,"ScalarOp", NULL, 6, 2, "*$", "in-0", 1.0, "in-1", 1.0 );
  
 
  
  // Audio file recording
  cmDspInst_t* recdGain= cmDspSysAllocInst(h,"Scalar", "Recd Gain",  5, kNumberDuiId, 0.0,   100.0,0.01, 1.5 );  
  cmDspInst_t* recdChk = cmDspSysAllocInst(h,"Button", "Record",     2, kCheckDuiId, 0.0 );
  cmDspInst_t* recdPtS = cmDspSysAllocInst(h,"GateToSym", NULL,      2, cmSymTblRegisterStaticSymbol(cmDspSysSymbolTable(h),"open"),cmSymTblRegisterStaticSymbol(cmDspSysSymbolTable(h),"close"));
  cmDspInst_t* afop    = cmDspSysAllocInst(h,"AudioFileOut",NULL,    2, r.recordDir,2);
  cmDspInst_t* mi0p    = cmDspSysAllocInst(h,"AMeter","In 0",  0);
  cmDspInst_t* mi1p    = cmDspSysAllocInst(h,"AMeter","In 1",  0);

  cmDspInst_t* meas    = cmDspSysAllocInst(h,"Scalar", "Begin Meas", 5, kNumberDuiId, 1.0, 1000.0, 1.0, 1.0 );  
  cmDspInst_t* eloc    = cmDspSysAllocInst(h,"Scalar", "End   Loc",  5, kNumberDuiId, 1.0, 1000.0, 1.0, 1.0 );
  cmDspInst_t* sfp_loc = cmDspSysAllocInst(h,"Label",  NULL, 1, "sf loc:");

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
  cmDspSysConnectAudio(h, php, "out", wt0, "phs" );      // phasor -> wave table
  cmDspSysConnectAudio(h, php, "out", wt1, "phs" );      // 
  
  cmDspSysConnectAudio(h, wt0, "out", lmix, "in-0" );    // wave table -> audio out (dry output)
  cmDspSysConnectAudio(h, wt1, "out", rmix, "in-0" );    // 

  //cmDspSysConnectAudio(h, wt0, "out", lmix, "in-1" );    // wave table -> audio out (dry output)
  //cmDspSysConnectAudio(h, wt1, "out", rmix, "in-1" );    // 


  cmDspSysConnectAudio( h, wt0,   "out", mi0p,   "in" );  //
  cmDspSysConnectAudio( h, wt0,   "out", c0.kr0, "in" );  // ain -> ch0.kr0
  cmDspSysConnectAudio( h, wt0,   "out", c0.kr1, "in" );  // ain -> ch0.kr1

  
  cmDspSysConnectAudio( h, c0.cmp,"out", lmix,    "in-1" );  // ch0.cmp -> aout
  cmDspSysConnectAudio( h, c0.cmp,"out", afop,   "in0");  // ch0.cmp -> audio_file_out


  cmDspSysConnectAudio( h, wt1,   "out", mi1p,   "in" );  //
  cmDspSysConnectAudio( h, wt1,   "out", c1.kr0, "in" );  // ain -> ch1.kr0
  cmDspSysConnectAudio( h, wt1,   "out", c1.kr1, "in" );  // ain -> ch1.kr1

  cmDspSysConnectAudio( h, c1.cmp,"out", rmix,    "in-1" );  // ch1.cmp -> aout
  cmDspSysConnectAudio( h, c1.cmp,"out", afop,   "in1");  // ch1.cmp ->audio_file_out
  
  cmDspSysConnectAudio( h, lmix, "out", ao0, "in" );
  cmDspSysConnectAudio( h, rmix, "out", ao1, "in" );


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
  cmDspSysInstallCb(   h, wt0, "fidx",tlp,  "curs", NULL); 

  cmDspSysInstallCb(h, prePath, "out", tlp, "path", NULL );

  //cmDspSysInstallCb(h, notesOffb,  "sym",    pic, "alloff",  NULL );
  
  // start connections
  cmDspSysInstallCb(h, onb,  "sym",    tlRt, "s-in",  NULL );
  cmDspSysInstallCb(h, tlRt, "s-out-0",tlp,  "reset", NULL );
  cmDspSysInstallCb(h, onb,  "sym",    scp,  "send",  NULL );
  cmDspSysInstallCb(h, onb,  "sym",    mfpRt,"s-in",  NULL );
  cmDspSysInstallCb(h, mfpRt,"s-out-0",mfp,  "sel",   NULL );

  cmDspSysInstallCb(h, onb, "sym",     pts,     "on",     NULL );
  cmDspSysInstallCb(h, pts, "on",      wt0,     "cmd",    NULL );
  cmDspSysInstallCb(h, pts, "on",      wt1,     "cmd",    NULL );
  cmDspSysInstallCb(h, pts, "on",      modp,    "cmd",    NULL );
  cmDspSysInstallCb(h, onb, "sym",     amCmd,   "rewind", NULL );
  cmDspSysInstallCb(h, onb, "out",     c0.achan,"reset",  NULL );
  cmDspSysInstallCb(h, onb, "out",     c1.achan,"reset",  NULL );

  cmDspSysInstallCb(h, mod_sel,"out",  modp,  "sel", NULL );

  // stop connections
  cmDspSysInstallCb(h, tlp,  "mfn", pts, "off",   NULL ); // Prevents WT start on new audio file from TL.
  cmDspSysInstallCb(h, offb, "sym", mfp, "sel",   NULL ); 
  cmDspSysInstallCb(h, offb, "sym", pts, "off",   NULL );  
  cmDspSysInstallCb(h, pts,  "off", wt0, "cmd",   NULL );
  cmDspSysInstallCb(h, pts,  "off", wt1, "cmd",   NULL );
  cmDspSysInstallCb(h, pts,  "off", modp,"cmd",   NULL );


  // time-line to MIDI file player selection
  cmDspSysInstallCb(h, tlp, "mbsi", mfp, "bsi",   NULL );
  cmDspSysInstallCb(h, tlp, "mesi", mfp, "esi",   NULL );
  cmDspSysInstallCb(h, tlp, "mfn",  mfp, "fn",    NULL );


  // time-line to Audio file player selection
  cmDspSysInstallCb(h, tlp, "absi", wt0, "beg",   NULL );
  cmDspSysInstallCb(h, tlp, "aesi", wt0, "end",   NULL );
  cmDspSysInstallCb(h, tlp, "afn",  wt0, "fn",    NULL );

  cmDspSysInstallCb(h, tlp, "absi", wt1, "beg",   NULL );
  cmDspSysInstallCb(h, tlp, "aesi", wt1, "end",   NULL );
  cmDspSysInstallCb(h, tlp, "afn",  wt1, "fn",    NULL );
  
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

  cmDspSysInstallCb(h, msrc,  "d0",      sfp,  "d0",   NULL );
  cmDspSysInstallCb(h, msrc,  "d0",      nmp,  "d0",   NULL );

  cmDspSysInstallCb(h, msrc,  "status",  sfp,  "status",NULL );
  cmDspSysInstallCb(h, msrc,  "status",  nmp,  "status",NULL );


  // score follower to recd_play,modulator and printers
  cmDspSysInstallCb(h, sfp, "out",     modp,    "index", NULL );
  cmDspSysInstallCb(h, sfp, "recent",  sfp_loc,   "in",  NULL );  // report 'recent' but only act on 'max' loc index

  cmDspSysInstallCb( h, eloc , "val", its,  "off-int", NULL);
  cmDspSysInstallCb( h, sfp,   "out", its,  "in",  NULL);
  cmDspSysInstallCb( h, its,   "out", offb, "in",  NULL);
  cmDspSysInstallCb( h, its,   "out", prp, "in",  NULL);
  

  cmDspSysInstallCb(h, modp, "dgain0",  ogain0, "val",  NULL ); // mod -> ogain
  cmDspSysInstallCb(h, modp, "dgain1",  ogain1, "val",  NULL );
  cmDspSysInstallCb(h, modp, "wgain0",  ogain2, "val",  NULL );
  cmDspSysInstallCb(h, modp, "wgain1",  ogain3, "val",  NULL );


  cmDspSysInstallCb(h, ogain0,  "val", gmult0, "in-0", NULL ); // ogain scalars -> gmult 0
  cmDspSysInstallCb(h, ogain1,  "val", gmult1, "in-0", NULL );
  cmDspSysInstallCb(h, ogain2,  "val", gmult2, "in-0", NULL );
  cmDspSysInstallCb(h, ogain3,  "val", gmult3, "in-0", NULL );
  
  cmDspSysInstallCb(h, ogainD,  "val", gmult0, "in-1", NULL ); // master scalars -> gmult 1
  cmDspSysInstallCb(h, ogainD,  "val", gmult1, "in-1", NULL );
  cmDspSysInstallCb(h, ogainW,  "val", gmult2, "in-1", NULL );
  cmDspSysInstallCb(h, ogainW,  "val", gmult3, "in-1", NULL );
  
  cmDspSysInstallCb(h, gmult0, "out", lmix, "gain-0", NULL );   // gmult -> wdmix - l dry 
  cmDspSysInstallCb(h, gmult1, "out", rmix, "gain-0", NULL );   //                  r dry
  cmDspSysInstallCb(h, gmult2, "out", lmix, "gain-1", NULL );   //                  l wet
  cmDspSysInstallCb(h, gmult3, "out", rmix, "gain-1", NULL );   //                  r wet

  //cmDspSysInstallCb(h, gmult2, "out", prp, "in", NULL );

  return rc;
}


//------------------------------------------------------------------------------
//)
