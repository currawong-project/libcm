#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmAudioPort.h"
#include "cmAudioNrtDev.h"
#include "cmAudioPortFile.h"
#include "cmApBuf.h"
#include "cmJson.h"
#include "cmThread.h"
#include "cmUdpPort.h"
#include "cmUdpNet.h"
#include "cmRtSysMsg.h"
#include "cmRtSys.h"
#include "cmMidi.h"
#include "cmMidiPort.h"

#include "cmMath.h"

typedef enum
{
  kNoCmdId,
  kEnableCbCmdId,
  kDisableCbCmdId
} kRtCmdId_t;

cmRtSysH_t cmRtSysNullHandle = { NULL };

struct cmRt_str;

typedef struct
{
  struct cmRt_str* p;           // pointer to the audio system instance which owns this sub-system
  cmRtSysSubSys_t  ss;          // sub-system configuration record
  cmRtSysCtx_t     ctx;         // DSP context
  cmRtSysStatus_t  status;      // current runtime status of this sub-system
  cmThreadH_t      threadH;     // audio system thread
  cmTsMp1cH_t      htdQueueH;   // host-to-dsp thread safe msg queue
  cmThreadMutexH_t engMutexH;   // thread mutex and condition variable
  cmUdpNetH_t      netH;
  bool             runFl;       // false during finalization otherwise true
  bool             statusFl;    // true if regular status notifications should be sent
  bool             syncInputFl;

  kRtCmdId_t       cmdId;       // written by app thread, read by rt thread
  unsigned         cbEnableFl;  // written by rt thread, read by app thread

  double*     iMeterArray;      //  
  double*     oMeterArray;      //

  unsigned    statusUpdateSmpCnt; // transmit a state update msg every statusUpdateSmpCnt samples
  unsigned    statusUpdateSmpIdx; // state update phase 

} _cmRtCfg_t;

typedef struct cmRt_str
{
  cmErr_t     err;
  _cmRtCfg_t* ssArray;
  unsigned    ssCnt;
  unsigned    waitRtSubIdx; // index of the next sub-system to try with cmRtSysIsMsgWaiting().
  cmTsMp1cH_t dthQueH;
  bool        initFl;       // true if the audio system is initialized
} cmRt_t;


cmRt_t* _cmRtHandleToPtr( cmRtSysH_t h )
{
  cmRt_t* p = (cmRt_t*)h.h;
  assert(p != NULL);
  return p;
}

cmRtRC_t _cmRtError( cmRt_t* p, cmRtRC_t rc, const char* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  cmErrVMsg(&p->err,rc,fmt,vl);
  va_end(vl);
  return rc;
}

// Wrapper function to put msgs into thread safe queues and handle related errors.
cmRtRC_t _cmRtEnqueueMsg( cmRt_t* p, cmTsMp1cH_t qH, const void* msgDataPtrArray[], unsigned msgCntArray[], unsigned segCnt, const char* queueLabel )
{
  cmRtRC_t rc = kOkRtRC;

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

        rc = _cmRtError(p,kMsgEnqueueFailRtRC,"The %s queue was unable to load a msg containing %i bytes. The queue is currently allocated %i bytes and has %i bytes available.",queueLabel,byteCnt,cmTsMp1cAllocByteCount(qH),cmTsMp1cAvailByteCount(qH));
      }
      break;

    default:
      rc = _cmRtError(p,kMsgEnqueueFailRtRC,"A %s msg. enqueue failed.",queueLabel);
  }
 
  return rc;
}

// This is the function pointed to by ctx->dspToHostFunc.
// It is called by the DSP proces to pass msgs to the host.
// therefore it is always called from inside of _cmRtDspExecCallback().
cmRtRC_t _cmRtDspToHostMsgCallback(struct cmRtSysCtx_str* ctx, const void* msgDataPtrArray[], unsigned msgByteCntArray[], unsigned msgSegCnt)
{ 
  cmRt_t* p = (cmRt_t*)ctx->reserved;
  assert( ctx->rtSubIdx < p->ssCnt );
  return _cmRtEnqueueMsg(p,p->dthQueH,msgDataPtrArray,msgByteCntArray,msgSegCnt,"DSP-to-Host"); 
} 

cmRtRC_t _cmRtSysDspToHostSegMsg( cmRt_t* p, const void* msgDataPtrArray[], unsigned msgByteCntArray[], unsigned msgSegCnt)
{
  return _cmRtEnqueueMsg(p,p->dthQueH,msgDataPtrArray,msgByteCntArray,msgSegCnt,"DSP-to-Host"); 
}

cmRtRC_t cmRtSysDspToHostSegMsg( cmRtSysH_t h, const void* msgDataPtrArray[], unsigned msgByteCntArray[], unsigned msgSegCnt)
{
  cmRt_t* p = _cmRtHandleToPtr(h);
  return _cmRtSysDspToHostSegMsg(p,msgDataPtrArray,msgByteCntArray,msgSegCnt);
}

cmRtRC_t cmRtSysDspToHost( cmRtSysH_t h, const void* msgDataPtr, unsigned msgByteCnt)
{
  const void* msgDataArray[] = { msgDataPtr };
  unsigned   msgByteCntArray[] = { msgByteCnt };
  return cmRtSysDspToHostSegMsg(h,msgDataArray,msgByteCntArray,1);
}

cmRtRC_t  _cmRtParseNonSubSysMsg(  cmRt_t* p, const void* msg, unsigned msgByteCnt )
{
  cmRtRC_t rc = kOkRtRC;
  cmRtSysMstr_t* m = (cmRtSysMstr_t*)msg;
  /*
  unsigned devIdx = cmRtSysUiInstIdToDevIndex(h->instId);
  unsigned chIdx  = cmRtSysUiInstIdToChIndex(h->instId);
  unsigned inFl   = cmRtSysUiInstIdToInFlag(h->instId);
  unsigned ctlId  = cmRtSysUiInstIdToCtlId(h->instId);
  */

  // if the valuu associated with this msg is a mtx then set
  // its mtx data area pointer to just after the msg header.
  //if( cmDsvIsMtx(&h->value) )
  //  h->value.u.m.u.vp = ((char*)msg) + sizeof(cmDspUiHdr_t);

  unsigned flags = m->inFl ? kInApFl : kOutApFl;
  
  switch( m->ctlId )
  {
    
    case kSliderUiRtId: // slider
      cmApBufSetGain(m->devIdx,m->chIdx, flags, m->value); 
      break;
      
    case kMeterUiRtId: // meter
      break;
      
    case kMuteUiRtId: // mute
      flags += m->value == 0 ? kEnableApFl : 0;
      cmApBufEnableChannel(m->devIdx,m->chIdx,flags);
      break;

    case kToneUiRtId: // tone
      flags += m->value > 0 ? kEnableApFl : 0;
      cmApBufEnableTone(m->devIdx,m->chIdx,flags);
      break;

    case kPassUiRtId: // pass
      flags += m->value > 0 ? kEnableApFl : 0;
      cmApBufEnablePass(m->devIdx,m->chIdx,flags);
      break;

    default:
      { assert(0); }
  }

  return rc;
}

// Process a UI msg sent from the host to the audio system  
cmRtRC_t  _cmRtHandleNonSubSysMsg(  cmRt_t* p, const void* msgDataPtrArray[], unsigned msgByteCntArray[], unsigned msgSegCnt )
{
  cmRtRC_t rc = kOkRtRC;

  // if the message is contained in a single segment it can be dispatched immediately ...
  if( msgSegCnt == 1 )
    rc = _cmRtParseNonSubSysMsg(p,msgDataPtrArray[0],msgByteCntArray[0]);
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
    rc = _cmRtParseNonSubSysMsg(p,buf,byteCnt);

  }
  
  return rc;
}


cmRtRC_t _cmRtSendStateStatusToHost(  _cmRtCfg_t* cp )
{
  cmRtRC_t rc    = kOkRtRC;
  
  cp->status.hdr.rtSubIdx = cp->ctx.rtSubIdx;
  cp->status.hdr.selId    = kStatusSelRtId;

  cmApBufGetStatus( cp->ss.args.inDevIdx,  kInApFl,  cp->iMeterArray, cp->status.iMeterCnt, &cp->status.overflowCnt );
  cmApBufGetStatus( cp->ss.args.outDevIdx, kOutApFl, cp->oMeterArray, cp->status.oMeterCnt, &cp->status.underflowCnt );

  unsigned    iMeterByteCnt     = sizeof(cp->iMeterArray[0]) * cp->status.iMeterCnt;
  unsigned    oMeterByteCnt     = sizeof(cp->oMeterArray[0]) * cp->status.oMeterCnt;  
  const void* msgDataPtrArray[] = { &cp->status,        cp->iMeterArray, cp->oMeterArray };
  unsigned    msgByteCntArray[] = { sizeof(cp->status),     iMeterByteCnt,   oMeterByteCnt };
  unsigned    segCnt            = sizeof(msgByteCntArray)/sizeof(unsigned);

  _cmRtSysDspToHostSegMsg(cp->p,msgDataPtrArray,msgByteCntArray, segCnt );

  return rc;
}


// The DSP execution callback happens through this function.
// This function is only called from inside _cmRtThreadCallback() 
// with the engine mutex locked.
void _cmRtDspExecCallback( _cmRtCfg_t* cp )
{
  
  // Fill iChArray[] and oChArray[] with pointers to the incoming and outgoing sample buffers.
  // Notes:
  //   1) Buffers associated with disabled input/output channels will be set to NULL in iChArray[]/oChArray[].
  //   2) Buffers associated with channels marked for pass-through will be set to NULL in oChArray[].
  //   3) All samples returned in oChArray[] buffers will be set to zero.
  cmApBufGetIO(cp->ss.args.inDevIdx, cp->ctx.iChArray, cp->ctx.iChCnt, cp->ss.args.outDevIdx, cp->ctx.oChArray, cp->ctx.oChCnt  );

  // call the application provided DSP process
  if( cp->cbEnableFl )
  {
    cp->ctx.audioRateFl = true;
    cp->ss.cbFunc( &cp->ctx, 0, NULL );
    cp->ctx.audioRateFl = false;
  }

  // Notice client callback enable/disable 
  // requests from the client thread
  switch( cp->cmdId )
  {
    case kNoCmdId:
      break;

    case kDisableCbCmdId: 
      if( cp->cbEnableFl )
        cmThUIntDecr(&cp->cbEnableFl,1);  
      break;                               

    case kEnableCbCmdId:
      if( cp->cbEnableFl==0)
        cmThUIntIncr(&cp->cbEnableFl,1);
      break;
  }

  // advance the audio buffer
  cmApBufAdvance( cp->ss.args.outDevIdx, kOutApFl );
  cmApBufAdvance( cp->ss.args.inDevIdx,  kInApFl  );

  // handle periodic status messages to the host
  if( (cp->statusUpdateSmpIdx += cp->ss.args.dspFramesPerCycle) >= cp->statusUpdateSmpCnt )
  {
    cp->statusUpdateSmpIdx -= cp->statusUpdateSmpCnt;

    if( cp->statusFl )
      _cmRtSendStateStatusToHost(cp);
  }

}

// Returns true if audio buffer is has waiting incoming samples and
// available outgoing space. 
bool _cmRtBufIsReady( const _cmRtCfg_t* cp )
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


