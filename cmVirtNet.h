#ifndef cmVirtNet_h
#define cmVirtNet_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"Wrapper object for cmUdpNet to handle UDP network communications." kw:[network]}
  enum
  {
    kOkVnRC = cmOkRC,
    kUdpNetFailVnRC,
    kNodeNotFoundVnRC,
    kDuplicateNodeIdVnRC,
    kDuplicateNodeLabelVnRC,
    kQueueFailVnRC
  };

  typedef cmHandle_t cmVnH_t;
  typedef cmRC_t     cmVnRC_t;

  extern cmVnH_t cmVnNullHandle;

  // 
  typedef cmVnRC_t (*cmVnCb_t)( void* cbArg, unsigned srcNodeId, unsigned byteCnt, const char* buf );

  // Create a virtual network with a single node that represents the owner.
  // cbFunc() will be called to receive incoming msg's when the owning thread calls cmVnReceive().  
  cmVnRC_t cmVnCreate( 
    cmCtx_t*        ctx, 
    cmVnH_t*        hp, 
    cmVnCb_t        cbFunc,             // owner msg receive callback function
    void*           cbArg,              // owner supplied callback argument for cbFunc
    const cmChar_t* ownerNodelabel,     // owners node label (must be unique among all nodes)
    unsigned        ownerNodeId,        // owners node id (must be unique among all nodes) 
    unsigned        udpRecvBufByteCnt,  // size of the UDP incoming data buffer 
    unsigned        updRecvTimeOutMs,   // UDP time out period while waiting for incoming data
    const cmChar_t* ipAddr,             // this machines IP
    unsigned        ipPort  );          // this nodes port (must be unique among all local nodes)

  // Destroy and release any resources held by a virtual net created with cmVnCreate(). 
  cmVnRC_t cmVnDestroy( cmVnH_t* hp );

  bool     cmVnIsValid( cmVnH_t h );

  // Put the network into listening mode.  This function enables and disables
  // the UDP port listening thread.
  cmVnRC_t cmVnEnableListen( cmVnH_t h, bool enableFl );

  // Local nodes are nodes which are in the owners address space but controlled by a different thread.
  // 
  cmVnRC_t cmVnCreateLocalNode(  cmVnH_t h, const cmChar_t* label, unsigned nodeId, cmVnH_t localNet, unsigned queBufByteCnt );

  // Remote nodes are outside the owners address space and are communicated with via UDP.
  cmVnRC_t cmVnCreateRemoteNode( cmVnH_t h, const cmChar_t* label, unsigned nodeId, const cmChar_t* ipAddr, unsigned ipPort );

  // Send a msg from the owner node to another node.
  cmVnRC_t cmVnSendById( cmVnH_t h, unsigned nodeId, unsigned byteCnt, const cmChar_t* buf );
  cmVnRC_t cmVnSendByLabel( cmVnH_t h, const cmChar_t* nodeLabel, unsigned byteCnt, const cmChar_t* buf );

  // Recv a msg from a local node. This function may be called outside the 'owner' thread.
  // This function stores the incoming msg in a muliple-producer single-consumer queue.
  cmVnRC_t cmVnRecvFromLocal( cmVnH_t h, unsigned srcNodeId, unsigned byteCnt, const cmChar_t* buf );

  // Calling this function results in callback's to the cmVnCb_t function.
  cmVnRC_t cmVnReceive( cmVnH_t h, unsigned* msgCntPtr );

  cmVnRC_t cmVnTest( cmCtx_t* ctx );

  //)
  
#ifdef __cplusplus
}
#endif

#endif
