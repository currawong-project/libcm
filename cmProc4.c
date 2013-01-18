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
#include "cmFileSys.h"
#include "cmJson.h"
#include "cmSymTbl.h"
#include "cmAudioFile.h"
#include "cmText.h"
#include "cmProcObj.h"
#include "cmProcTemplate.h"
#include "cmMath.h"
#include "cmProc.h"
#include "cmVectOps.h"
#include "cmMidi.h"
#include "cmMidiFile.h"
#include "cmTimeLine.h"
#include "cmScore.h"
#include "cmProc4.h"
#include "cmTime.h"


cmScFol* cmScFolAlloc( cmCtx* c, cmScFol* p, cmReal_t srate, cmScH_t scH, unsigned bufN, unsigned minWndLookAhead, unsigned maxWndCnt, unsigned minVel )
{
  cmScFol* op = cmObjAlloc(cmScFol,c,p);
  if( srate != 0 )
    if( cmScFolInit(op,srate,scH,bufN,minWndLookAhead,maxWndCnt,minVel) != cmOkRC )
      cmScFolFree(&op);
  return op;
}

cmRC_t   cmScFolFree(  cmScFol** pp )
{
  cmRC_t rc = cmOkRC;
  if( pp==NULL || *pp==NULL )
    return rc;

  cmScFol* p = *pp;
  if((rc = cmScFolFinal(p)) != cmOkRC )
    return rc;

  unsigned i;
  for(i=0; i<p->locN; ++i)
    cmMemFree(p->loc[i].evtV);

  cmMemFree(p->loc);
  cmMemFree(p->bufV);
  cmObjFree(pp);
  return rc;
}


cmRC_t   cmScFolFinal( cmScFol* p )
{ 
  cmMemFree(p->edWndMtx);
  return cmOkRC; 
}

void _cmScFolPrint( cmScFol* p )
{
  int i,j;
  for(i=0; i<p->locN; ++i)
  {
    printf("%2i %5i ",p->loc[i].barNumb,p->loc[i].scIdx);
    for(j=0; j<p->loc[i].evtCnt; ++j)
      printf("%s ",cmMidiToSciPitch(p->loc[i].evtV[j].pitch,NULL,0));
    printf("\n");
  }
}

unsigned* _cmScFolAllocEditDistMtx(unsigned maxN)
{
  maxN += 1;

  unsigned* m = cmMemAllocZ(unsigned,maxN*maxN);
  unsigned* p = m;
  unsigned i;

  // initialize the comparison matrix with the default costs in the
  // first row and column
  // (Note that this matrix is not oriented in column major order like most 'cm' matrices.)
  for(i=0; i<maxN; ++i)
  {
    p[i]          = i;		// 0th row
    p[ i * maxN ] = i;		// 0th col
  }

  return m;
}

cmRC_t   cmScFolInit(  cmScFol* p, cmReal_t srate, cmScH_t scH, unsigned bufN, unsigned minWndLookAhead, unsigned maxWndCnt, unsigned minVel )
{
  cmRC_t rc;
  if((rc = cmScFolFinal(p)) != cmOkRC )
    return rc;

  if( bufN > maxWndCnt )
    return cmCtxRtCondition( &p->obj, cmInvalidArgRC, "The score follower buffer count (%i) must be less than the max. window length (%i).",bufN,maxWndCnt );

  if( minWndLookAhead > maxWndCnt )
    return cmCtxRtCondition( &p->obj, cmInvalidArgRC, "The score follower look-ahead count (%i) must be less than the max. window length (%i).",minWndLookAhead,maxWndCnt); 

  p->srate          = srate;
  p->scH            = scH;
  p->bufN           = bufN;
  p->bufV           = cmMemResizeZ(cmScFolBufEle_t,p->bufV,bufN);
  p->locN           = cmScoreEvtCount(scH);
  p->loc            = cmMemResizeZ(cmScFolLoc_t,p->loc,p->locN);
  p->sbi            = cmInvalidIdx;
  p->sei            = cmInvalidIdx;
  p->msln           = minWndLookAhead;
  p->mswn           = maxWndCnt;
  p->forwardCnt     = 2;
  p->maxDist        = 4;
  p->edWndMtx       = _cmScFolAllocEditDistMtx(p->bufN);
  p->minVel         = minVel;
  p->printFl        = true;
  p->noBackFl       = true;
  p->missCnt        = 0;
  p->matchCnt       = 0;
  p->eventIdx       = 0;
  p->skipCnt        = 0;
  p->ret_idx        = cmInvalidIdx;

  // for each score location
  unsigned li,ei;
  
  for(li=0,ei=0; li<cmScoreLocCount(p->scH); ++li)
  {
    unsigned i,n;

    const cmScoreLoc_t* lp = cmScoreLoc(p->scH,li);

    // count the number of note events at location li
    for(n=0,i=0; i<lp->evtCnt; ++i)
      if( lp->evtArray[i]->type == kNonEvtScId )
        ++n;

    assert( ei+n <= p->locN );

    // duplicate each note at location li n times
    for(i=0; i<n; ++i)
    {
      unsigned j,k;

      p->loc[ei+i].evtCnt = n;
      p->loc[ei+i].evtV   = cmMemAllocZ(cmScFolEvt_t,n);
      p->loc[ei+i].scIdx    = li;
      p->loc[ei+i].barNumb  = lp->barNumb;
      for(j=0,k=0; j<lp->evtCnt; ++j)
        if( lp->evtArray[j]->type == kNonEvtScId )
        {
          p->loc[ei+i].evtV[k].pitch    = lp->evtArray[j]->pitch;
          p->loc[ei+i].evtV[k].scEvtIdx = lp->evtArray[j]->index;
          ++k;
        }

    }

    ei += n;
    
  }

  p->locN = ei;

  //_cmScFolPrint(p);

  return rc;
}

cmRC_t   cmScFolReset(   cmScFol* p, unsigned scEvtIdx )
{
  int i,j;

  // empty the event buffer
  memset(p->bufV,0,sizeof(cmScFolBufEle_t)*p->bufN);

  // don't allow the score index to be prior to the first note
  //if( scEvtIdx < p->loc[0].scIdx )
  //  scEvtIdx = p->loc[0].scIdx;

  p->sei      = cmInvalidIdx;
  p->sbi      = cmInvalidIdx;
  p->missCnt  = 0;
  p->matchCnt = 0;
  p->eventIdx = 0;
  p->skipCnt  = 0;
  p->ret_idx  = cmInvalidIdx;

  // locate the score element in svV[] that is closest to,
  // and possibly after, scEvtIdx.
  for(i=0; i<p->locN-1; ++i)
  {
    for(j=0; j<p->loc[i].evtCnt; ++j)
      if( p->loc[i].evtV[j].scEvtIdx <= scEvtIdx )
        p->sbi = i;
      else
        break;
  }

  // locate the score element at the end of the look-ahead region
  for(; i<p->locN; ++i)
  {
    for(j=0; j<p->loc[i].evtCnt; ++j)
      if( p->loc[i].evtV[j].scEvtIdx <= scEvtIdx + p->msln )
        p->sei = i;
  }

  return cmOkRC;
}

bool  _cmScFolIsMatch( const cmScFolLoc_t* loc, unsigned pitch )
{
  unsigned i;
  for(i=0; i<loc->evtCnt; ++i)
    if( loc->evtV[i].pitch == pitch )
      return true;
  return false;
}

int _cmScFolMatchCost( const cmScFolLoc_t* loc, unsigned li, const cmScFolBufEle_t* pitch, unsigned pi )
{
  if( _cmScFolIsMatch(loc+li,pitch[pi].val) )
    return 0;
  
  if( li>0 && pi>0 )
    if( _cmScFolIsMatch(loc+li-1,pitch[pi].val) && _cmScFolIsMatch(loc+li,pitch[pi-1].val) )
      return 0;

  return 1;
}

int _cmScFolDist(unsigned mtxMaxN, unsigned* m, const cmScFolBufEle_t* s1, const cmScFolLoc_t* s0, int n )
{
  mtxMaxN += 1;

  assert( n < mtxMaxN );
  
  int			  v		= 0;
  unsigned  i;
  // Note that m[maxN,maxN] is not oriented in column major order like most 'cm' matrices.

  for(i=1; i<n+1; ++i)
  {
    unsigned ii 	= i  * mtxMaxN;	// current row
    unsigned i_1 	= ii - mtxMaxN;	// previous row
    unsigned j;	
    for( j=1; j<n+1; ++j)
    {
      //int cost = s0[i-1] == s1[j-1] ? 0 : 1;
      //int cost = _cmScFolIsMatch(s0 + i-1, s1[j-1]) ? 0 : 1;
      int cost = _cmScFolMatchCost(s0,i-1,s1,j-1);

      //m[i][j] = min( m[i-1][j] + 1, min( m[i][j-1] + 1, m[i-1][j-1] + cost ) );
					
      m[ ii + j ] = v = cmMin( m[ i_1 + j] + 1, cmMin( m[ ii + j - 1] + 1, m[ i_1 + j - 1 ] + cost ) );
    }
  }	
  return v;		
}

void _cmScFolRpt0( cmScFol* p, unsigned locIdx, unsigned locN, const cmScFolBufEle_t* b, unsigned bn, unsigned min_idx )
{
  unsigned i;
  int      n;

  printf("--------------- event:%i -------------  \n",p->eventIdx);

  printf("loc: ");
  for(i=0; i<locN; ++i)
    printf("%4i ",i+locIdx);
  printf("\n");

  for(n=0,i=0; i<locN; ++i)
    if( p->loc[locIdx+i].evtCnt > n )
      n = p->loc[locIdx+i].evtCnt;

  --n;
  for(; n>=0; --n)
  {
    printf("sc%1i: ",n);
    for(i=0; i<locN; ++i)
    {
      if( n < p->loc[locIdx+i].evtCnt )
        printf("%4s ",cmMidiToSciPitch(p->loc[locIdx+i].evtV[n].pitch,NULL,0));
      else
        printf("     ");
    }
    printf("\n");
  }

  printf("perf:");
  for(i=0; i<min_idx; ++i)
    printf("     ");

  for(i=0; i<bn; ++i)
    printf("%4s ",cmMidiToSciPitch(b[i].val,NULL,0));

  printf("\n");
}

void _cmScFolRpt1( cmScFol*p, unsigned minDist, unsigned ret_idx, unsigned d1, unsigned missCnt, unsigned matchCnt )
{
  printf("dist:%i miss:%i match:%i skip:%i vel:%i ",minDist,missCnt,matchCnt,p->skipCnt,d1);
  if( ret_idx != cmInvalidIdx )
    printf("ret_idx:%i ",ret_idx);
  printf("\n");
}

