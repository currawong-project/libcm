#ifndef cmAudioFileMgr_h
#define cmAudioFileMgr_h

#ifdef __cplusplus
extern "C" {
#endif
  enum
  {
    kOkAfmRC = cmOkRC,
    kAudioFileFailAfmRC
  };

  typedef cmHandle_t cmAfmH_t;
  typedef cmHandle_t cmAfmFileH_t;
  typedef cmRC_t     cmAfmRC_t;

  extern cmAfmH_t     cmAfmNullHandle;
  extern cmAfmFileH_t cmAfmFileNullHandle;


  //----------------------------------------------------------------------------
  // Audio Files 
  //----------------------------------------------------------------------------  
  cmAfmRC_t cmAfmFileOpen( cmAfmH_t h, cmAfmFileH_t* fhp, const cmChar_t* audioFn, unsigned fileId, cmAudioFileInfo_t* afInfo );
  cmAfmRC_t cmAfmFileClose( cmAfmFileH_t* fhp );
  bool      cmAfmFileIsValid( cmAfmFileH_t fh );
  
  // Return the application supplied file id associated with this file.
  // This value was set by the 'fileId' argument to cmAfmFileOpen().
  unsigned cmAfmFileId( cmAfmFileH_t fh );

  // Return the file handle associated with this file.
  cmAudioFileH_t cmAfmFileHandle( cmAfmFileH_t fh );
  
  // Return a pointer to the information record associated with this file.
  const cmAudioFileInfo_t* cmAfmFileInfo( cmAfmFileH_t fh );

  // Summarize min and max values of the downSampled audio file.
  // The summary is kept in an internal cache which is used to 
  // optimize the time required to complete later calls to cmAfmFileGetSummary(). 
  // 'downSampleFactor' is the count of samples per summary point.
  cmAfmRC_t cmAfmFileSummarize( cmAfmFileH_t fh, unsigned downSampleFactor );

  // Return a summary of the samples in the range audio file range
  // begSmpIdx:begSmpIdx+smpCnt-1 reduced or expanded to 'outCnt' values
  // in minV[outCnt] and maxV[outCnt].
  // If 'outCnt' is equal to 'smpCnt' then the actual sample values are returned. 
  cmAfmRC_t cmAfmFileGetSummary( cmAfmFileH_t fh, unsigned chIdx, unsigned begSmpIdx, unsigned smpCnt, cmSample_t* minV, cmSample_t* maxV, unsigned outCnt );


  //----------------------------------------------------------------------------
  // Audio File Manager
  //----------------------------------------------------------------------------  
  cmAfmRC_t    cmAfmCreate( cmCtx_t* ctx, cmAfmH_t* hp );
  cmAfmRC_t    cmAfmDestroy( cmAfmH_t* hp );
  bool         cmAfmIsValid( cmAfmH_t h );
  cmAfmFileH_t cmAfmIdToHandle( cmAfmH_t h, unsigned fileId );

    


#ifdef __cplusplus
}
#endif


#endif
