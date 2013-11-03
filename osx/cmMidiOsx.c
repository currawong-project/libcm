#include <Carbon/Carbon.h>
#include <CoreAudio/CoreAudio.h>
#include <CoreMIDI/CoreMIDI.h>
#include <mach/mach_time.h>  

#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmMidi.h"
#include "cmMidiPort.h"

typedef struct
{
  bool            inputFl;
  char*           nameStr;
  SInt32          uid;
  MIDIEndpointRef epr;
  cmMpParserH_t   parserH;
  double          prevMicroSecs;
} cmMpPort;

typedef struct
{
  char*      nameStr;

  unsigned   iPortCnt;
  cmMpPort*  iPortArray;

  unsigned   oPortCnt;
  cmMpPort*  oPortArray;
} cmMpDev;

typedef struct
{
  cmErr_t       err;
  
  unsigned      devCnt;
  cmMpDev*      devArray;

	cmMpCallback_t cbFunc;
  void*          cbDataPtr;

  MIDIClientRef	clientRef;
  MIDIPortRef		inPortRef;
  MIDIPortRef		outPortRef; 

  mach_timebase_info_data_t timeBaseInfo; 

  
} cmMpRoot;

cmMpRoot _cmMpRoot;
cmMpRoot* _cmMpRtPtr = NULL;

cmMpRC_t _cmMpError( cmErr_t* errPtr, cmMpRC_t rc, OSStatus err, const char* fmt, ... )
{ 
  va_list vl0;
  va_list vl1;
  
  va_start(vl0,fmt);
  va_copy(vl1,vl0);

  unsigned    n0   = vsnprintf(NULL,0,fmt,vl0);
  unsigned    n1   = 0;
  unsigned    n2   = 0;
  const char* fmt2 = "OS Status:%i";
  va_end(vl0);


  if( err != noErr )
    n1 = snprintf(NULL,0,fmt2,err);

  unsigned bufCharCnt = n0 + n1;
  char     buf[ bufCharCnt + 1];

  vsnprintf(buf,n0,fmt,vl1);
  n2 = strlen(buf);
  snprintf(buf+n2,bufCharCnt-n2,fmt2,err);
  buf[bufCharCnt]=0;

  cmErrMsg(errPtr,rc,buf);
  
  va_end(vl1);

  return rc;
}


char* _cmMpAllocStringProperty( cmErr_t* errPtr, MIDIObjectRef obj, CFStringRef propId )
{
  OSStatus    err;
  CFStringRef cfStrRef  = NULL;
  char*       strPtr    = NULL;
	if(( err = MIDIObjectGetStringProperty(obj,propId,&cfStrRef)) != noErr )
  {
		_cmMpError(errPtr,kCfStringErrMpRC,err,"Get property failed.");
    return NULL;
  }

  unsigned charCnt = CFStringGetLength( cfStrRef ) + 1;

  if( charCnt > 0 )
  {
    strPtr = cmMemAllocZ( char, charCnt );

    if( !CFStringGetCString( cfStrRef, strPtr, charCnt, kCFStringEncodingASCII))
    {
      _cmMpError(errPtr,kCfStringErrMpRC,noErr,"CFstring to cstring failed.");    
      cmMemFree(strPtr);
      strPtr = NULL;
    }
  }
  
  CFRelease(cfStrRef);

  return strPtr;
}


void _cmMpDeviceInit( cmMpDev* drp )
{
  drp->nameStr    = NULL;
  drp->iPortCnt   = 0;
  drp->oPortCnt   = 0;
  drp->iPortArray = NULL;
  drp->oPortArray = NULL;
}

void _cmMpDeviceFree( cmMpDev* drp )
{
  unsigned i;
  for(i=0; i<drp->iPortCnt; ++i)
  {
    cmMemPtrFree(&drp->iPortArray[i].nameStr);
    cmMpParserDestroy( &drp->iPortArray[i].parserH );
  }

  for(i=0; i<drp->oPortCnt; ++i)
    cmMemPtrFree(&drp->oPortArray[i].nameStr);

  cmMemPtrFree(&drp->iPortArray);
  cmMemPtrFree(&drp->oPortArray);
  cmMemPtrFree(&drp->nameStr);
  
  _cmMpDeviceInit(drp);
}