unsigned   cmScFolExec(  cmScFol* p, unsigned smpIdx, unsigned status, cmMidiByte_t d0, cmMidiByte_t d1 )
{
  unsigned ret_idx = cmInvalidIdx;

  if( p->sbi == cmInvalidIdx )
  {
    cmCtxRtCondition( &p->obj, cmInvalidArgRC, "An initial score search location has not been set." );
    return ret_idx;
  }

  if( status != kNoteOnMdId )
    return ret_idx;

  ++p->eventIdx;

  // reject notes with very low velocity
  if( d1 < p->minVel )
  {
    ++p->skipCnt;
    return ret_idx;
  }

  // left shift bufV[] to make the right-most element available - then copy in the new element
  memmove(p->bufV, p->bufV+1, sizeof(cmScFolBufEle_t)*(p->bufN-1));
  p->bufV[ p->bufN-1 ].smpIdx = smpIdx;
  p->bufV[ p->bufN-1 ].val    = d0;
  p->bufV[ p->bufN-1 ].validFl= true;

  // fill in ebuf[] with the valid values in bufV[]
  int en = cmMin(p->eventIdx,p->bufN);
  int bbi  = p->eventIdx>=p->bufN ? 0 : p->bufN-p->eventIdx;


  // en is the count of valid elements in ebuf[].
  // ebuf[p->boi] is the first valid element

  int    j       = 0;
  int    minDist = INT_MAX;
  int    minIdx  = cmInvalidIdx;
  int    dist;

  // the score wnd must always be as long as the buffer n
  // at the end of the score this may not be the case 
  // (once sei hits locN - at this point we must begin
  // shrinking ewnd[] to contain only the last p->sei-p->sbi+1 elements)
  assert( p->sei-p->sbi+1 >= en );

  for(j=0; p->sbi+en+j-1 <= p->sei; ++j)
  {
    // use <= minDist to choose the latest window with the lowest match
    if((dist = _cmScFolDist(p->bufN, p->edWndMtx, p->bufV+bbi, p->loc + p->sbi+j, en )) < minDist )
    {
      // only make an eql match if the posn is greater than the last location 
      if( dist==minDist && p->ret_idx != cmInvalidId && p->ret_idx >= p->sbi+minIdx+en-1 )
        continue;

      minDist = dist;
      minIdx  = j;
    }
  }

  // The best fit is on the score window: p->loc[sbi+minIdx : sbi+minIdx+en-1 ]

  if( p->printFl )
    _cmScFolRpt0( p, p->sbi, p->sei-p->sbi+1, p->bufV+bbi, en, minIdx );
    
  // save current missCnt for later printing
  unsigned missCnt = p->missCnt;

  // if a perfect match occurred
  if( minDist == 0 )
  {
    ret_idx = p->sbi + minIdx + en - 1;       
    p->missCnt = 0;

    // we had a perfect match - shrink the window to it's minumum size
    p->sbi += (en==p->bufN) ? minIdx + 1 : 0;  // move wnd begin forward to just past first match
    p->sei  = p->sbi + minIdx + en + p->msln;  // move wnd end forward to lead by the min look-ahead
    

  }
  else
  {
    if( minDist > p->maxDist )
      ret_idx = cmInvalidIdx;
    else
    // if the last event matched - then return the match location as the current score location
    if( _cmScFolIsMatch(p->loc+(p->sbi+minIdx+en-1),p->bufV[p->bufN-1].val) )
    {
      ret_idx    = p->sbi + minIdx + en - 1;
      p->missCnt = 0;

      // this is probably a pretty good match reduce the part of the window prior to 
      // the first match (bring the end of the window almost up to the end of the 
      // buffers sync position)
      if( en >= p->bufN-1 && (en+2) <= ret_idx )
        p->sbi = ret_idx - (en+2);
              
    }
    else // the last event does not match based on the optimal edit-distance alignment
    {
      // Look backward from the closest match location for a match to the current pitch.
      // The backward search scope is limited by the current value of 'missCnt'.
      unsigned i;
      j = p->sbi+minIdx+en-2;
      for(i=1; i+1 <= p->bufN && j>=p->sbi && i<=p->missCnt; ++i,--j)
      {
        // if this look-back location already matched then stop the backward search
        if(_cmScFolIsMatch(p->loc+j,p->bufV[p->bufN-1-i].val))
          break;
        
        // does this look-back location match the current pitch
        if(_cmScFolIsMatch(p->loc+j,p->bufV[p->bufN-1].val))
        {
          ret_idx    = j;
          p->missCnt = i;  // set missCnt to the cnt of steps backward necessary for a match
          break;
        }
      }

      // If the backward search did not find a match - look forward
      if( ret_idx == cmInvalidIdx )
      {
        unsigned i;
        j = p->sbi+minIdx+en;
        for(i=0; j<=p->sei && i<p->forwardCnt; ++i,++j)
          if( _cmScFolIsMatch(p->loc+j,p->bufV[p->bufN-1].val) )
          {
            ret_idx = j;
            break;
          }

        p->missCnt = ret_idx == cmInvalidIdx ? p->missCnt + 1 : 0;
      }
        
    }

    // Adjust the end window position (sei)  based on the match location
    if( ret_idx == cmInvalidIdx )
    {
      // even though we didn't match move the end of the score window forward
      // this will enlarge the score window by one
      p->sei += 1;
    }
    else
    {
      assert( p->sei>=ret_idx);

      // force sei to lead by min look-ahead
      if( p->sei - ret_idx < p->msln )
        p->sei = ret_idx + p->msln;

    }

    assert( p->sei > p->sbi );

    // Adjust the begin window position
    if( p->noBackFl && ret_idx != cmInvalidIdx && en>=p->bufN && p->sbi > p->bufN )
      p->sbi = ret_idx - p->bufN;

    // if the score window length surpasses the max score window size 
    // move the beginning index forward 
    if( p->sei - p->sbi + 1 > p->mswn && p->sei > p->mswn )
      p->sbi = p->sei - p->mswn + 1;


  }

  if( p->printFl )
    _cmScFolRpt1(p, minDist, ret_idx, d1, missCnt, p->matchCnt );

  // don't allow the returned location to repeat or go backwards
  if( p->noBackFl && p->ret_idx != cmInvalidIdx && ret_idx <= p->ret_idx )
    ret_idx = cmInvalidIdx;


  // track the number of consecutive matches
  if( ret_idx == cmInvalidIdx )
    p->matchCnt = 0;
  else
  {
    ++p->matchCnt;

    p->ret_idx = ret_idx;
  }

  // Force the window to remain valid when it is at the end of the score 
  //  - sbi and sei must be inside 0:locN
  //  - sei-sbi + 1 must be >= en
  if( p->sei >= p->locN )
  {
    p->sei = p->locN - 1;
    p->sbi = p->sei  - p->bufN + 1;
  }

  if( ret_idx != cmInvalidIdx )
    ret_idx = p->loc[ret_idx].scIdx;

  return ret_idx;
}



//=======================================================================================================================

cmScTrk* cmScTrkAlloc( cmCtx* c, cmScTrk* p, cmReal_t srate, cmScH_t scH, unsigned bufN, unsigned minWndLookAhead, unsigned maxWndCnt, unsigned minVel )
{
  cmScTrk* op = cmObjAlloc(cmScTrk,c,p);

  op->sfp = cmScFolAlloc(c,NULL,srate,scH,bufN,minWndLookAhead,maxWndCnt,minVel);

  if( srate != 0 )
    if( cmScTrkInit(op,srate,scH,bufN,minWndLookAhead,maxWndCnt,minVel) != cmOkRC )
      cmScTrkFree(&op);
  return op;
}

cmRC_t   cmScTrkFree(  cmScTrk** pp )
{
  cmRC_t rc = cmOkRC;
  if( pp==NULL || *pp==NULL )
    return rc;

  cmScTrk* p = *pp;
  if((rc = cmScTrkFinal(p)) != cmOkRC )
    return rc;

  cmScFolFree(&p->sfp);

  cmObjFree(pp);
  return rc;

}

void _cmScTrkPrint( cmScTrk* p )
{
  int i,j;
  for(i=0; i<p->locN; ++i)
  {
    printf("%2i %5i ",p->loc[i].barNumb,p->loc[i].scIdx);
    for(j=0; j<p->loc[i].evtCnt; ++j)
      printf("%s ",cmMidiToSciPitch(p->loc[i].evtV[j].pitch,NULL,0));
    printf("\n");
  }
}

cmRC_t   cmScTrkInit(  cmScTrk* p, cmReal_t srate, cmScH_t scH, unsigned bufN, unsigned minWndLookAhead, unsigned maxWndCnt, unsigned minVel )
{
  cmRC_t rc;
  if((rc = cmScTrkFinal(p)) != cmOkRC )
    return rc;

  if( minWndLookAhead > maxWndCnt )
    return cmCtxRtCondition( &p->obj, cmInvalidArgRC, "The score follower look-ahead count (%i) must be less than the max. window length (%i).",minWndLookAhead,maxWndCnt); 

  if((rc = cmScFolInit(p->sfp,srate,scH,bufN,minWndLookAhead,maxWndCnt,minVel)) != cmOkRC )
    return rc;
  
  p->srate          = srate;
  p->scH            = scH;
  p->locN           = cmScoreLocCount(scH);
  p->loc            = cmMemResizeZ(cmScTrkLoc_t,p->loc,p->locN);
  p->minVel         = minVel;
  p->maxWndCnt      = maxWndCnt;
  p->minWndLookAhead= 4; //minWndLookAhead;
  p->printFl        = true;
  p->curLocIdx      = cmInvalidIdx;
  p->evtIndex       = 0;

  // for each score location
  unsigned li;
  
  for(li=0; li<cmScoreLocCount(p->scH); ++li)
  {
    unsigned i,j,k,n;

    const cmScoreLoc_t* lp = cmScoreLoc(p->scH,li);

    // count the number of note events at location li
    for(n=0,i=0; i<lp->evtCnt; ++i)
      if( lp->evtArray[i]->type == kNonEvtScId )
        ++n;

    p->loc[li].evtCnt   = n;
    p->loc[li].evtV     = cmMemAllocZ(cmScTrkEvt_t,n);
    p->loc[li].scIdx    = li;
    p->loc[li].barNumb  = lp->barNumb;
    for(j=0,k=0; j<lp->evtCnt; ++j)
      if( lp->evtArray[j]->type == kNonEvtScId )
      {
        p->loc[li].evtV[k].pitch    = lp->evtArray[j]->pitch;
        p->loc[li].evtV[k].scEvtIdx = lp->evtArray[j]->index;
        ++k;
      }
  }

  //_cmScTrkPrint(p);

  return rc;
}

cmRC_t   cmScTrkFinal( cmScTrk* p )
{
  unsigned i;
  for(i=0; i<p->locN; ++i)
    cmMemPtrFree(&p->loc[i].evtV);

  return cmOkRC; 
}

cmRC_t   cmScTrkReset( cmScTrk* p, unsigned scEvtIdx )
{
  unsigned i;

  cmScFolReset(p->sfp,scEvtIdx);

  p->curLocIdx = cmInvalidIdx;
  p->evtIndex  = 0;


  // locate the score element in svV[] that is closest to,
  // and possibly after, scEvtIdx.
  for(i=0; i<p->locN; ++i)
  {
    unsigned j;

    for(j=0; j<p->loc[i].evtCnt; ++j)
    {
      p->loc[i].evtV[j].matchFl = false;

      // it is possible that scEvtIdx is before the first event included in p->loc[0]
      // using the p->curLocIdx==cmInvalidIdx forces the first evt in p->loc[0] to be
      // selected in this case
      if( p->loc[i].evtV[j].scEvtIdx <= scEvtIdx || p->curLocIdx==cmInvalidIdx  )
        p->curLocIdx = i;
    }
  }

  if( p->curLocIdx == cmInvalidIdx )
    return cmCtxRtCondition( &p->obj, cmInvalidArgRC, "The initial score search location event %i was not found.", scEvtIdx );

  return cmOkRC;
}

