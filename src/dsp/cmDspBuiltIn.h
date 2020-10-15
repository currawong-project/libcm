//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
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
