//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmText.h"

#include "cmFloatTypes.h"
#include "cmVectOps.h"


typedef struct cmTextSys_str
{
  cmErr_t    err;
  cmLHeapH_t lhH;
  cmChar_t*  buf;
} cmTextSys_t;

cmTextSysH_t cmTextSysNullHandle = cmSTATIC_NULL_HANDLE;
cmTextSysH_t _cmTextSysGlobalH   = cmSTATIC_NULL_HANDLE;

cmTextSys_t* _cmTextSysHandleToPtr( cmTextSysH_t h )
{
  cmTextSys_t* p = (cmTextSys_t*)h.h;
  assert(p != NULL);
  return p;
}

cmTxRC_t _cmTextSysFinalize( cmTextSys_t* p )
{
  cmTxRC_t rc = kOkTxRC;

  if( cmLHeapIsValid(p->lhH) )
    cmLHeapDestroy(&p->lhH);

  cmMemFree(p->buf);

  cmMemFree(p);
  return rc;
}

cmTxRC_t cmTextSysInitialize(  cmCtx_t* ctx, cmTextSysH_t* hp )
{
  cmTxRC_t rc;
  if((rc = cmTextSysFinalize(hp)) != kOkTxRC )
    return rc;

  cmTextSys_t* p = cmMemAllocZ(cmTextSys_t,1);

  cmErrSetup(&p->err,&ctx->rpt,"Text System");

  if( cmLHeapIsValid( p->lhH = cmLHeapCreate( 8192, ctx )) == false )
  {
    rc = cmErrMsg(&p->err,kLHeapFailTxRC,"Linked Heap create failed.");
    goto errLabel;
  }
  
  hp->h = p;

 errLabel:
  return rc;
}

cmTxRC_t cmTextSysFinalize( cmTextSysH_t* hp )
{
  cmTxRC_t rc = kOkTxRC;

  if( hp == NULL || cmTextSysIsValid(*hp)==false )
    return kOkTxRC;

  cmTextSys_t* p = _cmTextSysHandleToPtr(*hp);
  
  if((rc = _cmTextSysFinalize(p)) == kOkTxRC )
  {
    hp->h = NULL;
  }

  return rc;
}

bool     cmTextSysIsValid( cmTextSysH_t h )
{ return h.h != NULL; }


cmChar_t*    _cmTextSysVPrintf( cmTextSysH_t h, cmLHeapH_t lhH, bool staticFl, const cmChar_t* fmt, va_list vl )
{  
  va_list     vl1;
  va_copy(vl1,vl);

  cmTextSys_t* p = cmLHeapIsValid(lhH) ? NULL : _cmTextSysHandleToPtr(h);
  unsigned     n = vsnprintf(NULL,0,fmt,vl);
  cmChar_t*    s = NULL;

  if( staticFl )
    s = p->buf = cmMemResize(cmChar_t,p->buf,n+1);
  else
    s = cmLhAllocZ(cmLHeapIsValid(lhH) ? lhH : p->lhH,cmChar_t,n+1);
  
  unsigned     m = vsnprintf(s,n+1,fmt,vl1);
  assert(m==n);
  s[n] = 0;
  va_end(vl1);
  return s;
}

cmChar_t*    cmTextSysVPrintfH( cmLHeapH_t lhH, const cmChar_t* fmt, va_list vl )
{ return _cmTextSysVPrintf( cmTextSysNullHandle, lhH, false, fmt, vl ); }

cmChar_t*    cmTextSysPrintfH( cmLHeapH_t lhH, const cmChar_t* fmt, ... )
{
  va_list     vl;
  va_start( vl, fmt );
  cmChar_t* s = cmTextSysVPrintfH(lhH,fmt,vl);
  va_end(vl);
  return s;
}

cmChar_t*     cmTextSysVPrintfS( cmTextSysH_t h, const cmChar_t* fmt, va_list vl )
{ return _cmTextSysVPrintf(h,cmLHeapNullHandle,true,fmt,vl); }

cmChar_t*     cmTextSysPrintfS(  cmTextSysH_t h, const cmChar_t* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  cmChar_t* s = cmTextSysVPrintfS(h,fmt,vl);
  va_end(vl);
  return s;
}

cmChar_t*     cmTextSysVPrintf( cmTextSysH_t h, const cmChar_t* fmt, va_list vl )
{ return _cmTextSysVPrintf(h,cmLHeapNullHandle,false,fmt,vl);  }

cmChar_t*     cmTextSysPrintf(  cmTextSysH_t h, const cmChar_t* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  cmChar_t* s = cmTextSysVPrintf(h,fmt,vl);
  va_end(vl);
  return s;
}

void                cmTextSysFreeStr( cmTextSysH_t h, const cmChar_t* s )
{
  cmTextSys_t* p = _cmTextSysHandleToPtr(h);
  cmLhFree(p->lhH,(cmChar_t*)s);
}

bool  cmTextSysIsStored(cmTextSysH_t h, const cmChar_t* s )
{ 
  cmTextSys_t* p = _cmTextSysHandleToPtr(h);  
  return cmLHeapIsPtrInHeap(p->lhH,s);
}


  //
  // Global interface
  //



cmTxRC_t cmTsInitialize( cmCtx_t* ctx )
{ return cmTextSysInitialize(ctx,&_cmTextSysGlobalH); }

cmTxRC_t cmTsFinalize()
{ return cmTextSysFinalize(&_cmTextSysGlobalH); }

bool     cmTsIsValid()
{ return cmTextSysIsValid(_cmTextSysGlobalH); }

cmChar_t*     cmTsVPrintfS( const cmChar_t* fmt, va_list vl )
{ return cmTextSysVPrintfS(_cmTextSysGlobalH,fmt,vl); }

cmChar_t*     cmTsPrintfS(  const cmChar_t* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  cmChar_t* s = cmTsVPrintfS(fmt,vl);
  va_end(vl);
  return s;
}

cmChar_t*     cmTsVPrintfH( cmLHeapH_t h, const cmChar_t* fmt, va_list vl )
{ return cmTextSysVPrintfH(h,fmt,vl); }

cmChar_t*     cmTsPrintfH(  cmLHeapH_t h, const cmChar_t* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  cmChar_t* s = cmTsVPrintfH(h,fmt,vl);
  va_end(vl);
  return s;  
}

cmChar_t*     cmTsVPrintf( const cmChar_t* fmt, va_list vl )
{ return cmTextSysVPrintf(_cmTextSysGlobalH,fmt,vl); }

cmChar_t*     cmTsPrintf(  const cmChar_t* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  cmChar_t* s = cmTsVPrintf(fmt,vl);
  va_end(vl);
  return s;
}

void                cmTsFreeStr( const cmChar_t* s )
{ cmTextSysFreeStr(_cmTextSysGlobalH,s); }

bool           cmTsIsStored( const cmChar_t* s )
{ return cmTextSysIsStored(_cmTextSysGlobalH,s); }

cmChar_t* cmTsVPrintfP( cmChar_t* s, const cmChar_t* fmt, va_list vl )
{
  va_list     vl1;
  va_copy(vl1,vl);

  int     n = vsnprintf(NULL,0,fmt,vl);
  assert(n != -1);

  s = cmMemResize(cmChar_t,s,n+1);
  
  unsigned     m = vsnprintf(s,n+1,fmt,vl1);
  assert(m==n);
  s[n] = 0;
  return s;
}

cmChar_t* cmTsPrintfP( cmChar_t* s, const cmChar_t* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  s = cmTsVPrintfP(s,fmt,vl);
  va_end(vl);
  return s;
}

void _cmTxError( cmErr_t* err, cmTxRC_t rc, const char* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  cmErrVMsg(err,rc,fmt,vl);
  va_end(vl);
}


cmTxRC_t _cmTxRptError( cmErr_t* err, const char* msg, const char* inputText )
{
  if( err == NULL )
    return kOkTxRC;

  if( inputText == NULL )
  {
    _cmTxError(err,kNullTxRC,"Text to %s conversion failed due to NULL input text.");
    return kNullTxRC;
  }

  if( errno != 0 )
  {
    _cmTxError(err,kCvtErrTxRC,"Text to %s conversion failed on input '%s'.",msg,inputText);
    return kCvtErrTxRC;
  }

  return kOkTxRC;
}


cmTxRC_t cmTextToInt(const char* text, int* vp, cmErr_t* err )
{
  assert( vp != NULL );

  cmTxRC_t rc = kOkTxRC;
  int en      = errno;

  errno       = 0;
  *vp         = text==NULL ? 0 : strtol(text,NULL,0);
  rc          = _cmTxRptError(err,"integer",text);
  errno       = en;

  return rc;
}

