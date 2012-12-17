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

#include "cmAudioFile.h"
#include "cmAudioFileMgr.h"
#include "cmGrPlotAudio.h"

typedef struct
{
  cmGrPlObjH_t       oH;
  cmAfmFileH_t       afH;
  unsigned           chIdx;

  cmGrRenderObjCb_t  renderCbFunc;
  void*              renderCbArg;

  cmGrDestroyObjCb_t destroyCbFunc;  
  void*              destroyCbArg;

  cmGrIsInsideObjCb_t isInsideCbFunc;
  void*               isInsideCbArg;

  void*       mem;   //
  cmGrPExt_t  pext;  // extents of the visible portion of this audio object
  unsigned    pixN;  // count of pixel columns used by this audio object
  cmSample_t* fMinV; // fMinV[pixN] = min sample value for each visible column
  cmSample_t* fMaxV; // fMaxV[pixN] = max sample value for each visible column
  int*        iMinV; // iMinV[pixN] = top pixel for each column line 
  int*        iMaxV; // iMaxV[pixN] = bottom pixel for each column line

} cmGrPlObjAf_t;

cmGrPlRC_t _cmGrPlObjAfCalcImage( cmGrPlObjAf_t* op, cmGrH_t grH )
{
  cmGrPlRC_t rc     = kOkGrPlRC;
  cmGrObjH_t grObjH = cmGrPlotObjHandle(op->oH);
  cmGrVExt_t vwExt,objExt,drExt;

  // get the intersection of the view and this audio object
  cmGrViewExtents( grH, &vwExt );
  cmGrPlotObjVExt( op->oH, &objExt );
  cmGrVExtIntersect(&drExt,&vwExt,&objExt);

  // if the audio object is visible
  if( cmGrVExtIsNullOrEmpty(&drExt) )
  {
    cmGrPExtSetNull(&op->pext);
    op->pixN = 0;
  }
  else
  {
    // get the extents of the visible portion of the audio object
    cmGrVExt_VtoP( grH, cmGrPlotObjHandle(op->oH), &drExt, &op->pext);

    // store the count of horizontal pixels
    op->pixN    = op->pext.sz.w;

    // allocate a cache to hold the image data
    unsigned byteCnt = op->pixN * 2 * sizeof(int) + op->pixN * 2 * sizeof(cmSample_t);
    op->mem   = cmMemResize(char,op->mem,byteCnt);
    op->fMinV = (cmSample_t*)op->mem;
    op->fMaxV = op->fMinV + op->pixN;
    op->iMinV = (int*)(op->fMaxV + op->pixN);
    op->iMaxV = op->iMinV + op->pixN;
    assert( op->iMaxV + op->pixN == op->mem + byteCnt );

    // locate the offset into the file of the first sample to be displayed
    unsigned si = 0;
    if( drExt.loc.x > objExt.loc.x )
      si = drExt.loc.x - objExt.loc.x;

   
    // get the floating point audio summary signal
    if( cmAfmFileGetSummary( op->afH, op->chIdx, si, drExt.sz.w, op->fMinV, op->fMaxV, op->pixN ) != kOkAfmRC )
    {
      const cmChar_t* afn = cmAudioFileName( cmAfmFileHandle(op->afH));
      rc = cmErrMsg( cmGrPlotErr( cmGrPlotObjMgrHandle(op->oH) ), kRsrcFailGrPlRC, "Audio file summary read failure on '%s'.",afn);
      goto errLabel;
    }

    unsigned i;
    // convert the summary to pixels values
    for(i=0; i<op->pixN; ++i)
    {
      // Note the reversal of min and max during the conversion.
      op->iMaxV[i] = cmGrY_VtoP( grH, grObjH, op->fMinV[i] );
      op->iMinV[i] = cmGrY_VtoP( grH, grObjH, op->fMaxV[i] );
    }
  }
 errLabel:
  return rc;
}


