#ifndef cmProc5_h
#define cmProc5_h

#ifdef __cplusplus
extern "C" {
#endif


  //=======================================================================================================================
  // Goertzel Filter
  //

  typedef struct
  {
    double s0;
    double s1;
    double s2;
    double coeff;
    double hz;
  } cmGoertzelCh;

  struct cmShiftBuf_str;

  typedef struct cmGoertzel_str
  {
    cmObj                  obj;
    cmGoertzelCh*          ch;
    unsigned               chCnt;
    double                 srate;
    struct cmShiftBuf_str* shb;
    cmSample_t*            wnd;
  } cmGoertzel;

  cmGoertzel* cmGoertzelAlloc( cmCtx* c, cmGoertzel* p, double srate, const double* fcHzV, unsigned chCnt, unsigned procSmpCnt, unsigned hopSmpCnt, unsigned wndSmpCnt );
  cmRC_t cmGoertzelFree( cmGoertzel** pp );
  cmRC_t cmGoertzelInit( cmGoertzel* p, double srate, const double* fcHzV, unsigned chCnt, unsigned procSmpCnt, unsigned hopSmpCnt, unsigned wndSmpCnt );
  cmRC_t cmGoertzelFinal( cmGoertzel* p );
  cmRC_t cmGoertzelSetFcHz( cmGoertzel* p, unsigned chIdx, double hz );
  cmRC_t cmGoertzelExec( cmGoertzel* p, const cmSample_t* in, unsigned procSmpCnt,  double* outV, unsigned chCnt );


  //=======================================================================================================================
  // Gold Code Signal Generator
  //

  typedef struct
  {
    unsigned chN;              // count of channels (each channel has a unique id)   
    double   srate;            // system sample rate (samples/second)
    unsigned lfsrN;            // linear feedback shift register (LFSR) length used to form Gold codes
    unsigned mlsCoeff0;        // LFSR coeff. 0
    unsigned mlsCoeff1;        // LFSR coeff. 1
    unsigned samplesPerChip;   // samples per spreading code bit
    double   rcosBeta;         // raised cosine impulse response beta coeff.
    unsigned rcosOSFact;       // raised cosine impulse response oversample factor
    double   carrierHz;        // carrier frequency
    double   envMs;            // attack/decay envelope duration
  } cmGoldSigArg_t;

  typedef struct
  {
    int*        pnV;  // pnV[ mlsN ]  spread code (aliased from pnM[:,i])
    cmSample_t* bbV;  // bbV[ sigN ]  baseband signal  at audio rate
    cmSample_t* mdV;  // mdV[ sigN ]  modulated signal at audio rate
  } cmGoldSigCh_t;

  typedef struct 
  {
    cmObj          obj;         // 
    cmGoldSigArg_t a;           // argument record
    cmGoldSigCh_t* ch;          // ch[ chN ] channel array
    int*           pnM;         // pnM[mlsN,chN] (aliased to ch[].pnV)
    cmSample_t*    rcosV;       // rcosV[rcosN] raised cosine impulse response
    unsigned       rcosN;       // length of raised cosine impulse response
    unsigned       mlsN;        // length of Gold codes (Maximum length sequence length)
    unsigned       sigN;        // length of channel signals bbV[] and mdV[]  
  } cmGoldSig_t;


  cmGoldSig_t* cmGoldSigAlloc( cmCtx* ctx, cmGoldSig_t* p, const cmGoldSigArg_t* a );
  cmRC_t cmGoldSigFree( cmGoldSig_t** pp );

  cmRC_t cmGoldSigInit( cmGoldSig_t* p, const cmGoldSigArg_t* a );
  cmRC_t cmGoldSigFinal( cmGoldSig_t* p );
  
  cmRC_t cmGoldSigWrite( cmCtx* ctx, cmGoldSig_t* p, const char* fn );

  // Generate a signal consisting of  underlying white noise with 
  // bsiN repeated copies of the id signal associated with 
  // channel 'chIdx'. Each discrete id signal copy is separated by 'dsN' samples.
  // The signal will be prefixed with 'prefixN' samples of silence (noise).
  // On return sets 'yVRef' to point to the generated signal and 'yNRef'
  // to the count of samples in 'yVRef'.
  // On error sets yVRef to NULL and  yNRef to zero.
  // The vector returned in 'yVRef' should be freed via atMemFree().
  // On return sets bsiV[bsiN] to the onset sample index of each id signal copy.
  // The background noise signal is limited to the range -noiseGain to noiseGain.
  cmRC_t cmGoldSigGen( 
    cmGoldSig_t*   p, 
    unsigned      chIdx, 
    unsigned      prefixN, 
    unsigned      dsN, 
    unsigned     *bsiV, 
    unsigned      bsiN, 
    double        noiseGain, 
    cmSample_t**  yVRef, 
    unsigned*     yNRef );

  cmRC_t cmGoldSigTest( cmCtx* ctx );

  
#ifdef __cplusplus
}
#endif

#endif
