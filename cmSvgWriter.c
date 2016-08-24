#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmTime.h"
#include "cmText.h"
#include "cmFile.h"
#include "cmSvgWriter.h"

enum
{
  kRectSvgId,
  kLineSvgId,
  kTextSvgId
};

typedef struct cmSvgEle_str
{
  unsigned  id;
  double    x0;
  double    y0;
  double    x1;
  double    y1;
  cmChar_t* text;
  cmChar_t* cssClass;
  struct cmSvgEle_str* link;
} cmSvgEle_t;

typedef struct cmSvg_str
{
  cmErr_t     err;
  cmLHeapH_t  lhH;
  cmSvgEle_t* elist;
  cmSvgEle_t* eol; 
} cmSvg_t;

cmSvgH_t cmSvgNullHandle = cmSTATIC_NULL_HANDLE;

cmSvg_t* _cmSvgHandleToPtr( cmSvgH_t h )
{
  cmSvg_t* p = (cmSvg_t*)h.h;
  assert(p != NULL );
  return p;
}

cmSvgRC_t _cmSvgInsertEle( cmSvg_t* p, unsigned id, double x0, double y0, double x1, double y1, const cmChar_t* text, const cmChar_t* class )
{
  cmSvgEle_t* e = cmLhAllocZ(p->lhH,cmSvgEle_t,1);

  e->id       = id;
  e->x0       = x0;
  e->y0       = y0;
  e->x1       = x1;
  e->y1       = y1;
  e->text     = text==NULL ? "" : cmLhAllocStr(p->lhH,text);
  e->cssClass = cmLhAllocStr(p->lhH,class);
  
  if( p->eol == NULL )
    p->elist = p->eol = e;
  else
    p->eol->link = e;

  p->eol = e;

  return kOkSvgRC;
}

cmSvgRC_t _cmSvgWriterFree( cmSvg_t* p )
{
  cmLHeapDestroy(&p->lhH);
  cmMemFree(p);
  return kOkSvgRC;
}

cmSvgRC_t cmSvgWriterAlloc( cmCtx_t* ctx, cmSvgH_t* hp )
{
  cmSvgRC_t rc;
  if((rc = cmSvgWriterFree(hp)) != kOkSvgRC )
    return rc;

  cmSvg_t* p = cmMemAllocZ(cmSvg_t,1);

  cmErrSetup(&p->err,&ctx->rpt,"SVG Writer");

  // create a local linked heap
  if( cmLHeapIsValid( p->lhH = cmLHeapCreate(8196,ctx)) == false )
  {
    rc = cmErrMsg(&p->err,kLHeapFailSvgRC,"Lheap create failed.");
    goto errLabel;
  }
  
  hp->h = p;

 errLabel:
  if( rc != kOkSvgRC )
    _cmSvgWriterFree(p);

  return rc;
}

cmSvgRC_t cmSvgWriterFree( cmSvgH_t* hp )
{
  cmSvgRC_t rc = kOkSvgRC;
  
  if( hp==NULL || cmSvgWriterIsValid(*hp)==false )
    return kOkSvgRC;

  cmSvg_t* p = _cmSvgHandleToPtr(*hp);
  
  if((rc = _cmSvgWriterFree(p)) != kOkSvgRC )
    return rc;

  hp->h = NULL;
  
  return rc;
}

bool      cmSvgWriterIsValid( cmSvgH_t h )
{ return h.h != NULL; }

cmSvgRC_t cmSvgWriterRect( cmSvgH_t h, double  x, double y,  double ww,  double hh,  const cmChar_t* cssClassLabel )
{
  cmSvg_t* p = _cmSvgHandleToPtr(h);
  return  _cmSvgInsertEle( p, kRectSvgId, x, y, x+ww, y+hh, NULL, cssClassLabel==NULL?"rectclass":cssClassLabel );
}

cmSvgRC_t cmSvgWriterLine( cmSvgH_t h, double x0, double y0, double x1, double y1, const cmChar_t* cssClassLabel )
{
  cmSvg_t* p = _cmSvgHandleToPtr(h);
  return  _cmSvgInsertEle( p, kLineSvgId, x0, y0, x1, y1, NULL, cssClassLabel==NULL?"lineclass":cssClassLabel );
}

cmSvgRC_t cmSvgWriterText( cmSvgH_t h, double  x, double y,  const cmChar_t* text,     const cmChar_t* cssClassLabel )
{
  cmSvg_t* p = _cmSvgHandleToPtr(h);
  return  _cmSvgInsertEle( p, kTextSvgId, x, y, 0, 0, text==NULL?"":text, cssClassLabel==NULL?"textclass":cssClassLabel );
}

