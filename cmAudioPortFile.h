#ifndef cmAudioPortFile_h
#define cmAudioPortFile_h

#ifdef __cplusplus
extern "C" {
#endif


  cmApRC_t      cmApFileInitialize( cmRpt_t* rpt, unsigned baseApDevIdx );
  cmApRC_t      cmApFileFinalize();
  bool          cmApFileIsValid();

  unsigned      cmApFileDeviceCreate( 
    const cmChar_t* devLabel,
    const cmChar_t* iFn,
    const cmChar_t* oFn,
    unsigned        oBits,
    unsigned        oChCnt );

  cmApRC_t      cmApFileDeviceDestroy( unsigned devIdx ); 
  
  unsigned      cmApFileDeviceCount();
  const char*   cmApFileDeviceLabel(          unsigned devIdx );
  unsigned      cmApFileDeviceChannelCount(   unsigned devIdx, bool inputFl );
  double        cmApFileDeviceSampleRate(     unsigned devIdx );
  unsigned      cmApFileDeviceFramesPerCycle( unsigned devIdx, bool inputFl );

  cmApRC_t      cmApFileDeviceSetup(          
    unsigned          devIdx, 
    double            srate, 
    unsigned          framesPerCycle, 
    cmApCallbackPtr_t callbackPtr,
    void*             userCbPtr );


  cmApRC_t      cmApFileDeviceStart( unsigned devIdx );
  cmApRC_t      cmApFileDeviceStop(  unsigned devIdx );
  bool          cmApFileDeviceIsStarted( unsigned devIdx );
  void          cmApFileReport( cmRpt_t* rpt );
  void          cmApFileTest( cmRpt_t* rpt );

#ifdef __cplusplus
}
#endif


#endif
