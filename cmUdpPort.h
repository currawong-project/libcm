#ifndef cmUdpPort_h
#define cmUdpPort_h

#ifdef __cplusplus
extern "C" {
#endif
  //( { file_desc:"UDP socket interface class." kw:[network] }
  
  #include <netinet/in.h>

  enum
  {
    kOkUdpRC = cmOkRC,
    kSockCreateFailUdpRC,
    kSockCloseFailUdpRC,
    kSockBindFailUdpRC,
    kSockConnectFailUdpRC,
    kSockOptSetFailUdpRC,
    kSockSendFailUdpRC,
    kSockRecvFailUdpRC,
    kSockSelectFailUdpRC,
    kPtoNFailUdpRC,
    kNtoPFailUdpRC,
    kNotConnectedUdpRC,
    kThreadFailUdpRC,
    kQueueFailUdpRC,
    kRecvBufOverflowUdpRC,
    kBufTooSmallUdpRC,
    kHostNameFailUdpRC,
    kInvalidPortNumbUdpRC,
    kTimeOutUdpRC,
    kTestFailUdpRC
  };

  typedef cmRC_t         cmUdpRC_t;
  typedef cmHandle_t     cmUdpH_t;
  typedef unsigned short cmUdpPort_t;

  extern cmUdpH_t cmUdpNullHandle;

  typedef void (*cmUdpCallback_t)( void* cbArg, const char* data, unsigned dataByteCnt, const struct sockaddr_in* fromAddr ); 

  enum
  {
    kNonBlockingUdpFl = 0x00,
    kBlockingUdpFl    = 0x01,
    kNoQueueUdpFl     = 0x02,
    kBroadcastUdpFl   = 0x04
    
  };

  enum
  {
    // port 0 is reserved by and is therefore a convenient invalid port number
    kInvalidUdpPortNumber = 0 
  };

  cmUdpRC_t cmUdpAlloc( cmCtx_t* ctx, cmUdpH_t* hp );
  cmUdpRC_t cmUdpFree(  cmUdpH_t* hp );

  cmUdpRC_t cmUdpInit( 
    cmUdpH_t        h, 
    cmUdpPort_t     port,            // this sockets port
    unsigned        flags,           // see kXXXUdpFl 
    cmUdpCallback_t cbFunc,          // Callback for use w/ cmUdpGetAvailData()
    void*           cbArg,           // First arg to cbFunc().
    const char*     remoteAddr,      // Remote addr to bind this socket to (or NULL).
    cmUdpPort_t     remotePort,      // Remote port to use with remoteAddr.
    unsigned        recvBufByteCnt,  // Size of the internal receive buffer in bytes. Size of the internal queue and msg receive buffer. No single msg can exceed this size. 
    unsigned        timeOutMs );     // Receive time-out in milliseconds

  cmUdpRC_t cmUdpFinal( cmUdpH_t h );
  bool      cmUdpIsValid( cmUdpH_t h );

  // This function may not return a useful value until the 
  // socket has gone into 'listen' mode.
  const struct sockaddr_in* cmUdpLocalAddr( cmUdpH_t h );

  // Set a destination address for this socket. Once a destination address is set
  // the caller may use cmUdpSend() to communicate with the specified remote socket
  // without having to specify an destination address on each call.
  cmUdpRC_t cmUdpConnect( cmUdpH_t h, const char* remoteAddr, cmUdpPort_t remotePort );

  // Send a message to a remote UDP socket. Use the function cmUdpInitAddr() to setup 
  // the 'sockaddr_in' arg. for cmUdpSendTo().
  cmUdpRC_t cmUdpSend(    cmUdpH_t h, const char* data, unsigned dataByteCnt );
  cmUdpRC_t cmUdpSendTo(  cmUdpH_t h, const char* data, unsigned dataByteCnt, const struct sockaddr_in* remoteAddr );
  cmUdpRC_t cmUdpSend2(   cmUdpH_t h, const char* data, unsigned dataByteCnt, const char* remoteAddr, cmUdpPort_t remotePort );

  // Receive incoming messages by directly checking the internal
  // socket for waiting data.  This function is used to receive
  // incoming data when the internal listening thread is not used.
  // Note that if kBlockingUdpFl was set
  // in cmUdpInit() that this call will block for available data
  // or for 'timeOutMs' milliseconds, whichever comes first.
  // If kNonBlockingUdpFl was set in cmUdpInit() then the function
  // will return immediately if no incoming messages are waiting.
  // If non-NULL *recvByteCntPtr is set to the length of the received
  // message or 0 if no msg was received. 
  cmUdpRC_t cmUdpRecv(    cmUdpH_t h, char* data, unsigned dataByteCnt, struct sockaddr_in* fromAddr, unsigned* recvByteCntPtr );

  // Start a listening thread. If the queue is enabled then incoming 
  // messages are received as they arrive and stored in an internal 
  // queue until the client requests them using cmUdpGetAvailData().
  // If the queue is disabled the messages are transmitted immediately
  // to the client in the context of the internal listening thread.
  cmUdpRC_t cmUdpEnableListen( cmUdpH_t h, bool enableFl );

  // Enable/disable the internal queue.  If the queue is disabled then
  // the receive callback function will be called immediately upon reception
  // of the incoming message in the context of the internal listening thread.  
  // If the queue is enabled then incoming
  // messages will be queued until they are transmitted by calling
  // cmUdpGetAvailData().
  bool      cmUdpIsQueueEnabled( cmUdpH_t h );
  void      cmUdpQueueEnable( cmUdpH_t h, bool enableFl );

  // Return the size of the next available message waiting in the
  // internal data queue.
  unsigned  cmUdpAvailDataByteCount( cmUdpH_t h );

  // The Call this function to receieve any data waiting in the internal queue.
  // Set 'data' to NULL to receive the data via the callback provided
  // in cmUdpAlloc().
  // On input *dataByteCntPtr must be set to the number of bytes in data[].
  // On return *dataByteCntPtr is set to the actual number of bytes copied into data[].
  // If fromAddr is non-NULL it is set to the data source address.
  cmUdpRC_t cmUdpGetAvailData( cmUdpH_t h, char* data, unsigned* dataByteCntPtr, struct sockaddr_in* fromAddr );

  void      cmUdpReport( cmUdpH_t h, cmRpt_t* rpt );

  // Prepare a struct sockadddr_in for use with cmUdpSendTo()
  cmUdpRC_t cmUdpInitAddr( cmUdpH_t h, const char* addrStr, cmUdpPort_t portNumber, struct sockaddr_in* retAddrPtr );

  const cmChar_t*  cmUdpAddrToString( cmUdpH_t h, const struct sockaddr_in* addr );

  bool             cmUdpAddrIsEqual( const struct sockaddr_in* a0, const struct sockaddr_in* a1 );

  const cmChar_t* cmUdpHostName( cmUdpH_t h );

  cmUdpRC_t cmUdpTest( cmCtx_t* ctx, const char* remoteIpAddr, cmUdpPort_t port );
  cmUdpRC_t cmUdpTestV( cmCtx_t* ctx, unsigned argc, const char* argv[]);

  //)
  
#ifdef __cplusplus
}
#endif

#endif
