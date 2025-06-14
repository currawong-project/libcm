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
#include "cmFileSys.h"
#include "cmText.h"

#include <sys/stat.h>
#include <errno.h>
#include <libgen.h> // basename(), dirname()
#include <dirent.h> // opendir()/readdir()
#include <limits.h> // PATH_MAX
#include <sys/types.h> // mkdir

#ifdef OS_OSX
#include "osx/cmFileSysOsx.h"
#endif

#ifdef OS_LINUX
#include "linux/cmFileSysLinux.h"
#endif

cmFileSysH_t cmFileSysNullHandle = {NULL};

typedef struct
{
  cmErr_t    err;
  cmLHeapH_t heapH;
  const cmChar_t* appNameStr;
#ifdef OS_OSX
  _cmFsOsx_t* p;
  cmChar_t* prefDir;
#endif
#ifdef OS_LINUX
  _cmFsLinux_t* p;
#endif
} cmFs_t;

cmFsRC_t _cmFileSysError( cmFs_t* p, cmFsRC_t rc, int sysErr, const cmChar_t* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  
  if( sysErr == 0 )
    rc = cmErrVMsg(&p->err,rc,fmt,vl);
  else
  {
    const unsigned bufCharCnt = 511;
    cmChar_t buf[bufCharCnt+1];
    vsnprintf(buf,bufCharCnt,fmt,vl);
    rc = cmErrMsg(&p->err,rc,"%s\nSystem Msg:%s",buf,strerror(sysErr));
  }

  va_end(vl);
  return rc;
}


cmFs_t* _cmFileSysHandleToPtr( cmFileSysH_t h )
{
  cmFs_t* p = (cmFs_t*)h.h;
  assert( p != NULL);
  return p;
}

cmFsRC_t _cmFileSysFinalize( cmFs_t* p )
{
  cmFsRC_t rc = kOkFsRC;

  if( cmLHeapIsValid(p->heapH) )
    cmLHeapDestroy(&p->heapH);

#ifdef OS_OSX
  if( p->p != NULL )
    if((rc = _cmOsxFileSysFinalize(p->p) ) != kOkFsRC )
    {
      _cmFileSysError(p,kOsxFailFsRC,0,"The OSX file system finalization failed.");
      return rc;
    }
#endif

#ifdef OS_LINUX
  if( p->p != NULL )
    if((rc = _cmLinuxFileSysFinalize(p->p) ) != kOkFsRC )
    {
      _cmFileSysError(p,kLinuxFailFsRC,0,"The Linux file system finalization failed.");
      return rc;
    }
#endif

  cmMemPtrFree(&p);

  return rc;
}
 
cmFsRC_t cmFileSysInitialize( cmFileSysH_t* hp, cmCtx_t* ctx, const cmChar_t* appNameStr )
{
  cmFs_t* p;
  cmFsRC_t rc;
  cmErr_t err;

  if((rc = cmFileSysFinalize(hp)) != kOkFsRC )
    return rc;

  cmErrSetup(&err,&ctx->rpt,"File System");

  if((p = cmMemAllocZ( cmFs_t, 1 )) == NULL )
    return cmErrMsg(&err,kMemAllocErrFsRC,"Unable to allocate the file system object.");

  cmErrClone(&p->err,&err);

  if(cmLHeapIsValid( p->heapH = cmLHeapCreate(1024,ctx)) == false )
  {
    rc =  _cmFileSysError(p,kLHeapAllocErrFsRC,0,"Unable to allocate the linked heap.");
    goto errLabel;
  }

  p->appNameStr =  cmLhAllocStr(p->heapH,appNameStr);

  hp->h = p;

#ifdef OS_OSX
  if( (rc = _cmOsxFileSysInit(&p->p, p->heapH, &p->err)) != kOkFsRC )
  {
    rc = _cmFileSysError(p,kOsxFailFsRC,0,"OSX file system initialization failed.");
    goto errLabel;
  }

  const cmChar_t* dir = cmFsMakeFn(p->p->prefDir,appNameStr,NULL,NULL);

  // BUG?
  // we reuse p->p->prefDir here because the one returned by the platform
  // specific code is never released  ... which isn't quite right either
  // See osx/cmFileSysOsx.c.
  p->p->prefDir = cmLhAllocStr(p->heapH,dir);
  cmFsFreeFn(dir);
#endif

#ifdef OS_LINUX
  if( (rc = _cmLinuxFileSysInit(&p->p, p->heapH, &p->err)) != kOkFsRC )
  {
    rc = _cmFileSysError(p,kLinuxFailFsRC,0,"Linux file system initialization failed.");
    goto errLabel;
  }
  else
  {
#endif


#ifdef OS_LINUX

  cmChar_t hidAppNameStr[ strlen(appNameStr) + 2 ];

  strcpy(hidAppNameStr,".");
  strcat(hidAppNameStr,appNameStr);

  p->p->prefDir = cmFileSysMakeFn( *hp, p->p->prefDir, hidAppNameStr, NULL, NULL  );

  // the resource directory must exist before the program can start
  p->p->rsrcDir = cmFileSysMakeFn( *hp, p->p->rsrcDir, appNameStr, NULL, NULL );
  }
#endif

 errLabel:
  if( rc != kOkFsRC )
    return _cmFileSysFinalize(p);

  return kOkFsRC;
}

cmFsRC_t cmFileSysFinalize(   cmFileSysH_t* hp )
{
  cmFsRC_t rc;

  if( hp==NULL || cmFileSysIsValid(*hp) == false )
    return kOkFsRC;

  cmFs_t* p = _cmFileSysHandleToPtr(*hp);

  if((rc = _cmFileSysFinalize(p)) != kOkFsRC )
    return rc;

  hp->h = NULL;

  return rc;
}

const cmChar_t* cmFileSysAppName( cmFileSysH_t h )
{
  cmFs_t* p = _cmFileSysHandleToPtr(h);
  return p->appNameStr;
}

const cmChar_t* cmFileSysPrefsDir( cmFileSysH_t h )
{
  cmFs_t* p = _cmFileSysHandleToPtr(h);
#if defined OS_OSX || defined OS_LINUX
  return p->p->prefDir;
#else
  return NULL;
#endif
}

