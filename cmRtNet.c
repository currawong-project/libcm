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
  kLocalNetFl = 0x01,
  kSockAddrNetFl = 0x02
};

typedef enum
{
  kSendHelloStNetId = 0,
  kWaitHelloAckStNetId,
  kSendEndpointStNetId,
  kWaitEndpointAckStNetId,
  kDoneStNetId,
  kErrorStNetId,
  kInvalidStNetId,
} cmRtNetNodeState_t;

typedef enum
{
  kHelloSelNetId,
  kHelloAckSelNetId,
  kEndpointSelNetId,
  kEndpointAckSelNetId
} cmRtNetSelId_t;

typedef struct cmRtNetEnd_str
{
  cmChar_t*              endPtLabel;
  unsigned               endPtId;
  struct cmRtNetEnd_str* link;
} cmRtNetEnd_t;


typedef struct cmRtNetNode_str
{
  cmChar_t*               label;
  struct sockaddr_in      sockaddr;
  cmChar_t*               addr;
  cmUdpPort_t             port;
  unsigned                flags;
  cmRtNetNodeState_t      state;
  unsigned                epIdx;
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
  bool            syncModeFl;
  unsigned        udpRecvBufByteCnt;
  unsigned        udpTimeOutMs;
  unsigned        interSyncSendTimeMs;
} cmRtNet_t;


typedef struct
{
  cmRtSysMsgHdr_t hdr;
  cmRtNetSelId_t  selId;
  const cmChar_t* endPtLabel;
  unsigned        endPtId;
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
    if( cmIsFlag(np->flags,kSockAddrNetFl) && np->sockaddr.sin_addr.s_addr == saddr->sin_addr.s_addr && np->sockaddr.sin_port == saddr->sin_port )
      return np;
  
  return NULL;
}

