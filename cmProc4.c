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

cmScAlign* cmScAlignAlloc( cmCtx* c, cmScAlign* p, cmScAlignCb_t cbFunc, void* cbArg, cmReal_t srate, cmScH_t scH, unsigned midiN, unsigned scWndN )
{
  cmScAlign* op = cmObjAlloc(cmScAlign,c,p);

  if( srate != 0 )
    if( cmScAlignInit(op,cbFunc,cbArg,srate,scH,midiN,scWndN) != cmOkRC )
      cmScAlignFree(&op);
  return op;
}

cmRC_t     cmScAlignFree( cmScAlign** pp )
{
  cmRC_t rc = cmOkRC;
  if( pp==NULL || *pp==NULL )
    return rc;

  cmScAlign* p = *pp;
  if((rc = cmScAlignFinal(p)) != cmOkRC )
    return rc;
  
  cmMemFree(p->loc);
  cmMemFree(p->midiBuf);
  cmMemFree(p->m);
  cmMemFree(p->p_mem);
  cmMemFree(p->res);
  cmObjFree(pp);
  return rc;
}

void _cmScAlignPrint( cmScAlign* p )
{
  int i,j;
  for(i=0; i<p->locN; ++i)
  {
    printf("%2i %5i ",p->loc[i].barNumb,p->loc[i].scLocIdx);
    for(j=0; j<p->loc[i].evtCnt; ++j)
      printf("%s ",cmMidiToSciPitch(p->loc[i].evtV[j].pitch,NULL,0));
    printf("\n");
  }
}


cmRC_t     cmScAlignInit( cmScAlign* p, cmScAlignCb_t cbFunc, void* cbArg, cmReal_t srate, cmScH_t scH, unsigned midiN, unsigned scWndN )
{
  cmRC_t rc;
  if((rc = cmScAlignFinal(p)) != cmOkRC )
    return rc;

  if( midiN > scWndN )
    return cmCtxRtCondition( &p->obj, cmInvalidArgRC, "The score alignment MIDI event buffer length (%i) must be less than the score window length (%i).",midiN,scWndN); 

  p->cbFunc         = cbFunc;
  p->cbArg          = cbArg;
  p->srate          = srate;
  p->scH            = scH;
  p->locN           = cmScoreEvtCount(scH);
  p->loc            = cmMemResizeZ(cmScAlignLoc_t,p->loc,p->locN);
  p->mn             = midiN;
  p->midiBuf        = cmMemResizeZ(cmScAlignMidiEvt_t,p->midiBuf,midiN);
  p->mbi            = midiN;
  p->printFl        = true;
  
  // Setup score structures

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

      p->loc[ei+i].evtCnt   = n;
      p->loc[ei+i].evtV     = cmMemAllocZ(cmScAlignScEvt_t,n);
      p->loc[ei+i].scLocIdx = li;
      p->loc[ei+i].barNumb  = lp->barNumb;

      for(j=0,k=0; j<lp->evtCnt; ++j)
        if( lp->evtArray[j]->type == kNonEvtScId )
        {
          p->loc[ei+i].evtV[k].pitch    = lp->evtArray[j]->pitch;
          ++k;
        }

    }

    ei += n;
    
  }

  assert(ei<=p->locN);
  p->locN = ei;

  // setup edit distance structures
  p->rn    = midiN+1;
  p->cn    = scWndN+1;
  p->m     = cmMemResizeZ(cmScAlignVal_t,  p->m, p->rn*p->cn );
  p->pn    = p->rn + p->cn;
  p->p_mem = cmMemResizeZ(cmScAlignPath_t, p->p_mem, 2*p->pn );
  p->p_avl = p->p_mem;
  p->p_cur = NULL;
  p->p_opt = p->p_mem + p->pn;
  p->s_opt = DBL_MAX;
  p->resN  = 2 * cmScoreEvtCount(scH);  // make twice as many result records as there are score events
  p->res   = cmMemResizeZ(cmScAlignResult_t, p->res, p->resN);
  p->stepCnt = 3;
  p->maxStepMissCnt = 4;
  
  // fill in the default values for the first row
  // and column of the DP matrix
  unsigned i,j,k;
  for(i=0; i<p->rn; ++i)
    for(j=0; j<p->cn; ++j)
    {
      unsigned v[] = {0,0,0,0};

      if( i == 0 )
      {
        v[kSaMinIdx] = j;
        v[kSaInsIdx] = j;
      }
      else
        if( j == 0 )
        {
          v[kSaMinIdx] = i;
          v[kSaDelIdx] = i;
        }

      for(k=0; k<kSaCnt; ++k)
        p->m[ i + (j*p->rn) ].v[k] = v[k];      
    }

  // put pn path records on the available list
  for(i=0; i<p->pn; ++i)
  {
    p->p_mem[i].next = i<p->pn-1 ? p->p_mem + i + 1 : NULL;
    p->p_opt[i].next = i<p->pn-1 ? p->p_opt + i + 1 : NULL;
  }

  //_cmScAlignPrint(p);

  cmScAlignReset(p,0);

  return rc;
}

cmRC_t cmScAlignFinal( cmScAlign* p )
{
  unsigned i;
  for(i=0; i<p->locN; ++i)
    cmMemPtrFree(&p->loc[i].evtV);

  return cmOkRC; 
}

void cmScAlignReset( cmScAlign* p, unsigned begScanLocIdx )
{
  assert( begScanLocIdx < p->locN );
  p->mbi         = p->mn;
  p->mni         = 0;
  p->begScanLocIdx = begScanLocIdx;
  p->begSyncLocIdx = cmInvalidIdx;
  p->s_opt       = DBL_MAX;
  p->esi         = cmInvalidIdx;
  p->missCnt     = 0;
  p->scanCnt     = 0;
  p->ri          = 0;
}

