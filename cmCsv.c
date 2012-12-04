#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLex.h"
#include "cmLinkedHeap.h"
#include "cmSymTbl.h"
#include "cmCsv.h"
#include "cmText.h"

enum
{
  kCommaLexTId = kUserLexTId+1,

  kMaxLexTId // always last in the lex id list
};

typedef struct
{
  const char* text;
  unsigned    id;  
} cmCsvToken_t;


cmCsvToken_t _cmCsvTokenArray[] = 
{
  { ",",  kCommaLexTId },
  { "",   kErrorLexTId }
};

// The binder linked list contains a list of records which tracks the first 
// (left-most) non-blank column in each row. 
typedef struct cmCsvBind_str
{
  struct cmCsvBind_str* linkPtr; 
  cmCsvCell_t*          rowPtr;
} cmCsvBind_t;

typedef struct cmCsvUdef_str
{
  struct cmCsvUdef_str* linkPtr;
  unsigned              id;
} cmCsvUdef_t;

typedef struct
{
  cmErr_t            err;
  void*              rptDataPtr;     //
  cmLexH             lexH;           // parsing lexer 
  cmSymTblH_t        symTblH;        // all XML identifiers and data is stored as a symbol in this table
  cmLHeapH_t         heapH;
  cmCsvBind_t*       bindPtr;      // base of the binder linked list
  cmCsvCell_t*       curRowPtr;    // used by the parser to track the row being filled
  cmCsvUdef_t*       udefList;
} cmCsv_t;

cmCsvH_t cmCsvNullHandle = { NULL };

cmCsv_t* _cmCsvHandleToPtr( cmCsvH_t h )
{
  cmCsv_t* p = (cmCsv_t*)h.h;
  assert( p != NULL );
  cmErrClearRC(&p->err);
  return p;
}

cmCsvRC_t _cmCsvVError( cmCsv_t* p, cmCsvRC_t rc, const char* fmt, va_list vl )
{  return cmErrVMsg(&p->err,rc,fmt,vl); }

cmCsvRC_t _cmCsvError( cmCsv_t* p, cmCsvRC_t rc, const char* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  rc = _cmCsvVError(p,rc,fmt,vl);
  va_end(vl);
  return rc;
}


cmCsvRC_t cmCsvInitialize( cmCsvH_t *hp, cmCtx_t* ctx )
{
  cmCsvRC_t rc;
  cmCsv_t* p = NULL;
  unsigned i;

  if((rc = cmCsvFinalize(hp)) != kOkCsvRC )
    return rc;

  // create the base object
  p = cmMemAllocZ( cmCsv_t, 1 );
  assert(p != NULL);

  cmErrSetup(&p->err,&ctx->rpt,"CSV");

  // create the symbol table
  if( cmSymTblIsValid(p->symTblH = cmSymTblCreate(cmSymTblNullHandle,0,ctx)) == false )
  {
    rc = _cmCsvError(p,kSymTblErrCsvRC,"Symbol table creation failed.");
    goto errLabel;
  }

  // allocate the linked heap mgr
  if( cmLHeapIsValid(p->heapH = cmLHeapCreate(1024,ctx)) == false )
  {
    rc = _cmCsvError(p,kMemAllocErrCsvRC,"Linked heap object allocation failed.");
    goto errLabel;
  }

  // allocate the lexer
  if(cmLexIsValid(p->lexH = cmLexInit(NULL,0,0,&ctx->rpt)) == false )
  {
    rc =  _cmCsvError(p,kLexErrCsvRC,"Lex allocation failed.");
    goto errLabel;
  }

  // register CSV specific tokens with the lexer
  for(i=0; _cmCsvTokenArray[i].id != kErrorLexTId; ++i)
  {
    cmRC_t lexRC;
    if( (lexRC = cmLexRegisterToken(p->lexH, _cmCsvTokenArray[i].id, _cmCsvTokenArray[i].text )) != kOkLexRC )
    {
      rc = _cmCsvError(p,kLexErrCsvRC,"Lex token registration failed for:'%s'.\nLexer Error:%s",_cmCsvTokenArray[i].text, cmLexRcToMsg(lexRC) );
      goto errLabel;
    }
  }

  hp->h = p;

  return kOkCsvRC;
      
 errLabel:
  
  cmMemPtrFree(&p);

  if( cmLHeapIsValid(p->heapH) )
    cmLHeapDestroy(&p->heapH);

  if( cmLexIsValid(p->lexH) )
    cmLexFinal(&p->lexH);

  return rc;

}
cmCsvRC_t cmCsvFinalize(   cmCsvH_t *hp )
{

  cmRC_t lexRC;

  if( hp == NULL || hp->h == NULL )
    return kOkCsvRC;

  cmCsv_t* p = _cmCsvHandleToPtr(*hp);
 
  // free the internal heap object
  cmLHeapDestroy( &p->heapH );

  // free the lexer
  if((lexRC = cmLexFinal(&p->lexH)) != kOkLexRC )
    return _cmCsvError(p,kLexErrCsvRC,"Lexer finalization failed.\nLexer Error:%s",cmLexRcToMsg(lexRC));

  // free the symbol table
  cmSymTblDestroy(&p->symTblH);

  // free the handle
  cmMemPtrFree(&hp->h);

  return kOkCsvRC;
}

cmCsvRC_t cmCsvInitializeFromFile( cmCsvH_t *hp, const char* fn, unsigned maxRowCnt, cmCtx_t* ctx )
{
  cmCsvRC_t rc;

  if((rc = cmCsvInitialize(hp,ctx)) != kOkCsvRC )
    return rc;

  return cmCsvParseFile( *hp, fn, maxRowCnt);
}


bool cmCsvIsValid(    cmCsvH_t h)
{ return h.h != NULL; } 

cmCsvRC_t cmCsvLastRC( cmCsvH_t h )
{ 
  cmCsv_t* p = _cmCsvHandleToPtr(h);
  return cmErrLastRC(&p->err);
}

void cmCsvClearRC( cmCsvH_t h )
{
  cmCsv_t* p = _cmCsvHandleToPtr(h);
  cmErrClearRC(&p->err);
}


cmCsvRC_t cmCsvParseFile(  cmCsvH_t h, const char* fn, unsigned maxRowCnt )
{
  cmCsvRC_t rc     = kOkCsvRC;
  FILE*    fp      = NULL;
  cmCsv_t*  p      = _cmCsvHandleToPtr(h);
  unsigned n       = 0;
  char*    textBuf = NULL;

  assert( fn != NULL && p != NULL );
  
  // open the file
  if((fp = fopen(fn,"rb")) == NULL )
    return _cmCsvError(p,kFileOpenErrCsvRC,"Unable to open the file:'%s'.",fn);

  // seek to the end
  if( fseek(fp,0,SEEK_END) != 0 )
  {
    rc= _cmCsvError(p,kFileSeekErrCsvRC,"Unable to seek to the end of '%s'.",fn);
    goto errLabel;
  }

  // get the length of the file
  if( (n=ftell(fp)) == 0 )
  {
    rc = _cmCsvError(p,kFileOpenErrCsvRC,"The file '%s' appears to be empty.",fn);
    goto errLabel;
  }

  // rewind the file
  if( fseek(fp,0,SEEK_SET) != 0 )
  {
    rc = _cmCsvError(p,kFileSeekErrCsvRC,"Unable to seek to the beginning of '%s'.",fn);
    goto errLabel;
  }

  // allocate the text buffer
  if((textBuf = cmMemAllocZ( char, n+1)) == NULL )
  {
    rc = _cmCsvError(p,kMemAllocErrCsvRC,"Unable to allocate the text file buffer for:'%s'.",fn);
    goto errLabel;
  }

  // read the file into the text buffer
  if( fread(textBuf,n,1,fp) != 1 )
  {
    rc = _cmCsvError(p,kFileReadErrCsvRC,"File read failed on:'%s'.",fn);
    goto errLabel;
  }  

  rc = cmCsvParse(h,textBuf,n,maxRowCnt); 

 errLabel:

  // close the file
  if( fclose(fp) != 0 )
  {
    rc =  _cmCsvError(p,kFileCloseErrCsvRC,"File close failed on:'%s'.",fn);
    goto errLabel;
  }

  // free the buffer
  if( textBuf != NULL )
    cmMemFree(textBuf);

  return rc;

}

cmCsvRC_t _cmCsvAppendNewBindRecd( cmCsv_t* p, cmCsvBind_t** newBindPtrPtr, unsigned* newRowIdxPtr  )
{
  cmCsvBind_t* nbp;
  cmCsvBind_t* bp = p->bindPtr;
  unsigned     newRowIdx = 0;

  if( newBindPtrPtr != NULL )
    *newBindPtrPtr = NULL;

  if( newRowIdxPtr != NULL )
    *newRowIdxPtr = cmInvalidIdx;
  
  // allocate the binder record
  if((nbp = cmLHeapAllocZ( p->heapH, sizeof(cmCsvBind_t))) == NULL )
    return _cmCsvError(p,kMemAllocErrCsvRC,"Binding record allocation failed.");

  // if this is the first binder record
  if( p->bindPtr == NULL )
  {
    p->bindPtr = nbp;
    bp         = nbp;
  }
  else
  {
    newRowIdx = 1;

    // iterate to the bottom of the binding
    while( bp->linkPtr != NULL )
    {
      bp = bp->linkPtr;

      ++newRowIdx;
    }

    bp->linkPtr = nbp;
  }

  if( newBindPtrPtr != NULL )
    *newBindPtrPtr = nbp;

  if( newRowIdxPtr != NULL )
    *newRowIdxPtr = newRowIdx;
  

  return kOkCsvRC;
}

cmCsvRC_t _cmCsvCreateBind( cmCsv_t* p, cmCsvCell_t* cp, const char* tokenText, unsigned lexRow, unsigned lexCol )
{
  cmCsvRC_t rc;
  cmCsvBind_t* nbp;
  if((rc = _cmCsvAppendNewBindRecd(p,&nbp,NULL)) != kOkCsvRC )
    return _cmCsvError(p,kMemAllocErrCsvRC,"Binding record allocation failed for '%s' on line %i column %i.",tokenText,lexRow,lexCol);
  
  nbp->rowPtr  = cp;  

  return rc;
}

