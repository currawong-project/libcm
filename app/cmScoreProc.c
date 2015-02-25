#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmSymTbl.h"
#include "cmJson.h"
#include "cmFile.h"
#include "cmTime.h"
#include "cmMidi.h"
#include "cmMidiFile.h"
#include "cmAudioFile.h"
#include "cmScore.h"
#include "cmTimeLine.h"
#include "cmScoreProc.h"

#include "cmProcObj.h"
#include "cmProc4.h"

typedef enum
{
  kBeginTakeSpId,  // tlObjPtr points to a cmTlMarker_t object.
  kEndTakeSpId,    // tlObjPtr is NULL.
  kNoteOnSpId,     // tlObjPtr points to a cmTlMidiEvt_t note-on object.
  kFailSpId        // tlObjPtr points to a cmTlMarker_t object (This takes score tracking failed.)
} cmScoreProcSelId_t;

struct cmSp_str;

typedef cmSpRC_t (*cmScoreProcCb_t)( void* arg, struct cmSp_str* p, cmScoreProcSelId_t id, cmTlObj_t* tlObjPtr );

typedef struct cmSp_str
{
  cmErr_t         err;          // score proc object error state
  cmCtx*          ctx;          // application context
  cmScH_t         scH;          // score object
  cmTlH_t         tlH;          // time-line object
  cmJsonH_t       jsH;          // 
  unsigned*       dynArray;     // dynArray[dynCnt] dynamics reference array
  unsigned        dynCnt;       // 
  double          srate;        // 
  cmScMatcher*    match;        // score follower

  cmScoreProcCb_t  procCb;      // score processor callback - called whenever a new 'marker' take or note-on is about to be processed
  cmScMatcherCb_t  matchCb;     // score follower callback  - called whenever the score follower detects a matched event
  void*            cbArg;       // callback arg. for both matchCb and procCb.

} cmSp_t;


// read the dynamics reference array from the time-line project file.
cmSpRC_t _cmJsonReadDynArray( cmJsonH_t jsH, unsigned** dynArray, unsigned* dynCnt )
{
  cmJsonNode_t* np;
  int i;

  if( cmJsonPathToArray(jsH, NULL, NULL, "dynRef", &np ) != kOkJsRC )
    return kJsonFailSpRC;

  *dynCnt = cmJsonChildCount(np);

  *dynArray = cmMemAllocZ(unsigned,*dynCnt);
  
  for(i=0; i<*dynCnt; ++i)
    if( cmJsonUIntValue( cmJsonArrayElement(np,i), (*dynArray)+i ) != kOkJsRC )
      return kJsonFailSpRC;

  return kOkSpRC;  
}

cmSpRC_t _cmScoreProcInit( 
  cmCtx_t*        ctx, 
  cmSp_t*         p, 
  const cmChar_t* rsrcFn, 
  cmScoreProcCb_t procCb, 
  cmScMatcherCb_t matchCb, 
  void*           cbArg  )
{
  cmSpRC_t        rc     = kOkSpRC;
  const cmChar_t* scFn   = NULL;
  const cmChar_t* tlFn   = NULL;
  const cmChar_t* tlPrefixPath = NULL;

  cmErrSetup(&p->err,&ctx->rpt,"ScoreProc");

  // open the resource file
  if( cmJsonInitializeFromFile( &p->jsH, rsrcFn, ctx ) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailSpRC,"Unable to load the main resource file:%s.",cmStringNullGuard(rsrcFn));
    goto errLabel;
  }

  // get the time line fn
  if( cmJsonPathToString( p->jsH, NULL, NULL, "timeLineFn", &tlFn ) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailSpRC,"Unable to locate the time line file name in the main resource file:%s",cmStringNullGuard(rsrcFn));
    goto errLabel;
  }

  // get the score file name
  if( cmJsonPathToString( p->jsH, NULL, NULL, "scoreFn", &scFn ) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailSpRC,"Unable to locate the score file name in the main resource file:%s",cmStringNullGuard(rsrcFn));
    goto errLabel;
  }

  // get the time line data file prefix path
  if( cmJsonPathToString( p->jsH, NULL, NULL, "tlPrefixPath", &tlPrefixPath ) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailSpRC,"Unable to locate the time line data file prefix path in the main resource file:%s",cmStringNullGuard(rsrcFn));
    goto errLabel;
  }


  // read the dynamics reference array
  if((rc = _cmJsonReadDynArray( p->jsH, &p->dynArray, &p->dynCnt )) != kOkSpRC )
  {
    rc = cmErrMsg(&p->err,rc,"Unable to read dynamics reference array resource from the main resource file:%s",cmStringNullGuard(rsrcFn));
    goto errLabel;
  }


  // load the time-line file
  if( cmTimeLineInitializeFromFile(ctx, &p->tlH, NULL, NULL, tlFn, tlPrefixPath ) != kOkTlRC )
  {
    rc = cmErrMsg(&p->err,kTimeLineFailSpRC,"Time line load failed for time line file:%s.",cmStringNullGuard(tlFn));
    goto errLabel;
  }

  p->srate = cmTimeLineSampleRate(p->tlH);

  // load the score file
  if( cmScoreInitialize(ctx, &p->scH, scFn, p->srate, NULL, 0, NULL, NULL, cmSymTblNullHandle ) != kOkScRC )
  {
    rc = cmErrMsg(&p->err,kScoreFailSpRC,"Score load failed for score file:%s.",cmStringNullGuard(scFn));
    goto errLabel;
  }

  p->ctx     = cmCtxAlloc(NULL, &ctx->rpt, cmLHeapNullHandle, cmSymTblNullHandle );
  p->matchCb = matchCb;
  p->procCb  = procCb;
  p->cbArg   = cbArg;

 errLabel:
  return rc;
}


