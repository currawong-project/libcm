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
#include "cmProc.h"
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


