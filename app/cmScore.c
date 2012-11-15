#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmMidi.h"
#include "cmLex.h"
#include "cmCsv.h"
#include "cmMidiFile.h"
#include "cmAudioFile.h"
#include "cmTimeLine.h"
#include "cmScore.h"

/*
#include "cmComplexTypes.h"
#include "cmLinkedHeap.h"
#include "cmSymTbl.h"
#include "cmProcObj.h"
#include "cmProc.h"
#include "cmProcTemplate.h"
*/

#include "cmVectOpsTemplateMain.h"

cmScH_t cmScNullHandle  = cmSTATIC_NULL_HANDLE;

enum
{
  kLabelCharCnt = 7,

  kInvalidDynScId = 0,

};

enum
{
  kMidiFileIdColScIdx= 0,  
  kTypeLabelColScIdx = 3,
  kDSecsColScIdx     = 4,
  kSecsColScIdx      = 5,
  kPitchColScIdx     = 11,
  kBarColScIdx       = 13,
  kSkipColScIdx      = 14,
  kEvenColScIdx      = 15,
  kTempoColScIdx     = 16,
  kDynColScIdx       = 17
};

typedef struct
{
  unsigned id;
  cmChar_t label[ kLabelCharCnt + 1 ];
} cmScEvtRef_t; 


typedef struct
{
  cmErr_t       err;
  cmScoreEvt_t* array;
  unsigned      cnt;
  cmCsvH_t      cH;
  cmScCb_t      cbFunc;
  void*         cbArg;
  cmChar_t*     fn;
  cmScoreLoc_t* loc;
  unsigned      locCnt;
} cmSc_t;

cmScEvtRef_t _cmScEvtRefArray[] = 
{
  { kTimeSigEvtScId, "tsg" },
  { kKeySigEvtScId,  "ksg" },
  { kTempoEvtScId,   "tmp" },
  { kTrackEvtScId,   "trk" },
  { kTextEvtScId,    "txt" },
  { kEOTrackEvtScId, "eot" },
  { kCopyEvtScId,    "cpy"},
  { kBlankEvtScId,   "blk"},
  { kBarEvtScId,     "bar"},
  { kPgmEvtScId,     "pgm" },
  { kCtlEvtScId,     "ctl" },
  { kNonEvtScId,     "non" },
  { kInvalidEvtScId, "***" }
};

cmScEvtRef_t _cmScDynRefArray[] = 
{
  { 1, "pppp" },
  { 2, "ppp" },
  { 3, "pp"  },
  { 4, "p"   },
  { 5, "mp"  },
  { 6, "m"   },
  { 7, "mf"  },
  { 8, "f"   },
  { 9, "ff"  },
  { 10, "fff" },
  { 11, "ffff"},
  { kInvalidDynScId, "***" },
};

cmSc_t* _cmScHandleToPtr( cmScH_t h )
{ 
  cmSc_t* p = (cmSc_t*)h.h;
  assert( p != NULL );
  return p;
}

unsigned _cmScEvtTypeLabelToId( const cmChar_t* label )
{
  cmScEvtRef_t* r = _cmScEvtRefArray;
  for(; r->id != kInvalidEvtScId; ++r )
    if( strcmp(label,r->label) == 0 )
      return r->id;
  return kInvalidEvtScId;
}

const cmChar_t* cmScEvtTypeIdToLabel( unsigned id )
{
  cmScEvtRef_t* r = _cmScEvtRefArray;
  for(; r->id != kInvalidEvtScId; ++r )
    if( r->id == id )
      return r->label;
  return NULL;
}

unsigned _cmScDynLabelToId( const cmChar_t* label )
{
  cmScEvtRef_t* r = _cmScDynRefArray;
  for(; r->id != kInvalidEvtScId; ++r )
    if( strcmp(label,r->label) == 0 )
      return r->id;
  return kInvalidDynScId;
}

const cmChar_t* cmScDynIdToLabel( unsigned id )
{
  cmScEvtRef_t* r = _cmScDynRefArray;
  for(; r->id != kInvalidDynScId; ++r )
    if( r->id == id )
      return r->label;
  return NULL;
}

