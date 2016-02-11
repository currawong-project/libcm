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
#include "cmText.h"

/*
To Do:
1) Escape node data strings and attribute values.
2) Attribute values must be quoted by they may be quoted with either single or double quotes.
3) Consider not buffering the XML file and reading directly from the file.

 */

cmXmlH_t cmXmlNullHandle = cmSTATIC_NULL_HANDLE;

typedef struct
{
  cmErr_t      err;     // 
  cmLHeapH_t   heapH;   // linked heap stores all node memory
  
  cmChar_t*    b;       // base of the text buffer
  unsigned     bn;      // length of the text buffer in characters
  cmChar_t*    c;       // current lexer position
  unsigned     line;    // lexer line number
  
  cmXmlNode_t* root;    // root XML tree node
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
  cmXmlRC_t rc = kOkXmlRC;
  
  cmLHeapDestroy( &p->heapH );

  cmMemPtrFree(&p->b);
  p->bn = 0;
  p->c  = NULL;
  
  cmMemFree(p);

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

  hp->h = p;

  if( fn != NULL )
    if((rc = cmXmlParse(*hp,fn)) != kOkXmlRC )
      hp->h = NULL;

 errLabel:
  if(rc != kOkXmlRC )
    _cmXmlFree(p);
    
  return rc;
}

cmXmlRC_t cmXmlFree(  cmXmlH_t* hp )
{
  cmXmlRC_t rc = kOkXmlRC;
  
  if( hp==NULL || cmXmlIsValid(*hp)==false )
    return kOkXmlRC;

  cmXml_t* p = _cmXmlHandleToPtr(*hp);

  if((rc = _cmXmlFree(p)) != kOkXmlRC )
    return rc;

  hp->h = NULL;
  
  return rc;  
}
  
bool      cmXmlIsValid( cmXmlH_t h )
{ return h.h != NULL; }

  
cmXmlRC_t _cmXmlSyntaxError( cmXml_t* p )
{
  return cmErrMsg(&p->err,kSyntaxErrorXmlRC,"Syntax error on line %i.",p->line);
}

cmXmlNode_t* _cmXmlNodeAlloc( cmXml_t* p, unsigned flags, const cmChar_t* label, unsigned labelN )
{
  cmXmlNode_t* np = cmLhAllocZ(p->heapH,cmXmlNode_t,1);

  np->parent = p->stack;

  if( p->stack != NULL )
  {
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

  // all new nodes are put on the top of the stack
  p->stack = np;

  // all nodes must have a valid 'type' flag
  if( (flags & kTypeXmlFlags) == 0 )
  {
    _cmXmlSyntaxError(p);
    return NULL;
  }
  
  // if this is the root node
  if( cmIsFlag(flags,kRootXmlFl) )
  {
    assert( p->root == NULL );
    p->root = np;
  }

  // if this is the 'doctype' node
  if( cmIsFlag(flags,kDoctypeXmlFl ) )
    p->doctype = np;
  
  if( label != NULL )
    np->label = cmLhAllocStrN(p->heapH,label,labelN);

  np->line  = p->line;
  np->flags = flags;

  return np;
}

cmXmlNode_t* _cmXmlAttrAlloc( cmXml_t* p, cmXmlNode_t* np, const cmChar_t* label, unsigned labelN, const cmChar_t* value, unsigned valueN )
{
  cmXmlAttr_t* ap = cmLhAllocZ(p->heapH, cmXmlAttr_t,1);

  if( label != NULL && labelN > 0 )
    ap->label = cmLhAllocStrN(p->heapH,label,labelN);

  if( value != NULL && valueN > 0 )
    ap->value = cmLhAllocStrN(p->heapH,value,valueN);
  
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

  while( i<n && _cmXmlAdvance(p) )
  {
    if( i>0 && *p->c == s[i] )
    {
      i += 1;
    }
    else
    {
      i = *p->c==s[0];
    }
    
  }
  return p->c;
}

// Return the character following the current character.
const cmChar_t* _cmXmlAdvanceOne( cmXml_t* p )
{
  if( _cmXmlAdvance(p) )
    return p->c;

  return NULL;
  
  /*
  if( _cmXmlIsEof(p) )
    return NULL;

  p->c += 1;

  if( *p->c == '\n' )
    p->line += 1;
  
  return _cmXmlIsEof(p) ? NULL : p->c;
  */
}

cmXmlRC_t  _cmXmlParseAttr( cmXml_t* p, cmChar_t endChar,  cmXmlNode_t* np )
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
  if((v0 = _cmXmlAdvanceToNextNonWhite(p)) == NULL )
    return _cmXmlSyntaxError(p);

  // the first character in the value must be a single quote
  if( *p->c == '\'' )
  {
    if((v0 = _cmXmlAdvanceOne(p)) == NULL )
      return _cmXmlSyntaxError(p);
    
    // advance to the next single quote
    v1 = _cmXmlAdvanceToNext(p,"'");
  }
  else
  {
    v1 = _cmXmlAdvanceToNextWhiteOr(p,endChar,' ');
  }

  if( v1 == NULL )
    return _cmXmlSyntaxError(p);

  
  // advance past the ending single quote
  if( *p->c != endChar )
    if( _cmXmlAdvanceOne(p) == NULL )
      return _cmXmlSyntaxError(p);

  _cmXmlAttrAlloc(p, np, l0, l1-l0, v0, v1-v0 );
  

  // p->c now points just past the ending single quote
  return rc;
}