cmTxRC_t   cmTextToUInt(   const char* text, unsigned* vp, cmErr_t* err )
{
  assert( vp != NULL );

  cmTxRC_t rc = kOkTxRC;
  int en = errno;

  errno  = 0;
  *vp    = text==NULL ? 0 : (unsigned)strtol(text,NULL,0);

  rc = _cmTxRptError(err,"unsigned integer",text);

  errno = en;

  return rc;
}

cmTxRC_t   cmTextToFloat(  const char* text, float*    vp, cmErr_t* err )
{
  assert( vp != NULL );

  cmTxRC_t rc = kOkTxRC;
  int en = errno;
  errno  = 0;
  *vp    = text==NULL ? 0 : (float)strtod(text,NULL);

  rc = _cmTxRptError(err,"float",text);

  errno = en;

  return rc;
}

cmTxRC_t   cmTextToDouble( const char* text, double*   vp, cmErr_t* err )
{
  assert( vp != NULL );

  cmTxRC_t rc = kOkTxRC;
  int en = errno;
  errno  = 0;
  *vp    = text==NULL ? 0 : strtod(text,NULL);

  rc = _cmTxRptError(err,"double",text);

  errno = en;

  return rc;
}

cmTxRC_t   cmTextToBool(   const char* text, bool*  vp, cmErr_t* err )
{
  assert( vp != NULL );

  cmTxRC_t rc = kOkTxRC;

  if( strcasecmp(text,"true") == 0 || strcasecmp(text,"0") == 0 )
    *vp = true;
  else
    if( strcasecmp(text,"false") == 0 || strcasecmp(text,"1") == 0 )
      *vp = false;
    else
      rc = _cmTxRptError(err,"bool",text);


  return rc;

}


cmChar_t*       cmTextNextNonWhiteOrEos( cmChar_t* s )
{
  assert( s != NULL );

  while( (*s) && isspace(*s) )
    ++s;

  return  s;
}

const cmChar_t* cmTextNextNonWhiteOrEosC( const cmChar_t* s )
{ return cmTextNextNonWhiteOrEos((cmChar_t*)s); }


cmChar_t*       cmTextNextNonWhite( cmChar_t* s )
{ //return (*(s=cmTextNextNonWhiteOrEos(s))) == 0 ? NULL : s; 
  s=cmTextNextNonWhiteOrEos(s);
  if( *s == 0 )
    return NULL;
  return s;
}

const cmChar_t* cmTextNextNonWhiteC( const cmChar_t* s )
{ return cmTextNextNonWhite((cmChar_t*)s);  }


cmChar_t*       cmTextPrevNonWhiteOrBos( cmChar_t* s0, const cmChar_t* s1 )
{
  assert( s0!=NULL && s1!=NULL && s0 <= s1 );

  for(; s0 < s1; --s1 )
    if( !isspace(*s1) )
      break;

  return (cmChar_t*)s1;
}

const cmChar_t* cmTextPrevNonWhiteOrBosC( const cmChar_t* s0, const cmChar_t* s1 )
{ return cmTextPrevNonWhiteOrBos((cmChar_t*)s0,s1); }

cmChar_t*       cmTextPrevNonWhite( cmChar_t* s0, const cmChar_t* s1 )
{  
  cmChar_t* s2;
  if((s2 = cmTextPrevNonWhiteOrBos(s0,s1)) == s0 )
    return NULL;
  return s2;
}

const cmChar_t* cmTextPrevNonWhiteC( const cmChar_t* s0, const cmChar_t* s1 )
{ return cmTextPrevNonWhite((cmChar_t*)s0,s1);  }

cmChar_t*       cmTextNextWhiteOrEos( cmChar_t* s )
{
  assert( s!=NULL);
  while(*s && !isspace(*s) )
    ++s;
  return s;
}

const cmChar_t* cmTextNextWhiteOrEosC( const cmChar_t* s )
{ return cmTextNextWhiteOrEos((cmChar_t*)s); }

cmChar_t*       cmTextNextWhite( cmChar_t* s )
{ return (*(s=cmTextNextWhiteOrEos(s)))!=0 ? s : NULL; }

