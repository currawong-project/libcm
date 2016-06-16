#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmFloatTypes.h"
#include "cmComplexTypes.h"
#include "cmFile.h"
#include "cmFileSys.h"
#include "cmJson.h"
#include "cmSymTbl.h"
#include "cmAudioFile.h"
#include "cmText.h"
#include "cmProcObj.h"
#include "cmProcTemplate.h"
#include "cmMath.h"
#include "cmTime.h"
#include "cmMidi.h"
#include "cmMidiFile.h"
#include "cmProc.h"
#include "cmProc2.h"
#include "cmVectOps.h"
#include "cmTimeLine.h"
#include "cmScore.h"
#include "cmProc4.h"
#include "cmMidiScoreFollow.h"
#include "cmScoreMatchGraphic.h"

typedef struct
{
  cmScMatcherResult_t* rV;       // rV[rN] - array of stored cmScMatcher callback records.
  unsigned             rAllocN;  //
  unsigned             rN;       //
} _cmMsf_ScoreFollow_t;

void _cmMsf_ReportScoreErrors( const _cmMsf_ScoreFollow_t* f, cmScH_t scH )
{
  unsigned scoreEvtN = cmScoreEvtCount(scH);
  unsigned i,j;
  
  for(i=0; i<scoreEvtN; ++i)
  {
    const cmScoreEvt_t* e = cmScoreEvt(scH,i);
    assert(e != NULL);

    if( e->type == kNonEvtScId && cmIsNotFlag(e->flags,kSkipScFl) )
    {
      unsigned matchN = 0;
      
      for(j=0; j<f->rN; ++j)
        if( f->rV[j].scEvtIdx == i )
          matchN += 1;
      
      if( matchN != 1 )
      {
        const cmScoreLoc_t* l = cmScoreEvtLoc(scH,e);
        assert(l != NULL);
        printf("bar:%3i evtIdx:%5i pitch:%4s match:%i ",l->barNumb,e->index,cmMidiToSciPitch(e->pitch,NULL,0),matchN);

        // print the midi event associated with multiple matches.
        if( matchN > 1 )
          for(j=0; j<f->rN; ++j)
            if( f->rV[j].scEvtIdx == i )
              printf("(%i %s) ",f->rV[j].muid, cmMidiToSciPitch(f->rV[j].pitch,NULL,0) );
        
        printf("\n");
      }
    }
  }
}

void _cmMsf_ReportMidiErrors( const _cmMsf_ScoreFollow_t* f, cmScH_t scH, const cmMidiTrackMsg_t** m, unsigned mN)
{
  unsigned i,j;
  unsigned lastBar = 0;
  
  // for each midi note-on msg
  for(i=0; i<mN; ++i)
  {    
    if( (m[i]!=NULL) && cmMidiIsChStatus(m[i]->status) && cmMidiIsNoteOn(m[i]->status) && (m[i]->u.chMsgPtr->d1>0) )          
    {
      unsigned matchN = 0;
      
      // find the note-on msg in the score-match result array
      for(j=0; j<f->rN; ++j)
        if( f->rV[j].muid == m[i]->uid )
        {
          if( f->rV[j].scEvtIdx != -1 )
          {
            const cmScoreEvt_t* e = cmScoreEvt(scH,f->rV[j].scEvtIdx);
            if( e != NULL )
            {
              const cmScoreLoc_t* l = cmScoreEvtLoc(scH,e);
              assert(l != NULL );
              lastBar = l->barNumb;
            }
          }
          
          matchN += 1;
          break;
        }

      if( matchN==0 )
      {
        printf("bar:%3i muid:%4i %s\n", lastBar, m[i]->uid, cmMidiToSciPitch(m[i]->u.chMsgPtr->d0,NULL,0));
      }
        
    }
  }
}

