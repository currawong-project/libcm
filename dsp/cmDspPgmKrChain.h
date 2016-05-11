
#ifndef cmDspPgmKrChain_h
#define cmDspPgmKrChain_h

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  const cmChar_t* tlFn;
  const cmChar_t* tlPrefixPath;
  const cmChar_t* scFn;
  const cmChar_t* tksbFn;
  const cmChar_t* modFn;
  const cmChar_t* measFn;
  const cmChar_t* recordDir;
  const cmChar_t* midiDevice;
  const cmChar_t* midiOutPort;
  const cmChar_t* midiOutPort2;
} krRsrc_t;


cmDspRC_t krLoadRsrc(cmDspSysH_t h, cmErr_t* err, krRsrc_t* r);


  // connection information for 1 transform channel
typedef struct
{
  cmDspInst_t* achan; 
  cmDspInst_t* kr0;  // audio input
  cmDspInst_t* kr1;
  cmDspInst_t* cmp;  // audio output
} cmDspTlXform_t;


void _cmDspSys_TlXformChain(
  cmDspSysH_t     h,
  cmDspTlXform_t* c,
  unsigned        preGrpSymId,
  unsigned        cmpPreGrpSymId,
  cmDspInst_t*    modp,
  unsigned        ach,
  unsigned        mch );


#ifdef __cplusplus
}
#endif

#endif