// This function iterates through each sequence and advances
// to each 'begin-marker' position.  
cmSpRC_t _cmScoreProcProcess(cmCtx_t* ctx, cmSp_t* sp)
{
  cmSpRC_t rc     = kOkSpRC;
  unsigned midiN  = 7;
  unsigned scWndN = 10;
  unsigned seqN   = cmTimeLineSeqCount(sp->tlH);
  double   srate  = cmTimeLineSampleRate(sp->tlH);
  unsigned seqId;

  assert( sp->srate == srate);

  // allocate the score matcher
  sp->match = cmScMatcherAlloc(sp->ctx,NULL,sp->srate,sp->scH,scWndN,midiN,sp->matchCb,sp->cbArg);
  assert(sp->match != NULL );

  
  // for each time line sequence
  for(seqId=0; seqId<seqN; ++seqId)
  {
    cmTlObj_t* o0p = NULL;
  
    // for each 'marker' in this time line sequence
    while(  (o0p = cmTimeLineNextTypeObj(sp->tlH, o0p, seqId, kMarkerTlId)) != NULL )
    {
      // get the 'marker' recd
      cmTlMarker_t* markPtr = cmTimeLineMarkerObjPtr(sp->tlH,o0p);
      assert( markPtr != NULL );

      // if the marker does not have a valid start bar location
      if( markPtr->bar == 0 )
        continue;

      // get the end-of-marker time as a sample index
      unsigned markEndSmpIdx = markPtr->obj.seqSmpIdx + markPtr->obj.durSmpCnt;

      // get the score event associated with the marker's bar number.
      const cmScoreEvt_t* evtPtr = cmScoreBarEvt(sp->scH,markPtr->bar);
      assert( evtPtr != NULL );


      // get the score location associated with the markers bar score event
      const cmScoreLoc_t* locPtr = cmScoreEvtLoc(sp->scH,evtPtr);
      assert( locPtr != NULL );

      cmRptPrintf(&ctx->rpt,"Processing loc:%i seq:%i %s %s\n",locPtr->index,seqId,cmStringNullGuard(markPtr->obj.name),cmStringNullGuard(markPtr->text));

      // reset the score matcher to begin searching at the bar location
      if( cmScMatcherReset(sp->match, locPtr->index ) != cmOkRC )
      {
        cmErrMsg(&sp->err,kScoreMatchFailSpRC,"The score matcher reset failed on location: %i.",locPtr->index);
        continue;
      }

      // inform the score processor that we are about to start a new take
      if( sp->procCb( sp->cbArg, sp, kBeginTakeSpId, o0p ) != kOkSpRC )
      {
        cmErrMsg(&sp->err,kProcFailSpRC,"The score process object failed on reset.");
        continue;
      }

      cmTlObj_t* o1p   = o0p;
      bool       errFl = false;

      // as long as more MIDI events are available get the next MIDI msg 
      while( (rc == kOkSpRC) && (o1p = cmTimeLineNextTypeObj(sp->tlH, o1p, seqId, kMidiEvtTlId )) != NULL )
      {
        cmTlMidiEvt_t* mep = cmTimeLineMidiEvtObjPtr(sp->tlH,o1p);
        assert(mep != NULL );

        // if the msg falls after the end of the marker then we are done
        if( mep->obj.seqSmpIdx != cmInvalidIdx && mep->obj.seqSmpIdx > markEndSmpIdx )
          break;
        
        // if the time line MIDI msg is a note-on
        if( mep->msg->status == kNoteOnMdId )
        {
          sp->procCb( sp->cbArg, sp, kNoteOnSpId,  o1p );

          cmRC_t cmRC = cmScMatcherExec(sp->match, mep->obj.seqSmpIdx, mep->msg->status, mep->msg->u.chMsgPtr->d0, mep->msg->u.chMsgPtr->d1, NULL );

          switch( cmRC )
          {
            case cmOkRC:         // continue processing MIDI events
              break;

            case cmEofRC:        // end of the score was encountered
              break;

            case cmInvalidArgRC: // p->eli was not set correctly
              rc = cmErrMsg(&sp->err,kScoreMatchFailSpRC,"The score matcher failed due to an invalid argument.");
              errFl = true;
              break;

            case cmSubSysFailRC: // scan resync failed
              rc = cmErrMsg(&sp->err,kScoreMatchFailSpRC,"The score matcher failed on resync.");

              sp->procCb( sp->cbArg, sp, kFailSpId, o0p );

              //cmScMatcherPrint(sp->match);
              //goto errLabel;
              break;

            default:
              { assert(0); }
          }
        }
      }

      // inform the score processor that we done processing a take
      if( sp->procCb( sp->cbArg, sp, kEndTakeSpId, NULL ) != kOkSpRC )
        cmErrMsg(&sp->err,kProcFailSpRC,"The score process object failed on reset.");

      // error flag is used to break out of the loop after the 'end-take' is called
      // so that the user defined processes has a chance to clean-up 
      if( errFl )
        goto errLabel;

      rc = kOkSpRC;
    }
  }


 errLabel:


  if( cmScMatcherFree(&sp->match) != cmOkRC )
    cmErrMsg(&sp->err,kScoreMatchFailSpRC,"The score matcher release failed.");

  return rc;
}

