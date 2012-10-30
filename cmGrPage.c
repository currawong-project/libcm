#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmErr.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmGr.h"
#include "cmGrDevCtx.h"
#include "cmGrPlot.h"
#include "cmGrPage.h"
#include "cmVectOpsTemplateMain.h"


enum
{
  kHashLength  = 6,
  kHashCharCnt = 10,
  kMaxHashCnt  = 10
}; 

enum
{
  kNotValidFl = 0x01,
  kDirtyFl    = 0x02,    // set if the layout is dirty
  kFocusFl    = 0x04
};

typedef struct
{
  cmGrPPt_t xy0;        // 
  cmGrPPt_t xy1;        // xy1 is on iext
} cmGrPgHash_t;

struct cmGrPgVw_str;
struct cmGrPgAxis_str;
struct cmGrPg_str;

typedef struct cmGrPgLabelFunc_str
{
  unsigned                    id;
  cmGrLabelFunc_t             func;
  void*                       arg;
  cmChar_t*                   label;
  struct cmGrPgLabelFunc_str* link;
} cmGrPgLabelFunc_t;

typedef struct cmGrPgAxis_str
{
  struct cmGrPgVw_str* vp;
  unsigned             flags;      // kHashMarkGrFl | kHashLabelGrFl
  unsigned             hashCnt;
  unsigned             hashLength;
  cmChar_t*            title;

  cmGrPgHash_t* hash;

  unsigned      maxHashCnt;

  cmGrPPt_t     titlePt;        // position of the axis title 
  unsigned      titleFontId;    // title font id
  unsigned      titleFontStyle; // title font style
  unsigned      titleFontSize;  // title font size

  cmGrPPt_t     labelPt;        // x = l/r label pos. and y=t/b label pos.
  unsigned      labelFontId;    // label font id
  unsigned      labelFontStyle; // label font style
  unsigned      labelFontSize;  // label font size

  cmGrPgLabelFunc_t* func;
} cmGrPgAxis_t;

typedef struct cmGrPgVw_str
{
  struct cmGrPg_str* p;
  cmGrH_t            grH;
  unsigned           ri;        // row index
  unsigned           ci;        // column index
  cmGrPExt_t         pext;      // page around outside of view
  cmGrPExt_t         iext;      // view physical extents
  cmGrPgAxis_t       axis[ kAxisGrCnt ]; // 
  cmGrPPt_t          titlePt;   // lower/center position of the view title
  cmChar_t*          title;     // view title (upper center)    
  unsigned           fontId;    // title font id
  unsigned           fontStyle; // title font style
  unsigned           fontSize;  // title font size

  cmGrPgLabelFunc_t*  xfunc;
  cmGrPgLabelFunc_t*  yfunc;
} cmGrPgVw_t;

typedef struct cmGrPg_str
{
  cmCtx_t       ctx;
  cmErr_t       err;
  cmGrCbFunc_t  cbFunc;
  void*         cbArg;
  unsigned      flags;            // kNotValidFl
  cmGrPExt_t    pext;             // outer border around all views
  unsigned      rn;               // count of rows
  unsigned      cn;               // count of columns
  unsigned      vn;               // view count (rn*cn)
  double*       rpropV;           // sum(rprop[rn]) == 1.0 pgaction of r.w() assigned to each row
  double*       cpropV;           // sum(cprop[cn]) == 1.0 pgaction of r.h() assigned to each row
  cmGrPgVw_t*   viewV;            // viewV[vn] 
  unsigned      focusIdx;         // focused view index

  cmGrPPt_t     titlePt;          //
  cmChar_t*     title;            // page title
  unsigned      fontId;           // title font id
  unsigned      fontStyle;        // title font style
  unsigned      fontSize;         // title font size

  unsigned           labelFuncId; // next page label function id
  cmGrPgLabelFunc_t* funcs;       // linked list of value to string translation functions

} cmGrPg_t;

cmGrPgH_t cmGrPgNullHandle = cmSTATIC_NULL_HANDLE;
cmGrVwH_t cmGrVwNullHandle = cmSTATIC_NULL_HANDLE;
cmGrAxH_t cmGrAxNullHandle = cmSTATIC_NULL_HANDLE;

cmGrPg_t* _cmGrPgHandleToPtr( cmGrPgH_t h )
{
  cmGrPg_t* p = (cmGrPg_t*)h.h;
  assert( p!=NULL);
  return p;
}

cmGrPgVw_t* _cmGrPgVwHandleToPtr( cmGrVwH_t h )
{
  cmGrPgVw_t* p = (cmGrPgVw_t*)h.h;
  assert( p!=NULL);
  return p;
}

cmGrPgAxis_t* _cmGrPgAxisHandleToPtr( cmGrAxH_t h )
{
  cmGrPgAxis_t* p = (cmGrPgAxis_t*)h.h;
  assert( p!=NULL);
  return p;
}


void _cmGrPgVwAxisDestroy( cmGrPgAxis_t* ap )
{
  ap->func  = NULL;

  cmMemFree(ap->hash);
  cmMemFree(ap->title);
}

void _cmGrPgVwDestroy( cmGrPgVw_t* vp )
{
  unsigned i;

  cmGrDestroy( &vp->grH );

  cmMemFree(vp->title);
  for(i=0; i<kAxisGrCnt; ++i)
    _cmGrPgVwAxisDestroy(vp->axis + i );
}

void _cmGrPgFinal( cmGrPg_t* p )
{
  unsigned i;
  for(i=0; i<p->vn; ++i)
    _cmGrPgVwDestroy( p->viewV + i );

  cmGrPgLabelFunc_t* lfp = p->funcs;
  while( lfp != NULL )
  {
    cmGrPgLabelFunc_t* np = lfp->link;
    cmMemFree(lfp->label);
    cmMemFree(lfp);
    lfp = np;
  }

  p->funcs = NULL;
  
  cmMemFree(p->viewV);
  cmMemFree(p->title);
  cmMemFree(p->rpropV);
  cmMemFree(p->cpropV);
}

cmGrRC_t _cmGrPgDestroy( cmGrPg_t* p )
{
  cmGrRC_t rc = kOkGrRC;
  _cmGrPgFinal(p);
  cmMemFree(p);
  return rc;
}

unsigned  _cmGrPgGrHandleToVwIndex( cmGrPg_t* p, cmGrH_t grH )
{
  unsigned i;
  for(i=0; i<p->vn; ++i)
    if( cmHandlesAreEqual(p->viewV[i].grH,grH) )
      return i;

  return cmInvalidIdx;
}

