#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmUdpPort.h"
#include "cmRtNet.h"
#include "cmTime.h"
#include "cmRtSysMsg.h"
#include "cmText.h"

// flags for cmRtNetNode_t.flags;
enum
{
  kLocalNodeNetFl = 0x01,
  kValidNodeNetFl = 0x02
};

// flags for cmRtNet_t.flags
enum
{
  kReportSyncNetFl = 0x01
};

typedef enum
{
  kHelloSelNetId,         // broadcast msg (label=node label, id=endpt cnt)
  kNodeSelNetId,          // define remote node  (label=remote node label, id=endpt cnt)
  kEndpointSelNetId,      // define remote endpt (label=remote endpt label, id=endpt id)
  kDoneSelNetId,          // declare all endpts sent
  kInvalidSelNetId
} cmRtNetSelId_t;

struct cmRtNetNode_str;

typedef struct cmRtNetEnd_str
{
  cmChar_t*               label;
  unsigned                id;
  struct cmRtNetNode_str* np;   // Owner node.
  struct cmRtNetEnd_str*  link;
} cmRtNetEnd_t;


typedef struct cmRtNetNode_str
{
  cmChar_t*               label;          // Node label.
  struct sockaddr_in      sockaddr;       // Socket address
  cmChar_t*               addr;           // IP Address (human readable)
  cmUdpPort_t             port;           // Socket port
  unsigned                flags;          // See kXXXNodeNetFl flags above.
  unsigned                endPtIdx;       // tracks the next endpoint to send during sync-mode
  unsigned                endPtCnt;       // local-node=actual cnt of endpt's remote-node:expected cnt of endpt's
  cmTimeSpec_t            lastSendTime;   // Time of last message sent
  cmRtNetEnd_t*           ends;           // End point list for this node
  struct cmRtNetNode_str* link;
} cmRtNetNode_t;

typedef struct
{
  cmErr_t         err;                   // Error state object
  unsigned        flags;                 // See kXXXNetFl above.
  cmUdpH_t        udpH;                  // UDP port handle
  cmUdpCallback_t cbFunc;                // Client callback to receive incoming messages from network.
  void*           cbArg;                 //
  cmRtNetNode_t*  nodes;                 // Node list.
  cmRtNetNode_t*  localNode;             // Pointer to local node (which is also in node list)
  unsigned        udpRecvBufByteCnt;     // UDP port receive buffer size.
  unsigned        udpTimeOutMs;          // UDP time-out period
  cmChar_t*       bcastAddr;             // Network broadcast address
} cmRtNet_t;

// Network synchronization message format
typedef struct
{
  cmRtSysMsgHdr_t hdr;    // standard cmRtSys msg  header 
  cmRtNetSelId_t  selId;  // message selector id (See kXXXSelNetId above)
  const cmChar_t* label;  // node     or endpoint label
  unsigned        id;     // endptCnt or endpoint id
} cmRtNetSyncMsg_t;

cmRtNetH_t      cmRtNetNullHandle      = cmSTATIC_NULL_HANDLE;
cmRtNetEndptH_t cmRtNetEndptNullHandle = cmSTATIC_NULL_HANDLE;

cmRtNet_t* _cmRtNetHandleToPtr( cmRtNetH_t h )
{
  cmRtNet_t* p = (cmRtNet_t*)h.h;
  assert( p != NULL );
  return p;
}

void _cmRtNetVRpt( cmRtNet_t* p, const cmChar_t* fmt, va_list vl )
{
  if( cmIsFlag(p->flags,kReportSyncNetFl) )
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
  p->nodes     = NULL;
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

  if( cmTextIsEmpty(label) )
    return cmErrMsg(&p->err,kInvalidLabelNetRC,"A null or blank node label was encountered.");

  if((np = _cmRtNetFindNode(p,label)) != NULL )
    return cmErrMsg(&p->err,kDuplLabelNetRC,"The node label '%s' is already in use.",cmStringNullGuard(label));
  
  np           = cmMemAllocZ(cmRtNetNode_t,1);
  np->label    = cmMemAllocStr(label);

  if( saddr != NULL )
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
  ep->np    = np;
  ep->link  = np->ends;
  np->ends  = ep;

  return rc;
}

