#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmTime.h"
#include "cmMidi.h"


enum
{
  mdStatusDescLabelCharCnt = 5
};

typedef struct
{
  cmMidiByte_t   status;
  cmMidiByte_t  byteCnt;
  char     label[ mdStatusDescLabelCharCnt+1 ];
} cmMidiStatusDesc;

cmMidiStatusDesc _cmMidiStatusDescArray[] =
{
  // channel messages
	{ kNoteOffMdId,	2, "nof" },
	{	kNoteOnMdId,	2, "non" },
	{	kPolyPresMdId,2, "ppr" },
	{	kCtlMdId,		  2, "ctl" },
	{	kPgmMdId,		  1, "pgm" },
	{	kChPresMdId,	1, "cpr" },
	{	kPbendMdId,		2, "pb"  },


	{	kSysExMdId,		kInvalidMidiByte,"sex" },

  // system common
  { kSysComMtcMdId,    1, "mtc" },
  { kSysComSppMdId,    2, "spp" },
  { kSysComSelMdId,    1, "sel" },
  { kSysComUndef0MdId, 0, "cu0" },
  { kSysComUndef1MdId, 0, "cu1" },
  { kSysComTuneMdId,   0, "tun" },
  { kSysComEoxMdId,    0, "eox" },

  // system real-time
  { kSysRtClockMdId, 0, "clk" },
  { kSysRtUndef0MdId,0, "ud0" },
  { kSysRtStartMdId, 0, "beg" },
  { kSysRtContMdId,  0, "cnt" },
  { kSysRtStopMdId,  0, "end" },
  { kSysRtUndef1MdId,0, "ud1" },
  { kSysRtSenseMdId, 0, "sns" },
  { kSysRtResetMdId, 0, "rst" },

 	{ kInvalidStatusMdId,  kInvalidMidiByte, "ERR" }
};

cmMidiStatusDesc _cmMidiMetaStatusDescArray[] =
{
  { kSeqNumbMdId,    2, "seqn"  },
  { kTextMdId,      -1, "text"  },
  { kCopyMdId,      -1, "copy"  },
  { kTrkNameMdId,   -1, "name"  },
  { kInstrNameMdId, -1, "instr" },
  { kLyricsMdId,    -1, "lyric" },
  { kMarkerMdId,    -1, "mark"  },
  { kCuePointMdId,  -1, "cue"   },
  { kMidiChMdId,     1, "chan"  },
  { kEndOfTrkMdId,   0, "eot"   },
  { kTempoMdId,      3, "tempo" },
  { kSmpteMdId,      5, "smpte" },
  { kTimeSigMdId,    4, "tsig"  },
  { kKeySigMdId,     2, "ksig"  },
  { kSeqSpecMdId,   -1, "seqs"  },
  { kInvalidMetaMdId, kInvalidMidiByte, "ERROR"}
};

//====================================================================================================

const char* cmMidiStatusToLabel( cmMidiByte_t status )
{
  unsigned i;

  if( !cmMidiIsStatus(status) )
    return NULL;

  // remove the channel value from ch msg status bytes
  if( cmMidiIsChStatus(status) )
    status &= 0xf0;

  for(i=0; _cmMidiStatusDescArray[i].status != kInvalidStatusMdId; ++i)
    if( _cmMidiStatusDescArray[i].status == status )
      return _cmMidiStatusDescArray[i].label;

  return _cmMidiStatusDescArray[i].label; 
}

const char*   cmMidiMetaStatusToLabel( cmMidiByte_t metaStatus )
{
  int i;
  for(i=0; _cmMidiMetaStatusDescArray[i].status != kInvalidMetaMdId; ++i)
    if( _cmMidiMetaStatusDescArray[i].status == metaStatus )
      break;

  return _cmMidiMetaStatusDescArray[i].label; 
}

cmMidiByte_t cmMidiStatusToByteCount( cmMidiByte_t status )
{
  unsigned i;

  if( !cmMidiIsStatus(status) )
    return kInvalidMidiByte;

  // remove the channel value from ch msg status bytes
  if( cmMidiIsChStatus(status) )
    status &= 0xf0;

  for(i=0; _cmMidiStatusDescArray[i].status != kInvalidStatusMdId; ++i)
    if( _cmMidiStatusDescArray[i].status == status )
      return _cmMidiStatusDescArray[i].byteCnt;

  assert(0);

  return 0; 
}

