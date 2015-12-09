#ifndef cmAudioFileDev_h
#define cmAudioFileDev_h

#ifdef __cplusplus
extern "C" {
#endif

//( { file_desc:"Implements cmAudioFileDev for reading and writing audio files under control of cmAudioPort.", kw:[audio file rt]}
enum
{
  kOkAfdRC = cmOkRC,
  kAudioFileFailAfdRC,
  kThreadFailAfdRC,
  kRestartFailAfdRC
};

typedef unsigned   cmAfdRC_t;
typedef cmHandle_t cmAfdH_t;

extern cmAfdH_t cmAfdNullHandle;

/// Initialize an audio file device.
/// Called by cmApPortInitialize().
cmAfdRC_t cmAudioFileDevInitialize( 
  cmAfdH_t*       hp, 
  const cmChar_t* label,
  unsigned        devIdx,
  const cmChar_t* iFn,
  const cmChar_t* oFn,
  unsigned        oBits,
  unsigned        oChCnt,
  cmRpt_t*        rpt );

/// Finalize an audio file device.
/// Called by cmApPortFinalize().
cmAfdRC_t cmAudioFileDevFinalize( cmAfdH_t* hp );

/// Return true if 'h' represents a successfully initialized audio file device.
bool      cmAudioFileDevIsValid( cmAfdH_t h );


/// Setup the device. This function must be called prior to cmAudioFileDevStart().
cmAfdRC_t cmAudioFileDevSetup( 
  cmAfdH_t          h, 
  unsigned          baseApDevIdx,
  double            srate, 
  unsigned          framesPerCycle, 
  cmApCallbackPtr_t callbackPtr, 
  void*             cbDataPtr );

/// Return a device label. 
const char* cmAudioFileDevLabel( cmAfdH_t h );

/// Return a channel count.
unsigned    cmAudioFileDevChannelCount( cmAfdH_t h, bool inputFl );

/// Return the sample rate.
double      cmAudioFileDevSampleRate(   cmAfdH_t h );

/// Return frames per cycle.
unsigned    cmAudioFileDevFramesPerCycle( cmAfdH_t h, bool inputFl );

/// Rewind the input file.
cmAfdRC_t cmAudioFileDevRewind( cmAfdH_t h );

/// Start the file device playing/recording.
cmAfdRC_t cmAudioFileDevStart( cmAfdH_t h );

/// Stop the file device playing/recording.
cmAfdRC_t cmAudioFileDevStop( cmAfdH_t h );

/// Return true if the file device is currently started.
bool      cmAudioFileDevIsStarted( cmAfdH_t h ); 

/// 
void      cmAudioFileDevReport( cmAfdH_t h, cmRpt_t* rpt );

void      cmAudioFileDevTest( cmRpt_t* rpt );

//)

#ifdef __cplusplus
}
#endif

#endif
