//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#ifndef cmSerialize_h
#define cmSerialize_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:" An API for serializing data structures into byte streams and then deserializing them back into data structures." kw:[base]}
  

  // Result codes
  enum
  {
    kOkSrRC,
    kLHeapFailSrRC,
    kParamErrSrRC,
    kFormatViolationSrRC,
  };

  // type id
  enum
  {
    kArraySrFl   = 0x80000000,
    kInvalidSrId = 0, // 0
    kCharSrId,        // 1
    kUCharSrId,       // 2
    kShortSrId,       // 3
    kUShortSrId,      // 4
    kIntSrId,         // 5
    kUIntSrId,        // 6
    kLongSrId,        // 7
    kULongSrId,       // 8
    kFloatSrId,       // 9
    kDoubleSrId,      // 10
    kBoolSrId,        // 11
    kStructSrId,      // 12 (all structTypeId's are >= kStructSrId)

    kCharVSrId   = kCharSrId   + kArraySrFl,
    kUCharVSrId  = kUCharSrId  + kArraySrFl,
    kShortVSrId  = kShortSrId  + kArraySrFl,
    kUShortVSrId = kUShortSrId + kArraySrFl,
    kIntVSrId    = kIntSrId    + kArraySrFl,
    kUIntVSrId   = kUIntSrId   + kArraySrFl,
    kLongVSrId   = kLongSrId   + kArraySrFl,
    kULongVSrId  = kULongSrId  + kArraySrFl,
    kFloatVSrId  = kFloatSrId  + kArraySrFl,
    kDoubleVSrId = kDoubleSrId + kArraySrFl,
    
  };

  typedef cmHandle_t cmSrH_t;
  typedef unsigned   cmSrRC_t;

  extern cmSrH_t cmSrNullHandle;

  cmSrRC_t cmSrAlloc( cmSrH_t* hp, cmCtx_t* ctx );
  cmSrRC_t cmSrFree(  cmSrH_t* hp );

  bool     cmSrIsValid( cmSrH_t h );
  cmSrRC_t cmSrLastErrorCode( cmSrH_t h );
  cmSrRC_t cmSrGetAndClearLastErrorCode( cmSrH_t h );

  //
  // Serializer Format Interface
  // 
  cmSrRC_t cmSrFmtReset(  cmSrH_t h );
  cmSrRC_t cmSrFmtDefineStruct( cmSrH_t h, unsigned structTypeId );

  cmSrRC_t cmSrFmtStruct(  cmSrH_t h, unsigned structTypeId );
  cmSrRC_t cmSrFmtChar(    cmSrH_t h );
  cmSrRC_t cmSrFmtUChar(   cmSrH_t h );
  cmSrRC_t cmSrFmtShort(   cmSrH_t h );
  cmSrRC_t cmSrFmtUShort(  cmSrH_t h );
  cmSrRC_t cmSrFmtLong(    cmSrH_t h );
  cmSrRC_t cmSrFmtULong(   cmSrH_t h );
  cmSrRC_t cmSrFmtInt(     cmSrH_t h );
  cmSrRC_t cmSrFmtUInt(    cmSrH_t h );
  cmSrRC_t cmSrFmtFloat(   cmSrH_t h );
  cmSrRC_t cmSrFmtDouble(  cmSrH_t h );
  cmSrRC_t cmSrFmtBool(    cmSrH_t h );

  cmSrRC_t cmSrFmtStructV( cmSrH_t h, unsigned structTypeId );
  cmSrRC_t cmSrFmtCharV(   cmSrH_t h );
  cmSrRC_t cmSrFmtUCharV(  cmSrH_t h );
  cmSrRC_t cmSrFmtShortV(  cmSrH_t h );
  cmSrRC_t cmSrFmtUShortV( cmSrH_t h );
  cmSrRC_t cmSrFmtLongV(   cmSrH_t h );
  cmSrRC_t cmSrFmtULongV(  cmSrH_t h );
  cmSrRC_t cmSrFmtIntV(    cmSrH_t h );
  cmSrRC_t cmSrFmtUIntV(   cmSrH_t h );
  cmSrRC_t cmSrFmtFloatV(  cmSrH_t h );
  cmSrRC_t cmSrFmtDoubleV( cmSrH_t h );
  cmSrRC_t cmSrFmtBool(    cmSrH_t h );

  // Combine a call to cmSrFmtDefineStruct() followed by multiple calls
  // to cmSrFmtXXX(). 
  // Notes:
  // 1. The arg. list must be terminated with a kInvalidSrId.
  // 2. Define primitive types using the appropriate id (kXXXSrId or kXXXVSrId)
  // 3. To define a structure field use the user defined structure type id.
  // 4. To define a structure array field use include the kArraySrFl flag as part of the id. (e.g. kMyStructId | kArraySrFl)
  cmSrRC_t cmSrDefFmt( cmSrH_t h, unsigned structTypeId, ... );

  cmSrRC_t cmSrFmtPrint(  cmSrH_t h );

  //
  // Serializer Input (Write) Interface
  //
  
  cmSrRC_t cmSrWrReset(     cmSrH_t h );

  cmSrRC_t cmSrWrStructBegin( cmSrH_t h, unsigned structTypeId );
  cmSrRC_t cmSrWrStructEnd(   cmSrH_t h );

  cmSrRC_t cmSrWrStruct( cmSrH_t h, unsigned structTypeId, unsigned eleCnt );
  
  cmSrRC_t cmSrWrChar(   cmSrH_t h, char val );
  cmSrRC_t cmSrWrUChar(  cmSrH_t h, unsigned char val );
  cmSrRC_t cmSrWrShort(  cmSrH_t h, short val );
  cmSrRC_t cmSrWrUShort( cmSrH_t h, unsigned short val );
  cmSrRC_t cmSrWrLong(   cmSrH_t h, long val );
  cmSrRC_t cmSrWrULong(  cmSrH_t h, unsigned long val );
  cmSrRC_t cmSrWrInt(    cmSrH_t h, int val );
  cmSrRC_t cmSrWrUInt(   cmSrH_t h, unsigned val );
  cmSrRC_t cmSrWrFloat(  cmSrH_t h, float val );
  cmSrRC_t cmSrWrDouble( cmSrH_t h, double val );
  cmSrRC_t cmSrWrBool(   cmSrH_t h, bool val );

  cmSrRC_t cmSrWrStr(     cmSrH_t h, const cmChar_t*       val );
  cmSrRC_t cmSrWrCharV(   cmSrH_t h, const char*           val, unsigned eleCnt );
  cmSrRC_t cmSrWrUCharV(  cmSrH_t h, const unsigned char*  val, unsigned eleCnt );
  cmSrRC_t cmSrWrShortV(  cmSrH_t h, const short*          val, unsigned eleCnt );
  cmSrRC_t cmSrWrUShortV( cmSrH_t h, const unsigned short* val, unsigned eleCnt );
  cmSrRC_t cmSrWrLongV(   cmSrH_t h, const long*           val, unsigned eleCnt );
  cmSrRC_t cmSrWrULongV(  cmSrH_t h, const unsigned long*  val, unsigned eleCnt );
  cmSrRC_t cmSrWrIntV(    cmSrH_t h, const int*            val, unsigned eleCnt );
  cmSrRC_t cmSrWrUIntV(   cmSrH_t h, const unsigned*       val, unsigned eleCnt );
  cmSrRC_t cmSrWrFloatV(  cmSrH_t h, const float*          val, unsigned eleCnt );
  cmSrRC_t cmSrWrDoubleV( cmSrH_t h, const double*         val, unsigned eleCnt );
  cmSrRC_t cmSrWrBoolV(   cmSrH_t h, const bool*           val, unsigned eleCnt );

  void*    cmSrWrAllocBuf( cmSrH_t h, unsigned* bufByteCntPtr );
  void*    cmSrWrGetBuf(   cmSrH_t h, unsigned* bufByteCntPtr );

  //
  // Serializer Output (Read) Interface
  //

  // cmSrRdProcessBuffer() validates the buffer and swaps its endianess if necessary.
  // This function must be called prior to cmSrRdSetup() however it should only be called
  // once on a given buffer.  Multiple calls to cmSrRdSetup() however may be made on
  // the same buffer following the call to this function.
  cmSrRC_t cmSrRdProcessBuffer( cmSrH_t h, void* buf, unsigned bufByteCnt );

  // Prepare a buffer for reading.
  cmSrRC_t cmSrRdSetup( cmSrH_t h, const void* buf, unsigned bufByteCnt );

  cmSrRC_t cmSrRdStructBegin( cmSrH_t h, unsigned structId );
  cmSrRC_t cmSrRdStructEnd( cmSrH_t h );

  cmSrRC_t cmSrReadStruct(    cmSrH_t h, unsigned structTypeId, unsigned* arrayCnt );  

  cmSrRC_t cmSrReadChar(   cmSrH_t h, char* valPtr );
  cmSrRC_t cmSrReadUChar(  cmSrH_t h, unsigned char* valPtr );
  cmSrRC_t cmSrReadShort(  cmSrH_t h, short* valPtr );
  cmSrRC_t cmSrReadUShort( cmSrH_t h, unsigned short* valPtr );
  cmSrRC_t cmSrReadLong(   cmSrH_t h, long* valPtr);
  cmSrRC_t cmSrReadULong(  cmSrH_t h, unsigned long* valPtr );
  cmSrRC_t cmSrReadInt(    cmSrH_t h, int* valPtr);
  cmSrRC_t cmSrReadUInt(   cmSrH_t h, unsigned* valPtr );
  cmSrRC_t cmSrReadFloat(  cmSrH_t h, float* valPtr );
  cmSrRC_t cmSrReadDouble( cmSrH_t h, double* valPtr );
  cmSrRC_t cmSrReadBool(   cmSrH_t h, bool* boolPtr );

  cmSrRC_t cmSrReadCharV(   cmSrH_t h, char**           valPtr, unsigned* eleCntPtr );
  cmSrRC_t cmSrReadUCharV(  cmSrH_t h, unsigned char**  valPtr, unsigned* eleCntPtr );
  cmSrRC_t cmSrReadShortV(  cmSrH_t h, short**          valPtr, unsigned* eleCntPtr );
  cmSrRC_t cmSrReadUShortV( cmSrH_t h, unsigned short** valPtr, unsigned* eleCntPtr );
  cmSrRC_t cmSrReadLongV(   cmSrH_t h, long**           valPtr, unsigned* eleCntPtr);
  cmSrRC_t cmSrReadULongV(  cmSrH_t h, unsigned long**  valPtr, unsigned* eleCntPtr );
  cmSrRC_t cmSrReadIntV(    cmSrH_t h, int**            valPtr, unsigned* eleCntPtr);
  cmSrRC_t cmSrReadUIntV(   cmSrH_t h, unsigned**       valPtr, unsigned* eleCntPtr );
  cmSrRC_t cmSrReadFloatV(  cmSrH_t h, float**          valPtr, unsigned* eleCntPtr );
  cmSrRC_t cmSrReadDoubleV( cmSrH_t h, double**         valPtr, unsigned* eleCntPtr );
  cmSrRC_t cmSrReadBoolV(   cmSrH_t h, bool**           valPtr, unsigned* eleCntPtr );
 
  cmSrRC_t cmSrReadCharCV(   cmSrH_t h, const char**           valPtr, unsigned* eleCntPtr );
  cmSrRC_t cmSrReadUCharCV(  cmSrH_t h, const unsigned char**  valPtr, unsigned* eleCntPtr );
  cmSrRC_t cmSrReadShortCV(  cmSrH_t h, const short**          valPtr, unsigned* eleCntPtr );
  cmSrRC_t cmSrReadUShortCV( cmSrH_t h, const unsigned short** valPtr, unsigned* eleCntPtr );
  cmSrRC_t cmSrReadLongCV(   cmSrH_t h, const long**           valPtr, unsigned* eleCntPtr);
  cmSrRC_t cmSrReadULongCV(  cmSrH_t h, const unsigned long**  valPtr, unsigned* eleCntPtr );
  cmSrRC_t cmSrReadIntCV(    cmSrH_t h, const int**            valPtr, unsigned* eleCntPtr);
  cmSrRC_t cmSrReadUIntCV(   cmSrH_t h, const unsigned**       valPtr, unsigned* eleCntPtr );
  cmSrRC_t cmSrReadFloatCV(  cmSrH_t h, const float**          valPtr, unsigned* eleCntPtr );
  cmSrRC_t cmSrReadDoubleCV( cmSrH_t h, const double**         valPtr, unsigned* eleCntPtr );
  cmSrRC_t cmSrReadBoolCV(   cmSrH_t h, const bool**           valPtr, unsigned* eleCntPtr );

  //
  // Abbreviated read functions. 
  //
  // (Use cmSrLastErrorCode() to check for errors.)
  //
  
  // Same as cmSrReadFieldStruct but returns array element count or cmInvalidCnt on error.
  unsigned              cmSrRdStruct( cmSrH_t h, unsigned structTypeId );

  char                  cmSrRdChar(   cmSrH_t h );
  unsigned char         cmSrRdUChar(  cmSrH_t h );
  short                 cmSrRdShort(  cmSrH_t h );
  unsigned short        cmSrRdUShort( cmSrH_t h );
  long                  cmSrRdLong(   cmSrH_t h );
  unsigned long         cmSrRdULong(  cmSrH_t h );
  int                   cmSrRdInt(    cmSrH_t h );
  unsigned int          cmSrRdUInt(   cmSrH_t h );
  float                 cmSrRdFloat(  cmSrH_t h );
  double                cmSrRdDouble( cmSrH_t h );
  bool                  cmSrRdBool(   cmSrH_t h );

  char*                 cmSrRdCharV(   cmSrH_t h, unsigned* eleCntPtr);
  unsigned char*        cmSrRdUCharV(  cmSrH_t h, unsigned* eleCntPtr);
  short*                cmSrRdShortV(  cmSrH_t h, unsigned* eleCntPtr);
  unsigned short*       cmSrRdUShortV( cmSrH_t h, unsigned* eleCntPtr);
  long*                 cmSrRdLongV(   cmSrH_t h, unsigned* eleCntPtr);
  unsigned long*        cmSrRdULongV(  cmSrH_t h, unsigned* eleCntPtr);
  int*                  cmSrRdIntV(    cmSrH_t h, unsigned* eleCntPtr);
  unsigned int*         cmSrRdUIntV(   cmSrH_t h, unsigned* eleCntPtr);
  float*                cmSrRdFloatV(  cmSrH_t h, unsigned* eleCntPtr);
  double*               cmSrRdDoubleV( cmSrH_t h, unsigned* eleCntPtr);
  bool*                 cmSrRdBoolV(   cmSrH_t h, unsigned* aleCntPtr);

  const char*           cmSrRdCharCV(   cmSrH_t h, unsigned* eleCntPtr);
  const unsigned char*  cmSrRdUCharCV(  cmSrH_t h, unsigned* eleCntPtr);
  const short*          cmSrRdShortCV(  cmSrH_t h, unsigned* eleCntPtr);
  const unsigned short* cmSrRdUShortCV( cmSrH_t h, unsigned* eleCntPtr);
  const long*           cmSrRdLongCV(   cmSrH_t h, unsigned* eleCntPtr);
  const unsigned long*  cmSrRdULongCV(  cmSrH_t h, unsigned* eleCntPtr);
  const int*            cmSrRdIntCV(    cmSrH_t h, unsigned* eleCntPtr);
  const unsigned int*   cmSrRdUIntCV(   cmSrH_t h, unsigned* eleCntPtr);
  const float*          cmSrRdFloatCV(  cmSrH_t h, unsigned* eleCntPtr);
  const double*         cmSrRdDoubleCV( cmSrH_t h, unsigned* eleCntPtr);
  const bool*           cmSrRdBoolCV(   cmSrH_t h, unsigned* aleCntPtr);

