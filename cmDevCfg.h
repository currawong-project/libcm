#ifndef cmDevCfg_h
#define cmDevCfg_h

#ifdef __cplusplus
extern "C" {
#endif
  /*
    IMPLEMENTATION:
    1) A 'cfg' record is a device reference with a 'cfg label'.
       There are three kinds of cfg records MIDI,Audio,Net.
       The device record identifies a particlar device
       end-point or pre-configured setup.  The 'cfg label'
       associated with this setup allows an application
       to refer to the setup by name.  This releives the
       application from having to handle the details of
       forming, storing, and maintaining device configurations.

    2) A 'map' record is a record which links an 
       application and reference id to a cfg record.

       The goal of this is to allow applications to refer to
       pre-configured device setups by name and then to 
       associate a numeric id with this setup. 
       The application can then send data to the device using
       only the numeric id. Likewise data arriving from the
       device to the application is tagged with 
       id of the of the device.

    NOTES:
    1) usrAppId's must be unique among all app.s'.
    2) usrDevId's must be unique among all usrDevId's for particular app.
       In other words the same 'usrDevId' may not be used by devices
       of different types.
    3) The usrAppId's and usrDevIds' are used internally as index
       locations. They should therefore be low numbers and 
       densely packed.
   */


  struct cmAudioSysArgs_t;

  enum
  {
    kOkDcRC = cmOkRC,
    cmLabelNotFoundDcRC,
    cmIdNotFoundDcRC,
    kInvalidDevArgDcRC
  };

  typedef enum
  {
    kInvalidDcmTId,
    kMidiDcmTId,
    kAudioDcmTId,
    kNetDcmTId
  } cmTypeDcmId_t;

  typedef cmRC_t     cmDcRC_t;
  typedef cmHandle_t cmDevCfgH_t;

  extern cmDevCfgH_t cmDevCfgNullHandle;

  cmDcRC_t cmDevCfgMgrAlloc( cmCtx_t* c, cmDevCfgH_t* hp, cmJsonH_t jsH );
  cmDcRC_t cmDevCfgMgrFree( cmDevCfgH_t* hp );
  cmDcRC_t cmDevCfgIsValid( cmDevCfgH_t h );

  
  // Delete a cfg record created by cmDevCfgNameMidiPort(), cmDevCfgNameAudioPort(), etc.
  cmDcRC_t cmDevCfgDeleteCfg( cmDevCfgH_t h, cmTypeDcmId_t typeId, const cmChar_t* dcLabelStr );

  // Create a map record to associate a app/dev id with a cfg. record.
  // Note that multiple app/dev id's may be assoc'd with the same cfg. record.
  cmDcRC_t cmDevCfgCreateMap( cmDevCfgH_t h, cmTypeDcmId_t typeId, const cmChar_t* dcLabelStr, unsigned usrAppId, unsigned usrDevId );

  // Delete a map record created by cmDevCfgCreateMap().
  cmDcRC_t cmDevCfgDeleteMap( cmDevCfgH_t h, cmTypeDcmId_t typeId, unsigned usrAppId, unsigned usrDevId );
  
  // Create a MIDI cfg. record.
  cmDcRC_t cmDevCfgNameMidiPort( 
    cmDevCfgH_t     h,
    const cmChar_t* dcLabelStr, 
    const cmChar_t* devNameStr, 
    const cmChar_t* portNameStr, 
    bool            inputFl );

  cmDcRC_t cmDevCfgMidiDevIdx(    cmDevCfgH_t h, unsigned usrAppId, unsigned usrDevId, unsigned* midiDevIdxRef, unsigned* midiPortIdxRef );


  cmDcRC_t cmDevCfgNameAudioPort( 
    cmDevCfgH_t     h,
    const cmChar_t* dcLabelStr, 
    const cmChar_t* inDevNameStr, 
    const cmChar_t* outDevNameStr,
    bool            syncInputFl,
    unsigned        msgQueueByteCnt,
    unsigned        devFramesPerCycle,
    unsigned        dspFramesPerCycle,
    unsigned        audioBufCnt,
    double          srate  );

  const struct cmAudioSysArgs_str* cmDevCfgAudioSysArgs( cmDevCfgH_t h, unsigned usrAppId, unsigned usrDevId );

  cmDcRC_t cmDevCfgNetPort(
    cmDevCfgH_t      h,
    const cmChar_t* dcLabelStr,
    const cmChar_t* sockAddr,
    unsigned        portNumber );

  unsigned        cmDevCfgNetNodeId(     cmDevCfgH_t h, unsigned usrAppId, unsigned usrDevId );

  // Preset Management Functions:
  // Store and recall groups cfg records.

  unsigned        cmDevCfgPresetCount( cmDevCfgH_t h );
  const cmChar_t* cmDevCfgPresetLabel( cmDevCfgH_t h, unsigned presetIdx );
  cmDcRC_t        cmDevCfgStore(       cmDevCfgH_t h, const cmChar_t* presetLabelStr );
  cmDcRC_t        cmDevCfgRecall(      cmDevCfgH_t h, const cmChar_t* presetLabelStr );
  cmDcRC_t        cmDevCfgDelete(      cmDevCfgH_t h, const cmChar_t* presetLabelStr );

  cmDcRC_t cmDevCfgWrite( cmDevCfgH_t h );
  

#ifdef __cplusplus
}
#endif

#endif
