//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#ifndef cmDspStore_h
#define cmDspStore_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"'snap' global variable interface used by units for getting and setting global variables." kw:[snap]}
  extern cmDspStoreH_t cmDspStoreNullHandle;

  cmDspRC_t cmDspStoreAlloc( cmCtx_t* ctx, cmDspStoreH_t* hp, unsigned initStoreCnt, unsigned growStoreCnt );
  
  cmDspRC_t cmDspStoreFree( cmDspStoreH_t *hp );

  bool      cmDspStoreIsValid( cmDspStoreH_t h );
  
  unsigned  cmDspStoreSymToId( cmDspStoreH_t h, unsigned symId );
  unsigned  cmDspStoreIdToSym( cmDspStoreH_t h, unsigned id );
  const cmDspValue_t*  cmDspStoreIdToValue( cmDspStoreH_t h, unsigned id );

  cmDspRC_t cmDspStoreSetValueViaId(  cmDspStoreH_t h, unsigned id,    const cmDspValue_t* val );

  // Sets the variable to the value (and creates it if it does not exist).
  // Returns the 'id' of the variable.
  unsigned cmDspStoreSetValueViaSym( cmDspStoreH_t h, unsigned symId, const cmDspValue_t* val );

  //)
  
#ifdef __cplusplus
  }
#endif

#endif
