#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmMidi.h"
#include "cmLex.h"
#include "cmCsv.h"
#include "cmSymTbl.h"
#include "cmMidiFile.h"
#include "cmAudioFile.h"
#include "cmTimeLine.h"
#include "cmScore.h"
#include "cmVectOpsTemplateMain.h"

cmScH_t cmScNullHandle  = cmSTATIC_NULL_HANDLE;

enum
{
  kLabelCharCnt = 7,

  kInvalidDynScId = 0,

};

enum
{
  kMidiFileIdColScIdx= 0,  
  kTypeLabelColScIdx = 3,
  kDSecsColScIdx     = 4,
  kSecsColScIdx      = 5,
  kPitchColScIdx     = 11,
  kBarColScIdx       = 13,
  kSkipColScIdx      = 14,
  kEvenColScIdx      = 15,
  kGraceColScIdx     = 16,
  kTempoColScIdx     = 17,
  kFracColScIdx      = 18,
  kDynColScIdx       = 19,
  kSectionColScIdx   = 20,
  kRemarkColScIdx    = 21
};


typedef struct
{
  unsigned id;
  unsigned flag;
  cmChar_t label[ kLabelCharCnt + 1 ];
} cmScEvtRef_t; 

typedef struct cmScSect_str
{
  const cmChar_t*      label;
  unsigned             startIdx;
  struct cmScSect_str* link;
} cmScSect_t;

typedef struct cmScSetEle_str
{
  cmChar_t*              label;  
  unsigned               eleIdx;
  struct cmScSetEle_str* link;
} cmScSetEle_t;

typedef struct cmScSet_str
{
  unsigned            typeFl; // type of this set
  cmScSetEle_t*       eles;   // indexes of set elements
  cmScSetEle_t*       sects;  // application section labels
  bool                inFl;   // true if currently accepting elements
  struct cmScSet_str* link;   // 
} cmScSet_t;

typedef struct
{
  cmErr_t           err;
  cmCsvH_t          cH;
  cmSymTblH_t       stH;
  cmScCb_t          cbFunc;
  void*             cbArg;
  cmChar_t*         fn;
  double            srate;

  cmScoreEvt_t*     array;
  unsigned          cnt;

  cmScoreLoc_t*     loc;
  unsigned          locCnt;

  cmScoreSection_t* sect;
  unsigned          sectCnt;

  unsigned          sciPitchLexTId; // sci pitch and section id lexer token id's
  unsigned          sectionLexTId;

  cmScSect_t*       sectList;       // lists used during parsing
  cmScSet_t*        setList;    

  cmScoreSet_t*     sets;
  unsigned          setCnt;

  unsigned*         dynRefArray;
  unsigned          dynRefCnt;  

  unsigned          nxtLocIdx;
  unsigned          minSetLocIdx;
  unsigned          maxSetLocIdx;
  
} cmSc_t;

cmScEvtRef_t _cmScEvtRefArray[] = 
{
  { kTimeSigEvtScId, 0, "tsg" },
  { kKeySigEvtScId, 0,  "ksg" },
  { kTempoEvtScId, 0,   "tmp" },
  { kTrackEvtScId, 0,   "trk" },
  { kTextEvtScId, 0,    "txt" },
  { kEOTrackEvtScId, 0, "eot" },
  { kCopyEvtScId, 0,    "cpy" },
  { kBlankEvtScId, 0,   "blk" },
  { kBarEvtScId, 0,     "bar" },
  { kPgmEvtScId, 0,     "pgm" },
  { kCtlEvtScId, 0,     "ctl" },
  { kNonEvtScId, 0,     "non" },
  { kInvalidEvtScId, 0, "***" }
};

cmScEvtRef_t _cmScDynRefArray[] = 
{
  { 1, 0, "pppp" },
  { 2, 0, "ppp" },
  { 3, 0, "pp"  },
  { 4, 0, "p"   },
  { 5, 0, "mp"  },
  { 6, 0, "mf"  },
  { 7, 0, "f"   },
  { 8, 0, "ff"  },
  { 9,0, "fff" },
  { kInvalidDynScId,0, "***" },
};

cmScEvtRef_t _cmScVarRefArray[] = 
{
  { kEvenVarScId, kEvenScFl, "e"},
  { kDynVarScId,  kDynScFl,  "d"},
  { kTempoVarScId,kTempoScFl,"t"},
  { cmInvalidId,  0,         "@"}
};

cmSc_t* _cmScHandleToPtr( cmScH_t h )
{ 
  cmSc_t* p = (cmSc_t*)h.h;
  assert( p != NULL );
  return p;
}

unsigned _cmScEvtTypeLabelToId( const cmChar_t* label )
{
  cmScEvtRef_t* r = _cmScEvtRefArray;
  for(; r->id != kInvalidEvtScId; ++r )
    if( strcmp(label,r->label) == 0 )
      return r->id;
  return kInvalidEvtScId;
}

const cmChar_t* cmScEvtTypeIdToLabel( unsigned id )
{
  cmScEvtRef_t* r = _cmScEvtRefArray;
  for(; r->id != kInvalidEvtScId; ++r )
    if( r->id == id )
      return r->label;
  return NULL;
}

unsigned _cmScDynLabelToId( const cmChar_t* label )
{
  cmScEvtRef_t* r = _cmScDynRefArray;
  for(; r->id != kInvalidEvtScId; ++r )
    if( strncmp(label,r->label,strlen(r->label)) == 0 )
      return r->id;
  return kInvalidDynScId;
}

// return the count of dynamic label/id pairs
unsigned _cmScDynLabelCount( )
{
  unsigned      n = 0;
  cmScEvtRef_t* r = _cmScDynRefArray;
  for(; r->id != kInvalidEvtScId; ++r )
    ++n;
  return n;
}
const cmChar_t* cmScDynIdToLabel( unsigned id )
{
  cmScEvtRef_t* r = _cmScDynRefArray;
  for(; r->id != kInvalidDynScId; ++r )
    if( r->id == id )
      return r->label;
  return NULL;
}

char _cmScVarFlagToChar( unsigned flags )
{
  unsigned i;
  for(i=0; _cmScVarRefArray[i].id!=cmInvalidId; ++i)
    if( _cmScVarRefArray[i].flag == flags )
      return _cmScVarRefArray[i].label[0];
  assert(0);
  return ' ';
}

char _cmScVarIdToChar( unsigned id )
{
  unsigned i;
  for(i=0; _cmScVarRefArray[i].id!=cmInvalidId; ++i)
    if( _cmScVarRefArray[i].id == id )
      return _cmScVarRefArray[i].label[0];
  assert(0);
  return ' ';
}

unsigned _cmScVarFlagToId( unsigned flags )
{
  unsigned i;
  for(i=0; _cmScVarRefArray[i].id!=cmInvalidId; ++i)
    if( _cmScVarRefArray[i].flag == flags )
      return _cmScVarRefArray[i].id;
  assert(0);
  return cmInvalidId;
}

const char* _cmScFlagsToStr( unsigned flags, char* buf, int bufCharCnt )
{

  unsigned i=0;
  if( cmIsFlag(flags,kEvenScFl) )
  {
    assert(i<bufCharCnt);
    buf[i] = 'e';
    ++i;
  }

  if( cmIsFlag(flags,kDynScFl) )
  {
    assert(i<bufCharCnt);
    buf[i] = 'd';
    ++i;
  }

  if( cmIsFlag(flags,kTempoScFl ))
  {
    assert(i<bufCharCnt);
    buf[i] = 't';
    ++i;
  }

  assert(i<bufCharCnt);
  buf[i] = 0;
  return buf;
}

unsigned _cmScLexSciPitchMatcher( const cmChar_t* cp, unsigned cn )
{
  if( cp==NULL || cn < 2 )
    return 0;

  // first char must be "A-G"
  if( strspn(cp,"ABCDEFG") != 1 )
    return 0;

  unsigned i = 1;

  // next char could be accidental
  if( cp[i] == '#' || cp[i] == 'b' )
    ++i; // i==2

  // the 2nd or 3rd char must be a digit
  if( i>=cn || isdigit(cp[i]) == false )
    return 0;

  ++i;  // i==2 or i==3

  // the 3rd or 4th char must be a digit or EOS
  if( i>=cn || isdigit(cp[i]) == false )
    return i;
  
  ++i;

  return i;  
}

