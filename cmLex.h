#ifndef cmLex_h
#define cmLex_h

//{
//(
//)

//(


// Predefined Lexer Id's
enum
{
  kErrorLexTId,    // 0  the lexer was unable to identify the current token
  kEofLexTId,      // 1  the lexer reached the end of input
  kSpaceLexTId,    // 2  white space
  kRealLexTId,     // 3  real number (contains a decimal point or is in scientific notation) 
  kIntLexTId,      // 4  decimal integer
  kHexLexTId,      // 5  hexidecimal integer
  kIdentLexTId,    // 6  identifier
  kQStrLexTId,     // 7  quoted string
  kBlockCmtLexTId, // 8  block comment
  kLineCmtLexTId,  // 9  line comment
  kUserLexTId      // 10 user registered token (See cmLexRegisterToken().)
};

// Lexer control flags used with cmLexInit().
enum
{
  kReturnSpaceLexFl    = 0x01, //< Return space tokens
  kReturnCommentsLexFl = 0x02  //< Return comment tokens
};

// cmLex result codes.
enum
{
  kOkLexRC = cmOkRC,       //< 0  No error. The operation completed successfully      
  kDuplicateTokenLexRC,    //< 1  The text or id passed as a user token is already in use by another token
  kMissingCmtEndLexRC,     //< 2  The end of a block comment could not be found. 
  kMissingEndQuoteLexRC,   //< 3  The end of a quoted string could not be found.
  kNoMatchLexRC,           //< 4  The lexer encountered a string which could not be classified.
  kFileOpenErrLexRC,       //< 5  File open failed on cmLexSetFile()
  kFileSeekErrLexRC,       //< 6  File seek failed on cmLexSetFile()
  kFileTellErrLexRC,       //< 7  File tell failed on cmLexSetFile()
  kFileReadErrLexRC,       //< 8  File read failed on cmLexSetFile()
  kFileCloseErrLexRC,      //< 9  File close failed on cmLexSetFile()
  kMemAllocErrLexRC,       //< 10  An attempted memory allocation failed
  kEofRC,                  //< 11 The end of the input text was encountered (this is a normal condition not an error)
  kInvalidLexRC            //< 12 Sentinal value.

};

typedef cmHandle_t cmLexH;

extern cmLexH cmLexNullH;


// Iniitalize the lexer and receive a lexer handle in return.
// Set cp to NULL if the buffer will be later via cmLexSetTextBuffer();
// See the kXXXLexFl enum's above for possible flag values.
cmLexH             cmLexInit( const cmChar_t* cp, unsigned cn, unsigned flags, cmRpt_t* rpt );

// Finalize a lexer created by an earlier call to cmLexInit()
cmRC_t             cmLexFinal( cmLexH* hp );

// Rewind the lexer to the begining of the buffer (the same as post initialize state)
cmRC_t             cmLexReset( cmLexH h );

// Verify that a lexer handle is valid
bool               cmLexIsValid( cmLexH h );

// Set a new text buffer and reset the lexer to the post initialize state.
cmRC_t             cmLexSetTextBuffer( cmLexH h, const cmChar_t* cp, unsigned cn );
cmRC_t             cmLexSetFile( cmLexH h, const cmChar_t* fn );

// Register a user defined token. The id of the first user defined token should be
// kUserLexTId+1.  Neither the id or token text can be used by a previously registered
// or built-in token. 
cmRC_t             cmLexRegisterToken( cmLexH h, unsigned id, const cmChar_t* token );

// Register a user defined token recognition function.  This function should return the count
// of initial, consecutive, characters in 'cp' which match its token pattern.
typedef unsigned (*cmLexUserMatcherPtr_t)( const cmChar_t* cp, unsigned cn );

cmRC_t             cmLexRegisterMatcher( cmLexH h, unsigned id, cmLexUserMatcherPtr_t funcPtr );

// Return the type id of the current token and advances to the next token
unsigned           cmLexGetNextToken( cmLexH h );

// Return the type id associated with the current token. This is the same value
// returned by the previous call to cmLexGetNextToken().
unsigned           cmLexTokenId( cmLexH h ); 

// Return a pointer to the first character of text associated with the 
// current token. The returned pointer directly references the text contained
// in the buffer given to the lexer in the call to cmLexInit().  The string
// is therefore not zero terminated. Use cmLexTokenCharCount() to get the 
// length of the token string.
const cmChar_t* cmLexTokenText( cmLexH h );

// Return the count of characters in the text associated with the current token.
// This is the only way to get this count since the string returned by 
// cmLexTokenText() is not zero terminated.
unsigned           cmLexTokenCharCount(  cmLexH h );

// Return the value of the current token as an integer.
int                cmLexTokenInt( cmLexH h );

// Return the value of the current token as an integer.
unsigned           cmLexTokenUInt( cmLexH h );

// Return the value of the current token as an integer.
float              cmLexTokenFloat( cmLexH h );

// Return the value of the current token as a double.
double             cmLexTokenDouble( cmLexH h );

// Return the line number associated with the current token 
unsigned           cmLexCurrentLineNumber( cmLexH h );

// Return the starting column of the current token
unsigned           cmLexCurrentColumnNumber( cmLexH h ); 

// Return the RC code associated with the last error
unsigned           cmLexErrorRC( cmLexH h );

// Return the label associated with a token id
const cmChar_t* cmLexIdToLabel( cmLexH h, unsigned typeId );

// Return the text message associated with a return code. 
const cmChar_t* cmLexRcToMsg( unsigned rc );

// Lexer testing stub.
void cmLexTest( cmRpt_t* rpt );

//)
//}

#endif
