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
#include "cmRtSysMsg.h"
#include "cmRtSys.h"
#include "cmUiDrvr.h"
#include "cmUi.h"
#include "cmUiRtSysMstr.h"

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
  unsigned              rtSubIdx;

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
  cmErr_t      err;
  unsigned     appId;
  cmUiH_t      uiH;
  cmRtSysH_t   asH;
  unsigned     nextId;
  cmAmPanel_t* list;
} cmAm_t;

cmUiRtMstrH_t cmUiRtMstrNullHandle = cmSTATIC_NULL_HANDLE;

cmAm_t* _cmUiAmHandleToPtr( cmUiRtMstrH_t h )
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
      if( cmUiClearPanel( p->uiH, p->appId, panelId ) != kOkUiRC )
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

  if( cmUiDestroyApp( p->uiH, p->appId ) != kOkUiRC )
    cmErrMsg(&p->err,kUiFailAmRC,"UI Mgr. app destroy failed.");
    
  cmMemFree(p);
  return kOkAmRC;
}

cmAmRC_t cmUiRtSysMstrAlloc( cmCtx_t* ctx, cmUiRtMstrH_t* hp, cmUiH_t uiH, cmRtSysH_t asH, unsigned appId )
{
  cmAmRC_t rc = kOkAmRC;
  
  if((rc = cmUiRtSysMstrFree(hp)) != kOkAmRC )
    return rc;

  cmAm_t* p = cmMemAllocZ(cmAm_t,1);
  cmErrSetup(&p->err,&ctx->rpt,"Audio System Master UI");

  p->appId  = appId;
  p->uiH    = uiH;
  p->asH    = asH;
  p->nextId = 0;

    // allocate the UI Mgr. app. slot for the audio system master control UI.
  if( cmUiCreateApp( uiH, appId, cmInvalidId ) != kOkUiRC )
  {
    rc = cmErrMsg(&p->err,kUiFailAmRC,"The UI Mgr. failed while creating the Audio System UI app. slot.");
    goto errLabel;
  }


  hp->h     = p;
 errLabel:
  if( rc != kOkAmRC )
    _cmUiAmFree(p);
  return rc;
}

cmAmRC_t cmUiRtSysMstrFree(  cmUiRtMstrH_t* hp )
{
  cmAmRC_t rc = kOkAmRC;

  if(hp==NULL || cmUiRtSysMstrIsValid(*hp)==false )
    return kOkAmRC;

  cmAm_t* p = _cmUiAmHandleToPtr(*hp);

  if((rc = _cmUiAmFree(p)) != kOkAmRC )
    return rc;

  hp->h = NULL;

  return rc;
}

bool     cmUiRtSysMstrIsValid( cmUiRtMstrH_t h )
{ return h.h != NULL; }