void _cmMpDevicePrint( cmMpDev* drp, unsigned devIdx, cmRpt_t* rpt )
{
  unsigned i,j;
  cmRptPrintf(rpt,"%i '%s'\n",devIdx,drp->nameStr);

  for(j=0; j<2; ++j)
  {
    unsigned portCnt = j==0 ? drp->iPortCnt : drp->oPortCnt;

    for(i=0; i<portCnt; ++i)
    {
      const cmMpPort* p = j==0 ? drp->iPortArray + i : drp->oPortArray + i ;
      cmRptPrintf(rpt,"  port:%s %i id:0x:%x '%s'\n", p->inputFl ? "in " : "out",i, p->uid, p->nameStr );
    }
  }
}

char* _cmMpFormLabel( cmErr_t* errPtr, MIDIObjectRef obj )
{
  char* modelStr = _cmMpAllocStringProperty( errPtr, obj, kMIDIPropertyManufacturer );
  char* nameStr  = _cmMpAllocStringProperty( errPtr, obj, kMIDIPropertyModel );
      
  char* labelStr = cmMemAllocZ( char, strlen(modelStr) + strlen(nameStr) + 4 );
  strcpy(labelStr,modelStr);
  strcat(labelStr," - ");
  strcat(labelStr,nameStr);
  cmMemFree(modelStr);
  cmMemFree(nameStr);

  return labelStr;
}

char* _cmMpFormPortLabel( cmErr_t* errPtr, MIDIEntityRef mer, ItemCount pi, bool inputFl )
{
  MIDIEndpointRef epr;

  if( inputFl )
    epr = MIDIEntityGetSource( mer, pi);
  else 
    epr = MIDIEntityGetDestination( mer, pi); 

  if(epr == 0 )
  {
    _cmMpError(errPtr,kSysErrMpRC,noErr,"Get %s endpoiint ref  %i failed.", inputFl ? "source" : "destination",pi);
    return NULL;
  }

  return _cmMpAllocStringProperty( errPtr, (MIDIObjectRef)epr, kMIDIPropertyName );
}


// Fill id array with the unique id of each endpoint or 0 if the endpoint is offline.
cmMpRC_t _cmMpGetEntityUniqueIdArray( MIDIEntityRef mer, SInt32* idArray, unsigned idCnt, bool inputFl, ItemCount* activeCntPtr, cmErr_t* errPtr )
{
  cmMpRC_t          rc = kOkMpRC;
  MIDIEndpointRef epr;
  unsigned        pi;
  char*           dirLabel = inputFl ? "source" : "dest";
  SInt32          offline;
  OSStatus        err;
 
  *activeCntPtr = 0;  

  for(pi=0; pi<idCnt; ++pi)
  {
    epr = inputFl ? MIDIEntityGetSource( mer, pi) : MIDIEntityGetDestination( mer, pi); 

    if(epr == 0 )
    {
      rc = _cmMpError(errPtr,kSysErrMpRC,noErr,"Get %s endpoiint ref  %i failed.",dirLabel,pi);
      goto errLabel;
    }

    offline = 1;
    if((err = MIDIObjectGetIntegerProperty( epr, kMIDIPropertyOffline, &offline)) != noErr )
    {
       _cmMpError(errPtr,kSysErrMpRC,err,"Get online status on %s endpoint %i failed.",dirLabel,pi);
      //goto errLabel;  kpl 09/05/13 - report error and fall through and report the device as 'offline'. 
      // This should be a warning rather than a error.
    }

    if( offline )
      idArray[pi] = 0;
    else
    {

      ++*activeCntPtr;

      if(( err = MIDIObjectGetIntegerProperty( epr, kMIDIPropertyUniqueID, idArray + pi)) != noErr )
      {
        rc = _cmMpError(errPtr,kSysErrMpRC,err,"Get unique id on %s endpoint %i failed.",dirLabel,pi);
        goto errLabel;
      }
    }
  }

 errLabel:
  return rc;
}


