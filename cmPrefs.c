#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmFileSys.h"
#include "cmText.h"
#include "cmJson.h"
#include "cmPrefs.h"

cmPrH_t cmPrNullHandle = cmSTATIC_NULL_HANDLE;

typedef struct cmPrCb_str
{
  cmPrefsOnChangeFunc_t  cbFunc;
  void*                  cbDataPtr;
  struct cmPrCb_str*     linkPtr;
} cmPrCb_t;

// Link record used by cmPrNode_t to form the 
// path labels for each pref. node.
typedef struct cmPrPath_str
{
  const cmChar_t*      label;
  struct cmPrPath_str* parentPtr;
} cmPrPath_t;

// Every pair in the prefs JSON tree is given a unique id.
// The 'path' chain forms a path of pair labels which names the variable.
typedef struct cmPrNode_str
{
  unsigned             id;       // unique id assigned to this pref node
  cmJsonNode_t*        nodePtr;  // pointer to the JSON value for this pref node
  cmPrPath_t*          pathPtr;  // pointer to the path label chain for this node
  struct cmPrNode_str* linkPtr;  // link used to form the pref. node chain
} cmPrNode_t;

typedef struct
{
  cmErr_t     err;              //
  cmJsonH_t   jsH;              // json tree
  cmLHeapH_t  lhH;              // linked heap handle
  cmChar_t*   fn;               // default filename 
  bool        dirtyFl;          // 
  cmPrCb_t*   cbChainPtr;       // callback record linked list
  cmPrNode_t* idChainPtr;       //
  unsigned    id;               // next JSON node id
  cmChar_t*   pathBuf;

} cmPr_t;

cmPr_t* _cmPrefsHandleToPtr( cmPrH_t h )
{
  assert(h.h != NULL);
  return (cmPr_t*)h.h;
}

cmPrNode_t* _cmPrefsIdToNodePtr( cmPr_t* p, unsigned id, bool reportErrFl )
{
  if( id == cmInvalidId )
  {
    cmErrMsg(&p->err,kInvalidIdPrRC,"No preference variables can use the id:%i.",id);
    return NULL;
  }

  cmPrNode_t* np = p->idChainPtr;
  for(; np!=NULL; np=np->linkPtr)
    if( np->id == id )
      return np;

  if( reportErrFl )
    cmErrMsg(&p->err,kVarNotFoundPrRC,"The preference variable associated with id:%i does not exist.",id);

  return NULL;
}

unsigned _cmPrefsCalcNextAvailId( cmPr_t* p )
{
  while( _cmPrefsIdToNodePtr( p, p->id, false) != NULL )
    ++p->id;

  return p->id;
}

cmPrNode_t* _cmPrefsCreateNode( cmPr_t* p, const cmJsonNode_t* jsPairNodePtr, cmPrPath_t* parentPathPtr )
{
  assert( jsPairNodePtr->typeId == kPairTId );

  cmPrNode_t* np         = cmLHeapAllocZ( p->lhH, sizeof(cmPrNode_t) );
  np->id                 = cmInvalidId;
  np->pathPtr            = cmLHeapAllocZ( p->lhH, sizeof(cmPrPath_t) );
  np->pathPtr->label     = jsPairNodePtr->u.childPtr->u.stringVal;
  np->pathPtr->parentPtr = parentPathPtr;
  np->nodePtr            = jsPairNodePtr->u.childPtr->siblingPtr;
  np->linkPtr            = p->idChainPtr;
  p->idChainPtr          = np;

  // object and pair nodes cannot be returned as pref values - so do not give them id's
  if( cmJsonIsObject(np->nodePtr)==false && cmJsonIsPair(np->nodePtr)==false )
    np->id  = _cmPrefsCalcNextAvailId(p);


  return np;
}

void _cmPrefsBuildIdList( cmPr_t* p, const cmJsonNode_t* jsnPtr, cmPrPath_t* parentPathPtr )
{
  assert( jsnPtr != NULL );

  if( jsnPtr->typeId == kPairTId )
  {
    cmPrNode_t* np         = _cmPrefsCreateNode(p,jsnPtr,parentPathPtr);

    _cmPrefsBuildIdList( p, jsnPtr->u.childPtr->siblingPtr, np->pathPtr );

  }
  else
    if( cmJsonChildCount(jsnPtr) )
    {
      const cmJsonNode_t* np = jsnPtr->u.childPtr;
      for(; np != NULL; np = np->siblingPtr )
        _cmPrefsBuildIdList(p,np,parentPathPtr);
    }
  
}


void _cmPrefsInstallCallback( cmPr_t* p, cmPrefsOnChangeFunc_t cbFunc, void* cbDataPtr )
{
  cmPrCb_t* nrp = cmLHeapAllocZ( p->lhH, sizeof(cmPrCb_t));

  nrp->cbFunc    = cbFunc;
  nrp->cbDataPtr = cbDataPtr;
  
  cmPrCb_t* cp = p->cbChainPtr;
  
  // if the cb chain is empty ...
  if(cp == NULL)
    p->cbChainPtr = nrp;// ... make the new cb the first on the chain
  else
  {
    // ... otherwise add the new cb to the end of the cb chain
    while( cp->linkPtr != NULL )
      cp = cp->linkPtr;

    cp->linkPtr = nrp;
  }
 }

cmPrRC_t _cmPrefsFinalize( cmPr_t* p )
{
  cmPrRC_t rc = kOkPrRC;

  if( cmLHeapIsValid(p->lhH) )
     cmLHeapDestroy(&p->lhH);

  if( cmJsonIsValid(p->jsH) )
    if( cmJsonFinalize(&p->jsH) != kOkJsRC )
      rc = cmErrMsg(&p->err,kJsonFailPrRC,"JSON finalization failed.");

  cmMemPtrFree(&p->pathBuf);

  cmMemFree(p);

  return rc;
}

// Convert 'pathString' to a sequence of zero terminated sub-strings.
// The character string returned from this function must be released with a
// call to cmMemFree()
cmChar_t* _cmPrefsCreatePathStr( const cmChar_t* pathString, int* cnt )
{
  assert( pathString != NULL );

  // duplicate the path string
  cmChar_t* pathStr = cmMemAllocStr(pathString);
  int i = 0;
  int  n = 1;
  for(i=0; pathStr[i]; ++i)
    if( pathStr[i]=='/' )
    {
      pathStr[i] = 0;
      ++n;
    }
  
  *cnt = n;
  return pathStr;
}

unsigned _cmPrefsId( cmPr_t* p, const cmChar_t* pathStr, bool reportErrFl )
{
  int      n = 1;
  int      i;
  unsigned retId = cmInvalidId;

  cmChar_t* path = _cmPrefsCreatePathStr(pathStr, &n );
  
  const cmChar_t* pathArray[n];
  const cmChar_t* cp = path;
  
  // store path parts in reverse order - because this
  // is the way the node sees the path 
  for(i=n-1; i>=0; --i)
  {
    pathArray[i]  = cp;
    cp           += strlen(cp) + 1;
  } 

  const cmPrNode_t* np      = p->idChainPtr;

  // go down the id chain
  for(; retId==cmInvalidId && np != NULL; np=np->linkPtr)
  {
    unsigned i = 0;
    const cmPrPath_t* pp = np->pathPtr;

    // go down the path chain from leaf to root
    while( strcmp(pp->label,pathArray[i]) == 0 )
    {
      ++i;
      pp = pp->parentPtr;

      // if the match is complete
      if( i==n && pp==NULL)
      {
        retId = np->id;
        break;
      }
    }
  }

  if( retId==cmInvalidId && reportErrFl )
    cmErrMsg(&p->err,kVarNotFoundPrRC,"The preference variable '%s' was not found.",pathStr);

  cmMemFree(path);

  return retId;
}

cmPrNode_t* _cmPrefsPathToNodePtr( cmPr_t* p, const cmChar_t* pathStr, bool reportErrFl )
{
  unsigned id;
  if((id = _cmPrefsId(p,pathStr,reportErrFl)) == cmInvalidId )
    return NULL;

  return _cmPrefsIdToNodePtr(p,id, reportErrFl);
}

cmPrRC_t cmPrefsInit( cmCtx_t* ctx, cmPrH_t* prefsH, const cmChar_t* fnName, const cmChar_t* fnExt, cmPrefsOnChangeFunc_t cbFunc, void* cbDataPtr )
{
  cmPrRC_t rc = kOkPrRC;

  const cmChar_t* prefsDir = cmFsPrefsDir();
  const cmChar_t* prefsFn  = NULL;

  if( fnName == NULL )
    fnName = cmFsAppName();

  if( fnExt == NULL )
    fnExt = ".js";
  
  // if the prefs directory does not exist then create it
  if( cmFsIsDir(prefsDir) == false )
  {
    if( cmFsMkDir(prefsDir) != kOkFsRC )
    {
      rc = cmErrMsg(&ctx->err,kFileSysFailPrRC,"The preference directory '%s' could not be created.",prefsDir);
      goto errLabel;
    }
  }

  // create the preference file name
  if((prefsFn = cmFsMakeFn( prefsDir, fnName, fnExt, NULL )) == NULL )
  {
    rc = cmErrMsg(&ctx->err,kFileSysFailPrRC,"The preference file name could not be formed.");
    goto errLabel;
  }

  // initialize the preference manager
  rc = cmPrefsInitialize(ctx,prefsH,prefsFn,cbFunc,cbDataPtr);

 errLabel:
  return rc;
}
cmPrRC_t cmPrefsInitialize( cmCtx_t* ctx, cmPrH_t* hp, const cmChar_t* fn, cmPrefsOnChangeFunc_t cbFunc, void* cbDataPtr )
{
  cmPrRC_t rc = kOkPrRC;

  if((rc = cmPrefsFinalize(hp)) != kOkPrRC)
    return rc;

  cmPr_t*  p  = cmMemAllocZ( cmPr_t, 1 );

  cmErrSetup(&p->err,&ctx->rpt,"Preferences");

  // initialize a linked heap
  if( cmLHeapIsValid(p->lhH = cmLHeapCreate(1024,ctx))==false )
  {
    rc = cmErrMsg(&p->err,kLHeapFailPrRC,"Linked heap initialization failed.");
    goto errLabel;
  }

  // if the preference file exists
  if( cmFsIsFile(fn) )
  {

    // initialize the JSON tree from the preferences file
    if( cmJsonInitializeFromFile( &p->jsH, fn, ctx ) != kOkJsRC )
    {
      rc = cmErrMsg(&p->err,kJsonFailPrRC,"Preferences initialization failed on JSON initialization.");
      goto errLabel;
    }


    // build the id list
    _cmPrefsBuildIdList(p, cmJsonRoot(p->jsH), NULL );
  }
  else // otherwise create an empty JSON tree
  {
    if( cmJsonInitialize( &p->jsH, ctx ) != kOkJsRC )
    {
      rc = cmErrMsg(&p->err,kJsonFailPrRC,"An empty JSON tree could not be created.");
      goto errLabel;
    }

    if( cmJsonCreateObject( p->jsH, NULL ) == NULL )
    {
      rc = cmErrMsg(&p->err,kJsonFailPrRC,"The JSON root object could not be created.");
      goto errLabel;
    }
  }

  // install the callback function 
  if( cbFunc != NULL )
    _cmPrefsInstallCallback(p,cbFunc,cbDataPtr);

  // store the file name
  p->fn = cmLHeapAllocZ( p->lhH, strlen(fn)+1 );
  strcpy(p->fn,fn);

  p->id = kMaxVarPrId;

  hp->h = p;

 errLabel:
  if( rc != kOkPrRC )
  {
    _cmPrefsFinalize(p);
    hp-> h = NULL;
  }

  return rc;

}

cmPrRC_t cmPrefsFinalize(   cmPrH_t* hp )
{
  cmPrRC_t rc = kOkPrRC;

  if( hp==NULL || hp->h == NULL )
    return kOkPrRC;

  if((rc = _cmPrefsFinalize( _cmPrefsHandleToPtr(*hp))) == kOkPrRC )
    return rc;

  hp->h = NULL;
  return rc;
}

bool cmPrefsIsValid( cmPrH_t h )
{ return h.h != NULL; }

const cmChar_t* cmPrefsFileName( cmPrH_t h )
{
  cmPr_t* p = _cmPrefsHandleToPtr(h);
  return p->fn;
}

cmPrRC_t cmPrefsRC( cmPrH_t h)
{ return cmErrLastRC(&_cmPrefsHandleToPtr(h)->err); }

cmPrRC_t cmPrefsSetRC( cmPrH_t h, cmPrRC_t rc )
{ return cmErrSetRC(&_cmPrefsHandleToPtr(h)->err,rc); }

cmPrRC_t cmPrefsInstallCallback( cmPrH_t h, cmPrefsOnChangeFunc_t cbFunc, void* cbDataPtr )
{
  cmPr_t* p = _cmPrefsHandleToPtr(h);
  _cmPrefsInstallCallback(p,cbFunc,cbDataPtr);
  return kOkPrRC;
}

cmPrRC_t cmPrefsRemoveCallback(  cmPrH_t h, cmPrefsOnChangeFunc_t cbFunc )
{
  cmPr_t*   p = _cmPrefsHandleToPtr(h);
  cmPrCb_t* cc = p->cbChainPtr;
  cmPrCb_t* pc = NULL;

  while( cc == NULL )
  {
    if( cc->cbFunc == cbFunc )
    {
      if( pc == NULL )
        p->cbChainPtr = cc->linkPtr;
      else
        pc->linkPtr = cc->linkPtr;

      break;
    }

    pc = cc;
    cc  = cc->linkPtr;
  }

  if( cc == NULL )
    return cmErrMsg(&p->err,kCbNotFoundPrRC,"The callback function %p could not be found.",cbFunc);
    
  return kOkPrRC;
}

      unsigned cmPrefsId( cmPrH_t h, const cmChar_t* pathStr, bool reportErrFl )
    {
      cmPr_t* p = _cmPrefsHandleToPtr(h);
      return _cmPrefsId(p,pathStr,reportErrFl);
    }

unsigned cmPrefsEleCount( cmPrH_t h, unsigned id )
{
  cmPr_t*      p = _cmPrefsHandleToPtr(h);
  cmPrNode_t* np = _cmPrefsIdToNodePtr(p,id,true);
  
  if( np == NULL )
    return -1;

  if( cmJsonIsArray(np->nodePtr) )
    return cmJsonChildCount(np->nodePtr);

  return 1;

}



void _cmPrefsFormPath( cmPr_t* p, const cmPrNode_t* np )
{
  cmPrPath_t* pp = np->pathPtr;
  int         n  = 0;

  for(; pp!=NULL; pp=pp->parentPtr)
    ++n;

  const char* pathArray[n];
  unsigned i = n-1;

  for(pp = np->pathPtr; pp!=NULL; pp=pp->parentPtr,--i)
    pathArray[i] = pp->label;

  cmMemPtrFree(&p->pathBuf);

  for(i=0; i<n; ++i)
  {
    if( i > 0 )
      p->pathBuf = cmTextAppendSS(p->pathBuf,"/");
    p->pathBuf = cmTextAppendSS(p->pathBuf,pathArray[i]);
  }
}

cmPrRC_t _cmPrefsValue( cmPr_t* p, const cmPrNode_t* np, const cmJsonNode_t* jnp, bool* bvp, int* ivp, double* rvp, const cmChar_t** svp, unsigned retEleCnt )
{
  cmJsRC_t    jsRC      = kOkJsRC;
  const char* typeLabel = NULL;

  // verify that adequate return space was provided
  if( retEleCnt < 1 )
  {
    _cmPrefsFormPath(p,np);
    return cmErrMsg(&p->err,kBufTooSmallPrRC,"No return space was provided for the preference variable '%s' (id:%i).",p->pathBuf,np->id);
  }

  if( bvp != NULL )
  {
    jsRC = cmJsonBoolValue(jnp,bvp);
    typeLabel = "bool";
  }
  else
    if( ivp != NULL )
    {
      jsRC = cmJsonIntValue(jnp,ivp);
      typeLabel = "int";
    }
    else
      if( rvp != NULL )
      {
        jsRC = cmJsonRealValue(jnp,rvp);
        typeLabel = "real";
      }
      else
        if( svp != NULL )
        {
          jsRC = cmJsonStringValue(jnp,svp);
          typeLabel = "string";
        }
        else
        {
          assert(0);
        }

  if( jsRC != kOkJsRC )
  {
    _cmPrefsFormPath(p,np);
    return cmErrMsg(&p->err,kCvtErrPrRC,"The perferences variable '%s' (id:%i) could not be converted to type '%s'.",p->pathBuf,np->id,typeLabel);
  }

  return kOkPrRC;

} 

cmPrRC_t _cmPrefsGetValue( cmPrH_t h, unsigned id, bool* bvp, int* ivp, double* rvp, const cmChar_t** svp, unsigned* eleCntPtr, unsigned eleIdx )
{
  cmPrRC_t    rc = kOkPrRC;
  cmPr_t*     p  = _cmPrefsHandleToPtr(h);
  cmPrNode_t* np = NULL;

  // if no return buffer was given - do nothing
  if( *eleCntPtr == 0 )
    return kOkPrRC;

  // locate the pref node from 'id'
  if((np = _cmPrefsIdToNodePtr(p,id,true)) == NULL )
  {
    rc = cmErrMsg(&p->err,kVarNotFoundPrRC,"No preference variable was found for id:%i.",id);
    goto errLabel;
  }

  // if the requested pref. var is a scalar
  if( cmJsonIsArray(np->nodePtr) == false )
  {    
    if((rc = _cmPrefsValue(p,np,np->nodePtr,bvp,ivp,rvp,svp,*eleCntPtr)) == kOkPrRC )
      *eleCntPtr = 1;
  }
  else // the request pref. var. is an array
  {
    unsigned i = 0;
    unsigned n = cmJsonChildCount(np->nodePtr);

    // if the entire array was requestedd
    if( eleIdx == cmInvalidIdx )
    {
      // if the return buffer is too small to hold all of the values.
      if( *eleCntPtr < n )
      {
        *eleCntPtr = 0; 
        _cmPrefsFormPath(p,np);
        rc = cmErrMsg(&p->err,kBufTooSmallPrRC,"The return array for the preference variable '%s' (id:%i) is too small to hold '%i elements",p->pathBuf,np->id,n);
        goto errLabel;
      }

      // read each element
      for(i=0; i<n; ++i)
      {
        const cmJsonNode_t* cnp = cmJsonArrayElement(np->nodePtr,i);

        if((rc= _cmPrefsValue(p,np,cnp,bvp==NULL?NULL:bvp+i,ivp==NULL?NULL:ivp+i,rvp==NULL?NULL:rvp+i,svp==NULL?NULL:svp+i,1)) != kOkPrRC )
          goto errLabel;
      }

      *eleCntPtr = n;
    }
    else // a single ele of the array was requested
    {
      // validate the index
      if( eleIdx >= n )
      {
        _cmPrefsFormPath(p,np);
        rc = cmErrMsg(&p->err,kInvalidIndexPrRC,"The index %i is invalid for the variable '%s' of length %i.",eleIdx,p->pathBuf,n);
        goto errLabel;
      }

      // get the json element at array index 'eleIdx'
      const cmJsonNode_t* cnp = cmJsonArrayElement(np->nodePtr,eleIdx);

      assert(cnp != NULL );

      // read the element from the array
      if((rc = _cmPrefsValue(p,np,cnp,bvp,ivp,rvp,svp,*eleCntPtr)) == kOkPrRC )
        *eleCntPtr = 1;
      
    }    
  }
  
 errLabel:
  if( rc != kOkPrRC )
    *eleCntPtr = 0;

  return rc;
}

cmPrRC_t cmPrefsGetBool(   cmPrH_t h, unsigned id, bool*  vp, unsigned* eleCntPtr )
{ return _cmPrefsGetValue(h, id, vp, NULL, NULL, NULL, eleCntPtr, cmInvalidIdx ); }

cmPrRC_t cmPrefsGetInt(    cmPrH_t h, unsigned id, int*         vp, unsigned* eleCntPtr )
{ return _cmPrefsGetValue(h, id, NULL, vp, NULL, NULL, eleCntPtr, cmInvalidIdx ); }

cmPrRC_t cmPrefsGetReal(   cmPrH_t h, unsigned id, double*      vp, unsigned* eleCntPtr )
{ return _cmPrefsGetValue(h, id, NULL, NULL, vp, NULL, eleCntPtr, cmInvalidIdx ); }

cmPrRC_t cmPrefsGetString( cmPrH_t h, unsigned id, const cmChar_t**  vp, unsigned* eleCntPtr )
{ return _cmPrefsGetValue(h, id, NULL, NULL, NULL, vp, eleCntPtr, cmInvalidIdx ); }

bool            cmPrefsBool(   cmPrH_t h, unsigned id )
{
  bool     v = false;
  unsigned n = 1;
  cmPrefsGetBool(h,id,&v,&n);
  return v;
}

unsigned        cmPrefsUInt(   cmPrH_t h, unsigned id )
{
  int      v = 0;
  unsigned n = 1;
  cmPrefsGetInt(h,id,&v,&n);
  return (unsigned)v;
}

int             cmPrefsInt(    cmPrH_t h, unsigned id )
{
  int      v = 0;
  unsigned n = 1;
  cmPrefsGetInt(h,id,&v,&n);
  return v;
}

float           cmPrefsFloat(  cmPrH_t h, unsigned id )
{
  double   v = 0;
  unsigned n = 1;
  cmPrefsGetReal(h,id,&v,&n);
  return (float)v;
}

double          cmPrefsReal(   cmPrH_t h, unsigned id )
{
  double   v = 0;
  unsigned n = 1;
  cmPrefsGetReal(h,id,&v,&n);
  return v;
}

const cmChar_t* cmPrefsString( cmPrH_t h, unsigned id ) 
{
  const cmChar_t* v = NULL;
  unsigned  n = 1;
  if( cmPrefsGetString(h,id,&v,&n) == kOkPrRC )
    return v==NULL ? "" : v;
  return "";
}


bool            cmPrefsBoolDef(   cmPrH_t h, const cmChar_t* pathStr, bool            dfltVal )
{
  unsigned id;
  if( cmPrefsIsValid(h)==false || (id = cmPrefsId(h,pathStr,true)) == cmInvalidId )
    return dfltVal;
  return cmPrefsBool(h,id);
}

unsigned        cmPrefsUIntDef(   cmPrH_t h, const cmChar_t* pathStr, unsigned        dfltVal )
{
  unsigned id;
  if( cmPrefsIsValid(h)==false || (id = cmPrefsId(h,pathStr,true)) == cmInvalidId )
    return dfltVal;
  return cmPrefsUInt(h,id);
}

int             cmPrefsIntDef(    cmPrH_t h, const cmChar_t* pathStr, int             dfltVal )
{
  unsigned id;
  if( cmPrefsIsValid(h)==false || (id = cmPrefsId(h,pathStr,true)) == cmInvalidId )
    return dfltVal;
  return cmPrefsInt(h,id);
}

float           cmPrefsFloatDef(  cmPrH_t h, const cmChar_t* pathStr, float           dfltVal )
{
  unsigned id;
  if( cmPrefsIsValid(h)==false || (id = cmPrefsId(h,pathStr,true)) == cmInvalidId )
    return dfltVal;
  return cmPrefsFloat(h,id);
}

double          cmPrefsRealDef(   cmPrH_t h, const cmChar_t* pathStr, double          dfltVal )
{
  unsigned id;
  if( cmPrefsIsValid(h)==false || (id = cmPrefsId(h,pathStr,true)) == cmInvalidId )
    return dfltVal;
  return cmPrefsReal(h,id);
}

const cmChar_t* cmPrefsStringDef( cmPrH_t h, const cmChar_t* pathStr, const cmChar_t* dfltVal ) 
{
  unsigned id;
  if( cmPrefsIsValid(h)==false || (id = cmPrefsId(h,pathStr,true)) == cmInvalidId )
    return dfltVal;
  const cmChar_t* v = cmPrefsString(h,id);
  return v==NULL || strlen(v)==0 ? dfltVal : v;
}


cmPrRC_t        cmPrefsBoolRc(   cmPrH_t h, const cmChar_t* pathStr, bool*            retValPtr )
{
  unsigned id;
  if((id = cmPrefsId(h,pathStr,true)) == cmInvalidId )
    return kVarNotFoundPrRC;
    
  unsigned n = 1;
  return cmPrefsGetBool(h,id,retValPtr,&n); 
}

cmPrRC_t        cmPrefsUIntRc(   cmPrH_t h, const cmChar_t* pathStr, unsigned*        retValPtr )
{
  unsigned id;
  unsigned n = 1;
  int      v;
  cmPrRC_t rc;

  if((id = cmPrefsId(h,pathStr,true)) == cmInvalidId )
    return kVarNotFoundPrRC;
    
  if((rc =cmPrefsGetInt(h,id,&v,&n)) == kOkPrRC )
    *retValPtr = v;

  return rc;
}

cmPrRC_t        cmPrefsIntRc(    cmPrH_t h, const cmChar_t* pathStr, int*             retValPtr )
{
  unsigned id;
  if((id = cmPrefsId(h,pathStr,true)) == cmInvalidId )
    return kVarNotFoundPrRC;
    
  unsigned n = 1;
  return cmPrefsGetInt(h,id,retValPtr,&n); 
}

cmPrRC_t        cmPrefsFloatRc(  cmPrH_t h, const cmChar_t* pathStr, float*           retValPtr )
{
  unsigned id;
  unsigned n = 1;
  double   v;
  cmPrRC_t rc;

  if((id = cmPrefsId(h,pathStr,true)) == cmInvalidId )
    return kVarNotFoundPrRC;
    
  if((rc =cmPrefsGetReal(h,id,&v,&n)) == kOkPrRC )
    *retValPtr = v;

  return rc;
}

cmPrRC_t        cmPrefsRealRc(   cmPrH_t h, const cmChar_t* pathStr, double*          retValPtr )
{
  unsigned id;
  if((id = cmPrefsId(h,pathStr,true)) == cmInvalidId )
    return kVarNotFoundPrRC;
    
  unsigned n = 1;
  return cmPrefsGetReal(h,id,retValPtr,&n); 
}

cmPrRC_t        cmPrefsStringRc( cmPrH_t h, const cmChar_t* pathStr, const cmChar_t** retValPtr ) 
{
  unsigned id;
  if((id = cmPrefsId(h,pathStr,true)) == cmInvalidId )
    return kVarNotFoundPrRC;
    
  unsigned n = 1;
  return cmPrefsGetString(h,id,retValPtr,&n); 
}




unsigned  cmPrefsArrayElementCount( cmPrH_t h, unsigned id )
{
  cmPrNode_t* np;
  cmPr_t*     p = _cmPrefsHandleToPtr(h);

  // locate the pref node from 'id'
  if((np = _cmPrefsIdToNodePtr(p,id,true)) == NULL )
  {
    cmErrMsg(&p->err,kVarNotFoundPrRC,"No preference variable was found for id:%i.",id);
    return cmInvalidCnt;
  }

  // if the requested pref. var is a scalar
  if( cmJsonIsArray(np->nodePtr) )
    return  cmJsonChildCount(np->nodePtr);

  return 0;
}

bool            cmPrefsBoolEle(   cmPrH_t h, unsigned id, unsigned idx )
{
  bool     v = false;;
  unsigned n = 1;
  _cmPrefsGetValue(h,id, &v, NULL, NULL, NULL, &n, idx );
  return v;
}

unsigned        cmPrefsUIntEle(   cmPrH_t h, unsigned id, unsigned idx )
{
  int      v = 0;
  unsigned n = 1;
  _cmPrefsGetValue(h,id, NULL, &v, NULL, NULL, &n, idx );
  return (unsigned)v;
}

int             cmPrefsIntEle(    cmPrH_t h, unsigned id, unsigned idx )
{
  int      v = 0;
  unsigned n = 1;
  _cmPrefsGetValue(h,id, NULL, &v, NULL, NULL, &n, idx );
  return v;
}

float           cmPrefsFloatEle(  cmPrH_t h, unsigned id, unsigned idx )
{
  double   v = 0;
  unsigned n = 1;
  _cmPrefsGetValue(h,id, NULL, NULL, &v, NULL, &n, idx );
  return (float)v;
}

double          cmPrefsRealEle(   cmPrH_t h, unsigned id, unsigned idx )
{
  double   v = 0;
  unsigned n = 1;
  _cmPrefsGetValue(h,id, NULL, NULL, &v, NULL, &n, idx );
  return v;
}

const cmChar_t* cmPrefsStringEle( cmPrH_t h, unsigned id, unsigned idx ) 
{
  const cmChar_t* v = "";
  unsigned        n = 1;
  if( _cmPrefsGetValue(h,id, NULL, NULL, NULL, &v, &n, idx ) == kOkPrRC )
    return v;
  return "";
}


