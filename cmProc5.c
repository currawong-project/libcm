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

  p->fir = cmFIRAllocKaiser(ctx, NULL, 0, 0, 0, 0, 0, 0, 0 );
  
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

  cmFIRFree(&p->fir);
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

  if(1)
  {
    double   passHz = 20000.0;
    double   stopHz = 17000.0;
    double   passDb = 1.0;
    double   stopDb = 90.0;
    unsigned flags  = 0;
    
    if( cmFIRInitKaiser(p->fir, 64, a->srate, passHz, stopHz, passDb, stopDb, flags ) != cmOkRC )
    {
      rc = cmCtxRtCondition(&p->obj,cmSubSysFailRC,"Unable to allocate internal FIR.");
      goto errLabel;
    }

    p->rcosN = p->fir->coeffCnt;
    p->rcosV = cmMemResizeZ(cmSample_t,p->rcosV,p->rcosN);
    cmVOS_CopyD(p->rcosV,p->rcosN,p->fir->coeffV);
    
  }
  

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
{ return cmFIRFinal(p->fir); }

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


//=======================================================================================================================
cmPhat_t*   cmPhatAlloc(  cmCtx* ctx, cmPhat_t* ap, unsigned chN, unsigned hN, float alpha, unsigned mult, unsigned flags )
{
  cmPhat_t* p = cmObjAlloc(cmPhat_t,ctx,ap);

  // The FFT buffer and the delay line is at least twice the size of the 
  // id signal. This will guarantee that at least one complete id signal
  // is inside the buffer.  In practice it means that it is possible
  // that there will be two id's in the buffer therefore if there are
  // two correlation spikes it is important that we take the second.
  unsigned fhN = cmNextPowerOfTwo(mult*hN);

  // allocate the FFT object
  cmFftAllocSR(ctx,&p->fft,NULL,fhN,kToPolarFftFl);
  cmIFftAllocRS(ctx,&p->ifft,fhN/2 + 1 );
  
  if( chN != 0 )  
    if( cmPhatInit(p,chN,hN,alpha,mult,flags) != cmOkRC )
      cmPhatFree(&p);

  return p;

}

cmRC_t   cmPhatFree(   cmPhat_t** pp )
{
  cmRC_t rc = cmOkRC;

  if( pp == NULL || *pp == NULL )
    return rc;

  cmPhat_t* p = *pp;
  if((rc = cmPhatFinal(p)) != cmOkRC )
    return rc;

  cmMemFree(p->t0V);
  cmMemFree(p->t1V);
  cmMemFree(p->dV);
  cmMemFree(p->xV);
  cmMemFree(p->fhM);
  cmMemFree(p->mhM);
  cmMemFree(p->wndV);
  cmObjFreeStatic(cmFftFreeSR, cmFftSR, p->fft);
  cmObjFreeStatic(cmIFftFreeRS, cmIFftRS, p->ifft);
  cmVectArrayFree(&p->ftVa);
  cmObjFree(pp);

  return rc;

}


cmRC_t   cmPhatInit(  cmPhat_t* p, unsigned chN, unsigned hN, float alpha, unsigned mult, unsigned flags )
{
  cmRC_t   rc = cmOkRC;
  if((rc = cmPhatFinal(cmOkRC)) != cmOkRC )
    return rc;

  p->fhN = cmNextPowerOfTwo(mult*hN);
  
  if((cmFftInitSR(&p->fft, NULL, p->fhN, kToPolarFftFl)) != cmOkRC )
    return rc;

  if((cmIFftInitRS(&p->ifft, p->fft.binCnt )) != cmOkRC )
    return rc;

  p->alpha = alpha;
  p->flags = flags;

  // allocate the delay line
  p->dV = cmMemResizeZ(cmSample_t,p->dV,p->fhN);
  p->di = 0;

  // allocate the linear buffer
  p->xV  = cmMemResizeZ(cmSample_t,p->xV,p->fhN);
  p->t0V = cmMemResizeZ(cmComplexR_t,p->t0V,p->fhN);
  p->t1V = cmMemResizeZ(cmComplexR_t,p->t1V,p->fhN);

  // allocate the window function
  p->wndV = cmMemResizeZ(cmSample_t,p->wndV,p->fhN);
  cmVOS_Hann(p->wndV,p->fhN);

  // allocate the signal id matrix
  p->chN = chN;
  p->hN  = hN;
  p->binN = p->fft.binCnt; //atFftRealBinCount(p->fftH);
  p->fhM = cmMemResizeZ(cmComplexR_t, p->fhM, p->fhN  * chN);
  p->mhM = cmMemResizeZ(float,        p->mhM, p->binN * chN);
  cmPhatReset(p);

  //if( cmIsFlag(p->flags,kDebugAtPhatFl)) 
  //  cmVectArrayAlloc(ctx, &p->ftVa,  kSampleVaFl );
  //else
  //  p->ftVa = NULL;

  return rc;

}

