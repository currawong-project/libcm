//{ { label:cmMd }
//(
// Implements an extended memory allocation and tracking manager.
//
// cmMallocDebug is a wrapper to cmMem.h for calls to malloc() and free().
// Most of the cmMdXXX() calls are directly associated with same named 
// functions in cmMem.h. In effect this library is an implementation of the
// cmMm object where malloc() and free() are the callback functions 
// provided to cmMmInitialize().
// 
//)

#ifndef cmMallocDebug_h
#define cmMallocDebug_h

#ifdef __cplusplus
extern "C" {
#endif
  //(
  // Initialize the malloc debug manager. guardByteCnt, alignByteeCnt, flags, and rpt
  // are used to initialize an internal cmMm object.  See cmMm for their semantics.
  cmMmRC_t  cmMdInitialize( unsigned guardByteCnt, unsigned alignByteCnt, unsigned flags, cmRpt_t* rptPtr );

  // Finalize the malloc debug manager which was previously iniitalized via 
  // a call to cmMdInitialize(). 
  cmMmRC_t  cmMdFinalize();

  // Returns true if the malloc debug manager has been initialized.
  bool      cmMdIsValid();

  unsigned  cmMdGuardByteCount();  //< Guard byte count set in cmMdInitialize().
  unsigned  cmMdAlignByteCount();  //< Byte alignment set in cmMdInitialize().
  unsigned  cmMdInitializeFlags(); //< cmMdInitialize() configuration flags.

  // Allocate a block of memory in release compile mode. This function is generally called
  // via one of the cmMemXXX() macros.
  void*    cmMdAllocate(      void *orgDataPtr, unsigned eleCnt, unsigned eleByteCnt, unsigned flags);

  // Allocate a block of memory in debug compile mode. This function is generally called 
  // via one of the cmMmXXX() macros.
  void*    cmMdAllocateDebug( void* orgDataPtr, unsigned eleCnt, unsigned eleByteCnt, unsigned flags, const char* func, const char* fn, unsigned line);

  // Free a block of memory allocated through cmMdAllocate().
  void     cmMdFree(     void*  p );

  // Free a block of memory allocated through cmMdAllocateDebug().
  void     cmMdFreeDebug(void*  p, const char* func, const char* fn, unsigned line );

  // Free a block of memory allocated through cmMdAllocateI() and set the 
  // pointer variable to NULL.  This function combines the act of releasing memory
  // and then setting the memory variable to NULL in order to indicate that the 
  // variable no longer points to a valid data area.
  //
  // The following two function calls cmMdFree/Debug(p) internally and then
  // set *p=NULL. In almost all cases this is what we want to do when freeing
  // allocated memory since it will prevent the pointer from being accidently
  // reused.
  //
  // cmMdFreePtr( void* p ) would ideally be written as 
  // cmMdFreePtr( void** p ) because it is really taking a pointer to a pointer.
  // void** however produces compiler warning because while any pointer will be
  // automatically cast to void* the same is not true of void** (because void**
  // is naturally more restrictive - asking for a pointer to the specific type void).
  void     cmMdFreePtr(     void* p );  
  void     cmMdFreePtrDebug(void* p, const char* func, const char* fn, unsigned line );

  // void  cmMdFreePtr( void** p )
  // void  cmMdFreePtrDebug( void** p, ... )

  // Allocate a block of memory and then copy a string into it. This function is generally 
  // called by the release compile mode version of the cmMemXXXStr() macros.
  cmChar_t* cmMdAllocStr(      void* orgStrPtr, const cmChar_t* str, unsigned strCharCnt, unsigned flags );

  // Allocate a block of memory and then copy a string into it. This function is generally
  // called by the debug compile mode version of the cmMemXXXStr() macros.
  cmChar_t* cmMdAllocStrDebug( void* orgStrPtr, const cmChar_t* str, unsigned strCharCnt, unsigned flags, const char* func, const char* fn, unsigned line );

  // Check if the guard bytes associated with a specified memory block are corrupt.
  // This call is implemented as a direct call the cmMmIsGuardCorrupt().
  cmMmRC_t cmMdIsGuardCorrupt( unsigned id );

  // Check if any of the currently allocated blocks contain corrupt guard bytes.
  // This call is implemented as a direct call the cmMmCheckAllGuards().
  cmMmRC_t cmMdCheckAllGuards();

  // Return the unique id associated with a address returned from cmMdAllocateXXX().
  unsigned cmMdDebugId( const void* dataPtr);

  // Report the status of the memory manager and all blocks. 
  // This call is implemented as a direct call to cmMmReport().
  cmMmRC_t cmMdReport( unsigned mmFlags );

  // An example and test function for the cmMallocDebug manager.
  void     cmMdTest( cmRpt_t* rpt );



#if cmDEBUG_FL == 0

  // Memory Allocation and Release Macros:
  // Release compile mode memory macros generally used in place of direct calls 
  // to cmMdAllocate() or cmMdFree().
  //

#define cmMemAllocate( type, p, eleCnt, fl ) ((type*)cmMdAllocate( p, eleCnt, sizeof(type), fl )) 
#define cmMemMalloc(   byteCnt )                 cmMdAllocate( NULL, byteCnt,           1,     kAlignMmFl) 
#define cmMemMallocZ(  byteCnt )                 cmMdAllocate( NULL, byteCnt,           1,     kAlignMmFl | kZeroMmFl) 
#define cmMemAlloc(    type, eleCnt )    ((type*)cmMdAllocate( NULL, eleCnt, sizeof(type),     kAlignMmFl))
#define cmMemAllocZ(   type, eleCnt )    ((type*)cmMdAllocate( NULL, eleCnt, sizeof(type),     kAlignMmFl | kZeroMmFl))
#define cmMemAllocStr(  str )                    cmMdAllocStr( NULL, str,    cmStringLen(str), kAlignMmFl )
#define cmMemAllocStrN( str, charCnt )           cmMdAllocStr( NULL, str,    charCnt,          kAlignMmFl )
#define cmMemResizeStr( p, str )                 cmMdAllocStr( p,    str,    cmStringLen(str), kAlignMmFl )
#define cmMemResizeStrN(p, str, charCnt )       cmMdAllocStr( p,    str,    charCnt,           kAlignMmFl )
#define cmMemResizeN(   n, p, eleCnt )          (cmMdAllocate( p,    eleCnt, n,                kAlignMmFl))
#define cmMemResizeNZ(  n, p, eleCnt )          (cmMdAllocate( p,    eleCnt, n,                kAlignMmFl | kZeroMmFl ))
#define cmMemResize(   type, p, eleCnt ) ((type*)cmMdAllocate( p,    eleCnt, sizeof(type),     kAlignMmFl))
#define cmMemResizeZ(  type, p, eleCnt ) ((type*)cmMdAllocate( p,    eleCnt, sizeof(type),     kAlignMmFl | kZeroMmFl))
#define cmMemResizeP(  type, p, eleCnt ) ((type*)cmMdAllocate( p,    eleCnt, sizeof(type),     kAlignMmFl | kPreserveMmFl))
#define cmMemResizePZ( type, p, eleCnt ) ((type*)cmMdAllocate( p,    eleCnt, sizeof(type),     kAlignMmFl | kZeroMmFl | kPreserveMmFl))
#define cmMemFree(     ptr )                     cmMdFree( ptr )
#define cmMemPtrFree(  ptrPtr )                  cmMdFreePtr(ptrPtr);


#define cmIsPadCorrupt( id ) (kOkMmRC)


#else 
  
  // Memory Allocation and Release Macros:
  // These are debug compile mode memory allocation macros generally used in place of 
  // direct calls to cmMdAllocateDebug() or cmMdFree().
  //
  //
#define cmMemAllocate( type, p, eleCnt, pre, fl ) ((type*)cmMdAllocateDebug( p, eleCnt, sizeof(type), fl, __FUNCTION__, __FILE__, __LINE__ ))
#define cmMemMalloc(   byteCnt )                 cmMdAllocateDebug( NULL, 1,      byteCnt,          kAlignMmFl,             __FUNCTION__, __FILE__, __LINE__ )
#define cmMemMallocZ(  byteCnt )                 cmMdAllocateDebug( NULL, 1,      byteCnt,          kAlignMmFl | kZeroMmFl, __FUNCTION__, __FILE__, __LINE__ )
#define cmMemAlloc(    type, eleCnt )    ((type*)cmMdAllocateDebug( NULL, eleCnt, sizeof(type),     kAlignMmFl,             __FUNCTION__, __FILE__, __LINE__ ))
#define cmMemAllocZ(   type, eleCnt )    ((type*)cmMdAllocateDebug( NULL, eleCnt, sizeof(type),     kAlignMmFl | kZeroMmFl, __FUNCTION__, __FILE__, __LINE__ ))
#define cmMemAllocStr( str )                    (cmMdAllocStrDebug( NULL, str,    cmStringLen(str), kAlignMmFl,             __FUNCTION__, __FILE__, __LINE__ ))
#define cmMemAllocStrN(str, charCnt )           (cmMdAllocStrDebug( NULL, str,    charCnt,          kAlignMmFl,             __FUNCTION__, __FILE__, __LINE__ ))
#define cmMemResizeStr(p, str )                 (cmMdAllocStrDebug( p,    str,    cmStringLen(str), kAlignMmFl,             __FUNCTION__, __FILE__, __LINE__ ))
#define cmMemResizeStrN(p, str, charCnt )       (cmMdAllocStrDebug( p,    str,    charCnt,          kAlignMmFl,             __FUNCTION__, __FILE__, __LINE__ ))
#define cmMemResizeN(  n,    p, eleCnt )        (cmMdAllocateDebug( p,    eleCnt, n,                kAlignMmFl | kZeroMmFl, __FUNCTION__, __FILE__, __LINE__ ))
#define cmMemResizeNZ( n,    p, eleCnt )        (cmMdAllocateDebug( p,    eleCnt, n,                kZeroMmFl,              __FUNCTION__, __FILE__, __LINE__ ))      
#define cmMemResize(   type, p, eleCnt ) ((type*)cmMdAllocateDebug( p,    eleCnt, sizeof(type),     kAlignMmFl,             __FUNCTION__, __FILE__, __LINE__ )) 
#define cmMemResizeZ(  type, p, eleCnt ) ((type*)cmMdAllocateDebug( p,    eleCnt, sizeof(type),     kAlignMmFl | kZeroMmFl, __FUNCTION__, __FILE__, __LINE__ ))
#define cmMemResizeP(  type, p, eleCnt ) ((type*)cmMdAllocateDebug( p,    eleCnt, sizeof(type),     kAlignMmFl | kPreserveMmFl,             __FUNCTION__, __FILE__, __LINE__ )) 
#define cmMemResizePZ( type, p, eleCnt ) ((type*)cmMdAllocateDebug( p,    eleCnt, sizeof(type),     kAlignMmFl | kZeroMmFl | kPreserveMmFl, __FUNCTION__, __FILE__, __LINE__ ))
#define cmMemFree(     ptr )                     cmMdFreeDebug( ptr,                                                                  __FUNCTION__, __FILE__, __LINE__ )
#define cmMemPtrFree(  ptrPtr )                  cmMdFreePtrDebug( (void**)ptrPtr,                                                    __FUNCTION__, __FILE__, __LINE__ )

#define cmIsPadCorrupt( id )                     cmMdIsPadCorrupt(id)

  // Reports corrupt blocks and returns false if any corrupt blocks are found
#define cmCheckAllPads( file )                   cmMdCheckAllPads(file)

#endif


  //)


#ifdef __cplusplus
}
#endif


//}

#endif
