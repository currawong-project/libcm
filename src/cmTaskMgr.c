//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmThread.h"
#include "cmTime.h"
#include "cmTaskMgr.h"
#include "cmLinkedHeap.h"
#include "cmText.h"

cmTaskMgrH_t cmTaskMgrNullHandle = cmSTATIC_NULL_HANDLE;

struct cmTmInst_str;

typedef struct cmTmTask_str
{
  unsigned             taskId;
  cmChar_t*            label; 
  cmTaskMgrFunc_t      func;
  cmTaskMgrRecv_t      recv;
  struct cmTmTask_str* link;
} cmTmTask_t;

typedef struct cmTmInst_str
{
  unsigned             instId;             // Task instance id.
  struct cmTmTask_str* task;               // Pointer to task record for this task instance.
  void*                funcArg;            // Client supplied pointer to cmTaskMgrFuncArg_t.arg;
  unsigned             progCnt;            // Maximum expected progress (cmTaskMgrStatusArg_t.prog) to be used by this task instance.
  cmChar_t*            label;              // Optional instance label.
  cmStatusTmId_t       status;             // Current instance status (See cmStatusTmId_t)
  void*                result;             // Task instance result pointer. 
  unsigned             resultByteCnt;      // Size of the task instance result pointer in bytes.
  cmTaskMgrCtlId_t     ctlId;              // ctlId must only be written from the client thread
  cmTs1p1cH_t          msgQueH;            // client->inst 'msg' communication queue
  bool                 deleteOnCompleteFl; // delete this instance when its status indicates that it is killed or complete
  struct cmTmInst_str* link;
} cmTmInst_t;

struct cmTm_str* p;

typedef struct cmTmThread_str
{
  struct cmTm_str*       p;            // Pointer to task mgr.
  cmThreadH_t            thH;          // Thread handle. 
  cmTmInst_t*            inst;         // Ptr to the task instance this thread is executing.
  double                 durSecs;      // Duration of the instance currently assigned to this thread in seconds.
  cmTimeSpec_t           t0;           // Task start time.  
  cmTimeSpec_t           t1;           // Time of last review by the master thread.
  bool                   deactivateFl; // True if this instance has been deactivated by the system.
  cmTaskMgrFuncArg_t     procArg;      // 
  cmChar_t*              text;         // Temporary text buffer
  struct cmTmThread_str* link;         // p->threads link.
} cmTmThread_t;

typedef struct cmTm_str
{
  cmErr_t             err;              // Task manager error object.
  cmThreadH_t         mstrThH;          // Master thread handle.
  cmTmThread_t*       threads;          // Thread record list.
  unsigned            threadRecdCnt;    // Current count of records in 'threads' list.
  unsigned            maxActiveTaskCnt; // Max. number of active tasks.
  cmTaskMgrStatusCb_t statusCb;         // Client task status callback.
  void*               statusCbArg;      // Client task status callback argument.
  unsigned            pauseSleepMs;     // 
  cmTs1p1cH_t         callQueH;         // client->mgr 'inst' communication queue
  cmTsMp1cH_t         outQueH;          // mgr->client communication queue
  cmTmTask_t*         tasks;            // Task list.
  cmTmInst_t*         insts;            // Task instance list.
  unsigned            nextInstId;       // Next available task instance id.
  unsigned            activeTaskCnt;    // Current active task count.
} cmTm_t;


void  _cmTaskMgrStatusArgSetup(
  cmTaskMgrStatusArg_t* s,
  void*                 arg,
  unsigned              instId,
  cmSelTmId_t           selId,
  cmStatusTmId_t        statusId,
  unsigned              prog,
  const cmChar_t*       text,
  const void*           msg,
  unsigned              msgByteCnt )
{
  s->arg           = arg;
  s->instId        = instId;
  s->selId         = selId;
  s->statusId      = statusId;
  s->prog          = prog;
  s->text          = text;
  s->msg           = msg;
  s->msgByteCnt    = msgByteCnt;
}


// Called by MASTER and WORKER.
cmTmRC_t _cmTmEnqueueStatusMsg0( cmTm_t* p, const cmTaskMgrStatusArg_t* s )
{
  enum { arrayCnt = 3 };
  const void*    msgPtrArray[arrayCnt];
  unsigned       msgSizeArray[arrayCnt];

  msgPtrArray[0] = s;
  msgPtrArray[1] = s->text==NULL ? "" : s->text;
  msgPtrArray[2] = s->msg;

  msgSizeArray[0] = sizeof(cmTaskMgrStatusArg_t);
  msgSizeArray[1] = s->text==NULL ? 1 : strlen(s->text)+1;
  msgSizeArray[2] = s->msgByteCnt;


  if( cmTsMp1cEnqueueSegMsg(p->outQueH, msgPtrArray, msgSizeArray, arrayCnt ) != kOkThRC )
    return kQueueFailTmRC;
  
  return kOkTmRC;
}


// Called by MASTER and WORKER.
// This function is called by the worker thread wrapper _cmTmWorkerStatusCb()
// function to enqueue messages being sent back to the client.
cmTmRC_t _cmTmEnqueueStatusMsg1( 
  cmTm_t*         p,
  unsigned        instId,
  cmSelTmId_t     selId,
  cmStatusTmId_t  statusId,
  unsigned        prog,
  const cmChar_t* text,
  const void*     msg,
  unsigned        msgByteCnt )
{
  cmTaskMgrStatusArg_t s;
  _cmTaskMgrStatusArgSetup(&s,p->statusCbArg,instId,selId,statusId,prog,text,msg,msgByteCnt);
  return _cmTmEnqueueStatusMsg0(p,&s);
}

// Called by WORKER.
// Worker threads call this function to enqueue status messages
// for delivery to the task mgr client.
void _cmTmWorkerStatusCb( const cmTaskMgrStatusArg_t* status )
{
  cmTmThread_t* trp = (cmTmThread_t*)status->arg;

  if( _cmTmEnqueueStatusMsg0( trp->p, status ) != kOkTmRC )
  {
    /// ??????? HOW DO WE HANDLE ERRORS IN THE WORKER THREAD
    /// (set an error code in trp and let the master thread notice it.)
    assert(0);
  }

}

// Called by WORKER.
// This function is called in the worker thread by
// cmTs1p1cDequeueMsg() from within  cmTaskMgrWorkerHandleCommand()
// to transfer msg's waiting in the worker's incoming msg queue
// (cmTmInst_t.msgQueH) to the instance recv function (cmTmInst_t.recv).
cmRC_t _cmTmWorkerRecvCb( void* arg, unsigned msgByteCnt, const void* msg )
{
  cmTmThread_t* trp = (cmTmThread_t*)arg;
  assert(trp->inst->task->recv);
  trp->inst->task->recv(&trp->procArg,msg,msgByteCnt);
  return cmOkRC;
}

// Called by WORKER.
// This is the wrapper for all worker threads.
bool _cmTmWorkerThreadFunc(void* arg)
{
  cmTmThread_t*      trp = (cmTmThread_t*)arg;
  
  trp->procArg.reserved    = trp;
  trp->procArg.arg         = trp->inst->funcArg;
  trp->procArg.instId      = trp->inst->instId;
  trp->procArg.statusCb    = _cmTmWorkerStatusCb;
  trp->procArg.statusCbArg = trp;
  trp->procArg.progCnt     = trp->inst->progCnt;
  trp->procArg.pauseSleepMs= trp->p->pauseSleepMs;
  
  // if the task was paused or killed while it was queued then
  // cmTaskMgrHandleCommand() will do the right thing
  if( cmTaskMgrWorkerHandleCommand(&trp->procArg) != kStopTmwRC )
  { 
    trp->inst->status = kStartedTmId;

    // Notify the client that the instance has started.
    _cmTmEnqueueStatusMsg1(trp->p,trp->inst->instId,kStatusTmId,trp->inst->status,0,NULL,NULL,0);

    // Execute the client provided task function.
    trp->inst->task->func(&trp->procArg);
  }

  // Notify the client if the instance was killed
  if( trp->inst->ctlId == kKillTmId )
  {
    trp->inst->status = kKilledTmId;
    _cmTmEnqueueStatusMsg1(trp->p,trp->inst->instId,kStatusTmId,trp->inst->status,0,NULL,NULL,0);
  }

  // Notify the client that the instance is completed 
  // (but don't actually set the status yet)
  _cmTmEnqueueStatusMsg1(trp->p,trp->inst->instId,kStatusTmId,kCompletedTmId,0,NULL,NULL,0);

  trp->inst->status = kCompletedTmId;

  // Force the thread to go into the 'pause' state when it 
  // returns to it's internal loop. The master thread recognizes paused
  // threads as available for reuse.
  cmThreadPause(trp->thH,kPauseThFl);

  return true;
}

void _cmTmMasterRptError( cmTm_t* p, unsigned rc, const cmChar_t* msg )
{
  _cmTmEnqueueStatusMsg1(p,cmInvalidId,kErrorTmId,kInvalidTmId,rc,msg,NULL,0);
}

int _cmTmSortThreadByDur( const void* t0, const void* t1 )
{  
  double d = ((cmTmThread_t*)t0)->durSecs - ((cmTmThread_t*)t1)->durSecs; 

  return d== 0 ? 0 : (d<0 ? -1 : 1);
}


// This is the master thread function.
bool _cmTmMasterThreadFunc(void* arg)
{
  cmTm_t*       p         = (cmTm_t*)arg;
  unsigned      activeThreadCnt = 0;
  unsigned      activeTaskCnt   = 0;
  cmTmThread_t* trp       = p->threads;
 
  if( p->threadRecdCnt > 0 )
  {
    cmTmThread_t* thArray[p->threadRecdCnt];
    unsigned      deactivatedCnt = 0;

    //
    // Determine the number of active threads and tasks
    //

    // for each thread record
    for(trp=p->threads; trp!=NULL; trp=trp->link)
    {
      cmThStateId_t thState;

      // if this thread is active ...
      if( (thState = cmThreadState(trp->thH)) != kPausedThId )
      {        
        assert(trp->inst!=NULL);

        thArray[activeThreadCnt] = trp;
        ++activeThreadCnt;

        // if the task assigned to this thread is started then the task is active
        if( trp->inst->status == kStartedTmId )  
          ++activeTaskCnt;

        // if the deactivatedFl is set then this thread has been deactivated by the system
        if( trp->deactivateFl )
          ++deactivatedCnt;

        // update the task lifetime duration
        if( trp->inst->status != kCompletedTmId )
        {
          cmTimeSpec_t t2;
          cmTimeGet(&t2);
          trp->durSecs += (double)cmTimeElapsedMicros(&trp->t1,&t2) / 1000000.0;
          trp->t1 = t2;
        }

      }
    }

    //
    // thArray[activeThreadCnt] now holds pointers to the
    // cmTmThread_t records of the active threads
    //


    // if more tasks are active than should be - then deactive the youngest
    if( activeTaskCnt > p->maxActiveTaskCnt )
    {
      // sort the active tasks in increasing order of lifetime
      qsort(&thArray[0],activeThreadCnt,sizeof(thArray[0]),_cmTmSortThreadByDur);

      // determine the number of threads that need to be deactivated
      int n = activeTaskCnt - p->maxActiveTaskCnt;
      int i,j;

      // pause the active threads with the lowest lifetime
      for(i=0,j=0; i<activeThreadCnt && j<n; ++i)
        if( thArray[i]->deactivateFl == false )
        {
          thArray[i]->deactivateFl = true;
          ++deactivatedCnt;
          ++j;
        }
    }
    
    //
    // if there are deactivated tasks and the max thread count has not been reached
    // then re-activate some of the deactivated tasks.
    //
    
    if( activeTaskCnt < p->maxActiveTaskCnt && deactivatedCnt > 0 )
    {
      // sort the active tasks in increasing order of lifetime
      qsort(&thArray[0],activeThreadCnt,sizeof(thArray[0]),_cmTmSortThreadByDur);

      int n = cmMin(p->maxActiveTaskCnt - activeTaskCnt, deactivatedCnt );
      int i;

      // re-activate the oldest deactivated tasks first
      for(i=activeThreadCnt-1; i>=0 && n>0; --i)
        if( thArray[i]->deactivateFl )
        {
          thArray[i]->deactivateFl = false;
          --n;
          ++activeTaskCnt;
        }

    }
  }
  
  
  // If the number of activeTaskCnt is less than the limit and a queued task exists
  while( activeTaskCnt < p->maxActiveTaskCnt && cmTs1p1cMsgWaiting(p->callQueH) )
  {
    cmTmInst_t*   ip        = NULL;
    cmTmThread_t* atrp      = NULL;


    // Find a worker thread that is in the 'paused' state.
    // This is the definitive indication that the thread
    // does not have an assigned instance and the thread recd can be reused.
    for(trp=p->threads; trp!=NULL; trp=trp->link)
      if( cmThreadState(trp->thH) == kPausedThId )
      {
        atrp = trp;
        break;
      }

    // If all the existing worker threads are in use ...
    if( atrp==NULL )
    {
      // ... then create a new worker thread recd
      atrp = cmMemAllocZ(cmTmThread_t,1);

      // ... create the new worker thread
      if( cmThreadCreate(&atrp->thH,_cmTmWorkerThreadFunc,atrp,p->err.rpt) != kOkThRC )
      {
        cmMemFree(atrp);
        atrp = NULL;
        _cmTmMasterRptError(p,kThreadFailTmRC,"Worker thread create failed.");
        break;
      }
      else
      {
        // ... setup the new thread record
        atrp->p           = p;
        atrp->link        = p->threads;
        p->threads        = atrp;
        p->threadRecdCnt += 1;
      }
    }

    // if the thread creation failed
    if( atrp == NULL )
      break;

    // dequeue a pending task instance pointer from the input queue
    if(cmTs1p1cDequeueMsg(p->callQueH,&ip,sizeof(ip)) != kOkThRC )
    {
      _cmTmMasterRptError(p,kQueueFailTmRC,"Dequeue failed on incoming task instance queue.");
      break;
    }

    // if the task has a msg recv callback then assign it here
    if( ip->task->recv != NULL )
      if( cmTs1p1cSetCallback( ip->msgQueH, _cmTmWorkerRecvCb, atrp ) != kOkThRC )
      {
        _cmTmMasterRptError(p,kQueueFailTmRC,"Worker thread msg queue callback assignment failed.");
        break;
      }

    // setup the thread record associated with the new task
    atrp->inst         = ip;
    atrp->durSecs      = 0;
    atrp->deactivateFl = false;
    cmTimeGet(&atrp->t0);
    atrp->t1 = atrp->t0;


    // start the worker thread 
    if( cmThreadPause(atrp->thH,kWaitThFl) != kOkThRC )
      _cmTmMasterRptError(p,kThreadFailTmRC,"Worker thread start failed.");

    ++activeTaskCnt;
  }


  cmSleepMs(p->pauseSleepMs);

  p->activeTaskCnt = activeTaskCnt;

  return true;
}


