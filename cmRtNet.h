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
    kDuplEndNetRC,
    kDuplLocalNetRC,
    kThreadFailNetRC,
    kBufToSmallNetRC,
    kNodeNotFoundNetRC,
    kEndNotFoundNetRC,
    kLocalNodeNetRC,
    kInvalidArgNetRC,
    kSyncFailNetRC,
    kNodeEndCntErrNetRC
  };

  typedef cmRC_t     cmRtNetRC_t;
  typedef cmHandle_t cmRtNetH_t;
  typedef cmHandle_t cmRtNetEndptH_t;

  extern cmRtNetH_t      cmRtNetNullHandle;
  extern cmRtNetEndptH_t cmRtNetEndptNullHandle;

  // 'cbFunc' will be called within the context of cmRtNetReceive() to receive
  // incoming network messages.
  cmRtNetRC_t cmRtNetAlloc( cmCtx_t* ctx, cmRtNetH_t* hp, cmUdpCallback_t cbFunc, void* cbArg );
  cmRtNetRC_t cmRtNetFree( cmRtNetH_t* hp );

  bool      cmRtNetIsValid( cmRtNetH_t h );

  // Get the local host name for this machine. This function
  // is synonomous with gethostname().
  const cmChar_t* cmRtNetLocalHostName( cmRtNetH_t h );

  // Initialize the local network node.
  // 'bcastAddr' is the network broadcast address (e.g. 192.168.15.255).
  // 'nodeLabel' is the local network node label
  // 'ipAddr' may be set to NULL to use any available IP address.
  // 'ipPort' refers to the socket port (which may need to be made available 
  // by the machine firewall cfg.)
  cmRtNetRC_t cmRtNetInitialize( cmRtNetH_t h, const cmChar_t* bcastAddr, const cmChar_t* nodeLabel, const cmChar_t* ipAddr, cmUdpPort_t ipPort );
  bool        cmRtNetIsInitialized( cmRtNetH_t h );

  // Register the local endpoints.
  // Endpoints may only be registered once the network is initialized via
  // cmRtNetInitialize().
  // Remote nodes will be able to send messages to these endpoints by
  // referring to (nodeLabel/endPtLabel)
  cmRtNetRC_t cmRtNetRegisterEndPoint( cmRtNetH_t h, const cmChar_t* endPtLabel, unsigned endPtId );

  // Delete all nodes and endpoints.
  cmRtNetRC_t cmRtNetFinalize( cmRtNetH_t h );

  // Broadcast the 'hello' to all machines listening on the 
  // broadcast addresss. This starts the synchronization sequence
  cmRtNetRC_t cmRtNetDoSync( cmRtNetH_t h );

  // This function must be polled to receive incoming network messages
  // via the callback funcion 'cbFunc' as passed to cmRtNetAlloc()
  cmRtNetRC_t cmRtNetReceive( cmRtNetH_t h );

  // Get an end point handle for use with cmRtNetSend.
  cmRtNetRC_t cmRtNetEndpointHandle( cmRtNetH_t h, const cmChar_t* nodeLabel, const cmChar_t* endptLabel, cmRtNetEndptH_t* hp );

  // Send a message to a remote endpoint.
  cmRtNetRC_t cmRtNetSend( cmRtNetH_t h, cmRtNetEndptH_t epH, const void* msg, unsigned msgByteCnt );

  // Send a message to a remote endpoint. This function is a composite
  // of cmRtNetEndpointHandle() and cmRtNetSend().
  cmRtNetRC_t cmRtNetSendByLabels( cmRtNetH_t h, const cmChar_t* nodeLabel, const cmChar_t* endptLabel, const void* msg, unsigned msgByteCnt );

  // Enable/disable synchronization protocol reporting.
  // Return the previous state of the report sync. flag.
  bool        cmRtNetReportSyncEnable( cmRtNetH_t h, bool enableFl );
  bool        cmRtNetReportSyncIsEnabled( cmRtNetH_t h );

  void        cmRtNetReport( cmRtNetH_t h );
    
  void        cmRtNetTest( cmCtx_t* ctx, bool mstrFl );

  /*

   Synchronization Protocol:

                  Machine A                          Machine B
     ==================================    ====================================
     broadcast 'hello' --------------------> create node-A w/ ei=0 -------+
                                                                          |
     +<-- create node-B w/ ei=0 <----------- send 'node' <----------------+
     |
     +--> switch(ei,m_t)
     |     ei  < en   : send endpt[ei++] ---> create endpt[] on node-A -->+
     |                                                                    |
     |     ei == en   : ++ei,send 'done' -------------------------------->+                                                    |
     |                                                                    |
     |    m_t!='done' :      send 'done' -------------------------------->+                                                              |
     |                                                                    |
     |    (stop)      :                                                   |
     |                                                                    |
     |                                                                    v
     |                                                           switch(ei,m_t)
     +<-- create endpt[] on node-B  <--------- send endpt[ei++] : ei < en
     | 
     +<--------------------------------------- send 'done',++ei : ei == en 
     |
     +<--------------------------------------- send 'done'      : m_t!= 'done'
                                                                  
                                                                :  (stop)

     Notes:
        1)  'ei' is the index of the next local end point to transmit.
        2)  'en' is the count of local endpoints.
        3)  'm_t' is the msg type (i.e.'hello','node','endpoint','done') 
            of the incoming message.

   */  

#ifdef __cplusplus
}
#endif


#endif
