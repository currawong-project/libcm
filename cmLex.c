#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmLex.h"
#include "cmErr.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmFile.h"

typedef struct
{
  unsigned        code;
  const cmChar_t* msg;
} cmLexErrorRecd;


cmLexErrorRecd cmLexErrorArray[] = 
{
  { kOkLexRC,              "No error. The operation completed successfully."},
  { kDuplicateTokenLexRC,  "The text or id passed as a user token is already in use by another token."},
  { kMissingCmtEndLexRC,   "The end of a block comment could not be found."}, 
  { kMissingEndQuoteLexRC, "The end of a quoted string could not be found."},
  { kNoMatchLexRC,         "The lexer encountered a string which could not be classified."},
  { kFileOpenErrLexRC,     "File open failed on cmLexSetFile()"},
  { kFileSeekErrLexRC,     "File seek failed on cmLexSetFile()"},
  { kFileTellErrLexRC,     "File tell failed on cmLexSetFile()"},
  { kFileReadErrLexRC,     "File read failed on cmLexSetFile()"},
  { kFileCloseErrLexRC,    "File close failed on cmLexSetFile()"},
  { kMemAllocErrLexRC,     "An attempted memory allocation failed"},
  { kEofRC,                "The end of the input text was encountered (this is a normal condition not an error)"},
  { kInvalidLexTIdLexRC,   "An invalid token id was encountered."},
  { kInvalidLexRC,         "Unknown lexer error code." }
};

struct cmLex_str;

typedef unsigned (*cmLexMatcherFuncPtr_t)( struct cmLex_str* p, const cmChar_t* cp, unsigned cn, const cmChar_t* keyStr );

// token match function record  
typedef struct
{
  unsigned              typeId;   // token type this matcher recognizes     
  cmLexMatcherFuncPtr_t funcPtr;  // recognizer function (only used if userPtr==NULL)
  cmChar_t*             tokenStr; // fixed string data used by the recognizer (only used if userPtr==NULL)
  cmLexUserMatcherPtr_t userPtr;  // user defined recognizer function (only used if funcPtr==NULL)
  bool                  enableFl; // true if this matcher is enabled
} cmLexMatcher;



typedef struct cmLex_str
{
  cmErr_t           err;
  const cmChar_t*   cp;              // character buffer
  unsigned          cn;              // count of characters in buffer
  unsigned          ci;              // current buffer index position
  unsigned          flags;           // lexer control flags

  unsigned          curTokenId;      // type id of the current token
  unsigned          curTokenCharIdx; // index into cp[] of the current token
  unsigned          curTokenCharCnt; // count of characters in the current token 
  unsigned          curLine;         // line number of the current token
  unsigned          curCol;          // column number of the current token

  unsigned          nextLine;
  unsigned          nextCol;

  cmChar_t*         blockBegCmtStr;
  cmChar_t*         blockEndCmtStr;
  cmChar_t*         lineCmtStr;

  cmLexMatcher*     mfp;             // base of matcher array   
  unsigned          mfi;             // next available matcher array slot
  unsigned          mfn;             // count of elementes in mfp[]

  cmChar_t*         textBuf;         // text buf used by cmLexSetFile()

} cmLex;


cmLexH cmLexNullH = { NULL };

bool _cmLexIsNewline( cmChar_t c )
{ return c == '\n'; }

bool _cmLexIsCommentTypeId( unsigned typeId )
{ return typeId == kBlockCmtLexTId || typeId == kLineCmtLexTId; }

cmLex* _cmLexHandleToPtr( cmLexH h )
{
  cmLex* p = (cmLex*)h.h;
  assert(p != NULL);
  return p;
};

cmRC_t _cmLexError( cmLex* p, unsigned rc, const char* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);

  unsigned bufCharCnt = 512;
  char buf[ bufCharCnt+1 ];
  snprintf(buf,bufCharCnt,"Error on line:%i ", p->curLine);

  unsigned sn = strlen(buf);
  vsnprintf(buf+sn,bufCharCnt-sn,fmt,vl);
  buf[bufCharCnt]=0;

  cmErrMsg(&p->err,rc,"%s",buf);

  va_end(vl);
  return rc;
}

unsigned _cmLexScanTo( const cmChar_t* cp, unsigned cn, const cmChar_t* keyStr )
{
  unsigned i = 0;
  unsigned n = strlen(keyStr);

  if( n <= cn )
    for(; i<=cn-n; ++i)
      if( strncmp(cp + i, keyStr, n ) == 0 )
        return i+n;

  return cmInvalidIdx;
    
}


unsigned _cmLexExactStringMatcher(   cmLex* p, const cmChar_t* cp, unsigned cn, const cmChar_t* keyStr )
{
  unsigned n = strlen(keyStr);
  return strncmp(keyStr,cp,n) == 0  ? n : 0;
}


unsigned _cmLexSpaceMatcher( cmLex* p, const cmChar_t* cp, unsigned cn, const cmChar_t* keyStr )
{
  unsigned i=0;
  for(; i<cn; ++i)
    if( !isspace(cp[i]) )
      break;
  return i;
}

unsigned _cmLexRealMatcher(  cmLex* p, const cmChar_t* cp, unsigned cn, const cmChar_t* keyStr )
{
  unsigned i  = 0;
  unsigned n  = 0;     // decimal point counter
  unsigned d  = 0;     // digit counter
  bool     fl = false; // true if this real includes an exponent

  for(; i<cn && n<=1; ++i)
  {
    if( i==0 && cp[i]=='-' )  // allow a leading '-'
      continue;

    if( isdigit(cp[i]) )      // allow digits 
    {
      ++d;
      continue;
    }

    if( cp[i] == '.'  && n==0 ) // allow exactly  one decimal point
      ++n;   
    else
      break;
  }

  // if there was at least one digit and the next char is an 'e'
  if( d>0 && i<cn && (cp[i] == 'e' || cp[i] == 'E') )
  {
    d=0;
    ++i;
    unsigned j = i;

    for(; i<cn; ++i)
    {
      if( i==j && cp[i]=='-' ) // allow the char following the e to be '-'
        continue;

      if( isdigit(cp[i]) )
      {
        ++d;
        continue;
      }

      // stop at the first non-digit
      break;
    }

    // an exp exists if digits follwed the 'e'
    fl = d > 0;
     
  }

  return i>1 && (n==1 || fl) ? i : 0;
}

unsigned _cmLexIntMatcher(   cmLex* p, const cmChar_t* cp, unsigned cn, const cmChar_t* keyStr )
{
  unsigned i = 0;
  bool signFl = false;
  for(; i<cn; ++i)
  {
    if( i==0 && cp[i]=='-' )
    {
      signFl = true;
      continue;
    }

    if( !isdigit(cp[i]) )
      break;
  }

  // BUG BUG BUG
  // If an integer is specified using 'e' notiation 
  // (see _cmLexRealMatcher()) and the number of exponent places
  // specified following the 'e' is positive and >= number of
  // digits following the decimal point (in effect zeros are
  // padded on the right side) then the value is an integer.
  //
  // The current implementation recognizes all numeric strings 
  // containing a decimal point as reals. 
 

  return signFl && i==1 ? 0 : i;
}

unsigned _cmLexHexMatcher(   cmLex* p, const cmChar_t* cp, unsigned cn, const cmChar_t* keyStr )
{
  unsigned i = 0;

  if( cn < 3 )
    return 0;

  if( cp[0]=='0' && cp[1]=='x')    
    for(i=2; i<cn; ++i)
      if( !isxdigit(cp[i]) )
        break;

  return i;
}


