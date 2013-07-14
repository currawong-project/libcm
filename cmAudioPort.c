#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmAudioPort.h"
#include "cmApBuf.h"  // only needed for cmApBufTest().
#include "cmAudioPortFile.h"
#include "cmAudioAggDev.h"
#include "cmAudioNrtDev.h"

#ifdef OS_LINUX
#include "linux/cmAudioPortAlsa.h"
#endif

#ifdef OS_OSX
#include "osx/cmAudioPortOsx.h"
#endif


typedef struct
{
  unsigned      begDevIdx;
  unsigned      endDevIdx;

  cmApRC_t      (*initialize)( cmRpt_t* rpt, unsigned baseApDevIdx );
  cmApRC_t      (*finalize)();
  cmApRC_t      (*deviceCount)();
  const char*   (*deviceLabel)(          unsigned devIdx );
  unsigned      (*deviceChannelCount)(   unsigned devIdx, bool inputFl );
  double        (*deviceSampleRate)(    unsigned devIdx );
  unsigned      (*deviceFramesPerCycle)( unsigned devIdx, bool inputFl );
  cmApRC_t      (*deviceSetup)( unsigned devIdx, double sr, unsigned frmPerCycle, cmApCallbackPtr_t cb, void* cbData );
  cmApRC_t      (*deviceStart)( unsigned devIdx );
  cmApRC_t      (*deviceStop)(  unsigned devIdx );
  bool          (*deviceIsStarted)( unsigned devIdx );

} cmApDriver_t;

typedef struct
{
  cmErr_t        err;
  cmApDriver_t*  drvArray;
  unsigned       drvCnt;
  unsigned       devCnt;
} cmAp_t;

cmAp_t* _ap = NULL;

cmApRC_t      _cmApIndexToDev( unsigned devIdx, cmApDriver_t** drvPtrPtr, unsigned* devIdxPtr )
{
  assert( drvPtrPtr != NULL && devIdxPtr != NULL );
  *drvPtrPtr = NULL;
  *devIdxPtr = cmInvalidIdx;

  unsigned i;
  for(i=0; i<_ap->drvCnt; ++i)
    if( _ap->drvArray[i].begDevIdx != cmInvalidIdx )
      if( (_ap->drvArray[i].begDevIdx <= devIdx) && (devIdx <= _ap->drvArray[i].endDevIdx) )
      {
        *drvPtrPtr = _ap->drvArray + i;
        *devIdxPtr = devIdx - _ap->drvArray[i].begDevIdx;
        return kOkApRC;
      }
  
  return cmErrMsg(&_ap->err,kInvalidDevIdApRC,"The audio port device index %i is not valid.",devIdx);
}

cmApRC_t      cmApInitialize( cmRpt_t* rpt )
{
  cmApRC_t rc = kOkApRC;
  if((rc = cmApFinalize()) != kOkApRC )
    return rc;

  _ap = cmMemAllocZ(cmAp_t,1);
  cmErrSetup(&_ap->err,rpt,"Audio Port Driver");

  _ap->drvCnt = 4;
  _ap->drvArray = cmMemAllocZ(cmApDriver_t,_ap->drvCnt);
  cmApDriver_t* dp = _ap->drvArray;
  
#ifdef OS_OSX
  dp->initialize           = cmApOsxInitialize;
  dp->finalize             = cmApOsxFinalize;
  dp->deviceCount          = cmApOsxDeviceCount;
  dp->deviceLabel          = cmApOsxDeviceLabel;
  dp->deviceChannelCount   = cmApOsxDeviceChannelCount;
  dp->deviceSampleRate     = cmApOsxDeviceSampleRate;
  dp->deviceFramesPerCycle = cmApOsxDeviceFramesPerCycle;
  dp->deviceSetup          = cmApOsxDeviceSetup;
  dp->deviceStart          = cmApOsxDeviceStart;
  dp->deviceStop           = cmApOsxDeviceStop;
  dp->deviceIsStarted      = cmApOsxDeviceIsStarted;  
#endif

#ifdef OS_LINUX
  dp->initialize           = cmApAlsaInitialize;
  dp->finalize             = cmApAlsaFinalize;
  dp->deviceCount          = cmApAlsaDeviceCount;
  dp->deviceLabel          = cmApAlsaDeviceLabel;
  dp->deviceChannelCount   = cmApAlsaDeviceChannelCount;
  dp->deviceSampleRate     = cmApAlsaDeviceSampleRate;
  dp->deviceFramesPerCycle = cmApAlsaDeviceFramesPerCycle;
  dp->deviceSetup          = cmApAlsaDeviceSetup;
  dp->deviceStart          = cmApAlsaDeviceStart;
  dp->deviceStop           = cmApAlsaDeviceStop;
  dp->deviceIsStarted      = cmApAlsaDeviceIsStarted;  
#endif

  dp = _ap->drvArray + 1;

  dp->initialize           = cmApFileInitialize;
  dp->finalize             = cmApFileFinalize;
  dp->deviceCount          = cmApFileDeviceCount;
  dp->deviceLabel          = cmApFileDeviceLabel;
  dp->deviceChannelCount   = cmApFileDeviceChannelCount;
  dp->deviceSampleRate     = cmApFileDeviceSampleRate;
  dp->deviceFramesPerCycle = cmApFileDeviceFramesPerCycle;
  dp->deviceSetup          = cmApFileDeviceSetup;
  dp->deviceStart          = cmApFileDeviceStart;
  dp->deviceStop           = cmApFileDeviceStop;
  dp->deviceIsStarted      = cmApFileDeviceIsStarted;  

  dp = _ap->drvArray + 2;

  dp->initialize           = cmApAggInitialize;
  dp->finalize             = cmApAggFinalize;
  dp->deviceCount          = cmApAggDeviceCount;
  dp->deviceLabel          = cmApAggDeviceLabel;
  dp->deviceChannelCount   = cmApAggDeviceChannelCount;
  dp->deviceSampleRate     = cmApAggDeviceSampleRate;
  dp->deviceFramesPerCycle = cmApAggDeviceFramesPerCycle;
  dp->deviceSetup          = cmApAggDeviceSetup;
  dp->deviceStart          = cmApAggDeviceStart;
  dp->deviceStop           = cmApAggDeviceStop;
  dp->deviceIsStarted      = cmApAggDeviceIsStarted;  

  dp = _ap->drvArray + 3;

  dp->initialize           = cmApNrtInitialize;
  dp->finalize             = cmApNrtFinalize;
  dp->deviceCount          = cmApNrtDeviceCount;
  dp->deviceLabel          = cmApNrtDeviceLabel;
  dp->deviceChannelCount   = cmApNrtDeviceChannelCount;
  dp->deviceSampleRate     = cmApNrtDeviceSampleRate;
  dp->deviceFramesPerCycle = cmApNrtDeviceFramesPerCycle;
  dp->deviceSetup          = cmApNrtDeviceSetup;
  dp->deviceStart          = cmApNrtDeviceStart;
  dp->deviceStop           = cmApNrtDeviceStop;
  dp->deviceIsStarted      = cmApNrtDeviceIsStarted;  

  _ap->devCnt = 0;

  unsigned i;
  for(i=0; i<_ap->drvCnt; ++i)
  {
    unsigned dn; 
    cmApRC_t rc0;

    _ap->drvArray[i].begDevIdx = cmInvalidIdx;
    _ap->drvArray[i].endDevIdx = cmInvalidIdx;

    if((rc0 = _ap->drvArray[i].initialize(rpt,_ap->devCnt)) != kOkApRC )
    {
      rc = rc0;
      continue;
    }

    if((dn = _ap->drvArray[i].deviceCount()) > 0)
    {
      _ap->drvArray[i].begDevIdx = _ap->devCnt;
      _ap->drvArray[i].endDevIdx = _ap->devCnt + dn - 1;
      _ap->devCnt += dn;
    }
  }

  if( rc != kOkApRC )
    cmApFinalize();

  return rc;
}

