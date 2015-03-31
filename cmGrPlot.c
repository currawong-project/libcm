#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmGr.h"
#include "cmGrDevCtx.h"

#include "cmGrPlot.h"
#include "cmVectOpsTemplateMain.h"


//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------



struct cmGrPl_str;


typedef struct cmGrPlotObj_str
{
  cmGrH_t           grH;        // the canvas this object is drawn on
  cmGrObjH_t        grObjH;     // the grObj this object is based on
  cmGrPlObjTypeId_t typeId;
  unsigned          cfgFlags;
  unsigned          stateFlags;
  cmGrVExt_t        vext;

  cmChar_t*         label;
  unsigned          labelFlags;
  int               labelAngle;
  cmGrColor_t       labelColor;

  int               loffs;
  int               toffs;
  int               roffs;
  int               boffs;

  cmGrColor_t       drawColors[ kMaxPlGrId ];
  cmGrColor_t       fillColors[ kMaxPlGrId ];
  unsigned          fontId;
  unsigned          fontSize;
  unsigned          fontStyle;
  void*             userPtr;
  unsigned          userByteCnt; // 0 if userPtr does not need to be realease on object destruction
  cmGrPlotCbFunc_t  cbFunc;
  void*             cbArg;

  struct cmGrPl_str*      p;       // owning plot object manager
  struct cmGrPlotObj_str* parent;  // containing object   
  struct cmGrPlotObj_str* xAnchor; // x-location reference object
  struct cmGrPlotObj_str* yAnchor; // y-location reference object 
  struct cmGrPlotObj_str* next;
  struct cmGrPlotObj_str* prev;

} cmGrPlotObj_t;


typedef struct cmGrPl_str
{
  cmCtx_t*         ctx;         // 
  cmErr_t          err;         //
  cmGrPlotObj_t*   list;        // plot object linked list
  cmGrPlotObj_t*   fop;         // focused object ptr
  cmGrPlotCbFunc_t cbFunc;      // dflt callback function
  void*            cbArg;       // dflt callback function arg.
} cmGrPl_t;

cmGrPlH_t    cmGrPlNullHandle    = cmSTATIC_NULL_HANDLE;
cmGrPlObjH_t cmGrPlObjNullHandle = cmSTATIC_NULL_HANDLE;

//------------------------------------------------------------------------------------------------------------------
// Plot Private Functions
//------------------------------------------------------------------------------------------------------------------

cmGrPl_t* _cmGrPlHandleToPtr( cmGrPlH_t h )
{
  cmGrPl_t* p = (cmGrPl_t*)h.h;
  assert(p!=NULL);
  return p;
}

cmGrPlotObj_t* _cmGrPlObjHandleToPtr( cmGrPlObjH_t oh )
{
  cmGrPlotObj_t* op = (cmGrPlotObj_t*)oh.h;
  assert( op!=NULL);
  return op;
}

// Destroy the embedded cmGrH_t object
cmGrPlRC_t _cmGrPlotObjDelete( cmGrPlotObj_t* op )
{
  if( op==NULL || cmGrObjIsValid( op->grH, op->grObjH)==false )
    return kOkGrPlRC;

  cmGrPl_t* p = op->p;

  // destroy the cmGrObj - which will call _cmGrPlotObjDestroy()
  if( cmGrObjDestroy( op->grH, &op->grObjH ) != kOkGrRC )
    return cmErrMsg( &p->err, kGrFailGrPlRC, "Delete failed on the object label='%s' id=%i\n",cmStringNullGuard( op->label ), cmGrObjId(op->grObjH) );
 
  return kOkGrPlRC;
}

void _cmGrPlotObjUnlink( cmGrPlotObj_t* op )
{
  cmGrPl_t* p = op->p;
  
  if( op->next != NULL )
    op->next->prev = op->prev;

  if( op->prev != NULL )
    op->prev->next = op->next;

  if( p->list == op )
    p->list = op->next;
}

void _cmGrPlotObjLink( cmGrPl_t* p, cmGrPlotObj_t* op )
{
  if( p->list != NULL )
    p->list->prev = op;

  op->next = p->list;
  op->prev = NULL;
  p->list  = op;
}


// Destroy all objects
cmGrPlRC_t _cmGrPlotClear( cmGrPl_t* p )
{
  cmGrPlRC_t     rc = kOkGrPlRC;
  cmGrPlotObj_t* op = p->list;

  while( op!=NULL )
  {
    cmGrPlotObj_t* t = op->next;

    if((rc = _cmGrPlotObjDelete(op)) != kOkGrPlRC )
      break;

    op = t;
  }

  return rc;
}

// Destroy the plot mgr
cmGrPlRC_t _cmGrPlotDestroy( cmGrPl_t* p )
{
  cmGrPlRC_t rc;

  if((rc = _cmGrPlotClear(p)) != kOkGrPlRC )
    return rc;

  cmMemFree(p);

  return kOkGrPlRC;
}

bool _cmGrPlotObjIsVisible( cmGrPlotObj_t* op )
{ return cmIsNotFlag(op->cfgFlags,kNoDrawGrPlFl); }

bool _cmGrPlotObjIsEnabled( cmGrPlotObj_t* op )
{
  // invisible objects are never enabled
  if( _cmGrPlotObjIsVisible(op) == false )
    return false;

  return cmIsFlag(op->stateFlags,kEnabledGrPlFl);
}

bool _cmGrPlotObjIsFocused(cmGrPlotObj_t* op)
{ return _cmGrPlotObjIsEnabled(op) && op->p->fop==op; }

bool _cmGrPlotObjIsSelected(cmGrPlotObj_t* op)
{ return /*_cmGrPlotObjIsFocused(op) ||*/ cmIsFlag(op->stateFlags,kSelectGrPlFl); }

void _cmGrPlotObjSetupCbArg( cmGrPlotCbArg_t* a, cmGrPlotObj_t* op, cmGrPlCbSelId_t selId )
{
  cmGrPlObjH_t    oH;
  oH.h = op;

  memset(a,0,sizeof(*a));
  a->ctx   = op->p->ctx;
  a->cbArg = op->cbArg;
  a->selId = selId;
  a->objH  = oH;
}

