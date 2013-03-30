#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmArray.h"
#include "cmJson.h"
#include "cmUiDrvr.h"
#include "cmUi.h"


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
  unsigned   appId;         // app id and index into cmUI_t.appArrH[].
  bool       activeFl;      // true if this app recd is active and valid
  cmArrayH_t ctlArrH;       // cmUiCtl_t[]
  cmArrayH_t pnlArrH;       // cmUiPanel_t[]
} cmUiApp_t;

typedef struct cmUiPanel_str
{
  cmUiApp_t* appPtr;            // owner app.
  unsigned   flags;             // See kXXXUiFl above
  unsigned   usrId;             // app. supplied id
  cmUiRect_t rect;              // rect to position next control (only used if kUseRectUiFl is set)
  cmUiRect_t prevRect;
  int        baseCol;
  int        baseRow;
  int        dfltW;
  int        dfltH;
  int        nextW;
  int        nextH;
  int        dfltHBorder;
  int        dfltVBorder;
  int        nextHBorder;
  int        nextVBorder;
  
} cmUiPanel_t;


typedef struct cmUiCtl_str
{
  cmUiCId_t           cId;     // control type and used to decode the value union below.
  unsigned            usrId;   // control instance id and index into cmUiApp_t.ctlArrH[].
  unsigned            panelId; // panel this ctl belongs to
  cmArrayH_t          idArrH;  // id's associated with each sub-element of this control (used by menu's, list's, etc...)
  
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
  cmErr_t           err;       //
  cmCtx_t*          ctx;       // Global context. 
  cmUiDriverFunc_t  drvr;      // Driver callback function
  void*             drvrArg;   // Driver callback function arg.
  int               panelW;    // Panel width.
  int               panelH;    // Panel height
  unsigned          curAppId;  // Id of application currently receiveing commands/queries.
  cmUiDriverFunc_t  cbFunc;    // Client callback function
  void *            cbArg;     //
  cmArrayH_t        appArrH;   // cmUiApp_t[]
} cmUi_t;

cmUiH_t cmUiNullHandle  = cmSTATIC_NULL_HANDLE;

cmUi_t* _cmUiHandleToPtr( cmUiH_t h )
{
  cmUi_t* p = (cmUi_t*)h.h;
  assert(p != NULL );
  return p;
}