unsigned _cmLexIdentMatcher( cmLex* p, const cmChar_t* cp, unsigned cn, const cmChar_t* keyStr )
{
  unsigned i = 0;
  if( isalpha(cp[0]) || (cp[0]== '_'))
  {
    i = 1;
    for(; i<cn; ++i)
      if( !isalnum(cp[i]) && (cp[i] != '_') )
        break;
  }
  return i;
}


unsigned _cmLexQStrMatcher( cmLex* p, const cmChar_t* cp, unsigned cn, const cmChar_t* keyStr )
{
  cmChar_t qStr[]="\"";
  unsigned n = strlen(qStr);
  if( strncmp(qStr,cp,n) == 0 )
  {
    unsigned i;
    if((i = _cmLexScanTo(cp+n, cn-n, qStr)) == cmInvalidIdx )
    {
      _cmLexError( p, kMissingEndQuoteLexRC, "Missing string end quote.");
      return 0;
    }
    return n+i;
  }
  return 0;
}


unsigned _cmLexBlockCmtMatcher( cmLex* p, const cmChar_t* cp, unsigned cn, const cmChar_t* keyStr )
{  
  unsigned n = strlen(p->blockBegCmtStr);

  if( strncmp( p->blockBegCmtStr, cp, n ) == 0 )
  {
    unsigned i;
    if((i = _cmLexScanTo(cp + n, cn-n,p->blockEndCmtStr)) == cmInvalidIdx )
    {
      _cmLexError(p, kMissingCmtEndLexRC, "Missing end of block comment.");
      return 0;
    }

    return n + i;
  }
  return 0;
}

unsigned _cmLexLineCmtMatcher( cmLex* p, const cmChar_t* cp, unsigned cn, const cmChar_t* keyStr )
{
  unsigned n = strlen(p->lineCmtStr);
  if( strncmp( p->lineCmtStr, cp, n ) == 0)
  {
    unsigned i;
    const char newlineStr[] = "\n";
    if((i = _cmLexScanTo(cp + n, cn-n, newlineStr)) == cmInvalidIdx )
    {
      // no EOL was found so the comment must be on the last line of the source
      return cn;
    }  

    return n + i;
  }
  return 0;
}

cmRC_t  _cmLexInstallMatcher( cmLex* p, unsigned typeId, cmLexMatcherFuncPtr_t funcPtr, const cmChar_t* keyStr, cmLexUserMatcherPtr_t userPtr )
{
  assert( funcPtr==NULL || userPtr==NULL );
  assert( !(funcPtr==NULL && userPtr==NULL));

  // if there is no space in the user token array - then expand it
  if( p->mfi == p->mfn )
  {
    int incr_cnt = 10;
    cmLexMatcher* np = cmMemAllocZ( cmLexMatcher, p->mfn + incr_cnt );
    memcpy(np,p->mfp,p->mfi*sizeof(cmLexMatcher));
    cmMemPtrFree(&p->mfp);
    p->mfp = np;
    p->mfn += incr_cnt;
  }

  p->mfp[p->mfi].tokenStr = NULL;
  p->mfp[p->mfi].typeId   = typeId;
  p->mfp[p->mfi].funcPtr  = funcPtr;
  p->mfp[p->mfi].userPtr  = userPtr;
  p->mfp[p->mfi].enableFl = true;

  if( keyStr != NULL )
  {
    // allocate space for the token string and store it
    p->mfp[p->mfi].tokenStr = cmMemAlloc( cmChar_t, sizeof(cmChar_t) * (strlen(keyStr)+1) );
    strcpy(p->mfp[p->mfi].tokenStr, keyStr );
  }


  p->mfi++;
  return kOkLexRC;
}
cmRC_t _cmLexReset( cmLex* p )
{

  p->ci              = 0;

  p->curTokenId      = kErrorLexTId;
  p->curTokenCharIdx = cmInvalidIdx;
  p->curTokenCharCnt = 0;

  p->curLine         = 0;
  p->curCol          = 0;
  p->nextLine        = 0;
  p->nextCol         = 0;

  cmErrClearRC(&p->err);

  return kOkLexRC;
}

