//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmTime.h"
#include "cmAudioPort.h"
#include "cmAudioNrtDev.h"
#include "cmAudioPortFile.h"
#include "cmApBuf.h"
#include "cmJson.h"
#include "cmThread.h"
#include "cmUdpPort.h"
#include "cmUdpNet.h"
#include "cmSerialPort.h"
#include "cmAudioSysMsg.h"
#include "cmAudioSys.h"
#include "cmMidi.h"
#include "cmMidiPort.h"

#include "cmMath.h"


cmAudioSysH_t cmAudioSysNullHandle = { NULL };

struct cmAs_str;

typedef struct
{
  struct cmAs_str*   p;         // pointer to the audio system instance which owns this sub-system
  cmAudioSysSubSys_t ss;        // sub-system configuration record
  cmAudioSysCtx_t    ctx;       // DSP context
  cmAudioSysStatus_t status;    // current runtime status of this sub-system
  cmThreadH_t        threadH;   // audio system thread
  cmTsMp1cH_t        htdQueueH; // host-to-dsp thread safe msg queue
  cmThreadMutexH_t   engMutexH; // thread mutex and condition variable
  cmUdpNetH_t        netH;
  cmSeH_t            serialPortH; 
  bool               enableFl;  // application controlled pause flag
  bool               runFl;     // false during finalization otherwise true
  bool               statusFl;  // true if regular status notifications should be sent
  unsigned           audCbLock; //
  bool               syncInputFl;

  double*     iMeterArray;      //  
  double*     oMeterArray;      //

  unsigned    statusUpdateSmpCnt; // transmit a state update msg every statusUpdateSmpCnt samples
  unsigned    statusUpdateSmpIdx; // state update phase 

} _cmAsCfg_t;

typedef struct cmAs_str
{
  cmErr_t     err;
  _cmAsCfg_t* ssArray;
  unsigned    ssCnt;
  unsigned    waitAsSubIdx; // index of the next sub-system to try with cmAudioSysIsMsgWaiting().
  cmTsMp1cH_t dthQueH;
  bool        initFl;       // true if the audio system is initialized
} cmAs_t;


cmAs_t* _cmAsHandleToPtr( cmAudioSysH_t h )
{
  cmAs_t* p = (cmAs_t*)h.h;
  assert(p != NULL);
  return p;
}

cmAsRC_t _cmAsError( cmAs_t* p, cmAsRC_t rc, const char* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  cmErrVMsg(&p->err,rc,fmt,vl);
  va_end(vl);
  return rc;
}

// Wrapper function to put msgs into thread safe queues and handle related errors.
cmAsRC_t _cmAsEnqueueMsg( cmAs_t* p, cmTsMp1cH_t qH, const void* msgDataPtrArray[], unsigned msgCntArray[], unsigned segCnt, const char* queueLabel )
{
  cmAsRC_t rc = kOkAsRC;

  switch( cmTsMp1cEnqueueSegMsg(qH, msgDataPtrArray, msgCntArray, segCnt) )
  {
    case kOkThRC:    
      break;

    case kBufFullThRC:
      {
        unsigned i;
        unsigned byteCnt = 0;
        for(i=0; i<segCnt; ++i)
          byteCnt += msgCntArray[i];

        rc = _cmAsError(p,kMsgEnqueueFailAsRC,"The %s queue was unable to load a msg containing %i bytes. The queue is currently allocated %i bytes and has %i bytes available.",queueLabel,byteCnt,cmTsMp1cAllocByteCount(qH),cmTsMp1cAvailByteCount(qH));
      }
      break;

    default:
      rc = _cmAsError(p,kMsgEnqueueFailAsRC,"A %s msg. enqueue failed.",queueLabel);
  }
 
  return rc;
}

// This is the function pointed to by ctx->dspToHostFunc.
// It is called by the DSP proces to pass msgs to the host.
// therefore it is always called from inside of _cmAsDspExecCallback().
cmAsRC_t _cmAsDspToHostMsgCallback(struct cmAudioSysCtx_str* ctx, const void* msgDataPtrArray[], unsigned msgByteCntArray[], unsigned msgSegCnt)
{ 
  cmAs_t* p = (cmAs_t*)ctx->reserved;
  assert( ctx->asSubIdx < p->ssCnt );
  //return _cmAsEnqueueMsg(p,p->ssArray[ctx->asSubIdx].dthQueueH,msgDataPtrArray,msgByteCntArray,msgSegCnt,"DSP-to-Host"); 
  return _cmAsEnqueueMsg(p,p->dthQueH,msgDataPtrArray,msgByteCntArray,msgSegCnt,"DSP-to-Host"); 
} 

cmAsRC_t _cmAsHostInitNotify( cmAs_t* p )
{
  cmAsRC_t rc = kOkAsRC;

  unsigned i;
  
  for(i=0; i<p->ssCnt; ++i)
  {
    cmAudioSysSsInitMsg_t m;
    _cmAsCfg_t*           cp          = p->ssArray + i;
    const char*           inDevLabel  = cp->ss.args.inDevIdx  == cmInvalidIdx ? "" : cmApDeviceLabel( cp->ss.args.inDevIdx );
    const char*           outDevLabel = cp->ss.args.outDevIdx == cmInvalidIdx ? "" : cmApDeviceLabel( cp->ss.args.outDevIdx );

    m.asSubIdx          = i;
    m.selId             = kSsInitSelAsId;
    m.asSubCnt          = p->ssCnt;
    m.inDevIdx          = cp->ss.args.inDevIdx;
    m.outDevIdx         = cp->ss.args.outDevIdx;
    m.dspFramesPerCycle = cp->ss.args.dspFramesPerCycle;
    m.srate             = cp->ss.args.srate;
    m.inChCnt           = cp->status.iMeterCnt;
    m.outChCnt          = cp->status.oMeterCnt;
    
    unsigned    segCnt = 3;
    const void* msgDataPtrArray[] = { &m, inDevLabel, outDevLabel };
    unsigned    msgByteCntArray[] = { sizeof(m), strlen(cmStringNullGuard(inDevLabel))+1, strlen(cmStringNullGuard(outDevLabel))+1 };

    assert( sizeof(msgDataPtrArray)/sizeof(void*)    == segCnt);
    assert( sizeof(msgByteCntArray)/sizeof(unsigned) == segCnt);

    if((rc = _cmAsDspToHostMsgCallback(&cp->ctx, msgDataPtrArray, msgByteCntArray, segCnt)) != kOkAsRC )
      return rc;    
  }

  return rc;
}

cmAsRC_t  _cmAsParseNonSubSysMsg(  cmAs_t* p, const void* msg, unsigned msgByteCnt )
{
  cmAsRC_t rc = kOkAsRC;
  cmAudioSysMstr_t* h = (cmAudioSysMstr_t*)msg;
  unsigned devIdx = cmAudioSysUiInstIdToDevIndex(h->instId);
  unsigned chIdx  = cmAudioSysUiInstIdToChIndex(h->instId);
  unsigned inFl   = cmAudioSysUiInstIdToInFlag(h->instId);
  unsigned ctlId  = cmAudioSysUiInstIdToCtlId(h->instId);

  // if the valuu associated with this msg is a mtx then set
  // its mtx data area pointer to just after the msg header.
  //if( cmDsvIsMtx(&h->value) )
  //  h->value.u.m.u.vp = ((char*)msg) + sizeof(cmDspUiHdr_t);

  unsigned flags = inFl ? kInApFl : kOutApFl;
  
  switch( ctlId )
  {
    
    case kSliderUiAsId: // slider
      cmApBufSetGain(devIdx,chIdx, flags, h->value); 
      break;
      
    case kMeterUiAsId: // meter
      break;
      
    case kMuteUiAsId: // mute
      flags += h->value == 0 ? kEnableApFl : 0;
      cmApBufEnableChannel(devIdx,chIdx,flags);
      break;

    case kToneUiAsId: // tone
      flags += h->value > 0 ? kEnableApFl : 0;
      cmApBufEnableTone(devIdx,chIdx,flags);
      break;

    case kPassUiAsId: // pass
      flags += h->value > 0 ? kEnableApFl : 0;
      cmApBufEnablePass(devIdx,chIdx,flags);
      break;

    default:
      { assert(0); }
  }

  return rc;
}

