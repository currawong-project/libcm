#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmComplexTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmXml.h"
#include "cmText.h"
#include "cmFileSys.h"
#include "cmXScore.h"
#include "cmTime.h"
#include "cmMidi.h"
#include "cmMidiFile.h"
#include "cmLex.h"
#include "cmCsv.h"
#include "cmSymTbl.h"
#include "cmScore.h"


#include "cmFile.h"
#include "cmSymTbl.h"
#include "cmAudioFile.h"
#include "cmAudioFile.h"
#include "cmProcObj.h"
#include "cmProcTemplate.h"
#include "cmProc.h"
#include "cmProc2.h"
#include "cmProc5.h"

#include "cmSvgWriter.h"



cmXsH_t cmXsNullHandle = cmSTATIC_NULL_HANDLE;

enum
{
  kSectionXsFl     = 0x00000001,  // rvalue holds section number
  kBarXsFl         = 0x00000002,
  kRestXsFl        = 0x00000004,
  kGraceXsFl       = 0x00000008,
  kDotXsFl         = 0x00000010,
  kChordXsFl       = 0x00000020,
  kDynXsFl         = 0x00000040,
  kEvenXsFl        = 0x00000080,
  kTempoXsFl       = 0x00000100,
  kHeelXsFl        = 0x00000200,
  kTieBegXsFl      = 0x00000400,
  kTieEndXsFl      = 0x00000800,
  kTieProcXsFl     = 0x00001000,
  kDampDnXsFl      = 0x00002000,
  kDampUpXsFl      = 0x00004000,
  kDampUpDnXsFl    = 0x00008000,
  kSostDnXsFl      = 0x00010000,
  kSostUpXsFl      = 0x00020000,
  kMetronomeXsFl   = 0x00040000,  // duration holds BPM
  kOnsetXsFl       = 0x00080000,  // this is a sounding note
  kBegGroupXsFl    = 0x00100000,
  kEndGroupXsFl    = 0x00200000,
  kBegGraceXsFl    = 0x00400000,  // (b) beg grace note group
  kEndGraceXsFl    = 0x00800000,  //     end grace note group
  kAddGraceXsFl    = 0x01000000,  // (a) end grace note group operator flag - add time
  kSubGraceXsFl    = 0x02000000,  // (s)  "    "    "     "      "       "  - subtract time
  kAFirstGraceXsFl = 0x04000000,  // (A) add time after first note
  kNFirstGraceXsFl = 0x08000000,  // (n) grace notes start as soon as possible after first note and add time
  kDeleteXsFl      = 0x10000000,
  kDynBegForkXsFl  = 0x20000000,
  kDynEndForkXsFl  = 0x40000000,

  kDynEndXsFl   = 0x100000000,
  kEvenEndXsFl  = 0x200000000,
  kTempoEndXsFl = 0x400000000
  
};

struct cmXsMeas_str;
struct cmXsVoice_str;

// Values measured for each sounding note in the preceding time window....
typedef struct cmXsComplexity_str
{
  unsigned sum_d_vel;   // sum of first order difference of cmXsNote_t.dynamics 
  unsigned sum_d_rym;   // sum of first order difference of cmXsNote_t.rvalue 
  unsigned sum_d_lpch;  // sum of first order difference of cmXsNote_t.pitch of note assigned to the bass cleff
  unsigned sum_n_lpch;  // count of notes assigned to the bass cleff
  unsigned sum_d_rpch;  // sum of first order difference of cmXsNote_t.pitch of note assigned to the treble cleff 
  unsigned sum_n_rpch;  // count of notes assigned to the treble cleff
} cmXsComplexity_t;

typedef struct cmXsNote_str
{
  unsigned                    uid;      // unique id of this note record
  unsigned long long          flags;    // See k???XsFl
  unsigned                    pitch;    // midi pitch
  unsigned                    dynamics; // dynamic level 1=pppp 9=fff
  unsigned                    vel;      // score specified MIDI velocity 
  cmChar_t                    step;     // A-G
  unsigned                    octave;   // sci pitch octave
  int                         alter;    // +n=sharps,-n=flats
  unsigned                    staff;    // 1=treble 2=bass
  unsigned                    tick;     //
  unsigned                    duration; // duration in ticks
  unsigned                    tied_dur; // duration in ticks (including all tied notes)
  double                      secs;     // absolute time in seconds
  double                      dsecs;    // delta time in seconds since previous event
  unsigned                    locIdx;   // location index (chords share the same location index)
  double                      rvalue;   // 1/rvalue = rythmic value (1/0.5 double whole 1/1 whole 1/2 half 1/4=quarter note, 1/8=eighth note, ...)
  const cmChar_t*             tvalue;   // text value

  unsigned                    evenGroupId;   // eveness group id
  unsigned                    dynGroupId;    // dynamics group id
  unsigned                    tempoGroupId;  // tempo group id
  unsigned                    graceGroupId;  // grace note group id

  struct cmXsVoice_str*       voice;    // voice this note belongs to
  struct cmXsMeas_str*        meas;     // measure this note belongs to

  const cmXmlNode_t*          xmlNode;  // note xml ptr

  struct cmXsNote_str*        tied;     // subsequent note tied to this note
  struct cmXsNote_str*        grace;    // grace note groups link backward in time from the anchor note
 
  struct cmXsNote_str*        mlink;    // measure note list
  struct cmXsNote_str*        slink;    // time sorted event list

  cmXsComplexity_t            cplx;

} cmXsNote_t;

typedef struct cmXsVoice_str
{
  unsigned              id;    // Voice id
  cmXsNote_t*           noteL; // List of notes in this voice
  struct cmXsVoice_str* link;  // Link to other voices in this measure
} cmXsVoice_t;

typedef struct cmXsSpan_str
{
  unsigned             staff;     
  unsigned             number;
  struct cmXsMeas_str* meas;
  unsigned             tick0;
  unsigned             tick1;
  int                  pitch_offset;
  struct cmXsSpan_str* link;
} cmXsSpan_t;

typedef struct cmXsMeas_str
{
  unsigned number;      // Measure number
  unsigned divisions;   // ticks-per-quarter-note
  unsigned beats;       // beats per measure
  unsigned beat_type;   // whole/half/quarter/eighth ...

  cmXsVoice_t* voiceL;  // List of voices in this measure
  cmXsNote_t*  noteL;   // List of time sorted notes in this measure

  struct cmXsMeas_str* link;    // Link to other measures in this part.
} cmXsMeas_t;

typedef struct cmXsPart_str
{
  const cmChar_t*      idStr;   // Id of this part
  cmXsMeas_t*          measL;   // List of measures in this part.
  struct cmXsPart_str* link;    // Link to other parts in this score
} cmXsPart_t;

typedef struct
{
  cmErr_t     err;
  cmXmlH_t    xmlH;
  cmLHeapH_t  lhH;
  cmXsPart_t* partL;
  cmCsvH_t    csvH;

  cmXsSpan_t* spanL;
  unsigned    nextUid;

  cmXsComplexity_t cplx_max;
} cmXScore_t;

void  _cmXScoreReport( cmXScore_t* p, cmRpt_t* rpt, bool sortFl );

cmXScore_t* _cmXScoreHandleToPtr( cmXsH_t h )
{
  cmXScore_t* p = (cmXScore_t*)h.h;
  assert( p != NULL );
  return p;
}

cmXsRC_t _cmXScoreFinalize( cmXScore_t* p )
{
  cmXsRC_t rc = kOkXsRC;

  // release the XML file
  cmXmlFree( &p->xmlH );

  // release the local linked heap memory
  cmLHeapDestroy(&p->lhH);

  // release the CSV output object
  cmCsvFinalize(&p->csvH);

  cmMemFree(p);

  return rc;
}

cmXsRC_t _cmXScoreMissingNode( cmXScore_t* p, const cmXmlNode_t* parent, const cmChar_t* label )
{
  return cmErrMsg(&p->err,kSyntaxErrorXsRC,"Missing XML node '%s'. Parent line:%i",label,parent->line);
}

cmXsRC_t _cmXScoreMissingAttribute( cmXScore_t* p, const cmXmlNode_t* np, const cmChar_t* attrLabel )
{
  return cmErrMsg(&p->err,kSyntaxErrorXsRC,"Missing XML attribute '%s' from node '%s'.",attrLabel,np->label);
}

cmXsVoice_t* _cmXScoreIdToVoice( cmXsMeas_t* meas, unsigned voiceId )
{
  cmXsVoice_t* v = meas->voiceL;
  for(; v!=NULL; v=v->link)
    if( v->id == voiceId )
      return v;

  return NULL;
}

cmXsRC_t _cmXScorePushNote( cmXScore_t* p, cmXsMeas_t* meas, unsigned voiceId, cmXsNote_t* note )
{
  cmXsVoice_t* v;

  // get the voice recd
  if((v = _cmXScoreIdToVoice(meas,voiceId)) == NULL)
  {
    // the voice recd doesn't exist for this voiceId - allocate one
    v = cmLhAllocZ(p->lhH,cmXsVoice_t,1);
    v->id = voiceId;

    // add the voice record to the meas->voiceL
    if( meas->voiceL == NULL )
      meas->voiceL = v;
    else
    {
      cmXsVoice_t* vp =  meas->voiceL;
      while( vp->link!=NULL )
        vp = vp->link;

      vp->link = v;
    }
  }

  // add the note record to the end of meas->voiceL
  if( v->noteL == NULL )
    v->noteL = note;
  else
  {
    cmXsNote_t* n = v->noteL;
    while( n->mlink != NULL )
      n = n->mlink;

    n->mlink = note;
  }

  note->voice    = v;
  note->uid      = p->nextUid++;
  note->tied_dur = note->duration;

  return kOkXsRC;
}


cmXsRC_t  _cmXScoreRemoveNote( cmXsNote_t* note )
{
  cmXsNote_t* n0 = NULL;
  cmXsNote_t* n1 = note->voice->noteL;
  unsigned    cnt = 0;
  for(; n1!=NULL; n1=n1->mlink)
  {
    if( n1->uid == note->uid )
    {
      if( n0 == NULL )
        note->voice->noteL = NULL;
      else
        n0->mlink = n1->mlink;

      cnt = 1;
      break;
    }

    n0 = n1;
  }
  
  n0 = NULL;
  n1 = note->meas->noteL;
  for(; n1!=NULL; n1=n1->slink)
  {
    if( n1->uid == note->uid )
    {
      if( n0 == NULL )
        note->voice->noteL = NULL;
      else
        n0->slink = n1->slink;

      cnt = 2;
      break;
    }
    
    n0 = n1;
  }
  return cnt == 2 ? kOkXsRC : kSyntaxErrorXsRC;
 
}

void _cmXScoreInsertNoteBefore( cmXsNote_t* note, cmXsNote_t* nn )
{
  assert( note != NULL );
  
  // insert the new note into the voice list before 'note'
  cmXsNote_t* n0 = NULL;
  cmXsNote_t* n1 = note->voice->noteL;
  for(; n1 != NULL; n1=n1->mlink )
  {
    if( n1->uid == note->uid )
    {
      if( n0 == NULL )
        note->voice->noteL = nn;        
      else
        n0->mlink = nn;

      nn->mlink = note;
      break;
    }
    
    n0 = n1;
  }

  assert( n1 != NULL );

  // insert the new note into the time sorted note list before 'note'
  n0 = NULL;
  n1 = note->meas->noteL;
  for(; n1!=NULL; n1=n1->slink)
  {
    if( n1->tick >= nn->tick )
    {
      if( n0 == NULL )
        note->meas->noteL = nn;
      else
        n0->slink = nn;      

      nn->slink = n1;
      
      break;
    }

    n0 = n1;
  }

  assert( n1 != NULL );
}

void _cmXScoreInsertNoteAfter( cmXsNote_t* note, cmXsNote_t* nn )
{

  // insert the new note into the voice list after 'note'
  cmXsNote_t* n0 = note->voice->noteL;
  for(; n0 != NULL; n0=n0->mlink )
    if( n0->uid == note->uid )
    {
      
      nn->mlink   = note->mlink;
      note->mlink = nn;
      break;
    }
  
  assert(n0 != NULL );

  // insert the new note into the time sorted note list after 'note'
  n0 = note->meas->noteL;
  for(; n0!=NULL; n0=n0->slink)
  {
    if( n0->tick >= nn->tick )
    {
      nn->slink = n0->slink;
      n0->slink = nn;      

      break;
    }

    // if n0 is the last ele in the list
    if( n0->slink == NULL )
    {
      n0->slink = nn;
      nn->slink = NULL;
      break;
    }
  }

  assert(n0 != NULL);
  
}


cmXsRC_t _cmXScoreParsePartList( cmXScore_t* p )
{
  cmXsRC_t           rc          = kOkXsRC;
  cmXsPart_t*        lastPartPtr = NULL;
  const cmXmlNode_t* xnp;

  // find the 'part-list'
  if((xnp = cmXmlSearch( cmXmlRoot(p->xmlH), "part-list", NULL, 0)) == NULL )
    return _cmXScoreMissingNode(p,cmXmlRoot(p->xmlH),"part-list");

  const cmXmlNode_t* cnp = xnp->children;

  // for each child of the 'part-list'
  for(; cnp!=NULL; cnp=cnp->sibling)
    if( cmTextCmp( cnp->label, "score-part" ) == 0 )
    {
      const cmXmlAttr_t* a;

      // find the 'score-part' id
      if((a = cmXmlFindAttrib(cnp,"id")) == NULL )
        return _cmXScoreMissingAttribute(p,cnp,"id");

      // allocate a new part record
      cmXsPart_t* pp = cmLhAllocZ(p->lhH,cmXsPart_t,1);

      pp->idStr = a->value;  // set the part id

      // link the new part record to the end of the part list
      if(lastPartPtr == NULL)
        p->partL = pp;
      else
        lastPartPtr->link = pp;

      lastPartPtr = pp;
    }

  return rc;
}

cmXsRC_t  _cmXScoreParsePitch( cmXScore_t* p, const cmXmlNode_t* nnp, cmXsNote_t* np )
{
  cmXsRC_t        rc     = kOkXsRC;
  unsigned        octave = 0;
  double          alter  = 0;
  const cmChar_t* step   = NULL;

  if((step = cmXmlNodeValue(nnp,"pitch","step",NULL)) == NULL )
    return _cmXScoreMissingNode(p,nnp,"step");

  if((rc = cmXmlNodeUInt( nnp,&octave,"pitch","octave",NULL)) != kOkXmlRC )
    return _cmXScoreMissingNode(p,nnp,"octave");

  cmXmlNodeDouble( nnp,&alter,"pitch","alter",NULL);

  int acc = alter;
  unsigned midi = cmSciPitchToMidiPitch(*step,acc,octave);

  np->pitch  = midi;
  np->step   = *step;
  np->octave = octave;
  np->alter  = alter;
  np->flags |= kOnsetXsFl;

  return rc;
}

cmXsRC_t  _cmXScoreParseNoteRValue( cmXScore_t* p, const cmXmlNode_t* nnp, const cmChar_t* label, double* rvalueRef )
{
  typedef struct map_str
  {
    double         rvalue;
    const cmChar_t* label;
  } map_t;

  map_t mapV[] =
  {
    {-1.0, "measure" },  // whole measure rest
    { 0.5, "breve"   },  // double whole
    { 1.0, "whole"   },
    { 2.0, "half"    },
    { 4.0, "quarter" },
    { 8.0, "eighth"  },
    {16.0, "16th"    },
    {32.0, "32nd"    },
   { 64.0, "64th"    },
   {128.0, "128th"   },
   {  0.0, ""        }
  };

  const cmChar_t* str;
  // get the XML rvalue label
  if((str = cmXmlNodeValue(nnp,label,NULL)) == NULL)
  {
    if((nnp = cmXmlSearch(nnp,"rest",NULL,0)) != NULL )
    {
      const cmXmlAttr_t* a;
      if((a = cmXmlFindAttrib(nnp,"measure")) != NULL && cmTextCmp(a->value,"yes")==0)
      {
        *rvalueRef = -1;
        return kOkXsRC;
      }
    }

    return cmErrMsg(&p->err,kSyntaxErrorXsRC,"The 'beat-unit' metronome value is missing on line %i.",nnp->line);
  }

  unsigned i;
  // lookup the rvalue numeric value from the mapV[] table
  for(i=0; mapV[i].rvalue!=0; ++i)
    if( cmTextCmp(mapV[i].label,str) == 0 )
    {
      *rvalueRef = mapV[i].rvalue;
      return kOkXsRC;
    }

  // the rvalue label was not found
  return cmErrMsg(&p->err,kSyntaxErrorXsRC,"Unknown rvalue type='%s'.",str);
}

cmXsRC_t  _cmXScoreParseColor( cmXScore_t* p, const cmXmlNode_t* nnp, cmXsNote_t* note )
{
  cmXsRC_t rc = kOkXsRC;
  const cmXmlAttr_t* a;

   typedef struct map_str
  {
    unsigned long long        value;
    const cmChar_t* label;
  } map_t;

  map_t mapV[] =
  {
    { kEvenXsFl,                                       "#0000FF" },  // blue (even)
    { kEvenXsFl  | kEvenEndXsFl,                       "#0000FE" },  // blue (even end)
    { kEvenXsFl  | kEvenEndXsFl,                       "#0000FD" },  // blue (even end)
    { kTempoXsFl,                                      "#00FF00" },  // green (tempo)
    { kTempoXsFl | kTempoEndXsFl,                      "#00FE00" },  // green (tempo end)
    { kDynXsFl,                                        "#FF0000" },  // red   (dynamics)
    { kDynXsFl   | kDynEndXsFl,                        "#FE0000" },  // red   (dynamics end)
    { kDynXsFl   | kDynEndXsFl,                        "#FD0000" },  // red   (dynamics end)
    { kTempoXsFl | kEvenXsFl,                          "#00FFFF" },  // green + blue (turquoise)
    { kTempoXsFl | kEvenXsFl | kEvenEndXsFl,           "#00FFFE" },  // green + blue (turquoise) (end)    
    { kDynXsFl   | kEvenXsFl,                          "#FF00FF" },  // red   + blue
    { kDynXsFl   | kEvenXsFl | kEvenEndXsFl,           "#FF00FE" },  // red   + blue (end)
    { kDynXsFl   | kEvenXsFl | kEvenEndXsFl,           "#FF00FD" },  // red   + blue (end)    
    { kDynXsFl   | kEvenXsFl,                          "#FF0CF7" },  // magenta (even+dyn)
    { kDynXsFl   | kTempoXsFl,                         "#FF7F00" },  // red   + green (brown)
    { kDynXsFl   | kTempoXsFl,                         "#FE7F00" },  // red   + green (brown)    (end)
    { kDynXsFl   | kTempoXsFl,                         "#FD7F00" },  // red   + green (brown)    (end)
    { kDynXsFl   | kTempoXsFl | kTempoEndXsFl,         "#FF7E00" },  //
    { kDynXsFl   | kDynEndXsFl | kEvenXsFl | kEvenEndXsFl, "#FE00FE" }, //
    { kDynXsFl   | kDynEndXsFl | kEvenXsFl,            "#FE00FF" },
    { kTempoXsFl | kEvenXsFl | kDynXsFl,               "#996633" },  // (purple)
    { kTempoXsFl | kEvenXsFl | kDynXsFl | kDynEndXsFl, "#996632" },  // (purple)
    { kDynXsFl,                                        "#FF6A03" },  //   176 orange  (dynamics)
    { kEvenXsFl,                                       "#2F00E8" },  //  1001 blue (even)
    { kTempoXsFl,                                      "#01CD1F" },  //  1196 green   (tempo)
    { kEvenXsFl,                                       "#3600E8" },  //  1627 blue (even)
    { kDynXsFl | kTempoXsFl,                           "#9E8F15" },  //  8827 brown (dyn + tempo)
    { kEvenXsFl,                                       "#2E00E6" },  //  5393 blue (even)
    { kEvenXsFl,                                       "#2C00DD" },  //  5895 blue (even)
    { kDynXsFl,                                        "#FF5B03" },  //  6498 orange (dyn)
    { kDynXsFl,                                        "#FF6104" },  //  6896 orange
    { kEvenXsFl,                                       "#2A00E6" },  //  7781 blue
    { kEvenXsFl,                                       "#2300DD" },  //  8300 blue (even)
    { kTempoXsFl,                                      "#03CD22" },  // 10820 green (tempo)
    { kEvenXsFl,                                       "#3400DB" },  // 11627 blue (dyn)
    { -1, "" }
  };

  /*
    orange  #FF6A03
    magenta #FF0CF7
    blue    #2F00E8
    green   #01CD1F
    gold    #9E8F15
    green   #03CD22
   */

  if((a = cmXmlFindAttrib(nnp, "color" )) != NULL )
  {
    unsigned i;
    for(i=0; mapV[i].value != -1; ++i)
      if( cmTextCmp(a->value,mapV[i].label) == 0 )
      {
        note->flags += mapV[i].value;
        break;
      }

    if( mapV[i].value == -1 )
      cmErrMsg(&p->err,kSyntaxErrorXsRC,"The note color '%s' was not found on line %i.",a->value,nnp->line);
  }

  return rc;
}

// On input tick0Ref is set to the tick of the previous event.
// On input tickRef is set to the tick of this event.
// On output tick0Ref is set to the tick of this event.
// On output tickRef is set to the tick of the next event.
cmXsRC_t _cmXScoreParseNote(cmXScore_t* p, cmXsMeas_t* meas, const cmXmlNode_t* nnp, unsigned* tick0Ref, unsigned* tickRef )
{
  cmXsRC_t    rc   = kOkXsRC;
  cmXsNote_t* note = cmLhAllocZ(p->lhH,cmXsNote_t,1);
  unsigned    voiceId;

  note->meas    = meas;
  note->xmlNode = nnp;

  // get the voice id for this node
  if( cmXmlNodeUInt(nnp,&voiceId,"voice",NULL) != kOkXmlRC )
    return _cmXScoreMissingNode(p,nnp,"voice");

  // if this note has a pitch
  if( cmXmlNodeHasChild(nnp,"pitch",NULL) )
    if((rc = _cmXScoreParsePitch(p,nnp,note)) != kOkXsRC )
      return rc;


  cmXmlNodeUInt(nnp,&note->duration,"duration",NULL);  // get the note duration
  cmXmlNodeUInt(nnp,&note->staff,"staff",NULL);        // get th staff number

  // is 'rest'
  if( cmXmlNodeHasChild(nnp,"rest",NULL) )
    note->flags |= kRestXsFl;

  // is 'grace'
  if( cmXmlNodeHasChild(nnp,"grace",NULL) )
    note->flags |= kGraceXsFl;

  // is 'dot'
  if( cmXmlNodeHasChild(nnp,"dot",NULL) )
    note->flags |= kDotXsFl;

  // is 'chord'
  if( cmXmlNodeHasChild(nnp,"chord",NULL) )
    note->flags |= kChordXsFl;

  // is this is first note in a tied pair
  if( cmXmlNodeHasChildWithAttrAndValue(nnp,"tie","type","start",NULL) )
    note->flags |= kTieBegXsFl;

  // is this is second note in a tied pair
  if( cmXmlNodeHasChildWithAttrAndValue(nnp,"tie","type","stop",NULL) )
    note->flags |= kTieEndXsFl;

  // has 'heel' mark
  if( cmXmlNodeHasChild(nnp,"notations","technical","heel",NULL) )
    note->flags |= kHeelXsFl;

  // set color coded flags
  if((rc = _cmXScoreParseColor(p, nnp, note )) != kOkXsRC )
    return rc;

  // get the note's rythmic value
  if((rc =  _cmXScoreParseNoteRValue(p,nnp,"type",&note->rvalue)) != kOkXsRC )
    return rc;


  // if this is a chord note
  if( cmIsFlag(note->flags,kChordXsFl) )
  {
    note->tick = *tick0Ref; // then use the onset time from the previous note and do not advance time
  }
  else
  {
    *tick0Ref  = *tickRef;
    note->tick = *tickRef;

    *tickRef += note->duration;
  }

  return _cmXScorePushNote(p, meas, voiceId, note );
}

cmXsRC_t _cmXScorePushNonNote( cmXScore_t* p, cmXsMeas_t* meas, const cmXmlNode_t* noteXmlNode, unsigned tick, unsigned duration, double rvalue, const cmChar_t* tvalue, unsigned flags )
{
  cmXsNote_t* note    = cmLhAllocZ(p->lhH,cmXsNote_t,1);
  unsigned    voiceId = 0;    // non-note's are always assigned to voiceId=0;

  note->tick     = tick;
  note->flags    = flags;
  note->rvalue   = rvalue;
  note->tvalue   = tvalue;
  note->duration = duration;
  note->tied_dur = duration;
  note->meas     = meas;
  note->xmlNode  = noteXmlNode;

  return _cmXScorePushNote(p, meas, voiceId, note );
}

cmXsSpan_t* _cmXScoreFindOpenOctaveShift( cmXScore_t* p, unsigned staff, unsigned number )
{
  cmXsSpan_t* s = p->spanL;
  for(; s!=NULL; s=s->link)
    if( s->tick1 == -1 && s->staff == staff && s->number == number )
      return s;

  return NULL;
}

cmXsRC_t _cmXScorePushOctaveShift(cmXScore_t* p, cmXsMeas_t* meas, unsigned staff, unsigned span_number, const cmChar_t* type_str, unsigned tick)
{
  assert( meas != NULL);

  cmXsSpan_t* s;
  if( cmTextCmp(type_str,"stop") == 0 )
  {
    if((s = _cmXScoreFindOpenOctaveShift(p,staff,span_number)) == NULL )
      return cmErrWarnMsg(&p->err,kUnterminatedOctaveShiftXsrRC,"An illegal octave shift was encounted in meas %i.\n",meas->number);

    s->tick1 = tick;
  }
  else
  {
    s               = cmLhAllocZ(p->lhH,cmXsSpan_t,1);
    s->staff        = staff;
    s->meas         = meas;
    s->number       = span_number;
    s->tick0        = tick;
    s->tick1        = -1;
    s->pitch_offset = cmTextCmp(type_str,"up")==0 ? -12 : 12;
    s->link         = p->spanL;
    p->spanL        = s;
  }

  return kOkXsRC;
}


cmXsRC_t  _cmXScoreParseDirection(cmXScore_t* p, cmXsMeas_t* meas, const cmXmlNode_t* dnp, unsigned tick)
{
  cmXsRC_t           rc       = kOkXsRC;
  const cmXmlNode_t* np       = NULL;
  const cmXmlAttr_t* a        = NULL;
  unsigned           flags    = 0;
  int                offset   = 0;
  double             rvalue   = 0;
  const cmChar_t*    tvalue   = NULL;
  unsigned           duration = 0;
  bool               pushFl   = true;
  unsigned           staff    = 0;

  cmXmlNodeInt( dnp, &offset, "offset", NULL );
  cmXmlNodeUInt(dnp, &staff,  "staff",  NULL );

  // if this is a metronome direction
  if((np = cmXmlSearch( dnp, "metronome", NULL, 0)) != NULL )
  {

    if( cmXmlNodeUInt(np,&duration,"per-minute",NULL) != kOkXmlRC )
      return cmErrMsg(&p->err,kSyntaxErrorXsRC,"The 'per-minute' metronome value is missing on line %i.",np->line);

    if((rc = _cmXScoreParseNoteRValue(p,np,"beat-unit",&rvalue)) != kOkXsRC )
      return rc;

    flags = kMetronomeXsFl;
  }
  else

  // if this is a pedal direction
  if((np = cmXmlSearch( dnp, "pedal",NULL,0)) != NULL )
  {

    if((a = cmXmlFindAttrib(np,"type")) == NULL )
      return _cmXScoreMissingAttribute(p, np, "type" );

    if( cmTextCmp(a->value,"start") == 0 )
      flags = kDampDnXsFl;
    else
      if( cmTextCmp(a->value,"change") == 0 )
        flags = kDampUpDnXsFl;
      else
        if( cmTextCmp(a->value,"stop") == 0 )
          flags = kDampUpXsFl;
        else
          return cmErrMsg(&p->err,kSyntaxErrorXsRC,"Unrecognized pedal type:'%s'.",cmStringNullGuard(a->value));
  }
  else

  // if this is a 'words' direction
  if((np = cmXmlSearch( dnp, "words", NULL, 0)) != NULL )
  {
    if((a = cmXmlFindAttrib(np,"enclosure")) != NULL && cmTextCmp(a->value,"rectangle")==0 )
    {
      if( cmTextIsEmpty( tvalue = np->dataStr ) )
        return cmErrMsg(&p->err,kSyntaxErrorXsRC,"Section number is blank or missing on line %i.",np->line);

      flags = kSectionXsFl;
    }
  }
  else

  // if this is an 'octave-shift' direction
  if((np = cmXmlSearch( dnp, "octave-shift", NULL, 0)) != NULL )
  {
    unsigned span_number = -1;
    if( cmXmlAttrUInt(np,"number",&span_number) != kOkXmlRC )
      return cmErrMsg(&p->err,kSyntaxErrorXsRC,"Octave-shift is missing a 'number' attribute.");


    if((a = cmXmlFindAttrib(np,"type")) == NULL)
      return cmErrMsg(&p->err,kSyntaxErrorXsRC,"Octave-shift is missing a 'type' attribute.");


    rc = _cmXScorePushOctaveShift(p,meas,staff,span_number,a->value,tick+offset);

    pushFl = false;
  }
  else
  {
    pushFl = false;
  }

  if( pushFl )
   rc = _cmXScorePushNonNote(p,meas,dnp,tick+offset,duration,rvalue,tvalue,flags);

  return rc;
}


// On input tickRef is set to the absolute tick of the bar line and on output it is set
// to the absolute tick of the next bar line.
cmXsRC_t _cmXScoreParseMeasure(cmXScore_t* p, cmXsPart_t* pp, const cmXmlNode_t* mnp, unsigned* tickRef)
{
  cmXsRC_t           rc   = kOkXsRC;
  const cmXmlNode_t* np   = NULL;
  unsigned           tick = *tickRef;
  unsigned           tick0= 0;
  cmXsMeas_t*        m    = NULL;

  // allocate the 'measure' record
  cmXsMeas_t* meas = cmLhAllocZ(p->lhH,cmXsMeas_t,1);

  // get measure number
  if( cmXmlAttrUInt(mnp,"number", &meas->number) != kOkXmlRC )
    return _cmXScoreMissingAttribute(p,mnp,"number");

  if( pp->measL == NULL )
    pp->measL = meas;
  else
  {
    m = pp->measL;
    while( m->link != NULL )
      m = m->link;

    m->link         = meas;
    meas->divisions = m->divisions;
    meas->beats     = m->beats;
    meas->beat_type = m->beat_type;
  }

  // get measure attributes node
  if((np = cmXmlSearch(mnp,"attributes",NULL,0)) != NULL)
  {
    cmXmlNodeUInt(np,&meas->divisions,"divisions",NULL);
    cmXmlNodeUInt(np,&meas->beats,    "time","beats",NULL);
    cmXmlNodeUInt(np,&meas->beat_type,"time","beat-type",NULL);
  }

  // store the bar line
  if((rc = _cmXScorePushNonNote(p,meas,mnp,tick,0,0,NULL,kBarXsFl)) != kOkXsRC )
    return rc;

  np = mnp->children;

  // for each child of the 'meas' XML node
  for(; rc==kOkXsRC && np!=NULL; np=np->sibling)
  {
    // if this is a 'note' node
    if( cmTextCmp(np->label,"note") == 0 )
    {
      rc = _cmXScoreParseNote(p,meas,np,&tick0,&tick);
    }
    else
      // if this is a 'backup' node
      if( cmTextCmp(np->label,"backup") == 0 )
      {
        unsigned backup;
        cmXmlNodeUInt(np,&backup,"duration",NULL);
        if( backup > tick )
          tick = 0;
        else
          tick -= backup;

        tick0 = tick;
      }
      else
        // if this is a 'direction' node
        if( cmTextCmp(np->label,"direction") == 0 )
        {
          rc = _cmXScoreParseDirection(p,meas,np,tick);
        }

  }

  *tickRef = tick;
  return rc;
}

cmXsRC_t _cmXScoreParsePart( cmXScore_t* p, cmXsPart_t* pp )
{
  cmXsRC_t           rc       = kOkXsRC;
  const cmXmlNode_t* xnp;
  cmXmlAttr_t        partAttr  = { "id", pp->idStr };
  unsigned           barTick   = 0;
  // find the 'part'
  if((xnp = cmXmlSearch( cmXmlRoot(p->xmlH), "part", &partAttr, 1)) == NULL )
    return cmErrMsg(&p->err,kSyntaxErrorXsRC,"The part '%s' was not found.",pp->idStr);

  // for each child of this part - find each measure
  const cmXmlNode_t* cnp = xnp->children;
  for(; cnp!=NULL; cnp=cnp->sibling)
    if( cmTextCmp(cnp->label,"measure") == 0 )
      if((rc = _cmXScoreParseMeasure(p,pp,cnp,&barTick)) != kOkXsRC )
        return rc;

  return rc;
}

// Insert note 'np' into the sorted note list based at 's0'.
// Return a pointer to the base of the list after the insertion.
cmXsNote_t*  _cmXScoreInsertSortedNote( cmXsNote_t* s0, cmXsNote_t* np )
{
  assert( np != NULL );

  // np->slink is not NULL if the list is being resorted
  np->slink = NULL;

  // this list is empty so np is the first element on the list
  if( s0 == NULL )
    return np;

  // np is before the first element on the list
  if( np->tick < s0->tick )
  {
    np->slink = s0;
    return np;
  }

  cmXsNote_t* s1 = s0;
  cmXsNote_t* s2 = s0->slink;

  while( s2 != NULL )
  {
    if( s2->tick > np->tick )
    {
      s1->slink = np;
      np->slink = s2;
      return s0;
    }

    s1 = s2;
    s2 = s2->slink;
  }

  s1->slink = np;

  return s0;
}

// Set the cmXsNode_t.secs and dsecs.
void _cmXScoreSetAbsoluteTime( cmXScore_t* p )
{
  double          tpqn         = 0; // ticks per quarter note
  double          tps          = 0; // ticks per second
  unsigned        metro_tick   = 0;
  double          metro_sec    = 0;
  double          sec0         = 0;

  cmXsPart_t* pp = p->partL;
  for(; pp!=NULL; pp=pp->link)
  {
    cmXsMeas_t* mp = pp->measL;
    for(; mp!=NULL; mp=mp->link)
    {
      if( mp->divisions != 0 )
        tpqn = mp->divisions;

      cmXsNote_t* np = mp->noteL;

      for(; np!=NULL; np=np->slink)
      {

        // Seconds are calculated as:
        // dticks = np->tick - metro_tick;     // where metro_tick is the absolute tick of the last metro event
        // secs   = (dticks/tps) + metro_secs; // where metro_secs is the absoute time of the last metro event

        unsigned dticks = np->tick - metro_tick;                  
        double   secs   = tps==0 ? 0 : (dticks/tps) + metro_sec; 
        double   dsecs  = secs - sec0;

        //
        if( cmIsFlag(np->flags,kMetronomeXsFl) )
        {
          double bpm = np->duration;  // beats per minute
          double bps = bpm / 60.0;    // beats per second
          tps        = bps * tpqn;    // ticks per second
          metro_tick = np->tick;      // store tick of the last metronome marker
          metro_sec  = secs;          // store time of the last metronome marker
        }

        if( cmIsFlag(np->flags,kBarXsFl|kDampDnXsFl|kDampUpXsFl|kDampUpDnXsFl|kSostDnXsFl|kSostUpXsFl|kOnsetXsFl|kSectionXsFl) )
        {
          np->secs  = secs;
          np->dsecs = dsecs;
          sec0      = secs;
        }
      }
    }
  }
}


void _cmXScoreSort( cmXScore_t* p )
{
  // for each part
  cmXsPart_t* pp = p->partL;
  for(; pp!=NULL; pp=pp->link)
  {
    // for each measure in this part
    cmXsMeas_t* mp = pp->measL;
    for(; mp!=NULL; mp=mp->link)
    {
      // explicitely set noteL to NULL to in case we are re-sorting
      mp->noteL = NULL;

      // for each voice in this measure
      cmXsVoice_t* vp = mp->voiceL;
      for(; vp!=NULL; vp=vp->link)
      {
        // for each note in this measure
        cmXsNote_t* np = vp->noteL;
        for(; np!=NULL; np=np->mlink)
          mp->noteL = _cmXScoreInsertSortedNote(mp->noteL,np);
      }
    }
  }

  // The order of events may have changed update the absolute time.
  _cmXScoreSetAbsoluteTime( p );
}

