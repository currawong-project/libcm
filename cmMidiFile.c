#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmFile.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmTime.h"
#include "cmMidi.h"
#include "cmMidiFile.h"

#ifdef cmBIG_ENDIAN
#define mfSwap16(v)  (v)
#define mfSwap32(v)  (v)
#else
#define mfSwap16(v)  cmSwap16(v)
#define mfSwap32(v)  cmSwap32(v)
#endif

typedef struct
{
  unsigned          cnt;   // count of track records
  cmMidiTrackMsg_t* base;  // pointer to first track recd
  cmMidiTrackMsg_t* last;  // pointer to last track recd
} _cmMidiTrack_t;

typedef struct
{
  cmErr_t            err;                // this objects error object
  cmLHeapH_t         lhH;                // linked heap used for all dynamically alloc'd data space
  cmFileH_t          fh;                 // cmFile handle (only used in fmMidiFileOpen() and cmMidiFileWrite())
  unsigned short     fmtId;              // midi file type id: 0,1,2
  unsigned short     ticksPerQN;         // ticks per quarter note or 0 if smpteFmtId is valid
  cmMidiByte_t       smpteFmtId;         // smpte format or 0 if ticksPerQN is valid
  cmMidiByte_t       smpteTicksPerFrame; // smpte ticks per frame or 0 if ticksPerQN is valid
  unsigned short     trkN;               // track count
  _cmMidiTrack_t*    trkV;               // track vector
  char*              fn;                 // file name or NULL if this object did not originate from a file
  unsigned           msgN;               // count of msg's in msgV[]
  cmMidiTrackMsg_t** msgV;               // sorted msg list
  unsigned           nextUid;            // next available msg uid
} _cmMidiFile_t;


cmMidiFileH_t cmMidiFileNullHandle = cmSTATIC_NULL_HANDLE;


_cmMidiFile_t* _cmMidiFileHandleToPtr( cmMidiFileH_t h )
{
  _cmMidiFile_t* p = (_cmMidiFile_t*)h.h;
  assert( p != NULL );
  return p;
}

void* _cmMidiFileMalloc( _cmMidiFile_t* mfp, unsigned byteN )
{ return cmLHeapAllocZ(mfp->lhH,byteN); }


cmMfRC_t _cmMidiFileRead8( _cmMidiFile_t* mfp, cmMidiByte_t* p )
{
  if( cmFileReadUChar(mfp->fh,p,1) != kOkFileRC )  
    return cmErrMsg(&mfp->err,kFileFailMfRC,"MIDI byte read failed.");
  return kOkMfRC;
}

cmMfRC_t _cmMidiFileRead16( _cmMidiFile_t* mfp, unsigned short* p )
{
  if( cmFileReadUShort(mfp->fh,p,1) != kOkFileRC )
    return cmErrMsg(&mfp->err,kFileFailMfRC,"MIDI short read failed.");

  *p = mfSwap16(*p);

  return kOkMfRC;
}

cmMfRC_t _cmMidiFileRead24( _cmMidiFile_t* mfp, unsigned* p )
{
  *p = 0;
  int i = 0;
  for(; i<3; ++i)
  {
    unsigned char c;
    if( cmFileReadUChar(mfp->fh,&c,1) != kOkFileRC )  
      return cmErrMsg(&mfp->err,kFileFailMfRC,"MIDI 24 bit integer read failed.");
    *p = (*p << 8) + c;
  }

  //*p =mfSwap32(*p);

  return kOkMfRC;
}

cmMfRC_t _cmMidiFileRead32( _cmMidiFile_t* mfp, unsigned* p )
{
  if( cmFileReadUInt(mfp->fh,p,1) != kOkFileRC )  
    return cmErrMsg(&mfp->err,kFileFailMfRC,"MIDI integer read failed.");

  *p = mfSwap32(*p);

  return kOkMfRC;
}

cmMfRC_t _cmMidiFileReadText( _cmMidiFile_t* mfp, cmMidiTrackMsg_t* tmp, unsigned byteN )
{
  if( byteN == 0 )
    return kOkMfRC;

  char*  t = (char*)_cmMidiFileMalloc(mfp,byteN+1);
  t[byteN] = 0;
  
  if( cmFileReadChar(mfp->fh,t,byteN) != kOkFileRC )  
    return cmErrMsg(&mfp->err,kFileFailMfRC,"MIDI read text failed.");

  tmp->u.text  = t;
  tmp->byteCnt = byteN;
  return kOkMfRC;
}

cmMfRC_t _cmMidiFileReadRecd( _cmMidiFile_t* mfp, cmMidiTrackMsg_t* tmp, unsigned byteN )
{
  char*  t = (char*)_cmMidiFileMalloc(mfp,byteN);
  
  if( cmFileReadChar(mfp->fh,t,byteN) != kOkFileRC )  
    return cmErrMsg(&mfp->err,kFileFailMfRC,"MIDI read record failed.");

  tmp->byteCnt = byteN;
  tmp->u.voidPtr = t;

  return kOkMfRC;
}

cmMfRC_t _cmMidiFileReadVarLen( _cmMidiFile_t* mfp, unsigned* p )
{
  unsigned char c;

  if( cmFileReadUChar(mfp->fh,&c,1) != kOkFileRC )  
    return cmErrMsg(&mfp->err,kFileFailMfRC,"MIDI read variable length integer failed.");
  
  if( !(c & 0x80) )
    *p = c;
  else
  {
    *p = c & 0x7f;

    do 
    {

      if( cmFileReadUChar(mfp->fh,&c,1) != kOkFileRC )  
        return cmErrMsg(&mfp->err,kFileFailMfRC,"MIDI read variable length integer failed.");

      *p = (*p << 7) + (c & 0x7f);
    
    }while( c & 0x80 );
  }

  return kOkMfRC; 
}

cmMidiTrackMsg_t* _cmMidiFileAllocMsg( _cmMidiFile_t* mfp, unsigned short trkIdx, unsigned dtick, cmMidiByte_t status )
{
  cmMidiTrackMsg_t* tmp = (cmMidiTrackMsg_t*)_cmMidiFileMalloc(mfp, sizeof(cmMidiTrackMsg_t) );

  // set the generic track record fields
  tmp->dtick     = dtick;
  tmp->status   = status;
  tmp->metaId   = kInvalidMetaMdId;
  tmp->trkIdx   = trkIdx;
  tmp->byteCnt  = 0;
  tmp->uid      = mfp->nextUid++;

  return tmp;
}

cmMfRC_t _cmMidiFileAppendTrackMsg( _cmMidiFile_t* mfp, unsigned short trkIdx, unsigned dtick, cmMidiByte_t status, cmMidiTrackMsg_t** trkMsgPtrPtr )
{
  cmMidiTrackMsg_t* tmp = _cmMidiFileAllocMsg( mfp, trkIdx, dtick, status );
    
  // link new record onto track record chain
  if( mfp->trkV[trkIdx].base == NULL )
    mfp->trkV[trkIdx].base = tmp;
  else
    mfp->trkV[trkIdx].last->link = tmp;

  mfp->trkV[trkIdx].last = tmp;
  mfp->trkV[trkIdx].cnt++;


  *trkMsgPtrPtr = tmp;

  return kOkMfRC;
}

cmMfRC_t _cmMidiFileReadSysEx( _cmMidiFile_t* mfp, cmMidiTrackMsg_t* tmp, unsigned byteN )
{
  cmMfRC_t     rc = kOkMfRC;
  cmMidiByte_t b  = 0;

  if( byteN == cmInvalidCnt )
  {

    long offs;
    if( cmFileTell(mfp->fh,&offs) != kOkFileRC )
      return cmErrMsg(&mfp->err,kFileFailMfRC,"MIDI File 'tell' failed.");

    byteN = 0;

    // get the length of the sys-ex msg
    while( !cmFileEof(mfp->fh) && (b != kSysComEoxMdId) )
    {
      if((rc = _cmMidiFileRead8(mfp,&b)) != kOkMfRC )
        return rc;
   
      ++byteN;
    }

    // verify that the EOX byte was found
    if( b != kSysComEoxMdId )
      return cmErrMsg(&mfp->err,kMissingEoxMfRC,"MIDI file missing 'end-of-sys-ex'.");

    // rewind to the beginning of the msg
    if( cmFileSeek(mfp->fh,kBeginFileFl,offs) != kOkFileRC )
      return cmErrMsg(&mfp->err,kFileFailMfRC,"MIDI file seek failed on sys-ex read.");

  }

  // allocate memory to hold the sys-ex msg
  cmMidiByte_t* mp = (cmMidiByte_t *)_cmMidiFileMalloc(mfp,  byteN );

  // read the sys-ex msg from the file into msg memory
  if( cmFileReadUChar(mfp->fh,mp,byteN) != kOkFileRC )  
    return cmErrMsg(&mfp->err,kFileFailMfRC,"MIDI sys-ex read failed.");
  
  tmp->byteCnt     = byteN;
  tmp->u.sysExPtr = mp;
  
  return rc;
}

cmMfRC_t _cmMidiFileReadChannelMsg( _cmMidiFile_t* mfp, cmMidiByte_t* rsPtr, cmMidiByte_t status, cmMidiTrackMsg_t* tmp )
{
  cmMfRC_t       rc       = kOkMfRC;
  cmMidiChMsg_t* p        = (cmMidiChMsg_t*)_cmMidiFileMalloc(mfp,sizeof(cmMidiChMsg_t));
  unsigned       useRsFl  = status <= 0x7f;
  cmMidiByte_t   statusCh = useRsFl ? *rsPtr : status;
  
  if( useRsFl )
    p->d0  = status;    
  else
    *rsPtr = status;

  tmp->byteCnt = sizeof(cmMidiChMsg_t);
  tmp->status  = statusCh & 0xf0;
  p->ch        = statusCh & 0x0f;
  p->durMicros = 0;

  unsigned byteN = cmMidiStatusToByteCount(tmp->status);
  
  if( byteN==kInvalidMidiByte || byteN > 2 )
    return cmErrMsg(&mfp->err,kInvalidStatusMfRC,"Invalid status:0x%x %i.",tmp->status,tmp->status);

  unsigned i;
  for(i=useRsFl; i<byteN; ++i)
  {
    cmMidiByte_t* b = i==0 ? &p->d0 : &p->d1;
  
    if((rc = _cmMidiFileRead8(mfp,b)) != kOkMfRC )
      return rc;
  }

  // convert note-on velocity=0 to note off
  if( tmp->status == kNoteOnMdId && p->d1==0 )
    tmp->status = kNoteOffMdId;

  tmp->u.chMsgPtr = p;
  
  return rc;
}

cmMfRC_t _cmMidiFileReadMetaMsg( _cmMidiFile_t* mfp, cmMidiTrackMsg_t* tmp )
{
  cmMidiByte_t metaId;
  cmMfRC_t     rc;
  unsigned   byteN = 0;

  if((rc = _cmMidiFileRead8(mfp,&metaId)) != kOkMfRC )
    return rc;

  if((rc = _cmMidiFileReadVarLen(mfp,&byteN)) != kOkMfRC )
    return rc;

  //printf("mt: %i 0x%x n:%i\n",metaId,metaId,byteN);

  switch( metaId )
  {
    case kSeqNumbMdId:   rc = _cmMidiFileRead16(mfp,&tmp->u.sVal); break;
    case kTextMdId:      rc = _cmMidiFileReadText(mfp,tmp,byteN);  break;
    case kCopyMdId:      rc = _cmMidiFileReadText(mfp,tmp,byteN);  break;
    case kTrkNameMdId:   rc = _cmMidiFileReadText(mfp,tmp,byteN);  break;
    case kInstrNameMdId: rc = _cmMidiFileReadText(mfp,tmp,byteN);  break;
    case kLyricsMdId:    rc = _cmMidiFileReadText(mfp,tmp,byteN);  break;
    case kMarkerMdId:    rc = _cmMidiFileReadText(mfp,tmp,byteN);  break;
    case kCuePointMdId:  rc = _cmMidiFileReadText(mfp,tmp,byteN);  break;
    case kMidiChMdId:    rc = _cmMidiFileRead8(mfp,&tmp->u.bVal);  break;
    case kEndOfTrkMdId:  break;
    case kTempoMdId:     rc = _cmMidiFileRead24(mfp,&tmp->u.iVal); break;
    case kSmpteMdId:     rc = _cmMidiFileReadRecd(mfp,tmp,sizeof(cmMidiSmpte_t));   break;
    case kTimeSigMdId:   rc = _cmMidiFileReadRecd(mfp,tmp,sizeof(cmMidiTimeSig_t)); break;
    case kKeySigMdId:    rc = _cmMidiFileReadRecd(mfp,tmp,sizeof(cmMidiKeySig_t));  break;
    case kSeqSpecMdId:   rc = _cmMidiFileReadSysEx(mfp,tmp,byteN); break;

    default:
      cmFileSeek(mfp->fh,kCurFileFl,byteN);
      rc = cmErrMsg(&mfp->err,kUnknownMetaIdMfRC,"Unknown meta status:0x%x %i.",metaId,metaId);
  }

  tmp->metaId = metaId;

  return rc;
}


cmMfRC_t _cmMidiFileReadTrack( _cmMidiFile_t* mfp, unsigned short trkIdx )
{
  cmMfRC_t     rc        = kOkMfRC;
  unsigned     dticks    = 0;
  cmMidiByte_t status;
  cmMidiByte_t runstatus = 0;
  bool         contFl    = true;

  while( contFl && (rc==kOkMfRC))
  { 
    cmMidiTrackMsg_t* tmp = NULL;

    // read the tick count
    if((rc = _cmMidiFileReadVarLen(mfp,&dticks)) != kOkMfRC )
      return rc;

    // read the status byte
    if((rc = _cmMidiFileRead8(mfp,&status)) != kOkMfRC )
      return rc;

    //printf("st:%i 0x%x\n",status,status);

    // append a track msg
    if((rc = _cmMidiFileAppendTrackMsg( mfp, trkIdx, dticks, status, &tmp )) != kOkMfRC )
      return rc;

    // switch on status
    switch( status )
    {
      // handle sys-ex msg
      case kSysExMdId:
        rc = _cmMidiFileReadSysEx(mfp,tmp,cmInvalidCnt);
        break;

        // handle meta msg
      case kMetaStId:
        rc = _cmMidiFileReadMetaMsg(mfp,tmp);

        // ignore unknown meta messages
        if( rc == kUnknownMetaIdMfRC )
          rc = kOkMfRC;

        contFl = tmp->metaId != kEndOfTrkMdId;        
        break;

      default:
        // handle channel msg
        rc = _cmMidiFileReadChannelMsg(mfp,&runstatus,status,tmp);

    }
  }
  return rc;
}


cmMfRC_t _cmMidiFileReadHdr( _cmMidiFile_t* mfp )
{
  cmMfRC_t rc;
  unsigned fileId;
  unsigned chunkByteN;

  // read the file id 
  if((rc = _cmMidiFileRead32(mfp,&fileId)) != kOkMfRC )
    return rc;
  
  // verify the file id
  if( fileId != 'MThd' )
    return cmErrMsg(&mfp->err,kNotAMidiFileMfRC,"");

  // read the file chunk byte count
  if((rc = _cmMidiFileRead32(mfp,&chunkByteN)) != kOkMfRC )
    return  rc;

  // read the format id
  if((rc = _cmMidiFileRead16(mfp,&mfp->fmtId)) != kOkMfRC )
    return rc;

  // read the track count
  if((rc = _cmMidiFileRead16(mfp,&mfp->trkN)) != kOkMfRC )
    return rc;

  // read the ticks per quarter note
  if((rc = _cmMidiFileRead16(mfp,&mfp->ticksPerQN)) != kOkMfRC )
    return rc;

  // if the division field was given in smpte
  if( mfp->ticksPerQN & 0x8000 )
  {
    mfp->smpteFmtId = (mfp->ticksPerQN & 0x7f00) >> 8;
    mfp->smpteTicksPerFrame = (mfp->ticksPerQN & 0xFF);
    mfp->ticksPerQN = 0;
  }

  // allocate and zero the track array
  if( mfp->trkN )
    mfp->trkV = _cmMidiFileMalloc( mfp, sizeof(_cmMidiTrack_t)*mfp->trkN);
  
  return rc;
}

