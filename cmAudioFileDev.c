#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmAudioFile.h"
#include "cmThread.h"
#include "cmAudioPort.h"
#include "cmAudioFileDev.h"


#ifdef OS_OSX
#include "osx/clock_gettime_stub.h"
#endif

#ifdef OS_LINUX
#include <time.h> // clock_gettime()
#endif


cmAfdH_t cmAfdNullHandle = cmSTATIC_NULL_HANDLE;

#define cmAfd_Billion (1000000000)
#define cmAfd_Million    (1000000)

typedef struct
{
  cmErr_t           err;             // error object 
  cmApCallbackPtr_t callbackPtr;     // client callback function
  void*             cbDataPtr;       // argument to be passed with the client callback 

  unsigned          devIdx;
  cmChar_t*         label;
  cmChar_t*         oFn; 
  unsigned          oBits;
  unsigned          oChCnt;

  cmAudioFileH_t    iAfH;            // audio input file handle
  cmAudioFileH_t    oAfH;            // audio output file handle
  cmThreadH_t       tH;              // thread handle

  double            srate;           // file device sample rate
  unsigned          framesPerCycle;  // count of samples sent/recv'd from the client on each callback

  cmApAudioPacket_t iPkt;            // audio packet used sent to the client via callbackPtr.
  cmApAudioPacket_t oPkt;            //
  cmApSample_t**    iChArray;        // audio buffer channel arrays used with cmAudioFile
  cmApSample_t**    oChArray;        //

  bool              runFl;            // set to true as long as the thread should continue looping
  bool              rewindFl;         // set to true when the input file should rewind
  unsigned          readErrCnt;       // count of read errors from the input file
  bool              eofFl;            // set to true when the input file reaches the EOF
  unsigned          writeErrCnt;      // count of write errors from the output file

  long              nanosPerCycle;    // nano-seconds per cycle
  struct timespec   baseTime;      
  struct timespec   nextTime;         // next execution time 

  unsigned          cycleCnt;         // count of cycles completed

} cmAfd_t;

cmAfd_t* _cmAfdHandleToPtr( cmAfdH_t h )
{
  cmAfd_t* p = (cmAfd_t*)h.h;
  assert(p != NULL );
  return p;
}

// 
void _cmAudioFileDevExec( cmAfd_t* p )
{
  unsigned iPktCnt = 0;
  unsigned oPktCnt = p->oPkt.chCnt!=0;

  // if the input device is enabled
  if( p->iPkt.chCnt )
  {
    unsigned actualFrmCnt = p->framesPerCycle;

    // if the input file has reached EOF - zero the input buffer
    if( p->eofFl )
      memset(p->iPkt.audioBytesPtr,0,p->framesPerCycle*sizeof(cmApSample_t));
    else
    {
      // otherwise fill the input buffer from the input file
      if( cmAudioFileReadSample(p->iAfH, p->framesPerCycle, p->iPkt.begChIdx, p->iPkt.chCnt, p->iChArray, &actualFrmCnt) != kOkAfRC )
        ++p->readErrCnt;

      // if the input file reachged EOF the set p->eofFl
      if( (actualFrmCnt < p->framesPerCycle) && cmAudioFileIsEOF(p->iAfH) )
        p->eofFl = true;
    }

    iPktCnt = actualFrmCnt>0;
  }

  // callback to the client to provde incoming samples and receive outgoing samples
  p->callbackPtr(iPktCnt ? &p->iPkt : NULL, iPktCnt, oPktCnt ? &p->oPkt : NULL, oPktCnt );

  // if the output device is enabled
  if( p->oPkt.chCnt )
  {
    // write the output samples
    if( cmAudioFileWriteSample( p->oAfH, p->framesPerCycle, p->oPkt.chCnt, p->oChArray ) != kOkAfRC )
      ++p->writeErrCnt;
  }

  ++p->cycleCnt;

}

// incrment p->nextTime to the next execution time
void _cmAfdIncrNextTime( cmAfd_t* p )
{
  long nsec            = p->nextTime.tv_nsec + p->nanosPerCycle;

  if( nsec < cmAfd_Billion )
    p->nextTime.tv_nsec = nsec;
  else
  {
    p->nextTime.tv_sec  += 1;
    p->nextTime.tv_nsec  = nsec - cmAfd_Billion;
  }
}

// calc the time between t1 and t0 - t1 is assummed to come after t0 in order to produce a positive result
long  _cmAfdDiffMicros( const struct timespec* t0, const struct timespec* t1 )
{
  long u0 = t0->tv_sec * cmAfd_Million;
  long u1 = t1->tv_sec * cmAfd_Million;
  
  u0 += t0->tv_nsec / 1000;
  u1 += t1->tv_nsec / 1000;

  return u1 - u0;
}

//  thread callback function 
bool _cmAudioDevThreadFunc(void* param)
{
  cmAfd_t* p       = (cmAfd_t*)param;
  struct timespec t0;

  // if this is the first time this callback has been called after a call to cmAudioFileDevStart().
  if( p->cycleCnt == 0 )
  {
    // get the baseTime - all other times will be relative to this time
    clock_gettime(CLOCK_REALTIME,&p->baseTime);
    p->nextTime = p->baseTime;
    p->nextTime.tv_sec = 0;
    _cmAfdIncrNextTime(p);
  }

  // if the thread has not been requested to stop
  if( p->runFl )
  {
    // get the current time as an offset from baseTime.
    clock_gettime(CLOCK_REALTIME,&t0);
    t0.tv_sec -= p->baseTime.tv_sec;

    // get length of time to next exec point
    long dusec = _cmAfdDiffMicros(&t0, &p->nextTime);

    // if the execution time has not yet arrived
    if( dusec > 0 )
    {
      cmSleepUs(dusec);
    }

    // if the thread is still running
    if( p->runFl )
    {
      // read/callback/write
      _cmAudioFileDevExec(p);
  
      // calc the next exec time
      _cmAfdIncrNextTime(p);
    }
  }

  return p->runFl;
}

cmAfdRC_t cmAudioFileDevInitialize( 
  cmAfdH_t*       hp, 
  const cmChar_t* label,
  unsigned        devIdx,
  const cmChar_t* iFn,
  const cmChar_t* oFn,
  unsigned        oBits,
  unsigned        oChCnt,
  cmRpt_t*        rpt )
{
  cmAfdRC_t rc;
  cmRC_t    afRC;

  if((rc = cmAudioFileDevFinalize(hp)) != kOkAfdRC )
    return rc;
  
  // allocate the object
  cmAfd_t* p = cmMemAllocZ(cmAfd_t,1);

  hp->h = p;

  cmErrSetup(&p->err,rpt,"AudioFileDevice");

  // create the input audio file handle
  if( iFn != NULL )
  {
    cmAudioFileInfo_t afInfo;

    // open the input file
    if(cmAudioFileIsValid( p->iAfH = cmAudioFileNewOpen(iFn,&afInfo,&afRC,rpt)) == false )
    {
      rc = cmErrMsg(&p->err,kAudioFileFailAfdRC,"The audio input file '%s' could not be opened.", iFn);
      goto errLabel;
    }

    p->iPkt.devIdx         = devIdx;
    p->iPkt.begChIdx       = 0;
    p->iPkt.chCnt          = afInfo.chCnt;   // setting iPkt.chCnt to a non-zero value marks the input file as active
    p->iPkt.audioFramesCnt = 0;
    p->iPkt.bitsPerSample  = afInfo.bits;
    p->iPkt.flags          = kFloatApFl;
    p->iPkt.audioBytesPtr  = NULL;
    p->iChArray            = cmMemResizeZ( cmApSample_t*, p->iChArray, afInfo.chCnt );
    p->readErrCnt          = 0;
    p->eofFl               = false;
  }

  // create the output audio file handle
  if(cmAudioFileIsValid( p->oAfH = cmAudioFileNewOpen(NULL,NULL,NULL,rpt)) == false )
  {
    rc = cmErrMsg(&p->err,kAudioFileFailAfdRC,"The audio output file object allocation failed.");
    goto errLabel;
  }

  // create the driver thread
  if( cmThreadCreate(&p->tH, _cmAudioDevThreadFunc, p, rpt ) != kOkThRC )
  {
    rc = cmErrMsg(&p->err,kThreadFailAfdRC,"The internal thread could not be created.");
    goto errLabel;
  }

  p->runFl  = true;
  p->devIdx = devIdx;
  p->label  = cmMemAllocStr(label);
  p->oFn    = cmMemAllocStr(oFn);
  p->oBits  = oBits;
  p->oChCnt = oChCnt;

 errLabel:
  if( rc != kOkAfdRC )
    cmAudioFileDevFinalize(hp);

  return rc;
}

cmAfdRC_t cmAudioFileDevFinalize( cmAfdH_t* hp )
{
  if( hp == NULL || cmAudioFileDevIsValid(*hp) == false )
    return kOkAfdRC;

  cmAfd_t* p = _cmAfdHandleToPtr(*hp);

  p->runFl = false;

  if( cmThreadIsValid(p->tH) )
    cmThreadDestroy(&p->tH);

  cmAudioFileDelete(&p->iAfH);
  cmAudioFileDelete(&p->oAfH);
  cmMemPtrFree(&p->label);
  cmMemPtrFree(&p->oFn);
  cmMemPtrFree(&p->iPkt.audioBytesPtr);
  cmMemPtrFree(&p->oPkt.audioBytesPtr);
  cmMemPtrFree(&p->iChArray);
  cmMemPtrFree(&p->oChArray);
  cmMemPtrFree(&p);
  hp->h = NULL;

  return kOkAfdRC;
}

bool      cmAudioFileDevIsValid( cmAfdH_t h )
{ return h.h != NULL;  }


cmAfdRC_t cmAudioFileDevSetup( 
  cmAfdH_t                        h, 
  unsigned                        baseDevIdx,
  double                          srate, 
  unsigned                        framesPerCycle, 
  cmApCallbackPtr_t               callbackPtr, 
  void*                           cbDataPtr )
{
  cmAfdRC_t rc        = kOkAfdRC;
  bool      restartFl = false;
  unsigned  i;

  if( cmAudioFileDevIsStarted(h) )
  {
    if((rc = cmAudioFileDevStop(h)) != kOkAfdRC )
      return rc;

    restartFl = true;
  }

  cmAfd_t* p = _cmAfdHandleToPtr(h);

  /*
  // close the existing input file
  if(cmAudioFileClose(&p->iAfH) != kOkAfRC )
    rc = cmErrMsg(&p->err,kAudioFileFailAfdRC,"Audio file close failed on input audio file.");

  p->iPkt.chCnt = 0; // mark the input file as inactive
  */

  if( cmAudioFileIsValid( p->iAfH ) )
    if( cmAudioFileSeek( p->iAfH, 0 ) != kOkAfRC )
      rc = cmErrMsg(&p->err,kAudioFileFailAfdRC,"Audio file device rewind failed.");


  // close the existing output file
  if(cmAudioFileClose(&p->oAfH) != kOkAfRC )
    rc = cmErrMsg(&p->err,kAudioFileFailAfdRC,"Audio file close failed on output audio file.");

  p->oPkt.chCnt = 0; // mark the output file as inactive

  // if an output audio file was given ...
  if( p->oFn != NULL )
  {
    // ... then open it
    if( cmAudioFileCreate( p->oAfH, p->oFn, srate, p->oBits, p->oChCnt ) != kOkAfRC )
    {
      rc = cmErrMsg(&p->err,kAudioFileFailAfdRC, "The audio output file '%s' could not be created.",p->oFn);
      goto errLabel;
    }

    cmApSample_t* bp = (cmApSample_t*)p->oPkt.audioBytesPtr;

    p->oPkt.devIdx         = p->devIdx + baseDevIdx;
    p->oPkt.begChIdx       = 0;
    p->oPkt.chCnt          = p->oChCnt;
    p->oPkt.audioFramesCnt = framesPerCycle;
    p->oPkt.bitsPerSample  = p->oBits;
    p->oPkt.flags          = kFloatApFl;
    p->oPkt.audioBytesPtr  = bp = cmMemResizeZ( cmApSample_t, bp, framesPerCycle*p->oChCnt );
    p->oPkt.userCbPtr      = cbDataPtr;
    p->oChArray            = cmMemResizeZ( cmApSample_t*, p->oChArray, p->oChCnt );
    
    for(i=0; i<p->oChCnt; ++i)
      p->oChArray[i] = bp + (i*framesPerCycle);

  }

  if( cmAudioFileIsValid( p->iAfH) )
  {
    cmApSample_t* bp = (cmApSample_t*)p->iPkt.audioBytesPtr;

    p->iPkt.devIdx         = p->devIdx + baseDevIdx;
    p->iPkt.audioFramesCnt = framesPerCycle;
    p->iPkt.audioBytesPtr  = bp = cmMemResizeZ( cmApSample_t, bp, framesPerCycle*p->iPkt.chCnt ); ;
    p->iPkt.userCbPtr      = cbDataPtr;
    for(i=0; i<p->iPkt.chCnt; ++i)
      p->iChArray[i] = bp + (i*framesPerCycle);
  }

  p->callbackPtr    = callbackPtr;
  p->cbDataPtr      = cbDataPtr;
  p->framesPerCycle = framesPerCycle;
  p->srate          = srate;
  p->cycleCnt       = 0;
  p->nanosPerCycle  = floor((double)framesPerCycle / srate * cmAfd_Billion );

  if( restartFl )
  {
    if((rc = cmAudioFileDevStart(h)) != kOkAfdRC )
    {
      rc = cmErrMsg(&p->err,kRestartFailAfdRC,"The audio file device could not be restarted.");
    }
  }

 errLabel:
  return rc;

}

