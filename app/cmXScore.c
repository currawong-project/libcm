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
#include "cmXScore.h"
#include "cmTime.h"
#include "cmMidi.h"
#include "cmMidiFile.h"
#include "cmLex.h"
#include "cmCsv.h"

#include "cmFile.h"
#include "cmSymTbl.h"
#include "cmAudioFile.h"
#include "cmAudioFile.h"
#include "cmProcObj.h"
#include "cmProcTemplate.h"
#include "cmProc.h"
#include "cmProc2.h"
#include "cmProc5.h"

cmXsH_t cmXsNullHandle = cmSTATIC_NULL_HANDLE;

enum
{
  kSectionXsFl   = 0x00001,  // rvalue holds section number
  kBarXsFl       = 0x00002,
  kRestXsFl      = 0x00004,
  kGraceXsFl     = 0x00008,
  kDotXsFl       = 0x00010,
  kChordXsFl     = 0x00020,
  kDynXsFl       = 0x00040,
  kEvenXsFl      = 0x00080,
  kTempoXsFl     = 0x00100,
  kHeelXsFl      = 0x00200,
  kTieBegXsFl    = 0x00400,
  kTieEndXsFl    = 0x00800,
  kTieProcXsFl   = 0x01000,
  kPedalDnXsFl   = 0x02000,
  kPedalUpXsFl   = 0x04000,
  kPedalUpDnXsFl = 0x08000,
  kMetronomeXsFl = 0x10000,  // duration holds BPM
  kOnsetXsFl     = 0x20000   // this is a sounding note
};

struct cmXsMeas_str;
struct cmXsVoice_str;