int _cmMidiFileSortFunc( const void *p0, const void* p1 )
{  
  if( (*(cmMidiTrackMsg_t**)p0)->atick == (*(cmMidiTrackMsg_t**)p1)->atick )
    return 0;

  return (*(cmMidiTrackMsg_t**)p0)->atick < (*(cmMidiTrackMsg_t**)p1)->atick ? -1 : 1;  
}

// Set the absolute accumulated ticks (atick) value of each track message.
// The absolute accumulated ticks gives a global time ordering for all
// messages in the file.
void _cmMidiFileSetAccumulateTicks( _cmMidiFile_t* p )
{
  cmMidiTrackMsg_t* nextTrkMsg[ p->trkN ]; // next msg in each track
  unsigned long long atick = 0;
  unsigned          i;
  bool              fl = true;
  
  // iniitalize nextTrkTick[] and nextTrkMsg[].
  for(i=0; i<p->trkN; ++i)
    if((nextTrkMsg[i]  =  p->trkV[i].base) != NULL )
      nextTrkMsg[i]->atick = nextTrkMsg[i]->dtick;

  while(1)
  {
    unsigned k = cmInvalidIdx;

    // find the trk which has the next msg (min atick time)
    for(i=0; i<p->trkN; ++i)
      if( nextTrkMsg[i]!=NULL && (k==cmInvalidIdx || nextTrkMsg[i]->atick < nextTrkMsg[k]->atick) )
        k = i;

    // no next msg was found - we're done
    if( k == cmInvalidIdx )
      break;

    if( fl && nextTrkMsg[k]->dtick > 0 )
    {
      fl = false;
      nextTrkMsg[k]->dtick = 1;
      nextTrkMsg[k]->atick = 1;
    }

    // store the current atick
    atick = nextTrkMsg[k]->atick;

    // advance the selected track to it's next message
    nextTrkMsg[k] = nextTrkMsg[k]->link;

    // set the selected tracks next atick time 
    if( nextTrkMsg[k] != NULL )
      nextTrkMsg[k]->atick = atick + nextTrkMsg[k]->dtick;      
  }  
}

void _cmMidiFileSetAbsoluteTime( _cmMidiFile_t* mfp )
{
  double             microsPerQN   = 60000000/120; // default tempo;
  double             microsPerTick = microsPerQN / mfp->ticksPerQN;
  unsigned long long amicro        = 0;
  unsigned           i;

  for(i=0; i<mfp->msgN; ++i)
  {
    cmMidiTrackMsg_t* mp    = mfp->msgV[i];
    unsigned          dtick = 0;
    
    if( i > 0 )
    {
      // atick must have already been set and sorted
      assert( mp->atick >= mfp->msgV[i-1]->atick );
      dtick = mp->atick -  mfp->msgV[i-1]->atick;
    }

    amicro     += microsPerTick * dtick;
    mp->amicro  = amicro;
    
    
    // track tempo changes
    if( mp->status == kMetaStId && mp->metaId == kTempoMdId )
      microsPerTick = mp->u.iVal / mfp->ticksPerQN;
  }
  
}

cmMfRC_t _cmMidiFileClose( _cmMidiFile_t* mfp )
{
  cmMfRC_t rc = kOkMfRC;

  if( mfp == NULL )
    return rc;

  cmMemPtrFree(&mfp->msgV);

  if( cmFileIsValid( mfp->fh ) )
    if( cmFileClose( &mfp->fh ) != kOkFileRC )
      rc = cmErrMsg(&mfp->err,kFileFailMfRC,"MIDI file close failed.");
    
  if( cmLHeapIsValid( mfp->lhH ) )
    cmLHeapDestroy(&mfp->lhH);

  cmMemPtrFree(&mfp);

  return rc;
  
}

void _cmMidiFileLinearize( _cmMidiFile_t* mfp )
{
  unsigned trkIdx,i,j;
  
  // get the total trk msg count
  mfp->msgN = 0;
  for(trkIdx=0; trkIdx<mfp->trkN; ++trkIdx)
    mfp->msgN += mfp->trkV[ trkIdx ].cnt;

  // allocate the trk msg index vector: msgV[]
  mfp->msgV = cmMemResizeZ(cmMidiTrackMsg_t*, mfp->msgV, mfp->msgN);

  // store a pointer to every trk msg in msgV[]
  for(i=0,j=0; i<mfp->trkN; ++i)
  {
    cmMidiTrackMsg_t* m = mfp->trkV[i].base;
    
    for(; m!=NULL; m=m->link)
    {
      assert( j < mfp->msgN );
      
      mfp->msgV[j++] = m;
    }
  }


  // set the atick value in each msg
  _cmMidiFileSetAccumulateTicks(mfp);
    
  // sort msgV[] in ascending order on atick
  qsort( mfp->msgV, mfp->msgN, sizeof(cmMidiTrackMsg_t*), _cmMidiFileSortFunc );

  // set the amicro value in each msg
  _cmMidiFileSetAbsoluteTime(mfp);
  
}


cmMfRC_t cmMidiFileOpen( cmCtx_t* ctx, cmMidiFileH_t* hPtr, const char* fn )
{
  cmMfRC_t       rc     = kOkMfRC;
  _cmMidiFile_t* mfp    = NULL;
  unsigned short trkIdx = 0;
  cmErr_t        err;

  if( cmMidiFileIsValid(*hPtr) )
    if((rc = _cmMidiFileClose(_cmMidiFileHandleToPtr(*hPtr))) != kOkMfRC )
      return rc;

  cmErrSetup(&err,&ctx->rpt,"MIDI File");

  // allocate the midi file object 
  if(( mfp = cmMemAllocZ( _cmMidiFile_t, 1)) == NULL )
    return rc                                 = cmErrMsg(&err,kMemAllocFailMfRC,"MIDI file memory allocation failed.");

  cmErrClone(&mfp->err,&err);

  // allocate the linked heap
  if( cmLHeapIsValid( mfp->lhH = cmLHeapCreate( 1024, ctx )) == false )
  {
    rc = cmErrMsg(&err,kMemAllocFailMfRC,"MIDI heap allocation failed.");
    goto errLabel;
  }

  // open the file
  if(cmFileOpen(&mfp->fh,fn,kReadFileFl | kBinaryFileFl,mfp->err.rpt) != kOkFileRC )
  {
    rc = cmErrMsg(&mfp->err,kFileFailMfRC,"MIDI file open failed.");
    goto errLabel;
  }

  // read header and setup track array
  if(( rc = _cmMidiFileReadHdr(mfp)) != kOkMfRC )
    goto errLabel;
  
  while( !cmFileEof(mfp->fh) && trkIdx < mfp->trkN )
  {
    unsigned chkId = 0,chkN=0;

    // read the chunk id
    if((rc = _cmMidiFileRead32(mfp,&chkId)) != kOkMfRC )
      goto errLabel;

    // read the chunk size
    if((rc = _cmMidiFileRead32(mfp,&chkN)) != kOkMfRC )
      goto errLabel;

    // if this is not a trk chunk then skip it
    if( chkId != (unsigned)'MTrk')
    {
      //if( fseek( mfp->fp, chkN, SEEK_CUR) != 0 )
      if( cmFileSeek(mfp->fh,kCurFileFl,chkN) != kOkFileRC )
      {
        rc = cmErrMsg(&mfp->err,kFileFailMfRC,"MIDI file seek failed.");
        goto errLabel;
      }
    }  
    else
    {
      if((rc = _cmMidiFileReadTrack(mfp,trkIdx)) != kOkMfRC )
        goto errLabel;

      ++trkIdx;
    }
  }

  // store the file name
  mfp->fn          = _cmMidiFileMalloc(mfp,strlen(fn)+1);
  assert( mfp->fn != NULL );
  strcpy(mfp->fn,fn);
  
  _cmMidiFileLinearize(mfp);
  
  hPtr->h = mfp;

 errLabel:

  if( cmFileClose(&mfp->fh) != kOkFileRC )
    rc = cmErrMsg(&mfp->err,kFileFailMfRC,"MIDI file close failed.");

  if( rc != kOkMfRC )
    _cmMidiFileClose(mfp);

  return rc;
}