unsigned _cmScTrkIsMatch(cmScTrk* p, int d, unsigned pitch )
{
  if( 0 <= p->curLocIdx + d && p->curLocIdx+1 < p->locN )
  {
    unsigned i;
    const cmScTrkLoc_t* lp = p->loc + p->curLocIdx + d;
    for(i=0; i<lp->evtCnt; ++i)
      if( lp->evtV[i].pitch == pitch && lp->evtV[i].matchFl==false)
        return i;      
  }
  return cmInvalidIdx;
}

void _cmScTrkRpt0( cmScTrk* p,  unsigned pitch, unsigned vel, unsigned nli, unsigned nei )
{
  bool missFl = nli==cmInvalidIdx || nei==cmInvalidIdx;

  printf("------- event:%i %s vel:%i cur:%i new:%i %s-------\n",p->evtIndex,cmMidiToSciPitch(pitch,NULL,0),vel,p->curLocIdx,nli,missFl?"MISS ":"");

  int bi = p->curLocIdx < p->minWndLookAhead ? 0 : p->curLocIdx - p->minWndLookAhead;  
  int ei = cmMin(p->locN-1,p->curLocIdx+p->minWndLookAhead);
  unsigned i,n=0;
  for(i=bi; i<=ei; ++i)
    if( p->loc[i].evtCnt>n )
      n = p->loc[i].evtCnt;
    
  printf("loc  ");
  for(i=bi; i<=ei; ++i)
    printf("%4i   ",i);
  printf("\n");

  for(i=0; i<n; ++i)
  {
    unsigned j;
    printf("sc%2i ",i);
    for(j=bi; j<=ei; ++j)
    {
      if( i < p->loc[j].evtCnt )
      {
        char* X = p->loc[j].evtV[i].matchFl ?  "__" : "  ";

        if( nli==j && nei==i)
        {
          X = "**";
          assert( p->loc[j].evtV[i].pitch == pitch );
        }

        printf("%4s%s ",cmMidiToSciPitch(p->loc[j].evtV[i].pitch,NULL,0),X);
      }
      else
        printf("       ");
    }
    printf("\n");
  }

}


unsigned cmScTrkExec(  cmScTrk* p, unsigned smpIdx, unsigned status, cmMidiByte_t d0, cmMidiByte_t d1 )
{
  unsigned ret_idx = cmInvalidIdx;

  //cmScFolExec(p->sfp, smpIdx, status, d0, d1);

  if( status != kNoteOnMdId )
    return cmInvalidIdx;

  if( p->curLocIdx == cmInvalidIdx )
  {
    cmCtxRtCondition( &p->obj, cmInvalidArgRC, "An initial score search location has not been set." );
    return cmInvalidIdx;
  }
 
  int i,nei,nli=cmInvalidIdx;

  // try to match curLocIdx first
  if((nei = _cmScTrkIsMatch(p,0,d0)) != cmInvalidIdx )
    nli = p->curLocIdx;

  for(i=1; nei==cmInvalidIdx && i<p->minWndLookAhead; ++i)
  {
    // go forward 
    if((nei = _cmScTrkIsMatch(p,i,d0)) != cmInvalidIdx )
      nli = p->curLocIdx + i;
    else
      // go backward
      if((nei = _cmScTrkIsMatch(p,-i,d0)) != cmInvalidIdx )
        nli = p->curLocIdx - i;
  }
  
  if( p->printFl )
  {
    _cmScTrkRpt0(p, d0, d1, nli, nei );
  }


  if( nli != cmInvalidIdx )
  {
    p->loc[nli].evtV[nei].matchFl = true;
    ret_idx = p->loc[nli].scIdx;
    
    if( nli > p->curLocIdx )
      p->curLocIdx = nli;
        
  }

  ++p->evtIndex;

  return ret_idx;
}

//=======================================================================================================================
//----------------------------------------------------------------------------------------
void ed_print_mtx( ed_r* r)
{
  unsigned i,j,k;
  for(i=0; i<r->rn; ++i)
  {
    for(j=0; j<r->cn; ++j)
    {
      printf("(");
      
      const ed_val* vp = r->m + i + (j*r->rn);

      for(k=0; k<kEdCnt; ++k)
      {
        printf("%i",vp->v[k]);
        if( k<kEdCnt-1)
          printf(", ");
        else
          printf(" ");
      }

      printf("%c)",vp->transFl?'t':' ');
      
    }
    printf("\n");
  }
}

void ed_init( ed_r* r, const char* s0, const char* s1 )
{
  unsigned i,j,k;

  r->rn    = strlen(s0)+1;
  r->cn    = strlen(s1)+1;
  r->m     = cmMemAllocZ(ed_val,  r->rn*r->cn );
  r->pn    = r->rn + r->cn;
  r->p_mem = cmMemAllocZ(ed_path, 2*r->pn );
  r->p_avl = r->p_mem;
  r->p_cur = NULL;
  r->p_opt = r->p_mem + r->pn;
  r->s_opt = DBL_MAX;
  r->s0    = s0;
  r->s1    = s1;

  for(i=0; i<r->rn; ++i)
    for(j=0; j<r->cn; ++j)
    {
      unsigned v[] = {0,0,0,0};

      if( i == 0 )
      {
        v[kEdMinIdx] = j;
        v[kEdInsIdx] = j;
      }
      else
        if( j == 0 )
        {
          v[kEdMinIdx] = i;
          v[kEdDelIdx] = i;
        }

      for(k=0; k<kEdCnt; ++k)
        r->m[ i + (j*r->rn) ].v[k] = v[k];      
    }

  // put pn path records on the available list
  for(i=0; i<r->pn; ++i)
    r->p_mem[i].next = i<r->pn-1 ? r->p_mem + i + 1 : NULL;

  
}

unsigned _ed_min( ed_r* r, unsigned i, unsigned j )
{ 
  assert( i<r->rn && j<r->cn );
  return r->m[ i + (j*r->rn) ].v[kEdMinIdx]; 
}

bool _ed_is_trans( ed_r* r, const ed_val* v1p, unsigned i, unsigned j )
{
  bool fl = false;
  ed_val*  v0p = r->m + i + (j*r->rn);   
  if( i>=1 && j>=1 &&   
    v1p->v[kEdMinIdx] == v1p->v[kEdSubIdx] 
    && v1p->matchFl == false 
    && v0p->v[kEdMinIdx] == v0p->v[kEdSubIdx]
    && v0p->matchFl == false )
  {
    char c00 = r->s0[i-1];
    char c01 = r->s0[i  ];
    char c10 = r->s1[j-1];
    char c11 = r->s1[j  ];
    fl = c00==c11 && c01==c10;
  }
  return fl;
}

void ed_calc_mtx( ed_r* r )
{
  unsigned i,j;
  for(i=1; i<r->rn; ++i)
    for(j=1; j<r->cn; ++j)
    {
      ed_val*  vp      = r->m + i + (j*r->rn); 
      vp->matchFl      = r->s0[i-1] == r->s1[j-1];
      unsigned cost    = vp->matchFl ? 0 : 1;
      vp->v[kEdSubIdx] = _ed_min(r,i-1,j-1) + cost;
      vp->v[kEdDelIdx] = _ed_min(r,i-1,j  ) + 1;
      vp->v[kEdInsIdx] = _ed_min(r,i,  j-1) + 1;
      vp->v[kEdMinIdx] = cmMin( vp->v[kEdSubIdx], cmMin(vp->v[kEdDelIdx],vp->v[kEdInsIdx]));
      vp->transFl      = _ed_is_trans(r,vp,i-1,j-1);     
    }
}

void ed_path_push( ed_r* r, unsigned code, unsigned ri, unsigned ci, bool matchFl, bool transFl )
{
  assert(r->p_avl != NULL );
  ed_path* p = r->p_avl;
  r->p_avl = r->p_avl->next;

  p->code    = code;
  p->ri      = ri;
  p->ci      = ci;
  p->matchFl = matchFl;
  p->transFl = transFl;
  p->next    = r->p_cur;  
  r->p_cur   = p;
}

void ed_path_pop( ed_r* r )
{
  assert( r->p_cur != NULL );
  ed_path* tp    = r->p_cur->next;
  r->p_cur->next = r->p_avl;
  r->p_avl       = r->p_cur;
  r->p_cur       = tp;
}

double ed_score_candidate( ed_r* r )
{
  ed_path* cp = r->p_cur;
  ed_path* bp = r->p_cur;
  ed_path* ep = NULL;

  for(; cp!=NULL; cp=cp->next)
    if( cp->code != kEdInsIdx )
    {
      bp = cp;
      break;
    }
  
  for(; cp!=NULL; cp=cp->next)
    if( cp->code!=kEdInsIdx )
      ep = cp;

  assert( ep!=NULL && bp!=ep);
  unsigned n=1;
  for(cp=bp; cp!=ep; cp=cp->next)
    ++n;

  double   gapCnt  = 0;
  double   penalty = 0;
  bool     pfl     = bp->matchFl;
  unsigned i;

  cp = bp;
  for(i=0; i<n; ++i,cp=cp->next)
  {
    // a gap is a transition from a matching subst. to an insert or deletion
    //if( pc != cp->code && cp->code != kEdSubIdx && pc==kEdSubIdx && pfl==true )
    if( pfl==true && cp->matchFl==false )
      ++gapCnt;

    //
    switch( cp->code )
    {
      case kEdSubIdx:
        penalty += cp->matchFl ? 0 : 1;
        penalty -= cp->transFl ? 1 : 0;
        break;

      case kEdDelIdx:
        penalty += 1;
        break;

      case kEdInsIdx:
        penalty += 1;
        break;
    }

    pfl = cp->matchFl;
    
  }

  double score = gapCnt/n + penalty;

  printf("n:%i gaps:%f gap_score:%f penalty:%f score:%f\n",n,gapCnt,gapCnt/n,penalty,score);

  return score;

}

void ed_eval_candidate( ed_r* r, double score )
{
  if( r->s_opt == DBL_MAX || r->s_opt > score)
  {
    // copy the p_cur to p_opt[]
    ed_path* cp = r->p_cur;
    unsigned i;
    for(i=0; cp!=NULL && i<r->pn; cp=cp->next,++i)
    {
      r->p_opt[i].code    = cp->code;
      r->p_opt[i].ri      = cp->ri;
      r->p_opt[i].ci      = cp->ci;
      r->p_opt[i].matchFl = cp->matchFl;
      r->p_opt[i].transFl = cp->transFl;
    }
    
    assert( i < r->pn );
    r->p_opt[i].code = 0; // terminate with code=0        
    r->s_opt = score;
  }
}

void ed_print_opt( ed_r* r )
{
  unsigned i;
  for(i=0; r->p_opt[i].code!=0; ++i)
  {
    ed_path* cp = r->p_opt + i;
    char     c0 = cp->matchFl ? 'm' : ' ';
    char     c1 = cp->transFl ? 't' : ' ';
    printf("%2i code:%i ri:%2i ci:%2i %c%c\n",i,cp->code,cp->ri,cp->ci,c0,c1);
  }
  printf("score:%f\n",r->s_opt);
}

void ed_print_candidate( ed_r* r )
{
  ed_path* cp = r->p_cur;
  unsigned pn = r->pn;
  unsigned i;
  char     s0[pn+1];
  char     s1[pn+1];
  char     s2[pn+1];
  char     s3[pn+1];

  s0[pn] = 0;
  s1[pn] = 0;
  s2[pn] = 0;
  s3[pn] = 0;

  for(i=0; i<pn && cp!=NULL; ++i,cp=cp->next)
  {
    switch(cp->code)
    {
      case kEdSubIdx:               // subst
        assert( 0 <= cp->ri && cp->ri <= r->rn );
        assert( 0 <= cp->ci && cp->ci <= r->cn );
        s0[i] = r->s0[cp->ri];
        s1[i] = r->s1[cp->ci];
        s2[i] = 's';
        s3[i] = cp->matchFl ? 'm' : ' ';
        break;

      case kEdDelIdx:               // delete
        assert( 0 <= cp->ri && cp->ri <= r->rn );
        s0[i] = r->s0[cp->ri];        
        s1[i] = ' ';
        s2[i] = 'd';
        s3[i] = ' ';
        break;

      case kEdInsIdx:               // insert
        assert( 0 <= cp->ci && cp->ci <= r->cn );
        s0[i] = ' ';
        s1[i] = r->s1[cp->ci];
        s2[i] = 'i';
        s3[i] = ' ';
        break;

    }
  }  
  if( i < pn )
  {
    s0[i] = 0;
    s1[i] = 0;
    s2[i] = 0;
    s3[i] = 0;
  }

  printf("\ns0:%s\n",s0);
  printf("s1:%s\n",s1);
  printf("s2:%s\n",s2);
  printf("s3:%s\n",s3);
  
}

// traverse the solution matrix from the lower-right to 
// the upper-left.
void ed_node( ed_r* r, int i, int j )
{
  unsigned m;

  // stop when the upper-right is encountered
  if( i==0 && j==0 )
  {
    ed_print_candidate(r);
    ed_eval_candidate(r, ed_score_candidate(r) );
    return;
  }

  ed_val* vp = r->m + i + (j*r->rn);

  // for each possible dir: up,left,up-left
  for(m=1; m<kEdCnt; ++m)
    if( vp->v[m] == vp->v[kEdMinIdx] )
    {
      unsigned ii = i-1;
      unsigned jj = j-1;

      switch(m)
      {
        case kEdSubIdx:
          break;

        case kEdDelIdx:
          jj = j;
          break;

        case kEdInsIdx:
          ii = i;
          break;
      }

      // prepend to the current candidate path: r->p_cur
      ed_path_push(r,m,ii,jj,vp->matchFl,vp->transFl);

      // recurse!
      ed_node(r,ii,jj);

      // remove the first element from the current path
      ed_path_pop(r);
    }
  
}

void ed_align( ed_r* r )
{
  int      i = r->rn-1;
  int      j = r->cn-1;
  unsigned m = r->m[i + (j*r->rn)].v[kEdMinIdx];

  if( m==cmMax(r->rn,r->cn) )
    printf("Edit distance is at max: %i. No Match.\n",m);
  else
    ed_node(r,i,j);
}


void ed_free( ed_r* r )
{
  cmMemFree(r->m);
  cmMemFree(r->p_mem);
}

void ed_main()
{
  const char* s0 = "YHCQPGK";     
  const char* s1 = "LAHYQQKPGKA"; 

  s0 = "ABCDE";     
  s1 = "ABDCE"; 
  //s1 = "FGHIJK";

  ed_r r;
  
  ed_init(&r,s0,s1);
  ed_calc_mtx(&r);
  ed_print_mtx(&r);
  ed_align(&r);
  ed_print_opt(&r);
  ed_free(&r);
}

//=======================================================================================================================
cmScMatch* cmScMatchAlloc( cmCtx* c, cmScMatch* p, cmScH_t scH, unsigned maxScWndN, unsigned maxMidiWndN )
{
  cmScMatch* op = cmObjAlloc(cmScMatch,c,p);

  if( cmScoreIsValid(scH) )
    if( cmScMatchInit(op,scH,maxScWndN,maxMidiWndN) != cmOkRC )
      cmScMatchFree(&op);
  return op;
}

cmRC_t     cmScMatchFree(  cmScMatch** pp )
{
  cmRC_t rc = cmOkRC;
  if( pp==NULL || *pp==NULL )
    return rc;

  cmScMatch* p = *pp;
  if((rc = cmScMatchFinal(p)) != cmOkRC )
    return rc;
  
  cmMemFree(p->loc);
  cmMemFree(p->m);
  cmMemFree(p->p_mem);
  cmObjFree(pp);
  return rc;
}

void _cmScMatchInitLoc( cmScMatch* p )
{
  unsigned li,ei;

  p->locN = cmScoreEvtCount(p->scH);
  p->loc  = cmMemResizeZ(cmScMatchLoc_t,p->loc,p->locN);


  // for each score location  
  for(li=0,ei=0; li<cmScoreLocCount(p->scH); ++li)
  {
    unsigned i,n;

    const cmScoreLoc_t* lp = cmScoreLoc(p->scH,li);

    // count the number of note events at location li
    for(n=0,i=0; i<lp->evtCnt; ++i)
      if( lp->evtArray[i]->type == kNonEvtScId )
        ++n;

    assert( ei+n <= p->locN );

    // duplicate each note at location li n times
    for(i=0; i<n; ++i)
    {
      unsigned j,k;

      p->loc[ei+i].evtCnt   = n;
      p->loc[ei+i].evtV     = cmMemAllocZ(cmScMatchEvt_t,n);
      p->loc[ei+i].scLocIdx = li;
      p->loc[ei+i].barNumb  = lp->barNumb;

      for(j=0,k=0; j<lp->evtCnt; ++j)
        if( lp->evtArray[j]->type == kNonEvtScId )
        {
          p->loc[ei+i].evtV[k].pitch    = lp->evtArray[j]->pitch;
          p->loc[ei+i].evtV[k].scEvtIdx = lp->evtArray[j]->index;
          ++k;
        }
    }

    ei += n;
  }

  assert(ei<=p->locN);
  p->locN = ei;
}

cmRC_t     cmScMatchInit(  cmScMatch* p, cmScH_t scH, unsigned maxScWndN, unsigned maxMidiWndN )
{
  unsigned i;
  cmRC_t   rc;

  if((rc = cmScMatchFinal(p)) != cmOkRC )
    return rc;

  p->scH  = scH;
  p->mrn  = maxMidiWndN + 1;
  p->mcn  = maxScWndN   + 1;
  p->mmn  = maxMidiWndN;
  p->msn  = maxScWndN;

  _cmScMatchInitLoc(p);

  p->m     = cmMemResizeZ(cmScMatchVal_t,  p->m, p->mrn*p->mcn );
  p->pn    = p->mrn + p->mcn;
  p->p_mem = cmMemResizeZ(cmScMatchPath_t, p->p_mem, 2*p->pn );
  p->p_avl = p->p_mem;
  p->p_cur = NULL;
  p->p_opt = p->p_mem + p->pn;

  // put pn path records on the available list
  for(i=0; i<p->pn; ++i)
  {
    p->p_mem[i].next = i<p->pn-1 ? p->p_mem + i + 1 : NULL;
    p->p_opt[i].next = i<p->pn-1 ? p->p_opt + i + 1 : NULL;
  }

  return rc;
}

cmRC_t     cmScMatchFinal( cmScMatch* p )
{
  unsigned i;
  if( p != NULL )
    for(i=0; i<p->locN; ++i)
      cmMemPtrFree(&p->loc[i].evtV);

  return cmOkRC; 
}

cmRC_t  _cmScMatchInitMtx( cmScMatch* p, unsigned rn, unsigned cn )
{
  if( rn >p->mrn && cn > p->mcn )
    return cmCtxRtCondition( &p->obj, cmInvalidArgRC, "MIDI sequence length must be less than %i. Score sequence length must be less than %i.",p->mmn,p->msn); 

  // if the size of the mtx is not changing then there is nothing to do
  if( rn == p->rn && cn == p->cn )
    return cmOkRC;

  // update the mtx size
  p->rn = rn;
  p->cn = cn;

  // fill in the default values for the first row
  // and column of the DP matrix
  unsigned i,j,k;
  for(i=0; i<rn; ++i)
    for(j=0; j<cn; ++j)
    {
      unsigned v[] = {0,0,0,0};

      if( i == 0 )
      {
        v[kSmMinIdx] = j;
        v[kSmInsIdx] = j;
      }
      else
        if( j == 0 )
        {
          v[kSmMinIdx] = i;
          v[kSmDelIdx] = i;
        }

      for(k=0; k<kSmCnt; ++k)
        p->m[ i + (j*rn) ].v[k] = v[k];      
    }

  return cmOkRC;
}

cmScMatchVal_t* _cmScMatchValPtr( cmScMatch* p, unsigned i, unsigned j, unsigned rn, unsigned cn )
{
  assert( i < rn && j < cn );

  return p->m + i + (j*rn);
}

bool  _cmScMatchIsMatch( const cmScMatchLoc_t* loc, unsigned pitch )
{
  unsigned i;
  for(i=0; i<loc->evtCnt; ++i)
    if( loc->evtV[i].pitch == pitch )
      return true;
  return false;
}

bool _cmScMatchIsTrans( cmScMatch* p, const unsigned* pitchV, const cmScMatchVal_t* v1p, unsigned bsi, unsigned i, unsigned j, unsigned rn, unsigned cn )
{
  bool             fl  = false;
  cmScMatchVal_t*  v0p = _cmScMatchValPtr(p,i,j,rn,cn);

  if( i>=1 && j>=1
    && v1p->v[kSmMinIdx] == v1p->v[kSmSubIdx] 
    && cmIsNotFlag(v1p->flags,kSmMatchFl) 
    && v0p->v[kSmMinIdx] == v0p->v[kSmSubIdx]
    && cmIsNotFlag(v0p->flags,kSmMatchFl) 
      )
  {
    unsigned        c00 = pitchV[i-1];
    unsigned        c01 = pitchV[i  ];
    cmScMatchLoc_t* c10 = p->loc + bsi + j - 1;
    cmScMatchLoc_t* c11 = p->loc + bsi + j;
    fl = _cmScMatchIsMatch(c11,c00) && _cmScMatchIsMatch(c10,c01);
  }
  return fl;
}

unsigned _cmScMatchMin( cmScMatch* p, unsigned i, unsigned j, unsigned rn, unsigned cn )
{ 
  return _cmScMatchValPtr(p,i,j,rn,cn)->v[kSmMinIdx];
}

