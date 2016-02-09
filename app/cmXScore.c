#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
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
#include "cmLex.h"
#include "cmCsv.h"

cmXsH_t cmXsNullHandle = cmSTATIC_NULL_HANDLE;

enum
{
  kSectionXsFl   = 0x0001,  // rvalue holds section number
  kBarXsFl       = 0x0002,
  kRestXsFl      = 0x0004,
  kGraceXsFl     = 0x0008,
  kDotXsFl       = 0x0010,
  kChordXsFl     = 0x0020,
  kDynXsFl       = 0x0040,
  kEvenXsFl      = 0x0080,
  kTempoXsFl     = 0x0100,
  kPedalDnXsFl   = 0x0200,
  kPedalUpXsFl   = 0x0400,
  kPedalUpDnXsFl = 0x0800,
  kMetronomeXsFl = 0x1000  // duration holds BPM 
};


typedef struct cmXsNote_str
{
  unsigned             flags;    // See k???XsFl 
  unsigned             pitch;    // midi pitch
  unsigned             tick;     // 
  unsigned             duration; // duration in ticks
  unsigned             rvalue;   // 1/type = rythmic value (1/4=quarter note, 1/8=eighth note, ...)
  const cmChar_t*      tvalue;   // text value
  struct cmXsNote_str* mlink;    // measure note list 
  struct cmXsNote_str* slink;    // time sorted event list
} cmXsNote_t;

typedef struct cmXsVoice_str
{
  unsigned              id;    // Voice id
  cmXsNote_t*           noteL; // List of notes in this voice 
  struct cmXsVoice_str* link;  // Link to other voices in this measure
} cmXsVoice_t;

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

cmXsRC_t  _cmXScoreParsePitch( cmXScore_t* p, const cmXmlNode_t* nnp, unsigned* midiPitchRef )
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

  *midiPitchRef = midi;

  return rc;  
}

unsigned _cmXScoreParseNoteRValue( cmXScore_t* p, const cmXmlNode_t* nnp, const cmChar_t* label )
{
  typedef struct map_str
  {
    unsigned        rvalue;
    const cmChar_t* label;
  } map_t;

  map_t mapV[] =
  {
    {  -1, "measure" },  // whole measure rest
    {   1, "whole"   },
    {   2, "half"    },
    {   4, "quarter" },
    {   8, "eighth"  },
    {  16, "16th"    },
    {  32, "32nd"    },
    {  64, "64th"    },
    { 128, "128th"   },
    {   0, ""        }
  };

  const cmChar_t* str;
  if((str = cmXmlNodeValue(nnp,label,NULL)) == NULL)
  {
    if((nnp = cmXmlSearch(nnp,"rest",NULL,0)) != NULL )
    {
      const cmXmlAttr_t* a;
      if((a = cmXmlFindAttrib(nnp,"measure")) != NULL && cmTextCmp(a->value,"yes")==0)
        return -1;
    }
    return 0;
  }
  
  unsigned i;
  for(i=0; mapV[i].rvalue!=0; ++i)
    if( cmTextCmp(mapV[i].label,str) == 0 )
      return mapV[i].rvalue;
  
  return 0;
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
    { kEvenXsFl,                         "#0000FF" },
    { kTempoXsFl,                        "#00FF00" },
    { kDynXsFl,                          "#FF0000" },
    { kTempoXsFl | kEvenXsFl,            "#00FFFF" },
    { kDynXsFl   | kEvenXsFl,            "#FF00FF" },    
    { kDynXsFl   | kTempoXsFl,           "#FF7F00" },
    { kTempoXsFl | kEvenXsFl | kDynXsFl, "#996633" },
    { 0, "" }
  };
  
  if((a = cmXmlFindAttrib(nnp, "color" )) != NULL )
  {
    unsigned i;
    for(i=0; mapV[i].value != 0; ++i)
      if( cmTextCmp(a->value,mapV[i].label) == 0 )
      {
        note->flags += mapV[i].value;
        break;
      }

    if( mapV[i].value == 0 )
      rc = cmErrMsg(&p->err,kSyntaxErrorXsRC,"The note color '%s' was not found.",a->value);
  }

  return rc;
}

