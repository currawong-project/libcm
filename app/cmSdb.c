#include "cmGlobal.h"
#include "cmFloatTypes.h"
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
#include "cmMath.h"
#include "cmTime.h"
#include "cmMidi.h"
#include "cmVectOpsTemplateMain.h"
#include "cmAudioFile.h"
#include "cmFileSys.h"

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

typedef struct cmSdbSeqBlk_str
{
  cmSdbSeqEvent_t*        eV;
  unsigned                cnt;
  struct cmSdbSeqBlk_str* link;
} cmSdbSeqBlk_t;

typedef struct cmSdbSeq_str
{
  struct cmSdb_str*    p;
  cmSdbSeqBlk_t*       blocks;
  cmSdbSeqBlk_t*       ebp;
  unsigned             cnt;        // total count of events in all blocks
  unsigned             chCnt;      // max(chIdx)+1 of all events
  double               minDurSec; // min dur of all events
  double               maxDurSec; // max dur of all events
  struct cmSdbSeq_str* link;
} cmSdbSeq_t;

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
  cmCtx_t       ctx;
  cmLHeapH_t    lhH;
  cmCsvH_t      csvH;
  cmSdbEvent_t* eV;
  unsigned      eN;
  cmChar_t*     audioDir;
  unsigned      blkIdxAllocCnt;
  unsigned      blkEvtAllocCnt;
  cmSdbRsp_t*   responses;
  cmSdbSeq_t*   seqs;
} cmSdb_t;

cmSdbH_t         cmSdbNullHandle         = cmSTATIC_NULL_HANDLE;
cmSdbResponseH_t cmSdbResponseNullHandle = cmSTATIC_NULL_HANDLE;
cmSdbSeqH_t      cmSdbSeqNullHandle      = cmSTATIC_NULL_HANDLE;

cmSdb_t* _cmSdbHandleToPtr( cmSdbH_t h )
{
  cmSdb_t* p = (cmSdb_t*)h.h;
  assert( p != NULL );
  return p;
}

void      _cmSdbRspFree( cmSdbRsp_t* );
cmSdbRC_t _cmSdbSeqFree( cmSdbSeq_t* );

cmSdbRC_t _cmSdbDestroy( cmSdb_t* p )
{
  cmSdbRC_t rc = kOkSdbRC;

  if( cmCsvFinalize(&p->csvH) != kOkCsvRC )
    rc = cmErrMsg(&p->ctx.err,kCsvFailSdbRC,"CSV file finalize failed.");

  while( p->responses != NULL )
    _cmSdbRspFree(p->responses);

  while( p->seqs != NULL )
    _cmSdbSeqFree(p->seqs);

  cmLHeapDestroy(&p->lhH);
  cmMemFree(p);
  return rc;
}

