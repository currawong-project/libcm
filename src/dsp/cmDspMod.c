//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
typedef struct
{
  unsigned modSymId;
  unsigned modIndex;
  
} cmDspMod_t;

typedef struct
{
} cmDspModArg_t

typedef struct
{
  unsigned modClassSymId;
  
} cmDspModClass_t;

cmDspModH_t cmDspModAlloc( "fxChain", "ch", modIndex, NULL, ain, "out", iChCnt, ef, "gate", iChCnt, ef, "rms", iChCnt, NULL );


