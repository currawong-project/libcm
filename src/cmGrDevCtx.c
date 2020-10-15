//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmGr.h"
#include "cmGrDevCtx.h"

#define _cmGrDcOffsX(p,xx) (xx) // ((p)->pext.loc.x + (xx))
#define _cmGrDcOffsY(p,yy) (yy) // ((p)->pext.loc.y + (yy))

cmGrDcH_t cmGrDcNullHandle = cmSTATIC_NULL_HANDLE;

// cmGrDcRecd is used to store the state of the
// device context on the cmGrDC_t stack.
typedef struct cmGrDcRecd_str
{
  cmGrColor_t color;
  unsigned    fontId;
  unsigned    fontStyle;
  unsigned    fontSize;
  unsigned    penWidth;
  unsigned    penStyle;

  struct cmGrDcRecd_str* next;
  struct cmGrDcRecd_str* prev;
} cmGrDcRecd_t;

typedef struct cmGrDc_str
{
  cmErr_t       err;
  cmGrDev_t*    dd;    // device driver used by this context
  void*         ddArg; // user assigned device driver callback arg
  cmGrDcRecd_t* list;  // First recd on the stack (not the top).
  cmGrDcRecd_t* cur;   // Top recd on the stack.
  cmGrPExt_t    pext;  // x,y is offset added to all drawing coordinates
                       // w,h is size of drawing area
} cmGrDc_t;

// Note: recd's prior to p->cur are available.
// Recd's after p->cur are on the stack.

cmGrDc_t* _cmGrDcHandleToPtr( cmGrDcH_t h )
{
  cmGrDc_t* p = (cmGrDc_t*)h.h;
  assert( p != NULL );
  return p;
}

void _cmGrDcRecdPrint( const cmChar_t* label, const cmGrDcRecd_t* r )
{
  printf("%s r:%i g:%i b:%i fid:%i fs:0x%x fsz:%i pw:%i ps:0x%x\n",
    cmStringNullGuard(label),
    cmGrColorToR(r->color),cmGrColorToG(r->color),cmGrColorToB(r->color),
    r->fontId,r->fontStyle,r->fontSize,r->penWidth,r->penStyle);
}

// Make a duplicate of the current record (if it exists) 
// and insert it prior to the current record.
// make the new record current.
void _cmGrDcPush( cmGrDc_t* p )
{
  if( p->cur == NULL )
  {
    assert( p->list == NULL );
    cmGrDcRecd_t* r = cmMemAllocZ( cmGrDcRecd_t, 1);
    p->dd->get_color( p->ddArg, &r->color );
    r->fontId    = p->dd->get_font_family(p->ddArg );
    r->fontSize  = p->dd->get_font_size(  p->ddArg );
    r->fontStyle = p->dd->get_font_style( p->ddArg );
    r->penWidth  = p->dd->get_pen_width(  p->ddArg );
    r->penStyle  = p->dd->get_pen_style(  p->ddArg );
    p->list = r;
    p->cur  = r;
  }
  else
  {
    cmGrDcRecd_t* r = p->cur->prev;

    // if no prev recd exists ...
    if( r == NULL )
    {
      // .... then allocate one
      r = cmMemAllocZ( cmGrDcRecd_t, 1 );
      *r           = *p->cur;
      p->cur->prev = r;
      r->next      = p->cur;    
    }
    else
    {
      // ... otherwise use the prev one
      cmGrDcRecd_t* nrp = r->next;
      cmGrDcRecd_t* prp = r->prev;
      *r      = *p->cur;
      r->next = nrp;
      r->prev = prp;
    }

    // make the new recd the cur recd
    p->cur = r;
  
    // if the new recd is the first on the list
    // then update the list begin pointer
    if( p->cur->prev == NULL )
      p->list = p->cur;
  }

  //_cmGrDcRecdPrint("push:", p->cur );

}

