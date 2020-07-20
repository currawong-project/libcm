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
#include "cmFile.h"
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
  unsigned          minScEvtIdx;  // min scEvtIdx in midiV[]
  unsigned          maxScEvtIdx;  // max scEvtIdx in midiV[]
  bool              failFl;
} cmScTrkTakeTsb_t;


// 
typedef struct cmMidiTsb_str
{  
  unsigned              rid;       // unique id among all object in this take
  unsigned              srcId;     // marker uid or -1 if this event was manually inserted
  unsigned              scEvtIdx;  // score event assocd with this midi event
  unsigned              flags;     // copy of cmScTrkMidiTsb_t.flags or 0 if not assoc'd with a score event
  struct cmMidiTsb_str* ref;       // previous MIDI event in time
  unsigned              offsetSmp; // time offset from *ref
  unsigned              durSmp;    // duration of this MIDI event
  unsigned              status;    // MIDI status value
  unsigned              d0;        // d0 MIDI channel msg data byte 0.
  unsigned              d1;        // d1 MIDI channel msg data byte 1.
  struct cmMidiTsb_str* link;      // pointer to next MIDI event in list
} cmMidiTsb_t;

// This record contains all the score events and and score synchronized MIDI events
// associated with a given take.  Each call to cmTakeSeqBldrLoadTake() creates
// one of these records.
typedef struct cmTakeTsb_str
{
  unsigned              tlMarkerUid; // time-line marker uid associated with this take
  cmScTrkTakeTsb_t*     stt;         // score tracking info
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

  cmScTrkTakeTsb_t*  scTrkTakeV;  // score tracker file info scTrkTakeV[ scTrkTakeN ]
  unsigned           scTrkTakeN;  //   (one record per take)
  
  cmTakeTsb_t* takes;   // list of loaded takes 
  cmTakeTsb_t* manual;  // list of manually inserted MIDI events
  
  cmTakeTsb_t* out;        // render list
  cmMidiTsb_t* evt;        // next event to play from the render list
  unsigned     absPlaySmp; // absolute sample index of the clock 
  unsigned     absEvtSmp;  // absolute sample index of the next event to play
  unsigned     nextRid;
  cmMidiTsb_t* rend;

} cmTsb_t;

cmTakeSeqBldrH_t cmTakeSeqBldrNullHandle = cmSTATIC_NULL_HANDLE;

cmTsb_t* _cmTsbHandleToPtr( cmTakeSeqBldrH_t h )
{
  cmTsb_t* p = (cmTsb_t*)h.h;
  return p;
}


//-------------------------------------------------------------------------------------------------------------------------

typedef struct cmOrderTsb_str
{
  cmTakeTsb_t*           t;
  unsigned               minScEvtIdx; // take begin
  unsigned               maxScEvtIdx; // take end
  unsigned               begScEvtIdx; // render begin  (render beg/end is always contained
  unsigned               endScEvtIdx; // render end     withing min/maxScEvtIdx)
  struct cmOrderTsb_str* link;
} cmOrderTsb_t;

// Notes:
// 1) begScEvtIdx - endScEvtIdx is always contained within minScEvtIdx-maxScEvtIdx.
// 2) If begScEvtIdx == cmInvalidIdx then the take was entirely contained inside the
//    previous take and is therefore skipped over.


// Free a linked list of cmOrderTsb_t records
void _cmTakeSeqBldrFreeOrder( cmOrderTsb_t* o )
{
  while(o!=NULL)
  {
    cmOrderTsb_t* o0 = o->link;
    cmMemFree(o);
    o = o0;
  }
}


// Create a linked list of cmOrderTsb_t records from the p->takes list.
cmOrderTsb_t*  _cmTakeSeqBldrAllocOrder( cmTsb_t* p )
{
  cmOrderTsb_t* o  = NULL;
  cmOrderTsb_t* o0 = NULL;
  cmTakeTsb_t*  t  = p->takes;

  // create a list of order records - one per take
  for(; t!=NULL; t=t->link)
    if( t->stt != NULL )
    {
      cmOrderTsb_t* o1 = cmMemAllocZ(cmOrderTsb_t,1);
      o1->t            = t;
      o1->minScEvtIdx  = t->stt->minScEvtIdx;
      o1->maxScEvtIdx  = t->stt->maxScEvtIdx;
      o1->begScEvtIdx  = o1->minScEvtIdx;
      o1->endScEvtIdx  = o1->maxScEvtIdx;

      if( o0 == NULL )
        o = o1;
      else
        o0->link = o1;

      o0  = o1;
    }

  return o;
}

