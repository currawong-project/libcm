#ifndef cmThread_h
#define cmThread_h

#ifdef __cplusplus
extern "C" {
#endif

  typedef cmHandle_t cmThreadH_t;
  typedef unsigned   cmThRC_t;

  enum
  {
    kOkThRC = cmOkRC,         // 0
    kCreateFailThRC,     // 1
    kDestroyFailThRC,    // 2
    kTimeOutThRC,        // 3
    kInvalidHandleThRC,  // 4
    kLockFailThRC,       // 5
    kUnlockFailThRC,     // 6
    kCVarWaitFailThRC,   // 7
    kCVarSignalFailThRC, // 8
    kBufFullThRC,        // 9
    kBufEmptyThRC,       // 10
    kBufTooSmallThRC     // 11

  };

  typedef enum 
  {
    kNotInitThId,
    kPausedThId,
    kRunningThId,
    kExitedThId   
  } cmThStateId_t;


  // Return 'false' to indicate that the thread should terminate 
  // otherwise return 'true'
  typedef bool (*cmThreadFunc_t)(void* param);

  extern cmThreadH_t cmThreadNullHandle;

  // Create a thread. The thread is automatically set to the 'paused' state.
  cmThRC_t      cmThreadCreate(  cmThreadH_t* hPtr, cmThreadFunc_t cmThreadFuncPtr,  void* funcParam, cmRpt_t* rpt );

  // Release the resources associated with a thread previously created with cmThreadCreate().
  cmThRC_t      cmThreadDestroy( cmThreadH_t* hPtr );

  enum 
  { 
    kPauseThFl = 0x01,  // set to pause, clear to unpause
    kWaitThFl  = 0x02   // set to wait for thread to pause/unpause prior to returning.
  };

  // Pause or unpause a thread.  Set kWaitThFl to wait for the thread to be
  // paused or unpaused prior to returning. 
  cmThRC_t      cmThreadPause(   cmThreadH_t h, unsigned cmdFlags );

  // Return the current thread state.
  cmThStateId_t cmThreadState(   cmThreadH_t h );
  bool          cmThreadIsValid( cmThreadH_t h);

  // The Pause time out gives the period in microseconds which the thread will
  // sleep while it is paused.  In other words the thread will wake up
  // every 'pause time out micro-seconds' to check to see if it has been
  // requested to leave the paused state. Default:50000.
  unsigned      cmThreadPauseTimeOutMicros( cmThreadH_t h );
  void          cmThreadSetPauseTimeOutMicros( cmThreadH_t h, unsigned usecs );

  // The wait time out gives the length of time the thread should expect to 
  // wait in order change states.  This value should always be greater than
  // or equal to the pause time out and the expected length of time the 
  // client callback function will run. 
  // This timeout comes into play in two situations:
  // 1) This is the maximum length of time that cmThreadPause() will wait for
  // the thread to enter/leave the pause state when the kWaitThFl has been set.
  // If the thread does not enter/leave the pause state in this amount of time
  // then cmThreadPause() returns the error code kTimeOutThRC.
  // 2) This is the maximum length of time the cmThreadDestroy() wll wait for
  // the thread to enter the 'exited' state after being requested to destroy 
  // itself. If this time period expires then cmThreadDestroy() returns
  // kTimeOutThRC.
  // Default:1000000.
  unsigned      cmThreadWaitTimeOutMicros(  cmThreadH_t h );
  void          cmThreadSetWaitTimeOutMicros( cmThreadH_t h, unsigned usecs );

  void          cmThreadTest( cmRpt_t* rpt );


  typedef struct
  {
    void* h;
  } cmThreadMutexH_t;

  extern cmThreadMutexH_t kCmThreadMutexNULL;

  cmThRC_t cmThreadMutexCreate(   cmThreadMutexH_t* hPtr, cmRpt_t* rpt );
  cmThRC_t cmThreadMutexDestroy(  cmThreadMutexH_t* hPtr );
  cmThRC_t cmThreadMutexTryLock(  cmThreadMutexH_t  h, bool* lockFlPtr );
  cmThRC_t cmThreadMutexLock(     cmThreadMutexH_t  h );
  cmThRC_t cmThreadMutexUnlock(   cmThreadMutexH_t  h );
  bool     cmThreadMutexIsValid(  cmThreadMutexH_t  h );

  // Set 'lockFl' if the function should lock the mutex prior to waiting.
  // If 'lockFl' is false then the function assumes the mutex is already locked
  // and directly waits. If 'lockFl' is set and the mutex is not already locked
  // then the result is undefined.
  cmThRC_t cmThreadMutexWaitOnCondVar( cmThreadMutexH_t h, bool lockFl );
  cmThRC_t cmThreadMutexSignalCondVar( cmThreadMutexH_t h );



  // cmThread safe message queue.
  //
  // This object is intended as a way to serialize one-way
  // communication between multiple sender threads and one 
  // receiver thread. The object is implemented as 
  // a double buffer.  One buffer acts as
  // an input queue the the other buffer acts as an
  // output queue. When the output queue is empty the buffers
  // are swapped. Any pending messages in the input queue
  // then become available to the receiver in the output queue.
  //
  // An internal mutex prevents the queue logic from becoming
  // corrupted.  The mutex is locked during the entire enqueue
  // operation. The enqueue operation may therefore block its
  // thread while waiting for mutex access.  The dequeue operation
  // only locks the mutex when the current output buffer is
  // empty, the input buffer contains messages, and the mutex
  // is not already locked. The mutex only remains locked for the
  // period of time necessary to switch the input and output
  // buffer pointers. The mutex is not locked during the actual
  // dequeue copy or transmit.
  //
  // Given this logic the dequeue thread should never
  // block because it only locks the mutex when it is not already
  // locked. The enqueue thread will only block when it happens to collide 
  // with the dequeue buffer switch operation or an enqueue operation
  // from another thread. If it happens that there is only a single
  // sender thread then the sender will virtually never block because
  // the dequeue lock is only maintained for a very short period of time.  
  //

  typedef cmHandle_t cmTsQueueH_t;

  extern cmTsQueueH_t cmTsQueueNullHandle;

  typedef cmRC_t (*cmTsQueueCb_t)(void* userCbPtr, unsigned msgByteCnt, const void* msgDataPtr );

  // Set 'cbFunc' to NULL if the dequeue callback option will not be used.
  cmThRC_t   cmTsQueueCreate(     cmTsQueueH_t* hPtr, unsigned bufByteCnt, cmTsQueueCb_t cbFunc, void* cbArg, cmRpt_t* rpt );
  cmThRC_t   cmTsQueueDestroy(    cmTsQueueH_t* hPtr );

  // Set or clear the dequeue callback option after the queue was created.
  cmThRC_t   cmTsQueueSetCallback( cmTsQueueH_t h, cmTsQueueCb_t cbFunc, void* cbArg );

  // Copy a msg into the queue. This function return kBufFullThRC if the buffer is full.
  // This interface allows the message to be formed from a concatenation of 'arrayCnt' segments.
  cmThRC_t   cmTsQueueEnqueueSegMsg( cmTsQueueH_t h, const void* msgPtrArray[], unsigned msgByteCntArray[], unsigned arrayCnt );

  // Copy a msg onto the queue.  This function is written in terms of cmTsQueueEnqueueSegMsg().
  cmThRC_t   cmTsQueueEnqueueMsg( cmTsQueueH_t  h, const void* dataPtr, unsigned byteCnt );

  // Prepend 'id' to the bytes contained in 'dataPtr[byteCnt]' and enqueue the resulting msg.
  // This function is written in terms of cmTesQueueEnqueueSegMsg().
  cmThRC_t   cmTsQueueEnqueueIdMsg( cmTsQueueH_t h, unsigned id, const void* dataPtr, unsigned byteCnt );

  // Total size of the queue buffer.
  unsigned cmTsQueueAllocByteCount( cmTsQueueH_t h );

  // Bytes available to enqueue the next message.
  unsigned cmTsQueueAvailByteCount( cmTsQueueH_t h );

  // Remove one msg from the queue.  
  // If 'dataPtr' is not NULL the msg is copied into the buffer it points to.
  // If 'cbFunc' in the earlier call to cmTsQueueCreate() was not NULL then
  // the msg is transmitted via the callback.
  // This function should only be called from the deque thread.
  cmThRC_t   cmTsQueueDequeueMsg( cmTsQueueH_t  h,  void* dataPtr, unsigned byteCnt );

  // thQueueMsgWaiting() returns true if there is a msg available
  // to dequeue.  This function should only be called from the
  // deque thread.
  bool     cmTsQueueMsgWaiting( cmTsQueueH_t h );

  // Return the size in bytes of the next msg to dequeue or zero
  // if no msgs are waiting. The function should only be called from the
  // deque thread.
  unsigned cmTsQueueDequeueMsgByteCount( cmTsQueueH_t h );
 
  bool     cmTsQueueIsValid(    cmTsQueueH_t h);


  // Single producer / Single consumer thread-safe queue.
  // These functions have identical semantics and return values
  // to the same named cmTsQueueXXXX() calls above.

  typedef cmHandle_t cmTs1p1cH_t;

  extern cmTs1p1cH_t cmTs1p1cNullHandle;

  cmThRC_t   cmTs1p1cCreate(     cmTs1p1cH_t* hPtr, unsigned bufByteCnt, cmTsQueueCb_t cbFunc, void* cbArg, cmRpt_t* rpt );

  cmThRC_t   cmTs1p1cDestroy(    cmTs1p1cH_t* hPtr );

  cmThRC_t   cmTs1p1cSetCallback( cmTs1p1cH_t h, cmTsQueueCb_t cbFunc, void* cbArg );

  cmThRC_t   cmTs1p1cEnqueueSegMsg( cmTs1p1cH_t h, const void* msgPtrArray[], unsigned msgByteCntArray[], unsigned arrayCnt );

  cmThRC_t   cmTs1p1cEnqueueMsg( cmTs1p1cH_t  h, const void* dataPtr, unsigned byteCnt );

  unsigned   cmTs1p1cAllocByteCount( cmTs1p1cH_t h );

  unsigned   cmTs1p1cAvailByteCount( cmTs1p1cH_t h );

  cmThRC_t   cmTs1p1cDequeueMsg( cmTs1p1cH_t  h,  void* dataPtr, unsigned byteCnt );

  bool       cmTs1p1cMsgWaiting( cmTs1p1cH_t h );

  unsigned   cmTs1p1cDequeueMsgByteCount( cmTs1p1cH_t h );
  
  bool       cmTs1p1cIsValid( cmTs1p1cH_t h );  


  // Thread safe compare-and-swap (actualy compare-and-test). 
  // Returns true if the *addr==new when the function returns 
  // otherwise returns false.
  bool     cmThIntCAS(   int*      addr, int      old, int      neww );
  bool     cmThUIntCAS(  unsigned* addr, unsigned old, unsigned neww );
  bool     cmThFloatCAS( float*    addr, float    old, float    neww );

  // Note: voidPtrPtr is must really be a pointer to a pointer.
  bool     cmThPtrCAS(   void* voidPtrPtr, void*    old, void*    neww );

  // Thread safe increment and decrement implemented in terms of
  // cmThXXXCAS().
  void     cmThIntIncr(  int*      addr, int      incr );
  void     cmThUIntIncr( unsigned* addr, unsigned incr );
  void     cmThFloatIncr(float*    addr, float    incr );
  void     cmThIntDecr(  int*      addr, int      decr );
  void     cmThUIntDecr( unsigned* addr, unsigned decr );
  void     cmThFloatDecr(float*    addr, float    decr );

  // Multiple producer / Single consumer thread-safe queue.
  // These functions have identical semantics and return values
  // to the same named cmTsQueueXXXX() calls above.

  typedef cmHandle_t cmTsMp1cH_t;

  extern cmTsMp1cH_t cmTsMp1cNullHandle;

  cmThRC_t   cmTsMp1cCreate(     cmTsMp1cH_t* hPtr, unsigned bufByteCnt, cmTsQueueCb_t cbFunc, void* cbArg, cmRpt_t* rpt );
  cmThRC_t   cmTsMp1cDestroy(    cmTsMp1cH_t* hPtr );

  void          cmTsMp1cSetCbFunc( cmTsMp1cH_t h,  cmTsQueueCb_t cbFunc, void* cbArg );
  cmTsQueueCb_t cmTsMp1cCbFunc(    cmTsMp1cH_t h );
  void*         cmTsMp1cCbArg(     cmTsMp1cH_t h );

  cmThRC_t   cmTsMp1cEnqueueSegMsg( cmTsMp1cH_t h, const void* msgPtrArray[], unsigned msgByteCntArray[], unsigned arrayCnt );

  cmThRC_t   cmTsMp1cEnqueueMsg( cmTsMp1cH_t  h, const void* dataPtr, unsigned byteCnt );

  unsigned   cmTsMp1cAllocByteCount( cmTsMp1cH_t h );

  unsigned   cmTsMp1cAvailByteCount( cmTsMp1cH_t h );

  cmThRC_t   cmTsMp1cDequeueMsg( cmTsMp1cH_t  h,  void* dataPtr, unsigned byteCnt );

  bool       cmTsMp1cMsgWaiting( cmTsMp1cH_t h );

  unsigned   cmTsMp1cDequeueMsgByteCount( cmTsMp1cH_t h );
  
  bool       cmTsMp1cIsValid( cmTsMp1cH_t h );  


  // Sleep functions
  void cmSleepUs( unsigned microseconds );
  void cmSleepMs( unsigned milliseconds );


  void cmTsQueueTest( cmRpt_t* rpt );
  void cmTs1p1cTest( cmRpt_t* rpt );
  void cmTsMp1cTest( cmRpt_t* rpt );
#ifdef __cplusplus
}
#endif

#endif
