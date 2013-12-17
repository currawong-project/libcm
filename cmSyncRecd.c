#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmTime.h"
#include "cmFile.h"
#include "cmAudioFile.h"
#include "cmSyncRecd.h"
#include "cmVectOpsTemplateMain.h"
#include "cmMidi.h"
typedef enum
{
  kInvalidSrId,
  kMidiSrId,
  kAudioSrId
  } cmSrTypeId_t;

typedef struct cmSrMidi_str
{
  cmTimeSpec_t timestamp;
  unsigned     status;
  unsigned     d0;
  unsigned     d1;
} cmSrMidi_t;

typedef struct cmSrAudio_str
{
  cmTimeSpec_t timestamp;
  unsigned     smpIdx;
} cmSrAudio_t;

typedef struct cmSrRecd_str
{
  cmSrTypeId_t tid;
  union
  {
    cmSrMidi_t  m;
    cmSrAudio_t a;
  } u;

} cmSrRecd_t;


enum
{
  kReadSrFl = 0x01,   // This is a read (not a write) file

  kFileUUSrId = 0xf00d
};

typedef struct cmSr_str
{
  cmErr_t           err;
  cmFileH_t         fH;
  cmAudioFileH_t    afH;
  unsigned          flags;      // See kXXXSrFl 
  cmSrRecd_t*       cache;      // cache[cn]
  cmSrAudio_t*      map;        // 
  unsigned          cn;         // count of records in cache[].
  unsigned          ci;         // next cache recd index
  unsigned          fn;         // count of recds written to file
  long              offs;
  cmAudioFileInfo_t afInfo;
} cmSr_t;


cmSyncRecdH_t cmSyncRecdNullHandle  = cmSTATIC_NULL_HANDLE;

cmSr_t* _cmSrHtoP( cmSyncRecdH_t h )
{
  cmSr_t* p = (cmSr_t*)h.h;
  assert(p!=NULL);
  return p;
}

cmSrRC_t _cmSrWriteCache( cmSr_t* p )
{
  if( cmFileWrite(p->fH,p->cache,p->ci * sizeof(cmSrRecd_t)) != kOkFileRC )
    return cmErrMsg(&p->err,kFileFailSrRC,"File write failed.");

  p->fn += p->ci;
  p->ci = 0;
  
  return kOkSrRC;
}


cmSrRC_t _cmSrFinal( cmSr_t* p )
{
  cmSrRC_t rc = kOkSrRC;
  
  // write any remaining cache records
  if( cmIsFlag(p->flags,kReadSrFl) == false )
  {
    if((rc = _cmSrWriteCache(p)) == kOkSrRC )
    {
      if(cmFileSeek(p->fH,kBeginFileFl,p->offs) != kOkFileRC )
        rc = cmErrMsg(&p->err,kFileFailSrRC, "File seek fail on file offset positioning.");
      else
        if(cmFileWriteUInt(p->fH,&p->fn,1) != kOkFileRC )
          rc = cmErrMsg(&p->err,kFileFailSrRC, "File write failed on record count.");
    }
  }

  // release the audio file object
  if( cmAudioFileIsValid(p->afH) )
    if( cmAudioFileDelete(&p->afH) != kOkAfRC )
      return cmErrMsg(&p->err,kAudioFileFailSrRC,"Audio file object delete failed.");

  // release the sync-recd file object
  if( cmFileIsValid(p->fH) )
    if( cmFileClose(&p->fH) != kOkFileRC )
      return cmErrMsg(&p->err,kFileFailSrRC,"File close failed.");

  cmMemFree(p->cache);
  cmMemFree(p->map);
  cmMemFree(p);

  return rc;
}

cmSr_t*  _cmSrAlloc( cmCtx_t* ctx, unsigned flags )
{
  cmSr_t* p = cmMemAllocZ(cmSr_t,1);

  cmErrSetup(&p->err,&ctx->rpt,"SyncRecd");

  p->cn    = 2048;
  p->cache = cmMemAllocZ(cmSrRecd_t,p->cn);

  return p;
}