cmTm_t* _cmTmHandleToPtr( cmTaskMgrH_t h )
{
  cmTm_t* p = (cmTm_t*)h.h;
  assert( p != NULL );
  return p;
}

cmTmTask_t* _cmTmTaskFromId( cmTm_t* p, unsigned taskId )
{
  cmTmTask_t* tp;
  for(tp=p->tasks; tp!=NULL; tp=tp->link)
    if( tp->taskId == taskId )
      return tp;
  return NULL;
}

cmTmInst_t* _cmTmInstFromId( cmTm_t* p, unsigned instId )
{
  cmTmInst_t* ip;
  for(ip=p->insts; ip!=NULL; ip=ip->link)
    if( ip->instId == instId )
      return ip;
  return NULL;
}


cmTmRC_t _cmTmInstFree( cmTm_t* p, unsigned instId )
{
  cmTmInst_t* ip = p->insts;  
  cmTmInst_t* pp = NULL;

  for(; ip!=NULL; ip=ip->link)
  {
    if( ip->instId == instId )
    {
      if( pp == NULL )
        p->insts = ip->link;
      else
        pp->link = ip->link;

      if( cmTs1p1cDestroy(&ip->msgQueH) != kOkThRC )
        return cmErrMsg(&p->err,kQueueFailTmRC,"The 'msg' input queue destroy failed.");

      cmMemFree(ip->label);
      cmMemFree(ip->result);
      cmMemFree(ip);
      return kOkTmRC;
    }
    pp = ip;
  }

  return cmErrMsg(&p->err,kAssertFailTmRC,"The instance %i could not be found to be deleted.",instId);
}


cmTmRC_t _cmTmDestroy( cmTm_t* p )
{
  cmTmRC_t rc = kOkTmRC;
  unsigned i;

  // stop and destroy the master thread
  if( cmThreadDestroy(&p->mstrThH) != kOkThRC )
  {
    rc = cmErrMsg(&p->err,kThreadFailTmRC,"Master thread destroy failed.");
    goto errLabel;
  }

  // stop and destroy all the worker threads
  for(i=0; p->threads != NULL; ++i )
  {
    if( cmThreadDestroy(&p->threads->thH) != kOkThRC )
    {
      rc = cmErrMsg(&p->err,kThreadFailTmRC,"Thread destruction failed for the worker thread at index %i.",i);
      goto errLabel;      
    }

    cmTmThread_t* trp = p->threads;
    p->threads = p->threads->link;
    cmMemFree(trp->text); 
    cmMemFree(trp);
  }

  // release the call input queue
  if( cmTs1p1cDestroy(&p->callQueH) != kOkThRC )
  {
    rc = cmErrMsg(&p->err,kQueueFailTmRC,"The 'call' input queue destroy failed.");
    goto errLabel;
  }

  // draining the output queue
  while( cmTsMp1cMsgWaiting(p->outQueH) )
    if(cmTsMp1cDequeueMsg(p->outQueH,NULL,0) != kOkThRC )
      cmErrMsg(&p->err,kQueueFailTmRC,"The output queue failed while draingin.");

  // release the output queue
  if( cmTsMp1cDestroy(&p->outQueH) != kOkThRC )
  {
    rc = cmErrMsg(&p->err,kQueueFailTmRC,"The input queue destroy failed.");
    goto errLabel;
  }


  // release instance list
  while( p->insts != NULL )
    _cmTmInstFree(p,p->insts->instId);

  // release the task list
  cmTmTask_t* tp = p->tasks;
  while( tp != NULL )
  {
    cmTmTask_t* np = tp->link;
    cmMemFree(tp->label);
    cmMemFree(tp);
    tp = np;
  }

  cmMemFree(p);
 errLabel:
  return rc;
}