cmMpRC_t _cmMpInitPortArray( unsigned devIdx, MIDIPortRef mpr, MIDIEntityRef mer, cmMpPort* portArray, SInt32* idArray, unsigned portCnt, bool inputFl, cmMpCallback_t cbFunc, void* cbDataPtr, unsigned parserBufByteCnt, cmErr_t* errPtr )
{
  cmMpRC_t   rc = kOkMpRC;
  unsigned pi;

  for(pi=0; pi<portCnt; ++pi)
  {
    MIDIEndpointRef epr = inputFl ? MIDIEntityGetSource(mer,pi) : MIDIEntityGetDestination(mer,pi);

    if( epr == 0 )
      return _cmMpError(errPtr,kSysErrMpRC,noErr,"Get %s endpoiint ref  %i failed.",inputFl ? "source" : "destination", pi);

    // if this is an active input port then connect it to the source port
    if( inputFl && (idArray[pi] != 0) )
    {
      OSStatus        err = noErr;

      if((err = MIDIPortConnectSource( mpr, epr, portArray + pi )) != noErr )
        return _cmMpError(errPtr,kSysErrMpRC,err,"Source connect failed on port:%i",pi);             

      portArray[pi].parserH = cmMpParserCreate( devIdx, pi, cbFunc, cbDataPtr, parserBufByteCnt, errPtr->rpt );
    }
    
    portArray[pi].uid           = idArray[pi];
    portArray[pi].inputFl       = inputFl;
    portArray[pi].nameStr       = _cmMpFormPortLabel(errPtr, mer, pi, inputFl );
    portArray[pi].epr           = epr;
    portArray[pi].prevMicroSecs = 0;
  }

  return rc;
}

cmMpRC_t _cmMpGetActiveEntityPortCount( MIDIEntityRef mer, bool inputFl, ItemCount* portCntPtr, ItemCount* activePortCntPtr, cmErr_t* errPtr  )
{
  *portCntPtr = inputFl  ? MIDIEntityGetNumberOfSources( mer ) : MIDIEntityGetNumberOfDestinations( mer );
  SInt32   idArray[ *portCntPtr ];

  return _cmMpGetEntityUniqueIdArray( mer, idArray, *portCntPtr, inputFl, activePortCntPtr, errPtr );
  
}

cmMpRC_t _cmMpIsDeviceActive( unsigned devIdx, MIDIDeviceRef mdr, ItemCount* srcCntPtr, ItemCount* dstCntPtr, bool* activeFlPtr, cmErr_t* errPtr )
{
  cmMpRC_t    rc      = kOkMpRC;
  ItemCount entityCnt = MIDIDeviceGetNumberOfEntities(mdr);
  unsigned ei;

  *srcCntPtr   = 0;
  *dstCntPtr   = 0;
  *activeFlPtr = false;

  for(ei=0; ei<entityCnt; ++ei)
  {
    ItemCount     srcCnt    = 0;
    ItemCount     dstCnt    = 0;
    ItemCount     activeCnt = 0;
    MIDIEntityRef mer;

    if((mer = MIDIDeviceGetEntity( mdr, ei)) == 0 )
      return _cmMpError( errPtr,kSysErrMpRC,noErr,"Get midi device %i entity %i failed.",devIdx,ei);

    if((rc= _cmMpGetActiveEntityPortCount(mer,true,&srcCnt,&activeCnt,errPtr)) != kOkMpRC )
      return _cmMpError( errPtr, rc, noErr, "Error retrieving source port information for device:%i entity:%i",devIdx,ei);

    *srcCntPtr += srcCnt;

    if( activeCnt )
      *activeFlPtr = true;

    activeCnt = 0;

    if((rc= _cmMpGetActiveEntityPortCount(mer,false,&dstCnt,&activeCnt,errPtr)) != kOkMpRC )
      return _cmMpError( errPtr, rc, noErr, "Error retrieving destination port information for device:%i entity:%i",devIdx,ei);
    
    *dstCntPtr += dstCnt;

    if( activeCnt )
      *activeFlPtr = true;    

  }

  return rc;
}

