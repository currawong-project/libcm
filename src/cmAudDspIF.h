//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#ifndef cmAudDspIF_h
#define cmAudDspIF_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc: "Virtual interface to the audio DSP system." kw:[rt]}
  //
  // This class provides a two-way interface to the audio DSP system.
  // It is designed to work independenty of the physical
  // method of communication.  For example, when used by
  // cmAudDspLocal, it  supports in memory transfer of messages
  // between the application and the audio-DSP engine.
  // Another implementation however could use it to support
  // a networked communication scheme to a remote audio-DSP
  // system.  Note that in either case, however, this class
  // resides with, and is linked to, the application, and not
  // the engine.  
  
  // This API has two basic responsibilities:
  //
  // 1) Provides a function based interface to the audio DSP system for the
  // client application.  This is more convenient, and safer, than the lower level
  // message based interface provided by cmAudDsp.h.
  //    The client calls these API functions to send commands to the audio DSP
  //    system. Internally the cmAdIfxxx functions converts the commands to 
  //    raw message packets and passes them to a transmission service
  //    via cmAdIfParm_t audioDspFunc().
  //
  // 2) Acts as the receiver of raw message streams from whatever external
  // service (e.g. cmAudDspLocal, cmAudDspUdp) is receiving raw message packets 
  // from audio DSP system.  
  // 
  //    This process is driven by periodic calls from the client to 
  //    cmAdIfDispatchMsgToHost().
  //    cmAdIfDispatchMsgToHost() then generates an internal 
  //    'kClientMsgPollDuiId' msg which is passed toward the 
  //    cmAudDsp system.  
  //    When the msg encounters a sub-system with queued msgs waiting
  //    for the client a callback chain ensues which eventually
  //    calls cmAdIfRecvAudDspMsg() which in turn calls the appropriate
  //    client provided cmAdIfDispatch_t function (ssInitFunc,statusFunc or uiFunc).
  //    Note that this entire chain of calls occurs in the client thread
  //    and in the context of the cmAdIfDispatchMsgToHost() procedure.
  //)

  //(
  enum
  {
    kOkAiRC = cmOkRC,
    kAudDspFailAiRC,
    kLHeapFailAiRC,
    kUnknownMsgTypeAiRC,
    kMsgCorruptAiRC,
    kSendFailAiRC,
    kQueueFailAiRC,
    kNoMsgAiRC,
    kJsonFailAiRC,
    kDeserialFailAiRC,
    kFileSysFailAiRC
  };

  typedef cmRC_t cmAiRC_t; 

  typedef cmHandle_t cmAiH_t;

  // These functions are provided by the client to receive messages 
  // from the audio DSP sytem. These functions are only called from the client thread
  // from within cmAdIfDispatchMsgToHost().
  typedef struct
  {
    void* cbDataPtr; // data to send as the first arg. to app. callbacks

    cmRC_t (*ssInitFunc)( void* cbDataPtr, const cmAudioSysSsInitMsg_t* r, const char* iDevLabel, const char* oDevLabel );
    cmRC_t (*statusFunc)( void* cbDataPtr, const cmAudioSysStatus_t* r, const double* iMeterArray, const double* oMeterArray );
    cmRC_t (*uiFunc)(     void* cbDataPtr, const cmDspUiHdr_t* r );
  } cmAdIfDispatch_t;

  typedef struct
  {
    cmAdIfDispatch_t   dispatchRecd;       // client application callback pointers
    cmMsgSendFuncPtr_t audDspFunc;         // the cmAdIfXXX functions use the callback to send msgs to the audio DSP system.
    void*              audDspFuncDataPtr;  // data to send with the audio DSP callback function
  } cmAdIfParm_t;


  extern cmAiH_t cmAiNullHandle;

  cmAiRC_t        cmAdIfAllocate( cmCtx_t* ctx, cmAiH_t* hp, const cmAdIfParm_t* parms  );
  cmAiRC_t        cmAdIfFree( cmAiH_t* hp );

  bool            cmAdIfIsValid( cmAiH_t h );

  // Receive a msg from the audio DSP system. This is the main point of entry
  // for all calls from the audio DSP system to the client.
  // This function is provided as a callback to the owner of this cmAudDspIF
  // (e.g. cmAudDspLocal, cmAudDspUdpClient) it should never need to be called
  // by the client.
  cmAiRC_t       cmAdIfRecvAudDspMsg( cmAiH_t h, unsigned msgByteCnt, const void* msgDataPtr);


  //-------------------------------------------------------------------------
  //
  // The functions below are used to send commands to the audio DSP system
  // from the client application. 
  //

  // Print a hardware report.
  cmAiRC_t        cmAdIfDeviceReport( cmAiH_t h );

  // Select a audio system configuration.  This must be done prior to 
  // sending any other commands.
  cmAiRC_t        cmAdIfSetAudioSysCfg( cmAiH_t h, unsigned asCfgIdx );

  // Select an audio input or output device for a given audio sub-system.
  // An audio configuration must have been selected via cmAdIfSetAudioSysCfg()
  // prior to calling this function.
  cmAiRC_t        cmAdIfSetAudioDevice( cmAiH_t h, unsigned asSubIdx, bool inputFl, unsigned devIdx );

  // Set the sample rate for a given audio sub-system or the entire audio system.
  // Set asSubIdx to cmInvalidIdx to assign the sample rate to all sub-systems 
  // of the current audio system configuration.
  // An audio configuration must have been selected via cmAdIfSetAudioSysCfg()
  // prior to calling this function.
  cmAiRC_t        cmAdIfSetSampleRate(  cmAiH_t h, unsigned asSubIdx, double srate );

  // Select a DSP program for a given audio sub-system or the entire audio system.
  // Set asSubIdx to cmInvalidIdx to load the program on all sub-systems 
  // of the current audio system configuration.
  // An audio configuration must have been selected via cmAdIfSetAudioSysCfg()
  // prior to calling this function.
  cmAiRC_t        cmAdIfLoadProgram(    cmAiH_t h, unsigned asSubIdx, unsigned pgmIdx );

  // Print a representation (instances and connections) of the loaded program 
  // to a JSON file.
  cmAiRC_t        cmAdIfPrintPgm(   cmAiH_t h, unsigned asSubIdx, const cmChar_t* fn );

  // Start the audio streaming. 
  // An audio configuration must have been selected via cmAdIfSetAudioSysCfg()
  // and a DSP program must have been selected via cmAdIfLoadProgram() 
  // prior to calling this function.
  cmAiRC_t        cmAdIfEnableAudio( cmAiH_t h, bool enableFl );

  // Enable/disable periodic audio system status notifications.
  cmAiRC_t        cmAdIfEnableStatusNotify( cmAiH_t h, bool enableFl );
  
  // Send a kUiSelAsId style message to the audio DSP system.
  cmAiRC_t        cmAdIfSendMsgToAudioDSP( 
    cmAiH_t             h, 
    unsigned            asSubIdx,
    unsigned            msgTypeId,
    unsigned            selId,
    unsigned            flags,
    unsigned            instId,
    unsigned            instVarId,
    const cmDspValue_t* valPtr );

  // The client application must periodically call this function to 
  // receive pending messages from the audio DSP system. The 
  // messages are delivered via callbacks provided by cmAdIfDispatch_t. 
  // This function should only be called from the client thread.
  cmAiRC_t        cmAdIfDispatchMsgToHost(  cmAiH_t h ); 

  /*
    Local call chain:
    cmAdIfDispatchMsgToHost() -> p->parms.audDspFunc = cmAudDspLocal::_cmAdlAudDspSendFunc() 
                              -> cmAudioDsp::cmAudDspReceiveClientMsg()
                              -> cmAudioDsp::_cmAudDspClientMsgPoll()
                              -> cmAudioSys::cmAudioSysReceiveMsg()
                              -> cmThread::cmTs1p1cDequeueMsg()
                              -> cmAudioSysCfg_t::clientCbFunc = cmAudDsp::_cmAudioSysToClientCallback()
                              -> cmAudDsp::cmAd_t::cbFunc = cmAudDspLocal::_cmAudDspLocalCallback()
                              -> cmAudDspIF::cmAdIfRecvAudDspMsg()
                              -> cmAudDspIF::_cmAiDispatchMsgToClient()
                              -> cmAudDspIF::cmAdIfDispatch_t.uiFunc = kcApp::_s_handleUiMsg()
                              
   */

  //)
  
#ifdef __cplusplus
}
#endif

#endif