// Process a UI msg sent from the host to the audio system  
cmAsRC_t  _cmAsHandleNonSubSysMsg(  cmAs_t* p, const void* msgDataPtrArray[], unsigned msgByteCntArray[], unsigned msgSegCnt )
{
  cmAsRC_t rc = kOkAsRC;

  // if the message is contained in a single segment it can be dispatched immediately ...
  if( msgSegCnt == 1 )
    rc = _cmAsParseNonSubSysMsg(p,msgDataPtrArray[0],msgByteCntArray[0]);
  else
  {
    // ... otherwise deserialize the message into contiguous memory ....
    unsigned byteCnt = 0;
    unsigned i;

    for(i=0; i<msgSegCnt; ++i)
      byteCnt += msgByteCntArray[i];
    
    char buf[ byteCnt ];
    char* b = buf;
    for(i=0; i<msgSegCnt; ++i)
    {
      memcpy(b, msgDataPtrArray[i], msgByteCntArray[i] );
      b += msgByteCntArray[i];
    }
    // ... and then dispatch it
    rc = _cmAsParseNonSubSysMsg(p,buf,byteCnt);

  }
  
  return rc;
}


cmAsRC_t _cmAsSendStateStatusToHost(  _cmAsCfg_t* cp )
{
  cmAsRC_t rc    = kOkAsRC;
  unsigned hdr[] = { cp->ctx.asSubIdx, kStatusSelAsId };

  cmApBufGetStatus( cp->ss.args.inDevIdx,  kInApFl,  cp->iMeterArray, cp->status.iMeterCnt, &cp->status.overflowCnt );
  cmApBufGetStatus( cp->ss.args.outDevIdx, kOutApFl, cp->oMeterArray, cp->status.oMeterCnt, &cp->status.underflowCnt );

  unsigned    iMeterByteCnt     = sizeof(cp->iMeterArray[0]) * cp->status.iMeterCnt;
  unsigned    oMeterByteCnt     = sizeof(cp->oMeterArray[0]) * cp->status.oMeterCnt;  
  const void* msgDataPtrArray[] = { &hdr, &cp->status, cp->iMeterArray, cp->oMeterArray };
  unsigned    msgByteCntArray[] = { sizeof(hdr), sizeof(cp->status), iMeterByteCnt, oMeterByteCnt };
  unsigned    segCnt            = sizeof(msgByteCntArray)/sizeof(unsigned);

  _cmAsDspToHostMsgCallback(&cp->ctx,msgDataPtrArray,msgByteCntArray, segCnt );

  return rc;
}


// The DSP execution callback happens through this function.
// This function is only called from inside _cmAsThreadCallback() with the engine mutex locked.
void _cmAsDspExecCallback( _cmAsCfg_t* cp )
{
  /*
  unsigned i;

  // get pointers to a set of audio out buffers - pointers to disabled channels will be set to NULL
  cmApBufGet( cp->ss.args.outDevIdx, kOutApFl, cp->ctx.oChArray, cp->ctx.oChCnt );
  cmApBufGet( cp->ss.args.inDevIdx,  kInApFl,  cp->ctx.iChArray, cp->ctx.iChCnt );

  // zero the output buffers on all enabled channels 
  for(i=0; i<cp->ctx.oChCnt; ++i)
    if( cp->ctx.oChArray[i] != NULL )
      memset( cp->ctx.oChArray[i], 0, cp->ss.args.dspFramesPerCycle * sizeof(cmSample_t));
  */
  
  // Fill iChArray[] and oChArray[] with pointers to the incoming and outgoing sample buffers.
  // Notes:
  //   1) Buffers associated with disabled input/output channels will be set to NULL in iChArray[]/oChArray[].
  //   2) Buffers associated with channels marked for pass-through will be set to NULL in oChArray[].
  //   3) All samples returned in oChArray[] buffers will be set to zero.
  cmApBufGetIO(cp->ss.args.inDevIdx,  cp->ctx.iChArray, cp->ctx.iChCnt, &cp->ctx.iTimeStamp, 
               cp->ss.args.outDevIdx, cp->ctx.oChArray, cp->ctx.oChCnt, &cp->ctx.oTimeStamp  );

  // call the application provided DSP process
  cp->ctx.audioRateFl = true;
  cp->ss.cbFunc( &cp->ctx, 0, NULL ); 
  cp->ctx.audioRateFl = false;

  // advance the audio buffer
  cmApBufAdvance( cp->ss.args.outDevIdx, kOutApFl );
  cmApBufAdvance( cp->ss.args.inDevIdx,  kInApFl  );

  // handle periodic status messages to the host
  if( (cp->statusUpdateSmpIdx += cp->ss.args.dspFramesPerCycle) >= cp->statusUpdateSmpCnt )
  {
    cp->statusUpdateSmpIdx -= cp->statusUpdateSmpCnt;

    if( cp->statusFl )
      _cmAsSendStateStatusToHost(cp);
  }

}

// Returns true if audio buffer is has waiting incoming samples and
// available outgoing space. 
bool _cmAsBufIsReady( const _cmAsCfg_t* cp )
{
  // if there neither the input or output device is valid
  if( cp->ss.args.inDevIdx==cmInvalidIdx && cp->ss.args.outDevIdx == cmInvalidIdx )
    return false;

  bool ibFl = cmApBufIsDeviceReady(cp->ss.args.inDevIdx, kInApFl);
  bool obFl = cmApBufIsDeviceReady(cp->ss.args.outDevIdx, kOutApFl);
  bool iFl = (cp->ss.args.inDevIdx  == cmInvalidIdx) || ibFl;
  bool oFl = (cp->ss.args.outDevIdx == cmInvalidIdx) || obFl;

  //printf("br: %i %i %i %i\n",ibFl,obFl,iFl,oFl);

  return iFl && oFl;
}


// This is only called with _cmAsRecd.engMutexH locked
cmAsRC_t _cmAsDeliverMsgsWithLock( _cmAsCfg_t* cp  )
{
  int      i;
  cmAsRC_t rc = kOkThRC;
    
  // as long as their may be a msg wating in the incoming msg queue
  for(i=0; rc == kOkThRC; ++i)
  {
    // if a msg is waiting transmit it via cfg->cbFunc()
    if((rc = cmTsMp1cDequeueMsg(cp->htdQueueH,NULL,0)) == kOkThRC)
      ++cp->status.msgCbCnt;
  }

  return rc;
}


// This is the main audio system loop (and thread callback function).
// It blocks by waiting on a cond. var (which simultaneously unlocks a mutex).
// With the mutex unlocked messages can pass directly to the DSP process
// via calls to cmAsDeliverMsg(). 
// When the audio buffers need to be serviced the audio device callback
// signals the cond. var. which results in this thread waking up (and
// simultaneously locking the mutex) as soon as the mutex is available. 
bool _cmAsThreadCallback(void* arg)
{
  cmAsRC_t rc;
  _cmAsCfg_t*  cp = (_cmAsCfg_t*)arg;

  // lock the cmAudioSys mutex
  if((rc = cmThreadMutexLock(cp->engMutexH)) != kOkAsRC )
  {
    _cmAsError(cp->p,rc,"The cmAudioSys thread mutex lock failed.");
    return false;
  }

  // runFl is always set except during finalization 
  while( cp->runFl )
  {

    // if the buffer is NOT ready or the cmAudioSys is disabled
    if(_cmAsBufIsReady(cp) == false || cp->enableFl==false )
    {
      // block on the cond var and unlock the mutex
      if( cmThreadMutexWaitOnCondVar(cp->engMutexH,false) != kOkAsRC )
      {
        cmThreadMutexUnlock(cp->engMutexH);
        _cmAsError(cp->p,rc,"The cmAudioSys cond. var. wait failed.");
        return false;
      }

      //
      // the cond var was signaled and the mutex is now locked 
      //
      ++cp->status.wakeupCnt;
    }

    // be sure we are still enabled and the buffer is still ready
    if( cp->enableFl && cp->runFl  )
    {
      while( _cmAsBufIsReady(cp) )
      {
        ++cp->status.audioCbCnt;
        
        // calling this function results in callbacks to cmAudDsp.c:_cmAdUdpNetCallback()
        // which in turn calls cmAudioSysDeliverMsg() which queues any incoming messages
        // which are then transferred to the DSP processes by the the call to 
        // _cmAsDeliverMsgWithLock() below.
        cmUdpNetReceive(cp->netH,NULL);
    
        // if there are msgs waiting to be sent to the DSP process send them. 
        if( cmTsMp1cMsgWaiting(cp->htdQueueH) )
          _cmAsDeliverMsgsWithLock(cp); 

        // make the cmAudioSys callback
        _cmAsDspExecCallback( cp ); 

        // update the signal time
        cp->ctx.begSmpIdx += cp->ss.args.dspFramesPerCycle;
      }
    }
   
  } 
  
  // unlock the mutex
  cmThreadMutexUnlock(cp->engMutexH);

  return true;
}


// This is the audio port callback function.
//
// _cmAudioSysAudioUpdate() assumes that at most two audio device threads (input and output) may call it.  
//  cmApBufUpdate() is safe under these conditions since the input and output buffers are updated separately.
// p->audCbLock is used to allow either the input or output thread to signal
// the condition variable.  This flag is necessary to prevent both threads from simultaneously
// attempting to signal the condition variable (which will lock the system). 
// 
// If more than two audio device threads call the function then this function is not safe.

unsigned phase = 0;

void   _cmAudioSysAudioUpdate( cmApAudioPacket_t* inPktArray, unsigned inPktCnt, cmApAudioPacket_t* outPktArray, unsigned outPktCnt )
{
  _cmAsCfg_t* cp = (_cmAsCfg_t*)(inPktArray!=NULL ? inPktArray[0].userCbPtr : outPktArray[0].userCbPtr);

  ++cp->status.updateCnt;

  if( cp->runFl )
  {

    // transfer incoming/outgoing samples from/to the audio device
    cmApBufUpdate(inPktArray,inPktCnt,outPktArray,outPktCnt);

   
    /*
    //fill output with noise
    unsigned i = 0,j =0, k = 0, phs = 0;
    for(; i<outPktCnt; ++i)
    {
      cmApAudioPacket_t*  a = outPktArray + i;
      cmApSample_t*      dp = (cmApSample_t*)a->audioBytesPtr;

      phs = a->audioFramesCnt;

      for(j=0; j<a->audioFramesCnt; ++j)
      {
        cmApSample_t v = (cmApSample_t)(0.7 * sin(2*M_PI/44100.0 * phase + j ));

        for(k=0; k<a->chCnt; ++k,++dp)
          *dp = v;
      }
      //for(j=0; j<a->audioFramesCnt*a->chCnt; ++j,++dp)
      // *dp = (cmApSample_t)(rand() - (RAND_MAX/2))/(RAND_MAX/2);

    }
    
    phase += phs;

    return;
    */

    //++p->audCbLock;

    bool testBufFl = (cp->syncInputFl==true && inPktCnt>0) || (cp->syncInputFl==false && outPktCnt>0);

    //printf("%i %i %i %i\n",testBufFl,cp->syncInputFl,inPktCnt,outPktCnt);

    // if the input/output buffer contain samples to be processed then signal the condition variable 
    // - this will cause the audio system thread to unblock and the used defined DSP process will be called.
    if( testBufFl && _cmAsBufIsReady(cp) )
    {
      if( cmThreadMutexSignalCondVar(cp->engMutexH) != kOkThRC )
        _cmAsError(cp->p,kMutexErrAsRC,"CmAudioSys signal cond. var. failed.");
      
    }
    //--p->audCbLock;
  }

}

// Called when MIDI messages arrive from external MIDI ports.
void _cmAudioSysMidiCallback( const cmMidiPacket_t* pktArray, unsigned pktCnt )
{
    unsigned i;
    for(i=0; i<pktCnt; ++i)
    {
      const cmMidiPacket_t* pkt = pktArray + i;
      _cmAsCfg_t*           cp  = (_cmAsCfg_t*)(pkt->cbDataPtr);

      if( !cp->runFl  )
        continue;

      cmAudioSysH_t    asH;
      asH.h = cp->p; 

      unsigned    selId             = kMidiMsgArraySelAsId;
      const void* msgPtrArray[]     = { &cp->ctx.asSubIdx,        &selId,        &pkt->devIdx,        &pkt->portIdx,        &pkt->msgCnt,        pkt->msgArray };
      unsigned    msgByteCntArray[] = { sizeof(cp->ctx.asSubIdx), sizeof(selId), sizeof(pkt->devIdx), sizeof(pkt->portIdx), sizeof(pkt->msgCnt), pkt->msgCnt*sizeof(cmMidiMsg) };
      unsigned    msgSegCnt         = sizeof(msgByteCntArray)/sizeof(unsigned);

      cmAudioSysDeliverSegMsg(asH,msgPtrArray,msgByteCntArray,msgSegCnt,cmInvalidId);
    }

}

void _cmAudioSysSerialPortCallback( void* cbArg, const void* byteA, unsigned byteN )
{
  //_cmAsCfg_t* p (_cmAsCfg_t*)cbArg;
  
  // TODO: handle serial receive
  /*
  int i;
  for(i=0; i<byteN; ++i)
  {
    printf("%02x ",((const uint8_t*)byteA)[i]);
    fflush(stdout);
  }
  */
}

cmAsRC_t cmAudioSysAllocate( cmAudioSysH_t* hp, cmRpt_t* rpt, const cmAudioSysCfg_t* cfg )
{
  cmAsRC_t rc;

  if((rc = cmAudioSysFree(hp)) != kOkAsRC )
    return rc;

  cmAs_t*  p = cmMemAllocZ( cmAs_t, 1 );

  cmErrSetup(&p->err,rpt,"Audio System");

  hp->h = p;

  if( cfg != NULL )
    if((rc = cmAudioSysInitialize( *hp, cfg )) != kOkAsRC )
      cmAudioSysFree(hp);

  return rc;
}

