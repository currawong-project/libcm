//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmComplexTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmSymTbl.h"
#include "cmJson.h"
#include "cmText.h"
#include "cmDspPgmJsonToDot.h"
#include "cmFile.h"
#include "cmFileSys.h"

struct cmDotProc_str;

typedef struct cmDotPort_str
{
  struct cmDotProc_str* proc;
  cmChar_t*             labelStr;
  unsigned              portNo;
  unsigned              connCnt;  // count of connections to this port
  struct cmDotPort_str* link;
} cmDotPort_t;

typedef struct cmDotProc_str
{
  cmChar_t*             classStr;
  cmChar_t*             instStr;
  cmChar_t*             outStr;
  unsigned              portCnt;
  bool                  skipFl;
  
  cmDotPort_t*          ports;
  struct cmDotProc_str* link;
} cmDotProc_t;

typedef struct cmDotConn_str
{
  cmDotPort_t*          srcPort; // output
  cmDotPort_t*          dstPort; // input
  bool                  skipFl;
  struct cmDotConn_str* link;
} cmDotConn_t;


typedef struct cmDot_str
{
  cmErr_t      err;
  cmLHeapH_t   lH;
  cmDotProc_t* procs;
  cmDotConn_t* conns;
} cmDot_t;

typedef struct
{
  const cmChar_t* s0;
  const cmChar_t* s1;
} cmDotSubst_t;

cmDotSubst_t _cmDotSubstArray[] =
{
  { "Router",     "Rtr" },
  { "Scalar",     "Sc"  },
  { "ScaleRange", "SR"  },
  { "MsgList",    "ML"  },
  { "Button",     "Btn" },
  { "PortToSym",  "PtS" },
  { "1ofN",       "lOfN"},
  { NULL, NULL }
};

const cmChar_t* _cmDotSkipClassArray[] =
{
  "Scalar",
  NULL
};

void _cmDotReplaceDash( cmChar_t* str )
{
  cmChar_t* s = str;
  for(; *s; ++s )
    if( *s == '-' )
      *s = '_';
}

cmChar_t* _cmDotSubstitute( cmDot_t* p, const cmChar_t* label )
{
  unsigned i;
  cmChar_t* s = cmLhAllocStr(p->lH,label);
  
  for(i=0; _cmDotSubstArray[i].s0 != NULL; ++i)
  {
    unsigned n0 = cmTextLength(_cmDotSubstArray[i].s0);
    
    if( cmTextCmpN( _cmDotSubstArray[i].s0, s, n0 ) == 0 )
    {
      unsigned n1 = cmTextLength(_cmDotSubstArray[i].s1);
      assert(n0>=n1);
      cmTextShrinkS(s,s+n1,n0-n1);
      strncpy(s,_cmDotSubstArray[i].s1,n1);
    }
  }
  
  return s;
}

bool _cmDotIsSkipClass( const cmChar_t* classStr )
{
  unsigned i;

  for(i=0; _cmDotSkipClassArray[i]!=NULL; ++i)
    if( cmTextCmp(_cmDotSkipClassArray[i],classStr) == 0 )
      return true;
  
  return false;
}

cmDotPort_t* _cmDotFindPort( cmDotProc_t* proc, const cmChar_t* labelStr )
{
  cmDotPort_t* port =  proc->ports;
  for(; port!=NULL; port=port->link)
    if( cmTextCmp(port->labelStr,labelStr) == 0 )
      return port;

  return NULL;
}

cmDotProc_t* _cmDotFindProc( cmDot_t* p, const cmChar_t* instStr )
{
  cmDotProc_t* dp = p->procs;
  for(; dp!=NULL; dp=dp->link)
    if( cmTextCmp(dp->instStr,instStr) == 0 )
      return dp;

  return NULL;
}

