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
  
beg-node   -> "<" tag-label  attr-list {"/"} ">"
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

/*


t = get_next_attr_token(p,end_char, tn* )
{

}

parse_attr_list(p,end_char)
{
}

read_beg_tag(p)
{
   c = goto_next_non_white_char(p)

   if( c != '<' )
     error();

   c = goto_next_non_white_char(p)

   if c == '?'
   {
      end_tag_str = "?";
      if( scan_past(p,"xml") == false )
        error();

      parse_attr_list(p,'?');
   }

   if c == '!'
   {
      if( scan_past(p,"--") )
      {
         if(go_past(p,"-->")==false)
           error();
      }

      if( scan_past(p,"DOCTYPE") )
      {
          while( s = get_next_attr_token(p,'>') != NULL )
             store_attr(p,s,"");
      }
   }
    
}

read_body( p )
{
  c = goto_next_non_white_char(p);

  if c == '<'
    read_node(p)
  else
    read_data_string(p)
}

n = read_node( p )
{
   t = read_beg_tag(p);

   if( is_beg_tag(t) )
   {
      read_body()
      read_end_tag()
   }
}

 */
 
cmXmlH_t cmXmlNullHandle = cmSTATIC_NULL_HANDLE;

typedef struct
{
  cmErr_t      err;     // 
  cmLHeapH_t   heapH;   // linked heap stores all node memory
  
  cmChar_t*    b;       // base of the text buffer
  unsigned     bn;      // length of the text buffer in characters
  cmChar_t*    c;       // current lexer position
  
  cmXmlNode_t* root;    // root XML tree node
  cmXmlNode_t* decl;    // xml declaratoin node <? ... ?>
  cmXmlNode_t* doctype; // DOCTYPE  node

  cmXmlNode_t* stack;   // parsing stack
} cmXml_t;


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

cmXmlRC_t _cmXmlSyntaxError( cmXml_t* p )
{
  return _cmErrMsg(&p->err,kSyntaxErrorXmlRC,"Syntax error on line '%i.",p->line);
}

cmXmlNode_t* _cmXmlNodeAlloc( cmXml_t* p, unsigned flags, const cmChar_t* label, unsigned labelN )
{
  cmXmlNode_t* np = cmLhAllocZ(p->heapH,cmXmlNode_t,1);

  if( cmIsFlag(kNormalXmlFl) )
  {  
    if( p->root == NULL )
      p->root = np;

    if( p->stack == NULL )
      p->stack = np;
    else
    {
      np->parent = p->stack;
      
      if( p->stack->children == NULL )
        p->stack->children = np;
      else
      {
        cmXmlNode_t* n0p = NULL;
        cmXmlNode_t* n1p = p->stack->children;
        
        for(; n1p != NULL; n1p=n1p->sibling )
          n0p = n1p;

        n0p->sibling = np;
      }
    }
  }
  else
  {
    if( cmIsFlag(kDeclXmlFl) )
      p->decl = np;
    else
    {
      if( cmIsFlag(kDoctypeXmlF0 ) )
        p->doctype = np;
      else
      {
        _cmXmlSyntaxError(p);
        return NULL;
      }
    }
  }
  
  if( label != NULL )
    np->label = cmLhAllocStrN(p->heapH,label,labelN);

  return np;
}

cmXmlNode_t* _cmXmlAttrAlloc( cmXml_t* p, cmXmlNode_t* np, const cmChar_t* label, unsigned labelN, const cmChar_t* value, unsigned valueN )
{
  cmXmlAttr_t* ap = cmLhAllocZ(p->heapH, cmXmlAttr_t,1);

  if( label != NULL && labelN > 0 )
    ap->label = cmLhAllocStr(p->heapH,label,labelN);

  if( value != NULL and valueN > 0 )
    ap->value = cmLhAllocStr(p->attrH,value,valueN);
  
  ap->link  = np->attr;
  np->attr  = ap;
  
  return np;
}


bool _cmXmlIsEof( cmXml_t* p )
{  return p->c >= p->b + p->bn; }

// Return false if EOF is encountered
bool _cmXmlAdvance( cmXml_t* p )
{
  if( _cmXmlIsEof(p) )
    return false;

  p->c += 1;

  if( *p->c == '\n' )
    p->line += 1;
  
  return true;
}

// Advance the cursor to the next non-white char
// Return a pointer to a non-space character.
// Return NULL if the EOF is encountered.
const cmChar_t*  _cmXmlAdvanceToNextNonWhite( cmXml_t* p )
{
  if( _cmXmlIsEof(p) )
    return NULL;
  
  while( isspace(*p->c) )
    if( _cmXmlAdvance(p) == false )
      return NULL;

  return p->c;
}

// Advance to the next white space character or 'c'.
// Returns a pointer to a white space or 'c'.
const cmChar_t*  _cmXmlAdvanceToNextWhiteOr( cmXml_t* p, cmChar_t c0, cmChar_t c1 )
{
  if( _cmXmlIsEof(p) )
    return NULL;

  while( isspace(*p->c)==false && *p->c!=c0 && *p->c!=c1 )
    if(_cmXmlAdvance(p) == false )
      return NULL;

  return p->c;
}

// Advance past leading white space followed by 's'.
// Note that 's' is expected to immediately follow any leading white space.
// Returns a pointer to the character after 's'.
// Returns NULL if 'c' is not encountered
const cmChar_t* _cmXmlAdvancePast( cmXml_t* p, const cmChar_t* s )
{
  if( _cmXmlIsEof(p) )
    return NULL;

  while( isspace(*p->c) )
    if( _cmXmlAdvance(p) == false )
      return NULL;

  for(; *s && *p->c == *s; ++s )
    if( _cmXmlAdvance(p) == false )
      return NULL;

  return *s==0 ? p->c : NULL; 
}

// Advance past the current character and then 
// advance to the next occurrence of 's' and return
// a pointer to the last char in 's'.
const cmChar_t* _cmXmlAdvanceToNext( cmXml_t* p, cmChar_t* s )
{
  unsigned i = 0;
  unsigned n = strlen(s);

  while( _cmXmlAdvance(p) )
  {
    if( *p->c != s[i] )
      i = 0;
    else
    {
      i+= 1;
      if( i == n )
        break;
    }
  }
  return p->c;
}

// Return the character following the current character.
const cmChar_t* _cmXmlAdvanceOne( cmXml_t* p )
{
  if( _cmXmlIsEof(p) )
    return NULL;

  p->c += 1;

  return _cmXmlIsEof(p) ? NULL : p->c;
}

cmXmlRC_t  _cmXmlParseAttr( cmXml_t* p, cmChar_t endChar )
{
  cmXmlRC_t       rc = kOkXmlRC;
  const cmChar_t* l0 = NULL;
  const cmChar_t* l1 = NULL;
  const cmChar_t* v0 = NULL;
  const cmChar_t* v1 = NULL;

  // advance to the next label
  if(( l0 = _cmXmlAdvanceToNextNonWhite(p)) == NULL )
    return _cmXmlSyntaxError(p);

  // if the 'endChar' was encountered
  if( *p->c == endChar )
    return kOkXmlRC;
  
  // advance past last character in label
  if((l1 = _cmXmlAdvanceToNextWhiteOr(p,'=',' ')) == NULL )
    return _cmXmlSyntaxError(p);

  // advance past the next '='
  if( _cmXmlAdvancePast(p,"=") == NULL )
    return _cmXmlSyntaxError(p);
  
  // advance to the next non-white character
  if( (v0 = _cmXmlAdvanceToNextNonWhite(p)) == NULL )
    return _cmXmlSyntaxError(p);

  // the first character in the value must be a single quote
  if( *p->c != '\'' )
    return _cmXmlSyntaxError(p);
  
  // advance to the next single quote
  if( (v1 = _cmXmlAdvanceToNext(p,"'")) == NULL )
    return _cmXmlSyntaxError(p);

  // advance past the ending single quote
  if( _cmXmlAdvanceOne(p) == NULL )
    return _cmXmlSyntaxError(p);

  // p->c now points just past the ending single quote
  return rc;
}

cmXmlRC_t _cmXmlParseAttrList( cmXml_t* p, cmChar_t endChar )
{
  cmXmlRC_t rc = kOkXmlRC;

  
  while( *p->c != endChar && *p->c != '>' )
    if((rc = _cmXmlParseAttr(p,endChar)) != kOkXmlRC )
      break;

  if( *p->c == endChar )
  {
    if( endChar = '/' )
    {
      // this is a simple node
    }
    
    if( _cmXmlAdvanceOne(p) == NULL )
      return _cmXmlSyntaxError(p);
  }
  
  if( *p->c != '>' )
    return _cmXmlSyntaxError(p);
  
  if( _cmXmlAdvancePast(p,">") == NULL )
    return _cmXmlSyntaxError(p);

  // p->c is now past the ending '>'
  
  return rc;
}

cmXmlRC_t _cmXmlParseDoctypeToken( cmXml_t* p, cmXmlNode_t* np )
{
  const cmChar_t* t0 = NULL;
  const cmChar_t* t1 = NULL;

  // advance to the first char in the doctype token
  if((t0 = _cmXmlAdvanceToNextNonWhite(p) ) == NULL )
  {
    return _cmXmlSyntaxError(p);
  }

  // if the end of the tag was encountered
  if( *p->c == '>' )
      return kOkXmlRC;
    

  // if the token begins with a quote
  if( *p->c == '\'' )
  {
    if((t1 = _cmXmlAdvanceToNext(p,"'")) == NULL )
      return _cmXmlSyntaxError(p);
  }
  else
  {
    if((t1 = _cmXmlAdvanceToNextWhiteOr(p,'>',' ')) == NULL )
      return _cmXmlSyntaxError(p);
  }

  // t1 and p->c now point just past the last character in the token

  return rc;  
}

