#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmJson.h"
#include "cmThread.h"
#include "cmUdpPort.h"
#include "cmUdpNet.h"
#include "cmVirtNet.h"
#include "cmText.h"   // use by cmVnTest()

enum { kOwnerVnFl = 0x01, kLocalVnFl = 0x02 };

struct cmVn_str;

typedef struct cmVnNode_str
{
  cmChar_t*            label;
  unsigned             id;
  unsigned             flags;
  cmVnH_t              vnH;     // handle of a local virtual net that is owned by this node
  cmTsMp1cH_t          qH;      // MPSC non-blocking queue to hold incoming msg's from local virt. net's
  struct cmVn_str*     p;       // pointer back to the cmVn_t which owns this NULL
  struct cmVnNode_str* link;
} cmVnNode_t;

typedef struct cmVn_str
{
  cmErr_t     err;
  cmUdpNetH_t udpH;
  cmVnNode_t* nodes;
  cmVnCb_t    cbFunc;
  void*       cbArg;
  unsigned    ownerNodeId;
} cmVn_t;

cmVnH_t cmVnNullHandle = cmSTATIC_NULL_HANDLE;

cmVn_t* _cmVnHandleToPtr( cmVnH_t h )
{
  cmVn_t* p = (cmVn_t*)h.h;
  assert(p != NULL);
  return p;
}

void _cmVnNodeFree( cmVnNode_t* np )
{
  if( np == NULL )
    return;
  
  if( cmTsMp1cIsValid(np->qH) )
    cmTsMp1cDestroy(&np->qH);
  
  np->vnH = cmVnNullHandle;

  cmMemFree(np->label);
  cmMemFree(np);
}

cmVnRC_t _cmVnDestroy( cmVn_t* p )
{
  cmVnRC_t rc = kOkVnRC;

  if( cmUdpNetFree(&p->udpH) != kOkUnRC )
    return cmErrMsg(&p->err,kUdpNetFailVnRC,"The UDP network release failed.");

  cmVnNode_t* np = p->nodes;
  while( np != NULL )
  {
    cmVnNode_t* nnp = np->link;
    _cmVnNodeFree(np);
    np = nnp;
  }

  cmMemFree(p);

  return rc;
}

// This function is called by cmUdpNetReceive() which is called by cmVnReceive().
void _cmVnUdpNetCb( void* cbArg, cmUdpNetH_t h, const char* data, unsigned dataByteCnt, unsigned remoteNodeId )
{
  cmVnNode_t* np = (cmVnNode_t*)cbArg;
  assert( np->id == remoteNodeId );
  np->p->cbFunc(np->p->cbArg,np->id,dataByteCnt,data);
}


cmVnRC_t _cmVnSend( cmVn_t* p, cmVnNode_t* np, unsigned byteCnt, const cmChar_t* buf )
{
  cmVnRC_t rc = kOkVnRC;

  // if the node is local then send the msg directly
  if( cmIsFlag(np->flags,kLocalVnFl)  )
  {
    if( cmVnIsValid(np->vnH) )
      rc = cmVnRecvFromLocal(np->vnH,p->ownerNodeId,byteCnt,buf);
  }
  else
  {
    // if the node is remote then send the msg via UDP
    if( cmUdpNetSendById(p->udpH,np->id,buf,byteCnt) != kOkUnRC )
      rc = cmErrMsg( &p->err, kUdpNetFailVnRC, "UDP Net send to remote node '%s' failed.",cmStringNullGuard(np->label));
  }

  return rc;
}

cmVnNode_t* _cmVnNodeFindById( cmVn_t* p, unsigned id )
{
  cmVnNode_t* np = p->nodes;
  for(; np!=NULL; np=np->link)
    if( np->id == id )
      return np;

  return NULL;
}

cmVnNode_t* _cmVnNodeFindByLabel( cmVn_t* p, const cmChar_t* label )
{
  cmVnNode_t* np = p->nodes;
  for(; np!=NULL; np=np->link)
    if( strcmp(np->label,label)==0 )
      return np;

  return NULL;
}

cmVnNode_t* _cmVnNodeFindOwner( cmVn_t* p )
{
  cmVnNode_t* np = p->nodes;
  for(; np!=NULL; np=np->link)
    if( cmIsFlag(np->flags,kOwnerVnFl) )
      return np;

  return NULL;
}