cmMpRC_t _cmMpCreateDevice( unsigned devIdx, cmMpDev* drp,  MIDIPortRef inPortRef, cmMpCallback_t cbFunc, void* cbDataPtr, unsigned parserBufByteCnt, cmErr_t* errPtr )
{
  cmMpRC_t        rc        = kOkMpRC;
  MIDIDeviceRef mdr;
  ItemCount     devSrcCnt = 0;
  ItemCount     devDstCnt = 0;
  bool          activeFl  = false;
  unsigned      ei;

  // zero the device record
  _cmMpDeviceInit(drp);

  // get the device ref
  if((mdr = MIDIGetDevice(devIdx)) == 0 )
    return _cmMpError(errPtr,kSysErrMpRC,noErr,"Get midi device %i failed.",devIdx);
   
  // determine if the device port count and whether it is active
  if((rc = _cmMpIsDeviceActive(devIdx, mdr, &devSrcCnt, &devDstCnt, &activeFl, errPtr )) != kOkMpRC )
    return rc;

  // if the device is not active return with the device record empty
  if( !activeFl)
    return kOkMpRC;

  // allocate the input port array
  drp->iPortCnt   = devSrcCnt;
  drp->iPortArray = cmMemAllocZ( cmMpPort, devSrcCnt );

  // allocate the output port array
  drp->oPortCnt   = devDstCnt;
  drp->oPortArray = cmMemAllocZ( cmMpPort, devDstCnt );

  // form the device name string
  drp->nameStr = _cmMpFormLabel( errPtr, (MIDIObjectRef)mdr);


  unsigned entityCnt = MIDIDeviceGetNumberOfEntities(mdr);
  unsigned iPortCnt  = 0;
  unsigned oPortCnt  = 0;

  // for each entity on this device
  for(ei=0; ei<entityCnt; ++ei)
  {
    MIDIEntityRef   mer;
    unsigned        i;

    // get the entity reference
    if((mer = MIDIDeviceGetEntity( mdr, ei)) == 0 )
    {
      rc = _cmMpError(errPtr,kSysErrMpRC,noErr,"Get midi device %i entity %i failed.",devIdx,ei);
      goto errLabel;
    }

    // for each input and output on this entity
    for( i=0; i<2; ++i)
    {
      bool      inputFl       = i==0;
      ItemCount activeCnt     = 0;
      unsigned  portCnt       = inputFl ? iPortCnt : oPortCnt;
      cmMpPort*   portArray     = inputFl ? drp->iPortArray : drp->oPortArray;
      ItemCount entityPortCnt = inputFl ? MIDIEntityGetNumberOfSources( mer ) : MIDIEntityGetNumberOfDestinations( mer );
      SInt32    idArray[ entityPortCnt ];

    
      // fill idArray with the unique id's of the active ports on this entity
      if((rc = _cmMpGetEntityUniqueIdArray(mer,idArray,entityPortCnt,inputFl,&activeCnt,errPtr)) != kOkMpRC )
      {
        rc = _cmMpError(errPtr,rc,noErr,"Unable to locate unique source ids on device:%i entity:%i",devIdx,ei);
        goto errLabel;
      }

      // assign the unique id's to each cmMpPort assoc'd with this entity
      _cmMpInitPortArray(devIdx, inPortRef, mer, portArray + portCnt, idArray, entityPortCnt, inputFl, cbFunc, cbDataPtr, parserBufByteCnt, errPtr );

      if( inputFl )
        iPortCnt += entityPortCnt;
      else
        oPortCnt += entityPortCnt;

    }
  }

  assert( iPortCnt == devSrcCnt );
  assert( oPortCnt == devDstCnt );

 errLabel:
  if( rc != kOkMpRC )
    _cmMpDeviceFree(drp);
  

  return rc;
}


void _cmMpDestroyDeviceArray( cmMpRoot* rp )
{
  unsigned i;
  for(i=0; i<rp->devCnt; ++i)
    _cmMpDeviceFree( rp->devArray + i );
  
  cmMemPtrFree(&rp->devArray);
  rp->devCnt = 0;
}

cmMpRC_t _cmMpCreateDeviceArray( cmMpRoot* rp, unsigned parserBufByteCnt )
{
  cmMpRC_t     rc          = kOkMpRC;
  ItemCount  devCnt      = MIDIGetNumberOfDevices();
  ItemCount  di          = 0;

  rp->devCnt   = 0;
  rp->devArray = cmMemAllocZ( cmMpDev, devCnt );
  
  for(di=0; di<devCnt; ++di)
  {
    cmMpDev* drp = rp->devArray + rp->devCnt;

    // attempt to create a device record for device di
    if((rc = _cmMpCreateDevice(  di, drp, rp->inPortRef, rp->cbFunc, rp->cbDataPtr, parserBufByteCnt, &rp->err )) != kOkMpRC )
      goto errLabel;

    // if the device di is active 
    if( drp->iPortCnt || drp->oPortCnt )
      ++rp->devCnt;
    
  }

 errLabel:
  
  if( rc != kOkMpRC )
    _cmMpDestroyDeviceArray(rp);

  return rc;
}



void _cmMpMidiSystemNotifyProc( const MIDINotification* notifyMsgPtr, void *refCon )
{
  // cmMpRoot* p = (mdRoot*)refCon;
  //	if(p != NULL )
	//	((fw::osxMidiPorts*)refCon)->onNotify(notifyMsgPtr);
}

void _cmMpMIDISystemReadProc( const MIDIPacketList *pktListPtr, void* readProcRefCon, void* srcConnRefCon )
{
  //cmMpRoot*  rp = (cmMpRoot*)readProcRefCon;
  cmMpPort*  pp = (cmMpPort*)srcConnRefCon;
  unsigned i;


  if( pktListPtr == NULL /*|| rp == NULL*/ || pp == NULL /*|| rp->cbFunc == NULL*/ )
    return;

	const MIDIPacket* packetPtr = &pktListPtr->packet[0];

	for(i=0; i < pktListPtr->numPackets; ++i) 
	{
    /*
      unsigned j;
    for(j=0; j<packetPtr->length; ++j)
      printf("0x%x ", packetPtr->data[j]);

    printf("\n");
    */

    // About converting mach_absolute_time() to seconds: http://lists.apple.com/archives/carbon-dev/2007/Jan//msg00282.html
    // About MIDIPacket.timeStamp: http://developer.apple.com/library/ios/#documentation/CoreMidi/Reference/MIDIServices_Reference/Reference/reference.html


    double nano = 1e-9 * ( (double) _cmMpRoot.timeBaseInfo.numer) / ((double) _cmMpRoot.timeBaseInfo.denom);

    // so here's the delta in nanoseconds:
    double nanoSeconds = ((double) packetPtr->timeStamp) * nano;

    // 1000 times that for microSeconds:
    double microSecs = 1000.0f * nanoSeconds;

    double deltaMicroSecs = microSecs - pp->prevMicroSecs;

    pp->prevMicroSecs = microSecs;

    assert( pp->inputFl == true );
    
    cmMpParseMidiData( pp->parserH, (unsigned)deltaMicroSecs, packetPtr->data, packetPtr->length );

		packetPtr = MIDIPacketNext(packetPtr);
	}

}

cmMpPort* _cmMpGetPort( cmMpDev* drp, unsigned portIdx, unsigned flags )
{
  assert( flags != 0 );

  bool inputFl = cmIsFlag(flags, kInMpFl);
  
  assert( portIdx < (inputFl ? drp->iPortCnt : drp->oPortCnt));

  return inputFl ? drp->iPortArray + portIdx : drp->oPortArray + portIdx;
}