cmCsvRC_t  _cmCsvAllocCell( cmCsv_t* p, unsigned symId, unsigned flags, unsigned cellRow, unsigned cellCol, cmCsvCell_t** cpp, unsigned lexTId )
{
  cmCsvCell_t* cp;

  // allocate cell memory
  if(( cp = cmLHeapAllocZ(p->heapH, sizeof(cmCsvCell_t) )) == NULL )
    return _cmCsvError(p,kMemAllocErrCsvRC,"Cell allocation failed. for row: %i column %i.",cellRow+1,cellCol+1);

  cp->row   = cellRow;
  cp->col   = cellCol;
  cp->symId = symId;
  cp->flags = flags;
  cp->lexTId= lexTId;

  *cpp = cp;

  return kOkCsvRC;
}

cmCsvRC_t _cmCsvCreateCell( cmCsv_t* p, const char* tokenText, unsigned flags, unsigned cellRow, unsigned cellCol, unsigned lexTId )
{
  unsigned     symId  = cmInvalidId;
  cmCsvCell_t* cp     = NULL;
  unsigned     lexRow = cmLexCurrentLineNumber(p->lexH);
  unsigned     lexCol = cmLexCurrentColumnNumber(p->lexH);
  cmCsvRC_t    rc     = kOkCsvRC;

  // register the token text as a symbol
  if((symId = cmSymTblRegisterSymbol(p->symTblH,tokenText)) == cmInvalidId )
    return _cmCsvError(p,kSymTblErrCsvRC,"Symbol registration failed. for '%s' on line %i column %i.",tokenText,lexRow,lexCol);

  // allocate a cell
  if((rc = _cmCsvAllocCell(p,symId,flags,cellRow,cellCol,&cp,lexTId)) != kOkCsvRC )
    return rc;
  
  // if this is the first cell in the row
  if( p->curRowPtr == NULL )
  {    
    // link in a new binding record
    if((rc = _cmCsvCreateBind( p, cp,tokenText,lexRow,lexCol)) != kOkCsvRC )
      return rc; // this return will result in a leak from the lheap but it is a fatal error so ignore it

    // update the current row ptr
    p->curRowPtr  = cp;
  }
  else
  {
    // iterate to the end of the row
    cmCsvCell_t* rp = p->curRowPtr;
    while( rp->rowPtr != NULL )
      rp = rp->rowPtr;
    
    rp->rowPtr = cp;
  }

  return rc;
  
}

const cmCsvUdef_t* _cmCsvFindUdef( cmCsv_t* p, unsigned id )
{
  const cmCsvUdef_t* u = p->udefList;
  while( u != NULL )
  {
    if( u->id == id )
      return u;

    u = u->linkPtr;
  }

  return NULL;
}

cmCsvRC_t _cmCsvRegUdef( cmCsv_t* p, unsigned id )
{
  if( _cmCsvFindUdef(p,id) != NULL )
    return _cmCsvError(p,kDuplicateLexCsvId,"%i has already been used as a user defined id.",id);

  cmCsvUdef_t* u = cmLhAllocZ(p->heapH,cmCsvUdef_t,1);
  u->id       = id;
  u->linkPtr  = p->udefList;
  p->udefList = u;
  return kOkCsvRC;
}

cmCsvRC_t    cmCsvLexRegisterToken(   cmCsvH_t h, unsigned id, const cmChar_t* token )
{
  cmCsv_t*    p   = _cmCsvHandleToPtr(h);
  if(cmLexRegisterToken(p->lexH,id,token) != kOkLexRC )
    return _cmCsvError(p, kLexErrCsvRC,"Error registering user defined token '%s'.",cmStringNullGuard(token));

  return _cmCsvRegUdef(p,id);
}

cmCsvRC_t    cmCsvLexRegisterMatcher( cmCsvH_t h, unsigned id, cmLexUserMatcherPtr_t matchFunc )
{
  cmCsv_t*    p   = _cmCsvHandleToPtr(h);
  if( cmLexRegisterMatcher(p->lexH,id,matchFunc) != kOkLexRC )
    return _cmCsvError(p, kLexErrCsvRC,"Error registering user defined matching function.");

  return _cmCsvRegUdef(p,id);
}

unsigned  cmCsvLexNextAvailId(     cmCsvH_t h )
{  return kMaxLexTId; }