void _cmGrPageCallback( void* arg, cmGrH_t grH, cmGrCbId_t id, unsigned eventFlags, cmGrKeyCodeId_t keycode )
{
  cmGrPg_t* p = (cmGrPg_t*)arg;

  switch(id)
  {
    case kCreateCbGrId:
    case kDestroyCbGrId:
    case kLocalPtCbGrId:
    case kGlobalPtCbGrId:
    case kPhysExtCbGrId:
    case kViewExtCbGrId:
    case kSelectExtCbGrId:
      if( p->cbFunc != NULL )
        p->cbFunc(p->cbArg,grH,id,eventFlags,keycode);
      break;

    case kKeyUpCbGrId:
    case kKeyDnCbGrId:
      {
        cmGrVwH_t vwH;
        cmGrPgH_t pgH;
        pgH.h = p;
        if(cmGrViewIsValid(vwH =cmGrPageFocusedView(pgH)))
          if( p->cbFunc != NULL )
            p->cbFunc(p->cbArg,cmGrViewGrHandle(vwH),id,eventFlags,keycode);
          
      }
      break;

    case kFocusCbGrId:
      {
        unsigned i;
        if((i = _cmGrPgGrHandleToVwIndex(p,grH)) != cmInvalidIdx )
        {
          cmGrPgH_t h;
          h.h = p;

          // if the focus is changing
          if( i != p->focusIdx )
          {
            // inform the prev view that it is losing focus
            if( p->focusIdx != cmInvalidIdx )
              cmGrPageViewFocus(h,p->focusIdx,false);

            // inform the new view that it is gaining focus
            cmGrPageViewFocus(h,i,true);

            if( p->cbFunc != NULL )
              p->cbFunc(p->cbArg,grH,id,eventFlags,keycode);
          }
        }
      }
      break;

    default:
      { assert(0); }
  }


} 



cmGrRC_t cmGrPageCreate( cmCtx_t* ctx, cmGrPgH_t* hp, cmGrCbFunc_t cbFunc, void* cbArg )
{
  cmGrRC_t rc;
  if((rc = cmGrPageDestroy(hp)) != kOkGrRC )
    return rc;

  cmGrPg_t* p = cmMemAllocZ(cmGrPg_t,1);

  cmErrSetup(&p->err,&ctx->rpt,"cmGrPage");
  p->cbFunc    = cbFunc;
  p->cbArg     = cbArg;
  p->ctx       = *ctx;
  hp->h        = p;

  if(rc != kOkGrRC )
    _cmGrPgDestroy(p);
  return rc;
}

cmGrRC_t cmGrPageDestroy( cmGrPgH_t* hp )
{
  cmGrRC_t rc;
  if(hp==NULL || cmGrPageIsValid(*hp) == false )
    return kOkGrRC;

  cmGrPg_t* p = _cmGrPgHandleToPtr(*hp);


  if((rc = _cmGrPgDestroy(p)) != kOkGrRC )
    return rc;

  hp->h = NULL;

  return rc;
}

bool     cmGrPageIsValid( cmGrPgH_t h )
{ return h.h != NULL; }

cmGrRC_t cmGrPageClear( cmGrPgH_t h )
{
  cmGrRC_t rc = kOkGrRC;
  unsigned i;
  for(i=0; i<cmGrPageViewCount(h); ++i)
  {
    cmGrVwH_t vwH = cmGrPageViewHandle(h,i);
    assert( cmGrViewIsValid(vwH) );
    if((rc = cmGrViewClear(vwH)) != kOkGrRC )
      break;
  }

  return rc;
}


// Calcuate the position of each view using the p->pext, and the row/col proportion vectors
// This function calculates the outer page around each view.
// It must be called whenever the size of the window holding the page changes.
bool _cmGrPgLayoutVwPosition( cmGrPg_t* p )
{
  enum { kVBord=4, kHBord=4 };

  // verify that the page state is sane
  if( p->rn==0 || p->cn==0 || p->pext.sz.h==0 || p->pext.sz.w==0)
    goto errLabel;

  else
  {
    unsigned i,j;
    int x=p->pext.loc.x;
    int y=p->pext.loc.y;

    // calculate the total page width/height w/o internal borders
    int h = p->pext.sz.h - ((p->rn - 1) * kVBord);
    int w = p->pext.sz.w - ((p->cn - 1) * kHBord);

    // verify that the page area exists
    if( h<=0 || w<=0 )
      goto errLabel;

    // force the row/col proportion vectors to sum to 1.0
    cmVOD_NormalizeProbability(p->rpropV,p->rn);
    cmVOD_NormalizeProbability(p->cpropV,p->cn);

    // determine the row height
    for(i=0; i<p->rn; ++i)
    {
      unsigned hh = (unsigned)floor(p->rpropV[i] * h);   

      if( hh == 0 )
        goto errLabel;

      for(j=0; j<p->vn; ++j)
        if( p->viewV[j].ri == i )
        {
          p->viewV[j].pext.loc.y = y;
          p->viewV[j].pext.sz.h  = hh;
        }

      y += hh + kVBord;
    }

    // determine the column width
    for(i=0; i<p->cn; ++i)
    {
      unsigned ww = (unsigned)floor(p->cpropV[i] * w);    

      if( ww == 0 )
        goto errLabel;

      for(j=0; j<p->vn; ++j)
        if( p->viewV[j].ci == i )
        {
          p->viewV[j].pext.loc.x = x;
          p->viewV[j].pext.sz.w  = ww;
        }

      x += ww + kHBord;
    }

    p->flags = cmClrFlag(p->flags,kNotValidFl);


    return true;
  }

 errLabel:
  p->flags = cmSetFlag(p->flags,kNotValidFl);

  return false;
}

