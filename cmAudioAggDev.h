#ifndef cmAudioAggDev_h
#define cmAudioAggDev_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc: "Audio device driver for cmAudioPort which aggregates multiple hardware devices to appear as a single devices." kw:[rt] }
  
  enum
  {
    kOkAgRC = cmOkRC,
    kMustAggTwoAgRC,
    kCantUseStartedDevAgRC,
    kDevAlreadyAggAgRC,
    kInvalidDevIdxAgRC,
    kPhysDevSetupFailAgRC,
    kPhysDevStartFailAgRC,
    kPhysDevStopFailAgRC
  };

  typedef cmRC_t cmAgRC_t;

  /// Allocate/free the aggregate device management system
  cmAgRC_t      cmApAggAllocate( cmRpt_t* rpt );
  cmAgRC_t      cmApAggFree();


  /// Called by cmAudioPort() driver to notify the aggregate device
  /// system that the hardware ports have been initialized.
  /// Setup the aggregate audio device  management object for this machine.
  cmAgRC_t      cmApAggInitialize( cmRpt_t* rpt, unsigned baseApDevIdx );

  /// Called by cmAudioPort() driver to notify the aggregate device
  /// system that the hardware ports have been finalized.
  /// Stop all aggregate audio devices and release any resources held 
  /// by the agg. audio dev. management object.
  cmAgRC_t      cmApAggFinalize();

  /// Create an aggregate device from physical devices.
  /// Set flags to kInApFl, kOutApFl or both to indicate whether the
  /// device should aggregate input audio, output audio or both.
  enum { kInAggFl = 0x01, kOutAggFl = 0x02 };
  cmAgRC_t      cmApAggCreateDevice(
    const cmChar_t* label,
    unsigned       devCnt,
    const unsigned physDevIdxArray[],
    unsigned       flags );


  // Return true if the specified physical device is included 
  // in an aggregated device.
  bool cmApAggIsDeviceAggregated( unsigned physDevIdx );


  /// Return the count of aggregate audio devices attached to this machine.
  cmAgRC_t      cmApAggDeviceCount();

  /// Get a textual description of the device at index 'aggDevIdx'.
  const char*   cmApAggDeviceLabel(          unsigned aggDevIdx );

  /// Get the count of audio input or output channels on device at index 'aggDevIdx'.
  unsigned      cmApAggDeviceChannelCount(   unsigned aggDevIdx, bool inputFl );

  /// Get the current sample rate of a device.  Note that if the device has both
  /// input and output capability then the sample rate is the same for both.
  double        cmApAggDeviceSampleRate(     unsigned aggDevIdx );

  /// Get the count of samples per callback for the input or output for this device.
  unsigned      cmApAggDeviceFramesPerCycle( unsigned aggDevIdx, bool inputFl );

  /// Configure a device.  
  /// All devices must be setup before they are started.
  /// framesPerCycle is the requested number of samples per audio callback. The
  /// actual number of samples made from a callback may be smaller. See the note
  /// regarding this in cmApAggAudioPacket_t.
  /// If the device cannot support the requested configuration then the function
  /// will return an error code.
  /// If the device is started when this function is called then it will be 
  /// automatically stopped and then restarted following the reconfiguration.
  /// If the reconfiguration fails then the device may not be restared.
  cmAgRC_t      cmApAggDeviceSetup(          
    unsigned          aggDevIdx, 
    double            srate, 
    unsigned          framesPerCycle, 
    cmApCallbackPtr_t callbackPtr,
    void*             userCbPtr );

  /// Start a device. Note that the callback may be made prior to this function returning.
  cmAgRC_t      cmApAggDeviceStart( unsigned aggDevIdx );

  /// Stop a device.
  cmAgRC_t      cmApAggDeviceStop(  unsigned aggDevIdx );

  /// Return true if the device is currently started.
  bool          cmApAggDeviceIsStarted( unsigned aggDevIdx );

  int cmApAggTest(  bool runFl, cmCtx_t* ctx, int argc, const char* argv[] );

  //)
  
#ifdef __cplusplus
}
#endif


#endif
