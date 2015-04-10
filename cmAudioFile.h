/// \file cmAudioFile.h
/// \brief Audio file reader/writer class.
///
/// This class supports reading uncompressed AIFF and WAV files and writing uncompressed AIFF files.
///  The reading and writing routines are known to work with 8,16,24, and 32 bit integer sample formats.
///
/// Testing and example usage for this API can be found in cmProcTest.c cmAudioReadWriteTest().
///
/// Usage example:
/// \snippet cmAudioFile.c cmAudioFileExample

#ifndef cmAudioFile_h
#define cmAudioFile_h

#ifdef __cplusplus
extern "C" {
#endif

#ifndef cmAudioFile_MAX_FRAME_READ_CNT
/// Maximum number of samples which will be read in one call to fread().
/// This value is only significant in that an internal buffer is created on the stack
/// whose size must be limited to prevent stack overflows.
#define cmAudioFile_MAX_FRAME_READ_CNT (8192) 
#endif


  /// Audio file result codes.
  enum
  {
    kOkAfRC = 0,
    kOpenFailAfRC,
    kReadFailAfRC,
    kWriteFailAfRC,
    kSeekFailAfRC,
    kCloseFailAfRC,
    kNotAiffAfRC,
    kInvalidBitWidthAfRC,
    kInvalidFileModeAfRC,
    kInvalidHandleAfRC,
    kInvalidChCountAfRC,
    kUnknownErrAfRC
  };

  /// Informational flags used by audioFileInfo
  enum
  {
    kAiffAfFl        = 0x01,    ///< this is an AIFF file 
    kWavAfFl         = 0x02,    ///< this is a WAV file 
    kSwapAfFl        = 0x04,    ///< file header bytes must be swapped
    kAifcAfFl        = 0x08,    ///< this is an AIFC file
    kSwapSamplesAfFl = 0x10     ///< file sample bytes must be swapped
  };


  /// Constants
  enum
  {
    kAudioFileLabelCharCnt = 256,

    kAfBextDescN       = 256,
    kAfBextOriginN     = 32,
    kAfBextOriginRefN  = 32,
    kAfBextOriginDateN = 10,
    kAfBextOriginTimeN = 8
  };

  /// Aiff marker record
  typedef struct
  {
    unsigned    id;
    unsigned    frameIdx;
    char        label[kAudioFileLabelCharCnt];
  } cmAudioFileMarker_t;

  /// Broadcast WAV header record As used by ProTools audio files. See http://en.wikipedia.org/wiki/Broadcast_Wave_Format
  /// When generated from Protools the timeRefLow/timeRefHigh values appear to actually refer
  /// to the position on the Protools time-line rather than the wall clock time.
  typedef struct
  {
    char     desc[      kAfBextDescN       + 1 ];
    char     origin[    kAfBextOriginN     + 1 ];
    char     originRef[ kAfBextOriginRefN  + 1 ];
    char     originDate[kAfBextOriginDateN + 1 ];
    char     originTime[kAfBextOriginTimeN + 1 ];
    unsigned timeRefLow;   // sample count since midnight low word
    unsigned timeRefHigh;  // sample count since midnight high word
  } cmAudioFileBext_t;

  /// Audio file information record used by audioFileNew and audioFileOpen
  typedef struct 
  {
    unsigned             bits;        ///< bits per sample
    unsigned             chCnt;       ///< count of audio file channels
    double               srate;       ///< audio file sample rate in samples per second
    unsigned             frameCnt;    ///< total number of sample frames in the audio file
    unsigned             flags;       ///< informational flags 
    unsigned             markerCnt;   ///< count of markers in markerArray
    cmAudioFileMarker_t* markerArray; ///< array of markers 
    cmAudioFileBext_t    bextRecd;    ///< only used with Broadcast WAV files
  } cmAudioFileInfo_t;


  
  typedef cmHandle_t cmAudioFileH_t;    ///< opaque audio file handle   
  extern cmAudioFileH_t cmNullAudioFileH;  ///< NULL audio file handle

  /// Create an audio file handle and optionally use the handle to open an audio file.
  ///
  /// \param  fn         The audio file name to open or NULL to create the audio file handle without opening the file.
  /// \param  infoPtr    A pointer to an audioFileInfo record to be filled when the file is open or NULL to ignore.
  /// \param  rcPtr      A pointer to a result code to be set in the event of a runtime error or NULL to ignore.
  /// \param  rpt        A pointer to a cmRpt_t object which error messages from this class will be directed to.
  /// \retval cmAudioFileH_t A new audio file handle.
  ///
  cmAudioFileH_t cmAudioFileNewOpen( const cmChar_t* fn, cmAudioFileInfo_t* infoPtr, cmRC_t* rcPtr, cmRpt_t* rpt ); 

  /// Open an audio file for writing
  cmAudioFileH_t cmAudioFileNewCreate( const cmChar_t* fn, double srate, unsigned bits, unsigned chCnt, cmRC_t* rcPtr, cmRpt_t* rpt );


  /// Open an audio file for reading using a handle returned from an earlier call to audioFileNewXXX().
  ///
  /// \param  h          A file handle returned from and earlier call to cmAudioFileNewOpen() or cmAudioFileNewCreate().
  /// \param  fn         The audio file name to open or NULL to create the audio file handle without opening the file.
  /// \param  infoPtr    A pointer to an audioFileInfo record to be filled when the file is open or NULL to ignore.
  /// \retval Returns an cmRC_t value indicating the success (kOkAfRC) or failure of the call.
  ///
  /// If the audio file handle 'h' refers to an open file then it is automatically closed prior to being
  /// reopened with the new file.
  cmRC_t     cmAudioFileOpen(       cmAudioFileH_t h, const cmChar_t* fn, cmAudioFileInfo_t* infoPtr );

  /// Open an audio file for writing.  The type of the audio file, AIF or WAV
  /// is determined by the file name extension.
  cmRC_t     cmAudioFileCreate(     
    cmAudioFileH_t h,    ///< Handle returned from an earlier call to cmAudioFileNewCreate() or cmAudioFileNewOpen().
    const cmChar_t* fn,  ///< File name of the new file.
    double srate,        ///< Sample rate of the new file.
    unsigned bits,       ///< Sample word width for the new file in bits (must be 8,16,24 or 32).
    unsigned chCnt       ///< Audio channel count for the new file.
                                    );

  /// Close a the file associated with handle 'h' but do not release the handle.
  /// If the file was opened for writing (cmAudioFileCreate()) then this function will
  /// write the file header prior to closing the file.
  cmRC_t     cmAudioFileClose(      cmAudioFileH_t* h );

  /// Close the file associated with handle 'h' (via an internal call to 
  /// cmAudioFileClose()) and release the handle and any resources
  /// associated with it.  This is the complement to cmAudioFileOpen/Create().
  cmRC_t     cmAudioFileDelete(     cmAudioFileH_t* h );

  /// Return true if the handle is not closed or deleted.
  bool       cmAudioFileIsValid(    cmAudioFileH_t h );

  /// Return true if the handle is open.
  bool       cmAudioFileIsOpen(     cmAudioFileH_t h );

  /// Return true if the current file position is at the end of the file.
  bool       cmAudioFileIsEOF(      cmAudioFileH_t h );

  /// Return the current file position as a frame index.
  unsigned   cmAudioFileTell(       cmAudioFileH_t h );

  /// Set the current file position as an offset from the first frame.
  cmRC_t     cmAudioFileSeek(       cmAudioFileH_t h, unsigned frmIdx );

  /// \name Sample Reading Functions.
  ///@{
  /// Fill a user suppled buffer with up to frmCnt samples.
  /// If less than frmCnt samples are available at the specified audio file location then the unused
  /// buffer space is set to zero. Check *actualFrmCntPtr for the count of samples actually available
  /// in the return buffer.  Functions which do not include a begFrmIdx argument begin reading from
  /// the current file location (see cmAudioFileSeek()). The buf argument is always a pointer to an
  /// array of pointers of length chCnt.  Each channel buffer specified in buf[] must contain at least
  /// frmCnt samples.
  ///
  /// \param h               An audio file handle returned from an earlier call to audioFileNew()
  /// \param fn              The name of the audio file to read.
  /// \param begFrmIdx       The frame index of the first sample to read. Functions that do not use this parameter begin reading at the current file location (See cmAudioFileTell()).
  /// \param frmCnt          The number of samples allocated in buf.
  /// \param chIdx           The index of the first channel to read.
  /// \param chCnt           The count of channels to read.
  /// \param buf             An array containing chCnt pointers to arrays of frmCnt samples.
  /// \param actualFrmCntPtr The number of frames actually written to the return buffer (ignored if NULL)

  cmRC_t     cmAudioFileReadInt(    cmAudioFileH_t h, unsigned frmCnt, unsigned chIdx, unsigned chCnt, int**    buf, unsigned* actualFrmCntPtr );
  cmRC_t     cmAudioFileReadFloat(  cmAudioFileH_t h, unsigned frmCnt, unsigned chIdx, unsigned chCnt, float**  buf, unsigned* actualFrmCntPtr );
  cmRC_t     cmAudioFileReadDouble( cmAudioFileH_t h, unsigned frmCnt, unsigned chIdx, unsigned chCnt, double** buf, unsigned* actualFrmCntPtr );

  cmRC_t     cmAudioFileGetInt(    const char* fn, unsigned begFrmIdx, unsigned frmCnt, unsigned chIdx, unsigned chCnt, int**    buf, unsigned* actualFrmCntPtr, cmAudioFileInfo_t* afInfoPtr, cmRpt_t* rpt );
  cmRC_t     cmAudioFileGetFloat(  const char* fn, unsigned begFrmIdx, unsigned frmCnt, unsigned chIdx, unsigned chCnt, float**  buf, unsigned* actualFrmCntPtr, cmAudioFileInfo_t* afInfoPtr, cmRpt_t* rpt );
  cmRC_t     cmAudioFileGetDouble( const char* fn, unsigned begFrmIdx, unsigned frmCnt, unsigned chIdx, unsigned chCnt, double** buf, unsigned* actualFrmCntPtr, cmAudioFileInfo_t* afInfoPtr, cmRpt_t* rpt );

  ///@}

  /// \name Sum the returned samples into the output buffer.
  ///@{
  cmRC_t     cmAudioFileReadSumInt(    cmAudioFileH_t h, unsigned frmCnt, unsigned chIdx, unsigned chCnt, int**    buf, unsigned* actualFrmCntPtr );
  cmRC_t     cmAudioFileReadSumFloat(  cmAudioFileH_t h, unsigned frmCnt, unsigned chIdx, unsigned chCnt, float**  buf, unsigned* actualFrmCntPtr );
  cmRC_t     cmAudioFileReadSumDouble( cmAudioFileH_t h, unsigned frmCnt, unsigned chIdx, unsigned chCnt, double** buf, unsigned* actualFrmCntPtr );

  cmRC_t     cmAudioFileGetSumInt(    const char* fn, unsigned begFrmIdx, unsigned frmCnt, unsigned chIdx, unsigned chCnt, int**    buf, unsigned* actualFrmCntPtr, cmAudioFileInfo_t* afInfoPtr, cmRpt_t* rpt );
  cmRC_t     cmAudioFileGetSumFloat(  const char* fn, unsigned begFrmIdx, unsigned frmCnt, unsigned chIdx, unsigned chCnt, float**  buf, unsigned* actualFrmCntPtr, cmAudioFileInfo_t* afInfoPtr, cmRpt_t* rpt );
  cmRC_t     cmAudioFileGetSumDouble( const char* fn, unsigned begFrmIdx, unsigned frmCnt, unsigned chIdx, unsigned chCnt, double** buf, unsigned* actualFrmCntPtr, cmAudioFileInfo_t* afInfoPtr, cmRpt_t* rpt );
  ///@}

  ///@}

  /// \name Sample Writing Functions
  ///@{
  cmRC_t    cmAudioFileWriteInt(    cmAudioFileH_t h, unsigned frmCnt, unsigned chCnt, int**    bufPtrPtr );
  cmRC_t    cmAudioFileWriteFloat(  cmAudioFileH_t h, unsigned frmCnt, unsigned chCnt, float**  bufPtrPtr );
  cmRC_t    cmAudioFileWriteDouble( cmAudioFileH_t h, unsigned frmCnt, unsigned chCnt, double** bufPtrPtr );

  cmRC_t    cmAudioFileWriteFileInt(    const char* fn, double srate, unsigned bit, unsigned frmCnt, unsigned chCnt, int**    bufPtrPtr, cmRpt_t* rpt );
  cmRC_t    cmAudioFileWriteFileFloat(  const char* fn, double srate, unsigned bit, unsigned frmCnt, unsigned chCnt, float**  bufPtrPtr, cmRpt_t* rpt );
  cmRC_t    cmAudioFileWriteFileDouble( const char* fn, double srate, unsigned bit, unsigned frmCnt, unsigned chCnt, double** bufPtrPtr, cmRpt_t* rpt );
  ///@}



  /// \name cmSample_t and cmReal_t Alias Macros
  ///@{
  /// Alias the cmSample_t and cmReal_t sample reading and writing functions to the appropriate
  /// type based on #CM_FLOAT_SMP and #CM_FLOAT_REAL.

#if CM_FLOAT_SMP == 1

#define cmAudioFileReadSample      cmAudioFileReadFloat
#define cmAudioFileReadSumSample   cmAudioFileReadSumFloat
#define cmAudioFileGetSample       cmAudioFileGetFloat
#define cmAudioFileGetSumSample    cmAudioFileGetSumFloat
#define cmAudioFileWriteSample     cmAudioFileWriteFloat
#define cmAudioFileWriteFileSample cmAudioFileWriteFileFloat

#else

#define cmAudioFileReadSample      cmAudioFileReadDouble
#define cmAudioFileReadSumSample   cmAudioFileReadSumDouble
#define cmAudioFileGetSample       cmAudioFileGetDouble
#define cmAudioFileGetSumSample    cmAudioFileGetSumDouble
#define cmAudioFileWriteSample     cmAudioFileWriteDouble
#define cmAudioFileWriteFileSample cmAudioFileWriteFileDouble

#endif

#if CM_FLOAT_REAL == 1

#define cmAudioFileReadReal      cmAudioFileReadFloat
#define cmAudioFileReadSumReal   cmAudioFileReadSumFloat
#define cmAudioFileGetReal       cmAudioFileGetFloat
#define cmAudioFileGetSumReal    cmAudioFileGetSumFloat
#define cmAudioFileWriteReal     cmAudioFileWriteFloat
#define cmAudioFileWriteFileReal cmAudioFileWriteFileFloat

#else

#define cmAudioFileReadReal      cmAudioFileReadDouble
#define cmAudioFileReadSumReal   cmAudioFileReadSumDouble
#define cmAudioFileGetReal       cmAudioFileGetDouble
#define cmAudioFileGetSumReal    cmAudioFileGetSumDouble
#define cmAudioFileWriteReal     cmAudioFileWriteDouble
#define cmAudioFileWriteFileReal cmAudioFileWriteFileDouble

#endif
  ///@}


  /// \name Minimum, Maximum, Mean
  ///@{
  /// Scan an entire audio file and return the minimum, maximum and mean sample value.
  /// On error *minPtr, *maxPtr, and *meanPtr are set to -acSample_MAX, cmSample_MAX, and 0 respectively
  cmRC_t     cmAudioFileMinMaxMean( cmAudioFileH_t h, unsigned chIdx, cmSample_t* minPtr, cmSample_t* maxPtr, cmSample_t* meanPtr );
  cmRC_t     cmAudioFileMinMaxMeanFn( const cmChar_t* fn, unsigned chIdx, cmSample_t* minPtr, cmSample_t* maxPtr, cmSample_t* meanPtr, cmRpt_t* rpt );
  ///@}

  /// Return the file name associated with a audio file handle.
  const cmChar_t* cmAudioFileName( cmAudioFileH_t h );

  /// Given an error code return the associated message.
  const char* cmAudioFileErrorMsg( unsigned rc );

  /// \name Get information about an audio file
  ///@{

  /// Return the cmAudioFileInfo_t record associated with a file.
  cmRC_t     cmAudioFileGetInfo(   const cmChar_t* fn, cmAudioFileInfo_t* infoPtr, cmRpt_t* rpt );
  
  /// Print the cmAudioFileInfo_t to a file.
  void       cmAudioFilePrintInfo( const cmAudioFileInfo_t* infoPtr, cmRpt_t* );

  /// Print the file header information and frmCnt sample values beginning at frame index frmIdx.
  cmRC_t     cmAudioFileReport(   cmAudioFileH_t h,  cmRpt_t* rpt, unsigned frmIdx, unsigned frmCnt );

    /// Print the file header information and  frmCnt sample values beginning at frame index frmIdx.
  cmRC_t     cmAudioFileReportFn( const cmChar_t* fn, unsigned frmIdx, unsigned frmCnt, cmRpt_t* rpt );
  ///@}

  /// Change the sample rate value in the header.  Note that this function does not resample the audio
  /// signal it simply changes the value of the sample rate in the header.
  cmRC_t     cmAudioFileSetSrate( const cmChar_t* audioFn, unsigned srate );

  // Generate a sine tone and write it to a file.
  cmRC_t     cmAudioFileSine( cmCtx_t* ctx, const cmChar_t* fn, double srate, unsigned bits, double hz, double gain, double secs );


  /// Testing and example routine for functions in cmAudioFile.h.
  /// Also see cmProcTest.c cmAudioFileReadWriteTest()
  void       cmAudioFileTest( cmCtx_t* ctx, int argc, const char* argv[] );

  
#ifdef __cplusplus
}
#endif

#endif
