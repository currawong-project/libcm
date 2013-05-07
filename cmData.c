#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmData.h"
#include "cmMem.h"
#include "cmMallocDebug.h"

cmDtRC_t _cmDataErrNo = kOkDtRC;

typedef struct
{
  cmDataFmtId_t tid;
  unsigned      cnt;
} cmDataSerialHdr_t;

void _cmDataFreeArray( cmData_t* p )
{
  if(cmIsFlag(p->flags,kDynPtrDtFl))
  {
    cmMemFree(p->u.vp);
    p->u.vp    = NULL;
    p->flags = cmClrFlag(p->flags,kDynPtrDtFl);
  }
  p->tid = kInvalidDtId;
  p->cnt = 0;
}

void _cmDataFree( cmData_t* p )
{
  _cmDataFreeArray(p);
  if( cmIsFlag(p->flags,kDynObjDtFl) )
    cmMemFree(p);
}

unsigned _cmDataByteCount( const cmData_t* p )
{
  unsigned n = sizeof(cmDataSerialHdr_t);

  switch( p->tid )
  {
    case kInvalidDtId:   return 0;
    case kNullDtId:      return n;
    case kUCharDtId:     return n + sizeof(unsigned char); 
    case kCharDtId:      return n + sizeof(char);
    case kUShortDtId:    return n + sizeof(unsigned short);
    case kShortDtId:     return n + sizeof(short);
    case kUIntDtId:      return n + sizeof(unsigned int);
    case kIntDtId:       return n + sizeof(int);
    case kULongDtId:     return n + sizeof(unsigned long);
    case kLongDtId:      return n + sizeof(long);
    case kFloatDtId:     return n + sizeof(float);
    case kDoubleDtId:    return n + sizeof(double);
      
    case kStrDtId:       return n + (p->u.z ==NULL ? 0 : strlen(p->u.z)  + 1);
    case kConstStrDtId:  return n + (p->u.cz==NULL ? 0 : strlen(p->u.cz) + 1);

    case kUCharPtrDtId:  return n + p->cnt * sizeof(unsigned char);
    case kCharPtrDtId:   return n + p->cnt * sizeof(char); 
    case kUShortPtrDtId: return n + p->cnt * sizeof(unsigned short);
    case kShortPtrDtId:  return n + p->cnt * sizeof(short);
    case kUIntPtrDtId:   return n + p->cnt * sizeof(unsigned int);
    case kIntPtrDtId:    return n + p->cnt * sizeof(int);
    case kULongPtrDtId:  return n + p->cnt * sizeof(unsigned long);
    case kLongPtrDtId:   return n + p->cnt * sizeof(long);
    case kFloatPtrDtId:  return n + p->cnt * sizeof(float);
    case kDoublePtrDtId: return n + p->cnt * sizeof(double);
    case kVoidPtrDtId:   return n + p->cnt * sizeof(char);

    default:
      return n;
  }
  assert(0);
  return 0;
}

char            cmDataChar(      const cmData_t* p ) { assert(p->tid==kCharDtId);      return p->u.c; }
unsigned char   cmDataUChar(     const cmData_t* p ) { assert(p->tid==kUCharDtId);     return p->u.uc; } 
short           cmDataShort(     const cmData_t* p ) { assert(p->tid==kShortDtId);     return p->u.s; } 
unsigned short  cmDataUShort(    const cmData_t* p ) { assert(p->tid==kUShortDtId);    return p->u.us; } 
int             cmDataInt(       const cmData_t* p ) { assert(p->tid==kIntDtId);       return p->u.i; } 
unsigned int    cmDataUInt(      const cmData_t* p ) { assert(p->tid==kUIntDtId);      return p->u.ui; } 
long            cmDataLong(      const cmData_t* p ) { assert(p->tid==kLongDtId);      return p->u.l; } 
unsigned long   cmDataULong(     const cmData_t* p ) { assert(p->tid==kULongDtId);     return p->u.ul; } 
float           cmDataFloat(     const cmData_t* p ) { assert(p->tid==kFloatDtId);     return p->u.f; } 
double          cmDataDouble(    const cmData_t* p ) { assert(p->tid==kDoubleDtId);    return p->u.d; } 
cmChar_t*       cmDataStr(       const cmData_t* p ) { assert(p->tid==kStrDtId);       return p->u.z; } 
const cmChar_t* cmDataConstStr(  const cmData_t* p ) { assert(p->tid==kConstStrDtId);  return p->u.cz; } 
void*           cmDataVoidPtr(   const cmData_t* p ) { assert(p->tid==kVoidPtrDtId);   return p->u.vp; }
char*           cmDataCharPtr(   const cmData_t* p ) { assert(p->tid==kCharPtrDtId);   return p->u.cp; } 
unsigned char*  cmDataUCharPtr(  const cmData_t* p ) { assert(p->tid==kUCharPtrDtId);  return p->u.ucp; } 
short*          cmDataShortPtr(  const cmData_t* p ) { assert(p->tid==kShortPtrDtId);  return p->u.sp; } 
unsigned short* cmDataUShortPtr( const cmData_t* p ) { assert(p->tid==kUShortPtrDtId); return p->u.usp; } 
int*            cmDataIntPtr(    const cmData_t* p ) { assert(p->tid==kIntPtrDtId);    return p->u.ip; } 
unsigned int*   cmDataUIntPtr(   const cmData_t* p ) { assert(p->tid==kUIntPtrDtId);   return p->u.uip; } 
long*           cmDataLongPtr(   const cmData_t* p ) { assert(p->tid==kLongPtrDtId);   return p->u.lp; } 
unsigned long*  cmDataULongPtr(  const cmData_t* p ) { assert(p->tid==kULongPtrDtId);  return p->u.ulp; } 
float*          cmDataFloatPtr(  const cmData_t* p ) { assert(p->tid==kFloatPtrDtId);  return p->u.fp; } 
double*         cmDataDoublePtr( const cmData_t* p ) { assert(p->tid==kDoublePtrDtId); return p->u.dp; } 

