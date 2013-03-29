#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmFileSys.h"
#include "cmJson.h"
#include "cmText.h"
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


typedef struct cmDcmCfg_str
{
  cmChar_t*     dcLabelStr;   // the cfg label 
  unsigned      cfgId;        // unique among all cfg's assigned to a loc
  cmTypeDcmId_t typeId;       // the cfg type id (e.g. midi, audio, net, ...)
  cmChar_t*     descStr;      // summary description string 

  // NOTE: any fields added to this structure, type generic (above) 
  // or type specific (below), must be explicitely duplicated in cmDevCfgLocStore().

  union
  {
    cmDcmMidi_t  m;
    cmDcmAudio_t a;
    cmDcmNet_t   n;
  } u;

  struct cmDcmCfg_str* next;
  struct cmDcmCfg_str* prev;
} cmDcmCfg_t;

typedef struct
{
  cmTypeDcmId_t tid;      // Type Id for this map or tInvalidDcmTId if the record is not active.
  unsigned      cfgId;    // cfgId of the cfg recd assoc'd with this map
  cmDcmCfg_t*   cfg;      // pointer to the cfg recd assoc'd with this map (cfg->cfgId == cmDcmMap_t.cfgId)
} cmDcmMap_t;

typedef struct
{
  bool        activeFl;
  cmDcmMap_t* map;
  unsigned    mapCnt;
} cmDcmApp_t;

typedef struct cmDcmLoc_str
{
  cmChar_t*            labelStr;

  cmDcmApp_t*          app;
  unsigned             appCnt;

  cmDcmCfg_t*          cfg;

  struct cmDcmLoc_str* next;
  struct cmDcmLoc_str* prev;

} cmDcmLoc_t;

typedef struct
{
  cmErr_t     err;
  cmCtx_t*    ctx;
  cmDcmLoc_t* loc;
  cmDcmLoc_t* clp;
  cmChar_t*   fn;
} cmDcm_t;


cmDcm_t* _cmDcmHandleToPtr( cmDevCfgH_t h )
{
  cmDcm_t* p = (cmDcm_t*)h.h;
  assert( p!=NULL );
  return p;
}

const cmChar_t* _cmDcmTrimLabel( cmDcm_t*p, const cmChar_t* s, const cmChar_t* title )
{
  if( s == NULL )
  {
    cmErrMsg(&p->err,kBlankLabelDcRC,"A blank '%s' label was encountered.",title);
    return NULL;
  }

  while( *s && isspace(*s) )
    ++s;

  if( strlen(s) == 0 )
  {
    cmErrMsg(&p->err,kBlankLabelDcRC,"An empty '%s' label was encountered.",title);
    return NULL;
  }

  return s;
}

void _cmDcmFreeMidi( cmDcmMidi_t* r )
{
  cmMemFree(r->devLabelStr);
  cmMemFree(r->portLabelStr);
}

void _cmDcmDuplMidi( cmDcmMidi_t* d, const cmDcmMidi_t* s )
{
  d->devLabelStr  = cmMemAllocStr(s->devLabelStr);
  d->portLabelStr = cmMemAllocStr(s->portLabelStr);
  d->inputFl      = s->inputFl;
  d->devIdx       = s->devIdx;
  d->portIdx      = s->portIdx;
}

void _cmDcmFreeAudio( cmDcmAudio_t* r )
{
  cmMemFree(r->inDevLabelStr);
  cmMemFree(r->outDevLabelStr);
}

void _cmDcmDuplAudio( cmDcmAudio_t* d, const cmDcmAudio_t* s )
{
  d->inDevLabelStr  = cmMemAllocStr(s->inDevLabelStr);
  d->outDevLabelStr = cmMemAllocStr(s->outDevLabelStr);
  d->audioSysArgs   = s->audioSysArgs;
}

void _cmDcmFreeNet( cmDcmNet_t* r )
{
  cmMemFree(r->sockAddr);
}

void _cmDcmDuplNet( cmDcmNet_t* d, const cmDcmNet_t* s )
{
  d->sockAddr   = cmMemAllocStr(s->sockAddr);
  d->portNumber = s->portNumber;
}


void _cmDcmFreeCfg( cmDcm_t* p, cmDcmLoc_t* lp, cmDcmCfg_t* cp )
{
  // unlink the cfg recd
  if( cp->prev == NULL )
    lp->cfg = cp->next;
  else
    cp->prev->next = cp->next;
  
  if( cp->next != NULL )
    cp->next->prev = cp->prev;

  cmMemFree(cp->dcLabelStr);
  cmMemFree(cp->descStr);

  switch( cp->typeId )
  {
    case kMidiDcmTId:  _cmDcmFreeMidi(&cp->u.m); break;
    case kAudioDcmTId: _cmDcmFreeAudio(&cp->u.a); break;
    case kNetDcmTId:   _cmDcmFreeNet(&cp->u.n); break;
    default:
      assert(0);
      break;
  }

  cmMemFree(cp);
}

void _cmDcmFreeLoc( cmDcm_t* p, cmDcmLoc_t* lp )
{
  unsigned i;

  // unlink the loc recd
  if( lp->prev == NULL )
    p->loc = lp->next;
  else
    lp->prev->next = lp->next;

  if( lp->next != NULL )
    lp->next->prev = lp->prev;

  
  // free each app
  for(i=0; i<lp->appCnt; ++i)
    cmMemFree(lp->app[i].map);

  cmMemFree(lp->app);
  cmMemFree(lp->labelStr);
  
  while( lp->cfg != NULL )
    _cmDcmFreeCfg(p,lp,lp->cfg);

  cmMemFree(lp);
}

void _cmDcmFreeAllLocs( cmDcm_t* p )
{
  while( p->loc != NULL )
    _cmDcmFreeLoc(p,p->loc);
  
  p->clp    = NULL;
}

cmDcRC_t _cmDcmFree( cmDcm_t* p )
{
  cmDcRC_t rc = kOkDcRC;

  _cmDcmFreeAllLocs(p);
  cmMemFree(p->fn);

  cmMemFree(p);
  return rc;
}

cmDcmLoc_t* _cmDcmFindLoc( cmDcm_t* p, const cmChar_t* labelStr )
{
  cmDcmLoc_t* lp = p->loc;
  for(; lp!=NULL; lp=lp->next)
    if(strcmp(lp->labelStr,labelStr)==0)
      return lp;
  return NULL;
}

cmDcRC_t _cmDcmNewLoc( cmDcm_t* p, const cmChar_t* labelStr, cmDcmLoc_t**  locRef)
{
  
  cmDcmLoc_t* lp;
  
  assert(locRef != NULL);
  *locRef = NULL;

  // verify and condition the label
  if((labelStr = _cmDcmTrimLabel(p,labelStr,"location")) == NULL )
    return cmErrLastRC(&p->err);

  // verify that the label is not already in use
  if((lp = _cmDcmFindLoc(p,labelStr)) != NULL )
    return cmErrMsg(&p->err,kDuplLabelDcRC,"The location label '%s' is already in use.",labelStr);

  // create a new loc recd
  lp  = cmMemAllocZ(cmDcmLoc_t,1);
  lp->labelStr = cmMemAllocStr(labelStr);

  // if the cur loc is not current set - then prepend loc
  if( p->clp == NULL )
  {
    lp->prev = NULL;
    lp->next = p->loc;
    if( p->loc == NULL )
      p->loc = lp;
    else
      p->loc->prev = lp;    
  }
  else // otherwise insert loc after the cur loc
  {
    lp->prev = p->clp;
    lp->next = p->clp->next;
    if( p->clp->next != NULL )
      p->clp->next->prev = lp;
    p->clp->next = lp;
  }

  // make the new loc the current loc
  p->clp = lp;

  *locRef = lp;

  return kOkDcRC;
   
}

cmDcRC_t _cmDevCfgRead( cmDcm_t* p, cmJsonH_t jsH, const cmJsonNode_t* rootObjPtr );

cmDcRC_t _cmDevCfgReadFile( cmDcm_t* p, const cmChar_t* fn )
{
  cmDcRC_t rc = kOkDcRC;

  cmJsonH_t jsH = cmJsonNullHandle;

  // initialize the JSON tree from the preferences file
  if( cmJsonInitializeFromFile( &jsH, fn, p->ctx ) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailDcRC,"JSON initialization from '%s' failed.",cmStringNullGuard(fn));
    goto errLabel;
  }

  if((rc = _cmDevCfgRead(p, jsH, cmJsonRoot(jsH))) != kOkJsRC )
    goto errLabel;

 errLabel:
  if( cmJsonFinalize(&jsH) != kOkJsRC )
    cmErrMsg(&p->err,kJsonFailDcRC,"JSON finalization failed following dev cfg read.");

  return rc;

}

cmDcRC_t cmDevCfgAlloc( cmCtx_t* c, cmDevCfgH_t* hp, const cmChar_t* fn )
{
  cmDcRC_t rc;
  cmDcmLoc_t* lp;

  if((rc = cmDevCfgFree(hp)) != kOkDcRC )
    return rc;

  cmDcm_t* p = cmMemAllocZ(cmDcm_t,1);

  cmErrSetup(&p->err,&c->rpt,"cmDevCfg");

  p->ctx = c;

  if( fn != NULL )
  {
    p->fn = cmMemAllocStr(fn);
    
    if( cmFsIsFile(fn) )
    {
      // if the file read fails then reset and go with the default empty setup.
      if(_cmDevCfgReadFile(p,fn) != kOkDcRC )
        _cmDcmFreeAllLocs(p);
    }
  }

  // if the location array is empty then create a default location
  if( p->loc == NULL )
  {
    if((rc = _cmDcmNewLoc(p,"Default", &lp)) != kOkDcRC )
      goto errLabel;
  }

  hp->h = p;

 errLabel:
  if( rc != kOkDcRC )
    _cmDcmFree(p);

  return rc;
}

cmDcRC_t cmDevCfgFree( cmDevCfgH_t* hp )
{
  cmDcRC_t rc = kOkDcRC;
  if( hp == NULL || cmDevCfgIsValid(*hp)==false )
    return rc;

  cmDcm_t* p = _cmDcmHandleToPtr(*hp);

  if((rc = _cmDcmFree(p)) != kOkDcRC )
    return rc;

  hp->h = NULL;

  return rc;
}

bool cmDevCfgIsValid( cmDevCfgH_t h )
{ return h.h != NULL; }

unsigned cmDevCfgCount( cmDevCfgH_t h, cmTypeDcmId_t typeId )
{
  unsigned n=0;
  cmDcm_t* p = _cmDcmHandleToPtr(h);

  assert( p->clp != NULL);
  const cmDcmCfg_t* cp = p->clp->cfg;
  for(; cp!= NULL; cp=cp->next)
    if( cp->typeId == typeId )
      ++n;

  return n;
}

cmDcmCfg_t* _cmDevCfgIndexToPtr( cmDcm_t* p, cmTypeDcmId_t typeId, unsigned index, bool errFl )
{
  unsigned n=0;
  assert( p->clp != NULL);
  cmDcmCfg_t* cp = p->clp->cfg;
  for(; cp!= NULL; cp=cp->next)
    if( cp->typeId == typeId )
    {
      if( n == index )
        return cp;
      ++n;
    }

  if( errFl )
    cmErrMsg(&p->err,kInvalidCfgIdxDcRC,"The cfg index %i is not valid in location '%s'.",cmStringNullGuard(p->clp->labelStr));

  return NULL;
}

const cmChar_t* cmDevCfgLabel( cmDevCfgH_t h, cmTypeDcmId_t typeId, unsigned index )
{
  cmDcm_t*    p = _cmDcmHandleToPtr(h);
  cmDcmCfg_t* cp;
  if((cp = _cmDevCfgIndexToPtr(p,typeId,index,false)) == NULL )
    return NULL;

  return cp->dcLabelStr;  
}

const cmChar_t* cmDevCfgDesc( cmDevCfgH_t h, cmTypeDcmId_t typeId, unsigned index )
{
  cmDcm_t*    p = _cmDcmHandleToPtr(h);
  cmDcmCfg_t* cp;
  if((cp = _cmDevCfgIndexToPtr(p,typeId,index,false)) == NULL )
    return NULL;

  return cp->descStr;    
}