unsigned _cmScLexSciPitchMatcher( const cmChar_t* cp, unsigned cn )
{
  // first char must be "A-G"
  if( strspn(cp,"ABCDEFG") != 1 )
    return 0;

  unsigned i = 1;

  // next char could be accidental
  if( cp[i] == '#' || cp[i] == 'b' )
    ++i; // i==2

  // the 2nd or 3rd char must be a digit
  if( isdigit(cp[i]) == false )
    return 0;

  ++i;  // i==2 or i==3

  // the 3rd or 4th char must be a digit or EOS
  if( isdigit(cp[i]) == false )
    return i;
  
  ++i;

  return i;
  
}

cmScRC_t _cmScFinalize( cmSc_t* p )
{
  cmScRC_t rc = kOkScRC;

  if( cmCsvFinalize(&p->cH) != kOkCsvRC )
    return rc;

  unsigned i;
  for(i=0; i<p->locCnt; ++i)
    cmMemFree(p->loc[i].evtArray);
  cmMemFree(p->loc);

  cmMemFree(p->fn);
  cmMemFree(p->array);
  cmMemFree(p);
  return rc;
}

cmScRC_t _cmScParseBar( cmSc_t* p, unsigned rowIdx, cmScoreEvt_t* s, int* barNumb )
{
  if((*barNumb = cmCsvCellInt(p->cH,rowIdx,kBarColScIdx)) == INT_MAX )
    return cmErrMsg(&p->err,kSyntaxErrScRC,"Unable to parse the bar number.");

  s->type    = kBarEvtScId;
  s->secs   = 0;
  s->barNumb = *barNumb;
  return kOkScRC;
}

cmScRC_t _cmScParseNoteOn( cmSc_t* p, unsigned rowIdx, cmScoreEvt_t* s, int barNumb, unsigned barNoteIdx )
{
  cmScRC_t        rc     = kOkScRC;
  unsigned        flags  = 0;
  unsigned        dynVal = kInvalidDynScId;
  const cmChar_t* sciPitch;
  cmMidiByte_t    midiPitch;
  const cmChar_t* attr;
  double          secs;
  double          durSecs;

  if((sciPitch = cmCsvCellText(p->cH,rowIdx,kPitchColScIdx)) == NULL )
    return cmErrMsg(&p->err,kSyntaxErrScRC,"Expected a scientific pitch value");
          
  if((midiPitch = cmSciPitchToMidi(sciPitch)) == kInvalidMidiPitch)
   return cmErrMsg(&p->err,kSyntaxErrScRC,"Unable to convert the scientific pitch '%s' to a MIDI value. ");

  // it is possible that note delta-secs field is empty - so default to 0
  if((secs =  cmCsvCellDouble(p->cH, rowIdx, kSecsColScIdx )) == DBL_MAX) // Returns DBL_MAX on error.
    flags += kInvalidScFl;

  if((attr = cmCsvCellText(p->cH,rowIdx,kSkipColScIdx)) != NULL && *attr == 's' )
    flags += kSkipScFl;

  if((attr = cmCsvCellText(p->cH,rowIdx,kEvenColScIdx)) != NULL && *attr == 'e' )
    flags += kEvenScFl;

  if((attr = cmCsvCellText(p->cH,rowIdx,kTempoColScIdx)) != NULL && *attr == 't' )
    flags += kTempoScFl;
          
  if((attr = cmCsvCellText(p->cH,rowIdx,kDynColScIdx)) != NULL )
  {
    if((dynVal = _cmScDynLabelToId(attr)) == kInvalidDynScId )
      return cmErrMsg(&p->err,kSyntaxErrScRC,"Unknown dynamic label '%s'.",cmStringNullGuard(attr));

    flags += kDynScFl;
  }

  // Returns DBL_MAX on error.
  if((durSecs =  cmCsvCellDouble(p->cH, rowIdx, kDSecsColScIdx )) == DBL_MAX) 
    durSecs = 0.25;


  s->type       = kNonEvtScId;
  s->secs       = secs;
  s->pitch      = midiPitch;
  s->flags      = flags;
  s->dynVal     = dynVal; 
  s->barNumb    = barNumb;
  s->barNoteIdx = barNoteIdx;
  s->durSecs    = durSecs;
  return rc;
}