void _cmUiDriverArgInit( cmUiDriverArg_t* a, cmUi_t* p, cmUiDId_t dId, cmUiCId_t cId, unsigned panelId, unsigned usrId )
{
  memset(a,0,sizeof(*a));
  a->cbArg   = p->drvrArg;
  a->dId     = dId;
  a->cId     = cId;
  a->appId   = p->curAppId;
  a->panelId = panelId;
  a->usrId   = usrId;
  a->flags   = 0;
  a->x       = -1;
  a->y       = -1;
  a->w       = -1;
  a->h       = -1;
  a->ival    = 0;
  a->fval    = 0;
  a->sval    = NULL;
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

cmUiRC_t  _cmUiFindApp( cmUi_t* p, unsigned appId, cmUiApp_t** appRef, bool errFl )
{
  cmUiRC_t  rc = kOkUiRC;
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


cmUiRC_t  _cmUiFindCtl( cmUi_t* p, unsigned usrId, cmUiCtl_t** ctlRef, bool errFl )
{
  cmUiRC_t     rc  = kOkUiRC;
  cmUiApp_t*   ap  = NULL;
  cmUiCtl_t*   ctl = NULL;

  // find the app this ctl belongs to
  if(( rc =_cmUiFindApp(p,p->curAppId,&ap,errFl)) != kOkUiRC )
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

cmUiRC_t _cmUiFastFindCtl( cmUi_t* p, unsigned usrId, cmUiCtl_t** ctlRef, bool errFl )
{
  assert( p->curAppId < cmArrayCount(p->appArrH) );
  
  cmUiApp_t* ap = cmArrayPtr(cmUiApp_t,p->appArrH,p->curAppId);

  assert( ap->activeFl && usrId < cmArrayCount(ap->ctlArrH) );
  
  *ctlRef = cmArrayPtr(cmUiCtl_t,ap->ctlArrH,usrId);

  assert( (*ctlRef)->usrId == usrId );

  return kOkUiRC;
}

cmUiRC_t  _cmUiFindPanel( cmUi_t* p, unsigned panelId, cmUiPanel_t** ppRef, bool errFl )

{
  cmUiRC_t     rc  = kOkUiRC;
  cmUiApp_t*   ap  = NULL;
  unsigned     i,n;

  *ppRef = NULL;

  // find the app this ctl belongs to
  if(( rc =_cmUiFindApp(p,p->curAppId,&ap,errFl)) != kOkUiRC )
    goto errLabel;

  n = cmArrayCount(ap->pnlArrH);
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

cmUiRC_t _cmUiCallDriver( cmUi_t* p, cmUiDriverArg_t* a )
{
  cmUiRC_t rc = kOkUiRC;
  if( p->drvr != NULL )
    if((rc = p->drvr(a)) != kOkUiRC )
      rc = cmErrMsg(&p->err,kDrvrErrUiRC,"UI manager driver error.");
  return rc;
}

cmUiRC_t _cmUiDestroyDrvrCtl( cmUi_t* p, cmUiCId_t cId, unsigned panelId, unsigned usrId )
{
  cmUiDriverArg_t a;
  _cmUiDriverArgInit(&a, p, kDestroyCtlDId, cId, panelId, usrId );


  return  _cmUiCallDriver(p,&a);
}

cmUiRC_t  _cmUiDestroyCtl( cmUi_t* p, cmUiCtl_t* ctl )
{
  cmUiRC_t rc = kOkUiRC;

  rc =_cmUiDestroyDrvrCtl(p,ctl->cId,ctl->panelId,ctl->usrId);

  switch(ctl->cId)
  {
    case kLabelUiCId:
    case kTextUiCId:
    case kFilenameUiCId:
    case kDirUiCId:
      cmMemFree(ctl->u.sval);
      break;

    default:
      break;
  }

  ctl->usrId   = cmInvalidId;
  ctl->panelId = cmInvalidId;
  cmArrayRelease(&ctl->idArrH);

  return rc;
}

cmUiRC_t _cmUiDestroyPanel( cmUi_t* p, unsigned panelId )
{
  cmUiRC_t rc = kOkUiRC;
  cmUiPanel_t* pp = NULL;

  // get the panel recd ptr
  if((rc = _cmUiFindPanel(p,panelId,&pp,true)) != kOkUiRC )
    return rc;

  cmUiApp_t* ap = pp->appPtr;

  // notify the driver to destroy the panel
  if((rc = _cmUiDestroyDrvrCtl(p,kPanelUiCId,panelId,panelId)) != kOkUiRC )
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
    if( _cmUiFindCtl(p,i,&ctl,false) == kOkUiRC && ctl != NULL && ctl->panelId == panelId)
      if((rc0 = _cmUiDestroyCtl(p,ctl)) != kOkUiRC )
        rc = rc0;

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
  unsigned   orgAppId = p->curAppId;

  p->curAppId = appId;
  
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
    cmUiPanel_t* pp = cmArrayPtr(cmUiPanel_t,ap->pnlArrH,i);
    if( pp->usrId != cmInvalidId )
      if((rc0 = _cmUiDestroyPanel(p,pp->usrId)) != kOkUiRC )
        rc = rc0;
  }

  ap->appId    = -1;
  ap->activeFl = false;
  cmArrayRelease(&ap->ctlArrH);
  cmArrayRelease(&ap->pnlArrH);

 errLabel:
  p->curAppId = orgAppId;

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
      rc = rc0;
  }

  cmArrayClear(p->appArrH,false);

  return rc;
}


cmUiRC_t _cmUiGetCtlXYWH( cmUi_t* p, cmUiDriverArg_t* a, cmUiPanel_t* pp )
{
  if( cmIsFlag(pp->flags,kUseRectUiFl ) )
  {
    pp->flags = cmClrFlag(pp->flags,kUseRectUiFl);
    a->x      = pp->rect.x;
    a->y      = pp->rect.y;
    a->w      = pp->rect.w;
    a->h      = pp->rect.h;
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
    a->x = pp->baseCol; // ... or down columns

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

  a->w           = pp->nextW;
  a->h           = pp->nextH;

  pp->prevRect.x = a->x;
  pp->prevRect.y = a->y;
  pp->prevRect.w = a->w;
  pp->prevRect.h = a->h; 

  pp->nextW      = pp->dfltW;
  pp->nextH      = pp->dfltH;
  pp->nextHBorder= pp->dfltHBorder;
  pp->nextVBorder= pp->dfltVBorder;

  pp->flags      = cmClrFlag(pp->flags,kNextWHUiFl | kPlaceRightUiFl | kPlaceBelowUiFl | kPlaceBaseRowUiFl );

  return kOkUiRC;
}


cmUiRC_t _cmUiCreateCtl( cmUi_t* p, cmUiDriverArg_t* a, unsigned panelId, cmUiCId_t cId, unsigned usrId, const cmChar_t* label, unsigned flags, cmUiCtl_t** ctlRef )
{
  cmUiRC_t     rc;
  cmUiPanel_t* pp = NULL;
  cmUiApp_t*   ap = NULL;

  if( ctlRef != NULL )
    *ctlRef = NULL;

  // initialize the driver arg record
  _cmUiDriverArgInit(a, p, kCreateCtlDId, cId, panelId, usrId );
 
  // locate the app
  if((rc = _cmUiFindApp(p,p->curAppId,&ap,true)) != kOkUiRC )
    return rc;

  // locate the panel the control belongs to
  if((rc = _cmUiFindPanel(p,panelId,&pp,true)) != kOkUiRC )
    return rc; 

  // calc the control location - for non-panel controls or
  // for panel controls using the custom 'next rect'.
  if( cId != kPanelUiCId || cmIsFlag(pp->flags,kUseRectUiFl ) )
    if((rc = _cmUiGetCtlXYWH(p,a,pp)) != kOkUiRC )
      return rc;

  // get the new ctl record
  cmUiCtl_t* ctl = cmArrayClr(cmUiCtl_t,ap->ctlArrH,usrId);

  // setup the new ctl record
  ctl->cId     = cId;
  ctl->usrId   = usrId;
  ctl->panelId = panelId;
  cmArrayAlloc(p->ctx,&ctl->idArrH,sizeof(unsigned));
  
  a->sval    = label;
  a->flags   = flags;

  if( ctlRef != NULL )
    *ctlRef = ctl;

  return  _cmUiCallDriver(p,a);
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

  if( hp == NULL || cmUiIsValid(*hp)==false )
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
  cmUi_t*  p  = _cmUiHandleToPtr(h);
  p->drvr    = drvrFunc;
  p->drvrArg = drvrArg;
}


cmUiRC_t cmUiCreateApp(  cmUiH_t h, unsigned appId )
{
  cmUiRC_t        rc = kOkUiRC;

  if( cmUiIsValid(h)==false)
    return rc;

  cmUi_t*         p  = _cmUiHandleToPtr(h);
  cmUiApp_t*      ap = NULL;

  // verify that the requested app does not exist
  if( _cmUiFindApp(p,appId, &ap, false ) == kOkUiRC )
    return cmErrMsg(&p->err,kInvalidIdUiRC,"The id (%i) of the new application is already in use.",appId);

  ap           = cmArrayClr(cmUiApp_t,p->appArrH,appId);
  ap->appId    = appId;
  ap->activeFl = true;
  cmArrayAlloc(p->ctx,&ap->ctlArrH,sizeof(cmUiCtl_t));
  cmArrayAlloc(p->ctx,&ap->pnlArrH,sizeof(cmUiPanel_t));


  return rc;  
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

cmUiRC_t  cmUiSetAppId( cmUiH_t h, unsigned appId )
{
  cmUiRC_t   rc = kOkUiRC;

  if( cmUiIsValid(h)==false)
    return rc;

  cmUi_t*    p  = _cmUiHandleToPtr(h);
  cmUiApp_t* ap = NULL;

  // verify that the requested app exists
  if( appId != cmInvalidId )
    if((rc = _cmUiFindApp(p,appId, &ap, true )) != kOkUiRC )
      return rc;

  p->curAppId = appId;
  return rc;
}

unsigned cmUiAppCount( cmUiH_t h )
{
  if( cmUiIsValid(h)==false)
    return 0;

  cmUi_t*    p  = _cmUiHandleToPtr(h);

  return cmArrayCount(p->appArrH);
}



cmUiRC_t cmUiOnDriverEvent( cmUiH_t h, const cmUiDriverArg_t* arg )
{
  cmUiRC_t        rc = kOkUiRC;
  cmUi_t*         p  = _cmUiHandleToPtr(h);
  cmUiDriverArg_t a = *arg;
  cmUiCtl_t*      ctl;

  if((rc = cmUiSetAppId(h,arg->appId)) != kOkUiRC )
    return rc;
  
  a.cbArg = p->cbArg;

  if((rc = _cmUiFindCtl(p,a.usrId,&ctl,true)) != kOkUiRC )
    goto errLabel;
    
  switch( a.cId )
  {
    case kInvalidUiCId:  break;
    case kPanelUiCId:    break;
    case kBtnUiCId:      break;
    case kCheckUiCId:    
      ctl->u.ival = a.ival; 
      break;

    case kMenuBtnUiCId:  
    case kListUiCId:     
      {
        ctl->u.ival = a.ival; 
        if(a.ival >= cmArrayCount(ctl->idArrH))
        {
          rc = cmErrMsg(&p->err,kInvalidIdUiRC,"Invalid menu or list driver element id=%i element count:%i.",a.ival,cmArrayCount(ctl->idArrH));
          goto errLabel;
        }

        a.ival = cmArrayEle(unsigned,ctl->idArrH,a.ival);
        //a.ival = ctl->id_arr[ a.ival ];
      }
      break;

    case kLabelUiCId:    
      ctl->u.sval = cmMemResizeStr(ctl->u.sval,cmStringNullGuard(a.sval)); 
      break;

    case kTextUiCId:     
      ctl->u.sval = cmMemResizeStr(ctl->u.sval,cmStringNullGuard(a.sval)); 
      break;

    case kSliderUiCId:
    case kNumberUiCId:   
      ctl->u.fval = a.fval; 
      a.cId = kNumberUiCId; // sliders callback the client as numbers
      break;

    case kProgressUiCId: 
      ctl->u.ival = a.ival; 
      break;

    case kMeterUiCId:    
      ctl->u.fval = a.fval; 
      break;

    case kFilenameUiCId: 
      ctl->u.sval = cmMemResizeStr(ctl->u.sval,cmStringNullGuard(a.sval)); 
      break;

    case kDirUiCId:      
      ctl->u.sval = cmMemResizeStr(ctl->u.sval,cmStringNullGuard(a.sval)); 
      break;
  }

  rc = p->cbFunc(&a);

 errLabel:
  cmUiSetAppId(h,cmInvalidId);

  return rc;
}

cmUiRC_t cmUiCreatePanel(   cmUiH_t uiH, unsigned newPanelId, const cmChar_t* label )
{
  cmUiRC_t        rc;
  cmUiDriverArg_t a;
  cmUi_t*         p    = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*      ctl  = NULL;
  cmUiApp_t*      ap   = NULL;

  if(( rc = _cmUiFindApp(p,p->curAppId,&ap,true)) != kOkUiRC )
    return rc;

  cmUiPanel_t*    pp   = cmArrayPush(ap->pnlArrH,NULL,1);
  assert( pp != NULL );

  pp->appPtr     = ap;
  pp->usrId      = newPanelId;
  pp->baseCol    = 2;
  pp->baseRow    = 0;
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
  

  if((rc = _cmUiCreateCtl(p, &a, newPanelId, kPanelUiCId, newPanelId, label, 0, NULL )) != kOkUiRC )
  {
    // TODO - destroy panel record here
    return rc;
  }
  
  _cmUiFindCtl(p,newPanelId,&ctl,true);
  
  assert(ctl!=NULL);
  ctl->u.pnl = pp;
  return rc;
}


cmUiRC_t cmUiCreateBtn(     cmUiH_t uiH, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags )
{
  cmUiRC_t       rc;
  cmUi_t*         p = _cmUiHandleToPtr(uiH);
  cmUiDriverArg_t a;
  cmUiCtl_t*      c;

  if((rc = _cmUiCreateCtl(p,&a,panelId,kBtnUiCId,id,label,flags,&c)) == kOkUiRC )
  {
    _cmUiSetIntAccessors(c);
  }
  return rc;
}

cmUiRC_t cmUiCreateCheck(   cmUiH_t uiH, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, bool dflt )
{
  cmUiRC_t       rc;
  cmUi_t*         p = _cmUiHandleToPtr(uiH);
  cmUiDriverArg_t a;
  cmUiCtl_t*      c;
  
  a.ival = dflt;
  if((rc = _cmUiCreateCtl(p,&a,panelId,kCheckUiCId,id,label,flags,&c)) == kOkUiRC )
    _cmUiSetIntAccessors(c);
  return rc;
}

cmUiRC_t cmUiCreateLabel(   cmUiH_t uiH, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags )
{
  cmUiRC_t       rc;
  cmUi_t*         p  = _cmUiHandleToPtr(uiH);
  cmUiDriverArg_t a;
  cmUiCtl_t*      c;

  if((rc = _cmUiCreateCtl(p,&a,panelId,kLabelUiCId,id,label,flags,&c)) == kOkUiRC )
    _cmUiSetStrAccessors(c);
  return rc;
}

cmUiRC_t cmUiCreateText(    cmUiH_t uiH, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, const cmChar_t* text )  
{
  cmUiRC_t        rc = kOkUiRC;
  cmUi_t*         p  = _cmUiHandleToPtr(uiH);
  cmUiDriverArg_t a;
  cmUiCtl_t*      c;

  if(( rc = _cmUiCreateCtl(p,&a,panelId,kTextUiCId,id,label,flags,&c)) == kOkUiRC )
  {
    _cmUiSetStrAccessors(c);

    a.dId   = kSetValDId;
    a.flags = kValUiFl;
    a.sval  = text;
    rc      = _cmUiCallDriver(p,&a);
  }

  return rc;
}

cmUiRC_t _cmUiCreateNumber(  cmUiH_t uiH, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, double min, double max, double incr, double dflt )
{
  cmUiRC_t        rc = kOkUiRC;
  cmUi_t*         p  = _cmUiHandleToPtr(uiH);
  cmUiDriverArg_t a;
  cmUiCId_t       cid = kNumberUiCId;
  cmUiCtl_t*      c;
  cmUiPanel_t*    pp;

  if( cmIsFlag(flags,kVertUiFl|kHorzUiFl) )
  {
    if( cmIsFlag(flags,kVertUiFl) )
    {
      if((rc =  _cmUiFindPanel(p,panelId,&pp,true)) != kOkUiRC )
        return rc;

      // if the size of the control was not excplicitly set 
      // then swap width and height
      if( cmIsNotFlag(pp->flags,kNextWHUiFl) && cmIsNotFlag(pp->flags,kUseRectUiFl) )
        cmUiSetNextWH( uiH, panelId, cmUiH(uiH,panelId), cmUiW(uiH,panelId) );
    }

    cid = kSliderUiCId;
  }

  if(( rc = _cmUiCreateCtl(p,&a,panelId,cid,id,label,flags,&c)) == kOkUiRC )
  {
    cmUiRC_t rc0;

    _cmUiSetDblAccessors(c);

    a.dId   = kSetValDId;
    a.flags = kMinUiFl;
    a.fval  = min;
    if((rc0 = _cmUiCallDriver(p,&a)) != kOkUiRC )
      rc = rc0;

    a.flags = kMaxUiFl;
    a.fval  = max;
    if((rc0 = _cmUiCallDriver(p,&a)) != kOkUiRC )
      rc = rc0;

    a.flags = kIncUiFl;
    a.fval  = incr;
    if((rc0 = _cmUiCallDriver(p,&a)) != kOkUiRC )
      rc = rc0;

    a.flags = kValUiFl;
    a.fval  = dflt;
    if((rc0 = _cmUiCallDriver(p,&a)) != kOkUiRC )
      rc = rc0;

  }

  return rc;
}

cmUiRC_t cmUiCreateNumber(  cmUiH_t uiH, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, double min, double max, double incr, double dflt )
{ 
  return _cmUiCreateNumber(uiH,panelId,id,label,flags,min,max,incr,dflt);  
}

cmUiRC_t cmUiCreateHSlider( cmUiH_t uiH, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, double min, double max, double incr, double dflt )
{ 
  return _cmUiCreateNumber(uiH,panelId,id,label,flags | kHorzUiFl, min,max,incr,dflt);
}

cmUiRC_t cmUiCreateVSlider( cmUiH_t uiH, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, double min, double max, double incr, double dflt )
{
  return _cmUiCreateNumber(uiH,panelId,id,label,flags | kVertUiFl, min,max,incr,dflt);
}


cmUiRC_t cmUiCreateProgress(cmUiH_t uiH, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, int min, int max, int dflt )
{
  cmUiRC_t        rc = kOkUiRC;
  cmUi_t*         p  = _cmUiHandleToPtr(uiH);
  cmUiDriverArg_t a;
  cmUiCtl_t*      c;

  if(( rc = _cmUiCreateCtl(p,&a,panelId,kProgressUiCId,id,label,flags,&c)) == kOkUiRC )
  {
    cmUiRC_t rc0;

    _cmUiSetIntAccessors(c);

    a.dId   = kSetValDId;

    a.flags = kMinUiFl;
    a.ival  = min;
    if((rc0 = _cmUiCallDriver(p,&a)) != kOkUiRC )
      rc = rc0;

    a.flags = kMaxUiFl;
    a.ival  = max;
    if((rc0 = _cmUiCallDriver(p,&a)) != kOkUiRC )
      rc = rc0;

    a.flags = kValUiFl;
    a.ival  = dflt;
    if((rc0 = _cmUiCallDriver(p,&a)) != kOkUiRC )
      rc = rc0;

  }

  return rc;
}

cmUiRC_t _cmUiCreateMeter(   cmUiH_t uiH, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, int min, int max, int dflt) 
{
  cmUiRC_t       rc;
  cmUi_t*         p  = _cmUiHandleToPtr(uiH);
  cmUiDriverArg_t a;
  cmUiCtl_t*      c;
  cmUiPanel_t*   pp;

  if( cmIsFlag(flags,kVertUiFl) )
  {    
    if((rc =  _cmUiFindPanel(p,panelId,&pp,true)) != kOkUiRC )
      return rc;

    // if the size of the control has not been explicitely set 
    // then swap height and width for vertical meters.
    if( cmIsNotFlag(pp->flags,kNextWHUiFl) && cmIsNotFlag(pp->flags,kUseRectUiFl) )
      cmUiSetNextWH( uiH, panelId, cmUiH(uiH,panelId), cmUiW(uiH,panelId) );
  }

  if((rc = _cmUiCreateCtl(p,&a,panelId,kMeterUiCId,id,label,flags,&c)) == kOkUiRC )
  {
    cmUiRC_t rc0;

    _cmUiSetIntAccessors(c);

    a.dId   = kSetValDId;

    a.flags = kMinUiFl;
    a.ival  = min;
    if((rc0 = _cmUiCallDriver(p,&a)) != kOkUiRC )
      rc = rc0;

    a.flags = kMaxUiFl;
    a.ival  = max;
    if((rc0 = _cmUiCallDriver(p,&a)) != kOkUiRC )
      rc = rc0;

    a.flags = kValUiFl;
    a.ival  = dflt;
    if((rc0 = _cmUiCallDriver(p,&a)) != kOkUiRC )
      rc = rc0;

  }
  return rc;
}


cmUiRC_t cmUiCreateHMeter(   cmUiH_t uiH, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, int min, int max, int dflt ) 
{
  return _cmUiCreateMeter(uiH,panelId,id,label,flags | kHorzUiFl,min,max,dflt);
}

cmUiRC_t cmUiCreateVMeter(   cmUiH_t uiH, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, int min, int max, int dflt ) 
{
  return _cmUiCreateMeter(uiH,panelId,id,label,flags | kVertUiFl,min,max,dflt);
}

cmUiRC_t cmUiCreateFileBtn(cmUiH_t uiH, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, const cmChar_t* dfltDir, const cmChar_t* patStr )
{
  cmUiRC_t        rc = kOkUiRC;
  cmUi_t*         p  = _cmUiHandleToPtr(uiH);
  cmUiDriverArg_t a;
  cmUiCtl_t*      c;

  if(( rc = _cmUiCreateCtl(p,&a,panelId,kFilenameUiCId,id,label,flags,&c)) == kOkUiRC )
  {
    cmUiRC_t rc0;

    _cmUiSetStrAccessors(c);

    if( dfltDir != NULL )
    {
      a.dId   = kSetValDId;
      a.flags = kValUiFl;
      a.sval  = dfltDir;
      if((rc0 = _cmUiCallDriver(p,&a)) != kOkUiRC )
        rc = rc0;
    }

    if( patStr != NULL )
    {
      a.dId   = kSetValDId;
      a.flags = kFnPatUiFl;
      a.sval  = patStr;
      if((rc0 = _cmUiCallDriver(p,&a)) != kOkUiRC )
        rc = rc0;
    }
  }
  return rc;
}

cmUiRC_t cmUiCreateDirBtn(  cmUiH_t uiH, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, const cmChar_t* dfltDir )
{
  cmUiRC_t        rc = kOkUiRC;
  cmUi_t*         p  = _cmUiHandleToPtr(uiH);
  cmUiDriverArg_t a;
  cmUiCtl_t*      c;

  if(( rc = _cmUiCreateCtl(p,&a,panelId,kDirUiCId,id,label,flags,&c)) == kOkUiRC )
  {
    cmUiRC_t rc0;

    _cmUiSetStrAccessors(c);

    if( dfltDir != NULL )
    {
      a.dId   = kSetValDId;
      a.flags = kValUiFl;
      a.sval  = dfltDir;
      if((rc0 = _cmUiCallDriver(p,&a)) != kOkUiRC )
        rc = rc0;
    }
  }
  return rc;
}

cmUiRC_t cmUiCreateMenuBtn( cmUiH_t uiH, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags )
{
  cmUiRC_t       rc;
  cmUi_t*         p  = _cmUiHandleToPtr(uiH);
  cmUiDriverArg_t a;
  cmUiCtl_t*      c;

  if((rc = _cmUiCreateCtl(p,&a,panelId,kMenuBtnUiCId,id,label,flags,&c)) == kOkUiRC )
    _cmUiSetIntAccessors(c);
  return rc;
}

cmUiRC_t cmUiCreateMenuBtnV(cmUiH_t uiH, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, const cmChar_t* label0, unsigned id0, va_list vl )
{
  cmUiRC_t rc = kOkUiRC;
  // TODO:
  return rc;
}

cmUiRC_t cmUiCreateMenuBtnA(cmUiH_t uiH, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, const cmChar_t* label0, unsigned id0, ... )
{
  va_list vl;
  va_start(vl,id0);
  cmUiRC_t rc = cmUiCreateMenuBtnV(uiH,panelId,id,label,flags,label0,id0,vl);
  va_end(vl);
  return rc;
}

cmUiRC_t cmUiCreateMenuBtnJson(cmUiH_t uiH, unsigned panelId, unsigned id, const cmChar_t* lavel, unsigned flags, const cmJsonNode_t* root, const cmChar_t* memberLabel )
{
  cmUiRC_t rc = kOkUiRC;
  // TODO:
  return rc;
}

cmUiRC_t cmUiCreateList(cmUiH_t uiH, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, unsigned visibleRowCnt )
{
  cmUi_t*         p    = _cmUiHandleToPtr(uiH);
  cmUiRC_t        rc;
  cmUiDriverArg_t a;
  cmUiCtl_t*      c;
  cmUiPanel_t*    pp;

  if((rc =  _cmUiFindPanel(p,panelId,&pp,true)) != kOkUiRC )
    return rc;

  if( cmIsNotFlag(pp->flags,kNextWHUiFl) )
    cmUiSetNextWH( uiH, panelId, cmUiNextW(uiH,panelId), cmUiH(uiH,panelId) * visibleRowCnt );
  
  if((rc =  _cmUiCreateCtl(p,&a,panelId,kListUiCId,id,label,flags,&c)) == kOkUiRC )
    _cmUiSetIntAccessors(c);
  
  return rc;
}

cmUiRC_t cmUiCreateListV(       cmUiH_t uiH, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, unsigned visibleRowCnt, const cmChar_t* label0, unsigned id0, va_list vl )
{
  cmUiRC_t rc = kOkUiRC;
  // TODO:
  return rc;
}

cmUiRC_t cmUiCreateListA(       cmUiH_t uiH, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, unsigned visibleRowCnt, const cmChar_t* label0, unsigned id0, ... )
{
  va_list vl;
  va_start(vl,id0);
  cmUiRC_t rc = cmUiCreateListV(uiH,panelId,id,label,flags,visibleRowCnt,label0,id0,vl);
  va_end(vl);
  return rc;
}

cmUiRC_t cmUiCreateListJson(    cmUiH_t uiH, unsigned panelId, unsigned id, const cmChar_t* label, unsigned flags, unsigned visibleRowCnt, const cmJsonNode_t* root, const cmChar_t* memberLabel )
{
  cmUiRC_t rc = kOkUiRC;
  // TODO:
  return rc;
}

cmUiRC_t cmUiAppendListEle(    cmUiH_t uiH, unsigned panelId, unsigned id, const cmChar_t* text, unsigned eleId )
{
  cmUiRC_t        rc  = kOkUiRC;
  cmUi_t*         p   = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*      ctl = NULL;
  cmUiDriverArg_t a;

  if((rc = _cmUiFindCtl(p,id,&ctl,true)) != kOkUiRC )
    return rc;

  if( ctl->cId != kListUiCId && ctl->cId != kMenuBtnUiCId )
    return cmErrMsg(&p->err,kInvalidCtlOpUiRC,"List elements may only be set on 'list' and 'menu button' controls.");

  _cmUiDriverArgInit(&a, p, kSetValDId, ctl->cId, panelId, id );
  

  //if( ctl->id_arr == NULL || ctl->id_cnt == ctl->id_alloc )
  //  ctl->id_arr = cmMemResizeZ(unsigned,ctl->id_arr,ctl->id_alloc+=10);

  //ctl->id_arr[ ctl->id_cnt++ ] = eleId;

  cmArrayPush(ctl->idArrH,&eleId,1);

  //a.dId     = kSetValDId;
  //a.cId     = ctl->cId;
  //a.panelId = panelId;
  //a.usrId   = id;
  a.flags   = kAppendUiFl;
  a.sval    = text;
  a.ival    = eleId;

  return _cmUiCallDriver(p,&a);
}

cmUiRC_t cmUiDestroyCtl( cmUiH_t uiH, unsigned id )
{
  cmUiRC_t     rc = kOkUiRC;
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*   ctl = NULL;

  if((rc = _cmUiFindCtl(p,id,&ctl,true)) == kOkUiRC )
  {
    if( ctl->cId == kPanelUiCId )
      _cmUiDestroyPanel(p,ctl->usrId);
    else
      rc = _cmUiDestroyCtl(p,ctl);
  }
  return rc;
}

bool cmUiCtlExists(     cmUiH_t uiH, unsigned panelId, unsigned id )
{
  cmUi_t*  p  = _cmUiHandleToPtr(uiH);

  if( panelId == id )
  {
    cmUiPanel_t* pp=NULL;
    return _cmUiFindPanel(p, panelId, &pp, false ) == kOkUiRC;
  }

  cmUiCtl_t* ctl=NULL;
  return _cmUiFindCtl(p,id,&ctl,false) == kOkUiRC;
}

cmUiRC_t cmUiClearPanel( cmUiH_t uiH, unsigned panelId )
{
  cmUiRC_t     rc = kOkUiRC;
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp = NULL;

  // get the panel recd ptr
  if((rc = _cmUiFindPanel(p,panelId,&pp,true)) != kOkUiRC )
    return rc;

  cmUiApp_t* ap  = pp->appPtr;
  cmUiCtl_t* ctl = NULL;
  unsigned   i   = 0;
  unsigned   n   = cmArrayCount(ap->ctlArrH);
  cmUiRC_t   rc0;

  // Destroy all controls that belong to this panel.
  for(i=0; i<n; ++i)
    if( _cmUiFindCtl(p,i,&ctl,false) == kOkUiRC && ctl != NULL && ctl->panelId == panelId && ctl->usrId != panelId)
      if((rc0 = _cmUiDestroyCtl(p,ctl)) != kOkUiRC )
        rc = rc0;

  return rc;
}

cmUiRC_t cmUiNextRect(    cmUiH_t uiH, unsigned panelId, int x, int y, int w, int h )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  cmUiRC_t     rc;
  
  if((rc = _cmUiFindPanel(p, panelId, &pp, true)) != kOkUiRC )
    return rc;
 
  pp->rect.x = x;
  pp->rect.y = y;
  pp->rect.w = w;
  pp->rect.h = h;
  pp->flags = cmSetFlag(pp->flags,kUseRectUiFl);

  return rc;
}

bool    cmUiFillRows( cmUiH_t uiH, unsigned panelId )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if( _cmUiFindPanel(p, panelId, &pp, true) != kOkUiRC )
    return false;

  return cmIsFlag(pp->flags,kFillRowsUiFl);
}

bool    cmUiSetFillRows( cmUiH_t uiH, unsigned panelId, bool enableFl )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if( _cmUiFindPanel(p, panelId, &pp, true) != kOkUiRC )
    return false;

  bool retFl = cmIsFlag(pp->flags,kFillRowsUiFl);

  pp->flags = cmEnaFlag(pp->flags,kFillRowsUiFl,enableFl);

  return retFl;
}

int      cmUiBaseCol(    cmUiH_t uiH, unsigned panelId, int x )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if( _cmUiFindPanel(p, panelId, &pp, true) != kOkUiRC )
    return -1;

  int bc = pp->baseCol;
  pp->baseCol  = x;
  pp->flags    = cmSetFlag(pp->flags,kPlaceBaseRowUiFl);
  return bc;
}

