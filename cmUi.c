#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmArray.h"
#include "cmJson.h"
#include "cmRtSysMsg.h"
#include "cmUiDrvr.h"
#include "cmUi.h"
#include "cmText.h"


typedef struct
{
  int x;
  int y;
  int w; 
  int h;
} cmUiRect_t; 

enum
{
  kUseRectUiFl      = 0x01,
  kPlaceRightUiFl   = 0x02,  // place only next control to right of prev control 
  kPlaceBelowUiFl   = 0x04,  // place only next control to below prev control
  kPlaceBaseRowUiFl = 0x08,  // place only next control at base row
  kNextWHUiFl       = 0x10,  // size next control according to nextW,nextH
  kFillRowsUiFl     = 0x20,  // current dflt fill direction
};

typedef struct cmUiApp_str
{
  unsigned          appId;      // app id and index into cmUI_t.appArrH[].
  unsigned          rtSubIdx;   // 
  bool              activeFl;   // true if this app recd is active and valid
  cmArrayH_t        ctlArrH;    // cmUiCtl_t[]
  cmArrayH_t        pnlArrH;    // cmUiPanel_t[]
} cmUiApp_t;

struct cmUiCtl_str;

typedef struct cmUiPanel_str
{
  cmUiApp_t*          appPtr;   // owner app.
  struct cmUiCtl_str* ctl;      // control recd assoc'd with this panel
  unsigned            flags;    // See kXXXUiFl above
  unsigned            usrId;    // app. supplied id
  cmUiRect_t          rect;     // rect to position next control (only used if kUseRectUiFl is set)
  cmUiRect_t          prevRect;
  int                 baseCol;
  int                 baseRow;
  int                 dfltW;
  int                 dfltH;
  int                 nextW;
  int                 nextH;
  int                 dfltHBorder;
  int                 dfltVBorder;
  int                 nextHBorder;
  int                 nextVBorder;
} cmUiPanel_t;

typedef struct 
{
  unsigned  id;
  cmChar_t* label;
} cmUiListEle_t;


typedef struct cmUiCtl_str
{
  cmUiCId_t       cId;          // control type and used to decode the value union below.
  unsigned        usrId;        // control instance id and index into cmUiApp_t.ctlArrH[].
  unsigned        panelId;      // panel this ctl belongs to
  cmArrayH_t      idArrH;       // id's associated with each sub-element of this control (used by menu's, list's, etc...)
  cmUiDriverArg_t arg;          // cached callback arg for this control

  // current value of this control
  union
  {
    int          ival;
    double       fval;
    cmChar_t*    sval;
    cmUiPanel_t* pnl;
  } u;

  int             (*getInt)( struct cmUiCtl_str* c );
  double          (*getDbl)( struct cmUiCtl_str* c );
  const cmChar_t* (*getStr)( struct cmUiCtl_str* c );

  void            (*setInt)( struct cmUiCtl_str* c, int v );
  void            (*setDbl)( struct cmUiCtl_str* c, double v );
  void            (*setStr)( struct cmUiCtl_str* c, const cmChar_t* s );

} cmUiCtl_t;


typedef struct
{
  cmErr_t          err;         //
  cmCtx_t*         ctx;         // Global context. 
  cmUiDriverFunc_t drvr;        // Driver callback function
  void*            drvrArg;     // Driver callback function arg.
  int              panelW;      // Panel width.
  int              panelH;      // Panel height
  cmUiDriverFunc_t cbFunc;      // Client callback function
  void*            cbArg;       //
  cmArrayH_t       appArrH;     // cmUiApp_t[]
  cmUiCtl_t        dummy;       // dummy recd used to create controls which will never be accessed after creation (like read-only labels)
} cmUi_t;

cmUiH_t cmUiNullHandle = cmSTATIC_NULL_HANDLE;

cmUi_t* _cmUiHandleToPtr( cmUiH_t h )
{
  cmUi_t* p = (cmUi_t*)h.h;
  assert(p != NULL );
  return p;
}

//---------------------------------------------------------------
// int accessors

int _cmUiGetIntFromInt( cmUiCtl_t* ctl )
{ return ctl->u.ival; }

double _cmUiGetDblFromInt( cmUiCtl_t* ctl )
{ return ctl->u.ival; }

const cmChar_t* _cmUiGetStrFromInt( cmUiCtl_t* ctl )
{ assert(0); return ""; }

void _cmUiSetIntFromInt( cmUiCtl_t* ctl, int v )
{ ctl->u.ival = v; }

void _cmUiSetIntFromDbl( cmUiCtl_t* ctl, double v )
{ ctl->u.ival = round(v); }

void _cmUiSetIntFromStr( cmUiCtl_t* ctl, const cmChar_t* v )
{ assert(0);  }

void _cmUiSetIntAccessors( cmUiCtl_t* ctl )
{
  ctl->getInt = _cmUiGetIntFromInt;
  ctl->getDbl = _cmUiGetDblFromInt;
  ctl->getStr = _cmUiGetStrFromInt;

  ctl->setInt = _cmUiSetIntFromInt;
  ctl->setDbl = _cmUiSetIntFromDbl;
  ctl->setStr = _cmUiSetIntFromStr;
}

//---------------------------------------------------------------
// double accessors

int _cmUiGetIntFromDbl( cmUiCtl_t* ctl )
{ return round(ctl->u.fval); }

double _cmUiGetDblFromDbl( cmUiCtl_t* ctl )
{ return ctl->u.fval; }

const cmChar_t* _cmUiGetStrFromDbl( cmUiCtl_t* ctl )
{ assert(0); return ""; }

void _cmUiSetDblFromInt( cmUiCtl_t* ctl, int v )
{ ctl->u.fval = v; }

void _cmUiSetDblFromDbl( cmUiCtl_t* ctl, double v )
{ ctl->u.fval = v; }

void _cmUiSetDblFromStr( cmUiCtl_t* ctl, const cmChar_t* v )
{ assert(0);  }

void _cmUiSetDblAccessors( cmUiCtl_t* ctl )
{
  ctl->getInt = _cmUiGetIntFromDbl;
  ctl->getDbl = _cmUiGetDblFromDbl;
  ctl->getStr = _cmUiGetStrFromDbl;

  ctl->setInt = _cmUiSetDblFromInt;
  ctl->setDbl = _cmUiSetDblFromDbl;
  ctl->setStr = _cmUiSetDblFromStr;
}

//---------------------------------------------------------------
// string accessors

int _cmUiGetIntFromStr( cmUiCtl_t* ctl )
{ assert(0); return -1; }

double _cmUiGetDblFromStr( cmUiCtl_t* ctl )
{ assert(0); return -1; }

const cmChar_t* _cmUiGetStrFromStr( cmUiCtl_t* ctl )
{ return ctl->u.sval; }

void _cmUiSetStrFromInt( cmUiCtl_t* ctl, int v )
{ assert(0); }

void _cmUiSetStrFromDbl( cmUiCtl_t* ctl, double v )
{ assert(0); }

void _cmUiSetStrFromStr( cmUiCtl_t* ctl, const cmChar_t* v )
{  ctl->u.sval = cmMemResizeStr(ctl->u.sval, v==NULL ? "" : v);  }

void _cmUiSetStrAccessors( cmUiCtl_t* ctl )
{
  ctl->getInt = _cmUiGetIntFromStr;
  ctl->getDbl = _cmUiGetDblFromStr;
  ctl->getStr = _cmUiGetStrFromStr;

  ctl->setInt = _cmUiSetStrFromInt;
  ctl->setDbl = _cmUiSetStrFromDbl;
  ctl->setStr = _cmUiSetStrFromStr;
}

//---------------------------------------------------------------


cmUiRC_t _cmUiCallDriver( cmUi_t* p, cmUiDId_t dId, cmUiCtl_t* ctl, unsigned flags )
{
  cmUiRC_t rc = kOkUiRC;
  if( p->drvr != NULL )
  {
    unsigned orgFlags  = ctl->arg.flags;
    ctl->arg.dId       = dId;
    ctl->arg.flags    |= flags;
    ctl->arg.hdr.selId   = kUiDrvrSelRtId;

    rc = p->drvr(p->drvrArg,&ctl->arg);

    ctl->arg.flags = orgFlags;

    if( rc != kOkUiRC )
      rc = cmErrMsg(&p->err,kDrvrErrUiRC,"UI manager driver error.");
  }
  return rc;
}

cmUiRC_t _cmUiSetDriverValueInt( cmUi_t* p, cmUiCtl_t* ctl, unsigned flag, int value )
{ 
  ctl->setInt(ctl,value);
  ctl->arg.ival = value;
  return _cmUiCallDriver(p, kSetValDId, ctl, flag | kIvalUiFl);
}

