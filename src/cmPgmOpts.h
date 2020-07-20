#ifndef cmPgmOpts_h
#define cmPgmOpts_h

//( { file_desc:"Command line argument description and parsing API." kw:[base]}
//
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
    kReqPoFl   = 0x01,  // this is a required parameter
    kHexPoFl   = 0x02   // this integer must be given in hexidecimal or output an integer in hex.
  };


  // Define a parameter.
  //
  // unsigned        numId,   - Numeric id used to identify this parameter. The min. numId should be kPoBaseId.
  // const cmChar_t  charId,  - A character used to identify this parameter.
  // const cmChar_t* wordId,  - A label used to identify this parameter.
  // unsigned        flags,   - kNoPoFlags | kReqPoFl (the type flags are automatically assigned)
  // unsigned        enumId,  - non-zero value used to group enumerated parameter values  (ignored for non-enum types)
  //                 dfltVal  - The default value for this parameter.
  //                 retValPtr- Optional pointer to a variable to receive the argument value for this parameter.
  // unsigned        cnt,     - count of times this parameter may repeated or 0 for an unlimited repetitions
  // const cmChar_t* helpStr  - a textual description of this parameter
  //
  // Notes
  // 1) 'numId','charId', and 'wordId' must all be unique among all parameter definitions.  
  //    An error will be generated if they are not.
  // 2) For all parameter value types except the string type arguments are automatically parsed to the
  //    defined type. To avoid automatic parsing simply define the type as a string (using cmPgmOptInstallStr()).
  // 3) All expected parameters must be defined prior to calling cmPgmOptParse().
  // 4) One call to cmPgmOptInstallEnum() is made for each possible enumeration value - where the 'enumId' gives the value.
  //    A given set of associated enum values is grouped by giving a common 'numId'. 
  //    Include a master help string in one of the enumerated elements to give documentation 
  //    text for the entire set of values.
  //    Example:
  //     cmPgmOptInstallEnum(h,colorId,...,redId,...,"Red","Select a color"); 
  //     cmPgmOptInstallEnum(h,colorId,...,greenId,..."Green",NULL); 
  //     cmPgmOptInstallEnum(h,colorId,...,blueId,...,"Blue",NULL);   
  //
  // 5) The following id's are used for built-in actions and are therefore restricted from use by the client:
  //    a. -h --help    Print the program usage information.
  //    b. -v --version Print the program version informatoin.
  //    c. -p --parms   Print the program parameter values.
  //
  // 6) If a retValPtr is specified then *retValPtr it is assigned 'dfltVal' as part of the
  //    call to cmPgmOptInstXXX().
  // 
  // 7) The default value of 'Flag' type parameters is always zero. 
  //    If the 'char' or 'word' id of the Flag parameter appears in the
  //    actual argument list then the value of the argument is 'onValue'.
  //    Unlike other parameters Flag parameters do not initialize *regValPtr. 
  //    If the retValPtr is given and the flag is set in the arg. list then
  //    the retValPtr is set by bitwise assignment (i.e. *retValPtr |= dfltFlagValue).
  //    This allows multiple Flag parameters to use the same retValPtr and
  //    set independent bit fields in it.
  cmPoRC_t cmPgmOptInstallChar(cmPgmOptH_t h, unsigned numId, cmChar_t charId, const cmChar_t* worldId, unsigned flags, cmChar_t        dfltVal, cmChar_t*        retValPtr, unsigned cnt, const cmChar_t* helpStr );
  cmPoRC_t cmPgmOptInstallBool(cmPgmOptH_t h, unsigned numId, cmChar_t charId, const cmChar_t* worldId, unsigned flags, bool            dfltVal, bool*            retValPtr, unsigned cnt, const cmChar_t* helpStr );
  cmPoRC_t cmPgmOptInstallInt( cmPgmOptH_t h, unsigned numId, cmChar_t charId, const cmChar_t* worldId, unsigned flags, int             dfltVal, int*             retValPtr, unsigned cnt, const cmChar_t* helpStr );
  cmPoRC_t cmPgmOptInstallUInt(cmPgmOptH_t h, unsigned numId, cmChar_t charId, const cmChar_t* worldId, unsigned flags, unsigned        dfltVal, unsigned*        retValPtr, unsigned cnt, const cmChar_t* helpStr );
  cmPoRC_t cmPgmOptInstallDbl( cmPgmOptH_t h, unsigned numId, cmChar_t charId, const cmChar_t* worldId, unsigned flags, double          dfltVal, double*          retValPtr, unsigned cnt, const cmChar_t* helpStr );
  cmPoRC_t cmPgmOptInstallStr( cmPgmOptH_t h, unsigned numId, cmChar_t charId, const cmChar_t* worldId, unsigned flags, const cmChar_t* dfltVal, const cmChar_t** retValPtr, unsigned cnt, const cmChar_t* helpStr );
  cmPoRC_t cmPgmOptInstallEnum(cmPgmOptH_t h, unsigned numId, cmChar_t charId, const cmChar_t* worldId, unsigned flags, unsigned enumId, unsigned dfltVal, unsigned* retValPtr, unsigned cnt, const cmChar_t* helpStr, const cmChar_t* mstrHelpStr  );
  cmPoRC_t cmPgmOptInstallFlag(cmPgmOptH_t h, unsigned numId, cmChar_t charId, const cmChar_t* worldId, unsigned flags, unsigned         onValue, unsigned*       retValPtr, unsigned cnt, const cmChar_t* helpStr );

  // Parse a set of command line arguments.
  //
  // 1) If only built-in parameters were specified then the NO check is done 
  //    to verify that required arguments were provided.  However, if any non-built-in
  //    arguments are provided then a check is performed to be sure that any 
  //    parameters specified with the kPoReqFl have associated argument values.
  //
  // 2) If a parameter was specified with a 'retValPtr' then *retValPtr is
  //    set to the value of the last argument associated with the given parameter.
  //    This means that 'retValPtr' is generally only useful when the
  //    parameter instance count limit (the 'cnt' param to cmPgmOptInstallXXX())
  //    was set to 1.
  //
  //    
  cmPoRC_t cmPgmOptParse( cmPgmOptH_t h, unsigned argCnt, char* argArray[] );
  
  // Get the total count of arguments passed to cmPgmOptParse().
  unsigned cmPgmOptArgCount( cmPgmOptH_t h);
  
  // Get the numeric id associated with each argument.
  unsigned cmPgmOptNumId( cmPgmOptH_t h, unsigned argIdx );

  // Get the character id associated with this argument.
  unsigned cmPgmOptCharId( cmPgmOptH_t h, unsigned argIdx );

  // Get the word id associated with this argument.
  const cmChar_t* cmPgmOptWordId( cmPgmOptH_t h, unsigned argIdx );

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

  // Returns 'false' if only built-in options were selected otherwise returns true.
  bool cmPgmOptHandleBuiltInActions( cmPgmOptH_t h, cmRpt_t* rpt );

  void cmPgmOptPrintHelp(    cmPgmOptH_t h, cmRpt_t* rpt );
  void cmPgmOptPrintVersion( cmPgmOptH_t h, cmRpt_t* rpt );
  void cmPgmOptPrintParms(   cmPgmOptH_t h, cmRpt_t* rpt );

  //)

#ifdef __cplusplus
}
#endif


#endif
