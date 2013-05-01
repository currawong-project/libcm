//
// http://developer.apple.com/library/mac/#documentation/MusicAudio/Reference/CACoreAudioReference
//
#include <Carbon/Carbon.h>

#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmAudioPort.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmAudioPortOsx.h"

#include <CoreAudio/CoreAudio.h>
#include <unistd.h> // usleap

typedef struct 
{
  unsigned chIdx;
  unsigned chCnt;
  unsigned framesPerBuf;
} cmApOsxBufRecd;

typedef struct
{
  AudioDeviceID       devId;
  AudioDeviceIOProcID ioProcId;
  unsigned            devIdx;
  cmApCallbackPtr_t   callbackPtr;

  char*               mfgStr;
  char*               nameStr;
  double              srate;

  unsigned            inFramesPerCycle;
  unsigned            inChCnt;
  unsigned            inPktCnt;
  cmApAudioPacket_t*  inPktArray;

  unsigned            outFramesPerCycle;
  unsigned            outChCnt;
  unsigned            outPktCnt;
  cmApAudioPacket_t*  outPktArray;

  unsigned            timeOutMs;
  AudioDeviceID       fmtChangeDevId;
  AudioStreamID       fmtChangeStreamId;

} cmApOsxDevRecd;

typedef struct
{
  cmRpt_t*      rpt;
  cmApOsxDevRecd*  devArray;
  unsigned      devCnt;
} cmApOsxRoot;

cmApOsxRoot _cmApOsxRoot = { NULL, NULL, 0 };



const char* _cmApOsxOsStatusToText( OSStatus errCode )
{
	switch( errCode )
	{
		case kAudioHardwareNoError:                   return "No Error"; 						
		case kAudioHardwareNotRunningError:           return "Not runing error";	
		case kAudioHardwareUnspecifiedError:          return "Unspecified error";
		case kAudioHardwareUnknownPropertyError:      return "Unknown property error";
		case kAudioHardwareBadPropertySizeError:      return "Bad property error";
		case kAudioHardwareIllegalOperationError:     return "Illegal operation error";	
		case kAudioHardwareBadDeviceError:            return "Bad device error";	
		case kAudioHardwareBadStreamError:            return "Bad stream error";
		case kAudioHardwareUnsupportedOperationError:	return "Unsupported operating error";
		case kAudioDeviceUnsupportedFormatError:      return "Unsupported format error";
		case kAudioDevicePermissionsError:            return "Permissions error";
	}
	return "Unknown error code";
}


cmApRC_t _cmApOsxOsErrorRpt( OSStatus err, cmApOsxRoot* r, const char* func, const char* file, int line )
{
  if( r->rpt != NULL )
  {
    if( err != noErr )
      cmRptErrorf(r->rpt,"Audio Port Error %s in %s line:%i %s\n",_cmApOsxOsStatusToText(err),func,line,file);
    else
      cmRptErrorf(r->rpt,"Audio Port Error: Unknown\n");
  }
	
	return kSysErrApRC;
}

#define _cmApOsxOsError( err, root ) _cmApOsxOsErrorRpt( err, root, __FUNCTION__, __FILE__, __LINE__ )

OSStatus _cmApOsxAllocDeviceCFString( AudioDeviceID devId, UInt32 ch, Boolean inputFl, AudioDevicePropertyID devPropId, char** strPtrPtr  ) 
{
  CFStringRef  cfStr;
	//Boolean      outWritableFl = false;
	UInt32       outByteCnt    = sizeof(cfStr);;
	OSStatus     err           = noErr;
  
	if((err = AudioDeviceGetProperty( devId, ch, inputFl, devPropId, &outByteCnt, &cfStr )) != noErr )
		return err;

  CFIndex cfLen = CFStringGetLength(cfStr) * 2;

  char* cStr = cmMemAllocZ( char, cfLen+1 );
  cStr[0] = 0;
  
  if( CFStringGetCString( cfStr, cStr, cfLen, kCFStringEncodingUTF8 ) )
    cStr[cfLen]=0;

  CFRelease(cfStr);
	
  *strPtrPtr = cStr;

	return noErr;
}

// Note: the streamIdArray* allocated by this function must be released by the calling function.
OSStatus _cmApOsxAllocStreamIdArray( AudioDeviceID devId, Boolean inputFl, AudioStreamID** streamIdArrayPtr, unsigned* streamIdCntPtr )
{
  UInt32   byteCnt    = 0;
  Boolean  canWriteFl = false;
	OSStatus err        = noErr;

  *streamIdArrayPtr = NULL;
  *streamIdCntPtr   = 0;

  // get the length of the stream id array
	if((err = AudioDeviceGetPropertyInfo( devId, 0, inputFl, kAudioDevicePropertyStreams, &byteCnt, &canWriteFl )) != noErr )
    return err;
	
	if( byteCnt <= 0 )
    goto doneLabel;

  // get the count of streams
  *streamIdCntPtr     = byteCnt / sizeof(AudioStreamID);

  // allocate the stream id array
  *streamIdArrayPtr = cmMemAllocZ( AudioStreamID, *streamIdCntPtr );

  // verify that the size of the stream id array is an integer multiple of the sizeof(AudioStreamID)
  assert( *streamIdCntPtr * sizeof(AudioStreamID) == byteCnt );
	
  // fill the stream id array
  if((err = AudioDeviceGetProperty( devId, 0, inputFl, kAudioDevicePropertyStreams, &byteCnt, *streamIdArrayPtr )) != noErr )
    return err;

 doneLabel:
  return noErr;
}

OSStatus 	cmApOsxSystemStreamPropertyListenerProc(
											    AudioStreamID         inStream, 
											    UInt32                inChannel, 
											    AudioDevicePropertyID inPropertyID, 
											    void*                 inClientData)
{
  cmApOsxDevRecd* drp = (cmApOsxDevRecd*)inClientData;

  drp->fmtChangeDevId    = drp->devId;
  drp->fmtChangeStreamId = inStream;

  return noErr;
}											    


OSStatus _cmApOsxSetSampleRate( cmApOsxDevRecd* drp, bool inputFl, double newSampleRate )
{
	AudioStreamID* streamIdArray = NULL;
  unsigned       streamCnt;
	OSStatus 			 err 		       = noErr;
	unsigned			 waitMs		     = 0;
	unsigned       i;

  // allocate a stream id array
  if((err = _cmApOsxAllocStreamIdArray( drp->devId, inputFl, &streamIdArray, &streamCnt )) != noErr )
    return _cmApOsxOsError(err,&_cmApOsxRoot);
		
	// 	for each stream on this device
	for(i=0; i<streamCnt; i++)
	{
    UInt32                   byteCnt    = 0;
		AudioStreamBasicDescription sdr;
		Boolean                     canWriteFl = false;

    // get the size of the stream desc recd
    if((err = AudioDeviceGetPropertyInfo( drp->devId, i, inputFl, kAudioDevicePropertyStreamFormat, &byteCnt, &canWriteFl )) != noErr )
    {
       _cmApOsxOsError(err,&_cmApOsxRoot);
       goto errLabel;
		}

    assert( byteCnt == sizeof(sdr) );
	
    // get the stream desc recd
    if((err = AudioDeviceGetProperty( drp->devId, i, inputFl, kAudioDevicePropertyStreamFormat, &byteCnt, &sdr )) != noErr )
    {
       _cmApOsxOsError(err,&_cmApOsxRoot);
       goto errLabel;
		}

		// if the format has not already been set
		if( sdr.mSampleRate != newSampleRate )
		{	
			// change the sample rate
      sdr.mSampleRate = newSampleRate;

      drp->fmtChangeDevId 	  = -1;
      drp->fmtChangeStreamId	= -1;

			// attempt to change the sample rate
			if((err = AudioStreamSetProperty(streamIdArray[i], NULL, 0, kAudioDevicePropertyStreamFormat, sizeof(sdr), &sdr)) != noErr )
			{
				err = _cmApOsxOsError(err,&_cmApOsxRoot);
				goto errLabel;
			}
				
			// wait for confirmation
			waitMs = 0;
			while( 	 drp->fmtChangeDevId != drp->devId 
        && 	drp->fmtChangeStreamId != streamIdArray[i] 
        && 	waitMs < drp->timeOutMs )
			{
				const unsigned waitIncrMs = 20;
				usleep(waitIncrMs*1000);
				waitMs += waitIncrMs;
			}
			
			// wait timed out
			if( waitMs >= drp->timeOutMs )
			{
				err = _cmApOsxOsError(kAudioHardwareUnspecifiedError,&_cmApOsxRoot);
				goto errLabel;
			}
			else
			{
        // read back the format to be really sure it changed
        if((err = AudioDeviceGetProperty( drp->devId, i, inputFl, kAudioDevicePropertyStreamFormat, &byteCnt, &sdr )) != noErr )
        {
           _cmApOsxOsError(err,&_cmApOsxRoot);
           goto errLabel;
        }
			    	 
				assert( sdr.mSampleRate == newSampleRate );		    	
			}			
		}
		
	}
	
 errLabel:
  cmMemPtrFree(&streamIdArray);
  
		return err;
}

OSStatus _cmApOsxSetFramesPerCycle( cmApOsxDevRecd* drp, Boolean inputFl, unsigned newFramesPerCycle )
{
  OSStatus       err               = noErr;
  AudioTimeStamp when;
  UInt32         ch                = 0;
  UInt32         devPropId         = kAudioDevicePropertyBufferFrameSize;
  UInt32         outByteCnt        = sizeof(UInt32);
  UInt32         curFramesPerCycle = 0;
	
	// set time stamp to zero to force the change to be immediate
	when.mHostTime = 0;
	when.mFlags = kAudioTimeStampHostTimeValid;

	// get the cur value off the param. to change
	if((err = AudioDeviceGetProperty( drp->devId, ch, inputFl, devPropId, &outByteCnt, &curFramesPerCycle )) != noErr )
		return _cmApOsxOsError(err,&_cmApOsxRoot);

	// if the cur value is the same as the new value then there is nothing to do
	if( curFramesPerCycle == newFramesPerCycle )
		return noErr;

	// attempt to set the new value	
	if((err = AudioDeviceSetProperty( drp->devId, &when, ch, inputFl, devPropId, sizeof(newFramesPerCycle), &newFramesPerCycle)) != noErr )
		return _cmApOsxOsError(err,&_cmApOsxRoot);
	
	// wait for the value to actually change
	unsigned waitMs = 0;
	while( waitMs < drp->timeOutMs )
	{
		const unsigned waitIncrMs = 20;		
		usleep(waitIncrMs*1000);
		
		// read the parameter value
    if((err = AudioDeviceGetProperty( drp->devId, ch, inputFl, devPropId, &outByteCnt, &curFramesPerCycle )) != noErr )
      return _cmApOsxOsError(err,&_cmApOsxRoot);

		// if the parameter value equals the new value then the change has taken effect	
		if( curFramesPerCycle == newFramesPerCycle )
			break;
		
		waitMs += waitIncrMs;
	}
			
	// wait timed out
	if( waitMs >= drp->timeOutMs )
		return _cmApOsxOsError(kAudioHardwareUnspecifiedError,&_cmApOsxRoot);
	
	return noErr;
}


