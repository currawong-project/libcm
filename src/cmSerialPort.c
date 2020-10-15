//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmSerialPort.h"
#include "cmThread.h"

#include <poll.h>
#include <termios.h>
#include <unistd.h>    // close()
#include <fcntl.h>     // O_RDWR
#include <sys/ioctl.h> // TIOCEXCL

typedef struct cmSerialPort_str
{
  cmErr_t               _err;
  cmThreadH_t           _thH; 
  const char*           _deviceStr;
  int                   _deviceH;
  unsigned              _baudRate;
  unsigned              _cfgFlags;
  cmSeCallbackFunc_t    _cbFunc;
  void*                 _cbArg;
  struct termios        _ttyAttrs;
  struct pollfd         _pollfd;
  unsigned              _pollPeriodMs;
} cmSerialPort_t;

cmSerialPort_t* _cmSePtrFromHandle( cmSeH_t h )
{
  cmSerialPort_t* p = (cmSerialPort_t*)h.h;
  assert(p!=NULL);
  return p;
}

void   _cmSeSetClosedState( cmSerialPort_t* p )
{
  if( p->_deviceStr != NULL )
    cmMemFree((char*)(p->_deviceStr));
     
  p->_deviceH = -1;
  p->_deviceStr = NULL;
  p->_baudRate  = 0;
  p->_cfgFlags  = 0;
  p->_cbFunc    = NULL;
  p->_cbArg     = NULL;
      
}
    
cmSeRC_t _cmSeGetAttributes( cmSerialPort_t* p, struct termios* attr ) 
{
  if( tcgetattr(p->_deviceH, attr) == -1 )
    return cmErrSysMsg(&p->_err,kGetAttrFailSeRC,errno,"Error getting tty attributes from %s.",p->_deviceStr);

  return kOkSeRC;
}
    
cmSeRC_t _cmSePoll( cmSerialPort_t* p, unsigned timeOutMs )
{
  cmSeRC_t rc = kOkSeRC;
  int sysRC;

  if((sysRC = poll(&p->_pollfd,1,timeOutMs)) == 0)
    rc = kTimeOutSeRC;
  else
  {
    if( sysRC < 0 )
      rc = cmErrSysMsg(&p->_err,kReadFailSeRC,errno,"Poll failed on serial port.");
  }
  
  return rc;
      
}

bool _cmSeThreadFunc(void* param)
{
  cmSerialPort_t* p = (cmSerialPort_t*)param;
  cmSeH_t h;
  h.h = p;
  unsigned readN;
  if( cmSeIsOpen(h) )
    cmSeReceiveCbTimeOut(h,p->_pollPeriodMs,&readN);
  
      
  
  return true;
}

    
cmSeRC_t _cmSeDestroy( cmSerialPort_t* p )
{
  cmSeRC_t rc = kOkSeRC;


  // stop the thread first
  if( cmThreadDestroy(&p->_thH) != kOkThRC )
  {
    rc = cmErrMsg(&p->_err,kThreadErrSeRC,"Thread destroy failed.");
    goto errLabel;
  }
  
  // Block until all written output has been sent from the device.
  // Note that this call is simply passed on to the serial device driver.
  // See tcsendbreak(3) ("man 3 tcsendbreak") for details.
  if (tcdrain(p->_deviceH) == -1)
  {
    rc = cmErrSysMsg(&p->_err,kFlushFailSeRC,errno,"Error waiting for serial device '%s' to drain.", p->_deviceStr );
    goto errLabel;
  }

  // It is good practice to reset a serial port back to the state in
  // which you found it. This is why we saved the original termios struct
  // The constant TCSANOW (defined in termios.h) indicates that
  // the change should take effect immediately.

  if (tcsetattr(p->_deviceH, TCSANOW, &p->_ttyAttrs) ==  -1)
  {
    rc = cmErrSysMsg(&p->_err,kSetAttrFailSeRC,errno,"Error resetting tty attributes on serial device '%s'.",p->_deviceStr);
    goto errLabel;
  }
	
  if( p->_deviceH != -1 )
  {
    if( close(p->_deviceH ) != 0 )
    {
      rc = cmErrSysMsg(&p->_err,kCloseFailSeRC,errno,"Port close failed on serial dvice '%s'.", p->_deviceStr);
      goto errLabel;
    }
		
    _cmSeSetClosedState(p);
  }

  cmMemPtrFree(&p);

 errLabel:
  return rc;      
}
    

cmSeH_t cmSeCreate( cmCtx_t* ctx, cmSeH_t* hp, const char* deviceStr, unsigned baudRate, unsigned cfgFlags, cmSeCallbackFunc_t cbFunc, void* cbArg, unsigned pollPeriodMs )
{
  cmSeRC_t       rc = kOkSeRC;
  struct termios options;
  cmSeH_t h;
  
  // if the port is already open then close it
  if((rc = cmSeDestroy(hp)) != kOkSeRC )
    return *hp;

  cmSerialPort_t* p = cmMemAllocZ(cmSerialPort_t,1);
  
  cmErrSetup(&p->_err,&ctx->rpt,"Serial Port");

  p->_deviceH = -1;
    
	// open the port		
	if( (p->_deviceH = open(deviceStr, O_RDWR | O_NOCTTY | O_NONBLOCK)) == -1 )
	{
		rc = cmErrSysMsg(&p->_err,kOpenFailSeRC,errno,"Error opening serial '%s'",cmStringNullGuard(deviceStr));
		goto errLabel;;
	}

  // Note that open() follows POSIX semantics: multiple open() calls to 
  // the same file will succeed unless the TIOCEXCL ioctl is issued.
  // This will prevent additional opens except by root-owned processes.
  // See tty(4) ("man 4 tty") and ioctl(2) ("man 2 ioctl") for details.
  
  if( ioctl(p->_deviceH, TIOCEXCL) == -1 )
  {
    rc = cmErrSysMsg(&p->_err,kResourceNotAvailableSeRC,errno,"The serial device '%s' is already in use.", cmStringNullGuard(deviceStr));
    goto errLabel;
  }


  // Now that the device is open, clear the O_NONBLOCK flag so 
  // subsequent I/O will block.
  // See fcntl(2) ("man 2 fcntl") for details.
 	/*
    if (fcntl(_deviceH, F_SETFL, 0) == -1)

    {
    _error("Error clearing O_NONBLOCK %s - %s(%d).", pr.devFilePath.c_str(), strerror(errno), errno);
    goto errLabel;
    }
	*/
	
  // Get the current options and save them so we can restore the 
  // default settings later.
  if (tcgetattr(p->_deviceH, &p->_ttyAttrs) == -1)
  {
    rc = cmErrSysMsg(&p->_err,kGetAttrFailSeRC,errno,"Error getting tty attributes from the device '%s'.",deviceStr);
    goto errLabel;
  }


  // The serial port attributes such as timeouts and baud rate are set by 
  // modifying the termios structure and then calling tcsetattr to
  // cause the changes to take effect. Note that the
  // changes will not take effect without the tcsetattr() call.
  // See tcsetattr(4) ("man 4 tcsetattr") for details.
  options = p->_ttyAttrs;


  // Set raw input (non-canonical) mode, with reads blocking until either 
  // a single character has been received or a 100ms timeout expires.
  // See tcsetattr(4) ("man 4 tcsetattr") and termios(4) ("man 4 termios") 
  // for details.
  cfmakeraw(&options);
  options.c_cc[VMIN] = 1;
  options.c_cc[VTIME] = 1;


  // The baud rate, word length, and handshake options can be set as follows:

 
	// set baud rate
  cfsetspeed(&options, baudRate);
  
  options.c_cflag |=  CREAD | CLOCAL; // ignore modem controls

  // set data word size
  cmClrBits(options.c_cflag, CSIZE); // clear the word size bits
  cmEnaBits(options.c_cflag,	CS5,			cmIsFlag(cfgFlags, kDataBits5SeFl));
  cmEnaBits(options.c_cflag,	CS6,			cmIsFlag(cfgFlags, kDataBits6SeFl));
  cmEnaBits(options.c_cflag,	CS7,			cmIsFlag(cfgFlags, kDataBits7SeFl));
  cmEnaBits(options.c_cflag,	CS8,			cmIsFlag(cfgFlags, kDataBits8SeFl));

  cmClrBits(options.c_cflag, PARENB); // assume no-parity

  // if the odd or even parity flag is set
  if( cmIsFlag( cfgFlags, kEvenParitySeFl) || cmIsFlag( cfgFlags, kOddParitySeFl ) )
  {
    cmSetBits(options.c_cflag,	PARENB);
    	
    if( cmIsFlag(cfgFlags, kOddParitySeFl ) )
      cmSetBits( options.c_cflag,	PARODD);
  }

	// set two stop bits    
  cmEnaBits( options.c_cflag, CSTOPB, cmIsFlag(cfgFlags, k2StopBitSeFl));
    
    		    
  // set hardware flow control
  //cmEnaBits(options.c_cflag,		CCTS_OFLOW, 	cmIsFlag(cfgFlags, kCTS_OutFlowCtlFl)); 
	//cmEnaBits(options.c_cflag, 	CRTS_IFLOW, 	cmIsFlag(cfgFlags, kRTS_InFlowCtlFl));
	//cmEnaBits(options.c_cflag, 	CDTR_IFLOW, 	cmIsFlag(cfgFlags, kDTR_InFlowCtlFl));
	//cmEnaBits(options.c_cflag, 	CDSR_OFLOW, 	cmIsFlag(cfgFlags, kDSR_OutFlowCtlFl));
	//cmEnaBits(options.c_cflag, 	CCAR_OFLOW, 	cmIsFlag(cfgFlags, kDCD_OutFlowCtlFl));
    
	cmClrBits(options.c_cflag,CRTSCTS); // turn-off hardware flow control

	// 7 bit words, enable even parity, CTS out ctl flow, RTS in ctl flow
	// note: set PARODD and PARENB to enable odd parity)
	//options.c_cflag |= (CS7 | PARENB | CCTS_OFLOW | CRTS_IFLOW );

  // Cause the new options to take effect immediately.
  if (tcsetattr(p->_deviceH, TCSANOW, &options) == -1)
  {

    rc = cmErrSysMsg(&p->_err,kSetAttrFailSeRC,errno,"Error setting tty attributes on serial device %.", deviceStr);
    goto errLabel;
  }
  
  memset(&p->_pollfd,0,sizeof(p->_pollfd));
  p->_pollfd.fd     = p->_deviceH;
  p->_pollfd.events = POLLIN;
  
  p->_deviceStr    = cmMemAllocStr( deviceStr );
  p->_baudRate     = baudRate;
	p->_cfgFlags     = cfgFlags;
  p->_cbFunc       = cbFunc;
  p->_cbArg        = cbArg;
  p->_pollPeriodMs = pollPeriodMs;

  // create the listening thread
  if( cmThreadCreate( &p->_thH, _cmSeThreadFunc, p, &ctx->rpt) != kOkThRC )
  {
    rc = cmErrMsg(&p->_err,kThreadErrSeRC,"Thread initialization failed.");
    goto errLabel;
  }

  if( hp != NULL )
    hp->h = p;
  else
    h.h = p;
  
 errLabel:
  if( rc != kOkSeRC )
  {
    _cmSeDestroy(p);
    h.h = NULL;
	}
  
  return hp != NULL ? *hp : h;
  
}

cmSeRC_t cmSeDestroy(cmSeH_t* hp )
{
  cmSeRC_t rc = kOkSeRC; 
  
  if( hp==NULL || !cmSeIsOpen(*hp) )
    return kOkSeRC;

  cmSerialPort_t* p = _cmSePtrFromHandle(*hp);

  if((rc = _cmSeDestroy(p)) != kOkSeRC )
    return rc;

  hp->h = NULL;
  
  return rc;
}

cmSeRC_t  cmSeSetCallback( cmSeH_t h, cmSeCallbackFunc_t cbFunc, void* cbArg  )
{
  cmSerialPort_t* p = _cmSePtrFromHandle(h);
  p->_cbFunc  = cbFunc;
  p->_cbArg   = cbArg;
  return kOkSeRC;
}

cmSeRC_t  cmSeStart( cmSeH_t h )
{
  cmSerialPort_t* p = _cmSePtrFromHandle(h);
  
  if( cmThreadPause(p->_thH,0) != kOkThRC )
    return cmErrMsg(&p->_err,kThreadErrSeRC,0,"Thread start failed.");
  
  return kOkSeRC;
}



bool cmSeIsOpen( cmSeH_t h)
{
  if( h.h == NULL )
    return false;
  
  cmSerialPort_t* p = _cmSePtrFromHandle(h);
  return p->_deviceH != -1;
}

cmSeRC_t cmSeSend( cmSeH_t h, const void* byteA, unsigned byteN )
{
  cmSeRC_t        rc = kOkSeRC;
  cmSerialPort_t* p  = _cmSePtrFromHandle(h);
      
  if( !cmSeIsOpen(h)  )
    return cmErrWarnMsg( &p->_err, kResourceNotAvailableSeRC, "An attempt was made to transmit from a closed serial port.");
        
  if( byteN == 0 )
    return rc;
        
  // implement a non blocking write - if less than all the bytes were written then iterate
  unsigned i = 0;
  do
  {
    int n = 0;
    if((n = write( p->_deviceH, ((char*)byteA)+i, byteN-i )) == -1 )
    {
      rc = cmErrSysMsg(&p->_err,kWriteFailSeRC,errno,"Write failed on serial port '%s'.", p->_deviceStr );
      break;
    }

    i += n;

      
  }while( i<byteN );

  return rc;
  
}


cmSeRC_t cmSeReceiveCbNb( cmSeH_t h, unsigned* readN_Ref)
{
  cmSeRC_t        rc   = kOkSeRC;
  cmSerialPort_t* p    = _cmSePtrFromHandle(h);
  const unsigned  bufN = 512;
  char            buf[ bufN ];

  if( readN_Ref != NULL)
    *readN_Ref = 0;

  if((rc = cmSeReceiveNb(h,buf,bufN,readN_Ref)) == kOkSeRC )
    if( readN_Ref > 0 && p->_cbFunc != NULL )
      p->_cbFunc( p->_cbArg, buf, *readN_Ref );

  return rc;
  
}
  
cmSeRC_t cmSeReceiveCbTimeOut( cmSeH_t h, unsigned timeOutMs, unsigned* readN_Ref)
{
  cmSeRC_t        rc;
  cmSerialPort_t* p = _cmSePtrFromHandle(h);

  if((rc = _cmSePoll(p,timeOutMs)) == kOkSeRC )
    rc = cmSeReceiveCbNb(h,readN_Ref);
  return rc;
  
}

cmSeRC_t cmSeReceiveNb( cmSeH_t h, void* buf, unsigned bufN, unsigned* readN_Ref)
{
  cmSeRC_t        rc = kOkSeRC;
  cmSerialPort_t* p  = _cmSePtrFromHandle(h);

  if( readN_Ref != NULL )
    *readN_Ref = 0;
  
  if( !cmSeIsOpen(h)  )
    return cmErrWarnMsg(&p->_err, kResourceNotAvailableSeRC, "An attempt was made to read from a closed serial port.");
  
  int       n    = 0;
  
  // if attempt to read the port succeeded ...
  if((n =read( p->_deviceH, buf, bufN )) != -1 )
    *readN_Ref = n;
  else
  {
    // ... or failed and it wasn't because the port was empty
    if( errno != EAGAIN)
      rc = cmErrSysMsg(&p->_err,kReadFailSeRC,errno,"An attempt to read the serial port '%s' failed.", p->_deviceStr );
  }
    
  return rc;
}
    
cmSeRC_t cmSeReceive( cmSeH_t h, void* buf, unsigned bufByteN, unsigned timeOutMs, unsigned* readN_Ref )
{
  cmSeRC_t        rc = kOkSeRC;
  cmSerialPort_t* p  = _cmSePtrFromHandle(h);
  
  if((rc = _cmSePoll(p,timeOutMs)) == kOkSeRC )
    rc = cmSeReceiveNb(h,buf,bufByteN,readN_Ref);
  
  return rc;  
}

const char* cmSeDevice( cmSeH_t h)
{
  cmSerialPort_t* p = _cmSePtrFromHandle(h);
  return p->_deviceStr;
}
    
unsigned    cmSeBaudRate( cmSeH_t h)
{
  cmSerialPort_t* p = _cmSePtrFromHandle(h);
  return p->_baudRate;
}

unsigned    cmSeCfgFlags( cmSeH_t h)
{
  cmSerialPort_t* p = _cmSePtrFromHandle(h);
  return p->_cfgFlags;
}

unsigned cmSeReadInBaudRate( cmSeH_t h )
{
	struct termios attr;
  cmSerialPort_t* p = _cmSePtrFromHandle(h);
	
	if((_cmSeGetAttributes(p,&attr)) != kOkSeRC )
		return 0;

	return cfgetispeed(&attr);	
  
}

unsigned cmSeReadOutBaudRate( cmSeH_t h)
{
	struct termios attr;
  cmSerialPort_t* p = _cmSePtrFromHandle(h);
	
	if((_cmSeGetAttributes(p,&attr)) != kOkSeRC )
		return 0;
		
	return cfgetospeed(&attr);	
  
}

unsigned cmSeReadCfgFlags( cmSeH_t h)
{
	struct termios attr;	
	unsigned result = 0;
  cmSerialPort_t* p = _cmSePtrFromHandle(h);

	if((_cmSeGetAttributes(p,&attr)) == false )
		return 0;

	switch( attr.c_cflag & CSIZE )
	{
		case CS5:
			cmSetBits( result, kDataBits5SeFl);
			break;
			
		case CS6:
			cmSetBits( result, kDataBits6SeFl );
			break;
			
		case CS7:
			cmSetBits( result, kDataBits7SeFl);
			break;
			
		case CS8:
			cmSetBits( result, kDataBits8SeFl);
			break;
	}
	
	cmEnaBits( result, k2StopBitSeFl, cmIsFlag(  attr.c_cflag, CSTOPB ));
	cmEnaBits( result, k1StopBitSeFl, !cmIsFlag( attr.c_cflag, CSTOPB ));

	if( cmIsFlag( attr.c_cflag, PARENB ) )
	{
		cmEnaBits( result, kOddParitySeFl, 	cmIsFlag( attr.c_cflag, PARODD ));
		cmEnaBits( result, kEvenParitySeFl, 	!cmIsFlag( attr.c_cflag, PARODD ));
	}		
	
	return result;
  
}


//====================================================================================================
//
//


void _cmSePortTestCb( void* arg, const void* byteA, unsigned byteN )
{
  const char* text = (const char*)byteA;
      
  for(unsigned i=0; i<byteN; ++i)
    printf("%c:%i ",text[i],(int)text[i]);

  if( byteN )
    fflush(stdout);      
}

cmSeRC_t cmSePortTest(cmCtx_t* ctx)
{
  // Use this test an Arduino running study/serial/arduino_xmt_rcv/main.c  
  cmSeRC_t                  rc             = kOkSeRC;
  const char*               device         = "/dev/ttyACM0";
  unsigned                  baud           = 38400;
  unsigned                  serialCfgFlags = kDefaultCfgSeFlags;
  unsigned                  pollPeriodMs   = 50;
  cmSeH_t                   h;

  h.h = NULL;
  
  h = cmSeCreate(ctx,&h,device,baud,serialCfgFlags,_cmSePortTestCb,NULL,pollPeriodMs);

  if( cmSeIsOpen(h) )
    cmSeStart(h);

  bool quitFl = false;
  printf("q=quit\n");
  while(!quitFl)
  {
    char c = getchar();
    
    if( c == 'q')
      quitFl = true;
    else
      if( '0' <= c && c <= 'z' )
        cmSeSend(h,&c,1);
      
    
  }

  cmSeDestroy(&h);
  return rc;
}
