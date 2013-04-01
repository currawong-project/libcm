#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmJson.h"
#include "cmThread.h"
#include "cmUdpPort.h"
#include "cmUdpNet.h"
#include "cmAudioSysMsg.h"
#include "cmAudioSys.h"
#include "cmUiDrvr.h"
#include "cmUi.h"
#include "cmUiAudioSysMstr.h"

enum
{
  kLabelAmId,
  kInSliderAmId,
  kInMeterAmId,
  kInToneAmId,
  kInPassAmId,
  kInMuteAmId,
  kOutSliderAmId,
  kOutMeterAmId,
  kOutToneAmId,
  kOutPassAmId,
  kOutMuteAmId,
  kAmCnt
};


enum
{
  kMinDb =  24,
  kMaxDb = -24,
  kMtrMin  = 0,
  kMtrMax  = 100
};

typedef struct cmAmPanel_str
{
  unsigned              asSubIdx;

  unsigned              panelId;

  unsigned              updateId;
  unsigned              wakeupId;
  unsigned              msgCbId;
  unsigned              audioCbId;

  unsigned              updateCnt;
  unsigned              wakeupCnt;
  unsigned              msgCbCnt;
  unsigned              audioCbCnt;

  unsigned              baseOutId;
  unsigned              iDevIdx;
  unsigned              oDevIdx;
  unsigned              iChCnt;
  unsigned              oChCnt;
  
  unsigned              a[ kAmCnt ];


  struct cmAmPanel_str* link;
} cmAmPanel_t;

typedef struct
{
  cmErr_t       err;
  unsigned      appId;
  cmUiH_t       uiH;
  cmAudioSysH_t asH;
  unsigned      nextId;
  cmAmPanel_t*  list;
} cmAm_t;

cmUiASMstrH_t cmUiASMstrNullHandle = cmSTATIC_NULL_HANDLE;

cmAm_t* _cmUiAmHandleToPtr( cmUiASMstrH_t h )
{
  cmAm_t* p = (cmAm_t*)h.h;
  assert( p!=NULL);
  return p;
}

cmAmRC_t _cmUiAmFreePanels( cmAm_t* p, bool callDriverFl )
{
  cmAmRC_t       rc = kOkAmRC;
  cmAmPanel_t* pp = p->list;
  
  while( pp != NULL )
  {
    cmAmPanel_t* np = pp->link;

    unsigned panelId = pp->panelId;

    if( callDriverFl )
      if( cmUiClearPanel( p->uiH, panelId ) != kOkUiRC )
      {
        rc = cmErrMsg(&p->err,kUiFailAmRC,"The panel %i clear failed.",panelId);
        goto errLabel;
      }

    cmMemFree(pp);

    pp = np;
  }

  p->nextId = 0;
  p->list   = NULL;
 
 errLabel:
  return rc;
}


cmAmRC_t _cmUiAmFree( cmAm_t* p )
{
  _cmUiAmFreePanels(p,false);
  cmMemFree(p);
  return kOkAmRC;
}

cmAmRC_t cmUiAudioSysMstrAlloc( cmCtx_t* ctx, cmUiASMstrH_t* hp, cmUiH_t uiH, cmAudioSysH_t asH, unsigned appId )
{
  cmAmRC_t rc = kOkAmRC;
  
  if((rc = cmUiAudioSysMstrFree(hp)) != kOkAmRC )
    return rc;

  cmAm_t* p = cmMemAllocZ(cmAm_t,1);
  cmErrSetup(&p->err,&ctx->rpt,"Audio System Master UI");
  p->appId  = appId;
  p->uiH    = uiH;
  p->asH    = asH;
  p->nextId = 0;
  hp->h     = p;
  return rc;
}

cmAmRC_t cmUiAudioSysMstrFree(  cmUiASMstrH_t* hp )
{
  cmAmRC_t rc = kOkAmRC;

  if(hp==NULL || cmUiAudioSysMstrIsValid(*hp)==false )
    return kOkAmRC;

  cmAm_t* p = _cmUiAmHandleToPtr(*hp);

  if((rc = _cmUiAmFree(p)) != kOkAmRC )
    return rc;

  hp->h = NULL;

  return rc;
}

