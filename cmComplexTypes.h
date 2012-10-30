#ifndef cmComplexTypes_h
#define cmComplexTypes_h

#include <complex.h>
#include <fftw3.h>

#if CM_FLOAT_SMP == 1

#define cmCabsS  cabsf
#define cmCatanS catanf
#define cmCrealS crealf
#define cmCimagS cimagf 
#define cmCargS  cargf

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

#define cmFftPlanAllocR   fftw_plan_dft_r2c_1d
#define cmFft1dPlanAllocR fftw_plan_dft_1d
#define cmIFftPlanAllocR  fftw_plan_dft_c2r_1d
#define cmFftPlanFreeR    fftw_destroy_plan
#define cmFftMallocR      fftw_malloc
#define cmFftFreeMemR     fftw_free
#define cmFftExecuteR     fftw_execute

  typedef fftw_plan       cmFftPlanR_t;

#endif

#endif