cmXsRC_t _cmXScoreParseNote(cmXScore_t* p, cmXsMeas_t* meas, const cmXmlNode_t* nnp, unsigned* tickRef )
{
  cmXsRC_t    rc   = kOkXsRC;
  cmXsNote_t* note = cmLhAllocZ(p->lhH,cmXsNote_t,1);
  unsigned    voiceId;

  // get the voice id for this node
  if( cmXmlNodeUInt(nnp,&voiceId,"voice",NULL) != kOkXmlRC )
    return _cmXScoreMissingNode(p,nnp,"voice");

  // if this note has a pitch
  if( cmXmlNodeHasChild(nnp,"pitch",NULL) )
    if((rc = _cmXScoreParsePitch(p,nnp,&note->pitch)) != kOkXsRC )
      return rc;

  // get the note duration
  cmXmlNodeUInt(nnp,&note->duration,"duration",NULL);
  
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

  // set color coded flags
  if((rc = _cmXScoreParseColor(p, nnp, note )) != kOkXsRC )
    return rc;
  
  // get the note's rythmic value
  if((note->rvalue =  _cmXScoreParseNoteRValue(p,nnp,"type")) == 0 )
    return _cmXScoreMissingNode(p,nnp,"type");

  note->tick = *tickRef;

  if( cmIsNotFlag(note->flags,kChordXsFl) )
    *tickRef += note->duration;
  
  return _cmXScorePushNote(p, meas, voiceId, note );
}

cmXsRC_t _cmXScorePushNonNote( cmXScore_t* p, cmXsMeas_t* meas, unsigned tick, unsigned duration, unsigned rvalue, const cmChar_t* tvalue, unsigned flags )
{
  cmXsNote_t* note    = cmLhAllocZ(p->lhH,cmXsNote_t,1);
  unsigned    voiceId = 0;    // non-note's are always assigned to voiceId=0;
  
  note->tick     = tick;
  note->flags    = flags;
  note->rvalue   = rvalue;
  note->tvalue   = tvalue;
  note->duration = duration;
  
  return _cmXScorePushNote(p, meas, voiceId, note );
}