cmXmlRC_t _cmXmlParseAttrList( cmXml_t* p, cmChar_t endChar, cmXmlNode_t* np )
{
  cmXmlRC_t rc = kOkXmlRC;
  
  while( *p->c != endChar && *p->c != '>' )
    if((rc = _cmXmlParseAttr(p,endChar,np)) != kOkXmlRC )
      break;

  if( *p->c == endChar )
  {
    // if this node is terminated at the end of its beginning tag
    if( endChar == '/' )
    {
      np->flags = cmSetFlag(np->flags,kClosedXmlFl);
      
      //p->stack  = p->stack->parent;
      
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

    if( _cmXmlAdvanceOne(p) == NULL )
      return _cmXmlSyntaxError(p);
    
  }
  else
  {
    if((t1 = _cmXmlAdvanceToNextWhiteOr(p,'>',' ')) == NULL )
      return _cmXmlSyntaxError(p);
  }

  // t1 and p->c now point just past the last character in the token

  return kOkXmlRC;  
}

cmXmlRC_t _cmXmlParseDoctype( cmXml_t* p, cmXmlNode_t** newNodeRef )
{
  cmXmlRC_t rc = kOkXmlRC;
  
  if((*newNodeRef = _cmXmlNodeAlloc(p,kDoctypeXmlFl | kClosedXmlFl,"DOCTYPE",strlen("DOCTYPE"))) == NULL )
    return cmErrLastRC(&p->err);
  
  while( *p->c != '>' )
    if((rc = _cmXmlParseDoctypeToken(p,*newNodeRef)) != kOkXmlRC )
      break;

  if( *p->c == '>' )
    _cmXmlAdvanceOne(p);
  
  return rc;
}

// Node tags are tags that begin with a '<' and are not
// followed by any special character.
cmXmlRC_t _cmXmlParseNodeTag( cmXml_t* p, cmXmlNode_t** newNodeRef )
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

  // Create the node.
  if( (*newNodeRef = _cmXmlNodeAlloc(p,kNormalXmlFl,l0,l1-l0)) == NULL )
    return cmErrLastRC(&p->err);
  
  // look for attributes
  if((rc = _cmXmlParseAttrList(p,'/',*newNodeRef)) != kOkXmlRC )
      return _cmXmlSyntaxError(p);

  // p->c is now past the ending '>'
  
  return rc;
}