cmScRC_t _cmScParseFile( cmSc_t* p, cmCtx_t* ctx, const cmChar_t* fn )
{
  cmScRC_t rc = kOkScRC;
  unsigned barNoteIdx = 0;
  int      barEvtIdx = cmInvalidIdx;
  int      barNumb   = 0;
  double   secs;
  double   cur_secs = 0;

  if( cmCsvInitialize(&p->cH, ctx ) != kOkCsvRC )
  {
    rc = cmErrMsg(&p->err,kCsvFailScRC,"Score file initialization failed.");
    goto errLabel;
  }

  if( cmCsvLexRegisterMatcher(p->cH, cmCsvLexNextAvailId(p->cH), _cmScLexSciPitchMatcher ) != kOkCsvRC )
  {
    rc = cmErrMsg(&p->err,kCsvFailScRC,"CSV token matcher registration failed.");
    goto errLabel;
  }

  if( cmCsvParseFile(p->cH, fn, 0 ) != kOkCsvRC )
  {
    rc = cmErrMsg(&p->err,kCsvFailScRC,"CSV file parsing failed on the file '%s'.",cmStringNullGuard(fn));
    goto errLabel;
  }

  p->cnt   = cmCsvRowCount(p->cH);
  p->array = cmMemAllocZ(cmScoreEvt_t,p->cnt);

  unsigned i,j;

  // skip labels line - start on line 1
  for(i=1,j=0; i<p->cnt && rc==kOkScRC; ++i)
  {
    // get the row 'type' label
    const char* typeLabel;
    if((typeLabel = cmCsvCellText(p->cH,i,kTypeLabelColScIdx)) == NULL )
    {
      rc = cmErrMsg(&p->err,kSyntaxErrScRC,"No type label.");
      break;
    }

    // convert the row 'type' label to an id
    unsigned tid;
    if((tid =  _cmScEvtTypeLabelToId(typeLabel)) == kInvalidEvtScId) 
    {
      rc = cmErrMsg(&p->err,kSyntaxErrScRC,"Unknown type '%s'.",cmStringNullGuard(typeLabel));
      break;
    }

    secs = DBL_MAX;

    switch(tid)
    {
      case kBarEvtScId: // parse bar lines        
        if((rc = _cmScParseBar(p,i,p->array+j,&barNumb)) == kOkScRC )
        {
          barNoteIdx = 0;
          barEvtIdx  = j;
          p->array[j].index = j;
          ++j;
        }
        break;

      case kNonEvtScId:  // parse note-on events
        if((rc =  _cmScParseNoteOn(p, i, p->array + j, barNumb, barNoteIdx )) == kOkScRC )
        {
          secs =  p->array[j].secs;

          if( p->array[j].secs == DBL_MAX )
            p->array[j].secs = cur_secs;

          if( cmIsFlag(p->array[j].flags,kSkipScFl) == false )
          {
            p->array[j].index = j;
            ++j;
          }

          ++barNoteIdx;
        }
        break;

      default:
        // Returns DBL_MAX on error.
        secs =  cmCsvCellDouble(p->cH, i, kSecsColScIdx );
        break;
    }
    
    if( secs != DBL_MAX )
      cur_secs = secs;

    // the bar lines don't have times so set the time of the bar line to the
    // time of the first event in the bar.
    if( barEvtIdx != cmInvalidIdx && secs != DBL_MAX )
    {
      assert( p->array[ barEvtIdx ].type == kBarEvtScId );
      p->array[ barEvtIdx ].secs = secs;

      // handle the case where the previous bar had no events
      if( p->array[ barEvtIdx-1].type == kBarEvtScId )
        p->array[ barEvtIdx-1].secs = secs;

      barEvtIdx = cmInvalidIdx;
    }
    
  }

  if( rc == kSyntaxErrScRC )
  {
    cmErrMsg(&p->err,rc,"Syntax error on line %i in '%s'.",i+1,cmStringNullGuard(fn));           
    goto errLabel;
  }

  p->cnt = i;
  
 errLabel:

  return rc;
}

