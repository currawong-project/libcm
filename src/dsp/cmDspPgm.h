#ifndef cmDspPgm_h
#define cmDspPgm_h

#ifdef __cplusplus
extern "C" {
#endif

  typedef cmDspRC_t (*cmDspPgmFunc_t)( cmDspSysH_t h, void** userPtrPtr );

  typedef struct
  {
    cmChar_t*      label;
    cmDspPgmFunc_t loadFunc;
    void*          userPtr;
    cmDspPgmFunc_t unloadFunc;

  } _cmDspSysPgm_t;



  _cmDspSysPgm_t* _cmDspSysPgmArrayBase();
  


#ifdef __cplusplus
}
#endif

#endif