cmMfRC_t        cmMidiFileClose( cmMidiFileH_t* h )
{
  cmMfRC_t rc;

  if( cmMidiFileIsNull(*h) )
    return kOkMfRC;

  if((rc = _cmMidiFileClose(_cmMidiFileHandleToPtr(*h))) == kOkMfRC )
    return rc;

  h->h = NULL;
  return rc;
}

cmMfRC_t _cmMidiFileWrite8( _cmMidiFile_t* mfp, unsigned char v )
{
  cmMfRC_t rc = kOkMfRC;

  if( cmFileWriteUChar(mfp->fh,&v,1) != kOkFileRC )
    rc = cmErrMsg(&mfp->err,kFileFailMfRC,"MIDI file byte write failed.");

  return rc;  
}

cmMfRC_t _cmMidiFileWrite16( _cmMidiFile_t* mfp, unsigned short v )
{
  cmMfRC_t rc = kOkMfRC;

  v = mfSwap16(v);

  if( cmFileWriteUShort(mfp->fh,&v,1) != kOkFileRC )
    rc = cmErrMsg(&mfp->err,kFileFailMfRC,"MIDI file short integer write failed.");

  return rc;
}

cmMfRC_t _cmMidiFileWrite24( _cmMidiFile_t* mfp, unsigned v )
{
  cmMfRC_t rc = kOkMfRC;
  unsigned mask = 0xff0000;
  int      i;

  for(i = 2; i>=0; --i)
  {
    unsigned char c = (v & mask) >> (i*8);
    mask >>= 8;

    if( cmFileWriteUChar(mfp->fh,&c,1) != kOkFileRC )
    {
      rc = cmErrMsg(&mfp->err,kFileFailMfRC,"MIDI file 24 bit integer write failed.");
      goto errLabel;
    }
    
  }

 errLabel:
  return rc;
}

cmMfRC_t _cmMidiFileWrite32( _cmMidiFile_t* mfp, unsigned v )
{
  cmMfRC_t rc = kOkMfRC;

  v = mfSwap32(v);

  if( cmFileWriteUInt(mfp->fh,&v,1) != kOkFileRC )
    rc = cmErrMsg(&mfp->err,kFileFailMfRC,"MIDI file integer write failed.");

  return rc;
}

cmMfRC_t _cmMidiFileWriteRecd( _cmMidiFile_t* mfp, const void* v, unsigned byteCnt )
{
  cmMfRC_t rc = kOkMfRC;

  if( cmFileWriteChar(mfp->fh,v,byteCnt) != kOkFileRC )  
    rc = cmErrMsg(&mfp->err,kFileFailMfRC,"MIDI file write record failed.");

  return rc;
}

cmMfRC_t _cmMidiFileWriteVarLen( _cmMidiFile_t* mfp, unsigned v )
{
  cmMfRC_t rc  = kOkMfRC;
  unsigned buf = v & 0x7f;
 
  while((v >>= 7) > 0 )
  {
    buf <<= 8;          
    buf |= 0x80;
    buf += (v & 0x7f);
  }

  while(1)
  {
    unsigned char c = (unsigned char)(buf & 0xff);
    if( cmFileWriteUChar(mfp->fh,&c,1) != kOkFileRC )
    {
      rc = cmErrMsg(&mfp->err,kFileFailMfRC,"MIDI file variable length integer write failed.");
      goto errLabel;
    }

    if( buf & 0x80 )
      buf >>= 8;
    else
      break;
  }

 errLabel:
  return rc;
}

cmMfRC_t _cmMidiFileWriteHdr( _cmMidiFile_t* mfp )
{
  cmMfRC_t rc;
  unsigned fileId = 'MThd';
  unsigned chunkByteN = 6;

  // write the file id ('MThd')
  if((rc = _cmMidiFileWrite32(mfp,fileId)) != kOkMfRC )
    return rc;
  
  // write the file chunk byte count (always 6)
  if((rc = _cmMidiFileWrite32(mfp,chunkByteN)) != kOkMfRC )
    return  rc;

  // write the MIDI file format id (0,1,2)
  if((rc = _cmMidiFileWrite16(mfp,mfp->fmtId)) != kOkMfRC )
    return rc;

  // write the track count
  if((rc = _cmMidiFileWrite16(mfp,mfp->trkN)) != kOkMfRC )
    return rc;

  unsigned short v = 0;

  // if the ticks per quarter note field is valid ...
  if( mfp->ticksPerQN )
    v = mfp->ticksPerQN;
  else
  {
    // ... otherwise the division field was given in smpte
    v = mfp->smpteFmtId << 8;
    v += mfp->smpteTicksPerFrame;    
  }

  if((rc = _cmMidiFileWrite16(mfp,v)) != kOkMfRC )
    return rc;

  return rc;

}


cmMfRC_t _cmMidiFileWriteSysEx( _cmMidiFile_t* mfp, cmMidiTrackMsg_t* tmp )
{
  cmMfRC_t rc = kOkMfRC;

  if((rc = _cmMidiFileWrite8(mfp,kSysExMdId)) != kOkMfRC )
    goto errLabel;

  if( cmFileWriteUChar(mfp->fh,tmp->u.sysExPtr,tmp->byteCnt) != kOkFileRC )
    rc = cmErrMsg(&mfp->err,kFileFailMfRC,"Sys-ex msg write failed.");

 errLabel:
  return rc;
}

cmMfRC_t _cmMidiFileWriteChannelMsg( _cmMidiFile_t* mfp, const cmMidiTrackMsg_t* tmp, cmMidiByte_t* runStatus )
{
  cmMfRC_t     rc    = kOkMfRC;
  unsigned     byteN = cmMidiStatusToByteCount(tmp->status);
  cmMidiByte_t status = tmp->status + tmp->u.chMsgPtr->ch;

  if( status != *runStatus )
  {
    *runStatus = status;
    if((rc = _cmMidiFileWrite8(mfp,status)) != kOkMfRC )
      goto errLabel;
  }

  if(byteN>=1)
    if((rc = _cmMidiFileWrite8(mfp,tmp->u.chMsgPtr->d0)) != kOkMfRC )
      goto errLabel;

  if(byteN>=2)
    if((rc = _cmMidiFileWrite8(mfp,tmp->u.chMsgPtr->d1)) != kOkMfRC )
      goto errLabel;

 errLabel:
  return rc;
}