cmUiRC_t _cmUiSetDriverValueDouble( cmUi_t* p, cmUiCtl_t* ctl, unsigned flag, double value )
{
  ctl->setDbl(ctl,value);
  ctl->arg.fval = value;
  return  _cmUiCallDriver(p, kSetValDId, ctl, flag | kFvalUiFl);  
}

cmUiRC_t _cmUiSetDriverValueStr( cmUi_t* p, cmUiCtl_t* ctl, unsigned flag, const cmChar_t* value )
{
  ctl->setStr(ctl,value);
  ctl->arg.sval = value;
  return _cmUiCallDriver(p, kSetValDId, ctl, flag | kSvalUiFl);    
}

cmUiRC_t _cmUiSetDriverValueIntAndStr( cmUi_t* p, cmUiCtl_t* ctl, unsigned flag, int ival, const cmChar_t* sval )
{
  ctl->arg.ival = ival;
  ctl->arg.sval = sval;
  return _cmUiCallDriver(p, kSetValDId, ctl, flag | kIvalUiFl|kSvalUiFl);      
}


//---------------------------------------------------------------

cmUiRC_t  _cmUiFindApp( cmUi_t* p, unsigned appId, cmUiApp_t** appRef, bool errFl )
{
  cmUiRC_t   rc = kOkUiRC;
  cmUiApp_t* ap = NULL;
  
  // verify that the appId is a valid index
  if( appId < cmArrayCount(p->appArrH) )
    ap = cmArrayPtr(cmUiApp_t,p->appArrH,appId);

  // if the app record was not already allocated
  if( ap != NULL && ap->activeFl == false )
    ap = NULL;

  // report errors
  if( ap == NULL )
  {
    rc = kAppNotFoundUiRC;    

    if( errFl )
      cmErrMsg(&p->err,rc,"App %i not found.", appId );

  }

  *appRef = ap;

  return rc;
}


cmUiRC_t  _cmUiFindCtl( cmUi_t* p, unsigned appId, unsigned usrId, cmUiCtl_t** ctlRef, bool errFl )
{
  cmUiRC_t   rc  = kOkUiRC;
  cmUiApp_t* ap  = NULL;
  cmUiCtl_t* ctl = NULL;

  // find the app this ctl belongs to
  if(( rc =_cmUiFindApp(p,appId,&ap,errFl)) != kOkUiRC )
    goto errLabel;

  // verify that the usrId is a valid index
  if( usrId < cmArrayCount(ap->ctlArrH) )
    ctl = cmArrayPtr(cmUiCtl_t,ap->ctlArrH,usrId);

  // verify that the usrId of the recd matches the requested usr id
  if( ctl!=NULL && ctl->usrId != usrId )
    ctl = NULL;

  // report errors
  if( ctl==NULL )
  {
    rc = kCtlNotFoundUiRC;

    if( errFl )
      cmErrMsg(&p->err,rc,"Control %i not found.",usrId);   
  }
 errLabel:

  *ctlRef = ctl;
  
  return rc;
}

cmUiRC_t _cmUiFastFindCtl( cmUi_t* p, unsigned appId, unsigned usrId, cmUiCtl_t** ctlRef, bool errFl )
{
  assert( appId < cmArrayCount(p->appArrH) );
  
  cmUiApp_t* ap = cmArrayPtr(cmUiApp_t,p->appArrH,appId);

  assert( ap->activeFl && usrId < cmArrayCount(ap->ctlArrH) );
  
  *ctlRef = cmArrayPtr(cmUiCtl_t,ap->ctlArrH,usrId);

  assert( (*ctlRef)->usrId == usrId && (*ctlRef)->arg.appId == ap->appId );

  return kOkUiRC;
}

cmUiRC_t  _cmUiFindPanel( cmUi_t* p, unsigned appId, unsigned panelId, cmUiPanel_t** ppRef, bool errFl )

{
  cmUiRC_t   rc = kOkUiRC;
  cmUiApp_t* ap = NULL;
  unsigned   i,n;

  *ppRef = NULL;

  // find the app this ctl belongs to
  if(( rc =_cmUiFindApp(p,appId,&ap,errFl)) != kOkUiRC )
    goto errLabel;

  n                                                              = cmArrayCount(ap->pnlArrH);
  for(i=0; i<n; ++i)
    if( (*ppRef = cmArrayPtr(cmUiPanel_t,ap->pnlArrH,i))->usrId == panelId )
      break;

 errLabel:

  if(*ppRef == NULL)
  {
    rc = kPanelNotFoundUiRC;

    if( errFl )
      cmErrMsg(&p->err,rc,"Panel %i was not found.",panelId);
  }


  return rc;
}

//---------------------------------------------------------------

void _cmUiReleaseListEles( cmUiCtl_t* ctl )
{
  if( cmArrayIsValid(ctl->idArrH) )
  {
    unsigned n = cmArrayCount(ctl->idArrH);
    unsigned i;
    for(i=0; i<n; ++i)
      cmMemFree( cmArrayPtr(cmUiListEle_t,ctl->idArrH,i)->label );
  }
  
}

cmUiRC_t  _cmUiDestroyCtl( cmUi_t* p, cmUiCtl_t* ctl, bool drvrFl )
{
  cmUiRC_t rc = kOkUiRC;

  if( drvrFl )
    rc = _cmUiCallDriver(p,kDestroyCtlDId,ctl,0);

  switch(ctl->cId)
  {
    case kLabelUiCId:
    case kStringUiCId:
    case kConsoleUiCId:
    case kFilenameUiCId:
    case kDirUiCId:
      cmMemFree(ctl->u.sval);
      break;

    default:
      break;
  }

  ctl->cId     = kInvalidUiCId;
  ctl->usrId   = cmInvalidId;
  ctl->panelId = cmInvalidId;

  _cmUiReleaseListEles(ctl);

  cmArrayRelease(&ctl->idArrH);

  return rc;
}

cmUiRC_t _cmUiDestroyPanel( cmUi_t* p, unsigned appId, unsigned panelId )
{
  cmUiRC_t     rc = kOkUiRC;
  cmUiPanel_t* pp = NULL;

  // get the panel recd ptr
  if((rc = _cmUiFindPanel(p,appId,panelId,&pp,true)) != kOkUiRC )
    return rc;

  cmUiApp_t* ap = pp->appPtr;

  // notify the driver to destroy the panel
  if((rc = _cmUiCallDriver(p,kDestroyCtlDId,pp->ctl,0)) != kOkUiRC )
    return rc;

  cmUiCtl_t* ctl = NULL;
  unsigned   i   = 0;
  unsigned   n   = cmArrayCount(ap->ctlArrH);
  cmUiRC_t   rc0;

  // Destroy all controls that belong to this panel.
  // Notice that _cmUiDestroyCtl() does not need to call the driver
  // because destroying the drivers panel has implicitely also 
  // destroyed all the contols assigned to it.
  for(i=0; i<n; ++i)
    if( _cmUiFindCtl(p,appId,i,&ctl,false)    == kOkUiRC && ctl != NULL && ctl->panelId == panelId)
      if((rc0 = _cmUiDestroyCtl(p,ctl,false)) != kOkUiRC )
        rc                                     = rc0;

  // release the panel record
  pp->appPtr = NULL;
  pp->usrId  = cmInvalidId;
  
  return rc;
}

cmUiRC_t _cmUiDestroyApp( cmUi_t* p, unsigned appId, bool errFl )
{
  cmUiRC_t   rc = kOkUiRC;
  cmUiRC_t   rc0;
  cmUiApp_t* ap;
  
  // find the app to destroy
  if( _cmUiFindApp(p,appId,&ap,false) != kOkUiRC )
  {
    if( errFl )
      rc = cmErrMsg(&p->err,kAppNotFoundUiRC,"The app %i was not found for destruction.",appId);    

    goto errLabel;
  }

  assert( ap != NULL );

  // destroy all panels owned by this app
  unsigned i;
  unsigned n = cmArrayCount(ap->pnlArrH);
  for(i=0; i<n; ++i)
  {
    cmUiPanel_t* pp                                    = cmArrayPtr(cmUiPanel_t,ap->pnlArrH,i);
    if( pp->usrId != cmInvalidId )
      if((rc0 = _cmUiDestroyPanel(p,appId,pp->usrId)) != kOkUiRC )
        rc                                             = rc0;
  }

  ap->appId = -1;
  ap->activeFl = false;
  cmArrayRelease(&ap->ctlArrH);
  cmArrayRelease(&ap->pnlArrH);

 errLabel:
  //p->curAppId = orgAppId;

  return rc;
}

cmUiRC_t _cmUiDestroyAllApps( cmUi_t* p )
{
  cmUiRC_t rc = kOkUiRC;
  unsigned n  = cmArrayCount(p->appArrH);
  unsigned i;
  for(i=0; i<n; ++i)
  {
    cmUiRC_t rc0;
    if((rc0 = _cmUiDestroyApp(p,i,false)) != kOkUiRC )
      rc                                   = rc0;
  }

  cmArrayClear(p->appArrH,false);

  return rc;
}

//---------------------------------------------------------------

cmUiRC_t _cmUiGetCtlXYWH( cmUi_t* p, cmUiDriverArg_t* a, cmUiPanel_t* pp )
{
  if( cmIsFlag(pp->flags,kUseRectUiFl ) )
  {
    pp->flags = cmClrFlag(pp->flags,kUseRectUiFl);
    a->x = pp->rect.x;
    a->y = pp->rect.y;
    a->w = pp->rect.w;
    a->h = pp->rect.h;
    return kOkUiRC;
  }

  if( pp->prevRect.x == -1 )
    pp->prevRect.x = pp->baseCol;

  if( pp->prevRect.y == -1 )
    pp->prevRect.y = pp->baseRow;

  if( pp->prevRect.w == -1 )
    pp->prevRect.w = 0;

  if( pp->prevRect.h == -1 )
    pp->prevRect.h = 0;

  // Get direction flag. Fill across rows? or down columns?
  bool fillRowFl = cmIsFlag(pp->flags,kFillRowsUiFl);

  // the 'place to right' flag overrides the directon flag for just this instance
  if( cmIsFlag(pp->flags,kPlaceRightUiFl ) )
    fillRowFl = true;

  // the 'place under' flag overides the direction flag for just this instance
  if( cmIsFlag(pp->flags,kPlaceBelowUiFl ) )
    fillRowFl = false;

  // set x coord - if filling acros rows ...
  if( fillRowFl )
    a->x = pp->prevRect.x + pp->prevRect.w + pp->nextHBorder;
  else
    a->x = pp->baseCol;         // ... or down columns

  // if a new column was set then move to the base row
  if( cmIsFlag(pp->flags,kPlaceBaseRowUiFl) )
    a->y = pp->baseRow;
  else
  {
    // set y coord - if filling acros rows ...
    if( fillRowFl )
      a->y = pp->prevRect.y;
    else
      a->y = pp->prevRect.y + pp->prevRect.h + pp->nextVBorder; // ... or down columns
  }

  a->w = pp->nextW;
  a->h = pp->nextH;

  pp->prevRect.x = a->x;
  pp->prevRect.y = a->y;
  pp->prevRect.w = a->w;
  pp->prevRect.h = a->h; 

  pp->nextW       = pp->dfltW;
  pp->nextH       = pp->dfltH;
  pp->nextHBorder = pp->dfltHBorder;
  pp->nextVBorder = pp->dfltVBorder;

  pp->flags = cmClrFlag(pp->flags,kNextWHUiFl | kPlaceRightUiFl | kPlaceBelowUiFl | kPlaceBaseRowUiFl );

  return kOkUiRC;
}


cmUiRC_t _cmUiCreateCtl( cmUi_t* p, unsigned appId, unsigned panelId, cmUiCId_t cId, unsigned usrId, const cmChar_t* label, unsigned flags, cmUiCtl_t** ctlRef )
{
  cmUiRC_t     rc;
  cmUiPanel_t* pp = NULL;
  cmUiApp_t*   ap = NULL;

  if( ctlRef != NULL )
    *ctlRef = NULL;

  // locate the app
  if((rc = _cmUiFindApp(p,appId,&ap,true)) != kOkUiRC )
    return rc;

  // locate the panel the control belongs to
  if((rc = _cmUiFindPanel(p,appId,panelId,&pp,true)) != kOkUiRC )
    return rc; 

  // get the new ctl record
  cmUiCtl_t* ctl;
  if( usrId == cmInvalidId )
  {
    ctl = &p->dummy;
    memset(ctl,0,sizeof(*ctl));
  }
  else
  {
    if( cmArrayIsValid(ap->ctlArrH) == false || usrId >= cmArrayCount(ap->ctlArrH) )
      ctl = cmArrayClr(cmUiCtl_t,ap->ctlArrH,usrId);
    else
    {
      ctl = cmArrayPtr(cmUiCtl_t,ap->ctlArrH,usrId);

      // if the ctl recd is already in use
      if( ctl->cId != kInvalidUiCId )
        _cmUiDestroyCtl(p,ctl,true);
    }
  }

  // setup this controls cached callback arg record
  cmUiDriverArgSetup(&ctl->arg,ap->rtSubIdx,cmInvalidId,kInvalidDId,ap->appId,usrId,panelId,cId,flags,0,0,label,-1,-1,-1,-1);

  // calc the control location - for non-panel controls
  if( cId != kPanelUiCId )
    if((rc = _cmUiGetCtlXYWH(p,&ctl->arg,pp)) != kOkUiRC )
      return rc;


  // setup the new ctl record
  ctl->cId     = cId;
  ctl->usrId   = usrId;
  ctl->panelId = panelId;

  // display-only controls don't need an id array
  if( usrId != cmInvalidId )
    cmArrayAlloc(p->ctx,&ctl->idArrH,sizeof(cmUiListEle_t));
  

  if( ctlRef != NULL )
    *ctlRef = ctl;

  return  _cmUiCallDriver(p,kCreateCtlDId,ctl,0);
}


cmUiRC_t cmUiAlloc( 
  cmCtx_t*         ctx, 
  cmUiH_t*         uiHPtr, 
  cmUiDriverFunc_t drvrFunc, 
  void*            drvrArg,
  cmUiDriverFunc_t cbFunc,
  void *           cbArg)
{
  cmUiRC_t rc;
  if((rc = cmUiFree(uiHPtr)) != kOkUiRC )
    return rc;

  cmUi_t* p = cmMemAllocZ(cmUi_t,1);
  cmErrSetup(&p->err,&ctx->rpt,"cmUi");
  
  p->ctx     = ctx;
  p->drvr    = drvrFunc;
  p->drvrArg = drvrArg;
  p->cbFunc  = cbFunc;
  p->cbArg   = cbArg;
  p->panelW  = 1000;
  p->panelH  = 500;

  cmArrayAlloc(p->ctx,&p->appArrH,sizeof(cmUiApp_t));

  uiHPtr->h = p;

  return rc;
}

cmUiRC_t cmUiFree(  cmUiH_t* hp )
{
  cmUiRC_t rc = kOkUiRC;

  if( hp == NULL || cmUiIsValid(*hp) == false )
    return kOkUiRC;
  
  cmUi_t* p = _cmUiHandleToPtr(*hp);


  if((rc = _cmUiDestroyAllApps(p)) != kOkUiRC )
    return rc;

  cmArrayRelease(&p->appArrH);
  cmMemFree(p);

  hp->h = NULL;

  return rc;
}


bool     cmUiIsValid( cmUiH_t h )
{ return h.h != NULL; }

void cmUiSetDriver( cmUiH_t h, cmUiDriverFunc_t drvrFunc, void* drvrArg )
{
  cmUi_t* p  = _cmUiHandleToPtr(h);
  p->drvr    = drvrFunc;
  p->drvrArg = drvrArg;
}


cmUiRC_t cmUiCreateApp(  cmUiH_t h, unsigned appId, unsigned rtSubIdx )
{
  cmUiRC_t rc = kOkUiRC;

  if( cmUiIsValid(h) == false)
    return rc;

  cmUi_t*    p  = _cmUiHandleToPtr(h);
  cmUiApp_t* ap = NULL;

  // verify that the requested app does not exist
  if( _cmUiFindApp(p,appId, &ap, false ) == kOkUiRC )
    return cmErrMsg(&p->err,kInvalidIdUiRC,"The id (%i) of the new application is already in use.",appId);

  ap           = cmArrayClr(cmUiApp_t,p->appArrH,appId);
  ap->appId    = appId;
  ap->rtSubIdx = rtSubIdx;
  ap->activeFl     = true;
  cmArrayAlloc(p->ctx,&ap->ctlArrH,sizeof(cmUiCtl_t));
  cmArrayAlloc(p->ctx,&ap->pnlArrH,sizeof(cmUiPanel_t));


  return rc;  
}

bool     cmUiAppIsActive( cmUiH_t uiH, unsigned appId )
{
  cmUi_t*         p  = _cmUiHandleToPtr(uiH);
  cmUiApp_t*      ap = NULL;

  return _cmUiFindApp(p,appId, &ap, false ) == kOkUiRC;
}


cmUiRC_t cmUiDestroyApp( cmUiH_t h, unsigned appId )
{
  if( cmUiIsValid(h)==false)
    return kOkUiRC;

  cmUi_t*  p  = _cmUiHandleToPtr(h);
  return _cmUiDestroyApp(p,appId,true);
}


cmUiRC_t cmUiDestroyAllApps( cmUiH_t h )
{
  if( cmUiIsValid(h)==false)
    return kOkUiRC;

  cmUi_t* p  = _cmUiHandleToPtr(h);  
  return _cmUiDestroyAllApps(p);
}


