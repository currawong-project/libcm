//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#ifndef cmRptFile_h
#define cmRptFile_h

#ifdef __cplusplus
extern "C" {
#endif
  
  //( { file_desc: "The cmRptFile provides a cmRpt class which outputs to a file." kw:[base]}

  enum
  {
    kOkRfRC = cmOkRC,
    kFileFailRfRC
  };

  typedef unsigned   cmRfRC_t;
  typedef cmHandle_t cmRptFileH_t;

  extern cmRptFileH_t cmRptFileNullHandle;

  cmRfRC_t cmRptFileCreate( cmCtx_t* ctx, cmRptFileH_t* hp, const cmChar_t* printFn, const cmChar_t* errorFn );
  cmRfRC_t cmRptFileClose( cmRptFileH_t* hp );
  
  bool     cmRptFileIsValid( cmRptFileH_t h );
    
  cmRpt_t* cmRptFileRpt( cmRptFileH_t h );

  //)
  
#ifdef __cplusplus
}
#endif

#endif
