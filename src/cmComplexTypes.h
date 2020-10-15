//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#ifndef cmComplexTypes_h
#define cmComplexTypes_h

#ifdef __cplusplus
extern "C" {
#endif

#include <complex.h>
#include <fftw3.h>

//( { file_desc: "Constants and functions used for working with complex values." kw:[base math] }

#if CM_FLOAT_SMP == 1

#define cmCabsS  cabsf
#define cmCatanS catanf
#define cmCrealS crealf
#define cmCimagS cimagf 
#define cmCargS  cargf
#define cmCconjS conjf

#define cmFftPlanAllocS   fftwf_plan_dft_r2c_1d
#define cmFft1dPlanAllocS fftwf_plan_dft_1d
#define cmIFftPlanAllocS  fftwf_plan_dft_c2r_1d
#define cmFftPlanFreeS    fftwf_destroy_plan
#define cmFftMallocS      fftwf_malloc
#define cmFftFreeMemS     fftwf_free
#define cmFftExecuteS     fftwf_execute

  typedef fftwf_plan      cmFftPlanS_t;

#else

#define cmCabsS  cabs
#define cmCatanS catan
#define cmCrealS creal
#define cmCimagS cimag 
#define cmCargS  carg
#define cmCconjS conj

#define cmFftPlanAllocS   fftw_plan_dft_r2c_1d
#define cmFft1dPlanAllocS fftw_plan_dft_1d
#define cmIFftPlanAllocS  fftw_plan_dft_c2r_1d
#define cmFftPlanFreeS    fftw_destroy_plan
#define cmFftMallocS      fftw_malloc
#define cmFftFreeMemS     fftw_free
#define cmFftExecuteS     fftw_execute

  typedef fftw_plan      cmFftPlanS_t;

#endif

//-----------------------------------------------------------------
//-----------------------------------------------------------------
//-----------------------------------------------------------------

#if CM_FLOAT_REAL == 1

#define cmCabsR  cabsf
#define cmCatanR catanf
#define cmCrealR crealf
#define cmCimagR cimagf 
#define cmCargR  cargf
#define cmCconjR conjf

#define cmFftPlanAllocR   fftwf_plan_dft_r2c_1d
#define cmFft1dPlanAllocR fftwf_plan_dft_1d
#define cmIFftPlanAllocR  fftwf_plan_dft_c2r_1d
#define cmFftPlanFreeR    fftwf_destroy_plan
#define cmFftMallocR      fftwf_malloc
#define cmFftFreeMemR     fftwf_free
#define cmFftExecuteR     fftwf_execute

  typedef fftwf_plan     cmFftPlanR_t;

#else

#define cmCabsR  cabs
#define cmCatanR catan
#define cmCrealR creal
#define cmCimagR cimag 
#define cmCargR  carg
#define cmCconjR conj

#define cmFftPlanAllocR   fftw_plan_dft_r2c_1d
#define cmFft1dPlanAllocR fftw_plan_dft_1d
#define cmIFftPlanAllocR  fftw_plan_dft_c2r_1d
#define cmFftPlanFreeR    fftw_destroy_plan
#define cmFftMallocR      fftw_malloc
#define cmFftFreeMemR     fftw_free
#define cmFftExecuteR     fftw_execute

  typedef fftw_plan       cmFftPlanR_t;

#endif

void cmVOCR_MultVVV( cmComplexR_t* y, const cmComplexS_t* x0, const cmComplexR_t* x1, unsigned n );
void cmVOCR_MultVFV(  cmComplexR_t* y, const float* x, unsigned n );
void cmVOCR_DivVFV(   cmComplexR_t* y, const float_t* x, unsigned n );
void cmVOCR_Abs(     cmSample_t*   y, const cmComplexR_t* x, unsigned n );
void cmVOCR_MultVS(  cmComplexR_t* y, cmReal_t v, unsigned n );
void cmVOCR_DivVS(   cmComplexR_t* y, cmReal_t v, unsigned n );

//)

#ifdef __cplusplus
}
#endif

#endif