bool     cmUiAudioSysMstrIsValid( cmUiASMstrH_t h )
{ return h.h != NULL; }


cmAmRC_t cmUiAudioSysMstrInitialize( cmUiASMstrH_t amH, const cmAudioSysSsInitMsg_t* m, const cmChar_t* inDevLabel, const cmChar_t* outDevLabel )
{
  cmAmRC_t rc  = kOkAmRC;
  cmAm_t*  p   = _cmUiAmHandleToPtr(amH);
  cmUiH_t  uiH = p->uiH;

  cmUiSetAppId( p->uiH, p->appId );

  if( m->asSubIdx == 0 )
  {
    if((rc = _cmUiAmFreePanels(p,true)) != kOkAmRC )
      return rc;
    assert(p->list == NULL );
  }

  // create the panel recd and link it to the beginning of the list
  cmAmPanel_t* pp = cmMemAllocZ(cmAmPanel_t,1);

  pp->link     = p->list;    
  p->list      = pp;
  pp->asSubIdx = m->asSubIdx;
  pp->iDevIdx  = m->inDevIdx;
  pp->oDevIdx  = m->outDevIdx;
  pp->iChCnt   = m->inChCnt;
  pp->oChCnt   = m->outChCnt;

  
  pp->panelId           = p->nextId++;  
  pp->updateId          = p->nextId++;
  pp->wakeupId          = p->nextId++;
  pp->msgCbId           = p->nextId++;
  pp->audioCbId         = p->nextId++;

  pp->a[kLabelAmId]     = p->nextId;
  pp->a[kInSliderAmId]  = p->nextId += m->inChCnt;
  pp->a[kInMeterAmId]   = p->nextId += m->inChCnt;
  pp->a[kInToneAmId]    = p->nextId += m->inChCnt;
  pp->a[kInPassAmId]    = p->nextId += m->inChCnt;
  pp->a[kInMuteAmId]    = p->nextId += m->inChCnt;

  pp->baseOutId         = p->nextId += m->inChCnt;  

  pp->a[kOutSliderAmId] = pp->baseOutId;
  pp->a[kOutMeterAmId]  = p->nextId += m->outChCnt;
  pp->a[kOutToneAmId]   = p->nextId += m->outChCnt;
  pp->a[kOutPassAmId]   = p->nextId += m->outChCnt;
  pp->a[kOutMuteAmId]   = p->nextId += m->outChCnt;
  p->nextId += m->outChCnt;

  unsigned panelId = pp->panelId;
  unsigned colW    = 50;
  unsigned ctlW    = 45;
  unsigned n       = 31;
  cmChar_t chNumStr[ n+1 ];
  int      w;

  if( cmUiCreatePanel(uiH, panelId, "Master" ) != kOkUiRC )
  {
    rc = cmErrMsg(&p->err,kUiFailAmRC,"Panel %i create failed.",panelId);
    goto errLabel;
  }

  cmUiSetFillRows( uiH, panelId, true );
  cmUiCreateProgress(uiH, panelId, pp->updateId, "Update",  0, 0, 1, 0 );
  cmUiCreateProgress(uiH, panelId, pp->wakeupId, "Wakeup",  0, 0, 1, 0 );
  cmUiCreateProgress(uiH, panelId, pp->msgCbId,  "Message", 0, 0, 1, 0 );
  cmUiCreateProgress(uiH, panelId, pp->audioCbId,"Audio",   0, 0, 1, 0 );
  cmUiSetFillRows( uiH, panelId, false );

  cmUiNewLine(     uiH, panelId );
  cmUiCreateLabel( uiH, panelId, cmInvalidId, inDevLabel, kInsideUiFl | kLeftUiFl ); 
  cmUiNewLine(     uiH, panelId );

  unsigned i;
  for(i=0; i<m->inChCnt; ++i)
  {
    snprintf(chNumStr,n,"%i",i);
    
    cmUiSetNextW(     uiH, panelId, ctlW );
    cmUiCreateLabel(  uiH, panelId, cmInvalidId, chNumStr, 0 );
    cmUiCreateVSlider(uiH, panelId, pp->a[kInSliderAmId] + i, NULL,     0, kMinDb,  kMaxDb, 0.1, 0 );
    cmUiPlaceRight(   uiH, panelId );
    cmUiCreateVMeter( uiH, panelId, pp->a[kInMeterAmId]  + i, NULL,     0, kMtrMin, kMtrMax, 0 ); 
    w = cmUiSetW(     uiH, panelId, ctlW );
    cmUiCreateCheck(  uiH, panelId, pp->a[kInToneAmId]   + i, "T",      0, false );
    cmUiCreateCheck(  uiH, panelId, pp->a[kInPassAmId]   + i, "P",      0, false );
    cmUiCreateCheck(  uiH, panelId, pp->a[kInMuteAmId]   + i, "M",      0, false );
    cmUiSetW(         uiH, panelId, w );
    cmUiSetBaseCol( uiH, panelId, 5 + (i+1)*colW);
  }

  cmUiSetBaseCol(  uiH, panelId, 0);
  cmUiNewLine(     uiH, panelId );
  cmUiCreateLabel( uiH,panelId, cmInvalidId, outDevLabel, kInsideUiFl | kLeftUiFl ); 
  cmUiNewLine(     uiH, panelId );

  for(i=0; i<m->outChCnt; ++i)
  {
    snprintf(chNumStr,n,"%i",i);
    
    cmUiSetNextW(     uiH, panelId, ctlW );
    cmUiCreateLabel(  uiH, panelId, cmInvalidId, chNumStr, 0 );
    cmUiCreateVSlider(uiH, panelId, pp->a[kOutSliderAmId] + i, NULL,     0, kMinDb,  kMaxDb, 0.1, 0 );
    cmUiPlaceRight(   uiH, panelId );
    cmUiCreateVMeter( uiH, panelId, pp->a[kOutMeterAmId]  + i, NULL,     0, kMtrMin, kMtrMax, 0 ); 
    w = cmUiSetW(     uiH, panelId, ctlW );
    cmUiCreateCheck(  uiH, panelId, pp->a[kOutToneAmId]   + i, "T",      0, false );
    cmUiCreateCheck(  uiH, panelId, pp->a[kOutPassAmId]   + i, "P",      0, false );
    cmUiCreateCheck(  uiH, panelId, pp->a[kOutMuteAmId]   + i, "M",      0, false );
    cmUiSetW(         uiH, panelId, w );
    cmUiSetBaseCol( uiH, panelId, 5 + (i+1)*colW);
  }

 errLabel:
  return rc;
}


