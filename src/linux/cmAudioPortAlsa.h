//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#ifndef cmAudioPortAlsa_h
#define cmAudioPortAlsa_h

#ifdef __cplusplus
extern "C" {
#endif

  //{ { label: cmApAlsa kw:[ audio, device, rt ] }
  //
  //( 
  //  ALSA audio device API
  //
  //  This API is used by the cmAudioPort interface when 
  //  the library is compiled for a Linux platform.
  //
  //)

  //[

  cmApRC_t      cmApAlsaInitialize( cmRpt_t* rpt, unsigned baseApDevIdx );
  cmApRC_t      cmApAlsaFinalize();
  cmApRC_t      cmApAlsaDeviceCount();
  const char*   cmApAlsaDeviceLabel(          unsigned devIdx );
  unsigned      cmApAlsaDeviceChannelCount(   unsigned devIdx, bool inputFl );
  double        cmApAlsaDeviceSampleRate(     unsigned devIdx );
  unsigned      cmApAlsaDeviceFramesPerCycle( unsigned devIdx, bool inputFl );
  cmApRC_t      cmApAlsaDeviceSetup(          
    unsigned          devIdx, 
    double            srate, 
    unsigned          framesPerCycle, 
    cmApCallbackPtr_t callbackPtr,
    void*             userCbPtr );

  cmApRC_t      cmApAlsaDeviceStart( unsigned devIdx );
  cmApRC_t      cmApAlsaDeviceStop(  unsigned devIdx );
  bool          cmApAlsaDeviceIsStarted( unsigned devIdx );

  void          cmApAlsaDeviceReport( cmRpt_t* rpt );

  //]
  //}

#ifdef __cplusplus
}
#endif

#endif
