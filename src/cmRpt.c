//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"

cmRpt_t cmRptNull = { NULL, NULL, NULL };

void _cmDefaultPrint( void* userPtr, const cmChar_t* text )
{ 
  if( text != NULL )
    fputs(text,stdout);
}

void _cmDefaultError( void* userPtr, const cmChar_t* text )
{  
  if( text != NULL )
    fputs(text,stderr);
}

void _cmOut( cmRptPrintFunc_t printFunc, void* userData, const cmChar_t* text )
{
  if( printFunc == NULL )
    _cmDefaultPrint(userData,text);
  else
    printFunc(userData,text);
}

void _cmVOut( cmRptPrintFunc_t printFunc, void* userData, const cmChar_t* fmt, va_list vl )
{
  va_list vl1;
  va_copy(vl1,vl);
  unsigned  n    = vsnprintf(NULL,0,fmt,vl1);
  va_end(vl1);

  unsigned  bufN = 511;
  cmChar_t  buf[bufN+1];
  cmChar_t* b    = buf;
  unsigned  bn   = bufN;

  if( n > bufN )
  {
    b  = cmMemAllocZ(cmChar_t,n+1); 
    bn = n;
  }

  b[0]=0;
  if( fmt != NULL )
    if( vsnprintf(b,bn,fmt,vl) > bn )
    _cmOut(printFunc,userData,"The following error message was truncated because the character buffer in cmRpt::_cmVOut() was too small.");

  _cmOut(printFunc,userData,b);

  if( n > bufN )
    cmMemFree(b);
}

void cmRptSetup( cmRpt_t* rpt, cmRptPrintFunc_t printFunc, cmRptPrintFunc_t errorFunc, void* userPtr )
{
  rpt->printFuncPtr = printFunc==NULL ? _cmDefaultPrint : printFunc;
  rpt->errorFuncPtr = errorFunc==NULL ? _cmDefaultError : errorFunc;
  rpt->userPtr      = userPtr;
}

void cmRptPrint( cmRpt_t* rpt, const cmChar_t* text )
{
  cmRptPrintFunc_t pfp = rpt==NULL ? NULL : rpt->printFuncPtr;
  void*            udp = rpt==NULL ? NULL : rpt->userPtr;
  _cmOut(pfp,udp,text);
}

void cmRptVPrintf( cmRpt_t* rpt, const cmChar_t* fmt, va_list vl )
{
  cmRptPrintFunc_t pfp = rpt==NULL ? NULL : rpt->printFuncPtr;
  void*            udp = rpt==NULL ? NULL : rpt->userPtr;
  _cmVOut(pfp,udp,fmt,vl);  
}

void cmRptPrintf(  cmRpt_t* rpt, const cmChar_t* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  cmRptVPrintf(rpt,fmt,vl);
  va_end(vl);
}

void cmRptError( cmRpt_t* rpt, const cmChar_t* text )
{
  cmRptPrintFunc_t pfp = rpt==NULL ? NULL : rpt->errorFuncPtr;
  void*            udp = rpt==NULL ? NULL : rpt->userPtr;
  _cmOut(pfp,udp,text);
}

void cmRptVErrorf( cmRpt_t* rpt, const cmChar_t* fmt, va_list vl )
{
  cmRptPrintFunc_t pfp = rpt==NULL ? NULL : rpt->errorFuncPtr;
  void*            udp = rpt==NULL ? NULL : rpt->userPtr;
  _cmVOut(pfp,udp,fmt,vl);
}

void cmRptErrorf(  cmRpt_t* rpt, const char* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  cmRptVErrorf(rpt,fmt,vl);
  va_end(vl);
}
