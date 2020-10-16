//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.

// This file is used to generate the documentation 
// source file for cmVectOpsTemplateHdr.h and 
// cmVectOpsRIHdr.h.  The actual documentation 
// is generated using the gcc preprocessor.
// switches: -E : Stop after preprocess
//           -C : Do not strip comments.
//           -P : Do not generate line markers
//           -traditional-cpp : preserve white space
// gcc -E -C -P -traditional-cpp -o cmVectOpsDocOut.h cmVectOpsDoc.h


#include "cmVectOpsTemplateUndef.h"
#define VECT_OP_TYPE       T_t
#define VECT_OP_FUNC(Func) cmVOT_##Func
#define VECT_OP_EPSILON    FLT_EPSILON
#define VECT_OP_MAX        FLT_MAX
#define VECT_OP_MIN        FLT_MIN
#define VECT_OP_LAP_FUNC(F)        s##F
#define VECT_OP_BLAS_FUNC(F) cblas_s##F
//end_cut
//( { file_desc:"Math vector operations." kw:[vop math] }
//)
#include "cmVectOpsTemplateHdr.h"
#include "cmVectOpsRIHdr.h"

