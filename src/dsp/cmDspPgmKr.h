//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#ifndef cmDspPgmKr_h
#define cmDspPgmKr_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"Dataflow pgm interfaces for 'GUTIM'." kw:[gutim snap] }
  
  cmDspRC_t _cmDspSysPgm_TimeLine( cmDspSysH_t h, void** userPtrPtr );
  cmDspRC_t _cmDspSysPgm_TimeLineLite( cmDspSysH_t h, void** userPtrPtr );
  cmDspRC_t _cmDspSysPgm_TimeLineLiteAf( cmDspSysH_t h, void** userPtrPtr );
  cmDspRC_t _cmDspSysPgm_Tksb(cmDspSysH_t h, void** userPtrPtr );
  cmDspRC_t _cmDspSysPgm_TksbLite(cmDspSysH_t h, void** userPtrPtr );

  //)
  
#ifdef __cplusplus
  }
#endif

#endif