unsigned cmUiAppIdToAsSubIndex( cmUiH_t uiH, unsigned appId )
{
  cmUi_t*    p  = _cmUiHandleToPtr(uiH);  
  cmUiApp_t* ap = NULL;

  if(  _cmUiFindApp(p,appId, &ap, true ) != kOkUiRC )
    return cmInvalidIdx;

  return ap->rtSubIdx;
}


unsigned cmUiAppCount( cmUiH_t h )
{
  if( cmUiIsValid(h)==false)
    return 0;

  cmUi_t*    p  = _cmUiHandleToPtr(h);

  return cmArrayCount(p->appArrH);
}

cmUiRC_t _cmUiCallClient( cmUi_t* p, cmUiDId_t dId, cmUiCtl_t* ctl, unsigned flags )
{
  unsigned orgFlags   = ctl->arg.flags;

  ctl->arg.hdr.selId  = kUiSelRtId,
  ctl->arg.flags     |= flags;
  ctl->arg.dId        = dId;
  cmUiRC_t rc         = p->cbFunc(p->cbArg,&ctl->arg);
  ctl->arg.dId        = kInvalidDId;
  ctl->arg.flags      = orgFlags;
  ctl->arg.hdr.selId  = cmInvalidId;
  return rc;
}

cmUiRC_t cmUiOnDriverEvent( cmUiH_t h,  const cmUiDriverArg_t* arg )
{
  cmUiRC_t        rc = kOkUiRC;
  cmUi_t*         p  = _cmUiHandleToPtr(h);
  cmUiCtl_t*      ctl;
  //unsigned        orgAppId = cmUiAppId(h);

  //if((rc = cmUiSetAppId(h,arg->appId)) != kOkUiRC )
  //  return rc;
  
  if((rc = _cmUiFindCtl(p,arg->appId,arg->usrId,&ctl,true)) != kOkUiRC )
    goto errLabel;

  switch( arg->dId )
  {
    case kEnableDId:
      ctl->arg.ival = arg->ival;
      return _cmUiCallClient(p,kEnableDId,ctl,arg->flags);

    case kSetValDId:
      break;

    default:
      assert(0);
      break;
  }

  switch( arg->cId )
  {
    case kInvalidUiCId:  
      break;

    case kPanelUiCId:    
    case kBtnUiCId:      
    case kCheckUiCId:    
      ctl->arg.ival = ctl->u.ival = cmUiDriverArgGetInt(arg); 
      break;

    case kMenuBtnUiCId:  
    case kListUiCId:     
      {
        if( cmIsFlag(arg->flags,kValUiFl ) )
        {
          unsigned eleIdx = cmUiDriverArgGetInt(arg);

          if(eleIdx  >= cmArrayCount(ctl->idArrH))
          {
            rc = cmErrMsg(&p->err,kInvalidIdUiRC,"Invalid menu or list driver element id=%i element count:%i.",eleIdx,cmArrayCount(ctl->idArrH));
            goto errLabel;
          }

          // convert the selected items index to the associated client id value
          unsigned eleId = cmArrayPtr(cmUiListEle_t,ctl->idArrH,eleIdx)->id;
          ctl->setInt(ctl,eleId);
          ctl->arg.ival = eleId;
        }
      }
      break;

    case kConsoleUiCId:
      {
        const cmChar_t* s;
        if((s = cmUiDriverArgGetString(arg)) != NULL )
          ctl->arg.sval = ctl->u.sval = cmTextAppendSS(ctl->u.sval, s );
      }      
      break;
     
    case kLabelUiCId:    
    case kStringUiCId:
    case kFilenameUiCId:
    case kDirUiCId:
      {
        const cmChar_t* s;
        if((s = cmUiDriverArgGetString(arg)) != NULL )
          ctl->arg.sval = ctl->u.sval = cmMemResizeStr(ctl->u.sval,s); 
      }
      break;

    case kSliderUiCId:
    case kNumberUiCId:   
      ctl->arg.fval = ctl->u.fval = cmUiDriverArgGetDouble(arg); 
      break;

    case kProgressUiCId: 
    case kMeterUiCId:    
      ctl->arg.ival = ctl->u.ival = cmUiDriverArgGetInt(arg);
      break;

    case kMaxUiCId:
      assert(0);
      break;

  }

  // reflect the event to the client
  if( cmIsNotFlag(arg->flags, kNoReflectUiFl ) )
  {
    rc = _cmUiCallClient(p,kSetValDId,ctl,0);
  }

 errLabel:
  //cmUiSetAppId(h,orgAppId);

  return rc;
}

void _cmUiPanelSetDefaultValues( cmUiPanel_t* pp)
{
  pp->baseCol    = 2;
  pp->baseRow    = 2;
  pp->dfltW      = 150;
  pp->dfltH      = 25;
  pp->nextW      = pp->dfltW;
  pp->nextH      = pp->dfltH;
  pp->dfltHBorder= 2;
  pp->dfltVBorder= 2;
  pp->nextHBorder= pp->dfltHBorder;
  pp->nextVBorder= pp->dfltVBorder;
  pp->prevRect.x = -1;
  pp->prevRect.y = -1;
  pp->prevRect.w = -1;
  pp->prevRect.h = -1;
}



cmUiRC_t cmUiCreatePanel(   cmUiH_t uiH, unsigned appId, unsigned newPanelId, const cmChar_t* label, unsigned flags )
{
  cmUiRC_t        rc;
  cmUi_t*         p    = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*      ctl  = NULL;
  cmUiApp_t*      ap   = NULL;

  if(( rc = _cmUiFindApp(p,appId,&ap,true)) != kOkUiRC )
    return rc;

  cmUiPanel_t*    pp   = cmArrayPush(ap->pnlArrH,NULL,1);
  assert( pp != NULL );

  pp->appPtr     = ap;
  pp->ctl        = NULL;
  pp->usrId      = newPanelId;

  _cmUiPanelSetDefaultValues(pp);

  if((rc = _cmUiCreateCtl(p, appId, newPanelId, kPanelUiCId, newPanelId, label, flags, &pp->ctl )) != kOkUiRC )
  {
    // TODO - destroy panel record here
    return rc;
  }
  
  _cmUiFindCtl(p,appId,newPanelId,&ctl,true);
  
  assert(ctl!=NULL);
  ctl->u.pnl = pp;
  return rc;
}


cmUiRC_t cmUiCreateBtn(     cmUiH_t uiH, unsigned appId, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags )
{
  cmUiRC_t       rc;
  cmUi_t*         p = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*      c;

  if((rc = _cmUiCreateCtl(p,appId,panelId,kBtnUiCId,id,label,flags,&c)) == kOkUiRC )
  {
    _cmUiSetIntAccessors(c);
  }
  return rc;
}

cmUiRC_t cmUiCreateCheck(   cmUiH_t uiH, unsigned appId, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, bool dflt )
{
  cmUiRC_t       rc;
  cmUi_t*         p = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*      c;
  
  if((rc = _cmUiCreateCtl(p,appId,panelId,kCheckUiCId,id,label,flags,&c)) == kOkUiRC )
  {
    _cmUiSetIntAccessors(c);

    rc = _cmUiSetDriverValueInt(p,c,kValUiFl | kNoReflectUiFl,dflt);
  }
  return rc;
}

cmUiRC_t cmUiCreateLabel(   cmUiH_t uiH, unsigned appId, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags )
{
  cmUiRC_t       rc;
  cmUi_t*         p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*      c;

  if((rc = _cmUiCreateCtl(p,appId,panelId,kLabelUiCId,id,label,flags,&c)) == kOkUiRC )
    _cmUiSetStrAccessors(c);
  return rc;
}

cmUiRC_t cmUiCreateString(    cmUiH_t uiH, unsigned appId, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, const cmChar_t* text )  
{
  cmUiRC_t        rc = kOkUiRC;
  cmUi_t*         p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*      c;

  if(( rc = _cmUiCreateCtl(p,appId,panelId,kStringUiCId,id,label,flags,&c)) == kOkUiRC )
  {
    _cmUiSetStrAccessors(c);

    rc   = _cmUiSetDriverValueStr(p,c,kValUiFl | kNoReflectUiFl,text);
  }

  return rc;
}

cmUiRC_t cmUiCreateConsole(    cmUiH_t uiH, unsigned appId, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, const cmChar_t* text )  
{
  cmUiRC_t        rc = kOkUiRC;
  cmUi_t*         p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*      c;

  if(( rc = _cmUiCreateCtl(p,appId,panelId,kConsoleUiCId,id,label,flags,&c)) == kOkUiRC )
  {
    _cmUiSetStrAccessors(c);

    rc   = _cmUiSetDriverValueStr(p,c,kValUiFl | kNoReflectUiFl,text);
  }

  return rc;
}

