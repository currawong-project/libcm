//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.

#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmThread.h"
#include "cmTime.h"
#include "cmMidi.h"
#include "cmMidiPort.h"

#include <alsa/asoundlib.h>


typedef struct
{
  bool            inputFl;    // true if this an input port
  char*           nameStr;    // string label of this device
  unsigned        alsa_type;  // ALSA type flags from snd_seq_port_info_get_type()
  unsigned        alsa_cap;   // ALSA capability flags from snd_seq_port_info_get_capability()
  snd_seq_addr_t  alsa_addr;  // ALSA client/port address for this port
  cmMpParserH_t   parserH;    // interface to the client callback function for this port
} cmMpPort_t;

// MIDI devices 
typedef struct
{
  char*         nameStr;    // string label for this device
  unsigned      iPortCnt;   // input ports on this device
  cmMpPort_t*   iPortArray;
  unsigned      oPortCnt;   // output ports on this device
  cmMpPort_t*   oPortArray;
  unsigned char clientId;  // ALSA client id (all ports on this device use use this client id in their address)

} cmMpDev_t;

typedef struct
{
  cmErr_t         err;          // error object
  cmLHeapH_t      lH;           // linked heap used for all internal memory

  unsigned        devCnt;       // MIDI devices attached to this computer
  cmMpDev_t*      devArray;

	cmMpCallback_t  cbFunc;       // MIDI input application callback 
  void*           cbDataPtr;

  snd_seq_t*      h;            // ALSA system sequencer handle

  snd_seq_addr_t  alsa_addr;    // ALSA client/port address representing the application
  int             alsa_queue;   // ALSA device queue
  cmThreadH_t     thH;          // MIDI input listening thread

  int             alsa_fdCnt;   // MIDI input driver file descriptor array
  struct pollfd*  alsa_fd;

  cmMpDev_t*      prvRcvDev;    // the last device and port to rcv MIDI 
  cmMpPort_t*     prvRcvPort;

  unsigned        prvTimeMicroSecs; // time of last recognized event in microseconds
  unsigned        eventCnt;     // count of recognized events
  
  cmTimeSpec_t    baseTimeStamp;

} cmMpRoot_t;

cmMpRoot_t* _cmMpRoot = NULL;

cmMpRC_t _cmMpErrMsgV(cmErr_t* err, cmMpRC_t rc, int alsaRc, const cmChar_t* fmt, va_list vl )
{
  if( alsaRc < 0 )
    cmErrMsg(err,kSysErrMpRC,"ALSA Error:%i %s",alsaRc,snd_strerror(alsaRc));

  return cmErrVMsg(err,rc,fmt,vl);  
}

cmMpRC_t _cmMpErrMsg(cmErr_t* err, cmMpRC_t rc, int alsaRc, const cmChar_t* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  rc = _cmMpErrMsgV(err,rc,alsaRc,fmt,vl);
  va_end(vl);
  return rc;
}

unsigned _cmMpGetPortCnt( snd_seq_t* h, snd_seq_port_info_t* pip, bool inputFl )
{
  unsigned i = 0;

  snd_seq_port_info_set_port(pip,-1);

  while( snd_seq_query_next_port(h,pip) == 0)
    if( cmIsFlag(snd_seq_port_info_get_capability(pip),inputFl?SND_SEQ_PORT_CAP_READ:SND_SEQ_PORT_CAP_WRITE) ) 
      ++i;
 
  return i;
}

cmMpDev_t* _cmMpClientIdToDev( int clientId )
{
  cmMpRoot_t* p = _cmMpRoot;
  unsigned    i;
  for(i=0; i<p->devCnt; ++i)
    if( p->devArray[i].clientId == clientId )
      return p->devArray + i;

  return NULL;
}

cmMpPort_t* _cmMpInPortIdToPort( cmMpDev_t* dev, int portId )
{
  unsigned i;

  for(i=0; i<dev->iPortCnt; ++i)
    if( dev->iPortArray[i].alsa_addr.port == portId )
      return dev->iPortArray + i;

  return NULL;
}


void _cmMpSplit14Bits( unsigned v, cmMidiByte_t* d0, cmMidiByte_t* d1 )
{
  *d0 = (v & 0x3f80) >> 7;
  *d1 = v & 0x7f;
}