cmRC_t cmPhatFinal( cmPhat_t* p )
{ return cmOkRC; }

cmRC_t cmPhatReset(  cmPhat_t* p )
{
  p->di     = 0;
  p->absIdx = 0;
  cmVOS_Zero(p->dV,p->fhN);
  return cmOkRC;
}

cmRC_t cmPhatSetId(  cmPhat_t* p, unsigned chIdx, const cmSample_t* hV, unsigned hN )
{
  unsigned i;
  assert( chIdx < p->chN );
  assert( hN == p->hN );

  // Allocate a window vector
  cmSample_t* wndV = cmMemAllocZ(cmSample_t,hN);
  cmVOS_Hann(wndV,hN);

  // get ptr to output column in p->fhM[].
  cmComplexR_t* yV = p->fhM + (chIdx*p->fhN);

  // Zero pad hV[hN] to p->fhN;
  assert( hN <= p->fhN );
  cmVOS_Zero(p->xV,p->fhN);
  cmVOS_Copy(p->xV,hN,hV);

  // Apply the window function to the id signal
  if(cmIsFlag(p->flags,kHannAtPhatFl) )
    cmVOS_MultVVV(p->xV,hN,hV,wndV);

  // take FFT of id signal. The result is in fft->complexV and fft->magV,phsV
  cmFftExecSR(&p->fft, p->xV, p->fhN );

  // Store the magnitude of the id signal
  //atFftComplexAbs(p->mhM + (chIdx*p->binN), yV,     p->binN);
  cmVOF_CopyR(p->mhM + (chIdx*p->binN), p->binN, p->fft.magV );

  // Scale the magnitude
  cmVOS_MultVS(   p->mhM + (chIdx*p->binN), p->binN, p->alpha);

  // store the complex conjugate of the FFT result in yV[]
  //atFftComplexConj(yV,p->binN);
  for(i=0; i<p->binN; ++i)
    yV[i] = cmCconjR(p->fft.complexV[i]);

  cmMemFree(wndV);

  return cmOkRC;
}

cmSample_t* _cmPhatReadVector( cmCtx* ctx, cmPhat_t* p, const char* fn, unsigned* vnRef )
{
  cmVectArray_t* vap = NULL;
  cmSample_t*    v   = NULL;
  cmRC_t         rc  = cmOkRC;
  
  // instantiate a VectArray from a file
  if( (vap = cmVectArrayAllocFromFile(ctx, fn )) == NULL )
  {
    rc = cmCtxRtCondition(&p->obj,cmSubSysFailRC,"Id component vector file read failed '%s'.",fn);
    goto errLabel;
  }

  // get the count of elements in the vector
  *vnRef = cmVectArrayEleCount(vap);

  // allocate memory to hold the vector
  v = cmMemAlloc(cmSample_t,*vnRef);

  // copy the vector from the vector array object into v[]
  if((rc = cmVectArrayGetF(vap,v,vnRef)) != cmOkRC )
  {
    cmMemFree(v);
    v = NULL;
    rc = cmCtxRtCondition(&p->obj,cmSubSysFailRC,"Id component vector copy out failed '%s'.",fn);
    goto errLabel;
  }

  cmRptPrintf(p->obj.err.rpt,"%i : %s",*vnRef,fn);
  

 errLabel:
  cmVectArrayFree(&vap);

  return v;
}


cmRC_t   cmPhatExec(   cmPhat_t* p, const cmSample_t* xV, unsigned xN )
{
  unsigned n = cmMin(xN,p->fhN-p->di);

  // update the delay line
  cmVOS_Copy(p->dV+p->di,n,xV);

  if( n < xN )
    cmVOS_Copy(p->dV,xN-n,xV+n);

  p->di      = cmModIncr(p->di,xN,p->fhN);

  // p->absIdx is the absolute sample index associated with di
  p->absIdx += xN;  

  return cmOkRC;
}


void cmPhatChExec( 
    cmPhat_t* p, 
    unsigned  chIdx,
    unsigned  sessionId,
    unsigned  roleId)
{

  unsigned n0 = p->fhN - p->di;
  unsigned n1 = p->fhN - n0;

  // Linearize the delay line into xV[]
  cmVOS_Copy(p->xV,    n0, p->dV + p->di );
  cmVOS_Copy(p->xV+n0, n1, p->dV         );

  if( cmIsFlag(p->flags,kDebugAtPhatFl)) 
    cmVectArrayAppendS(p->ftVa, p->xV, p->fhN );

  // apply a window function to the incoming signal
  if( cmIsFlag(p->flags,kHannAtPhatFl) )
    cmVOS_MultVV(p->xV,p->fhN,p->wndV);
  
  // Take the FFT of the delay line.
  // p->t0V[p->binN] = fft(p->xV)
  //atFftRealForward(p->fftH, p->xV, p->fhN, p->t0V, p->binN );
  cmFftExecSR(&p->fft, p->xV, p->fhN );
 
  // Calc. the Cross Power Spectrum (aka cross spectral density) of the
  // input signal with the id signal.
  // Note that the CPS is equivalent to the Fourier Transform of the
  // cross-correlation of the two signals.
  // t0V[] *= p->fhM[:,chIdx]
  //atFftComplexMult( p->t0V, p->fhM + (chIdx * p->fhN), p->binN );
  cmVOCR_MultVVV( p->t0V, p->fft.complexV, p->fhM + (chIdx * p->fhN), p->binN);
  
  // Calculate the magnitude of the CPS.
  // xV[] = | t0V[] |
  cmVOCR_Abs( p->xV, p->t0V, p->binN );
  
  // Weight the CPS by the scaled magnitude of the id signal
  // (we want to emphasize the limited frequencies where the
  //  id signal contains energy)
  // t0V[] *= p->mhM[:,chIdx]
  if( p->alpha > 0 )
    cmVOCR_MultVFV( p->t0V, p->mhM + (chIdx*p->binN), p->binN);

  // Divide through by the magnitude of the CPS
  // This has the effect of whitening the spectram and thereby
  // minimizing the effect of the magnitude correlation 
  // while maximimizing the effect of the phase correlation.
  // 
  // t0V[] /= xV[]
  cmVOCR_DivVFV( p->t0V, p->xV, p->binN );

  // Take the IFFT of the weighted CPS to recover the cross correlation.
  // xV[] = IFFT(t0V[])
  cmIFftExecRS( &p->ifft, p->t0V );

  // Normalize the result by the length of the transform.
  cmVOS_DivVVS( p->xV, p->fhN, p->ifft.outV, p->fhN );

  // Shift the correlation spike to mark the end of the id
  cmVOS_Rotate( p->xV, p->fhN, -((int)p->hN) );

  // normalize by the length of the correlation
  cmVOS_DivVS(p->xV,p->fhN,p->fhN);

  if( cmIsFlag(p->flags,kDebugAtPhatFl))
  {
    cmVectArrayAppendS(p->ftVa, p->xV, p->fhN );

    cmSample_t v[] = { sessionId, roleId };
    cmVectArrayAppendS(p->ftVa, v, sizeof(v)/sizeof(v[0]));
  }

}

cmRC_t cmPhatWrite( cmPhat_t* p, const char* dirStr )
{
  cmRC_t rc = cmOkRC;
  
  if( cmIsFlag(p->flags, kDebugAtPhatFl)) 
  {
    const char* path = NULL;

    if( p->ftVa != NULL )
      if((rc = cmVectArrayWrite(p->ftVa, path = cmFsMakeFn(path,"cmPhatFT","va",dirStr,NULL) )) != cmOkRC )
        rc = cmCtxRtCondition(&p->obj,cmSubSysFailRC,"PHAT debug file write failed.");
    
    cmFsFreeFn(path);
  }
  
  return rc;
}


//=======================================================================================================================
// 
//

cmReflectCalc_t* cmReflectCalcAlloc( cmCtx* ctx, cmReflectCalc_t* p, const cmGoldSigArg_t* gsa, float phat_alpha, unsigned phat_mult )
{
  cmReflectCalc_t* op = cmObjAlloc(cmReflectCalc_t,ctx,p);
  cmRC_t rc = cmOkRC;
  
  // allocate the Gold code signal generator
  if( (op->gs = cmGoldSigAlloc(ctx,NULL,NULL)) == NULL )
  {
    rc = cmCtxRtCondition(&p->obj,cmSubSysFailRC,"Gold sig allocate failed.");
    goto errLabel;
  }

  // allocate the PHAT object
  if( (op->phat = cmPhatAlloc(ctx,NULL,0,0,0,0,0)) == NULL )
  {
    rc = cmCtxRtCondition(&p->obj,cmSubSysFailRC,"PHAT allocate failed.");
    goto errLabel;
  }

  op->va = cmVectArrayAlloc(ctx,kSampleVaFl);

  // allocate 'this'
  if( gsa != NULL )  
    rc = cmReflectCalcInit(op,gsa,phat_alpha,phat_mult);
      

 errLabel:
      if( rc != cmOkRC )
        cmReflectCalcFree(&op);
                        
  
  return op;
  
}