unsigned _cmRtNetNodeEndpointCount( cmRtNetNode_t* np )
{
  cmRtNetEnd_t* ep = np->ends;
  unsigned      n  = 0;
  for(; ep!=NULL; ep=ep->link)
    ++n;
  return n;
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
  if( selId == kHelloSelNetId )
    udpRC = cmUdpSend2(p->udpH, buf, n, p->bcastAddr, np->port );
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

  cmMemFree(p->bcastAddr);

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

cmRtNetRC_t  _cmRtNetSendEndpointReplyMsg( cmRtNet_t* p, cmRtNetNode_t* np, cmRtNetSelId_t srcSelId )
{
  cmRtNetRC_t     rc = kOkNetRC;
  cmRtNetEnd_t*   ep;
  const cmChar_t* msgLabel = NULL;
  unsigned        msgId    = cmInvalidId;  
  cmRtNetSelId_t  selId    = kEndpointSelNetId;
  const cmChar_t* rptLabel = "endpoint";

  if( np == NULL )
    return cmErrMsg(&p->err,kNodeNotFoundNetRC,"The net node associated with an endpoint reply was not found.");

  // if we got here by receiving a 'done' msg from the remote node ...
  if( srcSelId == kDoneSelNetId )
  {
    // ... then mark the remote node as having recieved all endpoints
    unsigned n;
    if((n = _cmRtNetNodeEndpointCount(np)) != np->endPtCnt )
      rc = cmErrMsg(&p->err,kNodeEndCntErrNetRC,"The node '%s' expected %i endpoints but received %i.",cmStringNullGuard(np->label),np->endPtCnt,n);
    else
      np->flags = cmSetFlag(np->flags,kValidNodeNetFl);
  }

  // attempt to get the next local endpoint to send ...
  if((ep = _cmRtNetIndexToEndpoint(p,p->localNode,np->endPtIdx)) != NULL )
  {
    msgLabel = ep->label;  // ... send next local endpoint
    msgId    = ep->id;
  }
  else // .... all local endpoints have been sent
  {
    selId    = kInvalidSelNetId;  
    rptLabel = "done";

    // verify that no endpoints are available
    if( np->endPtIdx < p->localNode->endPtCnt )
      rc = cmErrMsg(&p->err,kSyncFailNetRC,"More endpoints are available to send but are not reachable.");
    else
    {
      // if the remote node still has endpts to send then continue
      // sending 'done' messages.
      if( np->endPtIdx==p->localNode->endPtCnt || srcSelId != kDoneSelNetId )
        selId = kDoneSelNetId;
    }   
  }

  // selId is set to kInvalidSelNetId when we encounter the (stop) criteria
  if( selId != kInvalidSelNetId )
  {
    if((rc = _cmRtNetSendSyncMsg(p,np,selId,msgLabel,msgId )) != kOkNetRC )
      rc = cmErrMsg(&p->err,rc,"Send '%s' to %s:%s:%i failed.",rptLabel,cmStringNullGuard(np->label),cmStringNullGuard(np->addr),np->port);
    else
      _cmRtNetRpt(p,"Sent %s.\n",cmStringNullGuard(rptLabel));

    np->endPtIdx += 1;
  }

  return rc;
}

bool    _cmRtNetIsSyncModeMsg( const void* data, unsigned dataByteCnt )
{
  cmRtNetSyncMsg_t* m = (cmRtNetSyncMsg_t*)data;
  return dataByteCnt >= sizeof(cmRtNetSyncMsg_t) && m->hdr.selId == kNetSyncSelRtId;
}

// When the network message recieve function (See cmRtNetAlloc() 'cbFunc') 
// receives a message with the cmRtSysMsgHdr_t.selId == kNetSyncSelRtId
// it should call this function to update the current sync state of the
// cmRtNet.
cmRtNetRC_t  _cmRtNetSyncModeRecv( cmRtNet_t* p, const char* data, unsigned dataByteCnt, const struct sockaddr_in* fromAddr )
{
  cmRtNetRC_t      rc = kOkNetRC;
  cmRtNetSyncMsg_t  m;
  m.label = NULL;

  assert( _cmRtNetIsSyncModeMsg(data,dataByteCnt));
  
  if( _cmRtNetDeserializeSyncMsg(data,dataByteCnt,&m) != kOkNetRC )
  {
    rc = cmErrMsg(&p->err,rc,"Net sync. receive failed due to deserialize fail.");
    goto errLabel;
  }

  _cmRtNetRpt(p,"recv from:%s\n",cmUdpAddrToString(p->udpH, fromAddr ));


  assert( m.hdr.selId == kNetSyncSelRtId );

  // attempt to locate the remote node which sent the msg
  cmRtNetNode_t* np = _cmRtNetFindNodeFromSockAddr(p,fromAddr);


  switch( m.selId )
  {
    case kHelloSelNetId:    
      // if this is a response to a broadcast from the local node then ignore it
      if(m.label!=NULL && p->localNode->label!=NULL && (np = _cmRtNetFindNode(p,m.label)) != NULL && strcmp(p->localNode->label,m.label)==0 )
      {
        const cmChar_t* fromAddrStr  = cmUdpAddrToString(p->udpH,fromAddr);
        const cmChar_t* localAddrStr = cmUdpAddrToString(p->udpH,cmUdpLocalAddr(p->udpH));

        if( fromAddrStr!=NULL && localAddrStr!=NULL && strcmp(fromAddrStr,localAddrStr)!=0)
          cmErrMsg(&p->err,kDuplLocalNetRC,"The node label '%s' appears to be duplicated at address %s and locally.",cmStringNullGuard(m.label),fromAddrStr);

        np->sockaddr = *fromAddr;
        goto errLabel;
      } 
      // fall through

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

        np = p->nodes; // newest node is always the first node

        // send response
        switch( m.selId )
        {
          case kHelloSelNetId:
            _cmRtNetRpt(p,"rcv hello\n"); // reply with local node
            rc = _cmRtNetSendSyncMsg( p, np, kNodeSelNetId, p->localNode->label, p->localNode->endPtCnt );
            break;

          case kNodeSelNetId:
            _cmRtNetRpt(p,"rcv node\n");
            _cmRtNetSendEndpointReplyMsg( p, np, m.selId ); // reply with first endpoint
            break;

          default:
            assert(0);
        }

      }
      break;

    case kDoneSelNetId:
      //case kEndpointAckSelNetId:
      rc = _cmRtNetSendEndpointReplyMsg(p,np,m.selId);
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
        rc = _cmRtNetSendEndpointReplyMsg( p, np, m.selId );
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
  
  if( _cmRtNetIsSyncModeMsg(data,dataByteCnt))
    _cmRtNetSyncModeRecv(p,data,dataByteCnt,fromAddr);
  else
    p->cbFunc(p->cbArg,data,dataByteCnt,fromAddr);
  
}

cmRtNetRC_t cmRtNetInitialize( cmRtNetH_t h, const cmChar_t* bcastAddr, const cmChar_t* nodeLabel, const cmChar_t* ipAddr, cmUdpPort_t port )
{
  cmRtNet_t*  p = _cmRtNetHandleToPtr(h);
  cmRtNetRC_t rc;

  // release the local node
  if((rc = cmRtNetFinalize(h)) != kOkNetRC )
    goto errLabel;

  if( cmTextIsEmpty(bcastAddr) )
  {
    rc = cmErrMsg(&p->err,kInvalidArgNetRC,"The 'broadcast address' is not valid.");
    goto errLabel;
  }

  // if this is the local node then initialze the local socket
  if( cmUdpInit(p->udpH,port,kNonBlockingUdpFl | kBroadcastUdpFl,_cmRtNetRecv,p,NULL,0,p->udpRecvBufByteCnt,p->udpTimeOutMs) != kOkUdpRC )
  {
    rc = cmErrMsg(&p->err,kUdpPortFailNetRC,"The UDP port initialization failed.");
    goto errLabel;
  }

  // create the local node
  if((rc = _cmRtNetCreateNode(p,nodeLabel, ipAddr, port, NULL, kLocalNodeNetFl, 0)) != kOkNetRC )
    goto errLabel;

  // the last created node is always the first node on the list
  p->localNode = p->nodes;
  p->bcastAddr = cmMemResizeStr(p->bcastAddr,bcastAddr);

  // begin listening on the local port
  if( cmUdpEnableListen(p->udpH, true ) != kOkUdpRC )
  {
    rc = cmErrMsg(&p->err,kUdpPortFailNetRC,"The UDP port failed to enter 'listen' mode.");
    goto errLabel;
  }


 errLabel:
  return rc;
}


bool cmRtNetIsInitialized( cmRtNetH_t h )
{
  if( cmRtNetIsValid(h) == false )
    return false;

  cmRtNet_t* p = _cmRtNetHandleToPtr(h);
  return p->localNode != NULL && cmTextIsNotEmpty(p->bcastAddr);
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

cmRtNetRC_t cmRtNetFinalize( cmRtNetH_t h )
{
  cmRtNet_t* p = _cmRtNetHandleToPtr(h);
  _cmRtNetReleaseNodes(p);
  
  return kOkNetRC;
}

cmRtNetRC_t cmRtNetDoSync( cmRtNetH_t h )
{
  cmRtNet_t* p = _cmRtNetHandleToPtr(h);

  // broadcast 'node' msg
  return _cmRtNetSendSyncMsg( p, p->localNode, kHelloSelNetId, p->localNode->label, p->localNode->endPtCnt );
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

cmRtNetRC_t cmRtNetEndpointHandle( cmRtNetH_t h, const cmChar_t* nodeLabel, const cmChar_t* endptLabel, cmRtNetEndptH_t* hp )
{
  cmRtNetRC_t     rc = kOkNetRC;
  cmRtNet_t*      p  = _cmRtNetHandleToPtr(h);
  cmRtNetNode_t* np;
  cmRtNetEnd_t*  ep;

  if(( np = _cmRtNetFindNode(p,nodeLabel)) == NULL )
    return cmErrMsg(&p->err,kNodeNotFoundNetRC,"The node '%s' was not found.",cmStringNullGuard(nodeLabel));


  if(( ep = _cmRtNetFindNodeEnd(np, endptLabel )) == NULL )
    return cmErrMsg(&p->err,kEndNotFoundNetRC,"The endpoint '%s' on '%s' on node was not found.",cmStringNullGuard(endptLabel),cmStringNullGuard(nodeLabel));

  hp->h = ep;

  return rc;
}

cmRtNetRC_t cmRtNetSend( cmRtNetH_t h, cmRtNetEndptH_t epH, const void* msg, unsigned msgByteCnt )
{
  cmRtNetRC_t     rc = kOkNetRC;
  cmRtNet_t*      p  = _cmRtNetHandleToPtr(h);
  cmRtNetEnd_t*   ep = (cmRtNetEnd_t*)epH.h;
  
  assert( ep != NULL );
  
  unsigned dN = sizeof(unsigned) + msgByteCnt; 
  char data[ dN ];
  unsigned *hdr = (unsigned*)data;
  hdr[0] = ep->id;
  memcpy(hdr+1,msg,msgByteCnt);
  
  if( cmUdpSendTo(p->udpH, data, dN, &ep->np->sockaddr ) != kOkUdpRC )
    return cmErrMsg(&p->err,kUdpPortFailNetRC,"Send to node:%s endpt:%s failed.\n",cmStringNullGuard(ep->np->label),cmStringNullGuard(ep->label));

  return rc;
}

cmRtNetRC_t cmRtNetSendByLabels( cmRtNetH_t h, const cmChar_t* nodeLabel, const cmChar_t* endptLabel, const void* msg, unsigned msgByteCnt )
{
  cmRtNetRC_t     rc  = kOkNetRC;
  cmRtNetEndptH_t epH = cmRtNetEndptNullHandle;

  if((rc = cmRtNetEndpointHandle(h,nodeLabel,endptLabel,&epH)) != kOkNetRC )
    return rc;

  return cmRtNetSend(h,epH,msg,msgByteCnt);
}



bool cmRtNetReportSyncEnable( cmRtNetH_t h, bool enableFl )
{
  cmRtNet_t* p  = _cmRtNetHandleToPtr(h);
  bool       fl =  cmIsFlag(p->flags,kReportSyncNetFl);
  p->flags = cmEnaFlag(p->flags,kReportSyncNetFl,enableFl);
  return fl;  
}

bool cmRtNetReportSyncIsEnabled( cmRtNetH_t h )
{
  cmRtNet_t* p = _cmRtNetHandleToPtr(h);
  return cmIsFlag(p->flags,kReportSyncNetFl);
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
  cmRtNetH_t  netH;
  unsigned    msgVal;
} _cmRtNetTest_t;

void _cmRtNetTestRecv( void* cbArg, const char* data, unsigned dataByteCnt, const struct sockaddr_in* fromAddr )
{
  //_cmRtNetTest_t* p = (_cmRtNetTest_t*)cbArg;
  
  unsigned* hdr = (unsigned*)data;
  printf("%i %i\n",hdr[0],hdr[1]);

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
  cmUdpPort_t     port = 5876;
  _cmRtNetTest_t* p    = &t;
  cmRtNetRC_t     rc   = kOkNetRC;
  const cmChar_t* localHostStr   = mstrFl  ? "master"    : "slave";
  const cmChar_t* localEndpStr   = mstrFl  ? "master_ep" : "slave_ep";
  const cmChar_t* remoteHostStr  = !mstrFl ? "master"    : "slave";
  const cmChar_t* remoteEndpStr  = !mstrFl ? "master_ep" : "slave_ep";
  const cmChar_t* bcastAddr      = "192.168.15.255";

  memset(&t,0,sizeof(t));

  if( cmThreadCreate(&p->thH,_cmRtNetTestThreadFunc,p,&ctx->rpt) != kOkThRC )
    goto errLabel;

  if((rc = cmRtNetAlloc(ctx,&p->netH,_cmRtNetTestRecv,p)) != kOkNetRC )
    goto errLabel;

  cmRtNetReportSyncEnable(p->netH,true); // enable sync. protocol reporting

  if((rc = cmRtNetInitialize(p->netH, bcastAddr, localHostStr, NULL, port )) != kOkNetRC)
    goto errLabel;

  if((rc = cmRtNetRegisterEndPoint(p->netH,localEndpStr, 0 )) != kOkNetRC )
    goto errLabel;
  
  if( cmThreadPause(p->thH,0) != kOkThRC )
    goto errLabel;

  cmRptPrintf(&ctx->rpt,"%s t=transmit s=sync r=report q=quit\n", localHostStr );

  while( (c=getchar()) != 'q' )
  {
    switch(c)
    {
      case 'r':
        cmRtNetReport(p->netH);
        break;

      case 's':
        cmRtNetDoSync(p->netH);
        break;

      case 't':
        {
          if( cmRtNetSendByLabels(p->netH, remoteHostStr, remoteEndpStr, &p->msgVal, sizeof(p->msgVal)) == kOkNetRC )
            p->msgVal += 1;

        }        
        break;
    }
    
  }

 errLabel:

  cmThreadDestroy(&p->thH);

  cmRtNetFree(&p->netH);


  return;

}
