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
  p->minWndLookAhead= 3; //minWndLookAhead;
  p->printFl        = false;
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