cmXmlRC_t _cmXmlParseDeclTag( cmXml_t* p, cmXmlNode_t** newNodeRef )
{
  assert( *p->c == '?' );
  
  const cmChar_t* l0 = NULL;
  const cmChar_t* l1 = NULL;

  if((l0 = _cmXmlAdvanceOne(p)) == NULL)
    return _cmXmlSyntaxError(p);
  
  if((l1 = _cmXmlAdvanceToNextWhiteOr(p,'?',' ')) == NULL )
    return _cmXmlSyntaxError(p);
      
  if( (*newNodeRef = _cmXmlNodeAlloc(p,kDeclXmlFl | kClosedXmlFl,l0,l1-l0)) == NULL )
    return cmErrLastRC(&p->err);
      
  return _cmXmlParseAttrList(p,'?',*newNodeRef);
}

cmXmlRC_t _cmXmlReadEndTag( cmXml_t* p, cmXmlNode_t* np )
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

  // if the label on the top of the stack does not match this label
  if( strncmp( p->stack->label, l0, (l1-l0)+1 ) )
    return kOkXmlRC;

  // since we just parsed an end-tag there should be at least one node on the stack
  if( p->stack == NULL )
    return _cmXmlSyntaxError(p);

  p->stack->flags = cmSetFlag(p->stack->flags,kClosedXmlFl);

  // pop the stack
  //p->stack = p->stack->parent;

  
  
  return kOkXmlRC;  
}
  
// *newNodeRef will be NULL on error or if the
// the parsed tag was an end tag, or if the last line is comment node.
cmXmlRC_t  _cmXmlReadTag( cmXml_t* p, cmXmlNode_t** newNodeRef )
{
  cmXmlRC_t rc = kOkXmlRC;

  assert(newNodeRef != NULL );
  *newNodeRef = NULL;
  
  // No leading '<' was found 
  if( _cmXmlAdvancePast(p,"<") == NULL )
  {
    // error or EOF
    
    return _cmXmlIsEof(p) ? kOkXmlRC : cmErrLastRC(&p->err);  
  }

  // examine the character following the opening '<'
  switch( *p->c )
  {
    // node end tag
    case '/':
      return _cmXmlReadEndTag(p,*newNodeRef);
    
    // declaration tag
    case '?':
      rc = _cmXmlParseDeclTag(p,newNodeRef);
      break;
      
    case '!':
      
      if( _cmXmlAdvanceOne(p) == NULL )
        return _cmXmlSyntaxError(p);
      
      switch( *p->c )
      {
        // comment node
        case '-':
          if( _cmXmlAdvancePast(p,"--") == NULL )
            return _cmXmlSyntaxError(p);
        
          if( _cmXmlAdvanceToNext(p,"->") == NULL )
            return _cmXmlSyntaxError(p);

          if( _cmXmlAdvanceOne(p) == NULL )
            return _cmXmlSyntaxError(p);

          // p->c is just after "-->"

          // Recurse to avoid returning NULL in newNodeRef.
          // (*newNodeRef can only be NULL if we just parsed an end-tag).
          return _cmXmlReadTag(p,newNodeRef);
          
          // DOCTYPE node
        case 'D':
          if( _cmXmlAdvancePast(p,"DOCTYPE")==NULL )
            return _cmXmlSyntaxError(p);

          if((rc = _cmXmlParseDoctype(p,newNodeRef)) != kOkXmlRC )
            return _cmXmlSyntaxError(p);

          // p->c is just after ">"

          break;
        
        default:
          return _cmXmlSyntaxError(p);
      }
      break;
      
    default:
      // normal node
      if((rc = _cmXmlParseNodeTag(p,newNodeRef)) != kOkXmlRC )
        return rc;

      // p->c is just after ">"
      
  }
  
  return rc;
}

