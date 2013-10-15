#ifndef cmTaskMgr_h
#define cmTaskMgr_h

#ifdef __cplusplus
extern "C" {
#endif

  enum
  {
    kOkTmRC,
    kThreadFailTmRC,
    kInvalidArgTmRC,
    kOpFailTmRC,
    kQueueFailTmRC,
    kAssertFailTmRC,
    kTestFailTmRC
  };

  typedef cmRC_t cmTmRC_t;

  typedef cmHandle_t cmTaskMgrH_t;

  extern cmTaskMgrH_t cmTaskMgrNullHangle;

  typedef enum
  {
    kInvalidTmId,
    kQueuedTmId,       // The task is waiting in the queue.
    kStartedTmId,      // The task is running.
    kPausedTmId,       // The task is paused.
    kCompletedTmId,    // The task successfully completed.
    kKilledTmId        // The task was killed by the client.
  } cmStatusTmId_t;

  typedef enum
  {
    kStatusTmId,       // Task status updates. These are automatically sent by the system when the task instance changes state.
    kProgTmId,         // Task progress update. The user function should increment the 'prog' toward 'progCnt'.
    kErrorTmId         // 
  } cmSelTmId_t;

  typedef enum 
  { 
    kNoneTmId,
    kStartTmId, 
    kPauseTmId, 
    kKillTmId 
  } cmTaskMgrCtlId_t;


  typedef struct cmTaskMgrStatusArg_str
  {
    void*           arg;
    unsigned        instId;
    cmSelTmId_t     selId;
    cmStatusTmId_t  statusId;
    unsigned        prog;
    const cmChar_t* msg;
    void*           result;
    unsigned        resultByteCnt;
  } cmTaskMgrStatusArg_t;

  typedef void (*cmTaskMgrStatusCb_t)( const cmTaskMgrStatusArg_t* status  );

  typedef struct cmTaskMgrFuncArg_str
  {
    void*               reserved; 
    void*               arg;           // 'funcArg' provided by cmTaskMgrCall();
    unsigned            argByteCnt;    // 'funcArgByteCnt' provided by cmTaskMgrCall()
    unsigned            instId;        // Task instance id.
    cmTaskMgrStatusCb_t statusCb;      // Status update function provided by cmTaskMgrCreate().
    void*               statusCbArg;   // Status update function arg. provided by cmTaskMgrCreate().
    unsigned            progCnt;       // Maximum expected value of cmTaskMgrStatusArg_t.prog during execution of this task instance.
    unsigned            pauseSleepMs;  // Length of time to sleep if the task receives a pause command.
  } cmTaskMgrFuncArg_t;

  typedef void (*cmTaskMgrFunc_t)(cmTaskMgrFuncArg_t* arg );

  cmTmRC_t cmTaskMgrCreate(  
    cmCtx_t*             ctx, 
    cmTaskMgrH_t*        hp, 
    cmTaskMgrStatusCb_t  statusCb, 
    void*                statusCbArg, 
    unsigned             threadCnt,
    unsigned             queueByteCnt,
    unsigned             pauseSleepMs );

  // Calling cmTaskMgrDestroy() will send a 'kill' control message
  // to any existing worker threads.  The threads will shutdown
  // gracefully but the task they were computing will not be completed.
  cmTmRC_t cmTaskMgrDestroy( cmTaskMgrH_t* hp );

  // Return true if the task manager handle is valid.
  bool     cmTaskMgrIsValid(   cmTaskMgrH_t h );

  // Called by the client to give the task mgr an opportunity to execute
  // period functions from within the client thread.  Note that 'statusCb()'
  // (as passed to cmTaskMgrCreate()) is only called within this function
  // This guarantees that all communication with the client occurs in the 
  // clients thread.
  cmTmRC_t cmTaskMgrOnIdle( cmTaskMgrH_t h );

  // Pause/unpause the task mgr.
  // This function pauses/unpauses each worker thread and then the master thread.
  // If waitFl is set then the function will not return until each of
  // the threads has entered the requested state. If 'waitFl' is false
  // The function will wait for the worker threads to pause but will
  // only signal the master thread to pause before returning.
  bool     cmTaskMgrIsEnabled( cmTaskMgrH_t h );
  cmTmRC_t cmTaskMgrEnable(    cmTaskMgrH_t h, bool enableFl );

  // Install a task function and associate it with a label and unique id.
  cmTmRC_t cmTaskMgrInstall( cmTaskMgrH_t h, unsigned taskId, const cmChar_t* label, cmTaskMgrFunc_t func ); 

  // Queue a new task instance.
  // The 'queued' status callback occurs from inside this call.
  cmTmRC_t cmTaskMgrCall( 
    cmTaskMgrH_t h, 
    unsigned     taskId, 
    void*        funcArg, 
    unsigned     progCnt, 
    unsigned*    retInstIdPtr ); 

  // Start,pause, or kill a task instance.  
  // If a queued task is paused then it will remain at the front
  // of the queue and tasks behdind it in the queue will be executed.
  cmTmRC_t cmTaskMgrTaskCtl( cmTaskMgrH_t h, unsigned instId, cmTaskMgrCtlId_t ctlId );

  // Get the status of a task.
  cmStatusTmId_t cmTaskMgrStatus( cmTaskMgrH_t h, unsigned instId );

  // 
  const void* cmTaskMgrResult(          cmTaskMgrH_t h, unsigned instId );
  unsigned    cmTaskMgrResultByteCount( cmTaskMgrH_t h, unsigned instId );
  cmTmRC_t    cmTaskMgrInstDelete(      cmTaskMgrH_t h, unsigned instId );


  // -----------------------------------------------------------------------------------
  // Worker thread helper functions.
  cmTaskMgrCtlId_t cmTaskMgrHandleCommand( cmTaskMgrFuncArg_t* a );
  cmTaskMgrCtlId_t cmTaskMgrSendStatus( cmTaskMgrFuncArg_t* a, cmStatusTmId_t statusId );
  cmTaskMgrCtlId_t cmTaskMgrSendProgress( cmTaskMgrFuncArg_t* a, unsigned prog );

  cmTmRC_t cmTaskMgrTest(cmCtx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif
