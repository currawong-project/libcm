#ifndef cmSerialPort_h
#define cmSerialPort_h

#ifdef __cplusplus
extern "C" {
#endif

  typedef unsigned cmSeRC_t;

  enum
  {
   kOkSeRC = cmOkRC,
   kFlushFailSeRC,
   kSetAttrFailSeRC,
   kCloseFailSeRC,
   kOpenFailSeRC,
   kResourceNotAvailableSeRC,
   kGetAttrFailSeRC,
   kWriteFailSeRC,
   kReadFailSeRC,
   kTimeOutSeRC,
   kThreadErrSeRC
  };


  enum
  {
   kDataBits5SeFl 	= 0x0001,
   kDataBits6SeFl 	= 0x0002,
   kDataBits7SeFl 	= 0x0004,
   kDataBits8SeFl 	= 0x0008,
   kDataBitsSeMask	= 0x000f,
   
   k1StopBitSeFl		= 0x0010,
   k2StopBitSeFl 	  = 0x0020,
   
   kEvenParitySeFl	= 0x0040,
   kOddParitySeFl	  = 0x0080,
   kNoParitySeFl		= 0x0000,

   kDefaultCfgSeFlags = kDataBits8SeFl | k1StopBitSeFl | kNoParitySeFl
  };



  
  typedef void (*cmSeCallbackFunc_t)( void* cbArg, const void* byteA, unsigned byteN );

  typedef cmHandle_t cmSeH_t;
    
  cmSeH_t   cmSeCreate( cmCtx_t* ctx, cmSeH_t* hp, const char* device, unsigned baudRate, unsigned cfgFlags, cmSeCallbackFunc_t cbFunc, void* cbArg, unsigned pollPeriodMs );
  cmSeRC_t  cmSeDestroy(cmSeH_t* hp );

  cmSeRC_t  cmSeSetCallback( cmSeH_t h, cmSeCallbackFunc_t cbFunc, void* cbArg  );
  cmSeRC_t  cmSeStart( cmSeH_t h );

  bool cmSeIsOpen( cmSeH_t h);
    
  cmSeRC_t cmSeSend( cmSeH_t h, const void* byteA, unsigned byteN );


  // Make callback to listener with result of read - Non-blocking
  cmSeRC_t cmSeReceiveCbNb( cmSeH_t h, unsigned* readN_Ref);
  
  // Make callback to listener with result of read - Block for up to timeOutMs.
  cmSeRC_t cmSeReceiveCbTimeOut( cmSeH_t h, unsigned timeOutMs, unsigned* readN_Ref); 

  // Return result of read in buf[bufByteN] - Non-blocking.
  cmSeRC_t cmSeReceiveNb( cmSeH_t h, void* buf, unsigned bufByteN, unsigned* readN_Ref);
    
  // Return result of read in buf[bufByteN] - Block for up to timeOutMs.
  cmSeRC_t cmSeReceive( cmSeH_t h, void* buf, unsigned bufByteN, unsigned timeOutMs, unsigned* readN_Ref );

  const char* cmSeDevice( cmSeH_t h);
    
  // Get the baud rate and cfgFlags used to initialize the port
  unsigned    cmSeBaudRate( cmSeH_t h);
  unsigned    cmSeCfgFlags( cmSeH_t h);

  // Get the baud rate and cfg flags by reading the device.
  // Note the the returned buad rate is a system id rather than the actual baud rate,
  // however the cfgFlags are converted to the same kXXXFl defined in this class.
  unsigned cmSeReadInBaudRate( cmSeH_t h );
  unsigned cmSeReadOutBaudRate( cmSeH_t h);
  unsigned cmSeReadCfgFlags( cmSeH_t h);

  cmSeRC_t cmSePortTest(cmCtx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif
