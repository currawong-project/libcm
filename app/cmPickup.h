#ifndef cmPickup_h
#define cmPickup_h

#ifdef __cplusplus
extern "C" {
#endif


  enum
  {
    kOkPuRC = cmOkRC,
    kProcFailPuRC,
    kJsonFailPuRC
  };

  typedef cmRC_t     cmPuRC_t;
  typedef cmHandle_t cmPuH_t;

  // This record holds information which is maintained on a per-pickup basis
  typedef struct
  {
    unsigned begSmpIdx;  // during auto-gain cfg set to the first sample in the audio example file where the group of notes from this pickup begin.
    unsigned endSmpIdx;  // ... end of the example notes for this pickup
    unsigned midiPitch;  // midi pitch associated with this pickup
    unsigned onCnt;      // during auto-gain cfg set to the count of onsets detected for this pickup
    unsigned offCnt;     // ... offset detected
    cmReal_t gateMaxAvg; // avg of the the max gate RMS values for all detected notes   
    cmReal_t gain;       // auto-gain coeff for this pickup

  } cmPuCh_t;

  
  extern cmPuH_t cmPuNullHandle;

  cmPuRC_t cmPuAlloc( cmCtx_t* ctx, cmPuH_t* hp );
  cmPuRC_t cmPuFree( cmPuH_t* hp );
  bool     cmPuIsValid( cmPuH_t h );
  
  // Given a recorded audio file containing a set of notes recorded from each pickup,
  // an Audacity label file which marks the beginning of teach set of notes,
  // and a set of gate detector parameters attempt to calculate a set of gain
  // coefficients to equalize the relative gain of all the pickup channels.
  // This algorithm works in two passes: In the first pass the gain coefficient
  // is established by finding an average RMS value among the examples and
  // then increasing and decreasing the pickups to move the examples closer
  // to the average.  The gain is then adjusted and a second pass is made to
  // examine how well the gate detectors work with the new gain setttings.
  cmPuRC_t cmPuAutoGainCfg( 
    cmPuH_t                   h,
    const cmChar_t*           audioFn,     // audio file containing a set of examples for each note
    const cmChar_t*           labelFn,     // audicity label file with one marker preceding each set of notes
    const cmChar_t*           outMtx0Fn,   // octave binary matrix file containing the intermediate and final results of the gate detector analysis after the first pass
    const cmChar_t*           outMtx1Fn,   // octave binary matrix file containing the intermediate and final results of the gate detector analysis after the second pass
    const cmChar_t*           outAudFn,    // audioFn rewritten with auto-gain applied
    unsigned                  procSmpCnt,  // analysis DSP frame size in samples
    cmReal_t                  hopMs,       // analysis window hop size in milliseconds
    const cmGateDetectParams* gd0Args,     // first pass gate detector args
    const cmGateDetectParams* gd1Args );   // second pass gate detector args

  // Calls cmPuReadJson() and then prepends the fileDir to each of the
  // file names. All the files are then read and written to this directory.
  // and calls cmPuAutoGainCfg(). 
  cmPuRC_t cmPuAutoGainCfgFromJson( 
    cmPuH_t             h,
    const cmChar_t*     fileDir,
    cmJsonH_t           jsH,
    cmJsonNode_t*       onp,
    unsigned            procSmpCnt );


  cmPuRC_t cmPuAutoGainExec( cmPuH_t h, const cmChar_t* fileDir, unsigned procSmpCnt );

  // Load the 'parmsCfg', 'gdArgs' and 'gain' array from the JSON 
  // 'autoTune' object contained in the JSON object 'onp'.
  cmPuRC_t cmPuReadJson(      cmPuH_t h, cmJsonH_t jsH, cmJsonNode_t* onp );

  unsigned        cmPuChannelCount(   cmPuH_t h );
  const cmPuCh_t* cmPuChannel( cmPuH_t h, unsigned chIdx );
    

  // Update the 'autoTune' JSON object contained the JSON object 'onp'.
  cmPuRC_t cmPuWriteJson(     cmPuH_t h, cmJsonH_t jsH, cmJsonNode_t* onp );
  
  // Update the 'autoTune' JSON object in the JSON file 'jsonFn'.
  // (Same as cmPuWriteJson() except the JSON tree to update is contained in a file.)
  cmPuRC_t cmPuWriteJsonFile( cmPuH_t h, const cmChar_t* jsonFn  );

  void cmPuReport( cmPuH_t h, cmRpt_t* rpt );

  void cmPuTest(cmCtx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif
