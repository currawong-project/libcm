//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.

//( { file_desc:"A collection of file system utility functions." kw:[system file]}
//
// Note that cmFileSysInitialize() creates an internal cmLHeapH_t based
// heap manager from which it allocates memory for some returned objects.
// (e.g. cmFileSysMakeFn(), cmFileSysPathParts(), cmFileSysDirEntries())
// Where possible the client can explicitely free these objects via the
// provided functions. (e.g. cmFileSysFreeFn(), cmFileSysFreePathParts(), cmFileSysDirFreeEntries())
// However if these objects are not free they will be automatically deallocated
// when the internal heap is destroyed by cmFileSysFinalize().
//
//)
 
#ifndef cmFileSys_h
#define cmFileSys_h

#ifdef __cplusplus
extern "C" {
#endif

  //(
  // Result codes returned by cmFileSys.h functions
  enum
  {
    kOkFsRC = cmOkRC,
    kMemAllocErrFsRC,
    kLHeapAllocErrFsRC,
    kStatFailFsRC,
    kAssertFailFsRC,
    kOpenDirFailFsRC,
    kFnTooLongFsRC,
    kMkDirFailFsRC,
    kSysErrFsRC,
    kOsxFailFsRC,
    kLinuxFailFsRC,
    kInvalidDirFsRC,
    kGenFileFailFsRC,
    kAccessFailFsRC,
    kFormFnFailFsRC
  };


  typedef cmHandle_t   cmFileSysH_t;  //< Opaque handle type used by all cmFileSys.h functions
  typedef unsigned     cmFsRC_t;      //< Result code used as the return type by many cmFileSys.h functions.

  extern  cmFileSysH_t cmFileSysNullHandle;  // NULL handle to be used for setting cmFileSysH_t type handles to an explicit uninitialized value.

  // Initialize a file system object.  
  // If *hp was not initalized by an earlier call to cmFileSysInitialize() then it should 
  // be set to cmFileSysNullHandle prior to calling this function. If *hp is a valid handle
  // then it is automatically finalized by an internal call to cmFileSysFinalize() prior to
  // being re-iniitalized.
  // The appNameStr is used to determine the location of the preference and resource directories
  // on some platforms
  cmFsRC_t cmFileSysInitialize( cmFileSysH_t* hp, cmCtx_t* ctx, const cmChar_t* appNameStr );

  // Finalize a file system object.  
  // Upon successful completion *hp is set to cmFileSysNullHandle.
  cmFsRC_t cmFileSysFinalize(   cmFileSysH_t* hp );

  // Returns true if the file system handle is active and initialized.
  bool     cmFileSysIsValid(    cmFileSysH_t h );

  const cmChar_t* cmFileSysAppName(  cmFileSysH_t h ); //< Return the application name as passed to cmFileSysInitialize()
  const cmChar_t* cmFileSysPrefsDir( cmFileSysH_t h ); //< Return the operating system dependent preference data directory for this application.
  const cmChar_t* cmFileSysRsrcDir(  cmFileSysH_t h ); //< Return the operating system dependent application resource directory for this application.
  const cmChar_t* cmFileSysUserDir(  cmFileSysH_t h ); //< Return the operating system dependent user directory for this application.

  // Check if a request to create a file will succeed.
  bool cmFileSysCanWriteToDir( cmFileSysH_t h, const cmChar_t* dirStr );

  // Test the type of a file system object:
  //
  bool     cmFileSysIsDir(  cmFileSysH_t h, const cmChar_t* dirStr ); //< Return true if 'dirStr' refers to an existing directory.
  bool     cmFileSysIsFile( cmFileSysH_t h, const cmChar_t* fnStr );  //< Return true if 'fnStr' refers to an existing file.
  bool     cmFileSysIsLink( cmFileSysH_t h, const cmChar_t* fnStr );  //< Return true if 'fnStr' refers to a symbolic link.

  // Create File Names:
  //
  // Create a file name by concatenating sub-strings.
  //
  // Variable arg's. entries are directories inserted between 
  // 'dirPrefixStr' and the file name.
  // Terminate var arg's directory list with a  NULL. 
  //
  // The returned string is allocated in a local heap maintained by the cmFileSys object.
  // The memory used by the string will exist until it is released with cmFileSysFreeFn()
  // or the cmFileSys object is finalized.
  const cmChar_t* cmFileSysMakeFn( cmFileSysH_t h, const cmChar_t* dirPrefix, const cmChar_t* fn, const cmChar_t* ext, ... );

  // Same as cmFileSysMakeFn but prefixes the entire file path with the current users
  // home directory. (i.e. /home/me/<dirPrefix>/<var-args-dir-0>/<var-args-dir1>/<fn>.<ext>)
  const cmChar_t* cmFileSysMakeUserFn( cmFileSysH_t h, const cmChar_t* dirPrefix, const cmChar_t* fn, const cmChar_t* ext, ... );
  
  // Same as cmFileSysMakeFn but with a va_list argument to accept the var. args. parameters.
  const cmChar_t* cmFileSysVMakeFn( cmFileSysH_t h, const cmChar_t* dirPrefix, const cmChar_t* fn, const cmChar_t* ext, va_list vl );

  // Same as cmFileSysMakeUserFn but with a va_list argument to accept the var. args parameters.
  const cmChar_t* cmFileSysVMakeUserFn( cmFileSysH_t h, const cmChar_t* dirPrefix, const cmChar_t* fn, const cmChar_t* ext, va_list vl );

  // Equivalent to same named cmFileSysMakeFn() functions but form a directory
  // path rather than a file name path.
  const cmChar_t* cmFileSysVMakeDir(     cmFileSysH_t h, const cmChar_t* dir,  va_list vl );
  const cmChar_t* cmFileSysMakeDir(      cmFileSysH_t h, const cmChar_t* dir,  ... );
  const cmChar_t* cmFileSysVMakeUserDir( cmFileSysH_t h, const cmChar_t* dir,  va_list vl );
  const cmChar_t* cmFileSysMakeUserDir(  cmFileSysH_t h, const cmChar_t* dir,  ... );

  // Equivalent to same named cmFileSysMake() functions but assumes a single directory prefix and the file name
  // extension is attached to 'fn'.  
  const cmChar_t* cmFileSysMakeDirFn(    cmFileSysH_t h, const cmChar_t* dir, const cmChar_t* fn );
  
  // Same as cmFileSysMakeDirFn() but prefixes 'dir' with the users home directory.
  const cmChar_t* cmFileSysMakeUserDirFn(cmFileSysH_t h, const cmChar_t* dir, const cmChar_t* fn );
  
  // Release the file name created through an earlier call to cmFileSysMakeFn().
  void            cmFileSysFreeFn( cmFileSysH_t h, const cmChar_t* fn );

  // Generate an unused filename in the directory 'dir' beginning with the prefix 'prefixStr'.
  // The returned file name will have the format: <dir>/<prefixStr>nnnn.<extStr> where
  // nnn represents 1 or more digits.  The returned string must be released with a 
  // call to cmMemFree().
  cmFsRC_t cmFileSysGenFn( cmFileSysH_t h, const cmChar_t* dir, const cmChar_t* prefixStr, const cmChar_t* extStr, const cmChar_t** fnPtr );

  // Create a directory - where the entire path already exists except for the 
  // final directory.
  cmFsRC_t    cmFileSysMkDir( cmFileSysH_t h, const cmChar_t* dir );
  
  // Same as cmFileSysMkDir() but 'dir' is automatically prefixed with the users home directory.
  cmFsRC_t    cmFileSysMkUserDir( cmFileSysH_t h, const cmChar_t* dir );
  
  // Create a complete directory path - where any of the path segments may
  // not already exist.
  cmFsRC_t    cmFileSysMkDirAll( cmFileSysH_t h, const cmChar_t* dir );

  // Same as cmFileSysMkDir() but 'dir' is automatically prefixed with the users home directory.
  cmFsRC_t    cmFileSysMkUserDirAll( cmFileSysH_t h, const cmChar_t* dir );

  // Parse a path into its parts:
  //  
  // Return record used by cmFileSysParts()
  typedef struct
  {
    const cmChar_t* dirStr;
    const cmChar_t* fnStr;
    const cmChar_t* extStr;
  } cmFileSysPathPart_t;

  // Given a file name decompose it into a directory string, file name string and file extension string.
  // The cmFileSysPathPart_t record and the memory used by the strings that it references 
  // are allocated from a local heap maintained  by the cmFileSys object. This memory will exist 
  // until it is released with cmFileSysFreePathParts() or the cmFileSysH_t  handle is finalized.
  cmFileSysPathPart_t* cmFileSysPathParts( cmFileSysH_t h, const cmChar_t* pathNameStr );

  // Free the memory associated with a cmFileSysPathPart_t record returned from an eariler call to cmFileSysPathParts(). 
  void  cmFileSysFreePathParts( cmFileSysH_t h, cmFileSysPathPart_t* p );

  // Return the parts of a directory path as an array of strings.
  // The last element in the array is set to NULL to mark the end of the array.
  // Note that all '/' separator characters are removed from the result with 
  // the exception of the first one - which denotes the root directory.
  // The returned array is allocated from the file systems internal heap and will
  // be automatically released when the file system is closed by cmFileSysDestroy().
  // The caller may optionally release the array memory with a call to
  // cmFileSysFreeDirParts().
  cmChar_t** cmFileSysDirParts(     cmFileSysH_t h, const cmChar_t* dirStr );
  void       cmFileSysFreeDirParts( cmFileSysH_t h, cmChar_t** dirStrArray );

  // Return the count of elements in a directory parts array as returned by 
  // cmFileSysDirParts().
  unsigned   cmFileSysDirPartsCount(cmFileSysH_t h, cmChar_t** dirStrArray );

  // Form a directory string from a NULL terminated array of strings.
  // If the first element in the array is set to '/' then the
  // resulting directory will be absolute rather than relative.
  // The returned string is allocated from the file systems internal heap and will
  // be automatically released when the file system is closed by cmFileSysDestroy().
  // The caller may optionally release the array memory with a call to
  // cmFileSysFreeDir().
  cmChar_t*  cmFileSysFormDir( cmFileSysH_t h, cmChar_t** dirStrArray, unsigned n );
  void       cmFileSysFreeDir( cmFileSysH_t h, const cmChar_t* dir );

  // Walk a directory tree:
  //
  // Flags used by cmFileSysDirEntries 'includeFlags' parameter.
  enum
  {
    kFileFsFl         = 0x001,   //< include all visible files
    kDirFsFl          = 0x002,   //< include all visible directory 
    kLinkFsFl         = 0x004,   //< include all symbolic links
    kInvisibleFsFl    = 0x008,   //< include file/dir name beginning with a '.'
    kCurDirFsFl       = 0x010,   //< include '.' directory
    kParentDirFsFl    = 0x020,   //< include '..' directory

    kAllFsFl          = 0x02f,   //< all type flags

    kFullPathFsFl     = 0x040,   //< return the full path in the 'name' field of cmFileSysDirEntry_t;
    kRecurseFsFl      = 0x080,   //< recurse into directories
    kRecurseLinksFsFl = 0x100    //< recurse into symbol link directories 

  };

  // The return type for cmFileSysDirEntries().
  typedef struct
  {
    unsigned        flags;    //< Entry type flags from kXXXFsFl.
    const cmChar_t* name;     //< Entry name or full path depending on kFullPathFsFl.
  } cmFileSysDirEntry_t;

  // Return the file and directory names contained in a given subdirectory.
  //
  // Set 'includeFlags' with the  kXXXFsFl flags of the files to include in the returned 
  // directory entry array.  The value pointed to by dirEntryCntPtr will be set to the
  // number of records in the returned array.
  cmFileSysDirEntry_t* cmFileSysDirEntries( cmFileSysH_t h, const cmChar_t* dirStr, unsigned includeFlags, unsigned* dirEntryCntPtr );

  // Release the memory assoicated with a cmFileSysDirEntry_t array returned from an earlier call to cmFileSysDirEntries().
  void cmFileSysDirFreeEntries( cmFileSysH_t h, cmFileSysDirEntry_t* p );


  // Return the last error code generated by the file system.
  cmFsRC_t cmFileSysErrorCode( cmFileSysH_t h );

  //-------------------------------------------------------------------------------------------------
  
  // Global file system functions:
  // These functions work using a global cmFileSysH created by cmFsInitialize().
  // The functions are otherwise just wrappers for the same named function above.

  cmFsRC_t        cmFsInitialize( cmCtx_t* ctx, const cmChar_t* appNameStr );
  cmFsRC_t        cmFsFinalize();

  const cmChar_t* cmFsAppName();
  const cmChar_t* cmFsPrefsDir();
  const cmChar_t* cmFsRsrcDir();
  const cmChar_t* cmFsUserDir();

  bool            cmFsCanWriteToDir( const cmChar_t* dirStr );

  bool            cmFsIsDir(  const cmChar_t* dirStr );
  bool            cmFsIsFile( const cmChar_t* fnStr ); 
  bool            cmFsIsLink( const cmChar_t* fnStr ); 

  const cmChar_t* cmFsVMakeFn(     const cmChar_t* dirPrefix, const cmChar_t* fn, const cmChar_t* ext, va_list vl );
  const cmChar_t* cmFsMakeFn(      const cmChar_t* dirPrefix, const cmChar_t* fn, const cmChar_t* ext, ... );
  const cmChar_t* cmFsVMakeUserFn( const cmChar_t* dirPrefix, const cmChar_t* fn, const cmChar_t* ext, va_list vl );
  const cmChar_t* cmFsMakeUserFn(  const cmChar_t* dirPrefix, const cmChar_t* fn, const cmChar_t* ext, ... );
  
  const cmChar_t* cmFsVMakeDir(     const cmChar_t* dirPrefix,  va_list vl );
  const cmChar_t* cmFsMakeDir(      const cmChar_t* dirPrefix,  ... );
  const cmChar_t* cmFsVMakeUserDir( const cmChar_t* dirPrefix,  va_list vl );
  const cmChar_t* cmFsMakeUserDir(  const cmChar_t* dirPrefix,  ... );

  const cmChar_t* cmFsMakeDirFn(    const cmChar_t* dir, const cmChar_t* fn );
  const cmChar_t* cmFsMakeUserDirFn(const cmChar_t* dir, const cmChar_t* fn );
  
  void            cmFsFreeFn(      const cmChar_t* fn );
  cmFsRC_t        cmFsGenFn(       const cmChar_t* dir, const cmChar_t* prefixStr, const cmChar_t* extStr, const cmChar_t** fnPtr );


  cmFsRC_t        cmFsMkDir( const cmChar_t* dir );
  cmFsRC_t        cmFsMkUserDir( const cmChar_t* dir );
  cmFsRC_t        cmFsMkDirAll( const cmChar_t* dir );
  cmFsRC_t        cmFsMkUserDirAll( const cmChar_t* dir );

  cmFileSysPathPart_t* cmFsPathParts(     const cmChar_t* pathNameStr );
  void   cmFsFreePathParts( cmFileSysPathPart_t* p );


  cmChar_t** cmFsDirParts(      const cmChar_t* dirStr );
  void       cmFsFreeDirParts(  cmChar_t** dirStrArray );
  unsigned   cmFsDirPartsCount( cmChar_t** dirStrArray );
  cmChar_t*  cmFsFormDir(       cmChar_t** dirStrArray, unsigned n );
  void       cmFsFreeDir(       const cmChar_t* dir );

  cmFileSysDirEntry_t* cmFsDirEntries( const cmChar_t* dirStr, unsigned includeFlags, unsigned* dirEntryCntPtr );
  void        cmFsDirFreeEntries( cmFileSysDirEntry_t* p );

  cmFsRC_t    cmFsErrorCode();


  // Test and example function to demonstrate the use of the functions in cmFileSys.h
  cmFsRC_t    cmFileSysTest( cmCtx_t* ctx );  

  //)

#ifdef __cplusplus
}
#endif

#endif
