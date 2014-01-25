#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmLex.h"
#include "cmCsv.h"
#include "cmSdb.h"
#include "cmText.h"

typedef enum
{
  kUuidColIdx,
  kBaseUuidColIdx,
  kChIdxColIdx,
  kObiColIdx,
  kIbiColIdx,
  kIeiColIdx,
  kOeiColIdx,
  kSrcColIdx,
  kMidiColIdx,
  kInstrColIdx,
  kSrateColIdx,
  kChCntColIdx,
  kNotesColIdx,
  kAfnColIdx,
  kInvalidColIdx
} cmSdbColIdx_t;

struct cmSdb_str;

typedef struct cmSdbRspBlk_str
{
  unsigned*               indexV;  // indexV[ cmSdb_t.blkIdxAllocCnt ]
  unsigned                cnt;     // count of indexes used
  struct cmSdbRspBlk_str* link;    // cmSdbRsp_t.blocks link
} cmSdbRspBlk_t;

typedef struct cmSdbRsp_str
{
  struct cmSdb_str*    p;       // 
  cmSdbRspBlk_t*       blocks;  // first block ptr
  cmSdbRspBlk_t*       ebp;     // end block ptr
  unsigned             cnt;     // total count of indexes
  struct cmSdbRsp_str* link;    // cmSdb_t.responses link
} cmSdbRsp_t;

typedef struct cmSdb_str
{
  cmCtx_t              ctx;
  cmLHeapH_t           lhH;
  cmCsvH_t             csvH;
  cmSdbEvent_t*        eV;
  unsigned             eN;
  unsigned             blkIdxAllocCnt;
  struct cmSdbRsp_str* responses;
} cmSdb_t;

cmSdbH_t         cmSdbNullHandle         = cmSTATIC_NULL_HANDLE;
cmSdbResponseH_t cmSdbResponseNullHandle = cmSTATIC_NULL_HANDLE;

cmSdb_t* _cmSdbHandleToPtr( cmSdbH_t h )
{
  cmSdb_t* p = (cmSdb_t*)h.h;
  assert( p != NULL );
  return p;
}

cmSdbRsp_t* _cmSdbRspHandleToPtr( cmSdbResponseH_t h )
{
  cmSdbRsp_t* p = (cmSdbRsp_t*)h.h;
  assert( p != NULL );
  return p;
}

void _cmSdbRspBlkFree( cmSdb_t* p, cmSdbRspBlk_t* bp )
{
  cmLhFree(p->lhH, bp->indexV);
  cmLhFree(p->lhH, bp);
}


cmSdbRspBlk_t*  _cmSdbRspBlkUnlink( cmSdbRsp_t* rp, cmSdbRspBlk_t* bp )
{
  cmSdbRspBlk_t* dp = rp->blocks;
  cmSdbRspBlk_t* pp = NULL;
  for(; dp!=NULL; dp=dp->link)
  {
    if( dp == bp )
    {
      if( pp == NULL )
        rp->blocks = dp->link;
      else
        pp->link = dp->link;

      return bp;
    }

    pp = dp;
  }

  assert(0);
  return NULL;
}

void _cmSdbRspInsertIndex( cmSdb_t* p, cmSdbRsp_t* rp, unsigned evtIndex )
{

  if( rp->ebp == NULL || rp->ebp->cnt == p->blkIdxAllocCnt )
  {
    cmSdbRspBlk_t* bp = cmLhAllocZ(p->lhH,cmSdbRspBlk_t,1);
    bp->indexV = cmLhAllocZ(p->lhH,unsigned,p->blkIdxAllocCnt);

    if( rp->ebp != NULL )
      rp->ebp->link = bp;

    if( rp->blocks == NULL )
      rp->blocks = bp;

    rp->ebp  = bp;

  }

  assert( rp->ebp!=NULL && rp->ebp->cnt < p->blkIdxAllocCnt );

  rp->ebp->indexV[ rp->ebp->cnt++ ] = evtIndex;
  rp->cnt += 1;
}

void _cmSdbRspRelease( cmSdbRsp_t* rp )
{
  while( rp->blocks != NULL )
  {
    cmSdbRspBlk_t* np = rp->blocks->link;
    cmSdbRspBlk_t* bp;

    if((bp = _cmSdbRspBlkUnlink(rp,rp->blocks)) != NULL )
      _cmSdbRspBlkFree(rp->p,bp);
    
    rp->blocks = np;
  }

  cmLhFree(rp->p->lhH,rp);
}

