#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmThread.h"
#include "cmTaskMgr.h"

cmTaskMgrH_t cmTaskMgrNullHandle = cmSTATIC_NULL_HANDLE;

struct cmTmInst_str;

typedef struct cmTmTask_str
{
  unsigned             taskId;
  cmChar_t*            label; 
  cmTaskMgrFunc_t      func;
  struct cmTmTask_str* link;
} cmTmTask_t;

typedef struct cmTmInst_str
{
  unsigned             instId;
  struct cmTmTask_str* task;
  void*                funcArg;
  unsigned             progCnt;
  cmStatusTmId_t       status;
  void*                result;
  unsigned             resultByteCnt;
  cmTaskMgrCtlId_t     ctlId;          // ctlId must only be written from the client thread
  bool                 deleteOnCompleteFl; // delete this instance when its status indicates that it is killed or complete
  struct cmTmInst_str* link;
} cmTmInst_t;

struct cmTm_str* p;

typedef struct cmTmThread_str
{
  struct cmTm_str* p;      // 
  cmThreadH_t      thH;    // 
  cmTmInst_t*      inst;   // Ptr to the task instance this thread is executing.
} cmTmThread_t;

typedef struct cmTm_str
{
  cmErr_t             err;
  cmTmThread_t*       thArray;      // 
  unsigned            threadCnt;    //
  cmTaskMgrStatusCb_t statusCb;     // 
  void*               statusCbArg;  // 
  unsigned            pauseSleepMs;
  cmTs1p1cH_t         inQueH;       // client->mgr
  cmTsMp1cH_t         outQueH;      // mgr->client
  cmTmTask_t*         tasks;        //
  cmTmInst_t*         insts;        //
  unsigned            nextInstId;
} cmTm_t;


void  _cmTaskMgrStatusArgSetup(
  cmTaskMgrStatusArg_t* s,
  void*                 arg,
  unsigned              instId,
  cmSelTmId_t           selId,
  cmStatusTmId_t        statusId,
  unsigned              prog,
  const cmChar_t*       msg,
  void*                 result,
  unsigned              resultByteCnt )
{
  s->arg           = arg;
  s->instId        = instId;
  s->selId         = selId;
  s->statusId      = statusId;
  s->prog          = prog;
  s->msg           = msg;
  s->result        = result;
  s->resultByteCnt = resultByteCnt;
}

// WARNING: THIS FUNCTION IS CALLED BY BOTH THE WORKER AND THE MASTER THREAD.
cmTmRC_t _cmTmEnqueueStatusMsg0( cmTm_t* p, const cmTaskMgrStatusArg_t* s )
{
  enum { arrayCnt = 3 };
  const void*    msgPtrArray[arrayCnt];
  unsigned       msgSizeArray[arrayCnt];

  msgPtrArray[0] = s;
  msgPtrArray[1] = s->msg==NULL ? "" : s->msg;
  msgPtrArray[2] = s->result;

  msgSizeArray[0] = sizeof(cmTaskMgrStatusArg_t);
  msgSizeArray[1] = s->msg==NULL ? 1 : strlen(s->msg)+1;
  msgSizeArray[2] = s->resultByteCnt;


  if( cmTsMp1cEnqueueSegMsg(p->outQueH, msgPtrArray, msgSizeArray, arrayCnt ) != kOkThRC )
    return kQueueFailTmRC;
  
  return kOkTmRC;
}


// This function is called by the worker thread wrapper _cmTmWorkerStatusCb()
// function to enqueue messages being sent back to the client.
cmTmRC_t _cmTmEnqueueStatusMsg1( 
  cmTm_t*         p,
  unsigned        instId,
  cmSelTmId_t     selId,
  cmStatusTmId_t  statusId,
  unsigned        prog,
  const cmChar_t* msg,
  void*           result,
  unsigned        resultByteCnt )
{
  cmTaskMgrStatusArg_t s;
  _cmTaskMgrStatusArgSetup(&s,p->statusCbArg,instId,selId,statusId,prog,msg,result,resultByteCnt);
  return _cmTmEnqueueStatusMsg0(p,&s);
}

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