cmMpRC_t cmMpPoll()
{
  cmMpRC_t    rc        = kOkMpRC;
  cmMpRoot_t* p         = _cmMpRoot;
  int         timeOutMs = 50;

  snd_seq_event_t *ev;

  if (poll(p->alsa_fd, p->alsa_fdCnt, timeOutMs) > 0) 
  {
    int rc = 1;

    do
    {
      rc = snd_seq_event_input(p->h,&ev);

      // if no input
      if( rc == -EAGAIN )
        break;

      // if input buffer overrun
      if( rc == -ENOSPC )
        break;
      
      // get the device this event arrived from
      if( p->prvRcvDev==NULL || p->prvRcvDev->clientId != ev->source.client )
        p->prvRcvDev = _cmMpClientIdToDev(ev->source.client);
      
      // get the port this event arrived from
      if( p->prvRcvDev != NULL && (p->prvRcvPort==NULL || p->prvRcvPort->alsa_addr.port != ev->source.port) )
        p->prvRcvPort = _cmMpInPortIdToPort(p->prvRcvDev,ev->source.port);

      if( p->prvRcvDev == NULL || p->prvRcvPort == NULL )
        continue;

      //printf("%i %x\n",ev->type,ev->type);
      //printf("dev:%i port:%i ch:%i %i\n",ev->source.client,ev->source.port,ev->data.note.channel,ev->data.note.note);

      unsigned     microSecs1     = (ev->time.time.tv_sec * 1000000) + (ev->time.time.tv_nsec/1000);
      //unsigned     deltaMicroSecs = p->prvTimeMicroSecs==0 ? 0 : microSecs1 - p->prvTimeMicroSecs;
      cmMidiByte_t d0             = 0xff;
      cmMidiByte_t d1             = 0xff;
      cmMidiByte_t status         = 0;

      switch(ev->type)
      {
        //
        // MIDI Channel Messages
        //

        case SND_SEQ_EVENT_NOTEON:
          status = kNoteOnMdId;
          d0     = ev->data.note.note;
          d1     = ev->data.note.velocity;
          //printf("%s (%i : %i) (%i)\n", snd_seq_ev_is_abstime(ev)?"abs":"rel",ev->time.time.tv_sec,ev->time.time.tv_nsec, deltaMicroSecs/1000);
          break;

        case SND_SEQ_EVENT_NOTEOFF:
          status = kNoteOffMdId;
          d0     = ev->data.note.note;
          d1     = ev->data.note.velocity;
          break;

        case SND_SEQ_EVENT_KEYPRESS:
          status = kPolyPresMdId;
          d0     = ev->data.note.note;
          d1     = ev->data.note.velocity;
          break;

        case SND_SEQ_EVENT_PGMCHANGE:
          status = kPgmMdId;
          d0     = ev->data.control.param;
          d1     = 0xff;
          break;

        case SND_SEQ_EVENT_CHANPRESS:
          status = kChPresMdId;
          d0     = ev->data.control.param;
          d1     = 0xff;
          break;

        case SND_SEQ_EVENT_CONTROLLER:
          status = kCtlMdId;
          d0     = ev->data.control.param;
          d1     = ev->data.control.value;
          break;

        case SND_SEQ_EVENT_PITCHBEND:
          _cmMpSplit14Bits(ev->data.control.value + 8192, &d0, &d1 );
          status = kPbendMdId;
          break;

        //
        // MIDI System Common Messages
        //
        case SND_SEQ_EVENT_QFRAME:       
          status = kSysComMtcMdId;  
          d0     = ev->data.control.value;
          break;

        case SND_SEQ_EVENT_SONGPOS:      
          _cmMpSplit14Bits(ev->data.control.value, &d0, &d1 );
          status = kSysComSppMdId;            
          break;

        case SND_SEQ_EVENT_SONGSEL:      
          status = kSysComSelMdId;  
          d0     = ev->data.control.value;
          break;

        case SND_SEQ_EVENT_TUNE_REQUEST: 
          status = kSysComTuneMdId; 
          break;

        //
        // MIDI System Real-time Messages
        //
        case SND_SEQ_EVENT_CLOCK:     status = kSysRtClockMdId; break;
        case SND_SEQ_EVENT_START:     status = kSysRtStartMdId; break;
        case SND_SEQ_EVENT_CONTINUE:  status = kSysRtContMdId;  break;          
        case SND_SEQ_EVENT_STOP:      status = kSysRtStopMdId;  break;
        case SND_SEQ_EVENT_SENSING:   status = kSysRtSenseMdId; break;
        case SND_SEQ_EVENT_RESET:     status = kSysRtResetMdId; break;

      }

      if( status != 0 )
      {
        cmMidiByte_t ch = ev->data.note.channel;
        cmTimeSpec_t ts;
        ts.tv_sec  = p->baseTimeStamp.tv_sec  + ev->time.time.tv_sec;
        ts.tv_nsec = p->baseTimeStamp.tv_nsec + ev->time.time.tv_nsec;
        if( ts.tv_nsec > 1000000000 )
        {
          ts.tv_nsec -= 1000000000;
          ts.tv_sec  += 1;
        }

        //printf("MIDI: %ld %ld : 0x%x %i %i\n",ts.tv_sec,ts.tv_nsec,status,d0,d1);

        cmMpParserMidiTriple(p->prvRcvPort->parserH, &ts, status | ch, d0, d1 );

        p->prvTimeMicroSecs  = microSecs1;
        p->eventCnt         += 1;
      }

    }while( snd_seq_event_input_pending(p->h,0));

    cmMpParserTransmit(p->prvRcvPort->parserH);
  }  

  return rc;
}


bool _cmMpThreadFunc(void* param)
{
  cmMpPoll();
  return true;
}

cmMpRC_t _cmMpAllocStruct( cmMpRoot_t* p, const cmChar_t* appNameStr, cmMpCallback_t cbFunc, void* cbDataPtr, unsigned parserBufByteCnt, cmRpt_t* rpt )
{
  cmMpRC_t                  rc   = kOkMpRC;
  snd_seq_client_info_t*    cip  = NULL;
  snd_seq_port_info_t*      pip  = NULL;
  snd_seq_port_subscribe_t *subs = NULL;
  unsigned                  i,j,k,arc;

  // alloc the subscription recd on the stack
  snd_seq_port_subscribe_alloca(&subs);

  // alloc the client recd
  if((arc = snd_seq_client_info_malloc(&cip)) < 0 )
  {
    rc = _cmMpErrMsg(&p->err,kSysErrMpRC,arc,"ALSA seq client info allocation failed.");
    goto errLabel;
  }

  // alloc the port recd
  if((arc = snd_seq_port_info_malloc(&pip)) < 0 )
  {
    rc = _cmMpErrMsg(&p->err,kSysErrMpRC,arc,"ALSA seq port info allocation failed.");
    goto errLabel;
  }

  if((p->alsa_queue = snd_seq_alloc_queue(p->h)) < 0 )
  {
    rc = _cmMpErrMsg(&p->err,kSysErrMpRC,p->alsa_queue,"ALSA queue allocation failed.");
    goto errLabel;
  }

  // Set arbitrary tempo (mm=100) and resolution (240) (FROM RtMidi.cpp)
  /*
  snd_seq_queue_tempo_t *qtempo;
  snd_seq_queue_tempo_alloca(&qtempo);
  snd_seq_queue_tempo_set_tempo(qtempo, 600000);
  snd_seq_queue_tempo_set_ppq(qtempo, 240);
  snd_seq_set_queue_tempo(p->h, p->alsa_queue, qtempo);
  snd_seq_drain_output(p->h);
  */

  // setup the client port
  snd_seq_set_client_name(p->h,appNameStr);
  snd_seq_port_info_set_client(pip, p->alsa_addr.client = snd_seq_client_id(p->h) );
  snd_seq_port_info_set_name(pip,cmStringNullGuard(appNameStr));
  snd_seq_port_info_set_capability(pip,SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_DUPLEX | SND_SEQ_PORT_CAP_SUBS_READ | SND_SEQ_PORT_CAP_SUBS_WRITE );
  snd_seq_port_info_set_type(pip, SND_SEQ_PORT_TYPE_SOFTWARE | SND_SEQ_PORT_TYPE_APPLICATION | SND_SEQ_PORT_TYPE_MIDI_GENERIC );
 
  snd_seq_port_info_set_midi_channels(pip, 16);

  // cfg for real-time time stamping
  snd_seq_port_info_set_timestamping(pip, 1);
  snd_seq_port_info_set_timestamp_real(pip, 1);    
  snd_seq_port_info_set_timestamp_queue(pip, p->alsa_queue);

  // create the client port
  if((p->alsa_addr.port = snd_seq_create_port(p->h,pip)) < 0 )
  {
    rc = _cmMpErrMsg(&p->err,kSysErrMpRC,p->alsa_addr.port,"ALSA client port creation failed.");
    goto errLabel;
  }


  p->devCnt = 0;

  // determine the count of devices
  snd_seq_client_info_set_client(cip, -1);
  while( snd_seq_query_next_client(p->h,cip) == 0)
    p->devCnt += 1;

  // allocate the device array
  p->devArray = cmLhAllocZ(p->lH,cmMpDev_t,p->devCnt);

  // fill in each device record
  snd_seq_client_info_set_client(cip, -1);
  for(i=0; snd_seq_query_next_client(p->h,cip)==0; ++i)
  {
    assert(i<p->devCnt);

    int         client = snd_seq_client_info_get_client(cip);
    const char* name   = snd_seq_client_info_get_name(cip);
    
    // initalize the device record
    p->devArray[i].nameStr    = cmLhAllocStr(p->lH,cmStringNullGuard(name));
    p->devArray[i].iPortCnt   = 0;
    p->devArray[i].oPortCnt   = 0;
    p->devArray[i].iPortArray = NULL;
    p->devArray[i].oPortArray = NULL;
    p->devArray[i].clientId   = client;


    snd_seq_port_info_set_client(pip,client);
    snd_seq_port_info_set_port(pip,-1);

    // determine the count of in/out ports on this device
    while( snd_seq_query_next_port(p->h,pip) == 0 )
    {
      unsigned    caps = snd_seq_port_info_get_capability(pip);

      if( cmIsFlag(caps,SND_SEQ_PORT_CAP_READ) )
        p->devArray[i].iPortCnt += 1;

      if( cmIsFlag(caps,SND_SEQ_PORT_CAP_WRITE) )
        p->devArray[i].oPortCnt += 1;
      
    }

    // allocate the device port arrays
    if( p->devArray[i].iPortCnt > 0 )
      p->devArray[i].iPortArray = cmLhAllocZ(p->lH,cmMpPort_t,p->devArray[i].iPortCnt);

    if( p->devArray[i].oPortCnt > 0 )
      p->devArray[i].oPortArray = cmLhAllocZ(p->lH,cmMpPort_t,p->devArray[i].oPortCnt);
    

    snd_seq_port_info_set_client(pip,client);    // set the ports client id
    snd_seq_port_info_set_port(pip,-1);

    // fill in the port information 
    for(j=0,k=0; snd_seq_query_next_port(p->h,pip) == 0; )
    {
      const char*    port = snd_seq_port_info_get_name(pip);
      unsigned       type = snd_seq_port_info_get_type(pip);
      unsigned       caps = snd_seq_port_info_get_capability(pip);
      snd_seq_addr_t addr = *snd_seq_port_info_get_addr(pip);

      if( cmIsFlag(caps,SND_SEQ_PORT_CAP_READ) )
      {
        assert(j<p->devArray[i].iPortCnt);
        p->devArray[i].iPortArray[j].inputFl   = true;
        p->devArray[i].iPortArray[j].nameStr   = cmLhAllocStr(p->lH,cmStringNullGuard(port));
        p->devArray[i].iPortArray[j].alsa_type = type;
        p->devArray[i].iPortArray[j].alsa_cap  = caps;
        p->devArray[i].iPortArray[j].alsa_addr = addr;
        p->devArray[i].iPortArray[j].parserH   = cmMpParserCreate(i, j, cbFunc, cbDataPtr, parserBufByteCnt, rpt );

        // port->app
        snd_seq_port_subscribe_set_sender(subs, &addr);
        snd_seq_port_subscribe_set_dest(subs, &p->alsa_addr);
        snd_seq_port_subscribe_set_queue(subs, 1);
        snd_seq_port_subscribe_set_time_update(subs, 1);
        snd_seq_port_subscribe_set_time_real(subs, 1);
        if((arc = snd_seq_subscribe_port(p->h, subs)) < 0)
          rc = _cmMpErrMsg(&p->err,kSysErrMpRC,arc,"Input port to app. subscription failed on port '%s'.",cmStringNullGuard(port));


        ++j;
      }

      if( cmIsFlag(caps,SND_SEQ_PORT_CAP_WRITE) )
      {
        assert(k<p->devArray[i].oPortCnt);
        p->devArray[i].oPortArray[k].inputFl   = false;
        p->devArray[i].oPortArray[k].nameStr   = cmLhAllocStr(p->lH,cmStringNullGuard(port));
        p->devArray[i].oPortArray[k].alsa_type = type;
        p->devArray[i].oPortArray[k].alsa_cap  = caps;
        p->devArray[i].oPortArray[k].alsa_addr = addr;

        // app->port connection
        snd_seq_port_subscribe_set_sender(subs, &p->alsa_addr);
        snd_seq_port_subscribe_set_dest(  subs, &addr);
        if((arc = snd_seq_subscribe_port(p->h, subs)) < 0 )
          rc = _cmMpErrMsg(&p->err,kSysErrMpRC,arc,"App to output port subscription failed on port '%s'.",cmStringNullGuard(port));
       
        ++k;
      }
    }
  }

 errLabel:
  if( pip != NULL)
    snd_seq_port_info_free(pip);

  if( cip != NULL )
    snd_seq_client_info_free(cip);

  return rc;
  
}


