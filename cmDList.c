#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmDList.h"


typedef struct cmDListRecd_str
{
  void*                   dV;
  unsigned                dN;
  struct cmDListRecd_str* prev;
  struct cmDListRecd_str* next;
} cmDListRecd_t;

typedef struct cmDListIndexRecd_str
{
  cmDListRecd_t*               r;
  struct cmDListIndexRecd_str* prev;
  struct cmDListIndexRecd_str* next;
} cmDListIndexRecd_t;

typedef struct cmDListIndex_str
{
  unsigned                 id;
  cmDListCmpFunc_t         cmpFunc;
  void*                    funcArg;
  cmDListIndexFreeFunc_t   freeFunc;
  cmDListIndexRecd_t*      first;
  cmDListIndexRecd_t*      last;
  unsigned                 recdN;
  struct cmDListIndex_str* link;
} cmDListIndex_t;


struct cmDList_str;

typedef struct cmDListIter_str
{
  struct cmDList_str*     p;
  cmDListIndex_t*         x;
  cmDListIndexRecd_t*     s;
  struct cmDListIter_str* link;
} cmDListIter_t;

typedef struct cmDList_str
{
  cmErr_t err;
  cmDListRecd_t*  first;    
  cmDListRecd_t*  last;
  unsigned        recdN; 
  
  cmDListIndex_t* indexes;
  cmDListIter_t*  iters;
} cmDList_t;

cmDListH_t     cmDListNullHandle      = cmSTATIC_NULL_HANDLE;
cmDListIterH_t cmDListIterNullHandle  = cmSTATIC_NULL_HANDLE;

cmDList_t* _cmDListHandleToPtr( cmDListH_t h )
{
  cmDList_t* p = (cmDList_t*)h.h;
  assert( p!=NULL );
  return p;
}

cmDListIndex_t* _cmDListIdToIndex( cmDList_t* p, unsigned indexId )
{
  cmDListIndex_t* x = p->indexes;
  for(; x!=NULL; x=x->link)
    if( x->id == indexId )
      return x;
  
  return NULL;
}

void _cmDListIndexAllocRecds( cmDListIndex_t* x, unsigned n )
{
  unsigned i;
  for(i=0; i<n; ++i)
  {
    cmDListIndexRecd_t* s = cmMemAllocZ(cmDListIndexRecd_t,1);
    s->prev = x->last;
    
    if( x->last != NULL )
      x->last->next = s;
    else
    {
      assert( x->first == NULL );
      x->first = s;
    }

    x->last = s;
  }

  x->recdN += n;

}

void _cmDListIndexUpdate( cmDList_t* p, cmDListIndex_t* x )
{
  cmDListIndexRecd_t* first = NULL;
  cmDListIndexRecd_t* last  = NULL;
  cmDListIndexRecd_t* avail = x->first;

  // for each data recd
  cmDListRecd_t* r = p->first;
  for(; r!=NULL; r=r->next)
  {
    // get the next available index record
    cmDListIndexRecd_t* a = avail;
    
    avail = avail->next;

    // The count of index records and data records should always be the same.
    assert( a != NULL );
    a->r = r;
    
    cmDListIndexRecd_t* s = first;

    // for each index recd that has already been sorted
    for(; s!=NULL; s=s->next)
      if( x->cmpFunc( x->funcArg, r->dV, r->dN, s->r->dV, s->r->dN ) < 0 )
      {
        // r is less than s->r
        // insert 'a' prior to 's' in the index

        a->next = s;
        a->prev = s->prev;

        // if 's' is not first
        if( s->prev != NULL )
          s->prev->next = a;
        else
        { // 's' was first - now 'a' is first
          assert( s == first );
          first = a;
        }
        
        s->prev = a;
                
        break;
      }

    // No records are greater than r or the index is empty - 'a' is last in the index.
    if( s == NULL )
    {      
      // insert 'a' after 'last'

      // if the index is empty
      if( last == NULL )
      {
        first   = a;
        a->prev = NULL;
      }
      else // make 'a' last in the index
      {
        a->prev    = last;
        a->next    = NULL;
        assert( last->next == NULL );
        last->next = a;
      }

      a->next = NULL;
      last    = a;
    }


    
  }

  x->first = first;
  x->last  = last;
}