cmGrDcRC_t  _cmGrDcPop(cmGrDc_t* p )
{
  if( p->cur==NULL || p->cur->next == NULL )
    return cmErrMsg(&p->err,kStackFaultGrDcRC,"Cannot pop the last context record off the stack.");

  p->cur = p->cur->next;

  p->dd->set_color(       p->ddArg, p->cur->color );
  p->dd->set_font_family( p->ddArg, p->cur->fontId );
  p->dd->set_font_size(   p->ddArg, p->cur->fontSize );
  p->dd->set_font_style(  p->ddArg, p->cur->fontStyle );
  p->dd->set_pen_width(   p->ddArg, p->cur->penWidth );
  p->dd->set_pen_style(   p->ddArg, p->cur->penStyle );

  //_cmGrDcRecdPrint("pop:", p->cur );

  return kOkGrDcRC;
}

cmGrDcRC_t _cmGrDcDestroy( cmGrDc_t* p )
{
  cmGrDcRecd_t* rp = p->list;
  while( rp!=NULL )
  {
    cmGrDcRecd_t* tp = rp->next;
    cmMemFree( rp );
    rp = tp;
  }

  p->dd->destroy(p->ddArg);


  cmMemFree(p);

  return kOkGrDcRC;
}

cmGrDcRC_t cmGrDevCtxCreate( cmCtx_t* ctx, cmGrDcH_t* hp, cmGrDev_t* dd, void* ddArg, int x, int y, int w, int h )
{
  cmGrDcRC_t rc;
  if((rc = cmGrDevCtxDestroy(hp)) != kOkGrDcRC )
    return rc;

  cmGrDc_t* p = cmMemAllocZ(cmGrDc_t,1);

  cmErrSetup(&p->err,&ctx->rpt,"cmGrDevCtx");

  p->dd    = dd;
  p->ddArg = ddArg;
 
  cmGrPExtSet(&p->pext,x,y,w,h);

  if( dd->create(ddArg,w,h) == false )
  {
    cmErrMsg(&p->err,kDevDrvFailGrDcRC,"Device driver create failed.");
    goto errLabel;
  }

  _cmGrDcPush(p); // create the default context

  hp->h = p;

  errLabel:
  if(rc != kOkGrDcRC )
    _cmGrDcDestroy(p);

  return rc;
}

cmGrDcRC_t cmGrDevCtxDestroy( cmGrDcH_t* hp )
{
  cmGrDcRC_t rc;

  if( hp==NULL || cmGrDevCtxIsValid(*hp)==false )
    return kOkGrDcRC;

  cmGrDc_t* p = _cmGrDcHandleToPtr(*hp);
  
  if((rc = _cmGrDcDestroy(p)) != kOkGrDcRC )
    return rc;

  hp->h = NULL;

  return rc;
}

bool       cmGrDevCtxIsValid( cmGrDcH_t h )
{ return h.h != NULL; }

cmGrDcRC_t      cmGrDevCtxResize(   cmGrDcH_t h, int x, int y, int  ww, int hh )
{
  cmGrDcRC_t rc = kOkGrDcRC;
  cmGrDc_t*  p  = _cmGrDcHandleToPtr(h);

  // store the current drawing context state
  _cmGrDcPush(p);
  
  if( p->dd->create(p->ddArg,ww,hh) == false )
  {
    cmErrMsg(&p->err,kDevDrvFailGrDcRC,"Device driver create failed on resize.");
    goto errLabel;
  }

  cmGrPExtSet(&p->pext,x,y,ww,hh);

 errLabel:
  // force the current state to be reapplied to the new drawing context
  _cmGrDcPop(p);

  return rc;
}

void cmGrDevCtxSize( cmGrDcH_t h, cmGrPExt_t* pext )
{
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);
  *pext = p->pext;
  pext->loc.x *= -1;
  pext->loc.y *= -1;
}

void            cmGrDevCtxBeginDraw( cmGrDcH_t h )
{
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);
  p->dd->begin_draw( p->ddArg );
}

void            cmGrDevCtxEndDraw(   cmGrDcH_t h )
{
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);
  p->dd->end_draw( p->ddArg );
}

void            cmGrDevCtxDraw( cmGrDcH_t h )
{
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);
  p->dd->draw( p->ddArg, -p->pext.loc.x, -p->pext.loc.y );
}


