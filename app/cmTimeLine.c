#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmJson.h"
#include "cmText.h"
#include "cmAudioFile.h"
#include "cmTime.h"
#include "cmMidi.h"
#include "cmMidiFile.h"
#include "cmFileSys.h"
#include "cmTimeLine.h"
#include "cmOnset.h"

// id's used to track the type of a serialized object
enum
{
  kMsgTlId,
  kObjTlId
};


// 
typedef struct _cmTlMsg_str
{
  unsigned          typeId; // always set to kMsgTlId
  cmTlUiMsgTypeId_t msgId;  //
  double            srate;
  unsigned          seqCnt;
  unsigned          seqId;
} _cmTlMsg_t;


typedef struct _cmTlObj_str
{
  void*                mem;
  unsigned             memByteCnt;
  cmTlObj_t*           obj;   
  struct _cmTlObj_str* prev;
  struct _cmTlObj_str* next;
} _cmTlObj_t;

typedef struct 
{
  _cmTlObj_t*          first;
  _cmTlObj_t*          last;
} _cmTlSeq_t;

typedef struct
{
  cmErr_t         err;
  cmCtx_t         ctx;
  cmLHeapH_t      lH;
  double          srate;
  unsigned        nextSeqId;
  cmTlCb_t        cbFunc;
  void*           cbArg;
  cmChar_t*       prefixPath;
  unsigned        nextUId;
  char*           tmpBuf;  
  unsigned        seqCnt;
  _cmTlSeq_t*     seq;          // seq[seqCnt]
  const cmChar_t* filename;
  cmOnsetCfg_t    onsetCfg;
} _cmTl_t;

typedef struct
{
  char label[8];
  unsigned id;
} _cmTlId_t;

_cmTlId_t _cmTlIdArray[] =
{
  { "mf", kMidiFileTlId  },
  { "me", kMidiEvtTlId   },
  { "af", kAudioFileTlId },
  { "ae", kAudioEvtTlId  },
  { "mk", kMarkerTlId    },
  { "",   cmInvalidId    }
};

cmTlH_t cmTimeLineNullHandle = cmSTATIC_NULL_HANDLE;

_cmTl_t* _cmTlHandleToPtr( cmTlH_t h )
{
  _cmTl_t* p = (_cmTl_t*)h.h;
  assert( p != NULL );
  return p;
}

_cmTlId_t* _cmTlIdLabelToRecd( _cmTl_t* p, const cmChar_t* label )
{
  unsigned i;
  if( label != NULL )
    for(i=0; _cmTlIdArray[i].id != cmInvalidId; ++i)
      if( strcmp(_cmTlIdArray[i].label,label) == 0 )
        return _cmTlIdArray + i;
  return NULL;
}

_cmTlId_t* _cmTlIdToRecd( _cmTl_t* p, unsigned id )
{
  unsigned i;
  for(i=0; _cmTlIdArray[i].id != cmInvalidId; ++i)
    if( _cmTlIdArray[i].id == id )
      return _cmTlIdArray + i;
  return NULL;
}

const cmChar_t* _cmTlIdToLabel( _cmTl_t* p, unsigned id )
{
  _cmTlId_t* rp;
  if((rp = _cmTlIdToRecd(p,id)) != NULL )
    return rp->label;
  return "";
}

unsigned _cmTlIdLabelToId( _cmTl_t* p, const cmChar_t* label )
{
  _cmTlId_t* rp;
  if((rp = _cmTlIdLabelToRecd(p,label)) != NULL )
    return rp->id;
  return cmInvalidId;
}

// cast a generic object to a midi file object
cmTlMidiFile_t*  _cmTlMidiFileObjPtr(  _cmTl_t* p, cmTlObj_t* op, bool errFl )
{ 
  if( op==NULL || op->typeId != kMidiFileTlId )
  {
    if( errFl && p != NULL)
      cmErrMsg(&p->err,kTypeCvtFailTlRC,"A time line object type promotion failed.");
    return NULL;
  }

  return (cmTlMidiFile_t*)op;
}

// cast a generic object to a midi event object
cmTlMidiEvt_t*   _cmTlMidiEvtObjPtr(   _cmTl_t* p, cmTlObj_t* op, bool errFl )
{ 
  if( op==NULL || op->typeId != kMidiEvtTlId )
  {
    if( errFl && p != NULL )
      cmErrMsg(&p->err,kTypeCvtFailTlRC,"A time line object type promotion failed.");
    return NULL;
  }

  return (cmTlMidiEvt_t*)op;
}

// case a generic object to an audio file object
cmTlAudioFile_t* _cmTlAudioFileObjPtr( _cmTl_t* p, cmTlObj_t* op, bool errFl )
{ 
  if( op==NULL || op->typeId != kAudioFileTlId )
  {
    if( errFl && p != NULL)
      cmErrMsg(&p->err,kTypeCvtFailTlRC,"A time line object type promotion failed.");
    return NULL;
  }

  return (cmTlAudioFile_t*)op;
}

// cast a generic object an audio event object to 
cmTlAudioEvt_t*  _cmTlAudioEvtObjPtr(  _cmTl_t* p, cmTlObj_t* op, bool errFl )
{ 
  if( op==NULL || op->typeId != kAudioEvtTlId )
  {
    if( errFl && p != NULL)
      cmErrMsg(&p->err,kTypeCvtFailTlRC,"A time line object type promotion failed.");
    return NULL;
  }
  return (cmTlAudioEvt_t*)op;
}


// cast a generic object to a marker object
cmTlMarker_t*    _cmTlMarkerObjPtr(    _cmTl_t* p, cmTlObj_t* op, bool errFl )
{ 
  if( op==NULL || op->typeId != kMarkerTlId )
  {
    if( errFl && p != NULL)
      cmErrMsg(&p->err,kTypeCvtFailTlRC,"A time line object type promotion failed.");
    return NULL;
  }
  return (cmTlMarker_t*)op;
}

cmTlMidiFile_t*  _cmTimeLineMidiFileObjPtr(  _cmTl_t* p, cmTlObj_t* op )
{ return _cmTlMidiFileObjPtr(p,op,true);  }

cmTlMidiEvt_t*   _cmTimeLineMidiEvtObjPtr(   _cmTl_t* p, cmTlObj_t* op )
{ return _cmTlMidiEvtObjPtr(p,op,true); }

cmTlAudioFile_t* _cmTimeLineAudioFileObjPtr( _cmTl_t* p, cmTlObj_t* op )
{ return _cmTlAudioFileObjPtr(p,op,true);}

cmTlAudioEvt_t*  _cmTimeLineAudioEvtObjPtr(  _cmTl_t* p, cmTlObj_t* op )
{ return _cmTlAudioEvtObjPtr(p,op,true);}

cmTlMarker_t*    _cmTimeLineMarkerObjPtr(    _cmTl_t* p, cmTlObj_t* op )
{ return _cmTlMarkerObjPtr(p,op,true);}


// Locate a record  which matches 'name' and (optionally) 'seqId'
_cmTlObj_t* _cmTlFindRecd( _cmTl_t* p, unsigned seqId, const cmChar_t* name )
{
  if( name == NULL )
    return NULL;

  unsigned i;

  for(i=0; i<p->seqCnt; ++i)
    if( seqId==cmInvalidId || seqId == i )
    {
      _cmTlObj_t* op = p->seq[i].first;
      while(op != NULL)
      {
        if( strcmp(op->obj->name,name) == 0 )
          return op;

        op = op->next;
      }
    }

  return NULL;
}

// Returns true if 'op' is a child of 'ref'.
bool _cmTlIsChild( _cmTlObj_t* ref, _cmTlObj_t* op )
{
  // if 'op' is not active then it can't be a child
  if( op->obj == NULL )
    return false;

  // if 'ref' is NULL then match obj's which do not have a parent 
  if( ref == NULL )
    return op->obj->ref == NULL;

  // if 'ref' is the parent of 'op'.
  return op->obj->ref == ref->obj;
}

// calc the absolute start time of this object by adding the
// time time offsets of all ancestors.
int _cmTlStartTime( const cmTlObj_t* obj )
{
  assert( obj != NULL );

  int t = 0;

  do
  {
    t += obj->begSmpIdx;
    obj=obj->ref;
  }while( obj != NULL);


  return t;
}



// Locate the closest record which is before 'np'.
// When multiple records have the same distance to 'np' then the last one inserted
// is taken as the closest. This way records with equal time  values will be
// secondarily sequenced on their order of insertion.
_cmTlObj_t* _cmTlFindRecdBefore( _cmTl_t* p, const _cmTlObj_t* np )
{
  assert( np->obj!=NULL && np->obj->seqId < p->seqCnt );

  // calc the absolute time of this object
  //int         absSmpIdx = _cmTlStartTime(np->obj);
  int         rsi = 0;
  _cmTlObj_t* rp  = NULL;
  _cmTlObj_t* op  = p->seq[np->obj->seqId].first;

  //printf("type:%i %i\n",np->obj->typeId,absSmpIdx);

  // for each object in the list ...
  while( op != NULL )
  {
    int csi;

    //if( op!=np && op->obj!=NULL && (csi = _cmTlStartTime(op->obj)) <= absSmpIdx )
    if( (op!=np) && (op->obj!=NULL) && ((csi = op->obj->seqSmpIdx) <= np->obj->seqSmpIdx) ) 
    {
      if( rp == NULL || csi >= rsi )
      {
        rp  = op;
        rsi = csi;
      }
    }
    op = op->next;
  }

  return rp;
}