cmSdbRsp_t*  _cmSdbRspUnlink( cmSdbRsp_t* rp )
{
  cmSdb_t*    p  = rp->p;
  cmSdbRsp_t* dp = p->responses;
  cmSdbRsp_t* pp = NULL;

  for(; dp!=NULL; dp=dp->link)
  {
    if( dp == rp )
    {
      if( pp == NULL )
        p->responses = dp->link;
      else
        pp->link = dp->link;

      return rp;
    }

    pp = dp;
  }

  assert( 0 );

  return NULL;
}


void _cmSdbRspFree( cmSdbRsp_t* rp )
{
  _cmSdbRspUnlink(rp);
  _cmSdbRspRelease(rp);
}

cmSdbRsp_t* _cmSdbRspAlloc( cmSdb_t* p, cmSdbResponseH_t* rhp )
{
  if( cmSdbResponseFree(rhp) != kOkSdbRC )
    return NULL;

  cmSdbRsp_t* rp = cmLhAllocZ(p->lhH,cmSdbRsp_t,1);
  rp->p = p;
  rp->link = p->responses;
  p->responses = rp;

  rhp->h = rp;

  return rp;
}

cmSdbRC_t _cmSdbDestroy( cmSdb_t* p )
{
  cmSdbRC_t rc = kOkSdbRC;

  if( cmCsvFinalize(&p->csvH) != kOkCsvRC )
    rc = cmErrMsg(&p->ctx.err,kCsvFailSdbRC,"CSV file finalize failed.");

  while( p->responses != NULL )
    _cmSdbRspRelease(p->responses);

  cmLHeapDestroy(&p->lhH);
  cmMemFree(p);
  return rc;
}

cmSdbRC_t cmSdbCreate( cmCtx_t* ctx,  cmSdbH_t* hp, const cmChar_t* audioDir, const cmChar_t* csvFn )
{
  cmSdbRC_t rc;
  if((rc = cmSdbDestroy(hp)) != kOkSdbRC )
    return rc;

  cmSdb_t* p = cmMemAllocZ(cmSdb_t,1);
  p->ctx = *ctx;
  p->blkIdxAllocCnt  = 1024;

  cmErrSetup(&p->ctx.err,&ctx->rpt,"sdb");

  if( cmLHeapIsValid( p->lhH = cmLHeapCreate(8192,ctx)) == false )
  {
    rc = cmErrMsg(&p->ctx.err,kLHeapFailSdbRC,"Linked heap mgr. allocation failed.");
    goto errLabel;
  }

  hp->h = p;

  if( csvFn != NULL )
    if((rc = cmSdbLoad(*hp,csvFn)) != kOkSdbRC )
      goto errLabel;

 errLabel:
  if( rc != kOkSdbRC )
    _cmSdbDestroy(p);

  return rc;
}

cmSdbRC_t cmSdbDestroy( cmSdbH_t* hp )
{
  cmSdbRC_t rc = kOkSdbRC;

  if( hp==NULL || cmSdbIsValid(*hp)==false )
    return rc;

  cmSdb_t* p = _cmSdbHandleToPtr(*hp);

  if((rc = _cmSdbDestroy(p)) != kOkSdbRC )
    return rc;

  hp->h = NULL;

  return rc;
}

bool     cmSdbIsValid( cmSdbH_t h )
{ return h.h != NULL; }


cmSdbRC_t _cmSdbSyntaxError(cmSdb_t* p, const cmChar_t* csvFn, unsigned rowIdx, unsigned colIdx, const cmChar_t* colLabel )
{
  return cmErrMsg(&p->ctx.err,kSyntaxErrSdbRC,"A syntax error was found at row %i col %i (label:%s) in '%s'.",rowIdx+1,colIdx+1,cmStringNullGuard(colLabel),cmStringNullGuard(csvFn));
}


