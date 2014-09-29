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
#include "cmFile.h"
#include "cmFileSys.h"
#include "cmProcObj.h"
#include "cmProcTemplate.h"
#include "cmAudioFile.h"
#include "cmMath.h"
#include "cmProc.h"
#include "cmVectOps.h"
#include "cmKeyboard.h"
#include "cmGnuPlot.h"
#include "cmTime.h"
#include "cmMidi.h"
#include "cmProc2.h"


//------------------------------------------------------------------------------------------------------------
cmArray* cmArrayAllocate( cmCtx* c, cmArray* ap, unsigned eleCnt, unsigned eleByteCnt, unsigned flags )
{
  cmArray* p = cmObjAlloc( cmArray, c, ap );

  if( eleCnt > 0 &&  eleByteCnt > 0 )
    if( cmArrayInit(p, eleCnt, eleByteCnt, flags ) != cmOkRC )
      cmArrayFree(&p);
  return cmOkRC;
}

cmRC_t   cmArrayFree(  cmArray** pp )
{
  cmRC_t rc = cmOkRC;
  cmArray* p = *pp;
  
  if( pp == NULL || *pp == NULL )
    return cmOkRC;

  if((rc = cmArrayFinal(p)) != cmOkRC )
    return rc;

  cmMemPtrFree(&p->ptr);

  cmObjFree(pp);
  return rc;
}

cmRC_t   cmArrayInit(  cmArray* p, unsigned eleCnt, unsigned eleByteCnt, unsigned flags )
{
  cmRC_t rc = cmOkRC;

  if((rc  = cmArrayFinal(p)) != cmOkRC )
    return rc;

  p->allocByteCnt = eleCnt * eleByteCnt;
  p->ptr          = cmIsFlag(flags,kZeroArrayFl) ? cmMemResizeZ( char, p->ptr, p->allocByteCnt ) : cmMemResize( char, p->ptr, p->allocByteCnt );
  p->eleCnt       = eleCnt;
  p->eleByteCnt   = eleByteCnt;

  return rc;
}

cmRC_t   cmArrayFinal( cmArray* p )
{ return cmOkRC; }


char*   cmArrayReallocDestroy(cmArray* p, unsigned newEleCnt, unsigned newEleByteCnt, unsigned flags )
{
  // if memory is expanding
  if( newEleCnt * newEleByteCnt > p->allocByteCnt )
    cmArrayInit( p, newEleCnt, newEleByteCnt, flags );
  else
  {
    // ... otherwise memory is contrcmting 
    p->eleCnt     = newEleCnt;
    p->eleByteCnt = newEleByteCnt;

    if( cmIsFlag( flags, kZeroArrayFl ))
      memset(p->ptr,0,p->eleByteCnt);
    
  }

  return p->ptr;
}

void     cmArrayReallocDestroyV(cmArray* p,  int eleByteCnt,unsigned flags,  ... )
{
  unsigned i;
  unsigned eleCnt  = 0;
  unsigned argCnt  = 0;
  va_list  vl0,vl1;

  assert(eleByteCnt>0);

  va_start(vl0,flags);
  va_copy(vl1,vl0);

  while( va_arg(vl0,void**) != NULL )
  {
    int argEleCnt = va_arg(vl0,int);

    assert(argEleCnt>0);

    eleCnt +=  argEleCnt;

    ++argCnt;
  }
  va_end(vl0);

  char* a = cmArrayReallocDestroy(p,eleCnt,eleByteCnt,flags);

  for(i=0; i<argCnt; ++i)
  {
    void**   vp = va_arg(vl1,void**);
    unsigned n  = va_arg(vl1,unsigned);
    *vp = a;
    a += n*eleByteCnt;
  }

  va_end(vl1);

}

char*   cmArrayReallocPreserve(cmArray* p, unsigned newEleCnt, unsigned newEleByteCnt, unsigned flags )
{
  unsigned cn = p->eleCnt * p->eleByteCnt;
  unsigned dn = newEleCnt * newEleByteCnt;

  if( dn > p->allocByteCnt )
    p->allocByteCnt = dn;


  p->ptr = cmIsFlag(flags,kZeroArrayFl ) ? cmMemResizePZ( char, p->ptr, cn ) : cmMemResizeP( char, p->ptr, cn);

  p->eleCnt    = newEleCnt;
  p->eleByteCnt= newEleByteCnt;

  return p->ptr;
}

//------------------------------------------------------------------------------------------------------------
cmAudioFileWr*     cmAudioFileWrAlloc( cmCtx* c, cmAudioFileWr* ap, unsigned procSmpCnt, const char* fn, double srate, unsigned chCnt, unsigned bitsPerSample )
{
  cmAudioFileWr* p = cmObjAlloc( cmAudioFileWr, c, ap );

  if( cmAudioFileWrInit( p, procSmpCnt, fn, srate, chCnt, bitsPerSample ) != cmOkRC )
    cmObjFree(&p);
  return p;
}

cmRC_t             cmAudioFileWrFree(  cmAudioFileWr** pp )
{
  cmRC_t rc = cmOkRC;

  if( pp != NULL && *pp != NULL )
  {
    cmAudioFileWr* p = *pp;

    if((rc = cmAudioFileWrFinal(p)) == cmOkRC )
    {
      cmMemPtrFree(&p->bufV);
      cmMemPtrFree(&p->fn );
      cmObjFree(pp);
    }
  }
  return rc;
}

cmRC_t             cmAudioFileWrInit(  cmAudioFileWr* p, unsigned procSmpCnt, const char* fn, double srate, unsigned chCnt, unsigned bitsPerSample )
{
  cmRC_t rc;
  cmRC_t afRC;

  if((rc = cmAudioFileWrFinal( p)) != cmOkRC )
    return rc;


  p->h = cmAudioFileNewCreate( fn, srate, bitsPerSample, chCnt, &afRC, p->obj.err.rpt ); 

  if( afRC != kOkAfRC )
    return cmCtxRtCondition( &p->obj, afRC, "Unable to open the audio file:'%s'", fn );

  p->bufV       = cmMemResize( cmSample_t, p->bufV, procSmpCnt * chCnt );
  p->procSmpCnt = procSmpCnt;
  p->chCnt      = chCnt;
  p->curChCnt   = 0;
  p->fn         = cmMemResizeZ( cmChar_t, p->fn, strlen(fn)+1 );
  strcpy(p->fn,fn);

  return rc;
}

cmRC_t             cmAudioFileWrFinal( cmAudioFileWr* p )
{
  cmRC_t afRC;

  if( p != NULL )
  {
    if( cmAudioFileIsValid( p->h ) )
      if(( afRC = cmAudioFileDelete( &p->h )) != kOkAfRC )
        return cmCtxRtCondition( &p->obj, afRC, "Unable to close the audio file:'%s'", p->fn );

  }
  return cmOkRC;
}

cmRC_t             cmAudioFileWrExec(  cmAudioFileWr* p, unsigned chIdx, const cmSample_t* sp, unsigned sn )
{
  cmRC_t afRC;

  assert( sn <= p->procSmpCnt && chIdx < p->chCnt );

  cmSample_t* buf     = p->bufV + (chIdx * p->procSmpCnt);

  cmVOS_Copy( buf, sn, sp);

  if( sn < p->procSmpCnt )
    cmVOS_Fill( buf+sn, p->procSmpCnt-sn, 0 ); 

  p->curChCnt++;

  if( p->curChCnt == p->chCnt )
  {
    p->curChCnt = 0;
    
    cmSample_t* bufPtrPtr[ p->chCnt ];
    unsigned i = 0;
    for(i=0; i<p->chCnt; ++i)
      bufPtrPtr[i] = p->bufV + (i*p->procSmpCnt);

    if((afRC = cmAudioFileWriteSample( p->h, p->procSmpCnt, p->chCnt, bufPtrPtr )) != kOkAfRC )
      return cmCtxRtCondition( &p->obj, afRC, "Write failed on audio file:'%s'", p->fn );
  }
  return cmOkRC;
}

void cmAudioFileWrTest( cmRpt_t* rpt, cmLHeapH_t lhH, cmSymTblH_t stH )
{
  const char* fn         = "/home/kevin/src/cm/test0.aif";
  double      durSecs    = 10;
  double      srate      = 44100;
  unsigned    chCnt      = 2;
  unsigned    bitsPerSmp = 16;
  unsigned    procSmpCnt = 64;
  double      hz         = 1000;
  unsigned    overToneCnt= 1;
  unsigned    smpCnt     = durSecs * srate;
  unsigned    i;

  cmCtx*         c   = cmCtxAlloc(         NULL, rpt, lhH, stH );
  cmSigGen*      sgp = cmSigGenAlloc(      c,    NULL, procSmpCnt, srate, kWhiteWfId, hz, overToneCnt );
  cmAudioFileWr* awp = cmAudioFileWrAlloc( c,    NULL, procSmpCnt, fn, srate, chCnt, bitsPerSmp );

  for(i=0; i<smpCnt; ++i)
  {
    cmSigGenExec(  sgp );
    cmAudioFileWrExec( awp, 0, sgp->outV, sgp->outN );
    cmAudioFileWrExec( awp, 1, sgp->outV, sgp->outN );
    i += sgp->outN;
  }
  
  printf("Frames:%i\n",smpCnt);

  cmAudioFileWrFree(&awp);
  cmSigGenFree( &sgp );
  cmCtxFree(&c);

  cmAudioFileReportFn( fn, 0, 20, rpt );
}

//------------------------------------------------------------------------------------------------------------
cmMatrixBuf*       cmMatrixBufAllocFile( cmCtx* c, cmMatrixBuf* p, const char* fn )
{
  cmRC_t rc;

  cmMatrixBuf* op = cmObjAlloc( cmMatrixBuf, c, p );

  if( fn != NULL )
    if((rc = cmMatrixBufInitFile(op,fn)) != cmOkRC )
      cmObjFree(&op);

  return op;
}

cmMatrixBuf*       cmMatrixBufAllocCopy(cmCtx* c, cmMatrixBuf* p, unsigned rn, unsigned cn, const cmSample_t* sp )
{
  cmRC_t rc;

  cmMatrixBuf* op = cmObjAlloc( cmMatrixBuf, c, p );
   
  if( sp != NULL && rn > 0 && cn > 0  )
    if((rc = cmMatrixBufInitCopy(op,rn,cn,sp)) != cmOkRC )
      cmObjFree(&op);

  return op;  
}

cmMatrixBuf*       cmMatrixBufAlloc( cmCtx* c, cmMatrixBuf* p, unsigned rn, unsigned cn )
{
  cmRC_t rc;
  cmMatrixBuf* op = cmObjAlloc( cmMatrixBuf, c, p );
  
  if( rn > 0 && cn > 0 )
    if((rc = cmMatrixBufInit(op,rn,cn)) != cmOkRC )
      cmObjFree(&op);

  return op;
}

cmRC_t             cmMatrixBufFree(  cmMatrixBuf** pp )
{
  cmRC_t rc = cmOkRC;

  if( pp != NULL && *pp != NULL )
  {
    cmMatrixBuf* p = *pp;

    if((rc = cmMatrixBufFinal(p)) == cmOkRC )
    {
      cmMemPtrFree(&p->bufPtr);
      cmObjFree(pp);
    }
  }

  return rc;      
}

void _cmMatrixBufGetFileSize( FILE* fp, unsigned* lineCharCntPtr, unsigned* lineCntPtr )
{
  *lineCharCntPtr = 0;
  *lineCntPtr     = 0;


  while( !feof(fp) )
  {
    char ch;
    unsigned charCnt = 0;
    while( (ch = getc(fp)) != EOF )
    {
      charCnt++;

      if( ch == '\n' )
        break;
      
    }

    *lineCntPtr += 1;

    if(charCnt > *lineCharCntPtr )
      *lineCharCntPtr = charCnt;
  }

  *lineCharCntPtr += 5; // add a safety margin

}

cmRC_t _cmMatrixBufGetMatrixSize( cmObj* op, FILE* fp, unsigned lineCharCnt, unsigned lineCnt,  unsigned* rowCntPtr, unsigned * colCntPtr, const char* fn )
{
  unsigned i;

  char lineBuf[ lineCharCnt + 1 ];
  
  *rowCntPtr = 0;
  *colCntPtr = 0;

  for(i=0; i<lineCnt; ++i)
  {
    if(fgets(lineBuf,lineCharCnt,fp)==NULL)
    {
      // if the last line is blank then return from here
      if( feof(fp) )
        return cmOkRC;

      return cmCtxRtCondition( op, cmSystemErrorRC, "A read error occured on the matrix file:'%s'.",fn);
    }
    assert( strlen(lineBuf) < lineCharCnt );

    char* lp = lineBuf;
    char* tp;

    // eat any leading white space
    while( (*lp) && isspace(*lp) )
      ++lp;

    // if the line was blank then skip it
    if( strlen(lp) == 0 || *lp == '#' )
      continue;


    (*rowCntPtr) += 1;

    unsigned colCnt;

    for(colCnt=0; (tp = strtok(lp," ")) != NULL; ++colCnt )
      lp = NULL;


    if( colCnt > *colCntPtr )
      *colCntPtr = colCnt;
  }

  return cmOkRC;
}

double _cmMatrixBufStrToNum( cmObj* op, const char* cp )
{
  double v;

  if( sscanf(cp,"%le ",&v) != 1 )
     cmCtxRtCondition( op, cmArgAssertRC, "Parse error reading matrix file.");

  return v;
}

cmRC_t  _cmMatrixBufReadFile(cmObj* op, FILE* fp, cmSample_t* p, unsigned lineCharCnt, unsigned rn, unsigned cn)
{
  char lineBuf[ lineCharCnt+1 ];
  unsigned ci = 0;
  unsigned ri = 0;

  while( fgets(lineBuf,lineCharCnt,fp) != NULL )
  {
    char* lp = lineBuf;
    char* tp;

    while( (*lp) && isspace(*lp) )
      lp++;

    if( strlen(lp) == 0 || *lp == '#' )
      continue;

    for(ci=0; (tp = strtok(lp," ")) != NULL; ++ci )
    {
      p[ (ci*rn) + ri ] = _cmMatrixBufStrToNum(op,tp); //atof(tp);
      lp = NULL;
    }

    ++ri;
  }

  return cmOkRC;
}

cmRC_t             cmMatrixBufInitFile(  cmMatrixBuf* p, const char* fn )
{
  cmRC_t   rc;
  FILE*    fp;
  unsigned lineCharCnt;
  unsigned lineCnt;
  unsigned rn;
  unsigned cn;

  if((fp = fopen(fn,"rt")) == NULL )
    return cmCtxRtCondition( &p->obj, cmSystemErrorRC, "Unable to open the matrix file:'%s'", fn );

  // get the length of the longest line in the file 
  _cmMatrixBufGetFileSize(fp,&lineCharCnt,&lineCnt);

  rewind(fp);

  // get the count of matrix rows and columns
  if((rc=_cmMatrixBufGetMatrixSize( &p->obj, fp, lineCharCnt, lineCnt, &rn, &cn, fn )) != cmOkRC )
    goto errLabel;

  rewind(fp);

  // allocate the matrix memory
  cmMatrixBufInit(p,rn,cn);

  // fill the matrix from the file
  rc = _cmMatrixBufReadFile(&p->obj,fp,p->bufPtr,lineCharCnt,rn,cn);
  
 errLabel:

  if( rc != cmOkRC )
    cmMatrixBufFinal(p);

  fclose(fp);
  return rc;
}

cmRC_t             cmMatrixBufInitCopy( cmMatrixBuf* p, unsigned rn, unsigned cn, const cmSample_t* sp )
{
  cmRC_t rc;
  if((rc = cmMatrixBufInit(p,rn,cn)) != cmOkRC )
    return rc;

  cmVOS_Copy(p->bufPtr,(rn*cn),sp);
  return rc;
}

cmRC_t             cmMatrixBufInit(  cmMatrixBuf* p, unsigned rn, unsigned cn )
{
  cmRC_t rc;

  if((rc = cmMatrixBufFinal(p)) != cmOkRC )
    return rc;

  p->rn     = rn;
  p->cn     = cn;
  p->bufPtr = cmMemResize( cmSample_t, p->bufPtr, rn*cn );

  return cmOkRC;
}

cmRC_t             cmMatrixBufFinal( cmMatrixBuf* p )
{ return cmOkRC; }

cmSample_t* cmMatrixBufColPtr( cmMatrixBuf* p, unsigned ci )
{ assert(ci<p->cn); return p->bufPtr + (ci * p->rn); }

cmSample_t* cmMatrixBufRowPtr( cmMatrixBuf* p, unsigned ri )
{ assert(ri<p->rn); return p->bufPtr + ri; }

void cmMatrixBufTest( cmRpt_t* rpt, cmLHeapH_t lhH, cmSymTblH_t stH )
{
  cmSample_t v[]   = {1,2,2,3};
  cmCtx*       c   = cmCtxAlloc(NULL,rpt,lhH,stH);
  cmMatrixBuf* mbp = cmMatrixBufAllocFile(c, NULL, "temp.mat" );
  cmMatrixBuf* vbp = cmMatrixBufAllocCopy(c, NULL, 4,1,v);
  unsigned     i;

  printf("rn:%i cn:%i\n",mbp->rn,mbp->cn);
  //cmVOS_Print( stdout, 10, 10, mbp->bufPtr );

  printf("%.1f\n ",cmVOS_Median( cmMatrixBufColPtr(vbp,0),vbp->rn));


  for(i=0; i<mbp->cn; ++i)
  {
    //cmVOS_Print( stdout, 1, mbp->cn, cmMatrixBufColPtr(c,mbp,i) );
    printf("%.1f, ",cmVOS_Median( cmMatrixBufColPtr(mbp,i),mbp->rn));
  }
  printf("\n");

  cmMatrixBufFree(&mbp);
  cmMatrixBufFree(&vbp);
  cmCtxFree(&c);

}

//------------------------------------------------------------------------------------------------------------


cmSigGen* cmSigGenAlloc( cmCtx* c,  cmSigGen* p, unsigned procSmpCnt, double srate, unsigned wfId, double fundFrqHz, unsigned overToneCnt )
{ 
  cmSigGen* op =  cmObjAlloc( cmSigGen, c, p ); 

  if( procSmpCnt > 0 && srate > 0 && wfId != kInvalidWfId )
    if( cmSigGenInit(  op, procSmpCnt, srate, wfId, fundFrqHz, overToneCnt ) != cmOkRC )
      cmObjFree(&op);

  return op;
}

cmRC_t    cmSigGenFree(  cmSigGen** pp )
{

  cmRC_t rc = cmOkRC;

  if( pp != NULL && *pp != NULL )
  {
    cmSigGen* p = *pp;
    if((rc = cmSigGenFinal(p)) == cmOkRC )
    {
      cmMemPtrFree(&p->outV);
      cmObjFree(pp);
    }
  }
  return rc;
}

cmRC_t    cmSigGenInit( cmSigGen* p, unsigned procSmpCnt, double srate, unsigned wfId, double fundFrqHz, unsigned overToneCnt )
{
  assert( srate > 0 && procSmpCnt > 0 );

  p->outV        = cmMemResize( cmSample_t, p->outV, procSmpCnt );
  p->outN        = procSmpCnt;
  p->wfId        = wfId;
  p->overToneCnt = overToneCnt;
  p->fundFrqHz   = fundFrqHz;
  p->phase       = 0;
  p->delaySmp    = 0;
  p->srate       = srate;

  return cmOkRC;
}

cmRC_t   cmSigGenFinal( cmSigGen* p )
{ return cmOkRC; }


cmRC_t    cmSigGenExec(  cmSigGen* p )
{
  switch( p->wfId )
  {

    case kSineWfId:     p->phase    = cmVOS_SynthSine(     p->outV, p->outN, p->phase, p->srate, p->fundFrqHz );  break;
    case kCosWfId:      p->phase    = cmVOS_SynthCosine(   p->outV, p->outN, p->phase, p->srate, p->fundFrqHz );  break;
    case kSquareWfId:   p->phase    = cmVOS_SynthSquare(   p->outV, p->outN, p->phase, p->srate, p->fundFrqHz, p->overToneCnt ); break; 
    case kTriangleWfId: p->phase    = cmVOS_SynthTriangle( p->outV, p->outN, p->phase, p->srate, p->fundFrqHz, p->overToneCnt ); break; 
    case kSawtoothWfId: p->phase    = cmVOS_SynthSawtooth( p->outV, p->outN, p->phase, p->srate, p->fundFrqHz, p->overToneCnt ); break; 
    case kWhiteWfId:                  cmVOS_Random(        p->outV, p->outN, -1.0, 1.0 ); break;
    case kPinkWfId:     p->delaySmp = cmVOS_SynthPinkNoise(p->outV, p->outN, p->delaySmp ); break;
    case kPulseWfId:    p->phase    = cmVOS_SynthPulseCos( p->outV, p->outN, p->phase, p->srate, p->fundFrqHz, p->overToneCnt ); break;
    case kImpulseWfId:  p->phase    = cmVOS_SynthImpulse(  p->outV, p->outN, p->phase, p->srate, p->fundFrqHz ); break;
    case kSilenceWfId:                cmVOS_Fill(          p->outV, p->outN, 0 ); break;
    case kPhasorWfId:   p->phase    = cmVOS_SynthPhasor(   p->outV, p->outN, p->phase, p->srate, p->fundFrqHz ); break;
    case kSeqWfId:      p->phase    = cmVOS_Seq(           p->outV, p->outN, p->phase, 1 ); break;
    case kInvalidWfId:  
    default:
      return cmCtxRtAssertFailed( &p->obj, 0, "Invalid waveform shape.");
      
  }
  return cmOkRC;
}

//------------------------------------------------------------------------------------------------------------

cmDelay*          cmDelayAlloc( cmCtx* c, cmDelay* ap, unsigned procSmpCnt, unsigned delaySmpCnt )
{
  cmDelay* p = cmObjAlloc( cmDelay, c, ap );

  if( procSmpCnt > 0 && delaySmpCnt > 0 )
    if( cmDelayInit( p,procSmpCnt,delaySmpCnt) != cmOkRC  && ap == NULL )
      cmObjFree(&p);

  return p;
}

cmRC_t            cmDelayFree(  cmDelay** pp )
{
  cmRC_t rc = cmOkRC;

  if( pp != NULL && *pp != NULL )
  {
    cmDelay* p = *pp;
    if((rc = cmDelayFinal(*pp)) == cmOkRC )
    {
      cmMemPtrFree(&p->bufPtr);
      cmObjFree(pp);
    }
  }
  return rc;
}

 cmRC_t            cmDelayInit(  cmDelay* p, unsigned procSmpCnt, unsigned delaySmpCnt )
{
  p->procSmpCnt  = procSmpCnt;
  p->delaySmpCnt = delaySmpCnt;
  p->bufSmpCnt   = delaySmpCnt + procSmpCnt;
  p->bufPtr      = cmMemResizeZ( cmSample_t,  p->bufPtr, p->bufSmpCnt);
  p->delayInIdx  = 0;
  p->outCnt      = 1;
  p->outV[0]     = p->bufPtr;
  p->outN[0]     = p->procSmpCnt;
  p->outV[1]     = NULL;
  p->outN[1]     = 0;
  return cmOkRC;
}

cmRC_t            cmDelayFinal( cmDelay* p )
{ return cmOkRC; }

cmRC_t            cmDelayCopyIn(  cmDelay* p, const cmSample_t* sp, unsigned sn )
{
  assert(sn<=p->procSmpCnt);

  unsigned n0 = cmMin(sn,p->bufSmpCnt - p->delayInIdx); 

  // copy as many samples as possible from the input to the delayInIdx
  cmVOS_Copy(p->bufPtr + p->delayInIdx, n0, sp);
  
  p->delayInIdx = (p->delayInIdx + n0) % p->bufSmpCnt;

  // if there was not enough room to copy all the samples into the end of the buffer ....
  if( n0 < sn )
  {
    assert( p->delayInIdx == 0 );

    // ... then copy the rest to the beginning of the buffer
    unsigned n1 = sn - n0;

    cmVOS_Copy(p->bufPtr,n1, sp + n0 );

    p->delayInIdx = (p->delayInIdx + n1) % p->bufSmpCnt;    
  }


  return cmOkRC;
}

cmRC_t           cmDelayAdvance( cmDelay* p, unsigned sn )
{
  // advance the output by sn and make sn samples available 
  int delayOutIdx = ((p->outV[0] - p->bufPtr) + sn) % p->bufSmpCnt;

  p->outV[0] = p->bufPtr + delayOutIdx;
  p->outN[0] = cmMin(p->bufSmpCnt - delayOutIdx , sn );
  p->outCnt  = p->outN[0] == sn ? 1    : 2 ;
  p->outV[1] = p->outCnt  == 1  ? NULL : p->bufPtr;
  p->outN[1] = p->outCnt  == 1  ? 0    : sn - p->outN[0];

  return cmOkRC;
}

cmRC_t           cmDelayExec( cmDelay* p, const cmSample_t* sp, unsigned sn, bool bypassFl )
{
  cmRC_t rc = cmOkRC;
  if( bypassFl )
    memcpy(p->outV,sp,sn*sizeof(cmSample_t));
  else
  {
    cmDelayCopyIn(p,sp,sn);
    rc = cmDelayAdvance(p,sn);
  }

  return rc;
}

void  cmDelayTest( cmRpt_t* rpt, cmLHeapH_t lhH, cmSymTblH_t stH )
{
  cmCtx    ctx;
  cmDelay  delay;
  cmSigGen sigGen;

  unsigned procCnt     = 4;
  unsigned procSmpCnt  = 5;
  unsigned delaySmpCnt = 3;
  unsigned i;

  cmCtx*      c = cmCtxAlloc( &ctx, rpt, lhH, stH );
  cmDelay*  dlp = cmDelayAlloc(  c, &delay,  procSmpCnt, delaySmpCnt );
  cmSigGen* sgp = cmSigGenAlloc( c, &sigGen, procSmpCnt, 0, kSeqWfId,0, 0); 

  for(i=0; i<procCnt; ++i)
  {
    cmSigGenExec(sgp);

    cmDelayExec(dlp,sgp->outV,sgp->outN,false);

    //cmVOS_Print( c->outFp, 1, sgp->outN, sgp->outV, 5, 0 );
    cmCtxPrint(c,"%i %i : ",i,0); 
    cmVOS_PrintE( rpt, 1, dlp->outN[0], dlp->outV[0] );

    if( dlp->outN[1] > 0 )
    {
      cmCtxPrint(c,"%i %i : ",i,1); 
      cmVOS_PrintE( rpt, 1, dlp->outN[1], dlp->outV[1] );
    }

  }

  cmSigGenFinal(sgp);
  cmDelayFinal(dlp);
  cmCtxFinal(c);
  
}

//------------------------------------------------------------------------------------------------------------

cmFIR* cmFIRAllocKaiser(cmCtx* c, cmFIR* p, unsigned procSmpCnt, double srate, double passHz, double stopHz, double passDb, double stopDb )
{
  cmFIR* op = cmObjAlloc( cmFIR, c, p );

  if( procSmpCnt > 0 && srate > 0  )
    if( cmFIRInitKaiser(op,procSmpCnt,srate,passHz,stopHz,passDb,stopDb) != cmOkRC )
      cmObjFree(&op);
  return op;
}

cmFIR* cmFIRAllocSinc( cmCtx* c, cmFIR* p, unsigned procSmpCnt, double srate, unsigned sincSmpCnt, double fcHz, unsigned flags )
{
  cmFIR* op = cmObjAlloc( cmFIR, c, p );

  if( srate > 0 && sincSmpCnt > 0 )
    if( cmFIRInitSinc(op,procSmpCnt,srate,sincSmpCnt,fcHz,flags) != cmOkRC )
      cmObjFree(&op);
  return op;  
}

cmRC_t cmFIRFree(       cmFIR** pp )
{
  cmRC_t rc = cmOkRC;

  if( pp != NULL && *pp != NULL)
  {
    cmFIR* p = *pp;
    
    if((rc = cmFIRFinal(*pp)) != cmOkRC )
    {
      cmMemPtrFree(&p->coeffV);
      cmMemPtrFree(&p->outV);
      cmMemPtrFree(&p->delayV);
      cmObjFree(pp);
    }
  }

  return rc;
}

