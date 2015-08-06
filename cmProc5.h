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


  //=======================================================================================================================
  // Phase aligned transform generalized cross correlator
  //

  // Flags for use with the 'flags' argument to cmPhatAlloc() 
  enum
  {
    kNoFlagsAtPhatFl= 0x00,
    kDebugAtPhatFl  = 0x01,  // generate debugging file
    kHannAtPhatFl   = 0x02   // apply a hann window function to the id/audio signals prior to correlation. 
  };

  typedef struct
  {
    cmObj            obj;
    cmFftSR          fft;
    cmIFftRS         ifft;
    
    float            alpha;
    unsigned         flags;

    cmComplexR_t*    fhM;      // fhM[fhN,chN]  FT of each id signal stored in complex form
    float*           mhM;      // mhM[binN,chN]  magnitude of each fhM column
    unsigned         chN;      // count of id signals
    unsigned         fhN;      // length of each FT id signal (fft->xN)
    unsigned        binN;      // length of each mhM column (fft->xN/2);
    unsigned          hN;      // length of each time domain id signal (hN<=fhN/2)

    unsigned         absIdx;   // abs. sample index of p->di

    cmSample_t*      dV;       // dV[fhN] delay line
    unsigned         di;       // next input into delay line

    cmSample_t*      xV;       // xV[fhN] linear delay buffer
    cmComplexR_t*   t0V;       // t0V[fhN]
    cmComplexR_t*   t1V;       // t1V[fhN]

    cmSample_t*      wndV;

    cmVectArray_t*   ftVa; 

  } cmPhat_t;


  // Allocate a PHAT based multi-channel correlator.
  // 'chN'  is the maximum count of id signals to be set via cmPhatSetId().
  // 'hN' is the the length of the id signal in samples.
  // 'alpha' weight used to emphasize the frequencies where the id signal contains energy.
  // 'mult' * 'hN' is the correlation length (fhN)
  // 'flags' See kDebugAtPhatFl and kWndAtPhatFl.
  cmPhat_t* cmPhatAlloc(  cmCtx* ctx, cmPhat_t* p, unsigned chN, unsigned hN, float alpha, unsigned mult, unsigned flags );
  cmRC_t    cmPhatFree(   cmPhat_t** pp );

  cmRC_t   cmPhatInit(  cmPhat_t* p, unsigned chN, unsigned hN, float alpha, unsigned mult, unsigned flags );  
  cmRC_t   cmPhatFinal( cmPhat_t* p );

  // Zero the audio delay line and reset the current input sample (di)
  // and absolute time index (absIdx) to 0.
  cmRC_t   cmPhatReset(  cmPhat_t* p );

  // Register an id signal with the correlator.
  cmRC_t   cmPhatSetId(  cmPhat_t* p, unsigned chIdx, const cmSample_t* hV, unsigned hN );

  // Update the correlators internal delay buffer.
  cmRC_t   cmPhatExec(   cmPhat_t* p, const cmSample_t* xV, unsigned xN );

  // Set p->xV[0:fhN-1] to the correlation function based on
  // correlation between the current audio delay line d[] and
  // the id signal in fhM[:,chIdx].
  // 'sessionId' and 'roleId' are only used to label the
  // data stored in the debug file and may be set to any
  // arbitrary value if the debug files are not being generated.
  void cmPhatChExec( 
    cmPhat_t* p,
    unsigned  chIdx,
    unsigned  sessionId,
    unsigned  roleId);


  cmRC_t cmPhatWrite( cmPhat_t* p, const char* dirStr );


  //=======================================================================================================================
  // 
  //

  typedef struct
  {
    cmObj obj;

    cmGoldSig_t*     gs;
    unsigned         xi;  // index into gs->ch[0].mdV[] of the next sample to output
    bool             zeroFl;
    cmPhat_t*        phat;
    cmVectArray_t*   va;
  } cmReflectCalc_t;

  
  cmReflectCalc_t* cmReflectCalcAlloc( cmCtx* ctx, cmReflectCalc_t* p, const cmGoldSigArg_t* gsa, float phat_alpha, unsigned phat_mult );
  cmRC_t cmReflectCalcFree( cmReflectCalc_t** pp );
  cmRC_t cmReflectCalcInit( cmReflectCalc_t* p, const cmGoldSigArg_t* gsa, float phat_alpha, unsigned phat_mult );
  cmRC_t cmReflectCalcFinal( cmReflectCalc_t* p );
  cmRC_t cmReflectCalcExec( cmReflectCalc_t* p, const cmSample_t xV, cmSample_t* yV, unsigned xyN );

  
  
#ifdef __cplusplus
}
#endif

#endif
