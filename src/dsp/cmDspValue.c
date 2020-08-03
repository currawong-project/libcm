#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmJson.h"

#include "cmDspValue.h"

cmDspValue_t cmDspNullValue = { 0 };

const cmDspValue_t* _vcptr( const cmDspValue_t* vp )
{
  while( cmIsFlag(vp->flags, kProxyDsvFl ) )
    vp = vp->u.vp;

  return vp;
}


cmDspValue_t* _vptr( cmDspValue_t* vp )
{
  while( cmIsFlag(vp->flags,kProxyDsvFl) )
    vp = vp->u.vp;

  return vp;
}

void** _cmDsvGetDataPtrPtr( cmDspValue_t* vp )
{
  if( cmDsvIsMtx(vp) )
    return &vp->u.m.u.vp;

  if( cmIsFlag(vp->flags,kStrzDsvFl) )
    return (void**)&vp->u.z;

  return NULL;

}

unsigned _cmDsvBasicType( unsigned flags )
{ return flags & (kTypeDsvMask & (~kMtxDsvFl) ); }

unsigned _cmDsvEleByteCount( unsigned flags )
{
 
  switch( _cmDsvBasicType(flags) )
  {
    case kBoolDsvFl:  return sizeof(bool);
    case kCharDsvFl:  return sizeof(char);
    case kUCharDsvFl: return sizeof(unsigned char);
    case kShortDsvFl: return sizeof(short);
    case kUShortDsvFl:return sizeof(unsigned short);
    case kLongDsvFl:  return sizeof(long);
    case kULongDsvFl: return sizeof(unsigned long);
    case kIntDsvFl:   return sizeof(int);
    case kUIntDsvFl:  return sizeof(unsigned int);
    case kFloatDsvFl: return sizeof(float);
    case kDoubleDsvFl:return sizeof(double);
    case kSampleDsvFl:return sizeof(cmSample_t);
    case kRealDsvFl:  return sizeof(cmReal_t);
    case kStrzDsvFl:  return sizeof(cmChar_t*);
    case kJsonDsvFl:  return sizeof(cmJsonNode_t*);
    default:
      { assert(0); }

  }
  return 0;
}
bool cmDsvIsNull(      const cmDspValue_t* vp ){ vp=_vcptr(vp); return  vp->flags == kNullDsvFl; }
bool cmDsvIsBool(      const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kBoolDsvFl);               }
bool cmDsvIsChar(      const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kCharDsvFl);               }
bool cmDsvIsUChar(     const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kUCharDsvFl);              }
bool cmDsvIsShort(     const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kShortDsvFl);              }
bool cmDsvIsUShort(    const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kUShortDsvFl);             } 
bool cmDsvIsLong(      const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kLongDsvFl);               }
bool cmDsvIsULong(     const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kULongDsvFl);              }
bool cmDsvIsInt(       const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kIntDsvFl);                }
bool cmDsvIsUInt(      const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kUIntDsvFl);               } 
bool cmDsvIsFloat(     const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kFloatDsvFl);              }
bool cmDsvIsDouble(    const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kDoubleDsvFl);             }
bool cmDsvIsSample(    const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kSampleDsvFl);             }
bool cmDsvIsReal(      const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kRealDsvFl);               }
bool cmDsvIsPtr(       const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kPtrDsvFl);                }
bool cmDsvIsStrz(      const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kStrzDsvFl);               }
bool cmDsvIsStrcz(     const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kStrzDsvFl | kConstDsvFl); }
bool cmDsvIsSymbol(    const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kSymDsvFl);                }
bool cmDsvIsJson(      const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kJsonDsvFl);               }

bool cmDsvIsBoolMtx(   const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kBoolDsvFl   | kMtxDsvFl); }
bool cmDsvIsCharMtx(   const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kCharDsvFl   | kMtxDsvFl); }
bool cmDsvIsUCharMtx(  const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kUCharDsvFl  | kMtxDsvFl); }
bool cmDsvIsShortMtx(  const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kShortDsvFl  | kMtxDsvFl); }
bool cmDsvIsUShortMtx( const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kUShortDsvFl | kMtxDsvFl); } 
bool cmDsvIsLongMtx(   const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kLongDsvFl   | kMtxDsvFl); }
bool cmDsvIsULongMtx(  const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kULongDsvFl  | kMtxDsvFl); }
bool cmDsvIsIntMtx(    const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kIntDsvFl    | kMtxDsvFl); }
bool cmDsvIsUIntMtx(   const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kUIntDsvFl   | kMtxDsvFl); } 
bool cmDsvIsFloatMtx(  const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kFloatDsvFl  | kMtxDsvFl); }
bool cmDsvIsDoubleMtx( const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kDoubleDsvFl | kMtxDsvFl); }
bool cmDsvIsSampleMtx( const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kSampleDsvFl | kMtxDsvFl); }
bool cmDsvIsRealMtx(   const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kRealDsvFl   | kMtxDsvFl); }
bool cmDsvIsJsonMtx(   const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kJsonDsvFl   | kMtxDsvFl); }
bool cmDsvIsStrzMtx(   const cmDspValue_t* vp ){ vp=_vcptr(vp); return  cmAllFlags(vp->flags,kStrzDsvFl   | kMtxDsvFl); }

bool cmDsvSetFromValist( cmDspValue_t* vp, unsigned flags, va_list vl )
{
  vp = _vptr(vp);

  switch(flags)
  {
    case kBoolDsvFl:   cmDsvSetBool(   vp, va_arg(vl,int           )); break;
    case kCharDsvFl:   cmDsvSetChar(   vp, va_arg(vl,int           )); break;
    case kUCharDsvFl:  cmDsvSetUChar(  vp, va_arg(vl,int           )); break;
    case kShortDsvFl:  cmDsvSetShort(  vp, va_arg(vl,int           )); break;
    case kUShortDsvFl: cmDsvSetUShort( vp, va_arg(vl,int           )); break;
    case kLongDsvFl:   cmDsvSetLong(   vp, va_arg(vl,long          )); break;
    case kULongDsvFl:  cmDsvSetULong(  vp, va_arg(vl,unsigned long )); break;
    case kIntDsvFl:    cmDsvSetInt(    vp, va_arg(vl,int           )); break;
    case kUIntDsvFl:   cmDsvSetUInt(   vp, va_arg(vl,unsigned      )); break;
    case kFloatDsvFl:  cmDsvSetFloat(  vp, va_arg(vl,double        )); break;
    case kDoubleDsvFl: cmDsvSetDouble( vp, va_arg(vl,double        )); break;
    case kSampleDsvFl: cmDsvSetSample( vp, va_arg(vl,double        )); break;
    case kRealDsvFl:   cmDsvSetReal(   vp, va_arg(vl,double        )); break;
    case kPtrDsvFl:    cmDsvSetPtr(    vp, va_arg(vl,void*         )); break;
    case kStrzDsvFl:   cmDsvSetStrz(   vp, va_arg(vl,cmChar_t*     )); break;
    default:
      return false;
  }
  return true;
}

void cmDsvSetNull(   cmDspValue_t* vp )                   { vp->flags = kNullDsvFl; }
void cmDsvSetBool(   cmDspValue_t* vp, bool v )           { vp->flags = kBoolDsvFl;   vp->u.b = v;  }
void cmDsvSetChar(   cmDspValue_t* vp, char v )           { vp->flags = kCharDsvFl;   vp->u.c = v;  }
void cmDsvSetUChar(  cmDspValue_t* vp, unsigned char v )  { vp->flags = kUCharDsvFl;  vp->u.uc = v; }
void cmDsvSetShort(  cmDspValue_t* vp, short v )          { vp->flags = kShortDsvFl;  vp->u.s = v;  }
void cmDsvSetUShort( cmDspValue_t* vp, unsigned short v ) { vp->flags = kUShortDsvFl; vp->u.us = v; }
void cmDsvSetLong(   cmDspValue_t* vp, long v )           { vp->flags = kLongDsvFl;   vp->u.l = v;  }
void cmDsvSetULong(  cmDspValue_t* vp, unsigned long v )  { vp->flags = kULongDsvFl;  vp->u.ul = v; }
void cmDsvSetInt(    cmDspValue_t* vp, int v )            { vp->flags = kIntDsvFl;    vp->u.i = v;  }  
void cmDsvSetUInt(   cmDspValue_t* vp, unsigned int v )   { vp->flags = kUIntDsvFl;   vp->u.u = v;  }
void cmDsvSetFloat(  cmDspValue_t* vp, float v )          { vp->flags = kFloatDsvFl;  vp->u.f = v;  }
void cmDsvSetDouble( cmDspValue_t* vp, double v )         { vp->flags = kDoubleDsvFl; vp->u.d = v;  }
void cmDsvSetSample( cmDspValue_t* vp, cmSample_t v )     { vp->flags = kSampleDsvFl; vp->u.a = v;  }
void cmDsvSetReal(   cmDspValue_t* vp, cmReal_t v )       { vp->flags = kRealDsvFl;   vp->u.r = v;  }
void cmDsvSetPtr(    cmDspValue_t* vp, void* v )          { vp->flags = kPtrDsvFl;    vp->u.vp = v;  }
void cmDsvSetSymbol( cmDspValue_t* vp, unsigned v )       { vp->flags = kSymDsvFl;    vp->u.u = v;  }  
void cmDsvSetStrz(   cmDspValue_t* vp, cmChar_t* v )      { vp->flags = kStrzDsvFl;   vp->u.z = v;  }  
void cmDsvSetStrcz(  cmDspValue_t* vp, const cmChar_t* v ){ vp->flags = kStrzDsvFl | kConstDsvFl; vp->u.cz = v; }
void cmDsvSetJson(   cmDspValue_t* vp, cmJsonNode_t* v)   { vp->flags = kJsonDsvFl;   vp->u.j = v;  }

void cmDsvSetBoolMtx(   cmDspValue_t* vp, bool* v,           unsigned rn, unsigned cn ) {   vp->flags = kBoolDsvFl   | kMtxDsvFl; vp->u.m.u.bp=v;  vp->u.m.rn=rn; vp->u.m.cn=cn; }
void cmDsvSetCharMtx(   cmDspValue_t* vp, char* v,           unsigned rn, unsigned cn ) {   vp->flags = kCharDsvFl   | kMtxDsvFl; vp->u.m.u.cp=v;  vp->u.m.rn=rn; vp->u.m.cn=cn; }
void cmDsvSetUCharMtx(  cmDspValue_t* vp, unsigned char* v,  unsigned rn, unsigned cn ) {   vp->flags = kUCharDsvFl  | kMtxDsvFl; vp->u.m.u.ucp=v; vp->u.m.rn=rn; vp->u.m.cn=cn; }
void cmDsvSetShortMtx(  cmDspValue_t* vp, short* v,          unsigned rn, unsigned cn ) {   vp->flags = kShortDsvFl  | kMtxDsvFl; vp->u.m.u.sp=v;  vp->u.m.rn=rn; vp->u.m.cn=cn; }
void cmDsvSetUShortMtx( cmDspValue_t* vp, unsigned short* v, unsigned rn, unsigned cn ) {   vp->flags = kUShortDsvFl | kMtxDsvFl; vp->u.m.u.usp=v; vp->u.m.rn=rn; vp->u.m.cn=cn; }
void cmDsvSetLongMtx(   cmDspValue_t* vp, long* v,           unsigned rn, unsigned cn ) {   vp->flags = kLongDsvFl   | kMtxDsvFl; vp->u.m.u.lp=v;  vp->u.m.rn=rn; vp->u.m.cn=cn; }
void cmDsvSetULongMtx(  cmDspValue_t* vp, unsigned long* v,  unsigned rn, unsigned cn ) {   vp->flags = kULongDsvFl  | kMtxDsvFl; vp->u.m.u.ulp=v; vp->u.m.rn=rn; vp->u.m.cn=cn; }
void cmDsvSetIntMtx(    cmDspValue_t* vp, int* v,            unsigned rn, unsigned cn ) {   vp->flags = kIntDsvFl    | kMtxDsvFl; vp->u.m.u.ip=v;  vp->u.m.rn=rn; vp->u.m.cn=cn; }  
void cmDsvSetUIntMtx(   cmDspValue_t* vp, unsigned int* v,   unsigned rn, unsigned cn ) {   vp->flags = kUIntDsvFl   | kMtxDsvFl; vp->u.m.u.up=v;  vp->u.m.rn=rn; vp->u.m.cn=cn; }
void cmDsvSetFloatMtx(  cmDspValue_t* vp, float* v,          unsigned rn, unsigned cn ) {   vp->flags = kFloatDsvFl  | kMtxDsvFl; vp->u.m.u.fp=v;  vp->u.m.rn=rn; vp->u.m.cn=cn; }
void cmDsvSetDoubleMtx( cmDspValue_t* vp, double* v,         unsigned rn, unsigned cn ) {   vp->flags = kDoubleDsvFl | kMtxDsvFl; vp->u.m.u.dp=v;  vp->u.m.rn=rn; vp->u.m.cn=cn; }
void cmDsvSetSampleMtx( cmDspValue_t* vp, cmSample_t* v,     unsigned rn, unsigned cn ) {   vp->flags = kSampleDsvFl | kMtxDsvFl; vp->u.m.u.ap=v;  vp->u.m.rn=rn; vp->u.m.cn=cn; }
void cmDsvSetRealMtx(   cmDspValue_t* vp, cmReal_t* v,       unsigned rn, unsigned cn ) {   vp->flags = kRealDsvFl   | kMtxDsvFl; vp->u.m.u.rp=v;  vp->u.m.rn=rn; vp->u.m.cn=cn; }
void cmDsvSetJsonMtx(   cmDspValue_t* vp, cmJsonNode_t** v,  unsigned rn, unsigned cn ) {   vp->flags = kJsonDsvFl   | kMtxDsvFl; vp->u.m.u.jp=v;  vp->u.m.rn=rn; vp->u.m.cn=cn; }
void cmDsvSetStrzMtx(   cmDspValue_t* vp, cmChar_t** v,      unsigned rn, unsigned cn ) {   vp->flags = kStrzDsvFl   | kMtxDsvFl; vp->u.m.u.zp=v;  vp->u.m.rn=rn; vp->u.m.cn=cn; }
void cmDsvSetStrczMtx(  cmDspValue_t* vp, const cmChar_t** v,unsigned rn, unsigned cn ) {   vp->flags = kStrzDsvFl   | kMtxDsvFl | kConstDsvFl; vp->u.m.u.czp=v;  vp->u.m.rn=rn; vp->u.m.cn=cn; }

void cmDsvSetMtx(       cmDspValue_t* vp, unsigned flags, void* v,   unsigned rn, unsigned cn ) 
{ 
  vp->flags=flags;              
  vp->u.m.u.vp=v;  
  vp->u.m.rn=rn; 
  vp->u.m.cn=cn; 
}

void cmDsvSetProxy(     cmDspValue_t* vp, cmDspValue_t* pp )
{ vp->flags = kProxyDsvFl; vp->u.vp = pp; }

bool cmDsvCanConvertFlags( unsigned destFlags, unsigned srcFlags )
{
  unsigned sFl = cmClrFlag(srcFlags  & kTypeDsvMask, kProxyDsvFl);
  unsigned dFl = cmClrFlag(destFlags & kTypeDsvMask, kProxyDsvFl );
  

  // if the flags are equal then no conversion is necessary
  if( sFl == dFl )
    return true;

  // if either the src or dst has no assigned type then conversion is always possible
  if( sFl == 0 || dFl == 0 )
    return true;

  // conversion is not possible between a mtx and a scalar or v.v.
  if( cmIsFlag(sFl,kMtxDsvFl) != cmIsFlag(dFl,kMtxDsvFl) )
    return false;

  // conversion between JSON nodes and anything else is not possible
  if( cmIsFlag(sFl,kJsonDsvFl) != cmIsFlag(dFl,kJsonDsvFl) )
    return false;

  // conversion between string and anything else is not possible.
  if( cmIsFlag(sFl,kStrzDsvFl) != cmIsFlag(dFl,kStrzDsvFl) )
    return false;

  // conversion betweens symbols and anything else is not possible
  if( cmIsFlag(sFl,kSymDsvFl) != cmIsFlag(dFl,kSymDsvFl) )
    return false;

  return true;

}

bool cmDsvCanConvert( const cmDspValue_t* dvp, const cmDspValue_t* svp )
{
  dvp = _vcptr(dvp);
  svp = _vcptr(svp);

  
  return cmDsvCanConvertFlags( _vcptr(dvp)->flags, _vcptr(svp)->flags );
}

bool           cmDsvBool(   const cmDspValue_t* vp ) { vp=_vcptr(vp); if(cmIsFlag((vp->flags & kTypeDsvMask),kBoolDsvFl))   return vp->u.b;  assert(0); return 0; }
char           cmDsvChar(   const cmDspValue_t* vp ) { vp=_vcptr(vp); if(cmIsFlag((vp->flags & kTypeDsvMask),kCharDsvFl))   return vp->u.c;  assert(0); return 0; }
unsigned char  cmDsvUChar(  const cmDspValue_t* vp ) { vp=_vcptr(vp); if(cmIsFlag((vp->flags & kTypeDsvMask),kUCharDsvFl))  return vp->u.uc; assert(0); return 0; }
short          cmDsvShort(  const cmDspValue_t* vp ) { vp=_vcptr(vp); if(cmIsFlag((vp->flags & kTypeDsvMask),kShortDsvFl))  return vp->u.s;  assert(0); return 0; }
unsigned short cmDsvUShort( const cmDspValue_t* vp ) { vp=_vcptr(vp); if(cmIsFlag((vp->flags & kTypeDsvMask),kUShortDsvFl)) return vp->u.us; assert(0); return 0; } 
long           cmDsvLong(   const cmDspValue_t* vp ) { vp=_vcptr(vp); if(cmIsFlag((vp->flags & kTypeDsvMask),kLongDsvFl))   return vp->u.l;  assert(0); return 0; }
unsigned long  cmDsvULong(  const cmDspValue_t* vp ) { vp=_vcptr(vp); if(cmIsFlag((vp->flags & kTypeDsvMask),kULongDsvFl))  return vp->u.ul; assert(0); return 0; }
int            cmDsvInt(    const cmDspValue_t* vp ) { vp=_vcptr(vp); if(cmIsFlag((vp->flags & kTypeDsvMask),kIntDsvFl))    return vp->u.i;  assert(0); return 0; }
unsigned int   cmDsvUInt(   const cmDspValue_t* vp ) { vp=_vcptr(vp); if(cmIsFlag((vp->flags & kTypeDsvMask),kUIntDsvFl))   return vp->u.u;  assert(0); return 0; } 
float          cmDsvFloat(  const cmDspValue_t* vp ) { vp=_vcptr(vp); if(cmIsFlag((vp->flags & kTypeDsvMask),kFloatDsvFl))  return vp->u.f;  assert(0); return 0; }
double         cmDsvDouble( const cmDspValue_t* vp ) { vp=_vcptr(vp); if(cmIsFlag((vp->flags & kTypeDsvMask),kDoubleDsvFl)) return vp->u.d;  assert(0); return 0; }
cmSample_t     cmDsvSample( const cmDspValue_t* vp ) { vp=_vcptr(vp); if(cmIsFlag((vp->flags & kTypeDsvMask),kSampleDsvFl)) return vp->u.a;  assert(0); return 0; }
cmReal_t       cmDsvReal(   const cmDspValue_t* vp ) { vp=_vcptr(vp); if(cmIsFlag((vp->flags & kTypeDsvMask),kRealDsvFl))   return vp->u.r;  assert(0); return 0; } 
void*          cmDsvPtr(    const cmDspValue_t* vp ) { vp=_vcptr(vp); if(cmIsFlag((vp->flags & kTypeDsvMask),kPtrDsvFl))    return vp->u.vp; assert(0); return 0; } 
unsigned int   cmDsvSymbol( const cmDspValue_t* vp ) { vp=_vcptr(vp); if(cmIsFlag((vp->flags & kTypeDsvMask),kSymDsvFl))    return vp->u.u;  assert(0); return cmInvalidId; } 
cmJsonNode_t*  cmDsvJson(   const cmDspValue_t* vp ) { vp=_vcptr(vp); if(cmIsFlag((vp->flags & kTypeDsvMask),kJsonDsvFl))   return vp->u.j;  assert(0); return NULL; }

const cmDspValue_t*  cmDsvValueCPtr(    const cmDspValue_t* vp )
{ return _vcptr(vp); }

cmDspValue_t*  cmDsvValuePtr( cmDspValue_t* vp )
{ return _vptr(vp); }

cmChar_t*      cmDsvStrz(   const cmDspValue_t* vp ) 
{ 
  vp=_vcptr(vp);

  if(cmIsFlag((vp->flags & kTypeDsvMask),kStrzDsvFl) && cmIsFlag(vp->flags,kConstDsvFl)==false )   
    return vp->u.z;  

  assert(0); 
  return NULL; 
}

const cmChar_t*cmDsvStrcz(  const cmDspValue_t* vp ) 
{ 
  vp = _vcptr(vp);

  if(cmIsFlag((vp->flags & kTypeDsvMask),kStrzDsvFl))   
    return vp->u.z;
  assert(0);
  return NULL;
}


const bool*           cmDsvBoolCMtx(   const cmDspValue_t* vp ) { vp=_vcptr(vp); if(cmAllFlags((vp->flags & kTypeDsvMask), kMtxDsvFl | kBoolDsvFl))   return vp->u.m.u.bp;  assert(0); return NULL; }
const char*           cmDsvCharCMtx(   const cmDspValue_t* vp ) { vp=_vcptr(vp); if(cmAllFlags((vp->flags & kTypeDsvMask), kMtxDsvFl | kCharDsvFl))   return vp->u.m.u.cp;  assert(0); return NULL; }
const unsigned char*  cmDsvUCharCMtx(  const cmDspValue_t* vp ) { vp=_vcptr(vp); if(cmAllFlags((vp->flags & kTypeDsvMask), kMtxDsvFl | kUCharDsvFl))  return vp->u.m.u.ucp; assert(0); return NULL; }
const short*          cmDsvShortCMtx(  const cmDspValue_t* vp ) { vp=_vcptr(vp); if(cmAllFlags((vp->flags & kTypeDsvMask), kMtxDsvFl | kShortDsvFl))  return vp->u.m.u.sp;  assert(0); return NULL; }
const unsigned short* cmDsvUShortCMtx( const cmDspValue_t* vp ) { vp=_vcptr(vp); if(cmAllFlags((vp->flags & kTypeDsvMask), kMtxDsvFl | kUShortDsvFl)) return vp->u.m.u.usp; assert(0); return NULL; } 
const long*           cmDsvLongCMtx(   const cmDspValue_t* vp ) { vp=_vcptr(vp); if(cmAllFlags((vp->flags & kTypeDsvMask), kMtxDsvFl | kLongDsvFl))   return vp->u.m.u.lp;  assert(0); return NULL; }
const unsigned long*  cmDsvULongCMtx(  const cmDspValue_t* vp ) { vp=_vcptr(vp); if(cmAllFlags((vp->flags & kTypeDsvMask), kMtxDsvFl | kULongDsvFl))  return vp->u.m.u.ulp; assert(0); return NULL; }
const int*            cmDsvIntCMtx(    const cmDspValue_t* vp ) { vp=_vcptr(vp); if(cmAllFlags((vp->flags & kTypeDsvMask), kMtxDsvFl | kIntDsvFl))    return vp->u.m.u.ip;  assert(0); return NULL; }
const unsigned int*   cmDsvUIntCMtx(   const cmDspValue_t* vp ) { vp=_vcptr(vp); if(cmAllFlags((vp->flags & kTypeDsvMask), kMtxDsvFl | kUIntDsvFl))   return vp->u.m.u.up;  assert(0); return NULL; } 
const float*          cmDsvFloatCMtx(  const cmDspValue_t* vp ) { vp=_vcptr(vp); if(cmAllFlags((vp->flags & kTypeDsvMask), kMtxDsvFl | kFloatDsvFl))  return vp->u.m.u.fp;  assert(0); return NULL; }
const double*         cmDsvDoubleCMtx( const cmDspValue_t* vp ) { vp=_vcptr(vp); if(cmAllFlags((vp->flags & kTypeDsvMask), kMtxDsvFl | kDoubleDsvFl)) return vp->u.m.u.dp;  assert(0); return NULL; }
const cmSample_t*     cmDsvSampleCMtx( const cmDspValue_t* vp ) { vp=_vcptr(vp); if(cmAllFlags((vp->flags & kTypeDsvMask), kMtxDsvFl | kSampleDsvFl)) return vp->u.m.u.ap;  assert(0); return NULL; }
const cmReal_t*       cmDsvRealCMtx(   const cmDspValue_t* vp ) { vp=_vcptr(vp); if(cmAllFlags((vp->flags & kTypeDsvMask), kMtxDsvFl | kRealDsvFl))   return vp->u.m.u.rp;  assert(0); return NULL; } 

const cmChar_t**      cmDsvStrzCMtx(   const cmDspValue_t* vp ) 
{ 
  vp = _vcptr(vp);

  if(cmAllFlags((vp->flags & kTypeDsvMask), kMtxDsvFl | kStrzDsvFl) )   
    return vp->u.m.u.czp;  

  assert(0); 
  return NULL; 
}

const cmChar_t**   cmDsvStrczCMtx(   const cmDspValue_t* vp ) 
{ 
  vp = _vcptr(vp);

  if(cmAllFlags((vp->flags & kTypeDsvMask), kMtxDsvFl | kStrzDsvFl))   
    return vp->u.m.u.czp;  

  assert(0); 
  return NULL; 
}


bool*           cmDsvBoolMtx(   cmDspValue_t* vp ) { vp=_vptr(vp); if(cmAllFlags((vp->flags & kTypeDsvMask), kMtxDsvFl | kBoolDsvFl))   return vp->u.m.u.bp;  assert(0); return NULL; }
char*           cmDsvCharMtx(   cmDspValue_t* vp ) { vp=_vptr(vp); if(cmAllFlags((vp->flags & kTypeDsvMask), kMtxDsvFl | kCharDsvFl))   return vp->u.m.u.cp;  assert(0); return NULL; }
unsigned char*  cmDsvUCharMtx(  cmDspValue_t* vp ) { vp=_vptr(vp); if(cmAllFlags((vp->flags & kTypeDsvMask), kMtxDsvFl | kUCharDsvFl))  return vp->u.m.u.ucp; assert(0); return NULL; }
short*          cmDsvShortMtx(  cmDspValue_t* vp ) { vp=_vptr(vp); if(cmAllFlags((vp->flags & kTypeDsvMask), kMtxDsvFl | kShortDsvFl))  return vp->u.m.u.sp;  assert(0); return NULL; }
unsigned short* cmDsvUShortMtx( cmDspValue_t* vp ) { vp=_vptr(vp); if(cmAllFlags((vp->flags & kTypeDsvMask), kMtxDsvFl | kUShortDsvFl)) return vp->u.m.u.usp; assert(0); return NULL; } 
long*           cmDsvLongMtx(   cmDspValue_t* vp ) { vp=_vptr(vp); if(cmAllFlags((vp->flags & kTypeDsvMask), kMtxDsvFl | kLongDsvFl))   return vp->u.m.u.lp;  assert(0); return NULL; }
unsigned long*  cmDsvULongMtx(  cmDspValue_t* vp ) { vp=_vptr(vp); if(cmAllFlags((vp->flags & kTypeDsvMask), kMtxDsvFl | kULongDsvFl))  return vp->u.m.u.ulp; assert(0); return NULL; }
int*            cmDsvIntMtx(    cmDspValue_t* vp ) { vp=_vptr(vp); if(cmAllFlags((vp->flags & kTypeDsvMask), kMtxDsvFl | kIntDsvFl))    return vp->u.m.u.ip;  assert(0); return NULL; }
unsigned int*   cmDsvUIntMtx(   cmDspValue_t* vp ) { vp=_vptr(vp); if(cmAllFlags((vp->flags & kTypeDsvMask), kMtxDsvFl | kUIntDsvFl))   return vp->u.m.u.up;  assert(0); return NULL; } 
float*          cmDsvFloatMtx(  cmDspValue_t* vp ) { vp=_vptr(vp); if(cmAllFlags((vp->flags & kTypeDsvMask), kMtxDsvFl | kFloatDsvFl))  return vp->u.m.u.fp;  assert(0); return NULL; }
double*         cmDsvDoubleMtx( cmDspValue_t* vp ) { vp=_vptr(vp); if(cmAllFlags((vp->flags & kTypeDsvMask), kMtxDsvFl | kDoubleDsvFl)) return vp->u.m.u.dp;  assert(0); return NULL; }
cmSample_t*     cmDsvSampleMtx( cmDspValue_t* vp ) { vp=_vptr(vp); if(cmAllFlags((vp->flags & kTypeDsvMask), kMtxDsvFl | kSampleDsvFl)) return vp->u.m.u.ap;  assert(0); return NULL; }
cmReal_t*       cmDsvRealMtx(   cmDspValue_t* vp ) { vp=_vptr(vp); if(cmAllFlags((vp->flags & kTypeDsvMask), kMtxDsvFl | kRealDsvFl))   return vp->u.m.u.rp;  assert(0); return NULL; } 
cmJsonNode_t**  cmDsvJsonMtx(   cmDspValue_t* vp ) { vp=_vptr(vp); if(cmAllFlags((vp->flags & kTypeDsvMask), kMtxDsvFl | kJsonDsvFl))   return vp->u.m.u.jp;  assert(0); return NULL; }
cmChar_t**      cmDsvStrzMtx(   cmDspValue_t* vp ) 
{ 
  vp = _vptr(vp);

  if(cmAllFlags((vp->flags & kTypeDsvMask), kMtxDsvFl | kStrzDsvFl) && cmIsFlag(vp->flags,kConstDsvFl))   
    return vp->u.m.u.zp;  

  assert(0); 
  return NULL; 
}

const cmChar_t**      cmDsvStrczMtx(   cmDspValue_t* vp ) 
{  return cmDsvStrczCMtx(vp); }

// Get the value of a cmDspValue_t as a <type>.  
// Conversion is performed if the return type does not exactly match the cmDspValue_t type.
bool           cmDsvGetBool(   const cmDspValue_t* vp )
{
  vp = _vcptr(vp);

  switch((vp->flags & kTypeDsvMask))
  {
    case kNullDsvFl:   break;
    case kBoolDsvFl:   return vp->u.b;
    case kCharDsvFl:   return (bool)vp->u.c;
    case kUCharDsvFl:  return (bool)vp->u.uc;
    case kShortDsvFl:  return (bool)vp->u.s;
    case kUShortDsvFl: return (bool)vp->u.us;
    case kLongDsvFl:   return (bool)vp->u.l;
    case kULongDsvFl:  return (bool)vp->u.ul;
    case kIntDsvFl:    return (bool)vp->u.i;
    case kUIntDsvFl:   return (bool)vp->u.u;
    case kFloatDsvFl:  return (bool)vp->u.f;
    case kDoubleDsvFl: return (bool)vp->u.d;
    case kSampleDsvFl: return (bool)vp->u.a;
    case kRealDsvFl:   return (bool)vp->u.r;
    default:
      assert(0);
  }
  return 0;
}

char           cmDsvGetChar(   const cmDspValue_t* vp )
{
  vp = _vcptr(vp);

  switch((vp->flags & kTypeDsvMask))
  {
    case kNullDsvFl:   break;
    case kBoolDsvFl:   return (char)vp->u.b;
    case kCharDsvFl:   return vp->u.c;
    case kUCharDsvFl:  return (char)vp->u.uc;
    case kShortDsvFl:  return (char)vp->u.s;
    case kUShortDsvFl: return (char)vp->u.us;
    case kLongDsvFl:   return (char)vp->u.l;
    case kULongDsvFl:  return (char)vp->u.ul;
    case kIntDsvFl:    return (char)vp->u.i;
    case kUIntDsvFl:   return (char)vp->u.u;
    case kFloatDsvFl:  return (char)vp->u.f;
    case kDoubleDsvFl: return (char)vp->u.d;
    case kSampleDsvFl: return (char)vp->u.a;
    case kRealDsvFl:   return (char)vp->u.r;
    default:
      assert(0);
  }
  return 0;
}

unsigned char  cmDsvGetUChar(  const cmDspValue_t* vp ) 
{
  vp = _vcptr(vp);

  switch((vp->flags & kTypeDsvMask))
  {
    case kNullDsvFl:   break;
    case kBoolDsvFl:   return (char)vp->u.b;
    case kCharDsvFl:   return (unsigned char)vp->u.c;
    case kUCharDsvFl:  return vp->u.uc;
    case kShortDsvFl:  return (unsigned char)vp->u.s;
    case kUShortDsvFl: return (unsigned char)vp->u.us;
    case kLongDsvFl:   return (unsigned char)vp->u.l;
    case kULongDsvFl:  return (unsigned char)vp->u.ul;
    case kIntDsvFl:    return (unsigned char)vp->u.i;
    case kUIntDsvFl:   return (unsigned char)vp->u.u;
    case kFloatDsvFl:  return (unsigned char)vp->u.f;
    case kDoubleDsvFl: return (unsigned char)vp->u.d;
    case kSampleDsvFl: return (unsigned char)vp->u.a;
    case kRealDsvFl:   return (unsigned char)vp->u.r;
    default:
      assert(0);
  }
  return 0;
}
short          cmDsvGetShort(  const cmDspValue_t* vp ) 
{
  vp = _vcptr(vp);

  switch((vp->flags & kTypeDsvMask))
  {
    case kNullDsvFl:   break;
    case kBoolDsvFl:   return (char)vp->u.b;
    case kCharDsvFl:   return (short)vp->u.c;
    case kUCharDsvFl:  return (short)vp->u.uc;
    case kShortDsvFl:  return vp->u.s;
    case kUShortDsvFl: return (short)vp->u.us;
    case kLongDsvFl:   return (short)vp->u.l;
    case kULongDsvFl:  return (short)vp->u.ul;
    case kIntDsvFl:    return (short)vp->u.i;
    case kUIntDsvFl:   return (short)vp->u.u;
    case kFloatDsvFl:  return (short)vp->u.f;
    case kDoubleDsvFl: return (short)vp->u.d;
    case kSampleDsvFl: return (short)vp->u.a;
    case kRealDsvFl:   return (short)vp->u.r;
    default:
      assert(0);
  }
  return 0;
}
unsigned short cmDsvGetUShort( const cmDspValue_t* vp ) 
{
  vp = _vcptr(vp);

  switch((vp->flags & kTypeDsvMask))
  {
    case kNullDsvFl:   break;
    case kBoolDsvFl:   return (char)vp->u.b;
    case kCharDsvFl:   return (unsigned short)vp->u.c;
    case kUCharDsvFl:  return (unsigned short)vp->u.uc;
    case kShortDsvFl:  return (unsigned short)vp->u.s;
    case kUShortDsvFl: return  vp->u.us;
    case kLongDsvFl:   return (unsigned short)vp->u.l;
    case kULongDsvFl:  return (unsigned short)vp->u.ul;
    case kIntDsvFl:    return (unsigned short)vp->u.i;
    case kUIntDsvFl:   return (unsigned short)vp->u.u;
    case kFloatDsvFl:  return (unsigned short)vp->u.f;
    case kDoubleDsvFl: return (unsigned short)vp->u.d;
    case kSampleDsvFl: return (unsigned short)vp->u.a;
    case kRealDsvFl:   return (unsigned short)vp->u.r;
    default:
      assert(0);
  }
  return 0;
} 
long           cmDsvGetLong(   const cmDspValue_t* vp ) 
{
  vp = _vcptr(vp);

  switch((vp->flags & kTypeDsvMask))
  {
    case kNullDsvFl:   break;
    case kBoolDsvFl:   return (char)vp->u.b;
    case kCharDsvFl:   return (long)vp->u.c;
    case kUCharDsvFl:  return (long)vp->u.uc;
    case kShortDsvFl:  return (long)vp->u.s;
    case kUShortDsvFl: return (long)vp->u.us;
    case kLongDsvFl:   return vp->u.l;
    case kULongDsvFl:  return (long)vp->u.ul;
    case kIntDsvFl:    return (long)vp->u.i;
    case kUIntDsvFl:   return (long)vp->u.u;
    case kFloatDsvFl:  return (long)vp->u.f;
    case kDoubleDsvFl: return (long)vp->u.d;
    case kSampleDsvFl: return (long)vp->u.a;
    case kRealDsvFl:   return (long)vp->u.r;
    default:
      assert(0);
  }
  return 0;
}
unsigned long  cmDsvGetULong(  const cmDspValue_t* vp ) 
{
  vp = _vcptr(vp);

  switch((vp->flags & kTypeDsvMask))
  {
    case kNullDsvFl:   break;
    case kBoolDsvFl:   return (char)vp->u.b;
    case kCharDsvFl:   return (unsigned long)vp->u.c;
    case kUCharDsvFl:  return (unsigned long)vp->u.uc;
    case kShortDsvFl:  return (unsigned long)vp->u.s;
    case kUShortDsvFl: return (unsigned long)vp->u.us;
    case kLongDsvFl:   return (unsigned long)vp->u.l;
    case kULongDsvFl:  return vp->u.ul;
    case kIntDsvFl:    return (unsigned long)vp->u.i;
    case kUIntDsvFl:   return (unsigned long)vp->u.u;
    case kFloatDsvFl:  return (unsigned long)vp->u.f;
    case kDoubleDsvFl: return (unsigned long)vp->u.d;
    case kSampleDsvFl: return (unsigned long)vp->u.a;
    case kRealDsvFl:   return (unsigned long)vp->u.r;
    default:
      assert(0);
  }
  return 0;
}
int            cmDsvGetInt(    const cmDspValue_t* vp ) 
{
  vp = _vcptr(vp);

  switch((vp->flags & kTypeDsvMask))
  {
    case kNullDsvFl:   break;
    case kBoolDsvFl:   return (char)vp->u.b;
    case kCharDsvFl:   return (int)vp->u.c;
    case kUCharDsvFl:  return (int)vp->u.uc;
    case kShortDsvFl:  return (int)vp->u.s;
    case kUShortDsvFl: return (int)vp->u.us;
    case kLongDsvFl:   return (int)vp->u.l;
    case kULongDsvFl:  return (int)vp->u.ul;
    case kIntDsvFl:    return vp->u.i;
    case kUIntDsvFl:   return (int)vp->u.u;
    case kFloatDsvFl:  return (int)vp->u.f;
    case kDoubleDsvFl: return (int)vp->u.d;
    case kSampleDsvFl: return (int)vp->u.a;
    case kRealDsvFl:   return (int)vp->u.r;
    default:
      assert(0);
  }
  return 0;
}
unsigned int   cmDsvGetUInt(   const cmDspValue_t* vp ) 
{
  vp = _vcptr(vp);

  switch((vp->flags & kTypeDsvMask))
  {
    case kNullDsvFl:   break;
    case kBoolDsvFl:   return (char)vp->u.b;
    case kCharDsvFl:   return (unsigned int)vp->u.c;
    case kUCharDsvFl:  return (unsigned int)vp->u.uc;
    case kShortDsvFl:  return (unsigned int)vp->u.s;
    case kUShortDsvFl: return (unsigned int)vp->u.us;
    case kLongDsvFl:   return (unsigned int)vp->u.l;
    case kULongDsvFl:  return (unsigned int)vp->u.ul;
    case kIntDsvFl:    return (unsigned int)vp->u.i;
    case kUIntDsvFl:   return vp->u.u;
    case kFloatDsvFl:  return (unsigned int)vp->u.f;
    case kDoubleDsvFl: return (unsigned int)vp->u.d;
    case kSampleDsvFl: return (unsigned int)vp->u.a;
    case kRealDsvFl:   return (unsigned int)vp->u.r;
    default:
      assert(0);
  }
  return 0;
} 
float          cmDsvGetFloat(  const cmDspValue_t* vp ) 
{
  vp = _vcptr(vp);

  switch((vp->flags & kTypeDsvMask))
  {
    case kNullDsvFl:   break;
    case kBoolDsvFl:   return (char)vp->u.b;
    case kCharDsvFl:   return (float)vp->u.c;
    case kUCharDsvFl:  return (float)vp->u.uc;
    case kShortDsvFl:  return (float)vp->u.s;
    case kUShortDsvFl: return (float)vp->u.us;
    case kLongDsvFl:   return (float)vp->u.l;
    case kULongDsvFl:  return (float)vp->u.ul;
    case kIntDsvFl:    return (float)vp->u.i;
    case kUIntDsvFl:   return (float)vp->u.u;
    case kFloatDsvFl:  return vp->u.f;
    case kDoubleDsvFl: return (float)vp->u.d;
    case kSampleDsvFl: return (float)vp->u.a;
    case kRealDsvFl:   return (float)vp->u.r;
    default:
      assert(0);
  }
  return 0;
}
double         cmDsvGetDouble( const cmDspValue_t* vp ) 
{
  vp = _vcptr(vp);

  switch((vp->flags & kTypeDsvMask))
  {
    case kNullDsvFl:   break;
    case kBoolDsvFl:   return (char)vp->u.b;
    case kCharDsvFl:   return (double)vp->u.c;
    case kUCharDsvFl:  return (double)vp->u.uc;
    case kShortDsvFl:  return (double)vp->u.s;
    case kUShortDsvFl: return (double)vp->u.us;
    case kLongDsvFl:   return (double)vp->u.l;
    case kULongDsvFl:  return (double)vp->u.ul;
    case kIntDsvFl:    return (double)vp->u.i;
    case kUIntDsvFl:   return (double)vp->u.u;
    case kFloatDsvFl:  return (double)vp->u.f;
    case kDoubleDsvFl: return vp->u.d;
    case kSampleDsvFl: return (double)vp->u.a;
    case kRealDsvFl:   return (double)vp->u.r;
    default:
      assert(0);
  }
  return 0;
}
cmSample_t     cmDsvGetSample( const cmDspValue_t* vp ) 
{
  vp = _vcptr(vp);

  switch((vp->flags & kTypeDsvMask))
  {
    case kNullDsvFl:   break;
    case kBoolDsvFl:   return (char)vp->u.b;
    case kCharDsvFl:   return (cmSample_t)vp->u.c;
    case kUCharDsvFl:  return (cmSample_t)vp->u.uc;
    case kShortDsvFl:  return (cmSample_t)vp->u.s;
    case kUShortDsvFl: return (cmSample_t)vp->u.us;
    case kLongDsvFl:   return (cmSample_t)vp->u.l;
    case kULongDsvFl:  return (cmSample_t)vp->u.ul;
    case kIntDsvFl:    return (cmSample_t)vp->u.i;
    case kUIntDsvFl:   return (cmSample_t)vp->u.u;
    case kFloatDsvFl:  return (cmSample_t)vp->u.f;
    case kDoubleDsvFl: return (cmSample_t)vp->u.d;
    case kSampleDsvFl: return vp->u.a;
    case kRealDsvFl:   return (cmSample_t)vp->u.r;
    default:
      assert(0);
  }
  return 0;
}
cmReal_t       cmDsvGetReal(   const cmDspValue_t* vp  )
{
  vp = _vcptr(vp);

  switch((vp->flags & kTypeDsvMask))
  {
    case kNullDsvFl:   break;
    case kBoolDsvFl:   return (char)vp->u.b;
    case kCharDsvFl:   return (cmReal_t)vp->u.c;
    case kUCharDsvFl:  return (cmReal_t)vp->u.uc;
    case kShortDsvFl:  return (cmReal_t)vp->u.s;
    case kUShortDsvFl: return (cmReal_t)vp->u.us;
    case kLongDsvFl:   return (cmReal_t)vp->u.l;
    case kULongDsvFl:  return (cmReal_t)vp->u.ul;
    case kIntDsvFl:    return (cmReal_t)vp->u.i;
    case kUIntDsvFl:   return (cmReal_t)vp->u.u;
    case kFloatDsvFl:  return (cmReal_t)vp->u.f;
    case kDoubleDsvFl: return (cmReal_t)vp->u.d;
    case kSampleDsvFl: return (cmReal_t)vp->u.a;
    case kRealDsvFl:   return vp->u.r;
    default:
      assert(0);
  }
  return 0;
} 