cmRC_t _cmTmMasterOutQueueCb(void* arg, unsigned msgByteCnt, const void* msgDataPtr );

cmTmRC_t cmTaskMgrCreate(  
  cmCtx_t*             ctx, 
  cmTaskMgrH_t*        hp, 
  cmTaskMgrStatusCb_t  statusCb, 
  void*                statusCbArg, 
  unsigned             maxActiveTaskCnt,
  unsigned             queueByteCnt,
  unsigned             pauseSleepMs)
{
  cmTmRC_t rc = kOkTmRC;

  if((rc = cmTaskMgrDestroy(hp)) != kOkTmRC )
    return rc;

  cmTm_t* p = cmMemAllocZ(cmTm_t,1);
 
  cmErrSetup(&p->err,&ctx->rpt,"Task Mgr.");

  p->maxActiveTaskCnt = maxActiveTaskCnt;
  p->statusCb     = statusCb;
  p->statusCbArg  = statusCbArg;
  p->pauseSleepMs = pauseSleepMs;

  // create the master thread
  if( cmThreadCreate(&p->mstrThH, _cmTmMasterThreadFunc,p,&ctx->rpt) != kOkThRC )
  {
    rc = cmErrMsg(&p->err,kThreadFailTmRC,"Thread index %i create failed.");
    goto errLabel;
  }

  // create the call input queue
  if(cmTs1p1cCreate( &p->callQueH, queueByteCnt, NULL, NULL, p->err.rpt ) != kOkThRC )
  {
    rc = cmErrMsg(&p->err,kQueueFailTmRC,"The call input queue creation failed.");
    goto errLabel;
  }

  // create the output queue
  if( cmTsMp1cCreate( &p->outQueH, queueByteCnt, _cmTmMasterOutQueueCb, p, p->err.rpt ) != kOkThRC )
  {
    rc = cmErrMsg(&p->err,kQueueFailTmRC,"The output queue creation failed.");
    goto errLabel;
  }

  hp->h = p;

 errLabel:
  return rc;  
}

cmTmRC_t cmTaskMgrDestroy( cmTaskMgrH_t* hp )
{
  cmTmRC_t rc = kOkTmRC;
  if( hp==NULL || cmTaskMgrIsValid(*hp)==false )
    return rc;

  cmTm_t* p = _cmTmHandleToPtr(*hp);

  if((rc = _cmTmDestroy(p)) != kOkTmRC )
    return rc;

  hp->h = NULL;

  return rc;
}

void _cmTmWaitForCompletion( cmTm_t* p, unsigned timeOutMs )
{
  unsigned durMs = 0;
  cmTimeSpec_t  t0,t1;

  cmTimeGet(&t0);

  // Go into timeout loop - waiting for all instances to finish
  while( timeOutMs==0 || durMs < timeOutMs )
  {
    cmTimeGet(&t1);
    durMs += cmTimeElapsedMicros(&t0,&t1) / 1000;
    t0 = t1;

    cmSleepMs(p->pauseSleepMs);

    if( p->activeTaskCnt == 0 )
      break;
  }

}


cmTmRC_t cmTaskMgrClose( cmTaskMgrH_t h, unsigned flags, unsigned timeOutMs )
{
  cmTmRC_t rc = kOkTmRC;
  cmTm_t*  p  = _cmTmHandleToPtr(h);
  bool     fl = false;

  // if requested kill any queued tasks
  if( cmIsFlag(flags,kKillQueuedTmFl) )
  {
    cmTmInst_t* ip = p->insts;
    for(; ip!=NULL; ip=ip->link)
      if( ip->status == kQueuedTmId )
        ip->ctlId = kKillTmId;
  }

  // wait for any existing or queued tasks to complete
  _cmTmWaitForCompletion(p,timeOutMs);

  // force any queued msgs for the client to be sent
  cmTaskMgrOnIdle(h);

  // if the 'kill on timeout' flag is set then kill any remaining active tasks
  if( cmIsFlag(flags,kTimeOutKillTmFl) )
  {
    cmTmInst_t* ip = p->insts;
    for(; ip!=NULL; ip=ip->link)
      if( ip->status != kCompletedTmId )
      {
        ip->ctlId = kKillTmId;    
        fl = true;
      }
  }
  
  // wait for the remaining tasks to complete
  if( fl )
    _cmTmWaitForCompletion(p,timeOutMs);

  // force any queued msgs for the client to be sent
  cmTaskMgrOnIdle(h);

  return rc;
}

unsigned cmTaskMgrActiveTaskCount( cmTaskMgrH_t h )
{
  cmTm_t*  p  = _cmTmHandleToPtr(h);
  return p->activeTaskCnt;
}


bool     cmTaskMgrIsValid(   cmTaskMgrH_t h )
{ return h.h != NULL; }

const cmChar_t* cmTaskMgrStatusIdToLabel( cmStatusTmId_t statusId )
{
  typedef struct map_str
  {
    cmStatusTmId_t  id;
    const cmChar_t* label;
  } map_t;

  map_t a[] = 
  {
    { kQueuedTmId,      "Queued" },
    { kStartedTmId,     "Started" },
    { kPausedTmId,      "Paused" },
    { kDeactivatedTmId, "Deactivated" },
    { kCompletedTmId,   "Completed" },
    { kKilledTmId,      "Killed" },
    { kInvalidTmId,     "<Invalid>" },
  };

  int i;
  for(i=0; a[i].id!=kInvalidTmId; ++i)
    if( a[i].id == statusId )
      return a[i].label;

  return "<Unknown>";
}

// This function is called by cmTaskMgrIdle() to dispatch
// status updates to the client.
cmRC_t _cmTmMasterOutQueueCb(void* arg, unsigned msgByteCnt, const void* msgDataPtr )
{
  cmTm_t* p = (cmTm_t*)arg;
  cmTaskMgrStatusArg_t s;

  // This is probably not nesessary since changing the memory  
  // pointed to by msgDataPtr should be safe even though it is marked as const.
  memcpy(&s,msgDataPtr,sizeof(s));

  // The 'text' and 'msg' data have been serialized after the status record.
  // The 'text' is guaranteed to at least contain a terminating zero.
  s.text = ((char*)msgDataPtr) + sizeof(s);

  // if the 'resultByteCnt' > 0 then there is a result record
  if( s.msgByteCnt > 0 )
    s.msg = ((char*)msgDataPtr) + sizeof(s) + strlen(s.text) + 1;
  else
    s.msg = NULL;

  s.arg = p->statusCbArg;

  p->statusCb( &s );
  return cmOkRC;
}


cmTmRC_t cmTaskMgrOnIdle( cmTaskMgrH_t h )
{
  cmTmRC_t rc = kOkTmRC;
  cmTm_t*  p  = _cmTmHandleToPtr(h);

  // Transmit any msgs waiting to be sent to the client.
  while( cmTsMp1cMsgWaiting(p->outQueH) )
  {
    // calling this function calls: _cmTmMasterOutQueueCb()
    if(cmTsMp1cDequeueMsg(p->outQueH,NULL,0) != kOkThRC )
    {
      rc = cmErrMsg(&p->err,kQueueFailTmRC,"The output queue failed during a dequeue.");
      goto errLabel;
    }
  }

  // Step through the instance list and delete instances that are
  // completed and also marked for deletion.
  cmTmInst_t* ip = p->insts;
  while( ip != NULL )
  {
    cmTmInst_t* np = ip->link;

    if( ip->status==kCompletedTmId && ip->deleteOnCompleteFl )
      _cmTmInstFree(p,ip->instId);
    
    ip = np;
  }

 errLabel:
  return rc;
}

bool     cmTaskMgrIsEnabled( cmTaskMgrH_t h )
{
  cmTm_t*  p = _cmTmHandleToPtr(h);

  return cmThreadState(p->mstrThH) != kPausedThId;
}

cmTmRC_t cmTaskMgrEnable(    cmTaskMgrH_t h, bool enableFl )
{
  cmTmRC_t rc    = kOkTmRC;
  cmTm_t*  p     = _cmTmHandleToPtr(h);
  unsigned flags = (enableFl ? 0 : kPauseThFl) | kWaitThFl;

  if( cmThreadPause(p->mstrThH, flags ) != kOkThRC )
    rc = cmErrMsg(&p->err,kThreadFailTmRC,"The master thread failed to %s.",enableFl ? "enable" : "disable" );

  return rc;
}

cmTmRC_t cmTaskMgrInstall( 
  cmTaskMgrH_t    h, 
  unsigned        taskId, 
  const cmChar_t* label, 
  cmTaskMgrFunc_t func,
  cmTaskMgrRecv_t recv)
{
  cmTmRC_t    rc = kOkTmRC;
  cmTm_t*     p  = _cmTmHandleToPtr(h);
  cmTmTask_t* tp = cmMemAllocZ(cmTmTask_t,1);

  if( _cmTmTaskFromId(p,taskId) != NULL )
  {
    rc = cmErrMsg(&p->err,kInvalidArgTmRC,"The task id %i is already in use.",taskId);
    goto errLabel;
  }

  tp->taskId = taskId;
  tp->func   = func;
  tp->recv   = recv;
  tp->label  = cmMemAllocStr(label);
  
  tp->link   = p->tasks;
  p->tasks   = tp;
  
 errLabel:
  return rc;
}


cmTmRC_t cmTaskMgrCall( 
  cmTaskMgrH_t    h, 
  unsigned        taskId, 
  void*           funcArg, 
  unsigned        progCnt, 
  unsigned        queueByteCnt,
  const cmChar_t* label,
  unsigned*       retInstIdPtr )
{
  cmTmRC_t    rc = kOkTmRC;
  cmTm_t*     p  = _cmTmHandleToPtr(h);
  cmTmTask_t* tp = NULL;
  cmTmInst_t* ip = NULL;

  if( retInstIdPtr != NULL )
    *retInstIdPtr = cmInvalidId;

  // locate the task for this instance
  if((tp = _cmTmTaskFromId(p,taskId)) == NULL )
  {
    rc = cmErrMsg(&p->err,kInvalidArgTmRC,"Task not found for task id=%i.",taskId);
    goto errLabel;
  }

  // allocate a new instance record
  ip = cmMemAllocZ(cmTmInst_t,1);

  // setup the instance record
  ip->instId         = p->nextInstId++;
  ip->task           = tp;
  ip->funcArg        = funcArg;
  ip->progCnt        = progCnt;
  ip->label          = label==NULL ? NULL : cmMemAllocStr(label);
  ip->status         = kQueuedTmId;
  ip->ctlId          = kStartTmId;
 
  // create the msg input queue
  if(cmTs1p1cCreate( &ip->msgQueH, queueByteCnt, NULL, NULL, p->err.rpt ) != kOkThRC )
  {
    rc = cmErrMsg(&p->err,kQueueFailTmRC,"The msg input queue creation failed.");
    goto errLabel;
  }

  // insert the new instance at the end of the instance list
  if( p->insts == NULL )
    p->insts = ip;
  else
  {
    cmTmInst_t* pp = p->insts;
    for(; pp != NULL; pp=pp->link )
      if( pp->link == NULL )
      {
        pp->link = ip;
        break;
      }
  }

  // enqueue the instance ptr in the input queue 
  if( cmTs1p1cEnqueueMsg(p->callQueH,&ip,sizeof(ip)) != kOkThRC )
  {
    rc = cmErrMsg(&p->err,kQueueFailTmRC,"New task instance command enqueue failed.");    
    goto errLabel;
  }

  // set the returned instance id
  if( retInstIdPtr != NULL )
    *retInstIdPtr = ip->instId;

  // notify the client that the instance was enqueued
  cmTaskMgrStatusArg_t s;
  _cmTaskMgrStatusArgSetup(&s,p->statusCbArg,ip->instId,kStatusTmId,kQueuedTmId,progCnt,NULL,NULL,0);
  
  p->statusCb( &s );
  
 errLabel:
  return rc;
}