unsigned _cmScLexSectionIdMatcher( const cmChar_t* cp, unsigned cn )
{
  if( cp==NULL || cn < 2 )
    return 0;

  // first char must be a number
  if( !isdigit(cp[0]) )
    return 0;

  // if 2nd char is a char then terminate
  if( 'a'<=cp[1] && cp[1]<='z' )
    return 2;

  // if 2nd char is digit and 3rd char is char then terminate
  if( isdigit(cp[1]) && cn>2 &&  'a'<=cp[2] && cp[2]<='z' )
    return 3;

  return 0;
}

void _cmScFreeSetList( cmScSet_t* setList )
{
  cmScSet_t* tp = setList;
  cmScSet_t* ntp = NULL;
  while(tp!=NULL)
  {
    ntp = tp->link;

    cmScSetEle_t* ep = tp->eles;    
    while( ep != NULL )
    {
      cmScSetEle_t* nep = ep->link;
      cmMemFree(ep);
      ep = nep;
    }

    ep = tp->sects;    
    while( ep != NULL )
    {
      cmScSetEle_t* nep = ep->link;
      cmMemFree(ep->label);
      cmMemFree(ep);
      ep = nep;
    }

    cmMemFree(tp);
    tp = ntp;
  }
}


cmScRC_t _cmScFinalize( cmSc_t* p )
{
  cmScRC_t rc = kOkScRC;
  unsigned i;

  if( cmCsvFinalize(&p->cH) != kOkCsvRC )
    return rc;

  if( p->sets != NULL )
  {
    for(i=0; i<p->setCnt; ++i)
    {
      cmMemFree(p->sets[i].eleArray);
      cmMemFree(p->sets[i].sectArray);
      cmMemFree(p->sets[i].symArray);
      cmMemFree(p->sets[i].costSymArray);
    }
    cmMemFree(p->sets);
  }

  _cmScFreeSetList(p->setList);

  if( p->loc != NULL )
  {
    for(i=0; i<p->locCnt; ++i)
    {
      cmMemFree(p->loc[i].evtArray);
      if( p->loc[i].begSectPtr != NULL )
        cmMemFree(p->loc[i].begSectPtr->setArray);
    }
    cmMemFree(p->loc);
  }

  cmMemPtrFree(&p->dynRefArray);

  cmMemFree(p->sect);
  cmMemFree(p->fn);
  cmMemFree(p->array);
  cmMemFree(p);
  return rc;
}

cmScRC_t _cmScParseBar( cmSc_t* p, unsigned rowIdx, cmScoreEvt_t* s, int* barNumb )
{
  if((*barNumb = cmCsvCellInt(p->cH,rowIdx,kBarColScIdx)) == INT_MAX )
    return cmErrMsg(&p->err,kSyntaxErrScRC,"Unable to parse the bar number.");

  s->type      = kBarEvtScId;
  s->secs      = 0;
  s->barNumb   = *barNumb;
  s->csvRowNumb = rowIdx + 1;
  return kOkScRC;
}

cmScSet_t* _cmScNewSet( cmSc_t* p, unsigned typeFl )
{
  // create a new set record 
  cmScSet_t* nsp = cmMemAllocZ(cmScSet_t,1);
  nsp->inFl = true;
  nsp->typeFl = typeFl;

  if( p->setList == NULL )
    p->setList = nsp;
  else
  {

    // go to the end of the the set list 
    cmScSet_t*  sp = p->setList;
    assert(sp!=NULL);
    while( sp->link != NULL )
      sp = sp->link;

    sp->link   = nsp;
  }


  return nsp;
}

cmScSet_t* _cmScFindSet( cmSc_t* p, unsigned typeFl )
{
  // locate the set currently accepting ele's for this type
  cmScSet_t*    sp = p->setList;

  for(; sp != NULL; sp=sp->link )
    if( sp->typeFl == typeFl && sp->inFl )
      break;
  return sp;
}

void _cmScSetDone(cmSc_t* p, unsigned typeFl)
{
  cmScSet_t* sp = _cmScFindSet(p,typeFl);
  assert( sp != NULL );
  sp->inFl = false;
}

// This function is called during parsing to 
// insert a set element or set section into a cmScSet_t
// element or section linked list. Either the scoreIdx
// or the label is valid but not both.
cmScSet_t* _cmScInsertSetEle(cmSc_t* p, unsigned scoreIdx, unsigned typeFl, const cmChar_t* label, unsigned labelCharCnt)
{
  assert( scoreIdx!=cmInvalidId || (scoreIdx==cmInvalidIdx && label!=NULL && labelCharCnt>0));

  cmScSet_t* sp = _cmScFindSet(p,typeFl);

  if( sp == NULL )
    sp = _cmScNewSet(p,typeFl);

  // allocate a new set element record
  cmScSetEle_t*  nep  = cmMemAllocZ(cmScSetEle_t,1);  
  cmScSetEle_t** list = NULL;

  nep->eleIdx  = scoreIdx;

  if( label == NULL )
  {
    // all elements must be of the same type
    assert( sp->typeFl == typeFl );
    sp->typeFl = typeFl;
    list = &sp->eles;
  }
  else
  {
    nep->label   = cmMemAllocStrN(label,labelCharCnt);
    list = &sp->sects;
  }

  // *list refers to sp->sects or sp->ele's depending on the type of ele
  if( *list == NULL )
    *list = nep;
  else
  {
    // got to the last element in the set
    cmScSetEle_t* ep = *list;
    while( ep->link != NULL )
      ep = ep->link;

    // append the new element to the end of the list
    ep->link = nep;
  }

  return sp;
}

// Extract the next attribute section identifier.
const cmChar_t*  _cmScParseOneSetSection( cmSc_t* p, unsigned typeFl, const cmChar_t* c0p )
{
  // advance white space
  while( *c0p && (isspace(*c0p) || *c0p==',') )
    ++c0p;

  if( *c0p==0 )
    return c0p;
      
  // c0p now points to a section id or an asterisk
  const cmChar_t* c1p = c0p;

  // advance past section id
  while( *c1p && (!isspace(*c1p) && (isdigit(*c1p) || isalpha(*c1p))))
    ++c1p;

  // if c0p pointed to an asterisk then c1p is still equal to c0p
  if( c1p > c0p )
    _cmScInsertSetEle(p,cmInvalidIdx,typeFl,c0p,c1p-c0p);

  return c1p;
}

// Parse an attribute string to extract the section
// identifiers which may follow the attribute token (e,t,mf,ff,...)
cmScRC_t _cmScParseAttr(cmSc_t* p, unsigned scoreIdx, const cmChar_t* text, unsigned typeFl)
{
  const cmChar_t* cp = text;

  // insert a set element - all attribute's produce one element record
  _cmScInsertSetEle(p,scoreIdx,typeFl,NULL,0);

  // advance past the attribute type marking (e,t,(p,mf,f,fff,etc)) in search
  // of section identifiers
  while( *cp && !isspace(*cp) )
    ++cp;

  if( *cp )
  {
    // search for the first section identifier
    if((cp =_cmScParseOneSetSection(p,typeFl,cp)) != NULL )
    {
      bool asteriskFl = false;

      // search for the second section identifier
      if((cp = _cmScParseOneSetSection(p,typeFl,cp)) != NULL && *cp!=0 )
        asteriskFl = *cp == '*';
      
      _cmScSetDone(p,typeFl);

      // if the attr just parsed ended with an asterisk then it is both
      // the last element of the previous set and the first ele of the
      // next set
      if( asteriskFl )
      {
        // if the attr just parsed had a section id then it was the last
        // element in the set - create a new set record to hold the next set
        _cmScNewSet(p,typeFl);

        _cmScInsertSetEle(p,scoreIdx,typeFl,NULL,0);
      }
    }
  }
  return kOkScRC;
}

