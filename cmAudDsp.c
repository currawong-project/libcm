#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmSymTbl.h"
#include "cmJson.h"
#include "cmFileSys.h"
#include "cmPrefs.h"
#include "cmAudioPort.h"
#include "cmAudioAggDev.h"
#include "cmAudioNrtDev.h"
#include "cmAudioPortFile.h"
#include "cmApBuf.h"
#include "cmMidi.h"
#include "cmMidiPort.h"
#include "dsp/cmDspValue.h"
#include "cmMsgProtocol.h"
#include "cmThread.h"
#include "cmUdpPort.h"
#include "cmUdpNet.h"
#include "cmAudioSys.h"
#include "cmProcObj.h"
#include "dsp/cmDspCtx.h"
#include "dsp/cmDspClass.h"
#include "dsp/cmDspSys.h"
#include "cmAudDsp.h"


cmAdH_t cmAdNullHandle = cmSTATIC_NULL_HANDLE;

typedef struct
{
  cmDspSysH_t dsH;
  unsigned    curPgmIdx;
  bool        isLoadedFl;
  unsigned    isSyncFl;  
} cmAdDsSubSys_t;

typedef struct
{
  const cmChar_t* label;
  cmAudioSysCfg_t cfg;
} cmAdAsCfg_t;

typedef struct
{
  const cmChar_t* label;
  unsigned        physDevCnt;
  unsigned*       physDevIdxArray;
} cmAdAggDev_t;

typedef struct
{
  const cmChar_t* label;
  double          srate;
  unsigned        iChCnt;
  unsigned        oChCnt;
  unsigned        cbPeriodMs;
} cmAdNrtDev_t;

typedef struct
{
  const cmChar_t* label;
  const cmChar_t* inAudioFn;
  const cmChar_t* outAudioFn;
  unsigned        oBits;
  unsigned        oChCnt;
} cmAdAfpDev_t;

typedef struct
{
  cmErr_t            err;
  cmCtx_t            ctx;
  cmMsgSendFuncPtr_t cbFunc;
  void*              cbDataPtr;
  cmJsonH_t          sysJsH;
  const cmChar_t*    sysJsFn;
  cmUdpNetH_t        netH; 
  cmDspSysH_t        dsH;
  cmAudioSysH_t      asH;

  unsigned           midiPortBufByteCnt;
  unsigned           meterMs;
  unsigned           msgsPerClientPoll;

  cmAdAggDev_t*      aggDevArray;
  unsigned           aggDevCnt;

  cmAdNrtDev_t*      nrtDevArray;
  unsigned           nrtDevCnt;

  cmAdAfpDev_t*      afpDevArray;
  unsigned           afpDevCnt;

  cmAdAsCfg_t*       asCfgArray;
  unsigned           asCfgCnt;
  
  unsigned           curAsCfgIdx;

  unsigned           dsSsCnt;
  cmAdDsSubSys_t*    dsSsArray;

  bool               syncFl; // all cmAdDsSubSys's are synced
} cmAd_t;

cmAd_t* _cmAdHandleToPtr( cmAdH_t h )
{ 
  cmAd_t* p = (cmAd_t*)h.h;
  assert( p != NULL );
  return p;
}

// cmAudioSys DSP processing callback - this is the entry point
// from the cmAudioSystem to the cmDspSystem.  Note that it is called from the
// the audio processing thread in cmAudioSys.c: _cmAsDeliverMsgsWithLock()
cmRC_t  _cmAudDspCallback( void *cbPtr, unsigned msgByteCnt, const void* msgDataPtr )
{
  cmAudioSysCtx_t* ctx = (cmAudioSysCtx_t*)cbPtr;
  cmAdDsSubSys_t*  p   = (cmAdDsSubSys_t*)ctx->ss->cbDataPtr;

  if( p != NULL && p->isLoadedFl )
    return cmDspSysRcvMsg(p->dsH,ctx, msgDataPtr, msgByteCnt, ctx->srcNetNodeId ); 

  return cmOkRC;
}

// This function is called by cmAudioSysReceiveMsg() to transfer messages from the
// cmAudioSys or cmDspSys to the client.
cmRC_t  _cmAudioSysToClientCallback(void* userCbPtr, unsigned msgByteCnt, const void* msgDataPtr )
{
  cmAd_t* p = (cmAd_t*)userCbPtr;
  return p->cbFunc(p->cbDataPtr, msgByteCnt, msgDataPtr );
}

// This function is called by cmUdpNetReceive(), which is called in 
// cmAudioSys.c:_cmAsThreadCallback() just prior to executing the DSP process. 
void _cmAdUdpNetCallback( void* cbArg, cmUdpNetH_t h, const char* data, unsigned dataByteCnt, unsigned srcNetNodeId )
{
  cmAd_t* p = (cmAd_t*)cbArg;

  // send the incoming message to the audio system for later delivery to the DSP system
  cmAudioSysDeliverMsg(p->asH, data, dataByteCnt, srcNetNodeId );
}


cmAdRC_t _cmAdParseMemberErr( cmAd_t* p, cmJsRC_t jsRC, const cmChar_t* errLabel, const cmChar_t* objectLabel )
{
  if( jsRC == kNodeNotFoundJsRC && errLabel != NULL )
    return cmErrMsg(&p->err,kJsonFailAdRC,"The required field '%s'was not found in the audio DSP resource tree in the object '%s' in the file '%s'.",errLabel,cmStringNullGuard(objectLabel), cmStringNullGuard(p->sysJsFn));

  return cmErrMsg(&p->err,kJsonFailAdRC,"JSON parsing failed on the Audio DSP resource file '%s' in the resource object '%s'.",cmStringNullGuard(p->sysJsFn),cmStringNullGuard(objectLabel));

}