cmDcmCfg_t* _cmDcmCfgLabelToPtr( cmDcm_t* p, cmTypeDcmId_t typeId, const cmChar_t* label, bool errFl )
{
  assert( p->clp != NULL);
  cmDcmCfg_t* cp = p->clp->cfg;
  for(; cp!= NULL; cp=cp->next)
    if( cp->typeId == typeId )
    {
      if( strcmp(cp->dcLabelStr,label)==0 )
        return cp;
    }

  if( errFl )
    cmErrMsg(&p->err,kLabelNotFoundDcRC, "The cfg label '%s' was not found.",cmStringNullGuard(label));

  return NULL;
}

unsigned cmDevCfgLabelToIndex( cmDevCfgH_t h, cmTypeDcmId_t typeId, const cmChar_t* label )
{
  unsigned n = 0;
  cmDcm_t* p = _cmDcmHandleToPtr(h);
  assert( p->clp != NULL);
  const cmDcmCfg_t* cp = p->clp->cfg;
  for(; cp!= NULL; cp=cp->next)
    if( cp->typeId == typeId )
    {
      if( strcmp(cp->dcLabelStr,label)==0 )
        break;
      ++n;
    }

  return n;
}

cmDcmCfg_t*  _cmDcmFindOrCreateCfg( cmDcm_t* p, cmTypeDcmId_t typeId, const cmChar_t* dcLabelStr )
{
  assert( p->clp != NULL );
  cmDcmCfg_t* cp;

  // validate the label
  if( (dcLabelStr=_cmDcmTrimLabel(p,dcLabelStr,"cfg")) == NULL )
    return NULL;

  // if the cfg recd already exists then return it
  if((cp = _cmDcmCfgLabelToPtr(p, typeId, dcLabelStr, false )) != NULL )
    return cp;
    
  unsigned newCfgId = 0;
  
  // use ep to track the end of the list
  cmDcmCfg_t* ep = NULL; 

  // find the max cfgId used by this loc
  cp = p->clp->cfg;
  for(; cp!=NULL; cp=cp->next)
  {
    if( cp->cfgId > newCfgId )
      newCfgId = cp->cfgId;
    ep = cp;
  }

  // add one to the max cfgId to create a unique cfgId
  newCfgId += 1;
  
  // allocate a new cfg recd
  cp = cmMemAllocZ(cmDcmCfg_t,1);
  cp->dcLabelStr = cmMemAllocStr(dcLabelStr);
  cp->cfgId      = newCfgId;
  cp->typeId     = typeId;

  // link to the end of the loc's cfg list
  if( ep == NULL )
    p->clp->cfg = cp;
  else
  {
    ep->next = cp;
    cp->prev = ep;
  }

  return cp;
}

void _cmDcmDisconnectMap( cmDcmMap_t* mp )
{
  mp->tid   = kInvalidDcmTId;
  mp->cfgId = cmInvalidId;
  mp->cfg   = NULL;
}
  
cmDcRC_t cmDevCfgDeleteCfg( cmDevCfgH_t h, cmTypeDcmId_t typeId, const cmChar_t* dcLabelStr )
{
  cmDcmCfg_t* cp;
  cmDcm_t*    p = _cmDcmHandleToPtr(h);
  unsigned i,j;

  // locate the cfg to delete
  if((cp = _cmDcmCfgLabelToPtr(p,typeId,dcLabelStr,true)) == NULL )
    return cmErrLastRC(&p->err);

  // for every app in the current location ...
  for(i=0; i<p->clp->appCnt; ++i)
  {
    cmDcmApp_t* ap = p->clp->app + i;

    // ... disconnect any maps to this cfg
    if( ap->activeFl && ap->map != NULL )
      for(j=0; j<ap->mapCnt; ++j)
        if( ap->map[j].cfgId == cp->cfgId )
          _cmDcmDisconnectMap(ap->map + j );
  }
   
  // unlink and release the cfg recd
  _cmDcmFreeCfg(p,p->clp,cp);

  return kOkDcRC;
}

cmDcRC_t cmDevCfgCreateMap( cmDevCfgH_t h, cmTypeDcmId_t typeId, const cmChar_t* dcLabelStr, unsigned usrAppId, unsigned usrMapId )
{
  cmDcRC_t    rc = kOkDcRC;
  cmDcm_t*    p  = _cmDcmHandleToPtr(h);
  cmDcmCfg_t* cp;

  // locate the cfg to map to
  if((cp = _cmDcmCfgLabelToPtr(p,typeId,dcLabelStr,true)) == NULL )
    return cmErrLastRC(&p->err);
  
  // if usrAppId references a new app 
  if( usrAppId >= p->clp->appCnt )
  {
    p->clp->appCnt = usrAppId + 1;
    p->clp->app    = cmMemResizePZ(cmDcmApp_t,p->clp->app,p->clp->appCnt);
  }

  cmDcmApp_t* ap = p->clp->app + usrAppId;

  // if usrMapId ref's a new map
  if( usrMapId >= ap->mapCnt )
  {
    ap->mapCnt = usrMapId+1;
    ap->map    = cmMemResizePZ(cmDcmMap_t,ap->map,ap->mapCnt);
  }

  cmDcmMap_t* mp = ap->map + usrMapId;

  mp->tid   = typeId;
  mp->cfgId = cp->cfgId;
  mp->cfg   = cp;

  return rc;
}

cmDcmMap_t* _cmDcmFindMap( cmDcm_t* p, cmTypeDcmId_t typeId, unsigned usrAppId, unsigned usrMapId, bool activeFl )
{

  if( usrAppId >= p->clp->appCnt )
  {
    cmErrMsg(&p->err,kInvalidUserAppIdRC,"The user app. id %i is out of range %i in location '%s'.",usrAppId,p->clp->appCnt,cmStringNullGuard(p->clp->labelStr));
    goto errLabel;
  }

  cmDcmApp_t* ap = p->clp->app + usrAppId;

  if( usrMapId >= ap->mapCnt )
  {
    cmErrMsg(&p->err,kInvalidUserMapIdRC,"The user map id %i is out of range %i in location '%s'.",usrMapId,ap->mapCnt,cmStringNullGuard(p->clp->labelStr));
    goto errLabel;
  }

  if( activeFl && (ap->map[usrMapId].tid == kInvalidDcmTId || ap->map[usrMapId].cfg == NULL))
  {
    cmErrMsg(&p->err,kInvalidUserMapIdRC,"The user map id %i inactive in location '%s'.",usrMapId,cmStringNullGuard(p->clp->labelStr));
    goto errLabel;
  }    

  return ap->map + usrMapId;
 
 errLabel:
  return NULL;
}

cmDcRC_t cmDevCfgDeleteMap( cmDevCfgH_t h, cmTypeDcmId_t typeId, unsigned usrAppId, unsigned usrMapId )
{
  cmDcm_t*    p  = _cmDcmHandleToPtr(h);
  cmDcmMap_t* mp;

  if((mp = _cmDcmFindMap(p,typeId,usrAppId,usrMapId,false)) == NULL )
    return cmErrLastRC(&p->err);

  _cmDcmDisconnectMap( mp );

  return kOkDcRC;
}  


cmDcRC_t cmDevCfgNameMidiPort( 
  cmDevCfgH_t     h,
  const cmChar_t* dcLabelStr, 
  const cmChar_t* devLabelStr, 
  const cmChar_t* portLabelStr, 
  bool            inputFl )
{
  cmDcm_t*    p = _cmDcmHandleToPtr(h);
  cmDcmCfg_t* cp;

  unsigned midiDevIdx;
  unsigned midiPortIdx;

  // validate the label
  if((dcLabelStr = _cmDcmTrimLabel(p,dcLabelStr,"MIDI cfg")) == NULL)
    return cmErrLastRC(&p->err);

  // verify that the device label is valid
  if((midiDevIdx = cmMpDeviceNameToIndex( devLabelStr )) == cmInvalidIdx )
    return cmErrMsg(&p->err, kInvalidArgDcRC,"The MIDI device name '%s' is not valid.",devLabelStr);

  // verify that the port label is valid
  if((midiPortIdx = cmMpDevicePortNameToIndex( midiDevIdx, inputFl ? kInMpFl : kOutMpFl, portLabelStr )) == cmInvalidIdx )
    return cmErrMsg(&p->err, kInvalidArgDcRC,"The MIDI port name '%s' is not valid on the device '%s'.",portLabelStr,devLabelStr);

  // if dcLabelStr is already in use for this location and type then update
  // the assoc'd recd otherwise create a new one.
  if((cp = _cmDcmFindOrCreateCfg(p,kMidiDcmTId, dcLabelStr)) == NULL )
    return cmErrLastRC(&p->err);

  cp->u.m.devLabelStr  = cmMemResizeStr(cp->u.m.devLabelStr,devLabelStr);
  cp->u.m.portLabelStr = cmMemResizeStr(cp->u.m.portLabelStr,portLabelStr);
  cp->u.m.inputFl      = inputFl;
  cp->u.m.devIdx       = midiDevIdx;
  cp->u.m.portIdx      = midiPortIdx;
  cp->descStr          = cmTsPrintfP(cp->descStr,"%s %s %s",inputFl?"Input":"Output",devLabelStr,portLabelStr);

  return kOkDcRC;
}

const cmDcmMidi_t* cmDevCfgMidiCfg( cmDevCfgH_t h, unsigned cfgIdx )
{
  cmDcm_t* p = _cmDcmHandleToPtr(h);
  cmDcmCfg_t* cp;

  if((cp = _cmDevCfgIndexToPtr(p, kMidiDcmTId, cfgIdx, true )) == NULL )
    return NULL;

  return &cp->u.m;
}

const cmDcmMidi_t* cmDevCfgMidiDevMap( cmDevCfgH_t h, unsigned usrAppId, unsigned usrMapId )
{
  cmDcm_t* p = _cmDcmHandleToPtr(h);
  cmDcmMap_t* mp;

  if((mp =_cmDcmFindMap(p,kMidiDcmTId, usrAppId, usrMapId, true )) == NULL )
    return NULL;

  return &mp->cfg->u.m;
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
  cmDcm_t*    p = _cmDcmHandleToPtr(h);
  cmDcmCfg_t* cp;
  unsigned    inDevIdx;
  unsigned    outDevIdx;

  // validate the label
  if((dcLabelStr = _cmDcmTrimLabel(p,dcLabelStr,"Audio cfg")) == NULL)
    return cmErrLastRC(&p->err);

  // validate the input device
  if(( inDevIdx = cmApDeviceLabelToIndex(inDevNameStr)) == cmInvalidIdx )
    return cmErrMsg(&p->err, kInvalidArgDcRC,"The input audio device name '%s' is not valid.",cmStringNullGuard(inDevNameStr));

  // validate the output device
  if(( outDevIdx = cmApDeviceLabelToIndex(outDevNameStr)) == cmInvalidIdx )
    return cmErrMsg(&p->err, kInvalidArgDcRC,"The output audio device name '%s' is not valid.",cmStringNullGuard(outDevNameStr));

  // validate the msg byte cnt
  if( msgQueueByteCnt == 0 )
    return cmErrMsg(&p->err, kInvalidArgDcRC,"The 'message queue size' must be greater than zero.");

  // validate the dev. frames per cycle
  if( devFramesPerCycle == 0 )
    return cmErrMsg(&p->err, kInvalidArgDcRC,"The 'device frames per cycle' must be greater than zero.");
  
  // validate the dsp frames per cycle
  if( dspFramesPerCycle == 0 || ((devFramesPerCycle/dspFramesPerCycle) * dspFramesPerCycle != devFramesPerCycle) )
    return cmErrMsg(&p->err, kInvalidArgDcRC,"The 'DSP frames per cycle' must be greater than zero and an integer factor of 'Device frames per cycle.'.");

  // validate the sample rate
  if( srate == 0 )
    return cmErrMsg(&p->err, kInvalidArgDcRC,"The audio sample rate must be greater than zero.");

  // if dcLabelStr is already in use for this location and type then update
  // the assoc'd recd otherwise create a new one.
  if((cp = _cmDcmFindOrCreateCfg(p,kAudioDcmTId, dcLabelStr)) == NULL )
    return cmErrLastRC(&p->err);

  unsigned inChCnt  = cmApDeviceChannelCount( inDevIdx,  true );
  unsigned outChCnt = cmApDeviceChannelCount( outDevIdx, false );

  cp->u.a.inDevLabelStr                  = cmMemAllocStr(inDevNameStr);
  cp->u.a.outDevLabelStr                 = cmMemAllocStr(outDevNameStr);
  cp->u.a.audioSysArgs.rpt               = p->err.rpt;
  cp->u.a.audioSysArgs.inDevIdx          = inDevIdx;
  cp->u.a.audioSysArgs.outDevIdx         = outDevIdx;
  cp->u.a.audioSysArgs.syncInputFl       = syncInputFl;
  cp->u.a.audioSysArgs.msgQueueByteCnt   = msgQueueByteCnt;
  cp->u.a.audioSysArgs.devFramesPerCycle = devFramesPerCycle;
  cp->u.a.audioSysArgs.dspFramesPerCycle = dspFramesPerCycle;
  cp->u.a.audioSysArgs.audioBufCnt       = audioBufCnt;
  cp->u.a.audioSysArgs.srate             = srate;
  cp->descStr = cmTsPrintfP(cp->descStr,"In: Chs:%i %s\nOut: Chs:%i %s",inChCnt,inDevNameStr,outChCnt,outDevNameStr);
  return kOkDcRC;  
}