cmRC_t cmFIRInitKaiser( cmFIR* p, unsigned procSmpCnt, double srate, double passHz, double stopHz, double passDb, double stopDb )
{
  // pass/stop frequencies above nyquist produce incorrect results
  assert( passHz <= srate/2 && stopHz<=srate/2);

  // based on Orfandis, Introduction to Signal Processing, p.551 Prentice Hall, 1996

  double fcHz     = (passHz + stopHz) / 2;  // fc is half way between passHz and stopHz
  double dHz      = fabs(stopHz-passHz);
  //double signFcmt = stopHz > passHz ? 1 : -1;

  // convert ripple spec from db to linear 
  double dPass    = (pow(10,passDb/20)-1) / (pow(10,passDb/20)+1);
  double dStop    = pow(10,-stopDb/20);
  
  // in prcmtice the ripple must be equal in the stop and pass band - so take the minimum between the two
  double d        = cmMin(dPass,dStop);

  // convert the ripple bcmk to db
  double A = -20 * log10(d);

  // compute the kaiser alpha coeff
	double alpha = 0;
	if( A >= 50.0 )                 // for ripple > 50
		alpha = 0.1102 * (A-8.7);
	else                            // for ripple <= 21
	{
    if( A > 21 )
      alpha = (0.5842 * (pow(A-21.0,0.4))) + (0.07886*(A-21));

	}
	 
	double D =  0.922;

	if( A > 21 )
		D = (A - 7.95) / 14.36;


	// compute the filter order
	unsigned N = (unsigned)(floor(D * srate / dHz) + 1);
	 
	//if N is even
	if( cmIsEvenU(N) )
		N = N + 1;  

  //printf("fc=%f df=%f dPass=%f dStop=%f d=%f alpha=%f A=%f D=%f N=%i\n",fcHz,dHz,dPass,dStop,d,alpha,A,D,N);
  
  // form an ideal FIR LP impulse response based on a sinc function
  cmFIRInitSinc(p,procSmpCnt,srate,N,fcHz,0);

  // compute a kaiser function to truncate the sinc 
  double wnd[ N ];
  cmVOD_Kaiser( wnd, N, alpha );

  // apply the kaiser window to the sinc function
  cmVOD_MultVV( p->coeffV, p->coeffCnt, wnd );

  double sum = cmVOD_Sum(p->coeffV,p->coeffCnt);
  cmVOD_DivVS(p->coeffV,p->coeffCnt,sum );

  //cmVOD_Print(stdout,1,p->coeffCnt,p->coeffV);

  return cmOkRC;

}

cmRC_t cmFIRInitSinc( cmFIR* p, unsigned procSmpCnt, double srate, unsigned sincSmpCnt, double fcHz, unsigned flags )
{
  cmRC_t rc;

  if((rc = cmFIRFinal(p)) != cmOkRC )
    return rc;

  p->coeffCnt = sincSmpCnt;
  p->outV     = cmMemResizeZ( cmSample_t, p->outV, procSmpCnt );
  p->outN     = procSmpCnt;
  p->coeffV   = cmMemResizeZ( double, p->coeffV, p->coeffCnt );
  p->delayV   = cmMemResizeZ( double, p->delayV, p->coeffCnt-1 ); // there is always one less delay than coeff
  p->delayIdx = 0;

  cmVOD_LP_Sinc(p->coeffV, p->coeffCnt, srate, fcHz, cmIsFlag(flags,kHighPassFIRFl) ? kHighPass_LPSincFl : 0 );

  return cmOkRC;
}

cmRC_t cmFIRFinal( cmFIR* p )
{ return cmOkRC; }

cmRC_t cmFIRExec( cmFIR* p, const cmSample_t* sbp, unsigned sn )
{
  unsigned   delayCnt   = p->coeffCnt-1;
  int        di         = p->delayIdx;     
  const cmSample_t* sep = sbp + sn;
  cmSample_t*       op  = p->outV;

  assert( di < delayCnt );
  assert( sn <= p->outN );

  // for each input sample
  while( sbp < sep )
  {		
    // advance the delay line
    p->delayIdx = (p->delayIdx + 1) % delayCnt;

    const double* cbp = p->coeffV;
    const double* cep = cbp + p->coeffCnt;

    // mult input sample by coeff[0] 
    double       v   = *sbp  * *cbp++;

    // calc the output sample
    while( cbp<cep)
    {
      // note that the delay is being iterated bcmkwards
      if( di == -1 )
      di=delayCnt-1;

      v += *cbp++ * p->delayV[di];			

      --di;
    }

    // store the output sample
    *op++ = v;					

    // insert the input sample
    p->delayV[ p->delayIdx ] = *sbp++;

    // store the position of the newest ele in the delay line
    di = p->delayIdx;
  }

  return cmOkRC;
}

void cmFIRTest( cmRpt_t* rpt, cmLHeapH_t lhH, cmSymTblH_t stH )
{
  unsigned N = 512;
  cmKbRecd kb;
  cmCtx c;
  cmCtxInit(&c,rpt,lhH,stH);

  double srate         = N;
  unsigned procSmpCnt  = N;


  cmPlotSetup("Test Proc Impl",2,1);


  cmSample_t in[ procSmpCnt ];
  

  cmVOS_Fill(in,procSmpCnt,0);
  in[0] = 1;

  cmVOS_Random(in,procSmpCnt, -1.0, 1.0 );

  cmFIR* ffp    = cmFIRAllocKaiser( &c, NULL, procSmpCnt,srate, srate*0.025, srate/2,  10, 60 );
  //cmFIR* ffp    = cmFIRAllocSinc( &c, NULL, 32, 1000, 0 );
  cmFftSR* ftp    = cmFftAllocSR(      &c, NULL, ffp->outV, ffp->outN, kToPolarFftFl );

  cmFIRExec( ffp, in, procSmpCnt ); 
  cmFftExecSR( ftp, NULL, 0 );
  cmVOR_AmplitudeToDb(ftp->magV,ftp->binCnt,ftp->magV);
  

  printf("coeff cnt:%i\n",ffp->coeffCnt );

  cmPlotClear();
  cmPlotLineR( "test",  NULL, ftp->magV, NULL, ftp->binCnt, NULL, kSolidPlotLineId  );    
  cmPlotDraw();
  cmKeyPress(&kb);    

  cmFftFreeSR(&ftp);
  cmFIRFree(&ffp);
}

//------------------------------------------------------------------------------------------------------------


cmFuncFilter* cmFuncFilterAlloc( cmCtx* c, cmFuncFilter* p, unsigned procSmpCnt, cmFuncFiltPtr_t funcPtr, void* userPtr, unsigned wndSmpCnt )
{
  cmRC_t        rc;

  cmFuncFilter* op = cmObjAlloc( cmFuncFilter,c, p );


  if( procSmpCnt > 0 && funcPtr != NULL && wndSmpCnt > 0 )
  {
    if( cmShiftBufAlloc(c,&p->shiftBuf,0,0,0) != NULL )
      if((rc = cmFuncFilterInit(op,procSmpCnt,funcPtr,userPtr,wndSmpCnt)) != cmOkRC )
      {
        cmShiftBuf* sbp = &p->shiftBuf;
        cmShiftBufFree(&sbp);
        cmObjFree(&op);
      }
  }
  return op;
}

cmRC_t        cmFuncFilterFree(  cmFuncFilter** pp )
{
  cmRC_t rc = cmOkRC;

  if( pp!=NULL && *pp != NULL )
  {
    cmFuncFilter* p = *pp;
  
    if((rc = cmFuncFilterFinal(*pp)) == cmOkRC )
    {
      cmShiftBuf* sbp = &p->shiftBuf;
      cmShiftBufFree(&sbp);
      cmMemPtrFree(&p->outV);
      cmObjFree(pp);
    }
  }
  return rc;
}

cmRC_t        cmFuncFilterInit(  cmFuncFilter* p, unsigned procSmpCnt, cmFuncFiltPtr_t funcPtr, void* userPtr, unsigned wndSmpCnt )
{
  cmRC_t rc;
  if(( rc = cmFuncFilterFinal(p)) != cmOkRC )
    return rc;

  // The shift buffer always consits of the p->wndSmpCnt-1 samples from the previous 
  // exec followed by the latest procSmpCnt samples at the end of the buffer 
  cmShiftBufInit( &p->shiftBuf, procSmpCnt, wndSmpCnt + procSmpCnt - 1, procSmpCnt );

  p->outV         = cmMemResizeZ( cmSample_t, p->outV, procSmpCnt);
  p->outN         = procSmpCnt;
  p->funcPtr      = funcPtr;
  p->curWndSmpCnt = 1;
  p->wndSmpCnt    = wndSmpCnt;
  return rc;
}

cmRC_t        cmFuncFilterFinal( cmFuncFilter* p )
{ return cmOkRC; }


cmRC_t        cmFuncFilterExec(  cmFuncFilter* p, const cmSample_t* sp, unsigned sn )
{
  assert( sn <= p->outN);

  // The window used by this function is always causal.  At the very beginning of the signal
  // the window length begins at 1 and increases until is has the length p->wndSmpCnt.
  // Note that this approach ignores any zeros automatically prepended to the beginning of the 
  // signal by the shift buffer.  The first window processed always has a length of 1 and 
  // begins with the first actual sample given to the shift buffer. Successive windows increase
  // by one and start at the first actual sample until the full window length is available 
  // from the shift buffer. At this point the window length remains constant and it is hopped 
  // by one sample for each window.

  while(cmShiftBufExec(&p->shiftBuf,sp,sn))
  {  
    const cmSample_t* fsp = p->shiftBuf.outV;
  
    cmSample_t*       dp  = p->outV;
    cmSample_t*       ep  = p->outV + sn; // produce as many output values as there are input samples

    // for each output sample
    while( dp < ep )
    {

      // the source range should never extend outside the shift buffer
      assert( fsp + p->curWndSmpCnt <= p->shiftBuf.outV + p->shiftBuf.wndSmpCnt );

      // calc the next output value
      *dp++ = p->funcPtr( fsp, p->curWndSmpCnt, p->userPtr );

      // if the window has not yet achieved its full length ...
      if( p->curWndSmpCnt < p->wndSmpCnt )
        ++p->curWndSmpCnt;  // ... then increase its length by 1
      else
        ++fsp;              // ... otherwise shift it ahead by 1
    }
  }
  return cmOkRC;
}

cmSample_t cmFuncFiltTestFunc( const cmSample_t* sp, unsigned sn, void* vp )
{
  //printf("% f % f   %p % i\n",*sp,*sp+(sn-1),sp,sn);
  cmSample_t v = cmVOS_Median(sp,sn);
  printf("%f ",v);
  return v;
    //return *sp;
}

void cmFuncFilterTest( cmRpt_t* rpt, cmLHeapH_t lhH, cmSymTblH_t stH )
{
  unsigned   procSmpCnt = 1;
  unsigned   N          = 32;
  cmSample_t v[N];
  cmCtx      c;
  unsigned   i;

  cmCtxAlloc(&c,rpt,lhH,stH);

  cmVOS_Seq(v,N,0,1);

  cmVOS_Print(rpt,1,32,v);

  cmFuncFilter* ffp = NULL;

  ffp = cmFuncFilterAlloc( &c, NULL, procSmpCnt, cmFuncFiltTestFunc, NULL, 5 );

  for(i=0; i<N; ++i)
    cmFuncFilterExec(ffp,v+(i*procSmpCnt),procSmpCnt);

  cmFuncFilterFree(  &ffp );

  cmCtxFinal(&c);
  
  //unsigned v1n = 9;
  //cmSample_t v1[9] = { 1,   75,   91,   35,    6,   80,   40,   91,   79};

  //cmSample_t v1[10] = {53,   64,   48,   78,   30,   59,    7,   50,   71,  53 };

  //printf("Median: %f \n",cmVOS_Median(v1,v1+v1n));
}

//------------------------------------------------------------------------------------------------------------

cmDhmm* cmDhmmAlloc( cmCtx* c, cmDhmm* ap, unsigned stateN, unsigned symN, cmReal_t* initV, cmReal_t* transM,  cmReal_t* stsM )
{
  cmDhmm* p = cmObjAlloc( cmDhmm, c, ap );

  if( stateN > 0 && symN > 0 )
    if( cmDhmmInit(p, stateN, symN, initV, transM, stsM ) != cmOkRC )
      cmObjFree(&p);
  return p;
}

cmRC_t  cmDhmmFree(  cmDhmm** pp )
{
  cmRC_t rc = cmOkRC;
  cmDhmm* p = *pp;

  if( pp==NULL || *pp==NULL )
    return cmOkRC;

  if((rc = cmDhmmFinal(p)) != cmOkRC )
    return cmOkRC;

  cmObjFree(pp);
  return rc;
}

cmRC_t  cmDhmmInit(  cmDhmm* p, unsigned stateN, unsigned symN, cmReal_t* initV, cmReal_t* transM,  cmReal_t* stsM )
{
  cmRC_t rc;
  if((rc = cmDhmmFinal(p)) != cmOkRC )
    return rc;

  p->stateN = stateN;
  p->symN   = symN;
  p->initV  = initV;
  p->transM = transM;
  p->stsM   = stsM;
  return cmOkRC;
}

cmRC_t  cmDhmmFinal( cmDhmm* p )
{ return cmOkRC; }

cmRC_t  cmDhmmExec(  cmDhmm* p )
{
  return cmOkRC;
}


// Generate a random matrix with rows that sum to 1.0.
void _cmDhmmGenRandMatrix( cmReal_t* dp, unsigned rn, unsigned cn )
{
  cmReal_t v[ cn ];
  unsigned i,j;

  for(i=0; i<rn; ++i)
  {
    cmVOR_Random( v, cn, 0.0, 1.0 );
    cmVOR_NormalizeProbability( v, cn);

    for(j=0; j<cn; ++j)
      dp[ (j * rn) + i ] = v[j];
  }
}

enum { kEqualProbHmmFl=0x01, kRandProbHmmFl=0x02, kManualProbHmmFl=0x04 };
void _cmDhmmGenProb( cmReal_t* dp, unsigned rn, unsigned cn, unsigned flags, const cmReal_t* sp )
{
  
  switch( flags )
  {
    case kRandProbHmmFl:
      _cmDhmmGenRandMatrix( dp, rn, cn );
      break;

    case kEqualProbHmmFl:
      {
        // equal prob
        cmReal_t pr = 1.0/cn;
        unsigned i,j;
        for(i=0; i<rn; ++i)
          for(j=0; j<cn; ++j)
            dp[ (j*rn) + i ] = pr;
      }
      break;

    case kManualProbHmmFl:
      cmVOR_Copy( dp, (rn*cn), sp );
      break;

    default:
      assert(0);

  }


}

// generate a random integer in the range 0 to probN-1 where probV[ probN ] contains
// the probability of generating each of the possible values.
unsigned _cmDhmmGenRandInt( const cmReal_t* probV, unsigned probN, unsigned stride )
{
  cmReal_t        tmp[ probN ];
  cmReal_t        cumSumV[ probN+1 ];
  const cmReal_t* sp = probV;

  cumSumV[0] = 0;

  if( stride > 1 )
  {
    cmVOR_CopyStride( tmp, probN, probV, stride );
    sp = tmp;
  }  

  cmVOR_CumSum( cumSumV+1, probN, sp );

  return cmVOR_BinIndex( cumSumV, probN+1, (cmReal_t)rand()/RAND_MAX );  
}

cmRC_t     cmDhmmGenObsSequence( cmDhmm* p, unsigned* dbp, unsigned dn )
{
  const unsigned* dep = dbp + dn;

  // generate the first state based on the init state prob. vector
  unsigned state = _cmDhmmGenRandInt( p->initV, p->stateN, 1 );

  // generate an observation from the state based on the symbol prob. vector
  *dbp++ = _cmDhmmGenRandInt( p->stsM + state, p->symN, p->stateN );

  while( dbp < dep )
  {

    // get the next state based on the previous state
    state  = _cmDhmmGenRandInt( p->transM + state, p->stateN, p->stateN );

    // given a state generate an observation
    *dbp++ = _cmDhmmGenRandInt( p->stsM  + state, p->symN, p->stateN   );
  }

  return cmOkRC;
}

/// Perform a forward evaluation of the model given a set of observations.
/// initPrV[ stateN ] is the probability of the model being in each state at the start of the evaluation.
/// alphaM[ stateN, obsN ] is a return value and represents the probability of seeing each symbol at each time step.  
enum { kNoLogScaleHmmFl = 0x00, kLogScaleHmmFl = 0x01 };
cmRC_t  cmDHmmForwardEval( cmDhmm* p, const cmReal_t* initPrV, const unsigned* obsV, unsigned obsN, cmReal_t* alphaM, unsigned flags, cmReal_t* logProbPtr )
{
  bool            scaleFl = cmIsFlag(flags,kLogScaleHmmFl);
  cmReal_t        logProb = 0;
  cmReal_t*       abp     = alphaM;                          // define first dest. column 
  const cmReal_t* aep     = abp + p->stateN;
  const cmReal_t* sts     = p->stsM + (obsV[0] * p->stateN); // stsM[] column for assoc'd with first obs. symbol

  unsigned          i;

  // calc the prob of begining in each state given the obs. symbol  
  for(i=0; abp < aep; ++i )
    *abp++ = *initPrV++ * *sts++;

  // scale to prevent underflow
  if( scaleFl )
  {
    cmReal_t sum = cmVOR_Sum(abp-p->stateN,p->stateN);
    if( sum > 0 )
    {
      cmVOR_DivVS( abp-p->stateN,p->stateN,sum);
      logProb += log(sum);    
    }
  }


  // for each time step
  for(i=1; i<obsN; ++i)
  {
    // next state 0 (first col, first row) is calc'd first
    const cmReal_t* tm  = p->transM;

    // pick the stsM[] column assoc'd with ith observation symbol 
    const cmReal_t* sts = p->stsM + (obsV[i] * p->stateN); 
    
    // store a pointer to the alpha column assoc'd with obsV[i-1]
    const cmReal_t* app0 = abp - p->stateN;

    aep = abp + p->stateN;

    // for each dest state
    while( abp < aep )
    {

      // prob of being in each state on the previous time step
      const cmReal_t* app = app0;  
      const cmReal_t* ape = app + p->stateN;


      *abp = 0;

      // for each src state - calc prob. of trans from src to dest
      while( app<ape )
        *abp += *app++ * *tm++;

      // calc prob of obs symbol in dest state
      *abp++ *= *sts++;      
    }

    // scale to prevent underflow
    if( scaleFl )
    {
      cmReal_t sum = cmVOR_Sum(abp-p->stateN,p->stateN);
      if( sum > 0 )
      {
        cmVOR_DivVS( abp-p->stateN,p->stateN,sum);
        logProb += log(sum);    
      }
    }
 
  }

  if( logProbPtr != NULL )
    *logProbPtr = logProb;

  return cmOkRC;
}


cmRC_t  cmDHmmBcmkwardEval( cmDhmm* p, const unsigned* obsV, unsigned obsN, cmReal_t* betaM, unsigned flags )
{
  bool            scaleFl = cmIsFlag(flags,kLogScaleHmmFl);
  int             i,j,t;

  cmVOR_Fill(betaM+((obsN-1)*p->stateN),p->stateN,1.0);

  // for each time step
  for(t=obsN-2; t>=0; --t)
  {
    // for each state at t
    for(i=0; i<p->stateN; ++i)
    {      
      double Bt = 0;

      // for each state at t+1
      for(j=0; j<p->stateN; ++j)
      {
        double aij = p->transM[  (j         * p->stateN) + i ];
        double bj  = p->stsM[    (obsV[t+1] * p->stateN) + j ];
        double Bt1 = betaM[      ((t+1)     * p->stateN) + j ];

        Bt += aij * bj * Bt1;
      }
      
      betaM[ (t * p->stateN) + i ] = Bt;
      
    }

    if( scaleFl )
    {
      double* bp = betaM + (t * p->stateN);
      double sum = cmVOR_Sum(bp, p->stateN );
      if( sum > 0 )
        cmVOR_DivVS( bp, p->stateN, sum );
    }

  }

  return cmOkRC;

}

void _cmDhmmNormRow( cmReal_t* p, unsigned pn, unsigned stride, const cmReal_t* sp )
{
  if( sp == NULL )
    sp = p;

  cmReal_t        sum = 0;
  unsigned        n   = pn * stride;
  const cmReal_t* bp  = sp;
  const cmReal_t* ep  = bp + n;

  for(; bp<ep; bp+=stride)
    sum += *bp;

  for(ep = p+n; p<ep; p+=stride,sp+=stride)
    *p = *sp / sum;
}

void _cmDhmmNormMtxRows( cmReal_t* dp, unsigned rn, unsigned cn, const cmReal_t* sp )
{
  const cmReal_t* erp = sp + rn;

  while( sp < erp )
    _cmDhmmNormRow( dp++, cn, rn, sp++ );
}

cmRC_t  cmDhmmTrainEM( cmDhmm* p, const unsigned* obsV, unsigned obsN, unsigned iterCnt, unsigned flags )
{
  unsigned i,j,k,t; 

  cmReal_t alphaM[ p->stateN * obsN ];
  cmReal_t betaM[  p->stateN * obsN ];
  cmReal_t g[      p->stateN * obsN ];
  cmReal_t q[      p->stateN * p->symN ];
  cmReal_t E[      p->stateN * p->stateN ]; 

  cmReal_t logProb = 0;

  //cmDhmmReport(p->obj.ctx,p);

  for(k=0; k<iterCnt; ++k)
  {
    cmVOR_Fill( q, (p->stateN * p->symN),   0 );
    cmVOR_Fill( E, (p->stateN * p->stateN), 0 );

    // calculate alpha and beta
    cmDHmmForwardEval(  p, p->initV, obsV, obsN, alphaM, flags, &logProb);
    cmDHmmBcmkwardEval( p,           obsV, obsN, betaM,  flags );

    // gamma[ stateN, obsN ] = alphaM .* betaM - gamma is the probability of being in each state at each time step
    cmVOR_MultVVV( g,(p->stateN*obsN), alphaM, betaM );

    // normalize gamma
    for(i=0; i<obsN; ++i)
      cmVOR_NormalizeProbability( g + (i*p->stateN), p->stateN );

    //printf("ITER:%i logProb:%f\n",k,logProb);

    // count the number of times state i emits obsV[0] in the starting location
    cmVOR_Copy( q + (obsV[0] * p->stateN), p->stateN, g );

    for(t=0; t<obsN-1; ++t)
    {
      // point to alpha[:,t] and beta[:,t+1]
      const cmReal_t* alpha_t0 = alphaM + (t*p->stateN);
      const cmReal_t* beta_t1  = betaM  + ((t+1)*p->stateN);

      cmReal_t        Et[ p->stateN * p->stateN ];
      cmReal_t        Esum  = 0;

      // for each source state
      for(i=0; i<p->stateN; ++i)
      {
        // for each dest state
        for(j=0; j<p->stateN; ++j)
        {
          // prob of transitioning from state i to j and emitting obs[t] at time t
          cmReal_t Eps  = alpha_t0[i] * p->transM[ (j*p->stateN) + i ] * p->stsM[ (obsV[t+1]*p->stateN) + j ] * beta_t1[j];
         
          // count the number of transitions from i to j
          Et[ (j*p->stateN) + i ] = Eps;
          Esum                   += Eps;
        }

        // count the number of times state i emits obsV[t]
        q[ (obsV[t+1] * p->stateN) + i ] += g[ ((t+1)*p->stateN) + i ];
      }

      // normalize Et and sum it into E
      cmVOR_DivVS( Et, (p->stateN*p->stateN), Esum );
      cmVOR_AddVV( E,  (p->stateN*p->stateN), Et );
   
    }

    // update the model
    _cmDhmmNormMtxRows( p->initV,  1,         p->stateN, g );
    _cmDhmmNormMtxRows( p->transM, p->stateN, p->stateN, E );
    _cmDhmmNormMtxRows( p->stsM,   p->stateN, p->symN,   q );
    
  }

  return cmOkRC;  
}

cmRC_t  cmDhmmReport(  cmDhmm* p )
{

  cmVOR_PrintL("initV:\n", p->obj.err.rpt,  1,         p->stateN, p->initV );

  cmVOR_PrintL("transM:\n", p->obj.err.rpt, p->stateN, p->stateN, p->transM );

  cmVOR_PrintL("symM:\n", p->obj.err.rpt,   p->stateN, p->symN,   p->stsM );


  return cmOkRC;
}

void  cmDhmmTest( cmRpt_t* rpt, cmLHeapH_t lhH, cmSymTblH_t stH )
{
  unsigned stateN = 2;
  unsigned symN   = 3;
  unsigned obsN   = 4;
  unsigned iterN  = 10;
 
  cmReal_t initV0[  stateN ];
  cmReal_t transM0[ stateN * stateN ];
  cmReal_t stsM0[   symN   * stateN ];

  cmReal_t initV1[  stateN ];
  cmReal_t transM1[ stateN * stateN ];
  cmReal_t stsM1[   symN   * stateN ];

  unsigned obsV[    obsN ]; 
  unsigned hist[    symN ];

  unsigned genFl = kManualProbHmmFl;

  cmReal_t initV[] = 
    {
      0.44094,
      0.55906
    };

  
  cmReal_t transM[] = 
    {
      0.48336,
      0.81353,
      0.51664,
      0.18647,
    };

  cmReal_t stsM[] = 
    {
      0.20784,
      0.18698,
      0.43437,
      0.24102,
      0.35779,
      0.57199
    };

  unsigned obsM[] = { 2, 2, 2, 0 };

  cmReal_t initV2[]  = { 0.79060, 0.20940 };
  cmReal_t transM2[] = { 0.508841, 0.011438, 0.491159, 0.988562 };
  cmReal_t stsM2[]   = { 0.25789, 0.35825, 0.61981, 0.42207, 0.12230, 0.21969 };


  //srand( time(NULL) );

  // generate a random HMM
  _cmDhmmGenProb( initV0,  1,      stateN, genFl, initV );
  _cmDhmmGenProb( transM0, stateN, stateN, genFl, transM );
  _cmDhmmGenProb( stsM0,   stateN, symN,   genFl, stsM );

  cmCtx*  c   = cmCtxAlloc( NULL, rpt, lhH, stH);
  cmDhmm* h0p = cmDhmmAlloc( c, NULL, stateN, symN, initV0, transM0, stsM0 );

  // generate an observation sequence based on the random HMM
  //cmDhmmGenObsSequence(c, h0p, obsV, obsN );
  memcpy(obsV,obsM,obsN*sizeof(unsigned));

  if( 0 )
  {
    // print the HMM 
    cmDhmmReport( h0p);

    // print the observation symbols
    cmVOU_PrintL("obs:\n", rpt, 1, obsN, obsV );

    // print the histogram of the obs. symbols
    cmVOU_Hist( hist, symN, obsV, obsN );
    cmVOU_PrintL("hist:\n", rpt, 1, symN, hist );

    // calc alpha (the forward probabilities)
    cmReal_t alphaM[ h0p->stateN*obsN ];
    cmReal_t logProb=0;
    cmDHmmForwardEval( h0p, h0p->initV, obsV, obsN, alphaM, kLogScaleHmmFl, &logProb);
    printf("log prob:%f\n alpha:\n", logProb );
    cmVOR_Print( rpt, h0p->stateN, obsN, alphaM );


    // calc beta (the bcmkward probabilities)
    cmReal_t betaM[ h0p->stateN*obsN ];
    logProb=0;
    cmDHmmBcmkwardEval(  h0p, obsV, obsN, betaM, kLogScaleHmmFl);
    printf("log prob:%f\n beta:\n", logProb );
    cmVOR_Print( h0p->obj.err.rpt, h0p->stateN, obsN, betaM );
  }

  // initialize a second  HMM with random probabilities
  _cmDhmmGenProb( initV1,  1,      stateN, kManualProbHmmFl, initV2 );
  _cmDhmmGenProb( transM1, stateN, stateN, kManualProbHmmFl, transM2 );
  _cmDhmmGenProb( stsM1,   stateN, symN,   kManualProbHmmFl, stsM2 );
  
  cmDhmm* h1p = cmDhmmAlloc( c, NULL, stateN, symN, initV1, transM1, stsM1 );

  cmDhmmTrainEM( h1p, obsV, obsN, iterN, kLogScaleHmmFl ); 

  cmDhmmFree(&h1p);
  cmDhmmFree(&h0p);
  cmCtxFree(&c); 
}