cmAdRC_t _cmAdParseSysJsonTree( cmAd_t* p )
{
  cmAdRC_t        rc               = kOkAdRC;
  cmJsonNode_t*   asCfgArrNodePtr  = NULL;
  cmJsonNode_t*   aggDevArrNodePtr = NULL;
  cmJsonNode_t*   nrtDevArrNodePtr = NULL;
  cmJsonNode_t*   afpDevArrNodePtr = NULL;
  cmJsonNode_t*   audDspNodePtr    = NULL;
  const cmChar_t* errLabelPtr      = NULL;
  unsigned        i;
  cmJsRC_t      jsRC = kOkJsRC;

  // locate the aud_dsp container object
  if( cmJsonNodeMember( cmJsonRoot(p->sysJsH), "aud_dsp", &audDspNodePtr ) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailAdRC,"The audio DSP system resource file '%s' does not contain an 'aud_dsp' object.",cmStringNullGuard(p->sysJsFn));
    goto errLabel;
  }

  // locate the read the aud_dsp sub-elements
  if(( jsRC = cmJsonMemberValues( audDspNodePtr, &errLabelPtr, 
        "midiPortBufByteCnt", kIntTId,   &p->midiPortBufByteCnt,
        "meterMs",            kIntTId,   &p->meterMs,
        "msgsPerClientPoll",  kIntTId,   &p->msgsPerClientPoll,
        "audioSysCfgArray",   kArrayTId, &asCfgArrNodePtr,
        "aggDevArray",        kArrayTId | kOptArgJsFl, &aggDevArrNodePtr,
        "nrtDevArray",        kArrayTId | kOptArgJsFl, &nrtDevArrNodePtr,
        "afpDevArray",        kArrayTId | kOptArgJsFl, &afpDevArrNodePtr,
        NULL )) != kOkJsRC )
  {
    rc = _cmAdParseMemberErr(p, jsRC, errLabelPtr, "aud_dsp" );
    goto errLabel;
  }

  // parse the aggregate device specifications into p->aggDevArray[].
  if( aggDevArrNodePtr != NULL && (p->aggDevCnt = cmJsonChildCount(aggDevArrNodePtr)) > 0)
  {
    // alloc the aggregate spec. array
    p->aggDevArray = cmMemResizeZ( cmAdAggDev_t, p->aggDevArray, p->aggDevCnt );

    // for each agg. device spec. recd
    for(i=0; i<p->aggDevCnt; ++i)
    {
      const cmJsonNode_t* np               = cmJsonArrayElementC(aggDevArrNodePtr,i);
      const cmJsonNode_t* devIdxArrNodePtr = NULL;
      unsigned            j;

      // read aggDevArray record values
      if(( jsRC      = cmJsonMemberValues( np, &errLabelPtr, 
            "label",           kStringTId, &p->aggDevArray[i].label,
            "physDevIdxArray", kArrayTId, &devIdxArrNodePtr,
            NULL )) != kOkJsRC )
      {
        rc = _cmAdParseMemberErr(p, jsRC, errLabelPtr, cmStringNullGuard(p->aggDevArray[i].label) );
        goto errLabel;
      }

      unsigned physDevCnt = cmJsonChildCount(devIdxArrNodePtr);

      // alloc the dev idx array for to hold the phys. dev indexes for this agg device
      p->aggDevArray[i].physDevIdxArray = cmMemResizeZ( unsigned, p->aggDevArray[i].physDevIdxArray, physDevCnt);
      p->aggDevArray[i].physDevCnt      = physDevCnt;

      // store the phys. dev. idx's 
      for(j=0; j<physDevCnt; ++j)        
        if( cmJsonUIntValue( cmJsonArrayElementC(devIdxArrNodePtr,j), p->aggDevArray[i].physDevIdxArray + j ) != kOkJsRC )
        {
          rc = cmErrMsg(&p->err,kJsonFailAdRC,"Unable to retrieve a physical device index for the aggregate device '%s'.", cmStringNullGuard(p->aggDevArray[i].label)); 
          goto errLabel;
        }
    }

  }

  // parse the non-real-time device specifications into p->nrtDevArray[].
  if( nrtDevArrNodePtr != NULL && (p->nrtDevCnt = cmJsonChildCount(nrtDevArrNodePtr)) > 0)
  {
    // alloc the non-real-time spec. array
    p->nrtDevArray = cmMemResizeZ( cmAdNrtDev_t, p->nrtDevArray, p->nrtDevCnt );

    // for each nrt. device spec. recd
    for(i=0; i<p->nrtDevCnt; ++i)
    {
      const cmJsonNode_t* np   = cmJsonArrayElementC(nrtDevArrNodePtr,i);

      // read nrtDevArray record values
      if(( jsRC      = cmJsonMemberValues( np, &errLabelPtr, 
            "label",      kStringTId, &p->nrtDevArray[i].label,
            "srate",      kRealTId,   &p->nrtDevArray[i].srate,
            "iChCnt",     kIntTId,    &p->nrtDevArray[i].iChCnt,
            "oChCnt",     kIntTId,    &p->nrtDevArray[i].oChCnt,
            "cbPeriodMs", kIntTId,    &p->nrtDevArray[i].cbPeriodMs,
            NULL )) != kOkJsRC )
      {
        rc = _cmAdParseMemberErr(p, jsRC, errLabelPtr, cmStringNullGuard(p->nrtDevArray[i].label) );
        goto errLabel;
      }

    }
    
  }

  // parse the audio file device specifications into p->afpDevArray[].
  if( afpDevArrNodePtr != NULL && (p->afpDevCnt = cmJsonChildCount(afpDevArrNodePtr)) > 0)
  {
    // alloc the non-real-time spec. array
    p->afpDevArray = cmMemResizeZ( cmAdAfpDev_t, p->afpDevArray, p->afpDevCnt );

    // for each afp. device spec. recd
    for(i=0; i<p->afpDevCnt; ++i)
    {
      const cmJsonNode_t* np   = cmJsonArrayElementC(afpDevArrNodePtr,i);

      // read afpDevArray record values
      if(( jsRC      = cmJsonMemberValues( np, &errLabelPtr, 
            "label",      kStringTId,                &p->afpDevArray[i].label,
            "iAudioFn",   kStringTId  | kOptArgJsFl, &p->afpDevArray[i].inAudioFn,
            "oAudioFn",   kStringTId  | kOptArgJsFl, &p->afpDevArray[i].outAudioFn,
            "oBits",      kIntTId     | kOptArgJsFl, &p->afpDevArray[i].oBits,
            "oChCnt",     kIntTId     | kOptArgJsFl, &p->afpDevArray[i].oChCnt,
            NULL )) != kOkJsRC )
      {
        rc = _cmAdParseMemberErr(p, jsRC, errLabelPtr, cmStringNullGuard(p->afpDevArray[i].label) );
        goto errLabel;
      }

    }

  }

  if((p->asCfgCnt = cmJsonChildCount(asCfgArrNodePtr)) == 0 )
    goto errLabel;
  
  p->asCfgArray = cmMemResizeZ( cmAdAsCfg_t, p->asCfgArray, p->asCfgCnt);

  // for each cmAsAudioSysCfg record in audioSysCfgArray[]
  for(i=0; i<p->asCfgCnt; ++i)
  {
    unsigned            j;
    const cmJsonNode_t* asCfgNodePtr   = cmJsonArrayElementC(asCfgArrNodePtr,i);
    const cmJsonNode_t* ssArrayNodePtr = NULL;
    const char*         cfgLabel       = NULL;

    // read cmAsAudioSysCfg record values
    if(( jsRC = cmJsonMemberValues( asCfgNodePtr, &errLabelPtr, 
        "label",   kStringTId, &cfgLabel,
        "ssArray", kArrayTId, &ssArrayNodePtr,
          NULL )) != kOkJsRC )
    {
      rc = _cmAdParseMemberErr(p, jsRC, errLabelPtr, cmStringNullGuard(p->asCfgArray[i].label) );
      goto errLabel;
    }

    p->asCfgArray[i].label            = cfgLabel;
    p->asCfgArray[i].cfg.ssCnt        = cmJsonChildCount( ssArrayNodePtr );
    p->asCfgArray[i].cfg.ssArray      = cmMemResizeZ( cmAudioSysSubSys_t, p->asCfgArray[i].cfg.ssArray, p->asCfgArray[i].cfg.ssCnt );
    p->asCfgArray[i].cfg.clientCbFunc = _cmAudioSysToClientCallback;
    p->asCfgArray[i].cfg.clientCbData = p;
      

    // for each audio system sub-subsystem 
    for(j=0; j<p->asCfgArray[i].cfg.ssCnt; ++j)
    {
      cmAudioSysArgs_t*   asap        = &p->asCfgArray[i].cfg.ssArray[j].args;
      const cmJsonNode_t* argsNodePtr = cmJsonArrayElementC(ssArrayNodePtr,j);

      if((jsRC = cmJsonMemberValues( argsNodePtr, &errLabelPtr,
          "inDevIdx",           kIntTId,  &asap->inDevIdx,
          "outDevIdx",          kIntTId,  &asap->outDevIdx,
          "syncToInputFl",      kTrueTId, &asap->syncInputFl,
          "msgQueueByteCnt",    kIntTId,  &asap->msgQueueByteCnt,
          "devFramesPerCycle",  kIntTId,  &asap->devFramesPerCycle,
          "dspFramesPerCycle",  kIntTId,  &asap->dspFramesPerCycle,
          "audioBufCnt",        kIntTId,  &asap->audioBufCnt,
          "srate",              kRealTId, &asap->srate,
            NULL )) != kOkJsRC )
      {
        rc = _cmAdParseMemberErr(p, jsRC, errLabelPtr, cmStringNullGuard(p->asCfgArray[i].label));
        goto errLabel;
      }

    }

    
  }
 errLabel:
  
  return rc;
}

