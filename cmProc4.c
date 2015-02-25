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
  //if( rn >p->mrn && cn > p->mcn )
  if( rn*cn > p->mrn*p->mcn )
  {
    return cmCtxRtCondition( &p->obj, cmInvalidArgRC, "MIDI sequence length must be less than %i. Score sequence length must be less than %i.",p->mmn,p->msn); 
  }

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

      // zero the value field
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

unsigned _cmScMatchIsMatchIndex( const cmScMatchLoc_t* loc, unsigned pitch )
{
  unsigned i;
  for(i=0; i<loc->evtCnt; ++i)
    if( loc->evtV[i].pitch == pitch )
      return i;
  return cmInvalidIdx;
}

bool  _cmScMatchIsMatch( const cmScMatchLoc_t* loc, unsigned pitch )
{ return _cmScMatchIsMatchIndex(loc,pitch) != cmInvalidIdx; }

bool _cmScMatchIsTrans( cmScMatch* p, const cmScMatchMidi_t* midiV, const cmScMatchVal_t* v1p, unsigned bsi, unsigned i, unsigned j, unsigned rn, unsigned cn )
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
    unsigned        c00 = midiV[i-1].pitch;
    unsigned        c01 = midiV[i  ].pitch;
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
bool  _cmScMatchCalcMtx( cmScMatch* p, unsigned bsi, const cmScMatchMidi_t* midiV, unsigned rn, unsigned cn )
{
  // loc[begScanLocIdx:begScanLocIdx+cn-1] must be valid
  if( bsi + cn > p->locN )
    return false;

  unsigned i,j;

  for(j=1; j<cn; ++j)
    for(i=1; i<rn; ++i)
    {
      cmScMatchLoc_t* loc   = p->loc + bsi + j - 1;
      unsigned        pitch = midiV[i-1].pitch;
      cmScMatchVal_t* vp    = _cmScMatchValPtr(p,i,j,rn,cn);
      unsigned        idx   = _cmScMatchIsMatchIndex(loc,pitch);
      vp->flags             =  idx==cmInvalidIdx ? 0            : kSmMatchFl;
      vp->scEvtIdx          =  idx==cmInvalidIdx ? cmInvalidIdx : loc->evtV[idx].scEvtIdx;
      unsigned        cost  =  cmIsFlag(vp->flags,kSmMatchFl) ? 0 : 1;
      vp->v[kSmSubIdx]      = _cmScMatchMin(p,i-1,j-1, rn, cn) + cost;
      vp->v[kSmDelIdx]      = _cmScMatchMin(p,i-1,j  , rn, cn) + 1;
      vp->v[kSmInsIdx]      = _cmScMatchMin(p,i,  j-1, rn, cn) + 1;
      vp->v[kSmMinIdx]      = cmMin( vp->v[kSmSubIdx], cmMin(vp->v[kSmDelIdx],vp->v[kSmInsIdx]));
      vp->flags            |= _cmScMatchIsTrans(p,midiV,vp,bsi,i-1,j-1,rn,cn) ? kSmTransFl : 0;
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

void _cmScMatchPathPush( cmScMatch* r, unsigned code, unsigned ri, unsigned ci, unsigned flags, unsigned scEvtIdx )
{
  assert(r->p_avl != NULL );

  cmScMatchPath_t* p = r->p_avl;
  r->p_avl = r->p_avl->next;

  p->code    = code;
  p->ri      = ri;
  p->ci      = ci;
  p->flags   = code==kSmSubIdx && cmIsFlag(flags,kSmMatchFl) ? kSmMatchFl  : 0;
  p->flags  |= cmIsFlag(flags,kSmTransFl) ? kSmTransFl : 0;
  p->scEvtIdx= scEvtIdx;
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
  assert( ep!=NULL );
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
      r->p_opt[i].scEvtIdx= cp->scEvtIdx;
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
      _cmScMatchPathPush(r,m,i,j,vp->flags,vp->scEvtIdx);

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


cmRC_t cmScMatchExec( cmScMatch* p, unsigned locIdx, unsigned locN, const cmScMatchMidi_t* midiV, unsigned midiN, double min_cost )
{
  cmRC_t   rc;
  unsigned rn = midiN + 1;
  unsigned cn = locN  + 1;

  // set the DP matrix default values
  if((rc = _cmScMatchInitMtx(p, rn, cn )) != cmOkRC )
    return rc;

  // _cmScMatchCalcMtx() returns false if the score window exceeds the length of the score
  if(!_cmScMatchCalcMtx(p,locIdx, midiV, rn, cn) )
    return cmEofRC;

  //_cmScMatchPrintMtx(p,rn,cn);
  
  // locate the path through the DP matrix with the lowest edit distance (cost)
  p->opt_cost =  _cmScMatchAlign(p, rn, cn, min_cost);

  return rc;
}

// Traverse the least cost path and:
// 1) Return, esi, the score location index of the last MIDI note
// which has a positive match with the score and assign
// the internal score index to cp->locIdx.
//
// 2) Set cmScAlignPath_t.locIdx - index into p->loc[] associated
// with each path element that is a 'substitute' or an 'insert'.
//
// 3) Set *missCnPtr: the count of trailing non-positive matches.
//
// i_opt is index into p->loc[] of p->p_opt. 
unsigned cmScMatchDoSync( cmScMatch* p, unsigned i_opt, cmScMatchMidi_t* midiBuf, unsigned midiN, unsigned* missCntPtr )
{
  cmScMatchPath_t* cp      = p->p_opt;
  unsigned         missCnt = 0;
  unsigned         esi     = cmInvalidIdx;
  unsigned         i;

  for(i=0; cp!=NULL; cp=cp->next)
  {
    // there is no MIDI note associated with 'inserts'
    if( cp->code != kSmInsIdx )
    {
      assert( cp->ri > 0 );
      midiBuf[ cp->ri-1 ].locIdx = cmInvalidIdx;
    }

    switch( cp->code )
    {
      case kSmSubIdx:
        midiBuf[ cp->ri-1 ].locIdx   = i_opt + i;
        midiBuf[ cp->ri-1 ].scEvtIdx = cp->scEvtIdx;

        if( cmIsFlag(cp->flags,kSmMatchFl) )
        {
          esi     = i_opt + i;
          missCnt = 0;
        }
        else
        {
          ++missCnt;
        }
        // fall through

      case kSmInsIdx:
        cp->locIdx = i_opt + i;
        ++i;
        break;

      case kSmDelIdx:
        cp->locIdx = cmInvalidIdx;
        ++missCnt;
        break;
    }
  }

  if( missCntPtr != NULL )
    *missCntPtr = missCnt;

  return esi;
}

void _cmScMatchMidiEvtFlags( cmScMatch* p, const cmScMatchLoc_t* lp, unsigned evtIdx, char* s, unsigned sn )
{
  const cmScoreLoc_t* slp = cmScoreLoc(p->scH,lp->scLocIdx);

  assert( evtIdx < slp->evtCnt );

  const cmScoreEvt_t* ep = slp->evtArray[evtIdx];
  unsigned            i  = 0;

  s[0] = 0;

  if( cmIsFlag(ep->flags,kEvenScFl) )
    s[i++] = 'e';

  if( cmIsFlag(ep->flags,kTempoScFl) )
    s[i++] = 't';

  if( cmIsFlag(ep->flags,kDynScFl) )
    s[i++] = 'd';

  if( cmIsFlag(ep->flags,kGraceScFl) )
    s[i++] = 'g';

  s[i++] = 0;

  assert( i <= sn );
  
}

void _cmScMatchPrintPath( cmScMatch* p, cmScMatchPath_t* cp, unsigned bsi, const cmScMatchMidi_t* midiV )
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

      printf("%4i%4s ",bsi+i," ");
      ++i;
    }
    else
      printf("%4s%4s "," "," ");
  }

  printf("\n");

  // print the score notes
  for(i=polyN; i>0; --i)
  {
    printf("%3i: ",i);
    for(pp=cp; pp!=NULL; pp=pp->next)
    {

      if( pp->code!=kSmDelIdx )
      {
        int locIdx = bsi + pp->ci - 1;
        assert(0 <= locIdx && locIdx <= p->locN);
        cmScMatchLoc_t* lp = p->loc + locIdx;

        if( lp->evtCnt >= i )
        {
          unsigned sn = 6;
          char s[sn];
          _cmScMatchMidiEvtFlags(p,lp,i-1,s,sn );
          printf("%4s%-4s ",cmMidiToSciPitch(lp->evtV[i-1].pitch,NULL,0),s);
        }
        else
          printf("%4s%4s "," "," ");
      }
      else
        printf("%4s%4s ", (pp->code==kSmDelIdx? "-" : " ")," ");

      /*
      int locIdx = bsi + pp->ci - 1;
      assert(0 <= locIdx && locIdx <= p->locN);
      cmScMatchLoc_t* lp = p->loc + locIdx;
      if( pp->code!=kSmDelIdx && lp->evtCnt >= i )
        printf("%4s ",cmMidiToSciPitch(lp->evtV[i-1].pitch,NULL,0));
      else
        printf("%4s ", pp->code==kSmDelIdx? "-" : " ");
      */
    }
    printf("\n");
  }

  printf("mid: ");

  // print the MIDI buffer
  for(pp=cp; pp!=NULL; pp=pp->next)
  {
    if( pp->code!=kSmInsIdx )
      printf("%4s%4s ",cmMidiToSciPitch(midiV[pp->ri-1].pitch,NULL,0)," ");
    else
      printf("%4s%4s ",pp->code==kSmInsIdx?"-":" "," ");
  }

  printf("\nvel: ");

  // print the MIDI velocity
  for(pp=cp; pp!=NULL; pp=pp->next)
  {
    if( pp->code!=kSmInsIdx )
      printf("%4i%4s ",midiV[pp->ri-1].vel," ");
    else
      printf("%4s%4s ",pp->code==kSmInsIdx?"-":" "," ");
  }

  printf("\nmni: ");

  // print the MIDI buffer index (mni)
  for(pp=cp; pp!=NULL; pp=pp->next)
  {
    if( pp->code!=kSmInsIdx )
      printf("%4i%4s ",midiV[pp->ri-1].mni," ");
    else
      printf("%4s%4s ",pp->code==kSmInsIdx?"-":" "," ");
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

    printf("%4c%4s ",c," ");
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

    printf("%4s%4s ",s," ");
  }

  printf("\nscl: ");

  // print the stored location index
  for(pp=cp; pp!=NULL; pp=pp->next)
  {
    if( pp->locIdx == cmInvalidIdx )
      printf("%4s%4s "," "," ");
    else
      printf("%4i%4s ",p->loc[pp->locIdx].scLocIdx," ");
  }
  
  printf("\nbar: ");

  // print the stored location index
  for(pp=cp; pp!=NULL; pp=pp->next)
  {
    if( pp->locIdx==cmInvalidIdx || pp->scEvtIdx==cmInvalidIdx )
      printf("%4s%4s "," "," ");
    else
    {
      const cmScoreEvt_t* ep = cmScoreEvt(p->scH, pp->scEvtIdx );
      printf("%4i%4s ",ep->barNumb," ");
    }
  }


  printf("\nsec: ");

  // print seconds
  unsigned begSmpIdx = cmInvalidIdx;
  for(pp=cp; pp!=NULL; pp=pp->next)
  {
    if( pp->code!=kSmInsIdx )
    {
      if( begSmpIdx == cmInvalidIdx )
        begSmpIdx = midiV[pp->ri-1].smpIdx;

      printf("%2.2f%4s ", (double)(midiV[pp->ri-1].smpIdx - begSmpIdx)/96000.0," ");
      
    }
    else
      printf("%4s%4s ",pp->code==kSmInsIdx?"-":" "," ");


  }

  
  printf("\n\n");

}


