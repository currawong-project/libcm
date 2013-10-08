#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmMem.h"
#include "cmFloatTypes.h"
#include "cmMath.h"
#include "cmThread.h"

// Block layout
//
// |a       |b   |c   |d       |e        |f
// |xx...xxx|yyyy|zzzz|gggggggg|ddd...ddd|gggggggg|
//
//

// xxxx = alignment (prefix) bytes - size is given by yyyy (may contain up to alignByteCnt-1 bytes).
// yyyy = count of bytes preceding yyyy
// zzzz = count of data bytes
// gggg = guard area (size is fixed by guardByteCnt arg. to cmMemInitialize())
// dddd = data area
//
// d = e - guardByteCnt
// c = d - sizeof(unsigned)
// b = c - sizeof(unsigned)
// a = b - yyyy
//
// Notes:
// 1) The smallest allocation is dataByteCnt +  (2*sizeof(unsigned)
// 2) If allocByteCnt > 0 then the smallest allocation is allocByteCnt + (2*sizeof(unsigned)) + dataByteCnt.
//

#define  _cmMmDataToByteCntPtr(    dp, gbc )      (dp==NULL?NULL:(((char*)(dp))-(gbc)-sizeof(unsigned)))
#define  _cmMmDataToByteCnt(       dp, gbc )      (dp==NULL?0   :(*(unsigned*)_cmMmDataToByteCntPtr(dp,gbc)))
#define  _cmMmDataToPrefixCntPtr(  dp, gbc )      (dp==NULL?NULL:( _cmMmDataToByteCntPtr(dp,gbc) - sizeof(unsigned)))
#define  _cmMmDataToPrefixCnt(     dp, gbc )      (dp==NULL?0   :(*(unsigned*)_cmMmDataToPrefixCntPtr(dp,gbc)))
#define  _cmMmDataToBasePtr(       dp, gbc )      (dp==NULL?NULL:(_cmMmDataToPrefixCntPtr(dp,gbc) - _cmMmDataToPrefixCnt(dp,gbc)))
#define  _cmMmDataToTotalByteCnt(  dp, gbc )      (dp==NULL?0   :(sizeof(unsigned) + (2*gbc) + _cmMmDataToByteCnt(dp,gbc) + (_cmMmDataToPrefixCnt(dp,gbc) + sizeof(unsigned))))
#define  _cmMmDataToPreGuardPtr(   dp, gbc )      (dp==NULL?NULL:(((char*)(dp))-(gbc)))
#define  _cmMmDataToPostGuardPtr(  dp, gbc )      (dp==NULL?NULL:(((char*)(dp))+_cmMmDataToByteCnt(dp,gbc)))


enum
{
  kFreedMmFl    = 0x01,
  kDblFreeMmFl  = 0x02
};

typedef struct cmMmRecd_str
{
  unsigned             uniqueId;     // 
  void*                dataPtr;      // dataPtr may be NULL if the assoc'd alloc request was for 0 bytes.
  unsigned             dataByteCnt;  // data area bytes on original allocation
  unsigned             fileLine;
  char*                fileNameStr;
  char*                funcNameStr;
  unsigned             flags;
  struct cmMmRecd_str* linkPtr;
} cmMmRecd_t;

typedef struct cmMmStr_str
{
  struct cmMmStr_str* link;
  char*               str;
} cmMmStr_t;

typedef struct
{
  cmRpt_t*        rpt;
  cmErr_t         err;
  cmMmRecd_t*     listPtr;
  unsigned        nextId;
  unsigned        alignByteCnt;
  unsigned        guardByteCnt;
  cmAllocMmFunc_t allocFunc;
  cmFreeMmFunc_t  freeFunc; 
  void*           funcArgPtr;
  char            uninitChar;
  char            freeChar;
  char            guardChar; 
  unsigned        flags;
  cmMmStr_t*      fnList;
  cmMmStr_t*      funcList;
} cmMm_t;

cmMmH_t cmMmNullHandle = { NULL };

cmMm_t* _cmMmHandleToPtr( cmMmH_t h )
{
  assert(h.h != NULL );
  return (cmMm_t*)h.h;
}

cmMmRC_t _cmMmCheckGuards( cmMm_t* p, cmMmRecd_t* rp )
{
  // it is possible that dataPtr is NULL if zero bytes was requested at allocation
  if( rp->dataPtr == NULL )
    return kOkMmRC;

  // check the pre data area guard space
  char* pp = _cmMmDataToPreGuardPtr( rp->dataPtr, p->guardByteCnt );
  char* ep = pp + p->guardByteCnt;

  for(;pp<ep; ++pp)
    if( *pp != p->guardChar )
      return kGuardCorruptMmRC;


  // check the post data area guard space 
  pp = _cmMmDataToPostGuardPtr( rp->dataPtr, p->guardByteCnt );
  ep = pp + p->guardByteCnt;

  for(;pp<ep; ++pp)
    if( *pp != p->guardChar )
      return kGuardCorruptMmRC;

  // if this block was freed and the kFillFreedMmFl was set then check for write-after-free
  if( cmIsFlag(rp->flags,kFreedMmFl) && cmIsFlag(p->flags,kFillFreedMmFl) )
  {
    pp = rp->dataPtr;
    ep = pp + _cmMmDataToByteCnt(pp,p->guardByteCnt);

    for(; pp<ep; ++pp)
      if( *pp != p->freeChar )
        return kWriteAfterFreeMmRC;    
  }
  
  return kOkMmRC;
}

cmMmRecd_t* _cmMmFindRecd( cmMm_t* p, const void* dataPtr )
{
  cmMmRecd_t* rp = p->listPtr;

  while( rp != NULL )
  {
    if( rp->dataPtr == dataPtr )
      break;

    rp = rp->linkPtr;
  }

  return rp;
}

cmMmRC_t _cmMmFree( cmMm_t* p, void* dataPtr, cmMmRecd_t* rp )
{
  cmMmRC_t rc = kOkMmRC;

  if( p->freeFunc(p->funcArgPtr, _cmMmDataToBasePtr(dataPtr,p->guardByteCnt)) == false )
  {
    if( rp == NULL )
      rc = cmErrMsg(&p->err,kFreeFailMmRC,"Memory free failed on data area at %p.",dataPtr);
    else
      rc = cmErrMsg(&p->err,kFreeFailMmRC,"Memory free failed on data area at %p. Allocated with id:%i at %s line:%i %s.",dataPtr, rp->uniqueId,rp->funcNameStr,rp->fileLine,rp->fileNameStr); 
  }

  return rc;
}

cmMmRC_t _cmMmFreeP( cmMm_t* p, void* dataPtr )
{
  if( dataPtr == NULL )
    return kOkMmRC;

  cmMmRecd_t* rp = p->listPtr;
  cmMmRC_t    rc = kOkMmRC;

  // if we are tracking alloc's and free's
  if( cmIsFlag(p->flags,kTrackMmFl) )
  {
    // locate the tracking recd
    rp = _cmMmFindRecd(p,dataPtr);

    // if no tracking recd was found
    if( rp == NULL )
      return cmErrMsg(&p->err,kMissingRecdMmRC,"Unable to locate a tracking record associated with released data area pointer:%p.",dataPtr);
    /*
    if( rp->uniqueId == 176690 )
    {
      cmErrMsg(&p->err,kOkMmRC,"Breakpoint for memory free id:%i.",rp->uniqueId);
    }
    */

    // if this block was already freed then this is a double free
    if( cmIsFlag(rp->flags,kFreedMmFl) )
      rp->flags = cmSetFlag(rp->flags,kDblFreeMmFl);
    else
      // otherwise fill the freed block with the freeChar (if requested)
      if( cmIsFlag(p->flags,kFillFreedMmFl) )
        memset(dataPtr, p->freeChar, _cmMmDataToByteCnt(dataPtr,p->guardByteCnt));

    // mark the block as having been freed
    rp->flags = cmSetFlag(rp->flags,kFreedMmFl);
  }

  // if we are not deferring free to the finalize stage ... then free the memory here
  if( cmIsFlag(p->flags,kDeferFreeMmFl) == false)
    rc = _cmMmFree(p,dataPtr,rp);
  
  return rc;
}

void* _cmMmAllocate(cmMm_t* p, void* orgDataPtr, unsigned newByteCnt, unsigned flags )
{
  unsigned abc        = cmIsFlag(flags,kAlignMmFl) ? p->alignByteCnt : 0;
  unsigned gbc        = p->guardByteCnt;
  char*    bp         = NULL;
  char*    dp         = NULL;
  char*    idp        = NULL;
  unsigned orgByteCnt = 0;
  unsigned prefixByteCnt = 0;

  // Determine the total allocation block size including the auxillary data.
  //
  //                      alignment bytes     +  data_byte_cnt   + guard bytes + actual data
  unsigned ttlByteCnt =  abc+sizeof(unsigned) + sizeof(unsigned) + (2*gbc)     + newByteCnt;

  // if this is a reallocation
  if( orgDataPtr != NULL )
  {
    // asking to reallocate a block with zero bytes free's the original block and returns NULL
    // (this is in keeping with realloc() behaviour)
    if( newByteCnt == 0 )
    {
      _cmMmFreeP(p,orgDataPtr);
      return NULL;
    }

    // get the size of the currently allocated block
    orgByteCnt = _cmMmDataToByteCnt(orgDataPtr,gbc);

    // if the requested data area is <= the alloc'd data area
    if( newByteCnt <= orgByteCnt)
    {
      // if preservation was requested simply return with the original data area ptr
      if( cmIsFlag(flags,kPreserveMmFl) )
        return orgDataPtr;

      // otherwise initialze the data area
      dp = orgDataPtr;
      goto initLabel;
    }

    // expansion was requested 

  }  

  // if an empty data area was requested
  if( newByteCnt == 0 )
    return NULL;

  //
  // A new data block must be allocated
  //

  // allocate the memory block via a callback
  if((bp = p->allocFunc(p->funcArgPtr,ttlByteCnt)) == NULL )
  {
    cmErrMsg(&p->err,kAllocFailMmRC,"Attempt to allocate %i bytes failed.",ttlByteCnt);
    goto errLabel;
  }

  // make the data area offset: data_byte_cnt + guard bytes
  dp = bp + (2*sizeof(unsigned)) + gbc;

  if( abc )
  {
    unsigned long alignMask = abc - 1;

    // alignment requires an additional offset
    dp += abc;

    // align the data area to the specified boundary
    char* ndp = ((char*)((unsigned long)dp & (~alignMask)));

    prefixByteCnt = abc - (dp - ndp);

    dp = ndp;
  }

  // set the prefix byteCnt
  *(unsigned*)_cmMmDataToPrefixCntPtr(dp,gbc) = prefixByteCnt;

  // set the data area byte cnt
  *(unsigned*)_cmMmDataToByteCntPtr(dp,gbc) = newByteCnt;

  // uncomment this line to print memory layout information for each block
  //printf("prefix:%i prefix*:%p bytes:%i bytes*:%p base:%p==%p data:%p\n",_cmMmDataToPrefixCnt(dp,gbc),_cmMmDataToPrefixCntPtr(dp,gbc),_cmMmDataToByteCnt(dp,gbc),_cmMmDataToByteCntPtr(dp,gbc),_cmMmDataToBasePtr(dp,gbc),bp,dp);
    
  // set the guard bytes
  if( gbc )
  {
    void* gp = _cmMmDataToPreGuardPtr(  dp,gbc);
    if( gp != NULL )
      memset(gp, p->guardChar, gbc);

    gp = _cmMmDataToPostGuardPtr( dp,gbc);
    if( gp != NULL )
      memset(gp, p->guardChar, gbc);
  }
    

 initLabel:

  //
  // initialize the new data area
  //
  
  idp = dp;

  // if this is a reallocation with expansion ...
  if( orgDataPtr != NULL && newByteCnt > orgByteCnt )
  {
    // ... and preservation was requested
    if( cmIsFlag(flags,kPreserveMmFl) )
    {
      // copy original data into the new block
      memcpy(idp,orgDataPtr,orgByteCnt);

      idp        += orgByteCnt;
      newByteCnt -= orgByteCnt;
    }

    _cmMmFreeP(p,orgDataPtr); // free the original data block
  }

  // if zeroing was requested
  if( cmIsFlag(flags,kZeroMmFl))
    memset(idp,0,newByteCnt);
  else
    // if uninitialized data should be set to a known character
    if( cmIsFlag(p->flags,kFillUninitMmFl) )
      memset(idp,p->uninitChar,newByteCnt);

    
  return dp;

 errLabel:
  return NULL;
}


cmMmRC_t cmMmInitialize( 
  cmMmH_t*        hp, 
  cmAllocMmFunc_t allocFunc, 
  cmFreeMmFunc_t  freeFunc, 
  void*           funcArgPtr,
  unsigned        guardByteCnt, 
  unsigned        alignByteCnt, 
  unsigned        flags,
  cmRpt_t*        rpt )
{
  cmMm_t* p;
  cmErr_t err;
  cmErrSetup(&err,rpt,"Memory Manager");

  // validate alignByteCnt
  if(alignByteCnt>0 && cmIsPowerOfTwo(alignByteCnt)==false)
    return cmErrMsg(&err,kParamErrMmRC,"The 'alignByteCnt' parameter must be a power of two.");
    
  // allocate the main object
  if((p= calloc(1,sizeof(cmMm_t))) == NULL )
    return cmErrMsg(&err,kObjAllocFailMmRC,"Object allocation failed.");

  // setting kDeferFreeMmFl only makes sense when kTrackMmFl is also set
  if(cmIsFlag(flags,kTrackMmFl)==false && cmIsFlag(flags,kDeferFreeMmFl)==true)
  {
    cmErrMsg(&err,kParamErrMmRC,"The flag 'kDeferFreeMmFl' may only be set if the 'kTrackMmFl' is also set. 'kDeferFreeMmFl' is begin disabled.");
    flags = cmClrFlag(flags,kDeferFreeMmFl);
  }

  cmErrClone(&p->err,&err);
  p->rpt          = rpt;
  p->alignByteCnt = alignByteCnt;
  p->guardByteCnt = guardByteCnt;
  p->allocFunc    = allocFunc;
  p->freeFunc     = freeFunc;
  p->funcArgPtr   = funcArgPtr;
  p->uninitChar   = 0x55;  // non-zeroed data areas are automatically filled w/ this value on allocation
  p->freeChar     = 0x33;  // data areas are overwritten with this value on free
  p->guardChar    = 0xAA;  // guard areas are set with this value

  p->flags        = flags;

  hp->h = p;

  return kOkMmRC;
}

void _cmMmFreeStrList( cmMmStr_t* cp )
{
  while( cp!=NULL )
  {
    cmMmStr_t* np = cp->link;
    free(cp->str);
    free(cp);
    cp = np;
  }

}

cmMmRC_t cmMmFinalize( cmMmH_t* hp )
{
  cmMm_t* p;
  cmMmRC_t rc = kOkMmRC;

  if( hp == NULL || cmMmIsValid(*hp)==false )
    return kOkMmRC;

  p = _cmMmHandleToPtr(*hp);

  cmMmRecd_t* rp = p->listPtr;
  while( rp != NULL )
  {
    cmMmRecd_t* tp = rp->linkPtr;
    cmMmRC_t    rrc;

    if( cmIsFlag(p->flags,kDeferFreeMmFl) )
      if((rrc = _cmMmFree(p,rp->dataPtr,rp)) != kOkMmRC )
        rc = rrc;

    free(rp);
    rp = tp;
  }

  _cmMmFreeStrList(p->fnList);
  _cmMmFreeStrList(p->funcList);

  free(p);
  hp->h = NULL;

  return rc;
}

unsigned cmMmGuardByteCount(  cmMmH_t h )
{
  cmMm_t* p = _cmMmHandleToPtr(h);
  return p->guardByteCnt;
}

unsigned cmMmAlignByteCount(  cmMmH_t h )
{
  cmMm_t* p = _cmMmHandleToPtr(h);
  return p->alignByteCnt;
}

unsigned cmMmInitializeFlags( cmMmH_t h )
{
  cmMm_t* p = _cmMmHandleToPtr(h);
  return p->flags;
}