// Note: *bufArrayPtr must be deallocated by the caller.
  cmApRC_t _cmApOsxGetBufferCfg(unsigned devIdx, AudioDeviceID devId, Boolean inputFl, cmApOsxDevRecd* drp )
{
  cmApRC_t           rc            = kOkApRC;
	OSStatus         err           = noErr;
	UInt32           byteCnt       = 0;
	Boolean          canWriteFl    = 0;
  AudioBufferList* ablp          = NULL;	
  unsigned         streamCnt     = 0;
  AudioStreamID*   streamIdArray = NULL;
  unsigned         i             = 0;
  unsigned         chIdx         = 0;
   
	// get the size of stream cfg buffer
	if( (err = AudioDeviceGetPropertyInfo(devId,0,inputFl,kAudioDevicePropertyStreamConfiguration,&byteCnt,&canWriteFl)) != noErr )
		return _cmApOsxOsError(err,&_cmApOsxRoot);

	// allocate memory to hold the AudioBufferList
	ablp = (AudioBufferList*)cmMemMallocZ(byteCnt);		
		
	// get the audio buffer list array
	if((err = AudioDeviceGetProperty(devId,0,inputFl,kAudioDevicePropertyStreamConfiguration,&byteCnt,ablp)) != noErr )
	{
    rc = _cmApOsxOsError(err,&_cmApOsxRoot);
    goto errLabel;		
	}

  // allocate a stream id array
  if((err = _cmApOsxAllocStreamIdArray( devId, inputFl, &streamIdArray, &streamCnt )) != noErr )
  {
    rc = _cmApOsxOsError(err,&_cmApOsxRoot);
    goto errLabel;
  }

  // the number of buffers and the number of frames must be the same
	assert( streamCnt == ablp->mNumberBuffers);

  cmApAudioPacket_t* pktArray = cmMemAllocZ(cmApAudioPacket_t,ablp->mNumberBuffers);

  if( inputFl )
  {
    drp->inPktCnt   = ablp->mNumberBuffers; 
    drp->inPktArray = pktArray;
  }
  else
  {
    drp->outPktCnt   = ablp->mNumberBuffers;
    drp->outPktArray = pktArray;
  }


  for(i=0; i<ablp->mNumberBuffers; ++i)
  {
		AudioStreamBasicDescription sdr;
		
    // get the size of the stream desc recd
    if((err = AudioDeviceGetPropertyInfo( devId, i, inputFl, kAudioDevicePropertyStreamFormat, &byteCnt, &canWriteFl )) != noErr )
    {
      _cmApOsxOsError(err,&_cmApOsxRoot);
      goto errLabel;
		}

    assert( byteCnt == sizeof(sdr) );
	
    // get the stream desc recd
    if((err = AudioDeviceGetProperty( devId, i, inputFl, kAudioDevicePropertyStreamFormat, &byteCnt, &sdr )) != noErr )
    {
      _cmApOsxOsError(err,&_cmApOsxRoot);
      goto errLabel;
    }

    // assert that the data format is packed float32 native endian
    //assert( IsAudioFormatNativeEndian(sdr) );

    // 0x6c70636d = lpcm 
    //printf("%s %i dev:%i sr:%f fmtId:0x%lx fmtFl:0x%lx bytesPerPkt:%li frmsPerPkt:%li bytesPerFrm:%li chsPerFrm:%li bitsPerCh:%li\n",
    //  inputFl?"i":"o",i,devIdx,sdr.mSampleRate,sdr.mFormatID,sdr.mFormatFlags,sdr.mBytesPerPacket,sdr.mFramesPerPacket,sdr.mBytesPerFrame,sdr.mChannelsPerFrame,sdr.mBitsPerChannel);


    // assert that all buffers have the sample rate of the device
    if( drp->srate == 0 )
      drp->srate = sdr.mSampleRate;

    assert( drp->srate == sdr.mSampleRate );

    // install a stream property listener
		AudioStreamAddPropertyListener( streamIdArray[i], 
										kAudioPropertyWildcardChannel,
										kAudioPropertyWildcardPropertyID,
										cmApOsxSystemStreamPropertyListenerProc,
										drp );
    
    pktArray[i].devIdx         = devIdx;
    pktArray[i].begChIdx       = chIdx;
    pktArray[i].chCnt          =  ablp->mBuffers[i].mNumberChannels; //sdr.mChannelsPerFrame;
    pktArray[i].audioFramesCnt =  ablp->mBuffers[i].mDataByteSize / (sizeof(cmApSample_t) * ablp->mBuffers[i].mNumberChannels); //sdr.mFramesPerPacket;
    pktArray[i].flags          = kInterleavedApFl | kFloatApFl;
    pktArray[i].bitsPerSample  = sizeof(cmApSample_t) * 8;
    pktArray[i].audioBytesPtr  = NULL; 
    pktArray[i].userCbPtr      = NULL; // the userCbPtr value isn't availabe until the cmApOsxDeviceSetup()

    // verify that all buffers on this device have the same size
    assert(i==0 || pktArray[i].audioFramesCnt == pktArray[i-1].audioFramesCnt );


    chIdx +=  ablp->mBuffers[i].mNumberChannels; //sdr.mChannelsPerFrame;

    if( inputFl )
    {
      // track the total number of input channels on this device
      drp->inChCnt = chIdx;

      // track the frames per cycle on this device
      if( drp->inFramesPerCycle == 0 )
        drp->inFramesPerCycle = pktArray[i].audioFramesCnt;

      
      assert( drp->inFramesPerCycle == pktArray[i].audioFramesCnt );
    }
    else
    {
      drp->outChCnt = chIdx;

      if( drp->outFramesPerCycle == 0 )
        drp->outFramesPerCycle = pktArray[i].audioFramesCnt;

      assert( drp->outFramesPerCycle == pktArray[i].audioFramesCnt );

    }
  }
  
 errLabel:
  cmMemPtrFree(&streamIdArray);
  cmMemPtrFree(&ablp);
  return rc; 
}