void _cmScPrintSets( const cmChar_t* label, cmScSet_t* setList )
{
  printf("%s\n",label);

  const cmScSet_t* sp = setList;
  for(; sp != NULL; sp=sp->link )
  {
    const cmScSetEle_t* ep = sp->eles;
    for(; ep!=NULL; ep=ep->link)
      printf("%i ",ep->eleIdx);

    printf(" : ");
    for(ep=sp->sects; ep!=NULL; ep=ep->link)
      printf("%s ",cmStringNullGuard(ep->label));

    printf("\n");
  }
}

cmScRC_t _cmScParseNoteOn( cmSc_t* p, unsigned rowIdx, cmScoreEvt_t* s, unsigned scoreIdx, int barNumb, unsigned barNoteIdx )
{
  cmScRC_t        rc     = kOkScRC;
  unsigned        flags  = 0;
  unsigned        dynVal = kInvalidDynScId;
  const cmChar_t* sciPitch;
  cmMidiByte_t    midiPitch;
  const cmChar_t* attr;
  double          secs;
  double          durSecs;
  const cmCsvCell_t* cell;

  s += scoreIdx;

  // verify the scientific pitch cell was formatted correcly  
  if((cell = cmCsvCellPtr(p->cH,rowIdx,kPitchColScIdx)) == NULL || cell->lexTId != p->sciPitchLexTId )
    return cmErrMsg(&p->err,kSyntaxErrScRC,"Pitch column format error.");

  if((sciPitch = cmCsvCellText(p->cH,rowIdx,kPitchColScIdx)) == NULL )
    return cmErrMsg(&p->err,kSyntaxErrScRC,"Expected a scientific pitch value");
          
  if((midiPitch = cmSciPitchToMidi(sciPitch)) == kInvalidMidiPitch)
   return cmErrMsg(&p->err,kSyntaxErrScRC,"Unable to convert the scientific pitch '%s' to a MIDI value. ");

  // it is possible that note delta-secs field is empty - so default to 0
  if((secs =  cmCsvCellDouble(p->cH, rowIdx, kSecsColScIdx )) == DBL_MAX) // Returns DBL_MAX on error.
    flags += kInvalidScFl;

  // skip attribute
  if((attr = cmCsvCellText(p->cH,rowIdx,kSkipColScIdx)) != NULL && *attr == 's' )
    flags += kSkipScFl;

  // evenness attribute
  if((attr = cmCsvCellText(p->cH,rowIdx,kEvenColScIdx)) != NULL && *attr == 'e' )
  {
    flags += kEvenScFl;
    _cmScParseAttr(p,scoreIdx,attr,kEvenScFl);
  }

  // grace attribute
  if((attr = cmCsvCellText(p->cH,rowIdx,kGraceColScIdx)) != NULL && *attr == 'g' )
  {
    flags += kGraceScFl;
    if( cmIsNotFlag(flags,kEvenScFl) )
      return cmErrMsg(&p->err,kSyntaxErrScRC,"All 'grace' notes should also be 'even' notes.");
  }

  // tempo attribute
  if((attr = cmCsvCellText(p->cH,rowIdx,kTempoColScIdx)) != NULL && *attr == 't' )
  {
    flags += kTempoScFl;
    _cmScParseAttr(p,scoreIdx,attr,kTempoScFl);
  }
       
  // dynamics attribute
  if((attr = cmCsvCellText(p->cH,rowIdx,kDynColScIdx)) != NULL )
  {
    if((dynVal = _cmScDynLabelToId(attr)) == kInvalidDynScId )
      return cmErrMsg(&p->err,kSyntaxErrScRC,"Unknown dynamic label '%s'.",cmStringNullGuard(attr));

    flags += kDynScFl;

    _cmScParseAttr(p,scoreIdx,attr,kDynScFl);

  }

  // tempo/non-grace rythmic value
  if( cmIsFlag(flags,kTempoScFl) || (cmIsFlag(flags,kEvenScFl) && cmIsNotFlag(flags,kGraceScFl)) )
  {
    double frac = cmCsvCellDouble(p->cH,rowIdx,kFracColScIdx);

    // no 'frac' value is given for the last note of the set we must accept error
    // values here and validate the actual values later
    if( frac>0 && frac!=DBL_MAX )
      s->frac = frac;
  }

  // Returns DBL_MAX on error.
  if((durSecs =  cmCsvCellDouble(p->cH, rowIdx, kDSecsColScIdx )) == DBL_MAX) 
    durSecs = 0.25;


  s->type       = kNonEvtScId;
  s->secs       = secs;
  s->pitch      = midiPitch;
  s->flags      = flags;
  s->dynVal     = dynVal; 
  s->barNumb    = barNumb;
  s->barNoteIdx = barNoteIdx;
  s->durSecs    = durSecs;
  s->csvRowNumb = rowIdx+1;
  return rc;
}

cmScRC_t _cmScParseSectionColumn( cmSc_t* p, unsigned rowIdx, unsigned evtIdx, cmScSect_t* sectList )
{
  const cmCsvCell_t* cell;
  cmScSect_t*        sect;
  const cmChar_t*    label;

  // most rows don't have section labels
  if(  (cell  = cmCsvCellPtr( p->cH,rowIdx,kSectionColScIdx)) == NULL
    || (label = cmCsvCellText(p->cH,rowIdx,kSectionColScIdx))  == NULL)
    return kOkScRC;

  // verify the section id type
  if( cell->lexTId != p->sectionLexTId && cell->lexTId != kIntLexTId )
    return cmErrMsg(&p->err,kSyntaxErrScRC,"'section' column format error.");
  
  sect = cmMemAllocZ(cmScSect_t,1);

  sect->label    = label;
  sect->startIdx = evtIdx;

  //printf("section:%s\n",label);

  cmScSect_t* sp = sectList;
  assert( sp != NULL );
  while( sp->link != NULL )
    sp = sp->link;

  sp->link = sect;

  return kOkScRC;
}

cmScoreSection_t* _cmScLabelToSection( cmSc_t* p, const cmChar_t* label )
{
  int i;
  for(i=0; i<p->sectCnt; ++i)
    if( strcmp(p->sect[i].label,label) == 0 )
      return p->sect + i;
  return NULL;
}



