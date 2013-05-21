#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmData.h"

cmDtRC_t _cmDataErrNo = kOkDtRC;

cmData_t cmDataNull = { kInvalidDtId,0,NULL,NULL,0 };

typedef struct
{
  cmDataFmtId_t tid;
  unsigned      cnt;
} cmDataSerialHdr_t;

void _cmDataSetError( unsigned err )
{
  _cmDataErrNo = err;
}

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
  if( cmDataIsStruct(p) )
  {
    cmData_t* cp = p->u.child;
    for(; cp!=NULL; cp=cp->sibling)
      _cmDataFree(cp);
  }
    
  _cmDataFreeArray(p);
  if( cmIsFlag(p->flags,kDynObjDtFl) )
    cmMemFree(p);
}

cmData_t* _cmDataAllocNode( cmData_t* parent, cmDataFmtId_t tid )
{
  cmData_t* p = cmMemAllocZ(cmData_t,1);
  p->tid    = tid;
  p->flags  = kDynObjDtFl;
  p->parent = parent;
  if( parent != NULL )
    return cmDataAppendChild(parent,p);
  return p;
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

bool cmDataIsValue(  const cmData_t* p )
{ return kMinValDtId <= p->tid && p->tid <= kMaxValDtId; }

bool cmDataIsPtr(    const cmData_t* p )
{ return kMinPtrDtId <= p->tid && p->tid <= kMaxPtrDtId; }

bool cmDataIsStruct( const cmData_t* p )
{ return kMinStructDtId <= p->tid && p->tid <= kMaxStructDtId; }

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
      _cmDataSetError(kCvtErrDtRC);
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
      _cmDataSetError(kCvtErrDtRC);
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
      _cmDataSetError(kCvtErrDtRC);
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
      _cmDataSetError(kCvtErrDtRC);
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
      _cmDataSetError(kCvtErrDtRC);
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
      _cmDataSetError(kCvtErrDtRC);
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
      _cmDataSetError(kCvtErrDtRC);
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
      _cmDataSetError(kCvtErrDtRC);
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
      _cmDataSetError(kCvtErrDtRC);
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
      _cmDataSetError(kCvtErrDtRC);
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
cmData_t* cmDataSetNull( cmData_t* p )
{
  _cmDataFreeArray(p);
  p->tid = kNullDtId;
  return p;
}

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
  return p;
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
  return p;
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
  memcpy(p->u.cp,vp,sizeof(char)*cnt);
  p->tid   = kCharPtrDtId;
  p->flags = cmSetFlag(p->flags,kDynPtrDtFl);
  p->cnt   = cnt;
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
  memcpy(p->u.ucp,vp,sizeof(unsigned char)*cnt);
  p->tid   = kUCharPtrDtId;
  p->flags = cmSetFlag(p->flags,kDynPtrDtFl);
  p->cnt   = cnt;
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
  memcpy(p->u.sp,vp,sizeof(short)*cnt);
  p->tid   = kShortPtrDtId;
  p->flags = cmSetFlag(p->flags,kDynPtrDtFl);
  p->cnt   = cnt;
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
  memcpy(p->u.usp,vp,sizeof(unsigned short)*cnt);
  p->tid   = kUShortPtrDtId;
  p->flags = cmSetFlag(p->flags,kDynPtrDtFl);
  p->cnt   = cnt;
  return p;
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
  memcpy(p->u.ip,vp,sizeof(int)*cnt);
  p->tid   = kIntPtrDtId;
  p->flags = cmSetFlag(p->flags,kDynPtrDtFl);
  p->cnt   = cnt;
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
  memcpy(p->u.uip,vp,sizeof(unsigned int)*cnt);
  p->tid   = kUIntPtrDtId;
  p->flags = cmSetFlag(p->flags,kDynPtrDtFl);
  p->cnt   = cnt;
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
  memcpy(p->u.lp,vp,sizeof(long)*cnt);
  p->tid   = kLongPtrDtId;
  p->flags = cmSetFlag(p->flags,kDynPtrDtFl);
  p->cnt   = cnt;
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
  memcpy(p->u.ulp,vp,sizeof(unsigned long)*cnt);
  p->tid   = kULongPtrDtId;
  p->flags = cmSetFlag(p->flags,kDynPtrDtFl);
  p->cnt   = cnt;
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
  memcpy(p->u.fp,vp,sizeof(float)*cnt);
  p->tid   = kFloatPtrDtId;
  p->flags = cmSetFlag(p->flags,kDynPtrDtFl);
  p->cnt   = cnt;
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
  memcpy(p->u.dp,vp,sizeof(double)*cnt);
  p->tid   = kDoublePtrDtId;
  p->flags = cmSetFlag(p->flags,kDynPtrDtFl);
  p->cnt   = cnt;
  return p;
}

  

// Dynamically allocate a data object and set it's value.
cmData_t* cmDataAllocNull( cmData_t* parent )
{ return _cmDataAllocNode(parent,kNullDtId); }

cmData_t* cmDataAllocChar(   cmData_t* parent, char v )
{
  cmData_t* p = _cmDataAllocNode(parent,kCharDtId);
  cmDataSetChar(p,v);
  return p;
}

cmData_t* cmDataAllocUChar(  cmData_t* parent, unsigned char v )
{
  cmData_t* p = _cmDataAllocNode(parent,kUCharDtId);
  cmDataSetUChar(p,v);
  return p;
}