const char* cmAudioFileDevLabel( cmAfdH_t h )
{
  cmAfd_t*  p  = _cmAfdHandleToPtr(h); 
  return p->label;
}

unsigned    cmAudioFileDevChannelCount( cmAfdH_t h, bool inputFl )
{
  cmAfd_t*  p  = _cmAfdHandleToPtr(h); 
  return inputFl ? p->iPkt.chCnt : p->oPkt.chCnt;
}

double      cmAudioFileDevSampleRate(   cmAfdH_t h )
{
  cmAfd_t*  p  = _cmAfdHandleToPtr(h);
  return p->srate;
}

unsigned    cmAudioFileDevFramesPerCycle( cmAfdH_t h, bool inputFl )
{
  cmAfd_t*  p  = _cmAfdHandleToPtr(h);
  return inputFl ? p->iPkt.audioFramesCnt : p->oPkt.audioFramesCnt;
}


cmAfdRC_t cmAudioFileDevRewind( cmAfdH_t h )
{
  cmAfd_t*  p  = _cmAfdHandleToPtr(h);
  p->rewindFl = true;
  return kOkAfdRC;
}

cmAfdRC_t cmAudioFileDevStart( cmAfdH_t h )
{
  cmAfdRC_t rc = kOkAfdRC;
  cmAfd_t*  p  = _cmAfdHandleToPtr(h);

  p->cycleCnt = 0;

  if( cmThreadPause( p->tH, 0 ) != kOkThRC )
  {
    rc = cmErrMsg(&p->err,kThreadFailAfdRC,"Thread start failed.");
    goto errLabel;
  }

  fputs("Start\n",stderr);

 errLabel:
  return rc;
}

cmAfdRC_t cmAudioFileDevStop( cmAfdH_t h )
{
  cmAfdRC_t rc = kOkAfdRC;
  cmAfd_t*  p  = _cmAfdHandleToPtr(h);

  if( cmThreadPause( p->tH, kPauseThFl | kWaitThFl ) != kOkThRC )
  {
    rc = cmErrMsg(&p->err,kThreadFailAfdRC,"Thread stop failed.");
    goto errLabel;
  }

  fputs("Stop\n",stderr);

 errLabel:
  return rc;
}