unsigned char   cmDataGetUChar(     const cmData_t* p )
{
  unsigned char v = kInvalidDtUChar;

  switch( p->tid )
  {
    case kUCharDtId:  v =                p->u.uc; break;
    case kCharDtId:   v = (unsigned char)p->u.c;  break;
    case kUShortDtId: v = (unsigned char)p->u.us; break;
    case kShortDtId:  v = (unsigned char)p->u.s;  break;
    case kUIntDtId:   v = (unsigned char)p->u.ui; break;
    case kIntDtId:    v = (unsigned char)p->u.i;  break;
    case kULongDtId:  v = (unsigned char)p->u.ul; break;
    case kLongDtId:   v = (unsigned char)p->u.l;  break;
    case kFloatDtId:  v = (unsigned char)p->u.f;  break;
    case kDoubleDtId: v = (unsigned char)p->u.d;  break;
    default:
      _cmDataErrNo = kCvtErrDtRC;
  }
  return v;
}

char  cmDataGetChar( const cmData_t* p )
{
  char v = kInvalidDtChar;

  switch( p->tid )
  {
    case kUCharDtId:  v = (char)p->u.uc; break;
    case kCharDtId:   v =       p->u.c;  break;
    case kUShortDtId: v = (char)p->u.us; break;
    case kShortDtId:  v = (char)p->u.s;  break;
    case kUIntDtId:   v = (char)p->u.ui; break;
    case kIntDtId:    v = (char)p->u.i;  break;
    case kULongDtId:  v = (char)p->u.ul; break;
    case kLongDtId:   v = (char)p->u.l;  break;
    case kFloatDtId:  v = (char)p->u.f;  break;
    case kDoubleDtId: v = (char)p->u.d;  break;
    default:
      _cmDataErrNo = kCvtErrDtRC;
  }
  return v;
}

short           cmDataGetShort(     const cmData_t* p )
{
  short v = kInvalidDtShort;

  switch( p->tid )
  {
    case kUCharDtId:  v = (short)p->u.uc; break;
    case kCharDtId:   v = (short)p->u.c;  break;
    case kUShortDtId: v = (short)p->u.us; break;
    case kShortDtId:  v =        p->u.s;  break;
    case kUIntDtId:   v = (short)p->u.ui; break;
    case kIntDtId:    v = (short)p->u.i;  break;
    case kULongDtId:  v = (short)p->u.ul; break;
    case kLongDtId:   v = (short)p->u.l;  break;
    case kFloatDtId:  v = (short)p->u.f;  break;
    case kDoubleDtId: v = (short)p->u.d;  break;
    default:
      _cmDataErrNo = kCvtErrDtRC;
  }

  return v;
}