cmDcRC_t            cmDevCfgAudioSetDefaultCfgIndex( cmDevCfgH_t h, unsigned cfgIdx )
{
  cmDcm_t*    p  = _cmDcmHandleToPtr(h);

  assert( p->clp != NULL );

  cmDcmCfg_t* cp = p->clp->cfg;
  unsigned    i;

  for(i=0; cp!=NULL; cp=cp->next)
    if( cp->typeId == kAudioDcmTId )
    {
      if( i == cfgIdx )
        cp->u.a.dfltFl = true;
      else
      {
        if( cp->u.a.dfltFl )
          cp->u.a.dfltFl = false;
      }

      ++i;
    }
  

  return kOkDcRC;
}

unsigned   cmDevCfgAudioGetDefaultCfgIndex( cmDevCfgH_t h )
{
  cmDcm_t*    p  = _cmDcmHandleToPtr(h);

  assert( p->clp != NULL );

  cmDcmCfg_t* cp = p->clp->cfg;
  unsigned    i;

  for(i=0; cp!=NULL; cp=cp->next)
    if( cp->typeId == kAudioDcmTId )
    {
      if( cp->u.a.dfltFl )
        return i;

      ++i;
    }
  return cmInvalidIdx;
}


const cmDcmAudio_t* cmDevCfgAudioCfg( cmDevCfgH_t h, unsigned cfgIdx )
{
  cmDcm_t* p = _cmDcmHandleToPtr(h);
  cmDcmCfg_t* cp;

  if((cp = _cmDevCfgIndexToPtr(p, kAudioDcmTId, cfgIdx, true )) == NULL )
    return NULL;

  return &cp->u.a;
}

const cmDcmAudio_t* cmDevCfgAudioDevMap(    cmDevCfgH_t h, unsigned usrAppId, unsigned usrMapId )
{
  cmDcm_t* p = _cmDcmHandleToPtr(h);
  cmDcmMap_t* mp;

  if((mp =_cmDcmFindMap(p,kAudioDcmTId, usrAppId, usrMapId, true )) == NULL )
    return NULL;

  return &mp->cfg->u.a;
}


cmDcRC_t cmDevCfgNameNetPort(
  cmDevCfgH_t      h,
  const cmChar_t* dcLabelStr,
  const cmChar_t* sockAddr,
  unsigned        portNumber )
{

  cmDcm_t* p = _cmDcmHandleToPtr(h);
  cmDcmCfg_t* cp;

  if( portNumber > 0xffff )
    return cmErrMsg(&p->err,kInvalidArgDcRC,"The network port number %i is invalid. The valid IP port number range is:0-0xffff.");

  // validate the label
  if((dcLabelStr = _cmDcmTrimLabel(p,dcLabelStr,"MIDI cfg")) == NULL)
    return cmErrLastRC(&p->err);

  // if dcLabelStr is already in use for this location and type then update
  // the assoc'd recd otherwise create a new one.
  if((cp = _cmDcmFindOrCreateCfg(p,kNetDcmTId, dcLabelStr)) == NULL )
    return cmErrLastRC(&p->err);

  cp->u.n.sockAddr = cmMemAllocStr(sockAddr);
  cp->u.n.portNumber = portNumber;
  cp->descStr = cmTsPrintfP(cp->descStr,"%s:%i",sockAddr,portNumber);
  

  return kOkDcRC;
}

const cmDcmNet_t* cmDevCfgNetCfg( cmDevCfgH_t h, unsigned cfgIdx )
{
  cmDcm_t* p = _cmDcmHandleToPtr(h);
  cmDcmCfg_t* cp;

  if((cp = _cmDevCfgIndexToPtr(p, kNetDcmTId, cfgIdx, true )) == NULL )
    return NULL;

  return &cp->u.n;
}

const cmDcmNet_t* cmDevCfgNetDevMap(    cmDevCfgH_t h, unsigned usrAppId, unsigned usrMapId )
{
  cmDcm_t* p = _cmDcmHandleToPtr(h);
  cmDcmMap_t* mp;

  if((mp =_cmDcmFindMap(p,kNetDcmTId, usrAppId, usrMapId, true )) == NULL )
    return NULL;

  return &mp->cfg->u.n;
}

unsigned        cmDevCfgLocCount(  cmDevCfgH_t h )
{
  unsigned n = 0;
  cmDcm_t* p = _cmDcmHandleToPtr(h);
  const cmDcmLoc_t* lp = p->loc;
  for(; lp!=NULL; lp=lp->next)
    ++n;
  return n;
}
const cmChar_t* cmDevCfgLocLabel(  cmDevCfgH_t h, unsigned locIdx )
{
  cmDcm_t* p = _cmDcmHandleToPtr(h);
  const cmDcmLoc_t* lp = p->loc;
  unsigned i;
  for(i=0; lp!=NULL; lp=lp->next,++i)
    if( i == locIdx )
      return lp->labelStr;

  assert(0);
  return NULL;
}