// Calculate the layout for a given view.  
// txW = max hash label width on x-axis
// txH = hash label height on x-axis
void _cmGrPgLayoutView( cmGrPg_t* p, cmGrPgVw_t* vp, cmGrDcH_t dcH )
{
  enum { kVBord=2, kHBord=2 };

  int x0 = vp->pext.loc.x + kHBord;  
  int y0 = vp->pext.loc.y + kVBord;
  int w  = vp->pext.sz.w  - 2*kHBord;
  int h  = vp->pext.sz.h  - 2*kVBord;

  int y  = y0;
  int x  = x0;

  cmGrPSz_t sz;
  

  // Create a negative string with a repeating decimal to simulate an arbitrary hash label
  // Use the measurements pgom this string to compute the geometry of the view layouts.
  char label[ kHashCharCnt + 1];
  snprintf(label,kHashCharCnt,"%f",-4.0/3.0); 
  label[kHashCharCnt] = 0;

  int mlx,mrx,mty,mby;
  int i;

  // add vertical space for the view title
  if( vp->title != NULL )
  {
    cmGrDcFontSetAndMeasure(dcH, vp->fontId, vp->fontSize, vp->fontStyle, vp->title, &sz );
    y += sz.h;
    vp->titlePt.y = y;
    vp->titlePt.x = vp->pext.loc.x + vp->pext.sz.w/2;
    y += 2;    
  }

  cmGrPgAxis_t* ap = vp->axis + kTopGrIdx;

  // add vertical space for the top axis  title
  if( ap->title != NULL )
  {
    cmGrDcFontSetAndMeasure(dcH, ap->titleFontId, ap->titleFontSize, ap->titleFontStyle, ap->title, &sz );
    y += sz.h;
    ap->titlePt.y = y;
    y += 4;
  }

  // add vertical space for top axis hash labels
  if( cmIsFlag(ap->flags,kHashLabelGrFl ) )
  {
    cmGrDcFontSetAndMeasure(dcH, ap->labelFontId, ap->labelFontSize, ap->labelFontStyle, label, &sz );
    y += sz.h;
    ap->labelPt.y = y;
    y += 1;
  }

  // calc vertical space for top axis hash marks
  if( cmIsFlag(ap->flags,kHashMarkGrFl ) )
  {
    mty = y;
    y += ap->hashLength + 1;        
  }
  
  // set the internal pext vertical location
  vp->iext.loc.y = y;

  ap = vp->axis + kBottomGrIdx;
  y  = y0 + h - 1 - kVBord;

  // subtract vertical space for bottom axis title
  if( ap->title != NULL )
  {
    cmGrDcFontSetAndMeasure(dcH, ap->titleFontId, ap->titleFontSize, ap->titleFontStyle, ap->title, &sz );
    ap->titlePt.y = y;
    y -= sz.h + 4;
  }

  // calc vertical space for bottom axis hash label
  if( cmIsFlag(ap->flags,kHashLabelGrFl) )
  {
    cmGrDcFontSetAndMeasure(dcH, ap->labelFontId, ap->labelFontSize, ap->labelFontStyle, label, &sz );
    ap->labelPt.y = y;
    y -= sz.h + 2;
  }

  // calc vertical space for bottom axis hash mark
  if( cmIsFlag(ap->flags,kHashMarkGrFl) )
  {
    mby = y;
    y -= ap->hashLength + 1;
  }

  // set the internal pext height
  vp->iext.sz.h = y - vp->iext.loc.y + 1;


  ap = vp->axis + kLeftGrIdx;

  // add horizontal space for the left axis  title
  if( ap->title != NULL )
  {
    cmGrDcFontSetAndMeasure(dcH, ap->titleFontId, ap->titleFontSize, ap->titleFontStyle, ap->title, &sz );

    x += sz.h;  // use txH as approx of char. width for rotated title
    ap->titlePt.x = x;
    x += 4;     
  }

  // add horizontal space for left axis hash labels
  if( cmIsFlag(ap->flags,kHashLabelGrFl ) )
  {
    cmGrDcFontSetAndMeasure(dcH, ap->labelFontId, ap->labelFontSize, ap->labelFontStyle, label, &sz );

    ap->labelPt.x = x;
    x += sz.w + 3;    
  }

  // calc horizontal space for left axis hash marks
  if( cmIsFlag(ap->flags,kHashMarkGrFl ) )
  {
    mlx = x;
    x += ap->hashLength + 1;        
  }
  
  // set the internal pext horz location
  vp->iext.loc.x = x;

  ap = vp->axis + kRightGrIdx;
  x  = x0 + w - 1 - kVBord;

  // subtract horizontal space for right axis title
  if( ap->title != NULL )
  {
    cmGrDcFontSetAndMeasure(dcH, ap->titleFontId, ap->titleFontSize, ap->titleFontStyle, ap->title, &sz );

    ap->titlePt.x = x;
    x -= sz.h + 2;       // use txH as approx of char. width for rotated title
  }

  // calc horizontal space for right axis hash label
  if( cmIsFlag(ap->flags,kHashLabelGrFl) )
  {
    cmGrDcFontSetAndMeasure(dcH, ap->labelFontId, ap->labelFontSize, ap->labelFontStyle, label, &sz );

    x -= sz.w + 1;
    ap->labelPt.x = x;
    x -= 3;
  }

  // calc horizontal space for right axis hash mark
  if( cmIsFlag(ap->flags,kHashMarkGrFl) )
  {
    mrx = x;
    x -= ap->hashLength + 1;
  }

  // set the internal pext width
  vp->iext.sz.w = x - vp->iext.loc.x + 1;
  

  // calc the top hash count and alloc hash array
  ap = vp->axis + kTopGrIdx;
  cmGrDcFontSetAndMeasure(dcH, ap->labelFontId, ap->labelFontSize, ap->labelFontStyle, label, &sz );
  ap->hashCnt = cmMin(ap->maxHashCnt,(vp->iext.sz.w + sz.w) / sz.w);
  ap->hash    = cmMemResizeZ( cmGrPgHash_t, ap->hash,    vp->axis[ kTopGrIdx ].hashCnt );

  // calc the bottom hash count and alloc hash array
  ap = vp->axis + kBottomGrIdx;
  cmGrDcFontSetAndMeasure(dcH, ap->labelFontId, ap->labelFontSize, ap->labelFontStyle, label, &sz );
  ap->hashCnt = cmMin(ap->maxHashCnt,(vp->iext.sz.w + sz.w) / sz.w);
  ap->hash    = cmMemResizeZ( cmGrPgHash_t, ap->hash, vp->axis[ kBottomGrIdx ].hashCnt );
  
  // calc the top hash mark line beg/end
  for(i=0; i< vp->axis[ kTopGrIdx ].hashCnt; ++i)
  {
    int x0 = round(vp->iext.loc.x - 1 + ((i * (vp->iext.sz.w + 1.0))/(vp->axis[ kTopGrIdx ].hashCnt-1)));

    vp->axis[kTopGrIdx].hash[i].xy0.y = mty;
    vp->axis[kTopGrIdx].hash[i].xy0.x = x0;
    vp->axis[kTopGrIdx].hash[i].xy1.y = vp->iext.loc.y - 1;
    vp->axis[kTopGrIdx].hash[i].xy1.x = x0;
  }

  // calc the bottom hash mark line beg/end
  for(i=0; i< vp->axis[ kBottomGrIdx ].hashCnt; ++i)
  {
    int x0 = round(vp->iext.loc.x - 1 + ((i * (vp->iext.sz.w + 1.0))/(vp->axis[ kBottomGrIdx ].hashCnt-1)));

    vp->axis[kBottomGrIdx].hash[i].xy0.y = mby;
    vp->axis[kBottomGrIdx].hash[i].xy0.x = x0;
    vp->axis[kBottomGrIdx].hash[i].xy1.y = cmGrPExtB(&vp->iext) + 1;
    vp->axis[kBottomGrIdx].hash[i].xy1.x = x0;    
  }

  // calc the left hash count and alloc hash array
  ap = vp->axis + kLeftGrIdx;
  cmGrDcFontSetAndMeasure(dcH, ap->labelFontId, ap->labelFontSize, ap->labelFontStyle, label, &sz );
  ap->hashCnt    = cmMin(ap->maxHashCnt,(vp->iext.sz.h + sz.h) / sz.h);
  ap->hash = cmMemResizeZ( cmGrPgHash_t, ap->hash, vp->axis[ kLeftGrIdx ].hashCnt );

  // calc right hash count and alloc hash array
  ap = vp->axis + kRightGrIdx;
  cmGrDcFontSetAndMeasure(dcH, ap->labelFontId, ap->labelFontSize, ap->labelFontStyle, label, &sz );
  ap->hashCnt   = cmMin(ap->maxHashCnt,(vp->iext.sz.h + sz.h) / sz.h);
  ap->hash = cmMemResizeZ( cmGrPgHash_t, ap->hash, vp->axis[ kRightGrIdx ].hashCnt );

  
  // calc the left hash mark beg/end
  for(i=0; i< vp->axis[ kLeftGrIdx ].hashCnt; ++i)
  {
    int y0 = round(vp->iext.loc.y - 1 + ((i * (vp->iext.sz.h + 1.0))/(vp->axis[ kLeftGrIdx ].hashCnt-1)));

    vp->axis[kLeftGrIdx].hash[i].xy0.x = mlx;
    vp->axis[kLeftGrIdx].hash[i].xy0.y = y0;
    vp->axis[kLeftGrIdx].hash[i].xy1.x = vp->iext.loc.x - 1;
    vp->axis[kLeftGrIdx].hash[i].xy1.y = y0;
  }

  // calc the right hash mark beg/end
  for(i=0; i< vp->axis[ kRightGrIdx ].hashCnt; ++i)
  {
    int y0 = round(vp->iext.loc.y - 1 + ((i * (vp->iext.sz.h + 1.0))/(vp->axis[ kRightGrIdx ].hashCnt-1)));

    vp->axis[kRightGrIdx].hash[i].xy0.x = mrx;
    vp->axis[kRightGrIdx].hash[i].xy0.y = y0;
    vp->axis[kRightGrIdx].hash[i].xy1.x = cmGrPExtR(&vp->iext) + 1;
    vp->axis[kRightGrIdx].hash[i].xy1.y = y0;
  }

  // change the location of the view to match vp->iext
  if( cmGrIsValid( vp->grH ) )
    cmGrSetPhysExtentsE( vp->grH, &vp->iext );

}