cmCsvRC_t cmCsvParse(      cmCsvH_t h, const char* buf, unsigned bufCharCnt, unsigned maxRowCnt )
{
  cmCsvRC_t   rc            = kOkCsvRC;
  unsigned    prvLexRow     = 1;
  unsigned    csvRowIdx     = 0;
  unsigned    csvColIdx     = 0;
  unsigned    lexTId        = kErrorLexTId; // cur lex token type id
  cmCsv_t*    p             = _cmCsvHandleToPtr(h);

  // assign the text buffer and reset the lexer
  if((rc = cmLexSetTextBuffer( p->lexH, buf, bufCharCnt )) != kOkLexRC )
    return _cmCsvError( p, kLexErrCsvRC, "Error setting lexer buffer.\nLexer Error:%s",cmLexRcToMsg(rc));
  
  // get the next token
  while( (lexTId = cmLexGetNextToken( p->lexH )) != kErrorLexTId && (lexTId != kEofLexTId ) && (rc==kOkCsvRC) && (maxRowCnt==0 || csvRowIdx<maxRowCnt))
  {
    unsigned flags     = 0;
    unsigned curLexRow = cmLexCurrentLineNumber(p->lexH);

    // if we are starting a new row
    if( curLexRow != prvLexRow )
    {
      prvLexRow = curLexRow;
      ++csvRowIdx;
      csvColIdx = 0;
      p->curRowPtr = NULL;  // force a new binding record
    }


    // copy the token text in a buffer
    unsigned n = cmLexTokenCharCount(p->lexH);
    char buf[n+1];
    strncpy(buf, cmLexTokenText(p->lexH), n );
    buf[n]=0;

    // do cell data type specific processing
    switch( lexTId )
    {
      case kCommaLexTId:
        ++csvColIdx;
        break;

      case kRealLexTId: flags = kRealCsvTFl;  break;
      case kIntLexTId:  flags = kIntCsvTFl;   break;
      case kHexLexTId:  flags = kHexCsvTFl;   break;
      case kIdentLexTId:flags = kIdentCsvTFl; break; 
      case kQStrLexTId: flags = kStrCsvTFl;   break;

      default:
        {
          const cmCsvUdef_t* u;
          if((u = _cmCsvFindUdef(p,lexTId)) == NULL )
            rc = _cmCsvError(p, kSyntaxErrCsvRC, "Unrecognized token type: '%s' for token: '%s' row:%i col:%i.",cmLexIdToLabel(p->lexH,lexTId),buf,csvRowIdx+1,csvColIdx+1);
          else
            flags = kStrCsvTFl | kUdefCsvTFl;            
        }
    }

    // if no error occurred and the token is not a comma  and the cell is not empty
    if( rc == kOkCsvRC && lexTId != kCommaLexTId && strlen(buf) > 0 )
      if((rc = _cmCsvCreateCell( p, buf,flags, csvRowIdx, csvColIdx, lexTId )) != kOkCsvRC )
        rc = _cmCsvError(p,rc,"CSV cell create fail on row:%i col:%i\n",csvRowIdx+1,csvColIdx+1);
  }

  if( lexTId == kErrorLexTId )
    rc = _cmCsvError(p, kSyntaxErrCsvRC,"The lexer encountered an unrecognized token row:%i col:%i.",csvRowIdx+1,csvColIdx+1);

  return rc;
}

unsigned  cmCsvRowCount( cmCsvH_t h )
{
  cmCsv_t* p  = _cmCsvHandleToPtr(h);

  unsigned     rowCnt = 0;
  cmCsvBind_t* bp     = p->bindPtr;

  for(; bp != NULL; ++rowCnt )
    bp = bp->linkPtr;

  return rowCnt;
}

cmCsvCell_t* cmCsvRowPtr( cmCsvH_t h, unsigned row )
{
  cmCsv_t*     p  = _cmCsvHandleToPtr(h);
  cmCsvBind_t* bp = p->bindPtr;
  
  while( bp != NULL )
  {
    // note: bp->rowPtr of blank rows will be NULL
    if( bp->rowPtr!=NULL && bp->rowPtr->row == row )
      return bp->rowPtr;

    bp = bp->linkPtr;
  }  

  return NULL;
}

cmCsvCell_t* cmCsvCellPtr( cmCsvH_t h, unsigned row, unsigned col )
{
  cmCsvCell_t* cp;

  if((cp = cmCsvRowPtr(h,row)) == NULL )
    return NULL;

  while( cp != NULL )
  {
    if( cp->col == col )
      return cp;

    cp = cp->rowPtr;
  }

  return NULL;
}

cmCsvCell_t* _cmCsvCellPtr( cmCsvH_t h, unsigned row, unsigned col )
{
  cmCsvCell_t* cp;
  if((cp = cmCsvCellPtr(h,row,col)) == NULL )
    _cmCsvError(_cmCsvHandleToPtr(h),kCellNotFoundCsvRC,"Cell at row:%i col:%i not found.",row,col);
  return cp;
}

const char* cmCsvCellSymText(   cmCsvH_t h, unsigned symId )
{
  cmCsv_t*    p = _cmCsvHandleToPtr(h);
  const char* cp;

  if((cp =  cmSymTblLabel(p->symTblH,symId)) == NULL )
    _cmCsvError(p,kSymTblErrCsvRC,"The text associated with the symbol '%i' was not found.",symId);

  return cp;
}

cmCsvRC_t   cmCsvCellSymInt(    cmCsvH_t h, unsigned symId, int* vp )
{
  const char* cp;
  cmCsv_t*    p  = _cmCsvHandleToPtr(h);

  if((cp = cmCsvCellSymText(h,symId)) == NULL )
    return kSymTblErrCsvRC;

  if( cmTextToInt(cp,vp,&p->err) != kOkTxRC )
    return _cmCsvError(p,kDataCvtErrCsvRC,"CSV text to int value failed.");

  return kOkCsvRC;
}