cmVnRC_t  _cmVnCreateNode( cmVn_t* p, const cmChar_t* label, unsigned nodeId, unsigned flags, cmVnNode_t** npp )
{
  cmVnNode_t* np;

  if((np = _cmVnNodeFindById(p,nodeId)) != NULL )
    return cmErrMsg(&p->err,kDuplicateNodeIdVnRC,"The node id=%i is already in use for '%s'.",cmStringNullGuard(np->label));

  if((np = _cmVnNodeFindByLabel(p,label)) != NULL )
    return cmErrMsg(&p->err,kDuplicateNodeLabelVnRC,"The node label '%s' is already used by another node.",cmStringNullGuard(label));

  np         = cmMemAllocZ(cmVnNode_t,1);
  np->label  = cmMemAllocStr(label);
  np->id     = nodeId;
  np->flags  = flags;
  np->p      = p;

  *npp = np;

  return kOkVnRC;
}


void _cmVnNodeLink( cmVn_t* p, cmVnNode_t* np )
{
  cmVnNode_t* cnp = p->nodes;
  if( cnp == NULL )
    p->nodes = np;
  else
  {
    while( cnp->link != NULL )
      cnp = cnp->link;

    cnp->link = np;
  }
}

cmVnRC_t _cmVnCreateOwnerNode(  cmVn_t* p, const cmChar_t* label, unsigned nodeId, unsigned udpRecvBufByteCnt, unsigned udpRecvTimeOutMs, const cmChar_t* ipAddr, unsigned ipPort )
{
  cmVnRC_t    rc = kOkVnRC;
  cmVnNode_t* np;

  // create a generic node
  if((rc = _cmVnCreateNode(p, label,nodeId, kOwnerVnFl, &np )) != kOkVnRC )
    goto errLabel;

  // initialize the UDP net with the owner node
  if( cmUdpNetInit(p->udpH,label,nodeId,ipPort,_cmVnUdpNetCb,np,udpRecvBufByteCnt,udpRecvTimeOutMs) != kOkUnRC )
  {
    rc = cmErrMsg(&p->err,kUdpNetFailVnRC,"UDP network initialization failed for node:'%s'.",cmStringNullGuard(label));
    goto errLabel;
  }

  _cmVnNodeLink(p,np);

  p->ownerNodeId = nodeId;

 errLabel:
  if( rc != kOkVnRC )
    _cmVnNodeFree(np);
  return rc;
}

//------------------------------------------------------------------------------------------------------------

cmVnRC_t cmVnCreate( cmCtx_t* ctx, cmVnH_t* hp, cmVnCb_t cbFunc, void* cbArg, const cmChar_t* label, unsigned nodeId, unsigned udpRecvBufByteCnt, unsigned udpRecvTimeOutMs, const cmChar_t* ipAddr, unsigned ipPort  )
{
  cmVnRC_t rc;
  if((rc = cmVnDestroy(hp)) != kOkVnRC )
    return rc;

  cmVn_t* p = cmMemAllocZ(cmVn_t,1);

  cmErrSetup(&p->err,&ctx->rpt,"cmVirtNet");

  if( cmUdpNetAlloc(ctx,&p->udpH) != kOkUnRC )
  {
    rc = cmErrMsg(&p->err,kUdpNetFailVnRC,"The UDP network allocation failed.");
    goto errLabel;
  }

  // create the owner node
  if((rc = _cmVnCreateOwnerNode(p,label,nodeId, udpRecvBufByteCnt, udpRecvTimeOutMs, ipAddr, ipPort )) != kOkVnRC )
    goto errLabel;

  p->ownerNodeId = nodeId;
  p->cbFunc      = cbFunc;
  p->cbArg       = cbArg;

  hp->h = p;

 errLabel:

  if( rc != kOkUdpRC )
    _cmVnDestroy(p);

  return rc;
}

cmVnRC_t cmVnDestroy( cmVnH_t* hp )
{
  cmVnRC_t rc = kOkVnRC;

  if( hp == NULL || cmVnIsValid(*hp)==false )
    return rc;

  cmVn_t* p = _cmVnHandleToPtr(*hp);

  if((rc = _cmVnDestroy(p)) != kOkVnRC )
    return rc;

  hp->h = NULL;

  return rc;
}

bool     cmVnIsValid( cmVnH_t h )
{ return h.h != NULL; }