// All notes in a[aN] are on the same tick
unsigned  _cmXsSpreadGraceNotes( cmXsNote_t** a, unsigned aN )
{
  unsigned i;
  bool barFl = false;

  // set barFl to true if a bar marker is included in the notes
  for(i=0; i<aN; ++i)
    if( cmIsFlag(a[i]->flags,kBarXsFl) )
      barFl = true;

  // spread any grace notes by one tick
  unsigned nextGraceTick = UINT_MAX;
  for(i=0; i<aN; ++i)
    if( cmIsFlag(a[i]->flags,kGraceXsFl) )
    {
      if( nextGraceTick == UINT_MAX )
        nextGraceTick = a[i]->tick + 1;
      else
      {
        a[i]->tick     = nextGraceTick;
        nextGraceTick += 1;
      }
    }

  // if this tick group includes the bar ...
  if( barFl && nextGraceTick != UINT_MAX )
  {
    // ... then move all non-grace note events (except the bar marker) after
    // the grace notes
    for(i=0; i<aN; ++i)
      if( cmIsNotFlag(a[i]->flags,kGraceXsFl) && cmIsNotFlag(a[i]->flags,kBarXsFl) )
        a[i]->tick = nextGraceTick;
  }

  return nextGraceTick==UINT_MAX ? 0 : nextGraceTick;
}

void _cmXScoreSpreadGraceNotes( cmXScore_t* p )
{
  cmXsPart_t* pp = p->partL;
  for(; pp!=NULL; pp=pp->link)
  {
    // tick1 is the location of the minimum current tick
    // (or 0 if it should be ignored)
    unsigned    tick1 = 0;

    cmXsMeas_t* mp = pp->measL;
    for(; mp!=NULL; mp=mp->link)
    {
      cmXsNote_t* np = mp->noteL;
      unsigned    aN = 128;
      cmXsNote_t* a[ aN ];
      unsigned    ai = 0;

      // The first event in a measure may have been forced ahead
      // by spreading at the end of the previous measure
      if( tick1 > np->tick )
        np->tick = tick1;
      else
        tick1 = 0;

      // tick0 is the tick of the current tick group we are examining
      // A tick group is a group of events that share the same tick.
      unsigned    tick0 = np->tick;

      // for each note 
      for(; np!=NULL; np=np->slink)
      {
        // if this event is the first of a new tick group (then a[] holds a group completed on the previous note)
        if( np->tick != tick0 )
        {
          // if there is more than one event in the completed tick group ...
          if( ai > 1 )
            tick1 = _cmXsSpreadGraceNotes(a,ai); // ... then process the group


          ai    = 0;        // empty the tick group array
          tick0 = np->tick; // update the current group's common tick
        }

        // if the min. current tick is ahead of this event then move the event ahead
        if( tick1 > np->tick )
          np->tick = tick1;
        else
          tick1 = 0; // otherwise disable tick1

        // add this event to the tick group
        assert(ai<aN);
        a[ai++] = np;

      }

      // if there are events in the group array then process them
      if( ai > 1 )
         tick1 = _cmXsSpreadGraceNotes(a,ai);
    }
  }
}

bool  _cmXScoreFindTiedNote( cmXScore_t* p, cmXsMeas_t* mp, cmXsNote_t* n0p, bool rptFl )
{
  cmXsNote_t* nbp       = n0p;
  cmXsNote_t* nnp       = n0p->slink;  // begin w/ note following np
  unsigned    measNumb  = mp->number;
  cmChar_t    acc       = n0p->alter==-1?'b' : (n0p->alter==1?'#':' ');

  if( rptFl )
    printf("%i %i %s ",n0p->meas->number,n0p->tick,cmMidiToSciPitch(n0p->pitch,NULL,0));


  while(1)
  {
    // if we are at the end of a measure advance to the next measure
    if( nnp == NULL )
    {
      mp  = mp->link;
      nnp = mp->noteL;

      // if a measure was completed and no end note was found ... then the tie is unterminated
      // (a tie must be continued in every measure which it passes through)
      if( mp->number > measNumb + 1 )
        break;
    }

    // for each note starting at nnp
    for(; nnp!=NULL; nnp=nnp->slink)
    {
      // if this note is tied to the originating note (np)
      if( nnp->voice->id == n0p->voice->id && nnp->step == n0p->step && nnp->octave == n0p->octave )
      {
        nnp->flags    |= kTieProcXsFl;
        nnp->flags     = cmClrFlag(nnp->flags,kOnsetXsFl);
        n0p->tied      = nnp;
        nbp->tied_dur += nnp->duration;
        nnp->tied_dur  = 0;

        if( rptFl )
          printf("---> %i %i %s ",nnp->meas->number,nnp->tick,cmMidiToSciPitch(nnp->pitch,NULL,0));

        // if this note is not tied to a subsequent note
        if( cmIsNotFlag(nnp->flags,kTieBegXsFl) )
          return true;

        n0p = nnp;
        
        // record the measure number of the last note with a tie-start
        measNumb = mp->number;
      }
    }
  }

  cmErrWarnMsg(&p->err,kUnterminatedTieXsRC,"The tied %c%c%i in measure %i (tick:%i) was not terminated.",n0p->step,acc,n0p->octave,measNumb,n0p->tick);
  return false;
}

void  _cmXScoreResolveTiesAndLoc( cmXScore_t* p )
{
  unsigned    n     = 0;        // count of notes which begin a tie
  unsigned    m     = 0;        // count of tied notes that are correctly terminated.
  bool        rptFl = false;
  cmXsPart_t* pp    = p->partL;

  // for each part
  for(; pp!=NULL; pp=pp->link)
  {
    unsigned locIdx = 1;
    cmXsMeas_t* mp = pp->measL;

    // for each measure
    for(; mp!=NULL; mp=mp->link)
    {
      cmXsNote_t* n0 = NULL;
      cmXsNote_t* np = mp->noteL;

      // for each note in this measure
      for(; np!=NULL; np=np->slink)
      {
        // if this note begins a tie and has not yet been  processed
        // (A note that continues a tie and therefore has a kTieBegXsFl set
        //  may have already been processed by an earlier tied note.)
        if( cmIsFlag(np->flags,kTieBegXsFl) && cmIsNotFlag(np->flags,kTieProcXsFl))
        {
          if( _cmXScoreFindTiedNote(p,mp,np,rptFl) )
            m += 1;

          if( rptFl )
            printf("\n");

          n += 1;
        }

        // Validate the tie state of the current note.
        if( cmIsFlag(np->flags,kTieEndXsFl) && cmIsFlag(np->flags,kOnsetXsFl) )
        {
          cmChar_t    acc  = np->alter==-1?'b' : (np->alter==1?'#':' ');
          cmErrWarnMsg(&p->err,kUnterminatedTieXsRC,"The tied %c%c%i in measure %i marked as a tied note but is also marked to sound.",np->step,acc,np->octave,mp->number);
        }

        //
        // Set the score location of notes marked for onset and bar lines.
        //
        if( cmIsFlag(np->flags,kOnsetXsFl|kBarXsFl) )
        {
          // if this note does not share the same location as the previous 'located' note then increment the 'loc' index
          if( cmIsFlag(np->flags,kBarXsFl) || (n0!=NULL && n0->tick!=np->tick))
            locIdx += 1;

          np->locIdx = locIdx;
          n0         = np;
        }
      }
    }
  }

  printf("Tied notes found:%i Not found:%i\n",m,n-m);
}

cmXsRC_t  _cmXScoreResolveOctaveShift( cmXScore_t* p )
{

  const cmXsSpan_t* s;
  for(s=p->spanL; s!=NULL; s=s->link)
  {
    if( s->tick1 == -1)
    {
      cmErrWarnMsg(&p->err,kSyntaxErrorXsRC,"An unterminated octave shift span was encountered in measure %i staff=%i.",s->meas->number,s->staff);
    }
    else
    {
      cmXsMeas_t* m = p->partL->measL;
      for(; m!=NULL; m=m->link)
        if( m->number == s->meas->number )
          break;

      assert( m != NULL );

      cmXsNote_t* note = m->noteL;
      for(; note!=NULL; note=note->slink)
        if( note->staff==s->staff && s->tick0 <= note->tick && note->tick < s->tick1 )
          note->pitch += s->pitch_offset;
    }
  }

  return kOkXsRC;
}

cmXsNote_t*  _cmXScoreFindOverlappingNote( cmXScore_t* p, const cmXsNote_t* knp )
{
  cmXsPart_t* pp = p->partL;

  // for each part
  for(; pp!=NULL; pp=pp->link)
  {
    cmXsMeas_t* mp = pp->measL;

    // for each measure
    for(; mp!=NULL; mp=mp->link)
    {
      cmXsNote_t* np = mp->noteL;

      // for each note in this measure
      for(; np!=NULL; np=np->slink)
        if( np->uid != knp->uid
          && cmIsFlag(np->flags,kOnsetXsFl)
          && knp->pitch == np->pitch
          && knp->tick >= np->tick 
          && knp->tick <  (np->tick + np->tied_dur)  )
        {
          return np;
        }
    }
  }
  return NULL;
}

void  _cmXScoreProcessOverlappingNotes( cmXScore_t* p )
{
  cmXsPart_t* pp = p->partL;

  // for each part
  for(; pp!=NULL; pp=pp->link)
  {
    cmXsMeas_t* mp = pp->measL;

    // for each measure
    for(; mp!=NULL; mp=mp->link)
    {
      cmXsNote_t* np = mp->noteL;
      cmXsNote_t* fnp;
      
      // for each note in this measure
      for(; np!=NULL; np=np->slink)
        if( cmIsFlag(np->flags,kOnsetXsFl) && (fnp = _cmXScoreFindOverlappingNote(p,np)) != NULL)
        {
          // is np entirely contained inside fnp
          bool embeddedFl = fnp->tick + fnp->tied_dur > np->tick + np->tied_dur;
          
          //printf("bar=%3i %4s voice:%2i %2i : %7i %7i : %7i %7i : %7i : %c \n",np->meas->number,cmMidiToSciPitch(np->pitch,NULL,0),np->voice->id,fnp->voice->id,fnp->tick,fnp->tick+fnp->duration,np->tick,np->tick+np->duration, (fnp->tick+fnp->duration) - np->tick, embeddedFl ? 'E' : 'O');

          // turn off embedded notes
          if( embeddedFl )
          {
            if( np->voice->id == fnp->voice->id )
              cmErrWarnMsg(&p->err,kOverlapWarnXsRC,"A time embedded note (bar=%i %s) was removed even though it overlapped with a note in the same voice.",np->meas->number,cmMidiToSciPitch(np->pitch,NULL,0));
            
            np->flags = cmClrFlag(np->flags,kOnsetXsFl);
          }
          else
          {
            int d = (fnp->tick+fnp->tied_dur) - np->tick;

            // shorten the first note
            if( d > 0 && d < fnp->tied_dur )
              fnp->tied_dur -= d;

            // move the second note just past it
            np->tick       = fnp->tick + fnp->tied_dur + 1;
          }
        }
    }
  }
}


// The identical pitch may be notated to play simultaneously on different voices.
// As performed on the piano this will equate to a single sounding note.
// This function clears the onset flag on all except one of the duplicated notes.
void _cmXScoreRemoveDuplicateNotes( cmXScore_t* p )
{
  cmXsPart_t* pp = p->partL;

  // for each part
  for(; pp!=NULL; pp=pp->link)
  {
    cmXsMeas_t* mp = pp->measL;

    // for each measure
    for(; mp!=NULL; mp=mp->link)
    {
      cmXsNote_t* np = mp->noteL;

      // for each note in this measure
      for(; np!=NULL; np=np->slink)
        if( cmIsFlag(np->flags,kOnsetXsFl) )
        {
          cmXsNote_t* n0p = mp->noteL;
          for(; n0p!=NULL; n0p=n0p->slink)
            if( n0p!=np && cmIsFlag(n0p->flags,kOnsetXsFl) && np->locIdx==n0p->locIdx && np->pitch==n0p->pitch )
              n0p->flags = cmClrFlag(n0p->flags,kOnsetXsFl);

        }
    }
  }
}

void  _cmXScoreSetMeasGroups( cmXScore_t* p, unsigned flag )
{
  unsigned sectionId = 0;
  cmXsNote_t* n0 = NULL;

  cmXsPart_t* pp = p->partL;

  // for each part
  for(; pp!=NULL; pp=pp->link)
  {
    cmXsMeas_t* mp = pp->measL;

    // for each measure
    for(; mp!=NULL; mp=mp->link)
    {
      cmXsNote_t* np = mp->noteL;

      // for each note in this measure
      for(; np!=NULL; np=np->slink)
      {
        
        // if this note has a heel marker and we are looking for evenness events
        if( cmIsFlag(flag,kEvenXsFl) && cmIsFlag(np->flags,kHeelXsFl) )
        {
          np->flags = cmSetFlag(np->flags,kBegGroupXsFl | kEndGroupXsFl );
          np->evenGroupId = sectionId + 1;
        }

        // if this note is of the type we are looking for
        if( cmIsFlag(np->flags,flag) )
        {
          if( n0 == NULL )
            np->flags = cmSetFlag(np->flags,kBegGroupXsFl);

          n0 = np;
        }

        // if this is a section marker
        if( cmIsFlag(np->flags,kSectionXsFl)  )
        {
          if( n0 != NULL )
          {
            np->flags = cmSetFlag(np->flags,kEndGroupXsFl);

            switch( flag )
            {
              case kEvenXsFl:  n0->evenGroupId = sectionId+1; break;
              case kDynXsFl:   n0->dynGroupId  = sectionId+1; break;
              case kTempoXsFl: n0->tempoGroupId= sectionId+1; break;
            }
          }

          if( cmIsFlag(np->flags,kSectionXsFl) )
          {
            sectionId = 0;
            
            if( np->tvalue == NULL )
              cmErrWarnMsg(&p->err,kSyntaxErrorXsRC,"An apparent section label in measure '%i' is blank.",np->meas->number);            
            else
            if( cmTextToUInt( np->tvalue, &sectionId, NULL ) != kOkTxRC )
              cmErrWarnMsg(&p->err,kSyntaxErrorXsRC,"The section label '%s' is not an integer.",np->tvalue);
                
                //sectionId = np->tvalue==NULL ? 0 : strtol(np->tvalue,NULL,10);
          }
          
          n0       = NULL;
        }
      }
    }
  }

}


cmXsRC_t _cmXScoreWriteScorePlotFile( cmXScore_t* p, const cmChar_t* fn )
{
  cmXsRC_t  rc            = kOkXsRC;
  cmFileH_t fH            = cmFileNullHandle;
  double    ticks_per_sec = 0;
  double    onset_secs    = 0;

  if( cmFileOpen(&fH,fn,kWriteFileFl,p->err.rpt) != kOkFileRC )
    return cmErrMsg(&p->err,kFileFailXsRC,"Unable to create the file '%s'.",cmStringNullGuard(fn));

  cmXsPart_t* pp = p->partL;
  for(; pp!=NULL; pp=pp->link)
  {
    cmXsMeas_t* mp   = pp->measL;
    for(; mp!=NULL; mp=mp->link)
    {
      cmFilePrintf(fH,"b %f %i %s B\n",onset_secs,mp->number,"bar");

      cmXsNote_t* np    = mp->noteL;
      unsigned    tick0 = 0;
      for(; np!=NULL; np=np->slink)
      {
        if( cmIsFlag(np->flags,kMetronomeXsFl) )
        {
          double bps =  np->tied_dur / 60.0;

          // t   b  t
          // - = -  -
          // s   s  b

          ticks_per_sec = bps * mp->divisions;
        }
        else
        {
          if( cmIsFlag(np->flags,kOnsetXsFl) )
          {
            onset_secs += (np->tick - tick0) / ticks_per_sec;
            tick0       = np->tick;
            cmFilePrintf(fH,"n %f %f %i %s %s\n",onset_secs,np->tied_dur/ticks_per_sec,np->uid,cmMidiToSciPitch(np->pitch,NULL,0),cmIsFlag(np->flags,kGraceXsFl)?"G":"N");
          }
        }
      }

      onset_secs += (mp->divisions * mp->beats - tick0) / ticks_per_sec;
    }
  }

  cmFileClose(&fH);

  return rc;
}

// Force the bar event to be the first event in the measure.
void _cmXScoreFixBarLines( cmXScore_t* p )
{
  cmXsPart_t* pp = p->partL;
  for(; pp!=NULL; pp=pp->link)
  {
    cmXsMeas_t* mp   = pp->measL;
    for(; mp!=NULL; mp=mp->link)
    {
      cmXsNote_t* np  = mp->noteL;
      cmXsNote_t* ep  = NULL;
        
      for(; np!=NULL; np=np->slink )
      {
        if( cmIsFlag(np->flags,kBarXsFl) )
        {
          if( ep != NULL )
            np->tick = ep->tick;
          break;
        }
          
        if( ep == NULL )
          ep = np;
      }
      
    }
  }
}

// Assign pedal down durations to pedal down events.
cmXsRC_t _cmXScoreProcessPedals( cmXScore_t* p )
{
  cmXsRC_t    rc = kOkXsRC;
  cmXsPart_t* pp = p->partL;
  
  for(; pp!=NULL; pp=pp->link)
  {
    cmXsNote_t* dnp = NULL;  // pointer to last damper down event
    cmXsNote_t* snp = NULL;  // pointer to last sostenuto down event
    
    cmXsMeas_t* mp   = pp->measL;
    for(; mp!=NULL; mp=mp->link)
    {
      cmXsNote_t* np  = mp->noteL;        
      for(; np!=NULL; np=np->slink )
      {
        unsigned x = np->flags & (kDampDnXsFl|kDampUpXsFl|kDampUpDnXsFl|kSostUpXsFl|kSostDnXsFl);
        switch( x )
        {
          case 0:
            break;
            
          case kDampDnXsFl:
            if( dnp != NULL )
              cmErrWarnMsg(&p->err,kPedalStateErrorXsRc,"Damper down not preceded by damper up in measure:%i.",mp->number);
            else
              dnp = np;
            break;
            
          case kDampUpXsFl:
            if( dnp == NULL )
              cmErrWarnMsg(&p->err,kPedalStateErrorXsRc,"Damper up not preceded by damper down in measure:%i.",mp->number);
            else
            {
              dnp->duration = np->tick - dnp->tick;
              dnp = NULL;
            }
            break;
            
          case kDampUpDnXsFl:
            if( dnp == NULL )
              cmErrWarnMsg(&p->err,kPedalStateErrorXsRc,"Damper up/down not preceded by damper down in measure:%i.",mp->number);
            else
            {            
              dnp->duration = np->tick - dnp->tick;
              dnp = np;
            }
            break;
            
          case kSostDnXsFl:
            if( snp != NULL )
              cmErrWarnMsg(&p->err,kPedalStateErrorXsRc,"Sostenuto down not preceded by sostenuto up in measure:%i.",mp->number);
            else
              snp = np;            
            break;
            
          case kSostUpXsFl:
            if( snp == NULL )
              cmErrWarnMsg(&p->err,kPedalStateErrorXsRc,"Sostenuto up not preceded by sostenuto down in measure:%i.",mp->number);
            else
            {
              snp->duration = np->tick - snp->tick;
              snp = NULL;
            }
            break;
            
          default:
            {
              assert(0);
            }
        } // switch
          
      } // for notes
      
    } // for measures

    if( dnp != NULL )
      cmErrWarnMsg(&p->err,kPedalStateErrorXsRc,"Damper left down at the end of a part.");

    if( snp != NULL )
      cmErrWarnMsg(&p->err,kPedalStateErrorXsRc,"Sostenuto left down at the end of a part.");
  }

  
  _cmXScoreSort(p);

  return rc;
}