cmSdbRC_t cmSdbCreate( cmCtx_t* ctx,  cmSdbH_t* hp, const cmChar_t* csvFn, const cmChar_t* audioDir )
{
  cmSdbRC_t rc;
  if((rc = cmSdbDestroy(hp)) != kOkSdbRC )
    return rc;

  cmSdb_t* p = cmMemAllocZ(cmSdb_t,1);
  p->ctx = *ctx;
  p->blkIdxAllocCnt = 1024;
  p->blkEvtAllocCnt = 1024;

  cmErrSetup(&p->ctx.err,&ctx->rpt,"sdb");

  if( cmLHeapIsValid( p->lhH = cmLHeapCreate(8192,ctx)) == false )
  {
    rc = cmErrMsg(&p->ctx.err,kLHeapFailSdbRC,"Linked heap mgr. allocation failed.");
    goto errLabel;
  }

  hp->h = p;

  if( csvFn != NULL )
    if((rc = cmSdbLoad(*hp,csvFn,audioDir)) != kOkSdbRC )
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


cmSdbRC_t cmSdbLoad( cmSdbH_t h, const cmChar_t* csvFn, const cmChar_t* audioDir )
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

  // store the audio directory
  if( cmTextLength(audioDir) )
    p->audioDir = cmLhAllocStr(p->lhH,audioDir);
  else
  {
    cmLhFree(p->lhH,&p->audioDir);
    p->audioDir = NULL;
  }
 errLabel:

  return rc;

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

const cmSdbEvent_t* _cmSdbEvent( cmSdb_t* p, unsigned uuid )
{
  unsigned i;
  for(i=0; i<p->eN; ++i)
    if( p->eV[i].uuid == uuid )
      return p->eV + i;

  return NULL;
}

const cmSdbEvent_t* cmSdbEvent( cmSdbH_t h, unsigned uuid )
{ 
  cmSdb_t* p = _cmSdbHandleToPtr(h);
  return _cmSdbEvent(p,uuid);
}

//================================================================================================================================
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

cmSdbRsp_t* _cmSdbRspAlloc( cmSdb_t* p )
{
  cmSdbRsp_t* rp = cmLhAllocZ(p->lhH,cmSdbRsp_t,1);
  rp->p = p;
  rp->link = p->responses;
  p->responses = rp;


  return rp;
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
    const unsigned*  pitchV,
    double           minDurSec,
    double           maxDurSec,
    unsigned         minChCnt,
    cmSdbResponseH_t* rhp )
{
  cmSdbRC_t rc;

  if((rc = cmSdbResponseFree(rhp)) != kOkSdbRC )
    return rc;

  cmSdb_t*    p  = _cmSdbHandleToPtr(h);
  cmSdbRsp_t* rp = _cmSdbRspAlloc(p);
  unsigned    i;

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

    if( pitchV != NULL )
    {
      for(j=0; pitchV[j]!=kInvalidMidiPitch; ++j)
        if( pitchV[j] == r->midi )
          break;

      if( pitchV[j] != r->midi )
        continue;
    }

    if( r->notesV != NULL )
    {
      for(j=0; r->notesV[j]!=NULL; ++j)
        if( _cmSdbSelectText(r,notesV,notSubFlV,notNegFlV,r->notesV[j]) == true )
          break;

      if( r->notesV[j]==NULL )
        continue;
    }

    
    _cmSdbRspInsertIndex(p,rp,i);
  }
  
  rhp->h = rp;

  if(rc != kOkSdbRC )
    _cmSdbRspFree(rp);
  
  return rc;
}

cmSdbRC_t _cmSdbSelectChPairs( cmSdb_t* p, const cmSdbEvent_t* ep, cmSdbResponseH_t* rhp )
{
  cmSdbRC_t   rc;
  if((rc = cmSdbResponseFree(rhp)) != kOkSdbRC )
    return rc;

  cmSdbRsp_t* rp = _cmSdbRspAlloc(p);
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

  rhp->h = rp;

  return rc;
}