cmRC_t cmReflectCalcFree( cmReflectCalc_t** pp )
{
  cmRC_t rc = cmOkRC;

  if( pp == NULL || *pp == NULL )
    return rc;

  cmReflectCalc_t* p = *pp;

  cmReflectCalcWrite(p,"/home/kevin/temp/cmkc");
  
  if((rc = cmReflectCalcFinal(p)) != cmOkRC )
    return rc;

  cmVectArrayFree(&p->va);
  cmGoldSigFree(&p->gs); 
  cmPhatFree(&p->phat);
  
  cmMemFree(p);
  *pp = NULL;

  return rc;
}

cmRC_t cmReflectCalcInit( cmReflectCalc_t* p, const cmGoldSigArg_t* gsa, float phat_alpha, unsigned phat_mult )
{
  cmRC_t rc;
  if((rc = cmReflectCalcFinal(p)) != cmOkRC )
    return rc;

  //  initialize the Gold code signal generator
  if((rc = cmGoldSigInit(p->gs,gsa)) != cmOkRC )
  {
    rc = cmCtxRtCondition(&p->obj,cmSubSysFailRC,"Gold code signal initialize failed.");
    goto errLabel;
  }
  
  unsigned phat_chN   = 1;
  unsigned phat_hN    = p->gs->sigN;
  unsigned phat_flags = 0;
  unsigned phat_chIdx = 0;
  
  // initialize the PHAT
  if((rc = cmPhatInit(p->phat,phat_chN,phat_hN,phat_alpha,phat_mult,phat_flags)) != cmOkRC )
  {
    rc = cmCtxRtCondition(&p->obj,cmSubSysFailRC,"PHAT intialize failed.");
    goto errLabel;
  }

  // register a target signal with the PHAT
  if((rc = cmPhatSetId( p->phat, phat_chIdx, p->gs->ch[phat_chIdx].mdV, p->gs->sigN )) != cmOkRC )
  {
    rc = cmCtxRtCondition(&p->obj,cmSubSysFailRC,"PHAT signal registration failed.");
    goto errLabel;
  }

  
  p->xi     = 0;
  p->zeroFl = false;

 errLabel:
  
  return rc;
}

cmRC_t cmReflectCalcFinal( cmReflectCalc_t* p )
{
  cmGoldSigFinal(p->gs);
  cmPhatFinal(p->phat);
  return cmOkRC;
}

cmRC_t cmReflectCalcExec( cmReflectCalc_t* p, const cmSample_t* xV, cmSample_t* yV, unsigned xyN )
{
  unsigned i;

  // feed audio into the PHAT's buffer
  cmPhatExec(p->phat,xV,xyN);
  
  for(i=0; i<xyN; ++i,++p->xi)
  {
    if( p->xi < p->gs->sigN )
      yV[i] = p->gs->ch[0].mdV[p->xi];
    else
      yV[i] = 0;

    // if the PHAT has a complete buffer
    if( p->xi == p->phat->fhN )
    {
      p->xi = 0;

      // execute the correlation
      cmPhatChExec(p->phat,0,0,0);

      // p->phat->xV now holds the correlation result
      
      if( p->va != NULL )
        cmVectArrayAppendS(p->va,p->phat->xV,p->phat->fhN );
    }
    
  }

  return cmOkRC;
  
}

cmRC_t cmReflectCalcWrite( cmReflectCalc_t* p, const char* dirStr )
{
  cmRC_t rc = cmOkRC;
  
  if( p->va != NULL) 
  {
    const char* path = NULL;

    if((rc = cmVectArrayWrite(p->va, path = cmFsMakeFn(path,"reflect_calc","va",dirStr,NULL) )) != cmOkRC )
        rc = cmCtxRtCondition(&p->obj,cmSubSysFailRC,"Reflect calc file write failed.");
    
    cmFsFreeFn(path);
  }
  
  return rc;
}