cmAsRC_t cmAudioSysFree( cmAudioSysH_t* hp )
{
  cmAsRC_t rc;

  if( hp == NULL || hp->h == NULL )
    return kOkAsRC;

  if((rc = cmAudioSysFinalize(*hp)) != kOkAsRC )
    return rc;

  cmAs_t* p = _cmAsHandleToPtr(*hp);

  cmMemFree(p);

  hp->h = NULL;

  return rc;
}

cmAsRC_t _cmAudioSysEnable( cmAs_t* p, bool enableFl )
{
  cmAsRC_t rc;

  unsigned i;

  for(i=0; i<p->ssCnt; ++i)
  {
    _cmAsCfg_t* cp = p->ssArray + i;

    cmApBufOnPortEnable(cp->ss.args.inDevIdx,enableFl);
    cmApBufOnPortEnable(cp->ss.args.outDevIdx,enableFl);
    
    if( enableFl )
    {

      //cmApBufPrimeOutput( cp->ss.args.outDevIdx, 2 );

      // start the input device
      if((rc = cmApDeviceStart( cp->ss.args.inDevIdx )) != kOkAsRC )
        return _cmAsError(p,kAudioDevStartFailAsRC,"The audio input device start failed.");

      // start the output device
      if( cmApDeviceStart( cp->ss.args.outDevIdx ) != kOkAsRC )
        return _cmAsError(p,kAudioDevStartFailAsRC,"The audio ouput device start failed.");
    } 
    else
    {
      // stop the input device
      if((rc = cmApDeviceStop( cp->ss.args.inDevIdx )) != kOkAsRC )
        return _cmAsError(p,kAudioDevStopFailAsRC,"The audio input device stop failed.");

      // stop the output device
      if((rc = cmApDeviceStop( cp->ss.args.outDevIdx )) != kOkAsRC )
        return _cmAsError(p,kAudioDevStopFailAsRC,"The audio output device stop failed.");

    }

    cp->enableFl = enableFl; 
  }
  return kOkAsRC;
}

cmAsRC_t _cmAudioSysFinalize( cmAs_t* p )
{
  cmAsRC_t rc = kOkAsRC;
  unsigned i;
  
  // mark  the audio system as NOT initialized
  p->initFl = false;

  // be sure all audio callbacks are disabled before continuing.
  if((rc = _cmAudioSysEnable(p,false)) != kOkAsRC )
    return _cmAsError(p,rc,"Audio system finalize failed because device halting failed.");

  for(i=0; i<p->ssCnt; ++i)
  {
    _cmAsCfg_t* cp = p->ssArray + i;

    if( cmThreadIsValid( cp->threadH ))
    {
      // inform the thread that it should exit
      cp->enableFl = false;
      cp->runFl    = false;
      cp->statusFl = false;

      // WARNING: be sure that the audio thread cannot simultaneously signal the 
      // cond variable from _cmAsAudioUpdate() otherwise the system may crash

      while( cp->audCbLock != 0 )
      { cmSleepUs(100000); }

      // signal the cond var to cause the thread to run
      if((rc = cmThreadMutexSignalCondVar(cp->engMutexH)) != kOkThRC )
        _cmAsError(p,kMutexErrAsRC,"Finalize signal cond. var. failed.");

      // wait to take control of the mutex - this will occur when the thread function exits
      if((rc = cmThreadMutexLock(cp->engMutexH)) != kOkThRC )
        _cmAsError(p,kMutexErrAsRC,"Finalize lock failed.");

      // unlock the mutex because it is no longer needed and must be unlocked to be destroyed
      if((rc = cmThreadMutexUnlock(cp->engMutexH)) != kOkThRC )
        _cmAsError(p,kMutexErrAsRC,"Finalize unlock failed.");

      // destroy the thread
      if((rc = cmThreadDestroy( &cp->threadH )) != kOkThRC )
        _cmAsError(p,kThreadErrAsRC,"Thread destroy failed.");   

    }

    // destroy the mutex
    if( cmThreadMutexIsValid(cp->engMutexH) )
      if((rc = cmThreadMutexDestroy( &cp->engMutexH )) != kOkThRC )
        _cmAsError(p,kMutexErrAsRC,"Mutex destroy failed.");


    // remove the MIDI callback
    if( cmMpIsInitialized() && cmMpUsesCallback(-1,-1, _cmAudioSysMidiCallback, cp) )
      if( cmMpRemoveCallback( -1, -1, _cmAudioSysMidiCallback, cp ) != kOkMpRC )
        _cmAsError(p,kMidiSysFailAsRC,"MIDI callback removal failed.");

    // destroy the host-to-dsp msg queue
    if( cmTsMp1cIsValid(cp->htdQueueH ) )
      if((rc = cmTsMp1cDestroy( &cp->htdQueueH )) != kOkThRC )
        _cmAsError(p,kTsQueueErrAsRC,"Host-to-DSP msg queue destroy failed.");

    // destroy the dsp-to-host msg queue
    if( cmTsMp1cIsValid(p->dthQueH) )
      if((rc = cmTsMp1cDestroy( &p->dthQueH )) != kOkThRC )
        _cmAsError(p,kTsQueueErrAsRC,"DSP-to-Host msg queue destroy failed.");


    cmMemPtrFree(&cp->ctx.iChArray);
    cmMemPtrFree(&cp->ctx.oChArray);
    cp->ctx.iChCnt = 0;
    cp->ctx.oChCnt = 0;

    cmMemPtrFree(&cp->iMeterArray);
    cmMemPtrFree(&cp->oMeterArray);
    cp->status.iMeterCnt = 0;
    cp->status.oMeterCnt = 0;

  }


  cmMemPtrFree(&p->ssArray);
  p->ssCnt = 0;

  return rc;
}

// A given device may be used as an input device exactly once and an output device exactly once.
// When the input to a given device is used by one sub-system and the output is used by another
// then both sub-systems must use the same srate,devFramesPerCycle, audioBufCnt and dspFramesPerCycle.
cmAsRC_t _cmAsSysValidate( cmErr_t* err, const cmAudioSysCfg_t* cfg )
{
  unsigned i,j,k;
  for(i=0; i<2; ++i)
  {
    // examine input devices - then output devices
    bool inputFl  = i==0;
    bool outputFl = !inputFl;

    for(j=0; j<cfg->ssCnt; ++j)
    {
      cmAudioSysArgs_t* s0     = &cfg->ssArray[j].args;
      unsigned          devIdx = inputFl ? s0->inDevIdx : s0->outDevIdx;

      for(k=0; k<cfg->ssCnt && devIdx != cmInvalidIdx; ++k)
        if( k != j )
        {
          cmAudioSysArgs_t* s1 = &cfg->ssArray[k].args;

          // if the device was used as input or output multple times then signal an error
          if( (inputFl && (s1->inDevIdx == devIdx) && s1->inDevIdx != cmInvalidIdx) || (outputFl && (s1->outDevIdx == devIdx) && s1->outDevIdx != cmInvalidIdx) )
            return cmErrMsg(err,kInvalidArgAsRC,"The device %i was used as an %s by multiple sub-systems.", devIdx, inputFl ? "input" : "output");

          // if this device is being used by another subsystem ...
          if( (inputFl && (s1->outDevIdx == devIdx) && s1->inDevIdx != cmInvalidIdx) || (outputFl && (s1->outDevIdx == devIdx) && s1->outDevIdx != cmInvalidIdx ) )
          {
            // ... then some of its buffer spec's must match 
            if( s0->srate != s1->srate || s0->audioBufCnt != s1->audioBufCnt || s0->dspFramesPerCycle != s1->dspFramesPerCycle || s0->devFramesPerCycle != s1->devFramesPerCycle )
              return cmErrMsg(err,kInvalidArgAsRC,"The device %i is used by different sub-system with different audio buffer parameters.",devIdx);
          }
        }
    }
  }
  
  return kOkAsRC;
}

