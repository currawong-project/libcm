#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmJson.h"
#include "cmMidi.h"
#include "cmMidiPort.h"

#include "cmDevCfg.h"

cmDevCfgH_t cmDevCfgNullHandle = cmSTATIC_NULL_HANDLE;


typedef struct
{
  cmChar_t* dcLabelStr;     // Name of this cfg recd or NULL if the recd is inactive.
  cmChar_t* devLabelStr;    // Midi device label.
  cmChar_t* portLabelStr;   // Midi device port label.
  bool      inputFl;        // 'True' if this is an input port.
  unsigned  devIdx;         // Midi device index.
  unsigned  portIdx;        // Midi port index.
} cmDcmMidi_t;

typedef struct
{
  cmChar_t* dcLabelStr;        // Name of this cfg record or NULL if the recd is inactive.
  cmChar_t* inDevLabelStr;     // Input audio device label.
  cmChar_t* outDevLabelStr;    // Output audio device label.
  bool      syncToInputFl;     // 'True' if the audio system should sync to the input port.
  unsigned  msgQueueByteCnt;   // Audio system msg queue in bytes.
  unsigned  devFramesPerCycle; // Audio system sample frames per device callback.
  unsigned  dspFramesPerCycle; // DSP system samples per block.
  unsigned  audioBufCnt;       // Count of audio buffers (of size devFramesPerCycle) 
  double    srate;             // Audio system sample rate.
} cmDcmAudio_t;

typedef struct              
{
  cmChar_t* dcLabelStr;        // Name of this cfg recd or NULL if the recd is inactive
  cmChar_t* sockAddr;          // Remote socket address.
  unsigned  portNumber;        // Remote socket port number
  unsigned  netNodeId;         // Network node id associated with sockAddr and portNumber.
} cmDcmNet_t;

typedef struct
{
  cmTypeDcmId_t tid;      // Type Id for this map or tInvalidDcmTId if the record is not active.
  unsigned   usrDevId; // Same as index into p->map[] for this recd.
  unsigned   cfgIndex; // Index into p->midi[],p->audio[], or p->net[].
} cmDcmMap_t;

typedef struct
{
  unsigned    usrAppId;
  bool        activeFl;
  cmDcmMap_t* map;
  unsigned    mapCnt;
} cmDcmApp_t;

typedef struct
{
  cmErr_t       err;

  cmDcmApp_t*   app;
  unsigned      appCnt;

  cmDcmMidi_t*  midi;
  unsigned      midiCnt;

  cmDcmAudio_t* audio;
  unsigned      audioCnt;

  cmDcmNet_t*   net;
  unsigned      netCnt;
} cmDcm_t;

cmDcm_t* _cmDcmHandleToPtr( cmDevCfgH_t h )
{
  cmDcm_t* p = (cmDcm_t*)h.h;
  assert( p!=NULL );
  return p;
}

void _cmDcmMidiFree( cmDcmMidi_t* r )
{
  cmMemPtrFree(&r->dcLabelStr);
  cmMemPtrFree(&r->devLabelStr);
  cmMemPtrFree(&r->portLabelStr);
}

void _cmDcmAudioFree( cmDcmAudio_t* r )
{
  cmMemPtrFree(&r->dcLabelStr);
  cmMemPtrFree(&r->inDevLabelStr);
  cmMemPtrFree(&r->outDevLabelStr);
}

void _cmDcmNetFree( cmDcmNet_t* r )
{
  cmMemPtrFree(&r->dcLabelStr);
  cmMemPtrFree(&r->sockAddr);
}

void _cmDcmAppFree( cmDcmApp_t* r )
{
  cmMemPtrFree(&r->map);
}

cmDcRC_t _cmDcmFree( cmDcm_t* p )
{
  unsigned i;

  for(i=0; p->midi!=NULL && i<p->midiCnt; ++i)
    _cmDcmMidiFree(p->midi + i );
  cmMemPtrFree(&p->midi);

  for(i=0; p->audio!=NULL && i<p->audioCnt; ++i)
    _cmDcmAudioFree(p->audio + i );
  cmMemPtrFree(&p->audio);

  for(i=0; p->net!=NULL && i<p->netCnt; ++i)
    _cmDcmNetFree(p->net + i );
  cmMemPtrFree(&p->net);

  for(i=0; p->app!=NULL && i<p->appCnt; ++i)
    _cmDcmAppFree(p->app + i );
  cmMemPtrFree(&p->app);
  
  return kOkDcRC;
}