cmAmRC_t cmUiRtSysMstrInitialize( cmUiRtMstrH_t amH, const cmRtSysCtx_t* c, const cmChar_t* inDevLabel, const cmChar_t* outDevLabel )
{
  cmAmRC_t rc       = kOkAmRC;
  cmAm_t*  p        = _cmUiAmHandleToPtr(amH);
  cmUiH_t  uiH      = p->uiH;
  unsigned panelId  = cmInvalidId;
  unsigned colW     = 50;
  unsigned ctlW     = 45;
  unsigned n        = 31;
  cmChar_t chNumStr[ n+1 ];
  int      w;
  cmAmPanel_t* pp = NULL;


  // This function is called once for each audio sub-system.
  // If this is the first call in the sequence then clear the previous setup.
  if( c->rtSubIdx == 0 )
  {
    if((rc = _cmUiAmFreePanels(p,true)) != kOkAmRC )
      goto errLabel;

    assert(p->list == NULL );
  }

  // create the panel recd and link it to the beginning of the list
  pp = cmMemAllocZ(cmAmPanel_t,1);

  pp->link     = p->list;    
  p->list      = pp;
  pp->rtSubIdx = c->rtSubIdx;
  pp->iDevIdx  = c->ss->args.inDevIdx;
  pp->oDevIdx  = c->ss->args.outDevIdx;
  pp->iChCnt   = c->iChCnt;
  pp->oChCnt   = c->oChCnt;

  
  pp->panelId           = p->nextId++;  
  pp->updateId          = p->nextId++;
  pp->wakeupId          = p->nextId++;
  pp->msgCbId           = p->nextId++;
  pp->audioCbId         = p->nextId++;

  pp->a[kLabelAmId]     = p->nextId;
  pp->a[kInSliderAmId]  = p->nextId += c->iChCnt;
  pp->a[kInMeterAmId]   = p->nextId += c->iChCnt;
  pp->a[kInToneAmId]    = p->nextId += c->iChCnt;
  pp->a[kInPassAmId]    = p->nextId += c->iChCnt;
  pp->a[kInMuteAmId]    = p->nextId += c->iChCnt;

  pp->baseOutId         = p->nextId += c->iChCnt;  

  pp->a[kOutSliderAmId] = pp->baseOutId;
  pp->a[kOutMeterAmId]  = p->nextId += c->oChCnt;
  pp->a[kOutToneAmId]   = p->nextId += c->oChCnt;
  pp->a[kOutPassAmId]   = p->nextId += c->oChCnt;
  pp->a[kOutMuteAmId]   = p->nextId += c->oChCnt;
  p->nextId += c->oChCnt;

  panelId = pp->panelId;

  if( cmUiCreatePanel(uiH, p->appId, panelId, "Master", 0 ) != kOkUiRC )
  {
    rc = cmErrMsg(&p->err,kUiFailAmRC,"Panel %i create failed.",panelId);
    goto errLabel;
  }

  cmUiSetFillRows( uiH, p->appId, panelId, true );
  cmUiCreateProgress(uiH, p->appId, panelId, pp->updateId, "Update",  0, 0, 1, 0 );
  cmUiCreateProgress(uiH, p->appId, panelId, pp->wakeupId, "Wakeup",  0, 0, 1, 0 );
  cmUiCreateProgress(uiH, p->appId, panelId, pp->msgCbId,  "Message", 0, 0, 1, 0 );
  cmUiCreateProgress(uiH, p->appId, panelId, pp->audioCbId,"Audio",   0, 0, 1, 0 );
  cmUiSetFillRows( uiH, p->appId, panelId, false );

  cmUiNewLine(     uiH, p->appId, panelId );
  cmUiCreateLabel( uiH, p->appId, panelId, cmInvalidId, inDevLabel, kInsideUiFl | kLeftUiFl ); 
  cmUiNewLine(     uiH, p->appId, panelId );

  unsigned i;
  for(i=0; i<c->iChCnt; ++i)
  {
    snprintf(chNumStr,n,"%i",i);
    
    cmUiSetNextW(     uiH, p->appId, panelId, ctlW );
    cmUiCreateLabel(  uiH, p->appId, panelId, cmInvalidId, chNumStr, 0 );
    cmUiCreateVSlider(uiH, p->appId, panelId, pp->a[kInSliderAmId] + i, NULL,     0, kMinDb,  kMaxDb, 0.1, 0 );
    cmUiPlaceRight(   uiH, p->appId, panelId );
    cmUiCreateVMeter( uiH, p->appId, panelId, pp->a[kInMeterAmId]  + i, NULL,     0, kMtrMin, kMtrMax, 0 ); 
    w = cmUiSetW(     uiH, p->appId, panelId, ctlW );
    cmUiCreateCheck(  uiH, p->appId, panelId, pp->a[kInToneAmId]   + i, "T",      0, false );
    cmUiCreateCheck(  uiH, p->appId, panelId, pp->a[kInPassAmId]   + i, "P",      0, false );
    cmUiCreateCheck(  uiH, p->appId, panelId, pp->a[kInMuteAmId]   + i, "M",      0, false );
    cmUiSetW(         uiH, p->appId, panelId, w );
    cmUiSetBaseCol( uiH, p->appId, panelId, 5 + (i+1)*colW);
  }

  cmUiSetBaseCol(  uiH, p->appId, panelId, 0);
  cmUiNewLine(     uiH, p->appId, panelId );
  cmUiCreateLabel( uiH,p->appId, panelId, cmInvalidId, outDevLabel, kInsideUiFl | kLeftUiFl ); 
  cmUiNewLine(     uiH, p->appId, panelId );

  for(i=0; i<c->oChCnt; ++i)
  {
    snprintf(chNumStr,n,"%i",i);
    
    cmUiSetNextW(     uiH, p->appId, panelId, ctlW );
    cmUiCreateLabel(  uiH, p->appId, panelId, cmInvalidId, chNumStr, 0 );
    cmUiCreateVSlider(uiH, p->appId, panelId, pp->a[kOutSliderAmId] + i, NULL,     0, kMinDb,  kMaxDb, 0.1, 0 );
    cmUiPlaceRight(   uiH, p->appId, panelId );
    cmUiCreateVMeter( uiH, p->appId, panelId, pp->a[kOutMeterAmId]  + i, NULL,     0, kMtrMin, kMtrMax, 0 ); 
    w = cmUiSetW(     uiH, p->appId, panelId, ctlW );
    cmUiCreateCheck(  uiH, p->appId, panelId, pp->a[kOutToneAmId]   + i, "T",      0, false );
    cmUiCreateCheck(  uiH, p->appId, panelId, pp->a[kOutPassAmId]   + i, "P",      0, false );
    cmUiCreateCheck(  uiH, p->appId, panelId, pp->a[kOutMuteAmId]   + i, "M",      0, false );
    cmUiSetW(         uiH, p->appId, panelId, w );
    cmUiSetBaseCol( uiH, p->appId, panelId, 5 + (i+1)*colW);
  }

 errLabel:
  return rc;
}