cmScAlignVal_t* _cmScAlignValPtr( cmScAlign* p, unsigned i, unsigned j )
{
  assert( i < p->rn );
  assert( j < p->cn );

  return p->m + i + (j*p->rn);
}

bool  _cmScAlignIsMatch( const cmScAlignLoc_t* loc, unsigned pitch )
{
  unsigned i;
  for(i=0; i<loc->evtCnt; ++i)
    if( loc->evtV[i].pitch == pitch )
      return true;
  return false;
}

bool _cmScAlignIsTrans( cmScAlign* p, const cmScAlignVal_t* v1p, unsigned i, unsigned j )
{
  bool             fl  = false;
  cmScAlignVal_t*  v0p = _cmScAlignValPtr(p,i,j);

  if( i>=1 && j>=1
    && v1p->v[kSaMinIdx] == v1p->v[kSaSubIdx] 
    && v1p->matchFl      == false 
    && v0p->v[kSaMinIdx] == v0p->v[kSaSubIdx]
    && v0p->matchFl      == false )
  {
    unsigned        c00 = p->midiBuf[i-1].pitch;
    unsigned        c01 = p->midiBuf[i  ].pitch;
    cmScAlignLoc_t* c10 = p->loc + p->begScanLocIdx + j - 1;
    cmScAlignLoc_t* c11 = p->loc + p->begScanLocIdx + j;
    fl = _cmScAlignIsMatch(c11,c00) && _cmScAlignIsMatch(c10,c01);
  }
  return fl;
}

unsigned _cmScAlignMin( cmScAlign* p, unsigned i, unsigned j )
{ 
  assert( i<p->rn && j<p->cn );
  //return p->m[ i + (j*p->rn) ].v[kSaMinIdx]; 
  return _cmScAlignValPtr(p,i,j)->v[kSaMinIdx];
}

// Returns 'false' if the score window goes past the end of the score
// (i.e. p->begScanLocIdx + p->cn > p->locN )
bool _cmScAlignCalcMtx( cmScAlign* p )
{
  // the midi buffer must be full
  assert( p->mbi == 0 ); 

  // loc[begScanLocIdx:begScanLocIdx+p->cn-1] must be valid
  if( p->begScanLocIdx + p->cn > p->locN )
    return false;

  unsigned i,j;

  for(j=1; j<p->cn; ++j)
    for(i=1; i<p->rn; ++i)
    {
      cmScAlignLoc_t* loc   = p->loc + p->begScanLocIdx + j - 1;
      unsigned        pitch = p->midiBuf[i-1].pitch;
      cmScAlignVal_t* vp    = _cmScAlignValPtr(p,i,j);
      vp->matchFl           = _cmScAlignIsMatch(loc,pitch);
      unsigned        cost  = vp->matchFl ? 0 : 1;
      vp->v[kSaSubIdx]      = _cmScAlignMin(p,i-1,j-1) + cost;
      vp->v[kSaDelIdx]      = _cmScAlignMin(p,i-1,j  ) + 1;
      vp->v[kSaInsIdx]      = _cmScAlignMin(p,i,  j-1) + 1;
      vp->v[kSaMinIdx]      = cmMin( vp->v[kSaSubIdx], cmMin(vp->v[kSaDelIdx],vp->v[kSaInsIdx]));
      vp->transFl           = _cmScAlignIsTrans(p,vp,i-1,j-1);     
    }

  return true;
}

void _cmScAlignPathPush( cmScAlign* r, unsigned code, unsigned ri, unsigned ci, bool matchFl, bool transFl )
{
  assert(r->p_avl != NULL );
  cmScAlignPath_t* p = r->p_avl;
  r->p_avl = r->p_avl->next;

  p->code    = code;
  p->ri      = ri;
  p->ci      = ci;
  p->matchFl = code==kSaSubIdx ? matchFl : false;
  p->transFl = transFl;
  p->next    = r->p_cur;  
  r->p_cur   = p;
}

void _cmScAlignPathPop( cmScAlign* r )
{
  assert( r->p_cur != NULL );
  cmScAlignPath_t* tp    = r->p_cur->next;
  r->p_cur->next = r->p_avl;
  r->p_avl       = r->p_cur;
  r->p_cur       = tp;
}

double _cmScAlignScoreCandidate( cmScAlign* r )
{
  cmScAlignPath_t* cp = r->p_cur;
  cmScAlignPath_t* bp = r->p_cur;
  cmScAlignPath_t* ep = NULL;

  for(; cp!=NULL; cp=cp->next)
    if( cp->code != kSaInsIdx )
    {
      bp = cp;
      break;
    }
  
  for(; cp!=NULL; cp=cp->next)
    if( cp->code!=kSaInsIdx )
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
    //if( pc != cp->code && cp->code != kSaSubIdx && pc==kSaSubIdx && pfl==true )
    if( pfl==true && cp->matchFl==false )
      ++gapCnt;

    //
    switch( cp->code )
    {
      case kSaSubIdx:
        penalty += cp->matchFl ? 0 : 1;
        penalty -= cp->transFl ? 1 : 0;
        break;

      case kSaDelIdx:
        penalty += 1;
        break;

      case kSaInsIdx:
        penalty += 1;
        break;
    }

    pfl = cp->matchFl;
    
  }

  double score = gapCnt/n + penalty;

  //printf("n:%i gaps:%f gap_score:%f penalty:%f score:%f\n",n,gapCnt,gapCnt/n,penalty,score);

  return score;

}