cmSpRC_t _cmScoreProcFinal( cmSp_t* p )
{
  cmSpRC_t rc = kOkSpRC;

  cmCtxFree(&p->ctx);

  if( cmScoreFinalize(&p->scH) != kOkScRC )
    cmErrMsg(&p->err,kScoreFailSpRC,"Score finalize failed.");

  if( cmTimeLineFinalize(&p->tlH) != kOkTlRC )
    cmErrMsg(&p->err,kTimeLineFailSpRC,"Time line finalize failed.");

  if( cmJsonFinalize(&p->jsH) != kOkJsRC )
    cmErrMsg(&p->err,kJsonFailSpRC,"JSON finalize failed.");

  cmMemFree(p->dynArray);

  return rc;
}

//==================================================================================================

typedef struct _cmSpMeas_t
{
  cmTlMarker_t*       markPtr;  // time-line marker in which this 'set' exists
  cmScoreSet_t*       setPtr;   // score set on which this measurment is based
  double              value;    // the value of the measurement
  double              cost;     // the quality of the perf->score match 
  struct _cmSpMeas_t* link;
} _cmSpMeas_t;

typedef struct
{  
  struct cmSp_str* sp;  
  cmScMeas*        meas;         // performance analyzer 
  cmTlMarker_t*    curMarkPtr;   //
  _cmSpMeas_t*     list_beg;     //
  _cmSpMeas_t*     list_end;     //
  _cmSpMeas_t*     slist_beg;    //
} _cmSpMeasProc_t;

typedef struct
{
  unsigned        srcSeqId;
  const cmChar_t* srcMarkNameStr;
  unsigned        srcTypeId;
  const cmChar_t* srcTypeLabelStr;
  unsigned        dstScLocIdx;
  unsigned        dstEvtIdx;
  const cmChar_t* dstSectLabelStr;
  double          value;
  double          cost;
} _cmSpMeasSect_t;

