/// \file cmAudioPort.h
/// \brief Cross platform audio I/O interface.
///
/// This interface provides data declarations for platform dependent 
/// audio I/O functions. The implementation for the functions are
/// in platform specific modules. See cmAudioPortOsx.c and cmAudioPortAlsa.c.
///
/// ALSA Notes:  
/// Assign capture device to line or mic input:
/// amixer -c 0 cset iface=MIXER,name='Input Source',index=0 Mic
/// amixer -c 0 cset iface=MIXER,name='Input Source',index=0 Line
///
/// -c 0                            select the first card
/// -iface=MIXER                    the cset is targetting the MIXER component
/// -name='Input Source',index=0    the control to set is the first 'Input Source'
/// Note that the 'Capture' control sets the input gain.
///
/// See alsamixer for a GUI to accomplish the same thing.
///
///
/// Usage example and testing code:
/// See cmApPortTest() and cmAudioSysTest().
/// \snippet cmAudioPort.c cmAudioPortExample
///
#ifndef cmAudioPort_h
#define cmAudioPort_h

#ifdef __cplusplus
extern "C" {
#endif

  typedef unsigned cmApRC_t;      ///< Audio port interface result code.
  typedef float    cmApSample_t;  ///< Audio sample type.

  enum 
  { 
    kOkApRC =0, 
    kSysErrApRC,
    kInvalidDevIdApRC,
    kAudioPortFileFailApRC,
    kThreadFailApRC
  };

  // cmApAudioPacket_t flags
  enum
  {
    kInterleavedApFl = 0x01,  ///< The audio samples are interleaved.
    kFloatApFl       = 0x02   ///< The audio samples are single precision floating point values.
  };

  /// Audio packet record used by the cmApAudioPacket_t callback.
  /// Audio ports send and receive audio using this data structure. 
  typedef struct
  {
    unsigned devIdx;         ///< device associated with packet
    unsigned begChIdx;       ///< first device channel 
    unsigned chCnt;          ///< count of channels
    unsigned audioFramesCnt; ///< samples per channel (see note below)
    unsigned bitsPerSample;  ///< bits per sample word
    unsigned flags;          ///< kInterleavedApFl | kFloatApFl
    void*    audioBytesPtr;  ///< pointer to sample data
    void*    userCbPtr;      ///< user defined value passed in cmApDeviceSetup()
    cmTimeSpec_t timeStamp;  ///< Packet time stamp.
  }  cmApAudioPacket_t; 


  /// Audio port callback signature. 
  /// inPktArray[inPktCnt] are full packets of audio coming from the ADC to the application.
  /// outPktArray[outPktCnt] are empty packets of audio which will be filled by the application 
  /// and then sent to the DAC.
  ///
  /// The value of audioFrameCnt  gives the number of samples per channel which are available
  /// in the packet data buffer 'audioBytesPtr'.  The callback function may decrease this number in
  /// output packets if the number of samples available is less than the size of the buffer.
  /// It is the responsibility of the calling audio port to notice this change and pass the new,
  /// decreased number of samples to the hardware.
  ///
  /// In general it should be assmed that this call is made from a system thread which is not 
  /// the same as the application thread.
  /// The usual thread safety precautions should therefore be taken if this function implementation
  /// interacts with data structures also handled by the application. The audio buffer class (\see cmApBuf.h) 
  /// is designed to provide a safe and efficient way to communicate between
  /// the audio thread and the application.
  typedef void (*cmApCallbackPtr_t)( cmApAudioPacket_t* inPktArray, unsigned inPktCnt, cmApAudioPacket_t* outPktArray, unsigned outPktCnt );

  /// Setup the audio port management object for this machine.
  cmApRC_t      cmApInitialize( cmRpt_t* rpt );

  /// Stop all audio devices and release any resources held 
  /// by the audio port management object.
  cmApRC_t      cmApFinalize();

  /// Return the count of audio devices attached to this machine.
  unsigned      cmApDeviceCount();

  /// Get a textual description of the device at index 'devIdx'.
  const char*   cmApDeviceLabel(          unsigned devIdx );

  /// Given an audio device label return the associated device index.
  unsigned      cmApDeviceLabelToIndex( const cmChar_t* label );

  /// Get the count of audio input or output channesl on device at index 'devIdx'.
  unsigned      cmApDeviceChannelCount(   unsigned devIdx, bool inputFl );

  /// Get the current sample rate of a device.  Note that if the device has both
  /// input and output capability then the sample rate is the same for both.
  double        cmApDeviceSampleRate(     unsigned devIdx );

  /// Get the count of samples per callback for the input or output for this device.
  unsigned      cmApDeviceFramesPerCycle( unsigned devIdx, bool inputFl );

  /// Configure a device.  
  /// All devices must be setup before they are started.
  /// framesPerCycle is the requested number of samples per audio callback. The
  /// actual number of samples made from a callback may be smaller. See the note
  /// regarding this in cmApAudioPacket_t.
  /// If the device cannot support the requested configuration then the function
  /// will return an error code.
  /// If the device is started when this function is called then it will be 
  /// automatically stopped and then restarted following the reconfiguration.
  /// If the reconfiguration fails then the device may not be restared.
  cmApRC_t      cmApDeviceSetup(          
    unsigned          devIdx, 
    double            srate, 
    unsigned          framesPerCycle, 
    cmApCallbackPtr_t callbackPtr,
    void*             userCbPtr );

  /// Start a device. Note that the callback may be made prior to this function returning.
  cmApRC_t      cmApDeviceStart( unsigned devIdx );

  /// Stop a device.
  cmApRC_t      cmApDeviceStop(  unsigned devIdx );

  /// Return true if the device is currently started.
  bool          cmApDeviceIsStarted( unsigned devIdx );

  /// Print a report of all the current audio device configurations.
  void          cmApReport( cmRpt_t* rpt );

  /// Test the audio port by synthesizing a sine signal or passing audio through
  /// from the input to the output.  This is also a good example of how to 
  /// use all of the functions in the interface.
  /// Set runFl to false to print a report without starting any audio devices.
  /// See cmAudiotPortTest.c for usage example for this function.
  int           cmApPortTest(bool runFl, cmRpt_t* rpt, int argc, const char* argv[] );

#ifdef __cplusplus
}
#endif

#endif
