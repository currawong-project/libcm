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
  unsigned scEvtIdx; // score event index this not is assoc'd with or -1 if it did not match
  unsigned flags;    // flags from cmScMatcherResult_t 
} cmScTrkMidiTsb_t;


// Score Tracking info. from a single take (time-line marker)
typedef struct cmScTrkTakeTsb_str
{
  unsigned          markerUid;  // marker time line uid assoc'd with this take
  cmScTrkMidiTsb_t* midiV;      // midiV[midiN] score to midi file map recd. array.
  unsigned          midiN;  
  bool              failFl;
} cmScTrkTakeTsb_t;

enum 
{
  kNoteTsbFl   = 0x01,
  kPedalTsbFl  = 0x02,
  kEnableTsbFl = 0x04
};

// 
typedef struct cmMidiEvt_str
{  
  unsigned              srcId;     // marker uid or -1 if this event was manually inserted
  unsigned              flags;     // note | pedal | enable  
  struct cmMidiEvt_str* ref;       // previous MIDI event in time
  unsigned              offsetSmp; // time offset from *ref
  unsigned              durSmp;    // duration of this MIDI event
  unsigned              d0;        // d0 MIDI channel msg data.
  unsigned              d1;        // d1 MIDI channel msg data
  struct cmMidiEvt_str* link;      // pointer to next MIDI event in list
} cmMidiEvt_t;

// This record represents a note or pedal score event 
typedef struct cmScEvtTsb_str
{
  unsigned     flags;    // note | pedal
  unsigned     scEvtIdx; // score event index (into scH)
  cmMidiEvt_t* evtList;  // list of alternate MIDI events which may render this event
} cmScEvtTsb_t;

// This record contains all the score events and and score synchronized MIDI events
// associated with a given take.  Each call to cmTakeSeqBldrLoadTake() creates
// one of these records.
typedef struct cmTakeScEvtArrayTsb_str
{
  unsigned      tlMarkerUid; // time-line marker uid associated with this take
  cmScEvtTsb_t* scEvtV;      // scEvtV[scEvtN] array of score events contained by this take  
  unsigned      scEvtN;      // count of score events in this take 
} cmTakeScEvtArrayTsb_t;

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
  
  cmTakeScEvtArrayTsb_t* takes;   // list of scEvt arrays used by this sequence 
  cmTakeScEvtArrayTsb_t* manual;  // list of manually inserted MIDI events
  
  cmTakeScEvtArrayTsb_t* out;     // render list

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


cmTsbRC_t _cmTsbFree( cmTsb_t* p )
{
  cmTsbRC_t rc     = kOkTsbRC;

  if((rc = _cmTsbScoreTrkFree(p)) != kOkTsbRC )
    goto errLabel;

  cmMemFree(p);

 errLabel:
  return rc;
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
    cmJsonNode_t* takeObj    = NULL;
    cmJsonNode_t* noteArrObj = NULL;
    unsigned      j;

    // get a pointer to the take record JSON object
    if((takeObj = cmJsonArrayElement(tkArrObj,i)) == NULL )
    {
      rc = cmErrMsg(&p->err,kParseFailTsbRC,"Take record header at index %i access failed.",i);
      goto errLabel;
    }

    // parse the take record
    if((jsRC = cmJsonMemberValues( takeObj, &errMsg,
          "markerUid",kIntTId,   &p->scTrkTakeV[i].markerUid,
          "failFl",   kIntTId,   &p->scTrkTakeV[i].failFl,
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
    p->scTrkTakeV[i].midiN = cmJsonChildCount(noteArrObj);
    
    // allocate a note record array for this take
    p->scTrkTakeV[i].midiV = cmMemAllocZ(cmScTrkMidiTsb_t, p->scTrkTakeV[i].midiN);

    // for each note record
    for(j=0; j<p->scTrkTakeV[i].midiN; ++j)
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
            "mni",      kIntTId, &p->scTrkTakeV[i].midiV[j].mni,
            "muid",     kIntTId, &p->scTrkTakeV[i].midiV[j].muid,
            "scEvtIdx", kIntTId, &p->scTrkTakeV[i].midiV[j].scEvtIdx,
            "flags",    kIntTId, &p->scTrkTakeV[i].midiV[j].flags,
            NULL)) != kOkJsRC )
      {
        if( jsRC == kNodeNotFoundJsRC && errMsg != NULL )
          rc = cmErrMsg(&p->err,kParseFailTsbRC,"JSON file note record parse failed missing required field:'%s'",errMsg);
        else
          rc = cmErrMsg(&p->err,kParseFailTsbRC,"JSON file note record parse failed.");              

        goto errLabel;
      }
    }
  }

 errLabel:
  if( rc != kOkTsbRC )
    rc = _cmTsbScoreTrkFree(p);

  return rc;
}

