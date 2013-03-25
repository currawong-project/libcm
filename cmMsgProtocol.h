#ifndef cmMsgProtocol_h
#define cmMsgProtocol_h

#ifdef __cplusplus
extern "C" {
#endif

  #define cmAudDspSys_FILENAME "aud_dsp.js"


  /// Reserved DSP message selector id's (second field of all host<->audio system messages)
  enum
  {
    kMidiMsgArraySelAsId = 1000,
    kMidiSysExSelAsId,
    kUiSelAsId,      // indicates a cmDspUiHdr_t msg
    kUiMstrSelAsId,  // indicates a cmDspUiHdr_t msg containing master control information for the audio system
    kSsInitSelAsId,  // indicates the msg is of type cmAudioSysSsInitMsg_t
    kStatusSelAsId,  // indicates the msg is of type cmAudioSysStatus_t
    kNetSyncSelAsId,   // sent with a cmDspNetMsg_t object  
  };



  // UI seletor id's used in the cmDspUiHdr_t selId field
  enum 
  {
    kPrintDuiId,   // ui<--eng print the value to the console
    kSliderDuiId,  // ui<--eng create a slider control
    kButtonDuiId,  // ui<--eng create button control
    kCheckDuiId,   // ui<--eng create a check box control
    kLabelDuiId,   // ui<--end create a label control
    kTimeLineDuiId,// ui<--end create a time-line control
    kScoreDuiId,   // ui<--end create a score control
    kNumberDuiId,  // ui<--eng create a number box
    kTextDuiId,    // ui<--eng create a text entry control
    kFnameDuiId,   // ui<--eng create a file/directory picker control
    kMsgListDuiId, // ui<--eng create a msg list control
    kMeterDuiId,   // ui<--eng create a meter display
    kValueDuiId,   // ui<->eng a control changed values  
    kColumnDuiId,  // ui<--eng start a new column
    kHBorderDuiId, // ui<--eng insert a vertical border 
    kPageDuiId,    // ui<--eng insert a new control page
    
    kAudioSysCfgDuiId,  // ui<--audio system cfg label      
    kSubSysCntDuiId,    // ui<--eng audio sub-system count
    kDeviceDuiId,       // ui<--eng device label
    kProgramDuiId,      // ui<--eng program label

    // The following selId's are used by cmAudDsp to indicate various commands.
    kSetAudioCfgDuiId,     // 1) select an audio system setup
    kSetAudioDevDuiId,     // 2) (optional) set an audio device on an audio sub-system
    kSetSampleRateDuiId,   // 3) (optional) set the sample rate of an audio sub-system
    kSetPgmDuiId,          // 4) select a program
    kEnableDuiId,          // 5) enable/disable the audio system (also resets the DSP system)
    kSyncDuiId,            // 6) sent by cmAudDsp to client to indicate sync success or failure.
    kSetNotifyEnableDuiId, // enable/disable periodic status notification from the audio system.
    kClientMsgPollDuiId,   // Periodic check for and deliver messages waiting in the audio system for delivery to the client. 
    kSendMsgDuiId,         // forward msg to the audio system
    kDevReportDuiId,       // print a device report

    kRightAlignDuiId = 0,  // label alignment id used by kLabelDuiId 
    kLeftAlignDuiId,  
    kCenterAlignDuiId
  };

  enum
  {
    kDuplexDuiFl = 0x01
  };

  // Header record for all messages between the host and the DSP controllers.
  typedef struct
  {
    unsigned     asSubIdx;  // the audio sub-system this UI belongs to
    unsigned     uiId;      // msg type kXXXAsId 
    unsigned     selId;     // action to perform see above
    unsigned     flags;     //
    unsigned     instId;    // DSP instance id
    unsigned     instVarId; // DSP instance var id
    
    // The cmDspValue_t field must come last in the structure in
    // order for the cmDsvSerialize() to work in cmDspUI.c:_cmDspUiMsg().
    cmDspValue_t value;     // Data value associated with this msg.                             
  } cmDspUiHdr_t;

  // All of the UI messages that create a UI control contain an array of integers
  // as in the 'value' field. The array contains the id's associated with
  // the different programmable paramters which are part of the control.
  // For example a slider control has minimum,maximum, step size, and value 
  // parameters. The location in the array is hard coded according to the
  // parameters meaning but the actual value of the id is left up to the 
  // engine. This allows the engine to use whatever values work best for
  // it on a per instance basis. 



  /// The cmDspUiHdr_t.instId of UI control messages associated with master
  /// control encode the device,channel,in/out, and control type. These macros
  /// should be used for encoding and decoding.
#define cmAudioSysFormUiInstId(dev,ch,ifl,ctl) (((dev)<<16) + ((ch)<<4) + ((ifl)<<3) + (ctl))
#define cmAudioSysUiInstIdToDevIndex(instId)  ( (instId) >> 16)
#define cmAudioSysUiInstIdToChIndex(instId)   (((instId) &  0x0000ffff) >> 4)
#define cmAudioSysUiInstIdToInFlag(instId)    ( (instId) &  0x08)
#define cmAudioSysUiInstIdToCtlId(instId)     ( (instId) &  0x07)

  /// Control id's used to identify the control type of master contols.
  enum
  {
    kSliderUiAsId = 0,
    kMeterUiAsId  = 1,
    kMuteUiAsId   = 2,
    kToneUiAsId   = 3,
    kPassUiAsId   = 4
  };


  /// This message is transmitted to the host application just prior to returning
  /// from cmAudioSysInitialize().
  /// When transmitted to the host this record acts as a message header.
  /// This header is followed by two zero terminated char arrays containing the device
  /// labels associated with the input and output devices.
  /// Message Layout: [ cmAudioSysInitMsg_t "In Device Label" "Out Device Label"]
  typedef struct
  {
    unsigned asSubIdx;  ///< asSubIdx of this sub-system
    unsigned selId;     ///< always kSsInitAsId
    unsigned asSubCnt;  ///< count of sub-systems
    unsigned inDevIdx;  ///< input device index
    unsigned outDevIdx; ///< output device index
    unsigned inChCnt;   ///< input device channel count
    unsigned outChCnt;  ///< outut device channel count
  } cmAudioSysSsInitMsg_t;

  /// Audio sub-system status record - this message can be transmitted to the host at
  /// periodic intervals.  See cmAudioSysStatusNotifyEnable().
  /// When transmitted to the host this record acts as the message header.
  /// This header is followed by two arrays of doubles containing the input and output meter values
  /// associated with the input and output audio devices.
  /// Message Layout: [ asSubIdx kStatusSelId cmAudioSysStatus_t iMeterArray[iMeterCnt] oMeterArray[oMeterCnt] ]
  typedef struct
  {
    unsigned asSubIdx;     ///< originating audio sub-system

    unsigned updateCnt;    ///< count of callbacks from the audio devices.
    unsigned wakeupCnt;    ///< count of times the audio system thread has woken up after the cond. var has been signaled by the audio update thread.
    unsigned msgCbCnt;     ///< count of msgs delivered via cmAsCallback() .
    unsigned audioCbCnt;   ///< count of times the DSP execution was requested via cmAsCallback().    

    unsigned iDevIdx;      ///< Input device index
    unsigned oDevIdx;      ///< Output device index

    unsigned overflowCnt;  ///< count of times the audio input buffers overflowed
    unsigned underflowCnt; ///< count of times the audio output buffers underflowed
    unsigned iMeterCnt;    ///< count of input meter channels
    unsigned oMeterCnt;    ///< count of output meter channels
    
  } cmAudioSysStatus_t;

  // cmDspNetMsg_t sub-selector id's
  enum {
    kNetHelloSelAsId,     // node->node awake msg 
    kNetDstIdReqSelAsId,  // src->dst request a dst id
    kNetDstIdReqDoneAsId, // src->dst all requests have been sent
    kNetDstIdSelAsId,     // dst->src provide dst id
    kNetDoneSelAsId,      // node->node sync done
    kNetErrSelAsId,       // node->node sync error
    kNetEvtSelAsId        // src->dst send cmDspEvnt_t
  };

  // Message Layout [ cmDspNetMsg_t dstInstLabel[] dstVarLabel[] ]
  typedef struct
  {
    unsigned asSubIdx;
    unsigned selId;       // kNetSyncSelAsId
    unsigned subSelId;    // see above kNetXXXSelAsId
    unsigned srcId; 
    unsigned dstId;
    cmDspValue_t value;
    // char dstInstLabel[] - with kNetSyncSelAsId only
    // char dstVarLabel[]  - with kNetSyncSelAsId only
  } cmDspNetMsg_t;

  enum
  {
    kOkMsgRC = cmOkRC,
    kSerializeFailMsgRC,
    kSendFailMsgRC,
    kDecodeFailMsgRC
  };

  typedef cmRC_t cmMsgRC_t;


  typedef cmMsgRC_t (*cmMsgSendFuncPtr_t)(void* cbDataPtr, unsigned msgByteCnt, const void* msg );

  cmMsgRC_t cmMsgSend( 
  cmErr_t*            err, 
  unsigned            asSubIdx,
  unsigned            msgTypeId,
  unsigned            selId,
  unsigned            flags,
  unsigned            instId,
  unsigned            instVarId,
  const cmDspValue_t* valPtr,
  cmMsgSendFuncPtr_t  sendFunc,
  void*               cbDataPtr );

  cmMsgRC_t cmMsgPeekAsSubIndex( const void* msgArray[], unsigned msgByteCntArray[], unsigned segCnt, unsigned* retValPtr );
  cmMsgRC_t cmMsgPeekMsgTypeId(  const void* msgArray[], unsigned msgByteCntArray[], unsigned segCnt, unsigned* retValPtr );
  cmMsgRC_t cmMsgPeekSelId(      const void* msgArray[], unsigned msgByteCntArray[], unsigned segCnt, unsigned* retValPtr );
  cmMsgRC_t cmMsgPeekFlags(      const void* msgArray[], unsigned msgByteCntArray[], unsigned segCnt, unsigned* retValPtr );
  cmMsgRC_t cmMsgPeekInstId(     const void* msgArray[], unsigned msgByteCntArray[], unsigned segCnt, unsigned* retValPtr );
  cmMsgRC_t cmMsgPeekInstVarId(  const void* msgArray[], unsigned msgByteCntArray[], unsigned segCnt, unsigned* retValPtr );

#ifdef __cplusplus
}
#endif


#endif
