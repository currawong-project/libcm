#ifndef cmMidiPort_h
#define cmMidiPort_h

#ifdef __cplusplus
extern "C" {
#endif

  typedef unsigned cmMpRC_t;

  // Flags used to identify input and output ports on MIDI devices
  enum 
  { 
    kInMpFl  = 0x01, 
    kOutMpFl = 0x02 
  };


  enum
  {
    kOkMpRC = cmOkRC,
    kCfStringErrMpRC,
    kLHeapErrMpRC,
    kThreadErrMpRC,
    kSysErrMpRC,
    kInvalidArgMpRC,
    kMemAllocFailMpRC,
    kNotImplMpRC,
    kCbNotFoundMpRC

  };

  typedef void (*cmMpCallback_t)( const cmMidiPacket_t* pktArray, unsigned pktCnt );


  //===============================================================================================
  // MIDI Parser
  //

  typedef cmHandle_t cmMpParserH_t;

  // 'cbFunc' and 'cbDataPtr' are optional.  If 'cbFunc' is not supplied in the call to
  // cmMpParserCreate() it may be supplied later by cmMpParserInstallCallback().
  // 'bufByteCnt' defines is the largest complete system-exclusive message the parser will 
  // by able to transmit. System-exclusive messages larger than this will be broken into 
  // multiple sequential callbacks. 
  cmMpParserH_t cmMpParserCreate( unsigned devIdx, unsigned portIdx, cmMpCallback_t cbFunc, void* cbDataPtr, unsigned bufByteCnt, cmRpt_t* rpt );
  void          cmMpParserDestroy(    cmMpParserH_t* hp );
  unsigned      cmMpParserErrorCount( cmMpParserH_t h );
  void          cmMpParseMidiData(    cmMpParserH_t h, const cmTimeSpec_t* timestamp, const cmMidiByte_t* buf, unsigned bufByteCnt );

  // The following two functions are intended to be used togetther.
  // Use cmMpParserMidiTriple() to insert pre-parsed msg's to the output buffer,
  // and then use cmMpParserTransmit() to send the buffer via the parsers callback function.
  // Set the data bytes to 0xff if they are not used by the message.
  cmMpRC_t      cmMpParserMidiTriple( cmMpParserH_t h, const cmTimeSpec_t* timestamp, cmMidiByte_t status, cmMidiByte_t d0, cmMidiByte_t d1 );
  cmMpRC_t      cmMpParserTransmit(    cmMpParserH_t h ); 

  // Install/Remove additional callbacks.
  cmMpRC_t      cmMpParserInstallCallback( cmMpParserH_t h, cmMpCallback_t cbFunc, void* cbDataPtr );
  cmMpRC_t      cmMpParserRemoveCallback(  cmMpParserH_t h, cmMpCallback_t cbFunc, void* cbDataPtr );

  // Returns true if the parser uses the given callback.
  bool          cmMpParserHasCallback(     cmMpParserH_t h, cmMpCallback_t cbFunc, void* cbDataPtr );


  //===============================================================================================
  // MIDI Device Interface
  //

  // 'cbFunc' and 'cbDataPtr' are optional (they may be set to NULL).  In this case
  // 'cbFunc' and 'cbDataPtr' may be set in a later call to cmMpInstallCallback().
  cmMpRC_t    cmMpInitialize( cmCtx_t* ctx, cmMpCallback_t cbFunc, void* cbDataPtr, unsigned parserBufByteCnt, const char* appNameStr );
  cmMpRC_t    cmMpFinalize();
  bool        cmMpIsInitialized();

  unsigned    cmMpDeviceCount();
  const char* cmMpDeviceName(       unsigned devIdx );
  unsigned    cmMpDeviceNameToIndex(const cmChar_t* name);
  unsigned    cmMpDevicePortCount(  unsigned devIdx, unsigned flags );
  const char* cmMpDevicePortName(   unsigned devIdx, unsigned flags, unsigned portIdx );
  unsigned    cmMpDevicePortNameToIndex( unsigned devIdx, unsigned flags, const cmChar_t* name );
  cmMpRC_t    cmMpDeviceSend(       unsigned devIdx, unsigned portIdx, cmMidiByte_t st, cmMidiByte_t d0, cmMidiByte_t d1 );
  cmMpRC_t    cmMpDeviceSendData(   unsigned devIdx, unsigned portIdx, const cmMidiByte_t* dataPtr, unsigned byteCnt );

  // Set devIdx to -1 to assign the callback to all devices.
  // Set portIdx to -1 to assign the callback to all ports on the specified devices.
  // 
  cmMpRC_t    cmMpInstallCallback( unsigned devIdx, unsigned portIdx, cmMpCallback_t cbFunc, void* cbDataPtr );
  cmMpRC_t    cmMpRemoveCallback(  unsigned devIdx, unsigned portIdx, cmMpCallback_t cbFunc, void* cbDataPtr );
  bool        cmMpUsesCallback(    unsigned devIdx, unsigned portIdx, cmMpCallback_t cbFunc, void* cbDataPtr );

  void        cmMpReport( cmRpt_t* rpt );

  void cmMpTest( cmCtx_t* ctx );

#ifdef __cplusplus
}
#endif

#endif
