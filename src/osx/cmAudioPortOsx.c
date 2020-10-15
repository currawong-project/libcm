//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
//
// http://developer.apple.com/library/mac/#documentation/MusicAudio/Reference/CACoreAudioReference
//
#include <Carbon/Carbon.h>

#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmTime.h"
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
  AudioObjectID       fmtChangeObjId;

} cmApOsxDevRecd;

typedef struct
{
  cmRpt_t*         rpt;
  cmApOsxDevRecd*  devArray;
  unsigned         devCnt;
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
    case kAudioHardwareBadObjectError:            return "Bad object error";
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
  char errStr[5];
  unsigned i;

  for(i=0; i<4; ++i)
    errStr[i] = (char)(err & (0x000000ff << i));

  errStr[4] = 0;
    

  if( r->rpt != NULL )
  {
    if( err != noErr )
      cmRptErrorf(r->rpt,"Audio Port Error : %s in %s line:%i %s\n",_cmApOsxOsStatusToText(err),func,line,file);
    else
      cmRptErrorf(r->rpt,"Audio Port Error: Unknown\n");
  }
	
	return kSysErrApRC;
}

#define _cmApOsxOsError( err, root ) _cmApOsxOsErrorRpt( err, root, __FUNCTION__, __FILE__, __LINE__ )

