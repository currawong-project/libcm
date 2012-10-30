
#ifndef cmRpt_h
#define cmRpt_h

#ifdef __cplusplus
extern "C" {
#endif
  //{

  
  //(
  // 
  // The cmRpt class provides console style output for all objects in the cm system.
  // 
  // The cmRpt class provides console output style output, like stdout and stderr
  // for most of the classes in the cm library.
  // 
  // By wrapping this functionality in a class it is possible to easily 
  // redirect output to one or more possible console targets.
  //
  //) 

  //(
  // Application supplied callback function which is called to deliver text
  // to the destination terminal or GUI.
  typedef void (*cmRptPrintFunc_t)(void* cmRptUserPtr, const cmChar_t* text);

  // Data record used to hold the state information.  
  typedef struct
  {
    cmRptPrintFunc_t printFuncPtr; //< Application supplied callback text printing function as set from printFunc argument to cmRptSetup().
    cmRptPrintFunc_t errorFuncPtr; //< Application supplied callback error printing function as set from the errFunc argument to cmRptSetup().
    void*            userPtr;      //< Application supplied callback argument (cmRptUserPtr in cmRptPrintFunc_t) to be passed back to the application with each call to printFuncPtr() or errorFuncPtr().
  } cmRpt_t;

  // A global cmRpt_t object which can be used to initialze another cmRpt_t.
  extern cmRpt_t cmRptNull;

  // The host application calls cmRptSetup() to initialize a cmRpt object. 
  void cmRptSetup(   cmRpt_t* rpt, cmRptPrintFunc_t printFunc, cmRptPrintFunc_t errFunc, void* userPtr );

  // Text output functions:
  // Functions to print text to the application console.
  void cmRptPrint(   cmRpt_t* rpt, const cmChar_t* text );
  void cmRptVPrintf( cmRpt_t* rpt, const cmChar_t* fmt, va_list vl );
  void cmRptPrintf(  cmRpt_t* rpt, const cmChar_t* fmt, ... );

  // Error reporting functions:
  // Functions to print error messages to the application error console,
  void cmRptError(   cmRpt_t* rpt, const cmChar_t* text );
  void cmRptVErrorf( cmRpt_t* rpt, const cmChar_t* fmt, va_list vl );
  void cmRptErrorf(  cmRpt_t* rpt, const cmChar_t* fmt, ... );
  //)
  //}

#ifdef __cplusplus
  }
#endif


#endif