cmMfRC_t _cmMidiFileWriteMetaMsg( _cmMidiFile_t* mfp, const cmMidiTrackMsg_t* tmp )
{
  cmMfRC_t rc;

  if((rc = _cmMidiFileWrite8(mfp,kMetaStId)) != kOkMfRC )
    return rc;

  if((rc = _cmMidiFileWrite8(mfp,tmp->metaId)) != kOkMfRC )
    return rc;


  switch( tmp->metaId )
  {
    case kSeqNumbMdId:
      if((rc = _cmMidiFileWrite8(mfp,sizeof(tmp->u.sVal))) == kOkMfRC )
        rc                                                  = _cmMidiFileWrite16(mfp,tmp->u.sVal);
      break;

    case kTempoMdId:
      if((rc = _cmMidiFileWrite8(mfp,3)) == kOkMfRC )
        rc = _cmMidiFileWrite24(mfp,tmp->u.iVal); 
      break;

    case kSmpteMdId:
        if((rc = _cmMidiFileWrite8(mfp,sizeof(cmMidiSmpte_t))) == kOkMfRC )
          rc   = _cmMidiFileWriteRecd(mfp,tmp->u.smptePtr,sizeof(cmMidiSmpte_t));
        break;
          
    case kTimeSigMdId:
        if((rc = _cmMidiFileWrite8(mfp,sizeof(cmMidiTimeSig_t))) == kOkMfRC )
          rc   = _cmMidiFileWriteRecd(mfp,tmp->u.timeSigPtr,sizeof(cmMidiTimeSig_t));
        break;

    case kKeySigMdId:
        if((rc = _cmMidiFileWrite8(mfp,sizeof(cmMidiKeySig_t))) == kOkMfRC )
          rc   = _cmMidiFileWriteRecd(mfp,tmp->u.keySigPtr,sizeof(cmMidiKeySig_t));
        break;

    case kSeqSpecMdId:
        if((rc = _cmMidiFileWriteVarLen(mfp,sizeof(tmp->byteCnt))) == kOkMfRC )
          rc   = _cmMidiFileWriteRecd(mfp,tmp->u.sysExPtr,tmp->byteCnt);
        break;

    case kMidiChMdId: 
        if((rc = _cmMidiFileWrite8(mfp,sizeof(tmp->u.bVal))) == kOkMfRC )
          rc  = _cmMidiFileWrite8(mfp,tmp->u.bVal);
        break;

    case kEndOfTrkMdId:  
      rc = _cmMidiFileWrite8(mfp,0);
      break;

    case kTextMdId:      
    case kCopyMdId:      
    case kTrkNameMdId:   
    case kInstrNameMdId: 
    case kLyricsMdId:    
    case kMarkerMdId:    
    case kCuePointMdId:  
      {
        unsigned n = tmp->u.text==NULL ? 0 : strlen(tmp->u.text);
        if((rc     = _cmMidiFileWriteVarLen(mfp,n)) == kOkMfRC && n>0 )
          rc       = _cmMidiFileWriteRecd(mfp,tmp->u.text,n);
      }
      break;

    default:
      {
      // ignore unknown meta messages
      }

  }

  return rc;
}

cmMfRC_t _cmMidiFileWriteTrack( _cmMidiFile_t* mfp, unsigned trkIdx )
{
  cmMfRC_t          rc        = kOkMfRC;
  cmMidiTrackMsg_t* tmp       = mfp->trkV[trkIdx].base;
  cmMidiByte_t      runStatus = 0;

  for(; tmp != NULL; tmp=tmp->link)
  {
    // write the msg tick count
    if((rc = _cmMidiFileWriteVarLen(mfp,tmp->dtick)) != kOkMfRC )
      return rc;

    // switch on status
    switch( tmp->status )
    {
      // handle sys-ex msg
      case kSysExMdId:
        rc = _cmMidiFileWriteSysEx(mfp,tmp);
        break;

        // handle meta msg
      case kMetaStId:
        rc = _cmMidiFileWriteMetaMsg(mfp,tmp);
        break;

      default:
        // handle channel msg
        rc = _cmMidiFileWriteChannelMsg(mfp,tmp,&runStatus);

    }

  }

  return rc;
}

cmMfRC_t   cmMidiFileWrite( cmMidiFileH_t h, const char* fn )
{
  cmMfRC_t       rc  = kOkMfRC;
  _cmMidiFile_t* mfp = _cmMidiFileHandleToPtr(h);
  unsigned       i;

  // create the output file
  if( cmFileOpen(&mfp->fh,fn,kWriteFileFl,mfp->err.rpt) != kOkFileRC )
    return cmErrMsg(&mfp->err,kFileFailMfRC,"The MIDI file '%s' could not be created.",cmStringNullGuard(fn));

  // write the file header
  if((rc = _cmMidiFileWriteHdr(mfp)) != kOkMfRC )
  {
    rc = cmErrMsg(&mfp->err,rc,"The file header write failed on the MIDI file '%s'.",cmStringNullGuard(fn));
    goto errLabel;
  }

  for(i=0; i < mfp->trkN; ++i )
  {
    unsigned chkId = 'MTrk';
    long     offs0,offs1;

    // write the track chunk id ('MTrk')
    if((rc = _cmMidiFileWrite32(mfp,chkId)) != kOkMfRC )
      goto errLabel;

    cmFileTell(mfp->fh,&offs0);

    // write the track chunk size as zero
    if((rc = _cmMidiFileWrite32(mfp,0)) != kOkMfRC )
      goto errLabel;

    if((rc = _cmMidiFileWriteTrack(mfp,i)) != kOkMfRC )
      goto errLabel;

    cmFileTell(mfp->fh,&offs1);

    cmFileSeek(mfp->fh,kBeginFileFl,offs0);

    _cmMidiFileWrite32(mfp,offs1-offs0-4);

    cmFileSeek(mfp->fh,kBeginFileFl,offs1);
    
  }

 errLabel:
  cmFileClose(&mfp->fh);
  return rc;
}


bool   cmMidiFileIsValid( cmMidiFileH_t h )
{ return !cmMidiFileIsNull(h); }

unsigned    cmMidiFileTrackCount( cmMidiFileH_t h )
{
  _cmMidiFile_t* mfp;

  if((mfp = _cmMidiFileHandleToPtr(h)) == NULL )
    return cmInvalidCnt;

  return mfp->trkN;
}

unsigned    cmMidiFileType( cmMidiFileH_t h )
{
  _cmMidiFile_t* mfp;

  if((mfp = _cmMidiFileHandleToPtr(h)) == NULL )
    return cmInvalidId;

  return mfp->fmtId;
}

const char*            cmMidiFileName( cmMidiFileH_t h )
{
  _cmMidiFile_t* mfp;
  if((mfp = _cmMidiFileHandleToPtr(h)) == NULL )
    return NULL;
  return mfp->fn;
}

unsigned    cmMidiFileTicksPerQN( cmMidiFileH_t h )
{
  _cmMidiFile_t* mfp;

  if((mfp = _cmMidiFileHandleToPtr(h)) == NULL )
    return cmInvalidCnt;

  return mfp->ticksPerQN;
}

cmMidiByte_t  cmMidiFileTicksPerSmpteFrame( cmMidiFileH_t h )
{
  _cmMidiFile_t* mfp;

  if((mfp = _cmMidiFileHandleToPtr(h)) == NULL )
    return kInvalidMidiByte;

  if( mfp->ticksPerQN != 0 )
    return 0;
 
  return mfp->smpteTicksPerFrame;
} 

cmMidiByte_t  cmMidiFileSmpteFormatId( cmMidiFileH_t h )
{ 
  _cmMidiFile_t* mfp;

  if((mfp = _cmMidiFileHandleToPtr(h)) == NULL )
    return kInvalidMidiByte;

  if( mfp->ticksPerQN != 0 )
    return 0;
 
  return mfp->smpteFmtId;
}
    
unsigned    cmMidiFileTrackMsgCount( cmMidiFileH_t h, unsigned trackIdx )
{
  _cmMidiFile_t* mfp;

  if((mfp = _cmMidiFileHandleToPtr(h)) == NULL )
    return cmInvalidCnt;

  return mfp->trkV[trackIdx].cnt;
}


const cmMidiTrackMsg_t* cmMidiFileTrackMsg( cmMidiFileH_t h, unsigned trackIdx )
{
  _cmMidiFile_t* mfp;

  if((mfp = _cmMidiFileHandleToPtr(h)) == NULL )
    return NULL;

  return mfp->trkV[trackIdx].base;
}

unsigned              cmMidiFileMsgCount( cmMidiFileH_t h )
{
  _cmMidiFile_t* mfp;

  if((mfp = _cmMidiFileHandleToPtr(h)) == NULL )
    return cmInvalidCnt;

  return mfp->msgN;
}

const cmMidiTrackMsg_t** cmMidiFileMsgArray(    cmMidiFileH_t h )
{
  _cmMidiFile_t* mfp;

  if((mfp = _cmMidiFileHandleToPtr(h)) == NULL )
    return NULL;

  // this cast is needed to eliminate an apparently needless 'incompatible type' warning
  return (const cmMidiTrackMsg_t**)mfp->msgV;
}


cmMidiTrackMsg_t*  _cmMidiFileUidToMsg( _cmMidiFile_t* mfp, unsigned uid )
{
  unsigned i;

  for(i=0; i<mfp->msgN; ++i)
    if( mfp->msgV[i]->uid == uid )
      return mfp->msgV[i];

  return NULL;
}

