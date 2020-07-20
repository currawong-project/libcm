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
#include "cmProcObj.h"
#include "cmProcTemplate.h"
#include "cmAudioFile.h"
#include "cmMath.h"
#include "cmProc.h"
#include "cmVectOps.h"
#include "cmKeyboard.h"
#include "cmGnuPlot.h"

#include <time.h> // time()

//------------------------------------------------------------------------------------------------------------
void cmFloatPointExceptHandler( int signo, siginfo_t* info, void* context )
{
  char* cp = "<Type Unknown>";
  switch( info->si_code )
  {
    case FPE_INTDIV: cp = "integer divide by zero";               break;
    case FPE_INTOVF: cp = "integer overflow";                     break;
    case FPE_FLTDIV: cp = "divide by zero";                 break;
    case FPE_FLTUND: cp = "underflow";                      break;
    case FPE_FLTRES: cp = "inexact result";                 break;
    case FPE_FLTINV: cp = "invalid operation";              break;
    case FPE_FLTSUB: cp = "subscript range error"; break;
  }

  fprintf(stderr,"Floating point exception: Type: %s\n",cp);
  exit(1);
}

// set 'orgSaPtr' to NULL to discard the current signal action state
void cmSetupFloatPointExceptHandler( struct sigaction* orgSaPtr )
{
  struct sigaction sa;

  sa.sa_handler    = SIG_DFL;
  sa.sa_flags      = SA_SIGINFO;
  sa.sa_sigaction  = cmFloatPointExceptHandler;

  sigemptyset(&sa.sa_mask);

  // set all FP except flags excetp: FE_INEXACT 

#ifdef OS_OSX
  // we don't yet support FP exceptions on OSX
  // for an example of how to make this work with the linux interface as below
  // see: http://www-personal.umich.edu/~williams/archive/computation/fe-handling-example.c
  assert(0);
#else
  // int flags = FE_DIVBYZERO | FE_UNDERFLOW | FE_OVERFLOW | FE_INVALID;
  // feenableexcept(flags);

  assert(0);

#endif
  sigaction( SIGFPE, &sa, orgSaPtr ); 

}


//------------------------------------------------------------------------------------------------------------

cmAudioFileRd* cmAudioFileRdAlloc( cmCtx* c,  cmAudioFileRd* p, unsigned procSmpCnt, const cmChar_t* fn, unsigned chIdx, unsigned begFrmIdx, unsigned endFrmIdx )
{ 
  cmAudioFileRd* op = cmObjAlloc( cmAudioFileRd, c, p ); 

  if( fn != NULL )
    if( cmAudioFileRdOpen( op, procSmpCnt, fn, chIdx, begFrmIdx, endFrmIdx ) != cmOkRC )
      cmAudioFileRdFree(&op);

  return op;
}

cmRC_t               cmAudioFileRdFree(  cmAudioFileRd** pp )
{ 
  cmRC_t rc = cmOkRC;

  if( pp != NULL && *pp != NULL )
  {
    cmAudioFileRd* p = *pp;

    if((rc = cmAudioFileRdClose(p)) == cmOkRC )
    {
      cmMemPtrFree(&p->outV);
      cmMemPtrFree(&p->fn);
      cmObjFree(pp);
    }
  }

  return rc;
}

cmRC_t               cmAudioFileRdOpen(  cmAudioFileRd* p,  unsigned procSmpCnt, const cmChar_t* fn, unsigned chIdx, unsigned begFrmIdx, unsigned endFrmIdx )
{
  cmRC_t rc;
  cmRC_t afRC;

  if((rc = cmAudioFileRdClose(p)) != cmOkRC )
    return rc;

  p->h = cmAudioFileNewOpen( fn, &p->info, &afRC, p->obj.err.rpt ); 

  if( afRC != kOkAfRC )
    return cmCtxRtCondition( &p->obj, afRC, "Unable to open the audio file:'%s'", fn );

  p->chIdx          = chIdx;
  p->outN           = endFrmIdx==cmInvalidIdx ? p->info.frameCnt : procSmpCnt;
  p->outV           = cmMemResizeZ( cmSample_t, p->outV, p->outN );  
  p->fn             = cmMemResizeZ( cmChar_t,   p->fn,   strlen(fn)+1 );
  strcpy(p->fn,fn);  
  //p->mfp            = cmCtxAllocDebugFile( p->obj.ctx,"audioFile");
  p->lastReadFrmCnt = 0;
  p->eofFl          = false;
  p->begFrmIdx      = begFrmIdx;
  p->endFrmIdx      = endFrmIdx==0 ? p->info.frameCnt : endFrmIdx;
  p->curFrmIdx      = p->begFrmIdx;

  if( p->begFrmIdx > 0 )
    rc = cmAudioFileRdSeek(p,p->begFrmIdx);

  return rc;
}

cmRC_t               cmAudioFileRdClose( cmAudioFileRd* p )
{
  cmRC_t rc = cmOkRC;
  cmRC_t afRC;

  if( p == NULL )
    return cmOkRC;

  //cmCtxFreeDebugFile(p->obj.ctx,&p->mfp);

  if( cmAudioFileIsOpen(p->h) == false )
    return cmOkRC;

  if((afRC = cmAudioFileDelete(&p->h)) != cmOkRC )
    rc = cmCtxRtCondition( &p->obj, afRC, "An attempt to close the audio file'%s' failed.", p->fn );

  return rc;
}

cmRC_t               cmAudioFileRdRead(  cmAudioFileRd* p )
{
  cmRC_t rc = cmOkRC;
  cmRC_t afRC;

  if(p->eofFl || ((p->eofFl = cmAudioFileIsEOF(p->h)) == true) )
    return cmEofRC;

  unsigned n = p->endFrmIdx==cmInvalidIdx ? p->outN : cmMin( p->outN, p->endFrmIdx - p->curFrmIdx );

  if((afRC =  cmAudioFileReadSample( p->h, n, p->chIdx, 1, &p->outV, &p->lastReadFrmCnt )) != kOkAfRC )
    rc = cmCtxRtCondition( &p->obj, afRC, "Audio file read failed on:'%s'.", p->fn);

  p->curFrmIdx += p->lastReadFrmCnt;

  if( n < p->outN )
  {
    cmVOS_Zero(p->outV + p->lastReadFrmCnt, p->outN - p->lastReadFrmCnt);
    p->eofFl = true;
  }

  if( p->mfp != NULL )
    cmMtxFileSmpExec( p->mfp, p->outV, p->outN );

  return rc;    
}

cmRC_t               cmAudioFileRdSeek( cmAudioFileRd* p, unsigned frmIdx )
{
  cmRC_t rc = cmOkRC;
  cmRC_t afRC;

  if((afRC = cmAudioFileSeek( p->h, frmIdx )) != kOkAfRC )
    rc = cmCtxRtCondition( &p->obj, afRC, "Audio file read failed on:'%s'.", p->fn);

  return rc;
}

cmRC_t             cmAudioFileRdMinMaxMean( cmAudioFileRd* p, unsigned chIdx, cmSample_t* minPtr, cmSample_t* maxPtr, cmSample_t* meanPtr )
{
  cmRC_t rc = cmOkRC;
  cmRC_t afRC;

  if(( afRC = cmAudioFileMinMaxMean( p->h, chIdx, minPtr, maxPtr, meanPtr )) != kOkAfRC )
    rc = cmCtxRtCondition( &p->obj, afRC, "Audio file min, max, and mean calculation failed on '%s'", p->fn );

  return rc;
}



//------------------------------------------------------------------------------------------------------------

cmShiftBuf*       cmShiftBufAlloc( cmCtx* c, cmShiftBuf* p, unsigned procSmpCnt, unsigned wndSmpCnt, unsigned hopSmpCnt )
{
  cmShiftBuf* op = cmObjAlloc( cmShiftBuf, c, p );

  if( procSmpCnt > 0 && wndSmpCnt > 0 && hopSmpCnt > 0 )
    if( cmShiftBufInit(op, procSmpCnt, wndSmpCnt, hopSmpCnt ) != cmOkRC)
      cmShiftBufFree(&op);

  return op;
}

cmRC_t            cmShiftBufFree(  cmShiftBuf** pp )
{
  cmRC_t rc = cmOkRC;

  if( pp != NULL && *pp != NULL )
  {
    if((rc = cmShiftBufFinal(*pp)) == cmOkRC )
    {
      cmMemPtrFree(&(*pp)->bufV);
      cmObjFree(pp);
    }
  }
  return rc;
}

cmRC_t            cmShiftBufInit(  cmShiftBuf* p, unsigned procSmpCnt, unsigned wndSmpCnt, unsigned hopSmpCnt )
{
  cmRC_t rc;

  if( hopSmpCnt > wndSmpCnt )
    return cmCtxRtAssertFailed( &p->obj, cmArgAssertRC, "The window sample count (%i) must be greater than or equal to the hop sample count (%i).", wndSmpCnt, hopSmpCnt );

  if((rc = cmShiftBufFinal(p)) != cmOkRC )
    return rc;

  // The worst case storage requirement is where there are wndSmpCnt-1 samples in outV[] and procSmpCnt new samples arrive.
  p->bufSmpCnt    = wndSmpCnt + procSmpCnt;
  p->bufV         = cmMemResizeZ( cmSample_t, p->outV, p->bufSmpCnt );
  p->outV         = p->bufV;
  p->outN         = wndSmpCnt;
  p->wndSmpCnt    = wndSmpCnt;
  p->procSmpCnt   = procSmpCnt;
  p->hopSmpCnt    = hopSmpCnt;
  p->inPtr        = p->outV;
  p->fl           = false;

  return cmOkRC;  
}

cmRC_t            cmShiftBufFinal( cmShiftBuf* p )
{
  return cmOkRC;
}


// This function should be called in a loop until it returns false.
// Note that 'sp' and 'sn' are ignored except p->fl == false.
bool           cmShiftBufExec(  cmShiftBuf* p, const cmSample_t* sp, unsigned sn   )
{
  assert( sn <= p->procSmpCnt );

  // The active samples are in outV[wndSmpCnt]
  // Stored samples are between outV + wndSmpCnt and inPtr.

  // if the previous call to this function returned true then the buffer must be
  // shifted by hopSmpCnt samples - AND sp[] is ignored.
  if( p->fl )
  {
    // shift the output buffer to the left to remove expired samples 
    p->outV += p->hopSmpCnt;

    // if there are not  wndSmpCnt samples left in the buffer 
    if( p->inPtr - p->outV < p->wndSmpCnt )
    {
      // then copy the remaining active samples (between outV and inPtr) 
      // to the base of the physicalbuffer
      unsigned n = p->inPtr - p->outV;
      memmove( p->bufV, p->outV, n * sizeof(cmSample_t));

      p->inPtr  = p->bufV + n; // update the input and output positions
      p->outV   = p->bufV;
    }
  }
  else
  {
    // if the previous call to this function returned false then sp[sn] should not be ignored
    assert( p->inPtr + sn <= p->outV + p->bufSmpCnt );
    // copy the incoming samples into the buffer
    cmVOS_Copy(p->inPtr,sn,sp);
    p->inPtr += sn;
  }

  // if there are at least wndSmpCnt available samples in outV[]
  p->fl = p->inPtr - p->outV >= p->wndSmpCnt;
  
  return p->fl;
}


void cmShiftBufTest( cmCtx* c )
{
  unsigned    smpCnt     = 48;
  unsigned    procSmpCnt =  5;

  unsigned    hopSmpCnt  =  6;
  unsigned    wndSmpCnt  =  7;
  unsigned    i;

  cmShiftBuf* b = cmShiftBufAlloc(c,NULL,procSmpCnt,wndSmpCnt,hopSmpCnt );

  cmSample_t x[ smpCnt ];
  cmVOS_Seq(x,smpCnt,1,1);

  //cmVOS_Print( rptFuncPtr, 1, smpCnt, x );
  
  for(i=0; i<smpCnt; i+=procSmpCnt)
  {
    while( cmShiftBufExec( b, x + i, procSmpCnt ) )
    {
      cmVOS_Print( c->obj.err.rpt, 1, wndSmpCnt, b->outV );
    }
  }


  cmShiftBufFree(&b);

}

/*
bool           cmShiftBufExec(  cmShiftBuf* p, const cmSample_t* sp, unsigned sn   )
{
  bool retFl = false;

  if( sn > p->procSmpCnt )
  {
     cmCtxRtAssertFailed( p->obj.ctx, cmArgAssertRC, "The input sample count (%i) must be less than or equal to the proc sample count (%i).", sn, p->procSmpCnt);
     return false;
  }

  assert( sn <= p->procSmpCnt );

  cmSample_t*  dbp = p->outV;
  cmSample_t*  dep = p->outV + (p->outN - sn);
  cmSample_t*  sbp = p->outV + sn;

  // shift the last bufCnt-shiftCnt samples over the first shiftCnt samples
  while( dbp < dep )
    *dbp++ = *sbp++;

  // copy in the new samples 
  dbp = dep;
  dep = dbp + sn;
  while( dbp < dep )
    *dbp++ = *sp++;

  // if any space remains at the end of the buffer then zero it
  dep = p->outV + p->outN;
  while( dbp < dep )
    *dbp++ = 0;


  if( p->firstPtr > p->outV )
    p->firstPtr = cmMax( p->outV, p->firstPtr - p->procSmpCnt);

  p->curHopSmpCnt += sn;

  if( p->curHopSmpCnt >= p->hopSmpCnt )
  {
    p->curHopSmpCnt -= p->hopSmpCnt;
    retFl         = true;
  }

  if( p->mfp != NULL )
    cmMtxFileSmpExec(p->mfp,p->outV,p->outN);

  return retFl;
}
*/

//------------------------------------------------------------------------------------------------------------

cmWndFunc*  cmWndFuncAlloc( cmCtx* c, cmWndFunc* p, unsigned wndId, unsigned wndSmpCnt, double kaiserSideLobeRejectDb )
{
  cmWndFunc* op = cmObjAlloc( cmWndFunc, c, p );

  if( wndId != kInvalidWndId )
    if( cmWndFuncInit(op,wndId,wndSmpCnt,kaiserSideLobeRejectDb ) != cmOkRC )
      cmWndFuncFree(&op);

  return op;
}

cmRC_t      cmWndFuncFree(  cmWndFunc** pp )
{
  cmRC_t rc = cmOkRC;
  if( pp != NULL && *pp != NULL )
  {

    cmWndFunc* p = *pp;
    if((rc = cmWndFuncFinal(p)) == cmOkRC )
    {
      cmMemPtrFree(&p->wndV);
      cmMemPtrFree(&p->outV);
      cmObjFree(pp);
    }
  }

  return rc;
}

cmRC_t      cmWndFuncInit(  cmWndFunc* p, unsigned wndId, unsigned wndSmpCnt, double kslRejectDb )
{
  cmRC_t rc;

  if( wndId == (p->wndId | p->flags) && wndSmpCnt == p->outN && kslRejectDb == p->kslRejectDb )
    return cmOkRC;

  if((rc = cmWndFuncFinal(p)) != cmOkRC )
    return rc;

  p->wndV        = cmMemResize( cmSample_t, p->wndV, wndSmpCnt );
  p->outV        = cmMemResize( cmSample_t, p->outV, wndSmpCnt );
  p->outN        = wndSmpCnt;
  p->wndId       = wndId;
  p->kslRejectDb = kslRejectDb;
  //p->mfp         = cmCtxAllocDebugFile(p->obj.ctx,"wndFunc");
  p->flags       = wndId & (~kWndIdMask);
 
  switch( wndId & kWndIdMask )
  {
    case kHannWndId:       cmVOS_Hann(       p->wndV, p->outN ); break;
    case kHannMatlabWndId: cmVOS_HannMatlab( p->wndV, p->outN ); break;
    case kHammingWndId:    cmVOS_Hamming(    p->wndV, p->outN ); break;
    case kTriangleWndId:   cmVOS_Triangle(   p->wndV, p->outN ); break;
    case kUnityWndId:      cmVOS_Fill(       p->wndV, p->outN, 1.0 ); break;
    case kKaiserWndId:    
      {
        double beta;
        
        if( cmIsFlag(wndId,kSlRejIsBetaWndFl) )
          beta = kslRejectDb;
        else
          beta = cmVOS_KaiserBetaFromSidelobeReject(fabs(kslRejectDb));
        
        cmVOS_Kaiser( p->wndV,p->outN, beta); 
      }
      break;
    case kInvalidWndId:    break;

    default:
      { assert(0); }
  }

  cmSample_t den = 0;
  cmSample_t num = 1;
  if( cmIsFlag(p->flags,kNormBySumWndFl) )
  {
    den = cmVOS_Sum(p->wndV, p->outN);
    num = wndSmpCnt;
  }

  if( cmIsFlag(p->flags,kNormByLengthWndFl) )
    den += wndSmpCnt;
    
  if( den > 0 )
  {
    cmVOS_MultVS(p->wndV,p->outN,num);
    cmVOS_DivVS(p->wndV,p->outN,den);
  }

  return cmOkRC;
}

cmRC_t      cmWndFuncFinal( cmWndFunc* p )
{
  //if( p != NULL )
  //  cmCtxFreeDebugFile(p->obj.ctx,&p->mfp);

  return cmOkRC;
}



cmRC_t      cmWndFuncExec(  cmWndFunc* p, const cmSample_t* sp, unsigned sn )
{
  if( sn > p->outN )
    return cmCtxRtAssertFailed( &p->obj, cmArgAssertRC, "The length of the input vector (%i)  is greater thean the length of the window function (%i).", sn, p->outN ); 

  if( p->wndId != kInvalidWndId )
    cmVOS_MultVVV( p->outV, sn, sp, p->wndV );

  if( p->mfp != NULL )
    cmMtxFileSmpExec(p->mfp,p->outV,p->outN);

  return cmOkRC;

}

void cmWndFuncTest( cmRpt_t* rpt, cmLHeapH_t lhH, cmSymTblH_t stH  )
{

    unsigned wndCnt               = 5;
    double kaiserSideLobeRejectDb = 30;

    cmCtx c;
    cmCtxAlloc(&c,rpt,lhH,stH);
    cmWndFunc*  p = cmWndFuncAlloc(&c,NULL,kHannWndId,wndCnt, 0 );
    cmVOS_Print( rpt, 1, wndCnt, p->wndV );
    
    cmWndFuncInit(p,kHammingWndId ,wndCnt, 0 );
    cmVOS_Print( rpt, 1, wndCnt, p->wndV );

    cmWndFuncInit(p,kTriangleWndId ,wndCnt, 0 );
    cmVOS_Print( rpt, 1, wndCnt, p->wndV );

    cmWndFuncInit(p,kKaiserWndId ,wndCnt, kaiserSideLobeRejectDb );
    cmVOS_Print( rpt, 1, wndCnt, p->wndV );

    cmSample_t wV[ wndCnt ];
    cmVOS_HannMatlab(wV,wndCnt);
    cmVOS_Print( rpt, 1, wndCnt, wV);

    cmWndFuncFree(&p);
}

//------------------------------------------------------------------------------------------------------------

cmSpecDelay* cmSpecDelayAlloc( cmCtx* c, cmSpecDelay* ap, unsigned maxDelayCnt, unsigned binCnt )
{
  cmSpecDelay* p = cmObjAlloc( cmSpecDelay, c, ap );


  if( maxDelayCnt > 0 && binCnt > 0 )
    if( cmSpecDelayInit(p,maxDelayCnt,binCnt) != cmOkRC  )
      cmSpecDelayFree(&p);

  return p;
}

cmRC_t       cmSpecDelayFree(  cmSpecDelay** pp )
{  
  cmRC_t rc = cmOkRC;

  if( pp != NULL && *pp != NULL  )
  {
    cmSpecDelay* p = *pp;
    if((rc=cmSpecDelayFinal(p)) == cmOkRC )
    {
      cmMemPtrFree(&p->bufPtr);
      cmObjFree(pp);
    }
  }
  return rc;
}


cmRC_t            cmSpecDelayInit( cmSpecDelay* p, unsigned maxDelayCnt, unsigned binCnt )
{
  cmRC_t rc;
  if((rc = cmSpecDelayFinal(p)) != cmOkRC )
    return rc;

  p->bufPtr      = cmMemResizeZ( cmSample_t, p->bufPtr, binCnt * maxDelayCnt );
  p->maxDelayCnt = maxDelayCnt;
  p->outN        = binCnt;
  p->inIdx       = 0;
  return cmOkRC;
}

cmRC_t            cmSpecDelayFinal(cmSpecDelay* p )
{ return cmOkRC; }

cmRC_t       cmSpecDelayExec(  cmSpecDelay* p, const cmSample_t* sp, unsigned sn )
{
  cmSample_t* dp = p->bufPtr + (p->inIdx * p->outN); 
  cmVOS_Copy( dp, cmMin(sn,p->outN), sp);
  p->inIdx = (p->inIdx+1) % p->maxDelayCnt;
  return cmOkRC;
}

const cmSample_t* cmSpecDelayOutPtr( cmSpecDelay* p, unsigned delayCnt )
{
  assert( delayCnt < p->maxDelayCnt );

  int i = p->inIdx - delayCnt;
  if( i < 0 )
    i = p->maxDelayCnt + i;

  return p->bufPtr + (i * p->outN);
}

//------------------------------------------------------------------------------------------------------------

cmFilter* cmFilterAlloc( cmCtx* c, cmFilter* ap, const cmReal_t* b, unsigned bn, const cmReal_t* a, unsigned an, unsigned procSmpCnt, const cmReal_t* d )
{
  cmRC_t rc;
  cmFilter* p = cmObjAlloc(cmFilter,c,ap);

  if( (bn > 0 || an > 0) && procSmpCnt > 0 )
    if( (rc = cmFilterInit( p, b, bn, a, an, procSmpCnt, d)) != cmOkRC )
      cmFilterFree(&p);

  return p;
}

cmFilter* cmFilterAllocEllip( cmCtx* c, cmFilter* ap, cmReal_t srate, cmReal_t passHz, cmReal_t stopHz, cmReal_t passDb, cmReal_t stopDb, unsigned procSmpCnt, const cmReal_t* d )
{
  cmRC_t rc;
  cmFilter* p = cmObjAlloc(cmFilter,c,ap);

  if( srate > 0 && passHz > 0 && procSmpCnt > 0 )
    if( (rc = cmFilterInitEllip( p, srate, passHz, stopHz, passDb, stopDb, procSmpCnt, d)) != cmOkRC )
      cmFilterFree(&p);

  return p;
}

cmRC_t cmFilterFree( cmFilter** pp )
{
  cmRC_t rc = cmOkRC;

  if( pp != NULL && *pp != NULL )
  {
    cmFilter* p = *pp;
    if((rc = cmFilterFinal(p))  == cmOkRC )
    {
      cmMemPtrFree(&p->a);
      cmMemPtrFree(&p->b);
      cmMemPtrFree(&p->d);
      cmMemPtrFree(&p->outSmpV);
      cmObjFree(pp);
    }
  }
  return rc;
}

cmRC_t cmFilterInit( cmFilter* p, const cmReal_t* b, unsigned bn, const cmReal_t* a, unsigned an, unsigned procSmpCnt, const cmReal_t* d )
{
  assert( bn >= 1 );
  assert( an >= 1 && a[0] != 0 );

  cmRC_t rc;
  if((rc = cmFilterFinal(p)) != cmOkRC )
    return rc;

  int cn  = cmMax(an,bn) - 1;

  // The output vector may be used as either cmReal_t or cmSample_t.
  // Find the larger of the two possible types.
  if( sizeof(cmReal_t) > sizeof(cmSample_t) )
  {
    p->outRealV   = cmMemResizeZ( cmReal_t, p->outRealV, procSmpCnt );
    p->outSmpV    = (cmSample_t*)p->outRealV;
  }
  else
  {
    p->outSmpV   = cmMemResizeZ( cmSample_t, p->outSmpV, procSmpCnt );
    p->outRealV  = (cmReal_t*)p->outRealV;

  }  

  p->a        = cmMemResizeZ( cmReal_t, p->a, cn );
  p->b        = cmMemResizeZ( cmReal_t, p->b, cn ); 
  p->d        = cmMemResizeZ( cmReal_t, p->d, cn+1 );
  //p->outV   = cmMemResizeZ( cmSample_t, p->outV, procSmpCnt );
  p->outN     = procSmpCnt;
  p->an       = an;
  p->bn       = bn;
  p->cn       = cn;
  p->di       = 0;
  p->b0       = b[0] / a[0];
  
  int i;
  for(i=0; i<an-1; ++i)
    p->a[i] = a[i+1] / a[0];

  for(i=0; i<bn-1; ++i)
    p->b[i] = b[i+1] / a[0];

  if( d != NULL )
    cmVOR_Copy(p->d,cn,d);

  return cmOkRC;
}