cmXmlRC_t _cmXmlReadNode( cmXml_t* p, cmXmlNode_t* parent )
{
  cmXmlRC_t rc;


  while( !_cmXmlIsEof(p) )
  {
    cmXmlNode_t* np = NULL;

    // Read a tag.
    if((rc = _cmXmlReadTag(p,&np)) != kOkXmlRC )
      return rc;

    // If we just read the parents end-tag
    if( cmIsFlag(parent->flags,kClosedXmlFl) )
    {
      assert(np==NULL && parent == p->stack );
      
      p->stack = p->stack->parent;
      return rc;
    }

    // if an  end-tag was just read or node was created but closed then pop the stack
    if( np==NULL || (np==p->stack && cmIsFlag(np->flags,kClosedXmlFl)) )
      p->stack = p->stack->parent;
    
    // if we just read an end-tag or a special node then there is no node-body
    if( np == NULL || cmIsFlag(np->flags,kClosedXmlFl) )
      continue;    

  
    // Advance to the node body.
    if( _cmXmlAdvanceToNextNonWhite(p) == NULL )
      return _cmXmlSyntaxError(p);
  
    // if the the node body contains nodes
    if( *p->c == '<' )
    {
      if((rc = _cmXmlReadNode(p,np)) != kOkXmlRC )
        return rc;
    }
    else // the node body contains a string
    {
      const cmChar_t* s0 = p->c;
      const cmChar_t* s1 = NULL;
      
      if((s1 = _cmXmlAdvanceToNext(p,"<")) == NULL )
        return _cmXmlSyntaxError(p);

      np->dataStr = cmLhAllocStrN(p->heapH,s0,s1-s0);
    }
  }

  return rc;

}


cmXmlRC_t cmXmlParse( cmXmlH_t h, const cmChar_t* fn )
{
  cmXmlRC_t    rc = kOkXmlRC;
  cmXml_t*     p  = _cmXmlHandleToPtr(h);
  cmXmlNode_t* np = NULL;
  
  cmLHeapClear( p->heapH, false );

  cmMemPtrFree(&p->b);
  
  if( (p->b = cmFileFnToBuf(fn, p->err.rpt, &p->bn )) == NULL )
  {
    rc = cmErrMsg(&p->err,kMemAllocErrXmlRC,"Unable to buffer the file '%s'.",cmStringNullGuard(fn));
    goto errLabel;
  }

  p->c    = p->b;
  p->line = 1;

  
  if((np = _cmXmlNodeAlloc(p,kRootXmlFl,"root",strlen("root"))) == NULL )
  {
    rc = cmErrMsg(&p->err,kMemAllocErrXmlRC,"Root node alloc failed.");
    goto errLabel;
  }
  
  if((rc = _cmXmlReadNode(p,np)) != kOkXmlRC )
    goto errLabel;
  
 errLabel:
  return rc;
}

cmXmlRC_t cmXmlClear( cmXmlH_t h )
{
  cmXmlRC_t rc = kOkXmlRC;
  return rc;
}

const cmXmlNode_t* cmXmlRoot( cmXmlH_t h )
{
  cmXml_t*  p  = _cmXmlHandleToPtr(h);
  return p->root;
}


void _cmXmlPrintNode( const cmXmlNode_t* np, cmRpt_t* rpt, unsigned indent )
{
  cmChar_t s[ indent + 1 ];
  memset(s,' ',indent);
  s[indent] = 0;

  // print indent and label
  cmRptPrintf(rpt,"%s%s: ",s,np->label);

  // print this node's attributes
  cmXmlAttr_t* ap = np->attr;
  for(; ap!=NULL; ap=ap->link)
    cmRptPrintf(rpt,"%s='%s' ",ap->label,ap->value);

  // print this nodes data string
  if( np->dataStr != NULL )
    cmRptPrintf(rpt," (%s)",np->dataStr);

  cmRptPrintf(rpt,"\n");

  // print this nodes children via recursion
  cmXmlNode_t* cnp = np->children;
  for(; cnp!=NULL; cnp=cnp->sibling )
    _cmXmlPrintNode(cnp,rpt,indent+2);

 
}

void      cmXmlPrint( cmXmlH_t h , cmRpt_t* rpt )
{
  cmXml_t*  p  = _cmXmlHandleToPtr(h);
  if( p->root != NULL )
    _cmXmlPrintNode(p->root,rpt,0);
}