unsigned _cmSpMeasSectCount( _cmSpMeasProc_t* m )
{
  const _cmSpMeas_t* mp = m->list_beg;
  unsigned n = 0;
  for(; mp != NULL; mp=mp->link)
    n += mp->setPtr->sectCnt;

  return n;
}

int _cmSpMeasSectCompare( const void* p0, const void* p1 )
{
  _cmSpMeasSect_t* m0 = (_cmSpMeasSect_t*)p0;
  _cmSpMeasSect_t* m1 = (_cmSpMeasSect_t*)p1;

  return (int)m0->dstScLocIdx - (int)m1->dstScLocIdx;
}

cmSpRC_t _cmScWriteMeasFile( cmCtx_t* ctx, cmSp_t* sp, _cmSpMeasProc_t* m, const cmChar_t* outFn )
{
  cmFileH_t fH = cmFileNullHandle;
  cmSpRC_t rc = kOkSpRC;
  unsigned i,j,k;
  _cmSpMeas_t* mp = m->list_beg;

  unsigned scnt = _cmSpMeasSectCount(m);
  _cmSpMeasSect_t sarray[ scnt ];
  for(i=0,k=0; k<scnt && mp!=NULL; ++i,mp=mp->link)
  {
    const cmChar_t* typeLabel = NULL;
    switch(mp->setPtr->varId)
    {
      case kEvenVarScId: typeLabel="even"; break;
      case kDynVarScId:  typeLabel="dyn";  break;
      case kTempoVarScId:typeLabel="tempo";break;
      default:
        { assert(0); }
    }

    for(j=0; j<mp->setPtr->sectCnt; ++j,++k)
    {
      _cmSpMeasSect_t* r = sarray + k;

        r->srcSeqId        = mp->markPtr->obj.seqId,
        r->srcMarkNameStr  = cmStringNullGuard(mp->markPtr->obj.name),
        r->srcTypeId       = mp->setPtr->varId,
        r->srcTypeLabelStr = typeLabel,
        r->dstScLocIdx     = mp->setPtr->sectArray[j]->locPtr->index,
        r->dstEvtIdx       = mp->setPtr->sectArray[j]->begEvtIndex,
        r->dstSectLabelStr = cmStringNullGuard(mp->setPtr->sectArray[j]->label),
        r->value           = mp->value,
        r->cost            = mp->cost;

    }
  }

  assert(mp==NULL && k==scnt);

  qsort(sarray,scnt,sizeof(sarray[0]),_cmSpMeasSectCompare);

  if( cmFileOpen(&fH,outFn,kWriteFileFl,&ctx->rpt) != kOkFileRC )
  {
    rc = cmErrMsg(&sp->err,kFileFailSpRC,"Unable to create the output file '%s'.",cmStringNullGuard(outFn));
    goto errLabel;
  }

  cmFilePrintf(fH,"{\n meas : \n[\n[  \"sec\"  \"typeLabel\"  \"val\" \"cost\" \"loc\" \"evt\" \"seq\" \"mark\" \"typeId\" ]\n");

  for(i=0; i<scnt; ++i)
  {
    _cmSpMeasSect_t* r = sarray + i;

      cmFilePrintf(fH,"[  \"%s\"  \"%s\"  %f %f  %i %i %i \"%s\" %i ]\n",
        r->dstSectLabelStr,
        r->srcTypeLabelStr,
        r->value,
        r->cost,
        r->dstScLocIdx,
        r->dstEvtIdx,
        r->srcSeqId,
        r->srcMarkNameStr,
        r->srcTypeId
                   );

  }

  /*
  mp = sp->list_beg;
  for(; mp!=NULL; mp=mp->link)
  {

    for(i=0; i<mp->setPtr->sectCnt; ++i)
    {
      cmFilePrintf(fH,"[ %i \"%s\" %i \"%s\" %i %i \"%s\" %f %f ]\n",
        mp->markPtr->obj.seqId,
        cmStringNullGuard(mp->markPtr->obj.name),
        mp->setPtr->varId,
        typeLabel,
        mp->setPtr->sectArray[i]->locPtr->index,
        mp->setPtr->sectArray[i]->begEvtIndex,
        cmStringNullGuard(mp->setPtr->sectArray[i]->label),
        mp->value,
        mp->cost );
    } 
  }
  */

  cmFilePrintf(fH,"\n]\n}\n");

 errLabel:
  if( cmFileClose(&fH) != kOkFileRC )
    cmErrMsg(&sp->err,kFileFailSpRC,"The output file close failed on '%s'.",cmStringNullGuard(outFn));

  return rc;
}

