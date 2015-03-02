#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmJson.h"
#include "cmTime.h"
#include "cmMidi.h"
#include "cmMidiFile.h"
#include "cmAudioFile.h"
#include "cmTimeLine.h"
#include "cmSymTbl.h"
#include "cmScore.h"
#include "cmTakeSeqBldr.h"


// Score track record: Map a score event to a MIDI event. 
typedef struct cmScTrkMidiTsb_str
{
  unsigned mni;      // MIDI note index as an offset from the take marker
  unsigned muid;     // MIDI file msg  unique id
  unsigned scEvtIdx; // score event index this note/pedal is assoc'd with or -1 if it did not match
  unsigned flags;    // flags from cmScMatcherResult_t 
} cmScTrkMidiTsb_t;


// Score Tracking info. from a single take (time-line marker)
typedef struct cmScTrkTakeTsb_str
{
  unsigned          tlMarkerUid;  // marker time line uid assoc'd with this take
  cmScTrkMidiTsb_t* midiV;        // midiV[midiN] score to midi file map recd. array.
  unsigned          midiN;        // count of records in midiV[]
  unsigned          minMuid;      // min MIDI muid in midiV[]
  unsigned          maxMuid;      // max MIDI muid in midiV[]
  bool              failFl;
} cmScTrkTakeTsb_t;

enum 
{
  kNoteTsbFl   = 0x01,
  kPedalTsbFl  = 0x02,
  kEnableTsbFl = 0x04
};

// 
typedef struct cmMidiTsb_str
{  
  unsigned              srcId;     // marker uid or -1 if this event was manually inserted
  unsigned              scEvtIdx;  // score event assocd with this midi event
  unsigned              flags;     // note | pedal | enable  
  struct cmMidiTsb_str* ref;       // previous MIDI event in time
  unsigned              offsetSmp; // time offset from *ref
  unsigned              durSmp;    // duration of this MIDI event
  unsigned              d0;        // d0 MIDI channel msg data.
  unsigned              d1;        // d1 MIDI channel msg data
  struct cmMidiTsb_str* link;      // pointer to next MIDI event in list
} cmMidiTsb_t;

// This record contains all the score events and and score synchronized MIDI events
// associated with a given take.  Each call to cmTakeSeqBldrLoadTake() creates
// one of these records.
typedef struct cmTakeTsb_str
{
  unsigned              tlMarkerUid; // time-line marker uid associated with this take
  cmMidiTsb_t*          midi;        // midi events contained by this take  
  struct cmTakeTsb_str* link;
} cmTakeTsb_t;

typedef struct
{
  cmCtx_t         ctx;          // application context
  cmErr_t         err;          // internal error object
  cmJsonH_t       jsH;          // JSON tree used to hold score tracker info.
  const cmChar_t* tlFn;         // time line filename
  const cmChar_t* scFn;         // score file name
  const cmChar_t* tlPrefixPath; // path to time line audio and MIDI files
  cmTlH_t         tlH;          // time-line handle
  cmScH_t         scH;          // score handle

  cmScTrkTakeTsb_t*  scTrkTakeV;  // score tracker scTrkTakeV[ scTrkTakeN ]
  unsigned           scTrkTakeN;  
  
  cmTakeTsb_t* takes;   // list of loaded takes 
  cmTakeTsb_t* manual;  // list of manually inserted MIDI events
  
  cmTakeTsb_t* out;     // render list

} cmTsb_t;

cmTakeSeqBldrH_t cmTakeSeqBldrNullHandle = cmSTATIC_NULL_HANDLE;

cmTsb_t* _cmTsbHandleToPtr( cmTakeSeqBldrH_t h )
{
  cmTsb_t* p = (cmTsb_t*)h.h;
  return p;
}

cmTsbRC_t _cmTsbScoreTrkFree( cmTsb_t* p )
{
  cmTsbRC_t rc = kOkTsbRC;
  unsigned  i;

  if( cmJsonFinalize(&p->jsH) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailTsbRC,"JSON object finalize failed.");
    goto errLabel;
  }
  
  if( p->scTrkTakeV != NULL )
  {
    for(i=0; i<p->scTrkTakeN; ++i)
      cmMemPtrFree(&p->scTrkTakeV[i].midiV);

    cmMemPtrFree(&p->scTrkTakeV);
  }

  if( cmTimeLineFinalize(&p->tlH) != kOkTlRC )
    rc = cmErrMsg(&p->err,kTimeLineFailTsbRC,"Time line object finalize failed.");

  if( cmScoreFinalize(&p->scH) != kOkScRC )
    rc = cmErrMsg(&p->err,kScoreFailTsbRC,"Score finalize failed.");

 errLabel:
  return rc;
}