cmDotPort_t* _cmDotNewPort( cmDot_t* p, cmDotProc_t* proc, const cmChar_t* labelStr )
{
  cmDotPort_t* port = NULL;

  if( labelStr==NULL || cmTextLength(labelStr)==0 )
  {
    cmErrMsg(&p->err,kInvalidArgDotRC,"A blank port label was encountered.");
    return NULL;
  }
  
  if((port = _cmDotFindPort(proc,labelStr)) != NULL )
    return port;

  port = cmLhAllocZ(p->lH,cmDotPort_t,1);

  port->proc      = proc;
  port->labelStr  = cmLhAllocStr(p->lH,labelStr);
  port->portNo    = proc->portCnt;
 
  proc->portCnt += 1;

  cmDotPort_t* p0 = NULL;
  cmDotPort_t* p1 = proc->ports;
  for(; p1!=NULL; p1=p1->link)
    p0 = p1;

  if( p0 == NULL )
    proc->ports = port;
  else
    p0->link = port;
  
  return port;
}

cmDotRC_t _cmDotNewConnection( cmDot_t* p, const cmChar_t* srcProcStr, const cmChar_t* srcPortStr, const cmChar_t* dstProcStr, const cmChar_t* dstPortStr )
{
  cmDotRC_t    rc = kOkDotRC;
  cmDotProc_t* srcProc;
  cmDotProc_t* dstProc;
  cmDotPort_t* srcPort;
  cmDotPort_t* dstPort;
  cmDotConn_t* conn;
  cmDotConn_t* c0 = NULL;
  cmDotConn_t* c1 = p->conns;

  // find the source (output) proc
  if((srcProc = _cmDotFindProc(p,srcProcStr)) == NULL )
  {
    rc = cmErrMsg(&p->err,kInvalidArgDotRC,"The connection source proc instance '%s' could not be found.",cmStringNullGuard(srcProcStr));
    goto errLabel;
  }


  // find the dest (input) proc
  if((dstProc = _cmDotFindProc(p,dstProcStr)) == NULL )
  {
    rc = cmErrMsg(&p->err,kInvalidArgDotRC,"The connection destination proc instance '%s' could not be found.",cmStringNullGuard(dstProcStr));
    goto errLabel;
  }

  // find the source port
  if((srcPort = _cmDotNewPort(p,srcProc,srcPortStr)) == NULL )
  {
    rc = cmErrMsg(&p->err,kInvalidArgDotRC,"The source port %s:%s could not be found or allocated.",cmStringNullGuard(srcProc->instStr),cmStringNullGuard(srcPortStr));
    goto errLabel;
  }

  // find the dest port
  if((dstPort = _cmDotNewPort(p,dstProc,dstPortStr)) == NULL )
  {
    rc = cmErrMsg(&p->err,kInvalidArgDotRC,"The destination port %s:%s could not be found or allocated.",cmStringNullGuard(dstProc->instStr),cmStringNullGuard(dstPortStr));
    goto errLabel;
  }

  conn = cmLhAllocZ(p->lH,cmDotConn_t,1);

  conn->srcPort = srcPort;
  conn->dstPort = dstPort;
  conn->skipFl  = _cmDotIsSkipClass(srcProc->classStr) || _cmDotIsSkipClass(dstProc->classStr);

  // track the number of connections to each port
  if( !conn->skipFl )
  {
    conn->dstPort->connCnt += 1;
    conn->srcPort->connCnt += 1;
  }

  
  // set c0 to point to the last connection record
  for(; c1!=NULL; c1=c1->link)
    c0 = c1;

  // make conn the last connection record
  if( c0 == NULL )
    p->conns = conn;
  else
    c0->link = conn;

 errLabel:
  return rc;
}


cmDotRC_t _cmDotNewProc( cmDot_t* p, const cmChar_t* classStr, const cmChar_t* instStr )
{
  cmDotRC_t rc = kOkDotRC;
  
  if( instStr==NULL || cmTextLength(instStr)==0 )
    return cmErrMsg(&p->err,kInvalidArgDotRC,"A blank or NULL instance label was encountered.");

  if( _cmDotFindProc( p, instStr ) )
    return cmErrMsg(&p->err,kInvalidArgDotRC,"A duplicate processor instance was encountered ('%s').",instStr);

  cmDotProc_t* ndp = cmLhAllocZ(p->lH,cmDotProc_t,1);

  ndp->classStr = cmLhAllocStr(p->lH,classStr);
  ndp->instStr  = cmLhAllocStr(p->lH,instStr);
  ndp->outStr   = _cmDotSubstitute(p,instStr);
  ndp->skipFl   = _cmDotIsSkipClass(classStr);

  cmDotProc_t* d0p = NULL;
  cmDotProc_t* d1p = p->procs;

  for(; d1p!=NULL; d1p=d1p->link )
    d0p = d1p;

  if( d0p == NULL )
    p->procs = ndp;
  else
    d0p->link = ndp;
  
  return rc;
}

unsigned _cmDotProcConnCount( cmDotProc_t* proc )
{
  unsigned connN = 0;
  
  cmDotPort_t* port = proc->ports;
  for(; port!=NULL; port=port->link)
    connN += port->connCnt;

  return connN;
}

cmDotRC_t _cmDotWriteOutput( cmDot_t* p, const cmChar_t* outFn )
{
  cmDotRC_t rc = kOkDotRC;
  
  cmFileH_t fH = cmFileNullHandle;

  cmFileSysPathPart_t* pathParts = cmFsPathParts(outFn);
  const cmChar_t* fn = NULL;
  
  if( pathParts == NULL )
  {
    rc = cmErrMsg(&p->err,kFileFailDotRC,"The output file name '%s' could be parsed.",cmStringNullGuard(outFn));
    goto errLabel;
  }

  if((fn = cmFsMakeFn( pathParts->dirStr, pathParts->fnStr, "dot", NULL )) == NULL )
  {
    rc = cmErrMsg(&p->err,kFileFailDotRC,"The output file name could not be formed.");
    goto errLabel;
  }

  if( cmFileOpen(&fH,fn,kWriteFileFl,p->err.rpt) != kOkFileRC )
  {
    rc = cmErrMsg(&p->err,kFileFailDotRC,"The output file '%s' could not be created.",cmStringNullGuard(outFn));
    goto errLabel;
  }

  cmFilePrintf(fH,"digraph dsppgm\n{\n node [shape=record]\n");

  cmDotProc_t* proc = p->procs;
  for(; proc!=NULL; proc=proc->link )
    if( proc->skipFl==false && _cmDotProcConnCount(proc)>0 )
    { 
      cmFilePrintf(fH,"\"%s\" [label=\"<n> %s",proc->outStr,proc->outStr);
      
      cmDotPort_t* port = proc->ports;
      for(; port!=NULL; port=port->link)
        if( port->connCnt > 0 )
          cmFilePrintf(fH,"|<p%i> %s",port->portNo,port->labelStr);
      
      cmFilePrintf(fH,"\"];\n");
    }
  
  cmDotConn_t* c = p->conns;
  for(; c!=NULL; c=c->link)
    if( !c->skipFl )
    {
      cmFilePrintf(fH,"\"%s\":p%i -> \"%s\":p%i;\n",
        c->srcPort->proc->outStr,c->srcPort->portNo,
        c->dstPort->proc->outStr,c->dstPort->portNo );
    }
  
  cmFilePrintf(fH,"}\n");
   
 errLabel:
  cmFileClose(&fH);
  cmFsFreeFn(fn);
  cmFsFreePathParts(pathParts);
  return rc;
}

