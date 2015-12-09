#ifndef cmAudDspLocal_h
#define cmAudDspLocal_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc: "Implementation of the audio DSP interface for local, in-memory, communication." kw:[rt]}
  //
  //  This class instantiates an audio-DSP engine (cmAudDsp),
  //  an interface for communicating with it (cmAudDspIF),
  //  and message delivery functions for copying messages
  //  in both directions between cmAuDsp and cmAudDspIF.
  //
  //  Note that the underlying inteface which allows an application to
  //  control, and receive message from, cmAudDsp is provided by
  //  cmAudDspIF - which this class provides a handle to.
  //)  

  //(
  
  enum
  {
    kOkAdlRC = cmOkRC,
    kAudDspIfFailAdlRC,
    kAudDspFailAdlRC,
    kFileSysFailAdlRC,
    kJsonFailAdlRC
  };

  typedef cmRC_t cmAdlRC_t;
  typedef cmHandle_t cmAdlH_t;

  extern cmAdlH_t cmAdlNullHandle;

  cmAdlRC_t cmAudDspLocalAllocate( 
    cmCtx_t*                ctx, 
    cmAdlH_t*               hp, 
    const cmAdIfDispatch_t* recd );

  cmAdlRC_t cmAudDspLocalFree( cmAdlH_t* hp );

  cmAdlRC_t cmAudDspLocalSendSetup( cmAdlH_t h );

  bool      cmAudDspLocalIsValid( cmAdlH_t h );

  cmAiH_t   cmAudDspLocalIF_Handle( cmAdlH_t h );
  
  //)

#ifdef __cplusplus
}
#endif

#endif