cmSrRC_t cmSyncRecdCreate( cmCtx_t* ctx, cmSyncRecdH_t* hp, const cmChar_t* srFn, const cmChar_t* audioFn, double srate, unsigned chCnt, unsigned bits )
{
  cmSrRC_t rc   = kOkSrRC;
  cmRC_t   afRC = kOkAfRC;

  assert( audioFn != NULL );

  if((rc = cmSyncRecdFinal(hp)) != kOkSrRC )
    return rc;

  cmSr_t* p = _cmSrAlloc(ctx,0);

  if( cmFileOpen(&p->fH,srFn,kWriteFileFl,&ctx->rpt) != kOkFileRC )
  {
    rc = cmErrMsg(&p->err,kFileFailSrRC,"Unable to create the sync-recd file '%s'.",cmStringNullGuard(srFn));
    goto errLabel;
  }

  if( cmAudioFileIsValid(p->afH = cmAudioFileNewCreate(audioFn,srate,bits,chCnt,&afRC,&ctx->rpt))==false)
  {
    rc = cmErrMsg(&p->err,kAudioFileFailSrRC,"Unable to create the sync-recd audio file '%s'.",cmStringNullGuard(audioFn));
    goto errLabel;
  }

  unsigned fileUUId = kFileUUSrId;
  unsigned audioFnCnt = strlen(audioFn)+1;

  if( cmFileWriteUInt(p->fH,&fileUUId,1) != kOkFileRC )
  {
    rc = cmErrMsg(&p->err,kFileFailSrRC,"File write failed on UUID.");
    goto errLabel;
  }

  if( cmFileWriteUInt(p->fH,&audioFnCnt,1) != kOkFileRC )
  {
    rc = cmErrMsg(&p->err,kFileFailSrRC,"File write failed on audio file length write count.");
    goto errLabel;
  }

  if( cmFileWriteChar(p->fH,audioFn,audioFnCnt) != kOkFileRC )
  {
    rc = cmErrMsg(&p->err,kFileFailSrRC,"File write failed on audio file string.");
    goto errLabel;
  }

  if( cmFileTell(p->fH,&p->offs) != kOkFileRC )
  {
    rc = cmErrMsg(&p->err,kFileFailSrRC,"Unable to determine file offset.");
    goto errLabel;
  }

  if( cmFileWriteUInt(p->fH,&p->fn,1) != kOkFileRC )
  {
    rc = cmErrMsg(&p->err,kFileFailSrRC,"File write failed on initial record count.");
    goto errLabel;
  }

  hp->h = p;

 errLabel:
  if( rc != kOkSrRC )
    _cmSrFinal(p);

  return rc;
}