unsigned short  cmDataGetUShort(    const cmData_t* p )
{
  unsigned short v = kInvalidDtUShort;

  switch( p->tid )
  {
    case kUCharDtId:  v = (unsigned short)p->u.uc; break;
    case kCharDtId:   v = (unsigned short)p->u.c;  break;
    case kUShortDtId: v =                 p->u.us; break;
    case kShortDtId:  v = (unsigned short)p->u.s;  break;
    case kUIntDtId:   v = (unsigned short)p->u.ui; break;
    case kIntDtId:    v = (unsigned short)p->u.i;  break;
    case kULongDtId:  v = (unsigned short)p->u.ul; break;
    case kLongDtId:   v = (unsigned short)p->u.l;  break;
    case kFloatDtId:  v = (unsigned short)p->u.f;  break;
    case kDoubleDtId: v = (unsigned short)p->u.d;  break;
    default:
      _cmDataErrNo = kCvtErrDtRC;
  }

  return v;
}

int             cmDataGetInt(       const cmData_t* p )
{
  int v = kInvalidDtInt;

  switch( p->tid )
  {
    case kUCharDtId:  v = (int)p->u.uc; break;
    case kCharDtId:   v = (int)p->u.c;  break;
    case kUShortDtId: v = (int)p->u.us; break;
    case kShortDtId:  v = (int)p->u.s;  break;
    case kUIntDtId:   v = (int)p->u.ui; break;
    case kIntDtId:    v =      p->u.i;  break;
    case kULongDtId:  v = (int)p->u.ul; break;
    case kLongDtId:   v = (int)p->u.l;  break;
    case kFloatDtId:  v = (int)p->u.f;  break;
    case kDoubleDtId: v = (int)p->u.d;  break;
    default:
      _cmDataErrNo = kCvtErrDtRC;
  }

  return v;
}

unsigned int    cmDataGetUInt(      const cmData_t* p )
{
  unsigned int v = kInvalidDtUInt;

  switch( p->tid )
  {
    case kUCharDtId:  v = (unsigned int)p->u.uc; break;
    case kCharDtId:   v = (unsigned int)p->u.c;  break;
    case kUShortDtId: v = (unsigned int)p->u.us; break;
    case kShortDtId:  v = (unsigned int)p->u.s;  break;
    case kUIntDtId:   v =               p->u.ui; break;
    case kIntDtId:    v = (unsigned int)p->u.i;  break;
    case kULongDtId:  v = (unsigned int)p->u.ul; break;
    case kLongDtId:   v = (unsigned int)p->u.l;  break;
    case kFloatDtId:  v = (unsigned int)p->u.f;  break;
    case kDoubleDtId: v = (unsigned int)p->u.d;  break;
    default:
      _cmDataErrNo = kCvtErrDtRC;
  }

  return v;
}

long            cmDataGetLong(      const cmData_t* p )
{
  long v = kInvalidDtLong;

  switch( p->tid )
  {
    case kUCharDtId:  v = (long)p->u.uc; break;
    case kCharDtId:   v = (long)p->u.c;  break;
    case kUShortDtId: v = (long)p->u.us; break;
    case kShortDtId:  v = (long)p->u.s;  break;
    case kUIntDtId:   v = (long)p->u.ui; break;
    case kIntDtId:    v = (long)p->u.i;  break;
    case kULongDtId:  v = (long)p->u.ul; break;
    case kLongDtId:   v =       p->u.l;  break;
    case kFloatDtId:  v = (long)p->u.f;  break;
    case kDoubleDtId: v = (long)p->u.d;  break;
    default:
      _cmDataErrNo = kCvtErrDtRC;
  }

  return v;
}

unsigned long   cmDataGetULong(     const cmData_t* p )
{
  unsigned long v = kInvalidDtULong;

  switch( p->tid )
  {
    case kUCharDtId:  v = (unsigned long)p->u.uc; break;
    case kCharDtId:   v = (unsigned long)p->u.c;  break;
    case kUShortDtId: v = (unsigned long)p->u.us; break;
    case kShortDtId:  v = (unsigned long)p->u.s;  break;
    case kUIntDtId:   v = (unsigned long)p->u.ui; break;
    case kIntDtId:    v = (unsigned long)p->u.i;  break;
    case kULongDtId:  v =                p->u.ul; break;
    case kLongDtId:   v = (unsigned long)p->u.l;  break;
    case kFloatDtId:  v = (unsigned long)p->u.f;  break;
    case kDoubleDtId: v = (unsigned long)p->u.d;  break;
    default:
      _cmDataErrNo = kCvtErrDtRC;
  }

  return v;
}

float           cmDataGetFloat(     const cmData_t* p )
{
  float v = FLT_MAX;

  switch( p->tid )
  {
    case kUCharDtId:  v = (float)p->u.uc; break;
    case kCharDtId:   v = (float)p->u.c;  break;
    case kUShortDtId: v = (float)p->u.us; break;
    case kShortDtId:  v = (float)p->u.s;  break;
    case kUIntDtId:   v = (float)p->u.ui; break;
    case kIntDtId:    v = (float)p->u.i;  break;
    case kULongDtId:  v = (float)p->u.ul; break;
    case kLongDtId:   v = (float)p->u.l;  break;
    case kFloatDtId:  v =        p->u.f;  break;
    case kDoubleDtId: v = (float)p->u.d;  break;
    default:
      _cmDataErrNo = kCvtErrDtRC;
  }

  return v;
}

double          cmDataGetDouble(    const cmData_t* p )
{
  double v = DBL_MAX;

  switch( p->tid )
  {
    case kUCharDtId:  v = (double)p->u.uc; break;
    case kCharDtId:   v = (double)p->u.c;  break;
    case kUShortDtId: v = (double)p->u.us; break;
    case kShortDtId:  v = (double)p->u.s;  break;
    case kUIntDtId:   v = (double)p->u.ui; break;
    case kIntDtId:    v = (double)p->u.i;  break;
    case kULongDtId:  v = (double)p->u.ul; break;
    case kLongDtId:   v = (double)p->u.l;  break;
    case kFloatDtId:  v = (double)p->u.f;  break;
    case kDoubleDtId: v =         p->u.d;  break;
    default:
      _cmDataErrNo = kCvtErrDtRC;
  }

  return v;
}

cmChar_t*       cmDataGetStr(       const cmData_t* p )
{ 
  assert( p->tid == kStrDtId || p->tid == kConstStrDtId); 
  return (p->tid == kStrDtId || p->tid == kConstStrDtId) ? p->u.z : NULL; 
}

const cmChar_t* cmDataGetConstStr(  const cmData_t* p )
{ 
  assert( p->tid == kStrDtId || p->tid == kConstStrDtId); 
  return (p->tid == kStrDtId || p->tid == kConstStrDtId) ? p->u.cz : NULL; 
}

void*           cmDataGetVoidPtr(   const cmData_t* p )
{
  assert( kMinPtrDtId <= p->tid && p->tid <= kMaxPtrDtId );
  return ( kMinPtrDtId <= p->tid && p->tid <= kMaxPtrDtId ) ? p->u.vp : NULL;  
}

char*           cmDataGetCharPtr(   const cmData_t* p )
{ 
  assert( p->tid == kCharPtrDtId || p->tid == kUCharPtrDtId );
  return (p->tid == kCharPtrDtId || p->tid == kUCharPtrDtId) ? p->u.cp : NULL; 
}

unsigned char*  cmDataGetUCharPtr(  const cmData_t* p )
{ 
  assert( p->tid == kCharPtrDtId || p->tid == kUCharPtrDtId );
  return (p->tid == kCharPtrDtId || p->tid == kUCharPtrDtId) ? p->u.ucp : NULL; 
}

short*          cmDataGetShortPtr(  const cmData_t* p )
{ 
  assert( p->tid == kShortPtrDtId || p->tid == kUShortPtrDtId );
  return (p->tid == kShortPtrDtId || p->tid == kUShortPtrDtId ) ? p->u.sp : NULL; 
}

unsigned short* cmDataGetUShortPtr( const cmData_t* p )
{
  assert( p->tid == kShortPtrDtId || p->tid == kUShortPtrDtId );
  return (p->tid == kShortPtrDtId || p->tid == kUShortPtrDtId ) ? p->u.usp : NULL; 
}

int*            cmDataGetIntPtr(    const cmData_t* p )
{
  assert( p->tid == kIntPtrDtId || p->tid == kUIntPtrDtId );
  return (p->tid == kIntPtrDtId || p->tid == kUIntPtrDtId ) ? p->u.ip : NULL; 
}

unsigned int*   cmDataGetUIntPtr(   const cmData_t* p )
{
  assert( p->tid == kIntPtrDtId || p->tid == kUIntPtrDtId );
  return (p->tid == kIntPtrDtId || p->tid == kUIntPtrDtId ) ? p->u.uip : NULL; 
}

long*           cmDataGetLongPtr(   const cmData_t* p )
{
  assert( p->tid == kLongPtrDtId || p->tid == kULongPtrDtId );
  return (p->tid == kLongPtrDtId || p->tid == kULongPtrDtId ) ? p->u.lp : NULL; 
}

unsigned long*  cmDataGetULongPtr(  const cmData_t* p )
{
  assert( p->tid == kLongPtrDtId || p->tid == kULongPtrDtId );
  return (p->tid == kLongPtrDtId || p->tid == kULongPtrDtId ) ? p->u.ulp : NULL; 
}