// Calculate the total number of all types of sets and
// then convert each of the cmScSet_t linked list's to 
// a single linear cmScoreSet_t list (p->sets[]).
cmScRC_t _cmScProcSets( cmSc_t* p )
{
  cmScRC_t rc = kOkScRC;

  // calculate the count of all sets
  unsigned   i;
  unsigned   n  = 0;
  cmScSet_t* sp = p->setList;
  for(n=0; sp!=NULL; sp=sp->link)
    if( sp->eles != NULL )
      ++n;
  
  // allocate the linear set array
  p->sets   = cmMemAllocZ(cmScoreSet_t,n);
  p->setCnt = n;
  
  // fill in the linear set array
  sp = p->setList;
  for(i=0;  sp!=NULL; sp=sp->link)
    if( sp->eles != NULL )
    {
      unsigned j;
      unsigned en;
      unsigned rowNumb = 0;

      assert( i<n );

      // get the count of elements assoc'd with this set
      cmScSetEle_t* ep = sp->eles;
      for(en=0; ep!=NULL; ep=ep->link)
        ++en;

      assert( en > 0 );

      // allocate the element array
      p->sets[i].eleCnt   = en;    
      p->sets[i].eleArray = cmMemAllocZ(cmScoreEvt_t*,en);

      // fill in the element array
      ep = sp->eles;
      unsigned graceCnt = 0;
      for(j=0; ep!=NULL; ep=ep->link,++j)
      {
        assert(ep->eleIdx != cmInvalidIdx && ep->eleIdx<p->cnt);
        p->sets[i].eleArray[j] = p->array + ep->eleIdx;
        assert( cmIsFlag( p->sets[i].eleArray[j]->flags, sp->typeFl) );
        rowNumb = p->array[ep->eleIdx].csvRowNumb;

        unsigned flags = p->array[ep->eleIdx].flags;

        // count grace notes
        if( cmIsFlag(flags,kGraceScFl) )
          ++graceCnt;

        // validate the 'frac' field - all but the last note in 
        // tempo and non-grace evenness sets must have a non-zero 'frac' value.
        if( en>0 && j<en-1 && (sp->typeFl==kTempoScFl || (sp->typeFl==kEvenScFl && graceCnt==0)) && p->array[ep->eleIdx].frac==0)
          rc = cmErrMsg(&p->err,kSyntaxErrScRC,"The note on row number %i must have a non-zero 'frac' value.",p->array[ep->eleIdx].csvRowNumb);          
      }    

      // get the count of sections assoc'd with this set
      ep = sp->sects;
      for(en=0; ep!=NULL; ep=ep->link)
        ++en;

      // allocate the section array
      p->sets[i].varId    = _cmScVarFlagToId(sp->typeFl);
      p->sets[i].sectCnt   = en;    
      p->sets[i].sectArray = cmMemAllocZ(cmScoreSection_t*,en);
      p->sets[i].symArray  = cmMemAllocZ(unsigned,en);
      p->sets[i].costSymArray = cmMemAllocZ(unsigned,en);

      // fill in the section array with sections which this set will be applied to
      ep = sp->sects;
      for(j=0; ep!=NULL; ep=ep->link,++j)
      {
        cmScoreSection_t* sp;
        assert(ep->label != NULL);
        if((sp  =  _cmScLabelToSection(p, ep->label )) == NULL )
          rc = cmErrMsg(&p->err,kSyntaxErrScRC,"The section labelled '%s' could not be found for the set which includes row number %i.",ep->label,rowNumb);        
        else
        {
          if( cmSymTblIsValid(p->stH) )
          {
            p->sets[i].symArray[j]     = cmSymTblRegisterFmt(p->stH,"%c-%s", _cmScVarIdToChar(p->sets[i].varId),ep->label);
            p->sets[i].costSymArray[j] = cmSymTblRegisterFmt(p->stH,"c%c-%s",_cmScVarIdToChar(p->sets[i].varId),ep->label);
          }
          else
          {
            p->sets[i].symArray[j] = cmInvalidId;
            p->sets[i].symArray[j] = cmInvalidId;
          }

          p->sets[i].sectArray[j] = sp;

          sp->setArray = cmMemResizeP(cmScoreSet_t*,sp->setArray,++sp->setCnt);
          sp->setArray[sp->setCnt-1] = p->sets + i;
        }
        
      }

      ++i;
    }

  assert(i==n);

  // assign each set to the location which contains it's last element.
  // (this is earliest point in the score location where all the  
  // performance information contained in the set may be valid)
  for(i=0; i<p->setCnt; ++i)
  {
    assert( p->sets[i].eleCnt >= 1 );

    // get a ptr to the last element for the ith set
    const cmScoreEvt_t* ep = p->sets[i].eleArray[ p->sets[i].eleCnt-1 ];
    unsigned j,k;

    // find the location which contains the last element
    for(j=0; j<p->locCnt; ++j)
    {
      for(k=0; k<p->loc[j].evtCnt; ++k)
        if( p->loc[j].evtArray[k] == ep )
          break;

      if(k<p->loc[j].evtCnt)
        break;
    }

    assert( j<p->locCnt );

    // assign the ith set to the location which contains it's last element
    p->sets[i].llink   = p->loc[j].setList;
    p->loc[j].setList = p->sets + i;
  }

  return rc;
}


cmScRC_t _cmScProcSections( cmSc_t* p, cmScSect_t* sectList )
{
  cmScRC_t rc = kOkScRC;
  unsigned i;

  // count the sections
  cmScSect_t* sp = sectList;
  p->sectCnt = 0;
  for(; sp!=NULL; sp=sp->link)
    if( sp->label != NULL )
      ++p->sectCnt;

  // alloc a section array
  p->sect = cmMemAllocZ(cmScoreSection_t,p->sectCnt);
  
  // fill the section array
  sp = sectList;
  for(i=0; sp!=NULL; sp=sp->link)
    if( sp->label != NULL )
    {
      p->sect[i].label    = sp->label;
      p->sect[i].index    = i;
      p->sect[i].begEvtIndex = sp->startIdx;
      ++i;
    }

  // assign the begSectPtr to each section
  for(i=0; i<p->sectCnt; ++i)
  {
    assert( p->sect[i].begEvtIndex < p->cnt );
    unsigned j,k;
    const cmScoreEvt_t* ep = p->array + p->sect[i].begEvtIndex;
    for(j=0; j<p->locCnt; ++j)
    {
      for(k=0; k<p->loc[j].evtCnt; ++k)
        if( p->loc[j].evtArray[k] == ep )
        {
          p->loc[j].begSectPtr = p->sect + i;
          p->sect[i].locPtr    = p->loc + j;
          break;
        }

      if( k<p->loc[j].evtCnt)
        break;
    }
  }

  // release the section linked list
  sp = sectList;
  cmScSect_t* np = NULL;
  while(sp!=NULL)
  {
    np = sp->link;
    cmMemFree(sp);
    sp = np;
  }

  //_cmScPrintSets("Sets",p->setList );

  _cmScProcSets(p);


  return rc;
}

cmScRC_t _cmScParseFile( cmSc_t* p, cmCtx_t* ctx, const cmChar_t* fn )
{
  cmScRC_t    rc         = kOkScRC;
  unsigned    barNoteIdx = 0;
  int         barEvtIdx  = cmInvalidIdx;
  int         barNumb    = 0;
  double      secs;
  double      cur_secs   = 0;

  p->sectList = cmMemAllocZ(cmScSect_t,1); // section zero
  
  //_cmScNewSet(p); // preallocate the first set

  // initialize the CSV file parser
  if( cmCsvInitialize(&p->cH, ctx ) != kOkCsvRC )
  {
    rc = cmErrMsg(&p->err,kCsvFailScRC,"Score file initialization failed.");
    goto errLabel;
  }

  // register custom lex token matchers for sci-pitch and section id's
  if( cmCsvLexRegisterMatcher(p->cH, p->sciPitchLexTId = cmCsvLexNextAvailId(p->cH)+0, _cmScLexSciPitchMatcher ) != kOkCsvRC 
    ||cmCsvLexRegisterMatcher(p->cH, p->sectionLexTId = cmCsvLexNextAvailId(p->cH)+1, _cmScLexSectionIdMatcher) != kOkCsvRC )
  {
    rc = cmErrMsg(&p->err,kCsvFailScRC,"CSV token matcher registration failed.");
    goto errLabel;
  }

  // parse the CSV file
  if( cmCsvParseFile(p->cH, fn, 0 ) != kOkCsvRC )
  {
    rc = cmErrMsg(&p->err,kCsvFailScRC,"CSV file parsing failed on the file '%s'.",cmStringNullGuard(fn));
    goto errLabel;
  }

  // allocate the score event array
  p->cnt   = cmCsvRowCount(p->cH);
  p->array = cmMemAllocZ(cmScoreEvt_t,p->cnt);

  unsigned i,j;

  // skip labels line - start on line 1
  for(i=1,j=0; i<p->cnt && rc==kOkScRC; ++i)
  {
    // get the row 'type' label
    const char* typeLabel;
    if((typeLabel = cmCsvCellText(p->cH,i,kTypeLabelColScIdx)) == NULL )
    {
      rc = cmErrMsg(&p->err,kSyntaxErrScRC,"No type label.");
      break;
    }

    // convert the row 'type' label to an id
    unsigned tid;
    if((tid =  _cmScEvtTypeLabelToId(typeLabel)) == kInvalidEvtScId) 
    {
      rc = cmErrMsg(&p->err,kSyntaxErrScRC,"Unknown type '%s'.",cmStringNullGuard(typeLabel));
      break;
    }

    secs = DBL_MAX;

    switch(tid)
    {
      case kBarEvtScId: // parse bar lines        
        if((rc = _cmScParseBar(p,i,p->array+j,&barNumb)) == kOkScRC )
        {
          barNoteIdx = 0;
          barEvtIdx  = j;
          p->array[j].index = j;
          ++j;
        }
        break;

      case kNonEvtScId:  // parse note-on events
        if((rc =  _cmScParseNoteOn(p, i, p->array, j, barNumb, barNoteIdx )) == kOkScRC )
        {
          secs =  p->array[j].secs;

          if( p->array[j].secs == DBL_MAX )
            p->array[j].secs = cur_secs;

          if( cmIsFlag(p->array[j].flags,kSkipScFl) == false )
          {
            p->array[j].index = j;
            ++j;
          }

          ++barNoteIdx;
        }
        break;

      default:
        // Returns DBL_MAX on error.
        secs =  cmCsvCellDouble(p->cH, i, kSecsColScIdx );
        break;
    }
    
    if( secs != DBL_MAX )
      cur_secs = secs;

    // form the section list
    if( j > 0 )
      if((rc = _cmScParseSectionColumn(p,i,j-1,p->sectList)) != kOkScRC )
        break;

    // the bar lines don't have times so set the time of the bar line to the
    // time of the first event in the bar.
    if( barEvtIdx != cmInvalidIdx && secs != DBL_MAX )
    {
      assert( p->array[ barEvtIdx ].type == kBarEvtScId );
      p->array[ barEvtIdx ].secs = secs;

      // handle the case where the previous bar had no events
      // BUG BUG BUG this is a hack which will fail if the first bar does not have events.
      if( barEvtIdx>=1 && p->array[ barEvtIdx-1].type == kBarEvtScId )
        p->array[ barEvtIdx-1].secs = secs;

      barEvtIdx = cmInvalidIdx;
    }
    
  }

  if( rc == kSyntaxErrScRC )
  {
    cmErrMsg(&p->err,rc,"Syntax error on line %i in '%s'.",i+1,cmStringNullGuard(fn));           
    goto errLabel;
  }

  p->cnt = j;
  
 errLabel:

  return rc;
}