cmTmRC_t cmTaskMgrCtl( cmTaskMgrH_t h, unsigned instId, cmTaskMgrCtlId_t ctlId )
{
  cmTmRC_t    rc = kOkTmRC;
  cmTm_t*     p  = _cmTmHandleToPtr(h);
  cmTmInst_t* ip = NULL;

  if((ip = _cmTmInstFromId(p,instId)) == NULL )
  {
    cmErrMsg(&p->err,kInvalidArgTmRC,"The task instance associated with id %i could not be found.",instId);
    goto errLabel;
  }

  // Once an instance ctlId is set to kKillTmId don't allow it to change.
  if( ip->ctlId == kKillTmId )
    return rc;

  switch(ctlId )
  {
    case kStartTmId:
      // Acting on a 'start' cmd only makes sense if the previous command was 'pause'
      if( ip->ctlId == kPauseTmId )
        ip->ctlId = kStartTmId;
      break;

    case kPauseTmId:
      // Acting on a 'pause' command only makes sense if  the previous command was a 'start'
      if( ip->ctlId == kStartTmId )
        ip->ctlId = kPauseTmId;
      break;

    case kKillTmId:
      ip->ctlId = kKillTmId;
      break;
  }


 errLabel:
  return rc;
}

cmStatusTmId_t cmTaskMgrStatus( cmTaskMgrH_t h, unsigned instId )
{
  cmTm_t*        p      = _cmTmHandleToPtr(h);
  cmTmInst_t*    ip     = NULL;
  cmStatusTmId_t status = kInvalidTmId;

  if((ip = _cmTmInstFromId(p,instId)) == NULL )
  {
    cmErrMsg(&p->err,kInvalidArgTmRC,"The task instance associated with id %i could not be found.",instId);
    goto errLabel;
  }
  
  status = ip->status;

 errLabel:
  return status;
}

cmTmRC_t  cmTaskMgrSendMsg( cmTaskMgrH_t h, unsigned instId, const void* msg, unsigned msgByteCnt )
{
  cmTm_t*     p  = _cmTmHandleToPtr(h);
  cmTmRC_t    rc = kOkTmRC;  
  cmTmInst_t* ip = NULL;

  if((ip = _cmTmInstFromId(p,instId)) == NULL )
    return cmErrMsg(&p->err,kInvalidArgTmRC,"The task instance associated with id %i could not be found.",instId);

  if( cmTs1p1cEnqueueMsg(ip->msgQueH, msg, msgByteCnt ) != kOkThRC )
    rc = cmErrMsg(&p->err,kQueueFailTmRC,"Task msg enqueue failed.");    

  return rc;
}

const cmChar_t* cmTaskMgrTaskIdToLabel( cmTaskMgrH_t h, unsigned taskId )
{
  cmTm_t*     p  = _cmTmHandleToPtr(h);
  cmTmTask_t* tp;
  if((tp = _cmTmTaskFromId(p,taskId)) == NULL )
    return NULL;
  return tp->label;
}

const cmChar_t* cmTaskMgrInstIdToLabel( cmTaskMgrH_t h, unsigned instId )
{
  cmTm_t* p = _cmTmHandleToPtr(h);
  cmTmInst_t* ip;
  if((ip = _cmTmInstFromId(p,instId)) == NULL )
    return NULL;
  return ip->label;
}
  
const void* cmTaskMgrResult( cmTaskMgrH_t h, unsigned instId )
{
  cmTm_t*     p  = _cmTmHandleToPtr(h);
  cmTmInst_t* ip = NULL;

  if((ip = _cmTmInstFromId(p,instId)) == NULL )
  {
    cmErrMsg(&p->err,kInvalidArgTmRC,"The task instance associated with id %i could not be found.",instId);
    goto errLabel;
  }

  if( ip->status != kCompletedTmId )
  {
    cmErrMsg(&p->err,kOpFailTmRC,"The result of a running task (id:%i) may not be accessed.",instId);
    goto errLabel;
  }

  return ip->result;

 errLabel:
  return NULL;
}

unsigned    cmTaskMgrResultByteCount( cmTaskMgrH_t h, unsigned instId )
{
  cmTm_t*     p  = _cmTmHandleToPtr(h);
  cmTmInst_t* ip = NULL;

  if((ip = _cmTmInstFromId(p,instId)) == NULL )
  {
    cmErrMsg(&p->err,kInvalidArgTmRC,"The task instance associated with id %i could not be found.",instId);
    goto errLabel;
  }

  if( ip->status != kCompletedTmId )
  {
    cmErrMsg(&p->err,kOpFailTmRC,"The result byte count of a running task (id:%i) may not be accessed.",instId);
    goto errLabel;
  }

  return ip->resultByteCnt;

 errLabel:
  return 0;
}

void* cmTaskMgrFuncArg( cmTaskMgrH_t h, unsigned instId )
{
  cmTm_t*     p  = _cmTmHandleToPtr(h);
  cmTmInst_t* ip = NULL;

  if((ip = _cmTmInstFromId(p,instId)) == NULL )
  {
    cmErrMsg(&p->err,kInvalidArgTmRC,"The task instance associated with id %i could not be found.",instId);
    goto errLabel;
  }

  if( ip->status != kCompletedTmId )
  {
    cmErrMsg(&p->err,kOpFailTmRC,"The function argument of a running task (id:%i) may not be accessed.",instId);
    goto errLabel;
  }

  return ip->funcArg;

 errLabel:
  return NULL;
}


cmTmRC_t    cmTaskMgrInstDelete(    cmTaskMgrH_t h, unsigned instId )
{
  cmTmRC_t    rc = kOkTmRC;
  cmTm_t*     p  = _cmTmHandleToPtr(h);
  cmTmInst_t* ip = NULL;

  if((ip = _cmTmInstFromId(p,instId)) == NULL )
  {
    cmErrMsg(&p->err,kInvalidArgTmRC,"The task instance associated with id %i could not be found.",instId);
    return 0;
  }

  ip->deleteOnCompleteFl = true;

  return rc;
}