void _cmScAlignEvalCandidate( cmScAlign* r, double score )
{
  
  if( r->s_opt == DBL_MAX || score < r->s_opt)
  {
    // copy the p_cur to p_opt[]
    cmScAlignPath_t* cp = r->p_cur;
    unsigned i;
    for(i=0; cp!=NULL && i<r->pn; cp=cp->next,++i)
    {
      r->p_opt[i].code    = cp->code;
      r->p_opt[i].ri      = cp->ri;
      r->p_opt[i].ci      = cp->ci;
      r->p_opt[i].matchFl = cp->matchFl;
      r->p_opt[i].transFl = cp->transFl;
      r->p_opt[i].next    = cp->next==NULL ? NULL : r->p_opt + i + 1;
    }
    
    assert( i < r->pn );
    r->p_opt[i].code = 0; // terminate with code=0        
    r->s_opt = score;
  }
}



// traverse the solution matrix from the lower-right to 
// the upper-left.
void _cmScAlignGenPaths( cmScAlign* r, int i, int j )
{
  unsigned m;

  // stop when the upper-right is encountered
  if( i==0 && j==0 )
  {
    _cmScAlignEvalCandidate(r, _cmScAlignScoreCandidate(r) );
    return;
  }

  cmScAlignVal_t* vp = _cmScAlignValPtr(r,i,j);

  // for each possible dir: up,left,up-left
  for(m=1; m<kSaCnt; ++m)
    if( vp->v[m] == vp->v[kSaMinIdx] )
    {
      // prepend to the current candidate path: r->p_cur
      _cmScAlignPathPush(r,m,i,j,vp->matchFl,vp->transFl);

      int ii = i-1;
      int jj = j-1;

      switch(m)
      {
        case kSaSubIdx:
          break;

        case kSaDelIdx:
          jj = j;
          break;

        case kSaInsIdx:
          ii = i;
          break;
      }

      // recurse!
      _cmScAlignGenPaths(r,ii,jj);

      // remove the first element from the current path
      _cmScAlignPathPop(r);
    }
  
}

double _cmScAlign( cmScAlign* p )
{
  int      i = p->rn-1;
  int      j = p->cn-1;
  unsigned m = _cmScAlignMin(p,i,j); //p->m[i + (j*p->rn)].v[kSaMinIdx];

  if( m==cmMax(p->rn,p->cn) )
    printf("Edit distance is at max: %i. No Match.\n",m);
  else
    _cmScAlignGenPaths(p,i,j);

  return p->s_opt;
}

cmRC_t  cmScAlignExec(  cmScAlign* p, unsigned smpIdx, unsigned status, cmMidiByte_t d0, cmMidiByte_t d1 )
{
  bool fl    = p->mbi > 0;
  cmRC_t rc = cmOkRC;

  // update the MIDI buffer with the incoming note
  cmScAlignInputMidi(p,smpIdx,status,d0,d1);

  // if the MIDI buffer transitioned to full then perform an initial scan sync.
  if( fl && p->mbi == 0 )
  {
    if( (p->begSyncLocIdx = cmScAlignScan(p,cmInvalidCnt)) == cmInvalidIdx )
      rc = cmInvalidArgRC; // signal init. scan sync. fail
  }
  else
  {
    // if the MIDI buffer is full then perform a step sync.
    if( !fl && p->mbi == 0 ) 
      rc = cmScAlignStep(p);
  }

  return rc;
}

bool  cmScAlignInputMidi(  cmScAlign* p, unsigned smpIdx, unsigned status, cmMidiByte_t d0, cmMidiByte_t d1 )
{
  if( status != kNoteOnMdId )
    return false;

  unsigned mi = p->mn-1;

  //printf("%3i %5.2f %4s\n",p->mni,(double)smpIdx/p->srate,cmMidiToSciPitch(d0,NULL,0));

  // shift the new MIDI event onto the end of the MIDI buffer
  memmove(p->midiBuf,p->midiBuf+1,sizeof(cmScAlignMidiEvt_t)*mi);
  p->midiBuf[mi].locIdx = cmInvalidIdx;
  p->midiBuf[mi].cbCnt  = 0;
  p->midiBuf[mi].mni    = p->mni++;
  p->midiBuf[mi].smpIdx = smpIdx;
  p->midiBuf[mi].pitch  = d0;
  p->midiBuf[mi].vel    = d1;
  if( p->mbi > 0 )
    --p->mbi;

  return true;
}