// Mark 'op' and all children of 'op' for deletion. 
// Note that this function is recursive.
cmTlRC_t _cmTlDeleteDependentRecds( _cmTl_t* p, _cmTlObj_t* op )
{
  assert( op->obj!=NULL && op->obj->seqId < p->seqCnt );

  cmTlRC_t     rc     = kOkTlRC;
  _cmTlObj_t*  dp     = p->seq[op->obj->seqId].first;

  // mark all recd's that are children of 'op' for deletion
  while( dp != NULL )
  {
    // if 'dp' is a child of 'op'.
    if( _cmTlIsChild(op,dp) )
      if(( rc = _cmTlDeleteDependentRecds(p,dp)) != kOkTlRC )
        return rc;

    dp = dp->next;
  }
  
  // release any resources held by 'op'.
  switch(op->obj->typeId)
  {
    case kMidiFileTlId:
      {
        cmTlMidiFile_t* mp = _cmTimeLineMidiFileObjPtr(p,op->obj);
        cmMidiFileClose(&mp->h);
      }
      break;

    case kMidiEvtTlId:
      break;

    case kAudioFileTlId:
      {
        cmTlAudioFile_t* ap = _cmTimeLineAudioFileObjPtr(p,op->obj);
        cmAudioFileDelete(&ap->h);
      }
      break;

    case kAudioEvtTlId:
      break;

    case kMarkerTlId:
      break;

    default:
      return cmErrMsg(&p->err,kUnknownRecdTypeTlRC,"An unknown time line object type (%i) was encounterned during object deletion.",op->obj->typeId);

  }

  // mark 'op' as deleted by setting op->obj to NULL
  op->obj = NULL; 

  return kOkTlRC;
}


// Delete 'op' and all of its dependents. 
cmTlRC_t _cmTlDeleteRecd( _cmTl_t* p, _cmTlObj_t* op )
{
  if( op == NULL )
    return kOkTlRC;

  assert( op->obj!=NULL && op->obj->seqId < p->seqCnt );
  cmTlRC_t rc;

  _cmTlSeq_t* s = p->seq + op->obj->seqId;

  // mark this object and any objecs which are dependent on it for deletion
  if((rc =_cmTlDeleteDependentRecds(p,op)) != kOkTlRC )
    return rc;


  // unlink and delete and records marked for deletion
  op = s->first;

  while( op )
  {
    _cmTlObj_t* tp = op->next;

    // if this object is marked for deletion unlink it from this master list
    if( op->obj == NULL )
    {
      if( op->next != NULL )
        op->next->prev = op->prev;
      else
      {
        assert( s->last == op );
        s->last = op->prev;

      }

      if( op->prev != NULL )
        op->prev->next = op->next;
      else
      {
        assert( s->first == op );
        s->first = op->next;      
      }

      // free the record
      cmLhFreePtr(p->lH,(void**)&op->mem);
      
    }

    op = tp;
  }

  return rc;
}

// Insert 'op' after 'bp'.
// If 'bp' is NULL then 'op' is inserted as p->seq[op->seqId].first.
void _cmTlInsertAfter( _cmTl_t* p, _cmTlObj_t* bp, _cmTlObj_t* op )
{
  assert( op->obj!=NULL && op->obj->seqId < p->seqCnt );
  _cmTlSeq_t* s = p->seq + op->obj->seqId;

  op->prev = bp; 

  // if op is being inserted at the beginning of the list
  if( bp == NULL )
  {
    op->next = s->first;

    if( s->first != NULL )
      s->first->prev = op;

    s->first = op;

  }
  else // op is being inserted in the middle or end of the list
  {
    op->next = bp->next;

    if( bp->next != NULL )
      bp->next->prev = op;
    else
    {
      // insertion at end
      assert( bp == s->last );
      s->last = op;
    }
    
    bp->next = op;
  }

  if( s->last == NULL )
    s->last = op;

  if( s->first == NULL )
    s->first = op;
}

// Allocate an object record
cmTlRC_t _cmTlAllocRecd2( 
  _cmTl_t*        p, 
  const cmChar_t* nameStr,     // NULL, empty or unique name
  _cmTlObj_t*     refPtr,      // parent object
  int             begSmpIdx,   // start time  
  unsigned        durSmpCnt,   // duration
  unsigned        typeId,      // object type id
  unsigned        seqId,       // owning seq
  unsigned        recdByteCnt, // byte for type specific data to follow _cmTlObj_t data
  _cmTlObj_t**    opp )        // return pointer
{
  *opp = NULL;

  if( nameStr == NULL )
    nameStr = "";

  // get the length of the recd name field
  unsigned nameByteCnt = strlen(nameStr)+1;

  // verify that the name was not already used by another recd
  if(  nameByteCnt>1 && _cmTlFindRecd(p,seqId,nameStr) != NULL )
    return cmErrMsg(&p->err,kDuplNameTlRC,"The object identifier '%s' was already used.",nameStr);

  assert( refPtr==NULL || refPtr->obj !=NULL );

  if( refPtr != NULL && refPtr->obj->seqId != seqId )
    return cmErrMsg(&p->err,kInvalidSeqIdTlRC,"The sequence id of the reference object (%i) does not match the sequence id (%i) of the new object (label:%s).",refPtr->obj->seqId,seqId,cmStringNullGuard(nameStr));

  if( seqId >= p->seqCnt )
  {    
    // assume the sequence id's arrive in increasing order
    assert( seqId == p->seqCnt );
    assert( refPtr == NULL );
    p->seqCnt = seqId+1;
    p->seq    = cmMemResizePZ(_cmTlSeq_t,p->seq,p->seqCnt);
  }

  // calc the total size of the recd. memory layout: [name /0 _cmTlObj_t cmTlObj_t <type specific fields> ]
  unsigned    byteCnt     = sizeof(unsigned) + sizeof(unsigned) + nameByteCnt + sizeof(_cmTlObj_t) + recdByteCnt;
  void*       mem         = cmLHeapAllocZ( p->lH, byteCnt );
  unsigned*   tidPtr      = (unsigned*)mem;
  unsigned*   parentIdPtr = tidPtr + 1;
  cmChar_t*   name        = (cmChar_t*)(parentIdPtr + 1);
  _cmTlObj_t* op          = (_cmTlObj_t*)(name + nameByteCnt);
  cmTlObj_t*  tp          = (cmTlObj_t*)(op+1);

  // The entire object is contained in mem[]
  // Memory Layout:
  // kObjTlId parentId name[] \0 _cmTlObj_t cmTlObj_t [recdByteCnt - sizeof(cmTlObj_t)]

  strcpy(name,nameStr);
  
  // the first element in the mem[] buffer must be kObjTlId - this allows
  // mem[] to be used directly as the serialized version of the buffer.
  *tidPtr        = kObjTlId; 
  *parentIdPtr   = refPtr==NULL ? cmInvalidId : refPtr->obj->uid; 
  tp->reserved   = op;
  tp->seqId      = seqId;
  tp->name       = name;
  tp->uid        = p->nextUId++;
  tp->typeId     = typeId;
  tp->ref        = refPtr==NULL ? NULL : refPtr->obj;
  tp->begSmpIdx  = refPtr==NULL ? 0    : begSmpIdx;
  tp->durSmpCnt  = durSmpCnt;
  tp->seqSmpIdx  = refPtr==NULL ? 0    : refPtr->obj->seqSmpIdx + begSmpIdx;
  tp->flags      = 0;
  tp->text       = NULL;

  //printf("%9i %9i %9i %9i\n",tp->begSmpIdx,tp->durSmpCnt,refPtr==NULL?0:refPtr->obj->seqSmpIdx, tp->seqSmpIdx);
  
  op->obj        = tp;
  op->mem        = mem;
  op->memByteCnt = byteCnt;
  op->next       = NULL;
  op->prev       = NULL;

  _cmTlInsertAfter(p, _cmTlFindRecdBefore(p,op), op );


  *opp = op;

  return kOkTlRC;
}