//------------------------------------------------------------------------------------------------------------
cmConvolve* cmConvolveAlloc( cmCtx* c, cmConvolve* ap, const cmSample_t* h, unsigned hn, unsigned procSmpCnt )
{
  cmConvolve* p = cmObjAlloc( cmConvolve, c, ap);
  
  p->fft = cmFftAllocSR( c,NULL,NULL,0,kNoConvertFftFl);
  p->ifft= cmIFftAllocRS(c,NULL,p->fft->binCnt);

  if( hn > 0 && procSmpCnt > 0 )
    if( cmConvolveInit(p,h,hn,procSmpCnt) != cmOkRC )
      cmObjFree(&p);

  return p;
}

cmRC_t      cmConvolveFree(  cmConvolve** pp )
{
  cmRC_t rc;
  cmConvolve* p = *pp;

  if( pp == NULL || *pp == NULL )
    return cmOkRC;

  if((rc = cmConvolveFinal(p)) != cmOkRC )
    return cmOkRC;

  cmFftFreeSR(&p->fft);
  cmIFftFreeRS(&p->ifft);
  cmMemPtrFree(&p->H);
  cmMemPtrFree(&p->outV);
  cmObjFree(pp);

  return cmOkRC;
}

cmRC_t      cmConvolveInit(  cmConvolve* p, const cmSample_t* h, unsigned hn, unsigned procSmpCnt )
{
  cmRC_t rc;
  unsigned i;
  unsigned cn = cmNextPowerOfTwo( hn + procSmpCnt - 1 );

  if((rc = cmConvolveFinal(p)) != cmOkRC )
    return rc;
  
  cmFftInitSR(  p->fft, NULL, cn, kNoConvertFftFl );
  cmIFftInitRS( p->ifft, p->fft->binCnt);

  p->H    = cmMemResizeZ( cmComplexR_t,p->H,  p->fft->binCnt );
  p->outV = cmMemResizeZ( cmSample_t,p->outV, cn );
  p->olaV = p->outV + procSmpCnt;
  p->outN = procSmpCnt;
  p->hn   = hn;

  // take the FFT of the impulse response
  cmFftExecSR(  p->fft, h, hn );

  // copy the FFT of the impulse response to p->H[]
  for(i=0; i<p->fft->binCnt; ++i)
     p->H[i] = p->fft->complexV[i] / p->fft->wndSmpCnt;

  return cmOkRC;
}

cmRC_t      cmConvolveFinal(  cmConvolve* p )
{  return cmOkRC; }

cmRC_t      cmConvolveExec(  cmConvolve* p, const cmSample_t* x, unsigned xn )
{
  unsigned i;

  // take FT of input signal
  cmFftExecSR(  p->fft, x, xn );

  // multiply the signal spectra of the input signal and impulse response
  for(i=0; i<p->fft->binCnt; ++i)
    p->ifft->complexV[i] = p->H[i] * p->fft->complexV[i];

  // take the IFFT of the convolved spectrum
  cmIFftExecRS(p->ifft,NULL);

  // sum with previous impulse response tail
  cmVOS_AddVVV( p->outV, p->outN-1, p->olaV, p->ifft->outV );

  // first sample of the impulse response tail is complete 
  p->outV[p->outN-1] = p->ifft->outV[p->outN-1];

  // store the new impulse response tail
  cmVOS_Copy(p->olaV,p->hn-1,p->ifft->outV + p->outN );

  return cmOkRC;
}

cmRC_t      cmConvolveSignal( cmCtx* c, const cmSample_t* h, unsigned hn, const cmSample_t* x, unsigned xn, cmSample_t* y, unsigned yn )
{
  cmConvolve* p = cmConvolveAlloc(c,NULL,h,hn,xn);

  cmConvolveExec(p,x,xn);

  unsigned n = cmMin(p->outN,yn);
  
  cmVOS_Copy(y,n,p->outV);

  if( yn > p->outN )
  {
    unsigned m = cmMin(yn-p->outN,p->hn-1);
    cmVOS_Copy(y+n,m,p->olaV);
  }

  cmConvolveFree(&p);
  return cmOkRC;
}

cmRC_t      cmConvolveTest(cmRpt_t* rpt, cmLHeapH_t lhH, cmSymTblH_t stH )
{
  cmCtx *c = cmCtxAlloc(NULL,rpt,lhH,stH);

  cmSample_t h[] = { 1, .5, .25, 0, 0 };
  unsigned   hn  = sizeof(h) / sizeof(h[0]);
  cmSample_t x[] = { 1, 0, 0, 0, 1, 0, 0, 0 };
  unsigned   xn  = sizeof(x) / sizeof(x[0]);
  unsigned   yn  = xn+hn-1;
  cmSample_t y[yn];

  //cmVOS_Hann(h,5);

  cmConvolve* p = cmConvolveAlloc(c,NULL,h,hn,xn);
 
  cmConvolveExec(p,x,xn);

  cmVOS_Print( rpt, 1, p->outN, p->outV );
  cmVOS_Print( rpt, 1, p->hn-1, p->olaV );
 
  cmConvolveFree(&p);

  cmConvolveSignal(c,h,hn,x,xn,y,yn);
  cmVOS_Print( rpt, 1, hn+xn-1, y );

  cmCtxFree(&c);

  return cmOkRC;
  
}

//------------------------------------------------------------------------------------------------------------
cmBfcc* cmBfccAlloc( cmCtx* ctx, cmBfcc* ap, unsigned bandCnt, unsigned binCnt, double binHz )
{
  cmBfcc* p = cmObjAlloc( cmBfcc, ctx, ap );

  if( bandCnt > 0 )
    if( cmBfccInit( p, bandCnt, binCnt, binHz ) != cmOkRC )
      cmBfccFree(&p);

  return p;
}

cmRC_t  cmBfccFree(  cmBfcc** pp )
{
  cmRC_t rc;

  if( pp== NULL || *pp==NULL)
    return cmOkRC;

  cmBfcc* p = *pp;
  
  if((rc = cmBfccFinal(p)) != cmOkRC )
    return rc;

  cmMemPtrFree(&p->dctMtx);
  cmMemPtrFree(&p->filtMask);
  cmMemPtrFree(&p->outV);
  cmObjFree(pp);
  return rc;
}

cmRC_t  cmBfccInit(  cmBfcc* p, unsigned bandCnt, unsigned binCnt, double binHz )
{
  cmRC_t rc;

  if((rc = cmBfccFinal(p)) != cmOkRC )
    return rc;

  p->dctMtx   = cmMemResizeZ( cmReal_t, p->dctMtx,   bandCnt*bandCnt);
  p->filtMask = cmMemResizeZ( cmReal_t, p->filtMask, bandCnt*binCnt);
  p->outV     = cmMemResizeZ( cmReal_t, p->outV, bandCnt );
  p->binCnt   = binCnt;
  p->bandCnt  = bandCnt;

  cmVOR_BarkMask( p->filtMask, bandCnt, binCnt, binHz );
  cmVOR_DctMatrix(p->dctMtx,   bandCnt,  bandCnt );

  return rc;
}

cmRC_t  cmBfccFinal( cmBfcc* p )
{ return cmOkRC; }

cmRC_t  cmBfccExec(  cmBfcc* p, const cmReal_t* magV, unsigned binCnt )
{
  assert( binCnt <= p->binCnt );
  cmReal_t t[ p->bandCnt ];
  cmReal_t v[ binCnt ];

  // convert magnitude to power
  cmVOR_PowVVS(v,binCnt,magV,2.0);  

  // apply the filter mask to the power spectrum
  cmVOR_MultVMV( t, p->bandCnt, p->filtMask, binCnt, v );


  //cmVOR_PrintL("\t:\n", p->obj.ctx->outFuncPtr, 1,       p->bandCnt, t);

  cmVOR_ReplaceLte( t, p->bandCnt, t, 0, 0.1e-5 );
  cmVOR_LogV( t, p->bandCnt, t );
 
  // decorellate the bands with a DCT
  cmVOR_MultVMV( p->outV, p->bandCnt, p->dctMtx, p->bandCnt, t );

  return cmOkRC;
}

void cmBfccTest(  cmRpt_t* rpt, cmLHeapH_t lhH, cmSymTblH_t stH )
{
  double   srate   = 11025;
  unsigned binCnt  = 129;
  double   binHz   = srate/(binCnt-1);
  unsigned bandCnt = kDefaultBarkBandCnt;
  
  cmCtx*  c = cmCtxAlloc(     NULL, rpt, lhH, stH );
  cmBfcc* b = cmBfccAlloc( c, NULL, bandCnt, binCnt, binHz );

  cmReal_t powV[] = {
    0.8402, 0.3944, 0.7831, 0.7984, 0.9116, 0.1976, 0.3352, 0.7682, 0.2778, 0.5540,
    0.4774, 0.6289, 0.3648, 0.5134, 0.9522, 0.9162, 0.6357, 0.7173, 0.1416, 0.6070, 
    0.0163, 0.2429, 0.1372, 0.8042, 0.1567, 0.4009, 0.1298, 0.1088, 0.9989, 0.2183, 
    0.5129, 0.8391, 0.6126, 0.2960, 0.6376, 0.5243, 0.4936, 0.9728, 0.2925, 0.7714, 
    0.5267, 0.7699, 0.4002, 0.8915, 0.2833, 0.3525, 0.8077, 0.9190, 0.0698, 0.9493, 
    0.5260, 0.0861, 0.1922, 0.6632, 0.8902, 0.3489, 0.0642, 0.0200, 0.4577, 0.0631, 
    0.2383, 0.9706, 0.9022, 0.8509, 0.2667, 0.5398, 0.3752, 0.7602, 0.5125, 0.6677, 
    0.5316, 0.0393, 0.4376, 0.9318, 0.9308, 0.7210, 0.2843, 0.7385, 0.6400, 0.3540, 
    0.6879, 0.1660, 0.4401, 0.8801, 0.8292, 0.3303, 0.2290, 0.8934, 0.3504, 0.6867, 
    0.9565, 0.5886, 0.6573, 0.8587, 0.4396, 0.9240, 0.3984, 0.8148, 0.6842, 0.9110, 
    0.4825, 0.2158, 0.9503, 0.9201, 0.1477, 0.8811, 0.6411, 0.4320, 0.6196, 0.2811, 
    0.7860, 0.3075, 0.4470, 0.2261, 0.1875, 0.2762, 0.5564, 0.4165, 0.1696, 0.9068, 
    0.1032, 0.1261, 0.4954, 0.7605, 0.9848, 0.9350, 0.6844, 0.3832, 0.7498 };


  //cmVOR_Random(powV, binCnt, 0.0, 1.0 );
  cmBfccExec(b,powV,binCnt);  

  //cmVOR_PrintL("\nin:\n",   rpt, 1,       binCnt, powV);
  //cmVOR_PrintL("\nfilt:\n", rpt, bandCnt, binCnt, b->filtMask);
  //cmVOR_PrintL("\ndct:\n",  rpt, bandCnt, bandCnt,b->dctMtx);
  cmVOR_PrintL("\nbfcc:\n", rpt, 1,       bandCnt, b->outV);
  cmBfccFree(&b);
  cmCtxFree(&c);
} 

//------------------------------------------------------------------------------------------------------------
cmCeps* cmCepsAlloc( cmCtx* ctx, cmCeps* ap, unsigned binCnt, unsigned outN )
{
  cmCeps* p = cmObjAlloc( cmCeps, ctx, ap );

  //cmIFftAllocRR(  ctx, &p->ft, 0 );

  if( binCnt > 0 )
    if( cmCepsInit( p, binCnt, outN ) != cmOkRC )
      cmCepsFree(&p);

  return p;
}

cmRC_t  cmCepsFree(  cmCeps** pp )
{
  cmRC_t rc;

  if( pp== NULL || *pp==NULL)
    return cmOkRC;

  cmCeps* p = *pp;
  
  if((rc = cmCepsFinal(p)) != cmOkRC )
    return rc;

  //cmObjFreeStatic( cmIFftFreeRR,   cmIFftRR,   p->ft );

  cmMemPtrFree(&p->dctM);
  cmMemPtrFree(&p->outV);

  cmObjFree(pp);
  return rc;
}

cmRC_t  cmCepsInit(  cmCeps* p, unsigned binCnt, unsigned outN )
{
  cmRC_t rc;

  if((rc = cmCepsFinal(p)) != cmOkRC )
    return rc;

  //cmIFftInitRR( &p->ft, binCnt );
  p->dct_cn    = (binCnt-1)*2;
  p->dctM      = cmMemResize( cmReal_t, p->dctM, outN*p->dct_cn );
  p->outN      = outN; //p->ft.outN;
  p->outV      = cmMemResizeZ( cmReal_t, p->outV, outN ); //p->ft.outV;
  p->binCnt    = binCnt;

  assert( outN <= p->dct_cn );

  cmVOR_DctMatrix( p->dctM, outN, p->dct_cn );
  
  return rc;
}

cmRC_t  cmCepsFinal( cmCeps* p )
{ return cmOkRC; }

cmRC_t  cmCepsExec(  cmCeps* p, const cmReal_t* magV, const cmReal_t* phsV, unsigned binCnt )
{

  assert( binCnt == p->binCnt );

  cmReal_t v[ p->dct_cn ];

  // guard against zeros in the magn spectrum 
  cmVOR_ReplaceLte(v,binCnt,magV,0.0,0.00001);

  // take the log of the spectrum
  cmVOR_LogV(v,binCnt,v);

  // reconstruct the negative frequencies
  int i,j;
  for(i=1,j=p->dct_cn-1; i<binCnt; ++i,--j)
    v[j] = v[i];

  // take the DCT
  cmVOR_MultVMV( p->outV, p->outN, p->dctM, p->dct_cn, v );

  //cmIFftExecPolarRR( &p->ft, v, phsV );

  return cmOkRC;
}



//------------------------------------------------------------------------------------------------------------
cmOla* cmOlaAlloc( cmCtx* ctx, cmOla* ap, unsigned wndSmpCnt, unsigned hopSmpCnt, unsigned procSmpCnt, unsigned wndTypeId )
{
  cmOla* p = cmObjAlloc( cmOla, ctx, ap );

  cmWndFuncAlloc( ctx, &p->wf, kHannWndId, wndSmpCnt, 0);

  if( wndSmpCnt > 0 )
    if( cmOlaInit(p,wndSmpCnt,hopSmpCnt,procSmpCnt,wndTypeId) != cmOkRC )
      cmOlaFree(&p);

  return p;
}

cmRC_t cmOlaFree(  cmOla** pp )
{
  cmRC_t rc;

  if( pp==NULL || *pp==NULL )
    return cmOkRC;

  cmOla* p = *pp;

  if(( rc = cmOlaFinal(p)) != cmOkRC )
    return rc;

  cmMemPtrFree(&p->bufV);
  cmMemPtrFree(&p->outV);
  cmObjFreeStatic( cmWndFuncFree,  cmWndFunc,  p->wf );
  cmObjFree(pp);
  return rc;
}

cmRC_t cmOlaInit(  cmOla* p, unsigned wndSmpCnt, unsigned hopSmpCnt, unsigned procSmpCnt, unsigned wndTypeId )
{
  cmRC_t rc;
  if((rc = cmOlaFinal(p)) != cmOkRC )
    return rc;

  if((rc = cmWndFuncInit( &p->wf, wndTypeId, wndSmpCnt, 0)) != cmOkRC ) 
    return rc;

  p->bufV = cmMemResizeZ( cmSample_t, p->bufV, wndSmpCnt );
  p->outV = cmMemResizeZ( cmSample_t, p->outV, hopSmpCnt );
  p->outPtr = p->outV + hopSmpCnt;

  // hopSmpCnt must be an even multiple of procSmpCnt
  assert( hopSmpCnt % procSmpCnt == 0 );

  assert( wndSmpCnt >= hopSmpCnt );

  p->wndSmpCnt = wndSmpCnt;
  p->hopSmpCnt = hopSmpCnt;
  p->procSmpCnt= procSmpCnt;
  p->idx       = 0;

  return rc; 
}

cmRC_t cmOlaFinal( cmOla* p )
{ return cmOkRC; }

// The incoming buffer and the ola buf (bufV)
// can be divided into three logical parts:
//
// [head][middle][tail]
// 
// head = hopSmpCnt values
// tail = hopSmpCnt values
// middle = wndSmpCnt - (2*hopSmpCnt) values
//
// Each exec can be broken into three phases:
//
//  outV          = bufV[tail] + in[head]
//  bufV[middle] += in[middle]
//  bufV[tail]    = in[tail]
//


cmRC_t cmOlaExecS( cmOla* p, const cmSample_t* sp, unsigned sN )
{
  assert( sN == p->wndSmpCnt );
  cmRC_t            rc = cmOkRC;
  const cmSample_t* ep = sp + sN;
  const cmSample_t* wp = p->wf.wndV;
  int         i,j,k,n;

  // [Sum head of incoming samples with tail of ola buf]
  // fill outV with the bufV[idx:idx+hopSmpCnt] + sp[hopSmpCnt]
  for(i=0; i<p->hopSmpCnt; ++i)
  {
    p->outV[i] = p->bufV[p->idx++] + (*sp++ * *wp++);

    if( p->idx == p->wndSmpCnt )
      p->idx = 0;
  }
  

  // [Sum middle of incoming samples with middle of ola buf]
  // sum next wndSmpCnt - hopSmpCnt samples of sp[] into bufV[]
  n = p->wndSmpCnt - (2*p->hopSmpCnt);
  k = p->idx;

  for(j=0; j<n; ++j)
  {
    p->bufV[k++] += (*sp++ * *wp++);
    if( k == p->wndSmpCnt )
      k = 0;
  } 

  // [Assign tail of incoming to tail of ola buf]
  // assign ending samples from sp[] into bufV[]
  while( sp < ep )
  {
    p->bufV[k++] = (*sp++ * *wp++);

    if( k == p->wndSmpCnt )
      k = 0;
  }

  p->outPtr = p->outV;
        
  return rc;
}

cmRC_t cmOlaExecR( cmOla* p, const cmReal_t*   sp, unsigned sN )
{
  assert( sN == p->wndSmpCnt );
  cmRC_t            rc = cmOkRC;
  const cmReal_t*   ep = sp + sN;
  const cmSample_t* wp = p->wf.wndV;
  int         i,j,k,n;

  // fill outV with the bufV[idx:idx+hopSmpCnt] + sp[hopSmpCnt]
  for(i=0; i<p->hopSmpCnt; ++i)
  {
    p->outV[i] = p->bufV[p->idx++] + (*sp++ * *wp++);

    if( p->idx == p->wndSmpCnt )
      p->idx = 0;
  }
  
  // sum next wndSmpCnt - hopSmpCnt samples of sp[] into bufV[]
  n = p->wndSmpCnt - (2*p->hopSmpCnt);
  k = p->idx;

  for(j=0; j<n; ++j)
  {
    p->bufV[k++] += (*sp++ * *wp++);
    if( k == p->wndSmpCnt )
      k = 0;
  } 

  // assign ending samples from sp[] into bufV[]
  while( sp < ep )
  {
    p->bufV[k++] = (*sp++ * *wp++);

    if( k == p->wndSmpCnt )
      k = 0;
  }

  p->outPtr = p->outV;
        
  return rc;

}

const cmSample_t* cmOlaExecOut(cmOla* p)
{
  const cmSample_t* sp = p->outPtr;
  if( sp >= p->outV + p->hopSmpCnt )
    return NULL;

  p->outPtr += p->procSmpCnt;

  return sp;
}


//------------------------------------------------------------------------------------------------------------
cmPhsToFrq* cmPhsToFrqAlloc( cmCtx* c, cmPhsToFrq* ap, double srate, unsigned binCnt, unsigned hopSmpCnt )
{
  cmPhsToFrq* p = cmObjAlloc( cmPhsToFrq, c, ap );
  if( srate != 0 )
    if( cmPhsToFrqInit( p, srate, binCnt, hopSmpCnt ) != cmOkRC )
      cmPhsToFrqFree(&p);
  return p;
}

cmRC_t  cmPhsToFrqFree( cmPhsToFrq** pp )
{
  cmRC_t  rc = cmOkRC;
  cmPhsToFrq* p  = *pp;

  if( pp==NULL || *pp== NULL )
    return rc;

  if((rc = cmPhsToFrqFinal(p)) != cmOkRC )
    return rc;

  cmMemPtrFree(&p->hzV);
  cmMemPtrFree(&p->phsV);
  cmMemPtrFree(&p->wV);
  cmObjFree(pp);

  return rc;
}

cmRC_t  cmPhsToFrqInit( cmPhsToFrq* p, double srate, unsigned binCnt, unsigned hopSmpCnt )
{
  cmRC_t rc;
  unsigned i;

  if((rc = cmPhsToFrqFinal(p)) != cmOkRC )
    return rc;

  p->hzV       = cmMemResizeZ( cmReal_t, p->hzV,  binCnt );
  p->phsV      = cmMemResizeZ( cmReal_t, p->phsV, binCnt );  
  p->wV        = cmMemResizeZ( cmReal_t, p->wV,   binCnt );
  p->srate     = srate;
  p->binCnt    = binCnt;
  p->hopSmpCnt = hopSmpCnt;

  for(i=0; i<binCnt; ++i)
    p->wV[i] = M_PI * i * hopSmpCnt / (binCnt-1);

  return rc;
}

cmRC_t  cmPhsToFrqFinal(cmPhsToFrq* p )
{ return cmOkRC; }

cmRC_t  cmPhsToFrqExec( cmPhsToFrq* p, const cmReal_t* phsV )
{
  cmRC_t   rc    = cmOkRC;
  unsigned i;
  double   twoPi = 2.0 * M_PI;
  double   den   = twoPi * p->hopSmpCnt;

  for(i=0; i<p->binCnt; ++i)
  {
    cmReal_t dPhs = phsV[i] - p->phsV[i];

    // unwrap phase - see phase_study.m for explanation
    cmReal_t k = round( (p->wV[i] - dPhs) / twoPi);

    // convert phase change to Hz
    p->hzV[i] = (k * twoPi + dPhs) * p->srate /  den;
  
    // store phase for next iteration
    p->phsV[i] = phsV[i];

  }
  
  return rc;  
}

//------------------------------------------------------------------------------------------------------------
cmPvAnl* cmPvAnlAlloc( cmCtx* ctx, cmPvAnl* ap, unsigned procSmpCnt, double srate, unsigned wndSmpCnt, unsigned hopSmpCnt, unsigned flags )
{
  cmRC_t rc;
  cmPvAnl* p = cmObjAlloc( cmPvAnl, ctx, ap );

  cmShiftBufAlloc(ctx, &p->sb, procSmpCnt, wndSmpCnt, hopSmpCnt );
  cmWndFuncAlloc( ctx, &p->wf, kHannWndId, wndSmpCnt, 0);
  cmFftAllocSR(   ctx, &p->ft, p->wf.outV, wndSmpCnt, kToPolarFftFl);
  cmPhsToFrqAlloc(ctx, &p->pf, srate, p->ft.binCnt, hopSmpCnt  );

  if( procSmpCnt > 0 )
    if((rc = cmPvAnlInit(p,procSmpCnt,srate,wndSmpCnt,hopSmpCnt,flags)) != cmOkRC )
      cmPvAnlFree(&p);

  return p;
}

cmRC_t     cmPvAnlFree( cmPvAnl** pp )
{ 
  cmRC_t rc;

  if( pp==NULL || *pp==NULL )
    return cmOkRC;

  cmPvAnl* p = *pp;

  if((rc = cmPvAnlFinal(p) ) != cmOkRC )
    return rc;
  
  cmObjFreeStatic( cmPhsToFrqFree, cmPhsToFrq, p->pf );
  cmObjFreeStatic( cmFftFreeSR,    cmFftSR,    p->ft );
  cmObjFreeStatic( cmWndFuncFree,  cmWndFunc,  p->wf );
  cmObjFreeStatic( cmShiftBufFree, cmShiftBuf, p->sb );
  cmObjFree(pp);

  return rc;  
}

cmRC_t     cmPvAnlInit( cmPvAnl* p, unsigned procSmpCnt, double srate, unsigned wndSmpCnt, unsigned hopSmpCnt, unsigned flags )
{
  cmRC_t rc;
  if((rc = cmPvAnlFinal(p)) != cmOkRC )
    return rc;

  if((rc = cmShiftBufInit( &p->sb, procSmpCnt, wndSmpCnt, hopSmpCnt )) != cmOkRC )
    return rc;

  if((rc = cmWndFuncInit( &p->wf, kHannWndId | kNormByLengthWndFl, wndSmpCnt, 0)) != cmOkRC )
    return rc;

  if((rc = cmFftInitSR(   &p->ft, p->wf.outV, wndSmpCnt, kToPolarFftFl)) != cmOkRC )
    return rc;

  if((rc = cmPhsToFrqInit( &p->pf, srate, p->ft.binCnt, hopSmpCnt)) != cmOkRC )
    return rc;

  // if the window was just initialized
  // divide the window to indirectly apply the magnitude  normalization
  //if( p->wndSmpCnt != wndSmpCnt )
  //  cmVOS_DivVS( p->wf.wndV, p->wf.outN, wndSmpCnt );

  p->flags      = flags;
  p->procSmpCnt = procSmpCnt;
  p->wndSmpCnt  = wndSmpCnt;
  p->hopSmpCnt  = hopSmpCnt;
  p->binCnt     = p->ft.binCnt;

  p->magV       = p->ft.magV;
  p->phsV       = p->ft.phsV;
  p->hzV        = p->pf.hzV;

  return rc;
}

cmRC_t     cmPvAnlFinal(cmPvAnl* p )
{ return cmOkRC; }

bool     cmPvAnlExec( cmPvAnl* p, const cmSample_t* x, unsigned xN )
{
  bool fl = false;
  while( cmShiftBufExec(&p->sb,x,xN) )
  {
    cmWndFuncExec(&p->wf, p->sb.outV, p->sb.wndSmpCnt );

    cmFftExecSR(&p->ft,NULL,0);

    if( cmIsFlag(p->flags,kCalcHzPvaFl) )
      cmPhsToFrqExec(&p->pf,p->phsV);

    fl = true;
  }

  return fl;
}

//------------------------------------------------------------------------------------------------------------
cmPvSyn*   cmPvSynAlloc( cmCtx* ctx, cmPvSyn* ap, unsigned procSmpCnt, double outSrate, unsigned wndSmpCnt, unsigned hopSmpCnt, unsigned wndTypeId )
{
  cmRC_t rc;
  cmPvSyn* p = cmObjAlloc( cmPvSyn, ctx, ap );

  cmWndFuncAlloc( ctx, &p->wf, kHannWndId, wndSmpCnt, 0);
  cmIFftAllocRS(  ctx, &p->ft, wndSmpCnt/2+1 );
  cmOlaAlloc(     ctx, &p->ola, wndSmpCnt, hopSmpCnt, procSmpCnt, wndTypeId );

  if( procSmpCnt )
    if((rc = cmPvSynInit(p,procSmpCnt,outSrate,wndSmpCnt,hopSmpCnt,wndTypeId)) != cmOkRC )
      cmPvSynFree(&p);
  return p;  
}

cmRC_t     cmPvSynFree( cmPvSyn** pp )
{
  cmRC_t rc;

  if( pp==NULL || *pp==NULL )
    return cmOkRC;

  cmPvSyn* p = *pp;

  if((rc = cmPvSynFinal(p)) != cmOkRC )
    return rc;

  cmMemPtrFree(&p->minRphV);
  cmMemPtrFree(&p->maxRphV);
  cmMemPtrFree(&p->itrV);
  cmMemPtrFree(&p->phs0V);
  cmMemPtrFree(&p->phsV);
  cmMemPtrFree(&p->mag0V);
  cmMemPtrFree(&p->magV);

  cmObjFreeStatic( cmOlaFree,      cmOla,      p->ola);
  cmObjFreeStatic( cmIFftFreeRS,   cmIFftRS,   p->ft );
  cmObjFreeStatic( cmWndFuncFree,  cmWndFunc,  p->wf );

  cmObjFree(pp);

  return cmOkRC;
}

