
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

  // If 'cmpFunc' is not NULL then a default index with an indexId==0 will be automatically created.
  cmDlRC_t cmDListAlloc( cmCtx_t* ctx, cmDListH_t* hp, cmDListCmpFunc_t cmpFunc, void* funcArg );
  cmDlRC_t cmDListFree(    cmDListH_t* hp );
  bool     cmDListIsValid( cmDListH_t  h );

  // Set resyncFl to automatically update the indexes to reflect the new record, otherwise
  // cmDListIndexUpdateAll() should be called to resynchronize the data list to the indexes.
  // If many inserts are to be performed with no intervening accesses to the list then it
  // is more efficient to defer updating the indexes until all the inserts are completed.
  cmDlRC_t cmDListInsert(  cmDListH_t  h, const void* recd, unsigned recdByteN, bool resyncFl );

  // Delete a data record.
  // 'recd' should be set to a value returned via one of the iterator accessors.
  // If 'resyncFl' is set then the indexes and interators will be
  // automatically synchronized with the data list after the deletion.
  // If 'resyncFl' is not set then the client must call cmDListIndexUpdateAll()
  // to resynchronize the indexes and iterators after the deletion.
  // Note that if multiple records are to be deleted without intervening accesses
  // to the list then it is more efficient to defer update the indexes until
  // all the deletions are completed.
  cmDlRC_t cmDListDelete(  cmDListH_t  h, const void* recd, bool resyncFl );

  // Allocate a new index.  'indexId' is used to identify this index and must be unique among all
  // previously allocated indexes.
  cmDlRC_t cmDListIndexAlloc(      cmDListH_t h, unsigned indexId, cmDListCmpFunc_t cmpFunc, void* funcArg );
  cmDlRC_t cmDListIndexFree(       cmDListH_t h, unsigned indexId );

  // Refresh all the indexes.  This function should be called after new records are inserted
  // via cmDListInsert(..,false).
  cmDlRC_t cmDListIndexUpdateAll(  cmDListH_t h );

  // Set a function to be called when indexes are released.  
  cmDlRC_t cmDListIndexSetFreeFunc(cmDListH_t h, unsigned indexId, cmDListIndexFreeFunc_t func );

  // Allocate an interator. By default the new iterator is pointing to the first record
  // in the index identified by 'indexId'.
  cmDlRC_t    cmDListIterAlloc( cmDListH_t h, cmDListIterH_t* iHp, unsigned indexId );
  cmDlRC_t    cmDListIterFree(      cmDListIterH_t* iHp );
  bool        cmDListIterIsValid(   cmDListIterH_t  iH );

  // Set the current iteration location to the begin/end of the index it is attached to.
  cmDlRC_t    cmDListIterSeekBegin( cmDListIterH_t  iH );
  cmDlRC_t    cmDListIterSeekEnd(   cmDListIterH_t  iH );

  // Return the current record this iterator is pointing to.
  const void* cmDListIterGet(       cmDListIterH_t  iH, unsigned* recdByteNRef );

  // Return the current record this iterator is pointint to and advance the iterator.
  const void* cmDListIterPrev(      cmDListIterH_t  iH, unsigned* recdByteNRef );
  const void* cmDListIterNext(      cmDListIterH_t  iH, unsigned* recdByteNRef );

  // Make the record which matches 'key' the current iterator.
  // The match is made by using the compare function which is assigned to the index
  // which this iterator is attached to.
  const void* cmDListIterFind(      cmDListIterH_t  iH, const void* key, unsigned keyN, unsigned* recdByteNRef);


#ifdef __cplusplus
}
#endif

#endif