cmDcmMidi_t* _cmDcmMidiFind( cmDcm_t* p, const cmChar_t* dcLabelStr, bool errFl )
{
  assert( dcLabelStr != NULL );

  unsigned i;
  for(i=0; i<p->midiCnt; ++i)
    if(p->midi[i].dcLabelStr!=NULL && strcmp(p->midi[i].dcLabelStr,dcLabelStr)==0)
      return p->midi + i;

  if( errFl )
    cmErrMsg(&p->err,cmLabelNotFoundDcRC,"The MIDI cfg. record '%s' not found.",dcLabelStr);

  return NULL;
}

cmDcmAudio_t* _cmDcmAudioFind( cmDcm_t* p, const cmChar_t* dcLabelStr, bool errFl )
{
  assert( dcLabelStr != NULL );

  unsigned i;
  for(i=0; i<p->audioCnt; ++i)
    if(p->audio[i].dcLabelStr!=NULL && strcmp(p->audio[i].dcLabelStr,dcLabelStr)==0)
      return p->audio + i;

  if( errFl )
    cmErrMsg(&p->err,cmLabelNotFoundDcRC,"The audio cfg. record '%s' not found.",dcLabelStr);

  return NULL;
}

cmDcmNet_t* _cmDcmNetFind( cmDcm_t* p, const cmChar_t* dcLabelStr, bool errFl )
{
  assert( dcLabelStr != NULL );

  unsigned i;
  for(i=0; i<p->netCnt; ++i)
    if(p->net[i].dcLabelStr!=NULL && strcmp(p->net[i].dcLabelStr,dcLabelStr)==0)
      return p->net + i;

  if( errFl )
    cmErrMsg(&p->err,cmLabelNotFoundDcRC,"The net cfg. record '%s' not found.",dcLabelStr);

  return NULL;
}


cmDcmApp_t*  _cmDcmFindOrCreateApp(cmDcm_t* p, unsigned usrAppId ) 
{
  cmDcmApp_t* a;
  if( usrAppId < p->appCnt )
    a = p->app + usrAppId;
  else
  {
    p->appCnt = usrAppId + 1;
    p->app    = cmMemResizePZ(cmDcmApp_t,p->app,p->appCnt);
    a = p->app + usrAppId;
    a->usrAppId = usrAppId;
    a->activeFl = true;    
  }

  return a;
}

cmDcmMap_t* _cmDcmFindOrCreateMap(cmDcm_t* p, cmDcmApp_t* a, unsigned usrDevId )
{
  cmDcmMap_t* m;

  if( usrDevId < a->mapCnt )
    m = a->map + usrDevId;
  else
  {
    a->mapCnt   = usrDevId + 1;
    a->map      = cmMemResizePZ(cmDcmMap_t,a->map,a->mapCnt);        
    m           = a->map + usrDevId;    
    m->usrDevId = usrDevId;
  }

  return m;
}

cmDcRC_t  _cmDcmLookupApp(cmDcm_t* p, unsigned usrAppId, cmDcmApp_t** appRef)
{
  // validate the usrAppId
  if( usrAppId >= p->appCnt )
    return cmErrMsg(&p->err,kInvalidDevArgDcRC,"Invalid user app. id:%i\n",usrAppId);

  // check that the app recd is active
  if( p->app[usrAppId].activeFl == false )
    return cmErrMsg(&p->err,kInvalidDevArgDcRC,"The user app. with id:%i is not active.",usrAppId);

  *appRef = p->app + usrAppId;

  return kOkDcRC;
}

cmDcRC_t  _cmDcmLookupMap(cmDcm_t* p, cmDcmApp_t* a, unsigned usrDevId, cmDcmMap_t** mapRef )
{
  // validate the usrDevId
  if( usrDevId >= a->mapCnt )
    return cmErrMsg(&p->err,kInvalidDevArgDcRC,"Invalid user device id:%i on app:%i\n",usrDevId,a->usrAppId);

  // check that the map recd is active
  if( a->map[ usrDevId ].tid == kInvalidDcmTId )
    return cmErrMsg(&p->err,kInvalidDevArgDcRC,"The user device id:%i on app:%i is not active.",usrDevId,a->usrAppId);

  *mapRef =  a->map + usrDevId;
  return kOkDcRC;
}

