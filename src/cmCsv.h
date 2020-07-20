#ifndef cmCsv_h
#define cmCsv_h


#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"Comma seperated value file reader and writer." kw[file] }
  
  enum
  {
    kOkCsvRC = 0,
    kMemAllocErrCsvRC,
    kLexErrCsvRC,
    kHashTblErrCsvRC,
    kSyntaxErrCsvRC,
    kFileOpenErrCsvRC,
    kFileCreateErrCsvRC,
    kFileReadErrCsvRC,
    kFileSeekErrCsvRC,
    kFileCloseErrCsvRC,
    kDataCvtErrCsvRC,
    kCellNotFoundCsvRC,
    kDuplicateLexCsvId
  };

  typedef unsigned   cmCsvRC_t;
  typedef cmHandle_t cmCsvH_t;

  enum
  {
    kIntCsvTFl   = 0x01,
    kHexCsvTFl   = 0x02,
    kRealCsvTFl  = 0x04,
    kIdentCsvTFl = 0x08,
    kStrCsvTFl   = 0x10,
    kUdefCsvTFl  = 0x20,

    kNumberTMask   = kIntCsvTFl   | kHexCsvTFl | kRealCsvTFl,
    kTextTMask     = kIdentCsvTFl | kStrCsvTFl,
    kTypeTMask     = kNumberTMask | kTextTMask
  };

  // Each non-blank CSV cell is represented by a cmCsvCell_t record.
  // All the non-blank cells in a given row are organized as a linked
  // list throught 'rowPtr'.
  typedef struct cmCsvCell_str
  {
    unsigned              row;     // CSV row number
    unsigned              col;     // CSV column number
    struct cmCsvCell_str* rowPtr;  // links together cells in this row
    
    unsigned              symId;   // symbol id for this cell
    unsigned              flags;   // cell flags (see kXXXCsvTFl)
    unsigned              lexTId;

  } cmCsvCell_t;

  extern cmCsvH_t cmCsvNullHandle;

  cmCsvRC_t cmCsvInitialize( cmCsvH_t *hp, cmCtx_t* ctx );
  cmCsvRC_t cmCsvFinalize(   cmCsvH_t *hp );

  cmCsvRC_t cmCsvInitializeFromFile( cmCsvH_t *hp, const char* fn, unsigned maxRowCnt, cmCtx_t* ctx );

  bool      cmCsvIsValid( cmCsvH_t h);
  cmCsvRC_t cmCsvLastRC(  cmCsvH_t h);
  void      cmCsvClearRC( cmCsvH_t h);
  
  // Register token matchers. See cmLexRegisterToken and cmLexRegisterMatcher
  // for details.
  cmCsvRC_t cmCsvLexRegisterToken(   cmCsvH_t h, unsigned id, const cmChar_t* token );
  cmCsvRC_t cmCsvLexRegisterMatcher( cmCsvH_t h, unsigned id, cmLexUserMatcherPtr_t funcPtr );
  
  // Return the next available lexer token id above the token id's used internally
  // by the object. This value is fixed after cmCsvInitialize()
  // and does not change for the life of the CSV object.  The application is
  // therefore free to choose any lexer id values equal to or above the 
  // returned value.
  unsigned  cmCsvLexNextAvailId(     cmCsvH_t h );

  // Set 'maxRowCnt' to 0 if there is no row limit on the file.
  cmCsvRC_t cmCsvParse(      cmCsvH_t h, const char* buf, unsigned bufCharCnt, unsigned maxRowCnt );
  cmCsvRC_t cmCsvParseFile(  cmCsvH_t h, const char* fn, unsigned maxRowCnt );

  unsigned  cmCsvRowCount(   cmCsvH_t h );

  // Return a pointer to a given cell.
  cmCsvCell_t* cmCsvCellPtr( cmCsvH_t h, unsigned row, unsigned col );

  // Return a pointer to the first cell in a given row
  cmCsvCell_t* cmCsvRowPtr(  cmCsvH_t h, unsigned row );


  // Convert a cell symbold id to a value.
  const char* cmCsvCellSymText(   cmCsvH_t h, unsigned symId );
  cmCsvRC_t   cmCsvCellSymInt(    cmCsvH_t h, unsigned symId, int* vp );
  cmCsvRC_t   cmCsvCellSymUInt(   cmCsvH_t h, unsigned symId, unsigned* vp );
  cmCsvRC_t   cmCsvCellSymFloat(  cmCsvH_t h, unsigned symId, float* vp );
  cmCsvRC_t   cmCsvCellSymDouble( cmCsvH_t h, unsigned symId, double* vp );

  // Return the value associated with a cell.
  const char* cmCsvCellText(  cmCsvH_t h, unsigned row, unsigned col ); // Returns NULL on error.
  int         cmCsvCellInt(   cmCsvH_t h, unsigned row, unsigned col ); // Returns INT_MAX on error.
  unsigned    cmCsvCellUInt(  cmCsvH_t h, unsigned row, unsigned col ); // Returns UINT_MAX on error.
  float       cmCsvCellFloat( cmCsvH_t h, unsigned row, unsigned col ); // Returns FLT_MAX  on error.
  double      cmCsvCellDouble(cmCsvH_t h, unsigned row, unsigned col ); // Returns DBL_MAX on error.

  // Insert a value into the internal symbol table.
  unsigned   cmCsvInsertSymText(   cmCsvH_t h, const char* text );
  unsigned   cmCsvInsertSymInt(    cmCsvH_t h, int v );
  unsigned   cmCsvInsertSymUInt(   cmCsvH_t h, unsigned v );
  unsigned   cmCsvInsertSymHex(    cmCsvH_t h, unsigned v );
  unsigned   cmCsvInsertSymFloat(  cmCsvH_t h, float v );
  unsigned   cmCsvInsertSymDouble( cmCsvH_t h, double v );  


  // Set the value associated with a cell.
  cmCsvRC_t  cmCsvSetCellIdent(  cmCsvH_t h, unsigned row, unsigned col, const char* text );
  cmCsvRC_t  cmCsvSetCellQText(  cmCsvH_t h, unsigned row, unsigned col, const char* text );
  cmCsvRC_t  cmCsvSetCellInt(    cmCsvH_t h, unsigned row, unsigned col, int v );
  cmCsvRC_t  cmCsvSetCellUInt(   cmCsvH_t h, unsigned row, unsigned col, unsigned v );
  cmCsvRC_t  cmCsvSetCellHex(    cmCsvH_t h, unsigned row, unsigned col, unsigned v );
  cmCsvRC_t  cmCsvSetCellFloat(  cmCsvH_t h, unsigned row, unsigned col, float v );
  cmCsvRC_t  cmCsvSetCellDouble( cmCsvH_t h, unsigned row, unsigned col, double v );

  // Insert a new row and column 0 cell just above the row assigned to 'row'.
  // lexTId is an arbitrary id used by the application to set the value of 
  // cmCsvCell_t.lexTId in the new cell. There are no constraints on its value.
  //cmCsvRC_t  cmCsvInsertRowBefore(  cmCsvH_t h, unsigned row, cmCsvCell_t** cellPtrPtr, unsigned symId, unsigned flags, unsigned lexTId );

  // Column 0 will be added if 'cellPtrPtr'!= NULL and 'symId'!=cmInvalidId.
  // If cellPtrPtr and symId are not valid then 'flags' and 'lexTId' are ignored.
  cmCsvRC_t  cmCsvAppendRow( cmCsvH_t h, cmCsvCell_t** cellPtrPtr, unsigned symId, unsigned flags, unsigned lexTId );

  // Insert a new cell to the right of leftCellPtr.
  // lexTId is an arbitrary id used by the application to set the value of 
  // cmCsvCell_t.lexTId in the new cell. There are no constraints on its value.
  cmCsvRC_t  cmCsvInsertColAfter(  cmCsvH_t h, cmCsvCell_t* leftCellPtr, cmCsvCell_t** cellPtrPtr, unsigned symId, unsigned flags, unsigned lexTId );

  cmCsvRC_t  cmCsvInsertIdentColAfter(  cmCsvH_t h, cmCsvCell_t* leftCellPtr, cmCsvCell_t** cellPtrPtr, const char* val, unsigned lexTId );
  cmCsvRC_t  cmCsvInsertQTextColAfter(  cmCsvH_t h, cmCsvCell_t* leftCellPtr, cmCsvCell_t** cellPtrPtr, const char* val, unsigned lexTId );
  cmCsvRC_t  cmCsvInsertIntColAfter(    cmCsvH_t h, cmCsvCell_t* leftCellPtr, cmCsvCell_t** cellPtrPtr, int val,         unsigned lexTId );
  cmCsvRC_t  cmCsvInsertUIntColAfter(   cmCsvH_t h, cmCsvCell_t* leftCellPtr, cmCsvCell_t** cellPtrPtr, unsigned val,    unsigned lexTId );
  cmCsvRC_t  cmCsvInsertHexColAfter(    cmCsvH_t h, cmCsvCell_t* leftCellPtr, cmCsvCell_t** cellPtrPtr, unsigned val,    unsigned lexTId );
  cmCsvRC_t  cmCsvInsertFloatColAfter(  cmCsvH_t h, cmCsvCell_t* leftCellPtr, cmCsvCell_t** cellPtrPtr, float val,       unsigned lexTId );
  cmCsvRC_t  cmCsvInsertDoubleColAfter( cmCsvH_t h, cmCsvCell_t* leftCellPtr, cmCsvCell_t** cellPtrPtr, double val,      unsigned lexTId );

  // Write the CSV object out to a file.
  cmCsvRC_t  cmCsvWrite( cmCsvH_t h, const char* fn );

  cmCsvRC_t  cmCsvPrint( cmCsvH_t h, unsigned rowCnt );

  //)
  
#ifdef __cplusplus
}
#endif

#endif