cmRC_t     cmPvSynInit( cmPvSyn* p, unsigned procSmpCnt, double outSrate, unsigned wndSmpCnt, unsigned hopSmpCnt, unsigned wndTypeId )
{
  cmRC_t   rc;
  int      k;
  double   twoPi     = 2.0 * M_PI;
  bool     useHannFl = true;
  int      m         = useHannFl ? 2 : 1;

  if((rc = cmPvSynFinal(p)) != cmOkRC )
    return rc;

  p->outSrate   = outSrate;
  p->procSmpCnt = procSmpCnt;
  p->wndSmpCnt  = wndSmpCnt;
  p->hopSmpCnt  = hopSmpCnt;
  p->binCnt     = wndSmpCnt / 2 + 1;

  p->minRphV    = cmMemResizeZ( cmReal_t, p->minRphV, p->binCnt );
  p->maxRphV    = cmMemResizeZ( cmReal_t, p->maxRphV, p->binCnt );
  p->itrV       = cmMemResizeZ( cmReal_t, p->itrV,    p->binCnt );
  p->phs0V      = cmMemResizeZ( cmReal_t, p->phs0V,   p->binCnt );
  p->phsV       = cmMemResizeZ( cmReal_t, p->phsV,    p->binCnt );
  p->mag0V      = cmMemResizeZ( cmReal_t, p->mag0V,   p->binCnt );
  p->magV       = cmMemResizeZ( cmReal_t, p->magV,    p->binCnt );


  if((rc = cmWndFuncInit( &p->wf, wndTypeId, wndSmpCnt, 0)) != cmOkRC )
    return rc;

  if((rc = cmIFftInitRS( &p->ft, p->binCnt )) != cmOkRC )
    return rc;

  if((rc = cmOlaInit(  &p->ola, wndSmpCnt, hopSmpCnt, procSmpCnt, wndTypeId )) != cmOkRC )
    return rc;

  for(k=0; k<p->binCnt; ++k)
  {
    // complete revolutions per hop in radians
    p->itrV[k] = twoPi * floor((double)k * hopSmpCnt / wndSmpCnt ); 

    p->minRphV[k] = ((cmReal_t)(k-m)) * hopSmpCnt * twoPi / wndSmpCnt;
    p->maxRphV[k] = ((cmReal_t)(k+m)) * hopSmpCnt * twoPi / wndSmpCnt;

    //printf("%f %f %f\n",p->itrV[k],p->minRphV[k],p->maxRphV[k]);
  }

  return rc;  
}

cmRC_t     cmPvSynFinal(cmPvSyn* p )
{ return cmOkRC; }

cmRC_t     cmPvSynExec( cmPvSyn* p, const cmReal_t* magV, const cmReal_t* phsV )
{
  double   twoPi     = 2.0 * M_PI;
  unsigned k;

  for(k=0; k<p->binCnt; ++k)
  {
    // phase dist between cur and prv frame
    cmReal_t dp = phsV[k] - p->phs0V[k];

    // dist must be positive (accum phase always increases)
    if( dp < -0.00001 )
      dp += twoPi;

    // add in complete revolutions based on the bin frequency
    // (these would have been lost from 'dp' due to phase wrap)
    dp += p->itrV[k];

    // constrain the phase change to lie within the range of the kth bin
    if( dp < p->minRphV[k] )
      dp += twoPi;

    if( dp > p->maxRphV[k] )
      dp -= twoPi;

    p->phsV[k] = p->phs0V[k] + dp;
    p->magV[k] = p->mag0V[k];
     
    
    p->phs0V[k] = phsV[k];
    p->mag0V[k] = magV[k];
  }
  
  cmIFftExecPolarRS( &p->ft, magV, phsV );
  
  cmOlaExecS( &p->ola, p->ft.outV, p->ft.outN ); 

  //printf("%i %i\n",p->binCnt,p->ft.binCnt );

  //cmVOR_Print( p->obj.ctx->outFuncPtr, 1, p->binCnt, magV );
  //cmVOR_Print( p->obj.ctx->outFuncPtr, 1, p->binCnt, p->phsV );
  //cmVOS_Print( p->obj.ctx->outFuncPtr, 1, 10, p->ft.outV );

  return cmOkRC;
}

cmRC_t  cmPvSynDoIt( cmPvSyn* p, const cmSample_t* v )
{
  cmOlaExecS( &p->ola, v, p->wndSmpCnt ); 

  //printf("%f\n",cmVOS_RMS(s,p->wndSmpCnt,p->wndSmpCnt));

  return cmOkRC;
}


const cmSample_t* cmPvSynExecOut(cmPvSyn* p )
{ return cmOlaExecOut(&p->ola); }

//------------------------------------------------------------------------------------------------------------
cmMidiSynth* cmMidiSynthAlloc( cmCtx* ctx, cmMidiSynth* ap, const cmMidiSynthPgm* pgmArray, unsigned pgmCnt, unsigned voiceCnt, unsigned procSmpCnt, unsigned outChCnt, cmReal_t srate  )
{
  cmMidiSynth* p = cmObjAlloc( cmMidiSynth, ctx, ap );

  if( pgmArray != NULL )
    if( cmMidiSynthInit( p, pgmArray, pgmCnt, voiceCnt, procSmpCnt, outChCnt, srate ) != cmOkRC )
      cmMidiSynthFree(&p);
  return p;
}

cmRC_t       cmMidiSynthFree(  cmMidiSynth** pp )
{
  cmRC_t rc;

  if( pp==NULL || *pp==NULL)
    return cmOkRC;
  
  cmMidiSynth* p = *pp;

  if((rc = cmMidiSynthFinal(p)) != cmOkRC )
    return rc;
  
  cmMemPtrFree(&p->voiceArray);
  cmMemPtrFree(&p->outM);
  cmMemPtrFree(&p->outChArray);

  cmObjFree(pp);
  return cmOkRC;
}

cmRC_t       cmMidiSynthInit(  cmMidiSynth* p, const cmMidiSynthPgm* pgmArray, unsigned pgmCnt, unsigned voiceCnt, unsigned procSmpCnt, unsigned outChCnt, cmReal_t srate  )
{
  // at least one pgm must be given
  assert( pgmCnt > 0 );
  
  unsigned i;
  cmRC_t rc;
  if((rc = cmMidiSynthFinal(p)) != cmOkRC )
    return rc;

  p->voiceArray     = cmMemResizeZ( cmMidiVoice, p->voiceArray, voiceCnt );
  p->outM           = cmMemResizeZ( cmSample_t,  p->outM,       outChCnt * procSmpCnt );
  p->outChArray     = cmMemResizeZ( cmSample_t*, p->outChArray, outChCnt );
  p->avail          = p->voiceArray;
  p->voiceCnt       = voiceCnt;
  p->activeVoiceCnt = 0;
  p->voiceStealCnt  = 0;
  p->procSmpCnt     = procSmpCnt;
  p->outChCnt       = outChCnt;
  p->srate          = srate;

  for(i=0; i<outChCnt; ++i)
    p->outChArray[i] = p->outM + (i*procSmpCnt);

  for(i=0; i<kMidiChCnt; ++i)
  {
    p->chArray[i].pgm       = 0;
    p->chArray[i].active    = NULL;
    p->chArray[i].pitchBend = 0;
    p->chArray[i].synthPtr  = p;
    memset(p->chArray[i].midiCtl, 0, kMidiCtlCnt * sizeof(cmMidiByte_t));
  }

  for(i=0; i<voiceCnt; ++i)
  {
    p->voiceArray[i].index         = i;
    p->voiceArray[i].flags         = 0;
    p->voiceArray[i].pitch         = kInvalidMidiPitch;
    p->voiceArray[i].velocity      = kInvalidMidiVelocity;
    p->voiceArray[i].pgm.pgm       = kInvalidMidiPgm;
    p->voiceArray[i].pgm.cbPtr     = NULL;
    p->voiceArray[i].pgm.cbDataPtr = NULL;
    p->voiceArray[i].chPtr         = NULL;
    p->voiceArray[i].link          = i<voiceCnt-1 ? p->voiceArray + i + 1 : NULL;
  }  

  for(i=0; i<pgmCnt; ++i)
  {
    unsigned idx = pgmArray[i].pgm;
    if( idx >= kMidiPgmCnt )
      rc = cmCtxRtCondition( &p->obj, cmArgAssertRC, "MIDI program change values must be less than %i.",kMidiPgmCnt);
    else
    {
      p->pgmArray[ idx ].cbPtr     = pgmArray[i].cbPtr;
      p->pgmArray[ idx ].cbDataPtr = pgmArray[i].cbDataPtr;
      p->pgmArray[ idx ].pgm       = idx;
    }
  }

  return rc;
}

cmRC_t       cmMidiSynthFinal( cmMidiSynth*  p )
{ return cmOkRC; }

cmRC_t _cmMidiSynthOnNoteOn( cmMidiSynth* p, cmMidiByte_t ch, cmMidiByte_t pitch, cmMidiByte_t vel )
{
  assert( ch < kMidiChCnt );
  
  if( p->activeVoiceCnt == p->voiceCnt )
  {
    ++p->voiceStealCnt;
    return cmOkRC;
  }

  assert( p->avail != NULL );

  cmMidiSynthCh* chPtr = p->chArray + ch;
  cmMidiVoice*   vp    = p->avail;

  ++p->activeVoiceCnt;

  // update avail
  p->avail = p->avail->link;

  // update active
  vp->flags         |= kActiveMsFl | kKeyGateMsFl;
  vp->pitch          = pitch;
  vp->velocity       = vel;
  vp->pgm            = p->pgmArray[ chPtr->pgm ];
  vp->chPtr          = chPtr;
  vp->link           = chPtr->active;

  chPtr->active       = vp;

  vp->pgm.cbPtr( vp, kAttackMsId, NULL, 0 );

  return cmOkRC;
}

cmRC_t _cmMidiSynthOnNoteOff( cmMidiSynth* p, cmMidiByte_t ch, cmMidiByte_t pitch, cmMidiByte_t vel )
{
  assert( ch < kMidiChCnt );

  cmMidiSynthCh* cp = p->chArray + ch;
  cmMidiVoice*   vp = cp->active;

  // find the voice for the given pitch
  while( vp != NULL )
  {
    if( (vp->pitch == pitch) && (cmIsFlag(vp->flags,kKeyGateMsFl)==true) )
      break;

    vp = vp->link;
  }

  // if no voice had a key down on this pitch
  if( vp == NULL )
  {
    return cmOkRC;
  }

  // mark the key as 'up'
  vp->flags = cmClrFlag(vp->flags,kKeyGateMsFl);

  // if the sustain pedal is up
  if( cp->midiCtl[ kSustainCtlMdId ] == 0 )
    vp->pgm.cbPtr( vp, kReleaseMsId, NULL, 0 );

  return cmOkRC;
}

cmRC_t _cmMidiSynthOnCtl( cmMidiSynth* p, cmMidiByte_t ch, cmMidiByte_t ctlId, cmMidiByte_t ctlValue )
{
  assert( ch < kMidiChCnt && ctlId < kMidiCtlCnt );

  cmMidiSynthCh* cp = p->chArray + ch;

  cp->midiCtl[ ctlId ] = ctlValue;
  
  // if the sustain pedal is going up
  if( ctlId == kSustainCtlMdId && ctlValue == 0 )
  {
    cmMidiVoice* vp = cp->active;
    while(vp != NULL)
    {
      if( cmIsFlag(vp->flags,kKeyGateMsFl)==false )
        vp->pgm.cbPtr(vp, kReleaseMsId, NULL, 0 );

      vp = vp->link;
    }
  }

  //printf("%i %i %f\n",ctlId,ctlValue,ctlValue/127.0);

  return cmOkRC;
}

cmRC_t       cmMidiSynthOnMidi( cmMidiSynth* p, const cmMidiPacket_t* pktArray, unsigned pktCnt )
{
  unsigned i=0;
  for(i=0; i<pktCnt; ++i)
    if( pktArray[i].msgArray != NULL )
    {
      unsigned j;
      for(j=0; j<pktArray[i].msgCnt; ++j)
      {
        const cmMidiMsg* mp     = pktArray[i].msgArray + j;
        cmMidiByte_t     ch     = mp->status & 0x0f;
        cmMidiByte_t     status = mp->status & 0xf0;
        
        switch( status )
        {
          case kNoteOnMdId:
            if( mp->d1 != 0 )
            {
              _cmMidiSynthOnNoteOn(p,ch,mp->d0,mp->d1);
              break;
            }
            // fall through

          case kNoteOffMdId:
            _cmMidiSynthOnNoteOff(p,ch,mp->d0,mp->d1);
            break;

          case kPolyPresMdId:
            break;

          case kCtlMdId:
            _cmMidiSynthOnCtl( p, ch, mp->d0, mp->d1 );
            break;

          case kPgmMdId:
            break;

          case kChPresMdId:
            break;

          case kPbendMdId:
            break;

          default:
            printf("Unknown MIDI status:%i %i\n",(int)status,(int)mp->status);
            break;
        }
      }
    }
  return cmOkRC;
}

cmRC_t       cmMidiSynthExec(  cmMidiSynth* p, cmSample_t* outChArray[], unsigned outChCnt )
{
  unsigned i;

  cmSample_t** chArray = outChArray == NULL ? p->outChArray : outChArray;
  unsigned     chCnt   = outChArray == NULL ? p->outChCnt   : outChCnt;

  // FIX: make one active chain attached to cmMidiSynth rather than many 
  // active chains attached to each channel - this will avoid the extra
  // iterations below.


  // for each channel
  for(i=0; i<kMidiChCnt; ++i)  
  {
    cmMidiVoice* vp  = p->chArray[i].active;
    cmMidiVoice* prv = NULL;

    // for each voice assigned to this channel
    while(vp != NULL)
    {
      // tell the voice to perform its DSP function  - returns 0 if the voice is no longer active
      if( vp->pgm.cbPtr( vp, kDspMsId, chArray, chCnt ) )
      {
        prv = vp;
        vp  = vp->link;
      }
      else
      {
        cmMidiVoice* nvp = vp->link;

        // remove vp from the active chain
        if( prv != NULL )
          prv->link = vp->link;
        else
        {
          assert( vp == p->chArray[i].active );

          // vp is first recd on active chain, nvp becomes first ...
          p->chArray[i].active = vp->link;   
          prv                  = NULL; // ... so prv must be NULL 
        }

        // insert this voice on the available chain
        vp->link = p->avail;
        p->avail = vp;

        --p->activeVoiceCnt;

        vp = nvp;
      }        
    }
  }
  return cmOkRC;
}

//------------------------------------------------------------------------------------------------------------
cmWtVoice* cmWtVoiceAlloc( cmCtx* ctx, cmWtVoice* ap, unsigned procSmpCnt, cmReal_t hz )
{
  cmWtVoice* p = cmObjAlloc( cmWtVoice, ctx, ap );
  if( procSmpCnt != 0 )
    if( cmWtVoiceInit( p, procSmpCnt, hz ) != cmOkRC )
      cmWtVoiceFree(&p);
  return p;
}

cmRC_t     cmWtVoiceFree( cmWtVoice** pp )
{
  cmRC_t rc = cmOkRC;

  if( pp==NULL || *pp==NULL )
    return cmOkRC;

  cmWtVoice* p = *pp;

  if((rc = cmWtVoiceFinal(p)) != cmOkRC )
    return rc;

  cmMemPtrFree(&p->outV);
  cmObjFree(pp);
  return rc;
}

cmRC_t     cmWtVoiceInit( cmWtVoice*  p, unsigned procSmpCnt, cmReal_t hz )
{
  p->outV      = cmMemResizeZ( cmSample_t, p->outV, procSmpCnt );
  p->outN      = procSmpCnt;
  p->hz        = hz;
  p->level     = 0;
  p->durSmpCnt = 0;
  p->phase     = 0;
  p->state     = kOffWtId;
  return cmOkRC;
}

cmRC_t     cmWtVoiceFinal( cmWtVoice* p )
{ return cmOkRC; }

int        cmWtVoiceExec( cmWtVoice* p, struct cmMidiVoice_str* mvp, unsigned sel, cmSample_t* outChArray[], unsigned outChCnt )
{
  switch( sel )
  {
    case kAttackMsId:  
      p->state = kAtkWtId;
      p->hz    = (13.75 * pow(2,(-9.0/12.0))) * pow(2,((double)mvp->pitch / 12));
      //printf("%fhz\n",p->hz);
      break;

    case kReleaseMsId:
      p->state = kRlsWtId;
      //printf("rls:%f\n",p->phase);
      break;

    case kDspMsId:
      {
        if( p->state == kRlsWtId )
        {
          p->state = kOffWtId;
          return 0;
        }

        cmMidiSynth* sp  = mvp->chPtr->synthPtr;
        cmSample_t*  dp  = outChCnt == 0 ? p->outV : outChArray[0];
        cmSample_t*  ep  = dp + p->outN;
        cmReal_t     rps = (2.0 * M_PI * p->hz) / sp->srate;
        cmReal_t sum=0;
        unsigned i=0;

        for(; dp < ep; ++dp)
        {
          *dp += (cmSample_t)(0.5 * sin( p->phase ));
          sum += *dp;
          ++i;
          p->phase += rps;
        }

        //printf("(%f %f %i %i %p) ",p->phase,sum,i,p->outN,outChArray[0] );
      }    
      break;

    default:
      assert(0);
      break;
  }

  return 1;
}

//------------------------------------------------------------------------------------------------------------
cmWtVoiceBank* cmWtVoiceBankAlloc( cmCtx* ctx, cmWtVoiceBank* ap, double srate, unsigned procSmpCnt, unsigned voiceCnt, unsigned chCnt )
{
  cmWtVoiceBank* p = cmObjAlloc( cmWtVoiceBank, ctx, ap );
  if( srate != 0 )
    if( cmWtVoiceBankInit( p, srate, procSmpCnt, voiceCnt, chCnt ) != cmOkRC )
      cmWtVoiceBankFree(&p);
  return p;
}

cmRC_t     cmWtVoiceBankFree( cmWtVoiceBank** pp )
{
  cmRC_t rc;

  if( pp==NULL || *pp==NULL)
    return cmOkRC;

  cmWtVoiceBank* p = *pp;

  if((rc = cmWtVoiceBankFinal(p)) != cmOkRC )
    return rc;

  cmMemPtrFree(&p->voiceArray);
  cmMemPtrFree(&p->chArray);
  cmMemPtrFree(&p->buf);

  cmObjFree(pp);
  return rc;

}

cmRC_t     cmWtVoiceBankInit( cmWtVoiceBank*  p, double srate, unsigned procSmpCnt, unsigned voiceCnt, unsigned chCnt )
{
  cmRC_t rc;
  unsigned i;

  if((rc = cmWtVoiceBankFinal(p)) != cmOkRC )
    return rc;

  p->voiceArray = cmMemResizeZ( cmWtVoice*, p->voiceArray, voiceCnt );

  for(i=0; i<voiceCnt; ++i)
    p->voiceArray[i] = cmWtVoiceAlloc(p->obj.ctx,NULL,procSmpCnt,0);

  p->voiceCnt = voiceCnt;

  p->buf = cmMemResizeZ( cmSample_t, p->buf, chCnt * procSmpCnt );

  p->chArray    = cmMemResizeZ( cmSample_t*, p->chArray, chCnt );

  for(i=0; i<chCnt; ++i)
    p->chArray[i] = p->buf + (i*procSmpCnt);

  p->chCnt     = chCnt;
  p->procSmpCnt = procSmpCnt;

  return cmOkRC;
}

cmRC_t     cmWtVoiceBankFinal( cmWtVoiceBank* p )
{ 
  unsigned i;
  for(i=0; i<p->voiceCnt; ++i)
    cmWtVoiceFree(&p->voiceArray[i]);
  
  return cmOkRC;    
}

int        cmWtVoiceBankExec( cmWtVoiceBank* p, struct cmMidiVoice_str* voicePtr, unsigned sel, cmSample_t* outChArray[], unsigned outChCnt )
{
  cmWtVoice* vp = p->voiceArray[ voicePtr->index ];
  bool fl = outChArray==NULL || outChCnt==0;
  cmSample_t** chArray = fl ? p->chArray : outChArray;
  unsigned     chCnt   = fl ? p->chCnt   : outChCnt;
  return cmWtVoiceExec( vp, voicePtr, sel, chArray, chCnt );
}


//------------------------------------------------------------------------------------------------------------


cmAudioFileBuf* cmAudioFileBufAlloc( cmCtx* ctx, cmAudioFileBuf* ap, unsigned procSmpCnt, const char* fn, unsigned audioChIdx, unsigned begSmpIdx, unsigned durSmpCnt )
{
  cmAudioFileBuf* p = cmObjAlloc( cmAudioFileBuf, ctx, ap );
  if( procSmpCnt != 0 )
    if( cmAudioFileBufInit( p, procSmpCnt, fn, audioChIdx, begSmpIdx, durSmpCnt ) != cmOkRC )
      cmAudioFileBufFree(&p);
  return p;
}

cmRC_t          cmAudioFileBufFree( cmAudioFileBuf** pp )
{
  cmRC_t rc;

  if( pp==NULL || *pp==NULL)
    return cmOkRC;

  cmAudioFileBuf* p = *pp;

  if((rc = cmAudioFileBufFinal(p)) != cmOkRC )
    return rc;

  cmMemPtrFree(&p->bufV);
  cmMemPtrFree(&p->fn);
  cmObjFree(pp);
  return rc;
}

cmRC_t          cmAudioFileBufInit( cmAudioFileBuf* p, unsigned procSmpCnt, const char* fn, unsigned audioChIdx, unsigned begSmpIdx, unsigned durSmpCnt )
{
  cmAudioFileH_t afH;
  cmRC_t       rc;

  if((rc = cmAudioFileBufFinal(p)) != cmOkRC )
    return rc;

  // open the audio file for reading
  if( cmAudioFileIsValid( afH = cmAudioFileNewOpen( fn, &p->info, &rc, p->obj.err.rpt ))==false || rc != kOkAfRC )
    return cmCtxRtCondition(&p->obj, cmArgAssertRC,"The audio file '%s' could not be opend.",fn );

  // validate the audio channel
  if( audioChIdx >= p->info.chCnt )
    return cmCtxRtCondition(&p->obj, cmArgAssertRC,"The audio file channel index %i is out of range for the audio file '%s'.",audioChIdx,fn);
  
  // validate the start sample index
  if( begSmpIdx > p->info.frameCnt )
    return cmCtxRtCondition(&p->obj, cmOkRC, "The start sample index %i is past the end of the audio file '%s'.",begSmpIdx,fn);

  if( durSmpCnt == cmInvalidCnt )
    durSmpCnt = p->info.frameCnt - begSmpIdx;

  // validate the duration
  if( begSmpIdx + durSmpCnt > p->info.frameCnt )
  {
    unsigned newDurSmpCnt = p->info.frameCnt - begSmpIdx;
    cmCtxRtCondition(&p->obj, cmOkRC, "The selected sample duration %i is past the end of the audio file '%s' and has been shorted to %i samples.",durSmpCnt,fn,newDurSmpCnt);
    durSmpCnt = newDurSmpCnt;
  }

  // seek to the starting sample
  if( cmAudioFileSeek( afH, begSmpIdx ) != kOkAfRC )
    return cmCtxRtCondition(&p->obj, cmArgAssertRC,"Seek to sample index %i failed on the audio file '%s'.",begSmpIdx,fn);

  // allocate the buffer memory
  p->bufV          = cmMemResizeZ( cmSample_t, p->bufV, durSmpCnt );
  p->fn            = cmMemResize(  char,       p->fn,   strlen(fn)+1 );
  p->bufN          = durSmpCnt;
  p->begSmpIdx     = begSmpIdx;
  p->chIdx         = audioChIdx;

  strcpy(p->fn,fn);

  cmSample_t* outV = p->bufV;


  // read the file into the buffer
  unsigned rdSmpCnt = cmMin(4096,durSmpCnt);
  unsigned cmc      = 0;
  while( cmc < durSmpCnt )
  {
    unsigned actualReadCnt = 0;
    unsigned n             = rdSmpCnt;
    cmSample_t* chArray[]  = {outV};

    if( cmc + n > durSmpCnt )
      n = durSmpCnt - cmc;

    if((rc=cmAudioFileReadSample( afH, n, audioChIdx, 1, chArray, &actualReadCnt)) != kOkAfRC )
      break;

    cmc  += actualReadCnt;
    outV += actualReadCnt;

  }

  if( rc==kOkAfRC || (rc != kOkAfRC && cmAudioFileIsEOF(afH)))
    rc =  cmOkRC;

  return rc;
}

cmRC_t    cmAudioFileBufFinal(cmAudioFileBuf* p )
{ return cmOkRC; }

unsigned  cmAudioFileBufExec( cmAudioFileBuf* p, unsigned smpIdx, cmSample_t* outV, unsigned outN, bool sumIntoOutFl )
{
  if( outV == NULL || outN == 0 || smpIdx >= p->bufN )
    return 0;

   unsigned n = cmMin(outN,p->bufN-smpIdx);
   if( sumIntoOutFl )
     cmVOS_AddVV(outV,n,p->bufV + smpIdx);
   else
     cmVOS_Copy(outV,n,p->bufV + smpIdx );

   if( n < outN )
     memset(outV+n,0,(outN-n)*sizeof(cmSample_t));

  return n;
}

//------------------------------------------------------------------------------------------------------------
cmMDelay* cmMDelayAlloc( cmCtx* ctx, cmMDelay* ap, unsigned procSmpCnt, cmReal_t srate, cmReal_t fbCoeff, unsigned delayCnt, const cmReal_t* delayMsArray, const cmReal_t* delayGainArray )
{
  cmMDelay* p = cmObjAlloc( cmMDelay, ctx, ap );
  if( procSmpCnt != 0 )
    if( cmMDelayInit( p, procSmpCnt, srate, fbCoeff, delayCnt, delayMsArray, delayGainArray ) != cmOkRC )
      cmMDelayFree(&p);
  return p;

}

cmRC_t    cmMDelayFree(  cmMDelay** pp )
{
  cmRC_t rc;

  if( pp == NULL || *pp==NULL)
    return cmOkRC;

  cmMDelay* p = *pp;
  
  if((rc = cmMDelayFinal(p)) != cmOkRC )
    return rc;

  unsigned i;
  for(i=0; i<p->delayCnt; ++i)
    cmMemPtrFree(&p->delayArray[i].delayBuf);

  cmMemPtrFree(&p->delayArray);
  cmMemPtrFree(&p->outV);
  cmObjFree(pp);

  return cmOkRC;

}

cmRC_t    cmMDelayInit( cmMDelay* p, unsigned procSmpCnt, cmReal_t srate, cmReal_t fbCoeff, unsigned delayCnt, const cmReal_t* delayMsArray, const cmReal_t* delayGainArray )
{
  cmRC_t rc;
  if((rc = cmMDelayFinal(p)) != cmOkRC )
    return rc;

  if( delayCnt <= 0 )
    return rc;

  p->delayArray = cmMemResizeZ( cmMDelayHead, p->delayArray, delayCnt );

  unsigned i;
  for(i=0; i<delayCnt; ++i)
  {
    p->delayArray[i].delayGain      = delayGainArray == NULL ? 1.0 : delayGainArray[i];
    p->delayArray[i].delayMs        = delayMsArray[i];
    p->delayArray[i].delaySmpFrac   = delayMsArray[i] * srate / 1000.0;
    p->delayArray[i].delayBufSmpCnt = ceil(delayMsArray[i] * srate / 1000)+2;
    p->delayArray[i].delayBuf       = cmMemResizeZ( cmSample_t, p->delayArray[i].delayBuf, p->delayArray[i].delayBufSmpCnt );
    p->delayArray[i].inIdx          = 0;

  }
  
  p->delayCnt= delayCnt;
  p->outV    = cmMemResizeZ( cmSample_t, p->outV, procSmpCnt );
  p->outN    = procSmpCnt;
  p->fbCoeff = fbCoeff;
  p->srate   = srate;

  return cmOkRC;
}

