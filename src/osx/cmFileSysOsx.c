#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmFileSys.h"
#include "cmFileSysOsx.h"


#include <Carbon/Carbon.h>
#include <glob.h>

// Convert a CFString to a C String allocated on the linked heap.
cmChar_t* _cmCFStringToCString( _cmFsOsx_t* p, CFStringRef cfString )
{
  cmChar_t*          cp              = NULL;
  CFMutableStringRef cfMutableString = CFStringCreateMutableCopy(NULL, 0, cfString);

  CFStringNormalize(cfMutableString,kCFStringNormalizationFormC);

  CFIndex  n             = CFStringGetLength(cfMutableString);
  cmChar_t buf[ n + 1];
  Boolean  noErrFl       = CFStringGetCString(cfMutableString,buf,n+1, kCFStringEncodingASCII);

  if( noErrFl )
  {
    cp = cmLhAllocZ(p->lhH,cmChar_t,n+1);
    strncpy(cp,buf,n);
    buf[n] = 0;
  }
    
  CFRelease(cfMutableString);

  return cp;
  
}

// based on: wxWindows/src/common/filefn.cpp:wxMacFSRefToPath()
cmChar_t*  _cmFSRefToPath( _cmFsOsx_t* p, const FSRef *fsRef )
{
  CFURLRef fullURLRef;
  fullURLRef = CFURLCreateFromFSRef(NULL, fsRef);

  CFStringRef cfString = CFURLCopyFileSystemPath(fullURLRef, kCFURLPOSIXPathStyle);
  CFRelease( fullURLRef ) ;

  cmChar_t* cp = _cmCFStringToCString(p,cfString);
  CFRelease( cfString );

  return cp;
  //return wxMacCFStringHolder(cfMutableString).AsString();
}

// based on wsWindows/src/mac/carbon/utils.cpp
cmChar_t* _cmFindFolder(_cmFsOsx_t* p, short vol, OSType folderType, Boolean createFolder)
{
  FSRef fsRef;

  if ( FSFindFolder( vol, folderType, createFolder, &fsRef) == noErr)
    return  _cmFSRefToPath( p, &fsRef );

  return NULL;
}

// based on wxWidgets/src/mac/corefoundation/stdPaths_cf.cpp:GetResourcesDir()
cmChar_t* _cmGetBundleDir( _cmFsOsx_t* p )
{
  // we get the Bundle Resource directory - although many other similar functions could
  // be called to get other standard directories.
  CFURLRef relativeURL = CFBundleCopyResourcesDirectoryURL(CFBundleGetMainBundle());

  CFURLRef absoluteURL = CFURLCopyAbsoluteURL(relativeURL);

  CFStringRef cfStrPath = CFURLCopyFileSystemPath(absoluteURL,kCFURLPOSIXPathStyle);

  CFRelease(absoluteURL);

  cmChar_t* cp = _cmCFStringToCString(p,cfStrPath);

  CFRelease(cfStrPath);

  return cp;
}


//http://developer.apple.com/library/mac/#qa/qa1549/_index.html
cmChar_t* _cmCreatePathByExpandingTildePath( _cmFsOsx_t* p, const char* path )
{
  glob_t globbuf;
  char **v;
  char *expandedPath = NULL, *result = NULL;

  assert(path != NULL);

  if (glob(path, GLOB_TILDE, NULL, &globbuf) == 0) //success
  {
    v = globbuf.gl_pathv; //list of matched pathnames
    expandedPath = v[0]; //number of matched pathnames, gl_pathc == 1

    result = cmLhAllocZ(p->lhH,cmChar_t,strlen(expandedPath) + 1);

    //result = (char*)calloc(1, strlen(expandedPath) + 1); //the extra char is for the null-termination
    if(result)
      strncpy(result, expandedPath, strlen(expandedPath) + 1); //copy the null-termination as well

    globfree(&globbuf);
  }

  return result;
}



cmFsRC_t _cmOsxFileSysInit( _cmFsOsx_t** pp, cmLHeapH_t lhH, cmErr_t* err )
{
  _cmFsOsx_t* p = cmLhAllocZ(lhH,_cmFsOsx_t,1);
  p->err        = err;
  p->lhH        = lhH;
  p->prefDir    = _cmFindFolder( p, (short) kUserDomain, kPreferencesFolderType, kDontCreateFolder );
  p->rsrcDir    = _cmGetBundleDir(p);
  p->userDir    = _cmCreatePathByExpandingTildePath(p,"~");
  *pp           = p;
  return kOkFsRC;
}

cmFsRC_t _cmOsxFileSysFinalize( _cmFsOsx_t* p )
{
  return kOkFsRC;
}