void            cmGrDcPushCtx( cmGrDcH_t h )
{
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);
  _cmGrDcPush(p);
}

void            cmGrDcPopCtx(  cmGrDcH_t h )
{ 
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);
  _cmGrDcPop(p);
}


unsigned        cmGrDcColor(        cmGrDcH_t h )
{ 
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);
  return p->cur->color;
}

void            cmGrDcSetColorRgb(  cmGrDcH_t h, unsigned char r, unsigned char g, unsigned char b )
{
  cmGrDcSetColor(h,cmGrRgbToColor(r,g,b));
}

void            cmGrDcSetColor(     cmGrDcH_t h, cmGrColor_t color )
{
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);
  p->dd->set_color( p->ddArg, color );
}

unsigned cmGrDcFontFamily(    cmGrDcH_t h )
{
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);
  return p->cur->fontId;
}

void            cmGrDcSetFontFamily( cmGrDcH_t h, unsigned fontId )
{
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);
  p->cur->fontId = fontId;
  p->dd->set_font_family( p->ddArg, fontId );
}

unsigned        cmGrDcFontStyle(      cmGrDcH_t h )
{
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);
  return p->cur->fontStyle;
}

void            cmGrDcSetFontStyle(   cmGrDcH_t h, unsigned style )
{
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);
  p->cur->fontStyle = style;
  p->dd->set_font_style( p->ddArg, style );
}

unsigned        cmGrDcFontSize(      cmGrDcH_t h )
{
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);
  return p->cur->fontSize;
}

void            cmGrDcSetFontSize(   cmGrDcH_t h, unsigned size )
{
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);
  p->cur->fontSize = size;
  p->dd->set_font_size( p->ddArg, size );
}

unsigned        cmGrDcPenWidth(      cmGrDcH_t h )
{
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);
  return p->cur->penWidth;
}

void            cmGrDcSetPenWidth(   cmGrDcH_t h, unsigned width )
{
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);
  p->cur->penWidth = width;
  p->dd->set_pen_width( p->ddArg, width );
}

unsigned        cmGrDcPenStyle(      cmGrDcH_t h )
{
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);
  return p->cur->penStyle;
}

void            cmGrDcSetPenStyle(   cmGrDcH_t h, unsigned style )
{
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);
  p->cur->penStyle = style;
  p->dd->set_pen_style( p->ddArg, style );
}

void            cmGrDcDrawLine(  cmGrDcH_t h, int x0, int y0, int x1, int y1 )
{ 
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);  
  p->dd->draw_line( p->ddArg, _cmGrDcOffsX(p,x0), _cmGrDcOffsY(p,y0), _cmGrDcOffsX(p,x1), _cmGrDcOffsY(p,y1) );
}

void            cmGrDcDrawRect(  cmGrDcH_t h, int x,  int y,  unsigned ww, unsigned hh )
{
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);  
  p->dd->draw_rect( p->ddArg, _cmGrDcOffsX(p,x), _cmGrDcOffsY(p,y), ww, hh );
}

void            cmGrDcDrawRectPExt(     cmGrDcH_t h, const cmGrPExt_t* pext )
{ cmGrDcDrawRect( h, cmGrPExtL(pext), cmGrPExtT(pext), cmGrPExtW(pext), cmGrPExtH(pext) ); }

void            cmGrDcFillRect(  cmGrDcH_t h, int x,  int y,  unsigned ww, unsigned hh )
{
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);  
  p->dd->fill_rect( p->ddArg, _cmGrDcOffsX(p,x), _cmGrDcOffsY(p,y), ww, hh );
}

void            cmGrDcDrawEllipse(  cmGrDcH_t h, int x,  int y,  unsigned ww, unsigned hh )
{
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);  
  p->dd->draw_ellipse( p->ddArg, _cmGrDcOffsX(p,x), _cmGrDcOffsY(p,y), ww, hh );
}

