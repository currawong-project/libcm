//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.

#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmTime.h"
#include "cmAudioPort.h"
#include "cmMem.h"
#include "cmTime.h"
#include "cmMallocDebug.h"
#include "cmAudioPort.h"
#include "cmAudioPortAlsa.h"
#include "cmThread.h"

#include "alsa/asoundlib.h"
#include <unistd.h> // usleep

#define NAME_CHAR_CNT (255)
#define DESC_CHAR_CNT (255)
#define INIT_DEV_CNT  (5)

//#define IMPULSE_FN "/home/kevin/temp/recd0.txt"

enum { kDfltPeriodsPerBuf = 2, kPollfdsArrayCnt=2 };

enum { kInFl=0x01, kOutFl=0x02 };

struct cmApRoot_str;


typedef struct devRecd_str
{
  struct cmApRoot_str* rootPtr;
  unsigned             devIdx;
  cmChar_t             nameStr[ NAME_CHAR_CNT ];
  cmChar_t             descStr[ DESC_CHAR_CNT ];
  unsigned             flags;

  unsigned             framesPerCycle; // samples per sub-buffer 
  unsigned             periodsPerBuf;  // sub-buffers per buffer
  snd_async_handler_t* ahandler;
  unsigned             srate;          // device sample rate

  unsigned             iChCnt;         // ch count 
  unsigned             oChCnt;

  unsigned             iBits;          // bits per sample
  unsigned             oBits;

  bool                 iSignFl;        // sample type is signed
  bool                 oSignFl;

  bool                 iSwapFl;        // swap the sample bytes
  bool                 oSwapFl;

  unsigned             iSigBits;       // significant bits in each sample beginning
  unsigned             oSigBits;       // with the most sig. bit.


  cmApSample_t*        iBuf;    // iBuf[ iFpc * iChCnt ]
  cmApSample_t*        oBuf;    // oBuf[ oFpc * oChCnt ]

  unsigned             oBufCnt;  // count of buffers written

#ifdef IMPULSE_FN
  int*                 recdBuf;
  unsigned             recdN;
  unsigned             recdIdx;
#endif

  unsigned             iFpC;    // buffer frames per cycle  (in ALSA this is call period_size)
  unsigned             oFpC;

  snd_pcm_t*           iPcmH;    // device handle
  snd_pcm_t*           oPcmH;

  unsigned             iCbCnt;   // callback count 
  unsigned             oCbCnt;

  unsigned             iErrCnt;  // error count
  unsigned             oErrCnt;

  cmApCallbackPtr_t    cbPtr;    // user callback
  void*                userCbPtr;

} cmApDevRecd_t;

typedef struct cmApPoll_str
{
  cmApDevRecd_t* devPtr;
  bool           inputFl;
  unsigned       fdsCnt;
} cmApPollfdsDesc_t;

typedef struct cmApRoot_str
{
  cmRpt_t*        rpt;          //
  cmApDevRecd_t*  devArray;     // array of device records
  unsigned        devCnt;       // count of actual dev recds in devArray[]
  unsigned        devAllocCnt;  // count of dev recds allocated in devArray[]

  bool            asyncFl;      // true=use async callback false=use polling thread

  cmThreadH_t        thH;             // polling thread
  unsigned           pollfdsAllocCnt; // 2*devCnt (max possible in+out handles)
  struct pollfd*     pollfds;         // pollfds[ pollfdsAllocCnt ]
  cmApPollfdsDesc_t *pollfdsDesc;     // pollfdsDesc[ pollfdsAllocCnt ]
  unsigned           pollfdsCnt;      // count of active recds in pollfds[] and pollfdsDesc[]
  
} cmApRoot_t;

cmApRoot_t _cmApRoot = { NULL, NULL, 0, 0 };

//===============================================================================================
enum
{
  kReadErrRId,
  kWriteErrRId
};


#undef cmALSA_RECD
#ifdef cmALSA_RECD

enum
{
  kNotUsedRId,
  kStartRId,
  kCbRId,
  kSysErrRId,
  kAppErrRId,
  kRecoverRId
};

typedef struct
{
  int code;
  char* label;
} recdErrMap_t;

typedef struct
{
  unsigned        devIdx;
  unsigned        typeId;
  cmTimeSpec_t t;
  bool            inputFl;
  unsigned        arg;
  unsigned        arg1;
} recd;

recd*           recdArray = NULL;
unsigned        recdCnt   = 0;
unsigned        recdIdx   = 0;
cmTimeSpec_t recdTime;

recdErrMap_t recdSysErrMap[] = 
{
  { -EPIPE,    "EPIPE" },
  { -ESTRPIPE, "ESTRPIPE" },
  { -EBADFD,   "EBADFD" },
  { 0, NULL }
};

recdErrMap_t recdAppErrMap[] = 
{
  { kReadErrRId, "Read Error"},
  { kWriteErrRId, "Write Error"},
  { 0, NULL }
};

const char* _recdSysErrToLabel( int err )
{
  unsigned i;
  for(i=0; recdSysErrMap[i].label != NULL; ++i)
    if( recdSysErrMap[i].code == err )
      return recdSysErrMap[i].label;

  return "<Unknown sys error>";
}

const char* _recdAppErrToLabel( int err )
{
  unsigned i;
  for(i=0; recdAppErrMap[i].label != NULL; ++i)
    if( recdAppErrMap[i].code == err )
      return recdAppErrMap[i].label;

  return "<Unknown app error>";
}

void recdSetup()
{
  recdCnt = 100;
  recdIdx = 0;
  clock_gettime(CLOCK_REALTIME,&recdTime);
  recdArray = cmMemAllocZ(recd,recdCnt);
}

void recdPush( unsigned typeId, unsigned devIdx, bool inputFl, unsigned arg, unsigned arg1 )
{
  //if( recdIdx == recdCnt )
  //  return;

  if( recdIdx == recdCnt )
    recdIdx = 0;

  recd* r    = recdArray + recdIdx;
  r->typeId  = typeId;
  r->devIdx  = devIdx;
  r->inputFl = inputFl;
  r->arg     = arg;
  r->arg1    = arg1;
  clock_gettime(CLOCK_REALTIME,&r->t);
  ++recdIdx;
}


void recdStart( const cmApDevRecd_t* drp, bool inputFl )
{ recdPush(kStartRId,drp->devIdx,inputFl,0,0); }

void recdCb( const cmApDevRecd_t* drp, bool inputFl, unsigned frmCnt )
{ recdPush(kCbRId,drp->devIdx,inputFl, inputFl ? drp->iCbCnt : drp->oCbCnt, 0 ); }

void recdSysErr( bool inputFl, int err )
{ recdPush(kSysErrRId,cmInvalidIdx,inputFl,err,0); }

void recdAppErr( const cmApDevRecd_t* drp, bool inputFl, int app, int err )
{ recdPush(kAppErrRId,drp->devIdx,inputFl,app,err); }

void recdRecover( const cmApDevRecd_t* drp, bool inputFl, int err, int line )
{ recdPush(kRecoverRId,drp->devIdx,inputFl,err,line); }

void recdPrint()
{
  unsigned i;
  cmTimeSpec_t t0 = recdTime;
  unsigned j = recdIdx;
  for(i=0; i<recdCnt; ++i)
  {
    
    recd* r = recdArray + j;
    ++j;
    if( j == recdCnt )
      j = 0;

    double ms = cmTimeElapsedMicros(&recdTime,&r->t)/1000.0;
    double dms = cmTimeElapsedMicros(&t0,&r->t)/1000.0;
    
    t0 = r->t;

    printf("%5i %8.3f %8.3f %i %s: ",i,ms,dms,r->devIdx,r->inputFl ? "in ":"out");

    switch(r->typeId)
    {
      case kStartRId:
        printf("start");
        break;

      case kCbRId:
        printf("callback %i",r->arg );
        break;

      case kSysErrRId:
        printf("sys err %s ",_recdSysErrToLabel(r->arg));
        break;

      case kAppErrRId:
        printf("app err %s %s",_recdAppErrToLabel(r->arg),_recdSysErrToLabel(r->arg1));
        break;

      case kRecoverRId:
        printf("Recover %s %i",_recdSysErrToLabel(r->arg),r->arg1);
        break;

      default:
        printf("Unknown recd type id.\n");
        break;
    }

    printf("\n");
  }
}

void recdFree()
{
  recdPrint();
  cmMemFree(recdArray);
}
#else

void recdSetup(){}
void recdPush( unsigned typeId, unsigned devIdx, bool inputFl, unsigned arg, unsigned arg1 ){}
void recdStart( const cmApDevRecd_t* drp, bool inputFl ){}
void recdCb( const cmApDevRecd_t* drp, bool inputFl, unsigned frmCnt ){}
void recdSysErr( bool inputFl, int err ){}
void recdAppErr( const cmApDevRecd_t* drp, bool inputFl, int app, int err ){}
void recdRecover( const cmApDevRecd_t* drp, bool inputFl, int err, int line ){}
void recdPrint(){}
void recdFree(){}