const cmChar_t* cmTextNextWhiteC( const cmChar_t* s )
{ return cmTextNextWhite((cmChar_t*)s); }

cmChar_t*       cmTextPrevWhiteOrBos( cmChar_t* s0, const cmChar_t* s1 )
{ 
  assert( s0!=NULL && s1!=NULL && s0 <= s1 );
  while( s1>s0 && !isspace(*s1) )
    --s1;
  return (cmChar_t*)s1;
}

const cmChar_t* cmTextPrevWhiteOrBosC( const cmChar_t* s0, const cmChar_t* s1 )
{ return cmTextPrevWhiteOrBos((cmChar_t*)s0,s1); } 

cmChar_t*       cmTextPrevWhite( cmChar_t* s0, const cmChar_t* s1 )
{
  cmChar_t* s2;
  if((s2 = cmTextPrevWhiteOrBos(s0,s1)) == s0 )
    return NULL;
  return s2;
}

const cmChar_t* cmTextPrevWhiteC( const cmChar_t* s0, const cmChar_t* s1 )
{ return cmTextPrevWhite((cmChar_t*)s0,s1); }

cmChar_t*       cmTextBegOfLine( cmChar_t* s0, const cmChar_t* s1 )
{
  assert( s1!=NULL && s0!=NULL && s1 >= s0 );

  if( s0 == s1 )
    return s0;

  --s1;

  while( s1>s0 && *s1 != '\n' )
    --s1;
  
  if( *s1 == '\n' )
    ++s1;

  return (cmChar_t*)s1;

}

const cmChar_t* cmTextBegOfLineC( const cmChar_t* s0, const cmChar_t* s1 )
{ return cmTextBegOfLine((cmChar_t*)s0,s1); }

cmChar_t*       cmTextEndOfLine( cmChar_t* s )
{
  
  assert( s!=NULL);

  while( *s!=0 && *s != '\n' )
    ++s;

  return s;

}

const cmChar_t* cmTextEndOfLineC( const cmChar_t* s )
{ return cmTextEndOfLine((cmChar_t*)s); }

cmChar_t*       cmTextLastNonWhiteChar(  const cmChar_t* s )
{
  unsigned n;
  if(s==NULL || (n = strlen(s)) == 0 )
    return NULL;

  cmChar_t* s0 = (cmChar_t*)s + n-1;

  for(; s0>=s; --s0)
    if( !isspace(*s0) )
      return s0;

  return NULL;

}

const cmChar_t* cmTextLastNonWhiteCharC( const cmChar_t* s )
{ return cmTextLastNonWhiteChar(s); }


cmChar_t*       cmTextLastWhiteChar(  const cmChar_t* s )
{
  unsigned n;
  if(s==NULL || (n = strlen(s)) == 0 )
    return NULL;

  cmChar_t* s0 = (cmChar_t*)s + n-1;

  for(; s0>=s; --s0)
    if( isspace(*s0) )
      return s0;

  return NULL;

}

const cmChar_t* cmTextLastWhiteCharC( const cmChar_t* s )
{ return cmTextLastWhiteChar(s); }


void cmTextShrinkS( cmChar_t* s, const cmChar_t* t, unsigned tn )
{ cmVOC_Shrink(s,strlen(s)+1,t,tn); }

void cmTextShrinkSN(cmChar_t* s, unsigned sn, const cmChar_t* t, unsigned tn )
{ cmVOC_Shrink(s,sn,t,tn); }

void cmTextClip(    cmChar_t* s, unsigned n )
{
  if( n == 0 || s == NULL || strlen(s)==0 )
    return;

  if( n >= strlen(s) )
  {
    s[0]=0;
    return;
  }

  s[ strlen(s)-n ] = 0; 
    
}

cmChar_t* cmTextTrimBegin( cmChar_t* s )
{
  if( s==NULL || strlen(s) == 0 )
    return s;

  cmChar_t* s0 = cmTextNextNonWhite(s);

  // no non-white char's exist
  if( s0 == NULL )
  {
    s[0] = 0;
    return s;
  }

  if( s0 != s )
    cmTextShrinkS(s,s,s0-s);

  return s;
}

cmChar_t* cmTextTrimEnd( cmChar_t* s )
{
  unsigned sn;

  if( s==NULL || (sn = strlen(s))==0)
    return s;

  cmChar_t* s0 = cmTextLastNonWhiteChar(s);

  if(s0-s+1 < sn )
    s[s0-s+1] = 0;


  return s;
}