OSStatus _cmApOsxSystemDeviceIOProc(	AudioDeviceID			inDevice,
													const AudioTimeStamp*	  inNow,
													const AudioBufferList*	iabl,
													const AudioTimeStamp*	  inInputTime,
													AudioBufferList*		    oabl, 
													const AudioTimeStamp*	  inOutputTime,
													void*					          inClientData)
{
 
	cmApOsxDevRecd*       drp    = (cmApOsxDevRecd*)inClientData;

  if( iabl->mNumberBuffers!=0 && iabl->mNumberBuffers != drp->inPktCnt )
    return noErr;

  if( oabl->mNumberBuffers!=0 && oabl->mNumberBuffers != drp->outPktCnt  )
    return noErr;

  //assert( iabl->mNumberBuffers==0 || iabl->mNumberBuffers == drp->inPktCnt );
  //assert( oabl->mNumberBuffers==0 || oabl->mNumberBuffers == drp->outPktCnt );

  // setup the incoming packets (ADC->app)
  AudioBuffer*       bp = iabl->mBuffers;
  AudioBuffer*       ep = bp + iabl->mNumberBuffers;
  cmApAudioPacket_t* pp = drp->inPktArray;
  unsigned           chIdx = 0;

  for(; bp<ep; ++bp,++pp)
  {
    pp->audioBytesPtr  = (float*)bp->mData;
    pp->audioFramesCnt =  bp->mDataByteSize / (bp->mNumberChannels * sizeof(cmApSample_t));
    pp->begChIdx       = chIdx;
    pp->chCnt          = bp->mNumberChannels;
    chIdx             += pp->chCnt;
  }

  // setup the outgoing packets (app->DAC)
  bp = oabl->mBuffers;
  ep = bp + oabl->mNumberBuffers;
  pp = drp->outPktArray;

  for(chIdx=0; bp<ep; ++bp,++pp)
  {
    pp->audioBytesPtr  = (float*)bp->mData;
    pp->audioFramesCnt =  bp->mDataByteSize / (bp->mNumberChannels * sizeof(cmApSample_t));
    pp->begChIdx       = chIdx;
    pp->chCnt          = bp->mNumberChannels;
    chIdx             += pp->chCnt;
  }

  // call the app
  drp->callbackPtr( 
    iabl->mNumberBuffers ? drp->inPktArray  : NULL, iabl->mNumberBuffers,
    oabl->mNumberBuffers ? drp->outPktArray : NULL, oabl->mNumberBuffers );
 
   return noErr;
}


cmApRC_t      cmApOsxInitialize( cmRpt_t* rpt, unsigned baseApDevIdx )
{
  cmApRC_t       rc            = kOkApRC;
  Boolean        outWritableFl = false;
  UInt32         outByteCnt    = 0;
  OSStatus       err           = noErr;
  AudioDeviceID* devIdArray    = NULL;
  unsigned       i             = 0;
  cmApOsxDevRecd*   devArray   = NULL;

  if((rc = cmApOsxFinalize()) != kOkApRC )
    return rc;

  _cmApOsxRoot.rpt      = rpt;
  _cmApOsxRoot.devArray = NULL;
	
  // get the size of the device id array
  if((err = AudioHardwareGetPropertyInfo(kAudioHardwarePropertyDevices, &outByteCnt, &outWritableFl )) != noErr )
    return _cmApOsxOsError(err,&_cmApOsxRoot);
		
  assert( outByteCnt > 0 );
  
  // calc. the device count
  _cmApOsxRoot.devCnt  = outByteCnt / sizeof(AudioDeviceID);

  assert( _cmApOsxRoot.devCnt*sizeof(AudioDeviceID) == outByteCnt );
		
  // allocate space for the device id array and the device array
  devIdArray       = cmMemAllocZ( AudioDeviceID, _cmApOsxRoot.devCnt );	
  devArray         = cmMemAllocZ( cmApOsxDevRecd,     _cmApOsxRoot.devCnt );
  _cmApOsxRoot.devArray = devArray;

  // get the device id array
  if((err = AudioHardwareGetProperty( kAudioHardwarePropertyDevices, &outByteCnt, devIdArray )) != noErr )
  {
    rc = _cmApOsxOsError(err,&_cmApOsxRoot);
    goto errLabel;
  }

  // for each 
  for(i=0; i<_cmApOsxRoot.devCnt; ++i)
  {
    // device name
    if((err = _cmApOsxAllocDeviceCFString(devIdArray[i], 0, false, kAudioDevicePropertyDeviceNameCFString, &devArray[i].nameStr  )) != noErr )
    {
      rc = _cmApOsxOsError(err,&_cmApOsxRoot);
      goto errLabel;
    }

    // device mfg
    if((err = _cmApOsxAllocDeviceCFString(devIdArray[i], 0, false, kAudioDevicePropertyDeviceManufacturerCFString, &devArray[i].mfgStr  )) != noErr )
    {
      rc = _cmApOsxOsError(err,&_cmApOsxRoot);
      goto errLabel;
    }

    // in buf array
    if((rc = _cmApOsxGetBufferCfg(i,devIdArray[i], true, devArray + i )) != kOkApRC )
      goto errLabel;

    // out buf array
    if((rc = _cmApOsxGetBufferCfg(i,devIdArray[i], false, devArray + i)) != kOkApRC )
      goto errLabel;


    devArray[i].devId           = devIdArray[i];
    devArray[i].devIdx          = i;
    devArray[i].callbackPtr     = NULL;
    devArray[i].timeOutMs       = 1000;
    devArray[i].ioProcId        = NULL;
  }
  
 errLabel:

  if( devIdArray != NULL )
    cmMemFree(devIdArray);

  if( rc == kOkApRC )
    return rc;


  cmApOsxFinalize();
  return rc;
}