void*       cmDsvGetPtr(   const cmDspValue_t* vp  )
{
  vp = _vcptr(vp);


  if( (vp->flags & kTypeDsvMask) == kPtrDsvFl )
    return vp->u.vp;

  return NULL;
} 

unsigned int   cmDsvGetSymbol(   const cmDspValue_t* vp ) 
{
  vp = _vcptr(vp);

  if( (vp->flags & kTypeDsvMask) == kSymDsvFl )
    return vp->u.u;
  return cmInvalidId;
} 


cmChar_t* cmDsvGetStrz( const cmDspValue_t* vp )
{
  vp = _vcptr(vp);

  if( (vp->flags & kTypeDsvMask) == kStrzDsvFl && cmIsFlag(vp->flags,kConstDsvFl)==false )
    return vp->u.z;
  return NULL;
}

const cmChar_t* cmDsvGetStrcz( const cmDspValue_t* vp )
{
  vp = _vcptr(vp);

  if( (vp->flags & kTypeDsvMask) == kStrzDsvFl  )
    return vp->u.z;
  return NULL;
}

cmJsonNode_t* cmDsvGetJson( const cmDspValue_t* vp )
{
  vp = _vcptr(vp);

  if( (vp->flags & kTypeDsvMask) == kJsonDsvFl )
    return vp->u.j;
  return NULL;
}



#define cvt_b(  dt,vp,op,sn) do{bool*           sp=vp->u.m.u.bp;  bool*            ep=sp+sn; while(sp<ep) *op++=(dt)*sp++; }while(0)
#define cvt_c(  dt,vp,op,sn) do{char*           sp=vp->u.m.u.cp;  char*            ep=sp+sn; while(sp<ep) *op++=(dt)*sp++; }while(0)
#define cvt_uc( dt,vp,op,sn) do{unsigned char*  sp=vp->u.m.u.ucp; unsigned char*   ep=sp+sn; while(sp<ep) *op++=(dt)*sp++; }while(0)
#define cvt_s(  dt,vp,op,sn) do{short*          sp=vp->u.m.u.sp;  short*           ep=sp+sn; while(sp<ep) *op++=(dt)*sp++; }while(0)
#define cvt_us( dt,vp,op,sn) do{unsigned short* sp=vp->u.m.u.usp; unsigned short*  ep=sp+sn; while(sp<ep) *op++=(dt)*sp++; }while(0)
#define cvt_l(  dt,vp,op,sn) do{long*           sp=vp->u.m.u.lp;  long*            ep=sp+sn; while(sp<ep) *op++=(dt)*sp++; }while(0)
#define cvt_ul( dt,vp,op,sn) do{unsigned long*  sp=vp->u.m.u.ulp; unsigned long*   ep=sp+sn; while(sp<ep) *op++=(dt)*sp++; }while(0)
#define cvt_i(  dt,vp,op,sn) do{int*            sp=vp->u.m.u.ip;  int*             ep=sp+sn; while(sp<ep) *op++=(dt)*sp++; }while(0)
#define cvt_u(  dt,vp,op,sn) do{unsigned int*   sp=vp->u.m.u.up;  unsigned int*    ep=sp+sn; while(sp<ep) *op++=(dt)*sp++; }while(0)
#define cvt_f(  dt,vp,op,sn) do{float*          sp=vp->u.m.u.fp;  float*           ep=sp+sn; while(sp<ep) *op++=(dt)*sp++; }while(0)
#define cvt_d(  dt,vp,op,sn) do{double*         sp=vp->u.m.u.dp;  double*          ep=sp+sn; while(sp<ep) *op++=(dt)*sp++; }while(0)
#define cvt_a(  dt,vp,op,sn) do{cmSample_t*     sp=vp->u.m.u.ap;  cmSample_t*      ep=sp+sn; while(sp<ep) *op++=(dt)*sp++; }while(0)
#define cvt_r(  dt,vp,op,sn) do{cmReal_t*       sp=vp->u.m.u.rp;  cmReal_t*        ep=sp+sn; while(sp<ep) *op++=(dt)*sp++; }while(0)

cmDsvRC_t _cmDsvValidateMtx( const cmDspValue_t* vp, unsigned vn, unsigned* snp )
{
  if( cmIsFlag(vp->flags,kMtxDsvFl)==false)
  {
    assert(0);
    return kNotMtxDsvRC;
  }

  *snp = vp->u.m.rn * vp->u.m.cn;

  if( *snp > vn )
  {
    assert(0);
    return kRetTooSmallDsvRC;
  }

  return kOkDsvRC;

}

cmDsvRC_t cmDsvGetBoolMtx( cmDspValue_t* vp,  bool* v, unsigned vn )
{
  cmDsvRC_t rc;
  unsigned sn;

  vp = _vptr(vp);

  if((rc = _cmDsvValidateMtx(vp,vn,&sn)) != kOkDsvRC )
    return rc;

  switch(cmDsvBasicType(vp))
  {
    case kBoolDsvFl:   cvt_b(  bool,vp,v,sn); break;
    case kCharDsvFl:   cvt_c(  bool,vp,v,sn); break;
    case kUCharDsvFl:  cvt_uc( bool,vp,v,sn); break;
    case kShortDsvFl:  cvt_s(  bool,vp,v,sn); break;
    case kUShortDsvFl: cvt_us( bool,vp,v,sn); break;
    case kLongDsvFl:   cvt_l(  bool,vp,v,sn); break;
    case kULongDsvFl:  cvt_ul( bool,vp,v,sn); break;
    case kIntDsvFl:    cvt_i(  bool,vp,v,sn); break;
    case kUIntDsvFl:   cvt_u(  bool,vp,v,sn); break;
    case kFloatDsvFl:  cvt_f(  bool,vp,v,sn); break;
    case kDoubleDsvFl: cvt_d(  bool,vp,v,sn); break;
    case kSampleDsvFl: cvt_s(  bool,vp,v,sn); break;
    case kRealDsvFl:   cvt_r(  bool,vp,v,sn); break;
    default:
      assert(0);
      return kUnknownTypeDsvRC;
  }
  return kOkDsvRC;
}

cmDsvRC_t cmDsvGetCharMtx( cmDspValue_t* vp,  char* v, unsigned vn )
{
  vp = _vptr(vp);

  cmDsvRC_t rc;
  unsigned sn;
  if((rc = _cmDsvValidateMtx(vp,vn,&sn)) != kOkDsvRC )
    return rc;

  switch(cmDsvBasicType(vp))
  {
    case kBoolDsvFl:   cvt_b(  char,vp,v,sn); break;
    case kCharDsvFl:   cvt_c(  char,vp,v,sn); break;
    case kUCharDsvFl:  cvt_uc( char,vp,v,sn); break;
    case kShortDsvFl:  cvt_s(  char,vp,v,sn); break;
    case kUShortDsvFl: cvt_us( char,vp,v,sn); break;
    case kLongDsvFl:   cvt_l(  char,vp,v,sn); break;
    case kULongDsvFl:  cvt_ul( char,vp,v,sn); break;
    case kIntDsvFl:    cvt_i(  char,vp,v,sn); break;
    case kUIntDsvFl:   cvt_u(  char,vp,v,sn); break;
    case kFloatDsvFl:  cvt_f(  char,vp,v,sn); break;
    case kDoubleDsvFl: cvt_d(  char,vp,v,sn); break;
    case kSampleDsvFl: cvt_s(  char,vp,v,sn); break;
    case kRealDsvFl:   cvt_r(  char,vp,v,sn); break;
    default:
      assert(0);
      return kUnknownTypeDsvRC;
  }
  return kOkDsvRC;
}


cmDsvRC_t cmDsvGetUCharMtx(  cmDspValue_t* vp, unsigned char* v,  unsigned vn )
{
  vp = _vptr(vp);

  cmDsvRC_t rc;
  unsigned sn;
  if((rc = _cmDsvValidateMtx(vp,vn,&sn)) != kOkDsvRC )
    return rc;

  switch(cmDsvBasicType(vp))
  {
    case kBoolDsvFl:   cvt_b(  unsigned char,vp,v,sn); break;
    case kCharDsvFl:   cvt_c(  unsigned char,vp,v,sn); break;
    case kUCharDsvFl:  cvt_uc( unsigned char,vp,v,sn); break;
    case kShortDsvFl:  cvt_s(  unsigned char,vp,v,sn); break;
    case kUShortDsvFl: cvt_us( unsigned char,vp,v,sn); break;
    case kLongDsvFl:   cvt_l(  unsigned char,vp,v,sn); break;
    case kULongDsvFl:  cvt_ul( unsigned char,vp,v,sn); break;
    case kIntDsvFl:    cvt_i(  unsigned char,vp,v,sn); break;
    case kUIntDsvFl:   cvt_u(  unsigned char,vp,v,sn); break;
    case kFloatDsvFl:  cvt_f(  unsigned char,vp,v,sn); break;
    case kDoubleDsvFl: cvt_d(  unsigned char,vp,v,sn); break;
    case kSampleDsvFl: cvt_s(  unsigned char,vp,v,sn); break;
    case kRealDsvFl:   cvt_r(  unsigned char,vp,v,sn); break;
    default:
      assert(0);
      return kUnknownTypeDsvRC;
  }
  return kOkDsvRC;
}

cmDsvRC_t cmDsvGetShortMtx(  cmDspValue_t* vp, short* v,          unsigned vn )
{
  vp = _vptr(vp);
  cmDsvRC_t rc;
  unsigned sn;
  if((rc = _cmDsvValidateMtx(vp,vn,&sn)) != kOkDsvRC )
    return rc;

  switch(cmDsvBasicType(vp))
  {
    case kBoolDsvFl:   cvt_b(  short,vp,v,sn); break;
    case kCharDsvFl:   cvt_c(  short,vp,v,sn); break;
    case kUCharDsvFl:  cvt_uc( short,vp,v,sn); break;
    case kShortDsvFl:  cvt_s(  short,vp,v,sn); break;
    case kUShortDsvFl: cvt_us( short,vp,v,sn); break;
    case kLongDsvFl:   cvt_l(  short,vp,v,sn); break;
    case kULongDsvFl:  cvt_ul( short,vp,v,sn); break;
    case kIntDsvFl:    cvt_i(  short,vp,v,sn); break;
    case kUIntDsvFl:   cvt_u(  short,vp,v,sn); break;
    case kFloatDsvFl:  cvt_f(  short,vp,v,sn); break;
    case kDoubleDsvFl: cvt_d(  short,vp,v,sn); break;
    case kSampleDsvFl: cvt_s(  short,vp,v,sn); break;
    case kRealDsvFl:   cvt_r(  short,vp,v,sn); break;
    default:
      assert(0);
      return kUnknownTypeDsvRC;
  }
  return kOkDsvRC;
}

cmDsvRC_t cmDsvGetUShortMtx( cmDspValue_t* vp, unsigned short* v, unsigned vn ) 
{
  vp = _vptr(vp);
  cmDsvRC_t rc;
  unsigned sn;
  if((rc = _cmDsvValidateMtx(vp,vn,&sn)) != kOkDsvRC )
    return rc;

  switch(cmDsvBasicType(vp))
  {
    case kBoolDsvFl:   cvt_b(  unsigned short,vp,v,sn); break;
    case kCharDsvFl:   cvt_c(  unsigned short,vp,v,sn); break;
    case kUCharDsvFl:  cvt_uc( unsigned short,vp,v,sn); break;
    case kShortDsvFl:  cvt_s(  unsigned short,vp,v,sn); break;
    case kUShortDsvFl: cvt_us( unsigned short,vp,v,sn); break;
    case kLongDsvFl:   cvt_l(  unsigned short,vp,v,sn); break;
    case kULongDsvFl:  cvt_ul( unsigned short,vp,v,sn); break;
    case kIntDsvFl:    cvt_i(  unsigned short,vp,v,sn); break;
    case kUIntDsvFl:   cvt_u(  unsigned short,vp,v,sn); break;
    case kFloatDsvFl:  cvt_f(  unsigned short,vp,v,sn); break;
    case kDoubleDsvFl: cvt_d(  unsigned short,vp,v,sn); break;
    case kSampleDsvFl: cvt_s(  unsigned short,vp,v,sn); break;
    case kRealDsvFl:   cvt_r(  unsigned short,vp,v,sn); break;
    default:
      assert(0);
      return kUnknownTypeDsvRC;
  }
  return kOkDsvRC;
}

cmDsvRC_t cmDsvGetLongMtx(   cmDspValue_t* vp, long* v,           unsigned vn )
{
  vp = _vptr(vp);
  cmDsvRC_t rc;
  unsigned sn;
  if((rc = _cmDsvValidateMtx(vp,vn,&sn)) != kOkDsvRC )
    return rc;

  switch(cmDsvBasicType(vp))
  {
    case kBoolDsvFl:   cvt_b(  long,vp,v,sn); break;
    case kCharDsvFl:   cvt_c(  long,vp,v,sn); break;
    case kUCharDsvFl:  cvt_uc( long,vp,v,sn); break;
    case kShortDsvFl:  cvt_s(  long,vp,v,sn); break;
    case kUShortDsvFl: cvt_us( long,vp,v,sn); break;
    case kLongDsvFl:   cvt_l(  long,vp,v,sn); break;
    case kULongDsvFl:  cvt_ul( long,vp,v,sn); break;
    case kIntDsvFl:    cvt_i(  long,vp,v,sn); break;
    case kUIntDsvFl:   cvt_u(  long,vp,v,sn); break;
    case kFloatDsvFl:  cvt_f(  long,vp,v,sn); break;
    case kDoubleDsvFl: cvt_d(  long,vp,v,sn); break;
    case kSampleDsvFl: cvt_s(  long,vp,v,sn); break;
    case kRealDsvFl:   cvt_r(  long,vp,v,sn); break;
    default:
      assert(0);
      return kUnknownTypeDsvRC;
  }
  return kOkDsvRC;
}

cmDsvRC_t cmDsvGetULongMtx(  cmDspValue_t* vp, unsigned long* v,  unsigned vn )
{
  vp = _vptr(vp);
  cmDsvRC_t rc;
  unsigned sn;
  if((rc = _cmDsvValidateMtx(vp,vn,&sn)) != kOkDsvRC )
    return rc;

  switch(cmDsvBasicType(vp))
  {
    case kBoolDsvFl:   cvt_b(  unsigned long,vp,v,sn); break;
    case kCharDsvFl:   cvt_c(  unsigned long,vp,v,sn); break;
    case kUCharDsvFl:  cvt_uc( unsigned long,vp,v,sn); break;
    case kShortDsvFl:  cvt_s(  unsigned long,vp,v,sn); break;
    case kUShortDsvFl: cvt_us( unsigned long,vp,v,sn); break;
    case kLongDsvFl:   cvt_l(  unsigned long,vp,v,sn); break;
    case kULongDsvFl:  cvt_ul( unsigned long,vp,v,sn); break;
    case kIntDsvFl:    cvt_i(  unsigned long,vp,v,sn); break;
    case kUIntDsvFl:   cvt_u(  unsigned long,vp,v,sn); break;
    case kFloatDsvFl:  cvt_f(  unsigned long,vp,v,sn); break;
    case kDoubleDsvFl: cvt_d(  unsigned long,vp,v,sn); break;
    case kSampleDsvFl: cvt_s(  unsigned long,vp,v,sn); break;
    case kRealDsvFl:   cvt_r(  unsigned long,vp,v,sn); break;
    default:
      assert(0);
      return kUnknownTypeDsvRC;
  }
  return kOkDsvRC;
}

cmDsvRC_t cmDsvGetIntMtx(    cmDspValue_t* vp, int* v,            unsigned vn )
{
  vp = _vptr(vp);
  cmDsvRC_t rc;
  unsigned sn;
  if((rc = _cmDsvValidateMtx(vp,vn,&sn)) != kOkDsvRC )
    return rc;

  switch(cmDsvBasicType(vp))
  {
    case kBoolDsvFl:   cvt_b(  int,vp,v,sn); break;
    case kCharDsvFl:   cvt_c(  int,vp,v,sn); break;
    case kUCharDsvFl:  cvt_uc( int,vp,v,sn); break;
    case kShortDsvFl:  cvt_s(  int,vp,v,sn); break;
    case kUShortDsvFl: cvt_us( int,vp,v,sn); break;
    case kLongDsvFl:   cvt_l(  int,vp,v,sn); break;
    case kULongDsvFl:  cvt_ul( int,vp,v,sn); break;
    case kIntDsvFl:    cvt_i(  int,vp,v,sn); break;
    case kUIntDsvFl:   cvt_u(  int,vp,v,sn); break;
    case kFloatDsvFl:  cvt_f(  int,vp,v,sn); break;
    case kDoubleDsvFl: cvt_d(  int,vp,v,sn); break;
    case kSampleDsvFl: cvt_s(  int,vp,v,sn); break;
    case kRealDsvFl:   cvt_r(  int,vp,v,sn); break;
    default:
      assert(0);
      return kUnknownTypeDsvRC;
  }
  return kOkDsvRC;
}

cmDsvRC_t cmDsvGetUIntMtx(   cmDspValue_t* vp, unsigned int* v,   unsigned vn ) 
{
  vp = _vptr(vp);
  cmDsvRC_t rc;
  unsigned sn;
  if((rc = _cmDsvValidateMtx(vp,vn,&sn)) != kOkDsvRC )
    return rc;

  switch(cmDsvBasicType(vp))
  {
    case kBoolDsvFl:   cvt_b(  unsigned int,vp,v,sn); break;
    case kCharDsvFl:   cvt_c(  unsigned int,vp,v,sn); break;
    case kUCharDsvFl:  cvt_uc( unsigned int,vp,v,sn); break;
    case kShortDsvFl:  cvt_s(  unsigned int,vp,v,sn); break;
    case kUShortDsvFl: cvt_us( unsigned int,vp,v,sn); break;
    case kLongDsvFl:   cvt_l(  unsigned int,vp,v,sn); break;
    case kULongDsvFl:  cvt_ul( unsigned int,vp,v,sn); break;
    case kIntDsvFl:    cvt_i(  unsigned int,vp,v,sn); break;
    case kUIntDsvFl:   cvt_u(  unsigned int,vp,v,sn); break;
    case kFloatDsvFl:  cvt_f(  unsigned int,vp,v,sn); break;
    case kDoubleDsvFl: cvt_d(  unsigned int,vp,v,sn); break;
    case kSampleDsvFl: cvt_s(  unsigned int,vp,v,sn); break;
    case kRealDsvFl:   cvt_r(  unsigned int,vp,v,sn); break;
    default:
      assert(0);
      return kUnknownTypeDsvRC;
  }
  return kOkDsvRC;
}

cmDsvRC_t cmDsvGetFloatMtx(  cmDspValue_t* vp, float* v,          unsigned vn )
{
  vp = _vptr(vp);
  cmDsvRC_t rc;
  unsigned sn;
  if((rc = _cmDsvValidateMtx(vp,vn,&sn)) != kOkDsvRC )
    return rc;

  switch(cmDsvBasicType(vp))
  {
    case kBoolDsvFl:   cvt_b(  float,vp,v,sn); break;
    case kCharDsvFl:   cvt_c(  float,vp,v,sn); break;
    case kUCharDsvFl:  cvt_uc( float,vp,v,sn); break;
    case kShortDsvFl:  cvt_s(  float,vp,v,sn); break;
    case kUShortDsvFl: cvt_us( float,vp,v,sn); break;
    case kLongDsvFl:   cvt_l(  float,vp,v,sn); break;
    case kULongDsvFl:  cvt_ul( float,vp,v,sn); break;
    case kIntDsvFl:    cvt_i(  float,vp,v,sn); break;
    case kUIntDsvFl:   cvt_u(  float,vp,v,sn); break;
    case kFloatDsvFl:  cvt_f(  float,vp,v,sn); break;
    case kDoubleDsvFl: cvt_d(  float,vp,v,sn); break;
    case kSampleDsvFl: cvt_s(  float,vp,v,sn); break;
    case kRealDsvFl:   cvt_r(  float,vp,v,sn); break;
    default:
      assert(0);
      return kUnknownTypeDsvRC;
  }
  return kOkDsvRC;
}

cmDsvRC_t cmDsvGetDoubleMtx( cmDspValue_t* vp, double* v,         unsigned vn )
{
  vp = _vptr(vp);
  cmDsvRC_t rc;
  unsigned sn;
  if((rc = _cmDsvValidateMtx(vp,vn,&sn)) != kOkDsvRC )
    return rc;

  switch(cmDsvBasicType(vp))
  {
    case kBoolDsvFl:   cvt_b(  double,vp,v,sn); break;
    case kCharDsvFl:   cvt_c(  double,vp,v,sn); break;
    case kUCharDsvFl:  cvt_uc( double,vp,v,sn); break;
    case kShortDsvFl:  cvt_s(  double,vp,v,sn); break;
    case kUShortDsvFl: cvt_us( double,vp,v,sn); break;
    case kLongDsvFl:   cvt_l(  double,vp,v,sn); break;
    case kULongDsvFl:  cvt_ul( double,vp,v,sn); break;
    case kIntDsvFl:    cvt_i(  double,vp,v,sn); break;
    case kUIntDsvFl:   cvt_u(  double,vp,v,sn); break;
    case kFloatDsvFl:  cvt_f(  double,vp,v,sn); break;
    case kDoubleDsvFl: cvt_d(  double,vp,v,sn); break;
    case kSampleDsvFl: cvt_s(  double,vp,v,sn); break;
    case kRealDsvFl:   cvt_r(  double,vp,v,sn); break;
    default:
      assert(0);
      return kUnknownTypeDsvRC;
  }
  return kOkDsvRC;
}

