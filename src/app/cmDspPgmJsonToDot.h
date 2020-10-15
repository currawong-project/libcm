//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#ifndef cmDspPgmJsonToDot_h
#define cmDspPgmJsonToDot_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"Convert a JSON graph description to a DOT vector graphics file." kw:[file plot]}
  
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

  //)
  
#ifdef __cplusplus
}
#endif
  
#endif
