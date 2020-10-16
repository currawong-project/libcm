//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#ifndef cmProcTest_h
#define cmProcTest_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc: "Some obsolete test stub functions. See the cmtools project for a complete set of test and example functions." kw:[proc]}
  
  void cmProcTestNoInit( cmCtx_t* ctx );
  void cmProcTestGnuPlot( cmCtx_t* ctx );
  void cmProcTest( cmCtx_t* ctx );

  //)
  
#ifdef __cplusplus
}
#endif


#endif