cmPrRC_t _cmPrefsSetValue( cmPr_t* p, cmPrNode_t* np, cmJsonNode_t* jnp,const bool* bvp, const int* ivp, const double* rvp, const cmChar_t** svp, bool* deltaFlPtr )
{
  cmJsRC_t jsRC = kOkJsRC;
  //const cmChar_t* typeLabel;

  if( bvp != NULL )
  {
    bool v;

    if((jsRC = cmJsonBoolValue(jnp,&v)) == kOkJsRC )
      if( (*deltaFlPtr = (v != *bvp))  )
        jsRC = cmJsonSetBool(p->jsH,jnp,*bvp);

    //typeLabel = "bool";
  }
  else
  if( ivp != NULL )
  {
    int v;

    if((jsRC = cmJsonIntValue(jnp,&v)) == kOkJsRC )
      if( (*deltaFlPtr = (v != *ivp)) )
        jsRC = cmJsonSetInt(p->jsH,jnp,*ivp);

    //typeLabel = "int";
  }
  else
  if( rvp != NULL )
  {

    double v;

    if((jsRC = cmJsonRealValue(jnp,&v)) == kOkJsRC )
      if( (*deltaFlPtr = (v != *rvp)) )
        jsRC = cmJsonSetReal(p->jsH,jnp,*rvp);

    //typeLabel = "real";
  }
  else
  if( svp != NULL )
  {
    const cmChar_t* v;

    if((jsRC = cmJsonStringValue(jnp,&v)) == kOkJsRC )
    {
      const cmChar_t* s0 = v   ==NULL ? "" : v;
      const cmChar_t* s1 = *svp==NULL ? "" : *svp;
        
      if( (*deltaFlPtr = (strcmp(s0,s1)!=0)) )
        jsRC = cmJsonSetString(p->jsH,jnp,s1);
    }
    //typeLabel = "string";
  }
  else
  {
    assert(0);
  }

  if( jsRC != kOkJsRC )
  {
    _cmPrefsFormPath(p,np);
    return cmErrMsg(&p->err,kCvtErrPrRC,"The perferences variable '%s' (id:%i) could not be set.",p->pathBuf,np->id); 
  }

  return kOkPrRC;
}


void _cmPrefsCallback( cmPr_t* p, unsigned id )
{
  // notify any listeners that a node has changed values
  cmPrCb_t* cbNodePtr = p->cbChainPtr;
  for(; cbNodePtr!=NULL; cbNodePtr=cbNodePtr->linkPtr)
  {
    cmPrH_t h;
    h.h = p;
    cbNodePtr->cbFunc(h,cbNodePtr->cbDataPtr,id);
        
  }
}

cmPrRC_t _cmPrefsSetValues2( cmPr_t* p, cmPrNode_t* np, const bool* bvp, const int* ivp, const double* rvp, const cmChar_t** svp, const unsigned* eleCntPtr )
{
  cmPrRC_t    rc      = kOkPrRC;
  bool        deltaFl = false; // set to true if the value of the pref. var. identified by 'id' actually changes.

  // a scalar has been passed in and the existing json node is also a scalar
  if( cmJsonIsArray(np->nodePtr)== false && *eleCntPtr==1 )
  {
    rc = _cmPrefsSetValue(p,np,np->nodePtr,bvp,ivp,rvp,svp,&deltaFl);
    goto errLabel;
  }

  // the pref. variable is an array
  if( cmJsonIsArray(np->nodePtr) )
  {
    unsigned i;
    unsigned curArrCnt = cmJsonChildCount(np->nodePtr);
    unsigned n         = cmMin(*eleCntPtr,curArrCnt);
    
    for(i=0; i<n; ++i)
    {
      cmJsonNode_t* jnp = cmJsonArrayElement(np->nodePtr,i);
      bool          dfl = false;
      if((rc = _cmPrefsSetValue(p,np,jnp,bvp==NULL?NULL:bvp+i,ivp==NULL?NULL:ivp+i,rvp==NULL?NULL:rvp+i,svp==NULL?NULL:svp+i,&dfl)) != kOkPrRC )
        return rc;

      if( dfl )
        deltaFl = true;
    }

    // if elements should be removed
    if( curArrCnt > *eleCntPtr )
    {
      deltaFl = true; // ele's are being removed so the variable value is changing

      for(i=*eleCntPtr; i<curArrCnt; ++i)
      {
        cmJsonNode_t* jnp = cmJsonArrayElement(np->nodePtr,*eleCntPtr);
        assert(jnp!=NULL);
        cmJsonRemoveNode(p->jsH,jnp,true);
      }
    }

    // if elements should be added
    if( *eleCntPtr > curArrCnt )
    {
      unsigned      typeId = cmInvalidId;
      cmJsonNode_t* jnp = NULL;      
      cmJsonNode_t* snp = curArrCnt==0 ? NULL : cmJsonArrayElement(np->nodePtr,curArrCnt-1);
      
      assert(snp->ownerPtr==np->nodePtr && snp->siblingPtr==NULL);

      deltaFl = true; // ele's are being added so the variable value is changing

      if( ivp != NULL )
        typeId = kIntTId;
      else
        if( rvp != NULL )
          typeId = kRealTId;
        else
          if( svp != NULL )
            typeId = kStringTId;

      // for each new array element
      for(; i<*eleCntPtr; ++i)
      {
        if( bvp != NULL )
          typeId = bvp[i];
      
        // create the new array element node - the new node is automatically added to the end of the array 
        if( cmJsonCreate(p->jsH, np->nodePtr, typeId, svp==NULL?NULL:svp[i], ivp==NULL?0:ivp[i], rvp==NULL?0:rvp[i], &jnp ) != kOkJsRC )
        {
          _cmPrefsFormPath(p,np);
          return cmErrMsg(&p->err,kJsonFailPrRC,"Unable to create a value node for '%s' (id=%i).",p->pathBuf,np->id); 
        }
      }
    }

  errLabel:

    if( (rc == kOkPrRC) && deltaFl )
    {
      _cmPrefsCallback(p, np->id );
      // update the global delta flag
      p->dirtyFl = true;
    }

    return rc;
  }

  _cmPrefsFormPath(p,np);
  return cmErrMsg(&p->err,kCvtErrPrRC,"The new preference value could not be converted to the existing preference variable type for '%s' (id=%i).",p->pathBuf,np->id);
}

cmPrRC_t _cmPrefsSetValues( cmPrH_t h, unsigned id, const bool* bvp, const int* ivp, const double* rvp, const cmChar_t** svp, const unsigned* eleCntPtr )
{
  cmPr_t*     p       = _cmPrefsHandleToPtr(h);
  cmPrNode_t* np;

  // locate the pref node
  if((np = _cmPrefsIdToNodePtr(p,id,true)) == NULL )
    return cmErrMsg(&p->err,kVarNotFoundPrRC,"The variable with id=%i was not found.",id);

  return _cmPrefsSetValues2( p, np, bvp, ivp, rvp, svp, eleCntPtr );
}

cmPrRC_t cmPrefsSetBoolArray(   cmPrH_t h, unsigned id, const bool*   vp, const unsigned* eleCntPtr )
{ return _cmPrefsSetValues(h, id, vp, NULL, NULL, NULL, eleCntPtr ); }

cmPrRC_t cmPrefsSetIntArray(    cmPrH_t h, unsigned id, const int*    vp, const unsigned* eleCntPtr )
{ return _cmPrefsSetValues(h, id, NULL, vp, NULL, NULL, eleCntPtr ); }

cmPrRC_t cmPrefsSetRealArray(   cmPrH_t h, unsigned id, const double* vp, const unsigned* eleCntPtr )
{ return _cmPrefsSetValues(h, id, NULL, NULL, vp, NULL, eleCntPtr ); }

cmPrRC_t cmPrefsSetStringArray( cmPrH_t h, unsigned id, const cmChar_t**   vp, const unsigned* eleCntPtr )
{ return _cmPrefsSetValues(h, id, NULL, NULL, NULL, vp, eleCntPtr ); }


cmPrRC_t cmPrefsSetBool(   cmPrH_t h, unsigned id, bool val )
{
  unsigned n = 1;
  return cmPrefsSetBoolArray(h,id,&val,&n);
}

cmPrRC_t cmPrefsSetUInt(   cmPrH_t h, unsigned id, unsigned val )
{
  unsigned n = 1;
  int ival = (int)val;
  return cmPrefsSetIntArray(h,id,&ival,&n);
}

cmPrRC_t cmPrefsSetInt(    cmPrH_t h, unsigned id, int val )
{
  unsigned n = 1;
  return cmPrefsSetIntArray(h,id,&val,&n);
}

cmPrRC_t cmPrefsSetFloat(  cmPrH_t h, unsigned id, float val )
{
  unsigned n = 1;
  double dval = val;
  return cmPrefsSetRealArray(h,id,&dval,&n);
}

cmPrRC_t cmPrefsSetReal( cmPrH_t h, unsigned id, double val )
{
  unsigned n = 1;
  return cmPrefsSetRealArray(h,id,&val,&n);
}

cmPrRC_t cmPrefsSetString( cmPrH_t h, unsigned id, const cmChar_t* val )
{
  unsigned n = 1;
  return cmPrefsSetStringArray(h,id,&val,&n);
}




cmPrRC_t cmPrefsPathSetBool(   cmPrH_t h, const cmChar_t* pathStr, bool val )
{
  unsigned id;
  unsigned n = 1;
  if((id = cmPrefsId(h,pathStr,true)) == cmInvalidId )
    return kVarNotFoundPrRC;

  return cmPrefsSetBoolArray(h,id,&val,&n);
}

cmPrRC_t cmPrefsPathSetUInt(   cmPrH_t h, const cmChar_t* pathStr, unsigned val )
{
  unsigned id;
  unsigned n = 1;
  if((id = cmPrefsId(h,pathStr,true)) == cmInvalidId )
    return kVarNotFoundPrRC;

  int ival = (int)val;
  return cmPrefsSetIntArray(h,id,&ival,&n);
}

cmPrRC_t cmPrefsPathSetInt(    cmPrH_t h, const cmChar_t* pathStr, int val )
{
  unsigned id;
  unsigned n = 1;
  if((id = cmPrefsId(h,pathStr,true)) == cmInvalidId )
    return kVarNotFoundPrRC;

  return cmPrefsSetIntArray(h,id,&val,&n);
}

cmPrRC_t cmPrefsPathSetFloat(  cmPrH_t h, const cmChar_t* pathStr, float val )
{
  unsigned id;
  unsigned n = 1;
  if((id = cmPrefsId(h,pathStr,true)) == cmInvalidId )
    return kVarNotFoundPrRC;

  double dval = val;
  return cmPrefsSetRealArray(h,id,&dval,&n);
}

cmPrRC_t cmPrefsPathSetReal( cmPrH_t h, const cmChar_t* pathStr, double val )
{
  unsigned id;
  unsigned n = 1;
  if((id = cmPrefsId(h,pathStr,true)) == cmInvalidId )
    return kVarNotFoundPrRC;

  return cmPrefsSetRealArray(h,id,&val,&n);
}

cmPrRC_t cmPrefsPathSetString( cmPrH_t h, const cmChar_t* pathStr, const cmChar_t* val )
{
  unsigned id;
  unsigned n = 1;
  if((id = cmPrefsId(h,pathStr,true)) == cmInvalidId )
    return kVarNotFoundPrRC;

  return cmPrefsSetStringArray(h,id,&val,&n);
}



bool _cmPrefsValidateNodeType( cmJsonNode_t* np, unsigned jsTypeId, unsigned valCnt )
{
  // if we are looking for a scalar node then the type must be jsTypeId
  if( valCnt == 1 )
    return np->typeId == jsTypeId;

  // if we are looking for an array node then the type must be kArrayTId ...
  if( np->typeId != kArrayTId )
    return false;
  
  unsigned      n = cmJsonChildCount(np);
  unsigned      i = 0;
  
  // ... and all of the array elements must be of type jsTypeId
  for(; i<n; ++i)
  {
    const cmJsonNode_t* cnp = cmJsonArrayElementC(np,i);
    assert( cnp != NULL );
    if( cnp->typeId != jsTypeId )
      return false;
  }

  return true;
}

