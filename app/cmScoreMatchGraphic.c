#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmTime.h"
#include "cmMidi.h"
#include "cmLex.h"
#include "cmCsv.h"
#include "cmSymTbl.h"
#include "cmMidiFile.h"
#include "cmAudioFile.h"
#include "cmTimeLine.h"
#include "cmText.h"
#include "cmFile.h"
#include "cmScore.h"
#include "cmScoreMatchGraphic.h"

enum
{
  kLocSmgFl     = 0x0001,
  kBarSmgFl     = 0x0002,
  kNoteSmgFl    = 0x0004,
  kPedalSmgFl   = 0x0008,
  kSostSmgFl    = 0x0010,
  kMidiSmgFl    = 0x0020,
  kNoMatchSmgFl = 0x0040
};

// Graphic box representing a score label or MIDI event
typedef struct cmSmgBox_str
{
  unsigned             flags;
  unsigned             id;    // csvEventId or midiUid
  unsigned             left;
  unsigned             top;
  unsigned             width;
  unsigned             height;
  cmChar_t*            text0;
  cmChar_t*            text1;
  struct cmSmgBox_str* link;  
} cmSmgBox_t;

// Graphic line linking secondary MIDI matches to score events
typedef struct cmSmgLine_str
{
  cmSmgBox_t*           b0;
  cmSmgBox_t*           b1;
  struct cmSmgLine_str* link;
} cmSmgLine_t;

// Score Location
typedef struct cmSmgLoc_str
{
  cmSmgBox_t* bV;       // List of graphic boxes assigned to this score location
} cmSmgLoc_t;

// Score label
typedef struct
{
  unsigned    type;          // kBarEvtScId | kNonEvtScId | kPedalEvtScId
  unsigned    barNumb;
  unsigned    csvEventId;
  unsigned    locIdx;
  cmSmgBox_t* box;
} cmSmgSc_t;

// Link a MIDI event to it's matched score label.
typedef struct cmSmgMatch_str
{
  cmSmgSc_t*             score;
  struct cmSmgMatch_str* link;
} cmSmgMatch_t;

// MIDI file event
typedef struct           
{
  double        secs;     
  unsigned      uid;
  unsigned      pitch;
  unsigned      vel;
  cmSmgMatch_t* matchV;    // list of matches to score events
  cmSmgBox_t*   box;
} cmSmgMidi_t;


typedef struct
{
  cmErr_t      err;

  cmChar_t*    scFn;
  cmSmgSc_t*   scV;    // scV[scN] score bars,notes, pedals
  unsigned     scN;
  
  cmSmgLoc_t*  locV;   // locV[locN] score locations (from the score file)
  unsigned     locN;

  cmSmgLine_t* lines;  // Graphic lines used to indicate that a midi event matches to multiple score notes.
                       // (Each match after the first gets a line from the box representing the midi event
                       //  to the matching score event)

  cmChar_t*    mfFn;   // MIDI file name
  cmSmgMidi_t* mV;     // mV[mN] midi note-on events
  unsigned     mN;
  double       mfDurSecs; // midi file duration in seconds
  
  unsigned     boxW;   // graphic box width and height
  unsigned     boxH;
} cmSmg_t;

cmSmgH_t cmSmgNullHandle = cmSTATIC_NULL_HANDLE;

cmSmg_t* _cmSmgHandleToPtr( cmSmgH_t h )
{
  cmSmg_t* p = (cmSmg_t*)h.h;
  assert(p!=NULL);
  return p;
}

cmSmgRC_t _cmSmgFree( cmSmg_t* p )
{
  unsigned i;

  
  for(i=0; i<p->mN; ++i)
  {
    cmSmgMatch_t* m0 = p->mV[i].matchV;
    cmSmgMatch_t* m1 = NULL;
    while(m0!=NULL)
    {
      m1 = m0->link;
      cmMemFree(m0);
      m0 = m1;
    }    
  }

  for(i=0; i<p->locN; ++i)
  {
    cmSmgBox_t* b0 = p->locV[i].bV;
    cmSmgBox_t* b1 = NULL;
    while(b0!=NULL)
    {
      b1 = b0->link;
      cmMemFree(b0->text0);
      cmMemFree(b0->text1);
      cmMemFree(b0);
      b0 = b1;
    }
  }

  cmSmgLine_t* l0 = p->lines;
  cmSmgLine_t* l1 = NULL;
  while( l0 != NULL )
  {
    l1 = l0->link;
    cmMemFree(l0);
    l0 = l1;
  }

  cmMemFree(p->scFn);
  cmMemFree(p->mfFn);
  cmMemFree(p->scV);
  cmMemFree(p->mV);
  cmMemFree(p->locV);
  cmMemFree(p);
  return kOkSmgRC;
}

