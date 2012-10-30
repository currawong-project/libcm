#ifndef cmAudDsp_h
#define cmAudDsp_h

#ifdef __cplusplus
extern "C" {
#endif

  // This API supports a serialized interface to an internal instance of 
  // cmAudioSys and cmDspSys.  

  enum
  {
    kOkAdRC = cmOkRC,
    kAudioPortFailAdRC,
    kAudioSysFailAdRC,
    kMidiSysFailAdRC,
    kDspSysFailAdRC,
    kFileSysFailAdRC,
    kJsonFailAdRC,
    kSendMsgFailAdRC,
    kInvalidCfgIdxAdRC,
    kNoPgmLoadedAdRC,
    kInvalidSubSysIdxAdRC,
    kUnknownMsgTypeAdRC,
    kAggDevSysFailAdRC,
    kAggDevCreateFailAdRC,
    kNrtDevSysFailAdRC,
    kNetSysFailAdRC
  };


  typedef cmRC_t     cmAdRC_t;
  typedef cmHandle_t cmAdH_t;
  
  extern cmAdH_t cmAdNullHandle;

  // Create a audio dsp engine and send device and program information to the 
  // host application.  
  // cbPtr provides a function used by cmAudDsp to send messages to the client.
  cmAdRC_t cmAudDspAlloc( cmCtx_t* ctx, cmAdH_t* hp, cmMsgSendFuncPtr_t cbPtr, void* cbDataPtr );
  cmAdRC_t cmAudDspFree(  cmAdH_t* hp );

  bool cmAudDspIsValid( cmAdH_t h );

  // This function provides the primary interface for communication from the
  // client program to the aud_dsp system.
  cmAdRC_t cmAudDspReceiveClientMsg( cmAdH_t h, unsigned msgBytecnt, const void* msg );
  

#ifdef __cplusplus
  }
#endif


#endif
