#ifndef cmAudDspLocal_h
#define cmAudDspLocal_h

#ifdef __cplusplus
extern "C" {
#endif

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

  bool      cmAudDspLocalIsValid( cmAdlH_t h );

  cmAiH_t   cmAudDspLocalIF_Handle( cmAdlH_t h );
  

#ifdef __cplusplus
}
#endif

#endif