void  _cmGrPageLayout( cmGrPg_t* p, cmGrDcH_t dcH )
{
  unsigned  i;

  if( cmIsNotFlag(p->flags,kDirtyFl) )
    return;

  cmGrDcSetFontFamily(dcH,kHelveticaFfGrId);
  cmGrDcSetFontSize(dcH,10);

  // Create a negative string with a repeating decimal to simulate an arbitrary hash label
  // Use the measurements pgom this string to compute the geometry of the view layouts.
  char label[ kHashCharCnt + 1];
  cmGrPSz_t sz;
  snprintf(label,kHashCharCnt,"%f",-4.0/3.0); 
  label[kHashCharCnt] = 0;
  cmGrDcMeasure(dcH,label,&sz);

  _cmGrPgLayoutVwPosition(p);

  for(i=0; i<p->vn; ++i)
  {
    cmGrPgVw_t* vp = p->viewV + i;

    _cmGrPgLayoutView(p, vp, dcH );
  }

  p->flags = cmClrFlag(p->flags,kDirtyFl);
}

cmGrRC_t cmGrPageInit( cmGrPgH_t h, const cmGrPExt_t* r, unsigned rn, unsigned cn, cmGrDcH_t dcH  )
{
  cmGrPg_t* p = _cmGrPgHandleToPtr(h);
  unsigned i;

  _cmGrPgFinal(p);

  if( rn*cn > 0 )
  {
    p->pext      = *r;
    p->rn        = rn;
    p->cn        = cn;
    p->vn        = rn*cn;
    p->rpropV    = cmMemAllocZ(double,     rn);
    p->cpropV    = cmMemAllocZ(double,     cn);
    p->viewV     = cmMemAllocZ(cmGrPgVw_t, p->vn);
    p->flags     = kDirtyFl; 
    p->focusIdx  = cmInvalidIdx;
    p->fontId    = kHelveticaFfGrId;
    p->fontStyle = kNormalFsGrFl;
    p->fontSize  = 16;

    // setup the view defaults
    for(i=0; i<p->vn; ++i)
    {
      cmGrPgVw_t* vp = p->viewV + i;

      vp->p         = p;
      vp->ri        = i % p->rn;
      vp->ci        = i / p->rn;
      vp->fontId    = kHelveticaFfGrId;
      vp->fontStyle = kNormalFsGrFl;
      vp->fontSize  = 14;

      unsigned j;
      for(j=0; j<kAxisGrCnt; ++j)
      {
        cmGrPgAxis_t* ap = vp->axis + j;

        ap->vp             = p->viewV + i;
        ap->maxHashCnt     = kMaxHashCnt;
        ap->hashLength     = kHashLength;
        ap->flags          = kHashMarkGrFl | kHashLabelGrFl;
        ap->titleFontId    = kHelveticaFfGrId;
        ap->titleFontStyle = kNormalFsGrFl;
        ap->titleFontSize  = 10;
        ap->labelFontId    = kHelveticaFfGrId;
        ap->labelFontStyle = kNormalFsGrFl;
        ap->labelFontSize  = 10;
      }

    } 

    // setup the default row proportions
    for(i=0; i<p->rn; ++i)
      p->rpropV[i] = 1.0/p->rn;

    // setup the default col proportions
    for(i=0; i<p->cn; ++i)
      p->cpropV[i] = 1.0/p->cn;
  
    // _cmGrPgLayoutVw() needs to be called.
    p->flags = cmSetFlag(p->flags,kDirtyFl);

    // layout the page
    _cmGrPageLayout(p, dcH );

    // notify the application that views have been created
    for(i=0; i<rn*cn; ++i)
    {
      cmGrPgVw_t* vp = p->viewV + i;
      
      // Set the 'id' assoc'd with this views cmGrH_t handle to 'i'. 
      // This will allow the grH to return the index of the plot.
      if( cmGrCreate(&p->ctx,&vp->grH,i,kExpandViewGrFl,_cmGrPageCallback,p,NULL) == kOkGrRC )
      {
        //if( p->rspdr != NULL )
        //  p->rspdr->on_view_create( p->rspdrArg, i );
      }
    }
  }    
  return kOkGrRC;
}