cmRC_t _cmLexSetTextBuffer( cmLex* p, const cmChar_t* cp, unsigned cn )
{
  p->cp = cp;
  p->cn = cn;

  return _cmLexReset(p);
}


cmLexH cmLexInit( const cmChar_t* cp, unsigned cn, unsigned flags, cmRpt_t* rpt )
{
  cmLexH       h;
  cmChar_t  dfltLineCmt[]     = "//";
  cmChar_t  dfltBlockBegCmt[] = "/*";
  cmChar_t  dfltBlockEndCmt[] = "*/";
  
  cmLex* p          = cmMemAllocZ( cmLex, 1 );

  cmErrSetup(&p->err,rpt,"Lexer");

  p->flags           = flags;
  
  _cmLexSetTextBuffer( p, cp, cn );

  /*
  p->cp              = (cn==0)    ? NULL : cp;
  p->cn              = (cp==NULL) ? 0    : cn;

  p->ci              = 0;


  p->curTokenId      = kErrorLexTId;
  p->curTokenCharIdx = cmInvalidIdx;
  p->curTokenCharCnt = 0;

  p->curLine         = 0;
  p->curCol          = 0;
  p->nextLine        = 0;
  p->nextCol         = 0;
  */

  int init_mfn       = 10;
  p->mfp             = cmMemAllocZ( cmLexMatcher, init_mfn );
  p->mfn             = init_mfn;
  p->mfi             = 0;


  p->lineCmtStr      = cmMemAlloc( cmChar_t, strlen(dfltLineCmt)+1 );
  strcpy( p->lineCmtStr, dfltLineCmt );

  p->blockBegCmtStr  = cmMemAlloc( cmChar_t, strlen(dfltBlockBegCmt)+1 );
  strcpy( p->blockBegCmtStr, dfltBlockBegCmt );

  p->blockEndCmtStr  = cmMemAlloc( cmChar_t, strlen(dfltBlockEndCmt)+1 );
  strcpy( p->blockEndCmtStr, dfltBlockEndCmt );


  _cmLexInstallMatcher( p, kSpaceLexTId,    _cmLexSpaceMatcher,    NULL, NULL );
  _cmLexInstallMatcher( p, kRealLexTId,     _cmLexRealMatcher,     NULL, NULL  );
  _cmLexInstallMatcher( p, kIntLexTId,      _cmLexIntMatcher,      NULL, NULL  );
  _cmLexInstallMatcher( p, kHexLexTId,      _cmLexHexMatcher,      NULL, NULL  );
  _cmLexInstallMatcher( p, kIdentLexTId,    _cmLexIdentMatcher,    NULL, NULL  );
  _cmLexInstallMatcher( p, kQStrLexTId,     _cmLexQStrMatcher,     NULL, NULL  );
  _cmLexInstallMatcher( p, kBlockCmtLexTId, _cmLexBlockCmtMatcher, NULL, NULL  );
  _cmLexInstallMatcher( p, kLineCmtLexTId,  _cmLexLineCmtMatcher,  NULL, NULL  );

  h.h = p;

  _cmLexReset(p);

  return h;
}

cmRC_t cmLexFinal( cmLexH* hp )
{
  if( hp == NULL || cmLexIsValid(*hp)==false )
    return cmOkRC;

  cmLex* p = _cmLexHandleToPtr(*hp);

  if( p != NULL )
  {

    if( p->mfp != NULL )
    {
      unsigned i = 0;

      // free the user token strings
      for(; i<p->mfi; ++i)
        if( p->mfp[i].tokenStr != NULL )
          cmMemPtrFree(&p->mfp[i].tokenStr);

      // free the matcher array
      cmMemPtrFree(&p->mfp);
      p->mfi = 0;
      p->mfn = 0;
    }

    cmMemPtrFree(&p->lineCmtStr);
    cmMemPtrFree(&p->blockBegCmtStr);
    cmMemPtrFree(&p->blockEndCmtStr);
    cmMemPtrFree(&p->textBuf);

    // free the lexer object
    cmMemPtrFree(&p);
    hp->h = NULL;
  }

  return kOkLexRC;
}

