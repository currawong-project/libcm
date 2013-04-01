#ifndef cmAudioSysMsg_h
#define cmAudioSysMsg_h

#ifdef __cplusplus
extern "C" {
#endif

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

  typedef struct
  {
    unsigned asSubIdx;
    unsigned selAsId;  // Message selector id See kXXXSelAsId above
    unsigned selId;    // Message specific selector
  } cmAudioSysMsg_t;

  // All of the UI messages that create a UI control contain an array of integers
  // as in the 'value' field. The array contains the id's associated with
  // the different programmable paramters which are part of the control.
  // For example a slider control has minimum,maximum, step size, and value 
  // parameters. The location in the array is hard coded according to the
  // parameters meaning but the actual value of the id is left up to the 
  // engine. This allows the engine to use whatever values work best for
  // it on a per instance basis. 


  // Header record for all messages between the host and the DSP controllers.
  typedef struct
  {
    unsigned     asSubIdx;  // the audio sub-system this UI belongs to
    unsigned     uiId;      // msg type kXXXAsId 
    unsigned     selId;     // action to perform see above
    unsigned     flags;     //
    unsigned     instId;    // DSP instance id
    unsigned     instVarId; // DSP instance var id
    unsigned     rsrvd;
    double       value;
  } cmAudioSysMstr_t;


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


#ifdef __cplusplus
}
#endif

#endif