typedef struct cmXsNote_str
{
  unsigned                    uid;      // unique id of this note record
  unsigned                    flags;    // See k???XsFl 
  unsigned                    pitch;    // midi pitch
  unsigned                    velocity; // midi velocity
  cmChar_t                    step;     // A-G
  unsigned                    octave;   // sci pitch octave
  int                         alter;    // +n=sharps,-n=flats
  unsigned                    staff;    // 1=treble 2=bass
  unsigned                    tick;     // 
  unsigned                    duration; // duration in ticks
  unsigned                    locIdx;    // location index (chords share the same location index)
  double                      rvalue;   // 1/rvalue = rythmic value (1/0.5 double whole 1/1 whole 1/2 half 1/4=quarter note, 1/8=eighth note, ...)
  const cmChar_t*             tvalue;   // text value

  struct cmXsVoice_str*       voice;    // voice this note belongs to 
  struct cmXsMeas_str*        meas;     // measure this note belongs to

  const cmXmlNode_t*          xmlNode;  // note xml ptr
  
  struct cmXsNote_str*        mlink;    // measure note list 
  struct cmXsNote_str*        slink;    // time sorted event list
  
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
} cmXScore_t;

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

  note->voice = v;
  note->uid   = p->nextUid++;

  return kOkXsRC;
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

  cmChar_t buf[3] = { *step, '0', '\0'};
  unsigned midi = cmSciPitchToMidi(buf);

  midi         += (12 * octave);
  midi         += alter;

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
    unsigned        value;
    const cmChar_t* label;
  } map_t;

  map_t mapV[] =
  {
    { kEvenXsFl,                         "#0000FF" },  // blue (even)
    { kTempoXsFl,                        "#00FF00" },  // green (tempo)
    { kDynXsFl,                          "#FF0000" },  // red   (dynamics)
    { kTempoXsFl | kEvenXsFl,            "#00FFFF" },  // green + blue (turquoise)
    { kDynXsFl   | kEvenXsFl,            "#FF00FF" },  // red   + blue  
    { kDynXsFl   | kEvenXsFl,            "#FF0CF7" },  // magenta (even+dyn)
    { kDynXsFl   | kTempoXsFl,           "#FF7F00" },  // red   + green (brown)
    { kTempoXsFl | kEvenXsFl | kDynXsFl, "#996633" },  // (purple)
    { kDynXsFl,                          "#FF6A03" },  //   176 orange  (dynamics)
    { kEvenXsFl,                         "#2F00E8" },  //  1001 blue (even)    
    { kTempoXsFl,                        "#01CD1F" },  //  1196 green   (tempo)
    { kEvenXsFl,                         "#3600E8" },  //  1627 blue (even)
    { kDynXsFl | kTempoXsFl,             "#9E8F15" },  //  8827 brown (dyn + tempo)
    { kEvenXsFl,                         "#2E00E6" },  //  5393 blue (even)
    { kEvenXsFl,                         "#2C00DD" },  //  5895 blue (even)
    { kDynXsFl,                          "#FF5B03" },  //  6498 orange (dyn)
    { kDynXsFl,                          "#FF6104" },  //  6896 orange
    { kEvenXsFl,                         "#2A00E6" },  //  7781 blue
    { kEvenXsFl,                         "#2300DD" },  //  8300 blue (even)    
    { kTempoXsFl,                        "#03CD22" },  // 10820 green (tempo)
    { kEvenXsFl,                         "#3400DB" },  // 11627 blue (dyn)
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
    s = cmLhAllocZ(p->lhH,cmXsSpan_t,1);
    s->staff  = staff;
    s->meas   = meas;
    s->number = span_number;
    s->tick0  = tick;
    s->tick1  = -1;
    s->pitch_offset = cmTextCmp(type_str,"up")==0 ? -12 : 12;
    s->link   = p->spanL;
    p->spanL  = s;    
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
      flags = kPedalDnXsFl;
    else
      if( cmTextCmp(a->value,"change") == 0 )
        flags = kPedalUpDnXsFl;
      else
        if( cmTextCmp(a->value,"stop") == 0 )
          flags = kPedalUpXsFl;
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


cmXsRC_t _cmXScoreParseMeasure(cmXScore_t* p, cmXsPart_t* pp, const cmXmlNode_t* mnp)
{
  cmXsRC_t           rc   = kOkXsRC;
  const cmXmlNode_t* np   = NULL;  
  unsigned           tick = 0;
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
  
  return rc;
}

cmXsRC_t _cmXScoreParsePart( cmXScore_t* p, cmXsPart_t* pp )
{
  cmXsRC_t           rc       = kOkXsRC;
  const cmXmlNode_t* xnp;
  cmXmlAttr_t        partAttr = { "id", pp->idStr };
  
  // find the 'part'
  if((xnp = cmXmlSearch( cmXmlRoot(p->xmlH), "part", &partAttr, 1)) == NULL )
    return cmErrMsg(&p->err,kSyntaxErrorXsRC,"The part '%s' was not found.",pp->idStr);

  // for each child of this part - find each measure
  const cmXmlNode_t* cnp = xnp->children;
  for(; cnp!=NULL; cnp=cnp->sibling)
    if( cmTextCmp(cnp->label,"measure") == 0 )
      if((rc = _cmXScoreParseMeasure(p,pp,cnp)) != kOkXsRC )
        return rc;
  
  return rc;
}

// Insert note 'np' into the sorted note list based at 's0'.
// Return a pointer to the base of the list after the insertion.
cmXsNote_t*  _cmXScoreInsertSortedNote( cmXsNote_t* s0, cmXsNote_t* np )
{
  if( s0 == NULL )
    return np;

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
}

bool  _cmXScoreFindTiedNote( cmXScore_t* p, cmXsMeas_t* mp, cmXsNote_t* n0p, bool rptFl )
{
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
        nnp->flags |= kTieProcXsFl;
        nnp->flags  = cmClrFlag(nnp->flags,kOnsetXsFl);

        if( rptFl )
          printf("---> %i %i %s ",nnp->meas->number,nnp->tick,cmMidiToSciPitch(nnp->pitch,NULL,0));

        // if this note is not tied to a subsequent note
        if( cmIsNotFlag(nnp->flags,kTieBegXsFl) )
          return true;

        // record the measure number of the last note with a tie-start
        measNumb = mp->number;
      }
    }
    
  }
  
  cmErrWarnMsg(&p->err,kUnterminatedTieXsRC,"The tied %c%c%i in measure %i was not terminated.",n0p->step,acc,n0p->octave,measNumb);
  return false;
}