cmUiRC_t _cmUiCreateNumber(  cmUiH_t uiH, unsigned appId, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, double min, double max, double incr, double dflt )
{
  cmUiRC_t        rc = kOkUiRC;
  cmUi_t*         p  = _cmUiHandleToPtr(uiH);
  cmUiCId_t       cid = kNumberUiCId;
  cmUiCtl_t*      c;
  cmUiPanel_t*    pp;

  if( cmIsFlag(flags,kVertUiFl|kHorzUiFl) )
  {
    if( cmIsFlag(flags,kVertUiFl) )
    {
      if((rc =  _cmUiFindPanel(p,appId,panelId,&pp,true)) != kOkUiRC )
        return rc;

      // if the size of the control was not excplicitly set 
      // then swap width and height
      if( cmIsNotFlag(pp->flags,kNextWHUiFl) && cmIsNotFlag(pp->flags,kUseRectUiFl) )
        cmUiSetNextWH( uiH, appId, panelId, cmUiH(uiH,appId,panelId), cmUiW(uiH,appId,panelId) );
    }

    cid = kSliderUiCId;
  }

  if(( rc = _cmUiCreateCtl(p,appId,panelId,cid,id,label,flags,&c)) == kOkUiRC )
  {
    cmUiRC_t rc0;

    _cmUiSetDblAccessors(c);

    if((rc0 = _cmUiSetDriverValueDouble(p,c,kMinUiFl | kNoReflectUiFl,min)) != kOkUiRC )
      rc = rc0;

    if((rc0 = _cmUiSetDriverValueDouble(p,c,kMaxUiFl | kNoReflectUiFl,max)) != kOkUiRC )
      rc = rc0;

    if((rc0 = _cmUiSetDriverValueDouble(p,c,kIncUiFl | kNoReflectUiFl,incr)) != kOkUiRC )
      rc = rc0;

    if((rc0 = _cmUiSetDriverValueDouble(p,c,kValUiFl | kNoReflectUiFl,dflt)) != kOkUiRC )
      rc = rc0;

  }

  return rc;
}

cmUiRC_t cmUiCreateNumber(  cmUiH_t uiH, unsigned appId, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, double min, double max, double incr, double dflt )
{ 
  return _cmUiCreateNumber(uiH,appId,panelId,id,label,flags,min,max,incr,dflt);  
}

cmUiRC_t cmUiCreateHSlider( cmUiH_t uiH, unsigned appId, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, double min, double max, double incr, double dflt )
{ 
  return _cmUiCreateNumber(uiH,appId,panelId,id,label,flags | kHorzUiFl, min,max,incr,dflt);
}

cmUiRC_t cmUiCreateVSlider( cmUiH_t uiH, unsigned appId, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, double min, double max, double incr, double dflt )
{
  return _cmUiCreateNumber(uiH,appId,panelId,id,label,flags | kVertUiFl, min,max,incr,dflt);
}


cmUiRC_t cmUiCreateProgress(cmUiH_t uiH, unsigned appId, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, int min, int max, int dflt )
{
  cmUiRC_t        rc = kOkUiRC;
  cmUi_t*         p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*      c;

  if(( rc = _cmUiCreateCtl(p,appId,panelId,kProgressUiCId,id,label,flags,&c)) == kOkUiRC )
  {
    cmUiRC_t rc0;

    _cmUiSetIntAccessors(c);

    if((rc0 = _cmUiSetDriverValueInt(p,c,kMinUiFl | kNoReflectUiFl,min)) != kOkUiRC )
      rc = rc0;

    if((rc0 = _cmUiSetDriverValueInt(p,c,kMaxUiFl | kNoReflectUiFl,max)) != kOkUiRC )
      rc = rc0;

    if((rc0 = _cmUiSetDriverValueInt(p,c,kValUiFl | kNoReflectUiFl,dflt)) != kOkUiRC )
      rc = rc0;

  }

  return rc;
}

cmUiRC_t _cmUiCreateMeter(   cmUiH_t uiH, unsigned appId, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, int min, int max, int dflt) 
{
  cmUiRC_t       rc;
  cmUi_t*         p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*      c;
  cmUiPanel_t*   pp;

  if( cmIsFlag(flags,kVertUiFl) )
  {    
    if((rc =  _cmUiFindPanel(p,appId,panelId,&pp,true)) != kOkUiRC )
      return rc;

    // if the size of the control has not been explicitely set 
    // then swap height and width for vertical meters.
    if( cmIsNotFlag(pp->flags,kNextWHUiFl) && cmIsNotFlag(pp->flags,kUseRectUiFl) )
      cmUiSetNextWH( uiH, appId, panelId, cmUiH(uiH,appId,panelId), cmUiW(uiH,appId,panelId) );
  }

  if((rc = _cmUiCreateCtl(p,appId,panelId,kMeterUiCId,id,label,flags,&c)) == kOkUiRC )
  {
    cmUiRC_t rc0;

    _cmUiSetIntAccessors(c);

    if((rc0 = _cmUiSetDriverValueInt(p,c,kMinUiFl | kNoReflectUiFl,min)) != kOkUiRC )
      rc = rc0;

    if((rc0 = _cmUiSetDriverValueInt(p,c,kMaxUiFl | kNoReflectUiFl,max)) != kOkUiRC )
      rc = rc0;

    if((rc0 = _cmUiSetDriverValueInt(p,c,kValUiFl  | kNoReflectUiFl,dflt)) != kOkUiRC )
      rc = rc0;

  }
  return rc;
}


cmUiRC_t cmUiCreateHMeter(   cmUiH_t uiH, unsigned appId, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, int min, int max, int dflt ) 
{
  return _cmUiCreateMeter(uiH,appId,panelId,id,label,flags | kHorzUiFl,min,max,dflt);
}

cmUiRC_t cmUiCreateVMeter(   cmUiH_t uiH, unsigned appId, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, int min, int max, int dflt ) 
{
  return _cmUiCreateMeter(uiH,appId,panelId,id,label,flags | kVertUiFl,min,max,dflt);
}

cmUiRC_t cmUiCreateFileBtn(cmUiH_t uiH, unsigned appId, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, const cmChar_t* dfltDir, const cmChar_t* patStr )
{
  cmUiRC_t        rc = kOkUiRC;
  cmUi_t*         p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*      c;

  if(( rc = _cmUiCreateCtl(p,appId,panelId,kFilenameUiCId,id,label,flags,&c)) == kOkUiRC )
  {
    cmUiRC_t rc0;

    _cmUiSetStrAccessors(c);

    if( dfltDir != NULL )
    {
      if((rc0 = _cmUiSetDriverValueStr(p,c,kFnDirUiFl | kNoReflectUiFl,dfltDir)) != kOkUiRC )
        rc = rc0;
    }

    if( patStr != NULL )
    {
      if((rc0 = _cmUiSetDriverValueStr(p,c,kFnPatUiFl  | kNoReflectUiFl,patStr)) != kOkUiRC )
        rc = rc0;
    }
  }
  return rc;
}

cmUiRC_t cmUiCreateDirBtn(  cmUiH_t uiH, unsigned appId, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, const cmChar_t* dfltDir )
{
  cmUiRC_t        rc = kOkUiRC;
  cmUi_t*         p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*      c;

  if(( rc = _cmUiCreateCtl(p,appId,panelId,kDirUiCId,id,label,flags,&c)) == kOkUiRC )
  {
    cmUiRC_t rc0;

    _cmUiSetStrAccessors(c);

    if( dfltDir != NULL )
    {
      if((rc0 = _cmUiSetDriverValueStr(p,c,kValUiFl | kNoReflectUiFl,dfltDir)) != kOkUiRC )
        rc = rc0;
    }
  }
  return rc;
}

cmUiRC_t cmUiCreateMenuBtn( cmUiH_t uiH, unsigned appId, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags )
{
  cmUiRC_t       rc;
  cmUi_t*         p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*      c;

  if((rc = _cmUiCreateCtl(p,appId,panelId,kMenuBtnUiCId,id,label,flags,&c)) == kOkUiRC )
    _cmUiSetIntAccessors(c);
  return rc;
}

cmUiRC_t cmUiCreateMenuBtnV(cmUiH_t uiH, unsigned appId, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, const cmChar_t* label0, unsigned id0, va_list vl )
{
  cmUiRC_t rc = kOkUiRC;
  // TODO:
  return rc;
}

cmUiRC_t cmUiCreateMenuBtnA(cmUiH_t uiH, unsigned appId, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, const cmChar_t* label0, unsigned id0, ... )
{
  va_list vl;
  va_start(vl,id0);
  cmUiRC_t rc = cmUiCreateMenuBtnV(uiH,appId,panelId,id,label,flags,label0,id0,vl);
  va_end(vl);
  return rc;
}

cmUiRC_t cmUiCreateMenuBtnJson(cmUiH_t uiH, unsigned appId, unsigned panelId, unsigned id, const cmChar_t* lavel, unsigned flags, const cmJsonNode_t* root, const cmChar_t* memberLabel )
{
  cmUiRC_t rc = kOkUiRC;
  // TODO:
  return rc;
}

