#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmAudioPort.h"
#include "cmAudioNrtDev.h"
#include "cmThread.h" 

enum
{
  kStartedApNrtFl = 0x01,
  kInStateApNrtFl = 0x02
};

typedef struct
{
  unsigned phase;
  bool     implFl;
  double   gain;
} cmApNrtCh_t;

typedef struct cmApNrtDev_str
{
  unsigned               flags;
  unsigned               devIdx; // nrt device index
  unsigned               baseApDevIdx; // global audio device index for first nrt device
  cmChar_t*              label;         
  unsigned               iChCnt;
  unsigned               oChCnt;
  double                 srate;
  unsigned               fpc;   
  cmThreadH_t            thH;
  cmApCallbackPtr_t      cbPtr;
  void*                  cbArg;
  unsigned               cbPeriodMs;
  struct cmApNrtDev_str* link;
  unsigned               curTimeSmp;
  cmApNrtCh_t*           iChArray;
  cmApNrtCh_t*           oChArray;
} cmApNrtDev_t;

typedef struct
{
  cmErr_t       err;
  unsigned      devCnt;
  cmApNrtDev_t* devs;
  unsigned      baseApDevIdx;
} cmApNrt_t;

cmApNrt_t* _cmNrt = NULL;

cmApNrtDev_t* _cmApNrtIndexToDev( unsigned idx )
{
  cmApNrtDev_t* dp = _cmNrt->devs;
  unsigned i;
  for(i=0; dp!=NULL && i<idx; ++i)
    dp = dp->link;

  assert( dp != NULL );
  return dp;
}

void cmApNrtGenImpulseCh( cmApNrtDev_t* dp, unsigned chIdx, cmSample_t* buf, unsigned chCnt, unsigned frmCnt )
{
  double   periodMs  = 500;    // impulse period
  double   durMs     = 50;      // impulse length
  double   loDb      = -90.0;
  double   hiDb      = -20.0;
  double   loLin     = pow(10.0,loDb/20.0);
  double   hiLin     = pow(10.0,hiDb/20.0);
  unsigned periodSmp = (unsigned)(periodMs * dp->srate / 1000.0);
  unsigned durSmp    = (unsigned)(   durMs * dp->srate / 1000.0);
  
  assert( chIdx < chCnt );

  cmApNrtCh_t* cp = dp->iChArray + chIdx;
  cmSample_t*  sp = buf + chIdx;

  unsigned i;
  for(i=0; i<frmCnt; ++i, sp+=chCnt )
  {
    unsigned limit = periodSmp;
    double   gain  = loLin;

    cp->phase += 1;

    if( cp->implFl )
    {
      gain  = cp->gain;
      limit = durSmp;
    }

    *sp = (gain * 2.0 * rand()/RAND_MAX) - 1.0;

    if( cp->phase > limit )
    {
      cp->phase  = 0;
      cp->implFl = !cp->implFl;

      if( cp->implFl )
        cp->gain = loLin + (hiLin - loLin) * rand()/RAND_MAX ;
      
    }

  }

}

void cmApNrtGenImpulse( cmApNrtDev_t* dp, cmSample_t* buf, unsigned chCnt, unsigned frmCnt )
{
  unsigned i=0;
  for(; i<chCnt; ++i)
    cmApNrtGenImpulseCh(dp,i,buf,chCnt,frmCnt);
}

void cmApNrtGenSamples( cmApNrtDev_t* dp, cmSample_t* buf, unsigned chCnt, unsigned frmCnt )
{
  memset(buf,0,chCnt*frmCnt*sizeof(cmSample_t));

  cmApNrtGenImpulse(dp,buf,chCnt,frmCnt);

  dp->curTimeSmp += frmCnt;
} 