cmTlRC_t _cmTlAllocRecd( _cmTl_t* p, const cmChar_t* nameStr, const cmChar_t* refIdStr, int begSmpIdx, unsigned durSmpCnt, unsigned typeId, unsigned seqId, unsigned recdByteCnt, _cmTlObj_t** opp )
{ 
    // locate the obj recd that this recd is part of (the parent recd)
  _cmTlObj_t* refPtr = _cmTlFindRecd(p,seqId,refIdStr);

  // if this obj has a parent but it was not found
  if( refPtr == NULL && refIdStr!=NULL && strlen(refIdStr)>0 )
    return cmErrMsg(&p->err,kRefNotFoundTlRC,"Reference identifier '%s' not found for object '%s'.",refIdStr,cmStringNullGuard(nameStr));

  return _cmTlAllocRecd2(p,nameStr,refPtr,begSmpIdx,durSmpCnt,typeId,seqId,recdByteCnt,opp); 
}

void _cmTlNotifyListener( _cmTl_t* p, cmTlUiMsgTypeId_t msgTypeId, _cmTlObj_t* op, unsigned seqId  )
{
  if( p->cbFunc == NULL )
    return;

  switch( msgTypeId )
  {
    case kInitMsgTlId:
    case kFinalMsgTlId:
    case kDoneMsgTlId:
      {
        _cmTlMsg_t m;
        m.typeId = kMsgTlId;
        m.msgId  = msgTypeId;
        m.srate  = p->srate;
        m.seqCnt = p->seqCnt;
        m.seqId  = seqId;
        p->cbFunc( p->cbArg, &m, sizeof(m) );
      }
      break;

    case kInsertMsgTlId:
      if( op != NULL )
        p->cbFunc( p->cbArg, op->mem, op->memByteCnt );
      break;

    default:
      { assert(0); }
  }
}

cmTlRC_t _cmTlAllocAudioFileRecd( _cmTl_t* p, const cmChar_t* nameStr, const cmChar_t* refIdStr, int begSmpIdx, unsigned seqId, const cmChar_t* fn )
{
  cmAudioFileH_t    afH = cmNullAudioFileH;
  cmAudioFileInfo_t info;
  cmRC_t            afRC        = cmOkRC;
  cmTlRC_t          rc;
  _cmTlObj_t*       op          = NULL;

  // prepend the time-line path prefix to the filename
  fn = cmFsMakeFn( p->prefixPath, fn, NULL, NULL );

  unsigned          recdByteCnt = sizeof(cmTlAudioFile_t) + strlen(fn) + 1;

  if( cmAudioFileIsValid( afH = cmAudioFileNewOpen(fn, &info, &afRC, p->err.rpt )) == false )
  {
    rc = cmErrMsg(&p->err,kAudioFileFailTlRC,"The time line audio file '%s' could not be opened.",cmStringNullGuard(fn));
    goto errLabel;
  }

  if((rc = _cmTlAllocRecd(p,nameStr,refIdStr,begSmpIdx,info.frameCnt,kAudioFileTlId,seqId,recdByteCnt,&op)) != kOkTlRC )
    goto errLabel;

  assert(op != NULL && fn != NULL );

  cmTlAudioFile_t* ap = _cmTimeLineAudioFileObjPtr(p,op->obj);
  char*  cp = (char*)ap;

  assert(ap != NULL );
    
  ap->h    = afH;
  ap->info = info;
  ap->fn   = cp + sizeof(cmTlAudioFile_t); 
  strcpy(ap->fn,fn); // copy the file name into the extra memory

  assert( ap->fn + strlen(fn) + 1 == cp + recdByteCnt );

  op->obj->text = ap->fn;

  // notify listeners that an new object was created by sending a kObjTlId msg
  // notifiy any listeners that a midi file object was created
  //_cmTlNotifyListener(p, kInsertMsgTlId, op );

 errLabel:
  cmFsFreeFn(fn);

  if( rc != kOkTlRC )
    cmAudioFileDelete(&afH);
  
  return rc;
}


cmTlRC_t _cmTlProcMidiFile( _cmTl_t* p,  _cmTlObj_t* op, cmMidiFileH_t mfH )
 {
  cmTlRC_t                 rc           = kOkTlRC;
  cmTlMidiFile_t*          mfp          = _cmTimeLineMidiFileObjPtr(p,op->obj);
  unsigned                 mn           = cmMidiFileMsgCount(mfH);
  const cmMidiTrackMsg_t** mapp         = cmMidiFileMsgArray(mfH);
  unsigned                 mi           = 0;
  _cmTlObj_t*              refOp        = op;
  double                   smpPerMicro  = p->srate / 1000000.0;
  unsigned                 begSmpIdx0   = 0;
  mfp->noteOnCnt = 0;
  
  // for each midi message
  for(; mi<mn; ++mi)
  {
    _cmTlObj_t*             meop              = NULL;
    const cmMidiTrackMsg_t* mp                = mapp[mi];
    int                     begSmpIdx         = mp->amicro * smpPerMicro;
    int                     durSmpCnt         = 0;
    unsigned                midiTrkMsgByteCnt = cmMidiFilePackTrackMsgBufByteCount( mp );
    unsigned                recdByteCnt       = sizeof(cmTlMidiEvt_t) + midiTrkMsgByteCnt;

    // count the note-on messages
    if( cmMidiIsNoteOn(mp->status) )
    {
      durSmpCnt = mp->u.chMsgPtr->durMicros * smpPerMicro;
      ++mfp->noteOnCnt;
    }

    if( cmMidiIsCtl(mp->status) && cmMidiIsSustainPedal(mp->status,mp->u.chMsgPtr->d0) )
      durSmpCnt = mp->u.chMsgPtr->durMicros * smpPerMicro;

    // allocate the generic time-line object record
    if((rc = _cmTlAllocRecd2(p, NULL, refOp, begSmpIdx-begSmpIdx0, durSmpCnt, kMidiEvtTlId, mfp->obj.seqId, recdByteCnt, &meop)) != kOkTlRC )
      goto errLabel;

    begSmpIdx0 = begSmpIdx;
    
    assert( meop != NULL );
    
    cmTlMidiEvt_t* mep = _cmTimeLineMidiEvtObjPtr(p,meop->obj);
    char*          cp  = (char*)mep;

    assert( mep != NULL );

    // Set the cmTlMidiEvt_t.msg cmMidiTrkMsg_t pointer to point to the
    // extra memory allocated just past the cmTlMidiEvt_t recd.
    mep->msg = (cmMidiTrackMsg_t*)(cp + sizeof(cmTlMidiEvt_t));
    mep->midiFileObjId = mfp->obj.uid;

    // Do not write MIDI objects that are part of a MIDI file. They will be automatically
    // loaded when the time line is loaded and therefore do not need to be save 
    // explicitely in the time line data.
    meop->obj->flags = cmSetFlag(meop->obj->flags,kNoWriteTlFl);

    // Pack the cmMidiTrackMsg_t record into the extra memory
    cmMidiFilePackTrackMsg( mp, (char*)mep->msg, midiTrkMsgByteCnt );

    // verify that the memory allocation was calculated correctly
    assert( cp + recdByteCnt == ((char*)mep->msg) + midiTrkMsgByteCnt);

    // this midi event is the ref. for the next midi evt
    refOp = meop;
  }

 errLabel:
  return rc;
}

cmTlRC_t _cmTlAllocMidiFileRecd( _cmTl_t* p, const cmChar_t* nameStr, const cmChar_t* refIdStr, int begSmpIdx, unsigned seqId, const cmChar_t* fn )
{
  cmMidiFileH_t mfH = cmMidiFileNullHandle;
  cmTlRC_t      rc  = kOkTlRC;
  _cmTlObj_t*   op  = NULL;

  // prepend the time-line path prefix to the filename
  fn = cmFsMakeFn( p->prefixPath, fn, NULL, NULL );

  // open the midi file
  if( cmMidiFileOpen(&p->ctx, &mfH, fn ) != kOkMfRC )
    return cmErrMsg(&p->err,kMidiFileFailTlRC,"The time line midi file '%s' could not be opened.",cmStringNullGuard(fn));

  // force the first msg to occurr one quarter note into the file
  cmMidiFileSetDelay(mfH, cmMidiFileTicksPerQN(mfH) );

  unsigned durSmpCnt = floor(cmMidiFileDurSecs(mfH)*p->srate);

  // convert the midi file from delta ticks to delta samples
  //cmMidiFileTickToSamples(mfH,p->srate,false);

  // assign note durations to all note-on msg's
  cmMidiFileCalcNoteDurations(mfH);

  unsigned recdByteCnt = sizeof(cmTlMidiFile_t) + strlen(fn) + 1;

  // allocate the midi file time line object
  if((rc = _cmTlAllocRecd(p,nameStr,refIdStr,begSmpIdx,durSmpCnt,kMidiFileTlId,seqId,recdByteCnt,&op)) != kOkTlRC )
    goto errLabel;

  assert( op != NULL && fn != NULL );

  cmTlMidiFile_t* mp = _cmTimeLineMidiFileObjPtr(p,op->obj);
  char*           cp = (char*)mp;

  assert(mp != NULL );
    
  mp->h    = mfH;
  mp->fn   = cp + sizeof(cmTlMidiFile_t);
  strcpy(mp->fn,fn); // copy the filename into the extra memory

  assert( mp->fn + strlen(mp->fn) + 1 == cp + recdByteCnt );

  op->obj->text = mp->fn;


  // insert the events in the midi file as individual time line objects
  if((rc = _cmTlProcMidiFile(p, op, mfH)) != kOkTlRC )
    goto errLabel;

  
 errLabel:
  cmFsFreeFn(fn);

  if( rc != kOkTlRC )
  {
    cmMidiFileClose(&mfH);

    _cmTlDeleteRecd(p, op);

  }

  return rc;
}