// initialize an elliptic lowpass filter with the given characteristics
// ref: Parks & Burrus, Digital Filter Design, sec. 7.2.7 - 7.2.8
cmRC_t cmFilterInitEllip( cmFilter* p, cmReal_t srate, cmReal_t passHz, cmReal_t stopHz, cmReal_t passDb, cmReal_t stopDb, unsigned procSmpCnt, const cmReal_t* d )
{
  assert( srate > 0 );
  assert( passHz > 0 && stopHz > passHz && srate/2 > stopHz );

  cmReal_t  Wp, Ws, ep, v0,
            k,  kc, k1, k1c,
            K,  Kc, K1, K1c,
            sn, cn, dn,
            sm, cm, dm,
            zr, zi, pr, pi;

  unsigned  N,  L,  j;

  // prewarp Wp and Ws, calculate k
  Wp = 2 * srate * tan(M_PI * passHz / srate);
  Ws = 2 * srate * tan(M_PI * stopHz / srate);
  k  = Wp / Ws;

  // calculate ep and k1 from passDb and stopDb
  ep = sqrt(pow(10, passDb/10) - 1);
  k1 = ep / sqrt(pow(10, stopDb/10) - 1);

  // calculate complimentary moduli
  kc  = sqrt(1-k*k);
  k1c = sqrt(1-k1*k1);

  // calculate complete elliptic integrals
  K   = cmEllipK( kc );
  Kc  = cmEllipK( k );
  K1  = cmEllipK( k1c );
  K1c = cmEllipK( k1 );

  // calculate minimum integer filter order N
  N = ceil(K*K1c/Kc/K1);

  // recalculate k and kc from chosen N
  // Ws is minimized while other specs held constant
  k  = cmEllipDeg( K1c/K1/N );
  kc = sqrt(1-k*k);
  K  = cmEllipK( kc );
  Kc = cmEllipK( k );
  Ws = Wp / k;

  // initialize temporary coefficient arrays
  cmReal_t b[N+1], a[N+1];
  a[0] = b[0] = 1;
  memset(b+1, 0, N*sizeof(cmReal_t));
  memset(a+1, 0, N*sizeof(cmReal_t));

  // intermediate value needed for determining poles
  v0 = K/K1/N * cmEllipArcSc( 1/ep, k1 );
  cmEllipJ( v0, k, &sm, &cm, &dm );

  for( L=1-N%2; L<N; L+=2 )
  {
    // find the next pole and zero on s-plane
    cmEllipJ( K*L/N, kc, &sn, &cn, &dn );
    zr =  0;
    zi =  L ? Ws/sn : 1E25;
    pr = -Wp*sm*cm*cn*dn/(1-pow(dn*sm,2));
    pi =  Wp*dm*sn/(1-pow(dn*sm,2));

    // convert pole and zero to z-plane using bilinear transform
    cmBlt( 1, srate, &zr, &zi );
    cmBlt( 1, srate, &pr, &pi );

    if( L == 0 )
    {
      // first order section
      b[1] = -zr;
      a[1] = -pr;
    }
    else
    {
      // replace complex root and its conjugate with 2nd order section
      zi = zr*zr + zi*zi;
      zr *= -2;
      pi = pr*pr + pi*pi;
      pr *= -2;

      // combine with previous sections to obtain filter coefficients
      for( j = L+1; j >= 2; j-- )
      {
        b[j] = b[j] + zr*b[j-1] + zi*b[j-2];
        a[j] = a[j] + pr*a[j-1] + pi*a[j-2];
      }

      b[1] += zr;
      a[1] += pr;
    }
  }

  // scale b coefficients s.t. DC gain is 0 dB
  cmReal_t sumB = 0, sumA = 0;
  for( j = 0; j < N+1; j++ )
  {
    sumB += b[j];
    sumA += a[j];
  }
  sumA /= sumB;
  for( j = 0; j < N+1; j++ )
    b[j] *= sumA;

  return cmFilterInit( p, b, N+1, a, N+1, procSmpCnt, d );
}

cmRC_t cmFilterFinal( cmFilter* p )
{ return cmOkRC; }


cmRC_t cmFilterExecS(  cmFilter* p, const cmSample_t* x, unsigned xn, cmSample_t* yy, unsigned yn )
{
  
  cmSample_t* y;

  if( yy == NULL || yn==0 )
  {
    y  = p->outSmpV;
    yn = p->outN;
  }
  else
  {
    y = yy;
  }

  cmVOS_Filter( y, yn, x, xn, p->b0, p->b, p->a, p->d, p->cn );
  return cmOkRC;
  
  /*

  int i,j;
  cmSample_t  y0 = 0;
  cmSample_t* y;
  unsigned    n;

  if( yy == NULL || yn==0 )
  {
    n  = cmMin(p->outN,xn);
    y  = p->outSmpV;
    yn = p->outN;
  }
  else
  {
    n = cmMin(yn,xn);
    y = yy;
  }

  // This is a direct form II algorithm based on the MATLAB implmentation
  // http://www.mathworks.com/access/helpdesk/help/techdoc/ref/filter.html#f83-1015962

  for(i=0; i<n; ++i)
  {
    //cmSample_t x0 = x[i];

    y[i] = (x[i] * p->b0) + p->d[0];

    y0 = y[i];

    for(j=0; j<p->cn; ++j)
      p->d[j] = (p->b[j] * x[i]) - (p->a[j] * y0) + p->d[j+1]; 
   
  }


  // if fewer input samples than output samples - zero the end of the output buffer
  if( yn > xn )
    cmVOS_Fill(y+i,yn-i,0);

  return cmOkRC;
  */
}

cmRC_t cmFilterExecR(  cmFilter* p, const cmReal_t* x, unsigned xn, cmReal_t* yy, unsigned yn )
{

  cmReal_t* y;

  if( yy == NULL || yn==0 )
  {
    y  = p->outRealV;
    yn = p->outN;
  }
  else
  {
    //n = cmMin(yn,xn);
    y = yy;
  }

  cmVOR_Filter( y, yn, x, xn, p->b0, p->b, p->a, p->d, p->cn );

  return cmOkRC;
}

cmRC_t    cmFilterSignal( cmCtx* c, const cmReal_t b[], unsigned bn, const cmReal_t a[], unsigned an, const cmSample_t* x, unsigned xn, cmSample_t* y, unsigned yn )
{
  int procSmpCnt = cmMin(1024,xn);
  cmFilter* p = cmFilterAlloc(c,NULL,b,bn,a,an,procSmpCnt,NULL);

  int i,n;

  for(i=0; i<xn && i<yn; i+=n)
  {
    n = cmMin(procSmpCnt,cmMin(yn-i,xn-i));
    cmFilterExecS(p,x+i,n,y+i,n);
  }

  if( i < yn )
    cmVOS_Fill(y+i,yn-i,0);

  cmFilterFree(&p);

  return cmOkRC;
}

cmRC_t    cmFilterFilterS(cmCtx* c, const cmReal_t bb[], unsigned bn, const cmReal_t aa[], unsigned an, const cmSample_t* x, unsigned xn, cmSample_t* y, unsigned yn )
{
  cmFilter* f  = cmFilterAlloc(c,NULL,NULL,0,NULL,0,0,NULL);
  cmVOS_FilterFilter( f, cmFilterExecS, bb,bn,aa,an,x,xn,y,yn);
  cmFilterFree(&f);
  return cmOkRC;
}

cmRC_t    cmFilterFilterR(cmCtx* c, const cmReal_t bb[], unsigned bn, const cmReal_t aa[], unsigned an, const cmReal_t* x, unsigned xn, cmReal_t* y, unsigned yn )
{
  cmFilter* f  = cmFilterAlloc(c,NULL,NULL,0,NULL,0,0,NULL);
  cmVOR_FilterFilter( f, cmFilterExecR, bb,bn,aa,an,x,xn,y,yn);
  cmFilterFree(&f);
  return cmOkRC;
}


void   cmFilterTest( cmRpt_t* rpt, cmLHeapH_t lhH, cmSymTblH_t stH )
{
  cmCtx c;
  cmCtxAlloc(&c, rpt, lhH, stH );

  cmReal_t   b[] = { 0.16, 0.32, 0.16 };
  unsigned   bn  = sizeof(b)/sizeof(cmReal_t);

  cmReal_t   a[] = {1 , -.5949, .2348 };
  unsigned   an  = sizeof(a)/sizeof(cmReal_t);

  cmReal_t   x[] = { 1,0,0,0,1,0,0,0 };
  unsigned   xn  = sizeof(x)/sizeof(cmReal_t);

  cmReal_t d[] = { .5, -.25};

  //  0.1600     0.4152     0.3694     0.1223     0.1460     0.3781     0.3507     0.1198   
  // -0.0111    -0.0281 

  cmFilter* p = cmFilterAlloc(&c,NULL,b,bn,a,an,xn,d);

  cmFilterExecR(p,x,xn,NULL,0);

  cmVOR_Print( rpt, 1, xn, p->outRealV );  

  cmVOR_Print( rpt, 1, p->cn, p->d );  

  cmFilterFree(&p);

  cmObjFreeStatic( cmCtxFree, cmCtx, c );

  /*
  cmReal_t   b[] = { 0.16, 0.32, 0.16 };
  unsigned   bn  = sizeof(b)/sizeof(cmReal_t);

  cmReal_t   a[] = { 1, -.5949, .2348};
  unsigned   an  = sizeof(a)/sizeof(cmReal_t);
  
  cmSample_t x[] = { 1,0,0,0,0,0,0,0,0,0 };
  unsigned   xn  = sizeof(x)/sizeof(cmSample_t);

  cmFilter* p = cmFilterAlloc(&c,NULL,b,bn,a,an,xn);

  cmFilterExec(&c,p,x,xn,NULL,0);
  cmVOS_Print( vReportFunc, 1, xn, p->outV );  

  cmVOR_Print( vReportFunc, 1, p->cn, p->d );  

  cmFilterExec(&c,p,x,xn,NULL,0);
  cmVOS_Print( vReportFunc, 1, xn, p->outV ); 

  cmFilterFree(&p);
  */

}

void   cmFilterFilterTest( cmRpt_t* rpt, cmLHeapH_t lhH, cmSymTblH_t stH )
{
  cmCtx c;
  cmCtxAlloc(&c, rpt, lhH, stH );

  cmReal_t   b[] = { 0.36, 0.32, 0.36 };
  unsigned   bn  = sizeof(b)/sizeof(cmReal_t);

  cmReal_t   a[] = {1 , -.5949, .2348 };
  unsigned   an  = sizeof(a)/sizeof(cmReal_t);

  cmReal_t   x[] = { 1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0 };
  unsigned   xn  = sizeof(x)/sizeof(cmReal_t);

  unsigned yn = xn;
  cmReal_t  y[yn];
  memset(y,0,sizeof(y));

  cmFilterFilterR(&c, b,bn,a,an,x,xn,y,yn );

  cmVOR_Print( rpt, 1, yn, y );  

  cmObjFreeStatic( cmCtxFree, cmCtx, c );

  

}

//------------------------------------------------------------------------------------------------------------

cmComplexDetect* cmComplexDetectAlloc(cmCtx* c, cmComplexDetect* p, unsigned binCnt )
{
  cmComplexDetect* op = cmObjAlloc( cmComplexDetect, c, p );
  
  cmSpecDelayAlloc(c,&op->phsDelay,0,0);
  cmSpecDelayAlloc(c,&op->magDelay,0,0);  

  if( binCnt > 0 )
    if( cmComplexDetectInit(op,binCnt) != cmOkRC && p == NULL )
      cmComplexDetectFree(&op);

  return op;
}

cmRC_t          cmComplexDetectFree( cmComplexDetect** pp )
{
  cmRC_t rc;
  if( pp != NULL && *pp != NULL )
  {
    cmComplexDetect* p = *pp;

    if((rc = cmComplexDetectFinal(p)) == cmOkRC )
    {
      cmSpecDelay* sdp;
      sdp = &p->phsDelay;
      cmSpecDelayFree(&sdp);
      sdp = &p->magDelay;
      cmSpecDelayFree(&sdp);

      cmObjFree(pp);
    }
  }

  return cmOkRC;
}

cmRC_t cmComplexDetectInit( cmComplexDetect* p, unsigned binCnt )
{
  cmRC_t rc;
  if((rc = cmComplexDetectFinal(p)) != cmOkRC )
    return rc;

  cmSpecDelayInit(&p->phsDelay,2,binCnt);
  cmSpecDelayInit(&p->magDelay,1,binCnt);
  p->binCnt      = binCnt;
  //p->mfp         = cmCtxAllocDebugFile(p->obj.ctx,"complexDetect");
  //p->cdfSpRegId  = cmStatsProcReg( p->obj.ctx->statsProcPtr, kCDF_FId, 1 );

  return cmOkRC;
}

cmRC_t           cmComplexDetectFinal( cmComplexDetect* p)
{
  //if( p != NULL )
  //  cmCtxFreeDebugFile(p->obj.ctx,&p->mfp);

  return cmOkRC;
}

cmRC_t          cmComplexDetectExec( cmComplexDetect* p, const cmSample_t* magV, const cmSample_t* phsV, unsigned binCnt  )
{
  p->out = cmVOS_ComplexDetect( magV, cmSpecDelayOutPtr(&p->magDelay,0), phsV, cmSpecDelayOutPtr(&p->phsDelay,1), cmSpecDelayOutPtr(&p->phsDelay,0), binCnt);
  p->out /= 10000000;

  cmSpecDelayExec(&p->magDelay,magV,binCnt);
  cmSpecDelayExec(&p->phsDelay,phsV,binCnt);

  //if( p->mfp != NULL )
  //  cmMtxFileSmpExec(  p->mfp, &p->out, 1 );

  return cmOkRC;
}

//------------------------------------------------------------------------------------------------------------

cmSample_t _cmComplexOnsetMedian( const cmSample_t* sp, unsigned sn, void* userPtr )
{ return cmVOS_Median(sp,sn); }

cmComplexOnset* cmComplexOnsetAlloc( cmCtx* c, cmComplexOnset* p, unsigned procSmpCnt, double srate, unsigned medFiltWndSmpCnt, double threshold, unsigned frameCnt )
{
  cmComplexOnset* op = cmObjAlloc( cmComplexOnset, c, p );

  if( procSmpCnt > 0 && srate > 0 && medFiltWndSmpCnt > 0 )
    if( cmComplexOnsetInit( op, procSmpCnt, srate, medFiltWndSmpCnt, threshold, frameCnt ) != cmOkRC )
      cmComplexOnsetFree(&op);

  return op;
}

cmRC_t          cmComplexOnsetFree(  cmComplexOnset** pp)
{
  cmRC_t          rc = cmOkRC;
  cmComplexOnset* p  = *pp;  

  if( pp==NULL || *pp == NULL )
    return cmOkRC;

  if((rc = cmComplexOnsetFinal(*pp)) != cmOkRC )
    return rc;

  cmMemPtrFree(&p->df);
  cmMemPtrFree(&p->fdf);
  cmObjFree(pp);

  return cmOkRC;
}

cmRC_t cmComplexOnsetInit( cmComplexOnset* p, unsigned procSmpCnt, double srate,  unsigned medFiltWndSmpCnt, double threshold, unsigned frameCnt )
{

  cmRC_t rc;
  if(( rc = cmComplexOnsetFinal(p)) != cmOkRC )
    return rc;

  p->frmCnt    = frameCnt;
  p->dfi       = 0;
  p->df        = cmMemResizeZ( cmSample_t, p->df,  frameCnt );
  p->fdf       = cmMemResizeZ( cmSample_t, p->fdf, frameCnt );
  p->onrate    = 0;
  p->threshold = threshold;
  p->medSmpCnt = medFiltWndSmpCnt;
  //p->mfp       = cmCtxAllocDebugFile(p->obj.ctx,"complexOnset");
  return cmOkRC;
}

cmRC_t          cmComplexOnsetFinal(  cmComplexOnset* p)
{
  //if( p != NULL )
  //  cmCtxFreeDebugFile(p->obj.ctx,&p->mfp);

  return cmOkRC;
}


cmRC_t          cmComplexOnsetExec(  cmComplexOnset* p, cmSample_t cdf )
{
  p->df[p->dfi++] = cdf;
  return cmOkRC;
}

cmRC_t          cmComplexOnsetCalc(  cmComplexOnset* p )
{
  // df -= mean(df)
  cmVOS_SubVS(p->df,p->frmCnt,cmVOS_Mean(p->df,p->frmCnt));

  // low pass forward/backward filter df[] into fdf[]
  double d = 2 + sqrt(2);
  cmReal_t b[] = {1/d, 2/d, 1/d};
  unsigned bn  = sizeof(b)/sizeof(b[0]);
  cmReal_t a[] = {1, 0, 7-2*d};
  unsigned an  = sizeof(a)/sizeof(a[0]);
  cmFilterFilterS(p->obj.ctx,b,bn,a,an,p->df,p->frmCnt,p->fdf,p->frmCnt);

  // median filter to low-passed filtered fdf[] into df[]
  cmVOS_FnThresh(p->fdf,p->frmCnt,p->medSmpCnt,p->df,1,NULL);

  // subtract med filtered signal from the low passed signal.
  // fdf[] -= df[];
  cmVOS_SubVV(p->fdf,p->frmCnt,p->df);
  cmVOS_SubVS(p->fdf,p->frmCnt,p->threshold);
  cmSample_t *bp = p->df,
             *ep = bp + p->frmCnt - 1,
             *dp = p->fdf + 1;
  *bp++ = *ep = 0;
  for( ; bp<ep; bp++,dp++)
  {
    *bp = (*dp > *(dp-1) && *dp > *(dp+1) && *dp > 0) ? 1 : 0;
    p->onrate += *bp;
  }

  p->onrate /= p->frmCnt;

  /*
  if( p->mfp != NULL )
  {
    bp = p->df;
    ep = bp + p->frmCnt;
    while( bp < ep )
      cmMtxFileSmpExec(  p->mfp, bp++, 1 );
  }
  */

  return cmOkRC;
}

//------------------------------------------------------------------------------------------------------------

cmMfcc* cmMfccAlloc( cmCtx* c, cmMfcc* ap, double srate, unsigned melBandCnt, unsigned dctCoeffCnt, unsigned binCnt )
{
  cmMfcc* p = cmObjAlloc( cmMfcc, c, ap );

  if( melBandCnt > 0 &&  binCnt > 0 && dctCoeffCnt > 0 )
    if( cmMfccInit( p, srate, melBandCnt, dctCoeffCnt, binCnt ) != cmOkRC )
      cmMfccFree(&p);
  return p;
}

cmRC_t  cmMfccFree(  cmMfcc** pp )
{
  cmRC_t rc = cmOkRC;

  if( pp==NULL ||  *pp == NULL )
    return cmOkRC;

  cmMfcc* p = *pp;

  if( (rc = cmMfccFinal(p)) != cmOkRC )
    return rc;

  cmMemPtrFree(&p->melM);
  cmMemPtrFree(&p->dctM);
  cmMemPtrFree(&p->outV);
  cmObjFree(pp);
  
  return rc;
}

cmRC_t  cmMfccInit(  cmMfcc* p, double srate, unsigned melBandCnt, unsigned dctCoeffCnt, unsigned binCnt )
{
  cmRC_t rc;
  if((rc = cmMfccFinal(p)) != cmOkRC )
    return rc;

  p->melM = cmMemResize( cmReal_t, p->melM, melBandCnt  * binCnt );
  p->dctM = cmMemResize( cmReal_t, p->dctM, dctCoeffCnt * melBandCnt );
  p->outV = cmMemResize( cmReal_t, p->outV, dctCoeffCnt );

  // each row of the matrix melp contains a mask
  cmVOR_MelMask(  p->melM, melBandCnt,  binCnt, srate, kShiftMelFl );

  // each row contains melBandCnt elements
  cmVOR_DctMatrix(p->dctM, dctCoeffCnt, melBandCnt ); 

  p->melBandCnt  = melBandCnt;
  p->dctCoeffCnt = dctCoeffCnt;
  p->binCnt      = binCnt;
  p->outN        = dctCoeffCnt;
  //p->mfp         = cmCtxAllocDebugFile(p->obj.ctx,"mfcc");

  //if( p->obj.ctx->statsProcPtr != NULL )
  //  p->mfccSpRegId = cmStatsProcReg( p->obj.ctx->statsProcPtr, kMFCC_FId, p->outN );

  return cmOkRC;
}

cmRC_t  cmMfccFinal( cmMfcc* p )
{
  //if( p != NULL )
  //  cmCtxFreeDebugFile(p->obj.ctx,&p->mfp);

  return cmOkRC;
}

cmRC_t  cmMfccExecPower(  cmMfcc* p, const cmReal_t* magPowV, unsigned binCnt )
{
  assert( binCnt == p->binCnt );

  cmReal_t t[ p->melBandCnt ];

  // apply the mel filter mask to the power spectrum
  cmVOR_MultVMV( t, p->melBandCnt, p->melM, binCnt, magPowV );

  // convert mel bands to dB
  cmVOR_PowerToDb( t, p->melBandCnt, t );
 
  // decorellate the bands with a DCT
  cmVOR_MultVMV( p->outV, p->dctCoeffCnt, p->dctM, p->melBandCnt, t );


  /*
  cmPlotSelectSubPlot(0,0);
  cmPlotClear();
  //cmPlotLineS( "power",  NULL, magPowV, NULL, 35, NULL, kSolidPlotLineId  );    
  cmPlotLineS( "mel",   NULL, t0, NULL, p->melBandCnt, NULL, kSolidPlotLineId  );    

  cmPlotSelectSubPlot(1,0);
  cmPlotClear();
  //cmPlotLineS( "meldb",  NULL, t1, NULL, p->melBandCnt, NULL, kSolidPlotLineId  );    
  cmPlotLineS( "mfcc",  NULL, p->outV+1, NULL, p->dctCoeffCnt-1, NULL, kSolidPlotLineId  );    
  */

  if( p->mfp != NULL )
    cmMtxFileRealExec(p->mfp,p->outV, p->outN);

  

  return cmOkRC;
}

cmRC_t  cmMfccExecAmplitude(  cmMfcc* p, const cmReal_t* magAmpV, unsigned binCnt )
{
  cmReal_t t[ binCnt ];

  cmVOR_MultVVV( t,binCnt, magAmpV, magAmpV );

  cmMfccExecPower(p,t,binCnt);

  if( p->mfp != NULL )
    cmMtxFileRealExec(p->mfp,p->outV, p->outN);

  return cmOkRC;
}

//------------------------------------------------------------------------------------------------------------
enum { cmSonesEqlConBinCnt = kEqualLoudBandCnt, cmSonesEqlConCnt=13 };

cmSones* cmSonesAlloc( cmCtx* c, cmSones*  ap, double srate, unsigned barkBandCnt, unsigned binCnt, unsigned flags )
{
  cmSones* p = cmObjAlloc( cmSones, c, ap );

  if( srate > 0 && barkBandCnt > 0 && binCnt > 0 )
    if( cmSonesInit(p,srate,barkBandCnt,binCnt,flags) != cmOkRC )
      cmSonesFree(&p);
  return p; 
}

cmRC_t   cmSonesFree(  cmSones** pp )
{
  cmRC_t   rc = cmOkRC;
  cmSones* p  = *pp;

  if( pp==NULL || *pp==NULL)
    return cmOkRC;

  if((rc = cmSonesFinal(p)) != cmOkRC )
    return rc;

  cmMemPtrFree(&p->ttmV);
  cmMemPtrFree(&p->sfM);
  cmMemPtrFree(&p->barkIdxV);
  cmMemPtrFree(&p->barkCntV);
  cmMemPtrFree(&p->outV);
  cmObjFree(pp);

  return rc;
}