cmAdRC_t _cmAdSetup( cmAd_t* p )
{
  unsigned i;
  cmAdRC_t rc = kOkAdRC;

  for(i=0; i<p->asCfgCnt; ++i)
  {
    unsigned j;

    p->asCfgArray[i].cfg.meterMs = p->meterMs;

    // BUG BUG BUG - each sub-system should have it's own network 
    // manager, and socket port.

    p->asCfgArray[i].cfg.netH    = p->netH;

    for(j=0; j<p->asCfgArray[i].cfg.ssCnt; ++j)
    {
      p->asCfgArray[i].cfg.ssArray[j].cbDataPtr = NULL;
      p->asCfgArray[i].cfg.ssArray[j].cbFunc    = _cmAudDspCallback;
      p->asCfgArray[i].cfg.ssArray[j].args.rpt  = p->err.rpt;
    }
  }

  return rc;
}

cmAdRC_t _cmAdCreateAggDevices( cmAd_t* p )
{
  cmAdRC_t rc = kOkAdRC;
  unsigned i;

  if( cmApAggAllocate(p->err.rpt) != kOkAgRC )
    return cmErrMsg(&p->err,kAggDevSysFailAdRC,"The aggregate device system allocation failed.");

  for(i=0; i<p->aggDevCnt; ++i)
  {
    cmAdAggDev_t* adp = p->aggDevArray + i;
    if( cmApAggCreateDevice(adp->label,adp->physDevCnt,adp->physDevIdxArray,kInAggFl | kOutAggFl) != kOkAgRC )
      rc = cmErrMsg(&p->err,kAggDevCreateFailAdRC,"The aggregate device '%s' creation failed.",cmStringNullGuard(adp->label));
  }
  
  return rc;
}

cmAdRC_t _cmAdCreateNrtDevices( cmAd_t* p )
{
  cmAdRC_t rc = kOkAdRC;
  unsigned i;

  if( cmApNrtAllocate(p->err.rpt) != kOkApRC )
    return cmErrMsg(&p->err,kNrtDevSysFailAdRC,"The non-real-time device system allocation failed.");

  for(i=0; i<p->nrtDevCnt; ++i)
  {
    cmAdNrtDev_t* adp = p->nrtDevArray + i;
    if( cmApNrtCreateDevice(adp->label,adp->srate,adp->iChCnt,adp->oChCnt,adp->cbPeriodMs) != kOkApRC )
      rc = cmErrMsg(&p->err,kNrtDevSysFailAdRC,"The non-real-time device '%s' creation failed.",cmStringNullGuard(adp->label));
  }
  
  return rc;
}

cmAdRC_t _cmAdCreateAfpDevices( cmAd_t* p )
{
  cmAdRC_t rc = kOkAdRC;

  if( cmApFileAllocate(p->err.rpt) != kOkApRC )
    return cmErrMsg(&p->err,kAfpDevSysFailAdRC,"The audio file device system allocation failed.");

  unsigned i;
  // create the audio file devices
  for(i=0; i<p->afpDevCnt; ++i)
  {
    //const cmAudioSysFilePort_t* afp = cfg->afpArray + i;
    cmAdAfpDev_t* afp = p->afpDevArray + i;
    if( cmApFileDeviceCreate( afp->label, afp->inAudioFn, afp->outAudioFn, afp->oBits, afp->oChCnt ) != kOkApRC )
      rc = cmErrMsg(&p->err,kAfpDevSysFailAdRC,"The audio file device '%s' creation failed.",cmStringNullGuard(afp->label));
  }

  return rc;
}

