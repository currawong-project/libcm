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
       to refer to the setup by name.  This relieves the
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


  struct cmRtSysArgs_str;

  enum
  {
    kOkDcRC = cmOkRC,
    kLHeapFailDcRC,
    kLabelNotFoundDcRC,
    kDuplLabelDcRC,
    kBlankLabelDcRC,
    kInvalidUserAppIdRC,
    kInvalidUserMapIdRC,
    kInvalidArgDcRC,
    kInvalidCfgIdxDcRC,
    kJsonFailDcRC,
    kInvalidFnDcRC
  };

  typedef enum
  {
    kInvalidDcmTId, // kInvalidDcmTId must be zero
    kMidiDcmTId,
    kAudioDcmTId,
    kNetDcmTId
  } cmTypeDcmId_t;

  typedef cmRC_t     cmDcRC_t;
  typedef cmHandle_t cmDevCfgH_t;

  typedef struct
  {
    cmChar_t* devLabelStr;    // Midi device label.
    cmChar_t* portLabelStr;   // Midi device port label.
    bool      inputFl;        // 'True' if this is an input port.
    unsigned  devIdx;         // Midi device index.
    unsigned  portIdx;        // Midi port index.
  } cmDcmMidi_t;

  typedef struct
  {
    cmChar_t*     inDevLabelStr;  // Input audio device label.
    cmChar_t*     outDevLabelStr; // Output audio device label.
    cmRtSysArgs_t audioSysArgs;   // Audio system  cfg recd
    bool          dfltFl;         // true if this is the default audio cfg.
    bool          activeFl;
  } cmDcmAudio_t;

  typedef struct              
  {
    cmChar_t* sockAddr;   // socket address.
    unsigned  portNumber; // socket port number
    bool      localFl;    // this is the local port
    bool      activeFl;   // this port is active/inactive
  } cmDcmNet_t;

  extern cmDevCfgH_t cmDevCfgNullHandle;

  cmDcRC_t cmDevCfgAlloc( cmCtx_t* c, cmDevCfgH_t* hp, const cmChar_t* fn );
  cmDcRC_t cmDevCfgFree( cmDevCfgH_t* hp );
  bool     cmDevCfgIsValid( cmDevCfgH_t h );

  // Return the count of cfg records for the given type in the current location.
  unsigned cmDevCfgCount( cmDevCfgH_t h, cmTypeDcmId_t typeId );

  // Return the label for a each cfg record of a given type in the current location.
  const cmChar_t* cmDevCfgLabel( cmDevCfgH_t h, cmTypeDcmId_t typeId, unsigned index );

  // Return the description for a give cfg. record.
  const cmChar_t* cmDevCfgDesc( cmDevCfgH_t h, cmTypeDcmId_t typeId, unsigned index );

  // Return the cfg index assoc'd with a given label in the current location.
  unsigned cmDevCfgLabelToIndex( cmDevCfgH_t h, cmTypeDcmId_t typeId, const cmChar_t* label );
  
  // Delete a cfg record created by cmDevCfgNameMidiPort(), cmDevCfgNameAudioPort(), etc.
  cmDcRC_t cmDevCfgDeleteCfg( cmDevCfgH_t h, cmTypeDcmId_t typeId, const cmChar_t* dcLabelStr );

  // Create a map record to associate a app/dev id with a cfg. record.
  // Note that multiple app/dev id's may be assoc'd with the same cfg. record.
  cmDcRC_t cmDevCfgCreateMap( cmDevCfgH_t h, cmTypeDcmId_t typeId, const cmChar_t* dcLabelStr, unsigned usrAppId, unsigned usrMapId );

  // Delete a map record created by cmDevCfgCreateMap().
  cmDcRC_t cmDevCfgDeleteMap( cmDevCfgH_t h, cmTypeDcmId_t typeId, unsigned usrAppId, unsigned usrMapId );
  

  // Create a MIDI cfg. record.
  cmDcRC_t cmDevCfgNameMidiPort( 
    cmDevCfgH_t     h,
    const cmChar_t* dcLabelStr, 
    const cmChar_t* devNameStr, 
    const cmChar_t* portNameStr, 
    bool            inputFl );

  const cmDcmMidi_t* cmDevCfgMidiCfg( cmDevCfgH_t h, unsigned cfgIdx );
  const cmDcmMidi_t* cmDevCfgMidiMap( cmDevCfgH_t h, unsigned usrAppId, unsigned usrMapId );
  
  const cmDcmMidi_t* cmDevCfgMidiCfgFromLabel( cmDevCfgH_t h, const cmChar_t* cfgLabel );


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
    double          srate,
    bool            activeFl );

  bool                cmDevCfgAudioIsDeviceActive( cmDevCfgH_t h, const cmChar_t* devNameStr, bool inputFl );
  unsigned            cmDevCfgAudioActiveCount( cmDevCfgH_t h );
  const cmChar_t*     cmDevCfgAudioActiveLabel( cmDevCfgH_t h, unsigned idx );
  const cmDcmAudio_t* cmDevCfgAudioActiveCfg(   cmDevCfgH_t h, unsigned idx );
  unsigned            cmDevCfgAudioActiveIndex( cmDevCfgH_t h, const cmChar_t* cfgLabel );

  const cmDcmAudio_t* cmDevCfgAudioCfg( cmDevCfgH_t h, unsigned cfgIdx );
  const cmDcmAudio_t* cmDevCfgAudioMap( cmDevCfgH_t h, unsigned usrAppId, unsigned usrMapId );
  

  const struct cmRtSysArgs_str* cmDevCfgRtSysArgs( cmDevCfgH_t h, unsigned usrAppId, unsigned usrMapId );

  cmDcRC_t cmDevCfgNameNetPort(
    cmDevCfgH_t      h,
    const cmChar_t* dcLabelStr,
    const cmChar_t* sockAddr,
    unsigned        portNumber,
    bool            localFl,
    bool            activeFl);

  unsigned          cmDevCfgNetActiveCount( cmDevCfgH_t h );
  const cmDcmNet_t* cmDevCfgNetActiveCfg( cmDevCfgH_t h, unsigned idx );

  const cmDcmNet_t* cmDevCfgNetCfg( cmDevCfgH_t h, unsigned cfgIdx );
  const cmDcmNet_t* cmDevCfgNetMap( cmDevCfgH_t h, unsigned usrAppId, unsigned usrMapId );

  //---------------------------------------------------------------------------------------
  // Location Management Functions:
  // Store and recall groups of cfg records.

  // Return a count of the current number of locations.
  unsigned        cmDevCfgLocCount(  cmDevCfgH_t h );

  // Given a location index (0 to cmDevCfgLocCount()-1) return the locations label.
  const cmChar_t* cmDevCfgLocLabel(  cmDevCfgH_t h, unsigned locIdx );

  // If 'locLabelStr' has already been used then this function does nothing and returns.
  // otherwise the current location is duplicated and the duplicate is named 'locLabelStr'.
  cmDcRC_t        cmDevCfgLocStore(  cmDevCfgH_t h, const cmChar_t* locLabelStr );

  // Make the location named 'locLabelStr' the current location.
  cmDcRC_t        cmDevCfgLocRecall( cmDevCfgH_t h, const cmChar_t* locLabelStr );

  // Delete the location named by 'locLabelStr'.
  cmDcRC_t        cmDevCfgLocDelete( cmDevCfgH_t h, const cmChar_t* locLabelStr );

  // Return the current location index
  unsigned        cmDevCfgLocCurIndex(  cmDevCfgH_t h );

  // Set 'fn' to NULL to use filename from cmDevCfgAlloc()
  cmDcRC_t cmDevCfgWrite( cmDevCfgH_t h, const cmChar_t* fn );
  

#ifdef __cplusplus
}
#endif

#endif
