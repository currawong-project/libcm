//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
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
