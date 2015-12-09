#ifndef cmFloatTypes_h
#define cmFloatTypes_h


#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"Declare the types cmReal_t and cmSample_t and define some useful limits." kw:[base]}
  //
  // For signal processing functions the cm library uses the types cmSample_t to indicate an audio
  // sample value and cmReal_t to specify a general purpose floating point value. The library
  // is designed in such a way that the actual type, float or double, for these two types may
  // be set at compilation time. Set the preprocessor variable CM_FLOAT_SMP to 1 to indicate 
  // that cmSample_t will be of type 'float' otherwise it will be of type 'double'. 
  // Set the preprocessor variable CM_FLOAT_REAL to 1 to indicate 
  // that cmSample_t will be of type 'float' otherwise it will be of type 'double'. 
  // By default cmSample_t is float nad cmReal_t is double.
  //

  //-----------------------------------------------------------------
#ifndef CM_FLOAT_SMP
#define CM_FLOAT_SMP 1
#endif

#if CM_FLOAT_SMP == 1

  typedef float          cmSample_t;  // cmSample_t is a float
  typedef float _Complex cmComplexS_t;// cmComplexS_t is single precision.

#define cmSample_EPSILON FLT_EPSILON  // Minimum increment between 1.0 and the next greaterv value. (1E-5)
#define cmSample_MAX     FLT_MAX      // Maximum representable number (1E+37).
#define cmSample_MIN     FLT_MIN      // Minimum representable number (1E-37).

#else

  typedef  double          cmSample_t;   // cmSample_t is a double
  typedef  double _Complex cmComplexS_t; // cmComplexS_t is doulbe precision.

#define cmSample_EPSILON DBL_EPSILON    // Minimum increment between 1.0 and the next greaterv value. (1E-9)
#define cmSample_MAX     DBL_MAX        // Maximum representable number (1E+37).
#define cmSample_MIN     DBL_MIN        // Minimum representable number (1E-37).

#endif

  //-----------------------------------------------------------------
  //-----------------------------------------------------------------
  //-----------------------------------------------------------------

#ifndef CM_FLOAT_REAL
#define CM_FLOAT_REAL 0
#endif

#if CM_FLOAT_REAL == 1

  typedef float          cmReal_t;      // cmReal_t is a float
  typedef float _Complex cmComplexR_t;  // cmComplexR_t is single precision.

#define cmReal_EPSILON FLT_EPSILON      // Minimum increment between 1.0 and the next greaterv value. (1E-5)
#define cmReal_MAX     FLT_MAX          // Maximum representable number (1E+37).
#define cmReal_MIN     FLT_MIN          // Minimum representable number (1E-37).

#else

  typedef  double          cmReal_t;      // cmReal_t is a double.
  typedef  double _Complex cmComplexR_t;  // cmComplexR_t is double precision.

#define cmReal_EPSILON DBL_EPSILON  // Minimum increment between 1.0 and the next greaterv value (1E-9).
#define cmReal_MAX     DBL_MAX      // Maximum representable number (1E+37).
#define cmReal_MIN     DBL_MIN      // Minimum representable number (1E-37).


#endif

  //)
  
#ifdef __cplusplus
}
#endif


#endif
