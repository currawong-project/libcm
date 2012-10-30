#ifndef cmGrPlotAudio_h
#define cmGrPlotAudio_h


#ifdef __cplusplus
extern "C" {
#endif


  cmGrPlRC_t  cmGrPlotAudioFileObjCreate(
    cmGrPlObjH_t oH,
    cmAfmFileH_t afH,
    unsigned     audioChIdx );

#ifdef __cplusplus
}
#endif

#endif
