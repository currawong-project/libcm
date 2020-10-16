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
#include "cmFileSysLinux.h"
#include <glob.h>

//http://developer.apple.com/library/mac/#qa/qa1549/_index.html
cmChar_t* _cmCreatePathByExpandingTildePath( _cmFsLinux_t* p, const char* path )
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


cmFsRC_t _cmLinuxFileSysInit( _cmFsLinux_t** pp, cmLHeapH_t lhH, cmErr_t* err )
{
  _cmFsLinux_t* p = cmLhAllocZ(lhH,_cmFsLinux_t,1);
  p->err        = err;
  p->lhH        = lhH;
  p->userDir    = _cmCreatePathByExpandingTildePath(p,"~");
  p->prefDir    = p->userDir;    // user preferences will be stored  invisible files in the home directory
  p->rsrcDir    = "/usr/share";  // program resources will be stored in /usr/share/app-name 

  *pp           = p;
  return kOkFsRC;
}

cmFsRC_t _cmLinuxFileSysFinalize( _cmFsLinux_t* p )
{
  return kOkFsRC;
}
