//{ 
//(
// This class is used to format error messages and track the last error generated.
//
// Most of the cmHandle_t based classes use cmErr_t to format error messages with a 
// title, maintain the last result code which indicated an error, and to hold
// a cmRpt_t object to manage application supplied text printing callbacks. 
//
//)
//

#ifndef cmErr_h
#define cmErr_h

#ifdef __cplusplus
extern "C" {
#endif
  
  //(

  typedef struct
  {
    cmRpt_t*        rpt;       //< Pointer to a cmRpt_t object which is used to direct error messages to an application supplied console.
    const cmChar_t* label;     //< This field contains a pointer to a text label used to form the error message title.
    cmRC_t          rc;        //< This is the last result code passed via one of the cmErrXXXMsg() functions.
    cmRC_t          warnRC;    //< Last warning RC
  } cmErr_t;

  // Setup a cmErr_t record.
  //
  // Note that rpt and staticLabelStr must point to client supplied objects
  // whose lifetime is at least that of this cmErr_t object. 
  void cmErrSetup( cmErr_t* err, cmRpt_t* rpt, const cmChar_t* staticLabelStr );

  // Duplicate a cmErr_t record.
  void cmErrClone( cmErr_t* dstErr, const cmErr_t* srcErr );
  
  // Error Reporting functions:
  // Functions to signal an error. The rc argument is generally specific to the 
  // client class using the error.  See the kXXXRC enumerations in the handle based
  // classes for examples of result codes.
  cmRC_t cmErrMsg( cmErr_t* err, cmRC_t rc, const cmChar_t* fmt, ... );
  cmRC_t cmErrVMsg(cmErr_t* err, cmRC_t rc, const cmChar_t* fmt, va_list vl );

  
  // Report Errors which contain accompanying system error codes.
  // Use these functions when a system error (e.g. Unix errno) gives additional information
  // about the source of the error. 
  cmRC_t cmErrSysMsg( cmErr_t* err, cmRC_t rc, cmSysErrCode_t sysErrCode, const cmChar_t* fmt, ... );
  cmRC_t cmErrVSysMsg(cmErr_t* err, cmRC_t rc, cmSysErrCode_t sysErrCode, const cmChar_t* fmt, va_list vl );

  // Warning Reporting functions:
  // Errors generally result in a task aborting. Warnings are informative but the task is
  // expected to continue.
  // Functions to signal a warning. The rc argument is generally specific to the 
  // client class using the error.  See the kXXXRC enumerations in the handle based
  // classes for examples of result codes.
  cmRC_t cmErrWarnMsg( cmErr_t* err, cmRC_t rc, const cmChar_t* fmt, ... );
  cmRC_t cmErrWarnVMsg(cmErr_t* err, cmRC_t rc, const cmChar_t* fmt, va_list vl );

  
  // Report warnings which contain accompanying system error codes.
  // Use these functions when a system error (e.g. Unix errno) gives additional information
  // about the source of the error. 
  cmRC_t cmErrWarnSysMsg( cmErr_t* err, cmRC_t rc, cmSysErrCode_t sysErrCode, const cmChar_t* fmt, ... );
  cmRC_t cmErrWarnVSysMsg(cmErr_t* err, cmRC_t rc, cmSysErrCode_t sysErrCode, const cmChar_t* fmt, va_list vl );

  // Return the last recorded RC.
  cmRC_t cmErrLastRC( cmErr_t* err );

  // Return the last recorded RC and set it to a new value.
  cmRC_t cmErrSetRC( cmErr_t* err, cmRC_t rc );

  // Return the last recorded RC and set it to cmOkRC.
  cmRC_t cmErrClearRC( cmErr_t* err );
   
  //)
  //}

#ifdef __cplusplus
}
#endif

#endif