cmMfRC_t cmMidiFileSetVelocity( cmMidiFileH_t h, unsigned uid, cmMidiByte_t vel )
{
  cmMidiTrackMsg_t* r;
  _cmMidiFile_t*    mfp = _cmMidiFileHandleToPtr(h);

  assert( mfp != NULL );

  if((r = _cmMidiFileUidToMsg(mfp,uid)) == NULL )
    return cmErrMsg(&mfp->err,kUidNotFoundMfRC,"The MIDI file uid %i could not be found.",uid);

  if( cmMidiIsNoteOn(r->status) == false && cmMidiIsNoteOff(r->status,0)==false )
    return cmErrMsg(&mfp->err,kUidNotANoteMsgMfRC,"Cannot set velocity on a non-Note-On/Off msg.");
  
  cmMidiChMsg_t* chm = (cmMidiChMsg_t*)r->u.chMsgPtr;

  chm->d1 = vel;

  return kOkMfRC;
}

// Returns NULL if uid is not found or if it the first msg on the track.
cmMidiTrackMsg_t*  _cmMidiFileMsgBeforeUid( _cmMidiFile_t* p, unsigned uid )
{
  cmMidiTrackMsg_t* m;
  
  if((m = _cmMidiFileUidToMsg(p,uid)) == NULL )
    return NULL;

  assert( m->trkIdx < p->trkN );

  cmMidiTrackMsg_t* m0 = NULL;
  cmMidiTrackMsg_t* m1 = p->trkV[ m->trkIdx ].base;
  for(; m1!=NULL; m1 = m1->link)
  {
    if( m1->uid == uid )
      break;
    m0 = m1;
  }

  return m0;
}

unsigned _cmMidiFileIsMsgFirstOnTrack( _cmMidiFile_t* p, unsigned uid )
{
  unsigned i;
  for(i=0; i<p->trkN; ++i)
    if( p->trkV[i].base!=NULL && p->trkV[i].base->uid == uid )
      return i;
  
  return cmInvalidIdx;   
}


cmMfRC_t cmMidiFileInsertMsg( cmMidiFileH_t h, unsigned uid, int dtick, cmMidiByte_t ch, cmMidiByte_t status, cmMidiByte_t d0, cmMidiByte_t d1 )
{
  _cmMidiFile_t*    mfp    = _cmMidiFileHandleToPtr(h);
  assert( mfp != NULL );
  cmMidiTrackMsg_t* ref    = NULL;
  unsigned          trkIdx = cmInvalidIdx;

  // if dtick is positive ...
  if( dtick >= 0 )
  {
    ref    = _cmMidiFileUidToMsg(mfp,uid); // ... then get the ref. msg.
    trkIdx = ref->trkIdx;
  }
  else // if dtick is negative ...
  {
    // ... get get the msg before the ref. msg.
    if((ref = _cmMidiFileMsgBeforeUid(mfp,uid)) != NULL )
      trkIdx = ref->trkIdx;
    else
    {
      // ... the ref. msg was first in the track so there is no msg before it
      trkIdx = _cmMidiFileIsMsgFirstOnTrack(mfp,uid);
    }
  }

  // verify that the reference msg was found
  if( trkIdx == cmInvalidIdx )
    return cmErrMsg(&mfp->err,kUidNotFoundMfRC,"The UID (%i) reference note could not be located.",uid);

  assert( trkIdx < mfp->trkN );

  // complete the msg setup
  _cmMidiTrack_t* trk   = mfp->trkV + trkIdx;
  cmMidiTrackMsg_t* m   = _cmMidiFileAllocMsg(mfp, trkIdx, abs(dtick), status );
  cmMidiChMsg_t*    c   = (cmMidiChMsg_t*)_cmMidiFileMalloc(mfp,sizeof(cmMidiChMsg_t));

  m->u.chMsgPtr = c;
  
  c->ch   = ch;
  c->d0   = d0;
  c->d1   = d1;

  // if 'm' is prior to the first msg in the track
  if( ref == NULL )
  {
    // ... then make 'm' the first msg in the first msg
    m->link = trk->base;
    trk->base = m;
    // 'm' is before ref and the track cannot be empty (because ref is in it) 'm'
    // can never be the last msg in the list
  } 
  else // ref is the msg before 'm'
  {
    m->link   = ref->link;
    ref->link = m;
    
    // if ref was the last msg in the trk ...
    if( trk->last == ref )
      trk->last = m;  //... then 'm' is now the last msg in the trk
  }

  trk->cnt += 1;

  _cmMidiFileLinearize(mfp);

  return kOkMfRC;

}

unsigned  cmMidiFileSeekUsecs( cmMidiFileH_t h, unsigned long long offsUSecs, unsigned* msgUsecsPtr, unsigned* microsPerTickPtr )
{
  _cmMidiFile_t* p;

  if((p = _cmMidiFileHandleToPtr(h)) == NULL )
    return cmInvalidIdx;

  if( p->msgN == 0 )
    return cmInvalidIdx;

  unsigned mi;
  double microsPerQN   = 60000000.0/120.0;
  double microsPerTick = microsPerQN / p->ticksPerQN;
  double accUSecs      = 0;

  for(mi=0; mi<p->msgN; ++mi)
  {
    const cmMidiTrackMsg_t* mp = p->msgV[mi];

    if( mp->amicro >= offsUSecs )
      break;
  }
  
  if( mi == p->msgN )
    return cmInvalidIdx;

  if( msgUsecsPtr != NULL )
    *msgUsecsPtr = round(accUSecs - offsUSecs);

  if( microsPerTickPtr != NULL )
    *microsPerTickPtr = round(microsPerTick);

  return mi;
}

double  cmMidiFileDurSecs( cmMidiFileH_t h )
{
  _cmMidiFile_t* mfp           = _cmMidiFileHandleToPtr(h);

  if( mfp->msgN == 0 )
    return 0;
  
  return mfp->msgV[ mfp->msgN-1 ]->amicro / 1000000.0;
}

typedef struct _cmMidiVoice_str
{
  const  cmMidiTrackMsg_t*  mp;
  unsigned                  durMicros;
  bool                      sustainFl;
  struct _cmMidiVoice_str*  link;
} _cmMidiVoice_t;


void _cmMidiFileSetDur( cmMidiTrackMsg_t* m0, cmMidiTrackMsg_t* m1 )
{
  // calculate the duration of the sounding note
  ((cmMidiChMsg_t*)m0->u.chMsgPtr)->durMicros = m1->amicro - m0->amicro;

  // set the note-off msg pointer
  ((cmMidiChMsg_t*)m0->u.chMsgPtr)->end       = m1;
}

bool _cmMidiFileCalcNoteDur( cmMidiTrackMsg_t* m0, cmMidiTrackMsg_t* m1, int noteGateFl, int sustainGateFl, bool sostGateFl )
{
  // if the note is being kept sounding because the key is still depressed,
  //    the sustain pedal is down or it is being held by the sostenuto pedal ....
  if( noteGateFl>0 || sustainGateFl>0 || sostGateFl )
    return false;  // ... do nothing

  _cmMidiFileSetDur(m0,m1);
  
  return true;
}