cmSdbRC_t cmSdbSelectChPairs( cmSdbH_t h, const cmSdbEvent_t* ep, cmSdbResponseH_t* rhp )
{
  cmSdb_t*    p  = _cmSdbHandleToPtr(h);
  return _cmSdbSelectChPairs( p, ep,rhp );
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

//================================================================================================================================
cmSdbSeq_t* _cmSdbSeqHandleToPtr( cmSdbSeqH_t sh )
{
  cmSdbSeq_t* sp = (cmSdbSeq_t*)sh.h;
  assert(sp !=NULL );
  return sp;
}

void _cmSdbSeqInsertEvent( cmSdbSeq_t* sp, unsigned uuid, unsigned chIdx, double begSecs, double durSecs )
{
  cmSdb_t* p = sp->p;

  // if no block has been allocated or the current block is full
  if( sp->ebp == NULL || sp->ebp->cnt >= p->blkEvtAllocCnt )
  {
    // allocate a new seq block recd
    cmSdbSeqBlk_t* bp = cmLhAllocZ(sp->p->lhH,cmSdbSeqBlk_t,1);

    // allocate a seq evt array
    bp->eV = cmLhAllocZ(sp->p->lhH,cmSdbSeqEvent_t,p->blkEvtAllocCnt);    
      
    // link in the block recd
    if( sp->ebp != NULL )
      sp->ebp->link = bp;

    if( sp->blocks == NULL )
    {
      sp->blocks = bp;
      sp->minDurSec = durSecs;
      sp->maxDurSec = durSecs;
    }

    sp->ebp  = bp;
  }

  assert( sp->ebp != NULL && sp->ebp->cnt < p->blkEvtAllocCnt );
  
  // get the next seq evt recd to fill
  cmSdbSeqEvent_t* ep = sp->ebp->eV + sp->ebp->cnt;

  // fill the seq evt recd
  ep->uuid   = uuid;
  ep->begSec = begSecs;
  ep->durSec = durSecs;
  ep->outChIdx  = chIdx;
  ep->gain   = 1.0;

  // incr the seq evt cnt
  sp->ebp->cnt  += 1;
  sp->cnt       += 1;
  sp->chCnt      = cmMax(sp->chCnt,chIdx+1);
  sp->minDurSec  = cmMin(sp->minDurSec,durSecs);
  sp->maxDurSec  = cmMax(sp->maxDurSec,durSecs);
}


// unlink a sequence record from p->seqs.
cmSdbSeq_t* _cmSdbSeqUnlink( cmSdbSeq_t* sp )
{
  cmSdb_t*    p  = sp->p;
  cmSdbSeq_t* cp = p->seqs;
  cmSdbSeq_t* pp = NULL;

  for(; cp!=NULL; cp=cp->link)
  {
    if( cp == sp )
    {
      if( pp == NULL )
        p->seqs = sp->link;
      else
        pp->link = sp->link;

      return sp;
    }

    pp = cp;
  }

  assert(0);
  return NULL;
}

// free a sequence record
cmSdbRC_t  _cmSdbSeqFree( cmSdbSeq_t* sp )
{
  cmSdb_t* p = sp->p;

  // unlink this seq. record from p->seqs
  if( _cmSdbSeqUnlink(sp) == NULL )
    return cmErrMsg(&p->ctx.err,kAssertFailSdbRC,"Sequence unlink failed.");

  // release the seq blocks held by the sequence
  while( sp->blocks != NULL )
  {
    cmSdbSeqBlk_t* np = sp->blocks->link;

    cmLhFree(p->lhH,sp->blocks->eV);
    cmLhFree(p->lhH,sp->blocks);

    sp->blocks = np;
  }


  cmLhFree(p->lhH,sp);

  return kOkSdbRC;
}

// allocate a sequence record
cmSdbSeq_t*  _cmSdbSeqAlloc( cmSdb_t* p )
{
  cmSdbSeq_t* sp = cmLhAllocZ(p->lhH,cmSdbSeq_t,1);

  sp->p    = p;
  sp->link = p->seqs;
  p->seqs  = sp;

  return sp;
}

cmSdbRC_t  _cmSdbStoreSeqEvent(
  cmSdb_t*         p,
  cmSdbSeq_t*      sp,
  cmSdbResponseH_t rh,  
  unsigned         ri,
  unsigned         seqChCnt,
  double           begSecs,
  double           limitEvtDurSecs,
  double*          durSecsRef )
{
  cmSdbRC_t rc = kOkSdbRC;

  // retrieve the event record
  const cmSdbEvent_t* ep;
  if((ep = cmSdbResponseEvent(rh,ri)) == NULL )
  {
    rc = cmErrMsg(&p->ctx.err,kRspEvtNotFoundSdbRC,"A response event could not be found during random sequence generation.");
    goto errLabel;
  }

  cmSdbResponseH_t rh0           = cmSdbResponseNullHandle;
  unsigned         rn0           = 0;
  unsigned         ci            = 0;
  double           maxEvtDurSecs = 0;

  // locate the channel pairs for 'ep'.
  if( seqChCnt>1 && ep->chCnt>1 )
  {
    if( _cmSdbSelectChPairs(p, ep, &rh0 ) != kOkSdbRC )
    {
      rc = cmErrMsg(&p->ctx.err,kChPairNotFoundSdbRC,"A response event could not find channel pairs during random sequence generation.");
      goto errLabel;
    }

    rn0 = cmSdbResponseCount(rh0);
  }

  while(1)
  {

    // calculate the event duration
    double durSecs = (double)(ep->oei - ep->obi)/ep->srate;

    // truncate the event if it is longer than limitEvtDurSecs
    if( limitEvtDurSecs!=0 && durSecs>limitEvtDurSecs )
      durSecs = cmMin(limitEvtDurSecs,durSecs);

    // track the longest event
    maxEvtDurSecs = cmMax(maxEvtDurSecs,durSecs);

    // store the sequence event
    _cmSdbSeqInsertEvent(sp,ep->uuid,ci,begSecs,durSecs);

    // incr the output ch index
    ++ci;
        
    // if all the out ch's are filled or the sample event has no more channels
    if( ci >= seqChCnt || ci-1 >= rn0 )
      break;

    // get the next channel pair
    if((ep = cmSdbResponseEvent(rh0,ci-1)) == NULL )
    {
      rc = cmErrMsg(&p->ctx.err,kRspEvtNotFoundSdbRC,"A channel pair response event could not be found during random sequence generation.");
      goto errLabel;
    }

  } // for each sample event pair

 errLabel:

  if( durSecsRef != NULL )
    *durSecsRef = maxEvtDurSecs;

  cmSdbResponseFree(&rh0);

  return rc;

}

cmSdbRC_t cmSdbSeqRand( 
    cmSdbResponseH_t rh, 
    unsigned         seqDurSecs,
    unsigned         seqChCnt, 
    unsigned         minEvtPerSec, 
    unsigned         maxEvtPerSec, 
    cmSdbSeqH_t*     shp )
{
  cmSdbRC_t   rc;

  if((rc = cmSdbSeqFree(shp)) != kOkSdbRC )
    return rc;

  cmSdbRsp_t* rp = _cmSdbRspHandleToPtr(rh);
  cmSdb_t*    p  = rp->p;
  cmSdbSeq_t* sp = _cmSdbSeqAlloc(p);

  if( seqChCnt < 1 )
    return cmErrMsg(&p->ctx.err,kInvalidArgSdbRC,"The random sequence generator channel count parameter must be non-zero.");

  if( seqDurSecs <= 0 )
    return cmErrMsg(&p->ctx.err,kInvalidArgSdbRC,"The random sequence generator signal duration must be greater than 0.");

  if( maxEvtPerSec < minEvtPerSec )
    return cmErrMsg(&p->ctx.err,kInvalidArgSdbRC,"The random sequence generator max. events per second must be greater or equal to the min. events per second.");
   
  if((rc = cmSdbSeqFree(shp)) != kOkSdbRC )
    return rc;

  unsigned  rn = cmSdbResponseCount(rh);
  unsigned sec;
  for(sec=0; sec<seqDurSecs; sec+=1 )
  {

    // calcuate the number of events to initiate during this second
    unsigned en = cmRandUInt(minEvtPerSec,maxEvtPerSec);
    unsigned ei;
    for(ei=0; ei<en; ++ei)
    {
      // select an event index
      unsigned ri = cmRandUInt(0,rn-1);

      // double select a start time for this event
      double begSecs = sec + cmRandDouble(0.0,1.0);
      double maxEvtDurSecs = 0;

      if((rc = _cmSdbStoreSeqEvent(p,sp,rh,ri,seqChCnt,begSecs,maxEvtDurSecs,NULL)) != kOkSdbRC )
        goto errLabel;

    } // for each event init'd during this second
     
    
  } // for each second

  shp->h = sp;

 errLabel:
  if( rc != kOkSdbRC )
    _cmSdbSeqFree(sp);

  return rc;
}

cmSdbRC_t cmSdbSeqSerial(
    cmSdbResponseH_t rh,
    unsigned         seqChCnt,
    double           gapSec,    
    double           maxEvtDurSec,
    cmSdbSeqH_t*     shp )
{ 
  cmSdbRC_t   rc;

  if((rc = cmSdbSeqFree(shp)) != kOkSdbRC )
    return rc;

  cmSdbRsp_t* rp      = _cmSdbRspHandleToPtr(rh);
  cmSdb_t*    p       = rp->p;
  cmSdbSeq_t* sp      = _cmSdbSeqAlloc(p);
  unsigned    n       = cmSdbResponseCount(rh);
  double      begSecs = 0;
  unsigned    ri;
  
  for(ri=0; ri<n; ++ri)
  {
    double durSecs = 0;

    if((rc = _cmSdbStoreSeqEvent(p,sp,rh,ri,seqChCnt,begSecs,maxEvtDurSec,&durSecs)) != kOkSdbRC )
      goto errLabel;

    // offset to next event
    begSecs += durSecs + gapSec;

  }

  shp->h = sp;

 errLabel:
  if(rc != kOkSdbRC )
    _cmSdbSeqFree(sp);

  return rc;
}

cmSdbRC_t cmSdbSeqChord(
  cmSdbResponseH_t* rhp,
  unsigned          rn,
  unsigned          seqChCnt,
  unsigned          maxEvtDurSec,
  cmSdbSeqH_t*      shp )
{
  cmSdbRC_t   rc = kOkSdbRC;

  assert( shp != NULL );

  if( rn == 0 )
    return rc;

  cmSdbResponseH_t rh = rhp[0];
  cmSdbRsp_t*      rp = _cmSdbRspHandleToPtr(rh);
  cmSdb_t*         p  = rp->p;
  cmSdbSeq_t*      sp = _cmSdbSeqAlloc(p);
  unsigned         i;

  if((rc = cmSdbSeqFree(shp)) != kOkSdbRC )
    return rc;

  // for each chord note
  for(i=0; i<rn; ++i)
  {
    // get the query response handle for this note
    rh = rhp[i];
    rp = _cmSdbRspHandleToPtr(rh);

    // verify that all query responses were drawn from the same cmSdbH_t handle.
    if( rp->p != p )
    {
      rc = cmErrMsg(&p->ctx.err,kAssertFailSdbRC,"All chord query response handle must be derived from the same cmSdbH_t handle.");
      goto errLabel;
    }

    // pick one event at random from the response
    unsigned         n       = cmSdbResponseCount(rh);
    unsigned         rei     = cmRandUInt(0,n-1);

    // all notes start at time: 0.0.
    double           begSecs = 0.0;
    
    // store the sequence event
    if((rc = _cmSdbStoreSeqEvent(p,sp,rh,rei,seqChCnt,begSecs,maxEvtDurSec,NULL)) != kOkSdbRC )
      goto errLabel;
    
  }

  shp->h = sp;

 errLabel:
  if(rc != kOkSdbRC )
    _cmSdbSeqFree(sp);

  return rc;
}



bool cmSdbSeqIsValid( cmSdbSeqH_t sh )
{ return sh.h != NULL; }

cmSdbRC_t cmSdbSeqFree( cmSdbSeqH_t* shp )
{
  cmSdbRC_t rc = kOkSdbRC;
  if( shp==NULL || cmSdbSeqIsValid(*shp)==false )
    return rc;

  cmSdbSeq_t* sp = _cmSdbSeqHandleToPtr(*shp);

  if((rc = _cmSdbSeqFree(sp)) != kOkSdbRC )
    return rc;

  shp->h = NULL;

  return rc;
}

unsigned cmSdbSeqCount( cmSdbSeqH_t sh )
{
  if( cmSdbSeqIsValid(sh)==false )
    return 0;

  cmSdbSeq_t* sp = _cmSdbSeqHandleToPtr(sh);
  return sp->cnt;
}

const cmSdbSeqEvent_t* cmSdbSeqEvent( cmSdbSeqH_t sh, unsigned index )
{
  cmSdbSeq_t* sp = _cmSdbSeqHandleToPtr(sh);
  if( index >= sp->cnt )
    return NULL;

  cmSdbSeqBlk_t* bp = sp->blocks;
  unsigned i;
  for(i=0; bp!=NULL; i+=bp->cnt,bp=bp->link)
    if( i <= index && index < (i + bp->cnt) )
      return bp->eV + index-i;
  
  cmErrMsg(&sp->p->ctx.err,kInvalidSeqIdxSdbRC,"Invalid sequence event index=%i.",index);

  return NULL;
}

const cmSdbEvent_t* cmSdbSeqSdbEvent( cmSdbSeqH_t sh, unsigned index )
{
  const cmSdbSeqEvent_t* ep;
  if((ep = cmSdbSeqEvent(sh,index)) == NULL )
    return NULL;

  cmSdbSeq_t* sp = _cmSdbSeqHandleToPtr(sh);

  return _cmSdbEvent(sp->p,ep->uuid);
}

double cmSdbSeqDurSeconds( cmSdbSeqH_t sh )
{
  cmSdbSeq_t* sp    = _cmSdbSeqHandleToPtr(sh);

  cmSdbSeqBlk_t* bp = sp->blocks;
  while( bp!=NULL && bp->link!=NULL )
    bp=bp->link;

  if( bp == NULL )
    return 0;

  cmSdbSeqEvent_t* ep = bp->eV + bp->cnt - 1;

  return ep->begSec + ep->durSec;  
}

double cmSdbSeqSampleRate( cmSdbSeqH_t sh )
{
  unsigned n = cmSdbSeqCount(sh);
  unsigned i;
  const cmSdbEvent_t* ep;

  for(i=0; i<n; ++i)
    if((ep = cmSdbSeqSdbEvent(sh,i)) != NULL && ep->srate != 0 )
      return ep->srate;

  return 0;
}

cmSdbRC_t cmSdbSeqToAudio( 
  cmSdbSeqH_t  sh, 
  unsigned     decayMs, 
  double       noiseDb, 
  double       normFact,
  cmSample_t** signalRef, 
  unsigned*    sigSmpCntRef )
{
  assert( signalRef!=NULL && sigSmpCntRef!=NULL);

  *signalRef    = NULL;
  *sigSmpCntRef = 0;

  cmSdbRC_t   rc      = kOkSdbRC;
  cmSdbSeq_t* sp      = _cmSdbSeqHandleToPtr(sh);
  cmSdb_t*    p       = sp->p;
  unsigned    qN      = cmSdbSeqCount(sh);
  double      durSecs = cmSdbSeqDurSeconds(sh);
  double      srate   = cmSdbSeqSampleRate(sh);

  assert(sp->maxDurSec>=sp->minDurSec);

  // verify that sequence events exist
  if( qN==0 || durSecs==0 || sp->chCnt==0 || sp->maxDurSec==0)
    return rc;

  // validate the sample rate
  if( srate == 0 )
    return cmErrMsg(&p->ctx.err,kAssertFailSdbRC,"The sample rate of the sequence could not be determined.");

  unsigned    sN    = (unsigned)floor(srate * (durSecs + 0.25)); // output signal sample count + 1/4 second of silence
  unsigned    dN    = (unsigned)floor(srate * decayMs / 1000.0); // decay env. sample count
  unsigned    tN    = (unsigned)floor(srate * sp->maxDurSec);    // length of longest audio event in samples
  cmSample_t* s     = cmMemAllocZ(cmSample_t,sN*sp->chCnt);      // allocate the outputsignal buffer   
  cmSample_t* t     = cmMemAllocZ(cmSample_t,tN*sp->chCnt);      // audio event read buffer
  cmSample_t* d     = NULL;
  cmSample_t* chBuf[ sp->chCnt ];
  unsigned    i;

  // fill the channel buffers
  for(i=0; i<sp->chCnt; ++i)
    chBuf[i] = t + (i*tN);

  // if a decay rate was specified
  if( dN > 0 )
  {
    d = cmMemAllocZ(cmSample_t,dN);  // allocate the decay env. buffer
    cmVOS_LinSpace(d,dN,1.0,0.0);    // calc. a decay envelope
    cmVOS_PowVS(d,dN,4.0);
  }

  // if a noise floor was specified
  if( noiseDb != 0 )
  {
    // fill the signal with low level white noise
    cmVOS_Random(s,sN,-1.0,1.0); 
    cmVOS_MultVS(s,sN,pow(10.0,-fabs(noiseDb)/20.0));
  }

  // for each sequence event 
  for(i=0; rc==kOkSdbRC && i<qN; ++i)
  {
    const cmSdbSeqEvent_t* qep;
    const cmSdbEvent_t*    ep;
  
    // get the sequence event record
    if((qep = cmSdbSeqEvent(sh,i)) == NULL )
    {
      rc = cmErrMsg(&p->ctx.err,kAssertFailSdbRC,"Unable to retrieve the sequence event at index %i.",i);
      goto errLabel;
    }

    // get the audio event record 
    if((ep = _cmSdbEvent(p,qep->uuid)) == NULL)
    {
      rc = cmErrMsg(&p->ctx.err,kAssertFailSdbRC,"Unable to retrieve the sample event with uuid:%i.",qep->uuid);
      goto errLabel;
    }

    unsigned          begFrmIdx = floor(srate * qep->begSec );  // dest. index into output signal
    unsigned          frmCnt    = floor(srate * qep->durSec );  // seq. event dur in samples
    const cmChar_t*   afn       = NULL;                         // audio event file name
    unsigned          actFrmCnt = 0;                            // actual count of samples read from the audio event file
    cmAudioFileInfo_t afInfo;                                   // audio file info. record

    // form the audio event file name
    if((afn = cmFsMakeFn(p->audioDir,ep->afn,NULL,NULL))==NULL)
    {
      rc = cmErrMsg(&p->ctx.err,kFileSysFailSdbRC,"Unable to form the file name for %s/%s.",cmStringNullGuard(p->audioDir),cmStringNullGuard(ep->afn));
      goto errLabel;
    }
    
    assert(ep->oei-ep->obi>0 );

    // read the audio event from the file into t[]
    if( cmAudioFileGetSample(afn, ep->obi, cmMin(tN,cmMin(frmCnt,ep->oei-ep->obi)), 0, ep->chCnt, chBuf, &actFrmCnt, &afInfo, p->ctx.err.rpt ) != kOkAfRC )
    {
      rc = cmErrMsg(&p->ctx.err,kFileSysFailSdbRC,"Audio event read failed for event uuid:%i in '%s'.",qep->uuid,cmStringNullGuard(afn));
      goto doneLabel;
    }

    // 'actFrmCnt' now holds the length of the event signal

    // verify that the audio event sample rate matches the sequence srate
    if( afInfo.srate != srate )
      cmErrWarnMsg(&p->ctx.err,kAssertFailSdbRC,"The sample rate (%f) of audio event uuid:%i in '%s' does not match the sequence sample rate:%f.",afInfo.srate,qep->uuid,cmStringNullGuard(afn),srate);

    // if a decay rate was specified
    if( dN > 0 )
    {
      unsigned ti = 0;  // start of decay in t[]  
      unsigned di = 0;  // start of decay in d[]  

      if( actFrmCnt > dN )  
        ti = actFrmCnt - dN; // decay func is applied to end of audio event
      else
        di = dN - actFrmCnt; // decay func is longer than audio event (shorten it)

      unsigned mn = dN - di;  // decay function length
      unsigned j;

      // apply the decay function
      for(j=0; j<sp->chCnt; ++j)
        cmVOS_MultVV(t + (j*tN) +ti , mn, d+di);
    }

    // normalize the event signal
    if( normFact != 0 )
      cmVOS_NormToAbsMax(t,actFrmCnt,normFact);

    
    // verify the the signal event falls inside the output signal
    if( begFrmIdx >= sN )
      rc = cmErrMsg(&p->ctx.err,kAssertFailSdbRC,"A sequence event start time falls after the end of the sequence signal.  This should never happen.");
    else
    {
      // if the event signal goes past the end of the signal - truncate the event
      if( begFrmIdx + actFrmCnt > sN )
        actFrmCnt = sN - begFrmIdx;

      // sum the event signal into the output signal
      cmVOS_AddVV(s + (qep->outChIdx*sN) + begFrmIdx,actFrmCnt,t);
    }
    
  doneLabel:
    cmFsFreeFn(afn);
    
  }
  
  *signalRef = s;
  *sigSmpCntRef = sN;
 errLabel:
  if( rc != kOkSdbRC )
    cmMemFree(s);

  cmMemFree(d);
  cmMemFree(t);
  return rc;
}

cmSdbRC_t cmSdbSeqToAudioFn( 
    cmSdbSeqH_t sh, 
    unsigned    decayMs,   
    double      noiseDb,   
    double      evtNormFact,
    double      sigNormFact, 
    const cmChar_t* fn,
    unsigned    bitsPerSample
    )
{
  cmSdbRC_t   rc    = kOkSdbRC;
  cmSample_t* s     = NULL;
  unsigned    sN    = 0;
  cmSdbSeq_t* sp    = _cmSdbSeqHandleToPtr(sh);
  cmSdb_t*    p     = sp->p;
  double      srate = cmSdbSeqSampleRate(sh);
  unsigned    i;

  // fill s[sN] with the sequence audio signal
  if((rc = cmSdbSeqToAudio(sh,decayMs,noiseDb,evtNormFact,&s,&sN)) != kOkSdbRC )
    return rc;

  // if no audio signal was created there is nothing to do
  if( sN == 0 )
    return rc;

  // the sample rate was already check by cmSdbSeqToAudio().
  assert(srate != 0 && s != NULL);

  // if requested normalize the signal
  if( sigNormFact != 0 )
    cmVOS_NormToAbsMax(s,sN*sp->chCnt,sigNormFact);

  // fill the channel buffer
  cmSample_t* chBuf[ sp->chCnt ];
  for(i=0; i<sp->chCnt; ++i)
    chBuf[i] = s + (i*sN);
  
  // write the signal to an audio file
  if((rc = cmAudioFileWriteFileFloat(fn, srate, bitsPerSample, sN, sp->chCnt, chBuf, p->ctx.err.rpt )) != kOkAfRC )
  {
    rc = cmErrMsg(&p->ctx.err,kAudioFileFailSdbRC,"The sequence audio file '%s' could not be created.",cmStringNullGuard(fn));
    goto errLabel;
  }

 errLabel:
  cmMemFree(s);
  return rc;
} 


void  cmSdbSeqPrint( cmSdbSeqH_t sh, cmRpt_t* rpt )
{
  unsigned               i;
  unsigned               n  = cmSdbSeqCount(sh);
  cmSdbSeq_t*            sp = _cmSdbSeqHandleToPtr(sh);
  const cmSdbSeqEvent_t* ep;

  cmRptPrintf(rpt,"evt cnt:%i ch cnt:%i  dur min:%f max:%f \n",sp->cnt,sp->chCnt,sp->minDurSec,sp->maxDurSec);
  cmRptPrintf(rpt," uuid   ch   beg     dur     gain  \n");
  cmRptPrintf(rpt,"------- --- ------- ------- -------\n");
  for(i=0; i<n; ++i)
    if((ep = cmSdbSeqEvent(sh,i)) != NULL )
      cmRptPrintf(rpt,"%7i %3i %7.3f %7.3f %7.3f\n",ep->uuid,ep->outChIdx,ep->begSec,ep->durSec,ep->gain );
        
}



cmSdbRC_t cmSdbTest( cmCtx_t* ctx )
{
  cmSdbRC_t       rc       = kOkSdbRC;
  cmSdbH_t        h        = cmSdbNullHandle;
  const cmChar_t* audioDir = "/home/kevin/media/audio";
  const cmChar_t* csvFn    = "/home/kevin/temp/sdb0/sdb_master.csv";
  cmErr_t         err;

  cmErrSetup(&err,&ctx->rpt,"sdb test");

  if((rc = cmSdbCreate(ctx,  &h, csvFn, audioDir )) != kOkSdbRC )
  {
    rc = cmErrMsg(&err,rc,"sdb create failed.");
    goto errLabel;
  }

  if((rc = cmSdbSyncChPairs(h)) != kOkSdbRC )
  {
    rc = cmErrMsg(&err,rc,"sdb sync-ch-pairs failed.");
    goto errLabel;
  }

  if(1)
  {
    cmSdbResponseH_t rH = cmSdbResponseNullHandle;
    cmSdbSeqH_t      sH = cmSdbSeqNullHandle;

    const cmChar_t* instrV[] = { "violin", NULL };
    const cmChar_t* srcV[]   = { "ui", NULL };
    const cmChar_t* notesV[] = { "!vibrato", NULL };

    if((rc = cmSdbSelect(h,0,instrV,srcV,notesV,NULL,0,0,0,&rH)) != kOkSdbRC )
    {
      rc = cmErrMsg(&err,rc,"sdb query failed.");
      goto errLabel;
    }

    //cmSdbResponsePrint(rH,&ctx->rpt);

    unsigned seqDurSecs = 15;
    unsigned seqChCnt   = 2; 
    unsigned sel        = 2;
    switch( sel )
    {
      case 0:
        {
          unsigned minEvtPerSec = 1; 
          unsigned maxEvtPerSec = 5; 

          if((rc = cmSdbSeqRand(rH,seqDurSecs,seqChCnt,minEvtPerSec,maxEvtPerSec,&sH)) != kOkSdbRC )
          {
            rc = cmErrMsg(&err,rc,"sdb random sequence generation failed.");
            goto errLabel;
          }
        }
        break;

      case 1:
        {
          double gapSec       = 0.1;
          double maxEvtDurSec = 1.0;
          
          if((rc = cmSdbSeqSerial(rH,seqChCnt,gapSec,maxEvtDurSec,&sH)) != kOkSdbRC )
          {
            rc = cmErrMsg(&err,rc,"sdb serial sequence generation failed.");
            goto errLabel;
          }
        }
        break;

      case 2:
        {
          cmSdbResponseH_t rhV[] = { rH, rH, rH };
          unsigned rN            = sizeof(rhV)/sizeof(rhV[0]);
          double   maxEvtDurSec  = 1.0;

          if((rc = cmSdbSeqChord(rhV,rN,seqChCnt,maxEvtDurSec,&sH)) != kOkSdbRC )
          {
            rc = cmErrMsg(&err,rc,"sdb chord sequence generation failed.");
            goto errLabel;
          }
        }
        break;
    }

    cmSdbSeqPrint(sH,&ctx->rpt);

    const cmChar_t* afn           = "/home/kevin/temp/aaa.aif";
    unsigned        decayMs       = 50;
    double          noiseDb       = -70.0;
    double          evtNormFact   = 0; //0.7;
    double          sigNormFact   = 0.7; //0.7;
    unsigned        bitsPerSample = 16;

    if((rc = cmSdbSeqToAudioFn(sH,decayMs,noiseDb,evtNormFact,sigNormFact,afn,bitsPerSample)) != kOkSdbRC )
    {
      rc = cmErrMsg(&err,rc,"sdb sequence audio file generation failed.");
      goto errLabel;
    }

    cmSdbSeqFree(&sH);
    cmSdbResponseFree(&rH);
  }

 errLabel:
  if((rc = cmSdbDestroy(&h)) != kOkSdbRC )
    rc = cmErrMsg(&err,rc,"sdb destroy failed.");

  return rc;
}