bool  _cmXScoreFindTiedNote1( cmXScore_t* p, cmXsMeas_t* mp, cmXsNote_t* np, bool rptFl )
{
  cmXsNote_t* nnp       = np->slink;  // begin w/ note following np
  unsigned    measNumb  = mp->number;
  unsigned    measNumb0 = measNumb;
  cmChar_t    acc       = np->alter==-1?'b' : (np->alter==1?'#':' ');

  if( rptFl )
    printf("%i %i %s ",np->meas->number,np->tick,cmMidiToSciPitch(np->pitch,NULL,0));
  
  // for each successive measure
  for(; mp!=NULL; mp=mp->link)
  {
    //if( nnp == NULL )
    //  nnp = mp->noteL;

    // for each note starting at nnp
    for(; nnp!=NULL; nnp=nnp->slink)
    {
      // if this note is tied to the originating note (np)
      if( nnp->voice->id == np->voice->id && nnp->step == np->step && nnp->octave == np->octave )
      {
        nnp->flags |= kTieProcXsFl;
        nnp->flags  = cmClrFlag(nnp->flags,kOnsetXsFl);

        if( rptFl )
          printf("---> %i %i %s ",nnp->meas->number,nnp->tick,cmMidiToSciPitch(nnp->pitch,NULL,0));

        // if this note is not tied to a subsequent note
        if( cmIsNotFlag(nnp->flags,kTieBegXsFl) )
        {
          return true;
        }

        measNumb0 = mp->number;  
      }
    }

    // if a measure was completed and no end note was found ... then the tie is unterminated
    // (a tie must be continued in every measure which it passes through)
    if( measNumb0 < mp->number )
      break;
    
  }

  cmErrWarnMsg(&p->err,kUnterminatedTieXsRC,"The tied %c%c%i in measure %i was not terminated.",np->step,acc,np->octave,measNumb0);
  return false;
}

void  _cmXScoreResolveTiesAndLoc( cmXScore_t* p )
{
  unsigned n   = 0;
  unsigned m   = 0;
  bool     rptFl = false;
  cmXsPart_t* pp = p->partL;
  
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

        if( cmIsFlag(np->flags,kTieEndXsFl) && cmIsFlag(np->flags,kOnsetXsFl) )
        {
          cmChar_t    acc  = np->alter==-1?'b' : (np->alter==1?'#':' ');
          cmErrWarnMsg(&p->err,kUnterminatedTieXsRC,"The tied %c%c%i in measure %i marked as a tied note but is also marked to sound.",np->step,acc,np->octave,mp->number);
        }

        // set the location 
        if( cmIsFlag(np->flags,kOnsetXsFl) )
        {
          if( n0!=NULL && n0->tick!=np->tick)
            locIdx += 1;

          np->locIdx = locIdx;
          n0         = np;
        }

      } 
    }
  }

  printf("Found:%i Not Found:%i\n",m,n-m);
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


cmXsMeas_t* _cmXScoreNextNonEmptyMeas( cmXsPart_t* pp, cmXsMeas_t* meas )
{
  if( meas == NULL )
  {
    if( pp==NULL || pp->measL==NULL )
      return NULL;
    
    meas = pp->measL;
  }
  else
  {
    meas = meas->link;
  }

  while( meas != NULL && meas->noteL == NULL )
    meas=meas->link;

  return meas;
}

cmXsNote_t* _cmXScoreNextNote( cmXsPart_t* pp, cmXsNote_t* note )
{
  // meas should always be valid (unless this is the first note in the score)
  cmXsMeas_t* meas = note==NULL ? NULL : note->meas;
  
  do
  { 
    if( note == NULL || note->slink==NULL )
    {
      if((meas = _cmXScoreNextNonEmptyMeas(pp,meas)) == NULL)
        return NULL;
      
      assert( meas->noteL != NULL );
      
      note = meas->noteL;
    }
    else
    {
      note = note->slink;        
    }

    assert( note != NULL );
    
    meas = note->meas;

    // note is now set to a non-NULL candidate note - advance to a sounding note
    while( note!=NULL && cmIsNotFlag(note->flags,kOnsetXsFl) )
      note = note->slink;

    // if no note was found in this measure
  }while( note == NULL );
     
  return note;
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
          double bps =  np->duration / 60.0;

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
            cmFilePrintf(fH,"n %f %f %i %s %s\n",onset_secs,np->duration/ticks_per_sec,np->uid,cmMidiToSciPitch(np->pitch,NULL,0),cmIsFlag(np->flags,kGraceXsFl)?"G":"N");
          }
        }
      }

      onset_secs += (mp->divisions * mp->beats - tick0) / ticks_per_sec;
    }
  }

  cmFileClose(&fH);

  return rc;
}

cmXsRC_t _cmXScoreWriteMidiPlotFile( cmXScore_t* p, cmChar_t* fn, const cmMidiTrackMsg_t** m, unsigned mN )
{
  cmXsRC_t  rc = kOkXsRC;
  cmFileH_t fH = cmFileNullHandle;
  unsigned  i  = 0;
  
  if( cmFileOpen(&fH,fn,kWriteFileFl,p->err.rpt) != kOkFileRC )
    return cmErrMsg(&p->err,kFileFailXsRC,"Unable to create the file '%s'.",cmStringNullGuard(fn));

  for(i=0; i<mN; ++i)
    if( (m[i]!=NULL) && cmMidiIsChStatus(m[i]->status) && cmMidiIsNoteOn(m[i]->status) && (m[i]->u.chMsgPtr->d1>0) )
      cmFilePrintf(fH,"n %f %f %i %s\n",m[i]->amicro/1000000.0,m[i]->u.chMsgPtr->durMicros/1000000.0,m[i]->uid,cmMidiToSciPitch(m[i]->u.chMsgPtr->d0,NULL,0));

  
  cmFileClose(&fH);
  return rc;
}


cmXsRC_t    _cmXScoreProcessMidi(cmXScore_t* p, cmCtx_t* ctx, const cmChar_t* midiFn)
{
  cmXsRC_t                 rc   = kOkXsRC;
  cmMidiFileH_t            mfH  = cmMidiFileNullHandle;
  const cmMidiTrackMsg_t** m    = NULL;
  unsigned                 mN   = 0;
  unsigned                 i    = 0;
  unsigned                 j    = 0;
  cmXsNote_t*              note = NULL;
  
  if( cmMidiFileOpen(ctx, &mfH, midiFn ) != kOkMfRC )
    return cmErrMsg(&p->err,kMidiFailXsRC,"The MIDI file object could not be opened from '%s'.",cmStringNullGuard(midiFn));

  //cmMidiFilePrintMsgs(mfH, p->err.rpt );
  
  if( (m = cmMidiFileMsgArray(mfH)) == NULL || (mN = cmMidiFileMsgCount(mfH)) == 0 )
  {
    rc = cmErrMsg(&p->err,kMidiFailXsRC,"The MIDI file object appears to be empty.");
    goto errLabel;
  }

  if((note = _cmXScoreNextNote(p->partL,NULL)) == NULL)
  {
    rc = cmErrWarnMsg(&p->err,kSyntaxErrorXsRC,"No MIDI processing to be done. The score appears to be empty.");
    goto errLabel;
  }

  cmCtx*        c = cmCtxAlloc( NULL, p->err.rpt, cmLHeapNullHandle, cmSymTblNullHandle );
  cmSeqAlign_t* s = cmSeqAlignAlloc(c,NULL);
  unsigned      offs = 0;
  
  for(; note!=NULL; note=_cmXScoreNextNote(p->partL,note))
  {
    if( cmIsFlag(note->flags,kGraceXsFl) )
      offs += 1;
    
    cmSeqAlignInsert(s,0,note->locIdx+offs,note->pitch);
  }

  unsigned locIdx = 1;
  for(i=0,j=0; i<mN; ++i)
    if( (m[i]!=NULL) && cmMidiIsChStatus(m[i]->status) && cmMidiIsNoteOn(m[i]->status) && (m[i]->u.chMsgPtr->d1>0) )
    {
      if( m[j]->atick != m[i]->atick )
        locIdx += 1;
      
      cmSeqAlignInsert(s,1,locIdx,m[i]->u.chMsgPtr->d0);

      //printf("%i : %s\n",locIdx,cmMidiToSciPitch(m[i]->u.chMsgPtr->d0,NULL,0));
      
      j = i;
    }

  cmMidiFileCalcNoteDurations( mfH );
  
  _cmXScoreWriteScorePlotFile(p, "/Users/kevin/temp/score.txt" );
  _cmXScoreWriteMidiPlotFile(p,  "/Users/kevin/temp/midi.txt", m, mN );


  cmSeqAlignReport(s,p->err.rpt);
  cmSeqAlignFree(&s);
  cmCtxFree(&c);
  goto errLabel;

  printf(" i     j    score    midi\n");
  printf("---- ---- --- ---- --- ----\n");
  
  for(j=0; note!=NULL; note=_cmXScoreNextNote(p->partL,note),++j)
  {
    unsigned midiPitch = 0;
    for(; i<mN; ++i)
      if( m[i]!=NULL && cmMidiIsChStatus(m[i]->status) && cmMidiIsNoteOn(m[i]->status) && m[i]->u.chMsgPtr->d1>0 )
      {
        midiPitch = m[i]->u.chMsgPtr->d0;
        ++i;
        break;
      }

    char buf[6];
    printf("%4i %4i %3i %4s %3i %4s\n",j,i,
      note->pitch,
      cmMidiToSciPitch(note->pitch,NULL,0),
      midiPitch,
      cmMidiToSciPitch(midiPitch,buf,5));
    
  }

 errLabel:
  cmMidiFileClose(&mfH);
  return rc;
}