cmRC_t    cmMDelayFinal( cmMDelay* p )
{ return cmOkRC; }


void _cmMDelayExec(  cmMDelay* p, cmMDelayHead* hp, const cmSample_t inV[], cmSample_t outV[], unsigned sigN )
{
	cmSample_t*	dl	 = hp->delayBuf;                                                  // ptr to the base of the delay line
	cmReal_t		dfi	 = (cmReal_t)(hp->inIdx - hp->delaySmpFrac) + hp->delayBufSmpCnt; // fractional delay in samples
	int         dii0 = ((int)dfi)  % hp->delayBufSmpCnt;                              // index to the sample just before the delay position
	int         dii1 = (dii0 + 1) % hp->delayBufSmpCnt;                               // index to the sample just after the delay position
	//cmReal_t		frac = 0; //dfi - dii0;                                               // interpolation coeff.
	unsigned    i;

	for(i=0; i<sigN; i++)
	{		
    /*
		outPtr[i] = -(((f+0)*(f-1)*(f-2)/6)  * _wtPtr[iPhs0])
					+(((f+1)*(f-1)*(f-2)/2)  * _wtPtr[iPhs0+1])
					-(((f+1)*(f-0)*(f-2)/2)  * _wtPtr[iPhs0+2]) 
					+(((f+1)*(f-0)*(f-1)/6)  * _wtPtr[iPhs0+3]);
    */

		cmSample_t outSmp = dl[dii0]; // + (frac * (dl[dii1]-dl[dii0]));	

    outV[i] += outSmp/p->delayCnt;

		dl[hp->inIdx] = (p->fbCoeff * outSmp)  + inV[i];		

		hp->inIdx = (hp->inIdx+1) % hp->delayBufSmpCnt;
		dii0      = (dii0+1)      % hp->delayBufSmpCnt;
		dii1      = (dii1+1)      % hp->delayBufSmpCnt;
	}	
}



cmRC_t    cmMDelayExec(  cmMDelay* p, const cmSample_t* inV, cmSample_t* outV, unsigned sigN, bool bypassFl )
{
  assert( sigN <= p->outN);

  if( outV == NULL )
  {
    outV = p->outV;
    sigN = cmMin(sigN,p->outN);
    cmVOS_Fill(outV,sigN,0);
  }
  else
  {
    cmVOS_Zero(outV,sigN);
  }

  if( inV == NULL )
    return cmOkRC;

  if( bypassFl )
  {
    memcpy(outV,inV,sigN*sizeof(cmSample_t));
    return cmOkRC;
  }

  unsigned di;
  for( di=0; di<p->delayCnt; ++di)
  {
    cmMDelayHead* hp   = p->delayArray + di;
    hp->delaySmpFrac   = hp->delayMs * p->srate / 1000.0;
    _cmMDelayExec(p,hp,inV,outV,sigN);
  }

  return cmOkRC;
}

void cmMDelaySetTapMs( cmMDelay* p, unsigned tapIdx, cmReal_t ms )
{
  assert( tapIdx < p->delayCnt );
  p->delayArray[tapIdx].delayMs = ms;
}

void cmMDelaySetTapGain(cmMDelay* p, unsigned tapIdx, cmReal_t gain )
{
  assert( tapIdx < p->delayCnt );
  p->delayArray[tapIdx].delayGain = gain;
}

void cmMDelayReport( cmMDelay* p, cmRpt_t* rpt )
{
  cmRptPrintf(rpt,"tap cnt:%i fb:%f sr:%f\n",p->delayCnt,p->fbCoeff,p->srate);
}


//------------------------------------------------------------------------------------------------------------

cmAudioSegPlayer* cmAudioSegPlayerAlloc( cmCtx* ctx, cmAudioSegPlayer* ap, unsigned procSmpCnt, unsigned outChCnt )
{
  cmAudioSegPlayer* p = cmObjAlloc( cmAudioSegPlayer, ctx, ap );
  if( procSmpCnt != 0 )
    if( cmAudioSegPlayerInit( p, procSmpCnt, outChCnt ) != cmOkRC )
      cmAudioSegPlayerFree(&p);
  return p;

}

cmRC_t            cmAudioSegPlayerFree(   cmAudioSegPlayer** pp )
{
  if( pp == NULL || *pp == NULL )
    return cmOkRC;

  cmAudioSegPlayer* p = *pp;

  cmMemPtrFree(&p->segArray);
  cmMemPtrFree(&p->outM);
  cmObjFree(pp);
  return cmOkRC;
}

cmRC_t            cmAudioSegPlayerInit(   cmAudioSegPlayer* p, unsigned procSmpCnt, unsigned outChCnt )
{
  cmRC_t rc = cmOkRC;

  if((rc = cmAudioSegPlayerFinal(p)) != cmOkRC )
    return rc;

  p->procSmpCnt = procSmpCnt;
  p->outChCnt   = outChCnt;
  p->segCnt     = 0;

  if( outChCnt )
  {
    unsigned i;

    p->outM       = cmMemResizeZ( cmSample_t,  p->outM,       procSmpCnt * outChCnt );
    p->outChArray = cmMemResizeZ( cmSample_t*, p->outChArray, outChCnt );

    for(i=0; i<outChCnt; ++i)
      p->outChArray[i] = p->outM + (i*procSmpCnt);
  }

  return rc;
}

cmRC_t            cmAudioSegPlayerFinal(  cmAudioSegPlayer* p )
{ return cmOkRC; }

cmRC_t            _cmAudioSegPlayerSegSetup(   cmAudioSeg* sp, unsigned id, cmAudioFileBuf* bufPtr, unsigned smpIdx, unsigned smpCnt, unsigned outChIdx )
{
  sp->bufPtr    = bufPtr;
  sp->id        = id;
  sp->smpIdx    = smpIdx;
  sp->smpCnt    = smpCnt;
  sp->outChIdx  = outChIdx;
  sp->outSmpIdx = 0;
  sp->flags     = 0;

  return cmOkRC;
}

cmAudioSeg* _cmAudioSegPlayerIdToSegPtr( cmAudioSegPlayer* p, unsigned id, bool ignoreErrFl )
{
  unsigned i = 0;
  for(i=0; i<p->segCnt; ++i)
    if( p->segArray[i].id == id )
      return p->segArray + i;

  if( !ignoreErrFl )
    cmCtxRtCondition(&p->obj, cmArgAssertRC,"Unable to locate an audio segment with id=%i.",id);

  return NULL;
}


cmRC_t            cmAudioSegPlayerInsert( cmAudioSegPlayer* p, unsigned id, cmAudioFileBuf* bufPtr, unsigned smpIdx, unsigned smpCnt, unsigned outChIdx )
{
  cmRC_t rc;

  assert( _cmAudioSegPlayerIdToSegPtr( p, id, true ) == NULL );

  p->segArray = cmMemResizePZ( cmAudioSeg, p->segArray, p->segCnt + 1 );
  
  cmAudioSeg* sp = p->segArray + p->segCnt;

  if((rc = _cmAudioSegPlayerSegSetup( sp, id, bufPtr, smpIdx, smpCnt, outChIdx )) == cmOkRC )
    ++p->segCnt;

  return rc;
}


cmRC_t            cmAudioSegPlayerEdit(   cmAudioSegPlayer* p, unsigned id, cmAudioFileBuf* bufPtr, unsigned smpIdx, unsigned smpCnt, unsigned outChIdx )
{
  cmAudioSeg* sp = _cmAudioSegPlayerIdToSegPtr(p,id,false);

  return _cmAudioSegPlayerSegSetup( sp, id, bufPtr, smpIdx, smpCnt, outChIdx );
}


cmRC_t            cmAudioSegPlayerRemove( cmAudioSegPlayer* p, unsigned id, bool delFl )
{
  cmAudioSeg* sp = _cmAudioSegPlayerIdToSegPtr(p,id,false);

  if( sp == NULL )
    return cmArgAssertRC;

  sp->flags = cmEnaFlag( sp->flags, kDelAspFl, delFl );

  return cmOkRC;
}

cmRC_t            cmAudioSegPlayerEnable( cmAudioSegPlayer* p, unsigned id, bool enableFl, unsigned outSmpIdx )
{
  cmAudioSeg* sp = _cmAudioSegPlayerIdToSegPtr(p,id,false);

  if( sp == NULL )
    return cmArgAssertRC;

  if( outSmpIdx != cmInvalidIdx )
    sp->outSmpIdx = outSmpIdx;

  sp->flags = cmEnaFlag( sp->flags, kEnableAspFl, enableFl );

  return cmOkRC;
}

void _cmAudioSegPlayerResetSeg( cmAudioSeg* sp )
{
    sp->outSmpIdx = 0;
    sp->flags     = cmClrFlag(sp->flags, kEnableAspFl );
}

cmRC_t            cmAudioSegPlayerReset(  cmAudioSegPlayer* p )
{
  unsigned i;
  for(i=0; i<p->segCnt; ++i)
  {
    cmAudioSeg* sp = p->segArray + i;
    _cmAudioSegPlayerResetSeg(sp);
  }
  return cmOkRC;
}

cmRC_t            cmAudioSegPlayerExec(   cmAudioSegPlayer* p, cmSample_t** outChPtr, unsigned outChCnt, unsigned procSmpCnt )
{
  unsigned    i;

  if( outChPtr == NULL || outChCnt == 0 )
  {
    assert( p->outChCnt > 0 );

    outChPtr = p->outChArray;
    outChCnt = p->outChCnt;

    assert( p->procSmpCnt <= procSmpCnt );
  }


  for(i=0; i<p->segCnt; ++i)
  {
    cmAudioSeg* sp = p->segArray + i;

    // if the output channel is valid and the segment is enabled and not deleted
    if( sp->outChIdx < outChCnt && (sp->flags & (kEnableAspFl | kDelAspFl)) == kEnableAspFl )
    {
      unsigned bufSmpIdx = sp->smpIdx + sp->outSmpIdx;
      unsigned bufSmpCnt = 0;

      // if all the samples have been played
      if( sp->bufPtr->bufN <= bufSmpIdx )
        _cmAudioSegPlayerResetSeg(sp);
      else
      {
        
        // prevent playing past the end of the buffer
        bufSmpCnt = cmMin( procSmpCnt, sp->bufPtr->bufN - bufSmpIdx );

        // limit the number of samples to the segment length
        bufSmpCnt = cmMin( bufSmpCnt,  sp->smpCnt - sp->outSmpIdx );

        // sum the samples into the output channel
        cmVOS_AddVV( outChPtr[ sp->outChIdx ], bufSmpCnt, sp->bufPtr->bufV + bufSmpIdx );

        // incr the next output sample index
        sp->outSmpIdx += bufSmpCnt;
      }

      if( bufSmpCnt < procSmpCnt )
        cmVOS_Zero( outChPtr[ sp->outChIdx ] + bufSmpCnt, procSmpCnt - bufSmpCnt );
      
    }
  }
  
  return cmOkRC;

}

//------------------------------------------------------------------------------------------------------------
/*
cmCluster0* cmCluster0Alloc( cmCtx* ctx, cmCluster0* ap, unsigned stateCnt, unsigned binCnt, unsigned flags, cmCluster0DistFunc_t distFunc, void* dstUserPtr )
{
  cmCluster0* p = cmObjAlloc( cmCluster0, ctx, ap );
  if( stateCnt != 0 )
    if( cmCluster0Init( p, stateCnt, binCnt, flags, distFunc, distUserPtr ) != cmOkRC )
      cmCluster0Free(&p);
  return p;
}

cmRC_t           cmCluster0Free(  cmCluster0** pp )
{
  if( pp == NULL || *pp == NULL )
    return cmOkRC;

  cmCluster0* p = *pp;

  cmMemPtrFree(&p->oM);
  cmMemPtrFree(&p->tM);
  cmMemPtrFree(&p->dV);
  cmObjFree(pp);
  return cmOkRC;
}

cmRC_t           cmCluster0Init(  cmCluster0* p,  unsigned stateCnt, unsigned binCnt, unsigned flags, cmCluster0DistFunc_t distFunc, void* distUserPtr )
{
  cmRC_t rc;
  if((rc = cmCluster0Final(p)) != cmOkRC )
    return rc;

  p->oM          = cmMemResizeZ( cmReal_t, p->oM, binCnt   * stateCnt );
  p->tM          = cmMemResizeZ( cmReal_t, p->tM, stateCnt * stateCnt );
  p->stateCnt    = stateCnt;
  p->binCnt      = binCnt;
  p->flags       = flags;
  p->distFunc    = distFunc;
  p->distUserPtr = distUserPtr;
  p->cnt         = 0;
}

cmRC_t           cmCluster0Final( cmCluster0* p )
{ return cmOkRC; }


cmRC_t           cmCluster0Exec(  cmCluster0* p, const cmReal_t* v, unsigned vn )
{
  assert( vn <= p->binCnt );

  ++cnt;  
  if( cnt <= stateCnt )
  {
    cmVOR_Copy( p->oM + ((cnt-1)*binCnt), vn, v );
    return cmOkRC;
  }

  
  

  return cmOkRC;


}
*/


  cmNmf_t* cmNmfAlloc( cmCtx* ctx, cmNmf_t* ap, unsigned n, unsigned m, unsigned r, unsigned maxIterCnt, unsigned convergeCnt )
{
  cmNmf_t* p = cmObjAlloc( cmNmf_t, ctx, ap );
  if( n != 0 )
    if( cmNmfInit( p, n, m, r, maxIterCnt, convergeCnt ) != cmOkRC )
      cmNmfFree(&p);
  return p;
  
}

cmRC_t   cmNmfFree( cmNmf_t** pp )
{
  if( pp== NULL || *pp == NULL )
    return cmOkRC;

  cmNmf_t* p = *pp;

  cmMemPtrFree(&p->V);
  cmMemPtrFree(&p->W);
  cmMemPtrFree(&p->H);
  cmMemPtrFree(&p->tr);
  cmMemPtrFree(&p->x);
  cmMemPtrFree(&p->t0nm);
  cmMemPtrFree(&p->t1nm);
  cmMemPtrFree(&p->Wt);
  cmMemPtrFree(&p->trm);
  cmMemPtrFree(&p->crm);
  cmMemPtrFree(&p->c0);
  cmMemPtrFree(&p->c1);
  cmMemPtrFree(&p->idxV);

  cmObjFree(pp);
  return cmOkRC;

  
}

cmRC_t   cmNmfInit( cmNmf_t* p,  unsigned n, unsigned m, unsigned r, unsigned maxIterCnt, unsigned convergeCnt )
{
  cmRC_t rc;

  if((rc = cmNmfFinal(p)) != cmOkRC )
    return rc;
  
  p->n          = n;
  p->m          = m;
  p->r          = r;
  p->maxIterCnt = maxIterCnt;
  p->convergeCnt= convergeCnt;
  p->V          = cmMemResizeZ(cmReal_t, p->V,    n*m );
  p->W          = cmMemResize( cmReal_t, p->W,    n*r );
  p->H          = cmMemResize( cmReal_t, p->H,    r*m );
  p->tr         = cmMemResize( cmReal_t, p->tr,   r );
  p->x          = cmMemResize( cmReal_t, p->x,    r*cmMax(m,n) );
  p->t0nm       = cmMemResize( cmReal_t, p->t0nm, cmMax(r,n)*m );
  p->Ht         = p->t0nm;
  p->t1nm       = cmMemResize( cmReal_t, p->t1nm, n*m );
  p->Wt         = cmMemResize( cmReal_t, p->Wt,   r*n );
  p->trm        = cmMemResize( cmReal_t, p->trm,  r*cmMax(m,n) );
  p->crm        = cmMemResizeZ(unsigned, p->crm,  r*m);
  p->tnr        = p->trm;
  p->c0         = cmMemResizeZ(unsigned, p->c0,   m*m);
  p->c1         = cmMemResizeZ(unsigned, p->c1,   m*m);
  p->idxV       = cmMemResizeZ(unsigned, p->idxV, m );
  p->c0m        = p->c0;
  p->c1m        = p->c1;

  cmVOR_Random(p->W,n*r,0.0,1.0);
  cmVOR_Random(p->H,r*m,0.0,1.0);

  return rc;
  
}

cmRC_t   cmNmfFinal(cmNmf_t* p )
{ return cmOkRC; }



// NMF base on: Lee and Seung, 2001, Algo's for Non-negative Matrix Fcmtorization
// Connectivity stopping technique based on: http://www.broadinstitute.org/mpr/publications/projects/NMF/nmf.m 
cmRC_t   cmNmfExec( cmNmf_t* p, const cmReal_t* vM, unsigned cn )
{
  cmRC_t   rc = cmOkRC;

  unsigned i,j,k;
  unsigned n        = p->n;
  unsigned m        = p->m;
  unsigned r        = p->r;
  unsigned stopIter = 0;

  assert(cn <= m );

  // shift in the incoming columns of V[]
  if( cn < m )
    cmVOR_Shift(p->V, n*m, n*cn,0);
  cmVOR_Copy( p->V, n*cn,vM );

  // shift H[] by the same amount as V[]
  if( cn < m )
    cmVOR_Shift( p->H, r*m, r*cn,0);
  cmVOR_Random(p->H, r*cn, 0.0, 1.0 );

  cmVOU_Zero( p->c1m, m*m );

  for(i=0,j=0; i<p->maxIterCnt && stopIter<p->convergeCnt; ++i)
  {
    // x[r,m] =repmat(sum(W,1)',1,m);
    cmVOR_SumM( p->W, n, r, p->tr );
    for(j=0; j<m; ++j)
      cmVOR_Copy( p->x + (j*r), r, p->tr );

    cmVOR_Transpose(p->Wt,p->W,n,r);

    //H=H.*(W'*(V./(W*H)))./x;
    cmVOR_MultMMM(p->t0nm,n,m,p->W,p->H,r);    // t0nm[n,m] =                W*H  
    cmVOR_DivVVV( p->t1nm,n*m,p->V,p->t0nm);   // t1nm[n,m] =            V./(W*H)
    cmVOR_MultMMM(p->trm,r,m,p->Wt,p->t1nm,n); // trm[r,m]  =        W'*(V./(W*H))
    cmVOR_MultVV(p->H,r*m,p->trm);             // H[r,m]    =  H .* (W'*(V./(W*H)))
    cmVOR_DivVV(p->H,r*m, p->x );              // H[r,m]    = (H .* (W'*(V./(W*H)))) ./ x

    // x[n,r]=repmat(sum(H,2)',n,1);
    cmVOR_SumMN(p->H, r, m, p->tr );
    for(j=0; j<n; ++j)
      cmVOR_CopyN(p->x + j, r, n, p->tr, 1 );

    cmVOR_Transpose(p->Ht,p->H,r,m);

    //  W=W.*((V./(W*H))*H')./x;
    cmVOR_MultMMM(p->tnr,n,r,p->t1nm,p->Ht,m); // tnr[n,r]  =       (V./(W*H))*Ht
    cmVOR_MultVV(p->W,n*r,p->tnr);             // W[n,r]    =    W.*(V./(W*H))*Ht
    cmVOR_DivVV(p->W,n*r,p->x);                // W[n,r]    =    W.*(V./(W*H))*Ht ./x

    if( i % 10 == 0 )
    {

      cmVOR_ReplaceLte( p->H, r*m, p->H, 2.2204e-16, 2.2204e-16 );
      cmVOR_ReplaceLte( p->W, n*r, p->W, 2.2204e-16, 2.2204e-16 );

      cmVOR_MaxIndexM( p->idxV, p->H, r, m );

      unsigned mismatchCnt = 0;

      for(j=0; j<m; ++j)
        for(k=0; k<m; ++k)
        {
          unsigned c_idx = (j*m)+k;
          p->c0m[ c_idx ] = p->idxV[j] == p->idxV[k];
          mismatchCnt += p->c0m[ c_idx ] != p->c1m[ c_idx ];
        }
      
      if( mismatchCnt == 0 )
        ++stopIter;
      else
        stopIter = 0;

      printf("%i %i %i\n",i,stopIter,mismatchCnt);
      fflush(stdout);

      unsigned* tcm = p->c0m;
      p->c0m = p->c1m;
      p->c1m = tcm;
    }
  }

  return rc;
}

//------------------------------------------------------------------------------------------------------------

cmRC_t _cmVectArrayAppend( cmVectArray_t* p, const void* v, unsigned typeByteCnt, unsigned valCnt )
{
  cmRC_t            rc      = cmSubSysFailRC;
  cmVectArrayVect_t* ep      = NULL;
  unsigned          byteCnt = typeByteCnt * valCnt;

  if( byteCnt == 0 || v == NULL )
    return rc;

  // verify that all vectors written to this vector array contain the same data type.
  if( (cmIsFlag(p->flags,kFloatVaFl) && typeByteCnt!=sizeof(float)) ||  (cmIsFlag(p->flags,kDoubleVaFl) && typeByteCnt!=sizeof(double)))  
    return cmCtxRtCondition(&p->obj,cmInvalidArgRC,"All data stored to a cmVectArray_t must be a consistent type.");

  // allocate space for the link record
  if((ep = cmMemAllocZ(cmVectArrayVect_t,1)) == NULL )
    goto errLabel;

  // allocate space for the vector data
  if((ep->u.v = cmMemAlloc(char,typeByteCnt*valCnt)) == NULL )
    goto errLabel;

  // append the link recd to the end of the  element list
  if( p->ep != NULL )
    p->ep->link = ep;
  else
  {
    p->bp = ep;
    p->cur = p->bp;
  }

  p->ep = ep;

  // store the length of the vector
  ep->n = valCnt;

  // copy in the vector data
  memcpy(ep->u.v,v,byteCnt);

  // track the number of vectors stored
  p->vectCnt += 1;

  // track the longest data vector
  if( valCnt > p->maxEleCnt )
    p->maxEleCnt = valCnt;

  rc = cmOkRC;

 errLabel:
  if(rc != cmOkRC )
  {
    cmMemFree(ep->u.v);
    cmMemFree(ep);    
  }

  return rc;
}

cmVectArray_t* cmVectArrayAlloc( cmCtx* ctx, unsigned flags )
{
  cmRC_t rc = cmOkRC;
  cmVectArray_t* p = cmObjAlloc(cmVectArray_t,ctx,NULL);

  assert(p != NULL);


  switch( flags & (kFloatVaFl | kDoubleVaFl | kSampleVaFl | kRealVaFl ) )
  {
    case kFloatVaFl:
      p->flags |= kFloatVaFl;
      p->typeByteCnt = sizeof(float);
      break;

    case kDoubleVaFl:
      p->flags |= kDoubleVaFl;
      p->typeByteCnt = sizeof(double);
      break;

    case kSampleVaFl:
      if( sizeof(cmSample_t) == sizeof(float) )
        p->flags |= kFloatVaFl;
      else
      {
        if( sizeof(cmSample_t) == sizeof(double))
          p->flags |= kDoubleVaFl;
        else
          rc = cmCtxRtCondition(&p->obj,cmInvalidArgRC,"The size of the cmSample_t is not consistent with float or double."); 
      }
      p->typeByteCnt = sizeof(cmSample_t);
      break;

    case kRealVaFl:
      if( sizeof(cmReal_t) == sizeof(float) )
        p->flags |= kFloatVaFl;
      else
      {
        if( sizeof(cmReal_t) == sizeof(double))
          p->flags |= kDoubleVaFl;
        else
          rc = cmCtxRtCondition(&p->obj,cmInvalidArgRC,"The size of the cmReal_t is not consistent with float or double."); 
      }
      p->typeByteCnt = sizeof(cmReal_t);
      break;

    default:
      rc = cmCtxRtCondition(&p->obj,cmInvalidArgRC,"The vector array value type flag was not recognized.");
  }

  

 if(rc != cmOkRC)
    cmVectArrayFree(&p);

  return p;      
}

cmVectArray_t* cmVectArrayAllocFromFile(cmCtx* ctx, const char* fn )
{
  cmRC_t         rc  = cmOkRC;
  FILE*          fp  = NULL;
  char*          buf = NULL;
  cmVectArray_t* p   = NULL;
  unsigned       hn  = 4;
  unsigned       hdr[hn];
  
    // create the file
  if((fp = fopen(fn,"rb")) == NULL )
  {
    rc = cmCtxRtCondition(&ctx->obj,cmSystemErrorRC,"The vector array file '%s' could not be opened.",cmStringNullGuard(fn));
    goto errLabel;
  }

  if( fread(hdr,sizeof(unsigned),hn,fp) != hn )
  {
    rc = cmCtxRtCondition(&ctx->obj,cmSystemErrorRC,"The vector array file header could not be read from '%s'.",cmStringNullGuard(fn));
    goto errLabel;
  }


  unsigned       flags       = hdr[0];
  unsigned       typeByteCnt = hdr[1];
  unsigned       vectCnt     = hdr[2];
  unsigned       maxEleCnt   = hdr[3];
  unsigned       i;

  buf    = cmMemAlloc(char,maxEleCnt*typeByteCnt);


  if((p = cmVectArrayAlloc(ctx, flags )) == NULL )
    goto errLabel;

  for(i=0; i<vectCnt; ++i)
  {
    unsigned vn;
    if( fread(&vn,sizeof(unsigned),1,fp) != 1 )
    {
      rc = cmCtxRtCondition(&p->obj,cmSystemErrorRC,"The vector array file element count read failed on vector index:%i in '%s'.",i,cmStringNullGuard(fn));
      goto errLabel;
    }

    assert( vn <= maxEleCnt );

    if( fread(buf,typeByteCnt,vn,fp) != vn )
    {
      rc = cmCtxRtCondition(&p->obj,cmSystemErrorRC,"The vector array data read failed on vector index:%i in '%s'.",i,cmStringNullGuard(fn));
      goto errLabel;
    }

    if((rc = _cmVectArrayAppend(p,buf, typeByteCnt, vn )) != cmOkRC )
    {
      rc = cmCtxRtCondition(&p->obj,rc,"The vector array data store failed on vector index:%i in '%s'.",i,cmStringNullGuard(fn));
      goto errLabel;
    }
    
  }
  

 errLabel:
  
  if( fp != NULL )
    fclose(fp);

  cmMemFree(buf);

  if(rc != cmOkRC && p != NULL) 
    cmVectArrayFree(&p);
 
  return p;

}


cmRC_t cmVectArrayFree(    cmVectArray_t** pp )
{
  cmRC_t rc = cmOkRC;

  if( pp == NULL || *pp == NULL )
    return rc;

  cmVectArray_t*     p  = *pp;

  if((rc = cmVectArrayClear(p)) != cmOkRC )
    return rc;

  cmMemFree(p->tempV);
  cmObjFree(pp);

  return rc;
}

cmRC_t cmVectArrayClear(   cmVectArray_t* p )
{
  cmVectArrayVect_t* ep = p->bp;
  while( ep!=NULL )
  {
    cmVectArrayVect_t* np = ep->link;

    cmMemFree(ep->u.v);
    cmMemFree(ep);
    
    ep = np;
  }

  p->bp        = NULL;
  p->ep        = NULL;
  p->maxEleCnt = 0;
  p->vectCnt   = 0;

  return cmOkRC;
}

unsigned cmVectArrayCount( const cmVectArray_t* p )
{ return p->vectCnt; }

/*
unsigned cmVectArrayEleCount( const cmVectArray_t* p )
{
  cmVectArrayVect_t* ep = p->bp;
  unsigned n = 0;
  for(; ep!=NULL; ep=ep->link )
    n += ep->n;

  return n;
}
*/


cmRC_t cmVectArrayAppendS( cmVectArray_t* p, const cmSample_t* v, unsigned vn )
{ return  _cmVectArrayAppend(p,v,sizeof(v[0]),vn); }

cmRC_t cmVectArrayAppendR( cmVectArray_t* p, const cmReal_t* v, unsigned vn )
{ return  _cmVectArrayAppend(p,v,sizeof(v[0]),vn); }