cmSmgBox_t*  _cmSmgInsertBox( cmSmg_t* p, unsigned locIdx, unsigned flags, unsigned id, cmChar_t* text0, cmChar_t* text1 )
{
  assert( locIdx < p->locN );
  
  cmSmgBox_t* b = cmMemAllocZ(cmSmgBox_t,1);
  b->flags  = flags;
  b->id     = id;
  b->text0  = text0;
  b->text1  = text1;

  if( p->locV[locIdx].bV == NULL )
  {
    p->locV[locIdx].bV = b;
  }
  else
  {
    cmSmgBox_t* b0 = p->locV[locIdx].bV;
    while( b0->link!=NULL )
      b0 = b0->link;

    b0->link = b;
  }

  return b;
}

cmSmgRC_t _cmSmgInitFromScore( cmCtx_t* ctx, cmSmg_t* p, const cmChar_t* scoreFn )
{
  cmSmgRC_t  rc    = kOkSmgRC;
  cmScH_t    scH   = cmScNullHandle;
  unsigned   i,j,k;
  
  if( cmScoreInitialize(ctx,&scH,scoreFn,44100.0, NULL, 0, NULL, NULL, cmSymTblNullHandle ) != kOkScRC )
    return cmErrMsg(&p->err,kScoreFailSmgRC,"Score initializatio failed on '%s'.",cmStringNullGuard(scoreFn));

  p->scFn = cmMemAllocStr(scoreFn);
  p->scN  = cmScoreEvtCount(scH);
  p->scV  = cmMemAllocZ(cmSmgSc_t,p->scN);
  
  p->locN = cmScoreLocCount(scH);
  p->locV = cmMemAllocZ(cmSmgLoc_t,p->locN);

  // for each score location
  for(i=0,k=0; i<cmScoreLocCount(scH); ++i)
  {
    cmScoreLoc_t* l = cmScoreLoc(scH,i);

    // insert the location label box
    _cmSmgInsertBox(p, i, kLocSmgFl, cmInvalidId, cmTsPrintfP(NULL,"%i",i), NULL );
    
    // for each event in location i
    for(j=0; j<l->evtCnt; ++j)
    {
      const cmScoreEvt_t* e = l->evtArray[j];
      unsigned flags = kNoMatchSmgFl;
      cmChar_t* text = NULL;
      
      switch( e->type)
      {
        case kNonEvtScId:
          flags |= kNoteSmgFl;
          text   = cmMemAllocStr( cmMidiToSciPitch( e->pitch, NULL, 0));
          break;
          
        case kBarEvtScId:
          flags |= kBarSmgFl;
          text   = cmTsPrintfP(NULL,"%i",e->barNumb);
          break;
          
        case kPedalEvtScId:
          flags |= kPedalSmgFl;
          
          text = cmTsPrintfP(NULL,"%s", cmIsFlag(e->flags,kPedalDnScFl)?"v":"^");

          if( e->pitch == kSostenutoCtlMdId )
            flags |= kSostSmgFl;
          
          break;
      }

      // if e is a score event of interest then store a reference to it
      if( flags != kNoMatchSmgFl )
      {            
        assert( k < p->scN );

        p->scV[k].type       = e->type;
        p->scV[k].csvEventId = e->csvEventId;
        p->scV[k].locIdx     = i;
        p->scV[k].barNumb    = e->barNumb;
        
        p->scV[k].box = _cmSmgInsertBox(p, i, flags, e->csvEventId, text, NULL );
          
        k += 1;
      }
    }
  }
  
  p->scN = k;
  
  cmScoreFinalize(&scH);
  
  return rc;
}