cmData_t* cmDataAllocShort(  cmData_t* parent, short v )
{
  cmData_t* p = _cmDataAllocNode(parent,kShortDtId);
  cmDataSetShort(p,v);
  return p;
}

cmData_t* cmDataAllocUShort( cmData_t* parent, unsigned short v )
{
  cmData_t* p = _cmDataAllocNode(parent,kUShortDtId);
  cmDataSetUShort(p,v);
  return p;
}

cmData_t* cmDataAllocInt(    cmData_t* parent, int v )
{
  cmData_t* p = _cmDataAllocNode(parent,kIntDtId);
  cmDataSetInt(p,v);
  return p;
}

cmData_t* cmDataAllocUInt(   cmData_t* parent, unsigned int v )
{
  cmData_t* p = _cmDataAllocNode(parent,kUIntDtId);
  cmDataSetUInt(p,v);
  return p;
}

cmData_t* cmDataAllocLong(   cmData_t* parent, long v )
{
  cmData_t* p = _cmDataAllocNode(parent,kLongDtId);
  cmDataSetLong(p,v);
  return p;
}

cmData_t* cmDataAllocULong(  cmData_t* parent, unsigned long v )
{
  cmData_t* p = _cmDataAllocNode(parent,kULongDtId);
  cmDataSetULong(p,v);
  return p;
}

cmData_t* cmDataAllocFloat(  cmData_t* parent, float v )
{
  cmData_t* p = _cmDataAllocNode(parent,kFloatDtId);
  cmDataSetFloat(p,v);
  return p;
}

cmData_t* cmDataAllocDouble( cmData_t* parent, double v )
{
  cmData_t* p = _cmDataAllocNode(parent,kDoubleDtId);
  cmDataSetDouble(p,v);
  return p;
}


// Dynamically allocate a data object and set its array value to an external
// array. The data is not copied.
cmData_t* cmDataAllocStr( cmData_t* parent, cmChar_t* str )
{
  cmData_t* p = _cmDataAllocNode(parent,kStrDtId);
  cmDataSetStr(p,str);
  return p;
}

cmData_t* cmDataAllocConstStr( cmData_t* parent, const cmChar_t* str )
{
  cmData_t* p = _cmDataAllocNode(parent,kConstStrDtId);
  cmDataSetConstStr(p,str);
  return p;
}

cmData_t* cmDataAllocCharPtr(   cmData_t* parent, char* v,  unsigned cnt )
{
  cmData_t* p = _cmDataAllocNode(parent,kCharPtrDtId);
  cmDataSetCharPtr(p,(char*)v,cnt);
  return p;
}

cmData_t* cmDataAllocUCharPtr(  cmData_t* parent, unsigned char* v,  unsigned cnt )
{
  cmData_t* p = _cmDataAllocNode(parent,kUCharPtrDtId);
  cmDataSetUCharPtr(p,(unsigned char*)v,cnt);
  return p;
}

cmData_t* cmDataAllocShortPtr(  cmData_t* parent, short* v, unsigned cnt )
{
  cmData_t* p = _cmDataAllocNode(parent,kShortPtrDtId);
  cmDataSetShortPtr(p,(short*)v,cnt);
  return p;
}

cmData_t* cmDataAllocUShortPtr( cmData_t* parent, unsigned short* v, unsigned cnt )
{
  cmData_t* p = _cmDataAllocNode(parent,kUShortPtrDtId);
  cmDataSetUShortPtr(p,(unsigned short*)v,cnt);
  return p;
}

cmData_t* cmDataAllocIntPtr(    cmData_t* parent, int* v, unsigned cnt )
{
  cmData_t* p = _cmDataAllocNode(parent,kIntPtrDtId);
  cmDataSetIntPtr(p,(int*)v,cnt);
  return p;
}

cmData_t* cmDataAllocUIntPtr(   cmData_t* parent, unsigned int* v,  unsigned cnt )
{
  cmData_t* p = _cmDataAllocNode(parent,kUIntPtrDtId);
  cmDataSetUIntPtr(p,(unsigned int*)v,cnt);
  return p;
}

cmData_t* cmDataAllocLongPtr(   cmData_t* parent, long* v,  unsigned cnt )
{
  cmData_t* p = _cmDataAllocNode(parent,kLongPtrDtId);
  cmDataSetLongPtr(p,(long*)v,cnt);
  return p;
}

cmData_t* cmDataAllocULongPtr(  cmData_t* parent, unsigned long* v,  unsigned cnt )
{
  cmData_t* p = _cmDataAllocNode(parent,kULongPtrDtId);
  cmDataSetULongPtr(p,(unsigned long*)v,cnt);
  return p;
}

cmData_t* cmDataAllocFloatPtr(  cmData_t* parent, float* v, unsigned cnt )
{
  cmData_t* p = _cmDataAllocNode(parent,kFloatPtrDtId);
  cmDataSetFloatPtr(p,(float*)v,cnt);
  return p;
}

cmData_t* cmDataAllocDoublePtr( cmData_t* parent, double* v,  unsigned cnt )
{
  cmData_t* p = _cmDataAllocNode(parent,kDoublePtrDtId);
  cmDataSetDoublePtr(p,(double*)v,cnt);
  return p;
}

cmData_t* cmDataAllocVoidPtr(   cmData_t* parent, void* v,  unsigned cnt )
{
  cmData_t* p = _cmDataAllocNode(parent,kVoidPtrDtId);
  cmDataSetCharPtr(p,(char*)v,cnt);
  p->tid = kVoidPtrDtId;
  return p;
}



