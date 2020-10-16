//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
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