cmAsRC_t cmAudioSysInitialize( cmAudioSysH_t h, const cmAudioSysCfg_t* cfg )
{
  cmAsRC_t rc;
  unsigned i;
  cmAs_t* p = _cmAsHandleToPtr(h);

  // validate the device setup
  if((rc =_cmAsSysValidate(&p->err, cfg )) != kOkAsRC )
    return rc;

  // always finalize before iniitalize
  if((rc = cmAudioSysFinalize(h)) != kOkAsRC )
    return rc;

  // create the audio file devices
  /*
  for(i=0; i<cfg->afpCnt; ++i)
  {
    const cmAudioSysFilePort_t* afp = cfg->afpArray + i;
    cmApFileDeviceCreate( afp->devLabel, afp->inAudioFn, afp->outAudioFn, afp->oBits, afp->oChCnt );
  }
  */

  p->ssArray = cmMemAllocZ( _cmAsCfg_t, cfg->ssCnt );
  p->ssCnt   = cfg->ssCnt;
  
  for(i=0; i<p->ssCnt; ++i)
  {
    _cmAsCfg_t*               cp = p->ssArray + i;
    const cmAudioSysSubSys_t* ss = cfg->ssArray + i;

    cp->p                 = p;
    cp->ss                = *ss;  // copy the cfg into the internal audio system state
    cp->runFl             = false;
    cp->enableFl          = false;
    cp->statusFl          = false;
    cp->ctx.reserved      = p;
    cp->ctx.asSubIdx      = i;
    cp->ctx.ss            = &cp->ss;
    cp->ctx.begSmpIdx     = 0;
    cp->ctx.dspToHostFunc = _cmAsDspToHostMsgCallback;

    // validate the input device index
    if( ss->args.inDevIdx != cmInvalidIdx && ss->args.inDevIdx >= cmApDeviceCount() )
    {
      rc = _cmAsError(p,kAudioDevSetupErrAsRC,"The audio input device index %i is invalid.",ss->args.inDevIdx);
      goto errLabel;
    }

    // validate the output device index
    if( ss->args.outDevIdx != cmInvalidIdx && ss->args.outDevIdx >= cmApDeviceCount() )
    {
      rc =  _cmAsError(p,kAudioDevSetupErrAsRC,"The audio output device index %i is invalid.",ss->args.outDevIdx);
      goto errLabel;
    }

    // setup the input device
    if( ss->args.inDevIdx != cmInvalidIdx )
      if((rc = cmApDeviceSetup( ss->args.inDevIdx, ss->args.srate, ss->args.devFramesPerCycle, _cmAudioSysAudioUpdate, cp )) != kOkAsRC )
      {
        rc = _cmAsError(p,kAudioDevSetupErrAsRC,"Audio input device setup failed.");
        goto errLabel;
      }

    // setup the output device
    if( ss->args.outDevIdx != ss->args.inDevIdx && ss->args.outDevIdx != cmInvalidIdx )
      if((rc = cmApDeviceSetup( ss->args.outDevIdx, ss->args.srate, ss->args.devFramesPerCycle, _cmAudioSysAudioUpdate, cp )) != kOkAsRC )
      {
        rc =  _cmAsError(p,kAudioDevSetupErrAsRC,"Audio output device setup failed.");
        goto errLabel;
      }

    // setup the input device buffer
    if( ss->args.inDevIdx != cmInvalidIdx )
      if((rc = cmApBufSetup( ss->args.inDevIdx, ss->args.srate, ss->args.dspFramesPerCycle, ss->args.audioBufCnt, cmApDeviceChannelCount(ss->args.inDevIdx, true),  ss->args.devFramesPerCycle, cmApDeviceChannelCount(ss->args.inDevIdx, false), ss->args.devFramesPerCycle, ss->args.srateMult )) != kOkAsRC )
      {
        rc = _cmAsError(p,kAudioBufSetupErrAsRC,"Audio buffer input  setup failed.");
        goto errLabel;
      }

    cmApBufEnableMeter(ss->args.inDevIdx, -1, kInApFl  | kEnableApFl );
    cmApBufEnableMeter(ss->args.outDevIdx,-1, kOutApFl | kEnableApFl );

    // setup the input audio buffer ptr array - used to send input audio to the DSP system in _cmAsDspExecCallback()
    if((cp->ctx.iChCnt   = cmApDeviceChannelCount(ss->args.inDevIdx, true)) != 0 )
      cp->ctx.iChArray = cmMemAllocZ( cmSample_t*, cp->ctx.iChCnt );

    // setup the output device buffer
    if( ss->args.outDevIdx != ss->args.inDevIdx )
      if((rc = cmApBufSetup( ss->args.outDevIdx, ss->args.srate, ss->args.dspFramesPerCycle, ss->args.audioBufCnt, cmApDeviceChannelCount(ss->args.outDevIdx, true), ss->args.devFramesPerCycle, cmApDeviceChannelCount(ss->args.outDevIdx, false), ss->args.devFramesPerCycle, ss->args.srateMult )) != kOkAsRC )
        return _cmAsError(p,kAudioBufSetupErrAsRC,"Audio buffer ouput device setup failed.");

    // setup the output audio buffer ptr array - used to recv output audio from the DSP system in _cmAsDspExecCallback()
    if((cp->ctx.oChCnt   = cmApDeviceChannelCount(ss->args.outDevIdx, false)) != 0 )
      cp->ctx.oChArray = cmMemAllocZ( cmSample_t*, cp->ctx.oChCnt );

    // determine the sync source
    cp->syncInputFl = ss->args.syncInputFl;

    // if sync'ing to an unavailable device then sync to the available device
    if( ss->args.syncInputFl && cp->ctx.iChCnt == 0 )
      cp->syncInputFl = false;

    if( ss->args.syncInputFl==false && cp->ctx.oChCnt == 0 )
      cp->syncInputFl = true;
    
    // setup the status record
    cp->status.asSubIdx  = cp->ctx.asSubIdx;
    cp->status.iDevIdx   = ss->args.inDevIdx;
    cp->status.oDevIdx   = ss->args.outDevIdx;
    cp->status.iMeterCnt = cp->ctx.iChCnt;
    cp->status.oMeterCnt = cp->ctx.oChCnt;
    cp->iMeterArray      = cmMemAllocZ( double, cp->status.iMeterCnt );
    cp->oMeterArray      = cmMemAllocZ( double, cp->status.oMeterCnt );
    cp->netH             = cfg->netH;
    cp->serialPortH      = cfg->serialPortH;

    // create the audio System thread
    if((rc = cmThreadCreate( &cp->threadH, _cmAsThreadCallback, cp, ss->args.rpt )) != kOkThRC )
    {
      rc = _cmAsError(p,kThreadErrAsRC,"Thread create failed.");
      goto errLabel;
    }

    // create the audio System mutex
    if((rc = cmThreadMutexCreate( &cp->engMutexH, ss->args.rpt )) != kOkThRC )
    {
      rc = _cmAsError(p,kMutexErrAsRC,"Thread mutex create failed.");
      goto errLabel;
    }

    // create the host-to-dsp thread safe msg queue 
    if((rc = cmTsMp1cCreate( &cp->htdQueueH, ss->args.msgQueueByteCnt, ss->cbFunc, &cp->ctx, ss->args.rpt )) != kOkThRC )
    {
      rc = _cmAsError(p,kTsQueueErrAsRC,"Host-to-DSP msg queue create failed.");
      goto errLabel;
    }

    // create the dsp-to-host thread safe msg queue 
    if( cmTsMp1cIsValid( p->dthQueH ) == false )
    {
      if((rc = cmTsMp1cCreate( &p->dthQueH, ss->args.msgQueueByteCnt, cfg->clientCbFunc, cfg->clientCbData, ss->args.rpt )) != kOkThRC )
      {
        rc = _cmAsError(p,kTsQueueErrAsRC,"DSP-to-Host msg queue create failed.");
        goto errLabel;
      }
    }
    
    //cp->dthQueueH = p->dthQueH;

    // install an external MIDI port callback handler for incoming MIDI messages
    if( cmMpIsInitialized() )
      if( cmMpInstallCallback( -1, -1, _cmAudioSysMidiCallback, cp ) != kOkMpRC )
      {
        rc = _cmAsError(p,kMidiSysFailAsRC,"MIDI system callback installation failed.");
        goto errLabel;
      }

    // install the serial port 
    if( cmSeIsOpen(cp->serialPortH) )
    {
      if( cmSeSetCallback(cp->serialPortH, _cmAudioSysSerialPortCallback, cp ) != kOkSeRC )
      {
        rc = _cmAsError(p,kSerialPortFailAsRC,"Serial port callback installation failed.");
        goto errLabel;        
      }
    }

    // setup the sub-system status notification 
    cp->statusUpdateSmpCnt = floor(cmApBufMeterMs() * cp->ss.args.srate / 1000.0 );
    cp->statusUpdateSmpIdx = 0;

    cp->runFl = true;

    // start the audio System thread
    if( cmThreadPause( cp->threadH, 0 ) != kOkThRC )
    {
      rc = _cmAsError(p,kThreadErrAsRC,"Thread start failed.");
      goto errLabel;
    }

    if( cmSeIsOpen(cp->serialPortH) )
    {
      if( cmSeStart( cp->serialPortH ) != kOkSeRC )
      {
        rc = _cmAsError(p,kSerialPortFailAsRC,"Serial port start failed.");
        goto errLabel;        
      }
      
    }
  }


  _cmAsHostInitNotify(p);
  p->initFl = true;

 errLabel:
  if( rc != kOkAsRC )
    _cmAudioSysFinalize(p);

  return rc;
}


