#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmJson.h"
#include "cmThread.h"
#include "cmMidi.h"
#include "cmMidiPort.h"
#include "cmAudioPort.h"
#include "cmUdpPort.h"
#include "cmUdpNet.h"
#include "cmAudioSysMsg.h"
#include "cmAudioSys.h"

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

  cmAudioSysArgs_t ss;
  
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
  unsigned      usrDevId; // Same as index into p->map[] for this recd.
  unsigned      cfgIndex; // Index into p->midi[],p->audio[], or p->net[].
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
  cmChar_t*     labelStr;

  cmDcmApp_t*   app;
  unsigned      appCnt;

  cmDcmMidi_t*  midi;
  unsigned      midiCnt;

  cmDcmAudio_t* audio;
  unsigned      audioCnt;

  cmDcmNet_t*   net;
  unsigned      netCnt;

} cmDcmLoc_t;

typedef struct
{
  cmErr_t      err;

  cmDcmLoc_t*  loc;
  unsigned     locCnt;

  cmDcmLoc_t*  l;

} cmDcm_t;

cmDcm_t* _cmDcmHandleToPtr( cmDevCfgH_t h )
{
  cmDcm_t* p = (cmDcm_t*)h.h;
  assert( p!=NULL );
  return p;
}

void _cmDcmAppDupl( cmDcmApp_t* d, const cmDcmApp_t* s )
{
  *d = *s;
  if( d->activeFl )
  {
    unsigned i;
    d->map = cmMemAllocZ(cmDcmMap_t,s->mapCnt);
    for(i=0; i<s->mapCnt; ++i)
      d->map[i] = s->map[i];
  }
  else
  {
    d->mapCnt = 0;
    d->map    = NULL;
  }
}

void _cmDcmMidiFree( cmDcmMidi_t* r )
{
  cmMemPtrFree(&r->dcLabelStr);
  cmMemPtrFree(&r->devLabelStr);
  cmMemPtrFree(&r->portLabelStr);
}

void _cmDcmMidiDupl( cmDcmMidi_t* d, const cmDcmMidi_t* s )
{
  d->dcLabelStr   = cmMemAllocStr(s->dcLabelStr);
  d->devLabelStr  = cmMemAllocStr(s->devLabelStr);
  d->portLabelStr = cmMemAllocStr(s->portLabelStr);
  d->inputFl      = s->inputFl;
  d->devIdx       = s->devIdx;
  d->portIdx      = s->portIdx;
}

void _cmDcmAudioFree( cmDcmAudio_t* r )
{
  cmMemPtrFree(&r->dcLabelStr);
  cmMemPtrFree(&r->inDevLabelStr);
  cmMemPtrFree(&r->outDevLabelStr);
}

void _cmDcmAudioDupl( cmDcmAudio_t* d, const cmDcmAudio_t* s )
{
  d->dcLabelStr = cmMemAllocStr(s->dcLabelStr);
  d->inDevLabelStr = cmMemAllocStr(s->inDevLabelStr);
  d->outDevLabelStr = cmMemAllocStr(s->outDevLabelStr);
  d->ss             = s->ss;
}

void _cmDcmNetFree( cmDcmNet_t* r )
{
  cmMemPtrFree(&r->dcLabelStr);
  cmMemPtrFree(&r->sockAddr);
}

void _cmDcmNetDupl( cmDcmNet_t* d, const cmDcmNet_t* s )
{
  d->dcLabelStr = cmMemAllocStr(s->dcLabelStr);
  d->sockAddr   = cmMemAllocStr(s->sockAddr);
  d->portNumber = s->portNumber;
  d->netNodeId  = s->netNodeId;
}

void _cmDcmAppFree( cmDcmApp_t* r )
{
  cmMemPtrFree(&r->map);
}

cmDcRC_t  _cmDcmFreeLoc( cmDcm_t* p, cmDcmLoc_t* loc )
{
  unsigned i;

  for(i=0; loc->midi!=NULL && i<loc->midiCnt; ++i)
    _cmDcmMidiFree(loc->midi + i );
  cmMemPtrFree(&loc->midi);
  loc->midiCnt = 0;

  for(i=0; loc->audio!=NULL && i<loc->audioCnt; ++i)
    _cmDcmAudioFree(loc->audio + i );
  cmMemPtrFree(&loc->audio);
  loc->audioCnt = 0;

  for(i=0; loc->net!=NULL && i<loc->netCnt; ++i)
    _cmDcmNetFree(loc->net + i );
  cmMemPtrFree(&loc->net);
  loc->netCnt = 0;

  for(i=0; loc->app!=NULL && i<loc->appCnt; ++i)
    _cmDcmAppFree(loc->app + i );
  cmMemPtrFree(&loc->app);
  loc->appCnt = 0;

  cmMemPtrFree(&loc->labelStr);

  return kOkDcRC;  
}

cmDcRC_t _cmDcmFree( cmDcm_t* p )
{
  cmDcRC_t rc = kOkDcRC;
  unsigned i;

  for(i=0; i<p->locCnt; ++i)
    if((rc = _cmDcmFreeLoc(p,p->loc + i )) != kOkDcRC )
      return rc;

  cmMemPtrFree(&p->loc);

  return rc;
}



cmDcmMidi_t* _cmDcmMidiFind( cmDcm_t* p, const cmChar_t* dcLabelStr, bool errFl )
{
  assert( dcLabelStr != NULL );

  unsigned i;
  for(i=0; i<p->l->midiCnt; ++i)
    if(p->l->midi[i].dcLabelStr!=NULL && strcmp(p->l->midi[i].dcLabelStr,dcLabelStr)==0)
      return p->l->midi + i;

  if( errFl )
    cmErrMsg(&p->err,cmLabelNotFoundDcRC,"The MIDI cfg. record '%s' not found.",dcLabelStr);

  return NULL;
}

cmDcmAudio_t* _cmDcmAudioFind( cmDcm_t* p, const cmChar_t* dcLabelStr, bool errFl )
{
  assert( dcLabelStr != NULL );

  unsigned i;
  for(i=0; i<p->l->audioCnt; ++i)
    if(p->l->audio[i].dcLabelStr!=NULL && strcmp(p->l->audio[i].dcLabelStr,dcLabelStr)==0)
      return p->l->audio + i;

  if( errFl )
    cmErrMsg(&p->err,cmLabelNotFoundDcRC,"The audio cfg. record '%s' not found.",dcLabelStr);

  return NULL;
}

cmDcmNet_t* _cmDcmNetFind( cmDcm_t* p, const cmChar_t* dcLabelStr, bool errFl )
{
  assert( dcLabelStr != NULL );

  unsigned i;
  for(i=0; i<p->l->netCnt; ++i)
    if(p->l->net[i].dcLabelStr!=NULL && strcmp(p->l->net[i].dcLabelStr,dcLabelStr)==0)
      return p->l->net + i;

  if( errFl )
    cmErrMsg(&p->err,cmLabelNotFoundDcRC,"The net cfg. record '%s' not found.",dcLabelStr);

  return NULL;
}