cmDcmLoc_t* _cmDcmLocLabelToPtr( cmDcm_t* p, const cmChar_t* locLabelStr, bool errFl)
{
  if((locLabelStr = _cmDcmTrimLabel(p,locLabelStr,"location")) == NULL )
    return NULL;

  cmDcmLoc_t* lp = p->loc;
  for(; lp!=NULL; lp=lp->next)
    if( strcmp(lp->labelStr,locLabelStr) == 0 )
      return lp;

  if( errFl )
    cmErrMsg(&p->err,kLabelNotFoundDcRC,"The location label '%s' was not found.",locLabelStr);

  return NULL;
}


cmDcRC_t cmDevCfgLocStore(  cmDevCfgH_t h, const cmChar_t* locLabelStr )
{
  cmDcRC_t  rc = kOkDcRC;
  cmDcm_t*   p = _cmDcmHandleToPtr(h);
  unsigned i,j;

  // if this location label is already in use then it has already been stored.
  if( _cmDcmLocLabelToPtr(p,locLabelStr,false) != NULL )
    return kOkDcRC;

  // store the current loc ptr
  cmDcmLoc_t* olp = p->clp;
  cmDcmLoc_t* nlp;
  
  // create a new location recd and make it current
  if((rc = _cmDcmNewLoc(p,locLabelStr,&nlp)) != kOkDcRC )
    return kOkDcRC;

  // duplicate the cfg's
  cmDcmCfg_t* ocp = olp->cfg;
  for(; ocp!=NULL; ocp=ocp->next)
  {
    cmDcmCfg_t* ncp;

    // this will always create (never find) a new cfg recd
    if((ncp = _cmDcmFindOrCreateCfg(p,ocp->typeId, ocp->dcLabelStr)) == NULL )
    {
      rc = cmErrLastRC(&p->err);
      goto errLabel;
    }

    // duplicate the desc. string
    ncp->descStr = cmMemAllocStr(ocp->descStr);
    
    switch( ncp->typeId )
    {
      case kMidiDcmTId:  _cmDcmDuplMidi(&ncp->u.m,&ocp->u.m);  break;
      case kAudioDcmTId: _cmDcmDuplAudio(&ncp->u.a,&ocp->u.a); break;
      case kNetDcmTId:   _cmDcmDuplNet(  &ncp->u.n,&ocp->u.n); break;
      default:
        assert(0);
        break;
    }  
  }
  
  // duplicate the app array
  nlp->appCnt = olp->appCnt;
  nlp->app    = cmMemAllocZ(cmDcmApp_t,nlp->appCnt);

  for(i=0; i<nlp->appCnt; ++i)
  {
    cmDcmApp_t* nap = nlp->app + i;
    cmDcmApp_t* oap = olp->app + i;

    //nap->usrAppId = oap->usrAppId;
    nap->activeFl = oap->activeFl;
    nap->mapCnt   = oap->mapCnt;
    nap->map      = cmMemAllocZ(cmDcmMap_t,nap->mapCnt);

    for(j=0; j<nap->mapCnt; ++j)
    {
      cmDcmMap_t* nmp = nap->map + j;
      cmDcmMap_t* omp = oap->map + j;

      nmp->tid   = omp->tid;
      nmp->cfgId = omp->cfgId;

      if( omp->tid != kInvalidDcmTId && omp->cfg->dcLabelStr != NULL )
        nmp->cfg   = _cmDcmCfgLabelToPtr(p,nmp->tid,omp->cfg->dcLabelStr,true);
    }
  }
  
 errLabel:
  if( rc != kOkDcRC )
    _cmDcmFreeLoc(p,nlp);

  return rc;

}

cmDcRC_t cmDevCfgLocRecall( cmDevCfgH_t h, const cmChar_t* locLabelStr )
{
  cmDcm_t*     p = _cmDcmHandleToPtr(h);
  cmDcmLoc_t* lp;

  if((lp = _cmDcmLocLabelToPtr(p,locLabelStr,true)) == NULL)
    return cmErrLastRC(&p->err);

  p->clp = lp;

  return kOkDcRC;
}
cmDcRC_t cmDevCfgLocDelete( cmDevCfgH_t h, const cmChar_t* locLabelStr )
{
  cmDcm_t*     p = _cmDcmHandleToPtr(h);
  cmDcmLoc_t* lp;

  if((lp = _cmDcmLocLabelToPtr(p,locLabelStr,true)) == NULL )
    return cmErrLastRC(&p->err);

  _cmDcmFreeLoc(p,lp);

  return kOkDcRC;
}

unsigned cmDevCfgLocCurIndex(  cmDevCfgH_t h )
{
  unsigned i;
  cmDcm_t* p = _cmDcmHandleToPtr(h);
  cmDcmLoc_t* lp = p->loc;
  for(i=0; lp!=NULL; lp=lp->next,++i)
    if( lp == p->clp )
      return i;

  assert(0);
  return cmInvalidIdx;
}

cmDcRC_t _cmDcmJsonNotFound( cmDcm_t* p, const cmChar_t* tagStr )
{ return cmErrMsg(&p->err,kJsonFailDcRC,"JSON element '%s' not found.",tagStr); }

cmDcRC_t _cmDcmJsonSyntaxErr( cmDcm_t* p, const cmChar_t* tagStr )
{ return cmErrMsg(&p->err,kJsonFailDcRC,"JSON syntax error '%s' not found.",tagStr); }

cmDcRC_t _cmDevCfgRead( cmDcm_t* p, cmJsonH_t jsH, const cmJsonNode_t* rootObjPtr )
{
  cmDcRC_t        rc          = kOkDcRC;
  const cmChar_t* errLabelPtr = NULL;
  cmJsonNode_t* cfgNp, *locArrNp;
  unsigned i,j;
  cmDevCfgH_t h;
  h.h = p;

  // clear the all locations
  _cmDcmFreeAllLocs(p);

  if((cfgNp = cmJsonFindValue(jsH, "cfg", rootObjPtr, kObjectTId )) == NULL )
    return _cmDcmJsonNotFound(p,"cfg");
  
  // get loc array
  if((locArrNp = cmJsonFindValue(jsH,"loc",cfgNp, kArrayTId )) == NULL )
    return _cmDcmJsonNotFound(p,"loc");

  // for each loc object
  for(i=0; i<cmJsonChildCount(locArrNp); ++i)
  {
    cmJsonNode_t* locObjNp, *cfgArrNp;
    const cmChar_t* locLabelStr = NULL;

    // get the loc object
    if((locObjNp = cmJsonArrayElement(locArrNp,i)) == NULL || cmJsonIsObject(locObjNp)==false )
      return _cmDcmJsonSyntaxErr(p,"loc object");

    // read the loc object fields
    if( cmJsonMemberValues(locObjNp, &errLabelPtr,  
        "label", kStringTId, &locLabelStr,
        "cfg",   kArrayTId,  &cfgArrNp,
        NULL ) != kOkJsRC )
    { return _cmDcmJsonNotFound(p,errLabelPtr); }

    // create a new location recd
    cmDcmLoc_t* locPtr = NULL;
    if((rc = _cmDcmNewLoc(p,locLabelStr,&locPtr)) != kOkDcRC )
      return cmErrMsg(&p->err,kJsonFailDcRC,"Location '%s' create failed.",cmStringNullGuard(locLabelStr));

    /*
    // read each app object
    for(j=0; j<cmJsonChildCount(appArrNp); ++j)
    {
      cmJsonNode_t* appObjNp;

      // get the app object
      if((appObjNp = cmJsonArrayElement(appArrNp,j)) == NULL || cmJsonIsObject(appObjNp)==false )
        return _cmDcmJsonSyntaxErr(p,"loc object");

      // locate the map array
      cmJsonNode_t* mapArrNp;
      if((mapArrNp = cmJsonFindValue(jsH,"map",appObjNp,kArrayTId)) == NULL )
        return _cmDcmJsonNotFound(p,"map");

      // for each map array
      for(k=0; k<cmJsonChildCount(mapArrNp); ++k)
      {
        unsigned tid,cfgId;
        cmJsonNode_t* mapObjNp;

        if((mapObjNp = cmJsonArrayElement(mapArrNp,j))==NULL || cmJsonIsObject(mapObjNp)==false)
          return _cmDcmJsonSyntaxErr(p,"cfg object");

        if( cmJsonMemberValues( mapObjNp, &errLabelPtr, 
            "tid",   kIntTId, &tid,
            "cfgId", kIntTId, &cfgId,
            NULL ) != kOkDcRC )
        { return _cmDcmJsonSyntaxErr(p,errLabelPtr); }
      }     
    }
    */

    // read each cfg object
    for(j=0; j<cmJsonChildCount(cfgArrNp); ++j)
    {
      cmJsonNode_t* cfgObjNp;
      const cmChar_t* dcLabelStr;
      const cmChar_t* descStr;
      unsigned cfgId, typeId;

      if((cfgObjNp = cmJsonArrayElement(cfgArrNp,j))==NULL || cmJsonIsObject(cfgObjNp)==false)
        return _cmDcmJsonSyntaxErr(p,"cfg object");
      
      if( cmJsonMemberValues( cfgObjNp, &errLabelPtr,
        "label",  kStringTId, &dcLabelStr,
        "cfgId",  kIntTId,    &cfgId,
        "typeId", kIntTId,    &typeId,
        "desc",   kStringTId, &descStr,
          NULL ) != kOkJsRC )
      { _cmDcmJsonSyntaxErr(p,errLabelPtr); }

      cmDcmMidi_t  m;
      cmDcmAudio_t a;
      cmDcmNet_t   n;
      
      switch( typeId )
      {
        case kMidiDcmTId:   
          if( cmJsonMemberValues( cfgObjNp, &errLabelPtr,
              "devLabelStr", kStringTId, &m.devLabelStr,
              "portLabelStr",kStringTId, &m.portLabelStr,
              "inputFl",     kBoolTId,   &m.inputFl,
              NULL) != kOkJsRC )
          {
            rc = _cmDcmJsonSyntaxErr(p,errLabelPtr);
            goto errLabel;
          }
          
          if((rc = cmDevCfgNameMidiPort(h,dcLabelStr,m.devLabelStr,m.portLabelStr,m.inputFl)) != kOkDcRC )
            goto errLabel;
          
          break;
            
        case kAudioDcmTId:
          if( cmJsonMemberValues( cfgObjNp, &errLabelPtr,
              "inDevLabelStr",     kStringTId, &a.inDevLabelStr,
              "outDevLabelStr",    kStringTId, &a.outDevLabelStr,
              "syncInputFl",       kBoolTId,   &a.audioSysArgs.syncInputFl,
              "msgQueueByteCnt",   kIntTId,    &a.audioSysArgs.msgQueueByteCnt,
              "devFramesPerCycle", kIntTId,    &a.audioSysArgs.devFramesPerCycle,
              "dspFramesPerCycle", kIntTId,    &a.audioSysArgs.dspFramesPerCycle,
              "audioBufCnt",       kIntTId,    &a.audioSysArgs.audioBufCnt,
              "srate",             kRealTId,   &a.audioSysArgs.srate,
              NULL ) != kOkJsRC )
          {
            rc = _cmDcmJsonSyntaxErr(p,errLabelPtr);
            goto errLabel;
          }

          if((rc = cmDevCfgNameAudioPort(h,dcLabelStr,a.inDevLabelStr,a.outDevLabelStr,
                a.audioSysArgs.syncInputFl,
                a.audioSysArgs.msgQueueByteCnt,
                a.audioSysArgs.devFramesPerCycle,
                a.audioSysArgs.dspFramesPerCycle,
                a.audioSysArgs.audioBufCnt,
                a.audioSysArgs.srate )) != kOkDcRC )
          {
            goto errLabel;
          }

          break;

        case kNetDcmTId:
          if( cmJsonMemberValues( cfgObjNp, &errLabelPtr,
              "sockAddr",   kStringTId, &n.sockAddr,
              "portNumber", kIntTId,    &n.portNumber,
              NULL ) != kOkJsRC )
          {
            rc = _cmDcmJsonSyntaxErr(p,errLabelPtr);
            goto errLabel;
          }

          if((rc = cmDevCfgNameNetPort(h,dcLabelStr,n.sockAddr,n.portNumber)) != kOkDcRC )
            goto errLabel;

          break;

        default:
          assert(0);
          break;
      }
    }    
  }

 errLabel:
  return kOkDcRC;
}

