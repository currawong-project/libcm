#ifndef cmAudioBuf_h
#define cmAudioBuf_h

#ifdef __cplusplus
extern "C" {
#endif

  //( {file_desc: "Obsolete audio buffer class. This class is superceded by cmApBuf."}
  
  enum
  {
    kOkBaRC = cmOkRC
    kBufOverunBaRC,
    kBufUnderunBaRC
  };

  enum
  {
    kInBaFl  = 0x01,
    kOutBaFl = 0x02
  };

  typedef cmRC_t cmBaRC_t;

  cmBaRC_t cmAudioBufInit( cmCtx_t* ctx, unsigned devCnt );

  cmBaRC_t cmAudioBufFinal();

  cmBaRC_t cmAudioBufSetup(
    unsigned devIdx,
    unsigned cycleCnt,
    unsigned inSubDevCnt,
    unsigned inChCnt,
    unsigned inFrameCnt,
    unsigned outChCnt,
    unsigned outFrameCnt,
    unsigned outSubDevCnt );

  // Called from the audio driver within incoming samples to store (inPktArray)
  // and empty buffers (outPktArray) to fill with outgoin samples.
  cmBaRC_t cmAudioBufUpdate(
    cmApAudioPacket_t* inPktArray,  ///< full audio packets from incoming audio (from ADC)
    unsigned           inPktCnt,    ///< count of incoming audio packets
    cmApAudioPacket_t* outPktArray, ///< empty audio packet for outgoing audio (to DAC)  
    unsigned           outPktCnt    ///< count of outgoing audio packets
                         );

  bool cmAudioBufIsDeviceReady( unsigned devIdx, unsigned flags );
  
  cmBaRC_t cmAudioBufGet(     
    unsigned devIdx, 
    unsigned flags, 
    cmApAudioPacket_t* pktArray[], 
    unsigned pktCnt );

  cmBaRC_t cmAudioBufAdvance( unsigned devIdx, unsigned flags );

  //)


#ifdef __cplusplus
  }
#endif

#endif