cmSdbRC_t cmSdbLoad( cmSdbH_t h, const cmChar_t* csvFn )
{
  cmSdbRC_t rc   = kOkSdbRC;
  unsigned  i;

  cmSdb_t* p = _cmSdbHandleToPtr(h);

  if( cmCsvInitializeFromFile(&p->csvH, csvFn, 0, &p->ctx ) != kOkCsvRC )
  {
    rc = cmErrMsg(&p->ctx.err,kCsvFailSdbRC,"CSV file load fail on '%s'.",cmStringNullGuard(csvFn));
    goto errLabel;
  }

  p->eN = cmCsvRowCount(p->csvH)-1;
  
  // release all the memory held by the linked heap
  cmLHeapClear(p->lhH,true);

  p->eV = cmLhAllocZ(p->lhH,cmSdbEvent_t,p->eN);


  for(i=0; rc==kOkSdbRC && i<p->eN; ++i)
  {
    unsigned rowIdx = i+1;

    if((p->eV[i].uuid = cmCsvCellUInt(p->csvH,rowIdx,kUuidColIdx)) == UINT_MAX )
      rc = _cmSdbSyntaxError(p,csvFn,rowIdx,kUuidColIdx,"uuid");

    if((p->eV[i].baseUuid = cmCsvCellUInt(p->csvH,rowIdx,kBaseUuidColIdx)) == UINT_MAX )
      rc = _cmSdbSyntaxError(p,csvFn,rowIdx,kBaseUuidColIdx,"baseUuid");

    if((p->eV[i].chIdx = cmCsvCellUInt(p->csvH,rowIdx,kChIdxColIdx)) == UINT_MAX )
      rc = _cmSdbSyntaxError(p,csvFn,rowIdx,kChIdxColIdx,"chIdx");
    else
      p->eV[i].chIdx -= 1; // CSV channel index is 1 based

    if((p->eV[i].obi = cmCsvCellUInt(p->csvH,rowIdx,kObiColIdx)) == UINT_MAX )
      rc = _cmSdbSyntaxError(p,csvFn,rowIdx,kObiColIdx,"obi");
    else
      p->eV[i].obi -= 1;

    if((p->eV[i].ibi = cmCsvCellUInt(p->csvH,rowIdx,kIbiColIdx)) == UINT_MAX )
      rc = _cmSdbSyntaxError(p,csvFn,rowIdx,kIbiColIdx,"ibi");
    else
      p->eV[i].ibi -= 1;

    if((p->eV[i].iei = cmCsvCellUInt(p->csvH,rowIdx,kIeiColIdx)) == UINT_MAX )
      rc = _cmSdbSyntaxError(p,csvFn,rowIdx,kIeiColIdx,"obi");
    else
      p->eV[i].iei -= 1;

    if((p->eV[i].oei = cmCsvCellUInt(p->csvH,rowIdx,kOeiColIdx)) == UINT_MAX )
      rc = _cmSdbSyntaxError(p,csvFn,rowIdx,kOeiColIdx,"ibi");
    else
      p->eV[i].oei -= 1;

    if((p->eV[i].src = cmCsvCellText(p->csvH,rowIdx,kSrcColIdx)) == NULL )
      rc = _cmSdbSyntaxError(p,csvFn,rowIdx,kSrcColIdx,"src");

    if((p->eV[i].midi = cmCsvCellInt(p->csvH,rowIdx,kMidiColIdx)) == INT_MAX )
      rc = _cmSdbSyntaxError(p,csvFn,rowIdx,kMidiColIdx,"midi");

    if((p->eV[i].instr = cmCsvCellText(p->csvH,rowIdx,kInstrColIdx)) == NULL )
      rc = _cmSdbSyntaxError(p,csvFn,rowIdx,kInstrColIdx,"instr");

    if((p->eV[i].srate = cmCsvCellUInt(p->csvH,rowIdx,kSrateColIdx)) == UINT_MAX )
      rc = _cmSdbSyntaxError(p,csvFn,rowIdx,kSrateColIdx,"srate");

    if((p->eV[i].chCnt = cmCsvCellUInt(p->csvH,rowIdx,kChCntColIdx)) == UINT_MAX )
      rc = _cmSdbSyntaxError(p,csvFn,rowIdx,kChCntColIdx,"chCnt");

     
    cmCsvCell_t* c;
    if((c = cmCsvCellPtr(p->csvH,rowIdx,kNotesColIdx)) == NULL )
    {
      rc = cmErrMsg(&p->ctx.err,kSyntaxErrSdbRC,"Syntax Error: No 'notes' or 'audio file name' field for row %i in '%s'.",rowIdx+1,cmStringNullGuard(csvFn));
      goto errLabel;
    }
    
    // count the number of 'notes'
    unsigned nn = 0;
    for(; c->rowPtr != NULL; c=c->rowPtr)
      ++nn;

    if( nn > 0 )
    {
      unsigned k  = 0;

      // allocate the 'notes' ptr array - the last entry is set to NULL.
      p->eV[i].notesV = cmLhAllocZ(p->lhH,const cmChar_t*,nn+1);

      // read each note
      for(c=cmCsvCellPtr(p->csvH,rowIdx,kNotesColIdx); c!=NULL&&c->rowPtr!=NULL; c=c->rowPtr,++k)
        if(( p->eV[i].notesV[k] = cmCsvCellText(p->csvH,rowIdx,kNotesColIdx+k)) == NULL )
          rc = _cmSdbSyntaxError(p,csvFn,rowIdx,kNotesColIdx+k,"notes");

      assert(k==nn);
    }

    // read the audio file name
    if((p->eV[i].afn = cmCsvCellText(p->csvH,rowIdx,kNotesColIdx+nn)) == NULL )
      rc = _cmSdbSyntaxError(p,csvFn,rowIdx,kNotesColIdx+nn,"afn");

  }

 errLabel:

  return rc;

}