cmRC_t cmVectArrayAppendF( cmVectArray_t* p, const float* v,      unsigned vn )
{ return  _cmVectArrayAppend(p,v,sizeof(v[0]),vn); }

cmRC_t cmVectArrayAppendD( cmVectArray_t* p, const double* v,     unsigned vn )
{ return  _cmVectArrayAppend(p,v,sizeof(v[0]),vn); }

cmRC_t cmVectArrayAppendI( cmVectArray_t* p, const int* v, unsigned vn )
{
  p->tempV = cmMemResize(double,p->tempV,vn);
  unsigned i;
  for(i=0; i<vn; ++i)
    p->tempV[i] = v[i];

  cmRC_t rc = cmVectArrayAppendD(p,p->tempV,vn);

  return rc;  
}

cmRC_t cmVectArrayAppendU( cmVectArray_t* p, const unsigned* v, unsigned vn )
{
  p->tempV = cmMemResize(double,p->tempV,vn);
  unsigned i;
  for(i=0; i<vn; ++i)
    p->tempV[i] = v[i];

  cmRC_t rc = cmVectArrayAppendD(p,p->tempV,vn);
  return rc;  
}

cmRC_t cmVectArrayWrite( cmVectArray_t* p, const char* fn )
{
  cmRC_t             rc  = cmOkRC;
  FILE*              fp  = NULL;
  cmVectArrayVect_t* ep;
  unsigned           i;
  unsigned           hn  = 4;
  unsigned           hdr[hn];

  hdr[0] = p->flags;
  hdr[1] = p->typeByteCnt;
  hdr[2] = p->vectCnt;
  hdr[3] = p->maxEleCnt;

  // create the file
  if((fp = fopen(fn,"wb")) == NULL )
    return cmCtxRtCondition(&p->obj,cmSystemErrorRC,"The vector array file '%s' could not be created.",cmStringNullGuard(fn));

  // write the header
  if( fwrite(hdr,sizeof(unsigned),hn,fp) != hn )
  {
    rc = cmCtxRtCondition(&p->obj,cmSystemErrorRC,"Vector array file header write failed in '%s'.",cmStringNullGuard(fn));
    goto errLabel;
  }

  // write each vector element
  for(ep=p->bp,i=0; ep!=NULL; ep=ep->link,++i)
  {
    // write the count of data values in the vector
    if( fwrite(&ep->n,sizeof(ep->n),1,fp) != 1 )
    {
      rc = cmCtxRtCondition(&p->obj,cmSystemErrorRC,"Vector array file write failed on element header %i in '%s'.",i,cmStringNullGuard(fn));
      goto errLabel;
    }
      
    // write the vector
    if(fwrite(ep->u.v,p->typeByteCnt,ep->n,fp) != ep->n )
    {
      rc = cmCtxRtCondition(&p->obj,cmSystemErrorRC,"Vector array file write failed on data vector %i in '%s'.",i,cmStringNullGuard(fn));
      goto errLabel;
    }
  }
  

 errLabel:
  if( fp != NULL )
    fclose(fp);
  return rc;
}

unsigned cmVectArrayForEachS( cmVectArray_t* p, unsigned idx, unsigned cnt, cmVectArrayForEachFuncS_t func, void* arg )
{
  cmVectArrayVect_t* ep = p->bp;
  unsigned           i  = 0;
  unsigned           n  = 0;

  // for each sub-array 
  for(; ep!=NULL && n<cnt; ep=ep->link )
  {
    // if the cur sub-array is in the range of idx:idx+cnt
    if( i <= idx && idx < i + ep->n )
    {
      unsigned j = idx - i;            // starting idx into cur sub-array
      assert(j<ep->n);
      unsigned m = cmMin(ep->n - j,cnt-n); // cnt of ele's to send from cur sub-array

      // do callback
      if( func(arg, idx, ep->u.sV + j, m ) != cmOkRC )
        break;

      idx += m;
      n   += m;
    }

    i += ep->n;
    
  }
  return n;
}

cmRC_t cmVectArrayWriteVectorS( cmCtx* ctx, const char* fn, const cmSample_t* v, unsigned  vn )
{
  cmRC_t        rc = cmOkRC;
  cmVectArray_t* p;

  if((p = cmVectArrayAlloc(ctx,kSampleVaFl)) == NULL )
    return cmCtxRtCondition(&ctx->obj,cmSubSysFailRC,"Unable to allocate an cmVectArray_t in cmVectArrayWriteVectorS().");

  if((rc = cmVectArrayAppendS(p,v,vn)) != cmOkRC )
  {
    rc = cmCtxRtCondition(&p->obj,rc,"Vector append failed in cmVectArrayWriteVectorS().");
    goto errLabel;
  }
    
  if((rc = cmVectArrayWrite(p,fn)) != cmOkRC )
    rc = cmCtxRtCondition(&p->obj,rc,"Vector array write failed in cmVectArrayWriteVectorS().");

 errLabel:
  if((rc = cmVectArrayFree(&p)) != cmOkRC )
    rc = cmCtxRtCondition(&ctx->obj,rc,"Free failed on cmVectArrayFree() in cmVectArrayWriteVectorS().");

  return rc;
}

cmRC_t cmVectArrayWriteMatrixS( cmCtx* ctx, const char* fn, const cmSample_t* m, unsigned  rn, unsigned cn )
{
  cmRC_t        rc = cmOkRC;
  cmVectArray_t* p;
  unsigned       i;

  if((p = cmVectArrayAlloc(ctx,kSampleVaFl)) == NULL )
    return cmCtxRtCondition(&ctx->obj,cmSubSysFailRC,"Unable to allocate an cmVectArray_t in cmVectArrayWriteVectorS().");

  for(i=0; i<cn; ++i)
  {
    if((rc = cmVectArrayAppendS(p,m + (i*rn), rn)) != cmOkRC )
    {
      rc = cmCtxRtCondition(&p->obj,rc,"Vector append failed in cmVectArrayWriteVectorS().");
      goto errLabel;
    }
    
  }

  if((rc = cmVectArrayWrite(p,fn)) != cmOkRC )
    rc = cmCtxRtCondition(&p->obj,rc,"Vector array write failed in cmVectArrayWriteVectorS().");

 errLabel:
  if((rc = cmVectArrayFree(&p)) != cmOkRC )
    rc = cmCtxRtCondition(&ctx->obj,rc,"Free failed on cmVectArrayFree() in cmVectArrayWriteVectorS().");

  return rc;

}

cmRC_t cmVectArrayWriteMatrixR( cmCtx* ctx, const char* fn, const cmReal_t* m, unsigned  rn, unsigned cn )
{
  cmRC_t        rc = cmOkRC;
  cmVectArray_t* p;
  unsigned       i;

  if((p = cmVectArrayAlloc(ctx,kRealVaFl)) == NULL )
    return cmCtxRtCondition(&ctx->obj,cmSubSysFailRC,"Unable to allocate an cmVectArray_t in cmVectArrayWriteVectorR().");

  for(i=0; i<cn; ++i)
  {
    if((rc = cmVectArrayAppendR(p,m + (i*rn), rn)) != cmOkRC )
    {
      rc = cmCtxRtCondition(&p->obj,rc,"Vector append failed in cmVectArrayWriteVectorR().");
      goto errLabel;
    }
    
  }

  if((rc = cmVectArrayWrite(p,fn)) != cmOkRC )
    rc = cmCtxRtCondition(&p->obj,rc,"Vector array write failed in cmVectArrayWriteVectorR().");

 errLabel:
  if((rc = cmVectArrayFree(&p)) != cmOkRC )
    rc = cmCtxRtCondition(&ctx->obj,rc,"Free failed on cmVectArrayFree() in cmVectArrayWriteVectorR().");

  return rc;

}

cmRC_t cmVectArrayWriteVectorI( cmCtx* ctx, const char* fn, const int* v,        unsigned  vn )
{
  cmRC_t        rc = cmOkRC;
  cmVectArray_t* p;

  if((p = cmVectArrayAlloc(ctx,kIntVaFl)) == NULL )
    return cmCtxRtCondition(&ctx->obj,cmSubSysFailRC,"Unable to allocate an cmVectArray_t in cmVectArrayWriteVectorS().");

  if((rc = cmVectArrayAppendI(p,v,vn)) != cmOkRC )
  {
    rc = cmCtxRtCondition(&p->obj,rc,"Vector append failed in cmVectArrayWriteVectorS().");
    goto errLabel;
  }
    
  if((rc = cmVectArrayWrite(p,fn)) != cmOkRC )
    rc = cmCtxRtCondition(&p->obj,rc,"Vector array write failed in cmVectArrayWriteVectorS().");

  errLabel:
  if((rc = cmVectArrayFree(&p)) != cmOkRC )
    rc = cmCtxRtCondition(&ctx->obj,rc,"Free failed on cmVectArrayFree() in cmVectArrayWriteVectorS().");

  return rc;
}


cmRC_t cmVectArrayWriteMatrixI( cmCtx* ctx, const char* fn, const int* m, unsigned  rn, unsigned cn )
{
  cmRC_t         rc = cmOkRC;
  cmVectArray_t* p;
  unsigned       i;

  if((p = cmVectArrayAlloc(ctx,kIntVaFl)) == NULL )
    return cmCtxRtCondition(&ctx->obj,cmSubSysFailRC,"Unable to allocate an cmVectArray_t in cmVectArrayWriteVectorI().");

  for(i=0; i<cn; ++i)
  {
    if((rc = cmVectArrayAppendI(p,m + (i*rn), rn)) != cmOkRC )
    {
      rc = cmCtxRtCondition(&p->obj,rc,"Vector append failed in cmVectArrayWriteVectorI().");
      goto errLabel;
    }
    
  }

  if((rc = cmVectArrayWrite(p,fn)) != cmOkRC )
    rc = cmCtxRtCondition(&p->obj,rc,"Vector array write failed in cmVectArrayWriteVectorI().");

 errLabel:
  if((rc = cmVectArrayFree(&p)) != cmOkRC )
    rc = cmCtxRtCondition(&ctx->obj,rc,"Free failed on cmVectArrayFree() in cmVectArrayWriteVectorI().");

  return rc;

}


cmRC_t cmVectArrayForEachTextFuncS( void* arg, unsigned idx, const cmSample_t* xV, unsigned xN )
{
  assert(0);
  return cmOkRC;
}


cmRC_t cmVectArrayRewind( cmVectArray_t* p )
{
  p->cur = p->bp;
  return cmOkRC;
}

cmRC_t cmVectArrayAdvance( cmVectArray_t* p, unsigned n )
{
  unsigned i;
  for(i=0; i<n; ++i)
  {
    if( p->cur == NULL )
      break;

    p->cur = p->cur->link;
  }
      
  return cmOkRC;
  
}

bool   cmVectArrayIsEOL( const cmVectArray_t* p )
{ return p->cur == NULL; }


unsigned cmVectArrayEleCount(   const cmVectArray_t* p )
{
  if( p->cur == NULL )
    return 0;
  return p->cur->n;
}



cmRC_t cmVectArrayGetF( cmVectArray_t* p, float* v, unsigned* vnRef )
{
  assert( cmIsFlag(p->flags,kFloatVaFl) );

  if( cmVectArrayIsEOL(p) )
    return cmCtxRtCondition(&p->obj,cmSubSysFailRC,"VectArray get failed because the state is EOL.");

  unsigned n = cmMin(*vnRef,p->cur->n);

  cmVOF_Copy(v,n,p->cur->u.fV);

  *vnRef = n;

  return cmOkRC;
}

cmRC_t cmVectArrayGetD( cmVectArray_t* p, double* v, unsigned* vnRef )
{
  assert( cmIsFlag(p->flags,kDoubleVaFl) );

  if( cmVectArrayIsEOL(p) )
    return cmCtxRtCondition(&p->obj,cmSubSysFailRC,"VectArray get failed because the state is EOL.");

  unsigned n = cmMin(*vnRef,p->cur->n);

  cmVOD_Copy(v,n,p->cur->u.dV);

  *vnRef = n;

  return cmOkRC;
}

cmRC_t cmVectArrayGetI( cmVectArray_t* p, int* v, unsigned* vnRef )
{
  if( cmVectArrayIsEOL(p) )
    return cmCtxRtCondition(&p->obj,cmSubSysFailRC,"VectArray get failed because the state is EOL.");

  unsigned n = cmMin(*vnRef,p->cur->n);
  unsigned i;

  if( cmIsFlag(p->flags,kDoubleVaFl) )
  {
    for(i=0; i<n; ++i)
      v[i] = (int)(p->cur->u.dV[i]);
  }
  else
  {
    if( cmIsFlag(p->flags,kFloatVaFl) )
    {
      for(i=0; i<n; ++i)
        v[i] = (int)(p->cur->u.fV[i]);
    }
    else
    {
      assert(0);
    }
  }

  *vnRef = n;

  return cmOkRC;
}

cmRC_t cmVectArrayGetU( cmVectArray_t* p, unsigned* v, unsigned* vnRef )
{
  if( cmVectArrayIsEOL(p) )
    return cmCtxRtCondition(&p->obj,cmSubSysFailRC,"VectArray get failed because the state is EOL.");

  unsigned n = cmMin(*vnRef,p->cur->n);
  unsigned i;

  if( cmIsFlag(p->flags,kDoubleVaFl) )
  {
    for(i=0; i<n; ++i)
      v[i] = (unsigned)(p->cur->u.dV[i]);
  }
  else
  {
    if( cmIsFlag(p->flags,kFloatVaFl) )
    {
      for(i=0; i<n; ++i)
        v[i] = (unsigned)(p->cur->u.fV[i]);
    }
    else
    {
      assert(0);     
    }
  }

  *vnRef = n;

  return cmOkRC;
}

unsigned cmVectArrayVectEleCount( cmVectArray_t* p, unsigned groupIdx, unsigned groupCnt )
{
  unsigned n = 0;

  cmVectArrayVect_t* pos = p->cur;

  if( cmVectArrayRewind(p) != cmOkRC )
    goto errLabel;
  
  if( cmVectArrayAdvance(p,groupIdx) != cmOkRC )
    goto errLabel;

  while( !cmVectArrayIsEOL(p) )
  {
    n += cmVectArrayEleCount(p);
    
    if(cmVectArrayAdvance(p,groupCnt) != cmOkRC )
      goto errLabel;
  }

 errLabel:
  p->cur = pos;

  return n;
  
}

cmRC_t   cmVectArrayFormVectF( cmVectArray_t* p, unsigned groupIdx, unsigned groupCnt, float** vRef, unsigned* vnRef )
{
  cmRC_t rc = cmOkRC;

  *vRef = NULL;
  *vnRef = 0;

  unsigned N = cmVectArrayVectEleCount(p,groupIdx,groupCnt);

  if( N == 0 )
    return rc;

  float*             v   = cmMemAllocZ(float,N);
  unsigned           i   = 0;
  cmVectArrayVect_t* pos = p->cur;

  if( cmVectArrayRewind(p) != cmOkRC )
    goto errLabel;
  
  if( cmVectArrayAdvance(p,groupIdx) != cmOkRC )
    goto errLabel;

  while( !cmVectArrayIsEOL(p) )
  {
    unsigned n = cmVectArrayEleCount(p);

    assert(i+n <= N);

    cmVectArrayGetF(p,v+i,&n);

    i += n;

    cmVectArrayAdvance(p,groupCnt);
  }

  *vRef = v;
  *vnRef = i;

 errLabel:

  p->cur = pos;

  return rc;
}

cmRC_t   cmVectArrayFormVectColF( cmVectArray_t* p, unsigned groupIdx, unsigned groupCnt, unsigned colIdx, float** vRef, unsigned* vnRef )
{
  cmRC_t rc = cmOkRC;

  *vRef = NULL;
  *vnRef = 0;

  // assume there will be one output element for each group
  unsigned N = cmVectArrayCount(p)/groupCnt + 1;

  if( N == 0 )
    return rc;

  float*             v   = cmMemAllocZ(float,N);
  unsigned           i   = 0;
  cmVectArrayVect_t* pos = p->cur;

  if( cmVectArrayRewind(p) != cmOkRC )
    goto errLabel;
  
  if( cmVectArrayAdvance(p,groupIdx) != cmOkRC )
    goto errLabel;

  while( i<N && !cmVectArrayIsEOL(p) )
  {
    unsigned tn = cmVectArrayEleCount(p);
    float   tv[tn];

    // read the sub-vector
    cmVectArrayGetF(p,tv,&tn);

    // store the output value
    if( colIdx < tn )
    {
      v[i] = tv[colIdx];

      i += 1;
    }

    cmVectArrayAdvance(p,groupCnt);
  }

  *vRef = v;
  *vnRef = i;

 errLabel:

  p->cur = pos;

  return rc;
}


cmRC_t   cmVectArrayFormVectColU( cmVectArray_t* p, unsigned groupIdx, unsigned groupCnt, unsigned colIdx, unsigned** vRef, unsigned* vnRef )
{
  cmRC_t rc = cmOkRC;

  *vRef = NULL;
  *vnRef = 0;

  // assume there will be one output element for each group
  unsigned N = cmVectArrayCount(p)/groupCnt + 1;

  if( N == 0 )
    return rc;

  unsigned*          v   = cmMemAllocZ(unsigned,N);
  unsigned           i   = 0;
  cmVectArrayVect_t* pos = p->cur;

  if( cmVectArrayRewind(p) != cmOkRC )
    goto errLabel;
  
  if( cmVectArrayAdvance(p,groupIdx) != cmOkRC )
    goto errLabel;

  while( i<N && !cmVectArrayIsEOL(p) )
  {
    unsigned tn = cmVectArrayEleCount(p);
    unsigned tv[tn];

    // read the sub-vector
    cmVectArrayGetU(p,tv,&tn);

    assert( colIdx < tn );

    // store the output value
    if( colIdx < tn )
      v[i++] = tv[colIdx];

    cmVectArrayAdvance(p,groupCnt);
  }

  *vRef = v;
  *vnRef = i;

 errLabel:

  p->cur = pos;

  return rc;
}

cmRC_t cmVectArrayTest( cmCtx* ctx, const char* fn )
{
  cmRC_t         rc    = cmOkRC;
  cmVectArray_t* p     = NULL;
  unsigned       flags = kSampleVaFl;
  cmSample_t     v[]   = { 0, 1, 2, 3, 4, 5 };

  if( fn == NULL || strlen(fn)==0 )
    return cmCtxRtCondition(&p->obj,cmSubSysFailRC,"Invalid test output file name.");

  if( (p = cmVectArrayAlloc(ctx,flags)) == NULL )
    return cmCtxRtCondition(&p->obj,cmSubSysFailRC,"The vectory array object allocation failed.");
  
  if( cmVectArrayAppendS(p,v,1) != cmOkRC )
  {
    rc = cmCtxRtCondition(&p->obj,cmSubSysFailRC,"Vector append 1 failed.");
    goto errLabel;
  }

  if( cmVectArrayAppendS(p,v+1,2) != cmOkRC )
  {
    rc = cmCtxRtCondition(&p->obj,cmSubSysFailRC,"Vector append 2 failed.");
    goto errLabel;
  }

  if( cmVectArrayAppendS(p,v+3,3) != cmOkRC )
  {
    rc = cmCtxRtCondition(&p->obj,cmSubSysFailRC,"Vector append 3 failed.");
    goto errLabel;
  }
  
  if( cmVectArrayWrite(p,fn) != cmOkRC )
  {
    rc = cmCtxRtCondition(&p->obj,cmSubSysFailRC,"Vector array write failed.");
    goto errLabel;
  }

  //cmVectArrayForEachS(p,0,cmVectArrayEleCount(p),cmVectArrayForEachTextFuncS,&ctx->printRpt);
  
  if( cmVectArrayFree(&p) != cmOkRC )
  {
    rc = cmCtxRtCondition(&p->obj,cmSubSysFailRC,"The vectory array release failed.");
    goto errLabel;
  }


  if((p = cmVectArrayAllocFromFile(ctx, fn )) == NULL )
  {
    rc = cmCtxRtCondition(&p->obj,cmSubSysFailRC,"VectArray alloc from file failed.");
    goto errLabel;
  }

  
  while(!cmVectArrayIsEOL(p))
  {
    unsigned n = cmVectArrayEleCount(p);
    cmSample_t v[n];
    

    if( cmVectArrayGetS(p,v,&n) != cmOkRC )
    {
      rc = cmCtxRtCondition(&p->obj,cmSubSysFailRC,"VectArrayGetS() failed.");
      goto errLabel;
    }

    cmVOS_PrintL("v:",NULL,1,n,v);
    
    cmVectArrayAdvance(p,1);
  }
  

 errLabel:
  if( cmVectArrayFree(&p) != cmOkRC )
    rc = cmCtxRtCondition(&p->obj,cmSubSysFailRC,"The vectory array release failed.");

  return rc;
}


//-----------------------------------------------------------------------------------------------------------------------

cmWhFilt* cmWhFiltAlloc( cmCtx* c, cmWhFilt* p, unsigned binCnt, cmReal_t binHz, cmReal_t coeff, cmReal_t maxHz )
{
  cmWhFilt* op = cmObjAlloc(cmWhFilt,c,p);

  if( binCnt > 0 )
    if( cmWhFiltInit(op,binCnt,binHz,coeff,maxHz) != cmOkRC )
      cmWhFiltFree(&op);

  return op;

}

cmRC_t    cmWhFiltFree( cmWhFilt** pp )
{
  cmRC_t rc = cmOkRC;

  if( pp==NULL || *pp==NULL )
    return rc;

  cmWhFilt* p = *pp;
  if((rc = cmWhFiltFinal(p)) != cmOkRC )
    return rc;

  cmMemFree(p->whM);
  cmMemFree(p->whiV);
  cmMemFree(p->iV);
  cmObjFree(pp);
  return rc;

}

cmRC_t    cmWhFiltInit( cmWhFilt* p, unsigned binCnt, cmReal_t binHz, cmReal_t coeff, cmReal_t maxHz )
{
  cmRC_t rc;
  if((rc = cmWhFiltFinal(p)) != cmOkRC )
    return rc;

  p->binCnt = binCnt;
  p->binHz  = binHz;
  p->bandCnt = maxHz == 0 ? 34 : ceil(log10(maxHz/229.0 + 1) * 21.4 - 1)-1;

  if( p->bandCnt <= 0 )
    return cmCtxRtCondition(&p->obj, cmInvalidArgRC, "Max. Hz too low to form any frequency bands.");

  cmReal_t flV[ p->bandCnt ];
  cmReal_t fcV[ p->bandCnt ];
  cmReal_t fhV[ p->bandCnt ];
  int i;

  for(i=0; i<p->bandCnt; ++i)
  {
    fcV[i] = 229.0 * (pow(10.0,(i+2)/21.4) - 1.0);
    flV[i] = i==0 ? 0 : fcV[i-1];
  }

  for(i=0; i<p->bandCnt-1; ++i)
    fhV[i] = fcV[i+1];

  fhV[p->bandCnt-1] = fcV[p->bandCnt-1] + (fcV[p->bandCnt-1] - fcV[p->bandCnt-2]);

  //cmVOR_PrintL("flV",NULL,1,p->bandCnt,flV);
  //cmVOR_PrintL("fcV",NULL,1,p->bandCnt,fcV);
  //cmVOR_PrintL("fhV",NULL,1,p->bandCnt,fhV);

  cmReal_t* tM  = cmMemAlloc(cmReal_t,p->bandCnt * p->binCnt);
  p->whM = cmMemResizeZ(cmReal_t,p->whM,p->binCnt * p->bandCnt);
  p->iV  = cmMemResizeZ(cmReal_t,p->iV,p->binCnt);

  // generate the bin index values
  for(i=0; i<p->binCnt; ++i)
    p->iV[i] = i;

  cmReal_t stSpread = 0; // set stSpread to 0 to use flV/fhV[]
  cmVOR_TriangleMask(tM, p->bandCnt, p->binCnt, fcV, p->binHz, stSpread, flV, fhV );
  cmVOR_Transpose(p->whM, tM, p->bandCnt, p->binCnt );
  cmMemFree(tM);

  //cmVOR_PrintL("whM",NULL,p->bandCnt,p->binCnt,p->whM);
  //cmVectArrayWriteMatrixR(p->obj.ctx, "/home/kevin/temp/frqtrk/whM.va", p->whM, p->binCnt, p->bandCnt );

  unsigned whiN = p->bandCnt+2;
  p->whiV = cmMemResizeZ(cmReal_t,p->whiV,whiN);

  for(i=0; i<whiN; ++i)
  {
    if( i == 0 )
      p->whiV[i] = 0; 
    else
      if( i == whiN-1 )
        p->whiV[i] = fhV[p->bandCnt-1]/binHz;
      else
        p->whiV[i] = fcV[i-1]/binHz;
  }

  //cmVOR_PrintL("whiV",NULL,1,whiN,p->whiV);
  //cmVectArrayWriteMatrixR(p->obj.ctx, "/home/kevin/temp/frqtrk/whiV.va", p->whiV, whiN, 1 );
  
  return rc;
}

cmRC_t    cmWhFiltFinal( cmWhFilt* p )
{ return cmOkRC; }

cmRC_t    cmWhFiltExec( cmWhFilt* p, const cmReal_t* xV, cmReal_t* yV, unsigned xyN )
{
  assert( xyN == p->binCnt);

  cmRC_t   rc   = cmOkRC;
  unsigned whiN = p->bandCnt + 2;

  unsigned mbi = cmMin(xyN, floor(p->whiV[whiN-1]));
  
  // calculate the level in each band to form a composite filter
  cmReal_t  y0V[ whiN ];
  cmReal_t* b0V = y0V + 1;
  cmVOR_MultVVM(b0V, p->bandCnt, xV, p->binCnt, p->whM );

  //cmVOR_PrintL("b0V",NULL,1,p->bandCnt,b0V);

  // BEWARE: zeros in b0V will generate Inf's when sent 
  // through the cmVOR_PowVS() function.
  int i;
  for(i=0; i<p->bandCnt; ++i)
    if( b0V[i] < 0.000001 )
      b0V[i] =  0.000001;

  // apply a non-linear expansion function to each band
  cmVOR_PowVS(b0V,p->bandCnt,p->coeff-1);

  //cmVOR_PrintL("b0V",NULL,1,p->bandCnt,b0V);

  // add edge values to the filter
  y0V[0]      = b0V[0];
  y0V[whiN-1] = b0V[p->bandCnt-1];
  
  //cmVOR_PrintL("y0V",NULL,1,whiN,y0V);

  cmVOR_Interp1(yV,p->iV,p->binCnt,p->whiV,y0V,whiN);

  cmVOR_Fill(yV+mbi,xyN-mbi,1.0);

  //cmVOR_PrintL("yV",NULL,1,p->binCnt,yV);

  cmVOR_MultVV(yV,xyN,xV);

  return rc;
}

//-----------------------------------------------------------------------------------------------------------------------

cmFrqTrk* cmFrqTrkAlloc( cmCtx* c, cmFrqTrk* p, const cmFrqTrkArgs_t* a )
{
  cmFrqTrk* op = cmObjAlloc(cmFrqTrk,c,p);

  op->logVa   = cmVectArrayAlloc(c,kRealVaFl);
  op->levelVa = cmVectArrayAlloc(c,kRealVaFl);
  op->specVa  = cmVectArrayAlloc(c,kRealVaFl);
  op->attenVa = cmVectArrayAlloc(c,kRealVaFl);

  op->wf  = cmWhFiltAlloc(c,NULL,0,0,0,0);

  if( a != NULL )
    if( cmFrqTrkInit(op,a) != cmOkRC )
      cmFrqTrkFree(&op);

  return op;

}