cmApRC_t      cmApOsxFinalize()
{
  cmApOsxDevRecd* devArray = _cmApOsxRoot.devArray;
  unsigned   i        = 0;
  OSStatus   err;

  for(i=0; i<_cmApOsxRoot.devCnt; ++i)
  {
    unsigned       j;

    for(j=0; j<2; ++j)
    { 
      AudioStreamID* streamIdArray;
      unsigned       streamCnt;
      unsigned       k;

      if((err = _cmApOsxAllocStreamIdArray(devArray[i].devId, true,&streamIdArray,&streamCnt )) != noErr )
        _cmApOsxOsError(err,&_cmApOsxRoot);

      for(k=0; k<streamCnt; ++k)
        if((err = AudioStreamRemovePropertyListener( streamIdArray[k], 
              kAudioPropertyWildcardChannel,
              kAudioPropertyWildcardPropertyID,
              cmApOsxSystemStreamPropertyListenerProc)) != noErr )
          _cmApOsxOsError(err,&_cmApOsxRoot);

      cmMemPtrFree(&streamIdArray);
        
    }  

		//if((err = AudioDeviceRemoveIOProc( devArray[i].devId, _cmApOsxSystemDeviceIOProc )) != noErr )
    if( devArray[i].ioProcId != NULL )
      if((err = AudioDeviceDestroyIOProcID( devArray[i].devId, devArray[i].ioProcId )) != noErr )
        _cmApOsxOsError(err,&_cmApOsxRoot);

    if( devArray[i].nameStr != NULL )
      cmMemFree(devArray[i].nameStr);

    if( devArray[i].mfgStr != NULL )
      cmMemFree(devArray[i].mfgStr);

    if( devArray[i].inPktArray != NULL )
      cmMemFree( devArray[i].inPktArray );

    if( devArray[i].outPktArray!= NULL )
      cmMemFree( devArray[i].outPktArray );  
  }

  if( devArray != NULL )
  {
    cmMemFree(devArray);
    _cmApOsxRoot.devArray = NULL;
    _cmApOsxRoot.devCnt   = 0;
  }

  return kOkApRC;
}

cmApRC_t      cmApOsxDeviceCount()
{ return _cmApOsxRoot.devCnt; }

const char* cmApOsxDeviceLabel( unsigned devIdx )
{
  assert( devIdx < _cmApOsxRoot.devCnt );
  return _cmApOsxRoot.devArray[ devIdx ].nameStr;
}

unsigned    cmApOsxDeviceChannelCount( unsigned devIdx, bool inputFl )
{
  assert( devIdx < _cmApOsxRoot.devCnt );
  return inputFl ? _cmApOsxRoot.devArray[ devIdx ].inChCnt : _cmApOsxRoot.devArray[ devIdx ].outChCnt;
}

double      cmApOsxDeviceSampleRate( unsigned devIdx )
{
  assert( devIdx < _cmApOsxRoot.devCnt );
  return _cmApOsxRoot.devArray[ devIdx ].srate;
}

unsigned    cmApOsxDeviceFramesPerCycle( unsigned devIdx, bool inputFl )
{
  assert( devIdx < _cmApOsxRoot.devCnt );
  return inputFl ? _cmApOsxRoot.devArray[ devIdx ].inFramesPerCycle : _cmApOsxRoot.devArray[ devIdx ].outFramesPerCycle;
}