// This is the wrapper for all worker threads.
bool _cmTmWorkerThreadFunc(void* arg)
{
  cmTmThread_t*      trp = (cmTmThread_t*)arg;
  cmTaskMgrFuncArg_t r;

  r.reserved    = trp;
  r.arg         = trp->inst->funcArg;
  r.instId      = trp->inst->instId;
  r.statusCb    = _cmTmWorkerStatusCb;
  r.statusCbArg = trp;
  r.progCnt     = trp->inst->progCnt;
  r.pauseSleepMs= trp->p->pauseSleepMs;
  
  // if the task was paused or killed while it was queued then
  // cmTaskMgrHandleCommand() will do the right thing
  if( cmTaskMgrHandleCommand(&r) != kKillTmId )
  { 
    trp->inst->status = kStartedTmId;

    // Notify the client that the instance has started.
    _cmTmEnqueueStatusMsg1(trp->p,trp->inst->instId,kStatusTmId,trp->inst->status,0,NULL,NULL,0);

    // Execute the client provided task function.
    trp->inst->task->func(&r);
  }

  // Notify the client that the instance has completed or been killed
  if( trp->inst->ctlId == kKillTmId )
    trp->inst->status = kKilledTmId;
  else
    trp->inst->status = kCompletedTmId;

  _cmTmEnqueueStatusMsg1(trp->p,trp->inst->instId,kStatusTmId,trp->inst->status,0,NULL,NULL,0);

  
  trp->inst = NULL;

  // Force the thread to go into the 'pause' state when it 
  // returns to it's internal loop. The master thread recognizes paused
  // threads as available.
  cmThreadPause(trp->thH,kPauseThFl);

  return true;
}