//=======================================================================================================================
cmScMatcher* cmScMatcherAlloc( cmCtx* c, cmScMatcher* p, double srate, cmScH_t scH, unsigned scWndN, unsigned midiWndN, cmScMatcherCb_t cbFunc, void* cbArg )
{
  cmScMatcher* op = cmObjAlloc(cmScMatcher,c,p);

  if( op != NULL )
    op->mp = cmScMatchAlloc(c,NULL,cmScNullHandle,0,0);

  if( srate != 0 )
  {
    if( cmScMatcherInit(op,srate,scH,scWndN,midiWndN,cbFunc,cbArg) != cmOkRC )
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

cmRC_t cmScMatcherInit(  cmScMatcher* p, double srate, cmScH_t scH, unsigned scWndN, unsigned midiWndN, cmScMatcherCb_t cbFunc, void* cbArg )
{
  cmRC_t rc;
  if((rc = cmScMatcherFinal(p)) != cmOkRC )
    return rc;

  if( midiWndN > scWndN )
    return cmCtxRtCondition( &p->obj, cmInvalidArgRC, "The score alignment MIDI event buffer length (%i) must be less than the score window length (%i).",midiWndN,scWndN); 

  if(( rc = cmScMatchInit(p->mp,scH,scWndN,midiWndN)) != cmOkRC )
    return rc;

  p->cbFunc     = cbFunc;
  p->cbArg      = cbArg;
  p->mn         = midiWndN;
  p->midiBuf    = cmMemResizeZ(cmScMatchMidi_t,p->midiBuf,p->mn);
  p->initHopCnt = 50;
  p->stepCnt    = 3;
  p->maxMissCnt = p->stepCnt+1;
  p->rn         = 2 * cmScoreEvtCount(scH);
  p->res        = cmMemResizeZ(cmScMatcherResult_t,p->res,p->rn);
  p->printFl    = false;

  cmScMatcherReset(p,0);

  return rc;
}

cmRC_t cmScMatcherFinal( cmScMatcher* p )
{
  return cmScMatchFinal(p->mp);
}

cmRC_t cmScMatcherReset( cmScMatcher* p, unsigned scLocIdx )
{
  p->mbi           = p->mp->mmn;
  p->mni           = 0;
  p->begSyncLocIdx = cmInvalidIdx;
  p->s_opt         = DBL_MAX;
  p->missCnt       = 0;
  p->scanCnt       = 0;
  p->ri            = 0;
  p->eli           = cmInvalidIdx;
  p->ili           = 0;

  // convert scLocIdx to an index into p->mp->loc[]
  unsigned i = 0;
  while(1)
  {
    for(i=0; i<p->mp->locN; ++i)
      if( p->mp->loc[i].scLocIdx == scLocIdx )
      {
        p->ili = i;
        break;
      }

    assert(p->mp->locN>0);
    if( i!=p->mp->locN || scLocIdx==p->mp->loc[p->mp->locN-1].scLocIdx)
      break;

    scLocIdx += 1;
  }

  if( i==p->mp->locN)
    return cmCtxRtCondition( &p->obj, cmSubSysFailRC, "Score matcher reset failed."); 


  return cmOkRC;
}

bool cmScMatcherInputMidi(  cmScMatcher* p, unsigned smpIdx, unsigned muid, unsigned status, cmMidiByte_t d0, cmMidiByte_t d1 )
{
  if( (status&0xf0) != kNoteOnMdId)
    return false;

  if( d1 == 0 )
    return false;

  unsigned mi = p->mn-1;

  //printf("%3i %4s\n",p->mni,cmMidiToSciPitch(d0,NULL,0));

  // shift the new MIDI event onto the end of the MIDI buffer
  memmove(p->midiBuf, p->midiBuf+1, sizeof(cmScMatchMidi_t)*mi);
  p->midiBuf[mi].locIdx   = cmInvalidIdx;
  p->midiBuf[mi].scEvtIdx = cmInvalidIdx;
  p->midiBuf[mi].mni      = p->mni++;
  p->midiBuf[mi].smpIdx   = smpIdx;
  p->midiBuf[mi].muid     = muid;
  p->midiBuf[mi].pitch    = d0;
  p->midiBuf[mi].vel      = d1;
  if( p->mbi > 0 )
    --p->mbi;

  return true;
}

void _cmScMatcherStoreResult( cmScMatcher* p, unsigned locIdx, unsigned scEvtIdx, unsigned flags, const cmScMatchMidi_t* mp )
{
  // don't store missed score note results
  assert( mp != NULL );
  bool                  matchFl = cmIsFlag(flags,kSmMatchFl);
  bool                  tpFl    = locIdx!=cmInvalidIdx && matchFl;
  bool                  fpFl    = locIdx==cmInvalidIdx || matchFl==false;
  cmScMatcherResult_t * rp      = NULL;
  unsigned              i;
  cmScMatcherResult_t   r;

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
    if( p->ri >= p->rn )
    {
      rp = &r;
      memset(rp,0,sizeof(r));
    }
    else
    {
      rp = p->res + p->ri;
      ++p->ri;
    }
  }

  rp->locIdx   = locIdx;
  rp->scEvtIdx = scEvtIdx;
  rp->mni      = mp->mni;
  rp->smpIdx   = mp->smpIdx;
  rp->pitch    = mp->pitch;
  rp->vel      = mp->vel;
  rp->flags    = flags | (tpFl ? kSmTruePosFl : 0) | (fpFl ? kSmFalsePosFl : 0);
  
  if( p->cbFunc != NULL )
    p->cbFunc(p,p->cbArg,rp);

}

void cmScMatcherPrintPath( cmScMatcher* p )
{
  _cmScMatchPrintPath(p->mp, p->mp->p_opt, p->begSyncLocIdx, p->midiBuf );
}

unsigned   cmScMatcherScan( cmScMatcher* p, unsigned bli, unsigned hopCnt )
{
  assert( p->mp != NULL && p->mp->mmn > 0 );

  unsigned i_opt = cmInvalidIdx;
  double   s_opt = DBL_MAX;
  cmRC_t   rc    = cmOkRC;
  unsigned i;

  // initialize the internal values set by this function
  p->missCnt = 0;
  p->eli     = cmInvalidIdx;
  p->s_opt   = DBL_MAX;
  
  // if the MIDI buf is not full
  if( p->mbi != 0 )
    return cmInvalidIdx;

  // calc the edit distance from pitchV[] to a sliding score window
  for(i=0; rc==cmOkRC && (hopCnt==cmInvalidCnt || i<hopCnt); ++i)
  {      
    rc = cmScMatchExec(p->mp, bli + i, p->mp->msn, p->midiBuf, p->mp->mmn, s_opt );

    switch(rc)
    {
      case cmOkRC:  // normal result 
        if( p->mp->opt_cost < s_opt )
        {
          s_opt = p->mp->opt_cost;
          i_opt = bli + i;
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

  // set the locIdx field in midiBuf[], trailing miss count and
  // return the latest positive-match locIdx
  p->eli = cmScMatchDoSync(p->mp,i_opt,p->midiBuf,p->mp->mmn,&p->missCnt);

  // if no positive matches were found
  if( p->eli == cmInvalidIdx )
    i_opt = cmInvalidIdx;
  else
  {
    cmScMatchPath_t* cp;

    // record result
    for(cp=p->mp->p_opt; cp!=NULL; cp=cp->next)
      if( cp->code != kSmInsIdx )
        _cmScMatcherStoreResult(p, cp->locIdx, cp->scEvtIdx, cp->flags, p->midiBuf + cp->ri - 1);
  }

  return i_opt;

}

cmRC_t     cmScMatcherStep(  cmScMatcher* p )
{
  int      i;
  unsigned pitch  = p->midiBuf[ p->mn-1 ].pitch;
  unsigned locIdx = cmInvalidIdx;
  unsigned pidx   = cmInvalidIdx;

  // the tracker must be sync'd to step
  if( p->eli == cmInvalidIdx )
    return cmCtxRtCondition( &p->obj, cmInvalidArgRC, "The p->eli value must be valid to perform a step operation."); 

  // if the end of the score has been reached
  if( p->eli + 1 >= p->mp->locN )
    return cmEofRC;
    
  // attempt to match to next location first
  if( (pidx = _cmScMatchIsMatchIndex(p->mp->loc + p->eli + 1, pitch)) != cmInvalidIdx )
  {
    locIdx = p->eli + 1;
  }
  else
  {
    // 
    for(i=2; i<p->stepCnt; ++i)
    {
      // go forward 
      if( p->eli+i < p->mp->locN && (pidx=_cmScMatchIsMatchIndex(p->mp->loc + p->eli + i, pitch))!=cmInvalidIdx )
      {
        locIdx = p->eli + i;
        break;
      }

      // go backward
      if( p->eli >= (i-1)  && (pidx=_cmScMatchIsMatchIndex(p->mp->loc + p->eli - (i-1), pitch))!=cmInvalidIdx )
      {
        locIdx = p->eli - (i-1);
        break;
      }
    }
  }

  unsigned scEvtIdx = locIdx==cmInvalidIdx ? cmInvalidIdx : p->mp->loc[locIdx].evtV[pidx].scEvtIdx;

  p->midiBuf[ p->mn-1 ].locIdx   = locIdx;
  p->midiBuf[ p->mn-1 ].scEvtIdx = scEvtIdx;

  if( locIdx == cmInvalidIdx )
    ++p->missCnt;
  else
  {
    p->missCnt = 0;
    p->eli     = locIdx;
  }

  // store the result
  _cmScMatcherStoreResult(p, locIdx,  scEvtIdx, locIdx!=cmInvalidIdx ? kSmMatchFl : 0, p->midiBuf + p->mn - 1);

  if( p->missCnt >= p->maxMissCnt )
  {
    unsigned begScanLocIdx = p->eli > p->mn ? p->eli - p->mn : 0;
    p->s_opt               = DBL_MAX;
    unsigned bli           = cmScMatcherScan(p,begScanLocIdx,p->mn*2);
    ++p->scanCnt;

    // if the scan failed find a match
    if( bli == cmInvalidIdx )
      return cmCtxRtCondition( &p->obj, cmSubSysFailRC, "Scan resync. failed."); 
  }

  return cmOkRC;
}

cmRC_t     cmScMatcherExec(  cmScMatcher* p, unsigned smpIdx, unsigned muid, unsigned status, cmMidiByte_t d0, cmMidiByte_t d1, unsigned* scLocIdxPtr )
{
  bool     fl  = p->mbi > 0;
  cmRC_t   rc  = cmOkRC;
  unsigned org_eli = p->eli;

  if( scLocIdxPtr != NULL )
    *scLocIdxPtr = cmInvalidIdx;

  // update the MIDI buffer with the incoming note
  if( cmScMatcherInputMidi(p,smpIdx,muid,status,d0,d1) == false )
    return rc;

  // if the MIDI buffer transitioned to full then perform an initial scan sync.
  if( fl && p->mbi == 0 )
  {
    if( (p->begSyncLocIdx = cmScMatcherScan(p,p->ili,p->initHopCnt)) == cmInvalidIdx )
    {
      rc = cmInvalidArgRC; // signal init. scan sync. fail
    }
    else
    {
      //cmScMatcherPrintPath(p);
    }
  }
  else
  {
    // if the MIDI buffer is full then perform a step sync.
    if( !fl && p->mbi == 0 ) 
      rc = cmScMatcherStep(p);
  }

  // if we lost sync 
  if( p->eli == cmInvalidIdx )
  {
    // IF WE LOST SYNC THEN WE BETTER DO SOMETHING - LIKE INCREASE THE SCAN HOPS
    // ON THE NEXT EVENT.
    p->eli = org_eli;
  }
  else
  {
    if( scLocIdxPtr!=NULL && p->eli != org_eli )
    {
      //printf("LOC:%i bar:%i\n",p->eli,p->mp->loc[p->eli].barNumb);
      *scLocIdxPtr = p->mp->loc[p->eli].scLocIdx;
    }
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
  unsigned                     barNumb;
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

  // get first/last scLocIdx from res[] - this is the range of
  // score events that the score matcher has identified
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

  // allocate an array of 'aan' print records
  cmScMatcherPrint_t* a  = cmMemAllocZ(cmScMatcherPrint_t,aan);

  // fill the cmScMatcherPrint_t array with note and bar events from the score
  for(i=bsli; i<=esli; ++i)
  {
    unsigned      scLocIdx = i;
    cmScoreLoc_t* lp       = cmScoreLoc(p->mp->scH, scLocIdx );

    // for each score event which occurs at this location
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
      pp->barNumb  = ep->barNumb;
    }

  }

  //
  // a[an] now contains a record for each note and bar event in the
  // time range associated with the score matcher's result array.
  //


  // for each result record
  for(i=0; i<p->ri; ++i)
  {
    cmScMatcherResult_t* rp = p->res + i;
    
    // if this result recd matched a score event
    if( cmIsFlag(rp->flags,kSmTruePosFl) )
    {
      // locate the matching score event in a[an]
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
      unsigned            d_min    = 0;
      cmScMatcherPrint_t* dp       = NULL;
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
  
  printf("sloc bar  mni  ptch flag\n");
  printf("---- ---- ---- ---- ----\n");

  for(i=0; i<an; ++i)
  {
    printf("%4i %4i %4i %4s %c%c%c\n",a[i].scLocIdx,a[i].barNumb,a[i].mni,
      cmIsFlag(a[i].flags,kSmBarFl)   ? "|" : cmMidiToSciPitch(a[i].pitch,NULL,0),
      cmIsFlag(a[i].flags,kSmNoteFl)  ? 'n' : ' ',
      cmIsFlag(a[i].flags,kSmMatchFl) ? 'm' : (cmIsFlag(a[i].flags,kSmTransFl) ? 't' : ' '),
      cmIsFlag(a[i].flags,kSmFalsePosFl) ? '*' : ' '
           );
  }
  

  cmMemFree(a);

}


//=======================================================================================================================

cmScMeas* cmScMeasAlloc( cmCtx* c, cmScMeas* p, cmScH_t scH, double srate, const unsigned* dynRefArray, unsigned dynRefCnt )
{
  cmScMeas* op = cmObjAlloc(cmScMeas,c,p);

  op->mp = cmScMatchAlloc( c, NULL, cmScNullHandle, 0, 0 );

  if( cmScoreIsValid(scH) )
    if( cmScMeasInit(op,scH,srate,dynRefArray,dynRefCnt) != cmOkRC )
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
  cmMemFree(p->dynRef);
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

int _cmScMeasSortFunc( const void* p0, const void* p1 )
{
  const cmScMeasSet_t* s0 = (const cmScMeasSet_t*)p0;
  const cmScMeasSet_t* s1 = (const cmScMeasSet_t*)p1;

  return s0->esli - s1->esli;
}


cmRC_t    cmScMeasInit(  cmScMeas* p, cmScH_t scH, double srate, const unsigned* dynRefArray, unsigned dynRefCnt )
{
  cmRC_t   rc;
  unsigned i,j;
  unsigned si;
  unsigned maxScWndN = 0;

  if((rc = cmScMeasFinal(p)) != cmOkRC )
    return rc;

  p->mii     = 0;
  p->mn      = 2 * cmScoreEvtCount(scH);
  p->midiBuf = cmMemResizeZ(cmScMatchMidi_t,p->midiBuf,p->mn);
  p->sn      = cmScoreSetCount(scH);
  p->set     = cmMemResizeZ(cmScMeasSet_t,p->set,p->sn);
  p->dynRef  = cmMemResizeZ(unsigned,p->dynRef,dynRefCnt);
  p->dn      = dynRefCnt;
  p->srate   = srate;

  memcpy(p->dynRef,dynRefArray,sizeof(dynRefArray[0])*dynRefCnt);

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

      msp->sp   = sp;
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

  // sort set[] on cmScMeasSet_t.esli
  qsort(p->set, p->sn, sizeof(cmScMeasSet_t), _cmScMeasSortFunc );

  //_cmScMeasPrint(p);

  cmScMeasReset(p);

  return rc;
}

cmRC_t    cmScMeasFinal( cmScMeas* p )
{ return cmScMatchFinal(p->mp); }

cmRC_t    cmScMeasReset( cmScMeas* p )
{
  cmRC_t rc = cmOkRC;
  p->mii    = 0;
  p->nsi    = cmInvalidIdx;
  p->vsi    = cmInvalidIdx;
  p->nsli   = cmInvalidIdx;

  unsigned i;
  for(i=0; i<p->sn; ++i)
  {
    p->set[i].value = DBL_MAX;
    p->set[i].tempo = 0;
    p->set[i].match_cost = 0;
  }
  return rc;
}

typedef struct
{
  unsigned scLocIdx; // score loc index
  double   frac;     // score based fraction of beat
  unsigned smpIdx;   // time of assoc'd MIDI event
  unsigned cnt;      // 
  double   val;      //
} _cmScMeasTimeEle_t;

typedef struct
{
  unsigned setN;     // set length
  unsigned midiN;    // count of MIDI events to match to score
  unsigned alignN;   // count of score events in the alignment (<= setN)
  unsigned matchN;   // count of positive matches
  double   tempo;
  double   value;
} _cmScMeasResult_t;

double _cmScMeasCalcTempo( const _cmScMeasTimeEle_t* b, unsigned bn, double srate )
{
  assert( bn >= 2 );
  assert( b[bn-1].smpIdx != cmInvalidIdx );
  assert( b[0].smpIdx    != cmInvalidIdx );

  double   durSmpCnt = b[bn-1].smpIdx - b[0].smpIdx;
  double   beats     = 0;
  unsigned i;

  for(i=0; i<bn; ++i)
    beats += b[i].frac;

  assert(beats != 0);

  return beats / (durSmpCnt / (srate * 60.0));
}


// Note: On successful completion (return!=0) the first 
// and last record returned in c[cn] will be matched records.
unsigned _cmScMeasTimeAlign( cmScMeas* p, cmScMeasSet_t* sp, cmScMatchMidi_t* m, unsigned mn, _cmScMeasTimeEle_t* c, unsigned cn, _cmScMeasResult_t* rp )
{
  int                 i,j,k;
  int                 an      = sp->sp->eleCnt;
  _cmScMeasTimeEle_t* b       = NULL;
  int                 bn      = 0;
  bool                tempoFl = false;
  unsigned            matchN  = 0;
  assert( an!=0);

  // alloc a 'score set' element array
  _cmScMeasTimeEle_t a[an];

  // get the scLocIdx of each set element from the score
  for(i=0,j=0; i<an; ++i)
    if( i==0 || sp->sp->eleArray[i-1]->locIdx != sp->sp->eleArray[i]->locIdx )
    {
      assert( sp->sp->eleArray[i]->locIdx != cmInvalidIdx );
      a[j].scLocIdx = sp->sp->eleArray[i]->locIdx;
      a[j].frac     = sp->sp->eleArray[i]->frac;
      a[j].smpIdx   = cmInvalidIdx;
      a[j].cnt      = 0;
      ++j;
    }
  
  an = j; // set the count of unique score locations (new length of a[])

  // collect the 'smpIdx' for each MIDI event which matches a set element
  for(i=0; i<mn; ++i)
    if( m[i].locIdx != cmInvalidIdx )
    {
      for(j=0; j<an; ++j)
        if( p->mp->loc[m[i].locIdx].scLocIdx == a[j].scLocIdx )
        {
          a[j].smpIdx += m[i].smpIdx;
          a[j].cnt    += 1;

          if( a[j].cnt == 1 )
            matchN += 1;  // only cnt one match per sc loc.
          break;
        }
    }

  // remove leading missing values
  for(i=0; i<an; ++i)
    if( a[i].smpIdx != cmInvalidIdx )
    {
      b = a + i;
      bn = an - i;
      break;
    }

  // remove trailing missing values
  for(i=bn-1; i>=0; --i,--bn)
    if( b[i].smpIdx != cmInvalidIdx )
      break;

  // can't measure evenness against less than 2 values
  if( bn < 2 )
  {
    return 0;
  }

  assert(b[0].smpIdx != cmInvalidIdx && b[bn-1].smpIdx != cmInvalidIdx);

  // calc avg. smpIdx, insert missing values, and convert b[].smpIdx to delta smp index
  for(i=0,j=0; i<bn; ++i)
  {
    if( b[i].cnt > 1 )
      b[i].smpIdx /= b[i].cnt;

    if( b[i].smpIdx == cmInvalidIdx )
      ++j; // incr missing value count
    else
    {
      if( i > 0 )
      {
        // fill in missing values
        ++j;
        unsigned d = (b[i].smpIdx - b[i-j].smpIdx)/j;
        for(k=0; k<j; ++k)
          b[i-j+k].val = d;          
      }
      j=0;
    }
        
    if( b[i].frac != 0 )
      tempoFl = true;
  }

  rp->setN   = an;
  rp->midiN  = mn;
  rp->alignN = bn;
  rp->matchN = matchN;
  rp->tempo  = 0;
  rp->value  = 0;

  // calculate tempo
  if( tempoFl )
    rp->tempo = _cmScMeasCalcTempo(b,bn,p->srate);

  assert(bn<=cn);

  // TODO: this copy should be eliminated
  // copy to output
  for(i=0; i<bn && i<cn; ++i)
    c[i] = b[i];
  
  

  return bn;
}

double _cmScMeasEven( cmScMeas* p, cmScMeasSet_t* sp, cmScMatchMidi_t* m, unsigned mn, _cmScMeasResult_t* rp )
{
  unsigned           bn = sp->sp->eleCnt;
  _cmScMeasTimeEle_t b[bn]; 
  unsigned           i;

  if((bn = _cmScMeasTimeAlign(p,sp,m,mn,b,bn,rp)) == 0 )
    return DBL_MAX;

  // calc avg. delta time
  double d_avg  = 0;
  for(i=0; i<bn-1; ++i)
    d_avg  += b[i].val;

  d_avg /= (bn-1);

  // calc std-dev of delta time
  double d_sd  = 0;    
  for(i=0; i<bn-1; ++i)
    d_sd  += (b[i].val-d_avg) * (b[i].val-d_avg);
  
  // if there is no deviation then we can't compute a z-score
  // (this will happen if we fill in all the missing values based on 2 values)
  if( d_sd == 0 )
    return 1.0;

  d_sd = sqrt(d_sd/(bn-1));

  // calc avg. z-score
  double z  = 0;
  for(i=0; i<bn-1; ++i)
    z  += fabs(b[i].val - d_avg)/d_sd;

  double val = z / (bn-1);

  rp->value = val;

  return val;
}

// return Tempo estimation in BPM
double _cmScMeasTempo( cmScMeas* p, cmScMeasSet_t* sp, cmScMatchMidi_t* m, unsigned mn, _cmScMeasResult_t* rp )
{
  unsigned           bn    = sp->sp->eleCnt;
  _cmScMeasTimeEle_t b[bn]; 

  if((bn= _cmScMeasTimeAlign(p,sp,m,mn,b,bn,rp)) == 0 )
    return DBL_MAX;
 
  return rp->tempo;
}


double _cmScMeasDyn( cmScMeas* p, cmScMeasSet_t* sp, cmScMatchMidi_t* m, unsigned mn, _cmScMeasResult_t* rp )
{
  typedef struct
  {
    unsigned scEvtIdx;
    unsigned vel;
    double   val;
  } ele_t;

  int      i,j;
  int      n      = sp->sp->eleCnt;
  double   vv     = 0;
  unsigned vn     = 0;
  unsigned matchN = 0;
  unsigned alignN = 0;

  assert( n!=0);

  ele_t a[n];

  // get the scEvtIdx of each set element
  for(i=0; i<n; ++i)
  {
    cmScoreEvt_t* ep = cmScoreEvt( p->mp->scH, sp->sp->eleArray[i]->index );
    assert( ep != NULL );

    a[i].scEvtIdx = sp->sp->eleArray[i]->index;
    a[i].vel      = 0;
    a[i].val      = ep->dynVal;
  }    

  // set the performed vel. of each note in the set
  // (if a note was not played it's a[].vel is left at 0)
  for(i=0; i<mn; ++i)
    if( m[i].scEvtIdx != cmInvalidIdx )
    {
      alignN += 1;

      for(j=0; j<n; ++j)
        if( m[i].scEvtIdx == a[j].scEvtIdx )
        {
          matchN  += 1;
          a[j].vel = m[i].vel;
          break;
        }
    }
  // assign a dynamics category to each note in the set
  for(i=0; i<n; ++i)
    if( a[i].vel > 0 )
    {
      unsigned mnv = 0; // lower bound for first dyn's category
      for(j=0; j<p->dn; ++j)
      {
        if( mnv <= a[i].vel && a[i].vel < p->dynRef[j] )
        {
          // accum. the diff. between the ref. and performed dyn. category
          vv += fabs(a[i].val - j); 
          vn += 1;
          break;
        }

        mnv = p->dynRef[j];
      }
      assert(j<p->dn);
    }

  rp->setN   = n;
  rp->midiN  = mn;
  rp->alignN = alignN;
  rp->matchN = matchN;
  rp->tempo  = 0;
  rp->value  = vn == 0 ? DBL_MAX : vv/vn;

  
  return rp->value;
}


unsigned MEAS_MATCH_CNT = 0;

void _cmScMeasPrintResult( cmScMeas* p,  cmScMeasSet_t* sp, _cmScMeasResult_t* rp, unsigned bli, const cmScMatchMidi_t* mb )
{
  const char* label = "<none>";

  switch( sp->sp->varId )
  {
    case kEvenVarScId:  
      label = "even";  
      break;

    case kDynVarScId:   
      label = "dyn";   
      break;

    case kTempoVarScId: 
      label = "tempo"; 
      break;
  }

  const cmChar_t* msg = "";
  if( rp->value == DBL_MAX )
  {
    msg       = "Measure FAILED.";
    sp->value = 0;
  }
  
  printf("%i set:%i %s bsli:%i esli:%i [set:%i match:%i] cost:%f val:%f %s",MEAS_MATCH_CNT, p->nsi, label, sp->bsli, sp->esli, rp->setN, rp->matchN, p->mp->opt_cost, sp->value, msg);

  if( rp->tempo != 0 )
    printf(" tempo:%f ",rp->tempo);

  printf("\n");

  _cmScMatchPrintPath(p->mp, p->mp->p_opt, bli, mb );

}

void _cmScMeasCalcVal( cmScMeas* p, cmScMeasSet_t* sp, int n_mii )
{
  unsigned mn = 0;
  int      i,k = cmInvalidIdx;

  if( n_mii == 0 )
    return;

  // Determine the count of MIDI notes to match to the set score
  // by searching from the MIDI note just recieved (midiBuf[n_mii]
  // back toward the beginning until a MIDI event which occurs just
  //  prior to the set's begScLocIdx.
  for(i=n_mii; i>=0; --i)
  {
    if( p->midiBuf[i].locIdx != cmInvalidIdx )
    {
      k = i;
      unsigned scLocIdx = p->mp->loc[ p->midiBuf[i].locIdx ].scLocIdx;
      if( scLocIdx < sp->bsli )
        break;
    }
  }

  assert(k != cmInvalidIdx);
  mn = n_mii - k + 1;
  i  = k;

  assert(i>=0);
  assert(mn>0);

  // Create a copy of the the MIDI buffer to prevent the
  // p->midiBuf[].locIdx from being overwritten by cmScMatchDoSync().
  cmScMatchMidi_t mb[ mn ];
  unsigned        j;
  for(j=0; j<mn; ++j)
  {
    mb[j]        = p->midiBuf[i+j];
    mb[j].locIdx = cmInvalidIdx;
  }
  
  // In general the first and last MIDI event should be assigned
  // to a score location - it's possible however that no MIDI
  // event's prior to the one at p->midiBuf[n_mii] were assigned.
  assert( (i==0 || p->midiBuf[i].locIdx!=cmInvalidIdx) && p->midiBuf[i+mn-1].locIdx != cmInvalidIdx);

  unsigned          l0i      = cmMin(p->midiBuf[i].locIdx,p->midiBuf[i+mn-1].locIdx);
  unsigned          l1i      = cmMax(p->midiBuf[i].locIdx,p->midiBuf[i+mn-1].locIdx);

  unsigned          bli      = l0i;
  unsigned          ln       = l1i - bli + 1;
  double            min_cost = DBL_MAX;
  _cmScMeasResult_t r;
  memset(&r,0,sizeof(r));

  // match MIDI to score
  if( cmScMatchExec(p->mp, bli, ln, mb, mn, min_cost ) != cmOkRC )
    return;

  // sync the score and MIDI based on the match information
  if( cmScMatchDoSync(p->mp, bli, mb, mn, NULL ) == cmInvalidIdx )
    return;

  if( p->mp->opt_cost != DBL_MAX )
    sp->match_cost = p->mp->opt_cost / sp->sp->eleCnt;

  switch( sp->sp->varId )
  {
    case kEvenVarScId:  
      sp->value = _cmScMeasEven(p, sp, mb, mn, &r );
      break;

    case kDynVarScId:   
      sp->value = _cmScMeasDyn(p, sp, mb, mn, &r );
      break;

    case kTempoVarScId: 
      sp->value = _cmScMeasTempo(p, sp, mb, mn, &r );
      break;

    default:
      { assert(0); }
  }

  sp->tempo = r.tempo;

  // print the result
  //_cmScMeasPrintResult(p, sp, &r, bli, mb );

  MEAS_MATCH_CNT++;
}

cmRC_t cmScMeasExec( cmScMeas* p, unsigned mni, unsigned locIdx, unsigned scEvtIdx, unsigned flags, unsigned smpIdx, unsigned pitch, unsigned vel )
{
  cmRC_t rc = cmOkRC;

  // if the midi buffer is full
  if( p->mii >= p->mn )
    return cmCtxRtCondition( &p->obj, cmEofRC, "The MIDI buffer is full."); 

  int n_mii = cmInvalidIdx;

  // locate the MIDI event assoc'd with 'mni'  ...
  if( p->mii>0 && mni <= p->midiBuf[p->mii-1].mni )
  {
    if( locIdx != cmInvalidIdx )
    {
      for(n_mii=p->mii-1; n_mii>=0; --n_mii)
        if( p->midiBuf[n_mii].mni == mni )
          break;

      if( n_mii<0 )
        n_mii = cmInvalidIdx;
    }
  }
  else // ... or push a new record onto p->midiBuf[]
  {
    n_mii = p->mii;
    ++p->mii;
  }

  // store the MIDI event
  p->midiBuf[n_mii].mni      = mni;
  p->midiBuf[n_mii].locIdx   = locIdx;
  p->midiBuf[n_mii].scEvtIdx = scEvtIdx;
  p->midiBuf[n_mii].smpIdx   = smpIdx;
  p->midiBuf[n_mii].pitch    = pitch;
  p->midiBuf[n_mii].vel      = vel;

  // setting vsi=nsi and vsli=nsli will indicate to the calling
  // program that no new sets are ready.
  p->vsi  = p->nsi;
  p->vsli = p->nsli;

  if( locIdx == cmInvalidIdx )
    return cmOkRC;

  // 
  unsigned scLocIdx    = p->mp->loc[ locIdx ].scLocIdx;
  unsigned maxScLocIdx = cmScoreLocCount(p->mp->scH)-1;

  // if this cmScMeas object has not yet synchronized to the cmScMatcher
  // (if p->nsli is not valid)
  if( p->nsli == cmInvalidIdx )
  {
    unsigned i;
    for(i=0; i<p->sn; ++i)
      if( p->set[i].esli+1 == scLocIdx )
      {
        p->nsli = scLocIdx;
        p->nsi  = i;
        break;
      }

    if(i==p->sn)
      return rc;
  }

  p->vsi = p->nsi;
  p->vsli = p->nsli;

  // for each cmScore location between p->nsli and scLocIdx 
  for(; p->nsli<=scLocIdx && p->nsi < p->sn; ++p->nsli)
  {
    // if this score location index (p->nsli) is one score location 
    // ahead of the next sets ending location.
    while( cmMin(maxScLocIdx,p->set[p->nsi].esli+1) == p->nsli )
    {
      // calculate the value assoc'd with p->set[p->nsi]
      _cmScMeasCalcVal(p, p->set + p->nsi, n_mii );

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
    if( (mep->msg->status&0xf0) == kNoteOnMdId )
    {
      rc = cmScMatcherExec(p, mep->obj.seqSmpIdx, mep->msg->uid, mep->msg->status, mep->msg->u.chMsgPtr->d0, mep->msg->u.chMsgPtr->d1, NULL );

      switch( rc )
      {
        case cmOkRC:         // continue processing MIDI events
          break;

        case cmEofRC:        // end of the score was encountered
          break;

        case cmInvalidArgRC: // p->eli was not set correctly
          break;

        case cmSubSysFailRC: // scan resync failed
          break;

        default:
          { assert(0); }
      }
    }
  }

  if( rc == cmEofRC )
    rc = cmOkRC;

  return rc;
}

// This callback connects/feeds the cmScMeas object from the cmScMatcher object.
// (See _cmScMatcherStoreResult().)
void cmScMatcherCb( cmScMatcher* p, void* arg, cmScMatcherResult_t* rp )
{
  cmScMeas* mp = (cmScMeas*)arg;
  cmScMeasExec(mp, rp->mni, rp->locIdx, rp->scEvtIdx, rp->flags, rp->smpIdx, rp->pitch, rp->vel );
}

void       cmScAlignScanMarkers(  cmRpt_t* rpt, cmTlH_t tlH, cmScH_t scH )
{
  unsigned     i;
  double       srate        = cmTimeLineSampleRate(tlH);
  unsigned     midiN        = 7; 
  unsigned     scWndN       = 10;
  unsigned     markN        = 291;
  unsigned     dynRefArray[] = {   14,  28, 42, 56, 71, 85, 99, 113,128 };
  unsigned     dynRefCnt    = sizeof(dynRefArray)/sizeof(dynRefArray[0]);
  cmCtx*       ctx          = cmCtxAlloc(NULL, rpt, cmLHeapNullHandle, cmSymTblNullHandle );
  cmScMeas*    mp           = cmScMeasAlloc(ctx,NULL,scH,srate,dynRefArray,dynRefCnt);
  cmScMatcher* p            = cmScMatcherAlloc(ctx,NULL,srate,scH,scWndN,midiN,cmScMatcherCb,mp);  
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
  
  // for each marker
  for(i=0; i<markN; ++i)
  {
    // form the marker text
    snprintf(markText,markCharCnt,"Mark %i",i);

    // locate the marker
    cmTlMarker_t*  tlmp    = cmTimeLineMarkerFind( tlH, markText );
    if( tlmp == NULL )
    {
      if( printFl )
        printf("The marker '%s' was not found.\n\n",markText);  
      continue;
    }

    // skip markers which do not contain text
    if( cmTextIsEmpty(tlmp->text) )
    {
      if( printFl )
        printf("The marker '%s' is being skipped because it has no text.\n\n",markText);  
      continue;
    }

    printf("=================== MARKER:%s ===================\n",markText);
    
    cmScMatcherReset(p,0); // reset the score follower to the beginnig of the score
    cmScMeasReset(mp);

    ++candCnt;

    // scan to the beginning of the marker
    cmRC_t rc  = cmScAlignScanToTimeLineEvent(p,tlH,&tlmp->obj,tlmp->obj.seqSmpIdx+tlmp->obj.durSmpCnt);
    bool   pfl = true;

    if( rc != cmOkRC || p->begSyncLocIdx==cmInvalidIdx)
    {

      bool fl = printFl;
      printFl = true;

      // if a no alignment was found
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

      printFl = fl;
    }

    if( pfl )
    {      
      double fmeas = cmScMatcherFMeas(p);

      if( printFl )
        printf("mark:%i midi:%i loc:%i bar:%i cost:%f f-meas:%f text:%s\n",i,p->mni,p->begSyncLocIdx,p->mp->loc[p->begSyncLocIdx].barNumb,p->s_opt,fmeas,tlmp->text);

      if( fmeas < scoreThresh )
        ++scoreFailCnt;

    }

    //print score and match for entire marker
    //cmScMatcherPrint(p);

     // ONLY USE ONE MARKER DURING TESTING
    // break; 

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

//=======================================================================================================================
cmScModulator* cmScModulatorAlloc( cmCtx* c, cmScModulator* p, cmCtx_t* ctx, cmSymTblH_t stH, double srate, unsigned samplesPerCycle, const cmChar_t* fn, const cmChar_t* modLabel, cmScModCb_t cbFunc, void* cbArg )
{
  cmScModulator* op = cmObjAlloc(cmScModulator,c,p);

  if( ctx != NULL )
    if( cmScModulatorInit(op,ctx,stH,srate,samplesPerCycle,fn,modLabel,cbFunc,cbArg) != cmOkRC )
      cmScModulatorFree(&op);

  return op;
}

cmRC_t cmScModulatorFree(  cmScModulator** pp )
{
  cmRC_t rc = cmOkRC;
  if( pp==NULL || *pp==NULL )
    return rc;

  cmScModulator* p = *pp;
  if((rc = cmScModulatorFinal(p)) != cmOkRC )
    return rc;

  cmMemFree(p->earray);
  cmObjFree(pp);
  return rc;
}

typedef struct
{
  unsigned        typeId;
  unsigned        minArgCnt;
  const cmChar_t* label;
} _cmScModTypeMap_t;

_cmScModTypeMap_t _cmScModTypeArray[] =
{
  { kSetModTId,     1, "set" },
  { kLineModTId,    2, "line" },
  { kSetLineModTId, 3, "sline" },
  { kPostModTId,    4, "post" },
  { kInvalidModTId, 0, "<invalid>"}
};

const _cmScModTypeMap_t*  _cmScModTypeLabelToMap( const cmChar_t* label )
{
  unsigned i;
  for(i=0; _cmScModTypeArray[i].typeId!=kInvalidModTId; ++i)
    if( strcmp(_cmScModTypeArray[i].label,label) == 0 )
      return _cmScModTypeArray + i;

  return NULL;
}

cmScModVar_t* _cmScModSymToVar( cmScModulator* p, unsigned varSymId )
{
  cmScModVar_t* vp = p->vlist;  
  for(; vp!=NULL; vp=vp->vlink)
    if( varSymId == vp->varSymId )
      return vp;
  return NULL;
}

cmScModVar_t* _cmScModulatorInsertVar( cmScModulator* p, unsigned varSymId, unsigned flags )
{
  cmScModVar_t* vp = _cmScModSymToVar(p,varSymId);

  if( vp == NULL )
  {
    vp = cmMemAllocZ(cmScModVar_t,1);
    vp->varSymId = varSymId;
    vp->outVarId = cmInvalidId;
    vp->vlink    = p->vlist;
    p->vlist     = vp;
  }

  vp->flags    = flags;
  vp->value    = DBL_MAX;
  vp->min      = DBL_MAX;
  vp->max      = DBL_MAX;
  vp->rate     = DBL_MAX;
  vp->phase    = 0;
  vp->entry    = NULL;
  vp->alink    = NULL;
  
  return vp;
}

cmScModEntry_t* _cmScModulatorInsertEntry(cmScModulator* p, unsigned idx, unsigned scLocIdx, unsigned modSymId, unsigned varSymId, unsigned typeId, unsigned paramCnt )
{
  assert( idx < p->en );

  p->earray[idx].scLocIdx = scLocIdx;
  p->earray[idx].typeId   = typeId;
  p->earray[idx].varPtr   = _cmScModulatorInsertVar(p,varSymId,0);


  if( p->earray[idx].varPtr->outVarId == cmInvalidIdx )
    p->earray[idx].varPtr->outVarId = p->outVarCnt++;

  return p->earray + idx;
}


/*
{
  [
  { loc:123, mod:modlabel, var:varlabel, param:[ ] }
  ]
}
 */

 // Parameter values are found as values of the 'data','min' or 'max' fields.
 // A parameter value may be either a symbol identifier (mapped to a variable)
 // or a literal number.  This function determines which form the paramter
 // value takes and parses it accordingly.
  cmRC_t _cmScModulatorParseParam( cmScModulator* p, cmSymTblH_t stH, cmJsonNode_t* np, cmScModParam_t* pp )
{
  cmRC_t rc = cmOkRC;

  switch( np->typeId )
  {
    case kIntTId:
    case kRealTId:
      if( cmJsonRealValue(np, &pp->val ) != kOkJsRC )
      {
        rc = cmCtxRtCondition( &p->obj, cmInvalidArgRC, "Error parsing in Modulator literal value." );    
        goto errLabel;
      }
      pp->pid = kLiteralModPId;
      break;

    case kStringTId:
      {
        const cmChar_t* label = NULL;
        if( cmJsonStringValue(np, &label) != kOkJsRC )
        {
          rc = cmCtxRtCondition( &p->obj, cmInvalidArgRC, "Error parsing in Modulator symbol label." );    
          goto errLabel;
        }

        pp->symId = cmSymTblRegisterSymbol(stH,label);
        pp->pid   = kSymbolModPId;
      }
      break;

    default:
      rc = cmCtxRtCondition( &p->obj, cmInvalidArgRC, "Parameter value is not a number or identifier." );    
      goto errLabel;
      break;
  }

 errLabel:
  return rc;
}


cmRC_t _cmScModulatorParse( cmScModulator* p, cmCtx_t* ctx, cmSymTblH_t stH, const cmChar_t* fn )
{
  cmRC_t        rc  = cmOkRC;
  cmJsonNode_t* jnp = NULL;
  cmJsonH_t     jsH = cmJsonNullHandle;
  unsigned      i   = cmInvalidIdx;
  unsigned      j   = cmInvalidIdx;

  // read the JSON file
  if( cmJsonInitializeFromFile(&jsH, fn, ctx ) != kOkJsRC )
    return cmCtxRtCondition( &p->obj, cmInvalidArgRC, "JSON file parse failed on the modulator file: %s.",cmStringNullGuard(fn) );

  jnp = cmJsonRoot(jsH);

  // validate that the first child as an array
  if( jnp==NULL || ((jnp = cmJsonNodeMemberValue(jnp,"array")) == NULL) || cmJsonIsArray(jnp)==false )
  {
    rc = cmCtxRtCondition( &p->obj, cmInvalidArgRC, "Modulator file header syntax error in file:%s",cmStringNullGuard(fn) );
    goto errLabel;
  }

  // allocate the entry array
  unsigned entryCnt = cmJsonChildCount(jnp);
  p->earray         = cmMemResizeZ(cmScModEntry_t,p->earray,entryCnt);
  p->en             = entryCnt;

  unsigned        prvScLocIdx = cmInvalidIdx;
  const cmChar_t* prvModLabel   = NULL;
  const cmChar_t* prvVarLabel   = NULL;
  const cmChar_t* prvTypeLabel  = NULL;
  for(i=0; i<entryCnt; ++i)
  {
    cmJsRC_t                 jsRC;
    const char*              errLabelPtr = NULL;
    unsigned                 scLocIdx    = cmInvalidIdx;
    const cmChar_t*          modLabel    = NULL;
    const cmChar_t*          varLabel    = NULL;
    const cmChar_t*          typeLabel   = NULL;
    cmJsonNode_t*            onp         = cmJsonArrayElement(jnp,i);
    cmJsonNode_t*            dnp         = NULL;
    const _cmScModTypeMap_t* map         = NULL;

    if((jsRC = cmJsonMemberValues( onp, &errLabelPtr, 
          "loc", kIntTId    | kOptArgJsFl, &scLocIdx,
          "mod", kStringTId | kOptArgJsFl, &modLabel,
          "var", kStringTId | kOptArgJsFl, &varLabel,
          "type",kStringTId | kOptArgJsFl, &typeLabel,
          NULL )) != kOkJsRC )
    {
      if( errLabelPtr == NULL )
        rc = cmCtxRtCondition( &p->obj, cmInvalidArgRC, "Error:%s on record at index %i in file:%s",errLabelPtr,i,cmStringNullGuard(fn) );
      else
        rc = cmCtxRtCondition( &p->obj, cmInvalidArgRC, "Synax error in Modulator record at index %i in file:%s",i,cmStringNullGuard(fn) );
      goto errLabel;
    }

    // if the score location was not given use the previous score location
    if( scLocIdx == cmInvalidIdx )
      scLocIdx = prvScLocIdx;
    else
      prvScLocIdx = scLocIdx;

    // if the mod label was not given use the previous one
    if( modLabel == NULL )
      modLabel = prvModLabel;
    else
      prvModLabel = modLabel;

    if( modLabel == NULL )
    {
      rc = cmCtxRtCondition(&p->obj, cmInvalidArgRC, "No 'mod' label has been set in mod file '%s'.",cmStringNullGuard(fn));
      goto errLabel;
    }

    // if the var label was not given use the previous one
    if( varLabel == NULL )
      varLabel = prvVarLabel;
    else
      prvVarLabel = varLabel;

    if( varLabel == NULL )
    {
      rc = cmCtxRtCondition(&p->obj, cmInvalidArgRC, "No 'var' label has been set in mod file '%s'.",cmStringNullGuard(fn));
      goto errLabel;
    }

    // if the type label was not given use the previous one 
    if( typeLabel == NULL )
      typeLabel = prvTypeLabel;
    else
      prvTypeLabel = typeLabel;

    if( typeLabel == NULL )
    {
      rc = cmCtxRtCondition(&p->obj, cmInvalidArgRC, "No 'type' label has been set in mod file '%s'.",cmStringNullGuard(fn));
      goto errLabel;
    }

    // validate the entry type label
    if((map = _cmScModTypeLabelToMap(typeLabel)) == NULL )
    {
      rc = cmCtxRtCondition( &p->obj, cmInvalidArgRC, "Unknown entry type '%s' in Modulator record at index %i in file:%s",cmStringNullGuard(typeLabel),i,cmStringNullGuard(fn) );
      goto errLabel;
    }
    
    unsigned modSymId = cmSymTblRegisterSymbol(stH,modLabel);
    unsigned varSymId = cmSymTblRegisterSymbol(stH,varLabel);

    // the mod entry label must match the modulators label
    if( p->modSymId != modSymId )
    {
      --p->en;
        continue;
    } 

    // get the count of the elmenets in the data array
    unsigned paramCnt = cmJsonChildCount(onp);

    // fill the entry record and find or create the target var
    cmScModEntry_t* ep = _cmScModulatorInsertEntry(p,i,scLocIdx,modSymId,varSymId,map->typeId,paramCnt);

    typedef struct
    {
      const cmChar_t* label;
      cmScModParam_t* param;
    } map_t;

    // parse the var and parameter records
    map_t mapArray[] = 
    {
      { "min", &ep->min  },
      { "max", &ep->max  },
      { "rate",&ep->rate },
      { "val", &ep->beg },
      { "end", &ep->end },
      { "dur", &ep->dur },
      { NULL, NULL }
    };

    unsigned j=0;
    for(j=0; mapArray[j].param!=NULL; ++j)
      if((dnp = cmJsonFindValue(jsH,mapArray[j].label, onp, kInvalidTId )) != NULL )
        if((rc = _cmScModulatorParseParam(p,stH,dnp,mapArray[j].param)) != cmOkRC )
          goto errLabel;    
  }

 errLabel:

  if( rc != cmOkRC )
    cmCtxRtCondition( &p->obj, cmInvalidArgRC, "Error parsing in Modulator 'data' record at index %i value index %i in file:%s",i,j,cmStringNullGuard(fn) );    


  // release the JSON tree
  if( cmJsonIsValid(jsH) )
    cmJsonFinalize(&jsH);

  return rc;
}


cmRC_t  _cmScModulatorReset( cmScModulator* p, cmCtx_t* ctx, unsigned scLocIdx )
{
  cmRC_t rc = cmOkRC;

  p->alist     = NULL;
  p->elist     = NULL;
  p->nei       = 0;
  p->outVarCnt = 0;
  
  // reload the file
  if((rc = _cmScModulatorParse(p,ctx,p->stH,p->fn)) != cmOkRC )
    goto errLabel;


  // clear the active flag on all variables
  cmScModVar_t* vp = p->vlist;
  for(; vp!=NULL; vp=vp->vlink)
  {
    vp->flags = cmClrFlag(vp->flags,kActiveModFl);
    vp->alink = NULL;
  }

 errLabel:
  return rc;
}

cmRC_t cmScModulatorInit(  cmScModulator* p, cmCtx_t* ctx, cmSymTblH_t stH, double srate, unsigned samplesPerCycle, const cmChar_t* fn, const cmChar_t* modLabel, cmScModCb_t cbFunc, void* cbArg )
{
  cmRC_t rc;

  if((rc = cmScModulatorFinal(p)) != cmOkRC )
    return rc;

  p->fn              = cmMemAllocStr(fn);
  p->stH             = stH;
  p->modSymId        = cmSymTblRegisterSymbol(stH,modLabel);
  p->cbFunc          = cbFunc;
  p->cbArg           = cbArg;
  p->samplesPerCycle = samplesPerCycle;
  p->srate           = srate;
  

  if( rc != cmOkRC )
    cmScModulatorFinal(p);
  else
    _cmScModulatorReset(p,ctx,0);

  return rc;
}

cmRC_t cmScModulatorFinal( cmScModulator* p )
{
  cmMemFree(p->fn);

  // release each var record
  cmScModVar_t* vp = p->vlist;
  while( vp!=NULL )
  {
    cmScModVar_t* np = vp->vlink;
    cmMemFree(vp);
    vp=np;
  }

  return cmOkRC;
}

unsigned        cmScModulatorOutVarCount( cmScModulator* p )
{ return p->outVarCnt; }

cmScModVar_t* cmScModulatorOutVar( cmScModulator* p, unsigned idx )
{
  unsigned i;
  for(i=0; i<p->en; ++i)
    if( p->earray[i].varPtr->outVarId == idx )
      return p->earray[i].varPtr;

  return NULL;  
}

cmRC_t         cmScModulatorSetValue( cmScModulator* p, unsigned varSymId, double value, double min, double max )
{
  cmScModVar_t* vp;
  // if the var does not exist ....
  if((vp = _cmScModSymToVar(p, varSymId )) == NULL )
  {
    // ... then create it
    vp =  _cmScModulatorInsertVar(p,varSymId,kCalcModFl);
    assert(vp!=NULL);    
  }

  assert( min <= max);

  vp->min   = min;
  vp->max   = max;  
  vp->value = value;

  return cmOkRC;
}


cmRC_t cmScModulatorReset( cmScModulator* p, cmCtx_t* ctx, unsigned scLocIdx )
{
  _cmScModulatorReset(p,ctx,scLocIdx);
  return cmScModulatorExec(p,scLocIdx);
}

void  _cmScModUnlinkActive( cmScModulator* p, cmScModVar_t* vp, cmScModVar_t* pp )
{
  // if vp is the first link on the chain
  if( vp == p->alist )
    p->alist = vp->alink;

  // if vp is the last link on the chain
  if( vp == p->elist )
    p->elist = pp;

  if( pp != NULL )
    pp->alink = vp->alink;

  vp->flags = cmClrFlag(vp->flags,kActiveModFl);
  vp->alink = NULL;  
  vp->entry = NULL;
}

// If the requested parameter has a value then return it in *valPtr.
// If it does not then do nothing. This function applies scaling to RHS values.
cmRC_t  _cmScModGetParam( cmScModulator* p, const cmScModParam_t* pp, double* valPtr )
{
  cmRC_t rc = cmOkRC;

  switch( pp->pid )
  {
    case kInvalidModPId:
      rc  = cmCtxRtCondition( &p->obj, cmInvalidArgRC, "An invalid parameter was encountered.");
      goto errLabel;
      break;

    case kLiteralModPId:
      *valPtr = pp->val;
      break;

    case kSymbolModPId:
      {
        cmScModVar_t* vp;

        // get a pointer to the parameter variable
        if((vp = _cmScModSymToVar(p, pp->symId )) == NULL )
        {
          rc  = cmCtxRtCondition( &p->obj, cmInvalidArgRC, "Variable '%s' not found.",cmSymTblLabel(p->stH,pp->symId));    
          goto errLabel;
        } 

        // if this is not a 'calculated' paramter then scale it here.
        if( cmIsFlag(vp->flags,kCalcModFl ) && vp->min!=DBL_MAX && vp->max!=DBL_MAX )
          *valPtr = (vp->value - vp->min)/(vp->max-vp->min);
        else
          *valPtr = vp->value;
      }
      break;

    default:
      { assert(0); }
  }

 errLabel:
  return rc;
}

// Type specific variable activation - 
cmRC_t _cmScModActivate(cmScModulator* p, cmScModEntry_t* ep )
{
  cmRC_t rc = cmOkRC;

  cmScModVar_t* vp = ep->varPtr;

  // optionally update the min/max/rate values in the target var
  if( ep->min.pid != kInvalidModPId )
    if((rc = _cmScModGetParam(p,&ep->min,&vp->min)) != cmOkRC )
      goto errLabel;

  if( ep->max.pid != kInvalidModPId )
    if((rc = _cmScModGetParam(p,&ep->max,&vp->max)) != cmOkRC )
      goto errLabel;

  if( ep->rate.pid != kInvalidModPId )
    if((rc = _cmScModGetParam(p,&ep->rate,&vp->rate)) != cmOkRC )
      goto errLabel;


  switch( ep->typeId )
  {
    case kSetModTId:
      break;

    case kLineModTId:
      vp->v0    = vp->value;
      vp->phase = 0;
      break;

    case kSetLineModTId:
      _cmScModGetParam(p,&ep->beg,&vp->value); // starting value
      vp->v0     = vp->value;                  // set initial value
      vp->phase  = 0;                          // reset phase
      break;

    case kPostModTId:
      p->postFl = vp->value;
      break;

    default:
      { assert(0); }
  }

 errLabel:
  return rc;
}

// Callback the application with a new variable value.
cmRC_t  _cmScModExecSendValue( cmScModulator* p, cmScModVar_t* vp )
{
  cmRC_t rc     = cmOkRC;
  bool   sendFl = true;
  double v      = vp->value;

  // scale the output value - this is equiv to scaling the LHS
  if( cmIsFlag(vp->flags,kCalcModFl) && vp->min!=DBL_MAX && vp->max!=DBL_MAX )
    v = vp->min + v * (vp->max - vp->min);

  // if an output rate throttle is in effect ....
  if( vp->rate!=DBL_MAX && vp->phase!=0 )
    sendFl = remainder(vp->phase*p->samplesPerCycle, p->srate*vp->rate/1000 ) < p->samplesPerCycle;

  if(sendFl)
    p->cbFunc(p->cbArg,vp->varSymId,v,p->postFl);

  return rc;
}

// Return true if vp should be deactivated otherwise return false.
bool  _cmScModExec( cmScModulator* p, cmScModVar_t* vp )
{
  cmRC_t rc     = cmOkRC;
  bool   fl     = false;
  bool   sendFl = true;

  switch( vp->entry->typeId )
  {
    case kSetModTId:
      {
        if((rc = _cmScModGetParam(p,&vp->entry->beg,&vp->value)) != cmOkRC )
          goto errLabel;
        
        vp->phase = 0; // force the value to be sent
        fl = true;
      }
      break;

    case kSetLineModTId:
    case kLineModTId:
      {
        double v1=0, td=0;

        // get the target value
        if((rc = _cmScModGetParam(p,&vp->entry->end,&v1)) != cmOkRC) 
          goto errLabel;

        // get the time duration
        if((rc = _cmScModGetParam(p,&vp->entry->dur,&td)) != cmOkRC) 
          goto errLabel;

        double v  = vp->v0 + (v1-vp->v0) * (vp->phase * p->samplesPerCycle) / (p->srate * td);        

        if((fl =  (vp->value <= v1 && v >= v1) || (vp->value >= v1 && v <= v1 )) == true )
          v  = v1;
       
        vp->value  = v;
      }
      break;

    case kPostModTId:
      sendFl = false;
      break;

    default:
      { assert(0); }
  }

  // notify the application that a new variable value has been generated
  if(sendFl)
  {
    rc = _cmScModExecSendValue(p,vp);

    // increment the phase - after send because send notices when phase is zero
    vp->phase += 1;
  }

 errLabel:
  if( rc != cmOkRC )
    fl = true;

  return fl;
}


cmRC_t cmScModulatorExec( cmScModulator* p, unsigned scLocIdx )
{
  cmRC_t trc;
  cmRC_t rc = cmOkRC;

  // trigger entries that have expired since the last call to this function
  for(; p->nei<p->en && (p->earray[p->nei].scLocIdx==-1 || p->earray[p->nei].scLocIdx<=scLocIdx); ++p->nei)
  {
    cmScModEntry_t* ep = p->earray + p->nei;

    // if the variable assoc'd with this entry is not on the active list ...
    if( cmIsFlag(ep->varPtr->flags,kActiveModFl) == false )
    {
      // ... then append it to the end of the active list ...
      ep->varPtr->flags |= kActiveModFl; 

      if( p->elist == NULL )
        p->elist = ep->varPtr;
      else
      {
        p->elist->alink = ep->varPtr;
        p->elist        = ep->varPtr;
      }

      p->elist->alink = NULL;

      if( p->alist == NULL )
        p->alist = ep->varPtr;
    }

    // do type specific activation
    if((trc = _cmScModActivate(p,ep)) != cmOkRC )
      rc = trc;

    ep->varPtr->entry = ep;

  }
    
  
  // Update the active variables
  cmScModVar_t* pp = NULL;
  cmScModVar_t* vp = p->alist;
  for(; vp!=NULL; vp=vp->alink)
  {
    if( _cmScModExec(p,vp) )
      _cmScModUnlinkActive(p,vp,pp);
    else
      pp = vp;
  }

  return rc;
}


void _cmScModDumpParam( cmScModulator* p, const cmChar_t* label, cmScModParam_t* pp )
{
  printf("%s: ",label);

  switch( pp->pid )
  {
    case kInvalidModPId:
      printf("<invalid>");
      break;

    case kLiteralModPId:
      if( pp->val == DBL_MAX )
        printf("<max> ");
      else
        printf("%f ",pp->val);
      break;

    case kSymbolModPId:
      printf("%s ",cmSymTblLabel(p->stH,pp->symId));
      break;

    default:
      { assert(0); }
  }
}

void _cmScModDumpVal( cmChar_t* label, double val )
{
  printf("%s:",label);

  if( val == DBL_MAX )
    printf("<max> " );
  else
    printf("%f ",val);
}

void _cmScModDumpVar( cmScModulator* p, const cmScModVar_t* vp )
{
    printf("%7s %3i fl:0x%x entry:%p alink:%p ",cmSymTblLabel(p->stH,vp->varSymId),vp->outVarId,vp->flags,vp->entry,vp->alink);
    _cmScModDumpVal("val",vp->value);
    _cmScModDumpVal("min",vp->min);
    _cmScModDumpVal("max",vp->max);
    _cmScModDumpVal("rate",vp->rate);
    _cmScModDumpVal("v0",vp->v0);
}

cmRC_t  cmScModulatorDump(  cmScModulator* p )
{
  cmRC_t rc = cmOkRC;

  printf("MOD:\n");
  printf("nei:%i alist:%p outVarCnt:%i\n",p->nei,p->alist,p->outVarCnt);

  printf("ENTRIES:\n");
  unsigned i;
  for(i=0; i<p->en; ++i)
  {
    cmScModEntry_t* ep = p->earray + i;
    printf("%3i %4i %2i %7s ", i, ep->scLocIdx, ep->typeId, cmSymTblLabel(p->stH,ep->varPtr->varSymId));
    _cmScModDumpParam(p," beg", &ep->beg);
    _cmScModDumpParam(p," end", &ep->end);
    _cmScModDumpParam(p," min", &ep->min);
    _cmScModDumpParam(p," max", &ep->max);
    _cmScModDumpParam(p," rate",&ep->rate);
    printf("\n");
  }

  printf("VARIABLES\n");
  cmScModVar_t* vp = p->vlist;
  for(; vp!=NULL; vp=vp->vlink)
  {
    _cmScModDumpVar(p,vp);
    printf("\n");
  }
  
  return rc;
}

//=======================================================================================================================
cmRecdPlay*    cmRecdPlayAlloc( cmCtx* c, cmRecdPlay* p, double srate, unsigned fragCnt, unsigned chCnt, double initFragSecs, double maxLaSecs, double curLaSecs  )
{
  cmRecdPlay* op = cmObjAlloc(cmRecdPlay,c,p);

  if( cmRecdPlayInit(op,srate,fragCnt,chCnt,initFragSecs,maxLaSecs,curLaSecs) != cmOkRC )
    cmRecdPlayFree(&op);

  return op;
}

cmRC_t         cmRecdPlayFree(  cmRecdPlay** pp )
{
  cmRC_t rc = cmOkRC;
  if( pp==NULL || *pp==NULL )
    return rc;

  cmRecdPlay* p = *pp;
  if((rc = cmRecdPlayFinal(p)) != cmOkRC )
    return rc;

  cmObjFree(pp);
  return rc;

}

cmRC_t cmRecdPlayInit(  cmRecdPlay* p, double srate, unsigned fragCnt, unsigned chCnt, double initFragSecs, double maxLaSecs, double curLaSecs  )
{
  cmRC_t rc;
  unsigned i;

  if( curLaSecs > maxLaSecs )
    return  cmCtxRtCondition( &p->obj, cmInvalidArgRC, "The initial look-ahead time %f is greater than the maximum look-ahead time %f.",curLaSecs,maxLaSecs);    

  if((rc = cmRecdPlayFinal(p)) != cmOkRC )
    return rc;

  if( chCnt == 0 )
    return cmOkRC;

  p->frags        = cmMemAllocZ(cmRecdPlayFrag,fragCnt);
  p->fragCnt      = fragCnt;
  p->srate        = srate;
  p->chCnt        = chCnt;
  p->initFragSecs = initFragSecs;
  p->maxLaSmpCnt  = floor(maxLaSecs*srate);
  p->curLaSmpCnt  = floor(curLaSecs*srate); 
  p->laChs        = cmMemAllocZ(cmSample_t*,chCnt);
  p->laSmpIdx     = 0;

  for(i=0; i<chCnt; ++i)
    p->laChs[i] = cmMemAllocZ(cmSample_t,p->maxLaSmpCnt);
  
  return rc;
}

cmRC_t         cmRecdPlayFinal( cmRecdPlay* p )
{ 
  unsigned i,j;
  // free the fragments
  for(i=0; i<p->fragCnt; ++i)
  {
    for(j=0; j<p->chCnt; ++j)
      cmMemFree(p->frags[i].chArray[j]);

    cmMemFree(p->frags[i].chArray);
  }

  // free the look-ahead buffers
  for(i=0; i<p->chCnt; ++i)
    cmMemFree(p->laChs[i]);

  cmMemPtrFree(&p->laChs);
  cmMemPtrFree(&p->frags);
  p->fragCnt = 0;
  p->chCnt   = 0;
  p->rlist   = NULL;
  p->plist   = NULL;
  return cmOkRC;
}

cmRC_t         cmRecdPlayRegisterFrag( cmRecdPlay* p,  unsigned fragIdx, unsigned labelSymId )
{
  assert( fragIdx < p->fragCnt );

  unsigned i;

  p->frags[ fragIdx ].labelSymId = labelSymId;

  p->frags[ fragIdx ].chArray = cmMemResizeZ(cmSample_t*,p->frags[fragIdx].chArray,p->chCnt);

  for(i=0; i<p->chCnt; ++i)
  {
    
    p->frags[ fragIdx ].allocCnt = floor(p->initFragSecs * p->srate);
    p->frags[ fragIdx ].chArray[i] = cmMemResizeZ(cmSample_t,p->frags[ fragIdx ].chArray[i],p->frags[fragIdx].allocCnt);
  }

  return cmOkRC;
}

cmRC_t cmRecdPlaySetLaSecs( cmRecdPlay* p, double curLaSecs )
{
   p->curLaSmpCnt  = floor(curLaSecs*p->srate);  
   return cmOkRC;
}


cmRC_t cmRecdPlayRewind( cmRecdPlay* p )
{
  unsigned i;

  // zero the look-ahead buffers
  p->laSmpIdx = 0;
  for(i=0; i<p->chCnt; ++i)
    cmVOS_Zero(p->laChs[i],p->maxLaSmpCnt);

  // remove all the active players
  while( p->plist != NULL )
    cmRecdPlayEndPlay(p,p->plist->labelSymId);
  
  // remove all the active recorders
  while( p->rlist != NULL )
    cmRecdPlayEndRecord(p,p->rlist->labelSymId);

  // rewind all the fragments play posn. 
  for(i=0; i<p->fragCnt; ++i)
    p->frags[i].playIdx = 0;

  return cmOkRC;
}


cmRC_t         cmRecdPlayBeginRecord( cmRecdPlay* p, unsigned labelSymId )
{
  unsigned i;

  for(i=0; i<p->fragCnt; ++i)
    if( p->frags[i].labelSymId == labelSymId )
    {
      // if the frag is not already on the recd list
      if( p->frags[i].rlink == NULL )
      {
        p->frags[i].recdIdx = 0;
        p->frags[i].playIdx = 0;
        p->frags[i].rlink   = p->rlist;
        p->rlist            = p->frags + i;

       
        // handle LA buf longer than frag buf.
        int cpyCnt  = cmMin(p->curLaSmpCnt,p->frags[i].allocCnt); 

         // go backwards in LA buf from newest sample to find init src offset
        int srcOffs = p->laSmpIdx - cpyCnt;

        // if the src is before the first sample in the LA buf then wrap to end of buf
        if( srcOffs < 0 )
          srcOffs += p->maxLaSmpCnt; 

        assert( 0 <= srcOffs && srcOffs < p->maxLaSmpCnt );

        // cnt of samples to copy from LA buf (limited by end of LA buf)
        int n0 = cmMin(cpyCnt,p->maxLaSmpCnt - srcOffs); 

        // if necessary wrap to begin of LA buf for remaining samples
        int n1 = cpyCnt>n0 ? n1 = cpyCnt-n0 : 0;
        int j;

        assert(n0+n1 == cpyCnt );

        for(j=0; j<p->chCnt; ++j)
          cmVOS_Copy(p->frags[i].chArray[j],n0,p->laChs[j]+srcOffs);

        if( n1 > 0 )
        {
          for(j=0; j<p->chCnt; ++j)
            cmVOS_Copy(p->frags[i].chArray[j]+n0,n1,p->laChs[j]);
        }

        p->frags[i].recdIdx = cpyCnt;
        p->frags[i].playIdx = 0;
        
      }
      
      return cmOkRC;
    }

  return  cmCtxRtCondition( &p->obj, cmInvalidArgRC, "The fragment label symbol id '%i' not found for 'begin record'.",labelSymId);    
}

cmRC_t         cmRecdPlayEndRecord(   cmRecdPlay* p, unsigned labelSymId )
{
  cmRecdPlayFrag* fp = p->rlist;
  cmRecdPlayFrag* pp = NULL;

  for(; fp != NULL; fp=fp->rlink )
  {
    if( fp->labelSymId == labelSymId )
    {
      if( pp == NULL )
        p->rlist = fp->rlink;
      else
        pp->rlink = fp->rlink;

      fp->rlink = NULL;

      return cmOkRC;
    }

    pp = fp;
  }

  return  cmCtxRtCondition( &p->obj, cmInvalidArgRC, "The fragment label symbol id '%i' not found for 'end record'.",labelSymId);      
}

cmRC_t         cmRecdPlayInsertRecord(cmRecdPlay* p, unsigned labelSymId, const cmChar_t* wavFn )
{
  cmRC_t rc = cmOkRC;
  unsigned i;

  for(i=0; i<p->fragCnt; ++i)
    if( p->frags[i].labelSymId == labelSymId )
    {
      cmAudioFileH_t    afH  = cmNullAudioFileH;
      cmAudioFileInfo_t afInfo;
      cmRC_t            afRC = kOkAfRC;

      // open the audio file
      if( cmAudioFileIsValid( afH = cmAudioFileNewOpen(wavFn, &afInfo, &afRC, p->obj.err.rpt )) == false )
        return  cmCtxRtCondition( &p->obj, cmInvalidArgRC, "The audio file '%s' could not be opened'.",cmStringNullGuard(wavFn));    

      // ignore blank
      if( afInfo.frameCnt == 0 )
        return cmOkRC;
          
      // allocate buffer space
      unsigned j;
      for(j=0; j<p->chCnt; ++j)
        p->frags[i].chArray[j] = cmMemResize(cmSample_t,p->frags[i].chArray[j],afInfo.frameCnt);

      p->frags[i].allocCnt = afInfo.frameCnt;

      // read samples into the buffer space
      unsigned chIdx = 0;
      unsigned chCnt = cmMin(p->chCnt,afInfo.chCnt);
      unsigned actFrmCnt = 0;
      if( cmAudioFileReadSample(afH,afInfo.frameCnt,chIdx,chCnt,p->frags[i].chArray, &actFrmCnt) != kOkAfRC )
        return cmCtxRtCondition(&p->obj, cmSubSysFailRC, "Read failed on the audio file '%s'.",cmStringNullGuard(wavFn));

      p->frags[i].recdIdx  = actFrmCnt;

      return rc;
    }

    return  cmCtxRtCondition( &p->obj, cmInvalidArgRC, "The fragment label symbol id '%i' not found for 'begin record'.",labelSymId);    
}


cmRC_t         cmRecdPlayBeginPlay(   cmRecdPlay* p, unsigned labelSymId )
{
  unsigned i;

  for(i=0; i<p->fragCnt; ++i)
    if( p->frags[i].labelSymId == labelSymId )
    {
      // if the frag is not already on the play list
      if( p->frags[i].plink == NULL )
      {
        p->frags[i].playIdx      = 0;
        p->frags[i].fadeSmpIdx   = 0;
        p->frags[i].fadeDbPerSec = 0.0;
        p->frags[i].plink        = p->plist;
        p->plist                 = p->frags + i;
      }

      return cmOkRC;
    }

  return  cmCtxRtCondition( &p->obj, cmInvalidArgRC, "The fragment label symbol id '%i' not found for 'begin play'.",labelSymId);    

}

cmRC_t         cmRecdPlayEndPlay(     cmRecdPlay* p, unsigned labelSymId )
{
  cmRecdPlayFrag* fp = p->plist;
  cmRecdPlayFrag* pp = NULL;

  for(; fp != NULL; fp=fp->plink )
  {
    if( fp->labelSymId == labelSymId )
    {
      if( pp == NULL )
        p->plist = fp->plink;
      else
        pp->plink = fp->plink;

      fp->plink = NULL;

      return cmOkRC;
    }

    pp = fp;
  }

  return  cmCtxRtCondition( &p->obj, cmInvalidArgRC, "The fragment label symbol id '%i' not found for 'end play'.",labelSymId);    
}

cmRC_t cmRecdPlayBeginFade(   cmRecdPlay* p, unsigned labelSymId, double fadeDbPerSec )
{
  cmRecdPlayFrag* fp = p->plist;

  for(; fp != NULL; fp=fp->plink )
    if( fp->labelSymId == labelSymId )
    {
      fp->fadeDbPerSec = -fabs(fadeDbPerSec);
      return cmOkRC;
    }
  
  return  cmCtxRtCondition( &p->obj, cmInvalidArgRC, "The fragment label symbol id '%i' not found for 'fade begin'.",labelSymId);      
}


cmRC_t         cmRecdPlayExec( cmRecdPlay* p, const cmSample_t** iChs, cmSample_t** oChs, unsigned chCnt, unsigned smpCnt )
{
  unsigned i;

  chCnt = cmMin(chCnt, p->chCnt);

  //-------------------------------------------------------------------
  // copy incoming audio into the look-ahead buffers
  //

  // if the number of incoming samples is longer than the look-head buffer
  // then copy exactly maxLaSmpCnt samples from the end of the incoming sample
  // buffer to the look-ahead buffer.
  unsigned srcOffs   = 0;
  unsigned srcSmpCnt = smpCnt;
  if( srcSmpCnt > p->maxLaSmpCnt )
  {
    // advance incoming sample buffer so that there are maxLaSmpCnt samples remaining
    srcOffs   = srcSmpCnt-p->maxLaSmpCnt; 
    srcSmpCnt = p->maxLaSmpCnt;        // decrease the total samples to copy  
  }

  // count of samples from cur posn to end of the LA buffer.
  unsigned          n0  = cmMin(srcSmpCnt, p->maxLaSmpCnt - p->laSmpIdx );

  // count of samples past the end of the LA buffer to be wrapped into begin of buffer
  unsigned          n1  = srcSmpCnt>n0 ? srcSmpCnt-n0 : 0;

  assert(n0+n1 == srcSmpCnt);

  // copy first block to end of LA buffer
  for(i=0; i<chCnt; ++i)
    if( iChs[i] == NULL )
      cmVOS_Zero(p->laChs[i]+p->laSmpIdx,n0);
    else
      cmVOS_Copy(p->laChs[i]+p->laSmpIdx,n0,iChs[i] + srcOffs);

  p->laSmpIdx += n0;

  if( n1!=0)
  {
    // copy second block to begin of LA buffer
    for(i=0; i<chCnt; ++i)
      if( iChs[i] == NULL )
        cmVOS_Zero(p->laChs[i],n1);
      else
        cmVOS_Copy(p->laChs[i],n1,iChs[i] + srcOffs + n0);

    p->laSmpIdx = n1; 

  }

  //-------------------------------------------------------------------
  // copy incoming audio into the active record buffers
  //
  cmRecdPlayFrag* fp = p->rlist;

  for(; fp!=NULL; fp=fp->rlink)
  {
    assert( fp->recdIdx <= fp->allocCnt);
    unsigned n = cmMin(fp->allocCnt - fp->recdIdx,smpCnt);
    unsigned i;
    for(i=0; i<p->chCnt; ++i)
      if( iChs[i] == NULL )
        cmVOS_Zero(fp->chArray[i] + fp->recdIdx, n );
      else
        cmVOS_Copy(fp->chArray[i] + fp->recdIdx, n, iChs[i] );

    fp->recdIdx += n;

  }  

  //-------------------------------------------------------------------
  // copy outgoing audio out of the active play buffers
  //
  fp = p->plist;
  for(; fp!=NULL; fp=fp->rlink)
  {
    assert( fp->playIdx <= fp->recdIdx);

    double   gain = pow(10.0,((fp->fadeSmpIdx / p->srate) * fp->fadeDbPerSec)/20.0);
    unsigned n    = cmMin(fp->recdIdx - fp->playIdx,smpCnt);
    unsigned i;

    for(i=0; i<p->chCnt; ++i)
      if( oChs[i] != NULL )
        cmVOS_MultVVS(oChs[i],n,fp->chArray[i] + fp->playIdx,gain);

    fp->playIdx += n;

    // if a fade rate has been set then advance the fade phase
    if(fp->fadeDbPerSec!=0.0)
      fp->fadeSmpIdx += smpCnt;
  }  

  return cmOkRC;
}