// Dynamically allocate a data object and its array value.  
// v[cnt] is copied into the allocated array.
cmData_t* cmDataStrAlloc( cmData_t* parent, cmChar_t* str )
{
  cmData_t* p = _cmDataAllocNode(parent,kStrDtId);
  cmDataSetStrAlloc(p,str);
  return p;
}

cmData_t* cmDataConstStrAlloc( cmData_t* parent, const cmChar_t* str )
{
  cmData_t* p = _cmDataAllocNode(parent,kConstStrDtId);
  cmDataSetConstStrAlloc(p,str);
  return p;
}

cmData_t* cmDataCharAllocPtr(   cmData_t* parent, const char* v,  unsigned cnt )
{
  cmData_t* p = _cmDataAllocNode(parent,kCharPtrDtId);
  cmDataSetCharAllocPtr(p, v, cnt );
  return p;
}

cmData_t* cmDataUCharAllocPtr(  cmData_t* parent, const unsigned char* v,  unsigned cnt )
{
  cmData_t* p = _cmDataAllocNode(parent,kUCharPtrDtId);
  cmDataSetUCharAllocPtr(p, v, cnt );
  return p;
}

cmData_t* cmDataShortAllocPtr(  cmData_t* parent, const short* v, unsigned cnt )
{
  cmData_t* p = _cmDataAllocNode(parent,kShortPtrDtId);
  cmDataSetShortAllocPtr(p, v, cnt );
  return p;
}

cmData_t* cmDataUShortAllocPtr( cmData_t* parent, const unsigned short* v, unsigned cnt )
{
  cmData_t* p = _cmDataAllocNode(parent,kUShortPtrDtId);
  cmDataSetUShortAllocPtr(p, v, cnt );
  return p;
}

cmData_t* cmDataIntAllocPtr(    cmData_t* parent, const int* v,   unsigned cnt )
{
  cmData_t* p = _cmDataAllocNode(parent,kIntPtrDtId);
  cmDataSetIntAllocPtr(p, v, cnt );
  return p;
}

cmData_t* cmDataUIntAllocPtr(   cmData_t* parent, const unsigned int* v,   unsigned cnt )
{
  cmData_t* p = _cmDataAllocNode(parent,kUIntPtrDtId);
  cmDataSetUIntAllocPtr(p, v, cnt );
  return p;
}

cmData_t* cmDataLongAllocPtr(   cmData_t* parent, const long* v,  unsigned cnt )
{
  cmData_t* p = _cmDataAllocNode(parent,kLongPtrDtId);
  cmDataSetLongAllocPtr(p, v, cnt );
  return p;
}

cmData_t* cmDataULongAllocPtr(  cmData_t* parent, const unsigned long* v,  unsigned cnt )
{
  cmData_t* p = _cmDataAllocNode(parent,kULongPtrDtId);
  cmDataSetULongAllocPtr(p, v, cnt );
  return p;  
}

cmData_t* cmDataFloatAllocPtr(  cmData_t* parent, const float* v, unsigned cnt )
{
  cmData_t* p = _cmDataAllocNode(parent,kFloatPtrDtId);
  cmDataSetFloatAllocPtr(p, v, cnt );
  return p;  
}

cmData_t* cmDataDoubleAllocPtr( cmData_t* parent, const double* v,  unsigned cnt )
{
  cmData_t* p = _cmDataAllocNode(parent,kDoublePtrDtId);
  cmDataSetDoubleAllocPtr(p, v, cnt );
  return p;  
}


cmData_t* cmDataVoidAllocPtr(   cmData_t* parent, const void* v,  unsigned cnt )
{
  cmData_t* p = _cmDataAllocNode(parent,kVoidPtrDtId);
  cmDataSetCharAllocPtr(p, (const char*)v, cnt );
  p->tid = kVoidPtrDtId;
  return p;
}

void  cmDataFree( cmData_t* p )
{
  _cmDataFree(p);
}

cmData_t* cmDataUnlink( cmData_t* p )
{
  if( p->parent == NULL )
    return p;

  assert( cmDataIsStruct(p->parent) );

  cmData_t* cp = p->u.child;
  cmData_t* pp = NULL;
  for(; cp!=NULL; cp=cp->sibling)
    if( cp == p )
    {
      if( pp == NULL )
        p->parent->u.child = p->sibling;
      else
        pp->sibling = cp->sibling;
    }
  return p;
}

void cmDataUnlinkAndFree( cmData_t* p )
{
  cmDataUnlink(p);
  cmDataFree(p);
}