bool _cmGrPlotObjCb( cmGrPlotObj_t* op, cmGrPlCbSelId_t selId, unsigned deltaFlags )
{
  if( op->cbFunc != NULL )
  {
    cmGrPlotCbArg_t a;

    _cmGrPlotObjSetupCbArg(&a,op,selId);
    a.deltaFlags = deltaFlags; 
    return op->cbFunc(&a);
  }

  return true;
}

void _cmGrPlotObjSetFocus( cmGrPlotObj_t* op )
{
  // if 'op' is not enabled then it cannot receive the focus
  if( _cmGrPlotObjIsEnabled(op) == false )
    return;

  // if the focus cannot be set on 'op' - then try op->parent
  for(; op!=NULL; op=op->parent)
    if( cmIsNotFlag(op->cfgFlags,kNoFocusGrPlFl) && cmIsNotFlag(op->cfgFlags,kNoDrawGrPlFl) )
      break;

  // if the focus is changing to a new object
  if( op != NULL  && op->p->fop != op )
  {  
    if( op->p->fop != NULL )
    {
      // if the application callback returns false then do no release focus from the current object
      if(_cmGrPlotObjCb(op->p->fop, kPreEventCbSelGrPlId, kFocusGrPlFl ) == false )
        return;

      cmGrPlotObj_t* fop = op->p->fop;

      op->p->fop = NULL;

      // notify focus loser
      _cmGrPlotObjCb(fop, kStateChangeGrPlId, kFocusGrPlFl );
    }

    // if the application callback returns false then do not give focus to the selected object
    if(_cmGrPlotObjCb(op, kPreEventCbSelGrPlId, kFocusGrPlFl ) == false )
      return;

    op->p->fop = op;

    // notify focus winner
    _cmGrPlotObjCb(op, kStateChangeGrPlId, kFocusGrPlFl );
        
  }

}

// Set the clear flag to clear all other selected objects if this object is 
// selected.
void _cmGrPlotObjSetSelect( cmGrPlotObj_t* op, bool clearFl )
{
  // if the object is disabled or not selectable
  if( _cmGrPlotObjIsEnabled(op)==false || cmIsFlag(op->cfgFlags,kNoSelectGrPlFl | kNoDrawGrPlFl) )
    return;

  unsigned stateFlags = op->stateFlags;

  // if the application callback returns false then do not change the select state of the object
  if(_cmGrPlotObjCb(op, kPreEventCbSelGrPlId, kSelectGrPlFl ) == false )
    return;

  if( clearFl )
  {
    cmGrObjH_t     parentObjH = cmGrObjParent(op->grObjH);
    cmGrPlotObj_t* cop        = op->p->list;

    // clear the select flag on all objects that share op->parent
    for(; cop!=NULL; cop=cop->next)
      if( cop!=op && cmHandlesAreEqual(cmGrObjParent(cop->grObjH),parentObjH) )
      {
        if( cmIsFlag(cop->stateFlags,kSelectGrPlFl) )
        {  
          cop->stateFlags = cmClrFlag(cop->stateFlags,kSelectGrPlFl);
          
          _cmGrPlotObjCb(cop, kStateChangeGrPlId, kSelectGrPlFl );
        }
      }
  }
         
  op->stateFlags = cmTogFlag(stateFlags,kSelectGrPlFl);

  // notify state change
  _cmGrPlotObjCb(op, kStateChangeGrPlId, kSelectGrPlFl );
}


cmGrColor_t _cmGrPlotColor( cmGrPlotObj_t* op, cmGrColor_t* array )
{
  if( _cmGrPlotObjIsFocused(op) )
    return array[kFocusPlGrId];
    
  if( _cmGrPlotObjIsSelected(op) )
    return array[kSelectPlGrId];

  if( _cmGrPlotObjIsEnabled(op) )
    return array[kEnablePlGrId];

  return array[kDisablePlGrId];  
}

unsigned _cmGrPlotObjTriShapeToFlags( unsigned typeId)
{
  switch(typeId)
  {
    case kUTriGrPlId: return kTopGrFl; 
    case kDTriGrPlId: return kBottomGrFl;
    case kLTriGrPlId: return kLeftGrFl;
    case kRTriGrPlId: return kRightGrFl;
    default:
      { assert(0); }
  }
  return 0;
}

//------------------------------------------------------------------------------------------------------------------
// Plot Object Callback Functions
//------------------------------------------------------------------------------------------------------------------

cmGrRC_t _cmGrPlotObjCreate(   cmGrObjFuncArgs_t* args )
{
  cmGrPlotObj_t* op = args->cbArg;
  _cmGrPlotObjCb(op,kCreatedCbSelGrPlId,0);

   // return kOkGrRC to indicate that the create was successful
  return kOkGrRC; 
}

void _cmGrPlotObjDestroy(  cmGrObjFuncArgs_t* args )
{
  cmGrPlotObj_t* op = args->cbArg;
  
  // if the focused op is being destroyed
  if( op->p->fop == op )
  {
    _cmGrPlotObjCb(op->p->fop, kStateChangeGrPlId, kFocusGrPlFl );
    op->p->fop = NULL;
  }

  // TODO: is it possible to prevent destruction by returning
  // 'false' from the used defined callback.  This feature is
  // slightly complicated by the fact
  // that in some circumstances the destroy request is not 
  // optional - for example when the program is closing.
  _cmGrPlotObjCb(op,kDestroyedCbSelGrPlId,0);

  _cmGrPlotObjUnlink( op );
  
  if( op->userByteCnt != 0 )
  {
    cmMemFree(op->userPtr);
    op->userByteCnt = 0;
  }

  cmMemFree(op->label);
  cmMemFree(op);
}

