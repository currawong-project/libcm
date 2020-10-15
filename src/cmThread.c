//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#include "cmPrefix.h"
#include "cmGlobal.h"

#include "cmRpt.h"
#include "cmErr.h"
#include "cmMem.h"
#include "cmMallocDebug.h"

#include "cmThread.h"

#include <pthread.h>
#include <unistd.h>  // usleep

#ifdef OS_OSX
#include <libkern/OSAtomic.h>
#endif

cmThreadH_t cmThreadNullHandle = {NULL};

enum
{
  kDoExitThFl  = 0x01,
  kDoPauseThFl = 0x02,
  kDoRunThFl   = 0x04
};

typedef struct
{
  cmErr_t        err;
  cmThreadFunc_t funcPtr;
  pthread_t      pthreadH;
  cmThStateId_t  state;
  void*          funcParam;
  unsigned       doFlags;
  unsigned       pauseMicroSecs;
  unsigned       waitMicroSecs;
} cmThThread_t;

cmThRC_t _cmThError( cmErr_t* err, cmThRC_t rc, int sysErr, const char* fmt, ... )
{ 
  va_list vl;
  va_start(vl,fmt);
  cmErrVSysMsg(err,rc,sysErr,fmt,vl);
  va_end(vl);
  return rc;
}

void _cmThThreadCleanUpCallback(void* t)
{
  ((cmThThread_t*)t)->state = kExitedThId;
}

void* _cmThThreadCallback(void* param)
{
  cmThThread_t* t = (cmThThread_t*)param;

	// set a clean up handler - this will be called when the 
	// thread terminates unexpectedly or pthread_cleanup_pop() is called.
	pthread_cleanup_push(_cmThThreadCleanUpCallback,t);

  while( cmIsFlag(t->doFlags,kDoExitThFl) == false )
  {

    if( t->state == kPausedThId )
    {
      cmSleepUs( t->pauseMicroSecs );

      if( cmIsFlag(t->doFlags,kDoRunThFl) )
      {
        t->doFlags = cmClrFlag(t->doFlags,kDoRunThFl);
        t->state   = kRunningThId;
      }
    }
    else
    {

      if( t->funcPtr(t->funcParam)==false )
        break;

      if( cmIsFlag(t->doFlags,kDoPauseThFl) )
      {
        t->doFlags = cmClrFlag(t->doFlags,kDoPauseThFl);
        t->state   = kPausedThId;        
      }
    }
  }

	pthread_cleanup_pop(1);
	
  pthread_exit(NULL);

  return t;
}

cmThThread_t* _cmThThreadFromHandle( cmThreadH_t h )
{
  cmThThread_t* tp = (cmThThread_t*)h.h;

  assert(tp != NULL);
  
  return tp->state==kNotInitThId ? NULL : tp;
}

cmThRC_t _cmThWaitForState( cmThThread_t* t, unsigned stateId )
{
  unsigned waitTimeMicroSecs = 0;

  while( t->state != stateId && waitTimeMicroSecs < t->waitMicroSecs )
  {
    //cmSleepUs( t->waitMicroSecs );
    cmSleepUs( 15000 );
    waitTimeMicroSecs += 15000; //t->waitMicroSecs;
  }

  return t->state==stateId ? kOkThRC :  kTimeOutThRC;
}

cmThRC_t cmThreadCreate(  cmThreadH_t* hPtr, cmThreadFunc_t funcPtr, void* funcParam, cmRpt_t* rpt )
{
  //pthread_attr_t attr;
  cmThRC_t         rc     = kOkThRC;
  cmThThread_t*    tp     = cmMemAllocZ( cmThThread_t, 1 );
  int            sysErr;

  cmErrSetup(&tp->err,rpt,"Thread");

  tp->funcPtr        = funcPtr;
  tp->funcParam      = funcParam;
  tp->state          = kPausedThId;
  tp->doFlags        = 0;
  tp->pauseMicroSecs = 50000;
  tp->waitMicroSecs  = 1000000;

  if((sysErr = pthread_create(&tp->pthreadH,NULL,_cmThThreadCallback, (void*)tp )) != 0 )
  {
    tp->state = kNotInitThId;
    rc = _cmThError(&tp->err,kCreateFailThRC,sysErr,"Thread create failed.");
  }

  hPtr->h = tp;

  return rc;
}


cmThRC_t cmThreadDestroy( cmThreadH_t* hPtr )
{
  cmThRC_t      rc     = kOkThRC;

  if( hPtr==NULL || cmThreadIsValid(*hPtr)==false )
    return rc;

  cmThThread_t* t      = _cmThThreadFromHandle(*hPtr );

  if( t == NULL )
    return kInvalidHandleThRC;

  // tell the thread to exit
  t->doFlags = cmSetFlag(t->doFlags,kDoExitThFl);

  // wait for the thread to exit and then deallocate the thread object
  if((rc = _cmThWaitForState(t,kExitedThId)) == kOkThRC )
  {
    cmMemFree(t);
    hPtr->h = NULL;
  }
  else
  {
    rc = _cmThError(&t->err,rc,0,"Thread timed out waiting for destroy.");
  }

  return rc;
}

cmThRC_t cmThreadPause(   cmThreadH_t h, unsigned cmdFlags )
{
  cmThRC_t    rc         = kOkThRC;
  bool        pauseFl    = cmIsFlag(cmdFlags,kPauseThFl);
  bool        waitFl     = cmIsFlag(cmdFlags,kWaitThFl);
  
  cmThThread_t* t        = _cmThThreadFromHandle(h);
  unsigned      waitId;

  if( t == NULL )
    return kInvalidHandleThRC;

  bool        isPausedFl = t->state == kPausedThId;
 
  if( isPausedFl == pauseFl )
    return kOkThRC;

  if( pauseFl )
  {
    t->doFlags = cmSetFlag(t->doFlags,kDoPauseThFl);
    waitId = kPausedThId;
  }
  else
  {
    t->doFlags = cmSetFlag(t->doFlags,kDoRunThFl);
    waitId = kRunningThId;
  }

  if( waitFl )
    rc = _cmThWaitForState(t,waitId);

  if( rc != kOkThRC )
    _cmThError(&t->err,rc,0,"Thread timed out waiting for '%s'.", pauseFl ? "pause" : "un-pause");
  
  return rc;
}

cmThStateId_t cmThreadState( cmThreadH_t h )
{
  cmThThread_t* tp = _cmThThreadFromHandle(h);

  if( tp == NULL )
    return kNotInitThId;

  return tp->state;
}

bool cmThreadIsValid( cmThreadH_t h )
{ return h.h != NULL; }

unsigned      cmThreadPauseTimeOutMicros( cmThreadH_t h )
{ 
  cmThThread_t* tp = _cmThThreadFromHandle(h);
  return tp->pauseMicroSecs;
}

void          cmThreadSetPauseTimeOutMicros( cmThreadH_t h, unsigned usecs )
{ 
  cmThThread_t* tp = _cmThThreadFromHandle(h);
  tp->pauseMicroSecs = usecs;
}

unsigned      cmThreadWaitTimeOutMicros(  cmThreadH_t h )
{ 
  cmThThread_t* tp = _cmThThreadFromHandle(h);
  return tp->waitMicroSecs;
}

void          cmThreadSetWaitTimeOutMicros( cmThreadH_t h, unsigned usecs )
{ 
  cmThThread_t* tp = _cmThThreadFromHandle(h);
  tp->waitMicroSecs = usecs;
}


bool _cmThreadTestCb( void* p )
{
  unsigned* ip = (unsigned*)p;
  ip[0]++;
  return true;
}


void cmThreadTest(cmRpt_t* rpt)
{
  cmThreadH_t th0;
  unsigned    val = 0;

  if( cmThreadCreate(&th0,_cmThreadTestCb,&val,rpt) == kOkThRC )
  {
    if( cmThreadPause(th0,0) != kOkThRC )
    {
      cmRptPrintf(rpt,"Thread start failed.\n");
      return;
    }

    char c = 0;

    cmRptPrintf(rpt,"o=print p=pause s=state q=quit\n");

    while( c != 'q' )
    {

      c = (char)fgetc(stdin);
      fflush(stdin);

      switch(c)
      {
        case 'o':
          cmRptPrintf(rpt,"val: 0x%x\n",val);
          break;

        case 's':
          cmRptPrintf(rpt,"state=%i\n",cmThreadState(th0));
          break;

        case 'p':
        {
          cmRC_t rc;
          if( cmThreadState(th0) == kPausedThId )
            rc = cmThreadPause(th0,kWaitThFl);
          else
            rc = cmThreadPause(th0,kPauseThFl|kWaitThFl);

          if( rc == kOkThRC )
            cmRptPrintf(rpt,"new state:%i\n", cmThreadState(th0));
          else
            cmRptPrintf(rpt,"cmThreadPause() failed.");

        }
        break;
        
        case 'q':
          break;

          //default:
          //cmRptPrintf(rpt,"Unknown:%c\n",c);
          
      }
    }

    if( cmThreadDestroy(&th0) != kOkThRC )
      cmRptPrintf(rpt,"Thread destroy failed.\n");
  }

}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

typedef struct 
{
  cmErr_t         err;
  pthread_mutex_t mutex;
  pthread_cond_t  cvar;
} cmThreadMutex_t;

cmThreadMutexH_t kThreadMutexNULL = {NULL};


cmThreadMutex_t* _cmThreadMutexFromHandle( cmThreadMutexH_t h )
{
  cmThreadMutex_t* p = (cmThreadMutex_t*)h.h;
  assert(p != NULL);
  return p;
}

cmThRC_t cmThreadMutexCreate(   cmThreadMutexH_t* hPtr, cmRpt_t* rpt )
{
  int sysErr;
  cmThreadMutex_t* p = cmMemAllocZ( cmThreadMutex_t, 1 );

  cmErrSetup(&p->err,rpt,"Thread Mutex");

  if((sysErr = pthread_mutex_init(&p->mutex,NULL)) != 0 )
    return _cmThError(&p->err,kCreateFailThRC,sysErr,"Thread mutex create failed.");

  if((sysErr = pthread_cond_init(&p->cvar,NULL)) != 0 )
    return _cmThError(&p->err,kCreateFailThRC,sysErr,"Thread Condition var. create failed.");

  hPtr->h = p;
  return kOkThRC;
}

cmThRC_t cmThreadMutexDestroy(  cmThreadMutexH_t* hPtr )
{
  int sysErr;
  cmThreadMutex_t* p = _cmThreadMutexFromHandle(*hPtr);

  if( p == NULL )
    return kInvalidHandleThRC;

  if((sysErr = pthread_cond_destroy(&p->cvar)) != 0)
    return _cmThError(&p->err,kDestroyFailThRC,sysErr,"Thread condition var. destroy failed.");

  if((sysErr = pthread_mutex_destroy(&p->mutex)) != 0)
    return _cmThError(&p->err,kDestroyFailThRC,sysErr,"Thread mutex destroy failed.");


  cmMemFree(p);
  hPtr->h = NULL;

  return kOkThRC;
}

cmThRC_t cmThreadMutexTryLock(  cmThreadMutexH_t  h, bool* lockFlPtr )
{
  cmThreadMutex_t* p = _cmThreadMutexFromHandle(h);

  if( p == NULL )
    return kInvalidHandleThRC;

  int sysErr = pthread_mutex_trylock(&p->mutex);

  switch(sysErr)
  {
    case EBUSY:
      *lockFlPtr = false;
      break;

    case 0:
      *lockFlPtr = true;
      break;

    default:
      return _cmThError(&p->err,kLockFailThRC,sysErr,"Thread mutex try-lock failed.");;
  }

  return kOkThRC;
}

cmThRC_t cmThreadMutexLock(     cmThreadMutexH_t  h )
{
  cmThreadMutex_t* p = _cmThreadMutexFromHandle(h);

  if( p == NULL )
    return kInvalidHandleThRC;

  int sysErr = pthread_mutex_lock(&p->mutex);

  if( sysErr == 0 )
    return kOkThRC;

  return _cmThError(&p->err,kLockFailThRC,sysErr,"Thread mutex lock failed.");
}

cmThRC_t cmThreadMutexUnlock(   cmThreadMutexH_t  h )
{
  cmThreadMutex_t* p = _cmThreadMutexFromHandle(h);

  if( p == NULL )
    return kInvalidHandleThRC;

  int sysErr = pthread_mutex_unlock(&p->mutex);

  if( sysErr == 0 )
    return kOkThRC;

  return _cmThError(&p->err,kUnlockFailThRC,sysErr,"Thread mutex unlock failed.");
}

bool cmThreadMutexIsValid( cmThreadMutexH_t h )
{ return h.h != NULL; }

cmThRC_t cmThreadMutexWaitOnCondVar( cmThreadMutexH_t h, bool lockFl )
{
  cmThreadMutex_t* p = _cmThreadMutexFromHandle(h);

  if( p == NULL )
    return kInvalidHandleThRC;

  int sysErr;
  
  if( lockFl )
    if( (sysErr=pthread_mutex_lock(&p->mutex)) != 0 )
      _cmThError(&p->err,kLockFailThRC,sysErr,"Thread lock failed on cond. var. wait.");

  if((sysErr = pthread_cond_wait(&p->cvar,&p->mutex)) != 0 )
    _cmThError(&p->err,kCVarWaitFailThRC,sysErr,"Thread cond. var. wait failed.");
        
  return kOkThRC;
}