cmRC_t cmLexReset( cmLexH h )
{
  cmLex* p = _cmLexHandleToPtr(h);
  return _cmLexReset(p);
}


bool               cmLexIsValid( cmLexH h )
{ return h.h != NULL; }

cmRC_t             cmLexSetTextBuffer( cmLexH h, const cmChar_t* cp, unsigned cn )
{
  cmLex* p = _cmLexHandleToPtr(h);
  return _cmLexSetTextBuffer(p,cp,cn);
}

cmRC_t cmLexSetFile( cmLexH h, const cmChar_t* fn )
{
  cmRC_t    rc = kOkLexRC;
  cmFileH_t fh = cmFileNullHandle;
  cmLex*    p  = _cmLexHandleToPtr(h);
  long      n  = 0;

  assert( fn != NULL && p != NULL );

  // open the file
  if( cmFileOpen(&fh,fn,kReadFileFl,p->err.rpt) != kOkFileRC )
    return kFileOpenErrLexRC;

  // seek to the end of the file
  if( cmFileSeek(fh,kEndFileFl,0) != kOkFileRC )
    return kFileSeekErrLexRC;
  
  // get the length of the file
  if( cmFileTell(fh,&n) != kOkFileRC )
    return kFileTellErrLexRC;

  // rewind to the beginning of the file
  if( cmFileSeek(fh,kBeginFileFl,0) != kOkFileRC )
    return kFileSeekErrLexRC;

  // allocate the text buffer
  if((p->textBuf = cmMemResizeZ( char, p->textBuf, n+1)) == NULL )
  {
    rc = _cmLexError(p,kMemAllocErrLexRC,"Unable to allocate the text file buffer for:'%s'.",fn);
    goto errLabel;
  }

  // read the file into the buffer
  if( cmFileRead(fh,p->textBuf,n) != kOkFileRC )
    return kFileReadErrLexRC;

  if((rc = _cmLexSetTextBuffer( p, p->textBuf, n )) != kOkLexRC )
    goto errLabel;
  
 errLabel:
  // close the file
  if( cmFileClose(&fh) != kOkFileRC )
    return kFileCloseErrLexRC;

  return rc;
}

/*
cmRC_t cmLexSetFile( cmLexH h, const cmChar_t* fn )
{
  cmRC_t   rc      = kOkLexRC;
  FILE*    fp      = NULL;
  cmLex*   p       = _cmLexHandleToPtr(h);
  unsigned n       = 0;

  assert( fn != NULL && p != NULL );
  
  // open the file
  if((fp = fopen(fn,"rb")) == NULL )
    return _cmLexError(p,kFileOpenErrLexRC,"Unable to open the file:'%s'.",fn);

  // seek to the end
  if( fseek(fp,0,SEEK_END) != 0 )
  {
    rc= _cmLexError(p,kFileSeekErrLexRC,"Unable to seek to the end of '%s'.",fn);
    goto errLabel;
  }

  // get the length of the file
  if( (n=ftell(fp)) == 0 )
  {
    rc = _cmLexError(p,kFileOpenErrLexRC,"The file '%s' appears to be empty.",fn);
    goto errLabel;
  }

  // rewind the file
  if( fseek(fp,0,SEEK_SET) != 0 )
  {
    rc = _cmLexError(p,kFileSeekErrLexRC,"Unable to seek to the beginning of '%s'.",fn);
    goto errLabel;
  }

  // allocate the text buffer
  if((p->textBuf = cmMemResizeZ( char, p->textBuf, n+1)) == NULL )
  {
    rc = _cmLexError(p,kMemAllocErrLexRC,"Unable to allocate the text file buffer for:'%s'.",fn);
    goto errLabel;
  }

  // read the file into the text buffer
  if( fread(p->textBuf,n,1,fp) != 1 )
  {
    rc = _cmLexError(p,kFileReadErrLexRC,"File read failed on:'%s'.",fn);
    goto errLabel;
  }  

  if((rc = _cmLexSetTextBuffer( p, p->textBuf, n )) != kOkLexRC )
    goto errLabel;

 errLabel:

  // close the file
  if( fclose(fp) != 0 )
  {
    rc =  _cmLexError(p,kFileCloseErrLexRC,"File close failed on:'%s'.",fn);
    goto errLabel;
  }

  return rc;
}
*/

