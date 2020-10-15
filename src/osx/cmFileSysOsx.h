//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
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