cmAdRC_t _cmAdSendAudioSysCfgLabels( cmAd_t* p)
{
  cmAdRC_t     rc = kOkAdRC;
  unsigned     i;

  for(i=0; i<p->asCfgCnt; ++i)
  {
    cmDspValue_t v;
    cmDsvSetStrcz(&v, p->asCfgArray[i].label);

    if( cmMsgSend( &p->err,cmInvalidIdx,kUiSelAsId,kAudioSysCfgDuiId,0,i,p->asCfgCnt,&v,p->cbFunc,p->cbDataPtr) != kOkMsgRC )
    {
      rc = cmErrMsg(&p->err,kSendMsgFailAdRC,"Error sending audio system cfg. label message to host.");
      break;
    }

  } 
  return rc;  
}

cmAdRC_t _cmAdSendDeviceLabels( cmAd_t* p )
{
  cmAdRC_t     rc = kOkAdRC;
  unsigned     i,j;

  unsigned n = cmApDeviceCount();

  for(i=0; i<2; ++i)
  {
    bool inputFl = i==0;

    for(j=0; j<n; ++j)
      if( cmApDeviceChannelCount(j,inputFl) )
      {
        cmDspValue_t v;
        cmDsvSetStrcz(&v, cmApDeviceLabel(j));

        if( cmMsgSend( &p->err,cmInvalidIdx,kUiSelAsId,kDeviceDuiId,inputFl,j,n,&v,p->cbFunc,p->cbDataPtr) != kOkMsgRC )
        {
          rc = cmErrMsg(&p->err,kSendMsgFailAdRC,"Error sending device label message to host.");
          break;
        }

      }

  } 
  return rc;
}

cmAdRC_t _cmAdSendProgramLabels( cmAd_t* p )
{
  cmAdRC_t  rc     = kOkAdRC;
  unsigned  pgmCnt = cmDspSysPgmCount(p->dsH);
  unsigned  i;

  for(i=0; i<pgmCnt; ++i)
  {
    cmDspValue_t v;
    cmDsvSetStrcz(&v, cmDspPgmLabel(p->dsH,i));

    if( cmMsgSend( &p->err,cmInvalidIdx,kUiSelAsId,kProgramDuiId,0,i,pgmCnt,&v,p->cbFunc,p->cbDataPtr) != kOkMsgRC )
    {
      rc = cmErrMsg(&p->err,kSendMsgFailAdRC,"Error sending program label message to host.");
      break;
    }

  }

  return rc;
}

cmAdRC_t _cmAudDspFree( cmAd_t* p )
{
  cmAdRC_t rc = kOkAdRC;

  if( cmAudioSysFree(&p->asH) != kOkAsRC )
  {
    rc = cmErrMsg(&p->err,kAudioSysFailAdRC,"The audio system release failed.");
    goto errLabel;
  }

  if( cmDspSysFinalize(&p->dsH) != kOkDspRC )
  {
    rc = cmErrMsg(&p->err,kDspSysFailAdRC,"DSP system finalization failed.");
    goto errLabel;
  }

  if( cmUdpNetFree(&p->netH) != kOkUnRC )
  {
    rc = cmErrMsg(&p->err,kNetSysFailAdRC,"UDP Network finalization failed.");
    goto errLabel;
  }

  if( cmMpIsInitialized() )
    if( cmMpFinalize() != kOkMpRC )
    {
      rc = cmErrMsg(&p->err,kMidiSysFailAdRC,"MIDI system finalization failed.");
      goto errLabel;
    }

  if( cmApFinalize() != kOkApRC )
  {
    rc = cmErrMsg(&p->err,kAudioPortFailAdRC,"Audio port finalization failed.");
    goto errLabel;
  }

  if( cmApBufFinalize() != kOkApRC )
  {
    rc = cmErrMsg(&p->err,kAudioPortFailAdRC,"Audio port buffer finalization failed.");
    goto errLabel;
  }

  if( cmApFileFree() != kOkApRC )
  {
    rc = cmErrMsg(&p->err,kAfpDevSysFailAdRC,"The audio file device system release failed.");
    goto errLabel;
  }

  if( cmApNrtFree() != kOkAgRC )
  {
    rc = cmErrMsg(&p->err,kNrtDevSysFailAdRC,"The non-real-time device system release failed.");
    goto errLabel;
  }

  if( cmApAggFree() != kOkAgRC )
  {
    rc = cmErrMsg(&p->err,kAggDevSysFailAdRC,"The aggregate device system release failed.");
    goto errLabel;
  }

  cmMemPtrFree(&p->nrtDevArray);

  unsigned i;
  for(i=0; i<p->aggDevCnt; ++i)
    cmMemPtrFree(&p->aggDevArray[i].physDevIdxArray);

  cmMemPtrFree(&p->aggDevArray);

  for(i=0; i<p->asCfgCnt; ++i)
    cmMemPtrFree(&p->asCfgArray[i].cfg.ssArray);

  cmMemPtrFree(&p->asCfgArray);

  cmMemPtrFree(&p->dsSsArray);

  if( cmJsonFinalize(&p->sysJsH) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailAdRC,"System JSON tree finalization failed.");
    goto errLabel;
  }

  if( p->sysJsFn != NULL )
    cmFsFreeFn(p->sysJsFn);

  cmMemFree(p);

 errLabel:

  return rc;
}

cmAdRC_t cmAudDspAlloc( cmCtx_t* ctx, cmAdH_t* hp, cmMsgSendFuncPtr_t cbFunc, void* cbDataPtr )
{

  cmAdRC_t      rc          = kOkAdRC;
  cmAdRC_t rc0 = kOkAdRC;

  if((rc = cmAudDspFree(hp)) != kOkAdRC )
    return rc;

  cmAd_t* p = cmMemAllocZ(cmAd_t,1);

  cmErrSetup(&p->err,&ctx->rpt,"Audio DSP Engine");

  // form the audio dsp resource file name
  if((p->sysJsFn = cmFsMakeFn( cmFsPrefsDir(),cmAudDspSys_FILENAME,NULL,NULL)) == NULL )
  {
    rc = cmErrMsg(&p->err,kFileSysFailAdRC,"Unable to form the audio dsp system resource file name.");
    goto errLabel;
  }

  // open the audio dsp resource file
  if(cmJsonInitializeFromFile(&p->sysJsH,p->sysJsFn,ctx) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailAdRC,"Unable to open the audio dsp resource file: '%s'.",cmStringNullGuard(p->sysJsFn));
    goto errLabel;
  }

  // parse the JSON tree
  if((rc = _cmAdParseSysJsonTree(p)) != kOkAdRC )
    goto errLabel;

  // create the aggregate device
  if( _cmAdCreateAggDevices(p) != kOkAdRC )
    goto errLabel;

  // create the non-real-time devices
  if( _cmAdCreateNrtDevices(p) != kOkAdRC )
    goto errLabel;

  // create the audio file devices
  if( _cmAdCreateAfpDevices(p) != kOkAdRC )
    goto errLabel;

  // initialize the audio device system
  if( cmApInitialize(&ctx->rpt) != kOkApRC )
  {
    rc = cmErrMsg(&p->err,kAudioPortFailAdRC,"Audio port intialization failed.");
    goto errLabel;
  }

  // initialize the audio buffer
  if( cmApBufInitialize( cmApDeviceCount(), p->meterMs ) != kOkApRC )
  {
    rc = cmErrMsg(&p->err,kAudioPortFailAdRC,"Audio port buffer initialization failed.");
    goto errLabel;
  }

  // initialize the MIDI system
  if( cmMpInitialize(ctx,NULL,NULL,p->midiPortBufByteCnt,"app") != kOkMpRC )
  {
    rc = cmErrMsg(&p->err,kMidiSysFailAdRC,"The MIDI system initialization failed.");
    goto errLabel;
  }

  // initialize the UDP network - but do not go into 'listening' mode.
  if( cmUdpNetAllocJson(ctx,&p->netH,p->sysJsH,_cmAdUdpNetCallback,p,kNetOptionalUnFl) != kOkUnRC )
  {
    cmErrMsg(&p->err,kNetSysFailAdRC,"The UDP network initialization failed.");
    goto errLabel;
  }

  if((rc = _cmAdSetup(p)) != kOkAdRC )
    goto errLabel;

  // initialize the DSP system
  if( cmDspSysInitialize(ctx,&p->dsH,p->netH) )
  {
    rc = cmErrMsg(&p->err,kDspSysFailAdRC,"The DSP system initialization failed.");
    goto errLabel;
  }

  // allocate the audio system
  if( cmAudioSysAllocate(&p->asH, &ctx->rpt, NULL ) != kOkAsRC )
  {
    rc = cmErrMsg(&p->err,kAudioSysFailAdRC,"The audio system allocation failed.");
    goto errLabel;
  }

  p->cbFunc      = cbFunc;
  p->cbDataPtr   = cbDataPtr;
  p->curAsCfgIdx = cmInvalidIdx;
  p->ctx         = *ctx;
  

  hp->h          = p;

 errLabel:

  
  if( rc != kOkAdRC )
    rc0 = _cmAudDspFree(p);

  return rc == kOkAdRC ? rc0 : rc;
}

cmAdRC_t cmAudDspFree( cmAdH_t* hp )
{
  cmAdRC_t rc = kOkAdRC;

  if( hp == NULL || cmAudDspIsValid(*hp)==false )
    return kOkAdRC;

  cmAd_t* p = _cmAdHandleToPtr(*hp);
  
  if((rc = _cmAudDspFree(p)) != kOkAdRC )
    return rc;

  hp->h = NULL;

  return rc;
}

cmAdRC_t cmAudDspSendSetup( cmAdH_t h )
{
  cmAdRC_t rc = kOkAdRC;
  cmAd_t* p = _cmAdHandleToPtr( h );

  // notify the client of the available audio system configurations
  if((rc =  _cmAdSendAudioSysCfgLabels(p)) != kOkAdRC )
    goto errLabel;

  // notify the client of the available devices
  if((rc =  _cmAdSendDeviceLabels(p)) != kOkAdRC) 
    goto errLabel;

  // notify the client of the available programs
  if((rc = _cmAdSendProgramLabels(p)) != kOkAdRC )
    goto errLabel;
   
 errLabel:
  return rc;
}

bool cmAudDspIsValid( cmAdH_t h )
{ return h.h != NULL; }


cmAdRC_t _cmAudDspUnloadPgm( cmAd_t* p, unsigned asSubSysIdx )
{
  cmAdRC_t rc = kOkAdRC;
  const cmAdAsCfg_t* cfgPtr = NULL;
  
  // Must disable audio thread callbacks to _cmAudDspCallback() 
  // while changing DSP system data structures.
  if( cmAudioSysIsEnabled(p->asH) )
    if(cmAudioSysEnable(p->asH,false) != kOkAsRC )
    {
      rc = cmErrMsg(&p->err,kAudioSysFailAdRC,"The audio system could not be disabled.");
      goto errLabel;
    }

  // validate the sub-system index
  if( asSubSysIdx >= p->dsSsCnt )
  {
    rc = cmErrMsg(&p->err,kInvalidSubSysIdxAdRC,"The invalid sub-system index %i was encountered while unloading a program.",asSubSysIdx);
    goto errLabel;
  }

  // if a valid cfg recd exists
  if( p->curAsCfgIdx != cmInvalidIdx )
  {
    // pointer to  audio system configuration
    cfgPtr = p->asCfgArray + p->curAsCfgIdx;

    // count of audio system sub-systems should be the same as the current cfg
    assert( p->dsSsCnt  == cfgPtr->cfg.ssCnt );

    // mark the DSP program as unloaded and pre-sync
    p->dsSsArray[ asSubSysIdx ].isLoadedFl = false;
    p->dsSsArray[ asSubSysIdx ].isSyncFl   = false;
    p->syncFl                              = false;
  }

  // unload the current program
  if( cmDspSysUnload(p->dsSsArray[asSubSysIdx].dsH) != kOkDspRC )
  {
    rc = cmErrMsg(&p->err,kDspSysFailAdRC,"Program unload failed.");
    goto errLabel;
  }


 errLabel:
  return rc;  
}


cmAdRC_t _cmAudDspUnloadAudioSys( cmAd_t* p )
{
  unsigned i;
  cmAdRC_t rc = kOkAdRC;

  p->syncFl = false;

  for(i=1; i<p->dsSsCnt; ++i)
  {
    if((rc = _cmAudDspUnloadPgm(p,i)) != kOkAdRC )
      goto errLabel;

    if( cmDspSysFinalize(&p->dsSsArray[i].dsH) != kOkDspRC )
    {
      rc = cmErrMsg(&p->err,kDspSysFailAdRC,"DSP system finalization failed.");
      goto errLabel;
    }
  }

  p->curAsCfgIdx = cmInvalidIdx;

 errLabel:
  return rc;
}

