#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmThread.h"

#include <sys/socket.h> 
#include <netinet/in.h>	
#include <arpa/inet.h>	
#include <fcntl.h>		
#include <unistd.h>  // close

#include "cmUdpPort.h"

#define cmUdp_SYS_ERR (-1)
#define cmUdp_NULL_SOCK (-1)

enum
{
  kIsConnectedUdpFl = 0x01,
  kIsBlockingUdpFl  = 0x02,
  kQueueingUdpFl    = 0x04
};


typedef struct 
{
  cmErr_t         err;
  int             sockH;
  cmUdpCallback_t cbFunc;
  void*           cbArg;
  unsigned        timeOutMs;
  unsigned        flags;
  cmThreadH_t     thH;
  cmTs1p1cH_t     qH;
  unsigned        recvBufByteCnt;
  char*           tempBuf;
  unsigned        timeOutCnt;
  unsigned        recvCnt;
  unsigned        queCbCnt;
  unsigned        errCnt;
  cmChar_t        ntopBuf[ INET_ADDRSTRLEN+1 ]; // use INET6_ADDRSTRLEN for IPv6
} cmUdp_t;

cmUdpH_t cmUdpNullHandle = cmSTATIC_NULL_HANDLE;

#define _cmUdpClear_errno() errno = 0

cmUdp_t* _cmUdpHandleToPtr( cmUdpH_t h )
{
  cmUdp_t* p = (cmUdp_t*)h.h;
  assert(p != NULL);
  return p;
}

cmUdpRC_t _cmUdpFinal( cmUdp_t* p )
{
  cmUdpRC_t rc = kOkUdpRC;

  if( cmThreadIsValid(p->thH) )
    if( cmThreadDestroy(&p->thH) != kOkThRC )
      return cmErrMsg(&p->err,kThreadFailUdpRC,"Listener thread destroy failed.");

  if( cmTs1p1cIsValid(p->qH) )
    if( cmTs1p1cDestroy(&p->qH) != kOkThRC )
      cmErrMsg(&p->err,kQueueFailUdpRC,"Receive data queue destroy failed.");

  cmMemPtrFree(&p->tempBuf);

  // close the socket		
	if( p->sockH != cmUdp_NULL_SOCK )
	{
    _cmUdpClear_errno();
		if( close(p->sockH) != 0 )
			cmErrSysMsg(&p->err,kSockCloseFailUdpRC,errno,"The socket close failed." );
			
		p->sockH = cmUdp_NULL_SOCK;		
	}


  return rc;
}

cmUdpRC_t _cmUdpFree( cmUdp_t* p )
{
  cmUdpRC_t rc;
  if((rc = _cmUdpFinal(p)) != kOkUdpRC )
    return rc;

  cmMemFree(p);
  return rc;  
}

cmUdpRC_t _cmUdpInitAddr( cmUdp_t* p, const char* addrStr, cmUdpPort_t portNumber, struct sockaddr_in* retAddrPtr )
{
	memset(retAddrPtr,0,sizeof(struct sockaddr_in));
	
	if( addrStr == NULL )
		retAddrPtr->sin_addr.s_addr 	= htonl(INADDR_ANY);
	else
	{
    _cmUdpClear_errno();

		if(inet_pton(AF_INET,addrStr,&retAddrPtr->sin_addr) == 0 )
      //if(( retAddrPtr->sin_addr.s_addr 	= inet_addr(addrStr)) == INADDR_NONE )
			return cmErrSysMsg(&p->err,kPtoNFailUdpRC,errno, "The network address string '%s' could not be converted to a netword address structure.",cmStringNullGuard(addrStr) );
	}
	
	//retAddrPtr->sin_len 			= sizeof(struct sockaddr_in);
	retAddrPtr->sin_family 		= AF_INET;
	retAddrPtr->sin_port 			= htons(portNumber);
	
	return kOkUdpRC;	
}