// Compare 'label' to every string in tV[i] and return true if any comparision is a match.
// If 'subFlV[i]' is set then 'label' must only contain tV[i] as a substring to match.
// If 'negFlV[i]' is set then return true if any comparision is a mismatch.
bool _cmSdbSelectText( const cmSdbEvent_t* r, const cmChar_t** tV, const bool* subFlV, const bool* negFlV, const cmChar_t* label )
{
  unsigned        i;

  if( label == NULL )
    return false;

  if( tV == NULL )
    return true;

  for(i=0; tV[i]!=NULL; ++i)
  {
    bool matchFl = false;
    if( subFlV[i] )
      matchFl = strstr(label,tV[i]) != NULL;
    else
      matchFl = strcmp(tV[i],label)==0;

    if( negFlV[i] )
      matchFl = !matchFl;

    if(matchFl)
      return true;
  }

  return false;
}

unsigned _cmSdbStrVectCnt( const cmChar_t** v )
{
  unsigned n = 0;
  unsigned i = 0;

  if( v == NULL )
    return 0;

  for(i=0; v[i]!=NULL; ++i)
    ++n;
  return n;
}

void _cmSdbStrVectFlags( const cmChar_t** v, bool* sV, bool* nV )
{
  unsigned i = 0;
  if( v == NULL )
    return;

  for(i=0; v[i]!=NULL; ++i)
  {
    nV[i] = false;
    sV[i] = false;

    if( strncmp(v[i],"*!",2)==0 || strncmp(v[i],"!*",2)==0)
    {
      sV[i] = nV[i] = true;
      v[i] += 2;
    }
    else
    {
      if( strncmp(v[i],"!",1)==0 )
      {
        nV[i] = true;
        v[i] += 1;
      }

      if( strncmp(v[i],"*",1)==0 )
      {
        sV[i] = true;
        v[i] += 1;
      }
    }

    
  }
}