cmThRC_t cmThreadMutexSignalCondVar( cmThreadMutexH_t h )
{
  int sysErr;
  cmThreadMutex_t* p = _cmThreadMutexFromHandle(h);

  if( p == NULL )
    return kInvalidHandleThRC;

  if((sysErr = pthread_cond_signal(&p->cvar)) != 0 )
    return _cmThError(&p->err,kCVarSignalFailThRC,sysErr,"Thread cond. var. signal failed.");

  return kOkThRC;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

cmTsQueueH_t cmTsQueueNullHandle = { NULL };

enum { cmTsQueueBufCnt = 2 };


typedef struct
{
  unsigned  allocCnt;  // count of bytes allocated for the buffer 
  unsigned  fullCnt;   // count of bytes used in the buffer
  char*     basePtr;   // base of buffer memory
  unsigned* msgPtr;    // pointer to first msg 
  unsigned  msgCnt;
} cmTsQueueBuf;

typedef struct
{
  cmThreadMutexH_t mutexH;
  cmTsQueueBuf     bufArray[cmTsQueueBufCnt];
  unsigned         inBufIdx;
  unsigned         outBufIdx;
  char*            memPtr;
  cmTsQueueCb_t    cbFunc;
  void*            userCbPtr;
} cmTsQueue_t;

cmTsQueue_t* _cmTsQueueFromHandle( cmTsQueueH_t h )
{
  cmTsQueue_t* p = h.h;
  assert(p != NULL);
  return p;
}

cmThRC_t _cmTsQueueDestroy( cmTsQueue_t* p )
{
  cmThRC_t rc;

  if( p == NULL )
    return kInvalidHandleThRC;

  if( p->mutexH.h != NULL )
    if((rc = cmThreadMutexDestroy(&p->mutexH)) != kOkThRC )
      return rc;

  if( p->memPtr != NULL )
    cmMemPtrFree(&p->memPtr);


  cmMemPtrFree(&p);


  return kOkThRC;
}


cmThRC_t   cmTsQueueCreate(     cmTsQueueH_t* hPtr, unsigned bufByteCnt, cmTsQueueCb_t cbFunc, void* userCbPtr, cmRpt_t* rpt )
{
  cmTsQueue_t* p = cmMemAllocZ( cmTsQueue_t, 1 );
  unsigned   i;

  if( cmThreadMutexCreate(&p->mutexH,rpt) != kOkThRC )
    goto errLabel;  

  p->memPtr    = cmMemAllocZ( char, bufByteCnt*cmTsQueueBufCnt );
  p->outBufIdx = 0;
  p->inBufIdx  = 1;
  p->cbFunc    = cbFunc;
  p->userCbPtr = userCbPtr;

  for(i=0; i<cmTsQueueBufCnt; ++i)
  {
    p->bufArray[i].allocCnt = bufByteCnt;
    p->bufArray[i].fullCnt  = 0;
    p->bufArray[i].basePtr  = p->memPtr + (i*bufByteCnt);
    p->bufArray[i].msgPtr   = NULL;
    p->bufArray[i].msgCnt   = 0;
  }

  hPtr->h = p;

  return kOkThRC;
  
  errLabel:
  
  _cmTsQueueDestroy(p);

  return kCreateFailThRC;
}


cmThRC_t   cmTsQueueDestroy(    cmTsQueueH_t*  hPtr )
{
  cmThRC_t rc = kOkThRC;

  if( (hPtr != NULL) && cmTsQueueIsValid(*hPtr))
    if((rc = _cmTsQueueDestroy(_cmTsQueueFromHandle(*hPtr))) == kOkThRC )
      hPtr->h = NULL;

  return rc;
}

cmThRC_t   cmTsQueueSetCallback( cmTsQueueH_t h, cmTsQueueCb_t cbFunc, void* cbArg )
{
  cmTsQueue_t* p = _cmTsQueueFromHandle(h);
  p->cbFunc    = cbFunc;
  p->userCbPtr = cbArg;
  return kOkThRC;
}

unsigned cmTsQueueAllocByteCount( cmTsQueueH_t h )
{
  cmTsQueue_t* p = _cmTsQueueFromHandle(h);
  unsigned     n = 0;

  if( cmThreadMutexLock(p->mutexH) == kOkThRC )
  {
    n = p->bufArray[ p->inBufIdx ].allocCnt;
    cmThreadMutexUnlock(p->mutexH);
  }
  return n;
}

unsigned cmTsQueueAvailByteCount( cmTsQueueH_t h )
{
  cmTsQueue_t* p = _cmTsQueueFromHandle(h);
  unsigned     n = 0;

  if(cmThreadMutexLock(p->mutexH) == kOkThRC )
  {
    n =  p->bufArray[ p->inBufIdx ].allocCnt - p->bufArray[ p->inBufIdx].fullCnt;
    cmThreadMutexUnlock(p->mutexH);
  }
  return n;
}

cmThRC_t   _cmTsQueueEnqueueMsg( cmTsQueueH_t  h, const void* msgPtrArray[], unsigned msgByteCntArray[], unsigned arrayCnt )
{
  cmThRC_t     rc;
  cmTsQueue_t* p = _cmTsQueueFromHandle(h);


  if( p == NULL )
    return kInvalidHandleThRC;

  // lock the mutex
  if((rc = cmThreadMutexLock(p->mutexH)) == kOkThRC )
  {
    
    cmTsQueueBuf*  b          = p->bufArray + p->inBufIdx;            // ptr to buf recd
    const char*    ep         = b->basePtr  + b->allocCnt;            // end of buf data space
    unsigned      *mp         = (unsigned*)(b->basePtr + b->fullCnt); // ptr to size of new msg space
    char*          dp         = (char*)(mp+1);                        // ptr to data area of new msg space
    unsigned       ttlByteCnt = 0;                                    // track size of msg data
    unsigned       i          = 0;   

    // get the total size of the msg
    for(i=0; i<arrayCnt; ++i)
      ttlByteCnt += msgByteCntArray[i];

    // if the msg is too big for the queue buf
    if( dp + ttlByteCnt > ep )
      rc = kBufFullThRC;
    else
    {
      // for each segment of the incoming msg
      for(i=0; i<arrayCnt; ++i)
      {
        // get the size of the segment
        unsigned n = msgByteCntArray[i]; 

        // copy in the segment
        memcpy(dp,msgPtrArray[i],n);
      
        dp   += n; //
      }

      assert(dp <= ep );

      // write the size ofthe msg into the buffer
      *mp = ttlByteCnt;

      // update the pointer to the first msg
      if( b->msgPtr == NULL )
        b->msgPtr = mp;

      // track the count of msgs in this buffer
      ++b->msgCnt;

      // update fullCnt last since dequeue uses fullCnt to
      // notice that a msg may be waiting
      b->fullCnt += sizeof(unsigned) + ttlByteCnt;    
    }

    cmThreadMutexUnlock(p->mutexH);
  }

  return rc;
}

cmThRC_t   cmTsQueueEnqueueSegMsg( cmTsQueueH_t h, const void* msgPtrArray[], unsigned msgByteCntArray[], unsigned arrayCnt )
{ return _cmTsQueueEnqueueMsg(h,msgPtrArray,msgByteCntArray,arrayCnt); }

cmThRC_t   cmTsQueueEnqueueMsg( cmTsQueueH_t h, const void* dataPtr, unsigned byteCnt )
{ 
  const void*  msgPtrArray[]     = { dataPtr };
  unsigned     msgByteCntArray[] = { byteCnt };

  return _cmTsQueueEnqueueMsg(h,msgPtrArray,msgByteCntArray,1);
}

cmThRC_t   cmTsQueueEnqueueIdMsg( cmTsQueueH_t h, unsigned id, const void* dataPtr, unsigned byteCnt )
{   
  const void* msgPtrArray[]     = { &id, dataPtr };
  unsigned    msgByteCntArray[] = { sizeof(id), byteCnt };

  return _cmTsQueueEnqueueMsg(h,msgPtrArray,msgByteCntArray,2);
}

cmThRC_t   _cmTsQueueDequeueMsg( cmTsQueue_t* p, void* retBuf, unsigned refBufByteCnt )
{
  cmTsQueueBuf* b = p->bufArray + p->outBufIdx;

  // if the output buffer is empty - there is nothing to do
  if( b->fullCnt == 0 )
    return kBufEmptyThRC;

  assert( b->msgPtr != NULL );

  // get the output msg size and data
  unsigned msgByteCnt = *b->msgPtr;
  char*    msgDataPtr = (char*)(b->msgPtr + 1);

  // transmit the msg via a callback
  if( retBuf == NULL && p->cbFunc != NULL )
    p->cbFunc(p->userCbPtr,msgByteCnt,msgDataPtr);
  else
  {
    // retBuf may be NULL if the func is being used by cmTsQueueDequeueByteCount()
    if( retBuf == NULL || msgByteCnt > refBufByteCnt )
      return kBufTooSmallThRC;

    // copy the msg to a buffer
    if( retBuf != NULL )
      memcpy(retBuf,msgDataPtr,msgByteCnt);

  }
  
  // update the buffer
  b->fullCnt -= sizeof(unsigned) + msgByteCnt;  
  b->msgPtr   = (unsigned*)(msgDataPtr + msgByteCnt);
  --(b->msgCnt);

  if( b->fullCnt == 0 )
  {
    assert(b->msgCnt == 0);
    b->msgPtr = NULL;
  }
  
  return kOkThRC;

}

cmThRC_t   cmTsQueueDequeueMsg( cmTsQueueH_t  h,  void* retBuf, unsigned refBufByteCnt )
{
  cmThRC_t rc;
  cmTsQueue_t* p = _cmTsQueueFromHandle(h);
  
  if( p == NULL )
    return kInvalidHandleThRC;

  // dequeue the next msg from the current output buffer
  if((rc =_cmTsQueueDequeueMsg( p, retBuf, refBufByteCnt )) != kBufEmptyThRC )
    return rc;
  
  // the current output buffer was empty 

  cmTsQueueBuf* b = p->bufArray + p->inBufIdx;

  // if the input buffer has msg's ...
  if( b->fullCnt > 0 )
  {
    bool lockFl = false;

    //  ...attempt to lock the mutex ...
    if( (cmThreadMutexTryLock(p->mutexH,&lockFl) == kOkThRC) && lockFl )
    {
      // ... swap the input and the output buffers ...
      unsigned tmp = p->inBufIdx;
      p->inBufIdx  = p->outBufIdx;
      p->outBufIdx = tmp;

      // .. unlock the mutex 
      cmThreadMutexUnlock(p->mutexH);

      // ... and dequeue the first msg from the new output buffer
      rc = _cmTsQueueDequeueMsg( p, retBuf, refBufByteCnt );      
    }
    
  }

  return rc;
  
}

bool     cmTsQueueMsgWaiting( cmTsQueueH_t  h )
{
  cmTsQueue_t* p = _cmTsQueueFromHandle(h);
  
  if( p == NULL )
    return false;

  if( p->bufArray[p->outBufIdx].fullCnt )
    return true;

  return p->bufArray[p->inBufIdx].fullCnt > 0;    
}

unsigned cmTsQueueDequeueMsgByteCount( cmTsQueueH_t h )
{
  cmTsQueue_t* p = _cmTsQueueFromHandle(h);
  
  if( p == NULL )
    return 0;

  // if output msgs are available then the msgPtr points to the size of the msg
  if( p->bufArray[p->outBufIdx].fullCnt )
    return *(p->bufArray[p->outBufIdx].msgPtr);

  // no msgs are waiting in the output buffer
  
  // force the buffers to swap - returns kBufEmptyThRC if there are
  // still no msgs waiting after the swap (the input buf was also empty)
  if( cmTsQueueDequeueMsg(h,NULL,0) == kBufTooSmallThRC )
  {
    // the buffers swapped so there must be msg waiting
    assert( p->bufArray[p->outBufIdx].fullCnt );

    return *(p->bufArray[p->outBufIdx].msgPtr);
  }

  return 0;
}

bool cmTsQueueIsValid( cmTsQueueH_t h )
{ return h.h != NULL; }



//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------

typedef struct
{
  volatile unsigned ii;
  cmErr_t           err;
  char*             buf;
  unsigned          bn;
  cmTsQueueCb_t     cbFunc;
  void*             cbArg;
  volatile unsigned oi;
} cmTs1p1c_t;

cmTs1p1c_t* _cmTs1p1cHandleToPtr( cmTs1p1cH_t h )
{
  cmTs1p1c_t* p = (cmTs1p1c_t*)h.h;
  assert( p != NULL );
  return p;
}

cmThRC_t   cmTs1p1cCreate( cmTs1p1cH_t* hPtr, unsigned bufByteCnt, cmTsQueueCb_t cbFunc, void* cbArg, cmRpt_t* rpt )
{
  cmThRC_t rc;
  if((rc = cmTs1p1cDestroy(hPtr)) != kOkThRC )
    return rc;
  
  cmTs1p1c_t* p = cmMemAllocZ(cmTs1p1c_t,1);
  cmErrSetup(&p->err,rpt,"1p1c Queue");
  p->buf        = cmMemAllocZ(char,bufByteCnt+sizeof(unsigned));
  p->ii         = 0;
  p->oi         = 0;
  p->bn         = bufByteCnt;
  p->cbFunc     = cbFunc;
  p->cbArg = cbArg;
  hPtr->h = p;

  return rc;
}

cmThRC_t cmTs1p1cDestroy( cmTs1p1cH_t* hp )
{
  cmThRC_t rc = kOkThRC;

  if( hp == NULL || cmTs1p1cIsValid(*hp)==false )
    return kOkThRC;

  cmTs1p1c_t* p = _cmTs1p1cHandleToPtr(*hp);
  
  cmMemFree(p->buf);
  cmMemFree(p);
  
  hp->h = NULL;
  return rc;
}

cmThRC_t cmTs1p1cSetCallback( cmTs1p1cH_t h, cmTsQueueCb_t cbFunc, void* cbArg )
{
  cmTs1p1c_t* p = _cmTs1p1cHandleToPtr(h);
  p->cbFunc = cbFunc;
  p->cbArg = cbArg;
  return kOkThRC;
}

cmThRC_t   cmTs1p1cEnqueueSegMsg( cmTs1p1cH_t h, const void* msgPtrArray[], unsigned msgByteCntArray[], unsigned arrayCnt )
{
  cmThRC_t    rc = kOkThRC;
  unsigned    mn = 0;
  unsigned    i;
  cmTs1p1c_t* p  = _cmTs1p1cHandleToPtr(h);

  // get the total count of bytes for this msg
  for(i=0; i<arrayCnt; ++i)
    mn += msgByteCntArray[i];

  int dn = mn + sizeof(unsigned);
  int oi = p->oi;
  int bi = p->ii;   // 'bi' is the idx of the leftmost cell which can be written 
  int en = p->bn;   // 'en' is the idx of the cell just to the right of the rightmost cell that can be written

  // note: If 'oi' marks the rightmost location then 'en' must be set
  // one cell to the left of 'oi', because 'ii' can never be allowed to
  // advance onto 'oi' - because 'oi'=='ii' marks an empty (NOT a full)
  // queue. 
  //
  // If 'bn' marks the rightmost location then 'ii' can advance onto 'bn'
  // beause the true queue length is bn+1.

  // if we need to wrap
  if( en-bi < dn && oi<=bi )
  {
    bi = 0;
    en = oi - 1;  // note if oi==0 then en is negative - see note above re: oi==ii
    assert( p->ii>=0 && p->ii <= p->bn );
    *(unsigned*)(p->buf + p->ii) = cmInvalidIdx; // mark the wrap location
  }

  // if oi is between ii and bn
  if( oi > bi )
    en = oi - 1;  // never allow ii to advance onto oi - see note above

  // if the msg won't fit
  if( en - bi < dn )
    return cmErrMsg(&p->err,kBufFullThRC,"%i consecutive bytes is not available in the queue.",dn); 

  // set the msg byte count - the msg byte cnt precedes the msg body
  char* dp = p->buf + bi;
  *(unsigned*)dp = dn - sizeof(unsigned);
  dp += sizeof(unsigned);
  
  // copy the msg into the buffer
  for(i=0,dn=0; i<arrayCnt; ++i)
  {
    memcpy(dp,msgPtrArray[i],msgByteCntArray[i]);
    dp += msgByteCntArray[i];
    dn += msgByteCntArray[i];
  }

  // incrementing p->ii must occur last - the unnecessary accumulation
  // of dn in the above loop is intended to prevent this line from
  // begin moved before the copy loop.
  p->ii = bi + dn + sizeof(unsigned);

  assert( p->ii >= 0 && p->ii <= p->bn);

  return rc;
}

cmThRC_t   cmTs1p1cEnqueueMsg( cmTs1p1cH_t  h, const void* dataPtr, unsigned byteCnt )
{ return cmTs1p1cEnqueueSegMsg(h,&dataPtr,&byteCnt,1); }

unsigned   cmTs1p1cAllocByteCount( cmTs1p1cH_t h )
{
  cmTs1p1c_t* p = _cmTs1p1cHandleToPtr(h);
  return p->bn;
}

unsigned   cmTs1p1cAvailByteCount( cmTs1p1cH_t h )
{
  cmTs1p1c_t* p = _cmTs1p1cHandleToPtr(h);
  unsigned oi = p->oi;
  unsigned ii = p->ii;
  return oi < ii ? p->bn - ii + oi : oi - ii;
}

unsigned _cmTs1p1cDequeueMsgByteCount( cmTs1p1c_t* p )
{
  // if the buffer is empty
  if( p->ii == p->oi )
    return 0;

  // get the length of the next msg
  unsigned mn = *(unsigned*)(p->buf + p->oi);

  // if the msg length is cmInvalidIdx ...
  if( mn == cmInvalidIdx )
  {
    p->oi = 0;  // ... wrap to buf begin and try again

    return _cmTs1p1cDequeueMsgByteCount(p);
  }
  
  return mn;
}

cmThRC_t   cmTs1p1cDequeueMsg( cmTs1p1cH_t  h,  void* dataPtr, unsigned byteCnt )
{
  cmThRC_t    rc = kOkThRC;
  cmTs1p1c_t* p  = _cmTs1p1cHandleToPtr(h);

  unsigned mn;

  if((mn = _cmTs1p1cDequeueMsgByteCount(p)) == 0 )
    return kBufEmptyThRC;

  void* mp = p->buf + p->oi + sizeof(unsigned);

  if( dataPtr != NULL )
  {
    if( byteCnt < mn )
      return cmErrMsg(&p->err,kBufTooSmallThRC,"The return buffer constains too few bytes (%i) to contain %i bytes.",byteCnt,mn);
      
    memcpy(dataPtr,mp,mn);
  }
  else
  {
    p->cbFunc(p->cbArg,mn,mp);
  }

  p->oi += mn + sizeof(unsigned);
  
  return rc;
}


unsigned cmTs1p1cDequeueMsgByteCount( cmTs1p1cH_t h )
{
  cmTs1p1c_t* p  = _cmTs1p1cHandleToPtr(h);
  return _cmTs1p1cDequeueMsgByteCount(p);
}

bool     cmTs1p1cMsgWaiting( cmTs1p1cH_t h )
{  return cmTs1p1cDequeueMsgByteCount(h) > 0;  }

bool cmTs1p1cIsValid( cmTs1p1cH_t h )
{ return h.h != NULL; }

//============================================================================================================================


bool     cmThIntCAS(   int*      addr, int      old, int      new )
{
#ifdef OS_OSX
  int rv = OSAtomicCompareAndSwap32Barrier(old,new,addr);
  return rv;
#endif

#ifdef OS_LINUX 
  return __sync_bool_compare_and_swap(addr,old,new); 
#endif
}

bool     cmThUIntCAS(  unsigned* addr, unsigned old, unsigned new )
{ 
#ifdef OS_OSX
  return OSAtomicCompareAndSwap32Barrier((int)old,(int)new,(int*)addr);
#endif

#ifdef OS_LINUX 
  return __sync_bool_compare_and_swap(addr,old,new); 
#endif
}

bool     cmThFloatCAS( float*    addr, float    old, float    new )
{ 
#ifdef OS_OSX
  return  OSAtomicCompareAndSwap32Barrier(*(int*)(&old),*(int*)(&new),(int*)addr ); 
#endif

#ifdef OS_LINUX
  // If we use pointer aliasing to pun the pointer types then it will violate the
  // strict pointer aliasing rules used by -O2. Instead we use this union
  // punning scheme which in theory should always work in C99 (although no C++).

  typedef union 
  {
    unsigned* up;
    float*    fp;
  } u;

  u u0,u1;
  u0.fp = &old;
  u1.fp = &new;

  return  __sync_bool_compare_and_swap((unsigned*)addr,*u0.up,*u1.up); 
#endif
}


bool     cmThPtrCAS(   void*    addr, void*    old, void*    neww )
{
#ifdef OS_OSX
  
  // REMOVE THIS HACK AND USE OSAtomicCompareAndSwapPtrBarrier() WHEN
  // A 64 BIT BUILD IS POSSIBLE ON OS-X.
 
  typedef struct 
  {
    union 
    {
      void* addr;
#ifdef OSX_VER_10_5
      int val;
#else
      int64_t   val;
#endif
    } u;
  } s_t;

  s_t ov,nv;
  
  ov.u.addr = old;
  nv.u.addr = neww;
#ifdef OSX_VER_10_5
  int rv = OSAtomicCompareAndSwap32Barrier(ov.u.val,nv.u.val,(int*)addr);
#else
  int rv = OSAtomicCompareAndSwap64Barrier(ov.u.val,nv.u.val,(int64_t*)addr);
#endif
  return rv;
#endif

#ifdef OS_LINUX
#ifdef OS_64
  return  __sync_bool_compare_and_swap((long long*)addr, (long long)old, (long long)neww); 
#else
  return  __sync_bool_compare_and_swap((int*)addr,(int)old,(int)neww); 
#endif
#endif
}



void     cmThIntIncr(  int*      addr, int      incr )
{
#ifdef OS_OSX
  OSAtomicAdd32Barrier(incr,addr);
#endif

#ifdef OS_LINUX
  // ... could also use __sync_add_and_fetch() ...
  __sync_fetch_and_add(addr,incr);
#endif
}

void     cmThUIntIncr( unsigned* addr, unsigned incr )
{
#ifdef OS_OSX
  OSAtomicAdd32Barrier((int)incr,(int*)addr);
#endif
#ifdef OS_LINUX
  __sync_fetch_and_add(addr,incr);
#endif
}

void     cmThFloatIncr(float*    addr, float    incr )
{

  float old,new;
  do
  {
    old = *addr;
    new = old + incr;
  }while( cmThFloatCAS(addr,old,new)==0 );
}


void     cmThIntDecr(  int*  addr, int      decr )
{
#ifdef OS_OSX
  OSAtomicAdd32Barrier(-decr,addr);
#endif

#ifdef OS_LINUX
  __sync_fetch_and_sub(addr,decr);
#endif
}

void     cmThUIntDecr( unsigned* addr, unsigned decr )
{
#ifdef OS_OSX
  OSAtomicAdd32Barrier(-((int)decr),(int*)addr);
#endif

#ifdef OS_LINUX
  __sync_fetch_and_sub(addr,decr);
#endif
}

void     cmThFloatDecr(float*    addr, float    decr )
{

  float old,new;
  do
  {
    old = *addr;
    new = old - decr;
  }while( cmThFloatCAS(addr,old,new)==0 );

}



//============================================================================================================================
//
//
typedef  pthread_t cmThreadId_t;

typedef struct
{
  cmThreadId_t id;    // id of this thread as returned by pthread_self()
  char*        buf;   // buf[bn]
  int          ii;    // input index
  int          oi;    // output index  (oi==ii == empty buffer)
} cmTsBuf_t;

// msg header - which is actually written AFTER the msg it is associated with
typedef struct cmTsHdr_str
{
  int                 mn;     // length of the msg
  int                 ai;     // buffer index
  struct cmTsHdr_str* link;   // pointer to next msg
} cmTsHdr_t;

typedef struct
{
  cmErr_t       err;
  int           bn;        // bytes per thread buffer  
  cmTsBuf_t*    a;         // a[an] buffer array
  unsigned      an;        // length of a[] - one buffer per thread
  cmTsQueueCb_t cbFunc;
  void*         cbArg;
  cmTsHdr_t*    ilp;        // prev msg hdr record
  cmTsHdr_t*    olp;        // prev msg hdr record (wait for olp->link to be set to go to next record)
} cmTsMp1c_t;

cmTsMp1cH_t cmTsMp1cNullHandle = cmSTATIC_NULL_HANDLE;

void _cmTsMp1cPrint( cmTsMp1c_t* p )
{
  unsigned i;
  for(i=0; i<p->an; ++i)
    printf("%2i ii:%3i oi:%3i\n",i,p->a[i].ii,p->a[i].oi);
}

cmTsMp1c_t* _cmTsMp1cHandleToPtr( cmTsMp1cH_t h )
{
  cmTsMp1c_t* p = (cmTsMp1c_t*)h.h;
  assert(p != NULL);
  return p;
}

unsigned _cmTsMp1cBufIndex( cmTsMp1c_t* p, cmThreadId_t id )
{
  unsigned i;
  for(i=0; i<p->an; ++i)
    if( p->a[i].id == id )
      return i;

  p->an       = i+1;
  p->a        = cmMemResizePZ(cmTsBuf_t,p->a,p->an);
  p->a[i].buf = cmMemAllocZ(char,p->bn);
  p->a[i].id  = id;
 
  return i;        
}

cmThRC_t   cmTsMp1cDestroy(    cmTsMp1cH_t* hp )
{
  if( hp == NULL || cmTsMp1cIsValid(*hp) == false )
    return kOkThRC;

  cmTsMp1c_t* p = _cmTsMp1cHandleToPtr(*hp);
  unsigned i;
  for(i=0; i<p->an; ++i)
    cmMemFree(p->a[i].buf);

  cmMemPtrFree(&p->a);
  cmMemFree(p);

  hp->h = NULL;

  return kOkThRC;
}

cmThRC_t   cmTsMp1cCreate(  cmTsMp1cH_t* hp, unsigned bufByteCnt, cmTsQueueCb_t cbFunc, void* cbArg, cmRpt_t* rpt )
{
  cmThRC_t rc;
  if((rc = cmTsMp1cDestroy(hp)) != kOkThRC )
    return rc;

  cmTsMp1c_t* p = cmMemAllocZ(cmTsMp1c_t,1);

  cmErrSetup(&p->err,rpt,"TsMp1c Queue");

  p->a          = NULL;
  p->an         = 0;
  p->bn         = bufByteCnt;
  p->cbFunc     = cbFunc;
  p->cbArg = cbArg;
  p->ilp        = NULL;
  p->olp        = NULL;

  hp->h = p;
  return rc;
}

void cmTsMp1cSetCbFunc( cmTsMp1cH_t h,  cmTsQueueCb_t cbFunc, void* cbArg )
{
  cmTsMp1c_t* p  = _cmTsMp1cHandleToPtr(h);
  p->cbFunc     = cbFunc;
  p->cbArg = cbArg;
}

cmTsQueueCb_t cmTsMp1cCbFunc( cmTsMp1cH_t h )
{
  cmTsMp1c_t* p  = _cmTsMp1cHandleToPtr(h);
  return p->cbFunc;
}

void* cmTsMp1cCbArg( cmTsMp1cH_t h )
{
  cmTsMp1c_t* p  = _cmTsMp1cHandleToPtr(h);
  return p->cbArg;
}

//#define CAS(addr,old,new) __sync_bool_compare_and_swap(addr,old,new)
//#define CAS(addr,old,neww) cmThPtrCAS(addr,old,neww)

cmThRC_t   cmTsMp1cEnqueueSegMsg( cmTsMp1cH_t h, const void* msgPtrArray[], unsigned msgByteCntArray[], unsigned arrayCnt )
{
  cmThRC_t    rc = kOkThRC;
  unsigned    mn = 0;
  cmTsMp1c_t* p  = _cmTsMp1cHandleToPtr(h);
  unsigned    ai = _cmTsMp1cBufIndex( p, pthread_self() );
  cmTsBuf_t*  b  = p->a + ai;
  int         i,bi,ei;
  cmTsHdr_t   hdr;

  // Use a stored oi for the duration of this function.
  // b->oi may be changed by the dequeue thread but storing it here
  // at least prevents it from changing during the course of the this function.
  // Note: b->oi is only used to check for buffer full.  Even if it changes
  // it would only mean that more bytes were available than calculated based
  // on the stored value.  A low estimate of the actual bytes available is 
  // never unsafe.
  volatile int    oi = b->oi; 

  // get the total count of bytes for this msg
  for(i=0; i<arrayCnt; ++i)
    mn += msgByteCntArray[i];

  // dn = count of msg bytes + count of header bytes
  int dn = mn + sizeof(hdr);

  // if oi is ahead of ii in the queue then we must write
  // in the area between ii and oi
  if( oi > b->ii )
  {
    ei = oi-1; // (never allow ii to equal oi (that's the empty condition))
    bi = b->ii;
  }
  else // otherwise oi is same or before ii in the queue and we have the option to wrap
  {
    // if the new msg will not fit at the end of the queue ....
    if( b->ii + dn > p->bn )
    {
      bi = 0;     // ... then wrap to the beginning
      ei = oi-1;  // (never allow ii to equal oi (that's the empty condition))
     }
    else 
    {
      ei = p->bn; // otherwise write at the current location
      bi = b->ii;
    }
  }
  
  if( bi + dn > ei )
    return cmErrMsg(&p->err,kBufFullThRC,"%i consecutive bytes is not available in the queue.",dn);

  char* dp = b->buf + bi;

  // write the msg
  for(i=0; i<arrayCnt; ++i)
  {
    memcpy(dp,msgPtrArray[i],msgByteCntArray[i]);
    dp += msgByteCntArray[i];
  }

  // setup the msg header
  hdr.ai   = ai;
  hdr.mn   = mn;
  hdr.link = NULL;

  // write the msg header (following the msg body in memory)
  cmTsHdr_t* hp = (cmTsHdr_t*)dp;
  memcpy(hp,&hdr,sizeof(hdr));

  // increment the buffers input index
  b->ii = bi + dn;

  // update the link list head to point to this msg hdr
  cmTsHdr_t* old_hp, *new_hp;
  do
  {
    old_hp = p->ilp;
    new_hp = hp;
  }while(!cmThPtrCAS(&p->ilp,old_hp,new_hp));

  // link the prev recd to this recd
  if( old_hp != NULL )
    old_hp->link = hp;

  // if this is the first record written by this queue then prime the output list 
  do
  {
    old_hp = p->olp;
    new_hp = hp;

    if( old_hp != NULL )
      break;
    
  }while(!cmThPtrCAS(&p->olp,old_hp,new_hp));

  //printf("%p %p %i\n",p->ilp,p->olp,p->olp->mn);

  return rc;
}

cmThRC_t   cmTsMp1cEnqueueMsg( cmTsMp1cH_t  h, const void* dataPtr, unsigned byteCnt )
{ return cmTsMp1cEnqueueSegMsg(h,&dataPtr,&byteCnt,1); }

unsigned   cmTsMp1cAllocByteCount( cmTsMp1cH_t h )
{
  cmTsMp1c_t* p  = _cmTsMp1cHandleToPtr(h);
  return p->bn;
}

unsigned   cmTsMp1cAvailByteCount( cmTsMp1cH_t h )
{
  cmTsMp1c_t*      p  = _cmTsMp1cHandleToPtr(h);
  unsigned         ai = _cmTsMp1cBufIndex(p,pthread_self());
  const cmTsBuf_t* b  = p->a + ai;

  if( b->oi > b->ii )
    return b->oi - b->ii - 1;

  return (p->bn - b->ii) + b->oi - 1;
}

unsigned _cmTsMp1cNextMsgByteCnt( cmTsMp1c_t* p )
{ 
  if( p->olp == NULL )
    return 0;

  // if the current msg has not yet been read
  if( p->olp->mn != 0 )
    return p->olp->mn;

  // if the current msg has been read but a new next msg has been linked
  if( p->olp->mn == 0 && p->olp->link != NULL )
  {
    // advance the buffer output msg past the prev msg header
    char* hp = (char*)(p->olp + 1);
    p->a[p->olp->ai].oi =  hp - p->a[p->olp->ai].buf;

    // advance msg pointer to point to the new msg header
    p->olp = p->olp->link;

    // return the size of the new msg
    return p->olp->mn;
  }
  return 0;
}

cmThRC_t   cmTsMp1cDequeueMsg( cmTsMp1cH_t  h,  void* dataPtr, unsigned byteCnt )
{
  cmTsMp1c_t* p  = _cmTsMp1cHandleToPtr(h);

  // if there are no messages waiting
  if( _cmTsMp1cNextMsgByteCnt(p) == 0 )
    return kBufEmptyThRC;

  char*    hp = (char*)p->olp;
  char*    dp = hp - p->olp->mn;  // the msg body is before the msg hdr

  if( dataPtr == NULL )
  {
    p->cbFunc(p->cbArg,p->olp->mn,dp);
  }
  else
  {
    if( p->olp->mn > byteCnt )
      return cmErrMsg(&p->err,kBufTooSmallThRC,"The return buffer constains too few bytes (%i) to contain %i bytes.",byteCnt,p->olp->mn);

    memcpy(dataPtr,dp,p->olp->mn);
  }

  // advance the buffers output index past the msg body
  p->a[p->olp->ai].oi = hp - p->a[p->olp->ai].buf;

  // mark the msg as read
  p->olp->mn = 0;

  return kOkThRC;
}


bool       cmTsMp1cMsgWaiting( cmTsMp1cH_t h )
{
  cmTsMp1c_t* p  = _cmTsMp1cHandleToPtr(h);
  return _cmTsMp1cNextMsgByteCnt(p) != 0;
}

unsigned   cmTsMp1cDequeueMsgByteCount( cmTsMp1cH_t h )
{
  cmTsMp1c_t* p  = _cmTsMp1cHandleToPtr(h);
  return _cmTsMp1cNextMsgByteCnt(p);
}
  
bool       cmTsMp1cIsValid( cmTsMp1cH_t h )
{ return h.h != NULL; }
  


//============================================================================================================================/
//
// cmTsQueueTest()
//

// param recd for use by _cmTsQueueCb0() and
// the msg record passed between the sender
// threads and the receiver thread
typedef struct
{
  unsigned     id;
  cmTsQueueH_t qH;
  int          val;
} _cmTsQCbParam_t;


// Generate a random number and put it in a TS queue
bool _cmTsQueueCb0(void* param)
{
  _cmTsQCbParam_t* p = (_cmTsQCbParam_t*)param;

  p->val = rand(); // generate a random number

  // send the msg
  if( cmTsQueueEnqueueMsg( p->qH, p, sizeof(_cmTsQCbParam_t)) == kOkThRC )
    printf("in:%i %i\n",p->id,p->val);
  else
    printf("in error %i\n",p->id);
  

  cmSleepUs(100*1000);

  return true;
}

// Monitor a TS queue for incoming messages from _cmTsQueueCb1()
bool _cmTsQueueCb1(void* param)
{
  // the thread param is a ptr to the TS queue to monitor.
  cmTsQueueH_t*   qp = (cmTsQueueH_t*)param;
  cmThRC_t        rc;
  _cmTsQCbParam_t msg;

  // dequeue any waiting messages
  if((rc = cmTsQueueDequeueMsg( *qp, &msg, sizeof(msg))) == kOkThRC )
    printf("out:%i %i\n",msg.id,msg.val);
  else
  {
    if( rc != kBufEmptyThRC )
      printf("out error:%i\n", rc);
  }

  return true;
}

// Test the TS queue by starting sender threads (threads 0 & 1)
// and a receiver thread (thread 2) and sending messages
// from the sender to the receiver.
void cmTsQueueTest( cmRpt_t* rpt )
{
  cmThreadH_t th0=cmThreadNullHandle,th1=cmThreadNullHandle,th2=cmThreadNullHandle;
  cmTsQueueH_t q=cmTsQueueNullHandle;
  _cmTsQCbParam_t param0, param1;

  // create a TS Queue
  if( cmTsQueueCreate(&q,100,NULL,NULL,rpt) != kOkThRC )
    goto errLabel;

  // create thread 0
  param0.id = 0;
  param0.qH = q;
  if( cmThreadCreate(&th0,_cmTsQueueCb0,&param0,rpt) != kOkThRC )
    goto errLabel;

  // create thread 1
  param1.id = 1;
  param1.qH = q;
  if( cmThreadCreate(&th1,_cmTsQueueCb0,&param1,rpt) != kOkThRC )
    goto errLabel;

  // create thread 2
  if( cmThreadCreate(&th2,_cmTsQueueCb1,&q,rpt) != kOkThRC )
    goto errLabel;

  // start thread 0
  if( cmThreadPause(th0,0) != kOkThRC )
    goto errLabel;

  // start thread 1
  if( cmThreadPause(th1,0) != kOkThRC )
    goto errLabel;

  // start thread 2
  if( cmThreadPause(th2,0) != kOkThRC )
    goto errLabel;

  printf("any key to quit.");
  getchar();

 
 errLabel:

  if( cmThreadIsValid(th0) )
    if( cmThreadDestroy(&th0) != kOkThRC )
      printf("Error destroying thread 0\n");

  if( cmThreadIsValid(th1) )
    if( cmThreadDestroy(&th1) != kOkThRC )
      printf("Error destroying thread 1\n");

  if( cmThreadIsValid(th2) )
    if( cmThreadDestroy(&th2) != kOkThRC )
      printf("Error destroying thread 1\n");

  if( cmTsQueueIsValid(q) )
    if( cmTsQueueDestroy(&q) != kOkThRC )
      printf("Error destroying queue\n");
  
}


//============================================================================================================================/
//
// cmTs1p1cTest()
//

// param recd for use by _cmTsQueueCb0() and
// the msg record passed between the sender
// threads and the receiver thread
typedef struct
{
  unsigned     id;
  cmTs1p1cH_t  qH;
  int          val;
} _cmTs1p1cCbParam_t;

cmTs1p1cH_t cmTs1p1cNullHandle = cmSTATIC_NULL_HANDLE;

// Generate a random number and put it in a TS queue
bool _cmTs1p1cCb0(void* param)
{
  _cmTs1p1cCbParam_t* p = (_cmTs1p1cCbParam_t*)param;

  p->val = rand(); // generate a random number

  // send the msg
  if( cmTs1p1cEnqueueMsg( p->qH, p, sizeof(_cmTs1p1cCbParam_t)) == kOkThRC )
    printf("in:%i %i\n",p->id,p->val);
  else
    printf("in error %i\n",p->id);
  
  ++p->id;

  cmSleepUs(100*1000);

  return true;
}

// Monitor a TS queue for incoming messages from _cmTs1p1cCb1()
bool _cmTs1p1cCb1(void* param)
{
  // the thread param is a ptr to the TS queue to monitor.
  cmTs1p1cH_t*   qp = (cmTs1p1cH_t*)param;
  cmThRC_t        rc;
  _cmTs1p1cCbParam_t msg;

  // dequeue any waiting messages
  if((rc = cmTs1p1cDequeueMsg( *qp, &msg, sizeof(msg))) == kOkThRC )
    printf("out:%i %i\n",msg.id,msg.val);
  else
  {
    if( rc != kBufEmptyThRC )
      printf("out error:%i\n", rc);
  }

  return true;
}

// Test the TS queue by starting sender threads (threads 0 & 1)
// and a receiver thread (thread 2) and sending messages
// from the sender to the receiver.
void cmTs1p1cTest( cmRpt_t* rpt )
{
  cmThreadH_t th0=cmThreadNullHandle,th1=cmThreadNullHandle,th2=cmThreadNullHandle;
  cmTs1p1cH_t q=cmTs1p1cNullHandle;
  _cmTs1p1cCbParam_t param1;

  // create a TS Queue
  if( cmTs1p1cCreate(&q,28*2,NULL,NULL,rpt) != kOkThRC )
    goto errLabel;

  // create thread 1
  param1.id = 0;
  param1.qH = q;
  if( cmThreadCreate(&th1,_cmTs1p1cCb0,&param1,rpt) != kOkThRC )
    goto errLabel;

  // create thread 2
  if( cmThreadCreate(&th2,_cmTs1p1cCb1,&q,rpt) != kOkThRC )
    goto errLabel;

  // start thread 1
  if( cmThreadPause(th1,0) != kOkThRC )
    goto errLabel;

  // start thread 2
  if( cmThreadPause(th2,0) != kOkThRC )
    goto errLabel;

  printf("any key to quit.");
  getchar();

 
 errLabel:

  if( cmThreadIsValid(th0) )
    if( cmThreadDestroy(&th0) != kOkThRC )
      printf("Error destroying thread 0\n");

  if( cmThreadIsValid(th1) )
    if( cmThreadDestroy(&th1) != kOkThRC )
      printf("Error destroying thread 1\n");

  if( cmThreadIsValid(th2) )
    if( cmThreadDestroy(&th2) != kOkThRC )
      printf("Error destroying thread 1\n");

  if( cmTs1p1cIsValid(q) )
    if( cmTs1p1cDestroy(&q) != kOkThRC )
      printf("Error destroying queue\n");
  
}


//============================================================================================================================/
//
// cmTsMp1cTest()
//
// param recd for use by _cmTsQueueCb0() and
// the msg record passed between the sender
// threads and the receiver thread
typedef struct
{
  unsigned     id;
  cmTsMp1cH_t  qH;
  int          val;
} _cmTsMp1cCbParam_t;


unsigned _cmTsMp1cVal = 0;

// Incr the global value _cmTsMp1cVal and put it in a TS queue
bool _cmTsMp1cCb0(void* param)
{
  _cmTsMp1cCbParam_t* p = (_cmTsMp1cCbParam_t*)param;

  //p->val = __sync_fetch_and_add(&_cmTsMp1cVal,1);

  cmThUIntIncr(&_cmTsMp1cVal,1);
  p->val = _cmTsMp1cVal;

  // send the msg
  if( cmTsMp1cEnqueueMsg( p->qH, p, sizeof(_cmTsMp1cCbParam_t)) == kOkThRC )
    printf("in:%i %i\n",p->id,p->val);
  else
    printf("in error %i\n",p->id);

  cmSleepUs(100*1000);

  return true;
}

// Monitor a TS queue for incoming messages from _cmTsMp1cCb1()
bool _cmTsMp1cCb1(void* param)
{
  // the thread param is a ptr to the TS queue to monitor.
  cmTsMp1cH_t*   qp = (cmTsMp1cH_t*)param;
  cmThRC_t        rc;
  _cmTsMp1cCbParam_t msg;

  // dequeue any waiting messages
  if((rc = cmTsMp1cDequeueMsg( *qp, &msg, sizeof(msg))) == kOkThRC )
    printf("out - cons id:%i val:%i\n",msg.id,msg.val);
  else
  {
    if( rc != kBufEmptyThRC )
      printf("out error:%i\n", rc);
  }

  return true;
}

// Test the TS queue by starting sender threads (threads 0 & 1)
// and a receiver thread (thread 2) and sending messages
// from the sender to the receiver.
void cmTsMp1cTest( cmRpt_t* rpt )
{
  cmThreadH_t th0=cmThreadNullHandle,th1=cmThreadNullHandle,th2=cmThreadNullHandle;
  cmTsMp1cH_t q=cmTsMp1cNullHandle;
  _cmTsMp1cCbParam_t param0, param1;

  // create a TS Queue
  if( cmTsMp1cCreate(&q,1000,NULL,NULL,rpt) != kOkThRC )
    goto errLabel;

  // create thread 0 - producer 0
  param0.id = 0;
  param0.qH = q;
  if( cmThreadCreate(&th0,_cmTsMp1cCb0,&param0,rpt) != kOkThRC )
    goto errLabel;

  // create thread 1 - producer 1
  param1.id = 1;
  param1.qH = q;
  if( cmThreadCreate(&th1,_cmTsMp1cCb0,&param1,rpt) != kOkThRC )
    goto errLabel;

  // create thread 2 - consumer 0
  if( cmThreadCreate(&th2,_cmTsMp1cCb1,&q,rpt) != kOkThRC )
    goto errLabel;

  // start thread 0
  if( cmThreadPause(th0,0) != kOkThRC )
    goto errLabel;

  // start thread 1
  if( cmThreadPause(th1,0) != kOkThRC )
    goto errLabel;

  // start thread 2
  if( cmThreadPause(th2,0) != kOkThRC )
    goto errLabel;

  printf("any key to quit.");
  getchar();

 
 errLabel:

  if( cmThreadIsValid(th0) )
    if( cmThreadDestroy(&th0) != kOkThRC )
      printf("Error destroying thread 0\n");

  if( cmThreadIsValid(th1) )
    if( cmThreadDestroy(&th1) != kOkThRC )
      printf("Error destroying thread 1\n");

  if( cmThreadIsValid(th2) )
    if( cmThreadDestroy(&th2) != kOkThRC )
      printf("Error destroying thread 1\n");

  if( cmTsMp1cIsValid(q) )
    if( cmTsMp1cDestroy(&q) != kOkThRC )
      printf("Error destroying queue\n");
  
}

void cmSleepUs( unsigned microseconds )
{ usleep(microseconds); }

void cmSleepMs( unsigned milliseconds )
{ cmSleepUs(milliseconds*1000); }