const cmChar_t* cmFileSysRsrcDir( cmFileSysH_t h )
{
  cmFs_t* p = _cmFileSysHandleToPtr(h);
#if defined OS_OSX || defined OS_LINUX
  return p->p->rsrcDir;
#else
  return NULL;
#endif
}

const cmChar_t* cmFileSysUserDir( cmFileSysH_t h )
{
  cmFs_t* p = _cmFileSysHandleToPtr(h);
#if defined OS_OSX || defined OS_LINUX
  return p->p->userDir;
#else
  return NULL;
#endif
}


bool     cmFileSysIsValid(    cmFileSysH_t h )
{ return h.h != NULL; }

bool _cmFileSysIsDir( cmFs_t* p, const cmChar_t* dirStr )
{
  struct stat s;

  errno = 0;

  if( stat(dirStr,&s)  != 0 )
  {
    // if the dir does not exist
    if( errno == ENOENT )
      return false;

    _cmFileSysError( p, kStatFailFsRC, errno, "'stat' failed on '%s'",dirStr);
    return false;
  }
 
  return S_ISDIR(s.st_mode);
}

bool cmFileSysCanWriteToDir( cmFileSysH_t h, const cmChar_t* dirStr )
{
  cmFs_t*     p = _cmFileSysHandleToPtr(h);
  int result;

  errno = 0;

  if((result = access(dirStr,W_OK)) == 0 )
    return true;

  if( result == EACCES || result==EROFS )
    return false;

  _cmFileSysError( p, kAccessFailFsRC, errno, "'access' failed on '%s'.",dirStr);

  return false;  
}

bool cmFileSysIsDir( cmFileSysH_t h, const cmChar_t* dirStr )
{
  cmFs_t*     p = _cmFileSysHandleToPtr(h);
  return _cmFileSysIsDir(p,dirStr);
}

bool _cmFileSysIsFile( cmFs_t* p, const cmChar_t* fnStr )
{
  struct stat s;
  errno = 0;

  if( stat(fnStr,&s)  != 0 )
  {

    // if the file does not exist
    if( errno == ENOENT )
      return false;

    _cmFileSysError( p, kStatFailFsRC, errno, "'stat' failed on '%s'.",fnStr);
    return false;
  }
 
  return S_ISREG(s.st_mode);
}

bool cmFileSysIsFile( cmFileSysH_t h, const cmChar_t* fnStr )
{
  cmFs_t*     p = _cmFileSysHandleToPtr(h);

  return _cmFileSysIsFile(p,fnStr);
}

bool _cmFileSysIsLink( cmFs_t* p, const cmChar_t* fnStr )
{
  struct stat s;
  errno = 0;

  if( lstat(fnStr,&s)  != 0 )
  {

    // if the file does not exist
    if( errno == ENOENT )
      return false;

    _cmFileSysError( p, kStatFailFsRC, errno, "'stat' failed on '%s'.",fnStr);
    return false;
  }
 
  return S_ISLNK(s.st_mode);
}

bool cmFileSysIsLink( cmFileSysH_t h, const cmChar_t* fnStr )
{
  cmFs_t*     p = _cmFileSysHandleToPtr(h);

  return _cmFileSysIsLink(p,fnStr);
}

bool _cmFileSysIsSocket( cmFs_t* p, const cmChar_t* fnStr )
{
  struct stat s;
  errno = 0;

  if( stat(fnStr,&s)  != 0 )
  {

    // if the file does not exist
    if( errno == ENOENT )
      return false;

    _cmFileSysError( p, kStatFailFsRC, errno, "'stat' failed on '%s'.",fnStr);
    return false;
  }
 
  return S_ISSOCK(s.st_mode);
}


bool _cmFileSysConcat( cmChar_t* rp, unsigned rn, char sepChar, const cmChar_t* suffixStr )
{
  unsigned m = strlen(rp);

  // m==0 if no sep needs to be inserted or removed

  //if( m == 0 )
  //  return false;

  if( m != 0 )
  {

    // if a sep char needs to be inserted
    if( rp[m-1] != sepChar && suffixStr[0] != sepChar )
    {
      assert((m+1)<rn);

      if((m+1)>=rn)
        return false;

      rp[m]  = sepChar;
      rp[m+1]= 0;
      ++m;
    }  
    else
      // if a sep char needs to be removed
      if( rp[m-1] == sepChar && suffixStr[0] == sepChar )
      {
        rp[m-1] = 0;
        --m;
      }

  }

  assert( rn>=m && strlen(rp)+strlen(suffixStr) <= rn );
  strncat(rp,suffixStr,rn-m);

  return true;
}

const cmChar_t* cmFileSysVMakeFn( cmFileSysH_t h, const cmChar_t* dir, const cmChar_t* fn, const cmChar_t* ext, va_list vl )
{
  cmFsRC_t        rc      = kOkFsRC;
  cmChar_t*       rp      = NULL;
  const cmChar_t* dp      = NULL;
  unsigned        n       = 0;
  cmFs_t*         p       = _cmFileSysHandleToPtr(h);
  char            pathSep = cmPathSeparatorChar[0];
  char            extSep  = '.';
  va_list         vl_t;
  va_copy(vl_t,vl);

  // get prefix directory length
  if( dir != NULL )
    n += strlen(dir) + 1;  // add 1 for ending sep

  // get file name length
  if( fn != NULL )
    n += strlen(fn);

  // get extension length
  if( ext != NULL )
    n += strlen(ext) + 1;  // add 1 for period

  // get length of all var args dir's
  while( (dp = va_arg(vl,const cmChar_t*)) != NULL )
    n += strlen(dp) + 1;  // add 1 for ending sep

   // add 1 for terminating zero and allocate memory

  if((rp = cmLHeapAllocZ( p->heapH, n+1 )) == NULL )
  {
    rc = _cmFileSysError(p,kMemAllocErrFsRC,0,"Unable to allocate file name memory.");
    goto errLabel;
  }

  va_copy(vl,vl_t);

  rp[n] = 0;
  rp[0] = 0;

  // copy out the prefix dir
  if( dir != NULL )
    strncat(rp,dir,n-strlen(rp));

  // copy out each of the var arg's directories
  while((dp = va_arg(vl,const cmChar_t*)) != NULL )
    if(!_cmFileSysConcat(rp,n,pathSep,dp) )
    {
      assert(0);
      rc = _cmFileSysError(p,kAssertFailFsRC,0,"Assert failed.");
      goto errLabel;
    }


  // copy out the file name
  if( fn != NULL )
    if(!_cmFileSysConcat(rp,n,pathSep,fn))
    {
      assert(0);
      rc = _cmFileSysError(p,kAssertFailFsRC,0,"Assert failed.");
      goto errLabel;
    }
  
  // copy out the extension
  if( ext != NULL )
    if(!_cmFileSysConcat(rp,n,extSep,ext))
    {
      assert(0);
      rc = _cmFileSysError(p,kAssertFailFsRC,0,"Assert failed.");
      goto errLabel;
    }

  assert(strlen(rp)<=n);
  
 errLabel:

  if( rc != kOkFsRC && rp != NULL )
    cmLHeapFree(p->heapH, rp );

  return rp;
}

