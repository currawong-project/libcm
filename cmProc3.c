#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmSymTbl.h"
#include "cmFloatTypes.h"
#include "cmComplexTypes.h"
#include "cmFileSys.h"
#include "cmJson.h"
#include "cmProcObj.h"
#include "cmProcTemplate.h"
#include "cmAudioFile.h"
#include "cmMath.h"
#include "cmProc.h"
#include "cmVectOps.h"
#include "cmProc3.h"
#include "cmMidi.h"  // cmMidiToSciPitch();

cmPitchShift* cmPitchShiftAlloc( cmCtx* c, cmPitchShift* p, unsigned procSmpCnt, cmReal_t srate )
{
  cmPitchShift* op = cmObjAlloc(cmPitchShift, c, p );
  if( procSmpCnt != 0 )
    if( cmPitchShiftInit(op, procSmpCnt,srate) != cmOkRC )
      cmPitchShiftFree(&op);
  return op;
}

cmRC_t        cmPitchShiftFinal(cmPitchShift* p )
{ return cmOkRC; }

#define _cube_interp(x,y0,y1,y2,y3) ((y3) - (y2) - (y0) + (y1))*(x)*(x)*(x) + ((y0) - (y1) - ((y3) - (y2) - (y0) + (y1)))*(x)*(x) + ((y2) - (y0))*(x) + (y1)
#define _lin_interp(x,y0,y1)        (y0) + (x) * ((y1)-(y0))


/*
cmRC_t        cmPitchShiftFree( cmPitchShift** pp )
{
  cmRC_t rc = cmOkRC;
  if( pp != NULL && *pp != NULL )
  {
    cmPitchShift* p = *pp;
    if(( rc = cmPitchShiftFinal(p)) == cmOkRC )
    {
      cmMemPtrFree(&p->x);
      cmMemPtrFree(&p->y);
      cmMemPtrFree(&p->cf);
      cmObjFree(pp);
    }
  }
  return rc;
}


cmRC_t        cmPitchShiftInit( cmPitchShift* p, unsigned procSmpCnt, double srate )
{
  cmRC_t rc;
  if((rc = cmPitchShiftFinal(p)) != cmOkRC )
    return rc;

  p->procSmpCnt = procSmpCnt;
  p->srate      = srate;
  p->wn         = 4096;
  p->cfn        = 1024;
  p->outN       = p->procSmpCnt;
  p->x          = cmMemResizeZ(cmSample_t, p->x,procSmpCnt + 3 );
  p->y          = cmMemResizeZ(cmSample_t, p->y, p->wn );
  p->cf         = cmMemResizeZ(cmSample_t, p->cf, p->cfn );
  p->pii        = procSmpCnt + p->cfn;
  p->poi        = 0;
  p->outV       = p->y;
  p->prv_x0     = 0;
  p->prv_x1     = 0;
  p->genFl      = true;
  p->xfi        = 3;   
  p->cfi        = 0;
  p->cubeFl     = true;
  p->linCfFl    = true;

  assert( p->cfn+p->procSmpCnt < p->wn );

  unsigned i;
  for(i=0; i<p->cfn; ++i)
  {
    p->cf[i] = (sin(((double)i/p->cfn * M_PI) - (M_PI/2.0)) + 1.0) / 2.0;
    p->cf[i]   = (double)i/p->cfn;
  }
  return rc;
}


// Algorithm:
// 1. Generate srate converted values into p->y[] until it is full.
// 2. Playback from y[] ignoring any incoming samples until less than
//   p->cfn + p->procSmpCnt  samples remain in y[] (i.e. pii < procSmpCnt + cfn).
// 3. At this point begin generating samples from the input and crossfading them
//    with the last cfn samples in y[]. 
// 4. After cfn new samples have been crossfaded proceed to fill the remaining
//   space in y[] 
// 5. Goto 2.
//
// Note that since we are pitch shifting down (ratio<1.0) the number of samples generated 
// will always be greater than the number of input samples.  
//
// Notes:
// For small downward shifts there will be large time aliasing because the length of time
// to fill the buffer (step 4) will  because only a few extra samples
// are generated on each input cycle.  Try decreasing the window length as the pitch ratio increases.
//
// Implement smoothly varying ratio changes.
//
// Change model so that at ratio 1.0 the input is effectively being constantly cross faded to produce
// identity output.


void _cmPitchShiftDown( cmPitchShift* p, double ratio, const cmSample_t* x, unsigned xn )
{
  // shift off the expired samples
  memmove(p->y,p->y + p->procSmpCnt, (p->pii - p->procSmpCnt) * sizeof(cmSample_t));
  p->pii -= p->procSmpCnt;

  // if not currently generating and there are less than cfn + procSmpCnt samples remaining ...
  if( p->genFl == false && p->pii <= p->procSmpCnt + p->cfn )
  {
    // ... then setup to begin generating
    p->cfi   = 0;  
    p->genFl = true;
    p->xfi   = 3;
  }
 
  if( p->genFl )
  {
    // setup the incoming samples
    p->x[0] = p->prv_x0;
    p->x[1] = p->prv_x1;
    p->x[2] = p->prv_x2;
    memcpy(p->x+2,x,p->procSmpCnt*sizeof(cmSample_t));

    int n0 = p->cubeFl ? xn+1 : xn+2;

    // as long as there are incoming samples available and space left in the output buffer
    for(; p->xfi < n0 && p->pii < p->wn; p->xfi += ratio )
    {
      unsigned   xii   = floor(p->xfi);
      double     frac  = p->xfi - xii;

      // generate the value of the output sample
      cmSample_t value;
      if( p->cubeFl )
        value = _cube_interp(frac,p->x[xii-1],p->x[xii],p->x[xii+1],p->x[xii+2]);
      else
        value = p->x[ xii ] + frac *  (p->x[ xii+1 ] - p->x[xii]);

      // if xfading
      if( p->cfi < p->cfn )
      {
        // calc index into y[] of the sample to crossfade with
        int idx = p->pii - p->cfn + p->cfi; 

        p->y[idx] = (1.0 - p->cf[p->cfi]) * p->y[idx] +  p->cf[p->cfi] * value;
        ++p->cfi;
      }
      else
      {
        // else fill the remaing space in y[]
        p->y[p->pii] = value;
        ++p->pii;
      }

    }

    // if the output buffer is full then stop generating
    if( p->pii == p->wn )
      p->genFl = false;
  }

  // reset the input index to the beginning of the buffer
  p->xfi -= xn;

  p->prv_x0 = x[xn-3];
  p->prv_x1 = x[xn-2];
  p->prv_x2 = x[xn-1];
    
}


cmRC_t        cmPitchShiftExec( cmPitchShift* p, const cmSample_t* x, cmSample_t* y, unsigned n, double shiftRatio )
{
  assert(n == p->procSmpCnt);

  if( shiftRatio >= 1 )
    memcpy(y,x,n*sizeof(cmSample_t));
  else
  {
    _cmPitchShiftDown(p,shiftRatio,x,n); 
    if( y != NULL )
      memcpy(y,p->y,n*sizeof(cmSample_t));
  }
  return cmOkRC;
}
*/

cmRC_t        cmPitchShiftFree( cmPitchShift** pp )
{
  cmRC_t rc = cmOkRC;
  if( pp != NULL && *pp != NULL )
  {
    cmPitchShift* p = *pp;
    if(( rc = cmPitchShiftFinal(p)) == cmOkRC )
    {
      cmMemPtrFree(&p->b);
      cmMemPtrFree(&p->outV);
      cmMemPtrFree(&p->wnd);
      cmMemPtrFree(&p->osc[0].y);
      cmMemPtrFree(&p->osc[1].y);
      cmObjFree(pp);
    }
  }
  return rc;
}

/*

cmRC_t cmPitchShiftInit( cmPitchShift* p,  unsigned procSmpCnt, double srate)
{
  cmRC_t rc;

  if((rc = cmPitchShiftFinal(p)) != cmOkRC )
    return rc;

  p->procSmpCnt = procSmpCnt;
  p->srate      = srate;
  p->wn         = 4096;
  p->in         = 2*p->wn + p->procSmpCnt;
  p->outN       = p->procSmpCnt;
  p->m          = cmMemResizeZ(cmSample_t, p->m,p->in + 2 );
  p->x          = p->m + 1;
  p->outV       = cmMemResizeZ(cmSample_t, p->outV,procSmpCnt );
  p->wnd        = cmMemResizeZ(cmSample_t, p->wnd, p->wn+1 );
  p->ii         = 1;
  p->cubeFl     = true;
  p->ei0        = -1;
  p->ei1        = -1;

  p->osc[0].ratio = 1.0;
  p->osc[0].wi    = 0;
  p->osc[0].xi    = 0;
  p->osc[0].yN    = 2*p->wn;
  p->osc[0].y     = cmMemResizeZ(cmSample_t, p->osc[0].y, p->osc[0].yN );
  p->osc[0].yii   = 0;
  p->osc[0].yoi   = 0;
  p->osc[0].ynn    = 0;

  p->osc[1].ratio = 1.0;
  p->osc[1].wi    = floor(p->wn/2);
  p->osc[1].xi    = 0;
  p->osc[1].yN    = 2*p->wn;
  p->osc[1].y     = cmMemResizeZ(cmSample_t, p->osc[1].y, p->osc[1].yN );
  p->osc[1].yii   = 0;
  p->osc[1].yoi   = 0;
  p->osc[1].ynn   = 0;

  assert( p->in >= p->procSmpCnt + 1 );
  
  cmVOS_Hann( p->wnd, p->wn+1 );

  return rc;
}

#define _isInNoGoZone(p,i) ( ((0 <= (i)) && ((i) <= (p)->ei0)) || (((p)->ii <= (i)) && ((i) <= (p)->ei1)) )

#define _isNotInNoGoZone(p,i) (!_isInNoGoZone(p,i))

void _startNewWindow( cmPitchShift* p, cmPitchShiftOsc_t* op, int li, double ratio)
{
  op->wi = 0;

  if( op->ratio < 1.0 )
    op->xi = p->ii - p->procSmpCnt;
  else
    op->xi = p->ii + (li)*(p->procSmpCnt/ratio);

  while( op->xi < 0 )
    op->xi += p->in-1;

  while( op->xi > (p->in-1) )
    op->xi -= (p->in-1);

  op->xi = floor(op->xi);

}

void _cmPitchShiftOsc( cmPitchShift* p, cmPitchShiftOsc_t* op, double ratio )
{
  int  li     = 0;
  bool contFl = 1;

  // if we are waiting to start a new window until the output buffer drains
  if( op->wi==-1 )
  {
    // if the output buffer has enough samples to complete this cycle - return
    if(op->ynn >= p->procSmpCnt)
      return;

    // otherwise start a new window
    _startNewWindow(p,op,li,ratio);

  }

  // iterate until all output samples have been generated
  for(li=0; contFl; ++li)
  {    
    // iterate over one window or until output is complete
    while( op->wi < p->wn )
    {
      int        wii  = floor(op->wi);
      double     frac = op->wi - wii;
      int        xii  = (op->xi + wii) % (p->in-1);
      cmSample_t wnd  = p->wnd[ wii ] + frac * (p->wnd[wii+1] - p->wnd[wii]);
      cmSample_t val;

      // if enough samples have been generated and the next input will not be in the no-go zone
      if( (op->ynn >= p->procSmpCnt) && _isNotInNoGoZone(p,xii-1) && _isNotInNoGoZone(p,xii+2) )
      {
        contFl = false;
        break;
      }

      if( op->ynn >= op->yN )
      {
        printf("OVER\n");
        contFl = false;
        break;
      }

      // generate the value of the output sample
      if( p->cubeFl )
        val = _cube_interp(frac,p->x[xii-1],p->x[xii],p->x[xii+1],p->x[xii+2]);
      else
        val = p->x[ xii ] + frac *  (p->x[ xii+1 ] - p->x[xii]);

      // apply the window function and assign to output
      op->y[op->yii] = wnd * val;

      // incr the count of samples in y[]
      ++op->ynn;

      // advance the output buffers input location
      op->yii = (op->yii + 1) % op->yN;

      op->wi += ratio; // increment the window location

    }

    // if a window completed
    if( op->wi >= p->wn )
    {
      if( (ratio < 1.0) && (op->ynn >= p->procSmpCnt) )
      {
        op->wi = -1;
        break;
      }
      _startNewWindow(p,op,li+1,ratio);
    }

  }  


}

//   in=5
//   m x
//     0 1 2 3 4 5  
// 1 x x a b c d e 
// 2 c d e f g h i 
// 3 g h i j k l m
//
// ratio < 1 - the output oscillators must be able to generate 1 complete window prior to new samples
// overwriting the oscillators location in the input buffer. 

cmRC_t cmPitchShiftExec( cmPitchShift* p, const cmSample_t* x, cmSample_t* y, unsigned xn, double ratio )
{
  cmRC_t rc = cmOkRC;

  //memcpy(y,x,xn*sizeof(cmSample_t));
  //return rc;

  int i;

  // copy in the incoming samples
  for(i=0; i<xn; ++i)
  {
    p->x[p->ii] = x[i];

    ++p->ii;

    if( p->ii == p->in+1 )
    {
      p->x[-1] = p->x[p->in-2];
      p->x[ 0] = p->x[p->in-1];
      p->x[ 1] = p->x[p->in];
      p->ii    = 2;
    }
  }

  // locate the section of the input buffer which
  // will be written over on the next cycle.
  // The oscillators have to avoid ending in this area
  if( (p->in+1) - p->ii >= p->procSmpCnt )
  {
    p->ei1 = p->ii + p->procSmpCnt - 1;
    p->ei0 = -1;    
  }
  else
  {
    int n = (p->in+1) - p->ii; 
    p->ei1 = (p->ii + n) - 1;
    p->ei0 = (p->procSmpCnt - n) - 1;
  }
  
  //memset(p->outV,0,p->procSmpCnt*sizeof(cmSample_t));
  _cmPitchShiftOsc(p, p->osc + 0, ratio );
  _cmPitchShiftOsc(p, p->osc + 1, ratio );

  // mix the indidual output of the two oscillators to form the final output
  if( p->osc[0].ynn < p->procSmpCnt || p->osc[1].ynn  < p->procSmpCnt )
    printf("UNDER\n");

  for(i=0; i<p->procSmpCnt; ++i)
  {
    p->outV[i]    = p->osc[0].y[p->osc[0].yoi] + p->osc[1].y[p->osc[1].yoi];
    p->osc[0].yoi = (p->osc[0].yoi + 1) % p->osc[0].yN;
    p->osc[1].yoi = (p->osc[1].yoi + 1) % p->osc[1].yN;
  }

  p->osc[0].ynn -= p->procSmpCnt;
  p->osc[1].ynn -= p->procSmpCnt;

  if( y != NULL )
    memcpy(y,p->outV,p->outN*sizeof(cmSample_t));

  return rc;
  
}
*/

cmRC_t cmPitchShiftInit( cmPitchShift* p,  unsigned procSmpCnt, double srate)
{
  cmRC_t rc;

  if((rc = cmPitchShiftFinal(p)) != cmOkRC )
    return rc;

  p->procSmpCnt = procSmpCnt;
  p->outN       = procSmpCnt;
  p->outV       = cmMemAllocZ(cmSample_t,procSmpCnt);
  p->wn         = 2048;
  p->xn         = 2*p->wn + procSmpCnt;
  p->bn         = p->xn + 3;
  p->b          = cmMemAllocZ(cmSample_t,p->bn);
  p->x          = p->b + 1;
  p->wnd        = cmMemAllocZ(cmSample_t,p->wn+1);
  p->xni        = p->xn - procSmpCnt + 2;
  p->cubeFl     = true;

  int i;
  for(i=0; i<2; ++i)
  {
    cmPitchShiftOsc_t* op = p->osc + i;
    op->xi = p->procSmpCnt;
    op->yn = p->wn * 2;
    op->y  = cmMemAllocZ(cmSample_t,op->yn);
    op->yii= 0;
    op->yoi= 0;
    op->ynn= 0;
    op->wi = i==0 ? 0 : p->wn/2;
  }

  cmVOS_Hann(p->wnd,p->wn+1);

  return rc;
}

void _cmPitchShiftOscExec( cmPitchShift* p, cmPitchShiftOsc_t* op, double ratio )
{

  int k = 0;

  // account for right buffer shift on input
  op->xi -= p->procSmpCnt; 

  // pass through code for testing
  if(0)
  {
    int i;
    for(i=0; i<p->procSmpCnt; ++i)
    {
      op->y[op->yii] = p->x[ (int)floor(op->xi) ];
      op->yii = (op->yii + 1) % op->yn;
      op->ynn += 1;
      op->xi  += 1.0;
    }
    return;
  }
  

  while(1)
  {
    int        wii   = floor(op->wi);
    double     wfrac = op->wi - wii;

    int        xii   = floor(op->xi);
    double     vfrac = op->xi - xii;
    cmSample_t val;

    // if enough output samples have been generated and we are outside the no-land zone
    if( (op->ynn >= p->procSmpCnt && op->xi >= p->procSmpCnt)  )
      break;

    if( op->xi >= p->xn )
    {
      //printf("Wrap %f %f\n",op->xi,op->wi);
    }

    cmSample_t wnd   = p->wnd[ wii ] + wfrac * (p->wnd[wii+1] - p->wnd[wii]);

    // generate the value of the output sample
    if( p->cubeFl )
      val = _cube_interp(vfrac,p->x[xii-1],p->x[xii],p->x[xii+1],p->x[xii+2]);
    else
      val = p->x[ xii ] + vfrac *  (p->x[ xii+1 ] - p->x[xii]);

    // apply the window function and assign to output
    op->y[op->yii] = wnd * val;

    // incr the count of samples in y[]
    ++op->ynn;

    // advance the output buffers input location
    op->yii = (op->yii + 1) % op->yn;

    op->wi += ratio<1 ? 1 : ratio; // increment the window location
    op->xi += ratio;

    if( op->wi >= p->wn )
    {
      ++k;

      op->wi = 0;
      
      if( ratio < 1 )
        op->xi = p->xni;  // begin of most recent block of procSmpCnt input samples
      else
        op->xi = k*p->procSmpCnt/ratio; 

    }
  }

  //if( op->ynn != p->procSmpCnt )
  //  printf("wi:%f xi:%f ynn:%i\n",op->wi,op->xi,op->ynn);

}
//  b: 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17          
//  x: -1 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16
//
// On each cycle:
// 1) shift x[] left by procSmpCnt samples.
// 2) add new samples on right side of x[]
// 3) oscillator scans from right to left.
cmRC_t cmPitchShiftExec( cmPitchShift* p, const cmSample_t* x, cmSample_t* y, unsigned xn, double ratio, bool bypassFl )
{
  if(0)
  {
    memcpy(p->outV,x,xn*sizeof(cmSample_t));

    if( y != NULL )
      memcpy(y,x,xn*sizeof(cmSample_t));

    return cmOkRC;
  }

  if( y == NULL )
    return cmOkRC;

  if( x == NULL )
  {
    cmVOS_Zero(y,xn);
    return cmOkRC;
  }

  if( bypassFl )
  {
    memcpy(p->outV,x,xn*sizeof(cmSample_t));

    if( y != NULL )
      memcpy(y,x,xn*sizeof(cmSample_t));

    return cmOkRC;
  }

  int i;
  memmove(p->x - 1,      p->x+p->procSmpCnt-1,(p->xn - p->procSmpCnt + 3)*sizeof(cmSample_t));
  memcpy( p->x + p->xni, x,                    xn * sizeof(cmSample_t) );

  //memmove(p->x + p->procSmpCnt -1, p->x - 1, (p->xn - p->procSmpCnt + 3) * sizeof(cmSample_t));
  //memcpy( p->x-1, x, xn * sizeof(cmSample_t));

  _cmPitchShiftOscExec(p, p->osc + 0, ratio );
  _cmPitchShiftOscExec(p, p->osc + 1, ratio );

  // mix the indidual output of the two oscillators to form the final output
  if( p->osc[0].ynn < p->procSmpCnt || p->osc[1].ynn  < p->procSmpCnt )
    printf("UNDER\n");

  for(i=0; i<p->procSmpCnt; ++i)
  {
    p->outV[i]    = p->osc[0].y[p->osc[0].yoi] + p->osc[1].y[p->osc[1].yoi];
    p->osc[0].yoi = (p->osc[0].yoi + 1) % p->osc[0].yn;
    p->osc[1].yoi = (p->osc[1].yoi + 1) % p->osc[1].yn;
  }

  p->osc[0].ynn -= p->procSmpCnt;
  p->osc[1].ynn -= p->procSmpCnt;

  if( y != NULL )
    memcpy(y,p->outV,xn*sizeof(cmSample_t));

  return cmOkRC;
}

//=======================================================================================================================
cmLoopRecord* cmLoopRecordAlloc( 
  cmCtx* c, cmLoopRecord* p, unsigned procSmpCnt, unsigned maxRecdSmpCnt, unsigned xfadeSmpCnt )
{
  cmLoopRecord* op = cmObjAlloc(cmLoopRecord, c, p );
  if( procSmpCnt != 0 )
    if( cmLoopRecordInit(op, procSmpCnt, maxRecdSmpCnt, xfadeSmpCnt) != cmOkRC )
      cmLoopRecordFree(&op);
  return op;
}
/*
cmRC_t cmLoopRecordFree( cmLoopRecord** pp )
{
  cmRC_t rc = cmOkRC;
  if( pp != NULL && *pp != NULL )
  {
    cmLoopRecord* p = *pp;
    if(( rc = cmLoopRecordFinal(p)) == cmOkRC )
    {
      cmMemPtrFree(&p->bufMem);
      cmMemPtrFree(&p->bufArray);
      cmMemPtrFree(&p->outV);
      cmMemPtrFree(&p->xfadeFunc);
      cmObjFree(pp);
    }
  }
  return rc;
}

cmRC_t cmLoopRecordInit( cmLoopRecord* p, unsigned procSmpCnt, unsigned maxRecdSmpCnt, unsigned xfadeSmpCnt )
{
  cmRC_t   rc;
  unsigned i;

  if((rc = cmLoopRecordFinal(p)) != cmOkRC )
    return rc;

  assert( xfadeSmpCnt < maxRecdSmpCnt );

  p->maxRecdSmpCnt = maxRecdSmpCnt;
  p->bufArrayCnt = 2;
  p->bufMem      = cmMemResizeZ( cmSample_t,    p->bufMem,    maxRecdSmpCnt * p->bufArrayCnt );
  p->bufArray    = cmMemResizeZ( cmLoopRecdBuf, p->bufArray,  p->bufArrayCnt );
  p->outV        = cmMemResizeZ( cmSample_t,    p->outV,      procSmpCnt );
  p->xfadeFunc   = cmMemResizeZ( cmSample_t,    p->xfadeFunc, xfadeSmpCnt+1 );
  p->outN        = procSmpCnt;
  p->procSmpCnt  = procSmpCnt;
  p->xfadeSmpCnt = xfadeSmpCnt;
  p->recdBufIdx  = 0;
  p->playBufIdx  = 1;
  p->recdFl      = false;
  p->playFl      = false;

  for(i=0; i<p->bufArrayCnt; ++i)
  {
    p->bufArray[i].bV  = p->bufMem + (i * maxRecdSmpCnt);
    p->bufArray[i].bN  = maxRecdSmpCnt;
    p->bufArray[i].xfi = xfadeSmpCnt;
  }

  for(i=0; i<=p->xfadeSmpCnt; ++i)
    p->xfadeFunc[i] = (cmReal_t)i/p->xfadeSmpCnt;

  return rc;
}

cmRC_t cmLoopRecordFinal( cmLoopRecord* p )
{ return cmOkRC; }

cmRC_t cmLoopRecordExec( cmLoopRecord* p, const cmSample_t* x, cmSample_t* y, unsigned xn, bool recdFl, bool playFl )
{
  unsigned i;

  // if the recdFl was enabled
  if(recdFl==true && p->recdFl==false)
  {
    p->recdFl = true;
    p->bufArray[p->recdBufIdx].bii = 0;
    p->bufArray[p->recdBufIdx].boi = 0;
    printf("Recd:on %i\n",p->recdBufIdx);
  }
  else
    // if the recdFl was disabled
    if( recdFl==false && p->recdFl==true )
    {
      p->recdFl     = false;
      playFl        = true;
      p->playBufIdx =  p->recdBufIdx;
      p->recdBufIdx = (p->recdBufIdx + 1) % p->bufArrayCnt;
      printf("Recd:off\n");
    }

  // if the playFl triggered on
  if( playFl==true && p->playFl==false)
  {
    p->bufArray[p->playBufIdx].boi = 0;
    p->playFl = true;
    //printf("Play:on %i %i\n",p->playBufIdx,p->bufArray[p->playBufIdx].bii);
  }

  // if recording 
  if( p->recdFl )
  {
    cmLoopRecdBuf* rp = p->bufArray + p->recdBufIdx;
    
    for(i=0; i<xn && rp->bii < rp->bN; ++i,++rp->bii)
    {
      rp->bV[ rp->bii ] = x[i];
    }

    p->recdFl = rp->bii != rp->bN;
  }

  // if playing
  if( p->playFl )
  {
    cmLoopRecdBuf* rp = p->bufArray + p->playBufIdx;
    unsigned xf0n =  p->xfadeSmpCnt/2;
    unsigned xf1n =  rp->bii - p->xfadeSmpCnt/2;
    unsigned xfi0 = (rp->boi + p->xfadeSmpCnt/2) % rp->bii;

    for(i=0; i<xn; ++i)
    {
      bool       fl  = rp->boi < xf0n || rp->boi > xf1n;
      cmSample_t env = fl ? p->xfadeFunc[ rp->xfi ] : 1;

      p->outV[i] = ((1.0-env) * rp->bV[ rp->boi ]) + (env * rp->bV[ xfi0 ]);
      rp->boi    = (rp->boi + 1) % rp->bii;
      xfi0       = (xfi0    + 1) % rp->bii;
    }
  }

  if( y != NULL )
    memcpy(y,p->outV,xn*sizeof(cmSample_t));

  return cmOkRC;
}

*/

cmRC_t cmLoopRecordFree( cmLoopRecord** pp )
{
  cmRC_t rc = cmOkRC;
  if( pp != NULL && *pp != NULL )
  {
    cmLoopRecord* p = *pp;
    if(( rc = cmLoopRecordFinal(p)) == cmOkRC )
    {
      cmMemPtrFree(&p->bufMem);
      cmMemPtrFree(&p->bufArray);
      cmMemPtrFree(&p->outV);
      cmObjFree(pp);
    }
  }
  return rc;
}

cmRC_t cmLoopRecordInit( cmLoopRecord* p, unsigned procSmpCnt, unsigned maxRecdSmpCnt, unsigned xfadeSmpCnt )
{
  cmRC_t   rc;
  unsigned i;

  if((rc = cmLoopRecordFinal(p)) != cmOkRC )
    return rc;

  assert( xfadeSmpCnt < maxRecdSmpCnt );

  p->maxRecdSmpCnt = maxRecdSmpCnt;
  p->bufArrayCnt = 2;
  p->bufMem      = cmMemResizeZ( cmSample_t,    p->bufMem,    (maxRecdSmpCnt+3) * p->bufArrayCnt * 2 );
  p->bufArray    = cmMemResizeZ( cmLoopRecdBuf, p->bufArray,  p->bufArrayCnt );
  p->outV        = cmMemResizeZ( cmSample_t,    p->outV,      procSmpCnt );
  p->outN        = procSmpCnt;
  p->procSmpCnt  = procSmpCnt;
  p->xfadeSmpCnt = xfadeSmpCnt;
  p->recdBufIdx  = 0;
  p->playBufIdx  = 1;
  p->recdFl      = false;
  p->playFl      = false;

  for(i=0; i<p->bufArrayCnt; ++i)
  {
    p->bufArray[i].xV  = p->bufMem + ((2 * i + 0) * (maxRecdSmpCnt+3)) + 1;
    p->bufArray[i].xN  = maxRecdSmpCnt;
    p->bufArray[i].xii = 0;
    p->bufArray[i].wV  = p->bufMem + ((2 * i + 1) * (maxRecdSmpCnt+3));
  }

  return rc;
}

cmRC_t cmLoopRecordFinal( cmLoopRecord* p )
{ return cmOkRC; }

void _cmLoopRecordOscExec( cmLoopRecord* p, cmLoopRecdBuf* rp, cmLoopRecdOsc* op, double ratio )
{
  unsigned i;
  for(i=0; i<p->outN; ++i)
  {
    int         xi    = floor(op->xi);
    double      vfrac = op->xi - xi;
    cmSample_t  val   = _cube_interp(vfrac,rp->xV[xi-1],rp->xV[xi],rp->xV[xi+1],rp->xV[xi+2]);

    int         wi    = floor(op->wi);
    double      wfrac = op->wi - wi;
    cmSample_t  wnd   = rp->wV[ wi ] + wfrac * (rp->wV[wi+1] - rp->wV[wi]);


    wnd   = (wnd - 0.5) * op->u + 0.5;
    
    p->outV[i] += wnd * val;

    op->wi += ratio;

    if( op->wi >= rp->xii )
    {
      op->wi -= rp->xii;
      op->u *= -1.0; 
    }

    op->xi += ratio;
    if( op->xi >= rp->xii )
      op->xi -= rp->xii;

  }
}

//
//  a   b   c   d   e   f   g   h    i  - osc 0 sample value
// 0.0 .25 0.5 .75 1.0 1.0 .75 0.5 .25  - osc 0 window value
//
//  e   f   g   h   i   a   b   c    d  - osc 1 sample value
// 1.0 .75 0.5 .25 0.0 0.0 .25 0.5 .75  - osc 1 window value
//
// Notes:
// 1) The window values transition through zero
// at the loop point.
// 2) 