cmUdpRC_t _cmUdpConnect( cmUdp_t* p, const char* remoteAddr, cmUdpPort_t remotePort )
{
  struct sockaddr_in addr;
  cmUdpRC_t          rc;

  // create the remote address		
  if((rc = _cmUdpInitAddr(p, remoteAddr, remotePort,  &addr )) != kOkUdpRC )
    return rc;
		
  _cmUdpClear_errno();

  // ... and connect this socket to the remote address/port
  if( connect(p->sockH, (struct sockaddr*)&addr, sizeof(addr)) == cmUdp_SYS_ERR )
    return cmErrSysMsg(&p->err,kSockConnectFailUdpRC, errno, "Socket connect failed." );

  p->flags = cmSetFlag(p->flags,kIsConnectedUdpFl);

  return rc;
}

cmUdpRC_t cmUdpAlloc( cmCtx_t* ctx, cmUdpH_t* hp )
{
  cmUdpRC_t rc;

  if((rc = cmUdpFree(hp)) != kOkUdpRC )
    return rc;

  cmUdp_t* p = cmMemAllocZ(cmUdp_t,1);

  cmErrSetup(&p->err,&ctx->rpt,"UDP Port");

  p->sockH = cmUdp_NULL_SOCK;

  hp->h = p;

  return rc;
}

cmUdpRC_t cmUdpFree( cmUdpH_t* hp )
{
  cmUdpRC_t rc = kOkUdpRC;
   
  if( hp == NULL || cmUdpIsValid(*hp)==false)
    return rc;

  cmUdp_t* p = _cmUdpHandleToPtr(*hp);

  if((rc = _cmUdpFree(p)) != kOkUdpRC )
    return rc;

  hp->h = NULL;
    
  return rc;
}



cmUdpRC_t cmUdpInit( 
  cmUdpH_t       h, 
  cmUdpPort_t     port,
  unsigned        flags,
  cmUdpCallback_t cbFunc, 
  void*           cbArg,
  const char*     remoteAddr,
  cmUdpPort_t     remotePort,
  unsigned        recvBufByteCnt,
  unsigned        timeOutMs )
{
  cmUdpRC_t rc;

  struct sockaddr_in 	addr;

  cmUdp_t* p = _cmUdpHandleToPtr(h);

  if((rc = _cmUdpFinal(p)) != kOkUdpRC )
    return rc;

  _cmUdpClear_errno();

  // get a handle to the socket
  if(( p->sockH = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP ) ) == cmUdp_SYS_ERR )
    return cmErrSysMsg(&p->err, kSockCreateFailUdpRC, errno, "Socket create failed." );	 
	
  // create the local address		
  if((rc = _cmUdpInitAddr(p, NULL, port,  &addr )) != kOkUdpRC )
    goto errLabel;
			
  // bind the socket to a local address/port	
  if( (bind( p->sockH, (struct sockaddr*)&addr, sizeof(addr))) == cmUdp_SYS_ERR )
  {
    rc = cmErrSysMsg(&p->err,kSockBindFailUdpRC,errno,"Socket bind failed." );
    goto errLabel;
  }

  // if a remote addr was given connect this socket to it
  if( remoteAddr != NULL )
    if((rc = _cmUdpConnect(p,remoteAddr,remotePort)) != kOkUdpRC )
      goto errLabel;

  // if this socket should block
  if( cmIsFlag(flags,kBlockingUdpFl) )
  {
    struct timeval 		timeOut;
    
    // set the socket time out 
    timeOut.tv_sec 	= timeOutMs/1000;
    timeOut.tv_usec = (timeOutMs - (timeOut.tv_sec * 1000)) * 1000;
		
    if( setsockopt( p->sockH, SOL_SOCKET, SO_RCVTIMEO, &timeOut, sizeof(timeOut) ) == cmUdp_SYS_ERR )
    {
      rc = cmErrSysMsg(&p->err,kSockOptSetFailUdpRC,errno, "Attempt to set the socket timeout failed." );
      goto errLabel;
    }

    p->flags = cmSetFlag(p->flags,kIsBlockingUdpFl);

  }
  else
  {

    int opts;
		
    // get the socket options flags
    if( (opts = fcntl(p->sockH,F_GETFL)) < 0 )
    {
      rc = cmErrSysMsg(&p->err,kSockOptSetFailUdpRC,errno, "Attempt to get the socket options flags failed." );
      goto errLabel;
    }
	    
    opts = (opts | O_NONBLOCK);
		
    // set the socket options flags
    if(fcntl(p->sockH,F_SETFL,opts) < 0) 
    {
      rc = cmErrSysMsg(&p->err,kSockOptSetFailUdpRC,errno, "Attempt to set the socket to non-blocking failed." );
      goto errLabel;
    }	    

  }

  if( recvBufByteCnt != 0 )
    p->tempBuf = cmMemAlloc(char,recvBufByteCnt );

  p->timeOutMs      = timeOutMs;
  p->cbFunc         = cbFunc;
  p->cbArg          = cbArg;
  p->recvBufByteCnt = recvBufByteCnt;
  p->timeOutCnt     = 0;
  p->recvCnt        = 0;
  p->queCbCnt       = 0;
  p->errCnt         = 0;

  if( cmIsFlag(flags,kNoQueueUdpFl) == false )
    p->flags = cmSetFlag(p->flags,kQueueingUdpFl);

 errLabel:
  if( rc != kOkUdpRC )
    _cmUdpFree(p);

  return rc;
}

