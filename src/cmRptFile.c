//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmFile.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmRptFile.h"

cmRptFileH_t cmRptFileNullHandle = cmSTATIC_NULL_HANDLE;

typedef struct
{
  cmErr_t   err;
  cmFileH_t printFileH;
  cmFileH_t errorFileH;
  cmRpt_t   rpt;
  
} cmRptFile_t;

cmRptFile_t* _cmRptFileHandleToPtr( cmRptFileH_t h )
{
  cmRptFile_t* p = (cmRptFile_t*)h.h;
  assert(p != NULL );
  return p;
}

void _cmRptFilePrintFunc(void* cmRptUserPtr, const cmChar_t* text)
{
  cmRptFile_t* p = (cmRptFile_t*)cmRptUserPtr;
  if( cmFileIsValid(p->printFileH))
    cmFilePrint(p->printFileH,text);
}

void _cmRptFileErrorFunc(void* cmRptUserPtr, const cmChar_t* text)
{
  cmRptFile_t* p = (cmRptFile_t*)cmRptUserPtr;
  if( cmFileIsValid(p->errorFileH))
    cmFilePrint(p->errorFileH,text);
}

cmRfRC_t _cmRptFileClose( cmRptFile_t* p )
{
  if( cmFileClose( &p->printFileH ) != kOkFileRC )
    cmErrMsg(&p->err,kFileFailRfRC,"Unable to close the print file.");

  if( cmFileClose( &p->errorFileH ) != kOkFileRC )
    cmErrMsg(&p->err,kFileFailRfRC,"Unable to close the error file.");
  
  cmMemFree(p);
  
  return kOkRfRC;
}

cmRfRC_t cmRptFileCreate( cmCtx_t* ctx, cmRptFileH_t* hp, const cmChar_t* printFn, const cmChar_t* errorFn )
{
  cmRptPrintFunc_t printFunc = NULL;
  cmRptPrintFunc_t errorFunc = NULL;
  cmRfRC_t rc;
  if((rc = cmRptFileClose(hp)) != kOkRfRC )
    return rc;

  cmRptFile_t* p = cmMemAllocZ( cmRptFile_t, 1 );
  
  cmErrSetup(&p->err,&ctx->rpt,"Rpt File");
  
  if( printFn != NULL )
  {
    if((rc = cmFileOpen(&p->printFileH,printFn,kWriteFileFl,p->err.rpt)) == kOkFileRC )
      printFunc = _cmRptFilePrintFunc;
    else
    {
      rc = cmErrMsg(&p->err,kFileFailRfRC,"Unable to open the print file '%s'.", cmStringNullGuard(printFn));
      goto errLabel;
    }
  }
  
  if( errorFn != NULL )
  {
    if((rc = cmFileOpen(&p->errorFileH,errorFn,kWriteFileFl,p->err.rpt)) == kOkFileRC )
      errorFunc = _cmRptFileErrorFunc;
    else
    {
      rc = cmErrMsg(&p->err,kFileFailRfRC,"Unable to open the error file '%s'.", cmStringNullGuard(errorFn));
      goto errLabel;
    }
  }
  
  if( errorFunc == NULL )
    errorFunc = printFunc;

  cmRptSetup(&p->rpt,printFunc,errorFunc,p);

  hp->h = p;
  
 errLabel:
  if( rc != kOkRfRC )
    _cmRptFileClose(p);

  return rc;
}

cmRfRC_t     cmRptFileClose( cmRptFileH_t* hp )
{
  cmRfRC_t rc = kOkRfRC;
  
  if( hp==NULL || cmRptFileIsValid(*hp)==false)
    return rc;

  cmRptFile_t* p = _cmRptFileHandleToPtr( *hp );

  if((rc = _cmRptFileClose(p)) != kOkRfRC )
    return rc;

  hp->h = NULL;
  
  return rc;
}

bool         cmRptFileIsValid( cmRptFileH_t h )
{ return h.h != NULL; }
    
cmRpt_t*     cmRptFileRpt( cmRptFileH_t h )
{
  cmRptFile_t* p = _cmRptFileHandleToPtr( h );
  return &p->rpt;
}