// Return the count of score events inside a given marker.
unsigned _cmTsbScoreTrkMarkerEventCount( cmTsb_t* p, unsigned markUid )
{
  unsigned i,j;
  unsigned minScEvtIdx = INT_MAX;
  unsigned maxScEvtIdx = 0;

  for(i=0; i<p->scTrkTakeN; ++i)
    for(j=0; j<p->scTrkTakeV[i].midiN; ++j)
      if( p->scTrkTakeV[i].midiV[j].scEvtIdx != cmInvalidIdx )
      {
        if( p->scTrkTakeV[i].midiV[j].scEvtIdx < minScEvtIdx )
          minScEvtIdx = p->scTrkTakeV[i].midiV[j].scEvtIdx;
        
        if( p->scTrkTakeV[i].midiV[j].scEvtIdx > maxScEvtIdx )
          maxScEvtIdx = p->scTrkTakeV[i].midiV[j].scEvtIdx;
      }

  if( maxScEvtIdx < minScEvtIdx )
    return 0;

  return (maxScEvtIdx - minScEvtIdx) + 1;
  
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

cmTsbRC_t cmTakeSeqBldrLoadTake( cmTakeSeqBldrH_t h, unsigned tlMarkUid, bool overwriteFL )
{
  cmTsbRC_t       rc   = kOkTsbRC;
  cmTsb_t*        p    = _cmTsbHandleToPtr(h);
  cmTlMarker_t*   mark = NULL;
  cmTlMidiFile_t* mf   = NULL;
  cmMidiFileH_t   mfH  = cmMidiFileNullHandle;
  unsigned        scEvtN = 0; 
  cmScEvtTsb_t*   scEvtV = NULL;

  // get a pointer to the time-line marker object
  if((mark = cmTlMarkerObjPtr( p->tlH, cmTimeLineIdToObj( p->tlH, cmInvalidId, tlMarkUid))) == NULL )
  {
    rc = cmErrMsg(&p->err,kInvalidArgTsbRC,"The time-line marker uid '%i' is not valid.",tlMarkUid);
    goto errLabel;
  }

  // get the count of score events in the take marker
  if((scEvtN = _cmTsbScoreTrkMarkerEventCount(p,tlMarkUid)) == 0 )
  {
    rc = cmErrMsg(&p->err,kInvalidArgTsbRC,"The selected take marker does not appear to contain any score events.");
    goto errLabel;
  }

  // allocate a score event array
  scEvtV = cmMemAllocZ(cmScEvtTsb_t,scEvtN);
  

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
  
  // convert the marker beg/end sample position to be relative to the MIDI file start time
  unsigned                 bsi = mark->obj.seqSmpIdx - mf->obj.seqSmpIdx;
  unsigned                 esi = mark->obj.seqSmpIdx + mark->obj.durSmpCnt - mf->obj.seqSmpIdx;
  unsigned                 i   = 0;
  unsigned                 n   = cmMidiFileMsgCount(mfH);
  const cmMidiTrackMsg_t** a   = cmMidiFileMsgArray(mfH);
  
  // seek to the first MIDI msg after sample index bsi in the MIDI file
  for(i=0; i<n; ++i)
    if( a[i]->dtick >= bsi )
      break;

  // if bsi is after the file then the MIDI file finished before the marker
  if( i == n )
  {
    rc = cmErrMsg(&p->err,kInvalidArgTsbRC,"No MIDI events were found in the marker.");
    goto errLabel;
  }

  // for each MIDI message between bsi and esi
  for(; i<n && a[i]->dtick < esi; ++i)
  {
    const cmMidiTrackMsg_t* m = a[i];
    switch( m->status )
    {
      case kNoteOffMdId:
      case kNoteOnMdId:
      case kCtlMdId:
        
        break;
    }
  }


  
 errLabel:
  if( cmMidiFileClose(&mfH) != kOkMfRC )
    rc = cmErrMsg(&p->err,kMidiFileFailTsbRC,"MIDI file close failed.");
  
  if( rc != kOkTsbRC )
  {
    cmMemFree(scEvtV);
  }

  return rc;
}

cmTsbRC_t cmTakeSeqBldrUnloadTake( cmTakeSeqBldrH_t h, unsigned tlMarkUid )
{
  cmTsbRC_t rc = kOkTsbRC;
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
  const cmChar_t*  scoreTrkFn = "/home/kevin/src/cmkc/src/kc/data/assoc0.js";
  cmTakeSeqBldrH_t tsbH  = cmTakeSeqBldrNullHandle;
  cmTsbRC_t        tsbRC = kOkTsbRC;

  if((tsbRC = cmTakeSeqBldrAllocFn(ctx, &tsbH, scoreTrkFn )) != kOkTsbRC )
    return cmErrMsg(&ctx->err,tsbRC,"TSB Allocate and parse '%s' failed.",scoreTrkFn);

  if((tsbRC = cmTakeSeqBldrFree(&tsbH)) != kOkTsbRC )
    return cmErrMsg(&ctx->err,tsbRC,"TSB Free failed.");

  return tsbRC;
}
