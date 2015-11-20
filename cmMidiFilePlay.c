#include <sys/time.h>  // gettimeofday()
#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmFile.h"
#include "cmTime.h"
#include "cmMidi.h"
#include "cmMidiPort.h"
#include "cmMidiFile.h"
#include "cmMidiFilePlay.h"
#include "cmThread.h" // cmSleepUs()
#include "cmTime.h"


typedef struct
{
  cmErr_t                  err;
  cmCtx_t                  ctx;
  cmMfpCallback_t          cbFunc;          
  void*                    userCbPtr;
  void*                    printDataPtr;
  unsigned                 memBlockByteCnt;
  cmMidiFileH_t            mfH;           // midi file handle
  bool                     closeFileFl;   // true mfH should be closed when this midi file player is closed
  unsigned                 ticksPerQN;    // global for file
  unsigned                 microsPerTick; // set via tempo
  unsigned                 etime;         // usecs elapsed since transmitting prev msg
  unsigned                 mtime;         // usecs to wait before transmitting next msg 
  unsigned                 msgN;          // count of pointers in msgV[]
  unsigned                 msgIdx;        // index into msgV[] of next msg to transmit
  const cmMidiTrackMsg_t** msgV;          // array of msg pointers

} cmMfp_t;

cmMfpH_t cmMfpNullHandle = cmSTATIC_NULL_HANDLE;

#define _cmMfpError( mfp, rc ) _cmMfpOnError(mfp, rc, __LINE__,__FILE__,__FUNCTION__ )

// note: mfp may be NULL
cmMfpRC_t _cmMfpOnError( cmMfp_t* mfp, cmMfpRC_t rc, int line, const char* fn, const char* func )
{
  return cmErrMsg(&mfp->err,rc,"rc:%i %i %s %s\n",rc,line,func,fn);
}

cmMfp_t* _cmMfpHandleToPtr( cmMfpH_t h )
{
  cmMfp_t* p = (cmMfp_t*)h.h;
  assert(p != NULL);
  return p;
}

void _cmMfpUpdateMicrosPerTick( cmMfp_t* mfp, unsigned microsPerQN )
{ 
  mfp->microsPerTick =  microsPerQN / mfp->ticksPerQN; 
  printf("microsPerTick: %i bpm:%i ticksPerQN:%i\n", mfp->microsPerTick,microsPerQN,mfp->ticksPerQN);
}

cmMfpRC_t cmMfpCreate( cmMfpH_t* hp, cmMfpCallback_t cbFunc, void* userCbPtr, cmCtx_t* ctx )
{
  cmMfp_t* p = cmMemAllocZ( cmMfp_t, 1 );

  cmErrSetup(&p->err,&ctx->rpt,"MIDI File Player");
  p->ctx             = *ctx;
  p->cbFunc          = cbFunc;
  p->userCbPtr       = userCbPtr;
  p->mfH.h           = NULL;
  p->closeFileFl     = false;
  p->ticksPerQN      = 0;
  p->microsPerTick   = 0;
  p->etime           = 0;
  p->msgN            = 0;
  p->msgV            = NULL;
  p->msgIdx          = 0;
  hp->h              = p;
  return kOkMfpRC;
}

cmMfpRC_t cmMfpDestroy( cmMfpH_t* hp )
{
  if( hp == NULL )
    return kOkMfpRC;

  if( cmMfpIsValid(*hp) )
  {
    cmMfp_t* p = _cmMfpHandleToPtr(*hp);
    
    if( cmMidiFileIsNull(p->mfH)==false && p->closeFileFl==true )
      cmMidiFileClose(&p->mfH);

    cmMemFree(p);
    hp->h = NULL;
  }

  return kOkMfpRC;
}

bool cmMfpIsValid(    cmMfpH_t h )
{ return h.h != NULL; }