cmRC_t   cmSonesInit(  cmSones*  p, double srate, unsigned barkBandCnt, unsigned binCnt, unsigned flags )
{
  p->ttmV     = cmMemResize( cmReal_t, p->ttmV,     binCnt);
  p->sfM      = cmMemResize( cmReal_t, p->sfM,      binCnt*barkBandCnt);
  p->barkIdxV = cmMemResize( unsigned, p->barkIdxV, barkBandCnt);
  p->barkCntV = cmMemResize( unsigned, p->barkCntV, barkBandCnt);
  p->outV     = cmMemResize( cmReal_t, p->outV,     barkBandCnt);

  // calc outer ear filter
  cmVOR_TerhardtThresholdMask( p->ttmV, binCnt, srate, kNoTtmFlags );

  // calc shroeder spreading function
  cmVOR_ShroederSpreadingFunc(p->sfM, barkBandCnt, srate);

  // calc the bin to bark maps
  p->barkBandCnt     = cmVOR_BarkMap( p->barkIdxV, p->barkCntV, barkBandCnt, binCnt, srate );  

  //unsigned i = 0;
  //for(; i<barkBandCnt; ++i)
  //  printf("%i %i %i\n", i+1, barkIdxV[i]+1, barkCntV[i]);

  bool     elFl      = cmIsFlag(p->flags, kUseEqlLoudSonesFl);

  p->binCnt          = binCnt;
  p->outN            = elFl ? cmSonesEqlConCnt : p->barkBandCnt;
  p->overallLoudness = 0;
  p->flags           = flags;
  //p->mfp             = cmCtxAllocDebugFile(p->obj.ctx,"sones");
  //p->sonesSpRegId    = cmStatsProcReg( p->obj.ctx->statsProcPtr, kSones_FId, p->outN );
  //p->loudSpRegId     = cmStatsProcReg( p->obj.ctx->statsProcPtr, kLoud_FId,  1 );
  return cmOkRC;  
}

cmRC_t   cmSonesFinal( cmSones*  p )
{
  //if( p != NULL )
  //  cmCtxFreeDebugFile(p->obj.ctx,&p->mfp);

  return cmOkRC;
}



cmRC_t   cmSonesExec(  cmSones*  p, const cmReal_t* magPowV, unsigned binCnt )
{
  assert( binCnt == p->binCnt );

  // Equal-loudness and phon map from: Y. Wonho, 1999, EMBSD: an objective speech quality measure based on audible distortion, 

  // equal-loudness contours
  double eqlcon[cmSonesEqlConCnt][cmSonesEqlConBinCnt] = 
    { 
      {12,7,4,1,0,0,0,-0.5,-2,-3,-7,-8,-8.5,-8.5,-8.5},
      {20,17,14,12,10,9.5,9,8.5,7.5,6.5,4,3,2.5,2,2.5},
      {29,26,23,21,20,19.5,19.5,19,18,17,15,14,13.5,13,13.5},
      {36,34,32,30,29,28.5,28.5,28.5,28,27.5,26,25,24.5,24,24.5},
      {45,43,41,40,40,40,40,40,40,39.5,38,37,36.5,36,36.5},
      {53,51,50,49,48.5,48.5,49,49,49,49,48,47,46.5,45.5,46},
      {62,60,59,58,58,58.5,59,59,59,59,58,57.5,57,56,56},
      {70,69,68,67.5,67.5,68,68,68,68,68,67,66,65.5,64.5,64.5},
      {79,79,79,79,79,79,79,79,78,77.5,76,75,74.5,73,73},
      {89,89,89,89.5,90,90,90,89.5,89,88.5,87,86,85.5,84,83.5},
      {100,100,100,100,100,99.5,99,99,98.5,98,96,95,94.5,93.5,93},
      {112,112,112,112,111,110.5,109.5,109,108.5,108,106,105,104.5,103,102.5},
      {122,122,121,121,120.5,120,119,118,117,116.5,114.5,113.5,113,111, 110.5}
    };

  // loudness levels (phone scales) 
  double phons[cmSonesEqlConCnt]=  {0.0,10.0,20.0,30.0,40.0,50.0,60.0,70.0,80.0,90.0,100.0,110.0,120.0};

  unsigned i,j;

  cmReal_t t0[ binCnt ];
  cmReal_t t1[ p->barkBandCnt ];
  cmReal_t t2[ p->barkBandCnt ];

  unsigned* idxV = p->barkIdxV; 
  unsigned* cntV = p->barkCntV; 
  cmReal_t* sfM  = p->sfM;

  // apply the outer ear filter
  cmVOR_MultVVV( t0, binCnt, magPowV, p->ttmV);


  // apply the bark frequency warping
  for(i=0; i<p->barkBandCnt; ++i)
  {
    if( cntV[i] == 0 )
      t1[i] = 0;
    else
    {
      t1[i] = t0[ idxV[i] ];
      
      for(j=1; j<cntV[i]; ++j)
        t1[i] += t0[ idxV[i] + j ];
    }
  }


  // apply the spreading filters
  cmVOR_MultVMV( t2, p->barkBandCnt, sfM, p->barkBandCnt, t1 );

  bool     elFl      = cmIsFlag(p->flags, kUseEqlLoudSonesFl);
  unsigned bandCnt   = elFl ? cmMin(p->barkBandCnt,cmSonesEqlConBinCnt) : p->barkBandCnt;

  //p->outN            = elFl ? cmSonesEqlConCnt                          : p->barkBandCnt;
  p->overallLoudness = 0;

  for( i = 0; i <bandCnt; i++ )
  {
    // if using the equal-loudness contours begin with the third bark band
    // and end with the 18th bark band
    unsigned k = elFl ? i+3 : i;

    if( k < p->barkBandCnt )
    {
      double v = t2[k];

      // convert to db
      v = 10*log10( v<1 ? 1 : v );

      if( elFl )
      {
        // db to phons 
        // see: Y. Wonho, 1999, EMBSD: an objective speech quality measure based on audible distortion, 

        j = 0;

        // find the equal loudness curve  for this frequency and db level
        while( v >= eqlcon[j][i] )
          ++j;

        if( j == cmSonesEqlConCnt )
        {
          cmCtxRtAssertFailed( &p->obj, cmArgAssertRC, "Bark band %i is out of range during equal-loudness mapping.",j );
          continue;
        }

        // convert db to phons
        if( j == 0 )
          v = phons[0];
        else 
        {
          double t1  = ( v - eqlcon[j-1][i] ) / ( eqlcon[j][i] - eqlcon[j-1][i] );
          v = phons[j-1] + t1 * (phons[j] - phons[j-1]);
        }
      }

      // convert to sones
      // bladon and lindblom, 1981, JASA, modelling the judment of vowel quality differences

      if( v >= 40 )
        p->outV[i] = pow(2,(v-40)/10);
      else
        p->outV[i] = pow(v/40,2.642);
      
      p->overallLoudness += p->outV[i];

    }

  }

  if( p->mfp != NULL )
    cmMtxFileRealExec( p->mfp, p->outV, p->outN );

  return cmOkRC;
}

void cmSonesTest()
{
  cmKbRecd   kb;
  double     srate   = 44100;
  unsigned   bandCnt =    23;
  unsigned   binCnt  =   513;
  cmSample_t tv[ binCnt ];
  cmSample_t sm[ bandCnt * bandCnt ];
  cmSample_t t[  bandCnt * bandCnt ];
  unsigned   binIdxV[ bandCnt ];
  unsigned   cntV[ bandCnt ];
  unsigned i;

  cmPlotSetup("Sones",1,1);

  cmVOS_TerhardtThresholdMask(tv,binCnt,srate, kModifiedTtmFl );
  
  cmVOS_ShroederSpreadingFunc(sm, bandCnt, srate);

  cmVOS_Transpose( t, sm, bandCnt, bandCnt );
 
  bandCnt = cmVOS_BarkMap(binIdxV,cntV, bandCnt, binCnt, srate );

  for(i=0; i<bandCnt; ++i)
    printf("%i %i %i\n", i, binIdxV[i], cntV[i] );

  for(i=0; i<bandCnt; ++i )
  {
    cmPlotLineS( NULL,  NULL,  t+(i*bandCnt), NULL, bandCnt, NULL, kSolidPlotLineId  );    
  }
 
  //cmPlotLineS( NULL,  NULL, tv, NULL, binCnt, NULL, kSolidPlotLineId  );    

  cmPlotDraw();
  cmKeyPress(&kb);    

}


//------------------------------------------------------------------------------------------------------------
  cmAudioOffsetScale* cmAudioOffsetScaleAlloc( cmCtx* c, cmAudioOffsetScale* ap, unsigned procSmpCnt, double srate, cmSample_t offset, double rmsWndSecs, double dBref, unsigned flags )
{
  cmAudioOffsetScale* p = cmObjAlloc( cmAudioOffsetScale, c, ap );


  if( procSmpCnt > 0 && srate > 0 )
    if( cmAudioOffsetScaleInit( p, procSmpCnt, srate, offset, rmsWndSecs, dBref, flags ) != cmOkRC )
      cmAudioOffsetScaleFree(&p);
  return p;
}

cmRC_t        cmAudioOffsetScaleFree(  cmAudioOffsetScale** pp )
{
  cmRC_t              rc = cmOkRC;
  cmAudioOffsetScale* p  = *pp;

  if( pp == NULL || *pp == NULL )
    return cmOkRC;


  if((rc = cmAudioOffsetScaleFinal(p)) != cmOkRC )
    return rc;

  cmMemPtrFree(&p->cBufPtr);
  cmMemPtrFree(&p->cCntPtr);
  cmMemPtrFree(&p->outV);

  cmObjFree(pp);

  return rc;
}

cmRC_t        cmAudioOffsetScaleInit(  cmAudioOffsetScale* p, unsigned procSmpCnt, double srate, cmSample_t offset, double rmsWndSecs, double dBref, unsigned flags )
{
  assert( procSmpCnt > 0 && srate > 0);

  cmRC_t rc;
  if((rc = cmAudioOffsetScaleFinal(p)) != cmOkRC )
    return rc;

  p->cBufCnt = 0;

  if( cmIsFlag(flags, kRmsAudioScaleFl) )
  {
    if( rmsWndSecs > 0 )
    {
      unsigned rmsSmpCnt = srate * rmsWndSecs;
      p->cBufCnt =  (unsigned)ceil( rmsSmpCnt / procSmpCnt );

      if( p->cBufCnt > 0 )
      {
        p->cBufPtr = cmMemResizeZ( cmReal_t, p->cBufPtr, p->cBufCnt );
        p->cCntPtr = cmMemResizeZ( unsigned, p->cCntPtr, p->cBufCnt );
      }
    }
    else
    {
      p->cBufCnt = 0;
      p->cBufPtr = NULL;
      p->cCntPtr = NULL;
    }
  }

  p->cBufIdx    = 0;
  p->cBufCurCnt = 0;
  p->cBufSum    = 0;
  p->cCntSum    = 0;
  p->outV       = cmMemResize( cmSample_t, p->outV, procSmpCnt );
  p->outN       = procSmpCnt;
  p->offset     = offset;
  p->dBref      = dBref;
  p->flags      = flags;
  //p->mfp        = cmCtxAllocDebugFile(p->obj.ctx,"audioOffsetScale");
  return cmOkRC;

}

cmRC_t        cmAudioOffsetScaleFinal( cmAudioOffsetScale* p )
{
  //if( p != NULL)
  //  cmCtxFreeDebugFile(p->obj.ctx,&p->mfp);

  return cmOkRC;
}

cmRC_t        cmAudioOffsetScaleExec(  cmAudioOffsetScale* p, const cmSample_t* sp, unsigned sn )
{

  double            Pref    = 20.0 / 1000000; // 20 micro Pascals
  cmSample_t*       dbp     = p->outV;
  const cmSample_t* dep     = dbp + sn;
  double            scale   = 0;


  // if no scaling was requested then add offset only
  if( cmIsFlag(p->flags, kNoAudioScaleFl) )
  {
    while( dbp < dep )
      *dbp++ = *sp++ + p->offset;
    goto doneLabel;
  }

  // if fixed scaling
  if( cmIsFlag(p->flags, kFixedAudioScaleFl) )
  {
    if( scale == 0 )
      scale = pow(10,p->dBref/20);

    while( dbp < dep )
      *dbp++ = (*sp++ + p->offset) * scale;

  }
  else // if RMS scaling
  {
    double sum = 0;
    double rms = 0;


    while( dbp < dep )
    {
      double v = (*sp++ + p->offset) / Pref;

      sum += v*v;

      *dbp++ = v;
    }

    // if there is no RMS buffer calc RMS on procSmpCnt samles
    if( p->cBufCnt == 0 )
      rms = sqrt( sum / sn );
    else
    {
      p->cBufSum   -= p->cBufPtr[ p->cBufIdx ];
      p->cBufSum   += sum;
      p->cCntSum   -= p->cCntPtr[ p->cBufIdx ];
      p->cCntSum   += sn;
      p->cBufIdx    = (p->cBufIdx+1) % p->cBufCnt;
      p->cBufCurCnt = cmMin( p->cBufCurCnt+1, p->cBufCnt );
 
      assert( p->cCntSum > 0 );

      rms           = sqrt( p->cBufSum / p->cCntSum );
    }

    double sigSPL = 20*log10(rms);

    scale   = pow(10,(p->dBref - sigSPL)/20); 

    dbp = p->outV;
    while( dbp < dep )
      *dbp++ *= scale;

  }

 doneLabel:
  dbp = p->outV + sn;
  dep = p->outV + p->outN;
  while( dbp < dep )
    *dbp++ = 0;

  if( p->mfp != NULL )
    cmMtxFileSmpExec(p->mfp,p->outV,p->outN);

  return cmOkRC;

}

//------------------------------------------------------------------------------------------------------------

cmSpecMeas* cmSpecMeasAlloc( cmCtx* c, cmSpecMeas* ap, double srate, unsigned binCnt, unsigned wndFrmCnt, unsigned flags )
{
  cmSpecMeas* p = cmObjAlloc( cmSpecMeas, c, ap );

  if( srate > 0 && binCnt > 0 )
    if( cmSpecMeasInit( p, srate, binCnt, wndFrmCnt, flags ) != cmOkRC )
      cmSpecMeasFree(&p);
  return p;
}

cmRC_t      cmSpecMeasFree(  cmSpecMeas** pp )
{
  cmRC_t      rc = cmOkRC;
  cmSpecMeas* p  = *pp;

  if( pp == NULL || *pp == NULL )
    return cmOkRC;

  if((rc = cmSpecMeasFinal(p)) != cmOkRC )
    return rc;

  cmMemPtrFree(&p->rmsV);
  cmMemPtrFree(&p->hfcV);
  cmMemPtrFree(&p->scnV);
  cmObjFree(pp);
  return rc;

}

cmRC_t      cmSpecMeasInit(  cmSpecMeas* p, double srate, unsigned binCnt, unsigned wndFrmCnt, unsigned flags )
{
  cmRC_t rc;
  if((rc = cmSpecMeasFinal(p)) != cmOkRC )
    return rc;

  if( cmIsFlag(flags, kUseWndSpecMeasFl) )
  {
    p->rmsV = cmMemResizeZ( cmReal_t, p->rmsV, wndFrmCnt );
    p->hfcV = cmMemResizeZ( cmReal_t, p->hfcV, wndFrmCnt );
    p->scnV = cmMemResizeZ( cmReal_t, p->scnV, wndFrmCnt );
  }

  p->rmsSum     = 0;
  p->hfcSum     = 0;
  p->scnSum     = 0;

  p->binCnt     = binCnt;
  p->flags      = flags;
  p->wndFrmCnt  = wndFrmCnt;
  p->frameCnt   = 0;
  p->frameIdx   = 0;
  p->binHz      = srate / ((binCnt-1) * 2);
  //p->mfp        = cmCtxAllocDebugFile(p->obj.ctx,"specMeas");

  //p->rmsSpRegId = cmStatsProcReg( p->obj.ctx->statsProcPtr, kRMS_FId, 1);
  //p->hfcSpRegId = cmStatsProcReg( p->obj.ctx->statsProcPtr, kHFC_FId, 1);
  //p->scSpRegId  = cmStatsProcReg( p->obj.ctx->statsProcPtr, kSC_FId,  1);
  //p->ssSpRegId  = cmStatsProcReg( p->obj.ctx->statsProcPtr, kSS_FId,  1);

  return cmOkRC;
}

cmRC_t      cmSpecMeasFinal( cmSpecMeas* p )
{
  //if( p != NULL )
  //  cmCtxFreeDebugFile(p->obj.ctx,&p->mfp);

  return cmOkRC;
}

cmRC_t      cmSpecMeasExec(  cmSpecMeas* p, const cmReal_t* magPowV, unsigned binCnt )
{
  assert( binCnt == p->binCnt );

  unsigned        i      = 0;
  const cmReal_t* sbp    = magPowV;
  const cmReal_t* sep    = sbp + binCnt;
  cmReal_t        rmsSum = 0;
  cmReal_t        hfcSum = 0;
  cmReal_t        scnSum = 0;
  cmReal_t        ssSum  = 0;

  for(i=0; sbp < sep; ++i, ++sbp )
  {
    rmsSum += *sbp;
    hfcSum += *sbp * i;
    scnSum += *sbp * i * p->binHz;
  }

  p->frameCnt++;

  if( cmIsFlag(p->flags, kUseWndSpecMeasFl) )
  {
    p->frameCnt = cmMin( p->frameCnt, p->wndFrmCnt );
    
    cmReal_t* rmsV = p->rmsV + p->frameIdx;
    cmReal_t* hfcV = p->hfcV + p->frameIdx; 
    cmReal_t* scnV = p->scnV + p->frameIdx; 

    p->rmsSum -= *rmsV;
    p->hfcSum -= *hfcV;
    p->scnSum -= *scnV;

    *rmsV = rmsSum;
    *hfcV = hfcSum;
    *scnV = scnSum;

    p->frameIdx = (p->frameIdx+1) % p->frameCnt;    

  }

  p->rmsSum += rmsSum;
  p->hfcSum += hfcSum;
  p->scnSum += scnSum;


  p->rms    = sqrt(p->rmsSum / (p->binCnt * p->frameCnt) );
  p->hfc    = p->hfcSum / ( p->binCnt * p->frameCnt );
  p->sc     = p->scnSum / cmMax( cmReal_EPSILON, p->rmsSum );

    
  sbp = magPowV;
  for(i=0; sbp < sep; ++i )
  {
    cmReal_t t = (i*p->binHz) - p->sc;
    ssSum += *sbp++ * (t*t);
  }

  p->ss = sqrt(ssSum / cmMax( cmReal_EPSILON, p->rmsSum ));
  
  if( p->mfp != NULL )
  {
    cmReal_t a[4] = { p->rms, p->hfc, p->sc, p->ss };
    cmMtxFileRealExec(  p->mfp, a, 4 );
  }

  return cmOkRC;
}

//------------------------------------------------------------------------------------------------------------
cmSigMeas* cmSigMeasAlloc( cmCtx* c, cmSigMeas* ap, double srate, unsigned procSmpCnt, unsigned measSmpCnt )
{
  cmSigMeas* p = cmObjAlloc( cmSigMeas, c, ap );

  p->sbp = cmShiftBufAlloc(c,&p->shiftBuf,0,0,0);

  if( srate > 0 && procSmpCnt > 0 && measSmpCnt > 0 )
    if( cmSigMeasInit( p, srate, procSmpCnt, measSmpCnt ) != cmOkRC )
      cmSigMeasFree(&p);
  return p;
}

cmRC_t     cmSigMeasFree(  cmSigMeas** pp )
{
  cmRC_t rc = cmOkRC;
  cmSigMeas* p = *pp;

  if( pp==NULL || *pp==NULL)
    return cmOkRC;

  if((rc = cmSigMeasFinal(p)) != cmOkRC )
    return rc;

  cmShiftBufFree(&p->sbp);
  
  cmObjFree(pp);
  return rc;
}

cmRC_t     cmSigMeasInit(  cmSigMeas* p, double srate, unsigned procSmpCnt, unsigned measSmpCnt )
{
  cmRC_t rc;

  if((rc = cmSigMeasFinal(p)) != cmOkRC )
    return rc;

  if( procSmpCnt != measSmpCnt )
    cmShiftBufInit( p->sbp, procSmpCnt, measSmpCnt, procSmpCnt );

  p->zcrDelay   = 0;
  p->srate      = srate;
  p->measSmpCnt = measSmpCnt;
  p->procSmpCnt = procSmpCnt;
  //p->zcrSpRegId = cmStatsProcReg( p->obj.ctx->statsProcPtr, kZCR_FId, 1 );
  //p->mfp        = cmCtxAllocDebugFile(p->obj.ctx,"sigMeas");

  return cmOkRC;
}

cmRC_t     cmSigMeasFinal( cmSigMeas* p )
{
  //if( p != NULL )
  //  cmCtxFreeDebugFile(p->obj.ctx,&p->mfp);

  return cmOkRC;
}

cmRC_t     cmSigMeasExec(  cmSigMeas* p, const cmSample_t* sp, unsigned sn )
{  
  if( p->procSmpCnt != p->measSmpCnt )
  {
    cmShiftBufExec( p->sbp, sp, sn );
    sp = p->sbp->outV;
    sn = p->sbp->wndSmpCnt;
    assert( p->sbp->wndSmpCnt == p->measSmpCnt );
  }

  unsigned zcn  =    cmVOS_ZeroCrossCount( sp, sn, NULL );
  p->zcr = (cmReal_t)zcn * p->srate / p->measSmpCnt;

  if( p->mfp != NULL )
    cmMtxFileRealExec(  p->mfp, &p->zcr, 1 );

  return cmOkRC;
}


//------------------------------------------------------------------------------------------------------------
cmSRC* cmSRCAlloc( cmCtx* c, cmSRC* ap, double srate, unsigned procSmpCnt, unsigned upFact, unsigned dnFact )
{
  cmSRC* p = cmObjAlloc( cmSRC, c,ap );

  cmFilterAlloc(c,&p->filt,NULL,0,NULL,0,0,NULL);
 
  if( srate > 0 && procSmpCnt > 0 )
    if( cmSRCInit( p, srate, procSmpCnt, upFact, dnFact ) != cmOkRC )
      cmSRCFree(&p);

  return p;
}

cmRC_t cmSRCFree(  cmSRC** pp )
{
  cmRC_t rc;

  if( pp != NULL && *pp != NULL )
  {
    cmSRC* p = *pp;

    if((rc = cmSRCFinal( p )) == cmOkRC )
    {
      cmFilter* fp = &p->filt;
      cmFilterFree(&fp);
      cmMemPtrFree(&p->outV);
      cmObjFree(pp);
    }
  }
  return cmOkRC;
}

cmRC_t cmSRCInit(  cmSRC* p, double srate, unsigned procSmpCnt, unsigned upFact, unsigned dnFact )
{
  cmRC_t rc;

  if((rc = cmSRCFinal(p)) != cmOkRC )
    return rc;

  double hiRate = upFact * srate;
  double loRate = hiRate / dnFact;
  double minRate= cmMin( loRate, srate ); 
  double fcHz   = minRate/2;
  double dHz    = (fcHz * .1); // transition band is 5% of min sample rate 
  double passHz = fcHz-dHz;
  double stopHz = fcHz;
  double passDb = 0.001;
  double stopDb = 20;
 
  cmFilterInitEllip( &p->filt, hiRate, passHz, stopHz,  passDb, stopDb, procSmpCnt, NULL );

  //printf("CoeffCnt:%i dHz:%f passHz:%f stopHz:%f passDb:%f stopDb:%f\n", p->fir.coeffCnt, dHz, passHz, stopHz, passDb, stopDb );

  p->outN   = (unsigned)ceil(procSmpCnt * upFact / dnFact); 
  p->outV   = cmMemResize( cmSample_t, p->outV, p->outN );  
  p->upi    = 0;
  p->dni    = 0;
  p->upFact = upFact;
  p->dnFact = dnFact;
  //p->mfp    = cmCtxAllocDebugFile(p->obj.ctx,"src");
  return cmOkRC;
}