void cmMidiFileCalcNoteDurations( cmMidiFileH_t h )
{
  _cmMidiFile_t* p;

  if((p = _cmMidiFileHandleToPtr(h)) == NULL )
    return;

  if( p->msgN == 0 )
    return;

  unsigned          mi = cmInvalidId;
  cmMidiTrackMsg_t* noteM[     kMidiNoteCnt * kMidiChCnt ];  // ptr to note-on or NULL if the note is not sounding
  cmMidiTrackMsg_t* sustV[                    kMidiChCnt ];
  cmMidiTrackMsg_t* sostV[                    kMidiChCnt ];
  int               noteGateM[ kMidiNoteCnt * kMidiChCnt ];  // true if the associated note key is depressed
  bool              sostGateM[ kMidiNoteCnt * kMidiChCnt ];  // true if the associated note was active when the sost. pedal went down
  int               sustGateV[ kMidiChCnt];                  // true if the associated sustain pedal is down
  int               sostGateV[ kMidiChCnt];                  // true if the associated sostenuto pedal is down
  unsigned          i,j;

  // initialize the state tracking variables
  for(i=0; i<kMidiChCnt; ++i)
  {
    sustV[i]     = NULL;
    sustGateV[i] = 0;
    
    sostV[i]     = NULL;
    sostGateV[i] = 0;
    
    for(j=0; j<kMidiNoteCnt; ++j)
    {
      noteM[     i*kMidiNoteCnt + j ] = NULL;
      noteGateM[ i*kMidiNoteCnt + j ] = 0;
      sostGateM[ i*kMidiNoteCnt + j ] = false;
    }
  }

  // for each midi event
  for(mi=0; mi<p->msgN; ++mi)
  {
    cmMidiTrackMsg_t* m = p->msgV[mi];

    // verify that time is also incrementing
    assert(  mi==0 || (mi>0 &&  m->amicro >= p->msgV[mi-1]->amicro) );

    // ignore all non-channel messages
    if(  !cmMidiIsChStatus( m->status ) )
      continue;

    cmMidiByte_t ch = m->u.chMsgPtr->ch; // get the midi msg channel
    cmMidiByte_t d0 = m->u.chMsgPtr->d0; // get the midi msg data value 

    // if this is a note-on msg
    if( cmMidiFileIsNoteOn(m) )
    {
      unsigned  k = ch*kMidiNoteCnt + d0;

      // there should be no existing sounding note instance for this pitch
      //if( noteGateM[k] == 0 && noteM[k] != NULL )
      //  cmErrWarnMsg(&p->err,kMissingNoteOffMfRC,"%i : Missing note-off instance for note on:%s",m->uid,cmMidiToSciPitch(d0,NULL,0));

      if( noteM[k] != NULL )
        noteGateM[k] += 1;
      else
      {
        noteM[k]     = m;
        noteGateM[k] = 1;
      }
    }
    else
      
      // if this is a note-off msg
      if( cmMidiFileIsNoteOff(m) )
      {
        unsigned            k = ch*kMidiNoteCnt + d0;
        cmMidiTrackMsg_t*  m0 = noteM[k];

        if( m0 == NULL )
          cmErrWarnMsg(&p->err,kMissingNoteOffMfRC,"%i : Missing note-on instance for note-off:%s",m->uid,cmMidiToSciPitch(d0,NULL,0));
        else
        {
          // a key was released - so it should not already be up
          if( noteGateM[k]==0 )
            cmErrWarnMsg(&p->err,kMissingNoteOffMfRC,"%i : Missing note-on for note-off:%s",m->uid,cmMidiToSciPitch(d0,NULL,0));
          else
          {
            noteGateM[k] -= 1; // update the note gate state

            // update the sounding note status
            if( _cmMidiFileCalcNoteDur(m0, m, noteGateM[k], sustGateV[ch], sostGateM[k]) )
              noteM[k] = NULL;
          }
        }
      }
      else
        
        // This is a sustain-pedal down msg
        if( cmMidiFileIsSustainPedalDown(m) )
        {
          // if the sustain channel is already down
          //if( sustGateV[ch] )
          //  cmErrWarnMsg(&p->err,kSustainPedalMfRC,"%i : The sustain pedal went down twice with no intervening release.",m->uid);

          sustGateV[ch] += 1;

          if( sustV[ch] == NULL )
            sustV[ch] = m;
          
        }
        else

          // This is a sustain-pedal up msg
          if( cmMidiFileIsSustainPedalUp(m) )
          {
            // if the sustain channel is already up
            if( sustGateV[ch]==0 )
              cmErrWarnMsg(&p->err,kSustainPedalMfRC,"%i : The sustain pedal release message was received with no previous pedal down.",m->uid);

            if( sustGateV[ch] >= 1 )
            {
              sustGateV[ch] -= 1;

              if( sustGateV[ch] == 0 )
              {
                unsigned k = ch*kMidiNoteCnt;
                
                // for each sounding note on this channel
                for(; k<ch*kMidiNoteCnt+kMidiNoteCnt; ++k)
                  if( noteM[k]!=NULL && _cmMidiFileCalcNoteDur(noteM[k], m, noteGateM[k], sustGateV[ch], sostGateM[k]) )
                    noteM[k] = NULL;

                if( sustV[ch] != NULL )
                {
                  _cmMidiFileSetDur(sustV[ch],m);
                  ((cmMidiChMsg_t*)sustV[ch]->u.chMsgPtr)->end = m; // set the pedal-up msg ptr. in the pedal-down msg.
                  sustV[ch] = NULL;
                }
              }
            }
          }
          else

            // This is a sostenuto pedal-down msg
            if( cmMidiFileIsSostenutoPedalDown(m) )
            {
              // if the sustain channel is already down
              //if( sostGateV[ch] )
              //  cmErrWarnMsg(&p->err,kSostenutoPedalMfRC,"%i : The sostenuto pedal went down twice with no intervening release.",m->uid);

              // record the notes that are active when the sostenuto pedal went down
              unsigned k = ch * kMidiNoteCnt;
              for(i=0; i<kMidiNoteCnt; ++i)
                sostGateM[k+i] = noteGateM[k+i] > 0;
              
              sostGateV[ch] += 1;          
            }
            else

              // This is a sostenuto pedal-up msg
              if( cmMidiFileIsSostenutoPedalUp(m) )
              {
                // if the sustain channel is already up
                if( sostGateV[ch]==0 )
                  cmErrWarnMsg(&p->err,kSostenutoPedalMfRC,"%i : The sostenuto pedal release message was received with no previous pedal down.",m->uid);

                if( sostGateV[ch] >= 1 )
                {
                  sostGateV[ch] -= 1;

                  if( sostGateV[ch] == 0 )
                  {
                    unsigned k = ch*kMidiNoteCnt;
                    
                    // for each note on this channel
                    for(; k<ch*kMidiNoteCnt+kMidiNoteCnt; ++k)
                    {
                      sostGateM[k] = false;
                      
                      if( noteM[k]!=NULL && _cmMidiFileCalcNoteDur(noteM[k], m, noteGateM[k], sustGateV[ch], sostGateM[k]) )
                        noteM[k] = NULL;
                    }
                    
                    if( sostV[ch] != NULL )
                    {
                      _cmMidiFileSetDur(sostV[ch],m);                      
                      ((cmMidiChMsg_t*)sostV[ch]->u.chMsgPtr)->end = m; // set the pedal-up msg ptr. in the pedal-down msg.
                      sostV[ch] = NULL;
                    }

                  }
                }
              }
    
  } // for each midi file event
}

void cmMidiFileSetDelay( cmMidiFileH_t h, unsigned ticks )
{
  _cmMidiFile_t* p;
  unsigned mi;

  if((p = _cmMidiFileHandleToPtr(h)) == NULL )
    return;

  if( p->msgN == 0 )
    return;

  for(mi=0; mi<p->msgN; ++mi)
  {
    cmMidiTrackMsg_t* mp = p->msgV[mi];
    
    // locate the first msg which has a non-zero delta tick
    if( mp->dtick > 0 )
    {
      mp->dtick = ticks;
      break;
    }
  }
}

unsigned              cmMidiFilePackTrackMsgBufByteCount( const cmMidiTrackMsg_t* m )
{ return sizeof(cmMidiTrackMsg_t) + m->byteCnt;  }

cmMidiTrackMsg_t*     cmMidiFilePackTrackMsg( const cmMidiTrackMsg_t* m, void* buf, unsigned bufByteCnt )
{
  unsigned          n   = sizeof(cmMidiTrackMsg_t) + m->byteCnt;

  if( n < bufByteCnt )
  {
    assert(0);
    return NULL;
  }

  // copy the cmMidiTrackMsg_t into the buffer
  memcpy(buf, m, sizeof(cmMidiTrackMsg_t));

  if( m->byteCnt > 0 )
  {
    // copy any linked data into the buffer    
    memcpy(buf + sizeof(cmMidiTrackMsg_t), m->u.voidPtr, m->byteCnt );
  
    // fixup the linked data ptr  
    cmMidiTrackMsg_t* mp = (cmMidiTrackMsg_t*)buf;
    mp->u.voidPtr = buf + sizeof(cmMidiTrackMsg_t);
  }
  return (cmMidiTrackMsg_t*)buf;
}