cmScRC_t _cmScInitLocArray( cmSc_t* p )
{
  cmScRC_t rc       = kOkScRC;
  double   maxDSecs = 0;  // max time between events that are considered simultaneous
  unsigned barNumb  = 0;
  int i,j,k;

  if( p->cnt==0)
    return rc;

  p->locCnt = 1;

  // count the number of unique time locations in the score
  p->locCnt = 1;
  for(i=1; i<p->cnt; ++i )
  {
    if( p->array[i].secs < p->array[i-1].secs )
      rc = cmErrMsg(&p->err,kSyntaxErrScRC,"The time associated with the score entry on line %i is less than the previous line.",p->array[i].csvRowNumb);

    if( (p->array[i].secs - p->array[i-1].secs) > maxDSecs )
      ++p->locCnt;
  }

  if( rc != kOkScRC )
    return rc;

  // allocate the loc. array
  p->loc = cmMemAllocZ(cmScoreLoc_t,p->locCnt);

  // fill in the location array
  for(i=0,k=0; i<p->cnt; ++k)
  {
    j = i+1;

    // get the count of events at this location
    while( j<p->cnt && p->array[j].secs - p->array[j-1].secs <= maxDSecs )
      ++j;

    assert(k<p->locCnt);

    p->loc[k].index    = k;
    p->loc[k].evtCnt   = j-i;
    p->loc[k].evtArray = cmMemAllocZ(cmScoreEvt_t*,p->loc[k].evtCnt);

    // fill in the location record event pointers
    for(j=0; j<p->loc[k].evtCnt; ++j)
    {
      p->loc[k].evtArray[j] = p->array + (i + j);

      p->loc[k].evtArray[j]->locIdx = k;

      if( p->array[i+j].type == kBarEvtScId )
        barNumb = p->array[i+j].barNumb;
    }

    // fill in the location record
    p->loc[k].secs     = p->array[i].secs;
    p->loc[k].barNumb  = barNumb;

    i += p->loc[k].evtCnt;

  }

  assert( p->locCnt == k );

  return rc;
}


cmScRC_t cmScoreInitialize( cmCtx_t* ctx, cmScH_t* hp, const cmChar_t* fn, double srate, const unsigned* dynRefArray, unsigned dynRefCnt, cmScCb_t cbFunc, void* cbArg, cmSymTblH_t stH )
{
  cmScRC_t rc = kOkScRC;
  if((rc = cmScoreFinalize(hp)) != kOkScRC )
    return rc;

  cmSc_t* p = cmMemAllocZ(cmSc_t,1);

  cmErrSetup(&p->err,&ctx->rpt,"Score");

  p->stH   = stH;

  if((rc = _cmScParseFile(p,ctx,fn)) != kOkScRC )
    goto errLabel;

  if((rc = _cmScInitLocArray(p)) != kOkScRC )
    goto errLabel;

  if((rc = _cmScProcSections(p,p->sectList)) != kOkScRC )
    goto errLabel;

  // load the dynamic reference array
  if( dynRefArray != NULL && dynRefCnt > 0)
  {
    unsigned n = _cmScDynLabelCount();
    if( dynRefCnt != n )
    {
      rc = cmErrMsg(&p->err,kInvalidDynRefCntScRC,"The count of dynamics labels must be %i not %i.",n,dynRefCnt);
      goto errLabel;      
    }

    p->dynRefArray = cmMemAllocZ(unsigned,dynRefCnt);
    memcpy(p->dynRefArray,dynRefArray,sizeof(unsigned)*dynRefCnt);
    p->dynRefCnt   = dynRefCnt;
  }

  p->srate        = srate;
  p->cbFunc       = cbFunc;
  p->cbArg        = cbArg;
  p->fn           = cmMemAllocStr(fn);
  p->nxtLocIdx    = 0;
  p->minSetLocIdx = cmInvalidIdx;
  p->maxSetLocIdx = cmInvalidIdx;

  hp->h = p;

  cmScoreClearPerfInfo(*hp);

  //cmScorePrintLoc(*hp);
 errLabel:
  if( rc != kOkScRC )
    _cmScFinalize(p);

  return rc;
}

cmScRC_t cmScoreFinalize( cmScH_t* hp )
{
  cmScRC_t rc = kOkScRC;

  if( hp == NULL || cmScoreIsValid(*hp) == false )
    return kOkScRC;

  cmSc_t* p = _cmScHandleToPtr(*hp);

  if((rc = _cmScFinalize(p)) != kOkScRC )
    return rc;
  
  hp->h = NULL;
  
  return rc;
}

const cmChar_t* cmScoreFileName( cmScH_t h )
{
  cmSc_t* p = _cmScHandleToPtr(h);
  return p->fn;
}

double          cmScoreSampleRate( cmScH_t h )
{
  cmSc_t* p = _cmScHandleToPtr(h);
  return p->srate;
}


bool     cmScoreIsValid( cmScH_t h )
{ return h.h != NULL; }

unsigned      cmScoreEvtCount( cmScH_t h )
{
  cmSc_t* p = _cmScHandleToPtr(h);
  return p->cnt;
}

cmScoreEvt_t* cmScoreEvt( cmScH_t h, unsigned idx )
{
  cmSc_t* p = _cmScHandleToPtr(h);
  if( idx >= p->cnt )
  {
    cmErrMsg(&p->err,kInvalidIdxScRC,"%i is an invalid index for %i records.",idx,p->cnt);
    return NULL;
  }
  return p->array + idx;
}

unsigned      cmScoreSectionCount( cmScH_t h )
{ 
  cmSc_t* p = _cmScHandleToPtr(h);
  return p->sectCnt;
}

cmScoreSection_t* cmScoreSection( cmScH_t h, unsigned idx )
{
  cmSc_t* p = _cmScHandleToPtr(h);
  assert( idx < p->sectCnt);
  return p->sect + idx;
}


unsigned      cmScoreLocCount( cmScH_t h )
{
  cmSc_t* p = _cmScHandleToPtr(h);
  return p->locCnt;
}

