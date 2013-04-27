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

  const cmChar_t* cmRtNetLocalHostName( cmRtNetH_t h );

  // Create a network node.
  // The 'nodeLabel' refers to a network device cfg. (see cmDevCfg).
  // Set 'ipAddr' to NULL if this is the local node.
  // During sync mode this node will attempt to sync with all
  // nodes in the node list.
  cmRtNetRC_t cmRtNetRegisterLocalNode( cmRtNetH_t h, const cmChar_t* nodeLabel, const cmChar_t* ipAddr, cmUdpPort_t ipPort );


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


     The 'master' is the machine which cmRtNetBeginSyncMode() is called on.
     1) 'master' sends local endpoints to all registered remote nodes.
     2) When a 'slave' receives the kDoneSelNetId msg it transmits
     it's own local endpoints back to the master.

     a. Each node in the node list has a type id:
       1. local 
       2. registered - remote node that was explicitely registered on a master
       3. received   - remote node that was received from a master

     b. 
       1. All nodes are created in the 'send-hello' state.
       2. If a master machine is in 'sync-mode' then it systematically sends
       each of it's local endpoints to all 'registered' nodes.
       3. When a slave machine recives a 'hello' it creates a
       'received' node.
       4. When a slave machine recieves a 'done' it enters sync mode
       and systematically sends each of its local endpoints to
       the 'done' source.
       

   Protocol:
     1.  A: on init bcast 'hello'
     2.  B: on 'hello'     -  create node-A w/ ei=0      - send 'node'
     3.  A: on 'node'      -  create node-B w/ ei=0      - send first 'endpt'
     4.  B: on 'endpt'     -  create endpt on node-A     - ei!=en ? send 'endpt' or send 'done'
     5.  A: on 'endpt'     -  create endpt on node-B     - ei!=en ? send 'endpt' or send 'done'
     6.  B: on 'done'      -  mark node-A as 'valid'
     7.  A: on 'done'      -  mark node-B as 'valid'.

   */  

#ifdef __cplusplus
}
#endif


#endif