cmRC_t    cmFrqTrkFree( cmFrqTrk** pp )
{
  cmRC_t rc = cmOkRC;

  if( pp==NULL || *pp==NULL )
    return rc;

  cmFrqTrk* p = *pp;
  if((rc = cmFrqTrkFinal(p)) != cmOkRC )
    return rc;

  unsigned i;
  for(i=0; i<p->a.chCnt; ++i)
  {
    cmMemFree(p->ch[i].dbV);
    cmMemFree(p->ch[i].hzV);
  }

  cmMemFree(p->ch);
  cmMemFree(p->dbM);
  cmMemFree(p->pkiV);
  cmMemFree(p->dbV);
  cmMemFree(p->aV);
  cmVectArrayFree(&p->logVa);
  cmVectArrayFree(&p->levelVa);
  cmVectArrayFree(&p->specVa);
  cmVectArrayFree(&p->attenVa);
  cmWhFiltFree(&p->wf);
  cmMemFree(p->logFn);
  cmMemFree(p->levelFn);
  cmMemFree(p->specFn);
  cmObjFree(pp);
  return rc;

}

cmRC_t    cmFrqTrkInit( cmFrqTrk* p, const cmFrqTrkArgs_t* a )
{
  cmRC_t rc;
  if((rc = cmFrqTrkFinal(p)) != cmOkRC )
    return rc;

  p->a         = *a;
  p->ch        = cmMemResizeZ(cmFrqTrkCh_t,p->ch,a->chCnt );
  p->hN        = cmMax(1,a->wndSecs * a->srate / a->hopSmpCnt );
  p->sN        = 4*p->hN;
  p->binHz       = a->srate / ((p->a.binCnt-1)*2);
  p->bN        = cmMin( p->a.binCnt, ceil(p->a.pkMaxHz / p->binHz ));
  p->dbM       = cmMemResizeZ(cmReal_t,p->dbM,p->hN*p->bN);
  p->hi        = 0;
  p->fN        = 0;
  p->dbV       = cmMemResizeZ(cmReal_t,p->dbV,p->bN);
  p->pkiV      = cmMemResizeZ(unsigned,p->pkiV,p->bN);
  p->deadN_max = a->maxTrkDeadSec * a->srate / a->hopSmpCnt;
  p->minTrkN   = a->minTrkSec * a->srate / a->hopSmpCnt;
  p->nextTrkId = 1;
  p->aV        = cmMemResizeZ(cmReal_t,p->aV,p->a.binCnt);
  p->attenDlyPhsMax = cmMax(3,a->attenDlySec * a->srate / a->hopSmpCnt );
  p->attenPhsMax    = cmMax(3,a->attenAtkSec * a->srate / a->hopSmpCnt );
  
  if( a->logFn != NULL )
    p->logFn = cmMemResizeStr(p->logFn,a->logFn);

  if( a->levelFn != NULL )
    p->levelFn = cmMemResizeStr(p->levelFn,a->levelFn);

  if( a->specFn != NULL )
    p->specFn = cmMemResizeStr(p->specFn,a->specFn);

  if( a->attenFn != NULL )
    p->attenFn = cmMemResizeStr(p->attenFn,a->attenFn);


  if(cmWhFiltInit(p->wf,p->bN,p->binHz,p->a.whFiltCoeff,p->a.pkMaxHz) != cmOkRC )
    cmCtxRtCondition(&p->obj, cmSubSysFailRC, "Whitening filter intitialization failed.");

  unsigned i;
  for(i=0; i<p->a.chCnt; ++i)
  {
    p->ch[i].dbV = cmMemResizeZ(cmReal_t,p->ch[i].dbV,p->sN);
    p->ch[i].hzV = cmMemResizeZ(cmReal_t,p->ch[i].hzV,p->sN);
  }

  return rc;
}

cmRC_t    cmFrqTrkFinal( cmFrqTrk* p )
{
  cmRC_t   rc = cmOkRC;

  if( p->logFn != NULL )
    cmVectArrayWrite(p->logVa,p->logFn);

  if( p->levelFn != NULL )
    cmVectArrayWrite(p->levelVa,p->levelFn);

  if( p->specFn != NULL )
    cmVectArrayWrite(p->specVa,p->specFn);

  if( p->attenFn != NULL )
    cmVectArrayWrite(p->attenVa,p->attenFn);

  cmWhFiltFinal(p->wf);
  return rc;
}

// Return an available channel record or NULL if all channel records are in use.
cmFrqTrkCh_t* _cmFrqTrkFindAvailCh( cmFrqTrk* p )
{
  unsigned i;
  for(i=0; i<p->a.chCnt; ++i)
    if( p->ch[i].activeFl == false )
      return p->ch + i;

  return NULL;
}


// Estimate the peak frequency by parabolic interpolotion into hzV[p->bN]
void _cmFrqTrkMagnToHz( cmFrqTrk* p, const cmReal_t* dbV, unsigned* pkiV, unsigned pkN, cmReal_t* hzV )
{
  unsigned i;
  for(i=0; i<pkN; ++i)
    if( pkiV[i] != cmInvalidIdx )
    {
      unsigned pki  = pkiV[i];
      cmReal_t y0   = pki>0             ? dbV[ pki-1 ] : dbV[pki];
      cmReal_t y1   =                     dbV[ pki   ];
      cmReal_t y2   = pki<p->bN-1       ? dbV[ pki+1 ] : dbV[pki];
      cmReal_t den  = y0 - (2.*y1) + y2;
      cmReal_t offs = den==0 ? 0 : 0.5 * ((y0 - y2) / den);
      hzV[pki] = p->binHz * (pki+offs);

      //if( hzV[pki] < 0 )
      //{
      //  printf("%f : %f %f %f : %f %f\n",hzV[pki],y0,y1,y2,den,offs);
      //}
    }
}

unsigned _cmFrqTrkActiveChCount( cmFrqTrk* p )
{
  unsigned n = 0;
  unsigned i;
  for(i=0; i<p->a.chCnt; ++i)
    if( p->ch[i].activeFl )
      ++n;

  return n;
}

void _cmFrqTrkWriteLevel( cmFrqTrk* p, const cmReal_t* dbV, const cmReal_t* hzV, unsigned bN )
{
  if( p->levelFn != NULL )
  {
    double          maxHz     = 5000.0;
    unsigned        maxBinIdx = cmMin(bN,maxHz / p->binHz);
    unsigned        vn        = 3;
    cmReal_t        v[vn];
    unsigned        idx       = cmVOR_MaxIndex(dbV,maxBinIdx,1);
    v[0] = cmVOR_Mean(dbV,maxBinIdx);
    v[1] = dbV[idx];
    v[2] = hzV[idx];
  
    cmVectArrayAppendR(p->levelVa,v,vn);
  }

}

void _cmFrqTrkWriteLog( cmFrqTrk* p )
{
  unsigned n;
  cmReal_t* vb = NULL;

  if( p->logFn == NULL )
    return;

  if((n = _cmFrqTrkActiveChCount(p)) > 0 )
  {
    unsigned  i,j;

    // sn = count of elements in the summary sub-vector
    unsigned  sn  = 3;

    // each active channel will emit 7 values
    unsigned  nn  = 1 + n*7 + sn;

    // allocate the row vector
    vb   = cmMemResize(cmReal_t,vb,nn);

    // row format
    // [ nn idV[n] hzV[n] ... hsV[n] smV[sn]  ]
    // n = (nn - (1 + sn)) / 7

    *vb = nn; // the first element in the vector contains the length of the row

    cmReal_t* v    = vb + 1;

    // setup the base pointer to each sub-vector
    cmReal_t* idV = v + n * 0;
    cmReal_t* hzV = v + n * 1;
    cmReal_t* dbV = v + n * 2;
    cmReal_t* stV = v + n * 3;
    cmReal_t* dsV = v + n * 4;
    cmReal_t* hsV = v + n * 5;
    cmReal_t* agV = v + n * 6;
    cmReal_t* smV = v + n * 7; // summary information

    smV[0] = p->newTrkCnt;
    smV[1] = p->curTrkCnt;
    smV[2] = p->deadTrkCnt;

    // for each active channel
    for(i=0,j=0; i<p->a.chCnt; ++i)
      if( p->ch[i].activeFl )
      {
        assert(j < n);

        // elements of each sub-vector associated with a given
        // index refer to the same track record - element i therefore
        // refers to active track index i.
        idV[j] = p->ch[i].id;
        hzV[j] = p->ch[i].hz;
        dbV[j] = p->ch[i].db;
        stV[j] = p->ch[i].dN;
        dsV[j] = p->ch[i].db_std;
        hsV[j] = p->ch[i].hz_std;
        agV[j] = p->ch[i].attenGain;
        ++j;
      }
    
    cmVectArrayAppendR(p->logVa, vb, nn );
  }

  cmMemFree(vb);

}

void _cmFrqTrkPrintChs( const cmFrqTrk* p )
{
  unsigned i;
  for(i=0; i<p->a.chCnt; ++i)
  {
    cmFrqTrkCh_t* c = p->ch + i;
    printf("%i : %i tN:%i hz:%f db:%f\n",i,c->activeFl,c->tN,c->hz,c->db);
  }
}

// Used to sort the channels into descending dB order.
int _cmFrqTrkChCompare( const void* p0, const void* p1 )
{  return ((cmFrqTrkCh_t*)p0)->db - ((cmFrqTrkCh_t*)p1)->db; }


// Return the index of the peak associated with pkiV[i] which best matches the tracker 'c'
// or cmInvalidIdx if no valid peaks were found.
// pkiV[ pkN ] holds the indexes into dbV[] and hzV[] which are peaks.
// Some elements of pkiV[] may be set to cmInvalidIdx if the associated peak has already
// been selected by another tracker.
unsigned _cmFrqTrkFindPeak( cmFrqTrk* p, const cmFrqTrkCh_t* c, const cmReal_t* dbV, const cmReal_t* hzV, unsigned* pkiV, unsigned pkN )
{
  unsigned i,pki;
  cmReal_t d_max = p->a.pkThreshDb;
  unsigned d_idx = cmInvalidIdx;

  cmReal_t hz_min = c->hz * pow(2,-p->a.stRange/12.0);
  cmReal_t hz_max = c->hz * pow(2, p->a.stRange/12.0);

  // find the peak with the most energy inside the frequency range hz_min to hz_max.
  for(i=0; i<pkN; ++i)
    if( ((pki = pkiV[i]) != cmInvalidIdx) && hz_min <= hzV[pki] && hzV[pki] <= hz_max && dbV[pki]>d_max )      
    {
      d_max= dbV[pki];
      d_idx = i;      
    }

  return d_idx;
}

void _cmFrqTrkScoreChs( cmFrqTrk* p )
{
  unsigned i;
  for(i=0; i<p->a.chCnt; ++i)
    if( p->ch[i].activeFl )
    {
      cmFrqTrkCh_t* c = p->ch + i;

      c->dbV[ c->si ] = c->db;
      c->hzV[ c->si ] = c->hz;
      c->si  = (c->si + 1) % p->sN;
      c->sn += 1;

      unsigned n = cmMin(c->sn,p->sN);

      c->db_mean = cmVOR_Mean(c->dbV,n);
      c->db_std  = sqrt(cmVOR_Variance( c->dbV,n,&c->db_mean));
      c->hz_mean = cmVOR_Mean(c->hzV,n);
      c->hz_std  = sqrt(cmVOR_Variance( c->hzV,n,&c->hz_mean));

      //c->score = c->db / ((cmMax(0.1,c->db_std) + cmMax(0.1,c->hz_std))/2);

      c->score = c->db - (c->db_std * 5) - (c->hz_std/50);

      //printf("%f %f %f %f %f\n",c->db,cmMin(0.1,c->db_std),c->hz,cmMin(0.1,c->hz_std),c->score);
    }
}

// Generate a filter that is wider for higher frequencies than lower frequencies.
unsigned _cmFrqTrkFillMap( cmFrqTrk* p, cmReal_t* map, unsigned maxN, cmReal_t hz )
{
  assert( maxN % 2 == 1 );

  unsigned i;
  cmReal_t maxHz = p->a.srate/2;
  unsigned mapN  = cmMin(maxN,ceil(hz/maxHz * maxN));

  if( mapN % 2 == 0 )
    mapN += 1;

  mapN = cmMin(maxN,mapN);

  unsigned   N = floor(mapN/2);

  double COEFF = 0.3;

  for(i=0; i<N; ++i)
  {
    map[i] = pow(((double)i+1)/(N+1),COEFF);
    map[mapN-(i+1)] = map[i];
  }

  map[N] = 1.0;
  return mapN;
}

void _cmFrqTrkApplyAtten( cmFrqTrk* p, cmReal_t* aV, cmReal_t gain, cmReal_t hz )
{
  int       cbi = cmMin(p->a.binCnt,cmMax(0,round(hz/p->binHz)));
  //cmReal_t  map[] = { .25, .5, 1, .5, .25 };
  //int       mapN = sizeof(map)/sizeof(map[0]);

  unsigned  maxN = 30;  // must be odd
  cmReal_t  map[ maxN ];
  int       mapN = _cmFrqTrkFillMap(p, map, maxN, hz );
  int       j;

  int ai = cbi - mapN/2;

  for(j=0; j<mapN; ++j,++ai)
    if( 0 <= ai && ai < p->a.binCnt )
      aV[ai] *=  1.0 - (map[j] * gain);
    
}

void _cmFrqTrkUpdateFilter( cmFrqTrk* p )
{
  unsigned i;
  cmVOR_Fill(p->aV,p->a.binCnt,1.0);
  
  for(i=0; i<p->a.chCnt; ++i)
    if( p->ch[i].activeFl )
    {
      cmFrqTrkCh_t* c = p->ch + i;
      // 
      if( c->score >= p->a.attenThresh && c->state == kNoStateFrqTrkId )
      {
        //printf("%f\n",c->score);
        c->attenPhsIdx = 0;
        c->state       = kDlyFrqTrkId;        
      }

      switch( c->state )
      {
        case kNoStateFrqTrkId:
          break;

        case kDlyFrqTrkId:
          c->attenPhsIdx += 1;

          if( c->attenPhsIdx >= p->attenDlyPhsMax && c->dN == 0 )
            c->state = kAtkFrqTrkId;          

          break;

        case kAtkFrqTrkId:
          if( c->attenPhsIdx < p->attenDlyPhsMax + p->attenPhsMax )
          {

            c->attenGain = cmMin(1.0,p->a.attenGain * c->attenPhsIdx / p->attenPhsMax);

            _cmFrqTrkApplyAtten(p, p->aV, c->attenGain, c->hz);
          }

          c->attenPhsIdx += 1;
          if( c->attenPhsIdx >= p->attenDlyPhsMax + p->attenPhsMax )
            c->state = kSusFrqTrkId;
          break;

        case kSusFrqTrkId:

          if( c->dN > 0 )
          {
            if( c->attenPhsIdx > 0 )
            {
              c->attenPhsIdx -= 1;
              c->attenGain = cmMin(1.0,p->a.attenGain * c->attenPhsIdx / p->attenPhsMax);
            }
          }

          _cmFrqTrkApplyAtten(p,p->aV, c->attenGain, c->hz);   

          if( c->dN >= p->deadN_max )
            c->state = kDcyFrqTrkId;

          break;

        case kDcyFrqTrkId:
          if( c->attenPhsIdx > 0 )
          {
            c->attenPhsIdx -= 1;
            c->attenGain = cmMin(1.0,p->a.attenGain * c->attenPhsIdx / p->attenPhsMax);
            _cmFrqTrkApplyAtten(p,p->aV, c->attenGain, c->hz);   
          }          
          
          if( c->attenPhsIdx == 0 )
            c->activeFl = false;

          break;
      }
    }
  

}

// Extend the existing trackers
void _cmFrqTrkExtendChs( cmFrqTrk* p, const cmReal_t* dbV, const cmReal_t* hzV, unsigned* pkiV, unsigned pkN )
{
  unsigned i; 

  p->curTrkCnt = 0;
  p->deadTrkCnt = 0;

  // sort the channels in descending order
  qsort(p->ch,p->a.chCnt,sizeof(cmFrqTrkCh_t),_cmFrqTrkChCompare);

  // for each active channel
  for(i=0; i<p->a.chCnt; ++i)
  {
    cmFrqTrkCh_t* c = p->ch + i;

    if( c->activeFl )
    {
      unsigned pki;

      // find the best peak to extend tracker 'c'.
      if((pki = _cmFrqTrkFindPeak(p,c,dbV,hzV,pkiV,pkN)) == cmInvalidIdx )
      {
        // no valid track was found to extend tracker 'c'
        c->dN += 1;
        c->tN += 1;

        if( c->dN >= p->deadN_max )
        {
          if( c->attenPhsIdx == 0 )
            c->activeFl = false;

          p->deadTrkCnt += 1;
        }
      }
      else // ... update the tracker using the matching peak
      {
        unsigned j = pkiV[pki];
        c->dN = 0;
        c->db = dbV[ j ];
        c->hz = hzV[ j ];
        c->tN += 1;
        pkiV[pki] = cmInvalidIdx;  // mark the peak as unavailable.
        p->curTrkCnt += 1;
      }
    }
  }
}

// disable peaks which are within 'stRange' semitones of the frequency of active trackers.
void _cmFrqTrkDisableClosePeaks( cmFrqTrk* p, const cmReal_t* dbV, const cmReal_t* hzV, unsigned* pkiV, unsigned pkN )
{
  unsigned i;
  for(i=0; i<p->a.chCnt; ++i)
  {
    const cmFrqTrkCh_t* c      = p->ch + i;

    if( !c->activeFl )
      continue;

    cmReal_t            hz_min = c->hz * pow(2,-p->a.stRange/12.0);
    cmReal_t            hz_max = c->hz * pow(2, p->a.stRange/12.0);
    unsigned            j;

    // find all peaks within the frequency range hz_min to hz_max.
    for(j=0; j<pkN; ++j)
      if( pkiV[j] != cmInvalidIdx && hz_min <= c->hz && c->hz <= hz_max )      
        pkiV[j] = cmInvalidIdx;
          
  }
}

// Return the index into pkiV[] of the maximum energy peak in dbV[] 
// that is also above kAtkThreshDb.
unsigned _cmFrqTrkMaxEnergyPeakIndex( const cmFrqTrk* p, const cmReal_t* dbV, const cmReal_t* hzV, const unsigned* pkiV, unsigned pkN )
{
  cmReal_t mv = p->a.pkAtkThreshDb;
  unsigned mi = cmInvalidIdx;
  unsigned i;

  for(i=0; i<pkN; ++i)
    if( pkiV[i] != cmInvalidIdx && dbV[pkiV[i]] >= mv && hzV[pkiV[i]] < p->a.pkMaxHz )
    {
      mi = i;
      mv = dbV[pkiV[i]];
    }
      
  return mi;
}

// start new trackers
void _cmFrqTrkNewChs( cmFrqTrk* p, const cmReal_t* dbV, const cmReal_t* hzV, unsigned* pkiV, unsigned pkN )
{

  p->newTrkCnt = 0;

  while(1)
  {
    unsigned db_max_idx;
    cmFrqTrkCh_t* c;

    // find an inactive channel
    if((c = _cmFrqTrkFindAvailCh(p)) == NULL )
      break;
    
    // find the largest peak that is above pkAtkThreshDb && less than pkAtkHz.
    if((db_max_idx = _cmFrqTrkMaxEnergyPeakIndex(p,dbV,hzV,pkiV,pkN)) == cmInvalidIdx )
      break;

    // activate a new channel
    c->activeFl = true;
    c->tN       = 1;
    c->dN       = 0;
    c->hz       = hzV[ pkiV[ db_max_idx ] ];
    c->db       = dbV[ pkiV[ db_max_idx ] ];
    c->id       = p->nextTrkId++;
    c->si       = 0;
    c->sn       = 0;

    c->score       = 0;
    c->state       = kNoStateFrqTrkId;
    c->attenPhsIdx = cmInvalidIdx;
    c->attenGain   = 1.0;

    // mark the peak as unavailable
    pkiV[ db_max_idx ] = cmInvalidIdx;    

    p->newTrkCnt += 1;
  }

}

void _cmFrqTrkApplyFrqBias( cmFrqTrk* p, cmReal_t* xV )
{
  // convert to decibel scale (0.0 - 100.0) and then scale to (0.0 to 1.0)
  unsigned i;
  for(i=0; i<p->bN; ++i)
    xV[i] =  cmMax(0.0, (20*log10( cmMax(xV[i]/1.5,0.00001)) + 100.0)/100.0);

}


cmRC_t cmFrqTrkExec( cmFrqTrk* p, const cmReal_t* magV, const cmReal_t* phsV, const cmReal_t* hertzV )
{
  cmRC_t   rc = cmOkRC;
  cmReal_t hzV[ p->bN ];

  //cmReal_t powV[ p->bN ];
  //cmReal_t yV[ p->bN];

  //cmVOR_MultVVV(powV,p->bN,magV,magV);
  //cmWhFiltExec(p->wf,powV,p->dbV,p->bN);

  // convert magV to Decibels
  //cmVOR_AmplToDbVV(p->dbV,p->bN, magV, -200.0);

  
  // copy p->dbV to dbM[hi,:] 
  //cmVOR_CopyN(p->dbM + p->hi, p->bN, p->hN, p->dbV, 1 );
  //cmVOR_CopyN(p->dbM + p->hi, p->bN, p->hN, whV, 1 );

  if( 1 )
  {
    cmReal_t powV[ p->bN ];
    cmVOR_MultVVV(powV,p->bN,magV,magV);
    cmWhFiltExec(p->wf,powV,p->dbV,p->bN);
    _cmFrqTrkApplyFrqBias(p,p->dbV);
  }
  else
  {
    // convert magV to Decibels
    cmVOR_AmplToDbVV(p->dbV,p->bN, magV, -200.0);
  }

  // copy p->dbV to dbM[hi,:] 
  cmVOR_CopyN(p->dbM + p->hi, p->bN, p->hN, p->dbV, 1 );

  // increment hi to next column to fill in dbM[]
  p->hi = (p->hi + 1) % p->hN;

  // Set dbV[] to spectral magnitude profile by taking the mean over time
  // of the last hN magnitude vectors 
  cmVOR_MeanM2(p->dbV, p->dbM, p->hN, p->bN, 0, cmMin(p->fN+1,p->hN));
  //cmVOR_MeanM(p->dbV, p->dbM, p->hN, p->bN, 0);

  if( p->fN >= p->hN )
  {
    // set pkiV[] to the indexes of the peaks above pkThreshDb in i0[]
    unsigned pkN = cmVOR_PeakIndexes(p->pkiV, p->bN, p->dbV, p->bN, p->a.pkThreshDb );

    // set hzV[] to the peak frequencies assoc'd with peaks at dbV[ pkiV[] ].
    _cmFrqTrkMagnToHz(p, p->dbV, p->pkiV, pkN, hzV );

    // extend the existing trackers
    _cmFrqTrkExtendChs(p, p->dbV, hzV, p->pkiV, pkN );

    //_cmFrqTrkDisableClosePeaks(p, p->dbV,  hzV, p->pkiV, pkN );

    // create new trackers
    _cmFrqTrkNewChs(p,p->dbV,hzV,p->pkiV,pkN);
  
    //
    _cmFrqTrkScoreChs(p);

    // 
    _cmFrqTrkUpdateFilter(p);

    /*
    // write the log file
    _cmFrqTrkWriteLog(p);

    // write the spectrum output file
    if( p->specFn != NULL )
      cmVectArrayAppendR(p->specVa,p->dbV,p->bN);

    // write the atten output file
    if( p->attenFn != NULL )
      cmVectArrayAppendR(p->attenVa,p->aV,p->bN);

    // write the the level file
    _cmFrqTrkWriteLevel(p,p->dbV,hzV,p->bN);
    */
  }

  p->fN += 1;

  return rc;
}

void  cmFrqTrkPrint( cmFrqTrk* p )
{
  printf("srate:         %f\n",p->a.srate);
  printf("chCnt:         %i\n",p->a.chCnt);
  printf("binCnt:        %i (bN=%i)\n",p->a.binCnt,p->bN);
  printf("hopSmpCnt:     %i\n",p->a.hopSmpCnt);
  printf("stRange:       %f\n",p->a.stRange);
  printf("wndSecs:       %f (%i)\n",p->a.wndSecs,p->hN);
  printf("minTrkSec:     %f (%i)\n",p->a.minTrkSec,p->minTrkN);
  printf("maxTrkDeadSec: %f (%i)\n",p->a.maxTrkDeadSec,p->deadN_max);
  printf("pkThreshDb:    %f\n",p->a.pkThreshDb);
  printf("pkAtkThreshDb: %f\n",p->a.pkAtkThreshDb);

}

//------------------------------------------------------------------------------------------------------------
cmFbCtl_t*  cmFbCtlAlloc( cmCtx* c, cmFbCtl_t* ap, const cmFbCtlArgs_t* a )
{
  cmFbCtl_t* p = cmObjAlloc( cmFbCtl_t, c, ap );

  p->sva = cmVectArrayAlloc(c,kRealVaFl);
  p->uva = cmVectArrayAlloc(c,kRealVaFl);

  if( a != NULL )
  {
    if( cmFbCtlInit( p, a ) != cmOkRC )
      cmFbCtlFree(&p);
  }

  return p;

}

cmRC_t    cmFbCtlFree( cmFbCtl_t** pp )
{
  if( pp == NULL || *pp == NULL )
    return cmOkRC;

  cmFbCtl_t* p = *pp;
  
  cmVectArrayWrite(p->sva, "/home/kevin/temp/frqtrk/fb_ctl_s.va");
  cmVectArrayWrite(p->uva, "/home/kevin/temp/frqtrk/fb_ctl_u.va");

  cmMemFree(p->bM);
  cmMemFree(p->rmsV);
  cmVectArrayFree(&p->sva);
  cmVectArrayFree(&p->uva);
  cmObjFree(pp);
  return cmOkRC;

}

cmRC_t    cmFbCtlInit( cmFbCtl_t* p, const cmFbCtlArgs_t* a )
{
  cmRC_t rc;
  if((rc = cmFbCtlFinal(p)) != cmOkRC )
    return rc;

  double binHz = a->srate / ((a->binCnt-1)*2);

  p->a      = *a;
  p->frmCnt = (a->bufMs * a->srate / 1000.0) /a->hopSmpCnt;
  p->binCnt = cmMin(p->a.binCnt, a->maxHz/binHz);
  p->bM     = cmMemResizeZ(cmReal_t, p->bM,   p->binCnt*p->frmCnt);
  p->rmsV   = cmMemResizeZ(cmReal_t, p->rmsV, p->frmCnt);
  p->sV     = cmMemResizeZ(cmReal_t, p->sV,   p->binCnt);
  p->uV     = cmMemResizeZ(cmReal_t, p->uV,   p->binCnt);

  printf("cmFbCtl: frmCnt:%i binCnt:%i \n",p->frmCnt,p->binCnt);
  return rc;
}

cmRC_t    cmFbCtlFinal(cmFbCtl_t* p )
{ return cmOkRC; }

cmRC_t    cmFbCtlExec( cmFbCtl_t* p, const cmReal_t* x0V )
{
  unsigned i;
  cmRC_t rc = cmOkRC;

  cmReal_t xV[ p->binCnt ];
  cmVOR_AmplToDbVV(xV, p->binCnt, x0V, -1000.0 );

  cmVOR_Shift( p->rmsV, p->frmCnt, -1, 0 );
  p->rmsV[0] = cmVOR_Mean(xV,p->binCnt);

  cmVOR_CopyN(p->bM + p->bfi, p->binCnt, p->frmCnt, xV, 1 );

  p->bfi  = (p->bfi + 1) % p->frmCnt;
  p->bfN  = cmMin(p->bfN+1,p->frmCnt);
  
  for(i=0; i<p->binCnt; ++i)
  {
    const cmReal_t* v = p->bM + i * p->frmCnt;
    cmReal_t u = cmVOR_Mean(v, p->bfN );
    cmReal_t s = sqrt(cmVOR_Variance(v, p->bfN,&u));

    

    p->sV[i] = (0.0002 - s);
    p->uV[i] = u;
  }
 

  cmVectArrayAppendR(p->sva,p->sV,p->binCnt);
  cmVectArrayAppendR(p->uva,p->uV,p->binCnt);

  return rc;
}

