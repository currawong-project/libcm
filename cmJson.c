#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmJson.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLex.h"
#include "cmLinkedHeap.h"
#include "cmFile.h"


enum
{
  kLCurlyLexTId = kUserLexTId+1,
  kRCurlyLexTId,
  kLHardLexTId,
  kRHardLexTId,
  kColonLexTId,
  kCommaLexTId,
  kTrueLexTId,
  kFalseLexTId,
  kNullLexTId
};


typedef struct
{
  unsigned    id;
  const char* text;
} cmJsToken_t;

// serialization buffer header
typedef struct
{
  unsigned id;       // always set to 'json'
  unsigned byteCnt;  // count of bytes following this field (total buf bytes = byteCnt + (2*sizeof(unsigned)))
  unsigned nodeCnt;  // count of nodes in this buffer
  unsigned version;  // buffer serialization version number
} cmJsSerialHdr_t;

// serialization helper record
typedef struct
{
  cmJsSerialHdr_t hdr;
  char*           basePtr;
  char*           nextPtr;
  char*           endPtr;  
} cmJsSerial_t;

// deserialization helper record
typedef struct 
{
  unsigned    nodeCnt;
  unsigned    nodeIdx;
  const char* nextPtr;
  const char* endPtr;

} cmJsDeserial_t;


typedef struct
{
  cmErr_t            err;            // 
  cmLexH             lexH;           // parsing lexer 
  cmLHeapH_t         heapH;          // linked heap stores all node memory 
  cmJsonNode_t*      rootPtr;        // root of internal node tree
  cmJsonNode_t*      basePtr;        // base of parsing stack
  cmJsonNode_t*      lastPtr;        // top of parsing stack
  cmJsRC_t           rc;             // last error code
  char*              serialBufPtr;   // serial buffer pointer
  unsigned           serialByteCnt;  // count of bytes in serialBuf[]
  bool               reportErrPosnFl;// report the file posn of syntax errors
} cmJs_t;

cmJsToken_t _cmJsTokenArray[] = 
{
  { kLCurlyLexTId, "{" },
  { kRCurlyLexTId, "}" },
  { kLHardLexTId,  "[" },
  { kRHardLexTId,  "]" },
  { kColonLexTId,  ":" },
  { kCommaLexTId,  "," },
  { kTrueLexTId,   "true"},
  { kFalseLexTId,  "false"},
  { kNullLexTId,   "null" },
  { kErrorLexTId,""}
  
};

cmJsToken_t _cmJsNodeTypeLabel[] = 
{
  { kObjectTId, "object" },
  { kPairTId,   "pair"   },
  { kArrayTId,  "array"  },
  { kStringTId, "string" },
  { kIntTId,    "int"    },
  { kRealTId,   "real"   },
  { kNullTId,   "null"   },
  { kTrueTId,   "true"   },
  { kFalseTId,  "false"  },
  { kInvalidTId,"invalid"}
};

cmJsonH_t cmJsonNullHandle = cmSTATIC_NULL_HANDLE;

cmJsRC_t _cmJsonRemoveNode( cmJs_t* p, cmJsonNode_t* np, bool freeFl, bool balancePairsFl );
void     _cmJsonFreeNode( cmJs_t* p, cmJsonNode_t* np);

const char* _cmJsonNodeTypeIdToLabel( unsigned nodeTypeId )
{
  unsigned i;
  for(i=0; _cmJsNodeTypeLabel[i].id != kInvalidTId; ++i)
    if( _cmJsNodeTypeLabel[i].id == nodeTypeId )
      break;
  return _cmJsNodeTypeLabel[i].text;
}

unsigned _cmJsonNodeTypeLabelToId( const char* typeLabel )
{
  unsigned i;
  for(i=0; _cmJsNodeTypeLabel[i].id != kInvalidTId; ++i)
    if( strcmp(_cmJsNodeTypeLabel[i].text,typeLabel) == 0 )
      break;

  return _cmJsNodeTypeLabel[i].id;
  
}

cmJs_t* _cmJsonHandleToPtr( cmJsonH_t h )
{
  cmJs_t* p = (cmJs_t*)h.h;
  assert(p != NULL);
  p->rc = kOkJsRC;
  return p;
}


cmJsRC_t _cmJsonError( cmJs_t* p, cmJsRC_t rc, const char* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  rc = cmErrVMsg(&p->err,rc,fmt,vl);
  va_end(vl);
  return rc;
}

cmJsRC_t _cmJsonSyntaxError( cmJs_t* p, const char* fmt, ... )
{
  int bn = 1024;
  char buf[bn+1];
  buf[0]=0;
  buf[bn]=0;

  va_list vl;
  va_start(vl,fmt);

  if( p->reportErrPosnFl )
    snprintf(buf,bn,"Syntax error on line:%i column:%i. ",cmLexCurrentLineNumber(p->lexH),cmLexCurrentColumnNumber(p->lexH));

  int n = strlen(buf);
  vsnprintf(buf+n,bn-n,fmt,vl);
  va_end(vl);

  return cmErrMsg(&p->err,kSyntaxErrJsRC,"%s",buf);
}


// Note that the stack is formed by a linked list 
// which is chained together using the nodes parentPtr.
// This works beacause the parentPtr for object,array,
// and pair nodes (the only nodes on the stack) is not
// needed as long as the node is on the stack. Once the
// node is popped the parentPtr is then set from the
// new stack top.
cmJsRC_t _cmJsonPushNode( cmJs_t* p, cmJsonNode_t* np )
{
  np->ownerPtr = p->lastPtr;
  p->lastPtr  = np;

  if( p->basePtr == NULL )
    p->basePtr = np;

  return kOkJsRC;
}

cmJsRC_t _cmJsonPopNode( cmJs_t* p )
{
  if( p->lastPtr == NULL )
    return _cmJsonSyntaxError(p,"A parser stack underlow signalled a syntax error.");

  cmJsonNode_t* t = p->lastPtr;

  // remove the top element
  p->lastPtr = p->lastPtr->ownerPtr;

  // set the parent of the popped node
  t->ownerPtr = p->lastPtr;

  if( p->lastPtr == NULL )
    p->basePtr = NULL;

  return kOkJsRC;
}



cmJsRC_t      cmJsonInitialize( cmJsonH_t* hp, cmCtx_t* ctx )
{
  cmJsRC_t rc;
  cmJs_t*  p;
  unsigned i;

  // finalize before initialize 
  if((rc = cmJsonFinalize(hp)) != kOkJsRC )
    return rc;

  // allocate the main object record
  if((p = cmMemAllocZ( cmJs_t, 1 )) == NULL )
    return cmErrMsg(&ctx->err,kMemAllocErrJsRC,"Object memory allocation failed.");

  cmErrSetup(&p->err,&ctx->rpt,"JSON Parser");

  // allocate the linked heap mgr
  if( cmLHeapIsValid(p->heapH = cmLHeapCreate(1024,ctx)) == false )
  {
    rc = _cmJsonError(p,kMemAllocErrJsRC,"Linked heap object allocation failed.");
    goto errLabel;
  }

  // allocate the lexer
  if(cmLexIsValid(p->lexH = cmLexInit(NULL,0,0,&ctx->rpt)) == false )
  {
    rc =  _cmJsonError(p,kLexErrJsRC,"Lex allocation failed.");
    goto errLabel;
  }

  // register json specific tokens with the lexer
  for(i=0; _cmJsTokenArray[i].id != kErrorLexTId; ++i)
  {
    cmRC_t lexRC;
    if( (lexRC = cmLexRegisterToken(p->lexH, _cmJsTokenArray[i].id, _cmJsTokenArray[i].text )) != kOkLexRC )
    {
      rc = _cmJsonError(p,kLexErrJsRC,"Lex token registration failed for:'%s'.\nLexer Error:%s",_cmJsTokenArray[i].text, cmLexRcToMsg(lexRC) );
      goto errLabel;
    }
  }

  hp->h = p;

  return kOkJsRC;
      
  errLabel:
  
  cmMemPtrFree(&p);

  if( cmLHeapIsValid(p->heapH) )
    cmLHeapDestroy(&p->heapH);

  if( cmLexIsValid(p->lexH) )
    cmLexFinal(&p->lexH);

  return rc;
}

cmJsRC_t      cmJsonInitializeFromFile( cmJsonH_t* hp, const char* fn, cmCtx_t* ctx )
{
  cmJsRC_t jsRC;

  if((jsRC = cmJsonInitialize(hp,ctx)) != kOkJsRC )
    return jsRC;

  if((jsRC = cmJsonParseFile(*hp,fn,NULL)) != kOkJsRC )
    cmJsonFinalize(hp);

  return jsRC;
}

cmJsRC_t      cmJsonInitializeFromBuf( cmJsonH_t* hp, cmCtx_t* ctx, const char* buf, unsigned bufByteCnt )
{
  cmJsRC_t jsRC;

  if((jsRC = cmJsonInitialize(hp,ctx)) != kOkJsRC )
    return jsRC;

  if((jsRC = cmJsonParse(*hp,buf,bufByteCnt,NULL)) != kOkJsRC )
    cmJsonFinalize(hp);

  return jsRC;
}

cmJsRC_t      cmJsonFinalize(   cmJsonH_t* hp )
{
  cmRC_t lexRC;

  if( hp == NULL || hp->h == NULL )
    return kOkJsRC;

  cmJs_t* p = _cmJsonHandleToPtr(*hp);
 
  // free the internal heap object
  cmLHeapDestroy( &p->heapH );

  // free the lexer
  if( cmLexIsValid(p->lexH) )
    if((lexRC = cmLexFinal(&p->lexH)) != kOkLexRC )
      return _cmJsonError(p,kLexErrJsRC,"Lexer finalization failed.\nLexer Error:%s",cmLexRcToMsg(lexRC));

  cmMemPtrFree(&p->serialBufPtr);

  // free the handle
  cmMemPtrFree(&hp->h);

  return kOkJsRC;
}



bool          cmJsonIsValid( cmJsonH_t h )
{ return h.h != NULL; }

cmJsRC_t _cmJsonLinkInChild( cmJs_t* p, cmJsonNode_t* parentPtr, cmJsonNode_t* np )
{
  cmJsRC_t rc = kOkJsRC;

  np->ownerPtr = parentPtr;

  switch( parentPtr->typeId )
  {
    case kObjectTId:
    case kArrayTId:
    case kPairTId:
      {
        // if the parent is an 'object' then the child must be a 'pair'
        if( parentPtr->typeId == kObjectTId && np->typeId != kPairTId )
          rc = _cmJsonSyntaxError(p,"Expect only 'pair' nodes as children of 'objects'.");

        // if the parent is a 'pair' then is may have a max of two children
        if( parentPtr->typeId == kPairTId && cmJsonChildCount(parentPtr) >= 2 )
          rc = _cmJsonSyntaxError(p,"'pair' nodes may only have 2 children.");

          
        // insert the new node into the parent child list

        // if the new node is the first child
        if( parentPtr->u.childPtr == NULL )
          parentPtr->u.childPtr = np;
        else
        {
          // if the new node is the second or greater child
          cmJsonNode_t* lp = parentPtr->u.childPtr;
          while( lp->siblingPtr != NULL )
            lp = lp->siblingPtr;
          
          lp->siblingPtr = np;
        }
      }
      break;



    default:
      rc = _cmJsonSyntaxError(p,"'%s' nodes cannot be parent nodes.",_cmJsonNodeTypeIdToLabel(parentPtr->typeId));
      break;
  }

  return rc;
}

// This function creates nodes it also:
// 1. inserts array elments into their parents child list
// 2. inserts pairs into their parents member list
// 3. assigns values to pairs
cmJsRC_t _cmJsonCreateNode( cmJs_t* p, cmJsonNode_t* parentPtr, unsigned newNodeTypeId, cmJsonNode_t** npp )
{
  cmJsRC_t rc = kOkJsRC;

  cmJsonNode_t* np;

  assert( npp != NULL );

  *npp = NULL;

  // allocate the new node
  if((np = cmLHeapAllocZ( p->heapH, sizeof(cmJsonNode_t) )) == NULL )
    return _cmJsonError(p,kMemAllocErrJsRC,"Error allocating node memory.");

  // set the new node type
  np->typeId  = newNodeTypeId;
  np->ownerPtr = parentPtr;

  if( parentPtr == NULL )
  {
    if( newNodeTypeId != kObjectTId && newNodeTypeId != kArrayTId )
      rc = _cmJsonSyntaxError(p,"'%s' nodes must have a parent node.",_cmJsonNodeTypeIdToLabel(newNodeTypeId));
  }
  else
  {
    // if the parent is an 'object', 'array', or 'pair' then the
    // new node must be a child - insert it into the parent child list
    if((rc = _cmJsonLinkInChild( p, parentPtr, np )) != kOkJsRC )
      return rc;

    switch( newNodeTypeId )
    {
      case kObjectTId:
      case kArrayTId:
        break;

      case kPairTId:
        if( parentPtr == NULL || parentPtr->typeId != kObjectTId )
          rc = _cmJsonSyntaxError(p,"'pair' nodes must be the child of an object 'node'.");
        break;

      default:
        if( parentPtr == NULL )
          rc = _cmJsonSyntaxError(p,"'%s' nodes must have parent nodes.",_cmJsonNodeTypeIdToLabel(newNodeTypeId));
    }
 
  }

  // assign the return value
  *npp = np;

  if( p->rootPtr == NULL)
  {
    if( np->typeId != kObjectTId && np->typeId != kArrayTId )
      rc = _cmJsonSyntaxError(p,"The root object must be an 'object' or 'array'.");

    p->rootPtr = np;
  }
  return rc;
}


cmJsRC_t _cmJsonCreateNumber( cmJs_t* p, cmJsonNode_t* parentPtr, unsigned nodeTId, cmJsonNode_t** npp )
{
  // numbers may only occurr as children of a 'pair' or element of an 'array'
  if( (parentPtr==NULL) || (parentPtr->typeId != kPairTId && parentPtr->typeId != kArrayTId) )
    return _cmJsonSyntaxError(p, "The parent of scalar:%*s is not a 'pair' or 'array'.", cmLexTokenCharCount(p->lexH), cmLexTokenText(p->lexH) ); 

  return _cmJsonCreateNode(p,parentPtr,nodeTId,npp);

}

cmJsRC_t _cmJsonCreateReal(cmJs_t* p, cmJsonNode_t* parentPtr,  double v, cmJsonNode_t** newNodePtrPtr )
{
  cmJsRC_t      rc;
  cmJsonNode_t* np = NULL;

  if( newNodePtrPtr != NULL )
    *newNodePtrPtr = NULL;

  if((rc= _cmJsonCreateNumber(p,parentPtr,kRealTId,&np)) == kOkJsRC )
  {
    np->u.realVal = v;

    if( newNodePtrPtr != NULL)
      *newNodePtrPtr = np;
  }
  return rc;  
}

cmJsRC_t _cmJsonCreateInt(cmJs_t* p, cmJsonNode_t* parentPtr,  int v, cmJsonNode_t** newNodePtrPtr )
{
  cmJsRC_t      rc;
  cmJsonNode_t* np = NULL;

  if( newNodePtrPtr != NULL )
    *newNodePtrPtr = NULL;

  if((rc= _cmJsonCreateNumber(p,parentPtr,kIntTId,&np)) == kOkJsRC )
  {
    np->u.intVal = v;

    if( newNodePtrPtr != NULL )
      *newNodePtrPtr = np;
  }

  return rc;  
}

cmJsRC_t _cmJsonCreateBool(cmJs_t* p, cmJsonNode_t* parentPtr,  bool fl, cmJsonNode_t** newNodePtrPtr )
{
  cmJsRC_t      rc;
  cmJsonNode_t* np = NULL;

  if( newNodePtrPtr != NULL )
    *newNodePtrPtr = NULL;
  

  if((rc= _cmJsonCreateNumber(p,parentPtr,fl?kTrueTId:kFalseTId,&np)) == kOkJsRC )
  {
    np->u.boolVal = fl;

    if( newNodePtrPtr != NULL )
      *newNodePtrPtr = np;
  }

  return rc;  
}

cmJsRC_t _cmJsonCreateNull(cmJs_t* p, cmJsonNode_t* parentPtr, cmJsonNode_t** newNodePtrPtr )
{
  cmJsRC_t rc;
  cmJsonNode_t* np = NULL;

  if( newNodePtrPtr != NULL )
    *newNodePtrPtr = NULL;
  
  if((rc = _cmJsonCreateNumber(p,parentPtr,kNullTId,&np)) == kOkJsRC )
  {
    if( newNodePtrPtr != NULL )
      *newNodePtrPtr = np;
  }
  return rc;
}



cmJsRC_t _cmJsonEscapeInt( cmJs_t* p, const char* sp, unsigned i, unsigned n, unsigned hexDigitCnt, char* rp )
{
  char buf[hexDigitCnt+1];

  memset(buf,0,hexDigitCnt+1);

  // fill buf[] with the next two characters
  unsigned j;
  for(j=0; i<n && j<hexDigitCnt; ++i,++j)
    buf[j] = sp[i];
    
  // be sure the buffer was filled - we must get exactly two characters
  if( i == n )
    return _cmJsonError(p,kInvalidHexEscapeJsRC,"An invalid hex escape code was encountered.");
 
  // do the text to int conversion
  errno = 0;
  long val = strtol(buf,NULL,16);

  // validate the conversion
  if( errno != 0 )
    return _cmJsonError(p,kInvalidHexEscapeJsRC,"Hex escape value conversion failed.");

  // convert the long to a character
  if( val > 0xff )
    return _cmJsonError(p,kInvalidHexEscapeJsRC,"Hex escape value is out of range (0x00-0xff).");
  
  *rp = (char)val;

  return kOkJsRC;
} 