cmUiRC_t cmUiCreateList(cmUiH_t uiH, unsigned appId, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, unsigned visibleRowCnt )
{
  cmUi_t*         p    = _cmUiHandleToPtr(uiH);
  cmUiRC_t        rc;
  cmUiCtl_t*      c;
  cmUiPanel_t*    pp;

  if((rc =  _cmUiFindPanel(p,appId,panelId,&pp,true)) != kOkUiRC )
    return rc;

  if( cmIsNotFlag(pp->flags,kNextWHUiFl) )
    cmUiSetNextWH( uiH, appId, panelId, cmUiNextW(uiH,appId,panelId), cmUiH(uiH,appId,panelId) * visibleRowCnt );
  
  if((rc =  _cmUiCreateCtl(p,appId,panelId,kListUiCId,id,label,flags,&c)) == kOkUiRC )
    _cmUiSetIntAccessors(c);
  
  return rc;
}

cmUiRC_t cmUiCreateListV(       cmUiH_t uiH, unsigned appId, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, unsigned visibleRowCnt, const cmChar_t* label0, unsigned id0, va_list vl )
{
  cmUiRC_t rc = kOkUiRC;
  // TODO:
  return rc;
}

cmUiRC_t cmUiCreateListA(       cmUiH_t uiH, unsigned appId, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, unsigned visibleRowCnt, const cmChar_t* label0, unsigned id0, ... )
{
  va_list vl;
  va_start(vl,id0);
  cmUiRC_t rc = cmUiCreateListV(uiH,appId,panelId,id,label,flags,visibleRowCnt,label0,id0,vl);
  va_end(vl);
  return rc;
}

cmUiRC_t cmUiCreateListJson(    cmUiH_t uiH, unsigned appId, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, unsigned visibleRowCnt, const cmJsonNode_t* root, const cmChar_t* memberLabel )
{
  cmUiRC_t rc = kOkUiRC;
  // TODO:
  return rc;
}

cmUiRC_t cmUiAppendListEle(    cmUiH_t uiH, unsigned appId, unsigned panelId, unsigned id, const cmChar_t* text, unsigned eleId )
{
  cmUiRC_t        rc  = kOkUiRC;
  cmUi_t*         p   = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*      ctl = NULL;

  if((rc = _cmUiFindCtl(p,appId,id,&ctl,true)) != kOkUiRC )
    return rc;

  if( ctl->cId != kListUiCId && ctl->cId != kMenuBtnUiCId )
    return cmErrMsg(&p->err,kInvalidCtlOpUiRC,"List elements may only be set on 'list' and 'menu button' controls.");

  

  // set the local value
  cmUiListEle_t ele;
  ele.id = eleId;
  ele.label = cmMemAllocStr(text);
  cmArrayPush(ctl->idArrH,&ele,1);

  return _cmUiSetDriverValueIntAndStr(p,ctl,kAppendUiFl,eleId,text);
}

cmUiRC_t cmUiClearList(        cmUiH_t uiH, unsigned appId, unsigned panelId, unsigned id )
{
  cmUiRC_t        rc  = kOkUiRC;
  cmUi_t*         p   = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*      ctl = NULL;

  if((rc = _cmUiFindCtl(p,appId,id,&ctl,true)) != kOkUiRC )
    return rc;

  // verify that this is a list or menu btn
  if( ctl->cId != kListUiCId && ctl->cId != kMenuBtnUiCId )
    return cmErrMsg(&p->err,kInvalidCtlOpUiRC,"List elements may only be cleared on 'list' and 'menu button' controls.");

  // call the driver
  if((rc = _cmUiSetDriverValueInt(p, ctl, kClearUiFl, 0 )) != kOkUiRC )
    return rc;

  // clear the ctl's id array
  if( cmArrayIsValid(ctl->idArrH) )
  {
    _cmUiReleaseListEles(ctl);

    cmArrayClear(ctl->idArrH,false);
  }

  return rc;
}

cmUiRC_t cmUiEnableCtl( cmUiH_t uiH, unsigned appId, unsigned panelId, unsigned id, bool enableFl )
{
  cmUiRC_t     rc = kOkUiRC;
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*   ctl = NULL;


  if((rc = _cmUiFindCtl(p,appId,id,&ctl,true)) == kOkUiRC )
  {
    ctl->arg.ival = enableFl ? 1 : 0;

    rc = _cmUiCallDriver(p, kEnableDId, ctl, kIvalUiFl );
  }

  return rc;
}

cmUiRC_t cmUiEnableAllExceptV( cmUiH_t uiH, unsigned appId, unsigned panelId, bool enableFl, va_list vl )
{
  va_list  vl0;
  cmUiRC_t rc = kOkUiRC;
  cmUi_t*  p  = _cmUiHandleToPtr(uiH);
  unsigned n  = 0;
  cmUiApp_t* ap = NULL;
  unsigned i,j;

  // get a count of exception id's
  va_copy(vl0,vl);
  while( va_arg(vl0,unsigned) != cmInvalidId )
    ++n;
  va_end(vl0);
  
  if( n == 0 )
    return rc;

  // alloc array to hold exception id's
  unsigned id[n];

  // store the exception id's
  for(i=0; i<n; ++i)
    id[i] = va_arg(vl,unsigned);

  // locate the current app recd
  if((rc = _cmUiFindApp(p,appId,&ap,true)) != kOkUiRC )
    return rc;

  // get a count of ctl's assigned to panel
  unsigned ctlN = cmArrayCount(ap->ctlArrH);

  // for each control
  for(i=0; i<ctlN; ++i)
  {
    // get the control recd
    cmUiCtl_t* ctl;
    if((ctl = cmArrayPtr(cmUiCtl_t,ap->ctlArrH,i))!=NULL && ctl->panelId == panelId && ctl->usrId != panelId )
    {  
      bool exceptFl = false;

      // check if this control is in the except list
      for(j=0; j<n; ++j)
        if( id[j] == i )
        {
          exceptFl = true;
          break;
        }
     
      // if the ctl is not in the except list then enable/disable it.
      if( !exceptFl )
      {
        ctl->arg.ival = enableFl ? 1 : 0;
        
        cmUiRC_t rc0;
        if((rc0 = _cmUiCallDriver(p, kEnableDId, ctl, kIvalUiFl )) != kOkUiRC )
          rc = rc0;

      }
    }
  }


  return rc;
}


cmUiRC_t cmUiEnableAllExcept( cmUiH_t uiH, unsigned appId, unsigned panelId, bool enableFl,  ... )
{
  va_list vl;
  va_start(vl,enableFl);
  cmUiRC_t rc = cmUiEnableAllExceptV(uiH,appId,panelId,enableFl,vl);
  va_end(vl);
  return rc;
}

cmUiRC_t cmUiDestroyCtl( cmUiH_t uiH, unsigned appId, unsigned id )
{
  cmUiRC_t     rc = kOkUiRC;
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*   ctl = NULL;

  if((rc = _cmUiFindCtl(p,appId,id,&ctl,true)) == kOkUiRC )
  {
    if( ctl->cId == kPanelUiCId )
      _cmUiDestroyPanel(p,appId,ctl->usrId);
    else
      rc = _cmUiDestroyCtl(p,ctl,true);
  }
  return rc;
}

bool cmUiCtlExists(     cmUiH_t uiH, unsigned appId, unsigned panelId, unsigned id )
{
  cmUi_t*  p  = _cmUiHandleToPtr(uiH);

  if( panelId == id )
  {
    cmUiPanel_t* pp=NULL;
    return _cmUiFindPanel(p, appId,panelId, &pp, false ) == kOkUiRC;
  }

  cmUiCtl_t* ctl=NULL;
  return _cmUiFindCtl(p,appId,id,&ctl,false) == kOkUiRC;
}

cmUiRC_t cmUiClearPanel( cmUiH_t uiH, unsigned appId, unsigned panelId )
{
  cmUiRC_t     rc = kOkUiRC;
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp = NULL;

  // get the panel recd ptr
  if((rc = _cmUiFindPanel(p,appId,panelId,&pp,true)) != kOkUiRC )
    return rc;

  cmUiApp_t* ap  = pp->appPtr;
  cmUiCtl_t* ctl = NULL;
  unsigned   i   = 0;
  unsigned   n   = cmArrayCount(ap->ctlArrH);
  cmUiRC_t   rc0;

  // Destroy all controls that belong to this panel.
  for(i=0; i<n; ++i)
    if( _cmUiFindCtl(p,appId,i,&ctl,false) == kOkUiRC && ctl != NULL && ctl->panelId == panelId && ctl->usrId != panelId)
      if((rc0 = _cmUiDestroyCtl(p,ctl,true)) != kOkUiRC )
        rc = rc0;

  // restore the layout variables to their default values.
  _cmUiPanelSetDefaultValues(pp);

  return rc;
}