cmDcmApp_t*  _cmDcmFindOrCreateApp(cmDcm_t* p, unsigned usrAppId ) 
{
  cmDcmApp_t* a;
  if( usrAppId < p->l->appCnt )
    a = p->l->app + usrAppId;
  else
  {
    p->l->appCnt = usrAppId + 1;
    p->l->app    = cmMemResizePZ(cmDcmApp_t,p->l->app,p->l->appCnt);
    a = p->l->app + usrAppId;
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
  if( usrAppId >= p->l->appCnt )
    return cmErrMsg(&p->err,kInvalidDevArgDcRC,"Invalid user app. id:%i\n",usrAppId);

  // check that the app recd is active
  if( p->l->app[usrAppId].activeFl == false )
    return cmErrMsg(&p->err,kInvalidDevArgDcRC,"The user app. with id:%i is not active.",usrAppId);

  *appRef = p->l->app + usrAppId;

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
  for(i=0; i<p->l->appCnt; ++i)
    if( p->l->app[i].activeFl )
      for(j=0; j<p->l->app[i].mapCnt; ++j)
        if( p->l->app[i].map[j].tid == tid && p->l->app[i].map[j].cfgIndex == cfgIndex )
        {
          p->l->app[i].map[j].tid = kInvalidDcmTId;
          break;
        }
}

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
          *cfgIndexRef = r - p->l->midi;
      }
      break;

    case kAudioDcmTId:
      {
        const cmDcmAudio_t* r;

        if((r = _cmDcmAudioFind(p,dcLabelStr,true)) == NULL )
          rc = cmErrLastRC(&p->err);
        else
          *cfgIndexRef = r - p->l->audio;
      }
      break;

    case kNetDcmTId:
      {
        const cmDcmNet_t* r;

        if((r = _cmDcmNetFind(p,dcLabelStr,true)) == NULL )
          rc = cmErrLastRC(&p->err);
        else
          *cfgIndexRef = r - p->l->net;
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
      _cmDcmMidiFree( p->l->midi + cfgIndex );
      break;

    case kAudioDcmTId:
      _cmDcmAudioFree( p->l->audio + cfgIndex );
      break;

    case kNetDcmTId:
      _cmDcmNetFree( p->l->net + cfgIndex );
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


cmDcRC_t cmDevCfgMgrAlloc( cmCtx_t* ctx, cmDevCfgH_t* hp )
{
  cmDcRC_t rc;
  if((rc = cmDevCfgMgrFree(hp)) != kOkDcRC )
    return rc;

  cmDcm_t* p = cmMemAllocZ(cmDcm_t,1);
  cmErrSetup(&p->err,&ctx->rpt,"DevCfgMgr");

  p->loc         = cmMemAllocZ(cmDcmLoc_t,1);
  p->locCnt      = 1;
  p->l           = p->loc;
  p->l->labelStr = cmMemAllocStr("Default");

  hp->h = p;

  if( rc != kOkDcRC )
    _cmDcmFree(p);

  return rc;
}


cmDcRC_t cmDevCfgMgrFree( cmDevCfgH_t* hp )
{
  cmDcRC_t rc = kOkDcRC;

  if(hp==NULL || cmDevCfgIsValid(*hp)==false)
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

unsigned cmDevCfgCount( cmDevCfgH_t h, cmTypeDcmId_t typeId )
{
  cmDcm_t* p        = _cmDcmHandleToPtr(h);
  unsigned n = 0;
  unsigned i;

  switch( typeId )
  {
    case kMidiDcmTId:
      for(i=0; i<p->l->midiCnt; ++i)
        if( p->l->midi[i].dcLabelStr != NULL )
          ++n;
      break;

    case kAudioDcmTId:
      for(i=0; i<p->l->audioCnt; ++i)
        if( p->l->audio[i].dcLabelStr != NULL )
          ++n;
      break;

    case kNetDcmTId:
      for(i=0; i<p->l->netCnt; ++i)
        if( p->l->net[i].dcLabelStr != NULL )
          ++n;
      break;

    default:
      assert(0);
      break;
  }

  return n;
}

const cmChar_t* cmDevCfgLabel( cmDevCfgH_t h, cmTypeDcmId_t typeId, unsigned index )
{
  cmDcm_t* p        = _cmDcmHandleToPtr(h);
  int j = -1;
  unsigned i;
  unsigned n = 0;
  const cmChar_t* s;

  switch( typeId )
  {
    case kMidiDcmTId:
      n = p->l->midiCnt;
      for(i=0; i<n; ++i)
        if( (s = p->l->midi[i].dcLabelStr) != NULL )
          if(++j == index )
            break;

      break;

    case kAudioDcmTId:
      n = p->l->audioCnt;
      for(i=0; i<n; ++i)
        if( (s = p->l->audio[i].dcLabelStr) != NULL )
          if(++j == index )
            break;
      break;

    case kNetDcmTId:
      n = p->l->netCnt;
      for(i=0; i<n; ++i)
        if( (s = p->l->net[i].dcLabelStr) != NULL )
          if(++j == index )
            break;
      break;
    default:
      assert(0);
      break;
  }

  if( i == n )
    return NULL;

  return s;
}

unsigned cmDevCfgLabelToIndex( cmDevCfgH_t h, cmTypeDcmId_t tid, const cmChar_t* dcLabelStr )
{
  cmDcm_t* p        = _cmDcmHandleToPtr(h);
  unsigned cfgIdx = cmInvalidIdx;

  if( _cmDcmFindCfgIndex( p, tid, dcLabelStr, &cfgIdx ) != kOkDcRC )
    return cmInvalidIdx;

  return cfgIdx;
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
  cmDcRC_t    rc  = kOkDcRC;
  cmDcm_t*     p  = _cmDcmHandleToPtr(h);
  cmDcmMidi_t* r  = _cmDcmMidiFind(p,dcLabelStr,false);
  unsigned     i;

  // if 'dcLabelStr' was not already used then look for an empty MIDI record.
  if( r == NULL )
    for(i=0; i<p->l->midiCnt; ++i)
      if( p->l->midi[i].dcLabelStr == NULL )
      {
        r = p->l->midi + i;
        break;
      }
  
  // if no available cfg record exists then create one 
  if( r == NULL )
  {
    p->l->midi     = cmMemResizePZ(cmDcmMidi_t,p->l->midi,p->l->midiCnt+1);    
    r           = p->l->midi + p->l->midiCnt;
    p->l->midiCnt += 1;
  }

 
  assert( r != NULL );
 
  // verify that the device label is valid
  if((r->devIdx = cmMpDeviceNameToIndex( devNameStr )) == cmInvalidIdx )
  {
    rc = cmErrMsg(&p->err, kInvalidDevArgDcRC,"The MIDI device name '%s' is not valid.",devNameStr);
    goto errLabel;
  }

  // verify that the port label is valid
  if((r->portIdx = cmMpDevicePortNameToIndex( r->devIdx, r->inputFl ? kInMpFl : kOutMpFl, portNameStr )) == cmInvalidIdx )
  {
    rc = cmErrMsg(&p->err, kInvalidDevArgDcRC,"The MIDI port name '%s' is not valid on the device '%s'.",portNameStr,devNameStr);
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
    _cmDevCfgDeleteCfg( p, kMidiDcmTId, r - p->l->midi );

  return rc;
}


cmDcRC_t cmDevCfgMidiDevIdx(    cmDevCfgH_t h, unsigned usrAppId, unsigned usrDevId, unsigned* midiDevIdxRef, unsigned* midiPortIdxRef )
{
  cmDcRC_t   rc = kOkDcRC;
  cmDcm_t*    p = _cmDcmHandleToPtr(h);
  cmDcmMap_t* m;

  if((rc =  _cmDcmLookupAppMap(p,usrAppId,usrDevId,&m)) != kOkDcRC )
    return rc;
                                                
  cmDcmMidi_t* r = p->l->midi + m->cfgIndex;

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
  cmDcRC_t     rc  = kOkDcRC;
  cmDcm_t*      p  = _cmDcmHandleToPtr(h);
  cmDcmAudio_t* r  = _cmDcmAudioFind(p,dcLabelStr,false);
  unsigned     i;

  // if 'dcLabelStr' was not already used then look for an empty audio record.
  if( r == NULL )
    for(i=0; i<p->l->audioCnt; ++i)
      if( p->l->audio[i].dcLabelStr == NULL )
      {
        r = p->l->audio + i;
        break;
      }
  
  // if no available cfg record exists then create one 
  if( r == NULL )
  {
    p->l->audio     = cmMemResizePZ(cmDcmAudio_t,p->l->audio,p->l->audioCnt+1);    
    r           = p->l->audio + p->l->audioCnt;
    p->l->audioCnt += 1;
  }

 
  assert( r != NULL );
 
  // verify that the device label is valid
  if((r->ss.inDevIdx = cmApDeviceLabelToIndex( inDevNameStr )) == cmInvalidIdx )
  {
    rc = cmErrMsg(&p->err, kInvalidDevArgDcRC,"The input audio device name '%s' is not valid.",inDevNameStr);
    goto errLabel;
  }

  // verify that the device label is valid
  if((r->ss.outDevIdx = cmApDeviceLabelToIndex( outDevNameStr )) == cmInvalidIdx )
  {
    rc = cmErrMsg(&p->err, kInvalidDevArgDcRC,"The output audio device name '%s' is not valid.",outDevNameStr);
    goto errLabel;
  }

  // if this cfg recd was not previously active then assign a cfg label
  if( r->dcLabelStr == NULL )
    r->dcLabelStr  = cmMemAllocStr(dcLabelStr);

  // fill in the cfg recd
  r->inDevLabelStr        = cmMemResizeStr(r->inDevLabelStr,inDevNameStr);
  r->outDevLabelStr       = cmMemResizeStr(r->outDevLabelStr,outDevNameStr);
  r->ss.rpt               = p->err.rpt;
  r->ss.syncInputFl       = syncInputFl;
  r->ss.msgQueueByteCnt   = msgQueueByteCnt;
  r->ss.devFramesPerCycle = devFramesPerCycle;
  r->ss.dspFramesPerCycle = dspFramesPerCycle;
  r->ss.audioBufCnt       = audioBufCnt;
  r->ss.srate             = srate;

 errLabel:
  // on error delete the cfg record and any maps depending on it
  if( rc != kOkDcRC && r != NULL )
    _cmDevCfgDeleteCfg( p, kAudioDcmTId, r - p->l->audio );

  return rc;
}


const struct cmAudioSysArgs_str* cmDevCfgAudioSysArgs( cmDevCfgH_t h, unsigned usrAppId, unsigned usrDevId )
{
  cmDcRC_t   rc = kOkDcRC;
  cmDcm_t*    p = _cmDcmHandleToPtr(h);
  cmDcmMap_t* m;

  if((rc =  _cmDcmLookupAppMap(p,usrAppId,usrDevId,&m)) != kOkDcRC )
    return NULL;
                                                
  cmDcmAudio_t* r = p->l->audio + m->cfgIndex;

  assert(r->dcLabelStr != NULL );
  
  return &r->ss;
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


// Loc Management Functions:
unsigned        cmDevCfgLocCount( cmDevCfgH_t h )
{
  cmDcm_t*    p = _cmDcmHandleToPtr(h);
  unsigned i;
  unsigned n = 0;
  for(i=0; i<p->locCnt; ++i)
    if( p->loc[i].labelStr != NULL )
      ++n;

  return n;
}

const cmChar_t* cmDevCfgLocLabel( cmDevCfgH_t h, unsigned locIdx )
{
  cmDcm_t*    p = _cmDcmHandleToPtr(h);

  unsigned i;
  int j = -1;
  for(i=0; j<locIdx && i<p->locCnt; ++i)
    if( p->loc[i].labelStr != NULL )
      ++j;

  if( i == p->locCnt )
    return NULL;

  return p->loc[i].labelStr;
}

cmDcmLoc_t* _cmDcmFindLoc( cmDcm_t* p, const cmChar_t* label )
{
  unsigned i=0;
  for(; i<p->locCnt; ++i)
    if( strcmp(p->loc[i].labelStr,label)==0 )
      return p->loc + i;
  return NULL;
}

cmDcmLoc_t* _cmDcmNewLoc( cmDcm_t* p )
{
  unsigned i;
  // find a deleted location record
  for(i=0; i<p->locCnt; ++i)
    if( p->loc[i].labelStr == NULL )
      return p->loc + i;

  // no deleted location records exist so append one to p->loc[].

  unsigned cli = p->l - p->loc; // store the cur loc recd idx
  p->locCnt += 1;
  p->loc     = cmMemResizeZ(cmDcmLoc_t,p->loc,p->locCnt);
  p->l       = p->loc + cli; // restore the cur loc recd ptr

  return p->loc + p->locCnt - 1;
}

// Duplicate *sl and return a ptr to the new loc recd.
cmDcmLoc_t*  _cmDcmDuplLoc( cmDcm_t* p, const cmDcmLoc_t* sl, const cmChar_t* label )
{
  unsigned i;

  cmDcmLoc_t* l = _cmDcmNewLoc(p);

  l->labelStr = cmMemAllocStr(label);

  l->appCnt   = sl->appCnt;
  l->app      = cmMemAllocZ(cmDcmApp_t,l->appCnt);
  for(i=0; i<l->appCnt; ++i)
    _cmDcmAppDupl(l->app +  i, sl->app + i );

  l->midiCnt  = sl->midiCnt;
  l->midi     = cmMemAllocZ(cmDcmMidi_t,l->midiCnt);
  for(i=0; i<l->midiCnt; ++i)
    _cmDcmMidiDupl(l->midi + i, sl->midi + i );

  l->audioCnt = sl->audioCnt;
  l->audio    = cmMemAllocZ(cmDcmAudio_t,l->audioCnt);
  for(i=0; i<l->audioCnt; ++i)
    _cmDcmAudioDupl(l->audio + i, sl->audio + i );

  l->netCnt   = sl->netCnt;
  l->net      = cmMemAllocZ(cmDcmNet_t,l->netCnt);
  for(i=0; i<l->netCnt; ++i)
    _cmDcmNetDupl(l->net + i, sl->net + i );  

  return l;
}

cmDcRC_t        cmDevCfgLocStore( cmDevCfgH_t h, const cmChar_t* locLabelStr )
{
  cmDcRC_t    rc= kOkDcRC;
  cmDcm_t*    p = _cmDcmHandleToPtr(h);

  if( locLabelStr==NULL || strlen(locLabelStr)==0)
    return cmErrMsg(&p->err,kEmptyLabelDcRC,"The location label was empty or NULL.");

  assert(p->l != NULL );

  // if the location name is the same as the current location name ...
  if( strcmp(locLabelStr,p->l->labelStr)==0 )
    return rc;  // ... there is noting to do

  // get a ptr (if it exists) to the location already named 'locLabelStr'.
  cmDcmLoc_t* sl = _cmDcmFindLoc(p,locLabelStr);

  // duplicate the current location
  cmDcmLoc_t* dl = _cmDcmDuplLoc(p, p->l, locLabelStr );

  // make the new location the current location
  p->l = dl;

  // if loc with the same name already existed then delete it
  if(sl != NULL )
    _cmDcmFreeLoc(p,sl);

  return rc;
}

cmDcRC_t        cmDevCfgLocRecall( cmDevCfgH_t h, const cmChar_t* locLabelStr )
{
  cmDcRC_t    rc = kOkDcRC;
  cmDcm_t*    p  = _cmDcmHandleToPtr(h);
  cmDcmLoc_t* loc;

  if((loc = _cmDcmFindLoc(p,locLabelStr)) == NULL )
    return cmErrMsg(&p->err,kLocNotFoundDcRC,"The location '%s' could not be found.",cmStringNullGuard(locLabelStr));

  p->l = loc;

  return rc;
}

cmDcRC_t        cmDevCfgLocDelete( cmDevCfgH_t h, const cmChar_t* locLabelStr )
{
  cmDcRC_t    rc = kOkDcRC;
  cmDcm_t*    p  = _cmDcmHandleToPtr(h);
  cmDcmLoc_t* loc;

  // find the loc to delete
  if((loc = _cmDcmFindLoc(p,locLabelStr)) == NULL )
    return cmErrMsg(&p->err,kLocNotFoundDcRC,"The location '%s' could not be found.",cmStringNullGuard(locLabelStr));

  // delete the requested loc. recd
  _cmDcmFreeLoc(p,loc);
  
  // if the current location was deleted
  if( loc == p->l )
  {
    unsigned i;
    unsigned cli = (p->l - p->loc) + 1;
    for(i=cli; i<p->locCnt; ++i)
      if( p->loc[i].labelStr != NULL )
      {
        p->l = p->loc + i;
        break;
      }
    
    if( i==p->locCnt )
      for(i=0; i<cli; ++i)
        if( p->loc[i].labelStr != NULL )
        {
          p->l = p->loc + i;
          break;
        }

    // if everything was deleted
    if( i==cli )
    {
      p->l = p->loc;
      p->l->labelStr = cmMemAllocStr("Default");
    }
    
  }
    

  return rc;
}

unsigned        cmDevCfgLocIndex(  cmDevCfgH_t h )
{
  cmDcm_t* p = _cmDcmHandleToPtr(h);
  unsigned i;
  int j = -1;
  for(i=0; i<p->locCnt; ++i)
  {
    if( p->loc[i].labelStr != NULL )
      ++j;

    if( p->loc + i == p->l )
      return j;
    
  }

  assert(0);
  return cmInvalidIdx;
}

cmDcRC_t cmDevCfgWrite( cmDevCfgH_t h )
{
  return kOkDcRC;
}

