//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#ifdef  cmProcTemplateCode_h

//-----------------------------------------------------------------------------------------------------------
// FFT
//

CLASS(Fft)* MEMBER(FftAlloc)( cmCtx* c, CLASS(Fft)* ap, T0* inPtr, unsigned wndSmpCnt, unsigned flags )
{
  CLASS(Fft)* p = cmObjAlloc( CLASS(Fft), c, ap );

  if( wndSmpCnt > 0 )
    if( MEMBER(FftInit)( p,inPtr,wndSmpCnt,flags) != cmOkRC )
      MEMBER(FftFree)(&p);

  return p;
}

cmRC_t        MEMBER(FftFree)(   CLASS(Fft)** pp )
{
  if( pp != NULL && *pp != NULL )
  {
    CLASS(Fft)* p = *pp;

    if( MEMBER(FftFinal)(  *pp ) == cmOkRC )
    {
      cmMemPtrFree( &p->complexV );
      cmMemPtrFree( &p->magV );
      cmMemPtrFree( &p->phsV);

      if( p->copyFl )
        cmMemPtrFree( &p->inPtr );

      cmObjFree(pp);
    }
  }
  return cmOkRC;
}

cmRC_t        MEMBER(FftInit)(   CLASS(Fft)* p, T0*   inPtr, unsigned wndSmpCnt, unsigned flags )
{
  cmRC_t rc;

  if( cmIsPowerOfTwo(wndSmpCnt) == false )
    return cmCtxRtAssertFailed(&p->obj,cmArgAssertRC,"The FFT window sample count (%i) is not a power of two.",wndSmpCnt);

  if((rc = MEMBER(FftFinal)(p)) != cmOkRC )
    return rc;

  p->wndSmpCnt = wndSmpCnt;
  p->binCnt    = wndSmpCnt / 2 + 1;
  p->flags     = flags;
  p->magV      = cmMemResize( T1,       p->magV,     p->binCnt );
  p->phsV      = cmMemResize( T1,       p->phsV,     p->binCnt );
  p->copyFl    = inPtr == NULL;
  p->complexV  = cmMemResize( COMPLEX_T0,   p->complexV, p->wndSmpCnt );
  p->inPtr     = p->copyFl ? cmMemResizeZ( T0, p->inPtr, p->wndSmpCnt ) : inPtr;

  p->plan      = FFT_FUNC_T0(FftPlanAlloc)( p->wndSmpCnt, p->inPtr, p->complexV, FFTW_ESTIMATE );  

  //p->mfp       = cmCtxAllocDebugFile( p->obj.ctx,"fft");
  return cmOkRC;


}

cmRC_t        MEMBER(FftFinal)(  CLASS(Fft)* p )
{
  if( p != NULL )
  {
    //cmCtxFreeDebugFile(p->obj.ctx, &p->mfp);

    if( p->plan != NULL )
    {
      FFT_FUNC_T0(FftPlanFree)( p->plan );
      p->plan = NULL;
    }

  }
  return cmOkRC;

}

cmRC_t        MEMBER(FftExec)(  CLASS(Fft)* p, const T0*   sp, unsigned sn  )
{
  // if a fixed input buffer is not being used then copy in the source samples
  if( sp != NULL  && p->copyFl == true )
  {
    assert( p->inPtr != NULL );
    unsigned n = cmMin(sn,p->wndSmpCnt);

    VOP_T0(Copy)( p->inPtr,n, sp );

    if( n < p->wndSmpCnt )
      VOP_T0(Fill)( p->inPtr+n, p->wndSmpCnt-n, 0 );
  }

  // perform the Fourier transform
  FFT_FUNC_T0(FftExecute)(p->plan);

  COMPLEX_T0* cp = p->complexV;
  T1*         mp = p->magV;
  T1*         pp = p->phsV;
  T1*         ep = mp + p->binCnt;

  // if polar conversion was requested
  if( cmIsFlag(p->flags, kToPolarFftFl ) )
  {
    while( mp < ep )
    {
      *mp++ = cmCabsR(*cp);
      *pp++ = cmCargR(*cp++);
    }
  }

  else
    // if rectangular splitting was requested
    if( cmIsFlag(p->flags, kToRectFftFl ) )
    {
      while( mp < ep )
      {
        *mp++ = cmCrealR(*cp);
        *pp++ = cmCimagR(*cp++);        
      }
    }

  /*
  if( p->mfp != NULL )
  {
    cmMtxFileRealExec(  p->mfp, p->magV, p->binCnt );
    cmMtxFileRealExec(  p->mfp, p->phsV, p->binCnt );
  }
  */

  return cmOkRC;

}

