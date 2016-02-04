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

cmXsH_t cmXsNullHandle = cmSTATIC_NULL_HANDLE;

enum
{
  kRestXsFl    = 0x0001,
  kGraceXsFl   = 0x0002,
  kDotXsFl     = 0x0004
};


typedef struct cmXsNote_str
{
  unsigned flags;
  unsigned pitch;     // midi pitch
  unsigned duration;  // duration in ticks
  unsigned rvalue;    // 1/type = rythmic value (1/4=quarter note, 1/8=eighth note, ...)
  struct cmXsNote_str* link;
  
} cmXsNote_t;

typedef struct cmXsVoice_str
{
  unsigned              id;    // Voice id
  cmXsNote_t*           noteL; // List of notes in this voice 
  struct cmXsVoice_str* link;  // Link to other voices in this measure
} cmXsVoice_t;

typedef struct cmXsMeas_str
{
  unsigned             number;  // Measure number
  
  unsigned             divisions; 
  unsigned             beats;
  unsigned             beat_type;
  
  cmXsVoice_t*         voiceL;  // List of voices in this measure   
  
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
  cmErr_t              err;
  cmXmlH_t             xmlH;
  cmLHeapH_t           lhH;
  cmXsPart_t*          partL;
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
  if( cmXmlIsValid(p->xmlH) )
    cmXmlFree( &p->xmlH );

  // release the local linked heap memory
  if( cmLHeapIsValid(p->lhH) )
    cmLHeapDestroy(&p->lhH);
  
  cmMemFree(p);
  
  return rc;
}

cmXsRC_t _cmXScoreMissingNode( cmXScore_t* p, const cmChar_t* label, const cmXmlAttr_t* attr )
{
  if( attr == NULL )
    return cmErrMsg(&p->err,kSyntaxErrorXsRC,"Missing XML node '%s'.",label);

  return cmErrMsg(&p->err,kSyntaxErrorXsRC,"Missing XML node '%s' - Attribute:%s=%s",label,attr->label,attr->value);
}

cmXsRC_t _cmXScoreMissingAttribute( cmXScore_t* p, const cmXmlNode_t* np, const cmChar_t* attrLabel )
{
  return cmErrMsg(&p->err,kSyntaxErrorXsRC,"Missing XML attribute '%s' from node '%s'.",attrLabel,np->label);
}

cmXsRC_t _cmXScoreParsePartList( cmXScore_t* p )
{
  cmXsRC_t           rc          = kOkXsRC;
  cmXsPart_t*        lastPartPtr = NULL;
  const cmXmlNode_t* xnp;

  // find the 'part-list'
  if((xnp = cmXmlSearch( cmXmlRoot(p->xmlH), "part-list", NULL, 0)) == NULL )
    return _cmXScoreMissingNode(p,"part-list",NULL);
  
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
    return _cmXScoreMissingNode(p,"step",NULL);
  
  if((rc = cmXmlNodeUInt( nnp,&octave,"pitch","octave",NULL)) != kOkXmlRC )
    return _cmXScoreMissingNode(p,"octave",NULL);
  
  cmXmlNodeDouble( nnp,&alter,"pitch","alter",NULL);

  cmChar_t buf[3] = { *step, '0', '\0'};
  unsigned midi = cmSciPitchToMidi(buf);

  midi         += (12 * octave);

  midi         += alter;

  *midiPitchRef = midi;

  return rc;  
}