const cmChar_t* cmFileSysVMakeUserFn( cmFileSysH_t h, const cmChar_t* dirPrefix, const cmChar_t* fn, const cmChar_t* ext, va_list vl )
{
  return cmFileSysMakeFn(h,cmFileSysUserDir(h),fn,ext,dirPrefix,NULL);
}

const cmChar_t* cmFileSysMakeFn( cmFileSysH_t h, const cmChar_t* dir, const cmChar_t* fn, const cmChar_t* ext, ... )
{
  va_list vl;
  va_start(vl,ext);
  const cmChar_t* retPtr = cmFileSysVMakeFn(h,dir,fn,ext,vl);
  va_end(vl);
  return retPtr;
}

const cmChar_t* cmFileSysMakeUserFn( cmFileSysH_t h, const cmChar_t* dir, const cmChar_t* fn, const cmChar_t* ext, ... )
{
  va_list vl;
  va_start(vl,ext);
  const cmChar_t* retPtr = cmFileSysVMakeUserFn(h,dir,fn,ext,vl);
  va_end(vl);
  return retPtr;
}


const cmChar_t* cmFileSysVMakeDir(     cmFileSysH_t h, const cmChar_t* dir,  va_list vl )
{ return cmFileSysVMakeFn(h,dir,NULL,NULL,vl); }

const cmChar_t* cmFileSysMakeDir(      cmFileSysH_t h, const cmChar_t* dir,  ... )
{
  va_list vl;
  va_start(vl,dir);
  const cmChar_t* retPtr = cmFileSysVMakeFn(h,dir,NULL,NULL,vl);
  va_end(vl);
  return retPtr;
}

const cmChar_t* cmFileSysVMakeUserDir( cmFileSysH_t h, const cmChar_t* dir,  va_list vl )
{ return cmFileSysVMakeUserFn(h,dir,NULL,NULL,vl);  }

const cmChar_t* cmFileSysMakeUserDir(  cmFileSysH_t h, const cmChar_t* dir,  ... )
{
  va_list vl;
  va_start(vl,dir);
  const cmChar_t* retPtr = cmFileSysVMakeUserFn(h,dir,NULL,NULL,vl);
  va_end(vl);
  return retPtr;
}

const cmChar_t* cmFileSysMakeDirFn(    cmFileSysH_t h, const cmChar_t* dir, const cmChar_t* fn )
{ return cmFileSysMakeFn( h, dir, fn, NULL, NULL); }

const cmChar_t* cmFileSysMakeUserDirFn(cmFileSysH_t h, const cmChar_t* dir, const cmChar_t* fn )
{ return cmFileSysMakeUserFn(h, dir, fn, NULL, NULL ); }
    
void cmFileSysFreeFn( cmFileSysH_t h, const cmChar_t* fn )
{
  cmFs_t* p = _cmFileSysHandleToPtr(h);

  if( fn == NULL )
    return;

  cmLHeapFree(p->heapH, (void*)fn); 
}

cmFsRC_t cmFileSysGenFn( cmFileSysH_t h, const cmChar_t* dir, const cmChar_t* prefixStr, const cmChar_t* extStr, const cmChar_t** fnPtr )
{
  cmFsRC_t rc            = kOkFsRC;
  cmFs_t*  p             = _cmFileSysHandleToPtr(h);
  unsigned maxAttemptCnt = 0xffff;

  *fnPtr = NULL;

  assert(dir != NULL);

  if( prefixStr == NULL )
    prefixStr = "";

  if( extStr == NULL )
    extStr = "";

  if( !cmFileSysIsDir(h,dir) )
    return cmErrMsg(&p->err,kOpenDirFailFsRC,"File name generation failed because the directory '%s' does not exist.",cmStringNullGuard(dir));

  unsigned i;
  for(i=0; *fnPtr==NULL; ++i)
  {
    cmChar_t* fn = cmTsPrintfP(NULL,"%s%i",prefixStr,i);

    const cmChar_t* path = cmFileSysMakeFn(h,dir,fn,extStr,NULL );
    
    if( !cmFileSysIsFile(h,path) )
      *fnPtr = cmMemAllocStr(path);

    cmFileSysFreeFn(h,path);
    cmMemFree(fn);

    if( i == maxAttemptCnt )
      return cmErrMsg(&p->err,kGenFileFailFsRC,"File name generation failed because a suitable file name could not be found after %i attempts.",maxAttemptCnt);
  };

  return rc;
}

cmFsRC_t    cmFileSysMkDir( cmFileSysH_t h, const cmChar_t* dir )
{
  cmFs_t* p = _cmFileSysHandleToPtr(h);

  if( mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0 )
    return _cmFileSysError( p, kMkDirFailFsRC, errno, "The attempt to create the directory '%s' failed.",dir);

  return kOkFsRC;
}

cmFsRC_t    cmFileSysMkUserDir( cmFileSysH_t h, const cmChar_t* dir0 )
{
  const cmChar_t* dir = cmFileSysMakeUserFn(h,dir0,NULL,NULL,NULL);
  
  if( dir == NULL )
    return _cmFileSysError(_cmFileSysHandleToPtr(h),kFormFnFailFsRC,0, "The specified directory string /<user>/%s could not be formed.",cmStringNullGuard(dir0));
  
  cmFsRC_t rc = cmFileSysMkDir(h,dir);
  cmFileSysFreeFn(h,dir);
  return rc;
}


