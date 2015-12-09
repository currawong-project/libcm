#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmJson.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLex.h"
#include "cmLinkedHeap.h"
#include "cmFile.h"
#include "cmXml.h"

cmXmlH_t cmXmlNullHandle = cmSTATIC_NULL_HANDLE;

typedef struct
{
  cmErr_t      err;   // 
  cmLHeapH_t   heapH; // linked heap stores all node memory
  cmXmlNode_t* root;
} cmXml_t;

cmXml_t* _cmXmlHandleToPtr( cmXmlH_t h )
{
  cmXml_t* p = (cmXml_t*)h.h;
  assert( p != NULL );
  return p;
}

cmXmlRC_t _cmXmlFree( cmXml_t* p )
{
  // free the internal heap object
  cmLHeapDestroy( &p->heapH );

}

cmXmlRC_t cmXmlAlloc( cmCtx_t* ctx, cmXmlH_t* hp, const cmChar_t* fn )
{
  cmXmlRC_t rc = kOkXmlRC;
  cmXml_t*   p = NULL;
  
  // finalize before initialize 
  if((rc = cmXmlFree(hp)) != kOkJsRC )
    return rc;

  // allocate the main object record
  if((p = cmMemAllocZ( cmXml_t, 1 )) == NULL )
    return cmErrMsg(&ctx->err,kMemAllocErrXmlRC,"Object memory allocation failed.");

  cmErrSetup(&p->err,&ctx->rpt,"XML Parser");

  // allocate the linked heap mgr
  if( cmLHeapIsValid(p->heapH = cmLHeapCreate(1024,ctx)) == false )
  {
    rc = cmErrMsg(&p->err,kMemAllocErrXmlRC,"Linked heap object allocation failed.");
    goto errLabel;
  }
  
  hp->h = p;
  
 errLabel:
  if(rc != kOkXmlRC )
    _cmXmlFree(p);
    
  return rc;
}

cmXmlRC_t cmXmlFree(  cmXmlH_t* hp )
{
  cmXmlRC_t rc = kOkXmlRC;
  
  if( hp!=NULL || cmXmlIsValid(*hp)==false )
    return kOkXmlRC;

  cmXml_t* p = _cmXmlHandleToPtr(*hp);

  if((rc = _cmXmlFree(p)) != kOkXmlRC )
    return rc;

  hp->h = NULL;
  
  return rc;  
}
  
bool      cmXmlIsValid( cmXmlH_t h )
{ return h.h != NULL; }

  
cmXmlRC_t cmXmlParse( cmXmlH_t h, const cmChar_t* fn )
{
}

cmXmlRC_t cmXmlClear( cmXmlH_t h )
{
}