cmApRC_t      cmApFinalize()
{
  cmApRC_t rc=kOkApRC;
  cmApRC_t rc0 = kOkApRC;
  unsigned i;

  if( _ap == NULL )
    return kOkApRC;

  for(i=0; i<_ap->drvCnt; ++i)
  {
    if((rc0 = _ap->drvArray[i].finalize()) != kOkApRC )
      rc = rc0;
  }

  cmMemPtrFree(&_ap->drvArray);
  cmMemPtrFree(&_ap);
  return rc;
}


unsigned cmApDeviceCount()
{ return _ap->devCnt; }

const char*   cmApDeviceLabel( unsigned devIdx )
{
  cmApDriver_t* dp = NULL;
  unsigned      di = cmInvalidIdx;
  cmApRC_t      rc;

  if( devIdx == cmInvalidIdx )
    return NULL;

  if((rc = _cmApIndexToDev(devIdx,&dp,&di)) != kOkApRC )
    return cmStringNullGuard(NULL);

  return dp->deviceLabel(di);
}

unsigned      cmApDeviceLabelToIndex( const cmChar_t* label )
{
  unsigned n = cmApDeviceCount();
  unsigned i;
  for(i=0; i<n; ++i)
  {
    const cmChar_t* s = cmApDeviceLabel(i);
    if( s!=NULL && strcmp(s,label)==0)
      return i;
  }
  return cmInvalidIdx;
}


unsigned      cmApDeviceChannelCount(   unsigned devIdx, bool inputFl )
{
  cmApDriver_t* dp = NULL;
  unsigned      di = cmInvalidIdx;
  cmApRC_t      rc;

  if( devIdx == cmInvalidIdx )
    return 0;

  if((rc = _cmApIndexToDev(devIdx,&dp,&di)) != kOkApRC )
    return rc;

  return dp->deviceChannelCount(di,inputFl);
}

double        cmApDeviceSampleRate(     unsigned devIdx )
{
  cmApDriver_t* dp = NULL;
  unsigned      di = cmInvalidIdx;
  cmApRC_t      rc;

  if((rc = _cmApIndexToDev(devIdx,&dp,&di)) != kOkApRC )
    return rc;

  return dp->deviceSampleRate(di);
}

unsigned      cmApDeviceFramesPerCycle( unsigned devIdx, bool inputFl )
{
  cmApDriver_t* dp = NULL;
  unsigned      di = cmInvalidIdx;
  cmApRC_t      rc;

  if( devIdx == cmInvalidIdx )
    return 0;

  if((rc = _cmApIndexToDev(devIdx,&dp,&di)) != kOkApRC )
    return rc;

  return dp->deviceFramesPerCycle(di,inputFl);
}

cmApRC_t      cmApDeviceSetup(          
  unsigned          devIdx, 
  double            srate, 
  unsigned          framesPerCycle, 
  cmApCallbackPtr_t callbackPtr,
  void*             userCbPtr )
{
  cmApDriver_t* dp;
  unsigned      di;
  cmApRC_t      rc;

  if( devIdx == cmInvalidIdx )
    return kOkApRC;

  if((rc = _cmApIndexToDev(devIdx,&dp,&di)) != kOkApRC )
    return rc;

  return dp->deviceSetup(di,srate,framesPerCycle,callbackPtr,userCbPtr);
}

cmApRC_t      cmApDeviceStart( unsigned devIdx )
{
  cmApDriver_t* dp;
  unsigned      di;
  cmApRC_t      rc;

  if( devIdx == cmInvalidIdx )
    return kOkApRC;

  if((rc = _cmApIndexToDev(devIdx,&dp,&di)) != kOkApRC )
    return rc;

  return dp->deviceStart(di);
}

