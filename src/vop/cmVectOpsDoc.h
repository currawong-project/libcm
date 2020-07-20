
// This file is used to generate the documentation 
// source file for cmVectOpsTemplateHdr.h and 
// cmVectOpsRIHdr.h.  The actual documentation 
// is generated using the gcc preprocessor.
// switches: -E : Stop after preprocess
//           -C : Do not strip comments.
//           -P : Do not generate line markers 
// gcc -E -C -P -o cmVectOpsDocOut.h cmVectOpsDoc.h


#include "cmVectOpsTemplateUndef.h"
#define VECT_OP_TYPE       T_t
#define VECT_OP_FUNC(Func) cmVOT_##Func
#define VECT_OP_EPSILON    FLT_EPSILON
#define VECT_OP_MAX        FLT_MAX
#define VECT_OP_MIN        FLT_MIN
#define VECT_OP_LAP_FUNC(F)        s##F
#define VECT_OP_BLAS_FUNC(F) cblas_s##F

//{
//[
#include "cmVectOpsTemplateHdr.h"
#include "cmVectOpsRIHdr.h"
//]
//}

