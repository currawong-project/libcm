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
    kDuplicateIdPrRC,
    kFileSysFailPrRC
  };

  enum
  {
    kMaxVarPrId = 0x7fffffff // User assigned variable id's given to cmPrefsCreateXXX()
                             // must be less than kMaxVarPrId
  };

  typedef void (*cmPrefsOnChangeFunc_t)( cmPrH_t h, void* cbDataPtr, unsigned id ); 

  extern cmPrH_t cmPrNullHandle;

  // cmPrefsInit() creates the preference directory if it does not exist 
  // according to cmFsPrefsDir(). It then forms the prefs file name as
  // 'cmFsPrefsDir()/fnName.fnExt' and call cmPrefsInitialize().
  // Set 'fnName' to NULL to use cmFsAppName() as the pref file name.
  // Set 'fnExt' to NULL to use '.js' as the pref file extension.
  // 'cbFunc'  and 'cbDataPtr' are optional in both versions.
  cmPrRC_t cmPrefsInit(   cmCtx_t* ctx, cmPrH_t* hp, const cmChar_t* fnName, const cmChar_t* fnExt, cmPrefsOnChangeFunc_t cbFunc, void* cbDataPtr );
  cmPrRC_t cmPrefsInitialize( cmCtx_t* ctx, cmPrH_t* hp, const cmChar_t* fn, cmPrefsOnChangeFunc_t cbFunc, void* cbDataPtr);
  cmPrRC_t cmPrefsFinalize(   cmPrH_t* hp );

  bool cmPrefsIsValid( cmPrH_t h );

  const cmChar_t* cmPrefsFileName( cmPrH_t h );

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
  // Set *eleCntPtr to 1 for scalar a values.
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
  cmPrRC_t        cmPrefsBoolRc(   cmPrH_t h, const cmChar_t* pathStr, bool*            retValPtr );
  cmPrRC_t        cmPrefsUIntRc(   cmPrH_t h, const cmChar_t* pathStr, unsigned*        retValPtr );
  cmPrRC_t        cmPrefsIntRc(    cmPrH_t h, const cmChar_t* pathStr, int*             retValPtr );
  cmPrRC_t        cmPrefsFloatRc(  cmPrH_t h, const cmChar_t* pathStr, float*           retValPtr );
  cmPrRC_t        cmPrefsRealRc(   cmPrH_t h, const cmChar_t* pathStr, double*          retValPtr );
  cmPrRC_t        cmPrefsStringRc( cmPrH_t h, const cmChar_t* pathStr, const cmChar_t** retValPtr ); 


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

  cmPrRC_t cmPrefsSetBoolArray(   cmPrH_t h, unsigned id, const bool*      vp, const unsigned* eleCntPtr );
  cmPrRC_t cmPrefsSetIntArray(    cmPrH_t h, unsigned id, const int*       vp, const unsigned* eleCntPtr );
  cmPrRC_t cmPrefsSetRealArray(   cmPrH_t h, unsigned id, const double*    vp, const unsigned* eleCntPtr );
  cmPrRC_t cmPrefsSetStringArray( cmPrH_t h, unsigned id, const cmChar_t** vp, const unsigned* eleCntPtr );

  cmPrRC_t cmPrefsSetBool(   cmPrH_t h, unsigned id, bool     val );
  cmPrRC_t cmPrefsSetUInt(   cmPrH_t h, unsigned id, unsigned val );
  cmPrRC_t cmPrefsSetInt(    cmPrH_t h, unsigned id, int      val );
  cmPrRC_t cmPrefsSetFloat(  cmPrH_t h, unsigned id, float    val );
  cmPrRC_t cmPrefsSetReal(   cmPrH_t h, unsigned id, double   val );
  cmPrRC_t cmPrefsSetString( cmPrH_t h, unsigned id, const cmChar_t* val );

  cmPrRC_t cmPrefsPathSetBool(   cmPrH_t h, const cmChar_t* pathStr, bool     val );
  cmPrRC_t cmPrefsPathSetUInt(   cmPrH_t h, const cmChar_t* pathStr, unsigned val );
  cmPrRC_t cmPrefsPathSetInt(    cmPrH_t h, const cmChar_t* pathStr, int      val );
  cmPrRC_t cmPrefsPathSetFloat(  cmPrH_t h, const cmChar_t* pathStr, float    val );
  cmPrRC_t cmPrefsPathSetReal(   cmPrH_t h, const cmChar_t* pathStr, double   val );
  cmPrRC_t cmPrefsPathSetString( cmPrH_t h, const cmChar_t* pathStr, const cmChar_t* val );

  // Create a new preference variable and set it's value to 'val'.
  // If a variable with the same path and type already exists and kForceValuePrFl
  // is set then update it's value to 'val'. Note that in this case if
  //  kForceValuePrFl is not set then the function returns quietly.
  //
  // If a variable with the same path but a different type exists then an error is returned.
  // 
  // The 'id' argument is optional.  If 'id' is set to cmInvalidId then the
  // variable will be automatically assigned an id.  The value of the
  // automatically assigned id can be found from the path string
  // via cmPrefsId().  If 'id' is not set to cmInvalidId then it must be less than 
  // kMaxVarId.

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

  // If 'fn' is NULL then the filename passed in cmPrefsInitialize() is used.
  cmPrRC_t cmPrefsWrite( cmPrH_t h, const cmChar_t* fn );

  void cmPrefsReport( cmPrH_t h );


  void cmPrefsTest( cmCtx_t* ctx, const char* ifn, const char* ofn );
    

#ifdef __cplusplus
}
#endif

#endif
