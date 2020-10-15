//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmMem.h"
#include "cmMallocDebug.h"

cmMmH_t _cmMdH = cmSTATIC_NULL_HANDLE;

void* _cmMdAlloc(void* funcArgPtr, unsigned byteCnt)
{ return malloc(byteCnt); }

bool  _cmMdFree(void* funcArgPtr, void* ptr)
{ free(ptr); return true; }


cmMmRC_t cmMdInitialize( unsigned guardByteCnt, unsigned alignByteCnt, unsigned flags, cmRpt_t* rptPtr )
{ return cmMmInitialize(&_cmMdH,_cmMdAlloc,_cmMdFree,NULL,guardByteCnt,alignByteCnt,flags,rptPtr); }

cmMmRC_t cmMdFinalize()
{ return cmMmFinalize(&_cmMdH); }

bool cmMdIsValid()
{ return _cmMdH.h != NULL; }

unsigned  cmMdGuardByteCount() { return cmMmGuardByteCount( _cmMdH); }
unsigned  cmMdAlignByteCount() { return cmMmAlignByteCount( _cmMdH); }
unsigned  cmMdInitializeFlags(){ return cmMmInitializeFlags(_cmMdH); }


void* cmMdAllocate( void *orgDataPtr, unsigned eleCnt, unsigned eleByteCnt, unsigned flags)
{ return cmMmAllocate(_cmMdH,orgDataPtr,eleCnt,eleByteCnt,flags,NULL,NULL,0); }

void* cmMdAllocateDebug( void* orgDataPtr, unsigned eleCnt, unsigned eleByteCnt, unsigned flags, const char* func, const char* fn, unsigned line)
{ return cmMmAllocate(_cmMdH,orgDataPtr,eleCnt,eleByteCnt,flags,fn,func,line); }

void     cmMdFree(     void*  p )
{ cmMmFree(_cmMdH,p); }

void     cmMdFreeDebug(void*  p, const char* func, const char* fn, unsigned line )
{ cmMmFreeDebug(_cmMdH,p,fn,func,line); }

void     cmMdFreePtr(     void* p )
{ cmMmFreePtr(_cmMdH,p); }

void     cmMdFreePtrDebug(void* p, const char* func, const char* fn, unsigned line )
{ cmMmFreePtrDebug(_cmMdH,p,fn,func,line); }

cmChar_t* cmMdAllocStr(      void* orgStrPtr, const cmChar_t* str, unsigned n, unsigned flags )
{
  if( str==NULL)
    return NULL;

  //unsigned  n  = strlen(str)+1;
  n += 1;
  cmChar_t* cp = cmMdAllocate(orgStrPtr,n,sizeof(cmChar_t),flags);
  strncpy(cp,str,n);
  cp[n-1] = 0;
  return cp;
}

cmChar_t* cmMdAllocStrDebug( void* orgStrPtr, const cmChar_t* str, unsigned n, unsigned flags, const char* func, const char* fn, unsigned line )
{
  if( str==NULL)
    return NULL;

  n += 1;  
  cmChar_t* cp = cmMdAllocateDebug((void*)orgStrPtr,n,sizeof(cmChar_t),flags,func,fn,line);
  strncpy(cp,str,n);
  cp[n-1] = 0;
  return cp;
}


cmMmRC_t cmMdIsGuardCorrupt( unsigned id )
{ return cmMmIsGuardCorrupt(_cmMdH,id); }

cmMmRC_t cmMdCheckAllGuards( cmRpt_t* rpt )
{ return cmMmCheckAllGuards(_cmMdH); }

unsigned cmMdDebugId( const void* dataPtr)
{ return cmMmDebugId(_cmMdH,dataPtr);  }

cmMmRC_t cmMdReport( unsigned mmFlags )
{ return cmMmReport(_cmMdH,mmFlags); }

//! [cmMdExample]
void cmMdTest( cmRpt_t* rpt )
{

  bool       memDebugFl          = true;
  unsigned   memGuardByteCnt       = memDebugFl ? 0 : 8;
  unsigned   memAlignByteCnt     = 16;
  unsigned   memFlags            = memDebugFl ? kTrackMmFl | kDeferFreeMmFl | kFillUninitMmFl | kFillFreedMmFl : 0;

  // initialize the library
  cmMdInitialize( memGuardByteCnt, memAlignByteCnt, memFlags, rpt );
  
  // Allocate a block of 16 bytes of aligned and zeroed memory.
  void*     d0p = cmMdAllocateDebug(NULL, 1, 16, kAlignMmFl | kZeroMmFl, __FUNCTION__, __FILE__, __LINE__ );

  // Allocate a block of 20 bytes of non-aligned, zeroed memory.
  unsigned* d1p = cmMdAllocateDebug(NULL, 1, 20, /*kAlignMmFl |*/ kZeroMmFl, __FUNCTION__, __FILE__, __LINE__ );
  unsigned i;
  
  // Intentionally overwrite the guard bytes by writing 
  // 24 bytes where only 20 where allocated
  for(i=0; i<3; ++i)
    d1p[i] = i;

  // Print a report showing the state of each memory block.
  // This should show that d1p[] has been corrupted and
  // memory leaks because active blocks exist.
  cmMdReport( 0 );

  // Expand d1p[] preserving the existing 20 bytes.
  d1p = cmMdAllocateDebug(d1p, 1, 24, kPreserveMmFl | /*kAlignMmFl |*/ kZeroMmFl, __FUNCTION__, __FILE__, __LINE__ );

  // Print the contents of the expanded d1p[]
  for(i=0; i<3; ++i)
    printf("%i ",d1p[i]);
  printf("\n");

  // Free d0p[] and d1p[];
  cmMdFreeDebug(d0p, __FUNCTION__, __FILE__, __LINE__);
  cmMdFreeDebug(d1p, __FUNCTION__, __FILE__, __LINE__);

  // Perform a write after free on d0p[].
  *(unsigned*)d0p = 1;

  // Print another report showing to test write-after-free detection. 
  cmMdReport( 0 );

  // Close the library.
  cmMdFinalize();
  
}
//! [cmMdExample]