cmAmPanel_t* _cmUiAmFindPanel( cmAm_t* p, unsigned panelId, bool errFl )
{
  cmAmPanel_t* pp = p->list;
  for(; pp!=NULL; pp=pp->link)
    if( pp->panelId == panelId )
      return pp;

  if( errFl )
    cmErrMsg(&p->err,kPanelNotFoundAmRC,"The panel %i was not found.",panelId);

  return NULL;
}

unsigned  _cmUiAmCtlTypeId( cmAm_t* p, cmAmPanel_t* pp, cmUiCId_t cId, unsigned usrId,
  unsigned                          sliderId, unsigned toneId, unsigned passId, unsigned muteId )
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
        break;
    
  }

  return cmInvalidId;
}


cmUiRC_t cmUiRtSysMstrOnUiEvent( cmUiRtMstrH_t h, const cmUiDriverArg_t* a )
{
  cmUiRC_t      rc       = kOkUiRC;
  cmAm_t*       p        = _cmUiAmHandleToPtr(h);
  cmAmPanel_t*  pp;
  cmRtSysMstr_t r;
  unsigned      typeId;
  bool          tabSelFl = a->dId==kSetValDId && a->cId == kPanelUiCId;
  
  if((pp = _cmUiAmFindPanel( p, a->panelId, !tabSelFl )) == NULL)
  {
    if( tabSelFl )
      return kOkUiRC;

    return cmErrLastRC(&p->err);
  }

  // if the panel tab was selected/deslected ival will be equal to 1/0
  if( a->usrId == pp->panelId )
  {
    cmRtSysStatusNotifyEnable(p->asH, pp->rtSubIdx, a->ival );
    return rc;
  }

  // based on the usrId determine which control generated the event
  if( a->usrId >= pp->baseOutId )
    typeId = _cmUiAmCtlTypeId(p,pp,a->cId,a->usrId,kOutSliderAmId,kOutToneAmId,kOutPassAmId,kOutMuteAmId);
  else
    typeId = _cmUiAmCtlTypeId(p,pp,a->cId,a->usrId,kInSliderAmId,kInToneAmId,kInPassAmId,kInMuteAmId);

  
  // this control is not a slider or check btn so ignore it
  if( typeId == cmInvalidId )
    return rc;

  
  unsigned asInFl  = 0;
  unsigned asCtlId = cmInvalidId;
  unsigned asCh    = a->usrId - pp->a[typeId];
  double   asValue = 0;

  switch( typeId )
  {
    case kInSliderAmId: 
      asInFl  = 1;
      asCtlId = kSliderUiRtId;
      asValue = a->fval;
      break;

    case kInToneAmId:   
      asInFl  = 1;
      asCtlId = kToneUiRtId;
      asValue = a->ival;
      break;

    case kInPassAmId:   
      asInFl  = 1;
      asCtlId = kPassUiRtId;
      asValue = a->ival;
      break;

    case kInMuteAmId:   
      asInFl  = 1;;
      asCtlId = kMuteUiRtId;
      asValue = a->ival;
      break;

    case kOutSliderAmId: 
      asCtlId = kSliderUiRtId;
      asValue = a->fval;
      break;

    case kOutToneAmId:   
      asCtlId = kToneUiRtId;
      asValue = a->ival;
      break;

    case kOutPassAmId:   
      asCtlId = kPassUiRtId;
      asValue = a->ival;
      break;

    case kOutMuteAmId:   
      asCtlId = kMuteUiRtId;
      asValue = a->ival;
      break;

  }

  unsigned asDevIdx = asInFl ? pp->iDevIdx : pp->oDevIdx;

  r.hdr.rtSubIdx  = pp->rtSubIdx;
  r.hdr.selId      = kUiMstrSelRtId;
  r.devIdx    = asDevIdx;
  r.chIdx     = asCh;
  r.inFl      = asInFl;
  r.ctlId     = asCtlId;
  r.value     = asValue;

  if( cmRtSysDeliverMsg(p->asH,  &r, sizeof(r), cmInvalidId ) != kOkRtRC )
     rc  = cmErrMsg(&p->err,kSubSysFailUiRC,"Audio System master control UI message delivery to the audio system failed.");

  return rc;
}