cmRC_t cmLoopRecordExec( cmLoopRecord* p, const cmSample_t* x, cmSample_t* y, unsigned xn, bool bypassFl, bool recdFl, bool playFl, double ratio, double pgain, double rgain )
{
  int i,j;

  assert( xn <= p->outN );

  if( bypassFl )
  {
    memcpy(p->outV,x,xn*sizeof(cmSample_t));

    if( y != NULL )
    {
      //memcpy(y,x,xn*sizeof(cmSample_t));
      cmVOS_MultVVS(y,xn,x,pgain);
    }
    return cmOkRC;
  }

  // if the recdFl was enabled
  if(recdFl==true && p->recdFl==false)
  {
    p->recdFl                      = true;
    p->bufArray[p->recdBufIdx].xii = 0;
    //printf("Recd:on %i\n",p->recdBufIdx);
  }
  else
    // if the recdFl was disabled
    if( recdFl==false && p->recdFl==true )
    {
      cmLoopRecdBuf* rp = p->bufArray + p->recdBufIdx;

      // arrange the 'wrap-around' samples
      rp->xV[-1]        = rp->xV[rp->xii-1];
      rp->xV[rp->xii  ] = rp->xV[0];
      rp->xV[rp->xii+1] = rp->xV[1];

      // calc the length of the cross-fade
      if( rp->xii < p->xfadeSmpCnt * 2)
        rp->xfN = rp->xii/2;
      else
        rp->xfN = p->xfadeSmpCnt;

      assert( rp->xfN > 0 );

      // fill the crossfade window with zeros
      cmVOS_Zero(rp->wV,rp->xii);


      // fill the last xfN samples in the xfade window vector with a fade-in slope
      int xfi = rp->xii - rp->xfN;
      
      for(j=0; j<rp->xfN; ++j)
        rp->wV[xfi+j] = (cmSample_t)j/rp->xfN;

      // initialize the oscillators
      for(j=0; j<2; ++j)
      {
        // 
        rp->osc[j].xi = j==0 ? 0.0 : rp->xfN; // oscillators are p->xfN samples out of phase
        rp->osc[j].u  = j==0 ? 1.0 : -1.0;   // set windows to have opposite polarity
        rp->osc[j].wi = xfi;                 // begin window at cross-fade
      }

      p->recdFl     = false;
      p->playFl     = true;
      p->playBufIdx =  p->recdBufIdx;
      p->recdBufIdx = (p->recdBufIdx + 1) % p->bufArrayCnt;
      //printf("Recd:off\n");
    }


  // if recording 
  if( p->recdFl )
  {
    cmLoopRecdBuf* rp = p->bufArray + p->recdBufIdx;
    
    for(i=0; i<xn && rp->xii < rp->xN; ++i,++rp->xii)
    {
      rp->xV[ rp->xii ] = x[i];
    }

  }

  if( playFl && p->bufArray[p->playBufIdx].xii > 0 )
  {
    p->playFl = !p->playFl;
    if( p->playFl )
    {
      // reset oscillators to start at the begin of loop 
    }
      
  }


  if( p->playFl )
  {
    cmLoopRecdBuf* rp = p->bufArray + p->playBufIdx;
    cmVOS_Zero(p->outV,p->outN);
    _cmLoopRecordOscExec(p,rp, rp->osc + 0, ratio );
    _cmLoopRecordOscExec(p,rp, rp->osc + 1, ratio );
  }

  if( y != NULL )
  {
    //memcpy(y,p->outV,xn*sizeof(cmSample_t));
    for(i=0; i<p->outN; ++i)
      y[i] = (pgain * x[i]) + (rgain * p->outV[i]);
  }

  return cmOkRC;
}

  //=======================================================================================================================
cmGateDetect* cmGateDetectAlloc( cmCtx* c, cmGateDetect* p, unsigned procSmpCnt, cmReal_t onThreshPct, cmReal_t onThreshDb, cmReal_t offThreshDb )
{
  cmGateDetect* op = cmObjAlloc(cmGateDetect, c, p );

  if( procSmpCnt != 0 )
    if( cmGateDetectInit(op, procSmpCnt, onThreshPct, onThreshDb, offThreshDb) != cmOkRC )
      cmGateDetectFree(&op);
  return op;

}

cmRC_t       cmGateDetectFree( cmGateDetect** pp )
{
  cmRC_t rc;

  if( pp==NULL || *pp == NULL )
    return cmOkRC;

  cmGateDetect* p = *pp;

  if((rc = cmGateDetectFinal(p)) != cmOkRC )
    return rc;
  
  cmMemPtrFree(&p->rmsV);
  cmMemPtrFree(&p->wndV);
  cmObjFree(pp);

  return cmOkRC;
}

cmRC_t       cmGateDetectInit( cmGateDetect* p, unsigned procSmpCnt, cmReal_t onThreshPct, cmReal_t onThreshDb, cmReal_t offThreshDb )
{
  cmRC_t rc;
  if((rc = cmGateDetectFinal(p)) != cmOkRC )
    return rc;

  p->rmsN        = 3;
  p->rmsV        = cmMemResizeZ(cmSample_t,p->rmsV,p->rmsN);
  p->wndN        = 9;
  p->wndV        = cmMemResizeZ(cmSample_t,p->wndV,p->wndN);
  p->rms         = 0;
  p->durSmpCnt   = 0;
  p->gateFl      = false;
  p->deltaFl     = false;
  p->onThreshPct = onThreshPct;
  p->onThreshDb  = onThreshDb;
  p->offThreshDb = offThreshDb;

  return rc;
}

cmRC_t       cmGateDetectFinal(cmGateDetect* p )
{
  return cmOkRC;
}

cmRC_t         cmGateDetectExec22( cmGateDetect* p, const cmSample_t* x, unsigned xn )
{
  p->deltaFl    = false;
  p->rms        = cmVOS_RMS(x,xn,xn);
  cmReal_t   db = p->rms==0 ? -100 : 20*log10(p->rms);
  //cmSample_t b0 = 0;
  cmSample_t b1 = 0;

  cmVOS_Shift(p->rmsV,p->rmsN,-1,p->rms);
   
   // if gate is on
   if( p->gateFl )
   {
     p->deltaFl = db < p->offThreshDb;
   }
   else
   {
     
     //cmVOS_Lsq1(NULL,p->rmsV,p->rmsN,&b0,&b1);
     // p->deltaFl =  b1 > p->onThreshPct && db > p->onThreshDb;       

     unsigned i;
     
     for(i=0; i<p->rmsN-1; ++i)
       b1 += p->rmsV[i+1] - p->rmsV[i];

     b1 /= p->rmsN;

     p->deltaFl = b1 > p->onThreshPct && db > p->onThreshDb;

     
   }

   if( p->deltaFl )
   {
     p->gateFl    = !p->gateFl;
     p->durSmpCnt = xn;
   }
   else
   {
     p->durSmpCnt += xn;
   }


   //p->rms = b1;
   //printf("%f %f %i %f;\n",db,p->rms,p->deltaFl,b1);

  return cmOkRC;
}

cmRC_t  cmGateDetectExec( cmGateDetect* p, const cmSample_t* x, unsigned xn )
{
  p->deltaFl    = false;
  p->rms        = cmVOS_RMS(x,xn,xn);
  cmReal_t   db = p->rms==0 ? -100 : 20*log10(p->rms);
  unsigned   i;
  cmSample_t d = 0;

  // update the rms window
  cmVOS_Shift(p->rmsV,p->rmsN,-1,p->rms);
 
  // calc. derr of RMS   
  for(i=0; i<p->rmsN-1; ++i)
    d += p->rmsV[i+1] - p->rmsV[i];

  d /= p->rmsN;
  
  // zero negative derr's  
  d = d<0 ? 0 : d;

  // update the peak window
  cmVOS_Shift(p->wndV,p->wndN,-1,d);

  p->mean = cmVOS_Mean(p->wndV,p->wndN);
  
  // if this is an offset
  if( p->gateFl && db < p->offThreshDb )
  {
    p->gateFl = false;
    p->deltaFl = true;
  }
  else
    // if this may be an onset.
    if( db > p->onThreshDb )
    {
    
      i = p->wndN - 2;

      // if previous derr was a peak
      if( p->wndV[i]>0 && p->wndV[i-1] < p->wndV[i] && p->wndV[i] > p->wndV[i+1] )
      {
        // and the peak is the largest value in the exteneded window
        if( p->wndV[i] > p->mean )
        {
          p->deltaFl   = true;
          p->gateFl    = true;
          p->durSmpCnt = 0;
        }
      
      }
      
    }

  if( p->gateFl )
    p->durSmpCnt += xn;

  p->rms = p->d0;
  p->d0  = d;

  return cmOkRC;
}

//=======================================================================================================================
cmGateDetect2* cmGateDetectAlloc2( cmCtx* c, cmGateDetect2* p, unsigned procSmpCnt, const cmGateDetectParams* args )
{
  cmGateDetect2* op = cmObjAlloc(cmGateDetect2, c, p );

  if( procSmpCnt != 0 )
    if( cmGateDetectInit2(op, procSmpCnt, args) != cmOkRC )
      cmGateDetectFree2(&op);
  return op;

}

cmRC_t         cmGateDetectFree2( cmGateDetect2** pp )
{
  cmRC_t rc;

  if( pp==NULL || *pp == NULL )
    return cmOkRC;

  cmGateDetect2* p = *pp;

  if((rc = cmGateDetectFinal2(p)) != cmOkRC )
    return rc;
  
  cmMemPtrFree(&p->medV);
  cmObjFree(pp);

  return cmOkRC;

}

cmRC_t         cmGateDetectInit2( cmGateDetect2* p, unsigned procSmpCnt, const cmGateDetectParams* args )
{
  cmRC_t rc;
  if((rc = cmGateDetectFinal2(p)) != cmOkRC )
    return rc;

  unsigned pkCnt = 3;
  
  unsigned eleCnt = 3*args->medCnt + args->avgCnt + args->suprCnt + args->offCnt + pkCnt;
  unsigned i;

  p->args  = *args;
  p->medV  = cmMemResizeZ(cmSample_t,p->medV,eleCnt);
  p->fcofV = p->medV  + args->medCnt;
  p->fdlyV = p->fcofV + args->medCnt;
  p->avgV  = p->fdlyV + args->medCnt;
  p->suprV = p->avgV  + args->avgCnt;
  p->offV  = p->suprV + args->suprCnt;
  p->pkV   = p->offV  + args->offCnt;

  assert( p->medV + eleCnt == p->pkV + pkCnt );

  p->medIdx    = 0;
  p->avgIdx    = 0;
  p->suprIdx   = args->suprCnt;
  p->offIdx    = 0;

  p->pkFl      = false;
  p->gateFl    = false;
  p->onFl      = false;
  p->offFl     = false;

  cmGateDetectSetOnThreshDb2(p,args->onThreshDb);
  cmGateDetectSetOnThreshDb2(p,args->offThreshDb);

  p->fcofV[0] = 1.0;
  for(i=1; i<args->medCnt; ++i)
  {
    p->fcofV[i] = (1.0 - 1.0/args->medCnt) / ( pow(2.0,i-1) );
    //printf("%i %f ",i,p->fcofV[i]);
  }

  for(i=0; i<args->suprCnt; ++i)
  {
    p->suprV[i] = 1.0/pow(args->suprCoeff,i + 1.0);
    //printf("%i %f ",i,p->suprV[i]);
  }

  return rc;
}

cmRC_t         cmGateDetectFinal2(cmGateDetect2* p )
{ return cmOkRC; }

void _cmGateDetectPickPeak( cmGateDetect2* p, cmSample_t input )
{
  p->pkV[0] = p->pkV[1];
  p->pkV[1] = p->pkV[2];
  p->pkV[2] = input;

  // if supression is active - apply it to the center pkV[]
  if( p->suprIdx < p->args.suprCnt )
  {
    p->pkV[1] -= p->pkV[1] * p->suprV[p->suprIdx];
    p->suprIdx = (p->suprIdx + 1) % p->args.suprCnt;
  }

  p->sup = p->pkV[1];

  // if the center value in pkV[] is a local peak and above the onset threshold
  if( p->pkV[1] > p->onThresh && p->pkV[0] < p->pkV[1] && p->pkV[1] > p->pkV[2] )  
  {
    p->suprIdx = 0;
    p->gateFl  = true;
    p->onFl    = true;
  }

  // update the offset buffer
  p->offV[p->offIdx] = fabs(p->pkV[2]);
  p->offIdx          = (p->offIdx + 1) % p->args.offCnt;

  // if this is an offset
  if( p->gateFl==true && cmVOS_Mean(p->offV,p->args.offCnt) < p->offThresh )
  {
    p->gateFl = false;
    p->offFl  = true;
  }
  
}

void _cmGateDetectPickPeak2( cmGateDetect2* p, cmSample_t input )
{
  // if supression is active - apply it to the input
  if( p->suprIdx < p->args.suprCnt )
  {
    input     -= input * p->suprV[p->suprIdx];
    ++p->suprIdx;// = (p->suprIdx + 1) % p->args.suprCnt;
  }

  // update the peak buffer
  p->pkV[0] = p->pkV[1];
  p->pkV[1] = p->pkV[2];
  p->pkV[2] = input;

  p->sup = input;

  // if the signal increased above the threshold and is not already waiting for a peak
  if( p->pkV[1] < input && input > p->onThresh && p->pkFl == false )
  {
    p->pkFl   = true;
    p->gateFl = true;
    p->onFl   = true;
  }

  // if the center value in pkV[] is a local peak and above the onset threshold ...
  if( p->gateFl && p->pkFl  && p->pkV[0] < p->pkV[1] && p->pkV[1] > p->pkV[2] )  
  {
    p->suprIdx = 0;      // ... turn on supression
    p->pkFl    = false;  // ... not longer waiting for the peak
  }

  // update the offset buffer
  p->offV[p->offIdx] = fabs(input);
  p->offIdx          = (p->offIdx + 1) % p->args.offCnt;

  // if this is an offset
  if( p->gateFl==true && cmVOS_Mean(p->offV,p->args.offCnt) < p->offThresh )
  {
    p->gateFl = false;
    p->offFl  = true;
    p->pkFl   = false;
  }
 
}

cmRC_t         cmGateDetectExec2( cmGateDetect2* p, const cmSample_t* x, unsigned xn )
{
  p->rms               = cmVOS_RMS(x,xn,xn);                    // take RMS of incoming window
  p->medV[ p->medIdx ] = p->rms;                                // input RMS to median filter
  p->medIdx            = (p->medIdx + 1) % p->args.medCnt;

  p->med               = cmVOS_Median(p->medV,p->args.medCnt);  // calc the median
  p->dif               = cmMax(0,p->rms - p->med);              // dif = half_rect( rms - med )   
  p->avgV[ p->avgIdx ] = p->dif;                                // input dif to avg filter
  p->avgIdx            = (p->avgIdx + 1) % p->args.avgCnt;

  p->avg               = cmVOS_Mean(p->avgV,p->args.avgCnt);    // calc the avg 
  p->ons               = p->dif - p->avg;                       // ons = dif - avg
  
  cmVOS_Shift(p->fdlyV,p->args.medCnt,1,p->ons);                    
  p->flt      = cmVOS_MultSumVV(p->fdlyV,p->fcofV,p->args.medCnt); 

  p->offFl  = false;
  p->onFl   = false;

  _cmGateDetectPickPeak2(p,p->flt);

  return cmOkRC;
}

