#ifndef cmDspPgmPPMain_h
#define cmDspPgmPPMain_h


cmDspInst_t*  _cmDspSys_PresetMgmt( cmDspSysH_t h, const cmChar_t* preLbl, unsigned presetGroupSymId );


typedef struct
{
  cmErr_t*      err; 
  unsigned      nsCnt; // count of noise shapers (4)
  cmDspInst_t*  chCfg; 
  cmDspInst_t*  print;
  unsigned      iChCnt;
  unsigned      oChCnt;
  
} _cmDspPP_Ctx_t;


typedef struct
{
  const cmChar_t* title;
  const cmChar_t* preLbl;
} _cmDspPP_CircDesc_t;


typedef struct
{
  // These fields are provided by _cmDspPP_CircuitSwitchAlloc()

  // public:
  unsigned        circuitCnt; // count of circuits (cn)

  cmDspInst_t**   omix;       // omix[ circuitCnt] one audio mixer per output channel



} cmDspPP_CircuitSwitch_t;


unsigned                   _cmDspPP_CircuitDescCount();
const _cmDspPP_CircDesc_t* _cmDspPP_CircuitDesc( unsigned idx );

cmDspRC_t _cmDspPP_CircuitSwitchAlloc( cmDspSysH_t h, _cmDspPP_Ctx_t* ctx, cmDspPP_CircuitSwitch_t* p, cmDspInst_t* reset, cmDspInst_t** csel, cmDspInst_t** ain, cmDspInst_t** ef );
cmDspRC_t _cmDspPP_CircuitSwitchFree(  cmDspSysH_t h, cmDspPP_CircuitSwitch_t* p);

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif


#endif