#endif

//===================================================================================================


cmApRC_t _cmApOsError( cmApRoot_t* p, int err, const char* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  int msgN = 255;
  char msg[ msgN+1];

  vsnprintf(msg,msgN,fmt,vl);

  if( err )
    cmRptPrintf(p->rpt,"%s ALSA Error:%s. ",msg,snd_strerror(err));
  else
    cmRptPrintf(p->rpt,"%s ",msg);

  cmRptPrintf(p->rpt,"\n");

  va_end(vl);

  return kSysErrApRC;
}

bool _cmApDevSetupError( cmApRoot_t* p, int err, bool inputFl, const cmApDevRecd_t* drp, const char* fmt, ... )
{
  va_list vl;
  int     msgN = 255;
  char    msg[ msgN + 1];

  va_start(vl,fmt);
  vsnprintf(msg,msgN,fmt,vl);
  _cmApOsError(p,err,"%s for %s '%s' : '%s'.",msg,inputFl ? "INPUT" : "OUTPUT", drp->nameStr, drp->descStr);
  va_end(vl);
  return false;
}

const char* _cmApPcmStateToString( snd_pcm_state_t state )
{
  switch( state )
  {
    case SND_PCM_STATE_OPEN:         return "open";
    case SND_PCM_STATE_SETUP:        return "setup";
    case SND_PCM_STATE_PREPARED:     return "prepared";
    case SND_PCM_STATE_RUNNING:      return "running";
    case SND_PCM_STATE_XRUN:         return "xrun";
    case SND_PCM_STATE_DRAINING:     return "draining";
    case SND_PCM_STATE_PAUSED:       return "paused";
    case SND_PCM_STATE_SUSPENDED:    return "suspended";
    case SND_PCM_STATE_DISCONNECTED: return "disconnected";
    case SND_PCM_STATE_PRIVATE1:   return "private1";

  }
  return "<invalid>";
}

void _cmApDevRtReport( cmRpt_t* rpt, cmApDevRecd_t* drp ) 
{
  cmRptPrintf(rpt,"cb i:%i o:%i err i:%i  o:%i",drp->iCbCnt,drp->oCbCnt,drp->iErrCnt,drp->oErrCnt);

  if( drp->iPcmH != NULL )
    cmRptPrintf(rpt," state i:%s",_cmApPcmStateToString(snd_pcm_state(drp->iPcmH)));

  if( drp->oPcmH != NULL )
    cmRptPrintf(rpt," o:%s",_cmApPcmStateToString(snd_pcm_state(drp->oPcmH)));

  cmRptPrint(rpt,"\n ");

}

void _cmApDevReportFormats( cmRpt_t* rpt, snd_pcm_hw_params_t* hwParams )
{
  snd_pcm_format_mask_t* mask;

  snd_pcm_format_t fmt[] =
  {
     SND_PCM_FORMAT_S8,
     SND_PCM_FORMAT_U8,
     SND_PCM_FORMAT_S16_LE,
     SND_PCM_FORMAT_S16_BE,
     SND_PCM_FORMAT_U16_LE,
     SND_PCM_FORMAT_U16_BE,
     SND_PCM_FORMAT_S24_LE,
     SND_PCM_FORMAT_S24_BE,
     SND_PCM_FORMAT_U24_LE,
     SND_PCM_FORMAT_U24_BE,
     SND_PCM_FORMAT_S32_LE,
     SND_PCM_FORMAT_S32_BE,
     SND_PCM_FORMAT_U32_LE,
     SND_PCM_FORMAT_U32_BE,
     SND_PCM_FORMAT_FLOAT_LE,
     SND_PCM_FORMAT_FLOAT_BE,
     SND_PCM_FORMAT_FLOAT64_LE,
     SND_PCM_FORMAT_FLOAT64_BE,
     SND_PCM_FORMAT_IEC958_SUBFRAME_LE,
     SND_PCM_FORMAT_IEC958_SUBFRAME_BE,
     SND_PCM_FORMAT_MU_LAW,
     SND_PCM_FORMAT_A_LAW,
     SND_PCM_FORMAT_IMA_ADPCM,
     SND_PCM_FORMAT_MPEG,
     SND_PCM_FORMAT_GSM,
     SND_PCM_FORMAT_SPECIAL,
     SND_PCM_FORMAT_S24_3LE,
     SND_PCM_FORMAT_S24_3BE,
     SND_PCM_FORMAT_U24_3LE,
     SND_PCM_FORMAT_U24_3BE,
     SND_PCM_FORMAT_S20_3LE,
     SND_PCM_FORMAT_S20_3BE,
     SND_PCM_FORMAT_U20_3LE,
     SND_PCM_FORMAT_U20_3BE,
     SND_PCM_FORMAT_S18_3LE,
     SND_PCM_FORMAT_S18_3BE,
     SND_PCM_FORMAT_U18_3LE,
     SND_PCM_FORMAT_U18_3BE,
     SND_PCM_FORMAT_G723_24,
     SND_PCM_FORMAT_G723_24_1B,
     SND_PCM_FORMAT_G723_40,
     SND_PCM_FORMAT_G723_40_1B,
     SND_PCM_FORMAT_DSD_U8,
     //SND_PCM_FORMAT_DSD_U16_LE,
     //SND_PCM_FORMAT_DSD_U32_LE,
     //SND_PCM_FORMAT_DSD_U16_BE,
     //SND_PCM_FORMAT_DSD_U32_BE,
     SND_PCM_FORMAT_UNKNOWN 
  };

  snd_pcm_format_mask_alloca(&mask);

  snd_pcm_hw_params_get_format_mask(hwParams,mask);

  cmRptPrintf(rpt,"Formats: " );
  
  int i;
  for(i=0; fmt[i]!=SND_PCM_FORMAT_UNKNOWN; ++i)
    if( snd_pcm_format_mask_test(mask, fmt[i] ))
      cmRptPrintf(rpt,"%s%s",snd_pcm_format_name(fmt[i]), snd_pcm_format_cpu_endian(fmt[i]) ? " " : " (swap) ");
  
  cmRptPrintf(rpt,"\n");
  
}

void _cmApDevReport( cmRpt_t* rpt, cmApDevRecd_t* drp )
{
  bool       inputFl = true;
  snd_pcm_t* pcmH;
  int        err;
  unsigned   i;

  cmApRoot_t* p = drp->rootPtr;

  cmRptPrintf(rpt,"%s %s Device:%s Desc:%s\n", drp->flags & kInFl ? "IN ":"", drp->flags & kOutFl ? "OUT ":"", drp->nameStr, drp->descStr);
  
  for(i=0; i<2; i++,inputFl=!inputFl)
  {
    if( ((inputFl==true) && (drp->flags&kInFl)) || (((inputFl==false) && (drp->flags&kOutFl))))
    {
      const char* ioLabel = inputFl ? "In " : "Out";

      // attempt to open the sub-device
      if((err = snd_pcm_open(&pcmH,drp->nameStr,inputFl ? SND_PCM_STREAM_CAPTURE : SND_PCM_STREAM_PLAYBACK,0)) < 0 )
        _cmApDevSetupError(p,err,inputFl,drp,"Attempt to open the PCM handle failed");
      else
      {		
        snd_pcm_hw_params_t* hwParams;
		  
        snd_pcm_hw_params_alloca(&hwParams);
        memset(hwParams,0,snd_pcm_hw_params_sizeof());

        // load the parameter record
        if((err = snd_pcm_hw_params_any(pcmH,hwParams)) < 0 )
          _cmApDevSetupError(p,err,inputFl,drp,"Error obtaining hw param record");
        else
        {
          unsigned minChCnt=0,maxChCnt=0,minSrate=0,maxSrate=0;
          snd_pcm_uframes_t minPeriodFrmCnt=0,maxPeriodFrmCnt=0,minBufFrmCnt=0,maxBufFrmCnt=0;
          int dir;


          // extract the min channel count
          if((err = snd_pcm_hw_params_get_channels_min(hwParams, &minChCnt )) < 0 )
            _cmApDevSetupError(p,err,inputFl,drp,"Error getting min. channel count.");

          // extract the max channel count
          if((err = snd_pcm_hw_params_get_channels_max(hwParams, &maxChCnt )) < 0 )
            _cmApDevSetupError(p,err,inputFl,drp,"Error getting max. channel count.");

          // extract the min srate
          if((err = snd_pcm_hw_params_get_rate_min(hwParams, &minSrate,&dir )) < 0 )
            _cmApDevSetupError(p,err,inputFl,drp,"Error getting min. sample rate.");

          // extract the max srate
          if((err = snd_pcm_hw_params_get_rate_max(hwParams, &maxSrate,&dir )) < 0 )
            _cmApDevSetupError(p,err,inputFl,drp,"Error getting max. sample rate.");

          // extract the min period
          if((err = snd_pcm_hw_params_get_period_size_min(hwParams, &minPeriodFrmCnt,&dir )) < 0 )
            _cmApDevSetupError(p,err,inputFl,drp,"Error getting min. period frame count.");

          // extract the max period
          if((err = snd_pcm_hw_params_get_period_size_max(hwParams, &maxPeriodFrmCnt,&dir )) < 0 )
            _cmApDevSetupError(p,err,inputFl,drp,"Error getting max. period frame count.");

          // extract the min buf
          if((err = snd_pcm_hw_params_get_buffer_size_min(hwParams, &minBufFrmCnt )) < 0 )
            _cmApDevSetupError(p,err,inputFl,drp,"Error getting min. period frame count.");

          // extract the max buffer
          if((err = snd_pcm_hw_params_get_buffer_size_max(hwParams, &maxBufFrmCnt )) < 0 )
            _cmApDevSetupError(p,err,inputFl,drp,"Error getting max. period frame count.");

          cmRptPrintf(rpt,"%s chs:%i - %i srate:%i - %i  period:%i - %i buf:%i - %i half duplex only:%s joint duplex:%s\n",
            ioLabel,minChCnt,maxChCnt,minSrate,maxSrate,minPeriodFrmCnt,maxPeriodFrmCnt,minBufFrmCnt,maxBufFrmCnt,
            (snd_pcm_hw_params_is_half_duplex(hwParams)  ? "yes" : "no"),
            (snd_pcm_hw_params_is_joint_duplex(hwParams) ? "yes" : "no"));
          
          _cmApDevReportFormats( rpt, hwParams );
        }

        if((err = snd_pcm_close(pcmH)) < 0)
          _cmApDevSetupError(p,err,inputFl,drp,"Error closing PCM handle");
      }
    }
  }
}			