cmScoreLoc_t* cmScoreLoc( cmScH_t h, unsigned idx )
{
  cmSc_t* p = _cmScHandleToPtr(h);
  if( idx >= p->locCnt )
  {
    cmErrMsg(&p->err,kInvalidIdxScRC,"%i is an invalid index for %i location records.",idx,p->locCnt);
    return NULL;
  }
  return p->loc + idx;
}


void cmScorePrintLoc( cmScH_t h )
{
  unsigned i          = 0;
  cmSc_t*  p          = _cmScHandleToPtr(h);
  unsigned colCnt     = 10;
  int      bufCharCnt = 4;
  char     buf[ bufCharCnt ];
  const char*     emptyStr = "        ";

  // for each set of 'colCnt' columns
  for(i=0; i<p->locCnt; i+=colCnt )
  {
    // print the location 'index' line
    unsigned c,j,k;
    printf("index: ");
    for(c=0,j=i; j<p->locCnt && c<colCnt; ++c,++j)
      printf("%7i ",j);
    printf("\n");

    // print the 'sectn' label line
    printf("sectn: ");
    for(c=0,j=i; j<p->locCnt && c<colCnt; ++c,++j)
      if( p->loc[j].begSectPtr==NULL )
        printf("%s",emptyStr);
      else
        printf("%7s ",p->loc[j].begSectPtr->label);
    printf("\n");

    // calculate the max number of simultan. events at any one location
    // for this set of 'colCnt' columns.
    unsigned n=0;
    for(c=0,j=i; j<p->locCnt && c<colCnt; ++c,++j)
      if( p->loc[j].evtCnt > n )
        n = p->loc[j].evtCnt;

    // for each 'sco' line
    for(k=0; k<n; ++k)
    {
      printf("sco%2i: ",k);
      for(c=0,j=i; j<p->locCnt && c<colCnt; ++c,++j)
        if( k >= p->loc[j].evtCnt )
          printf("%s",emptyStr);
        else
        {
          switch(p->loc[j].evtArray[k]->type)
          {
            case kBarEvtScId:
              printf("   |%3i ",p->loc[j].evtArray[k]->barNumb);
              break;

            case kNonEvtScId:
              {
                int bn=16;
                char b[bn];
                strcpy(b,cmMidiToSciPitch(p->loc[j].evtArray[k]->pitch,NULL,0));
                strcat(b,_cmScFlagsToStr( p->loc[j].evtArray[k]->flags,buf,bufCharCnt));
                assert(strlen(b)<bn);
                printf("%7s ",b);
                break;
              }
          }
        }

      printf("\n");
    }

    // calc the max number of set triggers which will occur on
    // any one location for this set of 'colCnt' columns.
    n=0;
    for(c=0,j=i; j<p->locCnt && c<colCnt; ++c,++j)
      if(p->loc[j].begSectPtr != NULL && p->loc[j].begSectPtr->setCnt > n )
        n = p->loc[j].begSectPtr->setCnt;      

    for(k=0; k<n; ++k)
    {
      printf("trig%1i: ",k);
      for(c=0,j=i; j<p->locCnt && c<colCnt; ++c,++j)
      {
        if( p->loc[j].begSectPtr != NULL && k<p->loc[j].begSectPtr->setCnt )
        {   
          const cmScoreSet_t* sp = p->loc[j].begSectPtr->setArray[k];
          printf("  %3s-%c ",p->loc[j].begSectPtr->label,_cmScVarIdToChar(sp->varId) );
        }
        else
        {
          printf("%s",emptyStr);
        }
      }
      printf("\n");
    }

    printf("\n");
  }
}

unsigned cmScoreSetCount( cmScH_t h )
{
  cmSc_t* p = _cmScHandleToPtr(h);
  return p->setCnt;
}

cmScRC_t      cmScoreSeqNotify( cmScH_t h )
{
  cmScRC_t  rc = kOkScRC;
  cmSc_t*   p  = _cmScHandleToPtr(h);
  cmScMsg_t m;
  unsigned  i;

  if( p->cbFunc != NULL )
  {
    memset(&m.u.evt,0,sizeof(m.u.evt));
    m.typeId = kBeginMsgScId;
    p->cbFunc(p->cbArg,&m,sizeof(m));

    m.typeId = kEventMsgScId;
    for(i=0; i<p->cnt; ++i)
    {
      m.u.evt = p->array[i];
      p->cbFunc(p->cbArg,&m,sizeof(m));
    }

    m.typeId = kSectionMsgScId;
    for(i=0; i<p->sectCnt; ++i)
    {
      m.u.sect = p->sect[i];
      p->cbFunc(p->cbArg,&m,sizeof(m));
    }

    memset(&m.u.evt,0,sizeof(m.u.evt));
    m.typeId = kEndMsgScId;
    p->cbFunc(p->cbArg,&m,sizeof(m));

  }
  return rc;
}

void          cmScoreClearPerfInfo( cmScH_t h )
{
  cmSc_t*   p  = _cmScHandleToPtr(h);
  unsigned i;
  for(i=0; i<p->cnt; ++i)
  {
    p->array[i].perfSmpIdx = cmInvalidIdx;
    p->array[i].perfVel    = 0;
    p->array[i].perfDynLvl = 0;
  }

  for(i=0; i<p->locCnt; ++i)
  {
    cmScoreSet_t* sp = p->loc[i].setList;
    for(; sp!=NULL; sp=sp->llink)
      sp->doneFl = false;
  }

  for(i=0; i<p->sectCnt; ++i)
  {
    unsigned j;
    for(j=0; j<kScVarCnt; ++j)
      p->sect[i].vars[j] = DBL_MAX;
  }

  p->nxtLocIdx = 0;
  p->minSetLocIdx = cmInvalidIdx;
  p->maxSetLocIdx = cmInvalidIdx;

}

bool _cmScIsSetPerfDone( cmScoreSet_t* sp )
{
  unsigned i = 0;
  for(i=0; i<sp->eleCnt; ++i)
    if( sp->eleArray[i]->perfSmpIdx == cmInvalidIdx )
      return false;
  return true;
}


void _cmScPerfSortTimes( unsigned *v, unsigned n )
{
  unsigned i;
  bool fl = true;
  while(fl && n)
  {
    fl = false;
    for(i=1; i<n; ++i)
    {
      if( v[i-1] > v[i] )
      {
        unsigned t = v[i-1];
        v[i-1] = v[i];
        v[i] = t;
        fl = true;
      }
    }
   --n;
  }
}

bool _cmScPerfEven(cmSc_t* p, cmScoreSet_t* stp, bool printMissFl)
{
  unsigned      i   = 0;
  double        u   = 0;
  double        x   = 0;
  bool          sortFl = false;
  bool          printFl = true;

  unsigned v[ stp->eleCnt ];
  unsigned d[ stp->eleCnt - 1];
  assert( stp->eleCnt > 1 );

  // calculate the sum of the time between events
  for(i=0; i<stp->eleCnt; ++i)
  {
    // if this event was not received - then the set is not valid
    if( stp->eleArray[i]->perfSmpIdx == cmInvalidIdx )
    {
      if( printFl && printMissFl)
        printf("EVENESS: missing loc:%i %s\n",stp->eleArray[i]->locIdx,cmMidiToSciPitch(stp->eleArray[i]->pitch,NULL,0));
      return false;
    }

    // load v[i]
    v[i] = stp->eleArray[i]->perfSmpIdx;

    // check for out of order elements
    if( i> 0 )
      if( v[i] < v[i-1] )
        sortFl = true;
  } 

  // sort the times in ascending order
  if( sortFl )
    _cmScPerfSortTimes(v,stp->eleCnt);


  // calc the sum of time differences 
  for(i=1; i<stp->eleCnt; ++i)
    u += d[i-1] = v[i] - v[i-1];

  // calculate the average time between events
  u /= stp->eleCnt-1;

  // calculate the std-dev of the time between events
  for(i=0; i<stp->eleCnt-1; ++i)
    x += (d[i]-u)*(d[i]-u);
  
  double sd = sqrt(x/(stp->eleCnt-1));

  // compute the average z-score
  double c = 0;
  for(i=0; i<stp->eleCnt-1; ++i)
    c += fabs(d[i]-u)/sd;


  stp->value = c/(stp->eleCnt-1);
  stp->doneFl = true;

  if(printFl)
  {
    /*
    for(i=0; i<stp->eleCnt; ++i)
    {
      printf("%i %i ",i,v[i]);
      if( i > 0 )
        printf("%i ", d[i-1]);
      printf("\n");
    }
    */
    printf("%s EVENESS:%f\n",sortFl?"SORTED ":"",stp->value);
  }

  return true;
}