void _cmGrPlotObjGetVExt( cmGrPlotObj_t* op, cmGrVExt_t* vext )
{
  switch( op->typeId )
  {
    case kStarGrPlId:
    case kCrossGrPlId:
    case kPlusGrPlId:
    case kDiamondGrPlId:
    case kUTriGrPlId:
    case kDTriGrPlId:
    case kLTriGrPlId:
    case kRTriGrPlId:
    case kRectGrPlId:
    case kLineGrPlId:
    case kEllipseGrPlId:
      {
        *vext = op->vext;        
      }
      break;

    case kHLineGrPlId:
    case kVLineGrPlId:
      {
        cmGrVExt_t wext;
        cmGrObjH_t oh = cmGrObjParent(op->grObjH);
        cmGrObjWorldExt(oh,&wext);

        // TODO: Put a check somewhere which can report an error
        // message when the parents world extent is not yet set.
        // Horz and Vert lines depend on the their parent's
        // world extents being set first.  There is no automatic
        // way to set the parent world extents because we don't
        // know the range of values which the data set will cover.
        // Any number picked could result in a range much to large
        // thereby leaving the data invisible.  It therefore must
        // be up to the application to set a good range.
        assert( cmGrVExtIsNotNullOrEmpty(&wext) );
        
        vext->loc.x = op->typeId==kHLineGrPlId ? wext.loc.x : op->vext.loc.x;
        vext->loc.y = op->typeId==kVLineGrPlId ? wext.loc.y : op->vext.loc.y;
        vext->sz.w  = op->typeId==kHLineGrPlId ? wext.sz.w  : op->vext.sz.w;
        vext->sz.h  = op->typeId==kVLineGrPlId ? wext.sz.h  : op->vext.sz.h;
      }
      break;

    default:
      { assert(0); }
  }

  // add up the anchor offsets until the first object in the container
  cmGrPlotObj_t* ap = op->xAnchor;
  for(; ap!=NULL; ap=ap->xAnchor)
  {
    vext->loc.x += ap->vext.loc.x;
    if( ap->xAnchor==ap->parent)
      break;
  }
  ap = op->yAnchor;
  for(; ap!=NULL; ap=ap->yAnchor)
  {
    vext->loc.y += ap->vext.loc.y;
    if( ap->yAnchor==ap->parent)
      break;
  }
}

void _cmGrPlotObjVExt( cmGrObjFuncArgs_t* args, cmGrVExt_t* vext )
{
  cmGrPlotObj_t* op = args->cbArg;
  _cmGrPlotObjGetVExt(op, vext);
}

bool _cmGrPlotObjRender(   cmGrObjFuncArgs_t* args, cmGrDcH_t dcH )
{
  cmGrPlotObj_t* op = args->cbArg;
  cmGrPExt_t pext;
  cmGrVExt_t vext;

  if( !_cmGrPlotObjIsVisible(op) )
    return false;

  // get the virtual extents of this object
  _cmGrPlotObjVExt( args, &vext );

  // convert the virtual ext's to phys ext's
  cmGrVExt_VtoP( op->grH, op->grObjH, &vext, &pext);

  // expand the ext's according to the physical offsets
  cmGrPExtExpand(&pext,op->loffs,op->toffs,op->roffs,op->boffs);


  switch( op->typeId )
  {
    case kLineGrPlId:
      //cmGrDcSetColor(    dcH, _cmGrPlotColor(op,op->drawColors) );
      //cmGrDcDrawLine(    dcH, cmGrPExtL(&pext), cmGrPExtT(&pext), cmGrPExtR(&pext), cmGrPExtB(&pext) );
      //break;

    case kStarGrPlId:
    case kCrossGrPlId:
    case kPlusGrPlId:
    case kEllipseGrPlId:
    case kDiamondGrPlId:
    case kUTriGrPlId:
    case kDTriGrPlId:
    case kLTriGrPlId:
    case kRTriGrPlId:
    case kRectGrPlId:
    case kHLineGrPlId:
    case kVLineGrPlId:
      {
        if( cmIsNotFlag(op->cfgFlags,kNoFillGrPlFl) )
        {
          // set the fill color
          cmGrDcSetColor( dcH, _cmGrPlotColor(op,op->fillColors) );

          // draw the fill
          switch(op->typeId)
          {
            case kEllipseGrPlId:
              cmGrDcFillEllipse( dcH, pext.loc.x, pext.loc.y, pext.sz.w, pext.sz.h);
              break;

            case kDiamondGrPlId:
              cmGrDcFillDiamond( dcH, pext.loc.x, pext.loc.y, pext.sz.w, pext.sz.h);
              break;

            case kUTriGrPlId:
            case kDTriGrPlId:
            case kLTriGrPlId:
            case kRTriGrPlId:
              cmGrDcFillTriangle( dcH, pext.loc.x, pext.loc.y, pext.sz.w, pext.sz.h, _cmGrPlotObjTriShapeToFlags(op->typeId));
              break;

            case kStarGrPlId:
            case kCrossGrPlId:
            case kPlusGrPlId:
            case kRectGrPlId:
            case kHLineGrPlId:
            case kVLineGrPlId:
              cmGrDcFillRect( dcH, pext.loc.x, pext.loc.y, pext.sz.w, pext.sz.h);
              break;

            case kLineGrPlId:
              break;

            default:
              { assert(0); }
          }
        }

        if( cmIsNotFlag(op->cfgFlags,kNoBorderGrPlFl) )
        {
          // set the border color
          cmGrDcSetColor( dcH, _cmGrPlotColor(op,op->drawColors) );


          // draw the border
          switch(op->typeId)
          {
            case kEllipseGrPlId:
              cmGrDcDrawEllipse( dcH, pext.loc.x, pext.loc.y, pext.sz.w, pext.sz.h);
              break;

            case kDiamondGrPlId:
              cmGrDcDrawDiamond( dcH, pext.loc.x, pext.loc.y, pext.sz.w, pext.sz.h);
              break;

            case kUTriGrPlId:
            case kDTriGrPlId:
            case kLTriGrPlId:
            case kRTriGrPlId:
              cmGrDcDrawTriangle( dcH, pext.loc.x, pext.loc.y, pext.sz.w, pext.sz.h, _cmGrPlotObjTriShapeToFlags(op->typeId));
              break;

            case kStarGrPlId:
              cmGrDcDrawLine( dcH, cmGrPExtL(&pext), cmGrPExtT(&pext), cmGrPExtR(&pext), cmGrPExtB(&pext));
              cmGrDcDrawLine( dcH, cmGrPExtL(&pext), cmGrPExtB(&pext), cmGrPExtR(&pext), cmGrPExtT(&pext));
              cmGrDcDrawLine( dcH, cmGrPExtL(&pext)  + cmGrPExtW(&pext)/2, cmGrPExtT(&pext),                      cmGrPExtL(&pext)  + cmGrPExtW(&pext)/2, cmGrPExtB(&pext));
              cmGrDcDrawLine( dcH, cmGrPExtL(&pext),                       cmGrPExtT(&pext) + cmGrPExtH(&pext)/2, cmGrPExtR(&pext),                       cmGrPExtT(&pext) + cmGrPExtH(&pext)/2);
              break;

            case kCrossGrPlId:
              cmGrDcDrawLine( dcH, cmGrPExtL(&pext), cmGrPExtT(&pext), cmGrPExtR(&pext), cmGrPExtB(&pext));
              cmGrDcDrawLine( dcH, cmGrPExtR(&pext), cmGrPExtT(&pext), cmGrPExtL(&pext), cmGrPExtB(&pext));
              break;

            case kPlusGrPlId:
              cmGrDcDrawLine( dcH, cmGrPExtL(&pext)  + cmGrPExtW(&pext)/2, cmGrPExtT(&pext),                      cmGrPExtL(&pext)  + cmGrPExtW(&pext)/2, cmGrPExtB(&pext));
              cmGrDcDrawLine( dcH, cmGrPExtL(&pext),                       cmGrPExtT(&pext) + cmGrPExtH(&pext)/2, cmGrPExtR(&pext),                       cmGrPExtT(&pext) + cmGrPExtH(&pext)/2);
              break;

            case kLineGrPlId:
              cmGrDcDrawLine(    dcH, cmGrPExtL(&pext), cmGrPExtT(&pext), cmGrPExtR(&pext), cmGrPExtB(&pext) );
              break;

            case kRectGrPlId:
            case kHLineGrPlId:
            case kVLineGrPlId:
              cmGrDcDrawRect( dcH, pext.loc.x, pext.loc.y, pext.sz.w, pext.sz.h);
              break;

            default:
              { assert(0); }

          }

        }

        if( (op->label != NULL) && cmIsNotFlag(op->cfgFlags, kNoLabelGrPlFl) )
        {
          unsigned cc = cmGrDcColor(dcH);
          cmGrDcSetColor(dcH,op->labelColor);
          cmGrDcDrawTextJustify( dcH, op->fontId, op->fontSize, op->fontStyle, op->label, &pext, op->labelFlags );
          cmGrDcSetColor(dcH,cc);
          
          

          /*
          cmGrPSz_t sz;
          cmGrPPt_t pt;
          cmGrDcFontSetAndMeasure( dcH, op->fontId, op->fontSize, op->fontStyle, op->label, &sz );
          cmGrPExtCtr( &pext, &pt );
          cmGrDcDrawText( dcH, op->label, pt.x - sz.w/2, pt.y + sz.h/2 );
          */
        }

      }
      break;


    default:
      { assert(0); }
  }

  return true;
}

