#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmUdpPort.h"
#include "cmRtNet.h"
#include "cmTime.h"
#include "cmRtSysMsg.h"

enum
{
  kLocalNodeNetFl = 0x01,
  kValidNodeNetFl = 0x02
};

typedef enum
{
  kHelloSelNetId,
  kNodeSelNetId,
  kEndpointSelNetId,
  kEndpointAckSelNetId,
  kDoneSelNetId,
} cmRtNetSelId_t;

typedef struct cmRtNetEnd_str
{
  cmChar_t*              label;
  unsigned               id;
  struct cmRtNetEnd_str* link;
} cmRtNetEnd_t;


typedef struct cmRtNetNode_str
{
  cmChar_t*               label;
  struct sockaddr_in      sockaddr;
  cmChar_t*               addr;
  cmUdpPort_t             port;
  unsigned                flags;
  unsigned                endPtIdx;       // tracks the next endpoint to send during sync-mode
  unsigned                endPtCnt;       // local-node=actual cnt of endpt's remote-node:expected cnt of endpt's
  cmTimeSpec_t            lastSendTime;
  cmRtNetEnd_t*           ends;
  struct cmRtNetNode_str* link;
} cmRtNetNode_t;

typedef struct
{
  cmErr_t         err;
  cmUdpH_t        udpH;
  cmUdpCallback_t cbFunc;
  void*           cbArg;
  cmRtNetNode_t*  nodes;
  cmRtNetNode_t*  localNode;
  unsigned        udpRecvBufByteCnt;
  unsigned        udpTimeOutMs;
  unsigned        interSyncSendTimeMs;
} cmRtNet_t;


typedef struct
{
  cmRtSysMsgHdr_t hdr;
  cmRtNetSelId_t  selId;
  const cmChar_t* label;  // node     or endpoint label
  unsigned        id;     // endptCnt or endpoint id
} cmRtNetSyncMsg_t;

cmRtNetH_t cmRtNetNullHandle = cmSTATIC_NULL_HANDLE;

cmRtNet_t* _cmRtNetHandleToPtr( cmRtNetH_t h )
{
  cmRtNet_t* p = (cmRtNet_t*)h.h;
  assert( p != NULL );
  return p;
}

void _cmRtNetVRpt( cmRtNet_t* p, const cmChar_t* fmt, va_list vl )
{
  cmRptVPrintf(p->err.rpt,fmt,vl);
}

void _cmRtNetRpt( cmRtNet_t* p, const cmChar_t* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  _cmRtNetVRpt(p,fmt,vl);
  va_end(vl);
}

cmRtNetNode_t* _cmRtNetFindNode( cmRtNet_t* p, const cmChar_t* label )
{
  if( label == NULL )
    return NULL;

  cmRtNetNode_t* np = p->nodes;
  for(; np!=NULL; np=np->link)
    if( strcmp(label,np->label)==0)
      return np;

  return NULL;
}

cmRtNetNode_t* _cmRtNetFindNodeFromSockAddr( cmRtNet_t* p, const struct sockaddr_in* saddr )
{
  if( saddr == NULL )
    return NULL;

  cmRtNetNode_t* np = p->nodes;
  for(; np!=NULL; np=np->link)
    if( np->sockaddr.sin_addr.s_addr == saddr->sin_addr.s_addr && np->sockaddr.sin_port == saddr->sin_port )
      return np;
  
  return NULL;
}

void _cmRtNetFreeNode( cmRtNetNode_t* np )
{
  cmRtNetEnd_t* ep = np->ends;
  while( ep != NULL )
  {
    cmRtNetEnd_t* nep = ep->link;
    cmMemFree(ep->label);
    cmMemFree(ep);
    ep = nep;
  }

  cmMemFree(np->label);
  cmMemFree(np->addr);
  cmMemFree(np);
}

void _cmRtNetReleaseNodes( cmRtNet_t* p )
{
  cmRtNetNode_t* np = p->nodes;
  while( np != NULL )
  {
    cmRtNetNode_t* nnp = np->link;

    _cmRtNetFreeNode(np);

    np = nnp;
  }
  p->nodes = NULL;
  p->localNode = NULL;
}

cmRtNetRC_t  _cmRtNetReleaseNode( cmRtNet_t* p, cmRtNetNode_t* np )
{
  cmRtNetNode_t* cnp = p->nodes;
  cmRtNetNode_t* pnp = NULL;

  while( cnp != NULL )
  {
    cmRtNetNode_t* nnp = cnp->link;
    if( np == cnp )
    {
      if( pnp == NULL )
        p->nodes = np->link;
      else
        pnp->link = np->link;

      _cmRtNetFreeNode(np);

      return kOkNetRC;
    }

    pnp = np;
    cnp = nnp;
  }

  assert(0);
  return cmErrMsg(&p->err,kNodeNotFoundNetRC,"Node to release not found.");
}

cmRtNetRC_t _cmRtNetCreateNode( cmRtNet_t* p, const cmChar_t* label, const cmChar_t* addr, cmUdpPort_t port, const struct sockaddr_in* saddr, unsigned flags, unsigned endPtCnt )
{
  cmRtNetRC_t rc = kOkNetRC;
  cmRtNetNode_t* np;

  if( label == NULL )
    return cmErrMsg(&p->err,kInvalidLabelNetRC,"A null or blank node label was encountered.");

  if((np = _cmRtNetFindNode(p,label)) != NULL )
    return cmErrMsg(&p->err,kDuplLabelNetRC,"The node label '%s' is already in use.",cmStringNullGuard(label));
  
  np           = cmMemAllocZ(cmRtNetNode_t,1);
  np->label    = cmMemAllocStr(label);
  np->sockaddr = *saddr;
  np->addr     = addr==NULL ? NULL : cmMemAllocStr(addr);
  np->port     = port;
  np->flags    = flags;
  np->endPtCnt = endPtCnt;
  np->link     = p->nodes;
  p->nodes     = np;

  return rc;
}

cmRtNetEnd_t* _cmRtNetFindNodeEnd(cmRtNetNode_t* np, const cmChar_t* endPtLabel )
{
  cmRtNetEnd_t* ep = np->ends;
  for(; ep!=NULL; ep=ep->link)
    if( strcmp(ep->label,endPtLabel)==0 )
      return ep;
  return NULL;
}

cmRtNetEnd_t* _cmRtNetIndexToEndpoint( cmRtNet_t* p, cmRtNetNode_t* np, unsigned endIndex )
{
  cmRtNetEnd_t* ep = np->ends;
  unsigned i;
  for(i=0; ep!=NULL; ep=ep->link)
  {
    if( i == endIndex )
      return ep;
    ++i;
  }

  return NULL;
}

cmRtNetRC_t _cmRtNetCreateEndpoint( cmRtNet_t* p, cmRtNetNode_t* np, const cmChar_t* endPtLabel, unsigned endPtId )
{
  if( endPtLabel == NULL )
    return cmErrMsg(&p->err,kInvalidLabelNetRC,"A null or blank node label was encountered.");

  if( _cmRtNetFindNodeEnd( np, endPtLabel) != NULL)
    return cmErrMsg(&p->err,kDuplEndNetRC,"A duplicate endpoint ('%s') was encountered on node '%s'.",endPtLabel,np->label);

  cmRtNetRC_t   rc = kOkNetRC;
  cmRtNetEnd_t* ep = cmMemAllocZ(cmRtNetEnd_t,1);

  ep->label = cmMemAllocStr(endPtLabel);
  ep->id    = endPtId;
  ep->link      = np->ends;
  np->ends      = ep;

  return rc;
}

unsigned _cmRtNetSyncMsgSerialByteCount( const cmRtNetSyncMsg_t* m )
{ return sizeof(cmRtNetSyncMsg_t) + (m->label==NULL ? 1 : strlen(m->label) + 1); }

cmRtNetRC_t _cmRtNetSerializeSyncMsg( cmRtNet_t* p, const cmRtNetSyncMsg_t* m, void* buf, unsigned n )
{
  unsigned bn = _cmRtNetSyncMsgSerialByteCount(m);
  char*    b  = (char*)buf;

  if( bn > n )
    return cmErrMsg(&p->err,kBufToSmallNetRC,"Serialize buffer too small.");

  memcpy(b,m,sizeof(*m));
  strcpy(b + sizeof(*m),m->label==NULL ? "" : m->label);
  return kOkNetRC;
}

cmRtNetRC_t _cmRtNetDeserializeSyncMsg( const void* buf, unsigned n, cmRtNetSyncMsg_t* m )
{
  assert( n > sizeof(*m));
  memcpy(m,buf,sizeof(*m));
  const cmRtNetSyncMsg_t* mp = (const cmRtNetSyncMsg_t*)buf;
  const cmChar_t*   s  = (const cmChar_t*)(mp+1);
  m->label = cmMemAllocStr(s);
  return kOkNetRC;
}