// return 'false' to terminate otherwise return 'true'.
bool cmApNrtThreadFunc(void* param)
{
  cmApNrtDev_t* dp = (cmApNrtDev_t*)param;

  usleep( dp->cbPeriodMs * 1000 );


  cmApAudioPacket_t pkt;
  bool              inFl = cmIsFlag(dp->flags,kInStateApNrtFl);  

  pkt.devIdx         = dp->devIdx + dp->baseApDevIdx;
  pkt.begChIdx       = 0;
  pkt.audioFramesCnt = dp->fpc;
  pkt.bitsPerSample  = 32;
  pkt.flags          = kInterleavedApFl | kFloatApFl;
  pkt.userCbPtr      = dp->cbArg;

  if( inFl )
  {
    unsigned   bn = dp->iChCnt * dp->fpc;
    cmSample_t b[ bn ];

    pkt.chCnt         = dp->iChCnt;
    pkt.audioBytesPtr = b;

    cmApNrtGenSamples(dp,b,dp->iChCnt,dp->fpc);
  
    dp->cbPtr(&pkt,1,NULL,0 );  // send the samples to the application
  }
  else
  {
    unsigned   bn = dp->oChCnt * dp->fpc;
    cmSample_t b[ bn ];

    pkt.chCnt         = dp->oChCnt;
    pkt.audioBytesPtr = b;

    dp->cbPtr(NULL,0,&pkt,1 );  // recv the samples from the application
  }
  
  dp->flags = cmTogFlag(dp->flags,kInStateApNrtFl);

  return true;
}

cmApRC_t cmApNrtAllocate( cmRpt_t* rpt )
{
  if( _cmNrt != NULL )
    cmApNrtFree();

  _cmNrt = cmMemAllocZ(cmApNrt_t,1);

  cmErrSetup(&_cmNrt->err,rpt,"cmAudioNrtDev");
  _cmNrt->devCnt       = 0;
  _cmNrt->devs         = NULL;
  _cmNrt->baseApDevIdx = 0;
  return kOkApRC;
}

cmApRC_t cmApNrtFree()
{
  cmApRC_t rc = kOkApRC;
  cmApNrtDev_t* dp = _cmNrt->devs;
  while( dp != NULL )
  {
    cmApNrtDev_t* np = dp->link;

    if( cmThreadIsValid(dp->thH) ) 
      if( cmThreadDestroy(&dp->thH) != kOkThRC )
        rc = cmErrMsg(&_cmNrt->err,kThreadFailApRC,"Thread destroy failed.");

    cmMemFree(dp->label);
    cmMemFree(dp->iChArray);
    cmMemFree(dp->oChArray);
    cmMemFree(dp);
    dp = np;
  }

  if( rc == kOkApRC )
  {
    cmMemPtrFree(&_cmNrt);
  }

  return rc;
}

cmApRC_t cmApNrtCreateDevice( 
  const cmChar_t* label,
  double          srate,
  unsigned        iChCnt,
  unsigned        oChCnt,
  unsigned        cbPeriodMs )
{
  cmApRC_t rc = kOkApRC;

  cmApNrtDev_t* dp = cmMemAllocZ(cmApNrtDev_t,1);

  dp->devIdx       = _cmNrt->devCnt;
  dp->baseApDevIdx = _cmNrt->baseApDevIdx;
  dp->label        = cmMemAllocStr(label);
  dp->iChCnt       = iChCnt;
  dp->oChCnt       = oChCnt;
  dp->srate        = srate;
  dp->fpc          = 0;
  dp->cbPeriodMs   = cbPeriodMs;
  dp->cbPtr        = NULL;
  dp->cbArg        = NULL;
  dp->link         = NULL;
  dp->iChArray    = iChCnt==0 ? NULL : cmMemAllocZ(cmApNrtCh_t,iChCnt);
  dp->oChArray    = oChCnt==0 ? NULL : cmMemAllocZ(cmApNrtCh_t,oChCnt);

  // attach the new recd to the end of the list
  cmApNrtDev_t* np = _cmNrt->devs;
  while( np != NULL && np->link != NULL )
    np = np->link;

  if( np == NULL )
    _cmNrt->devs = dp;
  else
    np->link = dp;

  if( cmThreadCreate( &dp->thH, cmApNrtThreadFunc, dp, _cmNrt->err.rpt ) != kOkThRC )
    rc = cmErrMsg(&_cmNrt->err,kThreadFailApRC,"Thread create failed.");

  ++_cmNrt->devCnt;
  return rc;
}  