bool     cmMmIsValid( cmMmH_t h )
{ return h.h != NULL; }

// Allocate and/or return a pointer to a stored string.
char* _cmMmAllocStr( cmMmStr_t** listPtrPtr, const char* str )
{
  if( str == NULL )
    str = "";

  cmMmStr_t* lp = *listPtrPtr; // get ptr to first recd in list
  cmMmStr_t* cp = lp;          // init current ptr

  // find 'str' in the list
  for(; cp!=NULL; cp=cp->link)
    if( strcmp(cp->str,str) == 0 )
      break;

  // 'str' was not found - create a new string recd
  if( cp == NULL )
  {
    cp          =  calloc(1,sizeof(cmMmStr_t));
    cp->str     = strdup(str);
    cp->link    = lp;
    *listPtrPtr = cp;
  }

  return cp->str;
}

void* cmMmAllocate(     
  cmMmH_t               h, 
  void*                 orgDataPtr, 
  unsigned              newEleCnt,
  unsigned              newEleByteCnt, 
  enum cmMmAllocFlags_t flags, 
  const char*           fileName, 
  const char*           funcName, 
  unsigned              fileLine )
{
   cmMm_t*  p          = _cmMmHandleToPtr(h); 
   unsigned newByteCnt = newEleCnt * newEleByteCnt;
   void*    ndp        = _cmMmAllocate(p,orgDataPtr,newByteCnt,flags);
   
   /*
   if( p->nextId == 189114 )
   {
     cmErrMsg(&p->err,kOkMmRC,"Breakpoint for memory allocation id:%i.",p->nextId);
   }

   if( (long long)_cmMmDataToBasePtr(ndp,p->guardByteCnt) == 0x7fffed8d0b40 )
   {
     cmErrMsg(&p->err,kOkMmRC,"Breakpoint for memory allocation id:%i.",p->nextId);
   }
   */


   // if we are tracking changes
   if( cmIsFlag(p->flags,kTrackMmFl) )
   {
     //
     // NOTE: it is possible that ndp is NULL if newByteCnt == 0.
     // 

     cmMmRecd_t* rp = NULL;

     // if a new memory block was allocated
     if( orgDataPtr == NULL || orgDataPtr != ndp )
     {
       // allocate a new tracking recd
       if( (rp =  calloc(1,sizeof(cmMmRecd_t))) == NULL )
       {
         cmErrMsg(&p->err,kTrkAllocFailMmRC,"Unable to allocate a tracking record for %s line:%i %s.",funcName,fileLine,fileName);
         return ndp;
       }

       cmThUIntIncr(&p->nextId,1);

       // initialize the new tracking recd
       rp->dataPtr      = ndp;
       rp->dataByteCnt  = newByteCnt;
       rp->fileLine     = fileLine;
       rp->fileNameStr  = _cmMmAllocStr( &p->fnList, fileName ); 
       rp->funcNameStr  = _cmMmAllocStr( &p->funcList, funcName);
       rp->flags        = 0;
       rp->uniqueId     = p->nextId;

       cmMmRecd_t *oldp, *newp;
       do
       {
         oldp         = p->listPtr;
         newp         = rp;
         rp->linkPtr  = p->listPtr;
       }while(!cmThPtrCAS(&p->listPtr,oldp,newp));

       assert( _cmMmCheckGuards(p,rp) == kOkMmRC );

     }
     else // a reallocation occurred.
       if( orgDataPtr == ndp )
       {
         if((rp = _cmMmFindRecd(p,orgDataPtr)) == NULL )
           cmErrMsg(&p->err,kMissingRecdMmRC,"Unable to locate a tracking record associated with reallocation data area pointer:%p.",orgDataPtr);           
       }
   }

   return ndp;
}

cmMmRC_t cmMmFree( cmMmH_t h, void* dataPtr )
{
  cmMm_t* p = _cmMmHandleToPtr(h);

  return _cmMmFreeP(p,dataPtr);
}