cmMfpRC_t cmMfpLoadFile(   cmMfpH_t h, const char* fn )
{
  cmMfpRC_t     rc   = kOkMfpRC;
  cmMfp_t*      p    = _cmMfpHandleToPtr(h);
  cmMidiFileH_t mfH  = cmMidiFileNullHandle;

  if((rc = cmMidiFileOpen( fn, &mfH, &p->ctx )) != kOkMfRC )
    return _cmMfpError(p,kFileOpenFailMfpRC);

  if((rc= cmMfpLoadHandle( h, mfH )) == kOkMfpRC )
    p->closeFileFl                  = true;

  return rc;
}

cmMfpRC_t cmMfpLoadHandle( cmMfpH_t h, cmMidiFileH_t mfH )
{
  cmMfp_t* p = _cmMfpHandleToPtr(h);

  // if a file has already been assigned to this player
  if( (cmMidiFileIsNull(p->mfH) == false) && p->closeFileFl)
  {
    // close the existing file
    cmMidiFileClose(&p->mfH);    
  }

  // get the count of msg's in the new midi file
  if((p->msgN = cmMidiFileMsgCount(mfH)) == cmInvalidCnt )
    return _cmMfpError(p,kInvalidFileMfpRC);

  // get a pointer to the first mesage
  if((p->msgV = cmMidiFileMsgArray(mfH)) == NULL )
    return _cmMfpError(p,kInvalidFileMfpRC);

  // get the count of ticks per qn
  if((p->ticksPerQN = cmMidiFileTicksPerQN( mfH )) == 0 )
    return _cmMfpError(p,kSmpteTickNotImplMfpRC);
  
  // set the initial tempo to 120
  _cmMfpUpdateMicrosPerTick(p,60000000/120);

  p->msgIdx     = 0;
  p->mfH        = mfH;
  p->etime      = 0;
  p->mtime      = 0;
  p->closeFileFl= false;

  return kOkMfpRC;
}

cmMfpRC_t cmMfpSeek( cmMfpH_t h, unsigned offsUsecs )
{
  cmMfp_t*   p = _cmMfpHandleToPtr(h);
  unsigned msgOffsUsecs = 0;
  unsigned msgIdx;
  unsigned newMicrosPerTick;

  // if the requested offset is past the end of the file then return EOF
  if((msgIdx = cmMidiFileSeekUsecs( p->mfH, offsUsecs, &msgOffsUsecs, &newMicrosPerTick )) == cmInvalidIdx )
  {
    p->msgIdx = p->msgN;
    return _cmMfpError(p,kEndOfFileMfpRC);
  }

  if( msgIdx < p->msgIdx )
    p->msgIdx = 0;

  p->mtime =  msgOffsUsecs;
  p->etime =  0;
  p->microsPerTick = newMicrosPerTick;
  p->msgIdx = msgIdx;

  assert(p->mtime >= 0);

  return kOkMfpRC;
}

//    p  0     1      n  2     
//    v  v     v      v  v     
// xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
// 012345678901234567890123456780
// 0         1         2
// 
// p =  3 = prev msg sent
// n = 19 = next msg to send
// 0 =  6 = call to cmMfpClock()
// 1 = 12 = call to cmMfpClock()
// 2 = 22 = call to cmMfpClock()
// 
//    dusecs   etime   mtime 
// 0   n/a       3      13        
// 1    6        9       7
// 2   10       19      -3
//      

cmMfpRC_t cmMfpClock(  cmMfpH_t h, unsigned dusecs )
{
  cmMfp_t* p = _cmMfpHandleToPtr(h);

  if( p->msgIdx >= p->msgN )
    return kEndOfFileMfpRC;

  // get a pointer to the next msg to send
  const cmMidiTrackMsg_t* mp    = p->msgV[p->msgIdx];

  // p->etime is the interval of time between when the last msg was
  // sent and the end of the time window for this mfpClock() cycle
  p->etime += dusecs;

  // if the elapsed time (etime) since the last msg is greater or equal
  // to the delta time to the next msg (mtime)
  while( p->etime >= p->mtime )
  {      

    // send the current message
    p->cbFunc( p->userCbPtr, p->mtime, mp );

    unsigned amicro = mp->amicro;
    
    ++(p->msgIdx);

    if( p->msgIdx >= p->msgN )
      break;

    // get the next msg to send
    mp   = p->msgV[p->msgIdx];


    // we probably went past the actual mtime - so update etime
    // with the delta usecs from the msg just sent and the current time
    p->etime -= p->mtime;

    // calc the delta usecs from the message just sent to the next msg to send
    p->mtime  = mp->amicro - amicro;

  }

  return p->msgIdx >= p->msgN ? kEndOfFileMfpRC : kOkMfpRC;

}