const cmXmlNode_t* cmXmlSearch( const cmXmlNode_t* np, const cmChar_t* label, const cmXmlAttr_t* attrV, unsigned attrN )
{
  // if the 'label' matches this node's label ...
  if( cmTextCmp(np->label,label) == 0 )
  {
    if( attrN == 0 )
      return np;
    
    unsigned           matchN = 0;
    const cmXmlAttr_t* a      = np->attr;
    unsigned           i;
    
    // ... then check for attribute matches also.
    for(i=0; i<attrN; ++i)
    {
      for(; a!=NULL; a=a->link)
      {
        if( cmTextCmp(a->label,attrV[i].label) == 0 && cmTextCmp(a->value,attrV[i].value) == 0 )
        {
          ++matchN;

          // if a match was found for all attributes then the return np as the solution
          if( matchN == attrN )
            return np;
          
          break;
        }
      }
    }

  }

  // this node did not match - try each of this nodes children
  const cmXmlNode_t* cnp = np->children;
  for(; cnp!=NULL; cnp=cnp->sibling)
    if(( np = cmXmlSearch(cnp,label,attrV,attrN)) != NULL )
      return np;  // a child matched 

  // no match was found.
  return NULL; 
}

const cmXmlNode_t* cmXmlSearchV( const cmXmlNode_t* np, const cmChar_t* label, const cmXmlAttr_t* attrV, unsigned attrN, va_list vl )
{

  while( label != NULL  )
  {
    if((np = cmXmlSearch(np,label,attrV,attrN)) == NULL )
      return NULL;

    if((label = va_arg(vl,cmChar_t*)) != NULL)
    {
      attrV = va_arg(vl,const cmXmlAttr_t*);
      attrN = va_arg(vl,unsigned);
    }

  }
  
  return np;
}

const cmXmlNode_t* cmXmlSearchN( const cmXmlNode_t* np, const cmChar_t* label, const cmXmlAttr_t* attrV, unsigned attrN, ... )
{
  va_list vl;
  va_start(vl,attrN);
  np = cmXmlSearchV(np,label,attrV,attrN,vl);
  va_end(vl);
  return np;
}

const cmXmlAttr_t* cmXmlFindAttrib( const cmXmlNode_t* np, const cmChar_t* label )
{
  const cmXmlAttr_t* a = np->attr;
  for(; a!=NULL; a=a->link)
    if( cmTextCmp(a->label,label) == 0 )
      return a;

  return NULL;
}

cmXmlRC_t cmXmlAttrInt( const cmXmlNode_t* np, const cmChar_t* attrLabel, int* retRef )
{
  const cmXmlAttr_t* a;
  if((a = cmXmlFindAttrib(np,attrLabel)) == NULL )
    return kNodeNotFoundXmlRC;

  assert(retRef != NULL);
  
  *retRef = 0;

  if( a->value != NULL )
  {
    errno = 0;

    // convert the string to an integer
    *retRef = strtol(a->value,NULL,10);
    
    if( errno != 0 )
      return kInvalidTypeXmlRC;
  }
  
  return kOkXmlRC;
  
}

cmXmlRC_t cmXmlAttrUInt( const cmXmlNode_t* np, const cmChar_t* attrLabel, unsigned* retRef )
{ return cmXmlAttrInt(np,attrLabel,(int*)retRef); }

cmXmlRC_t cmXmlGetInt( const cmXmlNode_t* np, int* retRef, const cmChar_t* label, const cmXmlAttr_t* attrV, unsigned attrN, ... )
{
  cmXmlRC_t          rc = kNodeNotFoundXmlRC;
  va_list            vl;
  
  va_start(vl,attrN);

  // find the requsted node
  if((np = cmXmlSearchV(np,label,attrV,attrN,vl)) != NULL )
  {
    // if the returned node does not have a data string
    if( np->dataStr == NULL )      
      return kInvalidTypeXmlRC;

    errno = 0;

    // convert the string to an integer
    strtol(np->dataStr,NULL,10);
    
    if( errno != 0 )
      return kInvalidTypeXmlRC;
    
    rc = kOkXmlRC;
  }
  
  va_end(vl);
  
  return rc;
}

