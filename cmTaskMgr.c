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
  cmTaskMgrCtlId_t     ctlId;
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
  unsigned            threadCnt;        //
  cmTaskMgrStatusCb_t statusCb;     // 
  void*               statusCbArg;  // 
  unsigned            pauseSleepMs;
  cmTs1p1cH_t         inQueH;       // client->mgr
  cmTsMp1cH_t         outQueH;      // mgr->client
  cmTmTask_t*         tasks;        //
  cmTmInst_t*         insts;        //
  unsigned            nextInstId;
} cmTm_t;

// WARNING: THIS FUNCTION IS CALLED BY BOTH THE WORKER AND THE MASTER THREAD.
cmTmRC_t _cmTmEnqueueStatusMsg0( cmTm_t* p, const cmTaskMgrStatusArg_t* s )
{
  enum { arrayCnt = 3 };
  const void*    msgPtrArray[arrayCnt];
  unsigned       msgSizeArray[arrayCnt];

  msgPtrArray[0] = s;
  msgPtrArray[1] = s->msg==NULL ? "" : s->msg;
  msgPtrArray[2] = s->result;

  msgSizeArray[0] = sizeof(*s);
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
  s.arg           = p->statusCbArg;
  s.instId        = instId;
  s.selId         = selId;
  s.statusId      = statusId;
  s.msg           = msg;
  s.result        = result;
  s.resultByteCnt = resultByteCnt;
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

  r.arg         = trp->inst->funcArg;
  r.instId      = trp->inst->instId;
  r.statusCb    = _cmTmWorkerStatusCb;
  r.statusCbArg = trp;
  r.progCnt     = trp->inst->progCnt;
  r.cmdIdPtr    = &trp->inst->ctlId;
  r.pauseSleepMs= trp->p->pauseSleepMs;

  // Notify the client that the instance has started.
  _cmTmEnqueueStatusMsg1(trp->p,trp->inst->instId,kStatusTmId,kStartedTmId,0,NULL,NULL,0);

  // Execute the client provided task function.
  trp->inst->task->func(&r);

  // Notify the client that the instance has completed or been killed
  if( trp->inst->ctlId == kKillTmId )
    _cmTmEnqueueStatusMsg1(trp->p,trp->inst->instId,kStatusTmId,kKilledTmId,0,NULL,NULL,0);
  else
    _cmTmEnqueueStatusMsg1(trp->p,trp->inst->instId,kStatusTmId,kCompletedTmId,0,NULL,NULL,0);

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
    
    if( cmThreadPause(p->thArray[i].thH,kWaitThFl) != kOkThRC )
    {
      /// ??????? HOW DO WE HANDLE ERRORS IN THE MASTER THREAD
    }

  }

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
  cmTmRC_t    rc = kOkTmRC;
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
      break;
    }
    pp = ip;
  }

  return rc;
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

 errLabel:
  return rc;  
}

cmTmRC_t cmTaskMgrDestroy( cmTaskMgrH_t* hp )
{
  cmTmRC_t rc = kOkTmRC;
  if( hp!=NULL || cmTaskMgrIsValid(*hp)==false )
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
  memcpy(&s,&msgDataPtr,sizeof(s));

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

  while( cmTsMp1cMsgWaiting(p->outQueH) )
  {
    // calling this function calls: _cmTmMasterOutQueueCb()
    if(cmTsMp1cDequeueMsg(p->outQueH,NULL,0) != kOkThRC )
    {
      rc = cmErrMsg(&p->err,kQueueFailTmRC,"The output queue failed during a dequeue.");
      goto errLabel;
    }

    
  }

 errLabel:
  return rc;
}