cmMpRC_t cmMpInitialize( cmCtx_t* ctx, cmMpCallback_t cbFunc, void* cbArg, unsigned parserBufByteCnt, const char* appNameStr )
{
  cmMpRC_t               rc  = kOkMpRC;
  int                    arc = 0;
  cmMpRoot_t*            p = NULL;

  if((rc = cmMpFinalize()) != kOkMpRC )
    return rc;

  // allocate the global root object
  _cmMpRoot = p = cmMemAllocZ(cmMpRoot_t,1);
  p->h          = NULL;
  p->alsa_queue = -1;

  cmErrSetup(&p->err,&ctx->rpt,"MIDI Port");

  // setup the local linked heap manager
  if(cmLHeapIsValid(p->lH = cmLHeapCreate(2048,ctx)) == false )
  {
    rc = _cmMpErrMsg(&p->err,kLHeapErrMpRC,0,"Linked heap initialization failed.");
    goto errLabel;
  }

  // create the listening thread
  if( cmThreadCreate( &p->thH, _cmMpThreadFunc, NULL, &ctx->rpt) != kOkThRC )
  {
    rc = _cmMpErrMsg(&p->err,kThreadErrMpRC,0,"Thread initialization failed.");
    goto errLabel;
  }

  // initialize the ALSA sequencer
  if((arc = snd_seq_open(&p->h, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK )) < 0 )
  {
    rc = _cmMpErrMsg(&p->err,kSysErrMpRC,arc,"ALSA Sequencer open failed.");
    goto errLabel;
  }

  // setup the device and port structures
  if((rc = _cmMpAllocStruct(p,appNameStr,cbFunc,cbArg,parserBufByteCnt,&ctx->rpt)) != kOkMpRC )
    goto errLabel;

  // allocate the file descriptors used for polling
  p->alsa_fdCnt = snd_seq_poll_descriptors_count(p->h, POLLIN);
  p->alsa_fd = cmMemAllocZ(struct pollfd,p->alsa_fdCnt);
  snd_seq_poll_descriptors(p->h, p->alsa_fd, p->alsa_fdCnt, POLLIN);

  p->cbFunc    = cbFunc;
  p->cbDataPtr = cbArg;

  // start the sequencer queue
  if((arc = snd_seq_start_queue(p->h, p->alsa_queue, NULL)) < 0 )
  {
    rc = _cmMpErrMsg(&p->err,kSysErrMpRC,arc,"ALSA queue start failed.");
    goto errLabel;
  }
  
  // send any pending commands to the driver
  snd_seq_drain_output(p->h);
  
  // all time stamps will be an offset from this time stamp
  clock_gettime(CLOCK_MONOTONIC,&p->baseTimeStamp);

  if( cmThreadPause(p->thH,0) != kOkThRC )
    rc = _cmMpErrMsg(&p->err,kThreadErrMpRC,0,"Thread start failed.");

 errLabel:

  if( rc != kOkMpRC )
    cmMpFinalize();

  return rc;
  
}