cmSmgRC_t _cmSmgInitFromMidi( cmCtx_t* ctx, cmSmg_t* p, const cmChar_t* midiFn )
{
  cmSmgRC_t     rc  = kOkSmgRC;
  cmMidiFileH_t mfH = cmMidiFileNullHandle;
  unsigned      i,j;
  
  if( cmMidiFileOpen(ctx, &mfH, midiFn ) != kOkMfRC )
    return cmErrMsg(&p->err,kMidiFileFailSmgRC,"MIDI file open failed on '%s'.",cmStringNullGuard(midiFn));

  const cmMidiTrackMsg_t** mV  = cmMidiFileMsgArray(mfH);
  unsigned                 mN  = cmMidiFileMsgCount(mfH);

  p->mV        = cmMemAllocZ(cmSmgMidi_t,mN);
  p->mN        = mN;
  p->mfDurSecs = cmMidiFileDurSecs(mfH);
  p->mfFn      = cmMemAllocStr(midiFn);
  
  for(i=0,j=0; i<mN; ++i)
    if( (mV[i]!=NULL) && cmMidiIsChStatus(mV[i]->status) && cmMidiIsNoteOn(mV[i]->status) && (mV[i]->u.chMsgPtr->d1>0) )          
    {
      assert(j < mN);
      p->mV[j].secs  = mV[i]->amicro / 1000000.0;
      p->mV[j].uid   = mV[i]->uid;
      p->mV[j].pitch = mV[i]->u.chMsgPtr->d0;
      p->mV[j].vel   = mV[i]->u.chMsgPtr->d1;
      
      ++j;
    }

  p->mN = j;
  
  cmMidiFileClose(&mfH);

  return rc;
}

cmSmgRC_t cmScoreMatchGraphicAlloc( cmCtx_t* ctx, cmSmgH_t* hp, const cmChar_t* scoreFn, const cmChar_t* midiFn )
{
  cmSmgRC_t rc;
  if((rc = cmScoreMatchGraphicFree(hp)) != kOkSmgRC )
    return rc;

  cmSmg_t* p = cmMemAllocZ(cmSmg_t,1);
  cmErrSetup(&p->err,&ctx->rpt,"ScoreMatchGraphic");

  if((rc = _cmSmgInitFromScore(ctx,p,scoreFn)) != kOkSmgRC )
    goto errLabel;

  if((rc = _cmSmgInitFromMidi(ctx,p,midiFn)) != kOkSmgRC )
    goto errLabel;

  p->boxW = 30;
  p->boxH = 50;
  hp->h = p;

 errLabel:
  if( rc != kOkSmgRC )
    _cmSmgFree(p);
  
  return rc;
}

bool cmScoreMatchGraphicIsValid( cmSmgH_t h )
{ return h.h != NULL; }

cmSmgRC_t cmScoreMatchGraphicFree( cmSmgH_t* hp )
{
  cmSmgRC_t rc = kOkSmgRC;
  
  if(hp==NULL || cmScoreMatchGraphicIsValid(*hp)==false)
    return kOkSmgRC;

  cmSmg_t* p = _cmSmgHandleToPtr(*hp);

  if((rc = _cmSmgFree(p)) != kOkSmgRC )
    return rc;

  hp->h = NULL;
  
  return rc;
}

bool cmScoreMatchGraphic( cmSmgH_t h )
{ return h.h != NULL; }

cmSmgRC_t cmScoreMatchGraphicInsertMidi( cmSmgH_t h, unsigned midiUid, unsigned midiPitch, unsigned midiVel, unsigned csvScoreEventId )
{
  cmSmg_t* p = _cmSmgHandleToPtr(h);
  unsigned i,j;

  // if this midi event did not match any score records
  if( csvScoreEventId == cmInvalidId )
    return kOkSmgRC;
  
  assert(midiUid != cmInvalidId );
  assert(midiPitch<128 && midiVel<128);

  // find the midi file record which matches the event
  for(i=0; i<p->mN; ++i)
    if( p->mV[i].uid == midiUid )
    {
      // find the score record which matches the score event id
      for(j=0; j<p->scN; ++j)
        if( p->scV[j].csvEventId == csvScoreEventId )
        {
          // create a match record
          cmSmgMatch_t* m = cmMemAllocZ(cmSmgMatch_t,1);
          
          m->score = p->scV + j;

          // mark the box associated with this score record as 'matched' by clearing the kNoMatchSmgFl
          p->scV[j].box->flags = cmClrFlag(p->scV[j].box->flags,kNoMatchSmgFl);

          // insert the match record in the midi files match list
          if( p->mV[i].matchV == NULL )
            p->mV[i].matchV = m;
          else
          {
            cmSmgMatch_t* m0 = p->mV[i].matchV;
            while( m0->link != NULL )
              m0 = m0->link;

            m0->link = m;
          }

          return kOkSmgRC;
        }

      return cmErrMsg(&p->err,kScoreFailSmgRC,"The score csv event id %i not found,",csvScoreEventId);
    }
  
  return cmErrMsg(&p->err,kMidiFileFailSmgRC,"MIDI uid %i not found.",midiUid);
}