cmDcRC_t  _cmDcmLookupAppMap(cmDcm_t* p, unsigned usrAppId, unsigned usrDevId, cmDcmMap_t** mapRef )
{
  cmDcRC_t rc;
  cmDcmApp_t* a;

  if((rc = _cmDcmLookupApp(p,usrAppId,&a)) == kOkDcRC )
    return rc;

  return _cmDcmLookupMap(p,a,usrDevId,mapRef );
}

// Delete all maps in all apps which reference a particular cfg recd.
void _cmDevCfgDeleteCfgMaps( cmDcm_t* p, cmTypeDcmId_t tid, unsigned cfgIndex )
{
  unsigned i,j;
  for(i=0; i<p->appCnt; ++i)
    if( p->app[i].activeFl )
      for(j=0; j<p->app[i].mapCnt; ++j)
        if( p->app[i].map[j].tid == tid && p->app[i].map[j].cfgIndex == cfgIndex )
        {
          p->app[i].map[j].tid = kInvalidDcmTId;
          break;
        }
}


cmDcRC_t cmDevCfgMgrAlloc( cmCtx_t* ctx, cmDevCfgH_t* hp, cmJsonH_t jsH )
{
  cmDcRC_t rc;
  if((rc = cmDevCfgMgrFree(hp)) != kOkDcRC )
    return rc;

  cmDcm_t* p = cmMemAllocZ(cmDcm_t,1);
  cmErrSetup(&p->err,&ctx->rpt,"DevCfgMgr");


  hp->h = p;

  if( rc != kOkDcRC )
    _cmDcmFree(p);

  return rc;
}


cmDcRC_t cmDevCfgMgrFree( cmDevCfgH_t* hp )
{
  cmDcRC_t rc = kOkDcRC;

  if(hp!=NULL || cmDevCfgIsValid(*hp))
    return rc;

  cmDcm_t* p = _cmDcmHandleToPtr(*hp);

  if((rc = _cmDcmFree(p)) != kOkDcRC )
    return rc;

  cmMemFree(p);

  hp->h = NULL;

  return rc;
}

cmDcRC_t cmDevCfgIsValid( cmDevCfgH_t h )
{ return h.h != NULL; }

cmDcRC_t _cmDcmFindCfgIndex( cmDcm_t* p, cmTypeDcmId_t tid, const cmChar_t* dcLabelStr, unsigned* cfgIndexRef )
{
  cmDcRC_t rc = kOkDcRC;

  *cfgIndexRef = cmInvalidIdx;

  switch( tid )
  {
    case kMidiDcmTId:
      {
        const cmDcmMidi_t* r;

        if((r = _cmDcmMidiFind(p,dcLabelStr,true)) == NULL )
          rc = cmErrLastRC(&p->err);
        else
          *cfgIndexRef = r - p->midi;
      }
      break;

    case kAudioDcmTId:
      {
        const cmDcmAudio_t* r;

        if((r = _cmDcmAudioFind(p,dcLabelStr,true)) == NULL )
          rc = cmErrLastRC(&p->err);
        else
          *cfgIndexRef = r - p->audio;
      }
      break;

    case kNetDcmTId:
      {
        const cmDcmNet_t* r;

        if((r = _cmDcmNetFind(p,dcLabelStr,true)) == NULL )
          rc = cmErrLastRC(&p->err);
        else
          *cfgIndexRef = r - p->net;
      }
      break;
      
    default:
      assert(0);
      break;
  }

  return rc;
}