// score matcher callback
void _cmSpMatchMeasCb( cmScMatcher* p, void* arg, cmScMatcherResult_t* rp )
{
  _cmSpMeasProc_t* m  = (_cmSpMeasProc_t*)arg;
  cmScMeas*        sm = m->meas;

  if( cmScMeasExec(sm, rp->mni, rp->locIdx, rp->scEvtIdx, rp->flags, rp->smpIdx, rp->pitch, rp->vel ) == cmOkRC )
  {
    unsigned i;
    for(i=sm->vsi; i<sm->nsi; ++i)
      // ignore set's which did not produce a valid value
      if(sm->set[i].value != DBL_MAX )
      {
        _cmSpMeas_t* r = cmMemAllocZ(_cmSpMeas_t,1);
        r->markPtr = m->curMarkPtr;
        r->setPtr  = sm->set[i].sp;
        r->value   = sm->set[i].value;
        r->cost    = sm->set[i].match_cost;

        if( m->list_beg == NULL )
        {
          m->list_beg = r;
          m->list_end = r;
        }
        else
        {
          m->list_end->link = r;
          m->list_end       = r;
        }
      }    
  }
}

// measurement proc callback
cmSpRC_t  _cmSpProcMeasCb( void* arg, cmSp_t* sp, cmScoreProcSelId_t id, cmTlObj_t* tlObjPtr )
{
  cmSpRC_t         rc = kOkSpRC;
  _cmSpMeasProc_t* m = (_cmSpMeasProc_t*)arg;

  switch( id )
  {
    case kBeginTakeSpId:

      // reset the performance evaluation object
      if( cmScMeasReset(m->meas) != cmOkRC )
        rc = cmErrMsg(&sp->err,kScoreMatchFailSpRC,"The score performance evaluation object failed on reset.");

      m->curMarkPtr = cmTimeLineMarkerObjPtr(sp->tlH,tlObjPtr);
      break;

    case kNoteOnSpId:
      break;

    case kEndTakeSpId:
      break;

    case kFailSpId:
      break;
  }

  return rc;
}


cmSpRC_t _cmScoreProcGenAllMeasurementsMain(cmCtx_t* ctx)
{
  const cmChar_t*  rsrcFn = "/home/kevin/.kc/time_line.js";
  const cmChar_t*  outFn  = "/home/kevin/src/cmkc/src/kc/data/meas0.js";

  cmSpRC_t         rc = kOkSpRC;
  _cmSpMeasProc_t* m  = cmMemAllocZ(_cmSpMeasProc_t,1);
  cmSp_t           s;
  cmSp_t*          sp = &s;

  memset(sp,0,sizeof(s));

  cmRptPrintf(&ctx->rpt,"Score Performance Evaluation Start\n");

  // initialize the score processor
  if((rc = _cmScoreProcInit(ctx,sp,rsrcFn,_cmSpProcMeasCb,_cmSpMatchMeasCb,m)) != kOkSpRC )
    goto errLabel;

  // allocate the performance evaluation measurement object
  m->meas     = cmScMeasAlloc( sp->ctx, NULL, sp->scH, sp->srate, sp->dynArray, sp->dynCnt );
  m->sp       = sp;

  // run the score processor
  _cmScoreProcProcess(ctx,sp);

  // write the results of the performance evaluation
  if((rc = _cmScWriteMeasFile(ctx, sp, m, outFn )) != kOkSpRC )
    cmErrMsg(&sp->err,kFileFailSpRC,"The measurement output did not complete without errors."); 

  // free the measurement linked list
  _cmSpMeas_t* mp = m->list_beg;
  while(mp!=NULL)
  {
    _cmSpMeas_t* np = mp->link;
    cmMemFree(mp);
    mp = np;
  }

  // free the performance evaluation object
  if( cmScMeasFree(&m->meas) != cmOkRC )
    cmErrMsg(&sp->err,kScoreMatchFailSpRC,"The performance evaluation object failed.");

  //cmScorePrint(sp.scH,&ctx->rpt);
  //cmScorePrintLoc(sp.scH);

 
 errLabel:
  _cmScoreProcFinal(sp);

  cmMemFree(m);

  cmRptPrintf(&ctx->rpt,"Score Proc End\n");

  return rc;
  
}