cmMmRC_t cmMmFreeDebug( cmMmH_t h, void* dataPtr, const char* fileName, const char* funcName, unsigned fileLine )
{
  cmMmRC_t rc;
  if((rc = cmMmFree(h,dataPtr)) != kOkMmRC )
    cmErrMsg(&_cmMmHandleToPtr(h)->err,rc,"Memory free failed at %s() line:%i %s",funcName,fileLine,fileName);
  return rc;
}

cmMmRC_t cmMmFreePtr( cmMmH_t h, void** dataPtrPtr )
{
  assert(dataPtrPtr != NULL );

  cmMmRC_t rc = cmMmFree(h,*dataPtrPtr);

  *dataPtrPtr = NULL;

  return rc;
}

cmMmRC_t cmMmFreePtrDebug( cmMmH_t h, void* dataPtr, const char* fileName, const char* funcName, unsigned fileLine )
{
  cmMmRC_t rc;
  if((rc = cmMmFreePtr(h,dataPtr)) != kOkMmRC )
    cmErrMsg(&_cmMmHandleToPtr(h)->err,rc,"Memory free failed at %s() line:%i %s",funcName,fileLine,fileName);
  return rc;
}

unsigned cmMmByteCount( cmMmH_t h, const void* dataPtr )
{ 
  cmMm_t* p = _cmMmHandleToPtr(h);
  return _cmMmDataToByteCnt(dataPtr,p->guardByteCnt);
}

unsigned cmMmDebugId( cmMmH_t h, const void* dataPtr)
{
  cmMm_t*     p  = _cmMmHandleToPtr(h);
  const cmMmRecd_t* rp = NULL;

  // if we are tracking alloc's and free's
  if( cmIsFlag(p->flags,kTrackMmFl) )
  {
    // locate the tracking recd
    rp = _cmMmFindRecd(p,dataPtr);
  }

  return rp==NULL ? cmInvalidId : rp->uniqueId;
}

cmMmRC_t _cmMmError( cmMm_t* p, cmMmRC_t rc, const cmMmRecd_t* rp, const char* msg )
{
 return cmErrMsg(&p->err,rc,"%s detected on block id:%i from %s() line:%i %s.",msg,rp->uniqueId,rp->funcNameStr,rp->fileLine,rp->fileNameStr);
}

cmMmRC_t _cmMmRecdPrint( cmMm_t* p, cmMmRecd_t* rp, cmMmRC_t rc )
{
  void*    dp  = rp->dataPtr;
  unsigned gbc = p->guardByteCnt;
  char*    lbl = NULL;

  switch( rc )
  {
    case kOkMmRC:               lbl = "Ok              "; break;
    case kLeakDetectedMmRC:     lbl = "Memory Leak     "; break;
    case kWriteAfterFreeMmRC:   lbl = "Write After Free"; break;
    case kDblFreeDetectedMmRC:  lbl = "Double Free     "; break;
    case kGuardCorruptMmRC:     lbl = "Guard Corrupt   "; break;
    default:
      lbl = "Unknown Status  ";
  }

  cmRptPrintf(p->err.rpt,"%s id:%5i data:%p : data:%5i prefix:%5i total:%5i base:%p : line=%5i %s %s\n",
    lbl,rp->uniqueId,rp->dataPtr,
    _cmMmDataToByteCnt(dp,gbc),
    _cmMmDataToPrefixCnt(dp,gbc), 
    _cmMmDataToTotalByteCnt(dp,gbc), 
    _cmMmDataToBasePtr(dp,gbc),  
    rp->fileLine,rp->funcNameStr,rp->fileNameStr );

  return rc;
}

