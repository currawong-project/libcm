#ifndef cmFrameFile_h
#define cmFrameFile_h

/*
  file  -> cmFfFile_t frame*
  frame -> cmFfFrame_t mtx*
  mtx   -> cmFfMtx_t data*   
 */

#ifdef __cplusplus
extern "C" {
#endif


  enum
  {
    kInvalidFrameTId  = 0,
    kInvalidStreamId  = 0,

    kTocFrameTId = -3,
    kTocStreamId = -3,

    kFileFfTId = 'FrmF',
    
  };

  enum
  {
    kOkFfRC = 0,          // 0
    kFileOpenFailFfRC,    // 1
    kFileReadFailFfRC,    // 2 
    kFileWriteFailFfRC,   // 3
    kFileSeekFailFfRC,    // 4
    kFileCloseFailFfRC,   // 5
    kEofFfRC,             // 6
    kInvalidHandleFfRC,   // 7
    kMemAllocErrFfRC,     // 8
    kNotFrameFileFfRC,    // 9
    kUnknownErrFfRC,      // 10
    kNoMatchingFrameFfRC, // 11
    kInvalidFileModeFfRC, // 12
    kJsonFailFfRC,        // 13
    kInvalidFrameIdxFfRC, // 14
    kDuplicateMtxIdFfRC,  // 15 
    kFileTellFailFfRC,    // 16
    kLHeapFailFfRC,       // 17
    kTocFrameRdFailFfRC,  // 18
    kTocRecdNotFoundFfRC, // 19
    kBufTooSmallFfRC      // 20
  };

  // row data formats
  enum
  {
    kInvalidFmtId, // 0
    kUCharFmtId,   // 1
    kCharFmtId,    // 2
    kUShortFmtId,  // 3 
    kShortFmtId,   // 4
    kULongFmtId,   // 5
    kLongFmtId,    // 6
    kUIntFmtId,    // 7
    kIntFmtId,     // 8
    kLLongFmtId,   // 9
    kULLongFmtId,  // 10
    kOff_tFmtId,   // 11
    kFloatFmtId,   // 12
    kDoubleFmtId,  // 13 
    kStringZFmtId, // 14 
    kBlobFmtId,    // 15
    kJsonFmtId     // 16
  };

  enum
  {
    kInvalidUId ,  // 0
    kNoUnitsUId,   // 1
    kHzUId,        // 2 
    kRadsUId,      // 3   -pi to pi 
    kAmplUId,      // 4   -1.0 to 1.0
    kPowUId,       // 5   amp^2/2
    k10DbUId,      // 6   10*log10(v)
    k20DbUId,      // 7   20*log10(v)  
    kCntUId,       // 8   count of elements
    kIdxUId,       // 9   element index 
    kBfccUId,      // 10
    kMfccUId,      // 11
    kCepsUId,       // 12

    kD1UFl  = 0x80000000 // this is a 1st difference 
  };

  enum
  {
    kTocMId = -2,


    kInvalidMId = 0,// 0
    kAudioMId,      // 1    
    kMagMId,        // 2
    kPhsMId,        // 3
    kFrqMId,        // 4  measured bin frequecies in Hz
    kTrkMId,        // 5
    kRmsMId,        // 6
    kBinCntMId,     // 7  count of frequency domain bins in frame
    kWndSmpCntMId,  // 8  actual count of samples in FFT window (this is no (binCnt-1)*2)
    kAudSmpCntMId,  // 9  count of audio samples in frame
    kBfccMId,       // 10 vector of BFCC's
    kBfccBandCntMId,// 11 count of coeff's BFCC vector in this frame
    kMfccMId,       // 12 vector of MFCC's 
    kCepsMId,       // 13 vector of cepstral coefficients
    kConstqMId,     // 14 vector of constant
    kDataMId        // 15 blob of misc data

  };

  typedef cmHandle_t cmFrameFileH_t;
  typedef unsigned   cmFfRC_t;

  // mtx desc record
  typedef struct
  {
    unsigned type;
    unsigned fmtId;
    unsigned unitsId;
    unsigned rowCnt;
    unsigned colCnt;
  } cmFfMtx_t;

  // frame desc record
  typedef struct
  {
    unsigned type;
    unsigned mtxCnt;
    unsigned flags;     // used internally to track time format (seconds or samples)
    unsigned streamId;

    union
    {
      unsigned sampleIdx;
      double   seconds;
    } time;
    
  } cmFfFrame_t;


  // file desc record
  typedef struct
  {
    cmChar_t*   filenameStr;
    unsigned    version;
    unsigned    frameCnt;      // count of frames in all streams
    double      srate;         // sample rate for all frames
  } cmFfFile_t;

  extern cmFrameFileH_t cmFrameFileNullHandle;

  cmFfRC_t           cmFrameFileCreate(  cmFrameFileH_t* hPtr, const char* fn, double srate, cmCtx_t* ctx );

  // The fileDescPtrPtr is optional. Set to NULL to ignore.
  cmFfRC_t           cmFrameFileOpen(    cmFrameFileH_t* hPtr, const char* fn, cmCtx_t* ctx, const cmFfFile_t** fileDescPtrPtr );
  cmFfRC_t           cmFrameFileClose(   cmFrameFileH_t* hPtr );
  bool               cmFrameFileIsValid( cmFrameFileH_t h );
  const cmFfFile_t*  cmFrameFileDesc(    cmFrameFileH_t h );

  // Return the count of frames in the requested stream.
  unsigned           cmFrameFileFrameCount( cmFrameFileH_t h, unsigned streamId );

  // Create a frame in a file created via cmFrameFileCreate().
  // Set 'sampleIdx' to -1 if seconds is being used instead of samples. 
  cmFfRC_t cmFrameFileFrameCreate( cmFrameFileH_t h, unsigned frameType, unsigned streamId, unsigned sampleIdx, double secs  );

  // (W) Complete and write a frame created via an earilier call
  // to cmFrameFileFrameCreate()
  cmFfRC_t cmFrameFileFrameClose(  cmFrameFileH_t h );

  // (W) Fill a frame with matrix data. The frame must have been created
  // via an earlier call to cmFrameFileCreate().
  cmFfRC_t cmFrameFileWriteMtxUChar(   cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, const unsigned char*            p, unsigned rn, unsigned cn );
  cmFfRC_t cmFrameFileWriteMtxChar(    cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, const char*                     p, unsigned rn, unsigned cn );
  cmFfRC_t cmFrameFileWriteMtxUShort(  cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, const unsigned short*           p, unsigned rn, unsigned cn );
  cmFfRC_t cmFrameFileWriteMtxShort(   cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, const short*                    p, unsigned rn, unsigned cn );
  cmFfRC_t cmFrameFileWriteMtxULong(   cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, const unsigned long*            p, unsigned rn, unsigned cn );
  cmFfRC_t cmFrameFileWriteMtxLong(    cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, const long*                     p, unsigned rn, unsigned cn );
  cmFfRC_t cmFrameFileWriteMtxUInt(    cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, const unsigned*                 p, unsigned rn, unsigned cn );
  cmFfRC_t cmFrameFileWriteMtxInt(     cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, const int*                      p, unsigned rn, unsigned cn );
  cmFfRC_t cmFrameFileWriteMtxULLong(  cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, const unsigned long long*       p, unsigned rn, unsigned cn );
  cmFfRC_t cmFrameFileWriteMtxLLong(   cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, const long long*                p, unsigned rn, unsigned cn );
  cmFfRC_t cmFrameFileWriteMtxOff_t(   cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, const off_t*                    p, unsigned rn, unsigned cn );
  cmFfRC_t cmFrameFileWriteMtxFloat(   cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, const float*                    p, unsigned rn, unsigned cn );
  cmFfRC_t cmFrameFileWriteMtxDouble(  cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, const double*                   p, unsigned rn, unsigned cn );
  cmFfRC_t cmFrameFileWriteMtxBlob(    cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, const void*                     p, unsigned rn, unsigned cn );  
  cmFfRC_t cmFrameFileWriteMtxStringZ( cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, const char* p );
  cmFfRC_t cmFrameFileWriteMtxJson(    cmFrameFileH_t h, unsigned mtxType, cmJsonH_t jsH, const cmJsonNode_t* nodePtr );

  // (R/W) Rewind to the first frame. Must call cmFrameFileFrameNext() 
  // following successful execution of this function to maintain correct
  // alignment.
  cmFfRC_t          cmFrameFileRewind( cmFrameFileH_t h );

  // (R/W) Seek to the frame at index 'frameIdx'.  Must call cmFrameFileFrameNext() 
  // following successful execution of this function to maintain correct
  // alignment.
  cmFfRC_t          cmFrameFileSeek( cmFrameFileH_t h, unsigned streamId, unsigned frameIdx );

  // (R/W) Seek to the next frame in stream frameStreamId with type frameTypeId.
  // Set frameTypeId to kInvalidFrameTId to return any frame type.
  // Set frameStreamId to kInvalidStreamId to return any stream type.
  // This function must be followed by a call to cmFrameFileFrameLoad()
  // or cmFrameFileSkip().
  cmFfRC_t           cmFrameFileFrameNext(     cmFrameFileH_t h, unsigned frameTypeId, unsigned streamId );

  // (R/W) Load the matrix data associated with the current frame. 
  // This function can only be called after a successful call to 
  // cmFrameFileFrameNext(). 
  // The frameDescPtrPtr is optional. Set to NULL to ignore.
  cmFfRC_t           cmFrameFileFrameLoad(     cmFrameFileH_t h, const cmFfFrame_t** frameDescPtrPtr );

  // (R/W) Skip over the matrix data associated with the current frame. 
  // This is an alternative to cmFrameFileLoad().
  cmFfRC_t           cmFrameFileFrameSkip(     cmFrameFileH_t h ); 

  // (R/W) Combines cmFrameFileFrameNext() and cmFrameFileFrameLoad()
  // into a single call.
  cmFfRC_t           cmFrameFileFrameLoadNext( cmFrameFileH_t h, unsigned frameTypeId, unsigned streamId, const cmFfFrame_t** frameDescPtrPtr );

  // (R/W) Write the current frame back to disk. 
  cmFfRC_t           cmFrameFileFrameUpdate( cmFrameFileH_t h );

  // Return the current frame description record.
  const cmFfFrame_t* cmFrameFileFrameDesc( cmFrameFileH_t h );

  // Currently loaded frame index.
  unsigned           cmFrameFileFrameLoadedIndex( cmFrameFileH_t h );

  // Return the index of the frame with type 'mtxTypeId', units 'unitsId', and format 'fmtId'
  // or cmInvalidIdx if the specified frame is not found.
  // Set mtxTypeId to kInvalidMId to return any mtx type.
  // Set unitsId to kInvalidUId to return a mtx with any units.
  // Set fmtId to kInvalidFmtId to return a mtx with any fmt.
  unsigned           cmFrameFileMtxIndex(      cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId, unsigned fmtId );

  // Return a matrix description record.
  const cmFfMtx_t*   cmFrameFileMtxDesc(       cmFrameFileH_t h, unsigned mtxIdx );

  // Access matrix data.
  //Set descPtr to NULL if the matrix desc is not needed.
  unsigned char*      cmFrameFileMtxIndexUChar(   cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr );
  char*               cmFrameFileMtxIndexChar(    cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr );
  unsigned short*     cmFrameFileMtxIndexUShort(  cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr );
  short*              cmFrameFileMtxIndexShort(   cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr );
  unsigned long*      cmFrameFileMtxIndexULong(   cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr );
  long*               cmFrameFileMtxIndexLong(    cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr );
  unsigned*           cmFrameFileMtxIndexUInt(    cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr );
  int*                cmFrameFileMtxIndexInt(     cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr );
  unsigned long long* cmFrameFileMtxIndexULLong(  cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr );
  long long*          cmFrameFileMtxIndexLLong(   cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr );
  off_t*              cmFrameFileMtxIndexOff_t(   cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr );
  float*              cmFrameFileMtxIndexFloat(   cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr );
  double*             cmFrameFileMtxIndexDouble(  cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr );
  char*               cmFrameFileMtxIndexStringZ( cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr );
  void*               cmFrameFileMtxIndexBlob(    cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr );
  // (The caller is responsible for invoking cmJsonFinalize() to finalize the returned json handle.)
  cmJsonH_t           cmFrameFileMtxIndexJson(    cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr );

  // Return a pointer to the data, and optionally the descPtr, for a matrix with the given 
  // type,units and format in the current frame.
  // The following functions are implmented in terms of
  // cmFrameFileMtxIndexXXX() and cmFrameFileMtxIndex().
  unsigned char*      cmFrameFileMtxUChar(   cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId,  const cmFfMtx_t** descPtrPtr );
  char*               cmFrameFileMtxChar(    cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId,  const cmFfMtx_t** descPtrPtr );
  unsigned short*     cmFrameFileMtxUShort(  cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId,  const cmFfMtx_t** descPtrPtr );
  short*              cmFrameFileMtxShort(   cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId,  const cmFfMtx_t** descPtrPtr );
  unsigned long*      cmFrameFileMtxULong(   cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId,  const cmFfMtx_t** descPtrPtr );
  long*               cmFrameFileMtxLong(    cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId,  const cmFfMtx_t** descPtrPtr );
  unsigned*           cmFrameFileMtxUInt(    cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId,  const cmFfMtx_t** descPtrPtr );
  int*                cmFrameFileMtxInt(     cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId,  const cmFfMtx_t** descPtrPtr );
  unsigned long long* cmFrameFileMtxULLong(  cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId,  const cmFfMtx_t** descPtrPtr );
  long long*          cmFrameFileMtxLLong(   cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId,  const cmFfMtx_t** descPtrPtr );
  off_t*              cmFrameFileMtxOff_t(   cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId,  const cmFfMtx_t** descPtrPtr );
  float*              cmFrameFileMtxFloat(   cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId,  const cmFfMtx_t** descPtrPtr );
  double*             cmFrameFileMtxDouble(  cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId,  const cmFfMtx_t** descPtrPtr );
  char*               cmFrameFileMtxStringZ( cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId,  const cmFfMtx_t** descPtrPtr );
  void*               cmFrameFileMtxBlob(    cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId,  const cmFfMtx_t** descPtrPtr );
  // (The caller is responsible for invoking cmJsonFinalize() to finalize the returned json handle.)
  cmJsonH_t           cmFrameFileMtxJson(    cmFrameFileH_t h, unsigned mtxTypeId,                                   const cmFfMtx_t** descPtrPtr );


  // Return the max row cnt, max col count, and total element count
  // for all matrices which match the given stream/mtx/units/fmt
  // combination.  Note that if the returned ele count is less than
  // maxRowCnt * maxColCnt * cmFrameFileFrameCount(streamId) then
  // some matched matrices contain fewer than maxRowCnt/maxColCnt
  // rows/columns.
  cmFfRC_t cmFrameFileMtxSize( cmFrameFileH_t h, unsigned streamId, unsigned mtxType, unsigned unitsId, unsigned fmtId, unsigned* frameCntPtr, unsigned* rowCntPtr, unsigned* colCntPtr, unsigned* eleCntPtr );

  // Load a buffer with all of the data which matches a given 
  // stream/mtx/unit/fmt combination in the given range of frames.
  // 'frmIdx' specifies the frame index relative to the given stream id as opposed to 
  // and absolute frame index.
  // Set frmCnt to -1 to include all frames following 'frmIdx'.
  // *outCntPtr is set to the actual number of elements copied into the buffer
  // The data is packed into the return buffer by copying columwise from the source.
  // matrices to the buf[].  If all of the matrices are not of a fixed known size
  // it may therefore be difficult to distinguish where one frames data ends and
  // the next begins.
  cmFfRC_t cmFrameFileMtxLoadUChar(   cmFrameFileH_t h, unsigned streamId, unsigned mtxTypeId, unsigned unitsId, unsigned frmIdx, unsigned frmCnt,  unsigned char*      buf, unsigned eleCnt, unsigned* outCntPtr );
  cmFfRC_t cmFrameFileMtxLoadChar(    cmFrameFileH_t h, unsigned streamId, unsigned mtxTypeId, unsigned unitsId, unsigned frmIdx, unsigned frmCnt,  char*               buf, unsigned eleCnt, unsigned* outCntPtr );
  cmFfRC_t cmFrameFileMtxLoadUShort(  cmFrameFileH_t h, unsigned streamId, unsigned mtxTypeId, unsigned unitsId, unsigned frmIdx, unsigned frmCnt,  unsigned short*     buf, unsigned eleCnt, unsigned* outCntPtr );
  cmFfRC_t cmFrameFileMtxLoadShort(   cmFrameFileH_t h, unsigned streamId, unsigned mtxTypeId, unsigned unitsId, unsigned frmIdx, unsigned frmCnt,  short*              buf, unsigned eleCnt, unsigned* outCntPtr );
  cmFfRC_t cmFrameFileMtxLoadULong(   cmFrameFileH_t h, unsigned streamId, unsigned mtxTypeId, unsigned unitsId, unsigned frmIdx, unsigned frmCnt,  unsigned long*      buf, unsigned eleCnt, unsigned* outCntPtr );
  cmFfRC_t cmFrameFileMtxLoadLong(    cmFrameFileH_t h, unsigned streamId, unsigned mtxTypeId, unsigned unitsId, unsigned frmIdx, unsigned frmCnt,  long*               buf, unsigned eleCnt, unsigned* outCntPtr );
  cmFfRC_t cmFrameFileMtxLoadUInt(    cmFrameFileH_t h, unsigned streamId, unsigned mtxTypeId, unsigned unitsId, unsigned frmIdx, unsigned frmCnt,  unsigned int*       buf, unsigned eleCnt, unsigned* outCntPtr );
  cmFfRC_t cmFrameFileMtxLoadInt(     cmFrameFileH_t h, unsigned streamId, unsigned mtxTypeId, unsigned unitsId, unsigned frmIdx, unsigned frmCnt,  int*                buf, unsigned eleCnt, unsigned* outCntPtr );
  cmFfRC_t cmFrameFileMtxLoadULLong(  cmFrameFileH_t h, unsigned streamId, unsigned mtxTypeId, unsigned unitsId, unsigned frmIdx, unsigned frmCnt,  unsigned long long* buf, unsigned eleCnt, unsigned* outCntPtr );
  cmFfRC_t cmFrameFileMtxLoadLLong(   cmFrameFileH_t h, unsigned streamId, unsigned mtxTypeId, unsigned unitsId, unsigned frmIdx, unsigned frmCnt,  long long*          buf, unsigned eleCnt, unsigned* outCntPtr );
  cmFfRC_t cmFrameFileMtxLoadOff_t(   cmFrameFileH_t h, unsigned streamId, unsigned mtxTypeId, unsigned unitsId, unsigned frmIdx, unsigned frmCnt,  off_t*              buf, unsigned eleCnt, unsigned* outCntPtr );
  cmFfRC_t cmFrameFileMtxLoadFloat(   cmFrameFileH_t h, unsigned streamId, unsigned mtxTypeId, unsigned unitsId, unsigned frmIdx, unsigned frmCnt,  float*              buf, unsigned eleCnt, unsigned* outCntPtr );
  cmFfRC_t cmFrameFileMtxLoadDouble(  cmFrameFileH_t h, unsigned streamId, unsigned mtxTypeId, unsigned unitsId, unsigned frmIdx, unsigned frmCnt,  double*             buf, unsigned eleCnt, unsigned* outCntPtr );
  cmFfRC_t cmFrameFileMtxLoadStringZ( cmFrameFileH_t h, unsigned streamId, unsigned mtxTypeId, unsigned unitsId, unsigned frmIdx, unsigned frmCnt,  char*               buf, unsigned eleCnt, unsigned* outCntPtr );
  cmFfRC_t cmFrameFileMtxLoadBlob(    cmFrameFileH_t h, unsigned streamId, unsigned mtxTypeId, unsigned unitsId, unsigned frmIdx, unsigned frmCnt,  void*               buf, unsigned eleCnt, unsigned* outCntPtr );

  cmFfRC_t cmFrameFileReport( cmFrameFileH_t h, bool summOnlyFl, cmRpt_t* rpt );
  cmFfRC_t cmFrameFileNameReport( const char* fn, bool summOnlyFl, cmCtx_t* ctx );

  cmFfRC_t cmFrameFileTest( const char* fn, cmCtx_t* ctx );

#if CM_FLOAT_SMP == 1
#define cmFrameFileMtxLoadSample  cmFrameFileMtxLoadFloat
#define cmFrameFileWriteMtxSample cmFrameFileWriteMtxFloat
#define cmFrameFileMtxIndexSample cmFrameFileMtxIndexFloat
#define cmFrameFileMtxSample      cmFrameFileMtxFloat
#define kSampleFmtId              kFloatFmtId
#else
#define cmFrameFileMtxLoadSample  cmFrameFileMtxLoadDouble
#define cmFrameFileWriteMtxSample cmFrameFileWriteMtxDouble
#define cmFrameFileMtxIndexSample cmFrameFileMtxIndexDouble
#define cmFrameFileMtxSample      cmFrameFileMtxDouble
#define kSampleFmtId              kDoubleFmtId
#endif

#if CM_FLOAT_REAL == 1
#define cmFrameFileMtxLoadReal  cmFrameFileMtxLoadFloat
#define cmFrameFileWriteMtxReal cmFrameFileWriteMtxFloat
#define cmFrameFileMtxIndexReal cmFrameFileMtxIndexFloat
#define cmFrameFileMtxReal      cmFrameFileMtxFloat
#define kRealFmtId              kFloatFmtId
#else
#define cmFrameFileMtxLoadReal  cmFrameFileMtxLoadDouble
#define cmFrameFileWriteMtxReal cmFrameFileWriteMtxDouble
#define cmFrameFileMtxIndexReal cmFrameFileMtxIndexDouble
#define cmFrameFileMtxReal      cmFrameFileMtxDouble
#define kRealFmtId              kDoubleFmtId
#endif

#ifdef __cplusplus
}
#endif

#endif