cmVnRC_t cmVnEnableListen( cmVnH_t h, bool enableFl )
{
  cmVn_t* p = _cmVnHandleToPtr(h);

  if( cmUdpNetEnableListen( p->udpH, enableFl ) != kOkUnRC )
    return cmErrMsg(&p->err,kUdpNetFailVnRC,"UDP listen enable failed.");
  return kOkVnRC;
}

// This function is called by cmTsMp1cDequeueMsg() which is called by cmVnReceive().
cmRC_t _cmVnTsQueueCb(void* userCbPtr, unsigned msgByteCnt, const void* msgDataPtr )
{
  cmVnNode_t* np = (cmVnNode_t*)userCbPtr;
  return np->p->cbFunc(np->p->cbArg,np->id,msgByteCnt,msgDataPtr);
}

cmVnRC_t cmVnCreateLocalNode(  cmVnH_t h, const cmChar_t* label, unsigned nodeId, cmVnH_t vnH, unsigned queBufByteCnt )
{
  cmVnRC_t    rc = kOkVnRC;
  cmVn_t*     p  = _cmVnHandleToPtr(h);
  cmVnNode_t* np = NULL; 

  // create a generic node
  if((rc = _cmVnCreateNode(p,label,nodeId,kLocalVnFl,&np )) != kOkVnRC )
    goto errLabel;

  if( cmTsMp1cCreate( &np->qH, queBufByteCnt, _cmVnTsQueueCb, np, p->err.rpt ) != kOkThRC )
  {
    rc = cmErrMsg(&p->err,kQueueFailVnRC,"The internal thread-safe queue creation failed for local node '%s'.",cmStringNullGuard(label));
    goto errLabel;
  }

  np->vnH = vnH;

  _cmVnNodeLink(p,np);

 errLabel:
  if( rc != kOkVnRC )
    _cmVnNodeFree(np);
  return rc;
}

cmVnRC_t cmVnCreateRemoteNode( cmVnH_t h, const cmChar_t* label, unsigned nodeId,  const cmChar_t* ipAddr, unsigned ipPort )
{
  cmVnRC_t    rc = kOkVnRC;
  cmVn_t*     p  = _cmVnHandleToPtr(h);
  cmVnNode_t* np;

  // creaet a generic node
  if((rc = _cmVnCreateNode(p, label,nodeId, 0, &np )) != kOkVnRC )
    goto errLabel;

  if( ipAddr!=NULL )
  {
    if( cmUdpNetRegisterRemote(p->udpH,label,nodeId,ipAddr,ipPort) != kOkUnRC )
    {
      rc = cmErrMsg(&p->err,kUdpNetFailVnRC,"UDP network remote registration failed for node:'%s'.",cmStringNullGuard(label));
      goto errLabel;
    }
  }

  _cmVnNodeLink(p,np);

 errLabel:
  if( rc != kOkVnRC )
    _cmVnNodeFree(np);
  return rc;
}

cmVnRC_t cmVnRecvFromLocal( cmVnH_t h, unsigned srcNodeId, unsigned byteCnt, const cmChar_t* buf )
{
  cmVnRC_t    rc = kOkVnRC;
  cmVnNode_t* np;
  cmVn_t*     p  = _cmVnHandleToPtr(h);

  if(( np = _cmVnNodeFindById(p,srcNodeId)) == NULL )
    return cmErrMsg(&p->err,kNodeNotFoundVnRC,"The node with id=%i could not be found.",srcNodeId);
  
  if( cmTsMp1cIsValid(np->qH) == false )
    return cmErrMsg(&p->err,kQueueFailVnRC,"The internal MPSC queue for the node '%s' is not valid. Is this a local node?",cmStringNullGuard(np->label));

  if( cmTsMp1cEnqueueMsg(np->qH,buf,byteCnt) != kOkThRC )
    return cmErrMsg(&p->err,kQueueFailVnRC,"Enqueue failed on the internal MPSC queue for the node '%s'.",cmStringNullGuard(np->label));
    
  return rc;
}

cmVnRC_t cmVnSendById( cmVnH_t h, unsigned nodeId, unsigned byteCnt, const cmChar_t* buf )
{
  cmVnNode_t* np;
  cmVn_t*     p  = _cmVnHandleToPtr(h);

  if(( np = _cmVnNodeFindById(p,nodeId)) == NULL )
    return cmErrMsg(&p->err,kNodeNotFoundVnRC,"The node with id=%i could not be found.",nodeId);

  return _cmVnSend(p,np,byteCnt,buf);
}

