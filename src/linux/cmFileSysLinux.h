#ifndef cmFileSysLinux_h
#define cmFileSysLinux_h

//{
//(
// 
// Linux file system API.
//
// This API is used by cmFileSys when the library
// is compiled under Linux.
//
//)


//[

typedef struct
{
  cmErr_t*        err;
  cmLHeapH_t      lhH;
  const cmChar_t* prefDir;
  const cmChar_t* rsrcDir;
  cmChar_t*       userDir;
} _cmFsLinux_t;


cmFsRC_t _cmLinuxFileSysInit( _cmFsLinux_t** pp, cmLHeapH_t lhH, cmErr_t* err );
cmFsRC_t _cmLinuxFileSysFinalize( _cmFsLinux_t* p );

//]
//}

#endif
