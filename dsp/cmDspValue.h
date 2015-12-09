#ifndef cmDspValue_h
#define cmDspValue_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"'snap' variable value class." kw:[snap] }
  
  typedef unsigned cmDsvRC_t;
  
  enum
  {
    kOkDsvRC = cmOkRC,
    kNotMtxDsvRC,
    kRetTooSmallDsvRC,
    kUnknownTypeDsvRC,
    kJsonDeserialFailDsvRC,
    kConstViolationDsvRC
  };

  enum
  {
    kNullDsvFl     = 0x00000000,
    kBoolDsvFl     = 0x00000001,
    kCharDsvFl     = 0x00000002,
    kUCharDsvFl    = 0x00000004,
    kShortDsvFl    = 0x00000008,
    kUShortDsvFl   = 0x00000010,
    kLongDsvFl     = 0x00000020,
    kULongDsvFl    = 0x00000040,
    kIntDsvFl      = 0x00000080,
    kUIntDsvFl     = 0x00000100,
    kFloatDsvFl    = 0x00000200,
    kDoubleDsvFl   = 0x00000400,
    kSampleDsvFl   = 0x00000800,
    kRealDsvFl     = 0x00001000,
    kPtrDsvFl      = 0x00002000,
    kStrzDsvFl     = 0x00004000,
    kSymDsvFl      = 0x00008000,
    kJsonDsvFl     = 0x00010000,
    kMtxDsvFl      = 0x00020000,
    kTypeDsvMask   = 0x0003ffff,
    kProxyDsvFl    = 0x00040000,

    // Auxilliary Flags
    kInDsvFl       = 0x00100000,  // parameter which is input to the instance (represented by an input port)
    kOutDsvFl      = 0x00200000,  // output value (represented by an output port)
    kReqArgDsvFl   = 0x00400000,  // marks DSP variables which are must be given initial values in cmDspInstAllocate() va_list argument.
    kOptArgDsvFl   = 0x00800000,  // marks DSP variables which may optionally be given initial values 
    kNoArgDsvFl    = 0x01000000,  // marks DSP variable which may NOT be initialized from in cmDspInstAllocate() va_list argument.
    kAudioBufDsvFl = 0x02000000,  // marks DSP variables which represent audio input or outputs
    kUiDsvFl       = 0x04000000,  // marks DSP variables whose values should be echoed to the UI
    kDynDsvFl      = 0x08000000,  // Set if the value was  dynamically allocated 
    kConstDsvFl    = 0x10000000,  // Set if this value is const.
    kDfltSetDsvFl  = 0x20000000,  // Set if the default variable in a DSP instance variable has a valid value.
    kSendDfltDsvFl = 0x40000000   // By default default values are not transmitted - setting this flag causes the default values to be sent. 
  };

  typedef struct
  {
    unsigned rn;
    unsigned cn;
    
    union
    {
      bool*            bp;
      char*            cp;
      unsigned char*  ucp;
      short*           sp;
      unsigned short* usp;
      long*            lp;
      unsigned long*  ulp;
      int*             ip;
      unsigned*        up;
      float*           fp;
      double*          dp;
      cmSample_t*      ap;
      cmReal_t*        rp;
      cmJsonNode_t**   jp;
      cmChar_t**       zp;
      const cmChar_t** czp;
      void*            vp;
    } u;

  } cmDspMtx_t;

  typedef struct cmDspValue_str
  {
    unsigned flags;

    union
    {
      bool            b;
      char            c;
      unsigned char  uc;
      short           s;
      unsigned short us;
      long            l;
      unsigned long  ul;
      int             i;
      unsigned        u;
      float           f;
      double          d;
      cmSample_t      a;
      cmReal_t        r;
      cmJsonNode_t*   j;
      cmChar_t*       z;
      const cmChar_t* cz;
      cmDspMtx_t      m;
      struct cmDspValue_str* vp;
    } u;

  } cmDspValue_t;

  extern cmDspValue_t cmDspNullValue;

  bool cmDsvIsNull(      const cmDspValue_t* vp );
  bool cmDsvIsBool(      const cmDspValue_t* vp );
  bool cmDsvIsChar(      const cmDspValue_t* vp );
  bool cmDsvIsUChar(     const cmDspValue_t* vp );
  bool cmDsvIsShort(     const cmDspValue_t* vp );
  bool cmDsvIsUShort(    const cmDspValue_t* vp ); 
  bool cmDsvIsLong(      const cmDspValue_t* vp );
  bool cmDsvIsULong(     const cmDspValue_t* vp );
  bool cmDsvIsInt(       const cmDspValue_t* vp );
  bool cmDsvIsUInt(      const cmDspValue_t* vp ); 
  bool cmDsvIsFloat(     const cmDspValue_t* vp );
  bool cmDsvIsDouble(    const cmDspValue_t* vp );
  bool cmDsvIsSample(    const cmDspValue_t* vp );
  bool cmDsvIsReal(      const cmDspValue_t* vp );
  bool cmDsvIsStrz(      const cmDspValue_t* vp );
  bool cmDsvIsSymbol(    const cmDspValue_t* vp );
  bool cmDsvIsJson(      const cmDspValue_t* vp );
  bool cmDsvIsBoolMtx(   const cmDspValue_t* vp );
  bool cmDsvIsCharMtx(   const cmDspValue_t* vp );
  bool cmDsvIsUCharMtx(  const cmDspValue_t* vp );
  bool cmDsvIsShortMtx(  const cmDspValue_t* vp );
  bool cmDsvIsUShortMtx( const cmDspValue_t* vp ); 
  bool cmDsvIsLongMtx(   const cmDspValue_t* vp );
  bool cmDsvIsULongMtx(  const cmDspValue_t* vp );
  bool cmDsvIsIntMtx(    const cmDspValue_t* vp );
  bool cmDsvIsUIntMtx(   const cmDspValue_t* vp ); 
  bool cmDsvIsFloatMtx(  const cmDspValue_t* vp );
  bool cmDsvIsDoubleMtx( const cmDspValue_t* vp );
  bool cmDsvIsSampleMtx( const cmDspValue_t* vp );
  bool cmDsvIsRealMtx(   const cmDspValue_t* vp );
  bool cmDsvIsJsonMtx(   const cmDspValue_t* vp );
  bool cmDsvIsStrzMtx(   const cmDspValue_t* vp );

  // Set scalar types from a va_list. Returns false if 'flags' does not
  // identify a scalar type.  Note that this function cannot convert
  // scalar JSON nodes but can accept scalar Strz's. 
  bool cmDsvSetFromValist( cmDspValue_t* vp, unsigned flags, va_list vl );

  // Return true if the source type can be converted to the dest type.
  bool cmDsvCanConvertFlags( unsigned destFlags, unsigned srcFlags );

  // Return true if svp can be assigned to dvp.
  bool cmDsvCanConvert( const cmDspValue_t* dvp, const cmDspValue_t* svp );

  // WARNING: All of the setter functions clear all existing flags
  // in the target cmDspValue_t object and then set the type flags
  // associated with the RHS value.  This means that any auxilliary 
  // flags will be cleared and therefore must be reset by the calling
  // function if they are to be preserved past the call to the setter.

  // Assign a scalar value to the cmDspValue_t and setup the type information.
  void cmDsvSetNull(   cmDspValue_t* vp );
  void cmDsvSetBool(   cmDspValue_t* vp, bool v );
  void cmDsvSetChar(   cmDspValue_t* vp, char v );
  void cmDsvSetUChar(  cmDspValue_t* vp, unsigned char v );
  void cmDsvSetShort(  cmDspValue_t* vp, short v );
  void cmDsvSetUShort( cmDspValue_t* vp, unsigned short v );
  void cmDsvSetLong(   cmDspValue_t* vp, long v );
  void cmDsvSetULong(  cmDspValue_t* vp, unsigned long v );
  void cmDsvSetInt(    cmDspValue_t* vp, int v );  
  void cmDsvSetUInt(   cmDspValue_t* vp, unsigned int v );
  void cmDsvSetFloat(  cmDspValue_t* vp, float v );
  void cmDsvSetDouble( cmDspValue_t* vp, double v );
  void cmDsvSetSample( cmDspValue_t* vp, cmSample_t v );
  void cmDsvSetReal(   cmDspValue_t* vp, cmReal_t v );
  void cmDsvSetPtr(    cmDspValue_t* vp, void* v );
  void cmDsvSetSymbol( cmDspValue_t* vp, unsigned int v );
  void cmDsvSetStrz(   cmDspValue_t* vp, cmChar_t* v );  
  void cmDsvSetStrcz(  cmDspValue_t* vp, const cmChar_t* v );  
  void cmDsvSetJson(   cmDspValue_t* vp, cmJsonNode_t* v);


  // Assign a matrix to the cmDspValue_t.
  void cmDsvSetBoolMtx(   cmDspValue_t* vp, bool* v,           unsigned rn, unsigned cn );
  void cmDsvSetCharMtx(   cmDspValue_t* vp, char* v,           unsigned rn, unsigned cn );
  void cmDsvSetUCharMtx(  cmDspValue_t* vp, unsigned char* v,  unsigned rn, unsigned cn );
  void cmDsvSetShortMtx(  cmDspValue_t* vp, short* v,          unsigned rn, unsigned cn );
  void cmDsvSetUShortMtx( cmDspValue_t* vp, unsigned short* v, unsigned rn, unsigned cn );
  void cmDsvSetLongMtx(   cmDspValue_t* vp, long* v,           unsigned rn, unsigned cn );
  void cmDsvSetULongMtx(  cmDspValue_t* vp, unsigned long* v,  unsigned rn, unsigned cn );
  void cmDsvSetIntMtx(    cmDspValue_t* vp, int* v,            unsigned rn, unsigned cn );  
  void cmDsvSetUIntMtx(   cmDspValue_t* vp, unsigned int* v,   unsigned rn, unsigned cn );
  void cmDsvSetFloatMtx(  cmDspValue_t* vp, float* v,          unsigned rn, unsigned cn );
  void cmDsvSetDoubleMtx( cmDspValue_t* vp, double* v,         unsigned rn, unsigned cn );
  void cmDsvSetSampleMtx( cmDspValue_t* vp, cmSample_t* v,     unsigned rn, unsigned cn );
  void cmDsvSetRealMtx(   cmDspValue_t* vp, cmReal_t* v,       unsigned rn, unsigned cn );
  void cmDsvSetJsonMtx(   cmDspValue_t* vp, cmJsonNode_t** v,  unsigned rn, unsigned cn );
  void cmDsvSetStrzMtx(   cmDspValue_t* vp, cmChar_t** v,      unsigned rn, unsigned cn );
  void cmDsvSetStrczMtx(  cmDspValue_t* vp, const cmChar_t** v,      unsigned rn, unsigned cn );
  void cmDsvSetMtx(       cmDspValue_t* vp, unsigned flags, void* v, unsigned rn, unsigned cn );
  void cmDsvSetProxy(     cmDspValue_t* vp, cmDspValue_t* pp );
 
  // Get the value of a char cmDspValue_t.  
  // Returns 0 if the cmDspValue_t does not exactly match the return type.
  bool           cmDsvBool(   const cmDspValue_t* vp );
  char           cmDsvChar(   const cmDspValue_t* vp );
  unsigned char  cmDsvUChar(  const cmDspValue_t* vp );
  short          cmDsvShort(  const cmDspValue_t* vp );
  unsigned short cmDsvUShort( const cmDspValue_t* vp ); 
  long           cmDsvLong(   const cmDspValue_t* vp );
  unsigned long  cmDsvULong(  const cmDspValue_t* vp );
  int            cmDsvInt(    const cmDspValue_t* vp );
  unsigned int   cmDsvUInt(   const cmDspValue_t* vp ); 
  float          cmDsvFloat(  const cmDspValue_t* vp );
  double         cmDsvDouble( const cmDspValue_t* vp );
  cmSample_t     cmDsvSample( const cmDspValue_t* vp );
  cmReal_t       cmDsvReal(   const cmDspValue_t* vp );
  void*          cmDsvPtr(    const cmDspValue_t* vp );
  unsigned int   cmDsvSymbol( const cmDspValue_t* vp );
  cmChar_t*      cmDsvStrz(   const cmDspValue_t* vp );
  const cmChar_t*cmDsvStrcz(  const cmDspValue_t* vp );
  cmJsonNode_t*  cmDsvJson(   const cmDspValue_t* vp );

  const cmDspValue_t*  cmDsvValueCPtr( const cmDspValue_t* vp );
  cmDspValue_t*        cmDsvValuePtr( cmDspValue_t* vp );

  // Return a pointer to the base of a matrix. 
  // Returns NULL if the cmDspValue_t is does not exactly match the return type.
  const bool*           cmDsvBoolCMtx(   const cmDspValue_t* vp );
  const char*           cmDsvCharCMtx(   const cmDspValue_t* vp );
  const unsigned char*  cmDsvUCharCMtx(  const cmDspValue_t* vp );
  const short*          cmDsvShortCMtx(  const cmDspValue_t* vp );
  const unsigned short* cmDsvUShortCMtx( const cmDspValue_t* vp ); 
  const long*           cmDsvLongCMtx(   const cmDspValue_t* vp );
  const unsigned long*  cmDsvULongCMtx(  const cmDspValue_t* vp );
  const int*            cmDsvIntCMtx(    const cmDspValue_t* vp );
  const unsigned int*   cmDsvUIntCMtx(   const cmDspValue_t* vp ); 
  const float*          cmDsvFloatCMtx(  const cmDspValue_t* vp );
  const double*         cmDsvDoubleCMtx( const cmDspValue_t* vp );
  const cmSample_t*     cmDsvSampleCMtx( const cmDspValue_t* vp );
  const cmReal_t*       cmDsvRealCMtx(   const cmDspValue_t* vp );
  const cmChar_t**      cmDsvStrzCMtx(   const cmDspValue_t* vp );
  const cmChar_t**      cmDsvStrczCMtx(  const cmDspValue_t* vp );


  bool*           cmDsvBoolMtx(   cmDspValue_t* vp );
  char*           cmDsvCharMtx(   cmDspValue_t* vp );
  unsigned char*  cmDsvUCharMtx(  cmDspValue_t* vp );
  short*          cmDsvShortMtx(  cmDspValue_t* vp );
  unsigned short* cmDsvUShortMtx( cmDspValue_t* vp ); 
  long*           cmDsvLongMtx(   cmDspValue_t* vp );
  unsigned long*  cmDsvULongMtx(  cmDspValue_t* vp );
  int*            cmDsvIntMtx(    cmDspValue_t* vp );
  unsigned int*   cmDsvUIntMtx(   cmDspValue_t* vp ); 
  float*          cmDsvFloatMtx(  cmDspValue_t* vp );
  double*         cmDsvDoubleMtx( cmDspValue_t* vp );
  cmSample_t*     cmDsvSampleMtx( cmDspValue_t* vp );
  cmReal_t*       cmDsvRealMtx(   cmDspValue_t* vp );
  cmJsonNode_t**  cmDsvJsonMtx(   cmDspValue_t* vp );
  cmChar_t**      cmDsvStrzMtx(   cmDspValue_t* vp );
  const cmChar_t**cmDsvStrczMtx(  cmDspValue_t* vp );

  // Get the value of a cmDspValue_t.
  // Conversion is performed if the return type does not exactly match the cmDspValue_t type.
  bool           cmDsvGetBool(   const cmDspValue_t* vp );
  char           cmDsvGetChar(   const cmDspValue_t* vp );
  unsigned char  cmDsvGetUChar(  const cmDspValue_t* vp );
  short          cmDsvGetShort(  const cmDspValue_t* vp );
  unsigned short cmDsvGetUShort( const cmDspValue_t* vp ); 
  long           cmDsvGetLong(   const cmDspValue_t* vp );
  unsigned long  cmDsvGetULong(  const cmDspValue_t* vp );
  int            cmDsvGetInt(    const cmDspValue_t* vp );
  unsigned int   cmDsvGetUInt(   const cmDspValue_t* vp ); 
  float          cmDsvGetFloat(  const cmDspValue_t* vp );
  double         cmDsvGetDouble( const cmDspValue_t* vp );
  cmSample_t     cmDsvGetSample( const cmDspValue_t* vp );
  cmReal_t       cmDsvGetReal(   const cmDspValue_t* vp );
  void*          cmDsvGetPtr(    const cmDspValue_t* vp );
  unsigned       cmDsvGetSymbol( const cmDspValue_t* vp );
  cmChar_t*      cmDsvGetStrz(   const cmDspValue_t* vp );
  const cmChar_t*cmDsvGetStrcz(  const cmDspValue_t* vp );
  cmJsonNode_t*  cmDsvGetJson(   const cmDspValue_t* vp );

  cmDsvRC_t       cmDsvGetBoolMtx(   cmDspValue_t* vp, bool* v,           unsigned vn );  
  cmDsvRC_t       cmDsvGetCharMtx(   cmDspValue_t* vp, char* v,           unsigned vn );
  cmDsvRC_t       cmDsvGetUCharMtx(  cmDspValue_t* vp, unsigned char* v,  unsigned vn );
  cmDsvRC_t       cmDsvGetShortMtx(  cmDspValue_t* vp, short* v,          unsigned vn );
  cmDsvRC_t       cmDsvGetUShortMtx( cmDspValue_t* vp, unsigned short* v, unsigned vn ); 
  cmDsvRC_t       cmDsvGetLongMtx(   cmDspValue_t* vp, long* v,           unsigned vn );
  cmDsvRC_t       cmDsvGetULongMtx(  cmDspValue_t* vp, unsigned long* v,  unsigned vn );
  cmDsvRC_t       cmDsvGetIntMtx(    cmDspValue_t* vp, int* v,            unsigned vn );
  cmDsvRC_t       cmDsvGetUIntMtx(   cmDspValue_t* vp, unsigned int* v,   unsigned vn ); 
  cmDsvRC_t       cmDsvGetFloatMtx(  cmDspValue_t* vp, float* v,          unsigned vn );
  cmDsvRC_t       cmDsvGetDoubleMtx( cmDspValue_t* vp, double* v,         unsigned vn );
  cmDsvRC_t       cmDsvGetSampleMtx( cmDspValue_t* vp, cmSample_t* v,     unsigned vn );
  cmDsvRC_t       cmDsvGetRealMtx(   cmDspValue_t* vp, cmReal_t* v,       unsigned vn ); 
  cmDsvRC_t       cmDsvGetStrzMtx(   cmDspValue_t* vp, cmChar_t* v,       unsigned vn );

  bool     cmDsvIsType(   const cmDspValue_t* vp, unsigned flags );
  bool     cmDsvIsMtx(    const cmDspValue_t* vp );
  unsigned cmDsvEleCount( const cmDspValue_t* vp );
  unsigned cmDsvRows(     const cmDspValue_t* vp );
  unsigned cmDsvCols(     const cmDspValue_t* vp );

  // Basic type flag with kMtxDsvFl and any other flags cleared.
  unsigned cmDsvBasicType( const cmDspValue_t* vp );

  // Size of each data element in bytes.
  unsigned cmDsvEleByteCount( const cmDspValue_t* vp );

  // Count of data bytes given a matrix size. Note that 'rn' and 'cn' are
  // not used if the kMtxDsvFl is not set in flags.
  unsigned cmDsvByteCount(    unsigned flags, unsigned rn, unsigned cn );

  // Return the number of bytes required to serialize the value.
  unsigned  cmDsvSerialByteCount( const cmDspValue_t* vp );

  // Returns the number of bytes required to serialize the data alone not including
  // the cmDspValue_t record. This is just cmDsvSerialByteCount(vp) - sizeof(cmDspValue_t)
  unsigned  cmDsvSerialDataByteCount( const cmDspValue_t* vp ); 

  cmDsvRC_t cmDsvSerialize(       const cmDspValue_t* vp, void* buf, unsigned bufByteCnt );
  cmDsvRC_t cmDsvDeserializeInPlace(  cmDspValue_t* vp, unsigned dataBufByteCnt );
  cmDsvRC_t cmDsvDeserializeJson(     cmDspValue_t* vp, cmJsonH_t jsH );

  void     cmDsvPrint( const cmDspValue_t* vp, const cmChar_t* label,  cmRpt_t* rpt );

  //)
  
#define cmDsvCopy( d, s ) (*(d)) = (*(s))

#ifdef __cplusplus
}
#endif


#endif