cmJsRC_t _cmJsonEscapeString( cmJs_t* p, char* dp, const char* sp, unsigned n )
{
  cmJsRC_t rc;
  unsigned hexDigCnt = 2;  // count of digits in \u notation
  unsigned i,j;
  for(i=0,j=0; i<n; ++i,++j)
  {
    if( (sp[i] == '\\') && (i<(n-1) ))
    {
      switch( sp[i+1] )
      {
        case 'b': dp[j] = '\b'; break;
        case 'f': dp[j] = '\f'; break;
        case 'n': dp[j] = '\n'; break;
        case 'r': dp[j] = '\r'; break;
        case 't': dp[j] = '\t'; break;
        case 'u':
          {
            if((rc = _cmJsonEscapeInt( p, sp, i+2, n, hexDigCnt, dp+j )) != kOkJsRC )
              return rc;

            i += hexDigCnt; // skip hex digits

          }
          break;

        default:
          dp[j] = sp[i+1];
          break;
      }          
      ++i; // skip the escape character '\'
    }
    else
    {
      dp[j] = sp[i];
          
    }
  }

  if( j < n )
    dp[j] = 0;

  return kOkJsRC;
} 

// if 'cp' is NULL or 'cn' is 0 then the current string is dealloc'd 
// and the internal string value is left as NULL.
cmJsRC_t _cmJsonSetString( cmJs_t* p, cmJsonNode_t* np, const char* cp, unsigned cn )
{
  assert( np->typeId == kStringTId );

  cmJsRC_t      rc = kOkJsRC;

  // deallocate the current string
  if( np->u.stringVal != NULL )
  {
    cmLHeapFree(p->heapH,np->u.stringVal);
    np->u.stringVal = NULL;
  }

  if( cp != NULL && cn>0 )
  {

    // allocate the node data memory to hold the string
    if((np->u.stringVal = cmLHeapAllocZ(p->heapH,cn+1)) == NULL )
      return _cmJsonError(p,kMemAllocErrJsRC,"Unable to allocate stirng memory.");

    // copy the string into the node data memory
    if((rc = _cmJsonEscapeString(p, np->u.stringVal, cp, cn)) != kOkJsRC )
      return rc;


    np->u.stringVal[cn]=0;
  }
  return rc;
}

cmJsRC_t _cmJsonCreateString(cmJs_t* p, cmJsonNode_t* parentPtr,  const char* cp, unsigned cn, cmJsonNode_t** newNodePtrPtr )
{
  cmJsRC_t      rc;
  cmJsonNode_t* np = NULL;

  if( newNodePtrPtr != NULL )
    *newNodePtrPtr = NULL;
  
  // strings may only occurr as children of a 'pair' or element of an 'array'
  if( (parentPtr->typeId != kPairTId && parentPtr->typeId != kArrayTId ) )
    return _cmJsonSyntaxError(p, "The parent of string:%*s is not a 'pair', 'array', or 'object'.", cn,cp);

  // create the new node
  if((rc = _cmJsonCreateNode(p,parentPtr,kStringTId,&np)) != kOkJsRC )
    return rc;

  if((rc = _cmJsonSetString(p,np,cp,cn)) != kOkJsRC )
    return rc;

  /*
  // allocate the node data memory to hold the string
  if((np->u.stringVal = cmLHeapAllocZ(p->heapH,cn+1)) == NULL )
    return _cmJsonError(p,kMemAllocErrJsRC,"Unable to allocate stirng memory.");

  if((rc = _cmJsonEscapeString(p, np->u.stringVal, cp, cn)) != kOkJsRC )
    return rc;

  // copy the string into the node data memory
  //strncpy(np->u.stringVal,cp,cn);
  np->u.stringVal[cn]=0;
  */

  if( newNodePtrPtr != NULL )
    *newNodePtrPtr = np;


  return rc;  
}

cmJsRC_t _cmJsonCreatePair( cmJs_t* p, cmJsonNode_t* parentPtr, const char* label, cmJsonNode_t** newNodePtrPtr )
{
  cmJsRC_t rc;

  if((rc = _cmJsonCreateNode(p,parentPtr,kPairTId,newNodePtrPtr)) != kOkJsRC )
    return rc;

  return _cmJsonCreateString( p, *newNodePtrPtr, label, strlen(label), NULL);
}


// if fn == NULL then the buffer must contain the text to parse
cmJsRC_t _cmJsonParse(cmJsonH_t h, const char* buf, unsigned bufCharCnt, const cmChar_t* fn, cmJsonNode_t* altRootPtr )
{
  unsigned      lexTId = kErrorLexTId;
  cmJs_t*       p      = _cmJsonHandleToPtr(h);
  cmJsonNode_t* cnp    = altRootPtr == p->rootPtr ? altRootPtr : p->rootPtr;
  cmJsonNode_t* nnp    = NULL;
  cmJsRC_t      rc;

  p->reportErrPosnFl = true;

  // assign the text buffer and reset the lexer
  if( fn == NULL )
    rc = cmLexSetTextBuffer( p->lexH, buf, bufCharCnt );
  else
    rc = cmLexSetFile( p->lexH, fn );

  if( rc != kOkLexRC )
    return _cmJsonError( p, kLexErrJsRC, "Error setting lexer buffer.");
  
  // get the next token
  while( (lexTId = cmLexGetNextToken( p->lexH )) != kErrorLexTId && (lexTId != kEofLexTId ) && (rc==kOkJsRC) )
  {
    cnp = p->lastPtr;

    // if cnp is a pair and it's value has been assigned
    if( cnp != NULL && cnp->typeId == kPairTId && cmJsonChildCount(cnp)==2 )
    {
      if((rc = _cmJsonPopNode(p)) != kOkJsRC )
        break;

      cnp = p->lastPtr;
    }
    
    switch( lexTId )
    {
      case kRealLexTId:     // real number
        rc = _cmJsonCreateReal(p, cnp,  cmLexTokenDouble( p->lexH ), NULL);
        break;

      case kHexLexTId:      // hexidecimal integer
        // allow hex integers to be equivalent to decimal integers

      case kIntLexTId:      // decimal integer
        rc = _cmJsonCreateInt(p, cnp, cmLexTokenInt( p->lexH ), NULL );
        break;

      case kTrueLexTId:     // true
        rc = _cmJsonCreateBool(p, cnp, true, NULL );
        break;

      case kFalseLexTId:    // false
        rc = _cmJsonCreateBool(p, cnp, false, NULL );
        break;

      case kNullLexTId:     // null
        rc = _cmJsonCreateNull(p, cnp, NULL );
        break;

      case kIdentLexTId:    // identifier
        // allow identifiers to be equivalent to strings.

      case kQStrLexTId:     // quoted string
        if( cnp == NULL )
          rc = _cmJsonSyntaxError(p,"Encountered a 'string' with no parent.");
        else
          if( cnp->typeId == kObjectTId )
          {
            if((rc = _cmJsonCreateNode(p,cnp,kPairTId,&nnp)) == kOkJsRC )
            {
              _cmJsonPushNode(p,nnp);
              cnp = nnp;
            }

            //if((rc = _cmJsonCreateNewParent(p,cnp,kPairTId)) == kOkJsRC )
            //  cnp = p->lastPtr;
          }

        if( rc == kOkJsRC )
          rc = _cmJsonCreateString(p, cnp, cmLexTokenText(p->lexH), cmLexTokenCharCount(p->lexH), NULL);
        break;

      case kColonLexTId:
        if( cnp->typeId != kPairTId )
          rc = _cmJsonSyntaxError(p,"A colon was found outside of a 'pair' element.");
        break;

      case kLCurlyLexTId:   // {
        //rc = _cmJsonCreateNewParent(p, cnp, kObjectTId );
        if((rc = _cmJsonCreateNode(p,cnp,kObjectTId,&nnp)) == kOkJsRC )
          _cmJsonPushNode(p,nnp);
        break;

      case kRCurlyLexTId:   // }
        if( cnp == NULL || cnp->typeId != kObjectTId )
          rc = _cmJsonSyntaxError(p,"A '}' was found without an accompanying opening bracket.");
        else
          rc = _cmJsonPopNode(p);
        break;

      case kLHardLexTId:    // [
        //rc = _cmJsonCreateNewParent(p, cnp, kArrayTId);
        if((rc = _cmJsonCreateNode(p,cnp,kArrayTId,&nnp)) == kOkJsRC )
          _cmJsonPushNode(p,nnp);
        break;

      case kRHardLexTId:    // ]
        if( cnp == NULL || cnp->typeId != kArrayTId )
          rc = _cmJsonSyntaxError(p,"A ']' was found without an accompanying opening bracket.");
        else
          rc = _cmJsonPopNode(p);
        break;

      case kCommaLexTId:    // ,
        if( (cnp==NULL) || (cnp->typeId != kArrayTId && cnp->typeId != kObjectTId) )
          rc = _cmJsonSyntaxError(p,"Commas may only occur in 'array' and 'object' nodes.");

        break;


      case kSpaceLexTId:    // white space
      case kBlockCmtLexTId: //  block comment
      case kLineCmtLexTId:  // line comment 
        assert(0); 
        break;

      default:
        break;
    }
  }

  if( lexTId == kErrorLexTId )
    rc = _cmJsonSyntaxError( p, "The lexer failed: %s.", cmLexRcToMsg(cmLexErrorRC(p->lexH)));

  p->reportErrPosnFl = false;

  return rc;
}

cmJsRC_t      cmJsonParse(      cmJsonH_t h, const char* buf, unsigned bufCharCnt, cmJsonNode_t* altRootPtr )
{ return _cmJsonParse(h,buf,bufCharCnt,NULL,altRootPtr); }

cmJsRC_t      cmJsonParseFile(  cmJsonH_t h, const char* fn, cmJsonNode_t* altRootPtr )
{ return _cmJsonParse(h,NULL,0,fn,altRootPtr); }

/*
cmJsRC_t      cmJsonParseFile(  cmJsonH_t h, const char* fn )
{
  cmJsRC_t rc      = kOkJsRC;
  FILE*    fp      = NULL;
  cmJs_t*  p       = _cmJsonHandleToPtr(h);
  unsigned n       = 0;
  char*    textBuf = NULL;

  assert( fn != NULL && p != NULL );
  
  // open the file
  if((fp = fopen(fn,"rb")) == NULL )
    return _cmJsonError(p,kFileOpenErrJsRC,"Unable to open the file:'%s'.",fn);

  // seek to the end
  if( fseek(fp,0,SEEK_END) != 0 )
  {
    rc= _cmJsonError(p,kFileSeekErrJsRC,"Unable to seek to the end of '%s'.",fn);
    goto errLabel;
  }

  // get the length of the file
  if( (n=ftell(fp)) == 0 )
  {
    rc = _cmJsonError(p,kFileOpenErrJsRC,"The file '%s' appears to be empty.",fn);
    goto errLabel;
  }

  // rewind the file
  if( fseek(fp,0,SEEK_SET) != 0 )
  {
    rc = _cmJsonError(p,kFileSeekErrJsRC,"Unable to seek to the beginning of '%s'.",fn);
    goto errLabel;
  }

  // allocate the text buffer
  if((textBuf = cmMemAllocZ( char, n+1)) == NULL )
  {
    rc = _cmJsonError(p,kMemAllocErrJsRC,"Unable to allocate the text file buffer for:'%s'.",fn);
    goto errLabel;
  }

  // read the file into the text buffer
  if( fread(textBuf,n,1,fp) != 1 )
  {
    rc = _cmJsonError(p,kFileReadErrJsRC,"File read failed on:'%s'.",fn);
    goto errLabel;
  }  

  rc = cmJsonParse(h,textBuf,n,NULL); 

 errLabel:

  // close the file
  if( fclose(fp) != 0 )
  {
    rc =  _cmJsonError(p,kFileCloseErrJsRC,"File close failed on:'%s'.",fn);
    goto errLabel;
  }

  // free the buffer
  if( textBuf != NULL )
    cmMemFree(textBuf);

  return rc;
}
*/

cmJsonNode_t* cmJsonRoot(  cmJsonH_t h )
{  
  cmJs_t* p = _cmJsonHandleToPtr(h);
  return p->rootPtr;
}


cmJsRC_t      cmJsonClearTree(      cmJsonH_t h )
{
  cmJs_t* p = _cmJsonHandleToPtr(h);

  p->rootPtr = NULL;
  p->basePtr = NULL;
  p->lastPtr = NULL;
  
  cmLHeapClear(p->heapH,true);

  return kOkJsRC;
}

bool cmJsonIsObject( const cmJsonNode_t* np ) { return cmIsFlag(np->typeId,kObjectTId); }
bool cmJsonIsArray(  const cmJsonNode_t* np ) { return cmIsFlag(np->typeId,kArrayTId); }
bool cmJsonIsPair(   const cmJsonNode_t* np ) { return cmIsFlag(np->typeId,kPairTId); }
bool cmJsonIsString( const cmJsonNode_t* np ) { return cmIsFlag(np->typeId,kStringTId); }
bool cmJsonIsInt(    const cmJsonNode_t* np ) { return cmIsFlag(np->typeId,kIntTId); }
bool cmJsonIsReal(   const cmJsonNode_t* np ) { return cmIsFlag(np->typeId,kRealTId); }
bool cmJsonIsBool(   const cmJsonNode_t* np ) { return cmIsFlag(np->typeId,kTrueTId | kFalseTId); }



unsigned cmJsonChildCount( const cmJsonNode_t* np )
{
  if( np == NULL )
    return 0;

  unsigned n = 0;
  switch( np->typeId )
  {
    case kObjectTId:
    case kArrayTId:
    case kPairTId:
      {        
        const cmJsonNode_t* lp = np->u.childPtr;

        while( lp != NULL )
        {
          ++n;
          lp = lp->siblingPtr;
        }

      }
      break;

    default:
      break;
  }
  return n;
}

cmJsonNode_t* cmJsonArrayElement( cmJsonNode_t* np, unsigned index )
{
  unsigned i;

  assert( index < cmJsonChildCount(np));

  np = np->u.childPtr;

  for(i=0; i<index; ++i)
    np = np->siblingPtr;

  return np;
}

const cmJsonNode_t* cmJsonArrayElementC( const cmJsonNode_t* np, unsigned index )
{ return cmJsonArrayElement( (cmJsonNode_t*)np, index ); }

const char* cmJsonPairLabel(  const cmJsonNode_t* pairPtr )
{
  assert( pairPtr->typeId == kPairTId );

  if( pairPtr->typeId != kPairTId )
    return NULL;

  return pairPtr->u.childPtr->u.stringVal;
}

unsigned cmJsonPairTypeId( const cmJsonNode_t* pairPtr )
{
  assert( pairPtr->typeId == kPairTId );
  return pairPtr->u.childPtr->siblingPtr->typeId; 
}

cmJsonNode_t* cmJsonPairValue( cmJsonNode_t* pairPtr )
{
  assert( pairPtr->typeId == kPairTId );

  if( pairPtr->typeId != kPairTId )
    return NULL;

  return pairPtr->u.childPtr->siblingPtr;

}

cmJsonNode_t* cmJsonFindValue( cmJsonH_t h, const char* label, const cmJsonNode_t* np, unsigned keyTypeMask )
{
  cmJs_t* p = _cmJsonHandleToPtr(h);
  
  if( np == NULL )
    np = p->rootPtr;

  if( np == NULL )
    return NULL;

  // we are only interested in pairs
  if( np->typeId == kPairTId )
  {
    // pairs must have exactly two nodes - the first must be a string 
    assert(np->u.childPtr != NULL && np->u.childPtr->typeId == kStringTId && np->u.childPtr->siblingPtr != NULL );

    if( strcmp(cmJsonPairLabel(np),label) == 0 )
    {
      if( (keyTypeMask==kInvalidTId) || (keyTypeMask & np->u.childPtr->siblingPtr->typeId) )
        return np->u.childPtr->siblingPtr;
    }
  }

  // if the node is an object,array, or pair ...
  if( np->typeId==kObjectTId || np->typeId==kArrayTId || np->typeId==kPairTId )
  {
    // ... then recurse on its children
    cmJsonNode_t* cnp = np->u.childPtr;
    while(cnp != NULL)
    {
      cmJsonNode_t* rp;

      if((rp = cmJsonFindValue(h,label,cnp,keyTypeMask)) != NULL )
        return rp;

      cnp = cnp->siblingPtr;
    }
  }

  return NULL;
}


cmJsRC_t  _cmJsonFindPathValue( cmJs_t* p, const char* pathPrefix, const char* path, const cmJsonNode_t* rp, const cmJsonNode_t** rpp )
{
  cmJsRC_t rc = kOkJsRC;

  if( rp == NULL )
    rp = p->rootPtr;

  if( rp == NULL )
    return kOkJsRC;  

  assert( cmJsonIsObject(rp));
  assert( rpp != NULL );
  
  *rpp = NULL;

  // create a copy of the path
  unsigned i,j;
  unsigned sn = (pathPrefix==NULL ? 0 : strlen(pathPrefix))  + strlen(path) + 1; // add one for the possible extra seperator
  char ss[ 1024 ];
  char* sp = NULL;
  char* sm = NULL;
  char* sb = ss;
  
  // don't put more than 1k on the stack
  if( sn + 1 > 1024 )
  {
    sm = cmMemAllocZ(char,sn+1);
    sb = sm;
  }

  sp  = sb;

  sp[0] = 0;
  if(pathPrefix != NULL )
  {
    strcpy(sp,pathPrefix);

    // if pathPrefix does not end in  a '/' then insert one
    if( sp[ strlen(sp)-1 ] != '/' )
      strcat(sp,"/");
  }

  // the '/' has already been inserted - skip any leading '/' character in path
  strcat(sp,path[0]=='/' ? path+1 : path );

  // terminate each path label with a '/0'.
  sp = ss;
  for(i=0,j=0; sp < ss + sn; ++sp, ++i )
    if( *sp == '/' )
    {
      *sp = 0;
      ++j;
    }

  if( i > 0 )
  {

    sp = sb;

    while( sp < sb + sn )
    {
      
      // labeled values are always associated with pairs and
      // pairs only exist as the children of objects.
      if( cmJsonIsObject(rp) == false )
      {
        rc = cmErrMsg(&p->err,kInvalidNodeTypeJsRC,"A non-object node was encountered on a object path.");
        break;
      }

      // get the count of pairs in this object
      unsigned      cn = cmJsonChildCount( rp );
      cmJsonNode_t* cp = rp->u.childPtr;  
      unsigned       k;

      for(k=0; k<cn; ++k)
      {
        // all children of an object must be pairs
        assert( cp != NULL && cmJsonIsPair(cp) );

        // if this is the labeled pair we are looking for
        if( strcmp(cmJsonPairLabel(cp),sp) == 0 )
          break;        

        cp = cp->siblingPtr;
      }

      // if the search failed
      if( k == cn || cp == NULL )
      {
        rc = cmErrMsg(&p->err,kNodeNotFoundJsRC,"The path label '%s' could not be found.",cmStringNullGuard(sp));
        break;
      }

      // take the value of the located pair to continue the search
      rp = cmJsonPairValue(cp);

      // advance to the next label
      sp += strlen(sp) + 1;

    }

    
  }

  cmMemPtrFree(&sm);

  *rpp = rp;

  return rc;
}

cmJsRC_t _cmJsonPathToValueError( cmJs_t* p, cmJsRC_t rc, const char* pathPrefix, const char* path, const char* typeLabel )
{
  if( pathPrefix != NULL )
    cmErrMsg(&p->err,rc,"The JSON value at '%s/%s' could not be converted to a '%s'.",cmStringNullGuard(pathPrefix),cmStringNullGuard(path),cmStringNullGuard(typeLabel));
  else
    cmErrMsg(&p->err,rc,"The JSON value at '%s' could not be converted to a '%s'.",cmStringNullGuard(path),cmStringNullGuard(typeLabel));

  return rc;
}

cmJsRC_t cmJsonPathToValueNode( cmJsonH_t h, const cmJsonNode_t* objectNodePtr, const char* pathPrefix, const char* path, const cmJsonNode_t** nodePtrPtr )
{
  cmJsRC_t rc = kOkJsRC;
  cmJs_t*  p  = _cmJsonHandleToPtr(h);

  if( objectNodePtr == NULL )
    objectNodePtr = p->rootPtr;

  if((rc = _cmJsonFindPathValue(p,pathPrefix,path,objectNodePtr,nodePtrPtr)) != kOkJsRC )
    return rc;
  
  return rc;
}

cmJsRC_t cmJsonPathToBool(   cmJsonH_t h, const cmJsonNode_t* objectNodePtr, const char* pathPrefix, const char* path, bool* retValPtr )
{
  const cmJsonNode_t* rp;
  cmJsRC_t            rc = kOkJsRC;
  cmJs_t*             p  = _cmJsonHandleToPtr(h);

  if((rc = _cmJsonFindPathValue(p,pathPrefix,path,objectNodePtr,&rp)) != kOkJsRC )
    return rc;

  if((rc = cmJsonBoolValue(rp,retValPtr)) != kOkJsRC )
    return _cmJsonPathToValueError(p,rc,pathPrefix,path,"bool");

  return rc;
}

cmJsRC_t cmJsonPathToInt(    cmJsonH_t h, const cmJsonNode_t* objectNodePtr, const char* pathPrefix, const char* path, int* retValPtr )
{
  const cmJsonNode_t* rp;
  cmJsRC_t            rc = kOkJsRC;
  cmJs_t*             p  = _cmJsonHandleToPtr(h);

  if((rc = _cmJsonFindPathValue(p,pathPrefix,path,objectNodePtr,&rp)) != kOkJsRC )
    return rc;

  if((rc = cmJsonIntValue(rp,retValPtr)) != kOkJsRC )
    return _cmJsonPathToValueError(p,rc,pathPrefix,path,"bool");

  return rc;
}

cmJsRC_t cmJsonPathToUInt(   cmJsonH_t h, const cmJsonNode_t* objectNodePtr, const char* pathPrefix, const char* path, unsigned* retValPtr )
{
  const cmJsonNode_t* rp;
  cmJsRC_t            rc = kOkJsRC;
  cmJs_t*             p  = _cmJsonHandleToPtr(h);

  if((rc = _cmJsonFindPathValue(p,pathPrefix,path,objectNodePtr,&rp)) != kOkJsRC )
    return rc;

  if((rc = cmJsonUIntValue(rp,retValPtr)) != kOkJsRC )
    return _cmJsonPathToValueError(p,rc,pathPrefix,path,"unsigned integer");

  return rc;
}

cmJsRC_t cmJsonPathToReal(   cmJsonH_t h, const cmJsonNode_t* objectNodePtr, const char* pathPrefix, const char* path, double* retValPtr )
{
  const cmJsonNode_t* rp;
  cmJsRC_t            rc = kOkJsRC;
  cmJs_t*             p  = _cmJsonHandleToPtr(h);

  if((rc = _cmJsonFindPathValue(p,pathPrefix,path,objectNodePtr,&rp)) != kOkJsRC )
    return rc;

  if((rc = cmJsonRealValue(rp,retValPtr)) != kOkJsRC )
    return _cmJsonPathToValueError(p,rc,pathPrefix,path,"real");

  return rc;
}

cmJsRC_t cmJsonPathToString( cmJsonH_t h, const cmJsonNode_t* objectNodePtr, const char* pathPrefix, const char* path, const char** retValPtr )
{
  const cmJsonNode_t* rp = NULL;
  cmJsRC_t            rc = kOkJsRC;
  cmJs_t*             p  = _cmJsonHandleToPtr(h);

  if((rc = _cmJsonFindPathValue(p,pathPrefix,path,objectNodePtr,&rp)) != kOkJsRC )
    return rc;

  if((rc = cmJsonStringValue(rp,retValPtr)) != kOkJsRC )
    return _cmJsonPathToValueError(p,rc,pathPrefix,path,"string");

  return rc;
}

cmJsRC_t cmJsonPathToPair(  cmJsonH_t h, const cmJsonNode_t* objectNodePtr, const char* pathPrefix, const char* path, cmJsonNode_t** retValPtr )
{
  const cmJsonNode_t* rp;
  cmJsRC_t            rc = kOkJsRC;
  cmJs_t*             p  = _cmJsonHandleToPtr(h);

  if((rc = _cmJsonFindPathValue(p,pathPrefix,path,objectNodePtr,&rp)) != kOkJsRC )
    return rc;

  if((rc = cmJsonPairNode(rp,retValPtr)) != kOkJsRC )
    return _cmJsonPathToValueError(p,rc,pathPrefix,path,"pair");

  return rc;
}

cmJsRC_t cmJsonPathToArray(  cmJsonH_t h, const cmJsonNode_t* objectNodePtr, const char* pathPrefix, const char* path, cmJsonNode_t** retValPtr )
{
  const cmJsonNode_t* rp;
  cmJsRC_t            rc = kOkJsRC;
  cmJs_t*             p  = _cmJsonHandleToPtr(h);

  if((rc = _cmJsonFindPathValue(p,pathPrefix,path,objectNodePtr,&rp)) != kOkJsRC )
    return rc;

  if((rc = cmJsonArrayNode(rp,retValPtr)) != kOkJsRC )
    return _cmJsonPathToValueError(p,rc,pathPrefix,path,"array");

  return rc;
}

cmJsRC_t cmJsonPathToObject( cmJsonH_t h, const cmJsonNode_t* objectNodePtr, const char* pathPrefix, const char* path, cmJsonNode_t** retValPtr )
{
  const cmJsonNode_t* rp;
  cmJsRC_t            rc = kOkJsRC;
  cmJs_t*             p  = _cmJsonHandleToPtr(h);

  if((rc = _cmJsonFindPathValue(p,pathPrefix,path,objectNodePtr,&rp)) != kOkJsRC )
    return rc;

  if((rc = cmJsonObjectNode(rp,retValPtr)) != kOkJsRC )
    return _cmJsonPathToValueError(p,rc,pathPrefix,path,"object");

  return rc;
}


const cmJsonNode_t*  cmJsonFindPathValueC( cmJsonH_t h, const char* path, const cmJsonNode_t* np, unsigned typeIdMask )
{
  cmJs_t*             p  = _cmJsonHandleToPtr(h);
  const cmJsonNode_t* rp = NULL;
  cmJsRC_t            rc = kOkJsRC;
  
  if((rc = _cmJsonFindPathValue(p,NULL,path,np,&rp)) != kOkJsRC )
  {
    // validate the return type
    if( rp != NULL )
      if( (typeIdMask!=kInvalidTId) && (typeIdMask & rp->typeId)==0 )
      {
        cmErrMsg(&p->err,kInvalidNodeTypeJsRC,"The value at the end of the path '%s' did not match the requested type.",cmStringNullGuard(path));
        rp = NULL;
      }
  }

  return rp;
}

cmJsonNode_t*  cmJsonFindPathValue( cmJsonH_t h, const char* path, const cmJsonNode_t* np, unsigned typeIdMask )
{ return (cmJsonNode_t*)cmJsonFindPathValueC(h,path,np,typeIdMask ); }

cmJsRC_t  _cmJsonFindMemberValue( const cmJsonNode_t* np, const char* label, unsigned keyTypeId, cmJsonNode_t** npp )
{
  *npp = NULL;

  // the src node must be an object
  if( np->typeId != kObjectTId )
    return kNodeNotFoundJsRC;

  // for each member pair
  const cmJsonNode_t* cnp = np->u.childPtr;
  while( cnp != NULL )
  {
    assert( (cnp->typeId & kMaskTId) == kPairTId );

    // if the labels match ...
    if( strcmp( label, cmJsonPairLabel(cnp)) == 0 )
    {
      // ... and the type flags match ...
      if( (keyTypeId==kInvalidTId || cmIsFlag(cnp->u.childPtr->siblingPtr->typeId,keyTypeId) ) )
      {
        *npp = cnp->u.childPtr->siblingPtr;
        return kOkJsRC; // ... then the key was found.
      }

      // ... label match but wrong type ... this is considered an error
      return kNodeCannotCvtJsRC;
    }
  
    cnp = cnp->siblingPtr; 
  }

  return kNodeNotFoundJsRC;
}

cmJsRC_t   cmJsonUIntValue( const cmJsonNode_t* vp, unsigned* retPtr )
{
  cmJsRC_t rc = kOkJsRC;

  if( vp == NULL )
    return kNodeCannotCvtJsRC;

  switch(vp->typeId)
  {
    case kIntTId:    *retPtr = vp->u.intVal;       break;
    case kRealTId:   *retPtr = (unsigned)vp->u.realVal; break;
    case kTrueTId:   *retPtr = 1;                  break;
    case kFalseTId:  *retPtr = 0;                  break;
    default:
      rc = kNodeCannotCvtJsRC;
  }
  return rc;
}

cmJsRC_t   cmJsonIntValue( const cmJsonNode_t* vp, int* retPtr )
{
  cmJsRC_t rc = kOkJsRC;

  if( vp == NULL )
    return kNodeCannotCvtJsRC;

  switch(vp->typeId)
  {
    case kIntTId:    *retPtr = vp->u.intVal;       break;
    case kRealTId:   *retPtr = (int)vp->u.realVal; break;
    case kTrueTId:   *retPtr = 1;                  break;
    case kFalseTId:  *retPtr = 0;                  break;
    default:
      rc = kNodeCannotCvtJsRC;
  }
  return rc;
}

cmJsRC_t   cmJsonRealValue(   const cmJsonNode_t* vp, double*      retPtr )
{
  cmJsRC_t rc = kOkJsRC;

  if( vp == NULL )
    return kNodeCannotCvtJsRC;

  switch(vp->typeId)
  {
    case kIntTId:    *retPtr = vp->u.intVal;  break;
    case kRealTId:   *retPtr = vp->u.realVal; break;
    case kTrueTId:   *retPtr = 1;             break;
    case kFalseTId:  *retPtr = 0;             break;
    default:
      rc = kNodeCannotCvtJsRC;

  }
  return rc;

}

cmJsRC_t   cmJsonBoolValue(   const cmJsonNode_t* vp, bool*        retPtr )
{
  cmJsRC_t rc = kOkJsRC;

  if( vp == NULL )
    return kNodeCannotCvtJsRC;

  switch(vp->typeId)
  {
    case kIntTId:    *retPtr = vp->u.intVal != 0;       break;
    case kRealTId:   *retPtr = (int)vp->u.realVal != 0; break;
    case kTrueTId:   *retPtr = 1;                       break;
    case kFalseTId:  *retPtr = 0;                       break;
    default:
      rc = kNodeCannotCvtJsRC;

  }
  return rc;

}

cmJsRC_t   cmJsonStringValue( const cmJsonNode_t* vp, const char **retPtrPtr )  
{
  cmJsRC_t rc = kOkJsRC;
  
  if( vp == NULL || vp->typeId != kStringTId )
    return kNodeCannotCvtJsRC;

  *retPtrPtr = vp->u.stringVal;

  return rc;
}

cmJsRC_t   cmJsonPairNode( const cmJsonNode_t* vp, cmJsonNode_t **retPtrPtr )  
{
  cmJsRC_t rc = kOkJsRC;

  if( !cmJsonIsPair(vp) )
    return kNodeCannotCvtJsRC;

  *retPtrPtr = (cmJsonNode_t*)vp;

  return rc;
}

cmJsRC_t   cmJsonArrayNode( const cmJsonNode_t* vp, cmJsonNode_t **retPtrPtr )  
{
  cmJsRC_t rc = kOkJsRC;

  if( !cmJsonIsArray(vp) )
    return kNodeCannotCvtJsRC;

  *retPtrPtr = (cmJsonNode_t*)vp;

  return rc;
}

cmJsRC_t   cmJsonObjectNode( const cmJsonNode_t* vp, cmJsonNode_t **retPtrPtr )  
{
  cmJsRC_t rc = kOkJsRC;

  if( !cmJsonIsObject(vp) )
    return kNodeCannotCvtJsRC;

  *retPtrPtr = (cmJsonNode_t*)vp;

  return rc;
}

cmJsRC_t      cmJsonUIntMember(    const cmJsonNode_t* np, const char* label, unsigned* retPtr )
{ 
  cmJsonNode_t* vp;
  cmJsRC_t            rc;

  if((rc = _cmJsonFindMemberValue(np,label,kNumericTId,&vp)) != kOkJsRC )
    return rc;

  return cmJsonUIntValue(vp,retPtr);
}
 
cmJsRC_t      cmJsonIntMember(    const cmJsonNode_t* np, const char* label, int* retPtr )
{ 
  cmJsonNode_t* vp;
  cmJsRC_t            rc;

  if((rc = _cmJsonFindMemberValue(np,label,kNumericTId,&vp)) != kOkJsRC )
    return rc;

  return cmJsonIntValue(vp,retPtr);
}

cmJsRC_t      cmJsonRealMember(   const cmJsonNode_t* np, const char* label, double* retPtr )
{ 
  cmJsonNode_t* vp;
  cmJsRC_t            rc;

  if((rc = _cmJsonFindMemberValue(np,label,kNumericTId,&vp)) != kOkJsRC )
    return rc;

  return cmJsonRealValue(vp,retPtr);
}

cmJsRC_t      cmJsonBoolMember(   const cmJsonNode_t* np, const char* label, bool* retPtr )
{ 
  cmJsonNode_t* vp;
  cmJsRC_t            rc;

  if((rc = _cmJsonFindMemberValue(np,label,kNumericTId,&vp)) != kOkJsRC )
    return rc;

  return cmJsonBoolValue(vp,retPtr);
  
}

cmJsRC_t      cmJsonStringMember( const cmJsonNode_t* np, const char* label, const char** retPtrPtr )
{ 
  cmJsonNode_t* vp;
  cmJsRC_t            rc;

  *retPtrPtr = NULL;

  if((rc = _cmJsonFindMemberValue(np,label,kStringTId,&vp)) != kOkJsRC )
    return rc;

  return cmJsonStringValue(vp,retPtrPtr);
}

cmJsRC_t      cmJsonNodeMember(   const cmJsonNode_t* np, const char* label, cmJsonNode_t** retPtrPtr )
{
  cmJsonNode_t* vp;
  cmJsRC_t      rc;

  *retPtrPtr = NULL;

  if((rc = _cmJsonFindMemberValue(np,label,kArrayTId|kObjectTId,&vp)) != kOkJsRC )
    return rc;

  *retPtrPtr = vp;

  return rc;
}

cmJsonNode_t* cmJsonNodeMemberValue( const cmJsonNode_t* np, const char* label )
{
  assert( cmJsonIsObject(np) );

  unsigned n = cmJsonChildCount(np);
  unsigned j = 0;
  for(; j<n; ++j)
  {
    const cmJsonNode_t* cnp = cmJsonArrayElementC(np,j);

    assert( cnp != NULL && cmJsonIsPair(cnp) );

    if( strcmp(label,cmJsonPairLabel(cnp))==0 )
      return cmJsonPairValue((cmJsonNode_t*)cnp);
  }
  
  return NULL;
}

cmJsRC_t      cmJsonVMemberValues( const cmJsonNode_t* objectNodePtr, const char** errLabelPtrPtr, va_list vl )
{
  unsigned typeId;
  cmJsRC_t rc = kOkJsRC;

  if( errLabelPtrPtr != NULL )
    *errLabelPtrPtr = NULL;

  const char* label;
  while( ((label = va_arg(vl,const char*)) != NULL) && (rc==kOkJsRC) )
  {
    typeId = va_arg(vl,unsigned);

    switch( typeId & kMaskTId )
    {

      case kObjectTId:
      case kPairTId:
      case kArrayTId:
        {
          cmJsonNode_t** nodePtrPtr = va_arg(vl, cmJsonNode_t**);

          if((rc = cmJsonNodeMember(objectNodePtr,label,nodePtrPtr )) == kOkJsRC )
          {
            cmJsonNode_t* np = *nodePtrPtr;
            if( (np->typeId & kMaskTId) != (typeId&kMaskTId) )
              rc = kNodeCannotCvtJsRC;
          }
          
        }
        break;

      case kIntTId:        
        {
          int* ip = va_arg(vl, int* );
          assert(ip != NULL);
          rc = cmJsonIntMember(objectNodePtr, label, ip);
        }
        break;

      case kRealTId:
        {
          double* dp = va_arg(vl, double*);
          assert(dp != NULL);
          rc = cmJsonRealMember(objectNodePtr, label, dp);
        }
        break;

      case kStringTId:
        {
          const char** cpp = va_arg(vl, const char**);
          assert(cpp != NULL);
          rc = cmJsonStringMember(objectNodePtr, label, cpp);
        }
        break;
        
      case kTrueTId:
      case kFalseTId:
      case kBoolTId:
        {
          bool* bp = va_arg(vl, bool* );
          rc = cmJsonBoolMember(objectNodePtr, label, bp);
        }
        break;

      default:
        // missing terminating NULL on the var args list???
        assert(0);
        break;
    }

    if( (rc == kNodeNotFoundJsRC) && cmIsFlag(typeId,kOptArgJsFl) )
      rc = kOkJsRC;

    if( rc != kOkJsRC && errLabelPtrPtr != NULL )
      *errLabelPtrPtr = label;
      
  }

  return rc;
}

cmJsRC_t      cmJsonMemberValues( const cmJsonNode_t* objectNodePtr, const char** errLabelPtrPtr, ...)
{
  va_list vl;
  va_start(vl,errLabelPtrPtr);
  cmJsRC_t rc = cmJsonVMemberValues(objectNodePtr,errLabelPtrPtr,vl);
  va_end(vl);
  return rc;
}

cmJsRC_t      cmJsonVPathValues( cmJsonH_t h, const char* pathPrefix, const cmJsonNode_t* objNodePtr, const char** errLabelPtrPtr, va_list vl )
{
  cmJsRC_t rc = kOkJsRC;
  cmJs_t*  p  = _cmJsonHandleToPtr(h);

  if( errLabelPtrPtr != NULL )
    *errLabelPtrPtr = NULL;

  const char* path;
  while( ((path = va_arg(vl,const char*)) != NULL) && (rc==kOkJsRC) )
  {
    unsigned            typeId;
    const cmJsonNode_t* vnp;

    typeId = va_arg(vl,unsigned);

    // find the requested pair value
    if((rc = _cmJsonFindPathValue(p,pathPrefix,path,objNodePtr,&vnp)) != kOkJsRC )
      break;

    switch( typeId & kMaskTId )
    {
      case kObjectTId:
      case kPairTId:
      case kArrayTId:
        {
          const cmJsonNode_t** nodePtrPtr = va_arg(vl, const cmJsonNode_t**);

          if( (vnp->typeId & kMaskTId) != (typeId & kMaskTId) )
            rc = kNodeCannotCvtJsRC;
          else
            *nodePtrPtr = vnp;
          
        }
        break;

      case kIntTId:        
        {
          int* ip = va_arg(vl, int* );
          assert(ip != NULL);
          rc = cmJsonIntValue(vnp, ip);
        }
        break;

      case kRealTId:
        {
          double* dp = va_arg(vl, double*);
          assert(dp != NULL);
          rc = cmJsonRealValue(vnp, dp);
        }
        break;

      case kStringTId:
        {
          const char** cpp = va_arg(vl, const char**);
          assert(cpp != NULL);
          rc = cmJsonStringValue(vnp, cpp);
        }
        break;
        
      case kTrueTId:
      case kFalseTId:
        {
          bool* bp = va_arg(vl, bool* );
          rc = cmJsonBoolValue(vnp, bp);
        }
        break;

      default:
        // missing terminating NULL on the var args list???
        assert(0);
        break;

    }

    if( (rc == kNodeNotFoundJsRC) && cmIsFlag(typeId,kOptArgJsFl) )
      rc = kOkJsRC;

    if( rc != kOkJsRC && errLabelPtrPtr != NULL )
      *errLabelPtrPtr = path;
      
  }

  return rc;
}

cmJsRC_t      cmJsonPathValues( cmJsonH_t h, const char* pathPrefix, const cmJsonNode_t* objectNodePtr, const char** errLabelPtrPtr, ... )
{
  va_list vl;
  va_start(vl,errLabelPtrPtr);
  cmJsRC_t rc = cmJsonVPathValues(h,pathPrefix,objectNodePtr,errLabelPtrPtr,vl);
  va_end(vl);
  return rc;
}


cmJsonNode_t* _cmJsonCreateNode2( cmJsonH_t h, unsigned newNodeTypeId, cmJsonNode_t* parentPtr )
{
  cmJs_t*       p   = _cmJsonHandleToPtr(h);
  cmJsonNode_t* np  = NULL;

  if((p->rc = _cmJsonCreateNode(p,parentPtr,newNodeTypeId,&np)) != kOkJsRC )
    return NULL;

  return np;
}


cmJsRC_t cmJsonCreate( cmJsonH_t h, cmJsonNode_t* parentPtr, unsigned typeId, const char* sv, int iv, double dv, cmJsonNode_t** rpp )
{
  cmJsonNode_t* rp = NULL;
  cmJsRC_t      rc = kOkJsRC;
  cmJs_t*       p  = _cmJsonHandleToPtr(h);

  if( rpp != NULL )
    *rpp = NULL;

  switch( typeId )
  {    
    case kObjectTId:
    case kArrayTId:  
      if((rp = _cmJsonCreateNode2(h,typeId,parentPtr)) == NULL)
        rc = p->rc;
      else
      {
        if( rpp != NULL )
          *rpp = rp;
      }
      break;
      
    case kPairTId:   rc = _cmJsonCreatePair(p,parentPtr,sv,rpp); break;
    case kIntTId:    rc = _cmJsonCreateInt(p,parentPtr,iv,rpp); break;
    case kRealTId:   rc = _cmJsonCreateReal(p,parentPtr,dv,rpp); break;
    case kTrueTId:   rc = _cmJsonCreateBool(p,parentPtr,true,rpp); break;
    case kFalseTId:  rc = _cmJsonCreateBool(p,parentPtr,false,rpp); break;
    case kNullTId:   rc = _cmJsonCreateNull(p,parentPtr,rpp); break;
    case kStringTId: rc = _cmJsonCreateString(p,parentPtr,sv,strlen(sv),rpp); break;
    default:
      assert(0);
      break;
  }

  return rc;
}


cmJsonNode_t* cmJsonCreateObject( cmJsonH_t h, cmJsonNode_t* parentPtr )
{ return _cmJsonCreateNode2(h,kObjectTId,parentPtr); }


cmJsonNode_t* cmJsonCreateArray(  cmJsonH_t h, cmJsonNode_t* parentPtr )
{ return _cmJsonCreateNode2(h,kArrayTId,parentPtr); }


cmJsonNode_t* cmJsonCreatePair(   cmJsonH_t h, cmJsonNode_t* parentPtr, const char* label )
{
  cmJsonNode_t* np;

  cmJs_t* p = _cmJsonHandleToPtr(h);

  if((_cmJsonCreatePair(p,parentPtr,label,&np)) != kOkJsRC )
    return NULL;

  return np;
}

cmJsRC_t      cmJsonCreateString( cmJsonH_t h, cmJsonNode_t* parentPtr, const char* stringValue )
{
  cmJs_t* p = _cmJsonHandleToPtr(h);
  return _cmJsonCreateString( p, parentPtr, stringValue, strlen(stringValue),NULL);;
}

cmJsRC_t      cmJsonCreateStringN(cmJsonH_t h, cmJsonNode_t* parentPtr, const char* stringValue, unsigned stringCharCnt )
{
  cmJs_t* p = _cmJsonHandleToPtr(h);
  return _cmJsonCreateString( p, parentPtr, stringValue, stringCharCnt,NULL);;
}

cmJsRC_t      cmJsonCreateInt(    cmJsonH_t h, cmJsonNode_t* parentPtr, int value )
{
  cmJs_t* p = _cmJsonHandleToPtr(h);
  return _cmJsonCreateInt( p, parentPtr, value, NULL );
}

cmJsRC_t      cmJsonCreateReal(   cmJsonH_t h, cmJsonNode_t* parentPtr, double value )
{
  cmJs_t* p = _cmJsonHandleToPtr(h);
  return _cmJsonCreateReal( p, parentPtr, value, NULL );
}

cmJsRC_t      cmJsonCreateBool(   cmJsonH_t h, cmJsonNode_t* parentPtr, bool value )
{
  cmJs_t* p = _cmJsonHandleToPtr(h);
  return _cmJsonCreateBool( p, parentPtr, value,NULL );
}

 cmJsRC_t      cmJsonCreateNull(   cmJsonH_t h, cmJsonNode_t* parentPtr )
{
  cmJs_t* p = _cmJsonHandleToPtr(h);
  return _cmJsonCreateNull( p, parentPtr, NULL );
}

cmJsRC_t      cmJsonCreateStringArray( cmJsonH_t h, cmJsonNode_t* parentPtr, unsigned n, const char** value )
{
  cmJs_t*       p  = _cmJsonHandleToPtr(h);
  cmJsRC_t      rc = kOkJsRC;
  cmJsonNode_t* np;
  unsigned      i;

  if((np = cmJsonCreateArray(h,parentPtr)) == NULL )
    return _cmJsonError(p,cmErrLastRC(&p->err),"Unable to create 'bool' array.");
  
  for(i=0; i<n; ++i)
    if((rc = cmJsonCreateString(h,np,value[i])) != kOkJsRC )
      return _cmJsonError(p,rc,"Unable to create 'bool' array element at index %i.",i);

  return rc;
}

cmJsRC_t      cmJsonCreateIntArray(    cmJsonH_t h, cmJsonNode_t* parentPtr, unsigned n, const int* value )
{
  cmJs_t*       p  = _cmJsonHandleToPtr(h);
  cmJsRC_t      rc = kOkJsRC;
  cmJsonNode_t* np;
  unsigned      i;

  if((np = cmJsonCreateArray(h,parentPtr)) == NULL )
    return _cmJsonError(p,cmErrLastRC(&p->err),"Unable to create 'int' array.");
  
  for(i=0; i<n; ++i)
    if((rc = cmJsonCreateInt(h,np,value[i])) != kOkJsRC )
      return _cmJsonError(p,rc,"Unable to create 'int' array element at index %i.",i);

  return rc;

}

cmJsRC_t      cmJsonCreateRealArray(   cmJsonH_t h, cmJsonNode_t* parentPtr, unsigned n, const double* value )
{
  cmJs_t*       p  = _cmJsonHandleToPtr(h);
  cmJsRC_t      rc = kOkJsRC;
  cmJsonNode_t* np;
  unsigned      i;

  if((np = cmJsonCreateArray(h,parentPtr)) == NULL )
    return _cmJsonError(p,cmErrLastRC(&p->err),"Unable to create 'real' array.");
  
  for(i=0; i<n; ++i)
    if((rc = cmJsonCreateReal(h,np,value[i])) != kOkJsRC )
      return _cmJsonError(p,rc,"Unable to create 'real' array element at index %i.",i);

  return rc;
}

cmJsRC_t      cmJsonCreateBoolArray(   cmJsonH_t h, cmJsonNode_t* parentPtr, unsigned n, const bool* value )
{
  cmJs_t*       p  = _cmJsonHandleToPtr(h);
  cmJsRC_t      rc = kOkJsRC;
  cmJsonNode_t* np;
  unsigned      i;

  if((np = cmJsonCreateArray(h,parentPtr)) == NULL )
    return _cmJsonError(p,cmErrLastRC(&p->err),"Unable to create 'bool' array.");
  
  for(i=0; i<n; ++i)
    if((rc = cmJsonCreateBool(h,np,value[i])) != kOkJsRC )
      return _cmJsonError(p,rc,"Unable to create 'bool' array element at index %i.",i);

  return rc;
}


cmJsRC_t    cmJsonSetInt(    cmJsonH_t h, cmJsonNode_t*  np, int         ival )
{
  if( np->typeId != kIntTId )
    return _cmJsonError(_cmJsonHandleToPtr(h),kInvalidNodeTypeJsRC,"Cannot assign type 'int' to node type '%s'.",_cmJsonNodeTypeIdToLabel(np->typeId));

  np->u.intVal = ival;

  return kOkJsRC;
}

cmJsRC_t    cmJsonSetReal(   cmJsonH_t h, cmJsonNode_t * np, double      rval )
{
  if( np->typeId != kRealTId )
    return _cmJsonError(_cmJsonHandleToPtr(h),kInvalidNodeTypeJsRC,"Cannot assign type 'real' to node type '%s'.",_cmJsonNodeTypeIdToLabel(np->typeId));

  np->u.realVal = rval;

  return kOkJsRC;
}

cmJsRC_t    cmJsonSetBool(   cmJsonH_t h, cmJsonNode_t * np, bool        bval )
{
  if( np->typeId == kTrueTId || np->typeId==kFalseTId )
    return _cmJsonError(_cmJsonHandleToPtr(h),kInvalidNodeTypeJsRC,"Cannot assign type 'bool' to node type '%s'.",_cmJsonNodeTypeIdToLabel(np->typeId));

  np->u.boolVal = bval;

  return kOkJsRC;  
}

cmJsRC_t    cmJsonSetString( cmJsonH_t h, cmJsonNode_t*  np, const char* sval )
{
  cmJs_t* p = _cmJsonHandleToPtr(h);

  if( np->typeId != kStringTId )
    return _cmJsonError(p,kInvalidNodeTypeJsRC,"Cannot assign type 'string' to node type '%s'.",_cmJsonNodeTypeIdToLabel(np->typeId));

  unsigned sn = strlen(sval);

  if( np->u.stringVal != NULL && strlen(np->u.stringVal) <= sn )
    strcpy(np->u.stringVal,sval);
  else
    return  _cmJsonSetString(p,np,sval,sn);

  return kOkJsRC;
}

cmJsonNode_t* cmJsonInsertPair( cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned typeId, const char* sv, int iv, double dv )
{
   assert( objectNodePtr->typeId == kObjectTId );

   cmJsRC_t      rc;
   cmJsonNode_t* pairNodePtr;
 
   cmJs_t* p = _cmJsonHandleToPtr(h);

   if((rc = _cmJsonCreatePair(p,objectNodePtr,label,&pairNodePtr)) != kOkJsRC )
     return NULL;
   
   assert( pairNodePtr != NULL );

   if((rc = cmJsonCreate(h,pairNodePtr,typeId,sv,iv,dv,NULL)) != kOkJsRC )
     return NULL;

   return pairNodePtr;  
} 


cmJsonNode_t* cmJsonInsertPairObject( cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label )
{
  assert( objectNodePtr->typeId == kObjectTId );
  cmJsonNode_t* pairNodePtr;
  if((pairNodePtr = cmJsonCreatePair(h,objectNodePtr,label)) == NULL )
    return NULL;
  
  return cmJsonCreateObject(h,pairNodePtr);  
}

cmJsonNode_t* cmJsonInsertPairArray(  cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label )
{
  assert( objectNodePtr->typeId == kObjectTId );
  cmJsonNode_t* pairNodePtr;
  if((pairNodePtr = cmJsonCreatePair(h,objectNodePtr,label)) == NULL )
    return NULL;
  
  return cmJsonCreateArray(h,pairNodePtr);  
}

cmJsonNode_t* cmJsonInsertPairPair(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, const char* pairLabel )
{
  assert( objectNodePtr->typeId == kObjectTId );
  cmJsonNode_t* pairNodePtr;
  if((pairNodePtr = cmJsonCreatePair(h,objectNodePtr,label)) == NULL )
    return NULL;
  
  return cmJsonCreatePair(h,pairNodePtr,pairLabel);  
}



cmJsRC_t      cmJsonInsertPairInt( cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, int intVal )
{
  assert( objectNodePtr->typeId == kObjectTId );
  cmJsonNode_t* pairNodePtr;
  if((pairNodePtr = cmJsonCreatePair(h,objectNodePtr,label)) == NULL )
    return cmJsonErrorCode(h);
  
  return cmJsonCreateInt(h,pairNodePtr,intVal);
}
cmJsRC_t      cmJsonInsertPairReal(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, double realVal )
{
  assert( objectNodePtr->typeId == kObjectTId );
  cmJsonNode_t* pairNodePtr;
  if((pairNodePtr = cmJsonCreatePair(h,objectNodePtr,label)) == NULL )
    return cmJsonErrorCode(h);
  
  return cmJsonCreateReal(h,pairNodePtr,realVal);
}

cmJsRC_t      cmJsonInsertPairString( cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, const char* stringVal )
{ return cmJsonInsertPairStringN(h,objectNodePtr,label,stringVal,stringVal==NULL ? 0 : strlen(stringVal)); }

cmJsRC_t      cmJsonInsertPairStringN( cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, const char* stringVal, unsigned stringCharCnt )
{
  assert( objectNodePtr->typeId == kObjectTId );
  cmJsonNode_t* pairNodePtr;
  if((pairNodePtr = cmJsonCreatePair(h,objectNodePtr,label)) == NULL )
    return cmJsonErrorCode(h);
  
  return cmJsonCreateStringN(h,pairNodePtr,stringVal,stringCharCnt);
}


cmJsRC_t      cmJsonInsertPairBool(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, bool boolVal )
{
  assert( objectNodePtr->typeId == kObjectTId );
  cmJsonNode_t* pairNodePtr;
  if((pairNodePtr = cmJsonCreatePair(h,objectNodePtr,label)) == NULL )
    return cmJsonErrorCode(h);
  
  return cmJsonCreateBool(h,pairNodePtr,boolVal);
}

cmJsRC_t      cmJsonInsertPairNull(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label )
{
  assert( objectNodePtr->typeId == kObjectTId );
  cmJsonNode_t* pairNodePtr;
  if((pairNodePtr = cmJsonCreatePair(h,objectNodePtr,label)) == NULL )
    return cmJsonErrorCode(h);
  
  return cmJsonCreateNull(h,pairNodePtr);
}

cmJsRC_t      cmJsonInsertPairIntArray(    cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned n, const int* values )
{
  assert( objectNodePtr->typeId == kObjectTId );
  cmJsonNode_t* pairNodePtr;
  if((pairNodePtr = cmJsonCreatePair(h,objectNodePtr,label)) == NULL )
    return cmJsonErrorCode(h);

  return cmJsonCreateIntArray(h,pairNodePtr,n,values);
}

cmJsRC_t      cmJsonInsertPairRealArray(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned n, const double* values )
{
  assert( objectNodePtr->typeId == kObjectTId );
  cmJsonNode_t* pairNodePtr;
  if((pairNodePtr = cmJsonCreatePair(h,objectNodePtr,label)) == NULL )
    return cmJsonErrorCode(h);

  return cmJsonCreateRealArray(h,pairNodePtr,n,values);
}

cmJsRC_t      cmJsonInsertPairStringArray( cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned n, const char** values )
{
  assert( objectNodePtr->typeId == kObjectTId );
  cmJsonNode_t* pairNodePtr;
  if((pairNodePtr = cmJsonCreatePair(h,objectNodePtr,label)) == NULL )
    return cmJsonErrorCode(h);

  return cmJsonCreateStringArray(h,pairNodePtr,n,values);
}

cmJsRC_t      cmJsonInsertPairBoolArray(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned n, const bool* values )
{
  assert( objectNodePtr->typeId == kObjectTId );
  cmJsonNode_t* pairNodePtr;
  if((pairNodePtr = cmJsonCreatePair(h,objectNodePtr,label)) == NULL )
    return cmJsonErrorCode(h);

  return cmJsonCreateBoolArray(h,pairNodePtr,n,values);

}

cmJsonNode_t*    cmJsonInsertPairIntArray2(    cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned n, const int* values )
{
  assert( objectNodePtr->typeId == kObjectTId );
  cmJsonNode_t* pairNodePtr;
  if((pairNodePtr = cmJsonCreatePair(h,objectNodePtr,label)) != NULL )
    if( cmJsonCreateIntArray(h,pairNodePtr,n,values) != kOkJsRC )
      return NULL;

  return pairNodePtr;
}

cmJsonNode_t*    cmJsonInsertPairRealArray2(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned n, const double* values )
{
  assert( objectNodePtr->typeId == kObjectTId );
  cmJsonNode_t* pairNodePtr;
  if((pairNodePtr = cmJsonCreatePair(h,objectNodePtr,label)) != NULL )
    if( cmJsonCreateRealArray(h,pairNodePtr,n,values) != kOkJsRC )
      return NULL;
  return pairNodePtr;
}

cmJsonNode_t*    cmJsonInsertPairStringArray2( cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned n, const char** values )
{
  assert( objectNodePtr->typeId == kObjectTId );
  cmJsonNode_t* pairNodePtr;
  if((pairNodePtr = cmJsonCreatePair(h,objectNodePtr,label)) != NULL )
    if( cmJsonCreateStringArray(h,pairNodePtr,n,values) != kOkJsRC )
      return NULL;

  return pairNodePtr;
}

cmJsonNode_t*    cmJsonInsertPairBoolArray2(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned n, const bool* values )
{
  assert( objectNodePtr->typeId == kObjectTId );
  cmJsonNode_t* pairNodePtr;
  if((pairNodePtr = cmJsonCreatePair(h,objectNodePtr,label)) != NULL )
    if( cmJsonCreateBoolArray(h,pairNodePtr,n,values) != kOkJsRC )
      return NULL;
  return pairNodePtr;
}


cmJsRC_t  _cmJsonInsertOrReplacePair( cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, unsigned newTypeId, const char* sv, int iv, double dv, cmJsonNode_t* nv, bool insertFl, cmJsonNode_t** retNodePtrPtr )
{
  cmJsRC_t      rc          = kOkJsRC;
  cmJs_t*       p           = _cmJsonHandleToPtr(h);
  cmJsonNode_t* valNodePtr  = NULL;
  cmJsonNode_t* pairNodePtr = NULL;

  assert( objectNodePtr->typeId == kObjectTId );

  if( retNodePtrPtr != NULL )
    *retNodePtrPtr = NULL;

  // if a matching pair was not found  ....
  if(( valNodePtr = cmJsonFindValue(h,label,objectNodePtr,matchTypeMask)) == NULL )
  {
    // ... and insertion is not allowed then return error
    if( insertFl == false )
      return kNodeNotFoundJsRC;

    // ... otherwise insert a new pair and return a pointer to it
    pairNodePtr = cmJsonInsertPair(h,objectNodePtr,label,newTypeId,sv,iv,dv);

    goto errLabel;
   
  }

  // ... otherwise a match was found to at least the pair label.
  // If matchTypeMask was set to kInvalidTId then the type id
  // of the found pair may not be the same as newTypeId. To handle
  // this case we proceed by first deallocating all resources held
  // by the found node and then proceeding by either creating 
  // deleting the found node and creating a replacement node 
  // (object,array,pair) or forcing the found node to a new  
  // type (int,real,true,false,string,null).

  assert( valNodePtr != NULL);

  pairNodePtr = valNodePtr->ownerPtr;

  // release any resources held by the found node
  switch( valNodePtr->typeId )
  {
    case kObjectTId:
    case kArrayTId:
    case kPairTId:

      // remove the pair value node and replace it with a 'null' node.
      if((rc =_cmJsonRemoveNode( p, valNodePtr, true, true )) != kOkJsRC )
        goto errLabel;
      break;

    case kStringTId:
      // deallocate string memory
      _cmJsonSetString( p, valNodePtr, NULL, 0 );
      break;
      
  }
  

  // relace the found node with the new node
  switch( newTypeId )
  {
    case kObjectTId:
    case kArrayTId:
    case kPairTId:
      {
        cmJsonNode_t* newValueNodePtr = NULL;

        // remove the current pair value  ....
        if((rc =_cmJsonRemoveNode( p, valNodePtr, true, false )) != kOkJsRC )
          goto errLabel;


        // new pair nodes should have the pair label in 'sv'
        assert( newTypeId!=kPairTId || (newTypeId==kPairTId && sv != NULL ));

        // if no new value was given or the new value is a pair then ...
        if( nv == NULL || newTypeId == kPairTId )
        {
          // ... create a new blank array,obj or pair
          if((rc = cmJsonCreate( h, pairNodePtr, newTypeId, sv, 0, 0, &newValueNodePtr )) != kOkJsRC )
            goto errLabel;
        }

        // if the new value is a pair and no value node was given then set the
        // new pairs value node to NULL
        if( nv == NULL && newTypeId == kPairTId )
        {
          if((rc = _cmJsonCreateNull(p,newValueNodePtr,NULL)) != kOkJsRC )
            goto errLabel;        
        }
        
        // if a new value node was given  
        if( nv != NULL )
        {
          // the new child node should not already be linked to a parent
          assert( nv->ownerPtr == NULL );

          // if the new value is an obj or array then the new 
          // value node type id should be the same
          assert( newTypeId==kPairTId || newTypeId==nv->typeId );

          // 
          cmJsonNode_t* pp = newTypeId==kPairTId ? newValueNodePtr : pairNodePtr;

          assert( pp->typeId == kPairTId );

          // link in the child to the pair
          if((rc = _cmJsonLinkInChild(p,pp,nv)) != kOkJsRC )
            goto errLabel;

        }
      }
      break;

      // All cases below follow the same pattern:
      // 1. Set the type id to the replacement value type id
      // 2. Assign the value to the node.
      // This sequence is safe because all resources were freed 
      // by the earlier switch.

    case kStringTId:
      valNodePtr->typeId = kStringTId;
      _cmJsonSetString( p, valNodePtr, sv, strlen(sv) );
      break;

    case kIntTId:
      valNodePtr->typeId =kIntTId;
      valNodePtr->u.intVal = iv;
      break;

    case kRealTId:
      valNodePtr->typeId = kRealTId;
      valNodePtr->u.realVal = dv;
      break;

    case kTrueTId:
      valNodePtr->typeId = kTrueTId;
      valNodePtr->u.boolVal = true;
      break;

    case kFalseTId:
      valNodePtr->typeId = kFalseTId;
      valNodePtr->u.boolVal = false;
      break;

    case kNullTId:
      valNodePtr->typeId = kNullTId;
      break;
    
    default:
      { assert(0); }

  }

 errLabel:
  
  if( rc == kOkJsRC )
    if( retNodePtrPtr != NULL )
      *retNodePtrPtr = pairNodePtr;

  return rc;
}

cmJsonNode_t* cmJsonInsertOrReplacePair( cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, unsigned typeId, const char* sv, int iv, double dv, cmJsonNode_t* nv )
{  
  cmJsonNode_t* retNodePtr = NULL;

  _cmJsonInsertOrReplacePair(h,objectNodePtr,label,matchTypeMask,typeId,sv,iv,dv,nv,true,&retNodePtr ); 

  return retNodePtr;
}

cmJsonNode_t* cmJsonInsertOrReplacePairObject( cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, cmJsonNode_t* newObjNodePtr )
{
  cmJsonNode_t* pairPtr;
  if((pairPtr = cmJsonInsertOrReplacePair(h,objectNodePtr,label,matchTypeMask,kObjectTId,NULL,0,0,newObjNodePtr)) == NULL )
    return NULL;

  return pairPtr->u.childPtr->siblingPtr;
}

cmJsonNode_t* cmJsonInsertOrReplacePairArray(  cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, cmJsonNode_t* newArrayNodePtr )
{
  cmJsonNode_t* pairPtr;
  if((pairPtr = cmJsonInsertOrReplacePair(h,objectNodePtr,label,matchTypeMask,kArrayTId,NULL,0,0,newArrayNodePtr)) == NULL )
    return NULL;

  return pairPtr->u.childPtr->siblingPtr;
}

cmJsonNode_t* cmJsonInsertOrReplacePairPair(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, const char* newPairLabel, cmJsonNode_t* newPairValNodePtr )
{
  cmJsonNode_t* pairPtr;
  if((pairPtr = cmJsonInsertOrReplacePair(h,objectNodePtr,label,matchTypeMask,kPairTId,newPairLabel,0,0,newPairValNodePtr)) == NULL )
    return NULL;

  return pairPtr->u.childPtr->siblingPtr;
}

cmJsRC_t      cmJsonInsertOrReplacePairInt(    cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, int intVal )
{ return cmJsonInsertOrReplacePair(h,objectNodePtr,label,matchTypeMask,kIntTId,NULL,intVal,0,NULL) == NULL ? cmJsonErrorCode(h) : kOkJsRC;   }

cmJsRC_t      cmJsonInsertOrReplacePairReal(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, double realVal )
{ return cmJsonInsertOrReplacePair(h,objectNodePtr,label,matchTypeMask,kRealTId,NULL,0,realVal,NULL) == NULL ? cmJsonErrorCode(h) : kOkJsRC;   }

cmJsRC_t      cmJsonInsertOrReplacePairString( cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, const char* stringVal )
{ return cmJsonInsertOrReplacePair(h,objectNodePtr,label,matchTypeMask,kStringTId,stringVal,0,0,NULL) == NULL ? cmJsonErrorCode(h) : kOkJsRC;   }

cmJsRC_t      cmJsonInsertOrReplacePairBool(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, bool boolVal )
{ return cmJsonInsertOrReplacePair(h,objectNodePtr,label,matchTypeMask,boolVal ? kTrueTId : kFalseTId,NULL,0,0,NULL) == NULL ? cmJsonErrorCode(h) : kOkJsRC;   }

cmJsRC_t      cmJsonInsertOrReplacePairNull(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask )
{ return cmJsonInsertOrReplacePair(h,objectNodePtr,label,matchTypeMask,kNullTId,NULL,0,0,NULL) == NULL ? cmJsonErrorCode(h) :  kOkJsRC;   }

cmJsonNode_t* cmJsonReplacePair(       cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, unsigned newTypeId, const char* sv, int iv, double dv, cmJsonNode_t* nv )
{
  cmJsonNode_t* retNodePtr = NULL;
  _cmJsonInsertOrReplacePair(h,objectNodePtr,label,matchTypeMask,newTypeId,sv,iv,dv,nv,false,&retNodePtr ); 
  return retNodePtr;
}

cmJsonNode_t* cmJsonReplacePairObject( cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, cmJsonNode_t* newPairNodePtr )
{
  cmJsonNode_t* pairPtr;
  if((pairPtr = cmJsonReplacePair(h,objectNodePtr,label,matchTypeMask,kObjectTId,NULL,0,0,newPairNodePtr)) == NULL )
    return NULL;
  return pairPtr->u.childPtr->siblingPtr;
}


cmJsonNode_t* cmJsonReplacePairArray(  cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, cmJsonNode_t* newArrayNodePtr )
{
  cmJsonNode_t* pairPtr;
  if((pairPtr = cmJsonReplacePair(h,objectNodePtr,label,matchTypeMask,kArrayTId,NULL,0,0,newArrayNodePtr)) == NULL )
    return NULL;
  return pairPtr->u.childPtr->siblingPtr;
}

cmJsonNode_t* cmJsonReplacePairPair(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, const char* newPairLabel, cmJsonNode_t* newPairValNodePtr )
{
  cmJsonNode_t* pairPtr;
  if((pairPtr = cmJsonReplacePair(h,objectNodePtr,label,matchTypeMask,kPairTId,newPairLabel,0,0,newPairValNodePtr)) == NULL )
    return NULL;
  return pairPtr->u.childPtr->siblingPtr;
}


cmJsRC_t      cmJsonReplacePairInt(    cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, int intVal )
{ return cmJsonReplacePair(h,objectNodePtr,label,matchTypeMask,kIntTId,NULL,intVal,0,NULL) == NULL ? cmJsonErrorCode(h) : kOkJsRC; }

cmJsRC_t      cmJsonReplacePairReal(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, double realVal )
{ return cmJsonReplacePair(h,objectNodePtr,label,matchTypeMask,kRealTId,NULL,0,realVal,NULL) == NULL ? cmJsonErrorCode(h) : kOkJsRC; }