cmTlRC_t _cmTlAllocMarkerRecd( _cmTl_t* p, const cmChar_t* nameStr, const cmChar_t* refIdStr, int begSmpIdx, unsigned durSmpCnt, unsigned seqId, const cmChar_t* text, unsigned bar, const cmChar_t* sectionStr, unsigned markerTypeId, cmTlObj_t* refObjPtr )
{
  cmTlRC_t        rc      = kOkTlRC;
  _cmTlObj_t*     op      = NULL;
  const cmChar_t* textStr = text==NULL       ? "" : text;
  const cmChar_t* sectStr = sectionStr==NULL ? "" : sectionStr;

  // add memory at the end of the the cmTlMarker_t record to hold the text string.
  unsigned recdByteCnt = sizeof(cmTlMarker_t) + strlen(textStr) + sizeof(bar) + strlen(sectStr) + 2;

  if(refIdStr == NULL )
    rc = _cmTlAllocRecd2(p,nameStr,(_cmTlObj_t*)refObjPtr->reserved,begSmpIdx,durSmpCnt,kMarkerTlId,seqId,recdByteCnt,&op);
  else
    rc = _cmTlAllocRecd(p,nameStr,refIdStr,begSmpIdx,durSmpCnt,kMarkerTlId,seqId,recdByteCnt,&op);

  if(rc != kOkTlRC )
    goto errLabel;

  assert(op != NULL);

  // get a ptr to the marker part of the object 
  cmTlMarker_t* mp = _cmTimeLineMarkerObjPtr(p,op->obj);

  assert(mp != NULL );

  // copy the marker text string into the memory just past the cmTlMarker_t recd.
  cmChar_t* tp = (cmChar_t*)(mp + 1);
  strcpy(tp,textStr);
  
  // copy the section label string into memory just past the markers text string
  cmChar_t* sp = tp + strlen(tp) + 1;
  strcpy(sp,sectStr);

  // onset markers are generated but not written
  if( markerTypeId != kAudioMarkTlId )
    mp->obj.flags = cmSetFlag(mp->obj.flags,kNoWriteTlFl);

  mp->typeId     = markerTypeId;
  mp->text       = tp;
  mp->sectionStr = sp;
  mp->bar        = bar;
  op->obj->text  = tp;

 errLabel:
  if( op != NULL && rc != kOkTlRC )
    _cmTlDeleteRecd(p,op);

  return rc;
}

cmTlRC_t _cmTlAllocAudioEvtRecd( _cmTl_t* p, const cmChar_t* nameStr, const cmChar_t* refIdStr, int begSmpIdx, unsigned durSmpCnt, unsigned seqId, const cmChar_t* text )
{
  cmTlRC_t    rc          = kOkTlRC;
  _cmTlObj_t* op          = NULL;
  const cmChar_t* textStr = text==NULL ? "" : text;

  unsigned    recdByteCnt = sizeof(cmTlAudioEvt_t) + strlen(textStr) + 1;

  if((rc = _cmTlAllocRecd(p,nameStr,refIdStr,begSmpIdx,durSmpCnt,kAudioEvtTlId,seqId,recdByteCnt,&op)) != kOkTlRC )
    goto errLabel;

  assert(op != NULL);

  cmTlAudioEvt_t* mp = _cmTimeLineAudioEvtObjPtr(p,op->obj);

  assert(mp != NULL );

  // copy the marker text string into the memory just past the cmTlAudioEvt_t recd.
  cmChar_t* tp = (cmChar_t*)(mp + 1);
  strcpy(tp,textStr);

  mp->text = tp;
  op->obj->text = tp;

 errLabel:
  if( op != NULL && rc != kOkTlRC )
    _cmTlDeleteRecd(p,op);

  return rc;
}

cmTlRC_t _cmTlAllocRecdFromJson(_cmTl_t* p,const cmChar_t* nameStr, const cmChar_t* typeIdStr,const cmChar_t* refIdStr, int begSmpIdx, unsigned durSmpCnt, unsigned seqId, const cmChar_t* textStr, unsigned bar, const cmChar_t* sectionStr)
{
  cmTlRC_t   rc    = kOkTlRC;
  unsigned   typeId = _cmTlIdLabelToId(p,typeIdStr);

  switch( typeId )
  {
    case kAudioFileTlId: rc = _cmTlAllocAudioFileRecd(p,nameStr,refIdStr,begSmpIdx,          seqId,textStr); break;
    case kMidiFileTlId:  rc = _cmTlAllocMidiFileRecd( p,nameStr,refIdStr,begSmpIdx,          seqId,textStr); break;
    case kMarkerTlId:    rc = _cmTlAllocMarkerRecd(   p,nameStr,refIdStr,begSmpIdx,durSmpCnt,seqId,textStr,bar,sectionStr,kAudioMarkTlId,NULL); break;
    case kAudioEvtTlId:  rc = _cmTlAllocAudioEvtRecd( p,nameStr,refIdStr,begSmpIdx,durSmpCnt,seqId,textStr); break;
    default:
      rc = cmErrMsg(&p->err,kParseFailTlRC,"'%s' is not a valid 'objArray' record type.",cmStringNullGuard(typeIdStr));
  }

  return rc;
}

cmTlRC_t _cmTimeLineFinalize( _cmTl_t* p )
{  
  cmTlRC_t rc = kOkTlRC;
  unsigned i;

  for(i=0; i<p->seqCnt; ++i)
    while( p->seq[i].first != NULL )
    {
      if((rc = _cmTlDeleteRecd(p,p->seq[i].first)) != kOkTlRC )
        goto errLabel;   
    }

  cmMemFree(p->seq);

  cmLHeapDestroy(&p->lH);

  cmMemPtrFree(&p->tmpBuf);

  cmMemPtrFree(&p->prefixPath);

  cmMemPtrFree(&p);


  return kOkTlRC;

 errLabel:
  return cmErrMsg(&p->err,kFinalizeFailTlRC,"Finalize failed.");
}

cmTlRC_t cmTimeLineInitialize( cmCtx_t* ctx, cmTlH_t* hp, cmTlCb_t cbFunc, void* cbArg, const cmChar_t* prefixPath )
{
  cmTlRC_t rc;

  if((rc = cmTimeLineFinalize(hp)) != kOkTlRC )
    return rc;

  _cmTl_t* p = cmMemAllocZ( _cmTl_t, 1 );

  cmErrSetup(&p->err,&ctx->rpt,"Time Line");
  p->ctx        = *ctx;
  p->cbFunc     = cbFunc;
  p->cbArg      = cbArg;
  p->prefixPath = cmMemAllocStr(prefixPath);

  if(cmLHeapIsValid( p->lH = cmLHeapCreate( 8192, ctx )) == false )
  {
    rc = cmErrMsg(&p->err,kLHeapFailTlRC,"The linked heap allocation failed.");
    goto errLabel;
  }

  hp->h = p;

  return rc;

 errLabel:
  _cmTimeLineFinalize(p);
  return rc;
}

cmTlRC_t   cmTimeLineInitializeFromFile( cmCtx_t* ctx, cmTlH_t* hp, cmTlCb_t cbFunc, void* cbArg, const cmChar_t* fn, const cmChar_t* prefixPath )
{
  cmTlRC_t rc;
  if((rc = cmTimeLineInitialize(ctx,hp,cbFunc,cbArg,prefixPath)) != kOkTlRC )
    return rc;

  return cmTimeLineReadJson(hp,fn);
}

const cmChar_t* cmTimeLineFileName( cmTlH_t h )
{ 
  _cmTl_t* p = _cmTlHandleToPtr(h);  
  return p->filename; 
}

const cmChar_t* cmTimeLinePrefixPath( cmTlH_t h )
{ 
  _cmTl_t* p = _cmTlHandleToPtr(h);  
  return p->prefixPath; 
}

cmTlRC_t cmTimeLineFinalize( cmTlH_t* hp )
{
  cmTlRC_t rc;
  if( hp == NULL || cmTimeLineIsValid(*hp) == false )
    return kOkTlRC;

  _cmTl_t* p = _cmTlHandleToPtr(*hp);


  if((rc = _cmTimeLineFinalize(p)) != kOkTlRC )
    return rc;

  hp->h = NULL;

  return rc;
}

bool cmTimeLineIsValid( cmTlH_t h )
{ return h.h != NULL; }