// Return false if bsi + cn > p->locN
// pitchV[rn-1]
bool  _cmScMatchCalcMtx( cmScMatch* p, unsigned bsi, const unsigned* pitchV, unsigned rn, unsigned cn )
{
  // loc[begScanLocIdx:begScanLocIdx+cn-1] must be valid
  if( bsi + cn > p->locN )
    return false;

  unsigned i,j;

  for(j=1; j<cn; ++j)
    for(i=1; i<rn; ++i)
    {
      cmScMatchLoc_t* loc   = p->loc + bsi + j - 1;
      unsigned        pitch = pitchV[i-1];
      cmScMatchVal_t* vp    = _cmScMatchValPtr(p,i,j,rn,cn);
      vp->flags             = _cmScMatchIsMatch(loc,pitch) ? kSmMatchFl : 0;
      unsigned        cost  =  cmIsFlag(vp->flags,kSmMatchFl) ? 0 : 1;
      vp->v[kSmSubIdx]      = _cmScMatchMin(p,i-1,j-1, rn, cn) + cost;
      vp->v[kSmDelIdx]      = _cmScMatchMin(p,i-1,j  , rn, cn) + 1;
      vp->v[kSmInsIdx]      = _cmScMatchMin(p,i,  j-1, rn, cn) + 1;
      vp->v[kSmMinIdx]      = cmMin( vp->v[kSmSubIdx], cmMin(vp->v[kSmDelIdx],vp->v[kSmInsIdx]));
      vp->flags            |= _cmScMatchIsTrans(p,pitchV,vp,bsi,i-1,j-1,rn,cn) ? kSmTransFl : 0;
    }

  return true;
}

void _cmScMatchPrintMtx( cmScMatch* r, unsigned rn, unsigned cn)
{
  unsigned i,j,k;
  for(i=0; i<rn; ++i)
  {
    for(j=0; j<cn; ++j)
    {
      printf("(");
      
      const cmScMatchVal_t* vp = _cmScMatchValPtr(r,i,j,rn,cn);

      for(k=0; k<kSmCnt; ++k)
      {
        printf("%i",vp->v[k]);
        if( k<kSmCnt-1)
          printf(", ");
        else
          printf(" ");
      }

      printf("%c%c)",cmIsFlag(vp->flags,kSmMatchFl)?'m':' ',cmIsFlag(vp->flags,kSmTransFl)?'t':' ');
      
    }
    printf("\n");
  }
}

void _cmScMatchPathPush( cmScMatch* r, unsigned code, unsigned ri, unsigned ci, unsigned flags )
{
  assert(r->p_avl != NULL );

  cmScMatchPath_t* p = r->p_avl;
  r->p_avl = r->p_avl->next;

  p->code    = code;
  p->ri      = ri;
  p->ci      = ci;
  p->flags   = code==kSmSubIdx && cmIsFlag(flags,kSmMatchFl) ? kSmMatchFl  : 0;
  p->flags  |= cmIsFlag(flags,kSmTransFl) ? kSmTransFl : 0;
  p->next    = r->p_cur;  
  r->p_cur   = p;
}

void _cmScMatchPathPop( cmScMatch* r )
{
  assert( r->p_cur != NULL );
  cmScMatchPath_t* tp    = r->p_cur->next;
  r->p_cur->next = r->p_avl;
  r->p_avl       = r->p_cur;
  r->p_cur       = tp;
}

double _cmScMatchCalcCandidateCost( cmScMatch* r )
{
  cmScMatchPath_t* cp = r->p_cur;
  cmScMatchPath_t* bp = r->p_cur;
  cmScMatchPath_t* ep = NULL;

  // skip leading inserts
  for(; cp!=NULL; cp=cp->next)
    if( cp->code != kSmInsIdx )
    {
      bp = cp;
      break;
    }
  
  // skip to trailing inserts
  for(; cp!=NULL; cp=cp->next)
    if( cp->code!=kSmInsIdx )
      ep = cp;

  // count remaining path length
  assert( ep!=NULL && bp!=ep);
  unsigned n=1;
  for(cp=bp; cp!=ep; cp=cp->next)
    ++n;

  double   gapCnt  = 0;
  double   penalty = 0;
  bool     pfl     = cmIsFlag(bp->flags,kSmMatchFl);
  unsigned i;

  cp = bp;
  for(i=0; i<n; ++i,cp=cp->next)
  {
    // a gap is a transition from a matching subst. to an insert or deletion
    //if( pc != cp->code && cp->code != kSmSubIdx && pc==kSmSubIdx && pfl==true )
    if( pfl==true && cmIsFlag(cp->flags,kSmMatchFl)==false )
      ++gapCnt;

    //
    switch( cp->code )
    {
      case kSmSubIdx:
        penalty += cmIsFlag(cp->flags,kSmMatchFl) ? 0 : 1;
        penalty -= cmIsFlag(cp->flags,kSmTransFl) ? 1 : 0;
        break;

      case kSmDelIdx:
        penalty += 1;
        break;

      case kSmInsIdx:
        penalty += 1;
        break;
    }

    pfl = cmIsFlag(cp->flags,kSmMatchFl);
    
  }

  double cost = gapCnt/n + penalty;

  //printf("n:%i gaps:%f gap_score:%f penalty:%f score:%f\n",n,gapCnt,gapCnt/n,penalty,score);

  return cost;

}

double _cmScMatchEvalCandidate( cmScMatch* r, double min_cost, double cost )
{
  
  if( min_cost == DBL_MAX || cost < min_cost)
  {
    // copy the p_cur to p_opt[]
    cmScMatchPath_t* cp = r->p_cur;
    unsigned         i;

    for(i=0; cp!=NULL && i<r->pn; cp=cp->next,++i)
    {
      r->p_opt[i].code    = cp->code;
      r->p_opt[i].ri      = cp->ri;
      r->p_opt[i].ci      = cp->ci;
      r->p_opt[i].flags   = cp->flags;
      r->p_opt[i].next    = cp->next==NULL ? NULL : r->p_opt + i + 1;
    }
    
    assert( i < r->pn );
    r->p_opt[i].code = 0; // terminate with code=0        
    min_cost         = cost;
  }

  return min_cost;
}


// NOTE: IF THE COST CALCULATION WAS BUILT INTO THE RECURSION THEN 
// THIS FUNCTION COULD BE MADE MORE EFFICIENT BECAUSE PATHS WHICH
// EXCEEDED THE min_cost COULD BE SHORT CIRCUITED.
// 
// traverse the solution matrix from the lower-right to 
// the upper-left.
double _cmScMatchGenPaths( cmScMatch* r, int i, int j, unsigned rn, unsigned cn, double min_cost )
{
  unsigned m;

  // stop when the upper-right is encountered
  if( i==0 && j==0 )
    return _cmScMatchEvalCandidate(r, min_cost, _cmScMatchCalcCandidateCost(r) );

  cmScMatchVal_t* vp = _cmScMatchValPtr(r,i,j,rn,cn);

  // for each possible dir: up,left,up-left
  for(m=1; m<kSmCnt; ++m)
    if( vp->v[m] == vp->v[kSmMinIdx] )
    {
      // prepend to the current candidate path: r->p_cur
      _cmScMatchPathPush(r,m,i,j,vp->flags);

      int ii = i-1;
      int jj = j-1;

      switch(m)
      {
        case kSmSubIdx:
          break;

        case kSmDelIdx:
          jj = j;
          break;

        case kSmInsIdx:
          ii = i;
          break;

        default:
          { assert(0); }
      }

      // recurse!
      min_cost = _cmScMatchGenPaths(r,ii,jj,rn,cn,min_cost);

      // remove the first element from the current path
      _cmScMatchPathPop(r);
    }

  return min_cost;
  
}

double _cmScMatchAlign( cmScMatch* p, unsigned rn, unsigned cn, double min_cost )
{
  int      i = rn-1;
  int      j = cn-1;
  unsigned m = _cmScMatchMin(p,i,j,rn,cn);

  if( m==cmMax(rn,cn) )
    printf("Edit distance is at max: %i. No Match.\n",m);
  else
    min_cost = _cmScMatchGenPaths(p,i,j,rn,cn,min_cost);

  return min_cost;
}


cmRC_t cmScMatchExec( cmScMatch* p, unsigned locIdx, unsigned locN, const unsigned* midiPitchV, unsigned midiPitchN, double min_cost )
{
  cmRC_t   rc;
  unsigned rn = midiPitchN + 1;
  unsigned cn = locN + 1;

  // set the DP matrix default values
  if((rc = _cmScMatchInitMtx(p, rn, cn )) != cmOkRC )
    return rc;

  // _cmScMatchCalcMtx() returns false if the score window exceeds the length of the score
  if(!_cmScMatchCalcMtx(p,locIdx,midiPitchV, rn, cn) )
    return cmEofRC;

  //_cmScMatchPrintMtx(p,rn,cn);
  
  // locate the path through the DP matrix with the lowest edit distance (cost)
  p->opt_cost =  _cmScMatchAlign(p, rn, cn, min_cost);

  return rc;
}

void _cmScMatchPrintPath( cmScMatch* p, cmScMatchPath_t* cp, unsigned bsi, const unsigned* pitchV, const unsigned* mniV )
{
  assert( bsi != cmInvalidIdx );

  cmScMatchPath_t* pp    = cp;
  int              polyN = 0;
  int              i;

  printf("loc: ");

  // get the polyphony count for the score window 
  for(i=0; pp!=NULL; pp=pp->next)
  {
    cmScMatchLoc_t* lp = p->loc + bsi + pp->ci;
    if( pp->code!=kSmDelIdx  )
    {
      if(lp->evtCnt > polyN)
        polyN = lp->evtCnt;

      printf("%4i ",bsi+i);
      ++i;
    }
    else
      printf("%4s "," ");
  }

  printf("\n");

  // print the score notes
  for(i=polyN; i>0; --i)
  {
    printf("%3i: ",i);
    for(pp=cp; pp!=NULL; pp=pp->next)
    {
      int locIdx = bsi + pp->ci - 1;
      assert(0 <= locIdx && locIdx <= p->locN);
      cmScMatchLoc_t* lp = p->loc + locIdx;
      if( pp->code!=kSmDelIdx && lp->evtCnt >= i )
        printf("%4s ",cmMidiToSciPitch(lp->evtV[i-1].pitch,NULL,0));
      else
        printf("%4s ", pp->code==kSmDelIdx? "-" : " ");
    }
    printf("\n");
  }

  printf("mid: ");

  // print the MIDI buffer
  for(pp=cp; pp!=NULL; pp=pp->next)
  {
    if( pp->code!=kSmInsIdx )
      printf("%4s ",cmMidiToSciPitch(pitchV[pp->ri-1],NULL,0));
    else
      printf("%4s ",pp->code==kSmInsIdx?"-":" ");
  }

  printf("\nmni: ");

  // print the MIDI buffer index (mni)
  for(pp=cp; pp!=NULL; pp=pp->next)
  {
    if( pp->code!=kSmInsIdx )
      printf("%4i ",mniV[pp->ri-1]);
    else
      printf("%4s ",pp->code==kSmInsIdx?"-":" ");
  }

  printf("\n op: ");

  // print the substitute/insert/delete operation
  for(pp=cp; pp!=NULL; pp=pp->next)
  {
    char c = ' ';
    switch( pp->code )
    {
      case kSmSubIdx: c = 's'; break;
      case kSmDelIdx: c = 'd'; break;
      case kSmInsIdx: c = 'i'; break;
      default:
        { assert(0); }
    }

    printf("%4c ",c);
  }

  printf("\n     ");

  // give substitute attribute (match or transpose)
  for(pp=cp; pp!=NULL; pp=pp->next)
  {
    cmChar_t s[3];
    int  k = 0;
    if( cmIsFlag(pp->flags,kSmMatchFl) )
      s[k++] = 'm';

    if( cmIsFlag(pp->flags,kSmTransFl) )
      s[k++] = 't';

    s[k]   = 0;

    printf("%4s ",s);
  }

  printf("\nscl: ");

  // print the stored location index
  for(pp=cp; pp!=NULL; pp=pp->next)
  {
    if( pp->locIdx == cmInvalidIdx )
      printf("%4s "," ");
    else
      printf("%4i ",p->loc[pp->locIdx].scLocIdx);
  }
  
  printf("\n\n");
}