// Free a take record. Note that the record must be unlinked
// unlinked from p->takes (See _cmTakeTsbUnlink().) prior to calling this function.
void _cmTsbTakeFree( cmTsb_t* p, cmTakeTsb_t** tp )
{
  if( tp == NULL || *tp==NULL )
    return;

  cmMidiTsb_t* m  = (*tp)->midi;
    
  while( m != NULL )
  {
    cmMidiTsb_t* nm = m->link;
    cmMemFree(m);
    m = nm;
  }
  
  cmMemPtrFree(tp);
}

// Unlink a 'take' record from p->takes.
cmTakeTsb_t*  _cmTsbTakeUnlink( cmTsb_t* p, cmTakeTsb_t* t )
{
  cmTakeTsb_t* t0 = NULL;
  cmTakeTsb_t* t1 = p->takes;

  while( t1 != NULL )
  {
    if( t1 == t )
    {
      if( t0 == NULL )
        p->takes = t->link;
      else
        t0->link = t1->link;

      return t;
    }
  }
  return NULL;
}


cmTsbRC_t _cmTsbFree( cmTsb_t* p )
{
  cmTsbRC_t rc     = kOkTsbRC;

  if((rc = _cmTsbScoreTrkFree(p)) != kOkTsbRC )
    goto errLabel;

  cmTakeTsb_t* t = p->takes;
  
  while( t != NULL )
  {
    cmTakeTsb_t* nt = t->link;
    _cmTsbTakeFree(p,&t);
    t = nt;
  }

  _cmTsbTakeFree(p,&p->out);

  cmMemFree(p);

 errLabel:
  return rc;
}

cmScTrkTakeTsb_t* _cmTsbMarkerIdToScTrkTake( cmTsb_t* p, unsigned markerUid )
{
  unsigned i;
  for(i=0; p->scTrkTakeN; ++i)
    if( p->scTrkTakeV[i].tlMarkerUid == markerUid )
      return p->scTrkTakeV + i;
  return NULL;
}

cmScTrkMidiTsb_t* _cmTsbMuidToScTrkMidi( cmScTrkTakeTsb_t* t, unsigned muid )
{
  unsigned i;
  for(i=0; i<t->midiN; ++i)
    if( t->midiV[i].muid == muid )
      return t->midiV + i;
  return NULL;
}