cmXsRC_t  _cmXScoreParseDirection(cmXScore_t* p, cmXsMeas_t* meas, const cmXmlNode_t* dnp, unsigned tick)
{
  const cmXmlNode_t* np = NULL;
  const cmXmlAttr_t* a = NULL;
  unsigned           flags    = 0;
  int                offset   = 0;
  unsigned           rvalue   = 0;
  const cmChar_t*    tvalue   = NULL;
  unsigned           duration = 0;

  
  cmXmlNodeInt(dnp, &offset, "offset", NULL );
   
 
  // if this is a metronome direction
  if((np = cmXmlSearch( dnp, "metronome", NULL, 0)) != NULL )
  {
    
    if( cmXmlNodeUInt(np,&duration,"per-minute",NULL) != kOkXmlRC )
      return cmErrMsg(&p->err,kSyntaxErrorXsRC,"The 'per-minute' metronome value is missing on line %i.",np->line);

    if((rvalue = _cmXScoreParseNoteRValue(p,np,"beat-unit")) == 0 )
      return cmErrMsg(&p->err,kSyntaxErrorXsRC,"The 'beat-unit' metronome value is missing on line %i.",np->line);

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

  return _cmXScorePushNonNote(p,meas,tick+offset,duration,rvalue,tvalue,flags);            
      
}


cmXsRC_t _cmXScoreParseMeasure(cmXScore_t* p, cmXsPart_t* pp, const cmXmlNode_t* mnp)
{
  cmXsRC_t           rc   = kOkXsRC;
  const cmXmlNode_t* np   = NULL;  
  unsigned           tick = 0;

  // allocate the 'measure' record
  cmXsMeas_t* meas = cmLhAllocZ(p->lhH,cmXsMeas_t,1);

  // get measure number
  if( cmXmlAttrUInt(mnp,"number", &meas->number) != kOkXmlRC )
    return _cmXScoreMissingAttribute(p,mnp,"number");

  if( pp->measL == NULL )
    pp->measL = meas;
  else
  {
    cmXsMeas_t* m = pp->measL;
    while( m->link != NULL )
      m = m->link;
    
    m->link       = meas;
  }
  
  // get measure attributes node
  if((np = cmXmlSearch(mnp,"attributes",NULL,0)) != NULL)
  {
    cmXmlNodeUInt(np,&meas->divisions,"divisions",NULL);
    cmXmlNodeUInt(np,&meas->beats,    "time","beats",NULL);
    cmXmlNodeUInt(np,&meas->beat_type,"time","beat-type",NULL);
  }
  
  // store the bar line
  if((rc = _cmXScorePushNonNote(p,meas,tick,0,0,NULL,kBarXsFl)) != kOkXsRC )
    return rc;

  
  np = mnp->children;

  // for each child of the 'meas' XML node
  for(; rc==kOkXsRC && np!=NULL; np=np->sibling)
  {
    // if this is a 'note' node
    if( cmTextCmp(np->label,"note") == 0 )
    {
      rc = _cmXScoreParseNote(p,meas,np,&tick);
    }
    else
      // if this is a 'backup' node
      if( cmTextCmp(np->label,"backup") == 0 )
      {
        unsigned backup;
        cmXmlNodeUInt(np,&backup,"duration",NULL);
        tick -= backup;
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
  cmXsPart_t* pp = p->partL;
  for(; pp!=NULL; pp=pp->link)
  {
    cmXsMeas_t* mp = pp->measL;
    for(; mp!=NULL; mp=mp->link)
    {   
      // for each voice in this measure
      cmXsVoice_t* vp = mp->voiceL;
      for(; vp!=NULL; vp=vp->link)
      {
        cmXsNote_t* np = vp->noteL;
        for(; np!=NULL; np=np->mlink)
          mp->noteL = _cmXScoreInsertSortedNote(mp->noteL,np);        
      }      
    }
  }
}

cmXsRC_t cmXScoreInitialize( cmCtx_t* ctx, cmXsH_t* hp, const cmChar_t* xmlFn )
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

void _cmXScoreReportNote( cmRpt_t* rpt, const cmXsNote_t* note )
{
  const cmChar_t* B = cmIsFlag(note->flags,kBarXsFl)       ? "|" : "-";
  const cmChar_t* R = cmIsFlag(note->flags,kRestXsFl)      ? "R" : "-";
  const cmChar_t* G = cmIsFlag(note->flags,kGraceXsFl)     ? "G" : "-";
  const cmChar_t* D = cmIsFlag(note->flags,kDotXsFl)       ? "D" : "-";
  const cmChar_t* C = cmIsFlag(note->flags,kChordXsFl)     ? "C" : "-";
  const cmChar_t* e = cmIsFlag(note->flags,kEvenXsFl)      ? "e" : "-";
  const cmChar_t* d = cmIsFlag(note->flags,kDynXsFl)       ? "d" : "-";
  const cmChar_t* t = cmIsFlag(note->flags,kTempoXsFl)     ? "t" : "-";
  const cmChar_t* P = cmIsFlag(note->flags,kPedalDnXsFl)   ? "V" : "-";
  P = cmIsFlag(note->flags,kPedalUpXsFl)   ? "^" : P;
  P = cmIsFlag(note->flags,kPedalUpDnXsFl) ? "X" : P;
  const cmChar_t* N = note->pitch==0 ? " " : cmMidiToSciPitch( note->pitch, NULL, 0 );
  cmRptPrintf(rpt,"      %5i %5i %2i %3s %s%s%s%s%s%s%s%s%s",note->tick,note->duration,note->rvalue,N,B,R,G,D,C,e,d,t,P);

  if( cmIsFlag(note->flags,kSectionXsFl) )
    cmRptPrintf(rpt," %i",cmStringNullGuard(note->tvalue));

  printf("\n");
  
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
    if( cmCsvInsertTextColAfter(p->csvH, lcp, &lcp, s[i], 0 ) != kOkCsvRC )
      return cmErrMsg(&p->err,kCsvFailXsRC,"CSV error inserting CSV title %i.\n",i);

  }

  return kOkXsRC;
}

cmXsRC_t _cmXScoreWriteCsvBlankCols( cmXScore_t* p, unsigned cnt, cmCsvCell_t** leftCellPtrPtr )
{
  unsigned i;
  for(i=0; i<cnt; ++i)
    if( cmCsvInsertTextColAfter(p->csvH,*leftCellPtrPtr,leftCellPtrPtr,0,0) != kOkCsvRC )
      return cmErrMsg(&p->err,kCsvFailXsRC,"CSV output failed on blank column.");

  return kOkCsvRC;    
}

cmXsRC_t _cmXScoreWriteCsvRow(
  cmXScore_t*     p,
  unsigned        rowIdx,
  unsigned        bar,
  const cmChar_t* sectionStr,
  const cmChar_t* opCodeStr,
  double          dsecs,
  double          secs,
  unsigned        d0,
  unsigned        d1,
  unsigned        pitch,
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
  
  // cols 1,2
  if((rc = _cmXScoreWriteCsvBlankCols(p,2,&lcp)) != kOkXsRC )
    goto errLabel;

  // col 3 : output the opcode
  if( cmCsvInsertTextColAfter(p->csvH,lcp,&lcp,opCodeStr,0) != kOkCsvRC )
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
  if( cmCsvInsertUIntColAfter(p->csvH,lcp,&lcp,pitch,0) != kOkCsvRC )
  {
    rc = cmErrMsg(&p->err,kCsvFailXsRC,"CSV insert failed on 'pitch'.");
    goto errLabel;
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

  // col 15: even
  if( cmCsvInsertTextColAfter(p->csvH,lcp,&lcp,cmIsFlag(flags,kEvenXsFl) ? "e" : "",0) != kOkCsvRC )
  {
    rc = cmErrMsg(&p->err,kCsvFailXsRC,"CSV insert failed on eveness flag label.");
    goto errLabel;
  }

  // col 16: grace
  if( cmCsvInsertTextColAfter(p->csvH,lcp,&lcp,cmIsFlag(flags,kGraceXsFl) ? "g" : "",0) != kOkCsvRC )
  {
    rc = cmErrMsg(&p->err,kCsvFailXsRC,"CSV insert failed on eveness flag label.");
    goto errLabel;
  }
  
  // col 17: tempo
  if( cmCsvInsertTextColAfter(p->csvH,lcp,&lcp,cmIsFlag(flags,kTempoXsFl) ? "t" : "",0) != kOkCsvRC )
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
  if( cmCsvInsertTextColAfter(p->csvH,lcp,&lcp,cmIsFlag(flags,kSectionXsFl) ? sectionStr : "",0) != kOkCsvRC )
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
          _cmXScoreWriteCsvRow(p,rowIdx,mp->number,NULL,"bar",dsecs,sec1,0,0,0,0,np->flags);
        }
        else
          
        // if this is a pedal event
        if( cmIsFlag(np->flags,kPedalDnXsFl|kPedalUpXsFl|kPedalUpDnXsFl) )
        {
          unsigned d0 = 64; // pedal MIDI ctl id
          unsigned d1 = cmIsFlag(np->flags,kPedalDnXsFl) ? 64 : 0; // pedal-dn: d1>=64 pedal-up:<64
          _cmXScoreWriteCsvRow(p,rowIdx,mp->number,NULL,"ctl",dsecs,sec1,d0,d1,0,0,np->flags);
        }
        else 


          // if this is a sounding note event
          if( cmIsNotFlag(np->flags,kRestXsFl) )
          {
            double frac = np->rvalue + (cmIsFlag(np->flags,kDotXsFl) ? (np->rvalue/2) : 0);
        
            // 
            _cmXScoreWriteCsvRow(p,rowIdx,mp->number,sectionIdStr,"non",dsecs,sec1,0,0,np->pitch,frac,np->flags);
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
      cmRptPrintf(rpt,"  %i : div:%i beat:%i beat-type:%i\n",meas->number,meas->divisions,meas->beats,meas->beat_type);

      if( sortFl )
      {
        const cmXsNote_t* note = meas->noteL;
        for(; note!=NULL; note=note->slink)
          _cmXScoreReportNote(rpt,note);        
      }
      else
      {
      
        const cmXsVoice_t* v = meas->voiceL;
        for(; v!=NULL; v=v->link)
        {        
          const cmXsNote_t* note = v->noteL;
          
          cmRptPrintf(rpt,"    voice:%i\n",v->id);
          
          for(; note!=NULL; note=note->mlink)
            _cmXScoreReportNote(rpt,note);
        }
      }      
    }
  }  
}


cmXsRC_t cmXScoreTest( cmCtx_t* ctx, const cmChar_t* fn )
{
  cmXsRC_t rc;
  cmXsH_t h = cmXsNullHandle;
  
  if((rc = cmXScoreInitialize( ctx, &h, fn)) != kOkXsRC )
    return cmErrMsg(&ctx->err,rc,"XScore alloc failed.");

  cmXScoreWriteCsv(h,"/home/kevin/temp/a0.csv");
  cmXScoreReport(h,&ctx->rpt,true);
  
  return cmXScoreFinalize(&h);

}