// Called by cmApInitialize() to append a cmApDevRecd to the _cmApRoot.devArray[].
void _cmApDevAppend( cmApRoot_t* p, cmApDevRecd_t* drp )
{
  if( p->devCnt == p->devAllocCnt )
  {
    p->devArray     = cmMemResizePZ( cmApDevRecd_t, p->devArray, p->devAllocCnt + INIT_DEV_CNT );
    p->devAllocCnt += INIT_DEV_CNT;
  }

  drp->devIdx  = p->devCnt; // set the device index 
  drp->rootPtr = p;         // set the pointer back to the root

#ifdef IMPULSE_FN
  drp->recdN   = 44100*2*2;
  drp->recdBuf = cmMemAllocZ(int,drp->recdN);
  drp->recdIdx = 0;
#endif

  memcpy(p->devArray + p->devCnt, drp, sizeof(cmApDevRecd_t));
  
  ++p->devCnt;
}

cmApRC_t _cmApDevShutdown( cmApRoot_t* p, cmApDevRecd_t* drp, bool inputFl )
{
  int  err;

  snd_pcm_t** pcmH = inputFl ? &drp->iPcmH : &drp->oPcmH;
  
  if( *pcmH != NULL )
  {
    if((err = snd_pcm_close(*pcmH)) < 0 )
    {
      _cmApDevSetupError(p,err,inputFl,drp,"Error closing device handle.");
      return kSysErrApRC;
    }

    *pcmH = NULL;
  }

  return kOkApRC;
}

int _cmApDevOpen( snd_pcm_t** pcmHPtr, const char* devNameStr, bool inputFl )
{
  int cnt = 0;
  int err;

  do
  {
    if((err = snd_pcm_open(pcmHPtr,devNameStr,inputFl ? SND_PCM_STREAM_CAPTURE : SND_PCM_STREAM_PLAYBACK,0)) < 0 )
    {
      cnt++;
      usleep(10000); // sleep for 10 milliseconds
    }

  }while(cnt<100 && err == -EBUSY );

  return err;
}

void _cmApXrun_recover( snd_pcm_t* pcmH, int err, cmApDevRecd_t* drp, bool inputFl, int line )
{
  char dirCh = inputFl ? 'I' : 'O';

  inputFl ? drp->iErrCnt++ : drp->oErrCnt++;

  recdRecover(drp,inputFl,err,line);

  // -EPIPE signals and over/underrun (see pcm.c example xrun_recovery())
  switch( err )
  {
    case -EPIPE:
      {

        int silentFl = 1;
        if((err = snd_pcm_recover( pcmH, err, silentFl )) < 0 )
          _cmApDevSetupError(drp->rootPtr,err,inputFl,drp,"recover failed.");

        if( inputFl )
        {
          if((err= snd_pcm_prepare(pcmH)) < 0 )
            _cmApDevSetupError(drp->rootPtr,err,inputFl,drp,"re-prepare failed.");
          else
          if((err = snd_pcm_start(pcmH)) < 0 )
            _cmApDevSetupError(drp->rootPtr,err,inputFl,drp,"restart failed.");
        }
        else
        {
          /*
          _cmApWriteBuf(pcmH, NULL, drp->oChCnt, drp->oFpC );
          _cmApWriteBuf(pcmH, NULL, drp->oChCnt, drp->oFpC );

          if((err = snd_pcm_prepare(pcmH))<0)
            _cmApDevSetupError(drp->rootPtr,err,inputFl,drp,"Recovery failed.\n");
          else
            if((err = snd_pcm_resume(pcmH))<0)
              _cmApDevSetupError(drp->rootPtr,err,inputFl,drp,"Resume failed.\n");
          */
        }
        

        printf("EPIPE %c %i %i %i\n",dirCh,drp->devIdx,drp->oCbCnt,line);

        //if((err = snd_pcm_prepare(pcmH))<0)
        //	_devSetupError(err,inputFl,*drp,"Recovery failed.\n");
        //else
        //	if((err = snd_pcm_resume(pcmH))<0)
        //	  _devSetupError(err,inputFl,*drp,"Resume failed.\n");
        break;
      }

    case -ESTRPIPE:
      {
        int silentFl = 1;
        if((err = snd_pcm_recover( pcmH, err, silentFl )) < 0 )
          _cmApDevSetupError(drp->rootPtr,err,inputFl,drp,"recover failed.");

        printf("audio port impl ESTRPIPE:%c\n",dirCh);
        break;
      }

    case -EBADFD:
      {
        _cmApDevSetupError(drp->rootPtr,err,inputFl,drp,"%s failed.",inputFl ? "Read" : "Write" );
        break;
      }

    default:
      _cmApDevSetupError(drp->rootPtr,err,inputFl,drp,"Unknown rd/wr error.\n");

  } // switch

}

void _cmApStateRecover( snd_pcm_t* pcmH, cmApDevRecd_t* drp, bool inputFl  )
{
  int err = 0;

  switch( snd_pcm_state(pcmH))
  {
    case SND_PCM_STATE_XRUN:
      err = -EPIPE;
      break;

    case SND_PCM_STATE_SUSPENDED:
      err = -ESTRPIPE;
      break;

    case SND_PCM_STATE_OPEN:
    case SND_PCM_STATE_SETUP:
    case SND_PCM_STATE_PREPARED:
    case SND_PCM_STATE_RUNNING:
    case SND_PCM_STATE_DRAINING:
    case SND_PCM_STATE_PAUSED:
    case SND_PCM_STATE_DISCONNECTED:
    case SND_PCM_STATE_PRIVATE1:
      //case SND_PCM_STATE_LAST:
      break;
  }

  if( err < 0 )
    _cmApXrun_recover( pcmH, err, drp, inputFl, __LINE__ );
  
}

void _cmApS24_3BE_to_Float( const char* x, cmApSample_t* y, unsigned n )
{
  unsigned i;
  for(i=0; i<n; ++i,x+=3)
  {
    int s = (((int)x[0])<<16) + (((int)x[1])<<8) + (((int)x[2]));
    y[i] = ((cmApSample_t)s)/0x7fffff;
  }
}

void _cmApS24_3BE_from_Float( const cmApSample_t* x, char* y, unsigned n )
{
  unsigned i;
  for(i=0; i<n; ++i)
  {
    int s = x[i] * 0x7fffff;
    y[i*3+2] = (char)((s & 0x7f0000) >> 16);
    y[i*3+1] = (char)((s & 0x00ff00) >>  8);
    y[i*3+0] = (char)((s & 0x0000ff) >>  0);
  }
}