void mfpPrint( void* userDataPtr, const char* fmt, va_list vl )
{
  vprintf(fmt,vl);
}

// this assumes that the seconds have been normalized to a recent start time
// so as to avoid overflow
unsigned _cmMfpElapsedMicroSecs( const struct timespec* t0, const struct timespec* t1 )
{
  // convert seconds to usecs
  long u0 = t0->tv_sec * 1000000;
  long u1 = t1->tv_sec * 1000000;

  // convert nanoseconds to usec
  u0 += t0->tv_nsec / 1000;
  u1 += t1->tv_nsec / 1000;

  // take diff between t1 and t0
  return u1 - u0;
}

void _cmMfpTestTimer()
{
  useconds_t  suspendUsecs    = 15 * 1000;
  struct timespec t0,t1,t2;
  unsigned        accum = 0;
  unsigned        i;
  unsigned        n = 4000;
  
  // t0 will be the base time which all other times will be 
  // set relative to.
  cmTimeGet(&t0);
  t2 = t0;
  t2.tv_sec = 0;

  for(i=0; i<n; ++i)
  {
    cmSleepUs(suspendUsecs);

    
    cmTimeGet(&t1);
    t1.tv_sec -= t0.tv_sec;

    unsigned d0usec = _cmMfpElapsedMicroSecs(&t0,&t1);
    unsigned d1usec = _cmMfpElapsedMicroSecs(&t2,&t1);

    accum += d1usec;

    if( i == n-1 )
      printf("%i %i %i\n",d0usec,d1usec,accum);
    
    t2 = t1;
  }

}

// midi file player callback test function
void _cmMfpCallbackTest( void* userCbPtr, unsigned dmicros, const cmMidiTrackMsg_t* msgPtr )
{
  if( kNoteOffMdId <= msgPtr->status && msgPtr->status <= kPbendMdId )
    cmMpDeviceSend( 0, 0, msgPtr->status+msgPtr->u.chMsgPtr->ch, msgPtr->u.chMsgPtr->d0,msgPtr->u.chMsgPtr->d1);

  //printf("%i 0x%x 0x%x %i\n",msgPtr->tick,msgPtr->status,msgPtr->metaId,msgPtr->trkIdx);
}

// midi port callback test function
void _cmMpCallbackTest( const cmMidiPacket_t* pktArray, unsigned pktCnt )
{}

cmMfpRC_t cmMfpTest( const char* fn, cmCtx_t* ctx )
{
  cmMfpH_t        mfpH               = cmMfpNullHandle;
  cmMfpRC_t       rc;
  useconds_t      suspendUsecs       = 15 * 1000;
  struct timespec t0,t1,base;
  //unsigned        i;
  //unsigned        n                = 4000;
  unsigned        mdParserBufByteCnt = 1024;

  printf("Initializing MIDI Devices...\n");
  cmMpInitialize( ctx, _cmMpCallbackTest, NULL, mdParserBufByteCnt,"app" );

  //mdReport();

  printf("Creating Player...\n");
  if((rc = cmMfpCreate( &mfpH, _cmMfpCallbackTest, NULL, ctx )) != kOkMfpRC )
    return rc;

  printf("Loading MIDI file...\n");
  if((rc = cmMfpLoadFile( mfpH, fn )) != kOkMfpRC )
    goto errLabel;

  if((rc = cmMfpSeek( mfpH, 60 * 1000000 )) != kOkMfpRC )
    goto errLabel;

  cmTimeGet(&base);
  t0 = base;
  t0.tv_sec = 0;

  //for(i=0; i<n; ++i)
  while(rc != kEndOfFileMfpRC)
  {
    cmSleepUs(suspendUsecs);
    
    cmTimeGet(&t1);    
    t1.tv_sec -= base.tv_sec;

    unsigned dusecs = _cmMfpElapsedMicroSecs(&t0,&t1);
   
    rc = cmMfpClock( mfpH, dusecs );
    //printf("%i %i\n",dusecs,rc);
    t0 = t1;
  }
 
 errLabel:
  cmMfpDestroy(&mfpH);

  cmMpFinalize();

  return rc;
}