//==================================================================================================
typedef struct cmSpAssoc_str
{
  unsigned              scEvtIdx; // score event index
  unsigned              tlUid;    // time-line MIDI note-on object id
  struct cmSpAssoc_str* link;
} cmSpAssoc_t;

typedef struct cmSpNoteMap_str
{
  unsigned                tlUid; // time-line MIDI note-on object id
  unsigned                mni;  // assocated 'mni' returned in a cmScMatcherResult_t record
  struct cmSpNoteMap_str* link;
} cmSpNoteMap_t;

typedef struct
{
  cmCtx_t*       ctx;
  cmSp_t*        sp; 
  unsigned       mni;
  bool           failFl;
  cmJsonH_t      jsH;
  cmJsonNode_t*  takeObj;
  cmJsonNode_t*  array;

  cmSpAssoc_t*   bap;  
  cmSpAssoc_t*   eap;
  cmSpNoteMap_t* bmp;
  cmSpNoteMap_t* emp;

  
} cmSpAssocProc_t;

void _cmSpMatchAssocCb( cmScMatcher* p, void* arg, cmScMatcherResult_t* rp )
{
  cmSpAssocProc_t* m   = (cmSpAssocProc_t*)arg;

  
  if( cmJsonCreateFilledObject(m->jsH, m->array,
      "mni",      kIntTId, rp->mni,
      "scEvtIdx", kIntTId, rp->scEvtIdx,
      "flags",    kIntTId, rp->flags, 
      NULL ) == NULL )
  {
    cmErrMsg(&m->ctx->err,kJsonFailSpRC,"JSON association record create failed.");
  }
  
  //cmScoreEvt_t*    sep = rp->scEvtIdx == -1 ? NULL : cmScoreEvt( m->sp->scH, rp->scEvtIdx );
  //printf("%3i loc:%4i pitch=%3i %3i  flags=0x%x\n",rp->mni,rp->locIdx,rp->pitch,sep==NULL ? -1 : sep->pitch,rp->flags );
  
}

cmSpRC_t  _cmSpProcAssocCb( void* arg, cmSp_t* sp, cmScoreProcSelId_t id, cmTlObj_t* tlObjPtr )
{
  cmSpRC_t         rc = kOkSpRC;
  cmSpAssocProc_t* m  = (cmSpAssocProc_t*)arg;

  switch( id )
  {
    case kBeginTakeSpId:
      {
        cmTlMarker_t* markPtr = cmTimeLineMarkerObjPtr( sp->tlH, tlObjPtr );
        assert( markPtr != NULL );
        m->mni    = 0;
        m->failFl = false;

        // insert a section object
        if((m->takeObj = cmJsonInsertPairObject(m->jsH, cmJsonRoot(m->jsH), "take" )) == NULL )
        {
          rc = cmErrMsg(&m->ctx->err,kJsonFailSpRC,"Take insert failed on seq:%i '%s' : '%s'.", tlObjPtr->seqId, cmStringNullGuard(tlObjPtr->text),cmStringNullGuard(markPtr->text));
          goto errLabel; 
        }

        // set the section time-line UID
        if( cmJsonInsertPairInt(m->jsH, m->takeObj,"markerUid", tlObjPtr->uid ) != kOkJsRC )
        {
          rc = cmErrMsg(&m->ctx->err,kJsonFailSpRC,"Marker uid field insert failed on seq:%i '%s' : '%s'.", tlObjPtr->seqId, cmStringNullGuard(tlObjPtr->text),cmStringNullGuard(markPtr->text));
          goto errLabel;           
        }

        // create an array to hold the assoc results
        if(( m->array = cmJsonInsertPairArray(m->jsH, m->takeObj, "array")) == NULL )
        {
          rc = cmErrMsg(&m->ctx->err,kJsonFailSpRC,"Marker array field insert failed on seq:%i '%s' : '%s'.", tlObjPtr->seqId, cmStringNullGuard(tlObjPtr->text),cmStringNullGuard(markPtr->text));
          goto errLabel;           
        }          
      }
      break;

    case kEndTakeSpId:
      {
        while( m->bmp != NULL )
        {
          cmSpNoteMap_t* nmp = m->bmp->link;
          cmMemFree(m->bmp);
          m->bmp = nmp;
        }
      
        m->bmp = NULL;
        m->emp = NULL;

        if( cmJsonInsertPairInt( m->jsH, m->takeObj, "failFl", m->failFl ) != kOkJsRC )
        {
          rc = cmErrMsg(&m->ctx->err,kJsonFailSpRC,"JSON fail flag insert failed.");
          goto errLabel;
        }
        
      }
      break;

    case kNoteOnSpId:
      {
        // create a cmSpNoteMap_t record ...
        cmSpNoteMap_t* map = cmMemAllocZ(cmSpNoteMap_t,1);
        map->tlUid = tlObjPtr->uid;
        map->mni   = m->mni;

        // .. and insert it in the note-map list
        if( m->emp == NULL )
        {
          m->bmp = map;
          m->emp = map;
        }
        else
        {
          m->emp->link = map;
          m->emp       = map;
        }

        m->mni += 1;
      }
      break;

    case kFailSpId:
      m->failFl = true;
      break;

  }

 errLabel:
  return rc;
}

