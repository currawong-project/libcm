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



cmScFol* cmScFolAlloc( cmCtx* c, cmScFol* p, cmReal_t srate, unsigned wndN, cmReal_t wndMs, cmScH_t scH )
{
  cmScFol* op = cmObjAlloc(cmScFol,c,p);
  if( srate != 0 )
    if( cmScFolInit(op,srate,wndN,wndMs,scH) != cmOkRC )
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
    cmMemFree(p->loc[i].pitchV);

  cmMemFree(p->loc);
  cmObjFree(pp);
  return rc;
}


cmRC_t   cmScFolFinal( cmScFol* p )
{ return cmOkRC; }

void _cmScFolPrint( cmScFol* p )
{
  int i,j;
  for(i=0; i<p->locN; ++i)
  {
    printf("%2i %5i ",p->loc[i].barNumb,p->loc[i].evtIdx);
    for(j=0; j<p->loc[i].pitchCnt; ++j)
      printf("%s ",cmMidiToSciPitch(p->loc[i].pitchV[j],NULL,0));
    printf("\n");
  }
}

cmRC_t   cmScFolInit(  cmScFol* p, cmReal_t srate, unsigned wndN, cmReal_t wndMs, cmScH_t scH )
{
  cmRC_t rc;
  if((rc = cmScFolFinal(p)) != cmOkRC )
    return rc;

  p->srate          = srate;
  p->scH            = scH;
  p->maxWndSmp      = floor(wndMs * srate / 1000.0);
  p->wndN           = wndN;
  p->wndV           = cmMemResizeZ(cmScFolWndEle_t,p->wndV,wndN);
  p->locN           = cmScoreEvtCount(scH);
  p->loc            = cmMemResizeZ(cmScFolLoc_t,p->loc,p->locN);
  p->sri            = cmInvalidIdx;
  p->sbi            = cmInvalidIdx;
  p->sei            = cmInvalidIdx;
  p->edWndMtx       = cmVOU_LevEditDistAllocMtx(p->wndN);
  p->evalWndN       = 5;
  p->allowedMissCnt = 1;

  assert(p->evalWndN<p->wndN);

  int i,n;
  double        maxDSecs = 0;   // max time between events to be considered simultaneous
  cmScoreEvt_t* e0p      = NULL;
  int           j0       = 0;

  // for each score event
  for(i=0,n=0; i<p->locN; ++i)
  {
    cmScoreEvt_t* ep = cmScoreEvt(scH,i);

    // if the event is not a note then ignore it
    if( ep->type == kNonEvtScId )
    {
      assert( j0+n < p->locN );

      p->loc[j0+n].evtIdx  = i;
      p->loc[j0+n].barNumb = ep->barNumb;

      // if the first event has not yet been selected
      if( e0p == NULL )
      {
        e0p = ep;
        n   = 1;
      }
      else
      {
        // time can never reverse
        assert( ep->secs >= e0p->secs );  

        // calc seconds between first event and current event
        double dsecs = ep->secs - e0p->secs;

        // if the first event and current event are simultaneous...
        if( dsecs <= maxDSecs )
          ++n;   // ... incr. the count of simultaneous events
        else
        {
          int k;
          //  ... a complete set of simultaneous events have been located
          // duplicate all the events at each of their respective time locations
          for(k=0; k<n; ++k)
          {
            int m;
            assert( j0+k < p->locN );

            p->loc[j0+k].pitchCnt = n;
            p->loc[j0+k].pitchV   = cmMemAllocZ(unsigned,n);

            for(m=0; m<n; ++m)
            {
              cmScoreEvt_t* tp = cmScoreEvt(scH,p->loc[j0+m].evtIdx);
              assert(tp!=NULL);
              p->loc[j0+k].pitchV[m] = tp->pitch;
            }
          }
          
          e0p = ep;
          j0 += n;
          n   = 1;
        }
      }
    }
  }

  p->locN = j0;

  //_cmScFolPrint(p);

  return rc;
}

cmRC_t   cmScFolReset(   cmScFol* p, unsigned scoreIndex )
{
  int i;

  // zero the event index
  memset(p->wndV,0,sizeof(cmScFolWndEle_t)*p->wndN);

  // don't allow the score index to be prior to the first note
  if( scoreIndex < p->loc[0].evtIdx )
    scoreIndex = p->loc[0].evtIdx;

  // locate the score element in svV[] that is closest to,
  // and possibly after, scoreIndex.
  for(i=0; i<p->locN-1; ++i)
    if( p->loc[i].evtIdx <= scoreIndex && scoreIndex < p->loc[i+1].evtIdx  )
      break;

  // force scEvtIndex to be valid
  assert( i<p->locN );

  p->sri = i;
  p->sbi = i;

  // score event window is dBar bars before and after scEvtIndex;
  int dBar = 1;

  // backup dBar bars from the 'scoreIndex'
  for(; i>=0; --i)
    if( p->loc[i].barNumb >= (p->loc[p->sri].barNumb-dBar) ) 
      p->sbi = i;
    else
      break;
  
  dBar = 3;

  // move forward dBar bars from 'scoreIndex'
  for(i=p->sri; i<p->locN; ++i)
    if( p->loc[i].barNumb <= (p->loc[p->sri].barNumb+dBar) )
      p->sei = i;
    else
      break;

  return cmOkRC;
}

