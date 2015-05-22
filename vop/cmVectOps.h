#ifndef cmVectOps_h
#define cmVectOps_h



#ifdef OS_OSX

#include <Accelerate/Accelerate.h>

typedef __CLPK_integer int_lap_t;

#else

#ifdef __cplusplus
extern "C" {
#endif

typedef int int_lap_t;

extern int  ilaenv_(int*, char *, char *, int*, int*,  int*, int*, int, int );

extern void dgetrf_( int*, int*, double*, int*, int*, int* );
extern void dgetri_( int* arn, double* a, int* lda, int* ipiv, double* w, int* wn, int* info ); 

extern void sgetrf_( int*, int*, float*, int*, int*, int* );
extern void sgetri_( int* arn, float* a, int* lda, int* ipiv, float* w, int* wn, int* info ); 

extern void dgesv_( int*, int*, double*, int*, int*, double*, int*, int* );
extern void sgesv_( int*, int*, float*,  int*, int*, float*,  int*, int* );

extern void dpotrf_(char*, int*, double*, int*, int* );
extern void spotrf_(char*, int*, float*,  int*, int* );

#ifdef __cplusplus
}
#endif


#endif

#ifdef __cplusplus
extern "C" {
#endif

// Constants for use with cmVOX_LPSinc()
enum { kNoLpSincFlags=0, kHighPass_LPSincFl=0x01, kNormalize_LPSincFl=0x02 };

// Constants for use with cmVOX_MelMask()
enum{ kNoMelFlags=0, kShiftMelFl=0x01, kNormalizeMelFl=0x02 };

// Constants for use with cmVOX_TerhardtThresholdMask()
enum { kNoTtmFlags=0, kModifiedTtmFl=0x01 };

  // Constants for cmVOX_MultMMM1() and cmVOX_MultMMM2()
  enum { kTransposeM0Fl=0x01, kTransposeM1Fl=0x02 };

#define kDefaultMelBandCnt  (36)
#define kDefaultBarkBandCnt (24)


#include "cmVectOpsTemplateMain.h"


#define cmDefaultFieldWidth  (10)
#define cmDefaultDecPlCnt    (4)

  // Flags for extended print control
#define cmPrintNoColLabelsFl  (0)
#define cmPrintMatlabLabelsFl (1)
#define cmPrintShortLabelsFl  (2)



void cmVOI_Print( cmRpt_t* rpt, unsigned rn, unsigned cn, const int*      sp );
void cmVOU_Print( cmRpt_t* rpt, unsigned rn, unsigned cn, const unsigned* sp );

void cmVOI_PrintL( const char* label, cmRpt_t* rpt, unsigned rn, unsigned cn, const int*      sp );
void cmVOU_PrintL( const char* label, cmRpt_t* rpt, unsigned rn, unsigned cn, const unsigned* sp );

unsigned* cmVOU_Mod( unsigned* dbp, unsigned dn, unsigned modVal );

unsigned* cmVOU_Hist( unsigned* hbp, unsigned hn, const unsigned* sbp, unsigned sn );

/// Fill vbp[vn] with random integer values between 0 and maxValue.  
unsigned* cmVOU_Random( unsigned* vbp, unsigned vn, unsigned maxValue );

/// Fill vbp[vn] with vn unique random integer values between 0 and maxValue.  
/// Do not use this function to generate a reordering of a consecutive
/// sequence of random values use cmVOU_RandomSeq instead.  
/// For best efficiency 'mavValue' should be large compared to 'vn'.
unsigned* cmVOU_UniqueRandom( unsigned* vbp, unsigned vn, unsigned maxValue );

/// FIll vbp[vn] the sequence 0:vn-1 in randomized order.
unsigned* cmVOU_RandomSeq( unsigned* vbp, unsigned vn );


cmReal_t cmVOU_Mean(    const unsigned* sp, unsigned sn );
cmReal_t cmVOU_Variance(const unsigned* sp, unsigned sn, const cmReal_t* mean); 

cmReal_t cmVOI_Mean(    const int* sp, unsigned sn );
cmReal_t cmVOI_Variance(const int* sp, unsigned sn, const cmReal_t* mean); 

// Complex vector * matrix multiply
// dbp[1,dn] = v[1,vn] * m[vn,dn]
cmComplexR_t* cmVORC_MultVVM( cmComplexR_t* dbp, unsigned dn, const cmComplexR_t* vp, unsigned vn, const cmComplexR_t* m );

#define cmAbs(x)              _Generic((x),  double:fabs,       float:fabsf,      int:abs,        unsigned:abs,        default:fabs )(x)  
#define cmIsClose(x0,x1,eps)  _Generic((x0), double:cmIsCloseD, float:cmIsCloseF, int:cmIsCloseI, unsigned:cmIsCloseU, default:cmIsCloseD)(x0,x1,eps)
  
#ifdef __cplusplus
}
#endif

#endif