//-----------------------------------------------------------------------------------------------------------
// IFft
//

CLASS(IFft)*  MEMBER(IFftAlloc)( cmCtx* c, CLASS(IFft)* ap, unsigned binCnt )
{
  CLASS(IFft)* p = cmObjAlloc( CLASS(IFft), c, ap );

  if( binCnt > 0 )
    if( MEMBER(IFftInit)( p,binCnt) != cmOkRC )
      MEMBER(IFftFree)(&p);
  return p;
}

cmRC_t   MEMBER(IFftFree)(  CLASS(IFft)** pp )
{
  if( pp != NULL && pp != NULL)
  {
    CLASS(IFft)* p = *pp;
    if( MEMBER(IFftFinal)(p) == cmOkRC )
    {
      cmMemPtrFree(&p->complexV);
      cmMemPtrFree(&p->outV);
      cmObjFree(pp);
    }
   
  }
  return cmOkRC;
}

cmRC_t   MEMBER(IFftInit)(  CLASS(IFft)* p, unsigned binCnt )
{
  cmRC_t rc;
  if((rc = MEMBER(IFftFinal)(p)) != cmOkRC )
    return rc;

  p->outN     = (binCnt-1)*2;
  p->outV     = cmMemResizeZ(T1,         p->outV,    p->outN);
  p->complexV = cmMemResizeZ(COMPLEX_T1, p->complexV,p->outN);

  if( p->binCnt != binCnt )
  {
    p->binCnt   = binCnt;
    p->plan     =  FFT_FUNC_T1(IFftPlanAlloc)( p->outN, p->complexV, p->outV, FFTW_ESTIMATE );    
  }

  return cmOkRC;
}

cmRC_t   MEMBER(IFftFinal)( CLASS(IFft)* p )
{ return cmOkRC; }


  // x must contain 'binCnt' elements.
cmRC_t   MEMBER(IFftExec)(      CLASS(IFft)* p, COMPLEX_T0* x )
{
  unsigned i,j;

  if( x != NULL )
    for(i=0; i<p->binCnt; ++i)
      p->complexV[i] = x[i];

  for(i=p->outN-1,j=1; j<p->binCnt-1; --i,++j)
    p->complexV[i] = (COMPLEX_T1)conj(p->complexV[j]);

  FFT_FUNC_T1(FftExecute)(p->plan);  

  return cmOkRC;
}

cmRC_t   MEMBER(IFftExecPolar)( CLASS(IFft)* p, const T0* magV, const T0* phsV ) 
{
  unsigned i,j;
  for(i=0; i<p->binCnt; ++i)
    p->complexV[i] = (COMPLEX_T1)(magV[i] * cos(phsV[i])) + (magV[i] * I * sin(phsV[i]));

  for(i=p->outN-1,j=1; j<p->binCnt-1; --i,++j)
    p->complexV[i] = (COMPLEX_T1)(magV[j] * cos(phsV[j])) + (magV[j] * I * sin(phsV[j]));

  FFT_FUNC_T1(FftExecute)(p->plan);  

  return cmOkRC;

}

cmRC_t   MEMBER(IFftExecRect)(  CLASS(IFft)* p, const T0* rV,   const T0* iV )
{
  unsigned i,j;
  for(i=0; i<p->binCnt; ++i)
    p->complexV[i] = rV[i] + (I *  iV[i]);

  for(i=p->outN-1,j=1; j<p->binCnt-1; --i,++j)
    p->complexV[i] = rV[j] + (I *  iV[j]);

  FFT_FUNC_T1(FftExecute)(p->plan);  

  return cmOkRC;
 
}




#endif