// 
cmPrRC_t  _cmPrefsCreateJsonNode( 
  cmPr_t*          p,           
  unsigned         id,         // new variable id
  const cmChar_t*  pathString, // new variable name
  unsigned         jsTypeId,   // type of new variable
  const cmChar_t** strVal,     // value of new variable 
  const double*    realVal,    //
  const int*       intVal,     //
  const bool*      boolVal,    //
  unsigned         valCnt,     // length of xxxVal[]
  bool             setFlag )   // true== if same named variable already exists then set the value to xxxVal
{
  cmPrRC_t        rc            = kOkPrRC;
  int             pathCnt       = 0;
  cmChar_t*       pathStr       = _cmPrefsCreatePathStr(pathString,&pathCnt);
  const cmChar_t* pathEleStr    = pathStr; 
  cmJsonNode_t*   jsnp          = cmJsonRoot(p->jsH);
  cmJsonNode_t*   jsPairNodePtr = NULL;
  cmPrNode_t*     pnp           = NULL;  // current pref node pointer
  bool            existsFl      = false; // set if variable already exists
  int             i;

  assert( pathCnt >= 1 );

  if( id != cmInvalidId && id >= kMaxVarPrId )
    return cmErrMsg(&p->err,kNodeCreateFailPrRC,"User supplied id's must be less than 0x%x.",kMaxVarPrId);

  
  // for each path element
  for(i=0; i<pathCnt; ++i)
  {    
    cmJsonNode_t* tnp = NULL;

    // find the child pair node of jsnp named 'pathEleStr'.    
    if( (tnp = cmJsonNodeMemberValue(jsnp, pathEleStr )) == NULL )
      break;

    // the located json node must be an object (i<pathCnt-1) or the same type as jsTypeId (i=pathCnt-1)
    // This check guarantees that all nodes up to the leaf node are objects and the leaf node matches the created type.
    if( !(((i<pathCnt-1) && cmJsonIsObject(tnp)) || ((i==pathCnt-1) && _cmPrefsValidateNodeType(tnp,jsTypeId,valCnt))) )
    {
      rc = cmErrMsg(&p->err,kNodeCreateFailPrRC,"Node creation for '%s' failed because an existing node with the same name but different type (%i) was found.",pathString,tnp->typeId);
      goto errLabel;
    }

    // assert that the json nodes we are finding form a tree
    assert( (tnp->ownerPtr==NULL && jsnp==NULL) || (tnp->ownerPtr!=NULL && tnp->ownerPtr->ownerPtr==jsnp));

    jsnp = tnp;

    // find the pref node which refers to the located json node 
    // (we need to track the pnp->pathPtr)
    pnp = p->idChainPtr;
    for(; pnp != NULL; pnp=pnp->linkPtr)
      if( pnp->nodePtr == jsnp )
        break;

    assert(pnp->pathPtr != NULL);

    // advance to the next path segment
    pathEleStr += strlen(pathEleStr) + 1;
  }

  // if i<pathCnt then json nodes must be created to 
  // if i==pathCnt then a matching variable already exists 
  if( i==pathCnt )
  {
    // the reference variable already exists and assignment was requested (setFlag==true).
    // but the assignment cannot be made until the 'id' is updated.
    existsFl = true;
  }

  // we have followed 'pathString' to the last node which already
  // exists in the JSON tree - now we must create new JSON nodes to reflect 
  // the remaining path elements
  for(; i<pathCnt; ++i)
  {
    // if this is the last element in the path - then it is a pair
    if( i==pathCnt-1 )
    {
      // create a scalar value pair node
      if( valCnt == 1 )
      {
        const char* sv = strVal==NULL ? NULL : *strVal;
        double      rv = realVal==NULL ? 0.0 : *realVal;
        int         iv = intVal ==NULL ? 0   : *intVal;

        if( boolVal != NULL )
          jsTypeId = *boolVal ? kTrueTId : kFalseTId;

        if((jsnp = cmJsonInsertPair( p->jsH, jsnp, pathEleStr,jsTypeId, sv, iv, rv )) == NULL )
        {
          rc = cmErrMsg(&p->err,kInvalidIdPrRC,"Preference node create failed for '%s'",cmStringNullGuard(pathString));
          goto errLabel;
        }
      }
      else // create a vector value pair node
      {
        switch( jsTypeId )
        {
          case kStringTId: jsnp = cmJsonInsertPairStringArray2(p->jsH, jsnp, pathEleStr, valCnt, strVal); break;
          case kRealTId:   jsnp = cmJsonInsertPairRealArray2(  p->jsH, jsnp, pathEleStr, valCnt, realVal);break;
          case kIntTId:    jsnp = cmJsonInsertPairIntArray2(   p->jsH, jsnp, pathEleStr, valCnt, intVal); break;
          case kBoolTId:   jsnp = cmJsonInsertPairBoolArray2(  p->jsH, jsnp, pathEleStr, valCnt, boolVal ); break;
          default:
            {
              rc = cmErrMsg(&p->err,kInvalidIdPrRC,"Preference node insert failed on '%s' due to invalid type id '%i'.",cmStringNullGuard(pathString),jsTypeId);
              goto errLabel;
            }
        }
      }

      assert( jsnp != NULL );

      jsPairNodePtr = jsnp;

    } 
    else // this is not the last element in the path - create an object node
    {
      // create the object associated with this new level
      if((jsnp = cmJsonInsertPairObject( p->jsH, jsnp, pathEleStr )) == NULL )
      {
        rc = cmErrMsg(&p->err,kJsonFailPrRC,"Preference node create failed because JSON node create failed on '%s'.",cmStringNullGuard(pathString));
        goto errLabel;
      }

      jsPairNodePtr = jsnp->ownerPtr;

    }

    unsigned nxtId = p->id;

    // create a pref node to associate with this new level
    if((pnp = _cmPrefsCreateNode(p, jsPairNodePtr, pnp==NULL ? NULL : pnp->pathPtr )) == NULL )
    {
      rc = cmErrMsg(&p->err,kNodeCreateFailPrRC,"Creation failed for the '%s' element of the preference node '%s'.",cmStringNullGuard(pathEleStr),cmStringNullGuard(pathString));
      goto errLabel;
    }

    // always leave internal nodes with id=cmInvalidId, leaf node id's will be set below
    p->id   = nxtId;
    pnp->id = cmInvalidId;

    pathEleStr += strlen(pathEleStr) + 1;
  }

  assert( pnp != NULL );

  // if an preference variable 'id' was given then set it here
  if( id == cmInvalidId )
    pnp->id = _cmPrefsCalcNextAvailId(p);
  else
  {
    if( _cmPrefsIdToNodePtr(p, id, false ) != NULL )
      cmErrWarnMsg(&p->err,kDuplicateIdPrRC,"The preference variable id '%i' is used by multiple preference variables including '%s'.",id,cmStringNullGuard(pathString));
  
    pnp->id = id;
  }

  // if the variable already existed and setFlag==true then update the variable value here
  // (note that this cannot occur earlier because the 'id' has not yet been updated 
  //  which might result in the incorrect id being set with the callback function)
  if( existsFl && setFlag )
    if((rc = _cmPrefsSetValues2(p, pnp, boolVal, intVal, realVal, strVal, &valCnt )) != kOkPrRC ) 
      goto errLabel;

  // if a new variable was created then notify the application of the new value
  if(existsFl==false)
  {
    assert(pnp!=NULL);
    _cmPrefsCallback(p,pnp->id);
  }

 errLabel:
  cmMemFree(pathStr);

  return rc;
}

cmPrRC_t cmPrefsCreateBool( cmPrH_t h, unsigned id, const cmChar_t* pathStr, unsigned flags, bool val )
{
  cmPr_t* p = _cmPrefsHandleToPtr(h);
  bool v = val;
  return  _cmPrefsCreateJsonNode(p, id, pathStr, kBoolTId, NULL, NULL, NULL, &v, 1, cmIsFlag(flags, kForceValuePrFl)  );
}

cmPrRC_t cmPrefsCreateUInt(   cmPrH_t h, unsigned id, const cmChar_t* pathStr, unsigned flags, unsigned val )
{
  cmPr_t* p = _cmPrefsHandleToPtr(h);
  int v = val;
  return  _cmPrefsCreateJsonNode(p, id, pathStr, kIntTId, NULL, NULL, &v, NULL, 1, cmIsFlag(flags, kForceValuePrFl) );
}

cmPrRC_t cmPrefsCreateInt(    cmPrH_t h, unsigned id, const cmChar_t* pathStr, unsigned flags, int      val )
{
  cmPr_t* p = _cmPrefsHandleToPtr(h);
  int v = val;
  return  _cmPrefsCreateJsonNode(p, id, pathStr, kIntTId, NULL, NULL,  &v, NULL, 1, cmIsFlag(flags, kForceValuePrFl) );
}

cmPrRC_t cmPrefsCreateFloat(  cmPrH_t h, unsigned id, const cmChar_t* pathStr, unsigned flags, float    val )
{
  cmPr_t* p = _cmPrefsHandleToPtr(h);
  double v = val;
  return  _cmPrefsCreateJsonNode(p, id, pathStr, kRealTId, NULL,  &v, NULL, NULL, 1, cmIsFlag(flags, kForceValuePrFl) );
}

cmPrRC_t cmPrefsCreateReal(   cmPrH_t h, unsigned id, const cmChar_t* pathStr, unsigned flags, double   val )
{
  cmPr_t* p = _cmPrefsHandleToPtr(h);
  double v = val;
  return  _cmPrefsCreateJsonNode(p, id, pathStr, kRealTId, NULL, &v,NULL, NULL, 1, cmIsFlag(flags, kForceValuePrFl) );
}

cmPrRC_t cmPrefsCreateString( cmPrH_t h, unsigned id, const cmChar_t* pathStr, unsigned flags, const cmChar_t* val )
{
  cmPr_t* p = _cmPrefsHandleToPtr(h);
  const cmChar_t* v = val;
  return  _cmPrefsCreateJsonNode(p, id, pathStr, kStringTId,  &v, NULL, NULL, NULL, 1, cmIsFlag(flags,kForceValuePrFl) );
}

cmPrRC_t cmPrefsCreateBoolArray(   cmPrH_t h, unsigned id, const cmChar_t* pathStr, unsigned flags, const bool*     val, unsigned eleCnt )
{
  cmPr_t* p = _cmPrefsHandleToPtr(h);
  return  _cmPrefsCreateJsonNode(p, id, pathStr, kBoolTId, NULL, NULL, NULL, val, eleCnt, cmIsFlag(flags, kForceValuePrFl)  );
}

cmPrRC_t cmPrefsCreateUIntArray(   cmPrH_t h, unsigned id, const cmChar_t* pathStr, unsigned flags, const unsigned* val, unsigned eleCnt )
{
  cmPr_t* p = _cmPrefsHandleToPtr(h);
  return  _cmPrefsCreateJsonNode(p, id, pathStr, kIntTId, NULL, NULL, (int*)val, NULL, eleCnt, cmIsFlag(flags, kForceValuePrFl) );
}

