#ifndef cmStrStream_h
#define cmStrStream_h


#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"String stream text sink." kw:[text] }
  
  enum
  {
    kOkSsRC = cmOkRC,
    kLHeapMemFailSsRC
  };

  typedef cmHandle_t cmStrStreamH_t;
  typedef cmRC_t     cmSsRC_t;

  extern cmStrStreamH_t cmStrStreamNullHandle;


  // If 'dfltBlkByteCnt' 'is set to 0 then the dfltBlkByteCnt is internally set to 4096.
  cmSsRC_t cmOStrStreamCreate( cmCtx_t* ctx, cmStrStreamH_t* hp, unsigned dfltBlkByteCnt );
  cmSsRC_t cmOStrStreamDestroy( cmStrStreamH_t* hp );

  bool     cmOStrStreamIsValid( cmStrStreamH_t h );

  cmSsRC_t cmOStrStreamWrite(     cmStrStreamH_t h, const void* dp, unsigned byteCnt );
  cmSsRC_t cmOStrStreamWriteStr(  cmStrStreamH_t h, const cmChar_t* str );
  cmSsRC_t cmOStrStreamWriteStrN( cmStrStreamH_t h, const cmChar_t* str, unsigned n );
  cmSsRC_t cmOStrStreamVPrintf(   cmStrStreamH_t h, const cmChar_t* fmt, va_list vl );  
  cmSsRC_t cmOStrStreamPrintf(    cmStrStreamH_t h, const cmChar_t* fmt, ... );

  unsigned  cmOStrStreamByteCount( cmStrStreamH_t h );
  void*     cmOStrStreamAllocBuf(  cmStrStreamH_t h );
  cmChar_t* cmOStrStreamAllocText( cmStrStreamH_t h );

  //)
  
#ifdef __cplusplus
}
#endif

#endif