bool _cmGrPlObjAfRender(   cmGrObjFuncArgs_t* args, cmGrDcH_t dcH )
{
  cmGrPlObjAf_t* op = (cmGrPlObjAf_t*)args->cbArg;

  if( _cmGrPlObjAfCalcImage(op, args->grH ) == kOkGrPlRC )
  {
    int i;
    cmGrPExt_t pext;
    cmGrPhysExtents( args->grH, &pext);

    cmGrDcSetColor(dcH, cmGrPlotObjCurLineColor(op->oH));

    // draw a horz line at y=0 
    int y0 = cmGrY_VtoP( args->grH, cmGrPlotObjHandle(op->oH), 0.0 );
    cmGrDcDrawLine(dcH, cmGrPExtL(&op->pext), y0, cmGrPExtR(&op->pext) , y0 );

    // draw a vertical line for each 
    for(i=0; i<op->pixN; ++i)
      cmGrDcDrawLine(dcH, op->pext.loc.x+i, op->iMinV[i], op->pext.loc.x+i, op->iMaxV[i] );        


    // draw a rectangle around the entire audio clip
    cmGrDcDrawRect(dcH, op->pext.loc.x, cmGrPExtT(&pext),  op->pext.sz.w, cmGrPExtB(&pext) );
    
    // draw the file label 
    cmGrDcDrawTextJustify( dcH, cmGrPlotObjFontFamily(op->oH), cmGrPlotObjFontSize(op->oH), cmGrPlotObjFontStyle(op->oH), cmGrPlotObjLabel(op->oH), &op->pext, kHorzCtrJsGrFl | kTopJsGrFl );

  }
  return true;
}

bool  _cmGrPlObjAfIsInside( cmGrObjFuncArgs_t* args, int px, int py, cmGrV_t vx, cmGrV_t vy )
{
  cmGrPlObjAf_t* op = (cmGrPlObjAf_t*)args->cbArg;

  if( cmGrPExtIsXyInside( &op->pext, px, py ) )
  {
    px -= op->pext.loc.x;
    if( 0 <= px && px < op->pixN )
      return op->iMinV[px] <= py && py <= op->iMaxV[px]; 
      
  }

  return false;
}

void _cmGrPlObjAfDestroy( cmGrObjFuncArgs_t* args )
{
  cmGrPlObjAf_t* op = (cmGrPlObjAf_t*)args->cbArg;
  args->cbArg = op->destroyCbArg;
  op->destroyCbFunc(args);  
  cmMemFree(op->mem);
  cmMemFree(op);
}

cmGrPlRC_t  cmGrPlotAudioFileObjCreate(
  cmGrPlObjH_t oH,
  cmAfmFileH_t afH,
  unsigned     audioChIdx )
{
  cmGrPlObjAf_t* op = cmMemAllocZ(cmGrPlObjAf_t,1);
  op->oH  = oH;
  op->afH = afH;
  op->chIdx = audioChIdx;

  cmGrObjH_t grObjH = cmGrPlotObjHandle(op->oH);

  op->renderCbFunc = cmGrObjRenderCbFunc(grObjH);
  op->renderCbArg  = cmGrObjRenderCbArg(grObjH);
  cmGrObjSetRenderCb( grObjH, _cmGrPlObjAfRender, op );

  op->destroyCbFunc = cmGrObjDestroyCbFunc(grObjH);
  op->destroyCbArg  = cmGrObjDestroyCbArg(grObjH);
  cmGrObjSetDestroyCb( grObjH, _cmGrPlObjAfDestroy, op );

  op->isInsideCbFunc = cmGrObjIsInsideCbFunc(grObjH);
  op->isInsideCbArg  = cmGrObjIsInsideCbArg(grObjH);
  cmGrObjSetIsInsideCb( grObjH, _cmGrPlObjAfIsInside, op );

  cmGrPlotObjSetUserPtr(oH,op);

  return kOkGrPlRC;
}