// This is the master thread function.
bool _cmTmMasterThreadFunc(void* arg)
{
  cmTmThread_t* trp = (cmTmThread_t*)arg;
  cmTm_t*         p = trp->p;
  
  while( cmTs1p1cMsgWaiting(p->inQueH) )
  {
    unsigned i;
    cmTmInst_t* ip = NULL;

    // find an available worker thread
    for(i=1; i<p->threadCnt; ++i)
      if( cmThreadState(p->thArray[i].thH) == kPausedThId )
        break;

    // if all worker threads are busy ... give up
    if( i==p->threadCnt )
      break;

    // dequeue a pending task instance pointer from the input queue
    if(cmTs1p1cDequeueMsg(p->inQueH,&ip,sizeof(ip)) != kOkThRC )
    {
      /// ??????? HOW DO WE HANDLE ERRORS IN THE MASTER THREAD
      continue;
    }

    // assign the instance to the available thread
    p->thArray[i].inst = ip;
    
    // start the thread and wait for it to enter the running state.
    
    if( cmThreadPause(p->thArray[i].thH,0) != kOkThRC )
    {
      /// ??????? HOW DO WE HANDLE ERRORS IN THE MASTER THREAD
    }

  }

  cmSleepMs(p->pauseSleepMs);

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

  // stop and destroy all the threads
  for(i=0; i<p->threadCnt; ++i)
  {
    if( cmThreadDestroy(&p->thArray[i].thH) != kOkThRC )
    {
      rc = cmErrMsg(&p->err,kThreadFailTmRC,"Thread index %i destroy failed.",i);
      goto errLabel;
    }
  }

  cmMemFree(p->thArray);

  // release the input queue
  if( cmTs1p1cDestroy(&p->inQueH) != kOkThRC )
  {
    rc = cmErrMsg(&p->err,kQueueFailTmRC,"The input queue destroy failed.");
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
  unsigned             threadCnt,
  unsigned             queueByteCnt,
  unsigned             pauseSleepMs)
{
  cmTmRC_t rc = kOkTmRC;
  unsigned i;

  if((rc = cmTaskMgrDestroy(hp)) != kOkTmRC )
    return rc;

  cmTm_t* p = cmMemAllocZ(cmTm_t,1);
 
  cmErrSetup(&p->err,&ctx->rpt,"Task Mgr.");

  threadCnt += 1;
  p->thArray      = cmMemAllocZ(cmTmThread_t,threadCnt);
  p->threadCnt    = threadCnt;
  p->statusCb     = statusCb;
  p->statusCbArg  = statusCbArg;
  p->pauseSleepMs = pauseSleepMs;

  // create the threads
  for(i=0; i<threadCnt; ++i)
  {
    if( cmThreadCreate(&p->thArray[i].thH, i==0 ? _cmTmMasterThreadFunc : _cmTmWorkerThreadFunc,  p->thArray+i, &ctx->rpt ) != kOkThRC )
    {
      rc = cmErrMsg(&p->err,kThreadFailTmRC,"Thread index %i create failed.",i);
      goto errLabel;
    }

    p->thArray[i].p = p;

  }

  // create the input queue
  if(cmTs1p1cCreate( &p->inQueH, queueByteCnt, NULL, NULL, p->err.rpt ) != kOkThRC )
  {
    rc = cmErrMsg(&p->err,kQueueFailTmRC,"The input queue creation failed.");
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

bool     cmTaskMgrIsValid(   cmTaskMgrH_t h )
{ return h.h != NULL; }


// This function is called by cmTaskMgrIdle() to dispatch
// status updates to the client.
cmRC_t _cmTmMasterOutQueueCb(void* arg, unsigned msgByteCnt, const void* msgDataPtr )
{
  cmTm_t* p = (cmTm_t*)arg;
  cmTaskMgrStatusArg_t s;

  // This is probably not nesessary since changing the memory  
  // pointed to by msgDataPtr should be safe even though it is marked as const.
  memcpy(&s,msgDataPtr,sizeof(s));

  // The 'msg' and 'result' data have been serialized after the status record.
  // The 'msg' is guaranteed to at least contain a terminating zero.
  s.msg = ((char*)msgDataPtr) + sizeof(s);

  // if the 'resultByteCnt' > 0 then there is a result record
  if( s.resultByteCnt > 0 )
    s.result = ((char*)msgDataPtr) + sizeof(s) + strlen(s.msg) + 1;
  else
    s.result = NULL;

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

    if( (ip->status==kCompletedTmId || ip->status==kKilledTmId) && ip->deleteOnCompleteFl )
      _cmTmInstFree(p,ip->instId);
    
    ip = np;
  }

 errLabel:
  return rc;
}

bool     cmTaskMgrIsEnabled( cmTaskMgrH_t h )
{
  cmTm_t*  p = _cmTmHandleToPtr(h);

  return cmThreadState(p->thArray[0].thH) != kPausedThId;
}

cmTmRC_t cmTaskMgrEnable(    cmTaskMgrH_t h, bool enableFl )
{
  cmTmRC_t rc    = kOkTmRC;
  cmTm_t*  p     = _cmTmHandleToPtr(h);
  unsigned flags = (enableFl ? 0 : kPauseThFl) | kWaitThFl;

  if( cmThreadPause(p->thArray[0].thH, flags ) != kOkThRC )
    rc = cmErrMsg(&p->err,kThreadFailTmRC,"The master thread failed to %s.",enableFl ? "enable" : "disable" );

  return rc;
}

cmTmRC_t cmTaskMgrInstall( cmTaskMgrH_t h, unsigned taskId, const cmChar_t* label, cmTaskMgrFunc_t func )
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
  tp->label  = cmMemAllocStr(label);
  
  tp->link   = p->tasks;
  p->tasks   = tp;
  
 errLabel:
  return rc;
}

cmTmRC_t cmTaskMgrCall( 
  cmTaskMgrH_t h, 
  unsigned     taskId, 
  void*        funcArg, 
  unsigned     progCnt, 
  unsigned*    retInstIdPtr )
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

  // setupt the instance record
  ip->instId         = p->nextInstId++;
  ip->task           = tp;
  ip->funcArg        = funcArg;
  ip->progCnt        = progCnt;
  ip->status         = kQueuedTmId;

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


  // enque the instance ptr in the input queue 
  if( cmTs1p1cEnqueueMsg(p->inQueH,&ip,sizeof(ip)) != kOkThRC )
  {
    rc = cmErrMsg(&p->err,kQueueFailTmRC,"New task instance command enqueue failed.");    
    goto errLabel;
  }

  // set the returned instance id
  if( retInstIdPtr != NULL )
    *retInstIdPtr = ip->instId;

  // notify the client that the instance was enqueued
  cmTaskMgrStatusArg_t s;
  _cmTaskMgrStatusArgSetup(&s,p->statusCbArg,ip->instId,kStatusTmId,kQueuedTmId,0,NULL,NULL,0);
  
  p->statusCb( &s );
  
 errLabel:
  return rc;
}

