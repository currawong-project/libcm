#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmUdpPort.h"
#include "cmJson.h"
#include "cmUdpNet.h"
#include "cmTime.h"

typedef struct cmUdpNode_str
{
  cmChar_t*             label;
  unsigned              id;
  struct sockaddr_in    addr;
  cmUdpPort_t           port;
  struct cmUdpNode_str* link;
} cmUdpNode_t;

typedef struct
{
  cmErr_t            err;
  cmUdpNetCallback_t cbFunc;
  void*              cbArg;
  unsigned           timeOutMs;
  unsigned           nodeId;
  cmChar_t*          nodeLabel;
  cmUdpNode_t*       list;
  cmUdpH_t           udpH;
} cmUdpNet_t;

cmUdpNetH_t cmUdpNetNullHandle = cmSTATIC_NULL_HANDLE;

cmUdpNet_t* _cmUnHandleToPtr( cmUdpNetH_t h )
{
  cmUdpNet_t* p = (cmUdpNet_t*)h.h;
  assert(p !=NULL );
  return p;
}

cmUnRC_t _cmUdpNetFinal( cmUdpNet_t* p )
{
  // release the node list
  while( p->list != NULL)
  {
    cmUdpNode_t* np = p->list->link;
    cmMemFree(p->list->label);
    cmMemFree(p->list);
    p->list = np;
  }
 
  cmMemFree(p->nodeLabel);

  p->nodeLabel = NULL;
  p->cbFunc    = NULL;
  p->cbArg     = NULL;
  p->nodeId    = cmInvalidId;

  return kOkUnRC;
}

cmUnRC_t _cmUdpNetFree( cmUdpNet_t* p )
{
  cmUnRC_t rc;
  if((rc = _cmUdpNetFinal(p)) != kOkUnRC )
    return rc;
  
  // release the UDP port
  if( cmUdpFree(&p->udpH) != kOkUdpRC )
    return cmErrMsg(&p->err,kUdpPortFailUnRC,"The UDP port release failed.");

  cmMemFree(p);
  return kOkUnRC;
}


cmUdpNode_t* _cmUnFindLabel( cmUdpNet_t* p, const cmChar_t* label )
{
  cmUdpNode_t* np = p->list;
  for(; np != NULL; np = np->link )
    if( strcmp(np->label,label) == 0 )
      return np;

  return NULL;
}

cmUdpNode_t* _cmUnFindId( cmUdpNet_t* p, unsigned id )
{
  cmUdpNode_t* np = p->list;
  for(; np != NULL; np = np->link )
    if( np->id == id )
      return np;

  return NULL;
}

void _cmUdpNetCallback( void* cbArg, const char* data, unsigned dataByteCnt, const struct sockaddr_in* fromAddr )
{
  cmUdpNet_t* p = (cmUdpNet_t*)cbArg;
  
  if( p->cbFunc != NULL )
  {
    cmUdpNetH_t h;
    cmUdpNode_t* np = p->list;
    h.h = p;

    // locate the source node in the node list to get the source node id
    for(; np != NULL; np = np->link)
      if( np->addr.sin_addr.s_addr == fromAddr->sin_addr.s_addr )
      {
        // forward the call to the networks callback function
        p->cbFunc(p->cbArg,h,data,dataByteCnt,np->id);
        break;
      }

    if( np == NULL )
      cmErrMsg(&p->err,kNodeNotFoundUnRC,"A callback source node could not be identified.");

  }    
}

cmUnRC_t cmUdpNetAlloc(cmCtx_t* ctx,  cmUdpNetH_t*  hp)
{
  cmUnRC_t rc;
  if((rc = cmUdpNetFree(hp)) != kOkUnRC )
    return rc;

  cmUdpNet_t* p = cmMemAllocZ(cmUdpNet_t,1);

  cmErrSetup(&p->err,&ctx->rpt,"UDP Network");

  if( cmUdpAlloc(ctx,&p->udpH) != kOkUdpRC )
    return cmErrMsg(&p->err,kUdpPortFailUnRC,"UDP port allocation failed.");

  p->nodeId = cmInvalidId;
  hp->h = p;

  return kOkUnRC;
}