// Sort a list of cmOrderTsb_t records on minScEvtIdx.
cmOrderTsb_t* _cmTakeSeqBldrSortOrder( cmOrderTsb_t* o )
{
  cmOrderTsb_t*  beg = NULL;
  cmOrderTsb_t*  end = NULL;
  cmOrderTsb_t*  m   = NULL;

  // while elements remain on the unordered list
  while( o != NULL )
  {
    cmOrderTsb_t* c0 = NULL;
    cmOrderTsb_t* c1 = o->link;

    m  = o;

    // set m to point to the recd with the min minScEvtIdx
    for(; c1!=NULL; c1=c1->link )
      if( c1->minScEvtIdx < m->minScEvtIdx )
        m = c1;
    
    // for each recd on the unordered list
    for(c1=o; c1!=NULL; c1=c1->link)
    {
      // if this is the min record
      if( c1==m )
      {
        // remove the min recd from this list
        if( c0 == NULL )
          o = c1->link;  
        else             
          c0->link = c1->link; 

        c1->link = NULL;

        break;
      }

      c0 = c1;
    }

    
    // c1 now points to the min record
    assert(c1==m);

    if( end == NULL )
      beg = end = m;
    else
    {
      end->link = m;
      end       = m;
    } 
    
  }
  
  return beg;
}

// Set the begScEvtIdx and endScEvtIdx fields to settle conflicts
// between overlapping takes. 
void  _cmTakeSeqBldrSetBegEndOrder( cmOrderTsb_t* o )
{
  if( o == NULL )
    return;

  // Earlier takes have priority over later takes so the first
  // take has highest priority and therefore it's beg=min and end=max.
  o->begScEvtIdx = o->minScEvtIdx;
  o->endScEvtIdx = o->maxScEvtIdx;

  cmOrderTsb_t* o0 = o;
  cmOrderTsb_t* o1 = o->link;

  while( o1!=NULL)
  {    
    bool skipFl = false;

    // if this take begins after the previous take
    if( o1->minScEvtIdx > o0->endScEvtIdx )
    {
      o1->begScEvtIdx = o1->minScEvtIdx;
      o1->endScEvtIdx = o1->maxScEvtIdx;
    } 
    else
    {
      // if this take is entirely contained in the previous take
      // then this take is skipped.
      if( o0->endScEvtIdx > o1->maxScEvtIdx )
      {
        o1->begScEvtIdx = cmInvalidIdx;
        o1->endScEvtIdx = cmInvalidIdx;
        skipFl          = true;
      }
      else // this take overlaps with the previous take
      {
        o1->begScEvtIdx = o0->endScEvtIdx + 1;
        o1->endScEvtIdx = o1->maxScEvtIdx;
      }
    }

    if( skipFl )
    {
      // the current take is  being skipped so do not change o0
      o1 = o1->link; 
    }
    else
    {
      // advance to the next take
      o0 = o1;
      o1 = o0->link;
    }

  }

}


void _cmTakeSeqBldrPrintOrder( cmOrderTsb_t* o )
{
  int i;
  for(i=0; o!=NULL; o=o->link,++i)
    printf("%i : %p min:%i max:%i beg:%i end:%i\n",i, o->t, o->minScEvtIdx, o->maxScEvtIdx, o->begScEvtIdx, o->endScEvtIdx);
  printf("\n");

}

//-------------------------------------------------------------------------------------------------------------------------
typedef struct
{
  cmScoreEvt_t* sep;
  unsigned      absSmp;
} cmTsbTempo_t;

void _cmTsbTempoPrint( cmTsbTempo_t* r, unsigned rN )
{
  unsigned i;
  for(i=0; i<rN; ++i)
    printf("%s %i\n",cmScEvtTypeIdToLabel(r[i].sep->type),r[i].absSmp);
  printf("\n");

}

cmTsbTempo_t* _cmTsbTempoFind( cmTsbTempo_t* r, unsigned rN, unsigned scEvtIdx )
{
  unsigned i;
  for(i=0; i<rN; ++i)
    if( r[i].sep->index == scEvtIdx )
      return r + i;

  return NULL;
}

// Starting at r[ri] calc the tempo for the next two bars.
unsigned _cmTsbTempoCalc( cmTsbTempo_t* r, unsigned rN, unsigned ri, double srate )
{
  unsigned durBarCnt = 2;
  unsigned barIdx    = 0;
  double   beats     = 0;
  unsigned abs0Smp   = -1;
  unsigned abs1Smp   = -1;

  for(; ri<rN; ++ri)
  {
    // if this is a note event
    if( r[ri].sep->type == kNonEvtScId )
    {
      beats += r[ri].sep->frac;

      if( abs0Smp == -1 )
        abs0Smp = r[ri].absSmp;

      if( r[ri].absSmp != -1 )
        abs1Smp = r[ri].absSmp;
    }

    // if this is a bar event
    if( r[ri].sep->type == kBarEvtScId )
    {
      barIdx += 1;

      if( barIdx == durBarCnt )
        break;
    }

   
  }

  double durSmp = abs1Smp - abs0Smp;

  if( durSmp == 0 )
    return 0;

  return (unsigned)round(beats / (durSmp / (srate * 60.0)));
}