//=======================================================================================================================
cmScMatcher* cmScMatcherAlloc( cmCtx* c, cmScMatcher* p, double srate, cmScH_t scH, unsigned scWndN, unsigned midiWndN )
{
  cmScMatcher* op = cmObjAlloc(cmScMatcher,c,p);

  if( op != NULL )
    op->mp = cmScMatchAlloc(c,NULL,cmScNullHandle,0,0);

  if( srate != 0 )
  {
    if( cmScMatcherInit(op,srate,scH,scWndN,midiWndN) != cmOkRC )
      cmScMatcherFree(&op);
  }

  return op;
}

cmRC_t cmScMatcherFree(  cmScMatcher** pp )
{
  cmRC_t rc = cmOkRC;
  if( pp==NULL || *pp==NULL )
    return rc;

  cmScMatcher* p = *pp;
  if((rc = cmScMatcherFinal(p)) != cmOkRC )
    return rc;

  cmScMatchFree(&p->mp);
  cmMemFree(p->midiBuf);
  cmMemFree(p->res);
  cmObjFree(pp);
  return rc;

}

cmRC_t cmScMatcherInit(  cmScMatcher* p, double srate, cmScH_t scH, unsigned scWndN, unsigned midiWndN )
{
  cmRC_t rc;
  if((rc = cmScMatcherFinal(p)) != cmOkRC )
    return rc;

  if( midiWndN > scWndN )
    return cmCtxRtCondition( &p->obj, cmInvalidArgRC, "The score alignment MIDI event buffer length (%i) must be less than the score window length (%i).",midiWndN,scWndN); 

  if(( rc = cmScMatchInit(p->mp,scH,scWndN,midiWndN)) != cmOkRC )
    return rc;

  p->mn         = midiWndN;
  p->midiBuf    = cmMemResize(cmScMatcherMidi_t,p->midiBuf,p->mn);
  p->stepCnt    = 3;
  p->maxMissCnt = p->stepCnt+1;
  p->rn         = 2 * cmScoreEvtCount(scH);
  p->res        = cmMemResizeZ(cmScMatcherResult_t,p->res,p->rn);
  
  cmScMatcherReset(p);

  return rc;
}

cmRC_t cmScMatcherFinal( cmScMatcher* p )
{
  return cmScMatchFinal(p->mp);
}

void cmScMatcherReset( cmScMatcher* p )
{
  p->mbi           = p->mp->mmn;
  p->mni           = 0;
  p->begSyncLocIdx = cmInvalidIdx;
  p->s_opt         = DBL_MAX;
  p->missCnt       = 0;
  p->scanCnt       = 0;
  p->ri            = 0;
}

bool cmScMatcherInputMidi(  cmScMatcher* p, unsigned smpIdx, unsigned status, cmMidiByte_t d0, cmMidiByte_t d1 )
{
  if( status != kNoteOnMdId )
    return false;

  unsigned mi = p->mn-1;

  //printf("%3i %5.2f %4s\n",p->mni,(double)smpIdx/p->srate,cmMidiToSciPitch(d0,NULL,0));

  // shift the new MIDI event onto the end of the MIDI buffer
  memmove(p->midiBuf, p->midiBuf+1, sizeof(cmScMatcherMidi_t)*mi);
  p->midiBuf[mi].locIdx = cmInvalidIdx;
  //p->midiBuf[mi].cbCnt  = 0;
  p->midiBuf[mi].mni    = p->mni++;
  p->midiBuf[mi].smpIdx = smpIdx;
  p->midiBuf[mi].pitch  = d0;
  p->midiBuf[mi].vel    = d1;
  if( p->mbi > 0 )
    --p->mbi;

  return true;
}

void _cmScMatcherStoreResult( cmScMatcher* p, unsigned locIdx, unsigned flags, const cmScMatcherMidi_t* mp )
{
  // don't store missed score note results
  assert( mp != NULL );
  bool                  matchFl = cmIsFlag(flags,kSmMatchFl);
  bool                  tpFl    = locIdx!=cmInvalidIdx && matchFl;
  bool                  fpFl    = locIdx==cmInvalidIdx || matchFl==false;
  cmScMatcherResult_t * rp      = NULL;
  unsigned              i;

  assert( tpFl==false || (tpFl==true && locIdx != cmInvalidIdx ) );

  // it is possible that the same MIDI event is reported more than once
  // (due to step->scan back tracking) - try to find previous result records
  // associated with this MIDI event
  for(i=0; i<p->ri; ++i) 
    if( p->res[i].mni == mp->mni )
    {
      // if this is not the first time this note was reported and it is a true positive 
      if( tpFl )
      {
        rp = p->res + i;
        break;
      }

      // a match was found but this was not a true-pos so ignore it
      return;
    }

  if( rp == NULL )
  {
    rp = p->res + p->ri;
    ++p->ri;
  }

  rp->locIdx = locIdx;
  rp->mni    = mp->mni;
  rp->smpIdx = mp->smpIdx;
  rp->pitch  = mp->pitch;
  rp->vel    = mp->vel;
  rp->flags  = flags | (tpFl ? kSmTruePosFl : 0) | (fpFl ? kSmFalsePosFl : 0);
  
  if( p->cbFunc != NULL )
    p->cbFunc(p,p->cbArg,rp);

}

void cmScMatcherPrintPath( cmScMatcher* p )
{
  unsigned pitchV[ p->mn ];
  unsigned mniV[ p->mn ];
  unsigned i;

  for(i=0; i<p->mn; ++i)
  {
    pitchV[i] = p->midiBuf[i].pitch;
    mniV[i]   = p->midiBuf[i].mni;
  }

  _cmScMatchPrintPath(p->mp, p->mp->p_opt, p->begSyncLocIdx, pitchV, mniV );
}

unsigned   cmScMatcherScan( cmScMatcher* p, unsigned bsi, unsigned scanCnt )
{
  assert( p->mp != NULL && p->mp->mmn > 0 );

  unsigned i_opt = cmInvalidIdx;
  double   s_opt = DBL_MAX;
  cmRC_t  rc     = cmOkRC;
  unsigned i;

  // initialize the internal values set by this function
  p->missCnt = 0;
  p->esi     = cmInvalidIdx;
  p->s_opt   = DBL_MAX;
  
  // if the MIDI buf is not full
  if( p->mbi != 0 )
    return cmInvalidIdx;

  // load a temporary MIDI pitch buffer for use by cmScMatch.
  unsigned pitchV[p->mp->mmn];
  for(i=0; i<p->mp->mmn; ++i)
    pitchV[i] = p->midiBuf[i].pitch;

  // calc the edit distance from pitchV[] to a sliding score window
  for(i=0; rc==cmOkRC && (scanCnt==cmInvalidCnt || i<scanCnt); ++i)
  {      
    rc = cmScMatchExec(p->mp, bsi + i, p->mp->msn, pitchV, p->mp->mmn, s_opt );

    switch(rc)
    {
      case cmOkRC:  // normal result 
        if( p->mp->opt_cost < s_opt )
        {
          s_opt = p->mp->opt_cost;
          i_opt = bsi + i;
        }
        break;

      case cmEofRC: // score window encountered the end of the score
        break;

      default: // error state
        return cmInvalidIdx;
    }

  }
          
  // store the cost assoc'd with i_opt
  p->s_opt = s_opt;

  if( i_opt == cmInvalidIdx )
    return cmInvalidIdx;

  // Traverse the least cost path and:
  // 1) Set p->esi to the score location index of the last MIDI note
  // which has a positive match with the score and assign
  // the internal score index to cp->locIdx.
  //
  // 2) Set cmScAlignPath_t.locIdx - index into p->loc[] associated
  // with each path element that is a 'substitute' or an 'insert'.
  //
  // 3) Set p->missCnt: the count of trailing non-positive matches.
  // p->missCnt is eventually used in cmScAlignStep() to track the number
  // of consecutive trailing missed notes.
  //
  cmScMatchPath_t* cp = p->mp->p_opt;

  for(i=0; cp!=NULL; cp=cp->next)
  {
    if( cp->code != kSmInsIdx )
    {
      assert( cp->ri > 0 );
      p->midiBuf[ cp->ri-1 ].locIdx = cmInvalidIdx;
    }

    switch( cp->code )
    {
      case kSmSubIdx:
        if( cmIsFlag(cp->flags,kSmMatchFl) || cmIsFlag(cp->flags,kSmTransFl))
        {
          p->esi     = i_opt + i;
          p->missCnt = 0;

          if( cmIsFlag(cp->flags,kSmMatchFl) )
            p->midiBuf[ cp->ri-1 ].locIdx = i_opt + i;
        }
        else
        {
          ++p->missCnt;
        }
        // fall through

      case kSmInsIdx:
        cp->locIdx = i_opt + i;
        ++i;
        break;

      case kSmDelIdx:
        cp->locIdx = cmInvalidIdx;
        ++p->missCnt;
        break;

    }
  }

  // if no positive matches were found
  if( p->esi == cmInvalidIdx )
    i_opt = cmInvalidIdx;
  else
  {
    // record result
    for(cp=p->mp->p_opt; cp!=NULL; cp=cp->next)
      if( cp->code != kSmInsIdx )
        _cmScMatcherStoreResult(p, cp->locIdx, cp->flags, p->midiBuf + cp->ri - 1);
  }

  return i_opt;

}

