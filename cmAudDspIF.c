#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmFileSys.h"
#include "cmJson.h"
#include "cmThread.h"
#include "dsp/cmDspValue.h"
#include "cmMsgProtocol.h"
#include "cmAudDspIF.h"


cmAiH_t cmAiNullHandle = cmSTATIC_NULL_HANDLE;

typedef struct
{
  cmErr_t      err;
  cmAdIfParm_t parms;
  cmJsonH_t    jsH;
} cmAi_t;

cmAi_t* _cmAiHandleToPtr( cmAiH_t h )
{
  cmAi_t* p = (cmAi_t*)h.h;
  assert(p != NULL);
  return p;
}


// Dispatch a message to the client application.
// This function is called from within cmTsQueueDequeueMsg() which is called
// by cmAdIfDispatchMsgToHost().  
cmRC_t _cmAiDispatchMsgToClient(void* cbDataPtr, unsigned msgByteCnt, const void* msgDataPtr )
{
  cmAi_t*       p    = (cmAi_t*)cbDataPtr;
  cmDspUiHdr_t* m    = (cmDspUiHdr_t*)msgDataPtr;
  cmRC_t        rc   = cmOkRC;

  switch( m->uiId )
  {
    case kStatusSelAsId:
      {
        // handle a status mesage
        const char*               base        = (const char*)msgDataPtr;
        const cmAudioSysStatus_t* st          = (const cmAudioSysStatus_t*)(base + (2 * sizeof(unsigned)));
        const double*             iMeterArray = (const double*)(st + 1);
        const double*             oMeterArray = iMeterArray + st->iMeterCnt;
        rc = p->parms.dispatchRecd.statusFunc(p->parms.dispatchRecd.cbDataPtr, st, iMeterArray, oMeterArray );
      }
      break;

    case kSsInitSelAsId:
      {
        // handle an ssInit message
        const cmAudioSysSsInitMsg_t* sip       = (const cmAudioSysSsInitMsg_t*)msgDataPtr;
        const char*                  iDevLabel = (const char*)(sip+1);
        const char*                  oDevLabel = iDevLabel + strlen(iDevLabel) + 1;
        rc = p->parms.dispatchRecd.ssInitFunc(p->parms.dispatchRecd.cbDataPtr, sip, iDevLabel, oDevLabel );
      }
      break;

    case kUiSelAsId:
      {
        bool          jsFl = false;
        cmDsvRC_t     rc   = kOkDsvRC;

        // if the value associated with this msg is a mtx then set
        // its mtx data area pointer to just after the msg header.
        if( cmDsvIsJson(&m->value) )
        {
          rc = cmDsvDeserializeJson(&m->value,p->jsH);
          jsFl = true;
        }
        else
          rc = cmDsvDeserializeInPlace(&m->value,msgByteCnt-sizeof(cmDspUiHdr_t));

        if( rc != kOkDsvRC )
          cmErrMsg(&p->err,kDeserialFailAiRC,"Deserialize failed.");
        else
          rc = p->parms.dispatchRecd.uiFunc(p->parms.dispatchRecd.cbDataPtr,m);

        if( jsFl )
          cmJsonClearTree(p->jsH);
      }
      break;

    default:
      cmErrMsg(&p->err,kUnknownMsgTypeAiRC,"The message type %i is unknown.",m->uiId);
      break;
  }

  return rc;  
}

cmAiRC_t _cmAdIfReadCfgFile( cmAi_t* p, cmCtx_t* ctx )
{
  cmAiRC_t        rc            = kOkAiRC;
  const cmChar_t* sysJsFn       = NULL;
  cmJsonH_t       jsH           = cmJsonNullHandle;
  cmJsonNode_t*   audDspNodePtr = NULL;
  //const cmChar_t* errLabelPtr   = NULL;

  // form the audio dsp resource file name
  if((sysJsFn = cmFsMakeFn( cmFsPrefsDir(),cmAudDspSys_FILENAME,NULL,NULL)) == NULL )
  {
    rc = cmErrMsg(&p->err,kFileSysFailAiRC,"Unable to form the audio dsp system resource file name.");
    goto errLabel;
  }

  // open the audio dsp resource file
  if(cmJsonInitializeFromFile(&jsH,sysJsFn,ctx) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailAiRC,"Unable to open the audio dsp resource file: '%s'.",cmStringNullGuard(sysJsFn));
    goto errLabel;
  }

  // locate the aud_dsp container object
  if( cmJsonNodeMember( cmJsonRoot(jsH), "aud_dsp", &audDspNodePtr ) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailAiRC,"The audio DSP system resource file '%s' does not contain an 'aud_dsp' object.",cmStringNullGuard(sysJsFn));
    goto errLabel;
  }

  /*
  // locate the read the aud_dsp sub-elements
  if( cmJsonMemberValues( audDspNodePtr, &errLabelPtr, 
      "msgQueueByteCnt", kIntTId,   &p->msgQueueByteCnt,
      NULL ) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailAiRC,"Syntax error while parsing the top level fields in the audio DSP system resource file:%s.",cmStringNullGuard(sysJsFn));
    goto errLabel;
  }
  */

 errLabel:
  if( cmJsonFinalize(&jsH) != kOkJsRC )
    rc = cmErrMsg(&p->err,kJsonFailAiRC,"JSON finalization failed.");

  if( sysJsFn != NULL )
    cmFsFreeFn(sysJsFn);

  return rc;

}


