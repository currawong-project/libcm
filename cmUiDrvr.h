#ifndef cmUiDrvr_h
#define cmUiDrvr_h

#ifdef __cplusplus
extern "C" {
#endif

  typedef unsigned cmUiRC_t;

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
  };


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
  } cmUiCId_t;

  typedef enum
  {
    kInvalidDId,
    kCreateCtlDId,
    kDestroyCtlDId,
    kSetValDId,
    kDestroyAllDId
  } cmUiDId_t;

  enum
  {
    // All controls recognize kValUiFl and kLblUiFl
    kValUiFl   = 0x000001,
    kLblUiFl   = 0x000002,

    // Flags for Number,Progress,Meter
    kMinUiFl   = 0x00004,
    kMaxUiFl   = 0x00010,
    kIncUiFl   = 0x00020,
    kNumMask   = kValUiFl | kMinUiFl | kMaxUiFl | kIncUiFl,
    kHorzUiFl  = 0x00040,
    kVertUiFl  = 0x00080,

    // Flags for Filename and Dir
    kFnPatUiFl = 0x00100, // file pattern string
    kFnDirUiFl = 0x00200, // toggle file btn type 
    kFnMask    = kFnPatUiFl | kFnDirUiFl,

    // Append list or menu element.
    kAppendUiFl = 0x00400,

    kLeftUiFl   = 0x01000,
    kTopUiFl    = 0x02000,
    kRightUiFl  = 0x04000,
    kBottomUiFl = 0x08000,
    kHCtrUiFl   = 0x10000,
    kVCtrUiFl   = 0x20000,
    kInsideUiFl = 0x40000,

  };


  // A control can be uniquely idenfied by 
  // appId and usrId (appId's are unique among app's)
  // (usrId's are unique among ctl's on an app)
  // appId's and usrId's should be zero based low numbers
  // because they are used internally as indexes.
  typedef struct
  {
    void*       cbArg;        //
    cmUiDId_t   dId;          // function selector id
    unsigned    appId;        // app id (plug-in instance id)
    unsigned    usrId;        // ctl id
    unsigned    panelId;      // parent panel id   
    cmUiCId_t   cId;          // UI control type 
    unsigned    flags;        // See kXXXUiFl above.
    int         x; 
    int         y; 
    int         w; 
    int         h;
    int         ival;
    double      fval;
    const char* sval;
  } cmUiDriverArg_t;
  
  typedef cmUiRC_t (*cmUiDriverFunc_t)( const cmUiDriverArg_t* a );

#ifdef __cplusplus
}
#endif


#endif