bool     cmTaskMgrIsEnabled( cmTaskMgrH_t h )
{
  cmTm_t*  p = _cmTmHandleToPtr(h);

  if( cmThreadState(p->thArray[0].thH) != kPausedThId )
    return false;

  return true;
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

  if((tp = _cmTmTaskFromId(p,taskId)) != NULL )
  {
    rc = cmErrMsg(&p->err,kInvalidArgTmRC,"Task not found for task id=%i.",taskId);
    goto errLabel;
  }

  ip = cmMemAllocZ(cmTmInst_t,1);

  ip->instId         = p->nextInstId++;
  ip->task           = tp;
  ip->funcArg        = funcArg;
  ip->progCnt        = progCnt;
  ip->status         = kQueuedTmId;

  if( p->insts == NULL )
    p->insts = ip;
  else
  {
    cmTmInst_t* pp = p->insts;
    while( pp != NULL )
      if( pp->link == NULL )
        pp->link = ip;
  }


  // enque the instance ptr in the input queue 
  if( cmTs1p1cEnqueueMsg(p->inQueH,&ip,sizeof(ip)) != kOkThRC )
  {
    rc = cmErrMsg(&p->err,kQueueFailTmRC,"New task instance command enqueue failed.");    
    goto errLabel;
  }

  // notify the client that the instance was enqueued
  // ???????????????
  // (is this ok??? - we are inserting into p->outQueH from the client thread)
  // it would be safe to simply callback directly
  // ???????????????
  if( _cmTmEnqueueStatusMsg1(p,ip->instId,kStatusTmId,kQueuedTmId,0,NULL,NULL,0) != kOkTmRC )
  {
    cmErrMsg(&p->err,kQueueFailTmRC,"The 'queued' status update message failed to enqueue.");
    goto errLabel;
  }

  
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

  // once the ctl id is set to kKillTmId don't allow it to change
  if( ip->ctlId != kKillTmId )
    ip->ctlId = ctlId;

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

cmTmRC_t    cmTaskMgrResultDelete(    cmTaskMgrH_t h, unsigned instId )
{
  cmTmRC_t rc = kOkTmRC;
  cmTm_t*  p  = _cmTmHandleToPtr(h);

  if((rc = _cmTmInstFree(p,instId)) != kOkTmRC )
    rc = cmErrMsg(&p->err,kOpFailTmRC,"The instace delete failed on instance id %i.",instId);

  return rc;
}


cmTaskMgrCtlId_t cmTaskMgrHandleCommand( cmTaskMgrFuncArg_t* a )
{

  while( *(a->cmdIdPtr) == kPauseTmId )
  {
    // ????????
    // maybe we should send a status code to notify the client that the instance has paused.
    ///
    cmSleepMs(a->pauseSleepMs);
  }

  return *(a->cmdIdPtr);
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
  cmRptPrintf(rpt,"%i ",s->instId );

  switch( s->selId )
  {
    case kStatusTmId:
      {
        const cmChar_t* label = "<none>"; 
        switch( s->statusId )
        {
          case kInvalidTmId:       label="<Invalid>"; break;
          case kQueuedTmId:        label="Queued";  break;
          case kQueuedPausedTmId:  label="Queued-Paused."; break;
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


void _cmTmTestFunc(cmTaskMgrFuncArg_t* arg )
{
  cmTaskMgrStatusArg_t s;
  memset(&s,0,sizeof(s));
  s.arg           = arg->statusCbArg;
  s.instId        = arg->instId;
  s.selId         = kProgTmId;
  s.statusId      = kStartedTmId;
  s.prog          = 0;
  s.msg           = NULL;
  s.result        = NULL;
  s.resultByteCnt = 0;

  for(; s.prog<arg->progCnt; ++s.prog)
  {
    if( cmTaskMgrHandleCommand(arg) == kKillTmId )
      break;

    cmSleepMs(1000); 
    arg->statusCb(&s);
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
  while((c = getchar()) != 'q')
  {
    switch(c)
    {
      case 'e':
        {
          // toggle the enable state of the task mgr.
          bool fl = cmTaskMgrIsEnabled(tmH);
          if( cmTaskMgrEnable(tmH,!fl) != kOkTmRC )
            rc = cmErrMsg(&ctx->err,kTestFailTmRC,"Test enable failed.");
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
              ++nextInstId;
          }
    }
  }

  

 errLabel:
  // destroy the task mgr
  if( cmTaskMgrDestroy(&tmH) != kOkTmRC )
    rc = cmErrMsg(&ctx->err,kTestFailTmRC,"Task mgr destroy failed.");

  return rc;
}