unsigned _cmXScoreParseNoteType( const cmXmlNode_t* nnp )
{
  typedef struct map_str
  {
    unsigned        rvalue;
    const cmChar_t* label;
  } map_t;

  map_t mapV[] =
  {
    { 1, "whole" },
    { 2, "half"  },
    { 4, "quarter" },
    { 8, "eighth" },
    { 16,"16th"},
    { 32,"32nd"},
    { 64,"64th"},
    {128,"128th"},
    {0,""}
  };

  if( cmXmlNodeHasChild(nnp,"type") )
  {
    const cmChar_t* str;
    if((str = cmXmlNodeValue(nnp,"type",NULL)) == NULL)
    {
      unsigned i;
      for(i=0; mapV[i].rvalue!=0; ++i)
        if( cmTextCmp(mapV[i].label,str) == 0 )
          return mapV[i].rvalue;
    }

  }

  return 0;
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
  if((v = _cmXScoreIdToVoice(meas,voiceId)) == NULL)
  {
    v = cmLhAllocZ(p->lhH,cmXsVoice_t,1);
    v->id = voiceId;
    
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

  if( v->noteL == NULL )
    v->noteL = note;
  else
  {
    cmXsNote_t* n = v->noteL;
    while( n != NULL )
      n = n->link;

    n->link = note;
  }

  return kOkXsRC;
}

cmXsRC_t _cmXScoreParseNote(cmXScore_t* p, cmXsMeas_t* meas, const cmXmlNode_t* nnp)
{
  cmXsRC_t    rc   = kOkXsRC;
  
  cmXsNote_t* note = cmLhAllocZ(p->lhH,cmXsNote_t,1);
  unsigned voiceId;

  // get the voice id for this node
  if( cmXmlNodeUInt(nnp,&voiceId,"voice",NULL) != kOkXmlRC )
    return _cmXScoreMissingNode(p,"voice",NULL);

  
  // if this note has a pitch
  if( cmXmlNodeHasChild(nnp,"pitch") )
    if((rc = _cmXScoreParsePitch(p,nnp,&note->pitch)) != kOkXsRC )
      return rc;

  // is 'rest'
  if( cmXmlNodeHasChild(nnp,"rest") )
    note->flags |= kRestXsFl;

  // is 'grace'
  if( cmXmlNodeHasChild(nnp,"grace") )
    note->flags |= kGraceXsFl;

  // is 'dot'
  if( cmXmlNodeHasChild(nnp,"dot") )
    note->flags |= kDotXsFl;

  if((note->rvalue =  _cmXScoreParseNoteType(nnp)) == 0 )
    return _cmXScoreMissingNode(nnp,"type",NULL);
    
}

cmXsRC_t _cmXScoreParseMeasure(cmXScore_t* p, cmXsPart_t* pp, const cmXmlNode_t* mnp)
{
  cmXsRC_t rc = kOkXsRC;

  // allocate the 'measure' record
  cmXsMeas_t* meas = cmLhAllocZ(p->lhH,cmXsMeas_t,1);
  const cmXmlNode_t* np;

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
    m->link = meas;
  }
  
  // get measure attributes node
  if((np = cmXmlSearch(mnp,"attributes",NULL,0)) == NULL)
    return rc; // (this measure does not have any attributes)

  cmXmlNodeUInt(np,&meas->divisions,"divisions",NULL);
  cmXmlNodeUInt(np,&meas->beats,"time","beats",NULL);
  cmXmlNodeUInt(np,&meas->beat_type,"time","beat-type",NULL);

  int tick = 0;
  
  np = mnp->children;
  for(; np!=NULL; np=np->sibling)
    if( cmTextCmp(np->label,"note") )
    {
      rc = _cmXScoreParseNote(p,meas,mnp);
    }
    else
      if( cmTextCmp(np->label,"backup") )
      {
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
    return _cmXScoreMissingNode(p,"part",&partAttr);

  // for each child of this part - find each measure
  const cmXmlNode_t* cnp = xnp->children;
  for(; cnp!=NULL; cnp=cnp->sibling)
    if( cmTextCmp(cnp->label,"measure") == 0 )
      if((rc = _cmXScoreParseMeasure(p,pp,cnp)) != kOkXsRC )
        return rc;
  
  return rc;
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

  // parse the part-list
  if((rc = _cmXScoreParsePartList( p )) != kOkXsRC )
    goto errLabel;

  cmXsPart_t* pp = p->partL;
  for(; pp!=NULL; pp=pp->link)
    if((rc = _cmXScoreParsePart(p,pp)) != kOkXsRC )
      goto errLabel;

  
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

void     cmXScoreReport( cmXsH_t h, cmRpt_t* rpt )
{
  cmXScore_t* p = _cmXScoreHandleToPtr(h);

  cmXsPart_t* pp = p->partL;
  for(; pp!=NULL; pp=pp->link)
  {
    cmRptPrintf(rpt,"Part:%s\n",pp->idStr);

    const cmXsMeas_t* meas = pp->measL;
    for(; meas!=NULL; meas=meas->link)
      cmRptPrintf(rpt,"  %i : div:%i beat:%i beat-type:%i\n",meas->number,meas->divisions,meas->beats,meas->beat_type);
  }
  
}


cmXsRC_t cmXScoreTest( cmCtx_t* ctx, const cmChar_t* fn )
{
  cmXsRC_t rc;
  cmXsH_t h = cmXsNullHandle;
  
  if((rc = cmXScoreInitialize( ctx, &h, fn)) != kOkXsRC )
    return cmErrMsg(&ctx->err,rc,"XScore alloc failed.");

  cmXScoreReport(h,&ctx->rpt);
  
  return cmXScoreFinalize(&h);

}