cmUnRC_t cmUdpNetAllocJson( 
  cmCtx_t*           ctx, 
  cmUdpNetH_t*       hp,
  cmJsonH_t          jsH,
  cmUdpNetCallback_t cbFunc,
  void*              cbArg,
  unsigned           flags)
{
  cmUnRC_t rc;

  if((rc = cmUdpNetAlloc(ctx,hp)) != kOkUnRC )
    return rc;

  if((rc = cmUdpNetInitJson(*hp,jsH,cbFunc,cbArg)) != kOkUnRC )
  {
    if( cmIsFlag(flags,kNetOptionalUnFl) )
      rc = kOkUnRC;

    goto errLabel;
  }

  if((rc = cmUdpNetEnableListen(*hp,cmIsFlag(flags,kListenUnFl))) != kOkUnRC )
    goto errLabel;

 errLabel:
  if( rc != kOkUnRC )
    cmUdpNetFree(hp);

  return rc;
}


cmUnRC_t cmUdpNetFree( cmUdpNetH_t* hp )
{
  cmUnRC_t rc = kOkUnRC;

  if( hp == NULL || cmUdpNetIsValid(*hp)==false )
    return kOkUnRC;

  cmUdpNet_t* p = _cmUnHandleToPtr(*hp);

  if((rc = _cmUdpNetFree(p)) != kOkUnRC )
    return rc;

  hp->h = NULL;

  return rc;
}


cmUnRC_t cmUdpNetInit( 
  cmUdpNetH_t         h,  
  const cmChar_t*     nodeLabel,
  unsigned            nodeId,
  cmUdpPort_t         nodeSocketPort,
  cmUdpNetCallback_t  cbFunc,
  void*               cbArg,
  unsigned            recvBufByteCnt,
  unsigned            socketRecvTimeOutMs )
{
  cmUnRC_t rc;
  cmUdpNet_t* p = _cmUnHandleToPtr(h);

  if((rc = _cmUdpNetFinal(p)) != kOkUnRC )
    return rc;

  // create the UDP port
  if( cmUdpInit(p->udpH,nodeSocketPort,kBlockingUdpFl,_cmUdpNetCallback,p,NULL,0,recvBufByteCnt,socketRecvTimeOutMs) != kOkUdpRC )
  {
    rc = cmErrMsg(&p->err,kUdpPortFailUnRC,"The UDP port create failed.");
    goto errLabel;
  }

  p->cbFunc    = cbFunc;
  p->cbArg     = cbArg;
  p->timeOutMs = socketRecvTimeOutMs;
  p->nodeId    = nodeId;
  p->nodeLabel = cmMemAllocStr(nodeLabel);
  p->list      = NULL;

 errLabel:

  if( rc != kOkUnRC )
    _cmUdpNetFinal(p);
  
  return rc;
}

cmUnRC_t _cmUnParseMemberErr( cmUdpNet_t* p, cmJsRC_t jsRC, const cmChar_t* errLabel, const cmChar_t* objectLabel )
{
  if( jsRC == kNodeNotFoundJsRC && errLabel != NULL )
    return cmErrMsg(&p->err,kJsonFailUnRC,"The required field '%s'was not found in the UDP network resource tree in the object '%s'.",errLabel,cmStringNullGuard(objectLabel));

  return cmErrMsg(&p->err,kJsonFailUnRC,"JSON parsing failed on the UDP network resource object '%s'.",cmStringNullGuard(objectLabel));

}