void            cmGrDcFillEllipse(  cmGrDcH_t h, int x,  int y,  unsigned ww, unsigned hh )
{
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);  
  p->dd->fill_ellipse( p->ddArg, _cmGrDcOffsX(p,x), _cmGrDcOffsY(p,y), ww, hh );
}

void            cmGrDcDrawDiamond(  cmGrDcH_t h, int x,  int y,  unsigned ww, unsigned hh )
{
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);  
  p->dd->draw_diamond( p->ddArg, _cmGrDcOffsX(p,x), _cmGrDcOffsY(p,y), ww, hh );
}

void            cmGrDcFillDiamond(  cmGrDcH_t h, int x,  int y,  unsigned ww, unsigned hh )
{
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);  
  p->dd->fill_diamond( p->ddArg, _cmGrDcOffsX(p,x), _cmGrDcOffsY(p,y), ww, hh );
}

void            cmGrDcDrawTriangle( cmGrDcH_t h, int x,  int y,  unsigned ww, unsigned hh, unsigned dirFlag )
{
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);  
  p->dd->draw_triangle( p->ddArg, _cmGrDcOffsX(p,x), _cmGrDcOffsY(p,y), ww, hh, dirFlag );
}

void            cmGrDcFillTriangle( cmGrDcH_t h, int x,  int y,  unsigned ww, unsigned hh, unsigned dirFlag )
{
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);  
  p->dd->fill_triangle( p->ddArg, _cmGrDcOffsX(p,x), _cmGrDcOffsY(p,y), ww, hh, dirFlag );
}



void            cmGrDcMeasure(     cmGrDcH_t h, const cmChar_t* text, cmGrPSz_t* sz )  
{
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);  
  if( text == NULL )
    cmGrPSzSet(sz,0,0);
  else
  {
    unsigned ww,hh;
    p->dd->measure_text( p->ddArg, text, &ww, &hh );
    sz->w = ww;
    sz->h = hh;
  }
}

void            cmGrDcDrawText(    cmGrDcH_t h, const cmChar_t* text, int x, int y )
{
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);  
  p->dd->draw_text( p->ddArg, text, _cmGrDcOffsX(p,x), _cmGrDcOffsY(p,y) );
}

void            cmGrDcDrawTextRot( cmGrDcH_t h, const cmChar_t* text, int x, int y, int angle )
{
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);  
  p->dd->draw_text_rot( p->ddArg, text, _cmGrDcOffsX(p,x), _cmGrDcOffsY(p,y), angle );
}


void            cmGrDcReadImage(   cmGrDcH_t h, unsigned char* a, const cmGrPExt_t* pext )
{
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);  
  p->dd->read_image( p->ddArg, a, _cmGrDcOffsX(p,pext->loc.x), _cmGrDcOffsY(p,pext->loc.y), pext->sz.w, pext->sz.h );
}

void            cmGrDcDrawImage(  cmGrDcH_t h, const unsigned char* a, const cmGrPExt_t* pext )
{
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);  
  p->dd->draw_image( p->ddArg, a, _cmGrDcOffsX(p,pext->loc.x), _cmGrDcOffsY(p,pext->loc.y), pext->sz.w, pext->sz.h );
}


void  cmGrDcSetFont( cmGrDcH_t h, unsigned fontId, unsigned size, unsigned style )
{
  cmGrDcSetFontFamily(h,fontId);
  cmGrDcSetFontSize(  h,size);
  cmGrDcSetFontStyle( h,style);
}

void  cmGrDcFontSetAndMeasure(cmGrDcH_t h, unsigned fontId, unsigned size, unsigned style, const cmChar_t* text, cmGrPSz_t* sz )
{
  cmGrDcSetFont(h,fontId,size,style);
  cmGrDcMeasure(h,text,sz);
}