OSStatus 	cmApOsxSystemStreamPropertyListenerProc(
											    AudioObjectID                     audioObjId, 
											    UInt32                            channel, 
											    const AudioObjectPropertyAddress* propertyAddr, 
											    void*                             clientData)
{
  cmApOsxDevRecd* drp = (cmApOsxDevRecd*)clientData;

  drp->fmtChangeDevId = drp->devId;
  drp->fmtChangeObjId = audioObjId;

  return noErr;
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
  const AudioBuffer* bp    = iabl->mBuffers;
  const AudioBuffer* ep    = bp + iabl->mNumberBuffers;
  cmApAudioPacket_t* pp    = drp->inPktArray;
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

OSStatus _cmApOsxAllocPropertyCFString( 
  AudioDeviceID               devId, 
  AudioObjectPropertySelector sel,
  AudioObjectPropertyScope    scope,
  AudioObjectPropertyElement  ele,
  char**                      strPtrPtr  ) 
{
  CFStringRef  cfStr;
	UInt32       outByteCnt    = sizeof(cfStr);;
	OSStatus     err           = noErr;
  AudioObjectPropertyAddress addr;

  addr.mSelector  = sel;
  addr.mScope     = scope;
  addr.mElement   = ele; 
  
  if((err = AudioObjectGetPropertyData(devId, &addr, 0, NULL, &outByteCnt, &cfStr)) != noErr )
		return err;

  CFIndex cfLen = CFStringGetLength(cfStr) * 2;
  char*   cStr  = cmMemAllocZ( char, cfLen+1 );

  cStr[0] = 0;
  
  if( CFStringGetCString( cfStr, cStr, cfLen, kCFStringEncodingUTF8 ) )
    cStr[cfLen]=0;

  CFRelease(cfStr);
	
  *strPtrPtr = cStr;

	return noErr;
}

// Note: the streamIdArray* allocated by this function must be released by the calling function.
OSStatus _cmApOsxAllocateStreamIdArray( AudioDeviceID devId, Boolean inputFl, AudioStreamID** streamIdArrayPtr, unsigned* streamIdCntPtr )
{

  UInt32   byteCnt                = 0;
	OSStatus err                    = noErr;
  AudioObjectPropertyAddress addr = 
    { 
      kAudioHardwarePropertyDevices, 
      inputFl ? kAudioDevicePropertyScopeInput :  kAudioObjectPropertyScopeOutput, 
      kAudioObjectPropertyElementMaster 
    };

  *streamIdArrayPtr = NULL;
  *streamIdCntPtr   = 0;

  // get the length of the stream id array
  addr.mSelector = kAudioDevicePropertyStreams;
  if((err = AudioObjectGetPropertyDataSize(devId, &addr, 0, NULL, &byteCnt)) != noErr ) 
    return _cmApOsxOsError(err,&_cmApOsxRoot);
	
	if( byteCnt <= 0 )
    goto errLabel;

  // get the count of streams
  *streamIdCntPtr   = byteCnt / sizeof(AudioStreamID);

  // allocate the stream id array
  *streamIdArrayPtr = cmMemAllocZ( AudioStreamID, *streamIdCntPtr );

  // verify that the size of the stream id array is an integer multiple of the sizeof(AudioStreamID)
  assert( *streamIdCntPtr * sizeof(AudioStreamID) == byteCnt );
	
  // fill the stream id array
  if ((err=AudioObjectGetPropertyData(devId, &addr, 0, NULL, &byteCnt, *streamIdArrayPtr)) != noErr ) 
  { 
    _cmApOsxOsError(err,&_cmApOsxRoot);
    goto errLabel;
  }

 errLabel:
  return noErr;
}


// Note: *bufArrayPtr must be deallocated by the caller.
cmApRC_t _cmApOsxGetBufferConfig(unsigned devIdx, AudioDeviceID devId, Boolean inputFl, cmApOsxDevRecd* drp )
{
  cmApRC_t         rc            = kOkApRC;
	OSStatus         err           = noErr;
	UInt32           byteCnt       = 0;
  AudioBufferList* ablp          = NULL;	
  unsigned         streamCnt     = 0;
  AudioStreamID*   streamIdArray = NULL;
  unsigned         i             = 0;
  unsigned         chIdx         = 0;
   
  AudioObjectPropertyAddress addr = 
    { 
      kAudioDevicePropertyStreamConfiguration,
      inputFl ? kAudioDevicePropertyScopeInput :  kAudioObjectPropertyScopeOutput, 
      kAudioObjectPropertyElementMaster 
    };

	// get the size of stream cfg buffer
  if((err = AudioObjectGetPropertyDataSize(devId, &addr, 0, NULL, &byteCnt)) != noErr ) 
    return _cmApOsxOsError(err,&_cmApOsxRoot);

	// allocate memory to hold the AudioBufferList
	ablp = (AudioBufferList*)cmMemMallocZ(byteCnt);		

  // get the audio buffer list array
  if ((err=AudioObjectGetPropertyData(devId, &addr, 0, NULL, &byteCnt, ablp)) != noErr ) 
  { 
    rc = _cmApOsxOsError(err,&_cmApOsxRoot);
    goto errLabel;
  }

  // allocate a stream id array
  if((err = _cmApOsxAllocateStreamIdArray( devId, inputFl, &streamIdArray, &streamCnt )) != noErr )
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
  

  //
  for(i=0; i<ablp->mNumberBuffers; ++i)
  {
		AudioStreamBasicDescription sdr;
		
    // get the size of the stream desc recd
    addr.mSelector = kAudioDevicePropertyStreamFormat;
    if((err = AudioObjectGetPropertyDataSize(devId, &addr, 0, NULL, &byteCnt)) != noErr ) 
    {
      _cmApOsxOsError(err,&_cmApOsxRoot);
      goto errLabel;
    }

    assert( byteCnt == sizeof(sdr) );
	
    // get the stream desc recd
    if((err=AudioObjectGetPropertyData(devId, &addr, 0, NULL, &byteCnt, &sdr)) != noErr ) 
    { 
      rc = _cmApOsxOsError(err,&_cmApOsxRoot);
      goto errLabel;
    }

    // assert that the data format is packed float32 native endian
    // assert( IsAudioFormatNativeEndian(sdr) );

    //  0x6c70636d = lpcm 
    //  printf("%s %i dev:%i sr:%f fmtId:0x%lx fmtFl:0x%lx bytesPerPkt:%li frmsPerPkt:%li bytesPerFrm:%li chsPerFrm:%li bitsPerCh:%li\n",
    //  inputFl?"i":"o",i,devIdx,sdr.mSampleRate,sdr.mFormatID,sdr.mFormatFlags,sdr.mBytesPerPacket,sdr.mFramesPerPacket,sdr.mBytesPerFrame,sdr.mChannelsPerFrame,sdr.mBitsPerChannel);


    // assert that all buffers have the sample rate of the device
    if( drp->srate == 0 )
      drp->srate = sdr.mSampleRate;

    assert( drp->srate == sdr.mSampleRate );

    AudioObjectPropertyAddress listenerAddr = 
      { 
        kAudioObjectPropertySelectorWildcard,
        kAudioObjectPropertyScopeWildcard,
        kAudioObjectPropertyElementWildcard
      };

    // install a stream property listener
    AudioObjectAddPropertyListener(streamIdArray[i], &listenerAddr, cmApOsxSystemStreamPropertyListenerProc, drp );


    pktArray[i].devIdx         = devIdx;
    pktArray[i].begChIdx       = chIdx;
    pktArray[i].chCnt          = ablp->mBuffers[i].mNumberChannels;
    pktArray[i].audioFramesCnt = ablp->mBuffers[i].mDataByteSize / (sizeof(cmApSample_t) * ablp->mBuffers[i].mNumberChannels); //sdr.mFramesPerPacket;
    pktArray[i].flags          = kInterleavedApFl | kFloatApFl;
    pktArray[i].bitsPerSample  = sizeof(cmApSample_t) * 8;
    pktArray[i].audioBytesPtr  = NULL; 
    pktArray[i].userCbPtr      = NULL; // the userCbPtr value isn't availabe until the cmApOsxDeviceSetup()

    // verify that all buffers on this device have the same size
    assert(i==0 || pktArray[i].audioFramesCnt == pktArray[i-1].audioFramesCnt );

    chIdx +=  ablp->mBuffers[i].mNumberChannels;

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


OSStatus _cmApOsxSetSampleRate( cmApOsxDevRecd* drp, bool inputFl, double newSampleRate )
{
	OSStatus 			 err              = noErr;
	unsigned			 waitMs           = 0;
	AudioStreamID* streamIdArray    = NULL;
  AudioObjectPropertyAddress addr = 
    { 
      kAudioDevicePropertyStreamConfiguration,
      inputFl ? kAudioDevicePropertyScopeInput :  kAudioObjectPropertyScopeOutput, 
      kAudioObjectPropertyElementMaster 
    };

  unsigned       streamCnt;
	unsigned       i;

  // allocate a stream id array
  if((err = _cmApOsxAllocateStreamIdArray( drp->devId, inputFl, &streamIdArray, &streamCnt )) != noErr )
    return _cmApOsxOsError(err,&_cmApOsxRoot);
		
	// 	for each stream on this device
	for(i=0; i<streamCnt; i++)
	{
    UInt32                      byteCnt    = 0;
		AudioStreamBasicDescription sdr;

    // get the size of the stream desc recd
    addr.mSelector = kAudioDevicePropertyStreamFormat;
    if((err = AudioObjectGetPropertyDataSize(drp->devId, &addr, 0, NULL, &byteCnt)) != noErr ) 
    {
      _cmApOsxOsError(err,&_cmApOsxRoot);
      goto errLabel;
    }

    assert( byteCnt == sizeof(sdr) );
	
    // get the stream desc recd
    if((err=AudioObjectGetPropertyData(drp->devId, &addr, 0, NULL, &byteCnt, &sdr)) != noErr ) 
    { 
      _cmApOsxOsError(err,&_cmApOsxRoot);
      goto errLabel;
    }

		// if the format has not already been set
		if( sdr.mSampleRate != newSampleRate )
		{	

      //printf("Changing %s stream %i sample rate from %f to %f.\n",(inputFl?"input":"output"),i,sdr.mSampleRate,newSampleRate);

			// change the sample rate
      sdr.mSampleRate = newSampleRate;

      drp->fmtChangeDevId = -1;
      drp->fmtChangeObjId	= -1;

			// attempt to change the sample rate
      addr.mSelector = kAudioDevicePropertyStreamFormat;
      if((err = AudioObjectSetPropertyData( streamIdArray[i], &addr, 0, NULL, sizeof(sdr), &sdr)) != noErr )
      {
				err = _cmApOsxOsError(err,&_cmApOsxRoot);
				goto errLabel;
      } 
				
			// wait for confirmation
			waitMs = 0;
			while( 	 drp->fmtChangeDevId != drp->devId 
        && 	drp->fmtChangeObjId    != streamIdArray[i] 
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
        addr.mSelector = kAudioDevicePropertyStreamFormat;
        if((err=AudioObjectGetPropertyData(drp->devId, &addr, 0, NULL, &byteCnt, &sdr)) != noErr ) 
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
  AudioValueRange r;
  UInt32         outByteCnt        = sizeof(r);
  UInt32         curFramesPerCycle = 0;
  AudioObjectPropertyAddress addr = 
    { 
      kAudioDevicePropertyBufferFrameSizeRange,
      inputFl ? kAudioDevicePropertyScopeInput :  kAudioObjectPropertyScopeOutput, 
      kAudioObjectPropertyElementMaster 
    };


  // get the frame size range
  if ((err=AudioObjectGetPropertyData(drp->devId, &addr, 0, NULL, &outByteCnt, &r)) != noErr ) 
    return _cmApOsxOsError(err,&_cmApOsxRoot);

  // verify that the requested frame size is within the acceptable frame size range
  if( newFramesPerCycle<r.mMinimum || r.mMaximum < newFramesPerCycle )
    return _cmApOsxOsError(err,&_cmApOsxRoot);


  addr.mSelector = kAudioDevicePropertyBufferFrameSize;
  outByteCnt        = sizeof(UInt32);

	// get the cur value off the param. to change
  if ((err=AudioObjectGetPropertyData(drp->devId, &addr, 0, NULL, &outByteCnt, &curFramesPerCycle)) != noErr ) 
    return _cmApOsxOsError(err,&_cmApOsxRoot);


	// if the cur value is the same as the new value then there is nothing to do
	if( curFramesPerCycle == newFramesPerCycle )
		return noErr;

  //printf("Changing %s frames per cycle from %i to %i.\n",(inputFl?"input":"output"),curFramesPerCycle,newFramesPerCycle);

	// attempt to set the new value	
  if((err = AudioObjectSetPropertyData( drp->devId, &addr, 0, NULL, sizeof(newFramesPerCycle), &newFramesPerCycle)) != noErr )
    return _cmApOsxOsError(err,&_cmApOsxRoot);

	// wait for the value to actually change
	unsigned waitMs = 0;
	while( waitMs < drp->timeOutMs )
	{
		const unsigned waitIncrMs = 20;		
		usleep(waitIncrMs*1000);
		
		// read the parameter value
    outByteCnt = sizeof(curFramesPerCycle);
    if ((err=AudioObjectGetPropertyData(drp->devId, &addr, 0, NULL, &outByteCnt, &curFramesPerCycle)) != noErr ) 
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


cmApRC_t      cmApOsxInitialize( cmRpt_t* rpt, unsigned baseApDevIdx )
{
  
  cmApRC_t        rc                     = kOkApRC;
  cmApOsxRoot*    p                      = &_cmApOsxRoot;
  UInt32          outByteCnt             = 0;
  OSStatus        err                    = noErr;
  AudioDeviceID*  devIdArray             = NULL;
  unsigned        i                      = 0;
  AudioObjectPropertyAddress thePropAddr = 
    { 
      kAudioHardwarePropertyDevices, 
      kAudioObjectPropertyScopeGlobal, 
      kAudioObjectPropertyElementMaster 
    };

  if((rc = cmApOsxFinalize()) != kOkApRC )
    return rc;

  p->rpt      = rpt;
  p->devArray = NULL;

  // get the size of the device ID array in bytes
  if((err = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &thePropAddr, 0, NULL, &outByteCnt)) != noErr ) 
    return _cmApOsxOsError(err,&_cmApOsxRoot);
    
  assert( outByteCnt > 0 );
  
  // calc. the device count
  p->devCnt  = outByteCnt / sizeof(AudioDeviceID);

  assert( p->devCnt*sizeof(AudioDeviceID) == outByteCnt );
		
  // allocate space for the device id array and the device array
  devIdArray   = cmMemAllocZ( AudioDeviceID,   p->devCnt );	
  p->devArray  = cmMemAllocZ( cmApOsxDevRecd,  p->devCnt );

  // get the deviceID array into devIdArray[ devCnt ]
  if ((err=AudioObjectGetPropertyData(kAudioObjectSystemObject, &thePropAddr, 0, NULL, &outByteCnt, devIdArray)) != noErr ) 
  { 
    rc = _cmApOsxOsError(err,&_cmApOsxRoot);
    goto errLabel;
  }

  // for each device
  for(i=0; i<p->devCnt; ++i)
  {
    
    // get device name
    if(( err = _cmApOsxAllocPropertyCFString(devIdArray[i],kAudioObjectPropertyName,kAudioObjectPropertyScopeGlobal,kAudioObjectPropertyElementMaster,&p->devArray[i].nameStr)) != noErr)
    {
      rc = _cmApOsxOsError(err,&_cmApOsxRoot);
      goto errLabel;      
    }

    // get manufacturer
    if(( rc = _cmApOsxAllocPropertyCFString(devIdArray[i],kAudioObjectPropertyManufacturer,kAudioObjectPropertyScopeGlobal,kAudioObjectPropertyElementMaster,&p->devArray[i].mfgStr)) != noErr)
      goto errLabel;      

    if(( rc = _cmApOsxGetBufferConfig(i, devIdArray[i], true, p->devArray + i )) != kOkApRC )
      goto errLabel;      

    if(( rc = _cmApOsxGetBufferConfig(i, devIdArray[i], false, p->devArray + i )) != kOkApRC )
      goto errLabel;      

    //printf("%s %s\n",p->devArray[i].mfgStr,p->devArray[i].nameStr);
    p->devArray[i].devId           = devIdArray[i];
    p->devArray[i].devIdx          = i;
    p->devArray[i].callbackPtr     = NULL;
    p->devArray[i].timeOutMs       = 1000;
    p->devArray[i].ioProcId        = NULL;

  }

 errLabel:

  cmMemFree(devIdArray);

  if( rc != noErr )
    cmApOsxFinalize();
  return rc;
}

cmApRC_t cmApOsxFinalize()
{
  cmApRC_t     rc = kOkApRC;
  cmApOsxRoot* p  = &_cmApOsxRoot;
  unsigned     i;
  OSStatus     err;

  for(i=0; i<p->devCnt; ++i)
  {
    cmApOsxDevRecd* drp = p->devArray + i;


    unsigned       j;

    for(j=0; j<2; ++j)
    { 
      AudioStreamID* streamIdArray;
      unsigned       streamCnt;
      unsigned       k;

      if((err = _cmApOsxAllocateStreamIdArray(drp->devId, j==0, &streamIdArray,&streamCnt )) != noErr )
        _cmApOsxOsError(err,p);

      for(k=0; k<streamCnt; ++k)
      {
        AudioObjectPropertyAddress listenerAddr = 
          { 
            kAudioObjectPropertySelectorWildcard,
            kAudioObjectPropertyScopeWildcard,
            kAudioObjectPropertyElementWildcard
          };
        
        // install a stream property listener
        AudioObjectRemovePropertyListener(streamIdArray[i], &listenerAddr, cmApOsxSystemStreamPropertyListenerProc, drp );

      }
        
      cmMemPtrFree(&streamIdArray);
        
    }  

    if( drp->ioProcId != NULL )
      if((err = AudioDeviceDestroyIOProcID( drp->devId, drp->ioProcId )) != noErr )
        _cmApOsxOsError(err,p);
    

    cmMemFree(drp->nameStr);
    cmMemFree(drp->mfgStr);
    cmMemFree(drp->inPktArray);
    cmMemFree(drp->outPktArray);
  }

  cmMemFree(p->devArray);
  return rc;
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

  cmApOsxRoot*    p   = &_cmApOsxRoot;
  cmApOsxDevRecd* drp = _cmApOsxRoot.devArray + devIdx;
  unsigned        j;
  OSStatus        err;
  
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
      _cmApOsxOsError(err,p);


  // set the io proc 			
  drp->ioProcId = NULL;

  if( (err = AudioDeviceCreateIOProcID(drp->devId,_cmApOsxSystemDeviceIOProc,(void*)drp,&drp->ioProcId) ) != noErr )
  {
    _cmApOsxOsError(err,p);
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
		 
  if( (err = AudioDeviceStop(_cmApOsxRoot.devArray[devIdx].devId,_cmApOsxRoot.devArray[devIdx].ioProcId)) != noErr )
	{
		_cmApOsxOsError(err,&_cmApOsxRoot);	
		return kSysErrApRC;
	}

  return kOkApRC;
}

bool  cmApOsxDeviceIsStarted( unsigned devIdx )
{
  assert( devIdx < _cmApOsxRoot.devCnt );
  return false;
}


void cmApOsxReport( cmRpt_t* rpt )
{
  cmApOsxRoot* p  = &_cmApOsxRoot;
  unsigned     i;

  for(i=0; i<p->devCnt; ++i)
  {
    cmApOsxDevRecd* drp = p->devArray + i;
    
    cmRptPrintf(rpt,"in ch:%2i | out ch:%2i | started:%1i | sr:%7.1f  %s %s\n",
      drp->inChCnt,
      drp->outChCnt,
      drp->srate,
      cmApOsxDeviceIsStarted( i ),
      cmStringNullGuard(drp->mfgStr),
      cmStringNullGuard(drp->nameStr));
  }
  
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


void cmApOsxTest( cmRpt_t* rpt )
{
  printf("Start\n");
  cmApOsxInitialize(rpt,0);

  cmApOsxReport(rpt);

  if( cmApOsxDeviceSetup(2,48000.0,1024,NULL,NULL) != kOkApRC )
    printf("Setup failed.\n");

  cmApOsxReport(rpt);

  cmApOsxFinalize();
  printf("Finish\n");
}