cmChar_t* cmTextTrim( cmChar_t* s)
{
  cmTextTrimBegin(s);
  cmTextTrimEnd(s);
  return s;
}


cmChar_t* cmTextExpandS( cmChar_t* s, const cmChar_t* t, unsigned tn )
{ return cmVOC_Expand(s,strlen(s)+1,t,tn); }

cmChar_t* cmTextReplaceSN( cmChar_t* s, const cmChar_t* t, unsigned tn, const cmChar_t* u, unsigned un )
{ 
  unsigned n = strlen(s)+1;
  return cmVOC_Replace(s,&n,t,tn,u,un);
}

cmChar_t* cmTextReplaceS( cmChar_t* s, const cmChar_t* t, unsigned tn, const cmChar_t* u )
{ return cmTextReplaceSN(s,t,tn,u,u==NULL ? 0 : strlen(u)); }

cmChar_t* _cmTextReplace( cmChar_t* s, const cmChar_t* t, const cmChar_t* u, unsigned n )
{
  // we will go into an endless loop if 't' is contained in 'u' and n > 1.
  //assert( s!= NULL && t!=NULL && u!=NULL && (n==1 || strstr(u,t) == NULL) );

  assert( s!= NULL && t!=NULL && u!=NULL );

  int       tn = strlen(t);
  cmChar_t* c  = NULL;
  unsigned  i  = 0;
  cmChar_t* s0 = s;

  while( (c = strstr(s0,t)) != NULL ) 
  {
    int offs = c - s;
    s = cmTextReplaceS(s,c,tn,u);

    assert(s!=NULL);

    s0 = s + offs + tn;

    ++i;
    if( n!=cmInvalidCnt && i>=n)
      break;

  };

  return s;
}

cmChar_t* cmTextReplaceAll( cmChar_t* s, const cmChar_t* t, const cmChar_t* u )
{ return _cmTextReplace(s,t,u,cmInvalidCnt); }

cmChar_t* cmTextReplaceFirst( cmChar_t* s, const cmChar_t* t, const cmChar_t* u )
{ return _cmTextReplace(s,t,u,1); }

cmChar_t* cmTextInsertSN(  cmChar_t* s, const cmChar_t* t, const cmChar_t* u, unsigned un )
{
  unsigned n = strlen(s)+1;
  return cmVOC_Replace(s,&n,t,0,u,un);
}

cmChar_t* cmTextInsertS(  cmChar_t* s, const cmChar_t* t, const cmChar_t* u )
{ return cmTextInsertSN(s,t,u,u==NULL?0:strlen(u)); }


cmChar_t* cmTextAppend( cmChar_t* s, unsigned* sn, const cmChar_t* u, unsigned un )
{  return cmVOC_Replace(s,sn,s+(*sn),0,u,un); }

cmChar_t* cmTextAppendSN( cmChar_t* s, const cmChar_t* u, unsigned un )
{
  unsigned sn = s==NULL ? 0 : strlen(s);
  if( un > 0 )
  {
    s = cmTextAppend(s,&sn,u,un+1);
    s[sn-1] = 0;
  }
  return s;
}

// append u[un] to s[] and append terminating zero.
cmChar_t* cmTextAppendSNZ( cmChar_t* s, const cmChar_t* u, unsigned un )
{
  unsigned sn = s==NULL ? 0 : strlen(s)+1;
  cmChar_t z = 0;
  s = cmTextAppend(s,&sn,u,un); 
  return cmTextAppend(s,&sn,&z,1);
}

// both s[] and u[] are strz's
cmChar_t* cmTextAppendSS( cmChar_t* s, const cmChar_t* u )
{ return cmTextAppendSN(s,u,strlen(u)); }

cmChar_t* cmTextVAppendSS( cmChar_t* s, ... )
{
  va_list vl;
  va_start(vl,s);
  do
  {
    cmChar_t* s0 = va_arg(vl,cmChar_t*);
    if( s0 == NULL )
      break;

    s = cmTextAppendSS(s,s0);

  }while(1);

  va_end(vl);

  return s;
}



cmChar_t* cmTextAppendChar( cmChar_t* s, cmChar_t c, unsigned n )
{
  if( n <= 0 )
    return s;

  cmChar_t t[ n+1 ];
  memset(t,' ',n);
  t[n] = 0;
  return cmTextAppendSS(s,t);
}

bool cmTextIsEmpty( const cmChar_t* s )
{
  if( s!=NULL )
    for(; *s; ++s )
      if( !isspace(*s) )
        return false;
  return true;
}

bool cmTextIsNotEmpty( const cmChar_t* s )
{ return !cmTextIsEmpty(s); }


unsigned cmTextLength( const cmChar_t* s0 )
{
  if( s0 == NULL )
    return 0;
  return strlen(s0);
}

int cmTextCmp( const cmChar_t* s0, const cmChar_t* s1 )
{
  if( s0 == NULL && s1 == NULL )
    return 0;

  if( s0 == NULL || s1 == NULL )
  {
    if( s0 == NULL )
      return -1;
    return 1;
  }

  return strcmp(s0,s1);
}

int cmTextCmpN( const cmChar_t* s0, const cmChar_t* s1, unsigned n )
{
  if( s0 == NULL && s1 == NULL )
    return 0;

  if( s0 == NULL || s1 == NULL )
  {
    if( s0 == NULL )
      return -1;
    return 1;
  }

  return strncmp(s0,s1,n);
}


void cmTextToLower( const cmChar_t* s0, cmChar_t* s1 )
{
  if( s0 == NULL || s1==NULL )
    return;

  for(; *s0; ++s0,++s1)
    *s1 = tolower(*s0);

  *s1 = 0;
  return;
}

void cmTextToUpper( const cmChar_t* s0, cmChar_t* s1 )
{
  if( s0 == NULL || s1==NULL )
    return;

  for(; *s0; ++s0,++s1)
    *s1 = toupper(*s0);

  *s1 = 0;
  return;
}

cmChar_t* cmTextLine( cmChar_t* s, unsigned line )
{
  assert( line>0);

  unsigned i;

  // count down to the code line containing the tag reference
  for(i=0; i<line-1; ++i)
  {
    s = strchr(s,'\n');
    
    if( s == NULL )
      return NULL;
    
    ++s;
  }

  return s+1;
}

const cmChar_t* cmTextLineC( const cmChar_t* s, unsigned line )
{ return cmTextLine((cmChar_t*)s,line); }

unsigned cmTextLineCount( const cmChar_t* s )
{
  unsigned n = *s ? 1 : 0;
  
  while( *s )
  {
    s = cmTextEndOfLineC(s);

    switch( *s )
    {
      case 0:
        break;
        
      case '\n':
        s += 1;
        n += 1;
        break;
        
      default:
        { assert(0); }
    }
    
  }

  return n;
  
}


cmChar_t* cmTextRemoveConsecutiveSpaces( cmChar_t* s )
{
  if( s==NULL || strlen(s) < 2 )
    return s;

  int i=1;
  while( s[i] )
  {
    if( isspace(s[i-1]) && isspace(s[i]) )
      cmTextShrinkS(s, s+i, 1 );
    else
      ++i;
  }

  return s;
}

cmChar_t* cmTextColumize( cmChar_t* s,  unsigned colCnt )
{
  if( s==NULL || strlen(s) < colCnt )
    return s;

  int       i  = 0;
  int       c  = 0;
  cmChar_t* c0 = NULL;

  for(; s[i]; ++i)
  {
    // remove any existing newlines 
    if( s[i] == '\n' )
      s[i] = ' ';

    // track the last space (which is a potential wrap point).
    if( isspace(s[i]) )
      c0 = s+i;

    if( c < colCnt )
      ++c;
    else
    {

      // if there is no previous wrap point ...
      if( c0 == NULL )
      {
        // ... then insert one
        s = cmTextInsertS(s,s+i,"\n");
      }
      else
      {
        // replace the wrap point with a '\n'
        *c0 = '\n';
      }

      c  = 0;
      c0 = NULL;
    }
  }

  return s;
}

cmChar_t* _cmTextPrefixRows( cmChar_t* s,  const cmChar_t* t )
{
  if( s==NULL || t==NULL || strlen(t)==0 )
    return s;

  int i;

  for(i=0; s[i]; ++i)
    if( i==0 || s[i]=='\n')
    {
      cmChar_t* u = s + (i==0 ?  0 : i+1);
      s = cmTextInsertS(s,u,t);
    }
  return s;
}