cmData_t* _cmDataDupl( const cmData_t* p, cmData_t* parent )
{
  cmData_t* rp = NULL;

  switch( p->tid )
  {
    case kNullDtId:      rp = cmDataAllocNull(parent);          break;
    case kUCharDtId:     rp = cmDataAllocUChar(parent,p->u.uc); break; 
    case kCharDtId:      rp = cmDataAllocChar(parent,p->u.c);   break;
    case kUShortDtId:    rp = cmDataAllocShort(parent,p->u.us); break;
    case kShortDtId:     rp = cmDataAllocUShort(parent,p->u.s); break;
    case kUIntDtId:      rp = cmDataAllocUInt(parent,p->u.i);   break;
    case kIntDtId:       rp = cmDataAllocInt(parent,p->u.ui);   break;
    case kULongDtId:     rp = cmDataAllocULong(parent,p->u.ul); break;
    case kLongDtId:      rp = cmDataAllocLong(parent,p->u.l);   break;
    case kFloatDtId:     rp = cmDataAllocFloat(parent,p->u.f);  break;
    case kDoubleDtId:    rp = cmDataAllocDouble(parent,p->u.d); break;

    case kStrDtId:       rp = cmDataStrAlloc(parent,p->u.z);                break;
    case kConstStrDtId:  rp = cmDataConstStrAlloc(parent,p->u.cz);          break;
    case kUCharPtrDtId:  rp = cmDataUCharAllocPtr(parent,p->u.ucp,p->cnt);  break;
    case kCharPtrDtId:   rp = cmDataCharAllocPtr(parent,p->u.cp,p->cnt);    break;
    case kUShortPtrDtId: rp = cmDataUShortAllocPtr(parent,p->u.usp,p->cnt); break;
    case kShortPtrDtId:  rp = cmDataShortAllocPtr(parent,p->u.sp,p->cnt);   break;
    case kUIntPtrDtId:   rp = cmDataUIntAllocPtr(parent,p->u.uip,p->cnt);   break;
    case kIntPtrDtId:    rp = cmDataIntAllocPtr(parent,p->u.ip,p->cnt);     break;
    case kULongPtrDtId:  rp = cmDataULongAllocPtr(parent,p->u.ulp,p->cnt);  break;
    case kLongPtrDtId:   rp = cmDataLongAllocPtr(parent,p->u.lp,p->cnt);    break;
    case kFloatPtrDtId:  rp = cmDataFloatAllocPtr(parent,p->u.fp,p->cnt);   break;
    case kDoublePtrDtId: rp = cmDataDoubleAllocPtr(parent,p->u.dp,p->cnt);  break;
    case kVoidPtrDtId:   rp = cmDataVoidAllocPtr(parent,p->u.vp,p->cnt);    break;

    case kListDtId:      
    case kPairDtId:  
    case kRecordDtId:
      {
        rp = _cmDataAllocNode(parent,p->tid);
        cmData_t* cp  = p->u.child;
        for(; cp!=NULL; cp=cp->sibling)
          cmDataAppendChild(rp,_cmDataDupl(cp,rp));
      }
      break;

    default:
      assert(0);
  }

  return rp;
}

cmData_t* cmDataDupl( const cmData_t* p )
{ return _cmDataDupl(p,NULL); }

cmData_t* cmDataReplace( cmData_t* dst, cmData_t* src )
{
  if( dst->parent == NULL )
  {
    cmDataUnlinkAndFree(dst);
    src->parent = NULL;
    return src;
  }

  cmData_t* parent = dst->parent;
  cmData_t* cp     = parent->u.child;
  cmData_t* pp     = NULL;
  unsigned  i      = 0;
  unsigned  n      = cmDataChildCount(parent);

  // locate dst's right sibling
  for(i=0; i<n; ++i,cp=cp->sibling)
  {
    
    if( cp == dst )
    {
      // link in 'src' in place of 'dst'
      src->sibling = dst->sibling;

      // free dst
      cmDataUnlinkAndFree(dst);

      // update the sibling link to 
      if( pp == NULL )
        parent->u.child = src;
      else
        pp->sibling = src;

      src->parent = parent;
      break;
    }
    pp = cp;
  }
  return src;
}


unsigned cmDataChildCount( const cmData_t* p )
{
  if( !cmDataIsStruct(p) )
    return 0;

  unsigned n = 0;
  const cmData_t* cp = p->u.child;
  for(; cp!=NULL; cp=cp->sibling)
    ++n;

  return n;
}

cmData_t* cmDataChild( cmData_t* p, unsigned index )
{
  if( !cmDataIsStruct(p) )
    return NULL;

  unsigned  n  = 0;
  cmData_t* cp = p->u.child;
  for(; cp!=NULL; cp=cp->sibling)
  {
    if( n == index )
      break;
    ++n;
  }

  return cp;
}

cmData_t* cmDataPrependChild(cmData_t* parent, cmData_t* p )
{
  assert( cmDataIsStruct(p) );
  
  p->u.child    = parent->u.child;
  parent->u.child = p;
  return p;
}

cmData_t* cmDataAppendChild( cmData_t* parent, cmData_t* p )
{
  assert( cmDataIsStruct(parent) );
  assert( parent->tid != kRecordDtId || (parent->tid == kRecordDtId && p->tid==kPairDtId));

  cmData_t* cp = parent->u.child;
  if( cp == NULL )
    parent->u.child = p;
  else
  {
    for(; cp!=NULL; cp=cp->sibling)
      if( cp->sibling == NULL )
      {
        cp->sibling = p;
        break;
      }
  }

  p->sibling = NULL;
  return p;
}

cmData_t* cmDataInsertChild( cmData_t* parent, unsigned index, cmData_t* p )
{
  if( !cmDataIsStruct(parent) )
    return NULL;

  unsigned  n  = 0;
  cmData_t* cp = parent->u.child;
  cmData_t* pp = NULL;
  for(; cp!=NULL; cp=cp->sibling)
  {
    if( n == index )
    {
      if( pp == NULL )
      {
        parent->u.child = p;
        p->sibling = NULL;
      }
      else
      {
        p->sibling  = pp->sibling;
        pp->sibling = p;
      }
      break;
        
    }
    ++n;
  }

  return p;
  
}

//----------------------------------------------------------------------------

bool _cmDataPairIsValid( const cmData_t* p )
{
  assert( p->tid == kPairDtId );

  const cmData_t* cp = p->u.child;
  bool fl = cp->u.child == NULL || cp->u.child->sibling == NULL || cp->u.child->sibling->sibling!=NULL;
  return !fl;
}

// Get the key/value of a pair
cmData_t* cmDataPairKey(          cmData_t* p )
{
  assert( _cmDataPairIsValid(p) );
  return p->u.child;
}

unsigned  cmDataPairKeyId(    cmData_t* p )
{
  assert( _cmDataPairIsValid(p) );
  return cmDataGetUInt(p->u.child);
}

const cmChar_t* cmDataPairKeyLabel( cmData_t* p )
{
  assert( _cmDataPairIsValid(p) );
  return cmDataGetConstStr(p->u.child);
}


cmData_t* cmDataPairValue(        cmData_t* p )
{
  assert( _cmDataPairIsValid(p) );
  return p->u.child->sibling;
}
    
  // Set the key or value of an existing pair node. 
cmData_t* cmDataPairSetValue( cmData_t* p, cmData_t* value )
{
  assert( _cmDataPairIsValid(p) );
  cmDataReplace( cmDataPairValue(p), value );
  return p;
}


cmData_t* cmDataPairSetKey(   cmData_t* p, cmData_t* key )
{
  assert( _cmDataPairIsValid(p) );
  cmDataReplace( cmDataPairValue(p), key );
  return p;
}

cmData_t* cmDataPairSetKeyId( cmData_t* p, unsigned id )
{
  assert( _cmDataPairIsValid(p) );
  cmDataSetUInt(p->u.child,id);
  return p;
}

cmData_t* cmDataPairSetKeyLabel(  cmData_t* p, const cmChar_t* label )
{
  assert( _cmDataPairIsValid(p) );
  cmDataSetConstStrAlloc(p->u.child,label);
  return p;  
}


cmData_t* cmDataMakePair( cmData_t* parent, cmData_t* p, cmData_t* key, cmData_t* value )
{
  _cmDataFree(p);
  p->tid    = kPairDtId;
  p->parent = parent;
  p->flags   = 0;
  p->u.child = NULL;
  return p;
}

// Dynamically allocate a pair node 
cmData_t* cmDataAllocPair(  cmData_t* parent, const cmData_t* key, const cmData_t* value )
{
  cmData_t* p  = _cmDataAllocNode(parent,kPairDtId);
  cmData_t* kp = cmDataDupl(key);
  cmData_t* vp = cmDataDupl(value);
  cmDataPrependChild(p,vp);
  cmDataPrependChild(p,kp);
  return p;
}

cmData_t* cmDataAllocPairId(cmData_t* parent, unsigned  keyId,  cmData_t* value )
{
  cmData_t* p  = _cmDataAllocNode(parent,kPairDtId);
  cmDataAllocUInt(p,keyId);
  cmDataAppendChild(p,value);
  return p;
}

cmData_t* cmDataAllocPairLabel( cmData_t* parent, const cmChar_t *label, cmData_t* value )
{
  cmData_t* p  = _cmDataAllocNode(parent,kPairDtId);
  cmDataConstStrAlloc(p,label);
  cmDataAppendChild(p,value);
  return p;
}
  
//----------------------------------------------------------------------------

unsigned  cmDataListCount(const cmData_t* p )
{ return cmDataChildCount(p);  }

cmData_t* cmDataListEle(    cmData_t* p, unsigned index )
{ return cmDataChild(p,index); }

cmData_t* cmDataListMake(  cmData_t* parent, cmData_t* p )
{
  _cmDataFree(p);
  p->parent  = parent;
  p->tid     = kListDtId;
  p->flags   = 0;
  p->u.child = NULL;
  return p;
}

cmData_t* cmDataListAlloc( cmData_t* parent)
{ return _cmDataAllocNode(parent,kListDtId); }

