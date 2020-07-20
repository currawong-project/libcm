#ifndef cmGrPlotAudio_h
#define cmGrPlotAudio_h


#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"Override a cmGrPlotObj to make an efficient audio plot object." kw:[plot audio]}
  
  cmGrPlRC_t  cmGrPlotAudioFileObjCreate(
    cmGrPlObjH_t oH,
    cmAfmFileH_t afH,
    unsigned     audioChIdx );

  //)
  
#ifdef __cplusplus
}
#endif

#endif