cmFsRC_t    cmFileSysMkDirAll( cmFileSysH_t h, const cmChar_t* dir )
{
  cmFsRC_t   rc = kOkFsRC;
  cmFs_t*    p  = _cmFileSysHandleToPtr(h);
  cmChar_t** a  = NULL;
  unsigned   i;

  if((a = cmFileSysDirParts(h,dir)) == NULL )
    return _cmFileSysError(p, kInvalidDirFsRC, 0, "The directory '%s' could not be parsed.",cmStringNullGuard(dir));

  for(i=0; rc==kOkFsRC && a[i]!=NULL; ++i)
  {
    cmChar_t* d = cmFileSysFormDir(h,a,i+1);

    if( cmFileSysIsDir(h,d)==false )
      if((rc = cmFileSysMkDir(h,d)) != kOkFsRC )
        break;

    cmFileSysFreeDir(h,d);
  }

  cmFileSysFreeDirParts(h,a);

  return rc;
}

cmFsRC_t    cmFileSysMkUserDirAll( cmFileSysH_t h, const cmChar_t* dir0 )
{
  const cmChar_t* dir = cmFileSysMakeUserFn(h,dir0,NULL,NULL,NULL);
  
  if( dir == NULL )
    return _cmFileSysError(_cmFileSysHandleToPtr(h),kFormFnFailFsRC,0,"The specified directory string /<user>/%s could not be formed.",cmStringNullGuard(dir0));
  
  cmFsRC_t rc = cmFileSysMkDirAll(h,dir);
  cmFileSysFreeFn(h,dir);
  return rc;
}

cmFileSysPathPart_t* cmFileSysPathParts( cmFileSysH_t h, const cmChar_t* pathStr )
{

  int                  n  = 0;       // char's in pathStr
  int                  dn = 0;       // char's in the dir part
  int                  fn = 0;       // char's in the name part
  int                  en = 0;       // char's in the ext part
  cmChar_t*                cp = NULL;
  cmFileSysPathPart_t* rp = NULL;

  cmFs_t* p = _cmFileSysHandleToPtr(h);

  if( pathStr==NULL )
    return NULL;

  // skip leading white space
  while( *pathStr )
  {
    if( isspace(*pathStr ) )
      ++pathStr;
    else
      break;
  }

  // get the length of pathStr
  if((n = strlen(pathStr)) == 0 )
    return NULL;

  // remove trailing spaces
  for(; n>0; --n)
  {
    if( isspace(pathStr[n-1]) )
      --n;
    else
      break;
  }

  // 
  if( n == 0 )
    return NULL;
  

  char buf[n+1];
  buf[n] = 0;

  // Get the last word (which may be a file name) from pathStr.
  // (pathStr must be copied into a buf because basename()
  // is allowed to change the values in its arg.)
  strncpy(buf,pathStr,n);
  cp = basename(buf);
  
  if( cp != NULL )
  {
    cmChar_t* ep;
    // does the last word have a period in it
    if( (ep = strchr(cp,'.')) != NULL )
    {
      *ep = 0;         // end the file name at the period
      ++ep;            // set the ext marker
      en = strlen(ep); // get the length of the ext
    }  

    fn = strlen(cp); //get the length of the file name
  }

  // Get the directory part.
  // ( pathStr must be copied into a buf because dirname()
  // is allowed to change the values in its arg.)
  strncpy(buf,pathStr,n);
  
  // if the last char in pathStr[] is '/' then the whole string is a dir.
  // (this is not the answer that dirname() and basename() would give).
  if( pathStr[n-1] == cmPathSeparatorChar[0] )
  {
    fn = 0;
    en = 0;
    dn = n;
    cp = buf;    
  }
  else
  {    
    cp = dirname(buf);
  }


  if( cp != NULL  )
    dn = strlen(cp);

  // get the total size of the returned memory. (add 3 for ecmh possible terminating zero)
  n = sizeof(cmFileSysPathPart_t) + dn + fn + en + 3;

  // alloc memory
  if((rp = (cmFileSysPathPart_t*)cmLHeapAllocZ( p->heapH, n )) == NULL )
  {
    _cmFileSysError( p, kLHeapAllocErrFsRC, 0, "Unable to allocate the file system path part record for '%s'.",pathStr);
    return NULL;
  }
  
  // set the return pointers for ecmh of the parts
  rp->dirStr = (const cmChar_t* )(rp + 1);
  rp->fnStr  = rp->dirStr + dn + 1;
  rp->extStr = rp->fnStr  + fn + 1;


  // if there is a directory part
  if( dn>0 )
    strcpy((cmChar_t*)rp->dirStr,cp);
  else
    rp->dirStr = NULL;
  
  if( fn || en )
  {
    // Get the trailing word again.
    // pathStr must be copied into a buf because basename() may
    // is allowed to change the values in its arg.
    strncpy(buf,pathStr,n);
    cp = basename(buf);

    
    if( cp != NULL )
    {
      
      cmChar_t* ep;
      if( (ep = strchr(cp,'.')) != NULL )
      {
        *ep = 0;
        ++ep;

        assert( strlen(ep) == en );
        strcpy((cmChar_t*)rp->extStr,ep);
      }
      

      assert( strlen(cp) == fn );
      if(fn)
        strcpy((cmChar_t*)rp->fnStr,cp);
    }
  }

  if( fn == 0 )
    rp->fnStr = NULL;

  if( en == 0 )
    rp->extStr = NULL;

  return rp;
  
}

void                 cmFileSysFreePathParts( cmFileSysH_t h, cmFileSysPathPart_t* pp )
{
  if( pp == NULL )
    return;

  cmFs_t* p = _cmFileSysHandleToPtr(h);

  cmLHeapFree(p->heapH, (void*)pp );
}