cmPrRC_t cmPrefsCreateIntArray(    cmPrH_t h, unsigned id, const cmChar_t* pathStr, unsigned flags, const int*      val, unsigned eleCnt )
{
  cmPr_t* p = _cmPrefsHandleToPtr(h);
  return  _cmPrefsCreateJsonNode(p, id, pathStr, kIntTId, NULL, NULL,  val, NULL, eleCnt, cmIsFlag(flags, kForceValuePrFl) );
}

cmPrRC_t cmPrefsCreateFloatArray(  cmPrH_t h, unsigned id, const cmChar_t* pathStr, unsigned flags, const float*    val, unsigned eleCnt )
{
  cmPr_t*  p = _cmPrefsHandleToPtr(h);
  double*  a = cmMemAlloc(double,eleCnt);
  unsigned i;
  for(i=0; i<eleCnt; ++i)
    a[i] = val[i];
 
  cmPrRC_t rc =  _cmPrefsCreateJsonNode(p, id, pathStr, kRealTId, NULL,  a, NULL, NULL, eleCnt, cmIsFlag(flags, kForceValuePrFl) );
  cmMemFree(a);
  return rc;
}

cmPrRC_t cmPrefsCreateRealArray(   cmPrH_t h, unsigned id, const cmChar_t* pathStr, unsigned flags, const double*   val, unsigned eleCnt )
{
  cmPr_t* p = _cmPrefsHandleToPtr(h);
  return  _cmPrefsCreateJsonNode(p, id, pathStr, kRealTId, NULL,  val, NULL, NULL, eleCnt, cmIsFlag(flags, kForceValuePrFl) );
}

cmPrRC_t cmPrefsCreateStringArray( cmPrH_t h, unsigned id, const cmChar_t* pathStr, unsigned flags, const cmChar_t** val, unsigned eleCnt )
{
  cmPr_t* p = _cmPrefsHandleToPtr(h);
  return  _cmPrefsCreateJsonNode(p, id, pathStr, kStringTId,  val, NULL, NULL, NULL, eleCnt, cmIsFlag(flags,kForceValuePrFl) );
}

bool     cmPrefsIsDirty( cmPrH_t h )
{
  cmPr_t* p = _cmPrefsHandleToPtr(h);
  return p->dirtyFl;
}

cmPrRC_t cmPrefsWrite( cmPrH_t h, const cmChar_t* fn )
{
  cmPrRC_t rc = kOkPrRC;
  cmPr_t*  p  = _cmPrefsHandleToPtr(h);

  if( fn == NULL )
    fn = p->fn;

  if( cmJsonWrite( p->jsH, cmJsonRoot(p->jsH), fn) != kOkJsRC )
    return cmErrMsg(&p->err,kWriteFileFailPrRC,"Preference writing failed.");

  p->dirtyFl = false;

  return rc;
}

void cmPrefsReport( cmPrH_t h )
{
  cmPr_t*  p  = _cmPrefsHandleToPtr(h);
  cmJsonReport(p->jsH);
}

//=============================================================================================
// cmPrefsTest()
//

void _cmPrintNode( const cmPrNode_t* np )
{

  const cmPrPath_t*    pp = np->pathPtr;
  const cmJsonNode_t* jnp = np->nodePtr;
  const cmJsonNode_t* jpp = jnp->ownerPtr;
  printf("id:%i type:0x%x parent:0x%x ",np->id,jnp->typeId,jpp==NULL?0:jpp->typeId);
  while( pp != NULL )
  {
    printf("%s ",pp->label);
    pp = pp->parentPtr;
  }
  printf("\n");

}

void _cmPrintNodes( const cmPrNode_t* np )
{
  if( np == NULL )
    return;

  _cmPrintNode(np);

  _cmPrintNodes(np->linkPtr);
}

/* example preferences file
{
  "cfg" : 
  {
    "inAudDevIdx" : 0     
    "outAudDevIdx" : 0     
    "syncInputFl" : false     
    "midiPortBufByteCnt" : 8192     
    "msgQueueByteCnt" : 64768     
    "devFramesPerCycle" : 128     
    "dspFramesPerCycle" : 64     
    "audioBufCnt" : 3     
    "srate" : 44100.000000     
    "chNames" : 
    {
      "left" : 0       
      "right" : 1       
      "array" : 
      [
        0 ,
        1 ,
        2 
      ]      
    }    
  }  
}
 */
  void cmPrefsTest( cmCtx_t* ctx, const char* ifn, const char* ofn )
{
  cmPrH_t h = cmPrNullHandle;
  if( cmPrefsInitialize(ctx,&h,ifn,NULL,NULL) != kOkPrRC )
    return;

  cmPr_t* p = _cmPrefsHandleToPtr(h);
  _cmPrintNodes(p->idChainPtr);

  unsigned id;
  id = cmPrefsId(h, "cfg/audioBufCnt", true );
  printf("id:%i\n",id);

  //id = cmPrefsId(h, "cfg/audioBuf", true );
  //printf("id:%i\n",id);
  
  int sr;
  unsigned n=1;
  cmPrefsGetInt(h, cmPrefsId(h,"cfg/srate",true), &sr, &n );
  printf("sr:%i %i\n",sr,n);

  cmPrefsGetInt(h, cmPrefsId(h,"cfg/chNames/array",true), &sr, &n );
  printf("sr:%i %i\n",sr,n);

  int arr[4];
  n = 4;
  cmPrefsGetInt(h, cmPrefsId(h,"cfg/chNames/array",true), arr, &n );
  printf("array:%i %i %i n=%i\n",arr[0],arr[1],arr[2],n);
  
  
  sr = 44100;
  n  = 1;
  cmPrefsSetIntArray(h, cmPrefsId(h,"cfg/srate",true), &sr, &n);
  cmPrefsGetInt(h, cmPrefsId(h,"cfg/chNames/array",true), &sr, &n );
  printf("sr:%i %i\n",sr,n);
 
  int sarr[] = {10,11,12,13 };
  n = sizeof(sarr)/sizeof(sarr[0]);
  cmPrefsSetIntArray(h, cmPrefsId(h,"cfg/chNames/array",true), sarr, &n);
  cmPrefsGetInt(h, cmPrefsId(h,"cfg/chNames/array",true), sarr, &n );
  printf("array:%i %i %i %i n=%i\n",sarr[0],sarr[1],sarr[2],sarr[3],n);

  int tarr[] = {20,21 };
  n = sizeof(tarr)/sizeof(tarr[0]);
  cmPrefsSetIntArray(h, cmPrefsId(h,"cfg/chNames/array",true), tarr, &n);
  cmPrefsGetInt(h, cmPrefsId(h,"cfg/chNames/array",true), tarr, &n );
  printf("array:%i %i  n=%i\n",tarr[0],tarr[1],n);

  cmPrefsCreateBool(h, cmInvalidId, "cfg/flags/flag0", 0, true );
  bool bv = cmPrefsBool(h, cmPrefsId(h,"cfg/flags/flag0",true));
  printf("bool:%i\n",bv);
  cmPrefsCreateString(h,cmInvalidId, "cfg/audioBufCnt",0,"hey");

  const cmChar_t* strArray[] =
  {
    "snap", "crackle", "pop"
  };
  cmPrefsCreateStringArray(h, cmInvalidId, "cfg/chNames/strList",0,strArray,3);

  cmPrefsWrite(h,ofn);

  cmPrefsFinalize(&h);
}