cmMpRC_t cmMpInitialize( cmCtx_t* c, cmMpCallback_t cbFunc, void* cbDataPtr, unsigned parserBufByteCnt, const char* appNameStr )
{
  cmMpRC_t                  rc                = kOkMpRC;
	OSStatus                  err               = noErr;
	CFStringRef               clientNameStrRef 	= NULL;
	CFStringRef               inPortNameStrRef	= NULL;
	CFStringRef               outPortNameStrRef	= NULL;
  cmRpt_t* rpt = &c->rpt;

  if( _cmMpRtPtr == NULL )
  {
    memset(&_cmMpRoot,0,sizeof(_cmMpRoot));
    _cmMpRtPtr = &_cmMpRoot;
  }

  if((rc = cmMpFinalize()) != kOkMpRC )
    return rc;

  cmErrSetup(&_cmMpRoot.err,rpt,"MIDI Port");

  _cmMpRoot.cbFunc            = cbFunc;
  _cmMpRoot.cbDataPtr         = cbDataPtr;

	if(( clientNameStrRef = CFStringCreateWithCString(NULL,"MIDI",kCFStringEncodingASCII)) == NULL )
	{
		rc = _cmMpError(&_cmMpRoot.err,kCfStringErrMpRC,noErr,"Client name create failed.");
		goto errLabel;
	}
	
	if(( err = MIDIClientCreate( clientNameStrRef, _cmMpMidiSystemNotifyProc, &_cmMpRoot, &_cmMpRoot.clientRef )) != noErr )
	{
		rc = _cmMpError(&_cmMpRoot.err,kSysErrMpRC,err,"Client create failed.");
		goto errLabel;
	}
			
	if((inPortNameStrRef = CFStringCreateWithCString(NULL,"in",kCFStringEncodingASCII)) == NULL )
	{
		rc = _cmMpError(&_cmMpRoot.err,kCfStringErrMpRC,noErr,"In port name string create failed.");
		goto errLabel;
	}
	
	if((err = MIDIInputPortCreate( _cmMpRoot.clientRef, inPortNameStrRef, _cmMpMIDISystemReadProc, &_cmMpRoot, &_cmMpRoot.inPortRef))	!= noErr )
	{
		rc = _cmMpError(&_cmMpRoot.err,kSysErrMpRC,err,"In port create failed.");
		goto errLabel;
	}
		
	if((outPortNameStrRef = CFStringCreateWithCString(NULL,"out",kCFStringEncodingASCII)) == NULL )
	{
		rc = _cmMpError(&_cmMpRoot.err,kCfStringErrMpRC,noErr,"Out port name string create failed.");
		goto errLabel;
	}
	
	if((err = MIDIOutputPortCreate( _cmMpRoot.clientRef, outPortNameStrRef, &_cmMpRoot.outPortRef))	!= noErr )
	{
		rc = _cmMpError(&_cmMpRoot.err,kSysErrMpRC,err,"Out port create failed.");
		goto errLabel;
	}

  if((rc = _cmMpCreateDeviceArray(&_cmMpRoot,parserBufByteCnt)) != kOkMpRC )
    goto errLabel;


  mach_timebase_info(&_cmMpRoot.timeBaseInfo);

	errLabel:
		if( clientNameStrRef != NULL )
			CFRelease(clientNameStrRef);
			
		if( inPortNameStrRef != NULL )
			CFRelease(inPortNameStrRef);
			
		if( outPortNameStrRef != NULL )
			CFRelease(outPortNameStrRef);
			
    if( rc != kOkMpRC )
      cmMpFinalize();

    return rc;
  
}