cmAdRC_t _cmAdSendIntMsgToHost( cmAd_t* p, unsigned asSubIdx, unsigned selId, unsigned flags, unsigned intValue )
{
  cmAdRC_t rc = kOkAdRC;
  cmDspValue_t v;
  cmDsvSetUInt(&v,intValue);

  if( cmMsgSend( &p->err,asSubIdx,kUiSelAsId,selId,flags,cmInvalidId,cmInvalidId,&v,p->cbFunc,p->cbDataPtr) != kOkMsgRC )
    rc = cmErrMsg(&p->err,kSendMsgFailAdRC,"Error sending message to host.");

  return rc;
}

// verify that a valid audio cfg has been selected
cmAdRC_t _cmAdIsAudioSysLoaded( cmAd_t* p )
{
  cmAdRC_t rc = kOkAdRC;

  if( cmAudioSysHandleIsValid(p->asH) == false )
  {
    rc = cmErrMsg(&p->err,kAudioSysFailAdRC,"The audio system is not allocated.");
    goto errLabel;
  }

  if( p->curAsCfgIdx == cmInvalidIdx )
    return kInvalidCfgIdxAdRC;

  // verify that an audio system is loaded
  if( cmAudioSysIsInitialized(p->asH) && p->curAsCfgIdx == cmInvalidIdx )
  {
    rc = cmErrMsg(&p->err,kInvalidCfgIdxAdRC,"The audio system has not been configured.");
    goto errLabel;
  }
  
  // count of audio system sub-systems should be the same as the current cfg
  assert( p->dsSsCnt  == p->asCfgArray[p->curAsCfgIdx].cfg.ssCnt );

 errLabel:
  return rc;
}

// verify that a valid audio cfg and DSP program has been selected
cmAdRC_t _cmAdIsPgmLoaded( cmAd_t* p, bool verboseFl )
{
  cmAdRC_t rc;
  
  // a program cannot be loaded if the audio system has not been configured
  if((rc = _cmAdIsAudioSysLoaded(p)) != kOkAdRC )
    return rc;

  unsigned     i;

  // for each sub-system
  for(i=0; i<p->dsSsCnt; ++i)
  {
    // verify that the DSP system has been created
    if( cmDspSysIsValid(p->dsSsArray[i].dsH) == false )
      return cmErrMsg(&p->err,kDspSysFailAdRC,"The DSP sub-system at index %i is not initialized.",i);

    // verify that the DSP program was loaded
    if( p->dsSsArray[ i ].isLoadedFl == false )
    {
      if( verboseFl )
        cmErrMsg(&p->err,kNoPgmLoadedAdRC,"There is no program loaded.");

      return kNoPgmLoadedAdRC;
    }
  }

  return rc;
}

bool _cmAudDspIsPgmSynced( cmAd_t* p )
{
  unsigned syncCnt = 0;
  unsigned i;

  if( p->syncFl  )
    return true;

  // if the pgm is not loaded then it cannot be sync'd
  if(_cmAdIsPgmLoaded(p,false) != kOkAdRC  )
    return false;

  // check each sub-system
  for(i=0; i<p->dsSsCnt; ++i)
  {
    unsigned syncState = cmDspSysSyncState(p->dsSsArray[i].dsH);

    // if the subsys is already synced
    if( p->dsSsArray[i].isSyncFl )
      ++syncCnt;
    else
    {
      switch( syncState )
      {
        // the sub-sys is pre or pending sync mode
        case kSyncPreDspId:    
        case kSyncPendingDspId:
          break;

          // sync mode completed - w/ success or fail
        case kSyncSuccessDspId:
        case kSyncFailDspId:
          {
            // notify the client of the the sync state
            bool syncFl = syncState == kSyncSuccessDspId;
            _cmAdSendIntMsgToHost(p,cmInvalidIdx,kSyncDuiId,syncFl,cmInvalidIdx);
            p->dsSsArray[i].isSyncFl = syncFl;
          }
          break;

      }   
    }
  }

 p->syncFl =  syncCnt == p->dsSsCnt;

 return p->syncFl; 
}


cmAdRC_t _cmAudDspLoadPgm( cmAd_t* p, unsigned asSubSysIdx, unsigned pgmIdx )
{
  cmAdRC_t rc = kOkAdRC;
  unsigned i;

  p->syncFl = false;

  // the audio system must be configured before a program is loaded
  if((rc = _cmAdIsAudioSysLoaded(p)) != kOkAdRC )
    return cmErrMsg(&p->err,rc,"The audio system is not configured. Program load failed.");

  // validate the sub-system index arg.
  if( asSubSysIdx!=cmInvalidIdx && asSubSysIdx >= p->dsSsCnt )
  {
    rc = cmErrMsg(&p->err,kInvalidSubSysIdxAdRC,"The sub-system index %i is invalid. Program load failed.",asSubSysIdx);
    goto errLabel;
  }

  // for each sub-system
  for(i=0; i<p->dsSsCnt; ++i)
    if( asSubSysIdx==cmInvalidIdx || i==asSubSysIdx )
    {
      // unload any currently loaded program on this sub-system
      // (unloading a program automatically disables the audio system)
      if((rc = _cmAudDspUnloadPgm(p, i )) != kOkAdRC )
        goto errLabel;

      // load the program
      if( cmDspSysLoad(p->dsSsArray[ i ].dsH, cmAudioSysContext(p->asH,i), pgmIdx ) != kOkDspRC )
      {
        rc = cmErrMsg(&p->err,kDspSysFailAdRC,"The program load failed on audio sub-system %i.",i);
        goto errLabel;
      }

      // update the state of the DSP sub-system
      p->dsSsArray[i].curPgmIdx   = pgmIdx;
      p->dsSsArray[i].isLoadedFl  = true;
      p->dsSsArray[i].isSyncFl    = false;
      p->syncFl                   = false;

      // notify the host of the new program
      _cmAdSendIntMsgToHost(p,i,kSetPgmDuiId,0,pgmIdx);
    }

 errLabel:
  return rc;
}