void       _cmGateDetectSetDb2( cmGateDetect2* p, cmReal_t db, cmReal_t* dbPtr )
{
  *dbPtr  = pow(10.0, db/20.0  );
}

void      cmGateDetectSetOnThreshDb2( cmGateDetect2* p, cmReal_t db )
{ _cmGateDetectSetDb2(p,db,&p->onThresh); }

void       cmGateDetectSetOffThreshDb2( cmGateDetect2* p, cmReal_t db )
{ _cmGateDetectSetDb2(p,db,&p->offThresh); }



//=======================================================================================================================

cmAutoGain* cmAutoGainAlloc( cmCtx* c, cmAutoGain* p, unsigned procSmpCnt, cmReal_t srate, cmReal_t hopMs, unsigned chCnt, const cmGateDetectParams* gd_args )
{
  cmAutoGain* op = cmObjAlloc(cmAutoGain, c, p );

  op->sbp = cmShiftBufAlloc(c,NULL,0,0,0);
  op->gdp = cmGateDetectAlloc2(c,NULL,0,NULL);

  if( procSmpCnt != 0 )
    if( cmAutoGainInit(op, procSmpCnt, srate, hopMs, chCnt, gd_args) != cmOkRC )
      cmAutoGainFree(&op);
  return op;
  
}

cmRC_t      cmAutoGainFree( cmAutoGain** pp )
{
  cmRC_t rc;

  if( pp==NULL || *pp == NULL )
    return cmOkRC;

  cmAutoGain* p = *pp;

  if((rc = cmAutoGainFinal(p)) != cmOkRC )
    return rc;
  
  cmMemPtrFree(&p->chArray);
  p->chCnt = 0;

  cmShiftBufFree(&p->sbp);
  cmGateDetectFree2(&p->gdp);

  cmObjFree(pp);

  return cmOkRC;

}

cmRC_t      cmAutoGainInit( cmAutoGain* p, unsigned procSmpCnt, cmReal_t srate, cmReal_t hopMs, unsigned chCnt, const cmGateDetectParams* gd_args )
{
  unsigned i;
  cmRC_t rc;
  if((rc = cmAutoGainFinal(p)) != cmOkRC )
    return rc;

  unsigned hopSmpCnt = (unsigned)floor(srate * hopMs / 1000.0);
  unsigned wndSmpCnt = hopSmpCnt * gd_args->medCnt;

  cmShiftBufInit(p->sbp,procSmpCnt,wndSmpCnt,hopSmpCnt);
  cmGateDetectInit2(p->gdp,procSmpCnt,gd_args);

  p->chCnt     = chCnt;
  p->chArray   = cmMemResizeZ(cmAutoGainCh,p->chArray,p->chCnt);
  p->chp       = NULL;

  for(i=0; i<p->chCnt; ++i)
    p->chArray[i].id = cmInvalidIdx;

  return rc;
}

cmRC_t      cmAutoGainFinal( cmAutoGain* p )
{ return cmOkRC;}

void _cmAutoGainChannelFinish( cmAutoGain* p )
{
  if( p->chp != NULL )
  {
    if( p->gateMax != 0 )
    {
      p->gateSum += p->gateMax;
      p->gateCnt += 1;
    }

    p->chp->gateMaxAvg = p->gateCnt == 0 ? 0.0 : p->gateSum / p->gateCnt;
  }

}

cmRC_t      cmAutoGainStartCh( cmAutoGain* p, unsigned id )
{
  cmReal_t rmsMax = 1.0;
  unsigned i;

  if( id == cmInvalidIdx )
    return cmOkRC;

  // do intermediate channel calculations on the last selected channel
  if( p->chp != NULL )
    _cmAutoGainChannelFinish( p );

  
  // if 'id' has already been used
  //   then select the associated channel as the current channel
  //   otherwise select the next avail channel as the current channel      
  for(i=0; i<p->chCnt; ++i)
    if( p->chArray[i].id == id || p->chArray[i].id == cmInvalidId )
    {
      p->chp = p->chArray + i;
      break;
    }
  

  if( p->chp == NULL )
    return cmCtxRtCondition( &p->obj, cmArgAssertRC, "All channels are in use.");


  p->chp->id = id;
  p->gateCnt = 0;
  p->gateSum = 0;
  p->gateMax = 0;
  p->minRms  = rmsMax;

  return cmOkRC;
}

// The goal of this process is to locate the max RMS value during
// the duration of each note.  When all the notes have been processed for
// a given channel the average maximum value for all of the notes is 
// then calculated.  
cmRC_t      cmAutoGainProcCh( cmAutoGain* p, const cmSample_t* x, unsigned xn )
{
  // shift the new samples into the shift buffer
  while(cmShiftBufExec(p->sbp,x,xn))
  {
    // update the gate detector
    cmGateDetectExec2(p->gdp,p->sbp->outV,p->sbp->outN);

    // write the output matrix file
    //if( _cmPuWriteMtxFile(p,segFl) != kOkPuRC )
    //  goto errLabel;

    // if this frame is an RMS minimum or onset or offset
    // then select it as a possible segment end.
    // Note that for onsets this will effectively force the end to
    // come after the onset because the onset will not be an energy minimum
    // relative to subsequent frames.
    if( p->gdp->rms < p->minRms || p->gdp->onFl || p->gdp->offFl )
    {
      p->minRms      = p->gdp->rms;

      // count onsets
      if( p->gdp->onFl )
        ++p->chp->onCnt;

      // count offsets
      if( p->gdp->offFl )
      {
        ++p->chp->offCnt;
          
        // update the gate sum and count
        p->gateSum += p->gateMax;
        p->gateCnt += 1;
        p->gateMax  = 0;
      }
    }

    // track the max RMS value during this gate
    if( p->gdp->gateFl && p->gdp->rms > p->gateMax )
      p->gateMax = p->gdp->rms;
      
  }

  return cmOkRC;
}

cmRC_t cmAutoGainCalcGains( cmAutoGain* p )
{
  unsigned i;
  cmReal_t avg = 0;

  if( p->chCnt == 0 )
    return cmOkRC;

  // verify that all channels were input
  for(i=0; i<p->chCnt; ++i)
    if( p->chArray[i].id == cmInvalidId )
      break;

  if( i != p->chCnt )
    return cmCtxRtCondition( &p->obj, cmArgAssertRC, "All channels must be set prior to calculating gains.");

  // process the last channel
  _cmAutoGainChannelFinish(p);
  
  // p->chp isn't used again unless we restart the indidual channel processing
  p->chp = NULL; 

 
  for(i=0; i<p->chCnt; ++i)
    avg += p->chArray[i].gateMaxAvg;

  avg /= p->chCnt;

  for(i=0; i<p->chCnt; ++i)
  {
    cmReal_t d = p->chArray[i].gateMaxAvg==0 ? 1.0 : p->chArray[i].gateMaxAvg;
    p->chArray[i].gain = avg / d;
  }

  return cmOkRC;
}

void  cmAutoGainPrint( cmAutoGain* p, cmRpt_t* rpt )
{
  unsigned i=0;
  for(i=0; i<p->chCnt; ++i)
  {
    cmAutoGainCh* chp = p->chArray + i;
    cmRptPrintf(rpt,"midi:%3i %4s on:%i off:%i avg:%5.5f gain:%5.5f\n", 
      chp->id, 
      cmStringNullGuard(cmMidiToSciPitch(chp->id,NULL,0)), 
      chp->onCnt, chp->offCnt, chp->gateMaxAvg, chp->gain );
  }
}

//==========================================================================================================================================

cmChCfg* cmChCfgAlloc( cmCtx* c, cmChCfg* p, cmCtx_t* ctx, const cmChar_t* fn )
{
  cmChCfg* op = cmObjAlloc(cmChCfg, c, p );

  if( fn != NULL )
    if( cmChCfgInit( op, ctx, fn ) != cmOkRC )
      cmChCfgFree(&op);
  return op;
}

cmRC_t   cmChCfgFree(  cmChCfg** pp )
{
  cmRC_t rc = cmOkRC;

  if( pp == NULL || *pp == NULL )
    return cmOkRC;
  
  cmChCfg* p = *pp;

  if((rc = cmChCfgFinal(p)) != cmOkRC )
    return rc;

  cmMemPtrFree(&p->chArray);
  cmFsFreeFn(p->fn);

  cmObjFree(pp);

  return cmOkRC;
}

enum { kChColIdx, kSsiColIdx, kPitchColIdx, kMidiColIdx, kGainColIdx, kNsFlColIdx, kCdFlColIdx, kColCnt };

cmRC_t   cmChCfgInit(  cmChCfg*  p, cmCtx_t* ctx, const cmChar_t* fn )
{
  cmRC_t          rc = cmOkRC;
  unsigned        i,j;
  const cmChar_t* ch_array_label = "ch_array";

  typedef struct 
  {
    unsigned    id;
    const char* label;
  } map_t;

  map_t map[ kColCnt ] = 
  {
    { kChColIdx,    "ch" },
    { kSsiColIdx,   "ssi" },
    { kPitchColIdx, "pitch" },
    { kMidiColIdx,  "midi" },
    { kGainColIdx,  "gain" },
    { kNsFlColIdx,  "ns_fl"},
    { kCdFlColIdx,  "cd_fl"}
  };


  if((rc = cmChCfgFinal(p)) != cmOkRC )
    return rc;

  p->fn = cmFsMakeFn(cmFsPrefsDir(),fn,NULL,NULL);
      
  // read the channel cfg JSON file
  if( cmJsonInitializeFromFile(&p->jsH, p->fn, ctx ) != kOkJsRC )
  {
    rc = cmCtxRtCondition( &p->obj, cmSubSysFailRC, "JSON initialize failed on '%s'.",cmStringNullGuard(p->fn));
    goto errLabel;
  }

  // locate the 'ch_array' node
  if((p->cap = cmJsonFindValue( p->jsH, ch_array_label, cmJsonRoot(p->jsH), kArrayTId )) == NULL )
  {
    rc = cmCtxRtCondition( &p->obj, cmSubSysFailRC, "Unable to locate the JSON element '%s'.",ch_array_label);
    goto errLabel;
  }

  unsigned rowCnt = cmJsonChildCount(p->cap);

  // there must be at least 1 label row and 1 ch row
  if( rowCnt < 2 )
  {
    rc = cmCtxRtCondition( &p->obj, cmSubSysFailRC, "The 'ch_array' appears to be empty.");
    goto errLabel;
  }

  // allocate the ch array
  p->chCnt   = rowCnt - 1;
  p->chArray = cmMemResizeZ( cmChCfgCh, p->chArray, p->chCnt );
  p->nsChCnt = 0;

  // read each row
  for(i=0; i<rowCnt; ++i)
  {
    // get the array on row i
    const cmJsonNode_t* np = cmJsonArrayElementC(p->cap,i);

    // all row arrays contain 'kColCnt' elements
    if( cmJsonChildCount(np) != kColCnt )
    {
      rc = cmCtxRtCondition( &p->obj, cmSubSysFailRC, "All 'ch_array' element at index %i does not contain %i values.",i,kColCnt);
      goto errLabel;    
    }

    // for each columns
    for(j=0; j<kColCnt; ++j)
    {
      const cmJsonNode_t* cp = cmJsonArrayElementC(np,j);

      // the first row contains the labels ....
      if( i == 0 )
      {
        // verify that the column format is as expected
        if( cmJsonIsString(cp)==false || strcmp(cp->u.stringVal,map[j].label) != 0 )
        {
          rc = cmCtxRtCondition( &p->obj, cmSubSysFailRC, "The first row column at index %i of 'ch_array' should be the label '%s'.",map[j].label);
          goto errLabel;
        }
      }
      else // ... other rows contain Ch Cfg's.
      {
        
        cmChCfgCh* chp = p->chArray + (i-1);

        switch(j)
        {
          case kChColIdx:    rc = cmJsonUIntValue(   cp, &chp->ch );       break;
          case kSsiColIdx:   rc = cmJsonUIntValue(   cp, &chp->ssi );      break;
          case kPitchColIdx: rc = cmJsonStringValue( cp, &chp->pitchStr ); break;
          case kMidiColIdx:  rc = cmJsonUIntValue(   cp, &chp->midi );     break;
          case kGainColIdx:  rc = cmJsonRealValue(   cp, &chp->gain );     break;
          case kNsFlColIdx:  
            rc = cmJsonBoolValue(   cp, &chp->nsFl );     
            if( chp->nsFl )
              ++p->nsChCnt;
            break;

          case kCdFlColIdx:  rc = cmJsonBoolValue(   cp, &chp->cdFl );     break;
          default:
            { assert(0); }
        }

        if( rc != kOkJsRC )
        {
          rc = cmCtxRtCondition( &p->obj, cmSubSysFailRC, "An error occurred while reading the '%s' column on row index:%i .", map[j].label,i);
          goto errLabel;
        }

      }
    }    
  }
    
 errLabel:

  return rc;
}

cmRC_t   cmChCfgFinal( cmChCfg*  p )
{ 
  cmRC_t rc = cmOkRC;

  if( cmJsonIsValid(p->jsH) )
    if( cmJsonFinalize(&p->jsH) != kOkJsRC )
      rc = cmCtxRtCondition( &p->obj, cmSubSysFailRC, "JSON finalize failed.");

  return rc;
}

cmRC_t   cmChCfgWrite( cmChCfg* p )
{
  cmRC_t rc = cmOkRC;
  unsigned i;
  
  for(i=0; i<p->chCnt; ++i)
  {
    // get the array on row i+1
    cmJsonNode_t* np = cmJsonArrayElement(p->cap,i+1);

    // get the ele in the 'gainColIdx' column
    np = cmJsonArrayElement(np,kGainColIdx);

    assert( cmJsonIsReal(np) );

    np->u.realVal = p->chArray[i].gain;

  }
  

  if( cmJsonWrite( p->jsH, cmJsonRoot(p->jsH), p->fn ) != kOkJsRC )
    rc = cmCtxRtCondition( &p->obj, cmSubSysFailRC, "The JSON Channel Cfg file write failed on '%s'.",cmStringNullGuard(p->fn));

  return rc;
}

void     cmChCfgPrint( cmChCfg* p, cmRpt_t* rpt )
{
  unsigned i;
  for(i=0; i<p->chCnt; ++i)
  {
    const cmChCfgCh* chp = p->chArray + i;
    cmRptPrintf(rpt,"%3i %3i %4s %3i %5.5f\n",chp->ch,chp->ssi,chp->pitchStr,chp->midi,chp->gain);
  }
}