cmSdbRC_t cmSdbSelect( 
    cmSdbH_t         h,
    double           srate,
    const cmChar_t** instrV,
    const cmChar_t** srcV,
    const cmChar_t** notesV,
    double           minDurSec,
    double           maxDurSec,
    unsigned         minChCnt,
    cmSdbResponseH_t* rhp )
{
  cmSdbRC_t rc = kOkSdbRC;
  cmSdb_t*  p  = _cmSdbHandleToPtr(h);
  unsigned  i;

  cmSdbRsp_t* rp   = _cmSdbRspAlloc(p,rhp);

  // get the length of each string vector
  unsigned    srcN = _cmSdbStrVectCnt(srcV);
  unsigned    insN = _cmSdbStrVectCnt(instrV);
  unsigned    notN = _cmSdbStrVectCnt(notesV);

  // allocate flag vectors
  bool srcSubFlV[ srcN ];
  bool srcNegFlV[ srcN ];
  bool insSubFlV[ insN ];
  bool insNegFlV[ insN ];
  bool notSubFlV[ notN ];
  bool notNegFlV[ notN ];
  
  // fill the flag vectors
  _cmSdbStrVectFlags(srcV,  srcSubFlV,srcNegFlV);
  _cmSdbStrVectFlags(instrV,insSubFlV,insNegFlV);
  _cmSdbStrVectFlags(notesV,notSubFlV,notNegFlV);

  for(i=0; i<p->eN; ++i)
  {
    const cmSdbEvent_t* r      = p->eV + i;
    double              durSec = (double)r->srate * (r->oei - r->obi);
    unsigned            j;

    if( srate!=0 && srate!=r->srate )
      continue;

    if( durSec < minDurSec || (maxDurSec!=0 && maxDurSec < durSec) )
      continue;

    if( minChCnt!=0 && r->chCnt > minChCnt )
      continue;

    if( !_cmSdbSelectText(r,srcV,srcSubFlV,srcNegFlV,r->src) )
      continue;
    
    if( !_cmSdbSelectText(r,instrV,insSubFlV,insNegFlV,r->instr) )
      continue;

    if( r->notesV != NULL )
      for(j=0; r->notesV[j]!=NULL; ++j)
        if( _cmSdbSelectText(r,notesV,notSubFlV,notNegFlV,r->notesV[j]) == true )
          break;

    if( r->notesV[j]==NULL )
      continue;

    
    _cmSdbRspInsertIndex(p,rp,i);
  }
  
  return rc;
}

cmSdbRC_t cmSdbSelectChPairs( cmSdbH_t h, const cmSdbEvent_t* ep, cmSdbResponseH_t* rhp )
{
  cmSdbRC_t   rc = kOkSdbRC;
  cmSdb_t*    p  = _cmSdbHandleToPtr(h);
  cmSdbRsp_t* rp = _cmSdbRspAlloc(p,rhp);
  unsigned    i;

  // for each channel of this event
  for(i=0; i<ep->chCnt; ++i)
  {
     // if i channel is not the known events channel
    if( ep->chIdx != i )
    {
      unsigned j;

      // examine each record
      for(j=0; j<p->eN; ++j)
        // if eV[j] shares a baseUuid but is on a different channel than *ep  ...
        if( p->eV[j].baseUuid == ep->baseUuid && p->eV[j].chIdx==i )
        {
          // .. then a match has been found
          _cmSdbRspInsertIndex(p,rp,j);
          break;
        }

      if( j== p->eN )
      {
        rc = cmErrMsg(&p->ctx.err,kChPairNotFoundSdbRC,"The channel pair associated with 'id:%i instr:%s src:%s ch index:%i could not be found.",ep->uuid,cmStringNullGuard(ep->instr),cmStringNullGuard(ep->src),ep->chIdx);
      }
    }
  }

  return rc;

}


unsigned cmSdbResponseCount( cmSdbResponseH_t rh )
{
  cmSdbRsp_t* rp = _cmSdbRspHandleToPtr(rh);
  return rp->cnt;
}

const cmSdbEvent_t* cmSdbResponseEvent( cmSdbResponseH_t rh, unsigned index )
{
  cmSdbRsp_t* rp = _cmSdbRspHandleToPtr(rh);

  if( index >= rp->cnt )
    return NULL;

  cmSdbRspBlk_t* bp = rp->blocks;
  unsigned i;
  for(i=0; bp!=NULL; i+=bp->cnt,bp=bp->link)
    if( i <= index && index < (i + bp->cnt) )
      return rp->p->eV + bp->indexV[index-i];
  
  cmErrMsg(&rp->p->ctx.err,kInvalidRspIdxSdbRC,"Invalid query response index=%i.",index);
  return NULL;
}

bool cmSdbResponseIsValid( cmSdbResponseH_t rh )
{ return rh.h != NULL; }

cmSdbRC_t cmSdbResponseFree( cmSdbResponseH_t* rhp )
{
  cmSdbRC_t rc = kOkSdbRC;

  if( rhp == NULL || cmSdbResponseIsValid(*rhp)==false )
    return rc;

  cmSdbRsp_t* rp = _cmSdbRspHandleToPtr(*rhp);

  _cmSdbRspFree(rp);

  rhp->h = NULL;

  return rc;
}