cmAsRC_t cmAudioSysFinalize(cmAudioSysH_t h )
{
  cmAsRC_t rc = kOkAsRC;

  if( cmAudioSysHandleIsValid(h) == false )
    return rc;

  cmAs_t* p = _cmAsHandleToPtr(h);

  rc = _cmAudioSysFinalize(p);

  h.h = NULL;

  return rc;
}


bool   cmAudioSysIsInitialized( cmAudioSysH_t h )
{ 
  cmAs_t* p = _cmAsHandleToPtr(h);
  return p->initFl;
}

cmAsRC_t _cmAudioSysVerifyInit( cmAs_t* p )
{
  if( p->initFl  == false )
  {
    // if the last msg generated was also a not init msg then don't 
    // generate another message - just return the error
    if( cmErrLastRC(&p->err) != kNotInitAsRC )
      cmErrMsg(&p->err,kNotInitAsRC,"The audio system is not initialized.");
    return kNotInitAsRC;
  }
 
  return kOkAsRC;
}


bool   cmAudioSysIsEnabled( cmAudioSysH_t h )
{ 
  if( cmAudioSysIsInitialized(h) == false )
    return false;

  cmAs_t* p = _cmAsHandleToPtr(h);
  unsigned i;
  for(i=0; i<p->ssCnt; ++i)
    if( p->ssArray[i].enableFl )
      return true;

  return false;
}


cmAsRC_t cmAudioSysEnable( cmAudioSysH_t h, bool enableFl )
{
  cmAs_t*  p = _cmAsHandleToPtr(h);
  return _cmAudioSysEnable(p,enableFl);
}

cmAsRC_t  cmAudioSysDeliverSegMsg(  cmAudioSysH_t h, const void* msgDataPtrArray[], unsigned msgByteCntArray[], unsigned msgSegCnt, unsigned srcNetNodeId )
{
  cmAs_t* p = _cmAsHandleToPtr(h);
  cmAsRC_t rc;

  // the system must be initialized to use this function
  if((rc = _cmAudioSysVerifyInit(p)) != kOkAsRC )
    return rc;

  if( msgSegCnt == 0 )
    return kOkAsRC;

  // BUG BUG BUG - there is no reason that both the asSubIdx and the selId must
  // be in the first segment but it would be nice.
  assert( msgByteCntArray[0] >= 2*sizeof(unsigned) || (msgSegCnt>1 && msgByteCntArray[0]==sizeof(unsigned) && msgByteCntArray[1]>=sizeof(unsigned)) );

  // The audio sub-system index is always the first field of the msg 
  // and the msg selector id is always the second field

  unsigned* array = (unsigned*)msgDataPtrArray[0];
  unsigned asSubIdx = array[0];
  unsigned selId    = array[1];

  if( selId == kUiMstrSelAsId )
    return _cmAsHandleNonSubSysMsg( p, msgDataPtrArray, msgByteCntArray, msgSegCnt );

  if( selId == kNetSyncSelAsId )
  {
    assert( msgSegCnt==1); 
    assert( asSubIdx < p->ssCnt );
    p->ssArray[asSubIdx].ctx.srcNetNodeId = srcNetNodeId;
    p->ssArray[asSubIdx].ss.cbFunc(&p->ssArray[asSubIdx].ctx,msgByteCntArray[0],msgDataPtrArray[0]);
    return kOkAsRC;
  }

  return _cmAsEnqueueMsg(p,p->ssArray[asSubIdx].htdQueueH,msgDataPtrArray,msgByteCntArray,msgSegCnt,"Host-to-DSP");  
}

cmAsRC_t cmAudioSysDeliverMsg( cmAudioSysH_t h, const void* msgPtr, unsigned msgByteCnt, unsigned srcNetNodeId )
{
  const void* msgDataPtrArray[] = {  msgPtr };
  unsigned    msgByteCntArray[] = { msgByteCnt };
  return cmAudioSysDeliverSegMsg(h,msgDataPtrArray,msgByteCntArray,1,srcNetNodeId);
}

cmAsRC_t cmAudioSysDeliverIdMsg( cmAudioSysH_t h, unsigned asSubIdx, unsigned id, const void* msgPtr, unsigned msgByteCnt, unsigned srcNetNodeId )
{
  cmAsRC_t rc;
  cmAs_t* p = _cmAsHandleToPtr(h);

  // the system must be initialized to use this function
  if((rc = _cmAudioSysVerifyInit(p)) != kOkAsRC )
    return rc;

  const void* msgDataPtrArray[] = { &asSubIdx, &id, msgPtr };
  unsigned    msgByteCntArray[] = { sizeof(asSubIdx), sizeof(id), msgByteCnt };
  return cmAudioSysDeliverSegMsg(h,msgDataPtrArray,msgByteCntArray,3,srcNetNodeId);
}