cmApRC_t      cmApOsxDeviceSetup(          
  unsigned          devIdx, 
  double            srate, 
  unsigned          framesPerCycle, 
  cmApCallbackPtr_t callbackPtr,
  void*             userCbPtr )
{
  assert( devIdx < _cmApOsxRoot.devCnt );

  //cmApRC_t     rc  = kOkApRC;
  OSStatus   err;
  cmApOsxDevRecd* drp = _cmApOsxRoot.devArray + devIdx;
  unsigned   j;
  
  if( cmApOsxDeviceIsStarted(devIdx) )
    cmApOsxDeviceStop(devIdx);

  // set the sample rate
  if( drp->srate != srate )
  {
    for(j=0; j<2; ++j )
      if((err = _cmApOsxSetSampleRate(drp, j==0, srate )) != noErr )
        goto errLabel;

    drp->srate = srate;
  }

  // set the frames per cycle
  for(j=0; j<2; ++j)
  {
    unsigned* fpcPtr = j==0 ? &drp->inFramesPerCycle : &drp->outFramesPerCycle;

    if( framesPerCycle !=  (*fpcPtr) )
    {
      if((err = _cmApOsxSetFramesPerCycle(drp, j==0, framesPerCycle )) == noErr )
        *fpcPtr = framesPerCycle;
      else
        goto errLabel;
    }  
  }

  // set the user callback data ptr in each packet on this device
  for(j=0; j<2; ++j)
  {
    unsigned         k;
    cmApAudioPacket_t* packetArray = j==0 ? drp->inPktArray : drp->outPktArray;
    unsigned           pktCnt      = j==0 ? drp->inPktCnt   : drp->outPktCnt;

    for(k=0; k<pktCnt; ++k)
      packetArray[k].userCbPtr = userCbPtr;
  }

  drp->callbackPtr = callbackPtr;

  // if the io 
  if( drp->ioProcId != NULL )
    if((err = AudioDeviceDestroyIOProcID( drp->devId, drp->ioProcId )) != noErr )
      _cmApOsxOsError(err,&_cmApOsxRoot);


  // set the io proc 			
  drp->ioProcId = NULL;
  //if( (err = AudioDeviceAddIOProc(drp->devId,_cmApOsxSystemDeviceIOProc,(void*)drp) ) != noErr )
  if( (err = AudioDeviceCreateIOProcID(drp->devId,_cmApOsxSystemDeviceIOProc,(void*)drp,&drp->ioProcId) ) != noErr )
  {
    _cmApOsxOsError(err,&_cmApOsxRoot);
    return kSysErrApRC;
  }	

  return kOkApRC;

 errLabel:

  return kSysErrApRC;
}


cmApRC_t      cmApOsxDeviceStart( unsigned devIdx )
{
  assert( devIdx < _cmApOsxRoot.devCnt );
  OSStatus err;

  //if( (err = AudioDeviceStart(_cmApOsxRoot.devArray[devIdx].devId,_cmApOsxSystemDeviceIOProc)) != noErr )
  if( (err = AudioDeviceStart(_cmApOsxRoot.devArray[devIdx].devId,_cmApOsxRoot.devArray[devIdx].ioProcId)) != noErr )
	{
		_cmApOsxOsError(err,&_cmApOsxRoot);	
		return kSysErrApRC;
	}

  return kOkApRC;
}

cmApRC_t      cmApOsxDeviceStop( unsigned devIdx )
{
  assert( devIdx < _cmApOsxRoot.devCnt );

	OSStatus err;
		 
  //if( (err = AudioDeviceStop(_cmApOsxRoot.devArray[devIdx].devId,_cmApOsxSystemDeviceIOProc)) != noErr )
  if( (err = AudioDeviceStop(_cmApOsxRoot.devArray[devIdx].devId,_cmApOsxRoot.devArray[devIdx].ioProcId)) != noErr )
	{
		_cmApOsxOsError(err,&_cmApOsxRoot);	
		return kSysErrApRC;
	}

  return kOkApRC;
}

bool        cmApOsxDeviceIsStarted( unsigned devIdx )
{
  assert( devIdx < _cmApOsxRoot.devCnt );
  return false;
}
/*
void apReport( apPrintFunc_t printFunc )
{
  unsigned i;
  for(i=0; i<_cmApOsxRoot.devCnt; ++i)
  {
    cmApOsxDevRecd* drp = _cmApOsxRoot.devArray + i;
    printf("%i in:%i out:%i %s %s\n",i,drp->inChCnt,drp->outChCnt,drp->nameStr,drp->mfgStr);
  }
}
*/