int _cmUiAmLinToDb( double v )
{
  if( v <= 0 )
    return 0;

  v = round(20.0*log10(v)+100.0);
  return cmMin(kMtrMax,cmMax(kMtrMin,v));
  
} 

cmUiRC_t cmUiRtSysMstrOnStatusEvent( cmUiRtMstrH_t h, const cmRtSysStatus_t* m, const double* iMeterArray, const double* oMeterArray )
{
  cmAm_t*      p        = _cmUiAmHandleToPtr(h);
  cmAmPanel_t* pp       = p->list;

  for(; pp!=NULL; pp=pp->link)
    if(pp->rtSubIdx == m->hdr.rtSubIdx )
      break;

  if( pp == NULL )
    return cmErrMsg(&p->err,kPanelNotFoundUiRC,"The panel associated with Audio system index %i could not be found.",m->hdr.rtSubIdx);

  cmUiSetInt(p->uiH, p->appId, pp->updateId,  m->updateCnt  != pp->updateCnt );
  cmUiSetInt(p->uiH, p->appId, pp->wakeupId,  m->wakeupCnt  != pp->wakeupCnt );
  cmUiSetInt(p->uiH, p->appId, pp->msgCbId,   m->msgCbCnt   != pp->msgCbCnt );
  cmUiSetInt(p->uiH, p->appId, pp->audioCbId, m->audioCbCnt != pp->audioCbCnt );

  pp->updateCnt = m->updateCnt;
  pp->wakeupCnt = m->wakeupCnt;
  pp->msgCbCnt  = m->msgCbCnt;
  pp->audioCbCnt= m->audioCbCnt;

  unsigned i;

  for(i=0; i<m->iMeterCnt; ++i)
    cmUiSetInt(p->uiH, p->appId, pp->a[kInMeterAmId]+i,  _cmUiAmLinToDb(iMeterArray[i]) );

  for(i=0; i<m->oMeterCnt; ++i)
    cmUiSetInt(p->uiH, p->appId, pp->a[kOutMeterAmId]+i, _cmUiAmLinToDb(oMeterArray[i])  );

  return kOkUiRC;
}

void cmUiRtSysMstrClearStatus( cmUiRtMstrH_t h )
{
  cmAm_t*  p        = _cmUiAmHandleToPtr(h);

  cmAmPanel_t* pp = p->list;
  for(; pp!=NULL; pp=pp->link)
  {
    cmUiSetInt(p->uiH, p->appId, pp->updateId,  0 );
    cmUiSetInt(p->uiH, p->appId, pp->wakeupId,  0 );
    cmUiSetInt(p->uiH, p->appId, pp->msgCbId,   0 );
    cmUiSetInt(p->uiH, p->appId, pp->audioCbId, 0 );
  }

}