cmGrRC_t cmGrPageResize( cmGrPgH_t h, const cmGrPExt_t* r, cmGrDcH_t dcH )
{
  cmGrPg_t* p = _cmGrPgHandleToPtr(h);
  p->pext = *r;

  // _cmGrPgLayoutVw() needs to be called.
  p->flags = cmSetFlag(p->flags,kDirtyFl);

  // layout the page
  _cmGrPageLayout(p, dcH );

  return kOkGrRC;
}

void  cmGrPageRect(   cmGrPgH_t h, cmGrPExt_t* r )
{
  cmGrPg_t* p = _cmGrPgHandleToPtr(h);
  *r = p->pext;
}

unsigned cmGrPageViewCount( cmGrPgH_t h )
{
  cmGrPg_t* p = _cmGrPgHandleToPtr(h);
  return p->vn;
}

void _cmGrViewSetTitle(cmGrPgVw_t* vp, const cmChar_t* title )
{ 
  if( title == vp->title || (title != NULL && vp->title!=NULL && strcmp(title,vp->title)==0 ))
    return;
  
  cmMemPtrFree(&vp->title);
  if( title != NULL )
  {
    assert( vp->title == NULL );
    vp->title = cmMemAllocStr(title);
  }

  vp->p->flags = cmSetFlag(vp->p->flags, kDirtyFl);
}

void _cmGrAxisSetTitle( cmGrPgAxis_t* ap, const cmChar_t* title )
{  
  if( title == ap->title || (title != NULL && ap->title!=NULL && strcmp(title,ap->title)==0 ))
    return;

  cmMemPtrFree(&ap->title);
  if( title != NULL )
  {
    assert( ap->title == NULL );
    ap->title = cmMemAllocStr(title);
  }

  ap->vp->p->flags = cmSetFlag(ap->vp->p->flags, kDirtyFl);  

}


void _cmGrPgDrawHashMarks( cmGrPg_t* p, cmGrPgVw_t* vp, cmGrDcH_t dcH, unsigned lineColor )
{
  int i,j;
  cmGrDcSetColor(dcH, lineColor );
  cmGrDcSetPenWidth( dcH, 1 );

  for(j=0; j<kAxisGrCnt; ++j )
  {
    cmGrPgAxis_t* ap = vp->axis + j;

    if( cmIsFlag(ap->flags, kHashMarkGrFl) )
      for(i=0; i<ap->hashCnt; ++i)
      {
        cmGrPgHash_t* hp = ap->hash + i;
        cmGrDcDrawLine(dcH, hp->xy0.x, hp->xy0.y, hp->xy1.x, hp->xy1.y);
      }
  }

  // draw border 1 pixel outside of iext
  cmGrDcDrawRect(dcH,vp->iext.loc.x-1,vp->iext.loc.y-1,vp->iext.sz.w+2,vp->iext.sz.h+2);

  //printf("pgm: x:%i y:%i\n", vp->iext.loc.x, vp->iext.loc.y);

  // draw the view border
  //cmGrDcDrawRect(dcH,vp->pext.loc.x,vp->pext.loc.y,vp->pext.sz.w,vp->pext.sz.h);

}

void _cmGrPgHashValueToLabel( cmGrPgAxis_t* ap, cmChar_t* label, unsigned labelCharCnt, cmGrV_t value )
{
  if( ap->func == NULL )
    snprintf(label,labelCharCnt,"%f",value);
  else
  {
    ap->func->func( ap->func->arg, label, labelCharCnt, value );
  }
}

void _cmGrPgDrawHashLabels( cmGrPg_t* p, cmGrPgVw_t* vp, cmGrDcH_t dcH, unsigned textColor )
{
  int        i;
  cmGrVExt_t vext;
  char       s[ kHashCharCnt+1 ];
  s[kHashCharCnt]=0;

  cmGrViewExtents( vp->grH, &vext );
  //cmGrVExtPrint("hash:",&vext);

  cmGrDcSetColor(dcH, textColor );

  cmGrPgAxis_t* ap = vp->axis + kLeftGrIdx;
  cmGrDcSetFont( dcH, ap->labelFontId, ap->labelFontSize, ap->labelFontStyle );

  // draw the left axis hash labels
  if( cmIsFlag(ap->flags, kHashLabelGrFl ) )   
  {
    for(i=0; i<vp->axis[ kLeftGrIdx ].hashCnt; ++i)
    {
      cmGrPSz_t sz;
      double    v = vext.loc.y + vext.sz.h - (i * vext.sz.h / (ap->hashCnt-1) ); 
      _cmGrPgHashValueToLabel(ap,s,kHashCharCnt,v);
      cmGrDcMeasure(dcH,s,&sz);
    
      int y = ap->hash[i].xy0.y;
      cmGrDcDrawText(dcH, s,  ap->labelPt.x, y + (sz.h/2) );
    }
  }

  ap = vp->axis + kRightGrIdx;
  cmGrDcSetFont( dcH, ap->labelFontId, ap->labelFontSize, ap->labelFontStyle );

  // draw the right axis hash labels
  if( cmIsFlag(ap->flags, kHashLabelGrFl ))
  {
    for(i=0; i<ap->hashCnt; ++i)
    {
      cmGrPSz_t sz;
      double    v = vext.loc.y + vext.sz.h - (i * vext.sz.h / (ap->hashCnt-1) );    
      _cmGrPgHashValueToLabel(ap,s,kHashCharCnt,v);
      cmGrDcMeasure(dcH,s,&sz);
    
      int y = ap->hash[i].xy0.y;
      cmGrDcDrawText(dcH, s,  ap->labelPt.x, y + (sz.h/2));
    }
  }


  // draw the top axis hash labels
  ap = vp->axis + kTopGrIdx;
  cmGrDcSetFont( dcH, ap->labelFontId, ap->labelFontSize, ap->labelFontStyle );

  if( cmIsFlag(ap->flags, kHashLabelGrFl ) )
  {
    for(i=0; i<ap->hashCnt; ++i)
    {
      cmGrPSz_t sz;
      double    v = vext.loc.x + (i * vext.sz.w / (ap->hashCnt-1));    
      _cmGrPgHashValueToLabel(ap,s,kHashCharCnt,v);
      cmGrDcMeasure(dcH,s,&sz);

      cmGrDcDrawText(dcH, s,  ap->hash[i].xy0.x - sz.w/2, ap->labelPt.y );
    }
  }

  // draw the bottom axis hash labels
  ap = vp->axis + kBottomGrIdx;
  cmGrDcSetFont( dcH, ap->labelFontId, ap->labelFontSize, ap->labelFontStyle );

  if( cmIsFlag(ap->flags, kHashLabelGrFl ) )    
  {
    for(i=0; i<ap->hashCnt; ++i)
    {
      cmGrPSz_t sz;
      double    v = vext.loc.x + (i * vext.sz.w / (ap->hashCnt-1));    
      _cmGrPgHashValueToLabel(ap,s,kHashCharCnt,v);
      cmGrDcMeasure(dcH,s,&sz);

      cmGrDcDrawText(dcH, s, ap->hash[i].xy0.x - sz.w/2, ap->labelPt.y );
    }
  }

}