cmUdpRC_t cmUdpFinal( cmUdpH_t h )
{
  cmUdp_t* p = _cmUdpHandleToPtr(h);
  return _cmUdpFinal(p);
}

bool      cmUdpIsValid( cmUdpH_t h )
{ return h.h != NULL; }


cmUdpRC_t cmUdpConnect( cmUdpH_t h, const char* remoteAddr, cmUdpPort_t remotePort )
{
  cmUdp_t* p = _cmUdpHandleToPtr(h);
  return _cmUdpConnect(p,remoteAddr,remotePort);
}

cmUdpRC_t cmUdpSend(    cmUdpH_t h, const char* data, unsigned dataByteCnt )
{
  cmUdp_t* p = _cmUdpHandleToPtr(h);

  _cmUdpClear_errno();

  if( cmIsFlag(p->flags,kIsConnectedUdpFl) == false )
    return cmErrMsg(&p->err,kNotConnectedUdpRC,"cmUdpSend() only works with connected sockets.");

  if( send( p->sockH, data, dataByteCnt, 0 ) == cmUdp_SYS_ERR )
    return cmErrSysMsg(&p->err,kSockSendFailUdpRC,errno,"Send failed.");

  return kOkUdpRC;
}

cmUdpRC_t cmUdpSendTo(  cmUdpH_t h, const char* data, unsigned dataByteCnt, const struct sockaddr_in* remoteAddr )
{
  cmUdp_t* p = _cmUdpHandleToPtr(h);
  
  _cmUdpClear_errno();
   
  if( sendto(p->sockH, data, dataByteCnt, 0, (struct sockaddr*)remoteAddr, sizeof(*remoteAddr)) == cmUdp_SYS_ERR )
    return cmErrSysMsg(&p->err,kSockSendFailUdpRC,errno,"SendTo failed.");

  return kOkUdpRC;
}

cmUdpRC_t cmUdpSend2(  cmUdpH_t h, const char* data, unsigned dataByteCnt, const char* remoteAddr, cmUdpPort_t remotePort )
{
  cmUdpRC_t rc;
  cmUdp_t* p = _cmUdpHandleToPtr(h);
  struct sockaddr_in addr;

  if((rc = _cmUdpInitAddr(p,remoteAddr,remotePort,&addr)) != kOkUdpRC )
    return rc;
  
  return cmUdpSendTo( h, data, dataByteCnt, &addr );
}

cmUdpRC_t cmUdpRecv(    cmUdpH_t h, char* data, unsigned dataByteCnt, struct sockaddr_in* fromAddr, unsigned* recvByteCntPtr )
{
  cmUdp_t*  p                = _cmUdpHandleToPtr(h);
  cmUdpRC_t rc               = kOkUdpRC;
  ssize_t   retVal           = 0;
	socklen_t sizeOfRemoteAddr = fromAddr==NULL ? 0 : sizeof(struct sockaddr_in);
	
  _cmUdpClear_errno();

  if( recvByteCntPtr != NULL )
    *recvByteCntPtr = 0;
	
	if((retVal = recvfrom(p->sockH, data, dataByteCnt, 0, (struct sockaddr*)fromAddr, &sizeOfRemoteAddr )) == cmUdp_SYS_ERR )
    return cmErrSysMsg(&p->err,kSockRecvFailUdpRC,errno,"recvFrom() failed.");
	
  if( recvByteCntPtr != NULL )
    *recvByteCntPtr = retVal;

  return rc;
}

bool _cmUdpThreadCb(void* param)
{
  cmUdp_t*              p = (cmUdp_t*)param;
	fd_set                rdSet;
	struct timeval        timeOut;

  // setup the select() call
  FD_ZERO(&rdSet);
  FD_SET(p->sockH, &rdSet );
		
  timeOut.tv_sec 	= p->timeOutMs/1000;
  timeOut.tv_usec = (p->timeOutMs - (timeOut.tv_sec * 1000)) * 1000;
	
  // NOTE; select() takes the highest socket value plus one of all the sockets in all the sets.
		
  switch( select(p->sockH+1,&rdSet,NULL,NULL,&timeOut) )
  {
    case -1: 	// error
      if( errno != EINTR )
        cmErrSysMsg(&p->err,kSockSelectFailUdpRC,errno,"Select failed.");
      ++p->errCnt;
      break;
			
    case 0: 	// select() timed out	
      ++p->timeOutCnt;
      break;
			
    case 1: 	// (> 0) count of ready descripters
      if( FD_ISSET(p->sockH,&rdSet) )
      {
        struct	sockaddr_in remoteAddr;
        socklen_t           addrByteCnt = sizeof(remoteAddr);
        ssize_t             retByteCnt;

        _cmUdpClear_errno();

        ++p->recvCnt;

        // recv the incoming msg into p->tempBuf[]
        if(( retByteCnt = recvfrom( p->sockH, p->tempBuf, p->recvBufByteCnt, 0, (struct sockaddr*)&remoteAddr, &addrByteCnt )) == cmUdp_SYS_ERR )
          cmErrSysMsg(&p->err,kSockRecvFailUdpRC,errno,"recvfrom() failed.");
        else
        {
          // check for overflow
          if( retByteCnt == p->recvBufByteCnt )
            cmErrMsg(&p->err,kRecvBufOverflowUdpRC,"The receive buffer requires more than %i bytes.",p->recvBufByteCnt);
          else
          {
            // if queueing is enabled
            if( cmIsFlag(p->flags,kQueueingUdpFl ) )
            {
              // enqueue the msg - with the source address appended after the data
              const void*    msgPtrArray[]     = { p->tempBuf, &remoteAddr, p->tempBuf };
              unsigned msgByteCntArray[] = { retByteCnt, sizeof(remoteAddr)  };
              if( cmTs1p1cEnqueueSegMsg( p->qH, msgPtrArray, msgByteCntArray, 2 ) != kOkThRC )
                cmErrMsg(&p->err,kQueueFailUdpRC,"A received msg containing %i bytes was not queued.",retByteCnt);
            }
            else // if queueing is not enabled - transmit the data directly via the callback
              if( p->cbFunc != NULL )
              {
                p->cbFunc(p->cbArg,p->tempBuf,retByteCnt,&remoteAddr);
              }
          }
        }					
      }	
      break;
			
    default:
      { assert(0); }
  } // switch


  return true;
}

cmRC_t _cmUdpQueueCb(void* userCbPtr, unsigned msgByteCnt, const void* msgDataPtr )
{
  cmUdp_t* p = (cmUdp_t*)userCbPtr;

  if( p->cbFunc != NULL )
  {
    struct sockaddr_in addr;

    assert( msgByteCnt >= sizeof(addr));

    const char* dataPtr = (const char*)msgDataPtr;

    // the address of the data source is apppended to the data bytes.
    const char* addrPtr = dataPtr + msgByteCnt - sizeof(addr);
    memcpy(&addr,addrPtr,sizeof(addr));  

    // make the receive callback
    p->cbFunc(p->cbArg,dataPtr,msgByteCnt-sizeof(addr),&addr);

    ++p->queCbCnt;
  }

  return cmOkRC;
}

cmUdpRC_t cmUdpEnableListen( cmUdpH_t h, bool enableFl )
{
  cmUdp_t* p = _cmUdpHandleToPtr(h);

  if( cmThreadIsValid(p->thH) == false && enableFl == true)
  {
    if(cmThreadCreate(&p->thH,_cmUdpThreadCb,p,p->err.rpt) != kOkThRC )
      return cmErrMsg(&p->err,kThreadFailUdpRC,"Listener thread create failed.");

    if(cmTs1p1cCreate(&p->qH,p->recvBufByteCnt,_cmUdpQueueCb,p,p->err.rpt) != kOkThRC )
      return cmErrMsg(&p->err,kQueueFailUdpRC,"Listener data queue create failed.");
  }

  if( cmThreadIsValid(p->thH) )
    if( cmThreadPause( p->thH, enableFl ? 0 : kPauseThFl ) != kOkThRC )
      return cmErrMsg(&p->err,kThreadFailUdpRC,"The listener thread failed to %s.", enableFl ? "pause" : "un-pause" );

  return kOkUdpRC;    
}

bool      cmUdpIsQueueEnabled( cmUdpH_t h )
{
  cmUdp_t* p = _cmUdpHandleToPtr(h);
  return cmIsFlag(p->flags,kQueueingUdpFl);
}

void      cmUdpQueueEnable( cmUdpH_t h, bool enableFl )
{
  cmUdp_t* p = _cmUdpHandleToPtr(h);
  p->flags = cmSetFlag(p->flags,kQueueingUdpFl);
}


unsigned  cmUdpAvailDataByteCount( cmUdpH_t h )
{
  cmUdp_t* p = _cmUdpHandleToPtr(h);
  return cmTs1p1cIsValid(p->qH) ? cmTs1p1cDequeueMsgByteCount( p->qH ) : 0;
}

cmUdpRC_t cmUdpGetAvailData( cmUdpH_t h, char* data, unsigned* dataByteCntPtr, struct sockaddr_in* fromAddr )
{
  cmUdp_t* p = _cmUdpHandleToPtr(h);

  unsigned availByteCnt;
    
  // if a received msg is queued
  if( (availByteCnt = cmTs1p1cAvailByteCount(p->qH)) > 0 )
  {
    // all msg's must have at least a source address
    assert( availByteCnt >= sizeof(*fromAddr) );

    // get the size of the return buffer (or 0 if there is no return buffer)
    unsigned dataByteCnt = (data != NULL && dataByteCntPtr != NULL) ? *dataByteCntPtr : 0;

    if( dataByteCnt == 0 )
      data = NULL;

    // dequeue the msg - if data==NULL then the data will be returned by
    // a call to the callback function provided in cmUdpAlloc().
    if( cmTs1p1cDequeueMsg(p->qH, data, dataByteCnt ) != kOkThRC )
      return cmErrMsg(&p->err,kQueueFailUdpRC,"Data dequeue failed.");
      
    // if a return buffer was given
    if( data != NULL )
    {
      assert( dataByteCntPtr != NULL );
        
      // the source address is appended to the end of the data 
      const char* addrPtr = data + availByteCnt - sizeof(*fromAddr);

      // copy out the source address
      if( fromAddr != NULL )
        memcpy(fromAddr,addrPtr,sizeof(*fromAddr));

      // subtract the address size from the total msg size
      *dataByteCntPtr = availByteCnt - sizeof(*fromAddr);
    }
  }
  return kOkUdpRC;
}

void      cmUdpReport( cmUdpH_t h, cmRpt_t* rpt )
{
  cmUdp_t* p = _cmUdpHandleToPtr(h);  
  cmRptPrintf(rpt,"time-out:%i recv:%i queue cb:%i\n",p->timeOutCnt,p->recvCnt,p->queCbCnt);
}

cmUdpRC_t cmUdpInitAddr( cmUdpH_t h, const char* addrStr, cmUdpPort_t portNumber, struct sockaddr_in* retAddrPtr )
{
  cmUdp_t* p = _cmUdpHandleToPtr(h);
  return _cmUdpInitAddr(p,addrStr,portNumber,retAddrPtr);
}

const cmChar_t* cmUdpAddrToString( cmUdpH_t h, const struct sockaddr_in* addr )
{
  cmUdp_t* p = _cmUdpHandleToPtr(h);

  _cmUdpClear_errno();
  
  if( inet_ntop(AF_INET, addr,  p->ntopBuf, INET_ADDRSTRLEN) == NULL)
  {
    cmErrSysMsg(&p->err,kNtoPFailUdpRC,errno, "Network address to string conversion failed." );
    return NULL;
  }
  
  p->ntopBuf[INET_ADDRSTRLEN]=0;
  return p->ntopBuf;
}
