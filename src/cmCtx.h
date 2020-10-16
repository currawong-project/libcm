//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
 
//( { file_desc:"Global application context record." kw[base] }
//
// cmCtx_t is used to hold application supplied cmRpt_t, cmErr_t and
// other global values for easy distribution throughtout a cm based application.
//
//  Most the libcm components need at least an application supplied cmRpt_t function
//  to initialize their own internal cmErr_t error class.  Likewise classes which
//  use a cmLHeapH_t based internal heap manager require application wide memory 
// manager configuration information.  The cmCtx_t packages this information and
// allows it to be easily distributed.  The applicaton and its constituent objects
// then need only maintain and pass pointers to a single cmCtx_t object to have access to 
// all the global program information.
//)

#ifndef cmCtx_h
#define cmCtx_h

#ifdef __cplusplus
extern "C" {
#endif

  //(

  // cmCtx_t data type.
  typedef struct
  {
    cmRpt_t  rpt;           // Application supplied global reporter. This reporter is also use by \ref err.
    cmErr_t  err;           // Application error reporter which can be used to report errors prior to the client object being initialized to the point where it can use it's own cmErr_t.
    unsigned guardByteCnt;  // Guard byte count in use by \ref cmMallocDebug.h .
    unsigned alignByteCnt;  // Align byte count used by the \ref cmMallocDebug.h
    unsigned mmFlags;       // Initialization flags used by \ref cmMallocDebug.h.
    void*    userDefPtr;    // Application defined pointer.
  } cmCtx_t;

  // cmCtx_t initialization function.
  void cmCtxSetup( 
    cmCtx_t*         ctx,         // The cmCtx_t to initialize.
    const cmChar_t*  title,       //  The cmCtx_t error label. See cmErrSetup().
    cmRptPrintFunc_t prtFunc,     //  The printFunc() to assign to the cmCtx_t.rpt.
    cmRptPrintFunc_t errFunc,     //  The errFunc() to assign to cmCtx_t.rpt.
    void*            cbPtr,       //  Callback data to use with prtFunc() and errFunc().
    unsigned         guardByteCnt,//  Guard byte count used to configure \ref cmMallocDebug.h
    unsigned         alignByteCnt,//  Align byte count used to configure \ref cmMallocDebug.h
    unsigned         mmFlags      //  Initialization flags used to configure \ref cmMallocDebug.h
                   );
  //)
 
#ifdef __cplusplus
}
#endif

#endif