cmRC_t cmSRCFinal( cmSRC* p )
{
  //if( p != NULL )
  //  cmCtxFreeDebugFile(p->obj.ctx,&p->mfp);

  return cmOkRC;
}

cmRC_t cmSRCExec(  cmSRC* p, const cmSample_t* sp, unsigned sn )
{
  const cmSample_t* sep = sp + sn;
  cmSample_t*       op  = p->outV;
  const cmSample_t* oep = op + p->outN;
  unsigned          iN  = sn * p->upFact;
  unsigned          i,j; 

  // run the filter at the upsampled rate ...
  for(i=0; i<iN; ++i) 
  {
    assert( p->upi!=0 || sp<sep ); 

    cmSample_t x0 = p->upi==0 ? *sp++ : 0,
               y0 = x0 * p->filt.b0 + p->filt.d[0];

    // ... but output at the down sampled rate
    if( p->dni==0 )
    {
      assert( op < oep );
      *op++ = y0;
    }

    // advance the filter delay line
    for(j=0; j<p->filt.cn; ++j)
      p->filt.d[j] = p->filt.b[j]*x0 - p->filt.a[j]*y0 + p->filt.d[j+1];

    // update the input/output clocks
    p->upi = (p->upi + 1) % p->upFact;
    p->dni = (p->dni + 1) % p->dnFact;
    
  }

  p->outN = op - p->outV;

  if( p->mfp != NULL )
    cmMtxFileSmpExec(p->mfp,p->outV,p->outN );

  return cmOkRC;
}


//------------------------------------------------------------------------------------------------------------
cmConstQ* cmConstQAlloc( cmCtx* c, cmConstQ* ap, double srate, unsigned minMidiPitch, unsigned maxMidiPitch, unsigned binsPerOctave, double thresh )
{
  cmConstQ* p = cmObjAlloc( cmConstQ, c, ap );

  if( srate >0  )
    if( cmConstQInit(p,srate,minMidiPitch,maxMidiPitch,binsPerOctave,thresh) != cmOkRC )
      cmConstQFree(&p);
  return p;
}

cmRC_t    cmConstQFree(  cmConstQ** pp )
{
  cmRC_t    rc;
  cmConstQ* p = *pp;

  if( pp==NULL || *pp==NULL)
    return cmOkRC;

  if((rc = cmConstQFinal(p)) != cmOkRC )
    return rc;
  
  cmMemPtrFree(&p->fiV);
  cmMemPtrFree(&p->foV);
  cmMemPtrFree(&p->skM);
  cmMemPtrFree(&p->outV);
  cmMemPtrFree(&p->magV);
  cmMemPtrFree(&p->skBegV);
  cmMemPtrFree(&p->skEndV);
  cmObjFree(pp);

  return cmOkRC;
}



cmRC_t    cmConstQInit(  cmConstQ* p, double srate, unsigned minMidiPitch, unsigned maxMidiPitch, unsigned binsPerOctave, double thresh )
{
  cmRC_t rc;
  if((rc = cmConstQFinal(p)) != cmOkRC )
    return rc;

  cmReal_t        minHz = cmMidiToHz(minMidiPitch);
  cmReal_t        maxHz = cmMidiToHz(maxMidiPitch);
  cmReal_t        Q     = 1.0/(pow(2,(double)1.0/binsPerOctave)-1);
  unsigned        K     = (unsigned)ceil( binsPerOctave * log2(maxHz/minHz) );
  unsigned        fftN  = cmNextPowerOfTwo( (unsigned)ceil(Q*srate/minHz) );
  unsigned        k     = 0;
  p->fiV                = cmMemResize(cmComplexR_t, p->fiV, fftN);
  p->foV                = cmMemResize(cmComplexR_t, p->foV, fftN);
  
  cmFftPlanR_t     plan = cmFft1dPlanAllocR(fftN, p->fiV, p->foV, FFTW_FORWARD, FFTW_ESTIMATE );

  p->wndSmpCnt    = fftN;
  p->constQBinCnt = K;
  p->binsPerOctave= binsPerOctave;
  p->skM          = cmMemResizeZ( cmComplexR_t, p->skM,     p->wndSmpCnt * p->constQBinCnt);
  p->outV         = cmMemResizeZ( cmComplexR_t, p->outV,    p->constQBinCnt);
  p->magV         = cmMemResizeZ( cmReal_t,     p->magV,    p->constQBinCnt);
  p->skBegV       = cmMemResizeZ( unsigned,     p->skBegV,  p->constQBinCnt);
  p->skEndV       = cmMemResizeZ( unsigned,     p->skEndV,  p->constQBinCnt);


  //p->mfp          = cmCtxAllocDebugFile( p->obj.ctx, "constQ");

  //printf("hz:%f %f bpo:%i sr:%f thresh:%f Q:%f K%i (cols)  fftN:%i (rows)\n", minHz,maxHz,binsPerOctave,srate,thresh,Q,K,fftN);

  double* hamm = NULL;

  // note that the bands are created in reverse order 
  for(k=0; k<K; ++k)
  {
    unsigned iN    = ceil( Q * srate / (minHz * pow(2,(double)k/binsPerOctave)));
    unsigned start = fftN/2;
    //double   hamm[ iN ];
    hamm = cmMemResizeZ(double,hamm,iN);

    memset( p->fiV, 0, fftN * sizeof(cmComplexR_t));
    memset( p->foV, 0, fftN * sizeof(cmComplexR_t));

    cmVOD_Hamming( hamm, iN );

    cmVOD_DivVS(  hamm, iN, iN );

    if( cmIsEvenU(iN) )
      start -=  iN/2;
    else
      start -= (iN+1)/2;

    //printf("k:%i iN:%i start:%i %i\n",k,iN,start,start+iN-1);

    unsigned i = start;
    for(; i<=start+iN-1; ++i)
    {
      double arg = 2.0*M_PI*Q*(i-start)/iN;
      double mag = hamm[i-start];

      p->fiV[i-1]   = (mag * cos(arg)) + (mag * I * sin(arg));       
    }
    
    cmFftExecuteR(plan);

    // since the bands are created in reverse order they are also stored in reverse order
    // (i.e column k-1 is stored first and column 0 is stored last)
    i=0;
    unsigned minIdx = -1;
    unsigned maxIdx = 0;
    for(; i<fftN; ++i)
    {
      bool fl = cabs(p->foV[i]) <= thresh;

      p->skM[ (k*p->wndSmpCnt) + i ] =  fl ? 0 : p->foV[i]/fftN;

      if( fl==false && minIdx == -1 )
        minIdx = i;

      if( fl==false && i>maxIdx )
        maxIdx = i;      
    }

    p->skBegV[k] = minIdx;
    p->skEndV[k] = maxIdx;

  }

  cmMemPtrFree(&hamm);

  cmFftPlanFreeR(plan);

 
  return cmOkRC;
}

cmRC_t    cmConstQFinal( cmConstQ* p )
{
  //if( p != NULL )
  //  cmCtxFreeDebugFile(p->obj.ctx,&p->mfp);

  return cmOkRC;
}

cmRC_t    cmConstQExec(  cmConstQ* p, const cmComplexR_t* ftV, unsigned srcBinCnt )
{
  //acVORC_MultVVM( p->outV, p->constQBinCnt,ftV,p->wndSmpCnt, p->skM );
  
  cmReal_t*            mbp = p->magV;
  cmComplexR_t*        dbp = p->outV;
  const cmComplexR_t*  dep = p->outV + p->constQBinCnt;
  
  unsigned i = 0;

  for(; dbp < dep; ++dbp,++i,++mbp )
  {
    const cmComplexR_t*    sbp = ftV + p->skBegV[i];
    const cmComplexR_t*    kp  = p->skM + (i*p->wndSmpCnt) + p->skBegV[i];
    const cmComplexR_t*    ep  = kp + (p->skEndV[i] - p->skBegV[i]) + 1;

    *dbp = 0;
    while( kp < ep )
      *dbp +=   *sbp++ * *kp++; 

    *mbp = cmCabsR(*dbp);
  }
 

  if( p->mfp != NULL )
    cmMtxFileComplexExec(  p->mfp, p->outV, p->constQBinCnt, 1 );

  return cmOkRC;
 
}

void cmConstQTest( cmConstQ* p )
{
  cmKbRecd kb;

  unsigned i,j;
  cmSample_t* t = cmMemAlloc( cmSample_t, p->wndSmpCnt );

  for(i=0; i<p->constQBinCnt; ++i)
  {
    for(j=0; j<p->wndSmpCnt; ++j)
      t[j] = cabs( p->skM[ (i*p->wndSmpCnt) + j ]);


    //cmPlotClear();
    cmPlotLineS( NULL,  NULL, t, NULL, 500, NULL, kSolidPlotLineId  );    
  }

  cmPlotDraw();
  cmKeyPress(&kb);

  cmMemPtrFree(&t);

}



//------------------------------------------------------------------------------------------------------------
cmHpcp* cmTunedHpcpAlloc( cmCtx* c, cmHpcp* ap, unsigned binsPerOctave, unsigned constQBinCnt, unsigned cqMinMidiPitch, unsigned frameCnt, unsigned medFiltOrder )
{
  cmHpcp* p = cmObjAlloc( cmHpcp, c, ap );

  if( binsPerOctave > 0 && constQBinCnt >> 0 )
    if( cmTunedHpcpInit( p, binsPerOctave, constQBinCnt, cqMinMidiPitch, frameCnt, medFiltOrder ) != cmOkRC)
      cmTunedHpcpFree(&p);
  return p;
}

cmRC_t         cmTunedHpcpFree(  cmHpcp** pp )
{
  cmRC_t rc = cmOkRC;
  cmHpcp* p = *pp;

  if(pp==NULL || *pp==NULL)
    return cmOkRC;

  if((rc = cmTunedHpcpFinal(p)) != cmOkRC )
    return rc;
  
  cmMemPtrFree(&p->hpcpM);
  cmMemPtrFree(&p->fhpcpM);
  cmMemPtrFree(&p->histV);
  cmMemPtrFree(&p->outM);
  cmMemPtrFree(&p->meanV);
  cmMemPtrFree(&p->varV);

  cmObjFree(pp);

  return cmOkRC;
}

cmRC_t         cmTunedHpcpInit(  cmHpcp* p, unsigned binsPerOctave, unsigned constQBinCnt, unsigned cqMinMidiPitch, unsigned frameCnt, unsigned medFiltOrder )
{
  assert( binsPerOctave % kStPerOctave == 0 );
  assert( cmIsOddU( binsPerOctave / kStPerOctave ) );

  cmRC_t rc;
  if((rc = cmTunedHpcpFinal(p)) != cmOkRC )
    return rc;

  p->histN  = binsPerOctave/kStPerOctave;
  p->hpcpM  = cmMemResizeZ( cmReal_t, p->hpcpM,  frameCnt*binsPerOctave );
  p->fhpcpM = cmMemResizeZ( cmReal_t, p->fhpcpM, binsPerOctave*frameCnt );
  p->histV  = cmMemResizeZ( unsigned, p->histV,  p->histN );
  p->outM   = cmMemResizeZ( cmReal_t, p->outM,   kStPerOctave * frameCnt );
  p->meanV  = cmMemResizeZ( cmReal_t, p->meanV,  kStPerOctave );
  p->varV   = cmMemResizeZ( cmReal_t, p->varV,   kStPerOctave );

  p->constQBinCnt  = constQBinCnt;
  p->binsPerOctave = binsPerOctave;
  p->frameCnt      = frameCnt;
  p->frameIdx      = 0;
  p->cqMinMidiPitch= cqMinMidiPitch;
  p->medFiltOrder  = medFiltOrder;
  //p->mf0p          = cmCtxAllocDebugFile(p->obj.ctx,"hpcp");
  //p->mf1p          = cmCtxAllocDebugFile(p->obj.ctx,"fhpcp");
  //p->mf2p          = cmCtxAllocDebugFile(p->obj.ctx,"chroma");
  return cmOkRC;
}

cmRC_t         cmTunedHpcpFinal( cmHpcp* p )
{
  /*
  if( p != NULL )
  {
    cmCtxFreeDebugFile(p->obj.ctx,&p->mf0p);
    cmCtxFreeDebugFile(p->obj.ctx,&p->mf1p);
    cmCtxFreeDebugFile(p->obj.ctx,&p->mf2p);
  }
  */
  return cmOkRC;
}

cmRC_t         cmTunedHpcpExec(  cmHpcp* p, const cmComplexR_t* cqp, unsigned cqn )
{
  assert( cqn == p->constQBinCnt );

  // if there is no space to store the output then do nothing
  if( p->frameIdx >= p->frameCnt )
    return cmOkRC;

  unsigned        octCnt = (unsigned)floor(p->constQBinCnt / p->binsPerOctave);
  unsigned        i;
  cmReal_t        hpcpV[ p->binsPerOctave + 2 ];
  unsigned        idxV[ p->binsPerOctave ];
  unsigned        binsPerSt = p->binsPerOctave / kStPerOctave;
  

  // Notice that the first and last elements of p->hpcp are reserved for
  // use in producing the appeareance of circularity for the peak picking
  // algorithm.  The cmtual hpcp[] data begins on the index 1 (not 0) and 
  // ends on p->binsPerOctave (not p->binsPerOctave-1).
    
  // sum the constQBinCnt constant Q bins into binsPerOctave bins to form the HPCP
  for(i=0; i<p->binsPerOctave; ++i)
  {
    cmReal_t           sum = 0;
    const cmComplexR_t* sbp = cqp + i;
    const cmComplexR_t* sep = cqp + (octCnt * p->binsPerOctave);

    for(; sbp < sep; sbp += p->binsPerOctave)
      sum += cmCabsR(*sbp);

    hpcpV[i+1] = sum;
  }

  // shift the lowest ST center bin to (binsPerSt+1)/2 such that an equal number of
  // flat and sharp bins are above an below it
  int rotateCnt = ((binsPerSt+1)/2) - 1;
  
  // shift pitch class C to the lowest semitone boundary 
  rotateCnt -= ( 48-(int)p->cqMinMidiPitch) * binsPerSt;

  // perform the shift
  cmVOR_Rotate(hpcpV+1, p->binsPerOctave, rotateCnt);

  // duplicate the first and last bin to produce circularity in the hpcp
  hpcpV[0]                    = hpcpV[ p->binsPerOctave ];
  hpcpV[ p->binsPerOctave+1 ] = hpcpV[1];

  // locate the indexes of the positive peaks in the hpcp
  unsigned pkN       = cmVOR_PeakIndexes( idxV, p->binsPerOctave, hpcpV, p->binsPerOctave, 0 );

  // Convert the peak indexes to values in the range 0 to binsPerSet-1
  // If stPerBin == 3 :  0=flat 1=in tune 2=sharp
  cmVOU_Mod( idxV, pkN, binsPerSt );
  
  // Form a histogram to keep count of the number of flat,in-tune and sharp peaks
  cmVOU_Hist( p->histV, binsPerSt, idxV, pkN );

  // store the hpcpV[] to the row p->hpcpM[p->frameIdx,:]
  cmVOR_CopyN( p->hpcpM + p->frameIdx, p->binsPerOctave, p->frameCnt, hpcpV+1, 1 );
    
  // write the hpcp debug file
  if( p->mf0p != NULL )
    cmMtxFileRealExecN(  p->mf0p, p->hpcpM + p->frameIdx, p->binsPerOctave, p->frameCnt );

  p->frameIdx++; 
  
  return cmOkRC;

}

cmRC_t         cmTunedHpcpTuneAndFilter( cmHpcp* p)
{
  // note: p->frameIdx now holds the cmtual count of frames in p->hpcpA[].
  //       p->frameCnt holds the allocated count of frames in p->hpcpA[].

  unsigned i,j;

  // filter each column of hpcpA[] into each row of fhpcpA[]
  for(i=0; i<p->binsPerOctave; ++i)
  {
    cmVOR_MedianFilt( p->hpcpM + (i * p->frameCnt), p->frameIdx, p->medFiltOrder, p->fhpcpM + i, p->binsPerOctave );  

    // write the fhpcp[i,:] to the debug file
    if( p->mf1p != NULL )
      cmMtxFileRealExecN(  p->mf1p, p->fhpcpM + i, p->frameIdx, p->binsPerOctave );

  }

  unsigned binsPerSt = p->histN;

  assert( (binsPerSt > 0) && (cmIsOddU(binsPerSt)) );

  
  unsigned maxIdx    = cmVOU_MaxIndex(p->histV,binsPerSt,1);
  int      tuneShift = -(maxIdx - ((binsPerSt+1)/2));
  cmReal_t gaussWndV[ binsPerSt ];

  // generate a gaussian window
  cmVOR_GaussWin( gaussWndV, binsPerSt, 2.5 );

  // Rotate the window to apply tuning via the weighted sum operation below
  // (the result will be equivalent to rotating p->fhpcpM[] prior to reducing the )
  cmVOR_Rotate(gaussWndV, binsPerSt, tuneShift);

  // zero the meanV[] before summing into it
  cmVOR_Fill(p->meanV,kStPerOctave,0);

  // for each frame
  for(i=0; i<p->frameIdx; ++i)
  {
    // for each semitone
    for(j=0; j<kStPerOctave; ++j)
    {
      // reduce each semitone to a single value by forming a weighted sum of all the assoc'd bins
      cmReal_t sum = cmVOR_MultSumVV( gaussWndV, p->fhpcpM + (i*p->binsPerOctave) + (j*binsPerSt), binsPerSt );

      // store time-series output to the ith column
      p->outM[ (i*kStPerOctave) + j ] = sum;

      // calc the sum of all chroma values in bin j. 
      p->meanV[ j ] += sum;
    }      

    // write the chroma debug file
    if( p->mf2p != NULL )
      cmMtxFileRealExec( p->mf2p, p->outM + (i*kStPerOctave), kStPerOctave );

  }

  // form the chroma mean from the sum calc'd above
  cmVOR_DivVS( p->meanV, kStPerOctave, p->frameIdx );

  // variance
  for(j=0; j<kStPerOctave; ++j)
    p->varV[j] = cmVOR_VarianceN( p->outM + j, p->frameIdx, kStPerOctave, p->meanV + j );


  return cmOkRC;    
}


//------------------------------------------------------------------------------------------------------------
/*
cmStatsProc* cmStatsProcAlloc( cmCtx* c, cmStatsProc* p, unsigned wndEleCnt, unsigned flags )
{
  cmStatsProc* op = cmObjAlloc(cmStatsProc,c,p);

  if( wndEleCnt > 0 )
    if( cmStatsProcInit(op, wndEleCnt, flags ) != cmOkRC )
      cmStatsProcFree(&op);
  
  if( op != NULL )
    cmCtxSetStatsProc(c,op);

  return op;
}


cmRC_t       cmStatsProcFree(  cmStatsProc** pp )
{
  cmRC_t       rc  = cmOkRC;
  cmStatsProc* p   = *pp;

  if( pp == NULL || *pp == NULL )
    return cmOkRC;

  if((rc = cmStatsProcFinal(p) ) != cmOkRC )
    return rc;

  cmCtxSetStatsProc(p->obj.ctx,NULL); // complement to cmSetStatsProc() in cmStatsProcAlloc()

  cmMemPtrFree(&p->regArrayV);
  cmMemPtrFree(&p->m);
  cmMemPtrFree(&p->sumV);
  cmMemPtrFree(&p->meanV);
  cmMemPtrFree(&p->varV);
  cmObjFree(pp);

  return cmOkRC;    
}

cmRC_t       cmStatsProcInit(  cmStatsProc* p, unsigned wndEleCnt, unsigned flags )
{
  cmRC_t rc;

  if((rc = cmStatsProcFinal(p)) != cmOkRC )
    return rc;

  p->wndEleCnt = wndEleCnt;
  p->dimCnt    = 0;
  p->curIdx    = 0;
  p->curCnt    = 1;
  p->flags     = flags;
  p->execCnt   = 0;
  p->regArrayN = 0;
  //p->mfp       = cmCtxAllocDebugFile(p->obj.ctx,"statsProc");
  return rc;
}

cmRC_t cmStatsProcFinal( cmStatsProc* p )
{
  if( p != NULL )
    cmCtxFreeDebugFile(p->obj.ctx,&p->mfp);

  return cmOkRC;
}

unsigned     cmStatsProcReg(    cmStatsProc* p, unsigned featId, unsigned featEleCnt )
{
  cmStatsProcRecd r;

  r.featId = featId;
  r.idx    = p->dimCnt;
  r.cnt    = featEleCnt;

  p->dimCnt += featEleCnt;

  //unsigned i;
  // printf("B:\n");
  //for(i=0; i<p->regArrayN; ++i)
  //  printf("fid:%i idx:%i cnt:%i : fid:%i cnt:%i\n", p->regArrayV[i].featId,p->regArrayV[i].idx,p->regArrayV[i].cnt,featId,featEleCnt);
  //printf("\nA:\n");
  
  p->regArrayV = cmMemResizePZ( cmStatsProcRecd, p->regArrayV, p->regArrayN+1 );
  p->regArrayV[ p->regArrayN ] = r;
  p->regArrayN++;


  //for(i=0; i<p->regArrayN; ++i)
  //  printf("fid:%i idx:%i cnt:%i : fid:%i cnt:%i\n", p->regArrayV[i].featId,p->regArrayV[i].idx,p->regArrayV[i].cnt,featId,featEleCnt);
  //printf("\n");

  return  p->regArrayN-1; // return the index of the new reg recd
}

const cmStatsProcRecd* cmStatsProcRecdPtr( cmStatsProc* p, unsigned regId )
{
  assert( regId < p->regArrayN );
  return  p->regArrayV + regId;
}

cmRC_t       cmStatsProcExecD(  cmStatsProc* p, unsigned regId, const double v[],    unsigned vCnt )
{
  cmRC_t rc = cmOkRC;

  // on the first pass allcoate the storage buffer (m) and vectors (sumV,meanV and varV)
  if( p->execCnt == 0 )
  {
    p->sumV  = cmMemResizeZ( double, p->sumV,  p->dimCnt);
    p->meanV = cmMemResizeZ( double, p->meanV, p->dimCnt );
    p->varV  = cmMemResizeZ( double, p->varV,  p->dimCnt );
    p->m     = cmMemResizeZ( double, p->m,     p->dimCnt * p->wndEleCnt );
  }

  // if the storage matrix is full 
  if( p->curIdx == p->wndEleCnt )
    return rc;

  // get the pointer to this data source reg recd
  assert( regId < p->regArrayN);
  cmStatsProcRecd* r = p->regArrayV + regId;

  // the dimensionality of the incoming data must be <= the registered dimensionality
  assert( r->cnt <= vCnt );

  unsigned dimIdx  = r->idx;
  bool    updateFl = cmIsFlag(p->flags,kUpdateOnExecStatProcFl);
  double* sbp      = p->sumV + dimIdx;                     // sum base ptr

  // mbp point to a segment (mbp[vCnt]) in column p->curIdx
  double* mbp = p->m + (p->curIdx * p->dimCnt) + dimIdx;   // mem col base ptr

  const double* mep = p->m + p->dimCnt * p->wndEleCnt;

  // decr the current col segment from the sum
  if( updateFl )
    cmVOD_SubVV( sbp, vCnt, mbp );

  assert( p->m <= mbp && mbp < mep && p->m <= mbp+vCnt && mbp+vCnt <= mep );

  // copy in the incoming values to mem col segment
  cmVOD_Copy(  mbp, vCnt, v );

  if( updateFl )
  {
    // incr the sum from the incoming value 
    cmVOD_AddVV( sbp, vCnt, mbp );

    // use the new sum to compute new mean values
    cmVOD_DivVVS( p->meanV + dimIdx, vCnt, sbp, p->curCnt );


    // update the variance - cmross each row 
    unsigned di;

    for(di=dimIdx; di<dimIdx+vCnt; ++di )
      p->varV[di] = cmVOD_VarianceN( p->m + dimIdx, p->curCnt, p->dimCnt, p->meanV + dimIdx );
  }

  ++p->execCnt;

  return cmOkRC;
}


cmRC_t       cmStatsProcExecF(  cmStatsProc* p, unsigned regId, const float  v[],   unsigned vCnt )
{
  double dv[ vCnt ];
  cmVOD_CopyF(dv,vCnt,v);
  cmStatsProcExecD(p,regId,dv,vCnt);
  return cmOkRC;
}

cmRC_t       cmStatsProcCalc(cmStatsProc* p )
{
  unsigned colCnt = cmMin(p->curCnt,p->wndEleCnt);
  unsigned i      = 0;

  cmVOD_Fill(p->sumV,p->dimCnt,0);
 
  // sum the ith col of p->m[] into p->sumV[i]
  for(; i<colCnt; ++i)
  {
    cmVOD_AddVV( p->sumV, p->dimCnt, p->m + (i * p->dimCnt) );
    if( p->mfp != NULL )
      cmMtxFileDoubleExec(  p->mfp, p->sumV, p->dimCnt, 1 );
  }

  // calc the mean of each row
  cmVOD_DivVVS( p->meanV, p->dimCnt, p->sumV, colCnt );

  // calc the variance cmross each row
  for(i=0; i<p->dimCnt; ++i)
    p->varV[i] = cmVOD_VarianceN(p->m + i, colCnt, p->dimCnt, p->meanV + i );

  return cmOkRC;
  
}

cmRC_t       cmStatsProcAdvance(    cmStatsProc* p )
{
  ++p->curIdx;
  if( p->curIdx > p->wndEleCnt )
    p->curIdx = 0;

  p->curCnt = cmMin(p->curCnt+1,p->wndEleCnt);

  return cmOkRC;
}


void         cmStatsProcTest( cmVReportFuncPtr_t vReportFunc )
{
  enum
  {
    wndEleCnt = 7,
    dDimCnt   = 3,
    fDimCnt   = 2,
    dimCnt    = dDimCnt + fDimCnt,

    kTypeId0 = 0,
    kTypeId1 = 1
  };

  unsigned flags     = 0;
  unsigned i;

  double dd[ dDimCnt * wndEleCnt ] =
    { 
      0, 1, 2, 
      3, 4, 5, 
      6, 7, 8, 
      9,  10, 11, 
      12, 13, 14, 
      15, 16, 17, 
      18, 19, 20 
    };

  float fd[ 14 ] = 
    {
      0, 1,
      2, 3,
      4, 5,
      6, 7,
      8, 9,
      10, 11,
      12, 13
    };

  cmCtx c;
  cmCtxInit(&c, vReportFunc, vReportFunc, NULL );
  cmStatsProc* p = cmStatsProcAlloc( &c, NULL, wndEleCnt, flags );

  unsigned regId0 = cmStatsProcReg( p, kTypeId0, dDimCnt );
  unsigned regId1 = cmStatsProcReg( p, kTypeId1, fDimCnt );
  

  for(i=0; i<wndEleCnt; ++i)
  {
    cmStatsProcExecD( p, regId0, dd + (i*dDimCnt), dDimCnt );
    cmStatsProcExecF( p, regId1, fd + (i*fDimCnt), fDimCnt );
    cmStatsProcAdvance(p);
  }

  cmStatsProcCalc( p);
  cmVOD_PrintE( vReportFunc, 1, p->dimCnt, p->meanV );
  cmVOD_PrintE( vReportFunc, 1, p->dimCnt, p->varV );

  cmStatsProcFree(&p);

}
*/