// Returns count of frames written on success or < 0 on error;
// set smpPtr to NULL to write a buffer of silence
int _cmApWriteBuf( cmApDevRecd_t* drp, snd_pcm_t* pcmH, const cmApSample_t* sp, unsigned chCnt, unsigned frmCnt, unsigned bits, unsigned sigBits )
{
  int                 err         = 0;
  unsigned            bytesPerSmp = (bits==24 ? 32 : bits)/8;
  unsigned            smpCnt      = chCnt * frmCnt;
  unsigned            byteCnt     = bytesPerSmp * smpCnt;
  const cmApSample_t* ep          = sp + smpCnt;
  char                obuf[ byteCnt ];
  
  // if no output was given then fill the device buffer with zeros
  if( sp == NULL )
    memset(obuf,0,byteCnt);
  else
  {
    // otherwise convert the floating point samples to integers
    switch( bits )
    {
      case 8:
        {
          char* dp = (char*)obuf;
          while( sp < ep )
            *dp++ = (char)(*sp++ * 0x7f);        
        }
        break;

      case 16:
        {
          short* dp = (short*)obuf;
          while( sp < ep )
          {
            *dp++ = (short)(*sp++ * 0x7fff);
            
          }
            
        }
        break;

      case 24:
        {
          // for use w/ MBox
          //_cmApS24_3BE_from_Float(sp, obuf, ep-sp );
          
          int* dp = (int*)obuf;
          while( sp < ep )
            *dp++ = (int)(*sp++ * 0x7fffff);        
            
        }
        break;

      case 32:
        {
          int* dp = (int*)obuf;

          while( sp < ep )
            *dp++ = (int)(*sp++ * 0x7fffffff);


#ifdef IMPLUSE_FN
          int* tmp = (int*)obuf;
          unsigned ii = 0;
          if( drp->oBufCnt < 3 )
            for(; ii<32; ++ii)
              tmp[ii] = 0x7fffffff;
#endif

        }
        break;
    }
  }


  // send the bytes to the device
  err = snd_pcm_writei( pcmH, obuf, frmCnt );

  ++drp->oBufCnt;
  
  if( err < 0 )
  {
    recdAppErr(drp,false,kWriteErrRId,err);
    _cmApDevSetupError(drp->rootPtr, err, false, drp, "ALSA write error" );
  }
  else
    if( err > 0 && err != frmCnt )
    {
      _cmApDevSetupError(drp->rootPtr, 0, false, drp, "Actual count of bytes written did not match the count provided." );
    }
 

  return err;
}


// Returns frames read on success or < 0 on error.
// Set smpPtr to NULL to read the incoming buffer and discard it
int _cmApReadBuf( cmApDevRecd_t* drp, snd_pcm_t* pcmH, cmApSample_t* smpPtr, unsigned chCnt, unsigned frmCnt, unsigned bits, unsigned sigBits )
{
  int      err         = 0;
  unsigned bytesPerSmp = (bits==24 ? 32 : bits)/8;
  unsigned smpCnt      = chCnt * frmCnt;
  unsigned byteCnt     = smpCnt * bytesPerSmp;

  char     buf[ byteCnt ];

  // get the incoming samples into buf[] ...
  err = snd_pcm_readi(pcmH,buf,frmCnt);

  // if a read error occurred
  if( err < 0 )
  {
    recdAppErr(drp,true,kReadErrRId,err);
    _cmApDevSetupError(drp->rootPtr, err, false, drp, "ALSA read error" );
  }
  else
    if( err > 0 && err != frmCnt )
    {
      _cmApDevSetupError(drp->rootPtr, 0, false, drp, "Actual count of bytes read did not match the count requested." );
    }

  // if no buffer was given then there is nothing else to do 
  if( smpPtr == NULL )
    return err;

  // setup the return buffer
  cmApSample_t* dp = smpPtr;
  cmApSample_t* ep = dp + cmMin(smpCnt,err*chCnt);
  
  switch(bits)
  {
    case 8: 
      {
        char* sp = buf;
        while(dp < ep)
          *dp++ = ((cmApSample_t)*sp++) /  0x7f;
      }
      break;

    case 16:
      {
        short* sp = (short*)buf;
        while(dp < ep)
          *dp++ = ((cmApSample_t)*sp++) /  0x7fff;
      }
      break;

    case 24:
      {
        // For use with MBox
        //_cmApS24_3BE_to_Float(buf, dp, ep-dp );
        int* sp = (int*)buf;
        while(dp < ep)
          *dp++ = ((cmApSample_t)*sp++) /  0x7fffff;
      }
      break;


    case 32:
      {
        int* sp = (int*)buf;
        // The delta1010 (ICE1712) uses only the 24 highest bits according to
        //
        // http://www.alsa-project.org/alsa-doc/alsa-lib/pcm.html
        // <snip> The example: ICE1712 chips support 32-bit sample processing, 
        // but low byte is ignored (playback) or zero (capture). 
        //
        int  mv = sigBits==24 ? 0x7fffff00 : 0x7fffffff;
        while(dp < ep)
          *dp++ = ((cmApSample_t)*sp++) /  mv;

#ifdef IMPULSE_FN
        sp = (int*)buf;
        int* esp = sp + smpCnt;
        for(; sp<esp && drp->recdIdx < drp->recdN; ++drp->recdIdx)
          drp->recdBuf[drp->recdIdx] = *sp++;
#endif

      }
      break;
    default:
      { assert(0); }
  }

  return err;
  
}

void _cmApStaticAsyncHandler( snd_async_handler_t* ahandler )
{ 
  int               err;
  snd_pcm_sframes_t avail;
  cmApDevRecd_t*    drp       = (cmApDevRecd_t*)snd_async_handler_get_callback_private(ahandler);
  snd_pcm_t*        pcmH      = snd_async_handler_get_pcm(ahandler);
  bool              inputFl   = snd_pcm_stream(pcmH) == SND_PCM_STREAM_CAPTURE;
  cmApSample_t*     b         = inputFl ? drp->iBuf   : drp->oBuf;
  unsigned          chCnt     = inputFl ? drp->iChCnt : drp->oChCnt;
  unsigned          frmCnt    = inputFl ? drp->iFpC   : drp->oFpC;
  cmApAudioPacket_t pkt;

  inputFl ? drp->iCbCnt++ : drp->oCbCnt++;

  pkt.devIdx               = drp->devIdx;
  pkt.begChIdx             = 0;
  pkt.chCnt                = chCnt;
  pkt.audioFramesCnt       = frmCnt;
  pkt.bitsPerSample        = 32;
  pkt.flags                = kInterleavedApFl | kFloatApFl;
  pkt.audioBytesPtr        = b;
  pkt.userCbPtr            = drp->userCbPtr;
  
  recdCb(drp,inputFl,0);

  _cmApStateRecover( pcmH, drp, inputFl );
  
  while( (avail = snd_pcm_avail_update(pcmH)) >= (snd_pcm_sframes_t)frmCnt )
  {

    // Handle input
    if( inputFl )
    {
      // read samples from the device
      if((err = _cmApReadBuf(drp,pcmH,drp->iBuf,chCnt,frmCnt,drp->iBits,drp->oBits)) > 0 )
      {
        pkt.audioFramesCnt = err;

        drp->cbPtr(&pkt,1,NULL,0 ); // send the samples to the application
      }
    }

    // Handle output
    else
    {
      // callback to fill the buffer
      drp->cbPtr(NULL,0,&pkt,1);

      // note that the application may return fewer samples than were requested
      err = _cmApWriteBuf(drp, pcmH, pkt.audioFramesCnt < frmCnt ? NULL : drp->oBuf,chCnt,frmCnt,drp->oBits,drp->oSigBits);

    }

    // Handle read/write errors
    if( err < 0 )
    {
      inputFl ? drp->iErrCnt++ : drp->oErrCnt++;
      _cmApXrun_recover( pcmH, err, drp, inputFl, __LINE__ );
    }
    else
    {
      // _cmApStateRecover( pcmH, drp, inputFl );      
    }

  } // while

}