// Create a box for each MIDI event and a line for each
// match beyond the first.
void _cmSmgResolveMidi( cmSmg_t* p )
{
  unsigned prevLocIdx = 0;
  unsigned i;
  
  // for each midi record
  for(i=0; i<p->mN; ++i)
  {
    // get the first match record for this MIDI event
    const cmSmgMatch_t* m = p->mV[i].matchV;

    // get the score location for this midi event
    unsigned locIdx = m==NULL ? prevLocIdx : m->score->locIdx;

    unsigned flags  = kMidiSmgFl | (m==NULL ? kNoMatchSmgFl : 0);
    
    // set the text label for this event
    cmChar_t* text  = cmMemAllocStr( cmMidiToSciPitch( p->mV[i].pitch, NULL, 0));

    // insert a box to represent this midi event
    cmSmgBox_t* box = _cmSmgInsertBox( p, locIdx, flags, p->mV[i].uid, text, cmTsPrintfP(NULL,"%i",p->mV[i].uid) );

    prevLocIdx = locIdx;

    // if this midi event matched to multiple score positions
    if( m != NULL && m->link != NULL )
    {
      // insert a line for each match after the first
      m = m->link;
      for(; m!=NULL; m=m->link )
      {
        cmSmgLine_t* l = cmMemAllocZ(cmSmgLine_t,1);
        l->b0 = box;
        l->b1 = m->score->box;

        l->link  = p->lines;
        p->lines = l;
      }
    }    
  }
}

void _cmSmgLayout( cmSmg_t* p )
{
  unsigned i;
  unsigned bordX = 5;
  unsigned bordY = 5;
  unsigned top   = p->boxH + 2*bordY;
  unsigned left  = bordX;
  
  for(i=0; i<p->locN; ++i)
  {
    cmSmgLoc_t* l = p->locV + i;
    cmSmgBox_t* b = l->bV;

    // for each box attached to this location
    for(; b!=NULL; b=b->link)
    {
      // bar boxes are always drawn at the top of the column
      if( cmIsFlag(b->flags,kBarSmgFl) )
        b->top = bordY;
      else
      {
        b->top = top;
        top   += p->boxH + bordY;
      }
      
      b->left   = left;
      b->width  = p->boxW;
      b->height = p->boxH;
    }

    left += p->boxW + bordX;    
    top   = p->boxH + 2*bordY;
  }
}

void _cmSmgSvgSize( cmSmg_t* p, unsigned* widthRef, unsigned* heightRef )
{
  unsigned i;
  unsigned maxWidth  = 0;
  unsigned maxHeight = 0;
  
  for(i=0; i<p->locN; ++i)
  {
    cmSmgBox_t* b = p->locV[i].bV;
    for(; b != NULL; b=b->link )
    {
      if( b->left + b->width > maxWidth )
        maxWidth = b->left + b->width;
      
      if( b->top + b->height > maxHeight )
        maxHeight = b->top + b->height;
    }
  }

  *widthRef  = maxWidth;
  *heightRef = maxHeight;
}