//------------------------------------------------------------------------------------------------------------
#include "cmFloatTypes.h"
#include "cmComplexTypes.h"
#include "cmLinkedHeap.h"
#include "cmSymTbl.h"
#include "cmAudioFile.h"
#include "cmProcObj.h"
#include "cmProcTemplateMain.h"
#include "cmVectOps.h"

#include "cmProc.h"
#include "cmProc2.h"


enum
{
  kOkMfptRC = cmOkRC,
  kMfpFailMfptRC,
  kAudioFileFailMfptRC,
  kProcObjFailMfptRC
};

typedef struct
{
  cmErr_t*     err;
  cmMidiSynth* msp; 
} _cmMfpTest2CbData_t;


// Called by the MIDI file player to send a msg to the MIDI synth. 
void _cmMfpCb( void* userCbPtr, unsigned dmicros, const cmMidiTrackMsg_t* msgPtr )
{
  if( kNoteOffMdId <= msgPtr->status && msgPtr->status <= kPbendMdId )
  {
    cmMidiPacket_t pkt;
    cmMidiMsg      msg;
    _cmMfpTest2CbData_t* d = (_cmMfpTest2CbData_t*)userCbPtr;

    msg.timeStamp.tv_sec = 0;
    msg.timeStamp.tv_nsec = 0;
    msg.status  = msgPtr->status + msgPtr->u.chMsgPtr->ch;
    msg.d0      = msgPtr->u.chMsgPtr->d0;
    msg.d1      = msgPtr->u.chMsgPtr->d1;

    pkt.cbDataPtr = NULL;
    pkt.devIdx    = cmInvalidIdx;
    pkt.portIdx   = cmInvalidIdx;
    pkt.msgArray  = &msg;
    pkt.sysExMsg  = NULL;
    pkt.msgCnt    = 1;

    if( cmMidiSynthOnMidi( d->msp, &pkt, 1 ) != cmOkRC )
      cmErrMsg(d->err,kProcObjFailMfptRC,"Synth. MIDI receive failed.");
  }
}

// Called by the MIDI synth to send a msg to the voice bank.
int _cmMidiSynthCb( struct cmMidiVoice_str* voicePtr, unsigned sel, cmSample_t* outChArray[], unsigned outChCnt )
{
  return cmWtVoiceBankExec( ((cmWtVoiceBank*)voicePtr->pgm.cbDataPtr), voicePtr, sel, outChArray, outChCnt );
}