int  _cmGrPlotObjDistance( cmGrObjFuncArgs_t* args, int x, int y )
{
  return 0;
}

bool _cmGrPlotObjEvent( cmGrObjFuncArgs_t* args, unsigned flags, unsigned key, int px, int py )
{
  cmGrPlotObj_t*  op = args->cbArg;
  bool            fl = false;
  cmGrPlotCbArg_t a;

  if( op->cbFunc != NULL )
  {
    cmGrPlotObj_t* cb_op = op;

    // if this is a key up/dn event and 'op' is not the 'focused op' then 
    // callback on the 'focused op' instead of this 'op'.
    if( (cmIsFlag(flags,kKeyDnGrFl) || cmIsFlag(flags,kKeyUpGrFl)) && op->p->fop != op )
      cb_op = op->p->fop;
      
    _cmGrPlotObjSetupCbArg(&a,cb_op,kPreEventCbSelGrPlId);
    a.eventFlags = flags;
    a.eventKey   = key;
    a.eventX     = px;
    a.eventY     = py;
    if( op->cbFunc(&a) == false )
      return true;      
  }

  switch( flags & kEvtMask )
  {
    case kMsDownGrFl:
      break;

    case kMsUpGrFl:
      _cmGrPlotObjSetFocus( op );
      fl = true;
      break;

    case kMsClickGrFl:
      _cmGrPlotObjSetSelect(op, cmIsNotFlag(flags,kCtlKeyGrFl) );
      fl = true;
      break;

    case kMsDragGrFl: 
      {
        cmGrVExt_t vext;
        cmGrVExt_t wext;

        if( cmIsFlag(op->cfgFlags,kNoDragGrPlFl | kNoDrawGrPlFl) )
          return false;

        // get the parent world extents
        cmGrObjWorldExt( cmGrObjParent( args->objH ), &wext );

        // calc the new position of the obj
        cmGrV_t x = args->msVPt.x - args->msDnVOffs.w;
        cmGrV_t y = args->msVPt.y - args->msDnVOffs.h;

        cmGrVExtSet(&vext,x,y,op->vext.sz.w,op->vext.sz.h);

        // the obj must be remain inside the parent wext
        cmGrVExtContain(&wext,&vext);

        // calculate the obj's location as an offset from it's anchors
        cmGrPlotObj_t* ap = op->xAnchor;
        for(; ap!=NULL; ap=ap->xAnchor)
          vext.loc.x -= ap->vext.loc.x;

        ap = op->yAnchor;
        for(; ap!=NULL; ap=ap->yAnchor)
          vext.loc.y -= ap->vext.loc.y;
        

        if( !cmGrVExtIsEqual(&op->vext,&vext) )
        {
          // move the object
          op->vext.loc.x = vext.loc.x;
          op->vext.loc.y = vext.loc.y;
          fl = true;
        }        
      }
      break;

    case kKeyDnGrFl:
    case kKeyUpGrFl:
      break;
  }

  // notify the app of the event
  if( op->cbFunc != NULL )
  {
    a.selId = kEventCbSelGrPlId;
    op->cbFunc(&a);
  }

  return fl;
}

