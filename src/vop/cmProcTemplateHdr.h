//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.


/*
enum
{
  kNoConvertFftFl = 0x00, // do not fill magV or phsV (leave output in complexV)
  kToPolarFftFl   = 0x01, // fill magV and phsV with the polar form of complexV
  kToRectFftFl    = 0x02 // fill magV (real) and phsV (imag) with the rect. form of commplexV,
};
*/

//-----------------------------------------------------------------------------------------------------------
// FFT
//

#ifndef CONST_GUARD
#define CONST_GUARD

  enum
  {
    kNoConvertFftFl = 0x00, // do not fill magV or phsV (leave output in complexV)
    kToPolarFftFl   = 0x01, // fill magV and phsV with the polar form of complexV
    kToRectFftFl    = 0x02 // fill magV (real) and phsV (imag) with the rect. form of commplexV,
  };
#endif

typedef struct STRUCT(Fft)
{
  cmObj           obj;

  FFT_PLAN_T0     plan;       // fftw plan 
  T0*             inPtr;      // 
  COMPLEX_T0*     complexV;   // fft output in complex form 

  unsigned        wndSmpCnt;  // length of the fft input buffer
  T1*             magV;       // magnitude or real output (amplitude NOT power)
  T1*             phsV;       // phase or imag output
  unsigned        binCnt;     // length of magV and phsV
  unsigned        flags;
  bool            copyFl;
  cmMtxFile*      mfp;

} CLASS(Fft);


CLASS(Fft)*   MEMBER(FftAlloc)( cmCtx* c, CLASS(Fft)* p, T0* inPtr, unsigned wndSmpCnt, unsigned flags );
cmRC_t        MEMBER(FftFree)(   CLASS(Fft)** pp );
cmRC_t        MEMBER(FftInit)(   CLASS(Fft)* p, T0*   inPtr, unsigned wndSmpCnt, unsigned flags );
cmRC_t        MEMBER(FftFinal)(  CLASS(Fft)* p );

/// Set sp to NULL if a fixed input buffer was given in the call to cmFftAlloc() or cmFftInit()
cmRC_t        MEMBER(FftExec)(  CLASS(Fft)* p, const T0*   sp, unsigned sn  );


//-----------------------------------------------------------------------------------------------------------
// IFFT
//

typedef struct  STRUCT(IFft)
{
  cmObj           obj;
  unsigned        binCnt;
  unsigned        outN;    
  COMPLEX_T1*     complexV;  // the type of complexV should match the type of outV
  T1*             outV;
  FFT_PLAN_T1     plan;
  
} CLASS(IFft);

CLASS(IFft)*  MEMBER(IFftAlloc)( cmCtx* c, CLASS(IFft)* p, unsigned binCnt );
cmRC_t   MEMBER(IFftFree)(  CLASS(IFft)** pp );
cmRC_t   MEMBER(IFftInit)(  CLASS(IFft)* p, unsigned binCnt );
cmRC_t   MEMBER(IFftFinal)( CLASS(IFft)* p );

  // x must contain 'binCnt' elements.
cmRC_t   MEMBER(IFftExec)(      CLASS(IFft)* p, COMPLEX_T0* x );
cmRC_t   MEMBER(IFftExecPolar)( CLASS(IFft)* p, const T0* magV, const T0* phsV ); 
cmRC_t   MEMBER(IFftExecRect)(  CLASS(IFft)* p, const T0* rV,   const T0* iV );

