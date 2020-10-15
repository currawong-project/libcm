#ifndef cmDspBuiltIn_h
#define cmDspBuiltIn_h


#ifdef __cplusplus
extern "C" {
#endif

  
  //( { file_desc:"Dataflow built-in process interface." kw:[snap] }

  // Returns NULL if index is outside of valid range.
  cmDspClassConsFunc_t cmDspClassGetBuiltIn( unsigned index );

  //)


#ifdef __cplusplus
}
#endif

#endif
