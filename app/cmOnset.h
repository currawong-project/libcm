#ifndef cmOnset_h
#define cmOnset_h

#ifdef __cplusplus
extern "C" {
#endif

  enum
  {
    kOkOnRC = cmOkRC,
    kDspProcFailOnRC,
    kDspAudioFileFailOnRC,
    kDspTextFileFailOnRC,
  };

  typedef cmRC_t     cmOnRC_t;
  typedef cmHandle_t cmOnH_t;

  typedef struct
  {
    double   wndMs;
    unsigned hopFact;
    unsigned audioChIdx;

    unsigned wndFrmCnt;   // 
    double   preWndMult;  //
    double   threshold;   //
    double   maxFrqHz;    //
    double   filtCoeff;   //
    
  } cmOnsetCfg_t;

  extern cmOnH_t cmOnsetNullHandle;

  cmOnRC_t cmOnsetInitialize( cmCtx_t* c, cmOnH_t* hp );

  cmOnRC_t cmOnsetFinalize( cmOnH_t* hp );

  bool     cmOnsetIsValid( cmOnH_t h );

  cmOnRC_t cmOnsetExec( 
    cmOnH_t h, 
    const cmOnsetCfg_t* cfg, 
    const cmChar_t* inAudioFn, 
    const cmChar_t* outAudioFn, 
    const cmChar_t* outTextFn );

  cmOnRC_t cmOnsetTest( cmCtx_t* c );

#ifdef __cplusplus
}
#endif


#endif