cmTsbRC_t _cmTsbLoadScoreTrkFile( cmTsb_t* p, const cmChar_t* scoreTrkFn )
{
  cmTsbRC_t       rc       = kOkTsbRC;
  cmJsonNode_t*   tkArrObj = NULL;
  cmJsRC_t        jsRC     = kOkJsRC;
  const cmChar_t* errMsg   = NULL;
  unsigned        i;
 
  // initialize the TSB json object
  if(( rc = cmJsonInitializeFromFile(&p->jsH,scoreTrkFn,&p->ctx)) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailTsbRC,"The Take Sequence Builder JSON file object could not be initialized from '%s'.",cmStringNullGuard(scoreTrkFn));
    goto errLabel;
  }

  // parse the header
  if((jsRC = cmJsonMemberValues( cmJsonRoot(p->jsH), &errMsg,
        "timeLineFn",   kStringTId,              &p->tlFn,
        "scoreFn",      kStringTId,              &p->scFn,
        "tlPrefixPath", kStringTId,              &p->tlPrefixPath,
        "takeArray",    kArrayTId | kOptArgJsFl, &tkArrObj,
        NULL )) != kOkJsRC )
  {
    if( jsRC == kNodeNotFoundJsRC && errMsg != NULL )
      rc = cmErrMsg(&p->err,kParseFailTsbRC,"JSON file header parse failed missing required field:'%s'",errMsg);
    else
      rc = cmErrMsg(&p->err,kParseFailTsbRC,"JSON file header parse failed.");

    goto errLabel;
  }

  // count of take records
  p->scTrkTakeN = cmJsonChildCount(tkArrObj);

  // array of take records
  p->scTrkTakeV  = cmMemAllocZ(cmScTrkTakeTsb_t,p->scTrkTakeN);

  // for each take record
  for(i=0; i<p->scTrkTakeN; ++i)
  {
    cmJsonNode_t*     takeObj    = NULL;
    cmJsonNode_t*     noteArrObj = NULL;
    cmScTrkTakeTsb_t* t          = p->scTrkTakeV + i;
    unsigned          j;

    // get a pointer to the take record JSON object
    if((takeObj = cmJsonArrayElement(tkArrObj,i)) == NULL )
    {
      rc = cmErrMsg(&p->err,kParseFailTsbRC,"Take record header at index %i access failed.",i);
      goto errLabel;
    }

    // parse the take record
    if((jsRC = cmJsonMemberValues( takeObj, &errMsg,
          "markerUid",kIntTId,   &t->tlMarkerUid,
          "failFl",   kIntTId,   &t->failFl,
          "array",    kArrayTId, &noteArrObj,
          NULL)) != kOkJsRC )
    {
      if( jsRC == kNodeNotFoundJsRC && errMsg != NULL )
        rc = cmErrMsg(&p->err,kParseFailTsbRC,"JSON file take record parse failed missing required field:'%s'",errMsg);
      else
        rc = cmErrMsg(&p->err,kParseFailTsbRC,"JSON file take record parse failed.");

      goto errLabel;
    }

    // get the count of note records
    t->midiN = cmJsonChildCount(noteArrObj);
    
    // allocate a note record array for this take
    t->midiV = cmMemAllocZ(cmScTrkMidiTsb_t, t->midiN);
    t->minMuid =  INT_MAX;
    t->maxMuid =  0;

    // for each note record
    for(j=0; j<t->midiN; ++j)
    {
      cmJsonNode_t* noteObj = NULL;
      
      // get the note record JSON object
      if((noteObj = cmJsonArrayElement(noteArrObj,j)) == NULL )
      {
        rc = cmErrMsg(&p->err,kParseFailTsbRC,"Access failed for note record at index %i at take index %i.",j,i);
        goto errLabel;
      }

      // parse the note record
      if((jsRC = cmJsonMemberValues( noteObj, &errMsg,
            "mni",      kIntTId, &t->midiV[j].mni,
            "muid",     kIntTId, &t->midiV[j].muid,
            "scEvtIdx", kIntTId, &t->midiV[j].scEvtIdx,
            "flags",    kIntTId, &t->midiV[j].flags,
            NULL)) != kOkJsRC )
      {
        if( jsRC == kNodeNotFoundJsRC && errMsg != NULL )
          rc = cmErrMsg(&p->err,kParseFailTsbRC,"JSON file note record parse failed missing required field:'%s'",errMsg);
        else
          rc = cmErrMsg(&p->err,kParseFailTsbRC,"JSON file note record parse failed.");              

        goto errLabel;
      }

      if( t->midiV[j].muid < t->minMuid )
        t->minMuid = t->midiV[j].muid;

      if( t->midiV[j].muid > t->maxMuid )
        t->maxMuid = t->midiV[j].muid;

    }
  }

 errLabel:
  if( rc != kOkTsbRC )
    rc = _cmTsbScoreTrkFree(p);

  return rc;
}

cmTsbRC_t _cmTakeSeqBldrRender( cmTsb_t* p )
{
  cmTsbRC_t rc = kOkTsbRC;

  _cmTsbTakeFree(p,&p->out);

  // get the min/max scEvtIdx among all takes
  cmTakeTsb_t* t           = p->takes;
  cmMidiTsb_t* m           = NULL;
  unsigned     minScEvtIdx = INT_MAX;
  unsigned     maxScEvtIdx = 0;
  unsigned     i;

  for(; t!=NULL; t=t->link)
  {
    for(m=t->midi; m!=NULL; m=m->link)
    {
      if( m->scEvtIdx < minScEvtIdx )
        minScEvtIdx = m->scEvtIdx;

      if( m->scEvtIdx > maxScEvtIdx )
        maxScEvtIdx = m->scEvtIdx;
    }
  }

  p->out = cmMemAllocZ(cmTakeTsb_t,1);

  // allocate one event for each score postion to render
  cmMidiTsb_t* m0 = NULL;
  for(i=0; i<maxScEvtIdx-minScEvtIdx+1; ++i)
  {
    m = cmMemAllocZ(cmMidiTsb_t,1);
    m->srcId    = cmInvalidId;
    m->scEvtIdx = minScEvtIdx + i;
    m->ref      = m0;
    m0          = m;

    if( p->out->midi == NULL )
      p->out->midi = m;
  }

  // fill the event list from the selected takes
  for(t=p->takes; t!=NULL; t=t->link)
  {
    
  }
  
  if( rc != kOkTsbRC )
    _cmTsbTakeFree(p,&p->out);

  return rc;
}

cmTsbRC_t cmTakeSeqBldrAlloc( cmCtx_t* ctx, cmTakeSeqBldrH_t* hp )
{
  cmTsbRC_t rc;
  if((rc = cmTakeSeqBldrFree(hp)) != kOkTsbRC )
    return kOkTsbRC;

  cmTsb_t* p  = cmMemAllocZ(cmTsb_t,1);

  cmErrSetup(&p->err,&ctx->rpt,"TakeSeqBldr");

  p->ctx = *ctx;
  hp->h  = p;

  return rc;
}

cmTsbRC_t cmTakeSeqBldrAllocFn( cmCtx_t* ctx, cmTakeSeqBldrH_t* hp, const cmChar_t* scoreTrkFn )
{
  cmTsbRC_t rc;
  if((rc = cmTakeSeqBldrAlloc(ctx,hp)) != kOkTsbRC )
    return rc;

  if((rc = cmTakeSeqBldrInitialize(*hp,scoreTrkFn)) != kOkTsbRC )
    return rc;

  return rc;
}

cmTsbRC_t cmTakeSeqBldrFree( cmTakeSeqBldrH_t* hp )
{
  cmRC_t rc = kOkTsbRC;

  if( hp == NULL || cmTakeSeqBldrIsValid(*hp)==false )
    return kOkTsbRC;

  cmTsb_t* p = _cmTsbHandleToPtr(*hp);

  if((rc = _cmTsbFree(p)) != kOkTsbRC )
    return rc;

  hp->h = NULL;

  return rc;
}

bool cmTakeSeqBldrIsValid( cmTakeSeqBldrH_t h )
{  return h.h != NULL; }


cmTsbRC_t cmTakeSeqBldrInitialize( cmTakeSeqBldrH_t h, const cmChar_t* scoreTrkFn )
{
  cmTsbRC_t rc = kOkTsbRC; 
  cmTsb_t*  p  = _cmTsbHandleToPtr(h);
  
  if(( rc = _cmTsbLoadScoreTrkFile( p, scoreTrkFn )) != kOkTsbRC )
    return rc;
  
  if( cmTimeLineInitializeFromFile(&p->ctx, &p->tlH, NULL, NULL, p->tlFn, p->tlPrefixPath ) != kOkTlRC )
  {
    rc = cmErrMsg(&p->err,kTimeLineFailTsbRC,"The time-line file '%s' could not be loaded.",p->tlFn);
    goto errLabel;
  }

  if( cmScoreInitialize(&p->ctx, &p->scH, p->scFn, 0, NULL, 0, NULL, NULL, cmSymTblNullHandle ) != kOkScRC )
  {
    rc = cmErrMsg(&p->err,kScoreFailTsbRC,"The score file '%s' could not be loaded.",p->scFn);
    goto errLabel;
  }

 errLabel:
  if( rc != kOkTsbRC )
    _cmTsbScoreTrkFree(p);


  return rc;
}

cmTakeTsb_t* _cmTsbMarkerUidToTake( cmTsb_t* p, unsigned tlMarkerUid )
{
  cmTakeTsb_t* t = p->takes;
  for(; t != NULL; t=t->link)
    if( t->tlMarkerUid == tlMarkerUid )
      return t;
  return NULL;
}