double cmTimeLineSampleRate( cmTlH_t h )
{
  _cmTl_t* p = _cmTlHandleToPtr(h);
  return p->srate;
}

int cmTimeLineSeqToLocalSampleIndex( int seqSmpIdx, cmTlObj_t* localObjPtr )
{
  return seqSmpIdx - localObjPtr->seqSmpIdx;
}

cmTlObj_t* _cmTimeLineIdToObj( _cmTl_t* p, unsigned seqId, unsigned id )
{
  assert( seqId < p->seqCnt );

  _cmTlObj_t* op = p->seq[seqId].first;
  for(; op != NULL; op=op->next )
    if( op->obj->uid == id )
      return op->obj;
  return NULL;
}

cmTlObj_t* cmTimeLineIdToObj( cmTlH_t h, unsigned seqId, unsigned id )
{
  _cmTl_t*    p  = _cmTlHandleToPtr(h);
  cmTlObj_t* op = NULL;

  if( seqId != cmInvalidId )
    op = _cmTimeLineIdToObj(p,seqId,id);
  else
  {
    for(seqId=0; seqId<p->seqCnt; ++seqId)
      if((op = _cmTimeLineIdToObj(p,seqId,id) ) != NULL )
        break;
  }

  return op;
}


cmTlObj_t* cmTimeLineNextObj( cmTlH_t h, cmTlObj_t* tp, unsigned seqId )
{
  _cmTl_t* p = _cmTlHandleToPtr(h);
  _cmTlObj_t* op;

  assert( seqId < p->seqCnt );
  if( seqId >= p->seqCnt )
    return NULL;

  // if tp is NULL then start at the begin of the obj list ...
  if( tp == NULL )
    op = p->seq[seqId].first;
  else
  {
    // ... otherwise advance to the obj after tp
    op = (_cmTlObj_t*)tp->reserved;
    assert( op != NULL );
    op = op->next;
  }

  // if the list is empty
  if( op == NULL )
    return NULL;
  
  // return the next object which matches seqId or
  // the next object if seqId == cmInvalidId
  for(; op != NULL; op = op->next)
    if( (seqId == cmInvalidId) || (op->obj->seqId == seqId) )
      return op->obj;

  return NULL;
}

cmTlObj_t* cmTimeLineNextTypeObj( cmTlH_t h, cmTlObj_t* p, unsigned seqId, unsigned typeId )
{
  cmTlObj_t* tp = p;
  while( (tp = cmTimeLineNextObj(h,tp,seqId)) != NULL )
    if( typeId == cmInvalidId || tp->typeId == typeId )
      return tp;

  return NULL;
}

cmTlMidiFile_t*  cmTlNextMidiFileObjPtr(  cmTlH_t h, cmTlObj_t* op, unsigned seqId )
{
  if((op = cmTimeLineNextTypeObj(h, op, seqId, kMidiFileTlId )) == NULL )
    return NULL;
  return cmTimeLineMidiFileObjPtr(h,op);
}

cmTlAudioFile_t* cmTlNextAudioFileObjPtr( cmTlH_t h, cmTlObj_t* op, unsigned seqId )
{
  if((op = cmTimeLineNextTypeObj(h, op, seqId, kAudioFileTlId )) == NULL )
    return NULL;
  return cmTimeLineAudioFileObjPtr(h,op);
}

cmTlMidiEvt_t*   cmTlNextMidiEvtObjPtr(   cmTlH_t h, cmTlObj_t* op, unsigned seqId )
{
  if((op = cmTimeLineNextTypeObj(h, op, seqId, kMidiEvtTlId )) == NULL )
    return NULL;
  return cmTimeLineMidiEvtObjPtr(h,op);
}

cmTlAudioEvt_t*  cmTlNextAudioEvtObjPtr(  cmTlH_t h, cmTlObj_t* op, unsigned seqId )
{
  if((op = cmTimeLineNextTypeObj(h, op, seqId, kAudioEvtTlId )) == NULL )
    return NULL;
  return cmTimeLineAudioEvtObjPtr(h,op);
}

cmTlMarker_t*    cmTlNextMarkerObjPtr(    cmTlH_t h, cmTlObj_t* op, unsigned seqId )
{
  if((op = cmTimeLineNextTypeObj(h, op, seqId, kMarkerTlId )) == NULL )
    return NULL;
  return cmTimeLineMarkerObjPtr(h,op);
}

cmTlObj_t*       cmTlIdToObjPtr( cmTlH_t h, unsigned uid )
{
  _cmTl_t* p = _cmTlHandleToPtr(h);
  unsigned i;
  for(i=0; i<p->seqCnt; ++i)
    //if( p->seq[i].first->obj->uid <= uid && uid <= p->seq[i].last->obj->uid )
    {
      _cmTlObj_t* op = p->seq[i].first;
      for(; op != NULL; op=op->next )
        if( op->obj->uid == uid )
          return op->obj;
    }

  return NULL;
}


cmTlMidiFile_t*  cmTimeLineMidiFileObjPtr(  cmTlH_t h, cmTlObj_t* op )
{
  _cmTl_t* p = _cmTlHandleToPtr(h);
  return _cmTimeLineMidiFileObjPtr(p,op);
}
cmTlAudioFile_t* cmTimeLineAudioFileObjPtr( cmTlH_t h, cmTlObj_t* op )
{
  _cmTl_t* p = _cmTlHandleToPtr(h);
  return _cmTimeLineAudioFileObjPtr(p,op);
}
cmTlMidiEvt_t*   cmTimeLineMidiEvtObjPtr(   cmTlH_t h, cmTlObj_t* op )
{
  _cmTl_t* p = _cmTlHandleToPtr(h);
  return _cmTimeLineMidiEvtObjPtr(p,op);
}
cmTlAudioEvt_t*  cmTimeLineAudioEvtObjPtr(  cmTlH_t h, cmTlObj_t* op )
{
  _cmTl_t* p = _cmTlHandleToPtr(h);
  return _cmTimeLineAudioEvtObjPtr(p,op);
}
cmTlMarker_t*    cmTimeLineMarkerObjPtr(    cmTlH_t h, cmTlObj_t* op )
{
  _cmTl_t* p = _cmTlHandleToPtr(h);
  return _cmTimeLineMarkerObjPtr(p,op);
}

cmTlMidiFile_t*  cmTlMidiFileObjPtr(  cmTlH_t h, cmTlObj_t* op )
{
  _cmTl_t* p = _cmTlHandleToPtr(h);
  return _cmTlMidiFileObjPtr(p,op,false);
}
cmTlAudioFile_t* cmTlAudioFileObjPtr( cmTlH_t h, cmTlObj_t* op )
{
  _cmTl_t* p = _cmTlHandleToPtr(h);
  return _cmTlAudioFileObjPtr(p,op,false);
}
cmTlMidiEvt_t*   cmTlMidiEvtObjPtr(   cmTlH_t h, cmTlObj_t* op )
{
  _cmTl_t* p = _cmTlHandleToPtr(h);
  return _cmTlMidiEvtObjPtr(p,op,false);
}
cmTlAudioEvt_t*  cmTlAudioEvtObjPtr(  cmTlH_t h, cmTlObj_t* op )
{
  _cmTl_t* p = _cmTlHandleToPtr(h);
  return _cmTlAudioEvtObjPtr(p,op,false);
}
cmTlMarker_t*    cmTlMarkerObjPtr(    cmTlH_t h, cmTlObj_t* op )
{
  _cmTl_t* p = _cmTlHandleToPtr(h);
  return _cmTlMarkerObjPtr(p,op,false);
}

cmTlRC_t cmTimeLineInsert( cmTlH_t h, const cmChar_t* nameStr, unsigned typeId, 
  const cmChar_t* fn, int begSmpIdx, unsigned durSmpCnt, const cmChar_t* refObjNameStr, unsigned seqId )
{
  _cmTl_t* p = _cmTlHandleToPtr(h);
  
  return _cmTlAllocRecdFromJson(p, nameStr, _cmTlIdToLabel(p,typeId), refObjNameStr, begSmpIdx, durSmpCnt, seqId, fn, 0, NULL); 
}

cmTlObj_t* _cmTimeLineFindFile( _cmTl_t* p, const cmChar_t* fn, unsigned typeId )
{
  unsigned i;
  for(i=0; i<p->seqCnt; ++i)
  {
    _cmTlObj_t* op = p->seq[i].first;
    for(; op != NULL; op=op->next )
      if( op->obj->typeId == typeId )
      {
        const cmChar_t* objFn = NULL;

        switch( typeId )
        {
          case kAudioFileTlId:
            objFn = ((cmTlAudioFile_t*)(op->obj))->fn;
            break;

          case kMidiFileTlId:
            objFn = ((cmTlMidiFile_t*)(op->obj))->fn;
            break;

          default:
            { assert(0); }
        }
        
        if( strcmp(objFn,fn) == 0 )
          return op->obj;

      }
  }

  return NULL;
}

cmTlAudioFile_t* cmTimeLineFindAudioFile( cmTlH_t h, const cmChar_t* fn )
{ 
  _cmTl_t*   p = _cmTlHandleToPtr(h);
  cmTlObj_t* op;
  if((op = _cmTimeLineFindFile(p,fn,kAudioFileTlId)) != NULL )
    return  _cmTlAudioFileObjPtr(p,op,true);
  return NULL;
}

cmTlMidiFile_t* cmTimeLineFindMidiFile( cmTlH_t h, const cmChar_t* fn )
{ 
  _cmTl_t*   p = _cmTlHandleToPtr(h);
  cmTlObj_t* op;
  if((op = _cmTimeLineFindFile(p,fn,kMidiFileTlId)) != NULL )
    return  _cmTlMidiFileObjPtr(p,op,true);
  return NULL;
}

_cmTlObj_t* _cmTimeLineObjAtTime( _cmTl_t* p, unsigned seqId, unsigned seqSmpIdx, unsigned typeId )
{
  assert( seqId < p->seqCnt );
  _cmTlObj_t* op      = p->seq[seqId].first;
  _cmTlObj_t* min_op  = NULL;
  unsigned    minDist = UINT_MAX;

  for(; op!=NULL; op=op->next)
    if( typeId==cmInvalidId || op->obj->typeId == typeId )
    {
      // if seqSmpIdx is inside this object - then return it as the solution
      if((op->obj->seqSmpIdx <= seqSmpIdx && seqSmpIdx < (op->obj->seqSmpIdx + op->obj->durSmpCnt)))
        return op;

      // measure the distance from seqSmpIdx to the begin and end of this object
      unsigned d0 =  op->obj->seqSmpIdx                    < seqSmpIdx ? seqSmpIdx - op->obj->seqSmpIdx                    : op->obj->seqSmpIdx - seqSmpIdx;
      unsigned d1 =  op->obj->seqSmpIdx+op->obj->durSmpCnt < seqSmpIdx ? seqSmpIdx - op->obj->seqSmpIdx+op->obj->durSmpCnt : op->obj->seqSmpIdx+op->obj->durSmpCnt - seqSmpIdx;

      // d0 and d1 should decrease as the cur object approaches seqSmpIdx
      // If they do not then the search is over - return the closest point.
      if( d0>minDist && d1>minDist)
        break;

      // track the min dist and the assoc'd obj
      if( d0 < minDist )
      {
        minDist = d0;
        min_op  = op;
      }

      if( d1 < minDist )
      {
        minDist = d1;
        min_op  = op;
      }
        
    }
       
  return min_op;
}

cmTlAudioFile_t* cmTimeLineAudioFileAtTime( cmTlH_t h, unsigned seqId, unsigned seqSmpIdx )
{ 
  _cmTl_t*   p = _cmTlHandleToPtr(h);
  _cmTlObj_t* op;
  if((op = _cmTimeLineObjAtTime(p,seqId,seqSmpIdx,kAudioFileTlId)) == NULL )
    return NULL;

  return _cmTlAudioFileObjPtr(p,op->obj,false);  
}

cmTlMidiFile_t*  cmTimeLineMidiFileAtTime(  cmTlH_t h, unsigned seqId, unsigned seqSmpIdx )
{
  _cmTl_t*   p = _cmTlHandleToPtr(h);
  _cmTlObj_t* op;
  if((op = _cmTimeLineObjAtTime(p,seqId,seqSmpIdx,kMidiFileTlId)) == NULL )
    return NULL;

  return _cmTlMidiFileObjPtr(p,op->obj,false);  
}

cmTlMidiEvt_t*   cmTimeLineMidiEvtAtTime(   cmTlH_t h, unsigned seqId, unsigned seqSmpIdx )
{
  _cmTl_t*   p = _cmTlHandleToPtr(h);
  _cmTlObj_t* op;
  if((op = _cmTimeLineObjAtTime(p,seqId,seqSmpIdx,kMidiEvtTlId)) == NULL )
    return NULL;

  return _cmTlMidiEvtObjPtr(p,op->obj,false);  
}

cmTlMarker_t*    cmTimeLineMarkerAtTime(    cmTlH_t h, unsigned seqId, unsigned seqSmpIdx )
{
  _cmTl_t*   p = _cmTlHandleToPtr(h);
  _cmTlObj_t* op;
  if((op = _cmTimeLineObjAtTime(p,seqId,seqSmpIdx,kMarkerTlId)) == NULL )
    return NULL;

  return _cmTlMarkerObjPtr(p,op->obj,false);  
}

cmTlMarker_t*    cmTimeLineMarkerFind( cmTlH_t h, const cmChar_t* markText )
{
  unsigned      i;
  unsigned      n   = cmTimeLineSeqCount(h);
  cmTlMarker_t* mop = NULL;
  
  for(i=0; i<n && mop==NULL; ++i)
  {
    cmTlObj_t* op = NULL;
    mop = NULL;
    while((mop = cmTlNextMarkerObjPtr(h,op,i)) != NULL )
    {
      if( mop->obj.name!=NULL && strcmp(mop->obj.name,markText)==0 )
        break;

      op = &mop->obj;
    }
  }
    
  return mop;

}

cmTlRC_t _cmTlParseErr( cmErr_t* err, const cmChar_t* errLabelPtr, unsigned idx, const cmChar_t* fn )
{
  cmTlRC_t rc;

  if( errLabelPtr != NULL )
    rc = cmErrMsg(err,kParseFailTlRC,"The required time line configuration field %s was not found in the record at index %i in '%s'.",cmStringNullGuard(errLabelPtr),idx,cmStringNullGuard(fn));
  else
    rc = cmErrMsg(err,kParseFailTlRC,"The time_line configuration parse failed on the record at index %i in '%s'.",idx,cmStringNullGuard(fn));

  return rc;
}

    
cmTlRC_t cmTimeLineReadJson(  cmTlH_t* hp, const cmChar_t* ifn )
{
  cmTlRC_t        rc  = kOkTlRC;
  cmJsRC_t        jsRC = kOkJsRC;
  cmJsonH_t       jsH = cmJsonNullHandle;
  cmJsonNode_t*   jnp = NULL;
  cmJsonNode_t*   cnp = NULL;
  const cmChar_t* errLabelPtr = NULL;
  int i;

  _cmTl_t* p = _cmTlHandleToPtr(*hp);
 
  // open the json file
  if( cmJsonInitializeFromFile(&jsH, ifn, &p->ctx ) != kOkJsRC )
    return cmErrMsg(&p->err,kJsonFailTlRC,"JSON file initialization failed.");

  if((jnp = cmJsonFindValue(jsH,"time_line",cmJsonRoot(jsH),kObjectTId)) == NULL)
  {
    rc = cmErrMsg(&p->err,kParseFailTlRC,"The JSON 'time_line' object was not found in '%s'.",cmStringNullGuard(ifn));
    goto errLabel;
  }

  if( cmJsonMemberValues(jnp,&errLabelPtr,
      "srate",kRealTId,&p->srate,
      "onset",kObjectTId | kOptArgJsFl,&cnp,
      "objArray",kArrayTId,&jnp,
      NULL) != kOkJsRC )
  {
    if(jsRC == kNodeNotFoundJsRC )
      rc = cmErrMsg(&p->err,kParseFailTlRC,"The JSON 'time_line' header required field:'%s' was not found in '%s'.",errLabelPtr,cmStringNullGuard(ifn));
    else
      rc = cmErrMsg(&p->err,kParseFailTlRC,"The JSON 'time_line' object header parse failed in '%s'.",cmStringNullGuard(ifn));
    goto errLabel;
  }

  if( cnp != NULL )
  {
    if((jsRC = cmJsonMemberValues(cnp,&errLabelPtr,
          "wndMs",        kRealTId, &p->onsetCfg.wndMs,
          "hopFact",      kIntTId,  &p->onsetCfg.hopFact,
          "audioChIdx",   kIntTId,  &p->onsetCfg.audioChIdx,
          "wndFrmCnt",    kIntTId,  &p->onsetCfg.wndFrmCnt,
          "preWndMult",   kRealTId, &p->onsetCfg.preWndMult,
          "threshold",    kRealTId, &p->onsetCfg.threshold,
          "maxFrqHz",     kRealTId, &p->onsetCfg.maxFrqHz,
          "filtCoeff",    kRealTId, &p->onsetCfg.filtCoeff,
          "medFiltWndMs", kRealTId, &p->onsetCfg.medFiltWndMs,
          "filterId",     kIntTId,  &p->onsetCfg.filterId,
          "preDelayMs",   kRealTId, &p->onsetCfg.preDelayMs,
          NULL)) !=        kOkJsRC )
    {
    
      if(jsRC == kNodeNotFoundJsRC )
        rc = cmErrMsg(&p->err,kParseFailTlRC,"The JSON 'time_line' onset analysizer cfg. required field:'%s' was not found in '%s'.",errLabelPtr,cmStringNullGuard(ifn));
      else
        rc = cmErrMsg(&p->err,kParseFailTlRC,"The JSON 'time_line' onset analyzer cfg.  in '%s'.",cmStringNullGuard(ifn));
      goto errLabel;
    }
  }
  
  for(i=0; i<cmJsonChildCount(jnp); ++i)
  {
    const cmJsonNode_t* rp = cmJsonArrayElementC(jnp,i);
    const cmChar_t* nameStr;
    const cmChar_t* typeIdStr;
    const cmChar_t* refIdStr;
    int             begSmpIdx;
    unsigned        durSmpCnt;
    unsigned        seqId;
    const cmChar_t* textStr;
    unsigned        bar = 0;
    const cmChar_t* sectStr = NULL;

    if( cmJsonMemberValues(rp,&errLabelPtr,
        "label",kStringTId,&nameStr,
        "type", kStringTId,&typeIdStr,
        "ref",  kStringTId,&refIdStr,
        "offset",kIntTId,&begSmpIdx,
        "smpCnt",kIntTId,&durSmpCnt,
        "trackId",kIntTId,&seqId,
        "textStr",kStringTId,&textStr,
        "bar",    kIntTId | kOptArgJsFl,&bar,
        "sectStr",kStringTId | kOptArgJsFl,&sectStr,
        NULL) != kOkJsRC )
    {
      rc = _cmTlParseErr(&p->err, errLabelPtr, i, ifn );
      goto errLabel;
    }


    if((rc = _cmTlAllocRecdFromJson(p,nameStr,typeIdStr,refIdStr,begSmpIdx,durSmpCnt,seqId,textStr,bar,sectStr)) != kOkTlRC )
      goto errLabel;
    
  }
  
  p->filename = cmLhAllocStr(p->lH,ifn);

 errLabel:
  if( rc != kOkTlRC )
    cmTimeLineFinalize(hp);

  cmJsonFinalize(&jsH);
  return rc;
}