cmMpRC_t cmMpFinalize()
{
  cmMpRC_t    rc = kOkMpRC;
  cmMpRoot_t* p  = _cmMpRoot;

  if( _cmMpRoot != NULL )
  {
    int arc;

    // stop the thread first
    if( cmThreadDestroy(&p->thH) != kOkThRC )
    {
      rc = _cmMpErrMsg(&p->err,kThreadErrMpRC,0,"Thread destroy failed.");
      goto errLabel;
    }

    // stop the queue
    if( p->h != NULL )
      if((arc = snd_seq_stop_queue(p->h,p->alsa_queue, NULL)) < 0 )
      {
        rc = _cmMpErrMsg(&p->err,kSysErrMpRC,arc,"ALSA queue stop failed.");
        goto errLabel;
      }

    // release the alsa queue
    if( p->alsa_queue != -1 )
    {
      if((arc = snd_seq_free_queue(p->h,p->alsa_queue)) < 0 )
        rc = _cmMpErrMsg(&p->err,kSysErrMpRC,arc,"ALSA queue release failed.");
      else
        p->alsa_queue = -1;
    }

    // release the alsa system handle
    if( p->h != NULL )
    {
      if( (arc = snd_seq_close(p->h)) < 0 )
        rc = _cmMpErrMsg(&p->err,kSysErrMpRC,arc,"ALSA sequencer close failed.");
      else
        p->h = NULL;
    }

    // release each parser
    unsigned i,j;
    for(i=0; i<p->devCnt; ++i)
      for(j=0; j<p->devArray[i].iPortCnt; ++j)
        cmMpParserDestroy(&p->devArray[i].iPortArray[j].parserH);

    cmLHeapDestroy(&p->lH);

    cmMemFree(p->alsa_fd);

    cmMemPtrFree(&_cmMpRoot);
    
  }

 errLabel:
  return rc;
}