void _cmRtNetFreeNode( cmRtNetNode_t* np )
{
  cmRtNetEnd_t* ep = np->ends;
  while( ep != NULL )
  {
    cmRtNetEnd_t* nep = ep->link;
    cmMemFree(ep->endPtLabel);
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
  // we should never release the local node via this function
  assert( np != p->localNode );

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

cmRtNetRC_t _cmRtNetCreateNode( cmRtNet_t* p, const cmChar_t* label, const cmChar_t* addr, cmUdpPort_t port, const struct sockaddr_in* saddr )
{
  cmRtNetNode_t* np;

  if( label == NULL )
    return cmErrMsg(&p->err,kInvalidLabelNetRC,"A null or blank node label was encountered.");

  if((np = _cmRtNetFindNode(p,label)) != NULL )
    return cmErrMsg(&p->err,kDuplLabelNetRC,"The node label '%s' is already in use.",cmStringNullGuard(label));

  bool localNodeFl = addr==NULL && saddr==NULL;
  
  if( localNodeFl && p->localNode != NULL )
    return cmErrMsg(&p->err,kDuplLocalNetRC,"The local node '%s' has already been set.",cmStringNullGuard(p->localNode->label));

  np = cmMemAllocZ(cmRtNetNode_t,1);
  np->label = cmMemAllocStr(label);
  np->addr  = addr==NULL ? NULL : cmMemAllocStr(addr);
  np->port  = port;
  np->flags = cmEnaFlag(np->flags,kLocalNetFl,localNodeFl);
  np->link  = p->nodes;
  p->nodes  = np;

  if( localNodeFl )
    p->localNode = np;

  if( saddr != NULL )
  {
    np->sockaddr = *saddr;
    np->flags = cmSetFlag(np->flags,kSockAddrNetFl);
  }

  return kOkNetRC;
}

cmRtNetEnd_t* _cmRtNetFindNodeEnd(cmRtNetNode_t* np, const cmChar_t* endPtLabel )
{
  cmRtNetEnd_t* ep = np->ends;
  for(; ep!=NULL; ep=ep->link)
    if( strcmp(ep->endPtLabel,endPtLabel)==0 )
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

  ep->endPtLabel = cmMemAllocStr(endPtLabel);
  ep->endPtId    = endPtId;
  ep->link      = np->ends;
  np->ends      = ep;

  return rc;
}

unsigned _cmRtNetSyncMsgSerialByteCount( const cmRtNetSyncMsg_t* m )
{ return sizeof(cmRtNetSyncMsg_t) + m->endPtLabel==NULL ? 1 : strlen(m->endPtLabel) + 1; }

cmRtNetRC_t _cmRtNetSerializeSyncMsg( cmRtNet_t* p, const cmRtNetSyncMsg_t* m, void* buf, unsigned n )
{
  unsigned bn = _cmRtNetSyncMsgSerialByteCount(m);
  char*    b  = (char*)buf;

  if( bn > n )
    return cmErrMsg(&p->err,kBufToSmallNetRC,"Serialize buffer too small.");

  memcpy(b,m,sizeof(*m));
  strcpy(b + sizeof(*m),m->endPtLabel==NULL ? "" : m->endPtLabel);
  return kOkNetRC;
}

cmRtNetRC_t _cmRtNetDeserializeSyncMsg( const void* buf, unsigned n, cmRtNetSyncMsg_t* m )
{
  assert( n > sizeof(*m));
  memcpy(m,buf,sizeof(*m));
  const cmRtNetSyncMsg_t* mp = (const cmRtNetSyncMsg_t*)buf;
  const cmChar_t*   s  = (const cmChar_t*)(mp+1);
  m->endPtLabel = cmMemAllocStr(s);
  return kOkNetRC;
}

cmRtNetRC_t _cmRtNetSendSyncMsg( cmRtNet_t* p, cmRtNetNode_t* np, cmRtNetSelId_t selId, const cmChar_t* endPtLabel, unsigned endPtId, cmRtNetNodeState_t nextStId )
{
  cmRtNetSyncMsg_t m;
  cmRtNetRC_t      rc = kOkNetRC;

  m.hdr.rtSubIdx = cmInvalidIdx;
  m.hdr.selId    = kNetSyncSelRtId;
  m.selId        = selId;
  m.endPtLabel    = endPtLabel;
  m.endPtId       = endPtId;

  // determine size of msg to send
  unsigned n  = _cmRtNetSyncMsgSerialByteCount(&m);
  cmChar_t buf[n];

  // serialize msg into buf[]
  if((rc = _cmRtNetSerializeSyncMsg(p,&m,buf,n)) != kOkNetRC )
    return rc;

  // store this nodes current sync state
  cmRtNetNodeState_t orgState = np->state;
  np->state = nextStId;

  // send the msg
  cmUdpRC_t udpRC;
  if( cmIsFlag(np->flags,kSockAddrNetFl) )
    udpRC = cmUdpSendTo(p->udpH, buf, n, &np->sockaddr );
  else
    udpRC = cmUdpSend2(p->udpH, buf, n, np->addr, np->port );

  // check for send errors
  if( udpRC != kOkUdpRC )
  {
    rc = cmErrMsg(&p->err,kUdpPortFailNetRC,"Sync msg. send on UDP port failed.");
    np->state = orgState;  // restore node state so we can try again
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

bool cmRtNetIsValid( cmRtNetH_t h )
{ return h.h !=NULL; }

cmUdpH_t  cmRtNetUdpPortHandle( cmRtNetH_t h )
{
  cmRtNet_t*  p = _cmRtNetHandleToPtr(h);
  return p->udpH;
}


cmRtNetRC_t cmRtNetCreateNode( cmRtNetH_t h, const cmChar_t* nodeLabel, const cmChar_t* ipAddr, cmUdpPort_t port )
{
  cmRtNet_t*  p = _cmRtNetHandleToPtr(h);
  cmRtNetRC_t rc;

  // create a node
  if((rc = _cmRtNetCreateNode(p,nodeLabel,ipAddr, port, NULL)) != kOkNetRC )
    return rc;

  // if this is not the local node
  if( ipAddr != NULL )
    return rc;

  // if this is the local node then initialze the local socket
  if( cmUdpInit(p->udpH,port,kNonBlockingUdpFl,p->cbFunc,p->cbArg,NULL,0,p->udpRecvBufByteCnt,p->udpTimeOutMs) != kOkUdpRC )
  {
    rc = cmErrMsg(&p->err,kUdpPortFailNetRC,"The UDP port initialization failed.");
    goto errLabel;
  }

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
  cmRtNet_t* p = _cmRtNetHandleToPtr(h);

  if( p->localNode == NULL )
    return cmErrMsg(&p->err,kLocalNodeNetRC,"Local endpoints may not be added if a local node has not been defined.");

  return _cmRtNetCreateEndpoint(p, p->localNode,endPtLabel,endPtId );

}

cmRtNetRC_t cmRtNetClearAll( cmRtNetH_t h )
{
  cmRtNet_t* p = _cmRtNetHandleToPtr(h);
  _cmRtNetReleaseNodes(p);
  return kOkNetRC;
}

cmRtNetRC_t cmRtNetBeginSyncMode( cmRtNetH_t h )
{
  cmRtNetRC_t rc = kOkNetRC;
  
  cmRtNet_t* p = _cmRtNetHandleToPtr(h);

  p->syncModeFl = true;
  
  return rc;
}

bool      cmRtNetIsInSyncMode(  cmRtNetH_t h )
{
  cmRtNet_t* p = _cmRtNetHandleToPtr(h);

  return p->syncModeFl;
}


// Used by slaves to send the master an 'ack' msg.
cmRtNetRC_t _cmRtNetSendAck( cmRtNet_t* p, cmRtNetSelId_t ackSelId, const struct sockaddr_in* saddr )
{
  cmRtNetNode_t* np;

  if((np = _cmRtNetFindNodeFromSockAddr(p,saddr)) != NULL )
    return cmErrMsg(&p->err,kNodeNotFoundNetRC,"The net node associated with an ack cmd was not found. Ack not sent.");

  return _cmRtNetSendSyncMsg(p,np,ackSelId,NULL,cmInvalidId,kInvalidStNetId);
}

// Used by master to update state upon receipt of 'ack' msg
cmRtNetRC_t _cmRtNetRecvAck( cmRtNet_t* p, const struct sockaddr_in* fromAddr, cmRtNetNodeState_t expectedState, cmRtNetNodeState_t nextState )
{
  cmRtNetNode_t* np;
  cmRtNetRC_t rc = kOkNetRC;

  if((np = _cmRtNetFindNodeFromSockAddr(p,fromAddr)) == NULL )
  {
    rc = cmErrMsg(&p->err,kNodeNotFoundNetRC,"The net node associated with a  ack receive was not found.");
    goto errLabel;
  }

  if( np->state != expectedState )
  {
    rc = cmErrMsg(&p->err,kNodeStateErrNetRC,"Node '%s' expected in state %i was in state %i.",kWaitHelloAckStNetId,np->state);
    np->state = kErrorStNetId;
    goto errLabel;
  }

  np->state = nextState;

 errLabel:
  return rc;
}


cmRtNetRC_t  cmRtNetSyncModeRecv( cmRtNetH_t h, const char* data, unsigned dataByteCnt, const struct sockaddr_in* fromAddr )
{
  cmRtNet_t*       p  = _cmRtNetHandleToPtr(h);
  cmRtNetRC_t      rc = kOkNetRC;
  cmRtNetNode_t*   np = NULL;
  cmRtNetSyncMsg_t m;

  assert( p->syncModeFl );
  
  if( _cmRtNetDeserializeSyncMsg(data,dataByteCnt,&m) != kOkNetRC )
  {
    rc = cmErrMsg(&p->err,rc,"Net sync. receive failed due to deserialize fail.");
    goto errLabel;
  }

  assert( m.hdr.selId == kNetSyncSelRtId );

  switch( m.selId )
  {
    
    case kHelloSelNetId: // slave response
      {
        _cmRtNetRpt(p,"rcv hello\n");

        // attempt to locate the remote node which sent the endpoint 
        if((np = _cmRtNetFindNodeFromSockAddr(p,fromAddr)) != NULL )
        {
          // delete the existing node because we are about to get new info. about it.
          if((rc =  _cmRtNetReleaseNode(p,np )) != kOkNetRC )
            goto errLabel;
        }

        //  create a node proxy to represent the remote node
        if(( rc = _cmRtNetCreateNode(p,m.endPtLabel,NULL,0,fromAddr)) != kOkNetRC )
          goto errLabel;

        // send an ackknowledgement of the 'hello' msg
        rc = _cmRtNetSendAck(p,kHelloAckSelNetId,fromAddr);
      }
      break;

    
    case kEndpointSelNetId: // slave response
      {
        cmRtNetEnd_t* ep;

        _cmRtNetRpt(p,"rcv endpoint\n");


        // locate the remote node which sent the endpoint
        if((np = _cmRtNetFindNodeFromSockAddr(p,fromAddr)) == NULL )
        {
          rc = cmErrMsg(&p->err,kNodeNotFoundNetRC,"The net node associated with an endpoint receive was not found.");
          goto errLabel;
        }
        
        // attempt to find the end point 
        if((ep = _cmRtNetFindNodeEnd(np,m.endPtLabel)) != NULL )
          ep->endPtId = m.endPtId; // the endpoint was found update the endPtId
        else
        {
          // create a local proxy for the endpoint
          if((rc = _cmRtNetCreateEndpoint(p,np,m.endPtLabel,m.endPtId)) != kOkNetRC )
            goto errLabel;
        }

        // ack. the endpoint msg
        rc = _cmRtNetSendAck(p,kEndpointAckSelNetId,fromAddr);
      }
      break;

    case kHelloAckSelNetId: // master response
      _cmRtNetRpt(p,"rcv hello ack\n");
      rc = _cmRtNetRecvAck(p,fromAddr,kWaitHelloAckStNetId,kSendEndpointStNetId);
      break;

    case kEndpointAckSelNetId: // master response
      _cmRtNetRpt(p,"rcv endpoint ack\n");
      rc = _cmRtNetRecvAck(p,fromAddr,kWaitEndpointAckStNetId,kSendEndpointStNetId);
      break;

    default:
      break;
  }

 errLabel:
  return rc;
}


cmRtNetRC_t _cmRtNetSendNodeSync( cmRtNet_t* p, cmRtNetNode_t* np )
{
  cmRtNetRC_t rc = kOkNetRC;

  switch( np->state )
  {
    case kSendHelloStNetId:
      // send a 'hello' to this remote node
      if((rc = _cmRtNetSendSyncMsg(p,np,kHelloSelNetId,p->localNode->label, cmInvalidId, kWaitHelloAckStNetId )) != kOkNetRC )
        rc = cmErrMsg(&p->err,rc,"Send 'hello' to %s:%s:%i failed.",cmStringNullGuard(np->label),cmStringNullGuard(np->addr),np->port);
      else
        _cmRtNetRpt(p,"send hello\n");
      break;

    case kSendEndpointStNetId:
      {
        cmRtNetEnd_t* ep;

        // if all of the endpoints have been sent to this node ...
        if((ep = _cmRtNetIndexToEndpoint(p,p->localNode,np->epIdx)) == NULL )
          np->state = kDoneStNetId; // ... we are done
        else
        {
          // send an endpoint to this node 
          if((rc = _cmRtNetSendSyncMsg(p,np,kHelloSelNetId,ep->endPtLabel, ep->endPtId, kWaitEndpointAckStNetId )) != kOkNetRC )
            rc = cmErrMsg(&p->err,rc,"Endpoint (%s index:%i) transmission to %s:%s:%i failed.",cmStringNullGuard(ep->endPtLabel),cmStringNullGuard(np->label),cmStringNullGuard(np->addr),np->port);
          else
            _cmRtNetRpt(p,"send endpoint\n");

        }
      }
      break;

    case kWaitHelloAckStNetId:
    case kWaitEndpointAckStNetId:
      {
        cmTimeSpec_t t;
        cmTimeGet(&t);
        unsigned fiveSecs = 5000000;
        if( cmTimeElapsedMicros(&np->lastSendTime,&t) > fiveSecs)
        {
          const cmChar_t* ackStr = np->state==kWaitHelloAckStNetId ? "hello" : "endpoint";
          rc = cmErrMsg(&p->err,kTimeOutErrNetRC,"The node %s:%s:%i did not give a '%s' acknowledge.",cmStringNullGuard(np->label),cmStringNullGuard(np->addr),np->port,ackStr);         
        }
      }
      break;

    default:
      break;
  }
      
  // if an error occurred put the node into an error state
  if( rc != kOkNetRC )
    np->state = kErrorStNetId;
      
  return rc;
}



cmRtNetRC_t cmRtNetSyncModeSend( cmRtNetH_t h )
{
  cmRtNetRC_t rc = kOkNetRC;
  cmRtNet_t*  p  = _cmRtNetHandleToPtr(h);

  if( p->syncModeFl == false )
    return rc;
  
  unsigned     activeCnt = 0;
  cmRtNetNode_t* np        = p->nodes;
  for(; np != NULL; np=np->link )
    if( np != p->localNode && np->state != kDoneStNetId && np->state != kErrorStNetId )
    {
      _cmRtNetSendNodeSync(p,np);
      activeCnt += 1;
    }
    
  if( activeCnt == 0 )
    p->syncModeFl = false;

  return rc;
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

  cmRptPrintf(rpt,"Sync Mode:%s\n",p->syncModeFl ? "ON" : "OFF");

  cmRtNetNode_t* np = p->nodes;
  for(; np!=NULL; np=np->link)
  {
    cmRptPrintf(rpt,"Node: %s ",np->label);

    if( np->addr != NULL )
      cmRptPrintf(rpt,"%s ",np->addr );

    if( cmIsFlag(np->flags,kLocalNetFl) )
      cmRptPrintf(rpt,"LOCAL ");

    if( cmIsFlag(np->flags,kSockAddrNetFl) )
      cmRptPrintf(rpt,"%s ",cmStringNullGuard(cmUdpAddrToString(p->udpH,&np->sockaddr)));

    if( np->port != cmInvalidId )
      cmRptPrintf(rpt,"%i ",np->port );

    cmRptPrintf(rpt,"\n");

    cmRtNetEnd_t* ep = np->ends;
    for(; ep!=NULL; ep=ep->link)
    {
      cmRptPrintf(rpt,"  endpt: %i %s\n",ep->endPtId,cmStringNullGuard(ep->endPtLabel));
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
  _cmRtNetTest_t* p = (_cmRtNetTest_t*)cbArg;

  if( cmRtNetIsSyncModeMsg(data,dataByteCnt))
    cmRtNetSyncModeRecv(p->netH,data,dataByteCnt,fromAddr);

}


bool _cmRtNetTestThreadFunc(void* param)
{
  _cmRtNetTest_t* p = (_cmRtNetTest_t*)param;

  
  if( cmRtNetIsValid(p->netH) && cmRtNetIsInSyncMode(p->netH) )
    cmRtNetSyncModeSend(p->netH);

  return true;
}

void  cmRtNetTest( cmCtx_t* ctx, bool mstrFl )
{
  char c;
  _cmRtNetTest_t t;
  cmUdpPort_t port = 5876;
  _cmRtNetTest_t* p = &t;
  cmRtNetRC_t rc = kOkNetRC;
  memset(&t,0,sizeof(t));

  if( cmThreadCreate(&p->thH,_cmRtNetTestThreadFunc,p,&ctx->rpt) != kOkThRC )
    goto errLabel;

  if((rc = cmRtNetAlloc(ctx,&p->netH,_cmRtNetTestRecv,p)) != kOkNetRC )
    goto errLabel;

  if((rc = cmRtNetCreateNode(p->netH, "local", NULL, port )) != kOkNetRC)
    goto errLabel;

  if( mstrFl )
  {
    if((rc = cmRtNetCreateNode(p->netH,"whirl", "192.168.15.109", port )) != kOkNetRC )
      goto errLabel;

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

  while( (c=getchar()) != 'q' )
  {
    
  }

 errLabel:

  cmRtNetFree(&p->netH);

  cmThreadDestroy(&p->thH);
  return;

}