// This function does not currently work because there is no
// guarantee that all the time values (secs field) have been filled in 
/// with valid times and that all event records have a valid 'type' id.
cmScRC_t _cmScoreInitLocArray( cmSc_t* p )
{
  cmScRC_t rc       = kOkScRC;
  double   minDSecs = 0;
  unsigned barNumb  = 0;

  if( p->cnt==0)
    return rc;

  p->locCnt = 1;

  // count the number of unique time locations in the score
  int i,j,k;
  double secs = p->array[0].secs;
  for(i=0; i<p->cnt; ++i)
  {
    assert( p->array[i].secs >= secs );

    if( p->array[i].secs - secs <= minDSecs )
    {
      p->locCnt += 1;
      secs = p->array[i].secs;
    }
  }

  // allocate the loc. array
  p->loc = cmMemAllocZ(cmScoreLoc_t,p->locCnt);

  

  // fill in the location array
  for(i=0,k=0; i<p->cnt; ++k)
  {
    j = i+1;

    assert(p->array[j].secs > p->array[i].secs );

    // get the count of events at this location
    while( j<p->cnt && p->array[j].secs - p->array[i].secs < minDSecs )
      ++j;

    assert(k<p->locCnt);

    p->loc[k].evtCnt   = j-i;
    p->loc[k].evtArray = cmMemAllocZ(cmScoreEvt_t*,p->loc[k].evtCnt);

    // fill in the location record event pointers
    for(j=0; j<p->loc[k].evtCnt; ++j)
    {
      p->loc[k].evtArray[j] = p->array + (i + j);

      if( p->array[i+j].type == kBarEvtScId )
        barNumb = p->array[i+j].barNumb;
    }

    // fill in the location record
    p->loc[k].secs     = p->array[i].secs;
    p->loc[k].evtIdx   = i;
    p->loc[k].barNumb  = barNumb;

    i += p->loc[k].evtCnt;

  }

  return rc;
}

cmScRC_t cmScoreInitialize( cmCtx_t* ctx, cmScH_t* hp, const cmChar_t* fn, cmScCb_t cbFunc, void* cbArg )
{
  cmScRC_t rc = kOkScRC;
  if((rc = cmScoreFinalize(hp)) != kOkScRC )
    return rc;

  cmSc_t* p = cmMemAllocZ(cmSc_t,1);

  cmErrSetup(&p->err,&ctx->rpt,"Score");

  if((rc = _cmScParseFile(p,ctx,fn)) != kOkScRC )
    goto errLabel;

  // See note at function
  //if((rc = _cmScoreInitLocArray(p)) != kOkScRC )
  //  goto errLabel;

  p->cbFunc = cbFunc;
  p->cbArg  = cbArg;
  p->fn     = cmMemAllocStr(fn);

  hp->h = p;

 errLabel:
  if( rc != kOkScRC )
    _cmScFinalize(p);

  return rc;
}

cmScRC_t cmScoreFinalize( cmScH_t* hp )
{
  cmScRC_t rc = kOkScRC;

  if( hp == NULL || cmScoreIsValid(*hp) == false )
    return kOkScRC;

  cmSc_t* p = _cmScHandleToPtr(*hp);

  if((rc = _cmScFinalize(p)) != kOkScRC )
    return rc;
  
  hp->h = NULL;
  
  return rc;
}

const cmChar_t* cmScoreFileName( cmScH_t h )
{
  cmSc_t* p = _cmScHandleToPtr(h);
  return p->fn;
}

bool     cmScoreIsValid( cmScH_t h )
{ return h.h != NULL; }

unsigned      cmScoreEvtCount( cmScH_t h )
{
  cmSc_t* p = _cmScHandleToPtr(h);
  return p->cnt;
}

