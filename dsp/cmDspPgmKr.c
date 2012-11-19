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
  cmDspRC_t       rc      = kOkDspRC;
  const cmChar_t* tlFn    = "/home/kevin/src/cmgv/src/gv/data/tl7.js";
  const cmChar_t* audPath = "/home/kevin/media/audio/20110723-Kriesberg/Audio Files";
  const cmChar_t* scFn    = "/home/kevin/src/cmgv/src/gv/data/mod2b.txt";

  cmDspInst_t* sci =  cmDspSysAllocInst(h,"Scalar", "ScIdx",  5, kNumberDuiId, 0.0,  10000.0, 1.0,  0.0);

  cmDspInst_t* tlp  = cmDspSysAllocInst(h,"TimeLine",    "tl",  2, tlFn, audPath );
  cmDspInst_t* php  = cmDspSysAllocInst(h,"Phasor",      NULL,  0 );
  cmDspInst_t* wtp  = cmDspSysAllocInst(h,"WaveTable",   NULL,  2, cmDspSysSampleRate(h), 0 );
  cmDspInst_t* pts  = cmDspSysAllocInst(h,"PortToSym",   NULL,  1, "start" );
  cmDspInst_t* mfp  = cmDspSysAllocInst(h,"MidiFilePlay",NULL,  0 );
  cmDspInst_t* sfp  = cmDspSysAllocInst(h,"ScFol",       NULL,  1, scFn );
  cmDspInst_t* ao0p = cmDspSysAllocInst(h,"AudioOut",    NULL,  1, 0 );
  cmDspInst_t* ao1p = cmDspSysAllocInst(h,"AudioOut",    NULL,  1, 1 );



  cmDspInst_t* prp = cmDspSysAllocInst(h,"Printer", NULL,   1, ">" );
  
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    return rc;

  cmDspSysConnectAudio(h, php, "out", wtp,  "phs" );   // phs -> wt
  cmDspSysConnectAudio(h, wtp, "out", ao0p, "in"  );   // wt  -> aout0
  cmDspSysConnectAudio(h, wtp, "out", ao1p, "in" );    // wt  -> aout1

  
  cmDspSysInstallCb(h, tlp, "afn",  prp, "in",  NULL );
  cmDspSysInstallCb(h, tlp, "mfn",  prp, "in", NULL );
  cmDspSysInstallCb(h, tlp, "sel",  prp, "in", NULL );

  cmDspSysInstallCb(h, tlp, "absi", wtp, "beg", NULL );
  cmDspSysInstallCb(h, tlp, "aesi", wtp, "end", NULL );
  cmDspSysInstallCb(h, tlp, "afn",  wtp, "fn",  NULL );

  cmDspSysInstallCb(h, tlp, "mbsi", mfp, "bsi", NULL );
  cmDspSysInstallCb(h, tlp, "mesi", mfp, "esi", NULL );
  cmDspSysInstallCb(h, tlp, "mfn",  mfp, "fn", NULL );
  cmDspSysInstallCb(h, tlp, "mfn",  pts, "start", NULL );
  cmDspSysInstallCb(h, pts, "out",  mfp, "sel", NULL );

  cmDspSysInstallCb(h, mfp, "status", sfp, "status", NULL );
  cmDspSysInstallCb(h, mfp, "d0",     sfp, "d0",     NULL );
  cmDspSysInstallCb(h, mfp, "d1",     sfp, "d1",     NULL );
  cmDspSysInstallCb(h, sci, "val",    sfp, "index",  NULL );
  cmDspSysInstallCb(h, sfp, "out",    prp, "in",     NULL );

  return rc;
}