bool _cmApThreadFunc(void* param)
{
  cmApRoot_t* p = (cmApRoot_t*)param;
  int result;
  bool retFl = true;

  switch( result = poll(p->pollfds, p->pollfdsCnt, 250) )
  {
    case 0:
      // time out
      break;

    case -1:
      _cmApOsError(p,errno,"Poll fail.");
      break;

    default:
      {
        assert( result > 0 );
    
        unsigned i;

        // for each i/o stream
        for(i=0; i<p->pollfdsCnt; ++i)
        {
          cmApDevRecd_t*    drp     = p->pollfdsDesc[i].devPtr;
          bool              inputFl = p->pollfdsDesc[i].inputFl;
          snd_pcm_t*        pcmH    = inputFl ? drp->iPcmH  : drp->oPcmH;
          unsigned          chCnt   = inputFl ? drp->iChCnt : drp->oChCnt;
          unsigned          frmCnt  = inputFl ? drp->iFpC   : drp->oFpC;
          cmApSample_t*     b       = inputFl ? drp->iBuf   : drp->oBuf;
          unsigned short    revents = 0;
          int               err;
          cmApAudioPacket_t pkt;
          snd_pcm_uframes_t avail_frames;

          inputFl ? drp->iCbCnt++ : drp->oCbCnt++;
          
          pkt.devIdx               = drp->devIdx;
          pkt.begChIdx             = 0;
          pkt.chCnt                = chCnt;
          pkt.audioFramesCnt       = frmCnt;
          pkt.bitsPerSample        = 32;
          pkt.flags                = kInterleavedApFl | kFloatApFl;
          pkt.audioBytesPtr        = b;
          pkt.userCbPtr            = drp->userCbPtr;

          inputFl ? drp->iCbCnt++ : drp->oCbCnt++;

          // get the timestamp for this buffer
          if((err = snd_pcm_htimestamp(pcmH,&avail_frames,&pkt.timeStamp)) != 0 )
          {
            _cmApDevSetupError(p, err, p->pollfdsDesc[i].inputFl, drp, "Get timestamp error.");
            pkt.timeStamp.tv_sec  = 0;
            pkt.timeStamp.tv_nsec = 0;
          }

          // Note that based on experimenting with the timestamp and the current
          // clock_gettime(CLOCK_MONOTONIC) time it appears that the time stamp
          // marks the end of the current buffer - so in fact the time stamp should
          // be backed up by the availble sample count period to get the time of the 
          // first sample in the buffer
          /*
          unsigned avail_nano_secs = (unsigned)(avail_frames * (1000000000.0/drp->srate));
          if( pkt.timeStamp.tv_nsec > avail_nano_secs )
            pkt.timeStamp.tv_nsec -= avail_nano_secs;
          else
          {
            pkt.timeStamp.tv_sec -= 1;
            pkt.timeStamp.tv_nsec = 1000000000 - avail_nano_secs;
          }
          */

          //printf("AUDI: %ld %ld\n",pkt.timeStamp.tv_sec,pkt.timeStamp.tv_nsec);
          //cmTimeSpec_t t;
          //clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&t);
          //printf("AUDI: %ld %ld\n",t.tv_sec,t.tv_nsec);
          

          switch( snd_pcm_state(pcmH) )
          {
            case SND_PCM_STATE_OPEN:
            case SND_PCM_STATE_SETUP:
            case SND_PCM_STATE_PREPARED:
            case SND_PCM_STATE_DRAINING:
            case SND_PCM_STATE_PAUSED:
            case SND_PCM_STATE_DISCONNECTED:
            case SND_PCM_STATE_PRIVATE1:
              continue;

            case SND_PCM_STATE_RUNNING:
            case SND_PCM_STATE_XRUN:
            case SND_PCM_STATE_SUSPENDED:
              break;
          }

          if(( err = snd_pcm_poll_descriptors_revents(pcmH, p->pollfds + i, 1 , &revents)) != 0 )
          {
            _cmApDevSetupError(p, err, p->pollfdsDesc[i].inputFl, drp, "Return poll events failed.");
            retFl = false;
            goto errLabel;
          }

          if(revents & POLLERR)
          {
            _cmApDevSetupError(p, err, p->pollfdsDesc[i].inputFl, drp, "Poll error.");
            _cmApStateRecover( pcmH, drp, inputFl );    
            //goto errLabel;
          }
          
          if( inputFl && (revents & POLLIN) )
          {
            if((err = _cmApReadBuf(drp,pcmH,drp->iBuf,chCnt,frmCnt,drp->iBits,drp->oBits)) > 0 )
            {
              pkt.audioFramesCnt = err;
              drp->cbPtr(&pkt,1,NULL,0 ); // send the samples to the application

            }
          }

          if( !inputFl && (revents & POLLOUT) )
          {

            /*
            unsigned srate = 96;
            cmTimeSpec_t t1;
            static cmTimeSpec_t t0 = {0,0};
            clock_gettime(CLOCK_MONOTONIC,&t1);

            // time since the time-stamp was generated
            unsigned smp =  (srate * (t1.tv_nsec - pkt.timeStamp.tv_nsec)) / 1000000;

            // time since the last output buffer was sent
            unsigned dsmp = (srate * (t1.tv_nsec - t0.tv_nsec)) / 1000000;
            printf("%i %ld %i : %ld %ld -> %ld %ld\n",smp,avail_frames,dsmp,pkt.timeStamp.tv_sec,pkt.timeStamp.tv_nsec,t1.tv_sec,t1.tv_nsec);          
            t0 = t1;
            */

            // callback to fill the buffer
            drp->cbPtr(NULL,0,&pkt,1);

            // note that the application may return fewer samples than were requested
            err = _cmApWriteBuf(drp, pcmH, pkt.audioFramesCnt < frmCnt ? NULL : drp->oBuf,chCnt,frmCnt,drp->oBits,drp->oSigBits);
            
          }

          
        }
      }
      
  }

 errLabel:
  return retFl;
}