cmChar_t** cmFileSysDirParts( cmFileSysH_t h, const cmChar_t* dirStr )
{
  cmFs_t*         p       = _cmFileSysHandleToPtr(h);
  const cmChar_t* s       = dirStr;
  const cmChar_t* ep      = dirStr + strlen(dirStr);
  char            pathSep = cmPathSeparatorChar[0];
  unsigned        n       = 2;
  unsigned        i       = 0;

  // skip leading white space or pathsep
  while( isspace(*s) && s < ep )
    ++s;

  // if the directory string is empty
  if( s >= ep )
    return NULL;

  // set the beginning of the input dirStr past any leading white space
  dirStr = s;

  // count the number of dir segments - this might overcount the
  // number of segments if there are multiple repeated path seperator
  // char's - but this is ok because 'n' is only used to determine
  // the size of the returned array - which will simply have some
  // NULL entries at the end.
  for(; s < ep; ++s )
    if( *s == pathSep )
      ++n;

  // allocate the array
  cmChar_t** a = cmLhAllocZ(p->heapH,cmChar_t*,n);

  // reset the cur location to the begin of the buf
  s = dirStr;

  // if the path starts at the root
  if( *s == pathSep )
  {
    a[0] = cmPathSeparatorChar; // cmPathSeparatorChar is a static string in cmGlobal.h
    i = 1;
    ++s;
  }

  for(; i<n && s<ep; ++i)
  {
    const cmChar_t* s1;
    if(( s1 = strchr(s,pathSep)) == NULL )
      s1 = ep;

    if( s1!=s )
    {
    
      unsigned sn = (s1 - s)+1;
      a[i] = cmLhAlloc(p->heapH,cmChar_t,sn);
      strncpy(a[i],s,sn-1);
      a[i][sn-1] = 0;
    }

    s = s1+1;
  }
  
  return a;  
}

void  cmFileSysFreeDirParts( cmFileSysH_t h, cmChar_t** dirStrArray )
{
  if( dirStrArray == NULL )
    return;

  cmFs_t* p = _cmFileSysHandleToPtr(h);

  unsigned i;
  for(i=0; dirStrArray[i]!=NULL; ++i)
  {
    // cmPathSeparatorChar is statically alloc'd in cmGlobal.h
    if( dirStrArray[i] != cmPathSeparatorChar )
      cmLHeapFree(p->heapH,dirStrArray[i]);
  }

  cmLHeapFree(p->heapH, (void*)dirStrArray );
}

unsigned         cmFileSysDirPartsCount(cmFileSysH_t h,  cmChar_t** dirStrArray )
{
  unsigned i = 0;
  if( dirStrArray == NULL )
    return 0;
  
  while(dirStrArray[i] != NULL )
    ++i;

  return i;
}

cmChar_t* cmFileSysFormDir( cmFileSysH_t h, cmChar_t** a, unsigned m )
{
  cmFs_t*   p = _cmFileSysHandleToPtr(h);
  unsigned  n;
  unsigned  i;

  // determine the length of the return string
  for(i=0,n=0; a[i]!=NULL && i<m; ++i)
    n += strlen(a[i]) + strlen(cmPathSeparatorChar);

  if( i<m && a[i] == NULL )
  {
    _cmFileSysError(p,kInvalidDirFsRC,0,"cmFileSysFormDir() cannot form a directory string from %i parts when only %i exist.",m,i);
     return NULL;
  }

  n += 1;

  // allocate the return string
  cmChar_t* r = cmLhAllocZ(p->heapH,cmChar_t,n);
  const cmChar_t* ep = r + n;

  // form the return string
  for(i=0; a[i]!=NULL && i<m; ++i)
  {
    strcat(r,a[i]);

    if( strcmp(a[i],cmPathSeparatorChar)!=0 && a[i+1]!=NULL )
      strcat(r,cmPathSeparatorChar);

    assert( r + strlen(r) <= ep );
  }
  
  return r;
}

void   cmFileSysFreeDir( cmFileSysH_t h, const cmChar_t* dir )
{
  if( dir == NULL )
    return;

  cmFs_t*   p = _cmFileSysHandleToPtr(h);

  cmLHeapFree(p->heapH,(void*)dir);
}


typedef struct 
{
  cmFs_t*              p;
  unsigned             filterFlags;
  cmFileSysDirEntry_t* rp;
  cmChar_t*            dataPtr;
  cmChar_t*            endPtr;
  unsigned             entryCnt;
  unsigned             entryIdx;
  unsigned             dataByteCnt;
  unsigned             passIdx;

}  cmFileSysDeRecd_t;

