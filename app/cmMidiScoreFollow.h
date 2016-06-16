#ifndef cmMidiScoreFollow_h
#define cmMidiScoreFollow_h


#ifdef __cplusplus
extern "C" {
#endif

  enum
  {
    kOkMsfRC = cmOkRC,
    kFailMsfRC
  };

  typedef cmRC_t cmMsfRC_t;
  

  cmMsfRC_t cmMidiScoreFollowMain( cmCtx_t* ctx );
  
#ifdef __cplusplus
}
#endif


#endif
