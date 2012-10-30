#ifndef cmDspPgmPP_h
#define cmDspPgmPP_h

#ifdef __cplusplus
extern "C" {
#endif

  cmDspRC_t _cmDspSysPgm_NoiseTails3( cmDspSysH_t h, void** userPtrPtr );
  cmDspRC_t _cmDspSysPgm_ChordDetect( cmDspSysH_t h, void** userPtrPtr );
  cmDspRC_t _cmDspSysPgm_CdFx(        cmDspSysH_t h, void** userPtrPtr );
  cmDspRC_t _cmDspSysPgm_Main(        cmDspSysH_t h, void** userPtrPtr );

#ifdef __cplusplus
  }
#endif

#endif