const cmXmlNode_t* _cmXmlNodeFindChild( const cmXmlNode_t* np, const cmChar_t* label )
{
  const cmXmlNode_t* cnp = np->children;
  for(; cnp!=NULL; cnp=cnp->sibling)
    if( cmTextCmp(cnp->label,label) == 0 )
      return cnp;  
  return NULL;
}

const cmChar_t*    cmXmlNodeValueV( const cmXmlNode_t* np, va_list vl )
{
  const cmChar_t* label;
  
  // for each node label
  while( (label = va_arg(vl,const cmChar_t*)) != NULL )
    if((np = _cmXmlNodeFindChild(np,label)) == NULL )
      break;  
  
  return np==NULL ? NULL : np->dataStr;
}

const cmChar_t*    cmXmlNodeValue( const cmXmlNode_t* np, ... )
{
  va_list vl;
  va_start(vl,np);
  const cmChar_t* str = cmXmlNodeValueV(np,vl);
  va_end(vl);
  return str;
}


cmXmlRC_t          cmXmlNodeIntV(const cmXmlNode_t* np, int* retRef, va_list vl )
{
  const cmChar_t* valueStr;
  if((valueStr = cmXmlNodeValueV(np,vl)) == NULL )
    return kNodeNotFoundXmlRC;

  errno = 0;

  // convert the string to an integer
  *retRef = strtol(valueStr,NULL,10);
    
  if( errno != 0 )
    return kInvalidTypeXmlRC;
  
  return kOkXmlRC;
}

cmXmlRC_t          cmXmlNodeUIntV(const cmXmlNode_t* np, unsigned* retRef, va_list vl )
{ return cmXmlNodeIntV(np,(int*)retRef,vl); }

cmXmlRC_t          cmXmlNodeDoubleV(const cmXmlNode_t* np, double* retRef, va_list vl )
{
  const cmChar_t* valueStr;
  if((valueStr = cmXmlNodeValueV(np,vl)) == NULL )
    return kNodeNotFoundXmlRC;

  errno = 0;

  // convert the string to a double
  *retRef = strtod(valueStr,NULL);
    
  if( errno != 0 )
    return kInvalidTypeXmlRC;
  
  return kOkXmlRC;
}

cmXmlRC_t          cmXmlNodeInt( const cmXmlNode_t* np, int* retRef, ... )
{
  cmXmlRC_t rc;
  va_list vl;
  va_start(vl,retRef);
  rc = cmXmlNodeIntV(np,retRef,vl);
  va_end(vl);
  return rc;
}

cmXmlRC_t          cmXmlNodeUInt( const cmXmlNode_t* np, unsigned* retRef, ... )
{
  cmXmlRC_t rc;
  va_list vl;
  va_start(vl,retRef);
  rc = cmXmlNodeUIntV(np,retRef,vl);
  va_end(vl);
  return rc;
}

cmXmlRC_t          cmXmlNodeDouble(const cmXmlNode_t* np, double* retRef, ...)
{
  cmXmlRC_t rc;
  va_list vl;
  va_start(vl,retRef);
  rc = cmXmlNodeDoubleV(np,retRef,vl);
  va_end(vl);
  return rc;

}

unsigned _cmXmlLabelCount( const cmChar_t* firstLabel, va_list vl )
{
  unsigned n = 0;

  if( firstLabel != NULL )
  {
    n = 1;
    
    va_list vl0;
    va_copy(vl0,vl);
    while( va_arg(vl0,const cmChar_t*) != NULL )
      n += 1;
  
    va_end(vl0);
  }
  return n;
}

const cmXmlNode_t*  _cmXmlNodeHasChildV( const cmXmlNode_t* np, const cmChar_t* label, va_list vl, unsigned argN )
{
  unsigned i;
  
  // get next label to match
  for(i=0; i<argN; ++i)
  {
    assert( label != NULL);
    
    np = np->children;
    for(; np!=NULL; np=np->sibling)
      if( cmTextCmp(np->label,label) == 0 )
        break;

    // if the end of the child list was encountered - with no match
    if( np == NULL )
      return NULL;

    label = va_arg(vl,const cmChar_t*);

  }

  return np;
}