bool      cmAudioFileDevIsStarted( cmAfdH_t h ) 
{  
  cmAfd_t* p = _cmAfdHandleToPtr(h);
  return cmThreadState(p->tH) == kRunningThId; 
}

void cmAudioFileDevReport( cmAfdH_t h, cmRpt_t* rpt )
{
  cmAfd_t* p = _cmAfdHandleToPtr(h);
  cmRptPrintf(rpt,"label:%s thr state:%i srate:%f\n",p->label,cmThreadState(p->tH),p->srate);
  cmRptPrintf(rpt, "in  chs:%i %s\n",p->iPkt.chCnt,cmAudioFileName(p->iAfH));
  cmRptPrintf(rpt, "out chs:%i %s\n",p->oPkt.chCnt,p->oFn);
}

// device callback function used with cmAudioFileDevTest() note that this assumes
// that the packet buffer contain non-interleaved data.
void _cmAfdCallback( 
  cmApAudioPacket_t* inPktArray, 
  unsigned           inPktCnt, 
  cmApAudioPacket_t* outPktArray, 
  unsigned           outPktCnt )
{
  cmApAudioPacket_t* ip  = inPktArray;
  cmApAudioPacket_t* op  = outPktArray;
  unsigned           opi = 0;
  unsigned           ipi = 0;
  unsigned           oci = 0;
  unsigned           ici = 0;

  while(1)
  {
    if( ici == ip->chCnt)
    {
      ici = 0;
      if( ++ipi >= inPktCnt )
        break;

      ip = inPktArray + ipi;
    }


    if( oci == op->chCnt )
    {
      oci = 0;
      if( ++opi >= outPktCnt )
        break;

      ip = outPktArray + opi;
    }

    assert( ip->audioFramesCnt == op->audioFramesCnt );
    assert( cmIsFlag(ip->flags,kInterleavedApFl)==false && cmIsFlag(ip->flags,kInterleavedApFl)==false );

    cmApSample_t* ibp = ((cmApSample_t*)ip->audioBytesPtr) + (ip->audioFramesCnt*ici);
    cmApSample_t* obp = ((cmApSample_t*)op->audioBytesPtr) + (op->audioFramesCnt*oci);

    memcpy(obp,ibp,ip->audioFramesCnt*sizeof(cmApSample_t));

    ++ici;
    ++oci;
  }
}

void      cmAudioFileDevTest( cmRpt_t* rpt )
{
  cmAfdH_t        afdH           = cmAfdNullHandle;
  double          srate          = 44100;
  unsigned        framesPerCycle = 512;
  void*           cbDataPtr      = NULL;
  unsigned        devIdx         = 0;
  const cmChar_t* iFn            = "/home/kevin/media/audio/McGill-1/1 Audio Track.aiff";
  const cmChar_t* oFn            = "/home/kevin/temp/afd0.aif";
  unsigned        oBits          = 16;
  unsigned        oChCnt         = 2;

  if( cmAudioFileDevInitialize(&afdH,"file",devIdx,iFn,oFn,oBits,oChCnt,rpt) != kOkAfdRC )
    goto errLabel;

  if( cmAudioFileDevSetup(afdH,0,srate,framesPerCycle,_cmAfdCallback,cbDataPtr) != kOkAfdRC )
    goto errLabel;

  char c;
  fputs("q=quit 1=start 0=stop\n",stderr);
  fflush(stderr);

  while((c=getchar()) != 'q')
  {
    switch(c)
    {
      case '1': cmAudioFileDevStart(afdH); break;
      case '0': cmAudioFileDevStop(afdH);  break;
    }
    
    c = 0;
    fflush(stdin);
  }

 errLabel:
  cmAudioFileDevFinalize(&afdH);

  
}