//------------------------------------------------------------------------------------------------------------
cmBeatHist* cmBeatHistAlloc( cmCtx* c, cmBeatHist* ap, unsigned frmCnt )
{
  cmBeatHist* p = cmObjAlloc(cmBeatHist,c,ap);
  
  p->fft  = cmFftAllocRR(c,NULL,NULL,0,kToPolarFftFl);
  p->ifft = cmIFftAllocRR(c,NULL,0);

  if( frmCnt > 0 )
    if( cmBeatHistInit(p,frmCnt) != cmOkRC )
      cmBeatHistFree(&p);

  return p;
}

cmRC_t      cmBeatHistFree(  cmBeatHist** pp )
{
  cmRC_t rc = cmOkRC;
  cmBeatHist* p = *pp;

  if( pp == NULL || *pp == NULL )
    return cmOkRC;

  if((rc = cmBeatHistFinal(p)) != cmOkRC )
    return rc;

  cmMemPtrFree(&p->m);
  cmMemPtrFree(&p->H);
  cmMemPtrFree(&p->df);
  cmMemPtrFree(&p->fdf);
  cmMemPtrFree(&p->histV);
  cmFftFreeRR(&p->fft);
  cmIFftFreeRR(&p->ifft);

  cmObjFree(&p);
  return rc;

}

void _cmBeatHistInitH( cmReal_t* H, unsigned hrn, unsigned hcn, unsigned ri, unsigned c0, unsigned c1 )
{
  unsigned ci;
  for(ci=c0; ci<=c1; ++ci)
    H[ (ci*hrn) + ri ] = 1;
}

cmRC_t      cmBeatHistInit(  cmBeatHist* p, unsigned frmCnt )
{
  cmRC_t rc;
  unsigned i,j,k;
  enum { kLagFact = 4, kHistBinCnt=15, kHColCnt=128 };

  if((rc = cmBeatHistFinal(p)) != cmOkRC )
    return rc;  

  p->frmCnt    = frmCnt;
  p->maxLagCnt = (unsigned)floor(p->frmCnt / kLagFact);
  p->histBinCnt= kHistBinCnt;
  p->hColCnt   = kHColCnt;
  p->dfi       = 0;
  
  unsigned cfbMemN = p->frmCnt     * p->maxLagCnt;
  unsigned hMemN   = p->histBinCnt * kHColCnt;

  //cmArrayResizeVZ(p->obj.ctx,&p->cfbMem, cmReal_t, &p->m, cfbMemN, &p->H, hMemN, NULL);
  p->m = cmMemResizeZ( cmReal_t, p->m, cfbMemN );
  p->H = cmMemResizeZ( cmReal_t, p->H, hMemN );

  //p->df    = cmArrayResizeZ(c,&p->dfMem, 2*p->frmCnt + kHistBinCnt, cmReal_t);
  //p->fdf   = p->df  + p->frmCnt;
  //p->histV = p->fdf + p->frmCnt;
  //cmArrayResizeVZ(p->obj.ctx, &p->dfMem, cmReal_t, &p->df, p->frmCnt, &p->fdf, p->frmCnt, &p->histV, kHistBinCnt, NULL );
  p->df    = cmMemResizeZ( cmReal_t, p->df,    p->frmCnt );
  p->fdf   = cmMemResizeZ( cmReal_t, p->fdf,   p->frmCnt );
  p->histV = cmMemResizeZ( cmReal_t, p->histV, kHistBinCnt );
  
  cmFftInitRR( p->fft,NULL,cmNextPowerOfTwo(2*frmCnt),kToPolarFftFl);
  cmIFftInitRR(p->ifft,p->fft->binCnt);

  // initialize H
  _cmBeatHistInitH( p->H,  p->histBinCnt, p->hColCnt, 0, 103, 127 );
  _cmBeatHistInitH( p->H,  p->histBinCnt, p->hColCnt, 1, 86,  102 );
  _cmBeatHistInitH( p->H,  p->histBinCnt, p->hColCnt, 2, 73,   85 );
  _cmBeatHistInitH( p->H,  p->histBinCnt, p->hColCnt, 3, 64,   72 );
  _cmBeatHistInitH( p->H,  p->histBinCnt, p->hColCnt, 4, 57,   63 );
  _cmBeatHistInitH( p->H,  p->histBinCnt, p->hColCnt, 5, 51,   56 );
  _cmBeatHistInitH( p->H,  p->histBinCnt, p->hColCnt, 6, 46,   50 );
  _cmBeatHistInitH( p->H,  p->histBinCnt, p->hColCnt, 7, 43,   45 );
  _cmBeatHistInitH( p->H,  p->histBinCnt, p->hColCnt, 8, 39,   42 );
  _cmBeatHistInitH( p->H,  p->histBinCnt, p->hColCnt, 9, 36,   38 );
  _cmBeatHistInitH( p->H,  p->histBinCnt, p->hColCnt, 10,32,   35 );
  _cmBeatHistInitH( p->H,  p->histBinCnt, p->hColCnt, 11,28,   31 );
  _cmBeatHistInitH( p->H,  p->histBinCnt, p->hColCnt, 12,25,   27 );
  _cmBeatHistInitH( p->H,  p->histBinCnt, p->hColCnt, 13,21,   24 );
  _cmBeatHistInitH( p->H,  p->histBinCnt, p->hColCnt, 14,11,   20 );  

  // for each column
  for(i=0; i<p->maxLagCnt; ++i)
  {
    
    // for each lag group
    for(j=0; j<kLagFact; ++j)
    {
      for(k=0; k<=2*j; ++k)
      {
        unsigned idx = (i*p->frmCnt) + (i*j) + i + k;
        if( idx < cfbMemN )
          p->m[ idx ] = 1.0/(2*j+1);
      }
    }
  }

  double g[ p->maxLagCnt ];
  double g_max = 0;
  double b     = 43;

  b = b*b;
  for(i=0; i<p->maxLagCnt; ++i)
  {
    double n = i+1;
    g[i] = n/b * exp(-(n*n) / (2*b));
    if( g[i] > g_max )
      g_max = g[i];
  }

  // normalize g[]
  cmVOD_DivVS( g, p->maxLagCnt, g_max );

  // for each column of p->m[]
  for(i=0; i<p->maxLagCnt; ++i)
  {
    double gg = g[i];

    k = i*p->frmCnt;

    for(j=0; j<p->frmCnt; ++j)
      p->m[  k + j ] *= gg;
  }

  //p->mfp = cmCtxAllocDebugFile(p->obj.ctx,"beatHist");
  return cmOkRC;
}

cmRC_t      cmBeatHistFinal( cmBeatHist* p )
{  
  //if( p != NULL )
  //  cmCtxFreeDebugFile(p->obj.ctx,&p->mfp);

  return cmOkRC;
}

cmRC_t      cmBeatHistExec(  cmBeatHist* p, cmSample_t df  )
{
  if( p->dfi < p->frmCnt )
    p->df[p->dfi++] = df;
  return cmOkRC;
}

cmRC_t cmBeatHistCalc( cmBeatHist* p )
{
  unsigned i;

  // df -= mean(df)
  cmVOR_SubVS(p->df,p->frmCnt,cmVOR_Mean(p->df,p->frmCnt));

  //cmPlotLineR( "dfm",  NULL, p->df, NULL, p->frmCnt, NULL, kSolidPlotLineId  );  

  // take alpha norm of df
  double alpha = 9;
  cmVOR_DivVS(p->df,p->frmCnt, cmVOR_AlphaNorm(p->df,p->frmCnt,alpha));


  //cmPlotLineS( "dfd",  NULL, p->df, NULL, p->frmCnt, NULL, kSolidPlotLineId  );  

  // low pass forward/backward filter df[] into fdf[]
  cmReal_t b[] = {0.1600, 0.3200, 0.1600};
  unsigned bn  = sizeof(b)/sizeof(b[0]);
  cmReal_t a[] = {1.0000, -0.5949, 0.2348};
  unsigned an  = sizeof(a)/sizeof(a[0]);
  cmFilterFilterR(p->obj.ctx,b,bn,a,an,p->df,p->frmCnt,p->fdf,p->frmCnt);

  //cmPlotLineS( "fdf",  NULL, p->fdf, NULL, p->frmCnt, NULL, kSolidPlotLineId  );  

  // median filter to low-passed filtered fdf[] into df[]
  cmVOR_FnThresh(p->fdf,p->frmCnt,16,p->df,1,NULL);

  // subtract med filtered signal from the low pa1ssed signal.
  // fdf[] -= df[];
  cmVOR_SubVV(p->fdf,p->frmCnt,p->df);

  // half-wave rectify fdf[] = set all negative values in fdf[] to zero.
  cmVOR_HalfWaveRectify(p->fdf,p->frmCnt,p->fdf);

  //cmPlotLineS( "meddf",  NULL, p->fdf, NULL, p->frmCnt, NULL, kSolidPlotLineId  );  

  // take FT of fdf[]
  cmFftExecRR(p->fft,p->fdf,p->frmCnt);

  // square FT magn.
  cmVOR_PowVS(p->fft->magV,p->fft->binCnt,2);

  //cmPlotLineS( "mag",  NULL, p->fft->magV, NULL, p->fft->binCnt, NULL, kSolidPlotLineId  );  

  // take the IFFT of the squared magnitude vector.
  cmVOR_Fill(p->fft->phsV,p->fft->binCnt,0);
  cmIFftExecRectRR(p->ifft,p->fft->magV,p->fft->phsV);

  // Matlab automatically provides this scaling as part of the IFFT.
  cmVOR_DivVS(p->ifft->outV,p->ifft->outN,p->ifft->outN);
  
  // remove bias for short periods from CMF
  for(i=0; i<p->frmCnt; ++i)
    p->ifft->outV[i] /= (p->frmCnt - i);

  //cmPlotLineS( "cm",  NULL, p->ifft->outV, NULL, p->frmCnt, NULL, kSolidPlotLineId  );  
  
  // apply comb filter to the CMF and store result in df[maxLagCnt]
  cmVOR_MultVMtV(p->df,p->maxLagCnt,p->m,p->frmCnt,p->ifft->outV);

  //acPlotLineS( "cfb",  NULL, p->df, NULL, p->maxLagCnt, NULL, kSolidPlotLineId  );  

  //acVOR_Print(p->obj.err.rpt,1,p->maxLagCnt,p->df);

  assert( p->maxLagCnt == p->hColCnt );
  cmVOR_MultVMV(p->histV,p->histBinCnt,p->H,p->hColCnt,p->df);

  cmReal_t bins[] = { 25, 17, 13, 9, 7, 6, 5, 3, 4, 3, 4, 4, 3, 4, 10};
  
  cmVOR_DivVV( p->histV, p->histBinCnt, bins );

  //cmPlotLineS( "cfb",  NULL, p->histV, NULL, p->histBinCnt, NULL, kSolidPlotLineId  );  

  if( p->mfp != NULL )
    cmMtxFileRealExec(  p->mfp, p->histV, p->histBinCnt );

  return cmOkRC;  
}

//------------------------------------------------------------------------------------------------------------
cmGmm_t* cmGmmAlloc( cmCtx* c, cmGmm_t* ap, unsigned K, unsigned D, const cmReal_t* gM, const cmReal_t* uM, const cmReal_t* sMM, unsigned uflags )
{
  cmGmm_t* p = cmObjAlloc( cmGmm_t, c, ap );

  if( K > 0 && D > 0 )
    if( cmGmmInit(p,K,D,gM,uM,sMM,uflags) != cmOkRC )
      cmGmmFree(&p);

  return p;
}

cmRC_t          cmGmmFree(  cmGmm_t** pp )
{
  cmRC_t   rc = cmOkRC;
  cmGmm_t* p  = *pp;

  if( pp == NULL || *pp == NULL )
    return cmOkRC;

  if((rc = cmGmmFinal(p)) != cmOkRC )
    return rc;

  cmMemPtrFree(&p->gV);
  cmMemPtrFree(&p->uM);
  cmMemPtrFree(&p->sMM);
  cmMemPtrFree(&p->isMM);
  cmMemPtrFree(&p->uMM);
  cmMemPtrFree(&p->logDetV);
  cmMemPtrFree(&p->t);
  cmObjFree(pp);
  return rc;
}

cmRC_t _cmGmmUpdateCovar( cmGmm_t* p, const cmReal_t* sMM )
{
  unsigned i;

  if( sMM == NULL )
    return cmOkRC;

  unsigned De2  = p->D*p->D;
  unsigned KDe2 = p->K*De2;
  
  cmVOR_Copy(p->sMM, KDe2, sMM);
  cmVOR_Copy(p->isMM,KDe2, sMM);
  cmVOR_Copy(p->uMM, KDe2, sMM);

  //if( cmIsFlag(p->uflags,cmGmmCovarNoProcFl) )
  //  return cmOkRC;

  // for each component
  for(i=0; i<p->K; ++i)
  {
    cmReal_t*       is = p->isMM + i*De2;
    cmReal_t*       u  = p->uMM  + i*De2;
    cmReal_t*       r;

    // if the covariance matrix is diagnal
    if( cmIsFlag(p->uflags,cmGmmDiagFl))
    {
      p->logDetV[i] = cmVOR_LogDetDiagM(is,p->D); // calc the det. of diag. covar. mtx
      r             = cmVOR_InvDiagM(is,p->D);    // calc the inverse in place
    }
    else
    {
      p->logDetV[i] = cmVOR_LogDetM(is,p->D);     // calc the det. of covar mtx
      r             = cmVOR_InvM(is,p->D);        // calc the inverse in place      
    }
   
    if( fabs(p->logDetV[i]) < 1e-20 )
    {
      cmCtxPrint(p->obj.ctx,"%i\n",i);
      cmVOR_PrintLE("DANGER SM:\n",p->obj.err.rpt,p->D,p->D,p->sMM);
    }
   

    if( cmVOR_CholZ(u,p->D) == NULL )
    {
      return cmCtxRtCondition(&p->obj, cmSingularMtxRC, "A singular covariance matrix (Cholesky factorization failed.)  was encountered in _cmGmmUpdateCovar().");
    }

    if( p->logDetV[i] == 0 )
    {
      cmGmmPrint(p,true);
      return cmCtxRtCondition(&p->obj, cmSingularMtxRC, "A singular covariance matrix (det==0)  was encountered in _cmGmmUpdateCovar().");
    }

    if( r == NULL )
    {
      //cmCtxPrint(c,"%i\n",i);
      //cmVOR_PrintLE("DANGER SM:\n",p->obj.err.rpt,p->D,p->D,p->sMM);

      return cmCtxRtCondition(&p->obj, cmSingularMtxRC, "A singular covariance matrix (inversion failed) was encountered in _cmGmmUpdateCovar().");
    }
  
  }
  return cmOkRC;
}

cmRC_t          cmGmmInit(  cmGmm_t* p, unsigned K, unsigned D, const cmReal_t* gV, const cmReal_t* uM, const cmReal_t* sMM, unsigned uflags )
{
  cmRC_t rc;
  if((rc = cmGmmFinal(p)) != cmOkRC )
    return rc;

  //          gM[K]    uM[DK]  sMM[DDK] isMM[DDK]+ uMM[DDK] logDetV[K] t[DD]  fact[K]
  /* 
  unsigned n =  K    + (D*K) + (D*D*K) + (D*D*K) + (D*D*K)     + K    + (D*D) + K; 
  p->gV      = cmArrayResizeZ(c,&p->memA, n, cmReal_t );
  p->uM      = p->gV      +  K;
  p->sMM     = p->uM      + (D*K);
  p->isMM    = p->sMM     + (D*D*K);
  p->uMM     = p->isMM    + (D*D*K);
  p->logDetV = p->uMM     + (D*D*K);
  p->t       = p->logDetV + K;
  */

  //cmArrayResizeVZ(c, &p->memA, cmReal_t,  &p->gV,K, &p->uM,D*K, &p->sMM,D*D*K,  
  //&p->isMM,D*D*K,  &p->uMM,D*D*K,  &p->logDetV,K, &p->t,D*D, NULL );

  p->gV      = cmMemResizeZ( cmReal_t, p->gV,      K );
  p->uM      = cmMemResizeZ( cmReal_t, p->uM,      D*K);
  p->sMM     = cmMemResizeZ( cmReal_t, p->sMM,     D*D*K);
  p->isMM    = cmMemResizeZ( cmReal_t, p->isMM,    D*D*K);
  p->uMM     = cmMemResizeZ( cmReal_t, p->uMM,     D*D*K);
  p->logDetV = cmMemResizeZ( cmReal_t, p->logDetV, K); 
  p->t       = cmMemResizeZ( cmReal_t, p->t,       D*D );

  p->K       = K;
  p->D       = D;  
  p->uflags  = uflags;
  
  if( gV != NULL )
    cmVOR_Copy(p->gV,K,gV);

  if( uM != NULL )
    cmVOR_Copy(p->uM,D*K,uM);

  return _cmGmmUpdateCovar(p,sMM );

}


cmRC_t          cmGmmFinal( cmGmm_t* p )
{ return cmOkRC; }


typedef struct
{
  cmGmm_t*          p;
  const cmReal_t* xM;
  unsigned        colCnt;
} _cmGmmRdFuncData_t;

const cmReal_t* _cmGmmReadFunc( void* userPtr, unsigned colIdx )
{   
  assert(colIdx < ((const _cmGmmRdFuncData_t*)userPtr)->colCnt); 
  return ((const _cmGmmRdFuncData_t*)userPtr)->xM + (colIdx * ((const _cmGmmRdFuncData_t*)userPtr)->p->D); 
}


// xM[D,xN]
// yV[xN]
// yM[xN,K]
cmRC_t          cmGmmEval(  cmGmm_t* p,  const cmReal_t* xM, unsigned xN, cmReal_t* yV, cmReal_t* yM )
{
  _cmGmmRdFuncData_t r;
  r.colCnt = xN;
  r.p      = p;
  r.xM     = xM;

  return cmGmmEval2(p,_cmGmmReadFunc,&r,xN,yV,yM);
}


cmRC_t   cmGmmEval2(  cmGmm_t* p, cmGmmReadFunc_t readFunc, void* userFuncPtr, unsigned xN, cmReal_t* yV, cmReal_t* yM)
{
  cmReal_t  tV[xN];
  unsigned k;

  //cmVOR_PrintL("cV: ",p->obj.err.rpt, 1, p->K, p->gV);
  //cmVOR_PrintL("uM:\n",p->obj.err.rpt, p->D, p->K, p->uM );
  
  //
  cmVOR_Fill(yV,xN,0);

  // for each component PDF
  for(k=0; k<p->K; k++)
  {
    const cmReal_t* meanV  = p->uM   + (k*p->D);
    const cmReal_t* isM    = p->isMM + (k*p->D*p->D);
    cmReal_t*       pV;
    
    if( yM == NULL )
      pV = tV;
    else
      pV = yM + (k*xN);


    // evaluate the kth component PDF with xM[1:T]

    //cmVOR_MultVarGaussPDF2( pV, xM,                    meanV, isM, p->logDetV[k], p->D, xN, cmIsFlag(p->uflags,cmGmmDiagFl) );
    cmVOR_MultVarGaussPDF3( pV, readFunc, userFuncPtr, meanV, isM, p->logDetV[k], p->D, xN, cmIsFlag(p->uflags,cmGmmDiagFl) );


    // apply the kth component weight
    cmVOR_MultVS( pV, xN, p->gV[k] );

    // sum the result into the output vector
    cmVOR_AddVV( yV, xN, pV );

  }
  return cmOkRC;   
}



// Evaluate each component for a single data point
// xV[D] - observed data point 
// yV[K] - output contains the evaluation for each component
cmRC_t          cmGmmEval3(  cmGmm_t* p,  const cmReal_t* xV,  cmReal_t* yV )
{
  unsigned k;

  for(k=0; k<p->K; ++k)
  {
    const cmReal_t* meanV  = p->uM   + (k*p->D);
    const cmReal_t* isM    = p->isMM + (k*p->D*p->D);

    // evaluate the kth component PDF with xM[1:T]
    cmVOR_MultVarGaussPDF2( yV + k, xV, meanV, isM, p->logDetV[k], p->D, 1, cmIsFlag(p->uflags,cmGmmDiagFl) );

    // apply the kth component weight
    yV[k] *= p->gV[k];

  }

  return cmOkRC;
}

cmReal_t _cmGmmKmeansDistFunc( void* userPtr, const cmReal_t* v0, const cmReal_t* v1, unsigned vn )
{ return  cmVOR_EuclidDistance(v0,v1,vn); }