unsigned cmChCfgChannelCount( cmCtx_t* ctx, const cmChar_t* fn, unsigned* nsChCntPtr )
{
  cmChCfg* ccp;
  unsigned chCnt = 0;
  cmCtx*   c =  cmCtxAlloc(NULL, &ctx->rpt, cmLHeapNullHandle, cmSymTblNullHandle );

  if((ccp = cmChCfgAlloc(c, NULL, ctx, fn )) == NULL )
    goto errLabel;

  chCnt = ccp->chCnt;

  if( nsChCntPtr != NULL )
    *nsChCntPtr = ccp->nsChCnt;

  cmChCfgFree(&ccp);

 errLabel:
  cmCtxFree(&c);
  return chCnt;
}

unsigned cmChCfgChannelIndex( cmCtx_t* ctx, const cmChar_t* fn, unsigned chIdx )
{
  cmChCfg* ccp;
  unsigned retChIdx = -1;
  cmCtx*   c =  cmCtxAlloc(NULL, &ctx->rpt, cmLHeapNullHandle, cmSymTblNullHandle );

  if((ccp = cmChCfgAlloc(c, NULL, ctx, fn )) == NULL )
    goto errLabel;

  if( chIdx >= ccp->chCnt )
    cmCtxRtCondition( &ccp->obj, cmArgAssertRC, "The channel index %i is not less than the channel count %i when querying '%s'.",chIdx,ccp->chCnt,cmStringNullGuard(ccp->fn));
  else
    retChIdx = ccp->chArray[chIdx].ch;
  


  cmChCfgFree(&ccp);

 errLabel:
  cmCtxFree(&c);
  return retChIdx;
}


//==========================================================================================================================================
cmChordDetect* cmChordDetectAlloc( cmCtx*c, cmChordDetect* p, cmReal_t srate, unsigned chCnt, cmReal_t maxTimeSpanMs, unsigned minNotesPerChord  )
{
  cmChordDetect* op = cmObjAlloc(cmChordDetect, c, p );

  if( srate != 0 )
    if( cmChordDetectInit( op, srate, chCnt, maxTimeSpanMs, minNotesPerChord ) != cmOkRC )
      cmChordDetectFree(&op);
  return op;
}

cmRC_t         cmChordDetectFree(  cmChordDetect** pp )
{
  cmRC_t rc = cmOkRC;

  if( pp == NULL || *pp == NULL )
    return cmOkRC;
  
  cmChordDetect* p = *pp;

  if((rc = cmChordDetectFinal(p)) != cmOkRC )
    return rc;

  cmMemPtrFree(&p->chArray);

  cmObjFree(pp);

  return cmOkRC;
}

cmRC_t         cmChordDetectInit(  cmChordDetect* p, cmReal_t srate, unsigned chCnt, cmReal_t maxTimeSpanMs, unsigned minNotesPerChord )
{
  cmRC_t rc;
  if((rc = cmChordDetectFinal(p)) != cmOkRC )
    return rc;

  p->chArray           = cmMemResizeZ(cmChordDetectCh, p->chArray, chCnt );
  p->chCnt             = chCnt;
  p->minNotesPerChord  = minNotesPerChord;
  p->detectFl          = false;
  p->timeSmp           = 0;
  p->srate             = srate;
  cmChordDetectSetSpanMs(p,maxTimeSpanMs);

  unsigned i;
  for(i=0; i<p->chCnt; ++i)
    p->chArray[i].readyFl = true;

  return rc;
}

cmRC_t cmChordDetectFinal( cmChordDetect* P )
{ return cmOkRC; }

cmRC_t cmChordDetectExec(  cmChordDetect* p, unsigned procSmpCnt, const bool* gateV, const cmReal_t* rmsV, unsigned chCnt )
{
  assert( chCnt == p->chCnt );

  chCnt   = cmMin(chCnt,p->chCnt);

  cmRC_t           rc      = cmOkRC;
  unsigned         candCnt = 0;  
  cmChordDetectCh* oldCand = NULL;
  unsigned         i;

  p->detectFl = false;
  p->timeSmp += procSmpCnt;

  // update the readyFl's and candFl's based on onset and offsets
  for(i=0; i<chCnt; ++i)
  {
    cmChordDetectCh* cp = p->chArray + i;    

    // if onset and the channel is ready
    if(  gateV[i] && cp->gateFl==false && cp->readyFl )
    {
      cp->candSmpTime = p->timeSmp;
      cp->candRMS     = rmsV[i];
      cp->candFl      = true;
    }

    // if offset then this channel is again ready to be a candidate
    if( gateV[i]==false && cp->gateFl )
    {
      cp->readyFl = true;
    }

    cp->gateFl  = gateV[i];
    cp->chordFl = false;
  }

  // check if we have enough channels to form a chord and
  // expire channels that have passed the max time out value
  for(i=0; i<chCnt; ++i)
  {
    cmChordDetectCh* cp        = p->chArray + i;
    unsigned         ageSmpCnt = p->timeSmp - cp->candSmpTime;

    // if this is a candiate ...
    if( cp->candFl )
    {
      // ... that has not expired 
      if( ageSmpCnt <= p->maxTimeSpanSmpCnt )
      {
        // track the oldest candidate
        if( oldCand==NULL || cp->candSmpTime < oldCand->candSmpTime )
          oldCand = cp;

        ++candCnt;
      }
      else // ... that has expired
      {
        cp->candFl = false;
      }
    }
  }


  // If we have enough notes and the oldest candiate will expire on the next cycle.
  // (By waiting for the oldest note to almost expire we gather the maximum number of
  // possible notes given the time duration.  If we remove this condition then
  // we will tend to trigger  chord detection with fewer notes.)
  if( oldCand != NULL )
  {
    unsigned nextAge = p->timeSmp + procSmpCnt - oldCand->candSmpTime;

    if( candCnt >= p->minNotesPerChord && nextAge >= p->maxTimeSpanSmpCnt )
    {
      for(i=0; i<chCnt; ++i)
      {
        cmChordDetectCh* cp = p->chArray + i;
        if( cp->candFl )
        {
          cp->chordFl     = true;
          cp->candFl      = false;
        }
      }

      p->detectFl = true;

    }
  }
  return rc;
}

cmRC_t         cmChordDetectSetSpanMs( cmChordDetect* p, cmReal_t maxTimeSpanMs )
{
  p->maxTimeSpanSmpCnt = floor( maxTimeSpanMs * p->srate / 1000.0 );
  return cmOkRC;
}


//==========================================================================================================================================
cmXfader* cmXfaderAlloc( cmCtx*c, cmXfader* p, cmReal_t srate, unsigned chCnt, cmReal_t fadeTimeMs  )
{
  cmXfader* op = cmObjAlloc(cmXfader, c, p );

  if( srate != 0 )
    if( cmXfaderInit( op, srate, chCnt, fadeTimeMs ) != cmOkRC )
      cmXfaderFree(&op);
  return op;
}

cmRC_t    cmXfaderFree(  cmXfader** pp )
{
  cmRC_t rc = cmOkRC;

  if( pp == NULL || *pp == NULL )
    return cmOkRC;
  
  cmXfader* p = *pp;

  if((rc = cmXfaderFinal(p)) != cmOkRC )
    return rc;

  cmMemPtrFree(&p->chArray);

  cmObjFree(pp);

  return cmOkRC;
}

cmRC_t    cmXfaderInit(  cmXfader* p, cmReal_t srate, unsigned chCnt, cmReal_t fadeTimeMs  )
{
  cmRC_t rc;
  if((rc = cmXfaderFinal(p)) != cmOkRC )
    return rc;

  p->chCnt      = chCnt;
  p->chArray    = cmMemResizeZ(cmXfaderCh,p->chArray,p->chCnt);
  p->srate      = srate;
  p->gateFl     = false;
  p->onFl       = false;
  p->offFl      = false;

  cmXfaderSetXfadeTime(p,fadeTimeMs);

  return rc;
}

cmRC_t    cmXfaderFinal(  cmXfader* p )
{ return cmOkRC; }

cmRC_t    cmXfaderExec(  cmXfader* p, unsigned procSmpCnt, const bool* chGateV, unsigned chCnt )
{
  assert( chCnt == p->chCnt);
  chCnt = cmMin(chCnt,p->chCnt);

  bool gateFl = false;

  // gain change associated with procSmpCnt
  cmReal_t dgain = (cmReal_t)procSmpCnt / p->fadeSmpCnt; 

  unsigned i;
  // for each channel
  for(i=0; i<chCnt; ++i)
  {
    cmXfaderCh* cp = p->chArray + i;

    if( chGateV != NULL )
      cp->gateFl = chGateV[i];

    if( cp->gateFl )
      cp->gain = cmMin(cp->gain + dgain,1.0); 
    else
      cp->gain = cmMax(cp->gain - dgain,0.0); 

    if( cp->gain != 0.0 )
      gateFl = true;
  }

  p->onFl  = false;
  p->offFl = false;

  if( p->gateFl==false && gateFl==true )
    p->onFl = true;
  else
    if( p->gateFl==true && gateFl==false )
      p->offFl = true;

  p->gateFl = gateFl;

  return cmOkRC;
}

cmRC_t    cmXfaderExecAudio( cmXfader* p, unsigned procSmpCnt, const bool* gateV, unsigned chCnt, const cmSample_t* x[], cmSample_t* y )
{
  cmRC_t rc;

  assert( chCnt == p->chCnt);
  chCnt = cmMin(chCnt,p->chCnt);

  if((rc = cmXfaderExec(p,procSmpCnt,gateV,chCnt)) != cmOkRC )
    return rc;
  
  unsigned i;
  for(i=0; i<chCnt; ++i)
    if( x[i] != NULL )
      cmVOS_MultVaVS(y,procSmpCnt,x[i],p->chArray[i].gain);

  return rc;  
}

void      cmXfaderSetXfadeTime( cmXfader* p, cmReal_t fadeTimeMs )
{
   p->fadeSmpCnt = floor(fadeTimeMs * p->srate /1000.0); 
}

void      cmXfaderSelectOne( cmXfader* p, unsigned chIdx )
{
  unsigned i = 0;
  for(i=0; i<p->chCnt; ++i)
    p->chArray[i].gateFl = i == chIdx;
}

void      cmXfaderAllOff( cmXfader* p )
{
  unsigned i = 0;
  for(i=0; i<p->chCnt; ++i)
    p->chArray[i].gateFl = false;
}

//==========================================================================================================================================
cmFader*  cmFaderAlloc( cmCtx*c, cmFader* p, cmReal_t srate, cmReal_t fadeTimeMs  )
{
  cmFader* op = cmObjAlloc(cmFader, c, p );

  if( srate != 0 )
    if( cmFaderInit( op, srate, fadeTimeMs ) != cmOkRC )
      cmFaderFree(&op);
  return op;  
}

cmRC_t    cmFaderFree(  cmFader** pp )
{
  cmRC_t rc = cmOkRC;

  if( pp == NULL || *pp == NULL )
    return cmOkRC;
  
  cmFader* p = *pp;

  if((rc = cmFaderFinal(p)) != cmOkRC )
    return rc;

  cmObjFree(pp);

  return cmOkRC;
}

cmRC_t    cmFaderInit(  cmFader* p, cmReal_t srate, cmReal_t fadeTimeMs  )
{
  cmRC_t rc;
  if((rc = cmFaderFinal(p)) != cmOkRC )
    return rc;

  p->srate  = srate;
  p->gain   = 0.0;
  cmFaderSetFadeTime(p,fadeTimeMs);

  return rc;
}

cmRC_t    cmFaderFinal(  cmFader* p )
{ return cmOkRC; }

cmRC_t    cmFaderExec(  cmFader* p, unsigned procSmpCnt, bool gateFl, bool mixFl, const cmSample_t* x, cmSample_t* y )
{
  cmReal_t d = (gateFl ? 1.0 : -1.0) * procSmpCnt / p->fadeSmpCnt;

  // TODO: add fade curves
  
  p->gain = cmMin(1.0,cmMax(0.0,p->gain + d));

  if( x!=NULL && y!=NULL )
  {
    if( mixFl )
      cmVOS_MultVaVS(y, procSmpCnt, x, 1.0/* p->gain*/);
    else
      cmVOS_MultVVS(y,procSmpCnt,x,p->gain);
  }

  return cmOkRC;
}

void      cmFaderSetFadeTime( cmFader* p, cmReal_t fadeTimeMs )
{
  p->fadeSmpCnt = (unsigned)floor( fadeTimeMs * p->srate / 1000.0 );
}

  //=======================================================================================================================
cmCombFilt* cmCombFiltAlloc( cmCtx* c, cmCombFilt* p, cmReal_t srate, bool feedbackFl, cmReal_t minHz, cmReal_t alpha, cmReal_t hz, bool bypassFl )
{
  cmCombFilt* op = cmObjAlloc(cmCombFilt, c, p );
 
  op->idp = cmIDelayAlloc(c, NULL, 0, 0, NULL, NULL, NULL, 0 );

  if( srate != 0 )
    if( cmCombFiltInit( op, srate, feedbackFl, minHz, alpha, hz, bypassFl ) != cmOkRC )
      cmCombFiltFree(&op);

  return op;  
}

cmRC_t      cmCombFiltFree(  cmCombFilt** pp)
{
  cmRC_t rc = cmOkRC;

  if( pp == NULL || *pp == NULL )
    return cmOkRC;
  
  cmCombFilt* p = *pp;

  if((rc = cmCombFiltFinal(p)) != cmOkRC )
    return rc;

  cmMemFree(p->a);
  cmMemFree(p->b);
  cmMemFree(p->d);

  cmIDelayFree(&p->idp);

  cmObjFree(pp);

  return cmOkRC;

}

cmRC_t      cmCombFiltInit(  cmCombFilt* p, cmReal_t srate, bool feedbackFl, cmReal_t minHz, cmReal_t alpha, cmReal_t hz, bool bypassFl )
{
  cmRC_t rc;
  if((rc = cmCombFiltFinal(p)) != cmOkRC )
    return rc;

  p->feedbackFl = feedbackFl;

  p->dN      = cmMax(1,floor(srate / minHz ));
  p->d       = cmMemResizeZ( cmReal_t, p->d, p->dN+1 );
  p->a       = cmMemResizeZ( cmReal_t, p->a, p->dN );
  p->b       = cmMemResizeZ( cmReal_t, p->b, p->dN );

  p->dn       = 1;
  p->alpha    = alpha;
  p->srate    = srate;
  p->bypassFl = bypassFl;

  cmReal_t tapMs = p->dn * 1000.0 / srate;
  cmReal_t tapFf = 1.0;
  cmReal_t tapFb = 1.0;
  cmIDelayInit(p->idp, srate, p->dN * 1000.0 / srate, &tapMs, &tapFf, &tapFb, 1 );

  cmCombFiltSetHz(p,hz);

  return rc;
}

cmRC_t      cmCombFiltFinal( cmCombFilt* p )
{ return cmOkRC; }

cmRC_t      cmCombFiltExec(  cmCombFilt* p, const cmSample_t* x, cmSample_t* y, unsigned n )
{
  if( y == NULL )
    return cmOkRC;

  if( x == NULL )
  {
    cmVOS_Zero(y,n);
    return cmOkRC;
  }

  if( p->bypassFl )
    cmVOS_Copy(y,n,x);
  else
    cmIDelayExec(p->idp,x,y,n);

  return cmOkRC;
}