// Write one scScoreMatcherResult_t record to the file fH.
unsigned _cmMsf_WriteMatchFileLine( cmFileH_t fH, cmScH_t scH, const cmScMatcherResult_t* r )
{
  unsigned scUid = -1;
  cmChar_t buf[6];
  buf[0] = 0;
  buf[5] = 0;
  
  cmScoreLoc_t* loc = NULL;
    
  if( r->scEvtIdx > 0 && r->scEvtIdx < cmScoreEvtCount(scH))
  {
    cmScoreEvt_t* e = cmScoreEvt(scH,r->scEvtIdx);
    loc   = cmScoreEvtLoc(scH,e);
    scUid = e->csvEventId;
    cmMidiToSciPitch(e->pitch,buf,5);
  }
  
  cmFilePrintf(fH,"m %3i %5i %4s %5i %4s %3i\n",
    loc==NULL ? 0 : loc->barNumb,              // score evt bar
    scUid,                                     // score event uuid
    buf,                                       // score event pitch
    r->muid,                                   // midi event uuid
    cmMidiToSciPitch(r->pitch,NULL,0),         // midi event pitch
    r->vel);                                   // midi event velocity

  return scUid;
}

void _cmMsf_ScoreFollowCb( struct cmScMatcher_str* p, void* arg, cmScMatcherResult_t* rp )
{
  _cmMsf_ScoreFollow_t* r = (_cmMsf_ScoreFollow_t*)arg;
  r->rV[r->rN++] = *rp;
}

cmMsfRC_t cmMidiScoreFollowMain( cmCtx_t* ctx )
{
  cmMsfRC_t             rc          = kOkMsfRC;
  //const cmChar_t* scoreFn         = cmFsMakeUserFn("src/kc/src/kc/data","mod2e","csv",NULL);
  const cmChar_t*          scoreFn  = cmFsMakeUserFn("temp","a5","csv",NULL);
  const cmChar_t*          midiFn   = cmFsMakeUserFn("media/projects/imag_themes/scores/gen","round1-utf8_11","mid",NULL);
  const cmChar_t*          outFn    = cmFsMakeUserFn("temp","match","txt",NULL);
  const cmChar_t*          svgFn    = cmFsMakeUserFn("temp","score0","html",NULL);
  const cmChar_t*          tlBarFn  = cmFsMakeUserFn("temp",NULL,"time_line_temp","txt",NULL);
  
  double                   srate    = 96000.0;
  cmScMatcher*             smp      = NULL;  
  cmScH_t                  scH      = cmScNullHandle;
  cmMidiFileH_t            mfH      = cmMidiFileNullHandle;
  unsigned                 scWndN   = 10;
  unsigned                 midiWndN = 7;
  const cmMidiTrackMsg_t** m        = NULL;
  unsigned                 mN       = 0;
  unsigned                 scLocIdx = 0;
  cmFileH_t                fH       = cmFileNullHandle;
  cmSmgH_t                 smgH     = cmSmgNullHandle;
  unsigned                 i;
  cmErr_t                  err;
  _cmMsf_ScoreFollow_t    sfr;
  memset(&sfr,0,sizeof(sfr));

  cmErrSetup(&err,&ctx->rpt,"cmMidiScoreFollow");

  cmCtx* prCtx   = cmCtxAlloc(NULL, err.rpt, cmLHeapNullHandle, cmSymTblNullHandle );
  
  // initialize the score
  if( cmScoreInitialize( ctx, &scH, scoreFn, srate, NULL, 0, NULL, NULL, cmSymTblNullHandle) != kOkScRC )
  {
    rc = cmErrMsg(&err,kFailMsfRC,"cmScoreInitialize() failed on %s",cmStringNullGuard(scoreFn));
    goto errLabel;
  }

  // setup the callback record
  if((sfr.rAllocN  = cmScoreEvtCount( scH )*2) == 0)
  {
    rc = cmErrMsg(&err,kFailMsfRC,"The score %s appears to be empty.",cmStringNullGuard(scoreFn));
    goto errLabel;
  }

  sfr.rV = cmMemAllocZ(cmScMatcherResult_t,sfr.rAllocN);
  sfr.rN = 0;
  
  // create a matcher
  if((smp = cmScMatcherAlloc(prCtx, NULL, srate, scH, scWndN, midiWndN, _cmMsf_ScoreFollowCb, &sfr)) == NULL )
  {
    rc = cmErrMsg(&err,kFailMsfRC,"cmScMatcherAlloc() failed.");
    goto errLabel;
  }

  // open the MIDI file
  if( cmMidiFileOpen(ctx, &mfH, midiFn ) != kOkMfRC )
  {
    rc = cmErrMsg(&err,kFailMsfRC,"The MIDI file object could not be opened from '%s'.",cmStringNullGuard(midiFn));
    goto errLabel;
  }

  // get a pointer to the MIDI msg array
  if( (m = cmMidiFileMsgArray(mfH)) == NULL || (mN = cmMidiFileMsgCount(mfH)) == 0 )
  {
    rc = cmErrMsg(&err,kFailMsfRC,"The MIDI file object appears to be empty.");
    goto errLabel;
  }

  // feed each MIDI note-on to the score follower
  for(i=0; i<mN; ++i)
    if( (m[i]!=NULL) && cmMidiIsChStatus(m[i]->status) && cmMidiIsNoteOn(m[i]->status) && (m[i]->u.chMsgPtr->d1>0) )          
      if( cmScMatcherExec( smp, m[i]->amicro * srate / 1000000.0, m[i]->uid, m[i]->status, m[i]->u.chMsgPtr->d0, m[i]->u.chMsgPtr->d1, &scLocIdx ) != cmOkRC )
      {
        rc = cmErrMsg(&err,kFailMsfRC,"The score matcher exec failed.");
        goto errLabel;
      }


  printf("MIDI notes:%i Score Events:%i\n",mN,cmScoreEvtCount(scH));

  // create the output file
  if( cmFileOpen(&fH,outFn,kWriteFileFl,&ctx->rpt) != kOkFileRC )
  {
    rc = cmErrMsg(&err,kFailMsfRC,"Unable to create the file '%s'.",cmStringNullGuard(outFn));
    goto errLabel;    
  }

  // allocate the graphics object
  if( cmScoreMatchGraphicAlloc( ctx, &smgH, scoreFn, midiFn ) != kOkSmgRC )
  {
    rc = cmErrMsg(&err,kFailMsfRC,"Score Match Graphics allocation failed..");
    goto errLabel;    
  }


  // for each score follower callback record 
  for(i=0; i<sfr.rN; ++i)
  {
    // write the record to the output file
    unsigned scUid = _cmMsf_WriteMatchFileLine( fH, scH, sfr.rV + i );
    

    // insert the event->score match in the score match graphics object
    if( cmScoreMatchGraphicInsertMidi( smgH, sfr.rV[i].muid, sfr.rV[i].pitch, sfr.rV[i].vel, scUid ) != kOkSmgRC )
    {
      rc = cmErrMsg(&err,kFailMsfRC,"Score Match Graphics MIDI event insertion failed.");
      goto errLabel;    
    }

  }

  //_cmMsf_ReportScoreErrors(&sfr, scH );

  //_cmMsf_ReportMidiErrors(&sfr, scH, m, mN);

  //cmScorePrint(scH,&atc->ctx->rpt);
  //cmMidiFilePrintMsgs( mfH, &atc->ctx->rpt );

  // write the tracking match file as an SVG file.
  cmScoreMatchGraphicWrite( smgH, svgFn );

  // write a cmTimeLine file which contains markers at each bar position
  //cmScoreMatchGraphicGenTimeLineBars(smgH, tlBarFn, srate );


 errLabel:
  
  cmFileClose(&fH);
  cmMemFree(sfr.rV);
  cmMidiFileClose(&mfH);
  cmScMatcherFree(&smp);
  cmScoreFinalize(&scH);
  cmScoreMatchGraphicFree(&smgH);

  cmCtxFree(&prCtx);

  cmFsFreeFn(scoreFn);
  cmFsFreeFn(midiFn);
  cmFsFreeFn(outFn);
  cmFsFreeFn(svgFn);
  cmFsFreeFn(tlBarFn);
  
  return rc;
}