bool        cmMpIsInitialized()
{ return _cmMpRoot!=NULL; }

unsigned      cmMpDeviceCount()
{ return _cmMpRoot==NULL ? 0 : _cmMpRoot->devCnt; }

const char* cmMpDeviceName( unsigned devIdx )
{ 
  if( _cmMpRoot==NULL || devIdx>=_cmMpRoot->devCnt)
    return NULL;

  return _cmMpRoot->devArray[devIdx].nameStr;
}

unsigned    cmMpDevicePortCount(  unsigned devIdx, unsigned flags )
{
  if( _cmMpRoot==NULL || devIdx>=_cmMpRoot->devCnt)
    return 0;

  if( cmIsFlag(flags,kInMpFl) )
    return _cmMpRoot->devArray[devIdx].iPortCnt;

  return _cmMpRoot->devArray[devIdx].oPortCnt;  
}

const char*    cmMpDevicePortName(   unsigned devIdx, unsigned flags, unsigned portIdx )
{
  if( _cmMpRoot==NULL || devIdx>=_cmMpRoot->devCnt)
    return 0;

  if( cmIsFlag(flags,kInMpFl) )
  {
    if( portIdx >= _cmMpRoot->devArray[devIdx].iPortCnt )
      return 0;

    return _cmMpRoot->devArray[devIdx].iPortArray[portIdx].nameStr;
  }

  if( portIdx >= _cmMpRoot->devArray[devIdx].oPortCnt )
    return 0;

  return _cmMpRoot->devArray[devIdx].oPortArray[portIdx].nameStr;  
}


cmMpRC_t  cmMpDeviceSend( unsigned devIdx, unsigned portIdx, cmMidiByte_t status, cmMidiByte_t d0, cmMidiByte_t d1 )
{
  cmMpRC_t rc = kOkMpRC;
  snd_seq_event_t ev;
  int arc;
  cmMpRoot_t* p = _cmMpRoot;

  assert( p!=NULL && devIdx < p->devCnt && portIdx < p->devArray[devIdx].oPortCnt );

  cmMpPort_t* port = p->devArray[devIdx].oPortArray + portIdx;

  snd_seq_ev_clear(&ev);
  snd_seq_ev_set_source(&ev, p->alsa_addr.port);
  //snd_seq_ev_set_subs(&ev);

  snd_seq_ev_set_dest(&ev, port->alsa_addr.client, port->alsa_addr.port);
  snd_seq_ev_set_direct(&ev);
  snd_seq_ev_set_fixed(&ev);

  
  switch( status & 0xf0 )
  {
    case kNoteOffMdId:  
      ev.type = SND_SEQ_EVENT_NOTEOFF;    
      ev.data.note.note     = d0;
      ev.data.note.velocity = d1;
      break;

    case kNoteOnMdId:   
      ev.type               = SND_SEQ_EVENT_NOTEON;     
      ev.data.note.note     = d0;
      ev.data.note.velocity = d1;
      break;

    case kPolyPresMdId: 
      ev.type = SND_SEQ_EVENT_KEYPRESS ;  
      ev.data.note.note     = d0;
      ev.data.note.velocity = d1;
      break;

    case kCtlMdId:      
      ev.type = SND_SEQ_EVENT_CONTROLLER; 
      ev.data.control.param  = d0;
      ev.data.control.value  = d1;
      break;

    case kPgmMdId:      
      ev.type = SND_SEQ_EVENT_PGMCHANGE;  
      ev.data.control.param  = d0;
      ev.data.control.value  = d1;
      break; 

    case kChPresMdId:   
      ev.type = SND_SEQ_EVENT_CHANPRESS;  
      ev.data.control.param  = d0;
      ev.data.control.value  = d1;
      break;

    case kPbendMdId:    
      {
        int val = d0;
        val <<= 7;
        val += d1;
        val -= 8192;

        ev.type = SND_SEQ_EVENT_PITCHBEND;  
        ev.data.control.param  = 0;
        ev.data.control.value  = val;
      }
      break;

    default:
      rc = _cmMpErrMsg(&p->err,kInvalidArgMpRC,0,"Cannot send an invalid MIDI status byte:0x%x.",status & 0xf0);
      goto errLabel;
  }

  ev.data.note.channel  = status & 0x0f;

  if((arc = snd_seq_event_output(p->h, &ev)) < 0 )
    rc = _cmMpErrMsg(&p->err,kSysErrMpRC,arc,"MIDI event output failed.");

  if((arc = snd_seq_drain_output(p->h)) < 0 )
    rc = _cmMpErrMsg(&p->err,kSysErrMpRC,arc,"MIDI event output drain failed.");

 errLabel:
	return rc;
}