cmUiRC_t cmUiSelectPanel( cmUiH_t uiH, const cmChar_t* label )
{
  cmUiRC_t rc = kOkUiRC;
  //cmUi_t*  p  = _cmUiHandleToPtr(uiH);

  


  return rc;
  
}


cmUiRC_t cmUiNextRect(    cmUiH_t uiH, unsigned appId, unsigned panelId, int x, int y, int w, int h )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  cmUiRC_t     rc;

  if((rc = _cmUiFindPanel(p, appId, panelId, &pp, true)) != kOkUiRC )
    return rc;
 
  pp->rect.x = x;
  pp->rect.y = y;
  pp->rect.w = w;
  pp->rect.h = h;
  pp->flags = cmSetFlag(pp->flags,kUseRectUiFl);

  return rc;
}

cmUiRC_t     cmUiPrevRect( cmUiH_t uiH, unsigned appId, unsigned panelId, int* xRef, int* yRef, int* wRef, int* hRef )
{
  cmUi_t* p  = _cmUiHandleToPtr(uiH);

  cmUiPanel_t* pp;
  cmUiRC_t     rc;

  if((rc = _cmUiFindPanel(p, appId, panelId, &pp, true)) != kOkUiRC )
    return rc;

  if( xRef != NULL )
    *xRef = pp->prevRect.x;

  if( yRef != NULL )
  *yRef = pp->prevRect.y;

  if( wRef != NULL )
    *wRef = pp->prevRect.w;

  if( hRef != NULL )
    *hRef = pp->prevRect.h;

  return rc;
}

bool    cmUiFillRows( cmUiH_t uiH, unsigned appId, unsigned panelId )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;

  if( _cmUiFindPanel(p, appId, panelId, &pp, true) != kOkUiRC )
    return false;

  return cmIsFlag(pp->flags,kFillRowsUiFl);
}

bool    cmUiSetFillRows( cmUiH_t uiH, unsigned appId, unsigned panelId, bool enableFl )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  

  if( _cmUiFindPanel(p, appId, panelId, &pp, true) != kOkUiRC )
    return false;

  bool retFl = cmIsFlag(pp->flags,kFillRowsUiFl);

  pp->flags = cmEnaFlag(pp->flags,kFillRowsUiFl,enableFl);

  return retFl;
}

void     cmUiPlaceRight( cmUiH_t uiH, unsigned appId, unsigned panelId )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if(_cmUiFindPanel(p, appId, panelId, &pp, true) != kOkUiRC )
    return;

  pp->flags = cmClrFlag(pp->flags,kPlaceBelowUiFl);
  pp->flags = cmSetFlag(pp->flags,kPlaceRightUiFl);
}

void     cmUiPlaceBelow( cmUiH_t uiH, unsigned appId, unsigned panelId )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;

  if(_cmUiFindPanel(p, appId, panelId, &pp, true) != kOkUiRC )
    return;

  pp->flags = cmClrFlag(pp->flags,kPlaceRightUiFl);
  pp->flags = cmSetFlag(pp->flags,kPlaceBelowUiFl);
}

void     cmUiNewLine(    cmUiH_t uiH, unsigned appId, unsigned panelId )
{
  cmUiSetBaseRow( uiH, appId, panelId, cmUiPrevB(uiH,appId,panelId) + cmUiNextVBorder(uiH,appId,panelId) );
  cmUiSetBaseCol( uiH, appId, panelId, cmUiBaseCol(uiH,appId,panelId));
}

int      cmUiBaseCol(       cmUiH_t uiH, unsigned appId, unsigned panelId )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if(_cmUiFindPanel(p, appId, panelId, &pp, true) != kOkUiRC )
    return -1;

  return pp->baseCol;
}

int      cmUiSetBaseCol(    cmUiH_t uiH, unsigned appId, unsigned panelId, int x )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;

  if( _cmUiFindPanel(p, appId, panelId, &pp, true) != kOkUiRC )
    return -1;

  int bc = pp->baseCol;
  pp->baseCol  = x;
  pp->flags    = cmSetFlag(pp->flags,kPlaceBaseRowUiFl);
  return bc;
}

int      cmUiBaseRow(       cmUiH_t uiH, unsigned appId, unsigned panelId )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if(_cmUiFindPanel(p, appId, panelId, &pp, true) != kOkUiRC )
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                return -1;

  return pp->baseRow;
}

int      cmUiSetBaseRow(    cmUiH_t uiH, unsigned appId, unsigned panelId, int y )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  cmUiRC_t     rc;

  if((rc = _cmUiFindPanel(p, appId, panelId, &pp, true)) != kOkUiRC )
    return -1;

  int br = pp->baseRow;
  pp->baseRow  = y;
  return br;
}



int      cmUiW( cmUiH_t uiH, unsigned appId, unsigned panelId )         
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;

  if(_cmUiFindPanel(p, appId, panelId, &pp, true) != kOkUiRC )
    return -1;

  return pp->dfltW;
}

int      cmUiH( cmUiH_t uiH, unsigned appId, unsigned panelId )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;

  if(_cmUiFindPanel(p, appId, panelId, &pp, true) != kOkUiRC )
    return -1;

  return pp->dfltH;
}

int      cmUiSetW(   cmUiH_t uiH, unsigned appId, unsigned panelId, int w )
{ 
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if(_cmUiFindPanel(p, appId, panelId, &pp, true) != kOkUiRC )
    return -1;
  
  int rv = pp->dfltW;
  pp->dfltW = w;
  pp->nextW = w;
  return rv;
}

int      cmUiSetH(   cmUiH_t uiH, unsigned appId, unsigned panelId, int h )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;

  if(_cmUiFindPanel(p, appId, panelId, &pp, true) != kOkUiRC )
    return -1;
  
  int rv = pp->dfltH;
  pp->dfltH = h;
  pp->nextW = h;
  return rv;
}

void     cmUiSetWH(  cmUiH_t uiH, unsigned appId, unsigned panelId, int w, int h )
{
  cmUiSetW( uiH, appId, panelId, w );
  cmUiSetH( uiH, appId, panelId, h );
}

int     cmUiNextW(  cmUiH_t uiH, unsigned appId, unsigned panelId )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if(_cmUiFindPanel(p, appId, panelId, &pp, true) != kOkUiRC )
    return -1;

  return pp->nextW;
}

int     cmUiNextH(  cmUiH_t uiH, unsigned appId, unsigned panelId )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;

  if(_cmUiFindPanel(p, appId, panelId, &pp, true) != kOkUiRC )
    return -1;

  return pp->nextH;
}

void     cmUiSetNextW(  cmUiH_t uiH, unsigned appId, unsigned panelId, int w )
{ 
  return cmUiSetNextWH( uiH, appId, panelId, w, cmUiNextH(uiH,appId,panelId)); 
}

void     cmUiSetNextH(  cmUiH_t uiH, unsigned appId, unsigned panelId, int h )
{ 
  return cmUiSetNextWH( uiH, appId, panelId, cmUiNextW(uiH,appId,panelId), h); 
}

void     cmUiSetNextWH( cmUiH_t uiH, unsigned appId, unsigned panelId, int w, int h )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if(_cmUiFindPanel(p, appId, panelId, &pp, true) != kOkUiRC )
    return;

  pp->nextW = w;
  pp->nextH = h;
  pp->flags = cmSetFlag(pp->flags,kNextWHUiFl);
}

int      cmUiHBorder( cmUiH_t uiH, unsigned appId, unsigned panelId )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if(_cmUiFindPanel(p, appId, panelId, &pp, true) != kOkUiRC )
    return -1;

  return pp->dfltHBorder;
}

int      cmUiVBorder( cmUiH_t uiH, unsigned appId, unsigned panelId )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  if(_cmUiFindPanel(p, appId, panelId, &pp, true) != kOkUiRC )
    return -1;

  return pp->dfltVBorder;
}

int      cmUiSetHBorder( cmUiH_t uiH, unsigned appId, unsigned panelId, int w )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if(_cmUiFindPanel(p, appId, panelId, &pp, true) != kOkUiRC )
    return -1;
  
  int rv = pp->dfltHBorder;
  pp->dfltHBorder = w;
  return rv;
}

int      cmUiSetVBorder( cmUiH_t uiH, unsigned appId, unsigned panelId, int h )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;

  if(_cmUiFindPanel(p, appId, panelId, &pp, true) != kOkUiRC )
    return -1;

  int rv = pp->dfltVBorder;
  pp->dfltVBorder = h;
  return rv;
}

int      cmUiNextHBorder( cmUiH_t uiH, unsigned appId, unsigned panelId )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;

  if(_cmUiFindPanel(p, appId, panelId, &pp, true) != kOkUiRC )
    return -1;

  return pp->nextHBorder;
}

