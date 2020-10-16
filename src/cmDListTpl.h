//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.

//( { file_desc:"Template 'include' code for using cmDList as a template." kw:[container] }

// The following two macros must be defined prior to including this code:
// #define cmSFX(a) a##_MySuffix
// #define cmTYPE   My_Type 

// Also define cmGEN_HDR to generate the .h code and/or
// cmGEN_CODE to generate the .c code

#ifdef cmGEN_HDR

typedef int (*cmSFX(cmDListFunc))( void* arg, const cmTYPE* v0, const cmTYPE* v1 );

cmDlRC_t cmSFX(cmDListAlloc)( cmCtx_t* ctx, cmDListH_t* hp, cmSFX(cmDListFunc) func, void* funcArg );

cmDlRC_t cmSFX(cmDListInsert)( cmDListH_t  h, const cmTYPE* recd, bool resyncFl );
cmDlRC_t cmSFX(cmDListDelete)( cmDListH_t  h, const cmTYPE* recd, bool resyncFl );

cmDlRC_t cmSFX(cmDListAllocIndex)( cmDListH_t h, unsigned indexId, cmSFX(cmDListFunc) f, void* farg );

const cmTYPE* cmSFX(cmDListIterGet)(       cmDListIterH_t  iH );
const cmTYPE* cmSFX(cmDListIterPrev)(      cmDListIterH_t  iH );
const cmTYPE* cmSFX(cmDListIterNext)(      cmDListIterH_t  iH );
const cmTYPE* cmSFX(cmDListIterFind)(      cmDListIterH_t  iH, const cmTYPE* key);

#endif // cmGEN_HDR


#ifdef cmGEN_CODE

typedef struct
{
  cmSFX(cmDListFunc) func;
  void*               funcArg;
} cmSFX(_cmDListArg);

// This function is called when the index identified by indexId is about to be deleted.
// It is used to cleanup the arg record created by cmSFX(cmDListIndexAlloc()).
void cmSFX(_cmDListIndexOnFree)( unsigned indexId, void* arg )
{
  cmMemFree(arg);
}

// Proxy function used to cast generic compare function to the user defined compare function.
int cmSFX(_cmDListCmp)( void* arg, const void* v0, unsigned vn0, const void* v1, unsigned vn1 )
{
  assert(vn0==vn1);
  cmSFX(_cmDListArg)* a = (cmSFX(_cmDListArg)*)arg;
  return a->func(a->funcArg,(const cmTYPE*)v0,(const cmTYPE*)v1);
}

cmDlRC_t cmSFX(cmDListAlloc)( cmCtx_t* ctx, cmDListH_t* hp, cmSFX(cmDListFunc) func, void* funcArg )
{
  cmDlRC_t rc;
  cmSFX(_cmDListArg)* a = NULL;

  if( func != NULL )
  {
    // allocate a record to redirect the compare function callback
    a = cmMemAllocZ(cmSFX(_cmDListArg),1);
    a->func    = func;
    a->funcArg = funcArg;
  }
  
  if((rc = cmDListAlloc(ctx,hp,cmSFX(_cmDListCmp),a)) != kOkDlRC )
    return rc;

  if( func != NULL )
    if((rc = cmDListIndexSetFreeFunc(*hp,0,cmSFX(_cmDListIndexOnFree))) != kOkDlRC )
      cmDListFree(hp);

  return rc;
}


cmDlRC_t cmSFX(cmDListIndexAlloc)(   cmDListH_t h, unsigned indexId, cmSFX(cmDListFunc) func, void* funcArg )
{
  cmDlRC_t rc;
  
  // allocate a record to redirect the compare function callback
  cmSFX(_cmDListArg)* a = cmMemAllocZ(cmSFX(_cmDListArg),1);
  a->func    = func;
  a->funcArg = funcArg;

  // allocate the index
  if((rc = cmDListIndexAlloc(h,indexId,cmSFX(_cmDListCmp),a)) != kOkDlRC )
  {
    cmMemFree(a);
    goto errLabel;
  }

  // set the index clean up handler
  if((rc = cmDListIndexSetFreeFunc(h,indexId,cmSFX(_cmDListIndexOnFree))) != kOkDlRC )
    cmDListIndexFree(h,indexId);
  
 errLabel:
  return rc;
}

cmDlRC_t cmSFX(cmDListInsert)( cmDListH_t  h, const cmTYPE* recd, bool resyncFl )
{ return cmDListInsert(h,recd,sizeof(cmTYPE),resyncFl); }

cmDlRC_t cmSFX(cmDListDelete)( cmDListH_t  h, const cmTYPE* recd, bool resyncFl )
{ return cmDListDelete(h,recd,resyncFl);  }


const cmTYPE* cmSFX(cmDListIterGet)(  cmDListIterH_t  iH )
{ return (const cmTYPE*)cmDListIterGet(iH,NULL);}

const cmTYPE* cmSFX(cmDListIterPrev)(      cmDListIterH_t  iH )
{ return (const cmTYPE*)cmDListIterPrev(iH,NULL); }

const cmTYPE* cmSFX(cmDListIterNext)(      cmDListIterH_t  iH )
{ return (const cmTYPE*)cmDListIterNext(iH,NULL); }

const cmTYPE* cmSFX(cmDListIterFind)(      cmDListIterH_t  iH, const cmTYPE* key)
{  return (const cmTYPE*)cmDListIterFind(iH,key,sizeof(cmTYPE),NULL); }

#endif // cmGEN_CODE

//)

#undef cmSFX
#undef cmTYPE

