//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmSymTbl.h"
#include "cmLinkedHeap.h"

cmSymTblH_t cmSymTblNullHandle = cmSTATIC_NULL_HANDLE;

enum
{
  kDynStFl = 0x01
};

typedef struct cmSymLabel_str
{
  unsigned        flags;
  const cmChar_t* label;
} cmSymLabel_t;


struct cmSym_str;

typedef struct cmSymAvail_str
{
  unsigned          id;
  struct cmSym_str* linkPtr;
} cmSymAvail_t;

typedef struct cmSym_str
{
  union
  {
    cmSymLabel_t label;
    cmSymAvail_t avail;
  } u;
} cmSym_t;

typedef struct cmSymBlock_t
{
  cmSym_t*             base;   // block base - contains cmSymTbl_t.symPerBlock cmSym_t records
  unsigned             cnt;    // cout of symbols actually used in this block
  struct cmSymBlock_t* link;   // next block
} cmSymBlock_t;

typedef struct
{
  cmSymTblH_t   parentH;        // parent symbols table
  cmLHeapH_t    heapH;          // symbol label storage
  cmSymBlock_t* first;          // pointer to head of symbol block chain
  cmSymBlock_t* last;           // pointer to last symbol block in chain
  unsigned      blkCnt;         // count of blocks on chain
  unsigned      symCnt;         // total count of symbols controlled by this symbol (does not include parent symbols)
  unsigned      baseSymId; 
  unsigned      symPerBlock;
  cmSym_t*      availPtr;
} cmSymTbl_t;

cmSymTbl_t* _cmSymTblHandleToPtr( cmSymTblH_t h )
{
  cmSymTbl_t* stp = (cmSymTbl_t*)h.h;
  assert( stp != NULL );
  return stp;
}

cmSymBlock_t*  _cmSymTblAllocateBlock( cmSymTbl_t* stp )
{
  // allocate the new block
  cmSymBlock_t* sbp = (cmSymBlock_t*)cmMemMallocZ( sizeof(cmSymBlock_t) + (stp->symPerBlock * sizeof(cmSym_t)));

  // initialize the new block control recd
  sbp->base = (cmSym_t*)(sbp+1);
  sbp->cnt  = 0;
  sbp->link = NULL;

  // add the new block control recd to the end of the block chain
  if( stp->last == NULL )
    stp->first = sbp;
  else
    stp->last->link = sbp;

  // the new block control recd always becomes the last recd in the chain
  stp->last = sbp;

  ++stp->blkCnt;

  return sbp;
}

bool _cmSymTblIsSymbolRemoved( cmSymTbl_t* stp, unsigned symId )
{
  const cmSym_t* sp = stp->availPtr;
  for(; sp != NULL; sp=sp->u.avail.linkPtr)
    if( sp->u.avail.id == symId )
      return true;
  return false;
}

unsigned _cmSymTblLabelToId( cmSymTbl_t* stp, const char* label )
{
  cmSymBlock_t* sbp = stp->first;

  unsigned symId = stp->baseSymId;

  while( sbp != NULL )
  {
    cmSym_t* sp = sbp->base;
    cmSym_t* ep = sbp->base + sbp->cnt;

    for(; sp<ep; ++sp,++symId)
    {
      if( _cmSymTblIsSymbolRemoved(stp, symId ) == false )
        if( strcmp( sp->u.label.label, label ) == 0 )
          return symId;
    }
    sbp = sbp->link;
  }

  return cmInvalidId; 
}

cmSym_t* _cmSymTblIdToSymPtr( cmSymTbl_t* stp, unsigned symId )
{
  if( cmSymTblIsValid(stp->parentH) && cmSymTblIsValidId( stp->parentH, symId ) )
    return _cmSymTblIdToSymPtr( _cmSymTblHandleToPtr(stp->parentH), symId );

  symId -= stp->baseSymId;

  unsigned    n   = symId / stp->symPerBlock;
  unsigned    i   = symId % stp->symPerBlock;

  if( n >= stp->blkCnt )
    return NULL;

  cmSymBlock_t* sbp = stp->first;

  unsigned j;
  for(j=0; j<n; ++j)
    sbp = sbp->link;

  if( i >= sbp->cnt )
    return NULL;

  return sbp->base + i;
}

cmSymTblH_t cmSymTblCreate(           cmSymTblH_t parentH, unsigned baseSymId, cmCtx_t* ctx )
{
  cmSymTblH_t h;
  cmSymTbl_t* stp = cmMemAllocZ( cmSymTbl_t, 1 );

  stp->heapH       = cmLHeapCreate( 2048, ctx );
  stp->symPerBlock = 3;
  stp->baseSymId   = baseSymId;
  stp->parentH     = parentH;

  _cmSymTblAllocateBlock( stp );


  h.h = stp;
  return h;
}

void        cmSymTblDestroy( cmSymTblH_t* hp )
{
  if( hp==NULL || hp->h == NULL )
    return;

  cmSymTbl_t*   stp = _cmSymTblHandleToPtr(*hp);

  assert( stp != NULL );

  cmSymBlock_t* sbp = stp->first;
  while( sbp != NULL )
  {
    cmSymBlock_t* t = sbp;
    sbp = sbp->link;
    cmMemFree(t);
  }

  cmLHeapDestroy(&stp->heapH); 

  cmMemFree(hp->h);

  hp->h = NULL;

}

unsigned    cmSymTblRegister( cmSymTblH_t h, const char* label, bool staticFl )
{
  cmSymTbl_t* stp   = _cmSymTblHandleToPtr(h);
  unsigned    symId;
  unsigned    flags = 0;
  cmSym_t*    sp    = NULL;

  // check for the label in the local symbol table
  if((symId = _cmSymTblLabelToId( stp, label )) != cmInvalidId )
    return symId;

  // check for the label in the parent symbol table
  if( cmSymTblIsValid(stp->parentH) )
    if((symId = _cmSymTblLabelToId( _cmSymTblHandleToPtr(stp->parentH), label)) != cmInvalidId )
      return symId;

  // if the label is not static then create a copy of it on the local heap
  if( !staticFl )
  {
    char* cp = (char*)cmLHeapAlloc( stp->heapH, strlen(label) + 1 );
    strcpy(cp,label);
    label = cp;
    flags |= kDynStFl;
  }

  // if there are no previosly removed symbols available
  if( stp->availPtr == NULL )
  {
    cmSymBlock_t*sbp = stp->last;

    // if the last block is full
    if( sbp->cnt == stp->symPerBlock )
      sbp = _cmSymTblAllocateBlock(stp);

    // the last block must now have an empty slot
    assert( sbp->cnt < stp->symPerBlock );


    unsigned idx = sbp->cnt++;
    sp = sbp->base + idx;
    //sbp->base[ idx ].u.label.label = label;
    //sbp->base[ idx ].u.label.flags = flags;

    // calculate the symbol id
    symId = stp->baseSymId + ((stp->blkCnt-1) * stp->symPerBlock) + sbp->cnt - 1;
  }
  else  // there are previously removed symbols available
  {
    sp            = stp->availPtr;        // get the next avail symbol
    stp->availPtr = sp->u.avail.linkPtr;  // take it off the avail list
    symId         = sp->u.avail.id;       // store the new symbol's id
  }

  // setup the symbol record
  sp->u.label.label = label;
  sp->u.label.flags = flags;

  // verify that the new symId does not already belong to the parent
  assert( cmSymTblIsValid(stp->parentH)==false ? 1 : cmSymTblLabel( stp->parentH, symId)==NULL );

  ++stp->symCnt;

  return symId;
  
}

unsigned    cmSymTblRegisterSymbol(       cmSymTblH_t h, const char* label )
{ return cmSymTblRegister( h, label, false ); }

unsigned    cmSymTblRegisterStaticSymbol( cmSymTblH_t h, const char* label )
{ return cmSymTblRegister( h, label, true ); }

unsigned    cmSymTblRegisterVFmt( cmSymTblH_t h, const cmChar_t* fmt, va_list vl )
{
  unsigned n = vsnprintf(NULL,0,fmt,vl);
  cmChar_t b[n+1];
  vsnprintf(b,n,fmt,vl);
  return cmSymTblRegister(h,fmt,vl);

}