cmUnRC_t cmUdpNetInitJson( 
  cmUdpNetH_t        h, 
  cmJsonH_t          jsH,
  cmUdpNetCallback_t cbFunc,
  void*              cbArg)
{
  cmUnRC_t        rc;
  cmUdpNet_t*     p           = _cmUnHandleToPtr(h);
  cmJsonNode_t*   unp;
  cmJsonNode_t*   nap;
  const cmChar_t* errLabelPtr = NULL;
  cmJsRC_t        jsRC;
  unsigned        port,recvBufByteCnt,timeOutMs;
  const cmChar_t* localLabel;
  const cmChar_t* topLabel = "udpnet";
  unsigned        nodeCnt  = 0;
  unsigned        i;

  typedef struct
  {
    const cmChar_t* label;
    const cmChar_t* addr;
    unsigned        id;    
  } unNode_t;

  unNode_t* a = NULL;

  unNode_t* localPtr = NULL;

  if((rc = _cmUdpNetFinal(p)) != kOkUnRC )
    return rc;

  if((unp = cmJsonFindValue(jsH,topLabel,NULL,kObjectTId )) == NULL )
  {
    return cmErrMsg(&p->err,kJsonFailUnRC,"The JSON 'udpnet' element was not found.");
  }
  if(( jsRC = cmJsonMemberValues( unp, &errLabelPtr, 
        "port",           kIntTId,   &port,
        "recvBufByteCnt", kIntTId,   &recvBufByteCnt,
        "timeOutMs",      kIntTId,   &timeOutMs,
        "local",          kStringTId,&localLabel,
        "nodeArray",      kArrayTId, &nap,
        NULL )) != kOkJsRC )
  {
    rc = _cmUnParseMemberErr(p, jsRC, errLabelPtr, topLabel );
    goto errLabel;
  }

  
  // get a count of the number of nodes
  if((nodeCnt = cmJsonChildCount(nap)) == 0 )
  {
    rc = cmErrMsg(&p->err,kJsonFailUnRC,"The JSON 'udpnet' node array appears to be empty.");
    goto errLabel;
  }

  // allocate a temporary array to hold the node list
  a = cmMemAllocZ(unNode_t,nodeCnt);

  // for each remote node
  for(i=0; i<nodeCnt; ++i)
  {
    const cmJsonNode_t* naep = cmJsonArrayElementC(nap,i);

    // read the JSON recd
    if(( jsRC = cmJsonMemberValues( naep, &errLabelPtr, 
          "label", kStringTId, &a[i].label,
          "id",    kIntTId,    &a[i].id,
          "addr",  kStringTId, &a[i].addr,
          NULL )) != kOkJsRC )
    {
      rc = _cmUnParseMemberErr(p, jsRC, errLabelPtr, "nodeArray" );
      goto errLabel;
    }
    
    // track which node is the local node
    if( strcmp(localLabel,a[i].label) == 0 )
      localPtr = a + i;
  }

  // if no local node was located
  if( localPtr == NULL )
  {
    rc = cmErrMsg(&p->err, kJsonFailUnRC,"The local node label '%s' was not found.",cmStringNullGuard(localLabel));
    goto errLabel;
  }

  // initialize the network object
  if((rc = cmUdpNetInit(h,localPtr->label, localPtr->id, port, cbFunc, cbArg, recvBufByteCnt, timeOutMs )) != kOkUnRC )
    goto errLabel;

  // register each remote node
  for(i=0; i<nodeCnt; ++i)
    if( a + i != localPtr )
      if((rc = cmUdpNetRegisterRemote(h,a[i].label,a[i].id,a[i].addr,port)) != kOkUnRC )
        goto errLabel;
    
 errLabel:
  cmMemFree(a);

  if( rc != kOkUnRC )
    _cmUdpNetFinal(p);
  
  return rc;
}

bool cmUdpNetIsInitialized( cmUdpNetH_t h )
{
  cmUdpNet_t* p = _cmUnHandleToPtr(h);
  return p->nodeId != cmInvalidId;
}

cmUnRC_t cmUdpNetFinal( cmUdpNetH_t h )
{
  cmUdpNet_t* p = _cmUnHandleToPtr(h);
  return _cmUdpNetFinal(p);
}

cmUnRC_t cmUdpNetEnableListen( cmUdpNetH_t h, bool enableFl )
{
  cmUdpNet_t* p = _cmUnHandleToPtr(h);

  if( cmUdpEnableListen(p->udpH,enableFl) != kOkUdpRC )
    return cmErrMsg(&p->err,kUdpPortFailUnRC,"UDP port listen %s failed.",enableFl?"enable":"disable");

  return kOkUnRC;
}

bool     cmUdpNetIsValid( cmUdpNetH_t h )
{ return h.h != NULL; }

unsigned        cmUdpNetLocalNodeId( cmUdpNetH_t h )
{
  cmUdpNet_t* p = _cmUnHandleToPtr(h);
  return p->nodeId;
}

const cmChar_t* cmUdpNetLocalNodeLabel( cmUdpNetH_t h )
{
  cmUdpNet_t* p = _cmUnHandleToPtr(h);
  return p->nodeLabel;
}