cmMpRC_t      cmMpDeviceSendData( unsigned devIdx, unsigned portIdx, const cmMidiByte_t* dataPtr, unsigned byteCnt )
{
  cmMpRoot_t* p  = _cmMpRoot;
  return cmErrMsg(&p->err,kNotImplMpRC,"cmMpDeviceSendData() has not yet been implemented for ALSA.");
}

cmMpRC_t    cmMpInstallCallback( unsigned devIdx, unsigned portIdx, cmMpCallback_t cbFunc, void* cbDataPtr )
{
  cmMpRC_t    rc = kOkMpRC;
  unsigned    di;
  unsigned    dn = cmMpDeviceCount();
  cmMpRoot_t* p  = _cmMpRoot;

  for(di=0; di<dn; ++di)
    if( di==devIdx || devIdx == -1 )
    {
      unsigned pi;
      unsigned pn = cmMpDevicePortCount(di,kInMpFl);

      for(pi=0; pi<pn; ++pi)
        if( pi==portIdx || portIdx == -1 )
            if( cmMpParserInstallCallback( p->devArray[di].iPortArray[pi].parserH, cbFunc, cbDataPtr ) != kOkMpRC )
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
  cmMpRoot_t* p = _cmMpRoot;

  for(di=0; di<dn; ++di)
    if( di==devIdx || devIdx == -1 )
    {
      unsigned pi;
      unsigned pn = cmMpDevicePortCount(di,kInMpFl);

      for(pi=0; pi<pn; ++pi)
        if( pi==portIdx || portIdx == -1 )
          if( cmMpParserHasCallback(  p->devArray[di].iPortArray[pi].parserH, cbFunc, cbDataPtr ) )
          {
            if( cmMpParserRemoveCallback( p->devArray[di].iPortArray[pi].parserH, cbFunc, cbDataPtr ) != kOkMpRC )
              goto errLabel;
            else
              ++remCnt;
          }
    }

  if( remCnt == 0 && dn > 0 )
    rc =  _cmMpErrMsg(&p->err,kCbNotFoundMpRC,0,"The callback was not found on any of the specified devices or ports.");

 errLabel:
  return rc;
}

bool        cmMpUsesCallback(    unsigned devIdx, unsigned portIdx, cmMpCallback_t cbFunc, void* cbDataPtr )
{
  unsigned di;
  unsigned dn     = cmMpDeviceCount();
  cmMpRoot_t* p = _cmMpRoot;

  for(di=0; di<dn; ++di)
    if( di==devIdx || devIdx == -1 )
    {
      unsigned pi;
      unsigned pn = cmMpDevicePortCount(di,kInMpFl);

      for(pi=0; pi<pn; ++pi)
        if( pi==portIdx || portIdx == -1 )
          if( cmMpParserHasCallback(  p->devArray[di].iPortArray[pi].parserH, cbFunc, cbDataPtr ) )
            return true;
    }

  return false;
}


void _cmMpReportPort( cmRpt_t* rpt, const cmMpPort_t* port )
{
  cmRptPrintf(rpt,"    client:%i port:%i    %s caps:(",port->alsa_addr.client,port->alsa_addr.port,port->nameStr);
  if( port->alsa_cap & SND_SEQ_PORT_CAP_READ       ) cmRptPrintf(rpt,"Read " );
  if( port->alsa_cap & SND_SEQ_PORT_CAP_WRITE      ) cmRptPrintf(rpt,"Writ " );
  if( port->alsa_cap & SND_SEQ_PORT_CAP_SYNC_READ  ) cmRptPrintf(rpt,"Syrd " );
  if( port->alsa_cap & SND_SEQ_PORT_CAP_SYNC_WRITE ) cmRptPrintf(rpt,"Sywr " );
  if( port->alsa_cap & SND_SEQ_PORT_CAP_DUPLEX     ) cmRptPrintf(rpt,"Dupl " );
  if( port->alsa_cap & SND_SEQ_PORT_CAP_SUBS_READ  ) cmRptPrintf(rpt,"Subr " );
  if( port->alsa_cap & SND_SEQ_PORT_CAP_SUBS_WRITE ) cmRptPrintf(rpt,"Subw " );
  if( port->alsa_cap & SND_SEQ_PORT_CAP_NO_EXPORT  ) cmRptPrintf(rpt,"Nexp " );

  cmRptPrintf(rpt,") type:(");
  if( port->alsa_type & SND_SEQ_PORT_TYPE_SPECIFIC   )    cmRptPrintf(rpt,"Spec ");
 	if( port->alsa_type & SND_SEQ_PORT_TYPE_MIDI_GENERIC)   cmRptPrintf(rpt,"Gnrc ");
 	if( port->alsa_type & SND_SEQ_PORT_TYPE_MIDI_GM  )      cmRptPrintf(rpt,"GM ");
 	if( port->alsa_type & SND_SEQ_PORT_TYPE_MIDI_GS  )      cmRptPrintf(rpt,"GS ");
 	if( port->alsa_type & SND_SEQ_PORT_TYPE_MIDI_XG  )      cmRptPrintf(rpt,"XG ");
 	if( port->alsa_type & SND_SEQ_PORT_TYPE_MIDI_MT32 )     cmRptPrintf(rpt,"MT32 ");
 	if( port->alsa_type & SND_SEQ_PORT_TYPE_MIDI_GM2  )     cmRptPrintf(rpt,"GM2 ");
 	if( port->alsa_type & SND_SEQ_PORT_TYPE_SYNTH   )       cmRptPrintf(rpt,"Syn ");
 	if( port->alsa_type & SND_SEQ_PORT_TYPE_DIRECT_SAMPLE)  cmRptPrintf(rpt,"Dsmp ");
 	if( port->alsa_type & SND_SEQ_PORT_TYPE_SAMPLE   )      cmRptPrintf(rpt,"Samp ");
 	if( port->alsa_type & SND_SEQ_PORT_TYPE_HARDWARE   )    cmRptPrintf(rpt,"Hwar ");
 	if( port->alsa_type & SND_SEQ_PORT_TYPE_SOFTWARE   )    cmRptPrintf(rpt,"Soft ");
 	if( port->alsa_type & SND_SEQ_PORT_TYPE_SYNTHESIZER   ) cmRptPrintf(rpt,"Sizr ");
 	if( port->alsa_type & SND_SEQ_PORT_TYPE_PORT   )        cmRptPrintf(rpt,"Port ");
 	if( port->alsa_type & SND_SEQ_PORT_TYPE_APPLICATION   ) cmRptPrintf(rpt,"Appl ");

  cmRptPrintf(rpt,")\n");

}

void cmMpReport( cmRpt_t* rpt )
{
  cmMpRoot_t* p = _cmMpRoot;
  unsigned i,j;

  cmRptPrintf(rpt,"Buffer size bytes in:%i out:%i\n",snd_seq_get_input_buffer_size(p->h),snd_seq_get_output_buffer_size(p->h));

  for(i=0; i<p->devCnt; ++i)
  {
    const cmMpDev_t* d = p->devArray + i;

    cmRptPrintf(rpt,"%i : Device: %s \n",i,cmStringNullGuard(d->nameStr));

    if(d->iPortCnt > 0 )
      cmRptPrintf(rpt,"  Input:\n");

    for(j=0; j<d->iPortCnt; ++j)
      _cmMpReportPort(rpt,d->iPortArray+j);
    
    if(d->oPortCnt > 0 )
      cmRptPrintf(rpt,"  Output:\n");

    for(j=0; j<d->oPortCnt; ++j)
      _cmMpReportPort(rpt,d->oPortArray+j);
  }
}


