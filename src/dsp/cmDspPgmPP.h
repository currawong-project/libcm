//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#ifndef cmDspPgmPP_h
#define cmDspPgmPP_h

#ifdef __cplusplus
extern "C" {
#endif

//( { file_desc:"'fluxo' 'snap' programs." kw:[sunit fluxo] }

  cmDspRC_t _cmDspSysPgm_NoiseTails3( cmDspSysH_t h, void** userPtrPtr );
  cmDspRC_t _cmDspSysPgm_ChordDetect( cmDspSysH_t h, void** userPtrPtr );
  cmDspRC_t _cmDspSysPgm_CdFx(        cmDspSysH_t h, void** userPtrPtr );
  cmDspRC_t _cmDspSysPgm_Main(        cmDspSysH_t h, void** userPtrPtr );
  
//)

#ifdef __cplusplus
  }
#endif

#endif
