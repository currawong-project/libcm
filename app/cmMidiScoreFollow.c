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

void _cmMsf_WriteMatchFileHeader( cmFileH_t fH )
{
  cmFilePrintf(fH,"  Score Score Score MIDI  MIDI MIDI\n");
  cmFilePrintf(fH,"  Bar   UUID  Pitch UUID  Ptch Vel.\n");
  cmFilePrintf(fH,"- ----- ----- ----- ----- ---- ----\n");
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
  
  cmFilePrintf(fH,"m %5i %5i %5s %5i %4s %3i\n",
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

cmMsfRC_t cmMidiScoreFollowMain(
  cmCtx_t* ctx,
  const cmChar_t* scoreCsvFn,      // score CSV file as generated from cmXScoreTest().
  const cmChar_t* midiFn,          // MIDI file to track
  const cmChar_t* matchRptOutFn,   // Score follow status report 
  const cmChar_t* matchSvgOutFn,   // Score follow graphic report
  const cmChar_t* midiOutFn,       // (optional) midiFn with apply sostenuto and velocities from the score to the MIDI file
  const cmChar_t* tlBarOutFn       // (optional) bar positions sutiable for use in a cmTimeLine description file.
)
{
  cmMsfRC_t                rc        = kOkMsfRC;  
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
  if( cmScoreInitialize( ctx, &scH, scoreCsvFn, srate, NULL, 0, NULL, NULL, cmSymTblNullHandle) != kOkScRC )
  {
    rc = cmErrMsg(&err,kFailMsfRC,"cmScoreInitialize() failed on %s",cmStringNullGuard(scoreCsvFn));
    goto errLabel;
  }

  // setup the callback record
  if((sfr.rAllocN  = cmScoreEvtCount( scH )*2) == 0)
  {
    rc = cmErrMsg(&err,kFailMsfRC,"The score %s appears to be empty.",cmStringNullGuard(scoreCsvFn));
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
  if( cmFileOpen(&fH,matchRptOutFn,kWriteFileFl,&ctx->rpt) != kOkFileRC )
  {
    rc = cmErrMsg(&err,kFailMsfRC,"Unable to create the file '%s'.",cmStringNullGuard(matchRptOutFn));
    goto errLabel;    
  }

  // allocate the graphics object
  if( cmScoreMatchGraphicAlloc( ctx, &smgH, scoreCsvFn, midiFn ) != kOkSmgRC )
  {
    rc = cmErrMsg(&err,kFailMsfRC,"Score Match Graphics allocation failed..");
    goto errLabel;    
  }

  // write the match report output file header
  _cmMsf_WriteMatchFileHeader(fH);

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

  _cmMsf_ReportScoreErrors(&sfr, scH );

  //_cmMsf_ReportMidiErrors(&sfr, scH, m, mN);

  //cmScorePrint(scH,&ctx->rpt);
  //cmMidiFilePrintMsgs( mfH, &ctx->rpt );

  // write the tracking match file as an SVG file.
  cmScoreMatchGraphicWrite( smgH, matchSvgOutFn );

  // write a cmTimeLine file which contains markers at each bar position
  if( tlBarOutFn != NULL )
    cmScoreMatchGraphicGenTimeLineBars(smgH, tlBarOutFn, srate );

  if( midiOutFn != NULL )
    cmScoreMatchGraphicUpdateMidiFromScore( ctx, smgH, midiOutFn );


 errLabel:
  
  cmFileClose(&fH);
  cmMemFree(sfr.rV);
  cmMidiFileClose(&mfH);
  cmScMatcherFree(&smp);
  cmScoreFinalize(&scH);
  cmScoreMatchGraphicFree(&smgH);

  cmCtxFree(&prCtx);

  //cmFsFreeFn(scoreCsvFn);
  //cmFsFreeFn(midiFn);
  //cmFsFreeFn(matchRptOutFn);
  //cmFsFreeFn(matchSvgOutFn);
  //cmFsFreeFn(outMidiFn);
  //cmFsFreeFn(tlBarFn);
  
  return rc;
}