cmFsRC_t _cmFileSysDirGetEntries(  cmFileSysDeRecd_t* drp, const cmChar_t* dirStr )
{
  cmFsRC_t             rc          = kOkFsRC;
  DIR*                 dirp        = NULL;
  struct dirent*       dp          = NULL;
  char                 curDirPtr[] = "./";  
  unsigned             dn          = 0;


  if( dirStr == NULL || strlen(dirStr) == 0 )
    dirStr = curDirPtr;

  if( _cmFileSysIsDir(drp->p,dirStr) == false )
    return rc;

  unsigned       fnCharCnt= strlen(dirStr) + PATH_MAX;
  char           fn[ fnCharCnt + 1 ];


  // copy the directory into fn[] ...
  fn[0]          =0;
  fn[fnCharCnt] = 0;

  strcpy(fn,dirStr);

  assert( strlen(fn)+2 < fnCharCnt );
  
  // ... and be sure that it is terminated with a path sep char
  if( fn[ strlen(fn)-1 ] != cmPathSeparatorChar[0] )
    strcat(fn,cmPathSeparatorChar);

  // file names will be appended to the path at this location
  unsigned fni = strlen(fn);

  // open the directory
  if((dirp = opendir(dirStr)) == NULL)
  {
    rc = _cmFileSysError(drp->p,kOpenDirFailFsRC,errno,"Unable to open the directory:'%s'.",dirStr);

    goto errLabel;
  }

  // get the next directory entry
  while((dp = readdir(dirp)) != NULL )
  {
    // validate d_name 
    if( (dn = strlen(dp->d_name)) > 0 )
    {
      unsigned flags = 0;
        
      // handle cases where d_name begins with '.'
      if( dp->d_name[0] == '.' )
      {

        if( strcmp(dp->d_name,".") == 0 )
        {
          if( cmIsFlag(drp->filterFlags,kCurDirFsFl) == false )
            continue;

          flags |= kCurDirFsFl;
        }

        if( strcmp(dp->d_name,"..") == 0 )
        {
          if( cmIsFlag(drp->filterFlags,kParentDirFsFl) == false )
            continue;

          flags |= kParentDirFsFl;
        }

        if( flags == 0 )
        {
          if( cmIsFlag(drp->filterFlags,kInvisibleFsFl) == false )
            continue;

          flags |= kInvisibleFsFl;
        }
      }

      fn[fni] = 0;
      strncat( fn, dp->d_name, fnCharCnt-fni );
      unsigned fnN = strlen(fn);

      // if the filename is too long for the buffer
      if( fnN > fnCharCnt )
      {
        rc = _cmFileSysError(drp->p, kFnTooLongFsRC, errno, "The directory entry:'%s' was too long to be processed.",dp->d_name);
        goto errLabel;
      }

      // is a link
      if( _cmFileSysIsLink(drp->p,fn) )
      {
        if( cmIsFlag(drp->filterFlags,kLinkFsFl) == false )
          continue;

        flags |= kLinkFsFl;

        if( cmIsFlag(drp->filterFlags,kRecurseLinksFsFl) )
          if((rc = _cmFileSysDirGetEntries(drp,fn)) != kOkFsRC )
            goto errLabel;
      }
      else
      {

        // is the entry a file
        if( _cmFileSysIsFile(drp->p,fn) )
        {
          if( cmIsFlag(drp->filterFlags,kFileFsFl)==false )
            continue;

          flags |= kFileFsFl;
        }
        else
        {
          // is the entry a dir
          if( _cmFileSysIsDir(drp->p,fn) )
          {
            if( cmIsFlag(drp->filterFlags,kDirFsFl) == false)
              continue;

            flags |= kDirFsFl;

            if( cmIsFlag(drp->filterFlags,kRecurseFsFl) )
              if((rc = _cmFileSysDirGetEntries(drp,fn)) != kOkFsRC )
                goto errLabel;
          }
          else
          {
            continue;
          }
        }
      }

      //assert(flags != 0);

      if( drp->passIdx == 0 )
      {
        ++drp->entryCnt;
        
        // add 1 for the name terminating zero
        drp->dataByteCnt += sizeof(cmFileSysDirEntry_t) + 1;

        if( cmIsFlag(drp->filterFlags,kFullPathFsFl) )
          drp->dataByteCnt += fnN;
        else
          drp->dataByteCnt += dn;

      }
      else
      {
        assert( drp->passIdx == 1 );
        assert( drp->entryIdx < drp->entryCnt );

        unsigned n = 0;
        if( cmIsFlag(drp->filterFlags,kFullPathFsFl) )
        {
          n = fnN+1;
          assert( drp->dataPtr + n <= drp->endPtr );
          strcpy(drp->dataPtr,fn);
        }
        else
        {
          n = dn+1;
          assert( drp->dataPtr + n <= drp->endPtr );
          strcpy(drp->dataPtr,dp->d_name);
        }

        drp->rp[ drp->entryIdx ].flags = flags;
        drp->rp[ drp->entryIdx ].name  = drp->dataPtr;
        drp->dataPtr += n;
        assert( drp->dataPtr <= drp->endPtr);
        ++drp->entryIdx;
      }
    }  
  }

 errLabel:
  if( dirp != NULL )
    closedir(dirp);

  return rc;
}


cmFileSysDirEntry_t* cmFileSysDirEntries(  cmFileSysH_t h, const cmChar_t* dirStr, unsigned filterFlags, unsigned* dirEntryCntPtr )
{
  cmFsRC_t          rc = kOkFsRC;
  cmFileSysDeRecd_t r;

  memset(&r,0,sizeof(r));
  r.p           = _cmFileSysHandleToPtr(h);
  r.filterFlags = filterFlags;

  assert( dirEntryCntPtr != NULL );
  *dirEntryCntPtr = 0;
  
  for(r.passIdx=0; r.passIdx<2; ++r.passIdx)
  {
    if((rc = _cmFileSysDirGetEntries( &r, dirStr )) != kOkFsRC )
      goto errLabel;

    if( r.passIdx == 0 && r.dataByteCnt>0 )
    {
      // allocate memory to hold the return values
      if(( r.rp = (cmFileSysDirEntry_t *)cmLHeapAllocZ( r.p->heapH, r.dataByteCnt )) == NULL )
      {
        rc= _cmFileSysError( r.p, kMemAllocErrFsRC, 0, "Unable to allocate %i bytes of dir entry memory.",r.dataByteCnt);
        goto errLabel;
      }

      r.dataPtr = (cmChar_t*)(r.rp + r.entryCnt);
      r.endPtr  = ((cmChar_t*)r.rp) + r.dataByteCnt; 
    }
  }

 errLabel:
  
  if( rc == kOkFsRC )
  {
    assert( r.entryIdx == r.entryCnt );
    *dirEntryCntPtr = r.entryCnt;
  }
  else
  {
    if( r.rp != NULL )
      cmLHeapFree(r.p->heapH,r.rp);    

    r.rp = NULL;
  }

  return r.rp;
}

void                cmFileSysDirFreeEntries( cmFileSysH_t h, cmFileSysDirEntry_t* dp )
{
  cmFs_t* p = _cmFileSysHandleToPtr(h);
  if( dp != NULL )
    cmLHeapFree(p->heapH,dp);
}

cmFsRC_t             cmFileSysErrorCode( cmFileSysH_t h )
{
  cmFs_t* p = _cmFileSysHandleToPtr(h);
  return cmErrLastRC(&p->err);
}

//
//======================================================================================================
// Begin global versions
//

cmFileSysH_t _cmFsH = { NULL };

cmFsRC_t             cmFsInitialize( cmCtx_t* ctx, const cmChar_t* appNameStr )
{ return cmFileSysInitialize(&_cmFsH,ctx,appNameStr); }

cmFsRC_t             cmFsFinalize()
{ return cmFileSysFinalize(&_cmFsH); }

const cmChar_t*      cmFsAppName() 
{ return cmFileSysAppName(_cmFsH); }

const cmChar_t*      cmFsPrefsDir()
{ return cmFileSysPrefsDir(_cmFsH); }

const cmChar_t*      cmFsRsrcDir()
{ return cmFileSysRsrcDir(_cmFsH); }

const cmChar_t*      cmFsUserDir()
{ return cmFileSysUserDir(_cmFsH); }

bool                cmFsCanWriteToDir( const cmChar_t* dirStr )
{ return cmFileSysCanWriteToDir(_cmFsH,dirStr); }

bool                 cmFsIsDir(  const cmChar_t* dirStr )
{ return cmFileSysIsDir(_cmFsH,dirStr); }

bool                 cmFsIsFile( const cmChar_t* fnStr ) 
{ return cmFileSysIsFile(_cmFsH,fnStr); }

bool                 cmFsIsLink( const cmChar_t* fnStr ) 
{ return cmFileSysIsLink(_cmFsH,fnStr); }