cmApRC_t      cmApNrtInitialize( cmRpt_t* rpt, unsigned baseApDevIdx )
{
  // set the baseApDevIdx for each device 
  cmApNrtDev_t* dp = _cmNrt->devs;
  for(; dp!=NULL; dp=dp->link)
    dp->baseApDevIdx = baseApDevIdx;

  // store the baseApDevIdx for any devices that may be created after initialization
  _cmNrt->baseApDevIdx = baseApDevIdx;

  return kOkApRC;
}

cmApRC_t      cmApNrtFinalize()
{
  return kOkApRC;
}

unsigned      cmApNrtDeviceCount()
{ return _cmNrt==NULL ? 0 : _cmNrt->devCnt; }

const char*   cmApNrtDeviceLabel(          unsigned devIdx )
{
  assert( devIdx < _cmNrt->devCnt );
  const cmApNrtDev_t* dp = _cmApNrtIndexToDev(devIdx);
  return dp->label;
}

unsigned      cmApNrtDeviceChannelCount(   unsigned devIdx, bool inputFl )
{
  assert( devIdx < _cmNrt->devCnt );
  const cmApNrtDev_t* dp = _cmApNrtIndexToDev(devIdx);
  return inputFl ? dp->iChCnt : dp->oChCnt;
}

double        cmApNrtDeviceSampleRate(     unsigned devIdx )
{
  assert( devIdx < _cmNrt->devCnt );
  const cmApNrtDev_t* dp = _cmApNrtIndexToDev(devIdx);
  return dp->srate;
}

unsigned      cmApNrtDeviceFramesPerCycle( unsigned devIdx, bool inputFl )
{
  assert( devIdx < _cmNrt->devCnt );
  const cmApNrtDev_t* dp = _cmApNrtIndexToDev(devIdx);
  return dp->fpc;
}

cmApRC_t      cmApNrtDeviceSetup(          
  unsigned          devIdx, 
  double            srate, 
  unsigned          framesPerCycle, 
  cmApCallbackPtr_t callbackPtr,
  void*             userCbPtr )
{
  assert( devIdx < _cmNrt->devCnt );
  cmApNrtDev_t* dp = _cmApNrtIndexToDev(devIdx);
  dp->srate        = srate;
  dp->fpc          = framesPerCycle;
  dp->cbPtr        = callbackPtr;
  dp->cbArg        = userCbPtr;
  return kOkApRC;
}

cmApRC_t      cmApNrtDeviceStart( unsigned devIdx )
{
  assert( devIdx < _cmNrt->devCnt );
  cmApNrtDev_t* dp = _cmApNrtIndexToDev(devIdx);

  dp->curTimeSmp = 0;

  if( cmThreadPause( dp->thH, 0 ) != kOkThRC )
    return cmErrMsg(&_cmNrt->err,kThreadFailApRC,"Thread start failed.");
  
  dp->flags = cmSetFlag(dp->flags,kStartedApNrtFl);
  
  return kOkApRC;
}

cmApRC_t      cmApNrtDeviceStop(  unsigned devIdx )
{
  assert( devIdx < _cmNrt->devCnt );
  cmApNrtDev_t* dp = _cmApNrtIndexToDev(devIdx);

  if( cmThreadPause( dp->thH, kPauseThFl ) != kOkThRC )
    return cmErrMsg(&_cmNrt->err,kThreadFailApRC,"Thread pause failed.");

  dp->flags = cmClrFlag(dp->flags,kStartedApNrtFl);

  return kOkApRC;
}

bool          cmApNrtDeviceIsStarted( unsigned devIdx )
{ 
  assert( devIdx < _cmNrt->devCnt );
  const cmApNrtDev_t* dp = _cmApNrtIndexToDev(devIdx);
  return cmIsFlag(dp->flags,kStartedApNrtFl); 
}
