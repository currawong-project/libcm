#ifndef cmProc_h
#define cmProc_h

#ifdef __cplusplus
extern "C" {
#endif

  //------------------------------------------------------------------------------------------------------------
  typedef struct
  {
    cmObj             obj;
    cmAudioFileH_t    h;        // audio file handle
    cmAudioFileInfo_t info;     // audio file info record
    unsigned          chIdx;
    cmSample_t*       outV;     // buffer of audio from last read
    unsigned          outN;     // length of outV in samples
    cmChar_t*         fn;       // name of audio file
    unsigned          lastReadFrmCnt; // count of samples actually read on last read
    bool              eofFl;
    unsigned          begFrmIdx;
    unsigned          endFrmIdx;
    unsigned          curFrmIdx; // frame index of the next frame to read
    cmMtxFile*        mfp;
  } cmAudioFileRd;

  /// set p to NULL to dynamically allocate the object
  /// fn and chIdx are optional - set fn to NULL to allocate the reader without opening a file.
  /// If fn is valid then chIdx must also be valid.
  /// Set 'endSmpIdx' to cmInvalidIdx to not limit the range of samples returned.
  cmAudioFileRd*     cmAudioFileRdAlloc( cmCtx* c, cmAudioFileRd* p, unsigned procSmpCnt, const char* fn, unsigned chIdx, unsigned begSmpIdx, unsigned endSmpIdx );
  cmRC_t             cmAudioFileRdFree(  cmAudioFileRd** p );
  cmRC_t             cmAudioFileRdOpen(  cmAudioFileRd* p, unsigned procSmpCnt,  const cmChar_t* fn, unsigned chIdx, unsigned begSmpIdx, unsigned endSmpIdx ); 
  cmRC_t             cmAudioFileRdClose( cmAudioFileRd* p );  


  /// Returns cmEofRC if the end of file is encountered.
  cmRC_t             cmAudioFileRdRead(  cmAudioFileRd* p );
  cmRC_t             cmAudioFileRdSeek(  cmAudioFileRd* p, unsigned frmIdx );

  /// Find the overall minimum, maximum, and mean sample values without changing the current file location.
  cmRC_t             cmAudioFileRdMinMaxMean( cmAudioFileRd* p, unsigned chIdx, cmSample_t* minPtr, cmSample_t* maxPtr, cmSample_t* meanPtr );


  //------------------------------------------------------------------------------------------------------------
  /// The buffer is intended to synchronize sample block rates between processes and to provide an overlapped 
  /// input buffer.
  typedef struct 
  {
    cmObj       obj;
    unsigned    bufSmpCnt;  // wndSmpCnt + hopSmpCnt
    cmSample_t* bufV;       // bufV[bufSmpCnt] all other pointers use this memory
    cmSample_t* outV;       // output window outV[ outN ]
    unsigned    outN;       // outN == wndSmpCnt
    unsigned    procSmpCnt; // input sample count
    unsigned    wndSmpCnt;  // output sample count
    unsigned    hopSmpCnt;  // count of samples to shift the buffer by on each call to cmShiftExec()
    cmSample_t* inPtr;      // ptr to location in outV[] to recv next sample
    bool        fl;         // reflects the last value returned by cmShiftBufExec().
  } cmShiftBuf;



  /// Set p to NULL to dynamically allocate the object.  hopSmpCnt must be  <= wndSmpCnt.
  cmShiftBuf*       cmShiftBufAlloc( cmCtx* c, cmShiftBuf* p, unsigned procSmpCnt, unsigned wndSmpCnt, unsigned hopSmpCnt );
  cmRC_t            cmShiftBufFree(  cmShiftBuf** p );
  cmRC_t            cmShiftBufInit(  cmShiftBuf* p, unsigned procSmpCnt, unsigned wndSmpCnt, unsigned hopSmpCnt );
  cmRC_t            cmShiftBufFinal( cmShiftBuf* p );

  /// Returns true if a new hop is ready to be read otherwise returns false.
  /// In general cmShiftBufExec() should be called in a loop until it returns false. 
  /// Note that 'sp' and 'sn' are ignored except for the first call after the function returns false.
  /// This means that when called in a loop 'sp' and 'sn' are only used on the first time through the loop.
  /// When procSmpCnt is less than hopSmpCnt the loop will only execute when at least wndSmpCnt 
  /// new samples have been buffered.
  /// When procSmpCnt is greater than hopSmpCnt the loop will execute multiple times until less 
  //  than wndSmpCnt new samples are available.
  /// Note that 'sn' must always be less than or equal to procSmpCnt.
  ///
  /// Example:
  /// while( fill(sp,sn) )                      // fill sp[] with sn samples
  /// {
  ///    // shift by hopSmpCnt samples on all passes - insert new samples on first pass
  ///    while( cmShiftBufExec(p,sp,sn) )       
  ///       proc(p->outV,p->outN);              // process p->outV[wndSmpCnt]
  /// }
  bool              cmShiftBufExec(  cmShiftBuf* p, const cmSample_t* sp, unsigned sn );

  void              cmShiftBufTest( cmCtx* c );


  //------------------------------------------------------------------------------------------------------------
  /*
  typedef struct
  {
    cmComplexS_t*   complexV;
    cmSample_t*     outV;
    cmFftPlanS_t     plan;
  } cmIFftObjS;

  typedef struct
  {
    cmComplexR_t*   complexV;
    cmReal_t*       outV;
    cmFftPlanR_t    plan;
  } cmIFftObjR;

  typedef struct
  {
    cmObj           obj;
    unsigned        binCnt;
    unsigned        outN;

    union 
    {
      cmIFftObjS sr;
      cmIFftObjR rr;
    }u;
  
  } cmIFft;

  cmIFft*  cmIFftAllocS( cmCtx* c, cmIFft* p, unsigned binCnt );
  cmIFft*  cmIFftAllocR( cmCtx* c, cmIFft* p, unsigned binCnt );

  cmRC_t   cmIFftFreeS(  cmIFft** pp );
  cmRC_t   cmIFftFreeR(  cmIFft** pp );

  cmRC_t   cmIFftInitS(  cmIFft* p, unsigned binCnt );
  cmRC_t   cmIFftInitR(  cmIFft* p, unsigned binCnt );

  cmRC_t   cmIFftFinalS( cmIFft* p );
  cmRC_t   cmIFftFinalR( cmIFft* p );

  // x must contain 'binCnt' elements.
  cmRC_t   cmIFftExecS(     cmIFft* p, cmComplexS_t* x );
  cmRC_t   cmIFftExecR(     cmIFft* p, cmComplexR_t* x );

  cmRC_t   cmIFftExecPolarS( cmIFft* p, const cmReal_t* magV, const cmReal_t* phsV ); 
  cmRC_t   cmIFftExecPolarR( cmIFft* p, const cmReal_t* magV, const cmReal_t* phsV ); 

  cmRC_t   cmIFftExecRectS(  cmIFft* p, const cmReal_t* rV,   const cmReal_t* iV );
  cmRC_t   cmIFftExecPolarR( cmIFft* p, const cmReal_t* magV, const cmReal_t* phsV ); 

  void cmIFftTest( cmRpt_t* rptFuncPtr );
  */
  //------------------------------------------------------------------------------------------------------------

  enum
  {
    kInvalidWndId   = 0x000,
    kHannWndId      = 0x001,
    kHammingWndId   = 0x002,
    kTriangleWndId  = 0x004,
    kKaiserWndId    = 0x008,
    kHannMatlabWndId= 0x010,
    kUnityWndId     = 0x020,
  
    kWndIdMask       = 0x0ff,      

    kNormByLengthWndFl = 0x100,  // mult by 1/wndSmpCnt 
    kNormBySumWndFl    = 0x200   // mult by wndSmpCnt/sum(wndV)
  };

  typedef struct
  {
    cmObj       obj;
    unsigned    wndId;
    unsigned    flags;
    cmSample_t* wndV;
    cmSample_t* outV;
    unsigned    outN;       // same as wndSmpCnt
    double      kslRejectDb;
    cmMtxFile*  mfp;
  } cmWndFunc;

  /// Set p to NULL to dynamically allocate the object
  /// if wndId is set to a valid value this function will internally call cmWndFuncInit()
  cmWndFunc*  cmWndFuncAlloc( cmCtx* c, cmWndFunc* p, unsigned wndId, unsigned wndSmpCnt, double kaierSideLobeRejectDb );
  cmRC_t      cmWndFuncFree(  cmWndFunc** pp );
  cmRC_t      cmWndFuncInit(  cmWndFunc* p, unsigned wndId, unsigned wndSmpCnt, double kaiserSideLobeRejectDb );
  cmRC_t      cmWndFuncFinal( cmWndFunc* p );
  cmRC_t      cmWndFuncExec(  cmWndFunc* p, const cmSample_t* sp, unsigned sn );


  void cmWndFuncTest( cmRpt_t* rpt, cmLHeapH_t lhH, cmSymTblH_t stH );

  //------------------------------------------------------------------------------------------------------------
  /// Spectral frame delay. A circular buffer for spectral (or other fixed length) vectors.
  typedef struct
  {
    cmObj       obj;
    cmSample_t* bufPtr;
    unsigned    maxDelayCnt;
    int         inIdx;
    unsigned    outN;  // outN == binCnt
  } cmSpecDelay;


  /// Set p to NULL to dynamically allocate the object.
  /// Allocate a spectral frame delay capable of delaying for 'maxDelayCnt' hops and 
  /// where each vector contains 'binCnt' elements.
  cmSpecDelay*      cmSpecDelayAlloc( cmCtx* c, cmSpecDelay* p, unsigned maxDelayCnt, unsigned binCnt );
  cmRC_t            cmSpecDelayFree(  cmSpecDelay** p );

  cmRC_t            cmSpecDelayInit( cmSpecDelay* p, unsigned maxDelayCnt, unsigned binCnt );
  cmRC_t            cmSpecDelayFinal(cmSpecDelay* p );

  /// Give an input vector to the delay. 'sn' must <= binCnt
  cmRC_t            cmSpecDelayExec(  cmSpecDelay* p, const cmSample_t* sp, unsigned sn );

  /// Get a pointer to a delayed vector. 'delayCnt' indicates the length of the delay in hops.
  /// (e.g. 1 is the previous hop, 2 is two hops previous, ... )
  const cmSample_t* cmSpecDelayOutPtr(cmSpecDelay* p, unsigned delayCnt );


  //------------------------------------------------------------------------------------------------------------
  typedef struct cmFilter_str
  {
    cmObj     obj;

    cmReal_t* a;    // feedback coeff's
    int       an;   // count of fb coeff's
    cmReal_t* b;    // feedforward coeff's
    int       bn;   // count of ff coeffs'
  
    cmReal_t* d;    // delay 
    int       di;   //  
    int       cn;   // length of delay
    cmReal_t  b0;   // 1st feedforward coeff 

    cmSample_t*  outSmpV;  // signal output vector
    cmReal_t*    outRealV; 
    unsigned     outN; // length of outV (procSmpCnt)
  } cmFilter;

  // d[dn] is the initial value of the delay line where dn = max(an,bn)-1.  
  // Set d to NULL to intialize the delays to 0.
  cmFilter* cmFilterAlloc(      cmCtx* c, cmFilter* p, const cmReal_t* b, unsigned bn, const cmReal_t* a, unsigned an, unsigned procSmpCnt, const cmReal_t* d );
  cmFilter* cmFilterAllocEllip( cmCtx* c, cmFilter* p, cmReal_t srate, cmReal_t passHz, cmReal_t stopHz, cmReal_t passDb, cmReal_t stopDb, unsigned procSmpCnt, const cmReal_t* d );
  cmRC_t    cmFilterFree(      cmFilter** pp );
  cmRC_t    cmFilterInit(      cmFilter* p, const cmReal_t* b, unsigned bn, const cmReal_t* a, unsigned an, unsigned procSmpCnt, const cmReal_t* d );
  cmRC_t    cmFilterInitEllip( cmFilter* p, cmReal_t srate, cmReal_t passHz, cmReal_t stopHz, cmReal_t passDb, cmReal_t stopDb, unsigned procSmpCnt, const cmReal_t* d );
  cmRC_t    cmFilterFinal(     cmFilter* p );

  // If y==NULL or yn==0 then the output is sent to p->outV[p->outN].
  // This function can safely filter a signal in plcme therefore it is allowable for x[] and y[] to refer to the same memory.
  // If x[] overlaps y[] then y must be <= x.
  cmRC_t    cmFilterExecS(  cmFilter* p, const cmSample_t* x, unsigned xn, cmSample_t* y, unsigned yn );
  cmRC_t    cmFilterExecR(  cmFilter* p, const cmReal_t* x,   unsigned xn, cmReal_t*   y, unsigned yn );

  cmRC_t    cmFilterSignal( cmCtx* c, const cmReal_t b[], unsigned bn, const cmReal_t a[], unsigned an, const cmSample_t* x, unsigned xn, cmSample_t* y, unsigned yn );

  // Perform forward-reverse filtering.
  cmRC_t    cmFilterFilterS(cmCtx* c, const cmReal_t bb[], unsigned bn, const cmReal_t aa[], unsigned an, const cmSample_t* x, unsigned xn, cmSample_t* y, unsigned yn );
  cmRC_t    cmFilterFilterR(cmCtx* c, const cmReal_t bb[], unsigned bn, const cmReal_t aa[], unsigned an, const cmReal_t* x, unsigned xn, cmReal_t* y, unsigned yn );

  void      cmFilterTest( cmRpt_t* rpt, cmLHeapH_t lhH, cmSymTblH_t stH );
  void      cmFilterFilterTest( cmRpt_t* rpt, cmLHeapH_t lhH, cmSymTblH_t stH );


  //------------------------------------------------------------------------------------------------------------
  typedef struct
  {
    cmObj        obj;
    cmSpecDelay  phsDelay;
    cmSpecDelay  magDelay;
    unsigned     binCnt;
    cmSample_t   out;
    //cmMtxFile*   mfp;
    //unsigned     cdfSpRegId;
  } cmComplexDetect;

  /// Set p to NULL to dynamically allocate the object.
  cmComplexDetect* cmComplexDetectAlloc(cmCtx* c, cmComplexDetect* p, unsigned binCnt );
  cmRC_t           cmComplexDetectFree( cmComplexDetect** pp);
  cmRC_t           cmComplexDetectInit( cmComplexDetect* p, unsigned binCnt );
  cmRC_t           cmComplexDetectFinal(cmComplexDetect* p);
  cmRC_t           cmComplexDetectExec( cmComplexDetect* p, const cmSample_t* magV, const cmSample_t* phsV, unsigned binCnt  );

  //------------------------------------------------------------------------------------------------------------
  typedef struct
  {
    cmObj        obj;
    double       threshold;
    unsigned     medSmpCnt;
    unsigned     frmCnt;         // expected number of frames to store
    unsigned     dfi;
    cmSample_t*  df;
    cmSample_t*  fdf;
    cmSample_t   onrate;
    //cmMtxFile*   mfp;
  } cmComplexOnset;

  cmComplexOnset* cmComplexOnsetAlloc( cmCtx* c, cmComplexOnset* p, unsigned procSmpCnt, double srate, unsigned medFiltWndSmpCnt, double threshold, unsigned frameCnt );
  cmRC_t          cmComplexOnsetFree(  cmComplexOnset** pp);
  cmRC_t          cmComplexOnsetInit(  cmComplexOnset* p, unsigned procSmpCnt, double srate, unsigned medFiltWndSmpCnt, double threshold, unsigned frameCnt );
  cmRC_t          cmComplexOnsetFinal( cmComplexOnset* p);
  cmRC_t          cmComplexOnsetExec(  cmComplexOnset* p, cmSample_t cdf );
  cmRC_t          cmComplexOnsetCalc(  cmComplexOnset* p );

  //------------------------------------------------------------------------------------------------------------
  typedef struct
  {
    cmObj       obj;
    unsigned    melBandCnt;
    unsigned    dctCoeffCnt;
    unsigned    binCnt;
    cmReal_t*   melM;
    cmReal_t*   dctM;
    cmReal_t*   outV;
    unsigned    outN;        // outN == dctCoeffCnt
    cmMtxFile*  mfp;
    unsigned    mfccSpRegId; // cmStatsProc regId
  } cmMfcc;

  cmMfcc* cmMfccAlloc( cmCtx* c, cmMfcc* p, double srate, unsigned melBandCnt, unsigned dctCoeffCnt, unsigned binCnt );
  cmRC_t  cmMfccFree(  cmMfcc** pp );
  cmRC_t  cmMfccInit(  cmMfcc* p, double srate, unsigned melBandCnt, unsigned dctCoeffCnt, unsigned binCnt );
  cmRC_t  cmMfccFinal( cmMfcc* p );
  cmRC_t  cmMfccExecPower(      cmMfcc* p, const cmReal_t* magPowV, unsigned binCnt );
  cmRC_t  cmMfccExecAmplitude(  cmMfcc* p, const cmReal_t* magAmpV, unsigned binCnt );
  void    cmMfccTest();

  //------------------------------------------------------------------------------------------------------------
  typedef struct
  {
    cmObj       obj;
    cmReal_t*   ttmV;            // Terhardt outer ear filter
    cmReal_t*   sfM;             // Shroeder spreading function
    unsigned*   barkIdxV;        // Bark to bin map
    unsigned*   barkCntV;        //
    cmReal_t*   outV;            // specific loudness in sones 
    unsigned    outN;            // outN == barkBandCnt;
    cmReal_t    overallLoudness; // overall loudness in sones
    unsigned    binCnt;          // expected length of incoming power spectrum
    unsigned    barkBandCnt;     // count of bark bands
    unsigned    flags;           // 
    cmMtxFile*  mfp;
    unsigned    sonesSpRegId;
    unsigned    loudSpRegId;
  } cmSones;

  enum { kDontUseEqlLoudSonesFl=0x00, kUseEqlLoudSonesFl=0x01 };

  cmSones* cmSonesAlloc( cmCtx* c, cmSones*  p, double srate, unsigned barkBandCnt, unsigned binCnt, unsigned flags );
  cmRC_t   cmSonesFree(  cmSones** pp );
  cmRC_t   cmSonesInit(  cmSones*  p, double srate, unsigned barkBandCnt, unsigned binCnt, unsigned flags );
  cmRC_t   cmSonesFinal( cmSones*  p );
  cmRC_t   cmSonesExec(  cmSones*  p, const cmReal_t* magPowV, unsigned binCnt ); 

  void     cmSonesTest();

  //------------------------------------------------------------------------------------------------------------
  typedef struct
  {
    cmObj             obj;
    unsigned          cBufCnt;
    unsigned          cBufCurCnt;
    unsigned          cBufIdx;
    double            cBufSum;
    unsigned          cCntSum;
    cmReal_t*         cBufPtr;
    unsigned*         cCntPtr;
    cmSample_t        offset;
    double            dBref;
    cmSample_t*       outV;
    unsigned          outN;   // (outN == procSmpCnt)
    unsigned          flags;
    cmMtxFile*        mfp;
  } cmAudioOffsetScale;


  /// This processor adds an offset to an audio signal and scales into dB (SPL) using one of two techniques
  /// 1) Measures the effective sound pressure (via RMS) and then scales the signal to the reference dB (SPL) 
  ///    In this case dBref is commonly set to 70. See Timony, 2004, Implementing Loudness Models in Matlab.
  /// 
  /// 2) treats the dBref as the maximum dB (SPL) and scales the signal by this amount without regard
  ///    measured signal level.  In this case dBref is commonly set to 96 (max. dB (SPL) value for 16 bits)
  ///    and rmsWndSecs is ignored.
  ///
  /// Note that setting rmsWndSecs to zero has the effect of using procSmpCnt as the window length.

  enum { kNoAudioScaleFl=0x01, kRmsAudioScaleFl=0x02, kFixedAudioScaleFl=0x04 };

  cmAudioOffsetScale* cmAudioOffsetScaleAlloc( cmCtx* c, cmAudioOffsetScale* p, unsigned procSmpCnt, double srate, cmSample_t offset, double rmsWndSecs, double dBref, unsigned flags );
  cmRC_t        cmAudioOffsetScaleFree(  cmAudioOffsetScale** pp );
  cmRC_t        cmAudioOffsetScaleInit(  cmAudioOffsetScale* p, unsigned procSmpCnt, double srate, cmSample_t offset, double rmsWndSecs, double dBref, unsigned flags );
  cmRC_t        cmAudioOffsetScaleFinal( cmAudioOffsetScale* p );
  cmRC_t        cmAudioOffsetScaleExec(  cmAudioOffsetScale* p, const cmSample_t* sp, unsigned sn );

  //------------------------------------------------------------------------------------------------------------
  typedef struct
  {
    cmObj      obj;

    cmReal_t*  rmsV;
    cmReal_t*  hfcV;
    cmReal_t*  scnV;

    cmReal_t rmsSum;
    cmReal_t hfcSum;
    cmReal_t scnSum;
    cmReal_t ssSum;

    cmReal_t rms;    // RMS output
    cmReal_t hfc;    // high-frequency content output
    cmReal_t sc;     // spectral centroid output
    cmReal_t ss;     // spectral spread output

    unsigned   binCnt;     
    unsigned   flags;
    unsigned   wndFrmCnt;
    unsigned   frameIdx;
    unsigned   frameCnt;
    double     binHz;
    cmMtxFile* mfp;

    unsigned rmsSpRegId;
    unsigned hfcSpRegId;
    unsigned scSpRegId;
    unsigned ssSpRegId;
  } cmSpecMeas;

  /// Set wndFrmCnt to the number of spectral frames to take the measurement over.
  /// Setting wndFrmCnt to 1 has the effect of calculating the value on the current frame only.
  /// Set flags = kWholeSigSpecMeasFl to ignore wndFrmCnt and calculate the result on the entire signal.
  /// In effect this treats the entire signal as the length of the measurement window.
  enum { kWholeSigSpecMeasFl=0x00, kUseWndSpecMeasFl=0x01 };

  cmSpecMeas* cmSpecMeasAlloc( cmCtx* c, cmSpecMeas* p, double srate, unsigned binCnt, unsigned wndFrmCnt, unsigned flags );
  cmRC_t      cmSpecMeasFree(  cmSpecMeas** pp );
  cmRC_t      cmSpecMeasInit(  cmSpecMeas* p, double srate, unsigned binCnt, unsigned wndFrmCnt, unsigned flags );
  cmRC_t      cmSpecMeasFinal( cmSpecMeas* p );
  cmRC_t      cmSpecMeasExec(  cmSpecMeas* p, const cmReal_t* magPowV, unsigned binCnt ); 

  //------------------------------------------------------------------------------------------------------------
  typedef struct
  {
    cmObj       obj;
    cmShiftBuf* sbp;        // shift buffer used internally if procSmpCnt < measSmpCnt
    cmShiftBuf  shiftBuf;
    double      srate;      // 
    cmReal_t    zcr;        // zero crossing rate per second
    cmSample_t  zcrDelay;   // used internally by zero crossing count algorithm
    unsigned    measSmpCnt; // length of measurement window in samples
    unsigned    procSmpCnt; // expected number of samples per call to exec
    unsigned    zcrSpRegId;
    cmMtxFile*  mfp;
  } cmSigMeas;

  // procSmpCnt must be <= measSmpCnt

  cmSigMeas* cmSigMeasAlloc( cmCtx* c, cmSigMeas* p, double srate, unsigned procSmpCnt, unsigned measSmpCnt );
  cmRC_t     cmSigMeasFree(  cmSigMeas** pp );
  cmRC_t     cmSigMeasInit(  cmSigMeas* p, double srate, unsigned procSmpCnt, unsigned measSmpCnt );
  cmRC_t     cmSigMeasFinal( cmSigMeas* p );
  cmRC_t     cmSigMeasExec(  cmSigMeas* p, const cmSample_t* sigV, unsigned smpCnt );

  //------------------------------------------------------------------------------------------------------------
  typedef struct
  {
    cmObj        obj;
    cmFilter     filt;
 
    cmSample_t*  outV;
    unsigned     outN;

    unsigned     upFact;
    unsigned     dnFact;

    unsigned     upi;
    unsigned     dni;

    cmMtxFile*   mfp;

  } cmSRC;

  /// The srate paramater is the sample rate of the source signal provided via cmSRCExec()
  cmSRC* cmSRCAlloc( cmCtx* c, cmSRC* p, double srate, unsigned procSmpCnt, unsigned upFact, unsigned dnFact );
  cmRC_t cmSRCFree(  cmSRC** pp );
  cmRC_t cmSRCInit(  cmSRC* p, double srate, unsigned procSmpCnt, unsigned upFact, unsigned dnFact );
  cmRC_t cmSRCFinal( cmSRC* p );
  cmRC_t cmSRCExec(  cmSRC* p, const cmSample_t* sp, unsigned sn );

  void   cmSRCTest();

  //------------------------------------------------------------------------------------------------------------
  typedef struct
  {
    cmObj           obj;
    cmComplexR_t*   fiV;
    cmComplexR_t*   foV;
    cmComplexR_t*   skM;           // skM[ wndSmpCnt, constQBinCnt ]
    unsigned*       skBegV;        // skBegV[ constQBinCnt ] indexes used to decrease the size of the mtx mult in cmConstQExex()
    unsigned*       skEndV;        // skEndV[ constQBinCnt ]
    unsigned        wndSmpCnt;     // window length of the complex FFT required to feed this transform
    unsigned        constQBinCnt;  // count of bins in the const Q output    
    unsigned        binsPerOctave; // 

    cmComplexR_t*   outV;          // outV[ constQBinCnt ]
    cmReal_t*       magV;          // outV[ constQBinCnt ]

    cmMtxFile*      mfp;

  } cmConstQ;

  cmConstQ* cmConstQAlloc( cmCtx* c, cmConstQ* p, double srate, unsigned minMidiPitch, unsigned maxMidiPitch, unsigned binsPerOctave, double thresh );
  cmRC_t    cmConstQFree(  cmConstQ** pp );
  cmRC_t    cmConstQInit(  cmConstQ* p, double srate, unsigned minMidiPitch, unsigned maxMidiPitch, unsigned binsPerOctave, double thresh );
  cmRC_t    cmConstQFinal( cmConstQ* p );
  cmRC_t    cmConstQExec(  cmConstQ* p, const cmComplexR_t* ftV, unsigned binCnt );


  //------------------------------------------------------------------------------------------------------------
  typedef struct
  {
    cmObj     obj;

    cmReal_t*  hpcpM; // hpcpM[  frameCnt , binsPerOctave ]  - stored hpcp
    cmReal_t*  fhpcpM;// fhpcpM[ binsPerOctave, frameCnt ]   - filtered hpcp (note transposed relative to hpcpA)
    unsigned*  histV; // histM[  binsPerOctave/12 ]
    cmReal_t*  outM;  // outM[   12, frameCnt ];

    unsigned  histN;         // binsPerOctave/12 
    unsigned  binsPerOctave; // const-q bins representing 1 octave
    unsigned  constQBinCnt;  // total count of const-q bins
    unsigned  frameCnt;      // expected count of hpcp vectors to store.
    unsigned  frameIdx;      // next column in hpcpM[] to receive input
    unsigned  cqMinMidiPitch;
    unsigned  medFiltOrder;

    cmReal_t* meanV;  // meanV[12]
    cmReal_t* varV;   // varV[12]

    cmMtxFile* mf0p;   // debug files
    cmMtxFile* mf1p;
    cmMtxFile* mf2p;
  
  } cmHpcp;

  cmHpcp*        cmTunedHpcpAlloc( cmCtx* c, cmHpcp* p, unsigned binsPerOctave, unsigned constQBinCnt, unsigned cqMinMidiPitch, unsigned frameCnt, unsigned medFiltOrder );
  cmRC_t         cmTunedHpcpFree(  cmHpcp** pp );
  cmRC_t         cmTunedHpcpInit(  cmHpcp* p, unsigned binsPerOctave, unsigned constQBinCnt, unsigned cqMinMidiPitch, unsigned frameCnt, unsigned medFiltOrder );
  cmRC_t         cmTunedHpcpFinal( cmHpcp* p );
  cmRC_t         cmTunedHpcpExec(  cmHpcp* p, const cmComplexR_t* constQBinPtr, unsigned constQBinCnt );
  cmRC_t         cmTunedHpcpTuneAndFilter( cmHpcp* p);


  //------------------------------------------------------------------------------------------------------------
  //------------------------------------------------------------------------------------------------------------

  struct cmFftRR_str;
  struct cmIFftRR_str;

  typedef struct
  {
    cmObj     obj;
    struct cmFftRR_str*  fft;
    struct cmIFftRR_str* ifft;

    unsigned frmCnt;        // 512  length of df  
    unsigned maxLagCnt;     // 128  length of longest CMF lag
    unsigned histBinCnt;    // 15   count of histogram elements and rows in H[] 
    unsigned hColCnt;       // 128  count of columns in H[]
  
    cmReal_t* m;            // m[ frmCnt x maxLagCnt ]
    cmReal_t* H;            // histogram transformation mtx
    cmReal_t* df;           // df[  frmCnt ] onset detection function       
    cmReal_t* fdf;          // fdf[ frmCnt ] filtered onset detection function
    unsigned  dfi;          // index next df[] location to receive an incoming value
    cmReal_t* histV;        // histV[ histBinCnt ] histogram output
    cmMtxFile* mfp;
  } cmBeatHist;

  cmBeatHist* cmBeatHistAlloc( cmCtx* c, cmBeatHist* p, unsigned frmCnt );
  cmRC_t      cmBeatHistFree(  cmBeatHist** pp );
  cmRC_t      cmBeatHistInit(  cmBeatHist* p, unsigned frmCnt );
  cmRC_t      cmBeatHistFinal( cmBeatHist* p );
  cmRC_t      cmBeatHistExec(  cmBeatHist* p, cmSample_t df );
  cmRC_t      cmBeatHistCalc(  cmBeatHist* p );

  //------------------------------------------------------------------------------------------------------------
  // Gaussian Mixture Model containing N Gaussian PDF's each of dimension D
  typedef struct
  {
    cmObj     obj;
    unsigned  K;      // count of components
    unsigned  D;      // dimensionality of each component 
    cmReal_t* gV;     // gM[ K ] mixture gain vector
    cmReal_t* uM;     // uM[ D x K ] component mean column vectors
    cmReal_t* sMM;    // sMM[D x D x K ] component covariance matrices - each column is a DxD matrix
    cmReal_t* isMM;   // isMM[D x D x K] inverted covar matrices
    cmReal_t* uMM;    // uMM[ D x D x K] upper triangle factor of chol(sMM)
    cmReal_t* logDetV;// detV[ K ] determinent of covar matrices
    cmReal_t* t;      // t[ D x D  ]scratch matrix used for training
    unsigned  uflags; // user defined flags

  } cmGmm_t;

  enum { cmMdgNoFlags=0x0, cmGmmDiagFl=0x01, cmGmmSkipKmeansFl=0x02 };

  cmGmm_t* cmGmmAlloc( cmCtx* c, cmGmm_t* p, unsigned N, unsigned D, const cmReal_t* gV, const cmReal_t* uM, const cmReal_t* sMM, unsigned flags );
  cmRC_t   cmGmmFree(  cmGmm_t** pp );
  cmRC_t   cmGmmInit(  cmGmm_t* p, unsigned N, unsigned D, const cmReal_t* gV, const cmReal_t* uM, const cmReal_t* sMM, unsigned flags );
  cmRC_t   cmGmmFinal( cmGmm_t* p );

  // Estimate the parameters of the GMM using the training data in xM[p->D,xN].
  // *iterCntPtr on input is the number of iterations with no change in class assignment to signal convergence.
  // *iterCntPtr on output is the total number of interations required to converge.
  cmRC_t   cmGmmTrain( cmGmm_t* p, const cmReal_t* xM, unsigned xN, unsigned* iterCntPtr );

  // Return a pointer to the feature vector at frmIdx containing D elements.
  typedef const cmReal_t* (*cmGmmReadFunc_t)( void* userPtr, unsigned colIdx );
  // Same as cmGmmTrain() but uses a function to access the feature vector.
  // The optional matrix uM[D,K] contains the initial mean values or NULL if not used.
  // The optional flag array roFlV[K] is used to indicate read-only components and is only used
  // when the uM[] arg. is non-NULL. Set roFlV[i] to true to indicate that the mean value supplied by
  // the uM[] arg. should not be alterned by the training process.
  // If 'maxIterCnt' is positive then it is the maximum number of iterations the training process will make
  // otherwise it is ignored.
  cmRC_t   cmGmmTrain2( cmGmm_t* p, cmGmmReadFunc_t readFunc, void* userFuncPtr, unsigned xN, unsigned* iterCntPtr, const cmReal_t* uM, const bool* roFlV, int maxIterCnt );

  // Generate data yN data points from the GMM and store the result in yM[p->D,yN].
  cmRC_t   cmGmmGenerate( cmGmm_t* p, cmReal_t* yM, unsigned yN );

  // Evaluate the probability of each column of xM[p->D,xN] and return the result in y[xN].
  // If yM[xN,K] is non-NULL then the individual component prob. values are returned
  cmRC_t   cmGmmEval(  cmGmm_t* p, const cmReal_t* xM, unsigned xN, cmReal_t* yV, cmReal_t* yM); 

  // Same as cmGmmEval() but uses a a function to access each data vector
  cmRC_t   cmGmmEval2(  cmGmm_t* p, cmGmmReadFunc_t readFunc, void* userFuncPtr, unsigned xN, cmReal_t* yV, cmReal_t* yM);

  // Evaluate each component for a single data point
  // xV[D] - observed data point 
  // yV[K] - output contains the evaluation for each component
  cmRC_t   cmGmmEval3(  cmGmm_t* p,  const cmReal_t* xV,  cmReal_t* yV );

  void     cmGmmPrint( cmGmm_t* p, bool detailsFl );

  void     cmGmmTest( cmRpt_t* rpt, cmLHeapH_t lhH, cmSymTblH_t stH );

  //------------------------------------------------------------------------------------------------------------
  // Continuous Hidden Markov Model
  typedef struct
  {
    cmObj       obj;
    unsigned    N;        // count of states
    unsigned    K;        // count of components per mixture
    unsigned    D;        // dimensionality of the observation data
    cmReal_t*   iV;       // iV[ N ] initial state probability mtx
    cmReal_t*   aM;       // aM[ N x N] transition probability mtx
    cmGmm_t**   bV;       // bV[ N ] observation probability mtx (array of pointers to GMM's) 
    cmReal_t*   bM;       // bM[ N,T]  state-observation probability matrix 

    cmMtxFile* mfp;

  } cmChmm_t;

  // Continuous HMM consisting of stateN states where the observations 
  // associated with each state are generated by a Gaussian mixture PDF.
  // stateN - count of states
  // mixN   - count of components in the mixtures
  // dimN   - dimensionality of the observation data
  cmChmm_t* cmChmmAlloc(    cmCtx* c, cmChmm_t* p, unsigned stateN, unsigned mixN, unsigned dimN, const cmReal_t* iV, const cmReal_t* aM );
  cmRC_t    cmChmmFree(     cmChmm_t** pp );
  cmRC_t    cmChmmInit(     cmChmm_t* p, unsigned stateN, unsigned mixN, unsigned dimN, const cmReal_t* iV, const cmReal_t* aM );
  cmRC_t    cmChmmFinal(    cmChmm_t* p );


  // Set the iV,aM and bV parameters to well-formed random values. 
  cmRC_t    cmChmmRandomize( cmChmm_t* p, const cmReal_t* oM, unsigned T );

  // Train the HMM using segmental k-means to initialize the model parameters.
  // threshProb is the min change in fit between the data and the model  above which the procedure will continue to iterate.
  // maxIterCnt is the maximum number of iterations the algorithm will make without regard for threshProb. 
  // iterCnt    is the value of iterCnt used in the call cmChmmTrain() on each iteration
  cmRC_t    cmChmmSegKMeans( cmChmm_t* p, const cmReal_t* oM, unsigned T, cmReal_t threshProb, unsigned maxIterCnt, unsigned iterCnt );

  cmRC_t    cmChmmSetGmm(   cmChmm_t* p, unsigned i, const cmReal_t* wV, const cmReal_t* uM, const cmReal_t* sMM, unsigned flags );

  // oM[D,T]     - observation matrix
  // alphaM[N,T] - prob of being in each state and observtin oM(:,t)
  // logPrV[T]   - (optional) record the log prob of the data given the model at each time step 
  // Returns sum(logPrV[T])
  cmReal_t  cmChmmForward( const cmChmm_t* p, const cmReal_t* oM, unsigned T, cmReal_t* alphaM, cmReal_t* logPrV );

  void      cmChmmBackward( const cmChmm_t* p, const cmReal_t* oM, unsigned T, cmReal_t* betaM );

  // bM[N,T] the state-observation probability table is optional
  cmReal_t  cmChmmCompare( const cmChmm_t* p0, const cmChmm_t* p1, unsigned T );


  // Generate a series of observations.
  // oM[ p->D , T ] - output matrix
  // sV[ T ]        - optional vector to record the state used to generate the ith observation.
  cmRC_t    cmChmmGenerate( const cmChmm_t* p, cmReal_t* oM, unsigned T, unsigned* sV );

  // Infer the HMM parameters (p->iV,p->aM,p->bV) from the observations oM[D,T]
  enum { kNoTrainMixCoeffChmmFl=0x01, kNoTrainMeanChmmFl=0x02, kNoTrainCovarChmmFl=0x04 };
  cmRC_t    cmChmmTrain(    cmChmm_t* p, const cmReal_t* oM, unsigned T, unsigned iterCnt, cmReal_t thresh, unsigned flags );

  // Determine the ML state sequence yV[T] given the observations oM[D,T].
  cmRC_t    cmChmmDecode(   cmChmm_t* p, const cmReal_t* oM, unsigned T, unsigned* yV );

  void      cmChmmPrint(    cmChmm_t* p );

  void      cmChmmTest( cmRpt_t* rpt, cmLHeapH_t lhH, cmSymTblH_t stH  );


  //------------------------------------------------------------------------------------------------------------
  // Chord recognizer

  typedef struct 
  {
    cmObj     obj;
    cmChmm_t* h;       // hmm 

    unsigned  N;       // state count     N=24
    unsigned  D;       // data dimension  D=12 
    unsigned  S;       // tonal space dim S=6
    unsigned  T;       // frames in chromaM
    cmReal_t* iV;      // iV[N]
    cmReal_t* aM;      // aM[N,N]
    cmReal_t* uM;      // uM[D,N]
    cmReal_t* sMM;     // sMM[D*D,N] 
    cmReal_t* phiM;    // phiM[S,T]
    cmReal_t* chromaM; // chromaM[D,T]
    cmReal_t* tsM;     // tsM[S,T]
    cmReal_t* cdtsV;   // cdts[1,T]

    cmReal_t  triadSeqMode;
    cmReal_t  triadSeqVar;
    cmReal_t  triadIntMean;
    cmReal_t  triadIntVar;
    cmReal_t* tsMeanV;       // tsMeanV[S];
    cmReal_t* tsVarV;        // tsVarV[S] 
    cmReal_t  cdtsMean;     
    cmReal_t  cdtsVar;      

  } cmChord;

  cmChord*   cmChordAlloc( cmCtx* c, cmChord*  p, const cmReal_t* chromaM, unsigned T );
  cmRC_t     cmChordFree(  cmChord** p );
  cmRC_t     cmChordInit(  cmChord*  p, const cmReal_t* chromaM, unsigned T );
  cmRC_t     cmChordFinal( cmChord*  p );

  void       cmChordTest( cmRpt_t* rpt, cmLHeapH_t lhH, cmSymTblH_t stH ); 

#ifdef __cplusplus
}
#endif


#endif
