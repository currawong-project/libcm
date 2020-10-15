//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.

// Float
#include "cmVectOpsTemplateUndef.h"
#define VECT_OP_TYPE       float
#define VECT_OP_FUNC(Func) cmVOF_##Func
#define VECT_OP_EPSILON    FLT_EPSILON
#define VECT_OP_MAX        FLT_MAX
#define VECT_OP_MIN        FLT_MIN
#define VECT_OP_LAP_FUNC(F)        s##F
#define VECT_OP_BLAS_FUNC(F) cblas_s##F

#include "cmVectOpsTemplateHdr.h"
#include "cmVectOpsTemplateCode.h"
#include "cmVectOpsRIHdr.h"
#include "cmVectOpsRICode.h"


// Double
#include "cmVectOpsTemplateUndef.h"
#define VECT_OP_TYPE    double
#define VECT_OP_FUNC(F) cmVOD_##F
#define VECT_OP_EPSILON DBL_EPSILON
#define VECT_OP_MAX     DBL_MAX
#define VECT_OP_MIN     DBL_MIN
#define VECT_OP_LAP_FUNC(F)        d##F
#define VECT_OP_BLAS_FUNC(F) cblas_d##F

#include "cmVectOpsTemplateHdr.h"
#include "cmVectOpsTemplateCode.h"
#include "cmVectOpsRIHdr.h"
#include "cmVectOpsRICode.h"


// cmSample_t
#include "cmVectOpsTemplateUndef.h"
#define VECT_OP_TYPE    cmSample_t
#define VECT_OP_FUNC(F) cmVOS_##F
#define VECT_OP_EPSILON cmSample_EPSILON
#define VECT_OP_MAX     cmSample_MAX
#define VECT_OP_MIN     cmSample_MIN

#if CM_FLOAT_SMP == 1
#define VECT_OP_LAP_FUNC(F)        s##F
#define VECT_OP_BLAS_FUNC(F) cblas_s##F
#else
#define VECT_OP_LAP_FUNC(F)        d##F
#define VECT_OP_BLAS_FUNC(F) cblas_d##F
#endif

#include "cmVectOpsTemplateHdr.h"
#include "cmVectOpsTemplateCode.h"
#include "cmVectOpsRIHdr.h"
#include "cmVectOpsRICode.h"

// cmReal_t
#include "cmVectOpsTemplateUndef.h"
#define VECT_OP_TYPE    cmReal_t
#define VECT_OP_FUNC(F) cmVOR_##F
#define VECT_OP_EPSILON cmReal_EPSILON
#define VECT_OP_MAX     cmReal_MAX
#define VECT_OP_MIN     cmReal_MIN

#if CM_FLOAT_REAL == 1
#define VECT_OP_LAP_FUNC(F)        s##F
#define VECT_OP_BLAS_FUNC(F) cblas_s##F
#else
#define VECT_OP_LAP_FUNC(F)        d##F
#define VECT_OP_BLAS_FUNC(F) cblas_d##F
#endif

#include "cmVectOpsTemplateHdr.h"
#include "cmVectOpsTemplateCode.h"
#include "cmVectOpsRIHdr.h"
#include "cmVectOpsRICode.h"


// Int
#include "cmVectOpsTemplateUndef.h"
#define VECT_OP_TYPE    int
#define VECT_OP_FUNC(F) cmVOI_##F

#include "cmVectOpsRIHdr.h"
#include "cmVectOpsRICode.h"


// Unsigned
#include "cmVectOpsTemplateUndef.h"
#define VECT_OP_TYPE    unsigned
#define VECT_OP_FUNC(F) cmVOU_##F

#include "cmVectOpsRIHdr.h"
#include "cmVectOpsRICode.h"


// bool
#include "cmVectOpsTemplateUndef.h"
#define VECT_OP_TYPE    bool
#define VECT_OP_FUNC(F) cmVOB_##F

#include "cmVectOpsRIHdr.h"
#include "cmVectOpsRICode.h"

// char
#include "cmVectOpsTemplateUndef.h"
#define VECT_OP_TYPE    char
#define VECT_OP_FUNC(F) cmVOC_##F

#include "cmVectOpsRIHdr.h"
#include "cmVectOpsRICode.h"