cmCsvRC_t  cmCsvCellSymUInt(   cmCsvH_t h, unsigned symId, unsigned* vp )
{
  const char* cp;
  cmCsv_t*    p = _cmCsvHandleToPtr(h);

  if((cp = cmCsvCellSymText(h,symId)) == NULL )
    return kSymTblErrCsvRC;

  if( cmTextToUInt(cp,vp,&p->err) != kOkTxRC )
    return _cmCsvError(p,kDataCvtErrCsvRC,"CSV text to uint value failed.");

  return kOkCsvRC;
}

cmCsvRC_t   cmCsvCellSymFloat(  cmCsvH_t h, unsigned symId, float* vp )
{
  const char* cp;
  cmCsv_t*    p  = _cmCsvHandleToPtr(h);

  if((cp = cmCsvCellSymText(h,symId)) == NULL )
    return kSymTblErrCsvRC;

  if( cmTextToFloat(cp,vp,&p->err) != kOkTxRC )
    return _cmCsvError(p,kDataCvtErrCsvRC,"CSV text to float value failed.");
  
  return kOkCsvRC;
}

cmCsvRC_t    cmCsvCellSymDouble( cmCsvH_t h, unsigned symId, double* vp )
{
  const char* cp;
  cmCsv_t*    p  = _cmCsvHandleToPtr(h);

  if((cp = cmCsvCellSymText(h,symId)) == NULL )
    return kSymTblErrCsvRC;

  if( cmTextToDouble(cp,vp,&p->err) != kOkTxRC )
    return _cmCsvError(p,kDataCvtErrCsvRC,"CSV text to double value failed.");

  return kOkCsvRC;
}

const char* cmCsvCellText(  cmCsvH_t h, unsigned row, unsigned col )
{
  cmCsvCell_t* cp;
  if((cp = cmCsvCellPtr(h,row,col)) == NULL )
    return NULL;

  return cmCsvCellSymText(h,cp->symId);
}

int         cmCsvCellInt(   cmCsvH_t h, unsigned row, unsigned col )
{
  cmCsvCell_t* cp;
  int          v;

  if((cp = cmCsvCellPtr(h,row,col)) == NULL )
    return INT_MAX;

  if(cmCsvCellSymInt(h,cp->symId,&v) != kOkCsvRC )
    return INT_MAX;

  return v;
}

unsigned    cmCsvCellUInt(  cmCsvH_t h, unsigned row, unsigned col )
{
  cmCsvCell_t* cp;
  unsigned      v;

  if((cp = cmCsvCellPtr(h,row,col)) == NULL )
    return UINT_MAX;

  if(cmCsvCellSymUInt(h,cp->symId,&v) != kOkCsvRC )
    return UINT_MAX;

  return v;
}

float       cmCsvCellFloat( cmCsvH_t h, unsigned row, unsigned col )
{
  cmCsvCell_t* cp;
  float         v;

  if((cp = cmCsvCellPtr(h,row,col)) == NULL )
    return FLT_MAX;

  if(cmCsvCellSymFloat(h,cp->symId,&v) != kOkCsvRC )
    return FLT_MAX;

  return v;
}

double      cmCsvCellDouble(cmCsvH_t h, unsigned row, unsigned col )
{
  cmCsvCell_t* cp;
  double       v;

  if((cp = cmCsvCellPtr(h,row,col)) == NULL )
    return DBL_MAX;

  if(cmCsvCellSymDouble(h,cp->symId,&v) != kOkCsvRC )
    return DBL_MAX;

  return v;
}



unsigned   cmCsvInsertSymText(   cmCsvH_t h, const char* text )
{
  cmCsv_t*    p  = _cmCsvHandleToPtr(h);
  unsigned    symId;

  if((symId = cmSymTblRegisterSymbol(p->symTblH,text)) == cmInvalidId )
    _cmCsvError(p,kSymTblErrCsvRC,"'%s' could not be inserted into the symbol table.",text);

  return symId;
}

unsigned   cmCsvInsertSymInt(    cmCsvH_t h, int v )
{
  const char* fmt = "%i";
  unsigned    n   = snprintf(NULL,0,fmt,v)+1;
  char        buf[n];

  buf[0]= 0;
  if( snprintf(buf,n,fmt,v) == n-1 )
    return cmCsvInsertSymText(h,buf);
  
  _cmCsvError(_cmCsvHandleToPtr(h),kDataCvtErrCsvRC,"The integer %i could not be converted to text.",v);
  return cmInvalidId;
}

unsigned   cmCsvInsertSymUInt(   cmCsvH_t h, unsigned v )
{
  const char* fmt = "%i";
  unsigned    n   = snprintf(NULL,0,fmt,v)+1;
  char        buf[n];

  buf[0]= 0;
  if( snprintf(buf,n,fmt,v) == n-1 )
    return cmCsvInsertSymText(h,buf);
  
  _cmCsvError(_cmCsvHandleToPtr(h),kDataCvtErrCsvRC,"The unsigned int %i could not be converted to text.",v);
  return cmInvalidId;
}

