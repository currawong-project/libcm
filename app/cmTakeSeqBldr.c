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


typedef struct cmNoteTsb_str
{
  unsigned mni;      // midi note index as an offset from the take marker
  unsigned scEvtIdx; // score event index this not is assoc'd with or -1 if it did not match
  unsigned flags;    // flags from cmScMatcherResult_t 
} cmNoteTsb_t;

typedef struct cmTakeTsb_str
{
  unsigned     markerUid;  // marker time line uid assoc'd with this take
  cmNoteTsb_t* noteV;      // noteV[noteN] score to midi file map recd. array.
  unsigned     noteN;  
  bool         failFl;
} cmTakeTsb_t;

typedef struct
{
  cmCtx_t         ctx;
  cmErr_t         err;
  cmJsonH_t       jsH;
  const cmChar_t* tlFn;
  const cmChar_t* scFn;
  const cmChar_t* tlPrefixPath;

  cmTakeTsb_t*    takeV;  // takeV[ takeN ]
  unsigned        takeN;
  
  cmTlH_t        tlH;
  cmScH_t         scH;

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
  
  for(i=0; i<p->takeN; ++i)
    cmMemPtrFree(&p->takeV[i].noteV);

  cmMemPtrFree(&p->takeV);

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
        0 )) != kOkJsRC )
  {
    if( jsRC == kNodeNotFoundJsRC && errMsg != NULL )
      rc = cmErrMsg(&p->err,kParseFailTsbRC,"JSON file header parse failed missing required field:'%s'",errMsg);
    else
      rc = cmErrMsg(&p->err,kParseFailTsbRC,"JSON file header parse failed.");

    goto errLabel;
  }

  // count of take records
  p->takeN = cmJsonChildCount(tkArrObj);

  // array of take records
  p->takeV  = cmMemAllocZ(cmTakeTsb_t,p->takeN);

  // for each take record
  for(i=0; i<p->takeN; ++i)
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
          "markerUid",kIntTId,   &p->takeV[i].markerUid,
          "failFl",   kIntTId,   &p->takeV[i].failFl,
          "array",    kArrayTId, &noteArrObj,
          0)) != kOkJsRC )
    {
      if( jsRC == kNodeNotFoundJsRC && errMsg != NULL )
        rc = cmErrMsg(&p->err,kParseFailTsbRC,"JSON file take record parse failed missing required field:'%s'",errMsg);
      else
        rc = cmErrMsg(&p->err,kParseFailTsbRC,"JSON file take record parse failed.");

      goto errLabel;
    }

    // get the count of note records
    p->takeV[i].noteN = cmJsonChildCount(noteArrObj);
    
    // allocate a note record array for this take
    p->takeV[i].noteV = cmMemAllocZ(cmNoteTsb_t, p->takeV[i].noteN);

    // for each note record
    for(j=0; j<p->takeV[i].noteN; ++j)
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
            "mni",      kIntTId, &p->takeV[i].noteV[j].mni,
            "scEvtIdx", kIntTId, &p->takeV[i].noteV[j].scEvtIdx,
            "flags",    kIntTId, &p->takeV[i].noteV[j].flags,
            0)) != kOkJsRC )
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
  cmTsbRC_t rc = kOkTsbRC;
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
