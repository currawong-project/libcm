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
#include "cmSaProc.h"
#include "cmVectOps.h"

#define SS_IMPL
#ifdef SS_IMPL
#include "ss0/surroundstereo.h"
#include "ss1/surroundstereo_1.h"
#else
void binauralEncoderProcessInit(long pNumInputs, long frames, int hrtfSet) {}
void binauralEncoderProcessSetPosition(int clientNum, float cazimuth, float celevation) {}
void binauralEncoderProcess(float **inputs, float **outputs, long numInputs, long sampleFrames){}

typedef struct
{
  void* h;
} binauralEncoderH_t;

binauralEncoderH_t binauralEncoderNullH = cmSTATIC_NULL_HANDLE;

void  binauralEncProcessInit(binauralEncoderH_t* hp, long pNumInputs, long frames, int hrtfSet)
{ if(hp!=NULL) hp->h=NULL; }

void  binauralEncProcessFree( binauralEncoderH_t* hp ){}
void	binauralEncProcessSetPosition(binauralEncoderH_t h, int clientNum, float cazimuth, float celevation){}
void	binauralEncProcess(binauralEncoderH_t h, float **inputs, float **outputs, long numInputs, long sampleFrames){}
#endif

cmBinEnc* cmBinEncAlloc( cmCtx* c, cmBinEnc* p, double srate, unsigned procSmpCnt )
{
  cmBinEnc* op = cmObjAlloc(cmBinEnc,c,p);
  
  if( srate > 0 )  
    if( cmBinEncInit(op,srate,procSmpCnt) != cmOkRC )
      cmBinEncFree(&op);

  return op;

}

cmRC_t    cmBinEncFree( cmBinEnc** pp )
{
  cmRC_t rc = cmOkRC;
  if( pp==NULL || *pp==NULL )
    return rc;

  cmBinEnc* p = *pp;
  if((rc = cmBinEncFinal(p)) != cmOkRC )
    return rc;

  cmObjFree(pp);
  return rc;

}

cmRC_t    cmBinEncInit( cmBinEnc* p, double srate, unsigned procSmpCnt )
{
  cmRC_t rc;
  binauralEncoderH_t h = binauralEncoderNullH;

  if((rc = cmBinEncFinal(p)) != cmOkRC )
    return rc;

  p->srate      = srate;
  p->procSmpCnt = procSmpCnt;
  p->freeFl     = false;
  
  long numInputs = 1;
  int  hrtfSet   = 1;

  switch(p->mode)
  {
    case 0:
      binauralEncoderProcessInit(numInputs, procSmpCnt, hrtfSet);
      break;

    case 1:
      binauralEncProcessInit(&h,numInputs, procSmpCnt, hrtfSet);
      p->freeFl = true;
      p->h      = h.h;
      break;
  }

  return rc;
}

cmRC_t    cmBinEncFinal(cmBinEnc* p )
{ 
  if( p->h != NULL && p->freeFl )
  {
    binauralEncoderH_t h;
    h.h = p->h;
    binauralEncProcessFree(&h);
    p->freeFl = false;
  }
  return cmOkRC; 
}

cmRC_t    cmBinEncSetMode(cmBinEnc* p, unsigned mode )
{
  if( mode != p->mode )
  {
    p->mode = mode;
    return cmBinEncInit(p,p->srate,p->procSmpCnt);
  }

  return cmOkRC;
}

cmRC_t    cmBinEncSetLoc( cmBinEnc* p, float azimDegrees, float elevDegrees, float dist )
{
  int clientNum = 0;

  if( elevDegrees > 90.0f )
    elevDegrees = 90.0f;

  if( elevDegrees < -40.0f )
    elevDegrees = -40.0f;

  float elev = (elevDegrees + 40.0f) / (90.0f + 40.0f);

  //printf("az:%f el:%f dist:%f\n",azimDegrees,elev,dist);

  switch( p->mode )
  {
    case 0:
      binauralEncoderProcessSetPosition(clientNum, azimDegrees/360.0f, elev);
      break;

    case 1:
      {      
        binauralEncoderH_t h;
        h.h = p->h;
        binauralEncProcessSetPosition(h,clientNum, azimDegrees/360.0f, elev);
      }
      break;
  }

  return cmOkRC;
}

cmRC_t    cmBinEncExec( cmBinEnc* p, const cmSample_t* x, cmSample_t* y0, cmSample_t* y1, unsigned xyN )
{
  float *inputs[]  = {(float*)x};
  float *outputs[] = {y0,y1};
  int numInputs    = sizeof(inputs)/sizeof(inputs[0]);


  switch( p->mode )
  {
    case 0:
      binauralEncoderProcess(inputs,outputs,numInputs,xyN);
      break;

    case 1:
      {
        binauralEncoderH_t h;
        h.h = p->h;
        binauralEncProcess(h,inputs,outputs,numInputs,xyN);
      }
      break;
  }

  return cmOkRC;
}
