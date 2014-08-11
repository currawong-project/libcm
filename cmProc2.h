#ifndef cmProc2_h
#define cmProc2_h

#ifdef __cplusplus
extern "C" {
#endif

  //------------------------------------------------------------------------------------------------------------
  // cmArray is an expandable array designed to work easily with the alloc/init/final/free model
  // used by this library.  The arrays can be safely used by using the cmArrayAllocXXX macros 
  // with static cmArray member fields during object allocation. cmArrayResizeXXX macros are then
  // used during the object initialization phase to allocate the actual array data space.  Notice that
  // when used this way there is no need to call cmArrayFinal() prior to cmArrayResizeXXX(). 

  // The data memory used by cmArray's is allocated through the cmAllocData() and cmAllocDataZ()
  // macros.  The resulting base memory address is therefore guaranteed to be aligned to a
  // 16 byte address boundary.
  typedef struct
  {
    cmObj    obj;
    char*    ptr;
    unsigned allocByteCnt;
    unsigned eleCnt;
    unsigned eleByteCnt;
  } cmArray;

  enum
  {
    kZeroArrayFl = 0x01
  };

  cmArray* cmArrayAllocate(       cmCtx* c, cmArray* p, unsigned eleCnt, unsigned eleByteCnt, unsigned flags );
  cmRC_t   cmArrayFree(           cmArray** pp );
  cmRC_t   cmArrayInit(           cmArray* p, unsigned eleCnt, unsigned eleByteCnt, unsigned flags );
  cmRC_t   cmArrayFinal(          cmArray* p );
  char*    cmArrayReallocDestroy( cmArray* p, unsigned newEleCnt, unsigned newEleByteCnt, unsigned flags );
  void     cmArrayReallocDestroyV(cmArray* p, int      eleByteCnt,unsigned flags,  ... );
  char*    cmArrayReallocPreserve(cmArray* p, unsigned newEleCnt, unsigned newEleByteCnt, unsigned flags );


#define cmArrayAlloc(      c, p )               cmArrayAllocate(c,p,0,0,0);
#define cmArrayAllocInit(  c, p, eleCnt, type ) cmArrayAllocate(c,p,eleCnt,sizeof(type),0)
#define cmArrayAllocInitZ( c, p, eleCnt, type ) cmArrayAllocate(c,p,eleCnt,sizeof(type),kZeroArrayFl)

#define cmArrayResize(         c, p, newEleCnt, type ) (type*)cmArrayReallocDestroy(c,p,newEleCnt,sizeof(type), 0 )
#define cmArrayResizeZ(        c, p, newEleCnt, type ) (type*)cmArrayReallocDestroy(c,p,newEleCnt,sizeof(type), kZeroArrayFl )
#define cmArrayResizePreserve( c, p, newEleCnt, type ) (type*)cmArrayReallocPreserve(c,p,newEleCnt,sizeof(type), 0 )
#define cmArrayResizePreserveZ(c, p, newEleCnt, type ) (type*)cmArrayReallocPreserve(c,p,newEleCnt,sizeof(type), kZeroArrayFl )
#define cmArrayResizeV(        c, p, type, ... )              cmArrayReallocDestroyV(c,p,sizeof(type),0,##__VA_ARGS__)
#define cmArrayResizeVZ(       c, p, type, ... )              cmArrayReallocDestroyV(c,p,sizeof(type),kZeroArrayFl,##__VA_ARGS__)
  

#define cmArrayPtr(     type, p )            (type*)(p)->ptr
#define cmArrayCount(   p )                  (p)->eleCnt


  //------------------------------------------------------------------------------------------------------------
  typedef struct
  {
    cmObj         obj;
    cmAudioFileH_t  h;
    unsigned      chCnt;
    unsigned      curChCnt;
    unsigned      procSmpCnt;
    char*         fn;
    cmSample_t*   bufV;
  } cmAudioFileWr;

  cmAudioFileWr*     cmAudioFileWrAlloc( cmCtx* c, cmAudioFileWr* p, unsigned procSmpCnt, const char* fn, double srate, unsigned chCnt, unsigned bitsPerSample );
  cmRC_t             cmAudioFileWrFree(  cmAudioFileWr** pp );
  cmRC_t             cmAudioFileWrInit(  cmAudioFileWr* p, unsigned procSmpCnt, const char* fn, double srate, unsigned chCnt, unsigned bitsPerSample );
  cmRC_t             cmAudioFileWrFinal( cmAudioFileWr* p );
  cmRC_t             cmAudioFileWrExec(  cmAudioFileWr* p, unsigned chIdx, const cmSample_t* sp, unsigned sn );
  void               cmAudioFileWrTest();

  //------------------------------------------------------------------------------------------------------------
  typedef struct
  {
    cmObj      obj;
    unsigned    rn;
    unsigned    cn;
    cmSample_t *bufPtr;

  } cmMatrixBuf;

  /// Set p to NULL to dynamically allocate the object
  cmMatrixBuf*       cmMatrixBufAllocFile(cmCtx* c, cmMatrixBuf* p, const char* fn );
  cmMatrixBuf*       cmMatrixBufAllocCopy(cmCtx* c, cmMatrixBuf* p, unsigned rn, unsigned cn, const cmSample_t* sp );
  cmMatrixBuf*       cmMatrixBufAlloc(    cmCtx* c, cmMatrixBuf* p, unsigned rn, unsigned cn );
  cmRC_t             cmMatrixBufFree(     cmMatrixBuf**p );
  cmRC_t             cmMatrixBufInitFile( cmMatrixBuf* p, const char* fn );
  cmRC_t             cmMatrixBufInitCopy( cmMatrixBuf* p, unsigned rn, unsigned cn, const cmSample_t* sp );
  cmRC_t             cmMatrixBufInit(     cmMatrixBuf* p, unsigned rn, unsigned cn );
  cmRC_t             cmMatrixBufFinal(    cmMatrixBuf* p );
  cmSample_t*        cmMatrixBufColPtr(   cmMatrixBuf* p, unsigned ci );
  cmSample_t*        cmMatrixBufRowPtr(   cmMatrixBuf* p, unsigned ri );
  void               cmMatrixBufTest();


  //------------------------------------------------------------------------------------------------------------

  enum
  {
    kInvalidWfId,
    kSineWfId,
    kCosWfId,
    kSquareWfId,
    kTriangleWfId,
    kSawtoothWfId,
    kWhiteWfId,
    kPinkWfId,
    kPulseWfId,
    kImpulseWfId,
    kSilenceWfId,
    kPhasorWfId,
    kSeqWfId,            // always incrementing integer sequence (srate,frq,otCnt is ignored)
  };

  typedef struct
  {
    cmObj              obj;
    unsigned           wfId;
    unsigned           overToneCnt;
    double             fundFrqHz;
    cmSample_t*        outV;
    unsigned           outN;   // outN == procSmpCnt
    unsigned           phase;
    cmSample_t         delaySmp;
    double             srate;
  } cmSigGen;


  /// Set p to NULL to dynamically allocate the object
  /// The last three arguments are optional. Set wfId to kInvalidWfId to allocate the signal generator without initializint it.
  cmSigGen* cmSigGenAlloc( cmCtx* c, cmSigGen*  p, unsigned procSmpCnt, double srate, unsigned wfId, double fundFrqHz, unsigned overToneCnt );
  cmRC_t    cmSigGenFree(  cmSigGen** p );
  cmRC_t    cmSigGenInit(  cmSigGen*  p, unsigned procSmpCnt, double srate, unsigned wfId, double fundFrqHz, unsigned overToneCnt );
  cmRC_t    cmSigGenFinal( cmSigGen*  p );
  cmRC_t    cmSigGenExec(  cmSigGen*  p );

  //------------------------------------------------------------------------------------------------------------
  typedef struct
  {
    cmObj*      obj;
    cmSample_t* bufPtr;
    unsigned    bufSmpCnt;    // count of samples in the delay line (bufSmpCnt = procSmpCnt+delaySmpCnt)
    unsigned    procSmpCnt;   // maximum legal samples to receive in a single call to cmDelayExec()
    unsigned    delaySmpCnt;  // delay time in samples
    int         delayInIdx;   // index into bufPtr[] of next element to receive an incoming sample
    unsigned    outCnt;       // count of valid buffers in outV[] 
    cmSample_t* outV[2];      // pointers to output buffers
    unsigned    outN[2];      // length of output buffers (the sum of the length of both output buffers is always procSmpCnt)
  } cmDelay;

  cmDelay*          cmDelayAlloc(   cmCtx* c, cmDelay* p, unsigned procSmpCnt, unsigned delaySmpCnt );
  cmRC_t            cmDelayFree(    cmDelay** p );
  cmRC_t            cmDelayInit(    cmDelay* p, unsigned procSmpCnt, unsigned delaySmpCnt );
  cmRC_t            cmDelayFinal(   cmDelay* p );
  cmRC_t            cmDelayCopyIn(  cmDelay* p, const cmSample_t* sp, unsigned sn );
  cmRC_t            cmDelayAdvance( cmDelay* p, unsigned sn );
  cmRC_t            cmDelayExec(    cmDelay* p, const cmSample_t* sp, unsigned sn, bool bypassFl );
  void              cmDelayTest();

  //------------------------------------------------------------------------------------------------------------
  typedef struct
  {
    cmObj       obj;
    double*     coeffV;      // FIR coefficient vector (impulse response)
    unsigned    coeffCnt;    // count of elements in coeffV
    double*     delayV;      // delay vector contains one less elements than the coeff array
    cmSample_t* outV;        // output signal
    unsigned    outN;        // length of the output signal (outN == ctx.procSmpCnt)
    unsigned    delayIdx;    // current next sample to receive input in the the delay line

  } cmFIR;

  enum { kHighPassFIRFl = 0x01 };

  /// Set p to NULL to dynamically allocate the object.
  cmFIR* cmFIRAllocKaiser(cmCtx* c, cmFIR* p, unsigned procSmpCnt, double srate, double passHz, double stopHz, double passDb, double stopDb );
  cmFIR* cmFIRAllocSinc(  cmCtx* c, cmFIR* p, unsigned procSmpCnt, double srate, unsigned sincSmpCnt, double fcHz, unsigned flags );
  cmRC_t cmFIRFree(       cmFIR** pp );
  cmRC_t cmFIRInitKaiser( cmFIR* p, unsigned procSmpCnt, double srate, double passHz, double stopHz, double passDb, double stopDb );
  cmRC_t cmFIRInitSinc(   cmFIR* p, unsigned procSmpCnt, double srate, unsigned sincSmpCnt, double fcHz, unsigned flags );
  cmRC_t cmFIRFinal(      cmFIR* p );
  cmRC_t cmFIRExec(       cmFIR* p, const cmSample_t* sp, unsigned sn );
  void   cmFIRTest();

  //------------------------------------------------------------------------------------------------------------
  // Apply a generic function to a windowed signal with a one sample hop size.

  typedef cmSample_t (*cmFuncFiltPtr_t)( const cmSample_t* sp, unsigned sn, void* userPtr );

  typedef struct
  {
    cmObj           obj;
    cmFuncFiltPtr_t funcPtr;
    cmShiftBuf      shiftBuf;
    cmSample_t*     outV;
    unsigned        outN;          // outN == procSmpCnt
    unsigned        curWndSmpCnt;
    unsigned        wndSmpCnt;
    void*           userPtr;
  } cmFuncFilter;

  /// Set p to NULL to dynamically allocate the object.
  cmFuncFilter* cmFuncFilterAlloc( cmCtx* c, cmFuncFilter* p, unsigned procSmpCnt, cmFuncFiltPtr_t funcPtr, void* userPtr, unsigned wndSmpCnt );
  cmRC_t        cmFuncFilterFree(  cmFuncFilter** pp );
  cmRC_t        cmFuncFilterInit(  cmFuncFilter* p, unsigned procSmpCnt, cmFuncFiltPtr_t funcPtr, void* userPtr, unsigned wndSmpCnt );
  cmRC_t        cmFuncFilterFinal( cmFuncFilter* p );
  cmRC_t        cmFuncFilterExec(  cmFuncFilter* p, const cmSample_t* sp, unsigned sn );
  void          cmFuncFilterTest();

  //------------------------------------------------------------------------------------------------------------
  typedef struct
  {
    cmObj           obj;
    unsigned        stateN;    // count of states
    unsigned        symN;      // count of discrete observation symbols

    cmReal_t* initV;     // initial state probability vector init[ stateN ]
    cmReal_t* transM;    // transition probability matrix   trans[ stateN (current), stateN (next) ]
    cmReal_t* stsM;      // state to symbol prob. matrix    stsM[  stateN, symN ]

  } cmDhmm;


  cmDhmm* cmDhmmAlloc( cmCtx* c, cmDhmm* p, unsigned stateN, unsigned symN, cmReal_t* initV,  cmReal_t* transM, cmReal_t* stsM );
  cmRC_t  cmDhmmFree(  cmDhmm** pp );
  cmRC_t  cmDhmmInit(  cmDhmm* p, unsigned stateN, unsigned symN, cmReal_t* initV, cmReal_t* transM, cmReal_t* stsM );
  cmRC_t  cmDhmmFinal( cmDhmm* p );
  cmRC_t  cmDhmmExec(  cmDhmm* p );

  cmRC_t  cmDhmmGenObsSequence( cmDhmm* p, unsigned* dbp, unsigned dn );
  cmRC_t  cmDhmmForwardEval(    cmDhmm* p, const cmReal_t* statePrV, const unsigned* obsV, unsigned obsN, cmReal_t* alphaM, unsigned flags, cmReal_t* logProbPtr );
  cmRC_t  cmDhmmReport(         cmDhmm* p );

  void    cmDhmmTest();

  //------------------------------------------------------------------------------------------------------------
  typedef struct
  {
    cmObj     obj;
    cmFftSR*  fft;
    cmIFftRS* ifft;

    cmComplexR_t* H; 

    unsigned    hn;
    cmSample_t* olaV;   // olaV[hn-1];

    cmSample_t* outV;  // outV[procSmpCnt]
    unsigned    outN;  // outN == procSmpCnt
  } cmConvolve;

  // After cmConvolveExec() outV[outN] contains the first outN samples
  // which are complete and can be used by the application.
  // The tail of the convolution is held in olaV[hn-1] and will
  // be automatically summed with the beginning of the next convolution
  // frame. 

  // BUG BUG BUG
  // This code seems to have a problem when hn != procSmpCnt (or maybe hn > procSmpCnt ???).  
  // See mas/main.c convolve() where procSmpCnt must be set to wndSmpCnt size or 
  // only the first half of the window is emitted.


  // h[hn] is the impulse response to convolve with
  cmConvolve* cmConvolveAlloc( cmCtx* c, cmConvolve* p, const cmSample_t* h, unsigned hn, unsigned procSmpCnt );
  cmRC_t      cmConvolveFree(  cmConvolve** pp );
  cmRC_t      cmConvolveInit(  cmConvolve* p, const cmSample_t* h, unsigned hn, unsigned procSmpCnt );
  cmRC_t      cmConvolveFinal( cmConvolve* p );
  // xn must be <= procSmpCnt
  cmRC_t      cmConvolveExec(  cmConvolve* p, const cmSample_t* x, unsigned xn );

  cmRC_t      cmConvolveSignal( cmCtx* c, const cmSample_t* h, unsigned hn, const cmSample_t* x, unsigned xn, cmSample_t* y, unsigned yn );

  cmRC_t      cmConvolveTest( cmRpt_t* rpt, cmLHeapH_t lhH, cmSymTblH_t stH );

  //------------------------------------------------------------------------------------------------------------
  typedef struct
  {
    cmObj     obj; 
    cmReal_t* dctMtx;    // dctMtx[ binCnt, bandCnt ]
    cmReal_t* filtMask;  // filtMask[ bandCnt, bandCnt ]
    unsigned  binCnt;    // bin cnt of input magnitude spectrum
    unsigned  bandCnt;   // must be <= kDefaultBarkBandCnt
    cmReal_t* outV;      // outV[binCnt]

  } cmBfcc;

  cmBfcc* cmBfccAlloc( cmCtx* ctx, cmBfcc* p, unsigned bandCnt, unsigned binCnt, double binHz );
  cmRC_t  cmBfccFree(  cmBfcc** pp );
  cmRC_t  cmBfccInit(  cmBfcc* p, unsigned bandCnt, unsigned binCnt, double binHz );
  cmRC_t  cmBfccFinal( cmBfcc* p );
  cmRC_t  cmBfccExec(  cmBfcc* p, const cmReal_t* magV, unsigned binCnt );
  void    cmBfccTest( cmRpt_t* rpt, cmLHeapH_t lhH, cmSymTblH_t stH );

  //------------------------------------------------------------------------------------------------------------
  typedef struct
  {
    cmObj     obj; 
    //cmIFftRR  ft;
    unsigned  dct_cn;    // (binCnt-1)*2
    cmReal_t* dctM;      // dctM[ outN, dct_cn ]
    unsigned  binCnt;    // bin cnt of input magnitude spectrum
    unsigned  outN;      // count of cepstral coeff's
    cmReal_t* outV;      // outV[outN]
    

  } cmCeps;

  // outN is the number of cepstral coeff's in the output vector
  cmCeps* cmCepsAlloc( cmCtx* ctx, cmCeps* p, unsigned binCnt, unsigned outN );
  cmRC_t  cmCepsFree(  cmCeps** pp );
  cmRC_t  cmCepsInit(  cmCeps* p, unsigned binCnt, unsigned outN );
  cmRC_t  cmCepsFinal( cmCeps* p );
  cmRC_t  cmCepsExec(  cmCeps* p, const cmReal_t* magV, const cmReal_t* phsV, unsigned binCnt );


  //------------------------------------------------------------------------------------------------------------
  typedef struct
  {
    cmObj       obj;
    cmWndFunc   wf;
    unsigned    wndSmpCnt;
    unsigned    hopSmpCnt;
    unsigned    procSmpCnt;

    cmSample_t* bufV;       // bufV[wndSmpCnt] overlap add buffer 
    cmSample_t* outV;       // outV[hopSmpCnt] output vector
    cmSample_t* outPtr;     // outPtr[procSmpCnt] output vector
    unsigned    idx;        // idx of next val in bufV[] to be moved to outV[]


  } cmOla;

  // hopSmpCnt must be <= wndSmpCnt.
  // hopSmpCnt must be an even multiple of procSmpCnt.
  // Call cmOlaExecR() or cmOlaExecS() at the spectral frame rate.
  // Call cmOlaExecOut() at the time domain audio frame rate.

  // Set wndTypeId to one of the cmWndFuncXXX enumerated widnow type id's.
  cmOla* cmOlaAlloc( cmCtx* ctx, cmOla* p, unsigned wndSmpCnt, unsigned hopSmpCnt, unsigned procSmpCnt, unsigned wndTypeId );
  cmRC_t cmOlaFree(  cmOla** pp );
  cmRC_t cmOlaInit(  cmOla* p, unsigned wndSmpCnt, unsigned hopSmpCnt, unsigned procSmpCnt, unsigned wndTypeId );
  cmRC_t cmOlaFinal( cmOla* p );
  cmRC_t cmOlaExecS( cmOla* p, const cmSample_t* xV, unsigned xN );
  cmRC_t cmOlaExecR( cmOla* p, const cmReal_t*   xV, unsigned xN );
  const cmSample_t* cmOlaExecOut(cmOla* p );



  //------------------------------------------------------------------------------------------------------------

  typedef struct
  {
    cmObj     obj;
    cmReal_t* hzV;  // hzV[binCnt] output vector - frequency in Hertz 

    cmReal_t* phsV; // phsV[binCnt] 

    cmReal_t* wV;   // bin freq in rads/hop
    double   srate;
    unsigned hopSmpCnt;
    unsigned binCnt;

  } cmPhsToFrq;

  cmPhsToFrq* cmPhsToFrqAlloc( cmCtx* c, cmPhsToFrq* p, double srate, unsigned binCnt, unsigned hopSmpCnt );
  cmRC_t  cmPhsToFrqFree( cmPhsToFrq** p );
  cmRC_t  cmPhsToFrqInit( cmPhsToFrq*  p, double srate, unsigned binCnt, unsigned hopSmpCnt );
  cmRC_t  cmPhsToFrqFinal(cmPhsToFrq*  p );
  cmRC_t  cmPhsToFrqExec( cmPhsToFrq*  p, const cmReal_t* phsV );
 

  //------------------------------------------------------------------------------------------------------------

  enum 
  {
    kNoCalcHzPvaFl = 0x00,
    kCalcHzPvaFl   = 0x01
  };

  typedef struct
  {
    cmObj           obj;
    cmShiftBuf      sb;
    cmFftSR         ft;
    cmWndFunc       wf;
    cmPhsToFrq      pf;

    unsigned        flags;
    unsigned        procSmpCnt;
    double          srate;
    unsigned        wndSmpCnt;
    unsigned        hopSmpCnt;
    unsigned        binCnt;

    const cmReal_t* magV; // amplitude NOT power
    const cmReal_t* phsV;
    const cmReal_t* hzV;

  
  } cmPvAnl;

  cmPvAnl*   cmPvAnlAlloc( cmCtx* ctx, cmPvAnl* p, unsigned procSmpCnt, double srate, unsigned wndSmpCnt, unsigned hopSmpCnt, unsigned flags );
  cmRC_t     cmPvAnlFree( cmPvAnl** pp );
  cmRC_t     cmPvAnlInit( cmPvAnl* p, unsigned procSmpCnt, double srate, unsigned wndSmpCnt, unsigned hopSmpCnt, unsigned flags );
  cmRC_t     cmPvAnlFinal(cmPvAnl* p );

  // Returns true when a new spectrum has been computed
  bool     cmPvAnlExec( cmPvAnl* p, const cmSample_t* x, unsigned xN );

  //------------------------------------------------------------------------------------------------------------

  typedef struct
  {
    cmObj           obj;
    cmIFftRS        ft;
    cmWndFunc       wf;
    cmOla           ola;

    cmReal_t*       minRphV;
    cmReal_t*       maxRphV;
    cmReal_t*       itrV;
    cmReal_t*       phs0V;
    cmReal_t*       mag0V;
    cmReal_t*       phsV;
    cmReal_t*       magV; 

    double          outSrate;
    unsigned        procSmpCnt;
    unsigned        wndSmpCnt;
    unsigned        hopSmpCnt;
    unsigned        binCnt;
  
  } cmPvSyn;

  cmPvSyn*   cmPvSynAlloc( cmCtx* ctx, cmPvSyn* p, unsigned procSmpCnt, double outSrate, unsigned wndSmpCnt, unsigned hopSmpCnt,unsigned wndTypeId );
  cmRC_t     cmPvSynFree( cmPvSyn** pp );
  cmRC_t     cmPvSynInit( cmPvSyn* p, unsigned procSmpCnt, double outSrate, unsigned wndSmpCnt, unsigned hopSmpCnt,unsigned wndTypeId );
  cmRC_t     cmPvSynFinal(cmPvSyn* p );
  cmRC_t     cmPvSynExec( cmPvSyn* p, const cmReal_t* magV, const cmReal_t* phsV );
  const cmSample_t* cmPvSynExecOut(cmPvSyn* p );



  //------------------------------------------------------------------------------------------------------------


  // callback selector values
  enum
  {
    kAttackMsId,
    kReleaseMsId,
    kDspMsId            // return 0 if the voice is no longer active
  };

  // voice flags
  enum
  {
    kActiveMsFl  = 0x01, // set if the voice is active
    kKeyGateMsFl = 0x02, // set if the key  is down for this note
  };

  struct cmMidiSynth_str;
  struct cmMidiSynthCh_str;
  struct cmMidiVoice_str;


  // voice update callback - use voicePtr->pgm.cbDataPtr to get voice specific data
  typedef int (*cmMidiSynthCb_t)( struct cmMidiVoice_str* voicePtr, unsigned sel, cmSample_t* outChArray[], unsigned outChCnt );


  typedef struct
  {
    cmMidiByte_t    pgm;          // MIDI pgm number   
    cmMidiSynthCb_t cbPtr;        // voice update callback
    void*           cbDataPtr;    // user data pointer
  } cmMidiSynthPgm;


  typedef struct cmMidiVoice_str
  {
    unsigned                  index;    // voice index
    unsigned                  flags;    // see kXXXMsFl above
    cmMidiByte_t              pitch;    // note-on pitch
    cmMidiByte_t              velocity; // note-on/off veloctiy
    cmMidiSynthPgm            pgm;      // pgm associated with this voice
    struct cmMidiSynthCh_str* chPtr;    // pointer to owning ch
    struct cmMidiVoice_str*   link;     // link to next active/avail voice in chain
  } cmMidiVoice;

  typedef struct cmMidiSynthCh_str
  {
    cmMidiByte_t            midiCtl[  kMidiCtlCnt ]; // current ctl values
    short                   pitchBend;               // current pitch bend value
    cmMidiByte_t            pgm;                     // last pgm received
    cmMidiVoice*            active;                  // first active voice on this channel
    struct cmMidiSynth_str* synthPtr;                // owning synth
  } cmMidiSynthCh;

  typedef struct cmMidiSynth_str
  {
    cmObj          obj;
    cmMidiSynthCh  chArray[ kMidiChCnt ];   // midi channel array  
    unsigned       voiceCnt;                // count of voice records
    cmMidiVoice*   avail;                   // avail voice chain
    unsigned       activeVoiceCnt;          // current count of active voices
    unsigned       voiceStealCnt;           // count of times voice stealing was required 
    cmMidiVoice*   voiceArray;              // array of voice records
    cmMidiSynthPgm pgmArray[ kMidiPgmCnt ]; // array of pgm records
    unsigned       procSmpCnt;              // samples per DSP cycle 
    unsigned       outChCnt;                // count of output channels
    cmSample_t*    outM;                    // outM[ procSmpCnt, outChCnt ] output buffer
    cmSample_t**   outChArray;              // buffer of pointers to each output channel
    cmReal_t       srate;                   // output signal sample rate
  } cmMidiSynth;

  cmMidiSynth* cmMidiSynthAlloc( cmCtx* ctx, cmMidiSynth* p, const cmMidiSynthPgm* pgmArray, unsigned pgmCnt, unsigned voiceCnt, unsigned procSmpCnt, unsigned outChCnt, cmReal_t srate  );
  cmRC_t       cmMidiSynthFree(  cmMidiSynth** pp );
  cmRC_t       cmMidiSynthInit(  cmMidiSynth* p, const cmMidiSynthPgm* pgmArray, unsigned pgmCnt, unsigned voiceCnt, unsigned procSmpCnt, unsigned outChCnt, cmReal_t srate  );
  cmRC_t       cmMidiSynthFinal( cmMidiSynth* p );
  cmRC_t       cmMidiSynthOnMidi(cmMidiSynth* p, const cmMidiPacket_t* pktArray, unsigned pktCnt );
  cmRC_t       cmMidiSynthExec(  cmMidiSynth* p, cmSample_t** outChArray, unsigned outChCnt );


  //------------------------------------------------------------------------------------------------------------

  // state id's
  enum
  {
    kOffWtId,
    kAtkWtId,
    kDcyWtId,
    kSusWtId,
    kRlsWtId
  };

  typedef struct
  {
    cmObj       obj;              
    cmReal_t    hz;        // current frq in Hz  
    cmReal_t    level;     // current gain (0.0 to 1.0)
    cmReal_t    phase;     // osc phase (radians)
    unsigned    durSmpCnt; // count of samples generated so far
    unsigned    state;     // osc state - see kXXXWtId above
    cmSample_t* outV;      // signal output vector
    unsigned    outN;      // samples in outV[]
  } cmWtVoice;

  cmWtVoice* cmWtVoiceAlloc( cmCtx* ctx, cmWtVoice* p, unsigned procSmpCnt, cmReal_t hz );
  cmRC_t     cmWtVoiceFree( cmWtVoice** pp );
  cmRC_t     cmWtVoiceInit( cmWtVoice*  p, unsigned procSmpCnt, cmReal_t hz );
  cmRC_t     cmWtVoiceFinal( cmWtVoice* p );

  // 'sel' values are cmMidiSynthExec (kXXXMsId) values
  // Set outChArray[] to NULL to use internal audio buffer.
  int        cmWtVoiceExec( cmWtVoice* p, struct cmMidiVoice_str* voicePtr, unsigned sel, cmSample_t* outChArray[], unsigned outChCnt );


  //------------------------------------------------------------------------------------------------------------

  typedef struct
  {
    cmObj obj;

    cmWtVoice**    voiceArray;   // osc state array
    unsigned       voiceCnt;

    cmSample_t*    buf;
    cmSample_t**   chArray;
    unsigned       chCnt;
    unsigned       procSmpCnt; // count of samples in each chArray[i] sample vector

    double         srate;     // synth sample rate

  } cmWtVoiceBank;

  cmWtVoiceBank* cmWtVoiceBankAlloc( cmCtx* ctx, cmWtVoiceBank* p, double srate, unsigned procSmpCnt, unsigned voiceCnt, unsigned chCnt );
  cmRC_t     cmWtVoiceBankFree( cmWtVoiceBank** pp );
  cmRC_t     cmWtVoiceBankInit( cmWtVoiceBank*  p, double srate, unsigned procSmpCnt, unsigned voiceCnt, unsigned chCnt );
  cmRC_t     cmWtVoiceBankFinal( cmWtVoiceBank* p );

  // 'sel' values are cmMidiSynthExec (kXXXMsId) values
  // Set outChArray[] to NULL to use internal audio buffer.
  // Return 0 if the voice has gone inactive otherwise return 1.
  int        cmWtVoiceBankExec( cmWtVoiceBank* p, struct cmMidiVoice_str* voicePtr, unsigned sel, cmSample_t* chArray[], unsigned chCnt );



  //------------------------------------------------------------------------------------------------------------

  typedef struct
  {
    cmObj           obj;
    cmSample_t*     bufV;   // bufV[ bufN ]
    unsigned        bufN;
    cmAudioFileInfo_t info;
    unsigned        begSmpIdx;
    unsigned        chIdx;
    char*           fn;
  } cmAudioFileBuf;

  // set 'durSmpCnt' to cmInvalidCnt to include all samples to the end of the file
  cmAudioFileBuf* cmAudioFileBufAlloc( cmCtx* ctx, cmAudioFileBuf* p, unsigned procSmpCnt, const char* fn, unsigned chIdx, unsigned begSmpIdx, unsigned durSmpCnt );
  cmRC_t          cmAudioFileBufFree( cmAudioFileBuf** pp );
  cmRC_t          cmAudioFileBufInit( cmAudioFileBuf* p, unsigned procSmpCnt, const char* fn, unsigned chIdx, unsigned begSmpIdx, unsigned durSmpCnt );
  cmRC_t          cmAudioFileBufFinal(cmAudioFileBuf* p );

  // Returns the count of samples copied into outV or 0 if smpIdx >= p->bufN. 
  // If less than outN samples are available then the remaining samples are set to 0.  
  unsigned        cmAudioFileBufExec( cmAudioFileBuf* p, unsigned smpIdx, cmSample_t* outV, unsigned outN, bool sumIntoOutFl );


  //------------------------------------------------------------------------------------------------------------
  // Multi-delay.  Each of the taps of this delay operates as a independent delay with feedback.
  
  // Delay line specification.
  typedef struct
  {
    cmReal_t    delayGain;       // delay gain
    cmReal_t    delayMs;         // delay time in milliseconds  
    cmReal_t    delaySmpFrac;    // delay time in samples (next fractional delay index = inIdx - delaySmpFrac) 
    cmSample_t* delayBuf;        // delayBuf[delayBufSmpCnt] delay line memory 
    int         delayBufSmpCnt;  // delay buffer length in samples
    int         inIdx;           // next delay input index
  } cmMDelayHead;

  typedef struct
  {
    cmObj         obj;
    unsigned      delayCnt;      // count of taps
    cmMDelayHead* delayArray;    // tap specs  
    cmSample_t*   outV;          // outV[outN] output buffer
    unsigned      outN;          // procSmpCnt
    cmReal_t      fbCoeff;       // feedback coeff.
    cmReal_t      srate;         // system sample rate
  } cmMDelay;


  cmMDelay* cmMDelayAlloc( cmCtx* ctx, cmMDelay* p, unsigned procSmpCnt, cmReal_t srate, cmReal_t fbCoeff, unsigned delayCnt, const cmReal_t* delayMsArray, const cmReal_t* delayGainArray );
  cmRC_t    cmMDelayFree(  cmMDelay** pp );
  cmRC_t    cmMDelayInit( cmMDelay* p, unsigned procSmpCnt, cmReal_t srate, cmReal_t fbCoeff, unsigned delayCnt, const cmReal_t* delayMsArray, const cmReal_t* delayGainArray );
  cmRC_t    cmMDelayFinal( cmMDelay* p );
  cmRC_t    cmMDelayExec(  cmMDelay* p, const cmSample_t* sigV, cmSample_t* outV, unsigned sigN, bool bypassFl ); 
  void      cmMDelaySetTapMs( cmMDelay* p, unsigned tapIdx, cmReal_t ms );
  void      cmMDelaySetTapGain(cmMDelay* p, unsigned tapIdx, cmReal_t gain );
  void      cmMDelayReport( cmMDelay* p, cmRpt_t* rpt );

  //------------------------------------------------------------------------------------------------------------
  enum
  {
    kEnableAspFl  = 0x01,
    kDelAspFl     = 0x02
  };


  typedef struct cmAudioSeg_str
  {
    cmAudioFileBuf*    bufPtr;     // pointer to the audio file buffer this segment is contained in
    unsigned           id;         // id (unique amoung segments) 
    unsigned           smpIdx;     // offset into audioBuf[] of first sample
    unsigned           smpCnt;     // total count of samples to play 
    unsigned           outChIdx;   // output buffer channel 
    unsigned           outSmpIdx;  // outSmpIdx + smpIdx == next sample to play
    unsigned           flags;      // see kXXXAspFl
  } cmAudioSeg;

  typedef struct
  {
    cmObj             obj;
    unsigned          segCnt;
    cmAudioSeg*       segArray;
    unsigned          procSmpCnt;
    cmSample_t**      outChArray;
    unsigned          outChCnt;
    cmSample_t*       outM;
  } cmAudioSegPlayer;

  cmAudioSegPlayer* cmAudioSegPlayerAlloc( cmCtx* ctx, cmAudioSegPlayer* p, unsigned procSmpCnt, unsigned outChCnt );
  cmRC_t            cmAudioSegPlayerFree(   cmAudioSegPlayer** pp );
  cmRC_t            cmAudioSegPlayerInit(   cmAudioSegPlayer* p, unsigned procSmpCnt, unsigned outChCnt );
  cmRC_t            cmAudioSegPlayerFinal(  cmAudioSegPlayer* p );
  cmRC_t            cmAudioSegPlayerInsert( cmAudioSegPlayer* p, unsigned id, cmAudioFileBuf* bufPtr, unsigned smpIdx, unsigned smpCnt, unsigned outChIdx );
  cmRC_t            cmAudioSegPlayerEdit(   cmAudioSegPlayer* p, unsigned id, cmAudioFileBuf* bufPtr, unsigned smpIdx, unsigned smpCnt, unsigned outChIdx );
  cmRC_t            cmAudioSegPlayerRemove( cmAudioSegPlayer* p, unsigned id, bool delFl );
  cmRC_t            cmAudioSegPlayerEnable( cmAudioSegPlayer* p, unsigned id, bool enableFl, unsigned outSmpIdx );
  cmRC_t            cmAudioSegPlayerReset(  cmAudioSegPlayer* p );
  cmRC_t            cmAudioSegPlayerExec(   cmAudioSegPlayer* p, cmSample_t** outChPtr, unsigned chCnt, unsigned outSmpCnt );


  //------------------------------------------------------------------------------------------------------------
  /*
  cmReal_t (*cmCluster0DistFunc_t)( void* userPtr, const cmReal_t* v0, const cmReal_t* v1, unsigned binCnt );

  typedef struct
  {
    cmObj                     obj;
    unsigned                  flags;
    unsigned                  stateCnt;
    unsigned                  binCnt;
    cmReal_t*                 oM;          // oM[ binCnt,   stateCnt ]
    unsigned*                 tM;          // tM[ stateCnt, stateCnt ]
    cmReal_t*                  dV;          // dV[ state
    cmCluster0DistFunc_t      distFunc;
    void*                     distUserPtr;
    unsigned                  cnt;
  } cmCluster0;
  
  enum
  {
    kCalcTransFl = 0x01,
    kCalcDurFl   = 0x02
  };

  cmCluster0* cmCluster0Alloc( cmCtx* ctx, cmCluster0* ap, unsigned stateCnt, unsigned binCnt, unsigned flags, cmCluster0DistFunc_t distFunc, void* dstUserPtr ); 
  cmRC_t           cmCluster0Free(  cmCluster0** pp );
  cmRC_t           cmCluster0Init(  cmCluster0* p,  unsigned stateCnt, unsigned binCnt, unsigned flags, cmCluster0DistFunc_t distFunc, void* dstUserPtr ); 
  cmRC_t           cmCluster0Final( cmCluster0* p );
  cmRC_t           cmCluster0Exec(  cmCluster0* p, const cmReal_t* v, unsigned vn );
  */

  //------------------------------------------------------------------------------------------------------------
  typedef struct
  {
    cmObj     obj;

    unsigned  n;
    unsigned  m;
    unsigned  r;
    unsigned  maxIterCnt;
    unsigned  convergeCnt;
    
    cmReal_t* V;  // V[n,m]
    cmReal_t* W;  // W[n,r]
    cmReal_t* H;  // H[r,m]

    cmReal_t* tr;
    cmReal_t* x;
    cmReal_t* t0nm;
    cmReal_t* t1nm;
    cmReal_t* Wt;
    cmReal_t* Ht;
    cmReal_t* trm;
    unsigned* crm;
    cmReal_t* tnr;
    unsigned* c0;
    unsigned* c1;
    unsigned* c0m;
    unsigned* c1m;
    unsigned* idxV;
    
  } cmNmf_t;


  cmNmf_t* cmNmfAlloc( cmCtx* ctx, cmNmf_t* ap, unsigned n, unsigned m, unsigned r, unsigned maxIterCnt, unsigned convergeCnt );
  cmRC_t   cmNmfFree( cmNmf_t** pp );
  cmRC_t   cmNmfInit( cmNmf_t* p,  unsigned n, unsigned m, unsigned r, unsigned maxIterCnt, unsigned convergeCnt );
  cmRC_t   cmNmfFinal(cmNmf_t* p );

  // 
  cmRC_t   cmNmfExec( cmNmf_t* p, const cmReal_t* v, unsigned cn );


  //------------------------------------------------------------------------------------------------------------
  // cmVectArray buffers row vectors of arbitrary lenght in  memory.
  // The buffers may then be access using the cmVectArrayGetXXX() functions.
  // The entire contents of the file may be written to a file using atVectArrayWrite().
  // The file may then be read in back into memory using cmVectArrayAllocFromFile()
  // or in octave via readVectArray.m.
  // A rectantular matrix in memory may be written to a VectArray file in one operation
  // via the function cmVectArrayWriteMatrixXXX(). 

  typedef struct cmVectArrayVect_str
  {
    unsigned n;  // length of this vector in values (not bytes)

    union           
    {
      char*        v;  // raw memory vector pointer
      double*     dV;  // dV[n] vector of doubles  
      float*      fV;  // fV[n] vecotr of floats
      cmSample_t* sV;  // sV[n] vector of cmSample_t
    } u;

    struct cmVectArrayVect_str* link; // link to next element record

  } cmVectArrayVect_t;

  enum
  {
    kFloatVaFl  = 0x01,
    kDoubleVaFl = 0x02,
    kSampleVaFl = 0x04,
    kRealVaFl   = 0x08,
    kIntVaFl    = 0x02,  // int and uint is converted to double
    kUIntVaFl   = 0x02   // 
  };

  typedef struct
  {
    cmObj              obj;
    cmVectArrayVect_t* bp;         // first list element
    cmVectArrayVect_t* ep;         // last list element
    unsigned           vectCnt;     // count of elements in linked list
    unsigned           flags;      // data vector type (See: kFloatVaFl, kDoubleVaFl, ... )
    unsigned           typeByteCnt; // size of a single data vector value (e.g. 4=float 8=double)
    unsigned           maxEleCnt; // length of the longest data vector
    double*            tempV;
    cmVectArrayVect_t* cur;
  } cmVectArray_t;

  // Flags must be set to one of the kXXXVAFl flag values.
  cmVectArray_t* cmVectArrayAlloc( cmCtx* ctx, unsigned flags );
  cmVectArray_t* cmVectArrayAllocFromFile(cmCtx* ctx, const char* fn );

  cmRC_t cmVectArrayFree(    cmVectArray_t** pp );

  // Release all the stored vectors but do not release the object.
  cmRC_t cmVectArrayClear(   cmVectArray_t* p );

  // Return the count of vectors contained in the vector array.
  cmRC_t cmVectArrayCount(   const cmVectArray_t* p );

  // Store a new vector by appending it to the end of the internal vector list.
  cmRC_t cmVectArrayAppendS( cmVectArray_t* p, const cmSample_t* v, unsigned vn );
  cmRC_t cmVectArrayAppendR( cmVectArray_t* p, const cmReal_t* v,   unsigned vn );
  cmRC_t cmVectArrayAppendF( cmVectArray_t* p, const float* v,      unsigned vn );
  cmRC_t cmVectArrayAppendD( cmVectArray_t* p, const double* v,     unsigned vn );
  cmRC_t cmVectArrayAppendI( cmVectArray_t* p, const int* v,        unsigned vn );
  cmRC_t cmVectArrayAppendU( cmVectArray_t* p, const unsigned* v,   unsigned vn );

  // Write a vector array in a format that can be read by readVectArray.m.
  cmRC_t cmVectArrayWrite(   cmVectArray_t* p, const char* fn );

  typedef cmRC_t (*cmVectArrayForEachFuncS_t)( void* arg, unsigned idx, const cmSample_t* xV, unsigned xN );
  unsigned cmVectArrayForEachS( cmVectArray_t* p, unsigned idx, unsigned cnt, cmVectArrayForEachFuncS_t func, void* arg ); 

  cmRC_t cmVectArrayWriteVectorS( cmCtx* ctx, const char* fn, const cmSample_t* v, unsigned  vn );
  cmRC_t cmVectArrayWriteVectorI( cmCtx* ctx, const char* fn, const int* v,        unsigned  vn );

  // Write the column-major matrix m[rn,cn] to the file 'fn'. Note that the matrix is transposed as it is 
  // written and therefore will be read back as a 'cn' by 'rn' matrix.
  cmRC_t cmVectArrayWriteMatrixS( cmCtx* ctx, const char* fn, const cmSample_t* m, unsigned  rn, unsigned cn );
  cmRC_t cmVectArrayWriteMatrixR( cmCtx* ctx, const char* fn, const cmReal_t*   m, unsigned  rn, unsigned cn );
  cmRC_t cmVectArrayWriteMatrixI( cmCtx* ctx, const char* fn, const int*        m, unsigned  rn, unsigned cn );

  cmRC_t   cmVectArrayRewind(   cmVectArray_t* p );
  cmRC_t   cmVectArrayAdvance(  cmVectArray_t* p, unsigned n );
  bool     cmVectArrayIsEOL(    const cmVectArray_t* p );
  unsigned cmVectArrayEleCount( const cmVectArray_t* p );
  cmRC_t   cmVectArrayGetF(     cmVectArray_t* p, float* v,      unsigned* vnRef );
  cmRC_t   cmVectArrayGetD(     cmVectArray_t* p, double* v,     unsigned* vnRef );
  cmRC_t   cmVectArrayGetI(     cmVectArray_t* p, int* v,        unsigned* vnRef );
  cmRC_t   cmVectArrayGetU(     cmVectArray_t* p, unsigned* v,   unsigned* vnRef );

  // If a vector array is composed of repeating blocks of 'groupCnt' sub-vectors 
  // where the concatenated ith sub-vectors in each group form a single super-vector then
  // this function will return the super-vector.  Use cmMemFree(*vRef) to release
  // the returned super-vector.
  cmRC_t   cmVectArrayFormVectF( cmVectArray_t* p, unsigned groupIdx, unsigned groupCnt, float** vRef, unsigned* vnRef );

  cmRC_t   cmVectArrayFormVectColF( cmVectArray_t* p, unsigned groupIdx, unsigned groupCnt, unsigned colIdx, float**    vRef, unsigned* vnRef );
  cmRC_t   cmVectArrayFormVectColU( cmVectArray_t* p, unsigned groupIdx, unsigned groupCnt, unsigned colIdx, unsigned** vRef, unsigned* vnRef );
  cmRC_t   cmVectArrayTest( cmCtx* ctx, const char* fn );  


#if CM_FLOAT_SMP == 1
#define cmVectArrayGetS cmVectArrayGetF
#define cmVectArrayFormVectS cmVectArrayFormVectF
#else
#define cmVectArrayGetS cmVectArrayGetD
#define cmVectArrayFormVectS cmVectArrayFormVectD
#endif

  //-----------------------------------------------------------------------------------------------------------------------
  // Spectral whitening filter.
  // Based on: Klapuri, A., 2006: Multiple fundamental frequency estimation by summing
  //  harmonic amplitudes.

  typedef struct
  {
    cmObj     obj;
    unsigned  binCnt;  // 
    cmReal_t  binHz;   //
    unsigned  bandCnt; //
    cmReal_t  coeff;   //
    cmReal_t* whiV;    // whiV[bandCnt+2] - fractional bin index of each center frequency 
    cmReal_t* whM;     // whM[binCnt,bandCnt]
    cmReal_t* iV;      // iV[ binCnt ] - working memory
  } cmWhFilt;

  cmWhFilt* cmWhFiltAlloc( cmCtx* c, cmWhFilt* p, unsigned binCnt, cmReal_t binHz, cmReal_t coeff, cmReal_t maxHz );
  cmRC_t    cmWhFiltFree( cmWhFilt** pp );
  cmRC_t    cmWhFiltInit( cmWhFilt* p, unsigned binCnt, cmReal_t binHz, cmReal_t coeff, cmReal_t maxHz );
  cmRC_t    cmWhFiltFinal( cmWhFilt* p );
  cmRC_t    cmWhFiltExec( cmWhFilt* p, const cmReal_t* xV, cmReal_t* yV, unsigned xyN );

  //-----------------------------------------------------------------------------------------------------------------------
  typedef enum
  {
    kNoStateFrqTrkId,
    kAtkFrqTrkId,
    kSusFrqTrkId,
    kDcyFrqTrkId
  } cmFrqTrkAttenStateId_t;

  typedef struct
  {
    double      srate;          // system sample rate
    unsigned    chCnt;          // tracking channel count
    unsigned    binCnt;         // count of spectrum elements passed in each call to cmFrqTrkExec()
    unsigned    hopSmpCnt;      // phase vocoder hop count in samples
    cmReal_t    stRange;        // maximum allowable semi-tones between a tracker and a peak
    cmReal_t    wndSecs;        // duration of the  
    cmReal_t    minTrkSec;      // minimum track length before track is considered stable
    cmReal_t    maxTrkDeadSec;  // maximum length of time a tracker may fail to connect to a peak before being declared disconnected.
    cmReal_t    pkThreshDb;     // minimum amplitide in Decibels of a selected spectral peak.
    cmReal_t    pkAtkThreshDb;  // minimum amplitude in Decibels for the first frame of a new track.
    cmReal_t    pkMaxHz;        // maximum frequency to track
    cmReal_t    whFiltCoeff;

    cmReal_t    attenThresh;
    cmReal_t    attenGain; 
    cmReal_t    attenAtkSec;   

    const char* logFn;          // log file name or NULL if no file is to be written
    const char* levelFn;        // level file name or NULL if no file is to be written
    const char* specFn;         // spectrum file name or NULL if no file is to be written
    const char* attenFn;

  } cmFrqTrkArgs_t;

  typedef struct
  {
    bool     activeFl; 
    unsigned id;
    unsigned tN;   // age of this track in frames
    unsigned dN;   // count of consecutive times this ch has not connected 
    cmReal_t hz;   // current center frequency
    cmReal_t db;   // current magnitude

    cmReal_t* dbV; // dbV[]
    cmReal_t* hzV; // hzV[]  
    unsigned  si;
    unsigned  sn;
    
    cmReal_t db_mean;
    cmReal_t db_std;
    cmReal_t hz_mean;
    cmReal_t hz_std;

    cmReal_t score;

    cmFrqTrkAttenStateId_t state;
    int      attenPhsIdx;
    cmReal_t attenGain;
  } cmFrqTrkCh_t;

  struct cmBinMtxFile_str;

  typedef struct cmFrqTrk_str
  {
    cmObj         obj;
    cmFrqTrkArgs_t  a;
    cmFrqTrkCh_t*  ch;  // ch[ a.chCnt ]
    unsigned       hN;  // count of magnitude buffer frames 
    unsigned       sN;  // count of frames in channel statistics buffers
    unsigned       bN;  // count of bins in peak matrices
    cmReal_t*     dbM;  // dbM[ hN, bN ]
    unsigned       hi;  // next row of dbM to fill
    unsigned       fN;  // total count of frames processed.
    cmReal_t      binHz;

    cmReal_t*     dbV;
    unsigned*     pkiV;
    unsigned      deadN_max; // max. count of hops a tracker may fail to connect before being set to inactive
    unsigned      minTrkN;   // minimum track length in hops
    unsigned      nextTrkId;

    unsigned      newTrkCnt;
    unsigned      curTrkCnt;
    unsigned      deadTrkCnt;

    cmReal_t*     aV;
    int           attenPhsMax;

    cmWhFilt*      wf;

    cmVectArray_t* logVa;
    cmVectArray_t* levelVa;
    cmVectArray_t* specVa;
    cmVectArray_t* attenVa;

    cmChar_t*      logFn;
    cmChar_t*      levelFn;
    cmChar_t*      specFn;
    cmChar_t*      attenFn;

  } cmFrqTrk;

  //
  // 1. Calculate the mean spectral magnitude profile over the last hN frames.
  // 2. Locate the peaks in the profile.
  // 3. Allow each active tracker to select the closest peak to extend its life.
  //     a) The distance between the trackers current location and a given
  //        peak is measured based on magnitude and frequency over time.
  //     b) There is a frequency range limit outside of which a given track-peak
  //        connection may not go.
  //     c) There is an amplitude threshold below which a track may not fall.

  cmFrqTrk* cmFrqTrkAlloc( cmCtx* c, cmFrqTrk* p, const cmFrqTrkArgs_t* a );
  cmRC_t    cmFrqTrkFree( cmFrqTrk** pp );
  cmRC_t    cmFrqTrkInit( cmFrqTrk* p, const cmFrqTrkArgs_t* a );
  cmRC_t    cmFrqTrkFinal( cmFrqTrk* p );
  cmRC_t    cmFrqTrkExec( cmFrqTrk* p, const cmReal_t* magV, const cmReal_t* phsV, const cmReal_t* hzV );
  void      cmFrqTrkPrint( cmFrqTrk* p );



  //------------------------------------------------------------------------------------------------------------

  enum
  {
    kBypassModeSdId,  // 0 - no effect
    kBasicModeSdId,   // 1 - fixed thresh
    kSpecCentSdId,    // 2 - thresh = max magn - (offset * spec_cent)
    kAmpEnvSdId,      // 3 - thresh = max magn - offset
    kBumpSdId,
    kModeSdCnt
  };

  typedef struct
  {
    cmObj    obj;
    double   srate;
    unsigned wndSmpCnt;
    unsigned hopFcmt;
    unsigned hopSmpCnt;
    unsigned procSmpCnt;

    cmPvAnl*  pva;
    cmPvSyn*  pvs;

    cmFrqTrk* ft;
    
    unsigned mode;
    double   thresh;

    double   uprSlope;
    double   lwrSlope;
    double   offset;
    bool     invertFl;

    double   spcBwHz;   // spectral centroid bandwidth in Hz
    double   spcSmArg;  // spectral centroid smoothing 
    double   spcMin;
    double   spcMax;
    unsigned spcBinCnt; // count of bins used in the spectral centroid 
    cmReal_t* hzV;    // hzV[spcBinCnt];
    cmReal_t  spc;

    unsigned  spcCnt;
    cmReal_t  spcSum;
    cmReal_t  spcSqSum;
   
    cmReal_t  aeSmMax;   // smoothed max bin magn - used by spectral centroid
    cmReal_t  aeSmOffs;  // smoothed offset 
    
    cmReal_t  ae;
    cmReal_t  aeMin;
    cmReal_t  aeMax;
    cmReal_t  aeUnit;

  } cmSpecDist_t;

  cmSpecDist_t*     cmSpecDistAlloc( cmCtx* ctx,cmSpecDist_t* ap, unsigned procSmpCnt, double srate, unsigned wndSmpCnt, unsigned hopFcmt, unsigned olaWndTypeId  ); 
  cmRC_t            cmSpecDistFree( cmSpecDist_t** pp );
  cmRC_t            cmSpecDistInit( cmSpecDist_t* p, unsigned procSmpCnt, double srate, unsigned wndSmpCnt, unsigned hopFcmt, unsigned olaWndTypeId  );
  cmRC_t            cmSpecDistFinal(cmSpecDist_t* p );
  cmRC_t            cmSpecDistExec( cmSpecDist_t* p, const cmSample_t* sp, unsigned sn );
  const cmSample_t* cmSpecDistOut(  cmSpecDist_t* p );


  //------------------------------------------------------------------------------------------------------------
  // Write a binary matrix file in the format acceppted by the octave function readBinFile.m

  typedef struct cmBinMtxFile_str
  {
    cmObj      obj;
    cmFileH_t  fh;
    unsigned   rowCnt;
    unsigned   maxRowEleCnt;
    unsigned   eleByteCnt;  
  } cmBinMtxFile_t;

  cmBinMtxFile_t* cmBinMtxFileAlloc( cmCtx* ctx, cmBinMtxFile_t* ap, const cmChar_t* fn );
  cmRC_t       cmBinMtxFileFree(  cmBinMtxFile_t** pp );
  cmRC_t       cmBinMtxFileInit(  cmBinMtxFile_t* p, const cmChar_t* fn );
  cmRC_t       cmBinMtxFileFinal( cmBinMtxFile_t* p );

  // Write one row of 'xn' columns to the matrix file.
  cmRC_t       cmBinMtxFileExecS( cmBinMtxFile_t* p, const cmSample_t* x, unsigned xn );
  cmRC_t       cmBinMtxFileExecR( cmBinMtxFile_t* p, const cmReal_t*   x, unsigned xn );

  bool         cmBinMtxFileIsValid( cmBinMtxFile_t* p );

  // Write a binary matrix file. 
  // The matrix data is provided as sp[rowCnt,colCnt] or rp[rowCnt,colCnt].
  // The matrix is assumed to be in column major order (like all matrices in the cm library)
  // Either 'sp' or 'rp' must be given but not both.
  // 'ctx' is optional and defaults to NULL.
  // If 'ctx' is not provided then 'rpt' must be provided.
  // If 'ctx' is provided then 'rpt' is not used.
  // See cmAudioFileReadWriteTest() in cmProcTest.c for an example usage.
  cmRC_t cmBinMtxFileWrite( const cmChar_t* fn, unsigned rowCnt, unsigned colCnt, const cmSample_t* sp, const cmReal_t* rp, cmCtx* ctx, cmRpt_t* rpt );
  
  // Return the matrix file geometry.
  // rowCntPtr,colCntPtr and eleByteCntPtr are optional
  cmRC_t cmBinMtxFileSize( cmCtx_t* ctx, const cmChar_t* fn, unsigned* rowCntPtr, unsigned* colCntPtr, unsigned* eleByteCntPtr );

  // Fill buf[rowCnt*colCnt*byteEleCnt] buffer from the binary matrix file 'fn'.
  // rowCnt,colCnt,eleByteCnt must be exactly the same as the actual file.
  // Use cmBinMtxFileSize() to determine the buffer size prior to calling this function.
  // colCntV[colCnt] is optional.
  cmRC_t cmBinMtxFileRead( cmCtx_t* ctx, const cmChar_t* fn, unsigned rowCnt, unsigned colCnt, unsigned eleByteCnt, void* buf, unsigned* colCntV );




#ifdef __cplusplus
}
#endif


#endif