cmMpRC_t cmMpFinalize()
{
  OSStatus err = noErr;
  cmMpRC_t   rc  = kOkMpRC;

  if( _cmMpRoot.inPortRef != 0 )
  {
    if((err = MIDIPortDispose(_cmMpRoot.inPortRef)) != noErr )
      rc = _cmMpError(&_cmMpRoot.err,kSysErrMpRC,err,"MIDIPortDispose() failed on the input port.");
    else
      _cmMpRoot.inPortRef = 0;
  }

  if( _cmMpRoot.outPortRef !=  0 )
  {
    if((err = MIDIPortDispose(_cmMpRoot.outPortRef)) != noErr )
      rc = _cmMpError(&_cmMpRoot.err,kSysErrMpRC,err,"MIDIPortDispose() failed on the output port.");
    else
      _cmMpRoot.outPortRef = 0;
  }

  if( _cmMpRoot.clientRef != 0 )
  {
    if((err = MIDIClientDispose(_cmMpRoot.clientRef)) != noErr )
      rc = _cmMpError(&_cmMpRoot.err,kSysErrMpRC,err,"MIDIClientDispose() failed.");
    else
      _cmMpRoot.clientRef = 0;
  }

   _cmMpDestroyDeviceArray(&_cmMpRoot);

   _cmMpRoot.devCnt     = 0;
   _cmMpRoot.devArray   = NULL;
   _cmMpRoot.cbFunc     = NULL;
   _cmMpRoot.cbDataPtr  = NULL;
   _cmMpRoot.clientRef  = 0;
   _cmMpRoot.inPortRef  = 0;
   _cmMpRoot.outPortRef = 0;


  return rc;
}

bool        cmMpIsInitialized()
{ return _cmMpRoot.clientRef != 0; }

unsigned      cmMpDeviceCount()
{
  return _cmMpRoot.devCnt;
}

const char* cmMpDeviceName(       unsigned devIdx )
{
  assert( devIdx < _cmMpRoot.devCnt );
  return _cmMpRoot.devArray[devIdx].nameStr;
}

unsigned    cmMpDevicePortCount(  unsigned devIdx, unsigned flags )
{
  assert( devIdx < _cmMpRoot.devCnt );
  return cmIsFlag(flags,kInMpFl) ? _cmMpRoot.devArray[devIdx].iPortCnt : _cmMpRoot.devArray[devIdx].oPortCnt;
}

const char*    cmMpDevicePortName(   unsigned devIdx, unsigned flags, unsigned portIdx )
{
  assert( devIdx < _cmMpRoot.devCnt );

  cmMpDev* drp = _cmMpRoot.devArray + devIdx;

  assert( portIdx < (cmIsFlag(flags,kInMpFl) ? drp->iPortCnt : drp->oPortCnt));

  return _cmMpGetPort( drp, portIdx, flags )->nameStr;

}


cmMpRC_t  cmMpDeviceSend( unsigned devIdx, unsigned portIdx, cmMidiByte_t status, cmMidiByte_t d0, cmMidiByte_t d1 )
{
  assert( devIdx < _cmMpRoot.devCnt );

  cmMpDev*        drp     = _cmMpRoot.devArray + devIdx;
  cmMpPort*       pp      = _cmMpGetPort( drp, portIdx, kOutMpFl);
	OSStatus        err     = noErr;
	MIDIPacketList 	mpl;
  unsigned        byteCnt = cmMidiStatusToByteCount(status & 0xf0 );

  if( byteCnt == 0 )
    return _cmMpError(&_cmMpRoot.err,kInvalidArgMpRC,noErr,"Invalid status byte (0x%x) on send to device:%i port:%i",status,devIdx,portIdx);

	mpl.numPackets	= 1;
	
	mpl.packet[0].timeStamp 	= 0;
	mpl.packet[0].length		= byteCnt+1;
	mpl.packet[0].data[0]		= status;
	mpl.packet[0].data[1]		= d0;
	mpl.packet[0].data[2]		= d1;

  //printf("%i %i %i %i\n",status,d0,d1,byteCnt);

	if(( err = MIDISend( _cmMpRoot.outPortRef, pp->epr, &mpl)) != noErr )
		return _cmMpError(&_cmMpRoot.err,kSysErrMpRC,err,"Send on device:%i port:%i failed.",devIdx,portIdx);
	
	return kOkMpRC;
}