bool  _cmScFolIsMatch( const cmScFolLoc_t* loc, unsigned pitch )
{
  unsigned i;
  for(i=0; i<loc->pitchCnt; ++i)
    if( loc->pitchV[i] == pitch )
      return true;
  return false;
}

int _cmScFolDist(unsigned mtxMaxN, unsigned* m, const unsigned* s1, const cmScFolLoc_t* s0, int n )
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
      int cost = _cmScFolIsMatch(s0 + i-1, s1[j-1]) ? 0 : 1;

      //m[i][j] = min( m[i-1][j] + 1, min( m[i][j-1] + 1, m[i-1][j-1] + cost ) );
					
      m[ ii + j ] = v = cmMin( m[ i_1 + j] + 1, cmMin( m[ ii + j - 1] + 1, m[ i_1 + j - 1 ] + cost ) );
    }
  }	
  return v;		
}


unsigned   cmScFolExec(  cmScFol* p, unsigned smpIdx, unsigned status, cmMidiByte_t d0, cmMidiByte_t d1 )
{
  assert( p->sri != cmInvalidIdx );

  unsigned ret_idx = cmInvalidIdx;
  unsigned ewnd[ p->wndN ];

  if( status != kNoteOnMdId && d1>0 )
    return ret_idx;

  // left shift wndV[] to make the right-most element available - then copy in the new element
  memmove(p->wndV, p->wndV+1, sizeof(cmScFolWndEle_t)*(p->wndN-1));
  p->wndV[ p->wndN-1 ].smpIdx = smpIdx;
  p->wndV[ p->wndN-1 ].val    = d0;
  p->wndV[ p->wndN-1 ].validFl= true;

  // fill in ewnd[] with the valid values in wndV[]
  int i = p->wndN-1;
  int en = 0;
  for(; i>=0; --i,++en)
  {
    if( p->wndV[i].validFl /*&& ((smpIdx-p->wnd[i].smpIdx)<=maxWndSmp)*/)
      ewnd[i] = p->wndV[i].val;
    else
      break;
  }
  ++i; // increment i to the first valid element in ewnd[].

  int k;
  printf("en:%i sbi:%i sei:%i pitch:%s : ",en,p->sbi,p->sei,cmMidiToSciPitch(d0,NULL,0));
  for(k=i; k<p->wndN; ++k)
    printf("%s ", cmMidiToSciPitch(ewnd[k],NULL,0));
  printf("\n");

  // en is the count of valid elements in ewnd[].
  // ewnd[i] is the first valid element

  int    j       = 0;
  int    dist;
  int    minDist = INT_MAX;
  int    minIdx  = cmInvalidIdx;
  for(j=0; p->sbi+en+j <= p->sei; ++j)
    if((dist = _cmScFolDist(p->wndN, p->edWndMtx, ewnd+i, p->loc + p->sbi+j, en )) < minDist )
    {
      minDist = dist;
      minIdx  = j;
    }

  // The best fit is on the score window: p->loc[sbi+minIdx : sbi+minIdx+en-1 ]
   
  int evalWndN = cmMin(en,p->evalWndN);

  assert(evalWndN<p->wndN);
  
  j = p->sbi+minIdx+en - evalWndN;

  // Determine how many of the last evalWndN elements match
  dist =  _cmScFolDist(p->wndN, p->edWndMtx, ewnd+p->wndN-evalWndN, p->loc+j, evalWndN );
  
  // a successful match has <= allowedMissCnt and an exact match on the last element  
  //if( dist <= p->allowedMissCnt && ewnd[p->wndN-1] == p->loc[p->sbi+minIdx+en-1] )
  if( /*dist <= p->allowedMissCnt &&*/ _cmScFolIsMatch(p->loc+(p->sbi+minIdx+en-1),ewnd[p->wndN-1]))
  {
    p->sbi  = p->sbi + minIdx;
    p->sei  = cmMin(p->sei+minIdx,p->locN-1);
    ret_idx = p->sbi+minIdx+en-1;
  }

  printf("minDist:%i minIdx:%i evalDist:%i sbi:%i sei:%i\n",minDist,minIdx,dist,p->sbi,p->sei);

  return ret_idx;
}
