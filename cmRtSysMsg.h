#ifndef cmRtSysMsg_h
#define cmRtSysMsg_h

#ifdef __cplusplus
extern "C" {
#endif
  //( { file_desc:"rtSys message contants and data structures." kw:[rtsys] }
  
  // Reserved DSP message selector id's (second field of all 
  // host<->audio system messages)
  enum
  {
    kMidiMsgArraySelRtId = 1000,
    kMidiSysExSelRtId,
    kUiDrvrSelRtId,    // cmUiDriverArg_t message to/from the UI driver
    kUiSelRtId,        // cmUiDriverArg_t message from the UI mgr to a client 
    kUiMstrSelRtId,    // indicates a cmDspUiHdr_t msg containing master control information for the audio system
    kStatusSelRtId,    // indicates the msg is of type cmRtSysStatus_t
    kNetSyncSelRtId,   // sent with a cmDspNetMsg_t object  
    kMsgSelRtId,       // client defined msg transmitted between threads or network nodes
  };

  typedef struct
  {
    unsigned rtSubIdx;
    unsigned selId;  // Message selector id See kXXXSelRtId above
  } cmRtSysMsgHdr_t;

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
    cmRtSysMsgHdr_t hdr;
    unsigned     devIdx;
    unsigned     chIdx;
    bool         inFl;
    unsigned     ctlId;
    double       value;
  } cmRtSysMstr_t;



  // Control id's used to identify the control type of master contols.
  enum
  {
    kSliderUiRtId = 0,
    kMeterUiRtId  = 1,
    kMuteUiRtId   = 2,
    kToneUiRtId   = 3,
    kPassUiRtId   = 4
  };


  // Audio sub-system status record - this message can be transmitted to the host at
  // periodic intervals.  See cmRtSysStatusNotifyEnable().
  // When transmitted to the host this record acts as the message header.
  // This header is followed by two arrays of doubles containing the input 
  // and output meter values associated with the input and output audio devices.
  // Message Layout: 
  // [ rtSubIdx kStatusSelId cmRtSysStatus_t iMeterArray[iMeterCnt] oMeterArray[oMeterCnt] ]
  typedef struct
  {
    cmRtSysMsgHdr_t hdr;

    unsigned updateCnt;    // count of callbacks from the audio devices.
    unsigned wakeupCnt;    // count of times the audio system thread has woken up after the cond. var has been signaled by the audio update thread.
    unsigned msgCbCnt;     // count of msgs delivered via cmRtCallback() .
    unsigned audioCbCnt;   // count of times the DSP execution was requested via cmRtCallback().    

    unsigned iDevIdx;      // Input device index
    unsigned oDevIdx;      // Output device index

    unsigned overflowCnt;  // count of times the audio input buffers overflowed
    unsigned underflowCnt; // count of times the audio output buffers underflowed
    unsigned iMeterCnt;    // count of input meter channels
    unsigned oMeterCnt;    // count of output meter channels
    
  } cmRtSysStatus_t;


  typedef struct
  {
    cmRtSysMsgHdr_t hdr;
    unsigned     devIdx;
    unsigned     portIdx;
    unsigned     msgCnt;
    // cmMidiMsg msgArray[msgCnt]
  } cmRtSysMidi_t;

  typedef struct
  {
    cmRtSysMsgHdr_t    hdr;         // hdr.rtSubIdx = dest rtSubIdx
                                    // hdr.selId    = msg contents
    unsigned           dstEndPtId;  //              = dest endpoint
    unsigned           srcEndPtId;  //              = src endpoint id
    unsigned           srcNodeIdx;  //              = src node index (filled in by receiving cmRtNet mgr)
    // char msg[ msgByteCnt ]
  } cmRtNetMsg_t;

  //)
  
#ifdef __cplusplus
}
#endif

#endif