cmXsRC_t cmXScoreInitialize( cmCtx_t* ctx, cmXsH_t* hp, const cmChar_t* xmlFn, const cmChar_t* midiFn )
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

  _cmXScoreResolveTiesAndLoc(p);

  //_cmXScoreResolveOctaveShift(p);

  //if( midiFn != NULL )
  //  _cmXScoreProcessMidi(p,ctx,midiFn);

  // CSV output initialize failed.
  if( cmCsvInitialize(&p->csvH,ctx) != kOkCsvRC )
    rc = cmErrMsg(&p->err,kCsvFailXsRC,"CSV output object create failed.");
  
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

void _cmXScoreReportTitle( cmRpt_t* rpt )
{
  cmRptPrintf(rpt,"      voc  loc  tick  durtn rval        flags\n");
  cmRptPrintf(rpt,"      --- ----- ----- ----- ---- --- -------------\n");
}
    
void _cmXScoreReportNote( cmRpt_t* rpt, const cmXsNote_t* note )
{
  const cmChar_t* B  = cmIsFlag(note->flags,kBarXsFl)       ? "|" : "-";
  const cmChar_t* R  = cmIsFlag(note->flags,kRestXsFl)      ? "R" : "-";
  const cmChar_t* G  = cmIsFlag(note->flags,kGraceXsFl)     ? "G" : "-";
  const cmChar_t* D  = cmIsFlag(note->flags,kDotXsFl)       ? "D" : "-";
  const cmChar_t* C  = cmIsFlag(note->flags,kChordXsFl)     ? "C" : "-";
  const cmChar_t* e  = cmIsFlag(note->flags,kEvenXsFl)      ? "e" : "-";
  const cmChar_t* d  = cmIsFlag(note->flags,kDynXsFl)       ? "d" : "-";
  const cmChar_t* t  = cmIsFlag(note->flags,kTempoXsFl)     ? "t" : "-";
  const cmChar_t* P  = cmIsFlag(note->flags,kPedalDnXsFl)   ? "V" : "-";
  const cmChar_t* S  = cmIsFlag(note->flags,kSectionXsFl)   ? "S" : "-";
  const cmChar_t* H  = cmIsFlag(note->flags,kHeelXsFl)      ? "H" : "-";
  const cmChar_t* T0 = cmIsFlag(note->flags,kTieBegXsFl)    ? "T" : "-";
  const cmChar_t* T1 = cmIsFlag(note->flags,kTieEndXsFl)    ? "_" : "-";
  P = cmIsFlag(note->flags,kPedalUpXsFl)   ? "^" : P;
  P = cmIsFlag(note->flags,kPedalUpDnXsFl) ? "X" : P;
  //const cmChar_t* N = note->pitch==0 ? " " : cmMidiToSciPitch( note->pitch, NULL, 0 );

  cmChar_t N[] = {'\0','\0','\0','\0'};
  cmChar_t acc = note->alter==-1?'b':(note->alter==1?'#':' ');
  snprintf(N,4,"%c%c%1i",note->step,acc,note->octave);
    
  cmRptPrintf(rpt,"      %3i %5i %5i %5i %4.1f %3s %s%s%s%s%s%s%s%s%s%s%s%s%s",
    note->voice->id,
    note->locIdx,
    note->tick,
    note->duration,
    note->rvalue,
    N,B,R,G,D,C,e,d,t,P,S,H,T0,T1);

  if( cmIsFlag(note->flags,kSectionXsFl) )
    cmRptPrintf(rpt," %s",cmStringNullGuard(note->tvalue));

  if( cmIsFlag(note->flags,kMetronomeXsFl) )
    cmRptPrintf(rpt," %i bpm",note->duration);

}