void _cmXScoreInsertTime( cmXScore_t* p, cmXsMeas_t* mp, cmXsNote_t* np, unsigned expand_ticks )
{
  for(; mp!=NULL; mp=mp->link)
  {  
    if( np == NULL )
      np = mp->noteL;
  
    for(; np!=NULL; np=np->slink)
      np->tick += expand_ticks;
  }   
}

void _cmXScoreGraceInsertTimeBase( cmXScore_t* p, unsigned graceGroupId, cmXsNote_t* aV[], unsigned aN, unsigned initTick )
{
  cmXsNote_t* np           = NULL;
  unsigned    expand_ticks = 0;
  unsigned    ticks        = initTick;
  
  unsigned    t0           = 0;
  unsigned    i;
  
  for(i=0; i<aN; ++i)
    if( cmIsFlag(aV[i]->flags,kGraceXsFl) && aV[i]->graceGroupId == graceGroupId )
    {
      // if this grace note falls on the same tick as the previous grace note 
      if( np != NULL && aV[i]->tick == t0 )
        aV[i]->tick = np->tick;
      else
      {
        t0            = aV[i]->tick;      // store the unmodified tick value of this note
        aV[i]->tick   = ticks;            // set the new tick value
        ticks        += aV[i]->duration;  // calc the next grace not location
        expand_ticks += aV[i]->duration;  // track how much we are expanding time by
      }

      np = aV[i];
    }

  np = np->slink;
  if( np != NULL )
    _cmXScoreInsertTime(p,np->meas,np,expand_ticks);
}

// (a) Insert the grace notes in between the first and last note in the group
// by inserting time between the first and last note.
// Note that in effect his means that the last note is pushed back
// in time by the total duration of the grace notes.
void _cmXScoreGraceInsertTime( cmXScore_t* p, unsigned graceGroupId, cmXsNote_t* aV[], unsigned aN )
{
  _cmXScoreGraceInsertTimeBase( p, graceGroupId,aV,aN, aV[aN-1]->tick );
}

// (s) Insert the grace notes in between the first and last note in the group
// but do not insert any additional time betwee the first and last note.
// In effect time is removed from the first note and taken by the grace notes.
// The time position of the last note is therefore unchanged.
void _cmXScoreGraceOverlayTime( cmXScore_t* p, unsigned graceGroupId, cmXsNote_t* aV[], unsigned aN )
{
  assert(aN >= 3 );
  
  int         i  = (int)aN-2;
  cmXsNote_t* np = aV[aN-1];
  unsigned    t0 = -1;
  
  for(; i>0; --i)
    if( cmIsFlag(aV[i]->flags,kGraceXsFl) && aV[i]->graceGroupId == graceGroupId )
    {
      if( aV[i]->tick == t0)
        aV[i]->tick = np->tick;
      else
      {
        t0          = aV[i]->tick;
        aV[i]->tick = np->tick - aV[i]->duration;
      }

      np = aV[i];
    }
}

// (A) Play the first grace at the time of the first note in the group (which is a non-grace note)
// and then expand time while inserting the other grace notes.
void _cmXScoreGraceInsertAfterFirst( cmXScore_t* p, unsigned graceGroupId, cmXsNote_t* aV[], unsigned aN )
{
  _cmXScoreGraceInsertTimeBase( p, graceGroupId,aV,aN, aV[0]->tick );
}


// (n) Play the first grace not shortly (one grace note duration) after the first note
// in the group (which is a non-grace note) and then expand time while inserting the other
// grace notes.
void _cmXScoreGraceInsertSoonAfterFirst( cmXScore_t* p, unsigned graceGroupId, cmXsNote_t* aV[], unsigned aN )
{
  _cmXScoreGraceInsertTimeBase( p, graceGroupId,aV,aN, aV[0]->tick + aV[1]->duration ); 
}


// Adjust the locations of grace notes. Note that this must be done
// after reordering so that we can be sure that the order in time of
// the notes in each group has been set prior to building the
// grace note groups - which must be in reverse time order.
cmXsRC_t _cmXScoreProcessGraceNotes( cmXScore_t* p, unsigned nextGraceGroupId )
{
  cmXsRC_t rc           = kOkXsRC;
  unsigned graceGroupId = 1;
  double   graceDurSec  = 1.0/15.0;  // duration of all grace notes in seconds

  for(; graceGroupId<nextGraceGroupId; ++graceGroupId)
  {
    cmXsNote_t* gn0p        = NULL;      // first note in the grace group
    cmXsNote_t* gn1p        = NULL;      // last note in the grace group
    unsigned    gN          = 0;
    cmXsPart_t* pp          = p->partL;
    double      ticksPerSec = 0;

    // Build a note chain, using cmXsNote_t.grace, between gn0p and
    // gn1p containing all the grace notes with
    // cmXsNote_t.graceGroupId == graceGroupId.
    
    for(; pp!=NULL; pp=pp->link)
    {
      cmXsMeas_t* mp = pp->measL;
      for(; mp!=NULL; mp=mp->link)
      {
        cmXsNote_t* np = mp->noteL;        
        for(; np!=NULL; np=np->slink )
        {
          // notice change of tempo
          if( cmIsFlag(np->flags,kMetronomeXsFl) )
          {
            // ticks/sec = ticks/qn * qn/sec
            ticksPerSec  = mp->divisions * np->duration / 60.0;
          }

          // if this note is part of the grace note group we are searching for
          if( np->graceGroupId == graceGroupId )
          {
            // track the first note in the grace note list
            if( gn0p == NULL )
              gn0p = np;

            // add the note to the end of the grace note list
            if( gn1p != NULL )
              gn1p->grace = np;

            // track the last note in the grace note list
            gn1p = np;
              
            // set each grace note to have 1/20 of a second duration
            if( cmIsFlag(np->flags,kGraceXsFl) )
              np->duration = np->tied_dur = floor(ticksPerSec * graceDurSec);

            gN += 1;
          } 
            
        } // for each note in this meassure
      } // for each measure
    } // for each part

    // no records were found for this grace id - this grace group id was not used
    if( gn0p == NULL )
      continue;

    // grace note groups must have at least 3 members
    if( gN < 3 )
    {
      rc = cmErrMsg(&p->err,kSyntaxErrorXsRC,"The grace note group (groupid=%i) ending in meas %i has fewer than 3 (%i) members.", gn1p->graceGroupId, gn1p->meas->number, gN );
      break;
    }
    
    // gn0p is now set to the first note in th group
    // gn1p is now set to the last note in the group

    // verify that the first note is marked with kBegGraceXsFl
    if( cmIsNotFlag(gn0p->flags,kBegGraceXsFl) )
    {
      rc = cmErrMsg(&p->err,kSyntaxErrorXsRC,"The first note in a grace note group in meas %i tick %i groupid:%i is not marked with a 'b'.", gn0p->meas->number, gn0p->tick, graceGroupId );
      break;
    }

    // verify that the last note is marked with kEndGraceXsFl
    if( cmIsNotFlag(gn1p->flags,kEndGraceXsFl) )
    {
      rc = cmErrMsg(&p->err,kSyntaxErrorXsRC,"The last note in a grace note group in meas %i is not marked with a valid operator character.", gn1p->meas->number );
      break;
    }

    // Count the total number of events between gn0p and gn1p
    cmXsNote_t* n0p = NULL;
    cmXsNote_t* n1p = gn0p;
    cmXsMeas_t* mp  = gn0p->meas;
    unsigned    aN  = 0;
    for(; n0p != gn1p; n1p=n1p->slink )
    {
      // if we are crossing a measure boundary
      if( n1p == NULL )
      {
        mp  = mp->link;
        assert(mp!=NULL);
        n1p = mp->noteL;        
      }

      if(0)
      {
        bool     fl   = n0p != NULL && n0p->tick < n1p->tick;
        unsigned type = n1p->flags & (kBegGraceXsFl|kEndGraceXsFl|kAddGraceXsFl|kSubGraceXsFl|kAFirstGraceXsFl|kNFirstGraceXsFl);
        printf("%3i 0x%08x %i %3i %5i %i\n",n1p->graceGroupId,type,n1p->meas->number,n1p->tick,n1p->duration,fl);
      }
      
      ++aN;
      n0p = n1p;
    }

    // create a vector of pointers to all events between gn0p and gn1p
    cmXsNote_t* aV[ aN ];
    unsigned    i;
    n1p = gn0p;
    n0p = NULL;
    mp = gn0p->meas;
    for(i=0; n0p != gn1p; n1p=n1p->slink )
    {
      // if we are crossing a measure boundary
      if( n1p == NULL )
      {
        mp  = mp->link;
        assert(mp!=NULL);
        n1p = mp->noteL;        
      }
      
      assert(i<aN);
      aV[i++] = n1p;
      n0p     = n1p;
    }

    switch( gn1p->flags & (kAddGraceXsFl | kSubGraceXsFl | kAFirstGraceXsFl | kNFirstGraceXsFl ) )
    {
      case kAddGraceXsFl:
        _cmXScoreGraceInsertTime(p, graceGroupId, aV, aN );
        break;
        
      case kSubGraceXsFl:
        _cmXScoreGraceOverlayTime(p, graceGroupId, aV, aN );
        break;
        
      case kAFirstGraceXsFl:
        _cmXScoreGraceInsertAfterFirst(p,graceGroupId,aV,aN);
        break;
        
      case kNFirstGraceXsFl:
        _cmXScoreGraceInsertSoonAfterFirst(p,graceGroupId,aV,aN);        
        break;
        
      default:
        { assert(0); }
    }

    

  }
  return rc;
}


cmXsNote_t* _cmXScoreDynamicForkEndNote( cmXScore_t* p, cmXsNote_t* bnp )
{
  assert( cmIsFlag(bnp->flags,kDynBegForkXsFl) );

  cmXsMeas_t* mp = bnp->meas;
  cmXsNote_t* np = bnp->slink;

  for(; mp!=NULL; mp=mp->link,np = mp==NULL?NULL :  mp->noteL)
  {    
    for(; np!=NULL; np=np->slink)
    {
      if( cmIsFlag(np->flags,kDynBegForkXsFl) )
      {
        cmErrMsg(&p->err,kSyntaxErrorXsRC,"The a dynamic fork begin (meas:%i loc:%i) was found prior to a matched end.",np->meas->number,np->locIdx);
        return NULL;
      }   
        
      if( cmIsFlag(np->flags,kDynEndForkXsFl) /*&& np->voice->id == bnp->voice->id*/ )
        return np;
    }
  }

  return NULL;
}

cmXsRC_t _cmXScoreProcessDynamicFork( cmXScore_t* p, cmXsNote_t* bnp )
{
  cmXsNote_t* enp;

  if((enp = _cmXScoreDynamicForkEndNote(p,bnp)) == NULL)
    return cmErrMsg(&p->err,kSyntaxErrorXsRC,"The dynamic fork beginning in measure %i at tick %i voice %i loc:%i was not terminated.", bnp->meas->number,bnp->tick,bnp->voice->id,bnp->locIdx);

  //cmRptPrintf( p->err.rpt, "Dynamic Fork: meas:%i tick:%i loc:%i to meas:%i tick:%i loc:%i\n", bnp->meas->number, bnp->tick, bnp->locIdx, enp->meas->number, enp->tick, enp->locIdx );
  
  double begDynFrac = bnp->dynamics;
  double begVelFrac = bnp->vel;
  double endDynFrac = enp->dynamics;
  double endVelFrac = enp->vel;

  cmXsMeas_t* mp = bnp->meas;
  cmXsNote_t* np = bnp->slink;

  while( mp != NULL )
  {
    for(; np!=NULL; np=np->slink)
    {

      //Conditioning this on np->dynamics==0 will may would cause 'silent' notes to be assigned 
      //  a non-zero dynamics value - so we condition on np->vel instead.
      if( cmIsFlag(np->flags,kOnsetXsFl) && np->vel == 0 )
      {
        double frac     = ((double)np->tick - bnp->tick) / (enp->tick - bnp->tick);
        np->dynamics =  lround(begDynFrac + ((endDynFrac - begDynFrac) * frac));
        np->vel      =  lround(begVelFrac + ((endVelFrac - begVelFrac) * frac));        
      }

      if( np == enp )
        break;
    }

    if( np == enp)
      break;
        
    if( (mp=mp->link) != NULL )
      np=mp->noteL;
  }

  return kOkXsRC;
}


cmXsRC_t _cmXScoreProcessDynamicForks( cmXScore_t* p )
{
  cmXsRC_t    rc = kOkXsRC;
  cmXsPart_t* pp = p->partL;

  for(; pp!=NULL; pp=pp->link)
  {
    cmXsMeas_t* mp = pp->measL;
    for(; mp!=NULL; mp=mp->link)
      {
        cmXsNote_t* np = mp->noteL;        
        for(; np!=NULL; np=np->slink )
        {
          if( cmIsFlag(np->flags,kDynBegForkXsFl) )
            if((rc =  _cmXScoreProcessDynamicFork(p,np)) != kOkXsRC )
              return rc;
        }
      }
  }
  
  return rc;
}
//-------------------------------------------------------------------------------------------


typedef struct
{
  unsigned    idx;      // Fields from the reordering input file which are
  unsigned    voice;    // used to match the reorder record to 
  unsigned    locIdx;   // the associated a cmXsNode_t record.
  unsigned    tick;     //
  unsigned    durtn;    //
  float       rval;     //
  unsigned    midi;     //
  
  cmXsNote_t* note;     // The cmXsNote_t* associated with this cmXsReorder_t record
  
  unsigned    dynIdx;       // cmInvalidIdx=ignore otherwise index into _cmXScoreDynMarkArray[]
  unsigned    newFlags;     // 0=ignore | kSostUp/DnXsFl | kDampUp/DnXsFl  | kTieEndXsFl
  unsigned    newTick;      // 0=ignore >0 new tick value
  unsigned    graceFlags;   // 0=ignore See kXXXGraceXsFl
  unsigned    graceGroupId; // 0=ignore >0=grace note group id
  unsigned    pitch;        // 0=ignore >0 new pitch
} cmXsReorder_t;

typedef struct _cmXScoreDynMark_str
{
  const cmChar_t* mark;   //
  unsigned        id;     // (1-17) maps to velocity
  unsigned        dyn;    // pppp - fff (1-9) as used by cmScore
  int             adjust; // {-1,0,+1}
  unsigned        vel;    // associated MIDI velocity
} _cmXScoreDynMark_t;

_cmXScoreDynMark_t _cmXScoreDynMarkArray[] =
{
  {"s",     1,  0,  0,   1}, // silent note
  {"pppp-", 2,  1, -1,   3},
  {"pppp",  3,  1,  0,  10},
  {"pppp+", 4,  1,  1,  22},
  {"ppp-",  4,  2, -1,  22},
  {"ppp",   5,  2,  0,  29},
  {"ppp+",  6,  2,  1,  36},
  {"pp-",   6,  3, -1,  36},
  {"pp",    7,  3,  0,  43},
  {"pp+",   8,  3,  1,  50},
  {"p-",    8,  4, -1,  50},
  {"p",     9,  4,  0,  57},
  {"p+",   10,  4,  1,  64},
  {"mp-",  10,  5, -1,  64},
  {"mp",   11,  5,  0,  71},
  {"mp+",  12,  5,  1,  78},
  {"mf-",  12,  6, -1,  78},
  {"mf",   13,  6,  0,  85},
  {"mf+",  14,  6,  1,  92},
  {"f-",   14,  7, -1,  92},
  {"f",    15,  7,  0,  99},
  {"f+",   16,  7,  1, 106}, 
  {"ff",   17,  8,  0, 113},
  {"ff+",  18,  8,  1, 120},
  {"fff",  19,  9,  0, 127},
  {NULL,0,0,0,0}
  
};


cmXsNote_t*  _cmXsReorderFindNote( cmXScore_t* p, unsigned measNumb, const cmXsReorder_t* r, unsigned iii )
{
  cmXsPart_t* pp = p->partL;
  for(; pp!=NULL; pp=pp->link)
  {
    cmXsMeas_t* mp = pp->measL;
    for(; mp!=NULL; mp=mp->link)
      if( mp->number == measNumb)
      {

        cmXsNote_t* np    = mp->noteL;
        int         index = 0;
        for(; np!=NULL; np=np->slink,++index)
        {


          if( 0 /*mp->number==19*/ )
            printf("voice: %i %i loc:%i %i  tick:%i %i pitch:%i %i rval:%f %f idx:%i %i \n",
              np->voice->id, r->voice, 
              np->locIdx ,  r->locIdx ,
              np->tick ,  r->tick ,
              np->pitch ,  r->midi ,
              np->rvalue, r->rval,
              index ,  r->idx);
              
          
          if( np->voice->id == r->voice &&
            np->locIdx == r->locIdx &&
            np->tick == r->tick &&
            //np->duration == r->durtn &&
            np->rvalue == r->rval &&
            np->pitch == r->midi &&
            index == r->idx )
          {
            //printf("Found: %i \n", r->midi);
            return np;
          }
        }
      }
  }

  cmErrMsg(&p->err,kSyntaxErrorXsRC,"Reorder note not found meas:%i index:%i.",measNumb,iii);
  return NULL;
}

void _cmXScoreInsertPedalEvent( cmXScore_t* p, const cmXsReorder_t* r, unsigned flags )
{
  // Create a new score event record
  cmXsNote_t* nn = cmLhAllocZ(p->lhH,cmXsNote_t,1);
  
  nn->uid   = p->nextUid++;
  nn->voice = r->note->voice;
  nn->meas  = r->note->meas;
  nn->flags = flags;
  
  // Pedal down events occur after the event they are attached to
  if( cmIsFlag(flags,kSostDnXsFl | kDampDnXsFl ) )
  {
    nn->tick  = r->note->tick + 1;
    _cmXScoreInsertNoteAfter(r->note,nn);
  }
  else
  {
    // Pedal up events occur before the event they are attached to
    if( cmIsFlag(flags,kSostUpXsFl | kDampUpXsFl ) )
    {
      nn->tick  = r->note->tick==0 ? 0 : r->note->tick - 1;
      _cmXScoreInsertNoteBefore(r->note,nn);
    }
    else
    { assert(0); }
  }

  
}

cmXsRC_t _cmXScoreReorderMeas( cmXScore_t* p, unsigned measNumb, cmXsReorder_t* rV, unsigned rN )
{
  unsigned i;

  if( rN == 0 )
    return kOkXsRC;

  // set the 'note' field on each cmXsReorder_t record
  for(i=0; i<rN; ++i)
    if((rV[i].note = _cmXsReorderFindNote(p,measNumb,rV+i,i)) == NULL )
      return kSyntaxErrorXsRC;


  // remove deleted notes
  for(i=0; i<rN; ++i)
    if( cmIsFlag(rV[i].newFlags,kDeleteXsFl) )
      if( _cmXScoreRemoveNote( rV[i].note ) != kOkXsRC )
        return cmErrMsg(&p->err,kSyntaxErrorXsRC,"Event marked to skip was not found in measure: %i",measNumb);
      
  cmXsMeas_t* mp  = rV[0].note->meas;
  cmXsNote_t* n0p = NULL;

  assert( mp->number == measNumb );

  // Reassign the slink of the cmXsNote_t records in this measure
  // according to their order in rV[].
  for(i=0; i<rN; ++i)
  {
    
    if( cmIsFlag(rV[i].newFlags,kDeleteXsFl) )
      continue;
    
    if( n0p == NULL )
      mp->noteL = rV[i].note;
    else
      n0p->slink = rV[i].note;

    // if a new tick was specified 
    if( rV[i].newTick != 0 )
      rV[i].note->tick = rV[i].newTick;

    // if a dynamic or velocity mark was included
    if( rV[i].dynIdx != cmInvalidIdx )
    {
      rV[i].note->dynamics = _cmXScoreDynMarkArray[ rV[i].dynIdx ].dyn;
      rV[i].note->vel      = _cmXScoreDynMarkArray[ rV[i].dynIdx ].vel;
    }

    // Set the dynamic fork begin/end flags for later _cmXScoreProcessDynamicForks()
    if( cmIsFlag(rV[i].newFlags,kDynBegForkXsFl) )
      rV[i].note->flags = cmSetFlag(rV[i].note->flags,kDynBegForkXsFl); 

    if( cmIsFlag(rV[i].newFlags,kDynEndForkXsFl) )
      rV[i].note->flags = cmSetFlag(rV[i].note->flags,kDynEndForkXsFl); 

    
    // if the tie end flag was set
    if( cmIsFlag(rV[i].newFlags,kTieEndXsFl) )
    {
      rV[i].note->flags |= kTieEndXsFl;
      rV[i].note->flags  = cmClrFlag( rV[i].note->flags, kOnsetXsFl );
      rV[i].newFlags     = cmClrFlag( rV[i].newFlags,    kTieEndXsFl);
    }

    // if a new note value was specified
    if( rV[i].pitch != 0 )
      rV[i].note->pitch = rV[i].pitch;
    
    rV[i].note->flags        |= rV[i].graceFlags;
    rV[i].note->graceGroupId  = rV[i].graceGroupId;


    
    n0p        = rV[i].note;
    n0p->slink = NULL;
  }

  
  // Insert new note records for pedal up/dn events.
  for(i=0; i<rN; ++i)
  {
    if( rV[i].newFlags != 0 )
    {
      if( cmIsFlag(rV[i].newFlags,kDampDnXsFl ) )
        _cmXScoreInsertPedalEvent(p,rV + i,kDampDnXsFl);

      if( cmIsFlag(rV[i].newFlags,kSostDnXsFl ) )
        _cmXScoreInsertPedalEvent(p,rV + i,kSostDnXsFl);
      
      if( cmIsFlag(rV[i].newFlags,kDampUpXsFl ) )
        _cmXScoreInsertPedalEvent(p,rV + i,kDampUpXsFl);

      if( cmIsFlag(rV[i].newFlags,kSostUpXsFl ) )
        _cmXScoreInsertPedalEvent(p,rV + i,kSostUpXsFl);      
    }
  }

  return kOkXsRC;

}

cmXsRC_t _cmXScoreReorderParseDyn(cmXScore_t* p, const cmChar_t* b, unsigned lineNumb, unsigned* dynIdxRef, unsigned* flagsRef, int measNumb )
{
  cmXsRC_t        rc        = kOkXsRC;
  const cmChar_t* s         = NULL;
  bool            begForkFl = false;
  bool            endForkFl = false;
    

  *dynIdxRef = cmInvalidIdx;

  // locate the '!' which indicates the start of a dynamic marking
  if( (s = strchr(b,'!')) == NULL )
    return rc;
  
  ++s; // increment past the '!'
  
  if( *s == 0 )
    return cmErrMsg(&p->err,kSyntaxErrorXsRC,"Unexpected end-of-line on dynamics parsing on line:%i.",lineNumb);

  // some dynamic markings are surrounded by parenthesis (to indicate a dynamic level with greater uncertainty)
  if( *s == '(' )
    ++s;  // skip the paren.

  if( *s == '!')
  {
    //printf("E %3i %5i %s\n",measNumb,lineNumb,b);
    endForkFl = true;
    ++s;      
  }   

  if( *s == 0 )
    return cmErrMsg(&p->err,kSyntaxErrorXsRC,"Unexpected end-of-line on dynamics parsing on line:%i.",lineNumb);


  // check for beg/end fork dynamic 
  if( isupper(*s) )
  {
    if( !endForkFl)
    {
      begForkFl=true;
      //printf("B %3i %5i %s\n",measNumb,lineNumb,b);
      
    }
  }
  else
  {
    if( endForkFl )
      return cmErrMsg(&p->err,kSyntaxErrorXsRC,"A double exclaimation point (end-of-dynamic fork) must always be followed by an upper case M,P, or F. See line:%i",lineNumb);
  }

  unsigned i      = 0;
  unsigned j      = 0;
  unsigned n      = 6;
  bool     doneFl = false;
  cmChar_t mark[n+1];
  memset(mark,0,n+1);
  
  for(i=0; j<n && doneFl==false; ++i)
  {
    char c = tolower(s[i]);
    
    switch(c)
    {
      case 's':
      case 'm':
      case 'p':
      case 'f':
      case '+':
      case '-':
        mark[j++] = c;
        break;
        
      case ')':   // ending paren.
      case 0:     // end of string
      case ' ':   // end of mark       
      case '\n':  // end of line
      default:    // anything else
        doneFl = true;
        break;
        
    }
  }

  if( !doneFl )
    return cmErrMsg(&p->err,kSyntaxErrorXsRC,"Illegal dynamic mark (%s) syntax on line:%i.",mark,lineNumb);

  // look up the dynamic mark in the reference table
  for(j=0; _cmXScoreDynMarkArray[j].mark!=NULL; ++j)
    if( strcmp(mark,_cmXScoreDynMarkArray[j].mark) == 0 )
      break;

  if( _cmXScoreDynMarkArray[j].mark == NULL )
    return cmErrMsg(&p->err,kSyntaxErrorXsRC,"The dynamic mark '%s' is not legal on line:%i.",mark,lineNumb);


  *dynIdxRef = j;
  
  if( begForkFl )
    *flagsRef = cmSetFlag(*flagsRef,kDynBegForkXsFl);

  if( endForkFl )
    *flagsRef = cmSetFlag(*flagsRef,kDynEndForkXsFl);
      
  return rc;
}


cmXsRC_t  _cmXScoreReorderParseFlags(cmXScore_t* p, const cmChar_t* b, unsigned line, unsigned* newFlagsRef )
{
  cmXsRC_t rc = kOkXsRC;
  const cmChar_t* s;
  bool doneFl = false;
  unsigned i = 0;
  
  *newFlagsRef = 0;
  
  // tilde indicates a pedal event
  if((s = strchr(b,'~')) == NULL )
    return rc;

  do
  {
    ++s;

    switch( *s )
    {
      case 'd':
        *newFlagsRef |= kSostDnXsFl;  // sostenuto pedal down just after this note onset
        break;

      case 'u':
        *newFlagsRef |= kSostUpXsFl;  // sostenuto pedal up just before this event
        break;

      case 'x':
        *newFlagsRef |= (kSostUpXsFl | kSostDnXsFl);  // sostenuto pedal up just before this event and sost down just after it.
        break;                
        
      case 'D':
        *newFlagsRef |= kDampDnXsFl;  // damper pedal down
        break;

      case 'U':
        *newFlagsRef |= kDampUpXsFl;  // damper pedal up
        break;

      case '_':
        *newFlagsRef |= kTieEndXsFl;   // set tie end flag
        break;

      case '&':
        *newFlagsRef |= kDeleteXsFl; // delete this evetn
        break;
        
      default:
        if( i == 0 )
          return cmErrMsg(&p->err,kSyntaxErrorXsRC,"Unexpected flag marking '%c' on line %i.",*s,line);
        
        doneFl = true;
    }

    ++i;
    
  }while(!doneFl);
  
  return rc;
}

cmXsRC_t  _cmXScoreReorderParseTick(cmXScore_t* p, const cmChar_t* b, unsigned line, unsigned* tickRef )
{
  cmXsRC_t rc = kOkXsRC;
  const cmChar_t* s;

  if((s = strchr(b,'@')) == NULL )
    return rc;

  ++s;

  if(!isdigit(*s))
      return cmErrMsg(&p->err,kSyntaxErrorXsRC,"Unexpected tick reorder value '%c' on line %i.",*s,line);

  if(sscanf(s,"%i",tickRef) != 1 )
      return cmErrMsg(&p->err,kSyntaxErrorXsRC,"tick reorder parse failed on line %i.",line);
  
  return rc;
}

cmXsRC_t  _cmXScoreReorderParseGrace(cmXScore_t* p, const cmChar_t* b, unsigned line, cmXsReorder_t* r, unsigned* graceGroupIdRef )
{
  cmXsRC_t        rc = kOkXsRC;
  const cmChar_t* s;

  if((s = strchr(b,'%')) == NULL )
    return rc;

  ++s;

  r->graceGroupId = *graceGroupIdRef;
  
  while(1)
  {
    switch(*s)
    {
      case 'b': r->graceFlags |= kBegGraceXsFl;                   break;        
      case 'a': r->graceFlags |= kAddGraceXsFl   | kEndGraceXsFl; break;
      case 's': r->graceFlags |= kSubGraceXsFl   | kEndGraceXsFl; break;
      case 'A': r->graceFlags |= kAFirstGraceXsFl| kEndGraceXsFl; break;
      case 'n': r->graceFlags |= kNFirstGraceXsFl| kEndGraceXsFl; break;
      case 'g': break;
        
      case '1':
        r->graceGroupId += 1;
        ++s;
        continue;
        

      case '%':
        *graceGroupIdRef += 1;
        ++s;
        continue;
        
      default:
        {
          return cmErrMsg(&p->err,kSyntaxErrorXsRC,"Unexpected grace note reorder character code %c on line %i.",*s,line);

          assert(0);
        }
    }
    
    break;
  }
  
  return rc;
  
}

cmXsRC_t  _cmXScoreReorderParsePitch(cmXScore_t* p, const cmChar_t* b, unsigned line, unsigned* pitchRef )
{
  cmXsRC_t rc = kOkXsRC;
  cmChar_t* s;
  cmChar_t buf[4];
  unsigned i,j;
  memset(buf,0,sizeof(buf));

  *pitchRef = 0;
  
  if((s = strchr(b,'$')) == NULL )
    return rc;

  ++s;

  j=2;
  for(i=0; i<j && *s; ++i,++s)
  {
    buf[i] = *s;
    
    if( i==1 && (*s=='#' || *s=='b') )
      j = 3;

    if( i==0 && strchr("ABCDEFG",*s)==NULL )
      return cmErrMsg(&p->err,kSyntaxErrorXsRC,"Illegal pitch letter ('%c')specification line %i.",*s,line);
    
    if( i==1 && !isdigit(*s) && *s!='#' && *s!='b' )
      return cmErrMsg(&p->err,kSyntaxErrorXsRC,"Illegal pitch level ('%c') specification line %i.",*s,line);

    if( i==2 && !isdigit(*s) )
      return cmErrMsg(&p->err,kSyntaxErrorXsRC,"Illegal pitch octave ('%c') specification line %i.",*s,line);
  }

  unsigned pitch = cmSciPitchToMidi(buf);

  if( pitch<kInvalidMidiByte)
    *pitchRef = pitch;
  else
  {
    rc = cmErrMsg(&p->err,kSyntaxErrorXsRC,"Pitch conversion from '%s' failed on line %i.",buf,line); 
  }
  return rc;  
}

cmXsRC_t _cmXsApplyEditFile( cmXScore_t* p, const cmChar_t* fn )
{
  typedef enum { kFindMeasStId, kFindEventStId, kReadEventStId } stateId_t;

  cmXsRC_t      rc       = kOkXsRC;
  cmFileH_t     fH       = cmFileNullHandle;
  cmChar_t*     b        = NULL;
  unsigned      bN       = 0;
  unsigned      ln       = 0;
  stateId_t     stateId  = kFindMeasStId;
  unsigned      rN       = 1024;
  unsigned      ri       = 0;
  unsigned      measNumb = 0;
  unsigned      graceGroupId = 1;
  cmXsReorder_t rV[ rN ];
  
  if( cmFileOpen(&fH,fn,kReadFileFl,p->err.rpt) != kOkFileRC )
  {
    rc = cmErrMsg(&p->err,kFileFailXsRC,"The reordering file '%s' could not be opened.",cmStringNullGuard(fn));
    return rc;
  }

  for(; cmFileGetLineAuto(fH,&b,&bN)==kOkFileRC; ++ln)
  {
    switch( stateId )
    {
      case kFindEventStId:  // scanning past labels to an event line
        {
          unsigned voice,loc;
          if( sscanf(b,"%i %i",&voice,&loc) != 2 )
            continue;

          stateId = kReadEventStId;
        }
        // fall through

      case kReadEventStId:
        {
          cmXsReorder_t r;
          char     pitchStr[4];

          memset(&r,0,sizeof(r));
          
          // parse an event line
          if( sscanf(b,"%i %i %i %i %i %f",&r.idx,&r.voice,&r.locIdx,&r.tick,&r.durtn,&r.rval) == 6 )
          {
            assert( strlen(b)>=52);
            int PC = 39; // text file column where first pitch char occurs
            
            if( b[PC] == ' ')
              r.midi = 0;
            else
            {
              pitchStr[0] = b[PC+0];
              pitchStr[1] = b[PC+1];
              pitchStr[2] = b[PC+2];              
              pitchStr[3] = 0;
              
              if( !isdigit(pitchStr[2]) )
                r.midi = 0;
              else
              {
                if( pitchStr[1] == ' ')
                {
                  pitchStr[1] = pitchStr[2];
                  pitchStr[2] = 0;
                }

                r.midi = cmSciPitchToMidi(pitchStr);
              }

            }

            
            // parse the flag edits following a '~'
            if((rc = _cmXScoreReorderParseFlags(p,b,ln+1, &r.newFlags)) != kOkXsRC )
              goto errLabel;

            // parse the dynamic marking following a '!'
            if((rc = _cmXScoreReorderParseDyn(p,b,ln+1,&r.dynIdx, &r.newFlags, measNumb)) != kOkXsRC )
              goto errLabel;
                        
            // parse the @newtick marker
            if((rc = _cmXScoreReorderParseTick(p, b, ln+1, &r.newTick)) != kOkXsRC )
              goto errLabel;

            // parse the %grace note marker
            if((rc = _cmXScoreReorderParseGrace(p, b, ln+1, &r, &graceGroupId)) != kOkXsRC )
              goto errLabel;

            // parse the $pitch marker
            if((rc =  _cmXScoreReorderParsePitch(p, b, ln+1, &r.pitch )) != kOkXsRC )
              goto errLabel;

            // process grace notes - these need to be processed separate from
            // the _cmXScoreReorderMeas() because grace notes may cross measure boundaries.
            /*
            if( r.graceType != 0 )
            {
              r.graceGroupId = graceGroupId;

              // if this is an end of a grace note group
              if( r.graceType != 'g' && r.graceType != 'b' )
              {
                graceGroupId += 1;
              }
              
            }
            */
            
            // store the record
            assert( ri < rN );

            rV[ri++] = r;

            continue;
          }

          // the end of the measure was encountered -
          // reorder the measure based on the cmXsReorder_t in rV[ri]
          if((rc =  _cmXScoreReorderMeas(p, measNumb, rV, ri )) != kOkXsRC )
            goto errLabel;

          ri = 0;

          stateId = kFindMeasStId;
          // fall through
        }

      case kFindMeasStId:  // scanning for a bar-line
        {
          char colon;
          if( sscanf(b,"%i %c",&measNumb,&colon) == 2 && colon == ':' )
          {
            //printf("meas: %i \n",measNumb);
            stateId = kFindEventStId;

          }
        }
        break;
    }

  }

  // If reorder records remain to be processed
  if( ri > 0 )
    if((rc =  _cmXScoreReorderMeas(p, measNumb, rV, ri )) != kOkXsRC )
      goto errLabel;
    
  
  // the ticks may have changed so the 'secs' and 'dsecs' must be updated
  _cmXScoreSetAbsoluteTime( p );

  // the bar lines should be the first event in the measure
  _cmXScoreFixBarLines(p);

  // resort to force the links to be correct 
  _cmXScoreSort(p);

  // process the grace notes.
  _cmXScoreProcessGraceNotes( p, graceGroupId );

  // inserting grace notes may have left the score unsorted
  _cmXScoreSort(p);
  
  // process the dynamic forks
  _cmXScoreProcessDynamicForks(p);

  //_cmXScoreReport(p, NULL, true );


 errLabel:
  cmFileClose(&fH);
  cmMemFree(b);
  return rc;
}



cmXsRC_t cmXScoreInitialize( cmCtx_t* ctx, cmXsH_t* hp, const cmChar_t* xmlFn, const cmChar_t* editFn )
{
  cmXsRC_t rc = kOkXsRC;

  if((rc = cmXScoreFinalize(hp)) != kOkXsRC )
    return rc;

  cmXScore_t* p = cmMemAllocZ(cmXScore_t,1);

  cmErrSetup(&p->err,&ctx->rpt,"XScore");

  // create a local linked heap
  if( cmLHeapIsValid( p->lhH = cmLHeapCreate(8196,ctx)) == false )
    return cmErrMsg(&p->err,kLHeapFailXsRC,"Lheap create failed.");

  // open the music xml file
  if( cmXmlAlloc(ctx, &p->xmlH, xmlFn) != kOkXmlRC )
  {
    rc = cmErrMsg(&p->err,kXmlFailXsRC,"Unable to open the MusicXML file '%s'.",cmStringNullGuard(xmlFn));
    goto errLabel;
  }

  //cmXmlPrint(p->xmlH,&ctx->rpt);

  // parse the part-list
  if((rc = _cmXScoreParsePartList( p )) != kOkXsRC )
    goto errLabel;

  // parse each score 'part'
  cmXsPart_t* pp = p->partL;
  for(; pp!=NULL; pp=pp->link)
    if((rc = _cmXScoreParsePart(p,pp)) != kOkXsRC )
      goto errLabel;

  // fill in the note->slink chain to link the notes in each measure in time order
  _cmXScoreSort(p);

  // kpl: 4/19/17 fix problem where notes ended up out of order by one tick _cmXScoreSpreadGraceNotes(p);

  _cmXScoreSort(p);

  _cmXScoreResolveTiesAndLoc(p);

  _cmXScoreRemoveDuplicateNotes(p);

  _cmXScoreSetMeasGroups(p,kEvenXsFl);
  _cmXScoreSetMeasGroups(p,kDynXsFl);
  _cmXScoreSetMeasGroups(p,kTempoXsFl);

  //_cmXScoreResolveOctaveShift(p);

  // CSV output initialize failed.
  if( cmCsvInitialize(&p->csvH,ctx) != kOkCsvRC )
    rc = cmErrMsg(&p->err,kCsvFailXsRC,"CSV output object create failed.");

  if( editFn != NULL )
  {
    if((rc = _cmXsApplyEditFile(p,editFn)) != kOkXsRC )
    {
      cmErrMsg(&ctx->err,rc,"XScore reorder failed.");
      goto errLabel;
    }
  }

  // assign durations to pedal down events
  _cmXScoreProcessPedals(p);

  // remove some notes which share a pitch and are overlapped or embedded within another note.
  _cmXScoreProcessOverlappingNotes(p);

  
 errLabel:
  if( rc != kOkXsRC )
    _cmXScoreFinalize(p);
  else
    hp->h = p;

  return rc;
}

cmXsRC_t cmXScoreFinalize( cmXsH_t* hp )
{
  cmXsRC_t rc = kOkXsRC;

  if( hp == NULL || cmXScoreIsValid(*hp)==false )
    return kOkXsRC;

  cmXScore_t* p = _cmXScoreHandleToPtr(*hp);

  if((rc = _cmXScoreFinalize(p)) != kOkXsRC )
    return rc;

  hp->h = NULL;

  return rc;
}

bool     cmXScoreIsValid( cmXsH_t h )
{ return h.h != NULL; }




/* CSV score columns
  kMidiFileIdColScIdx= 0,
  kTypeLabelColScIdx = 3,
  kDSecsColScIdx     = 4,
  kSecsColScIdx      = 5,
  kD0ColScIdx        = 9,
  kD1ColScIdx        = 10,
  kPitchColScIdx     = 11,
  kBarColScIdx       = 13,
  kSkipColScIdx      = 14,
  kEvenColScIdx      = 15,
  kGraceColScIdx     = 16,
  kTempoColScIdx     = 17,
  kFracColScIdx      = 18,
  kDynColScIdx       = 19,
  kSectionColScIdx   = 20,
  kRecdPlayColScIdx  = 21,
  kRemarkColScIdx    = 22
 */


cmXsRC_t _cmXScoreWriteCsvHdr( cmXScore_t* p )
{
  const cmChar_t* s[] =
  {
    "id","trk","evt","opcode","dticks","micros","status",
    "meta","ch","d0","d1","arg0","arg1","bar","skip",
    "even","grace","tempo","t_frac","dyn","section","play_recd","remark",NULL
  };

  cmCsvCell_t* lcp = NULL;

  if(  cmCsvAppendRow( p->csvH, &lcp, cmCsvInsertSymText(p->csvH,s[0]), 0, 0 ) != kOkCsvRC )
    return cmErrMsg(&p->err,kCsvFailXsRC,"CSV append row failed.");

  unsigned i;
  for(i=1; s[i]!=NULL; ++i)
  {
    if( cmCsvInsertIdentColAfter(p->csvH, lcp, &lcp, s[i], 0 ) != kOkCsvRC )
      return cmErrMsg(&p->err,kCsvFailXsRC,"CSV error inserting CSV title %i.\n",i);

  }

  return kOkXsRC;
}


cmXsRC_t _cmXScoreWriteCsvBlankCols( cmXScore_t* p, unsigned cnt, cmCsvCell_t** leftCellPtrPtr )
{
  unsigned i;
  for(i=0; i<cnt; ++i)
    if( cmCsvInsertIdentColAfter(p->csvH,*leftCellPtrPtr,leftCellPtrPtr,"",0) != kOkCsvRC )
      return cmErrMsg(&p->err,kCsvFailXsRC,"CSV output failed on blank column.");

  return kOkCsvRC;
}

const cmChar_t* _cmXScoreTranslateDynamics( cmXScore_t* p,  const cmXsNote_t* np, cmChar_t* buf, unsigned bufN )
{
  if( cmIsFlag(np->flags,kDynXsFl) && np->dynamics != 0 )
  {
    const cmChar_t* dynStr = NULL;
    switch(np->dynamics)
    {
      case 1: dynStr = "pppp"; break;
      case 2: dynStr = "ppp";  break;
      case 3: dynStr = "pp";   break;
      case 4: dynStr = "p";    break;
      case 5: dynStr = "mp";   break;
      case 6: dynStr = "mf";   break;
      case 7: dynStr = "f";    break;
      case 8: dynStr = "ff";   break;
      case 9: dynStr = "fff";  break;
      default:
        cmErrMsg(&p->err,kSyntaxErrorXsRC,"An invalid dynamic value (%i) was encountered.",np->dynamics);
        goto errLabel;
    }

    if( np->dynGroupId == 0 )
      snprintf(buf,bufN,"%s",dynStr);
    else
      snprintf(buf,bufN,"%s %i",dynStr,np->dynGroupId);

    return buf;
  }
 errLabel:
  return "";
}

const cmChar_t* cmXsFormatMeasurementCsvField( unsigned flags, unsigned fl, char abbrev, unsigned sectionId, char* buf, unsigned bufN )
{
  assert( bufN > 1 );
  buf[0] = ' ';
  buf[1] = 0;
  
  if( cmIsFlag(flags,fl) )
  {
    if( sectionId != 0 )
      snprintf(buf,bufN-1,"%c %i%c",abbrev,sectionId, cmIsFlag(flags,kHeelXsFl)?'*':' ');
    else
      buf[0] = abbrev;
  }
  return buf;
}


cmXsRC_t _cmXScoreWriteCsvRow(
  cmXScore_t*     p,
  unsigned        rowIdx,
  unsigned        uid,
  unsigned        bar,
  const cmChar_t* sectionStr,
  const cmChar_t* opCodeStr,
  double          dsecs,
  double          secs,
  unsigned        d0,
  unsigned        d1,
  unsigned        pitch,   // set to -1 if the pitch is not valid
  double          frac,
  const cmChar_t* dynStr,
  unsigned        flags,
  const cmChar_t* evenStr,
  const cmChar_t* tempoStr)
{
  cmXsRC_t     rc  = kOkXsRC;
  cmCsvCell_t* lcp = NULL;

  // append an empty row to the CSV object
  if(  cmCsvAppendRow( p->csvH, &lcp, cmCsvInsertSymUInt(p->csvH, rowIdx ), 0, 0 ) != kOkCsvRC )
  {
    rc = cmErrMsg(&p->err,kCsvFailXsRC,"CSV append row failed.");
    goto errLabel;
  }

  /*
  // col 0 : blanks
  if( cmCsvInsertUIntColAfter(p->csvH, lcp, &lcp, rowIdx,    0 ) != kOkCsvRC )
  {
    rc = cmErrMsg(&p->err,kCsvFailXsRC,"CSV output row index failed.");
    goto errLabel;
  }
  */

  // col 1 : track (always 1)
  if( cmCsvInsertUIntColAfter(p->csvH,lcp,&lcp,1,0) != kOkCsvRC )
  {
    rc = cmErrMsg(&p->err,kCsvFailXsRC,"CSV insert failed on 'd0'.");
    goto errLabel;
  }

  // col 2 : evt (set to event uid, or blank if uid == -1)
  if( uid == -1 )
  {
    if((rc = _cmXScoreWriteCsvBlankCols(p,1,&lcp)) != kOkXsRC )
      goto errLabel;
  }
  else
  if( cmCsvInsertUIntColAfter(p->csvH,lcp,&lcp,uid,0) != kOkCsvRC )
  {
    rc = cmErrMsg(&p->err,kCsvFailXsRC,"CSV insert failed on 'd0'.");
    goto errLabel;
  }

  // col 3 : output the opcode
  if( cmCsvInsertIdentColAfter(p->csvH,lcp,&lcp,opCodeStr,0) != kOkCsvRC )
  {
    rc = cmErrMsg(&p->err,kCsvFailXsRC,"CSV insert failed on opcode label.");
    goto errLabel;
  }

  // col 4 : dsecs
  if( cmCsvInsertDoubleColAfter(p->csvH,lcp,&lcp,dsecs,0) != kOkCsvRC )
  {
    rc = cmErrMsg(&p->err,kCsvFailXsRC,"CSV insert failed on 'dsecs'.");
    goto errLabel;
  }

  // col 5 : secs
  if( cmCsvInsertDoubleColAfter(p->csvH,lcp,&lcp,secs,0) != kOkCsvRC )
  {
    rc = cmErrMsg(&p->err,kCsvFailXsRC,"CSV insert failed on 'secs'.");
    goto errLabel;
  }

  // cols 6,7,8 blanks
  if((rc = _cmXScoreWriteCsvBlankCols(p,3,&lcp)) != kOkXsRC )
    goto errLabel;

  // col 9 : d0
  if( cmCsvInsertUIntColAfter(p->csvH,lcp,&lcp,d0,0) != kOkCsvRC )
  {
    rc = cmErrMsg(&p->err,kCsvFailXsRC,"CSV insert failed on 'd0'.");
    goto errLabel;
  }

  // col 10 : d1
  if( cmCsvInsertUIntColAfter(p->csvH,lcp,&lcp,d1,0) != kOkCsvRC )
  {
    rc = cmErrMsg(&p->err,kCsvFailXsRC,"CSV insert failed on 'd1'.");
    goto errLabel;
  }

  // col 11 : pitch
  if( pitch == -1 )
  {
    if((rc = _cmXScoreWriteCsvBlankCols(p,1,&lcp)) != kOkXsRC )
      goto errLabel;
  }
  else
  {
    if( cmCsvInsertIdentColAfter(p->csvH,lcp,&lcp,cmMidiToSciPitch(pitch,NULL,0),0) != kOkCsvRC )
    {
      rc = cmErrMsg(&p->err,kCsvFailXsRC,"CSV insert failed on 'pitch'.");
      goto errLabel;
    }
  }

  // col 12 : blanks
  if((rc = _cmXScoreWriteCsvBlankCols(p,1 + (cmIsFlag(flags,kBarXsFl) ? 0 : 1), &lcp)) != kOkXsRC )
    goto errLabel;

  // col 13 : bar number
  if( cmIsFlag(flags,kBarXsFl) )
  {
    if( cmCsvInsertUIntColAfter(p->csvH,lcp,&lcp,bar,0) != kOkCsvRC )
    {
      rc = cmErrMsg(&p->err,kCsvFailXsRC,"CSV insert failed on 'pitch'.");
      goto errLabel;
    }
  }

  // col 14 : skip (blank for now)
  if((rc = _cmXScoreWriteCsvBlankCols(p,1,&lcp)) != kOkXsRC )
    goto errLabel;

  // col 15: even (all grace notes are 'even' notes
  if( cmCsvInsertQTextColAfter(p->csvH,lcp,&lcp, evenStr, 0) != kOkCsvRC )
  {
    rc = cmErrMsg(&p->err,kCsvFailXsRC,"CSV insert failed on eveness flag label.");
    goto errLabel;
  }

  // col 16: grace
  if( cmCsvInsertIdentColAfter(p->csvH,lcp,&lcp,cmIsFlag(flags,kGraceXsFl) ? "g" : "",0) != kOkCsvRC )
  {
    rc = cmErrMsg(&p->err,kCsvFailXsRC,"CSV insert failed on eveness flag label.");
    goto errLabel;
  }

  // col 17: tempo
  if( cmCsvInsertQTextColAfter(p->csvH,lcp,&lcp,tempoStr,0) != kOkCsvRC )
  {
    rc = cmErrMsg(&p->err,kCsvFailXsRC,"CSV insert failed on eveness flag label.");
    goto errLabel;
  }

  // col 18: frac
  if( frac == 0 )
  {
    if((rc = _cmXScoreWriteCsvBlankCols(p,1,&lcp)) != kOkXsRC )
      goto errLabel;
  }
  else
  {
    if( cmCsvInsertDoubleColAfter(p->csvH,lcp,&lcp,1.0/frac,0) != kOkCsvRC )
    {
      rc = cmErrMsg(&p->err,kCsvFailXsRC,"CSV insert failed on 't frac'.");
      goto errLabel;
    }
  }

  // col 19: dynamic marking
  if(  cmCsvInsertQTextColAfter(p->csvH, lcp, &lcp, dynStr, 0 ) != kOkCsvRC )
  {
    rc = cmErrMsg(&p->err,kCsvFailXsRC,"CSV insert failed on 'dynamics'.");
    goto errLabel;
  }

  //if((rc = _cmXScoreWriteCsvBlankCols(p,1,&lcp)) != kOkXsRC )
  //  goto errLabel;

  // col 20: section
  if( cmCsvInsertIdentColAfter(p->csvH,lcp,&lcp,sectionStr!=NULL ? sectionStr : "",0) != kOkCsvRC )
  {
    rc = cmErrMsg(&p->err,kCsvFailXsRC,"CSV insert failed on eveness flag label.");
    goto errLabel;
  }

  // col 21, 22 : recd-play, remark (blank for now)
  if((rc = _cmXScoreWriteCsvBlankCols(p,2,&lcp)) != kOkXsRC )
    goto errLabel;

 errLabel:
  return rc;
}

cmXsRC_t cmXScoreWriteCsv( cmXsH_t h, int begMeasNumb, const cmChar_t* csvFn )
{
  cmXsRC_t        rc           = kOkXsRC;
  cmXScore_t*     p            = _cmXScoreHandleToPtr(h);
  unsigned        rowIdx       = 1;
  const cmChar_t* sectionIdStr = NULL;
  double          baseSecs     = -1;

  if( !cmCsvIsValid(p->csvH) )
    return cmErrMsg(&p->err,kCsvFailXsRC,"The CSV output object is not initialized.");

  if((rc = _cmXScoreWriteCsvHdr( p )) != kOkXsRC )
    goto errLabel;

  cmXsPart_t* pp = p->partL;
  for(; pp!=NULL; pp=pp->link)
  {
    cmXsMeas_t* mp = pp->measL;
            
    for(; mp!=NULL; mp=mp->link)
    {

      if( mp->number < begMeasNumb)
        continue;
      
      cmXsNote_t* np = mp->noteL;

      if( baseSecs == -1 )
        baseSecs = np->secs;
      
      for(; np!=NULL; np=np->slink)
      {
        double thisSecs = np->secs - baseSecs;
        
        // if this is a section event
        if( cmIsFlag(np->flags,kSectionXsFl) )
          sectionIdStr = np->tvalue;
        
        // if this is a bar event
        if( cmIsFlag(np->flags,kBarXsFl)  )
        {
          _cmXScoreWriteCsvRow(p,rowIdx,-1,mp->number,sectionIdStr,"bar",np->dsecs,thisSecs,0,0,-1,0,"",np->flags,"","");
          sectionIdStr = NULL;
        }
        else
        {
          // if this is a pedal event
          if( cmIsFlag(np->flags,kDampDnXsFl|kDampUpXsFl|kDampUpDnXsFl|kSostDnXsFl|kSostUpXsFl) )
          {
            unsigned d0 = cmIsFlag(np->flags,kSostDnXsFl |kSostUpXsFl) ? 66 : 64; // pedal MIDI ctl id
            unsigned d1 = cmIsFlag(np->flags,kDampDnXsFl|kSostDnXsFl) ? 64 : 0;  // pedal-dn: d1>=64 pedal-up:<64
            _cmXScoreWriteCsvRow(p,rowIdx,-1,mp->number,sectionIdStr,"ctl",np->dsecs,thisSecs,d0,d1,-1,0,"",np->flags,"","");
            sectionIdStr = NULL;
            
            if( cmIsFlag(np->flags,kDampUpDnXsFl) )
            {
              rowIdx += 1;
              double millisecond = 0.0;
              _cmXScoreWriteCsvRow(p,rowIdx,-1,mp->number,sectionIdStr,"ctl",millisecond,thisSecs+millisecond,d0,64,-1,0,"",np->flags,"","");
            }

          }
          else
          {

            // if this is a sounding note event
            if( cmIsFlag(np->flags,kOnsetXsFl) )
            {
              unsigned bufN = 128;
              cmChar_t ebuf[ bufN+1]; ebuf[bufN] = 0;
              cmChar_t dbuf[ bufN+1]; dbuf[bufN] = 0;
              cmChar_t tbuf[ bufN+1]; tbuf[bufN] = 0;
                            
              double          frac  = np->rvalue + (cmIsFlag(np->flags,kDotXsFl) ? (np->rvalue/2) : 0);
              const cmChar_t* dyn   = _cmXScoreTranslateDynamics( p,  np, dbuf, bufN );
              unsigned        vel   = np->vel==0 ? 60 : np->vel;

              // 
              _cmXScoreWriteCsvRow(p,rowIdx,np->uid,mp->number,sectionIdStr,"non",np->dsecs,thisSecs,np->pitch,vel,np->pitch,frac,dyn,np->flags,
                cmXsFormatMeasurementCsvField(np->flags, kEvenXsFl, 'e', np->evenGroupId,  ebuf, bufN ),
                cmXsFormatMeasurementCsvField(np->flags, kTempoXsFl,'t', np->tempoGroupId, tbuf, bufN ));
              
              sectionIdStr = NULL;
            }
          }
        }

        rowIdx += 1;
      }

    }
  }

  // Section labels are output on the next bar/pedal/note
  // but what if there is no bar pedal note-on after the section label
  if( sectionIdStr != NULL )
    cmErrMsg(&p->err,kSyntaxErrorXsRC,"The section label '%s' was ignored because it was not followed by a score event.",sectionIdStr);

  if( cmCsvWrite( p->csvH, csvFn ) != kOkCsvRC )
    rc = cmErrMsg(&p->err,kCsvFailXsRC,"The CSV output write failed on file '%s'.",csvFn);

 errLabel:
  return rc;
}