void _cmTaskMgrWorkerHelper( cmTaskMgrFuncArg_t* a, cmSelTmId_t selId, cmStatusTmId_t statusId, unsigned prog, const cmChar_t* text )
{
  cmTaskMgrStatusArg_t s;
  
  _cmTaskMgrStatusArgSetup(
    &s,
    a->statusCbArg,
    a->instId,
    statusId == kInvalidTmId ? kProgTmId    : kStatusTmId,
    statusId == kInvalidTmId ? kStartedTmId : statusId,
    statusId == kInvalidTmId ? prog         : 0,
    text,NULL,0);

  a->statusCb(&s);
}

cmTmWorkerRC_t cmTaskMgrWorkerHandleCommand( cmTaskMgrFuncArg_t* a )
{
  cmTmThread_t*  trp       = a->reserved;

  // Check if we should go into the paused or deactivated state.
  if( trp->inst->ctlId == kPauseTmId || trp->deactivateFl == true )
  {
    cmStatusTmId_t prvStatus = kInvalidTmId;

    do
    {
      // Note that it is possible that the state of the task switch from
      // paused <-> deactivated during the course of this loop.  
      // In either case we continue looping but should report the change
      // to the client via a status callback.

      // change the instance status to reflect the true status
      trp->inst->status = trp->deactivateFl ? kDeactivatedTmId : kPausedTmId;

      // if the status actually changed then notify the client
      if( trp->inst->status != prvStatus )
      {
        _cmTaskMgrWorkerHelper(a,kStatusTmId,trp->inst->status,0,NULL);
        prvStatus = trp->inst->status;
      }

      // sleep the thread for pauseSleepMs milliseconds
      cmSleepMs(a->pauseSleepMs);

      // if the task was unpaused while we slept
    }while( trp->inst->ctlId == kPauseTmId || trp->deactivateFl==true );
      
    // we are leaving the paused state because we were restarted or killed.
    switch( trp->inst->ctlId )
    {
      case kStartTmId:
        // change the instance status to 'started'.
        trp->inst->status = kStartedTmId;

        // notify the client of the change in state
        _cmTaskMgrWorkerHelper(a,kStatusTmId,kStartedTmId,0,NULL);
        break;

      case kKillTmId:
        // if killed the client will be notified in the worker thread wrapper
        // function: _cmTmWorkerThreadFunc()
        break;

      default:
        { assert(0); }
    }
  }
  else  // There was no command to handle so check for incoming msg's.
  {
    
    if( cmTs1p1cMsgWaiting(trp->inst->msgQueH) )
    {
      // if the task registered a msg receive callback
      if( trp->inst->task->recv != NULL )
      {
        
        if( cmTs1p1cDequeueMsg(trp->inst->msgQueH,  NULL, 0 ) != kOkThRC )
        {
          
          // ?????
          // ????? how do we send error messages back to the client
          // ??????
          return kOkTmwRC;
        }
        

      }
      else
      {
      }
      
      return kRecvTmwRC;
    }
  }

  // if ctlId==kKillTmId then the status update will be handled 
  // when the task custom function returns in  _cmTmWorkerThreadFunc()

  return trp->inst->ctlId == kKillTmId  ? kStopTmwRC : kOkTmwRC;
}



cmTmRC_t cmTaskMgrWorkerSendStatus( cmTaskMgrFuncArg_t* a, cmStatusTmId_t statusId )
{  
  _cmTaskMgrWorkerHelper(a,kStatusTmId,statusId,0,NULL); 
  return kOkTmRC;
}

cmTmRC_t cmTaskMgrWorkerSendProgress( cmTaskMgrFuncArg_t* a, unsigned prog, const cmChar_t* text )
{ 
  _cmTaskMgrWorkerHelper(a,kProgTmId,kInvalidTmId,prog,text);  
  return kOkTmRC;
}

cmTmRC_t       cmTaskMgrWorkerSendProgressV( cmTaskMgrFuncArg_t* a, unsigned prog, const cmChar_t* fmt, va_list vl )
{
  cmTmThread_t*  trp  = a->reserved;
  cmTsVPrintfP(trp->text,fmt,vl);
  return  cmTaskMgrWorkerSendProgress(a,prog,trp->text);
}

cmTmRC_t       cmTaskMgrWorkerSendProgressF( cmTaskMgrFuncArg_t* a, unsigned prog, const cmChar_t* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  cmTmRC_t rc = cmTaskMgrWorkerSendProgressV(a,prog,fmt,vl);
  va_end(vl);
  return rc;
}

cmTmRC_t  cmTaskMgrWorkerError(  cmTaskMgrFuncArg_t* a, unsigned rc, const cmChar_t* text )
{
  _cmTaskMgrWorkerHelper(a, kErrorTmId, kInvalidTmId, rc, text);
  return rc;
}

cmTmRC_t  cmTaskMgrWorkerErrorV( cmTaskMgrFuncArg_t* a, unsigned rc, const cmChar_t* fmt, va_list vl )
{
  cmTmThread_t*  trp  = a->reserved;
  cmTsVPrintfP(trp->text,fmt,vl);  
  return cmTaskMgrWorkerError(a,rc,trp->text);
}
 
cmTmRC_t  cmTaskMgrWorkerErrorF( cmTaskMgrFuncArg_t* a, unsigned rc, const cmChar_t* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  cmTmRC_t rc0 = cmTaskMgrWorkerErrorV(a,rc,fmt,vl);
  va_end(vl);
  return rc0;
}

cmTmRC_t cmTaskMgrWorkerSetResult(    cmTaskMgrFuncArg_t* a, void* result, unsigned resultByteCnt )
{
  cmTmThread_t*  trp = a->reserved;
  trp->inst->result        = result;
  trp->inst->resultByteCnt = resultByteCnt;
  return kOkTmRC;
}

unsigned cmTaskMgrWorkerMsgByteCount( cmTaskMgrFuncArg_t* a )
{
  cmTmThread_t*  trp       = a->reserved;
  return cmTs1p1cDequeueMsgByteCount(trp->inst->msgQueH);
}

