//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#ifndef cmProc5_h
#define cmProc5_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"Process Library 5", kw:[proclib]}
  //)

  
  //( { label:cmGoertzel file_desc:"Goertzel tone detection filter." kw:[proc]}

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

  //------------------------------------------------------------------------------------------------------------
  //)

  //( { label:cmGoldCode file_desc:"Gold code random generator." kw:[proc]}

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
    cmFIR*         fir;
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


  //------------------------------------------------------------------------------------------------------------
  //)

  //( { label:cmPhat file_desc:"Phase-aligned transform for generalized cross correlator." kw:[proc]}

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


  //------------------------------------------------------------------------------------------------------------
  //)

  //( { label:cmReflectCal file_desc:"Calculate the time of flight of Gold code acoustic reflections." kw:[proc]}


  typedef struct
  {
    cmObj obj;

    cmGoldSig_t*     gs;
    cmPhat_t*        phat;
    
    unsigned         xi;     // index into xV[] of the next sample to output
    
    unsigned         t;
    unsigned*        t0V;    // t0V[tN] - last tN signal start times 
    unsigned*        t1V;    // t1V[tN] - last tN signal detection times 
    unsigned         tN;
    unsigned         ti;
    
    cmVectArray_t*   phVa;
    cmVectArray_t*   xVa;
    cmVectArray_t*   yVa;
  } cmReflectCalc_t;

  
  cmReflectCalc_t* cmReflectCalcAlloc( cmCtx* ctx, cmReflectCalc_t* p, const cmGoldSigArg_t* gsa, float phat_alpha, unsigned phat_mult );
  cmRC_t cmReflectCalcFree(  cmReflectCalc_t** pp );
  cmRC_t cmReflectCalcInit(  cmReflectCalc_t* p, const cmGoldSigArg_t* gsa, float phat_alpha, unsigned phat_mult );
  cmRC_t cmReflectCalcFinal( cmReflectCalc_t* p );
  cmRC_t cmReflectCalcExec(  cmReflectCalc_t* p, const cmSample_t* xV, cmSample_t* yV, unsigned xyN );
  cmRC_t cmReflectCalcWrite( cmReflectCalc_t* p, const char* dirStr );

  //------------------------------------------------------------------------------------------------------------
  //)

  //( { label:cmNlms file_desc:"Normalized least mean squares echo canceller." kw:[proc]}
  
  typedef struct
  {
    cmObj          obj;
    float          mu;          // LMS step rate
    unsigned       hN;          // filter length
    unsigned       delayN;      // fixed delay to apply to align xV with fV.
    unsigned       dN;          // max length of the fixed delay
    cmSample_t*    delayV;      // delayV[ dN ] fixed delay buffer[]
    unsigned       di;          // delay index
    double*        wV;          // wV[hN] filter weights
    double*        hV;          // hV[hN] filter delay line    
    unsigned       w0i;         // The index into hV[] of the start of the delay line.

    cmVectArray_t* uVa;
    cmVectArray_t* fVa;
    cmVectArray_t* eVa;
  } cmNlmsEc_t;

  cmNlmsEc_t* cmNlmsEcAlloc( cmCtx* ctx, cmNlmsEc_t* p, double srate, float mu, unsigned hN, unsigned delayN );
  cmRC_t      cmNlmsEcFree( cmNlmsEc_t** pp );
  cmRC_t      cmNlmsEcInit( cmNlmsEc_t* p, double srate, float mu, unsigned hN, unsigned delayN );
  cmRC_t      cmNlmsEcFinal( cmNlmsEc_t* p );
  
  // xV[] unfiltered reference signal  (direct from xform output)
  // fV[] filtered reference signal    (from mic)
  // yV[] echo-canelled signal 
  cmRC_t      cmNlmsEcExec( cmNlmsEc_t* p, const cmSample_t* xV, const cmSample_t* fV, cmSample_t* yV, unsigned xyN );
  cmRC_t      cmNlmsEcWrite( cmNlmsEc_t* p, const cmChar_t* dir );

  void        cmNlmsEcSetMu(     cmNlmsEc_t* p, float mu );
  void        cmNlmsEcSetDelayN( cmNlmsEc_t* p, unsigned delayN );
  void        cmNlmsEcSetIrN(    cmNlmsEc_t* p, unsigned irN );
  
  //------------------------------------------------------------------------------------------------------------
  //)

  //( { label:cmSeqAlign file_desc:"Align two sequences where each sequence is composed of records with varying numbers of features." kw:[proc]}

  typedef struct cmSeqAlignLoc_str
  {
    unsigned                  id;  // location id
    unsigned                  vN;  // 
    unsigned*                 vV;  // vV[vN]
    struct cmSeqAlignLoc_str* link;
  } cmSeqAlignLoc_t;

  typedef struct cmSeqAlignSeq_str
  {
    unsigned                  id;
    cmSeqAlignLoc_t*          locL;
    struct cmSeqAlignSeq_str* link;
  } cmSeqAlignSeq_t;
  
  typedef struct
  {
    cmObj            obj;
    cmSeqAlignSeq_t* seqL;
    
  } cmSeqAlign_t;

  cmSeqAlign_t* cmSeqAlignAlloc(  cmCtx* ctx, cmSeqAlign_t* p );
  cmRC_t        cmSeqAlignFree(   cmSeqAlign_t** p );
  cmRC_t        cmSeqAlignInit(   cmSeqAlign_t* p );
  cmRC_t        cmSeqAlignFinal(  cmSeqAlign_t* p );
  cmRC_t        cmSeqAlignInsert( cmSeqAlign_t* p, unsigned seqId, unsigned locId, unsigned value );
  cmRC_t        cmSeqAlignExec(   cmSeqAlign_t* p );
  void          cmSeqAlignReport( cmSeqAlign_t* p, cmRpt_t* rpt );
  
  
  //------------------------------------------------------------------------------------------------------------
  //)
  
#ifdef __cplusplus
}
#endif

#endif