cmXmlRC_t _cmXmlParseDoctype( cmXml_t* p )
{
  cmXmlRC_t rc = kOkXmlRC;
  cmXmlNode_t* np;
  
  if((np = _cmXmlNodeAlloc(p,kDoctypeXmlFl,"DOCTYPE",strlen("DOCTYPE"))) == NULL )
    return cmErrLastRC(&p->err);
  
  while( *p->c != '>' )
    if((rc = _cmXmlParseDoctypeToken(p,np)) != kOkXmlRC )
      break;

  return rc;
}

// Node tags are tags that begin with a '<' and are not
// followed by any special character.
cmXmlRC_t _cmXmlParseNodeTag( cmXml_t* p )
{
  cmXmlRC_t       rc = kOkXmlRC;
  const cmChar_t* l0 = NULL;
  const cmChar_t* l1 = NULL;

  // Advance to the first character of the tag label.
  if((l0 = _cmXmlAdvanceToNextNonWhite(p)) == NULL )
    return _cmXmlSyntaxError(p);

  // Advance to the last character following the tag label.
  if((l1 = _cmXmlAdvanceToNextWhiteOr(p,'/','>')) == NULL )
    return _cmXmlSyntaxError(p);

  // look for attributes
  if((rc = _cmXmlParseAttrList(p,'/')) != kOkXmlRC )
      return _cmXmlSyntaxError(p);

  // p->c is now past the ending '>'
  
  return rc;
}

cmXmlRC_t _cmXmlReadEndTag( cmXml_t* p )
{
  const cmChar_t* l0 = NULL;
  const cmChar_t* l1 = NULL;

  assert( *p->c == '/' );

  // advance past the '/'
  if(( l0 = _cmXmlAdvanceOne(p)) == NULL )
    return _cmXmlSyntaxError(p);

  // advance to the ending '>'
  if(( l1 = _cmXmlAdvanceToNext(p,">")) == NULL )
    return _cmXmlSyntaxError(p);

  // advance past the 
  if( _cmXmlAdvanceOne(p) == NULL )
    return _cmXmlSyntaxError(p);

  // trim trailing space on label
  l1 -= 1;
  while( l1>l0 && isspace(*l1) )
    --l1;

  // verify that the label has a length
  if( l0 == l1 )
    return _cmXmlSyntaxError(p);

  assert( !isspace(*l1) );

  // the label should match the node on the top of the stack
  if( strncmp( p->stack->label, l0, (l1-l0)+1 ) )
    return _cmXmlSyntaxError(p);

  // since we just parsed an end-tag there should be at least one node on the stack
  if( p->stack == NULL )
    return _cmXmlSyntaxError(p);

  // pop the stack
  p->stack = p->stack->parent;
  
  return kOkXmlRC;  
}
  


// 
cmXmlRC_t  _cmXmlReadTag( cmXml_t* p, cmXmlNode_t** newNodeRef )
{
  cmXmlRC_t rc = kOkXmlRC;

  assert(newNodeRef != NULL );
  *newNodeRef = NULL;
  
  // No leading '<' was found 
  if( _cmXmlAdvancePast(p,"<") == NULL )
  {
    // error or EOF
    return NULL;  
  }

  // examine the character following the opening '<'
  switch( *p->c )
  {
    // node end tag
    case '/':
      return _cmXmlReadEndTag(p);
    
    // declaration tag
    case '?':
      if( _cmXmlAdvancePast(p,"xml") == NULL )
        return _cmXmlSyntaxError(p);

      if( _cmXmlNodeAlloc(p,kDeclXmlFl, "xml",strlen("xml") ) == NULL )
        return cmErrLastRC(&p->err);
      
      if((rc = _cmXmlParseAttrList(p,'?')) != kOkXmlRC )
        return rc;
      
      break;
      
    case '!':
      switch( *(p->c+1) )
      {
        // comment node
        case '-':
          if( _cmXmlAdvancePast(p,"--") == NULL )
            return _cmXmlSyntaxError(p);
        
          if( _cmXmlAdvanceToNext("->") == NULL )
            return _cmXmlSyntaxError(p);

          // p->c is just after "-->"
          break;
          
          // DOCTYPE node
        case 'D':
          if( _cmXmlAdvancePast(P,"DOCTYPE")==NULL )
            return _cmXmlSyntaxError(p);
        
          if((rc = _cmXmlParseDocType(p)) != kOkXmlRC )
            return _cmXmlSyntaxError(p);

          // p->c is just after ">"

          break;
        
        default:
          return _cmXmlSyntaxError(p);
      }
      break;
      
    default:
      // normal node
      if((rc = _cmXmlParseNodeTag(p)) != kOkXmlRC )
        return rc;

      // p->c is just after ">"
      
  }
  
  return rc;
}

cmXmlRC_t  _cmXmlReadNode( cmXml_t* p )
{
}