bool _cmScPerfDyn( cmSc_t* p, cmScoreSet_t* stp, bool printMissFl)
{
  double   a       = 0;
  unsigned i       = 0;
  bool     printFl = true;

  for(i=0; i<stp->eleCnt; ++i)
  {
    unsigned j;

    // if this event was not received - then the set is not valid
    if( stp->eleArray[i]->perfSmpIdx == cmInvalidIdx )
    {
      if( printFl && printMissFl )
        printf("DYNAMICS: missing loc:%i %s\n",stp->eleArray[i]->locIdx,cmMidiToSciPitch(stp->eleArray[i]->pitch,NULL,0));
      return false;
    }

    unsigned m  = 0;  // lower bound for the first dyn. category

    // determine the dynamic category for the performed velocity of each event
    for(j=0; j<p->dynRefCnt; ++j)
    {
      // if the vel fall's into the jth dyn. category
      if( m <= stp->eleArray[i]->perfVel && stp->eleArray[i]->perfVel < p->dynRefArray[j] )
        break;

      // store the min vel for the next dyn category
      m = p->dynRefArray[j];
    }

    assert( j < p->dynRefCnt );

    stp->eleArray[i]->perfDynLvl = j+1;

    a += abs((j+1) - stp->eleArray[i]->dynVal);

    if( p->cbFunc != NULL )
    {
      cmScMsg_t m;
      m.typeId        = kDynMsgScId;
      m.u.dyn.evtIdx  = stp->eleArray[i]->index;
      m.u.dyn.dynLvl  = stp->eleArray[i]->perfDynLvl;
      p->cbFunc(p->cbArg,&m,sizeof(m));
    }

  }

  stp->value  = a / stp->eleCnt;
  stp->doneFl = true;

  if( printFl )
    printf("DYNAMICS:%f\n",stp->value);

  return true;
}

bool _cmScPerfTempo1(cmSc_t* p, cmScoreSet_t* stp, bool printMissFl)
{
  bool     printFl = true;
  unsigned durSmpCnt = 0;
  double   durBeats  = 0;
  int      i;

  for(i=0; i<stp->eleCnt; ++i)
  {
    // if this event was not received - then the set is not valid
    if( stp->eleArray[i]->perfSmpIdx == cmInvalidIdx )
    {
      if( printFl && printMissFl )
        printf("TEMPO: missing loc:%i %s\n",stp->eleArray[i]->locIdx,cmMidiToSciPitch(stp->eleArray[i]->pitch,NULL,0));
      return false;
    }

    if( i > 0 )
      durSmpCnt += stp->eleArray[i]->perfSmpIdx - stp->eleArray[i-1]->perfSmpIdx;

    durBeats += stp->eleArray[i]->frac;
  }

  stp->value = durBeats / (durSmpCnt / p->srate*60.0 );
  stp->doneFl = true;

  if( printFl )
    printf("TEMPO:%f\n",stp->value);

  return true;
}

bool _cmScPerfTempo(cmSc_t* p, cmScoreSet_t* stp, bool printMissFl)
{
  bool     printFl = true;
  unsigned durSmpCnt = 0;
  double   durBeats  = 0;
  int      i;

  unsigned bi = cmInvalidIdx;
  unsigned ei = cmInvalidIdx;
  unsigned missCnt = 0;

  for(i=0; i<stp->eleCnt; ++i)
  {
    // if this event was not received - then the set is not valid
    if( stp->eleArray[i]->perfSmpIdx == cmInvalidIdx )
    {
      ++missCnt;
      if( printFl && printMissFl )
        printf("TEMPO: missing loc:%i %s\n",stp->eleArray[i]->locIdx,cmMidiToSciPitch(stp->eleArray[i]->pitch,NULL,0));
    }
    else
    {
      if( bi == cmInvalidIdx )
        bi = i;
      
      ei = i;
    }
  }

  if( ei > bi )
  {
    for(i=bi; i<=ei; ++i)
      durBeats += stp->eleArray[i]->frac;

    durSmpCnt = stp->eleArray[ei]->perfSmpIdx - stp->eleArray[bi]->perfSmpIdx;

    stp->value = durBeats / (durSmpCnt / (p->srate*60.0) );

    stp->doneFl = true;

  }

  if( printFl )
    printf("TEMPO:%f bi:%i ei:%i secs:%f bts:%f\n",stp->value,bi,ei,durSmpCnt/p->srate,durBeats);

  return true;
}


void _cmScPerfExec( cmSc_t* p, cmScoreSet_t* sp, bool printMissFl )
{
  if( sp->doneFl == false )
  {
    switch( sp->varId )
    {
      case kEvenVarScId:  
        _cmScPerfEven(p,sp,printMissFl);
        break;

      case kDynVarScId:   
        _cmScPerfDyn(p,sp,printMissFl);
        break;

      case kTempoVarScId: 
        _cmScPerfTempo(p,sp,printMissFl);
        break;

      default:
        { assert(0); }
    }
  }
    
}

void _cmScPerfExecRange( cmSc_t* p )
{
  if( p->minSetLocIdx == cmInvalidIdx || p->maxSetLocIdx==cmInvalidIdx )
    return;
  
  unsigned i = p->minSetLocIdx;
  for(; i<=p->maxSetLocIdx; ++i)
  {
    cmScoreSet_t* sp = p->loc[i].setList;
    for(; sp!=NULL; sp=sp->llink)      
      _cmScPerfExec(p,sp,true);
    
  }
}

bool  _cmScSetPerfEvent( cmSc_t* p, unsigned locIdx, unsigned smpIdx, unsigned pitch, unsigned vel )
{
  assert(locIdx < p->locCnt );
  cmScoreLoc_t* lp       = p->loc + locIdx;
  bool          doneFl   = true;
  unsigned      i;
#ifndef NDEBUG
  bool          foundFl  = false;
#endif

  // locate the event at the loc[locIdx]
  for(i=0; i<lp->evtCnt; ++i)
  {
    cmScoreEvt_t* ep = lp->evtArray[i];
    if( ep->type == kNonEvtScId  )
    {
      if( ep->pitch == pitch )
      {
        assert( ep->perfSmpIdx == cmInvalidIdx );
      
        ep->perfSmpIdx = smpIdx;
        ep->perfVel    = vel;
#ifndef NDEBUG
        foundFl        = true;
#endif
      }

      // check if all notes have arrived for this location
      if( ep->perfSmpIdx == cmInvalidIdx )
        doneFl = false;
    }
  }

  // the event must always be found 
  assert( foundFl );

  return doneFl;
}

bool  cmScoreSetPerfEvent( cmScH_t h, unsigned locIdx, unsigned smpIdx, unsigned pitch, unsigned vel )
{
  cmSc_t* p  = _cmScHandleToPtr(h);
  return _cmScSetPerfEvent(p,locIdx,smpIdx,pitch,vel);
}

