#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmFloatTypes.h"
#include "cmComplexTypes.h"
#include "cmFileSys.h"
#include "cmJson.h"
#include "cmSymTbl.h"
#include "cmAudioFile.h"
#include "cmText.h"
#include "cmProcObj.h"
#include "cmProcTemplate.h"
#include "cmMath.h"
#include "cmFile.h"
#include "cmTime.h"
#include "cmMidi.h"
#include "cmProc.h"
#include "cmProc2.h"
#include "cmProc5.h"

#include "cmVectOps.h"


//=======================================================================================================================
cmGoertzel* cmGoertzelAlloc( cmCtx* c, cmGoertzel* p, double srate, const double* fcHzV, unsigned chCnt, unsigned procSmpCnt, unsigned hopSmpCnt, unsigned wndSmpCnt )
{
  cmGoertzel* op = cmObjAlloc(cmGoertzel,c,p);
  
  op->shb = cmShiftBufAlloc(c,NULL,0,0,0);

  if( srate > 0 )  
    if( cmGoertzelInit(op,srate,fcHzV,chCnt,procSmpCnt,wndSmpCnt,hopSmpCnt) != cmOkRC )
      cmGoertzelFree(&op);

  return op;
}

cmRC_t cmGoertzelFree( cmGoertzel** pp )
{
  cmRC_t rc = cmOkRC;
  if( pp==NULL || *pp==NULL )
    return rc;

  cmGoertzel* p = *pp;
  if((rc = cmGoertzelFinal(p)) != cmOkRC )
    return rc;

  cmShiftBufFree(&p->shb);
  cmMemFree(p->ch);
  cmMemFree(p->wnd);
  cmObjFree(pp);
  return rc;

}

cmRC_t cmGoertzelInit( cmGoertzel* p, double srate, const double* fcHzV, unsigned chCnt, unsigned procSmpCnt, unsigned hopSmpCnt, unsigned wndSmpCnt )
{
  cmRC_t rc;
  unsigned i;

  if((rc = cmGoertzelFinal(p)) != cmOkRC )
    return rc;

  p->ch    = cmMemResizeZ(cmGoertzelCh,p->ch,chCnt);
  p->chCnt = chCnt;
  p->srate = srate;
  p->wnd   = cmMemResizeZ(cmSample_t,p->wnd,wndSmpCnt);

  cmVOS_Hann(p->wnd,wndSmpCnt);

  cmShiftBufInit(p->shb,procSmpCnt,wndSmpCnt,hopSmpCnt);

  for(i=0; i<p->chCnt; ++i)
  {
    cmGoertzelSetFcHz(p,i,fcHzV[i]);
  }

  return rc;
}

cmRC_t cmGoertzelFinal( cmGoertzel* p )
{ return cmOkRC; }

cmRC_t cmGoertzelSetFcHz( cmGoertzel* p, unsigned chIdx, double hz )
{
  assert( chIdx < p->chCnt );
  p->ch[chIdx].hz   = hz;
  p->ch[chIdx].coeff = 2*cos(2*M_PI*hz/p->srate);
  
  return cmOkRC;
}

cmRC_t cmGoertzelExec( cmGoertzel* p, const cmSample_t* inpV, unsigned procSmpCnt, double* outV, unsigned chCnt )
{
  unsigned i,j;

  while( cmShiftBufExec(p->shb,inpV,procSmpCnt) )
  {
    unsigned   xn = p->shb->wndSmpCnt;
    cmSample_t x[ xn ];

    cmVOS_MultVVV(x,xn,p->wnd,p->shb->outV);

    for(i=0; i<chCnt; ++i)
    {
      cmGoertzelCh* ch = p->ch + i;
    
      ch->s2 = x[0];
      ch->s1 = x[1] + 2 * x[0] * ch->coeff;
      for(j=2; j<xn; ++j)
      {
        ch->s0 = x[j] + ch->coeff * ch->s1 - ch->s2;
        ch->s2 = ch->s1;
        ch->s1 = ch->s0;
      }
    
      outV[i] = ch->s2*ch->s2 + ch->s1*ch->s1 - ch->coeff * ch->s2 * ch->s1;
    }
  }

  return cmOkRC;
}