cmApRC_t      cmApDeviceStop(  unsigned devIdx )
{
  cmApDriver_t* dp;
  unsigned      di;
  cmApRC_t      rc;

  if( devIdx == cmInvalidIdx )
    return kOkApRC;

  if((rc = _cmApIndexToDev(devIdx,&dp,&di)) != kOkApRC )
    return rc;

  return dp->deviceStop(di);
}

bool          cmApDeviceIsStarted( unsigned devIdx )
{
  cmApDriver_t* dp;
  unsigned      di;
  cmApRC_t      rc;

  if( devIdx == cmInvalidIdx )
    return false;

  if((rc = _cmApIndexToDev(devIdx,&dp,&di)) != kOkApRC )
    return rc;

  return dp->deviceIsStarted(di);
}



void cmApReport( cmRpt_t* rpt )
{
  unsigned i,j,k;
  for(j=0,k=0; j<_ap->drvCnt; ++j)
  {
    cmApDriver_t* drvPtr = _ap->drvArray + j;
    unsigned      n      = drvPtr->deviceCount(); 

    for(i=0; i<n; ++i,++k)
    {
      cmRptPrintf(rpt, "%i %f in:%i (%i) out:%i (%i) %s\n",
        k, drvPtr->deviceSampleRate(i),
        drvPtr->deviceChannelCount(i,true),  drvPtr->deviceFramesPerCycle(i,true),
        drvPtr->deviceChannelCount(i,false), drvPtr->deviceFramesPerCycle(i,false),
        drvPtr->deviceLabel(i));
  
    }
  }
}

/// [cmAudioPortExample]

// See cmApPortTest() below for the main point of entry.

// Data structure used to hold the parameters for cpApPortTest()
// and the user defined data record passed to the host from the
// audio port callback functions.
typedef struct
{
  unsigned      bufCnt;         // 2=double buffering 3=triple buffering
  unsigned      chIdx;          // first test channel
  unsigned      chCnt;          // count of channels to test
  unsigned      framesPerCycle; // DSP frames per cycle
  unsigned      bufFrmCnt;      // count of DSP frames used by the audio buffer  (bufCnt * framesPerCycle)
  unsigned      bufSmpCnt;      // count of samples used by the audio buffer     (chCnt  * bufFrmCnt)
  unsigned      inDevIdx;       // input device index
  unsigned      outDevIdx;      // output device index
  double        srate;          // audio sample rate
  unsigned      meterMs;        // audio meter buffer length

  // param's and state for cmApSynthSine()
  unsigned      phase;          // sine synth phase
  double        frqHz;          // sine synth frequency in Hz

  // buffer and state for cmApCopyIn/Out()
  cmApSample_t* buf;            // buf[bufSmpCnt] - circular interleaved audio buffer
  unsigned      bufInIdx;       // next input buffer index
  unsigned      bufOutIdx;      // next output buffer index
  unsigned      bufFullCnt;     // count of full samples

  // debugging log data arrays 
  unsigned      logCnt;        // count of elements in log[] and ilong[]
  char*         log;           // log[logCnt]
  unsigned*     ilog;          // ilog[logCnt]
  unsigned      logIdx;        // current log index

  unsigned      cbCnt;         // count the callback
} cmApPortTestRecd;