void     cmUiPlaceRight( cmUiH_t uiH, unsigned panelId )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if(_cmUiFindPanel(p, panelId, &pp, true) != kOkUiRC )
    return;

  pp->flags = cmClrFlag(pp->flags,kPlaceBelowUiFl);
  pp->flags = cmSetFlag(pp->flags,kPlaceRightUiFl);
}

void     cmUiPlaceBelow( cmUiH_t uiH, unsigned panelId )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if(_cmUiFindPanel(p, panelId, &pp, true) != kOkUiRC )
    return;

  pp->flags = cmClrFlag(pp->flags,kPlaceRightUiFl);
  pp->flags = cmSetFlag(pp->flags,kPlaceBelowUiFl);
}

int      cmUiBaseRow(    cmUiH_t uiH, unsigned panelId, int y )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  cmUiRC_t     rc;
  
  if((rc = _cmUiFindPanel(p, panelId, &pp, true)) != kOkUiRC )
    return -1;

  int br = pp->baseRow;
  pp->baseRow  = y;
  return br;
}

int      cmUiW( cmUiH_t uiH, unsigned panelId )         
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if(_cmUiFindPanel(p, panelId, &pp, true) != kOkUiRC )
    return -1;

  return pp->dfltW;
}

int      cmUiH( cmUiH_t uiH, unsigned panelId )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if(_cmUiFindPanel(p, panelId, &pp, true) != kOkUiRC )
    return -1;

  return pp->dfltH;
}