cmVnRC_t cmVnSendByLabel( cmVnH_t h, const cmChar_t* nodeLabel, unsigned byteCnt, const cmChar_t* buf )
{
  cmVnNode_t* np;
  cmVn_t*     p  = _cmVnHandleToPtr(h);

  if(( np = _cmVnNodeFindByLabel(p,nodeLabel)) == NULL )
    return cmErrMsg(&p->err,kNodeNotFoundVnRC,"The node named '%s' could not be found.",cmStringNullGuard(nodeLabel));

  return _cmVnSend(p,np,byteCnt,buf);
}

cmVnRC_t _cmVnRecvQueueMsgs( cmVn_t* p, cmVnNode_t* np, unsigned *msgCntPtr )
{
  unsigned i;
  unsigned mn = *msgCntPtr;
  *msgCntPtr = 0;

  for(i=0; (i<mn || mn==cmInvalidCnt) && cmTsMp1cMsgWaiting(np->qH); ++i)
  {
    // calling this function results in calls to _cmVnTsQueueCb()
    if( cmTsMp1cDequeueMsg( np->qH,NULL,0) != kOkThRC )
      return cmErrMsg(&p->err,kQueueFailVnRC,"Msg deque failed for node '%s'.",cmStringNullGuard(np->label));
  }

  *msgCntPtr = i;

  return kOkVnRC;
}

cmVnRC_t cmVnReceive( cmVnH_t h, unsigned* msgCntPtr )
{
  cmVnRC_t    rc = kOkVnRC;
  cmVn_t*     p  = _cmVnHandleToPtr(h);
  cmVnNode_t* np = p->nodes;
  unsigned    mn = msgCntPtr == NULL ? cmInvalidCnt : *msgCntPtr;

  if( msgCntPtr != NULL )
    *msgCntPtr = 0;
  
  for(; np!=NULL && rc==kOkVnRC; np=np->link)
  {
    unsigned msgCnt = mn;
    switch( np->flags & (kOwnerVnFl | kLocalVnFl) )
    {
      case kOwnerVnFl:
        break;

      case kLocalVnFl:
        rc = _cmVnRecvQueueMsgs(p,np,&msgCnt);
        break;

      default:
        if( cmUdpNetReceive(p->udpH,msgCntPtr==NULL?NULL:&msgCnt) != kOkUnRC )
          rc = cmErrMsg(&p->err,kUdpNetFailVnRC,"The UDP net receive failed on node '%s'.",cmStringNullGuard(np->label));
        break;
    }

    if( rc == kOkVnRC && msgCntPtr != NULL )
      *msgCntPtr += msgCnt;

  }

  return rc;
}

//---------------------------------------------------------------------------------------------------

unsigned udpRecvBufByteCnt = 8192;
unsigned udpRecvTimeOutMs  = 100;
unsigned queueBufByteCnt   = 8192;
  void*  cbArg             = NULL;


typedef struct
{
  const cmChar_t* label;
  unsigned        id;
  const cmChar_t* ipAddr;
  unsigned        ipPort;
  cmVnH_t         vnH;
} node_t;

cmVnRC_t _cmVnTestCb( void* cbArg, unsigned srcNodeId, unsigned byteCnt, const char* buf )
{
  printf("src node:%i bytes:%i %s\n",srcNodeId,byteCnt,buf);
  return kOkVnRC;
}

cmVnRC_t _cmVnTestCreateLocalNet( cmCtx_t* ctx, cmErr_t* err, unsigned id, node_t* nodeArray   )
{
  cmVnRC_t rc = kOkVnRC;

  if((rc = cmVnCreate(ctx, &nodeArray[id].vnH, _cmVnTestCb, cbArg, nodeArray[id].label, nodeArray[id].id, udpRecvBufByteCnt, udpRecvTimeOutMs, nodeArray[id].ipAddr, nodeArray[id].ipPort )) != kOkVnRC )
    rc = cmErrMsg(err,rc,"Virtual network create failed.");

  return rc;
}