bool _cmApDevSetup( cmApDevRecd_t *drp, unsigned srate, unsigned framesPerCycle, unsigned periodsPerBuf )
{
  int               err;
  int               dir;
  unsigned          i;
  bool              retFl          = true;
  bool              inputFl        = true;
  snd_pcm_uframes_t periodFrameCnt = framesPerCycle;
  snd_pcm_uframes_t bufferFrameCnt;
  unsigned          bits           = 0;
  int               sig_bits       = 0;
  bool              signFl         = true;
  bool              swapFl         = false;
  cmApRoot_t*       p              = drp->rootPtr;

  snd_pcm_format_t fmt[] =
  {
    SND_PCM_FORMAT_S32_LE,
    SND_PCM_FORMAT_S32_BE,
    SND_PCM_FORMAT_S24_LE,
    SND_PCM_FORMAT_S24_BE,
    SND_PCM_FORMAT_S24_3LE,
    SND_PCM_FORMAT_S24_3BE,
    SND_PCM_FORMAT_S16_LE,
    SND_PCM_FORMAT_S16_BE,
  };
  

  // setup input, then output device
  for(i=0; i<2; i++,inputFl=!inputFl)
  {
    unsigned          chCnt  = inputFl ? drp->iChCnt : drp->oChCnt;
    snd_pcm_uframes_t actFpC = 0;

    // if this is the in/out pass and the in/out flag is set
    if( ((inputFl==true) && (drp->flags & kInFl)) || ((inputFl==false) && (drp->flags & kOutFl)) )
    {
      snd_pcm_t* pcmH = NULL;

      if( _cmApDevShutdown(p, drp, inputFl ) != kOkApRC )
        retFl = false;

      // attempt to open the sub-device
      if((err = snd_pcm_open(&pcmH,drp->nameStr, inputFl ? SND_PCM_STREAM_CAPTURE : SND_PCM_STREAM_PLAYBACK, 0)) < 0 )
        retFl = _cmApDevSetupError(p,err,inputFl,drp,"Unable to open the PCM handle");
      else
      {		

        snd_pcm_hw_params_t* hwParams;
        snd_pcm_sw_params_t* swParams;

        // prepare the hwParam recd
        snd_pcm_hw_params_alloca(&hwParams);
        memset(hwParams,0,snd_pcm_hw_params_sizeof());


        // load the hw parameter record
        if((err = snd_pcm_hw_params_any(pcmH,hwParams)) < 0 )
          retFl = _cmApDevSetupError(p,err,inputFl,drp,"Error obtaining hw param record");
        else
        {
          if((err = snd_pcm_hw_params_set_rate_resample(pcmH,hwParams,0)) < 0 )
            retFl = _cmApDevSetupError(p,err,inputFl, drp,"Unable to disable the ALSA sample rate converter.");
		 

          if((err = snd_pcm_hw_params_set_channels(pcmH,hwParams,chCnt)) < 0 )
            retFl = _cmApDevSetupError(p,err,inputFl, drp,"Unable to set channel count to: %i",chCnt);
		 

          if((err = snd_pcm_hw_params_set_rate(pcmH,hwParams,srate,0)) < 0 )
            retFl = _cmApDevSetupError(p,err,inputFl, drp, "Unable to set sample rate to: %i",srate);
		  

          if((err = snd_pcm_hw_params_set_access(pcmH,hwParams,SND_PCM_ACCESS_RW_INTERLEAVED )) < 0 )
            retFl = _cmApDevSetupError(p,err,inputFl, drp, "Unable to set access to: RW Interleaved");
          
          // select the format width
          int j;
          int fmtN = sizeof(fmt)/sizeof(fmt[0]);
          for(j=0; j<fmtN; ++j)
            if((err = snd_pcm_hw_params_set_format(pcmH,hwParams,fmt[j])) >= 0 )
              break;

          if( j == fmtN )
            retFl = _cmApDevSetupError(p,err,inputFl, drp, "Unable to set format to: S16");
          else
          {
            bits = snd_pcm_format_width(fmt[j]); // bits per sample
            signFl = snd_pcm_format_signed(fmt[j]);
            swapFl = !snd_pcm_format_cpu_endian(fmt[j]);
          }
          
          sig_bits = snd_pcm_hw_params_get_sbits(hwParams);

          snd_pcm_uframes_t ps_min,ps_max;
          if((err = snd_pcm_hw_params_get_period_size_min(hwParams,&ps_min,NULL)) < 0 )
            retFl = _cmApDevSetupError(p,err,inputFl, drp, "Unable to get the minimum period size.");

          if((err = snd_pcm_hw_params_get_period_size_max(hwParams,&ps_max,NULL)) < 0 )
            retFl = _cmApDevSetupError(p,err,inputFl, drp, "Unable to get the maximum period size.");

          if( periodFrameCnt < ps_min )
            periodFrameCnt = ps_min;
          else
            if( periodFrameCnt > ps_max )
              periodFrameCnt = ps_max;
		  
          if((err = snd_pcm_hw_params_set_period_size_near(pcmH,hwParams,&periodFrameCnt,NULL)) < 0 )
            retFl = _cmApDevSetupError(p,err,inputFl, drp, "Unable to set period to %i.",periodFrameCnt);

          bufferFrameCnt = periodFrameCnt * periodsPerBuf + 1;

          if((err = snd_pcm_hw_params_set_buffer_size_near(pcmH,hwParams,&bufferFrameCnt)) < 0 )
            retFl = _cmApDevSetupError(p,err,inputFl, drp, "Unable to set buffer to %i.",bufferFrameCnt);


          // Note: snd_pcm_hw_params() automatically calls snd_pcm_prepare()
          if((err = snd_pcm_hw_params(pcmH,hwParams)) < 0 )
            retFl = _cmApDevSetupError(p,err,inputFl, drp, "Parameter application failed.");
		 
		 
          //_reportActualParams( hwParams, inputFl, dr, srate, periodFrameCnt, bufferFrameCnt );
        }

        // prepare the sw param recd
        snd_pcm_sw_params_alloca(&swParams);
        memset(swParams,0,snd_pcm_sw_params_sizeof());

        // load the sw param recd
        if((err = snd_pcm_sw_params_current(pcmH,swParams)) < 0 )
          retFl = _cmApDevSetupError(p,err,inputFl,drp,"Error obtaining sw param record.");
        else
        {

          if((err = snd_pcm_sw_params_set_start_threshold(pcmH,swParams, inputFl ? 0x7fffffff : periodFrameCnt)) < 0 )
            retFl = _cmApDevSetupError(p,err,inputFl,drp,"Error seting the start threshold.");

          // setting the stop-threshold to twice the buffer frame count is intended to stop spurious
          // XRUN states - it will also mean that there will have no direct way of knowing about a
          // in/out buffer over/under run.
          if((err = snd_pcm_sw_params_set_stop_threshold(pcmH,swParams,bufferFrameCnt*2)) < 0 )
            retFl = _cmApDevSetupError(p,err,inputFl,drp,"Error setting the stop threshold.");

          if((err = snd_pcm_sw_params_set_avail_min(pcmH,swParams,periodFrameCnt)) < 0 )
            retFl = _cmApDevSetupError(p,err,inputFl,drp,"Error setting the avail. min. setting.");

          if((err = snd_pcm_sw_params_set_tstamp_mode(pcmH,swParams,SND_PCM_TSTAMP_MMAP)) < 0 )
            retFl = _cmApDevSetupError(p,err,inputFl,drp,"Error setting the time samp mode.");

          if((err = snd_pcm_sw_params(pcmH,swParams)) < 0 )
            retFl = _cmApDevSetupError(p,err,inputFl,drp,"Error applying sw params.");
        }

        // setup the callback
        if( p->asyncFl )
          if((err = snd_async_add_pcm_handler(&drp->ahandler,pcmH,_cmApStaticAsyncHandler, drp )) < 0 )
            retFl = _cmApDevSetupError(p,err,inputFl,drp,"Error assigning callback handler.");

        // get the actual frames per cycle
        if((err = snd_pcm_hw_params_get_period_size(hwParams,&actFpC,&dir)) < 0 )
          retFl = _cmApDevSetupError(p,err,inputFl,drp,"Unable to get the actual period.");
	 
        // store the device handle
        if( inputFl )
        {
          drp->iBits    = bits;
          drp->iSigBits = sig_bits;
          drp->iSignFl  = signFl;
          drp->iSwapFl  = swapFl;
          drp->iPcmH    = pcmH;
          drp->iBuf     = cmMemResizeZ( cmApSample_t, drp->iBuf, actFpC * drp->iChCnt );
          drp->iFpC     = actFpC;
        }		
        else
        {
          drp->oBits    = bits;
          drp->oSigBits = sig_bits;
          drp->oSignFl  = signFl;
          drp->oSwapFl  = swapFl;
          drp->oPcmH    = pcmH;
          drp->oBuf     = cmMemResizeZ( cmApSample_t, drp->oBuf, actFpC * drp->oChCnt );
          drp->oFpC     = actFpC;
        }

        if( p->asyncFl == false )
        {
          assert( p->pollfdsCnt < p->pollfdsAllocCnt );
        
          unsigned incrFdsCnt = 0;
          unsigned fdsCnt     = 0;

          // locate the pollfd associated with this device/direction
          unsigned j;
          for(j=0; j<p->pollfdsCnt; j+=p->pollfdsDesc[j].fdsCnt)
            if( p->pollfdsDesc[j].devPtr == drp && inputFl == p->pollfdsDesc[j].inputFl )
              break;

          // get the count of descriptors for this device/direction
          fdsCnt = snd_pcm_poll_descriptors_count(pcmH);           

          // if the device was not found
          if( j == p->pollfdsCnt )
          {            
            j          = p->pollfdsCnt;
            incrFdsCnt = fdsCnt;

            // if the pollfds[] needs more memroy
            if( p->pollfdsCnt + fdsCnt > p->pollfdsAllocCnt )
            {
              p->pollfds          = cmMemResizePZ(struct pollfd,     p->pollfds,     p->pollfdsCnt + fdsCnt );
              p->pollfdsDesc      = cmMemResizePZ(cmApPollfdsDesc_t, p->pollfdsDesc, p->pollfdsCnt + fdsCnt );
              p->pollfdsAllocCnt += fdsCnt;
            }
          }

          // get the poll descriptors for this device/dir
          if( snd_pcm_poll_descriptors(pcmH,p->pollfds + j,fdsCnt) != 1 )
            retFl = _cmApDevSetupError(p,0,inputFl,drp,"Poll descriptor assignment failed.");
          else
          {
            // store the desc. record assicoated with the poll descriptor
            p->pollfdsDesc[ j ].fdsCnt  = fdsCnt;
            p->pollfdsDesc[ j ].devPtr  = drp;
            p->pollfdsDesc[ j ].inputFl = inputFl;
          }

          p->pollfdsCnt += incrFdsCnt;
        }
        printf("%s %s period:%i %i buffer:%i bits:%i sig_bits:%i\n",inputFl?"in ":"out",drp->nameStr,(unsigned)periodFrameCnt,(unsigned)actFpC,(unsigned)bufferFrameCnt,bits,sig_bits);

      }
      //_dumpAlsaDevice(pcmH);

    } // end if
	
  } // end for 

  return retFl;
}

#ifdef NOTDEF
#define TRY(e) while(e<0){ printf("LINE:%i ALSA ERROR:%s\n",__LINE__,snd_strerror(e)); exit(EXIT_FAILURE); }

