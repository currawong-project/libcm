#ifndef cmAudLabelFile_h
#define cmAudLabelFile_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"Read and write Audacity label files." kw:[audio file] }
  
enum
{
  kOkAlfRC = cmOkRC,
  kLHeapFailAlfRC,
  kFileFailAlfRC,
  kSyntaxErrAlfRC,
  kAlfFileFailPuRC
};

  typedef cmRC_t     cmAlfRC_t;
  typedef cmHandle_t cmAlfH_t;

  extern cmAlfH_t cmAlfNullHandle;

  typedef struct
  {
    cmReal_t        begSecs;
    cmReal_t        endSecs;
    const cmChar_t* label;
  } cmAlfLabel_t;

  cmAlfRC_t cmAudLabelFileAlloc(     cmCtx_t* ctx, cmAlfH_t* hp );
  cmAlfRC_t cmAudLabelFileAllocOpen( cmCtx_t* ctx, cmAlfH_t* hp, const cmChar_t* fn );
  cmAlfRC_t cmAudLabelFileFree(    cmAlfH_t* hp );

  bool      cmAudLabelFileIsValid( cmAlfH_t h );
  
  cmAlfRC_t cmAudLabelFileOpen(    cmAlfH_t h, const cmChar_t* fn );

  cmAlfRC_t cmAudLabelFileInsert(  cmAlfH_t h, cmReal_t begSecs, cmReal_t endSecs, const cmChar_t* label );
  
  unsigned cmAudLabelFileCount(    cmAlfH_t h );
  const cmAlfLabel_t* cmAudLabelFileLabel( cmAlfH_t h, unsigned idx );

  cmAlfRC_t cmAudLabelFileWrite( cmAlfH_t h, const cmChar_t* fn );
  
  void cmAudLabelFileTest( cmCtx_t* ctx );

  //)
  
#ifdef __cplusplus
}
#endif

#endif