cmJsRC_t      cmJsonReplacePairString( cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, const char* stringVal )
{ return cmJsonReplacePair(h,objectNodePtr,label,matchTypeMask,kStringTId,stringVal,0,0,NULL) == NULL ? cmJsonErrorCode(h) : kOkJsRC;   }

cmJsRC_t      cmJsonReplacePairBool(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, bool boolVal )
{ return cmJsonReplacePair(h,objectNodePtr,label,matchTypeMask,boolVal ? kTrueTId : kFalseTId,NULL,0,0,NULL) == NULL ? cmJsonErrorCode(h) : kOkJsRC;   }

cmJsRC_t      cmJsonReplacePairNull(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask )
{ return cmJsonReplacePair(h,objectNodePtr,label,matchTypeMask,kNullTId,NULL,0,0,NULL) == NULL ? cmJsonErrorCode(h) :  kOkJsRC;   }

cmJsRC_t      cmJsonVInsertPairs( cmJsonH_t h, cmJsonNode_t* objNodePtr, va_list vl )
{
  cmJsRC_t rc = kOkJsRC;

  assert( objNodePtr->typeId == kObjectTId );

  const char* label;

  while( ((label = va_arg(vl,const char*)) != NULL) && (rc == kOkJsRC) )
  {
    unsigned sel = va_arg(vl,unsigned);
    switch( sel )
    {
      case kObjectTId:
        if( cmJsonInsertPairObject(h,objNodePtr,label) == NULL )
          rc = cmJsonErrorCode(h);
        break;

      case kArrayTId:
        if( cmJsonInsertPairArray(h,objNodePtr,label) == NULL )
          rc = cmJsonErrorCode(h);
        break;

      case kPairTId:
        if( cmJsonInsertPairPair(h,objNodePtr,label, va_arg(vl,const char*)) == NULL )
          rc = cmJsonErrorCode(h);
        break;


      case kIntTId:
        rc = cmJsonInsertPairInt(h,objNodePtr,label, va_arg(vl,int) );
        break;

      case kRealTId:
        rc = cmJsonInsertPairReal(h,objNodePtr,label, va_arg(vl,double) );
        break;

      case kStringTId:
        rc = cmJsonInsertPairString(h,objNodePtr,label, va_arg(vl,const char *) );
        break;

      case kTrueTId:
      case kFalseTId:
      case kBoolTId:
        rc = cmJsonInsertPairBool(h,objNodePtr,label, va_arg(vl,int) );
        break;

      case kNullTId:
        rc = cmJsonInsertPairNull(h,objNodePtr,label );
        break;

      default:
        // missing terminating NULL on the var args list???
        assert(0);
        break;
    }
  }

  return rc;
}    

cmJsRC_t      cmJsonInsertPairs( cmJsonH_t h, cmJsonNode_t* objectNodePtr, ... )
{
  va_list vl;
  va_start(vl,objectNodePtr);
  cmJsRC_t rc = cmJsonVInsertPairs(h,objectNodePtr,vl);
  va_end(vl);
  return rc;
}

cmJsonNode_t* cmJsonVCreateFilledObject( cmJsonH_t h, cmJsonNode_t* parentPtr, va_list vl )
{
  cmJsonNode_t* np;

  if((np = cmJsonCreateObject(h,parentPtr)) == NULL)
    return NULL;

  if( cmJsonVInsertPairs(h,np,vl) != kOkJsRC )
  {
    cmJsonRemoveNode(h,np,true);
    return NULL;
  }

  return np;  
}

cmJsonNode_t* cmJsonCreateFilledObject( cmJsonH_t h, cmJsonNode_t* parentPtr, ... )
{
  va_list vl;
  va_start(vl,parentPtr);
  cmJsonNode_t* np = cmJsonVCreateFilledObject(h,parentPtr,vl);
  va_end(vl);
  return np;
}

void _cmJsonFreeNode( cmJs_t* p, cmJsonNode_t* np )
{
  switch( np->typeId )
  {
    case kObjectTId:
    case kPairTId:
    case kArrayTId:
      {
        cmJsonNode_t* cnp = np->u.childPtr;
        while( cnp != NULL )
        {
          cmJsonNode_t* nnp = cnp->siblingPtr;
          _cmJsonFreeNode(p,cnp);
          cnp = nnp;
        }

      }
      break;

    case kStringTId:
      if( np->u.stringVal != NULL )
        cmLHeapFree(p->heapH,np->u.stringVal);
      break;


  }

  cmLHeapFree(p->heapH,np);
    
}

cmJsRC_t _cmJsonRemoveNode( cmJs_t* p, cmJsonNode_t* np, bool freeFl, bool balancePairsFl )
{
  if(np == NULL )
    return kOkJsRC;

  cmJsonNode_t* parentPtr = np->ownerPtr;

  // if np is the root ...
  if( np == p->rootPtr )
  {
    //  ... we only need to set the root to null to remove the node.
    p->rootPtr = NULL;
  }
  else
  {
    if( parentPtr != NULL )
    {
      // get the parents first child
      cmJsonNode_t* cnp = parentPtr->u.childPtr;

      // if np is the first child then make the second child the first child
      if( cnp == np )
      {
        if( parentPtr->typeId == kPairTId )
          return _cmJsonError( p, kCannotRemoveLabelJsRC, "Cannot remove pair label nodes because this would invalidate the tree structure.");

        parentPtr->u.childPtr = cnp->siblingPtr;
      }
      else
      {
        // otherwise unlink it from the child chain
        while( cnp != NULL )    
        {
          if( cnp->siblingPtr == np )
          {

            cnp->siblingPtr = np->siblingPtr;

            // if the parent is a pair then the removed node is a
            // 'pair value' which must be replaced with a null node in order
            // to maintain a valid tree.
            if( (parentPtr->typeId == kPairTId) && balancePairsFl)
              _cmJsonCreateNull( p, parentPtr, NULL );

            break;
          }

          cnp = cnp->siblingPtr;
        }

        assert( cnp != NULL );        
      }
    }
  }

  // if the memory assoc'd with the removed node should be released
  if( freeFl )
  {
    _cmJsonFreeNode(p,np);
    /*
    if( np->typeId == kStringTId )
    {
      cmLHeapFree(p->heapH,np->u.stringVal);
      np->u.stringVal = NULL;
    }

    cmLHeapFree(p->heapH,np);
    */
  } 

  return kOkJsRC;
}


cmJsRC_t cmJsonRemoveNode( cmJsonH_t h, cmJsonNode_t* np, bool freeFl )
{
  cmJs_t* p = _cmJsonHandleToPtr(h);

  return _cmJsonRemoveNode( p, np, freeFl, true );
}

cmJsRC_t _cmJsonValidateErr( cmJs_t* p, const char* text )
{
  if( p != NULL )
    return _cmJsonSyntaxError(p,text);

  return kValidateFailJsRC;
}

// 'p' is optional.  If 'p' is set to NULL the function will return kValidFailJsRC if the 
// tree rooted at 'np' is invalid and will not print an error message.
cmJsRC_t _cmJsonValidateNode( cmJs_t* p, const cmJsonNode_t* np, const cmJsonNode_t* parentPtr )
{
  cmJsRC_t rc = kOkJsRC;

  if( parentPtr != np->ownerPtr )
    return _cmJsonValidateErr(p,"A child->parent link does not agree with a parent->child link.");

  if( parentPtr == NULL )
  {
    if( np->typeId != kArrayTId && np->typeId != kObjectTId )
      return _cmJsonValidateErr(p,"Only 'array' and 'object' nodes may be the root element.");
  }
  else
  {

    if( parentPtr->typeId != kArrayTId && parentPtr->typeId != kObjectTId && parentPtr->typeId != kPairTId )
      return _cmJsonValidateErr(p,"Parent nodes must be either 'object','array' or 'pair' nodes.");
  }  
  

  switch( np->typeId )
  {
    case kPairTId:
      if( cmJsonChildCount(np) != 2 )
        return _cmJsonValidateErr(p,"'pair' nodes must have exactly two children.");

      if( np->u.childPtr->typeId != kStringTId )
        return _cmJsonValidateErr(p,"The first child of 'pair' nodes must be a 'string' node.");

      // fall through


    case kObjectTId:
    case kArrayTId:
      {
        // validate each child node
        cmJsonNode_t* cnp = np->u.childPtr;
        while(cnp != NULL)
        {
          if( cnp->ownerPtr != np )
            return _cmJsonValidateErr(p,"A parent->child pointer was not validated with a child->parent pointer.");

          if( np->typeId == kObjectTId && cnp->typeId != kPairTId )
            return _cmJsonValidateErr(p,"All 'object' child nodes must be 'pair' nodes.");

          if((rc = _cmJsonValidateNode(p,cnp,np)) != kOkJsRC )
            return rc;

          cnp = cnp->siblingPtr;
        }
      }
      break;


    case kStringTId:
    case kIntTId:
    case kRealTId:
    case kNullTId:
    case kTrueTId:
    case kFalseTId:
    default:
      break;

  }

  return rc;
}

cmJsRC_t      cmJsonValidateTree( cmJsonH_t h )
{
  cmJs_t* p = _cmJsonHandleToPtr(h);

  return _cmJsonValidateNode(p,p->rootPtr,NULL);
}

cmJsRC_t      cmJsonValidate( const cmJsonNode_t* np )
{ return _cmJsonValidateNode(NULL,np,np->ownerPtr); }


cmJsonNode_t* _cmJsonDuplicateNode( cmJs_t* p, const cmJsonNode_t* np, cmJsonNode_t* parentPtr )
{
  cmJsonNode_t* newParentPtr = NULL;
  cmJsonNode_t* newNodePtr   = NULL;
  cmJsRC_t      rc           = kOkJsRC;

  switch( np->typeId )
  {
    case kObjectTId:
    case kArrayTId:
      rc = _cmJsonCreateNode(p,parentPtr,np->typeId,&newParentPtr);
      break;

    case kPairTId:
      rc = _cmJsonCreatePair(p,parentPtr,np->u.childPtr->u.stringVal,&newParentPtr);
      break;

    case kIntTId:
      rc = _cmJsonCreateInt(p,parentPtr,np->u.intVal,&newNodePtr);
      break;

    case kRealTId:
      rc = _cmJsonCreateReal(p,parentPtr,np->u.realVal,&newNodePtr);
      break;

    case kStringTId:
      rc = _cmJsonCreateString(p,parentPtr,np->u.stringVal,strlen(np->u.stringVal),&newNodePtr);
      break;

    case kTrueTId:
    case kFalseTId:
      rc = _cmJsonCreateBool(p,parentPtr,np->u.boolVal,&newNodePtr);
      break;

    case kNullTId:
      rc = _cmJsonCreateNull(p,parentPtr,&newNodePtr);
  }

  if( rc != kOkJsRC )
    return NULL;

  if( newParentPtr != NULL )
  {
    newNodePtr = newParentPtr;

    cmJsonNode_t* cnp = np->u.childPtr;
    
    if(np->typeId == kPairTId)
      cnp = np->u.childPtr->siblingPtr;

    while( cnp != NULL )
    {
      if( _cmJsonDuplicateNode(p,cnp,newParentPtr) != kOkJsRC )
        return NULL;

      cnp = cnp->siblingPtr;
    }
  }

  return newNodePtr;

}

cmJsonNode_t* cmJsonDuplicateNode( cmJsonH_t h, const cmJsonNode_t* np, cmJsonNode_t* parentPtr )
{
  cmJs_t* p = _cmJsonHandleToPtr(h);

  assert( _cmJsonValidateNode(p,np,NULL) == kOkJsRC );

  return _cmJsonDuplicateNode(p,np,parentPtr);
}


cmJsRC_t cmJsonMergeObjectNodes( cmJsonH_t h, cmJsonNode_t* dstObjNodePtr, const cmJsonNode_t* srcObjNodePtr )
{
  assert( dstObjNodePtr!=NULL && dstObjNodePtr->typeId == kObjectTId );
  assert( srcObjNodePtr!=NULL && srcObjNodePtr->typeId == kObjectTId );

  cmJsRC_t            rc  = kOkJsRC;
  cmJs_t*             p   = _cmJsonHandleToPtr(h);
  cmJsonNode_t*       cnp = NULL;
  const cmJsonNode_t* snp = srcObjNodePtr->u.childPtr;

  while( snp!=NULL && rc==kOkJsRC )
  {
    cmJsonNode_t* dnp;

    assert( snp->typeId == kPairTId );

    // if the src pair was not found in the dst object ...
    if((rc = _cmJsonFindMemberValue(dstObjNodePtr, cmJsonPairLabel(snp), snp->u.childPtr->siblingPtr->typeId, &dnp )) != kOkJsRC )
    {
      cmJsonNode_t* newPairNodePtr;

      // the only acceptable error is kNodeNotFoundJsRC 
      // (in particular we reject kNodeCannotCvtJsRC to avoid duplicating
      // pairs with the same name but different types)
      if( rc != kNodeNotFoundJsRC )
        goto errLabel;

      // create the new pair and attach it to the dst obj node
      if((rc = _cmJsonCreatePair(p,dstObjNodePtr,cmJsonPairLabel(snp),&newPairNodePtr)) != kOkJsRC )
        goto errLabel;

      // duplicate the src pair value node and attcmh it to the new dst node
      if(  _cmJsonDuplicateNode(p,snp->u.childPtr->siblingPtr,newPairNodePtr) == NULL )
        rc = p->rc;

      // set kTempJsFl on the new node to use possible cleanup on error later
      newPairNodePtr->typeId = cmSetFlag(newPairNodePtr->typeId,kTempJsFl);

    }


    snp = snp->siblingPtr;
          
  }

 errLabel:

  // for each dst obj pair
  cnp  = dstObjNodePtr->u.childPtr;

  while( cnp != NULL)
  {
    // if this child is a new node
    if( cmIsFlag(cnp->typeId,kTempJsFl) )
    {
      // clear the temp fl
      cnp->typeId = cmClrFlag(cnp->typeId,kTempJsFl);

      // if there was an error remove all new pairs
      if( rc != kOkJsRC )
        cmJsonRemoveNode(h,cnp,true);
    }

    cnp = cnp->siblingPtr;
  }

  return rc;
}


void _cmJsonSerialCopy( cmJsSerial_t* rp, const void* sp, unsigned sn )
{
  assert( rp->nextPtr + sn <= rp->endPtr );

  memcpy(rp->nextPtr,sp,sn);

  rp->nextPtr += sn;
}

cmJsRC_t _cmJsonSerializeNode( const cmJsonNode_t* np, cmJsSerial_t* rp )
{
  cmJsRC_t rc = kOkJsRC;

  // on the first pass rp->basePtr will be NULL so collect size information
  // on the second pass rp->basePtr will be set so copy out data

  // write the type id of this node
  if( rp->basePtr != NULL )
    _cmJsonSerialCopy(rp,&np->typeId,sizeof(np->typeId));
  else
    rp->hdr.byteCnt += sizeof(np->typeId);

  rp->hdr.nodeCnt++;

  switch( np->typeId )
  {

    // write the child count
    case kObjectTId:
    case kArrayTId:
      if( rp->basePtr == NULL )
        rp->hdr.byteCnt += sizeof(unsigned);
      else
      {

        unsigned n = cmJsonChildCount( np );

        _cmJsonSerialCopy(rp,&n,sizeof(unsigned));
      }
      // fall through

    case kPairTId:        
      {
        cmJsonNode_t* cnp = np->u.childPtr;
        while(cnp != NULL )
        {
          _cmJsonSerializeNode(cnp,rp);
          cnp = cnp->siblingPtr;
        }
      }
      break;

    case kStringTId:
      // write the string contents
      if( rp->basePtr == NULL )
        rp->hdr.byteCnt += strlen(np->u.stringVal) + 1;
      else
        _cmJsonSerialCopy( rp, np->u.stringVal, strlen(np->u.stringVal)+1 );

      break;

    case kIntTId:
      // write the int value
      if( rp->basePtr == NULL )
        rp->hdr.byteCnt += sizeof(np->u.intVal);
      else
        _cmJsonSerialCopy(rp,&np->u.intVal,sizeof(np->u.intVal));
      break;

    case kRealTId:
      // write the real value
      if( rp->basePtr == NULL )
        rp->hdr.byteCnt += sizeof(np->u.realVal);
      else
        _cmJsonSerialCopy(rp, &np->u.realVal, sizeof(np->u.realVal));
      break;
  }

  return rc;

}

unsigned      cmJsonSerialByteCount( const cmJsonNode_t* np )
{
  cmJsRC_t rc;
  cmJsSerial_t vr;
  memset(&vr,0,sizeof(vr));

  // make a first pass to determine the size of the buffer
  if((rc = _cmJsonSerializeNode(np,&vr)) != kOkJsRC )
    return rc;

  // increment the buffer size to include the buffer header
  return  (4*sizeof(unsigned)) + vr.hdr.byteCnt;
}

cmJsRC_t      cmJsonSerialize(  const cmJsonNode_t* np, void* buf, unsigned bufByteCnt )
{
  cmJsRC_t     rc = kOkJsRC;
  cmJsSerial_t vr;
  memset(&vr,0,sizeof(vr));

    // setup the serial buffer
  vr.hdr.id      = 'json';
  vr.hdr.byteCnt = bufByteCnt - (2*sizeof(unsigned));
  vr.hdr.version = 0;
  vr.basePtr     = buf;
  vr.nextPtr     = vr.basePtr;
  vr.endPtr      = vr.basePtr + bufByteCnt;

  // write the header recd
  _cmJsonSerialCopy(&vr, &vr.hdr.id,      sizeof(unsigned));  
  _cmJsonSerialCopy(&vr, &vr.hdr.byteCnt, sizeof(unsigned));  
  _cmJsonSerialCopy(&vr, &vr.hdr.nodeCnt, sizeof(unsigned));  
  _cmJsonSerialCopy(&vr, &vr.hdr.version, sizeof(unsigned));  

  // copy the node data into the serial buffer
  if((rc = _cmJsonSerializeNode(np,&vr)) != kOkJsRC )
    return rc;

  vr.basePtr = buf;
  vr.nextPtr = vr.basePtr;
  _cmJsonSerialCopy(&vr, &vr.hdr.id,      sizeof(unsigned));  
  _cmJsonSerialCopy(&vr, &vr.hdr.byteCnt, sizeof(unsigned));  
  _cmJsonSerialCopy(&vr, &vr.hdr.nodeCnt, sizeof(unsigned));  

  return rc;

}

cmJsRC_t      cmJsonSerializeTree( cmJsonH_t h, const cmJsonNode_t* np, void** bufPtrPtr, unsigned* bufByteCntPtr)
{
  cmJsRC_t rc;
  cmJsSerial_t vr;
  memset(&vr,0,sizeof(vr));

  cmJs_t* p      = _cmJsonHandleToPtr(h);

  assert( bufPtrPtr != NULL && bufByteCntPtr != NULL );

  *bufPtrPtr     = NULL;
  *bufByteCntPtr = 0;

  if( np == NULL )
    np = p->rootPtr;
   
  // validate the tree
  if((rc = _cmJsonValidateNode(p,np,np->ownerPtr)) != kOkJsRC )
    return rc;

  // increment the buffer size to include the buffer header
  p->serialByteCnt = cmJsonSerialByteCount(np);

  // allocate the serial buffer memory
  p->serialBufPtr  = cmMemResize( char, p->serialBufPtr, p->serialByteCnt );

  // serialize the tree
  if((rc = cmJsonSerialize(np, p->serialBufPtr, p->serialByteCnt )) != kOkJsRC )
    return rc;

  *bufPtrPtr     = p->serialBufPtr;
  *bufByteCntPtr = p->serialByteCnt;

  return rc;
}

const void* _cmJsonDeserialAdv( cmJsDeserial_t* rp, unsigned n )
{
  const void* vp = rp->nextPtr;
  rp->nextPtr += n;
  assert(rp->nextPtr <= rp->endPtr);
  assert(rp->nodeIdx <= rp->nodeCnt);
  return vp;
}

int _cmJsonDeserialInt( cmJsDeserial_t* rp )
{ return *(const int*)_cmJsonDeserialAdv(rp,sizeof(int)); }

unsigned _cmJsonDeserialUint( cmJsDeserial_t* rp )
{ return *(const unsigned*)_cmJsonDeserialAdv(rp,sizeof(unsigned)); }

double _cmJsonDeserialReal( cmJsDeserial_t* rp )
{ return *(const double*)_cmJsonDeserialAdv(rp,sizeof(double)); }


cmJsRC_t _cmJsonDeserializeNode( cmJs_t* p, cmJsonNode_t* parentPtr, cmJsDeserial_t* rp )
{
  cmJsRC_t      rc           = kOkJsRC;
  unsigned      typeId       = _cmJsonDeserialUint(rp);
  unsigned      childCnt     = 0;
  cmJsonNode_t* newParentPtr = NULL;  

  rp->nodeIdx++;
  
  switch( typeId )
  {
    case kPairTId:
      rc       = _cmJsonCreateNode(p,parentPtr,typeId,&newParentPtr);
      childCnt = 2;
      break;

    case kObjectTId:
    case kArrayTId:
      rc       = _cmJsonCreateNode(p,parentPtr,typeId,&newParentPtr);
      childCnt = _cmJsonDeserialUint(rp);
      break;

    case kStringTId:
      {
        unsigned sn = strlen(rp->nextPtr);
        rc =  _cmJsonCreateString( p, parentPtr, rp->nextPtr, sn,NULL);
        _cmJsonDeserialAdv(rp,sn+1);
      }
      break;

    case kIntTId: 
      {
        int v = _cmJsonDeserialInt(rp);
        rc = _cmJsonCreateInt( p, parentPtr, v, NULL );
      }
      break;

    case kRealTId:
      {
        double v = _cmJsonDeserialReal(rp);
        rc = _cmJsonCreateReal( p, parentPtr, v, NULL );
      }
      break;

    case kNullTId:
      rc = _cmJsonCreateNull( p, parentPtr, NULL );
      break;

    case kTrueTId:
      rc = _cmJsonCreateBool( p, parentPtr, true, NULL );
      break;

    case kFalseTId:
      rc = _cmJsonCreateBool( p, parentPtr, false, NULL );
      break;

    default:
      assert(0);
      break;
  }

  if( rc != kOkJsRC )
    return rc;

  // if the current node is a parent
  if( childCnt > 0 )
  {
    unsigned i;
    assert( newParentPtr != NULL );
    
    for(i=0; i<childCnt; ++i)
      if((rc= _cmJsonDeserializeNode( p, newParentPtr, rp )) != kOkJsRC )
        return rc;
  }

  return rc;

}

cmJsRC_t _cmJsonDeserialize( cmJs_t* p, const void* vbuf, cmJsonNode_t* altRootPtr )
{
  cmJsDeserial_t         r;
  cmJsonNode_t*          rootPtr = altRootPtr == NULL ? p->rootPtr : altRootPtr;
  const char*            buf     = (const char*)vbuf;

  memset(&r,0,sizeof(r));

  r.nextPtr = buf;
  r.endPtr  = buf + (4*sizeof(unsigned)); // the buf must at least contain a header

  // read the buffer header
  unsigned hdrId   = _cmJsonDeserialUint(&r);
  unsigned byteCnt = _cmJsonDeserialUint(&r);
  r.nodeCnt        = _cmJsonDeserialUint(&r);
  /*unsigned version =*/ _cmJsonDeserialUint(&r);
  
  if( hdrId != 'json' )
    return _cmJsonError(p,kSerialErrJsRC,"The buffer does not have the correct header.");

  if( byteCnt < (4*sizeof(unsigned)) )
    return _cmJsonError(p,kSerialErrJsRC,"The buffer is too small to be contain any information.");

  // change the buffer end pointer to the correct size based
  // on the the byte count stored in the buffer
  r.endPtr = buf + byteCnt + (2*sizeof(unsigned));

  return _cmJsonDeserializeNode(p, rootPtr, &r );   

} 

cmJsRC_t      cmJsonDeserialize( cmJsonH_t h, const void* bufPtr, cmJsonNode_t* altRootPtr )
{
  cmJs_t* p = _cmJsonHandleToPtr(h);
  return _cmJsonDeserialize(p,bufPtr,altRootPtr);
}

cmJsRC_t      cmJsonLeafToString( const cmJsonNode_t* np, cmChar_t* buf, unsigned bufCharCnt )
{
  const char* cp = NULL;
  unsigned    n  = 0;

  assert( buf!=NULL && bufCharCnt > 0 );

  switch(np->typeId & kMaskTId )
  {
    case kStringTId:      
      cp = np->u.stringVal==NULL ? "" : np->u.stringVal;
      break;

    case kNullTId:
      cp = "null";
      break;

    case kTrueTId:
      cp = "true";
      break;

    case kFalseTId:
      cp = "false";
      break;
    
    case kIntTId:
      n = snprintf(buf,bufCharCnt,"%i",np->u.intVal)+1;
      break;

    case kRealTId:
      n = snprintf(buf,bufCharCnt,"%f",np->u.realVal)+1;
      break;


    default:
      assert(0); 
      return kInvalidNodeTypeJsRC;

  }

  if( cp != NULL )
  {
    n = strlen(cp)+1;

    if( bufCharCnt < n )
      n  = bufCharCnt;

    strncpy(buf,cp,n);

    /*
    n = strlen(np->u.stringVal)+1;

    if( bufCharCnt < n )
      n  = bufCharCnt;

    strncpy(buf,np->u.stringVal,n);
    */
  }

  buf[bufCharCnt-1]=0;

  assert( n>0 && n < bufCharCnt );

  return n == bufCharCnt ? kBufTooSmallJsRC : kOkJsRC ;
}


cmJsRC_t _cmJsonRptCsvTokErr( cmJs_t* p, unsigned lineNo, cmLexH lexH, const char* iFn )
{
  unsigned n = cmMin(31,cmLexTokenCharCount(lexH));
  char b[n+1];
  strncpy(b,cmLexTokenText(lexH),n);
  b[n]=0;
  return _cmJsonError( p, kCsvErrJsRC, "Unexpected token '%s' during CSV parse on line %i of '%s'.",b,cmLexCurrentLineNumber(lexH),cmStringNullGuard(iFn));

}

cmJsRC_t _cmJsonRptCsvTypeErr( cmJs_t* p, unsigned lineNo, cmLexH lexH, const char* iFn, const char* actualTypeStr, unsigned expTypeId )
{
  unsigned n = cmMin(31,cmLexTokenCharCount(lexH));
  char b[n+1];
  strncpy(b,cmLexTokenText(lexH),n);
  b[n]=0;
  return _cmJsonError( p, kCsvErrJsRC, "Unexpected token '%s' during CSV parse on line %i of '%s'.\nExpected type:%s actual type:%s",b,cmLexCurrentLineNumber(lexH),cmStringNullGuard(iFn),_cmJsonNodeTypeIdToLabel(expTypeId),actualTypeStr);
}



cmJsRC_t      cmJsonFromCSV(  cmJsonH_t h, const char* iFn, cmJsonNode_t* parentNodePtr, cmJsonNode_t** arrayNodePtrPtr )
{
  enum
  {
    kCommaTokId = kUserLexTId+1,
    kIntTokId,
    kRealTokId,
    kTrueTokId,
    kFalseTokId,
    kStringTokId,
    kBoolTokId
  };

  typedef struct field_str
  {
    char*             fieldLabel;
    unsigned          typeId;
    struct field_str* linkPtr;
  } field_t;

  cmJsRC_t      rc           = kOkJsRC;
  cmJs_t*       p            = _cmJsonHandleToPtr(h);
  cmLexH        lexH         = cmLexInit( NULL, 0, 0, p->err.rpt );
  field_t*      fieldList    = NULL;
  cmJsonNode_t* arrayNodePtr = NULL;
  cmJsonNode_t* objNodePtr   = NULL;
  unsigned      lineNo       = 0;
  field_t*      fieldPtr     = NULL;
  field_t*      lp           = NULL;
  unsigned      tokId;
  unsigned      fieldIdx     = 0;

  // validate the init state of the lexer
  if( cmLexIsValid(lexH) == false )
  {
    rc = _cmJsonError( p, kLexErrJsRC, "Lexer initialization failed on CSV parse of '%s'.",cmStringNullGuard(iFn));
    goto errLabel;
  }

  // register CSV specific tokens
  cmLexRegisterToken( lexH, kCommaTokId,  ",");
  cmLexRegisterToken( lexH, kIntTokId,    "int");
  cmLexRegisterToken( lexH, kRealTokId,   "real");
  cmLexRegisterToken( lexH, kTrueTokId,   "true");
  cmLexRegisterToken( lexH, kFalseTokId,  "false");
  cmLexRegisterToken( lexH, kStringTokId, "string");
  cmLexRegisterToken( lexH, kBoolTokId,   "bool");

  // lex the file
  if( cmLexSetFile( lexH, iFn ) != kOkLexRC )
  {
    rc = _cmJsonError( p, kLexErrJsRC, "Lex failed on CSV parse of '%s'.",cmStringNullGuard(iFn));
    goto errLabel;
  }

  // create the parent array
  if((arrayNodePtr = cmJsonCreateArray( h, parentNodePtr )) == NULL )
  {
    rc = _cmJsonError( p, kCsvErrJsRC, "CSV array create failed during parse of '%s'.",cmStringNullGuard(iFn));
    goto errLabel;
  }

  // iterate through the lexer file
  while(((tokId = cmLexGetNextToken(lexH)) != kErrorLexTId) && (tokId != kEofLexTId) )
  {
    unsigned fieldTypeTokId = kInvalidTId;

    switch( cmLexCurrentLineNumber(lexH) )
    {
      // line 1 contains the field type labels (e.g. int,real,string,true,false,bool)
      case 1:
        switch(tokId)
        {
          case kCommaTokId:
            break;

          case kIntTokId:
            fieldTypeTokId = (fieldTypeTokId==kInvalidTId) ? kIntTId : fieldTypeTokId;

          case kRealTokId:
            fieldTypeTokId = (fieldTypeTokId==kInvalidTId) ? kRealTId : fieldTypeTokId;

          case kTrueTokId:
            fieldTypeTokId = (fieldTypeTokId==kInvalidTId) ? kTrueTId : fieldTypeTokId;

          case kFalseTokId:
            fieldTypeTokId = (fieldTypeTokId==kInvalidTId) ? kFalseTId : fieldTypeTokId;

          case kBoolTokId:
            fieldTypeTokId = (fieldTypeTokId==kInvalidTId) ? kFalseTId : fieldTypeTokId;

          case kStringTokId:
            fieldTypeTokId = (fieldTypeTokId==kInvalidTId) ? kStringTId : fieldTypeTokId;

            // create and intitialize a new field 
            field_t* rp = cmMemAllocZ( field_t, 1 );

            rp->fieldLabel = NULL;
            rp->typeId     = fieldTypeTokId;
            rp->linkPtr    = NULL;

            // and add it to the end of the field list
            if( fieldList == NULL )
              fieldList = rp;
            else
              fieldPtr->linkPtr = rp;

            // fieldPtr points to the end of the list
            fieldPtr = rp;

            break;

          default:
            rc =  _cmJsonRptCsvTokErr( p, 1, lexH, iFn );
            goto errLabel;
            
        }
        break;
        
        // line 2 contains the field labels
      case 2:        
        if( fieldIdx == 0 )
          fieldPtr = fieldList;
        ++fieldIdx;

        switch(tokId)
        {
          case kCommaTokId:
            break;

            // all line 2 tokens must be identifiers or q-strings
          case kIdentLexTId:
          case kQStrLexTId:
            if( fieldPtr == NULL )
            {
              rc = _cmJsonError( p, kCsvErrJsRC, "More fields on line 2 than type specifiers on line 1 of '%s'.",cmStringNullGuard(iFn));
              goto errLabel;
            }
            else
            {
              // set the field name in the field list
              unsigned n              = cmLexTokenCharCount(lexH);
              fieldPtr->fieldLabel    = cmMemAllocZ( char, n+1 );
              strncpy(fieldPtr->fieldLabel,cmLexTokenText(lexH),n);
              fieldPtr->fieldLabel[n] = 0;
              fieldPtr                = fieldPtr->linkPtr;
            }
            break;

          default:
            rc =  _cmJsonRptCsvTokErr( p, 2, lexH, iFn );
            goto errLabel;

        }
        break;

        // lines 3 to end of file contain data
      default:
        {
          int      ival = 0;
          cmReal_t rval = 0;
          
          //  if we are starting a new line in the CSV file
          if( lineNo != cmLexCurrentLineNumber(lexH) )
          {
            // verify that field ptr is pointing to the end of the field list
            if( fieldPtr != NULL )
            {
              rc = _cmJsonError( p, kCsvErrJsRC, "Missing columns were detected on line %i in CSV file '%s'.", lineNo, cmStringNullGuard(iFn));
              goto errLabel;
            }
            
            fieldPtr = fieldList;
            lineNo   = cmLexCurrentLineNumber(lexH);

            // create the object to hold the fields on this line
            if((objNodePtr = cmJsonCreateObject( h, arrayNodePtr )) == NULL )
            {
              rc = _cmJsonError( p, kCsvErrJsRC, "Object node create failed on line %i in CSV file '%s'.",lineNo,cmStringNullGuard(iFn));
              goto errLabel;
            } 
          }

          if( tokId == kCommaTokId )
            continue;

          if( fieldPtr == NULL  )
          {
            rc = _cmJsonError( p, kCsvErrJsRC, "More columns than fields on line %i in CSV file '%s'.", lineNo,cmStringNullGuard(iFn));
            goto errLabel;
          }

          // given the tokens type convert the token string into a value 
          switch(tokId)
          {

            case kRealLexTId:
#ifdef CM_FLOAT_REAL
              rval = cmLexTokenFloat(lexH);
#else
              rval = cmLexTokenDouble(lexH);
#endif
              ival = (int)rval;

              if( fieldPtr->typeId == kStringTId )
              {
                rc = _cmJsonRptCsvTypeErr(p, lineNo, lexH, iFn, "numeric", fieldPtr->typeId );
                goto errLabel;

              }
              break;

            case kIntLexTId:
            case kHexLexTId:
              ival = cmLexTokenInt(lexH);
              rval = ival;
              if( fieldPtr->typeId == kStringTId )
              {
                rc = _cmJsonRptCsvTypeErr(p, lineNo, lexH, iFn, "numeric", fieldPtr->typeId );
                goto errLabel;

              }
              break;

            case kTrueTokId:
              ival = 1;
              rval = 1.0;
              break;

            case kFalseTokId:
              ival = 0;
              rval = 0.0;
              break;

            case kIdentLexTId:
            case kQStrLexTId:
              if( fieldPtr->typeId != kStringTId )
              {
                rc = _cmJsonRptCsvTypeErr(p, lineNo, lexH, iFn, "string", fieldPtr->typeId );
                goto errLabel;
              }
              break;

            default:
              rc =  _cmJsonRptCsvTokErr( p, lineNo, lexH, iFn );
              goto errLabel;
          }


          // create the pair object from the current field label and value
          switch(fieldPtr->typeId)
          {
            case kIntTId:
              rc = cmJsonInsertPairInt( h, objNodePtr, fieldPtr->fieldLabel, ival );
              break;

            case kRealTId:
              rc = cmJsonInsertPairReal( h, objNodePtr, fieldPtr->fieldLabel, rval );
              break;

            case kTrueTId:
            case kFalseTId:
              rc = cmJsonInsertPairBool( h, objNodePtr, fieldPtr->fieldLabel, ival );
              break;

            case kStringTId:
              rc = cmJsonInsertPairStringN( h, objNodePtr, fieldPtr->fieldLabel, cmLexTokenText(lexH), cmLexTokenCharCount(lexH) );
              break;

            default:
              { 
                assert(0); 
                goto errLabel;
              }
          }

          if( rc != kOkJsRC )
            goto errLabel;
          

          fieldPtr = fieldPtr->linkPtr;
        }
        break;

    }
  }

 errLabel:
  if( cmLexFinal(&lexH) != kOkLexRC )
  {
    rc = _cmJsonError( p, kLexErrJsRC, "Lexer finalize failed on CSV parse of '%s'.",cmStringNullGuard(iFn));
    goto errLabel;
  }

  lp = fieldList;
  while( lp!=NULL )
  {
    field_t* pp = lp->linkPtr;
    cmMemPtrFree(&lp->fieldLabel);
    cmMemPtrFree(&lp);
    lp = pp;
  }

  if( rc != kOkJsRC )
    _cmJsonRemoveNode( p, arrayNodePtr, true, true );

  if( rc == kOkJsRC && arrayNodePtrPtr != NULL )
    *arrayNodePtrPtr = arrayNodePtr;

  return rc;
}

