#ifndef cmPrefs_h
#define cmPrefs_h

#ifdef __cplusplus
extern "C" {
#endif

  typedef unsigned cmPrRC_t;
  typedef cmHandle_t cmPrH_t;

  enum
  {
    kOkPrRC = cmOkRC,
    kJsonFailPrRC,
    kLHeapFailPrRC,
    kCbNotFoundPrRC,
    kVarNotFoundPrRC,
    kBufTooSmallPrRC,
    kCvtErrPrRC,
    kInvalidIdPrRC,
    kInvalidIndexPrRC,
    kWriteFileFailPrRC,
    kNodeCreateFailPrRC,
    kDuplicateIdPrRC
  };

  typedef void (*cmPrefsOnChangeFunc_t)( cmPrH_t h, void* cbDataPtr, unsigned id ); 

  extern cmPrH_t cmPrNullHandle;

  // 'cbFunc' is optional
  cmPrRC_t cmPrefsInitialize( cmPrH_t* hp, const cmChar_t* fn, cmPrefsOnChangeFunc_t cbFunc, void* cbDataPtr, cmCtx_t* ctx );
  cmPrRC_t cmPrefsFinalize(   cmPrH_t* hp );

  bool cmPrefsIsValid( cmPrH_t h );

  // Return last RC.
  cmPrRC_t cmPrefsRC( cmPrH_t h);

  // Return last RC and set new RC.
  cmPrRC_t cmPrefsSetRC( cmPrH_t h, cmPrRC_t rc );

  cmPrRC_t cmPrefsInstallCallback( cmPrH_t h, cmPrefsOnChangeFunc_t cbFunc, void* cbDataPtr );
  cmPrRC_t cmPrefsRemoveCallback(  cmPrH_t h, cmPrefsOnChangeFunc_t cbFunc );

  // Return cmInvalidId if the no variable is found with the requested path.
  unsigned cmPrefsId( cmPrH_t h, const cmChar_t* pathStr, bool reportNotFoundFl );

  // Returns -1 (and generates an error msg) if no pref. variable
  // is associated with id.  Returns 1 if the variable is a scalar.
  unsigned cmPrefsEleCount( cmPrH_t h,  unsigned id );

  // On input *'eleCntPtr' must contain the number of elements in the buffer pointed to by 'vp'.
  // On return *'eleCntPtr' contains the actuall number of elements returned by the function.
  cmPrRC_t cmPrefsGetBool(   cmPrH_t h, unsigned id, bool*            vp, unsigned* eleCntPtr );
  cmPrRC_t cmPrefsGetInt(    cmPrH_t h, unsigned id, int*             vp, unsigned* eleCntPtr );
  cmPrRC_t cmPrefsGetReal(   cmPrH_t h, unsigned id, double*          vp, unsigned* eleCntPtr );
  cmPrRC_t cmPrefsGetString( cmPrH_t h, unsigned id, const cmChar_t** vp, unsigned* eleCntPtr );

  // Simplified scalar interface - check cmPrefsRC() for errors.
  bool            cmPrefsBool(   cmPrH_t h, unsigned id );
  unsigned        cmPrefsUInt(   cmPrH_t h, unsigned id );
  int             cmPrefsInt(    cmPrH_t h, unsigned id );
  float           cmPrefsFloat(  cmPrH_t h, unsigned id );
  double          cmPrefsReal(   cmPrH_t h, unsigned id );
  const cmChar_t* cmPrefsString( cmPrH_t h, unsigned id ); 

  // Simplified scalar interface w/ default values - check cmPrefsRC() for errors.
  // Returns the stored preference variable unless 'pathStr' does not identify a variable
  // or if 'h' is not valid.  In either of these cases 'dfltVal' is returned.
  bool            cmPrefsBoolDef(   cmPrH_t h, const cmChar_t* pathStr, bool            dfltVal );
  unsigned        cmPrefsUIntDef(   cmPrH_t h, const cmChar_t* pathStr, unsigned        dfltVal );
  int             cmPrefsIntDef(    cmPrH_t h, const cmChar_t* pathStr, int             dfltVal );
  float           cmPrefsFloatDef(  cmPrH_t h, const cmChar_t* pathStr, float           dfltVal );
  double          cmPrefsRealDef(   cmPrH_t h, const cmChar_t* pathStr, double          dfltVal );
  const cmChar_t* cmPrefsStringDef( cmPrH_t h, const cmChar_t* pathStr, const cmChar_t* dfltVal ); 

  // Get a scalar value. 
  cmPrRC_t        cmPrefsScalarBool(   cmPrH_t h, const cmChar_t* pathStr, bool*            retValPtr );
  cmPrRC_t        cmPrefsScalarUInt(   cmPrH_t h, const cmChar_t* pathStr, unsigned*        retValPtr );
  cmPrRC_t        cmPrefsScalarInt(    cmPrH_t h, const cmChar_t* pathStr, int*             retValPtr );
  cmPrRC_t        cmPrefsScalarFloat(  cmPrH_t h, const cmChar_t* pathStr, float*           retValPtr );
  cmPrRC_t        cmPrefsScalarReal(   cmPrH_t h, const cmChar_t* pathStr, double*          retValPtr );
  cmPrRC_t        cmPrefsScalarString( cmPrH_t h, const cmChar_t* pathStr, const cmChar_t** retValPtr ); 


  // Simplified array interface - check cmPrefsRC() for errors

  // Returns cmInvalidCnt if 'id' is invalid or 0 if the identified var. is a scalar.
  unsigned        cmPrefsArrayElementCount( cmPrH_t h, unsigned id );

  bool            cmPrefsBoolEle(   cmPrH_t h, unsigned id, unsigned idx );
  unsigned        cmPrefsUIntEle(   cmPrH_t h, unsigned id, unsigned idx );
  int             cmPrefsIntEle(    cmPrH_t h, unsigned id, unsigned idx );
  float           cmPrefsFloatEle(  cmPrH_t h, unsigned id, unsigned idx );
  double          cmPrefsRealEle(   cmPrH_t h, unsigned id, unsigned idx );
  const cmChar_t* cmPrefsStringEle( cmPrH_t h, unsigned id, unsigned idx ); 

  // Set a preference value.
  // The size of array variables is allowed to shrink or grow but the type (bool/int/real/string)
  // must match.
  //
  // Note that the following limitations apply:
  // 1) This interface allows setting the value of an existing preference variable.
  //    New variables may not be added.
  // 2) The type (bool/int/real/string) of the variable must match the value.
  // (e.g. 'int' type variables can only be set via cmPrefsSetInt()).
  // 3) For  scalar (non-array) variables *eleCntPtr must be set to 1.
  //

  cmPrRC_t cmPrefsSetBool(   cmPrH_t h, unsigned id, const bool*      vp, const unsigned* eleCntPtr );
  cmPrRC_t cmPrefsSetInt(    cmPrH_t h, unsigned id, const int*       vp, const unsigned* eleCntPtr );
  cmPrRC_t cmPrefsSetReal(   cmPrH_t h, unsigned id, const double*    vp, const unsigned* eleCntPtr );
  cmPrRC_t cmPrefsSetString( cmPrH_t h, unsigned id, const cmChar_t** vp, const unsigned* eleCntPtr );

  cmPrRC_t cmPrefsSetScalarBool(   cmPrH_t h, const cmChar_t* pathStr, bool     val );
  cmPrRC_t cmPrefsSetScalarUInt(   cmPrH_t h, const cmChar_t* pathStr, unsigned val );
  cmPrRC_t cmPrefsSetScalarInt(    cmPrH_t h, const cmChar_t* pathStr, int      val );
  cmPrRC_t cmPrefsSetScalarFloat(  cmPrH_t h, const cmChar_t* pathStr, float    val );
  cmPrRC_t cmPrefsSetScalarReal(   cmPrH_t h, const cmChar_t* pathStr, double   val );
  cmPrRC_t cmPrefsSetScalarString( cmPrH_t h, const cmChar_t* pathStr, const cmChar_t* val );

  // Create a new preference variable and set it's value to 'val'.
  // If a variable with the same path and type already exists and kForceValuePrFl is set then update it's value to 'val'.
  // Note that in this case if kForceValuePrFl is not set then the function returns quietly.
  //
  // If a variable with the same path but a different type exists then an error is returned.
  // 

  // Set kForceValuePrFl 
  enum { kForceValuePrFl=0x01 };
  cmPrRC_t cmPrefsCreateBool(   cmPrH_t h, unsigned id, const cmChar_t* pathStr, unsigned flags, bool     val );
  cmPrRC_t cmPrefsCreateUInt(   cmPrH_t h, unsigned id, const cmChar_t* pathStr, unsigned flags, unsigned val );
  cmPrRC_t cmPrefsCreateInt(    cmPrH_t h, unsigned id, const cmChar_t* pathStr, unsigned flags, int      val );
  cmPrRC_t cmPrefsCreateFloat(  cmPrH_t h, unsigned id, const cmChar_t* pathStr, unsigned flags, float    val );
  cmPrRC_t cmPrefsCreateReal(   cmPrH_t h, unsigned id, const cmChar_t* pathStr, unsigned flags, double   val );
  cmPrRC_t cmPrefsCreateString( cmPrH_t h, unsigned id, const cmChar_t* pathStr, unsigned flags, const cmChar_t* val );

  cmPrRC_t cmPrefsCreateBoolArray(   cmPrH_t h, unsigned id, const cmChar_t* pathStr, unsigned flags, const bool*     val, unsigned eleCnt );
  cmPrRC_t cmPrefsCreateUIntArray(   cmPrH_t h, unsigned id, const cmChar_t* pathStr, unsigned flags, const unsigned* val, unsigned eleCnt );
  cmPrRC_t cmPrefsCreateIntArray(    cmPrH_t h, unsigned id, const cmChar_t* pathStr, unsigned flags, const int*      val, unsigned eleCnt );
  cmPrRC_t cmPrefsCreateFloatArray(  cmPrH_t h, unsigned id, const cmChar_t* pathStr, unsigned flags, const float*    val, unsigned eleCnt );
  cmPrRC_t cmPrefsCreateRealArray(   cmPrH_t h, unsigned id, const cmChar_t* pathStr, unsigned flags, const double*   val, unsigned eleCnt );
  cmPrRC_t cmPrefsCreateStringArray( cmPrH_t h, unsigned id, const cmChar_t* pathStr, unsigned flags, const cmChar_t** val, unsigned eleCnt );

  bool     cmPrefsIsDirty( cmPrH_t h );
  cmPrRC_t cmPrefsWrite( cmPrH_t h, const cmChar_t* fn );


  void cmPrefsTest( cmCtx_t* ctx, const char* ifn, const char* ofn );
    

#ifdef __cplusplus
}
#endif

#endif