cmSmgRC_t cmScoreMatchGraphicWrite( cmSmgH_t h, const cmChar_t* fn )
{
  cmSmg_t*  p         = _cmSmgHandleToPtr(h);
  cmFileH_t fH        = cmFileNullHandle;
  unsigned  svgHeight = 0;
  unsigned  svgWidth  = 0;
  unsigned  i;

  
  // BUG BUG BUG : this can only be called once
  // create a box for each midi event
  _cmSmgResolveMidi( p );

  // layout the boxes
  _cmSmgLayout( p );

  
  if( cmFileOpen(&fH,fn,kWriteFileFl,p->err.rpt) != kOkFileRC )
    return cmErrMsg(&p->err,kFileFailScRC,"Graphic file create failed for '%s'.",cmStringNullGuard(fn));

  _cmSmgSvgSize(p,&svgWidth,&svgHeight);

  svgHeight += 10; // add a right and lower border
  svgWidth  += 10;

  cmFilePrintf(fH,"<!DOCTYPE html>\n<html>\n<head><link rel=\"stylesheet\" type=\"text/css\" href=\"score0.css\"></head><body>\n<svg width=\"%i\" height=\"%i\">\n",svgWidth,svgHeight);

  for(i=0; i<p->locN; ++i)
  {
    cmSmgBox_t* b = p->locV[i].bV;
    for(; b != NULL; b=b->link )
    {
      const cmChar_t* classStr = "score";

      if( cmIsFlag(b->flags,kLocSmgFl) )
        classStr = "loc";
      
      if( cmIsFlag(b->flags,kMidiSmgFl) )
        classStr = "midi";

      if( cmIsFlag(b->flags,kNoMatchSmgFl) )
        if( cmIsFlag(b->flags,kMidiSmgFl) )
          classStr = "midi_miss";
      
      if( cmIsFlag(b->flags,kNoMatchSmgFl) )
        if( cmIsFlag(b->flags,kNoteSmgFl) )
          classStr = "score_miss";

      if( cmIsFlag(b->flags,kBarSmgFl) )
        classStr = "bar";

      if( cmIsFlag(b->flags,kPedalSmgFl) )
      {
        if( cmIsFlag(b->flags,kSostSmgFl) )
          classStr = "sost";
        else
          classStr = "damper";
      }
      
      if( cmFilePrintf(fH,"<rect x=\"%i\" y=\"%i\" width=\"%i\" height=\"%i\" class=\"%s\"/>\n",b->left,b->top,b->width,b->height,classStr) != kOkFileRC )
        return cmErrMsg(&p->err,kFileFailScRC,"File write failed on graphic file rect output.");

      if( b->text0 != NULL )
      {
        unsigned tx = b->left + b->width/2;
        unsigned ty = b->top  + 20;
        
        if( cmFilePrintf(fH,"<text x=\"%i\" y=\"%i\" text-anchor=\"middle\" class=\"stext\">%s</text>\n",tx,ty,b->text0) != kOkFileRC )
          return cmErrMsg(&p->err,kFileFailScRC,"File write failed on graphic file text output.");
      }

      if( b->text1 != NULL )
      {
        unsigned tx = b->left + b->width/2;
        unsigned ty = b->top  + 20 + 20;
        
        if( cmFilePrintf(fH,"<text x=\"%i\" y=\"%i\" text-anchor=\"middle\" class=\"stext\">%s</text>\n",tx,ty,b->text1) != kOkFileRC )
          return cmErrMsg(&p->err,kFileFailScRC,"File write failed on graphic file text output.");
      }
      
    }
  }

  cmSmgLine_t* l = p->lines;
  for(; l!=NULL; l=l->link)
  {
    unsigned x0 = l->b0->left + l->b0->width/2;
    unsigned y0 = l->b0->top  + l->b0->height/2;
    unsigned x1 = l->b1->left + l->b1->width/2;
    unsigned y1 = l->b1->top  + l->b1->height/2;
    
    if( cmFilePrintf(fH,"<line x1=\"%i\" y1=\"%i\" x2=\"%i\" y2=\"%i\" class=\"sline\"/>\n",x0,y0,x1,y1) != kOkFileRC )
      return cmErrMsg(&p->err,kFileFailScRC,"File write failed on graphic file line output.");
  }
    
  cmFilePrint(fH,"</svg>\n</body>\n</html>\n");

  cmFileClose(&fH);
  return kOkSmgRC;
}