//=======================================================================================================================
double _cmGoldSigSinc( double t, double T )
{
  double x = t/T;
  return x == 0 ? 1.0 : sin(M_PI*x)/(M_PI*x);         
}

void _cmGoldSigRaisedCos(  cmSample_t* yV, int yN, double sPc, double beta )
{
  int i;
  
  for(i=0; i<yN; ++i)
  {
    double t   = i - yN/2;
    double den = 1 - (4*(beta*beta)*(t*t) / (sPc*sPc));
    double a;
      
    if(fabs(den) < 0.00001 )
      a = 1;
    else
      a = cos(M_PI * beta * t/ sPc ) / den;

    yV[i] = _cmGoldSigSinc(t,sPc) * a;  
  }
}

void _cmGoldSigConv( cmGoldSig_t* p, unsigned chIdx )
{
  int i;
  int sPc = p->a.samplesPerChip;
  int osf = p->a.rcosOSFact;

  // for each bit in the spreading-code
  for(i=0; i<p->mlsN; ++i)
  {
    int j = (i*sPc) + sPc/2;  // index into bbV[] of center of impulse response
    int k = j - (sPc*osf)/2;  // index into bbV[] of start of impulse response
    int h;

    // for each sample in the impulse response
    for(h=0; h<p->rcosN; ++h,++k)
    {
     
      while( k<0 )
        k += p->sigN;

      while( k>=p->sigN )
        k -= p->sigN;

      p->ch[chIdx].bbV[k] += p->ch[chIdx].pnV[i] * p->rcosV[h];     
    }
  }
}

void _cmGoldSigModulate( cmGoldSig_t* p, unsigned chIdx )
{
  unsigned    i;
  double      rps = 2.0 * M_PI * p->a.carrierHz / p->a.srate;
  cmSample_t* yV  = p->ch[chIdx].mdV;
  cmSample_t* bbV = p->ch[chIdx].bbV;

  for(i=0; i<p->sigN; ++i)
    yV[ i ] = bbV[i]*cos(rps*i) + bbV[i]*sin(rps*i);

  // apply a half Hann envelope to the onset/offset of the id signal
  if( p->a.envMs > 0 )
  {
    unsigned   wndMs = p->a.envMs * 2;
    unsigned   wndN  = wndMs * p->a.srate / 1000;
    wndN += wndN % 2 ? 0 : 1;   // force the window length to be odd
    unsigned   wNo2 = wndN/2 + 1;
    cmSample_t wndV[ wndN ];
    cmVOS_Hann(wndV,wndN);
    cmVOS_MultVV(yV,wNo2,wndV);
    cmVOS_MultVV(yV + p->sigN - wNo2, wNo2, wndV + wNo2 - 1);
  }

}

cmGoldSig_t* cmGoldSigAlloc( cmCtx* ctx, cmGoldSig_t* p, const cmGoldSigArg_t* a )
{
  cmGoldSig_t* op = cmObjAlloc(cmGoldSig_t,ctx,p);
  
  if( a != NULL )  
    if( cmGoldSigInit(op,a) != cmOkRC )
      cmGoldSigFree(&op);

  return op;

}

cmRC_t cmGoldSigFree( cmGoldSig_t** pp )
{
  cmRC_t rc = cmOkRC;

  if( pp == NULL || *pp == NULL )
    return rc;

  cmGoldSig_t* p = *pp;

  if((rc = cmGoldSigFinal(p)) != cmOkRC )
    return rc;
  
  unsigned i;
  for(i=0; i<p->a.chN; ++i)
  {
    cmMemFree(p->ch[i].bbV);
    cmMemFree(p->ch[i].mdV);
  }
  
  cmMemFree(p->ch);
  cmMemFree(p->rcosV);
  cmMemFree(p->pnM);
  cmMemFree(p);
  *pp = NULL;

  return rc;
}

cmRC_t cmGoldSigInit( cmGoldSig_t* p, const cmGoldSigArg_t* a )
{
  cmRC_t      rc = cmOkRC;
  unsigned    i;
  
  p->a      = *a;                                            // store arg recd
  p->ch     = cmMemResizeZ(cmGoldSigCh_t,p->ch,a->chN);  // alloc channel array
  p->mlsN   = (1 << a->lfsrN) - 1;                           // calc spreading code length
  p->rcosN  = a->samplesPerChip * a->rcosOSFact;             // calc rcos imp. resp. length
  p->rcosN += (p->rcosN % 2)==0;                             // force rcos imp. length odd               
  p->rcosV  = cmMemResizeZ(cmSample_t,p->rcosV,p->rcosN);  // alloc rcos imp. resp. vector
  p->pnM    = cmMemResizeZ(int,p->pnM,p->mlsN*a->chN);   // alloc spreading-code mtx
  p->sigN   = p->mlsN * a->samplesPerChip;                   // calc audio signal length

  // generate spreading codes
  if( cmGenGoldCodes(a->lfsrN, a->mlsCoeff0, a->mlsCoeff1, a->chN, p->pnM, p->mlsN ) == false )
  {
    rc = cmCtxRtCondition(&p->obj,cmSubSysFailRC,"Unable to generate sufficient balanced Gold codes.");
    goto errLabel;
  }

  // generate the rcos impulse response
  _cmGoldSigRaisedCos(p->rcosV,p->rcosN,a->samplesPerChip,a->rcosBeta);

  // for each channel
  for(i=0; i<a->chN; ++i)
  {
    // Note: if (i*p->mlsN) is set to 0 in the following line then all channels
    // will use the same spreading code.
    p->ch[i].pnV = p->pnM + (i*p->mlsN);                        // get ch. spreading code
    p->ch[i].bbV = cmMemResizeZ(cmSample_t,p->ch[i].bbV,p->sigN); // alloc baseband signal vector
    p->ch[i].mdV = cmMemResizeZ(cmSample_t,p->ch[i].mdV,p->sigN); // alloc output audio vector

    // Convolve spreading code with rcos impulse reponse to form baseband signal.
    _cmGoldSigConv(p, i );     

    // Modulate baseband signal to carrier frq. and apply attack/decay envelope.
    _cmGoldSigModulate(p, i );
  }

 errLabel:
  if((rc = cmErrLastRC(&p->obj.err)) != cmOkRC )
    cmGoldSigFree(&p);

  return rc;
}

cmRC_t cmGoldSigFinal( cmGoldSig_t* p )
{ return cmOkRC; }

cmRC_t cmGoldSigWrite( cmCtx* ctx, cmGoldSig_t* p, const char* fn )
{
  cmVectArray_t* vap = NULL;
  unsigned       i;

  vap = cmVectArrayAlloc(ctx,kSampleVaFl);

  for(i=0; i<p->a.chN; ++i)
  {
    cmVectArrayAppendS(vap,p->ch[i].bbV,p->sigN);
    cmVectArrayAppendS(vap,p->ch[i].mdV,p->sigN);
  }

  cmVectArrayWrite(vap,fn);

  cmVectArrayFree(&vap);

  return cmOkRC;
}


cmRC_t cmGoldSigGen( cmGoldSig_t* p, unsigned chIdx, unsigned prefixN, unsigned dsN, unsigned *bsiV, unsigned bsiN, double noiseGain, cmSample_t** yVRef, unsigned* yNRef )
{
  unsigned    yN = prefixN + bsiN * (p->sigN + dsN);
  cmSample_t* yV = cmMemAllocZ(cmSample_t,yN);
  unsigned    i;

  cmVOS_Random(yV, yN, -noiseGain, noiseGain );

  for(i=0; i<bsiN; ++i)
  {
    bsiV[i] = prefixN + i*(p->sigN + dsN);
    
    cmVOS_AddVV(yV + bsiV[i], p->sigN, p->ch[chIdx].mdV );
  }
  
  if( yVRef != NULL )
    *yVRef = yV;

  if( yNRef != NULL )
    *yNRef = yN;

  return cmOkRC;  
}