void  cmScoreExecPerfEvent( cmScH_t h, unsigned locIdx, unsigned smpIdx, unsigned pitch, unsigned vel )
{
  unsigned      i;
  cmSc_t*       p        = _cmScHandleToPtr(h);
  bool          doneFl   = _cmScSetPerfEvent(p,locIdx,smpIdx,pitch,vel);
  unsigned      printLvl = 1;
  cmScoreLoc_t* lp       = p->loc + locIdx;


  // all events for a location must be complete to trigger attached events
  if( doneFl == false )
    return;

  if( p->loc[locIdx].setList != NULL )
  {
    // set idx of most recent loc w/ a set end event
    p->maxSetLocIdx = locIdx; 

    if( p->minSetLocIdx == cmInvalidIdx )
      p->minSetLocIdx = locIdx;
  }

  // attempt to calculate all sets between loc[p->minSetLocIdx] and loc[p->maxSetLocIdx]
  _cmScPerfExecRange(p);

  // prevent event retriggering or going backwards
  if( printLvl && locIdx < p->nxtLocIdx )
  {
    printf("----- BACK ----- \n");
    return;
  }

  if( printLvl && locIdx > p->nxtLocIdx )
  {
    printf("----- SKIP ----- \n");
  }

  // for each location between the current and previous location
  for(; p->nxtLocIdx<=locIdx; ++p->nxtLocIdx)
  {

    lp = p->loc + p->nxtLocIdx;

    // if this location is the start of a new section - then apply
    // sets that are assigned to this section
    if( lp->begSectPtr != NULL && lp->begSectPtr->setCnt > 0 )
    {
      // notice the location of the oldest section start - once we cross this point
      // it is too late to notice set completions - so incr p->inSetLocIdx 
      if( lp->begSectPtr->setCnt ) 
        p->minSetLocIdx = p->nxtLocIdx+1;

      for(i=0; i<lp->begSectPtr->setCnt; ++i)
      {
        cmScoreSet_t* stp = lp->begSectPtr->setArray[i];

        // temporarily commented out for testing purposes
        // if( stp->doneFl == false )
        //  _cmScPerfExec(p, stp, printLvl>0 );
        
        if( stp->doneFl )
        {
          assert( stp->varId < kScVarCnt );

          lp->begSectPtr->vars[ stp->varId ] = stp->value;

          if( p->cbFunc != NULL )
          {
            cmScMsg_t m;
            m.typeId        = kVarMsgScId;
            m.u.meas.varId  = stp->varId;
            m.u.meas.value  = stp->value;
            p->cbFunc(p->cbArg,&m,sizeof(m));

          }
        }
      }        
    }
  }
}

void  cmScoreSetPerfValue(  cmScH_t h, unsigned locIdx, unsigned varId, double value )
{
  cmSc_t*       p      = _cmScHandleToPtr(h);

  int li = locIdx;
  for(; li>=0; --li)
    if( p->loc[li].begSectPtr != NULL )
    {
      assert( varId < kScVarCnt );
      p->loc[li].begSectPtr->vars[varId] = value;
      break;
    }

  assert( li>=0);
}

void cmScoreSetPerfDynLevel( cmScH_t h, unsigned evtIdx, unsigned dynLvl )
{
  cmSc_t* p = _cmScHandleToPtr(h);
  
  assert(evtIdx < p->cnt );
  p->array[ evtIdx ].perfDynLvl = dynLvl;
}


cmScRC_t      cmScoreDecode( const void* msg, unsigned msgByteCnt, cmScMsg_t* m)
{
  cmScMsg_t* mp = (cmScMsg_t*)msg;
  *m = *mp;
  return kOkScRC;
}


void cmScorePrint( cmScH_t h, cmRpt_t* rpt )
{
  cmSc_t* p = _cmScHandleToPtr(h);
  unsigned i;
  for(i=0; i<20 /*p->cnt*/; ++i)
  {
    cmScoreEvt_t* r = p->array + i;
    switch(r->type)
    {
      case kNonEvtScId:
        cmRptPrintf(rpt,"%5i %3i %3i %s 0x%2x %c%c%c %s\n",
          i,
          r->barNumb,
          r->barNoteIdx,
          cmScEvtTypeIdToLabel(r->type),
          r->pitch,
          cmIsFlag(r->flags,kEvenScFl)  ? 'e' : ' ',
          cmIsFlag(r->flags,kTempoScFl) ? 't' : ' ',
          cmIsFlag(r->flags,kDynScFl)   ? 'd' : ' ',
          cmIsFlag(r->flags,kDynScFl)   ? cmScDynIdToLabel(r->dynVal) : "");          
        break;

      default:
        break;
    }
  }
}

void cmScoreTest( cmCtx_t* ctx, const cmChar_t* fn )
{
  cmScH_t h = cmScNullHandle;
  if( cmScoreInitialize(ctx,&h,fn,0,NULL,0,NULL,NULL, cmSymTblNullHandle ) != kOkScRC )
    return;

  cmScorePrint(h,&ctx->rpt);

  cmScoreFinalize(&h);
}

// 1. Fix absolute message time which was incorrect on original score file.
// 2. 
void cmScoreFix( cmCtx_t* ctx )
{
  const cmChar_t*          mfn  = "/home/kevin/src/cmgv/src/gv/data/ImaginaryThemes.mid";
  const cmChar_t*          crfn = "/home/kevin/src/cmgv/src/gv/data/mod0a.txt";
  const cmChar_t*          cwfn = "/home/kevin/src/cmgv/src/gv/data/mod1.csv";
  cmMidiFileH_t            mfH  = cmMidiFileNullHandle;
  cmCsvH_t                 csvH = cmCsvNullHandle;
  const cmMidiTrackMsg_t** msg  = NULL;
  double                   secs = 0.0;
  int                      ci,mi,crn,mn;
  bool                     errFl = true;
  unsigned                 handCnt = 0;
  unsigned                 midiMissCnt = 0;

  if( cmCsvInitialize(&csvH,ctx) != kOkCsvRC )
    goto errLabel;

  if( cmCsvLexRegisterMatcher(csvH, cmCsvLexNextAvailId(csvH), _cmScLexSciPitchMatcher ) != kOkCsvRC )
    goto errLabel;

  if( cmCsvParseFile(csvH, crfn, 0 ) != kOkCsvRC )
    goto errLabel;

  if( cmMidiFileOpen(mfn,&mfH,ctx) != kOkMfRC )
    goto errLabel;

  cmMidiFileTickToMicros(mfH);

  cmMidiFileCalcNoteDurations(mfH);

  mn = cmMidiFileMsgCount(mfH);

  msg = cmMidiFileMsgArray(mfH);

  crn = cmCsvRowCount(csvH);

  // for each row in the score file
  for(ci=1,mi=0; ci<crn && cmCsvLastRC(csvH)==kOkCsvRC; ++ci)
  {
    unsigned  id;

    // zero the duration column 
    if( cmCsvCellPtr(csvH, ci, kDSecsColScIdx ) != NULL )
      cmCsvSetCellUInt(   csvH, ci, kDSecsColScIdx, 0 );

    // get the MIDI file event id for this row
    if((id = cmCsvCellUInt(csvH,ci,kMidiFileIdColScIdx)) == UINT_MAX)
    {
      // this is a hand-entered event -  so it has no event id
      ++handCnt;
      
    }
    else
    {
      for(; mi<mn; ++mi)
      {
        const cmMidiTrackMsg_t* m = msg[mi];

        assert( mi+1 <= id );
        secs += m->dtick/1000000.0;

        if( mi+1 != id )
        {
          if( m->status == kNoteOnMdId && m->u.chMsgPtr->d1>0 )
          {
            // this MIDI note-on does not have a corresponding score event
            ++midiMissCnt;
          }
        }
        else
        {
          cmCsvSetCellDouble( csvH, ci, kSecsColScIdx, secs );
          ++mi;

          if( m->status == kNoteOnMdId )
            cmCsvSetCellDouble(   csvH, ci, kDSecsColScIdx, m->u.chMsgPtr->durTicks/1000000.0 );
          break;
        }
        
        
      }

      if( mi==mn)
        printf("done on row:%i\n",ci);
    }
  }

  if( cmCsvLastRC(csvH) != kOkCsvRC )
    goto errLabel;

  if( cmCsvWrite(csvH,cwfn) != kOkCsvRC )
    goto errLabel;

  errFl = false;

 errLabel:
  if( errFl )
    printf("Score fix failed.\n");
  else
    printf("Score fix done! hand:%i miss:%i\n",handCnt,midiMissCnt);
  cmMidiFileClose(&mfH);

  cmCsvFinalize(&csvH);

}