unsigned  cmAudioSysIsMsgWaiting( cmAudioSysH_t h )
{ 
  cmAsRC_t rc;
  cmAs_t* p = _cmAsHandleToPtr(h);

  // the system must be initialized to use this function
  if((rc = _cmAudioSysVerifyInit(p)) != kOkAsRC )
    return 0;

  unsigned n = 0;
  unsigned retByteCnt;

  for(n=0; n < p->ssCnt; ++n )
  {
    //if( (retByteCnt = cmTsMp1cDequeueMsgByteCount(p->ssArray[p->waitAsSubIdx].dthQueueH)) > 0 )
    if( (retByteCnt = cmTsMp1cDequeueMsgByteCount(p->dthQueH)) > 0 )
      return retByteCnt;

    p->waitAsSubIdx = (p->waitAsSubIdx + 1) % p->ssCnt;
  }  

  return 0;
}

cmAsRC_t  cmAudioSysReceiveMsg( cmAudioSysH_t h, void* msgDataPtr, unsigned msgByteCnt )
{
  cmAsRC_t rc;
  cmAs_t* p = _cmAsHandleToPtr(h);

  // the system must be initialized to use this function
  if((rc = _cmAudioSysVerifyInit(p)) != kOkAsRC )
    return rc;

  //switch( cmTsMp1cDequeueMsg(p->ssArray[p->waitAsSubIdx].dthQueueH,msgDataPtr,msgByteCnt) )
  switch( cmTsMp1cDequeueMsg(p->dthQueH,msgDataPtr,msgByteCnt) )
  {
    case kOkThRC:          
      p->waitAsSubIdx = (p->waitAsSubIdx + 1) % p->ssCnt;  
      return kOkAsRC;

    case kBufTooSmallThRC: 
      return kBufTooSmallAsRC;

    case kBufEmptyThRC:    
      return kNoMsgWaitingAsRC;
  }

  return _cmAsError(p,kTsQueueErrAsRC,"A deque operation failed on the DSP-to-Host message queue.");
}


void cmAudioSysStatus( cmAudioSysH_t h, unsigned asSubIdx, cmAudioSysStatus_t* statusPtr )
{
  cmAs_t* p = _cmAsHandleToPtr(h);

  // the system must be initialized to use this function
  if( _cmAudioSysVerifyInit(p) != kOkAsRC )
    return;

  if( asSubIdx < p->ssCnt )
    *statusPtr = p->ssArray[asSubIdx].status;
}

void cmAudioSysStatusNotifyEnable( cmAudioSysH_t h, unsigned asSubIdx, bool enableFl )
{
  cmAs_t*  p = _cmAsHandleToPtr(h);

  // the system must be initialized to use this function
  if( _cmAudioSysVerifyInit(p) != kOkAsRC )
    return;

  unsigned i = asSubIdx == cmInvalidIdx ? 0 : asSubIdx;
  unsigned n = asSubIdx == cmInvalidIdx ? p->ssCnt : asSubIdx+1;
  for(; i<n; ++i)
    p->ssArray[i].statusFl = enableFl;
}

bool cmAudioSysHandleIsValid( cmAudioSysH_t h )
{ return h.h != NULL; }

cmAudioSysCtx_t* cmAudioSysContext( cmAudioSysH_t h, unsigned asSubIdx )
{
  cmAs_t* p = _cmAsHandleToPtr(h);

  if( _cmAudioSysVerifyInit(p) != kOkAsRC )
    return NULL;

  return &p->ssArray[asSubIdx].ctx;
}

unsigned cmAudioSysSubSystemCount( cmAudioSysH_t h )
{
  cmAs_t* p = _cmAsHandleToPtr(h);
  if( _cmAudioSysVerifyInit(p) != kOkAsRC )
    return 0;

  return p->ssCnt;
}

//===========================================================================================================================
//
//  cmAudioSysTest()
//

//{ { label:cmAudioSysTest }
//(
// cmAudioSysTest() demonstrates the audio system usage.
//)

//(
 
typedef struct 
{
  double   hz;       // current synth frq
  long     phs;      // current synth phase
  double   srate;    // audio sample rate
  unsigned cbCnt;    // DSP cycle count
  bool     synthFl;  // true=synth false=pass through
} _cmAsTestCbRecd;

typedef struct
{
  unsigned asSubIdx; // asSubIdx must always be the first field in the msg
  unsigned id;   // 0 = set DSP Hz, 1 = report cbCount to host
  double   hz;   
  unsigned uint;
} _cmAsTestMsg;


long _cmAsSynthSine( _cmAsTestCbRecd* r, cmApSample_t* p, unsigned chCnt, unsigned frmCnt )
{
  long     ph = 0;
  unsigned i;


  for(i=0; i<chCnt; ++i)
  {
    unsigned      j;
    cmApSample_t* op = p + i;

    ph    = r->phs;
    for(j=0; j<frmCnt; j++, op+=chCnt, ph++)
      *op = (cmApSample_t)(0.9 * sin( 2.0 * M_PI * r->hz * ph / r->srate ));
  }
  
  return ph;
}

unsigned _cmAsTestChIdx = 0;

cmRC_t _cmAsTestCb( void* cbPtr, unsigned msgByteCnt, const void* msgDataPtr )
{
  cmRC_t              rc  = cmOkRC;
  cmAudioSysCtx_t*    ctx = (cmAudioSysCtx_t*)cbPtr;
  cmAudioSysSubSys_t* ss  = ctx->ss;
  _cmAsTestCbRecd*    r   = (_cmAsTestCbRecd*)ss->cbDataPtr;

  // update the calback counter
  ++r->cbCnt;

  // if this is an audio update request
  if( msgByteCnt == 0 )
  {
    unsigned      i;
    if( r->synthFl )
    {
      long phs = 0;
      if(0)
      {
        for(i=0; i<ctx->oChCnt; ++i)
          if( ctx->oChArray[i] != NULL )
            phs = _cmAsSynthSine(r, ctx->oChArray[i], 1, ss->args.dspFramesPerCycle );
      }
      else
      {
        if( _cmAsTestChIdx < ctx->oChCnt )
          phs = _cmAsSynthSine(r, ctx->oChArray[_cmAsTestChIdx], 1, ss->args.dspFramesPerCycle );
      }

      r->phs = phs;
    }
    else
    {
      // BUG BUG BUG - this assumes that the input and output channels are the same.
      unsigned chCnt = cmMin(ctx->oChCnt,ctx->iChCnt);
      for(i=0; i<chCnt; ++i)
        memcpy(ctx->oChArray[i],ctx->iChArray[i],sizeof(cmSample_t)*ss->args.dspFramesPerCycle);
    }

  }
  else // ... otherwise it is a msg for the DSP process from the host
  {
    _cmAsTestMsg* msg = (_cmAsTestMsg*)msgDataPtr;

    msg->asSubIdx = ctx->asSubIdx;

    switch(msg->id)
    {
      case 0:
        r->hz = msg->hz;
        break;

      case 1:
        msg->uint = r->cbCnt;
        msgByteCnt = sizeof(_cmAsTestMsg);
        rc = ctx->dspToHostFunc(ctx,(const void **)&msg,&msgByteCnt,1);
        break;
    }

  }
  
  return rc;
}