cmMmRC_t cmMmReport( cmMmH_t h, unsigned flags )
{
  cmMm_t*     p             = _cmMmHandleToPtr(h);
  unsigned    allocByteCnt  = 0;
  unsigned    ttlByteCnt    = 0;
  unsigned    dataFrByteCnt = 0;
  unsigned    ttlFrByteCnt  = 0;
  unsigned    dataLkByteCnt = 0;
  unsigned    ttlLkByteCnt  = 0;
  unsigned    allocBlkCnt   = 0;
  unsigned    freeBlkCnt    = 0;
  unsigned    leakBlkCnt    = 0;
  cmMmRecd_t* rp            = p->listPtr;
  cmMmRC_t    ret_rc        = kOkMmRC;

  for(; rp != NULL; rp = rp->linkPtr,++allocBlkCnt )
  {
     cmMmRC_t rc               = kOkMmRC;
     unsigned blkDataByteCnt   = _cmMmDataToByteCnt(rp->dataPtr,p->guardByteCnt);
     unsigned blkTtlByteCnt    = _cmMmDataToTotalByteCnt(rp->dataPtr,p->guardByteCnt);

    allocByteCnt += blkDataByteCnt;
    ttlByteCnt   += blkTtlByteCnt;
    
    // if this block was freed or never alloc'd
    if( cmIsFlag(rp->flags,kFreedMmFl) || rp->dataPtr == NULL )
    {
      ++freeBlkCnt;
      dataFrByteCnt += blkDataByteCnt;
      ttlFrByteCnt  += blkTtlByteCnt;
    }
    else // if this block leaked
    {
      ++leakBlkCnt;
      dataLkByteCnt += blkDataByteCnt;
      ttlLkByteCnt  += blkTtlByteCnt;
      if( cmIsFlag(flags,kIgnoreLeaksMmFl) == false )
        rc   = _cmMmRecdPrint(p,rp,kLeakDetectedMmRC);
    }
    
    // if this block was double freed
    if( cmIsFlag(rp->flags,kDblFreeMmFl) )
      rc = _cmMmRecdPrint(p,rp,kDblFreeDetectedMmRC);

    // check for guard corruption and write-after-free 
    cmMmRC_t t_rc = _cmMmCheckGuards(p,rp);

    switch( t_rc )
    {
      case kOkMmRC:
        break;

      case kGuardCorruptMmRC:
        rc = _cmMmRecdPrint(p,rp,t_rc);
        break;

      case kWriteAfterFreeMmRC:
        rc = _cmMmRecdPrint(p,rp,t_rc);
        break;

      default:
        assert(0);
        break;
    }

    if( rc != kOkMmRC )
      ret_rc = rc;
    else
    {
      if( cmIsFlag(flags,kIgnoreNormalMmFl)==false )
        _cmMmRecdPrint(p,rp,rc);
    }

  }
  
  if( p->listPtr != NULL && cmIsFlag(flags,kSuppressSummaryMmFl)==false )
  {
    cmRptPrintf(p->err.rpt,"Blocks Allocated:%i Freed:%i Leaked:%i\n",allocBlkCnt,freeBlkCnt,leakBlkCnt);
    cmRptPrintf(p->err.rpt,"Bytes  Allocated: data=%i total=%i Freed: data=%i total=%i Leaked: data=%i total=%i\n",allocByteCnt,ttlByteCnt,dataFrByteCnt,ttlFrByteCnt,dataLkByteCnt,ttlLkByteCnt);
  }

  return ret_rc;
}

cmMmRC_t cmMmIsGuardCorrupt( cmMmH_t h, unsigned id )
{
  cmMm_t*     p  = _cmMmHandleToPtr(h);
  cmMmRecd_t* rp = p->listPtr;

  while(rp != NULL )
  {
    if( rp->uniqueId == id )
      break;
    rp = rp->linkPtr;
  }
  
  if( rp == NULL )
    return cmErrMsg(&p->err,kMissingRecdMmRC,"Unable to locate the tracking record associated with id %i.",id);

  return _cmMmCheckGuards(p,rp);
}

cmMmRC_t cmMmCheckAllGuards( cmMmH_t h )
{
  cmMm_t*     p  = _cmMmHandleToPtr(h);
  cmMmRecd_t* rp = p->listPtr;
  cmMmRC_t    rc = kOkMmRC;

  while(rp != NULL )
  {
    if((rc = _cmMmCheckGuards(p,rp)) != kOkMmRC )
      rc = cmErrMsg(&p->err,rc,"A corrupt guard or 'write after free' was detected in the data area allocated with id:%i at %s (line:%i) %s.",rp->uniqueId,rp->funcNameStr,rp->fileLine,rp->fileNameStr);

    rp = rp->linkPtr;
  }

  return rc;
}