cmRC_t cmGmmRandomize2( cmGmm_t* p, cmGmmReadFunc_t readFunc, void* funcUserPtr, unsigned xN, const cmReal_t* uM, const bool* roFlV )
{
  unsigned k;
  unsigned iV[ p->K ];

  if( uM == NULL )
    roFlV = NULL;

  // randomize the mixture coefficients
  cmVOR_Random( p->gV, p->K, 0.0, 1.0 );
  cmVOR_NormalizeProbability(p->gV,p->K);

  // fill iV with random integers between 0 and xN-1
  cmVOU_Random( iV, p->K, xN-1 );

  
  // for each component
  for(k=0; k<p->K; ++k)
  {
    cmReal_t r[ p->D ];
    
    // if this component's mean is not read-only
    if( roFlV==NULL || roFlV[k]==false )
    {
      const cmReal_t* xV = NULL;

      if( uM == NULL )
        xV = readFunc( funcUserPtr, iV[k] ); // read a random frame index
      else
        xV = uM + (k*p->D);                  // select a user supplied mean vector

      assert( xV != NULL );

      // set the random feature vector as this components mean value
      cmVOR_Copy(p->uM+(k*p->D),p->D,xV); 
    }

    cmReal_t* sM = p->sMM+(k*p->D*p->D);

    // create a random covariance mtx
    if( cmIsFlag(p->uflags,cmGmmDiagFl) )
    {
      // create a random diag. covar mtx
      cmVOR_Random(r,p->D,0.0,1.0);    
      cmVOR_Diag(sM,p->D,r);
    }
    else
    {
      // create a random symetric positive definite matrix
      cmVOR_RandSymPosDef(sM,p->D,p->t);
    }       
  }

  unsigned* classIdxV = cmMemAllocZ(unsigned, xN );

  // if some components have read-only mean's 
  if( uM != NULL && roFlV != NULL )
  {
    assert( xN >= p->K );

    for(k=0; k<p->K; ++k)
      classIdxV[k] = roFlV[k];
  }

  // use kmeans clustering to move the means closer to their center values
  if( cmIsFlag( p->uflags, cmGmmSkipKmeansFl) == false )
    cmVOR_Kmeans2( classIdxV, p->uM, p->K, readFunc, p->D, xN, funcUserPtr, _cmGmmKmeansDistFunc, NULL, -1, 0 ); 

  cmMemPtrFree(&classIdxV);

  return _cmGmmUpdateCovar(p,p->sMM);

}

cmRC_t cmGmmRandomize( cmGmm_t* p, const cmReal_t* xM, unsigned xN )
{
  _cmGmmRdFuncData_t r;
  r.colCnt = xN;
  r.p      = p;
  r.xM     = xM;

  return cmGmmRandomize2(p,_cmGmmReadFunc,&r,xN,NULL,NULL);
}


// xM[D,xN]
cmRC_t cmGmmTrain( cmGmm_t* p, const cmReal_t* xM, unsigned xN, unsigned* iterCntPtr )
{
  unsigned i,k;
  cmRC_t rc;

  if((rc = cmGmmRandomize(p,xM,xN)) != cmOkRC )
    return rc;

  //cmGmmPrint(c,p);

  // wM[xN,K]
  cmReal_t wM[ xN * p->K ]; // wM[N,K] soft assignment mtx
  unsigned wV[ xN ];        // wV[N]   hard assignment vector
  unsigned stopCnt = 0;
  unsigned curStopCnt = 0;

  if( iterCntPtr != NULL )
  {
    stopCnt     = *iterCntPtr;
    *iterCntPtr = 0;
  }
  else
  {
    // BUG BUG BUG 
    // stopCnt is used uninitialized when iterCntPtr == NULL
    assert( 0 );
  }

  while(1)
  {

    //cmVOR_NormalizeProbability(p->gV,p->K);

    cmCtxPrint(p->obj.ctx,"iter:%i --------------------------------------------\n",*iterCntPtr );

    cmVOR_PrintL("uM:\n", p->obj.err.rpt, p->D, p->K, p->uM );
    cmVOR_PrintL("gV:\n", p->obj.err.rpt, 1,    p->K, p->gV );

    //cmGmmPrint(c,p);

    for(k=0; k<p->K; ++k)
    {
      cmReal_t* wp = wM + (k*xN);
   
      // calc the prob that each data point in xM[] was generated by the kth gaussian
      // and store as a column vector in wM[:,k]
      cmVOR_MultVarGaussPDF2( wp, xM, p->uM + (k*p->D), p->isMM + (k*p->D*p->D), p->logDetV[k], p->D, xN, cmIsFlag(p->uflags,cmGmmDiagFl) );

        
      // scale the prob by the gain coeff for gaussian k
      cmVOR_MultVS( wp, xN, p->gV[k]);

    }



    //cmVOR_PrintL("wM:\n",p->obj.err.rpt,xN,p->K,wM);

    bool doneFl = true;
    for(i=0; i<xN; ++i)
    {        
      
      // form a probability for the ith data point weights
      cmVOR_NormalizeProbabilityN( wM + i, p->K, xN);
 
      // select the cluster to which the ith data point is most likely to belong
      unsigned mi = cmVOR_MaxIndex(wM + i, p->K, xN);

      // if the ith data point changed clusters
      if( mi != wV[i] )
      {
        doneFl = false;
        wV[i]  = mi;
      }
    }

    curStopCnt = doneFl ? curStopCnt+1 : 0;
    

    // if no data points changed owners then the clustering is complete
    if( curStopCnt == stopCnt )
    {
      //cmVOU_PrintL("wV: ",p->obj.err.rpt,xN,1,wV);
      break;
    }

    // for each cluster
    for(k=0; k<p->K; ++k)
    {
      cmReal_t* uV  = p->uM  + (k*p->D);            // meanV[k]
      cmReal_t* sM  = p->sMM + (k*p->D*p->D);       // covarM[k]

      cmReal_t  sw  = cmVOR_Sum(wM + (k*xN), xN ); 


      // update the kth weight
      p->gV[k] = sw / xN;


      // form a sum of all data points weighted by their soft assignment to cluster k      
      cmReal_t sumV[p->D];      
      cmVOR_MultVMV( sumV, p->D, xM, xN, wM + k*xN );

      // update the mean[k]
      cmVOR_DivVVS(uV, p->D, sumV, sw );
         

      // update the covar[k]
      // sM += ( W(i,k) .* ((X(:,i) - uV) * (X(:,i) - uV)')); 
      cmVOR_Fill(sM,p->D*p->D,0);
      for(i=0; i<xN; ++i)
      {
        cmReal_t duV[ p->D ];
        
        cmVOR_SubVVV(  duV,  p->D, xM + (i*p->D), uV );      // duV[]  = xM[:,i] - uV[];
        cmVOR_MultMMM( p->t, p->D, p->D, duV, duV, 1 );      // t[,]   = duV[] * duV[]' 
        cmVOR_MultVS(  p->t, p->D*p->D, wM[ k * xN + i ]);   // t[,]  *= wM[i,k]
        cmVOR_AddVV(   sM, p->D*p->D, p->t );                // sM[,] += t[,];
      }          

      cmVOR_DivVS( sM, p->D*p->D, sw );  // sM[,] ./ sw;
    }  

    // update the inverted covar mtx and covar det.
    if((rc = _cmGmmUpdateCovar(p,p->sMM )) != cmOkRC )
      return rc;

    if( iterCntPtr != NULL )
      *iterCntPtr += 1;
  }
  return cmOkRC;
}

// xM[D,xN]
cmRC_t cmGmmTrain2( cmGmm_t* p, cmGmmReadFunc_t readFunc, void* userFuncPtr, unsigned xN, unsigned* iterCntPtr, const cmReal_t* uM, const bool* roFlV, int maxIterCnt )
{
  unsigned i,j,k;
  cmRC_t rc;

  // if uM[] is not set then ignore roFlV[]
  if( uM == NULL )
    roFlV=NULL;

  if((rc = cmGmmRandomize2(p,readFunc,userFuncPtr,xN,uM,roFlV)) != cmOkRC )
    return rc;

  //cmGmmPrint(c,p);

  // wM[xN,K] soft assignment mtx
  cmReal_t* wM = cmMemAlloc(cmReal_t, xN * p->K ); 

  // wV[N]   hard assignment vector
  unsigned* wV = cmMemAlloc(unsigned, xN );
        
  unsigned stopCnt    = 0;
  unsigned curStopCnt = 0;

  if( iterCntPtr != NULL )
  {
    stopCnt     = *iterCntPtr;
    *iterCntPtr = 0;
  }
  else
  {
    // BUG BUG BUG 
    // stopCnt is used uninitialized when iterCntPtr == NULL
    assert( 0 );
  }

  while(1)
  {

    //cmCtxPrint(p->obj.ctx,"iter:%i --------------------------------------------\n",*iterCntPtr );
    //cmVOR_PrintL("uM:\n", p->obj.err.rpt, p->D, p->K, p->uM );
    cmVOR_PrintL("gV:\n", p->obj.err.rpt, 1,    p->K, p->gV );
    //cmGmmPrint(c,p);

    for(k=0; k<p->K; ++k)
    {
      cmReal_t* wp = wM + (k*xN);
   
      // calc the prob that each data point in xM[] was generated by the kth gaussian
      // and store as a column vector in wM[:,k]
      cmVOR_MultVarGaussPDF3( wp, readFunc, userFuncPtr, p->uM + (k*p->D), p->isMM + (k*p->D*p->D), p->logDetV[k], p->D, xN, cmIsFlag(p->uflags,cmGmmDiagFl) );

        
      // scale the prob by the gain coeff for gaussian k
      cmVOR_MultVS( wp, xN, p->gV[k]);

    }

    //cmVOR_PrintL("wM:\n",p->obj.err.rpt,xN,p->K,wM);

    bool doneFl = true;
    unsigned changeCnt = 0;
    for(i=0; i<xN; ++i)
    {        
      
      // form a probability for the ith data point weights
      cmVOR_NormalizeProbabilityN( wM + i, p->K, xN);
 
      // select the cluster to which the ith data point is most likely to belong
      unsigned mi = cmVOR_MaxIndex(wM + i, p->K, xN);

      // if the ith data point changed clusters
      if( mi != wV[i] )
      {
        ++changeCnt;
        doneFl = false;
        wV[i]  = mi;
      }
    }

    curStopCnt = doneFl ? curStopCnt+1 : 0;
    
    printf("%i stopCnt:%i changeCnt:%i\n",*iterCntPtr,curStopCnt,changeCnt);

    // if no data points changed owners then the clustering is complete
    if( curStopCnt == stopCnt )
    {
      //cmVOU_PrintL("wV: ",p->obj.err.rpt,xN,1,wV);
      break;
    }

    // if a maxIterCnt was given and the cur iter cnt exceeds the max iter cnt then stop
    if( maxIterCnt>=1 && *iterCntPtr >= maxIterCnt )
      break;

    // form a sum of all data points weighted by their soft assignment to cluster k      
    // NOTE: cmGmmTrain() performs this step more efficiently because it use
    //       an LAPACK matrix multiply.  
    cmReal_t sumM[ p->D * p->K ];

    cmVOR_Zero(sumM,p->D*p->K);

    for(i=0; i<xN; ++i)
    {
      const cmReal_t* xV = readFunc( userFuncPtr, i );
      
      assert( xV != NULL );

      for(k=0; k<p->K; ++k)
      {
        cmReal_t weight = wM[ i + (k*xN)];

        for(j=0; j<p->D; ++j)
          sumM[ j + (k*p->D) ] += xV[j] * weight;
      }
    }



    // for each cluster that is not marked as read-only
    for(k=0; k<p->K; ++k)
    {
      cmReal_t* uV  = p->uM  + (k*p->D);            // meanV[k]
      cmReal_t* sM  = p->sMM + (k*p->D*p->D);       // covarM[k]

      cmReal_t  sw  = cmVOR_Sum(wM + (k*xN), xN ); 


      // update the kth weight
      p->gV[k] = sw / xN;


      // if this component's mean is not read-only
      if( (roFlV==NULL || roFlV[k]==false) && sw != 0)
      {
        // get vector of all data points weighted by their soft assignment to cluster k      
        cmReal_t* sumV = sumM + (k*p->D);  // sumV[p->D];      

        // update the mean[k]
        cmVOR_DivVVS(uV, p->D, sumV, sw );
      }  

      // update the covar[k]
      // sM += ( W(i,k) .* ((X(:,i) - uV) * (X(:,i) - uV)')); 
      cmVOR_Fill(sM,p->D*p->D,0);
      for(i=0; i<xN; ++i)
      {
        cmReal_t duV[ p->D ];

        const cmReal_t* xV = readFunc( userFuncPtr, i );
        
        assert( xV != NULL );

        
        cmVOR_SubVVV(  duV,  p->D, xV, uV );                 // duV[]  = xM[:,i] - uV[];
        cmVOR_MultMMM( p->t, p->D, p->D, duV, duV, 1 );      // t[,]   = duV[] * duV[]' 
        cmVOR_MultVS(  p->t, p->D*p->D, wM[ k * xN + i ]);   // t[,]  *= wM[i,k]
        cmVOR_AddVV(   sM, p->D*p->D, p->t );                // sM[,] += t[,];
      }          

      if( sw != 0 )        
        cmVOR_DivVS( sM, p->D*p->D, sw );  // sM[,] ./ sw;
    }  

    // update the inverted covar mtx and covar det.
    if((rc = _cmGmmUpdateCovar(p,p->sMM )) != cmOkRC )
      goto errLabel;

    if( iterCntPtr != NULL )
      *iterCntPtr += 1;
  }

  cmMemPtrFree(&wM);
  cmMemPtrFree(&wV);

 errLabel:

  return cmOkRC;
}

cmRC_t cmGmmTrain3( cmGmm_t* p, const cmReal_t* xM, unsigned xN, unsigned* iterCntPtr )
{
  _cmGmmRdFuncData_t r;
  r.colCnt = xN;
  r.p      = p;
  r.xM     = xM;
  
  return cmGmmTrain2(p,_cmGmmReadFunc,&r,xN,iterCntPtr,NULL,NULL,-1);

}


cmRC_t cmGmmGenerate( cmGmm_t* p, cmReal_t* yM, unsigned yN )
{
  unsigned i=0;

  unsigned kV[yN];  

  // use weighted random selection to choose the component for each output value
  cmVOR_WeightedRandInt(kV,yN, p->gV, p->K );

  // cmVOU_Print(p->obj.err.rpt,1,yN,kV);

  for(i=0; i<yN; ++i)
  {
    const cmReal_t* uV  = p->uM  + (kV[i] * p->D);
    unsigned        idx = kV[i] * p->D * p->D;

    //cmVOR_PrintL("sM\n",p->obj.err.rpt,p->D,p->D,sM);

    if( cmIsFlag(p->uflags,cmGmmDiagFl) )
    {
      const cmReal_t* sM = p->sMM + idx;
      cmVOR_RandomGaussDiagM( yM + (i*p->D), p->D, 1, uV, sM ); 
    }
    else
    {
      const cmReal_t* uM = p->uMM + idx;
      cmVOR_RandomGaussNonDiagM2(yM + (i*p->D), p->D, 1, uV, uM ); 
    }

  }
  return cmOkRC;
}

void     cmGmmPrint( cmGmm_t* p, bool fl )
{
  unsigned k;
  //cmCtx* c = p->obj.ctx;

  cmVOR_PrintL("gV: ",  p->obj.err.rpt, 1,    p->K, p->gV );
  cmVOR_PrintL("mM:\n", p->obj.err.rpt, p->D, p->K, p->uM );

  for(k=0; k<p->K; ++k)
    cmVOR_PrintL("sM:\n", p->obj.err.rpt, p->D, p->D, p->sMM + (k*p->D*p->D));

  if( fl )
  {
    for(k=0; k<p->K; ++k)
      cmVOR_PrintL("isM:\n", p->obj.err.rpt, p->D, p->D, p->isMM + (k*p->D*p->D));

    for(k=0; k<p->K; ++k)
      cmVOR_PrintL("uM:\n", p->obj.err.rpt, p->D, p->D, p->uMM + (k*p->D*p->D));

    cmVOR_PrintL("logDetV:\n", p->obj.err.rpt, 1, p->K, p->logDetV);

  }

}

void cmGmmTest(  cmRpt_t* rpt, cmLHeapH_t lhH, cmSymTblH_t stH )
{
  cmCtx* c = cmCtxAlloc(NULL,rpt,lhH,stH);

  unsigned K        = 2;
  unsigned D        = 2;
  cmReal_t gV[ 2 ]  = { .5, .5 };
  cmReal_t uM[ 4 ]  = { .3, .3, .8, .8 };
  cmReal_t sMM[ 8 ] = { .1,  0,  0, .1, .1,  0,  0, .1 };
  unsigned flags    = cmGmmDiagFl ;

  unsigned M = 100;
  cmReal_t xM[ D*M ];
  cmReal_t yV[ M ];
  unsigned i,j;

  cmPlotSetup("MultDimGauss Test",1,1);

  cmGmm_t* p = cmGmmAlloc(c, NULL, K, D, gV, uM, sMM, flags );

  if(0)
  {
    cmGmmPrint(p,true);

    for(i=0; i<10; i++)
      for(j=0; j<20; j+=2)
      {
        xM[(i*20)+j]     = .1 * i;;
        xM[(i*20)+j + 1] = .1 * (j/2);;
      } 

    // octave equivalent
    // x0= .1 * ones(1,10);
    // x = [ 0*x0 1*x0 2*x0 3*x0 4*x0 5*x0 6*x0 7*x0 8*x0 9*x0];
    // x = [x; repmat([0:.1:1],1,10)];
    // y = mvnpdf(x',[.3 .3],[.1 0; 0 .1]); plot(y);

    cmGmmEval(p,xM,M,yV,NULL);

  
    //cmVOR_PrintL( "xM\n", rpt, D,   M, xM );
    cmVOR_PrintL( "yV\n",   rpt, 10, 10, yV );

    //printf("y:%f\n",yV[0]);


    cmPlotLineD( NULL, NULL, yV, NULL, M, NULL, kSolidPlotLineId );


  }

  if(0)
  {
    cmReal_t yM[  D*M ];
    cmReal_t yMt[ M*D ];
    cmReal_t uMt[ p->K*D];
    unsigned iterCnt = 10;

    //srand( time(NULL) );

    cmGmmGenerate( p, yM, M );

    p->uflags = 0; // turn off diagonal condition

    if( cmGmmTrain3( p, yM, M, &iterCnt ) != cmOkRC )
      return;

    cmCtxPrint(c,"iterCnt:%i\n",iterCnt);

    cmGmmPrint( p, true );

    cmVOR_Transpose(yMt, yM, D, M );

    //cmVOR_PrintL("yMt\n",vReportFunc,M,D,yMt);

    cmPlotLineD(NULL, yMt, yMt+M, NULL, M, "blue", kAsteriskPlotPtId );
    
    cmVOR_Transpose( uMt, p->uM, D, p->K);

    cmVOR_PrintL("uMt:\n", p->obj.err.rpt, p->K, p->D, uMt );

    cmPlotLineD(NULL, uMt, uMt+p->K, NULL, p->D, "red", kXPlotPtId );

  }

  if(1)
  {
    cmGmmFree(&p);

    
    cmReal_t cV0[]  = { .7, .3 };
    cmReal_t uM0[]  = { .2, .1, .1, .2 };
    cmReal_t sMM0[] = { .01, 0, 0, .01,   .01, 0, 0, .01 };
    unsigned flags  = 0;

    K = 2;
    D = 2;

    cmGmm_t* p = cmGmmAlloc(c,NULL, K, D, cV0, uM0, sMM0, flags );
    
    xM[0] = 0.117228;
    xM[1] = 0.110079; 

    cmGmmEval(p,xM,1,yV,NULL);

    cmCtxPrint(c,"y: %f\n",yV[0]);
    
  }

  cmPlotDraw();
  cmGmmFree(&p);
  cmCtxFree(&c);

}

//------------------------------------------------------------------------------------------------------------
cmChmm_t* cmChmmAlloc( cmCtx* c, cmChmm_t* ap, unsigned stateN, unsigned mixN, unsigned dimN, const cmReal_t* iV, const cmReal_t* aM )
{
  cmChmm_t* p = cmObjAlloc(cmChmm_t,c,ap);

  if( stateN >0 && dimN > 0 )
    if( cmChmmInit(p,stateN,mixN,dimN,iV,aM) != cmOkRC )
      cmChmmFree(&p);

  return p;  
}

cmRC_t    cmChmmFree(  cmChmm_t** pp )
{
  cmRC_t    rc = cmOkRC;
  cmChmm_t* p  = *pp;

  if( pp == NULL || *pp == NULL )
    return cmOkRC;

  if((rc = cmChmmFinal(p)) != cmOkRC )
    return rc;

  cmMemPtrFree(&p->iV);
  cmMemPtrFree(&p->aM);
  cmMemPtrFree(&p->bV);
  cmMemPtrFree(&p->bM);
  cmObjFree(pp);
  return cmOkRC;

}

cmRC_t    cmChmmInit(  cmChmm_t* p, unsigned stateN, unsigned mixN, unsigned dimN, const cmReal_t* iV, const cmReal_t* aM )
{
  cmRC_t rc;
  unsigned i;

  if((rc = cmChmmFinal(p)) != cmOkRC )
    return rc;

  //             iV[]         aM             
  /*
  unsigned n = stateN + (stateN*stateN);
  p->iV      = cmArrayResizeZ(c, &p->memA, n, cmReal_t );
  p->aM      = p->iV + stateN;
  */

  //cmArrayResizeVZ(c,&p->memA, cmReal_t, &p->iV,stateN, &p->aM, stateN*stateN, NULL );

  p->iV = cmMemResizeZ( cmReal_t, p->iV, stateN );
  p->aM = cmMemResizeZ( cmReal_t, p->aM, stateN * stateN );
  p->bV = cmMemResizeZ( cmGmm_t*, p->bV, stateN );
  p->N  = stateN;
  p->K  = mixN;
  p->D  = dimN;

  if( iV != NULL )
    cmVOR_Copy(p->iV,p->N,iV);

  if( aM != NULL )
    cmVOR_Copy(p->aM,p->N*p->N,aM);

  for(i=0; i<p->N; ++i)
    p->bV[i] = cmGmmAlloc( p->obj.ctx, NULL, p->K, p->D, NULL, NULL, NULL, 0 ); 

  //p->mfp = cmCtxAllocDebugFile( p->obj.ctx,"chmm");

  return cmOkRC;
}

cmRC_t    cmChmmFinal( cmChmm_t* p )
{ 
  if( p != NULL )
  {
    unsigned i;

    for(i=0; i<p->N; ++i)
      cmGmmFree( &p->bV[i] );

    cmMemPtrFree(&p->bM);

    //if( p->mfp != NULL )
    //  cmCtxFreeDebugFile(p->obj.ctx,&p->mfp);

  }
  return cmOkRC; 
}

cmRC_t      cmChmmRandomize( cmChmm_t* p, const cmReal_t* oM, unsigned T )
{
  cmRC_t rc;
  unsigned i;
  unsigned N = p->N;

  // randomize the initial state probabilities
  cmVOR_Random( p->iV, N, 0.0, 1.0 ); 
  cmVOR_NormalizeProbability( p->iV, N ); 

  // randomize the state transition matrix
  cmVOR_Random( p->aM, N*N, 0.0, 1.0 );

  for(i=0; i<N; ++i)
  {
    cmVOR_NormalizeProbabilityN( p->aM + i, N, N );  // rows of aM must sum to 1.0

    if((rc = cmGmmRandomize( p->bV[i], oM, T )) != cmOkRC)  // randomize the GMM assoc'd with state i
      return rc;
  }

  cmMemPtrFree(&p->bM); // force bM[] to be recalculated

  return cmOkRC;
}

cmReal_t  _cmChmmKmeansDist( void* userPtr, const cmReal_t* v0, const cmReal_t* v1, unsigned vn )
{ return cmVOR_EuclidDistance(v0,v1,vn); }