unsigned   cmUdpNetNodeLabelToId( cmUdpNetH_t h, const cmChar_t* label )
{
  cmUdpNet_t* p = _cmUnHandleToPtr(h);

  // if the network was not initialized then nodeLabel == NULL
  if( p->nodeLabel == NULL )
    return cmInvalidId;

  // if 'label' refers to the local node
  if( strcmp(label,p->nodeLabel) == 0 )
    return p->nodeId;

  // otherwise search the remote node list
  cmUdpNode_t* np = p->list;
  for(; np != NULL; np = np->link )
    if( strcmp(np->label,label) == 0 )
      return np->id;

  return cmInvalidId;
}

const cmChar_t*   cmUdpNetNodeIdToLabel( cmUdpNetH_t h, unsigned id )
{
  cmUdpNet_t* p = _cmUnHandleToPtr(h);

  // if 'id' refers to the local node
  if( id == p->nodeId )
    return p->nodeLabel;

  // otherwise search the remote node list
  cmUdpNode_t* np = p->list;
  for(; np != NULL; np = np->link )
    if( np->id == id )
      return np->label;

  return NULL;
}

unsigned        cmUdpNetNodeCount( cmUdpNetH_t h )
{
  unsigned     cnt = 0;
  cmUdpNet_t*  p   = _cmUnHandleToPtr(h);
  cmUdpNode_t* np  = p->list;
  for(; np != NULL; np = np->link )
    ++cnt;

  return cnt;
}

unsigned        cmUdpNetNodeId(    cmUdpNetH_t h, unsigned nodeIdx )
{
  unsigned     cnt = 0;
  cmUdpNet_t*  p   = _cmUnHandleToPtr(h);
  cmUdpNode_t* np  = p->list;

  for(; np != NULL; np = np->link )
    if( cnt == nodeIdx )
      return np->id;

  return cmInvalidId;

}


cmUnRC_t cmUdpNetRegisterRemote( 
  cmUdpNetH_t h, 
  const cmChar_t* remoteNodeLabel,
  unsigned        remoteNodeId,
  const char*     remoteNodeSockAddr,
  cmUdpPort_t     remoteNodePort )
{
  cmUdpNet_t* p = _cmUnHandleToPtr(h);
  struct sockaddr_in addr;

  if( remoteNodeLabel == NULL || strlen(remoteNodeLabel)==0 )
    return cmErrMsg(&p->err,kInvalidNodeLabelUnRC,"The node label must contain a non-empty string.");

  if( _cmUnFindLabel(p,remoteNodeLabel) != NULL )
    return cmErrMsg(&p->err,kDuplicateNodeLabelUnRC,"The node label '%s' is already in use.",cmStringNullGuard(remoteNodeLabel));

  if( _cmUnFindId(p,remoteNodeId) != NULL )
    return cmErrMsg(&p->err,kDuplicateNodeIdUnRC,"The node id '%i' is already in use.",remoteNodeId);

  if( cmUdpInitAddr(p->udpH, remoteNodeSockAddr, remoteNodePort, &addr ) != kOkUdpRC )
    return cmErrMsg(&p->err,kInvalidNodeAddrUnRC,"The node address '%s' port:%i  is not valid.",cmStringNullGuard(remoteNodeSockAddr),remoteNodePort);

  cmUdpNode_t* np = cmMemAllocZ(cmUdpNode_t,1);

  np->label = cmMemAllocStr(remoteNodeLabel);
  np->id    = remoteNodeId;
  np->addr  = addr;
  np->port  = remoteNodePort;
  np->link  = p->list;
  p->list   = np;

  return kOkUnRC;
}

cmUnRC_t cmUdpNetSendById(    cmUdpNetH_t h, unsigned remoteNodeId, const void* data, unsigned dataByteCnt )
{
  cmUdpNet_t*     p = _cmUnHandleToPtr(h);
  cmUdpNode_t* np;
  
  if((np = _cmUnFindId(p,remoteNodeId)) == NULL )
    return cmErrMsg(&p->err,kNodeNotFoundUnRC,"The remote node (id=%i) was not found.",remoteNodeId);

  if( cmUdpSendTo(p->udpH, data, dataByteCnt, &np->addr ) != kOkUdpRC )
    return cmErrMsg(&p->err,kSendFailUnRC,"An attempt to send to a remote node with id=%i failed.",remoteNodeId);

  return kOkUnRC;
}