cmDcRC_t _cmDevCfgDeleteCfg( cmDcm_t* p, cmTypeDcmId_t tid, unsigned cfgIndex )
{
  // release any resources held by this cfg record and mark
  // the record as inactive by setting the dcLabelStr field to NULL.
  switch( tid )
  {
    case kMidiDcmTId:
      _cmDcmMidiFree( p->midi + cfgIndex );
      break;

    case kAudioDcmTId:
      _cmDcmAudioFree( p->audio + cfgIndex );
      break;

    case kNetDcmTId:
      _cmDcmNetFree( p->net + cfgIndex );
      break;
      
    default:
      assert(0);
      break;
  }

  // delete all maps which reference this cfg recd
  if( cfgIndex != cmInvalidIdx )
    _cmDevCfgDeleteCfgMaps(p, tid, cfgIndex );
  

  return kOkDcRC;
}

cmDcRC_t cmDevCfgDeleteCfg(  cmDevCfgH_t h, cmTypeDcmId_t tid, const cmChar_t* dcLabelStr )
{
  cmDcRC_t rc;
  cmDcm_t* p        = _cmDcmHandleToPtr(h);
  unsigned cfgIndex = cmInvalidIdx;

  // locate the cfg record index
  if((rc = _cmDcmFindCfgIndex(p,tid,dcLabelStr,&cfgIndex)) == kOkDcRC )
    return rc;
  
  return _cmDevCfgDeleteCfg( p, tid, cfgIndex );
}

cmDcRC_t cmDevCfgCreateMap( cmDevCfgH_t h, cmTypeDcmId_t tid, const cmChar_t* dcLabelStr, unsigned usrAppId, unsigned usrDevId )
{
  cmDcRC_t    rc       = kOkDcRC;
  cmDcm_t*    p        = _cmDcmHandleToPtr(h);
  unsigned    cfgIndex = cmInvalidIdx;

  if((rc = _cmDcmFindCfgIndex(p,tid,dcLabelStr,&cfgIndex)) == kOkDcRC )
    return rc;

  if( cfgIndex != cmInvalidIdx )
  {
    cmDcmApp_t* a;
    cmDcmMap_t* m;

    // locate or create the requested app recrod
    if((a = _cmDcmFindOrCreateApp(p,usrAppId)) == NULL )
    {
      rc = cmErrLastRC(&p->err);
      goto errLabel;
    }

    // locate or create the requested map record
    if((m = _cmDcmFindOrCreateMap(p,a,usrDevId)) == NULL )
    {
      rc = cmErrLastRC(&p->err);
      goto errLabel;
    }

    m->usrDevId = usrDevId;
    m->tid      = tid;
    m->cfgIndex = cfgIndex;
  }

 errLabel:
  return rc;
}

cmDcRC_t cmDevCfgDeleteMap( cmDevCfgH_t h, cmTypeDcmId_t typeId, unsigned usrAppId, unsigned usrDevId )
{
  cmDcRC_t   rc = kOkDcRC;
  cmDcm_t*    p = _cmDcmHandleToPtr(h);
  cmDcmMap_t* m;

  if((rc =  _cmDcmLookupAppMap(p,usrAppId,usrDevId,&m)) != kOkDcRC )
    return rc;

  m->usrDevId = cmInvalidId;
  m->tid      = kInvalidDcmTId;
  m->cfgIndex = cmInvalidIdx;
  return rc;
}
  