cmDcRC_t _cmDevCfgWrite( cmDcm_t* p, cmJsonH_t jsH, cmJsonNode_t* rootObjPtr )
{
  cmDcRC_t rc = kOkDcRC;
  const cmDcmLoc_t* lp = p->loc;
  
  cmJsonNode_t* cfgNp = cmJsonInsertPairObject(jsH, rootObjPtr, "cfg" );

  // create the loc array
  cmJsonNode_t* locArrNp = cmJsonInsertPairArray( jsH, cfgNp, "loc" );
  
  for(; lp!=NULL; lp=lp->next)
  {
    // create the 'loc' object
    cmJsonNode_t* locObjNp = cmJsonCreateObject(jsH,locArrNp);

    // set the loc label
    cmJsonInsertPairString(jsH, locObjNp, "label", lp->labelStr );

    /*
    // create the 'loc.app[]' array
    cmJsonNode_t* appArrNp = cmJsonInsertPairArray(jsH,locObjNp,"app");

    // for each app recd
    for(i=0; i<lp->appCnt; ++i)
      if( lp->app[i].activeFl )
      {
        // create the app recd
        cmJsonNode_t* appNp = cmJsonCreateObject(jsH,appArrNp );

        // create the map array
        cmJsonNode_t* mapArrNp = cmJsonInsertPairArray(jsH, appNp, "map" );

        // for each map recd
        for(j=0; j<lp->app[i].mapCnt; ++j)
        {
          cmJsonNode_t* mapNp = cmJsonCreateObject(jsH,mapArrNp);

          cmJsonInsertPairs(jsH, mapNp, 
            "tid",   kIntTId, lp->app[i].map[j].tid,
            "cfgId", kIntTId, lp->app[i].map[j].cfgId,
            NULL );
        }
      }
    */

    // create the 'loc.cfg[]' array
    cmJsonNode_t* cfgArrNp = cmJsonInsertPairArray(jsH,locObjNp,"cfg");

    // for each cfg recd
    cmDcmCfg_t* cp = lp->cfg;
    for(; cp!=NULL; cp=cp->next)
    {
      // create the cfg recd
      cmJsonNode_t* cfgObjNp = cmJsonCreateObject(jsH,cfgArrNp);

      // fill the cfg recd
      cmJsonInsertPairs(jsH, cfgObjNp,
        "label",  kStringTId, cp->dcLabelStr,
        "cfgId",  kIntTId,    cp->cfgId,
        "typeId", kIntTId,    cp->typeId,
        "desc",   kStringTId, cp->descStr,
        NULL );

      switch( cp->typeId )
      {
        case kMidiDcmTId:    
          cmJsonInsertPairs(jsH, cfgObjNp, 
            "devLabelStr", kStringTId, cp->u.m.devLabelStr,
            "portLabelStr",kStringTId, cp->u.m.portLabelStr,
            "inputFl",     kBoolTId,   cp->u.m.inputFl,
            NULL );
            break;

        case kAudioDcmTId:   
          cmJsonInsertPairs(jsH, cfgObjNp,
            "inDevLabelStr",     kStringTId, cp->u.a.inDevLabelStr,
            "outDevLabelStr",    kStringTId, cp->u.a.outDevLabelStr,
            "syncInputFl",       kBoolTId,   cp->u.a.audioSysArgs.syncInputFl,
            "msgQueueByteCnt",   kIntTId,    cp->u.a.audioSysArgs.msgQueueByteCnt,
            "devFramesPerCycle", kIntTId,    cp->u.a.audioSysArgs.devFramesPerCycle,
            "dspFramesPerCycle", kIntTId,    cp->u.a.audioSysArgs.dspFramesPerCycle,
            "audioBufCnt",       kIntTId,    cp->u.a.audioSysArgs.audioBufCnt,
            "srate",             kRealTId,   cp->u.a.audioSysArgs.srate,
            NULL );
          break;

        case kNetDcmTId:
          cmJsonInsertPairs(jsH, cfgObjNp,
            "sockAddr",  kStringTId, cp->u.n.sockAddr,
            "portNumber",kIntTId,    cp->u.n.portNumber,
            NULL );
          break;

        default:
          assert(0);
          break;
      }
    }
  }
  return rc;
}

cmDcRC_t cmDevCfgWrite( cmDevCfgH_t h, const cmChar_t* fn )
{
  cmDcRC_t rc = kOkDcRC;
  cmDcm_t* p = _cmDcmHandleToPtr(h);

  cmJsonH_t jsH = cmJsonNullHandle;

  if( fn == NULL )
    fn = p->fn;

  // validate the filename
  if( fn == NULL || strlen(fn)==0 )
    return cmErrMsg(&p->err,kInvalidFnDcRC,"No output file name was provided.");

  // create a json object
  if( cmJsonInitialize( &jsH, p->ctx ) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailDcRC,"An empty JSON tree could not be created.");
    goto errLabel;
  }

  // insert a wrapper object as the root
  if( cmJsonCreateObject( jsH, NULL ) == NULL )
  {
    rc = cmErrMsg(&p->err,kJsonFailDcRC,"The JSON root object could not be created.");
    goto errLabel;
  }

  // fill the JSON tree
  if((rc = _cmDevCfgWrite(p,jsH,cmJsonRoot(jsH))) != kOkDcRC )
    goto errLabel;

  // write the output file
  if( cmJsonWrite(jsH, cmJsonRoot(jsH), fn ) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailDcRC,"The JSON file write failed on '%s'.",cmStringNullGuard(fn));
    goto errLabel;
  }
  
  
 errLabel:
  if( cmJsonFinalize(&jsH) != kOkJsRC )
    cmErrMsg(&p->err,kJsonFailDcRC,"JSON tree finalization failed.");

  return rc;
}
