#ifndef cmExec_h
#define cmExec_h


#ifdef __cplusplus
extern "C" {
#endif

  //( file_desc:"Run a child process via 'execvp()'" kw[system]
  enum
  {
    kOkExRC,
    kInvalidPgmFnExRC, // pgmFn was NULL
    kForkFailExRC,     // internal fork() failed
    kExecFailExRC,     // internal exec() failed.
    kPgmFailExRC,      // pgm returned a non-zero exit status
    kWaitFailExRC      // internal waitpid() failed
  };

  typedef unsigned cmExRC_t;

  // If returnValRef is non-NULL *returnValRef is set to the program return value.
  cmExRC_t cmExecV( cmErr_t* err, int* returnValRef, const cmChar_t* pgmFn, va_list vl );
  cmExRC_t cmExec(  cmErr_t* err, int* returnValRef, const cmChar_t* pgmFn, ... );  

  //)
  
#ifdef __cplusplus
}
#endif

#endif