unsigned   cmCsvInsertSymFloat(  cmCsvH_t h, float v )
{
  const char* fmt = "%f";
  unsigned    n   = snprintf(NULL,0,fmt,v)+1;
  char        buf[n];

  buf[0]                     = 0;
  if( snprintf(buf,n,fmt,v) == n-1 )
    return cmCsvInsertSymText(h,buf);
  
  _cmCsvError(_cmCsvHandleToPtr(h),kDataCvtErrCsvRC,"The float %f could not be converted to text.",v);
  return cmInvalidId;
}

unsigned   cmCsvInsertSymDouble( cmCsvH_t h, double v )  
{
  const char* fmt = "%f";
  unsigned    n   = snprintf(NULL,0,fmt,v)+1;
  char        buf[n];

  buf[0]= 0;
  if( snprintf(buf,n,fmt,v) == n-1 )
    return cmCsvInsertSymText(h,buf);
  
  _cmCsvError(_cmCsvHandleToPtr(h),kDataCvtErrCsvRC,"The double %f could not be converted to text.",v);
  return cmInvalidId;

}

cmCsvRC_t  cmCsvSetCellText(   cmCsvH_t h, unsigned row, unsigned col, const char* text )
{
  cmCsvCell_t* cp;
  unsigned     symId;

  if((cp = _cmCsvCellPtr(h,row,col)) == NULL )
    return cmErrLastRC(&_cmCsvHandleToPtr(h)->err);

  if((symId = cmCsvInsertSymText(h,text)) == cmInvalidId )
    return cmErrLastRC(&_cmCsvHandleToPtr(h)->err);
  
  cp->symId = symId;
  cp->flags &= !kTypeTMask;
  cp->flags |= kStrCsvTFl;

  return kOkCsvRC;  
}

cmCsvRC_t  cmCsvSetCellInt(    cmCsvH_t h, unsigned row, unsigned col, int v )
{
  cmCsvCell_t* cp;
  unsigned     symId;

  if((cp = _cmCsvCellPtr(h,row,col)) == NULL )
    return cmErrLastRC(&_cmCsvHandleToPtr(h)->err);

  if((symId = cmCsvInsertSymInt(h,v)) == cmInvalidId )
    return cmErrLastRC(&_cmCsvHandleToPtr(h)->err);
  
  cp->symId = symId;
  cp->flags &= !kTypeTMask;
  cp->flags |= kIntCsvTFl;

  return kOkCsvRC;  
}

cmCsvRC_t  cmCsvSetCellUInt(   cmCsvH_t h, unsigned row, unsigned col, unsigned v )
{
  cmCsvCell_t* cp;
  unsigned     symId;

  if((cp = _cmCsvCellPtr(h,row,col)) == NULL )
    return cmErrLastRC(&_cmCsvHandleToPtr(h)->err);

  if((symId = cmCsvInsertSymUInt(h,v)) == cmInvalidId )
    return cmErrLastRC(&_cmCsvHandleToPtr(h)->err);
  
  cp->symId = symId;
  cp->flags &= !kTypeTMask;
  cp->flags |= kIntCsvTFl;

  return kOkCsvRC;  
}

cmCsvRC_t  cmCsvSetCellFloat(  cmCsvH_t h, unsigned row, unsigned col, float v )
{
  cmCsvCell_t* cp;
  unsigned     symId;

  if((cp = _cmCsvCellPtr(h,row,col)) == NULL )
    return cmErrLastRC(&_cmCsvHandleToPtr(h)->err);

  if((symId = cmCsvInsertSymFloat(h,v)) == cmInvalidId )
    return cmErrLastRC(&_cmCsvHandleToPtr(h)->err);
  
  cp->symId = symId;
  cp->flags &= !kTypeTMask;
  cp->flags |= kRealCsvTFl;

  return kOkCsvRC;  
}

cmCsvRC_t  cmCsvSetCellDouble( cmCsvH_t h, unsigned row, unsigned col, double v )
{
  cmCsvCell_t* cp;
  unsigned     symId;

  if((cp = _cmCsvCellPtr(h,row,col)) == NULL )
    return cmErrLastRC(&_cmCsvHandleToPtr(h)->err);

  if((symId = cmCsvInsertSymDouble(h,v)) == cmInvalidId )
    return cmErrLastRC(&_cmCsvHandleToPtr(h)->err);
  
  cp->symId = symId;
  cp->flags &= !kTypeTMask;
  cp->flags |= kRealCsvTFl;

  return kOkCsvRC;  
}


cmCsvRC_t cmCsvInsertRowBefore(  cmCsvH_t h, unsigned row, cmCsvCell_t** cellPtrPtr, unsigned symId, unsigned flags, unsigned lexTId )
{
  cmCsvRC_t    rc  = kOkCsvRC;
  cmCsv_t*     p   = _cmCsvHandleToPtr(h);
  cmCsvBind_t* bp  = p->bindPtr;
  cmCsvBind_t* nbp = NULL;
  unsigned     i   = 0;

  if( cellPtrPtr != NULL )
    *cellPtrPtr = NULL;

  // allocate the binder record
  if((nbp = cmLHeapAllocZ( p->heapH, sizeof(cmCsvBind_t))) == NULL )
    return _cmCsvError(p,kMemAllocErrCsvRC,"Binding record allocation failed row %i column %i.",row,0);

  // if a new first row is being inserted
  if( row == 0 )
  {
    bp           = p->bindPtr;
    nbp->linkPtr = p->bindPtr;
    p->bindPtr   = nbp;
  }
  else 
  {
    bp = p->bindPtr;

    // iterate to the row before the new row
    for(i=0; bp != NULL; ++i )
    {
      if( i == (row-1) || bp->linkPtr == NULL )
        break;

      bp = bp->linkPtr;
    }

    assert( bp != NULL );

    nbp->linkPtr = bp->linkPtr; 
    bp->linkPtr  = nbp;
    bp           = bp->linkPtr;
  }
  
  // update the row numbers in all cells below the new row
  while( bp != NULL )
  {
    cmCsvCell_t* cp = bp->rowPtr;
    while( cp != NULL )
    {
      cp->row += 1;
      cp = cp->rowPtr;
    }
    bp = bp->linkPtr;
  }

  // allocate the first cell in the new row
  if(cellPtrPtr != NULL && symId != cmInvalidId )
  {
    if((rc = _cmCsvAllocCell(p, symId, flags, row, 0, cellPtrPtr, lexTId )) != kOkCsvRC )
      return rc;
    
    nbp->rowPtr = *cellPtrPtr;
  }
  return rc;
}

cmCsvRC_t  cmCsvAppendRow( cmCsvH_t h, cmCsvCell_t** cellPtrPtr, unsigned symId, unsigned flags, unsigned lexTId )
{
  cmCsvRC_t    rc  = kOkCsvRC;
  cmCsv_t*     p   = _cmCsvHandleToPtr(h);
  cmCsvBind_t* nbp = NULL;
  unsigned     newRowIdx = cmInvalidIdx;

  if( cellPtrPtr != NULL )
    *cellPtrPtr = NULL;

  if((rc =  _cmCsvAppendNewBindRecd(p,&nbp,&newRowIdx)) != kOkCsvRC )
    return rc;

  
  // allocate the first cell in the new row
  if(cellPtrPtr != NULL && symId != cmInvalidId )
  {
    if((rc = _cmCsvAllocCell(p, symId, flags, newRowIdx, 0, cellPtrPtr, lexTId )) != kOkCsvRC )
      return rc;

    nbp->rowPtr = *cellPtrPtr;
  }
  
  return rc;
}

cmCsvRC_t cmCsvInsertColAfter(  cmCsvH_t h, cmCsvCell_t* leftCellPtr, cmCsvCell_t** cellPtrPtr, unsigned symId, unsigned flags, unsigned lexTId )
{
  cmCsvRC_t   rc   = kOkCsvRC;
  cmCsvCell_t* ncp = NULL;
  cmCsvCell_t* cp  = NULL;
  cmCsv_t*     p   = _cmCsvHandleToPtr(h);
  unsigned     col; 

  if(cellPtrPtr != NULL )
    *cellPtrPtr = NULL;

  // allocate the new cell
  if((rc =  _cmCsvAllocCell(p, symId, flags, leftCellPtr->row, leftCellPtr->col+1, &ncp, lexTId )) != kOkCsvRC )
    return rc;

  // update the col values of cells to right of new cell
  cp  = leftCellPtr->rowPtr;
  col = leftCellPtr->col + 1;

  for(; cp != NULL; ++col )
  {
    // don't update any col numbers after a blank (missing) column
    if( cp->col == col )
      cp->col += 1;

    cp = cp->rowPtr;
  }

  // link in the new cell
  ncp->rowPtr         = leftCellPtr->rowPtr;
  leftCellPtr->rowPtr = ncp;

  if(cellPtrPtr != NULL )
    *cellPtrPtr = ncp;
  
  return rc;
  
}

cmCsvRC_t  cmCsvInsertTextColAfter(   cmCsvH_t h, cmCsvCell_t* leftCellPtr, cmCsvCell_t** cellPtrPtr, const char* text, unsigned lexTId )
{
  cmCsvRC_t    rc;
  cmCsvCell_t* ncp;

  if( cellPtrPtr != NULL )
    cellPtrPtr = NULL;

  if((rc = cmCsvInsertColAfter(h, leftCellPtr, &ncp, cmInvalidId, 0, lexTId )) == kOkCsvRC )
    if((rc = cmCsvSetCellText(h, ncp->row, ncp->col, text )) == kOkCsvRC )
      if( cellPtrPtr != NULL )
        *cellPtrPtr = ncp;
  
  return rc;
}

cmCsvRC_t  cmCsvInsertIntColAfter(    cmCsvH_t h, cmCsvCell_t* leftCellPtr, cmCsvCell_t** cellPtrPtr, int val, unsigned lexTId )
{
  cmCsvRC_t    rc;
  cmCsvCell_t* ncp;

  if( cellPtrPtr != NULL )
    cellPtrPtr = NULL;

  if((rc = cmCsvInsertColAfter(h, leftCellPtr, &ncp, cmInvalidId, 0, lexTId )) == kOkCsvRC )
    if((rc = cmCsvSetCellInt(h, ncp->row, ncp->col, val )) == kOkCsvRC )
      if( cellPtrPtr != NULL )
        *cellPtrPtr = ncp;
  
  return rc;
}

