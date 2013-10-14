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
    kTestFailTmRC
  };

  typedef cmRC_t cmTmRC_t;

  typedef cmHandle_t cmTaskMgrH_t;

  extern cmTaskMgrH_t cmTaskMgrNullHangle;

  typedef enum
  {
    kInvalidTmId,
    kQueuedTmId,       // The task is waiting in the queue.
    kQueuedPausedTmId, // The task is waiting in the queue but is deferred.
    kStartedTmId,      // The task is running.
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
    void*               arg;           // 'funcArg' provided by cmTaskMgrCall();
    unsigned            argByteCnt;    // 'funcArgByteCnt' provided by cmTaskMgrCall()
    unsigned            instId;        // Task instance id.
    cmTaskMgrStatusCb_t statusCb;      // Status update function provided by cmTaskMgrCreate().
    void*               statusCbArg;   // Status update function arg. provided by cmTaskMgrCreate().
    unsigned            progCnt;       // Maximum expected value of cmTaskMgrStatusArg_t.prog during execution of this task instance.
    cmTaskMgrCtlId_t*   cmdIdPtr;      // Command id used to inform the running task that it should pause or exit.
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

  cmTmRC_t cmTaskMgrDestroy( cmTaskMgrH_t* hp );

  bool     cmTaskMgrIsValid(   cmTaskMgrH_t h );

  // Called by the client to give the task mgr an opportunity to execute
  // period functions from within the client thread.  Note that 'statusCb()'
  // (as passed to cmTaskMgrCreate()) is only called within this function
  // This guarantees that all communication with the client occurs in the 
  // clients thread.
  cmTmRC_t cmTaskMgrOnIdle( cmTaskMgrH_t h );

  // Pause/unpause the task mgr.
  bool     cmTaskMgrIsEnabled( cmTaskMgrH_t h );
  cmTmRC_t cmTaskMgrEnable(    cmTaskMgrH_t h, bool enableFl );

  // Install a task function and associate it with a label and unique id.
  cmTmRC_t cmTaskMgrInstall( cmTaskMgrH_t h, unsigned taskId, const cmChar_t* label, cmTaskMgrFunc_t func ); 

  // Queue a task.
  cmTmRC_t cmTaskMgrCall( 
    cmTaskMgrH_t h, 
    unsigned     taskId, 
    void*        funcArg, 
    unsigned     progCnt, 
    unsigned*    retInstIdPtr ); 

  // Start,pause, or kill a task.  
  // If a queued task is paused then it will remain at the front of the queue
  // and tasks behdind it in the queue will be executed.
  cmTmRC_t cmTaskMgrTaskCtl( cmTaskMgrH_t h, unsigned instId, cmTaskMgrCtlId_t ctlId );

  // Get the status of a task.
  cmStatusTmId_t cmTaskMgrStatus( cmTaskMgrH_t h, unsigned instId );

  
  const void* cmTaskMgrResult( cmTaskMgrH_t h, unsigned instId );
  unsigned    cmTaskMgrResultByteCount( cmTaskMgrH_t h, unsigned instId );
  cmTmRC_t    cmTaskMgrResultDelete(    cmTaskMgrH_t h, unsigned instId );

  cmTmRC_t cmTaskMgrTest(cmCtx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif
