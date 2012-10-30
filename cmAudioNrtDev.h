#ifndef cmAudioNrtDev_h
#define cmAudioNrtDev_h

#ifdef __cplusplus
extern "C" {
#endif

  cmApRC_t cmApNrtAllocate( cmRpt_t* rpt );

  cmApRC_t cmApNrtFree();

  cmApRC_t cmApNrtCreateDevice( 
    const cmChar_t* label,
    double          srate,
    unsigned        iChCnt,
    unsigned        oChCnt,
    unsigned        cbPeriodMs );
  

  /// Setup the audio port management object for this machine.
  cmApRC_t      cmApNrtInitialize( cmRpt_t* rpt, unsigned baseApDevIdx );

  /// Stop all audio devices and release any resources held 
  /// by the audio port management object.
  cmApRC_t      cmApNrtFinalize();

  /// Return the count of audio devices attached to this machine.
  unsigned      cmApNrtDeviceCount();

  /// Get a textual description of the device at index 'devIdx'.
  const char*   cmApNrtDeviceLabel(          unsigned devIdx );

  /// Get the count of audio input or output channesl on device at index 'devIdx'.
  unsigned      cmApNrtDeviceChannelCount(   unsigned devIdx, bool inputFl );

  /// Get the current sample rate of a device.  Note that if the device has both
  /// input and output capability then the sample rate is the same for both.
  double        cmApNrtDeviceSampleRate(     unsigned devIdx );

  /// Get the count of samples per callback for the input or output for this device.
  unsigned      cmApNrtDeviceFramesPerCycle( unsigned devIdx, bool inputFl );

  /// Configure a device.  
  cmApRC_t      cmApNrtDeviceSetup(          
    unsigned          devIdx, 
    double            srate, 
    unsigned          framesPerCycle, 
    cmApCallbackPtr_t callbackPtr,
    void*             userCbPtr );

  /// Start a device. Note that the callback may be made prior to this function returning.
  cmApRC_t      cmApNrtDeviceStart( unsigned devIdx );

  /// Stop a device.
  cmApRC_t      cmApNrtDeviceStop(  unsigned devIdx );

  /// Return true if the device is currently started.
  bool          cmApNrtDeviceIsStarted( unsigned devIdx );


#ifdef __cplusplus
}
#endif

#endif