bool  _cmXsIsCsvValid(cmCtx_t* ctx, cmXsH_t h, const cmChar_t* outFn)
{
  bool        retFl = true;
  cmScH_t     scH   = cmScNullHandle;
  double      srate = 44100.0;
  cmSymTblH_t stH   = cmSymTblCreate(cmSymTblNullHandle, 0, ctx );
    
  if( cmScoreInitialize( ctx, &scH, outFn, srate, NULL, 0, NULL, NULL, stH) != kOkScRC )
  {
    cmErrMsg(&ctx->err,kFileFailXsRC,"The generated CSV file (%s) could not be parsed.",cmStringNullGuard(outFn));
    retFl = false;
  }
  else
  {
    //cmScorePrintSets(scH,&ctx->rpt);
    //cmScorePrint(scH,&ctx->rpt);
      
    cmScoreFinalize(&scH);
  }

  cmSymTblDestroy(&stH);

  return retFl;
}

void _cmXScoreReportTitle( cmRpt_t* rpt )
{
  cmRptPrintf(rpt,"      idx voc  loc    tick  durtn rval        flags\n");
  cmRptPrintf(rpt,"      --- --- ----- ------- ----- ---- --- ---------------\n");
}

void _cmXScoreReportNote( cmRpt_t* rpt, const cmXsNote_t* note,unsigned index )
{
  const cmChar_t* B  = cmIsFlag(note->flags,kBarXsFl)       ? "|" : "-";
  const cmChar_t* R  = cmIsFlag(note->flags,kRestXsFl)      ? "R" : "-";
  const cmChar_t* G  = cmIsFlag(note->flags,kGraceXsFl)     ? "G" : "-";
  const cmChar_t* D  = cmIsFlag(note->flags,kDotXsFl)       ? "." : "-";
  const cmChar_t* C  = cmIsFlag(note->flags,kChordXsFl)     ? "C" : "-";
  const cmChar_t* P  = cmIsFlag(note->flags,kDampDnXsFl)    ? "V" : "-";
  const cmChar_t* s  = cmIsFlag(note->flags,kSostDnXsFl)    ? "{" : "-";
  const cmChar_t* S  = cmIsFlag(note->flags,kSectionXsFl)   ? "S" : "-";
  const cmChar_t* H  = cmIsFlag(note->flags,kHeelXsFl)      ? "H" : "-";
  const cmChar_t* T0 = cmIsFlag(note->flags,kTieBegXsFl)    ? "T" : "-";
  const cmChar_t* T1 = cmIsFlag(note->flags,kTieEndXsFl)    ? "_" : "-";
  const cmChar_t* O  = cmIsFlag(note->flags,kOnsetXsFl)     ? "*" : "-";

  const cmChar_t* e  = cmIsFlag(note->flags,kEvenXsFl)      ? "e" : "-";
  const cmChar_t* d  = cmIsFlag(note->flags,kDynXsFl)       ? "d" : "-";
  const cmChar_t* t  = cmIsFlag(note->flags,kTempoXsFl)     ? "t" : "-";

  if( cmIsFlag(note->flags,kEvenEndXsFl) )  e="E";
  if( cmIsFlag(note->flags,kDynEndXsFl) )   d="D";
  if( cmIsFlag(note->flags,kTempoEndXsFl) ) t="T";
  
  P = cmIsFlag(note->flags,kDampUpXsFl)   ? "^" : P;
  P = cmIsFlag(note->flags,kDampUpDnXsFl) ? "X" : P;
  s = cmIsFlag(note->flags,kSostUpXsFl)    ? "}" : s;
  //const cmChar_t* N = note->pitch==0 ? " " : cmMidiToSciPitch( note->pitch, NULL, 0 );

  cmChar_t N[] = {'\0','\0','\0','\0'};
  cmChar_t acc = note->alter==-1?'b':(note->alter==1?'#':' ');
  snprintf(N,4,"%c%c%1i",note->step,acc,note->octave);

  cmRptPrintf(rpt,"      %3i %3i %5i %7i %5i %4.1f %3s %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
    index,
    note->voice->id,
    note->locIdx,
    note->tick,
    note->tied_dur,
    note->rvalue,
    N,B,R,G,D,C,e,d,t,P,s,S,H,T0,T1,O);

  if( cmIsFlag(note->flags,kSectionXsFl) )
    cmRptPrintf(rpt," %s",cmStringNullGuard(note->tvalue));

  if( cmIsFlag(note->flags,kMetronomeXsFl) )
    cmRptPrintf(rpt," %i bpm",note->duration);

  if( note->evenGroupId != 0 )
    cmRptPrintf(rpt," e=%i",note->evenGroupId);

  if( note->dynGroupId != 0 )
    cmRptPrintf(rpt," d=%i",note->dynGroupId);

  if( note->tempoGroupId != 0 )
    cmRptPrintf(rpt," t=%i",note->tempoGroupId);

  if( note->graceGroupId != 0)
    cmRptPrintf(rpt," g=%i",note->graceGroupId);

  if( note->dynamics != 0)
    cmRptPrintf(rpt," dyn=%i %i",note->dynamics,note->vel);
  
  /*
  if( cmIsFlag(note->flags,kBegGraceXsFl) )
    cmRptPrintf(rpt," B");

  if( cmIsFlag(note->flags,kEndGraceXsFl) )
    cmRptPrintf(rpt," E");
    
  if( cmIsFlag(note->flags,kDynBegForkXsFl) )
    cmRptPrintf(rpt," B");

  if( cmIsFlag(note->flags,kDynEndForkXsFl) )
    cmRptPrintf(rpt," E");
  */
    
}


void  _cmXScoreReport( cmXScore_t* p, cmRpt_t* rpt, bool sortFl )
{
  if( rpt == NULL )
    rpt = p->err.rpt;
  
  cmXsPart_t* pp = p->partL;

  for(; pp!=NULL; pp=pp->link)
  {
    cmRptPrintf(rpt,"Part:%s\n",pp->idStr);

    const cmXsMeas_t* meas = pp->measL;
    for(; meas!=NULL; meas=meas->link)
    {
      unsigned idx = 0;

      cmRptPrintf(rpt,"  %i : div:%i beat:%i beat-type:%i (%i)\n",meas->number,meas->divisions,meas->beats,meas->beat_type,meas->divisions*meas->beats);

      _cmXScoreReportTitle(rpt);

      if( sortFl )
      {

        const cmXsNote_t* note = meas->noteL;
        unsigned t0 = 0;
        unsigned t1 = 0;
        for(; note!=NULL; note=note->slink,++idx)
        {
          _cmXScoreReportNote(rpt,note,idx);

          t1 = note->slink==NULL ? note->tick : note->slink->tick;

          if( !(t0 <= note->tick && note->tick <= t1) )
          {
            cmRptPrintf(rpt," +");
          }

          t0 = note->tick;


          if( note->slink!=NULL  || note->voice->id==0)
            cmRptPrintf(rpt,"\n");
          else
            cmRptPrintf(rpt," %i\n", note->tick + note->duration);
        }

      }
      else
      {

        const cmXsVoice_t* v = meas->voiceL;
        for(; v!=NULL; v=v->link)
        {
          const cmXsNote_t* note = v->noteL;

          cmRptPrintf(rpt,"    voice:%i\n",v->id);

          for(; note!=NULL; note=note->mlink)
          {
            _cmXScoreReportNote(rpt,note,idx);

            if( note->mlink!=NULL || note->voice->id==0)
              cmRptPrintf(rpt,"\n");
            else
              cmRptPrintf(rpt," %i\n", note->tick + note->duration);
          }

        }
      }
    }
  }
}

void  cmXScoreReport( cmXsH_t h, cmRpt_t* rpt, bool sortFl )
{
  cmXScore_t* p  = _cmXScoreHandleToPtr(h);
  return _cmXScoreReport(p,rpt,sortFl);
}

void _cmXScoreGenEditFileWrite( void* arg, const cmChar_t* text )
{
  if( text != NULL && arg != NULL )
  {
    cmFileH_t* hp = (cmFileH_t*)arg;
    cmFilePrint(*hp,text);
  }
}

cmXsRC_t cmXScoreGenEditFile( cmCtx_t* ctx, const cmChar_t* xmlFn, const cmChar_t* outFn )
{
  cmXsH_t   xsH = cmXsNullHandle;
  cmFileH_t fH  = cmFileNullHandle;
  cmXsRC_t  rc  = kOkXsRC;
  cmErr_t   err;
  cmRpt_t   rpt;

  cmErrSetup(&err,&ctx->rpt,"cmXScoreGenEditFile");
  cmRptSetup(&rpt,_cmXScoreGenEditFileWrite,_cmXScoreGenEditFileWrite,&fH);

  if((rc = cmXScoreInitialize(ctx,&xsH,xmlFn,NULL)) != kOkXsRC )
    return rc;

  if( cmFileOpen(&fH,outFn,kWriteFileFl,&ctx->rpt) != kOkFileRC )
  {
    cmErrMsg(&err,kFileFailXsRC,"Unable to open the output file '%s'.",cmStringNullGuard(outFn));
    goto errLabel;
  }
  
  cmXScoreReport(xsH,&rpt,true);
  
 errLabel:
  
  if( cmFileClose(&fH) != kOkFileRC )
    rc = cmErrMsg(&err,kFileFailXsRC,"File close failed on '%s'.",cmStringNullGuard(outFn));
  
  cmXScoreFinalize(&xsH);

  return rc;
}

typedef struct
{
  unsigned ival;
  double   fval;
  unsigned cnt;
} cmXsHist_t;

void _cmXsHistUpdateI( cmXsHist_t* hist, unsigned histN, unsigned ival )
{
  unsigned i;
  
  for(i=0; i<histN && hist[i].cnt!=0; ++i)
    if( hist[i].ival == ival )
      break;

  if( i==histN )
    return;
  
  hist[i].ival = ival;
  hist[i].cnt += 1;
  
}

void _cmXsHistUpdateF( cmXsHist_t* hist, unsigned histN, double fval )
{
  unsigned i;
  
  for(i=0; i<histN && hist[i].cnt!=0; ++i)
    if( hist[i].fval == fval )
      break;

  if( i==histN )
    return;
  
  hist[i].fval = fval;
  hist[i].cnt += 1;
}

unsigned _cmXsHistValue( cmXsHist_t* hist, unsigned histN )
{
  unsigned i,n;
  for(i=0,n=0; i<histN && hist[i].cnt>0; ++i)
    n += 1;

  return n;
}

// Measure the score complexity for the the time window 'wndSecs' seconds
// prior to the note n1 and following n0.
const cmXsNote_t*  _cmXsMeasComplexityInWindow( const cmXsNote_t* n0, cmXsNote_t* n1, double wndSecs )
{
  const cmXsNote_t* n2          = NULL;
  unsigned    l_pch_0     = 0;
  unsigned    l_pch_value = 0;
  unsigned    l_pch_cnt   = n1->staff==1 ? 0 : 1;
  unsigned    r_pch_0     = n1->staff==1 ? 1 : 0;
  unsigned    r_pch_value = 0;
  unsigned    r_pch_cnt   = 0;
  unsigned    i           = 0;


  unsigned   histN = 100;
  cmXsHist_t velHist[ histN ];
  cmXsHist_t rymHist[ histN ];

  memset(velHist,0,sizeof(velHist));
  memset(rymHist,0,sizeof(rymHist));

  const cmXsNote_t* n = n0;
  
  while(n!=NULL && n != n1)
  {
    // if this event is less than wndSecs behind 'n1' and is not a sounding note ...
    if( n1->secs - n->secs <= wndSecs && cmIsFlag(n->flags,kOnsetXsFl) )
    {
      _cmXsHistUpdateI( velHist,  histN, n->dynamics );
      _cmXsHistUpdateF( rymHist,  histN, n->rvalue );

      switch( n->staff )
      {
        case 1:        // treble cleff
          if( i > 0 )
          {
            r_pch_value += r_pch_0 > n->pitch ? r_pch_0-n->pitch : n->pitch-r_pch_0;
            r_pch_cnt   += 1;
          }
          
          r_pch_0 = n->pitch;
          break;
          
        case 2:        // bass cleff
          if( i > 0 )
          {
            l_pch_value += l_pch_0 > n->pitch ? l_pch_0-n->pitch : n->pitch-l_pch_0;
            l_pch_cnt   += 1;
          }
          
          l_pch_0 = n->pitch;                      
          break;
          
        default:
          { assert(0); }
      }
      
      // track the first note that is inside the window
      if( i == 0 )
        n2 = n;

      // count the number of notes in the window
      i += 1;

    }

    cmXsMeas_t* m = n->meas;
    
    // advance th note pointer
    n  = n->slink;

    // if we have reached the end of a measure
    if( n == NULL )
    {
      if( m != NULL )
      {
        m = m->link;
        if( m != NULL )
          n = m->noteL;
      }
    }
    
  }

  // update the cplx record in n1 with the results of this window analysis
  n1->cplx.sum_d_vel  = _cmXsHistValue( velHist, histN );
  n1->cplx.sum_d_rym  = _cmXsHistValue( rymHist, histN );
  n1->cplx.sum_d_lpch = l_pch_value;
  n1->cplx.sum_n_lpch = l_pch_cnt;
  n1->cplx.sum_d_rpch = r_pch_value;
  n1->cplx.sum_n_rpch = r_pch_cnt;
  
  return n2;
}

// Measure the score complexity and fill in the cmXsComplexity_t record associated
// with the cmXsNote_t record of each sounding note.
cmXsRC_t _cmXsMeasComplexity( cmXsH_t h, double wndSecs )
{
  cmXsRC_t    rc = kOkXsRC;
  cmXScore_t* p  = _cmXScoreHandleToPtr(h);  
  cmXsPart_t* pp = p->partL;

  memset(&p->cplx_max,0,sizeof(p->cplx_max));
  
  const cmXsNote_t* n0 = NULL;
  
  // for each part
  for(; pp!=NULL; pp=pp->link)
  {
    cmXsMeas_t* mp = pp->measL;

    // for each measure
    for(; mp!=NULL; mp=mp->link)
    {
      cmXsNote_t* n1 = mp->noteL;


      // for each note in this measure
      for(; n1!=NULL; n1=n1->slink)
        if( cmIsFlag(n1->flags,kOnsetXsFl) )
        {
          if( n0 == NULL )
            n0 = n1;
          else
            if((n0 = _cmXsMeasComplexityInWindow(n0,n1,wndSecs)) == NULL )
              n0 = n1;

          // track the max value for all complexity values to allow 
          // eventual normalization of the complexity values
          p->cplx_max.sum_d_vel  = cmMax(p->cplx_max.sum_d_vel, n1->cplx.sum_d_vel);
          p->cplx_max.sum_d_rym  = cmMax(p->cplx_max.sum_d_rym, n1->cplx.sum_d_rym);
          p->cplx_max.sum_d_lpch = cmMax(p->cplx_max.sum_d_lpch,n1->cplx.sum_d_lpch);
          p->cplx_max.sum_n_lpch = cmMax(p->cplx_max.sum_n_lpch,n1->cplx.sum_n_lpch);
          p->cplx_max.sum_d_rpch = cmMax(p->cplx_max.sum_d_rpch,n1->cplx.sum_d_rpch);
          p->cplx_max.sum_n_rpch = cmMax(p->cplx_max.sum_n_rpch,n1->cplx.sum_n_rpch);
        
        }
    }
  }
  return rc;
}