cmDotRC_t cmDspPgmJsonToDot( cmCtx_t* ctx, const cmChar_t* inFn, const cmChar_t* outFn )
{
  cmDotRC_t       rc     = kOkDotRC;
  cmJsonH_t       jsH    = cmJsonNullHandle;
  cmJsonNode_t*   arr    = NULL;
  cmJsonNode_t*   rp     = NULL;
  const char*     errLbl = NULL;
  cmDot_t         dot;
  unsigned        i;
  cmDot_t*        p = &dot;

  memset(p,0,sizeof(dot));
  
  cmErrSetup(&p->err,&ctx->rpt,"cmDspPgmJsonToDot");

  // open the pgm description json file
  if( cmJsonInitializeFromFile( &jsH, inFn, ctx ) != kOkJsRC )
    return cmErrMsg(&p->err,kJsonFailDotRC,"The program description file '%s' could not be opened.",cmStringNullGuard(inFn));

  // create an lheap to hold internal data objects
  if(cmLHeapIsValid( p->lH = cmLHeapCreate( 8192, ctx))==false )
  {
    rc = cmErrMsg(&p->err,kLHeapFailDotRC,"The internal LHeap could not be created.");
    goto errLabel;
  }

  // locate the proc instance desc. array in the JSON tree
  if((arr = cmJsonFindValue(jsH, "inst_array", NULL, kArrayTId )) == NULL )
  {
    rc = cmErrMsg(&p->err,kJsonSyntaxErrDotRC,"The 'inst_array' tag was not found.");
    goto errLabel;
  }

  // get a count of proc instances
  unsigned n = cmJsonChildCount(arr);

  // parse each proc instance
  for(i=0; i<n; ++i)
  {
    
    if((rp = cmJsonArrayElement(arr,i)) == NULL )
    {
      rc = cmErrMsg(&p->err,kJsonSyntaxErrDotRC,"The 'inst_array' element %i was not found.",i);
      goto errLabel;      
    }

    cmChar_t* classStr = NULL;
    cmChar_t* instStr = NULL;
    if( cmJsonMemberValues(rp, &errLbl,
        "class", kStringTId, &classStr,
        "label", kStringTId, &instStr,
        NULL ) != kOkJsRC )
    {
      rc = cmErrMsg(&p->err,kJsonSyntaxErrDotRC,"The 'inst_array' element %i parse failed.",i);
      goto errLabel;      
    }

    // create a proc instance data record
    _cmDotNewProc( p, classStr, instStr );
    
  }

  // locate the connection desc array in the JSON tree
  if((arr = cmJsonFindValue(jsH, "conn_array", NULL, kArrayTId)) == NULL )
  {
    rc = cmErrMsg(&p->err,kJsonSyntaxErrDotRC,"The 'conn_array' tag was not found.");
    goto errLabel;
  }

  // get a count of the connections
  n = cmJsonChildCount(arr);

  // for each connection
  for(i=0; i<n; ++i)
  {
    
    if((rp = cmJsonArrayElement(arr,i)) == NULL )
    {
      rc = cmErrMsg(&p->err,kJsonSyntaxErrDotRC,"The 'conn_array' element %i was not found.",i);
      goto errLabel;      
    }

    cmChar_t* srcStr     = NULL;
    cmChar_t* srcPortStr = NULL;
    cmChar_t* dstStr     = NULL;
    cmChar_t* dstPortStr = NULL;

    if( cmJsonMemberValues(rp, &errLbl,
        "sid",  kStringTId, &srcStr,
        "svar", kStringTId, &srcPortStr,
        "did",  kStringTId, &dstStr,
        "dvar", kStringTId, &dstPortStr,
        NULL) != kOkJsRC )
    {
      rc = cmErrMsg(&p->err,kJsonSyntaxErrDotRC,"The 'conn_array' element %i parse failed.",i);
      goto errLabel;      
    }

    // create a connection data record
    _cmDotNewConnection( p, srcStr, srcPortStr, dstStr, dstPortStr );    
    
  }
  
  rc = _cmDotWriteOutput(p, outFn );

  
 errLabel:
  cmJsonFinalize(&jsH);
  cmLHeapDestroy(&p->lH);
  
  return rc;
}
