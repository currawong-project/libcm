//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"

void cmErrSetup( cmErr_t* err, cmRpt_t* rpt, const cmChar_t* label )
{
  err->rpt   = rpt;
  err->label = label;
  err->rc    = cmOkRC;
}

void cmErrClone( cmErr_t* dstErr, const cmErr_t* srcErr )
{ memcpy(dstErr,srcErr,sizeof(*dstErr)); }

void _cmErrVMsg(cmErr_t* err, bool warnFl, cmRC_t rc, const cmChar_t* fmt, va_list vl )
{
  if( err->rpt == NULL )
    return;
 
  va_list vl0;
  va_copy(vl0,vl);

  const cmChar_t* hdrFmt = warnFl ? "%s warning: " : "%s error: ";
  const cmChar_t* codeFmt = " (RC:%i)";

  int        n0 = snprintf( NULL,0,hdrFmt,cmStringNullGuard(err->label)); 
  int        n1 = vsnprintf(NULL,0,fmt,vl);
  int        n2 = snprintf( NULL,0,codeFmt,rc);
  int        n  = n0+n1+n2+1;
  cmChar_t s[n];

  n0 =  snprintf(s,n,hdrFmt,cmStringNullGuard(err->label));
  n0 += vsnprintf(s+n0,n-n0,fmt,vl0);
  n0 += snprintf(s+n0,n-n0,codeFmt,rc);
  assert(n0 <= n );
  cmRptErrorf(err->rpt,"%s\n",s);
  va_end(vl0);
}

void _cmErrMsg( cmErr_t* err, bool warnFl, cmRC_t rc, const cmChar_t* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  _cmErrVMsg(err,warnFl,rc,fmt,vl);
  va_end(vl);
}

cmRC_t cmErrVMsg(cmErr_t* err, cmRC_t rc, const cmChar_t* fmt, va_list vl )
{ 
  cmErrSetRC(err,rc);
  _cmErrVMsg(err,false,rc,fmt,vl);
  return rc;
}

  
cmRC_t cmErrMsg( cmErr_t* err, cmRC_t rc, const cmChar_t* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  rc = cmErrVMsg(err,rc,fmt,vl);
  va_end(vl);
  return rc;
}


void _cmErrSysVMsg(cmErr_t* err, bool warnFl, cmRC_t rc, cmSysErrCode_t sysErrCode, const cmChar_t* fmt, va_list vl )
{
  const char* sysFmt = "\n System Error: (code:%i) %s.";
  int         n0     = snprintf(NULL,0,sysFmt,sysErrCode,strerror(sysErrCode));
  int         n1     = vsnprintf(NULL,0,fmt,vl);
  int         n      = n0 + n1 + 1;
  cmChar_t    s[n0+n1+1];

  n0  = snprintf(s,n,sysFmt,sysErrCode,strerror(sysErrCode));
  n0 += vsnprintf(s+n0,n-n0,fmt,vl);
  assert( n0 <= n );
  _cmErrMsg(err,warnFl,rc,s);  
}

cmRC_t cmErrVSysMsg(cmErr_t* err, cmRC_t rc, cmSysErrCode_t sysErrCode, const cmChar_t* fmt, va_list vl )
{
  cmErrSetRC(err,rc);
  _cmErrSysVMsg(err,false,rc,sysErrCode,fmt,vl);
  return rc;   
}

cmRC_t cmErrSysMsg( cmErr_t* err, cmRC_t rc, cmSysErrCode_t sysErrCode, const cmChar_t* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  rc = cmErrVSysMsg(err,rc,sysErrCode,fmt,vl);
  va_end(vl);
  return rc;
}

cmRC_t cmErrWarnVMsg(cmErr_t* err, cmRC_t rc, const cmChar_t* fmt, va_list vl )
{
  _cmErrVMsg(err,true,rc,fmt,vl);
  err->warnRC = rc;
  return rc;
}

cmRC_t cmErrWarnMsg( cmErr_t* err, cmRC_t rc, const cmChar_t* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  rc = cmErrWarnVMsg(err,rc,fmt,vl);
  va_end(vl);
  return rc;
}

cmRC_t cmErrWarnVSysMsg(cmErr_t* err, cmRC_t rc, cmSysErrCode_t sysErrCode, const cmChar_t* fmt, va_list vl )
{
  _cmErrSysVMsg(err,true,rc,sysErrCode,fmt,vl);
  err->warnRC = rc;
  return rc;
}


cmRC_t cmErrWarnSysMsg( cmErr_t* err, cmRC_t rc, cmSysErrCode_t sysErrCode, const cmChar_t* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  rc =  cmErrWarnVSysMsg(err,rc,sysErrCode,fmt,vl);
  va_end(vl);
  return rc;
}

  
cmRC_t cmErrLastRC( cmErr_t* err )
{ return err->rc; }

cmRC_t cmErrSetRC( cmErr_t* err, cmRC_t rc )
{
  cmRC_t retVal = err->rc;
  err->rc = rc;
  return retVal;
}

cmRC_t cmErrClearRC( cmErr_t* err )
{ return cmErrSetRC(err,cmOkRC); }
