

// SS
#include "cmProcTemplateUndef.h"
#define T0             cmSample_t
#define T1             cmSample_t
#define COMPLEX_T0     cmComplexS_t
#define COMPLEX_T1     cmComplexS_t
#define FFT_PLAN_T0    cmFftPlanS_t
#define FFT_PLAN_T1    cmFftPlanS_t
#define CLASS(F)       cm##F##SS
#define STRUCT(F)      cm##F##SS_str
#define MEMBER(F)      cm##F##SS
#define FFT_FUNC_T0(F) cm##F##S
#define FFT_FUNC_T1(F) cm##F##S
#define VOP_T0(F)      cmVOS_##F

#include "cmProcTemplateHdr.h"
#include "cmProcTemplateCode.h"


// SR
#include "cmProcTemplateUndef.h"
#define T0             cmSample_t
#define T1             cmReal_t
#define COMPLEX_T0     cmComplexS_t
#define COMPLEX_T1     cmComplexR_t
#define FFT_PLAN_T0    cmFftPlanS_t
#define FFT_PLAN_T1    cmFftPlanR_t
#define CLASS(F)       cm##F##SR
#define STRUCT(F)      cm##F##SR_str
#define MEMBER(F)      cm##F##SR
#define FFT_FUNC_T0(F) cm##F##S
#define FFT_FUNC_T1(F) cm##F##R
#define VOP_T0(F)      cmVOS_##F

#include "cmProcTemplateHdr.h"
#include "cmProcTemplateCode.h"

// RS
#include "cmProcTemplateUndef.h"
#define T0             cmReal_t
#define T1             cmSample_t
#define COMPLEX_T0     cmComplexR_t
#define COMPLEX_T1     cmComplexS_t
#define FFT_PLAN_T0    cmFftPlanR_t
#define FFT_PLAN_T1    cmFftPlanS_t
#define CLASS(F)       cm##F##RS
#define STRUCT(F)      cm##F##RS_str
#define MEMBER(F)      cm##F##RS
#define FFT_FUNC_T0(F) cm##F##R
#define FFT_FUNC_T1(F) cm##F##S
#define VOP_T0(F)      cmVOR_##F

#include "cmProcTemplateHdr.h"
#include "cmProcTemplateCode.h"

// RR
#include "cmProcTemplateUndef.h"
#define T0             cmReal_t
#define T1             cmReal_t
#define COMPLEX_T0     cmComplexR_t
#define COMPLEX_T1     cmComplexR_t
#define FFT_PLAN_T0    cmFftPlanR_t
#define FFT_PLAN_T1    cmFftPlanR_t
#define CLASS(F)       cm##F##RR
#define STRUCT(F)      cm##F##RR_str
#define MEMBER(F)      cm##F##RR
#define FFT_FUNC_T0(F) cm##F##R
#define FFT_FUNC_T1(F) cm##F##R
#define VOP_T0(F)      cmVOR_##F


#include "cmProcTemplateHdr.h"
#include "cmProcTemplateCode.h"

#include "cmProcTemplateUndef.h"