void open_device( const char* device_name, bool inputFl )
{
  snd_pcm_t           *pcm_handle  = NULL;
  snd_pcm_hw_params_t *hw_params;


  snd_pcm_uframes_t bs_min,bs_max,ps_min,ps_max;
  unsigned rt_min,rt_max,ch_min,ch_max;
  const char* ioLabel = inputFl ? "in" : "out";


  // Open the device 
  TRY( snd_pcm_open (&pcm_handle, device_name, inputFl ? SND_PCM_STREAM_CAPTURE : SND_PCM_STREAM_PLAYBACK, 0));
  
  TRY( snd_pcm_hw_params_malloc (&hw_params) );
  TRY( snd_pcm_hw_params_any (pcm_handle, hw_params));

  TRY( snd_pcm_hw_params_test_format(pcm_handle, hw_params, SND_PCM_FORMAT_S16_LE));

  // get the sample rate range
  TRY(snd_pcm_hw_params_get_rate_min(hw_params,&rt_min,NULL));
  TRY(snd_pcm_hw_params_get_rate_max(hw_params,&rt_max,NULL));
  TRY(snd_pcm_hw_params_get_channels_min(hw_params,&ch_min));
  TRY(snd_pcm_hw_params_get_channels_max(hw_params,&ch_max));  

  // set the basic device format - setting the format may influence the size of the possible 
  // buffer and period size 
  //TRY( snd_pcm_hw_params_set_access (pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED));
  //TRY( snd_pcm_hw_params_set_format (pcm_handle, hw_params, SND_PCM_FORMAT_S16_LE));
  //TRY( snd_pcm_hw_params_set_rate_near(pcm_handle, hw_params, &srate, NULL));
  //TRY( snd_pcm_hw_params_set_channels(pcm_handle, hw_params, ch_cnt));  


  // get the range of possible buffer and period sizes
  TRY(snd_pcm_hw_params_get_buffer_size_min(hw_params,&bs_min));
  TRY(snd_pcm_hw_params_get_buffer_size_max(hw_params,&bs_max));
  TRY(snd_pcm_hw_params_get_period_size_min(hw_params,&ps_min,NULL));
  TRY(snd_pcm_hw_params_get_period_size_max(hw_params,&ps_max,NULL));

  //printf("%s %s bs min:%u max:%u  ps min:%u max:%u  rate min:%u max:%u  ch min:%u max:%u\n",device_name,ioLabel,bs_min,bs_max,ps_min,ps_max,rt_min,rt_max,ch_min,ch_max);
  printf("%s %s rate min:%u max:%u  ch min:%u max:%u\n",device_name,ioLabel,rt_min,rt_max,ch_min,ch_max);

  snd_pcm_hw_params_free(hw_params);
  snd_pcm_close(pcm_handle);
}
#endif

cmApRC_t      cmApAlsaInitialize( cmRpt_t* rpt, unsigned baseApDevIdx )
{
  cmApRC_t rc      = kOkApRC;
  int      err;
  int      cardNum = -1;

  if((rc = cmApAlsaFinalize()) != kOkApRC )
    return rc;

  recdSetup();

  cmApRoot_t* p  = &_cmApRoot;
  memset(p,0,sizeof(cmApRoot_t));
  p->rpt         = rpt;
  p->asyncFl     = false;

  // for each sound card
  while(1)
  {
    snd_ctl_t* cardH;
    char*      cardNamePtr     = NULL;
    char*      cardLongNamePtr = NULL;
    int        devNum          = -1;
    int        devStrN         = 31;
    char       devStr[devStrN+1];


    // get the next card handle
    if((err = snd_card_next(&cardNum)) < 0 )
    {
      _cmApOsError(p,err,"Error getting sound card handle");
      return kSysErrApRC;
    }

    // if no more card's to get
    if( cardNum < 0 )
      break;

    // get the card short name
    if(((err = snd_card_get_name(cardNum,&cardNamePtr)) < 0) || (cardNamePtr == NULL))
    {
      _cmApOsError(p,err,"Unable to get card name for card number %i\n",cardNum);
      goto releaseCard;
    }

    // get the card long name
    if((err = snd_card_get_longname(cardNum,&cardLongNamePtr)) < 0 || cardLongNamePtr == NULL )
    {
      _cmApOsError(p,err,"Unable to get long card name for card number %i\n",cardNum);
      goto releaseCard;
    }
           
    // form the device name for this card
    if(snprintf(devStr,devStrN,"hw:%i",cardNum) > devStrN )
    {
      _cmApOsError(p,0,"Device name is too long for buffer.");
      goto releaseCard;
    }

    // open the card device driver
    if((err = snd_ctl_open(&cardH, devStr, 0)) < 0 )
    {
      _cmApOsError(p,err,"Error opening sound card %i.",cardNum);
      goto releaseCard;
    }
        

    // for each device on this card
    while(1)
    {
      snd_pcm_info_t* info; 
      int             subDevCnt = 1;
      int             i,j;

      // get the next device on this card
      if((err = snd_ctl_pcm_next_device(cardH,&devNum)) < 0 )
      {
        _cmApOsError(p,err,"Error gettign next device on card %i",cardNum);
        break;
      }

      // if no more devices to get
      if( devNum < 0 )
        break;

      // allocate a pcmInfo record
      snd_pcm_info_alloca(&info);
      memset(info, 0, snd_pcm_info_sizeof());

      // set the device  to query
      snd_pcm_info_set_device(info, devNum );

      for(i=0; i<subDevCnt; i++)
      {
        cmApDevRecd_t dr;
        bool        inputFl = false;

        memset(&dr,0,sizeof(dr));

        for(j=0; j<2; j++,inputFl=!inputFl)
        {
          snd_pcm_t* pcmH = NULL;

          dr.devIdx = -1;

          // set the subdevice and I/O direction to query
          snd_pcm_info_set_subdevice(info,i);
          snd_pcm_info_set_stream(info,inputFl ? SND_PCM_STREAM_CAPTURE : SND_PCM_STREAM_PLAYBACK);

          // if this device does not use this sub-device
          if((err = snd_ctl_pcm_info(cardH,info)) < 0 )
            continue;                   

          // get the count of subdevices this device uses
          if(i == 0 )
            subDevCnt = snd_pcm_info_get_subdevices_count(info);
                
          // if this device has no sub-devices
          if(subDevCnt == 0 )
            continue;

          // form the device name and desc. string
          if(strlen(dr.nameStr) == 0)
            snprintf(dr.nameStr,NAME_CHAR_CNT,"hw:%i,%i,%i",cardNum,devNum,i);


          if(strlen(dr.descStr) == 0)
          {
            snprintf(dr.descStr,DESC_CHAR_CNT,"%s %s",cardNamePtr,snd_pcm_info_get_name(info));
            //snprintf(dr.descStr,DESC_CHAR_CNT,"Name:%s Card:[%s] [%s] Device:%s Subdevice:%s",dr.nameStr, cardNamePtr,cardLongNamePtr,snd_pcm_info_get_id(info),snd_pcm_info_get_name(info));
          }

          // attempt to open the sub-device
          if((err = _cmApDevOpen(&pcmH,dr.nameStr,inputFl)) < 0 )
            _cmApDevSetupError(p,err,inputFl,&dr,"Unable to open the PCM handle");
          else
          {
            snd_pcm_hw_params_t* hwParams;

            // allocate the parameter record
            snd_pcm_hw_params_alloca(&hwParams);
            memset( hwParams,0,snd_pcm_hw_params_sizeof());

                        
            // load the parameter record
            if((err = snd_pcm_hw_params_any(pcmH,hwParams)) < 0 )
              _cmApDevSetupError(p,err,inputFl,&dr,"Error obtaining hw param record");
            else
            {
              unsigned* chCntPtr = inputFl ? &dr.iChCnt : &dr.oChCnt;
              unsigned rate;

              snd_pcm_hw_params_get_rate_max(hwParams,&rate,NULL);
              

              // extract the channel count
              if((err = snd_pcm_hw_params_get_channels_max(hwParams, chCntPtr )) < 0 )
                _cmApDevSetupError(p,err,inputFl,&dr,"Error getting channel count.");
              else
                // this device uses this subdevice in the current direction 
                dr.flags += inputFl ? kInFl : kOutFl;

              //printf("%s in:%i chs:%i rate:%i\n",dr.nameStr,inputFl,*chCntPtr,rate);
              
            }                        

            // close the sub-device
            snd_pcm_close(pcmH);

          } 
                  
        } // in/out loop

        // insert the device in the device array
        if( dr.flags != 0 )
          _cmApDevAppend(p,&dr);

      } // sub-dev loop
                  
    } // device loop

  releaseCard:
    snd_ctl_close(cardH);

  } // card loop


  if( rc == kOkApRC && p->asyncFl==false )
  {
    p->pollfdsCnt      = 0;
    p->pollfdsAllocCnt = 2*p->devCnt;
    p->pollfds         = cmMemAllocZ(struct pollfd,     p->pollfdsAllocCnt );
    p->pollfdsDesc     = cmMemAllocZ(cmApPollfdsDesc_t, p->pollfdsAllocCnt );

    if( cmThreadCreate(&p->thH,_cmApThreadFunc,p,rpt) != kOkThRC )
    {
      _cmApOsError(p,0,"Thread create failed.");
      rc = kThreadFailApRC;
    }
  }

  return rc;
}

cmApRC_t      cmApAlsaFinalize()
{
  cmApRoot_t* p = &_cmApRoot;
  int         i;
  cmApRC_t    rc = kOkApRC;

  if( p->asyncFl==false && cmThreadIsValid(p->thH) )
    if( cmThreadDestroy(&p->thH) != kOkThRC )
    {
      _cmApOsError(p,0,"Thread destroy failed.");
      rc = kThreadFailApRC;
    }

  for(i=0; i<p->devCnt; ++i)
  {
    _cmApDevShutdown(p,p->devArray+i,true);
    _cmApDevShutdown(p,p->devArray+i,false);

#ifdef IMPULSE_FN
    if( p->devArray[i].recdIdx > 0 )
    {
      const char* fn = "/home/kevin/temp/recd0.txt";
      FILE* fp = fopen(fn,"wt");
      if( fp != NULL )
      {
        unsigned j;
        for(j=0; j<p->devArray[i].recdIdx; ++j)
          fprintf(fp,"%i\n",p->devArray[i].recdBuf[j]);
        fclose(fp);
      }
    }
    cmMemFree(p->devArray[i].recdBuf);
#endif

    cmMemPtrFree(&p->devArray[i].iBuf);
    cmMemPtrFree(&p->devArray[i].oBuf);
  
  }

  cmMemPtrFree(&p->pollfds);
  cmMemPtrFree(&p->pollfdsDesc);

  cmMemPtrFree(&p->devArray);
  p->devAllocCnt = 0;
  p->devCnt      = 0;

  recdFree();
  //write_rec(2);


  return rc;
}

cmApRC_t      cmApAlsaDeviceCount()
{ return _cmApRoot.devCnt;  }

const char* cmApAlsaDeviceLabel( unsigned devIdx )
{
  assert(devIdx < cmApAlsaDeviceCount());
  return _cmApRoot.devArray[devIdx].descStr;  
}

unsigned    cmApAlsaDeviceChannelCount( unsigned devIdx, bool inputFl )
{
  assert(devIdx < cmApAlsaDeviceCount());
  return inputFl ? _cmApRoot.devArray[devIdx].iChCnt : _cmApRoot.devArray[devIdx].oChCnt;  
}

double      cmApAlsaDeviceSampleRate( unsigned devIdx )
{
  assert(devIdx < cmApAlsaDeviceCount());
  return (double)_cmApRoot.devArray[devIdx].srate;
}

unsigned    cmApAlsaDeviceFramesPerCycle( unsigned devIdx, bool inputFl )
{
  assert(devIdx < cmApAlsaDeviceCount());
  return _cmApRoot.devArray[devIdx].framesPerCycle;
}

cmApRC_t      cmApAlsaDeviceSetup(          
  unsigned          devIdx, 
  double            srate, 
  unsigned          framesPerCycle, 
  cmApCallbackPtr_t callbackPtr,
  void*             userCbPtr )
{
  assert( devIdx < cmApAlsaDeviceCount());
  cmApRoot_t*    p             = &_cmApRoot;
  cmApDevRecd_t* drp           = _cmApRoot.devArray + devIdx;
  unsigned       periodsPerBuf = kDfltPeriodsPerBuf;

  if( p->asyncFl == false )
    if( cmThreadPause(p->thH,kWaitThFl | kPauseThFl) != kOkThRC )
    {
      _cmApOsError(p,0,"Audio thread pause failed.");
      return kThreadFailApRC;
    }

  if( _cmApDevSetup(drp, srate, framesPerCycle, periodsPerBuf ) )
  {
    drp->srate          = srate;
    drp->framesPerCycle = framesPerCycle;
    drp->periodsPerBuf  = periodsPerBuf;
    drp->cbPtr          = callbackPtr;
    drp->userCbPtr      = userCbPtr;
    
    return kOkApRC;
  }
		  
  return kSysErrApRC;  
}


cmApRC_t      cmApAlsaDeviceStart( unsigned devIdx )
{
  assert( devIdx < cmApAlsaDeviceCount());
  int err;
  cmApRoot_t*    p       = &_cmApRoot;
  cmApDevRecd_t* drp     = p->devArray + devIdx;
  bool           retFl   = true;
  bool           inputFl = true;
  unsigned       i;

  for(i=0; i<2; ++i,inputFl=!inputFl)
  {
    snd_pcm_t* pcmH = inputFl ? drp->iPcmH : drp->oPcmH;

    if(  pcmH != NULL )	
    {

      snd_pcm_state_t state = snd_pcm_state(pcmH);

      if( state != SND_PCM_STATE_RUNNING )
      {
        unsigned    chCnt   = inputFl ? drp->iChCnt : drp->oChCnt;
        unsigned    frmCnt  = inputFl ? drp->iFpC   : drp->oFpC;
        const char* ioLabel = inputFl ? "Input"     : "Output";
	  
        //printf("%i %s state:%s %i %i\n",drp->devIdx, ioLabel,_pcmStateToString(snd_pcm_state(pcmH)),chCnt,frmCnt);

        // preparing may not always be necessary because the earlier call to snd_pcm_hw_params()
        // may have left the device prepared - the redundant call however doesn't seem to hurt
        if((err= snd_pcm_prepare(pcmH)) < 0 )
        	retFl = _cmApDevSetupError(p,err,inputFl,drp,"Error preparing the %i device.",ioLabel);
        else
        {

          recdStart(drp,inputFl);

          if( inputFl == false )
          {
            int j;
            for(j=0; j<1; ++j)
              if((err = _cmApWriteBuf( drp, pcmH, NULL, chCnt, frmCnt, drp->oBits, drp->oSigBits )) < 0 )
              {
                retFl = _cmApDevSetupError(p,err,inputFl,drp,"Write before start failed.");
                break;
              }
          }
          else
          {
          
            if((err = snd_pcm_start(pcmH)) < 0 )
              retFl = _cmApDevSetupError(p,err,inputFl,drp,"'%s' start failed.",ioLabel);
          }

          // wait 500 microseconds between starting and stopping - this prevents
          // input and output and other device callbacks from landing on top of
          // each other - when this happens callbacks are dropped.
          if( p->asyncFl )
            usleep(500);

        }
        //printf("%i %s state:%s %i %i\n",drp->devIdx, ioLabel,_cmApPcmStateToString(snd_pcm_state(pcmH)),chCnt,frmCnt);

      }
	  
    }
  }

  if( p->asyncFl == false )
    if( cmThreadPause(p->thH,0) != kOkThRC )
    {
      _cmApOsError(p,0,"Audio thread start failed.");
      retFl = false;
    }

  return retFl ? kOkApRC : kSysErrApRC;
}

cmApRC_t      cmApAlsaDeviceStop( unsigned devIdx )
{
  int            err;
  bool           retFl = true;
  cmApRoot_t*    p     = &_cmApRoot;
  cmApDevRecd_t* drp   = p->devArray + devIdx;

  if( drp->iPcmH != NULL )
	if((err = snd_pcm_drop(drp->iPcmH)) < 0 )
	  retFl = _cmApDevSetupError(p,err,true,drp,"Input stop failed.");

  if( drp->oPcmH != NULL )
	if((err = snd_pcm_drop(drp->oPcmH)) < 0 )
	  retFl = _cmApDevSetupError(p,err,false,drp,"Output stop failed.");

  if( p->asyncFl == false )
    if( cmThreadPause(p->thH,kPauseThFl) != kOkThRC )
    {
      _cmApOsError(p,0,"Audio thread pause failed.");
      retFl = false;
    }

  return retFl ? kOkApRC : kSysErrApRC;
}

bool        cmApAlsaDeviceIsStarted( unsigned devIdx )
{
  assert( devIdx < cmApAlsaDeviceCount());

  bool                 iFl = false;
  bool                 oFl = false;
  const cmApDevRecd_t* drp = _cmApRoot.devArray + devIdx;

  if( drp->iPcmH != NULL )
    iFl = snd_pcm_state(drp->iPcmH) == SND_PCM_STATE_RUNNING;
	
  if( drp->oPcmH != NULL )
    oFl = snd_pcm_state(drp->oPcmH) == SND_PCM_STATE_RUNNING;
	  
  return iFl || oFl;
}

//{ { label:alsaDevRpt  }
//(
//  Here's an example of generating a report of available
//  ALSA devices.
//)

//[
void	cmApAlsaDeviceReport( cmRpt_t* rpt ) 
{
  unsigned i;
  for(i=0; i<_cmApRoot.devCnt; i++)
  {
    cmRptPrintf(rpt,"%i : ",i);
    _cmApDevReport(rpt,_cmApRoot.devArray + i );
  }
}
//]
//}

void cmApAlsaDeviceRtReport( cmRpt_t* rpt, unsigned devIdx )
{
  _cmApDevRtReport(rpt, _cmApRoot.devArray + devIdx ); 
  
}