unsigned      cmMidiTo14Bits( cmMidiByte_t d0, cmMidiByte_t d1 )
{
  unsigned val = d0;
  val <<= 7;
  val += d1;
  return val;
}

void          cmMidiSplit14Bits( unsigned v, cmMidiByte_t* d0Ref, cmMidiByte_t* d1Ref )
{
  *d0Ref = (v & 0x3f80) >> 7;
  *d1Ref = v & 0x7f;
}

int           cmMidiToPbend(  cmMidiByte_t d0, cmMidiByte_t d1 )
{
  int v = cmMidiTo14Bits(d0,d1);
  return v - 8192;
}

void          cmMidiSplitPbend( int v, cmMidiByte_t* d0Ref, cmMidiByte_t* d1Ref )
{
  unsigned uv = v + 8192;
  cmMidiSplit14Bits(uv,d0Ref,d1Ref);
}

//====================================================================================================
const char*     cmMidiToSciPitch( cmMidiByte_t pitch, char* label, unsigned labelCharCnt )
{
  static char buf[ kMidiSciPitchCharCnt ];

  if( label == NULL || labelCharCnt == 0 )
  {
    label = buf;
    labelCharCnt = kMidiSciPitchCharCnt;
  }

  assert( labelCharCnt >= kMidiSciPitchCharCnt );

  if( /*pitch < 0 ||*/ pitch > 127 )
  {
    label[0] = 0;
    return label;
  }

  assert( labelCharCnt >= 5 && /*pitch >= 0 &&*/ pitch <= 127 );

  char     noteV[]      =  { 'C', 'C', 'D', 'D', 'E', 'F', 'F', 'G', 'G', 'A', 'A', 'B' };
  char     shrpV[]      =  { ' ', '#', ' ', '#', ' ', ' ', '#', ' ', '#', ' ', '#', ' ' };
  int      octave       =  (pitch / 12)-1;
  unsigned noteIdx      =  pitch % 12;
  char     noteCh       =  noteV[ noteIdx ];
  char     sharpCh      =  shrpV[ noteIdx ];
  unsigned idx          =  1;

  label[labelCharCnt-1] = 0;
  label[0]              = noteCh;

  if( sharpCh != ' ' )
  {
    label[1] = sharpCh;
    idx      = 2;
  }

  assert( -1 <= octave && octave <= 9);

  snprintf(label+idx,kMidiSciPitchCharCnt-idx-1,"%i",octave);

  return label;
}


cmMidiByte_t    cmSciPitchToMidi( const char* sciPitchStr )
{
  const char* cp      = sciPitchStr;
  bool        sharpFl = false;
  bool        flatFl  = false;
  int         octave;
  int         idx     = -1;

  if( sciPitchStr==NULL || strlen(sciPitchStr) > 5 )
    return kInvalidMidiPitch;

  switch(tolower(*cp))
  {
    case 'a': idx = 9;  break;
    case 'b': idx = 11; break;
    case 'c': idx = 0;  break;
    case 'd': idx = 2;  break;
    case 'e': idx = 4;  break;
    case 'f': idx = 5;  break;
    case 'g': idx = 7;  break;
    default:
      return kInvalidMidiPitch;
  }

  ++cp;

  if( !(*cp) )
    return kInvalidMidiPitch;

  
  if((sharpFl = *cp=='#') == true )
    ++idx;
  else
    if((flatFl  = *cp=='b') == true )
      --idx;

  if( sharpFl || flatFl )
  {
    ++cp;

    if( !(*cp) )
      return kInvalidMidiPitch;
  }
  
  if( isdigit(*cp) == false && *cp!='-' )
    return kInvalidMidiPitch;

  octave = atoi(cp);

  unsigned rv =  (octave*12) + idx + 12;
  
  if( 0 <= rv && rv <= 127 )
    return rv;

  return kInvalidMidiPitch;
  
}
