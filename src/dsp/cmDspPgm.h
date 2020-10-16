//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#ifndef cmDspPgm_h
#define cmDspPgm_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"Dataflow program instance interface." kw:[snap] }
  
  typedef cmDspRC_t (*cmDspPgmFunc_t)( cmDspSysH_t h, void** userPtrPtr );

  typedef struct
  {
    cmChar_t*      label;
    cmDspPgmFunc_t loadFunc;
    void*          userPtr;
    cmDspPgmFunc_t unloadFunc;

  } _cmDspSysPgm_t;



  _cmDspSysPgm_t* _cmDspSysPgmArrayBase();
  
  //)

#ifdef __cplusplus
}
#endif

#endif
