#ifndef cmMsgProtocol_h
#define cmMsgProtocol_h

#include "cmAudioSysMsg.h"

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"Global constants and data structures for transmiting messages between threads and network nodes.", kw:[real_time]}
  #define cmAudDspSys_FILENAME "aud_dsp.js"



  // UI seletor id's used in the cmDspUiHdr_t selId field
  enum 
  {
    kPrintDuiId,       // ui<--eng print the value to the console
    kSliderDuiId,      // ui<--eng create a slider control
    kButtonDuiId,      // ui<--eng create button control
    kCheckDuiId,       // ui<--eng create a check box control
    kLabelDuiId,       // ui<--eng create a label control
    kTimeLineDuiId,    // ui<--eng create a time-line control
    kScoreDuiId,       // ui<--eng create a score control
    kTakeSeqBldrDuiId, // ui<--eng create a take sequence builder
    kTakeSeqRendDuiId, // ui<--eng create a take sequence renderer
    kTwodDuiId,        // ui<--eng create a 2d control
    kNumberDuiId,      // ui<--eng create a number box
    kTextDuiId,        // ui<--eng create a text entry control
    kFnameDuiId,       // ui<--eng create a file/directory picker control
    kMsgListDuiId,     // ui<--eng create a msg list control
    kMeterDuiId,       // ui<--eng create a meter display
    kValueDuiId,       // ui<->eng a control changed values  
    kColumnDuiId,      // ui<--eng start a new column
    kHBorderDuiId,     // ui<--eng insert a vertical border 
    kPageDuiId,        // ui<--eng insert a new control page
    
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
    kPrintPgmDuiId,        // write the currently loaded pgm as a JSON file

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

  //)
  
#ifdef __cplusplus
}
#endif


#endif