cmChar_t* cmTextIndentRows( cmChar_t* s,  unsigned indent )
{
  if( s==NULL || indent==0 )
    return s;

  cmChar_t* t = cmMemAllocZ( cmChar_t, indent+1 );
  cmVOC_Fill(t,indent,' ');
  t[indent] = 0;

  s =  _cmTextPrefixRows(s,t);

  cmMemFree(t);
  return s;
}

cmChar_t* cmTextPrefixRows( cmChar_t* s, const cmChar_t* t )
{ return _cmTextPrefixRows(s,t); }


cmChar_t* cmTextTrimRows( cmChar_t* s )
{
  bool fl = true;
  int i = 0;
  while( s[i] )
  {
    if( s[i] == '\n' )
    {
      fl = true;
      ++i;
    }
    else
    {
      if( isspace(s[i]) && fl )
        cmTextShrinkS(s, s+i, 1 );
      else
      {
        fl = false;
        ++i;
      }
    }  
  }

  return s;  
}


cmChar_t* cmTextEatLeadingSpace( cmChar_t* s )
{
  if( s == NULL )
    return s;

  while( *s )
  {
    if( !isspace(*s) )
      break;
    
    cmTextShrinkS(s,s,1);
  }

  return s;
}

cmChar_t*       cmTextNextRow(  cmChar_t* s )
{
  if( s == NULL)
    return NULL;

  for(; *s; ++s)
    if( *s == '\n' )
    {
      ++s;
      return *s==0 ? NULL : s;
    }

  return NULL;
}

const cmChar_t* cmTextNextRowC( const cmChar_t* s )
{ return cmTextNextRow((cmChar_t*)s); }
  
unsigned  cmTextMinIndent( const cmChar_t* s )
{
  // leadFl=true if at beginning of row
  bool     leadFl     = true;   
  unsigned min_indent = INT_MAX;
  unsigned indent     = 0;
  for(; *s; ++s)
  {
    if( leadFl )
    {
      if( isspace(*s) && *s!='\n' )
        indent += 1;
      else
      {
        if( indent < min_indent )
          min_indent = indent;

        indent = 0;
        leadFl = false;
      }
    }
    else
    {
      if( *s == '\n' )
        leadFl = true;
    }
      
  }

  return min_indent==INT_MAX ? 0 : min_indent;
}

cmChar_t*       cmTextOutdent( cmChar_t* s, unsigned outdent )
{
  // leadFl=true if at beginning of row
  bool      leadFl = true;   
  unsigned  indent = 0;
  cmChar_t* cs     = s;
  cmChar_t* s0     = s;

  for(; *cs; ++cs)
  {
    if( leadFl )
    {
      if( isspace(*cs) && *cs!='\n' )
        indent += 1;
      else
      {
        unsigned n  = cmMin(outdent,indent);
        cmTextShrinkS(s,s0,n);
        cs         -= n;
        
        indent = 0;
        leadFl = false;
      }
    }
    else
    {
      if( *cs == '\n' )
      {
        leadFl = true;
        s0     = cs + 1;
      }
    }
      
  }

  return s;
     
}

unsigned cmTextDecodeBase64BufferByteCount( const char* xV, unsigned xN )
{
  if( xN % 4 != 0 )
    return cmInvalidCnt;

  unsigned yN = xN / 4 * 3;

  if( xV[xN-1] == '=' )
    yN -= 1;

  if( xV[xN-2] == '=' )
    yN -= 2;

  return yN;  
}

