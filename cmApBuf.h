/// \file cmApBuf.h
/// \brief  Thread-safe audio buffer class.
///
/// This file defines an audio buffer class which handles
/// buffering incoming (recording) and outgoing (playback)
/// samples in a thread-safe manner. 
///
/// Usage example and testing code:
/// See cmApBufTest() and cmAudioSysTest().
/// \snippet cmApBuf.c cmApBufExample
///
/// Notes on channel flags:
/// Disabled channels:  kChFl is cleared
///   cmApBufGet()     
///      in  - return NULL buffer pointers  
///      out - return NULL buffer points
///
///   cmApBufUpdate()
///      in  - incoming samples are set to 0. 
///      out - outgoing samples are set to 0.
///
/// Muted channels: kMuteFl is set 
///   cmApBufUpdate()
///      in  - incoming samples are set to 0. 
///      out - outgoing samples are set to 0.
///
/// Tone channels: kToneFl is set 
///   cmApBufUpdate()
///      in  - incoming samples are filled with a 1k sine tone
///      out - outgoing samples are filled with a 1k sine tone
///

#ifndef cmApBuf_h
#define cmApBuf_h

#ifdef __cplusplus
extern "C" {
#endif

  typedef cmRC_t cmAbRC_t;  ///< Result code type
  
  enum
  {
    kOkAbRC = 0
  };

  /// Allocate and initialize an audio buffer.
  /// devCnt - count of devices this buffer will handle.
  /// meterMs - length of the meter buffers in milliseconds (automatically limit to the range:10 to 1000)
  cmAbRC_t cmApBufInitialize( unsigned devCnt, unsigned meterMs );

  /// Deallocate and release any resource held by an audio buffer allocated via cmApBufInitialize().
  cmAbRC_t cmApBufFinalize();

  /// Configure a buffer for a given device.  
  cmAbRC_t cmApBufSetup( 
    unsigned devIdx,              ///< device to setup
    double   srate,               ///< device sample rate (only required for synthesizing the correct test-tone frequency)
    unsigned dspFrameCnt,         /// dspFrameCnt - count of samples in channel buffers returned via cmApBufGet() 
    unsigned cycleCnt,            ///< number of audio port cycles to store 
    unsigned inChCnt,             ///< input channel count on this device
    unsigned inFramesPerCycle,    ///< maximum number of incoming sample frames on an audio port cycle
    unsigned outChCnt,            ///< output channel count on this device
    unsigned outFramesPerCycle    ///< maximum number of outgoing sample frames in an audio port cycle
                         );

  /// Prime the buffer with 'audioCycleCnt' * outFramesPerCycle samples ready to be played
  cmAbRC_t cmApBufPrimeOutput( unsigned devIdx, unsigned audioCycleCnt );

  /// Notify the audio buffer that a device is being enabled or disabled.
  void     cmApBufOnPortEnable( unsigned devIdx, bool enabelFl );

  /// This function is called asynchronously by the audio device driver to transfer incoming samples to the
  /// the buffer and to send outgoing samples to the DAC. This function is 
  /// intended to be called from the audio port callback function (\see cmApCallbackPtr_t).
  /// This function is thread-safe under the condition where the audio device uses
  /// different threads for input and output.
  ///
  /// Enable Flag: 
  /// Input: If an input channel is disabled then the incoming samples are replaced with zeros.
  /// Output: If an output channel is disabled then the packet samples are set to zeros.
  ///
  /// Tone Flag:
  /// Input: If the tone flag is set on an input channel then the incoming samples are set to a sine tone.
  /// Output: If the tone flag is set on an output channel then the packet samples are set to a sine tone.
  ///
  /// The enable flag has higher precedence than the tone flag therefore disabled channels
  /// will be set to zero even if the tone flag is set.
  cmAbRC_t cmApBufUpdate(
    cmApAudioPacket_t* inPktArray,  ///< full audio packets from incoming audio (from ADC)
    unsigned           inPktCnt,    ///< count of incoming audio packets
    cmApAudioPacket_t* outPktArray, ///< empty audio packet for outgoing audio (to DAC)  
    unsigned           outPktCnt    ///< count of outgoing audio packets
                         );
  /// Channel flags
  enum
  {
    kInApFl     = 0x01,  ///< Identify an input channel
    kOutApFl    = 0x02,  ///< Identify an output channel
    kEnableApFl = 0x04,  ///< Set to enable a channel, Clear to disable. 

    kChApFl     = 0x08,  ///< Used to enable/disable a channel
    kMuteApFl   = 0x10,  ///< Mute this channel
    kToneApFl   = 0x20,  ///< Generate a tone on this channel
    kMeterApFl  = 0x40,  ///< Turn meter's on/off
    kPassApFl   = 0x80   ///< Pass input channels throught to the output. Must use cmApBufGetIO() to implement this functionality.
  
  };

  /// Return the meter window period as set by cmApBufInitialize()
  unsigned cmApBufMeterMs();
  
  // Set the meter update period. THis function limits the value to between 10 and 1000.
  void     cmApBufSetMeterMs( unsigned meterMs );

  /// Returns the channel count set via cmApBufSetup().
  unsigned cmApBufChannelCount( unsigned devIdx, unsigned flags );

  /// Set chIdx to -1 to enable all channels on this device.
  /// Set flags to {kInApFl | kOutApFl} | {kChApFl | kToneApFl | kMeterFl} | { kEnableApFl=on | 0=off }  
  void cmApBufSetFlag( unsigned devIdx, unsigned chIdx, unsigned flags );
  
  /// Return true if the the flags is set.
  bool cmApBufIsFlag( unsigned devIdx, unsigned chIdx, unsigned flags );

  /// Set chIdx to -1 to enable all channels on this device.
  void  cmApBufEnableChannel(   unsigned devIdx, unsigned chIdx, unsigned flags );

  /// Returns true if an input/output channel is enabled on the specified device.
  bool  cmApBufIsChannelEnabled(unsigned devIdx, unsigned chIdx, unsigned flags );

  /// Set the state of the tone generator on the specified channel.
  /// Set chIdx to -1 to apply the change to all channels on this device.
  /// Set flags to {kInApFl | kOutApFl} | { kEnableApFl=on | 0=off }
  void  cmApBufEnableTone(   unsigned devIdx, unsigned chIdx, unsigned flags );

  /// Returns true if an input/output tone is enabled on the specified device.
  bool  cmApBufIsToneEnabled(unsigned devIdx, unsigned chIdx, unsigned flags );

  /// Mute a specified channel.
  /// Set chIdx to -1 to apply the change to all channels on this device.
  /// Set flags to {kInApFl | kOutApFl} | { kEnableApFl=on | 0=off }
  void  cmApBufEnableMute(   unsigned devIdx, unsigned chIdx, unsigned flags );

  /// Returns true if an input/output channel is muted on the specified device.
  bool  cmApBufIsMuteEnabled(unsigned devIdx, unsigned chIdx, unsigned flags );

  /// Set the specified channel to pass through.
  /// Set chIdx to -1 to apply the change to all channels on this device.
  /// Set flags to {kInApFl | kOutApFl} | { kEnableApFl=on | 0=off }
  void  cmApBufEnablePass(   unsigned devIdx, unsigned chIdx, unsigned flags );

  /// Returns true if pass through is enabled on the specified channel.
  bool  cmApBufIsPassEnabled(unsigned devIdx, unsigned chIdx, unsigned flags );

  /// Turn meter data collection on and off.
  /// Set chIdx to -1 to apply the change to all channels on this device.
  /// Set flags to {kInApFl | kOutApFl} | { kEnableApFl=on | 0=off }
  void  cmApBufEnableMeter(   unsigned devIdx, unsigned chIdx, unsigned flags );

  /// Returns true if an input/output tone is enabled on the specified device.
  bool  cmApBufIsMeterEnabled(unsigned devIdx, unsigned chIdx, unsigned flags );

  /// Return the meter value for the requested channel.
  /// Set flags to kInApFl | kOutApFl.
  cmApSample_t cmApBufMeter(unsigned devIdx, unsigned chIdx, unsigned flags );

  /// Set chIdx to -1 to apply the gain to all channels on the specified device.
  void cmApBufSetGain( unsigned devIdx, unsigned chIdx, unsigned flags, double gain );

  /// Return the current gain seting for the specified channel.
  double cmApBufGain( unsigned devIdx, unsigned chIdx, unsigned flags ); 

  /// Get the meter and fault status of the channel input or output channel array of a device.
  /// Set 'flags' to { kInApFl | kOutApFl }.
  /// The returns value is the count of channels actually written to meterArray.
  /// If 'faultCntPtr' is non-NULL then it is set to the faultCnt of the associated devices input or output buffer.
  unsigned cmApBufGetStatus( unsigned devIdx, unsigned flags, double* meterArray, unsigned meterCnt, unsigned* faultCntPtr );

  /// Do all enabled input/output channels on this device have samples available?
  /// 'flags' can be set to either or both kInApFl and kOutApFl
  bool  cmApBufIsDeviceReady( unsigned devIdx, unsigned flags ); 

  /// This function is called by the application to get full incoming sample buffers and
  /// to fill empty outgoing sample buffers.
  /// Upon return each element in bufArray[bufChCnt] holds a pointer to a buffer assoicated 
  /// with an audio channel or to NULL if the channel is disabled.
  /// 'flags' can be set to kInApFl or kOutApFl but not both.
  /// The buffers pointed to by bufArray[] each contain 'dspFrameCnt' samples. Where 
  /// 'dspFrameCnt' was set in the earlier call to cmApBufSetup() for this device.
  /// (see cmApBufInitialize()).
  /// Note that this function just returns audio information it does not
  /// change any cmApBuf() internal states.
  void cmApBufGet(     unsigned devIdx, unsigned flags, cmApSample_t* bufArray[], unsigned bufChCnt );

  /// This function replaces calls to cmApBufGet() and implements pass-through and output 
  /// buffer zeroing: 
  /// 
  /// 1) cmApBufGet(in);
  /// 2) cmApBufGet(out);
  /// 3) Copy through channels marked for 'pass' and set the associated oBufArray[i] channel to NULL.
  /// 4) Zero all other enabled output channels.
  ///
  /// Notes:
  /// 1) The oBufArray[] channels that are disabled or marked for pass-through will 
  /// be set to NULL.
  /// 2) The client is required to use this function to implement pass-through internally.
  /// 3) This function just returns audio information it does not
  /// change any cmApBuf() internal states.
  /// 4) The timestamp pointers are optional.
  void cmApBufGetIO(   unsigned iDevIdx, cmApSample_t* iBufArray[], unsigned iBufChCnt, cmTimeSpec_t* iTimeStampPtr, 
                       unsigned oDevIdx, cmApSample_t* oBufArray[], unsigned oBufChCnt, cmTimeSpec_t* oTimeStampPtr );


  /// The application calls this function each time it completes processing of a bufArray[]
  /// returned from cmApBufGet(). 'flags' can be set to either or both kInApFl and kOutApFl.
  /// This function should only be called from the client thread.
  void cmApBufAdvance( unsigned devIdx, unsigned flags );

    /// Copy all available samples incoming samples from an input device to an output device.
  /// The source code for this example is a good example of how an application should use cmApBufGet()
  /// and cmApBufAdvance().
  void cmApBufInputToOutput( unsigned inDevIdx, unsigned outDevIdx );

  /// Print the current buffer state.
  void cmApBufReport( cmRpt_t* rpt );

  /// Run a buffer usage simulation to test the class. cmAudioPortTest.c calls this function.
  void cmApBufTest( cmRpt_t* rpt );




#ifdef __cplusplus
}
#endif

#endif