const cmChar_t*      cmFsVMakeFn( const cmChar_t* dirPrefix, const cmChar_t* fn, const cmChar_t* ext, va_list vl )
{ return cmFileSysVMakeFn(_cmFsH,dirPrefix,fn,ext,vl); }

const cmChar_t*      cmFsMakeFn(  const cmChar_t* dirPrefix, const cmChar_t* fn, const cmChar_t* ext, ... )
{
  va_list vl;
  va_start(vl,ext);
  const cmChar_t* retPtr = cmFsVMakeFn(dirPrefix,fn,ext,vl);
  va_end(vl);
  return retPtr;
}

const cmChar_t* cmFsVMakeUserFn( const cmChar_t* dirPrefix, const cmChar_t* fn, const cmChar_t* ext, va_list vl )
{ return cmFileSysVMakeUserFn(_cmFsH,dirPrefix,fn,ext,vl); }

const cmChar_t* cmFsMakeUserFn(  const cmChar_t* dirPrefix, const cmChar_t* fn, const cmChar_t* ext, ... )
{
  va_list vl;
  va_start(vl,ext);
  const cmChar_t* retPtr = cmFsVMakeUserFn(dirPrefix,fn,ext,vl);
  va_end(vl);
  return retPtr;
}

const cmChar_t* cmFsMakeDirFn(const cmChar_t* dir, const cmChar_t* fn )
{ return cmFileSysMakeDirFn(_cmFsH,dir,fn); }

const cmChar_t* cmFsMakeUserDirFn(const cmChar_t* dir, const cmChar_t* fn )
{ return cmFileSysMakeUserDirFn(_cmFsH,dir,fn); }

const cmChar_t* cmFsVMakeDir(     const cmChar_t* dir,  va_list vl )
{ return cmFileSysVMakeDir(_cmFsH,dir,vl); }

const cmChar_t* cmFsMakeDir(     const cmChar_t* dir,  ... )
{
  va_list vl;
  va_start(vl,dir);
  const cmChar_t* retPtr = cmFsVMakeDir(dir,vl);
  va_end(vl);
  return retPtr;
}

const cmChar_t* cmFsVMakeUserDir( const cmChar_t* dir,  va_list vl )
{ return cmFileSysVMakeUserDir(_cmFsH,dir,vl);  }

const cmChar_t* cmFsMakeUserDir( const cmChar_t* dir,  ... )
{
  va_list vl;
  va_start(vl,dir);
  const cmChar_t* retPtr = cmFsVMakeUserDir(dir,vl);
  va_end(vl);
  return retPtr;
}

void                 cmFsFreeFn(  const cmChar_t* fn )
{ cmFileSysFreeFn(_cmFsH, fn); }

cmFsRC_t             cmFsGenFn( const cmChar_t* dir, const cmChar_t* prefixStr, const cmChar_t* extStr, const cmChar_t** fnPtr )
{ return cmFileSysGenFn(_cmFsH,dir,prefixStr,extStr,fnPtr); }

cmFsRC_t             cmFsMkDir( const cmChar_t* dir )
{ return cmFileSysMkDir(_cmFsH,dir); }

cmFsRC_t             cmFsMkDirAll( const cmChar_t* dir )
{ return cmFileSysMkDirAll(_cmFsH,dir); }

cmFsRC_t             cmFsMkUserDir( const cmChar_t* dir )
{ return cmFileSysMkUserDir(_cmFsH,dir); }

cmFsRC_t             cmFsMkUserDirAll( const cmChar_t* dir )
{ return cmFileSysMkUserDirAll(_cmFsH,dir); }

cmFileSysPathPart_t* cmFsPathParts(     const cmChar_t* pathNameStr )
{ return cmFileSysPathParts(_cmFsH,pathNameStr); }

void                 cmFsFreePathParts( cmFileSysPathPart_t* p )
{ cmFileSysFreePathParts(_cmFsH,p); }

cmChar_t**           cmFsDirParts(      const cmChar_t* dirStr )
{ return cmFileSysDirParts(_cmFsH,dirStr); }

void                 cmFsFreeDirParts(  cmChar_t** dirStrArray )
{ cmFileSysFreeDirParts(_cmFsH,dirStrArray); }

unsigned             cmFsDirPartsCount( cmChar_t** dirStrArray )
{ return cmFileSysDirPartsCount(_cmFsH, dirStrArray); }

cmChar_t*            cmFsFormDir(       cmChar_t** dirStrArray, unsigned n )
{ return cmFileSysFormDir(_cmFsH,dirStrArray,n); }
 
void                 cmFsFreeDir(       const cmChar_t* dir )
{ cmFileSysFreeDir(_cmFsH,dir); }

cmFileSysDirEntry_t* cmFsDirEntries(     const cmChar_t* dirStr, unsigned includeFlags, unsigned* dirEntryCntPtr )
{ return cmFileSysDirEntries(_cmFsH,dirStr,includeFlags,dirEntryCntPtr); }

void                 cmFsDirFreeEntries( cmFileSysDirEntry_t* p )
{ cmFileSysDirFreeEntries(_cmFsH,p); }

cmFsRC_t             cmFsErrorCode()
{ return cmFileSysErrorCode(_cmFsH); }

// end global version
//======================================================================================================
//


//{ { label:cmFileSysEx }
//(
//
// cmFileSysTest() function gives usage and testing examples 
// for some of the cmFileSys functions. Note that the
// 'dir0' directory should exist and contain files and
// a shallow sub-tree in order to exercise the directory
// tree walking routine.
//
//)
//(


void _cmFileSysTestFnParser( 
  cmFileSysH_t    h, 
  cmRpt_t*        rpt, 
  const cmChar_t* fn );

