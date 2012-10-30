#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmFileSys.h"

#include "cmLib.h"

#ifdef OS_LINUX

#include <dlfcn.h>

typedef void* _libH_t;

bool _cmLibIsNull( _libH_t lh )
{ return lh == NULL; };

const cmChar_t* _cmLibSysError()
{
  const char* msg = dlerror();
  if( msg == NULL )
    msg = "<none>";
  return msg;
}

_libH_t  _cmLibOpen( const char* libFn )
{ return dlopen(libFn,RTLD_LAZY); }

bool _cmLibClose( _libH_t* lH )
{
  if( *lH != NULL )
  {
    if( dlclose(*lH) == 0 )
      *lH = NULL;
    else
      return false;
  }

  return true;
}

void* _cmLibSym( _libH_t h, const char* symLabel )
{ return dlsym(h,symLabel); }


#endif


typedef struct cmLibNode_str
{
  cmChar_t*             fn;
  unsigned              id;
  _libH_t               lH;
  struct cmLibNode_str* link;
} cmLibNode_t;

typedef struct
{
  cmErr_t      err;
  cmLibNode_t* nodes;
  unsigned     id;
  cmFileSysH_t fsH;
} cmLib_t;

cmLib_t* _cmLibHandleToPtr( cmLibH_t h )
{
  cmLib_t* p = (cmLib_t*)h.h;
  return p;
}

cmLibH_t cmLibNullHandle = cmSTATIC_NULL_HANDLE;


cmLibRC_t _cmLibNodeFree( cmLib_t* p, cmLibNode_t* np )
{
  if( !_cmLibClose( &np->lH ) )
    return cmErrMsg(&p->err,kCloseFailLibRC,"Library close failed. System Message: %s", _cmLibSysError());

  // free np->fn and set np->fn to NULL - so the node may be reused.
  cmMemPtrFree(&np->fn);
  np->id = cmInvalidId;
 
  return kOkLibRC;
}



cmLibRC_t _cmLibFinalize( cmLib_t* p )
{
  cmLibNode_t* np = p->nodes;

  while( np != NULL )
  {
    cmLibNode_t* pp = np->link;
    _cmLibNodeFree(p,np);
    cmMemFree(np);
    np = pp;
  }

  if( cmFileSysIsValid(p->fsH) )
    cmFileSysFinalize(&p->fsH);

  cmMemFree(p);

  return kOkLibRC;
}

cmLibRC_t cmLibInitialize(  cmCtx_t* ctx, cmLibH_t* hp, const cmChar_t* dirStr )
{
  cmLibRC_t rc = kOkLibRC;
  cmLib_t*  p  = cmMemAllocZ(cmLib_t,1);

  cmErrSetup(&p->err,&ctx->rpt,"cmLib");

  cmFileSysInitialize(&p->fsH,ctx,"cmLibFs");

  hp->h = p;

  if( dirStr != NULL )
    if((rc = cmLibScan(*hp,dirStr)) != kOkLibRC )
      hp->h = NULL;

  if( rc != kOkLibRC )
    _cmLibFinalize(p);

  return rc;
}


cmLibRC_t cmLibFinalize( cmLibH_t* hp )
{
  cmLibRC_t rc;

  if( hp == NULL || hp->h == NULL )
    return kOkLibRC;

  cmLib_t* p = _cmLibHandleToPtr(*hp);

  if((rc = _cmLibFinalize(p)) == kOkLibRC )
    hp->h = NULL;

  return rc;
}

bool      cmLibIsValid( cmLibH_t h )
{ return h.h != NULL; }


unsigned  cmLibOpen(  cmLibH_t h, const cmChar_t* libFn )
{
  cmLib_t*     p   = _cmLibHandleToPtr(h);  
  _libH_t      lH  = _cmLibOpen(libFn);
  cmLibNode_t* np  = p->nodes;
  unsigned     idx = 0;

  if( _cmLibIsNull(lH) )
  {
    cmErrMsg(&p->err,kOpenFailLibRC,"Library load failed. System Message: %s", _cmLibSysError() );
    return cmInvalidId;
  }
  
  while( np != NULL )
  {
    if( np->fn == NULL )
      break;

    np = np->link;
    ++idx;
  }

  if( np == NULL )
  {
    np = cmMemAllocZ(cmLibNode_t,1);
    np->link = p->nodes;
    p->nodes = np;
  }

  np->fn = cmMemAllocStr(libFn);
  np->lH = lH;
  np->id = p->id++;

  return idx;  
}