cmDtRC_t  _cmDataParseArgV( cmData_t* parent, va_list vl, cmData_t** vpp )
{
  cmDtRC_t  rc  = kOkDtRC;
  cmData_t* vp  = NULL;
  unsigned  tid = va_arg(vl,unsigned);
    
  switch(tid)
  {
    case kInvalidDtId:    rc = kEolDtRC; break;
    case kNullDtId:       vp = cmDataAllocNull(parent);                                break;
    case kUCharDtId:      vp = cmDataAllocUChar(    parent,va_arg(vl,int));            break;
    case kCharDtId:       vp = cmDataAllocChar(     parent,va_arg(vl,int));            break;
    case kUShortDtId:     vp = cmDataAllocUShort(   parent,va_arg(vl,int));            break;
    case kShortDtId:      vp = cmDataAllocShort(    parent,va_arg(vl,int));            break;
    case kUIntDtId:       vp = cmDataAllocUInt(     parent,va_arg(vl,unsigned int));   break;
    case kIntDtId:        vp = cmDataAllocInt(      parent,va_arg(vl,int));            break;
    case kULongDtId:      vp = cmDataAllocULong(    parent,va_arg(vl,unsigned long));  break;
    case kLongDtId:       vp = cmDataAllocLong(     parent,va_arg(vl,long));           break;
    case kFloatDtId:      vp = cmDataAllocFloat(    parent,va_arg(vl,double));         break;
    case kDoubleDtId:     vp = cmDataAllocDouble(   parent,va_arg(vl,double));         break;

    case kStrDtId:        vp = cmDataStrAlloc(      parent,va_arg(vl,cmChar_t*));       break;
    case kConstStrDtId:   vp = cmDataConstStrAlloc( parent,va_arg(vl,const cmChar_t*)); break;

    case kUCharPtrDtId:   
      {
        unsigned char* p = va_arg(vl,unsigned char*);
        vp = cmDataUCharAllocPtr(parent, p, va_arg(vl,unsigned)); 
      }
      break;

    case kCharPtrDtId:    
      {
        char* p = va_arg(vl,char*);
        vp = cmDataCharAllocPtr(parent, p,  va_arg(vl,unsigned)); 
      }
      break;

    case kUShortPtrDtId:  
      {
        unsigned short* p = va_arg(vl,unsigned short*);
        vp = cmDataUShortAllocPtr(parent, p, va_arg(vl,unsigned)); 
      }
      break;

    case kShortPtrDtId:   
      {
        short* p = va_arg(vl,short*);
        vp = cmDataShortAllocPtr(parent, p, va_arg(vl,unsigned)); 
      }
      break;

    case kUIntPtrDtId:    
      {
        unsigned int* p = va_arg(vl,unsigned int*);
        vp = cmDataUIntAllocPtr(parent, p, va_arg(vl,unsigned)); 
      }
      break;

    case kIntPtrDtId:     
      {
        int * p = va_arg(vl,int*);
        vp = cmDataIntAllocPtr(parent, p,  va_arg(vl,unsigned)); 
      }
      break;

    case kULongPtrDtId:   
      {
        unsigned long* p = va_arg(vl,unsigned long*);
        vp = cmDataULongAllocPtr(parent, p, va_arg(vl,unsigned)); 
      }
      break;

    case kLongPtrDtId:    
      {
        long* p = va_arg(vl,long*);
        vp = cmDataLongAllocPtr(parent, p, va_arg(vl,unsigned)); 
      }
      break;

    case kFloatPtrDtId:   
      {
        float* p = va_arg(vl,float*);
        vp = cmDataFloatAllocPtr(parent, p, va_arg(vl,unsigned)); 
      }
      break;

    case kDoublePtrDtId:  
      {
        double* p = va_arg(vl,double*);
        vp = cmDataDoubleAllocPtr(parent,p, va_arg(vl,unsigned)); 
      }
      break;

    case kVoidPtrDtId:    
      {
        void* p = va_arg(vl,void*);
        vp = cmDataVoidAllocPtr(parent, p, va_arg(vl,unsigned)); 
      }
      break;

    case kListDtId:
    case kPairDtId:
    case kRecordDtId:
      vp = _cmDataAllocNode(parent,tid);
      break;

    default:
      _cmDataSetError(kVarArgErrDtRC);
      break;
  }

  *vpp = vp;

  return rc;
}

cmData_t* _cmDataListParseV(cmData_t* parent, va_list vl )
{
  cmData_t* p      = NULL;
  bool      contFl = true;

  while( contFl )
  {  
    cmData_t* vp;
    cmDtRC_t rc = _cmDataParseArgV(parent, vl, &vp);
    
    if(rc != kOkDtRC || cmDataAppendChild(parent,vp)==NULL )    
      contFl = false;      

  }
  return p;
}

cmData_t* cmDataListAllocV(cmData_t* parent, va_list vl )
{
  cmData_t* p = cmDataListAlloc(parent);
  _cmDataListParseV(p, vl );
  return p;
}

cmData_t* cmDataListAllocA(cmData_t* parent,  ... )
{
  va_list vl;
  va_start(vl,parent);
  cmData_t* p = cmDataListAllocV(parent,vl);
  va_end(vl);
  return p;
}
  

cmData_t* cmDataListAppendEle( cmData_t* p, cmData_t* ele )
{ 
  assert(p->tid == kListDtId);
  return cmDataAppendChild(p,ele);
}

cmData_t* cmDataListAppendEleN(cmData_t* p, cmData_t* ele[], unsigned n )
{
  assert(p->tid == kListDtId);

  cmData_t* rp = NULL;
  unsigned i;
  for(i=0; i<n; ++i)
  {
    cmData_t* ep = cmDataAppendChild(p,ele[i]);
    if( rp == NULL )
      rp = ep;
  }
  return rp;
}

cmDtRC_t  cmDataListAppendV(   cmData_t* p, va_list vl )
{
  if( _cmDataListParseV(p, vl ) == NULL )
    return _cmDataErrNo;

  return kOkDtRC;
}

cmDtRC_t  cmDataListAppend(    cmData_t* p, ... )
{
  va_list vl;  
  va_start(vl,p);
  cmDtRC_t rc = cmDataListAppendV(p,vl); 
  va_end(vl);
  return rc;
}

cmData_t* cmDataListInsertEle( cmData_t* p, unsigned index, cmData_t* ele )
{ return cmDataInsertChild(p,index,ele); }

cmData_t* cmDataListInsertEleN(cmData_t* p, unsigned index, cmData_t* ele[], unsigned n )
{
  unsigned i;
  for(i=0; i<n; ++i)
    cmDataListInsertEle(p,index+i,ele[i]);
  return p;
}


//----------------------------------------------------------------------------
unsigned       cmDataRecdCount(    const cmData_t* p )
{ 
  assert( p->tid == kRecordDtId );
  return cmDataChildCount(p);
}