cmAdRC_t _cmAudDspPrintPgm( cmAd_t* p, unsigned asSubSysIdx, const cmChar_t* fn )
{
  cmAdRC_t rc = kOkAdRC;
  unsigned i;

  // the audio system must be configured before a program is loaded
  if((rc = _cmAdIsAudioSysLoaded(p)) != kOkAdRC )
    return cmErrMsg(&p->err,rc,"The audio system is not configured. Program print failed.");

  // validate the sub-system index arg.
  if( asSubSysIdx!=cmInvalidIdx && asSubSysIdx >= p->dsSsCnt )
  {
    rc = cmErrMsg(&p->err,kInvalidSubSysIdxAdRC,"The sub-system index %i is invalid. Program print failed.",asSubSysIdx);
    goto errLabel;
  }

  // for each sub-system
  for(i=0; i<p->dsSsCnt; ++i)
    if(  i==asSubSysIdx || asSubSysIdx==cmInvalidIdx )
    {
      if( cmDspSysPrintPgm(p->dsSsArray[i].dsH,fn) != kOkDspRC )
        rc = cmErrMsg(&p->err,kDspSysFailAdRC,"The program print failed.");
      
      break;
    }

 errLabel:
  return rc;
}

cmAdRC_t _cmAdReinitAudioSys( cmAd_t* p )
{
  cmAdRC_t rc = kOkAdRC;

  p->syncFl = false;

  // pointer to the new audio system configuration
  cmAdAsCfg_t* cfgPtr = p->asCfgArray + p->curAsCfgIdx;

  // initialize the audio system
  if( cmAudioSysInitialize(p->asH, &cfgPtr->cfg ) != kOkAsRC )
  {
    rc = cmErrMsg(&p->err,kAudioSysFailAdRC,"The audio system initialization failed.");
    goto errLabel;
  }

  // reload any currently loaded programs
  unsigned i;
  for(i=0; i<p->dsSsCnt; ++i)
  {
    unsigned pgmIdx;
    if((pgmIdx = p->dsSsArray[i].curPgmIdx) != cmInvalidIdx )
      if((rc = _cmAudDspLoadPgm(p,i,pgmIdx)) != kOkAdRC )
        break;
  }

 errLabel:
  return rc;
}

cmAdRC_t _cmAudDspLoadAudioSys( cmAd_t* p, unsigned asCfgIdx )
{
  cmAdRC_t     rc = kOkAdRC;
  cmAdAsCfg_t* cfgPtr;
  unsigned     i;

  // validate asCfgIdx
  if( asCfgIdx >= p->asCfgCnt )
  {
    cmErrMsg(&p->err,kInvalidCfgIdxAdRC,"The audio system index %i is invalid.",asCfgIdx);
    goto errLabel;
  }

  // clear the current audio system setup - this will automatically disable the audio system
  if((rc = _cmAudDspUnloadAudioSys(p)) != kOkAdRC )
    goto errLabel;

  // pointer to the new audio system configuration
  cfgPtr = p->asCfgArray + asCfgIdx;

  // get the count of audio system sub-systems
  p->dsSsCnt     = cfgPtr->cfg.ssCnt;

  // store the index of the current audio system configuration
  p->curAsCfgIdx = asCfgIdx;

  if( p->dsSsCnt > 0 )
  {
    p->dsSsArray   = cmMemResizeZ(cmAdDsSubSys_t, p->dsSsArray, p->dsSsCnt );

    for(i=0; i<p->dsSsCnt; ++i)
    {
      cmDspSysH_t dsH;

      // the first sub-system will always use the existing DSP system handle ...
      if( i==0 )
        dsH = p->dsH;
      else
      {
        // ... and allocate additional DSP systems when more than one sub-sys is 
        // defined in the audio system configuration
        if( cmDspSysInitialize(&p->ctx,&dsH,p->netH) != kOkDspRC )
        {
          rc = cmErrMsg(&p->err,kDspSysFailAdRC,"Unable to initialize an additional DSP system.");
          goto errLabel;
        }
      }

      p->dsSsArray[i].dsH       = dsH;
      p->dsSsArray[i].curPgmIdx = cmInvalidIdx;
      p->dsSsArray[i].isLoadedFl= false;

      // this cbDataPtr is picked up in  _cmAudDspCallback().
      // It is used to connect the audio system to a DSP system handle.
      cfgPtr->cfg.ssArray[i].cbDataPtr = p->dsSsArray + i;

    }
  }

  // notify the client of the change of audio configuration
  _cmAdSendIntMsgToHost(p,cmInvalidIdx,kSetAudioCfgDuiId,0,asCfgIdx);

  // notify the client of the count of audio sub-systems
  _cmAdSendIntMsgToHost(p,cmInvalidIdx,kSubSysCntDuiId,0,p->dsSsCnt);

  // for each sub-system
  for(i=0; i<p->dsSsCnt; ++i)
  {
    // notify the client of the currently selected devices
    _cmAdSendIntMsgToHost(p,i,kSetAudioDevDuiId,true, cfgPtr->cfg.ssArray[i].args.inDevIdx);
    _cmAdSendIntMsgToHost(p,i,kSetAudioDevDuiId,false,cfgPtr->cfg.ssArray[i].args.outDevIdx);

    // notify the client of the sample rate
    _cmAdSendIntMsgToHost(p,i,kSetSampleRateDuiId,0,(unsigned)cfgPtr->cfg.ssArray[i].args.srate);

    _cmAdSendIntMsgToHost(p,i,kSetPgmDuiId,0,cmInvalidIdx);
  }

  // the audio system configuration changed so we need to initialize the audio system
  if((rc = _cmAdReinitAudioSys(p)) != kOkAdRC )
    goto errLabel;

 errLabel:
  if( rc != kOkAdRC )
    _cmAudDspUnloadAudioSys(p);

  return rc;
}



cmAdRC_t _cmAudDspEnableAudio( cmAd_t* p, bool enableFl )
{
  cmAdRC_t rc = kOkAdRC;

  if( enableFl )
  {
    // verify an audio system cfg and DSP program has been selected
    if(( rc =  _cmAdIsPgmLoaded(p,true)) != kOkAdRC )
      return cmErrMsg(&p->err,rc,"Audio enable failed.");

    // if the audio system is already enabled/disabled then do nothing
    if( cmAudioSysIsEnabled(p->asH) == enableFl )
      return kOkAdRC;

    // for each sub-system
    unsigned i;
    for(i=0; i<p->dsSsCnt; ++i)
    {
      if( cmDspSysReset(p->dsSsArray[i].dsH) != kOkDspRC )
      {
        rc = cmErrMsg(&p->err,kDspSysFailAdRC,"The DSP system reset failed.");
        goto errLabel;
      }
    }
    
  }

  // start/stop the audio sub-system
  if( cmAudioSysEnable(p->asH,enableFl) != kOkAsRC )
  {
    rc = cmErrMsg(&p->err,kAudioSysFailAdRC,"The audio system %s failed.", enableFl ? "enable" : "disable");
    goto errLabel;
  }

  // notify the host of the new enable state
  _cmAdSendIntMsgToHost(p,cmInvalidIdx,kEnableDuiId,enableFl,cmInvalidIdx);

 errLabel:
  return rc;

}