cmDcRC_t cmDevCfgNameMidiPort( 
  cmDevCfgH_t     h,
  const cmChar_t* dcLabelStr, 
  const cmChar_t* devNameStr, 
  const cmChar_t* portNameStr,
  bool            inputFl )
{
  cmDcRC_t    rc = kOkDcRC;
  cmDcm_t*     p  = _cmDcmHandleToPtr(h);
  cmDcmMidi_t* r  = _cmDcmMidiFind(p,dcLabelStr,false);
  unsigned     i;

  // if 'dcLabelStr' was not already used then look for an empty MIDI record.
  if( r == NULL )
    for(i=0; i<p->midiCnt; ++i)
      if( p->midi[i].dcLabelStr == NULL )
      {
        r = p->midi + i;
        break;
      }
  
  // if no available cfg record exists then create one 
  if( r == NULL )
  {
    p->midi     = cmMemResizePZ(cmDcmMidi_t,p->midi,p->midiCnt+1);    
    r           = p->midi + p->midiCnt;
    p->midiCnt += 1;
  }

 
  assert( r != NULL );
 
  // verify that the device label is valid
  if((r->devIdx = cmMpDeviceNameToIndex( r->devLabelStr )) == cmInvalidIdx )
  {
    rc = cmErrMsg(&p->err, kInvalidDevArgDcRC,"The MIDI device name '%s' is not valid.",r->devLabelStr);
    goto errLabel;
  }

  // verify that the port label is valid
  if((r->portIdx = cmMpDevicePortNameToIndex( r->devIdx, r->inputFl ? kInMpFl : kOutMpFl, r->portLabelStr )) == cmInvalidIdx )
  {
    rc = cmErrMsg(&p->err, kInvalidDevArgDcRC,"The MIDI port name '%s' is not valid on the device '%s'.",r->portLabelStr,r->devLabelStr);
    goto errLabel;
  }

  // if this cfg recd was not previously active then assign a cfg label
  if( r->dcLabelStr == NULL )
    r->dcLabelStr  = cmMemAllocStr(dcLabelStr);

  // fill in the cfg recd
  r->devLabelStr  = cmMemResizeStr(r->devLabelStr,devNameStr);
  r->portLabelStr = cmMemResizeStr(r->portLabelStr,portNameStr);
  r->inputFl      = inputFl;

 errLabel:
  // on error delete the cfg record and any maps depending on it
  if( rc != kOkDcRC && r != NULL )
    _cmDevCfgDeleteCfg( p, kMidiDcmTId, r - p->midi );

  return rc;
}


cmDcRC_t cmDevCfgMidiDevIdx(    cmDevCfgH_t h, unsigned usrAppId, unsigned usrDevId, unsigned* midiDevIdxRef, unsigned* midiPortIdxRef )
{
  cmDcRC_t   rc = kOkDcRC;
  cmDcm_t*    p = _cmDcmHandleToPtr(h);
  cmDcmMap_t* m;

  if((rc =  _cmDcmLookupAppMap(p,usrAppId,usrDevId,&m)) != kOkDcRC )
    return rc;
                                                
  cmDcmMidi_t* r = p->midi + m->cfgIndex;

  assert(r->dcLabelStr != NULL );

  *midiDevIdxRef  = r->devIdx;
  *midiPortIdxRef = r->portIdx;

  return rc;
}

cmDcRC_t cmDevCfgNameAudioPort( 
  cmDevCfgH_t     h,
  const cmChar_t* dcLabelStr, 
  const cmChar_t* inDevNameStr, 
  const cmChar_t* outDevNameStr,
  bool            syncInputFl,
  unsigned        msgQueueByteCnt,
  unsigned        devFramesPerCycle,
  unsigned        dspFramesPerCycle,
  unsigned        audioBufCnt,
  double          srate  )
{
  return kOkDcRC;
}


const struct cmAudioSysArgs_str* cmDevCfgAudioSysArgs( cmDevCfgH_t h, unsigned usrAppId, unsigned usrDevId )
{
  return NULL;
}


cmDcRC_t cmDevCfgNetPort(
  cmDevCfgH_t      h,
  const cmChar_t* dcLabelStr,
  const cmChar_t* sockAddr,
  unsigned        portNumber )
{
  return kOkDcRC;
}


unsigned        cmDevCfgNetNodeId(     cmDevCfgH_t h, unsigned usrAppId, unsigned usrDevId )
{
  return cmInvalidId;
}


// Preset Management Functions:
unsigned        cmDevCfgPresetCount( cmDevCfgH_t h )
{
  return 0;
}

const cmChar_t* cmDevCfgPresetLabel( cmDevCfgH_t h, unsigned presetIdx )
{
  return NULL;
}

cmDcRC_t        cmDevCfgStore(       cmDevCfgH_t h, const cmChar_t* presetLabelStr )
{
  return kOkDcRC;
}

cmDcRC_t        cmDevCfgRecall(      cmDevCfgH_t h, const cmChar_t* presetLabelStr )
{
  return kOkDcRC;
}

cmDcRC_t        cmDevCfgDelete(      cmDevCfgH_t h, const cmChar_t* presetLabelStr )
{
  return kOkDcRC;
}


cmDcRC_t cmDevCfgWrite( cmDevCfgH_t h )
{
  return kOkDcRC;
}