void     cmUiSetWH(  cmUiH_t uiH, unsigned panelId, int w, int h )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if(_cmUiFindPanel(p, panelId, &pp, true) != kOkUiRC )
    return;

  pp->dfltW = w;
  pp->dfltH = h;
}

int     cmUiNextW(  cmUiH_t uiH, unsigned panelId )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if(_cmUiFindPanel(p, panelId, &pp, true) != kOkUiRC )
    return -1;

  return pp->nextW;
}

int     cmUiNextH(  cmUiH_t uiH, unsigned panelId )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if(_cmUiFindPanel(p, panelId, &pp, true) != kOkUiRC )
    return -1;

  return pp->nextH;
}

void     cmUiSetNextWH( cmUiH_t uiH, unsigned panelId, int w, int h )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if(_cmUiFindPanel(p, panelId, &pp, true) != kOkUiRC )
    return;

  pp->nextW = w;
  pp->nextH = h;
  pp->flags = cmSetFlag(pp->flags,kNextWHUiFl);
}

int      cmUiHBorder( cmUiH_t uiH, unsigned panelId )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if(_cmUiFindPanel(p, panelId, &pp, true) != kOkUiRC )
    return -1;

  return pp->dfltHBorder;
}

int      cmUiVBorder( cmUiH_t uiH, unsigned panelId )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if(_cmUiFindPanel(p, panelId, &pp, true) != kOkUiRC )
    return -1;

  return pp->dfltVBorder;
}

