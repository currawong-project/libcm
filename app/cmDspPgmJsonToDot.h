#ifndef cmDspPgmJsonToDot_h
#define cmDspPgmJsonToDot_h

#ifdef __cplusplus
extern "C" {
#endif

  enum
  {
    kOkDotRC = cmOkRC,
    kJsonFailDotRC,
    kJsonSyntaxErrDotRC,
    kInvalidArgDotRC,
    kLHeapFailDotRC,
    kFileFailDotRC
  };

  typedef unsigned cmDotRC_t;

  cmDotRC_t cmDspPgmJsonToDot( cmCtx_t* ctx, const cmChar_t* inFn, const cmChar_t* outFn );
  
#ifdef __cplusplus
}
#endif
  
#endif
