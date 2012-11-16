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

  cmDspInst_t* tlp = cmDspSysAllocInst(h,"TimeLine",   "tl",  2, tlFn, audPath );
  cmDspInst_t* prp = cmDspSysAllocInst(h,"Printer", NULL,   1, ">" );
  
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    return rc;

  
  cmDspSysInstallCb(h, tlp, "afn", prp, "in", NULL );
  cmDspSysInstallCb(h, tlp, "sel", prp, "in", NULL );

  return rc;
}