cmFsRC_t cmFileSysTest( cmCtx_t* ctx )
{
  // The global heap manager must have been initialized 
  // via cmMdInitialize() prior to running this function.

  cmFsRC_t        rc      = kOkFsRC;
  cmFileSysH_t    h       = cmFileSysNullHandle;
  const char*     dir0    = cmFsMakeUserDir("src/kc",NULL); 
  const char*     dir1    = cmFsMakeUserDir("blah",NULL,NULL,NULL);
  const char*     file0   = cmFsMakeUserFn(NULL,".emacs",NULL,NULL);
  const char*     file1   = cmFsMakeUserFn(NULL,"blah","txt",NULL);
  const char      not[]   = " not ";
  const char      e[]     = " ";
  bool            fl      = false;
  const cmChar_t* fn      = NULL;

  // Initialize the file system.
  if((rc = cmFileSysInitialize(&h,ctx,"fs_test")) != kOkFsRC )
    return rc;

  //----------------------------------------------------------
  // Print the standard directories
  //
  printf("Prefs Dir:%s\n",cmFsPrefsDir());
  printf("Rsrc Dir: %s\n",cmFsRsrcDir());
  printf("User Dir: %s\n",cmFsUserDir());

  //----------------------------------------------------------
  // Run the file system type checker
  //
  fl = cmFileSysIsDir(h,dir0);
  printf("'%s' is%sa directory.\n",dir0, (fl ? e : not));  

  fl = cmFileSysIsDir(h,dir1);
  printf("'%s' is%sa directory.\n",dir1, (fl ? e : not));  

  fl = cmFileSysIsFile(h,file0);
  printf("'%s' is%sa file.\n",file0, (fl ? e : not));  

  fl = cmFileSysIsFile(h,file1);
  printf("'%s' is%sa file.\n",file1, (fl ? e : not));  

  //----------------------------------------------------------
  // Test the file name creation functions
  //
  if((fn = cmFileSysMakeUserFn(h,"src","cmFileSys",
        "c","cm","src",NULL)) != NULL)
  {
    printf("File:'%s'\n",fn);
  }
  cmFileSysFreeFn(h,fn);

  if((fn = cmFileSysMakeUserFn(h,"src","cmFileSys",
        ".c","/cm/","/src/",NULL)) != NULL )
  {
    printf("File:'%s'\n",fn);
  }
  cmFileSysFreeFn(h,fn);

  //----------------------------------------------------------
  // Test the file name parsing functions
  //

  const char* fn0 = cmFileSysMakeUserFn(h,"src/cm/src","cmFileSys","c",NULL);
  const char* fn1 = cmFileSysMakeUserFn(h,"src/cm/src","cmFileSys",NULL,NULL);
  const char* fn2 = cmFileSysMakeUserDir(h,"src/cm/src/cmFileSys/",NULL);
  
  _cmFileSysTestFnParser(h,&ctx->rpt,fn0);
  _cmFileSysTestFnParser(h,&ctx->rpt,fn1);
  _cmFileSysTestFnParser(h,&ctx->rpt,fn2);

  _cmFileSysTestFnParser(h,&ctx->rpt,"cmFileSys.c");
  _cmFileSysTestFnParser(h,&ctx->rpt,"/");
  _cmFileSysTestFnParser(h,&ctx->rpt," ");

  cmFileSysFreeFn(h,fn0);
  cmFileSysFreeFn(h,fn1);
  cmFileSysFreeFn(h,fn1);

  //----------------------------------------------------------
  // Test the directory tree walking routines.
  //
  cmFileSysDirEntry_t* dep;
  unsigned             dirEntCnt;
  unsigned             filterFlags = kDirFsFl 
                                   | kFileFsFl 
                                   | kRecurseFsFl 
                                   | kFullPathFsFl;

  const char* src_dir = cmFileSysMakeFn(h,dir0,"doc",NULL,NULL);

  cmRptPrintf(&ctx->rpt,"Dir Entry Test: %s\n",src_dir);

  if((dep = cmFileSysDirEntries(h,src_dir,filterFlags,&dirEntCnt)) != NULL)
  {
    unsigned i;
    for(i=0; i<dirEntCnt; ++i)
      cmRptPrintf(&ctx->rpt,"%s\n",dep[i].name);

    cmFileSysDirFreeEntries(h,dep);
  }

  cmFileSysFreeFn(h,src_dir);

  //----------------------------------------------------------
  // Test the directory parsing/building routines.
  //
  cmRptPrintf(&ctx->rpt,"Dir Parsing routings:\n");
  cmChar_t**      a;
  unsigned        j;
  for(j=0; j<2; ++j)
  {
    const cmChar_t* dstr = dir0 + j;
    if((a = cmFileSysDirParts(h,dstr)) == NULL)
      cmRptPrint(&ctx->rpt,"cmFileSysDirParts() failed.\n");
    else
    {
      unsigned i;

      cmRptPrintf(&ctx->rpt,"Input:%s\n",dstr);
      for(i=0; a[i]!=NULL; ++i)
        cmRptPrintf(&ctx->rpt,"%i : %s\n",i,a[i]);

      cmChar_t* d;
      if((d = cmFileSysFormDir(h,a,
            cmFileSysDirPartsCount(h,a))) != NULL )
      {
        cmRptPrintf(&ctx->rpt,"Reformed:%s\n",d);
      }

      cmFileSysFreeDirParts(h,a);
    }
  }

  //----------------------------------------------------------
  // Test the extended mkdir routine.
  //
  if( cmFileSysMkUserDirAll(h, "/temp/doc/doc" )!=kOkFsRC )
  {
    cmRptPrint(&ctx->rpt,"cmFileSysMkDirAll() failed.\n");
  }

  // finalize the file system
  if((rc = cmFileSysFinalize(&h)) != kOkFsRC )
    return rc;

  cmRptPrintf(&ctx->rpt,"File Test done\n");

  cmFsFreeFn(dir0);
  cmFsFreeFn(dir1);
  cmFsFreeFn(file0);
  cmFsFreeFn(file1);

  return rc;    
}

// Parse a file name and print the results. 
// Called by cmFileSysTest().
void _cmFileSysTestFnParser( 
  cmFileSysH_t    h, 
  cmRpt_t*        rpt, 
  const cmChar_t* fn )
{
  cmFileSysPathPart_t* pp;

  cmRptPrintf(rpt,"Fn Parse Test:%s\n",fn);

  if((pp = cmFileSysPathParts(h,fn)) != NULL )
  {
    if(pp->dirStr != NULL)
      cmRptPrintf(rpt,"Dir:%s\n",pp->dirStr);

    if(pp->fnStr != NULL)
      cmRptPrintf(rpt,"Fn:%s\n",pp->fnStr);

    if(pp->extStr != NULL )
      cmRptPrintf(rpt,"Ext:%s\n",pp->extStr);

    cmFileSysFreePathParts(h,pp);
  }

  cmRptPrintf(rpt,"\n");

}
//)
//}