cmLibNode_t* _cmLibIdToNode( cmLib_t* p, unsigned libId )
{
  cmLibNode_t* np  = p->nodes;

  while( np != NULL )
  {
    if( np->id == libId )
      return np;

    np = np->link;
  }

  return NULL;
}

cmLibRC_t cmLibClose( cmLibH_t h, unsigned libId )
{
  cmLib_t*     p   = _cmLibHandleToPtr(h);  
  cmLibNode_t* np  = _cmLibIdToNode(p,libId);
  
  if( (np == NULL) || _cmLibIsNull(np->lH) )
    return cmErrMsg(&p->err,kInvalidIdLibRC,"The library id %i is not valid or the library is closed.",libId);

  
  return kOkLibRC;
}

void*     cmLibSym( cmLibH_t h, unsigned libId, const cmChar_t* funcStr )
{
  void*        f;
  cmLib_t*     p  = _cmLibHandleToPtr(h);  
  cmLibNode_t* np = _cmLibIdToNode(p,libId);
  
  if( (np == NULL) || _cmLibIsNull(np->lH) )
  {
     cmErrMsg(&p->err,kInvalidIdLibRC,"The library id %i is not valid or the library is closed.",libId);
     return NULL;
  }

  if((f = _cmLibSym(np->lH,funcStr)) == NULL)
  {
    cmErrMsg(&p->err,kSymFailLibRC,"The dynamic symbol '%s' was not found. System Message: %s", cmStringNullGuard(funcStr), _cmLibSysError());
    return NULL;
  }

  return f;
}


cmLibRC_t cmLibScan( cmLibH_t h, const cmChar_t* dirStr )
{
  cmLib_t*             p           = _cmLibHandleToPtr(h);  
  unsigned             dirEntryCnt = 0;
  cmFileSysDirEntry_t* d           = NULL;
  cmLibRC_t            rc          = kOkLibRC;
  unsigned             i           = 0;

  if( cmFileSysIsValid(p->fsH) == false )
    return cmErrMsg(&p->err,kFileSysFailLibRC,"The file system object was not successfully initialized.");

  if((d = cmFileSysDirEntries(p->fsH, dirStr, kFileFsFl, &dirEntryCnt )) != NULL )
    return cmErrMsg(&p->err,kFileSysFailLibRC,"The scan of directory '%s' failed.",cmStringNullGuard(dirStr));

  for(i=0; i<dirEntryCnt; ++i)
    cmLibOpen(h,d[i].name);

  cmFileSysDirFreeEntries(p->fsH,d);
  
  return rc;
}

unsigned  cmLibCount( cmLibH_t h )
{
  cmLib_t*     p   = _cmLibHandleToPtr(h);  
  cmLibNode_t* np  = p->nodes;
  unsigned     n   = 0;

  while( np != NULL )
  {
    np = np->link;
    ++n;
  }
  return n;
}

unsigned  cmLibIndexToId( cmLibH_t h, unsigned idx )
{
  cmLib_t*     p   = _cmLibHandleToPtr(h);  
  cmLibNode_t* np  = p->nodes;
  unsigned     i   = 0;

  while( np != NULL )
  {
    if( i == idx )
      return np->id;

    np = np->link;
    ++i;
  }

  if(np == NULL )
  {
     cmErrMsg(&p->err,kInvalidIdLibRC,"The library index %i is not valid.",idx);
     return cmInvalidId;
  }

  return np->id;
}

const cmChar_t* cmLibName( cmLibH_t h, unsigned libId )
{
  cmLib_t*     p  = _cmLibHandleToPtr(h);  
  cmLibNode_t* np = _cmLibIdToNode(p,libId);

  if( (np == NULL) || _cmLibIsNull(np->lH) )
  {
     cmErrMsg(&p->err,kInvalidIdLibRC,"The library id %i is not valid or the library is closed.",libId);
     return NULL;
  }
  
  return np->fn;
}
