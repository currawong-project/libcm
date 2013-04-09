#ifndef cmUdpNet_h
#define cmUdpNet_h

#ifdef __cplusplus
extern "C" {
#endif

/*
  A cmUdpNet is a wrapper around a single cmUdpPort. This object
  maintains an array of remote nodes which map application defined
  node label/id's to IP address/port. This allows the application
  to communicate in terms of its own id scheme without having to
  consider the IP addr/port of the remote nodes.
 */

enum
{
  kOkUnRC = cmOkRC,
  kDuplicateNodeLabelUnRC,
  kDuplicateNodeIdUnRC,
  kUdpPortFailUnRC,
  kInvalidNodeLabelUnRC,
  kNodeNotFoundUnRC,
  kInvalidNodeAddrUnRC,
  kSendFailUnRC,
  kGetDataFailUnRC,
  kJsonFailUnRC
};

typedef cmRC_t     cmUnRC_t;
typedef cmHandle_t cmUdpNetH_t;

extern cmUdpNetH_t cmUdpNetNullHandle;

typedef void (*cmUdpNetCallback_t)( void* cbArg, cmUdpNetH_t h, const char* data, unsigned dataByteCnt, unsigned remoteNodeId ); 

// Allocate a UDP net manager. To use the manager one of the
// initialization functions must be used configure it.
cmUnRC_t cmUdpNetAlloc( cmCtx_t* ctx,  cmUdpNetH_t*  hp );

// Allocate and initialize a UDP network manager from
// a JSON script.  This function is a simple wrapper for
// calls to cmUdpNetAlloc(), cmUdpNetInitJson(), and
// cmUdpNetEnableListen(h,listenFl).
enum { kListenUnFl=0x01, kNetOptionalUnFl=0x02 };
cmUnRC_t cmUdpNetAllocJson( 
  cmCtx_t*           ctx, 
  cmUdpNetH_t*       hp,
  cmJsonH_t          jsH,
  cmUdpNetCallback_t cbFunc,
  void*              cbArg,
  unsigned           flags);

// Release a UDP network manager and any resources it may hold.
cmUnRC_t cmUdpNetFree( cmUdpNetH_t* hp );  

// Initialize a UDP net using a previously allocated handle
// The node information (nodeLabel,nodeId,nodeSocketPort) refers
// to the local node. The callback information (cbFunc,cbArg)
// are used during cmUdpNetReceive() to receive incoming
// information from the local node.
cmUnRC_t cmUdpNetInit(
  cmUdpNetH_t         h,
  const cmChar_t*     nodeLabel,
  unsigned            nodeId,
  cmUdpPort_t         nodeSocketPort,
  cmUdpNetCallback_t  cbFunc,
  void*               cbArg,
  unsigned            recvBufByteCnt,
  unsigned            socketRecvTimeOutMs );

// Initialize a UDP net and register  remote nodes using  a JSON resource.
cmUnRC_t cmUdpNetInitJson( 
  cmUdpNetH_t        h, 
  cmJsonH_t          jsH,
  cmUdpNetCallback_t cbFunc,
  void*              cbArg );

// Return true if the if the network has been initialized.
bool cmUdpNetIsInitialized( cmUdpNetH_t h );

// Finalize a UDP net.  This releases any resources allocated
// via one of the above 'init' functions. 
cmUnRC_t cmUdpNetFinal( cmUdpNetH_t h );

// Enable/disable the networks listening port.  While in 
// 'listening' mode the network internally queue's all arriving
// messages.  Messages are then forwarded to the client via
// calls to cmUdpNetReceive().
cmUnRC_t cmUdpNetEnableListen( cmUdpNetH_t h, bool enableFl );

// Return true if the handle is valid.
bool     cmUdpNetIsValid( cmUdpNetH_t h );

unsigned        cmUdpNetLocalNodeId( cmUdpNetH_t h );
const cmChar_t* cmUdpNetLocalNodeLabel( cmUdpNetH_t h );

// Return the node id associated with a node label or 'cmInvalidId' if the
// label is not found.
unsigned        cmUdpNetNodeLabelToId( cmUdpNetH_t h, const cmChar_t* label );

// Return the node label associated with a node id or NULL if the id 
// is not found.
const cmChar_t* cmUdpNetNodeIdToLabel( cmUdpNetH_t h, unsigned id );

// Get the total count of nodes on the network.  This count includes the local node.
unsigned        cmUdpNetNodeCount( cmUdpNetH_t h );

// Return the node id of each network node.
unsigned        cmUdpNetNodeId(    cmUdpNetH_t h, unsigned nodeIdx );

// Register a remote node.
cmUnRC_t cmUdpNetRegisterRemote( 
  cmUdpNetH_t h, 
  const cmChar_t* remoteNodeLabel,
  unsigned        remoteNodeId,
  const char*     remoteNodeSockAddr,
  cmUdpPort_t     remoteNodePort );

// Send a message to a remote network node.
cmUnRC_t cmUdpNetSendById(    cmUdpNetH_t h, unsigned remoteNodeId, const void* data, unsigned dataByteCnt );
cmUnRC_t cmUdpNetSendByLabel( cmUdpNetH_t h, const cmChar_t* remoteNodeLabel, const void* data, unsigned dataByteCnt );

// Transmit any waiting incoming messages to the client via the
// cmUdpNetCallback_t callback function provided
// cmUdpNetInit().
// On input *msgCntPtr should hold the max. number of 
// messages to receive or NULL to receive all available.
// On return *msgCntPtr is set to the actual number of
// messages received.
cmUnRC_t cmUdpNetReceive(  cmUdpNetH_t h, unsigned* msgCntPtr );


cmUnRC_t cmUdpNetPrintNodes( cmUdpNetH_t h, cmRpt_t* rpt );
void     cmUdpNetReport( cmUdpNetH_t h, cmRpt_t* rpt );

cmRC_t cmUdpNetTest( cmCtx_t* ctx, int argc, char* argv[] );

#ifdef __cplusplus
}
#endif

#endif