cmUnRC_t cmUdpNetSendByLabel( cmUdpNetH_t h, const cmChar_t* remoteNodeLabel, const void* data, unsigned dataByteCnt )
{
  cmUdpNet_t*     p = _cmUnHandleToPtr(h);
  cmUdpNode_t* np;
  
  if((np = _cmUnFindLabel(p,remoteNodeLabel)) == NULL )
    return cmErrMsg(&p->err,kNodeNotFoundUnRC,"The remote node (label=%s) was not found.",cmStringNullGuard(remoteNodeLabel));

  if( cmUdpSendTo(p->udpH, data, dataByteCnt, &np->addr ) != kOkUdpRC )
    return cmErrMsg(&p->err,kSendFailUnRC,"An attempt to send to the remote node labeled:%s failed.",cmStringNullGuard(remoteNodeLabel));

  return kOkUnRC;
}

cmUnRC_t cmUdpNetReceive(  cmUdpNetH_t h, unsigned* msgCntPtr )
{
  cmUnRC_t rc        = kOkUnRC;
  unsigned maxMsgCnt = 0;
  unsigned msgCnt    = 0;

  if( msgCntPtr == NULL )
    maxMsgCnt = cmInvalidCnt;
  else
    maxMsgCnt = *msgCntPtr;

  cmUdpNet_t* p = _cmUnHandleToPtr(h);
  if( cmUdpIsValid(p->udpH ) )
  {
    for(msgCnt=0; (maxMsgCnt==cmInvalidCnt || msgCnt<maxMsgCnt) && cmUdpAvailDataByteCount(p->udpH); ++msgCnt )
    {
      if( cmUdpGetAvailData(p->udpH,NULL,NULL,NULL) != kOkUdpRC )
      {
        rc = cmErrMsg(&p->err,kGetDataFailUnRC,"An attempt to get available message data failed.");
        break;
      }
    }
  }

  if( msgCntPtr != NULL )
    *msgCntPtr = msgCnt;

  return rc;
}

cmUnRC_t cmUdpNetPrintNodes( cmUdpNetH_t h, cmRpt_t* rpt )
{
  cmUdpNet_t* p = _cmUnHandleToPtr(h);
  cmUdpNode_t* np = p->list;
  unsigned i;
  for(i=0; np != NULL; np = np->link, ++i )
    cmRptPrintf(rpt,"%5i %5i %s\n",i,np->id,np->label);

  return kOkUnRC;
}

void cmUdpNetReport( cmUdpNetH_t h, cmRpt_t* rpt )
{
  cmUdpNet_t* p = _cmUnHandleToPtr(h);
  cmUdpReport(p->udpH,rpt);
  
}

//=========================================================================================
typedef struct
{
  cmTimeSpec_t t;
  unsigned     localNodeId;
  cmUdpNetH_t  h;
} _cmUdpNetTestMsg_t;

void _cmUdpNetTestCb( void* cbArg, cmUdpNetH_t h, const char* data, unsigned dataByteCnt, unsigned remoteNodeId )
{
  cmRpt_t* rpt = (cmRpt_t*)cbArg;
  cmRptPrintf(rpt,"recv - id:%i %s\n",remoteNodeId, data);
}

void _cmUdpNetTestCb2( void* cbArg, cmUdpNetH_t h, const char* data, unsigned dataByteCnt, unsigned remoteNodeId )
{
  _cmUdpNetTestMsg_t* m = (_cmUdpNetTestMsg_t*)data;
  cmRpt_t* rpt = (cmRpt_t*)cbArg;

  assert( dataByteCnt == sizeof(_cmUdpNetTestMsg_t));

  if( m->localNodeId == cmUdpNetLocalNodeId(h) )
  {
    cmTimeSpec_t t;
    cmTimeGet(&t);
    cmRptPrintf(rpt,"elapsed:%i\n", cmTimeElapsedMicros(&m->t,&t));
  }
  else
  {
    cmRptPrintf(rpt,"echo\n");
    cmUdpNetSendById(h,m->localNodeId,&m,sizeof(*m));
  }
  
  
}


const cmChar_t* _cmUdpNetTestGetOpt( int argc, char* argv[], const char* opt )
{
  unsigned i;
  for(i=0; i<argc; ++i)
    if( strcmp(argv[i],opt) == 0 )
      return argv[i+1];

  return NULL;
}

