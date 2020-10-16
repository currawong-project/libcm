//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#ifndef cmAudioPortOsx_h
#define cmAudioPortOsx_h

#ifdef __cplusplus
extern "C" {
#endif

  cmApRC_t      cmApOsxInitialize( cmRpt_t* rpt, unsigned baseApDevIdx );
  cmApRC_t      cmApOsxFinalize();
  cmApRC_t      cmApOsxDeviceCount();
  const char*   cmApOsxDeviceLabel(          unsigned devIdx );
  unsigned      cmApOsxDeviceChannelCount(   unsigned devIdx, bool inputFl );
  double        cmApOsxDeviceSampleRate(     unsigned devIdx );
  unsigned      cmApOsxDeviceFramesPerCycle( unsigned devIdx, bool inputFl );
  cmApRC_t      cmApOsxDeviceSetup(          
    unsigned          devIdx, 
    double            srate, 
    unsigned          framesPerCycle, 
    cmApCallbackPtr_t callbackPtr,
    void*             userCbPtr );

  cmApRC_t      cmApOsxDeviceStart( unsigned devIdx );
  cmApRC_t      cmApOsxDeviceStop(  unsigned devIdx );
  bool          cmApOsxDeviceIsStarted( unsigned devIdx );

  void          cmApOsxTest( cmRpt_t* );

#ifdef __cplusplus
}
#endif

#endif