// This is only called with _cmRtRecd.engMutexH locked
cmRtRC_t _cmRtDeliverMsgsWithLock( _cmRtCfg_t* cp  )
{
  int      i;
  cmRtRC_t rc = kOkThRC;
    
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
// via calls to cmRtDeliverMsg(). 
// When the audio buffers need to be serviced the audio device callback
// signals the cond. var. which results in this thread waking up (and
// simultaneously locking the mutex) as soon as the mutex is available. 
bool _cmRtThreadCallback(void* arg)
{
  cmRtRC_t rc;
  _cmRtCfg_t*  cp = (_cmRtCfg_t*)arg;

  // lock the cmRtSys mutex
  if((rc = cmThreadMutexLock(cp->engMutexH)) != kOkRtRC )
  {
    _cmRtError(cp->p,rc,"The cmRtSys thread mutex lock failed.");
    return false;
  }

  // runFl is always set except during finalization 
  while( cp->runFl )
  {

    // if the buffer is NOT ready or the cmRtSys is disabled
    if(_cmRtBufIsReady(cp) == false || cp->cbEnableFl==false )
    {
      // block on the cond var and unlock the mutex
      if( cmThreadMutexWaitOnCondVar(cp->engMutexH,false) != kOkRtRC )
      {
        cmThreadMutexUnlock(cp->engMutexH);
        _cmRtError(cp->p,rc,"The cmRtSys cond. var. wait failed.");
        return false;
      }

      //
      // the cond var was signaled and the mutex is now locked 
      //
      ++cp->status.wakeupCnt;
    }

    // be sure we are still enabled and the buffer is still ready
    if( 1 /*cp->runFl*/ )
    {
      while( cp->runFl && _cmRtBufIsReady(cp) )
      {
        ++cp->status.audioCbCnt;
        
        // calling this function results in callbacks to cmAudDsp.c:_cmAdUdpNetCallback()
        // which in turn calls cmRtSysDeliverMsg() which queues any incoming messages
        // which are then transferred to the DSP processes by the the call to 
        // _cmRtDeliverMsgWithLock() below.
        cmUdpNetReceive(cp->netH,NULL);
    
        // if there are msgs waiting to be sent to the DSP process send them. 
        if( cp->cbEnableFl )
          if( cmTsMp1cMsgWaiting(cp->htdQueueH) )
            _cmRtDeliverMsgsWithLock(cp); 

        // make the cmRtSys callback
        _cmRtDspExecCallback( cp ); 

        // update the signal time
        cp->ctx.begSmpIdx += cp->ss.args.dspFramesPerCycle;
      }
    }
   
  } 
  
  // unlock the mutex
  cmThreadMutexUnlock(cp->engMutexH);

  return true;
}

void _cmRtGenSignal( cmApAudioPacket_t* outPktArray, unsigned outPktCnt, bool sineFl )
{
  static unsigned rtPhase = 0;

  //fill output with noise
  unsigned i = 0,j =0, k = 0, phs = 0;
  for(; i<outPktCnt; ++i)
  {
    cmApAudioPacket_t*  a = outPktArray + i;
    cmApSample_t*      dp = (cmApSample_t*)a->audioBytesPtr;

    phs = a->audioFramesCnt;

    if( sineFl )
    {
      for(j=0; j<a->audioFramesCnt; ++j)
      {
        cmApSample_t v = (cmApSample_t)(0.7 * sin(2*M_PI/44100.0 * rtPhase + j ));
        
        for(k=0; k<a->chCnt; ++k,++dp)
          *dp = v;
      }
    }
    else
    {
      for(j=0; j<a->audioFramesCnt*a->chCnt; ++j,++dp)
        *dp = (cmApSample_t)(rand() - (RAND_MAX/2))/(RAND_MAX/2);
    }
  }
    
  rtPhase += phs;

}

// This is the audio port callback function.
//
// _cmRtSysAudioUpdate() assumes that at most two audio device threads 
// (input and output) may call it.   cmApBufUpdate() is safe under these conditions 
// since the input and output buffers are updated separately.
// p->syncInputFl is used to allow either the input or output thread to signal
// the condition variable.  This flag is necessary to prevent both threads from simultaneously
// attempting to signal the condition variable (which will lock the system). 
// 
// If more than two audio device threads call the function then this function is not safe.
void   _cmRtSysAudioUpdate( cmApAudioPacket_t* inPktArray, unsigned inPktCnt, cmApAudioPacket_t* outPktArray, unsigned outPktCnt )
{
  _cmRtCfg_t* cp = (_cmRtCfg_t*)(inPktArray!=NULL ? inPktArray[0].userCbPtr : outPktArray[0].userCbPtr);

  ++cp->status.updateCnt;

  if( cp->runFl )
  {

    // transfer incoming/outgoing samples from/to the audio device
    cmApBufUpdate(inPktArray,inPktCnt,outPktArray,outPktCnt);

   
    // generate a test signal
    //_cmRtGenSignal( cmApAudioPacket_t* outPktArray, unsigned outPktCnt, bool sineFl );
    //return;

    bool testBufFl = (cp->syncInputFl==true && inPktCnt>0) || (cp->syncInputFl==false && outPktCnt>0);

    //printf("%i %i %i %i\n",testBufFl,cp->syncInputFl,inPktCnt,outPktCnt);

    // if the input/output buffer contain samples to be processed then signal the condition variable 
    // - this will cause the audio system thread to unblock and the used defined DSP process will be called.
    if( testBufFl && _cmRtBufIsReady(cp) )
    {
      if( cmThreadMutexSignalCondVar(cp->engMutexH) != kOkThRC )
        _cmRtError(cp->p,kMutexErrRtRC,"CmRtSys signal cond. var. failed.");
      
    }
  }

}

// Called when MIDI messages arrive from external MIDI ports.
void _cmRtSysMidiCallback( const cmMidiPacket_t* pktArray, unsigned pktCnt )
{
    unsigned i;
    for(i=0; i<pktCnt; ++i)
    {
      const cmMidiPacket_t* pkt = pktArray + i;
      _cmRtCfg_t*           cp  = (_cmRtCfg_t*)(pkt->cbDataPtr);

      if( !cp->runFl  )
        continue;

      cmRtSysH_t    asH;
      asH.h = cp->p; 

      cmRtSysMidi_t m;
      m.hdr.rtSubIdx = cp->ctx.rtSubIdx;
      m.hdr.selId    = kMidiMsgArraySelRtId;
      m.devIdx       = pkt->devIdx;
      m.portIdx      = pkt->portIdx;
      m.msgCnt       = pkt->msgCnt;
      
      
      /*
      unsigned    selId             = kMidiMsgArraySelRtId;
      const void* msgPtrArray[]     = { &cp->ctx.rtSubIdx,        &selId,        &pkt->devIdx,        &pkt->portIdx,        &pkt->msgCnt,        pkt->msgArray };
      unsigned    msgByteCntArray[] = { sizeof(cp->ctx.rtSubIdx), sizeof(selId), sizeof(pkt->devIdx), sizeof(pkt->portIdx), sizeof(pkt->msgCnt), pkt->msgCnt*sizeof(cmMidiMsg) };
      unsigned    msgSegCnt         = sizeof(msgByteCntArray)/sizeof(unsigned);
      */

      const void* msgPtrArray[]     = { &m,        pkt->msgArray };
      unsigned    msgByteCntArray[] = { sizeof(m), pkt->msgCnt*sizeof(cmMidiMsg) };
      unsigned    msgSegCnt         = sizeof(msgByteCntArray)/sizeof(unsigned);

      cmRtSysDeliverSegMsg(asH,msgPtrArray,msgByteCntArray,msgSegCnt,cmInvalidId);
    }

}

cmRtRC_t cmRtSysAllocate( cmRtSysH_t* hp, cmRpt_t* rpt, const cmRtSysCfg_t* cfg )
{
  cmRtRC_t rc;

  if((rc = cmRtSysFree(hp)) != kOkRtRC )
    return rc;

  cmRt_t*  p = cmMemAllocZ( cmRt_t, 1 );

  cmErrSetup(&p->err,rpt,"Audio System");

  hp->h = p;

  if( cfg != NULL )
    if((rc = cmRtSysInitialize( *hp, cfg )) != kOkRtRC )
      cmRtSysFree(hp);

  return rc;
}

cmRtRC_t cmRtSysFree( cmRtSysH_t* hp )
{
  cmRtRC_t rc;

  if( hp == NULL || hp->h == NULL )
    return kOkRtRC;

  if((rc = cmRtSysFinalize(*hp)) != kOkRtRC )
    return rc;

  cmRt_t* p = _cmRtHandleToPtr(*hp);

  cmMemFree(p);

  hp->h = NULL;

  return rc;
}

cmRtRC_t _cmRtSysEnable( cmRt_t* p, bool enableFl )
{
  cmRtRC_t rc = kOkRtRC;

  unsigned i;
  unsigned n;
  unsigned tickMs = 20;
  unsigned timeOutMs = 10000;

  for(i=0; i<p->ssCnt; ++i)
  {
    _cmRtCfg_t* cp = p->ssArray + i;

    if( enableFl )
    {
      cp->cmdId = kNoCmdId;
      cmThUIntIncr(&cp->cmdId,kEnableCbCmdId);
      
      for(n=0; n<timeOutMs && cp->cbEnableFl==false; n+=tickMs )
        cmSleepMs(tickMs);        

      cmThUIntDecr(&cp->cmdId,kEnableCbCmdId);

    } 
    else
    {
      cp->cmdId = kNoCmdId;
      cmThUIntIncr(&cp->cmdId,kDisableCbCmdId);

      // wait for the rt thread to return from a client callbacks
      for(n=0; n<timeOutMs && cp->cbEnableFl; n+=tickMs )
        cmSleepMs(tickMs);        

      cmThUIntDecr(&cp->cmdId,kDisableCbCmdId);

    }

    if( n >= timeOutMs )
      rc = cmErrMsg(&p->err,kTimeOutErrRtRC,"RT System %s timed out after %i milliseconds.",enableFl?"enable":"disable",timeOutMs);

  }

  return rc;
}

cmRtRC_t _cmRtSysFinalize( cmRt_t* p )
{
  cmRtRC_t rc = kOkRtRC;
  unsigned i;
  
  // mark  the audio system as NOT initialized
  p->initFl = false;

  // be sure all audio callbacks are disabled before continuing.
  if((rc = _cmRtSysEnable(p,false)) != kOkRtRC )
    return _cmRtError(p,rc,"Audio system finalize failed because device halting failed.");

  // stop the audio devices 
  for(i=0; i<p->ssCnt; ++i)
  {
    _cmRtCfg_t* cp = p->ssArray + i;

    // stop the input device
    if((rc = cmApDeviceStop( cp->ss.args.inDevIdx )) != kOkRtRC )
      return _cmRtError(p,kAudioDevStopFailRtRC,"The audio input device stop failed.");

    // stop the output device
    if((rc = cmApDeviceStop( cp->ss.args.outDevIdx )) != kOkRtRC )
      return _cmRtError(p,kAudioDevStopFailRtRC,"The audio output device stop failed.");
  }


  for(i=0; i<p->ssCnt; ++i)
  {
    _cmRtCfg_t* cp = p->ssArray + i;

    if( cmThreadIsValid( cp->threadH ))
    {
      // inform the thread that it should exit
      cp->runFl    = false;
      cp->statusFl = false;


      // signal the cond var to cause the thread to run
      if((rc = cmThreadMutexSignalCondVar(cp->engMutexH)) != kOkThRC )
        _cmRtError(p,kMutexErrRtRC,"Finalize signal cond. var. failed.");

      // wait to take control of the mutex - this will occur when the thread function exits
      if((rc = cmThreadMutexLock(cp->engMutexH)) != kOkThRC )
        _cmRtError(p,kMutexErrRtRC,"Finalize lock failed.");

      // unlock the mutex because it is no longer needed and must be unlocked to be destroyed
      if((rc = cmThreadMutexUnlock(cp->engMutexH)) != kOkThRC )
        _cmRtError(p,kMutexErrRtRC,"Finalize unlock failed.");

      // destroy the thread
      if((rc = cmThreadDestroy( &cp->threadH )) != kOkThRC )
        _cmRtError(p,kThreadErrRtRC,"Thread destroy failed.");   

    }

    // destroy the mutex
    if( cmThreadMutexIsValid(cp->engMutexH) )
      if((rc = cmThreadMutexDestroy( &cp->engMutexH )) != kOkThRC )
        _cmRtError(p,kMutexErrRtRC,"Mutex destroy failed.");


    // remove the MIDI callback
    if( cmMpIsInitialized() && cmMpUsesCallback(-1,-1, _cmRtSysMidiCallback, cp) )
      if( cmMpRemoveCallback( -1, -1, _cmRtSysMidiCallback, cp ) != kOkMpRC )
        _cmRtError(p,kMidiSysFailRtRC,"MIDI callback removal failed.");

    // destroy the host-to-dsp msg queue
    if( cmTsMp1cIsValid(cp->htdQueueH ) )
      if((rc = cmTsMp1cDestroy( &cp->htdQueueH )) != kOkThRC )
        _cmRtError(p,kTsQueueErrRtRC,"Host-to-DSP msg queue destroy failed.");

    // destroy the dsp-to-host msg queue
    if( cmTsMp1cIsValid(p->dthQueH) )
      if((rc = cmTsMp1cDestroy( &p->dthQueH )) != kOkThRC )
        _cmRtError(p,kTsQueueErrRtRC,"DSP-to-Host msg queue destroy failed.");


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
cmRtRC_t _cmRtSysValidate( cmErr_t* err, const cmRtSysCfg_t* cfg )
{
  unsigned i,j,k;
  for(i=0; i<2; ++i)
  {
    // examine input devices - then output devices
    bool inputFl  = i==0;
    bool outputFl = !inputFl;

    for(j=0; j<cfg->ssCnt; ++j)
    {
      cmRtSysArgs_t* s0     = &cfg->ssArray[j].args;
      unsigned          devIdx = inputFl ? s0->inDevIdx : s0->outDevIdx;

      for(k=0; k<cfg->ssCnt && devIdx != cmInvalidIdx; ++k)
        if( k != j )
        {
          cmRtSysArgs_t* s1 = &cfg->ssArray[k].args;

          // if the device was used as input or output multple times then signal an error
          if( (inputFl && (s1->inDevIdx == devIdx) && s1->inDevIdx != cmInvalidIdx) || (outputFl && (s1->outDevIdx == devIdx) && s1->outDevIdx != cmInvalidIdx) )
            return cmErrMsg(err,kInvalidArgRtRC,"The device %i was used as an %s by multiple sub-systems.", devIdx, inputFl ? "input" : "output");

          // if this device is being used by another subsystem ...
          if( (inputFl && (s1->outDevIdx == devIdx) && s1->inDevIdx != cmInvalidIdx) || (outputFl && (s1->outDevIdx == devIdx) && s1->outDevIdx != cmInvalidIdx ) )
          {
            // ... then some of its buffer spec's must match 
            if( s0->srate != s1->srate || s0->audioBufCnt != s1->audioBufCnt || s0->dspFramesPerCycle != s1->dspFramesPerCycle || s0->devFramesPerCycle != s1->devFramesPerCycle )
              return cmErrMsg(err,kInvalidArgRtRC,"The device %i is used by different sub-system with different audio buffer parameters.",devIdx);
          }
        }
    }
  }
  
  return kOkRtRC;
}

cmRtRC_t cmRtSysInitialize( cmRtSysH_t h, const cmRtSysCfg_t* cfg )
{
  cmRtRC_t rc;
  unsigned i;
  cmRt_t* p = _cmRtHandleToPtr(h);

  // validate the device setup
  if((rc =_cmRtSysValidate(&p->err, cfg )) != kOkRtRC )
    return rc;

  // always finalize before iniitalize
  if((rc = cmRtSysFinalize(h)) != kOkRtRC )
    return rc;


  p->ssArray = cmMemAllocZ( _cmRtCfg_t, cfg->ssCnt );
  p->ssCnt   = cfg->ssCnt;
  
  for(i=0; i<p->ssCnt; ++i)
  {
    _cmRtCfg_t*               cp = p->ssArray + i;
    const cmRtSysSubSys_t* ss = cfg->ssArray + i;

    cp->p                 = p;
    cp->ss                = *ss;  // copy the cfg into the internal audio system state
    cp->runFl             = false;
    cp->statusFl          = false;
    cp->ctx.reserved      = p;
    cp->ctx.rtSubIdx      = i;
    cp->ctx.ss            = &cp->ss;
    cp->ctx.begSmpIdx     = 0;
    cp->ctx.dspToHostFunc = _cmRtDspToHostMsgCallback;

    // validate the input device index
    if( ss->args.inDevIdx != cmInvalidIdx && ss->args.inDevIdx >= cmApDeviceCount() )
    {
      rc = _cmRtError(p,kAudioDevSetupErrRtRC,"The audio input device index %i is invalid.",ss->args.inDevIdx);
      goto errLabel;
    }

    // validate the output device index
    if( ss->args.outDevIdx != cmInvalidIdx && ss->args.outDevIdx >= cmApDeviceCount() )
    {
      rc =  _cmRtError(p,kAudioDevSetupErrRtRC,"The audio output device index %i is invalid.",ss->args.outDevIdx);
      goto errLabel;
    }

    // setup the input device
    if( ss->args.inDevIdx != cmInvalidIdx )
      if((rc = cmApDeviceSetup( ss->args.inDevIdx, ss->args.srate, ss->args.devFramesPerCycle, _cmRtSysAudioUpdate, cp )) != kOkRtRC )
      {
        rc = _cmRtError(p,kAudioDevSetupErrRtRC,"Audio input device setup failed.");
        goto errLabel;
      }

    // setup the output device
    if( ss->args.outDevIdx != ss->args.inDevIdx && ss->args.outDevIdx != cmInvalidIdx )
      if((rc = cmApDeviceSetup( ss->args.outDevIdx, ss->args.srate, ss->args.devFramesPerCycle, _cmRtSysAudioUpdate, cp )) != kOkRtRC )
      {
        rc =  _cmRtError(p,kAudioDevSetupErrRtRC,"Audio output device setup failed.");
        goto errLabel;
      }

    // setup the input device buffer
    if( ss->args.inDevIdx != cmInvalidIdx )
      if((rc = cmApBufSetup( ss->args.inDevIdx, ss->args.srate, ss->args.dspFramesPerCycle, ss->args.audioBufCnt, cmApDeviceChannelCount(ss->args.inDevIdx, true),  ss->args.devFramesPerCycle, cmApDeviceChannelCount(ss->args.inDevIdx, false), ss->args.devFramesPerCycle )) != kOkRtRC )
      {
        rc = _cmRtError(p,kAudioBufSetupErrRtRC,"Audio buffer input  setup failed.");
        goto errLabel;
      }

    cmApBufEnableMeter(ss->args.inDevIdx, -1, kInApFl  | kEnableApFl );
    cmApBufEnableMeter(ss->args.outDevIdx,-1, kOutApFl | kEnableApFl );

    // setup the input audio buffer ptr array - used to send input audio to the DSP system in _cmRtDspExecCallback()
    if((cp->ctx.iChCnt   = cmApDeviceChannelCount(ss->args.inDevIdx, true)) != 0 )
      cp->ctx.iChArray = cmMemAllocZ( cmSample_t*, cp->ctx.iChCnt );

    // setup the output device buffer
    if( ss->args.outDevIdx != ss->args.inDevIdx )
      if((rc = cmApBufSetup( ss->args.outDevIdx, ss->args.srate, ss->args.dspFramesPerCycle, ss->args.audioBufCnt, cmApDeviceChannelCount(ss->args.outDevIdx, true), ss->args.devFramesPerCycle, cmApDeviceChannelCount(ss->args.outDevIdx, false), ss->args.devFramesPerCycle )) != kOkRtRC )
        return _cmRtError(p,kAudioBufSetupErrRtRC,"Audio buffer ouput device setup failed.");

    // setup the output audio buffer ptr array - used to recv output audio from the DSP system in _cmRtDspExecCallback()
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
    cp->status.hdr.rtSubIdx  = cp->ctx.rtSubIdx;
    cp->status.iDevIdx   = ss->args.inDevIdx;
    cp->status.oDevIdx   = ss->args.outDevIdx;
    cp->status.iMeterCnt = cp->ctx.iChCnt;
    cp->status.oMeterCnt = cp->ctx.oChCnt;
    cp->iMeterArray      = cmMemAllocZ( double, cp->status.iMeterCnt );
    cp->oMeterArray      = cmMemAllocZ( double, cp->status.oMeterCnt );
    cp->netH             = cfg->netH;

    // create the audio System thread
    if((rc = cmThreadCreate( &cp->threadH, _cmRtThreadCallback, cp, ss->args.rpt )) != kOkThRC )
    {
      rc = _cmRtError(p,kThreadErrRtRC,"Thread create failed.");
      goto errLabel;
    }

    // create the audio System mutex
    if((rc = cmThreadMutexCreate( &cp->engMutexH, ss->args.rpt )) != kOkThRC )
    {
      rc = _cmRtError(p,kMutexErrRtRC,"Thread mutex create failed.");
      goto errLabel;
    }

    // create the host-to-dsp thread safe msg queue 
    if((rc = cmTsMp1cCreate( &cp->htdQueueH, ss->args.msgQueueByteCnt, ss->cbFunc, &cp->ctx, ss->args.rpt )) != kOkThRC )
    {
      rc = _cmRtError(p,kTsQueueErrRtRC,"Host-to-DSP msg queue create failed.");
      goto errLabel;
    }

    // create the dsp-to-host thread safe msg queue 
    if( cmTsMp1cIsValid( p->dthQueH ) == false )
    {
      if((rc = cmTsMp1cCreate( &p->dthQueH, ss->args.msgQueueByteCnt, cfg->clientCbFunc, cfg->clientCbData, ss->args.rpt )) != kOkThRC )
      {
        rc = _cmRtError(p,kTsQueueErrRtRC,"DSP-to-Host msg queue create failed.");
        goto errLabel;
      }
    }
    
    //cp->dthQueueH = p->dthQueH;

    // install an external MIDI port callback handler for incoming MIDI messages
    if( cmMpIsInitialized() )
      if( cmMpInstallCallback( -1, -1, _cmRtSysMidiCallback, cp ) != kOkMpRC )
      {
        rc = _cmRtError(p,kMidiSysFailRtRC,"MIDI system callback installation failed.");
        goto errLabel;
      }

    // setup the sub-system status notification 
    cp->statusUpdateSmpCnt = floor(cmApBufMeterMs() * cp->ss.args.srate / 1000.0 );
    cp->statusUpdateSmpIdx = 0;

    cp->runFl = true;

    // start the audio System thread
    if( cmThreadPause( cp->threadH, 0 ) != kOkThRC )
    {
      rc = _cmRtError(p,kThreadErrRtRC,"Thread start failed.");
      goto errLabel;
    }
  }


  //_cmRtHostInitNotify(p);

  for(i=0; i<p->ssCnt; ++i)
  {
    _cmRtCfg_t* cp = p->ssArray + i;

    // start the input device
    if((rc = cmApDeviceStart( cp->ss.args.inDevIdx )) != kOkRtRC )
      return _cmRtError(p,kAudioDevStartFailRtRC,"The audio input device start failed.");

    // start the output device
    if( cmApDeviceStart( cp->ss.args.outDevIdx ) != kOkRtRC )
      return _cmRtError(p,kAudioDevStartFailRtRC,"The audio ouput device start failed.");
  }

  p->initFl = true;

 errLabel:
  if( rc != kOkRtRC )
    _cmRtSysFinalize(p);

  return rc;
}


cmRtRC_t cmRtSysFinalize(cmRtSysH_t h )
{
  cmRtRC_t rc = kOkRtRC;

  if( cmRtSysHandleIsValid(h) == false )
    return rc;

  cmRt_t* p = _cmRtHandleToPtr(h);

  rc = _cmRtSysFinalize(p);

  h.h = NULL;

  return rc;
}


bool   cmRtSysIsInitialized( cmRtSysH_t h )
{ 
  cmRt_t* p = _cmRtHandleToPtr(h);
  return p->initFl;
}

cmRtRC_t _cmRtSysVerifyInit( cmRt_t* p, bool errFl )
{
  if( p->initFl  == false )
  {

    // if the last msg generated was also a not init msg then don't 
    // generate another message - just return the error
    if( errFl )
      if( cmErrLastRC(&p->err) != kNotInitRtRC )
        cmErrMsg(&p->err,kNotInitRtRC,"The audio system is not initialized.");

    return kNotInitRtRC;
  }
 
  return kOkRtRC;
}


bool   cmRtSysIsEnabled( cmRtSysH_t h )
{ 
  if( cmRtSysIsInitialized(h) == false )
    return false;

  cmRt_t* p = _cmRtHandleToPtr(h);
  unsigned i;
  for(i=0; i<p->ssCnt; ++i)
    if( p->ssArray[i].cbEnableFl )
      return true;

  return false;
}


cmRtRC_t cmRtSysEnable( cmRtSysH_t h, bool enableFl )
{
  cmRt_t*  p = _cmRtHandleToPtr(h);
  return _cmRtSysEnable(p,enableFl);
}

cmRtRC_t  cmRtSysDeliverSegMsg(  cmRtSysH_t h, const void* msgDataPtrArray[], unsigned msgByteCntArray[], unsigned msgSegCnt, unsigned srcNetNodeId )
{
  cmRt_t* p = _cmRtHandleToPtr(h);
  cmRtRC_t rc;

  // the system must be initialized to use this function
  if((rc = _cmRtSysVerifyInit(p,true)) != kOkRtRC )
    return rc;

  if( msgSegCnt == 0 )
    return kOkRtRC;

  // BUG BUG BUG - there is no reason that both the rtSubIdx and the selId must
  // be in the first segment but it would be nice.
  assert( msgByteCntArray[0] >= 2*sizeof(unsigned) || (msgSegCnt>1 && msgByteCntArray[0]==sizeof(unsigned) && msgByteCntArray[1]>=sizeof(unsigned)) );

  // The audio sub-system index is always the first field of the msg 
  // and the msg selector id is always the second field

  unsigned* array = (unsigned*)msgDataPtrArray[0];
  unsigned rtSubIdx = array[0];
  unsigned selId    = array[1];

  if( selId == kUiMstrSelRtId )
    return _cmRtHandleNonSubSysMsg( p, msgDataPtrArray, msgByteCntArray, msgSegCnt );

  if( selId == kNetSyncSelRtId )
  {
    assert( msgSegCnt==1); 
    assert( rtSubIdx < p->ssCnt );
    p->ssArray[rtSubIdx].ctx.srcNetNodeId = srcNetNodeId;
    p->ssArray[rtSubIdx].ss.cbFunc(&p->ssArray[rtSubIdx].ctx,msgByteCntArray[0],msgDataPtrArray[0]);
    return kOkRtRC;
  }

  return _cmRtEnqueueMsg(p,p->ssArray[rtSubIdx].htdQueueH,msgDataPtrArray,msgByteCntArray,msgSegCnt,"Host-to-DSP");  
}

cmRtRC_t cmRtSysDeliverMsg( cmRtSysH_t h, const void* msgPtr, unsigned msgByteCnt, unsigned srcNetNodeId )
{
  const void* msgDataPtrArray[] = {  msgPtr };
  unsigned    msgByteCntArray[] = { msgByteCnt };
  return cmRtSysDeliverSegMsg(h,msgDataPtrArray,msgByteCntArray,1,srcNetNodeId);
}

cmRtRC_t cmRtSysDeliverIdMsg( cmRtSysH_t h, unsigned rtSubIdx, unsigned id, const void* msgPtr, unsigned msgByteCnt, unsigned srcNetNodeId )
{
  cmRtRC_t rc;
  cmRt_t* p = _cmRtHandleToPtr(h);

  // the system must be initialized to use this function
  if((rc = _cmRtSysVerifyInit(p,true)) != kOkRtRC )
    return rc;

  const void* msgDataPtrArray[] = { &rtSubIdx, &id, msgPtr };
  unsigned    msgByteCntArray[] = { sizeof(rtSubIdx), sizeof(id), msgByteCnt };
  return cmRtSysDeliverSegMsg(h,msgDataPtrArray,msgByteCntArray,3,srcNetNodeId);
}

unsigned  cmRtSysIsMsgWaiting( cmRtSysH_t h )
{ 
  cmRtRC_t rc;
  cmRt_t* p = _cmRtHandleToPtr(h);

  // the system must be initialized to use this function
  if((rc = _cmRtSysVerifyInit(p,false)) != kOkRtRC )
    return 0;

  unsigned n = 0;
  unsigned retByteCnt;

  for(n=0; n < p->ssCnt; ++n )
  {
    if( (retByteCnt = cmTsMp1cDequeueMsgByteCount(p->dthQueH)) > 0 )
      return retByteCnt;

    p->waitRtSubIdx = (p->waitRtSubIdx + 1) % p->ssCnt;
  }  

  return 0;
}

cmRtRC_t  cmRtSysReceiveMsg( cmRtSysH_t h, void* msgDataPtr, unsigned msgByteCnt )
{
  cmRtRC_t rc;
  cmRt_t* p = _cmRtHandleToPtr(h);

  // the system must be initialized to use this function
  if((rc = _cmRtSysVerifyInit(p,true)) != kOkRtRC )
    return rc;

  //switch( cmTsMp1cDequeueMsg(p->ssArray[p->waitRtSubIdx].dthQueueH,msgDataPtr,msgByteCnt) )
  switch( cmTsMp1cDequeueMsg(p->dthQueH,msgDataPtr,msgByteCnt) )
  {
    case kOkThRC:          
      p->waitRtSubIdx = (p->waitRtSubIdx + 1) % p->ssCnt;  
      return kOkRtRC;

    case kBufTooSmallThRC: 
      return kBufTooSmallRtRC;

    case kBufEmptyThRC:    
      return kNoMsgWaitingRtRC;
  }

  return _cmRtError(p,kTsQueueErrRtRC,"A deque operation failed on the DSP-to-Host message queue.");
}


void cmRtSysStatus( cmRtSysH_t h, unsigned rtSubIdx, cmRtSysStatus_t* statusPtr )
{
  cmRt_t* p = _cmRtHandleToPtr(h);

  // the system must be initialized to use this function
  if( _cmRtSysVerifyInit(p,true) != kOkRtRC )
    return;

  if( rtSubIdx < p->ssCnt )
    *statusPtr = p->ssArray[rtSubIdx].status;
}

void cmRtSysStatusNotifyEnable( cmRtSysH_t h, unsigned rtSubIdx, bool enableFl )
{
  cmRt_t*  p = _cmRtHandleToPtr(h);

  // the system must be initialized to use this function
  if( _cmRtSysVerifyInit(p,true) != kOkRtRC )
    return;

  unsigned i = rtSubIdx == cmInvalidIdx ? 0 : rtSubIdx;
  unsigned n = rtSubIdx == cmInvalidIdx ? p->ssCnt : rtSubIdx+1;
  for(; i<n; ++i)
    p->ssArray[i].statusFl = enableFl;
}

bool cmRtSysHandleIsValid( cmRtSysH_t h )
{ return h.h != NULL; }

cmRtSysCtx_t* cmRtSysContext( cmRtSysH_t h, unsigned rtSubIdx )
{
  cmRt_t* p = _cmRtHandleToPtr(h);

  if( _cmRtSysVerifyInit(p,true) != kOkRtRC )
    return NULL;

  return &p->ssArray[rtSubIdx].ctx;
}

unsigned cmRtSysSubSystemCount( cmRtSysH_t h )
{
  cmRt_t* p = _cmRtHandleToPtr(h);
  if( _cmRtSysVerifyInit(p,true) != kOkRtRC )
    return 0;

  return p->ssCnt;
}

//===========================================================================================================================
//
//  cmRtTest()
//

/// [cmRtSysTest]

typedef struct 
{
  double   hz;       // current synth frq
  long     phs;      // current synth phase
  double   srate;    // audio sample rate
  unsigned cbCnt;    // DSP cycle count
  bool     synthFl;  // true=synth false=pass through
} _cmRtTestCbRecd;

typedef struct
{
  unsigned rtSubIdx; // rtSubIdx must always be the first field in the msg
  unsigned id;   // 0 = set DSP Hz, 1 = report cbCount to host
  double   hz;   
  unsigned uint;
} _cmRtTestMsg;


long _cmRtSynthSine( _cmRtTestCbRecd* r, cmApSample_t* p, unsigned chCnt, unsigned frmCnt )
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

unsigned _cmRtTestChIdx = 0;

cmRC_t _cmRtTestCb( void* cbPtr, unsigned msgByteCnt, const void* msgDataPtr )
{
  cmRC_t              rc  = cmOkRC;
  cmRtSysCtx_t*    ctx = (cmRtSysCtx_t*)cbPtr;
  cmRtSysSubSys_t* ss  = ctx->ss;
  _cmRtTestCbRecd*    r   = (_cmRtTestCbRecd*)ss->cbDataPtr;

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
            phs = _cmRtSynthSine(r, ctx->oChArray[i], 1, ss->args.dspFramesPerCycle );
      }
      else
      {
        if( _cmRtTestChIdx < ctx->oChCnt )
          phs = _cmRtSynthSine(r, ctx->oChArray[_cmRtTestChIdx], 1, ss->args.dspFramesPerCycle );
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
    _cmRtTestMsg* msg = (_cmRtTestMsg*)msgDataPtr;

    msg->rtSubIdx = ctx->rtSubIdx;

    switch(msg->id)
    {
      case 0:
        r->hz = msg->hz;
        break;

      case 1:
        msg->uint = r->cbCnt;
        msgByteCnt = sizeof(_cmRtTestMsg);
        rc = ctx->dspToHostFunc(ctx,(const void **)&msg,&msgByteCnt,1);
        break;
    }

  }
  
  return rc;
}

// print the usage message for cmAudioPortTest.c
void _cmRtPrintUsage( cmRpt_t* rpt )
{
char msg[] =
  "cmRtSysTest() command switches:\n"
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
int _cmRtGetOpt( int argc, const char* argv[], const char* label, int defaultVal, bool boolFl )
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

bool _cmRtGetBoolOpt( int argc, const char* argv[], const char* label, bool defaultVal )
{ return _cmRtGetOpt(argc,argv,label,defaultVal?1:0,true)!=0; }

int _cmRtGetIntOpt( int argc, const char* argv[], const char* label, int defaultVal )
{ return _cmRtGetOpt(argc,argv,label,defaultVal,false); }


void cmRtSysTest( cmRpt_t* rpt, int argc, const char* argv[] )
{
  cmRtSysCfg_t    cfg;
  cmRtSysSubSys_t ss;
  cmRtSysH_t      h      = cmRtSysNullHandle;
  cmRtSysStatus_t status;
  _cmRtTestCbRecd    cbRecd = {1000.0,0,48000.0,0};

  cfg.ssArray = &ss;
  cfg.ssCnt   = 1;
  //cfg.afpArray= NULL;
  //cfg.afpCnt  = 0;
  cfg.meterMs = 50;

  if(_cmRtGetBoolOpt(argc,argv,"-h",false))
    _cmRtPrintUsage(rpt);

  cbRecd.srate   = _cmRtGetIntOpt(argc,argv,"-r",48000);
  cbRecd.synthFl = _cmRtGetBoolOpt(argc,argv,"-t",false)==false;

  ss.args.rpt               = rpt;
  ss.args.inDevIdx          = _cmRtGetIntOpt( argc,argv,"-i",0); 
  ss.args.outDevIdx         = _cmRtGetIntOpt( argc,argv,"-o",2); 
  ss.args.syncInputFl       = _cmRtGetBoolOpt(argc,argv,"-s",true);
  ss.args.msgQueueByteCnt   = _cmRtGetIntOpt( argc,argv,"-m",8192);
  ss.args.devFramesPerCycle = _cmRtGetIntOpt( argc,argv,"-f",512);
  ss.args.dspFramesPerCycle = _cmRtGetIntOpt( argc,argv,"-d",64);;
  ss.args.audioBufCnt       = _cmRtGetIntOpt( argc,argv,"-b",3);       
  ss.args.srate             = cbRecd.srate;
  ss.cbFunc                 = _cmRtTestCb;                            // set the DSP entry function
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
  if( cmRtSysAllocate(&h,rpt,&cfg) != kOkRtRC )
    goto errLabel;
  
  // start the audio system
  cmRtSysEnable(h,true);

  char c = 0;
  printf("q=quit a-g=note n=ch r=rqst s=status\n");

  // simulate a host event loop
  while(c != 'q')
  {
    _cmRtTestMsg msg = {0,0,0,0};
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

      case 'n': ++_cmRtTestChIdx; printf("ch:%i\n",_cmRtTestChIdx); break;

      case 's':  
        // report the audio system status
        cmRtSysStatus(h,0,&status);
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
      cmRtSysDeliverMsg(h,&msg, sizeof(msg), cmInvalidId);
    }


    // check if messages are waiting to be delivered from the DSP process
    unsigned msgByteCnt;
    if((msgByteCnt = cmRtSysIsMsgWaiting(h)) > 0 )
    {
      char buf[ msgByteCnt ];

      // rcv a msg from the DSP process
      if( cmRtSysReceiveMsg(h,buf,msgByteCnt) == kOkRtRC )
      {
        _cmRtTestMsg* msg = (_cmRtTestMsg*)buf;
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
  cmRtSysEnable(h,false);

 
  goto exitLabel;

 errLabel:
  printf("AUDIO SYSTEM TEST ERROR\n");

 exitLabel:

  cmRtSysFree(&h);
  cmApFinalize();
  cmApFileFree();
  cmApNrtFree();
  cmApBufFinalize();

}

/// [cmRtSysTest]
