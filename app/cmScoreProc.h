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
    kProcFailSpRC
  };


  cmSpRC_t  cmScoreProc(cmCtx_t* ctx );

  //)
  
#ifdef __cplusplus
}
#endif

#endif