int      cmUiSetHBorder( cmUiH_t uiH, unsigned panelId, int w )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if(_cmUiFindPanel(p, panelId, &pp, true) != kOkUiRC )
    return -1;
  
  int rv = pp->dfltHBorder;
  pp->dfltHBorder = w;
  return rv;
}

int      cmUiSetVBorder( cmUiH_t uiH, unsigned panelId, int h )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if(_cmUiFindPanel(p, panelId, &pp, true) != kOkUiRC )
    return -1;

  int rv = pp->dfltVBorder;
  pp->dfltVBorder = h;
  return rv;
}

int      cmUiNextHBorder( cmUiH_t uiH, unsigned panelId )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if(_cmUiFindPanel(p, panelId, &pp, true) != kOkUiRC )
    return -1;

  return pp->nextHBorder;
}

int      cmUiNextVBorder( cmUiH_t uiH, unsigned panelId )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if(_cmUiFindPanel(p, panelId, &pp, true) != kOkUiRC )
    return -1;

  return pp->nextVBorder;
}

int      cmUiSetNextHBorder( cmUiH_t uiH, unsigned panelId, int w )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if(_cmUiFindPanel(p, panelId, &pp, true) != kOkUiRC )
    return -1;

  int rv = pp->nextHBorder;
  pp->nextHBorder = w;
  return rv;
}