float*          cmDataGetFloatPtr(  const cmData_t* p )
{ return p->tid == kFloatPtrDtId ? p->u.fp : NULL; }

double*         cmDataGetDoublePtr( const cmData_t* p )
{ return p->tid == kDoublePtrDtId ? p->u.dp : NULL; }

  // Set the value of an existing data object.
cmData_t* cmDataSetChar(      cmData_t* p, char v )
{
  _cmDataFreeArray(p);
  p->tid = kCharDtId;
  p->u.c = v;
  return p;
}

cmData_t* cmDataSetUChar(     cmData_t* p, unsigned char v )
{
  _cmDataFreeArray(p);
  p->tid = kUCharDtId;
  p->u.uc = v;
  return p;
}

cmData_t* cmDataSetShort(     cmData_t* p, short v )
{
  _cmDataFreeArray(p);
  p->tid = kShortDtId;
  p->u.s = v;
  return p;
}

cmData_t* cmDataSetUShort(    cmData_t* p, unsigned short v )
{
  _cmDataFreeArray(p);
  p->tid = kUShortDtId;
  p->u.us = v;
  return p;
}

cmData_t* cmDataSetInt(       cmData_t* p, int v )
{
  _cmDataFreeArray(p);
  p->tid = kCharDtId;
  p->u.c = v;
}

cmData_t* cmDataSetUInt(      cmData_t* p, unsigned int v )
{
  _cmDataFreeArray(p);
  p->tid = kUIntDtId;
  p->u.ui = v;
  return p;
}

cmData_t* cmDataSetLong(      cmData_t* p, long v )
{
  _cmDataFreeArray(p);
  p->tid = kLongDtId;
  p->u.l = v;
}

cmData_t* cmDataSetULong(     cmData_t* p, unsigned long v )
{
  _cmDataFreeArray(p);
  p->tid = kULongDtId;
  p->u.ul = v;
  return p;
}

cmData_t* cmDataSetFloat(     cmData_t* p, float v )
{
  _cmDataFreeArray(p);
  p->tid = kFloatDtId;
  p->u.f = v;
  return p;
}

cmData_t* cmDataSetDouble(    cmData_t* p, double v )
{
  _cmDataFreeArray(p);
  p->tid = kDoubleDtId;
  p->u.d = v;
  return p;
}

cmData_t* cmDataSetStr(       cmData_t* p, cmChar_t* s )
{
  _cmDataFreeArray(p);
  p->tid = kStrDtId;
  p->u.z = s;
  return p;
}

cmData_t* cmDataSetConstStr(  cmData_t* p, const cmChar_t* s )
{
  _cmDataFreeArray(p);
  p->tid = kConstStrDtId;
  p->u.cz = s;
  return p;
}

// Set the value of an existing data object to an external array.
// The array is not copied.
cmData_t* cmDataSetVoidPtr(   cmData_t* p, void* vp,          unsigned cnt )
{
  cmDataSetCharPtr(p,(char*)vp,cnt);  
  p->tid = kVoidPtrDtId;
  return p;
}

cmData_t* cmDataSetCharPtr(   cmData_t* p, char* vp,          unsigned cnt )
{
  _cmDataFreeArray(p);
  p->tid  = kCharPtrDtId;
  p->u.cp = vp;
  p->cnt  = cnt;
  return p;
}

cmData_t* cmDataSetUCharPtr(  cmData_t* p, unsigned char* vp,  unsigned cnt )
{
  _cmDataFreeArray(p);
  p->tid  = kUCharPtrDtId;
  p->u.ucp = vp;
  p->cnt  = cnt;
  return p;
}

cmData_t* cmDataSetShortPtr(  cmData_t* p, short* vp,          unsigned cnt )
{
  _cmDataFreeArray(p);
  p->tid  = kShortPtrDtId;
  p->u.sp = vp;
  p->cnt  = cnt;
  return p;
}

cmData_t* cmDataSetUShortPtr( cmData_t* p, unsigned short* vp, unsigned cnt )
{
  _cmDataFreeArray(p);
  p->tid   = kUShortPtrDtId;
  p->u.usp = vp;
  p->cnt   = cnt;
  return p;
}

cmData_t* cmDataSetIntPtr(    cmData_t* p, int* vp,            unsigned cnt )
{
  _cmDataFreeArray(p);
  p->tid  = kCharPtrDtId;
  p->u.ip = vp;
  p->cnt  = cnt;
  return p;
}

cmData_t* cmDataSetUIntPtr(   cmData_t* p, unsigned int* vp,   unsigned cnt )
{
  _cmDataFreeArray(p);
  p->tid   = kUIntPtrDtId;
  p->u.uip = vp;
  p->cnt   = cnt;
  return p;
}