cmLexMatcher* _cmLexFindUserToken( cmLex* p, unsigned id, const cmChar_t* tokenStr )
{
  unsigned i = 0;
  for(; i<p->mfi; ++i)
  {
    if( id != cmInvalidId && p->mfp[i].typeId == id  )
      return p->mfp + i;

    if( p->mfp[i].tokenStr != NULL && tokenStr != NULL && strcmp(p->mfp[i].tokenStr,tokenStr)==0  )
      return p->mfp + i;

  }

  return NULL;
}


cmRC_t            cmLexRegisterToken( cmLexH h, unsigned id, const cmChar_t* tokenStr )
{
  cmLex* p = _cmLexHandleToPtr(h);

  // prevent duplicate tokens
  if( _cmLexFindUserToken( p, id, tokenStr ) != NULL )
    return _cmLexError( p, kDuplicateTokenLexRC, "id:%i token:%s duplicates the token string or id", id, tokenStr );


  return _cmLexInstallMatcher( p, id, _cmLexExactStringMatcher, tokenStr, NULL );
  

}

cmRC_t             cmLexRegisterMatcher( cmLexH h, unsigned id, cmLexUserMatcherPtr_t userPtr )
{
  cmLex* p = _cmLexHandleToPtr(h);

  // prevent duplicate tokens
  if( _cmLexFindUserToken( p, id, NULL ) != NULL )
    return _cmLexError( p, kDuplicateTokenLexRC, "A token matching function has already been installed for token id: %i", id );

  return _cmLexInstallMatcher( p, id, NULL, NULL, userPtr );
}

cmRC_t             cmLexEnableToken( cmLexH h, unsigned id, bool enableFl )
{
  cmLex* p = _cmLexHandleToPtr(h);

  unsigned mi = 0;
  for(; mi<p->mfi; ++mi)
    if( p->mfp[mi].typeId == id )
    {
      p->mfp[mi].enableFl = enableFl;
      return cmOkRC;
    }

  return _cmLexError( p, kInvalidLexTIdLexRC, "%i is not a valid token type id.",id);
}

unsigned           cmLexFilterFlags( cmLexH h )
{
  cmLex* p = _cmLexHandleToPtr(h);
  return p->flags;
}

void               cmLexSetFilterFlags( cmLexH h, unsigned flags )
{
  cmLex* p = _cmLexHandleToPtr(h);
  p->flags = flags;
}


