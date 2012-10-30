#ifndef cmPgmOpts_h
#define cmPgmOpts_h

//{
//(
// Command line program option syntax:
//
//
// '-'<charId>*  <value>+  - a dash followed by one or more one character id's optionally followed by a parameter value.
// '--'<wordId> <value>+   - a double dash followed by a word id optionally followed by a parameter value.
// 
//  A char id is a single character.
//  A word id is a string of characters with no intervening white space.
//
// Notes:
// 1) There is no way to give multiple <values> without an intervening character or word id.
//    A <value>  must therefore always be immediately preceded by an id.
// 2) There must never be a space between the dash(es) and the characters forming the identifier.
// 3) There must always be a space between the identifier and any subsequent <value>.
// 4) See src/mas/src/main.c for a complete example.
//
// Terms:
// Parameter - Description of the allowable types and constraints for a program option.
// Argument  - An instance of a parameter or the values associated with a parameter.
// 
//)

#ifdef __cplusplus
extern "C" {
#endif

  //(

  // cmPgmOpts Result Codes
  enum
  {
    kOkPoRC = cmOkRC,
    kSyntaxErrPoRC,
    kInvalidIdxPoRC,
    kParseFailPoRC,
    kNoReqArgPoRC,
    kDuplicateIdPoRC,
    kArgCntErrPoRC,
    kParmNotFoundPoRC,
    kInstNotFoundPoRC,
    kTypeErrPoRC
  };

  // cmPgmOpts parameter id's
  enum
  {
    kPrintHelpPoId,
    kVersionPoId,
    kPrintParmsPoId,
    kNoExecPoId,
    kBasePoId
  };

  typedef cmRC_t     cmPoRC_t;
  typedef cmHandle_t cmPgmOptH_t;
  
  extern cmPgmOptH_t cmPgmOptNullHandle;

  // Initialize a program options parser.
  cmPoRC_t cmPgmOptInitialize(cmCtx_t* c, cmPgmOptH_t* hp, const cmChar_t* helpBegStr, const cmChar_t* helpEndStr );

  // Finalize a program options parser.
  cmPoRC_t cmPgmOptFinalize( cmPgmOptH_t* hp );

  // Return true if the program options parser was successfully initialized.
  bool     cmPgmOptIsValid( cmPgmOptH_t h );

  // Flag used by the 'flags' arg. to cmPgmOptInstall().
  enum { 
    kNoPoFlags = 0x000, 
    kReqPoFl   = 0x001,  // this is a required parameter
    kBoolPoFl  = 0x002,  // this parameter takes a value
    kCharPoFl  = 0x004,  // parm. value is a character
    kIntPoFl   = 0x008,  // parm. value is a decimal int
    kUIntPoFl  = 0x010,  // parm. value is a decimal unsigned int
    kHexPoFl   = 0x020,  // parm. value is a hex. unsigned int 
    kDblPoFl   = 0x040,  // parm. value is a decimal double
    kStrPoFl   = 0x080,  // parm. value is a string (default)
    kEnumPoFl  = 0x100,  // parm. valus is a enum type (automatically set by a non-zero enumId)

    kTypeMaskPoFl = 0x1f6
  };


  // Define a parameter.
  //
  // unsigned        numId,   - numeric id used to identify this parameter 
  // const cmChar_t  charId,  - a character used to identify this parameter
  // const cmChar_t* wordId,  - a label used to identify this parameter
  // unsigned        flags,   - kNoPoFlags | kReqPoFl (the type flags are automatically assigned)
  // unsigned        enumId,  - non-zero value used to group enumerated parameter values  (ignored for non-enum types)
  // unsigned        cnt,     - count of times this parameter may repeated or 0 for an unlimited repetitions
  // const cmChar_t* helpStr  - a textual description of this parameter
  //
  // Notes
  // 1) 'numId','charId', and 'wordId' must all be unique among all parameter definitions.
  // 2) If no  parameter value type flag is given then the type is assumed to be of type bool.
  // 3) For all parameter value types except the string type arguments are automatically parsed to the
  //    defined type. To avoid automatic parsing simply define the type as a string (using cmPgmOptInstallStr()).
  // 4) All expected parameters must be defined prior to calling cmPgmOptParse().
  // 5) One call to cmPgmOPtInstallEnum() is made for each possible enumeration value - where the 'enumId' gives the value.
  //    A given set of associated enum values is grouped by giving a common 'numId'.
  //    Example:
  //     cmPgmOptInstallEnum(h,colorId,...,redId,...); 
  //     cmPgmOptInstallEnum(h,colorId,...,greenId,...); 
  //     cmPgmOptInstallEnum(h,colorId,...,blueId,...);   
  //
  // 6) The following id's are used for built-in actions and are therefore restricted from use by the client:
  //    a. -h --help    Print the program usage information.
  //    b. -v --version Print the program version informatoin.
  //    c. -p --parms   Print the program parameter values.
  //
  cmPoRC_t cmPgmOptInstallChar(cmPgmOptH_t h, unsigned numId, cmChar_t charId, const cmChar_t* worldId, unsigned flags, cmChar_t        dfltVal, cmChar_t*        retValPtr, unsigned cnt, const cmChar_t* helpStr );
  cmPoRC_t cmPgmOptInstallBool(cmPgmOptH_t h, unsigned numId, cmChar_t charId, const cmChar_t* worldId, unsigned flags, bool            dfltVal, bool*            retValPtr, unsigned cnt, const cmChar_t* helpStr );
  cmPoRC_t cmPgmOptInstallInt( cmPgmOptH_t h, unsigned numId, cmChar_t charId, const cmChar_t* worldId, unsigned flags, int             dfltVal, int*             retValPtr, unsigned cnt, const cmChar_t* helpStr );
  cmPoRC_t cmPgmOptInstallUInt(cmPgmOptH_t h, unsigned numId, cmChar_t charId, const cmChar_t* worldId, unsigned flags, unsigned        dfltVal, unsigned*        retValPtr, unsigned cnt, const cmChar_t* helpStr );
  cmPoRC_t cmPgmOptInstallDbl( cmPgmOptH_t h, unsigned numId, cmChar_t charId, const cmChar_t* worldId, unsigned flags, double          dfltVal, double*          retValPtr, unsigned cnt, const cmChar_t* helpStr );
  cmPoRC_t cmPgmOptInstallStr( cmPgmOptH_t h, unsigned numId, cmChar_t charId, const cmChar_t* worldId, unsigned flags, const cmChar_t* dfltVal, const cmChar_t** retValPtr, unsigned cnt, const cmChar_t* helpStr );
  cmPoRC_t cmPgmOptInstallEnum(cmPgmOptH_t h, unsigned numId, cmChar_t charId, const cmChar_t* worldId, unsigned flags, unsigned enumId, unsigned dfltVal, unsigned* retValPtr, unsigned cnt, const cmChar_t* helpStr  );

  // Parse a set of command line arguments.
  cmPoRC_t cmPgmOptParse( cmPgmOptH_t h, unsigned argCnt, char* argArray[] );
  
  // Get the total count of arguments passed to cmPgmOptParse().
  unsigned cmPgmOptArgCount( cmPgmOptH_t h);
  
  // Get the numeric id associated with each argument.
  unsigned cmPgmOptNumId( cmPgmOptH_t h, unsigned argIdx );

  // Manually convert each argument string into the specified type.
  // These functions are useful if all of the parameters were defined using cmPgmOptInstallStr().
  // Use cmPgmOptRC() to check for errors.
  char        cmPgmOptParseArgChar(cmPgmOptH_t h, unsigned argIdx );
  bool        cmPgmOptParseArgBool(cmPgmOptH_t h, unsigned argIdx );
  int         cmPgmOptParseArgInt( cmPgmOptH_t h, unsigned argIdx );
  unsigned    cmPgmOptParseArgUInt(cmPgmOptH_t h, unsigned argIdx );
  double      cmPgmOptParseArgDbl( cmPgmOptH_t h, unsigned argIdx );
  const char* cmPgmOptParseArgStr( cmPgmOptH_t h, unsigned argIdx );

  // Get the count of arg's for a given parameter.
  unsigned    cmPgmOptParmArgCount( cmPgmOptH_t h, unsigned numId );

  // Get the value associated with each parsed argument.
  // If no argument was given for the requested parameter 
  // (cmPgmOptParmArgCount(numId)==0) and 'instIdx' == 0 then the default value is returned. 
  // Use cmPgOptRC() to check for errors.
  //
  // The parameter identified by numId must has been defined by an earlier call to
  // cmPgmOptInstallChar() or this function
  char        cmPgmOptArgChar(   cmPgmOptH_t h, unsigned numId, unsigned instIdx );

  // No matter the type of the parameter it will be converted to a bool.
  bool        cmPgmOptArgBool(   cmPgmOptH_t h, unsigned numId, unsigned instIdx );

  // All types, except strings, are converted to type int. Doubles are rounded.
  int         cmPgmOptArgInt(    cmPgmOptH_t h, unsigned numId, unsigned instIdx );

  // All types, except strings, are converted to type unsigned. Doubles are rounded.
  unsigned    cmPgmOptArgUInt(   cmPgmOptH_t h, unsigned numId, unsigned instIdx );

  // All types except strings, are converted to double.
  double      cmPgmOptArgDbl(    cmPgmOptH_t h, unsigned numId, unsigned instIdx );

  // If the parameter is not defined as a string then the arg. string value us returned.
  const char* cmPgmOptArgStr(    cmPgmOptH_t h, unsigned numId, unsigned instIdx );
  
  
  // Get and set the current result code.
  cmPoRC_t    cmPgmOptRC( cmPgmOptH_t h, cmPoRC_t rc );

  // Returns 'true' only if non- built-in options were selected
  bool cmPgmOptHandleBuiltInActions( cmPgmOptH_t h, cmRpt_t* rpt );

  void cmPgmOptPrintHelp(    cmPgmOptH_t h, cmRpt_t* rpt );
  void cmPgmOptPrintVersion( cmPgmOptH_t h, cmRpt_t* rpt );
  void cmPgmOptPrintParms(   cmPgmOptH_t h, cmRpt_t* rpt );

  //)
  //}

#ifdef __cplusplus
}
#endif


#endif
