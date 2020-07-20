#ifndef cmFileSysOsx_h
#define cmFileSysOsx_h

typedef struct
{
  cmErr_t*   err;
  cmLHeapH_t lhH;
  cmChar_t*  prefDir;
  cmChar_t*  rsrcDir;
  cmChar_t*  userDir;
} _cmFsOsx_t;


cmFsRC_t _cmOsxFileSysInit( _cmFsOsx_t** pp, cmLHeapH_t lhH, cmErr_t* err );
cmFsRC_t _cmOsxFileSysFinalize( _cmFsOsx_t* p );
#endif