cmTsbRC_t _cmTsbCalcTempo( cmTsb_t* p, cmTakeTsb_t* t, unsigned* begBpSRef, unsigned* endBpSRef )
{
  unsigned i,j;

  assert( begBpSRef != NULL && endBpSRef != NULL );

  *begBpSRef = 0;
  *endBpSRef = 0;

  if( t->stt == NULL )
    return cmErrMsg(&p->err,kMissingScTrkTsbRC,"The tempo of takes without score-tracking results cannot be calculated.");

  if( t->midi == NULL )
    return cmErrMsg(&p->err,kInvalidArgTsbRC,"The tempo of a take without MIDI events cannot be estimated.");

  // allocate an array to with one record per score event in the take
  unsigned     rN = t->stt->maxScEvtIdx - t->stt->minScEvtIdx + 1;
  cmTsbTempo_t r[ rN ];

  // assign a score event pointer to each record
  for(i=0; i<rN; ++i)
  {
    r[i].sep    = cmScoreEvt(p->scH,t->stt->minScEvtIdx + i);
    assert( r[i].sep != NULL );

    r[i].absSmp   = -1;
  }

  // use MIDI events with score information to assign absolute
  // time information to as many score events as possible.
  unsigned     absSmp = 0;
  cmMidiTsb_t* m      = t->midi;

  while( m!=NULL )
  {
    cmTsbTempo_t* r0 = NULL;

    if( m->scEvtIdx != cmInvalidIdx && (r0 = _cmTsbTempoFind(r,rN,m->scEvtIdx)) != NULL )
    {
      assert( r0->sep->type == kNonEvtScId );
      r0->absSmp = absSmp;
    }

    m = m->link;
    
    if( m != NULL )
      absSmp += m->offsetSmp;
        
  }

  unsigned barCnt = 0;

  // assign an absolute time to each bar recd
  for(i=0; i<rN; ++i)
    if( r[i].sep->type == kBarEvtScId )
    {
      barCnt += 1;  // count the number of bars found

      // search ahead from the bar to the next note
      unsigned j;
      for(j=i; j<rN; ++j)
        if( r[j].sep->type == kNonEvtScId )
        {
          r[i].absSmp = r[j].absSmp;  // the bar time is the same as the next
          break;
        }
                  
    }


  //_cmTsbTempoPrint(r,rN);

  unsigned tempoV[ barCnt ];
  
  for(i=0,j=0; i<rN && j<barCnt; ++i)
  {    
    tempoV[j++] = _cmTsbTempoCalc(r, rN, i, cmTimeLineSampleRate(p->tlH) );

    for(; i<rN; ++i)
      if( r[i].sep->type == kBarEvtScId )
        break;
  }

  if( j > 0 )
  {
    *begBpSRef = tempoV[0];
    *endBpSRef = tempoV[j-1];
  }


  printf("Bars:%i ",barCnt);
  for(i=0; i<j; ++i)
    printf("%i ",tempoV[i]);
  printf("\n");

  return kOkTsbRC;
    
}


//-------------------------------------------------------------------------------------------------------------------------
void _cmTsbPrintMidi( cmTakeTsb_t* t )
{
  if( t==NULL )
    return;

  cmMidiTsb_t* m = t->midi;
  for(; m!=NULL; m=m->link)
    printf("mark:%4i sei:%4i fl:0x%x offs:%8i dur:%8i\n", m->srcId, m->scEvtIdx,m->flags,m->offsetSmp,m->durSmp);
  printf("\n");
}

void _cmTsbTakePrint( cmTakeTsb_t* t )
{
  for(; t!=NULL; t=t->link)
  {
    printf("Take:%i %p\n", t->tlMarkerUid, t->stt);
    _cmTsbPrintMidi(t);
  }
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
// from p->takes (See _cmTakeTsbUnlink().) prior to calling this function.
void _cmTsbTakeFree( cmTsb_t* p, cmTakeTsb_t** tp )
{
  if( tp==NULL || *tp==NULL )
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

void _cmTsbRendTakeFree( cmTsb_t* p, cmTakeTsb_t** t )
{
  _cmTsbTakeFree(p,t);
  p->evt        = NULL;
  p->absPlaySmp = 0;
  p->absEvtSmp  = 0;
  p->nextRid    = 0;
  p->rend       = NULL;
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
        t0->link = t->link;

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

  //_cmTsbTakePrint(p->takes);
  //_cmTsbTakePrint(p->out);

  cmTakeTsb_t* t = p->takes;
  
  while( t != NULL )
  {
    cmTakeTsb_t* nt = t->link;
    _cmTsbTakeFree(p,&t);
    t = nt;
  }

  _cmTsbRendTakeFree(p,&p->out);

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
    t->minMuid     =  INT_MAX;
    t->maxMuid     =  0;
    t->minScEvtIdx = INT_MAX;
    t->maxScEvtIdx = 0;

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

      unsigned scEvtIdx = t->midiV[j].scEvtIdx;

      if( scEvtIdx!=0 && scEvtIdx!=cmInvalidIdx && scEvtIdx < t->minScEvtIdx )
        t->minScEvtIdx = scEvtIdx;

      if( scEvtIdx!=0 && scEvtIdx!=cmInvalidIdx && scEvtIdx > t->maxScEvtIdx )
        t->maxScEvtIdx = t->midiV[j].scEvtIdx;


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
  unsigned takeCnt = 0;
  unsigned midiCnt = 0;

  // delete the previous output rendering
  _cmTsbRendTakeFree(p,&p->out);

  // allocate a take order list
  cmOrderTsb_t*  o = _cmTakeSeqBldrAllocOrder( p );

  // sort the list by minScEvtIdx
  o = _cmTakeSeqBldrSortOrder(o);

  // assign beg/endScEvtIdx values to each take
  _cmTakeSeqBldrSetBegEndOrder(o);

  //_cmTakeSeqBldrPrintOrder(o);

  // if the render take has not yet been allocated
  p->out              = cmMemAllocZ(cmTakeTsb_t,1);
  p->out->tlMarkerUid = cmInvalidId;
  p->nextRid          = 0;

  cmMidiTsb_t*  m0  = NULL;
  cmOrderTsb_t* t   = o;
  for(; t!=NULL; t=t->link)
  {
    // skip takes whose begScEvtIdx is not valid
    if( t->begScEvtIdx == cmInvalidIdx )
      continue;

    takeCnt += 1;

    //_cmTsbPrintMidi(t->t);

    // advance to the MIDI event assoc'd with t->begScEvtIdx
    cmMidiTsb_t* m = t->t->midi;
    for(; m!=NULL; m=m->link)
      if( m->scEvtIdx >= t->begScEvtIdx )
        break;

    // copy the MIDI events from the take into the render list
    for(; m!=NULL; m=m->link)
    {
      midiCnt += 1;

      // allocate a MIDI record to hold this render event
      cmMidiTsb_t* m1 = cmMemAllocZ(cmMidiTsb_t,1);

      // copy in the MIDI record
      *m1 = *m;
      //memcpy(m1,m,sizeof(cmMidiTsb_t));

      m1->link= NULL;         // NULLify the copied link
      m1->ref = m0;           // set prev. link
      m1->rid = p->nextRid++; // set the unique id 
        
      // if this is not the first MIDI event ...
      if( m0 != NULL )
        m0->link = m1;      // then set next link on the previous record
      else                  //
      {                     // otherwise
        p->out->midi  = m1; //   1) set the render take event list
        m1->offsetSmp = 0;  //   2) the first event always starts at time zero.
        p->evt        = m1; //   3) set the first event to play
        p->absPlaySmp = 0;  //   4) set the current play clock to 0 
        p->absEvtSmp  = 0;  //   5) set the current next play event time to 0
      }

      m0 = m1;              

      // if this is the last event in this take
      if( m0->scEvtIdx == t->endScEvtIdx )
        break;    
    }
  }

  // free the take order list
  _cmTakeSeqBldrFreeOrder(o);

  printf("rendered takes:%i events:%i\n",takeCnt,midiCnt);
  //_cmTsbPrintMidi(p->out);

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
  if( cmMidiFileOpen( &p->ctx, &mfH, cmMidiFileName(mf->h) ) != kOkMfRC )
  {
    rc = cmErrMsg(&p->err,kInvalidArgTsbRC,"The MIDI file '%s' could not be opened.", cmStringNullGuard(cmMidiFileName(mf->h)));
    goto errLabel;
  }

  // convert the dtick field to delta samples
  //cmMidiFileTickToSamples( mfH, cmTimeLineSampleRate(p->tlH), false );
  
  // calculate MIDI note and pedal durations (see cmMidiChMsg_t.durTicks)
  cmMidiFileCalcNoteDurations( mfH );
  
  unsigned                 i     = 0;
  unsigned                 n     = cmMidiFileMsgCount(mfH);
  const cmMidiTrackMsg_t** a     = cmMidiFileMsgArray(mfH);
  double                   srate = cmTimeLineSampleRate(p->tlH);        
  
  // allocate and link a new take render record
  cmTakeTsb_t* t = cmMemAllocZ(cmTakeTsb_t,1);

  t->tlMarkerUid = tlMarkUid;
  t->stt         = stt;
  t->link        = p->takes;
  p->takes       = t;


  unsigned                rid = 0;
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

    // get a pointer to the tracking map for the given MIDI file event.
    // (Note that since control messages are not tracked so this function may return NULL.)
    cmScTrkMidiTsb_t* stm = _cmTsbMuidToScTrkMidi(stt, mf1->uid );

    // create a MIDI render event 
    cmMidiTsb_t* m1 = cmMemAllocZ(cmMidiTsb_t,1);
      
    m1->rid       = rid++;
    m1->srcId     = tlMarkUid;
    m1->scEvtIdx  = stm != NULL ? stm->scEvtIdx : cmInvalidIdx;
    m1->flags     = stm != NULL ? stm->flags    : 0;
    m1->ref       = m0;
    m1->offsetSmp = mf0 == NULL ? 0 : round(mf1->amicro * srate / 1000000.0);
    m1->durSmp    = mf1->u.chMsgPtr->durMicros * srate / 1000000.0;
    m1->d0        = mf1->u.chMsgPtr->d0;
    m1->d1        = mf1->u.chMsgPtr->d1;
    m1->status    = mf1->status;
    m1->link      = NULL;

    //printf("0x%x %f %f\n",m1->status,m1->offsetSmp/96000.0,m1->durSmp/96000.0);

    if( m0 != NULL )
      m0->link = m1;
    
    if( t->midi == NULL )
      t->midi = m1;

    m0  = m1;
    mf0 = mf1;

  }

  //unsigned begBpS, endBpS;
  //_cmTsbCalcTempo(p, t, &begBpS, &endBpS );

  // render the new output sequence
  if((rc =  _cmTakeSeqBldrRender(p)) != kOkTsbRC)
    cmErrMsg(&p->err,rc,"Take sequence builder rendering failed.");

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

  // render the new output sequence
  if((rc =  _cmTakeSeqBldrRender(p)) != kOkTsbRC)
    cmErrMsg(&p->err,rc,"Take sequence builder rendering failed.");
    
  return rc;
}
  

double    cmTakeSeqBldrSampleRate( cmTakeSeqBldrH_t h )
{
  cmTsb_t*  p  = _cmTsbHandleToPtr(h);
  if( cmTimeLineIsValid( p->tlH ) )
    return cmTimeLineSampleRate(p->tlH );
  return 0;
}


cmScH_t   cmTakeSeqBldrScoreHandle( cmTakeSeqBldrH_t h )
{
  cmTsb_t*  p  = _cmTsbHandleToPtr(h);
  return p->scH;
}

unsigned cmTakeSeqBldrScTrkTakeCount( cmTakeSeqBldrH_t h )
{
  cmTsb_t*  p  = _cmTsbHandleToPtr(h);
  return p->scTrkTakeN;
}

cmTsbRC_t cmTakeSeqBldrScTrkTake( cmTakeSeqBldrH_t h, unsigned idx, cmTksbScTrkTake_t* ref )
{
  cmTsb_t*      p      = _cmTsbHandleToPtr(h);

  assert( idx < p->scTrkTakeN );

  ref->minScEvtIdx = p->scTrkTakeV[ idx ].minScEvtIdx;
  ref->maxScEvtIdx = p->scTrkTakeV[ idx ].maxScEvtIdx;
  ref->tlMarkerUid = p->scTrkTakeV[idx].tlMarkerUid;
  
  return kOkTsbRC;
}

const cmChar_t* cmTakeSeqBldrScTrkTakeText( cmTakeSeqBldrH_t h, unsigned tlMarkerUid )
{
  cmTsb_t*      p      = _cmTsbHandleToPtr(h);
  cmTlObj_t*    tlObj  = NULL;
  cmTlMarker_t* tlMark = NULL;

  if((tlObj = cmTimeLineIdToObj( p->tlH, cmInvalidId, tlMarkerUid )) == NULL )
  {
    cmErrMsg(&p->err,kTimeLineFailTsbRC,"Unable to locate the time line record for score-track-take uid: %i.",tlMarkerUid);
    return NULL;
  }

  if((tlMark = cmTimeLineMarkerObjPtr( p->tlH, tlObj)) == NULL )
  {
    cmErrMsg(&p->err,kTimeLineFailTsbRC,"Unable to cast a time line marker object for score-track-take %i.",tlMarkerUid);
    return NULL;
  }

  return tlMark->text;
}


cmTsbRC_t cmTakeSeqBldrPlaySeekLoc( cmTakeSeqBldrH_t h, unsigned scLocIdx )
{
  cmTsb_t* p = _cmTsbHandleToPtr(h);
  
  if( p->out == NULL )
    return cmErrMsg(&p->err,kRenderSeqEmptyTsbRC,"Seek faied. The render sequence is empty.");

  if( scLocIdx == cmInvalidIdx )
    p->evt = p->out->midi;
  else
  {

    cmScoreLoc_t* loc;
    if(( loc = cmScoreLoc(p->scH, scLocIdx )) == NULL )
      return cmErrMsg(&p->err,kInvalidArgTsbRC,"Seek failed. The requested score location (%i) could not be found.",scLocIdx);

    if( loc->evtCnt == 0 || loc->evtArray[0]==NULL )
      return cmErrMsg(&p->err,kInvalidArgTsbRC,"Seek failed. The requested score location (%i) has no associated event.",scLocIdx);

    unsigned scEvtIdx = loc->evtArray[0]->index;

    cmMidiTsb_t* m = p->out->midi;
    for(; m!=NULL; m=m->link)
      if( m->scEvtIdx >= scEvtIdx )
      {
        p->evt = m;
        break;
      }

    if( m == NULL )
      return cmErrMsg(&p->err,kInvalidArgTsbRC,"Seek failed. The requested score event index (%i) is out of range.",scEvtIdx );
  }

  return kOkTsbRC;  
}

cmTsbRC_t cmTakeSeqBldrPlayExec( cmTakeSeqBldrH_t h, unsigned deltaSmp, cmTakeSeqBldrPlayFunc_t cbFunc, void* cbArg )
{  
  cmTsb_t* p = _cmTsbHandleToPtr(h);

  if( p->evt == NULL )
    return kOkTsbRC;

  // advance the play clock
  p->absPlaySmp += deltaSmp;

  // if the next event time has elapsed
  while( p->evt != NULL && p->absEvtSmp <= p->absPlaySmp )
  {
    // make the event callback
    cmTksbEvent_t e;
    e.smpIdx = p->absEvtSmp;
    e.status = p->evt->status;
    e.d0     = p->evt->d0;
    e.d1     = p->evt->d1;

    cbFunc(cbArg,&e);
        
    do
    {
      // advance the current play event
      p->evt = p->evt->link;

      // the last event was not encountered and the events offset time is legal
      if( p->evt != NULL && p->evt->offsetSmp != cmInvalidIdx )
      {
        // advance the event absolute time 
        p->absEvtSmp += p->evt->offsetSmp;
        break;
      }

    }while( p->evt != NULL );

  }

  return kOkTsbRC;
}

void cmTakeSeqBldrRendReset( cmTakeSeqBldrH_t h )
{
  cmTsb_t* p = _cmTsbHandleToPtr(h);

  if( p->out == NULL )
    return;

  p->rend = p->out->midi;
}

void _cmTakeSeqBldrMidiToRend( cmTksbRend_t* r, const cmMidiTsb_t* m )
{
  r->rid        = m->rid;
  r->srcId      = m->srcId;
  r->scEvtIdx   = m->scEvtIdx;
  r->flags      = m->flags;
  r->offsetSmp  = m->offsetSmp;
  r->durSmp     = m->durSmp;
  r->evt.smpIdx = 0;
  r->evt.status = m->status;
  r->evt.d0     = m->d0;
  r->evt.d1     = m->d1;
}

cmMidiTsb_t* _cmTakeSeqBldrRidToMidi( cmTsb_t* p, unsigned rid )
{
  cmMidiTsb_t* m;
  if( p->out == NULL )
    return NULL;

  for(m=p->out->midi; m!=NULL; m=m->link)
    if( m->rid == rid )
      return m;

  return NULL;
}

bool cmTakeSeqBldrRendNext(cmTakeSeqBldrH_t h, cmTksbRend_t* r)
{
  cmTsb_t*     p = _cmTsbHandleToPtr(h);
  cmMidiTsb_t* m = p->rend;

  if( m == NULL )
    return false;

  p->rend = p->rend->link;
  
  _cmTakeSeqBldrMidiToRend(r,m);

  return true;
}

cmTsbRC_t cmTakeSeqBldrRendInfo( cmTakeSeqBldrH_t h, unsigned rid, cmTksbRend_t* r )
{
  cmTsb_t*     p = _cmTsbHandleToPtr(h);
  cmMidiTsb_t* m;

  if((m = _cmTakeSeqBldrRidToMidi(p, rid )) == NULL )
    return cmErrMsg(&p->err,kInvalidArgTsbRC,"Unable to locate the MIDI render record associated with rid:%i.",rid);

  _cmTakeSeqBldrMidiToRend(r,m);
  return kOkTsbRC;
}

cmTsbRC_t cmTakeSeqBldrRendDelete( cmTakeSeqBldrH_t h, unsigned rid )
{
  cmTsb_t*     p = _cmTsbHandleToPtr(h);

  if( p->out == NULL || p->out->midi == NULL )
    return kOkTsbRC;

  cmMidiTsb_t* m1 = p->out->midi;
  
  for(; m1!=NULL; m1=m1->link)
    if( m1->rid == rid )
    {
      // if there is no recd before m1
      if( m1->ref == NULL )
      {
        p->out->midi = m1->link;

        // if there is a record after m1
        if( p->out->midi != NULL )
        {
          p->out->midi->ref = NULL;
          p->out->midi->offsetSmp = 0; // the first record never has a time offset
        }
      }
      else  // there is a record before m1
      {
        m1->ref->link = m1->link;

        // if there is a record after m1
        if( m1->link != NULL )
        {
          m1->link->ref  = m1->ref;
          m1->link->offsetSmp += m1->offsetSmp; // absorb m1's offset time into the next revent
        }
      }

      cmMemFree(m1);

      break;
    }

  _cmTsbTakePrint(p->out);

  return kOkTsbRC;
  
}

cmTsbRC_t cmTakeSeqBldrRendInsert( cmTakeSeqBldrH_t h, const cmTksbEvent_t* e, unsigned durSmp, unsigned* ridRef)
{
  cmTsb_t*     p = _cmTsbHandleToPtr(h);

  if( ridRef != NULL )
    *ridRef = cmInvalidId;

  cmMidiTsb_t* nm = cmMemAllocZ(cmMidiTsb_t,1);

  nm->rid      = p->nextRid++;
  nm->srcId    = cmInvalidId;
  nm->scEvtIdx = cmInvalidId;
  nm->flags    = 0;
  nm->durSmp   = durSmp;
  nm->status   = e->status;
  nm->d0       = e->d0;
  nm->d1       = e->d1;
  
  if( p->out == NULL )
  {
    p->out         = cmMemAllocZ(cmTakeTsb_t,1);
    p->out->tlMarkerUid = cmInvalidId;    
  }

  if( p->out->midi == NULL )
  {
    p->out->midi = nm;
    goto doneLabel;
  }

  cmMidiTsb_t* m0        = NULL;
  cmMidiTsb_t* m1        = p->out->midi;
  unsigned     absSmpIdx = m1==NULL ? 0 : m1->offsetSmp;

  for(; m1!=NULL; m1=m1->link)
  {
    // absSmpIdx is the absolute time of m1

    // if m1 is the recd just after the new record
    if( absSmpIdx > e->smpIdx )
    {

      // the record prior to the new record is m0
      nm->ref       = m0;

      // the reocrd after the new record is m1
      nm->link      = m1;

      // the new record is before m1
      m1->ref       = nm;

      // if the new record is first on the list
      if( m0 == NULL )
      {
        p->out->midi = nm;

        // TODO: without giving more information there is no way
        // to give the old first event an offset relative to the new
        // first event - so the both events will be scheduled at
        // time zero.

        nm->offsetSmp = 0;

      }
      else // the new record is between m0 and m1
      {
        m0->link = nm;

        // offset from new record to m1
        unsigned dsi = absSmpIdx - e->smpIdx;

        // m1's time offset is being reduced
        assert( m1->offsetSmp >= dsi );

        // the offset to the new record from m0
        nm->offsetSmp = m1->offsetSmp - dsi;

        m1->offsetSmp = dsi;
      }

      break;
    }

    
    // if m1 is not the last element on the list
    if( m1->link != NULL )
      absSmpIdx += m1->link->offsetSmp;
    else
    {
      // insert the new event at the end of the list
      nm->ref = m1;
      nm->link = NULL;
      m1->link = nm;
      assert( e->smpIdx > absSmpIdx );
      nm->offsetSmp = e->smpIdx - absSmpIdx; 

      break;
    }

    m0 = m1;
  }
  
  
 doneLabel:
  if( ridRef != NULL )
    *ridRef = nm->rid;

  //_cmTsbTakePrint(p->out);

  return kOkTsbRC;
}


cmTsbRC_t cmTakeSeqBldrWrite( cmTakeSeqBldrH_t h, const cmChar_t* fn )
{
  cmTsbRC_t     rc  = kOkTsbRC;
  cmJsonH_t     jsH = cmJsonNullHandle;
  cmJsonNode_t* arr;
  cmMidiTsb_t*  m;
  cmTsb_t*      p   = _cmTsbHandleToPtr(h);

  if( p->out == NULL )
    return rc;
  
  // allocate a JSON tree
  if( cmJsonInitialize(&jsH,&p->ctx) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailTsbRC,"JSON write tree allocate failed.");
    goto errLabel;
  }

  // insert the root object
  if( cmJsonCreateObject(jsH, NULL ) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailTsbRC,"JSON root object allocate failed.");
    goto errLabel;
  }
  
  // create the header record
  if(  cmJsonInsertPairs(jsH, cmJsonRoot(jsH), 
        "tlMarkerUid", kIntTId,    p->out->tlMarkerUid,
        "tlFileName",  kStringTId, cmTimeLineFileName(p->tlH),        
                                     NULL) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailTsbRC,"JSON header record create failed.");
    goto errLabel;
  }

  // create the MIDI event array
  if((arr = cmJsonInsertPairArray(jsH, cmJsonRoot(jsH), "midi")) == NULL )
  {
    rc = cmErrMsg(&p->err,kJsonFailTsbRC,"Create the MIDI event array.");
    goto errLabel;
  }

  // fill the MIDI array
  for(m=p->out->midi; m!=NULL; m=m->link)
  {
    if( cmJsonCreateFilledObject(jsH, arr, 
        "rid",       kIntTId, m->rid,
        "srcId",     kIntTId, m->srcId,
        "scEvtIdx",  kIntTId, m->scEvtIdx,
        "flags",     kIntTId, m->flags,
        "offsetSmp", kIntTId, m->offsetSmp,
        "durSmp",    kIntTId, m->durSmp,
        "status",    kIntTId, m->status,
        "d0",        kIntTId, m->d0,
        "d1",        kIntTId, m->d1,          
        NULL) == NULL )
    {
      rc = cmErrMsg(&p->err,kJsonFailTsbRC,"JSON MIDI record create failed.");
      goto errLabel;
    }
  }
  
  // write the tree
  if( cmJsonWrite( jsH, cmJsonRoot(jsH), fn ) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailTsbRC,"JSON write to '%s' failed.",cmStringNullGuard(fn));
  }

  
 errLabel:
  if( cmJsonFinalize(&jsH) != kOkJsRC )
    rc = cmErrMsg(&p->err,kJsonFailTsbRC,"JSON tree finalize failed.");

  return rc;
}


cmTsbRC_t cmTakeSeqBldrRead( cmTakeSeqBldrH_t h, const cmChar_t* fn )
{
  cmTsbRC_t     rc         = kOkTsbRC;
  cmTsb_t*      p          = _cmTsbHandleToPtr(h);
  cmJsonH_t     jsH        = cmJsonNullHandle;
  const cmChar_t*     jsErrLabel = NULL;
  cmTakeTsb_t*  t          = NULL;
  cmMidiTsb_t*  m0         = NULL;
  cmJsonNode_t* arr        = NULL;
  cmJsRC_t      jsRC;

  if( cmJsonInitializeFromFile( &jsH, fn, &p->ctx ) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailTsbRC,"Unable to parse the JSON file '%s'.",cmStringNullGuard(fn));
    goto errLabel;
  }

  t = cmMemAllocZ(cmTakeTsb_t,1);
    
  if((jsRC = cmJsonMemberValues( cmJsonRoot(jsH), &jsErrLabel, 
        "tlMarkerUid", kIntTId,    &t->tlMarkerUid,
        "midi",        kArrayTId,  &arr,
        NULL)) != kOkJsRC )
  {    
    if( jsRC == kNodeNotFoundJsRC && jsErrLabel != NULL )
      rc = cmErrMsg(&p->err,kParseFailTsbRC,"JSON file MIDI render header failed missing required field:'%s'",cmStringNullGuard(jsErrLabel));
    else
      rc = cmErrMsg(&p->err,kParseFailTsbRC,"JSON file MIDI render header parse failed.");              

    goto errLabel;
  }

  unsigned n = cmJsonChildCount(arr);
  unsigned i;

  for(i=0; i<n; ++i)
  {
    const cmJsonNode_t* e = cmJsonArrayElementC(arr, i );
    cmMidiTsb_t*        m = cmMemAllocZ(cmMidiTsb_t,1);

    if((jsRC = cmJsonMemberValues( e, &jsErrLabel, 
          "rid",       kIntTId, &m->rid,
          "srcId",     kIntTId, &m->srcId,
          "scEvtIdx",  kIntTId, &m->scEvtIdx,
          "flags",     kIntTId, &m->flags,
          "offsetSmp", kIntTId, &m->offsetSmp,
          "durSmp",    kIntTId, &m->durSmp,
          "status",    kIntTId, &m->status,
          "d0",        kIntTId, &m->d0,
          "d1",        kIntTId, &m->d1,          
          NULL)) != kOkJsRC )
    {    
      if( jsRC == kNodeNotFoundJsRC && jsErrLabel != NULL )
        rc = cmErrMsg(&p->err,kParseFailTsbRC,"JSON file MIDI render element failed missing required field:'%s' on index:%i",cmStringNullGuard(jsErrLabel),i);
      else
        rc = cmErrMsg(&p->err,kParseFailTsbRC,"JSON file MIDI render element parse failed on index: %i.", i);              

      goto errLabel;
    }

    m->ref = m0;

    if( m0 == NULL )
      t->midi = m;
    else
      m0->link = m;    
  }


 errLabel:
  if( cmJsonFinalize(&jsH) != kOkJsRC )
    rc = cmErrMsg(&p->err,kJsonFailTsbRC,"JSON finalize failed.");

  if( rc != kOkTsbRC )
    _cmTsbTakeFree(p,&t);
  else
  {
    _cmTsbRendTakeFree(p,&p->out);
    p->out = t;
  }
  
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
