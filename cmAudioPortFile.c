#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmAudioPort.h"
#include "cmAudioPortFile.h"
#include "cmAudioFileDev.h"

typedef struct
{
  cmAfdH_t devH;
} cmApDev_t;

typedef struct
{
  cmErr_t    err;
  cmApDev_t* devArray;
  unsigned   devCnt;
} cmApf_t;

cmApf_t* _cmApf = NULL;

cmApRC_t      cmApFileInitialize( cmRpt_t* rpt, unsigned baseApDevIdx )
{
  cmApRC_t rc;
  if((rc = cmApFileFinalize()) != kOkApRC )
    return rc;

  _cmApf = cmMemAllocZ(cmApf_t,1);

  cmErrSetup(&_cmApf->err,rpt,"Audio Port File");

  return rc;      
}

cmApRC_t      cmApFileFinalize()
{
  cmApRC_t rc = kOkApRC;

  if( _cmApf == NULL )
    return kOkApRC;

  unsigned i;
  for(i=0; i<_cmApf->devCnt; ++i)
  {
    cmApRC_t rc0;
    if((rc0 = cmApFileDeviceDestroy(i)) != kOkApRC )
      rc = rc0;
  }

  cmMemPtrFree(&_cmApf->devArray);
  _cmApf->devCnt = 0;

  cmMemPtrFree(&_cmApf);

  return rc;
}

unsigned      cmApFileDeviceCreate( 
  const cmChar_t* devLabel,
  const cmChar_t* iFn,
  const cmChar_t* oFn,
  unsigned        oBits,
  unsigned        oChCnt )
{
  unsigned i;

  // find an available device slot
  for(i=0; i<_cmApf->devCnt; ++i)
    if( cmAudioFileDevIsValid( _cmApf->devArray[i].devH ) == false )
      break;

  // if no device slot is availd ...
  if( i == _cmApf->devCnt )
  {
    // ... create a new one
    _cmApf->devArray = cmMemResizePZ(cmApDev_t, _cmApf->devArray, _cmApf->devCnt+1);
    ++_cmApf->devCnt;
  }

  // open the file device
  if( cmAudioFileDevInitialize( &_cmApf->devArray[i].devH, devLabel, i, iFn, oFn, oBits, oChCnt, _cmApf->err.rpt ) != kOkAfdRC )
  {
    cmErrMsg(&_cmApf->err,kAudioPortFileFailApRC,"The audio file device initialization failed.");
    i = cmInvalidIdx;
  }
  
  return i;
}

cmApRC_t      cmApFileDeviceDestroy( unsigned devIdx )
{
  if( cmAudioFileDevFinalize( &_cmApf->devArray[devIdx].devH ) != kOkAfdRC )
    return cmErrMsg(&_cmApf->err,kAudioPortFileFailApRC,"The audio file device finalize failed.");

  return kOkApRC;
}

unsigned      cmApFileDeviceCount()
{ return _cmApf->devCnt; }

const char*   cmApFileDeviceLabel( unsigned devIdx )
{
  assert( devIdx < cmApFileDeviceCount());
  return cmAudioFileDevLabel( _cmApf->devArray[devIdx].devH ); 
}

unsigned      cmApFileDeviceChannelCount(   unsigned devIdx, bool inputFl )
{
  assert( devIdx < cmApFileDeviceCount());
  return cmAudioFileDevChannelCount( _cmApf->devArray[devIdx].devH, inputFl ); 
}

double        cmApFileDeviceSampleRate(     unsigned devIdx )
{
  assert( devIdx < cmApFileDeviceCount());
  return cmAudioFileDevSampleRate( _cmApf->devArray[devIdx].devH ); 
}

unsigned      cmApFileDeviceFramesPerCycle( unsigned devIdx, bool inputFl )
{
  assert( devIdx < cmApFileDeviceCount());
  return cmAudioFileDevFramesPerCycle( _cmApf->devArray[devIdx].devH, inputFl ); 
}

cmApRC_t      cmApFileDeviceSetup(          
  unsigned          devIdx, 
  double            srate, 
  unsigned          framesPerCycle, 
  cmApCallbackPtr_t callbackPtr,
  void*             userCbPtr )
{
  assert( devIdx < cmApFileDeviceCount());

  if( cmAudioFileDevSetup( _cmApf->devArray[devIdx].devH,srate,framesPerCycle,callbackPtr,userCbPtr) != kOkAfdRC )
    return cmErrMsg(&_cmApf->err,kAudioPortFileFailApRC,"The audio file device setup failed.");

  return kOkApRC;
}