cmRC_t    cmChmmSegKMeans( cmChmm_t* p, const cmReal_t* oM, unsigned T, cmReal_t threshProb, unsigned maxIterCnt, unsigned iterCnt )
{
  cmCtx* c  = p->obj.ctx;
  cmRC_t rc = cmOkRC;
  unsigned i,j,k,t;

  unsigned N = p->N;
  unsigned K = p->K;
  unsigned D = p->D;

  //unsigned qV[T];
  //cmReal_t alphaM[N*T];
  //unsigned clusterIdxV[T];
  //cmReal_t centroidM[D*N];

  /*
  unsigned sz = 2*ALIGN_B(T,unsigned) +
                ALIGN_B(N*T,cmReal_t) +
                ALIGN_B(D*N,cmReal_t);
  cmArray mem;
  cmArrayAlloc(c, &mem);

  unsigned* qV          = (unsigned*) cmArrayResize(c, &mem, sz, char);
  cmReal_t* alphaM      = (cmReal_t*) (qV          + ALIGN_T(T,  unsigned));
  unsigned* clusterIdxV = (unsigned*) (alphaM      + ALIGN_T(N*T,cmReal_t));
  cmReal_t* centroidM   = (cmReal_t*) (clusterIdxV + ALIGN_T(T,  unsigned));
  */

  unsigned* qV          = cmMemAlloc( unsigned, T );
  cmReal_t* alphaM      = cmMemAlloc( cmReal_t, N*T);
  unsigned* clusterIdxV = cmMemAlloc( unsigned, T );
  cmReal_t* centroidM   = cmMemAlloc( cmReal_t, D*N);


  cmReal_t logPr = 0;
  bool reportFl = true;

  cmChmmRandomize(p,oM,T);
  
  // cluster the observations into N groups
  cmVOR_Kmeans( qV, centroidM, N, oM, D, T, NULL, 0, false, _cmChmmKmeansDist, NULL ); 

  for(i=0; i<maxIterCnt; ++i)
  {
    unsigned jnV[N];

    if( reportFl )
      cmCtxPrint(c,"SegKM: ----------------------------------------------------%i\n",i);

    // get the count of data points in each state
    cmVOU_Fill(jnV,N,0);
    for(t=0; t<T; ++t)
      ++jnV[ qV[t] ]; 

    // for each state
    for(j=0; j<N; ++j)
    {
      cmGmm_t* g = p->bV[j];
      
      // cluster all datapoints which were assigned to state j
      cmVOR_Kmeans( clusterIdxV, g->uM, K, oM, D, T, qV, j, false, _cmChmmKmeansDist, NULL ); 

      // for each cluster
      for(k=0; k<K; ++k)
      {
        unsigned kN = 0;

        // kN is count of data points assigned to cluster k
        for(t=0; t<T; ++t)
          if( clusterIdxV[t] == k )
            ++kN;

        g->gV[k] = (cmReal_t)kN/jnV[j];

        
        // the covar of the kth component is the sample covar of cluster k
        cmVOR_GaussCovariance(g->sMM + (k*D*D), D, oM, T, g->uM + (k*D), clusterIdxV, k );

      }      

      if((rc = _cmGmmUpdateCovar(g, g->sMM )) != cmOkRC )
        goto errLabel;
    }

    if( i== 0 )
    {
      // count transitions from i to j
      for(t=0; t<T-1; ++t)
        p->aM[  (qV[t+1]*N) + qV[t] ] += 1.0;

      for(j=0; j<N; ++j)
      {
        // normalize state transitions by dividing by times in each state
        for(k=0; k<N; k++)
          p->aM[ (k*N) + j ] /= jnV[j];

        cmVOR_NormalizeProbabilityN(p->aM + j, N, N);

        cmGmmEval( p->bV[j], oM, 1, p->iV + j, NULL );
      }
    }


    if((rc = cmChmmTrain(p, oM, T, iterCnt,0,0 )) != cmOkRC )
      goto errLabel;

    // calculate the prob. that the new model generated the data
    cmReal_t logPr0 = cmChmmForward(p,oM,T,alphaM,NULL);
    cmReal_t dLogPr = logPr0 - logPr;
  
    if( reportFl )
      cmCtxPrint(c,"pr:%f d:%f\n",logPr0,dLogPr);

    if( (dLogPr > 0) && (dLogPr < threshProb) )
      break;

    logPr = logPr0;



    // fill qV[] with the state at each time t
    cmChmmDecode(p,oM,T,qV);

  }

 errLabel:

  cmMemPtrFree(&qV);
  cmMemPtrFree(&alphaM);
  cmMemPtrFree(&clusterIdxV);
  cmMemPtrFree(&centroidM);

  return rc;

}


cmRC_t    cmChmmSetGmm(   cmChmm_t* p, unsigned i, const cmReal_t* wV, const cmReal_t* uM, const cmReal_t* sMM, unsigned flags )
{
  assert( i < p->N);
  cmMemPtrFree(&p->bM); // force bM[] to be recalculated
  return cmGmmInit(p->bV[i],p->K,p->D,wV,uM,sMM,flags);
}


// Return the probability of the observation for each state.
// oV[D] - multi-dim. observation data point
// pV[N] - probability of this observation for each state
void cmChmmObsProb( const cmChmm_t* p, const cmReal_t* oV,  cmReal_t* prV )
{
  unsigned i;
  for(i=0; i<p->N; ++i)
    cmGmmEval( p->bV[i], oV, 1, prV + i, NULL ); 
}

// oM[D,T]     - observation matrix
// alphaM[N,T] - prob of being in each state and observtin oM(:,t)
// bM[N,T]     - (optional) state-observation probability matrix
// logPrV[T]   - (optional) record the log prob of the data given the model at each time step 
// Returns sum(logPrV[T])
cmReal_t cmChmmForward( const cmChmm_t* p, const cmReal_t* oM, unsigned T, cmReal_t* alphaM, cmReal_t* logPrV )
{
  unsigned t;

  cmReal_t logPr = 0;

  // calc the prob of starting in each state
  if( p->bM == NULL )
    cmChmmObsProb( p, oM, alphaM );
  else
    cmVOR_Copy( alphaM, p->N*T, p->bM );
  

  cmVOR_MultVV( alphaM, p->N, p->iV );

  cmReal_t s = cmVOR_Sum(alphaM,p->N);
  
  cmVOR_DivVS(alphaM,p->N,s);
  //cmVOR_PrintL("alpha:\n",p->obj.err.rpt,p->N,1,alphaM);

  for(t=1; t<T; ++t)
  {
    cmReal_t  tmp[p->N];

    cmReal_t* alphaV = alphaM + t*p->N;

    // calc the prob of the observation for each state
    if( p->bM == NULL )
      cmChmmObsProb(p,oM + (t*p->D), alphaV );

    // calc. the prob. of transitioning to each state at time t, from each state at t-1
    cmVOR_MultVVM(tmp,p->N, alphaM + ((t-1)*p->N), p->N, p->aM ); 

    // calc the joint prob of transitioning from each state to each state and observing O(t).
    cmVOR_MultVV(alphaV, p->N, tmp );
    
    // scale the probabilities to prevent underflow
    s = cmVOR_Sum(alphaV,p->N);
    cmVOR_DivVS(alphaV,p->N,s);

    // track the log prob. of the model having generated the data up to time t.
    cmReal_t pr = log(s);

    if( logPrV != NULL )
      logPrV[t] = pr;

    logPr += pr;
  }

  return logPr;
  
}

// oM[D,T]
// betaM[N,T]
void cmChmmBackward( const cmChmm_t* p, const cmReal_t* oM, unsigned T, cmReal_t* betaM )
{
  cmVOR_Fill(betaM,p->N*T,1.0);

  assert(T >= 2 );

  int t = (int)T - 2;
  for(; t>=0; --t)
  {
    cmReal_t tmp[p->N];

    if( p->bM == NULL )
      cmChmmObsProb(p,oM+((t+1)*p->D), tmp );
    else
      cmVOR_Copy(tmp,p->N,p->bM + ((t+1)*p->N));

    cmVOR_MultVV(tmp,p->N,betaM + ((t+1)*p->N));

    cmVOR_MultVMV(betaM+(t*p->N),p->N, p->aM, p->N, tmp );

    cmVOR_NormalizeProbability(betaM+(t*p->N),p->N );

  }
    
}

cmReal_t  cmChmmCompare( const cmChmm_t* p0, const cmChmm_t* p1, unsigned T )
{
  assert(p0->D == p1->D);
  assert(p0->N == p1->N);

  cmReal_t oM[p0->D*T];
  cmReal_t alphaM[p0->N*T];
  
  cmChmmGenerate(p0,oM,T,NULL);
  cmReal_t logPr00 = cmChmmForward(p0,oM,T,alphaM,NULL);
  cmReal_t logPr01 = cmChmmForward(p1,oM,T,alphaM,NULL);

  cmChmmGenerate(p1,oM,T,NULL);
  cmReal_t logPr10 = cmChmmForward(p0,oM,T,alphaM,NULL);
  cmReal_t logPr11 = cmChmmForward(p1,oM,T,alphaM,NULL);

  cmReal_t d0 = (logPr01-logPr00)/T;
  cmReal_t d1 = (logPr10-logPr11)/T;

  return (d0+d1)/2;

}

cmRC_t    cmChmmGenerate( const cmChmm_t* p, cmReal_t* oM,  unsigned T, unsigned* sV )
{
  unsigned i,si;

  // use weighted random selection to choose an intitial state
  cmVOR_WeightedRandInt(&si, 1, p->iV, p->N );

  for(i=0; i<T; ++i)
  {
    if( sV != NULL )
      sV[i] = si;

    // generate a random value using the GMM assoc'd with the current state
    cmGmmGenerate( p->bV[si], oM + (i*p->D), 1 );

    // choose the next state using the transition weights from the current state
    cmVOR_WeightedRandInt(&si, 1, p->aM + (si*p->N), p->N );
  }  
 
  return cmOkRC;
}


cmRC_t    cmChmmDecode(   cmChmm_t* p, const cmReal_t* oM, unsigned T, unsigned* yV )
{
  int i,j,t;
  unsigned N = p->N;

  //unsigned psiM[N*T];
  //cmReal_t delta[N];
  /*
  unsigned sz = ALIGN_B(N*T,unsigned) + ALIGN_B(N,cmReal_t);

  cmArray  mem;
  cmArrayAlloc(c, &mem);

  unsigned* psiM  = (unsigned*) cmArrayResize(c, &mem, sz, char);
  cmReal_t* delta = (cmReal_t*) (psiM + ALIGN_T(N*T,unsigned));
  */

  unsigned* psiM = cmMemAlloc( unsigned, N*T );
  cmReal_t* delta= cmMemAlloc( cmReal_t, N );

  // get the prob of starting in each state
  if( p->bM == NULL )
    cmChmmObsProb( p, oM, delta );
  else
    cmVOR_Copy( delta, N, p->bM );

  cmVOR_MultVV( delta, p->N, p->iV );
  cmVOR_NormalizeProbability(delta, p->N);
  
  for(t=1; t<T; ++t)
  {
    cmReal_t         mV[N];

    const cmReal_t*  ap = p->aM;

    // calc. the most likely new state given the most likely prev state
    // and the transition matrix
    for(i=0; i<N; ++i)
    {
      const cmReal_t*  dp     = delta;
      unsigned         psiIdx = t*N + i;

      mV[i]          = *ap++ * *dp++;
      psiM[ psiIdx ] = 0;

      // find max value of: delta .* A(:,i)
      for(j=1; j<N; ++j )
      {        
        cmReal_t v = *ap++ * *dp++;

        if( v > mV[i] )
        {
          mV[i]          = v;
          psiM[ psiIdx ] = j;
        }
      }
    }

    // mV[] now holds the prob. of the max likelihood state at time t 
    // for each possible state at t-1

    // psiM[:,t] holds the index of the max likelihood state 

    // condition the most likely new state on the observations
    if( p->bM == NULL )
      cmChmmObsProb(p,oM + (t*p->D), delta);  
    else
      cmVOR_Copy(delta, N, p->bM + (t*p->N) );

    cmVOR_MultVV(delta, N, mV );              // condition it on the max. like current states
    cmVOR_NormalizeProbability( delta, N );   // normalize the prob.
  }

  // unwind psiM[] to form the max. likelihood state  sequence
  yV[T-1] = cmVOR_MaxIndex(delta,N,1);

  for(t=T-2; t>=0; --t)
    yV[t] = psiM[ ((t+1)*N) + yV[t+1] ];


  cmMemPtrFree(&psiM);
  cmMemPtrFree(&delta);

  return cmOkRC;
}


cmRC_t    cmChmmTrain( cmChmm_t* p, const cmReal_t* oM, unsigned T, unsigned iterCnt, cmReal_t thresh, unsigned flags )
{
  cmRC_t   rc = cmOkRC;
  unsigned i,j,k,t,d;
  unsigned iter;
  unsigned N          = p->N;
  unsigned K          = p->K;
  unsigned D          = p->D;
  unsigned De2        = D * D;
  bool     mixFl      = !cmIsFlag(flags,kNoTrainMixCoeffChmmFl);
  bool     meanFl     = !cmIsFlag(flags,kNoTrainMeanChmmFl);
  bool     covarFl    = !cmIsFlag(flags,kNoTrainCovarChmmFl);
  bool     bFl        = mixFl | meanFl | covarFl;
  bool     calcBFl    = true;
  bool     progFl     = false;
  bool     timeProgFl = false;
  cmReal_t progInc    = 0.1;
  cmReal_t progFrac   = 0;
  cmReal_t logProb    = 0;

  //cmReal_t alphaM[N*T]; // alpha[N,T]
  //cmReal_t betaM[N*T];  // betaM[N,T]
  //cmReal_t logPrV[T];
  //cmReal_t EpsM[N*N];
  //cmReal_t BK[N*K*T]; 
  //cmReal_t gamma_jk[N*K];
  //cmReal_t uM[K*D*N];
  //cmReal_t sMM[K*De2*N];
  /*
  unsigned sz = ALIGN_T(N*T,    cmReal_t) +
                ALIGN_T(N*T,    cmReal_t) +
                ALIGN_T(T,      cmReal_t) +
                ALIGN_T(N*N,    cmReal_t) +
                ALIGN_T(N*K*T,  cmReal_t) +
                ALIGN_T(N*K,    cmReal_t) +
                ALIGN_T(K*D*N,  cmReal_t) +
                ALIGN_T(K*De2*N,cmReal_t);
  cmArray mem;
  cmArrayAlloc(c, &mem);

  cmReal_t* alphaM   = cmArrayResize(c, &mem, sz, cmReal_t); // alpha[N,T]
  cmReal_t* betaM    = alphaM   + ALIGN_T(N*T,  cmReal_t);   // betaM[N,T]
  cmReal_t* logPrV   = betaM    + ALIGN_T(N*T,  cmReal_t);
  cmReal_t* EpsM     = logPrV   + ALIGN_T(T,    cmReal_t);
  cmReal_t* BK       = EpsM     + ALIGN_T(N*N,  cmReal_t);
  cmReal_t* gamma_jk = BK       + ALIGN_T(N*K*T,cmReal_t);
  cmReal_t* uM       = gamma_jk + ALIGN_T(N*K,  cmReal_t);
  cmReal_t* sMM      = uM       + ALIGN_T(K*D*N,cmReal_t);
  */

  cmReal_t* alphaM   = cmMemAlloc( cmReal_t, N*T ); 
  cmReal_t* betaM    = cmMemAlloc( cmReal_t, N*T ); 
  cmReal_t* logPrV   = cmMemAlloc( cmReal_t, T ); 
  cmReal_t* EpsM     = cmMemAlloc( cmReal_t, N*N ); 
  cmReal_t* BK       = cmMemAlloc( cmReal_t, N*K*T ); 
  cmReal_t* gamma_jk = cmMemAlloc( cmReal_t, N*K ); 
  cmReal_t* uM       = cmMemAlloc( cmReal_t, K*D*N ); 
  cmReal_t* sMM      = cmMemAlloc( cmReal_t, K*De2*N ); 

  if( thresh <=0 )
    thresh = 0.0001;

  //cmArrayResizeZ(c,&p->memC,N*T,cmReal_t);
  p->bM = cmMemResizeZ( cmReal_t, p->bM, N*T); 

  for(iter=0; iter<iterCnt; ++iter)
  {
    // zero the mean and covar summation arrays
    cmVOR_Fill(uM,  K*D  *N,0);
    cmVOR_Fill(sMM, K*De2*N,0);
    cmVOR_Fill(EpsM,N*N,    0);
    cmVOR_Fill(gamma_jk,N*K,0);

    // 
    // B[i,t]     The prob that state i generated oM(:,t)                                           
    // BK[i,k,t]  The prob that state i component k generated oM(:,t)
    // Note: B[i,t] = sum(BK(i,k,:))
    //
    if( calcBFl || bFl )
    {
      calcBFl = false;
      for(t=0; t<T; ++t)
      {
        // prob. that state i generated objservation O[t]
        for(i=0; i<N; ++i )
          cmGmmEval(  p->bV[i], oM + (t*D), 1, p->bM + (t*N) + i, BK + (t*N*K) + (i*K) );
      }
    }

    // alpha[N,T] is prob. of transitioning forward to each state given the observed data
    cmReal_t logProb0 =  cmChmmForward( p, oM, T, alphaM, logPrV );

    // check for convergence
    cmReal_t dLogProb =  fabs(logProb0-logProb) / ((fabs(logProb0)+fabs(logProb)+cmReal_EPSILON)/2);
    if( dLogProb < thresh )
      break;

    logProb = logProb0;

    // betaM[N,T] is prob of transitioning backward from each state given the observed data
    cmChmmBackward(p, oM, T, betaM );


    if(progFl)
      cmCtxPrint(p->obj.ctx,"%i (%f) ",iter+1, dLogProb );

    if(timeProgFl)
      progFrac = progInc;


    // for each time step
    for(t=0; t<T-1; ++t)
    {
      // oV[D] is the observation at step t
      const cmReal_t* oV = oM + (t*D); 


      //
      // Update EpsM[N,N] (6.37)
      // (prob. of being in state i at time t and transitioning
      //  to state j at time t+1)
      //
      cmReal_t E[N*N];

      // for each possible state transition
      for(i=0; i<N; ++i)
        for(j=0; j<N; ++j)
        {

          E[ i + (j*N) ] 
            =   exp(log(alphaM[ (t*N) + i ])
              + log(p->aM[  i + (j*N) ])
              + log(p->bM[ ((t+1)*N) + j ])
              + log(betaM[ ((t+1)*N) + j ]));           
        }


      cmVOR_NormalizeProbability( E, N*N );
      cmVOR_AddVV( EpsM, N*N, E );


      // If t==0 then update the initial state prob's
      if( t == 0 )
      {
        for(i=0; i<N; ++i)
          p->iV[i] = cmVOR_SumN(EpsM+i, N, N);

        assert( cmVOR_IsNormal(p->iV,N) );    
      }

      if( bFl )
      {
        // 
        // Calculate gamma_jk[]
        // 
        cmReal_t gtjk[N*K];  // gamma_jk[N,K] at time t
        cmReal_t abV[N];     //

      
        // (alphaM[j,t] * betaM[j:t]) / (sum(alphaM[:,t] * betaM[:,t]))
        cmVOR_MultVVV(abV,N,alphaM + t*N, betaM+t*N);
        cmReal_t abSum = cmVOR_Sum(abV,N);

        if( abSum<=0 )
          assert(abSum>0);

        cmVOR_DivVS(abV,N,abSum);

     
        for(j=0; j<N; ++j)
        {
          cmReal_t bkSum = cmVOR_Sum(BK + (t*N*K) + (j*K), K );
        
          for(k=0; k<K; ++k)
            gtjk[ (k*N)+j ]  = abV[j] * (BK[ (t*N*K) + (j*K) + k ] / bkSum);       
        }


        // sum gtjk[N,K] into gamma_jk (integrate gamma over time)
        cmVOR_AddVV( gamma_jk, N*K, gtjk );


        // update the mean and covar numerators
        for(j=0; j<N; ++j)
        {
          cmReal_t* uV = uM  + (j*D*K);
          cmReal_t* sV = sMM + (j*De2*K);

          for(k=0; k<K; ++k,uV+=D,sV+=De2)
          {
            cmReal_t c = gtjk[ (k*N)+j ];

            if( covarFl )
            {
              cmReal_t dV[D];
              cmReal_t dM[D*D];

              // covar numerator b[j].sM[k]
              cmVOR_SubVVV(dV, D, oV, p->bV[j]->uM + (k*D));
              cmVOR_MultMMM( dM, D, D, dV, dV, 1 );
              cmVOR_MultVS(  dM, De2, c );
              cmVOR_AddVV(   sV, De2, dM ); 
            }

            if( meanFl )
            {
              // mean numerator b[j].uM[k]
              for(d=0; d<D; ++d)
                uV[d] += c * oV[ d ];
            }
          }
        }
      }

      if( timeProgFl && (t >= floor(T*progFrac)) )
      {
        cmCtxPrint(p->obj.ctx,"%i  ", (unsigned)round(progFrac*100) );
        progFrac+=progInc;
      }

    } // end time loop


    for(i=0; i<N; ++i)
    {

      // update the state transition matrix
      cmReal_t den = cmVOR_SumN(EpsM + i, N, N );

      assert(den != 0 );

      for(j=0; j<N; ++j)
        p->aM[ i + (j*N) ] = EpsM[ i + (j*N) ] / den;

      if( bFl )
      {
        // update the mean, covariance and mix coefficient
        cmGmm_t*        g   = p->bV[i];
        const cmReal_t* uV  = uM  + (i*D*K);
        const cmReal_t* sMV = sMM + (i*De2*K);

        for(k=0; k<K; ++k,uV+=D,sMV+=De2)
        {
          cmReal_t gjk = gamma_jk[ (k*N) + i ];

          if( meanFl )
            cmVOR_DivVVS(g->uM  + (k*D),   D,   uV,  gjk );
          
          if( covarFl )
            cmVOR_DivVVS(g->sMM + (k*De2), De2, sMV, gjk );

          if( mixFl )
            g->gV[k] =  gjk / cmVOR_SumN( gamma_jk + i, K, N );
        }

        if((rc = _cmGmmUpdateCovar(g,g->sMM)) != cmOkRC )
          goto errLabel;
      }
    }        

    assert( cmVOR_IsNormalZ(p->aM,N*N) );

    if( timeProgFl )
      cmCtxPrint(p->obj.ctx,"\n");

  } // end iter loop

  if( progFl)
    cmCtxPrint(p->obj.ctx,"\n");

  if( p->mfp != NULL )
  {
    // first line is iV[N]
    cmMtxFileRealExec(p->mfp,p->iV,p->N);
    
    // next N lines are aM[N,N]
    for(i=0; i<p->N; ++i)
      cmMtxFileRealExecN(p->mfp,p->aM + i,p->N,p->N);

    // next T lines are bM[T,N]
    if( p->bM != NULL )
      for(i=0; i<T; ++i)
        cmMtxFileRealExec(p->mfp, p->bM + (i*p->N),p->N);
  }

 errLabel:
  cmMemPtrFree(&alphaM);
  cmMemPtrFree(&betaM);
  cmMemPtrFree(&logPrV);
  cmMemPtrFree(&EpsM);
  cmMemPtrFree(&BK);
  cmMemPtrFree(&gamma_jk);
  cmMemPtrFree(&uM);
  cmMemPtrFree(&sMM);

  return rc;
}


void      cmChmmPrint(    cmChmm_t* p )
{
  unsigned i;
  cmCtxPrint(p->obj.ctx,"======================================== \n");
  cmVOR_PrintL("iV: ",  p->obj.err.rpt, 1,    p->N, p->iV);
  cmVOR_PrintL("aM:\n", p->obj.err.rpt, p->N, p->N, p->aM);

  for(i=0; i<p->N; ++i)
  {
    cmCtxPrint(p->obj.ctx,"bV[%i] ----------------- %i \n",i,i);
    cmGmmPrint(p->bV[i],false);
  }
}


