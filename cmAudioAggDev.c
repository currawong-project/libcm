#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmAudioPort.h"
#include "cmAudioAggDev.h"
#include "cmThread.h" // cmThUIntIncr()

#include "cmApBuf.h"  // only needed for cmApBufTest().

//#include <unistd.h> // usleep

enum
{
  kBufArrayCnt = 2
};

struct cmApAgg_str;

typedef struct 
{
  unsigned             physDevIdx;
  struct cmApAgg_str*  ap;

  unsigned             iChIdx;
  unsigned             iChCnt;

  unsigned             oChIdx;
  unsigned             oChCnt;
} cmApAggDev_t;

typedef struct cmApAgg_str
{
  cmChar_t*           label;          // agg. device label
  unsigned            aggDevIdx;      // agg. device index
  unsigned            sysDevIdx;      // system device index
  unsigned            devCnt;         // count of phys devices 
  cmApAggDev_t*       devArray;       // devArray[ devCnt ] - physical device array
  unsigned            iChCnt;         // sum of phys device input channels
  unsigned            oChCnt;         // sum of phys device output channels
  double              srate;          // agg. dev sample rate
  unsigned            framesPerCycle; // agg. dev frames per cycle
  unsigned            flags;          // kAgInFl | kAgOutFl
  cmApCallbackPtr_t   cbFunc;         // client supplied callback func
  void*               cbArg;          // client supplied callback func arg.
  bool                startedFl;      // true if the agg. device is started
  struct cmApAgg_str* link;           // _cmAg.list link
} cmApAgg_t;

typedef struct
{
  cmErr_t    err;
  cmApAgg_t* list;
} cmApAggMain_t;

cmApAggMain_t _cmAg;


void _cmApAggCb( cmApAudioPacket_t* inPktArray, unsigned inPktCnt, cmApAudioPacket_t* outPktArray, unsigned outPktCnt )
{
  unsigned          i;
  cmApAudioPacket_t pkt;

  for(i=0; i<inPktCnt; ++i)
  {
    cmApAggDev_t* dp = (cmApAggDev_t*)inPktArray[i].userCbPtr;  
    pkt           = inPktArray[i];
    pkt.devIdx    = dp->ap->sysDevIdx;
    pkt.begChIdx  = dp->iChIdx;
    pkt.userCbPtr = dp->ap->cbArg;
    dp->ap->cbFunc( &pkt, 1, NULL, 0 );    
  }

  for(i=0; i<outPktCnt; ++i)
  {
    cmApAggDev_t* dp = (cmApAggDev_t*)outPktArray[i].userCbPtr;  
    pkt           = outPktArray[i];
    pkt.devIdx    = dp->ap->sysDevIdx;
    pkt.begChIdx  = dp->oChIdx;
    pkt.userCbPtr = dp->ap->cbArg;
    dp->ap->cbFunc( NULL, 0, &pkt, 1 );        
  }

}


void _cmApAgDeleteAggDev( cmApAgg_t* ap )
{
  cmApAgg_t* cp = _cmAg.list;
  cmApAgg_t* pp = NULL;
  while( cp != NULL )
  {    
    if( cp == ap )
    {
      if( pp == NULL )
        _cmAg.list = cp->link;
      else
        pp->link = cp->link;

      cmMemFree(ap->label);
      cmMemFree(ap->devArray);
      cmMemFree(ap);
      return;
    }
    pp = cp;
    cp = cp->link;
  }
}

cmAgRC_t      cmApAggAllocate( cmRpt_t* rpt )
{
  cmAgRC_t rc = kOkAgRC;

  cmErrSetup(&_cmAg.err,rpt,"cmAudioAggDev");

  _cmAg.list = NULL;
  
  return rc;
}

cmAgRC_t      cmApAggFree()
{
  cmAgRC_t rc = kOkAgRC;

  while( _cmAg.list != NULL )
    _cmApAgDeleteAggDev(_cmAg.list );

  return rc;
}

cmAgRC_t      cmApAggInitialize( cmRpt_t* rpt, unsigned baseApDevIdx )
{ 
  cmApAgg_t* ap = _cmAg.list;
  unsigned i;

  assert( baseApDevIdx == cmApDeviceCount() );

  for(i=0; ap!=NULL; ap=ap->link,++i)
  {
    ap->sysDevIdx = cmApDeviceCount() + i;
    ap->iChCnt    = 0;
    ap->oChCnt    = 0;

    unsigned i;
    for(i=0; i<ap->devCnt; ++i)
    {
      ap->devArray[i].iChIdx  = ap->iChCnt;
      ap->devArray[i].oChIdx  = ap->oChCnt;
      ap->devArray[i].iChCnt  = cmApDeviceChannelCount(ap->devArray[i].physDevIdx,true);
      ap->devArray[i].oChCnt  = cmApDeviceChannelCount(ap->devArray[i].physDevIdx,false);
      ap->iChCnt             += ap->devArray[i].iChCnt;
      ap->oChCnt             += ap->devArray[i].oChCnt;
    }

    
  }

  return kOkAgRC;
}

cmAgRC_t      cmApAggFinalize()
{ return kOkAgRC; }

cmAgRC_t      cmApAggCreateDevice(
  const cmChar_t* label,
  unsigned       devCnt,
  const unsigned physDevIdxArray[],
  unsigned       flags )
{
  cmAgRC_t rc = kOkAgRC;
  unsigned i;

  if( devCnt < 2 )
    return cmErrMsg(&_cmAg.err,kMustAggTwoAgRC,"Cannot aggregate less than two devices.");
  /*

  for(i=0; i<devCnt; ++i)
  {
    unsigned physDevIdx = physDevIdxArray[i];

    if( cmApAggIsDeviceAggregated(physDevIdx) )
      return cmErrMsg(&_cmAg.err,kDevAlreadyAggAgRC,"The physical device associated with index '%i' ('%s') has already been assigned to another aggregated device.",physDevIdx,cmStringNullGuard(cmApDeviceLabel(physDevIdx)));

    if( cmApDeviceIsStarted(physDevIdx) )
      return cmErrMsg(&_cmAg.err,kCantUseStartedDevAgRC,"The physical device associated with index '%i' ('%s') cannot be aggregated while it is running.",physDevIdx,cmStringNullGuard(cmApDeviceLabel(physDevIdx)));

  }
  */

  cmApAgg_t* ap            = cmMemAllocZ(cmApAgg_t,1);
  ap->label                = cmMemAllocStr(label==NULL?"Aggregated Device":label);
  ap->devArray             = cmMemAllocZ(cmApAggDev_t,devCnt);
  ap->aggDevIdx            = cmApAggDeviceCount();
  ap->sysDevIdx            = cmInvalidIdx;
  ap->devCnt               = devCnt;
  ap->iChCnt               = 0;
  ap->oChCnt               = 0;

  for(i=0; i<devCnt; ++i)
  {
    ap->devArray[i].ap          = ap;
    ap->devArray[i].physDevIdx  = physDevIdxArray[i];
  }

  ap->link = _cmAg.list;
  _cmAg.list = ap;

  return rc;
}

cmApAgg_t* _cmApAggDevIdxToPtr( unsigned aggDevIdx )
{
  cmApAgg_t* ap = _cmAg.list;
  unsigned   i  = 0;
  for(; ap!=NULL; ap=ap->link,++i)
    if( ap->aggDevIdx == aggDevIdx )
      return ap;
  return NULL;
}

cmAgRC_t  _cmApAggGetAgg( unsigned aggDevIdx, cmApAgg_t** retPtrPtr )
{
  if((*retPtrPtr = _cmApAggDevIdxToPtr(aggDevIdx)) == NULL )
    return cmErrMsg(&_cmAg.err,kInvalidDevIdxAgRC,"The aggregate system device index '%i' is invalid.");
  return kOkAgRC;
}


bool cmApAggIsDeviceAggregated( unsigned physDevIdx )
{
  cmApAgg_t* ap = _cmAg.list;
  for(; ap!=NULL; ap=ap->link)
  {
    unsigned i;
    for(i=0; i<ap->devCnt; ++i)
      if( ap->devArray[i].physDevIdx == physDevIdx )
        return true;
  }
  return false;
}

cmAgRC_t      cmApAggDeviceCount()
{
  unsigned devCnt=0;
  cmApAgg_t* ap = _cmAg.list;
  for(; ap!=NULL; ap=ap->link)
    ++devCnt;

  return devCnt;
}

const char*   cmApAggDeviceLabel(          unsigned aggDevIdx )
{
  cmApAgg_t* ap;
  cmAgRC_t   rc;
  if((rc = _cmApAggGetAgg(aggDevIdx, &ap )) == kOkAgRC )
    return ap->label;    
  return NULL;
}

unsigned      cmApAggDeviceChannelCount(   unsigned aggDevIdx, bool inputFl )
{
  cmApAgg_t* ap;
  cmAgRC_t   rc;
  if((rc = _cmApAggGetAgg(aggDevIdx, &ap )) == kOkAgRC )
    return inputFl ? ap->iChCnt : ap->oChCnt;    
  return 0;
}

double        cmApAggDeviceSampleRate(     unsigned aggDevIdx )
{
  cmApAgg_t* ap;
  cmAgRC_t   rc;
  if((rc = _cmApAggGetAgg(aggDevIdx, &ap )) == kOkAgRC )
    return ap->srate;
  return 0;  
}

unsigned      cmApAggDeviceFramesPerCycle( unsigned aggDevIdx, bool inputFl )
{
  cmApAgg_t* ap;
  cmAgRC_t   rc;
  if((rc = _cmApAggGetAgg(aggDevIdx, &ap )) == kOkAgRC )
    return ap->framesPerCycle;
  return 0;  
}

cmAgRC_t      cmApAggDeviceSetup(          
  unsigned             aggDevIdx, 
  double               srate, 
  unsigned             framesPerCycle, 
  cmApCallbackPtr_t    callbackPtr,
  void*                userCbPtr )
{
  cmApAgg_t* ap;
  cmAgRC_t   rc;
  unsigned   i;

  if((rc = _cmApAggGetAgg(aggDevIdx, &ap )) != kOkAgRC )
    return rc;

  if((rc = cmApAggDeviceStop(aggDevIdx)) != kOkAgRC )
    return rc;

  for(i=0; i<ap->devCnt; ++i)
  {
    unsigned      physDevIdx = ap->devArray[i].physDevIdx;
    cmApAggDev_t* devPtr     = ap->devArray + i;

    if( cmApDeviceSetup( physDevIdx, srate, framesPerCycle, _cmApAggCb, devPtr ) != kOkApRC )
      rc = cmErrMsg(&_cmAg.err,kPhysDevSetupFailAgRC,"The physical device (index:%i '%s') setup failed for sample rate:%f frames-per-cycle:%i.",physDevIdx,cmStringNullGuard(cmApDeviceLabel(physDevIdx)),srate,framesPerCycle);
  }

  if( rc == kOkAgRC )
  {
    ap->cbFunc      = callbackPtr;
    ap->cbArg       = userCbPtr;
  }

  return rc;
}

cmAgRC_t      cmApAggDeviceStart( unsigned aggDevIdx )
{
  cmAgRC_t   rc = kOkAgRC;
  cmApAgg_t* ap;
  unsigned   i;

  if((rc = _cmApAggGetAgg(aggDevIdx, &ap )) != kOkAgRC )
    return rc;
  
  for(i=0; i<ap->devCnt; ++i)
  {
    unsigned physDevIdx = ap->devArray[i].physDevIdx;

    if( cmApDeviceStart( physDevIdx ) != kOkApRC )
      return cmErrMsg(&_cmAg.err,kPhysDevStartFailAgRC,"The physical device (index:%i '%s') start failed.",physDevIdx,cmStringNullGuard(cmApDeviceLabel(physDevIdx)));

    //usleep(1000);
  }

  ap->startedFl = true;

  return rc;
}