cmVnRC_t _cmVnTestCreateVirtNodes( cmErr_t* err, unsigned id, node_t* nodeArray )
{
  unsigned rc = kOkVnRC;
  unsigned i;

  for(i=0; nodeArray[i].label != NULL; ++i)
  {
    // if this is a local node
    if( strcmp(nodeArray[i].ipAddr,nodeArray[id].ipAddr) == 0 )
    {
      // if this is not the owner node
      if( i != id )
        if((rc = cmVnCreateLocalNode( nodeArray[id].vnH, nodeArray[i].label, nodeArray[i].id, nodeArray[i].vnH, queueBufByteCnt )) != kOkVnRC )
        {
          cmErrMsg(err,rc,"Local node create failed for node:'%s'.",nodeArray[i].label);
          goto errLabel;
        }
    }
    else  // this must be a remote node
    {
      if((rc = cmVnCreateRemoteNode(nodeArray[id].vnH, nodeArray[i].label, nodeArray[i].id, nodeArray[i].ipAddr, nodeArray[i].ipPort )) != kOkVnRC )
      {
        cmErrMsg(err,rc,"Remote node create failed for node '%s'.",nodeArray[i].label);
        goto errLabel;
      }
    }
  }

 errLabel:
  return rc;
}

cmVnRC_t _cmVnTestSend( cmErr_t* err, node_t* nodeArray, unsigned id, unsigned i, unsigned n )
{
  cmVnRC_t rc = kOkVnRC;

  if( id == i )
  {
    printf("A node cannot send to itself.\n");
    return rc;
  }

  const cmChar_t* buf = cmTsPrintfS("msg-%i",n);
  unsigned bufByteCnt = strlen(buf)+1;

  printf("Sending from '%s' to '%s'.\n", cmStringNullGuard(nodeArray[id].label), cmStringNullGuard(nodeArray[i].label));


  if((rc =  cmVnSendById(nodeArray[id].vnH, nodeArray[i].id, bufByteCnt, buf ))!= kOkVnRC )
    cmErrMsg(err,rc,"Send from '%s' to '%s' failed.", cmStringNullGuard(nodeArray[id].label), cmStringNullGuard(nodeArray[i].label));

  return rc;
}

cmVnRC_t cmVnTest( cmCtx_t* ctx )
{
  cmVnRC_t rc                = kOkVnRC;
  cmErr_t  err;  
  unsigned id                = 0;
  unsigned i;
  unsigned n = 0;

  node_t nodeArray[] =
  {
    { "whirl-0", 10, "192.168.15.109", 5768, cmVnNullHandle },
    { "whirl-1", 20, "192.168.15.109", 5767, cmVnNullHandle },
    { "thunk-0", 30, "192.168.15.111", 5766, cmVnNullHandle },
    { NULL, -1, NULL }
  };

  cmErrSetup(&err,&ctx->rpt,"cmVnTest");

  // create the virt networks for the local nodes
  for(i=0; nodeArray[i].label !=NULL; ++i)
    if( strcmp(nodeArray[i].ipAddr,nodeArray[id].ipAddr ) == 0 )
      if((rc = _cmVnTestCreateLocalNet(ctx,&err,i,nodeArray)) != kOkVnRC )
        goto errLabel;

  // create the virtual nodes for each local virtual network
  for(i=0; nodeArray[i].label != NULL; ++i)
    if( cmVnIsValid(nodeArray[i].vnH) )
      if((rc = _cmVnTestCreateVirtNodes(&err, i, nodeArray )) != kOkVnRC )
        break;

  char c;
  while((c=getchar()) != 'q')
  {
    bool promptFl = true;

    switch( c )
    {
      case '0':
      case '1':
      case '2':
        _cmVnTestSend(&err, nodeArray, id, (unsigned)c - (unsigned)'0', n );
        ++n;
        break;
    }
    
    for(i=0; nodeArray[i].label!=NULL; ++i)
      if( cmVnIsValid(nodeArray[i].vnH) )
        cmVnReceive(nodeArray[i].vnH,NULL);

    if( promptFl )
      cmRptPrintf(&ctx->rpt,"%i> ",n);

  }

 errLabel:
  // destroy the virtual networks
  for(i=0; nodeArray[i].label!=NULL; ++i)
    if( cmVnIsValid(nodeArray[i].vnH) )
      if((rc = cmVnDestroy(&nodeArray[i].vnH)) != kOkVnRC )
        cmErrMsg(&err,rc,"Node destroy failed for node '%s'.",cmStringNullGuard(nodeArray[i].label));

  return rc;
}