unsigned           cmLexGetNextToken( cmLexH h )
{
  cmLex* p = _cmLexHandleToPtr(h);

  if( cmErrLastRC(&p->err) != kOkLexRC )
    return kErrorLexTId;

  while( p->ci < p->cn )
  {
    unsigned i;
    unsigned mi         = 0;
    unsigned maxCharCnt = 0;
    unsigned maxIdx     = cmInvalidIdx;

    p->curTokenId      = kErrorLexTId;
    p->curTokenCharIdx = cmInvalidIdx;
    p->curTokenCharCnt = 0;


    // try each mater
    for(; mi<p->mfi; ++mi)
      if( p->mfp[mi].enableFl )
      {
        unsigned charCnt = 0;
        if( p->mfp[mi].funcPtr != NULL )
          charCnt = p->mfp[mi].funcPtr(p, p->cp + p->ci, p->cn - p->ci, p->mfp[mi].tokenStr );
        else
          charCnt = p->mfp[mi].userPtr( p->cp + p->ci, p->cn - p->ci);

        if( cmErrLastRC(&p->err) != kOkLexRC )
          return kErrorLexTId;

        // if this matched token is longer then the prev. matched token or
        // if the prev matched token was an identifier and this matched token is an equal length user defined token
        if( (charCnt > maxCharCnt) 
          || (charCnt>0 && charCnt==maxCharCnt && p->mfp[maxIdx].typeId==kIdentLexTId && p->mfp[mi].typeId >=kUserLexTId ) 
          || (charCnt>0 && charCnt<maxCharCnt  && p->mfp[maxIdx].typeId==kIdentLexTId && p->mfp[mi].typeId >=kUserLexTId && cmIsFlag(p->flags,kUserDefPriorityLexFl))
            )
        {
          maxCharCnt = charCnt;
          maxIdx     = mi;
        }

      }

    // no token was matched
    if( maxIdx == cmInvalidIdx )
    {
      if( cmIsFlag(p->flags,kReturnUnknownLexFl) )
      {
        maxCharCnt = 1;
      }
      else
      {
        _cmLexError( p, kNoMatchLexRC, "Unable to recognize token:'%c'.",*(p->cp+p->ci));
        return kErrorLexTId;     
      }
    }

    // update the current line and column position    
    p->curLine = p->nextLine;
    p->curCol  = p->nextCol;
    

    // find the next column and line position
    for(i=0; i<maxCharCnt; ++i)
    {
      if( _cmLexIsNewline(p->cp[ p->ci + i ]) )
      {
        p->nextLine++;
        p->nextCol = 1;
      }
      else
        p->nextCol++;
    }

    bool returnFl = true;

    if( maxIdx != cmInvalidIdx )
    {
      // check the space token filter
      if( (p->mfp[ maxIdx ].typeId == kSpaceLexTId) && (cmIsFlag(p->flags,kReturnSpaceLexFl)==0) )
        returnFl = false;

      // check the comment token filter
      if( _cmLexIsCommentTypeId(p->mfp[ maxIdx ].typeId) && (cmIsFlag(p->flags,kReturnCommentsLexFl)==0) )
        returnFl = false;
    }

    // update the lexer state
    p->curTokenId      = maxIdx==cmInvalidIdx ? kUnknownLexTId : p->mfp[ maxIdx ].typeId;    
    p->curTokenCharIdx = p->ci;
    p->curTokenCharCnt = maxCharCnt;
      
    // advance the text buffer
    p->ci += maxCharCnt;

    if( returnFl )
      return p->curTokenId;
  }

  cmErrSetRC(&p->err,kEofRC);

  return kEofLexTId;

}

unsigned cmLexTokenId( cmLexH h )
{
  cmLex* p = _cmLexHandleToPtr(h);

  return p->curTokenId;
}

const cmChar_t* cmLexTokenText( cmLexH h )
{
  cmLex* p = _cmLexHandleToPtr(h);

  if( p->curTokenCharIdx == cmInvalidIdx )
    return NULL;

  unsigned n = p->curTokenId == kQStrLexTId ? 1 : 0;

  return p->cp + p->curTokenCharIdx + n;
}


unsigned           cmLexTokenCharCount(  cmLexH h )
{
  cmLex* p = _cmLexHandleToPtr(h);

  if( p->curTokenCharIdx == cmInvalidIdx )
    return 0;

  unsigned n = p->curTokenId == kQStrLexTId ? 2 : 0;

  return p->curTokenCharCnt - n;
}

int                cmLexTokenInt(        cmLexH h )
{  return strtol( cmLexTokenText(h),NULL,0 ); }

unsigned           cmLexTokenUInt(        cmLexH h )
{  return strtol( cmLexTokenText(h),NULL,0 ); }

float              cmLexTokenFloat(        cmLexH h )
{  return strtof( cmLexTokenText(h),NULL ); }