cmAmPanel_t* _cmUiAmFindPanel( cmAm_t* p, unsigned panelId )
{
  cmAmPanel_t* pp = p->list;
  for(; pp!=NULL; pp=pp->link)
    if( pp->panelId == panelId )
      return pp;

  cmErrMsg(&p->err,kPanelNotFoundAmRC,"The panel %i was not found.",panelId);

  return NULL;
}

unsigned  _cmUiAmCtlTypeId( cmAm_t* p, cmAmPanel_t* pp, cmUiCId_t cId, unsigned usrId,
  unsigned sliderId, unsigned toneId, unsigned passId, unsigned muteId )
{
    switch( cId )
    {
      case kSliderUiCId:
        assert( pp->a[sliderId] <= usrId && usrId < pp->a[sliderId]+pp->oChCnt);
        return sliderId;
        break;

      case kCheckUiCId:
        if( pp->a[toneId] <= usrId && usrId < pp->a[toneId]+pp->oChCnt )
          return toneId;

        if( pp->a[passId] <= usrId && usrId < pp->a[passId]+pp->oChCnt )
          return passId;

        if( pp->a[muteId] <= usrId && usrId < pp->a[muteId]+pp->oChCnt )
          return muteId;
        break;

      default:
        assert(0);
        break;
    
  }

  return cmInvalidId;
}