void        cmCombFiltSetAlpha( cmCombFilt* p, cmReal_t alpha )
{
  p->b0         = 1.0 - fabs(alpha); // normalization coeff (could be applied directly to the input or output)
  p->a[p->dn-1] = -alpha; 
  p->alpha         = alpha;

  p->idp->tff[0] = p->b0;
  p->idp->tfb[0] = -alpha;

}

cmRC_t        cmCombFiltSetHz( cmCombFilt* p, cmReal_t hz )
{
  if( hz < p->minHz )
    return cmCtxRtCondition( &p->obj, cmInvalidArgRC, "The comb filter frequency %f Hz is invalid given the minimum frequency setting of %f Hz.",hz,p->minHz );

  // clear the current alpha
  p->a[p->dn-1] = 0;        
                   
  // change location of alpha in p->a[]
  p->dn         = cmMin(p->dN-1,cmMax(1,floor(p->srate / hz )));

  p->hz            = hz;

  cmCombFiltSetAlpha(p,p->alpha);  // the location of filter coeff changed - reset it here

  cmIDelaySetTapMs(p->idp, 0,p->dn * 1000.0 / p->srate );

  return cmOkRC;
}

//=======================================================================================================================
cmDcFilt* cmDcFiltAlloc( cmCtx* c, cmDcFilt* p, bool bypassFl )
{
  cmDcFilt* op = cmObjAlloc(cmDcFilt, c, p );

  if( cmDcFiltInit( op, bypassFl ) != cmOkRC )
    cmDcFiltFree(&op);
  return op;  
}

cmRC_t      cmDcFiltFree(  cmDcFilt** pp)
{
  cmRC_t rc = cmOkRC;

  if( pp == NULL || *pp == NULL )
    return cmOkRC;
  
  cmDcFilt* p = *pp;

  if((rc = cmDcFiltFinal(p)) != cmOkRC )
    return rc;

  cmObjFree(pp);

  return cmOkRC;

}

cmRC_t      cmDcFiltInit(  cmDcFilt* p, bool bypassFl )
{
  cmRC_t rc;
  if((rc = cmDcFiltFinal(p)) != cmOkRC )
    return rc;
  
  p->b0 = 1;
  p->b[0] = -1;
  p->a[0] = -0.999;
  p->d[0] = 0;
  p->d[1] = 0;
  p->bypassFl = bypassFl;
  return rc;
}

cmRC_t      cmDcFiltFinal( cmDcFilt* p )
{ return cmOkRC; }

cmRC_t      cmDcFiltExec(  cmDcFilt* p, const cmSample_t* x, cmSample_t* y, unsigned n )
{
  if( p->bypassFl )
    cmVOS_Copy(y,n,x);
  else
    cmVOS_Filter(y,n,x,n,p->b0, p->b, p->a,  p->d, 1 );

  return cmOkRC;
}

//=======================================================================================================================
cmIDelay* cmIDelayAlloc( cmCtx* c, cmIDelay* p, cmReal_t srate, cmReal_t maxDelayMs, const cmReal_t* tapMs, const cmReal_t* tapFfGain, const cmReal_t* tapFbGain, unsigned tapCnt )
{
  cmIDelay* op = cmObjAlloc(cmIDelay, c, p );

  if( srate != 0 )
    if( cmIDelayInit( op, srate, maxDelayMs, tapMs, tapFfGain, tapFbGain, tapCnt ) != cmOkRC )
      cmIDelayFree(&op);
  return op;  
}

cmRC_t    cmIDelayFree( cmIDelay** pp )
{
  cmRC_t rc = cmOkRC;

  if( pp == NULL || *pp == NULL )
    return cmOkRC;
  
  cmIDelay* p = *pp;

  if((rc = cmIDelayFinal(p)) != cmOkRC )
    return rc;

  cmMemFree(p->m);
  cmMemFree(p->ti);
  cmMemFree(p->tff);
  cmMemFree(p->tfb);
  cmObjFree(pp);

  return cmOkRC;
}

cmRC_t    cmIDelayInit( cmIDelay* p, cmReal_t srate, cmReal_t maxDelayMs, const cmReal_t* tapMs, const cmReal_t* tapFfGain, const cmReal_t* tapFbGain, unsigned tapCnt )
{
  cmRC_t rc;
  if(( rc = cmIDelayFinal(p)) != cmOkRC )
    return rc;

  p->dn = (unsigned)floor( maxDelayMs * srate / 1000.0 );
  p->mn = p->dn + 3;
  p->m  = cmMemResizeZ(cmSample_t,p->m,p->mn);
  p->d  = p->m + 1;
  p->tn = tapCnt;
  p->ti = cmMemResize(cmReal_t,p->ti,tapCnt);
  p->tff = cmMemResize(cmReal_t,p->tff,tapCnt);
  p->tfb = cmMemResize(cmReal_t,p->tfb,tapCnt);
  p->srate = srate;

  unsigned i;
  for(i=0; i<tapCnt; ++i)
  {
    cmIDelaySetTapMs(p,i,tapMs[i]);
    p->tff[i] = tapFfGain[i];
    p->tfb[i] = tapFbGain[i];
  }

  return rc;
}

cmRC_t    cmIDelayFinal(cmIDelay* p )
{ return cmOkRC; }

cmRC_t    cmIDelayExec( cmIDelay* p, const cmSample_t* x, cmSample_t* y, unsigned n )
{
  unsigned i,j;
  for(i=0; i<n; ++i)
  {
    cmSample_t fb = 0;
    y[i] = 0;


    // calculate the output sample
    for(j=0; j<p->tn; ++j)
    {
      // get tap_j
      cmReal_t tfi = p->ii - p->ti[j];

      // mod it into 0:dn-1
      while( tfi < 0 )
        tfi += p->dn;

      int        ti = floor(tfi);
      cmReal_t   tf = tfi - ti;
      cmSample_t v  = _lin_interp(tf,p->d[ti],p->d[ti+1]); // _cube_interp(tf,p->d[ti-1],p->d[ti],p->d[ti+1],p->d[ti+2]);

      y[i] += p->tff[j] * v;
      fb   += p->tfb[j] * v;
    }

    // insert incoming sample
    p->d[p->ii] = x[i] + fb;

    // advance the delay line
    ++p->ii;
    
    // handle beg/end delay sample duplication
    // -1 0 1 2 .... dn-1 dn dn+1 
    //  a b c          a   b   c
    // The samples at position -1 0 1 must be duplicated
    // at positions dn-1,dn,dn+1 so that output sample calculation
    // does not need to deal with buffer wrap-around issues.

    if( p->ii == 1 )
      p->d[p->dn] = x[i];
    else
      if( p->ii == 2 )
        p->d[p->dn+1] = x[i];
      else
        if( p->ii == p->dn )
        {
          p->d[-1] = x[i];
          p->ii = 0;
        }
  }

  return cmOkRC;
}

cmRC_t    cmIDelaySetTapMs( cmIDelay* p, unsigned tapIdx, cmReal_t tapMs )
{
  cmRC_t rc = cmOkRC;

  if( tapIdx < p->tn )
    p->ti[tapIdx] = tapMs * p->srate / 1000.0;
  else
    rc =  cmCtxRtCondition( &p->obj, cmInvalidArgRC, "Tap index %i is out of range 0 - %i.",tapIdx,p->tn-1 );

  return rc;
}


//=======================================================================================================================
  
cmGroupSel* cmGroupSelAlloc( cmCtx* c, cmGroupSel* p, unsigned chCnt, unsigned groupCnt, unsigned chsPerGroup )
{
  cmGroupSel* op = cmObjAlloc(cmGroupSel, c, p );

  if( chCnt != 0 )
    if( cmGroupSelInit( op, chCnt, groupCnt, chsPerGroup ) != cmOkRC )
      cmGroupSelFree(&op);
  return op;  

}

cmRC_t      cmGroupSelFree( cmGroupSel** pp )
{
  cmRC_t rc = cmOkRC;

  if( pp == NULL || *pp == NULL )
    return cmOkRC;
  
  cmGroupSel* p = *pp;

  if((rc = cmGroupSelFinal(p)) != cmOkRC )
    return rc;

  cmMemFree(p->groupArray);
  cmMemFree(p->chArray);
  cmObjFree(pp);

  return cmOkRC;

}

cmRC_t      cmGroupSelInit( cmGroupSel* p, unsigned chCnt, unsigned groupCnt, unsigned chsPerGroup )
{
  cmRC_t rc;
  if((rc = cmGroupSelFinal(p)) != cmOkRC )
    return rc;

  p->chArray     = cmMemResizeZ(cmGroupSelCh,p->chArray,chCnt);
  p->groupArray  = cmMemResizeZ(cmGroupSelGrp,p->groupArray,chCnt);
  p->chCnt       = chCnt;
  p->groupCnt    = groupCnt;
  p->chsPerGroup = chsPerGroup;
  
  unsigned i;
  for(i=0; i<p->chCnt; ++i)
    p->chArray[i].groupIdx = cmInvalidIdx;

  for(i=0; i<p->groupCnt; ++i)
  {
    p->groupArray[i].chIdxArray = cmMemAllocZ(unsigned, p->chCnt);
    p->groupArray[i].chIdxCnt   = 0;
  }

  return rc;
}

cmRC_t      cmGroupSelFinal( cmGroupSel* p )
{
  unsigned i;

  for(i=0; i<p->groupCnt; ++i)
    cmMemFree(p->groupArray[i].chIdxArray);

  return cmOkRC; 
}

cmRC_t      cmGroupSetChannelGate( cmGroupSel* p, unsigned chIdx, bool gateFl )
{
  assert(chIdx <p->chCnt);
  cmGroupSelCh* cp = p->chArray + chIdx;
  
  if( gateFl==true && cp->gateFl==false )
  {
    cp->readyFl = true;
  }

  if( gateFl==false && cp->gateFl==true )
  {
    cp->offsetFl = true;
    cp->readyFl  = false;
  }


  cp->gateFl = gateFl;

  //printf("ch:%i gate:%i ready:%i offset:%i grp:%i\n",chIdx,cp->gateFl,cp->readyFl,cp->offsetFl,cp->groupIdx);

  return cmOkRC;
}

cmRC_t      cmGroupSetChannelRMS( cmGroupSel* p, unsigned chIdx, cmReal_t rms )
{
  assert(chIdx <p->chCnt);
  cmGroupSelCh* cp = p->chArray + chIdx;
  cp->rms    = rms;
  return cmOkRC;
}
  
cmRC_t      cmGroupSelExec( cmGroupSel* p )
{
  unsigned i,j;
  unsigned availGroupCnt = 0;

  p->chsPerGroup = cmMin( p->chsPerGroup, p->chCnt );
  
  p->updateFl = false;

  // clear the create and release flags on each group 
  // and get the count of available groups
  for(i=0; i<p->groupCnt; ++i)  
  {
    cmGroupSelGrp* gp = p->groupArray + i;

    gp->createFl = false;

    if( gp->releaseFl )
    {
      // clear the groupIdx from the channels assigned to this released group
      for(j=0; j<gp->chIdxCnt; ++j)       
        p->chArray[ gp->chIdxArray[j] ].groupIdx = cmInvalidIdx;

      gp->releaseFl = false;
      gp->chIdxCnt  = 0;
    }

    if( gp->chIdxCnt == 0 )
      ++availGroupCnt;
  }

  // count the number of ready but unassigned ch's and 
  // release any groups which contain a channel with a gate offset
  unsigned readyChCnt = 0;

  for(i=0; i<p->chCnt; ++i)
  {
    cmGroupSelCh* cp = p->chArray + i;

    // count the number of channels that are ready to be assigned
    if( cp->readyFl )
      ++readyChCnt;

    // if this channel offset and it had been assigned to a group
    // then mark its group for release
    if( cp->offsetFl && cp->groupIdx != cmInvalidIdx )
    {
      p->groupArray[cp->groupIdx].releaseFl = true;

      p->updateFl = true;
    }
  }


  // assign ready ch's to available groups
  while( readyChCnt > p->chsPerGroup && availGroupCnt > 0  )
  {
    unsigned groupIdx;

    // locate an available group
    for(groupIdx=0; groupIdx<p->groupCnt; ++groupIdx)
      if( p->groupArray[groupIdx].chIdxCnt == 0 )
        break;

    if( groupIdx == p->groupCnt )
      break;

    --availGroupCnt;

    cmGroupSelGrp* gp = p->groupArray + groupIdx;

    printf("gs - group:%i chs: ",groupIdx);
    
    // assign chsPerGroup ch's to this group
    for(i=0; i<p->chCnt && gp->chIdxCnt < p->chsPerGroup; ++i)
      if(p->chArray[i].readyFl )
      {
        p->chArray[i].readyFl           = false;    // this ch is no longer ready because it is assigned
        p->chArray[i].groupIdx          = groupIdx; // set this ch's group idx
        gp->chIdxArray[ gp->chIdxCnt ]  = i;        // assign channel to group 
        gp->chIdxCnt                   += 1;        // update the group ch count
        gp->createFl                    = true;
        p->updateFl                     = true;
        --readyChCnt;
        printf("%i ",i);
      }

    printf("\n");
      
  }
  
  return cmOkRC;
}

  //=======================================================================================================================

cmAudioNofM* cmAudioNofMAlloc( cmCtx* c, cmAudioNofM* p, cmReal_t srate, unsigned inChCnt, unsigned outChCnt, cmReal_t fadeTimeMs )
{
  cmAudioNofM* op = cmObjAlloc(cmAudioNofM, c, p );

  if( srate != 0 )
    if( cmAudioNofMInit( op, srate, inChCnt, outChCnt, fadeTimeMs ) != cmOkRC )
      cmAudioNofMFree(&op);
  return op;  
}

cmRC_t  cmAudioNofMFree( cmAudioNofM** pp )
{
  cmRC_t rc = cmOkRC;

  if( pp == NULL || *pp == NULL )
    return cmOkRC;
  
  cmAudioNofM* p = *pp;

  if((rc = cmAudioNofMFinal(p)) != cmOkRC )
    return rc;

  cmMemFree(p->outArray);
  cmMemFree(p->inArray);
  cmObjFree(pp);

  return cmOkRC;
}

cmRC_t cmAudioNofMInit( cmAudioNofM* p, cmReal_t srate, unsigned iChCnt, unsigned oChCnt, cmReal_t fadeTimeMs )
{
  cmRC_t rc;
  if((rc = cmAudioNofMFinal(p)) != cmOkRC )
    return rc;

  p->iChCnt = iChCnt;
  p->inArray = cmMemResizeZ(cmAudioNofM_In,p->inArray,iChCnt);
  p->oChCnt = oChCnt;
  p->outArray = cmMemResizeZ(cmAudioNofM_Out,p->outArray,oChCnt);
  p->nxtOutChIdx = 0;

  unsigned i;
  for(i=0; i<p->iChCnt; ++i)
  {
    p->inArray[i].fader = cmFaderAlloc(p->obj.ctx, NULL, srate, fadeTimeMs );
    p->inArray[i].outChIdx = cmInvalidIdx;
  }

  cmAudioNofMSetFadeMs( p, fadeTimeMs );

  return rc;
}

