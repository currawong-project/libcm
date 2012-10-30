#ifndef cmLinkedHeap_h
#define cmLinkedHeap_h

#ifdef __cplusplus
extern "C" {
#endif

  //{
  //(
  typedef cmHandle_t cmLHeapH_t;
  
  extern cmLHeapH_t cmLHeapNullHandle;

  cmLHeapH_t cmLHeapCreate( unsigned dfltBlockByteCnt, cmCtx_t* ctx );
  void       cmLHeapDestroy( cmLHeapH_t* hp );

  void*     cmLHeapAllocate(     cmLHeapH_t h, void* orgDataPtr, unsigned eleCnt, unsigned eleByteCnt,   unsigned flags, const char* fileStr, const char* funcStr, unsigned fileLine );
  cmChar_t* cmLHeapAllocStr(     cmLHeapH_t h, void* orgDataPtr, const cmChar_t*  str,  unsigned charCnt, unsigned flags, const char* fileStr, const char* funcStr, unsigned fileLine );

  // If ptr==NULL the function returns with no side effect.
  void      cmLHeapFree(         cmLHeapH_t h, void* ptr );
  void      cmLHeapFreeDebug(    cmLHeapH_t h, void* dataPtr,  const char* fileName, const char* funcName, unsigned fileLine );

  // If *ptr==NULL the function returns with no side effect.
  void      cmLHeapFreePtr(      cmLHeapH_t h, void** ptrPtr );
  void      cmLHeapFreePtrDebug( cmLHeapH_t h, void** dataPtrPtr,  const char* fileName, const char* funcName, unsigned fileLine );

  bool       cmLHeapIsValid( cmLHeapH_t h );

  // Return the values set in call to cmLHeapCreate()
  unsigned   cmLHeapDefaultBlockByteCount( cmLHeapH_t h );
  unsigned   cmLHeapGuardByteCount(  cmLHeapH_t h );
  unsigned   cmLHeapAlignByteCount(  cmLHeapH_t h );
  unsigned   cmLHeapInitializeFlags( cmLHeapH_t h );

  // If releaseFl==false then marks all internal memory blocks as empty but does not actually 
  // release the associated memory, otherwise releases all memory blocks.
  void       cmLHeapClear(   cmLHeapH_t h, bool releaseFl );

  // mmFlags take the same values as the flags parameter to cmMmReport().
  cmMmRC_t   cmLHeapReportErrors( cmLHeapH_t h, unsigned mmFlags );
  void       cmLHeapReport(  cmLHeapH_t h );
  void       cmLHeapTest();

#if cmDEBUG_FL == 0

#define cmLHeapAlloc( h, byteCnt ) cmLHeapAllocate(h,NULL,1,byteCnt, kAlignMmFl,             NULL,NULL,0)
#define cmLHeapAllocZ(h, byteCnt ) cmLHeapAllocate(h,NULL,1,byteCnt, kAlignMmFl | kZeroMmFl, NULL,NULL,0)

#define cmLhAlloc(h,t,n)      ((t*)cmLHeapAllocate(h,NULL,n,sizeof(t), kAlignMmFl,              NULL,NULL,0))
#define cmLhAllocZ(h,t,n)     ((t*)cmLHeapAllocate(h,NULL,n,sizeof(t), kAlignMmFl  | kZeroMmFl, NULL,NULL,0))
#define cmLhResizeN( h,t,p,n) ((t*)cmLHeapAllocate(h,p,   n,sizeof(t), kAlignMmFl,              NULL,NULL,0))
#define cmLhResizeNZ(h,t,p,n) ((t*)cmLHeapAllocate(h,p,   n,sizeof(t), kAlignMmFl  | kZeroMmFl, NULL,NULL,0))

#define cmLhAllocStr(   h, str )       cmLHeapAllocStr( h, NULL, str, strlen(str), kAlignMmFl, NULL,NULL,0 )
#define cmLhAllocStrN(  h, str, n )    cmLHeapAllocStr( h, NULL, str, n,           kAlignMmFl, NULL,NULL,0 )
#define cmLhResizeStr(  h, p, str )    cmLHeapAllocStr( h, p,    str, strlen(str), kAlignMmFl, NULL,NULL,0 )
#define cmLhResizeStrN( h, p, str, n ) cmLHeapAllocStr( h, p,    str, n,           kAlignMmFl, NULL,NULL,0 )

#define cmLhFree( h, p )           cmLHeapFree( h, p )
#define cmLhFreePtr(h, p )         cmLHeapFreePtr( h, p ) 

#else 

#define cmLHeapAlloc( h, byteCnt ) cmLHeapAllocate(h,NULL,1,byteCnt, kAlignMmFl,             __FILE__,__FUNCTION__,__LINE__)
#define cmLHeapAllocZ(h, byteCnt ) cmLHeapAllocate(h,NULL,1,byteCnt, kAlignMmFl | kZeroMmFl, __FILE__,__FUNCTION__,__LINE__)

#define cmLhAlloc(h,t,n)      ((t*)cmLHeapAllocate(h,NULL,n,sizeof(t), kAlignMmFl,              __FILE__,__FUNCTION__,__LINE__))
#define cmLhAllocZ(h,t,n)     ((t*)cmLHeapAllocate(h,NULL,n,sizeof(t), kAlignMmFl  | kZeroMmFl, __FILE__,__FUNCTION__,__LINE__))
#define cmLhResizeN( h,t,p,n) ((t*)cmLHeapAllocate(h,p,   n,sizeof(t), kAlignMmFl,              __FILE__,__FUNCTION__,__LINE__))
#define cmLhResizeNZ(h,t,p,n) ((t*)cmLHeapAllocate(h,p,   n,sizeof(t), kAlignMmFl  | kZeroMmFl, __FILE__,__FUNCTION__,__LINE__))

#define cmLhAllocStr(  h, str )        cmLHeapAllocStr( h, NULL, str, strlen(str),    kAlignMmFl, __FILE__,__FUNCTION__,__LINE__ )
#define cmLhAllocStrN( h, str, n )     cmLHeapAllocStr( h, NULL, str, n,              kAlignMmFl, __FILE__,__FUNCTION__,__LINE__ )
#define cmLhResizeStr( h, p, str )     cmLHeapAllocStr( h, p,    str, strlen(str),    kAlignMmFl, __FILE__,__FUNCTION__,__LINE__ )
#define cmLhResizeStrN( h, p, str, n ) cmLHeapAllocStr( h, p,    str, n,              kAlignMmFl, __FILE__,__FUNCTION__,__LINE__ )

#define cmLhFree( h, p )           cmLHeapFreeDebug(    h, p, __FILE__,__FUNCTION__,__LINE__ )
#define cmLhFreePtr(h, p )         cmLHeapFreePtrDebug( h, p, __FILE__,__FUNCTION__,__LINE__ ) 

#endif

  //)
  //}

#ifdef __cplusplus
}
#endif

#endif