int      cmUiSetNextVBorder( cmUiH_t uiH, unsigned panelId, int h )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiPanel_t* pp;
  
  if(_cmUiFindPanel(p, panelId, &pp, true) != kOkUiRC )
    return -1;

  int rv = pp->nextVBorder;
  pp->nextVBorder = h;
  return rv;
}


cmUiRC_t  cmUiSetInt(    cmUiH_t uiH,  unsigned id, int v )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*   ctl;
  cmUiRC_t     rc;
  cmUiDriverArg_t a;

  if((rc = _cmUiFastFindCtl(p,id,&ctl,true)) != kOkUiRC )
    return rc;

  // TODO: cache the cmUiDriverArg_t for this control in the ctl_t
  // object to avoid having to recreate the arg. recd on every call.
  _cmUiDriverArgInit(&a, p, kSetValDId, ctl->cId, ctl->panelId, id );

  a.ival  = v;
  a.flags |= kValUiFl;

  return _cmUiCallDriver(p,&a);

}

cmUiRC_t  cmUiSetUInt(   cmUiH_t uiH,  unsigned id, unsigned v )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*   ctl;
  cmUiRC_t     rc;
  cmUiDriverArg_t a;

  if((rc = _cmUiFastFindCtl(p,id,&ctl,true)) != kOkUiRC )
    return rc;

  _cmUiDriverArgInit(&a, p, kSetValDId, ctl->cId, ctl->panelId, id );

  a.ival  = (int)v;
  a.flags |= kValUiFl;

  return _cmUiCallDriver(p,&a);
}

