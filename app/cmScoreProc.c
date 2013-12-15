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

enum
{
  kOkSpRC,
  kJsonFailSpRC,
  kScoreFailSpRC,
  kTimeLineFailSpRC,
  kScoreMatchFailSpRC,
  kFileFailSpRC,
};

typedef struct _cmScMeas_t
{
  cmTlMarker_t*       markPtr;  // time-line marker in which this 'set' exists
  cmScoreSet_t*       setPtr;   // score set on which this measurment is based
  double              value;    // the value of the measurement
  double              cost;     // the quality of the perf->score match 
  struct _cmScMeas_t* link;
} _cmScMeas_t;


typedef struct
{
  cmErr_t         err;
  cmCtx*          ctx;
  cmScH_t         scH;
  const cmChar_t* tlFn;
  cmTlH_t         tlH;
  cmJsonH_t       jsH;
  unsigned*       dynArray;
  unsigned        dynCnt;
  double          srate;
  cmScMeas*       meas;
  cmScMatcher*    match;

  cmTlMarker_t*   curMarkPtr;
  _cmScMeas_t*    list_beg;
  _cmScMeas_t*    list_end;
  _cmScMeas_t*    slist_beg; 
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

cmSpRC_t _cmScoreProcInit( cmCtx_t* ctx, cmSp_t* p, const cmChar_t* rsrcFn  )
{
  cmSpRC_t        rc     = kOkSpRC;
  const cmChar_t* scFn   = NULL;
  const cmChar_t* tlFn   = NULL;
  const cmChar_t* tlPrefixPath = NULL;

  p->srate = 96000;

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


  // load the score file
  if( cmScoreInitialize(ctx, &p->scH, scFn, p->srate, NULL, 0, NULL, NULL, cmSymTblNullHandle ) != kOkScRC )
  {
    rc = cmErrMsg(&p->err,kScoreFailSpRC,"Score load failed for score file:%s.",cmStringNullGuard(scFn));
    goto errLabel;
  }

  // load the time-line file
  if( cmTimeLineInitializeFromFile(ctx, &p->tlH, NULL, NULL, tlFn, tlPrefixPath ) != kOkTlRC )
  {
    rc = cmErrMsg(&p->err,kTimeLineFailSpRC,"Time line load failed for time line file:%s.",cmStringNullGuard(tlFn));
    goto errLabel;
  }

  p->ctx  = cmCtxAlloc(NULL, &ctx->rpt, cmLHeapNullHandle, cmSymTblNullHandle );

 errLabel:
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

unsigned _cmScMeasSectCount( cmSp_t* sp )
{
  const _cmScMeas_t* mp = sp->list_beg;
  unsigned n = 0;
  for(; mp != NULL; mp=mp->link)
    n += mp->setPtr->sectCnt;

  return n;
}



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
} _cmScMeasSect_t;

int _cmScMeasSectCompare( const void* p0, const void* p1 )
{
  _cmScMeasSect_t* m0 = (_cmScMeasSect_t*)p0;
  _cmScMeasSect_t* m1 = (_cmScMeasSect_t*)p1;

  return (int)m0->dstScLocIdx - (int)m1->dstScLocIdx;
}

cmSpRC_t _cmScWriteMeasFile( cmCtx_t* ctx, cmSp_t* sp, const cmChar_t* outFn )
{
  cmFileH_t fH = cmFileNullHandle;
  cmSpRC_t rc = kOkSpRC;
  unsigned i,j,k;
  _cmScMeas_t* mp = sp->list_beg;

  unsigned scnt = _cmScMeasSectCount(sp);
  _cmScMeasSect_t sarray[ scnt ];
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
      _cmScMeasSect_t* r = sarray + k;

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

  qsort(sarray,scnt,sizeof(sarray[0]),_cmScMeasSectCompare);

  if( cmFileOpen(&fH,outFn,kWriteFileFl,&ctx->rpt) != kOkFileRC )
  {
    rc = cmErrMsg(&sp->err,kFileFailSpRC,"Unable to create the output file '%s'.",cmStringNullGuard(outFn));
    goto errLabel;
  }

  cmFilePrintf(fH,"{\n meas : \n[\n[  \"sec\"  \"typeLabel\"  \"val\" \"cost\" \"loc\" \"evt\" \"seq\" \"mark\" \"typeId\" ]\n");

  for(i=0; i<scnt; ++i)
  {
    _cmScMeasSect_t* r = sarray + i;

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


void _cmScMatchCb( cmScMatcher* p, void* arg, cmScMatcherResult_t* rp )
{
  cmSp_t*   sp = (cmSp_t*)arg;
  cmScMeas* sm = sp->meas;

  if( cmScMeasExec(sm, rp->mni, rp->locIdx, rp->scEvtIdx, rp->flags, rp->smpIdx, rp->pitch, rp->vel ) == cmOkRC )
  {
    unsigned i;
    for(i=sm->vsi; i<sm->nsi; ++i)
      // ignore set's which did not produce a valid value
      if(sm->set[i].value != DBL_MAX )
      {
        _cmScMeas_t* r = cmMemAllocZ(_cmScMeas_t,1);
        r->markPtr = sp->curMarkPtr;
        r->setPtr  = sm->set[i].sp;
        r->value   = sm->set[i].value;
        r->cost    = sm->set[i].match_cost;

        if( sp->list_beg == NULL )
        {
          sp->list_beg = r;
          sp->list_end = r;
        }
        else
        {
          sp->list_end->link = r;
          sp->list_end       = r;
        }
      }    
  }
}


cmSpRC_t _cmScoreGenAllMeasurements(cmCtx_t* ctx, cmSp_t* sp, const cmChar_t* outFn)
{
  cmSpRC_t rc     = kOkSpRC;
  unsigned midiN  = 7;
  unsigned scWndN = 10;
  unsigned seqN   = cmTimeLineSeqCount(sp->tlH);
  double   srate  = cmTimeLineSampleRate(sp->tlH);
  unsigned seqId;

  assert( sp->srate == srate);

  // allocate the performance eval. object
  sp->meas = cmScMeasAlloc( sp->ctx, NULL, sp->scH, sp->srate, sp->dynArray, sp->dynCnt );
  assert( sp->meas != NULL );

  // allocate the score matcher
  sp->match = cmScMatcherAlloc(sp->ctx,NULL,sp->srate,sp->scH,scWndN,midiN,_cmScMatchCb,sp);
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

      // set the marker ptr (which is used in _cmScMatchCb())
      sp->curMarkPtr = markPtr;

      // get the end-of-marker time as a sample index
      unsigned markEndSmpIdx = markPtr->obj.seqSmpIdx + markPtr->obj.durSmpCnt;

      // get the score event associated with the marker's bar number.
      const cmScoreEvt_t* evtPtr = cmScoreBarEvt(sp->scH,markPtr->bar);
      assert( evtPtr != NULL );


      // get the score location associated with the markers bar score event
      const cmScoreLoc_t* locPtr = cmScoreEvtLoc(sp->scH,evtPtr);
      assert( locPtr != NULL );

      cmRptPrintf(&ctx->rpt,"Processing loc:%i seq:%i %s %s\n",locPtr->index,seqId,cmStringNullGuard(markPtr->obj.name),cmStringNullGuard(markPtr->text));

      // reset the performance evaluation object
      if( cmScMeasReset(sp->meas) != cmOkRC )
      {
        cmErrMsg(&sp->err,kScoreMatchFailSpRC,"The score performance evaluation object failed on reset.");
        continue;
      }
      
      // reset the score matcher to begin searching at the bar location
      if( cmScMatcherReset(sp->match, locPtr->index ) != cmOkRC )
      {
        cmErrMsg(&sp->err,kScoreMatchFailSpRC,"The score matcher reset failed on location: %i.",locPtr->index);
        continue;
      }

      cmTlObj_t* o1p = o0p;

      // as long as more MIDI events are available get the next MIDI msg 
      while( (rc == kOkSpRC) && (o1p = cmTimeLineNextTypeObj(sp->tlH, o1p, seqId, kMidiEvtTlId )) != NULL )
      {
        cmTlMidiEvt_t* mep = cmTimeLineMidiEvtObjPtr(sp->tlH,o1p);
        assert(mep != NULL );

        // if the msg falls after the end of the marker then we are done
        if( mep->obj.seqSmpIdx != cmInvalidIdx && mep->obj.seqSmpIdx > markEndSmpIdx )
          break;

        
        // if the time line MIDI msg a note-on
        if( mep->msg->status == kNoteOnMdId )
        {
          cmRC_t cmRC = cmScMatcherExec(sp->match, mep->obj.seqSmpIdx, mep->msg->status, mep->msg->u.chMsgPtr->d0, mep->msg->u.chMsgPtr->d1, NULL );

          switch( cmRC )
          {
            case cmOkRC:         // continue processing MIDI events
              break;

            case cmEofRC:        // end of the score was encountered
              break;

            case cmInvalidArgRC: // p->eli was not set correctly
              rc = cmErrMsg(&sp->err,kScoreMatchFailSpRC,"The score matcher failed due to an invalid argument.");
              goto errLabel;
              break;

            case cmSubSysFailRC: // scan resync failed
              rc = cmErrMsg(&sp->err,kScoreMatchFailSpRC,"The score matcher failed on resync.");
              cmScMatcherPrint(sp->match);
              //goto errLabel;
              break;

            default:
              { assert(0); }
          }
        }
      }

      rc = kOkSpRC;
    }
  }


 errLabel:

  if((rc = _cmScWriteMeasFile(ctx, sp, outFn )) != kOkSpRC )
    cmErrMsg(&sp->err,kFileFailSpRC,"The measurement output did not complete without errors."); 

  _cmScMeas_t* mp = sp->list_beg;
  while(mp!=NULL)
  {
    _cmScMeas_t* np = mp->link;
    cmMemFree(mp);
    mp = np;
  }

  if( cmScMatcherFree(&sp->match) != cmOkRC )
    cmErrMsg(&sp->err,kScoreMatchFailSpRC,"The score matcher release failed.");

  if( cmScMeasFree(&sp->meas) != cmOkRC )
    cmErrMsg(&sp->err,kScoreMatchFailSpRC,"The performance evaluation object failed.");


  return rc;
}

unsigned cmScoreProc(cmCtx_t* ctx)
{
  cmSpRC_t rc = kOkSpRC;
  const cmChar_t* rsrcFn = "/home/kevin/.kc/time_line.js";
  const cmChar_t* outFn  = "/home/kevin/src/cmkc/src/kc/data/meas0.js";
  cmSp_t sp;

  memset(&sp,0,sizeof(sp));

  cmRptPrintf(&ctx->rpt,"Score Proc Start\n");

  if((rc = _cmScoreProcInit(ctx,&sp,rsrcFn)) != kOkSpRC )
    goto errLabel;
  
  _cmScoreGenAllMeasurements(ctx,&sp,outFn);

  //cmScorePrint(sp.scH,&ctx->rpt);
  //cmScorePrintLoc(sp.scH);

 
 errLabel:
  _cmScoreProcFinal(&sp);

  cmRptPrintf(&ctx->rpt,"Score Proc End\n");

  return rc;
  
}