//------------------------------------------------------------------------------------------------------------
cmSpecDist_t* cmSpecDistAlloc( cmCtx* ctx,cmSpecDist_t* ap, unsigned procSmpCnt, double srate, unsigned wndSmpCnt, unsigned hopFcmt, unsigned olaWndTypeId  )
{
  cmSpecDist_t* p = cmObjAlloc( cmSpecDist_t, ctx, ap );

  p->iSpecVa   = cmVectArrayAlloc(ctx,kRealVaFl);
  p->oSpecVa   = cmVectArrayAlloc(ctx,kRealVaFl);

  if( procSmpCnt != 0 )
  {
    if( cmSpecDistInit( p, procSmpCnt, srate, wndSmpCnt, hopFcmt, olaWndTypeId ) != cmOkRC )
      cmSpecDistFree(&p);
  }

  return p;

}

cmRC_t cmSpecDistFree( cmSpecDist_t** pp )
{
  if( pp == NULL || *pp == NULL )
    return cmOkRC;

  cmSpecDist_t* p = *pp;
  
  cmSpecDistFinal(p);
  cmVectArrayFree(&p->iSpecVa);
  cmVectArrayFree(&p->oSpecVa);
  cmMemPtrFree(&p->hzV);
  cmMemPtrFree(&p->iSpecM);
  cmMemPtrFree(&p->oSpecM);
  cmMemPtrFree(&p->iSpecV);
  cmMemPtrFree(&p->oSpecV);
  cmObjFree(pp);
  return cmOkRC;

}

cmRC_t cmSpecDistInit( cmSpecDist_t* p, unsigned procSmpCnt, double srate, unsigned wndSmpCnt, unsigned hopFcmt, unsigned olaWndTypeId  )
{
  cmFrqTrkArgs_t fta;

  cmRC_t rc;
  if((rc = cmSpecDistFinal(p)) != cmOkRC )
    return rc;

  unsigned flags = 0;


  p->srate        = srate;
  p->wndSmpCnt    = wndSmpCnt;
  p->hopSmpCnt    = (unsigned)floor(wndSmpCnt/hopFcmt);
  p->procSmpCnt   = procSmpCnt;

  p->mode         = kBasicModeSdId;
  p->thresh       = 60;
  p->offset       = 0;
  p->invertFl     = false;
  p->uprSlope     = 0.0;
  p->lwrSlope     = 2.0;

  p->pva = cmPvAnlAlloc(  p->obj.ctx, NULL, procSmpCnt, srate, wndSmpCnt, p->hopSmpCnt, flags );
  p->pvs = cmPvSynAlloc(  p->obj.ctx, NULL, procSmpCnt, srate, wndSmpCnt, p->hopSmpCnt, olaWndTypeId );

  fta.srate         = srate;
  fta.chCnt         = 50;       
  fta.binCnt        = p->pva->binCnt;
  fta.hopSmpCnt     = p->pva->hopSmpCnt;
  fta.stRange       = 0.25;
  fta.wndSecs       = 0.25;
  fta.minTrkSec     = 0.25;
  fta.maxTrkDeadSec = 0.25;
  fta.pkThreshDb    = 0.1; //-110.0;
  fta.pkAtkThreshDb = 0.4; //-60.0;
  fta.pkMaxHz       = 20000;
  fta.whFiltCoeff   = 0.33;

  fta.attenThresh = 0.4;
  fta.attenGain   = 0.5; 
  fta.attenDlySec = 1.0;
  fta.attenAtkSec = 1.0;  

  fta.logFn         = "/home/kevin/temp/frqtrk/trk_log.va";
  fta.levelFn       = "/home/kevin/temp/frqtrk/level.va";
  fta.specFn        = "/home/kevin/temp/frqtrk/spec.va";
  fta.attenFn       = "/home/kevin/temp/frqtrk/atten.va";

  p->ft  = cmFrqTrkAlloc( p->obj.ctx, NULL, &fta );
  cmFrqTrkPrint(p->ft);

  cmFbCtlArgs_t fba;
  fba.srate = srate;
  fba.binCnt = p->pva->binCnt;
  fba.hopSmpCnt = p->hopSmpCnt;
  fba.bufMs = 500;
  fba.maxHz = 5000;

  p->fbc  = cmFbCtlAlloc( p->obj.ctx, NULL, &fba );

  p->spcBwHz   = cmMin(srate/2,10000);
  p->spcSmArg  = 0.05;
  p->spcMin    = p->spcBwHz;
  p->spcMax    = 0.0;
  p->spcSum    = 0.0;
  p->spcCnt    = 0;
  double binHz = srate / p->pva->wndSmpCnt;
  p->spcBinCnt = (unsigned)floor(p->spcBwHz / binHz);
  p->hzV       = cmMemResizeZ(cmReal_t,p->hzV,p->spcBinCnt);
  cmVOR_Seq(    p->hzV, p->spcBinCnt, 0, 1 );
  cmVOR_MultVS( p->hzV, p->spcBinCnt, binHz );

  p->aeUnit = 0;
  p->aeMin  = 1000;
  p->aeMax  = -1000;

  
  double histSecs = 0.05;
  p->hN      = cmMax(1,histSecs * p->srate / p->hopSmpCnt );
  p->iSpecM  = cmMemResizeZ(cmReal_t,p->iSpecM,p->hN*p->pva->binCnt);
  p->oSpecM  = cmMemResizeZ(cmReal_t,p->oSpecM,p->hN*p->pva->binCnt);
  p->iSpecV  = cmMemResizeZ(cmReal_t,p->iSpecV,      p->pva->binCnt);
  p->oSpecV  = cmMemResizeZ(cmReal_t,p->oSpecV,      p->pva->binCnt);
  p->hi      = 0;


  //p->bypOut = cmMemResizeZ(cmSample_t, p->bypOut, procSmpCnt );

  return rc;
}

cmRC_t cmSpecDistFinal(cmSpecDist_t* p )
{
  cmRC_t rc = cmOkRC;

  cmVectArrayWrite(p->iSpecVa, "/home/kevin/temp/frqtrk/iSpec.va");
  cmVectArrayWrite(p->oSpecVa, "/home/kevin/temp/frqtrk/oSpec.va");

  cmPvAnlFree(&p->pva);
  cmPvSynFree(&p->pvs);
  cmFrqTrkFree(&p->ft);
  cmFbCtlFree(&p->fbc);
  return rc;
}

void _cmSpecDistBasicMode0(cmSpecDist_t* p, cmReal_t* X1m, unsigned binCnt, cmReal_t thresh )
{
  // octavez> thresh = 60;
  // octave> X1m = [-62 -61 -60 -59];
  // octave> -abs(abs(X1m+thresh)-(X1m+thresh)) - thresh
  // octave> ans = -64  -62  -60  -60

  unsigned i=0;
  for(i=0; i<binCnt; ++i)
  {
    cmReal_t a = fabs(X1m[i]);
    cmReal_t d = a - thresh;

    X1m[i] = -thresh;

    if( d > 0 )
      X1m[i] -= 2*d;

  }


}

void _cmSpecDistBasicMode(cmSpecDist_t* p, cmReal_t* X1m, unsigned binCnt, cmReal_t thresh )
{

  unsigned i=0;

  if( p->lwrSlope < 0.3 )
    p->lwrSlope = 0.3;

  for(i=0; i<binCnt; ++i)
  {
    cmReal_t a = fabs(X1m[i]);
    cmReal_t d = a - thresh;

    X1m[i] = -thresh;

    if( d > 0 )
      X1m[i] -= (p->lwrSlope*d);
    else
      X1m[i] -= (p->uprSlope*d);
  }

}

cmReal_t  _cmSpecDistCentMode( cmSpecDist_t* p, cmReal_t* X1m )
{
  // calc the spectral centroid
  double num = cmVOR_MultSumVV( p->pva->magV, p->hzV, p->spcBinCnt );
  double den = cmVOR_Sum(       p->pva->magV, p->spcBinCnt );
  double result = 0;
  if( den != 0 )
    result = num/den;

  // apply smoothing filter to spectral centroid
  p->spc =  (result * p->spcSmArg) + (p->spc * (1.0-p->spcSmArg));

  // track spec. cetr. min and max
  p->spcMin = cmMin(p->spcMin,p->spc);
  p->spcMax = cmMax(p->spcMax,p->spc);

  //-----------------------------------------------------
  ++p->spcCnt;
  p->spcSum   += p->spc;
  p->spcSqSum += p->spc * p->spc;

  // use the one-pass std-dev calc. trick
  //double mean     = p->spcSum / p->spcCnt;
  //double variance = p->spcSqSum / p->spcCnt - mean * mean;
  //double std_dev  =  sqrt(variance);

  double smin = p->spcMin;
  double smax = p->spcMax;

  //smin = mean - std_dev;
  //smax = mean + std_dev;
  //-----------------------------------------------------


  // convert spec. cent. to unit range
  double spcUnit = (p->spc - smin) / (smax - smin);

  spcUnit = cmMin(1.0,cmMax(0.0,spcUnit));

  if( p->invertFl )
    spcUnit = 1.0 - spcUnit;

  //if( p->spcMin==p->spc || p->spcMax==p->spc )
  //  printf("min:%f avg:%f sd:%f max:%f\n",p->spcMin,p->spcSum/p->spcCnt,std_dev,p->spcMax);
    
  return spcUnit;
}

void _cmSpecDistBump( cmSpecDist_t* p, cmReal_t* x, unsigned binCnt, double thresh)
{
  /*
  thresh *= -1;
  minDb = -100;

  if db < minDb
     db = minDb;
  endif
  
  if db > thresh 
    y = 1;
  else
    x =  (minDb - db)/(minDb - thresh);

    y = x + (x - (x.^coeff));    
  endif

  y = minDb + abs(minDb) * y;
  */
  unsigned i=0;

  //printf("%f %f %f\n",thresh,p->lwrSlope,x[0]);
  
  double minDb = -100.0;
  thresh = -thresh;

  for(i=0; i<binCnt; ++i)
  {
    double y;

    if( x[i] < minDb )
      x[i] = minDb;

    if( x[i] > thresh )
      y = 1;
    else
    {
      y  = (minDb - x[i])/(minDb - thresh);
      y += y - pow(y,p->lwrSlope);
    }

    x[i] = minDb + (-minDb) * y;

  }

  
}

void _cmSpecDistAmpEnvMode( cmSpecDist_t* p, cmReal_t* X1m )
{

  cmReal_t smCoeff = 0.1;

  //
  cmReal_t mx = cmVOR_Max(X1m,p->pva->binCnt,1);
  p->aeSmMax  = (mx * smCoeff) + (p->aeSmMax * (1.0-smCoeff));

  cmReal_t a = cmVOR_Mean(X1m,p->pva->binCnt);
  
  p->ae = (a * smCoeff) + (p->ae * (1.0-smCoeff));
  
  p->aeMin = cmMin(p->ae,p->aeMin);
  p->aeMax = cmMax(p->ae,p->aeMax);
  
  p->aeUnit = (p->ae - p->aeMin) / (p->aeMax-p->aeMin);

  p->aeUnit = cmMin(1.0,cmMax(0.0,p->aeUnit));

  if( p->invertFl )
    p->aeUnit = 1.0 - p->aeUnit;


  //printf("%f\n",p->aeSmMax);

}

void _cmSpecDistPhaseMod( cmSpecDist_t* p, cmReal_t* phsV, unsigned binCnt )
{
  unsigned i;
  cmReal_t offs =  sin( 0.1 * 2.0 * M_PI * (p->phaseModIndex++) / (p->srate/p->hopSmpCnt) );

  //printf("offs %f %i %i %f\n",offs,p->phaseModIndex,p->hopSmpCnt,p->srate);

  cmReal_t new_phs = phsV[0] + offs;
  for(i=0; i<binCnt-1; ++i)
  {
    while( new_phs > M_PI )
      new_phs -= 2.0*M_PI;

    while( new_phs < -M_PI )
      new_phs += 2.0*M_PI;

    cmReal_t d = phsV[i+1] - phsV[i];

    phsV[i] = new_phs;

    new_phs += d;    
  }
  
}

cmRC_t  cmSpecDistExec( cmSpecDist_t* p, const cmSample_t* sp, unsigned sn )
{

  assert( sn == p->procSmpCnt );

  bool recordFl = false;

  // cmPvAnlExec() returns true when it calc's a new spectral output frame
  if( cmPvAnlExec( p->pva, sp, sn ) )
  {
    cmReal_t X1m[p->pva->binCnt]; 

    // take the mean of the the input magntitude spectrum
    cmReal_t u0 = cmVOR_Mean(p->pva->magV,p->pva->binCnt);

    if(recordFl)
    {
      // store a time windowed average of the input spectrum to p->iSpecV
      cmVOR_CopyN(p->iSpecM + p->hi, p->pva->binCnt, p->hN, X1m, 1 );
      cmVOR_MeanM2(p->iSpecV, p->iSpecM, p->hN, p->pva->binCnt, 0, cmMin(p->fi+1,p->hN));
    }

    cmVOR_AmplToDbVV(X1m, p->pva->binCnt, p->pva->magV, -1000.0 );
    //cmVOR_AmplToDbVV(X1m, p->pva->binCnt, X1m, -1000.0 );



    switch( p->mode )
    {
      case kBypassModeSdId:
        break;

      case kBasicModeSdId:
        _cmSpecDistBasicMode(p,X1m,p->pva->binCnt,p->thresh);
        break;

      case kSpecCentSdId:
        {
          _cmSpecDistAmpEnvMode(p,X1m);
          double spcUnit = _cmSpecDistCentMode(p,X1m);
          double thresh  = fabs(p->aeSmMax) - (spcUnit*p->offset);
          _cmSpecDistBasicMode(p,X1m,p->pva->binCnt, thresh);
        }
        break;

      case kAmpEnvSdId:
        {
          _cmSpecDistAmpEnvMode(p,X1m);
          //double thresh = fabs(p->aeSmMax) - p->offset;
          double thresh = fabs(p->aeSmMax) - (p->aeUnit*p->offset);
          thresh = fabs(p->thresh) - (p->aeUnit*p->offset);
          _cmSpecDistBasicMode(p,X1m,p->pva->binCnt, thresh);
        }
        break;

      case kBumpSdId:
        _cmSpecDistBump(p,X1m, p->pva->binCnt, p->offset);
        _cmSpecDistBasicMode(p,X1m,p->pva->binCnt,p->thresh);
        break;

      case 5:
        break;

      default:
        break;
    }

    cmVOR_DbToAmplVV(X1m, p->pva->binCnt, X1m );


    // run and apply the tracker/supressor
    cmFrqTrkExec(p->ft, X1m, p->pva->phsV, NULL ); 
    //cmVOR_MultVV(X1m, p->pva->binCnt,p->ft->aV );


    // convert the mean input magnitude to db
    cmReal_t idb = 20*log10(u0);
    
    // get the mean output magnitude spectra
    cmReal_t u1 = cmVOR_Mean(X1m,p->pva->binCnt);

    if( idb > -150.0 )
    {
      // set the output gain such that the mean output magnitude
      // will match the mean input magnitude
      p->ogain = u0/u1;  
    }
    else
    {
      cmReal_t a0 = 0.9;
      p->ogain *= a0;
    }

    cmVOR_MultVS(X1m,p->pva->binCnt,cmMin(4.0,p->ogain));


    //cmFbCtlExec(p->fbc,X1m);

    //cmReal_t v[ p->pva->binCnt ];
    //cmVOR_Copy(v,p->pva->binCnt,p->pva->phsV);
    //_cmSpecDistPhaseMod(p, v, p->pva->binCnt );

    
    if(recordFl)
    {

      // store a time windowed average of the output spectrum to p->iSpecV
      cmVOR_CopyN(p->oSpecM + p->hi, p->pva->binCnt, p->hN, X1m, 1 );
      cmVOR_MeanM2(p->oSpecV, p->oSpecM, p->hN, p->pva->binCnt, 0, cmMin(p->fi+1,p->hN));
      
      // store iSpecV and oSpecV to iSpecVa and oSpecVa to create debugging files
      cmVectArrayAppendR(p->iSpecVa,p->iSpecV,p->pva->binCnt);
      cmVectArrayAppendR(p->oSpecVa,p->oSpecV,p->pva->binCnt);

      p->hi = (p->hi + 1) % p->hN;
    }
    

    cmPvSynExec(p->pvs, X1m, p->pva->phsV );
  
    p->fi += 1;
  }
 
  return cmOkRC;
}


const cmSample_t* cmSpecDistOut(  cmSpecDist_t* p )
{ 
  return cmPvSynExecOut(p->pvs); 
}

//------------------------------------------------------------------------------------------------------------

cmRC_t _cmBinMtxFileWriteHdr( cmBinMtxFile_t* p )
{
  cmFileRC_t fileRC;
  unsigned   n = 3;
  unsigned   hdr[n];

  hdr[0] = p->rowCnt;
  hdr[1] = p->maxRowEleCnt;
  hdr[2] = p->eleByteCnt;

  if((fileRC = cmFileSeek(p->fh,kBeginFileFl,0)) != kOkFileRC )
    return cmCtxRtCondition(&p->obj, fileRC, "File seek failed on matrix file:'%s'.", cmStringNullGuard(cmFileName(p->fh)));
  
  if((fileRC = cmFileWriteUInt(p->fh,hdr,n)) != kOkFileRC )
    return cmCtxRtCondition( &p->obj, fileRC, "Header write failed on matrix file:'%s'.", cmStringNullGuard(cmFileName(p->fh)) );

  return cmOkRC;
}


cmBinMtxFile_t* cmBinMtxFileAlloc( cmCtx* ctx, cmBinMtxFile_t* ap, const cmChar_t* fn )
{
  cmBinMtxFile_t* p = cmObjAlloc( cmBinMtxFile_t, ctx, ap );
  if( fn != NULL )
    if( cmBinMtxFileInit( p, fn ) != cmOkRC )
      cmBinMtxFileFree(&p);
  return p;
}

cmRC_t       cmBinMtxFileFree(  cmBinMtxFile_t** pp )
{
  cmRC_t rc;

  if( pp==NULL || *pp == NULL )
    return cmOkRC;

  cmBinMtxFile_t* p = *pp;

  if((rc = cmBinMtxFileFinal(p)) == cmOkRC )
  {
    cmObjFree(pp);
  }
  return rc;
}

cmRC_t       cmBinMtxFileInit(  cmBinMtxFile_t* p, const cmChar_t* fn )
{
  cmRC_t     rc;
  cmFileRC_t fileRC;

  if((rc = cmBinMtxFileFinal(p)) != cmOkRC )
    return rc;

  // open the output file for writing
  if((fileRC = cmFileOpen(&p->fh,fn,kWriteFileFl | kBinaryFileFl, p->obj.err.rpt)) != kOkFileRC )
    return cmCtxRtCondition( &p->obj, fileRC, "Unable to open the matrix file:'%s'", cmStringNullGuard(fn) );

  // iniitlaize the object
  p->rowCnt       = 0;
  p->maxRowEleCnt = 0;
  p->eleByteCnt   = 0;

  // write the blank header as place holder
  if((rc = _cmBinMtxFileWriteHdr(p)) != cmOkRC )
    return rc;

  return rc;
}

cmRC_t       cmBinMtxFileFinal( cmBinMtxFile_t* p )
{
  cmRC_t     rc;
  cmFileRC_t fileRC;

  if( p != NULL && cmFileIsValid(p->fh))
  {
    // re-write the file header
    if((rc = _cmBinMtxFileWriteHdr(p)) != cmOkRC )
      return rc;
      
    // close the file
    if((fileRC = cmFileClose(&p->fh)) != kOkFileRC )
      return cmCtxRtCondition(&p->obj, fileRC, "Matrix file close failed on:'%s'",cmStringNullGuard(cmFileName(p->fh)));
  }

  return cmOkRC;
}

bool cmBinMtxFileIsValid( cmBinMtxFile_t* p )
{ return p != NULL && cmFileIsValid(p->fh); }

cmRC_t _cmBinMtxFileWriteRow( cmBinMtxFile_t* p, const void* buf, unsigned eleCnt, unsigned eleByteCnt )
{
  cmFileRC_t fileRC;

  if((fileRC = cmFileWrite(p->fh,&eleCnt,sizeof(eleCnt))) != kOkFileRC )
    return cmCtxRtCondition(&p->obj, fileRC, "Matrix file row at index %i element count write failed on '%s'.", p->rowCnt, cmStringNullGuard(cmFileName(p->fh)));

  if((fileRC = cmFileWrite(p->fh,buf,eleCnt*eleByteCnt)) != kOkFileRC )
    return cmCtxRtCondition(&p->obj, fileRC, "Matrix file row at index %i data write failed on '%s'.", p->rowCnt, cmStringNullGuard(cmFileName(p->fh)));

  if( eleCnt > p->maxRowEleCnt )
    p->maxRowEleCnt = eleCnt;

  ++p->rowCnt;

  return cmOkRC;
}

cmRC_t       cmBinMtxFileExecS( cmBinMtxFile_t* p, const cmSample_t* x, unsigned xn )
{
  // verify that all rows are written as cmSample_t
  assert( p->eleByteCnt == 0 || p->eleByteCnt == sizeof(cmSample_t));

  p->eleByteCnt = sizeof(cmSample_t);
  
  return _cmBinMtxFileWriteRow(p,x,xn,p->eleByteCnt);
}

cmRC_t       cmBinMtxFileExecR( cmBinMtxFile_t* p, const cmReal_t*   x, unsigned xn )
{
  // verify that all rows are written as cmReal_t
  assert( p->eleByteCnt == 0 || p->eleByteCnt == sizeof(cmReal_t));

  p->eleByteCnt = sizeof(cmReal_t);
  
  return _cmBinMtxFileWriteRow(p,x,xn,p->eleByteCnt);
}

cmRC_t cmBinMtxFileWrite( const cmChar_t* fn, unsigned rowCnt, unsigned colCnt, const cmSample_t* sp, const cmReal_t* rp, cmCtx* ctx, cmRpt_t* rpt )
{
  assert( sp == NULL || rp == NULL );
  
  cmCtx*          ctxp = NULL;
  cmBinMtxFile_t* bp   = NULL;

  if( ctx == NULL )   
    ctx = ctxp = cmCtxAlloc(NULL,rpt,cmLHeapNullHandle,cmSymTblNullHandle);  

  if((bp = cmBinMtxFileAlloc(ctx,NULL,fn)) != NULL )
  {
    unsigned        i    = 0;
    cmSample_t*     sbp  = sp == NULL ? NULL : cmMemAlloc(cmSample_t,colCnt);
    cmReal_t*       rbp  = rp == NULL ? NULL : cmMemAlloc(cmReal_t,colCnt);
  
    for(i=0; i<rowCnt; ++i)
    {
      if( sp!=NULL )
      {
        cmVOS_CopyN(sbp,colCnt,1,sp+i,rowCnt);
        cmBinMtxFileExecS(bp,sbp,colCnt);
      }

      if( rp!=NULL )
      {
        cmVOR_CopyN(rbp,colCnt,1,rp+i,rowCnt);
        cmBinMtxFileExecR(bp,rbp,colCnt);
      }
    
    }

    cmMemPtrFree(&sbp);
    cmMemPtrFree(&rbp);
    cmBinMtxFileFree(&bp);
  }

  if( ctxp != NULL )
    cmCtxFree(&ctxp);

  return cmOkRC;
}


cmRC_t _cmBinMtxFileReadHdr( cmCtx_t* ctx, cmFileH_t h, unsigned* rowCntPtr, unsigned* colCntPtr, unsigned* eleByteCntPtr, const cmChar_t* fn )
{
  cmRC_t rc = cmOkRC;
  unsigned hdr[3];

  if( cmFileRead(h,&hdr,sizeof(hdr)) != kOkFileRC )
  {
    rc = cmErrMsg(&ctx->err,cmSubSysFailRC,"Binary matrix file header read failed on '%s'.",cmStringNullGuard(fn));
    goto errLabel;
  }
  
  if( rowCntPtr != NULL )
    *rowCntPtr     = hdr[0];

  if( colCntPtr != NULL )
    *colCntPtr     = hdr[1];

  if( eleByteCntPtr != NULL )
    *eleByteCntPtr = hdr[2];

 errLabel:
  return rc;

}

cmRC_t cmBinMtxFileSize( cmCtx_t* ctx, const cmChar_t* fn, unsigned* rowCntPtr, unsigned* colCntPtr, unsigned* eleByteCntPtr )
{
  cmFileH_t h  = cmFileNullHandle;
  cmRC_t    rc = cmOkRC;

  if(cmFileOpen(&h,fn,kReadFileFl | kBinaryFileFl, ctx->err.rpt) != kOkFileRC )
  {
    rc = cmErrMsg(&ctx->err,cmSubSysFailRC,"Binary matrix file:%s open failed.",cmStringNullGuard(fn));
    goto errLabel;
  }

  rc =  _cmBinMtxFileReadHdr(ctx,h,rowCntPtr,colCntPtr,eleByteCntPtr,fn);

 errLabel:

  cmFileClose(&h);

  return rc;
  
}

cmRC_t cmBinMtxFileRead( cmCtx_t* ctx, const cmChar_t* fn, unsigned mRowCnt, unsigned mColCnt, unsigned mEleByteCnt, void* retBuf, unsigned* colCntV )
{

  cmFileH_t h  = cmFileNullHandle;
  cmRC_t    rc = cmOkRC;
  char*     rowBuf = NULL;
  unsigned rowCnt,colCnt,eleByteCnt,i;

  cmErr_t err;
  cmErrSetup(&err,ctx->err.rpt,"Binary Matrix File Reader");

  if(cmFileOpen(&h,fn,kReadFileFl | kBinaryFileFl, err.rpt) != kOkFileRC )
  {
    rc = cmErrMsg(&err,cmSubSysFailRC,"Binary matrix file:%s open failed.",cmStringNullGuard(fn));
    goto errLabel;
  }

  if((rc =  _cmBinMtxFileReadHdr(ctx,h,&rowCnt,&colCnt,&eleByteCnt,fn)) != cmOkRC )
    goto errLabel;

  if( mRowCnt != rowCnt )
    rc = cmErrMsg(&err,cmArgAssertRC,"The binary matrix file row count and the return buffer row count are not the same.");

  if( mColCnt != colCnt )
    rc = cmErrMsg(&err,cmArgAssertRC,"The binary matrix file column count and the return buffer column count are not the same.");

  if( mEleByteCnt != eleByteCnt )
    rc = cmErrMsg(&err,cmArgAssertRC,"The binary matrix file element byte count and the return buffer element byte count are not the same.");

  if( rc == cmOkRC )
  {
    rowBuf = cmMemAllocZ(char,colCnt*eleByteCnt);

    for(i=0; i<rowCnt; ++i)
    {
      unsigned cn;


      // read the row length
      if( cmFileReadUInt(h,&cn,1) != kOkFileRC )
      {
        rc = cmErrMsg(&err,cmSubSysFailRC,"Error reading row length at row index:%i.",i);
        goto errLabel;
      }

      if( colCntV != NULL )
        colCntV[i] = cn;

      // verify the actual col count does not exceed the max col count
      if( cn > colCnt )
      {
        rc = cmErrMsg(&err,cmSubSysFailRC,"The actual column count:%i exceeds the max column count:%i.",cn,colCnt);
        goto errLabel;
      }
      
      //read the row data
      if( cmFileReadChar(h,rowBuf,cn*eleByteCnt) != kOkFileRC )
      {
        rc = cmErrMsg(&err,cmSubSysFailRC,"File read failed at row index:%i.",i);
        goto errLabel;
      }

      char* dp = ((char*)retBuf) + i * eleByteCnt;

      // the data is read in row-major order but the matrix must be
      // returned on col major order - rearrange the columns here.
      switch(eleByteCnt)
      {
        case sizeof(cmSample_t):
          cmVOS_CopyN(((cmSample_t*)dp), cn, rowCnt, (cmSample_t*)rowBuf, 1 );
          break;

        case sizeof(cmReal_t):
          cmVOR_CopyN(((cmReal_t*)dp), cn, rowCnt, (cmReal_t*)rowBuf, 1 );
          break;

        default:
          rc = cmErrMsg(&err,cmSubSysFailRC,"Invalid element byte count:%i.",eleByteCnt);
          goto errLabel;
      }
      
    }
  }

 errLabel:

  cmMemPtrFree(&rowBuf);

  cmFileClose(&h);

  return rc;
  
  
}



