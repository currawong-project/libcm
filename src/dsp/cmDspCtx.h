//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#ifndef cmDspCtx_h
#define cmDspCtx_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"Dataflow global context interface." kw:[snap] }

  typedef cmHandle_t cmDspSysH_t;
  typedef cmHandle_t cmDspStoreH_t;
  

  struct cmAudioSysCtx_str;
  struct cmDspGlobalVar_str;

  // DSP system context passed to many DSP instance functions
  typedef struct
  {
    cmDspSysH_t               dspH;
    cmRpt_t*                  rpt;
    cmCtx_t*                  cmCtx;     // global pgm context
    struct cmCtx_str*         cmProcCtx; // context used by cmProc objects
    struct cmAudioSysCtx_str* ctx;       // audio sub-system context this DSP program is executing within 
    cmLHeapH_t                lhH;
    cmJsonH_t                 jsH;
    cmSymTblH_t               stH;
    cmDspStoreH_t             dsH;
    cmJsonH_t                 rsrcJsH;
    unsigned                  cycleCnt;  // count of DSP execution cycles (multiply by cmDspSamplesPerCycle() to get time since start of DSP system in samples)


    unsigned _disableSymId;
    unsigned _enableSymId;

    unsigned execDurUsecs;
  } cmDspCtx_t;

  //)
  
#ifdef __cplusplus
}
#endif

#endif