bool _cmGrPlotObjIsBorderClick( const cmGrPExt_t* pext, int px, int py )
{
  int dist = 3;

  if( cmVOR_PtToLineDistance(cmGrPExtL(pext),cmGrPExtT(pext),cmGrPExtL(pext),cmGrPExtB(pext),px,py) < dist )
    return true;

  if(cmVOR_PtToLineDistance(cmGrPExtR(pext),cmGrPExtT(pext),cmGrPExtR(pext),cmGrPExtB(pext),px,py) < dist )
    return true;

  if( cmVOR_PtToLineDistance(cmGrPExtL(pext),cmGrPExtT(pext),cmGrPExtR(pext),cmGrPExtT(pext),px,py) < dist )
    return true;

  if(cmVOR_PtToLineDistance(cmGrPExtL(pext),cmGrPExtB(pext),cmGrPExtR(pext),cmGrPExtB(pext),px,py) < dist )
    return true;
  
  return false;
}

bool _cmGrPlotObjIsInside( cmGrObjFuncArgs_t* args, unsigned evtFlags, int px, int py, cmGrV_t vx, cmGrV_t vy )
{
  cmGrVExt_t vext;
  cmGrPExt_t pext;
  cmGrPlotObj_t* op = args->cbArg;

  // no matter the type of event the object must be visible and enabled to respond to it 
  if(!_cmGrPlotObjIsVisible(op)|| !_cmGrPlotObjIsEnabled(op) )
    return false;

  // objects that are not selectable are also not clickable
  if( cmIsFlag(evtFlags,kMsClickGrFl) && cmIsFlag(op->cfgFlags,kNoSelectGrPlFl) )
    return false;

  // non-draggable objects can't be dragged.
  if( cmIsFlag(evtFlags,kMsDragGrFl) && cmIsFlag(op->cfgFlags,kNoDragGrPlFl) )
    return false;

  // get the virtual extents of this object
  _cmGrPlotObjVExt( args, &vext );

  // convert the virtual ext's to phys ext's
  cmGrVExt_VtoP(args->grH,args->objH,&vext,&pext);

  // expand the ext's according to the off's
  cmGrPExtExpand(&pext,op->loffs,op->toffs,op->roffs,op->boffs);

  if( op->typeId == kLineGrPlId )
    if( cmVOR_PtToLineDistance(cmGrPExtL(&pext),cmGrPExtT(&pext),cmGrPExtR(&pext),cmGrPExtB(&pext),px,py) < 3 )
      return true;

  // if this is a click event and this is a border selectable object
  if( cmIsFlag(evtFlags,kMsClickGrFl) && cmIsFlag(op->cfgFlags,kBorderSelGrPlFl) )
    return _cmGrPlotObjIsBorderClick(&pext,px,py);

  // check if the px,py is inside pext
  return cmGrPExtIsXyInside(&pext,px,py);
}

void _cmGrPlotFuncObjSetupDefault(cmGrObjFunc_t *f, void* arg )
{
  f->createCbFunc   = _cmGrPlotObjCreate;
  f->createCbArg    = arg;
  f->destroyCbFunc  = _cmGrPlotObjDestroy;
  f->destroyCbArg   = arg;
  f->renderCbFunc   = _cmGrPlotObjRender;
  f->renderCbArg    = arg;
  f->distanceCbFunc = _cmGrPlotObjDistance;
  f->distanceCbArg  = arg;
  f->eventCbFunc    = _cmGrPlotObjEvent;
  f->eventCbArg     = arg;
  f->vextCbFunc     = _cmGrPlotObjVExt;
  f->vextCbArg      = arg;
  f->isInsideCbFunc = _cmGrPlotObjIsInside;
  f->isInsideCbArg  = arg;
}

//------------------------------------------------------------------------------------------------------------------
// Plot Object Public Functions
//------------------------------------------------------------------------------------------------------------------

cmGrPlRC_t cmGrPlotObjCreate( 
  cmGrPlH_t         hh, 
  cmGrH_t           grH, 
  cmGrPlObjH_t*     hp, 
  unsigned          id, 
  cmGrPlObjH_t      parentPlObjH, 
  cmGrPlObjH_t      xAnchorPlObjH,
  cmGrPlObjH_t      yAnchorPlObjH,
  cmGrPlObjTypeId_t typeId, 
  unsigned          cfgFlags, 
  cmReal_t          x, 
  cmReal_t          y,  
  cmReal_t          w, 
  cmReal_t          h, 
  const cmChar_t*   label, 
  const cmGrVExt_t* wext )
{  
  cmGrPlRC_t    rc;
  cmGrObjFunc_t funcs;

  if((rc = cmGrPlotObjDestroy(hp)) != kOkGrPlRC )
    return rc;

  cmGrPl_t*      p  = _cmGrPlHandleToPtr(hh);
  cmGrPlotObj_t* op = cmMemAllocZ(cmGrPlotObj_t,1);

  _cmGrPlotObjLink(p,op);

  _cmGrPlotFuncObjSetupDefault(&funcs,op);

  // setup the object
  op->grH        = grH;
  op->typeId     = typeId;
  op->cfgFlags   = cfgFlags;
  op->stateFlags = kEnabledGrPlFl;
  op->label      = label==NULL ?NULL : cmMemAllocStr(label);
  op->labelFlags = kHorzCtrJsGrFl | kVertCtrJsGrFl;
  op->labelAngle = 0;
  op->labelColor = kBlackGrId;
  op->grObjH     = cmGrObjNullHandle;
  op->parent     = cmGrPlotObjIsValid(parentPlObjH)  ? _cmGrPlObjHandleToPtr(parentPlObjH)  : NULL;
  op->xAnchor    = cmGrPlotObjIsValid(xAnchorPlObjH) ? _cmGrPlObjHandleToPtr(xAnchorPlObjH) : NULL;
  op->yAnchor    = cmGrPlotObjIsValid(yAnchorPlObjH) ? _cmGrPlObjHandleToPtr(yAnchorPlObjH) : NULL;
  op->p          = p;
  op->fontId     = kHelveticaFfGrId;
  op->fontSize   = 12;
  op->fontStyle  = kNormalFsGrFl;
  op->cbFunc     = p->cbFunc;
  op->cbArg      = p->cbArg;

  if( cmIsFlag(op->cfgFlags,kSymbolGrPlFl) )
  {
    int ww = w==0 ? kDefaultSymW : w;
    int hh = h==0 ? kDefaultSymH : h;
    
    op->loffs = ww/2;
    op->roffs = ww/2;
    op->toffs = hh/2;
    op->boffs = hh/2;
    w         = 0;
    h         = 0;
  }

  cmGrVExtSet(&op->vext,x,y,w,h);


  // set the default colors
  op->drawColors[kSelectPlGrId] =   kPeruGrId;
  op->fillColors[kSelectPlGrId] =   kBurlyWoodGrId;

  op->drawColors[kFocusPlGrId] =  kDarkSlateBlueGrId;
  op->fillColors[kFocusPlGrId] =  0x8470ff; 

  op->drawColors[kEnablePlGrId] =  kBlackGrId;
  op->fillColors[kEnablePlGrId] =  0x009ff7; 

  op->drawColors[kDisablePlGrId] = 0xbebebe;
  op->fillColors[kDisablePlGrId] = kLightGrayGrId; 

  unsigned   grObjCfgFlags = 0;
  cmGrObjH_t parentGrH     = op->parent == NULL ? cmGrObjNullHandle : op->parent->grObjH;



  // create the graphics system object - during this call a
  // call is made to funcs.create().
  if( cmGrObjCreate(grH, &op->grObjH, parentGrH, &funcs, id, grObjCfgFlags, wext ) != kOkGrRC )
  {
    rc = cmErrMsg(&p->err,kGrFailGrPlRC,"Graphic system object create failed for object (id=%i).",id);
    goto errLabel;
  }

  if( hp != NULL )
    hp->h = op;

 errLabel:
  if( rc != kOkGrPlRC )
    _cmGrPlotObjDelete(op);

  return rc;
}

cmGrPlRC_t cmGrPlotObjDestroy( cmGrPlObjH_t* hp )
{
  cmGrPlRC_t rc = kOkGrPlRC;

  if( hp==NULL || cmGrPlotObjIsValid(*hp)==false )
    return kOkGrPlRC;

  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(*hp);
  
  if((rc = _cmGrPlotObjDelete(op)) != kOkGrPlRC )
    return rc;

  hp->h = NULL;

  return rc;
}

bool       cmGrPlotObjIsValid( cmGrPlObjH_t h )
{ return h.h != NULL; }


cmGrPlH_t  cmGrPlotObjMgrHandle( cmGrPlObjH_t oh )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);
  cmGrPlH_t grPlH;
  grPlH.h = op->p;
  return grPlH;
}

cmGrObjH_t     cmGrPlotObjHandle( cmGrPlObjH_t oh )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);
  return op->grObjH;
}

cmGrPlObjH_t     cmGrPlotObjParent( cmGrPlObjH_t oh )
{
  cmGrPlObjH_t p_oh;
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);
 
  p_oh.h = op->parent;
  return p_oh;
}

cmGrPlObjH_t     cmGrPlotObjXAnchor( cmGrPlObjH_t oh )
{
  cmGrPlObjH_t p_oh;
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);
 
  p_oh.h = op->xAnchor;
  return p_oh;
}

cmGrPlObjH_t     cmGrPlotObjYAnchor( cmGrPlObjH_t oh )
{
  cmGrPlObjH_t p_oh;
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);
 
  p_oh.h = op->yAnchor;
  return p_oh;
}


void            cmGrPlotObjSetId( cmGrPlObjH_t oh, unsigned id )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);
  cmGrObjSetId( op->grObjH, id );
}

void            cmGrPlotObjSetUserPtr(  cmGrPlObjH_t oh, void* userPtr )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);
  if( op->userByteCnt != 0 )
  {
    cmMemFree(op->userPtr);
    op->userByteCnt = 0;
  }

  op->userPtr = userPtr;
}

void            cmGrPlotObjAllocUser( cmGrPlObjH_t oh, const void* data, unsigned byteCnt )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);

  if( op->userByteCnt != byteCnt )
  {
    if( op->userByteCnt != 0  )
    {
      cmMemFree(op->userPtr);
      op->userByteCnt = 0;
    }

    op->userPtr     = cmMemAlloc(char,byteCnt);
    op->userByteCnt = byteCnt;
  }

  memcpy(op->userPtr,data,byteCnt);
  
}

void*           cmGrPlotObjUserPtr( cmGrPlObjH_t oh )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);
  return op->userPtr;
}


unsigned        cmGrPlotObjId( cmGrPlObjH_t oh )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);
  return cmGrObjId(op->grObjH);
}

void            cmGrPlotObjSetLabel(      cmGrPlObjH_t oh, const cmChar_t* label )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);
 
  if( label == op->label || (label != NULL && op->label!=NULL && strcmp(label,op->label)==0 ))
    return;

  cmMemPtrFree(&op->label);
  
  if( label != NULL )
  {
    assert( op->label == NULL );
    op->label = cmMemAllocStr(label);
  }
}

const cmChar_t* cmGrPlotObjLabel( cmGrPlObjH_t oh )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);
  return op->label;
}

void            cmGrPlotObjSetLabelAttr(  cmGrPlObjH_t oh, unsigned flags, int angle, const cmGrColor_t color )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);
  op->labelFlags = flags;
  op->labelAngle = angle;
  op->labelColor = color;
}

unsigned        cmGrPlotObjLabelFlags(    cmGrPlObjH_t oh )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);
  return op->labelFlags;
}

int             cmGrPlotObjLabelAngle(    cmGrPlObjH_t oh )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);
  return op->labelAngle;
}

cmGrColor_t cmGrPlotObjLabelColor(    cmGrPlObjH_t oh )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);
  return op->labelColor;
}

void       cmGrPlotObjSetStateFlags( cmGrPlObjH_t oh, unsigned flags )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);

  if( cmIsFlag(flags,kVisibleGrPlFl) != _cmGrPlotObjIsVisible(op) )
  {
    if( _cmGrPlotObjCb(op, kPreEventCbSelGrPlId, kVisibleGrPlFl ) == false )
      return;

    op->cfgFlags = cmTogFlag(op->cfgFlags,kNoDrawGrPlFl);

    _cmGrPlotObjCb(op, kStateChangeGrPlId, kVisibleGrPlFl );
  }

  if( cmIsFlag(flags,kEnabledGrPlFl) != _cmGrPlotObjIsEnabled(op) )
  {
    if( _cmGrPlotObjCb(op, kPreEventCbSelGrPlId, kEnabledGrPlFl ) == false )
      return;

    op->stateFlags = cmTogFlag(op->cfgFlags,kEnabledGrPlFl);

    _cmGrPlotObjCb(op, kStateChangeGrPlId, kEnabledGrPlFl );        
  }

  bool fl;
  if( cmIsFlag(flags,kSelectGrPlFl) != (fl=_cmGrPlotObjIsSelected(op)) )
    _cmGrPlotObjSetSelect(op, !fl );

  if( cmIsFlag(flags,kFocusGrPlFl) != (fl=_cmGrPlotObjIsFocused(op)) )
    if( fl )
      _cmGrPlotObjSetFocus(op);
  
}

unsigned   cmGrPlotObjStateFlags( cmGrPlObjH_t oh  )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);
  return  
      (_cmGrPlotObjIsEnabled(op)  ? kEnabledGrPlFl : 0) 
    | (_cmGrPlotObjIsVisible(op)  ? kVisibleGrPlFl : 0)
    | (_cmGrPlotObjIsFocused(op)  ? kFocusGrPlFl   : 0)
    | (_cmGrPlotObjIsSelected(op) ? kSelectGrPlFl  : 0);

}

void       cmGrPlotObjSetCfgFlags( cmGrPlObjH_t oh, unsigned flags )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);
  op->cfgFlags = flags;
}

void        cmGrPlotObjClrCfgFlags(   cmGrPlObjH_t oh, unsigned flags )
{
  unsigned curFlags = cmGrPlotObjCfgFlags(oh);
  cmGrPlotObjSetCfgFlags(oh, cmClrFlag(curFlags,flags));
}

void   cmGrPlotObjTogCfgFlags(   cmGrPlObjH_t oh, unsigned flags )
{
  unsigned curFlags = cmGrPlotObjCfgFlags(oh);
  cmGrPlotObjSetCfgFlags(oh, cmTogFlag(curFlags,flags));
}



unsigned   cmGrPlotObjCfgFlags( cmGrPlObjH_t oh  )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);
  return op->cfgFlags;
}

cmGrPlRC_t cmGrPlotObjSetPhysExt( cmGrPlObjH_t oh, int loffs, int toffs, int roffs, int boffs )
{  
  cmGrPlRC_t     rc = kOkGrPlRC;
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);
  
  op->loffs = loffs;
  op->toffs = toffs;
  op->roffs = roffs;
  op->boffs = boffs;

  return rc;
}

void  cmGrPlotObjPhysExt( cmGrPlObjH_t oh,    int* loffs, int* toffs, int* roffs, int* boffs  )
{
 cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);

 if( loffs != NULL )
   *loffs = op->loffs;

 if( toffs != NULL )
   *toffs = op->toffs;

 if( roffs != NULL )
   *roffs = op->roffs;

 if( boffs != NULL )
   *boffs = op->boffs;

}

void    cmGrPlotObjVExt(  cmGrPlObjH_t oh, cmGrVExt_t* vext  )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);
  _cmGrPlotObjGetVExt(op, vext);
}


void            cmGrPlotObjSetFontFamily( cmGrPlObjH_t oh, unsigned id )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);
  op->fontId = id;
}

unsigned        cmGrPlotObjFontFamily(    cmGrPlObjH_t oh )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);
  return op->fontId;
}

void            cmGrPlotObjSetFontStyle(  cmGrPlObjH_t oh, unsigned style )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);
  op->fontStyle = style;
}

unsigned        cmGrPlotObjFontStyle(     cmGrPlObjH_t oh )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);
  return op->fontStyle;
}

void            cmGrPlotObjSetFontSize(   cmGrPlObjH_t oh, unsigned size )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);
  op->fontSize = size;
}

unsigned        cmGrPlotObjFontSize(      cmGrPlObjH_t oh )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);
  return op->fontSize;
}

void            cmGrPlotObjSetLineColor(  cmGrPlObjH_t oh, cmGrPlStateId_t id, const cmGrColor_t c )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);
  assert( id < kMaxPlGrId );
  op->drawColors[ id ] = c;
}

cmGrColor_t cmGrPlotObjLineColor(     cmGrPlObjH_t oh, cmGrPlStateId_t id )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);
  assert( id < kMaxPlGrId );
  return op->drawColors[id];
}

cmGrColor_t cmGrPlotObjCurLineColor(  cmGrPlObjH_t h )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(h);
  return _cmGrPlotColor(op,op->drawColors);
}

void            cmGrPlotObjSetFillColor(  cmGrPlObjH_t oh, cmGrPlStateId_t id, const cmGrColor_t c )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);
  assert( id < kMaxPlGrId );
  op->fillColors[ id ] = c;
}