cmScoreEvt_t* cmScoreEvt( cmScH_t h, unsigned idx )
{
  cmSc_t* p = _cmScHandleToPtr(h);
  if( idx >= p->cnt )
  {
    cmErrMsg(&p->err,kInvalidIdxScRC,"%i is an invalid index for %i records.",idx,p->cnt);
    return NULL;
  }
  return p->array + idx;
}

unsigned      cmScoreLocCount( cmScH_t h )
{
  cmSc_t* p = _cmScHandleToPtr(h);
  return p->locCnt;
}

cmScoreLoc_t* cmScoreLoc( cmScH_t h, unsigned idx )
{
  cmSc_t* p = _cmScHandleToPtr(h);
  if( idx >= p->locCnt )
  {
    cmErrMsg(&p->err,kInvalidIdxScRC,"%i is an invalid index for %i location records.",idx,p->locCnt);
    return NULL;
  }
  return p->loc + idx;
}


cmScRC_t      cmScoreSeqNotify( cmScH_t h )
{
  cmScRC_t  rc = kOkScRC;
  cmSc_t*   p  = _cmScHandleToPtr(h);
  cmScMsg_t m;
  unsigned  i;

  if( p->cbFunc != NULL )
  {
    memset(&m.evt,0,sizeof(m.evt));
    m.typeId = kBeginMsgScId;
    p->cbFunc(p->cbArg,&m,sizeof(m));

    m.typeId = kEventMsgScId;
    for(i=0; i<p->cnt; ++i)
    {
      m.evt = p->array[i];
      p->cbFunc(p->cbArg,&m,sizeof(m));
    }

    memset(&m.evt,0,sizeof(m.evt));
    m.typeId = kEndMsgScId;
    p->cbFunc(p->cbArg,&m,sizeof(m));

  }
  return rc;
}

cmScRC_t      cmScoreDecode( const void* msg, unsigned msgByteCnt, cmScMsg_t* m)
{
  cmScMsg_t* mp = (cmScMsg_t*)msg;
  *m = *mp;
  return kOkScRC;
}


void cmScorePrint( cmScH_t h, cmRpt_t* rpt )
{
  cmSc_t* p = _cmScHandleToPtr(h);
  unsigned i;
  for(i=0; i<20 /*p->cnt*/; ++i)
  {
    cmScoreEvt_t* r = p->array + i;
    switch(r->type)
    {
      case kNonEvtScId:
        cmRptPrintf(rpt,"%5i %3i %3i %s 0x%2x %c%c%c %s\n",
          i,
          r->barNumb,
          r->barNoteIdx,
          cmScEvtTypeIdToLabel(r->type),
          r->pitch,
          cmIsFlag(r->flags,kEvenScFl)  ? 'e' : ' ',
          cmIsFlag(r->flags,kTempoScFl) ? 't' : ' ',
          cmIsFlag(r->flags,kDynScFl)   ? 'd' : ' ',
          cmIsFlag(r->flags,kDynScFl)   ? cmScDynIdToLabel(r->dynVal) : "");          
        break;

      default:
        break;
    }
  }
}

// Each time line note-on object is decorated (via cmTlObj_t.userDataPtr) with a
// cmScSyncState_t record.  
typedef struct 
{
  unsigned cnt;        // count of candidate sync locations
  double   dist;       // edit distance to the closest sync location
  unsigned scEvtIdx;   // score record this note-on is assigned to
} cmScSyncState_t;

void _cmScSyncTimeLineAllocFree( cmTlH_t tlH, bool allocFl )
{
  cmTlMidiEvt_t* mep = cmTlNextMidiEvtObjPtr(tlH,NULL,cmInvalidId);
  
  for(; mep != NULL; mep = cmTlNextMidiEvtObjPtr(tlH,&mep->obj,cmInvalidId))
    if( mep->msg->status == kNoteOnMdId )
    {
      if( allocFl )
        mep->obj.userDataPtr = cmMemAllocZ(cmScSyncState_t,1);      
      else
        cmMemPtrFree(&mep->obj.userDataPtr);
    }
}

void _cmScPrintSyncState( cmSc_t* p, cmTlH_t tlH )
{
  unsigned i = 0;
  double sr = cmTimeLineSampleRate(tlH);
  cmTlMidiEvt_t* mep = cmTlNextMidiEvtObjPtr(tlH,NULL,cmInvalidId);
  
  for(; mep != NULL; mep = cmTlNextMidiEvtObjPtr(tlH,&mep->obj,cmInvalidId))
    if( mep->msg->status == kNoteOnMdId )
    {
      cmScSyncState_t* ssp        = (cmScSyncState_t*)mep->obj.userDataPtr;

      cmRptPrintf(p->err.rpt,"%5.3f pit:0x%2x (%3i) bar:%3i bni:%3i cnt:%3i dst:%1.6f ref:%s\n",
        (mep->obj.ref->begSmpIdx - mep->obj.begSmpIdx) / (sr*60),
        mep->msg->u.chMsgPtr->d0,
        mep->msg->u.chMsgPtr->d0,
        ssp->cnt ? p->array[ ssp->scEvtIdx ].barNumb    : 0,
        ssp->cnt ? p->array[ ssp->scEvtIdx ].barNoteIdx : 0,
        ssp->cnt,
        ssp->dist,
        cmStringNullGuard(mep->obj.ref->name));

      ++i;
      if( i>=300)
        break;
    }
}

double _cmScWndEditDist( cmSc_t* p,  unsigned* mtx, const unsigned* tlWnd, cmScSyncState_t* tlObjWnd[], unsigned wndCnt )
{
  unsigned scWnd[ wndCnt ];
  unsigned scIdxWnd[ wndCnt ];
  unsigned i;
  unsigned wn = 0;
  double   minDist = DBL_MAX;

  // for each note-on score event
  for(i=0; i<p->cnt; ++i)
    if( p->array[i].type == kNonEvtScId )
    {
      // shift the score event window to the the left
      memmove(scWnd,   scWnd+1,   (wndCnt-1)*sizeof(scWnd[0]));
      memmove(scIdxWnd,scIdxWnd+1,(wndCnt-1)*sizeof(scIdxWnd[0]));

      // insert new score event data on right
      scWnd[wndCnt-1]    = p->array[i].pitch;
      scIdxWnd[wndCnt-1] = i;
      ++wn;

      // if the window is full
      if(wn >= wndCnt )
      {
        // score the edit distance between the time line window and the edit window
        double dist = cmVOU_LevEditDist(wndCnt,mtx,scWnd,wndCnt,tlWnd,wndCnt,wndCnt);

        if( dist < minDist )
          minDist = dist;

        // update the match information in the time line window
        unsigned j;
        for(j=0; j<wndCnt; ++j)
        {
          // if the pitch matches and the score is less than the previous score
          if( scWnd[j] == tlWnd[j] && (tlObjWnd[j]->cnt == 0 || dist < tlObjWnd[j]->dist) )
          {
            tlObjWnd[j]->cnt      += 1;
            tlObjWnd[j]->dist      = dist;
            tlObjWnd[j]->scEvtIdx  = scIdxWnd[j];
          }
        }
      }      
    }
  
  return minDist;
}

cmScRC_t cmScoreSyncTimeLine( cmScH_t scH, cmTlH_t tlH, unsigned edWndCnt, cmReal_t maxSecs )
{
  cmSc_t*          p            = _cmScHandleToPtr(scH);
  unsigned*        edWndMtx     = cmVOU_LevEditDistAllocMtx(edWndCnt);
  unsigned         maxMicroSecs = floor(maxSecs*1000000);
  unsigned         edWndData[ edWndCnt ];
  cmScSyncState_t* edWndObj[ edWndCnt ];

  // alloc a sync state record for each MIDI note-on in the time line
  _cmScSyncTimeLineAllocFree(tlH, true );

  // get the first time line object
  cmTlObj_t*      rfp = cmTimeLineNextTypeObj(tlH,NULL,cmInvalidId,kMidiFileTlId);

  // interate through the time line in search of MIDI file objects
  for(; rfp != NULL; rfp = cmTimeLineNextTypeObj(tlH,rfp,cmInvalidId,kMidiFileTlId))
  {
    cmTlMidiFile_t* mfp         = cmTimeLineMidiFileObjPtr(tlH,rfp);
    unsigned        curEdWndCnt = 0;
    double          prog        = 0.1;
    unsigned        progIdx     = 0;

    cmRptPrintf(p->err.rpt,"MIDI File:%s\n", cmMidiFileName( mfp->h ));

    // get first midi event object
    cmTlMidiEvt_t* mep = cmTlNextMidiEvtObjPtr(tlH,NULL,cmInvalidId);

    // iterate through the time line in search of MIDI note-on events with belong to mfp      
    for(; mep != NULL; mep = cmTlNextMidiEvtObjPtr(tlH,&mep->obj,cmInvalidId) )
    {
      if( mep->obj.ref == rfp && mep->msg->status == kNoteOnMdId )
      {
        // If this notes inter-onset time is greater than maxMicroSecs
        // then dispose of the current window and begin refilling it again.
        if( mep->msg->dtick > maxMicroSecs )
          curEdWndCnt = 0;

        // shift window one slot to left
        unsigned i;
        for(i=0; i<edWndCnt-1; ++i)
        {
          edWndData[i] = edWndData[i+1];
          edWndObj[i]  = edWndObj[i+1];
        }
        
        // fill window on right
        edWndData[edWndCnt-1] = mep->msg->u.chMsgPtr->d0; // d0=pitch
        edWndObj[ edWndCnt-1] = (cmScSyncState_t*)mep->obj.userDataPtr;

        ++curEdWndCnt;
        
        // if a complete window exists then update the time-line / score match state
        if( curEdWndCnt >= edWndCnt )
         _cmScWndEditDist( p, edWndMtx, edWndData, edWndObj, edWndCnt );       

        // print the progress
        ++progIdx;
        if( progIdx >= prog * mfp->noteOnCnt )
        {
          cmRptPrintf(p->err.rpt,"%i ",(unsigned)round(prog*10));
          prog += 0.1;
        }
      }
    }
    cmRptPrintf(p->err.rpt,"\n");
  }

  _cmScPrintSyncState(p,tlH );

  // free sync state records
  _cmScSyncTimeLineAllocFree(tlH,false);

  cmMemFree(edWndMtx);

  return kOkScRC;
}


cmScRC_t cmScoreSyncTimeLineTest( cmCtx_t* ctx,  const cmChar_t* timeLineJsFn, const cmChar_t* scoreCsvFn )
{
  cmScRC_t rc  = kOkScRC;
  cmTlH_t  tlH = cmTimeLineNullHandle;
  cmScH_t  scH = cmScNullHandle;
  unsigned edWndCnt     = 7;
  cmReal_t maxSecs = 2.0;

  if((rc = cmTimeLineInitialize(ctx,&tlH,NULL,NULL)) != kOkTlRC )
    return cmErrMsg(&ctx->err,kTimeLineFailScRC,"Time line initialization failed.");;

  if((rc = cmTimeLineReadJson(tlH,timeLineJsFn)) != kOkTlRC )
  {
    rc = cmErrMsg(&ctx->err,kTimeLineFailScRC,"Time line parse failed.");;
    goto errLabel;
  }

  //cmTimeLinePrint(tlH,&ctx->rpt);

  if(1)
  {
    if((rc = cmScoreInitialize(ctx,&scH,scoreCsvFn,NULL,NULL))  != kOkScRC )
      goto errLabel;


    rc = cmScoreSyncTimeLine(scH, tlH, edWndCnt, maxSecs );

  }
  //cmScorePrint(scH, ctx->err.rpt );

  

 errLabel:
  cmScoreFinalize(&scH);
  cmTimeLineFinalize(&tlH);

  return rc;

}


void cmScoreTest( cmCtx_t* ctx, const cmChar_t* fn )
{
  cmScH_t h = cmScNullHandle;
  if( cmScoreInitialize(ctx,&h,fn,NULL,NULL) != kOkScRC )
    return;

  cmScorePrint(h,&ctx->rpt);

  cmScoreFinalize(&h);
}

// 1. Fix absolute message time which was incorrect on original score file.
// 2. 
void cmScoreFix( cmCtx_t* ctx )
{
  const cmChar_t*          mfn  = "/home/kevin/src/cmgv/src/gv/data/ImaginaryThemes.mid";
  const cmChar_t*          crfn = "/home/kevin/src/cmgv/src/gv/data/mod0a.txt";
  const cmChar_t*          cwfn = "/home/kevin/src/cmgv/src/gv/data/mod1.csv";
  cmMidiFileH_t            mfH  = cmMidiFileNullHandle;
  cmCsvH_t                 csvH = cmCsvNullHandle;
  const cmMidiTrackMsg_t** msg  = NULL;
  double                   secs = 0.0;
  int                      ci,mi,crn,mn;
  bool                     errFl = true;
  unsigned                 handCnt = 0;
  unsigned                 midiMissCnt = 0;

  if( cmCsvInitialize(&csvH,ctx) != kOkCsvRC )
    goto errLabel;

  if( cmCsvLexRegisterMatcher(csvH, cmCsvLexNextAvailId(csvH), _cmScLexSciPitchMatcher ) != kOkCsvRC )
    goto errLabel;

  if( cmCsvParseFile(csvH, crfn, 0 ) != kOkCsvRC )
    goto errLabel;

  if( cmMidiFileOpen(mfn,&mfH,ctx) != kOkMfRC )
    goto errLabel;

  cmMidiFileTickToMicros(mfH);

  cmMidiFileCalcNoteDurations(mfH);

  mn = cmMidiFileMsgCount(mfH);

  msg = cmMidiFileMsgArray(mfH);

  crn = cmCsvRowCount(csvH);

  // for each row in the score file
  for(ci=1,mi=0; ci<crn && cmCsvLastRC(csvH)==kOkCsvRC; ++ci)
  {
    unsigned  id;

    // zero the duration column 
    if( cmCsvCellPtr(csvH, ci, kDSecsColScIdx ) != NULL )
      cmCsvSetCellUInt(   csvH, ci, kDSecsColScIdx, 0 );

    // get the MIDI file event id for this row
    if((id = cmCsvCellUInt(csvH,ci,kMidiFileIdColScIdx)) == UINT_MAX)
    {
      // this is a hand-entered event -  so it has no event id
      ++handCnt;
      
    }
    else
    {
      for(; mi<mn; ++mi)
      {
        const cmMidiTrackMsg_t* m = msg[mi];

        assert( mi+1 <= id );
        secs += m->dtick/1000000.0;

        if( mi+1 != id )
        {
          if( m->status == kNoteOnMdId && m->u.chMsgPtr->d1>0 )
          {
            // this MIDI note-on does not have a corresponding score event
            ++midiMissCnt;
          }
        }
        else
        {
          cmCsvSetCellDouble( csvH, ci, kSecsColScIdx, secs );
          ++mi;

          if( m->status == kNoteOnMdId )
            cmCsvSetCellDouble(   csvH, ci, kDSecsColScIdx, m->u.chMsgPtr->durTicks/1000000.0 );
          break;
        }
        
        
      }

      if( mi==mn)
        printf("done on row:%i\n",ci);
    }
  }

  if( cmCsvLastRC(csvH) != kOkCsvRC )
    goto errLabel;

  if( cmCsvWrite(csvH,cwfn) != kOkCsvRC )
    goto errLabel;

  errFl = false;

 errLabel:
  if( errFl )
    printf("Score fix failed.\n");
  else
    printf("Score fix done! hand:%i miss:%i\n",handCnt,midiMissCnt);
  cmMidiFileClose(&mfH);

  cmCsvFinalize(&csvH);

}