bool    cmXmlNodeHasChildV( const cmXmlNode_t* np, const cmChar_t* label, va_list vl )
{
  return _cmXmlNodeHasChildV(np,label,vl,_cmXmlLabelCount(label,vl))!=NULL;
}

bool    cmXmlNodeHasChild( const cmXmlNode_t* np, const cmChar_t* label, ... )
{
  va_list vl;
  va_start(vl,label);
  bool fl = cmXmlNodeHasChildV(np,label,vl);
  va_end(vl);
  return fl;
}

bool    _cmXmlNodeHasChildWithAttrAndValueV( const cmXmlNode_t* np, const cmChar_t* label, va_list vl0, bool valueFl )
{
  unsigned argN = _cmXmlLabelCount(label,vl0);
  unsigned    n = valueFl ? 2 : 1;
  va_list vl1;
  unsigned i;
  
  assert( argN > n-1 ); // an attribute label must be given.

  if( argN <= n-1 )
    return false;

  va_copy(vl1,vl0);
  np = _cmXmlNodeHasChildV(np,label,vl1,argN-1);
  va_end(vl1);

  if( np == NULL )
    return false;

  // advance vl0 to the attribute label
  for(i=0; i<argN-1; ++i)
  {
    label = va_arg(vl0,const cmChar_t*);
    assert( label != NULL );
  }

  // get the attr label
  label = va_arg(vl0,const cmChar_t*);

  const cmXmlAttr_t* a;
  if((a = cmXmlFindAttrib(np,label)) == NULL )
    return false;


  if( valueFl )
  {
    label = va_arg(vl0,const cmChar_t*);
    if( cmTextCmp(a->value,label) != 0 )
      return false;
  }

  return true;
 }

bool    cmXmlNodeHasChildWithAttrAndValueV(  const cmXmlNode_t* np, const cmChar_t* label, va_list vl )
{ return _cmXmlNodeHasChildWithAttrAndValueV(np,label,vl,true); }

bool    cmXmlNodeHasChildWithAttrAndValue(  const cmXmlNode_t* np, const cmChar_t* label, ... )
{
  va_list vl;
  va_start(vl,label);
  bool fl = cmXmlNodeHasChildWithAttrAndValueV(np,label,vl);
  va_end(vl);
  return fl;
}

bool    cmXmlNodeHasChildWithAttrV(  const cmXmlNode_t* np, const cmChar_t* label, va_list vl )
{ return _cmXmlNodeHasChildWithAttrAndValueV(np,label,vl,false); }

bool    cmXmlNodeHasChildWithAttr(  const cmXmlNode_t* np, const cmChar_t* label, ... )
{
  va_list vl;
  va_start(vl,label);
  bool fl = cmXmlNodeHasChildWithAttrV(np,label,vl);
  va_end(vl);
  return fl;
}

cmXmlRC_t cmXmlTest( cmCtx_t* ctx, const cmChar_t* fn )
{
  cmXmlRC_t rc = kOkXmlRC;
  cmXmlH_t   h = cmXmlNullHandle;

  if((rc = cmXmlAlloc(ctx, &h, fn )) != kOkXmlRC )
    return cmErrMsg(&ctx->err,rc,"XML alloc failed.");

  if((rc = cmXmlParse(h,fn)) != kOkXmlRC )
    goto errLabel;

  cmXmlAttr_t aV[] =
  {
    { "id","P1"}
  };

  if( cmXmlSearch(cmXmlRoot(h),"part",aV,1) == NULL )
  {
    cmErrMsg(&ctx->err,kTestFailXmlRC,"Search failed.");
    goto errLabel;
  }
  
  //cmXmlPrint(h,&ctx->rpt);

 errLabel:
  cmXmlFree(&h);
  
  return rc;  
}
