#ifndef cmTaskMgr_h
#define cmTaskMgr_h

#ifdef __cplusplus
extern "C" {
#endif

  /*( { file_desc:"Task manager for controlling and monitoring tasks running in independent thread." kw:[parallel]}

    Usage:
    1) Use cmTaskMgrInstall() to register a worker function 
    (cmTaskMgrFunc_t) with the task manager.

    2) Use cmTaskMgrCall() to queue a new instance of a
    task to run.

    3) An internal scheduling program will start the task
    when there are less than  'maxActiveTaskCnt' running.
    This will occur when active tasks complete or are
    paused. 

    When 'maxActiveTaskCnt' tasks are active and
    a previously paused task is unpaused the unpaused
    task will take precedence over tasks started after
    it and one of these new tasks will be deactivated.

    As tasks are pause/unpaused and activated/deactivated
    the number of active tasks may briefly exceed 
    'maxActiveTaskCnt'.

    4) Once a task is instantiated the task manager
    will keep the client notified of the task status
    via callbacks to the cmTaskMgrStatusCb_t function.

    Status:
    kQueuedTmId      - The task instance is waiting to be 
                       exectued in the task queue.

    kStartedTmId     - The task has been made active and 
                       is running.

    kPausedTmId      - The client sent a kPauseTmId to the
                       task via cmTaskMgrCtl() and the task
                       has been paused.  The only way to 
                       restart the task is to send kStartTmId
                       command.

    kDeactivatedTmId - The task was previously started but has
                       now been deactivated by the system 
                       in an effort to keep the number of 
                       activate tasks at 'maxActiveTaskCnt'.

    kKilledTmId      - The task was killed either by the client
                       (via a cmTaskMgrCtl(kKillTmId)) or
                       or by the system within cmTaskMgrClose()
                       or cmTaskMgrDestroy().

    kCompletedTmId   - The task has completed. Note that any
                       task that gets queued is guaranteed to
                       generate kCompletedTmId status callback.
    
    5) Closing the task manager should follow a two step
    process. First all active tasks should be ended and then
    cmTaskMgrDestroy() should be called.

    Ending all active tasks can be accomplished by sending
    kill signals via cmTaskMgrCtl(), use of cmTaskMgrClose()
    or if all tasks are guaranteed to eventually end - waiting
    for the task count to go to zero (See cmTaskMgrActiveTaskCount()).
    Note that waiting for the task count to go to zero may be
    error prone unless one can guarantee that no tasks are 
    queued or that the last task started has completed.
   */

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

  extern cmTaskMgrH_t cmTaskMgrNullHandle;

  typedef enum
  {
    kInvalidTmId,
    kQueuedTmId,       // The task is waiting in the queue.
    kStartedTmId,      // The task is running.
    kPausedTmId,       // The task is paused.
    kDeactivatedTmId,  // The task was temporarily deactivated by the system
    kCompletedTmId,    // The task successfully completed.
    kKilledTmId        // The task was killed by the client.
  } cmStatusTmId_t;

  typedef enum
  {
    kStatusTmId,       // Task status updates. These are automatically sent by the system when the task instance changes state.
    kProgTmId,         // Task progress update. The user function should increment the 'prog' toward 'progCnt'.
    kErrorTmId,        // Error message ('cmTaskMgrStatusArg_t.prog' has error result code)
    kMsgTmId           // Msg from a task instance in cmTaskMgrStatusArg_t.msg[msgByteCnt].
  } cmSelTmId_t;

  typedef enum 
  { 
    kStartTmId, 
    kPauseTmId, 
    kKillTmId 
  } cmTaskMgrCtlId_t;


  typedef struct cmTaskMgrStatusArg_str
  {
    void*           arg;       // Client status arg. as passed to cmTaskMgrCreate().
    unsigned        instId;    // Task instance id of the task which generated the callback.
    cmSelTmId_t     selId;     // See cmSelTmId_t.
    cmStatusTmId_t  statusId;  // See cmStatusTmId_t.
    unsigned        prog;      // selId==kProgTmId (0<=prog<=cmTaskMgrFuncArg_t.progCnt) selId=kErrorTmid prog=error result code.
    const cmChar_t* text;      // Used by kErrorTmId.
    const void*     msg;       // Used by kMsgTmId. msg[msgByteCnt]
    unsigned        msgByteCnt; // Count of bytes in msg[].
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

  // Task process function.
  typedef void (*cmTaskMgrFunc_t)(cmTaskMgrFuncArg_t* arg );

  // Task message receive function.
  typedef void (*cmTaskMgrRecv_t)(cmTaskMgrFuncArg_t* arg, const void* msg, unsigned msgByteCnt );

  // Allocate the task manager.
  cmTmRC_t cmTaskMgrCreate(  
    cmCtx_t*             ctx,                // 
    cmTaskMgrH_t*        hp,                 // 
    cmTaskMgrStatusCb_t  statusCb,           // Task status callbacks.
    void*                statusCbArg,        // Status callback arg
    unsigned             maxActiveTaskCnt,   // Max. number of active tasks (see Usage notes above.)
    unsigned             queueByteCnt,       // Size of task client->taskMgr and taskMgr->client msg queues.
    unsigned             pauseSleepMs );     // Scheduler sleep time. (20-50ms)

  // Calling cmTaskMgrDestroy() will send a 'kill' control message
  // to any existing worker threads.  The threads will shutdown
  // gracefully but the task they were computing will not be completed.
  cmTmRC_t cmTaskMgrDestroy( cmTaskMgrH_t* hp );

  enum
  {
    kKillQueuedTmFl  = 0x01, // Kill any queued (otherwise queued tasks will be started)
    kTimeOutKillTmFl = 0x02  // KIll any instance not completed before returning
  };

  // Wait for the current task instances to complete.
  // Set 'timeOutMs' to the number of milliseconds to wait for the current tasks to complete.
  // Set kKillQueueTmFl to kill any queued tasks otherwise queued tasks will run as usual.
  // Set kRefuseCallTmFl to refuse to queue any new tasks following the return from this function.
  // Set kTimeOutKillTmFl to kill any tasks that are still running after timeOutMs has expired
  // otherwise these tasks will still be running when the function returns.
  cmTmRC_t cmTaskMgrClose( cmTaskMgrH_t h, unsigned flags, unsigned timeOutMs );

  // Return the current number of active tasks.
  unsigned cmTaskMgrActiveTaskCount( cmTaskMgrH_t h );

  // Return true if the task manager handle is valid.
  bool     cmTaskMgrIsValid(   cmTaskMgrH_t h );

  // Given a statusId return a the associated label.
  const cmChar_t* cmTaskMgrStatusIdToLabel( cmStatusTmId_t statusId );

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
  cmTmRC_t cmTaskMgrInstall( 
    cmTaskMgrH_t    h, 
    unsigned        taskId, // Unique task id.
    const cmChar_t* label,  // (optional) Task label
    cmTaskMgrFunc_t func,   // Task worker function.
    cmTaskMgrRecv_t recv);  //  (optional) Task message receive function.

  // Queue a new task instance.
  // The 'queued' status callback occurs from inside this call.
  // If this function completes successfully then the client is
  // guaranteed to get both a 'kQueuedTmId' status update and
  // and a 'kCompletedTmId' update.  Any per task instance cleanup
  // can therefore be triggered by the 'kCompleteTmId' status callback.
  cmTmRC_t cmTaskMgrCall( 
    cmTaskMgrH_t    h, 
    unsigned        taskId,         // Task id of a task previously registered by cmTaskMgrInstall().
    void*           funcArg,        // The value assigned to cmTaskMgrFuncArg_t.arg for this instance. 
    unsigned        progCnt,        // Max. expected progress value to be eventually reported by this task instances 'kProgTmId' progress updates.
    unsigned        queueByteCnt,   // Size of the client->task message buffer.
    const cmChar_t* label,          // (optional) Instance label.
    unsigned*       retInstIdPtr ); // (optional) Unique id assigned to this instance.

  // Start,pause, or kill a task instance.  
  //
  // If a queued task is paused then it will remain at the front
  // of the queue and tasks behind it in the queue may be executed.
  // See the usage note above regarding the interaction between 
  // pausing/unpausing, activating/deactivating and the 
  // maxActiveTaskCnt parameter.
  //
  // Note that killing a task causes it to terminate  quickly
  // but this does not imply that the task ends in an uncontrolled
  // manner.  Even killed tasks properly release any resource they
  // may hold prior to ending. For long running tasks which do not
  // have a natural stopping point prior to the end of the client
  // process using the kill signal to end the task is both convenient
  // and efficient.
  //
  // Once a task is paused it is safe to directly interact with 
  // the data space it has access to (e.g. cmTaskMgrFuncArg_t.funcArg).
  // This is true because the client has control over when the task
  // may start again via the cmStartTmId signal.  Note that this is
  // not the case for deactivated signals.  Deactivated signals may
  // be restarted at any time.  Note however that it is possible to
  // pause a deactivated task in which case it's data space may
  // be accessed as soon as the client is notified that the task
  // has switched to the paused state.
  cmTmRC_t cmTaskMgrCtl( cmTaskMgrH_t h, unsigned instId, cmTaskMgrCtlId_t ctlId );

  // Get the status of a task.
  cmStatusTmId_t cmTaskMgrStatus( cmTaskMgrH_t h, unsigned instId );

  // Send a thread-safe msg to a task instance.
  cmTmRC_t    cmTaskMgrSendMsg( cmTaskMgrH_t h, unsigned instId, const void* msg, unsigned msgByteCnt );

  const cmChar_t* cmTaskMgrTaskIdToLabel( cmTaskMgrH_t h, unsigned taskId );
  const cmChar_t* cmTaskMgrInstIdToLabel( cmTaskMgrH_t h, unsigned instId );

  // The following functions are only valid when the task has completed and
  // has a status of either kCompletedTmId.  
  const void* cmTaskMgrResult(          cmTaskMgrH_t h, unsigned instId );
  unsigned    cmTaskMgrResultByteCount( cmTaskMgrH_t h, unsigned instId );
  void*       cmTaskMgrFuncArg(         cmTaskMgrH_t h, unsigned instId );
  cmTmRC_t    cmTaskMgrInstDelete(      cmTaskMgrH_t h, unsigned instId );


  // -----------------------------------------------------------------------------------
  // Worker thread helper functions.
  //
  // These functions should only be called from inside the client supplied
  // worker function (cmTaskMgrFuncArg_t).
  //

  // There are two thread-safe methods the worker thread can use to receive
  // raw byte messages from the client.
  //
  // 1. Assign a cmTaskMgrRecv_t function at task installation
  //    (See cmTaskMgrInstall().) This callback will be invoked from inside
  //    cmTaskMgrWorkerHandleCommand()
  //    if a client message is found to be waiting.  When this occurs the 
  //    worker helper function will return kRecvTmwRC. 
  //
  // 2. If the task was not assigned a cmTaskMgrRect_t function then
  //    the worker should notice when 
  //    cmTaskMgrWorkerHandleCommand() returns cmRecvTmwRC.  When this occurs
  //    it should call cmTaskMgrMsgRecv() to copy the message into a
  //    locally allocated buffer.  cmTaskMgrWorkerMsgByteCount() can
  //    be called to find out the size of the message and therefore
  //    the minimum size of the copy destination buffer.

  typedef enum
  {
    kOkTmwRC,
    kStopTmwRC,
    kRecvTmwRC
  } cmTmWorkerRC_t;

  // The instance function cmTaskMgrFunc_t must  poll this 
  // function to respond to incoming commands and messages.
  // The polling rate should be determined by the application to 
  // trade-off the cost of the poll versus the command execution
  // latency.  Checking every 20 to 100 milliseconcds is probably
  // about right.
  // If the function returns 'kStopTmwRC' then the function has received
  // a kKillCtlId and should release any resources and return immediately.
  // If the function returns 'kRecvTmwRC' and has not registered a 
  // cmTaskMgrRecv_t function then it should call cmTaskMgrWorkerRecv()
  // to pick up an incoming message from the client.
  // If the function returns 'kRecvTmwRC' and does have a registered
  // cmTaskMgrRecv_t function then a new message was received by the
  // cmTaskMGrRecv_t function.
  cmTmWorkerRC_t cmTaskMgrWorkerHandleCommand( cmTaskMgrFuncArg_t* a );

  // Send a task status update to the client.
  //cmTmRC_t       cmTaskMgrWorkerSendStatus(    cmTaskMgrFuncArg_t* a, cmStatusTmId_t statusId );
  
  // Send a progress update to the client.
  cmTmRC_t       cmTaskMgrWorkerSendProgress(  cmTaskMgrFuncArg_t* a, unsigned prog, const cmChar_t* text );
  cmTmRC_t       cmTaskMgrWorkerSendProgressV( cmTaskMgrFuncArg_t* a, unsigned prog, const cmChar_t* fmt, va_list vl );
  cmTmRC_t       cmTaskMgrWorkerSendProgressF( cmTaskMgrFuncArg_t* a, unsigned prog, const cmChar_t* fmt, ... );

  // Send an error message to the client. The result code is application 
  // dependent.  These function return 'rc' as does cmErrMsg().
  cmTmRC_t       cmTaskMgrWorkerError(  cmTaskMgrFuncArg_t* a, unsigned rc, const cmChar_t* text );
  cmTmRC_t       cmTaskMgrWorkerErrorV( cmTaskMgrFuncArg_t* a, unsigned rc, const cmChar_t* fmt, va_list vl );
  cmTmRC_t       cmTaskMgrWorkerErrorF( cmTaskMgrFuncArg_t* a, unsigned rc, const cmChar_t* fmt, ... );

  // Set the internal result buffer for this instance.
  // This is a convenient way to assign a final result to a instance
  // which can eventually be picked up by cmTaskMgrResult()
  // after the worker has officially completed.  Alternatively the
  // result of the worker computation can be returned using 
  // cmTaskMgrWorkerMsgSend().
  cmTmRC_t       cmTaskMgrWorkerSetResult(     cmTaskMgrFuncArg_t* a, void* result, unsigned resultByteCnt );

  // Get the size of an incoming message sent to this task instance
  // from the client.  Use cmTaskMgrWorkerMsgRecv() to get  
  // the msg. contents.
  unsigned       cmTaskMgrWorkerMsgByteCount(  cmTaskMgrFuncArg_t* a );

  // Copy a msg from the client into buf[bufByteCnt] and 
  // return the count of bytes copied into buf[bufByteCnt] or cmInvalidCnt
  // if the buf[] was too small. This function is only used when
  // the task did notregister a cmTaskMgrRecv_t with cmTaskMgrInstall().
  // cmTaskMgrWorkerMsgByteCount() returns the minimum required size of buf[].
  unsigned       cmTaskMgrWorkerMsgRecv( cmTaskMgrFuncArg_t* a, void* buf, unsigned bufByteCnt );
  
  // Send a generic msg to the client.
  cmTmWorkerRC_t cmTaskMgrWorkerMsgSend( cmTaskMgrFuncArg_t* a, const void* buf, unsigned bufByteCnt );

  cmTmRC_t cmTaskMgrTest(cmCtx_t* ctx);
  //)
  
#ifdef __cplusplus
}
#endif

#endif