cmSpRC_t _cmScoreProcGenAssocMain(cmCtx_t* ctx)
{
  const cmChar_t*  rsrcFn = "/home/kevin/.kc/time_line.js";
  const cmChar_t*  outFn  = "/home/kevin/src/cmkc/src/kc/data/assoc0.js";
  cmSpRC_t         rc     = kOkSpRC;
  cmSpAssocProc_t* m      = cmMemAllocZ(cmSpAssocProc_t,1);
  cmSp_t           s;
  cmSp_t*          sp     = &s;

  memset(sp,0,sizeof(s));

  m->ctx = ctx;

  cmRptPrintf(&ctx->rpt,"Score Association Start\n");

  // create a JSON object to hold the results
  if( cmJsonInitialize(&m->jsH, ctx ) != kOkJsRC )
  {
    cmErrMsg(&m->ctx->err,kJsonFailSpRC,"Score association JSON output object create failed.");
    goto errLabel;
  }

  // create the JSON root object
  if( cmJsonCreateObject(m->jsH, NULL ) == NULL )
  {
    cmErrMsg(&m->ctx->err,kJsonFailSpRC,"Create JSON root object.");
    goto errLabel;
  }

  // initialize the score processor
  if((rc = _cmScoreProcInit(ctx,sp,rsrcFn,_cmSpProcAssocCb,_cmSpMatchAssocCb, m)) != kOkSpRC )
    goto errLabel;

  m->sp = sp;

  // store the time-line and score file name
  if( cmJsonInsertPairs(m->jsH, cmJsonRoot(m->jsH), 
      "tlFn",    kStringTId, cmTimeLineFileName( sp->tlH),
      "scoreFn", kStringTId, cmScoreFileName( sp->scH ),
      NULL ) != kOkJsRC )
  {
    cmErrMsg(&m->ctx->err,kJsonFailSpRC,"File name JSON field insertion failed.");
    goto errLabel;
  }

  // run the score processor
  _cmScoreProcProcess(ctx,sp);

  cmRptPrintf(&ctx->rpt,"Writing results to '%s'.",outFn);

  // write the results to a JSON file
  if(cmJsonWrite(m->jsH, NULL, outFn ) != kOkJsRC )
  {
    cmErrMsg(&m->ctx->err,kJsonFailSpRC,"Score association output file write failed.");
    goto errLabel;
  }
 
 errLabel:
  if( cmJsonFinalize(&m->jsH) != kOkJsRC )
  {
    cmErrMsg(&m->ctx->err,kJsonFailSpRC,"JSON finalize failed.");
  }

  _cmScoreProcFinal(sp);

  cmMemFree(m);

  cmRptPrintf(&ctx->rpt,"Score Proc End\n");

  return rc;
  
}



//==================================================================================================

cmSpRC_t cmScoreProc(cmCtx_t* ctx)
{
  cmSpRC_t rc = kOkSpRC;

  //_cmScoreProcGenAllMeasurementsMain(ctx);
  _cmScoreProcGenAssocMain(ctx);

  return rc;
  
}