unsigned cmTaskMgrWorkerMsgRecv( cmTaskMgrFuncArg_t* a, void* buf, unsigned bufByteCnt )
{
  cmTmThread_t*  trp    = a->reserved;
  unsigned       retVal = bufByteCnt;

  switch( cmTs1p1cDequeueMsg(trp->inst->msgQueH,  buf, bufByteCnt ) )
  {
    case kOkThRC:
      break;

    case kBufEmptyThRC:
      retVal = 0;
      break;

    case kBufTooSmallThRC:
      retVal = cmInvalidCnt;
      break;

    default:
      { assert(0); }
  }

  return retVal;
}

cmTmRC_t cmTaskMgrWorkerMsgSend( cmTaskMgrFuncArg_t* a, const void* buf, unsigned bufByteCnt )
{
  cmTmThread_t*  trp    = a->reserved;
  return _cmTmEnqueueStatusMsg1(trp->p,trp->inst->instId,kMsgTmId,trp->inst->status,0,NULL,buf,bufByteCnt);
}


//-----------------------------------------------------------------------------

enum { kMaxTestInstCnt = 3 };

typedef struct cmTmTestInst_str
{
  unsigned instId;
} cmTmTestInst_t;

typedef struct cmTmTestApp_str
{
  cmErr_t*       err;
  cmTmTestInst_t insts[kMaxTestInstCnt];
} cmTmTestApp_t;

void _cmTmTestReportStatus( cmRpt_t* rpt, const cmTaskMgrStatusArg_t* s )
{
  cmRptPrintf(rpt,"inst:%i ",s->instId );

  switch( s->selId )
  {
    case kStatusTmId:
      {
        const cmChar_t* label = cmTaskMgrStatusIdToLabel(s->statusId); 
        cmRptPrintf(rpt,"status '%s'",label);
      }
      break;

    case kProgTmId:
      cmRptPrintf(rpt,"prog %i",s->prog);
      break;

    case kErrorTmId:
      cmRptPrintf(rpt,"error %s",cmStringNullGuard(s->msg));
      break;

    default:
      { assert(0); }
  }

  cmRptPrintf(rpt,"\n");

}

// Test client status callback function.
void _cmTmTestStatusCb( const cmTaskMgrStatusArg_t* s  )
{
   // s.arg set from cmTaskMgrCreate( ..., statusCbArg, ...);
  cmTmTestApp_t* app = (cmTmTestApp_t*)s->arg;
  unsigned i;

  // locate the instance record assoc'd with this callback
  for(i=0; i<kMaxTestInstCnt; ++i)
    if( app->insts[i].instId ==  s->instId )
      break;

  if( i==kMaxTestInstCnt )
    cmRptPrintf(app->err->rpt,"instId %i not found.\n",s->instId);

  _cmTmTestReportStatus(app->err->rpt,s);
}


// Test worker function.
void _cmTmTestFunc(cmTaskMgrFuncArg_t* arg )
{
  if( cmTaskMgrWorkerHandleCommand(arg) == kStopTmwRC )
    return;

  unsigned prog = 0;

  for(; prog<arg->progCnt; ++prog)
  {
    if( cmTaskMgrWorkerHandleCommand(arg) == kStopTmwRC )
      break;

    cmSleepMs(1000); 

    if( cmTaskMgrWorkerSendProgress(arg,prog,NULL) == kStopTmwRC )
      break;
  }
  
}


cmTmRC_t cmTaskMgrTest(cmCtx_t* ctx)
{
  cmTmRC_t        rc           = kOkTmRC;
  cmTaskMgrH_t    tmH          = cmTaskMgrNullHandle;
  unsigned        threadCnt    = 2;
  unsigned        queueByteCnt = 1024;
  unsigned        pauseSleepMs = 50;
  unsigned        nextInstId   = 0;
  unsigned        taskId       = 0;
  const cmChar_t* taskLabel    = "Task Label";
  cmTmTestApp_t   app;
  char            c;

  memset(&app,0,sizeof(app));
  app.err = &ctx->err;

  // create the task mgr
   if( cmTaskMgrCreate( ctx,&tmH,_cmTmTestStatusCb,&app,threadCnt,queueByteCnt,pauseSleepMs) != kOkTmRC )
  {
    rc = cmErrMsg(&ctx->err,kTestFailTmRC,"Task mgr create failed.");
    goto errLabel;
  }

  // install a task
   if( cmTaskMgrInstall(tmH, taskId, taskLabel, _cmTmTestFunc, NULL ) != kOkTmRC )
  {
    rc = cmErrMsg(&ctx->err,kTestFailTmRC,"Task mgr task install failed.");
    goto errLabel;    
  }


  // go into interactive mode
  printf("q=quit e=enable c=call i=idle\n");
  while((c = getchar()) != 'q')
  {
    switch(c)
    {
      case 'i':
        cmTaskMgrOnIdle(tmH);
        cmRptPrintf(&ctx->rpt,"idled\n");
        break;

      case 'e':
        {
          // toggle the enable state of the task mgr.
          bool fl = !cmTaskMgrIsEnabled(tmH);
          if( cmTaskMgrEnable(tmH,fl) != kOkTmRC )
            rc = cmErrMsg(&ctx->err,kTestFailTmRC,"Test enable failed.");
          else
            cmRptPrintf(&ctx->rpt,"%s\n", fl ? "enabled" : "disabled" );
        }
        break;

      case 'c':
          if( nextInstId < kMaxTestInstCnt )
          {
            void* funcArg = app.insts + nextInstId;
            unsigned progCnt = 5;
            if( cmTaskMgrCall( tmH, taskId, funcArg, progCnt, queueByteCnt, "My Inst", &app.insts[nextInstId].instId ) != kOkTmRC )
              rc = cmErrMsg(&ctx->err,kTestFailTmRC,"Test call failed.");            
            else
            {
              ++nextInstId;
              cmRptPrintf(&ctx->rpt,"called\n");
            }
          }
    }
  }

  

 errLabel:
  // destroy the task mgr
  if( cmTaskMgrDestroy(&tmH) != kOkTmRC )
    rc = cmErrMsg(&ctx->err,kTestFailTmRC,"Task mgr destroy failed.");

  return rc;
}