void  _cmGrPgDrawAxisTitles( cmGrPg_t* p, cmGrPgVw_t* vp, cmGrDcH_t dcH )
{
  unsigned i;
  for(i=0; i<kAxisGrCnt; ++i)
  {
    cmGrPgAxis_t* ap = vp->axis + i;

    if( ap->title != NULL )
    {
      cmGrPSz_t sz;
      cmGrDcFontSetAndMeasure( dcH, ap->titleFontId, ap->titleFontSize, ap->titleFontStyle, ap->title, &sz );

      if( i==kBottomGrIdx || i==kTopGrIdx )
      {   
        int x = vp->iext.loc.x + (vp->iext.sz.w/2) - (sz.w/2);
        cmGrDcDrawText( dcH, ap->title, x, ap->titlePt.y );
      }    

      if( i==kLeftGrIdx || i==kRightGrIdx )
      {
        int y = vp->iext.loc.y + (vp->iext.sz.h/2) + (sz.w/2);
        cmGrDcDrawTextRot( dcH, ap->title, ap->titlePt.x, y, 90 );      
      }
    }
  }
}

void cmGrPageViewFocus( cmGrPgH_t h, unsigned vwIdx, bool enableFl )
{
  cmGrPg_t* p = _cmGrPgHandleToPtr(h);
  assert( vwIdx < p->vn );
  if( enableFl )
    p->focusIdx = vwIdx;
  else
  {
    if( p->focusIdx == vwIdx )
      p->focusIdx = cmInvalidId;
  }
}

cmGrVwH_t cmGrPageFocusedView( cmGrPgH_t h )
{
  cmGrPg_t* p   = _cmGrPgHandleToPtr(h);
  cmGrVwH_t vwH = cmGrVwNullHandle;

  if( p->focusIdx != cmInvalidIdx )
    vwH.h = p->viewV + p->focusIdx;

  return vwH;
}


void     cmGrPageLayout( cmGrPgH_t h, cmGrDcH_t dcH )
{
  cmGrPg_t* p = _cmGrPgHandleToPtr(h);
  cmGrDcPushCtx(dcH);
  _cmGrPageLayout(p,dcH);
  cmGrDcPopCtx(dcH);
}

void     cmGrPageDraw( cmGrPgH_t h, cmGrDcH_t dcH )
{
  unsigned  i;
  cmGrPg_t* p       = _cmGrPgHandleToPtr(h);

  cmGrDcPushCtx(dcH);

  _cmGrPageLayout(p,dcH);

  cmGrDcSetColor(dcH,kBlackGrId);

  // for each view
  for(i=0; i<p->vn; ++i)
  {
    cmGrPgVw_t* vp = p->viewV + i;
      
    unsigned lineColor = p->focusIdx==i ? kRedGrId : kBlackGrId;
    _cmGrPgDrawHashMarks(  p, vp, dcH, lineColor );
    _cmGrPgDrawHashLabels( p, vp, dcH, kBlackGrId);
    _cmGrPgDrawAxisTitles( p, vp, dcH );

    if( vp->title != NULL )
    {
      cmGrPSz_t sz;
      cmGrDcFontSetAndMeasure(dcH,vp->fontId,vp->fontSize,vp->fontStyle,vp->title,&sz);
      cmGrDcDrawText(dcH,vp->title, vp->titlePt.x - sz.w/2, vp->titlePt.y );
    }
          
  }

  cmGrDcPopCtx(dcH);

  
}

cmGrPgLabelFunc_t* _cmGrPageLabelIndexToRecd( cmGrPg_t* p, unsigned idx )
{
  cmGrPgLabelFunc_t* lfp = p->funcs;
  unsigned i;
  for(i=0; i<idx && lfp!=NULL; ++i)
    lfp=lfp->link;

  return lfp;
}

cmGrPgLabelFunc_t* _cmGrPageLabelIdToRecd( cmGrPg_t* p, unsigned id )
{
  cmGrPgLabelFunc_t* lfp = p->funcs;
  for(; lfp!=NULL; lfp=lfp->link)
    if( lfp->id == id )
      return lfp;

  return lfp;
}

unsigned            cmGrPageLabelFuncRegister( cmGrPgH_t h, cmGrLabelFunc_t func, void* arg, const cmChar_t* label )
{
  cmGrPg_t*          p   = _cmGrPgHandleToPtr(h); 
  cmGrPgLabelFunc_t* lfp = cmMemAllocZ(cmGrPgLabelFunc_t,1);

  lfp->id    = p->labelFuncId++;
  lfp->func  = func;
  lfp->arg   = arg; 
  lfp->label = cmMemAllocStr(label);
  lfp->link  = p->funcs;
  p->funcs   = lfp;
  return lfp->id;
}

unsigned cmGrPageLabelFuncCount( cmGrPgH_t h )
{
  cmGrPg_t*          p   = _cmGrPgHandleToPtr(h); 
  cmGrPgLabelFunc_t* lfp = p->funcs;
  unsigned           n   = 0;

  for(; lfp != NULL; lfp=lfp->link )
    ++n;
  return n;
}

unsigned         cmGrPageLabelFuncIndexToId( cmGrPgH_t h, unsigned index )
{
  cmGrPgLabelFunc_t* lfp;
  cmGrPg_t*          p   = _cmGrPgHandleToPtr(h); 

  if((lfp = _cmGrPageLabelIndexToRecd(p,index)) == NULL )
    return cmInvalidId;
  return lfp->id;
}

unsigned  cmGrPageLabelFuncLabelToId( cmGrPgH_t h, const cmChar_t* label )
{
  cmGrPg_t*          p   = _cmGrPgHandleToPtr(h); 
  cmGrPgLabelFunc_t* lfp = p->funcs;
  unsigned i;
  for(i=0; lfp!=NULL; lfp=lfp->link,++i)
    if( lfp->label!=NULL && strcmp(label,lfp->label)==0 )
      return lfp->id;

  return cmInvalidId;
}


cmGrLabelFunc_t  cmGrPageLabelFunc(         cmGrPgH_t h, unsigned id )
{
  cmGrPg_t*           p   = _cmGrPgHandleToPtr(h); 
  cmGrPgLabelFunc_t*  lfp = _cmGrPageLabelIdToRecd(p,id);
  assert( lfp != NULL );
  return lfp->func;
}

const cmChar_t* cmGrPageLabelFuncLabel(    cmGrPgH_t h, unsigned id )
{
  cmGrPg_t*           p   = _cmGrPgHandleToPtr(h); 
  cmGrPgLabelFunc_t*  lfp = _cmGrPageLabelIdToRecd(p,id);
  assert( lfp != NULL );
  return lfp->label;
}

void*           cmGrPageLabelFuncArg(      cmGrPgH_t h, unsigned id )
{
  cmGrPg_t*           p   = _cmGrPgHandleToPtr(h); 
  cmGrPgLabelFunc_t*  lfp = _cmGrPageLabelIdToRecd(p,id);
  assert( lfp != NULL );
  return lfp->arg;
}


cmGrVwH_t     cmGrPageViewHandle(        cmGrPgH_t h,  unsigned vwIdx )
{
  cmGrPg_t* p  = _cmGrPgHandleToPtr(h);
  cmGrVwH_t vwH;
  assert( vwIdx < p->vn );
  vwH.h = p->viewV + vwIdx;
  return vwH;
}

cmGrVwH_t cmGrPageGrHandleToView(  cmGrPgH_t pgH, cmGrH_t grH )
{
  cmGrPg_t* p   = _cmGrPgHandleToPtr(pgH);
  cmGrVwH_t vwH = cmGrVwNullHandle;
  int i;
  for(i=0; i<p->vn; ++i)
    if( cmHandlesAreEqual(grH,p->viewV[i].grH) )
    {
      vwH.h = p->viewV + i;
      break;
    }    
  return vwH;
}


bool      cmGrViewIsValid( cmGrVwH_t h )
{ return h.h != NULL; }

cmGrRC_t cmGrViewInit( cmGrVwH_t h, cmGrH_t grH, const cmChar_t* vwTitle, const cmChar_t* xLabel, const cmChar_t* yLabel )
{
  cmGrPgVw_t* vp = _cmGrPgVwHandleToPtr(h);

  vp->grH = grH;
  
  _cmGrViewSetTitle( vp, vwTitle );
  _cmGrAxisSetTitle( vp->axis + kBottomGrIdx, xLabel );
  _cmGrAxisSetTitle( vp->axis + kLeftGrIdx,   yLabel ); 

  vp->p->flags = cmSetFlag(vp->p->flags, kDirtyFl);
  
  return kOkGrRC;
}

cmGrRC_t cmGrViewClear( cmGrVwH_t h )
{
  cmGrPgVw_t* vp  = _cmGrPgVwHandleToPtr(h);
  cmGrH_t     grH = cmGrViewGrHandle(h);
  assert( cmGrIsValid(grH) );
  cmGrRC_t    rc  = cmGrClear( grH  );

  vp->p->flags = cmSetFlag(vp->p->flags, kDirtyFl);
  
  return rc;
}

cmGrRC_t cmGrViewPExt( cmGrVwH_t h, cmGrPExt_t* pext )
{
  cmGrPgVw_t* vp = _cmGrPgVwHandleToPtr(h);
  *pext = vp->iext;
  return kOkGrRC;
}

bool      cmGrViewHasFocus( cmGrVwH_t h )
{
  if( cmGrViewIsValid(h) == false )
    return false;

  cmGrPgVw_t* vp = _cmGrPgVwHandleToPtr(h);

  if( vp->p->focusIdx == cmInvalidIdx )
    return false;

  return cmGrId(vp->grH) == vp->p->focusIdx;
}

cmGrH_t  cmGrViewGrHandle( cmGrVwH_t h )
{
  cmGrPgVw_t* vp = _cmGrPgVwHandleToPtr(h);
  return vp->grH;
}

void            cmGrViewSetCfg(        cmGrVwH_t h, unsigned cfgFlags )
{
  cmGrPgVw_t* vp = _cmGrPgVwHandleToPtr(h);
  cmGrSetCfgFlags( vp->grH, cfgFlags ); 
  vp->p->flags = cmSetFlag(vp->p->flags, kDirtyFl);
}

unsigned        cmGrViewCfg(           cmGrVwH_t h )
{
  cmGrPgVw_t* vp = _cmGrPgVwHandleToPtr(h);
  return cmGrCfgFlags( vp->grH );
}
void            cmGrViewSetTitle(      cmGrVwH_t h, const cmChar_t* title )
{
  cmGrPgVw_t* vp = _cmGrPgVwHandleToPtr(h);
  _cmGrViewSetTitle( vp, title );
}
const cmChar_t* cmGrViewTitle(         cmGrVwH_t h )
{
  cmGrPgVw_t* vp = _cmGrPgVwHandleToPtr(h);
  return vp->title;
}
void            cmGrViewSetFontFamily( cmGrVwH_t h, unsigned id )
{
  cmGrPgVw_t* vp = _cmGrPgVwHandleToPtr(h);
  if( vp->fontId != id )
  {
    vp->fontId = id;
    vp->p->flags = cmSetFlag(vp->p->flags, kDirtyFl);
  }

}
unsigned        cmGrViewFontFamily(    cmGrVwH_t h )
{
  cmGrPgVw_t* vp = _cmGrPgVwHandleToPtr(h);
  return vp->fontId;
}
void            cmGrViewSetFontStyle(  cmGrVwH_t h, unsigned flags )
{
  cmGrPgVw_t* vp = _cmGrPgVwHandleToPtr(h);
  if( vp->fontStyle != flags )
  {
    vp->fontStyle = flags;
    vp->p->flags = cmSetFlag(vp->p->flags, kDirtyFl);
  }
}
unsigned        cmGrViewFontStyle(     cmGrVwH_t h )
{
  cmGrPgVw_t* vp = _cmGrPgVwHandleToPtr(h);
  return vp->fontStyle;
}
void            cmGrViewSetFontSize(   cmGrVwH_t h, unsigned size )
{
  cmGrPgVw_t* vp = _cmGrPgVwHandleToPtr(h);
  if( vp->fontSize != size )
  {
    vp->fontSize = size;
    vp->p->flags = cmSetFlag(vp->p->flags, kDirtyFl);
  }
}
unsigned        cmGrViewFontSize(      cmGrVwH_t h )
{
  cmGrPgVw_t* vp = _cmGrPgVwHandleToPtr(h);
  return vp->fontSize;
}

void cmGrViewSetLabelFunc(  cmGrVwH_t h, cmGrAxisIdx_t axisId, unsigned pgLabelFuncId )
{
  cmGrPgVw_t*          vp   = _cmGrPgVwHandleToPtr(h);
  cmGrPgLabelFunc_t** lfpp = NULL;

  switch( axisId )
  {
    case kLeftGrIdx:
    case kRightGrIdx:
      lfpp = &vp->yfunc;
      break;

    case kTopGrIdx:
    case kBottomGrIdx:
      lfpp = &vp->xfunc;
      break;

    default:
      { assert(0); }
  }

  assert( lfpp != NULL );

  *lfpp = _cmGrPageLabelIdToRecd(vp->p, pgLabelFuncId );
}
  
const cmChar_t* cmGrViewValue( cmGrVwH_t h, cmGrViewValueId_t id, cmChar_t* buf, unsigned bufCharCnt )
{
  cmGrPgVw_t*        vp = _cmGrPgVwHandleToPtr(h);
  cmGrPgLabelFunc_t* lfp = NULL;
  cmGrH_t            grH = vp->grH;
  cmGrV_t v;
  cmGrVPt_t pt0,pt1;

  switch( id )
  {
    case kLocalX_VwId:  
      v   = cmGrLocalPt(grH)->x; 
      lfp = vp->xfunc; 
      break;

    case kLocalY_VwId:  
      v   = cmGrLocalPt(grH)->y; 
      lfp = vp->yfunc; 
      break;

    case kGlobalX_VwId: 
      v   = cmGrGlobalPt(grH)->x; 
      lfp = vp->xfunc; 
      break;

    case kGlobalY_VwId: 
      v   = cmGrGlobalPt(grH)->y;
      lfp = vp->yfunc; 
      break;

    case kSelX0_VwId:   
      cmGrSelectPoints(grH,&pt0,NULL);
      v   = pt0.x;
      lfp = vp->xfunc; 
      break;

    case kSelY0_VwId:   
      cmGrSelectPoints(grH,&pt0,NULL);
      v   = pt0.y;
      lfp = vp->yfunc; 
      break;

    case kSelX1_VwId: 
      cmGrSelectPoints(grH,NULL,&pt1);
      v   = pt1.x;
      lfp = vp->xfunc; 
      break;

    case kSelY1_VwId:   
      cmGrSelectPoints(grH,NULL,&pt1);
      v   = pt1.y;
      lfp = vp->yfunc; 
      break;

    case kSelW_VwId:    
      cmGrSelectPoints(grH,&pt0,&pt1);
      v   = fabs(pt1.x - pt0.x);
      lfp = vp->xfunc; 
      break;

    case kSelH_VwId:    
      cmGrSelectPoints(grH,&pt0,&pt1);
      v   = fabs(pt1.y - pt0.y);
      lfp = vp->yfunc; 
      break;

    default:
      { assert(0); }
  }

  if( bufCharCnt > 0 )
  {
    buf[0]=0;
    if( lfp != NULL )
      lfp->func( lfp->arg, buf, bufCharCnt, v );
    else
      snprintf(buf,bufCharCnt,"%f",v);
  }

  return buf;
}


cmGrAxH_t     cmGrViewAxisHandle(    cmGrVwH_t h,  cmGrAxisIdx_t axisIdx  )
{
  cmGrPgVw_t* vp = _cmGrPgVwHandleToPtr(h);
  cmGrAxH_t   axH;
  assert( axisIdx < kAxisGrCnt );
  axH.h = vp->axis + axisIdx;
  return axH;
}

bool            cmGrAxisIsValid( cmGrAxH_t h )
{ return h.h != NULL; }

void            cmGrAxisSetCfg(        cmGrAxH_t h, unsigned flags )
{
  cmGrPgAxis_t* ap = _cmGrPgAxisHandleToPtr(h); 
  if( ap->flags != flags )
  {
    ap->flags = flags;
    ap->vp->p->flags = cmSetFlag(ap->vp->p->flags, kDirtyFl);
  }
}

unsigned        cmGrAxisCfg(           cmGrAxH_t h )
{
  cmGrPgAxis_t* ap = _cmGrPgAxisHandleToPtr(h); 
  return ap->flags;
}

void            cmGrAxisSetTitle(  cmGrAxH_t h, const cmChar_t* title )
{
  cmGrPgAxis_t* ap = _cmGrPgAxisHandleToPtr(h); 
  _cmGrAxisSetTitle(ap,title);
}

const cmChar_t* cmGrAxisTitle(         cmGrAxH_t h  )
{
  cmGrPgAxis_t* ap = _cmGrPgAxisHandleToPtr(h); 
  return ap->title;
}

void            cmGrAxisSetTitleFontFamily( cmGrAxH_t h, unsigned id )
{
  cmGrPgAxis_t* ap = _cmGrPgAxisHandleToPtr(h); 
  if( ap->titleFontId != id )
  {
    ap->titleFontId = id;
    ap->vp->p->flags = cmSetFlag(ap->vp->p->flags, kDirtyFl);      
  }
}

unsigned        cmGrAxisTitleFontFamily(    cmGrAxH_t h )
{
  cmGrPgAxis_t* ap = _cmGrPgAxisHandleToPtr(h); 
  return ap->titleFontId;
}

void            cmGrAxisTitleSetFontStyle(  cmGrAxH_t h, unsigned flags )
{
  cmGrPgAxis_t* ap = _cmGrPgAxisHandleToPtr(h); 
  if( ap->titleFontStyle != flags )
  {
    ap->titleFontStyle = flags;
    ap->vp->p->flags = cmSetFlag(ap->vp->p->flags, kDirtyFl);      
  }
}

unsigned        cmGrAxisTitleFontStyle(     cmGrAxH_t h )
{
  cmGrPgAxis_t* ap = _cmGrPgAxisHandleToPtr(h); 
  return ap->titleFontStyle;
}

void            cmGrAxisTitleSetFontSize(   cmGrAxH_t h, unsigned size )
{
  cmGrPgAxis_t* ap = _cmGrPgAxisHandleToPtr(h); 
  if( ap->titleFontSize != size )
  {
    ap->titleFontSize = size;
    ap->vp->p->flags = cmSetFlag(ap->vp->p->flags, kDirtyFl);      
  }
}

unsigned        cmGrAxisTitleFontSize(      cmGrAxH_t h )
{
  cmGrPgAxis_t* ap = _cmGrPgAxisHandleToPtr(h); 
  return ap->titleFontSize;
}

void            cmGrAxisSetLabelFunc(      cmGrAxH_t h, unsigned id )
{
  cmGrPgAxis_t* ap = _cmGrPgAxisHandleToPtr(h); 
  
  ap->func = _cmGrPageLabelIdToRecd( ap->vp->p, id );
}