void _cmMidiFilePrintHdr( const _cmMidiFile_t* mfp, cmRpt_t* rpt )
{
  if( mfp->fn != NULL )
    cmRptPrintf(rpt,"%s ",mfp->fn);

  cmRptPrintf(rpt,"fmt:%i ticksPerQN:%i tracks:%i\n",mfp->fmtId,mfp->ticksPerQN,mfp->trkN);

  cmRptPrintf(rpt," UID     dtick     atick      amicro     type  ch  D0  D1\n");
  cmRptPrintf(rpt,"----- ---------- ---------- ---------- : ---- --- --- ---\n");
  
}

void _cmMidiFilePrintMsg( cmRpt_t* rpt, const cmMidiTrackMsg_t* tmp )
{
  cmRptPrintf(rpt,"%5i %10u %10llu %10llu : ",
    tmp->uid,
    tmp->dtick,
    tmp->atick,
    tmp->amicro );

  if( tmp->status == kMetaStId )
  {
    cmRptPrintf(rpt,"%s ", cmMidiMetaStatusToLabel(tmp->metaId));
  }
  else
  {
    cmRptPrintf(rpt,"%4s %3i %3i %3i",
      cmMidiStatusToLabel(tmp->status),
      tmp->u.chMsgPtr->ch,
      tmp->u.chMsgPtr->d0,
      tmp->u.chMsgPtr->d1);    
  }

  if( cmMidiIsChStatus(tmp->status) && cmMidiIsNoteOn(tmp->status) && (tmp->u.chMsgPtr->d1>0) )
    cmRptPrintf(rpt," %4s ",cmMidiToSciPitch(tmp->u.chMsgPtr->d0,NULL,0));
    
  
  cmRptPrintf(rpt,"\n");
}

void cmMidiFilePrintMsgs( cmMidiFileH_t h, cmRpt_t* rpt )
{
  const _cmMidiFile_t* p = _cmMidiFileHandleToPtr(h);
  unsigned mi;
  
  _cmMidiFilePrintHdr(p,rpt);

  for(mi=0; mi<p->msgN; ++mi)
  {
    cmMidiTrackMsg_t* mp = p->msgV[mi];

    if( mp != NULL )
      _cmMidiFilePrintMsg(rpt,mp);
  }
  
}

void cmMidiFilePrintTracks( cmMidiFileH_t h, unsigned trkIdx, cmRpt_t* rpt )
{
  const _cmMidiFile_t* mfp = _cmMidiFileHandleToPtr(h);

  _cmMidiFilePrintHdr(mfp,rpt);

  int i = trkIdx == cmInvalidIdx ? 0         : trkIdx;
  int n = trkIdx == cmInvalidIdx ? mfp->trkN : trkIdx+1;

  for(; i<n; ++i)
  {
    cmRptPrintf(rpt,"Track:%i\n",i);
    
    cmMidiTrackMsg_t* tmp = mfp->trkV[i].base;
    while( tmp != NULL )
    {
      _cmMidiFilePrintMsg(rpt,tmp);
      tmp = tmp->link;
    }
  }  
}

bool cmMidiFileIsNull( cmMidiFileH_t h )
{ return (_cmMidiFile_t*)h.h == NULL; }


void cmMidiFileTestPrint( void* printDataPtr, const char* fmt, va_list vl )
{ vprintf(fmt,vl); }

cmMfRC_t cmMidiFileGenPlotFile( cmCtx_t* ctx, const cmChar_t* midiFn, const cmChar_t* outFn )
{
  cmMfRC_t                 rc  = kOkMfRC;
  cmMidiFileH_t            mfH = cmMidiFileNullHandle;  
  cmFileH_t                fH  = cmFileNullHandle;
  unsigned                 i   = 0;
  const cmMidiTrackMsg_t** m   = NULL;
  unsigned                 mN  = 0;

  if((rc = cmMidiFileOpen(ctx, &mfH, midiFn )) != kOkMfRC )
    return cmErrMsg(&ctx->err,rc,"The MIDI file object could not be opened from '%s'.",cmStringNullGuard(midiFn));

  _cmMidiFile_t* p = _cmMidiFileHandleToPtr(mfH);
  
  if( (m = cmMidiFileMsgArray(mfH)) == NULL || (mN = cmMidiFileMsgCount(mfH)) == 0 )
  {
    rc = cmErrMsg(&p->err,kFileFailMfRC,"The MIDI file object appears to be empty.");
    goto errLabel;
  }
  
  cmMidiFileCalcNoteDurations( mfH );
  
  if( cmFileOpen(&fH,outFn,kWriteFileFl,p->err.rpt) != kOkFileRC )
    return cmErrMsg(&p->err,kFileFailMfRC,"Unable to create the file '%s'.",cmStringNullGuard(outFn));

  for(i=0; i<mN; ++i)
    if( (m[i]!=NULL) && cmMidiIsChStatus(m[i]->status) && cmMidiIsNoteOn(m[i]->status) && (m[i]->u.chMsgPtr->d1>0) )
      cmFilePrintf(fH,"n %f %f %i %s\n",m[i]->amicro/1000000.0,m[i]->u.chMsgPtr->durMicros/1000000.0,m[i]->uid,cmMidiToSciPitch(m[i]->u.chMsgPtr->d0,NULL,0));

 errLabel:
  
  cmMidiFileClose(&mfH);
  cmFileClose(&fH);
  return rc;
}

void cmMidiFilePrintControlNumbers( cmCtx_t* ctx, const char* fn )
{
  cmMidiFileH_t h = cmMidiFileNullHandle;
  cmMfRC_t rc;

  if((rc = cmMidiFileOpen(ctx, &h, fn )) != kOkMfRC )
  {
    cmErrMsg(&ctx->err,rc,"MIDI file open failed on '%s'.",fn);
    goto errLabel;
  }

  const cmMidiTrackMsg_t** mm;
  unsigned n = cmMidiFileMsgCount(h);
  if((mm = cmMidiFileMsgArray(h)) != NULL )
  {
    unsigned j;
    for(j=0; j<n; ++j)
    {
      const cmMidiTrackMsg_t* m = mm[j];
      
      if(  m->status == kCtlMdId && m->u.chMsgPtr->d0==66 )
        printf("%i %i\n",m->u.chMsgPtr->d0,m->u.chMsgPtr->d1);
    }
  }

 errLabel:
  cmMidiFileClose(&h);

}

void cmMidiFileTest( const char* fn, cmCtx_t* ctx )
{
  cmMfRC_t      rc;
  cmMidiFileH_t h = cmMidiFileNullHandle;

  if((rc = cmMidiFileOpen(ctx,&h,fn)) != kOkMfRC )
  {
    printf("Error:%i Unable to open the cmMidi file: %s\n",rc,fn);
    return;
  }

  cmMidiFileCalcNoteDurations(  h );

  if( 1 )
  {
    //cmMidiFileTickToMicros( h );
    //cmMidiFileTickToSamples(h,96000,false);
    cmMidiFilePrintMsgs(h,&ctx->rpt);
  }

  if( 0 )
  {
    //cmMidiFilePrint(h,cmMidiFileTrackCount(h)-1,&ctx->rpt);
    //cmMidiFilePrint(h,cmInvalidIdx,&ctx->rpt);
    cmMidiFilePrintControlNumbers(ctx, fn );

  }
  if( 0 )
  {
    printf("Tracks:%i\n",cmMidiFileTrackCount(h));

    unsigned i = 0;
    for(i=0; i<cmMidiFileMsgCount(h); ++i)
    {
      cmMidiTrackMsg_t* tmp = (cmMidiTrackMsg_t*)cmMidiFileMsgArray(h)[i];
      
      if( tmp->status==kMetaStId && tmp->metaId == kTempoMdId )
      {
        double bpm = 60000000.0/tmp->u.iVal;
        printf("Tempo:%i %f\n",tmp->u.iVal,bpm);

        tmp->u.iVal = floor( 60000000.0/69.0 );

        break;
      }
    }

    cmMidiFileWrite(h,"/home/kevin/temp/test0.mid");
  }

  cmMidiFileClose(&h);
}
