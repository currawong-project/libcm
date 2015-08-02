
#ifndef cmDList_h
#define cmDList_h

#ifdef __cplusplus
extern "C" {
#endif

  enum
  {
    kOkDlRC = cmOkRC,
    kDuplicateIndexIdDlRC,
    kInvalidIndexDlRC,
    kIterNotFoundDlRC,
    kDataRecdNotFoundDlRC,
    
  };

  typedef unsigned   cmDlRC_t;
  typedef cmHandle_t cmDListH_t;
  typedef cmHandle_t cmDListIterH_t;

  extern cmDListH_t     cmDListNullHandle;
  extern cmDListIterH_t cmDListIterNullHandle;

  // Return <  0 if v0 <  v1
  //        == 0 if v0 == v1
  //        >  0 if v0 >  v1
  typedef int (*cmDListCmpFunc_t)( void* arg, const void* v0, unsigned v0N, const void* v1, unsigned v1N );
  
  typedef void (*cmDListIndexFreeFunc_t)( unsigned indexId, void* arg );

  // If 'f' is not NULL then a default index with an indexId==0 will be automatically created.
  cmDlRC_t cmDListAlloc( cmCtx_t* ctx, cmDListH_t* hp, cmDListCmpFunc_t f, void* farg );
  cmDlRC_t cmDListFree(   cmDListH_t* hp );
  bool     cmDListIsValid( cmDListH_t h );
  cmDlRC_t cmDListInsert( cmDListH_t  h, const void* recd, unsigned recdByteN );
  cmDlRC_t cmDListDelete( cmDListH_t  h, const void* recd );

  
  cmDlRC_t cmDListIndexAlloc(   cmDListH_t h, unsigned indexId, cmDListCmpFunc_t f, void* farg );
  cmDlRC_t cmDListIndexFree(    cmDListH_t h, unsigned indexId );
  cmDlRC_t cmDListIndexSetFreeFunc(cmDListH_t h, unsigned indexId, cmDListIndexFreeFunc_t func );

 
  cmDlRC_t    cmDListIterAlloc( cmDListH_t h, cmDListIterH_t* iHp, unsigned indexId );
  cmDlRC_t    cmDListIterFree(      cmDListIterH_t* iHp );
  bool        cmDListIterIsValid(   cmDListIterH_t  iH );
  cmDlRC_t    cmDListIterSeekBegin( cmDListIterH_t  iH );
  cmDlRC_t    cmDListIterSeekEnd(   cmDListIterH_t  iH );
  const void* cmDListIterGet(       cmDListIterH_t  iH, unsigned* recdByteNRef );
  const void* cmDListIterPrev(      cmDListIterH_t  iH, unsigned* recdByteNRef );
  const void* cmDListIterNext(      cmDListIterH_t  iH, unsigned* recdByteNRef );
  const void* cmDListIterFind(      cmDListIterH_t  iH, const void* key, unsigned keyN, unsigned* recdByteNRef);


#ifdef __cplusplus
}
#endif

#endif
