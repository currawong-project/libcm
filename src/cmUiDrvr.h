//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#ifndef cmUiDrvr_h
#define cmUiDrvr_h

#ifdef __cplusplus
extern "C" {
#endif

  //( {file_desc:"UI independent driver used by an rtSys application to communicate with the UI." kw:[rtsys]}
  
  typedef unsigned cmUiRC_t;

  // cmUi result codes
  enum
  {
    kOkUiRC = cmOkRC,
    kAppNotFoundUiRC,
    kCtlNotFoundUiRC,
    kPanelNotFoundUiRC,
    kInvalidAppIdUiRC,
    kPanelFullUiRC,
    kDrvrErrUiRC,    
    kInvalidCtlOpUiRC,
    kInvalidColRowUiRC,
    kInvalidIdUiRC,
    kSubSysFailUiRC,
    kBufTooSmallUiRC,
    kBufCorruptUiRC,
    kNotImplementedUiRC
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
    kStringUiCId,
    kConsoleUiCId,
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
    kEnableDId,       // ival holds new enable state
    kMaxDId
  } cmUiDId_t;

  // Control flags.
  enum
  {
    // All controls recognize kValUiFl and kLblUiFl
    kValUiFl       = 0x00000001,
    kLblUiFl       = 0x00000002,

    // Flags for Number,Progress,Meter
    kMinUiFl       = 0x00000004,
    kMaxUiFl       = 0x00000010,
    kIncUiFl       = 0x00000020,
    kNumMask       = kValUiFl | kMinUiFl | kMaxUiFl | kIncUiFl,

    kHorzUiFl      = 0x00000040,
    kVertUiFl      = 0x00000080,

    // Flags for Filename and Dir
    kFnPatUiFl     = 0x00000100, // file pattern string
    kFnDirUiFl     = 0x00000200, // toggle file btn type 
    kFnMask        = kFnPatUiFl | kFnDirUiFl,

    // Append list or menu element.
    kAppendUiFl    = 0x00000400,
    kPrependUiFl   = 0x00000800,
    kClearUiFl     = 0x00001000, // clear all ele' from a list or menu

    // Alignment flags
    kLeftUiFl      = 0x00002000,
    kTopUiFl       = 0x00004000,
    kRightUiFl     = 0x00008000,
    kBottomUiFl    = 0x00010000,
    kHCtrUiFl      = 0x00020000,
    kVCtrUiFl      = 0x00040000,
    kInsideUiFl    = 0x00080000,

    // Value flags indicate which value fields are valid
    kIvalUiFl      = 0x00100000,
    kFvalUiFl      = 0x00200000,
    kSvalUiFl      = 0x00400000,

    kNoReflectUiFl = 0x01000000, // do not reflect event to the client
    
    // When to send text fields
    kSendChangeFl  = 0x10000000, // whenever text changes
    kSendEnterFl   = 0x20000000, // when enter key is pressed (and changed or not-changed)
    kSendFocusFl   = 0x40000000, // when ctl loses focus
    kSendNoChangeFl= 0x80000000  // on enter even if no-change 
  };


  // A control can be uniquely idenfied by 
  // appId and usrId (appId's are unique among app's)
  // (usrId's are unique among ctl's on an app)
  // appId's and usrId's should be zero based low numbers
  // because they are used internally as indexes.
  typedef struct
  {
    cmRtSysMsgHdr_t hdr;
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
    unsigned  rtSubIdx,
    unsigned  selId,
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
  cmUiRC_t cmUiDriverArgDeserialize( cmUiDriverArg_t* a, const void* buf, unsigned bufByteCnt );

  // Return an arg. value converted to the requiested type.
  // Note that numeric values will be automatically converted but
  // values will not be converted between string and numeric values.
  int             cmUiDriverArgGetInt(    const cmUiDriverArg_t* a );
  double          cmUiDriverArgGetDouble( const cmUiDriverArg_t* a );
  const cmChar_t* cmUiDriverArgGetString( const cmUiDriverArg_t* a );

  //)

#ifdef __cplusplus
}
#endif


#endif
