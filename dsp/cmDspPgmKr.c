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


cmDspRC_t _cmDspSysPgm_TimeLine(cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t       rc         = kOkDspRC;
  const cmChar_t* tlFn       = "/home/kevin/src/cmgv/src/gv/data/tl7.js";
  const cmChar_t* audPath    = "/home/kevin/media/audio/20110723-Kriesberg/Audio Files";
  const cmChar_t* scFn       = "/home/kevin/src/cmgv/src/gv/data/mod2b.txt";
  unsigned        wtLoopCnt  = 1;                            // play once (do not loop)
  unsigned        wtInitMode = 0;                            // initial wt mode is 'silence'
  unsigned        wtSmpCnt   = floor(cmDspSysSampleRate(h)); // wt length == srate

  cmDspInst_t* tlp  = cmDspSysAllocInst(h,"TimeLine",    "tl",  2, tlFn, audPath );
  cmDspInst_t* scp  = cmDspSysAllocInst(h,"Score",       "sc",  1, scFn );
  cmDspInst_t* php  = cmDspSysAllocInst(h,"Phasor",      NULL,  0 );
  cmDspInst_t* wtp  = cmDspSysAllocInst(h,"WaveTable",   NULL,  4, wtSmpCnt, wtInitMode, NULL, wtLoopCnt );
  cmDspInst_t* pts  = cmDspSysAllocInst(h,"PortToSym",   NULL,  2, "on", "off" );
  cmDspInst_t* mfp  = cmDspSysAllocInst(h,"MidiFilePlay",NULL,  0 );
  cmDspInst_t* sfp  = cmDspSysAllocInst(h,"ScFol",       NULL,  1, scFn );
  cmDspInst_t* ao0p = cmDspSysAllocInst(h,"AudioOut",    NULL,  1, 0 );
  cmDspInst_t* ao1p = cmDspSysAllocInst(h,"AudioOut",    NULL,  1, 1 );

  cmDspSysNewPage(h,"Controls");
  cmDspInst_t* onb  = cmDspSysAllocInst(h,"Button", "start",  2, kButtonDuiId, 1.0 );
  cmDspInst_t* offb = cmDspSysAllocInst(h,"Button", "stop",   2, kButtonDuiId, 1.0 );



  cmDspInst_t* prp = cmDspSysAllocInst(h,"Printer", NULL,   1, ">" );
  
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    return rc;

  // phasor->wt->aout
  cmDspSysConnectAudio(h, php, "out", wtp,  "phs" );   // phs -> wt
  cmDspSysConnectAudio(h, wtp, "out", ao0p, "in"  );   // wt  -> aout0
  cmDspSysConnectAudio(h, wtp, "out", ao1p, "in" );    // wt  -> aout1
  cmDspSysInstallCb(   h, wtp, "fidx",tlp,  "curs", NULL); 
  //cmDspSysInstallCb(   h, wtp, "fidx",prp,  "in", NULL );

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

  // score to score follower
  cmDspSysInstallCb(h, scp, "sel",    sfp, "index",  NULL );


  // MIDI file player to score-follower
  cmDspSysInstallCb(h, mfp, "status", sfp, "status", NULL );
  cmDspSysInstallCb(h, mfp, "d0",     sfp, "d0",     NULL );
  cmDspSysInstallCb(h, mfp, "d1",     sfp, "d1",     NULL );


  // Printer connections
  cmDspSysInstallCb(h, tlp, "afn",  prp, "in",  NULL );
  cmDspSysInstallCb(h, tlp, "mfn",  prp, "in",  NULL );
  cmDspSysInstallCb(h, tlp, "sel",  prp, "in",  NULL );
  //cmDspSysInstallCb(h, sfp, "out",  prp, "in",     NULL );


  return rc;
}