cmRtNetRC_t _cmRtNetSendSyncMsg( cmRtNet_t* p, cmRtNetNode_t* np, cmRtNetSelId_t selId, const cmChar_t* msgLabel, unsigned msgId )
{
  cmRtNetSyncMsg_t m;
  cmRtNetRC_t      rc = kOkNetRC;
  cmUdpRC_t        udpRC = kOkUdpRC;

  m.hdr.rtSubIdx = cmInvalidIdx;
  m.hdr.selId    = kNetSyncSelRtId;
  m.selId        = selId;
  m.label        = msgLabel;
  m.id           = msgId;

  // determine size of msg to send
  unsigned n  = _cmRtNetSyncMsgSerialByteCount(&m);
  cmChar_t buf[n];

  // serialize msg into buf[]
  if((rc = _cmRtNetSerializeSyncMsg(p,&m,buf,n)) != kOkNetRC )
    return rc;

 
  // send the msg
  if( np==p->localNode )
    udpRC = cmUdpSend2(p->udpH, buf, n, "255.255.255.255", np->port );
  else
    udpRC = cmUdpSendTo(p->udpH, buf, n, &np->sockaddr );

  // check for send errors
  if( udpRC != kOkUdpRC )
  {
    rc = cmErrMsg(&p->err,kUdpPortFailNetRC,"Sync msg. send on UDP port failed.");
  }
  else
  {
    // record the last send time
    cmTimeGet(&np->lastSendTime);
  }

  return rc;
}

cmRtNetRC_t _cmRtNetFree( cmRtNet_t* p )
{
  cmRtNetRC_t rc = kOkNetRC;
  
  if( cmUdpFree(&p->udpH) != kOkUdpRC )
    cmErrMsg(&p->err,kUdpPortFailNetRC,"UDP Port free failed.");

  _cmRtNetReleaseNodes(p);

  cmMemFree(p);
  return rc;
}


cmRtNetRC_t cmRtNetAlloc( cmCtx_t* ctx, cmRtNetH_t* hp, cmUdpCallback_t cbFunc, void* cbArg )
{
  cmRtNetRC_t rc;
  if((rc = cmRtNetFree(hp)) != kOkNetRC )
    return rc;

  cmRtNet_t* p = cmMemAllocZ(cmRtNet_t,1);
  cmErrSetup(&p->err,&ctx->rpt,"cmRtNet");
 
  // allocate the UDP port
  if(cmUdpAlloc(ctx,&p->udpH) != kOkUdpRC )
  {
    cmErrMsg(&p->err,kUdpPortFailNetRC,"UDP Port allocate failed.");
    goto errLabel;
  }

  p->udpTimeOutMs        = 50;
  p->udpRecvBufByteCnt   = 8192;
  p->interSyncSendTimeMs = 10;
  p->cbFunc = cbFunc;
  p->cbArg  = cbArg;

  hp->h = p;

 errLabel:
  if(rc != kOkNetRC )
    _cmRtNetFree(p);

  return rc;
}

cmRtNetRC_t cmRtNetFree( cmRtNetH_t* hp )
{
  cmRtNetRC_t rc = kOkNetRC;

  if( hp==NULL || cmRtNetIsValid(*hp)==false )
    return rc;

  cmRtNet_t* p = _cmRtNetHandleToPtr(*hp);

  if((rc = _cmRtNetFree(p)) != kOkNetRC )
    return rc;

  hp->h = NULL;

  return rc;
}

const cmChar_t* cmRtNetLocalHostName( cmRtNetH_t h )
{
  cmRtNet_t*  p = _cmRtNetHandleToPtr(h);
  return cmUdpHostName(p->udpH);
}


bool cmRtNetIsValid( cmRtNetH_t h )
{ return h.h !=NULL; }

cmUdpH_t  cmRtNetUdpPortHandle( cmRtNetH_t h )
{
  cmRtNet_t*  p = _cmRtNetHandleToPtr(h);
  return p->udpH;
}

cmRtNetRC_t  _cmRtNetSendEndpointReplyMsg( cmRtNet_t* p, cmRtNetNode_t* np )
{
  cmRtNetRC_t     rc = kOkNetRC;
  cmRtNetEnd_t*   ep;
  const cmChar_t* msgLabel = NULL;
  unsigned        msgId    = cmInvalidId;  
  cmRtNetSelId_t  selId    = kEndpointSelNetId;
  const cmChar_t* rptLabel = "endpoint";

  if( np == NULL )
    return cmErrMsg(&p->err,kNodeNotFoundNetRC,"The net node associated with an endpoint reply was not found.");

  // if all of the endpoints have been sent to this node ...
  if((ep = _cmRtNetIndexToEndpoint(p,p->localNode,np->endPtIdx)) == NULL )
  {
    if( np->endPtIdx == p->localNode->endPtCnt )
    {
      selId = kDoneSelNetId;
      rptLabel = "done";
    }
    else
    {
      selId = kEndpointAckSelNetId;
      rptLabel = "ep ack";
    }
   
  }
  else
  {
    msgLabel = ep->label;
    msgId    = ep->id;
  }

  // notify the remote node that all endpoints have been sent
  if((rc = _cmRtNetSendSyncMsg(p,np,selId,msgLabel,msgId )) != kOkNetRC )
    rc = cmErrMsg(&p->err,rc,"Send '%s' to %s:%s:%i failed.",rptLabel,cmStringNullGuard(np->label),cmStringNullGuard(np->addr),np->port);
  else
    _cmRtNetRpt(p,"Sent %s.\n",cmStringNullGuard(rptLabel));

  np->endPtIdx += 1;

  return rc;
  
}

// When the network message recieve function (See cmRtNetAlloc() 'cbFunc') 
// receives a message with the cmRtSysMsgHdr_t.selId == kNetSyncSelRtId
// it should call this function to update the current sync state of the
// cmRtNet.
cmRtNetRC_t  _cmRtNetSyncModeRecv( cmRtNet_t* p, const char* data, unsigned dataByteCnt, const struct sockaddr_in* fromAddr )
{
  cmRtNetRC_t      rc = kOkNetRC;
  cmRtNetSyncMsg_t m;
  m.label = NULL;

  assert( cmRtNetIsSyncModeMsg(data,dataByteCnt));
  
  if( _cmRtNetDeserializeSyncMsg(data,dataByteCnt,&m) != kOkNetRC )
  {
    rc = cmErrMsg(&p->err,rc,"Net sync. receive failed due to deserialize fail.");
    goto errLabel;
  }

  assert( m.hdr.selId == kNetSyncSelRtId );

  // attempt to locate the remote node which sent the msg
  cmRtNetNode_t* np = _cmRtNetFindNodeFromSockAddr(p,fromAddr);

  switch( m.selId )
  {
    case kHelloSelNetId:    
    case kNodeSelNetId: 
      {
        // if the node already exists ...
        if( np != NULL )
        {
          // ... delete it because we are about to get new info. about it.
          if((rc =  _cmRtNetReleaseNode(p,np )) != kOkNetRC )
            goto errLabel;
        }

        //  create a node proxy to represent the remote node
        // (Note:m.id == remote node endpoint count (i.e. the count of endpoints expected for the remote node.))
        if(( rc = _cmRtNetCreateNode(p,m.label,NULL,0,fromAddr,0,m.id)) != kOkNetRC )
          goto errLabel;        

        // send response
        switch( m.selId )
        {
          case kHelloSelNetId:
            _cmRtNetRpt(p,"rcv hello\n"); // reply with local node
            rc = _cmRtNetSendSyncMsg( p, np, kNodeSelNetId, NULL, p->localNode->endPtCnt );
            break;

          case kNodeSelNetId:
            _cmRtNetRpt(p,"rcv node\n");
            _cmRtNetSendEndpointReplyMsg( p, np ); // reply with first endpoint
            break;

          default:
            assert(0);
        }

      }
      break;

    
    case kEndpointAckSelNetId:
    case kDoneSelNetId:
      rc = _cmRtNetSendEndpointReplyMsg(p,np);
      break;

    case kEndpointSelNetId: 
      {
        cmRtNetEnd_t* ep;


        // verify the remote node exists.
        if( np == NULL )
        {
          rc = cmErrMsg(&p->err,kNodeNotFoundNetRC,"The net node associated with an endpoint receive was not found.");
          goto errLabel;
        }
        
        // attempt to find the end point 
        if((ep = _cmRtNetFindNodeEnd(np,m.label)) != NULL )
          ep->id = m.id; // the endpoint was found update the endPtId
        else
        {
          // create a local proxy for the endpoint
          if((rc = _cmRtNetCreateEndpoint(p,np,m.label,m.id)) != kOkNetRC )
            goto errLabel;
        }

        // reply with a local endpoint or 'done' msg
        rc = _cmRtNetSendEndpointReplyMsg( p, np );
      }
      break;

    default:
      assert(0);
      break;
  }

 errLabel:

  cmMemFree((cmChar_t*)m.label);
  return rc;
}

void _cmRtNetRecv( void* cbArg, const char* data, unsigned dataByteCnt, const struct sockaddr_in* fromAddr )
{
  cmRtNet_t* p = (cmRtNet_t*)cbArg;
  
  if( cmRtNetIsSyncModeMsg(data,dataByteCnt))
    _cmRtNetSyncModeRecv(p,data,dataByteCnt,fromAddr);
  else
    p->cbFunc(p->cbArg,data,dataByteCnt,fromAddr);
  
}



cmRtNetRC_t cmRtNetRegisterLocalNode( cmRtNetH_t h, const cmChar_t* nodeLabel, const cmChar_t* ipAddr, cmUdpPort_t port )
{
  cmRtNet_t*  p = _cmRtNetHandleToPtr(h);
  cmRtNetRC_t rc;
  struct sockaddr_in sockaddr;

  // release the local node
  if( p->localNode != NULL )
  {    
    _cmRtNetReleaseNode(p,p->localNode);
    p->localNode = NULL;
  }

  // if this is the local node then initialze the local socket
  if( cmUdpInit(p->udpH,port,kNonBlockingUdpFl | kBroadcastUdpFl,_cmRtNetRecv,p,NULL,0,p->udpRecvBufByteCnt,p->udpTimeOutMs) != kOkUdpRC )
  {
    rc = cmErrMsg(&p->err,kUdpPortFailNetRC,"The UDP port initialization failed.");
    goto errLabel;
  }

  // get the socket address
  if( cmUdpInitAddr(p->udpH, ipAddr, port, &sockaddr ) != kOkUdpRC )
  {
    rc = cmErrMsg(&p->err,kUdpPortFailNetRC,"IP::port to socket address conversion failed.");
    goto errLabel;
  }

  // create the local node
  if((rc = _cmRtNetCreateNode(p,nodeLabel, ipAddr, port, &sockaddr, kLocalNodeNetFl, 0)) != kOkNetRC )
    goto errLabel;

  // the last created node is always the first node on the list
  p->localNode = p->nodes;

  // begin listening on the local port
  if( cmUdpEnableListen(p->udpH, true ) != kOkUdpRC )
  {
    rc = cmErrMsg(&p->err,kUdpPortFailNetRC,"The UDP port failed to enter 'listen' mode.");
    goto errLabel;
  }


 errLabel:
  return rc;
}

cmRtNetRC_t cmRtNetRegisterEndPoint( cmRtNetH_t h, const cmChar_t* endPtLabel, unsigned endPtId )
{
  cmRtNetRC_t rc = kOkNetRC;
  cmRtNet_t* p = _cmRtNetHandleToPtr(h);

  if( p->localNode == NULL )
    return cmErrMsg(&p->err,kLocalNodeNetRC,"Local endpoints may not be added if a local node has not been defined.");

  if((rc = _cmRtNetCreateEndpoint(p, p->localNode,endPtLabel,endPtId )) == kOkNetRC )
    p->localNode->endPtCnt += 1;

  return rc;
}

cmRtNetRC_t cmRtNetClearAll( cmRtNetH_t h )
{
  cmRtNet_t* p = _cmRtNetHandleToPtr(h);
  _cmRtNetReleaseNodes(p);
  return kOkNetRC;
}

cmRtNetRC_t cmRtNetBeginSyncMode( cmRtNetH_t h )
{
  cmRtNet_t* p = _cmRtNetHandleToPtr(h);

  // broadcast 'node' msg
  return _cmRtNetSendSyncMsg( p, p->localNode, kHelloSelNetId, NULL, p->localNode->endPtCnt );
}



cmRtNetRC_t cmRtNetReceive( cmRtNetH_t h )
{
  cmRtNetRC_t rc = kOkNetRC;
  cmRtNet_t*  p  = _cmRtNetHandleToPtr(h);

  if( cmUdpGetAvailData(p->udpH, NULL, NULL, NULL ) != kOkUdpRC )
  {
    cmErrMsg(&p->err,kUdpPortFailNetRC,"UDP port query failed.");
    goto errLabel;
  }
 errLabel:
  return rc;
}

bool    cmRtNetIsSyncModeMsg( const void* data, unsigned dataByteCnt )
{
  cmRtNetSyncMsg_t* m = (cmRtNetSyncMsg_t*)data;
  return dataByteCnt >= sizeof(cmRtNetSyncMsg_t) && m->hdr.selId == kNetSyncSelRtId;
}


unsigned  cmRtNetEndPointIndex( cmRtNetH_t h, const cmChar_t* nodeLabel, const cmChar_t* endPtLabel )
{
  //cmRtNet_t* p = _cmRtNetHandleToPtr(h);
  return cmInvalidIdx;
}  

cmRtNetRC_t cmRtNetSend( cmRtNetH_t h, unsigned endPointIndex, const void* msg, unsigned msgByteCnt )
{
  cmRtNetRC_t rc = kOkNetRC;
  //cmRtNet_t* p = _cmRtNetHandleToPtr(h);
  return rc;
}

void   cmRtNetReport( cmRtNetH_t h )
{
  cmRtNet_t* p = _cmRtNetHandleToPtr(h);
  cmRpt_t* rpt = p->err.rpt;


  cmRtNetNode_t* np = p->nodes;
  for(; np!=NULL; np=np->link)
  {
    cmRptPrintf(rpt,"Node: %s ",np->label);

    if( np->addr != NULL )
      cmRptPrintf(rpt,"%s ",np->addr );

    if( cmIsFlag(np->flags,kLocalNodeNetFl) )
      cmRptPrintf(rpt,"LOCAL ");

    cmRptPrintf(rpt,"%s ",cmStringNullGuard(cmUdpAddrToString(p->udpH,&np->sockaddr)));

    if( np->port != cmInvalidId )
      cmRptPrintf(rpt,"%i ",np->port );

    cmRptPrintf(rpt,"\n");

    cmRtNetEnd_t* ep = np->ends;
    for(; ep!=NULL; ep=ep->link)
    {
      cmRptPrintf(rpt,"  endpt: %i %s\n",ep->id,cmStringNullGuard(ep->label ));
    }
  }
}


//==========================================================================
#include "cmThread.h"

typedef struct
{
  cmThreadH_t thH;
  cmRtNetH_t netH;
} _cmRtNetTest_t;

void _cmRtNetTestRecv( void* cbArg, const char* data, unsigned dataByteCnt, const struct sockaddr_in* fromAddr )
{
  //_cmRtNetTest_t* p = (_cmRtNetTest_t*)cbArg;


}


bool _cmRtNetTestThreadFunc(void* param)
{
  _cmRtNetTest_t* p = (_cmRtNetTest_t*)param;

  if( cmRtNetIsValid(p->netH) )
  {
    cmRtNetReceive(p->netH);
  }

  cmSleepMs(40);

  return true;
}

void  cmRtNetTest( cmCtx_t* ctx, bool mstrFl )
{
  char            c;
  _cmRtNetTest_t  t;
  const cmChar_t* hostNameStr;
  cmUdpPort_t     port = 5876;
  _cmRtNetTest_t* p    = &t;
  cmRtNetRC_t     rc   = kOkNetRC;
  memset(&t,0,sizeof(t));

  if( cmThreadCreate(&p->thH,_cmRtNetTestThreadFunc,p,&ctx->rpt) != kOkThRC )
    goto errLabel;

  if((rc = cmRtNetAlloc(ctx,&p->netH,_cmRtNetTestRecv,p)) != kOkNetRC )
    goto errLabel;

  hostNameStr = cmRtNetLocalHostName(p->netH);
  if( hostNameStr == NULL )
    hostNameStr = "<no-host-name>";

  if((rc = cmRtNetRegisterLocalNode(p->netH, hostNameStr, NULL, port )) != kOkNetRC)
    goto errLabel;

  if( mstrFl )
  {
    if((rc = cmRtNetRegisterEndPoint(p->netH,"thunk_ep0", 0 )) != kOkNetRC )
      goto errLabel;

    if(( rc = cmRtNetBeginSyncMode(p->netH)) != kOkNetRC )
      goto errLabel;
    
  }
  else
  {
    if((rc = cmRtNetRegisterEndPoint(p->netH,"whirl_ep0", 0 )) != kOkNetRC )
      goto errLabel;
  }
  
  if( cmThreadPause(p->thH,0) != kOkThRC )
    goto errLabel;

  cmRptPrintf(&ctx->rpt,"%s q=quit\n", mstrFl ? "Master: " : "Slave: ");
  while( (c=getchar()) != 'q' )
  {
    switch(c)
    {
      case 'r':
        cmRtNetReport(p->netH);
        break;
    }
    
  }

 errLabel:

  cmThreadDestroy(&p->thH);

  cmRtNetFree(&p->netH);


  return;

}