cmData_t* cmDataSetLongPtr(   cmData_t* p, long* vp,           unsigned cnt )
{
  _cmDataFreeArray(p);
  p->tid  = kLongPtrDtId;
  p->u.lp = vp;
  p->cnt  = cnt;
  return p;
}

cmData_t* cmDataSetULongPtr(  cmData_t* p, unsigned long* vp,  unsigned cnt )
{
  _cmDataFreeArray(p);
  p->tid   = kULongPtrDtId;
  p->u.ulp = vp;
  p->cnt   = cnt;
  return p;
}

cmData_t* cmDataSetFloatPtr(  cmData_t* p, float* vp,          unsigned cnt )
{
  _cmDataFreeArray(p);
  p->tid  = kFloatPtrDtId;
  p->u.fp = vp;
  p->cnt  = cnt;
  return p;
}

cmData_t* cmDataSetDoublePtr( cmData_t* p, double* vp,         unsigned cnt )
{
  _cmDataFreeArray(p);
  p->tid  = kDoublePtrDtId;
  p->u.dp = vp;
  p->cnt  = cnt;
  return p;
}

// Set the value of an existing array based data object. 
// Allocate the internal array and copy the array into it.
cmData_t* cmDataSetStrAlloc( cmData_t* p, const cmChar_t* s )
{
  if( cmIsFlag(p->flags,kDynPtrDtFl) )
    cmMemResizeStr(p->u.z,s);
  else
  {
    _cmDataFreeArray(p);
    p->u.z = cmMemAllocStr(s);
  }
  p->tid    = kStrDtId;
  p->flags  = cmSetFlag(p->flags,kDynPtrDtFl);
  return p;
}

cmData_t* cmDataSetConstStrAlloc(  cmData_t* p, const cmChar_t* s )
{ return cmDataSetStrAlloc(p,s); }

cmData_t* cmDataSetVoidAllocPtr(   cmData_t* p, const void* vp, unsigned cnt )
{ return cmDataSetCharAllocPtr(p,(char*)vp,cnt); }

cmData_t* cmDataSetCharAllocPtr(  cmData_t* p, const char* vp,  unsigned cnt )
{
  if( cmIsFlag(p->flags,kDynPtrDtFl) )
    p->u.cp = cmMemResize(char, p->u.cp, cnt );
  else
  {
    _cmDataFreeArray(p);
    p->u.cp = cmMemAlloc(char, cnt );
  }
  p->tid   = kCharPtrDtId;
  p->flags = cmSetFlag(p->flags,kDynPtrDtFl);
  return p;
}

cmData_t* cmDataSetUCharAllocPtr(  cmData_t* p, const unsigned char* vp,  unsigned cnt )
{
  if( cmIsFlag(p->flags,kDynPtrDtFl) )
    p->u.ucp = cmMemResize(unsigned char, p->u.ucp, cnt );
  else
  {
    _cmDataFreeArray(p);
    p->u.ucp = cmMemAlloc(unsigned char, cnt );
  }
  p->tid   = kUCharPtrDtId;
  p->flags = cmSetFlag(p->flags,kDynPtrDtFl);
  return p;
}

cmData_t* cmDataSetShortAllocPtr(  cmData_t* p, const short* vp,          unsigned cnt )
{
  if( cmIsFlag(p->flags,kDynPtrDtFl) )
    p->u.sp = cmMemResize(short, p->u.sp, cnt );
  else
  {
    _cmDataFreeArray(p);
    p->u.sp = cmMemAlloc(short, cnt );
  }
  p->tid   = kShortPtrDtId;
  p->flags = cmSetFlag(p->flags,kDynPtrDtFl);
  return p;
}

cmData_t* cmDataSetUShortAllocPtr( cmData_t* p, const unsigned short* vp, unsigned cnt )
{
  if( cmIsFlag(p->flags,kDynPtrDtFl) )
    p->u.usp = cmMemResize(unsigned short, p->u.usp, cnt );
  else
  {
    _cmDataFreeArray(p);
    p->u.usp = cmMemAlloc(unsigned short, cnt );
  }
  p->tid   = kUShortPtrDtId;
  p->flags = cmSetFlag(p->flags,kDynPtrDtFl);
}

cmData_t* cmDataSetIntAllocPtr(    cmData_t* p, const int* vp,            unsigned cnt )
{
  if( cmIsFlag(p->flags,kDynPtrDtFl) )
    p->u.ip = cmMemResize(int, p->u.ip, cnt );
  else
  {
    _cmDataFreeArray(p);
    p->u.ip = cmMemAlloc(int, cnt );
  }
  p->tid   = kIntPtrDtId;
  p->flags = cmSetFlag(p->flags,kDynPtrDtFl);
  return p;
}

cmData_t* cmDataSetUIntAllocPtr(   cmData_t* p, const unsigned int* vp,   unsigned cnt )
{
  if( cmIsFlag(p->flags,kDynPtrDtFl) )
    p->u.uip = cmMemResize(unsigned int, p->u.uip, cnt );
  else
  {
    _cmDataFreeArray(p);
    p->u.uip = cmMemAlloc(unsigned int, cnt );
  }
  p->tid   = kUIntPtrDtId;
  p->flags = cmSetFlag(p->flags,kDynPtrDtFl);
  return p;
}


cmData_t* cmDataSetLongAllocPtr(   cmData_t* p, const long* vp,           unsigned cnt )
{
  if( cmIsFlag(p->flags,kDynPtrDtFl) )
    p->u.lp = cmMemResize(long, p->u.lp, cnt );
  else
  {
    _cmDataFreeArray(p);
    p->u.lp = cmMemAlloc(long, cnt );
  }
  p->tid   = kLongPtrDtId;
  p->flags = cmSetFlag(p->flags,kDynPtrDtFl);
  return p;
}


cmData_t* cmDataSetULongAllocPtr(  cmData_t* p, const unsigned long* vp,  unsigned cnt )
{
  if( cmIsFlag(p->flags,kDynPtrDtFl) )
    p->u.ulp = cmMemResize(unsigned long, p->u.ulp, cnt );
  else
  {
    _cmDataFreeArray(p);
    p->u.ulp = cmMemAlloc(unsigned long, cnt );
  }
  p->tid   = kULongPtrDtId;
  p->flags = cmSetFlag(p->flags,kDynPtrDtFl);
  return p;
}


cmData_t* cmDataSetFloatAllocPtr(  cmData_t* p, const float* vp,          unsigned cnt )
{
  if( cmIsFlag(p->flags,kDynPtrDtFl) )
    p->u.fp = cmMemResize(float, p->u.fp, cnt );
  else
  {
    _cmDataFreeArray(p);
    p->u.fp = cmMemAlloc(float, cnt );
  }
  p->tid   = kFloatPtrDtId;
  p->flags = cmSetFlag(p->flags,kDynPtrDtFl);
  return p;
}


cmData_t* cmDataSetDoubleAllocPtr( cmData_t* p, const double* vp,         unsigned cnt )
{
  if( cmIsFlag(p->flags,kDynPtrDtFl) )
    p->u.dp = cmMemResize(double, p->u.dp, cnt );
  else
  {
    _cmDataFreeArray(p);
    p->u.dp = cmMemAlloc(double, cnt );
  }
  p->tid   = kDoublePtrDtId;
  p->flags = cmSetFlag(p->flags,kDynPtrDtFl);
  return p;
}

  

// Dynamically allocate a data object and set it's value.
cmData_t* cmDataAllocChar(   char v )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetChar(p,v);
  return p;
}

cmData_t* cmDataAllocUChar(  unsigned char v )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetUChar(p,v);
  return p;
}

cmData_t* cmDataAllocShort(  short v )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetShort(p,v);
  return p;
}

cmData_t* cmDataAllocUShort( unsigned short v )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetUShort(p,v);
  return p;
}

cmData_t* cmDataAllocInt(    int v )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetInt(p,v);
  return p;
}

cmData_t* cmDataAllocUInt(   unsigned int v )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetUInt(p,v);
  return p;
}

cmData_t* cmDataAllocLong(   long v )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetLong(p,v);
  return p;
}

cmData_t* cmDataAllocULong(  unsigned long v )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetULong(p,v);
  return p;
}

cmData_t* cmDataAllocFloat(  float v )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetFloat(p,v);
  return p;
}

cmData_t* cmDataAllocDouble( double v )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetDouble(p,v);
  return p;
}


cmData_t* cmDataAllocStr( cmChar_t* str )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetStr(p,str);
  return p;
}

cmData_t* cmDataAllocConstStr( const cmChar_t* str )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetConstStr(p,str);
  return p;
}

// Dynamically allocate a data object and set its array value to an external
// array. The data is not copied.
cmData_t* cmDataAllocVoidPtr(   const void* v,           unsigned cnt )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetCharPtr(p,(char*)v,cnt);
  p->tid = kVoidPtrDtId;
  return p;
}