/*
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
    "even","grace","tempo","t frac","dyn","section","play_recd","remark",NULL
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
  unsigned        flags )
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
  if( cmCsvInsertIdentColAfter(p->csvH,lcp,&lcp, cmIsFlag(flags,kGraceXsFl) | cmIsFlag(flags,kEvenXsFl) ? "e" : "",0) != kOkCsvRC )
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
  if( cmCsvInsertIdentColAfter(p->csvH,lcp,&lcp,cmIsFlag(flags,kTempoXsFl) ? "t" : "",0) != kOkCsvRC )
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
  
  // col 19: dynamic marking (blank for now)
  if((rc = _cmXScoreWriteCsvBlankCols(p,1,&lcp)) != kOkXsRC )
    goto errLabel;
  
  // col 20: section
  if( cmCsvInsertIdentColAfter(p->csvH,lcp,&lcp,cmIsFlag(flags,kSectionXsFl) ? sectionStr : "",0) != kOkCsvRC )
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

cmXsRC_t cmXScoreWriteCsv( cmXsH_t h, const cmChar_t* csvFn )
{
  cmXsRC_t        rc           = kOkXsRC;
  cmXScore_t*     p            = _cmXScoreHandleToPtr(h);
  unsigned        rowIdx       = 1;
  double          tpqn         = 0; // ticks per quarter note
  double          tps          = 0; // ticks per second
  double          sec          = 0; // current time in seconds
  const cmChar_t* sectionIdStr = NULL;
        
  
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
      if( mp->divisions != 0 )
        tpqn = mp->divisions;
      
      cmXsNote_t* np = mp->noteL;
      
      double sec0 = sec;
      
      for(; np!=NULL; np=np->slink)
      {

        //  
        if( cmIsFlag(np->flags,kMetronomeXsFl) )
        {
          double bpm = np->duration;
          double bps = bpm / 60.0;
          tps = bps * tpqn;
        }

        double meas_sec = tps == 0 ? 0 : np->tick / tps;
        double sec1     = sec  + meas_sec;
        double dsecs    = sec1 - sec0;
        sec0            = sec1;

        // if this is a section event
        if( cmIsFlag(np->flags,kSectionXsFl) )
          sectionIdStr = np->tvalue;
        
        // if this is a bar event
        if( cmIsFlag(np->flags,kBarXsFl) )
        {
          _cmXScoreWriteCsvRow(p,rowIdx,-1,mp->number,NULL,"bar",dsecs,sec1,0,0,-1,0,np->flags);
        }
        else
          
        // if this is a pedal event
        if( cmIsFlag(np->flags,kPedalDnXsFl|kPedalUpXsFl|kPedalUpDnXsFl) )
        {
          unsigned d0 = 64; // pedal MIDI ctl id
          unsigned d1 = cmIsFlag(np->flags,kPedalDnXsFl) ? 64 : 0; // pedal-dn: d1>=64 pedal-up:<64
          _cmXScoreWriteCsvRow(p,rowIdx,-1,mp->number,NULL,"ctl",dsecs,sec1,d0,d1,-1,0,np->flags);
        }
        else 


          // if this is a sounding note event
          if( cmIsFlag(np->flags,kOnsetXsFl) )
          {
            double frac = np->rvalue + (cmIsFlag(np->flags,kDotXsFl) ? (np->rvalue/2) : 0);
        
            // 
            _cmXScoreWriteCsvRow(p,rowIdx,np->uid,mp->number,sectionIdStr,"non",dsecs,sec1,np->pitch,60,np->pitch,frac,np->flags);
            sectionIdStr = NULL;
          }
        
        rowIdx += 1;
      }

      sec = sec0;
    }    
  }

  if( cmCsvWrite( p->csvH, csvFn ) != kOkCsvRC )
    rc = cmErrMsg(&p->err,kCsvFailXsRC,"The CSV output write failed on file '%s'.",csvFn);

 errLabel:
  return rc;
}


void  cmXScoreReport( cmXsH_t h, cmRpt_t* rpt, bool sortFl )
{
  cmXScore_t* p  = _cmXScoreHandleToPtr(h);
  cmXsPart_t* pp = p->partL;
  
  for(; pp!=NULL; pp=pp->link)
  {
    cmRptPrintf(rpt,"Part:%s\n",pp->idStr);

    const cmXsMeas_t* meas = pp->measL;
    for(; meas!=NULL; meas=meas->link)
    {
      cmRptPrintf(rpt,"  %i : div:%i beat:%i beat-type:%i (%i)\n",meas->number,meas->divisions,meas->beats,meas->beat_type,meas->divisions*meas->beats);

      if( sortFl )
      {
        _cmXScoreReportTitle(rpt);
        
        const cmXsNote_t* note = meas->noteL;
        for(; note!=NULL; note=note->slink)
        {
          _cmXScoreReportNote(rpt,note);
        
          if( note->slink!=NULL  || note->voice->id==0)
            cmRptPrintf(rpt,"\n");
          else
            cmRptPrintf(rpt," %i\n", note->tick + note->duration);  
        }
        
      }
      else
      {
      
        _cmXScoreReportTitle(rpt);
        
        const cmXsVoice_t* v = meas->voiceL;
        for(; v!=NULL; v=v->link)
        {        
          const cmXsNote_t* note = v->noteL;
          
          cmRptPrintf(rpt,"    voice:%i\n",v->id);
          
          for(; note!=NULL; note=note->mlink)
          {
            _cmXScoreReportNote(rpt,note);

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


cmXsRC_t cmXScoreWriteMidi( cmXsH_t h, const cmChar_t* fn )
{
  assert(0); // function not implemented
  cmXScore_t* p  = _cmXScoreHandleToPtr(h);
  cmXsPart_t* pp = p->partL;
  
  for(; pp!=NULL; pp=pp->link)
  {
    const cmXsMeas_t* meas = pp->measL;
    for(; meas!=NULL; meas=meas->link)
    {

      const cmXsNote_t* note = meas->noteL;
      for(; note!=NULL; note=note->slink)
      {
        
      }
    }
  }  
}

cmXsRC_t cmXScoreTest( cmCtx_t* ctx, const cmChar_t* xmlFn, const cmChar_t* midiFn )
{
  cmXsRC_t rc;
  cmXsH_t h = cmXsNullHandle;
  
  if((rc = cmXScoreInitialize( ctx, &h, xmlFn, midiFn)) != kOkXsRC )
    return cmErrMsg(&ctx->err,rc,"XScore alloc failed.");

  cmXScoreWriteCsv(h,"/Users/kevin/temp/a0.csv");
  cmXScoreReport(h,&ctx->rpt,true);
  
  return cmXScoreFinalize(&h);

}