cmRC_t cmAudioNofMFinal( cmAudioNofM* p )
{ 
  unsigned i;
  for(i=0; i<p->iChCnt; ++i)
    cmFaderFree(&p->inArray[i].fader);
  return cmOkRC;
}

cmRC_t cmAudioNofMSetChannelGate( cmAudioNofM* p, unsigned inChIdx, bool gateFl )
{
  assert( inChIdx < p->iChCnt );

  cmAudioNofM_In* ip = p->inArray + inChIdx;

  if( ip->gateFl && gateFl == false )
    ip->offsetFl = true;

  if( ip->gateFl == false && gateFl )
    ip->onsetFl = true;

  ip->gateFl = gateFl;

  printf("nom: %p ch:%i gate:%i on:%i off:%i\n",p,inChIdx,ip->gateFl,ip->offsetFl,ip->onsetFl);

  return cmOkRC;
}

cmRC_t cmAudioNofMExec( cmAudioNofM* p, const cmSample_t* x[], unsigned inChCnt, cmSample_t* y[], unsigned outChCnt, unsigned n )
{
  assert( inChCnt == p->iChCnt && outChCnt == p->oChCnt );

  unsigned i;

  // for each output channel
  for(i=0; i<p->oChCnt; ++i)
  {
    cmAudioNofM_Out* op = p->outArray + i;
    cmAudioNofM_In*  ip = op->list;
    cmAudioNofM_In*  pp = NULL;

    // for each input assigned to this output chanenl
    while( ip != NULL )
    {
      // if the channel is no longer active
      if( ip->offsetFl && ip->fader->gain == 0.0 )
      {
        printf("nom: off - out:%i\n",ip->outChIdx);

        // remove it from the output channels list
        ip->offsetFl = false;
        ip->outChIdx = cmInvalidIdx;
        
        if( pp == NULL )
          op->list = ip->link;
        else
          pp->link = ip->link;
        
        
      }

      pp = ip;
      ip = ip->link;
    }

  }

  // for each input channel
  for(i=0; i<inChCnt; ++i)
  {
    cmAudioNofM_In* ip = p->inArray + i;

    // if this channel is starting 
    if( ip->onsetFl == true && ip->offsetFl == false )
    {
      // assign it to the next output channel
      ip->onsetFl = false;
      ip->outChIdx = p->nxtOutChIdx;

      ip->link = p->outArray[ ip->outChIdx ].list;
      p->outArray[ ip->outChIdx ].list = ip;
      
      p->nxtOutChIdx = (p->nxtOutChIdx + 1) % p->oChCnt;

      printf("nom: on - in:%i out:%i\n",i,ip->outChIdx);
    }

    // if this channel is active - then mix its input 
    if( ip->outChIdx != cmInvalidIdx )
    {
      cmFaderExec( ip->fader, n, ip->gateFl, true, x[i], y[ ip->outChIdx ] );
      
    }
  }
  return cmOkRC;
}

cmRC_t cmAudioNofMSetFadeMs( cmAudioNofM* p, cmReal_t fadeTimeMs )
{
  unsigned i;
  for(i=0; i<p->iChCnt; ++i)
    cmFaderSetFadeTime(p->inArray[i].fader,fadeTimeMs);
  return cmOkRC;
}


//=======================================================================================================================
cmAdsr*   cmAdsrAlloc( cmCtx* c, cmAdsr* p, cmReal_t srate, bool trigFl, cmReal_t minL, cmReal_t dlyMs, cmReal_t atkMs, cmReal_t atkL, cmReal_t dcyMs, cmReal_t susMs, cmReal_t susL, cmReal_t rlsMs )
{
  cmAdsr* op = cmObjAlloc(cmAdsr, c, p );
    
  if( srate != 0 )
    if( cmAdsrInit( op, srate, trigFl, minL, dlyMs, atkMs, atkL, dcyMs, susMs, susL, rlsMs ) != cmOkRC )
      cmAdsrFree(&op);
  return op;  

}

cmRC_t    cmAdsrFree(  cmAdsr** pp )
{
  cmRC_t rc = cmOkRC;

  if( pp == NULL || *pp == NULL )
    return cmOkRC;
  
  cmAdsr* p = *pp;

  if((rc = cmAdsrFinal(p)) != cmOkRC )
    return rc;

  cmObjFree(pp);

  return cmOkRC;

}

cmRC_t    cmAdsrInit( cmAdsr* p, cmReal_t srate, bool trigFl, cmReal_t minL, cmReal_t dlyMs, cmReal_t atkMs, cmReal_t atkL, cmReal_t dcyMs, cmReal_t susMs, cmReal_t susL, cmReal_t rlsMs )
{
  cmRC_t rc;
  if((rc = cmAdsrFinal(p)) != cmOkRC )
    return rc;

  // this is a limitation of the design - the design should be replaced with one
  // which increments/decrements the level until it reaches a limit instead of calculating
  // durations
  //assert(atkL>=0 && minL>=0);

  p->srate      = srate;
  p->trigModeFl = trigFl;
  p->levelMin   = minL;
  p->scaleDur   = 1.0;

  cmAdsrSetTime(p,dlyMs,kDlyAdsrId);
  cmAdsrSetTime(p,atkMs,kAtkAdsrId);
  cmAdsrSetTime(p,dcyMs,kDcyAdsrId);
  cmAdsrSetTime(p,susMs,kSusAdsrId);
  cmAdsrSetTime(p,rlsMs,kRlsAdsrId);

  cmAdsrSetLevel(p,atkL,kAtkAdsrId);
  cmAdsrSetLevel(p,susL,kSusAdsrId);

  p->state    = kDoneAdsrId;
  p->durSmp   = 0;
  p->level    = p->levelMin;
  p->gateFl   = false;

  p->atkDurSmp = 0;
  p->rlsDurSmp = 0;


  return cmOkRC;
}

cmRC_t    cmAdsrFinal( cmAdsr* p )
{ return cmOkRC; }

cmReal_t  cmAdsrExec( cmAdsr* p, unsigned procSmpCnt, bool gateFl, cmReal_t tscale, cmReal_t ascale )
{
  double scaleAmp = ascale;
  double scaleDur = tscale;

  // if onset
  if( p->gateFl == false && gateFl==true )
  {
    p->scaleDur = scaleDur==0 ? 1.0 : fabs(scaleDur);


    //printf("sd:%f %f\n",scaleDur,scaleAmp);

    switch( p->state )
    {
      case kDlyAdsrId: 
        // if in delay mode when the re-attack occurs don't do anything
        break;

      case kAtkAdsrId:
      case kDcyAdsrId: 
      case kSusAdsrId:
      case kRlsAdsrId:
        // if the atk level == 0 then fall through to kDoneAdsrId
        if( p->actAtkLevel != 0 )
        {
          // re-attak mode:
          // Scale the attack time to the current level relative to the attack level.
          // In general this will result in a decrease in the attack duration.
          p->atkDurSmp   = cmMax(1,floor(p->atkSmp * (p->actAtkLevel - p->level) / p->actAtkLevel));
          p->atkBegLevel = p->level;
          p->durSmp      = 0;
          p->state       = kAtkAdsrId;
          p->actAtkLevel = p->atkLevel * scaleAmp;
          p->actSusLevel = p->susLevel * scaleAmp;
          break;
        }

      case kDoneAdsrId:
        p->atkBegLevel = p->levelMin;
        p->atkDurSmp   = p->atkSmp;
        p->state       = p->dlySmp == 0 ? kAtkAdsrId : kDlyAdsrId;
        p->durSmp      = 0;
        p->actAtkLevel = p->atkLevel * scaleAmp;
        p->actSusLevel = p->susLevel * scaleAmp;
        break;

      default:
        { assert(0); }
    }
  }

  // if an offset occurred and we are not in trigger mode - then go into release mode
  if( p->trigModeFl==false && p->gateFl == true && gateFl == false )
  {
    switch( p->state )
    {
      case kDlyAdsrId: 
      case kAtkAdsrId:
      case kDcyAdsrId:
      case kSusAdsrId:
        if( p->actSusLevel == 0 )
          p->state = kDoneAdsrId;
        else
        {
          // scale the release time to the current level relative to the sustain level
          p->rlsDurSmp =  cmMax(1,floor(p->rlsSmp * p->level / p->actSusLevel));
          p->rlsLevel  = p->level;        
          p->durSmp    = 0;
        }
        break;
        
      case kRlsAdsrId:
      case kDoneAdsrId:
        // nothing to do
        break;

      default:
        { assert(0); }
    }

    p->state = kRlsAdsrId;

  }

  p->gateFl = gateFl;



  switch( p->state )
  {
    case kDlyAdsrId:
      p->level = p->levelMin;

      if( p->durSmp >= p->dlySmp )
      {
        p->state  = kAtkAdsrId;
        p->durSmp = 0;
        
        if( p->trigModeFl )
        {
          p->atkBegLevel  = p->level;
          p->atkDurSmp = p->atkSmp;
        }
      }
      break;

    case kAtkAdsrId:
      if( p->atkDurSmp != 0 )
        p->level = p->atkBegLevel + (p->actAtkLevel - p->atkBegLevel) * cmMin(p->durSmp,p->atkDurSmp) / p->atkDurSmp;

      if( p->durSmp >= p->atkDurSmp || p->atkDurSmp == 0 )
      {
        p->state  = kDcyAdsrId;
        p->durSmp = 0;
      }
      break;

    case kDcyAdsrId:
      if( p->dcySmp != 0 )
        p->level = p->actAtkLevel - ((p->actAtkLevel - p->actSusLevel) * cmMin(p->durSmp,p->dcySmp) / p->dcySmp );

      if( p->durSmp >= p->dcySmp || p->dcySmp==0 )
      {
        p->state = kSusAdsrId;
        p->durSmp = 0;
      }
      break;

    case kSusAdsrId:
      p->level = p->actSusLevel;

      if( p->trigModeFl==true && p->durSmp >= p->susSmp )
      {
        p->state     = kRlsAdsrId;
        p->durSmp    = 0;
        p->rlsLevel  = p->level;
        p->rlsDurSmp = p->rlsSmp;
      }
      break;

    case kRlsAdsrId:
      if( p->rlsDurSmp != 0 )
        p->level = p->rlsLevel - ((p->rlsLevel - p->levelMin) * cmMin(p->durSmp,p->rlsDurSmp) / p->rlsDurSmp);

      if( p->durSmp >= p->rlsDurSmp || p->rlsDurSmp==0 )
      {
        p->state = kDoneAdsrId;
        p->durSmp = 0;
      }

      break;

    case kDoneAdsrId:
      p->level = p->levelMin;
      break;

    default:
      { assert(0); }
  }
  
  p->durSmp += floor(procSmpCnt/p->scaleDur);

  return p->level;
}

void _cmAdsrSetTime( cmAdsr* p, cmReal_t ms, int* smpPtr )
{
  *smpPtr = cmMax(1,floor(p->srate * ms / 1000.0 ));
}

void  cmAdsrSetTime( cmAdsr* p, cmReal_t ms, unsigned id )
{
  switch( id )
  {
    case kDlyAdsrId:
      _cmAdsrSetTime(p,ms,&p->dlySmp);
      break;

    case kAtkAdsrId:
      _cmAdsrSetTime(p,ms,&p->atkSmp);
      break;

    case kDcyAdsrId:
      _cmAdsrSetTime(p,ms,&p->dcySmp);
      break;

    case kSusAdsrId:
      _cmAdsrSetTime(p,ms,&p->susSmp);
      break;

    case kRlsAdsrId:
      _cmAdsrSetTime(p,ms,&p->rlsSmp);
      break;

    default:
      { assert(0); }
  }
}

void  cmAdsrSetLevel( cmAdsr* p, cmReal_t level, unsigned id )
{
  switch( id )
  {
    case kDlyAdsrId:
      p->levelMin = level;
      break;

    case kAtkAdsrId:
      p->atkLevel = level;
      p->actAtkLevel = p->atkLevel;
      break;

    case kSusAdsrId:
      p->susLevel = level;
      p->actSusLevel = p->susLevel;
      break;

    default:
      { assert(0); }
  }
}

void      cmAdsrReport( cmAdsr* p, cmRpt_t* rpt )
{
  cmRptPrintf(rpt,"state:%i gate:%i phs:%i d:%i a:%i (%i) d:%i s:%i r:%i (%i) - min:%f atk:%f\n",p->state,p->gateFl,p->durSmp,p->dlySmp,p->atkSmp,p->atkDurSmp,p->dcySmp,p->susSmp,p->rlsSmp,p->rlsDurSmp, p->levelMin, p->atkLevel);
}

//=======================================================================================================================

cmCompressor* cmCompressorAlloc( cmCtx* c, cmCompressor* p, cmReal_t srate, unsigned procSmpCnt, cmReal_t inGain, cmReal_t rmsWndMaxMs, cmReal_t rmsWndMs, cmReal_t threshDb, cmReal_t ratio_num, cmReal_t atkMs, cmReal_t rlsMs, cmReal_t outGain, bool bypassFl )
{
  cmCompressor* op = cmObjAlloc(cmCompressor, c, p );
    
  if( srate != 0 )
    if( cmCompressorInit( op, srate, procSmpCnt, inGain, rmsWndMaxMs, rmsWndMs, threshDb, ratio_num, atkMs, rlsMs, outGain, bypassFl  ) != cmOkRC )
      cmCompressorFree(&op);
  return op;  
}

cmRC_t  cmCompressorFree( cmCompressor** pp )
{
  cmRC_t rc = cmOkRC;

  if( pp == NULL || *pp == NULL )
    return cmOkRC;
  
  cmCompressor* p = *pp;

  if((rc = cmCompressorFinal(p)) != cmOkRC )
    return rc;

  cmMemFree(p->rmsWnd);
  cmObjFree(pp);

  return cmOkRC;

}


  
cmRC_t  cmCompressorInit( cmCompressor* p, cmReal_t srate, unsigned procSmpCnt, cmReal_t inGain, cmReal_t rmsWndMaxMs, cmReal_t rmsWndMs, cmReal_t threshDb, cmReal_t ratio_num, cmReal_t atkMs, cmReal_t rlsMs, cmReal_t outGain, bool bypassFl )
{
  cmRC_t rc;
  if((rc = cmCompressorFinal(p)) != cmOkRC )
    return rc;

  p->srate      = srate;
  p->procSmpCnt = procSmpCnt;
  p->threshDb   = threshDb;
  p->ratio_num  = ratio_num;

  cmCompressorSetAttackMs(p,atkMs);
  cmCompressorSetReleaseMs(p,rlsMs);

  p->inGain     = inGain;
  p->outGain    = outGain;
  p->bypassFl   = bypassFl;

  p->rmsWndAllocCnt = cmMax(1,(unsigned)floor(rmsWndMaxMs * srate / (1000.0 * procSmpCnt)));
  p->rmsWnd      = cmMemResizeZ(cmSample_t,p->rmsWnd,p->rmsWndAllocCnt);
  cmCompressorSetRmsWndMs(p, rmsWndMs );
  p->rmsWndIdx = 0; 

  p->state       = kRlsCompId;
  p->timeConstDb = 10.0;
  p->accumDb     = p->threshDb;

  return rc;
}