cmRC_t cmUdpNetTest( cmCtx_t* ctx, int argc, char* argv[] )
{
  cmUdpNetH_t     h          = cmUdpNetNullHandle;
  cmJsonH_t       jsH        = cmJsonNullHandle;
  cmErr_t         err;
  cmRC_t          rc         = cmOkRC;
  unsigned        i          = 0;
  const cmChar_t* jsonFn;
  const cmChar_t* remoteNodeLabel;
  unsigned        strCharCnt = 31;
  cmChar_t        str[ strCharCnt + 1 ];
  bool            msgFl = false;

  enum { kUdpNetTestErrRC    = 1 };
  
  cmErrSetup(&err,&ctx->rpt,"UDP Net Test");

  // get the JSON file name
  if(( jsonFn = _cmUdpNetTestGetOpt(argc,argv,"-j")) == NULL)
    return cmErrMsg(&err,kUdpNetTestErrRC,"No JSON file name was given.");

  // get the remote node label to send too 
  if(( remoteNodeLabel= _cmUdpNetTestGetOpt(argc,argv,"-n")) == NULL )
    return cmErrMsg(&err,kUdpNetTestErrRC,"No remote node label was given.");

  // create the JSON tree
  if( cmJsonInitializeFromFile(&jsH, jsonFn, ctx ) != kOkJsRC )
  {
    rc = cmErrMsg(&err,kUdpNetTestErrRC,"JSON file init failed on '%s'.",cmStringNullGuard(jsonFn));
    goto errLabel;
  }


  if( msgFl )
  {
    // alloc/init and put the network into listening mode
    if( cmUdpNetAllocJson(ctx,&h,jsH,_cmUdpNetTestCb2,&ctx->rpt,kListenUnFl) != kOkUnRC )
    {
      rc = cmErrMsg(&err,kUdpNetTestErrRC,"UDP alloc/init/listen failed.");
      goto errLabel;
    }
  }
  else
  {
    if( cmUdpNetAlloc(ctx,&h) != kOkUnRC )
      return cmErrMsg(&err,kUdpNetTestErrRC,"UDP alloc failed.");

    if( cmUdpNetInitJson(h,jsH,_cmUdpNetTestCb,&ctx->rpt) != kOkUnRC )
    {
      rc = cmErrMsg(&err,kUdpNetTestErrRC,"UDP net init failed.");
      goto errLabel;
    }

    if( cmUdpNetEnableListen(h,true) != kOkUnRC )
    {
      rc = cmErrMsg(&err,kUdpNetTestErrRC,"UDP listen enable failed.");
      goto errLabel;
    }
  }

  cmRptPrintf(&ctx->rpt,"s=send p=print r=report\n");

  char c;
  while((c=getchar()) != 'q')
  {
    bool promptFl = true;

    switch(c)
    {
      case 's':
        {
          // form the emessage
          snprintf(str,strCharCnt,"msg=%i",i);

          // send a message to the remote node
          if( !msgFl )
            if( cmUdpNetSendByLabel(h,remoteNodeLabel,str,strlen(str)+1 ) != kOkUnRC )
              cmErrMsg(&err,kUdpNetTestErrRC,"UDP net send failed.");

          ++i;
        }
        break;

      case 't':
        {
          _cmUdpNetTestMsg_t m;
          cmTimeGet(&m.t);
          m.localNodeId = cmUdpNetLocalNodeId(h);
          m.h           = h;

          // send a message to the remote node
          if( msgFl )
            if( cmUdpNetSendByLabel(h,remoteNodeLabel,&m,sizeof(m) ) != kOkUnRC )
              cmErrMsg(&err,kUdpNetTestErrRC,"UDP net send failed.");
        }
        break;

      case 'p':
        cmUdpNetPrintNodes(h,&ctx->rpt);
        break;

      case 'r':
        cmUdpNetReport(h,&ctx->rpt);
        break;
        
      default:
        promptFl = false;
    }

    cmUdpNetReceive(h,NULL);

    if( promptFl )
      cmRptPrintf(&ctx->rpt,"%i> ",i);
  }    

 errLabel:

  if( cmUdpNetFree(&h) != kOkUnRC )
    rc = cmErrMsg(&err,kUdpNetTestErrRC,"UDP free failed.");

  if( cmJsonFinalize(&jsH) != kOkJsRC )
    rc = cmErrMsg(&err,kUdpNetTestErrRC,"JSON final fail.");

  return rc;
  
}