cmRC_t     cmScMatcherStep(  cmScMatcher* p )
{
  int      i;
  unsigned pitch  = p->midiBuf[ p->mn-1 ].pitch;
  unsigned locIdx = cmInvalidIdx;

  // the tracker must be sync'd to step
  if( p->esi == cmInvalidIdx )
    return cmCtxRtCondition( &p->obj, cmInvalidArgRC, "The p->esi value must be valid to perform a step operation."); 

  // if the end of the score has been reached
  if( p->esi + 1 >= p->mp->locN )
    return cmEofRC;
    
  // attempt to match to next location first
  if( _cmScMatchIsMatch(p->mp->loc + p->esi + 1, pitch) )
  {
    locIdx = p->esi + 1;
  }
  else
  {
    // 
    for(i=2; i<p->stepCnt; ++i)
    {
      // go forward 
      if( p->esi+i < p->mp->locN && _cmScMatchIsMatch(p->mp->loc + p->esi + i, pitch) )
      {
        locIdx = p->esi + i;
        break;
      }

      // go backward
      if( p->esi >= (i-1)  && _cmScMatchIsMatch(p->mp->loc + p->esi - (i-1), pitch) )
      {
        locIdx = p->esi - (i-1);
        break;
      }
    }
  }

  p->midiBuf[ p->mn-1 ].locIdx = locIdx;

  if( locIdx == cmInvalidIdx )
    ++p->missCnt;
  else
  {
    p->missCnt = 0;
    p->esi     = locIdx;
  }

  // store the result
  _cmScMatcherStoreResult(p, locIdx,  locIdx!=cmInvalidIdx ? kSmMatchFl : 0, p->midiBuf + p->mn - 1);

  if( p->missCnt >= p->maxMissCnt )
  {
    unsigned begScanLocIdx = p->esi > p->mn ? p->esi - p->mn : 0;
    p->s_opt               = DBL_MAX;
    unsigned bsi           = cmScMatcherScan(p,begScanLocIdx,p->mn*2);
    ++p->scanCnt;

    // if the scan failed find a match
    if( bsi == cmInvalidIdx )
      return cmCtxRtCondition( &p->obj, cmSubSysFailRC, "Scan resync. failed."); 

  }

  return cmOkRC;
}



cmRC_t     cmScMatcherExec(  cmScMatcher* p, unsigned smpIdx, unsigned status, cmMidiByte_t d0, cmMidiByte_t d1 )
{
  bool   fl = p->mbi > 0;
  cmRC_t rc = cmOkRC;

  // update the MIDI buffer with the incoming note
  cmScMatcherInputMidi(p,smpIdx,status,d0,d1);

  // if the MIDI buffer transitioned to full then perform an initial scan sync.
  if( fl && p->mbi == 0 )
  {
    if( (p->begSyncLocIdx = cmScMatcherScan(p,0,cmInvalidCnt)) == cmInvalidIdx )
      rc = cmInvalidArgRC; // signal init. scan sync. fail
    else
    {
      // cmScMatcherPrintPath(p);
    }
  }
  else
  {
    // if the MIDI buffer is full then perform a step sync.
    if( !fl && p->mbi == 0 ) 
      rc = cmScMatcherStep(p);
  }

  return rc;

}



double cmScMatcherFMeas( cmScMatcher* p )
{
  unsigned bli       = p->mp->locN;
  unsigned eli       = 0;
  unsigned scNoteCnt = 0;       // total count of score notes
  unsigned matchCnt  = 0;       // count of matched notes       (true positives)
  unsigned wrongCnt  = 0;       // count of incorrect notes     (false positives)
  unsigned missCnt   = 0;       // count of missed score notes  (false negatives)
  unsigned i;

  for(i=0; i<p->ri; ++i)
    if( p->res[i].locIdx != cmInvalidIdx )
    {
      bli = cmMin(bli,p->res[i].locIdx);
      eli = cmMax(eli,p->res[i].locIdx);
      
      if( cmIsFlag(p->res[i].flags,kSmTruePosFl) )
        ++matchCnt;

      if( cmIsFlag(p->res[i].flags,kSmFalsePosFl) )
        ++wrongCnt;
    }

  scNoteCnt = eli - bli + 1;
  missCnt   = scNoteCnt - matchCnt;

  double prec = (double)2.0 * matchCnt / (matchCnt + wrongCnt);
  double rcal = (double)2.0 * matchCnt / (matchCnt + missCnt);
  double fmeas = prec * rcal / (prec + rcal);

  //printf("total:%i match:%i wrong:%i miss:%i\n",scNoteCnt,matchCnt,wrongCnt,missCnt);

  return fmeas;
}

typedef struct cmScMatcherPrint_str
{
  unsigned                     flags;
  unsigned                     scLocIdx;
  unsigned                     mni; 
  unsigned                     pitch;
  unsigned                     vel; 
} cmScMatcherPrint_t;

void  _cmScMatcherInsertPrint(cmScMatcherPrint_t* a, unsigned i, unsigned* anp, unsigned aan, const cmScMatcherResult_t* rp, unsigned scLocIdx )
{
  assert( *anp + 1 <= aan );

  memmove(a + i + 1, a + i, (*anp-i)*sizeof(cmScMatcherPrint_t));
  memset( a + i, 0, sizeof(cmScMatcherPrint_t));
  *anp += 1;

  a[i].flags    = rp->flags;
  a[i].scLocIdx = scLocIdx;
  a[i].mni      = rp->mni;
  a[i].pitch    = rp->pitch;
  a[i].vel      = rp->vel;
}

void cmScMatcherPrint( cmScMatcher* p )
{
  unsigned bsli       = cmScoreEvtCount(p->mp->scH);
  unsigned esli       = 0;
  unsigned i,j,k;

  // get first/last scLocIdx from res[]
  for(i=0; i<p->ri; ++i)
    if( p->res[i].locIdx != cmInvalidIdx )
    {
      bsli = cmMin(bsli,p->mp->loc[p->res[i].locIdx].scLocIdx);
      esli = cmMax(esli,p->mp->loc[p->res[i].locIdx].scLocIdx);
    }

  unsigned an  = 0;
  unsigned aan = p->ri;

  // calc the count of score events between bsli and esli.
  for(i=bsli; i<=esli; ++i)
  {
    cmScoreLoc_t* lp = cmScoreLoc(p->mp->scH, i);
    assert(lp != NULL);
    aan += lp->evtCnt;
  }

  // allocate an array off 'aan' print records
  cmScMatcherPrint_t* a  = cmMemAllocZ(cmScMatcherPrint_t,aan);

  // fill a[] with score note and bar events
  for(i=bsli; i<=esli; ++i)
  {
    unsigned      scLocIdx = i;
    cmScoreLoc_t* lp       = cmScoreLoc(p->mp->scH, scLocIdx );

    for(j=0; j<lp->evtCnt; ++j)
    {
      assert( an < aan );

      cmScoreEvt_t*       ep = lp->evtArray[j];
      cmScMatcherPrint_t* pp = a + an;

      an += 1;
      
      switch( ep->type )
      {
        case kBarEvtScId:
          pp->flags = kSmBarFl;
          break;

        case kNonEvtScId:
          pp->flags = kSmNoteFl;
          break;
      }

      pp->scLocIdx = scLocIdx;
      pp->mni      = cmInvalidIdx;
      pp->pitch    = ep->pitch;
      pp->vel      = kInvalidMidiVelocity;
    }

  }

  // for each result record
  for(i=0; i<p->ri; ++i)
  {
    cmScMatcherResult_t* rp = p->res + i;
    
    // if this result recd matched a score event
    if( cmIsFlag(rp->flags,kSmTruePosFl) )
    {
      // locate the matching score event
      for(k=0; k<an; ++k)
        if( a[k].scLocIdx==p->mp->loc[rp->locIdx].scLocIdx && a[k].pitch==rp->pitch )
        {
          a[k].mni    = rp->mni;
          a[k].vel    = rp->vel;
          a[k].flags |= kSmMatchFl;
          break;
        }      
    }

    // if this result did not match a score event
    if( cmIsFlag(rp->flags,kSmFalsePosFl) )
    {
      unsigned            d_min;
      cmScMatcherPrint_t* dp = NULL;
      unsigned            scLocIdx = cmInvalidIdx;

      // if this result does not have a valid locIdx 
      // (e.g. errant MIDI notes: scan:'delete' note or a step:mis-match note)
      if( rp->locIdx == cmInvalidIdx )
      {

        // find the print recd with the closet 'mni'
        for(k=0; k<an; ++k)
          if( a[k].mni != cmInvalidIdx )
          {
            unsigned d;
            if( a[k].mni > rp->mni )
              d = a[k].mni - rp->mni;
            else
              d = rp->mni - a[k].mni;
          
            if(  dp==NULL || d < d_min )
            {
              dp    = a + k;
              d_min = d;
            }
          }   

        k = dp - a;
        assert( k < an );

        scLocIdx = p->mp->loc[k].scLocIdx;

        if( a[k].mni < rp->mni )
          ++k;

      }
      else // result w/ a valid locIdx (e.g. scan 'substitute' with no match)
      {
        scLocIdx = p->mp->loc[rp->locIdx].scLocIdx;

        // find the print recd with the closest scIdx
        for(k=0; k<an; ++k)
          if( a[k].scLocIdx != cmInvalidIdx )
          {
            unsigned d;
            if( a[k].scLocIdx > scLocIdx )
              d = a[k].scLocIdx - scLocIdx;
            else
              d = scLocIdx - a[k].scLocIdx;
          
            if(  dp==NULL || d < d_min )
            {
              dp    = a + k;
              d_min = d;
            }
          }   

        k = dp - a;
        assert( k < an );

        if( a[k].scLocIdx < scLocIdx )
          ++k;

      }

      // create a new print recd to represent the false-positive result recd
      assert( dp != NULL );
      _cmScMatcherInsertPrint(a, k, &an,aan,rp,scLocIdx);
    }
  }
  
  for(i=0; i<an; ++i)
  {
    printf("%4i %4i %4s %c%c%c\n",a[i].scLocIdx,a[i].mni,
      cmIsFlag(a[i].flags,kSmBarFl)   ? "|" : cmMidiToSciPitch(a[i].pitch,NULL,0),
      cmIsFlag(a[i].flags,kSmNoteFl)  ? 'n' : ' ',
      cmIsFlag(a[i].flags,kSmMatchFl) ? 'm' : (cmIsFlag(a[i].flags,kSmTransFl) ? 't' : ' '),
      cmIsFlag(a[i].flags,kSmFalsePosFl) ? '*' : ' '
           );
  }
  
}


//=======================================================================================================================

cmScMeas* cmScMeasAlloc( cmCtx* c, cmScMeas* p, cmScH_t scH )
{
  cmScMeas* op = cmObjAlloc(cmScMeas,c,p);

  op->mp = cmScMatchAlloc( c, NULL, cmScNullHandle, 0, 0 );

  if( cmScoreIsValid(scH) )
    if( cmScMeasInit(op,scH) != cmOkRC )
      cmScMeasFree(&op);

  return op;
}

cmRC_t    cmScMeasFree(  cmScMeas** pp )
{
  cmRC_t rc = cmOkRC;
  if( pp==NULL || *pp==NULL )
    return rc;

  cmScMeas* p = *pp;
  if((rc = cmScMeasFinal(p)) != cmOkRC )
    return rc;

  cmScMatchFree(&p->mp);
  
  cmMemFree(p->midiBuf);
  cmMemFree(p->set);
  cmObjFree(pp);
  return rc;
}

void _cmScMeasPrint( cmScMeas* p )
{
  unsigned i;
  for(i=0; i<p->sn; ++i)
  {
    cmScMeasSet_t* sp = p->set + i;
    printf("%4i: sli:%4i %4i li:%4i %4i\n", i, sp->bsli, sp->esli, sp->bli, sp->eli );
  }
     
}