void     cmChmmTestForward( cmRpt_t* rpt, cmLHeapH_t lhH, cmSymTblH_t stH  )
{
  cmReal_t oM[] = {
    0.117228,   0.110079,
    0.154646,   0.210436,
    0.947468,   0.558136,
    0.202023,   0.138123,
    0.929933,   0.456102,
    0.897566,   0.685078,
    0.945177,   0.663145,
    0.272399,   0.055107,
    0.863386,   0.621546,
    0.217545,   0.274709,
    0.838777,   0.650038,
    0.134966,   0.159472,
    0.053990,   0.264051,
    0.884269,   0.550019,
    0.764787,   0.554484,
    0.114771,   0.077518,
    0.835121,   0.606137,
    0.070733,   0.120015,
    0.819814,   0.588482,
    0.105511,   0.197699,
    0.824778,   0.533047,
    0.945223,   0.511411,
    0.126971,   0.050083,
    0.869497,   0.567737,
    0.144866,   0.197363,
    0.985726,   0.590402,
    0.181094,   0.192827,
    0.162179,   0.155297,
    1.034691,   0.513413,
    0.220708,   0.036158,
    0.750061,   0.671224,
    0.246971,   0.093246,
    0.997567,   0.680491,
    0.916887,   0.530981,
    0.022328,   0.121969,
    0.794031,   0.618081,
    0.845066,   0.625512,
    0.174731,   0.094773,
    0.968665,   0.652435,
    0.932484,   0.388081,
    0.202732,   0.148710,
    0.911307,   0.637139,
    0.211127,   0.201362,
    0.138152,   0.057290,
    0.819132,   0.579888,
    0.135625,   0.176140,
    0.146017,   0.157853,
    0.950319,   0.624150,
    0.285064,   0.038825,
    0.716844,   0.575189,
    0.907433,   0.504946,
    0.219772,   0.129993,
    0.076507,   0.193079,
    0.808906,   0.548409,
    0.880892,   0.523950,
    0.758099,   0.636729,
    1.014017,   0.557120,
    0.277888,   0.181492,
    0.877588,   0.508634,
    0.251266,   0.225890,
    0.990904,   0.482949,
    0.999899,   0.534579,
    0.904179,   0.707349,
    0.952879,   0.617955,
    0.172068,   0.151984,
    1.026262,   0.662600,
    0.812003,   0.430856,
    0.173393,   0.017885,
    0.099370,   0.146661,
    0.785785,   0.564333,
    0.698222,   0.449299,
    0.276539,   0.225314,
    0.799271,   0.618159,
    0.098813,   0.090839,
    0.883666,   0.554150,
    0.274934,   0.185403,
    0.200419,   0.109972,
    0.925076,   0.608610,
    0.864486,   0.348689,
    0.176733,   0.136235,
    0.967278,   0.656875,
    0.986994,   0.659877,
    1.015618,   0.596549,
    0.689903,   0.528107,
    0.978238,   0.630989,
    0.269847,   0.144358,
    0.092303,   0.139894,
    0.168185,   0.095327,
    0.897767,   0.584203,
    0.068316,   0.018452,
    0.953395,   0.530545,
    0.266405,   0.173987,
    0.233845,   0.205276,
    0.900060,   0.477108,
    0.052909,   0.053077,
    0.885850,   0.496546,
    0.268494,   0.104785,
    1.041405,   0.655079,
    1.055915,   0.697988,
    0.181569,   0.146840
  };

  unsigned i;
  cmReal_t iV[] = { .5 , .5};
  cmReal_t A[]  = { .3, .6, .7, .4 };

  cmReal_t cV0[] = { .7, .3 };
  cmReal_t uM0[] = { .2, .1, .1, .2 };
  cmReal_t sMM0[]= { .01, 0, 0, .01,  .01, 0, 0, .01 };

  cmReal_t cV1[] = { .2, .8 };
  cmReal_t uM1[] = { .8, .9, .9, .5 };
  cmReal_t sMM1[]= { .01, 0, 0, .01,  .01, 0, 0, .01 };
  
  
  unsigned T = 100;
  unsigned N = 2;
  unsigned K = 2;
  unsigned D = 2;

  cmReal_t alphaM[N*T];
  cmReal_t betaM[N*T];
  cmReal_t logPrV[T];
  unsigned qV[T];
  unsigned sV[T];
  cmReal_t oMt[T*D];

  // scale covariance
  cmVOR_MultVS(sMM0,D*D*K,1);
  cmVOR_MultVS(sMM1,D*D*K,1);


  cmCtx c;
  cmCtxAlloc(&c,rpt,lhH,stH);

  cmChmm_t* p = cmChmmAlloc(&c,NULL,N,K,D,iV,A);

  cmChmmSetGmm(p,0,cV0,uM0,sMM0,0);
  cmChmmSetGmm(p,1,cV1,uM1,sMM1,0);

  cmChmmPrint(p);

  cmChmmGenerate(p, oM, T, sV );

  cmChmmForward( p, oM, T, alphaM,logPrV );
  
  //cmVOR_PrintL("logPrV:\n",rpt,1,T,logPrV);

  cmCtxPrint(&c,"log prob:%f\n", cmVOR_Sum(logPrV,T));

  cmChmmBackward( p, oM, T, betaM );

  //cmVOR_PrintL("beta:\n",rpt,N,T,betaM);

  cmChmmDecode(p,oM,T,qV);

  cmVOU_PrintL("sV:\n",rpt,1,T,sV);
  cmVOU_PrintL("qV:\n",rpt,1,T,qV);

  unsigned d=0;
  for(i=0; i<T; ++i)
    d += sV[i] != qV[i];

  cmCtxPrint(&c,"Diff:%i\n",d);


  cmPlotSetup("Chmm Forward Test",1,1);
  cmVOR_Transpose(oMt,oM,D,T); 
  cmPlotLineD(NULL, oMt, oMt+T, NULL, T, "blue", kXPlotPtId );
  cmPlotDraw();

  cmChmmFree(&p);

}

void     cmChmmTest( cmRpt_t* rpt, cmLHeapH_t lhH, cmSymTblH_t stH )
{
  
  time_t t =  time(NULL); //0x4b9e82aa; //time(NULL);
  srand( t );
  printf("TIME: 0x%x\n",(unsigned)t);
  
  //cmChmmTestForward(vReportFunc);
  //return;

  unsigned i;
  cmReal_t iV[]   = { 1.0/3.0, 1.0/3.0, 1.0/3.0 };
  cmReal_t A[]    = { .1, .4, .7, .4, .2, .2 };

  cmReal_t cV0[]  = { .7, .3 };
  cmReal_t uM0[]  = { .2, .1, .1, .2 };
  cmReal_t sMM0[] = { .01, 0, 0, .01,   .01, 0, 0, .01 };

  cmReal_t cV1[]  = { .2, .8 };
  cmReal_t uM1[]  = { .8, .9, .9, .8 };
  cmReal_t sMM1[] = { .01, 0, 0, .01,   .01, 0, 0, .01 };

  cmReal_t cV2[]  = { .5, .5 };
  cmReal_t uM2[]  = { .5, .5, .5, .5 };
  cmReal_t sMM2[] = { .01, 0, 0, .01,   .01, 0, 0, .01 };


  cmReal_t kmThreshProb = 0.001;
  unsigned kmMaxIterCnt = 10;
  unsigned iterCnt      = 20;


  unsigned N       = sizeof(iV)  / sizeof(iV[0]);
  unsigned K       = sizeof(cV0) / sizeof(cV0[0]);
  unsigned D       = sizeof(uM0) / sizeof(uM0[0]) / K;
  unsigned T       = 100;

  cmReal_t alphaM[N*T];
  cmReal_t oM[D*T];
  unsigned sV[T];
  unsigned qV[T];
  
  cmCtx c;
  cmCtxAlloc(&c,rpt,lhH,stH);

  cmCtxPrint(&c,"N:%i K:%i D:%i\n",N,K,D);

  cmChmm_t* p = cmChmmAlloc(&c,NULL,N,K,D,iV,A);

  cmChmmSetGmm(p,0,cV0,uM0,sMM0,0);
  cmChmmSetGmm(p,1,cV1,uM1,sMM1,0);
  cmChmmSetGmm(p,2,cV2,uM2,sMM2,0);

  // generate data using the parameters above
  cmChmmGenerate(p,oM,T,sV);  

  cmVOU_PrintL("sV: ",rpt,1,T,sV);

  cmChmmRandomize(p,oM,T);

  if(cmChmmSegKMeans(p,oM,T,kmThreshProb,kmMaxIterCnt,iterCnt) != cmOkRC )
    goto errLabel;


  if( cmChmmTrain(p,oM,T,iterCnt,0,0) != cmOkRC )
    goto errLabel;

  //cmChmmPrint(p);

  cmChmmDecode(p,oM,T,qV);

  cmReal_t pr = cmChmmForward(p,oM,T,alphaM,NULL);

  cmCtxPrint(&c,"pr:%f\n",pr);
  cmVOU_PrintL("sV:\n",rpt,1,T,sV);
  cmVOU_PrintL("qV:\n",rpt,1,T,qV);

  unsigned d=0;
  for(i=0; i<T; ++i)
    d += sV[i] != qV[i];

  cmCtxPrint(&c,"Diff:%i\n",d);

 errLabel:


  cmChmmFree(&p);
 
}

//------------------------------------------------------------------------------------------------------------
cmChord* cmChordAlloc( cmCtx* c, cmChord* ap, const cmReal_t* chromaM, unsigned T )
{
  unsigned i,j;
  unsigned S = 6;
  unsigned N = 24;
  unsigned D = 12;
  cmChord* p = cmObjAlloc(cmChord,c,ap);

  p->h = cmChmmAlloc( p->obj.ctx, NULL, 0, 0, 0, NULL, NULL );

  if( chromaM != NULL && T > 0 )
    if( cmChordInit(p,chromaM,T) != cmOkRC )
      cmChordFree(&p);

  p->N = N;
  p->D = D;
  p->S = kTonalSpaceDimCnt;

  /*
  // iv[N] aM[N*N] uM[D*N] sMM[D*D*N] phiM[D*S] tsxxxV[S]
  unsigned n   = ALIGN_T(N,    cmReal_t) +
                 ALIGN_T(N*N,  cmReal_t) +
                 ALIGN_T(D*N,  cmReal_t) +
                 ALIGN_T(D*D*N,cmReal_t) +
                 ALIGN_T(D*S,  cmReal_t) +
               2*ALIGN_T(S,    cmReal_t);

  p->iV        = cmArrayResizeZ(c, &p->memA, n, cmReal_t); 
  p->aM        = p->iV      + ALIGN_T(N,    cmReal_t);
  p->uM        = p->aM      + ALIGN_T(N*N,  cmReal_t);
  p->sMM       = p->uM      + ALIGN_T(D*N,  cmReal_t);
  p->phiM      = p->sMM     + ALIGN_T(D*D*N,cmReal_t);
  p->tsMeanV   = p->phiM    + ALIGN_T(D*S,  cmReal_t);
  p->tsVarV    = p->tsMeanV + ALIGN_T(S,    cmReal_t);
  */

  p->iV        = cmMemAllocZ( cmReal_t, N );
  p->aM        = cmMemAllocZ( cmReal_t, N*N);
  p->uM        = cmMemAllocZ( cmReal_t, D*N);
  p->sMM       = cmMemAllocZ( cmReal_t, D*D*N );
  p->phiM      = cmMemAllocZ( cmReal_t, D*S);
  p->tsMeanV   = cmMemAllocZ( cmReal_t, S );
  p->tsVarV    = cmMemAllocZ( cmReal_t, S );


  // initialize iV[N] (HMM initial state probabilities)
  cmVOR_Fill(p->iV,N,1.0/N);


  // initialize aM[N,N] (HMM transition matrix)
  cmReal_t epsilon    = 0.01;
  cmReal_t CMaj2any[] = { 12, 2, 8, 6, 4, 10, 0, 10, 4, 6, 8, 2, 5, 5, 9, 1, 11, 3, 7, 7, 3, 11, 1, 9 };
  
  for(i=0; i<N; ++i)
  {
    cmVOR_Copy( p->aM+(i*N), N, CMaj2any );
    cmVOR_Rotate( CMaj2any, N, 1 );
  }

  cmVOR_AddVS(p->aM, N*N, epsilon);
  cmVOR_DivVS(p->aM, N*N, ( (N/2)*(N/2) ) + (N*epsilon) );

  //cmVOR_PrintL("A:\n",p->obj.err.rpt,N,N,A);


  // initialize sMM[D*D,N] (HMM covariance matrices)
  cmReal_t diagMV[] = { 1, 0.2, 0.2, 0.2, 1.0, 0.2, 0.2, 1.0, 0.2, 0.2, 0.2, 0.2 };
  cmReal_t diagmV[] = { 1, 0.2, 0.2, 1.0, 0.2, 0.2, 0.2, 1.0, 0.2, 0.2, 0.2, 0.2 };
  cmReal_t Maj[D*D];
  cmReal_t Min[D*D];
  cmVOR_DiagZ(Maj,D,diagMV);
  Maj[ (4*D) + 0 ] = 0.6;  Maj[ (0*D) + 4 ] = 0.6;
  Maj[ (7*D) + 0 ] = 0.8;  Maj[ (0*D) + 7 ] = 0.8;
  Maj[ (7*D) + 4 ] = 0.8;  Maj[ (4*D) + 7 ] = 0.8;

  
  cmVOR_DiagZ(Min,D,diagmV);
  Min[ (3*D) + 0 ] = 0.6;  Min[ (0*D) + 3 ] = 0.6;
  Min[ (7*D) + 0 ] = 0.8;  Min[ (0*D) + 7 ] = 0.8;
  Min[ (7*D) + 3 ] = 0.8;  Min[ (3*D) + 7 ] = 0.8;

  cmReal_t* sM = p->sMM;
  for(i=0; i<N/2; ++i,sM+=D*D)
    cmVOR_RotateM( sM, D, D, Maj, i, i );

  for(i=0; i<N/2; ++i,sM+=D*D)
    cmVOR_RotateM( sM, D, D, Min, i, i );
  

  /*
  cmVOR_PrintL("Maj:\n",p->obj.err.rpt,D,D,Maj);
  cmVOR_PrintL("Min:\n",p->obj.err.rpt,D,D,Min);

  for(i=0; i<N; ++i)
  {
    cmCtxPrint(c,"%i----\n",i);
    cmVOR_PrintL("sM:\n",p->obj.err.rpt,D,D,sMM + (i*D*D));
  }
  */

  // initialize uM[D,N]  (HMM GMM mean vectors)
  cmVOR_Fill(p->uM,D*N,0);
  for(i=0; i<D; ++i)
  {
    unsigned dom  = (i+7) % D;
    unsigned medM = (i+4) % D;
    unsigned medm = (i+3) % D;

    p->uM[ (i     * D) + i    ] = 1;
    p->uM[ (i     * D) + medM ] = 1;
    p->uM[ (i     * D) + dom  ] = 1;

    p->uM[ ((i+D) * D) + i    ] = 1;
    p->uM[ ((i+D) * D) + medm ] = 1;
    p->uM[ ((i+D) * D) + dom  ] = 1;
  }

  cmVOR_AddVS(p->uM,D*N,0.01);
  
  for(i=0; i<N; ++i)
    cmVOR_NormalizeProbability( p->uM + (i*D), D);


  // initialize phiM[D,S]
  cmReal_t phi[D*S];
  for(i=0,j=0; i<D; ++i,++j) 
    phi[j] = sin( M_PI*7.0*i/6.0 );

  for(i=0; i<D; ++i,++j) 
    phi[j] = cos( M_PI*7.0*i/6.0 );

  for(i=0; i<D; ++i,++j) 
    phi[j] = sin( M_PI*3.0*i/2.0 );

  for(i=0; i<D; ++i,++j) 
    phi[j] = cos( M_PI*3.0*i/2.0 );

  for(i=0; i<D; ++i,++j) 
    phi[j] = 0.5 * sin( M_PI*2.0*i/3.0 );

  for(i=0; i<D; ++i,++j) 
    phi[j] = 0.5 * cos( M_PI*2.0*i/3.0 );
  
  cmVOR_Transpose(p->phiM,phi,D,S);
  

  return p;

}

cmRC_t     cmChordFree( cmChord** pp )
{
  cmRC_t   rc = cmOkRC;
  cmChord* p  = *pp;

  if( pp == NULL || *pp == NULL )
    return cmOkRC;

  if((rc = cmChordFinal(p)) != cmOkRC )
    return rc;

  cmChmmFree( &p->h );

  cmMemPtrFree(&p->iV);
  cmMemPtrFree(&p->aM);
  cmMemPtrFree(&p->uM);
  cmMemPtrFree(&p->sMM);
  cmMemPtrFree(&p->phiM);
  cmMemPtrFree(&p->tsMeanV);
  cmMemPtrFree(&p->tsVarV);

  cmMemPtrFree(&p->chromaM);
  cmMemPtrFree(&p->tsM);
  cmMemPtrFree(&p->cdtsV);


  cmObjFree(pp);
  return rc;
}

cmRC_t     cmChordInit( cmChord* p, const cmReal_t* chromaM, unsigned T )
{
  cmRC_t   rc     = cmOkRC;
  unsigned i;
  unsigned N     = p->N;    // count of states
  unsigned K     = 1;       // count of components per mixture
  unsigned D     = p->D;    // dimensionality of the observation vector
  unsigned S     = p->S;    //
  cmReal_t alpha = 6.63261; // alpha norm coeff

  if((rc = cmChordFinal(p)) != cmOkRC )
    return rc;

  // Create the hidden markov model
  cmChmmInit( p->h, N, K, D, p->iV, p->aM);

  // load the GMM's for each markov state
  cmReal_t mixCoeff = 1.0;
  bool     diagFl   = false;
  for(i=0; i<N; ++i)
    if((rc = cmChmmSetGmm(p->h, i, &mixCoeff, p->uM + (i*D), p->sMM+(i*D*D), diagFl )) != cmOkRC )
      return rc;


  // Allocate memory
  //           chromaM[D,T]               tsM[S,T]                   cdtsV[T]
  /*
  unsigned n = ALIGN_T(D*T,cmReal_t) + ALIGN_T(S*T,cmReal_t) + ALIGN_T(T,cmReal_t);
  p->chromaM = cmArrayResizeZ(c, &p->memB, n, cmReal_t);
  p->tsM     = p->chromaM + ALIGN_T(D*T,cmReal_t);
  p->cdtsV   = p->tsM     + ALIGN_T(S*T,cmReal_t);
  p->T       = T;
  */

  p->chromaM = cmMemResizeZ( cmReal_t, p->chromaM, p->D*T );
  p->tsM     = cmMemResizeZ( cmReal_t, p->tsM,     p->S*T );
  p->cdtsV   = cmMemResizeZ( cmReal_t, p->cdtsV,   p->D*T );
  p->T       = T;

  // Allocate local memory
  //  qV[], triadIntV[]        triadSeqV[]       tsNormsV[]
  /*
  n = 2*ALIGN_B(T,unsigned) + ALIGN_B(T,int) + ALIGN_B(T,cmReal_t);
  cmArray mem;
  cmArrayAlloc(c, &mem);

  unsigned* qV        = (unsigned*) cmArrayResize(c, &mem, n, char);
  unsigned* triadSeqV = (unsigned*) (qV        + ALIGN_T(T,unsigned));
  int*      triadIntV = (int*)      (triadSeqV + ALIGN_T(T,unsigned));
  cmReal_t* tsNormsV  = (cmReal_t*) (triadIntV + ALIGN_T(T,int));
  */

  //unsigned qV[T];
  //unsigned triadSeqV[T];
  //int triadIntV[T];
  //cmReal_t tsNormsV[T];


  unsigned* qV        = cmMemAlloc( unsigned, T );
  unsigned* triadSeqV = cmMemAlloc( unsigned, T );
  int*      triadIntV = cmMemAlloc( int,      T );
  cmReal_t* tsNormsV  = cmMemAlloc( cmReal_t, T );


  // Take the alpha norm of chroma and store the result in p->chromaM[]
  for(i=0; i<T; ++i)
    p->chromaM[i] = cmVOR_AlphaNorm( chromaM + (i*D), D, alpha);      

  cmVOR_DivVVS(p->chromaM,D*T,chromaM, cmVOR_AlphaNorm(p->chromaM,T,alpha));


  // Train the HMM iniital state prob. p->h->iV[] and transition matrix p->h->aM[]
  unsigned flags   = kNoTrainMixCoeffChmmFl | kNoTrainMeanChmmFl | kNoTrainCovarChmmFl;
  unsigned iterCnt = 40;
  if( chromaM != NULL && T > 0 )
    if((rc = cmChmmTrain(p->h, p->chromaM, p->T, iterCnt, 0, flags )) != cmOkRC )
      goto errLabel;

  // Find the most likely chords using a Viterbi decoding of the chroma.
  cmChmmDecode(p->h,p->chromaM,T,qV);


  // Reorder the chord sequence cmcording to circle of fifths
  unsigned map[] = {0, 14, 4, 18, 8, 22, 12, 2, 16, 6, 20, 10, 17, 7, 21, 11, 1, 15, 5, 19, 9, 23, 13, 3 };

  for(i=0; i<T; ++i)
    qV[i] = map[ qV[i] ];

  //cmVOU_PrintL("qV:\n",p->obj.err.rpt,1,T,qV);

  // Smooth the chord sequence with a median filter.
  cmVOU_MedianFilt(qV,T,3,triadSeqV,1);

  //cmVOU_PrintL("triadSeqV:\n",p->obj.err.rpt,1,T,triadSeqV);

  // Calculate the chord change distance on the circle of fifths.
  int d = 0;
  for(i=0; i<T; ++i)
  {
    int v = abs(d);

    assert(v<N);

    v = (v<=11) ? v : -(12-(v-12));

    if( i > 0 )
      triadIntV[i-1] = (d < 0 ? -1 : 1) * v;

    if( i + 1 < T)
      d = triadSeqV[i+1] - triadSeqV[i];    
  }

  // Project chroma into a 6D tonal space.
  cmVOR_MultMMM( p->tsM,S,T,p->phiM,p->chromaM,D);

  // Find the norm of p->tsM[6,T]
  cmVOR_Fill(tsNormsV,T,0);
  for(i=0; i<T; ++i)
    tsNormsV[i] = cmVOR_MultSumVV( p->tsM + (i*S), p->tsM + (i*S), S );
  
  cmVOR_PowVS(tsNormsV,T,0.5);

  // Take the cosine distance.
  p->cdtsV[0] = 1;
  for(i=1; i<T; ++i)
    p->cdtsV[i] = cmVOR_MultSumVV( p->tsM + ((i-1)*S), p->tsM + (i*S), S );

  for(i=0; i<T-1; ++i)
    p->cdtsV[i+1] /= tsNormsV[i] * tsNormsV[i+1];
  
  //cmVOR_PrintL("tsNormsV:\n",p->obj.err.rpt,1,T,tsNormsV);
  //cmVOR_PrintL("CDTS:\n",    p->obj.err.rpt,1,T,p->cdtsV);


  p->triadSeqMode = cmVOU_Mode(triadSeqV,T);
  p->triadSeqVar  = cmVOU_Variance(triadSeqV,T,NULL);
  p->triadIntMean = cmVOI_Mean(triadIntV,T);
  p->triadIntVar  = cmVOI_Variance(triadIntV,T,&p->triadIntMean);


  cmVOR_MeanM(     p->tsMeanV, p->tsM, S, T, 1 );
  cmVOR_VarianceM( p->tsVarV,  p->tsM, S, T, p->tsMeanV, 1);

  p->cdtsMean = cmVOR_Mean(     p->cdtsV, T );
  p->cdtsVar  = cmVOR_Variance( p->cdtsV, T, &p->cdtsMean );

  /*
  cmReal_t tsMt[T*S];
  cmKbRecd kb;
  cmPlotInitialize(NULL);
  cmPlotSetup("Chords",1,1);
  cmCtxPrint(c,"%s\n","press any key");
  //cmPlotLineD( NULL, NULL, p->cdtsV, NULL, T, NULL, kSolidPlotLineId );
  cmVOR_Transpose(tsMt,p->tsM,S,T);
  cmPlotLineMD(NULL, tsMt, NULL, T, S, kSolidPlotLineId);
  cmPlotDraw();
  cmKeyPress(&kb);
  cmPlotFinalize();
  */

 errLabel:
  cmMemPtrFree(&qV);
  cmMemPtrFree(&triadSeqV);
  cmMemPtrFree(&triadIntV);
  cmMemPtrFree(&tsNormsV);

  return rc;
}

cmRC_t     cmChordFinal(    cmChord* p )
{ return cmOkRC; }

void       cmChordTest( cmRpt_t* rpt, cmLHeapH_t lhH, cmSymTblH_t stH )
{
  cmCtx c;
  cmCtxAlloc(&c,rpt,lhH,stH);

  cmChord* p = cmChordAlloc(&c,NULL,NULL,0);

  cmChordFree(&p);
}