cmSrRC_t cmSyncRecdOpen(   cmCtx_t* ctx, cmSyncRecdH_t* hp, const cmChar_t* srFn )
{
  cmSrRC_t  rc         = kOkSrRC;
  cmRC_t    afRC       = kOkAfRC;
  unsigned  fileUUId   = cmInvalidId;
  unsigned  audioFnCnt = 0;
  cmChar_t* audioFn    = NULL;
  unsigned i;
  unsigned acnt = 0;
  unsigned mcnt = 0;
  unsigned* tiV  = NULL;

  if((rc = cmSyncRecdFinal(hp)) != kOkSrRC )
    return rc;

  cmSr_t* p = _cmSrAlloc(ctx,kReadSrFl);

  if( cmFileOpen(&p->fH,srFn,kReadFileFl,&ctx->rpt) != kOkFileRC )
  {
    rc = cmErrMsg(&p->err,kFileFailSrRC,"Unable to open the sync-recd file '%s'.",cmStringNullGuard(srFn));
    goto errLabel;
  }

  if( cmFileReadUInt(p->fH,&fileUUId,1) != kOkFileRC )
  {
    rc = cmErrMsg(&p->err,kFileFailSrRC,"File read failed on UUId.");
    goto errLabel;
  }

  if( cmFileReadUInt(p->fH,&audioFnCnt,1) != kOkFileRC )
  {
    rc = cmErrMsg(&p->err,kFileFailSrRC,"File read failed on audio file name count.");
    goto errLabel;
  }

  audioFn = cmMemAllocZ(cmChar_t,audioFnCnt);

  if( cmFileReadChar(p->fH,audioFn,audioFnCnt) != kOkFileRC )
  {
    rc = cmErrMsg(&p->err,kFileFailSrRC,"File read failed on audio file string.");
    goto errLabel;
  }

  if( cmFileReadUInt(p->fH,&p->fn,1) != kOkFileRC )
  {
    rc = cmErrMsg(&p->err,kFileFailSrRC,"File read failed on record count.");
    goto errLabel;
  }

  // store the file offset to the first recd
  if( cmFileTell(p->fH,&p->offs) != kOkFileRC )
  {
    rc = cmErrMsg(&p->err,kFileFailSrRC,"Unable to determine the current file offset.");
    goto errLabel;
  }


  // read each file - and count the types
  for(i=0; i<p->fn; ++i)
  {
    cmSrRecd_t r;
    if( cmFileRead(p->fH,&r,sizeof(r)) != kOkFileRC )
    {
      rc = cmErrMsg(&p->err,kFileFailSrRC,"Unable to read the record at index %i.");
      goto errLabel;
    }

    switch(r.tid)
    {
      case kMidiSrId:
        mcnt += 1;
        break;

      case kAudioSrId:
        acnt += 1;
        break;

      default:
        { assert(0); }
    }
  }

  printf("%i %i = %i\n",mcnt,acnt,p->fn);

  // rewind to the begining of the records
  if( cmFileSeek(p->fH,kBeginFileFl,p->offs) != kOkFileRC )
  {
    rc = cmErrMsg(&p->err,kFileFailSrRC,"Unable to seek to first recd offset.");
    goto errLabel;
  }

  // allocate space to hold the MIDI records
  p->cn    = mcnt;
  p->ci    = 0;
  p->cache = cmMemResizeZ(cmSrRecd_t,p->cache,p->cn);
  p->map   = cmMemAllocZ(cmSrAudio_t,p->cn);
  tiV      = cmMemAllocZ(unsigned,p->cn);
  
  for(i=0; p->ci<p->cn && i<p->fn; ++i)
  {
    if( cmFileRead(p->fH,p->cache + p->ci,sizeof(cmSrRecd_t)) != kOkFileRC )
    {
      rc = cmErrMsg(&p->err,kFileFailSrRC,"Unable to read the record at index %i.");
      goto errLabel;
    }

    if( p->cache[p->ci].tid == kMidiSrId )
      p->ci += 1;
  }

  // assert that all the MIDI records were read
  assert( p->ci == p->cn);

  // rewind to the first recd
  if( cmFileSeek(p->fH,kBeginFileFl,p->offs) != kOkFileRC )
  {
    rc = cmErrMsg(&p->err,kFileFailSrRC,"Unable to seek to first recd offset.");
    goto errLabel;
  }


  // for each recd in the file
  for(i=0; i<p->fn; ++i)
  {
    cmSrRecd_t r;
    if( cmFileRead(p->fH,&r,sizeof(r)) != kOkFileRC )
    {
      rc = cmErrMsg(&p->err,kFileFailSrRC,"Unable to read the record at index %i.");
      goto errLabel;
    }

    // if this is an audio record
    if( r.tid == kAudioSrId )
    {
      unsigned j;
      
      // for each midi record
      for(j=0; j<p->cn; ++j)
      {
        // measure the time interval between this midi and this audio recd
        unsigned time_interval_micros = cmTimeAbsElapsedMicros(&r.u.a.timestamp,&p->cache[j].u.m.timestamp);

        // if the audio recd is closer to this midi recd than prior audio records ...
        if( time_interval_micros < tiV[j]  || i==0 )
        {
          // ... then store the audio time stamp in the map
          tiV[j]              = time_interval_micros;
          p->map[j].timestamp = r.u.a.timestamp;
          p->map[j].smpIdx    = r.u.a.smpIdx;
        }        
      }
    }
  }

  // open the audio file
  if( cmAudioFileIsValid(p->afH = cmAudioFileNewOpen(audioFn,&p->afInfo,&afRC,&ctx->rpt ))==false)
  {
    rc = cmErrMsg(&p->err,kAudioFileFailSrRC,"Unable to open the sync-recd audio file '%s'.",cmStringNullGuard(audioFn));
    goto errLabel;
  }
 
  p->flags = cmSetFlag(p->flags,kReadSrFl);

  hp->h    = p;

 errLabel:

  cmMemFree(tiV);
  cmMemFree(audioFn);

  if( rc != kOkSrRC )
    _cmSrFinal(p);

  return rc;
}