cmDsvRC_t cmDsvGetSampleMtx( cmDspValue_t* vp, cmSample_t* v,     unsigned vn )
{
  vp = _vptr(vp);
  cmDsvRC_t rc;
  unsigned sn;
  if((rc = _cmDsvValidateMtx(vp,vn,&sn)) != kOkDsvRC )
    return rc;

  switch(cmDsvBasicType(vp))
  {
    case kBoolDsvFl:   cvt_b(  cmSample_t,vp,v,sn); break;
    case kCharDsvFl:   cvt_c(  cmSample_t,vp,v,sn); break;
    case kUCharDsvFl:  cvt_uc( cmSample_t,vp,v,sn); break;
    case kShortDsvFl:  cvt_s(  cmSample_t,vp,v,sn); break;
    case kUShortDsvFl: cvt_us( cmSample_t,vp,v,sn); break;
    case kLongDsvFl:   cvt_l(  cmSample_t,vp,v,sn); break;
    case kULongDsvFl:  cvt_ul( cmSample_t,vp,v,sn); break;
    case kIntDsvFl:    cvt_i(  cmSample_t,vp,v,sn); break;
    case kUIntDsvFl:   cvt_u(  cmSample_t,vp,v,sn); break;
    case kFloatDsvFl:  cvt_f(  cmSample_t,vp,v,sn); break;
    case kDoubleDsvFl: cvt_d(  cmSample_t,vp,v,sn); break;
    case kSampleDsvFl: cvt_s(  cmSample_t,vp,v,sn); break;
    case kRealDsvFl:   cvt_r(  cmSample_t,vp,v,sn); break;
    default:
      assert(0);
      return kUnknownTypeDsvRC;
  }
  return kOkDsvRC;
}

cmDsvRC_t cmDsvGetRealMtx(   cmDspValue_t* vp, cmReal_t* v,       unsigned vn ) 
{
  vp = _vptr(vp);
  cmDsvRC_t rc;
  unsigned sn;
  if((rc = _cmDsvValidateMtx(vp,vn,&sn)) != kOkDsvRC )
    return rc;

  switch(cmDsvBasicType(vp))
  {
    case kBoolDsvFl:   cvt_b(  cmReal_t,vp,v,sn); break;
    case kCharDsvFl:   cvt_c(  cmReal_t,vp,v,sn); break;
    case kUCharDsvFl:  cvt_uc( cmReal_t,vp,v,sn); break;
    case kShortDsvFl:  cvt_s(  cmReal_t,vp,v,sn); break;
    case kUShortDsvFl: cvt_us( cmReal_t,vp,v,sn); break;
    case kLongDsvFl:   cvt_l(  cmReal_t,vp,v,sn); break;
    case kULongDsvFl:  cvt_ul( cmReal_t,vp,v,sn); break;
    case kIntDsvFl:    cvt_i(  cmReal_t,vp,v,sn); break;
    case kUIntDsvFl:   cvt_u(  cmReal_t,vp,v,sn); break;
    case kFloatDsvFl:  cvt_f(  cmReal_t,vp,v,sn); break;
    case kDoubleDsvFl: cvt_d(  cmReal_t,vp,v,sn); break;
    case kSampleDsvFl: cvt_s(  cmReal_t,vp,v,sn); break;
    case kRealDsvFl:   cvt_r(  cmReal_t,vp,v,sn); break;
    default:
      assert(0);
      return kUnknownTypeDsvRC;
  }
  return kOkDsvRC;
}

cmDsvRC_t cmDsvGetStrzMtx(      cmDspValue_t* vp, cmChar_t* v,       unsigned vn )
{
  vp = _vptr(vp);
  cmDsvRC_t rc;
  unsigned sn;
  if((rc = _cmDsvValidateMtx(vp,vn,&sn)) != kOkDsvRC )
    return rc;

  if( cmIsFlag(vp->flags,kConstDsvFl) )
    return kConstViolationDsvRC;

  switch(cmDsvBasicType(vp))
  {
    case kBoolDsvFl:   cvt_b(  cmChar_t,vp,v,sn); break;
    case kCharDsvFl:   cvt_c(  cmChar_t,vp,v,sn); break;
    case kUCharDsvFl:  cvt_uc( cmChar_t,vp,v,sn); break;
    case kShortDsvFl:  cvt_s(  cmChar_t,vp,v,sn); break;
    case kUShortDsvFl: cvt_us( cmChar_t,vp,v,sn); break;
    case kLongDsvFl:   cvt_l(  cmChar_t,vp,v,sn); break;
    case kULongDsvFl:  cvt_ul( cmChar_t,vp,v,sn); break;
    case kIntDsvFl:    cvt_i(  cmChar_t,vp,v,sn); break;
    case kUIntDsvFl:   cvt_u(  cmChar_t,vp,v,sn); break;
    case kFloatDsvFl:  cvt_f(  cmChar_t,vp,v,sn); break;
    case kDoubleDsvFl: cvt_d(  cmChar_t,vp,v,sn); break;
    case kSampleDsvFl: cvt_s(  cmChar_t,vp,v,sn); break;
    case kRealDsvFl:   cvt_r(  cmChar_t,vp,v,sn); break;
    default:
      assert(0);
      return kUnknownTypeDsvRC;
  }
  return kOkDsvRC;
}


bool cmDsvIsType( const cmDspValue_t* vp, unsigned flags )
{   vp = _vcptr(vp); return cmAllFlags(vp->flags,flags); }

bool cmDsvIsMtx( const cmDspValue_t* vp )
{ return cmIsFlag(vp->flags,kMtxDsvFl); }

unsigned cmDsvEleCount( const cmDspValue_t* vp ) 
{ 
  vp = _vcptr(vp); 

  if( cmIsFlag(vp->flags,kMtxDsvFl)==false) 
    return 0; 
  return vp->u.m.rn * vp->u.m.cn; 
}
unsigned cmDsvRows( const cmDspValue_t* vp ) 
{ 
  vp = _vcptr(vp); 

  if( cmIsFlag(vp->flags,kMtxDsvFl)==false) 
    return 0; 
  return vp->u.m.rn; 
}

unsigned cmDsvCols( const cmDspValue_t* vp ) 
{ 
  vp = _vcptr(vp); 

  if( cmIsFlag(vp->flags,kMtxDsvFl)==false) 
    return 0; 
  return vp->u.m.cn; 
}

unsigned cmDsvBasicType( const cmDspValue_t* vp )
{ vp = _vcptr(vp); return _cmDsvBasicType(vp->flags); }

unsigned cmDsvEleByteCount( const cmDspValue_t* vp )
{ vp = _vcptr(vp); return _cmDsvEleByteCount(vp->flags); }


unsigned cmDsvByteCount( unsigned flags, unsigned rn, unsigned cn )
{
  flags  = flags & kTypeDsvMask;
  return (cmIsFlag(flags,kMtxDsvFl) ? rn*cn : 1) * _cmDsvEleByteCount(flags);
}

unsigned  cmDsvSerialByteCount( const cmDspValue_t* vp )
{
  vp = _vcptr(vp); 

  unsigned byteCnt   = sizeof(cmDspValue_t);
  unsigned basicType = cmDsvBasicType(vp);

  if( cmDsvIsMtx(vp) )
  {
    switch(basicType)
    {
      case kBoolDsvFl:
      case kCharDsvFl:
      case kUCharDsvFl:
      case kShortDsvFl:
      case kUShortDsvFl:
      case kLongDsvFl:
      case kULongDsvFl:
      case kIntDsvFl:
      case kUIntDsvFl:
      case kFloatDsvFl:
      case kDoubleDsvFl:
      case kSampleDsvFl:
      case kRealDsvFl:
      case kSymDsvFl:
        byteCnt += cmDsvRows(vp) * cmDsvCols(vp) * cmDsvEleByteCount(vp);
        break;

      case kStrzDsvFl:
      case kJsonDsvFl:
        // not implemented - these serializers still need to be implemented
        byteCnt = 0;
        assert(0); 
        break;

      default:
        { assert(0); }

    }
    
  }
  else // scalars
  {
    switch(basicType)
    {
      case kBoolDsvFl:
      case kCharDsvFl:
      case kUCharDsvFl:
      case kShortDsvFl:
      case kUShortDsvFl:
      case kLongDsvFl:
      case kULongDsvFl:
      case kIntDsvFl:
      case kUIntDsvFl:
      case kFloatDsvFl:
      case kDoubleDsvFl:
      case kSampleDsvFl:
      case kRealDsvFl:
      case kPtrDsvFl:
      case kSymDsvFl:
        // these types are stored as part of the type record
        // and therefore don't need any extra storage
        break;

      case kStrzDsvFl:
        byteCnt +=  vp->u.z==NULL ? 0 : strlen(vp->u.z) + 1;
        break;

      case kJsonDsvFl:
        if( vp->u.j != NULL )
        {
          assert( cmJsonValidate(vp->u.j) == kOkJsRC );
          byteCnt += cmJsonSerialByteCount( vp->u.j );
        }
        break;

      default:
        { assert(0); }

    }
  }

  return byteCnt;
}

unsigned  cmDsvSerialDataByteCount( const cmDspValue_t* vp )
{ vp = _vcptr(vp); return cmDsvSerialByteCount(vp) - sizeof(cmDspValue_t); }

cmDsvRC_t cmDsvSerialize( const cmDspValue_t* vp, void* buf, unsigned bufByteCnt )
{
  vp = _vcptr(vp); 

  // the buffer must be at least large enough to
  // hold the cmDspValue_t header record
  if( bufByteCnt < sizeof(cmDspValue_t) )
    return kRetTooSmallDsvRC;

  cmDspValue_t* hp        = (cmDspValue_t*)buf;
  char*         dp        = (char*)(hp + 1);
  char*         ep        = ((char*)buf) + bufByteCnt;
  unsigned      basicType = cmDsvBasicType(vp);

  *hp = *vp; // copy the type header record

  if( cmDsvIsMtx(vp) )
  {
    switch(basicType)
    {
      case kBoolDsvFl:
      case kCharDsvFl:
      case kUCharDsvFl:
      case kShortDsvFl:
      case kUShortDsvFl:
      case kLongDsvFl:
      case kULongDsvFl:
      case kIntDsvFl:
      case kUIntDsvFl:
      case kFloatDsvFl:
      case kDoubleDsvFl:
      case kSampleDsvFl:
      case kRealDsvFl:
        {
          // if the mtx data ptr is NULL then set it to NULL in the serialized data
          if( vp->u.m.u.vp == NULL )
            hp->u.m.u.vp = NULL;
          else
          {
            unsigned bn = cmDsvRows(vp) * cmDsvCols(vp) * cmDsvEleByteCount(vp);
            if( dp + bn > ep )
              return kRetTooSmallDsvRC;

            memcpy(dp, vp->u.m.u.vp, bn);
            hp->u.m.u.vp = dp;
          }
        }
        break;

      case kStrzDsvFl:
      case kJsonDsvFl:
        // not implemented - fall through and assert

      default:
        { assert(0); }

    }
    
  }
  else // scalars
  {
    switch(basicType)
    {
      case kBoolDsvFl:
      case kCharDsvFl:
      case kUCharDsvFl:
      case kShortDsvFl:
      case kUShortDsvFl:
      case kLongDsvFl:
      case kULongDsvFl:
      case kIntDsvFl:
      case kUIntDsvFl:
      case kFloatDsvFl:
      case kDoubleDsvFl:
      case kSampleDsvFl:
      case kRealDsvFl:
      case kPtrDsvFl:
      case kSymDsvFl:
        // these types are stored as part of the type record
        // and therefore don't need any extra storage
        break;

      case kStrzDsvFl:
        if( vp->u.z==NULL )
          hp->u.z = NULL;
        else
        {
          unsigned bn =  strlen(vp->u.z) + 1;

          if( dp + bn > ep )
            return kRetTooSmallDsvRC;

          strncpy(dp,vp->u.z,bn);
          hp->u.z = dp;
        }
        
        break;

      case kJsonDsvFl:
        if( vp->u.j == NULL )
          hp->u.j = NULL;
        else
        {
          unsigned bn = cmJsonSerialByteCount( vp->u.j );

          if( dp + bn > ep )
            return kRetTooSmallDsvRC;

          if( cmJsonSerialize(vp->u.j,dp,bufByteCnt-sizeof(cmDspValue_t)) != kOkJsRC )
            return kRetTooSmallDsvRC;
        }
        break;

      default:
        { assert(0); }

    }
  }

  return kOkDsvRC;  
}