cmGrColor_t cmGrPlotObjFillColor(     cmGrPlObjH_t oh, cmGrPlStateId_t id )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(oh);
  assert( id < kMaxPlGrId );
  return op->fillColors[id];
}

cmGrColor_t cmGrPlotObjCurFillColor(  cmGrPlObjH_t h )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(h);
  return _cmGrPlotColor(op,op->fillColors);
}

void             cmGrPlotObjSetCb(        cmGrPlObjH_t h, cmGrPlotCbFunc_t func, void* arg )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(h);
  op->cbFunc = func;
  op->cbArg  = arg;
}

cmGrPlotCbFunc_t cmGrPlotObjCbFunc(       cmGrPlObjH_t h )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(h);
  return op->cbFunc;
}

void*            cmGrPlotObjCbArg(        cmGrPlObjH_t h )
{
  cmGrPlotObj_t* op = _cmGrPlObjHandleToPtr(h);
  return op->cbArg;
}

void            cmGrPlotObjDrawAbove(     cmGrPlObjH_t bH, cmGrPlObjH_t aH )
{
  cmGrPlotObj_t* bop  = _cmGrPlObjHandleToPtr(bH);  
  cmGrPlotObj_t* aop  = _cmGrPlObjHandleToPtr(aH);  
  cmGrObjDrawAbove(bop->grObjH,aop->grObjH);
}

//------------------------------------------------------------------------------------------------------------------
// Plot Object Manager Functions
//------------------------------------------------------------------------------------------------------------------

cmGrPlRC_t cmGrPlotCreate(  cmCtx_t* ctx, cmGrPlH_t* hp )
{
  cmGrPlRC_t rc;
  if((rc = cmGrPlotDestroy(hp)) != kOkGrPlRC )
    return rc;

  cmGrPl_t* p = cmMemAllocZ(cmGrPl_t,1);
  cmErrSetup(&p->err,&ctx->rpt,"cmGrPlot");
  p->ctx = ctx;

  hp->h = p;
  
  if( rc != kOkGrPlRC )
    _cmGrPlotDestroy(p);

  return rc;
}

cmGrPlRC_t cmGrPlotDestroy( cmGrPlH_t* hp )
{
  cmGrPlRC_t rc;
  
  if( hp==NULL || cmGrPlotIsValid(*hp) == false )
    return kOkGrPlRC;

  cmGrPl_t* p = _cmGrPlHandleToPtr(*hp);

  if((rc = _cmGrPlotDestroy(p)) != kOkGrPlRC )
    return rc;

  hp->h = NULL;

  return rc;
}

bool       cmGrPlotIsValid( cmGrPlH_t h )
{ return h.h != NULL; }

cmGrPlRC_t cmGrPlotClear( cmGrPlH_t h )
{
 cmGrPl_t*      p  = _cmGrPlHandleToPtr(h);
 
 return _cmGrPlotClear(p);
}

cmErr_t*     cmGrPlotErr(     cmGrPlH_t h )
{
 cmGrPl_t*      p  = _cmGrPlHandleToPtr(h);
 return &p->err;
}

cmRpt_t*     cmGrPlotRpt(    cmGrPlH_t h )
{
 cmGrPl_t*      p  = _cmGrPlHandleToPtr(h);
 return p->err.rpt;
}

cmGrPlObjH_t cmGrPlotObjectIdToHandle( cmGrPlH_t h, unsigned id )
{
  cmGrPl_t*      p  = _cmGrPlHandleToPtr(h);
  cmGrPlObjH_t   oh = cmGrPlObjNullHandle;
  cmGrPlotObj_t* op = p->list;

  for(; op!=NULL; op=op->next)
    if( cmGrObjId(op->grObjH) == id )
    {
      oh.h = op;
      break;
    }

  return oh;
}

unsigned     cmGrPlotObjectCount(   cmGrPlH_t h )
{
  cmGrPl_t*       p = _cmGrPlHandleToPtr(h);
  cmGrPlotObj_t* op = p->list;
  unsigned        n = 0;
  for(; op!=NULL; ++n )
    op=op->next;
  return n;
}

cmGrPlObjH_t cmGrPlotObjectIndexToHandle( cmGrPlH_t h, unsigned index )
{
  cmGrPl_t*       p = _cmGrPlHandleToPtr(h);
  cmGrPlotObj_t* op = p->list;
  unsigned        i = 0;
  cmGrPlObjH_t   oh = cmGrPlObjNullHandle;

  for(; i<index && op!=NULL; ++i)
    op = op->next;

  if( op != NULL )
    oh.h = op;

  return oh;
}

void         cmGrPlotObjCb( cmGrPlH_t h, cmGrPlotObjCbFunc_t func, void* arg  )
{
  cmGrPl_t*       p = _cmGrPlHandleToPtr(h);
  cmGrPlotObj_t* op = p->list;
  cmGrPlObjH_t   oh = cmGrPlObjNullHandle;

  for(; op!=NULL; op=op->next)
  {
    oh.h = op;
    func(arg,oh);
  }
}

void  cmGrPlotKeyEvent(   cmGrPlH_t h, cmGrH_t grH, unsigned eventFlags, cmGrKeyCodeId_t keycode )
{
  assert( cmIsFlag(eventFlags,kKeyDnGrFl | kKeyUpGrFl));

  cmGrPl_t*      p  = _cmGrPlHandleToPtr(h);
  if( p->fop != NULL && cmHandlesAreEqual(p->fop->grH,grH) )
  {
    cmGrObjFuncArgs_t a;
    memset(&a,0,sizeof(a));
    a.cbArg = p->fop;
    a.ctx = p->ctx;
    a.grH = grH;
    a.objH = p->fop->grObjH;

    _cmGrPlotObjEvent(&a, eventFlags, keycode, 0, 0 );   
  }   
}

void  cmGrPlotSetCb( cmGrPlH_t h, cmGrPlotCbFunc_t func, void* arg )
{
  cmGrPl_t*      p  = _cmGrPlHandleToPtr(h);
  p->cbFunc = func;
  p->cbArg  = arg;
}