// If mep==NULL then the identified score location was not matched (this is an 'insert')
// these only occurr during 'scan' not 'step'.
//
// If locIdx == cmInvalidIdx then the MIDI event did not match a score location 
// When this occurrs during a scan then this is a 'deleted' MIDI note otherwise 
// the note was not found inside loc[esi-stepCnt:esi+stepCnt].
//
// If mep!=NULL && scLocIdx!=cmInvalidIdx but matchFl==false then this is a 
// 'substitute' with a mismatch. These only occur during 'scan'.
void _cmScAlignCb( cmScAlign* p, unsigned locIdx, cmScAlignMidiEvt_t* mep, bool matchFl, bool transFl )
{
  // verify that the result buffer is not full
  if( p->ri >= p->resN )
  {
    cmCtxRtCondition( &p->obj, cmArgAssertRC, "The score alignment result buffer is full."); 
    return;
  }
  // don't report unmatched score locations
  if( mep == NULL ) 
    return;

  ++mep->cbCnt;

  cmScAlignResult_t* rp = NULL;

  // if this is the first time this MIDI event has generated a callback ...
  if( mep->cbCnt == 1 )
    rp = p->res + p->ri++;  // ... then create a new record in p->res[] ...
  else
    if( mep->cbCnt > 1 && matchFl ) // ... otherwise if it was matched ...
    {
      unsigned i; 
      for(i=0; i<p->ri; ++i)
        if(p->res[i].mni == mep->mni )
        {
          if( p->res[i].matchFl == false ) // ... and it's previous recd was not matched then update the record with the match info.
            rp = p->res + i;
        }
    }

  if(rp == NULL )
    return;
    
  assert( locIdx != cmInvalidIdx || mep != NULL );

  rp->locIdx   = locIdx;
  rp->smpIdx   = mep==NULL ? cmInvalidIdx         : mep->smpIdx;
  rp->mni      = mep==NULL ? cmInvalidIdx         : mep->mni;
  rp->pitch    = mep==NULL ? kInvalidMidiPitch    : mep->pitch;
  rp->vel      = mep==NULL ? kInvalidMidiVelocity : mep->vel;
  rp->matchFl  = mep==NULL ? false                : matchFl;
  rp->transFl  = mep==NULL ? false                : transFl;
}

void _cmScAlignPrintPath( cmScAlign* p, cmScAlignPath_t* cp, unsigned bsi )
{
  assert( bsi != cmInvalidIdx );

  cmScAlignPath_t* pp    = cp;
  int              polyN = 0;
  int              i;

  printf("loc: ");

  // get the polyphony count for the score window 
  for(i=0; pp!=NULL; pp=pp->next)
  {
    cmScAlignLoc_t* lp = p->loc + bsi + pp->ci;
    if( pp->code!=kSaDelIdx  )
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
      cmScAlignLoc_t* lp = p->loc + locIdx;
      if( pp->code!=kSaDelIdx && lp->evtCnt >= i )
        printf("%4s ",cmMidiToSciPitch(lp->evtV[i-1].pitch,NULL,0));
      else
        printf("%4s ", pp->code==kSaDelIdx? "-" : " ");
    }
    printf("\n");
  }

  printf("mid: ");

  // print the MIDI buffer
  for(pp=cp; pp!=NULL; pp=pp->next)
  {
    if( pp->code!=kSaInsIdx )
      printf("%4s ",cmMidiToSciPitch(p->midiBuf[pp->ri-1].pitch,NULL,0));
    else
      printf("%4s ",pp->code==kSaInsIdx?"-":" ");
  }

  printf("\nmni: ");

  // print the MIDI buffer index (mni)
  for(pp=cp; pp!=NULL; pp=pp->next)
  {
    if( pp->code!=kSaInsIdx )
      printf("%4i ",p->midiBuf[pp->ri-1].mni);
    else
      printf("%4s ",pp->code==kSaInsIdx?"-":" ");
  }

  printf("\n op: ");

  // print the substitute/insert/delete operation
  for(pp=cp; pp!=NULL; pp=pp->next)
  {
    char c = ' ';
    switch( pp->code )
    {
      case kSaSubIdx: c = 's'; break;
      case kSaDelIdx: c = 'd'; break;
      case kSaInsIdx: c = 'i'; break;
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
    if( pp->matchFl )
      s[k++] = 'm';

    if( pp->transFl )
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

// Returns the p->loc[] index at the start of the min cost score window
// based on the current MIDI buffer.
// scanCnt is the number of time the score window will be shifted one
// location to the left
unsigned cmScAlignScan( cmScAlign* p, unsigned scanCnt )
{
  unsigned bsi   = cmInvalidIdx;

  assert( p->mbi == 0 );
  
  // if the MIDI buf is full
  if( p->mbi == 0 )
  {
    double   s_opt = DBL_MAX;
    unsigned i;

    // Loop as long as the score window is inside the score.
    // Fill the Dyn Pgm matrix: MIDI_buf to score[begScanLocIdx:begScanLocIdx+scWndN-1].
    for(i=0; _cmScAlignCalcMtx(p) && (scanCnt==cmInvalidCnt || i<scanCnt); ++i)
    {
      // locate the path through the DP matrix with the lowest edit distance (cost)
      double cost = _cmScAlign(p);

      // if it is less than any previous score window
      if(cost < s_opt)
      {
        s_opt = cost;
        bsi   = p->begScanLocIdx; 
      }

      // increment the score window
      p->begScanLocIdx += 1;
    }
    
    // store the cost assoc'd with bsi
    p->s_opt = s_opt;

  }

  assert( bsi != cmInvalidIdx );


  // Traverse the least cost path and:
  // 1) Set p->esi to the score location index of the last MIDI note
  // which has a positive match with the score and assign
  // the internal score index to cp->locIdx.
  // 2) Set cmScAlignPath_t.locIdx - index into p->loc[] associated
  // with each path element that is a 'substitute' or an 'insert'.
  // 3) Set p->missCnt: the count of trailing non-positive matches.
  // p->missCnt is eventually used in cmScAlignStep() to track the number
  // of consecutive trailing missed notes.
  cmScAlignPath_t* cp = p->p_opt;
  unsigned          i = bsi;

  p->missCnt = 0;
  p->esi     = cmInvalidIdx;
  for(i=0; cp!=NULL; cp=cp->next)
  {

    if( cp->code != kSaInsIdx )
    {
      assert( cp->ri > 0 );
      p->midiBuf[ cp->ri-1 ].locIdx = cmInvalidIdx;
    }

    switch( cp->code )
    {
      case kSaSubIdx:
        if( cp->matchFl || cp->transFl)
        {
          p->esi     = bsi + i;
          p->missCnt = 0;

          if( cp->matchFl )
            p->midiBuf[ cp->ri-1 ].locIdx = bsi + i;
        }
        else
        {
          ++p->missCnt;
        }
        
        cp->locIdx = bsi + i;
        ++i;
        break;

      case kSaInsIdx:
        cp->locIdx = bsi + i;
        ++i;
        break;

      case kSaDelIdx:
        cp->locIdx = cmInvalidIdx;
        ++p->missCnt;
        break;

    }
  }

  // if no positive matches were found
  if( p->esi == cmInvalidIdx )
    bsi = cmInvalidIdx;
  else
  {

    // report matches
    for(cp=p->p_opt; cp!=NULL; cp=cp->next)
    {
      unsigned            locIdx = cp->locIdx;
      cmScAlignMidiEvt_t* mep    = NULL;

      if( cp->code != kSaInsIdx )
        mep = p->midiBuf + cp->ri - 1;

      _cmScAlignCb(p,locIdx,mep,cp->matchFl,cp->transFl);
    }
  }

  return bsi;
}


cmRC_t cmScAlignStep(  cmScAlign* p )
{
  int      i;
  unsigned pitch          = p->midiBuf[ p->mn-1 ].pitch;
  unsigned locIdx         = cmInvalidIdx;

  // the tracker must be sync'd to step
  if( p->esi == cmInvalidIdx )
    return cmCtxRtCondition( &p->obj, cmInvalidArgRC, "The p->esi value must be valid to perform a step operation."); 

  // if the end of the score has been reached
  if( p->esi + 1 >= p->locN )
    return cmEofRC;
    
  // attempt to match to next location first
  if( _cmScAlignIsMatch(p->loc + p->esi + 1, pitch) )
  {
    locIdx = p->esi + 1;
  }
  else
  {
    // 
    for(i=2; i<p->stepCnt; ++i)
    {
      // go forward 
      if( p->esi+i < p->locN && _cmScAlignIsMatch(p->loc + p->esi + i, pitch) )
      {
        locIdx = p->esi + i;
        break;
      }

      // go backward
      if( p->esi >= (i-1)  && _cmScAlignIsMatch(p->loc + p->esi - (i-1), pitch) )
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
    p->esi = locIdx;
    _cmScAlignCb(p,locIdx, p->midiBuf + p->mn - 1,true,false);
  }

  if( p->missCnt >= p->maxStepMissCnt )
  {
    p->begScanLocIdx = p->esi > p->rn ? p->esi - p->rn : 0;
    p->s_opt         = DBL_MAX;
    unsigned bsi     = cmScAlignScan(p,p->rn*2);
    ++p->scanCnt;

    // if the scan failed find a match
    if( bsi == cmInvalidIdx )
      return cmCtxRtCondition( &p->obj, cmSubSysFailRC, "Scan resync. failed."); 

    //if( bsi != cmInvalidIdx )
    //  _cmScAlignPrintPath(p, p->p_opt, bsi );
  }

  return cmOkRC;
}


void _cmScAlignPrintMtx( cmScAlign* r)
{
  unsigned i,j,k;
  for(i=0; i<r->rn; ++i)
  {
    for(j=0; j<r->cn; ++j)
    {
      printf("(");
      
      const cmScAlignVal_t* vp = _cmScAlignValPtr(r,i,j);

      for(k=0; k<kSaCnt; ++k)
      {
        printf("%i",vp->v[k]);
        if( k<kSaCnt-1)
          printf(", ");
        else
          printf(" ");
      }

      printf("%c)",vp->transFl?'t':' ');
      
    }
    printf("\n");
  }
}


void cmScAlignPrintOpt( cmScAlign* p )
{
  unsigned i;
  for(i=0; p->p_opt[i].code!=0; ++i)
  {
    cmScAlignPath_t* cp = p->p_opt + i;
    char             c0 = cp->matchFl ? 'm' : ' ';
    char             c1 = cp->transFl ? 't' : ' ';
    printf("%2i code:%i ri:%2i ci:%2i %c%c\n",i,cp->code,cp->ri,cp->ci,c0,c1);
  }
  printf("score:%f\n",p->s_opt);
}




enum
{
  kBarSaFl      = 0x01,  // this is a score bar
  kScNoteSaFl   = 0x02,  // this is a score reference note (if mni != cmInvalidIdx then it was matched)
  kSubsErrSaFl  = 0x04,  // 'subs' mismatch midi note
  kMidiErrSaFl  = 0x08,  // 'deleted' Midi note
};

typedef struct cmScAlignPrint_str
{
  unsigned                   flags;
  unsigned                   scLocIdx;
  unsigned                   smpIdx;
  unsigned                   pitch;
  unsigned                   vel;
  unsigned                   mni;
  bool                       matchFl;
  bool                       transFl;
} cmScAlignPrint_t;

void _cmScAlignPrintList( cmScAlignPrint_t* a, unsigned an )
{
  cmScAlignPrint_t* pp;
  unsigned i;
  printf("----------------------------------------------------\n");
  printf("idx scl mni pit  flg \n");
  for(i=0; i<an; ++i)
  {
    pp = a + i;
    printf("%3i %3i %3i %4s 0x%x\n",i,pp->scLocIdx,pp->mni,
      pp->pitch==kInvalidMidiPitch ? " " : cmMidiToSciPitch(pp->pitch,NULL,0),
      pp->flags);
  }
  printf("\n");
}

// insert a black record at a[i]
unsigned _cmScAlignPrintExpand( cmScAlignPrint_t* a, unsigned aan, unsigned i, unsigned an )
{
  assert( an < aan );
  memmove( a + i + 1, a + i, (an-i)*sizeof(cmScAlignPrint_t));
  memset( a + i, 0, sizeof(cmScAlignPrint_t));
  return an + 1;
}

void _cmScAlignPrintOutResult( cmScAlign* p, cmScAlignResult_t* rp, const cmChar_t* label )
{
  printf("loc:%4i scloc:%4i smp:%10i mni:%4i %4s %c %c %s\n",
    rp->locIdx,
    rp->locIdx==cmInvalidIdx ? -1 : p->loc[rp->locIdx].scLocIdx,
    rp->smpIdx,
    rp->mni,
    rp->pitch<=127 ? cmMidiToSciPitch(rp->pitch,NULL,0) : " ",
    rp->matchFl ? 'm' : ' ',
    rp->transFl ? 't' : ' ',
    label);
}

void _cmScAlignPrintSet( cmScAlignPrint_t* pp, const cmScAlignResult_t* rp, unsigned flags, unsigned scLocIdx  )
{
  pp->scLocIdx = scLocIdx;
  pp->flags    = flags;
  pp->smpIdx   = rp->smpIdx;
  pp->pitch    = rp->pitch;
  pp->vel      = rp->vel;
  pp->mni      = rp->mni;

  assert( pp->scLocIdx!=cmInvalidIdx || pp->mni != cmInvalidIdx );

}

unsigned _cmScAlignPrintPoly( cmScAlignPrint_t* a, unsigned an, unsigned scLocIdx )
{
  unsigned polyN = 0;
  unsigned i;
  for(i=0; i<an; ++i)
    if( a[i].scLocIdx == scLocIdx )
      break;

  if( i < an )
  {
    for(; i<an; ++i,++polyN)
      if( a[i].scLocIdx != scLocIdx )
        break;

    // identical scLocIdx values must be consecutive
    for(; i<an; ++i)
    {
      if( a[i].scLocIdx == scLocIdx )
        _cmScAlignPrintList(a,an);

      assert( a[i].scLocIdx != scLocIdx );
    }
  }    

  return polyN;
}

cmScAlignPrint_t* _cmScAlignPrintRecd(cmScAlignPrint_t* a, unsigned an, unsigned scLocIdx, unsigned polyIdx )
{
  unsigned i,j;
  for(i=0; i<an; ++i)
  {
    if( a[i].scLocIdx == scLocIdx )
      for(j=0; i<an; ++j,++i)
      {
        if( a[i].scLocIdx != scLocIdx )
          break;

        if( j == polyIdx )
          return a + i;
      }
  }
  return NULL;
}


void _cmScAlignPrintReport( cmScAlign* p, cmScAlignPrint_t* a, unsigned an, unsigned bsi, unsigned esi )
{
  unsigned colN    = 5;
  unsigned bli     = bsi;
  bool     titleFl = true;

  while( bli < esi )
  {
    unsigned i,j;

    // get ending scLocIdx
    unsigned eli   = cmMin(bli+colN, esi);

    // get the max poly count
    unsigned polyN = 0;
    for(i=bli; i<eli; ++i)
    {
      unsigned pn = _cmScAlignPrintPoly(a,an,i);

      if( pn > polyN )
        polyN = pn;      
    }   

    // print titles
    if( titleFl )
    {
      printf("     ");
      for(j=bli; j<eli; ++j)
        printf("| %4s %4s %3s %1s ","mni"," ","vel"," ");
      printf("\n");
      titleFl = false;
    }

    // print 'loc' index line
    printf("scl: ");
    for(j=bli; j<eli; ++j)
      printf("| %4i %4s %3s %1s ",j," "," "," ");
    printf("\n");

    for(i=polyN; i>0; --i)
    {
      printf("%3i: ",i);
      for(j=bli; j<eli; ++j)
      {
        cmScAlignPrint_t* pp;
    
        if((pp = _cmScAlignPrintRecd(a,an,j,i-1)) == NULL )
          printf("| %4s %4s %3s %1s "," "," "," "," ");
        else
        {
          if( pp->mni == cmInvalidIdx && cmIsNotFlag(pp->flags,kBarSaFl) )
            printf("| %4s %4s %3s %1s "," ",cmMidiToSciPitch(pp->pitch,NULL,0)," "," ");
          else
          {
            if( cmIsFlag(pp->flags,kBarSaFl) )
              printf("| %4s %4s %3i %1s "," "," | ",pp->pitch,"b");
            else
            {
              const cmChar_t* op = cmIsFlag(pp->flags,kMidiErrSaFl) ? "d" : " ";
              op = cmIsFlag(pp->flags,kSubsErrSaFl) ? "s" : op;
              printf("| %4i %4s %3i %1s ",pp->mni,cmMidiToSciPitch(pp->pitch,NULL,0),pp->vel,op);
            }
          }
        }
      }
      printf("\n");
    }
    printf("\n");

    bli = eli;
  }

}

// The goal of this function is to create a cmScAlignPrint_t array containing
// one record for each score bar, score note and errant MIDI note.
// The function works by first creating a record for each score bar and note
// and then scanning the cmScAlignResult_t array (p->res[]) for each result
// record create by an earlier call to _cmScAlignCb().  A result record can 
// uniquely indicates one of the following result states based on receiving
// a MIDI event.
// Match      - locIdx!=cmInvalidIdx matchFl==true  mni!=cmInvalidIdx
// Mis-match  - locIdx!=cmInvalidIdx matchFl==false mni!=cmInvalidIdx
// Delete     - locIdx==cmInvalidIdx matchFl==false mni!=cmInvalidIdx
// Insert     - locIdx==cmInvalidIdx matchFl==false mni==cmInvalidIdx
//
// This is made slightly more complicated by the fact that a given MIDI event
// may generate more than one result record.  This can occur when the 
// tracker is in 'step' mode and generates a result record with a given state
// as a result of a given MIDI note and then reconsiders that MIDI note
// while during a subsequent 'scan' mode resync. operation.  For example
// a MIDI note which generate a 'delete' result during a step operation 
// may later generate a match result during a scan. 
double _cmScAlignPrintResult( cmScAlign* p )
{
  // determine the scH score begin and end indexes
  unsigned bsi = cmScoreLocCount(p->scH);
  unsigned esi = 0;
  unsigned i,j;
  for(i=0; i<p->ri; ++i)
  {
    cmScAlignResult_t* rp = p->res + i;
 
    assert( rp->locIdx==cmInvalidIdx || rp->locIdx<p->locN);
    if( rp->locIdx != cmInvalidIdx  )
    {
      bsi = cmMin(bsi,p->loc[ rp->locIdx ].scLocIdx);
      esi = cmMax(esi,p->loc[ rp->locIdx ].scLocIdx);
    }
  }

  // get a count of MIDI events + score events
  unsigned  aan = p->ri;
  for(i=bsi; i<=esi; ++i)
  {
    cmScoreLoc_t*     lp = cmScoreLoc( p->scH, i);

    aan += lp->evtCnt;
  }

  cmScAlignPrint_t* a   = cmMemAllocZ(cmScAlignPrint_t,aan);
  unsigned          an  = 0;
  unsigned          scNoteCnt = 0; // notes in the score
  unsigned          matchCnt  = 0; // matched score notes
  unsigned          wrongCnt  = 0; // errant midi notes
  unsigned          skipCnt   = 0; // skipped score events

  // create a record for each score event
  for(i=bsi; i<=esi; ++i)
  {
    cmScoreLoc_t*     lp = cmScoreLoc( p->scH, i);
    
    for(j=0; j<lp->evtCnt; ++j,++an)
    {
      assert( an < aan );
      cmScAlignPrint_t* pp = a + an;

      assert( lp->index != cmInvalidIdx );

      pp->scLocIdx = lp->index;   
      pp->mni      = cmInvalidIdx;
      pp->pitch    = kInvalidMidiPitch;
      pp->vel      = cmInvalidIdx;

      switch( lp->evtArray[j]->type )
      {
        case kBarEvtScId:
          pp->flags = kBarSaFl;
          pp->pitch = lp->evtArray[j]->barNumb;
          pp->mni   = cmInvalidIdx;
          break;

        case kNonEvtScId:
          pp->flags = kScNoteSaFl;
          pp->pitch = lp->evtArray[j]->pitch;
          ++scNoteCnt;
          break;
      }
    }
  }

  //_cmScAlignPrintList(a,an);

  // Update the score with matching MIDI notes

  // for each result record ...
  for(i=0; i<p->ri; ++i)
  {
    cmScAlignResult_t* rp       = p->res + i; 

    rp->foundFl = false;

    // ... if this is not an errant MIDI note (delete)
    if( rp->locIdx != cmInvalidIdx )
    {
      assert( rp->locIdx != cmInvalidIdx && rp->locIdx < p->locN );

      unsigned           scLocIdx = p->loc[rp->locIdx].scLocIdx;
      cmScAlignPrint_t*  pp;

    
      // ... find the score location matching the result record score location 
      for(j=0; j<an; ++j)
      {
        pp = a + j;

        // if this score location matches the result score location 
        if( scLocIdx == pp->scLocIdx )
        {
          // if this is a matching midi node
          if( rp->matchFl && cmIsFlag(pp->flags,kScNoteSaFl) && pp->pitch == rp->pitch )
          {
            //_cmScAlignPrintOutResult(p,rp,"match");
            rp->foundFl      = true;
            _cmScAlignPrintSet(pp, rp, pp->flags, pp->scLocIdx  );
            ++matchCnt;
            break;
          }

          // if this is a 'substitute' non-matching note
          if( rp->matchFl == false && rp->mni != cmInvalidIdx )
          {
            //_cmScAlignPrintOutResult(p,rp,"mis-match");
            ++j; // insert after the a[j]
            an = _cmScAlignPrintExpand(a,aan,j,an);
            _cmScAlignPrintSet(a + j, rp, kSubsErrSaFl, scLocIdx  );
            rp->foundFl = true;
            ++wrongCnt;
            break;
          }

          // if this is a 'skipped' score note ('insert') alert
          if( rp->mni == cmInvalidIdx )
          {
            //_cmScAlignPrintOutResult(p,rp,"skip");
            rp->foundFl = true;
            break;
          }
        
        }
      }
    }

    if( rp->foundFl == false )
    {
      //  _cmScAlignPrintOutResult(p,rp,"not-found");
    }
  } 

  //_cmScAlignPrintList(a,an);

  // Insert records into the print record array (a[](
  // to represent errant MIDI notes. (Notes which 
  // were played but do not match any notes in the score.)
  
  // for each result record ...
  for(i=0; i<p->ri; ++i)
  {
    cmScAlignResult_t* rp       = p->res + i; 
    cmScAlignPrint_t*  pp       = NULL;
    cmScAlignPrint_t*  dpp      = NULL;
    unsigned           dmin;

    // if this result did not have a matching score event
    if(rp->foundFl)
      continue;

    // find the print recd with the closest mni
    for(j=0; j<an; ++j)
    {
      pp = a + j;
      if( pp->mni!=cmInvalidId )
      {
        unsigned d;
        if( pp->mni > rp->mni )
          d = pp->mni - rp->mni;
        else
          d = rp->mni - pp->mni;

        if( dpp == NULL || d < dmin )
        {
          dpp  = pp;
          dmin = d;
        }
      }
    }

    assert( dpp != NULL  );

    j = dpp - a;

    if( rp->mni > dpp->mni )
      ++j;

    assert( rp->locIdx == cmInvalidIdx );
    
    // insert a print recd before or after the closest print recd
    an = _cmScAlignPrintExpand(a,aan,j,an);
    _cmScAlignPrintSet(a + j, rp, kMidiErrSaFl, dpp->scLocIdx  );

    ++wrongCnt;
    
  }

  for(i=0; i<an; ++i)
    if( cmIsFlag(a[i].flags,kScNoteSaFl) && (a[i].mni == cmInvalidIdx || cmIsFlag(a[i].flags,kSubsErrSaFl)))
      ++skipCnt;

  //_cmScAlignPrintList(a,an);

  //_cmScAlignPrintReport(p,a,an,bsi,esi);

  double prec = (double)2.0 * matchCnt / (matchCnt + wrongCnt);
  double rcal = (double)2.0 * matchCnt / (matchCnt + skipCnt);
  double fmeas = prec * rcal / (prec + rcal);

  printf("midi:%i scans:%i score notes:%i match:%i skip:%i wrong:%i : %f\n",p->mni,p->scanCnt,scNoteCnt,matchCnt,skipCnt,wrongCnt,fmeas);

  cmMemFree(a);

  return fmeas;
}


cmRC_t cmScAlignScanToTimeLineEvent( cmScAlign* p, cmTlH_t tlH, cmTlObj_t* top, unsigned endSmpIdx )
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
      rc = cmScAlignExec(p, mep->obj.seqSmpIdx, mep->msg->status, mep->msg->u.chMsgPtr->d0, mep->msg->u.chMsgPtr->d1 );

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

void cmScAlignCb( void* cbArg, unsigned scLocIdx, unsigned mni, unsigned pitch, unsigned vel )
{
  //cmScAlign* p = (cmScAlign*)cbArg;
  
}

void       cmScAlignScanMarkers(  cmRpt_t* rpt, cmTlH_t tlH, cmScH_t scH )
{
  unsigned   i;
  double     srate       = 96000;
  unsigned   midiN       = 7; 
  unsigned   scWndN      = 10;
  unsigned   markN       = 291;
  cmCtx*     ctx         = cmCtxAlloc(NULL, rpt, cmLHeapNullHandle, cmSymTblNullHandle );
  cmScAlign* p           = cmScAlignAlloc(ctx,NULL,cmScAlignCb,NULL,srate,scH,midiN,scWndN);
  unsigned   markCharCnt = 31;
  cmChar_t   markText[ markCharCnt+1 ];

  double     scoreThresh  = 0.5;
  unsigned   candCnt      = 0;
  unsigned   initFailCnt  = 0;
  unsigned   otherFailCnt = 0;
  unsigned   scoreFailCnt = 0;

  p->cbArg = p; // set the callback arg. 
  
  // for each marker
  for(i=0; i<markN; ++i)
  {
    // form the marker text
    snprintf(markText,markCharCnt,"Mark %i",i);

    // locate the marker
    cmTlMarker_t*  mp    = cmTimeLineMarkerFind( tlH, markText );
    if( mp == NULL )
    {
      printf("The marker '%s' was not found.\n\n",markText);  
      continue;
    }

    // skip markers which do not contain text
    if( cmTextIsEmpty(mp->text) )
    {
      printf("The marker '%s' is being skipped because it has no text.\n\n",markText);  
      continue;
    }

    // reset the score follower to the beginnig of the score
    cmScAlignReset(p,0);

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
        printf("mark:%i midi:%i Not enough MIDI notes to fill the scan buffer.\n",i,p->mni);
        pfl = false;
      }
      else
      {
        switch(rc)
        {
          case cmInvalidArgRC:
            printf("mark:%i INITIAL SYNC FAIL\n",i);
            ++initFailCnt;
            pfl = false;
            break;

          case cmSubSysFailRC:
            printf("mark:%i SCAN RESYNC FAIL\n",i);
            ++otherFailCnt;
            break;

          default:
            printf("mark:%i UNKNOWN FAIL\n",i);
            ++otherFailCnt;
        }
      }
    }

    if( pfl )
    {      
      //_cmScAlignPrintMtx(p);
      printf("mark:%i scans:%4i loc:%4i bar:%4i score:%5.2f miss:%i text:'%s'\n",i,p->scanCnt,p->begSyncLocIdx,p->loc[p->begSyncLocIdx].barNumb,p->s_opt,p->missCnt,mp->text);
      //_cmScAlignPrintPath(p, p->p_opt, bsi );

      //printf("mark:%i scans:%i midi:%i text:'%s'\n",i,p->scanCnt,p->mni,mp->text);

      if( _cmScAlignPrintResult(p) < scoreThresh )
        ++scoreFailCnt;

    }

    
    //break;  // ONLY USE ONE MARKER DURING TESTING

    printf("\n");

  }

  printf("cand:%i fail:%i - init:%i score:%i other:%i\n\n",candCnt,initFailCnt+scoreFailCnt+otherFailCnt,initFailCnt,scoreFailCnt,otherFailCnt);

  cmScAlignFree(&p);  
  cmCtxFree(&ctx);
}
