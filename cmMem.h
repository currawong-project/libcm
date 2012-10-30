//{
//(
// The cmMem class implements a memory allocation manager interface.
//
//
// Using cmMem allows memory leaks and some instances of memory corruption 
// to be be detected. It can also perform memory block alignment.
// 
// The cmMm class acts as an interface for implementing functions designed to replace
// malloc() and free().  cmMm does not actually allocate memory itself but rather
// tracks and conditions block of memory provided by other sources.  In this sense 
// it acts as a backend for a memory allocation manager.  
// cmMallocDebug.h gives an example of using cmMm to interface to malloc() and free().
// cmLinkedHeap.h gives an example of using cmMm to link to an alternate heap manager.
// See cmMdTest() and cmLHeapTest() for usage examples of cmMm.
//
// cmMm works as follows:
//
// 1. A client memory manager creates and configures a cmMm object via cmMmInitialize().
// As part of the configuration the client gives callback functions which implement
// actual memory allocation and release.  In practice this means the callback probably
// call malloc() or free(). 
// 2. At some point later when the client needs to allocate a block of memory it calls
// cmMmAllocate() with the size of the requested block.  cmMm translates this request
// into a call to the client provided memory allocation callback to get a block of raw
// memory which is slightly larger than the request block.
// 3. Given the raw memory block cmMm conditions it in the following ways and returns
// it to the client.
// * The base of the blocks data area is shifted such that it is has an arbitrary 
// address aligned according to the value set by the alignByteCnt parameter to cmMmInitialize().
// Address aligment is sometimes required by routines which make use of the the SIMD
// unit on some CPUs.  
// * 'Guard' bytes are prepended and appended to the blocks data area. 
// These bytes are set to the known fixed value (0xaa).  At some point later cmMm can 
// then test for accidental writes just before or just after the legal data area by 
// checking the value of these guard bytes. 
// * The number of bytes allocated is written just prior to the leading guard bytes.
// This allows the memory manager to track the
// size of the memory and thereby makes reallocations() to smaller or equal data areas 
// very fast.  This also allows the size of the data area to be known just by having a 
// pointer to the data area (see cmMmByteCount()). This basic information is not availabe
// via malloc().
// * A record is added to an internal database to track the allocation code location 
// (file name, file line, function name) and the allocation status (active or released).
// * The client may request that a new block of memory be automatically filled with zeros.
// If automatic zeroing is not requested then the block is filled with 0x55 to indicate that
// it is not initialized.  This can be useful when attempting to recognize uninitialized
// memory during debugging.
//
// When a client requests that a block of memory is released cmMm does the following:
// 
// 1. If deferred release is enabled (kDeferFreeFl) then the block is filled with 0x33
// but the callback to freeFunc() is not actually made. This allows cmMm to track attempted
// writes to freed memory areas.  When deferred release is enabled the freeFunc() is not called
// on any blocks until cmMmFinalize().  If the program continually allocates memory over the 
// life of the program this may mean that the program will eventually exhaust physical memory.
// 2. If tracking is enabled (kTrackMmFl) then the block pointer is looked up in the internal database.
// If the pointer is not found then a kMissingRecdRC is returned indicating an attempt to release
// a non-allocated block.  
// 3. If tracking is enabled (kTrackMmFl) then the block is marked as released in the 
// internal tracking database. At the end of the program all blocks should be marked for release
// otherwise they are considered leaks.  
//
//
// At any time during the life of the cmMm object the client can request a report of the 
// allocated blocks cmMmReport(). This report examines each allocated block for corrupt guard bytes,
// double frees (attempts to release an allocated block that was already released), and
// leaked blocks (active blocks).
//
//)


#ifndef cmMem_h
#define cmMem_h