cmUiRC_t cmUiAudioSysMstrOnUiEvent( cmUiASMstrH_t h, const cmUiDriverArg_t* a )
{
  cmUiRC_t          rc = kOkUiRC;
  cmAm_t*            p = _cmUiAmHandleToPtr(h);
  cmAmPanel_t*      pp;
  cmAudioSysMstr_t  r;
  unsigned          typeId;

  if((pp = _cmUiAmFindPanel( p, a->panelId )) == NULL)
    return cmErrLastRC(&p->err);

  if( a->usrId == pp->panelId )
  {
    cmAudioSysStatusNotifyEnable(p->asH, pp->asSubIdx, a->ival );
    return rc;
  }

  if( a->usrId >= pp->baseOutId )
    typeId = _cmUiAmCtlTypeId(p,pp,a->cId,a->usrId,kOutSliderAmId,kOutToneAmId,kOutPassAmId,kOutMuteAmId);
  else
    typeId = _cmUiAmCtlTypeId(p,pp,a->cId,a->usrId,kInSliderAmId,kInToneAmId,kInPassAmId,kInMuteAmId);

  
  assert( typeId != cmInvalidId );
  if( typeId == cmInvalidId )
    return cmErrMsg(&p->err,kCtlNotFoundUiRC,"The type of a UI control could not be determined.");

  
  unsigned asInFl = 0;
  unsigned asCtlId = cmInvalidId;
  unsigned asCh    = a->usrId - pp->a[typeId];
  double   asValue = 0;

  switch( typeId )
  {
    case kInSliderAmId: 
      asInFl  = 1;
      asCtlId = kSliderUiAsId;
      asValue = a->fval;
      break;

    case kInToneAmId:   
      asInFl  = 1;
      asCtlId = kToneUiAsId;
      asValue = a->ival;
      break;

    case kInPassAmId:   
      asInFl  = 1;
      asCtlId = kPassUiAsId;
      asValue = a->ival;
      break;

    case kInMuteAmId:   
      asInFl  = 1;;
      asCtlId = kMuteUiAsId;
      asValue = a->ival;
      break;

    case kOutSliderAmId: 
      asCtlId = kSliderUiAsId;
      asValue = a->fval;
      break;

    case kOutToneAmId:   
      asCtlId = kToneUiAsId;
      asValue = a->ival;
      break;

    case kOutPassAmId:   
      asCtlId = kPassUiAsId;
      asValue = a->ival;
      break;

    case kOutMuteAmId:   
      asCtlId = kMuteUiAsId;
      asValue = a->ival;
      break;

  }

  unsigned asDevIdx = asInFl ? pp->iDevIdx : pp->oDevIdx;

  r.asSubIdx  = pp->asSubIdx;
  r.uiId      = kUiMstrSelAsId;
  r.selId     = cmInvalidId;
  r.instId    = cmAudioSysFormUiInstId(asDevIdx,asCh,asInFl,asCtlId);
  r.instVarId = cmInvalidId;
  r.value     = asValue;

  if( cmAudioSysDeliverMsg(p->asH,  &r, sizeof(r), cmInvalidId ) != kOkAsRC )
     rc = cmErrMsg(&p->err,kSubSysFailUiRC,"Audio System master control UI message delivery to the audio system failed.");


  return rc;
}

cmUiRC_t cmUiAudioSysMstrOnStatusEvent( cmUiASMstrH_t h, const cmAudioSysStatus_t* m, const double* iMeterArray, const double* oMeterArray )
{
  cmAm_t*       p = _cmUiAmHandleToPtr(h);
  cmAmPanel_t* pp = p->list;
  for(; pp!=NULL; pp=pp->link)
    if(pp->asSubIdx == m->asSubIdx )
      break;

  if( pp == NULL )
    return cmErrMsg(&p->err,kPanelNotFoundUiRC,"The panel associated with Audio system index %i could not be found.",m->asSubIdx);

  cmUiSetAppId(p->uiH,p->appId);

  cmUiSetInt(p->uiH, pp->updateId,  m->updateCnt  != pp->updateCnt );
  cmUiSetInt(p->uiH, pp->wakeupId,  m->wakeupCnt  != pp->wakeupCnt );
  cmUiSetInt(p->uiH, pp->msgCbId,   m->msgCbCnt   != pp->msgCbCnt );
  cmUiSetInt(p->uiH, pp->audioCbId, m->audioCbCnt != pp->audioCbCnt );

  pp->updateCnt = m->updateCnt;
  pp->wakeupCnt = m->wakeupCnt;
  pp->msgCbCnt  = m->msgCbCnt;
  pp->audioCbCnt= m->audioCbCnt;

  cmUiSetAppId(p->uiH,cmInvalidId);

  return kOkUiRC;
}