cmData_t* cmDataAllocCharPtr(   const char* v,           unsigned cnt )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetCharPtr(p,(char*)v,cnt);
  return p;
}

cmData_t* cmDataAllocUCharPtr(  const unsigned char* v,  unsigned cnt )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetUCharPtr(p,(unsigned char*)v,cnt);
  return p;
}

cmData_t* cmDataAllocShortPtr(  const short* v,          unsigned cnt )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetShortPtr(p,(short*)v,cnt);
  return p;
}

cmData_t* cmDataAllocUShortPtr( const unsigned short* v, unsigned cnt )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetUShortPtr(p,(unsigned short*)v,cnt);
  return p;
}

cmData_t* cmDataAllocIntPtr(    const int* v,            unsigned cnt )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetIntPtr(p,(int*)v,cnt);
  return p;
}

cmData_t* cmDataAllocUIntPtr(   const unsigned int* v,   unsigned cnt )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetUIntPtr(p,(unsigned int*)v,cnt);
  return p;
}

cmData_t* cmDataAllocLongPtr(   const long* v,           unsigned cnt )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetLongPtr(p,(long*)v,cnt);
  return p;
}

cmData_t* cmDataAllocULongPtr(  const unsigned long* v,  unsigned cnt )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetULongPtr(p,(unsigned long*)v,cnt);
  return p;
}

cmData_t* cmDataAllocFloatPtr(  const float* v,          unsigned cnt )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetFloatPtr(p,(float*)v,cnt);
  return p;
}

cmData_t* cmDataAllocDoublePtr( const double* v,         unsigned cnt )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetDoublePtr(p,(double*)v,cnt);
  return p;
}



// Dynamically allocate a data object and its array value.  
// v[cnt] is copied into the allocated array.
cmData_t* cmDataVoidAllocPtr(   const void* v,           unsigned cnt )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetCharAllocPtr(p, (const char*)v, cnt );
  return p;
}

cmData_t* cmDataCharAllocPtr(   const char* v,           unsigned cnt )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetCharAllocPtr(p, v, cnt );
  return p;
}

cmData_t* cmDataUCharAllocPtr(  const unsigned char* v,  unsigned cnt )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetUCharAllocPtr(p, v, cnt );
  return p;
}

cmData_t* cmDataShortAllocPtr(  const short* v,          unsigned cnt )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetShortAllocPtr(p, v, cnt );
  return p;
}

cmData_t* cmDataUShortAllocPtr( const unsigned short* v, unsigned cnt )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetUShortAllocPtr(p, v, cnt );
  return p;
}

cmData_t* cmDataIntAllocPtr(    const int* v,            unsigned cnt )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetIntAllocPtr(p, v, cnt );
  return p;
}

cmData_t* cmDataUIntAllocPtr(   const unsigned int* v,   unsigned cnt )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetUIntAllocPtr(p, v, cnt );
  return p;
}

cmData_t* cmDataLongAllocPtr(   const long* v,           unsigned cnt )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetLongAllocPtr(p, v, cnt );
  return p;
}

cmData_t* cmDataULongAllocPtr(  const unsigned long* v,  unsigned cnt )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetULongAllocPtr(p, v, cnt );
  return p;  
}

cmData_t* cmDataFloatAllocPtr(  const float* v,          unsigned cnt )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetFloatAllocPtr(p, v, cnt );
  return p;  
}

cmData_t* cmDataDoubleAllocPtr( const double* v,         unsigned cnt )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  cmDataSetDoubleAllocPtr(p, v, cnt );
  return p;  
}


void  cmDataFree( cmData_t* p )
{
  _cmDataFree(p);
}

  
  
unsigned cmDataSerializeByteCount( const cmData_t* p )
{
  unsigned bn = 0;

  // if this data type has a child then calculate it's size
  if( kMinStructDtId <= p->tid && p->tid <= kMaxStructDtId && p->u.child != NULL )
    bn = cmDataSerializeByteCount(p->u.child);
  
  // if this data type has siblings get their type
  cmData_t* dp = p->u.child;
  for(; dp != NULL; dp=dp->sibling )
    bn += cmDataSerializeByteCount(dp->sibling);
    
  // 
  return  _cmDataByteCount(p) + bn;
}



cmDtRC_t cmDataSerialize( const cmData_t* p, void* buf, unsigned bufByteCnt )
{
}

cmDtRC_t cmDataDeserialize( const void* buf, unsigned bufByteCnt, cmData_t** pp )
{
}

void     cmDataPrint( const cmData_t* p, cmRpt_t* rpt )
{
}

void     cmDataTest( cmCtx_t* ctx )
{
}


