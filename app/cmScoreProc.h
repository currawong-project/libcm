#ifndef cmScoreProc_h
#define cmScoreProc_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"Programs for processing cmScore and peformane data." kw:[score seq]}

  typedef unsigned cmSpRC_t;

  enum
  {
    kOkSpRC,
    kJsonFailSpRC,
    kScoreFailSpRC,
    kTimeLineFailSpRC,
    kScoreMatchFailSpRC,
    kFileFailSpRC,
    kProcFailSpRC,
    kSelectorFailSpRC
  };


  cmSpRC_t  cmScoreProc(cmCtx_t* ctx, const cmChar_t* sel, const cmChar_t* pgmRsrcFn, const cmChar_t* outFn);
  
  //)
  
#ifdef __cplusplus
}
#endif

#endif