#ifdef __cplusplus
extern "C" {
#endif
  
  //(

  typedef cmHandle_t cmMmH_t;  //< cmMm handle type. 
  typedef cmRC_t     cmMmRC_t; //< cmMm result code types.

  // cmMm result codes 
  enum
  {
    kOkMmRC = cmOkRC,
    kObjAllocFailMmRC,
    kTrkAllocFailMmRC,
    kAllocFailMmRC,
    kFreeFailMmRC,
    kMissingRecdMmRC,
    kGuardCorruptMmRC,
    kWriteAfterFreeMmRC,
    kLeakDetectedMmRC,
    kDblFreeDetectedMmRC,
    kParamErrMmRC
  };

  // All cmMmH_t variables should be initialized with this value prior to calling cmMmInitialize().
  extern cmMmH_t cmMmNullHandle;

  // Function signature for data allocation routine client provided to cmMmInitialize().
  // Return NULL if byteCnt == 0.
  typedef void* (*cmAllocMmFunc_t)(void* funcArgPtr, unsigned byteCnt);

  // Function signature for data release routine client provided to cmMmInitialize().
  // Return true on success and false on failure.  Return true if ptr==NULL.
  typedef bool  (*cmFreeMmFunc_t)( void* funcArgPtr, void* ptr);

  // Flags for use with cmMmInitialize()
  enum
  {
    kTrackMmFl      = 0x01,   //< Track alloc's and free's for use by cmMmReport().
    kDeferFreeMmFl  = 0x02,   //< Defer memory release until cmMmFinalize() (ignored unless kTrackMmFl is set.)  Allows checks for 'write after release'.
    kFillUninitMmFl = 0x04,   //< Fill uninitialized (non-zeroed) memory with a 0x55 upon allocation
    kFillFreedMmFl  = 0x08    //< Fill freed memory with 0x33. This allow checks for wite-after-free.
  };

  // Create a new cmMm object.
  // If *hp was not initalized by an earlier call to cmMmInitialize() then it should 
  // be set to cmMmNullHandle prior to calling this function. If *hp is a valid handle
  // then it is automatically finalized by an internal call to cmMmFinalize() prior to
  // being re-iniitalized.
  cmMmRC_t cmMmInitialize( 
    cmMmH_t*        hp,           //< Pointer to a client provided cmMmH_t handle to recieve the handle of the new object.
    cmAllocMmFunc_t allocFunc,    //< The memory allocation function equivalent to malloc().
    cmFreeMmFunc_t  freeFunc,     //< The memory release function equivalent to free().
    void*           funcArgPtr,   //< An application supplied data value sent with call backs to allocFunc() and freeFunc().
    unsigned        guardByteCnt, //< Count of guardBytes to precede and follow each allocated block.
    unsigned        alignByteCnt, //< Address alignment to provide for each allocated block.
    unsigned        flags,        //< Configuration flags (See cmXXXMmFl).
    cmRpt_t*        rptPtr        //< Pointer to an error reporting object.
  );

  // Release a cmMm object created by an earlier call to cmMmInitialize(). Upon successful completion *hp is set to cmMmNullHandle.
  cmMmRC_t cmMmFinalize( cmMmH_t* hp );

  unsigned cmMmGuardByteCount(  cmMmH_t h ); //< Return the count of guard bytes this cmMm object is applying.
  unsigned cmMmAlignByteCount(  cmMmH_t h ); //< Return the byte alignment this cmMm object is applying.
  unsigned cmMmInitializeFlags( cmMmH_t h ); //< Return the configuration flags this cmMm object was initialized with.

  // Return true if 'h' is a valid handle for an existing cmMm object.
  bool     cmMmIsValid(  cmMmH_t h );

  // flags for use with cmMmAllocate()
  enum cmMmAllocFlags_t
  {
    kZeroMmFl     = 0x01, //< Initialize new memory area to zero.
    kAlignMmFl    = 0x02, //< Align the returned memory according to the alignByteCnt set in cmMmInitialize().
    kPreserveMmFl = 0x04  //< Preserve existing memory contents during reallocation (orgDataPtr!=NULL). 
  }; 

  // Allocate a block of memory.
  // Calling this function results in a call to the function named in allocFunc() in cmMmInitialize(). 
  void* cmMmAllocate(     
    cmMmH_t     h,                  //< Handle for this cmMm object returned from an earlier successful call to cmMmInitialize().
    void*       orgDataPtr,         //< If this is a re-allocation then this pointer should point to the original allocation otherwise it should be NULL.
    unsigned    newEleCnt,          //< Count of elmements in this allocation.
    unsigned    newEleByteCnt,      //< Bytes per element in this allocation. The total memory request is newEleCnt*newEleByteCnt.
    enum cmMmAllocFlags_t    flags, //< See cmMmAllocFlags_t.
    const char* fileName,           //< Name of the C file from which the allocation request is being made. 
    const char* funcName,           //< Name of the C function from which the allocation request is being made.
    unsigned    fileLine            //< Line in the C file on which the allocation request is being made.
                          );

  // Free memory pointed to by dataPtr.  
  // If dataPtr==NULL then the functon does nothing and returns.
  // Calling this function results in a call to the function named in freeFunc() in cmMmInitialize().
  // This is the release mode memory free routine. See cmMmFreeDebug() for the debug mode memory release routine. 
  //  See \ref debug_mode for more about debug vs. release mode.
  cmMmRC_t cmMmFree( cmMmH_t h, void* dataPtr );

  // Debug mode version of cmMmFree(). See cmMmFree() for the release mode memory free routine. 
  // See debug_mode for more about debug vs. release mode.  
  // This routine is functionally identical to the cmMmFree() but takes the calling
  // location information for use in tracking the block of memory.
  cmMmRC_t cmMmFreeDebug( cmMmH_t h, void* dataPtr, const char* fileName, const char* funcName, unsigned fileLine );

  // This function is identical to cmMmFree() but takes the address of the pointer
  // to the block of memory to free. Upon successful completion *dataPtrPtr is
  // set to NULL. In general this should be the preferred version of the free routine
  // because it helps to eliminate problems of reusing deallocated memory blocks.
  // Note that although dataPtrPtr must  point to a valid address  *dataPtrPtr may be NULL.
  // This routine is generally only used in the release compile mode.
  //  See cmMmFreePtrDebug() for the debug mode version.  See \ref debug_mode for more
  //  about compile vs. release mode.
  cmMmRC_t cmMmFreePtr(  cmMmH_t h, void** dataPtrPtr );

  // Debug compile mode version of cmMmFreePtr(). 
  // This function is functionally identical to cmMmFreePtr() but accepts information
  // on the location of the call to aid in debuging.
  cmMmRC_t cmMmFreePtrDebug( cmMmH_t h, void* dataPtr, const char* fileName, const char* funcName, unsigned fileLine );

  // Return the size of a memory block returned from cmMmAllocate().
  unsigned cmMmByteCount( cmMmH_t h, const void* dataPtr );

  // Return the unique id associated with an address returned from cmMmAllocate().
  unsigned cmMmDebugId( cmMmH_t h, const void* dataPtr);

  // Flags for use with cmMmReport().
  enum 
  {
    kSuppressSummaryMmFl = 0x01, //< Do not print a memory use summary report.
    kIgnoreNormalMmFl    = 0x02, //< Do not print information for non-leaked,non-corrupt memory blocks. 
    kIgnoreLeaksMmFl     = 0x04  //< Do not print information for leaked blocks. 
  
  };

  // Report on the memory tracking data. 
  // Returns kMmOkRC if no errors were found otherwise returns the error of the
  // last anomoly reported. 
  cmMmRC_t cmMmReport(   cmMmH_t h, unsigned flags );

  // Analyze the memory assoc'd with a specific tracking record for corruption.
  // Returns: kOkMmRC,kGuardCorruptMmRC,kWriteAfterFreeMmRc, or kMissingRecdMmRC.
  // This function is only useful if kTrackMmFl was set in cmMmInitialize().
  // Write-after-free errors are only detectable if kDeferFreeMmFl was set in cmMmInitialize().
  cmMmRC_t cmMmIsGuardCorrupt( cmMmH_t h, unsigned id );

  // Check all tracking records by calling cmMmmIsGuardCorrupt() on each record.
  cmMmRC_t cmMmCheckAllGuards( cmMmH_t h );

  //)
  //}
#ifdef __cplusplus
}
#endif

#endif
