//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
//( { file_desc:" Audio file acoustic feature analyzer and accompanying file reader." kw:[audio analysis file]}
//
//

#ifndef cmFeatFile_h
#define cmFeatFile_h

#ifdef __cplusplus
extern "C" {
#endif



  // Result codes for all functions in cmFeatFile.h
  enum
  {
    kOkFtRC = cmOkRC,
    kCfgParseFailFtRC,
    kFileSysFailFtRC,
    kJsonFailFtRC,
    kDspProcFailFtRC,
    kDirCreateFailFtRC,
    kFileNotFoundFtRC,
    kAudioFileOpenFailFtRC,
    kFrameFileFailFtRC,
    kChIdxInvalidFtRC,
    kParamRangeFtRC,
    kParamErrorFtRC,
    kFrameWriteFailFtRC,
    kEofFtRC,
    kPlviewFailFtRC,
    kSerialFailFtRC,
    kInvalidFeatIdFtRC,
    kFileFailFtRC,
    kInvalidFrmIdxFtRC
  };

  // Feature Id's
  enum
  {
    kInvalidFtId,     // 0  
    kAmplFtId,        // 1 Fourier transform amplitude 
    kDbAmplFtId,      // 2 Fourier transform decibel
    kPowFtId,         // 3 Fourier transform power
    kDbPowFtId,       // 4 Fourier transform power decibel
    kPhaseFtId,       // 5 Fourier transform phase (not unwrapped)
    kBfccFtId,        // 6 Bark Frequency Cepstral Coeffcients
    kMfccFtId,        // 7 Mel Frequency Cepstral Coefficients
    kCepsFtId,        // 8 Cepstral Coefficients
    kConstQFtId,      // 9 Constant-Q transform
    kLogConstQFtId,   // 10 Log Constant-Q transform
    kRmsFtId,         // 11 Root means square of the audio signal
    kDbRmsFtId,       // 12 RMS in decibels                         

    kD1AmplFtId,      // 13 1st order difference over time of the Fourier transform amplitude              
    kD1DbAmplFtId,    // 14 1st order difference over time of the Fourier transform decibel                
    kD1PowFtId,       // 15 1st order difference over time of the Fourier transform power                  
    kD1DbPowFtId,     // 16 1st order difference over time of the Fourier transform power decibel          
    kD1PhaseFtId,     // 17 1st order difference over time of the Fourier transform phase (not unwrapped)  
    kD1BfccFtId,      // 18 1st order difference over time of the Bark Frequency Cepstral Coeffcients      
    kD1MfccFtId,      // 19 1st order difference over time of the Mel Frequency Cepstral Coefficients      
    kD1CepsFtId,      // 20 1st order difference over time of the Cepstral Coefficients                    
    kD1ConstQFtId,    // 21 1st order difference over time of the Constant-Q transform                     
    kD1LogConstQFtId, // 22 1st order difference over time of the Log Constant-Q transform                
    kD1RmsFtId,       // 23 1st order difference over time of the Root means square of the audio signal   
    kD1DbRmsFtId,     // 24 1st order difference over time of the RMS in decibels                         

  };

  // User defined feature parameters
  typedef struct
  {
    unsigned id;         // feature id
    unsigned cnt;        // length of feature vector
    bool     normFl;     // normalize this feature 
    bool     enableFl;   // true if this feature is enabled
  } cmFtAttr_t;


  // Skip input audio range record
  typedef struct
  {
    unsigned smpIdx;  // Index of first sample to skip
    unsigned smpCnt;  // Count of successive samples to skip.
  } cmFtSkip_t;


  // Analysis parameters
  typedef struct
  {
    const char*        audioFn;               // Audio file name.
    const char*        featFn;                // Feature file name.
    unsigned           chIdx;                 // Audio file channel index
    cmReal_t           wndMs;                 // Length of the analysis window in milliseconds.
    unsigned           hopFact;               // Analysis window overlap factor 1 = 1:1 2=2:1 ...
    bool               normAudioFl;           // Normalize the audio over the length of the audio file
    cmMidiByte_t       constQMinPitch;        // Used to determine the base const-q octave.
    cmMidiByte_t       constQMaxPitch;        // Used to determine the maximum const-q frequency of interest.
    unsigned           constQBinsPerOctave;   // Bands per const-q octave.
    unsigned           onsetMedFiltWndSmpCnt; // Complex onset median filter 
    cmReal_t           onsetThreshold;        // Complex onset threshold
    cmReal_t           minDb;                 // Fourier Transform magnitude values below minDb are set to minDb.
    cmReal_t           floorThreshDb;         // Frames with an RMS below this value will be skipped
    cmFtSkip_t*        skipArray;             // skipArray[skipCnt] user defined sample skip ranges
    unsigned           skipCnt;               // Count of records in skipArray[].
    cmFtAttr_t*        attrArray;             // attrArray[attrCnt] user defined parameter array
    unsigned           attrCnt;               // Count of records in attrArray[].
  } cmFtParam_t;


  // Feature summary information
  typedef struct 
  {
    unsigned    id;   // feature id (same as associated cmFtAttr.id)
    unsigned    cnt;  // length of each feature vector (same as associated cmFtAttr.cnt)

    // The raw feature summary values are calculated prior to normalization.
    cmReal_t* rawMinV;  // Vector of min value over time for each feature element.
    cmReal_t* rawMaxV;  // Vector of max value over time for each feature element.
    cmReal_t* rawAvgV;  // Vector of avg value over time for each feature element.
    cmReal_t* rawSdvV;  // Vector of standard deviation values over time for each feature element.
    cmReal_t  rawMin;   // Min value of all values for this feature. Equivalent to min(rawMinV).
    cmReal_t  rawMax;   // Max value of all values for this feature. Equivalent to max(rawMaxV).

    // normalized feature summary values
    cmReal_t* normMinV; // Vector of min value over time for each feature element.
    cmReal_t* normMaxV; // Vector of max value over time for each feature element.
    cmReal_t* normAvgV; // Vector of avg value over time for each feature element.
    cmReal_t* normSdvV; // Vector of standard deviation values over time for each feature element.
    cmReal_t  normMin;  // Min value of all values for this feature. Equivalent to min(normMinV).
    cmReal_t  normMax;  // Max value of all values for this feature. Equivalent to max(rawMaxV).

  } cmFtSumm_t;

  // Feature file info record
  typedef struct
  {
    unsigned    frmCnt;         // count of frames in the file
    cmReal_t    srate;          // audio sample rate
    unsigned    smpCnt;         // audio sample count
    unsigned    fftSmpCnt;      // FFT window length (always power of 2)
    unsigned    hopSmpCnt;      // audio sample hop count
    unsigned    binCnt;         // FFT bin count (always fftSmpCnt/2 + 1)
    unsigned    skipFrmCnt;     // count of frames skipped based on user skip array 
    unsigned    floorFrmCnt;    // count of frames skipped because below floorThreshDb
    cmFtParam_t param;          // analysis parameter record used to form this feature file
    cmFtSumm_t* summArray;      // summArray[ param.attrCnt ] feature summary information
  } cmFtInfo_t;

  // Data structure returned by cmFtReaderAdvance(). 
  typedef struct
  {
    unsigned smpIdx;  // The audio signal sample index this frames information is based on.
    unsigned frmIdx;  // The frame index relative to other frames in this feature file.
  } cmFtFrameDesc_t;

  typedef cmHandle_t cmFtH_t;      // Analyzer handle
  typedef cmHandle_t cmFtFileH_t;  // Feature file handle.
  typedef unsigned   cmFtRC_t;     // Result code type used by all functions in cmFeatFile.h.

  extern cmFtH_t     cmFtNullHandle;      // A NULL handle useful for indicating an uninitialized analyzer.
  extern cmFtFileH_t cmFtFileNullHandle;  // A NULL handle useful for indicating an uninitialized feature file.


  // Given a feature type id return the associated label.
  const char*     cmFtFeatIdToLabel( unsigned featId );

  // Given a feature type label return the associated id.
  unsigned        cmFtFeatLabelToId( const char* label );

  // Feature Analyzer Related functions

  // Initialize the feature analyzer. The memory manager and file system must
  // be initialized (cmMdInitialize(), cmFsInitialize()) prior to calling this function.
  cmFtRC_t        cmFtInitialize( cmFtH_t* hp, cmCtx_t* ctx );

  // Finalize a feature analyzer.
  cmFtRC_t        cmFtFinalize( cmFtH_t* h );

  // Return true if the handle represents an initialized feature analyzer.
  bool            cmFtIsValid( cmFtH_t  h );

  // Parse a JSON file containing a set of analysis parameters. 
  cmFtRC_t        cmFtParse( cmFtH_t  h, const char* cfgFn );

  // Run the analyzer.
  cmFtRC_t        cmFtAnalyze( cmFtH_t h );

  // If cmFtAnalyze() is being run in a seperate thread this function
  // can be used to access the analyzers progress.
  const char*     cmFtAnalyzeProgress( cmFtH_t h, unsigned* passPtr, cmReal_t* percentPtr );  

  
  // Feature File Related Functions

  // Open a feature file.
  // Note that inforPtrPtr is optional and will be ignored if it is set to NULL.
  cmFtRC_t        cmFtReaderOpen(    cmFtH_t h, cmFtFileH_t* hp, const char* featFn, const cmFtInfo_t** infoPtrPtr );

  // Close a feature file.
  cmFtRC_t        cmFtReaderClose(   cmFtFileH_t* hp );

  // Return true if the handle reprents an open feature file.
  bool            cmFtReaderIsValid( cmFtFileH_t h );

  // Return the count of features types this file contains.
  unsigned        cmFtReaderFeatCount( cmFtFileH_t h );

  // Return the feature type id associated with the specified index.
  unsigned        cmFtReaderFeatId( cmFtFileH_t h, unsigned index );

  // Reset the current file location to the first frame but do not load it.
  // The next call to cmFtReadAdvance() will load the next frame.
  cmFtRC_t        cmFtReaderRewind(  cmFtFileH_t h );

  // Make frmIdx the current file location.
  cmFtRC_t        cmFtReaderSeek(    cmFtFileH_t h, unsigned frmIdx );

  // Load the current frame, advance the current file position, and return
  // a pointer to a cmFtFrameDesc_t record for the loaded frame.
  // Returns kEofFtRC upon reaching end of file. 
  // The frameDescPtr is optional.
  cmFtRC_t        cmFtReaderAdvance( cmFtFileH_t h, cmFtFrameDesc_t* frameDescPtr );

  // Returns a pointer to a data matrix in the feature identified by featId in the current feature frame. 
  cmReal_t*       cmFtReaderData(    cmFtFileH_t h, unsigned featId, unsigned* cntPtr );

  // Copy the contents of a given set of frames into buf[frmCnt*elePerFrmCnt].
  cmFtRC_t        cmFtReaderCopy(    cmFtFileH_t h, unsigned featId, unsigned frmIdx, cmReal_t* buf, unsigned frmCnt, unsigned elePerFrmCnt, unsigned* outEleCntPtr );  

  // Data structure used to specify multiple features for use by cmFtReaderMultiSetup().
  typedef struct
  {
    unsigned featId; // Feature id of feature to include in the feature vector
    unsigned cnt;    // Set to count of feat ele's for this feat. Error if greater than avail. Set to -1 to use all avail ele's.
                     //  returned with actual count used

    unsigned id0;    // Ignored on input. Used internally by cmFtReaderXXX() 
    unsigned id1;    // Ignored on input. Used internally by cmFtReaderXXX() 
  } cmFtMulti_t;
  
  // Setup an array of cmFtMulti_t records.  The cmFtMulti_t array
  // used by cmFtReaderMulitData() must be initialized by this function.
  cmFtRC_t        cmFtReaderMultiSetup(  cmFtFileH_t h, cmFtMulti_t* multiArray, unsigned multiCnt, unsigned* featVectEleCntPtr ); 

  // Fill outV[outN] with a consecutive data from the features specified in the cmFtMulti_t array.
  // Use cmFtReaderMultiSetup() to configure the cmFtMulti_t array prior to calling this function.
  cmFtRC_t        cmFtReaderMultiData(   cmFtFileH_t h, const cmFtMulti_t* multiArray, unsigned multiCnt, cmReal_t* outV, unsigned outN ); 

  // Report summary information for the specified feature.
  cmFtRC_t        cmFtReaderReport(  cmFtFileH_t h, unsigned featId );

  // Identical to cmFtReaderReport() except the feature file is identified from a file name rather than an open cmFtFileH_t.
  cmFtRC_t        cmFtReaderReportFn(  cmFtH_t h, const cmChar_t* fn, unsigned featId );

  // Report feature data for the specified set of feature frames.
  cmFtRC_t        cmFtReaderReportFeature( cmFtFileH_t h, unsigned featId, unsigned frmIdx, unsigned frmCnt );

  // Write a feature into a binary file.  
  // Set 'frmCnt' to the cmInvalidCnt to include all frames past frmIdx.
  // The first three unsigned values in the output file
  // contain the row count, maximum column count, and the count of bytes in each data element (4=float,8=double). 
  // Each row of the file begins with the count of elements in the row and is followed by a data array.
  cmFtRC_t        cmFtReaderToBinary( cmFtFileH_t h, unsigned featId, unsigned frmIdx, unsigned frmCnt, const cmChar_t* outFn ); 

  // Identical to cmFtReaderToBinary() except it takes a feature file name instead of a file handle.
  cmFtRC_t        cmFtReaderToBinaryFn( cmFtH_t h, const cmChar_t* fn, unsigned featId, unsigned frmIdx, unsigned frmCnt, const cmChar_t* outFn ); 

  //)
  
#ifdef __cplusplus
}
#endif

#endif