void _cmDListRecdFree( cmDList_t* p, cmDListRecd_t* r )
{
  if( r->prev != NULL )
    r->prev->next = r->next;
  else
    p->first = r->next;

  if( r->next != NULL )
    r->next->prev = r->prev;
  else
    p->last = r->prev;

  cmMemFree(r);
}

// Unlink and free the index record s;
void _cmDListIndexRecdFree( cmDListIndex_t* x, cmDListIndexRecd_t* s )
{
  if( s->prev != NULL )
    s->prev->next = s->next;
  else
    x->first = s->next;

  if( s->next != NULL )
    s->next->prev = s->prev;
  else
    x->last = s->prev;
  
  cmMemFree(s);  
}

// Unlink and release an index.
void _cmDListIndexFree( cmDList_t* p, cmDListIndex_t* x )
{
  if( x == NULL )
    return;
  
  cmDListIndex_t* x0 = NULL;
  cmDListIndex_t* x1 = p->indexes;

  // unlink 'x' from p->indexes
  while( x1 != NULL )
  {
    if( x1 == x )
    {
      // x is the first index
      if( x0 == NULL )
      {
        assert( x1 = p->indexes );
        p->indexes = x->link;
      }
      else
      {
        x0->link = x1->link;        
      }

      break;
    }
    
    x0 = x1;
    x1 = x1->link;
  }

  // 'x' must have been found
  assert( x1 == x );


  // release each index record in 'x'
  cmDListIndexRecd_t* s  = x->first;
  while( s != NULL )
  {
    cmDListIndexRecd_t* ns = s->next;
    cmMemFree(s);
    s = ns;
  }

  if( x->freeFunc != NULL )
    x->freeFunc(x->id,x->funcArg);
  
  cmMemFree(x);

}

// Unlink and release the iterator 'e'.
cmDlRC_t _cmDListIterFree( cmDListIter_t* e )
{
  cmDList_t*     p  = e->p;
  cmDListIter_t* e0 = NULL;
  cmDListIter_t* e1 = p->iters;
  
  while( e1 != NULL )
  {
    if( e1 == e )
    {
      if( e0 == NULL )
        p->iters = e1->link;
      else
        e0->link = e1->link;

      cmMemFree(e0);
      break;
    }

    e0 = e1;
    e1 = e1->link;
  }

  if( e1 == NULL )
    return cmErrMsg(&p->err,kIterNotFoundDlRC,"The delete target iterator could not be found.");

  return kOkDlRC;  
}

cmDlRC_t _cmDListFree( cmDList_t* p )
{
  cmDlRC_t rc = kOkDlRC;

  // release all indexes
  while( p->indexes != NULL )
    _cmDListIndexFree(p,p->indexes);

  while( p->iters != NULL )
    _cmDListIterFree(p->iters);

  // release all data records
  while( p->first != NULL )
    _cmDListRecdFree(p,p->first);

  cmMemFree(p);
  return rc;
}

cmDlRC_t _cmDListIndexAlloc( cmDList_t* p, unsigned indexId, cmDListCmpFunc_t func, void* funcArg )
{
  cmDListIndex_t* x;

  if((x =_cmDListIdToIndex(p, indexId )) != NULL )
    return cmErrMsg(&p->err,kDuplicateIndexIdDlRC,"The indexId '%i' has already been used.",indexId);

  x = cmMemAllocZ(cmDListIndex_t,1);

  x->id      = indexId;
  x->cmpFunc = func;
  x->funcArg = funcArg;
  x->link    = p->indexes;
  p->indexes = x;

  
  _cmDListIndexAllocRecds(x,p->recdN);
  _cmDListIndexUpdate(p,x);

  return kOkDlRC;
  
}

