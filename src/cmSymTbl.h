//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#ifndef cmSymTbl_h
#define cmSymTbl_h

#ifdef __cplusplus
extern "C" {
#endif
  //( { file_desc:"Symbol table object." kw:[base] }
  
  typedef cmHandle_t cmSymTblH_t;

  extern cmSymTblH_t cmSymTblNullHandle;

  // Create a new symbol table. 
  // 'parentH' is a parent table embedded in this table and is optional. Set this
  // value to 'cmSymTblNullHandle'. if the table does not have a parent.
  //
  // All symbol searches will include this table however all symbols registered with 
  // this table will be inserted in this table not the parent. 
  //
  // 'baseSymId' is the minimum symbol id used by this table and will be returned
  // as the value of the first symbol registered with this table. Subsequent symbols
  // will increment this value. Internal assertions prevent the symbol id range
  // of this table from overlapping with its parent. 
  cmSymTblH_t cmSymTblCreate(  cmSymTblH_t parentH, unsigned baseSymId, cmCtx_t* ctx );

  // Destroy and release the resources associated with a symbol table created by
  // an earlier call to cmSymTblCreate().
  void        cmSymTblDestroy(  cmSymTblH_t* hp );

  // Register a symbol label. Set 'staticFl' to true if the label is allocated statically.
  unsigned    cmSymTblRegister( cmSymTblH_t h, const char* label, bool staticFl );
  unsigned    cmSymTblRegisterSymbol( cmSymTblH_t h, const char* label );
  unsigned    cmSymTblRegisterStaticSymbol( cmSymTblH_t h, const char* label );

  unsigned    cmSymTblRegisterVFmt( cmSymTblH_t h, const cmChar_t* fmt, va_list vl );
  unsigned    cmSymTblRegisterFmt( cmSymTblH_t h, const cmChar_t* fmt, ... );

  bool        cmSymTblRemove(    cmSymTblH_t h, unsigned symId );

  // Given a symbol id return the associated label.
  const char* cmSymTblLabel(     cmSymTblH_t h, unsigned    symId );

  // Given a symbol label return the associated id or cmInvalidId if the symbol could not be found.
  unsigned    cmSymTblId(        cmSymTblH_t h, const char* label );

  // Returns true if the symbol table handle is not valid otherwise returns false.
  bool        cmSymTblIsValid(   cmSymTblH_t h );

  // Returns true if 'symId' is stored in this symbol table or its parent otherwise returns false.
  bool        cmSymTblIsValidId( cmSymTblH_t h, unsigned symId );

  // Print thes symbol table (but not its parent).
  void        cmSymTblReport(    cmSymTblH_t h );

  // Symbol table test stub.
  void        cmSymTblTest(cmCtx_t* ctx);
  
  //)

#ifdef __cplusplus
}
#endif


#endif