unsigned    cmSymTblRegisterFmt( cmSymTblH_t h, const cmChar_t* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  unsigned id = cmSymTblRegisterVFmt(h,fmt,vl);
  va_end(vl);
  return id;
}


bool        cmSymTblRemove( cmSymTblH_t h, unsigned symId )
{
  cmSymTbl_t* stp = _cmSymTblHandleToPtr(h);

  cmSym_t* sp;

  if((sp = _cmSymTblIdToSymPtr(stp,symId)) == NULL )
    return false;

  if( cmIsFlag(sp->u.label.flags,kDynStFl))
    cmLHeapFree(stp->heapH,(void*)sp->u.label.label);

  sp->u.avail.id      = symId;
  sp->u.avail.linkPtr = stp->availPtr;
  stp->availPtr       = sp;
  

  return true;
}


const char* cmSymTblLabel( cmSymTblH_t h, unsigned    symId )
{

  cmSymTbl_t* stp = _cmSymTblHandleToPtr(h);

  cmSym_t* sp;

  if((sp = _cmSymTblIdToSymPtr(stp,symId)) == NULL )
    return NULL;

  return sp->u.label.label;
}

unsigned    cmSymTblId(                   cmSymTblH_t h, const char* label )
{
  cmSymTbl_t* stp = _cmSymTblHandleToPtr(h);
  unsigned     id;
  if((id=_cmSymTblLabelToId(stp,label)) == cmInvalidId )
    if( cmSymTblIsValid(stp->parentH))
      return _cmSymTblLabelToId( _cmSymTblHandleToPtr(stp->parentH), label );
  return id;
}

bool        cmSymTblIsValid(               cmSymTblH_t h )
{ return h.h != NULL; }

bool        cmSymTblIsValidId(            cmSymTblH_t h, unsigned symId )
{
  cmSymTbl_t* stp = _cmSymTblHandleToPtr(h);
  return stp->baseSymId <= symId && symId < (stp->baseSymId + stp->symCnt);
}

void        cmSymTblReport(               cmSymTblH_t h )
{
  cmSymTbl_t*   stp = _cmSymTblHandleToPtr(h);
  cmSymBlock_t* sbp = stp->first;
  unsigned i=0, j=0, symId = stp->baseSymId;

  printf("blks:%i syms:%i\n", stp->blkCnt, stp->symCnt );

  for(; sbp != NULL; sbp=sbp->link,++i)
    for(j=0; j<sbp->cnt; ++j,++symId)
    {
      bool remFl =  _cmSymTblIsSymbolRemoved(stp, symId );
      printf("blk:%i sym:%i id:%i label:%s\n",i,j,symId,remFl ? "<removed>" : sbp->base[j].u.label.label);
    }

}

//( { label:cmSymTblEx }
//
//  cmSymTblTest() gives a usage example for the symbol table component.
//

void cmSymTblTest( cmCtx_t* ctx )
{
  unsigned    baseSymId = 100;
  unsigned    i;
  unsigned    n = 10;
  unsigned    idArray[n];

  // create a symbol table
  cmSymTblH_t h = cmSymTblCreate( cmSymTblNullHandle, baseSymId, ctx );

  if( cmSymTblIsValid(h) == false )
  {
    cmRptPrintf(&ctx->rpt,"Symbol table creation failed.");
    return;
  }

  // generate and register some symbols
  for(i=0; i<n; ++i)
  {
    bool staticFl = false;
    char str[10];
    snprintf(str,9,"sym%i",i);
    idArray[i] = cmSymTblRegister( h, str, staticFl );    
  }

  // remove  the fourth symbol generated
  cmSymTblRemove( h, baseSymId+3 );
  
  // print the symbol table
  cmSymTblReport(h);

  // iterate through the symbol table
  for(i=0; i<n; ++i)
  {
    const cmChar_t* lbl = cmSymTblLabel(h,idArray[i]);

    if( lbl == NULL )
      cmRptPrintf(&ctx->rpt,"%i <removed>\n",i);
    else
      cmRptPrintf(&ctx->rpt,"%i %i==%i %s \n",i,idArray[i],cmSymTblId(h,lbl),lbl);
  }

  // release the symbol table
  cmSymTblDestroy(&h);

  return;
}
//)