cmData_t*       cmDataRecdEle(      cmData_t* p, unsigned index )
{
  assert( p->tid == kRecordDtId );
  cmData_t* cp = cmDataChild(p,index);
  assert( p->tid == kPairDtId );
  return cp;
}

cmData_t*       cmDataRecdValueFromIndex(    cmData_t* p, unsigned index )
{
  assert( p->tid == kRecordDtId );
  cmData_t* cp =  cmDataChild(p,index);
  assert( p->tid == kPairDtId );
  return cmDataPairValue(cp);
}

cmData_t*       cmDataRecdValueFromId(    cmData_t* p, unsigned id )
{
  assert( p->tid == kRecordDtId );
  cmData_t* cp =  p->u.child;
  for(; cp!=NULL; cp=cp->sibling)
    if( cmDataPairKeyId(cp) == id )
      break;
      
  assert( cp!=NULL && cp->tid==kPairDtId );

  return cmDataPairValue(cp);
}

cmData_t*       cmDataRecdValueFromLabel(    cmData_t* p, const cmChar_t* label )
{
  assert( p->tid == kRecordDtId );
  cmData_t* cp =  p->u.child;
  for(; cp!=NULL; cp=cp->sibling)
  {
    const cmChar_t* lp = cmDataPairKeyLabel(cp);

    if( lp!=NULL && strcmp(lp,label)==0 )
      break;
  }    
  assert( cp!=NULL && cp->tid==kPairDtId );

  return cmDataPairValue(cp);
}

cmData_t*       cmDataRecdKey(      cmData_t* p, unsigned index )
{
  assert( p->tid == kRecordDtId );
  cmData_t* cp =  cmDataChild(p,index);
  assert( p->tid == kPairDtId );
  return cmDataPairKey(cp);
}

unsigned        cmDataRecdKeyId(    cmData_t* p, unsigned index )
{
  cmData_t* kp = cmDataRecdKey(p,index);
  return cmDataGetUInt(kp);
}

const cmChar_t* cmDataRecdKeyLabel( cmData_t* p, unsigned index )
{
  cmData_t* kp = cmDataRecdKey(p,index);
  return cmDataGetConstStr(kp);
}
  
cmData_t*       cmRecdMake( cmData_t* parent, cmData_t* p )
{
  _cmDataFree(p);
  p->parent  = parent;
  p->tid     = kRecordDtId;
  p->flags   = 0;
  p->u.child = NULL;
  return p;
}

cmData_t*       cmRecdAlloc(cmData_t* parent)
{ return _cmDataAllocNode(parent,kRecordDtId); }

cmData_t* cmRecdAppendPair( cmData_t* p, cmData_t* pair )
{
  assert( p!=NULL && p->tid==kRecordDtId);
  cmDataAppendChild(p,pair);
  return p;
}


cmDtRC_t  _cmDataRecdParseV(cmData_t* parent, unsigned idFl, va_list vl )
{
  assert( parent != NULL && parent->tid == kRecordDtId );
  bool      contFl = true;
  cmDtRC_t  rc     = kOkDtRC;

  // for each record field
  while( contFl )
  {  
    cmData_t*       vp    = NULL;
    unsigned        id    = cmInvalidId;
    const cmChar_t* label = NULL;

    // parse the field idenfier
    if( idFl )
      id    = va_arg(vl,unsigned);     // numeric field identifier
    else
      label = va_arg(vl,const char*);  // text field label identifier
    
    // validate the field identifier
    if( id==kInvalidDtId || label==NULL )
      break;

    // parse the field data
    if((rc =_cmDataParseArgV( NULL, vl, &vp )) != kOkDtRC )
    {
      contFl = false;
    }
    else
    {
      // create the field pair
      if( idFl )
        cmDataAllocPairId(parent,id,vp);
      else
        cmDataAllocPairLabel(parent,label,vp);
    }
  }
  return rc;
}

cmData_t*       cmDataRecdAllocLabelV( cmData_t* parent, va_list vl )
{
  cmData_t* p = cmRecdAlloc(parent);
  cmDtRC_t rc = _cmDataRecdParseV(p, false, vl );
  if( rc != kOkDtRC )
  {
    cmDataFree(p);
    p = NULL;
  }
  return p;
}

cmData_t*       cmDataRecdAllocLabelA( cmData_t* parent, ... )
{
  va_list vl;
  va_start(vl,parent);
  cmData_t* p = cmDataRecdAllocLabelV(parent,vl);
  va_end(vl);
  return p;
}

cmData_t*       cmDataRecdAllocIdV( cmData_t* parent, va_list vl )
{
  cmData_t* p = cmRecdAlloc(parent);
  cmDtRC_t rc = _cmDataRecdParseV(p, true, vl );
  if( rc != kOkDtRC )
  {
    cmDataFree(p);
    p = NULL;
  }
  return p;
}

cmData_t*       cmDataRecdAllocIdA( cmData_t* parent, ... )
{
  va_list vl;
  va_start(vl,parent);
  cmData_t* p = cmDataRecdAllocIdV(parent,vl);
  va_end(vl);
  return p;
}

//----------------------------------------------------------------------------  
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
  return kOkDtRC;
}

cmDtRC_t cmDataDeserialize( const void* buf, unsigned bufByteCnt, cmData_t** pp )
{
  return kOkDtRC;
}

#define parr(rpt,fmt,arr,n) do{int i=0; cmRptPrintf(rpt,"[ "); for(;i<n;++i) cmRptPrintf(rpt,fmt,arr[i]); cmRptPrintf(rpt," ]"); }while(0) 

void _cmDataPrintIndent( cmRpt_t* rpt, unsigned indent )
{
  unsigned j=0;
  for(; j<indent; ++j)
    cmRptPrintf(rpt," ");
}

void     _cmDataPrint( const cmData_t* p, cmRpt_t* rpt, unsigned indent )
{
  cmData_t* cp;

  //_cmDataPrintIndent(rpt,indent);

  switch(p->tid)
  {
    case kNullDtId:       cmRptPrintf(rpt,"<null>"); break;
    case kUCharDtId:      cmRptPrintf(rpt,"%c ",cmDataGetUChar(p));  break;
    case kCharDtId:       cmRptPrintf(rpt,"%c ",cmDataGetChar(p));   break;
    case kUShortDtId:     cmRptPrintf(rpt,"%i ",cmDataGetUShort(p)); break;
    case kShortDtId:      cmRptPrintf(rpt,"%i ",cmDataGetShort(p));  break;
    case kUIntDtId:       cmRptPrintf(rpt,"%i ",cmDataGetUInt(p));   break;
    case kIntDtId:        cmRptPrintf(rpt,"%i ",cmDataGetInt(p));    break;
    case kULongDtId:      cmRptPrintf(rpt,"%i ",cmDataGetULong(p));  break;
    case kLongDtId:       cmRptPrintf(rpt,"%i ",cmDataGetLong(p));   break;
    case kFloatDtId:      cmRptPrintf(rpt,"%f ",cmDataGetFloat(p));  break;
    case kDoubleDtId:     cmRptPrintf(rpt,"%f ",cmDataGetDouble(p)); break;

    case kStrDtId:        cmRptPrintf(rpt,"%s ",cmDataGetStr(p));      break;
    case kConstStrDtId:   cmRptPrintf(rpt,"%s ",cmDataGetConstStr(p)); break;

    case kUCharPtrDtId:   parr(rpt,"%c ",cmDataGetUCharPtr(p), p->cnt); break;
    case kCharPtrDtId:    parr(rpt,"%c ",cmDataGetCharPtr(p),  p->cnt); break;
    case kUShortPtrDtId:  parr(rpt,"%i ",cmDataGetUShortPtr(p),p->cnt); break;
    case kShortPtrDtId:   parr(rpt,"%i ",cmDataGetShortPtr(p), p->cnt); break;
    case kUIntPtrDtId:    parr(rpt,"%i ",cmDataGetUIntPtr(p),  p->cnt); break;
    case kIntPtrDtId:     parr(rpt,"%i ",cmDataGetIntPtr(p),   p->cnt); break;
    case kULongPtrDtId:   parr(rpt,"%i ",cmDataGetULongPtr(p), p->cnt); break;
    case kLongPtrDtId:    parr(rpt,"%i ",cmDataGetLongPtr(p),  p->cnt); break;
    case kFloatPtrDtId:   parr(rpt,"%f ",cmDataGetFloatPtr(p), p->cnt); break;
    case kDoublePtrDtId:  parr(rpt,"%f ",cmDataGetDoublePtr(p),p->cnt); break;

    case kVoidPtrDtId:    cmRptPrintf(rpt,"<void:%i>",p->cnt); break;

    case kPairDtId:
      _cmDataPrint(p->u.child,rpt,indent);
      cmRptPrintf(rpt," : ");
      _cmDataPrint(p->u.child->sibling,rpt,indent);
      cmRptPrintf(rpt,"\n");
      break;

    case kListDtId:
      cmRptPrintf(rpt,"(\n");
      indent += 2;
      cp = p->u.child;
      for(; cp!=NULL; cp=cp->sibling)
      {
        _cmDataPrintIndent(rpt,indent);
        _cmDataPrint(cp,rpt,indent);
        cmRptPrintf(rpt,"\n");
      }
      indent -= 2;
      _cmDataPrintIndent(rpt,indent);
      cmRptPrintf(rpt,")\n");
      break;

    case kRecordDtId:
      cmRptPrintf(rpt,"{\n");
      indent += 2;
      cp = p->u.child;
      for(; cp!=NULL; cp=cp->sibling)
      {
        _cmDataPrintIndent(rpt,indent);
        _cmDataPrint(cp,rpt, indent);        
      }
      indent -= 2;
      _cmDataPrintIndent(rpt,indent);
      cmRptPrintf(rpt,"}\n");
      break;

    default:
      break;
  }
}

void cmDataPrint( const cmData_t* p, cmRpt_t* rpt )
{ _cmDataPrint(p,rpt,0); }

void     cmDataTest( cmCtx_t* ctx )
{
  float farr[] = { 1.23, 45.6, 7.89 };

  cmData_t* d0 = cmDataRecdAllocLabelA(NULL,
    "name",kConstStrDtId,"This is a string.",
    "id",  kUIntDtId,    21,
    "real",kFloatDtId, 1.23,
    "arr", kFloatPtrDtId, farr, 3,
    NULL);
  
  cmDataPrint(d0,&ctx->rpt);
  cmDataFree(d0);

  cmData_t* d1 = cmDataListAllocA(NULL,
    kUIntDtId, 53,
    kStrDtId, "Blah blah",
    kFloatPtrDtId, farr, 3,
    kInvalidDtId );


  cmDataPrint(d1,&ctx->rpt);
  cmDataFree(d1);



  cmRptPrintf(&ctx->rpt,"Done!.\n");
}


