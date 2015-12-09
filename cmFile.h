#ifndef cmFile_h
#define cmFile_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc: "File abstraction class." kw:[file system base]}
  // 
  // The cmFile API extends the C standard file handling routines
  // with cm style error handling. All cm file input and output occurs
  // through this interface."
  //
  
  enum
  {
    kOkFileRC = cmOkRC,
    kInvalidFlagFileRC,
    kOpenFailFileRC,
    kCloseFailFileRC,
    kReadFailFileRC,
    kWriteFailFileRC,
    kSeekFailFileRC,
    kTellFailFileRC,
    kPrintFailFileRC,
    kObjAllocFailFileRC,
    kHandleInvalidFileRC,
    kStatFailFileRC,
    kBufAllocFailFileRC,
    kBufTooSmallFileRC,
    kFileSysFailFileRC,

  };

  typedef unsigned   cmFileRC_t;
  typedef cmHandle_t cmFileH_t;

  extern cmFileH_t cmFileNullHandle;

  // Flags for use with cmFileOpen().
  enum cmFileOpenFlags_t
  {
    kReadFileFl   = 0x01, //< Open a file for reading
    kWriteFileFl  = 0x02, //< Create an empty file for writing
    kAppendFileFl = 0x04, //< Open a file for writing at the end of the file.
    kUpdateFileFl = 0x08, //< Open a file for reading and writing.
    kBinaryFileFl = 0x10, //< Open a file for binary (not text) input/output.
    kStdoutFileFl = 0x20, //< Ignore fn use 'stdout'
    kStderrFileFl = 0x40, //< Ignore fn use 'stderr'
    kStdinFileFl  = 0x80, //< Ignore fn use 'stdin'
  };

  // Open or create a file.    
  // Equivalent to fopen().
  // If *hp was not initalized by an earlier call to cmFileOpen() then it should 
  // be set to cmFileNullHandle prior to calling this function. If *hp is a valid handle
  // then it is automatically finalized by an internal call to cmFileClose() prior to
  // being re-iniitalized.
  //
  // If kStdoutFileFl, kStderrFileFl or kStdinFileFl are set then 
  // file name argument 'fn' is ignored.
  cmFileRC_t cmFileOpen(    
    cmFileH_t*             hp,    // Pointer to a client supplied cmFileHandle_t to recieve the handle for the new object.
    const cmChar_t*        fn,    // The name of the file to open or create. 
    enum cmFileOpenFlags_t flags, // See cmFileOpenFlags_t
    cmRpt_t*               rpt    // The cmRpt_t to use for error reporting
                            );

  // Close a file opened with  Equivalent to fclose().
  cmFileRC_t cmFileClose(   cmFileH_t* hp );

  // Return true if the file handle is associated with an open file.
  bool       cmFileIsValid( cmFileH_t h );

  // Read a block bytes from a file. Equivalent to fread().
  cmFileRC_t cmFileRead(    cmFileH_t h, void* buf, unsigned bufByteCnt );

  // Write a block of bytes to a file. Equivalent to fwrite().
  cmFileRC_t cmFileWrite(   cmFileH_t h, const void* buf, unsigned bufByteCnt );
  
  enum cmFileSeekFlags_t 
  { 
    kBeginFileFl = 0x01, 
    kCurFileFl   = 0x02, 
    kEndFileFl   = 0x04 
  };

  // Set the file position indicator. Equivalent to fseek().
  cmFileRC_t cmFileSeek(    cmFileH_t h, enum cmFileSeekFlags_t flags, int offsByteCnt );

  // Return the file position indicator. Equivalent to ftell().
  cmFileRC_t cmFileTell(    cmFileH_t h, long* offsPtr );

  // Return true if the file position indicator is at the end of the file. 
  // Equivalent to feof().
  bool       cmFileEof(     cmFileH_t h );

  // Return the length of the file in bytes
  unsigned   cmFileByteCount(  cmFileH_t h );
  cmFileRC_t cmFileByteCountFn( const cmChar_t* fn, cmRpt_t* rpt, unsigned* fileByteCntPtr );

  // Set *isEqualPtr=true if the two files are identical.
  cmFileRC_t cmFileCompare( const cmChar_t* fn0, const cmChar_t* fn1, cmRpt_t* rpt, bool* isEqualFlPtr );

  // Return the file name associated with a file handle.
  const cmChar_t* cmFileName( cmFileH_t h );

  // Write a buffer to a file.
  cmFileRC_t cmFileFnWrite( const cmChar_t* fn, cmRpt_t* rpt, const void* buf, unsigned bufByteCnt );

  // Allocate and fill a buffer from the file.
  // Set *bufByteCntPtr to count of bytes read into the buffer.
  // 'bufByteCntPtr' is optional - set it to NULL if it is not required by the caller.
  // It is the callers responsibility to delete the returned buffer with a
  // call to cmMemFree()
  cmChar_t*  cmFileToBuf( cmFileH_t h, unsigned* bufByteCntPtr ); 
  
  // Same as cmFileToBuf() but accepts a file name argument.
  // 'rpt' is the report object to use for error reporting.
  cmChar_t*  cmFileFnToBuf( const cmChar_t* fn, cmRpt_t* rpt, unsigned* bufByteCntPtr );


  // Copy the file named in srcDir/srcFn/srcExt to a file named dstDir/dstFn/dstExt.
  // Note that srcExt/dstExt may be set to NULL if the file extension is included
  // in srcFn/dstFn.  Likewise srcFn/dstFn may be set to NULL if the file name
  // is included in srcDir/dstDir.
  cmFileRC_t    cmFileCopy( 
    const cmChar_t* srcDir, 
    const cmChar_t* srcFn, 
    const cmChar_t* srcExt, 
    const cmChar_t* dstDir, 
    const cmChar_t* dstFn, 
    const cmChar_t* dstExt,
    cmErr_t*        err);


  // This function creates a backup copy of the file 'fn' by duplicating it into
  // a file named fn_#.ext where # is an integer which makes the file name unique.
  // The integers chosen with zero and are incremented until an
  // unused file name is found in the same directory as 'fn'.
  // If the file identified by 'fn' is not found then the function returns quietly.
  cmFileRC_t cmFileBackup( const cmChar_t* dir, const cmChar_t* name, const cmChar_t* ext, cmErr_t* err );
  
  // Allocate and fill a zero terminated string from a file.
  // Set *bufByteCntPtr to count of bytes read into the buffer.=
  // (the buffer memory size is one byte larger to account for the terminating zero) 
  // 'bufByteCntPtr' is optional - set it to NULL if it is not required by the caller.
  // It is the callers responsibility to delete the returned buffer with a
  // call to cmMemFree()
  cmChar_t*  cmFileToStr( cmFileH_t h, unsigned* bufByteCntPtr );

  // Same as cmFileToBuf() but accepts a file name argument.
  // 'rpt' is the report object to use for error reporting.
  cmChar_t*  cmFileFnToStr( const cmChar_t* fn, cmRpt_t* rpt, unsigned* bufByteCntPtr );

  // Return the count of lines in a file.
  cmFileRC_t cmFileLineCount( cmFileH_t h, unsigned* lineCntPtr );

  // Read the next line into buf[bufByteCnt]. 
  // Consider using cmFileGetLineAuto() as an alternative to this function 
  // to avoid having to use a buffer with an explicit size.
  //
  // If buf is not long enough to hold the entire string then
  //
  // 1. The function returns kFileBufTooSmallRC
  // 2. *bufByteCntPtr is set to the size of the required buffer.
  // 3. The internal file position is left unchanged.
  //
  // If the buffer is long enough to hold the entire line then 
  // *bufByteCntPtr is left unchanged.
  // See  cmFileGetLineTest() in cmProcTest.c or cmFileGetLineAuto()
  // in cmFile.c for examples of how to use this function to a
  // achieve proper buffer sizing.
  cmFileRC_t cmFileGetLine( cmFileH_t h, cmChar_t* buf, unsigned* bufByteCntPtr );

  // A version of cmFileGetLine() which eliminates the need to handle buffer
  // sizing. 
  //
  // Example usage:
  // 
  // cmChar_t* buf        = NULL;
  // unsigned  bufByteCnt = 0;
  // while(cmFileGetLineAuto(h,&buf,&bufByteCnt)==kOkFileRC)
  //   proc(buf);
  // cmMemPtrFree(buf);
  //
  // On the first call to this function *bufPtrPtr must be set to NULL and
  // *bufByteCntPtr must be set to 0.
  // Following the last call to this function call cmMemPtrFree(bufPtrptr)
  // to be sure the line buffer is fully released. Note this step is not
  // neccessary if the last call does not return kOkFileRC.
  cmFileRC_t cmFileGetLineAuto( cmFileH_t h, cmChar_t** bufPtrPtr, unsigned* bufByteCntPtr );

  // Binary Array Reading Functions
  // Each of these functions reads a block of binary data from a file.
  // The advantage to using these functions over cmFileRead() is only that they are type specific.
  cmFileRC_t cmFileReadChar(   cmFileH_t h, char*           buf, unsigned cnt );
  cmFileRC_t cmFileReadUChar(  cmFileH_t h, unsigned char*  buf, unsigned cnt );
  cmFileRC_t cmFileReadShort(  cmFileH_t h, short*          buf, unsigned cnt );
  cmFileRC_t cmFileReadUShort( cmFileH_t h, unsigned short* buf, unsigned cnt );
  cmFileRC_t cmFileReadLong(   cmFileH_t h, long*           buf, unsigned cnt );
  cmFileRC_t cmFileReadULong(  cmFileH_t h, unsigned long*  buf, unsigned cnt );
  cmFileRC_t cmFileReadInt(    cmFileH_t h, int*            buf, unsigned cnt );
  cmFileRC_t cmFileReadUInt(   cmFileH_t h, unsigned int*   buf, unsigned cnt );
  cmFileRC_t cmFileReadFloat(  cmFileH_t h, float*          buf, unsigned cnt );
  cmFileRC_t cmFileReadDouble( cmFileH_t h, double*         buf, unsigned cnt );
  cmFileRC_t cmFileReadBool(   cmFileH_t h, bool*           buf, unsigned cnt );

  // Binary Array Writing Functions
  // Each of these functions writes an array to a binary file.
  // The advantage to using functions rather than cmFileWrite() is only that they are type specific.
  cmFileRC_t cmFileWriteChar(   cmFileH_t h, const char*           buf, unsigned cnt );
  cmFileRC_t cmFileWriteUChar(  cmFileH_t h, const unsigned char*  buf, unsigned cnt );
  cmFileRC_t cmFileWriteShort(  cmFileH_t h, const short*          buf, unsigned cnt );
  cmFileRC_t cmFileWriteUShort( cmFileH_t h, const unsigned short* buf, unsigned cnt );
  cmFileRC_t cmFileWriteLong(   cmFileH_t h, const long*           buf, unsigned cnt );
  cmFileRC_t cmFileWriteULong(  cmFileH_t h, const unsigned long*  buf, unsigned cnt );
  cmFileRC_t cmFileWriteInt(    cmFileH_t h, const int*            buf, unsigned cnt );
  cmFileRC_t cmFileWriteUInt(   cmFileH_t h, const unsigned int*   buf, unsigned cnt );
  cmFileRC_t cmFileWriteFloat(  cmFileH_t h, const float*          buf, unsigned cnt );
  cmFileRC_t cmFileWriteDouble( cmFileH_t h, const double*         buf, unsigned cnt );
  cmFileRC_t cmFileWriteBool(   cmFileH_t h, const bool*           buf, unsigned cnt );

  // Write a string to a file as <N> <char0> <char1> ... <char(N-1)>
  // where N is the count of characters in the string.
  cmFileRC_t cmFileWriteStr( cmFileH_t h, const cmChar_t* s );

  // Read a string back from a file as written by cmFileWriteStr().
  // Note that the string will by string will be dynamically allocated
  // and threfore must eventually be released via cmMemFree().
  // If maxCharN is set to zero then the default maximum string
  // length is 16384.  Note that this limit is used to prevent
  // corrupt files from generating excessively long strings.
  cmFileRC_t cmFileReadStr(  cmFileH_t h, cmChar_t** sRef, unsigned maxCharN );

  // Formatted Text Output Functions:
  // Print formatted text to a file.
  cmFileRC_t cmFilePrint(   cmFileH_t h, const cmChar_t* text );
  cmFileRC_t cmFilePrintf(  cmFileH_t h, const cmChar_t* fmt, ... );
  cmFileRC_t cmFileVPrintf( cmFileH_t h, const cmChar_t* fmt, va_list vl );


  cmFileRC_t cmFileLastRC( cmFileH_t h );
  cmFileRC_t cmFileSetRC( cmFileH_t h, cmFileRC_t rc );
  
  //)

#ifdef __cplusplus
}
#endif


#endif