cmMpRC_t      cmMpDeviceSendData( unsigned devIdx, unsigned portIdx, const cmMidiByte_t* dataPtr, unsigned byteCnt )
{
  assert( devIdx < _cmMpRoot.devCnt );

  cmMpDev*        drp     = _cmMpRoot.devArray + devIdx;
  cmMpPort*       pp      = _cmMpGetPort( drp, portIdx, kOutMpFl);
	OSStatus        err     = noErr;
	MIDIPacketList 	mpl;
	unsigned        i;

  if( byteCnt > 256 )
    return _cmMpError(&_cmMpRoot.err,kInvalidArgMpRC,noErr,"mdDeviceSendData() only support data buffers up to 256 bytes.");

	
	mpl.numPackets	= 1;
	
	mpl.packet[0].timeStamp = 0;
	mpl.packet[0].length		= byteCnt;

	for(i=0; i<byteCnt; i++)
		mpl.packet[0].data[i] = dataPtr[i];
	
	if(( err = MIDISend( _cmMpRoot.outPortRef, pp->epr, &mpl)) != noErr )
		return _cmMpError(&_cmMpRoot.err,kSysErrMpRC,err,"Data Send on device:%i port:%i failed.",devIdx,portIdx);
	
	return kOkMpRC;

}

cmMpRC_t    cmMpInstallCallback( unsigned devIdx, unsigned portIdx, cmMpCallback_t cbFunc, void* cbDataPtr )
{
  cmMpRC_t rc = kOkMpRC;
  unsigned di;
  unsigned dn = cmMpDeviceCount();

  for(di=0; di<dn; ++di)
    if( di==devIdx || devIdx == -1 )
    {
      unsigned pi;
      unsigned pn = cmMpDevicePortCount(di,kInMpFl);

      for(pi=0; pi<pn; ++pi)
        if( pi==portIdx || portIdx == -1 )
            if( cmMpParserInstallCallback( _cmMpRoot.devArray[di].iPortArray[pi].parserH, cbFunc, cbDataPtr ) != kOkMpRC )
              goto errLabel;
    }

 errLabel:
  return rc;
}

cmMpRC_t    cmMpRemoveCallback(  unsigned devIdx, unsigned portIdx, cmMpCallback_t cbFunc, void* cbDataPtr )
{
  cmMpRC_t rc     = kOkMpRC;
  unsigned di;
  unsigned dn     = cmMpDeviceCount();
  unsigned remCnt = 0;

  for(di=0; di<dn; ++di)
    if( di==devIdx || devIdx == -1 )
    {
      unsigned pi;
      unsigned pn = cmMpDevicePortCount(di,kInMpFl);

      for(pi=0; pi<pn; ++pi)
        if( pi==portIdx || portIdx == -1 )
          if( cmMpParserHasCallback(  _cmMpRoot.devArray[di].iPortArray[pi].parserH, cbFunc, cbDataPtr ) )
          {
            if( cmMpParserRemoveCallback( _cmMpRoot.devArray[di].iPortArray[pi].parserH, cbFunc, cbDataPtr ) != kOkMpRC )
              goto errLabel;
            else
              ++remCnt;
          }
    }

  if( remCnt == 0 && dn > 0 )
    rc =  _cmMpError(&_cmMpRoot.err,kCbNotFoundMpRC,0,"The callback was not found on any of the specified devices or ports.");

 errLabel:
  return rc;
}

bool        cmMpUsesCallback(    unsigned devIdx, unsigned portIdx, cmMpCallback_t cbFunc, void* cbDataPtr )
{
  unsigned di;
  unsigned dn     = cmMpDeviceCount();

  for(di=0; di<dn; ++di)
    if( di==devIdx || devIdx == -1 )
    {
      unsigned pi;
      unsigned pn = cmMpDevicePortCount(di,kInMpFl);

      for(pi=0; pi<pn; ++pi)
        if( pi==portIdx || portIdx == -1 )
          if( cmMpParserHasCallback(  _cmMpRoot.devArray[di].iPortArray[pi].parserH, cbFunc, cbDataPtr ) )
            return true;
    }

  return false;
}

void cmMpReport( cmRpt_t* rpt )
{
  unsigned i;
  for(i=0; i<_cmMpRoot.devCnt; ++i)
    _cmMpDevicePrint(_cmMpRoot.devArray + i, i, rpt );
}