cmTsbRC_t cmTakeSeqBldrLoadTake( cmTakeSeqBldrH_t h, unsigned tlMarkUid, bool overwriteFL )
{
  cmTsbRC_t       rc   = kOkTsbRC;
  cmTsb_t*        p    = _cmTsbHandleToPtr(h);
  cmTlMarker_t*   mark = NULL;
  cmTlMidiFile_t* mf   = NULL;
  cmMidiFileH_t   mfH  = cmMidiFileNullHandle;
  cmScTrkTakeTsb_t* stt = NULL;

  // verify that the requested take has not already been loaded
  if( _cmTsbMarkerUidToTake( p, tlMarkUid ) != NULL )
  {
    rc = cmErrMsg(&p->err,kInvalidArgTsbRC,"The take indicated by marker id %i has already been loaded.",tlMarkUid );
    goto errLabel;
  }

  // find the score tracked take for the requested marker
  if((stt = _cmTsbMarkerIdToScTrkTake(p,tlMarkUid )) == NULL )
  {
    rc = cmErrMsg(&p->err,kInvalidArgTsbRC,"The score tracked take indicated by marker id %i could not be found.", tlMarkUid );
    goto errLabel;
  }

  // get a pointer to the time-line marker object
  if((mark = cmTlMarkerObjPtr( p->tlH, cmTimeLineIdToObj( p->tlH, cmInvalidId, tlMarkUid))) == NULL )
  {
    rc = cmErrMsg(&p->err,kInvalidArgTsbRC,"The time-line marker uid '%i' is not valid.",tlMarkUid);
    goto errLabel;
  }

  // get the name of the MIDI file which contains the marker
  if((mf = cmTimeLineMidiFileAtTime( p->tlH, mark->obj.seqId, mark->obj.seqSmpIdx )) == NULL )
  {
    rc = cmErrMsg(&p->err,kInvalidArgTsbRC,"The time-line marker '%i' does not intersect with a MIDI file.",tlMarkUid);
    goto errLabel;
  }

  // open the MIDI file
  if( cmMidiFileOpen( cmMidiFileName(mf->h), &mfH, &p->ctx ) != kOkMfRC )
  {
    rc = cmErrMsg(&p->err,kInvalidArgTsbRC,"The MIDI file '%s' could not be opened.", cmStringNullGuard(cmMidiFileName(mf->h)));
    goto errLabel;
  }

  // convert the dtick field to absolute sample indexes
  cmMidiFileTickToSamples( mfH, cmTimeLineSampleRate(p->tlH), true );
  
  // calculate MIDI note and pedal durations (see cmMidiChMsg_t.durTicks)
  cmMidiFileCalcNoteDurations( mfH );
  
  unsigned                 i   = 0;
  unsigned                 n   = cmMidiFileMsgCount(mfH);
  const cmMidiTrackMsg_t** a   = cmMidiFileMsgArray(mfH);
  
  // allocate and link a new take render record
  cmTakeTsb_t* t = cmMemAllocZ(cmTakeTsb_t,1);

  t->tlMarkerUid = tlMarkUid;
  t->link        = p->takes;
  p->takes       = t;


  cmMidiTsb_t*            m0  = NULL;
  const cmMidiTrackMsg_t* mf0 = NULL;

  // for each MIDI message in the file 
  for(i=0; i<n; ++i)
  {
    const cmMidiTrackMsg_t* mf1 = a[i];

    // we are only interested in rendering notes and control msgs
    switch( mf1->status )
    {
      case kNoteOnMdId:
      case kNoteOffMdId:
      case kCtlMdId:        
        break;

      default:
        continue;
    }
    
    // if this MIDI message is inside the tracked region of the take
    if( stt->minMuid > mf1->uid || mf1->uid > stt->maxMuid )
      continue;

    // get a pointer to the tracking map
    cmScTrkMidiTsb_t* stm = _cmTsbMuidToScTrkMidi(stt, mf1->uid );

    // create a MIDI render event 
    cmMidiTsb_t* m1 = cmMemAllocZ(cmMidiTsb_t,1);
      
    m1->srcId     = tlMarkUid;
    m1->scEvtIdx  = stm != NULL ? stm->scEvtIdx : cmInvalidIdx;
    m1->flags     = stm != NULL ? stm->flags    : 0;
    m1->ref       = m0;
    m1->offsetSmp = mf0 == NULL ? 0 : mf1->dtick - mf0->dtick;
    m1->durSmp    = mf1->u.chMsgPtr->durTicks;
    m1->d0        = mf1->u.chMsgPtr->d0;
    m1->d1        = mf1->u.chMsgPtr->d1;
    m1->link      = NULL;

    if( m0 != NULL )
      m0->link      = m1;
    
    if( t->midi == NULL )
      t->midi = m1;

    m0  = m1;
    mf0 = mf1;

  }
  
 errLabel:
  if( cmMidiFileClose(&mfH) != kOkMfRC )
    rc = cmErrMsg(&p->err,kMidiFileFailTsbRC,"MIDI file close failed.");
  
  return rc;
}

cmTsbRC_t cmTakeSeqBldrUnloadTake( cmTakeSeqBldrH_t h, unsigned tlMarkUid )
{
  cmTsbRC_t    rc = kOkTsbRC;
  cmTsb_t*     p  = _cmTsbHandleToPtr(h);
  cmTakeTsb_t* t;

  if((t = _cmTsbMarkerUidToTake(p, tlMarkUid )) == NULL )
    return cmErrMsg(&p->err,kInvalidArgTsbRC,"The take indicated by marker id %i could not be found.",tlMarkUid);
  
  t = _cmTsbTakeUnlink(p,t);

  assert( t != NULL );

  _cmTsbTakeFree(p,&t);
    
  return rc;
}
  
cmTsbRC_t cmTakeSeqBldrInsertScoreNotes( cmTakeSeqBldrH_t h, unsigned begScEvtIdx, unsigned endScEvtId )
{
  cmTsbRC_t rc = kOkTsbRC;
  return rc;
}

cmTsbRC_t cmTakeSeqBldrRemoveScoreNotes( cmTakeSeqBldrH_t h, unsigned begScEvtIdx, unsigned endScEvtId )
{
  cmTsbRC_t rc = kOkTsbRC;
  return rc;
}

cmTsbRC_t cmTakeSeqBldrSelectEnable( cmTakeSeqBldrH_t h, unsigned flags, unsigned id, bool selectFl )
{
  cmTsbRC_t rc = kOkTsbRC;
  return rc;
}

cmTsbRC_t cmTakeSeqBldrEnableNote( cmTakeSeqBldrH_t h, unsigned ssqId, bool enableFl )
{
  cmTsbRC_t rc = kOkTsbRC;
  return rc;
}

cmTsbRC_t cmTakeSeqBldrMoveNote(   cmTakeSeqBldrH_t h, unsigned ssqId, int deltaSmpIdx )
{
  cmTsbRC_t rc = kOkTsbRC;
  return rc;
}

cmTsbRC_t cmTakeSeqBldrWriteMidiFile( cmTakeSeqBldrH_t h, const char* fn )
{
  cmTsbRC_t rc = kOkTsbRC;
  return rc;
}

cmTsbRC_t cmTakeSeqBldrTest( cmCtx_t* ctx )
{
  const cmChar_t*  scoreTrkFn = "/home/kevin/src/cmkc/src/kc/data/takeSeqBldr0.js";
  cmTakeSeqBldrH_t tsbH       = cmTakeSeqBldrNullHandle;
  cmTsbRC_t        tsbRC      = kOkTsbRC;
  unsigned         markerIdV[]  = { 2200, 2207 };
  unsigned         markerN      = sizeof(markerIdV)/sizeof(markerIdV[0]);
  unsigned         i;

  if((tsbRC = cmTakeSeqBldrAllocFn(ctx, &tsbH, scoreTrkFn )) != kOkTsbRC )
    return cmErrMsg(&ctx->err,tsbRC,"TSB Allocate and parse '%s' failed.",scoreTrkFn);

  cmRptPrintf(&ctx->rpt, "TakeSeqBldr Allocation Completed.");

  for(i=0; i<markerN; ++i)
  {
    if((tsbRC = cmTakeSeqBldrLoadTake(tsbH,markerIdV[i],false)) != kOkTsbRC )
      cmErrMsg(&ctx->err,tsbRC,"TSB load take failed.");

    cmRptPrintf(&ctx->rpt, "TakeSeqBldr Load Take %i Completed.",markerIdV[i]);
  }

  if((tsbRC = cmTakeSeqBldrFree(&tsbH)) != kOkTsbRC )
    return cmErrMsg(&ctx->err,tsbRC,"TSB Free failed.");

  return tsbRC;
}