double             cmLexTokenDouble(        cmLexH h )
{  return strtod( cmLexTokenText(h),NULL ); }

unsigned cmLexCurrentLineNumber( cmLexH h )
{ 
  cmLex* p = _cmLexHandleToPtr(h);
  return p->curLine + 1;
}

unsigned cmLexCurrentColumnNumber( cmLexH h ) 
{
  cmLex* p = _cmLexHandleToPtr(h);
  return p->curCol + 1;
}

unsigned           cmLexErrorRC( cmLexH h )
{
  cmLex* p = _cmLexHandleToPtr(h);
  return cmErrLastRC(&p->err);
}

const cmChar_t* cmLexIdToLabel( cmLexH h, unsigned typeId )
{
  cmLex* p = _cmLexHandleToPtr(h);

  switch( typeId )
  {
    case kErrorLexTId:    return "<error>";
    case kEofLexTId:      return "<EOF>";
    case kSpaceLexTId:    return "<space>";
    case kRealLexTId:     return "<real>";
    case kIntLexTId:      return "<int>";
    case kHexLexTId:      return "<hex>";
    case kIdentLexTId:    return "<ident>";
    case kQStrLexTId:     return "<qstr>";
    case kBlockCmtLexTId: return "<bcmt>";
    case kLineCmtLexTId:  return "<lcmt>";
    default:
      {
        cmLexMatcher*  mp;
        if((mp = _cmLexFindUserToken(p,typeId,NULL)) == NULL )
          return "<unknown>";
        return mp->tokenStr;
      }
  }
  return "<invalid>";
}

const cmChar_t* cmLexRcToMsg( unsigned rc )
{
  unsigned i=0;
  for(i=0; cmLexErrorArray[i].code != kInvalidLexRC; ++i)
    if( cmLexErrorArray[i].code == rc )
      break;

  return cmLexErrorArray[i].msg;
}


//{ { label:cmLexEx }
//(
// cmLexTest() gives a simple cmLex example.
//)

//(
void cmLexTest( cmRpt_t* rpt)
{
  cmChar_t buf[] =
"123ident0\n 123.456\nident0\n"
"0xa12+.2\n"
"// comment \n"
"/* block \n"
"comment */"
"\"quoted string\""
"ident1"
"// last line comment";

  // initialize a lexer with a buffer of text
  cmLexH h = cmLexInit(buf,strlen(buf),
    kReturnSpaceLexFl | kReturnCommentsLexFl,rpt);

  // verify that the lexer initialization succeded.
  if( cmLexIsValid(h) == false )
  {
    cmRptPrintf(rpt,"Lexer initialization failed.");
    return;
  }

  // register some additional recoginizers
  cmLexRegisterToken(h,kUserLexTId+1,"+");
  cmLexRegisterToken(h,kUserLexTId+2,"-");

  unsigned tid;

  // ask for token id's 
  while( (tid = cmLexGetNextToken(h)) != kEofLexTId )
  {
    // print information about each token
    cmRptPrintf(rpt,"%i %i %s '%.*s' (%i) ", 
      cmLexCurrentLineNumber(h), 
      cmLexCurrentColumnNumber(h), 
      cmLexIdToLabel(h,tid), 
      cmLexTokenCharCount(h), 
      cmLexTokenText(h) , 
      cmLexTokenCharCount(h));

    // if the token is a number ...
    if( tid==kIntLexTId || tid==kRealLexTId || tid==kHexLexTId )
    {
      // ... then request the numbers value
      int    iv = cmLexTokenInt(h);
      double dv = cmLexTokenDouble(h);

      cmRptPrintf(rpt,"%i %f",iv,dv);
    }

    cmRptPrintf(rpt,"\n");

    // handle errors
    if( tid == kErrorLexTId )
    {
      cmRptPrintf(rpt,"Error:%i\n", cmLexErrorRC(h));
      break;
    }

  }

  // finalize the lexer 
  cmLexFinal(&h);

}

//)
//}