// print the usage message for cmAudioPortTest.c
void _cmAsPrintUsage( cmRpt_t* rpt )
{
char msg[] =
  "cmAudioSysTest() command switches:\n"
  "-r <srate> -c <chcnt> -b <bufcnt> -f <frmcnt> -i <idevidx> -o <odevidx> -m <msgqsize> -d <dspsize> -t -p -h \n"
  "\n"
  "-r <srate> = sample rate (48000)\n"
  "-c <chcnt> = audio channels (2)\n"
  "-b <bufcnt> = count of buffers (3)\n"
  "-f <frmcnt> = count of samples per buffer (512)\n"
  "-i <idevidx> = input device index (0)\n"
  "-o <odevidx> = output device index (2)\n"
  "-m <msgqsize> = message queue byte count (1024)\n"
  "-d <dspsize>  = samples per DSP frame (64)\n" 
  "-s = true: sync to input port false: sync to output port\n"
  "-t = copy input to output otherwise synthesize a 1000 Hz sine (false)\n"
  "-p = report but don't start audio devices\n"
  "-h = print this usage message\n";

 cmRptPrintf(rpt,"%s",msg);
}

// Get a command line option.
int _cmAsGetOpt( int argc, const char* argv[], const char* label, int defaultVal, bool boolFl )
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

bool _cmAsGetBoolOpt( int argc, const char* argv[], const char* label, bool defaultVal )
{ return _cmAsGetOpt(argc,argv,label,defaultVal?1:0,true)!=0; }

int _cmAsGetIntOpt( int argc, const char* argv[], const char* label, int defaultVal )
{ return _cmAsGetOpt(argc,argv,label,defaultVal,false); }


void cmAudioSysTest( cmRpt_t* rpt, int argc, const char* argv[] )
{
  cmAudioSysCfg_t    cfg;
  cmAudioSysSubSys_t ss;
  cmAudioSysH_t      h      = cmAudioSysNullHandle;
  cmAudioSysStatus_t status;
  _cmAsTestCbRecd    cbRecd = {1000.0,0,48000.0,0};

  cfg.ssArray = &ss;
  cfg.ssCnt   = 1;
  //cfg.afpArray= NULL;
  //cfg.afpCnt  = 0;
  cfg.meterMs = 50;

  if(_cmAsGetBoolOpt(argc,argv,"-h",false))
    _cmAsPrintUsage(rpt);

  cbRecd.srate   = _cmAsGetIntOpt(argc,argv,"-r",48000);
  cbRecd.synthFl = _cmAsGetBoolOpt(argc,argv,"-t",false)==false;

  ss.args.rpt               = rpt;
  ss.args.inDevIdx          = _cmAsGetIntOpt( argc,argv,"-i",0); 
  ss.args.outDevIdx         = _cmAsGetIntOpt( argc,argv,"-o",2); 
  ss.args.syncInputFl       = _cmAsGetBoolOpt(argc,argv,"-s",true);
  ss.args.msgQueueByteCnt   = _cmAsGetIntOpt( argc,argv,"-m",8192);
  ss.args.devFramesPerCycle = _cmAsGetIntOpt( argc,argv,"-f",512);
  ss.args.dspFramesPerCycle = _cmAsGetIntOpt( argc,argv,"-d",64);;
  ss.args.audioBufCnt       = _cmAsGetIntOpt( argc,argv,"-b",3);       
  ss.args.srate             = cbRecd.srate;
  ss.cbFunc                 = _cmAsTestCb;                            // set the DSP entry function
  ss.cbDataPtr              = &cbRecd;                                // set the DSP function argument record

  cmRptPrintf(rpt,"in:%i out:%i syncFl:%i que:%i fpc:%i dsp:%i bufs:%i sr:%f\n",ss.args.inDevIdx,ss.args.outDevIdx,ss.args.syncInputFl,
    ss.args.msgQueueByteCnt,ss.args.devFramesPerCycle,ss.args.dspFramesPerCycle,ss.args.audioBufCnt,ss.args.srate);

  if( cmApNrtAllocate(rpt) != kOkApRC )
    goto errLabel;

  if( cmApFileAllocate(rpt) != kOkApRC )
    goto errLabel;

  // initialize the audio device system
  if( cmApInitialize(rpt) != kOkApRC )
    goto errLabel;

  cmApReport(rpt);

  // initialize the audio buffer
  if( cmApBufInitialize( cmApDeviceCount(), cfg.meterMs ) != kOkApRC )
    goto errLabel;

  // initialize the audio system
  if( cmAudioSysAllocate(&h,rpt,&cfg) != kOkAsRC )
    goto errLabel;
  
  // start the audio system
  cmAudioSysEnable(h,true);

  char c = 0;
  printf("q=quit a-g=note n=ch r=rqst s=status\n");

  // simulate a host event loop
  while(c != 'q')
  {
    _cmAsTestMsg msg = {0,0,0,0};
    bool         fl  = true;

    // wait here for a key press
    c =(char)fgetc(stdin);
    fflush(stdin);
    
    
    switch(c)
    {
      case 'c': msg.hz = cmMidiToHz(60); break;
      case 'd': msg.hz = cmMidiToHz(62); break;
      case 'e': msg.hz = cmMidiToHz(64); break;
      case 'f': msg.hz = cmMidiToHz(65); break;
      case 'g': msg.hz = cmMidiToHz(67); break;
      case 'a': msg.hz = cmMidiToHz(69); break;
      case 'b': msg.hz = cmMidiToHz(71); break;

      case 'r': msg.id = 1; break; // request DSP process to send a callback count

      case 'n': ++_cmAsTestChIdx; printf("ch:%i\n",_cmAsTestChIdx); break;

      case 's':  
        // report the audio system status
        cmAudioSysStatus(h,0,&status);
        printf("phs:%li cb count:%i (upd:%i wake:%i acb:%i msgs:%i)\n",cbRecd.phs, cbRecd.cbCnt, status.updateCnt, status.wakeupCnt, status.audioCbCnt, status.msgCbCnt);
        //printf("%f \n",status.oMeterArray[0]);
        fl = false;
        break;

      default:
        fl=false;
        
    }
    
    if( fl )
    {
      // transmit a command to the DSP process
      cmAudioSysDeliverMsg(h,&msg, sizeof(msg), cmInvalidId);
    }


    // check if messages are waiting to be delivered from the DSP process
    unsigned msgByteCnt;
    if((msgByteCnt = cmAudioSysIsMsgWaiting(h)) > 0 )
    {
      char buf[ msgByteCnt ];

      // rcv a msg from the DSP process
      if( cmAudioSysReceiveMsg(h,buf,msgByteCnt) == kOkAsRC )
      {
        _cmAsTestMsg* msg = (_cmAsTestMsg*)buf;
        switch(msg->id)
        {
          case 1:
            printf("RCV: Callback count:%i\n",msg->uint);
            break;
        }

      }
    }

    // report the audio buffer status
    //cmApBufReport(ss.args.rpt);
  }

  // stop the audio system
  cmAudioSysEnable(h,false);

 
  goto exitLabel;

 errLabel:
  printf("AUDIO SYSTEM TEST ERROR\n");

 exitLabel:

  cmAudioSysFree(&h);
  cmApFinalize();
  cmApFileFree();
  cmApNrtFree();
  cmApBufFinalize();

}

//)
//}