#if CM_FLOAT_SMP == 1
#define kSampleSrId  kFloatSrId
#define kSampleVSrId kFloatSrId + kArraySrFl
#define cmSrFmtSmp   cmSrFmtFloat
#define cmSrFmtSmpV  cmSrFmtFloatV
#define cmSrWrSmp    cmSrWrFloat
#define cmSrWrSmpV   cmSrWrFloatV
#define cmSrReadSmp  cmSrReadFloat
#define cmSrReadSmpV cmSrReadFloatV
#define cmSrRdSmp    cmSrRdFloat
#define cmSrRdSmpV   cmSrRdFloatV
#else
#define kSampleSrId  kDoubleSrId
#define kSampleVSrId kDoubleSrId + kArraySrFl
#define cmSrFmtSmp   cmSrFmtDouble
#define cmSrFmtSmpV  cmSrFmtDoubleV
#define cmSrWrSmp    cmSrWrDouble
#define cmSrWrSmpV   cmSrWrDoubleV
#define cmSrReadSmp  cmSrReadDouble
#define cmSrReadSmpV cmSrReadDoubleV
#define cmSrRdSmp    cmSrRdDouble
#define cmSrRdSmpV   cmSrRdDoubleV
#endif

#if CM_FLOAT_REAL == 1
#define kRealSrId     kFloatSrId
#define kRealVSrId    kFloatSrId + kArraySrFl
#define cmSrFmtReal   cmSrFmtFloat
#define cmSrFmtRealV  cmSrFmtFloatV
#define cmSrWrReal    cmSrWrFloat
#define cmSrWrRealV   cmSrWrFloatV
#define cmSrReadReal  cmSrReadFloat
#define cmSrReadRealV cmSrReadFloatV
#define cmSrRdReal    cmSrRdFloat
#define cmSrRdRealV   cmSrRdFloatV
#else
#define kRealSrId     kDoubleSrId
#define kRealVSrId    kDoubleSrId + kArraySrFl
#define cmSrFmtReal   cmSrFmtDouble
#define cmSrFmtRealV  cmSrFmtDoubleV
#define cmSrWrReal    cmSrWrDouble
#define cmSrWrRealV   cmSrWrDoubleV
#define cmSrReadReal  cmSrReadDouble
#define cmSrReadRealV cmSrReadDoubleV
#define cmSrRdReal    cmSrRdDouble
#define cmSrRdRealV   cmSrRdDoubleV
#endif
  
    
  cmSrRC_t cmSrTest( cmCtx_t* ctx );

  //)

#ifdef __cplusplus
}
#endif


#endif


