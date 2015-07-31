#ifndef cmText_h
#define cmText_h

#ifdef __cplusplus
extern "C" {
#endif

  //{
  //(
  // The cmText API supports two basic tasks: the generation of
  // formatted text into dynamic arrays and text to number parsing.
  // 
  //)

  //(
  enum
  {
    kOkTxRC = 0,
    kNullTxRC,
    kCvtErrTxRC,
    kLHeapFailTxRC,
    kBufTooSmallTxRC
  };

  typedef unsigned     cmTxRC_t;
  typedef cmHandle_t   cmTextSysH_t;
  
  extern cmTextSysH_t  cmTextSysNullHandle;

  cmTxRC_t cmTextSysInitialize(  cmCtx_t* ctx, cmTextSysH_t* hp );
  cmTxRC_t cmTextSysFinalize( cmTextSysH_t* hp );
  bool     cmTextSysIsValid( cmTextSysH_t h );

  // Print the string into a static, global, but resizable, buffer.
  // This string is only valid until the next call to cmTextSysPrintS().
  // This function is not currently thread safe - calls from different
  // threads can therefore overwrite one anothers buffered strings
  // unexpectedly. 
  cmChar_t*     cmTextSysVPrintfS( cmTextSysH_t h, const cmChar_t* fmt, va_list vl );
  cmChar_t*     cmTextSysPrintfS(  cmTextSysH_t h, const cmChar_t* fmt, ... );


  // Print the string into memory supplied by a linked heap.
  // Strings returned from this function may be deallocated with a call to
  // cmLhFree().
  cmChar_t*     cmTextSysVPrintfH( cmLHeapH_t h, const cmChar_t* fmt, va_list vl );
  cmChar_t*     cmTextSysPrintfH(  cmLHeapH_t h, const cmChar_t* fmt, ... );

  // Print a string and store it in the internal text system heap.  
  // The memory used by these strings will be automatically released
  // whent he text system is destroyed.  The caller may optionally release
  // a string via cmTextSysFreeStr().
  cmChar_t*     cmTextSysVPrintf( cmTextSysH_t h, const cmChar_t* fmt, va_list vl );
  cmChar_t*     cmTextSysPrintf(  cmTextSysH_t h, const cmChar_t* fmt, ... );
  void          cmTextSysFreeStr( cmTextSysH_t h, const cmChar_t* s );
  // Return true if 's' is stored in the text systems internal heap,
  bool          cmTextSysIsStored(cmTextSysH_t h, const cmChar_t* s );

  //
  // Global interface:
  //
  // These functions are equivalent to the same-named functions above
  // but use a globally allocated cmTextSysH_t handle.
  cmTxRC_t cmTsInitialize( cmCtx_t* ctx );
  cmTxRC_t cmTsFinalize();
  bool     cmTsIsValid();

  cmChar_t*     cmTsVPrintfS( const cmChar_t* fmt, va_list vl );
  cmChar_t*     cmTsPrintfS(  const cmChar_t* fmt, ... );

  cmChar_t*     cmTsVPrintfH( cmLHeapH_t h, const cmChar_t* fmt, va_list vl );
  cmChar_t*     cmTsPrintfH(  cmLHeapH_t h, const cmChar_t* fmt, ... );

  cmChar_t*     cmTsVPrintf( const cmChar_t* fmt, va_list vl );
  cmChar_t*     cmTsPrintf(  const cmChar_t* fmt, ... );
  void          cmTsFreeStr( const cmChar_t* s );
  bool          cmTsIsStored(const cmChar_t* s );

  // Print a formatted string into s[].  s[] is reallocated as necessary to
  // hold the string.  s must be freed by the caller via cmMemFree().
  cmChar_t*     cmTsVPrintfP( cmChar_t* s, const cmChar_t* fmt, va_list vl );
  cmChar_t*     cmTsPrintfP( cmChar_t* s, const cmChar_t* fmt, ... );


  // Set err to NULL to ignore errors.
  // 
  cmTxRC_t   cmTextToInt(    const char* text, int*      ip, cmErr_t* err );
  cmTxRC_t   cmTextToUInt(   const char* text, unsigned* up, cmErr_t* err );
  cmTxRC_t   cmTextToFloat(  const char* text, float*    fp, cmErr_t* err );
  cmTxRC_t   cmTextToDouble( const char* text, double*   dp, cmErr_t* err );
  cmTxRC_t   cmTextToBool(   const char* text, bool*     bp, cmErr_t* err );

  // In all cases s, s0 and s1 are expected to be non-NULL.  
  // If any of the arguments were allowed to be NULL, then the natural
  // return value would be NULL and this would lead to ambiguities 
  // with the meaning of a NULL return value.

  // Return ptr to next non-white or terminating 0 if EOS is encountered.
  cmChar_t*       cmTextNextNonWhiteOrEos( cmChar_t* s );
  const cmChar_t* cmTextNextNonWhiteOrEosC( const cmChar_t* s );

  // Return ptr to next non-white char. or NULL if EOS is encountered.
  cmChar_t*       cmTextNextNonWhite( cmChar_t* s );
  const cmChar_t* cmTextNextNonWhiteC( const cmChar_t* s );

  // Return ptr to prev non-white or s if BOS is encountered.
  // If returned ptr == s0 then BOS was encountered.
  cmChar_t*       cmTextPrevNonWhiteOrBos( cmChar_t* s0, const cmChar_t* s1 );
  const cmChar_t* cmTextPrevNonWhiteOrBosC( const cmChar_t* s0, const cmChar_t* s1 );

  // Return ptr to prev non-white char. or NULL if BOS is encountered.
  cmChar_t*       cmTextPrevNonWhite( cmChar_t* s0, const cmChar_t* s1 );
  const cmChar_t* cmTextPrevNonWhiteC( const cmChar_t* s0, const cmChar_t* s1 );


  // Return ptr to next white or terminating 0 if EOS is encountered.
  // ( *(return_ptr)==0 if EOS is encountered )
  cmChar_t*       cmTextNextWhiteOrEos( cmChar_t* s );
  const cmChar_t* cmTextNextWhiteOrEosC( const cmChar_t* s );
  
  // Return ptr to next white char. or NULL if EOS is encountered.
  cmChar_t*       cmTextNextWhite( cmChar_t* s );
  const cmChar_t* cmTextNextWhiteC( const cmChar_t* s );

  // Return ptr to prev white or s if BOS is encountered.
  // If returned ptr != s then *(returned_ptr-1) == '\n'
  cmChar_t*       cmTextPrevWhiteOrBos( cmChar_t* s0, const cmChar_t* s1 );
  const cmChar_t* cmTextPrevWhiteOrBosC( const cmChar_t* s0, const cmChar_t* s1 );


  // Return ptr to prev white char. or NULL if BOS is encountered.
  cmChar_t*       cmTextPrevWhite( cmChar_t* s0, const cmChar_t* s1 );
  const cmChar_t* cmTextPrevWhiteC( const cmChar_t* s0, const cmChar_t* s1 );


  // Backup to first char on line. 
  // Return ptr to first char on line or first char in buffer.
  // If return_ptr!=s0 then *(return_ptr-1) == '\n'.
  cmChar_t*       cmTextBegOfLine( cmChar_t* s0, const cmChar_t* s1 );
  const cmChar_t* cmTextBegOfLineC( const cmChar_t* s0, const cmChar_t* s1 );

  // Move forward to last char on line.
  // Return ptr to last char on line.
  // If *return_ptr == 0 then the end of buffer was encountered prior 
  // to the next '\n' otherwise *(return_ptr) == '\n'.
  cmChar_t*       cmTextEndOfLine( cmChar_t* s );
  const cmChar_t* cmTextEndOfLineC( const cmChar_t* s );

  // Return a pointer to the last non-white character in the string
  // or NULL if s is NULL or empty.
  cmChar_t*       cmTextLastNonWhiteChar(  const cmChar_t* s );
  const cmChar_t* cmTextLastNonWhiteCharC( const cmChar_t* s );
  
  // Return a pointer to the last white character in the string
  // or NULL if s is NULL or empty.
  cmChar_t*       cmTextLastWhiteChar(  const cmChar_t* s );
  const cmChar_t* cmTextLastWhiteCharC( const cmChar_t* s );
    


  // Shrink the text by copying all char's beginning with t+tn to t.
  // ex: 'abcd' s=a, t=b, tn=1 -> acd
  //     'abcd' s=a, t=b, tn=2 -> ad
  void      cmTextShrinkS( cmChar_t* s, const cmChar_t* t, unsigned tn );
  void      cmTextShrinkSN(cmChar_t* s, unsigned sn, const cmChar_t* t, unsigned tn );

  // Remove the last n characters from s by inserting a '\0' at s[ strlen(s)-n ].
  void      cmTextClip(    cmChar_t* s, unsigned n );

  // Trim white space from the begining/end/both of a string
  cmChar_t* cmTextTrimBegin( cmChar_t* s );
  cmChar_t* cmTextTrimEnd( cmChar_t* s );
  cmChar_t* cmTextTrim( cmChar_t* );

  // Expand s by copying all bytes past t to t+tn.
  cmChar_t* cmTextExpandS( cmChar_t* s, const cmChar_t* t, unsigned tn );

  // Replace t[tn] with u[un] in s[].
  cmChar_t* cmTextReplaceSN( cmChar_t* s, const cmChar_t* t, unsigned tn, const cmChar_t* u, unsigned un );

  // Replace t[tn] with u in s[].
  cmChar_t* cmTextReplaceS( cmChar_t* s, const cmChar_t* t, unsigned tn, const cmChar_t* u );
  
  // Replace all occurrences of t[] with u[] in s[].
  cmChar_t* cmTextReplaceAll( cmChar_t* s, const cmChar_t* t, const cmChar_t* u );
  
  // Replace the first occurence of t[] in s[] with u[].
  cmChar_t* cmTextReplaceFirst( cmChar_t* s, const cmChar_t* t, const cmChar_t* u );

  // Insert u[un] at before t in s[].
  cmChar_t* cmTextInsertSN(  cmChar_t* s, const cmChar_t* t, const cmChar_t* u, unsigned un );

  // Insert u[] at before t in s[].
  cmChar_t* cmTextInsertS(  cmChar_t* s, const cmChar_t* t, const cmChar_t* u );

  cmChar_t* cmTextAppend( cmChar_t* s, unsigned* sn, const cmChar_t* u, unsigned un );

  cmChar_t* cmTextAppendSN( cmChar_t* s, const cmChar_t* u, unsigned un );

  // append u[un] to s[] and append terminating zero.
  cmChar_t* cmTextAppendSNZ( cmChar_t* s, const cmChar_t* u, unsigned un );

  // both s[] and u[] are strz's
  cmChar_t* cmTextAppendSS( cmChar_t* s, const cmChar_t* u );

  // Same as multiple calls to cmTextAppendSS(). 
  // Terminate the var-args list with NULL.
  cmChar_t* cmTextVAppendSS( cmChar_t* s, ... );

  // Append 'n' copies of 'c' to the end of s[].
  cmChar_t* cmTextAppendChar( cmChar_t* s, cmChar_t c, unsigned n );

  // Returns true if the string is NULL or all spaces.
  bool cmTextIsEmpty( const cmChar_t* s );
  bool cmTextIsNotEmpty( const cmChar_t* s );

  // Same as strlen() but handles case where s0 == NULL as length==0.
  unsigned cmTextLength( const cmChar_t* s0 );

  // Same as strcmp() but handles NULL.  Note that if both s0 and s1 are NULL
  // then return is 0.
  int cmTextCmp( const cmChar_t* s0, const cmChar_t* s1 );

  // Same as cmTextCmp() but only compare the first 'n' characters.
  int cmTextCmpN( const cmChar_t* s0, const cmChar_t* s1, unsigned n );

  // Convert text in s0[] to upper/lower case in s1[].
  // Note that s0[] and s1[] may point to the same string
  void cmTextToLower( const cmChar_t* s0, cmChar_t* s1 );
  void cmTextToUpper( const cmChar_t* s0, cmChar_t* s1 );

  // Returns NULL if string contains fewer than lineIdx lines.
  // Note: first line == 1.
  cmChar_t* cmTextLine( cmChar_t* s, unsigned line );
  const cmChar_t* cmTextLineC( const cmChar_t* s, unsigned line );

  // Reduce all consecutive white spaces to a single space.
  cmChar_t* cmTextRemoveConsecutiveSpaces( cmChar_t* s );

  // Columnize s[] by inserting EOL's at the first available
  // space prior 'colCnt' columns. If no space exists then
  // the words will be broken.  If length s[] is less than
  // 'colCnt' then s[] is returned with no changes.
  cmChar_t* cmTextColumize( cmChar_t* s,  unsigned colCnt );

  // Indent the rows of s[] by inserting 'indent' spaces
  // just after each '\n'.  If s[] contains no '\n' then
  // s[] is returned with no changes.
  cmChar_t* cmTextIndentRows( cmChar_t* s,  unsigned indent );

  // Prefix all rows of s[] with p[].
  cmChar_t* cmTextPrefixRows( cmChar_t* s, const cmChar_t* t );

  // Remove leading white space from all rows of s.
  cmChar_t* cmTextTrimRows( cmChar_t* s );



  // Remove all white space prior to the first non-white space
  // character.
  cmChar_t* cmTextEatLeadingSpace( cmChar_t* s );


  // Return a pointer to the beginning of the next row
  // or NULL if there is no next row.
  cmChar_t*       cmTextNextRow(  cmChar_t* s );
  const cmChar_t* cmTextNextRowC( const cmChar_t* s );
  
  // Return the minimum indent of all rows in s[].
  unsigned        cmTextMinIndent( const cmChar_t* s );

  // Outdent s[] by 'n'. 
  // If a row is indented by less than 'n' then it is 
  // then all leading white space is removed.
  cmChar_t*       cmTextOutdent( cmChar_t* s, unsigned n );

  // Given a string containing binary data encoded as base64
  // return the size of the buffer required to hold the
  // decoded binary data. Note that if xV[] is a legal base64
  // string then xN must be a multiple of 4.  If xN is not
  // a mulitple of 4 then the function returns kInvalidCnt.
  unsigned cmTextDecodeBase64BufferByteCount( const char* xV, unsigned xN );

  // Decode the base64 data in xV[xN] into yV[yN].  Note that the
  // minimum value of yN can be determined via
  // cmTextDecodeBase64BufferByteCount().
  // Return the actual count of bytes decoded into yV[].
  unsigned cmTextDecodeBase64( const char* xV, unsigned xN, void* yV, unsigned yN );

  // Given the count of bytes in a binary array, return
  // the count of bytes required to store the array in base64.
  unsigned cmTextEncodeBase64BufferByteCount( unsigned xN );

  // Encode the binary array xV[xN] into yV[yN].  Note that the value
  // of yN can be determined via cmTextEncodeBase64BufferByteCount().
  cmTxRC_t cmTextEncodeBase64( const void* xV, unsigned xN, char* yV, unsigned yN );


  //)
  //}

#ifdef __cplusplus
}
#endif

#endif