cmTmRC_t cmTaskMgrTaskCtl( cmTaskMgrH_t h, unsigned instId, cmTaskMgrCtlId_t ctlId )
{
  cmTmRC_t       rc     = kOkTmRC;
  cmTm_t*        p      = _cmTmHandleToPtr(h);
  cmTmInst_t*    ip     = NULL;

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
    case kNoneTmId:
      break;

    case kStartTmId:
      // Acting on a 'start' cmd only makes sense if the previous command was 'pause'
      if( ip->ctlId == kPauseTmId )
        ip->ctlId = kStartTmId;
      break;

    case kPauseTmId:

      // Acting on a 'pause' command only makes sense if this is the first command
      // or the previous command was a 'start'
      if( ip->ctlId == kNoneTmId || ip->ctlId == kStartTmId )
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
  
const void* cmTaskMgrResult( cmTaskMgrH_t h, unsigned instId )
{
  cmTm_t*     p  = _cmTmHandleToPtr(h);
  cmTmInst_t* ip = NULL;

  if((ip = _cmTmInstFromId(p,instId)) == NULL )
  {
    cmErrMsg(&p->err,kInvalidArgTmRC,"The task instance associated with id %i could not be found.",instId);
    return NULL;
  }

  return ip->result;

}

unsigned    cmTaskMgrResultByteCount( cmTaskMgrH_t h, unsigned instId )
{
  cmTm_t*     p  = _cmTmHandleToPtr(h);
  cmTmInst_t* ip = NULL;

  if((ip = _cmTmInstFromId(p,instId)) == NULL )
  {
    cmErrMsg(&p->err,kInvalidArgTmRC,"The task instance associated with id %i could not be found.",instId);
    return 0;
  }

  return ip->resultByteCnt;
}

cmTmRC_t    cmTaskMgrInstDelete(    cmTaskMgrH_t h, unsigned instId )
{
  cmTmRC_t rc = kOkTmRC;
  cmTm_t*  p  = _cmTmHandleToPtr(h);
  cmTmInst_t* ip = NULL;

  if((ip = _cmTmInstFromId(p,instId)) == NULL )
  {
    cmErrMsg(&p->err,kInvalidArgTmRC,"The task instance associated with id %i could not be found.",instId);
    return 0;
  }

  ip->deleteOnCompleteFl = true;

  return rc;
}


cmTaskMgrCtlId_t _cmTaskMgrHelper( cmTaskMgrFuncArg_t* a, unsigned prog, cmStatusTmId_t statusId )
{
  cmTaskMgrStatusArg_t s;

  _cmTaskMgrStatusArgSetup(
    &s,
    a->statusCbArg,
    a->instId,
    statusId == kInvalidTmId ? kProgTmId    : kStatusTmId,
    statusId == kInvalidTmId ? kStartedTmId : statusId,
    statusId == kInvalidTmId ? prog         : 0,
    NULL,NULL,0);

  a->statusCb(&s);

  return cmTaskMgrHandleCommand(a);
}

cmTaskMgrCtlId_t cmTaskMgrHandleCommand( cmTaskMgrFuncArg_t* a )
{
  cmTmThread_t* trp = a->reserved;

  while( trp->inst->ctlId == kPauseTmId )
  {
    // change the instance status to 'paused'.
    trp->inst->status = kPausedTmId;

    // notify the client of the change in state
    cmTaskMgrSendStatus(a,kPausedTmId);

    // sleep the thread for pauseSleepMs milliseconds
    cmSleepMs(a->pauseSleepMs);

    // if the task was unpaused while we slept
    if( trp->inst->ctlId == kStartTmId )
    {
      // change the instance status to 'started'.
      trp->inst->status = kStartedTmId;

      // notify the client of the change in state
      cmTaskMgrSendStatus(a,kStartedTmId);
    }
  }

  // if ctlId==kKillTmId then the status update will be handled 
  // when the task custom function returns in  _cmTmWorkerThreadFunc()

  return trp->inst->ctlId;
}

cmTaskMgrCtlId_t cmTaskMgrSendStatus( cmTaskMgrFuncArg_t* a, cmStatusTmId_t statusId )
{ return _cmTaskMgrHelper(a,0,statusId); }

cmTaskMgrCtlId_t cmTaskMgrSendProgress( cmTaskMgrFuncArg_t* a, unsigned prog )
{ return _cmTaskMgrHelper(a,prog,kInvalidTmId);  }


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
        const cmChar_t* label = "<none>"; 
        switch( s->statusId )
        {
          case kInvalidTmId:       label="<Invalid>"; break;
          case kQueuedTmId:        label="Queued";  break;
          case kStartedTmId:       label="Started"; break;
          case kCompletedTmId:     label="Completed"; break;
          case kKilledTmId:        label="Killed"; break;
          default:
            { assert(0); }        
        }
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

  unsigned prog = 0;

  for(; prog<arg->progCnt; ++prog)
  {
    if( cmTaskMgrHandleCommand(arg) == kKillTmId )
      break;

    cmSleepMs(1000); 

    if( cmTaskMgrSendProgress(arg,prog) == kKillTmId )
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
  if( cmTaskMgrInstall(tmH, taskId, taskLabel, _cmTmTestFunc ) != kOkTmRC )
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
            if( cmTaskMgrCall( tmH, taskId, funcArg, progCnt, &app.insts[nextInstId].instId ) != kOkTmRC )
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