int      cmUiNextVBorder( cmUiH_t uiH, unsigned appId, unsigned panelId )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;

  if(_cmUiFindPanel(p, appId, panelId, &pp, true) != kOkUiRC )
    return -1;

  return pp->nextVBorder;
}

int      cmUiSetNextHBorder( cmUiH_t uiH, unsigned appId, unsigned panelId, int w )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;

  if(_cmUiFindPanel(p, appId, panelId, &pp, true) != kOkUiRC )
    return -1;

  int rv = pp->nextHBorder;
  pp->nextHBorder = w;
  return rv;
}

int      cmUiSetNextVBorder( cmUiH_t uiH, unsigned appId, unsigned panelId, int h )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;

  if(_cmUiFindPanel(p, appId, panelId, &pp, true) != kOkUiRC )
    return -1;

  int rv = pp->nextVBorder;
  pp->nextVBorder = h;
  return rv;
}

int      cmUiPrevL(    cmUiH_t uiH, unsigned appId, unsigned panelId )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if(_cmUiFindPanel(p, appId, panelId, &pp, true) != kOkUiRC )
    return -1;

  return pp->prevRect.x;
}

int      cmUiPrevT(    cmUiH_t uiH, unsigned appId, unsigned panelId )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if(_cmUiFindPanel(p, appId, panelId, &pp, true) != kOkUiRC )
    return -1;

  return pp->prevRect.y;
}

int      cmUiPrevR(    cmUiH_t uiH, unsigned appId, unsigned panelId )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
 
  if(_cmUiFindPanel(p, appId, panelId, &pp, true) != kOkUiRC )
    return -1;

  return pp->prevRect.x + pp->prevRect.w;
}

int      cmUiPrevB(    cmUiH_t uiH, unsigned appId, unsigned panelId )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if(_cmUiFindPanel(p, appId, panelId, &pp, true) != kOkUiRC )
    return -1;

  return pp->prevRect.y + pp->prevRect.h;
}
int      cmUiPrevW(    cmUiH_t uiH, unsigned appId, unsigned panelId )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
 
  if(_cmUiFindPanel(p, appId, panelId, &pp, true) != kOkUiRC )
    return -1;

  return pp->prevRect.w;
}

int      cmUiPrevH(    cmUiH_t uiH, unsigned appId, unsigned panelId )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if(_cmUiFindPanel(p, appId, panelId, &pp, true) != kOkUiRC )
    return -1;

  return pp->prevRect.h;
}

cmUiRC_t  cmUiSetInt(    cmUiH_t uiH,  unsigned appId, unsigned id, int v )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*   ctl;
  cmUiRC_t     rc;
  
  if((rc = _cmUiFastFindCtl(p,appId,id,&ctl,true)) != kOkUiRC )
    return rc;

  return _cmUiSetDriverValueInt(p,ctl,kValUiFl,v);

}

cmUiRC_t  cmUiSetUInt(   cmUiH_t uiH,  unsigned appId, unsigned id, unsigned v )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*   ctl;
  cmUiRC_t     rc;

  if((rc = _cmUiFastFindCtl(p,appId,id,&ctl,true)) != kOkUiRC )
    return rc;

  return _cmUiSetDriverValueInt(p,ctl,kValUiFl,v);
}

cmUiRC_t cmUiSetDouble( cmUiH_t uiH,  unsigned appId, unsigned id, double v )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*   ctl;
  cmUiRC_t     rc;

  if((rc = _cmUiFastFindCtl(p,appId,id,&ctl,true)) != kOkUiRC )
    return rc;

  return _cmUiSetDriverValueDouble(p,ctl,kValUiFl,v);
}

cmUiRC_t  cmUiSetString( cmUiH_t uiH,  unsigned appId, unsigned id, const cmChar_t* v )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*   ctl;
  cmUiRC_t     rc;

  if((rc = _cmUiFastFindCtl(p,appId,id,&ctl,true)) != kOkUiRC )
    return rc;

  return _cmUiSetDriverValueStr(p,ctl,kValUiFl,v);
}

cmUiRC_t cmUiSetVPrintf( cmUiH_t uiH, unsigned appId, unsigned id, const cmChar_t* fmt, va_list vl )
{
  va_list vl1;
  va_copy(vl1,vl);
  unsigned n = vsnprintf(NULL,0,fmt,vl1);
  va_end(vl1);

  unsigned  sbufN = 2047;
  cmChar_t  sbuf[ sbufN+1 ];
  cmChar_t* b;
  unsigned  bn;
  cmChar_t* abuf = NULL;

  if( n >= sbufN )
  {
    bn   = n + 1;
    abuf = cmMemAlloc(cmChar_t,bn);
    b    = abuf;
  }
  else
  {
    bn = sbufN;
    b  = sbuf;
  }

  vsnprintf(b,bn,fmt,vl);
  cmUiRC_t rc = cmUiSetString(uiH,appId,id,b);
  cmMemFree(abuf);
  return rc;
}


cmUiRC_t  cmUiSetPrintf( cmUiH_t uiH,  unsigned appId, unsigned id, const cmChar_t* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  cmUiRC_t rc = cmUiSetVPrintf(uiH,appId,id,fmt,vl);
  va_end(vl);
  return rc;
}


int  cmUiInt(cmUiH_t uiH, unsigned appId, unsigned id )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*   ctl;
  cmUiRC_t     rc;

  if((rc = _cmUiFastFindCtl(p,appId,id,&ctl,true)) != kOkUiRC )
    return 0;

  return ctl->getInt(ctl);
}

unsigned        cmUiUInt(   cmUiH_t uiH, unsigned appId,  unsigned id )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*   ctl;
  cmUiRC_t     rc;

  if((rc = _cmUiFastFindCtl(p,appId,id,&ctl,true)) != kOkUiRC )
    return 0;

  return ctl->getInt(ctl);
}

double          cmUiDouble( cmUiH_t uiH, unsigned appId,  unsigned id )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*   ctl;
  cmUiRC_t     rc;
  
  if((rc = _cmUiFastFindCtl(p,appId,id,&ctl,true)) != kOkUiRC )
    return 0;

  return ctl->getDbl(ctl);
}

const cmChar_t* cmUiString( cmUiH_t uiH, unsigned appId,  unsigned id )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*   ctl;
  cmUiRC_t     rc;

  if((rc = _cmUiFastFindCtl(p,appId,id,&ctl,true)) != kOkUiRC )
    return 0;

  return ctl->getStr(ctl);
}

unsigned        cmUiListEleCount( cmUiH_t uiH, unsigned appId, unsigned id )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*   ctl;
  cmUiRC_t     rc;

  if((rc = _cmUiFastFindCtl(p,appId,id,&ctl,true)) != kOkUiRC )
    return 0;

  return cmArrayCount( ctl->idArrH );
}

unsigned        cmUiListEleId(    cmUiH_t uiH, unsigned appId, unsigned id, unsigned index )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*   ctl;
  cmUiRC_t     rc;

  if((rc = _cmUiFastFindCtl(p,appId,id,&ctl,true)) != kOkUiRC )
    return 0;

  if( !cmArrayIsValid( ctl->idArrH ) || index >= cmArrayCount( ctl->idArrH) )
    return cmInvalidId;

  return cmArrayPtr(cmUiListEle_t, ctl->idArrH, index )->id;
}

const cmChar_t* cmUiListEleLabel( cmUiH_t uiH, unsigned appId, unsigned id, unsigned index )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*   ctl;
  cmUiRC_t     rc;
  
  if((rc = _cmUiFastFindCtl(p,appId,id,&ctl,true)) != kOkUiRC )
    return 0;

  if( !cmArrayIsValid( ctl->idArrH ) || index >= cmArrayCount( ctl->idArrH) )
    return NULL;

  return cmArrayPtr(cmUiListEle_t, ctl->idArrH, index )->label;
}

unsigned cmUiListEleLabelToIndex( cmUiH_t uiH, unsigned appId, unsigned id, const cmChar_t* label )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*   ctl;
  cmUiRC_t     rc;
  unsigned     i,n;

  if( label == NULL )
    return cmInvalidIdx;

  if((rc = _cmUiFastFindCtl(p,appId,id,&ctl,true)) != kOkUiRC )
    return cmInvalidIdx;

  if( cmArrayIsValid( ctl->idArrH )==false || (n = cmArrayCount( ctl->idArrH))==0 )
    return cmInvalidIdx;

  for(i=0; i<n; ++i)
    if( cmTextCmp(cmArrayPtr(cmUiListEle_t, ctl->idArrH, i )->label,label) == 0 )
      return i;

  return cmInvalidIdx;
}



cmUiRC_t   cmUiLastRC( cmUiH_t uiH )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  return cmErrLastRC(&p->err);
}

cmUiRC_t   cmUiSetRC( cmUiH_t uiH, cmUiRC_t rc )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  return cmErrSetRC(&p->err, rc);
}