unsigned cmTimeLineSeqCount( cmTlH_t h )
{
  _cmTl_t* p = _cmTlHandleToPtr(h);
  return p->seqCnt;
}

cmTlRC_t cmTimeLineSeqNotify( cmTlH_t h, unsigned seqId )
{
  cmTlRC_t rc = kOkTlRC;
  _cmTl_t* p  = _cmTlHandleToPtr(h);

  assert( seqId < p->seqCnt );

  _cmTlNotifyListener(p,kInitMsgTlId,NULL,seqId);

  _cmTlObj_t* op = p->seq[seqId].first;
  for(; op!=NULL; op=op->next)
    if( op->obj->seqId == seqId )
      _cmTlNotifyListener(p, kInsertMsgTlId, op, cmInvalidId );

  _cmTlNotifyListener(p,kDoneMsgTlId,NULL,seqId);

  return rc;
}

cmTlRC_t cmTimeLineGenOnsetMarks( cmTlH_t h, unsigned seqId )
{
  cmTlRC_t   rc         = kOkTlRC;
  _cmTl_t*   p          = _cmTlHandleToPtr(h);
  cmTlObj_t* op         = NULL;
  cmOnH_t    onsH       = cmOnsetNullHandle;
  cmChar_t*  textStr    = "";
  cmChar_t*  sectionStr = "";
  unsigned   bar        = cmInvalidIdx;
  unsigned   i,j;
  unsigned   smpIdx;

  if( p->onsetCfg.wndMs == 0 )
  {
    rc = cmErrMsg(&p->err,kOnsetFailTlRC,"Audio onset analyzer not-configured.");
    goto errLabel;    
  }

  // initialize the audio onset analyzer
  if( cmOnsetInitialize(&p->ctx, &onsH ) != kOkOnRC )
  {
    rc = cmErrMsg(&p->err,kOnsetFailTlRC,"Audio onset analyzer initialization failed.");
    goto errLabel;
  }

  // for each audio file
  
  for(i=0; (op = cmTimeLineNextTypeObj(h, op, seqId, kAudioFileTlId )) != NULL; ++i)
  {
    cmTlAudioFile_t* aop = NULL;

    // get a pointer to this audio file object
    if((aop = cmTlAudioFileObjPtr(h, op )) == NULL )
    {
      rc = cmErrMsg(&p->err,kTypeCvtFailTlRC,"Unexpected audio file object type conversion failure.");
      goto errLabel;
    }

    // analyze the onsets in this audio file
    if( cmOnsetProc(onsH,&p->onsetCfg,aop->fn) != kOkOnRC)
    {
      rc = cmErrMsg(&p->err,kOnsetFailTlRC,"Audio onset analyzer failed during processing.");
      goto errLabel;
    }
    
    // for each detected onset
    for(j=0; (smpIdx = cmOnsetSampleIndex(onsH,j))!=cmInvalidIdx; ++j)
    {
      cmChar_t* labelStr   = NULL;

      labelStr = cmTsPrintfP(labelStr,"a-%i-%i",i,j);

      // generate a marker record
      rc = _cmTlAllocMarkerRecd(p, labelStr, aop->obj.name, smpIdx, cmOnsetHopSampleCount(onsH), seqId, textStr, bar, sectionStr, kAudioOnsetMarkTlId,NULL );

      cmMemFree(labelStr);

      if( rc != kOkTlRC )
        goto errLabel;
    }    
  }

  unsigned       intervalSmpCnt = 0;
  cmTlMidiEvt_t* max_eop        = NULL;
  cmTlObj_t*     fop            = NULL;
  unsigned       midiFileObjId  = cmInvalidId;
 
  op = NULL;
  for(i=0; (op = cmTimeLineNextTypeObj(h, op, seqId, kMidiEvtTlId )) != NULL; ++i)
  {
    cmTlMidiEvt_t* eop = NULL;

    // get a pointer to this event object
    if((eop = cmTlMidiEvtObjPtr(h, op )) == NULL )
    {
      rc = cmErrMsg(&p->err,kTypeCvtFailTlRC,"Unexpected MIDI event object type conversion failure.");
      goto errLabel;
    }

    // if this is the first event in a new MIDI file
    if(eop->midiFileObjId != midiFileObjId)
    {
      // get the MIDI file object
      if((fop = cmTimeLineIdToObj(h,seqId,eop->midiFileObjId)) == NULL )
      {
        rc = cmErrMsg(&p->err,kAssertFailTlRC,"The MIDi file associated with a MIDI event could not be found.");
        goto errLabel;
      }

      midiFileObjId  = eop->midiFileObjId;
      max_eop        = NULL;
      intervalSmpCnt = 0;
      j              = 0;
    }

    // if we have reviewed one second of MIDI events
    if( intervalSmpCnt > p->srate )
    {
      // rewind the interval accumulator to less than one second
      while(intervalSmpCnt >= p->srate )
        intervalSmpCnt -= p->srate;

      // if a max veloctiy event was selected ...
      if( max_eop != NULL )
      {
        cmChar_t* labelStr = NULL;
        labelStr = cmTsPrintfP(labelStr,"%i-%i",i,j);

        // ... generate a marker record
        rc = _cmTlAllocMarkerRecd(p, labelStr, NULL, 0, cmOnsetHopSampleCount(onsH), seqId, textStr, bar, sectionStr, kMidiOnsetMarkTlId, &max_eop->obj );
    
        cmMemFree(labelStr);

        if( rc != kOkTlRC )
          goto errLabel;

        max_eop = NULL;

        ++j;
      }

    }

    // update the max velocity event
    if( max_eop==NULL || (eop->msg->status == kNoteOnMdId && eop->msg->u.chMsgPtr->d1>0 && max_eop->msg->u.chMsgPtr->d1 < eop->msg->u.chMsgPtr->d1) )
      max_eop = eop;

    // increment the interval accumulator
    intervalSmpCnt += eop->obj.begSmpIdx;
    
  }

 errLabel:
  if( cmOnsetFinalize(&onsH) != kOkOnRC )
    rc = cmErrMsg(&p->err,kOnsetFailTlRC,"Audio onset analyzer finalization failed.");

  return rc;
}

cmTlRC_t cmTimeLineDeleteOnsetMarks( cmTlH_t h, unsigned seqId )
{
  return kOkTlRC;
}