cmDlRC_t cmDListAlloc( cmCtx_t* ctx, cmDListH_t* hp, cmDListCmpFunc_t func, void* funcArg )
{
  cmDlRC_t rc = kOkDlRC;
  
  if((rc = cmDListFree(hp)) != kOkDlRC )
    return rc;

  cmDList_t* p = cmMemAllocZ(cmDList_t,1);
  cmErrSetup(&p->err,&ctx->rpt,"cmDList");

  if( func!=NULL )
    if((rc = _cmDListIndexAlloc(p,0,func,funcArg)) != kOkDlRC )
      goto errLabel; 

  hp->h = p;

 errLabel:
  if( rc != kOkDlRC )
    _cmDListFree(p);

  return rc;
}

cmDlRC_t cmDListFree(   cmDListH_t* hp )
{
  cmDlRC_t rc;

  if( hp==NULL || cmDListIsValid(*hp)==false)
    return kOkDlRC;

  cmDList_t* p = _cmDListHandleToPtr(*hp);

  if((rc = _cmDListFree(p)) != kOkDlRC )
    return rc;

  hp->h = NULL;

  return rc;  
}

bool     cmDListIsValid( cmDListH_t h )
{ return h.h != NULL; }

cmDlRC_t cmDListInsert( cmDListH_t  h, const void* recd, unsigned recdByteN )
{
  cmDList_t*     p  = _cmDListHandleToPtr(h);
  char*          vp = cmMemAllocZ(char,sizeof(cmDListRecd_t) + recdByteN );
  cmDListRecd_t* r  = (cmDListRecd_t*)vp;
  
  r->dV = r + 1;
  r->dN = recdByteN;
  memcpy( r->dV, recd, recdByteN );

  // Add records at the end of the list.

  // If the list is not empty
  if( p->last != NULL )
    p->last->next = r;
  else
  {
    // The list was empty
    assert( p->first == NULL );
    p->first = r;
  }

  r->prev   = p->last;
  r->next   = NULL;
  p->last   = r;
  p->recdN += 1;

  // update the indexes
  cmDListIndex_t* x = p->indexes;
  for(; x!=NULL; x=x->link)
  {
    _cmDListIndexAllocRecds(x,1);
    _cmDListIndexUpdate(p,x);
  }
  
  return kOkDlRC;
}

cmDlRC_t cmDListDelete( cmDListH_t  h, const void* recd )
{
  cmDList_t*      p = _cmDListHandleToPtr(h);
  cmDListIndex_t* x = p->indexes;
  cmDListIndexRecd_t* s = NULL;
  cmDListRecd_t* r = NULL;
  
  for(; x!=NULL; x=x->link)
  {
    for(s=x->first; s!=NULL; s=s->next)
      if( s->r->dV == recd )
      {
        if( r == NULL )
          r = s->r;
        else
        {
          // the same data record should be found for all indexes
          assert( s->r == r );
        }
        
        _cmDListIndexRecdFree(x,s);
        
        break;
      }

    if( r == NULL )
      return cmErrMsg(&p->err,kDataRecdNotFoundDlRC,"The delete target data record could not be found.");
    
    // if the indexes are valid then the recd should always be found
    assert( s!=NULL );
  }

  // release the data record
  _cmDListRecdFree(p,r);

  return kOkDlRC;
  
}

cmDlRC_t cmDListIndexAlloc( cmDListH_t h, unsigned indexId, cmDListCmpFunc_t func, void* funcArg )
{
  cmDList_t*      p = _cmDListHandleToPtr(h);
  return _cmDListIndexAlloc(p,indexId,func,funcArg);
}

cmDlRC_t cmDListIndexFree( cmDListH_t h, unsigned indexId )
{
  cmDList_t*      p = _cmDListHandleToPtr(h);
  cmDListIndex_t* x;
  if((x = _cmDListIdToIndex(p,indexId)) == NULL )
    return cmErrMsg(&p->err,kInvalidIndexDlRC,"The indexId '%i' could not be found.",indexId);

  _cmDListIndexFree(p,x);

  return kOkDlRC;
}

cmDlRC_t cmDListIndexSetFreeFunc(cmDListH_t h, unsigned indexId, cmDListIndexFreeFunc_t func )
{
  cmDList_t* p = _cmDListHandleToPtr(h);
  cmDListIndex_t* x;
  if((x = _cmDListIdToIndex(p,indexId)) == NULL )
    return cmErrMsg(&p->err,kInvalidIndexDlRC,"The indexId '%i' could not be found.",indexId);

  x->freeFunc = func;
  return kOkDlRC;
}


cmDListIter_t* _cmDListIterHandleToPtr( cmDListIterH_t h )
{
  cmDListIter_t* e = (cmDListIter_t*)h.h;
  assert(e != NULL );
  return e;
}

cmDlRC_t    cmDListIterAlloc( cmDListH_t h, cmDListIterH_t* iHp, unsigned indexId )
{
  cmDlRC_t       rc = kOkDlRC;
  cmDList_t*      p = _cmDListHandleToPtr(h);
  cmDListIndex_t* x;

  if((rc = cmDListIterFree(iHp)) != kOkDlRC )
    return rc;
  
  if((x = _cmDListIdToIndex(p, indexId)) == NULL )
    return cmErrMsg(&p->err,kInvalidIndexDlRC,"The indexId '%i' could not be found.",indexId);

  cmDListIter_t* e = cmMemAllocZ(cmDListIter_t,1);

  e->p     = p;
  e->x     = x;
  e->s     = x->first;
  e->link  = p->iters;
  p->iters = e;

  iHp->h = e;

  return rc;
  
}

cmDlRC_t    cmDListIterFree(  cmDListIterH_t* iHp )
{
  cmDlRC_t rc;
  
  if( iHp==NULL || cmDListIterIsValid(*iHp)==false )
    return kOkDlRC;

  cmDListIter_t* e = _cmDListIterHandleToPtr(*iHp);

  if(( rc = _cmDListIterFree( e )) != kOkDlRC )
    return rc;

  iHp->h = NULL;
  
  return rc;  
}

bool        cmDListIterIsValid( cmDListIterH_t iH )
{ return iH.h != NULL; }


cmDlRC_t    cmDListIterSeekBegin( cmDListIterH_t  iH )
{
  cmDListIter_t* e = _cmDListIterHandleToPtr(iH);
  e->s = e->x->first;
  return kOkDlRC;
}

cmDlRC_t    cmDListIterSeekLast( cmDListIterH_t  iH )
{
  cmDListIter_t* e = _cmDListIterHandleToPtr(iH);
  e->s = e->x->last;
  return kOkDlRC;
}

const void* _cmDListIterGet(   cmDListIter_t*  e, unsigned* recdByteNRef )
{
  if( e->s == NULL )
  {
    if( recdByteNRef != NULL )
      *recdByteNRef = 0;
    return NULL;
  }

  if( recdByteNRef != NULL )
    *recdByteNRef = e->s->r->dN;

  return e->s->r->dN==0 ? NULL : e->s->r->dV;
}

const void* cmDListIterGet(   cmDListIterH_t  iH, unsigned* recdByteNRef )
{
  cmDListIter_t* e = _cmDListIterHandleToPtr(iH);

  return _cmDListIterGet(e,recdByteNRef);
}

const void* cmDListIterPrev(  cmDListIterH_t  iH, unsigned* recdByteNRef )
{
  cmDListIter_t* e = _cmDListIterHandleToPtr(iH);
  const void*   rv = _cmDListIterGet(e,recdByteNRef);

  if( e->s != NULL )
    e->s = e->s->prev;

  return rv;
}

const void* cmDListIterNext( cmDListIterH_t  iH, unsigned* recdByteNRef )
{
  cmDListIter_t* e = _cmDListIterHandleToPtr(iH);
  const void*   rv = _cmDListIterGet(e,recdByteNRef);

  if( e->s != NULL )
    e->s = e->s->next;

  return rv;
}

const void* cmDListIterFind( cmDListIterH_t  iH, const void* key, unsigned keyN, unsigned* recdByteNRef)
{
  cmDListIter_t* e = _cmDListIterHandleToPtr(iH);

  cmDListIndexRecd_t* s = e->s;

  for(; s!=NULL; s=s->next)
    if( e->x->cmpFunc( e->x->funcArg, s->r->dV, s->r->dN, key, keyN ) == 0 )
    {
      e->s = s;
      return _cmDListIterGet(e,recdByteNRef);
    }

  if( recdByteNRef != NULL )
    *recdByteNRef = 0;

  return NULL;
}