cmDsvRC_t cmDsvDeserializeInPlace( cmDspValue_t* vp, unsigned dataBufByteCnt )
{
  vp = _vptr(vp); 
 
  assert( cmIsFlag(vp->flags,kJsonDsvFl)==false );

  void**   vpp   = _cmDsvGetDataPtrPtr(vp);
  if( vpp != NULL )
  {
    *vpp = vp + 1;
  }
  return kOkDsvRC;
}

cmDsvRC_t cmDsvDeserializeJson( cmDspValue_t* vp, cmJsonH_t jsH )
{
  vp = _vptr(vp); 

  if( cmJsonClearTree(jsH) )
    return kJsonDeserialFailDsvRC;

  if( cmJsonDeserialize(jsH, vp + 1, NULL ) != kOkJsRC )
    return kJsonDeserialFailDsvRC;
  
  vp->u.j = cmJsonRoot(jsH);

  return kOkDsvRC;  
}

void cmDsvPrint( const cmDspValue_t* vp, const cmChar_t* label, cmRpt_t* rpt )
{
  vp = _vcptr(vp); 

  const char* noLbl="";
  const char* lbl  = label==NULL? noLbl : label;

  if( cmDsvIsMtx(vp) )
  {
    unsigned i,j;
    unsigned rn = cmDsvCols(vp);
    unsigned cn = cmDsvRows(vp);
    for(i=0; i<rn; ++i)
    {
      for(j=0; j<cn; ++j)
      {
        switch( cmDsvBasicType(vp) )
        {
          case kCharDsvFl:   cmRptPrintf (rpt,"%s%c ",lbl,vp->u.m.u.cp[  (j*rn) + i ]); break;
          case kUCharDsvFl:  cmRptPrintf (rpt,"%s%c ",lbl,vp->u.m.u.ucp[ (j*rn) + i ]); break;
          case kShortDsvFl:  cmRptPrintf (rpt,"%s%i ",lbl,vp->u.m.u.sp[  (j*rn) + i ]); break;
          case kUShortDsvFl: cmRptPrintf (rpt,"%s%i ",lbl,vp->u.m.u.usp[ (j*rn) + i ]); break;
          case kLongDsvFl:   cmRptPrintf (rpt,"%s%i ",lbl,vp->u.m.u.lp[  (j*rn) + i ]); break;
          case kULongDsvFl:  cmRptPrintf (rpt,"%s%i ",lbl,vp->u.m.u.ulp[ (j*rn) + i ]); break;
          case kIntDsvFl:    cmRptPrintf (rpt,"%s%i ",lbl,vp->u.m.u.ip[  (j*rn) + i ]); break;
          case kUIntDsvFl:   cmRptPrintf (rpt,"%s%i ",lbl,vp->u.m.u.up[  (j*rn) + i ]); break;
          case kFloatDsvFl:  cmRptPrintf (rpt,"%s%f ",lbl,vp->u.m.u.fp[  (j*rn) + i ]); break;
          case kDoubleDsvFl: cmRptPrintf (rpt,"%s%f ",lbl,vp->u.m.u.dp[  (j*rn) + i ]); break;
          case kSampleDsvFl: cmRptPrintf (rpt,"%s%f ",lbl,vp->u.m.u.ap[  (j*rn) + i ]); break;
          case kRealDsvFl:   cmRptPrintf (rpt,"%s%f ",lbl,vp->u.m.u.rp[  (j*rn) + i ]); break;
          case kStrzDsvFl:   cmRptPrintf (rpt,"%s%s ",lbl,vp->u.m.u.zp[  (j*rn) + i ]); break;
          case kJsonDsvFl:   cmJsonPrintTree(vp->u.m.u.jp[ (j*rn) + i ],rpt);   break;    
          default:
            { assert(0); }
        }
      }
      cmRptPrint(rpt,"\n");
    }
  }
  else
  {
    switch( cmDsvBasicType(vp) )
    {
      case kBoolDsvFl:   cmRptPrintf(rpt,"%s%s ",lbl,vp->u.b ? "true" : "false"); break;
      case kCharDsvFl:   cmRptPrintf(rpt,"%s%c ",lbl,vp->u.c);  break;
      case kUCharDsvFl:  cmRptPrintf(rpt,"%s%c ",lbl,vp->u.uc); break;
      case kShortDsvFl:  cmRptPrintf(rpt,"%s%i ",lbl,vp->u.s);  break;
      case kUShortDsvFl: cmRptPrintf(rpt,"%s%i ",lbl,vp->u.us); break;
      case kLongDsvFl:   cmRptPrintf(rpt,"%s%i ",lbl,vp->u.l);  break;
      case kULongDsvFl:  cmRptPrintf(rpt,"%s%i ",lbl,vp->u.ul); break;
      case kIntDsvFl:    cmRptPrintf(rpt,"%s%i ",lbl,vp->u.i);  break;
      case kUIntDsvFl:   cmRptPrintf(rpt,"%s%i ",lbl,vp->u.u);  break;
      case kFloatDsvFl:  cmRptPrintf(rpt,"%s%f ",lbl,vp->u.f);  break;
      case kDoubleDsvFl: cmRptPrintf(rpt,"%s%f ",lbl,vp->u.d);  break;
      case kSampleDsvFl: cmRptPrintf(rpt,"%s%f ",lbl,vp->u.a);  break;
      case kRealDsvFl:   cmRptPrintf(rpt,"%s%f ",lbl,vp->u.r);  break;
      case kPtrDsvFl:    cmRptPrintf(rpt,"%s%p ",lbl,vp->u.vp); break;
      case kStrzDsvFl:   cmRptPrintf(rpt,"%s%s ",lbl,vp->u.z);  break;
      case kSymDsvFl:    cmRptPrintf(rpt,"%s%i ",lbl,vp->u.u); break;
      case kJsonDsvFl:   cmJsonPrintTree(vp->u.j,rpt);   break;    
      default:
        { assert(0); }
    }
  }
}

void cmDsvToString( const cmDspValue_t* vp, const cmChar_t* label, cmChar_t* s, unsigned sN )
{
  vp = _vcptr(vp); 

  const char* noLbl="";
  const char* lbl  = label==NULL? noLbl : label;

  if( cmDsvIsMtx(vp) )
  {
    unsigned i,j;
    unsigned rn = cmDsvCols(vp);
    unsigned cn = cmDsvRows(vp);
    for(i=0; i<rn; ++i)
    {
      for(j=0; j<cn && sN>2; ++j)
      {
        switch( cmDsvBasicType(vp) )
        {
          case kCharDsvFl:   snprintf(s,sN,"%s%c ",lbl,vp->u.m.u.cp[  (j*rn) + i ]); break;
          case kUCharDsvFl:  snprintf(s,sN,"%s%c ",lbl,vp->u.m.u.ucp[ (j*rn) + i ]); break;
          case kShortDsvFl:  snprintf(s,sN,"%s%i ",lbl,vp->u.m.u.sp[  (j*rn) + i ]); break;
          case kUShortDsvFl: snprintf(s,sN,"%s%i ",lbl,vp->u.m.u.usp[ (j*rn) + i ]); break;
          case kLongDsvFl:   snprintf(s,sN,"%s%li ",lbl,vp->u.m.u.lp[  (j*rn) + i ]); break;
          case kULongDsvFl:  snprintf(s,sN,"%s%li ",lbl,vp->u.m.u.ulp[ (j*rn) + i ]); break;
          case kIntDsvFl:    snprintf(s,sN,"%s%i ",lbl,vp->u.m.u.ip[  (j*rn) + i ]); break;
          case kUIntDsvFl:   snprintf(s,sN,"%s%i ",lbl,vp->u.m.u.up[  (j*rn) + i ]); break;
          case kFloatDsvFl:  snprintf(s,sN,"%s%f ",lbl,vp->u.m.u.fp[  (j*rn) + i ]); break;
          case kDoubleDsvFl: snprintf(s,sN,"%s%f ",lbl,vp->u.m.u.dp[  (j*rn) + i ]); break;
          case kSampleDsvFl: snprintf(s,sN,"%s%f ",lbl,vp->u.m.u.ap[  (j*rn) + i ]); break;
          case kRealDsvFl:   snprintf(s,sN,"%s%f ",lbl,vp->u.m.u.rp[  (j*rn) + i ]); break;
          case kStrzDsvFl:   snprintf(s,sN,"%s%s ",lbl,vp->u.m.u.zp[  (j*rn) + i ]); break;
          case kJsonDsvFl:   cmJsonLeafToString(vp->u.m.u.jp[ (j*rn) + i ],s,sN);   break;    
          default:
            { assert(0); }
        }

        unsigned n = strlen(s);
        sN -= n;
        s  += n;
        
        
      }
      if( sN > 2 )
        snprintf(s,sN,"\n");
    }
  }
  else
  {
    switch( cmDsvBasicType(vp) )
    {
      case kBoolDsvFl:   snprintf(s,sN,"%s%s ",lbl,vp->u.b ? "true" : "false"); break;
      case kCharDsvFl:   snprintf(s,sN,"%s%c ",lbl,vp->u.c);  break;
      case kUCharDsvFl:  snprintf(s,sN,"%s%c ",lbl,vp->u.uc); break;
      case kShortDsvFl:  snprintf(s,sN,"%s%i ",lbl,vp->u.s);  break;
      case kUShortDsvFl: snprintf(s,sN,"%s%i ",lbl,vp->u.us); break;
      case kLongDsvFl:   snprintf(s,sN,"%s%li ",lbl,vp->u.l);  break;
      case kULongDsvFl:  snprintf(s,sN,"%s%li ",lbl,vp->u.ul); break;
      case kIntDsvFl:    snprintf(s,sN,"%s%i ",lbl,vp->u.i);  break;
      case kUIntDsvFl:   snprintf(s,sN,"%s%i ",lbl,vp->u.u);  break;
      case kFloatDsvFl:  snprintf(s,sN,"%s%f ",lbl,vp->u.f);  break;
      case kDoubleDsvFl: snprintf(s,sN,"%s%f ",lbl,vp->u.d);  break;
      case kSampleDsvFl: snprintf(s,sN,"%s%f ",lbl,vp->u.a);  break;
      case kRealDsvFl:   snprintf(s,sN,"%s%f ",lbl,vp->u.r);  break;
      case kPtrDsvFl:    snprintf(s,sN,"%s%p ",lbl,vp->u.vp); break;
      case kStrzDsvFl:   snprintf(s,sN,"%s%s ",lbl,vp->u.z);  break;
      case kSymDsvFl:    snprintf(s,sN,"%s%i ",lbl,vp->u.u); break;
      case kJsonDsvFl:   cmJsonLeafToString(vp->u.j,s,sN);   break;    
      default:
        { assert(0); }
    }
  }
}
