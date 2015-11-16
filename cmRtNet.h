#ifndef cmRtNet_h
#define cmRtNet_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"rtSys networking component." kw:[rtsys network] }
  
  /*
   Nodes and Endpoints:
   ---------------------
   A node corresponds to a process and owns a socket. It also has a label which is 
   unique among all other nodes on the network. A node also has a set of application 
   defined 'endpoints'.  Each endpoint has a label and id that is unique among all 
   other endpoints on the same node.  Endpoints on different nodes however may share
   the same label and id.  Endpoints are used by remote senders to identify 
   a particular receiver which is sharing the node with other receivers.  Endpoints
   are therefore analogous to port numbers on sockets.

   See gt/doc/notes.txt for more discussion of cmRtNet.
   
   */

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


  // selector id's for cmRtNetSyncMsg_t.selId.
  typedef enum
  {
    kHelloSelNetId,         // 0 broadcast msg (label=node label, id=endpt cnt)
    kNodeSelNetId,          // 1 define remote node  (label=remote node label,  id=endpt cnt)
    kEndpointSelNetId,      // 2 define remote endpt (label=remote endpt label, id=endpt id)
    kDoneSelNetId,          // 3 declare all endpts sent
    kInvalidSelNetId        // 4
  } cmRtNetSelId_t;


  // Network synchronization message format.
  // cmRtNetRC_t.hdr.selId == kNetSyncSelRtid.
  typedef struct
  {
    cmRtSysMsgHdr_t hdr;      // standard cmRtSys msg  header 
    cmRtNetSelId_t  selId;    // message selector id (See kXXXSelNetId above)
    unsigned        hdrByteCnt; // size of the header record at transmission (used to locate the serialzed label)
    unsigned        rtSubIdx; // cmInvalidIdx or rtSubIdx
    unsigned        id;       // endptCnt     or endpoint id
    const cmChar_t* label;    // node         or endpoint label
  } cmRtNetSyncMsg_t;

  const cmChar_t* cmRtNetSyncMsgLabel( const cmRtNetSyncMsg_t* m );


  // NOTE: Messages passed between cmRtNet nodes during the synchronization 
  // process use the cmRtNetSyncMsg_t format (w/ the body of label following 
  // the record.  All other messages use cmRtNetMsg_t (cmRtSysMsg.h) format.

  // 'cbFunc' will be called within the context of cmRtNetReceive() to receive
  // incoming network messages.
  // rtSubIdx is the rtSubIdx of the cmRtSys which owns this cmRtNet.  
  cmRtNetRC_t cmRtNetAlloc( cmCtx_t* ctx, cmRtNetH_t* hp, unsigned rtSubIdx, cmUdpCallback_t cbFunc, void* cbArg );
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
  // via the callback funcion 'cbFunc' as passed to cmRtNetAlloc().
  // Note that all messages received via 'cbFunc' will be prefixed with
  // an cmRtSysMsgHdr_t header (See cmRtSysMsg.h).
  cmRtNetRC_t cmRtNetReceive( cmRtNetH_t h );


  // Get a remote end point handle for use with cmRtNetSend.
  cmRtNetRC_t cmRtNetEndpointHandle( cmRtNetH_t h, const cmChar_t* nodeLabel, const cmChar_t* endptLabel, cmRtNetEndptH_t* hp );

  bool        cmRtNetEndpointIsValid( cmRtNetEndptH_t endPtH );

  // Given an endpoint handle return the id/label of the associated endpoint.
  unsigned        cmRtNetEndpointId( cmRtNetEndptH_t endPtH );
  const cmChar_t* cmRtNetEndpointLabel( cmRtNetEndptH_t endPtH );

  // Send a message to a remote endpoint.
  // Note that srcEndPtId is used only to inform the receiver of the endpoint
  // of the transmitter. It is not used in any part of the transmit or receive
  // process.
  cmRtNetRC_t cmRtNetSend( cmRtNetH_t h, unsigned srcEndPtId, cmRtNetEndptH_t epH, const void* msg, unsigned msgByteCnt );

  // Send a message to a remote endpoint. This function is a composite
  // of cmRtNetEndpointHandle() and cmRtNetSend().
  cmRtNetRC_t cmRtNetSendByLabels( cmRtNetH_t h, unsigned srcEndPtId, const cmChar_t* nodeLabel, const cmChar_t* endptLabel, const void* msg, unsigned msgByteCnt );

  cmRtNetRC_t cmRtNetSendByIndex( cmRtNetH_t h, unsigned srcEndPtId, unsigned dstNodeIdx, unsigned dstEndptIdx, const void* msg, unsigned msgByteCnt ); 

  // Enable/disable synchronization protocol reporting.
  // Return the previous state of the report sync. flag.
  bool        cmRtNetReportSyncEnable( cmRtNetH_t h, bool enableFl );
  bool        cmRtNetReportSyncIsEnabled( cmRtNetH_t h );

  // Query network configuration. Returns true on success or false if
  // {nodeIdx, epIdx} does not identify a valid endpoint.
  const cmChar_t* cmRtNetLocalNodeLabel( cmRtNetH_t h );
  unsigned        cmRtNetRemoteNodeCount( cmRtNetH_t h );
  unsigned        cmRtNetAddrToNodeIndex( cmRtNetH_t h, const struct sockaddr_in* a );
  unsigned        cmRtNetRemoteNodeIndex( cmRtNetH_t h, const cmChar_t* label );
  const cmChar_t* cmRtNetRemoteNodeLabel( cmRtNetH_t h, unsigned idx );
  unsigned        cmRtNetRemoteNodeEndPointCount(   cmRtNetH_t h, unsigned nodeIdx );
  cmRtNetRC_t     cmRtNetRemoteNodeEndPoint( 
    cmRtNetH_t       h, 
    unsigned         nodeIdx, 
    unsigned         epIdx, 
    const cmChar_t** labelRef,
    unsigned*        idRef,
    unsigned*        rsiRef );

  void        cmRtNetReport( cmRtNetH_t h );

    
  void        cmRtNetTest( cmCtx_t* ctx, bool mstrFl );

  /*

   Synchronization Protocol:

                  Machine A                          Machine B
     ==================================    ====================================
     broadcast 'hello' ------------------=-> create node-A w/ ei=0 -------+
                                                                          |
     +<-- create node-B w/ ei=0 <--------=-- send 'node' <----------------+
     |
     +--> switch(ei,m_t)
     |     ei  < en  : send endpt[ei++] -=--> create endpt[] on node-A -->+
     |                                                                    |
     |     ei == en  : ++ei,send 'done' -=------------------------------->+                                                    |
     |                                                                    |
     |    m_t!='done':      send 'done' -=------------------------------->+                                                              |
     |                                                                    |
     |    (stop)     :                                                    |
     |                                                                    |
     |                                                                    v
     |                                                           switch(ei,m_t)
     +<-- create endpt[] on node-B  <---=----- send endpt[ei++] : ei < en
     | 
     +<---------------------------------=----- send 'done',++ei : ei == en 
     |
     +<---------------------------------=----- send 'done'      : m_t!= 'done'
                                                                  
                                                                :  (stop)

     Notes:
        1)  'ei' is the index of the next local end point to transmit.
        2)  'en' is the count of local endpoints.
        3)  'm_t' is the msg type (i.e.'hello','node','endpoint','done') 
            of the incoming message.
        4)  The symbol -=- in the flow chart implies a network transmission.

   */  
  //)
  
#ifdef __cplusplus
}
#endif


#endif
