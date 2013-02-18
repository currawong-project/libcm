#ifndef cmStack_h
#define cmStack_h

#ifdef __cplusplus
extern "C" {
#endif

  enum
  {
    kOkStRC = cmOkRC,
    kLHeapFailStRC,
    kInvalidIdxStRC,
    kUnderflowStRC
  };

  typedef cmRC_t     cmStRC_t;
  typedef cmHandle_t cmStackH_t;

  extern cmStackH_t cmStackNullHandle;

  // Allocate a stack to hold data elements each of size 'eleByteCnt'.
  // The stack will be initialized with 'initCnt' empty slots. Once these
  // slots are filled 'expandCnt' additional slots will be added as necessary.
  cmStRC_t    cmStackAlloc( cmCtx_t* ctx, cmStackH_t* hp, unsigned initCnt, unsigned expandCnt, unsigned eleByteCnt );
  cmStRC_t    cmStackFree(    cmStackH_t* hp );
  cmStRC_t    cmStackIsValid( cmStackH_t h );

  // Return the current count of elements on the stack.
  unsigned    cmStackCount(   cmStackH_t h );

  // Empty the stack. Set release flag to also release any memory used by the data elements.
  void        cmStackClear(   cmStackH_t h, bool releaseFl );

  // Push 'dataEleCnt' elments onto the stack.
  cmStRC_t    cmStackPush(    cmStackH_t h, const void* data, unsigned dataEleCnt );

  // Remove 'eleCnt' elements from the stack.
  cmStRC_t    cmStackPop(     cmStackH_t h, unsigned eleCnt );

  // Return a pointer to the top element on the stack. This is the one which will be
  // lost with the next call to cmStackPop(h,1).
  const void* cmStackTop(     cmStackH_t h );

  // Set the value of 'dataEleCnt' elements on the stack.
  cmStRC_t    cmStackSet(     cmStackH_t h, unsigned index, const void* data, unsigned dataEleCnt );

  // Copy 'dataEleCnt' elements into the buffer pointed to by 'data'.
  cmStRC_t    cmStackGetN(    cmStackH_t h, unsigned index, void* data, unsigned dataEleCnt );

  // Return a pointer to a single element on the stack.
  const void* cmStackGet(     cmStackH_t h, unsigned index );

  // Convert the internal representation of the stack to a linear array and return
  // a pointer to the array base.
  void*       cmStackFlatten( cmStackH_t h );

  // Stack test function.
  void        cmStackTest(    cmCtx_t* ctx );

#define cmStackEle(h,t,i) (*(t*)cmStackGet(h,i))


#ifdef __cplusplus
}
#endif

#endif