#ifdef NOT_DEF
// The application can request any block of channels from the device. The packets are provided with the starting
// device channel and channel count.  This function converts device channels and channel counts to buffer
// channel indexes and counts.  
//
//  Example:
//      input                            output
//       i,n                              i n
//  App: 0,4   0 1 2 3                ->  2 2
//  Pkt  2,8       2 3 4 5 6 7 8      ->  0 2
//
// The return value is the count of application requested channels located in this packet.
//
// input: *appChIdxPtr and appChCnt describe a block of device channels requested by the application.
//        *pktChIdxPtr and pktChCnt describe a block of device channels provided to the application
//
// output:*appChIdxPtr and <return value> describe a block of app buffer channels which will send/recv samples.
//        *pktChIdxPtr and <return value>  describe a block of pkt buffer channels which will send/recv samples
//
unsigned _cmApDeviceToBuffer( unsigned* appChIdxPtr, unsigned appChCnt, unsigned* pktChIdxPtr, unsigned pktChCnt )
{
  unsigned abi = *appChIdxPtr;
  unsigned aei = abi+appChCnt-1;

  unsigned pbi = *pktChIdxPtr;
  unsigned pei = pbi+pktChCnt-1;

  // if the ch's rqstd by the app do not overlap with this packet - return false.
  if( aei < pbi || abi > pei )
    return 0;

  // if the ch's rqstd by the app overlap with the beginning of the pkt channel block
  if( abi < pbi )
  {
    appChCnt     -= pbi - abi;
    *appChIdxPtr  = pbi - abi;
    *pktChIdxPtr  = 0;
  }
  else
  {
    // the rqstd ch's begin inside the pkt channel block
    pktChCnt     -= abi - pbi;
    *pktChIdxPtr  = abi - pbi;
    *appChIdxPtr  = 0;
  }

  // if the pkt channels extend beyond the rqstd ch block
  if( aei < pei )
    pktChCnt -= pei - aei;
  else 
    appChCnt -= aei - pei; // the rqstd ch's extend beyond or coincide with the pkt block

  // the returned channel count must always be the same for both the rqstd and pkt 
  return cmMin(appChCnt,pktChCnt);

}


// synthesize a sine signal into an interleaved audio buffer
unsigned _cmApSynthSine( cmApPortTestRecd* r, float* p, unsigned chIdx, unsigned chCnt, unsigned frmCnt, unsigned phs, double hz )
{
  long     ph = 0;
  unsigned i;
  unsigned bufIdx    = r->chIdx;
  unsigned bufChCnt;

  if( (bufChCnt =  _cmApDeviceToBuffer( &bufIdx, r->chCnt, &chIdx, chCnt )) == 0)
    return phs;

  
  //if( r->cbCnt < 50 )
  //  printf("ch:%i cnt:%i  ch:%i cnt:%i  bi:%i bcn:%i\n",r->chIdx,r->chCnt,chIdx,chCnt,bufIdx,bufChCnt);
 

  for(i=bufIdx; i<bufIdx+bufChCnt; ++i)
  {
    unsigned j;
    float*   op = p + i;

    ph = phs;
    for(j=0; j<frmCnt; j++, op+=chCnt, ph++)
    {
      *op = (float)(0.9 * sin( 2.0 * M_PI * hz * ph / r->srate ));
    }
  }
  
  return ph;
}

// Copy the audio samples in the interleaved audio buffer sp[srcChCnt*srcFrameCnt]
// to the internal record buffer.
void _cmApCopyIn( cmApPortTestRecd* r, const cmApSample_t* sp, unsigned srcChIdx, unsigned srcChCnt, unsigned srcFrameCnt  )
{
  unsigned i,j;

  unsigned chCnt = cmMin(r->chCnt,srcChCnt);

  for(i=0; i<srcFrameCnt; ++i)
  {
    for(j=0; j<chCnt; ++j)
      r->buf[ r->bufInIdx + j ] = sp[ (i*srcChCnt) + j ];

    for(; j<r->chCnt; ++j)
      r->buf[ r->bufInIdx + j ] = 0;

    r->bufInIdx = (r->bufInIdx+r->chCnt) % r->bufFrmCnt;
  }

  //r->bufFullCnt = (r->bufFullCnt + srcFrameCnt) % r->bufFrmCnt;
  r->bufFullCnt += srcFrameCnt;
}

