
#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmMidi.h"
#include "cmMidiPort.h"

typedef struct
{
  bool            inputFl;
  char*           nameStr;
  //SInt32          uid;
  //MIDIEndpointRef epr;
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


  
} cmMpRoot;

cmMpRoot _cmMpRoot = { {NULL,NULL,kOkMpRC}, 0, NULL, NULL, NULL };


cmMpRC_t cmMpInitialize( cmMpCallback_t cbFunc, void* cbDataPtr, unsigned parserBufByteCnt, const char* appNameStr, cmRpt_t* rpt )
{
  cmMpRC_t                  rc                = kOkMpRC;

  if((rc = cmMpFinalize()) != kOkMpRC )
    return rc;

  cmErrSetup(&_cmMpRoot.err,rpt,"MIDI Port");

  _cmMpRoot.cbFunc            = cbFunc;
  _cmMpRoot.cbDataPtr         = cbDataPtr;



  return rc;
  
}

cmMpRC_t cmMpFinalize()
{
  cmMpRC_t   rc  = kOkMpRC;



  return rc;
}

bool        cmMpIsInitialized()
{ return false; }

unsigned      cmMpDeviceCount()
{
  return 0;
}

const char* cmMpDeviceName(       unsigned devIdx )
{ return NULL;
}

unsigned    cmMpDevicePortCount(  unsigned devIdx, unsigned flags )
{
  return 0;
}

const char*    cmMpDevicePortName(   unsigned devIdx, unsigned flags, unsigned portIdx )
{
  return NULL;
}


cmMpRC_t  cmMpDeviceSend( unsigned devIdx, unsigned portIdx, cmMidiByte_t status, cmMidiByte_t d0, cmMidiByte_t d1 )
{
	return kOkMpRC;
}

cmMpRC_t      cmMpDeviceSendData( unsigned devIdx, unsigned portIdx, const cmMidiByte_t* dataPtr, unsigned byteCnt )
{
	return kOkMpRC;

}

cmMpRC_t    cmMpInstallCallback( unsigned devIdx, unsigned portIdx, cmMpCallback_t cbFunc, void* cbDataPtr )
{
  cmMpRC_t rc = kOkMpRC;
  return rc;
}

cmMpRC_t    cmMpRemoveCallback(  unsigned devIdx, unsigned portIdx, cmMpCallback_t cbFunc, void* cbDataPtr )
{
  cmMpRC_t rc     = kOkMpRC;
  return rc;
}

bool        cmMpUsesCallback(    unsigned devIdx, unsigned portIdx, cmMpCallback_t cbFunc, void* cbDataPtr )
{
  return false;
}

void cmMpReport( cmRpt_t* rpt )
{
}


