#ifndef cmNet_h
#define cmNet_h

#ifdef __cplusplus
extern "C" {
#endif

  enum
  {
    kOkNetRC = cmOkRC,
    kUdpPortFailNetRC,
    kInvalidLabelNetRC,
    kDuplLabelNetRC,
    kDuplLocalNetRC,
    kDuplEndNetRC,
    kThreadFailNetRC,
    kBufToSmallNetRC,
    kNodeNotFoundNetRC,
    kNodeStateErrNetRC,
    kTimeOutErrNetRC,
    kLocalNodeNetRC,
  };

  typedef cmRC_t     cmRtNetRC_t;
  typedef cmHandle_t cmRtNetH_t;


  extern cmRtNetH_t cmRtNetNullHandle;

  // 'cbFunc' will be called within the context of cmRtNetReceive() to receive
  // incoming network messages.
  cmRtNetRC_t cmRtNetAlloc( cmCtx_t* ctx, cmRtNetH_t* hp, cmUdpCallback_t cbFunc, void* cbArg );
  cmRtNetRC_t cmRtNetFree( cmRtNetH_t* hp );

  bool      cmRtNetIsValid( cmRtNetH_t h );


  // Create a network node.
  // The 'nodeLabel' refers to a network device cfg. (see cmDevCfg).
  // Set 'ipAddr' to NULL if this is the local node.
  // During sync mode this node will attempt to sync with all
  // nodes in the node list.
  cmRtNetRC_t cmRtNetCreateNode( cmRtNetH_t h, const cmChar_t* nodeLabel, const cmChar_t* ipAddr, cmUdpPort_t ipPort );

  // Register the local endpoints.
  // Remote nodes will be able to send messages to these endpoints by
  // referring to (nodeLabel/endPtLabel)
  cmRtNetRC_t cmRtNetRegisterEndPoint( cmRtNetH_t h, const cmChar_t* endPtLabel, unsigned endPtId );

  // Delete all nodes and endpoints.
  cmRtNetRC_t cmRtNetClearAll( cmRtNetH_t h );


  // Go into 'sync' node.
  // When a node enters sync mode it systematically transmits all of it's 
  // local endpoint information to each registered remote node.  Prior to 
  // entering sync mode a node must therefore have been setup with a list 
  // of remote nodes (via cmRtNetCreateNode()) and a list of local endpoints 
  // (cmRtNetRegisterEndpoint()).  During sync mode a node sends it's local 
  // endpoint list to each registered remote node. When a remote node receives 
  // an endpoint it updates it's own remote node/endpoint 
  // list.
  cmRtNetRC_t cmRtNetBeginSyncMode( cmRtNetH_t h );
  bool      cmRtNetIsInSyncMode(  cmRtNetH_t h );

  // When the network message recieve function (See cmRtNetAlloc() 'cbFunc') 
  // receives a message with the cmRtSysMsgHdr_t.selId == kNetSyncSelRtId
  // it should call this function to update the current sync state of the
  // cmRtNet.
  cmRtNetRC_t  cmRtNetSyncModeRecv( cmRtNetH_t h, const char* data, unsigned dataByteCnt, const struct sockaddr_in* fromAddr );

  
  // When in the network is in sync mode (cmRtNetIsSync()==true) 
  // the client system must poll this function to update the networks sync state.
  cmRtNetRC_t cmRtNetSyncModeSend( cmRtNetH_t h );

  // This function must be polled to receive incoming network messages
  // via the callback funcion 'cbFunc' as passed to cmRtNetAlloc()
  cmRtNetRC_t cmRtNetReceive( cmRtNetH_t h );

  bool        cmRtNetIsSyncModeMsg( const void* data, unsigned dataByteCnt );

  unsigned  cmRtNetEndPointIndex( cmRtNetH_t h, const cmChar_t* nodeLabel, const cmChar_t* endPtLabel );
  

  cmRtNetRC_t cmRtNetSend( cmRtNetH_t h, unsigned endPointIndex, const void* msg, unsigned msgByteCnt );

  void      cmRtNetReport( cmRtNetH_t h );
    
  void      cmRtNetTest( cmCtx_t* ctx, bool mstrFl );

  /*
    Master:
      cmRtNetBeginSyncMode().
      while( cmRtNetIsSyncMode())
      {
       // Give the master an oppurtunity to advance it's sync mode state.
       // When the master is has sync'd with all remote nodes in it's
       // remote node list then it will automatically exit sync mode.
       cmRtNetSyncModeSend()
      }

      _myNetRecv(dataV,dataN,addr)
     {
       if( cmRtNetIsSyncModeMsg(dataV,dataN) )
         cmRtNetSyncModeRecv(dataV,dataN,addr)
     }   
   */  

#ifdef __cplusplus
}
#endif


#endif
