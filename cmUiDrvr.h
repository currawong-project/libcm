#ifndef cmUiDrvr_h
#define cmUiDrvr_h

#ifdef __cplusplus
extern "C" {
#endif

  typedef unsigned cmUiRC_t;

  // cmUi result codes
  enum
  {
    kOkUiRC = cmOkRC,
    kAppNotFoundUiRC,
    kCtlNotFoundUiRC,
    kPanelNotFoundUiRC,
    kPanelFullUiRC,
    kDrvrErrUiRC,    
    kInvalidCtlOpUiRC,
    kInvalidColRowUiRC,
    kInvalidIdUiRC,
    kSubSysFailUiRC,
    kBufTooSmallUiRC,
    kBufCorruptUiRC
  };

  // Built-in control types.
  typedef enum
  {
    kInvalidUiCId,
    kPanelUiCId,
    kBtnUiCId,
    kCheckUiCId,
    kMenuBtnUiCId,
    kListUiCId,
    kLabelUiCId,
    kTextUiCId,
    kNumberUiCId,
    kSliderUiCId,
    kProgressUiCId,
    kMeterUiCId,
    kFilenameUiCId,
    kDirUiCId,
    kMaxUiCId
  } cmUiCId_t;

  // Control selector id's
  typedef enum
  {
    kInvalidDId,
    kCreateCtlDId,
    kDestroyCtlDId,
    kSetValDId,
    kDestroyAllDId,
    kMaxDId
  } cmUiDId_t;

  // Control flags.
  enum
  {
    // All controls recognize kValUiFl and kLblUiFl
    kValUiFl   = 0x0000001,
    kLblUiFl   = 0x0000002,

    // Flags for Number,Progress,Meter
    kMinUiFl   = 0x000004,
    kMaxUiFl   = 0x000010,
    kIncUiFl   = 0x000020,
    kNumMask   = kValUiFl | kMinUiFl | kMaxUiFl | kIncUiFl,
    kHorzUiFl  = 0x000040,
    kVertUiFl  = 0x000080,

    // Flags for Filename and Dir
    kFnPatUiFl = 0x000100, // file pattern string
    kFnDirUiFl = 0x000200, // toggle file btn type 
    kFnMask    = kFnPatUiFl | kFnDirUiFl,

    // Append list or menu element.
    kAppendUiFl = 0x000400,

    kLeftUiFl   = 0x001000,
    kTopUiFl    = 0x002000,
    kRightUiFl  = 0x004000,
    kBottomUiFl = 0x008000,
    kHCtrUiFl   = 0x010000,
    kVCtrUiFl   = 0x020000,
    kInsideUiFl = 0x040000,

    // Value flags indicate which value fields are valid
    kIvalUiFl   = 0x100000,
    kFvalUiFl   = 0x200000,
    kSvalUiFl   = 0x400000

  };


  // A control can be uniquely idenfied by 
  // appId and usrId (appId's are unique among app's)
  // (usrId's are unique among ctl's on an app)
  // appId's and usrId's should be zero based low numbers
  // because they are used internally as indexes.
  typedef struct
  {
    cmUiDId_t       dId;        // function selector id
    unsigned        appId;      // app id (plug-in instance id)
    unsigned        usrId;      // ctl id
    unsigned        panelId;    // parent panel id   
    cmUiCId_t       cId;        // UI control type 
    unsigned        flags;      // See kXXXUiFl above.
    int             ival;       // Valid if kIvalUiFl is set.
    double          fval;       // Valid if kFvalUiFl is set.
    const cmChar_t* sval;       // Valid if kSvalUiFl is set.
    int             x; 
    int             y; 
    int             w; 
    int             h;
  } cmUiDriverArg_t;
  
  typedef cmUiRC_t (*cmUiDriverFunc_t)( void* arg, const cmUiDriverArg_t* a );

  void cmUiDriverArgSetup( cmUiDriverArg_t* a, 
    cmUiDId_t dId,
    unsigned  appId,
    unsigned  usrId,
    unsigned  panelId,
    cmUiCId_t cId,
    unsigned  flags,
    int       ival,
    double    fval,
    const cmChar_t* sval,
    int       x,
    int       y,
    int       w,
    int       h
 );

  unsigned cmUiDriverArgSerializeBufByteCount( const cmUiDriverArg_t* a );

  // Returns kBufTooSmallUiRC if the buffer is too small otherwise returns kOkUiRC.
  // This function does not call cmErrMsg() on error
  // the caller is therefore responsible for generating errors.
  cmUiRC_t cmUiDriverArgSerialize( const cmUiDriverArg_t* a, void* buf, unsigned bufByteCnt );

  // Return kBufTooSmallUiRC or kBufCorruptUiRC if buffer corruption is detected 
  // otherwise returns kOkUiRC.  This function does not call cmErrMsg() on error
  // the caller is therefore responsible for generating errors.
  cmUiRC_t cmUiDriverArgDeserialize( cmUiDriverArg_t* a, void* buf, unsigned bufByteCnt );

  // Return an arg. value converted to the requiested type.
  // Note that numeric values will be automatically converted but
  // values will not be converted between string and numeric values.
  int             cmUiDriverArgGetInt(    const cmUiDriverArg_t* a );
  double          cmUiDriverArgGetDouble( const cmUiDriverArg_t* a );
  const cmChar_t* cmUiDriverArgGetString( const cmUiDriverArg_t* a );


#ifdef __cplusplus
}
#endif


#endif