cmRC_t    cmScMeasInit(  cmScMeas* p, cmScH_t scH )
{
  cmRC_t   rc;
  unsigned i,j;
  unsigned si;
  unsigned maxScWndN = 0;

  if((rc = cmScMeasFinal(p)) != cmOkRC )
    return rc;

  p->mi      = 0;
  p->mn      = 2 * cmScoreEvtCount(scH);
  p->midiBuf = cmMemResizeZ(cmScMeasMidi_t,p->midiBuf,p->mn);
  p->sn      = cmScoreSetCount(scH);
  p->set     = cmMemResize(cmScMeasSet_t,p->set,p->sn);

  unsigned n = cmScoreLocCount(scH);

  // for each score location
  for(i=0,si=0; i<n; ++i)
  {
    cmScoreLoc_t* lp = cmScoreLoc(scH,i);
    cmScoreSet_t* sp = lp->setList;

    // for each set that ends on this score location
    for(; sp!=NULL; sp=sp->llink,++si)
    {
      assert(si < p->sn);

      cmScMeasSet_t* msp = p->set + si;

      msp->bsli = cmScoreLocCount(scH);
      msp->esli = 0;
      msp->bsei = cmScoreEvtCount(scH);
      msp->esei = 0;
      msp->bli  = cmInvalidIdx;
      msp->eli  = cmInvalidIdx;

      for(j=0; j<sp->eleCnt; ++j)
      {
        msp->bsli = cmMin(msp->bsli,sp->eleArray[j]->locIdx);
        msp->esli = cmMax(msp->esli,sp->eleArray[j]->locIdx);
    
        msp->bsei = cmMin(msp->bsei,sp->eleArray[j]->index);
        msp->esei = cmMax(msp->esei,sp->eleArray[j]->index);
      }
    } 
  }

  // initialize p->mp so that mp->loc[] is loaded - use dummy scWndN and midiN 
  if((rc = cmScMatchInit(p->mp, scH, 11, 10 )) != cmOkRC )
    return rc;

  // assign set[].bli and set[].eli
  for(j=0; j<p->sn; ++j)
  {
    cmScMeasSet_t* msp = p->set + j;

    for(i=0; i<p->mp->locN; ++i)
    {
      if( msp->bli==cmInvalidIdx && msp->bsli==p->mp->loc[i].scLocIdx )
        msp->bli = i;

      if( msp->esli==p->mp->loc[i].scLocIdx )
        msp->eli = i;
    }  

    assert( msp->eli > msp->bli );

    maxScWndN = cmMax( maxScWndN, msp->eli - msp->bli + 1 );

  }

  // setup the match
  if((rc = cmScMatchInit(p->mp, scH, 2*maxScWndN+1, 2*maxScWndN )) != cmOkRC )
    return rc;

  //_cmScMeasPrint(p);

  return rc;
}

cmRC_t    cmScMeasFinal( cmScMeas* p )
{ return cmScMatchFinal(p->mp); }

cmRC_t    cmScMeasReset( cmScMeas* p )
{
  cmRC_t rc = cmOkRC;
  p->nsi    = 0;
  p->nsli   = 0;
  return rc;
}

void _cmScMeasMatch( cmScMeas* p, cmScMeasSet_t* sp )
{
  double min_cost = DBL_MAX;

  if( p->mi == 0 )
    return;

  unsigned pn = 0;
  int i       = p->mi-1;

  // determine the count of MIDI notes to match to the set score
  for(; i>=0; --i)
  {
    ++pn;

    if( p->midiBuf[i].locIdx != cmInvalidIdx )
    {
      unsigned scLocIdx = p->mp->loc[ p->midiBuf[i].locIdx ].scLocIdx;
      if( scLocIdx <= sp->bsli )
        break;
    }
  }

  assert(pn>0);
  
  // fill the pitch vector
  unsigned pitchV[ pn ];
  unsigned mniV[   pn ];
  unsigned j;
  for(j=0; i<p->mn && j<pn; ++i,++j)
  {
    pitchV[j] = p->midiBuf[i].pitch;
    mniV[j]   = p->midiBuf[i].mni;
  }
    
  cmScMatchExec(p->mp,sp->bli, sp->eli-sp->bli+1, pitchV, pn, min_cost );
 
  _cmScMatchPrintPath(p->mp, p->mp->p_opt, sp->bli, pitchV, mniV );

}

cmRC_t cmScMeasExec( cmScMeas* p, unsigned mni, unsigned locIdx, unsigned smpIdx, unsigned pitch, unsigned vel )
{
  cmRC_t rc = cmOkRC;

  // if the midi buffer is full
  if( p->mi >= p->mn )
    return cmCtxRtCondition( &p->obj, cmEofRC, "The MIDI buffer is full."); 

  // store the MIDI event
  p->midiBuf[p->mi].mni      = mni;
  p->midiBuf[p->mi].locIdx   = locIdx;
  p->midiBuf[p->mi].smpIdx   = smpIdx;
  p->midiBuf[p->mi].pitch    = pitch;
  p->midiBuf[p->mi].vel      = vel;

  ++p->mi;

  if( locIdx == cmInvalidIdx )
    return cmOkRC;

  // 
  unsigned scLocIdx    = p->mp->loc[ locIdx ].scLocIdx;
  unsigned maxScLocIdx = cmScoreLocCount(p->mp->scH)-1;

  // 
  for(; p->nsli<=scLocIdx && p->nsi < p->sn; ++p->nsli)
  {
    while( cmMin(maxScLocIdx,p->set[p->nsi].esli+1) == p->nsli )
    {
      _cmScMeasMatch(p, p->set + p->nsi );

      // advance the set index
      ++p->nsi;
    }
  }

  return rc;
}

//=======================================================================================================================
cmRC_t cmScAlignScanToTimeLineEvent( cmScMatcher* p, cmTlH_t tlH, cmTlObj_t* top, unsigned endSmpIdx )
{
  assert( top != NULL );
  cmTlMidiEvt_t* mep = NULL;
  cmRC_t         rc  = cmOkRC;
  
  // as long as more MIDI events are available get the next MIDI msg 
  while( rc==cmOkRC && (mep = cmTlNextMidiEvtObjPtr(tlH, top, top->seqId )) != NULL )
  {
    top = &mep->obj;

    // if the msg falls after the end of the marker then we are through
    if( mep->obj.seqSmpIdx != cmInvalidIdx && mep->obj.seqSmpIdx > endSmpIdx )
      break;

    // if the time line MIDI msg a note-on
    if( mep->msg->status == kNoteOnMdId )
    {
      rc = cmScMatcherExec(p, mep->obj.seqSmpIdx, mep->msg->status, mep->msg->u.chMsgPtr->d0, mep->msg->u.chMsgPtr->d1 );

      switch( rc )
      {
        case cmOkRC:        // continue processing MIDI events
          break;

        case cmEofRC:       // end of the score was encountered
          break;

        case cmInvalidArgRC: // p->esi was not set correctly
          break;

        case cmSubSysFailRC: // scan resync failed
          break;
      }
    }
  }

  if( rc == cmEofRC )
    rc = cmOkRC;

  return rc;
}

void cmScMatcherCb( cmScMatcher* p, void* arg, cmScMatcherResult_t* rp )
{
  //cmScMeas* mp = (cmScMeas*)arg;
  //cmScMeasExec(mp, rp->mni, rp->locIdx, rp->smpIdx, rp->pitch, rp->vel );
}

void       cmScAlignScanMarkers(  cmRpt_t* rpt, cmTlH_t tlH, cmScH_t scH )
{
  unsigned     i;
  double       srate        = 96000;
  unsigned     midiN        = 7; 
  unsigned     scWndN       = 10;
  unsigned     markN        = 291;
  cmCtx*       ctx          = cmCtxAlloc(NULL, rpt, cmLHeapNullHandle, cmSymTblNullHandle );
  cmScMeas*    mp           = cmScMeasAlloc(ctx,NULL,scH);
  cmScMatcher* p            = cmScMatcherAlloc(ctx,NULL,srate,scH,scWndN,midiN);  
  double       scoreThresh  = 0.5;
  unsigned     candCnt      = 0;
  unsigned     initFailCnt  = 0;
  unsigned     otherFailCnt = 0;
  unsigned     scoreFailCnt = 0;
  bool         printFl      = false;
  unsigned     markCharCnt  = 31;
  cmChar_t     markText[ markCharCnt+1 ];
  cmTimeSpec_t t0,t1;

  cmTimeGet(&t0);

  p->cbArg  = mp; // set the callback arg. 
  p->cbFunc = cmScMatcherCb; 
  
  // for each marker
  for(i=0; i<markN; ++i)
  {
    // form the marker text
    snprintf(markText,markCharCnt,"Mark %i",i);

    // locate the marker
    cmTlMarker_t*  mp    = cmTimeLineMarkerFind( tlH, markText );
    if( mp == NULL )
    {
      if( printFl )
        printf("The marker '%s' was not found.\n\n",markText);  
      continue;
    }

    // skip markers which do not contain text
    if( cmTextIsEmpty(mp->text) )
    {
      if( printFl )
        printf("The marker '%s' is being skipped because it has no text.\n\n",markText);  
      continue;
    }

    // reset the score follower to the beginnig of the score
    cmScMatcherReset(p);

    ++candCnt;

    // scan to the beginning of the marker
    cmRC_t rc = cmScAlignScanToTimeLineEvent(p,tlH,&mp->obj,mp->obj.seqSmpIdx+mp->obj.durSmpCnt);
    bool   pfl = true;

    if( rc != cmOkRC || p->begSyncLocIdx==cmInvalidIdx)
    {
      if( p->begSyncLocIdx == cmInvalidIdx )
        rc = cmInvalidArgRC;

      if( p->mni == 0 )
      {
        if( printFl )
          printf("mark:%i midi:%i Not enough MIDI notes to fill the scan buffer.\n",i,p->mni);
        pfl = false;
      }
      else
      {
        switch(rc)
        {
          case cmInvalidArgRC:
            if( printFl )
              printf("mark:%i INITIAL SYNC FAIL\n",i);
            ++initFailCnt;
            pfl = false;
            break;

          case cmSubSysFailRC:
            if( printFl )
              printf("mark:%i SCAN RESYNC FAIL\n",i);
            ++otherFailCnt;
            break;

          default:
            if( printFl )
              printf("mark:%i UNKNOWN FAIL\n",i);
            ++otherFailCnt;
        }
      }
    }

    if( pfl )
    {      
      double fmeas = cmScMatcherFMeas(p);

      if( printFl )
        printf("mark:%i midi:%i loc:%i bar:%i cost:%f f-meas:%f text:%s\n",i,p->mni,p->begSyncLocIdx,p->mp->loc[p->begSyncLocIdx].barNumb,p->s_opt,fmeas,mp->text);

      if( fmeas < scoreThresh )
        ++scoreFailCnt;

    }

    //print score and match for entire marker
    //cmScMatcherPrint(p);

     // ONLY USE ONE MARKER DURING TESTING
    //break; 

    if( printFl )
      printf("\n");

  }

  printf("cand:%i fail:%i - init:%i score:%i other:%i\n\n",candCnt,initFailCnt+scoreFailCnt+otherFailCnt,initFailCnt,scoreFailCnt,otherFailCnt);

  cmTimeGet(&t1);
  printf("elapsed:%f\n", (double)cmTimeElapsedMicros(&t0,&t1)/1000000.0 );

  cmScMatcherFree(&p);  
  cmScMeasFree(&mp);
  cmCtxFree(&ctx);
}