cmJsRC_t      cmJsonToCSV(   cmJsonH_t h, const char* oFn, const cmJsonNode_t* arrayNodePtr )
{
  assert( arrayNodePtr->typeId == kArrayTId );

  typedef struct r_str
  {
    const char*   fieldLabel;
    unsigned      typeId;
    struct r_str* linkPtr;
  } field_t;

  cmJs_t*  p         = _cmJsonHandleToPtr(h);
  cmJsRC_t rc        = kOkJsRC;
  unsigned arrayCnt  = cmJsonChildCount(arrayNodePtr);
  field_t* fieldList = NULL;
  FILE*    fp        = NULL;
  field_t* lp        = NULL;
  unsigned i,j;

  // for each object in the array
  for(i=0; i<arrayCnt; ++i)
  {    
    const cmJsonNode_t* objNodePtr = cmJsonArrayElementC( arrayNodePtr, i );

    assert( objNodePtr->typeId == kObjectTId );

    // for each pair in the object
    for(j=0; j<cmJsonChildCount(objNodePtr); ++j)
    {
      const cmJsonNode_t* pairNodePtr = cmJsonArrayElementC( objNodePtr, j );

      const char*         pairLabel   = cmJsonPairLabel(pairNodePtr);

      lp = fieldList;

      // find this field in the field list
      for(; lp != NULL; lp = lp->linkPtr )
        if( strcmp(lp->fieldLabel,pairLabel) == 0)
        {
          unsigned typeId = cmJsonPairTypeId(pairNodePtr);

          switch( typeId )
          {
            case kIntTId:
            case kRealTId:
            case kTrueTId:
            case kFalseTId:
            case kStringTId:
              break;
            default:
              rc = _cmJsonError( p, kInvalidNodeTypeJsRC, "Field '%s' has type '%s' which cannot be written by cmJsonToCSV().",cmStringNullGuard(pairLabel),_cmJsonNodeTypeIdToLabel(typeId) );
              goto errLabel;
              
          }

          if( typeId != lp->typeId )
          {
            rc = _cmJsonError( p, kInvalidNodeTypeJsRC, "All nodes for a field label '%s' do not have the same type. '%s' != '%s'",cmStringNullGuard(pairLabel), _cmJsonNodeTypeIdToLabel(typeId), _cmJsonNodeTypeIdToLabel(lp->typeId) );
            goto errLabel;
          }

          break;
        }

      // if this field was not found then insert it
      if( lp == NULL )
      {
        field_t* rp = (field_t*)cmLHeapAlloc(p->heapH,sizeof(field_t));
        rp->fieldLabel = pairLabel;
        rp->linkPtr    = fieldList;
        rp->typeId     = cmJsonPairTypeId(pairNodePtr);
        fieldList      = rp;
      }
    }
  }

  // create the output file
  if((fp = fopen(oFn,"wt")) == NULL )
  {
    rc = _cmJsonError( p, kFileCreateErrJsRC, "CSV file '%s' create failed.", oFn );
    goto errLabel;
  }

  // write the field type
  lp = fieldList;
  for(; lp!=NULL; lp=lp->linkPtr)
  {
    fprintf(fp,"%s", _cmJsonNodeTypeIdToLabel(lp->typeId));

    if( lp->linkPtr != NULL )
      fprintf(fp,",");
  }
  fprintf(fp,"\n");
  
  // write the field label
  lp = fieldList;
  for(; lp!=NULL; lp=lp->linkPtr)
  {
    fprintf(fp,"\"%s\"", lp->fieldLabel);

    if( lp->linkPtr != NULL )
      fprintf(fp,",");
  }
  fprintf(fp,"\n");
    
  
  // for each object in the array
  for(i=0; i<arrayCnt; ++i)
  {
    unsigned            j;
    const cmJsonNode_t* objNodePtr = cmJsonArrayElementC( arrayNodePtr, i );
    unsigned            fieldCnt   = cmJsonChildCount(objNodePtr);

    lp = fieldList;

    // for each field ...
    for(j=0; lp!=NULL; lp=lp->linkPtr,++j)
    {
      cmJsonNode_t* valNodePtr;

      // ... locate the pair given the field label
      if((valNodePtr = cmJsonFindValue(h,lp->fieldLabel, objNodePtr, lp->typeId )) == NULL)
      {
        // no pair was found for the field label - output a NULL value
        switch( lp->typeId )
        {
          case kIntTId:
            fprintf(fp,"%i",0);
            break;

          case kRealTId:
            fprintf(fp,"%f",0.0);
            break;

          case kTrueTId:
          case kFalseTId:
            fprintf(fp,"%i",0);
            break;

          case kStringTId:
            fprintf(fp,"\"\"");
            break;

          default:
            assert(0);
            break;
        }

        //rc = _cmJsonError( p, kNodeNotFoundJsRC,"No field with label '%s' was found.", lp->fieldLabel);
        //goto errLabel;
      }
      else
      {
        switch( valNodePtr->typeId )
        {
          case kIntTId:
            fprintf(fp,"%i", valNodePtr->u.intVal);
            break;

          case kRealTId:
            fprintf(fp,"%e", valNodePtr->u.realVal);
            break;

          case kTrueTId:
          case kFalseTId:
            fprintf(fp,"%i", valNodePtr->u.boolVal);
            break;

          case kStringTId:
            fprintf(fp,"\"%s\"", valNodePtr->u.stringVal);
            break;

          default:
            assert(0);
            break;
        }
      }
      if( j < fieldCnt-1 )
        fprintf(fp,",");
      
    }
    
    fprintf(fp,"\n");
    
  }
  
 errLabel:

  if( fp != NULL )
    fclose(fp);

  lp = fieldList;
  while( lp!=NULL )
  {
    field_t* pp = lp->linkPtr;
    cmLHeapFree(p->heapH,lp);
    lp = pp;
  }

  return rc;
}


void  _cmJsonPrintIndent( cmRpt_t* rpt, unsigned indent )
{
  if( indent )
  {
    char spaces[indent+1];
    spaces[indent]=0;
    memset(spaces,' ',indent);
    cmRptPrint(rpt,spaces);
  }
}


void  _cmJsonPrintNode( const cmJsonNode_t* np, cmRpt_t* rpt, unsigned indent )
{
  unsigned    childCnt    = 0;
  char eoObjStr[]         = "}";
  char eoArrStr[]         = "]";
  char eoPairStr[]        = "\n";
  char commaStr[]         = ",\n";
  char colonStr[]         = ": ";
  const char* eleStr      = NULL;
  const char* lastEleStr  = NULL;
  const char* eoStr       = NULL;
  unsigned    localIndent = 0;


  switch(np->typeId)
  {
    case kObjectTId:
      cmRptPrint(rpt,"\n");
      _cmJsonPrintIndent(rpt,indent);      
      cmRptPrint(rpt,"{\n");
      childCnt    = cmJsonChildCount(np);
      eoStr       = eoObjStr;
      localIndent = 2;
      break;

    case kArrayTId:
      cmRptPrint(rpt,"\n");
      _cmJsonPrintIndent(rpt,indent);      
      cmRptPrint(rpt,"[\n");
      childCnt    = cmJsonChildCount(np);
      eoStr       = eoArrStr;
      localIndent = 2;
      eleStr      = commaStr;
      lastEleStr  = "\n";
      break;

    case kPairTId:
      childCnt = cmJsonChildCount(np);
      eleStr = colonStr;
      eoStr  = eoPairStr;
      break;

    case kStringTId:
      {
        const char* fmt0 = "\"%s\" ";
        const char* fmt1 = "%s";
        const char* fmt  = fmt0;

        // if this string is the label part of a pair
        if( np->u.stringVal != NULL && np->ownerPtr != NULL && cmJsonIsPair(np->ownerPtr) && np->ownerPtr->u.childPtr == np )
        {
          // and the label has no white space 
          char* cp = np->u.stringVal;
          while( *cp!=0 && isspace(*cp)==false )
            ++cp;
          
          // then print without quotes
          if( *cp == 0 )
            fmt = fmt1;
        }
        cmRptPrintf(rpt,fmt,np->u.stringVal==NULL ? "" : np->u.stringVal);
      }
      break;

    case kIntTId:
      cmRptPrintf(rpt,"%i ",np->u.intVal);
      break;

    case kRealTId:
      cmRptPrintf(rpt,"%f ",np->u.realVal);
      break;

    case kNullTId:
      cmRptPrint(rpt,"null ");
      break;

    case kTrueTId:
      cmRptPrint(rpt,"true ");
      break;

    case kFalseTId:
      cmRptPrint(rpt,"false ");
      break;
  }

  if( childCnt )
  {
    indent += localIndent;

    unsigned      i;
    cmJsonNode_t* cnp = np->u.childPtr;

    for(i=0; i<childCnt; ++i)
    {
      assert(cnp != NULL);

      if( np->typeId != kPairTId )
        _cmJsonPrintIndent(rpt,indent);

      _cmJsonPrintNode(cnp,rpt,indent);

      cnp = cnp->siblingPtr;

      if( i < childCnt-1 && eleStr != NULL )
        cmRptPrint(rpt,eleStr);

      if( i == childCnt-1 && lastEleStr != NULL )
        cmRptPrint(rpt,lastEleStr);
    }

    indent -= localIndent;
  }

  if( eoStr != NULL )
  {
    _cmJsonPrintIndent(rpt,indent);
    cmRptPrint(rpt,eoStr);
  }

}


void      cmJsonPrintTree( const cmJsonNode_t* np, cmRpt_t* rpt )
{
  _cmJsonPrintNode(np,rpt,0);
}

void _cmJsPrintFile(void* cmRptUserPtr, const cmChar_t* text)
{
  cmFileH_t* hp = (cmFileH_t*)cmRptUserPtr;
  cmFilePrint(*hp,text);
}

cmJsRC_t      cmJsonWrite( cmJsonH_t h, const cmJsonNode_t* np, const cmChar_t* fn )
{
  cmRpt_t   rpt;  
  cmFileH_t fh = cmFileNullHandle;
  cmJs_t*   p = _cmJsonHandleToPtr(h);

  if( np == NULL )
    np = cmJsonRoot(h);

  // create the output file
  if( cmFileOpen(&fh,fn,kWriteFileFl,p->err.rpt) != kOkFileRC )
    return _cmJsonError( p, kFileCreateErrJsRC, "Output file '%s' create failed.", fn );
  
  // setup a reporter to write to the file
  cmRptSetup(&rpt,_cmJsPrintFile,_cmJsPrintFile,&fh);

  // print the tree to the file
  cmJsonPrintTree(np,&rpt);

  // close the file
  if( cmFileClose(&fh) != kOkFileRC )
    return _cmJsonError( p, kFileCloseErrJsRC, "Output file  '%s' close failed.", fn );

  return kOkJsRC;
}
  
cmJsRC_t      cmJsonReport( cmJsonH_t h )
{
  cmJsRC_t rc;
  cmJs_t* p = _cmJsonHandleToPtr(h);

  if((rc = cmJsonValidateTree(h)) != kOkJsRC )
    return rc;

  if(p->rootPtr != NULL )
    _cmJsonPrintNode(p->rootPtr,p->err.rpt,0);

  return rc;
}

cmJsRC_t cmJsonErrorCode( cmJsonH_t h )
{
  cmJs_t* p = _cmJsonHandleToPtr(h);
  return p->rc;
}

 void    cmJsonClearErrorCode( cmJsonH_t h )
 {
   cmJs_t* p = _cmJsonHandleToPtr(h);
   p->rc = kOkJsRC;
 }

void _cmJsonTestVPrint( void* rptDataPtr, const char* fmt, va_list vl )
{
  vfprintf(stdout,fmt,vl);
}

void _cmJsonTestPrint( void* userPtr, const cmChar_t* text )
{
  fputs(text,stdout);
}


//{ { label:cmJsonEx }
//(
// cmJsonTest() demonstrates some JSON tree operations.
//)
//[
cmJsRC_t cmJsonTest( const char* fn, cmCtx_t* ctx )
{
  cmJsRC_t      rc  = kOkJsRC;
  cmJsRC_t      rc1 = kOkJsRC;
  cmJsonH_t     h   = cmJsonNullHandle;
  cmJsonH_t     h1  = cmJsonNullHandle;
  void*         sbp = NULL;
  unsigned      sbn = 0;
  cmJsonNode_t* np  = NULL;
  cmRpt_t*      rpt = &ctx->rpt;

  // initialize an empty JSON tree
  if((rc = cmJsonInitialize(&h,ctx)) != kOkJsRC )
    goto errLabel;

  // load the tree from a file
  if((rc = cmJsonParseFile(h,fn,NULL)) != kOkJsRC )
    goto errLabel;

  // print the tree
  cmJsonReport(h);

  // find an array member named 'mem14'
  if((np = cmJsonFindValue(h,"mem14",NULL,kArrayTId)) == NULL )
    cmRptPrint(rpt,"'mem14' not found.\n");
  else
  {
    cmRptPrint(rpt,"'mem14' found.\n");
    cmJsonPrintTree(np,rpt);
  }

  // remove the array node from the tree
  cmJsonRemoveNode(h,np, true);
  cmRptPrint(rpt,"mem14 removed.\n");

  // print the tree with the array node removed
  cmJsonPrintTree( cmJsonRoot(h), rpt );

  // serialize the tree into a dynamically allocated
  // buffer sbp[sbn].
  if((rc = cmJsonSerializeTree(h,NULL,&sbp,&sbn)) != kOkJsRC )
    goto errLabel;
  else
    cmRptPrint(rpt,"***Serialize Ok.****\n");

  // initialize an empty JSON tree
  if((rc = cmJsonInitialize(&h1,ctx)) != kOkJsRC )
    goto errLabel;

  // deserialize sbp[sbn] into the empty tree
  if((rc = cmJsonDeserialize(h1,sbp,NULL)) != kOkJsRC )
    goto errLabel;
  else
  {
    cmJsonPrintTree( cmJsonRoot(h1),rpt);
    cmRptPrint(rpt,"***Deserialize Ok.****\n");
  }
  
  // find an member node named 'mem5'
  if((np = cmJsonFindValue(h,"mem5",NULL,0)) == NULL )
    cmRptPrint(rpt,"mem5 not found.");

  // merge two sub-trees
  if( cmJsonMergeObjectNodes( h, np->u.childPtr, 
      np->u.childPtr->siblingPtr) != kOkJsRC )
  {
    cmRptPrint(rpt,"merge failed.");
  }
  else
  {
    cmJsonReport(h);
  }

 errLabel:

  // release the JSON trees
  rc = cmJsonFinalize(&h);
  rc1 = cmJsonFinalize(&h1);

  return rc == kOkJsRC ? rc1 : rc;
}
//]
//}