cmRC_t  cmCompressorFinal( cmCompressor* p )
{ return cmOkRC; }

/*
  The ratio determines to what degree a signal above the threshold is reduced.
  Given a 2:1 ratio, a signal 2dB above the threshold will be reduced to 1db above the threshold.
  Given a 4:1 ratio, a signal 2dB above the threshold will be reduced to 0.25db above the threshold.
  Gain_reduction_db = (thresh - signal) / ratio_numerator  (difference between the threshold and signal level after reduction)
  Gain Coeff = 10^(gain_reduction_db / 20);
 
  Total_reduction_db = signal - threshold + Gain_reduc_db
  (total change in signal level)

  The attack can be viewed as beginning at the threshold and moving to the peak
  over some period of time. In linear terms this will go from 1.0 to the max gain
  reductions. In this case we step from thresh to peak at a fixed rate in dB
  based on the attack time.

  Db:        thresh - [thesh:peak] / ratio_num
  Linear: pow(10, (thresh - [thesh:peak] / ratio_num)/20 );

  During attacks p->accumDb increments toward the p->pkDb.
  During release p->accumDb decrements toward the threshold.
  
  (thresh - accumDb) / ratio_num gives the signal level which will be achieved
  if this value is converted to linear form and applied as a gain coeff.

  See compressor.m
 */

cmRC_t  cmCompressorExec( cmCompressor* p, const cmSample_t* x, cmSample_t* y, unsigned n )
{
  cmSample_t xx[n];

  cmVOS_MultVVS(xx,n,x,p->inGain); // apply input gain

  p->rmsWnd[ p->rmsWndIdx ] = cmVOS_RMS(xx, n, n ); // calc and store signal RMS
  p->rmsWndIdx = (p->rmsWndIdx + 1) % p->rmsWndCnt; // advance the RMS storage buffer

  cmReal_t rmsLin = cmVOS_Sum(p->rmsWnd,p->rmsWndCnt) / p->rmsWndCnt; // calc avg RMS
  cmReal_t rmsDb  = cmMax(-100.0,20 * log10(cmMax(0.00001,rmsLin)));  // convert avg RMS to dB
  rmsDb += 100.0;

  // if the compressor is bypassed
  if( p->bypassFl )
  {
    cmVOS_Copy(y,n,x); // copy through - with no input gain
    return cmOkRC;
  }

  // if the signal is above the threshold
  if( rmsDb <= p->threshDb )
    p->state = kRlsCompId;
  else
  {
    if( rmsDb > p->pkDb )  
      p->pkDb   = rmsDb;
    
    p->state  = kAtkCompId;    
  }

  switch( p->state )
  {
    case kAtkCompId:                      
      p->accumDb = cmMin(p->pkDb, p->accumDb + p->timeConstDb * n / p->atkSmp );
      break;

    case kRlsCompId:
      p->accumDb = cmMax(p->threshDb, p->accumDb - p->timeConstDb * n / p->rlsSmp );
      break;
  }

  p->gain = pow(10.0,(p->threshDb - p->accumDb) / (p->ratio_num * 20.0));
  
  cmVOS_MultVVS(y,n,xx,p->gain * p->outGain);
  
  return cmOkRC;
}

void    _cmCompressorSetMs( cmCompressor* p, cmReal_t ms, unsigned* smpPtr )
{ *smpPtr = cmMax(1,(unsigned)floor(ms * p->srate / 1000.0)); }

void    cmCompressorSetAttackMs( cmCompressor* p, cmReal_t ms )
{ _cmCompressorSetMs(p,ms,&p->atkSmp); }

void    cmCompressorSetReleaseMs( cmCompressor* p, cmReal_t ms )
{ _cmCompressorSetMs(p,ms,&p->rlsSmp); }

void    cmCompressorSetThreshDb(  cmCompressor* p, cmReal_t threshDb )
{  p->threshDb = cmMax(0.0,100 + threshDb); }

void    cmCompressorSetRmsWndMs( cmCompressor* p, cmReal_t ms )
{
  p->rmsWndCnt = cmMax(1,(unsigned)floor(ms * p->srate / (1000.0 * p->procSmpCnt)));

  // do not allow rmsWndCnt to exceed rmsWndAllocCnt
  if( p->rmsWndCnt > p->rmsWndAllocCnt )
    p->rmsWndCnt = p->rmsWndAllocCnt;
}

  //=======================================================================================================================

cmBiQuadEq* cmBiQuadEqAlloc( cmCtx* c, cmBiQuadEq* p, cmReal_t srate, unsigned mode, cmReal_t f0Hz, cmReal_t Q, cmReal_t gainDb, bool bypassFl )
{
  cmBiQuadEq* op = cmObjAlloc(cmBiQuadEq, c, p );
    
  if( srate != 0 )
    if( cmBiQuadEqInit( op, srate, mode, f0Hz, Q, gainDb, bypassFl  ) != cmOkRC )
      cmBiQuadEqFree(&op);
  return op;  
}

cmRC_t      cmBiQuadEqFree( cmBiQuadEq** pp )
{
  cmRC_t rc = cmOkRC;

  if( pp == NULL || *pp == NULL )
    return cmOkRC;
  
  cmBiQuadEq* p = *pp;

  if((rc = cmBiQuadEqFinal(p)) != cmOkRC )
    return rc;

  cmObjFree(pp);

  return cmOkRC;
}

void  _cmBiQuadEqInit( cmBiQuadEq* p )
{
  cmReal_t w0     = 2*M_PI*p->f0Hz/p->srate;
  cmReal_t cos_w0 = cos(w0);
  cmReal_t alpha  = sin(w0)/(2*p->Q);

  if( p->mode==kLowShelfBqId || p->mode==kHighShelfBqId )
  {
    cmReal_t c = p->mode==kLowShelfBqId ? 1.0 : -1.0;
    cmReal_t A = pow(10.0,p->gainDb/40.0);
    cmReal_t B = 2*sqrt(A)*alpha;

    p->b[0] =      A*( (A+1) - c * (A-1)*cos_w0 + B );
    p->b[1] =  c*2*A*( (A-1) - c * (A+1)*cos_w0);
    p->b[2] =      A*( (A+1) - c * (A-1)*cos_w0 - B );
    p->a[0] =          (A+1) + c * (A-1)*cos_w0 + B;
    p->a[1] =   -c*2*( (A-1) + c * (A+1)*cos_w0);
    p->a[2] =          (A+1) + c * (A-1)*cos_w0 - B;
  }
  else
  {
    if( p->mode != kPeakBqId )
    {
      p->a[0] =   1 + alpha;
      p->a[1] =  -2*cos_w0;
      p->a[2] =   1 - alpha;
    }

    switch(p->mode)
    {
      case kLpfBqId:
      case kHpFBqId:
        {
          cmReal_t c = p->mode==kLpfBqId ? 1.0 : -1.0;
          p->b[0] =      (1 - c * cos_w0)/2;
          p->b[1] =  c * (1 - c * cos_w0);
          p->b[2] =      (1 - c * cos_w0)/2;
        }
        break;

      case kBpfBqId:
        p->b[0] =   p->Q*alpha;
        p->b[1] =   0;
        p->b[2] =  -p->Q*alpha;
        break;

      case kNotchBqId:
        p->b[0] =   1;
        p->b[1] =  -2*cos_w0;
        p->b[2] =   1;
        break;

      case kAllpassBqId:
        p->b[0] =   1 - alpha;
        p->b[1] =  -2*cos_w0;
        p->b[2] =   1 + alpha;
        break;

      case kPeakBqId:
        {
          cmReal_t A = pow(10.0,p->gainDb/40.0);
          p->b[0] =   1 + alpha*A;
          p->b[1] =  -2*cos_w0;
          p->b[2] =   1 - alpha*A;
          p->a[0] =   1 + alpha/A;
          p->a[1] =  -2*cos_w0;
          p->a[2] =   1 - alpha/A;
        }
        break;
    
    }

  }

  cmReal_t a0 = p->a[0];
  unsigned i=0;
  for(; i<3; ++i)
  {
    p->b[i]/=a0;
    p->a[i]/=a0;
  }

  if(0)
  {
    printf("sr:%f mode:%i f0:%f q:%f gain:%f\n", p->srate,p->mode,p->f0Hz,p->Q,p->gainDb);
  
    for(i=0; i<3; ++i)
      printf("a[%i]=%f b[%i]=%f  ",i,p->a[i],i,p->b[i]);
    printf("\n");
  }
  
}

cmRC_t      cmBiQuadEqInit( cmBiQuadEq* p, cmReal_t srate, unsigned mode, cmReal_t f0Hz, cmReal_t Q, cmReal_t gainDb, bool bypassFl )  
{
  cmRC_t rc;
  if((rc = cmBiQuadEqFinal(p)) != cmOkRC )
    return rc;

  memset(p->d,0,sizeof(p->d));

  p->srate = srate;
  cmBiQuadEqSet(p, mode, f0Hz, Q, gainDb );

  _cmBiQuadEqInit(p);

  return rc;
}

cmRC_t      cmBiQuadEqFinal( cmBiQuadEq* p )
{ return cmOkRC;}

cmRC_t      cmBiQuadEqExec( cmBiQuadEq* p, const cmSample_t* x, cmSample_t* y, unsigned n )
{
  if( y == NULL )
    return cmOkRC;

  if( x == NULL )
  {
    cmVOS_Zero(y,n);
    return cmOkRC;
  }

  if( p->bypassFl )
  {
    cmVOS_Copy(y,n,x);
    return cmOkRC;
  }

  // Direct Form I implementation
  unsigned i=0;
  for(; i<n; ++i)
  {
    y[i] =  p->b[0]*x[i] + p->b[1]*p->d[0] + p->b[2]*p->d[1] - p->a[1]*p->d[2] - p->a[2]*p->d[3];
    p->d[1] = p->d[0];
    p->d[0] = x[i];
    p->d[3] = p->d[2];
    p->d[2] = y[i];
  }

  return cmOkRC;
}


void        cmBiQuadEqSet( cmBiQuadEq* p, unsigned mode, cmReal_t f0Hz, cmReal_t Q, cmReal_t gainDb )
{
  p->mode  = mode;
  p->f0Hz  = f0Hz;
  p->Q     = Q;
  p->gainDb= gainDb;
  _cmBiQuadEqInit(p);
}

//=======================================================================================================================

cmDistDs* cmDistDsAlloc( cmCtx* c, cmDistDs* p, cmReal_t srate, cmReal_t inGain, cmReal_t downSrate, cmReal_t bits, bool rectFl, bool fullFl, cmReal_t clipDb, cmReal_t outGain, bool bypassFl )
{
  cmDistDs* op = cmObjAlloc(cmDistDs, c, p );
    
  if( srate != 0 )
    if( cmDistDsInit( op, srate, inGain, downSrate, bits, rectFl, fullFl, clipDb, outGain, bypassFl  ) != cmOkRC )
      cmDistDsFree(&op);
  return op;  

}

cmRC_t    cmDistDsFree( cmDistDs** pp )
{
  cmRC_t rc = cmOkRC;

  if( pp == NULL || *pp == NULL )
    return cmOkRC;
  
  cmDistDs* p = *pp;

  if((rc = cmDistDsFinal(p)) != cmOkRC )
    return rc;

  cmObjFree(pp);

  return cmOkRC;
}

cmRC_t    cmDistDsInit( cmDistDs* p, cmReal_t srate, cmReal_t inGain, cmReal_t downSrate, cmReal_t bits, bool rectFl, bool fullFl, cmReal_t clipDb, cmReal_t outGain, bool bypassFl )
{
  cmRC_t rc;
  if((rc = cmDistDsFinal(p)) != cmOkRC )
    return rc;

  p->srate     = srate;
  p->downSrate = downSrate;
  p->bits      = bits;
  p->rectFl    = rectFl;
  p->fullFl    = fullFl;
  p->clipDb    = clipDb;
  p->inGain    = inGain;
  p->outGain   = outGain;
  p->bypassFl  = bypassFl;
  p->fracIdx   = 0;
  p->lastVal   = 0;
  p->lastY     = 0;
  p->lastX     = 0;
  return rc;
}

cmRC_t    cmDistDsFinal( cmDistDs* p )
{ return cmOkRC; }

void _cmDistDsExpr0( cmDistDs* p, const cmSample_t* x, cmSample_t* y, unsigned n )
{
  unsigned i= 0;
  for(i=0; i<n; ++i)
  {
    p->lastY = fmod(p->lastY + fabs(x[i] - p->lastX),2.0);
    y[i] = cmVOS_RMS(x,n,n) * x[i] * (p->lastY - 1.0);
    p->lastX = x[i];
  }
}

void _cmDistDsExpr1( cmDistDs* p, cmSample_t* x, cmSample_t* y, unsigned n )
{
  unsigned i= 0;
  for(i=0; i<n; ++i)
  {
    p->lastY = fmod(p->lastY + fabs(x[i] - p->lastX),2.0*M_PI);
    y[i] = x[i] * sin(p->lastY);
    p->lastX = x[i];
  }
}



cmRC_t    cmDistDsExec(  cmDistDs* p, const cmSample_t* x, cmSample_t* y, unsigned n )
{
  if( x == NULL )
  {
    if( y != NULL )
      cmVOS_Zero(y,n);
    return cmOkRC;
  }

  if( p->bypassFl )
  {
    cmVOS_Copy(y,n,x);
    return cmOkRC;
  }


  unsigned maxVal   = cmMax(2,(unsigned)round(pow(2.0,p->bits)));
  double   incr     = 1.0; // p->downSrate / p->srate;
  unsigned i;
  enum { kNoRectId, kFullRectId, kHalfRectId };
  unsigned rectCode = kNoRectId;

  //cmSample_t t[n];
  //_cmDistDsExpr0(p,x,t,n);
  //x = t;

  if( p->rectFl )
  {

    if( p->fullFl )
      rectCode = kFullRectId;
    else
      rectCode = kHalfRectId;
  }

  double clipLevel = p->clipDb < -100.0 ? 0.0 : pow(10.0,p->clipDb/20.0);

  for(i=0; i<n; ++i)
  {
    double ii = floor(p->fracIdx);

    p->fracIdx += incr;

    // if it is time to sample again
    if( floor(p->fracIdx) != ii )
    {
      cmSample_t v = p->inGain * floor(x[i] * maxVal) / maxVal;

      switch( rectCode )
      {
        case kFullRectId:
          v = (cmSample_t)fabs(v);
          break;

        case kHalfRectId:
          if( v < 0.0 )
            v = 0.0;
          break;
      }

      if( fabs(v) > clipLevel )
        v = v<0.0 ? -clipLevel : clipLevel;

      p->lastVal = v * p->outGain;
    }

    y[i] = p->lastVal;
  }
  
  return cmOkRC;
}
