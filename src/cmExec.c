#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmExec.h"
#include <sys/wait.h>

cmExRC_t cmExecV( cmErr_t* err, int* returnValRef, const cmChar_t* pgmFn, va_list vl0 )
{
  cmExRC_t rc = kOkExRC;
  int      n  = 0;
  int      i  = 0;
  pid_t    pid;
  va_list  vl1;

  if( pgmFn == NULL )
    return cmErrMsg(err,kInvalidPgmFnExRC,"No executable program file name given in call to %s.",__FUNCTION__);

  // get the count of arguments
  va_copy(vl1,vl0);
  while( va_arg(vl1,cmChar_t*)!=NULL )
    ++n;
  va_end(vl1);


  // load argv with ptrs to the args
  cmChar_t* argv[n+2];
  
  argv[0] = (cmChar_t*)pgmFn;
  for(i=0; i<n+1; ++i)
    argv[i+1] = va_arg(vl0,cmChar_t*);
  argv[n+1] = NULL;
  
  errno = 0;
  
  switch( pid = fork())
  {
    case -1:
      rc = cmErrSysMsg(err,kForkFailExRC,errno,"Fork failed.");
      break;
      
    case 0:
      // we are in the child process - never to return
      execvp(pgmFn,argv);
       
      // under normal conditions execlp() does not return so we should never get here.
      rc = cmErrSysMsg(err,kExecFailExRC,errno,"Fork to '%s' failed. Is '%s' installed and on the execution path?",pgmFn,pgmFn);      
      break;
      
    default:
      {
          int rv;
          int wrc;
          
          // we are in the parent process - wait for the child to return
          if((wrc = waitpid(pid,&rv,0))==-1)
          {
            rc = cmErrSysMsg(err,kWaitFailExRC,errno,"Wait failed on call to '%s'.",pgmFn);
            goto errLabel;
          }

          if( returnValRef != NULL )
            *returnValRef = rv;
          
          if( WEXITSTATUS(rv) != 0 )
            rc = cmErrMsg(err,kPgmFailExRC,"'%s' failed.",pgmFn);
      }
      break;
  }

 errLabel:
  return rc;
}

cmExRC_t cmExec( cmErr_t* err, int* returnValRef, const cmChar_t* pgmFn, ... )
{
  va_list vl;
  va_start(vl,pgmFn);
  cmExRC_t rc = cmExecV(err,returnValRef,pgmFn,vl);
  va_end(vl);
  return rc;
}