cmAdRC_t  _cmAudDspSetDevice(cmAd_t* p,unsigned asSubIdx, bool inputFl, unsigned devIdx)
{
  cmAdRC_t rc;

  // a device cannot be set if the audio system is not already configured
  if((rc = _cmAdIsAudioSysLoaded(p)) != kOkAdRC )
    return cmErrMsg(&p->err,rc,"Set audio device failed.");

  cmAdAsCfg_t* cfgPtr = p->asCfgArray + p->curAsCfgIdx;

  // validate the sub-system index
  if( asSubIdx >= p->dsSsCnt )
  {
    rc = cmErrMsg(&p->err,kInvalidSubSysIdxAdRC,"The sub-system index %i is invalid.",asSubIdx);
    goto errLabel;
  }

  // assign the new device index to the indicated audio system configuration recd
  if( inputFl )
  {
    if( cfgPtr->cfg.ssArray[ asSubIdx ].args.inDevIdx != devIdx )
      cfgPtr->cfg.ssArray[ asSubIdx ].args.inDevIdx = devIdx;
    
  }
  else
  {
    if( cfgPtr->cfg.ssArray[ asSubIdx ].args.outDevIdx != devIdx )
      cfgPtr->cfg.ssArray[ asSubIdx ].args.outDevIdx = devIdx;
  }
  
  // notify the host that the new device has been set
  _cmAdSendIntMsgToHost(p,asSubIdx,kSetAudioDevDuiId,inputFl, devIdx);

  // reinitialize the audio system
  rc =  _cmAdReinitAudioSys(p);

 errLabel:

  return rc;
}

cmAdRC_t  _cmAudDspSetSampleRate(cmAd_t* p, unsigned asSubIdx, double srate )
{
  cmAdRC_t rc;

  if((rc = _cmAdIsAudioSysLoaded(p)) != kOkAdRC )
    return cmErrMsg(&p->err,rc,"Set audio device failed.");

  cmAdAsCfg_t* cfgPtr = p->asCfgArray + p->curAsCfgIdx;

  // validate the sub-system index
  if( asSubIdx != cmInvalidIdx && asSubIdx >= p->dsSsCnt )
  {
    rc = cmErrMsg(&p->err,kInvalidSubSysIdxAdRC,"The sub-system index %i is invalid.",asSubIdx);
    goto errLabel;
  }

  unsigned i;
  for(i=0; i<p->dsSsCnt; ++i)
  {
    // assign the new device index to the indicated audio system configuration recd
    if( asSubIdx==cmInvalidIdx || asSubIdx == i )
    {
      if( cfgPtr->cfg.ssArray[ i ].args.srate != srate )
        cfgPtr->cfg.ssArray[ i ].args.srate = srate;
    }
  }

  // notify the client of the new sample rate
  _cmAdSendIntMsgToHost(p,asSubIdx,kSetSampleRateDuiId,0,(unsigned)srate);

  // reinitialize the audio system
  rc =  _cmAdReinitAudioSys(p);

 errLabel:

  return rc;
}


cmAdRC_t _cmAudDspClientMsgPoll( cmAd_t* p )
{
  unsigned i = 0;

  // if the program is not synced then don't bother polling the audio system
  if( _cmAudDspIsPgmSynced(p) == false )
    return kOkAdRC;

  for(i=0; i<p->msgsPerClientPoll; ++i)
  {
    if( cmAudioSysIsMsgWaiting(p->asH) == 0 )
      break;

    if(cmAudioSysReceiveMsg(p->asH,NULL,0) != kOkAsRC )
      return cmErrMsg(&p->err,kAudioSysFailAdRC,"The delivery of an audio system msg for the client failed.");

  }

  return kOkAdRC;
}

cmAdRC_t cmAudDspReceiveClientMsg( cmAdH_t h, unsigned msgByteCnt, const void* msg )
{
  cmAdRC_t      rc = kOkAdRC;
  cmAd_t*       p  = _cmAdHandleToPtr(h);
  cmDspUiHdr_t* m  = (cmDspUiHdr_t*)msg;
  /*
  if( m->uiId != kUiSelAsId )
  {
    rc = cmErrMsg(&p->err,kUnknownMsgTypeAdRC,"The message type %i is unknown.");
    goto errLabel;
  }
  */

  switch( m->selId )
  {
    case kDevReportDuiId:
      cmRptPrintf(p->err.rpt,"\nAUDIO DEVICES\n");
      cmApReport(p->err.rpt);
      cmRptPrintf(p->err.rpt,"\nMIDI DEVICES\n");
      cmMpReport(p->err.rpt);
      break;
        
    case kSetAudioCfgDuiId:
      rc = _cmAudDspLoadAudioSys(p,cmDsvUInt(&m->value));
      break;

    case kSetPgmDuiId:
      rc = _cmAudDspLoadPgm(p,m->asSubIdx,cmDsvUInt(&m->value));
      break;

    case kSetAudioDevDuiId:   
      rc = _cmAudDspSetDevice(p,m->asSubIdx,m->flags,cmDsvUInt(&m->value));
      break;

    case kSetSampleRateDuiId:
      rc = _cmAudDspSetSampleRate(p,m->asSubIdx,cmDsvDouble(&m->value));
      break;

    case kEnableDuiId:
      rc =  _cmAudDspEnableAudio(p,m->flags);
      break;

    case kSetNotifyEnableDuiId:
      cmAudioSysStatusNotifyEnable(p->asH, cmInvalidIdx, m->flags );
      break;

    case kClientMsgPollDuiId:
      rc = _cmAudDspClientMsgPoll(p);
      break;

    case kPrintPgmDuiId:
      _cmAudDspPrintPgm(p,m->asSubIdx,cmDsvStrcz(&m->value));
      break;

    default:
      if( cmAudioSysDeliverMsg(p->asH,msg,msgByteCnt,cmInvalidId) != kOkAsRC )
        rc = cmErrMsg(&p->err,kSendMsgFailAdRC,"Message delivery to the audio system failed.");
      break;
  }
    
  

  // errLabel:
  return rc;
}