cmApRC_t      cmApFileDeviceStart( unsigned devIdx )
{
  assert( devIdx < cmApFileDeviceCount());

  if( cmAudioFileDevStart( _cmApf->devArray[devIdx].devH ) != kOkAfdRC )
    return cmErrMsg(&_cmApf->err,kAudioPortFileFailApRC,"The audio file device setup failed.");

  return kOkApRC;
}

cmApRC_t      cmApFileDeviceStop(  unsigned devIdx )
{
  assert( devIdx < cmApFileDeviceCount());

  if( cmAudioFileDevStop( _cmApf->devArray[devIdx].devH ) != kOkAfdRC )
    return cmErrMsg(&_cmApf->err,kAudioPortFileFailApRC,"The audio file device setup failed.");

  return kOkApRC;
}

bool          cmApFileDeviceIsStarted( unsigned devIdx )
{
  assert( devIdx < cmApFileDeviceCount());
  return cmAudioFileDevIsStarted( _cmApf->devArray[devIdx].devH );
}

void          cmApFileReport( cmRpt_t* rpt )
{
  unsigned i;
  for(i=0; _cmApf->devCnt; ++i)
  {
    cmRptPrintf(rpt,"%i: ",i);
    cmAudioFileDevReport( _cmApf->devArray[i].devH, rpt );
    cmRptPrintf(rpt,"\n");
  }
}


// device callback function used with cmAudioPortFileTest() note that this assumes
// that the packet buffer contain non-interleaved data.
void _cmApFileTestCb(
  cmApAudioPacket_t* inPktArray, 
  unsigned           inPktCnt, 
  cmApAudioPacket_t* outPktArray, 
  unsigned           outPktCnt )
{
  cmApAudioPacket_t* ip  = inPktArray;
  cmApAudioPacket_t* op  = outPktArray;
  unsigned           opi = 0;
  unsigned           ipi = 0;
  unsigned           oci = 0;
  unsigned           ici = 0;

  while(1)
  {
    if( ici == ip->chCnt)
    {
      ici = 0;
      if( ++ipi >= inPktCnt )
        break;

      ip = inPktArray + ipi;
    }


    if( oci == op->chCnt )
    {
      oci = 0;
      if( ++opi >= outPktCnt )
        break;

      ip = outPktArray + opi;
    }

    assert( ip->audioFramesCnt == op->audioFramesCnt );
    assert( cmIsFlag(ip->flags,kInterleavedApFl)==false && cmIsFlag(ip->flags,kInterleavedApFl)==false );

    cmApSample_t* ibp = ((cmApSample_t*)ip->audioBytesPtr) + (ip->audioFramesCnt*ici);
    cmApSample_t* obp = ((cmApSample_t*)op->audioBytesPtr) + (op->audioFramesCnt*oci);

    memcpy(obp,ibp,ip->audioFramesCnt*sizeof(cmApSample_t));

    ++ici;
    ++oci;
  }
}


void          cmApFileTest( cmRpt_t* rpt )
{
  unsigned dev0Idx;
 
  const cmChar_t* promptStr      = "apf> q=quit 1=start 0=stop\n";
  const cmChar_t* label0         = "file0";
  const cmChar_t* i0Fn           = "/home/kevin/media/audio/McGill-1/1 Audio Track.aiff";
  const cmChar_t* o0Fn           = "/home/kevin/temp/afd1.aif";
  unsigned        o0Bits         = 16;
  unsigned        o0ChCnt        = 2;
   double         srate          = 44100;
  unsigned        framesPerCycle = 512;

  // initialize audio port file API
  if( cmApFileInitialize(rpt,0) != kOkApRC )
    return;

  // create an audio port file
  if((dev0Idx = cmApFileDeviceCreate(label0,i0Fn,o0Fn,o0Bits,o0ChCnt)) == cmInvalidIdx )
    goto errLabel;
 
  // configure an audio port file
  if( cmApFileDeviceSetup( dev0Idx, srate, framesPerCycle, _cmApFileTestCb, NULL ) != kOkAfdRC )
    goto errLabel;
 
  char c;
  fputs(promptStr,stderr);
  fflush(stdin);

  while((c=getchar()) != 'q')
  {
    switch(c)
    {
      case '0': cmApFileDeviceStart(dev0Idx); break;
      case '1': cmApFileDeviceStop(dev0Idx);  break;
    }

    fputs(promptStr,stderr);
    fflush(stdin);
    c = 0;
  }



 errLabel:
  //cmApFileDeviceDestroy(dev0Idx);

  cmApFileFinalize();

}