void   cmSdbResponsePrint( cmSdbResponseH_t rh, cmRpt_t* rpt )
{
  unsigned n = cmSdbResponseCount(rh);
  unsigned i;
  for(i=0; i<n; ++i)
  {
    const cmSdbEvent_t* e = cmSdbResponseEvent(rh,i);
    if( e != NULL )
      cmRptPrintf(rpt,"%6i %6i %2i %12i %12i %12i %12i %2i %6i %2i %10s %15s\n",
        e->uuid,e->baseUuid,e->chIdx,e->obi,e->ibi,e->iei,e->oei,e->midi,e->srate,e->chCnt,
        cmStringNullGuard(e->src), cmStringNullGuard(e->instr) );
  }
}

cmSdbRC_t  cmSdbSyncChPairs( cmSdbH_t h )
{
  cmSdbRC_t rc = kOkSdbRC;
  cmSdb_t* p = _cmSdbHandleToPtr(h);
  unsigned i;
  // for each multi-channel event
  for(i=0; i<p->eN; ++i)
    if(p->eV[i].chCnt > 1 )
    {
      const cmSdbEvent_t* ep = p->eV + i;
      unsigned iV[ep->chCnt];
      unsigned j,k;

      // load iV[] with the event indexes of the channel pairs
      for(j=0,k=0; j<p->eN && k<ep->chCnt; ++j)
        if( p->eV[j].baseUuid == ep->baseUuid )
        {
          assert( p->eV[j].chIdx < ep->chCnt );

          iV[p->eV[j].chIdx] = j;
          ++k;
        }

      if( k != ep->chCnt )
        rc = cmErrMsg(&p->ctx.err,kChPairNotFoundSdbRC,"The channel pair associated with 'id:%i instr:%s src:%s ch index:%i could not be found.",ep->uuid,cmStringNullGuard(ep->instr),cmStringNullGuard(ep->src),ep->chIdx);
      else
      {
        unsigned mobi = ep->obi;
        unsigned mibi = ep->ibi;
        unsigned miei = ep->iei;
        unsigned moei = ep->oei;

        // get the min onsets and max offsets
        for(j=0; j<ep->chCnt; ++j)
        {
          mobi = cmMin(mobi,p->eV[ iV[j] ].obi);
          mibi = cmMin(mibi,p->eV[ iV[j] ].ibi);
          miei = cmMax(miei,p->eV[ iV[j] ].iei);
          moei = cmMax(moei,p->eV[ iV[j] ].oei);
        }

        // set the onsets to the min onset / offsets to max offsets 
        for(j=0; j<ep->chCnt; ++j)
        {
          p->eV[ iV[j] ].obi = mobi;
          p->eV[ iV[j] ].ibi = mibi;
          p->eV[ iV[j] ].iei = miei;
          p->eV[ iV[j] ].oei = moei;
        }
      }
    }
 
  return rc;
}


cmSdbRC_t cmSdbTest( cmCtx_t* ctx )
{
  cmSdbRC_t       rc       = kOkSdbRC;
  cmSdbH_t        h        = cmSdbNullHandle;
  const cmChar_t* audioDir = "/home/kevin/media/audio";
  const cmChar_t* csvFn    = "/home/kevin/temp/sdb0/sdb_master.csv";
  cmErr_t         err;

  cmErrSetup(&err,&ctx->rpt,"sdb test");

  if((rc = cmSdbCreate(ctx,  &h, audioDir, csvFn )) != kOkSdbRC )
  {
    rc = cmErrMsg(&err,rc,"sdb create failed.");
    goto errLabel;
  }

  if((rc = cmSdbSyncChPairs(h)) != kOkSdbRC )
  {
    rc = cmErrMsg(&err,rc,"sdb sync-ch-pairs failed.");
    goto errLabel;
  }

  if(0)
  {
    cmSdbResponseH_t rH = cmSdbResponseNullHandle;

    const cmChar_t* instrV[] = { "*viol", NULL };

    if((rc = cmSdbSelect(h,0,instrV,NULL,NULL,0,0,0,&rH)) != kOkSdbRC )
    {
      rc = cmErrMsg(&err,rc,"sdb query failed.");
      goto errLabel;
    }


    cmSdbResponsePrint(rH,&ctx->rpt);

    cmSdbResponseFree(&rH);
  }

 errLabel:
  if((rc = cmSdbDestroy(&h)) != kOkSdbRC )
    rc = cmErrMsg(&err,rc,"sdb destroy failed.");

  return rc;
}
