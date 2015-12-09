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
#include "cmXml.h"

/*
file     -> decl doctype node
decl     -> "<?" attr-list "?>"
doctype  -> "<!DOCTYPE" dt-text ">"
node     -> beg-node node-body end-node
         |  "<!--" cmmt-text "-->"

node-body -> data-text
          |  node
  
beg-node   -> "<" tag-label  attr-list ">"
end-node   -> "<" tag-label "/>"
attr-list  -> attr*
attr       -> attr-label "=" qstring

attr-label -> A string of characters ending with an '=' or <space>.
              Attribute labels may not contain '<' or '>'.

tag-label  -> A string of characters ending with:
                <space>, '>' or '/>'.
              Tag labels may not contain '<' or '>'.

data-text  -> A string of characters ending with '<'.

dt-text    -> A string of characters beginning with a non-whitespace
               and ending with '>'

cmmt-text  -> A string of characters ending with '-->'

*/  
 
cmXmlH_t cmXmlNullHandle = cmSTATIC_NULL_HANDLE;

typedef struct
{
  cmErr_t      err;   // 
  cmLHeapH_t   heapH; // linked heap stores all node memory
  cmLexH       lexH; 
  cmXmlNode_t* root;
} cmXml_t;

enum
{
  kTagBegLexTId = kUserLexTId+1,
  kTagEndLexTId,
  kDeclBegLexTId,
  kDeclEndLexTId,
  kSpclBegLexTId,
  kDocTypeLexTId,
  kCmmtBegLexTId,
  kCmmtEndLexTId,
  kEqualLexTId
};

cmXmlToken_t _cmXmlTokenArray[] = 
{
  { kTagBegLexTId = kUserLexId+1,  "<" },
  { kTagEndLexTid,  ">" },
  { kDeclBegLexTId, "<?" },
  { kDeclEndLexTid, "?>" },
  { kSpclBegLexTId, "<!" },
  { kDocTypeLexTId, "<!DOCTYPE" },
  { kCmmtBegLexTId, "<!--" },
  { kCmmtEndLexTid, "-->" },
  { kEqualLexTid,   "=" },
  { kErrorLexTId,""}  
};

// Match a tag label.  
// A string ending with a <space> or '>'
unsigned cmLexTagLabelMatcher( const cmChar_t* cp, unsigned cn )
{
  for(i=0; i<cn; ++i)
    if( cp[i] == '>' || isspace(cp[i]) )
      break;
  return i>0 ? i-1 : 0;  
}

unsigned cmLexStringMatcher( const cmChar_t* cp, unsigned cn )
{
  for(i=0; i<cn; ++i)
  {
    if( cp[i] == ' ')
      break;
    
    if( cp[i] == '<' )
      break;

  }
  return i>0 ?   
}


cmXml_t* _cmXmlHandleToPtr( cmXmlH_t h )
{
  cmXml_t* p = (cmXml_t*)h.h;
  assert( p != NULL );
  return p;
}

cmXmlRC_t _cmXmlFree( cmXml_t* p )
{
  cmLHeapDestroy( &p->heapH );
  cmLexDestroy( &p->lexH );
}

cmXmlRC_t _cmXmlParse( cmXml_t* p, const cmChar_t* fn )
{
  cmXmlRC_t rc = kOkXmlRC;
  
  if( cmLexReset( p->lexH ) != kOkLexRC )
  {
    rc = cmErrMsg(&p->err,kLexErrXmlRC,"Lexer reset failed.");
    goto errLabel:
  }

  if( cmLexSetFile( p->lexH, fn ) != kOkLexRC )
  {
    rc = cmErrMsg(&p->err,kLexErrXmlRC,"Lexer parse failed on '%s'.",cmStringNullGuard(fn));
    goto errLabel;
  }

  unsigned tokId;
  
  while((tokId = cmLexGetNextToken( cmLexH h )) != kEofRC && tokId != kErrorLexTId )
  {
    switch(tokId)
    {
      case kTagBegLexTId:
      case kTagEndLexTid:
      case kEqualLexTId:
      case kQStrLexTId:
    }
  }

 errLabel:
  return rc;
}

cmXmlRC_t cmXmlAlloc( cmCtx_t* ctx, cmXmlH_t* hp, const cmChar_t* fn )
{
  cmXmlRC_t rc = kOkXmlRC;
  cmXml_t*   p = NULL;
  
  // finalize before initialize 
  if((rc = cmXmlFree(hp)) != kOkXmlRC )
    return rc;

  // allocate the main object record
  if((p = cmMemAllocZ( cmXml_t, 1 )) == NULL )
    return cmErrMsg(&ctx->err,kMemAllocErrXmlRC,"Object memory allocation failed.");

  cmErrSetup(&p->err,&ctx->rpt,"XML Parser");

  // allocate the linked heap mgr
  if( cmLHeapIsValid(p->heapH = cmLHeapCreate(1024,ctx)) == false )
  {
    rc = cmErrMsg(&p->err,kMemAllocErrXmlRC,"Linked heap object allocation failed.");
    goto errLabel;
  }

  // allocate the lexer
  if(cmLexIsValid(p->lexH = cmLexInit(NULL,0,0,&ctx->rpt)) == false )
  {
    rc = cmErrMsg(&p->err,kLexErrXmlRC,"Lex allocation failed.");
    goto errLabel;
  }

  // register xml specific tokens with the lexer
  for(i=0; _cmXmlTokenArray[i].id != kErrorLexTId; ++i)
  {
    cmRC_t lexRC;
    if( (lexRC = cmLexRegisterToken(p->lexH, _cmXmlTokenArray[i].id, _cmXmlTokenArray[i].text )) != kOkLexRC )
    {
      rc = cmErrMsg(&p->err,kLexErrXmlRC,"Lex token registration failed for:'%s'.",_cmXmlTokenArray[i].text );
      goto errLabel;
    }
  }
  
  hp->h = p;
  
 errLabel:
  if(rc != kOkXmlRC )
    _cmXmlFree(p);
    
  return rc;
}

cmXmlRC_t cmXmlFree(  cmXmlH_t* hp )
{
  cmXmlRC_t rc = kOkXmlRC;
  
  if( hp!=NULL || cmXmlIsValid(*hp)==false )
    return kOkXmlRC;

  cmXml_t* p = _cmXmlHandleToPtr(*hp);

  if((rc = _cmXmlFree(p)) != kOkXmlRC )
    return rc;

  hp->h = NULL;
  
  return rc;  
}
  
bool      cmXmlIsValid( cmXmlH_t h )
{ return h.h != NULL; }

  
cmXmlRC_t cmXmlParse( cmXmlH_t h, const cmChar_t* fn )
{
}

cmXmlRC_t cmXmlClear( cmXmlH_t h )
{
}