cmAgRC_t      cmApAggDeviceStop(  unsigned aggDevIdx )
{
  cmAgRC_t   rc = kOkAgRC;
  cmApAgg_t* ap;
  unsigned   i;

  if((rc = _cmApAggGetAgg(aggDevIdx, &ap )) != kOkAgRC )
    return rc;
  
  for(i=0; i<ap->devCnt; ++i)
  {
    unsigned physDevIdx = ap->devArray[i].physDevIdx;

    if( cmApDeviceStop( physDevIdx ) != kOkApRC )
      return cmErrMsg(&_cmAg.err,kPhysDevStartFailAgRC,"The physical device (index:%i '%s') start failed.",physDevIdx,cmStringNullGuard(cmApDeviceLabel(physDevIdx)));
  }

  ap->startedFl = false;

  return rc;
}

bool          cmApAggDeviceIsStarted( unsigned aggDevIdx )
{
  cmApAgg_t* ap;

  if(_cmApAggGetAgg(aggDevIdx, &ap ) != kOkAgRC )
    return false;

  return ap->startedFl;
}






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
  bool          synthFl;
  unsigned      phase;          // sine synth phase
  double        frqHz;          // sine synth frequency in Hz

  // buffer and state for cmApCopyIn/Out()
  cmApSample_t* buf;            // buf[bufSmpCnt] - circular interleaved audio buffer
  unsigned      bufInIdx;       // next input buffer index
  unsigned      bufOutIdx;      // next output buffer index
  unsigned      bufFullCnt;     // count of full samples

  unsigned      cbCnt;         // count the callback
  unsigned      underunCnt;    // 
  unsigned      overunCnt;

  double*        iMeter;         // iMeter[ chCnt ]

  FILE* ifp;
  FILE* ofp;
} cmApAggPortTestRecd;

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
unsigned _cmApAggDeviceToBuffer( unsigned* appChIdxPtr, unsigned appChCnt, unsigned* pktChIdxPtr, unsigned pktChCnt )
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
unsigned _cmApAggSynthSine( cmApAggPortTestRecd* r, float* p, unsigned chIdx, unsigned chCnt, unsigned frmCnt, unsigned phs, double hz )
{
  long     ph = 0;
  unsigned i;
  unsigned bufIdx    = r->chIdx;
  unsigned bufChCnt;

  if( (bufChCnt =  _cmApAggDeviceToBuffer( &bufIdx, r->chCnt, &chIdx, chCnt )) == 0)
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
void _cmApAggCopyIn( cmApAggPortTestRecd* r, const cmApSample_t* sp, unsigned srcChIdx, unsigned srcChCnt, unsigned srcFrameCnt  )
{
  unsigned i,j;

  unsigned chCnt = cmMin(r->chCnt,srcChCnt);

  // write the incoming sample to an output file for debugging
  if( r->ifp != NULL )
    if( fwrite(sp,sizeof(cmApSample_t),srcChCnt*srcFrameCnt,r->ifp) != srcChCnt*srcFrameCnt )
      printf("file write fail.\n");

  // zero the input meter array
  for(i=0; i<r->chCnt; ++i)
    r->iMeter[i] = 0;

  for(i=0; i<srcFrameCnt; ++i)
  {
    // copy samples from the src to the buffer - both src and buffer are interleaved
    for(j=0; j<chCnt; ++j)
    {
      r->buf[ r->bufInIdx + j ] = sp[ (i*srcChCnt) + srcChIdx + j ];
      
      // record the max value in the input meter array
      if( r->buf[ r->bufInIdx + j ] > r->iMeter[j] )
        r->iMeter[j] = r->buf[ r->bufInIdx + j ];
    }

    // zero channels that are not used in the buffer
    for(; j<r->chCnt; ++j)
      r->buf[ r->bufInIdx + j ] = 0;

    // advance to the next frame
    r->bufInIdx = (r->bufInIdx+r->chCnt) % r->bufFrmCnt;
  }

  //r->bufFullCnt = (r->bufFullCnt + srcFrameCnt) % r->bufFrmCnt;
  cmThUIntIncr(&r->bufFullCnt,srcFrameCnt);

  if( r->bufFullCnt > r->bufFrmCnt )
  {
    //printf("Input buffer overrun.\n");
    ++r->overunCnt;
    r->bufFullCnt = 0;
  }

}

// Copy audio samples out of the internal record buffer into dp[dstChCnt*dstFrameCnt].
void _cmApAggCopyOut( cmApAggPortTestRecd* r, cmApSample_t* dp, unsigned dstChIdx, unsigned dstChCnt, unsigned dstFrameCnt )
{
 
  // if there are not enough samples available to fill the destination 
  // buffer then zero the dst buf.
  if( r->bufFullCnt < dstFrameCnt )
  {
    //printf("Empty Output Buffer %i < %i\n",r->bufFullCnt,dstFrameCnt);
    memset( dp, 0, dstFrameCnt*dstChCnt*sizeof(cmApSample_t) );
    ++r->underunCnt;
  }
  else
  {
    unsigned i,j;
    unsigned chCnt = cmMin(dstChCnt,r->chCnt);

    for(i=0; i<dstFrameCnt; ++i)
    {
      // copy the stored buffer samples to the dst buffer
      for(j=0; j<chCnt; ++j)
        dp[ (i*dstChCnt) + dstChIdx + j ] = r->buf[ r->bufOutIdx + j ];

      // zero unset channels in the dst buffer
      for(; j<dstChCnt; ++j)
        dp[ (i*dstChCnt) + dstChIdx + j ] = 0;

      r->bufOutIdx = (r->bufOutIdx + r->chCnt) % r->bufFrmCnt;
    }

    cmThUIntDecr(&r->bufFullCnt,dstFrameCnt);
  }

  if( r->ofp != NULL )
    fwrite(dp,sizeof(cmApSample_t),dstChCnt*dstFrameCnt,r->ofp);
}

// Audio port callback function called from the audio device thread.
void _cmApAggPortCb( cmApAudioPacket_t* inPktArray, unsigned inPktCnt, cmApAudioPacket_t* outPktArray, unsigned outPktCnt )
{
  unsigned i;

  // for each incoming audio packet

  for(i=0; i<inPktCnt; ++i)
  {
    cmApAggPortTestRecd* r = (cmApAggPortTestRecd*)inPktArray[i].userCbPtr; 

    if( r->synthFl==false && inPktArray[i].devIdx == r->inDevIdx )
    {
      // copy the incoming audio into an internal buffer where it can be picked up by _cpApCopyOut().
      _cmApAggCopyIn( r, (cmApSample_t*)inPktArray[i].audioBytesPtr, inPktArray[i].begChIdx, inPktArray[i].chCnt, inPktArray[i].audioFramesCnt );
    }
    ++r->cbCnt;

    //printf("i %4i in:%4i out:%4i\n",r->bufFullCnt,r->bufInIdx,r->bufOutIdx);
  }

  unsigned hold_phase = 0;

  // for each outgoing audio packet
  for(i=0; i<outPktCnt; ++i)
  {
    cmApAggPortTestRecd* r = (cmApAggPortTestRecd*)outPktArray[i].userCbPtr; 

    if( outPktArray[i].devIdx == r->outDevIdx )
    {
      // zero the output buffer
      memset(outPktArray[i].audioBytesPtr,0,outPktArray[i].chCnt * outPktArray[i].audioFramesCnt * sizeof(cmApSample_t) );
      
      // if the synth is enabled
      if( r->synthFl )
      {
        unsigned tmp_phase  = _cmApAggSynthSine( r, outPktArray[i].audioBytesPtr, outPktArray[i].begChIdx, outPktArray[i].chCnt, outPktArray[i].audioFramesCnt, r->phase, r->frqHz );  

        // the phase will only change on packets that are actually used
        if( tmp_phase != r->phase )
          hold_phase = tmp_phase;
      }
      else
      {
        // copy the any audio in the internal record buffer to the playback device 
        _cmApAggCopyOut( r, (cmApSample_t*)outPktArray[i].audioBytesPtr, outPktArray[i].begChIdx, outPktArray[i].chCnt, outPktArray[i].audioFramesCnt );   
      }
    }

    r->phase = hold_phase;

    //printf("o %4i in:%4i out:%4i\n",r->bufFullCnt,r->bufInIdx,r->bufOutIdx);
    // count callbacks
    ++r->cbCnt;
  }
}


// print the usage message for cmAudioPortTest.c
void _cmApAggPrintUsage( cmRpt_t* rpt )
{
char msg[] =
  "cmApAggPortTest() command switches\n"
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
int _cmApAggGetOpt( int argc, const char* argv[], const char* label, int defaultVal, bool boolFl )
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


void _cmApBufShowMeter( cmRpt_t* rpt, unsigned devIdx )
{
  unsigned faultCnt = 0;
  unsigned meterCnt = cmApBufChannelCount(devIdx,kInApFl);
  double   meterArray[ meterCnt ];

  unsigned n = cmApBufGetStatus(devIdx, kInApFl, meterArray, meterCnt, &faultCnt );
  unsigned i;

  cmRptPrintf(rpt,"In: actual:%i fault: %i : ",n,faultCnt);
  for(i=0; i<meterCnt; ++i)
    cmRptPrintf(rpt,"%i:%f ",i,meterArray[i]);
  cmRptPrintf(rpt,"\n");
}


unsigned _cmAggGlobalInDevIdx  = 0;
unsigned _cmAggGlobalOutDevIdx = 0;

void _cmApAggPortCb2( cmApAudioPacket_t* inPktArray, unsigned inPktCnt, cmApAudioPacket_t* outPktArray, unsigned outPktCnt )
{

  cmApBufInputToOutput( _cmAggGlobalInDevIdx, _cmAggGlobalOutDevIdx );

  cmApBufUpdate( inPktArray, inPktCnt, outPktArray, outPktCnt );
}


  void recdPrint();

// Audio Port testing function
int cmApAggTest( bool runFl, cmCtx_t* ctx, int argc, const char* argv[] )
{
  cmApAggPortTestRecd r;
  unsigned         i;
  cmRpt_t* rpt = &ctx->rpt;

  if( _cmApAggGetOpt(argc,argv,"-h",0,true) )
    _cmApAggPrintUsage(rpt);


  runFl            = _cmApAggGetOpt(argc,argv,"-p",!runFl,true)?false:true;
  r.chIdx          = _cmApAggGetOpt(argc,argv,"-a",0,false);
  r.chCnt          = _cmApAggGetOpt(argc,argv,"-c",2,false);
  r.bufCnt         = _cmApAggGetOpt(argc,argv,"-b",3,false);
  r.framesPerCycle = _cmApAggGetOpt(argc,argv,"-f",512,false);
  r.bufFrmCnt      = (r.bufCnt*r.framesPerCycle);
  r.bufSmpCnt      = (r.chCnt  * r.bufFrmCnt);
  r.synthFl        = false;
  r.meterMs        = 50;

  cmApSample_t buf[r.bufSmpCnt];
  double      imeter[r.chCnt];

  r.iMeter     = imeter;      
  
  r.inDevIdx   =  _cmAggGlobalInDevIdx = _cmApAggGetOpt(argc,argv,"-i",0,false);   
  r.outDevIdx  =  _cmAggGlobalOutDevIdx = _cmApAggGetOpt(argc,argv,"-o",2,false); 
  r.phase      = 0;
  r.frqHz      = 2000;
  r.srate      = 44100;
  r.bufInIdx   = 0;
  r.bufOutIdx  = 0;
  r.bufFullCnt = 0;

  r.buf        = buf;
  r.cbCnt      = 0;
  r.underunCnt = 0;
  r.overunCnt  = 0;
  r.ifp        = NULL;
  r.ofp        = NULL;

  
  if(0)
  {
    if((r.ifp = fopen("/home/kevin/temp/itemp0.bin","wb")) == NULL )
      cmRptPrintf(rpt,"File open failed.\n");

    if((r.ofp = fopen("/home/kevin/temp/otemp0.bin","wb")) == NULL )
      cmRptPrintf(rpt,"File open failed.\n");
  }

  cmRptPrintf(rpt,"%s in:%i out:%i chidx:%i chs:%i bufs=%i frm=%i rate=%f\n",runFl?"exec":"rpt",r.inDevIdx,r.outDevIdx,r.chIdx,r.chCnt,r.bufCnt,r.framesPerCycle,r.srate);

  // allocate the aggregate device system
  if( cmApAggAllocate(rpt) != kOkAgRC )
  {
    cmRptPrintf(rpt,"The aggregate device system allocation failed.\n");
    return 1;
  }


  unsigned physDevIdxArray[] = { 0, 1 };
  unsigned physDevCnt = sizeof(physDevIdxArray)/sizeof(physDevIdxArray[0]);
  if( cmApAggCreateDevice("aggdev",physDevCnt,physDevIdxArray,kInAggFl | kOutAggFl) != kOkAgRC )
  {
    cmRptPrintf(rpt,"The aggregate device creation failed.n");
    goto doneLabel;
  }


  // initialize the audio device interface
  if( cmApInitialize(rpt) != kOkApRC )
  {
    cmRptPrintf(rpt,"Initialize failed.\n");
    goto doneLabel;
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

    // setup an input device
    if( cmApDeviceSetup(r.inDevIdx,r.srate,r.framesPerCycle,_cmApAggPortCb2,&r) != kOkApRC )
    {
      cmRptPrintf(rpt,"In device setup failed.\n");
      goto errLabel;
    }

    // setup an output device
    if( r.inDevIdx != r.outDevIdx )
    {
      if(cmApDeviceSetup(r.outDevIdx,r.srate,r.framesPerCycle,_cmApAggPortCb2,&r) != kOkApRC )
      {
        cmRptPrintf(rpt,"Out device setup failed.\n");
        goto errLabel;
      }
    }

    // start the input device
    if( cmApDeviceStart(r.inDevIdx) != kOkApRC )
    {
      cmRptPrintf(rpt,"In device start failed.\n");
      goto errLabel;
    }

    if( r.inDevIdx != r.outDevIdx )
    {
      // start the output device
      if( cmApDeviceStart(r.outDevIdx) != kOkApRC )
      {
        cmRptPrintf(rpt,"Out Device start failed.\n");
        goto errLabel;
      }
    }

    cmApBufEnableChannel(r.inDevIdx, -1, kInApFl | kEnableApFl );
    cmApBufEnableChannel(r.outDevIdx, -1, kOutApFl | kEnableApFl );
    cmApBufEnableMeter(r.inDevIdx, -1, kInApFl | kEnableApFl );

    cmRptPrintf(rpt,"q=quit O/o output tone, I/i input tone P/p pass\n");
    char c;
    while((c=getchar()) != 'q')
    {
      //cmApDeviceRtReport(rpt,r.outDevIdx);

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

        case 'm':
          _cmApBufShowMeter(rpt,_cmAggGlobalInDevIdx);
          
          /*
          cmRptPrintf(rpt,"iMeter: ");
          for(i=0; i<r.chCnt; ++i)
            cmRptPrintf(rpt,"%f ",r.iMeter[i]);
          cmRptPrintf(rpt,"\n");
          */
          break;


        case 'r':
          recdPrint();
          break;

        default:
          cmRptPrintf(rpt,"cb:%i\n",r.cbCnt);
      }

    }

  errLabel:
    // stop the input device
    if( cmApDeviceIsStarted(r.inDevIdx) )
      if( cmApDeviceStop(r.inDevIdx) != kOkApRC )
        cmRptPrintf(rpt,"In device stop failed.\n");

    // stop the output device
    if( cmApDeviceIsStarted(r.outDevIdx) )
      if( cmApDeviceStop(r.outDevIdx) != kOkApRC )
        cmRptPrintf(rpt,"Out device stop failed.\n");
  }

 doneLabel:

  // report the count of audio buffer callbacks
  cmRptPrintf(rpt,"cb:%i under:%i over:%i\n", r.cbCnt, r.underunCnt, r.overunCnt );


  // release any resources held by the audio port interface
  if( cmApFinalize() != kOkApRC )
    cmRptPrintf(rpt,"Finalize failed.\n");

  if( cmApAggFree() != kOkAgRC )
    cmRptPrintf(rpt,"Agg device system free failed.");

  if(r.ifp != NULL)
    fclose(r.ifp);

  if(r.ofp != NULL)
    fclose(r.ofp);

  cmApBufFinalize();

  return 0;
}