cmTxRC_t cmTextDecodeBase64( const char* xV, unsigned xN, void* yV, unsigned yN )
{
  int t[] =
  {
   64, //  0
   64, //  1
   64, //  2
   64, //  3
   64, //  4
   64, //  5
   64, //  6
   64, //  7
   64, //  8
   64, //  9
   64, // 10
   64, // 11
   64, // 12
   64, // 13
   64, // 14
   64, // 15
   64, // 16
   64, // 17
   64, // 18
   64, // 19
   64, // 20
   64, // 21
   64, // 22
   64, // 23
   64, // 24
   64, // 25
   64, // 26
   64, // 27
   64, // 28
   64, // 29
   64, // 30
   64, // 31
   64, // 32
   64, // 33
   64, // 34
   64, // 35
   64, // 36
   64, // 37
   64, // 38
   64, // 39
   64, // 40
   64, // 41
   64, // 42
   62, // 43 +
   64, // 44
   64, // 45
   64, // 46
   63, // 47 /
   52, // 48 0
   53, // 49 1
   54, // 50 2
   55, // 51 3
   56, // 52 4
   57, // 53 5
   58, // 54 6
   59, // 55 7
   60, // 56 8
   61, // 57 9
   64, // 58
   64, // 59
   64, // 60
   64, // 61
   64, // 62
   64, // 63
   64, // 64
    0, // 65 A
    1, // 66 B
    2, // 67 C
    3, // 68 D
    4, // 69 E
    5, // 70 F
    6, // 71 G
    7, // 72 H
    8, // 73 I
    9, // 74 J
   10, // 75 K
   11, // 76 L
   12, // 77 M
   13, // 78 N
   14, // 79 O
   15, // 80 P
   16, // 81 Q
   17, // 82 R
   18, // 83 S
   19, // 84 T
   20, // 85 U
   21, // 86 V
   22, // 87 W
   23, // 88 X
   24, // 89 Y
   25, // 90 Z
   64, // 91
   64, // 92
   64, // 93
   64, // 94
   64, // 95
   64, // 96
   26, // 97 a
   27, // 98 b
   28, // 99 c
   29, //100 d
   30, //101 e
   31, //102 f
   32, //103 g
   33, //104 h
   34, //105 i
   35, //106 j
   36, //107 k
   37, //108 l
   38, //109 m
   39, //110 n
   40, //111 o
   41, //112 p
   42, //113 q
   43, //114 r
   44, //115 s
   45, //116 t
   46, //117 u 
   47, //118 v
   48, //119 w
   49, //120 x
   50, //121 y
   51, //122 z
   64, //123
   64, //124
   64, //125
   64, //126
   64  //127    
  };
  
  unsigned i  = 0;
  unsigned j  = 0;
  char*    zV = (char*)yV;

  while( i < xN )
  {
    unsigned yn = 3;
    
    if( xV[i+3] == '=' )
      --yn;
    
    if( xV[i+2] == '=' )
      --yn;

    unsigned v = 0;

    assert( i + 4 <= xN );
    
    v += t[(int)xV[i++]] << 18;
    v += t[(int)xV[i++]] << 12;
    v += t[(int)xV[i++]] <<  6;
    v += t[(int)xV[i++]] <<  0;

    if( j >= yN )
      break;
    
    zV[j++] = (v & 0xff0000) >> 16;

    if( yn > 1 )
    {
      if( j >= yN )
        break;
      
      zV[j++] = (v & 0x00ff00) >> 8;
    }
    
    if( yn > 2 )
    {
      if( j >= yN )
        break;
      
      zV[j++] = (v & 0x0000ff) >> 0;
    }
    
  }

  return j;
}


unsigned cmTextEncodeBase64BufferByteCount( unsigned binByteCnt )
{
  int rem = binByteCnt % 3;
  binByteCnt -= rem;

  int n = binByteCnt / 3 * 4;

  if( rem )
    n += 4;

  return n;
  
}

unsigned cmTextEncodeBase64( const void* xV, unsigned xN, char* yV, unsigned yN )
{
  const char*  t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const char* zV = (const char*)xV;
  unsigned     i = 0;
  unsigned     j = 0;
  
  while( 1 )
  {
    unsigned k = 3;
    unsigned v = ((int)zV[i++]) << 16;

    if( i < xN )
      v += ((int)zV[i++]) << 8;
    else
      --k;

    if( i < xN )
      v += ((int)zV[i++]);
    else
      --k;

    if( j >= yN )
      break;
    
    yV[j++] = t[ (v & 0xfc0000) >> 18 ];

    if( j >= yN )
      break;
    
    yV[j++] = t[ (v & 0x03f000) >> 12 ];

    if( j >= yN )
      break;
    
    if( k > 1 )
      yV[j++] = t[ (v & 0x000fc0) >>  6 ];
    else
      yV[j++] = '=';

    if( j >= yN )
      break;
    
    if( k > 2 )
      yV[j++] = t[ (v & 0x00003f) >>  0 ];
    else
      yV[j++] = '=';
    
  }
  
  return j;
}