// Copy audio samples out of the internal record buffer into dp[dstChCnt*dstFrameCnt].
void _cmApCopyOut( cmApPortTestRecd* r, cmApSample_t* dp, unsigned dstChIdx, unsigned dstChCnt, unsigned dstFrameCnt )
{
  // if there are not enough samples available to fill the destination buffer then zero the dst buf.
  if( r->bufFullCnt < dstFrameCnt )
  {
    printf("Empty Output Buffer\n");
    memset( dp, 0, dstFrameCnt*dstChCnt*sizeof(cmApSample_t) );
  }
  else
  {
    unsigned i,j;
    unsigned chCnt = cmMin(dstChCnt, r->chCnt);

    // for each output frame
    for(i=0; i<dstFrameCnt; ++i)
    {
      // copy the first chCnt samples from the internal buf to the output buf
      for(j=0; j<chCnt; ++j)
        dp[ (i*dstChCnt) + j ] = r->buf[ r->bufOutIdx + j ];

      // zero any output ch's for which there is no internal buf channel
      for(; j<dstChCnt; ++j)
        dp[ (i*dstChCnt) + j ] = 0;

      // advance the internal buffer
      r->bufOutIdx = (r->bufOutIdx + r->chCnt) % r->bufFrmCnt;
    }

    r->bufFullCnt -= dstFrameCnt;
  }
}

// Audio port callback function called from the audio device thread.
void _cmApPortCb( cmApAudioPacket_t* inPktArray, unsigned inPktCnt, cmApAudioPacket_t* outPktArray, unsigned outPktCnt )
{
  unsigned i;

  // for each incoming audio packet
  for(i=0; i<inPktCnt; ++i)
  {
    cmApPortTestRecd* r = (cmApPortTestRecd*)inPktArray[i].userCbPtr; 

    if( inPktArray[i].devIdx == r->inDevIdx )
    {
      // copy the incoming audio into an internal buffer where it can be picked up by _cpApCopyOut().
      _cmApCopyIn( r, (cmApSample_t*)inPktArray[i].audioBytesPtr, inPktArray[i].begChIdx, inPktArray[i].chCnt, inPktArray[i].audioFramesCnt );
    }
    ++r->cbCnt;

    //printf("i %4i in:%4i out:%4i\n",r->bufFullCnt,r->bufInIdx,r->bufOutIdx);
  }

  unsigned hold_phase = 0;

  // for each outgoing audio packet
  for(i=0; i<outPktCnt; ++i)
  {
    cmApPortTestRecd* r = (cmApPortTestRecd*)outPktArray[i].userCbPtr; 

    if( outPktArray[i].devIdx == r->outDevIdx )
    {
      // zero the output buffer
      memset(outPktArray[i].audioBytesPtr,0,outPktArray[i].chCnt * outPktArray[i].audioFramesCnt * sizeof(cmApSample_t) );
      
      // if the synth is enabled
      if( r->synthFl )
      {
        unsigned tmp_phase  = _cmApSynthSine( r, outPktArray[i].audioBytesPtr, outPktArray[i].begChIdx, outPktArray[i].chCnt, outPktArray[i].audioFramesCnt, r->phase, r->frqHz );  

        // the phase will only change on packets that are actually used
        if( tmp_phase != r->phase )
          hold_phase = tmp_phase;
      }
      else
      {
        // copy the any audio in the internal record buffer to the playback device 
        _cmApCopyOut( r, (cmApSample_t*)outPktArray[i].audioBytesPtr, outPktArray[i].begChIdx, outPktArray[i].chCnt, outPktArray[i].audioFramesCnt );   
      }
    }

    r->phase = hold_phase;

    //printf("o %4i in:%4i out:%4i\n",r->bufFullCnt,r->bufInIdx,r->bufOutIdx);
    // count callbacks
    ++r->cbCnt;
  }
}
#endif

// print the usage message for cmAudioPortTest.c
void _cmApPrintUsage( cmRpt_t* rpt )
{
char msg[] =
  "cmApPortTest() command switches\n"
  "-r <srate> -c <chcnt> -b <bufcnt> -f <frmcnt> -i <idevidx> -o <odevidx> -t -p -h \n"
  "\n"
  "-r <srate> = sample rate\n"
  "-a <chidx> = first channel\n"
  "-c <chcnt> = audio channels\n"
  "-b <bufcnt> = count of buffers\n"
  "-f <frmcnt> = count of samples per buffer\n"
  "-i <idevidx> = input device index\n"
  "-o <odevidx> = output device index\n"
  "-p = print report but do not start audio devices\n"
  "-h = print this usage message\n";

 cmRptPrintf(rpt,msg);
}

// Get a command line option.
int _cmApGetOpt( int argc, const char* argv[], const char* label, int defaultVal, bool boolFl )
{
  int i = 0;
  for(; i<argc; ++i)
    if( strcmp(label,argv[i]) == 0 )
    {
      if(boolFl)
        return 1;

      if( i == (argc-1) )
        return defaultVal;

      return atoi(argv[i+1]);
    }
  
  return defaultVal;
}

unsigned _cmGlobalInDevIdx  = 0;
unsigned _cmGlobalOutDevIdx = 0;

void _cmApPortCb2( cmApAudioPacket_t* inPktArray, unsigned inPktCnt, cmApAudioPacket_t* outPktArray, unsigned outPktCnt )
{
  cmApBufInputToOutput( _cmGlobalInDevIdx, _cmGlobalOutDevIdx );

  cmApBufUpdate( inPktArray, inPktCnt, outPktArray, outPktCnt );
}

// Audio Port testing function
int cmApPortTest( bool runFl, cmRpt_t* rpt, int argc, const char* argv[] )
{
  cmApPortTestRecd r;
  unsigned         i;
  int              result = 0;

  if( _cmApGetOpt(argc,argv,"-h",0,true) )
    _cmApPrintUsage(rpt);


  runFl            = _cmApGetOpt(argc,argv,"-p",!runFl,true)?false:true;
  r.chIdx          = _cmApGetOpt(argc,argv,"-a",0,false);
  r.chCnt          = _cmApGetOpt(argc,argv,"-c",2,false);
  r.bufCnt         = _cmApGetOpt(argc,argv,"-b",3,false);
  r.framesPerCycle = _cmApGetOpt(argc,argv,"-f",512,false);
  r.bufFrmCnt      = (r.bufCnt*r.framesPerCycle);
  r.bufSmpCnt      = (r.chCnt  * r.bufFrmCnt);
  r.logCnt         = 100; 
  r.meterMs        = 50;

  cmApSample_t buf[r.bufSmpCnt];
  char         log[r.logCnt];
  unsigned    ilog[r.logCnt];
  
  r.inDevIdx   = _cmGlobalInDevIdx  = _cmApGetOpt(argc,argv,"-i",0,false);   
  r.outDevIdx  = _cmGlobalOutDevIdx = _cmApGetOpt(argc,argv,"-o",2,false); 
  r.phase      = 0;
  r.frqHz      = 2000;
  r.srate      = 44100;
  r.bufInIdx   = 0;
  r.bufOutIdx  = 0;
  r.bufFullCnt = 0;
  r.logIdx     = 0;

  r.buf        = buf;
  r.log        = log;
  r.ilog       = ilog;
  r.cbCnt      = 0;

  cmRptPrintf(rpt,"%s in:%i out:%i chidx:%i chs:%i bufs=%i frm=%i rate=%f\n",runFl?"exec":"rpt",r.inDevIdx,r.outDevIdx,r.chIdx,r.chCnt,r.bufCnt,r.framesPerCycle,r.srate);

  if( cmApFileAllocate(rpt) != kOkApRC )
  {
    cmRptPrintf(rpt,"Audio port file allocation failed.");
    result = -1;
    goto errLabel;
  }

  // allocate the non-real-time port
  if( cmApNrtAllocate(rpt) != kOkApRC )
  {
    cmRptPrintf(rpt,"Non-real-time system allocation failed.");
    result = 1;
    goto errLabel;
  }

  // initialize the audio device interface
  if( cmApInitialize(rpt) != kOkApRC )
  {
    cmRptPrintf(rpt,"Initialize failed.\n");
    result = 1;
    goto errLabel;
  }
  
  // report the current audio device configuration
  for(i=0; i<cmApDeviceCount(); ++i)
  {
    cmRptPrintf(rpt,"%i [in: chs=%i frames=%i] [out: chs=%i frames=%i] srate:%f %s\n",i,cmApDeviceChannelCount(i,true),cmApDeviceFramesPerCycle(i,true),cmApDeviceChannelCount(i,false),cmApDeviceFramesPerCycle(i,false),cmApDeviceSampleRate(i),cmApDeviceLabel(i));
  }
  // report the current audio devices using the audio port interface function
  cmApReport(rpt);

  if( runFl )
  {
    // initialize the audio bufer
    cmApBufInitialize( cmApDeviceCount(), r.meterMs );

    // setup the buffer for the output device
    cmApBufSetup( r.outDevIdx, r.srate, r.framesPerCycle, r.bufCnt, cmApDeviceChannelCount(r.outDevIdx,true), r.framesPerCycle, cmApDeviceChannelCount(r.outDevIdx,false), r.framesPerCycle );

    // setup the buffer for the input device
    if( r.inDevIdx != r.outDevIdx )
      cmApBufSetup( r.inDevIdx, r.srate, r.framesPerCycle, r.bufCnt, cmApDeviceChannelCount(r.inDevIdx,true), r.framesPerCycle, cmApDeviceChannelCount(r.inDevIdx,false), r.framesPerCycle ); 

    // setup an output device
    if(cmApDeviceSetup(r.outDevIdx,r.srate,r.framesPerCycle,_cmApPortCb2,&r) != kOkApRC )
      cmRptPrintf(rpt,"Out device setup failed.\n");
    else
      // setup an input device
      if( cmApDeviceSetup(r.inDevIdx,r.srate,r.framesPerCycle,_cmApPortCb2,&r) != kOkApRC )
        cmRptPrintf(rpt,"In device setup failed.\n");
      else
        // start the input device
        if( cmApDeviceStart(r.inDevIdx) != kOkApRC )
          cmRptPrintf(rpt,"In device start failed.\n");
        else
          // start the output device
          if( cmApDeviceStart(r.outDevIdx) != kOkApRC )
            cmRptPrintf(rpt,"Out Device start failed.\n");

    
    cmRptPrintf(rpt,"q=quit O/o output tone, I/i input tone P/p pass\n");
    char c;
    while((c=getchar()) != 'q')
    {
      //cmApAlsaDeviceRtReport(rpt,r.outDevIdx);

      switch(c)
      {
        case 'i':
        case 'I':
          cmApBufEnableTone(r.inDevIdx,-1,kInApFl | (c=='I'?kEnableApFl:0));
          break;

        case 'o':
        case 'O':
          cmApBufEnableTone(r.outDevIdx,-1,kOutApFl | (c=='O'?kEnableApFl:0));
          break;

        case 'p':
        case 'P':
          cmApBufEnablePass(r.outDevIdx,-1,kOutApFl | (c=='P'?kEnableApFl:0));
          break;
          
        case 's':
          cmApBufReport(rpt);
          break;
      }

    }

    // stop the input device
    if( cmApDeviceIsStarted(r.inDevIdx) )
      if( cmApDeviceStop(r.inDevIdx) != kOkApRC )
        cmRptPrintf(rpt,"In device stop failed.\n");

    // stop the output device
    if( cmApDeviceIsStarted(r.outDevIdx) )
      if( cmApDeviceStop(r.outDevIdx) != kOkApRC )
        cmRptPrintf(rpt,"Out device stop failed.\n");
  }

 errLabel:

  // release any resources held by the audio port interface
  if( cmApFinalize() != kOkApRC )
    cmRptPrintf(rpt,"Finalize failed.\n");

  cmApBufFinalize();

  cmApNrtFree();
  cmApFileFree();

  // report the count of audio buffer callbacks
  cmRptPrintf(rpt,"cb count:%i\n", r.cbCnt );
  //for(i=0; i<_logCnt; ++i)
  //  cmRptPrintf(rpt,"%c(%i)",_log[i],_ilog[i]);
  //cmRptPrintf(rpt,"\n");


  return result;
}

/// [cmAudioPortExample]