void _cmSvgSize( cmSvg_t* p, double* widthRef, double* heightRef )
{
  *widthRef  = 0;
  *heightRef = 0;

  if( p->elist == NULL )
    return; 
  
  cmSvgEle_t* e     = p->elist;
  double      min_x = cmMin(e->x0,e->x1);
  double      max_x = cmMax(e->x0,e->x1);
  double      min_y = cmMin(e->y0,e->y1);
  double      max_y = cmMax(e->y0,e->y1);

  for(e=e->link; e!=NULL; e=e->link)
  {
    min_x = cmMin(cmMin(min_x,e->x0),e->x1);
    max_x = cmMax(cmMax(max_x,e->x0),e->x1);
    min_y = cmMin(cmMin(min_y,e->y0),e->y1);
    max_y = cmMax(cmMax(max_y,e->y0),e->y1);
  }

  *widthRef  = max_x - min_x;
  *heightRef = max_y - min_y;
}

void _cmSvgWriterFlipY( cmSvg_t* p, unsigned height )
{
  cmSvgEle_t* e = p->elist;
  for(; e!=NULL; e=e->link)
  {
    e->y0 = (-e->y0) + height;
    e->y1 = (-e->y1) + height;

    if( e->id == kRectSvgId )
    {
      double t = e->y1;
      e->y1 = e->y0;
      e->y0 = t;
    }
    
  }
}



cmSvgRC_t cmSvgWriterWrite( cmSvgH_t h,  const cmChar_t* cssFn, const cmChar_t* outFn )
{
  cmSvgRC_t   rc        = kOkSvgRC;
  cmSvg_t*    p         = _cmSvgHandleToPtr(h);
  double      svgWidth  = 0;
  double      svgHeight = 0;
  cmSvgEle_t* e         = p->elist;
  cmFileH_t   fH        = cmFileNullHandle;
  cmChar_t*   s0        = NULL;
  cmChar_t*   s1        = NULL;

  cmChar_t hdr[] =
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "<meta charset=\"utf-8\">"    
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
    "<script type=\"text/javascript\" src=\"svg-pan-zoom.min.js\"></script>"
    "<script>"
    " var panZoom = null;"
    "  function doOnLoad() { panZoom = svgPanZoom(document.querySelector('#mysvg'), { controlIconsEnabled:true } ) }"
    "</script>"
    "</head>"
    "<body onload=\"doOnLoad()\">"
    "<svg id=\"mysvg\" width=\"%f\" height=\"%f\">";

 
  
  _cmSvgSize(p, &svgWidth, &svgHeight );

  _cmSvgWriterFlipY( p, svgHeight );

  // print the file header
  if( (s0 = cmTsPrintfP(s0,hdr,cssFn,svgWidth,svgHeight)) == NULL )
  {
    rc = cmErrMsg(&p->err,kPrintFailSvgRC,"File prefix write failed.");
    goto errLabel;
  }

  for(; e!=NULL; e=e->link)
  {
    switch( e->id )
    {
      case kRectSvgId:
        if( (s1 = cmTsPrintfP(s1,"<rect x=\"%f\" y=\"%f\" width=\"%f\" height=\"%f\" class=\"%s\"/>\n",e->x0,e->y0,e->x1-e->x0,e->y1-e->y0,e->cssClass)) == NULL )
          rc = kPrintFailSvgRC;
        break;
        
      case kLineSvgId:
        if( (s1 = cmTsPrintfP(s1,"<line x1=\"%f\" y1=\"%f\" x2=\"%f\" y2=\"%f\" class=\"%s\"/>\n",e->x0,e->y0,e->x1,e->y1,e->cssClass)) == NULL )
          rc = kPrintFailSvgRC;
        break;
        
      case kTextSvgId:
        if( (s1 = cmTsPrintfP(s1,"<text x=\"%f\" y=\"%f\" class=\"%s\">%s</text>\n",e->x0,e->y0,e->cssClass,e->text)) == NULL )
          rc = kPrintFailSvgRC;        
        break;
        
      default:
        { assert(0); }
      
    }

    if( rc != kOkSvgRC )
    {
      rc = cmErrMsg(&p->err,kPrintFailSvgRC,"Element write failed.");
      break;
    }

    s0 = cmTextAppendSS(s0,s1);
    
  }
  
  if( (s1 = cmTsPrintfP(s1,"</svg>\n</body>\n</html>\n")) == NULL )
  {
    rc = cmErrMsg(&p->err,kPrintFailSvgRC,"File suffix write failed.");
    goto errLabel;
  }

  if( cmFileOpen(&fH,outFn,kWriteFileFl,p->err.rpt) != kOkFileRC )
  {
    rc = cmErrMsg(&p->err,kFileFailSvgRC,"SVG file create failed for '%s'.",cmStringNullGuard(outFn));
    goto errLabel;
  }
  
  if( cmFilePrint(fH,s0 = cmTextAppendSS(s0,s1)) != kOkFileRC )
  {    
    rc = cmErrMsg(&p->err,kFileFailSvgRC,"File write failed.");
    goto errLabel;
  }

 errLabel:
  cmFileClose(&fH);

  cmMemFree(s0);
  cmMemFree(s1);
  
  return rc;
}