cmXsRC_t _cmXsWriteMidiFile( cmCtx_t* ctx, cmXsH_t h, int beginMeasNumb, int beginBPM, const cmChar_t* dir, const cmChar_t* fn )
{
  cmXsRC_t rc = kOkXsRC;
  cmXScore_t* p = _cmXScoreHandleToPtr(h);
  
  if( p->partL==NULL || p->partL->measL == NULL )
    return rc;
  
  cmMidiFileH_t   mfH        = cmMidiFileNullHandle;
  unsigned        trkN       = 2;
  unsigned        ticksPerQN = p->partL->measL->divisions;
  const cmChar_t* outFn      = cmFsMakeFn(dir,fn,"mid",NULL);
  unsigned        baseTick   = -1;
  unsigned        bpm        = beginBPM==0 ? 60 : beginBPM;

  //if( cmMidiFileCreate( ctx, &mfH, trkN, ticksPerQN ) != kOkMfRC )
  //  return cmErrMsg(&p->err,kMidiFailXsRC,"Unable to create the MIDI file object.");
  
  cmXsPart_t* pp = p->partL;

  // for each part
  for(; pp!=NULL; pp=pp->link)
  {
    cmXsMeas_t* mp = pp->measL;

    // for each measure
    for(; mp!=NULL; mp=mp->link)
    {

      // skip all measures until we reach the first measure to output
      if(mp->number < beginMeasNumb)
        continue;

      // if the MIDI file has not yet been created 
      if( !cmMidiFileIsValid(mfH) )
      {
        ticksPerQN = mp->divisions;

        // create the MIDI file
        if( cmMidiFileCreate( ctx, &mfH, trkN, ticksPerQN ) != kOkMfRC )
        {
          rc = cmErrMsg(&p->err,kMidiFailXsRC,"Unable to create the MIDI file object.");
          goto errLabel;
        }

        // set the starting tempo
        cmMidFileInsertTrackTempoMsg(mfH, 0, 0, bpm );
        
      }
      
      cmXsNote_t* np = mp->noteL;

      if( baseTick == -1 )
        baseTick = np->tick;

      if( mp->divisions != ticksPerQN )
        cmErrWarnMsg(&p->err,kMidiFailXsRC,"The 'tick per quarter note' (divisions) field in measure %i does not match the value in the first measure (%i).",mp->divisions,ticksPerQN);

      unsigned ni = 0;
      // for each note in this measure
      for(; np!=NULL; np=np->slink,++ni)
      {
        unsigned thisTick = np->tick - baseTick;
        
        switch( np->flags & (kOnsetXsFl|kMetronomeXsFl|kDampDnXsFl|kDampUpDnXsFl|kSostDnXsFl) )
        {
          case kOnsetXsFl:
            if( cmMidiFileIsValid(mfH) )
            {
              if( np->tied_dur <= 0 )
                cmErrWarnMsg(&p->err,kOkXsRC,"A zero length note was encountered bar:%i tick:%i (%i) %s",np->meas->number,np->tick,thisTick,cmMidiToSciPitch(np->pitch,NULL,0));

              /*
              if( mp->number == 20 )
              {
                _cmXScoreReportNote(ctx->err.rpt,np,ni);
                cmRptPrintf(ctx->err.rpt,"\n");
              }
              */

              if( np->vel == 0 )
                cmErrWarnMsg(&p->err,kOkXsRC,"A sounding note with zero velocity was encountered at bar:%i tick:%i pitch:%s.",np->meas->number,np->tick,cmMidiToSciPitch(np->pitch,NULL,0));
                            
              if( cmMidiFileInsertTrackChMsg(mfH, 1, thisTick,                kNoteOnMdId,  np->pitch, np->vel ) != kOkMfRC
                ||cmMidiFileInsertTrackChMsg(mfH, 1, thisTick + np->tied_dur, kNoteOffMdId, np->pitch, 0 )       != kOkMfRC )
              {
                rc = kMidiFailXsRC;
              }
            }
            break;
            
          case kDampDnXsFl:
          case kDampUpDnXsFl:
          case kSostDnXsFl:
            if( cmMidiFileIsValid(mfH) )
            {
              if( np->duration <= 0 )
                cmErrWarnMsg(&p->err,kOkXsRC,"A zero length pedal event was encountered bar:%i tick:%i (%i)",np->meas->number,np->tick,thisTick);
              
              cmMidiByte_t d0     = cmIsFlag(np->flags,kSostDnXsFl) ? kSostenutoCtlMdId : kSustainCtlMdId;              
              if( (cmMidiFileInsertTrackChMsg(mfH, 1, thisTick,                kCtlMdId, d0, 127 ) != kOkMfRC )
                ||(cmMidiFileInsertTrackChMsg(mfH, 1, thisTick + np->duration, kCtlMdId, d0,   0 ) != kOkMfRC ) )
              {
                rc = kMidiFailXsRC;
              }
            }
            break;

          case kMetronomeXsFl:
            bpm = np->duration;
            if( cmMidiFileIsValid(mfH) )
              if( cmMidFileInsertTrackTempoMsg(mfH, 0, thisTick, bpm ) != kOkMfRC )
                rc = kMidiFailXsRC;            
            break;
            
          case 0:
            break;
            
          default:
            { assert(0); }
        }

        if( rc != kOkXsRC )
        {
          rc = cmErrMsg(&p->err,rc,"MIDI message insert failed on '%s'.",cmStringNullGuard(outFn));
          goto errLabel;
        }
      }
    }
  }

  if( cmMidiFileIsValid(mfH) )
    if( cmMidiFileWrite(mfH,outFn) != kOkMfRC )
    {
      rc = cmErrMsg(&p->err,kMidiFailXsRC,"MIDI file write failed on '%s'.",cmStringNullGuard(outFn));
      goto errLabel;
    }
  
 errLabel:
  cmFsFreeFn(outFn);
  if( cmMidiFileClose(&mfH) != kOkMfRC )
  {
    rc = cmErrMsg(&p->err,kMidiFailXsRC,"Unable to create the MIDI file object.");
    goto errLabel;
  }
  
  return rc;
}

bool _cmXsIsMidiFileValid( cmCtx_t* ctx, cmXsH_t h, const cmChar_t* dir, const cmChar_t* fn )
{
  const cmChar_t* midiFn = cmFsMakeFn(dir,fn,"mid",NULL);
  cmMidiFileH_t mfH = cmMidiFileNullHandle;
  
  if( cmMidiFileOpen( ctx, &mfH, midiFn ) == kOkMfRC )
  {
    cmMidiFileClose(&mfH);
    return true;
  }

  cmXScore_t* p = _cmXScoreHandleToPtr(h);
  cmErrMsg(&p->err,kMidiFailXsRC,"The generated MIDI file '%s' is not valid.", cmStringNullGuard(midiFn));
  
  return false;
}

typedef struct cmXsSvgEvt_str
{
  unsigned               flags;    // k???XsFl
  unsigned               tick;     // start tick
  unsigned               durTicks; // dur-ticks
  unsigned               voice;    // score voice number
  unsigned               d0;       // MIDI d0   (barNumb)
  unsigned               d1;       // MIDI d1
  cmXsComplexity_t       cplx;
  struct cmXsSvgEvt_str* link;
} cmXsSvgEvt_t;

typedef struct cmXsMidiFile_str
{
  cmXsSvgEvt_t* elist;
  cmXsSvgEvt_t* eol;

  unsigned pitch_min;
  unsigned pitch_max;
  
} cmXsMidiFile_t;


void _cmXsWriteMidiSvgLegend( cmSvgH_t svgH, unsigned index,  const cmChar_t* label, const cmChar_t* classStr )
{
  double x = 100;
  double y = 120*10 - 20*index;
  
  cmSvgWriterText( svgH, x, y, label, "legend" );

  x += 75;
  cmSvgWriterLine( svgH, x, y, x+125, y, classStr );
}

void _cmXsWriteMidiSvgLinePoint( cmSvgH_t svgH, double x0, double y0, double x1, double y1, double y_max, const cmChar_t* classStr, const cmChar_t* label )
{
  int  bn = 255;
  char b[bn+1];
  double y_scale = 10;
  double y_label = y1;
  
  b[0] = 0;
  y0 = (y0/y_max) * 127.0 * y_scale;
  y1 = (y1/y_max) * 127.0 * y_scale;
  
  cmSvgWriterLine(svgH, x0, y0,  x1, y1,  classStr );

  if( y0 != y1 )
    snprintf(b,bn,"%5.0f %s",y_label,label==NULL?"":label);
  else
  {
    if( label != NULL )
      snprintf(b,bn,"%s",label);
  }
  
  if( strlen(b) )
    cmSvgWriterText(svgH, x1, y1, b, "pt_text");

}

cmXsRC_t _cmXsWriteMidiSvg( cmCtx_t* ctx, cmXScore_t* p, cmXsMidiFile_t* mf, const cmChar_t* svgFn, bool standAloneFl, bool panZoomFl )
{
  cmXsRC_t        rc         = kOkXsRC;
  cmSvgH_t        svgH       = cmSvgNullHandle;
  cmXsSvgEvt_t*   e          = mf->elist;
  unsigned        noteHeight = 10;
  //cmChar_t*       fn0        = cmMemAllocStr( fn );  
  //const cmChar_t* svgFn      = cmFsMakeFn(dir,fn0 = cmTextAppendSS(fn0,"_midi_svg"),"html",NULL);
  const cmChar_t* cssFn      = cmFsMakeFn(NULL,"score_midi_svg","css",NULL);
  cmChar_t*       t0         = NULL;  // temporary dynamic string
  unsigned        i          = 0;
  const cmXsSvgEvt_t* e0 = NULL;
  //cmMemFree(fn0);
  
  // create the SVG writer
  if( cmSvgWriterAlloc(ctx,&svgH) != kOkSvgRC )
  {
    rc = cmErrMsg(&p->err,kSvgFailXsRC,"Unable to create the MIDI SVG output file '%s'.",cmStringNullGuard(svgFn));
    goto errLabel;
  }

  _cmXsWriteMidiSvgLegend( svgH, 0,  "Velocity",    "cplx_vel" );
  _cmXsWriteMidiSvgLegend( svgH, 1,  "Duration",    "cplx_rym" );
  _cmXsWriteMidiSvgLegend( svgH, 2,  "Left Pitch",  "cplx_lpch" );
  _cmXsWriteMidiSvgLegend( svgH, 3,  "Right Pitch", "cplx_rpch" );
  _cmXsWriteMidiSvgLegend( svgH, 4,  "Density",     "cplx_density" );

  
  // for each MIDI file element
  for(; e!=NULL && rc==kOkXsRC; e=e->link)
  {
    switch( e->flags & (kOnsetXsFl|kBarXsFl|kDampDnXsFl|kDampUpDnXsFl|kSostDnXsFl))
    {
      
      // if this is a note
      case kOnsetXsFl:
        {
          //const cmChar_t* classLabel = "note";
      
          t0 = cmTsPrintfP(t0,"note_%i%s",e->voice, cmIsFlag(e->flags,kGraceXsFl) ? "_g":"");
      
          //if( cmIsFlag(e->flags,kGraceXsFl) )
          //  classLabel = "grace";
      
          if( cmSvgWriterRect(svgH, e->tick, e->d0 * noteHeight,  e->durTicks,  noteHeight-1, t0 ) != kOkSvgRC )
            rc = kSvgFailXsRC;
          else
          {
            t0 = cmTsPrintfP(t0,"%s",cmMidiToSciPitch( e->d0, NULL, 0));
            
            if( cmSvgWriterText(svgH, e->tick + e->durTicks/2, e->d0 * noteHeight + noteHeight/2, t0, "pitch") != kOkSvgRC )
              rc = kSvgFailXsRC;
            else
            {
              if( e0 != NULL )
              {
                bool fl = (i % 10) == 0;
                
                _cmXsWriteMidiSvgLinePoint(svgH, e0->tick, e0->cplx.sum_d_vel,  e->tick, e->cplx.sum_d_vel,  p->cplx_max.sum_d_vel, "cplx_vel",fl?"V":NULL);
                _cmXsWriteMidiSvgLinePoint(svgH, e0->tick, e0->cplx.sum_d_rym,  e->tick, e->cplx.sum_d_rym,  p->cplx_max.sum_d_rym, "cplx_rym",fl?"D":NULL);
                _cmXsWriteMidiSvgLinePoint(svgH, e0->tick, e0->cplx.sum_d_lpch, e->tick, e->cplx.sum_d_lpch, p->cplx_max.sum_d_lpch, "cplx_lpch",fl?"L":NULL);
                _cmXsWriteMidiSvgLinePoint(svgH, e0->tick, e0->cplx.sum_d_rpch, e->tick, e->cplx.sum_d_rpch, p->cplx_max.sum_d_rpch, "cplx_rpch",fl?"R":NULL);
                _cmXsWriteMidiSvgLinePoint(svgH, e0->tick, e0->cplx.sum_n_lpch + e0->cplx.sum_n_rpch, e->tick, e->cplx.sum_n_lpch  + e->cplx.sum_n_rpch, p->cplx_max.sum_n_lpch + p->cplx_max.sum_n_rpch, "cplx_density",fl?"N":NULL);
              }

              e0 = e;
            }
            
          }

          i+=1;
        }
        break;

        // if this is a bar
      case kBarXsFl:
        {
          if( cmSvgWriterLine(svgH, e->tick, 0, e->tick, 127*noteHeight, "bar") != kOkSvgRC )
            rc = kSvgFailXsRC;
          else
          {
            if( cmSvgWriterText(svgH, e->tick, 10, t0 = cmTsPrintfP(t0,"%i",e->d0), "text" ) != kOkSvgRC )
              rc = kSvgFailXsRC;
          }
        }
        break;

        // if this is a pedal event
      case kDampDnXsFl:
      case kDampUpDnXsFl:
      case kSostDnXsFl:
        {
          const cmChar_t* classLabel =        cmIsFlag(e->flags,kSostDnXsFl) ? "sost" : "damp";
          unsigned        y          = (128 + cmIsFlag(e->flags,kSostDnXsFl)?1:0) * noteHeight;
          cmSvgWriterRect(svgH, e->tick, y, e->durTicks, noteHeight-1, classLabel);
        }
        break;
    }    
  }
  
  if( rc != kOkXsRC )
    cmErrMsg(&p->err,kSvgFailXsRC,"SVG element insert failed.");

  if( rc == kOkXsRC )
    if( cmSvgWriterWrite(svgH,cssFn,svgFn,standAloneFl, panZoomFl) != kOkSvgRC )
      rc = cmErrMsg(&p->err,kSvgFailXsRC,"SVG file write to '%s' failed.",cmStringNullGuard(svgFn));
  
 errLabel:
  cmSvgWriterFree(&svgH);
  //cmFsFreeFn(svgFn);
  cmFsFreeFn(cssFn);
  cmMemFree(t0);
  
  return rc;
}


void _cmXsPushSvgEvent( cmXScore_t* p, cmXsMidiFile_t* mf, unsigned flags, unsigned tick, unsigned durTick, unsigned voice, unsigned d0, unsigned d1, const cmXsComplexity_t* cplx )
{
  cmXsSvgEvt_t* e = cmLhAllocZ(p->lhH,cmXsSvgEvt_t,1);
  e->flags    = flags;
  e->tick     = tick;
  e->durTicks = durTick;
  e->voice    = voice;
  e->d0       = d0;       // note=pitch bar=number pedal=ctl# metronome=BPM 
  e->d1       = d1;
  
  if( cplx != NULL )
    e->cplx     = *cplx;
  
  if( mf->eol != NULL )
    mf->eol->link = e;
  else
    mf->elist = e;

  // track the min/max pitch
  if( cmIsFlag(flags,kOnsetXsFl) )
  {
    mf->pitch_min = mf->eol==NULL ? d0 : cmMin(mf->pitch_min,d0);
    mf->pitch_max = mf->eol==NULL ? d0 : cmMin(mf->pitch_max,d0);
  }
  
  mf->eol = e;
}

cmXsRC_t _cmXScoreGenSvg( cmCtx_t* ctx, cmXsH_t h, int beginMeasNumb, const cmChar_t* svgFn, bool standAloneFl, bool panZoomFl )
{
  cmXScore_t* p  = _cmXScoreHandleToPtr(h);
  cmXsPart_t* pp = p->partL;
  
  cmXsMidiFile_t mf;
  memset(&mf,0,sizeof(mf));

  for(; pp!=NULL; pp=pp->link)
  {
    const cmXsMeas_t* meas = pp->measL;
    for(; meas!=NULL; meas=meas->link)
    {
      if( meas->number < beginMeasNumb )
        continue;

      const cmXsNote_t* note = meas->noteL;
      for(; note!=NULL; note=note->slink)
      {
        // if this is a metronome marker
        if( cmIsFlag(note->flags,kMetronomeXsFl) )
        {
          // set BPM as d0
          _cmXsPushSvgEvent(p,&mf,note->flags,note->tick,0,0,note->duration,0,NULL);
          continue;
          
        }
        
        // if this is a sounding note
        if( cmIsFlag(note->flags,kOnsetXsFl) )
        {
          unsigned d0      =  cmSciPitchToMidiPitch( note->step, note->alter, note->octave );
          unsigned durTick = note->tied_dur;
          if( note->tied != NULL )
          {
            cmXsNote_t* tn = note->tied;
            for(; tn!=NULL; tn=tn->tied)
              durTick += tn->tied_dur;
          }
          _cmXsPushSvgEvent(p,&mf,note->flags,note->tick,durTick,note->voice->id,d0,note->vel,&note->cplx);
          continue;
        }

        // if this is a bar event
        if( cmIsFlag(note->flags,kBarXsFl) )
        {
          _cmXsPushSvgEvent(p,&mf,note->flags,note->tick,0,0,note->meas->number,0,NULL);
          continue;
        }

        // if this is a pedal event
        if( cmIsFlag(note->flags,kDampDnXsFl|kDampUpDnXsFl|kSostDnXsFl) )
        {
          unsigned d0 = cmIsFlag(note->flags,kSostDnXsFl) ? kSostenutoCtlMdId : kSustainCtlMdId;          
          _cmXsPushSvgEvent(p,&mf,note->flags,note->tick,note->duration,0,d0,127,NULL);
          continue;
        }
      }
    }
  }
  
  return _cmXsWriteMidiSvg( ctx, p, &mf, svgFn, standAloneFl, panZoomFl );
}


cmXsRC_t cmXScoreTest(
  cmCtx_t*        ctx,
  const cmChar_t* xmlFn,
  const cmChar_t* editFn,
  const cmChar_t* csvOutFn,
  const cmChar_t* midiOutFn,
  const cmChar_t* svgOutFn,
  bool            reportFl,
  int             beginMeasNumb,
  int             beginBPM,
  bool            standAloneFl,
  bool            panZoomFl )
{
  cmXsRC_t rc;
  cmXsH_t h = cmXsNullHandle;

  if( editFn!=NULL && !cmFsIsFile(editFn) )
  {
    cmRptPrintf(&ctx->rpt,"The edit file %s does not exist. A new edit file will be created.",editFn);
    cmXScoreGenEditFile(ctx,xmlFn,editFn);
    editFn = NULL;
  }
  

  // Parse the XML file and apply the changes in editFn.
  if((rc = cmXScoreInitialize( ctx, &h, xmlFn,editFn)) != kOkXsRC )
    return cmErrMsg(&ctx->err,rc,"XScore alloc failed.");

  if( csvOutFn != NULL )
  {
    
    cmXScoreWriteCsv(h,beginMeasNumb,csvOutFn);

    _cmXsIsCsvValid(ctx,h,csvOutFn);
  }
  
  if( midiOutFn != NULL )
  {
    // measure the score complexity
    double wndSecs = 1.0;
    
    _cmXsMeasComplexity(h,wndSecs);
    
    cmFileSysPathPart_t* pp = cmFsPathParts(midiOutFn);

    _cmXsWriteMidiFile(ctx, h, beginMeasNumb, beginBPM, pp->dirStr, pp->fnStr );
    
    _cmXsIsMidiFileValid(ctx, h, pp->dirStr, pp->fnStr );
    
    cmFsFreePathParts(pp);
    
  }

  if( svgOutFn != NULL )
    _cmXScoreGenSvg( ctx, h, beginMeasNumb, svgOutFn, standAloneFl, panZoomFl );

  if(reportFl)
    cmXScoreReport(h,&ctx->rpt,true);

  return cmXScoreFinalize(&h);

}