cmSmgRC_t cmScoreMatchGraphicGenTimeLineBars( cmSmgH_t h, const cmChar_t* fn, unsigned srate )
{
  cmSmgRC_t rc = kOkSmgRC;
  cmFileH_t f  = cmFileNullHandle;
  cmSmg_t*  p  = _cmSmgHandleToPtr(h);
  unsigned  i  = 0;

  if( cmFileOpen(&f,fn,kWriteFileFl,p->err.rpt)  != kOkFileRC )
    return cmErrMsg(&p->err,kFileSmgRC,"The time-line bar file '%s' could not be created.",cmStringNullGuard(fn));

  for(i=0; i<p->mN; ++i)
    if( p->mV[i].matchV != NULL && p->mV[i].matchV->score != NULL && p->mV[i].matchV->score > p->scV && p->mV[i].matchV->score[-1].type==kBarEvtScId )      
    {
      unsigned bar = p->mV[i].matchV->score->barNumb;
      unsigned offset = p->mV[i].secs * srate;
      unsigned smpCnt = p->mfDurSecs * srate - offset;
      cmFilePrintf(f,"{ label: \"%i\" type: \"mk\" ref: \"mf-0\" offset: %8i smpCnt:%8i trackId: 0 textStr: \"Bar %3i\" bar: %3i sec:\"%3i\" }\n",bar,offset,smpCnt,bar,bar,bar);
    }

  cmFileClose(&f);
  return rc;
  
}
/*
cmSmRC_t _cmScoreMatchGraphicUpdateSostenuto( cmSmg_t* p, cmMidiFileH_t mfH, cmScH_t scH )
{
  unsigned evtN = cmScoreEvtCount(scH);
  unsigned i;
  const cmScoreEvt_t* e;
  const cmScoreEvt_t* e0 = NULL;
  for(i=0; i<evtN; ++i)

    if( e->type == kNonEvtScId )
      
      
    
      
    if( e->type == kPedalEvtScId && e->pitch == kSostenutoCtlMdId )
    {
      
    }
}
*/
cmSmgRC_t cmScoreMatchGraphicUpdateMidiFromScore( cmCtx_t* ctx, cmSmgH_t h, const cmChar_t* newMidiFn )
{
  cmSmgRC_t     rc  = kOkSmgRC;
  cmSmg_t*      p   = _cmSmgHandleToPtr(h);
  unsigned      i   = 0;
  cmMidiFileH_t mfH = cmMidiFileNullHandle;
  cmScH_t       scH = cmScNullHandle;

  // open the MIDI file
  if( cmMidiFileOpen(ctx, &mfH, p->mfFn ) != kOkMfRC )
    return cmErrMsg(&p->err,kMidiFileFailSmgRC,"MIDI file open failed on '%s'.",cmStringNullGuard(p->mfFn));

  // instantiate the score from the score CSV file
  if( cmScoreInitialize(ctx,&scH,p->scFn,44100.0, NULL, 0, NULL, NULL, cmSymTblNullHandle ) != kOkScRC )
  {
    rc = cmErrMsg(&p->err,kScoreFailSmgRC,"Score initializatio failed on '%s'.",cmStringNullGuard(p->scFn));
    goto errLabel;
  } 

  // for each MIDI note-on event
  for(i=0; i<p->mN; ++i)
  {
    cmSmgMidi_t* mr = p->mV + i;

    // only update midi events which were matched exactly once
    if( mr->matchV==NULL || mr->matchV->link!=NULL  )
      continue;

    // locate the matched score event
    const cmScoreEvt_t* s= cmScoreIdToEvt( scH, mr->matchV->score->csvEventId );
    assert( s!=NULL );

    // assign the score velocity to the MIDI note
    if(cmMidiFileSetVelocity( mfH, mr->uid, s->vel ) != kOkMfRC )
    {
      rc = cmErrMsg(&p->err,kOpFailSmgRC,"Set velocify operation failed.");
      goto errLabel;
    }

  }

  // write the updated MIDI file
  if( cmMidiFileWrite( mfH, newMidiFn ) != kOkMfRC )
  {
    rc = cmErrMsg(&p->err,kMidiFileFailSmgRC,"MIDI file write failed on '%s'.",cmStringNullGuard(newMidiFn));
    goto errLabel;
  }


 errLabel:
  cmMidiFileClose(&mfH);
  cmScoreFinalize(&scH);
  
  return rc;
 
}