void _cmTimeLinePrintObj(_cmTl_t* p, _cmTlObj_t* op, cmRpt_t* rpt )
{

  cmRptPrintf(rpt,"%2i %5i %9i %9i %s %10s ",op->obj->seqId, op->obj->uid, op->obj->seqSmpIdx, op->obj->begSmpIdx, _cmTlIdToLabel(p,op->obj->typeId),op->obj->name);
  switch(op->obj->typeId )
  {
    case kMidiFileTlId:
      {
        cmTlMidiFile_t* rp = _cmTimeLineMidiFileObjPtr(p,op->obj);
        cmRptPrintf(rpt,"%s ", cmMidiFileName(rp->h));
      }
      break;

    case kMidiEvtTlId:
      {
        cmTlMidiEvt_t* rp = _cmTimeLineMidiEvtObjPtr(p,op->obj);
        if( op->obj->ref != NULL )
          cmRptPrintf(rpt,"%s ",op->obj->ref->name);
        cmRptPrintf(rpt,"%s ",cmMidiStatusToLabel(rp->msg->status));
      }
      break;

    case kAudioFileTlId:
      {
        cmTlAudioFile_t* rp = _cmTimeLineAudioFileObjPtr(p,op->obj);
        cmRptPrintf(rpt,"%s ", cmAudioFileName(rp->h));
      }
      break;

    case kAudioEvtTlId:
      {
        cmTlAudioEvt_t* rp = _cmTimeLineAudioEvtObjPtr(p,op->obj);
        cmRptPrintf(rpt,"%s",rp->text);
      }
      break;

    case kMarkerTlId:
      {
        cmTlMarker_t* rp = _cmTimeLineMarkerObjPtr(p,op->obj);
        cmRptPrintf(rpt,"%s ", rp->text );
      }
      break;
  }
  cmRptPrint(rpt,"\n");
}

cmTlRC_t _cmTimeLineWriteRecd( _cmTl_t* p, cmJsonH_t jsH, cmJsonNode_t* np, cmTlObj_t* obj )
{
  cmTlRC_t rc = kOkTlRC;

  // if this recd is writable and has not been previously written
  if( cmIsNotFlag(obj->flags,kNoWriteTlFl) && cmIsNotFlag(obj->flags,kReservedTlFl) )
  {
    const cmChar_t* refStr = NULL;

    if( obj->ref != NULL )
    {
      refStr = obj->ref->name;

      // if the ref obj was not previously written then write it (recursively) first
      if( cmIsNotFlag(obj->ref->flags, kReservedTlFl) )
        if((rc = _cmTimeLineWriteRecd(p,jsH,np,obj->ref)) != kOkTlRC )
          return rc;
    }

    // create the time-line recd
    if( cmJsonInsertPairs( jsH, cmJsonCreateObject(jsH,np),
        "label",   kStringTId, obj->name,
        "type",    kStringTId, _cmTlIdToLabel(p, obj->typeId ),
        "ref",     kStringTId, refStr==NULL ? "" : refStr,
        "offset",  kIntTId,    obj->begSmpIdx,
        "smpCnt",  kIntTId,    obj->durSmpCnt,
        "trackId", kIntTId,    obj->seqId,
        "textStr", kStringTId, obj->text==NULL ? "" : obj->text,
        NULL ) != kOkJsRC )
    {
      return cmErrMsg(&p->err,kJsonFailTlRC,"A time line insertion failed on label:%s type:%s text:%s.",cmStringNullGuard(obj->name),cmStringNullGuard(_cmTlIdToLabel(p, obj->typeId )), cmStringNullGuard(obj->text));
    }

    obj->flags = cmSetFlag(obj->flags, kReservedTlFl);
  }

  return rc;
}

cmTlRC_t cmTimeLineWrite( cmTlH_t h, const cmChar_t* fn )
{
  cmTlRC_t      rc  = kOkTlRC;
  _cmTl_t*      p   = _cmTlHandleToPtr(h);
  cmJsonH_t     jsH = cmJsonNullHandle;
  cmJsonNode_t* np;

  // initialize a JSON tree
  if( cmJsonInitialize(&jsH,&p->ctx) != kOkJsRC )
    return cmErrMsg(&p->err,kJsonFailTlRC,"JSON object initialization failed.");

  // create the root object
  if((np = cmJsonCreateObject(jsH,NULL)) == NULL )
  {
    rc = cmErrMsg(&p->err,kJsonFailTlRC,"Time line root object create failed while creating the time line file '%s.",cmStringNullGuard(fn));
    goto errLabel;
  }

  // create the time-line object
  if((np = cmJsonInsertPairObject(jsH, np, "time_line" )) == NULL )
  {
    rc = cmErrMsg(&p->err,kJsonFailTlRC,"'time_line' object created failed while creating the time line file '%s.",cmStringNullGuard(fn));
    goto errLabel;
  }

  // write the sample rate 
  if( cmJsonInsertPairReal(jsH, np, "srate", p->srate ) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailTlRC,"'sample rate' output failed while creating the time line file '%s.",cmStringNullGuard(fn));
    goto errLabel;
  }

  // create the time-line object array
  if((np = cmJsonInsertPairArray(jsH, np, "objArray")) == NULL )
  {
    rc = cmErrMsg(&p->err,kJsonFailTlRC,"'objArray' output failed while creating the time line file '%s.",cmStringNullGuard(fn));
    goto errLabel;
  }
  

  unsigned i;
  // write each time-line object
  for(i=0; i<p->seqCnt; ++i)
  {
    _cmTlObj_t* op = p->seq[i].first;
    for(; op != NULL; op=op->next )
      if((rc = _cmTimeLineWriteRecd(p,jsH,np,op->obj)) != kOkTlRC )
        goto errLabel;
  }  

  if( cmJsonErrorCode(jsH) == kOkJsRC )
    if( cmJsonWrite(jsH,NULL,fn) != kOkJsRC )
    {
      rc = cmErrMsg(&p->err,kJsonFailTlRC,"JSON write failed.");
      goto errLabel;
    }


 errLabel:

  if( cmJsonFinalize(&jsH) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailTlRC,"JSON finalize failed.");
    goto errLabel;
  }

  return rc;
}

cmTlRC_t cmTimeLinePrint( cmTlH_t h, cmRpt_t* rpt )
{
  _cmTl_t*    p  = _cmTlHandleToPtr(h);
  unsigned i;
  for(i=0; i<p->seqCnt; ++i)
  {
    _cmTlObj_t* op = p->seq[i].first;
    while(op != NULL)
    {
      _cmTimeLinePrintObj(p,op,rpt);
      op = op->next;
    }
  }
  return kOkTlRC;
}

cmTlRC_t cmTimeLinePrintFn( cmCtx_t* ctx, const cmChar_t* fn, const cmChar_t* prefixPath, cmRpt_t* rpt )
{
  cmTlRC_t rc;
  cmTlH_t h = cmTimeLineNullHandle;

  if((rc = cmTimeLineInitializeFromFile(ctx,&h,NULL,NULL,fn,prefixPath)) != kOkTlRC )
    return rc;

  cmTimeLinePrint(h,rpt);

  return cmTimeLineFinalize(&h);
}


cmTlRC_t     cmTimeLineTest( cmCtx_t* ctx, const cmChar_t* jsFn, const cmChar_t* prefixPath )
{
  cmTlRC_t rc  = kOkTlRC;
  cmTlH_t  tlH = cmTimeLineNullHandle;

  if((rc = cmTimeLineInitialize(ctx,&tlH,NULL,NULL,prefixPath)) != kOkTlRC )
    return rc;

  if((rc = cmTimeLineReadJson(&tlH,jsFn)) != kOkTlRC )
    goto errLabel;

  if((rc = cmTimeLineInsert(tlH,"Mark",kMarkerTlId,"My Marker",10,0,NULL,0)) != kOkTlRC )
    goto errLabel;

  cmTimeLinePrint(tlH, &ctx->rpt );

 errLabel:
  cmTimeLineFinalize(&tlH);

  return rc;
}

cmTlRC_t  _cmTimeLineDecodeObj( const void* msg, unsigned msgByteCnt, cmTlUiMsg_t* r )
{
  cmTlRC_t    rc          = kOkTlRC;
  unsigned*   typeIdPtr   = (unsigned*)msg;
  unsigned*   parentIdPtr = typeIdPtr + 1;
  const char* text        = (const char*)(parentIdPtr + 1);
  _cmTlObj_t* op          = (_cmTlObj_t*)(text + strlen(text) + 1);
  cmTlObj_t*  tp          = (cmTlObj_t*)(op + 1 );

  r->msgId       = kInsertMsgTlId;
  r->objId       = tp->uid;
  r->seqId       = tp->seqId;
  r->srate       = 0;
  return rc;
}

cmTlRC_t cmTimeLineDecode( const void* msg, unsigned msgByteCnt, cmTlUiMsg_t* r )
{
  cmTlRC_t    rc = kOkTlRC;
  _cmTlMsg_t* m  = (_cmTlMsg_t*)msg;

  switch( m->typeId )
  {
    case kMsgTlId:
      r->msgId = m->msgId;
      r->srate = m->srate;
      r->seqCnt= m->seqCnt;
      r->seqId = m->seqId;
      break;

    case kObjTlId:
      r->msgId = kInsertMsgTlId;
      rc = _cmTimeLineDecodeObj(msg,msgByteCnt,r);
      break;

    default:
      assert(0);
  }

  return rc;
  
}