cmSrRC_t cmSyncRecdFinal(  cmSyncRecdH_t* hp )
{
  cmSrRC_t rc = kOkSrRC;

  if( hp==NULL || cmSyncRecdIsValid(*hp)==false)
    return rc;

  cmSr_t* p = _cmSrHtoP(*hp);

  if((rc = _cmSrFinal(p)) != kOkSrRC )
    return rc;

  hp->h = NULL;

  return rc;
}

bool     cmSyncRecdIsValid( cmSyncRecdH_t h )
{ return h.h != NULL; }

cmSrRC_t cmSyncRecdMidiWrite(  cmSyncRecdH_t h, const cmTimeSpec_t* timestamp, unsigned status, unsigned d0, unsigned d1 )
{
  cmSrRC_t    rc    = kOkSrRC;
  cmSr_t*     p     = _cmSrHtoP(h);
  cmSrRecd_t* rp    = p->cache + p->ci;
  rp->tid           = kMidiSrId;
  rp->u.m.timestamp = *timestamp;
  rp->u.m.status    = status;
  rp->u.m.d0        = d0;
  rp->u.m.d1        = d1;

  p->ci += 1;

  if( p->ci == p->cn )
    rc = _cmSrWriteCache(p);
  
  return rc;
}

cmSrRC_t cmSyncRecdAudioWrite( cmSyncRecdH_t h, const cmTimeSpec_t* timestamp, unsigned smpIdx, const cmSample_t* ch[], unsigned chCnt, unsigned frmCnt )
{
  cmSrRC_t    rc    = kOkSrRC;
  cmSr_t*     p     = _cmSrHtoP(h);
  cmSrRecd_t* rp    = p->cache + p->ci;
  rp->tid           = kAudioSrId;
  rp->u.a.timestamp = *timestamp;
  rp->u.a.smpIdx    =  smpIdx;

  p->ci += 1;

  if( p->ci == p->cn )
    if((rc = _cmSrWriteCache(p)) != kOkSrRC )
      goto errLabel;

  if( cmAudioFileWriteSample(p->afH,frmCnt,chCnt,(cmSample_t**)ch) != kOkAfRC )
    rc = cmErrMsg(&p->err,kAudioFileFailSrRC,"Audio file write failed.");

 errLabel:
  return rc;
}


cmSrRC_t cmSyncRecdPrint( cmSyncRecdH_t h )
{
  cmSrRC_t rc = kOkSrRC;
  cmSr_t*  p  = _cmSrHtoP(h);
  unsigned i;

  if( cmIsFlag(p->flags,kReadSrFl)==false)
    return cmErrMsg(&p->err,kInvalidOpSrRC,"The 'print' operation is only valid on sync-recd files opened for reading.");
  
  for(i=0; i<p->cn; ++i)
  {
    cmSrRecd_t* r = p->cache + i;
    cmRptPrintf(p->err.rpt,"0x%x %3i %3i %ld %5.3f : %ld %5.3f %i",r->u.m.status,r->u.m.d0,r->u.m.d1,r->u.m.timestamp.tv_sec,r->u.m.timestamp.tv_nsec/1000000000.0,p->map[i].timestamp.tv_sec,p->map[i].timestamp.tv_nsec/1000000000.0,p->map[i].smpIdx);
  }

  return rc;
}