cmUiRC_t cmUiSetDouble( cmUiH_t uiH,  unsigned id, double v )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*   ctl;
  cmUiRC_t     rc;
  cmUiDriverArg_t a;

  if((rc = _cmUiFastFindCtl(p,id,&ctl,true)) != kOkUiRC )
    return rc;

  _cmUiDriverArgInit(&a, p, kSetValDId, ctl->cId, ctl->panelId, id );

  a.fval  = v;
  a.flags |= kValUiFl;

  return _cmUiCallDriver(p,&a);
}

cmUiRC_t  cmUiSetString( cmUiH_t uiH,  unsigned id, const cmChar_t* v )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*   ctl;
  cmUiRC_t     rc;
  cmUiDriverArg_t a;

  if((rc = _cmUiFastFindCtl(p,id,&ctl,true)) != kOkUiRC )
    return rc;

  _cmUiDriverArgInit(&a, p, kSetValDId, ctl->cId, ctl->panelId, id );

  a.sval  = v;
  a.flags |= kValUiFl;

  return _cmUiCallDriver(p,&a);
}

int             cmUiInt(    cmUiH_t uiH,  unsigned id )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*   ctl;
  cmUiRC_t     rc;
  
  if((rc = _cmUiFastFindCtl(p,id,&ctl,true)) != kOkUiRC )
    return 0;

  return ctl->getInt(ctl);
}

unsigned        cmUiUInt(   cmUiH_t uiH,  unsigned id )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*   ctl;
  cmUiRC_t     rc;
  
  if((rc = _cmUiFastFindCtl(p,id,&ctl,true)) != kOkUiRC )
    return 0;

  return ctl->getInt(ctl);
}

double          cmUiDouble( cmUiH_t uiH,  unsigned id )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*   ctl;
  cmUiRC_t     rc;
  
  if((rc = _cmUiFastFindCtl(p,id,&ctl,true)) != kOkUiRC )
    return 0;

  return ctl->getDbl(ctl);
}

const cmChar_t* cmUiString( cmUiH_t uiH,  unsigned id )
{
  cmUi_t*      p  = _cmUiHandleToPtr(uiH);
  cmUiCtl_t*   ctl;
  cmUiRC_t     rc;
  
  if((rc = _cmUiFastFindCtl(p,id,&ctl,true)) != kOkUiRC )
    return 0;

  return ctl->getStr(ctl);
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