// BUG BUG BUG: THIS FUNCTION IS NOT TESTED!!!!!
cmRC_t cmMfpTest2( const char* midiFn, const char* audioFn, cmCtx_t* ctx )
{
  cmRC_t              rc         = kOkMfptRC;
  cmMfpH_t            mfpH       = cmMfpNullHandle;
  _cmMfpTest2CbData_t cbData;
  cmErr_t             err;
  cmAudioFileH_t      afH        = cmNullAudioFileH;
  cmRC_t              afRC       = kOkAfRC;
  double              afSrate    = 44100;
  unsigned            afBits     = 16;
  unsigned            afChCnt    = 1;
  cmCtx*              cctx;
  cmMidiSynth*        msp;
  cmWtVoiceBank*      vbp;
  unsigned            msPgmCnt   = 127;
  cmMidiSynthPgm      msPgmArray[ msPgmCnt ];
  unsigned            msVoiceCnt = 36;
  unsigned            procSmpCnt = 64;
  unsigned            i;

  cmErrSetup(&err,&ctx->rpt,"MFP Test 2");

  // create the MIDI file player
  if( cmMfpCreate(&mfpH, _cmMfpCb, &cbData, ctx ) != kOkMfpRC )
    return cmErrMsg(&err,kMfpFailMfptRC,"MIDI file player create failed.");

  // create an output audio file
  if( cmAudioFileIsValid( afH = cmAudioFileNewCreate(audioFn, afSrate, afBits, afChCnt, &afRC, &ctx->rpt))==false)
  {
    rc = cmErrMsg(&err,kAudioFileFailMfptRC,"The audio file create failed.");
    goto errLabel;
  }

  // load the midi file into the player
  if( cmMfpLoadFile( mfpH, midiFn ) != kOkMfpRC )
  {
    rc = cmErrMsg(&err,kMfpFailMfptRC,"MIDI file load failed.");
    goto errLabel;
  }

  // create the proc obj context
  if((cctx = cmCtxAlloc(NULL, &ctx->rpt, cmLHeapNullHandle, cmSymTblNullHandle )) == NULL)
  {
    rc = cmErrMsg(&err,kProcObjFailMfptRC,"cmCtx allocate failed.");
    goto errLabel;
  }

  // create the voice bank
  if((vbp = cmWtVoiceBankAlloc(cctx, NULL, afSrate, procSmpCnt, msVoiceCnt, afChCnt )) == NULL)
  {
    rc = cmErrMsg(&err,kProcObjFailMfptRC,"WT voice bank allocate failed.");
    goto errLabel;
  }

  // a MIDI synth
  if((msp =  cmMidiSynthAlloc(cctx, NULL, msPgmArray, msPgmCnt, msVoiceCnt, procSmpCnt, afChCnt, afSrate  )) == NULL )
  {
    rc = cmErrMsg(&err,kProcObjFailMfptRC,"MIDI synth allocate failed.");
    goto errLabel;
  }

  cbData.msp = msp;
  cbData.err = &err;

  // load all of the the MIDI pgm recds with the same settings
  for(i=0; i<msPgmCnt; ++i)
  {
    msPgmArray[i].pgm       = i;
    msPgmArray[i].cbPtr     = _cmMidiSynthCb; // Call this function to update voices using this pgm
    msPgmArray[i].cbDataPtr = vbp;            // Voice bank containing the voice states.
  }


  unsigned dusecs = floor((double)procSmpCnt * 1000000. / afSrate);

  while(rc != kEndOfFileMfpRC)
  {
    // update the MFP's current time and call _cmMfpCb() for MIDI msgs whose time has elapsed
    rc = cmMfpClock( mfpH, dusecs );

    // check for MFP errors
    if(rc!=kOkMfpRC && rc!=kEndOfFileMfpRC)
    {
      cmErrMsg(&err,kMfpFailMfptRC,"MIDI file player exec failed.");
      goto errLabel;
    }
    
    // generate audio based on the current state of the synth voices
    if( cmMidiSynthExec(msp, NULL, 0 ) != cmOkRC )
    {
      cmErrMsg(&err,kProcObjFailMfptRC,"MIDI synth exec. failed.");
      goto errLabel;
    }

    // write the last frame of synth. generated audio to the output file
    if( cmAudioFileWriteSample(afH, procSmpCnt, msp->outChCnt, msp->outChArray ) != kOkAfRC )
    {
      cmErrMsg(&err,kProcObjFailMfptRC,"Audio file write failed.");
      goto errLabel;
    }
  }
  
 errLabel:
  if( cmMidiSynthFree(&msp) != cmOkRC )
    cmErrMsg(&err,kProcObjFailMfptRC,"MIDI synth. free failed.");

  if( cmWtVoiceBankFree(&vbp) != cmOkRC )
    cmErrMsg(&err,kProcObjFailMfptRC,"WT voice free failed.");

  if( cmCtxFree(&cctx) != cmOkRC )
    cmErrMsg(&err,kProcObjFailMfptRC,"cmCtx free failed.");

  if( cmAudioFileDelete(&afH) )
    cmErrMsg(&err,kAudioFileFailMfptRC,"The audio file close failed.");

  if( cmMfpDestroy(&mfpH) != kOkMfpRC )
    cmErrMsg(&err,kMfpFailMfptRC,"MIDI file player destroy failed.");


  return rc;
}