cmAiRC_t _cmAdIfSendIntMsg(cmAiH_t h, unsigned selId, unsigned asSubIdx, unsigned flags, unsigned iv, double dv )
{  
  cmAi_t*      p = _cmAiHandleToPtr( h );
  cmDspValue_t v;

  if(iv == cmInvalidIdx )
    cmDsvSetDouble(&v,dv);
  else
    cmDsvSetUInt(&v,iv);
 
  if( cmMsgSend(&p->err,asSubIdx,kUiSelAsId,selId,flags,cmInvalidId,cmInvalidId,&v,p->parms.audDspFunc,p->parms.audDspFuncDataPtr) != kOkMsgRC )
    return cmErrMsg(&p->err,kSendFailAiRC,"The integer message sel id:%i value:%i transmission failed.",selId,iv);
    
  return kOkAiRC;

}

cmAiRC_t _cmAdIfFree( cmAi_t* p )
{
  cmAiRC_t rc = kOkAiRC;

  if( cmJsonFinalize(&p->jsH) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailAiRC,"JSON finalization failed.");
    goto errLabel;
  }

  cmMemFree(p);

 errLabel:
  return rc;
}


cmAiRC_t cmAdIfAllocate( cmCtx_t* ctx, cmAiH_t* hp, const cmAdIfParm_t* parms  )
{
  cmAiRC_t rc;
  if((rc = cmAdIfFree(hp)) != kOkAiRC )
    return rc;
  
  cmAi_t* p = cmMemAllocZ(cmAi_t,1);

  cmErrSetup(&p->err,&ctx->rpt,"Audio DSP Interface");

  p->parms = *parms;

  // read the system configuration file
  if((rc = _cmAdIfReadCfgFile(p, ctx )) != kOkAiRC )
    goto errLabel;


  // initialize a JSON tree for use in deserializing JSON messages
  if((rc = cmJsonInitialize( &p->jsH, ctx )) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailAiRC,"JSON initialization failed.");
    goto errLabel;
  }


  hp->h = p;

 errLabel:
  if( rc != kOkAiRC )
    _cmAdIfFree(p);

  return rc;
}

cmAiRC_t cmAdIfFree( cmAiH_t* hp )
{
  cmAiRC_t rc = kOkAiRC;

  if( hp==NULL || cmAdIfIsValid(*hp)==false )
    return kOkAiRC;

  cmAi_t* p = _cmAiHandleToPtr(*hp);

  if((rc = _cmAdIfFree(p)) != kOkAiRC )
    return rc;

  hp->h = NULL;
  return rc;
}

bool            cmAdIfIsValid( cmAiH_t h )
{ return h.h != NULL; }

cmAiRC_t       cmAdIfRecvAudDspMsg( cmAiH_t h, unsigned msgByteCnt, const void* msg )
{
  cmAi_t* p = _cmAiHandleToPtr(h);
  cmAiRC_t rc = kOkAiRC;

  _cmAiDispatchMsgToClient(p,msgByteCnt,msg);
  return rc;
}

cmAiRC_t        cmAdIfDeviceReport( cmAiH_t h )
{ return _cmAdIfSendIntMsg(h,kDevReportDuiId,cmInvalidIdx,0,cmInvalidIdx,0.0); }
 
cmAiRC_t        cmAdIfSetAudioSysCfg(   cmAiH_t h, unsigned asCfgIdx )
{ return _cmAdIfSendIntMsg(h,kSetAudioCfgDuiId,cmInvalidIdx,0,asCfgIdx,0.0); }

cmAiRC_t        cmAdIfSetAudioDevice(   cmAiH_t h, unsigned asSubIdx, bool inputFl, unsigned devIdx )
{ return _cmAdIfSendIntMsg(h,kSetAudioDevDuiId,asSubIdx,inputFl,devIdx,0.0); }

cmAiRC_t        cmAdIfSetSampleRate(  cmAiH_t h, unsigned asSubIdx, double srate )
{ return _cmAdIfSendIntMsg(h,kSetSampleRateDuiId,asSubIdx,0,cmInvalidIdx,srate); }

cmAiRC_t        cmAdIfLoadProgram(   cmAiH_t h, unsigned asSubIdx, unsigned pgmIdx )
{ return _cmAdIfSendIntMsg(h,kSetPgmDuiId,asSubIdx,0,pgmIdx,0.0); }
  
cmAiRC_t        cmAdIfEnableAudio( cmAiH_t h, bool enableFl )
{ return _cmAdIfSendIntMsg(h,kEnableDuiId,cmInvalidIdx,enableFl,cmInvalidIdx,0.0); }

cmAiRC_t        cmAdIfEnableStatusNotify( cmAiH_t h, bool enableFl )
{ return _cmAdIfSendIntMsg(h,kSetNotifyEnableDuiId,cmInvalidIdx,enableFl,cmInvalidIdx,0.0); }

cmAiRC_t        cmAdIfSendMsgToAudioDSP( 
  cmAiH_t             h, 
  unsigned            asSubIdx,
  unsigned            msgTypeId,
  unsigned            selId,
  unsigned            flags,
  unsigned            instId,
  unsigned            instVarId,
  const cmDspValue_t* valPtr )
{
  cmAiRC_t rc = kOkAiRC;
  cmAi_t*  p  = _cmAiHandleToPtr(h);
  
  if( cmMsgSend( &p->err, asSubIdx, msgTypeId,selId,flags,instId,instVarId,valPtr, p->parms.audDspFunc, p->parms.audDspFuncDataPtr ) != kOkMsgRC )
    rc = cmErrMsg(&p->err, kSendFailAiRC, "A UI message intened for the the audio DSP system was not successfully delivered.");

  return rc;

}


cmAiRC_t        cmAdIfDispatchMsgToHost(  cmAiH_t h ) 
{ return _cmAdIfSendIntMsg(h,kClientMsgPollDuiId,cmInvalidIdx,0,cmInvalidIdx,0.0); }