cmCsvRC_t  cmCsvInsertUIntColAfter(   cmCsvH_t h, cmCsvCell_t* leftCellPtr, cmCsvCell_t** cellPtrPtr, unsigned val, unsigned lexTId )
{
  cmCsvRC_t    rc;
  cmCsvCell_t* ncp;

  if( cellPtrPtr != NULL )
    cellPtrPtr = NULL;

  if((rc = cmCsvInsertColAfter(h, leftCellPtr, &ncp, cmInvalidId, 0, lexTId )) == kOkCsvRC )
    if((rc = cmCsvSetCellUInt(h, ncp->row, ncp->col, val )) == kOkCsvRC )
      if( cellPtrPtr != NULL )
        *cellPtrPtr = ncp;
  
  return rc;
}

cmCsvRC_t  cmCsvInsertFloatColAfter(  cmCsvH_t h, cmCsvCell_t* leftCellPtr, cmCsvCell_t** cellPtrPtr, float val, unsigned lexTId )
{
  cmCsvRC_t    rc;
  cmCsvCell_t* ncp;

  if( cellPtrPtr != NULL )
    cellPtrPtr = NULL;

  if((rc = cmCsvInsertColAfter(h, leftCellPtr, &ncp, cmInvalidId, 0, lexTId )) == kOkCsvRC )
    if((rc = cmCsvSetCellFloat(h, ncp->row, ncp->col, val )) == kOkCsvRC )
      if( cellPtrPtr != NULL )
        *cellPtrPtr = ncp;
  
  return rc;
}

cmCsvRC_t  cmCsvInsertDoubleColAfter( cmCsvH_t h, cmCsvCell_t* leftCellPtr, cmCsvCell_t** cellPtrPtr, double val, unsigned lexTId )
{
  cmCsvRC_t    rc;
  cmCsvCell_t* ncp;

  if( cellPtrPtr != NULL )
    cellPtrPtr = NULL;

  if((rc = cmCsvInsertColAfter(h, leftCellPtr, &ncp, cmInvalidId, 0, lexTId )) == kOkCsvRC )
    if((rc = cmCsvSetCellDouble(h, ncp->row, ncp->col, val )) == kOkCsvRC )
      if( cellPtrPtr != NULL )
        *cellPtrPtr = ncp;
  
  return rc;
}


cmCsvRC_t  cmCsvWrite( cmCsvH_t h, const char* fn )
{
  FILE*        fp = NULL;
  cmCsvRC_t    rc = kOkCsvRC;
  cmCsv_t*     p  = _cmCsvHandleToPtr(h);
  cmCsvBind_t* bp = p->bindPtr;

  if((fp = fopen(fn,"wt")) == NULL )
    return _cmCsvError(p,kFileCreateErrCsvRC,"Unable to create the output CSV file:'%s'.",fn);

  bp = p->bindPtr;

  // for each row
  while( bp != NULL )
  {
    cmCsvCell_t* cp  = bp->rowPtr;
    unsigned     col = 0;

    // for each column
    for(; cp != NULL; ++col )
    {
      // skip blank columns
      if( cp->col == col )
      {
        const char* tp;

        if((tp =  cmSymTblLabel(p->symTblH,cp->symId)) == NULL )
          return _cmCsvError(p,kSymTblErrCsvRC,"Unable to locate the symbol text for cell at row:%i col:%i.",cp->row,cp->col);

        if( cmIsFlag(cp->flags,kTextTMask) )
          fprintf(fp,"\"");

        fputs(tp,fp);

        if( cmIsFlag(cp->flags,kTextTMask) )
          fprintf(fp,"\"");

        cp = cp->rowPtr;
      }
      
      if( cp == NULL )
        fprintf(fp,"\n");  // end of row
      else 
        fprintf(fp,",");   // between columns
    }
    
    bp = bp->linkPtr;
  }

  fclose(fp);
  
  return rc;
}

cmCsvRC_t  cmCsvPrint( cmCsvH_t h, unsigned rowCnt )
{
  cmCsv_t*     p  = _cmCsvHandleToPtr(h);
  cmCsvBind_t* bp = p->bindPtr;
  unsigned     i;

  for(i=0; bp!=NULL && i<rowCnt; ++i,bp=bp->linkPtr)
  {
    cmCsvCell_t* cp = bp->rowPtr;
    unsigned     j;

    for(j=0; cp!=NULL; ++j)
    {
      if( cp->col == j )
      {
        const char* tp;

        if((tp =  cmSymTblLabel(p->symTblH,cp->symId)) == NULL )
          _cmCsvError(p,kSymTblErrCsvRC,"The text associated with the symbol '%i' was not found.",cp->symId);

        fputs(tp,stdin);
      }

      cp=cp->rowPtr;
      if( cp == NULL )
        printf("\n");
      else
        printf(",");

    }
  }
  return kOkCsvRC;
}