void  cmGrDcDrawTextJustify(  cmGrDcH_t h, unsigned fontId, unsigned size, unsigned style, const cmChar_t* text, const cmGrPExt_t* pext, unsigned flags )
{
  int x = cmGrPExtCtrX(pext);
  int y = cmGrPExtCtrY(pext);

  if( cmIsFlag(flags,kNorthJsGrFl) )
    y = cmGrPExtT(pext);
  else
    if( cmIsFlag(flags,kSouthJsGrFl) )
      y = cmGrPExtB(pext);

  if( cmIsFlag(flags,kEastJsGrFl) )
    x = cmGrPExtR(pext);
  else
    if( cmIsFlag(flags,kWestJsGrFl) )
      x = cmGrPExtL(pext);

  cmGrDcDrawTextJustifyPt(h,fontId,size,style,text,flags,x,y);
}

void   cmGrDcDrawTextJustifyPt(  cmGrDcH_t h, unsigned fontId, unsigned size, unsigned style, const cmChar_t* text, unsigned flags, int xx, int yy )
{
  cmGrPSz_t sz;
  cmGrDcFontSetAndMeasure(h, fontId, size, style, text, &sz );
  
  int x,y;
  if( cmIsFlag(flags,kRightJsGrFl) )
    x = xx;
  else
    if( cmIsFlag(flags,kLeftJsGrFl) )
      x = xx - sz.w;
    else
      x = xx - sz.w/2;

  if( cmIsFlag(flags,kBottomJsGrFl) )
    y = yy;
  else
    if( cmIsFlag(flags,kTopJsGrFl) )
      y = yy + sz.h;
    else
      y = yy + sz.h/2;



  cmGrPExt_t r;
  cmGrPExtSet(&r,x,y,sz.w,sz.h);

  // Note: Checking for visibility should not be necessary however
  // there appears to be a problem with the FLTK text output whereby
  // text coordinates over 0xffff wrap around and may appear in the
  // visible region.
  if( cmGrDcRectIsVisible(h,&r) )
  {
    /*
    if( text!=NULL && (strncmp(text,"loc:138",7)==0 || strcmp(text,"loc:8 ")==0))
    {
      printf("%s %i %i %i %i\n",text,x,y,sz.w,xx);
      cmGrPExtPrint(text,&r);
      cmGrDc_t* p = _cmGrDcHandleToPtr(h);  
      cmGrPExt_t res;
      cmGrPExtIntersect(&res,&p->pext,&r );
      cmGrPExtPrint(text,&p->pext);
      cmGrPExtPrint("isect:",&res);
      printf("%i\n",cmGrPExtIsNotNullOrEmpty(&res));
    }
    */

    cmGrDcDrawText(h, text, x+.5, y+.5 );  
  }
  //cmGrPExt_t pext;
  //cmGrDcDrawTextJustifyRect(h, fontId, size, style, text, flags, xx, yy, &pext );
  //cmGrDcDrawRectPExt(h,&pext);
}

void   cmGrDcDrawTextJustifyRect( cmGrDcH_t h, unsigned fontId, unsigned size, unsigned style, const cmChar_t* text, unsigned flags, int xx, int yy, cmGrPExt_t* pext )
{
  cmGrPSz_t sz;
  cmGrDcFontSetAndMeasure(h, fontId, size, style, text, &sz );
  
  int x,y;
  if( cmIsFlag(flags,kRightJsGrFl) )
    x = xx;
  else
    if( cmIsFlag(flags,kLeftJsGrFl) )
      x = xx - sz.w;
    else
      x = xx - sz.w/2;

  if( cmIsFlag(flags,kBottomJsGrFl) )
    y = yy;
  else
    if( cmIsFlag(flags,kTopJsGrFl) )
      y = yy + sz.h;
    else
      y = yy + sz.h/2;

  cmGrPExtSet( pext, x, y-sz.h, sz.w+1, sz.h );
}

bool            cmGrDcPointIsVisible( cmGrDcH_t h, int x, int y )
{
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);  
  return cmGrVExtIsXyInside(&p->pext,x,y);
}

bool            cmGrDcRectIsVisible(  cmGrDcH_t h, const cmGrPExt_t* r )
{
  cmGrPExt_t res;
  cmGrDc_t* p = _cmGrDcHandleToPtr(h);  
  cmGrPExtIntersect(&res,&p->pext,r );
  return cmGrPExtIsNotNullOrEmpty(&res);
}
