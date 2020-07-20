#ifndef cmOnset_h
#define cmOnset_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"Musical event onset detector." kw:[audio] }
  
  enum
  {
    kOkOnRC = cmOkRC,
    kDspProcFailOnRC,
    kDspAudioFileFailOnRC,
    kDspTextFileFailOnRC,
  };

  typedef cmRC_t     cmOnRC_t;
  typedef cmHandle_t cmOnH_t;

  enum { kNoneFiltId, kSmoothFiltId, kMedianFiltId };

  typedef struct
  {
    double   wndMs;
    unsigned hopFact;
    unsigned audioChIdx;

    unsigned wndFrmCnt;   // Detection window length
    double   preWndMult;  // Detection window stretch factor prior to current location.
    double   threshold;   // Spectal flux detection threshold
    double   maxFrqHz;    // Ignore frequencies above maxFrqHz during processing.
    double   filtCoeff;   // smoothing filter coeff (-.7)
    double   medFiltWndMs;// median filter window in milliseconds
    unsigned filterId;    // kSmoothFiltId || kMedianFiltId
    double   preDelayMs;  // move each detection preDelayMs backwards in time 
                          // on the audio output. (compensates for detection delay due to filtering)

    
  } cmOnsetCfg_t;

  extern cmOnH_t cmOnsetNullHandle;

  cmOnRC_t cmOnsetInitialize( cmCtx_t* c, cmOnH_t* hp );

  cmOnRC_t cmOnsetFinalize( cmOnH_t* hp );

  bool     cmOnsetIsValid( cmOnH_t h );

  cmOnRC_t cmOnsetProc( 
    cmOnH_t h, 
    const cmOnsetCfg_t* cfg, 
    const cmChar_t* inAudioFn );

  // Return count of detected onsets.
  unsigned cmOnsetCount( cmOnH_t h );

  // Return location of detected onsets as a sample offset into the file.
  unsigned cmOnsetSampleIndex( cmOnH_t h, unsigned idx );

  unsigned cmOnsetHopSampleCount( cmOnH_t h );

  cmOnRC_t cmOnsetWrite(
    cmOnH_t h,
    const cmChar_t* outAudioFn, 
    const cmChar_t* outTextFn);

  cmOnRC_t cmOnsetTest( cmCtx_t* c );

  //)
  
#ifdef __cplusplus
}
#endif


#endif
