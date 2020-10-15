//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#ifndef cmLib_h
#define cmLib_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"Manage shared-libraries and query them for known symbols." kw:[system]}
  
  enum
  {
    kOkLibRC = cmOkRC,
    kOpenFailLibRC,
    kCloseFailLibRC,
    kSymFailLibRC,
    kInvalidIdLibRC,
    kFileSysFailLibRC
  
  };

  typedef unsigned   cmLibRC_t;
  typedef cmHandle_t cmLibH_t; 


  extern cmLibH_t cmLibNullHandle;

  // Initialize a dynamic library manager and scan a directory for dynamic libraries
  // to load. 'dirStr' is optional.
  cmLibRC_t cmLibInitialize( cmCtx_t* ctx, cmLibH_t* hp, const cmChar_t* dirStr );

  // Release a dynamic library manager and close any open libraries it may own.
  cmLibRC_t cmLibFinalize(   cmLibH_t* hp );
  
  // Return true if the dynamic library mgr. is initialized.
  bool      cmLibIsValid( cmLibH_t h );

  // Open a dynamic library.
  // Return cmInvalidId on error.
  unsigned  cmLibOpen(  cmLibH_t h, const cmChar_t* libFn );

  // Close a dynamic library.
  cmLibRC_t cmLibClose( cmLibH_t h, unsigned libId );

  // Return a pointer to a symbol from a dynamic library.
  void*     cmLibSym( cmLibH_t h, unsigned libId, const cmChar_t* fn );

  // Scan a directory for dynamic libraries.
  cmLibRC_t cmLibScan( cmLibH_t h, const cmChar_t* dirStr );

  // Return the count of open libraries.
  unsigned  cmLibCount( cmLibH_t h );

  // Return a library id given an index
  unsigned  cmLibIndexToId( cmLibH_t h, unsigned idx );

  // Return the libraries file name.
  const cmChar_t* cmLibName( cmLibH_t h, unsigned libId );
  
  //)

#ifdef __cplusplus
}
#endif

#endif