cmSrRC_t cmSyncRecdAudioFile( cmSyncRecdH_t h, const cmChar_t* fn )
{
  cmSrRC_t       rc        = kOkSrRC;
  cmSr_t*        p         = _cmSrHtoP(h);
  cmAudioFileH_t afH       = cmNullAudioFileH;
  unsigned       chCnt     = 2;
  unsigned       frmCnt    = 1024;
  unsigned       chIdx     = 0;
  cmRC_t         afRC      = kOkAfRC;
  unsigned       actFrmCnt = 0;
  unsigned       smpIdx    = 0;
  cmSample_t*    chs[chCnt];
  cmSample_t     buf[frmCnt*2];
  chs[0] = buf;
  chs[1] = buf+frmCnt;

  if( cmIsFlag(p->flags,kReadSrFl)==false)
    return cmErrMsg(&p->err,kInvalidOpSrRC,"The 'audio-file-output' operation is only valid on sync-recd files opened for reading.");
  
  /// Open an audio file for writing
  if(cmAudioFileIsValid(afH = cmAudioFileNewCreate(fn, p->afInfo.srate, p->afInfo.bits, chCnt, &afRC, p->err.rpt))==false)
  {
    rc = cmErrMsg(&p->err,kAudioFileFailSrRC,"Unable to create the synchronized audio file '%s'.",cmStringNullGuard(fn));
    goto errLabel;
  }
  
  // rewind the input audio file
  if( cmAudioFileSeek(p->afH,0) != kOkAfRC )
  {
    rc = cmErrMsg(&p->err,kAudioFileFailSrRC,"Seek failed during synchronized audio file output.");
    goto errLabel;
  }

  actFrmCnt = frmCnt;

  // for each buffer of audio 
  for(smpIdx=0; actFrmCnt==frmCnt; smpIdx+=actFrmCnt)
  {
    unsigned i;

    // read frmCnt samples from the first channel of the input audio file
    if( cmAudioFileReadSample(p->afH, frmCnt, chIdx, 1, chs, &actFrmCnt ) != kOkAfRC )
    {
      rc = cmErrMsg(&p->err,kAudioFileFailSrRC,"Audio file read failed.");
      break;
    }

    // zero the output buffer for the second audio channel
    cmVOS_Zero(chs[1],frmCnt);

    // insert impulses at the location of the MIDI messages.
    for(i=0; i<p->cn; ++i)
      if( p->cache[i].u.m.status==kNoteOnMdId && smpIdx <= p->map[i].smpIdx && p->map[i].smpIdx < smpIdx+frmCnt )
        chs[1][ p->map[i].smpIdx - smpIdx ] = 1.0;

    // write the audio output samples
    if( cmAudioFileWriteSample(afH, frmCnt, chCnt, chs ) != kOkAfRC )
    {
      rc = cmErrMsg(&p->err,kAudioFileFailSrRC,"Synchronized audio file write failed.");
      break;
    }    

  }

 errLabel:
  if( cmAudioFileDelete(&afH) != kOkAfRC )
    rc = cmErrMsg(&p->err,kAudioFileFailSrRC,"Synchronized audio file close failed.");

  return rc;
}

cmSrRC_t cmSyncRecdTest( cmCtx_t* ctx )
{
  enum
  {
    kOkTestRC,
    kTestFailRC,
  };

  cmSrRC_t rc = kOkSrRC;
  const cmChar_t* srFn = "/home/kevin/temp/kr/sr/sr0.sr";
  const cmChar_t* aFn  = "/home/kevin/temp/kr/sr/sync_af.aiff";
  cmErr_t err;
  cmSyncRecdH_t srH = cmSyncRecdNullHandle;

  cmErrSetup(&err,&ctx->rpt,"SyncRecdTest");
  

  if((rc = cmSyncRecdOpen(ctx, &srH, srFn )) != kOkSrRC )
  {
    cmErrMsg(&err,kTestFailRC,"Sync-recd open failed.");
    goto errLabel;
  }

  cmSyncRecdPrint(srH);
  cmSyncRecdAudioFile(srH,aFn);

 errLabel:
  if((rc = cmSyncRecdFinal(&srH)) != kOkSrRC )
    cmErrMsg(&err,kTestFailRC,"Sync-recd close failed.");

  return rc;

}
