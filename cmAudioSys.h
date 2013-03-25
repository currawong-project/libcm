// cmAudioSys.h
// Implements a real-time audio processing engine.
//
// The audio system is composed a collection of independent sub-systems.
// Each sub-system maintains a thread which runs asynchrounsly
// from the application, the MIDI devices, and the audio devices.
// To faciliate communication between these components each sub-system maintains 
// two thread-safe data buffers one for control information and a second 
// for audio data.
//
// The audio devices are the primary driver for the system. 
// Callbacks from the audio devices (See #cmApCallbackPtr_t) 
// inserts incoming audio samples into the audio
// record buffers and extracts samples from the playback buffer.  
// When sufficient incoming samples and outgoing empty buffer space exists
// a sub-system thread is waken up by the callback. This triggers a DSP audio 
// processing cycle which empties/fills the audio buffers. During a DSP
// processing cycle control messages from the application and MIDI are blocked and
// buffered. Upon completetion of the DSP cycle a control message
// transfer cycles occurs - buffered incoming messages are passed to 
// the DSP system and messages originating in the DSP system are
// buffered by the audio system for later pickup by the application
// or MIDI system.
// 
// Note that control messages that arrive when the DSP cycle is not
// occurring can pass directly through to the DSP system.
//
// The DSP system sends messages back to the host by calling
// cmAsDspToHostFunc_t provided by cmAudioSysCtx_t. These
// calls are always made from within an audio system call to 
// audio or control update within cmAsCallback_t. cmAsDspToHostFunc_t
// simply stores the message in a message buffer.  The host picks
// up the message at some later time when it notices that messages
// are waiting via polling cmAudioSysIsMsgWaiting().
//
// Implementation: \n
// The audio sub-systems work by maintaining an internal thread
// which blocks on a mutex condition variable.
// While the thread is blocked the mutex is unlocked allowing messages
// to pass directly through to the DSP procedure via cmAsCallback().
//
// Periodic calls from running audio devices update the audio buffer. 
// When the audio buffer has input samples waiting and output space
// available the condition variable is signaled, the mutex is 
// then automatically locked by the system, and the DSP execution
// procedure is called via cmAsCallback().
//
// Messages arriving while the mutex is locked are queued and
// delivered to the DSP procedure at the end of the DSP execution
// procedure.
//
// Usage example and testing code:
// See  cmAudioSysTest().
// \snippet cmAudioSys.c cmAudioSysTest

#ifndef cmAudioSys_h
#define cmAudioSys_h

#ifdef __cplusplus
extern "C" {
#endif

  // Audio system result codes
  enum
  {
    kOkAsRC = cmOkRC,
    kThreadErrAsRC,
    kMutexErrAsRC,
    kTsQueueErrAsRC,
    kMsgEnqueueFailAsRC,
    kAudioDevSetupErrAsRC,
    kAudioBufSetupErrAsRC,
    kAudioDevStartFailAsRC,
    kAudioDevStopFailAsRC,
    kBufTooSmallAsRC,
    kNoMsgWaitingAsRC,
    kMidiSysFailAsRC,
    kMsgSerializeFailAsRC,
    kStateBufFailAsRC,
    kInvalidArgAsRC,
    kNotInitAsRC
  };

  typedef cmHandle_t cmAudioSysH_t;  //< Audio system handle type
  typedef unsigned   cmAsRC_t;       //< Audio system result code

  struct cmAudioSysCtx_str;

  //
  // DSP system callback function.
  //
  // This is the sole point of entry into the DSP system while the audio system is running.
  //
  // ctxPtr is pointer to a cmAudioSysCtx_t record.
  //
  // This function is called under two circumstances:
  //
  // 1) To notify the DSP system that the audio input/output buffers need to be serviced.
  // This is a perioidic request which the DSP system uses as its execution trigger.
  // cmAudioSysCtx_t.audioRateFl is set to true to indicate this type of callback.
  //
  // 2) To pass messages from the host application to the DSP system.
  // The DSP system is asyncronous with the host because it executes in the 
  // audio system thread rather than the host thread.  The cmAudioSysDeliverMsg() 
  // function synchronizes incoming messages with the internal audio system 
  // thread to prevent thread collisions.
  //
  // Notes:
  // This callback is always made with the internal audio system mutex locked.
  //
  // The signal time covered by the callback is from 
  // ctx->begSmpIdx to ctx->begSmpIdx+cfg->dspFramesPerCycle.
  //
  // The return value is currently not used.
  typedef cmRC_t (*cmAsCallback_t)(void* ctxPtr, unsigned msgByteCnt, const void* msgDataPtr );

  
  // Audio device sub-sytem configuration record 
  typedef struct cmAudioSysArgs_str
  {
    cmRpt_t*       rpt;               // system console object
    unsigned       inDevIdx;          // input audio device
    unsigned       outDevIdx;         // output audio device
    bool           syncInputFl;       // true/false sync the DSP update callbacks with audio input/output
    unsigned       msgQueueByteCnt;   // Size of the internal msg queue used to buffer msgs arriving via cmAudioSysDeliverMsg().
    unsigned       devFramesPerCycle; // (512) Audio device samples per channel per device update buffer.
    unsigned       dspFramesPerCycle; // (64)  Audio samples per channel per DSP cycle.
    unsigned       audioBufCnt;       // (3)   Audio device buffers.
    double         srate;             // Audio sample rate.
  } cmAudioSysArgs_t;

  // Audio sub-system configuration record.
  // This record is provided by the host to configure the audio system
  // via cmAudioSystemAllocate() or cmAudioSystemInitialize().
  typedef struct cmAudioSysSubSys_str
  {
    cmAudioSysArgs_t args;              // Audio device configuration
    cmAsCallback_t   cbFunc;            // DSP system entry point function.
    void*            cbDataPtr;         // Host provided data for the DSP system callback.   
  } cmAudioSysSubSys_t;


  // Signature of a callback function provided by the audio system to receive messages 
  // from the DSP system for later dispatch to the host application.
  // This declaration is used by the DSP system implementation and the audio system.
  // Note that this function is intended to convey one message broken into multiple parts.
  // See cmTsQueueEnqueueSegMsg() for the equivalent interface.
  typedef cmAsRC_t (*cmAsDspToHostFunc_t)(struct cmAudioSysCtx_str* p, const void* msgDataPtrArray[], unsigned msgByteCntArray[], unsigned msgSegCnt);

  // Record passed with each call to the DSP callback function cmAsCallback_t
  typedef struct cmAudioSysCtx_str
  {
    void*               reserved;      // used internally by the audio system

    bool                audioRateFl;   // true if this is an audio update callback

    unsigned            srcNetNodeId;  // Source net node if this is a msg callback originating from a remote network node. 
    unsigned            asSubIdx;      // index of the sub-system this DSP process is serving

    cmAudioSysSubSys_t* ss;            // ptr to a copy of the cfg recd used to initialize the audio system
    unsigned            begSmpIdx;     // gives signal time as a sample count

    cmAsDspToHostFunc_t dspToHostFunc; // Callback used by the DSP process to send messages to the host
                                       // via the audio system. Returns a cmAsRC_t result code.

                                       // output (playback) buffers
    cmSample_t**        oChArray;      // each ele is a ptr to buffer with cfg.dspFramesPerCycle samples
    unsigned            oChCnt;        // count of output channels (ele's in oChArray[])

                                       // input (recording) buffers
    cmSample_t**        iChArray;      // each ele is a ptr to buffer with cfg.dspFramesPerCycle samples
    unsigned            iChCnt;        // count of input channels (ele's in iChArray[])
    
  } cmAudioSysCtx_t;


  // Audio system configuration record used by cmAudioSysAllocate().
  typedef struct cmAudioSysCfg_str
  {
    cmAudioSysSubSys_t*   ssArray;      // sub-system cfg record array
    unsigned              ssCnt;        // count of sub-systems   
    unsigned              meterMs;      // Meter sample period in milliseconds
    void*                 clientCbData; // User arg. for clientCbFunc().
    cmTsQueueCb_t         clientCbFunc; // Called by  cmAudioSysReceiveMsg() to deliver internally generated msg's to the host. 
                                        //  Set to NULL if msg's will be directly returned by buffers passed to cmAudioSysReceiveMsg().
    cmUdpNetH_t           netH;
  } cmAudioSysCfg_t;

  extern cmAudioSysH_t cmAudioSysNullHandle;

  // Allocate and initialize an audio system as a collection of 'cfgCnt' sub-systems.
  // Prior to call this function the audio audio ports system must be initalized 
  // (via cmApInitialize()) and the MIDI port system must be initialized 
  // (via cmMpInitialize()).  Note also that cmApFinalize() and cmMpFinalize() 
  // cannot be called prior to cmAudioSysFree().
  // See cmAudioSystemTest() for a complete example.
  cmAsRC_t  cmAudioSysAllocate( cmAudioSysH_t* hp, cmRpt_t* rpt, const cmAudioSysCfg_t* cfg  );

  // Finalize and release any resources held by the audio system.
  cmAsRC_t  cmAudioSysFree( cmAudioSysH_t* hp );

  // Returns true if 'h' is a handle which was successfully allocated by 
  // cmAudioSysAllocate().
  bool      cmAudioSysHandleIsValid( cmAudioSysH_t h );

  // Reinitialize a previously allocated audio system.  This function
  // begins with a call to cmAudioSysFinalize().   
  // Use cmAudioSysEnable(h,true) to begin processing audio following this call.
  cmAsRC_t  cmAudioSysInitialize( cmAudioSysH_t h, const cmAudioSysCfg_t* cfg );

  // Complements cmAudioSysInitialize(). In general there is no need to call this function
  // since calls to cmAudioSysInitialize() and cmAudioSysFree() automaticatically call it.
  cmAsRC_t  cmAudioSysFinalize( cmAudioSysH_t h );

  // Returns true if the audio system has been successfully initialized.
  bool     cmAudioSysIsInitialized( cmAudioSysH_t );

  // Returns true if the audio system is enabled.
  bool      cmAudioSysIsEnabled( cmAudioSysH_t h );

  // Enable/disable the audio system.  Enabling the starts audio stream
  // in/out of the system.
  cmAsRC_t  cmAudioSysEnable( cmAudioSysH_t h, bool enableFl );

  //
  // Host to DSP delivery functions
  // 

  // Deliver a message from the host application to the DSP process. (host -> DSP);
  // The message is formed as a concatenation of the bytes in each of the segments
  // pointed to by 'msgDataPtrArrary[segCnt][msgByteCntArray[segCnt]'.
  // This is the canonical msg delivery function in so far as the other host->DSP
  // msg delivery function are written in terms of this function.
  // The first 4 bytes in the first segment must contain the index of the audio sub-system
  // which is to receive the message.
  cmAsRC_t  cmAudioSysDeliverSegMsg(  cmAudioSysH_t h, const void* msgDataPtrArray[], unsigned msgByteCntArray[], unsigned msgSegCnt, unsigned srcNetNodeId );

  // Deliver a single message from the host to the DSP system.
  cmAsRC_t  cmAudioSysDeliverMsg(   cmAudioSysH_t h,  const void* msgPtr, unsigned msgByteCnt, unsigned srcNetNodeId );

  // Deliver a single message from the host to the DSP system.
  // Prior to delivery the 'id' is prepended to the message.
  cmAsRC_t  cmAudioSysDeliverIdMsg(  cmAudioSysH_t h, unsigned asSubIdx, unsigned id, const void* msgPtr, unsigned msgByteCnt, unsigned srcNetNodeId );


  //
  // DSP to Host message functions
  // 

  // Is a msg from the DSP waiting to be picked up by the host?  (host <- DSP)
  // 0  = no msgs are waiting or the msg queue is locked by the DSP process.
  // >0 = the size of the buffer required to hold the next msg returned via 
  // cmAudioSysReceiveMsg().
  unsigned  cmAudioSysIsMsgWaiting(  cmAudioSysH_t h );

  // Copy the next available msg sent from the DSP process to the host into the host supplied msg buffer
  // pointed to by 'msgBufPtr'.  Set 'msgDataPtr' to NULL to receive msg by callback from cmAudioSysCfg_t.clientCbFunc.
  // Returns kBufTooSmallAsRC if msgDataPtr[msgByteCnt] is too small to hold the msg.
  // Returns kNoMsgWaitingAsRC if no messages are waiting for delivery or the msg queue is locked by the DSP process.
  // Returns kOkAsRC if a msg was delivered.
  // Call cmAudioSysIsMsgWaiting() prior to calling this function to get
  // the size of the data buffer required to hold the next message.
  cmAsRC_t  cmAudioSysReceiveMsg(    cmAudioSysH_t h,  void* msgDataPtr, unsigned msgByteCnt );


  // Fill an audio system status record.
  void      cmAudioSysStatus(  cmAudioSysH_t h, unsigned asSubIdx, cmAudioSysStatus_t* statusPtr );

  // Enable cmAudioSysStatus_t notifications to be sent periodically to the host.
  // Set asSubIdx to cmInvalidIdx to enable/disable all sub-systems.
  // The notifications occur approximately every cmAudioSysCfg_t.meterMs milliseconds.
  void cmAudioSysStatusNotifyEnable( cmAudioSysH_t, unsigned asSubIdx, bool enableFl );

  // Return a pointer the context record associated with a sub-system
  cmAudioSysCtx_t* cmAudioSysContext( cmAudioSysH_t h, unsigned asSubIdx );

  // Return the count of audio sub-systems.
  // This is the same as the count of cfg recds passed to cmAudioSystemInitialize().
  unsigned cmAudioSysSubSystemCount( cmAudioSysH_t h );

  // Audio system test and example function.
  void      cmAudioSysTest( cmRpt_t* rpt, int argc, const char* argv[] );



#ifdef __cplusplus
}
#endif

#endif
