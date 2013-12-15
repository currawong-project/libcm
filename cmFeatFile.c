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
#include "cmSymTbl.h"
#include "cmMath.h"
#include "cmFile.h"
#include "cmAudioFile.h"
#include "cmJson.h"
#include "cmFileSys.h"
#include "cmTime.h"
#include "cmMidi.h"
#include "cmProcObj.h"
#include "cmProcTemplateMain.h"
#include "cmProc.h"
#include "cmProc2.h"
#include "cmVectOps.h"
#include "cmFrameFile.h"
#include "cmFeatFile.h"
#include "cmSerialize.h"

#define kConstQThresh (0.0054)

enum
{
  kFrameTypeFtId   = 1,
  kFrameStreamFtId = 1,
  kStatRecdVectCnt = 8  // count of vectors in cmFtSumm_t
};

// master control record
typedef struct
{
  cmErr_t            err;
  cmCtx_t            ctx;

  cmJsonH_t          jsH;        // 
  cmSrH_t            srH;        //  file header serializer


  cmFtAttr_t*        attrArray;  //
  unsigned           attrCnt;    //

  cmFtParam_t*       paramArray; //
  unsigned           paramCnt;   //

  cmCtx*             ctxPtr;     // process context object
  cmAudioFileRd*     afRdPtr;    // audio file reader object
  cmPvAnl*           pvocPtr;    // phase vocoder object
  cmBfcc*            bfccPtr;    // BFCC generator
  cmMfcc*            mfccPtr;    // MFCC generator
  cmCeps*            cepsPtr;    // Cepstrum generator
  cmConstQ*          constqPtr;  // Const Q generator

  unsigned           progParamIdx; 
  unsigned           progPassIdx;
  unsigned           progSmpCnt;
  unsigned           progSmpIdx;


} _cmFt_t;


// static feature values
typedef struct
{
  unsigned          order;      // determines the order the feature extractors are init'd and exec'd
  const char*       label;      // feature label
  unsigned          id;         // feature id 
  unsigned          ffMtxId;    // cmFrameFile matrix type id
  unsigned          ffUnitsId;  // cmFrameFile data unit id
  unsigned          srcId;      // id of the source vector for this secondary feature  (or kInvalidFtId if no src)
  bool              bipolarFl;  // this feature is bipolar
  unsigned          maxCnt;     // maximum feature vector element count (scalar==1 no max==0)
} _cmFtLabel_t;



// analysis control record - one recd per feature
typedef struct _cmFtAttr_str
{
  cmFtAttr_t*           ap;     // user supplied feature parameters        
  const _cmFtLabel_t*   lp;     // static feature parameters
  cmFtSumm_t*           sr;     // summary record assoc'd with this feature

  struct _cmFtAttr_str* sp;     // sourcePtr (used by secondary feats to locate primary feature)
  cmReal_t*             v;      // v[ap->cnt*2] feature vector memory used by cvp and pvp
  cmReal_t*             cvp;    // current  feat vect
  cmReal_t*             pvp;    // previous feat vect
} _cmFtAnl_t;


// internal feature desc record
typedef struct
{
  const cmFtAttr_t*   ap;
  const _cmFtLabel_t* lp;    
  cmFtSumm_t*         sr;
} _cmFtDesc_t;

// internal feature file control record - file handle record
typedef struct
{
  cmFtH_t        h;          //  feat file library handle
  cmFrameFileH_t ffH;        //  handle for the frame file
  cmFtInfo_t     info;       //  file hdr recd
  _cmFtDesc_t*   descArray;  //  descArray[infoPtr->attrCnt] internal feature desc data
  void*          hdrBuf;     //  memory used to hold the serialized header
} _cmFtFile_t;


cmFtH_t     cmFtNullHandle      = { NULL };
cmFtFileH_t cmFtFileNullHandle  = { NULL };

_cmFtLabel_t _cmFtLabelArray[] =
{
  { 0,  "ampl",          kAmplFtId,       kMagMId,    kAmplUId,          kInvalidFtId,   false, 0 },
  { 1,  "db_ampl",       kDbAmplFtId,     kMagMId,    k20DbUId,          kAmplFtId,      false, 0 },
  { 2,  "pow",           kPowFtId,        kMagMId,    kPowUId,           kInvalidFtId,   false, 0 },
  { 3,  "db_pow",        kDbPowFtId,      kMagMId,    k10DbUId,          kPowFtId,       false, 0 },
  { 4,  "phase",         kPhaseFtId,      kPhsMId,    kRadsUId,          kInvalidFtId,   false, 0 },
  { 5,  "bfcc",          kBfccFtId,       kBfccMId,   kBfccUId,          kInvalidFtId,   false, kDefaultBarkBandCnt },
  { 6,  "mfcc",          kMfccFtId,       kMfccMId,   kMfccUId,          kInvalidFtId,   false, kDefaultMelBandCnt },
  { 7,  "ceps",          kCepsFtId,       kCepsMId,   kCepsUId,          kInvalidFtId,   false, 0 },
  { 8,  "constq",        kConstQFtId,     kConstqMId, kAmplUId,          kInvalidFtId,   false, 0 },
  { 9,  "log_constq",    kLogConstQFtId,  kConstqMId, k20DbUId,          kConstQFtId,    false, 0 },
  { 10, "rms",           kRmsFtId,        kRmsMId,    kAmplUId,          kInvalidFtId,   false, 1 },
  { 11, "db_rms",        kDbRmsFtId,      kRmsMId,    k20DbUId,          kRmsFtId,       false, 1 },
  { 12, "d1_ampl",       kD1AmplFtId,     kMagMId,    kAmplUId | kD1UFl, kAmplFtId,      true,  0 },  
  { 13, "d1_db_ampl",    kD1DbAmplFtId,   kMagMId,    k20DbUId | kD1UFl, kDbAmplFtId,    true,  0 },
  { 14, "d1_pow",        kD1PowFtId,      kMagMId,    kPowUId  | kD1UFl, kPowFtId,       true,  0 },
  { 15, "d1_db_pow",     kD1DbPowFtId,    kMagMId,    k10DbUId | kD1UFl, kDbPowFtId,     true,  0 },
  { 16, "d1_phase",      kD1PhaseFtId,    kPhsMId,    kRadsUId | kD1UFl, kPhaseFtId,     true,  0 },
  { 17, "d1_bfcc",       kD1BfccFtId,     kBfccMId,   kBfccUId | kD1UFl, kBfccFtId,      true,  kDefaultBarkBandCnt },
  { 18, "d1_mfcc",       kD1MfccFtId,     kMfccMId,   kMfccUId | kD1UFl, kMfccFtId,      true,  kDefaultMelBandCnt },
  { 19, "d1_ceps",       kD1CepsFtId,     kCepsMId,   kCepsUId | kD1UFl, kCepsFtId,      true,  0 },
  { 20, "d1_constq",     kD1ConstQFtId,   kConstqMId, kAmplUId | kD1UFl, kConstQFtId,    true,  0 },
  { 21, "d1_log_constq", kD1LogConstQFtId,kConstqMId, k20DbUId | kD1UFl, kLogConstQFtId, true,  0 },
  { 22, "d1_rms",        kD1RmsFtId,      kRmsMId,    kAmplUId | kD1UFl, kRmsFtId,       true,  1 },
  { 23, "d1_db_rms",     kD1DbRmsFtId,    kRmsMId,    k20DbUId | kD1UFl, kDbRmsFtId,     true,  1 },
  { 24, "<invalid>",     kInvalidFtId,    kInvalidMId,kInvalidUId,       kInvalidFtId,   true,  0 }
    
};

void _cmFtPrint( _cmFt_t* p, const char* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  cmRptVPrintf(&p->ctx.rpt,fmt,vl);
  va_end(vl);
}

cmFtRC_t _cmFtErrorV( cmFtRC_t rc, _cmFt_t* p, const char* fmt, va_list vl )
{ return cmErrVMsg(&p->err,rc,fmt,vl); }


cmFtRC_t _cmFtError( cmFtRC_t rc, _cmFt_t* p, const char* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  _cmFtErrorV(rc,p,fmt,vl);
  va_end(vl);
  return rc;
}

_cmFt_t* _cmFtHandleToPtr( cmFtH_t h )
{
  assert( h.h != NULL );
  return (_cmFt_t*)h.h;
}

_cmFtFile_t* _cmFtFileHandleToPtr( cmFtFileH_t h )
{
  assert( h.h != NULL );
  return (_cmFtFile_t*)h.h;
} 

_cmFtLabel_t* _cmFtIdToLabelPtr( unsigned id )
{
  unsigned i=0;
  for(i=0; _cmFtLabelArray[i].id != kInvalidFtId; ++i)
    if( _cmFtLabelArray[i].id == id )
      return _cmFtLabelArray + i;

  assert(0);
  return NULL;
}


enum
{
  kInfoSrFtId = kStructSrId,
  kParamSrFtId,
  kSkipSrFtId,
  kAttrSrFtId,
  kStatSrFtId,
  kHdrSrFtId,

  kSkipVSrFtId = kSkipSrFtId + kArraySrFl,
  kAttrVSrFtId = kAttrSrFtId + kArraySrFl,
  kStatVSrFtId = kStatSrFtId + kArraySrFl

};


cmFtRC_t _cmFtFormatFileHdr( _cmFt_t* p )
{
  cmFtRC_t rc = kOkFtRC;
  cmSrH_t h = p->srH;

  cmSrGetAndClearLastErrorCode(h);

  if( cmSrFmtReset( h ) != kOkSrRC )
  {
    rc = _cmFtError( kSerialFailFtRC, p, "Serializer format reset failed.");
    goto errLabel;
  }

  // cmFtSkip_t                smpIdx    smpCnt
  cmSrDefFmt( h, kSkipSrFtId, kUIntSrId, kUIntSrId, kInvalidSrId );

  // cmFtAttr_t               featId     vect cnt   normFl
  cmSrDefFmt( h, kAttrSrFtId, kUIntSrId, kUIntSrId, kBoolSrId, kInvalidSrId );
    
  // cmFtParam_t
  cmSrDefFmt( h, kParamSrFtId, 
    // audioFn     featFn      chIdx
    kCharVSrId, kCharVSrId, kUIntSrId, 
    // wndMs     hopFact  normAudFl  cqMinPitch  cqMaxPitch
    kRealSrId, kUIntSrId, kBoolSrId, kUCharSrId, kUCharSrId,
    // cqBins   minDb      skipV          attrV        
    kUIntSrId, kRealSrId, kSkipVSrFtId, kAttrVSrFtId, kInvalidSrId );
    

  // cmFtInfo_t
  cmSrDefFmt( h, kInfoSrFtId, 
    // frmCnt    srate    fftSmpCnt  hopSmpCnt  binCnt     skipFrmCnt floorFrmCnt  param
    kUIntSrId, kRealSrId, kUIntSrId, kUIntSrId, kUIntSrId, kUIntSrId, kUIntSrId, kParamSrFtId, kInvalidSrId );

  // cmFtSumm_t
  cmSrDefFmt( h, kStatSrFtId,
    // id         cnt
    kUIntSrId,  kUIntSrId,
    // raw minV    maxV       avgV       std-dev
    kRealVSrId, kRealVSrId, kRealVSrId, kRealVSrId,
    // raw min     max
    kRealSrId, kRealSrId,
    // norm minV    maxV       avgV       std-dev
    kRealVSrId, kRealVSrId, kRealVSrId, kRealVSrId,
    // raw min  max
    kRealSrId, kRealSrId, kInvalidSrId );


  // master header record     info          stat array
  cmSrDefFmt( h, kHdrSrFtId, kInfoSrFtId, kStatVSrFtId, kInvalidSrId );
 
  if( cmSrLastErrorCode(h) != kOkSrRC )
    rc = _cmFtError( kSerialFailFtRC,p, "Serializer formatting failed.");
    
   
 errLabel:
  return rc;
}


cmFtRC_t _cmFtSerializeFileHdr( _cmFt_t* p, cmFtInfo_t* f, cmFtParam_t* pp, cmFtSumm_t* summArray, void** bufPtrPtr, unsigned* bufByteCntPtr )
{
  cmFtRC_t rc = kOkFtRC;
  cmSrH_t  h  = p->srH;
  unsigned i;

  cmSrWrReset(h);

  cmSrWrStructBegin(h, kHdrSrFtId );


  // info record
  cmSrWrStruct( h, kInfoSrFtId, 1 );
  cmSrWrStructBegin(h, kInfoSrFtId );

  cmSrWrUInt( h, f->frmCnt );
  cmSrWrReal( h, f->srate );
  cmSrWrUInt( h, f->fftSmpCnt );
  cmSrWrUInt( h, f->hopSmpCnt );
  cmSrWrUInt( h, f->binCnt );
  cmSrWrUInt( h, f->skipFrmCnt );
  cmSrWrUInt( h, f->floorFrmCnt);

  // param recd
  cmSrWrStruct( h, kParamSrFtId, 1 );
  cmSrWrStructBegin(h, kParamSrFtId );
  cmSrWrCharV( h, pp->audioFn, strlen(pp->audioFn)+1);
  cmSrWrCharV( h, pp->featFn,  strlen(pp->featFn)+1);
  cmSrWrUInt(  h, pp->chIdx );
  cmSrWrReal(  h, pp->wndMs );
  cmSrWrUInt(  h, pp->hopFact);
  cmSrWrBool(  h, pp->normAudioFl);
  cmSrWrUChar( h, pp->constQMinPitch);
  cmSrWrUChar( h, pp->constQMaxPitch);
  cmSrWrUInt(  h, pp->constQBinsPerOctave);
  cmSrWrReal(  h, pp->minDb );

  // skip array
  cmSrWrStruct(h, kSkipSrFtId, pp->skipCnt );
  for(i=0; i<pp->skipCnt; ++i)
  {
    cmSrWrStructBegin(h, kSkipSrFtId );
    cmSrWrUInt( h, pp->skipArray[i].smpIdx);
    cmSrWrUInt( h, pp->skipArray[i].smpCnt);
    cmSrWrStructEnd(h);
  }

  // attr array
  cmSrWrStruct(h, kAttrSrFtId, pp->attrCnt );
  for(i=0; i<pp->attrCnt; ++i)
  {
    cmSrWrStructBegin( h, kAttrSrFtId );
    cmSrWrUInt( h, pp->attrArray[i].id );
    cmSrWrUInt( h, pp->attrArray[i].cnt );
    cmSrWrBool( h, pp->attrArray[i].normFl);
    cmSrWrStructEnd(h);
  }

  cmSrWrStructEnd(h); // end param
  cmSrWrStructEnd(h); // end info


  // write the status array
  cmSrWrStruct(h, kStatSrFtId, pp->attrCnt );

  for(i=0; i<pp->attrCnt; ++i)
  {
    assert( summArray[i].id == pp->attrArray[i].id );

    cmSrWrStructBegin(h,kStatSrFtId);

    cmSrWrUInt(  h, summArray[i].id);
    cmSrWrUInt(  h, summArray[i].cnt);

    cmSrWrRealV( h, summArray[i].rawMinV, pp->attrArray[i].cnt);
    cmSrWrRealV( h, summArray[i].rawMaxV, pp->attrArray[i].cnt);
    cmSrWrRealV( h, summArray[i].rawAvgV, pp->attrArray[i].cnt);
    cmSrWrRealV( h, summArray[i].rawSdvV, pp->attrArray[i].cnt);
    cmSrWrReal(  h, summArray[i].rawMin );
    cmSrWrReal(  h, summArray[i].rawMax );

    cmSrWrRealV( h, summArray[i].normMinV, pp->attrArray[i].cnt);
    cmSrWrRealV( h, summArray[i].normMaxV, pp->attrArray[i].cnt);
    cmSrWrRealV( h, summArray[i].normAvgV, pp->attrArray[i].cnt);
    cmSrWrRealV( h, summArray[i].normSdvV, pp->attrArray[i].cnt);
    cmSrWrReal(  h, summArray[i].normMin );
    cmSrWrReal(  h, summArray[i].normMax );
      
    cmSrWrStructEnd(h);
  }

  if( cmSrLastErrorCode(h) != kOkSrRC )
  {
    rc = _cmFtError( kSerialFailFtRC, p, "Header serialization failed.");
    goto errLabel;
  }

  if((*bufPtrPtr = cmSrWrAllocBuf(h,bufByteCntPtr)) == NULL )
  {
    rc = _cmFtError( kSerialFailFtRC, p, "Header serializer failed on write buffer allocation.");
    goto errLabel;
  }

 errLabel:
  return rc;
}

cmFtRC_t  _cmDeserializeFileHdr( _cmFt_t* p, _cmFtFile_t* fp, void* buf, unsigned bufByteCnt )
{
  cmFtRC_t     rc = kOkFtRC;
  cmSrH_t      h  = p->srH;
  unsigned     n,i;
  cmFtInfo_t*  f  = &fp->info;
  cmFtParam_t* pp = &fp->info.param;

  // do endian swap
  if( cmSrRdProcessBuffer(h, buf, bufByteCnt ) != kOkSrRC )
  {
    rc = _cmFtError( kSerialFailFtRC, p, "Deserializatoin buffer pre-process failed.");
    goto errLabel;
  }

  // duplciate the buffer - this will allow us to use memory in the buffer to hold header objects.
  fp->hdrBuf = cmMemResize( char, fp->hdrBuf, bufByteCnt );
  memcpy(fp->hdrBuf,buf,bufByteCnt);

  // setup the serializer reader
  if( cmSrRdSetup( h, fp->hdrBuf, bufByteCnt ) != kOkSrRC )
  {
    rc = _cmFtError( kSerialFailFtRC, p, "Deserialization buffer setup failed.");
    goto errLabel;
  }

  cmSrRdStructBegin(h, kHdrSrFtId );

  // info record
  cmSrReadStruct( h, kInfoSrFtId, &n ); assert(n==1);
  cmSrRdStructBegin(h, kInfoSrFtId );

  cmSrReadUInt( h, &f->frmCnt );
  cmSrReadReal( h, &f->srate );
  cmSrReadUInt( h, &f->fftSmpCnt );
  cmSrReadUInt( h, &f->hopSmpCnt );
  cmSrReadUInt( h, &f->binCnt );
  cmSrReadUInt( h, &f->skipFrmCnt );
  cmSrReadUInt( h, &f->floorFrmCnt );

  // param recd
  cmSrReadStruct( h, kParamSrFtId, &n ); assert(n==1);
  cmSrRdStructBegin(h, kParamSrFtId );
  cmSrReadCharCV(h, &pp->audioFn, &n );
  cmSrReadCharCV(h, &pp->featFn,  &n );
  cmSrReadUInt(  h, &pp->chIdx );
  cmSrReadReal(  h, &pp->wndMs );
  cmSrReadUInt(  h, &pp->hopFact);
  cmSrReadBool(  h, &pp->normAudioFl);
  cmSrReadUChar( h, &pp->constQMinPitch);
  cmSrReadUChar( h, &pp->constQMaxPitch);
  cmSrReadUInt(  h, &pp->constQBinsPerOctave);
  cmSrReadReal(  h, &pp->minDb );

  // skip array
  cmSrReadStruct(h, kSkipSrFtId, &pp->skipCnt );
  pp->skipArray = cmMemResizeZ( cmFtSkip_t, pp->skipArray, pp->skipCnt );
  for(i=0; i<pp->skipCnt; ++i)
  {
    cmSrRdStructBegin(h, kSkipSrFtId );
    cmSrReadUInt( h, &pp->skipArray[i].smpIdx);
    cmSrReadUInt( h, &pp->skipArray[i].smpCnt);
    cmSrRdStructEnd(h);
  }

  // attr array
  cmSrReadStruct(h, kAttrSrFtId, &pp->attrCnt );
  pp->attrArray = cmMemResizeZ( cmFtAttr_t, pp->attrArray, pp->attrCnt );
  for(i=0; i<pp->attrCnt; ++i)
  {
    cmSrRdStructBegin( h, kAttrSrFtId );
    cmSrReadUInt( h, &pp->attrArray[i].id );
    cmSrReadUInt( h, &pp->attrArray[i].cnt );
    cmSrReadBool( h, &pp->attrArray[i].normFl);
    cmSrRdStructEnd(h);
  }

  cmSrRdStructEnd(h); // end param
  cmSrRdStructEnd(h); // end info


  // read the status array
  cmSrReadStruct(h, kStatSrFtId, &n );
  assert( n == pp->attrCnt );

  fp->info.summArray = cmMemResizeZ( cmFtSumm_t, fp->info.summArray, pp->attrCnt );

  for(i=0; i<pp->attrCnt; ++i)
  {
    cmSrRdStructBegin(h,kStatSrFtId);

    cmSrReadUInt(  h, &fp->info.summArray[i].id);

    assert( fp->info.summArray[i].id == pp->attrArray[i].id );
    cmSrReadUInt(  h, &fp->info.summArray[i].cnt);

    cmSrReadRealV( h, &fp->info.summArray[i].rawMinV, &pp->attrArray[i].cnt);
    cmSrReadRealV( h, &fp->info.summArray[i].rawMaxV, &pp->attrArray[i].cnt);
    cmSrReadRealV( h, &fp->info.summArray[i].rawAvgV, &pp->attrArray[i].cnt);
    cmSrReadRealV( h, &fp->info.summArray[i].rawSdvV, &pp->attrArray[i].cnt);
    cmSrReadReal(  h, &fp->info.summArray[i].rawMin );
    cmSrReadReal(  h, &fp->info.summArray[i].rawMax );

    cmSrReadRealV( h, &fp->info.summArray[i].normMinV, &pp->attrArray[i].cnt);
    cmSrReadRealV( h, &fp->info.summArray[i].normMaxV, &pp->attrArray[i].cnt);
    cmSrReadRealV( h, &fp->info.summArray[i].normAvgV, &pp->attrArray[i].cnt);
    cmSrReadRealV( h, &fp->info.summArray[i].normSdvV, &pp->attrArray[i].cnt);
    cmSrReadReal(  h, &fp->info.summArray[i].normMin );
    cmSrReadReal(  h, &fp->info.summArray[i].normMax );
      
    cmSrRdStructEnd(h);
  }
  
  if( cmSrLastErrorCode(h) != kOkSrRC )
  {
    rc = _cmFtError( kSerialFailFtRC, p, "Deserialization failed.");
    goto errLabel;
  }

 errLabel:
  return rc;
}


unsigned cmFtFeatLabelToId( const char* label )
{
  unsigned i=0;
  for(i=0; _cmFtLabelArray[i].id != kInvalidFtId; ++i)
    if( strcmp(label,_cmFtLabelArray[i].label) == 0 )
      return _cmFtLabelArray[i].id;

  return kInvalidFtId;
}

const char* cmFtFeatIdToLabel( unsigned id )
{
  unsigned i=0;
  for(i=0; _cmFtLabelArray[i].id != kInvalidFtId; ++i)
    if( _cmFtLabelArray[i].id == id )
      return _cmFtLabelArray[i].label;

  return NULL;
}

cmFtRC_t  cmFtInitialize( cmFtH_t* hp, cmCtx_t* ctx )
{
  cmFtRC_t rc;

  if((rc = cmFtFinalize(hp)) != kOkFtRC )
    return rc;

  _cmFt_t* p  = cmMemAllocZ( _cmFt_t, 1 );
  cmErrSetup(&p->err,&ctx->rpt,"Feature file");
  p->ctx          = *ctx;
  p->jsH          = cmJsonNullHandle;
  p->progParamIdx = cmInvalidIdx;
  p->progPassIdx  = 0;
  p->progSmpIdx   = 0;
  p->progSmpCnt   = 0;

  // initialize the serializer
  if( cmSrAlloc(&p->srH,ctx) != kOkSrRC )
  {
    rc = _cmFtError( kSerialFailFtRC, p, "The serializer allocation failed.");
    goto errLabel;
  }

  // setup the serializer format
  if((rc = _cmFtFormatFileHdr(p)) != kOkFtRC )
    goto errLabel;

  // create the proc context object
  if((p->ctxPtr  = cmCtxAlloc(NULL,&p->ctx.rpt,cmLHeapNullHandle,cmSymTblNullHandle)) == NULL )
  {
    rc = _cmFtError(kDspProcFailFtRC,p, "The ctx compoenent allocation failed.");
    goto errLabel;
  }

  // create the audio file reader
  if((p->afRdPtr = cmAudioFileRdAlloc( p->ctxPtr, NULL, 0, NULL, cmInvalidIdx, 0, cmInvalidIdx )) == NULL )
  {
    rc =  _cmFtError( kDspProcFailFtRC, p, "The audio file reader allocation failed.");
    goto errLabel;
  }

  // create the phase vocoder 
  if((p->pvocPtr = cmPvAnlAlloc( p->ctxPtr, NULL, 0, 0, 0, 0, 0 )) == NULL )
  {
    rc = _cmFtError( kDspProcFailFtRC,p,"The phase vocoder allocation failed.");
    goto errLabel;
  }

  // create the BFCC transformer
  if((p->bfccPtr = cmBfccAlloc( p->ctxPtr, NULL, 0, 0, 0 )) == NULL )
  {
    rc = _cmFtError( kDspProcFailFtRC,p,"The BFCC generator allocation failed.");
    goto errLabel;
  }

  // create the MFCC generator
  if((p->mfccPtr = cmMfccAlloc( p->ctxPtr, NULL, 0, 0, 0, 0)) == NULL )
  {
    rc = _cmFtError( kDspProcFailFtRC,p,"The MFCC generator allocation failed.");
    goto errLabel;
  }

  // create the Cepstrum transformer
  if((p->cepsPtr = cmCepsAlloc( p->ctxPtr, NULL, 0, 0 )) == NULL )
  {
    rc = _cmFtError( kDspProcFailFtRC,p,"The Cepstrum generator allocation failed.");
    goto errLabel;
  }

  // create the Constant Q generator
  if((p->constqPtr = cmConstQAlloc( p->ctxPtr, NULL, 0, 0, 0, 0,0 )) == NULL )
  {
    rc = _cmFtError( kDspProcFailFtRC,p,"The Constant-Q generator allocation failed.");
    goto errLabel;
  }


  hp->h = p;

 errLabel:
  return rc;

}

cmFtRC_t        cmFtFinalize(   cmFtH_t* hp )
{
  cmFtRC_t rc = kOkFsRC;
  unsigned i;

  assert( hp != NULL );

  if( hp->h == NULL )
    return kOkFsRC;

  _cmFt_t* p  = _cmFtHandleToPtr(*hp);

  for(i=0; i<p->paramCnt; ++i)
    cmMemPtrFree(&p->paramArray[i].skipArray);

  cmMemPtrFree(&p->attrArray);
  p->attrCnt = 0;

  cmMemPtrFree(&p->paramArray);
  p->paramCnt = 0;

  if( cmConstQFree(&p->constqPtr) != cmOkRC )
  {
    rc = _cmFtError( kDspProcFailFtRC,p,"Constant-Q generator free failed.");
    goto errLabel;    
  }

  if( cmCepsFree(&p->cepsPtr) != cmOkRC )
  {
    rc = _cmFtError( kDspProcFailFtRC,p,"Cepstrum generator free failed.");
    goto errLabel;
  }

  if( cmMfccFree(&p->mfccPtr) != cmOkRC )
  {
    rc = _cmFtError( kDspProcFailFtRC,p,"MFCC generator free failed.");
    goto errLabel;
  }

  if( cmBfccFree(&p->bfccPtr) != cmOkRC )
  {
    rc = _cmFtError( kDspProcFailFtRC,p,"BFCC generator free failed.");
    goto errLabel;
  }

  if( cmPvAnlFree(&p->pvocPtr) != cmOkRC )
  {
    rc = _cmFtError( kDspProcFailFtRC,p,"Phase voocoder free failed.");
    goto errLabel;
  }

  if( cmAudioFileRdFree(&p->afRdPtr) != cmOkRC )
  {
    rc = _cmFtError( kDspProcFailFtRC,p,"Audio file reader failed.");
    goto errLabel;
  }

  if( cmCtxFree(&p->ctxPtr) != cmOkRC )
  {
    rc = _cmFtError( kDspProcFailFtRC,p,"Context proc failed.");
    goto errLabel;
  }

  if( cmJsonFinalize(&p->jsH) != kOkJsRC )
  {
    rc = _cmFtError(kJsonFailFtRC, p, "The JSON system object finalization failed.");
    goto errLabel;
  }

  if( cmSrFree(&p->srH) != kOkSrRC )
  {
    rc = _cmFtError(kSerialFailFtRC, p, "The serializer free failed.");
    goto errLabel;
  }


  cmMemPtrFree(&p);
  hp->h = NULL;

 errLabel:
  return rc;
}

bool cmFtIsValid( cmFtH_t h )
{ return h.h != NULL; }

cmFtRC_t        cmFtParse( cmFtH_t h, const char* cfgFn )
{
  cmFtRC_t      rc                  = kOkFtRC;
  cmJsRC_t      jsRC                = kOkJsRC;
  cmJsonNode_t* rootPtr             = NULL;
  const char*   errLabelPtr         = NULL;
  const char*   outDir              = NULL;
  cmReal_t      wndMs               = 0;
  unsigned      hopFact             = 0;
  bool          normAudioFl         = false;
  const char*   constQMinPitchStr   = NULL;
  const char*   constQMaxPitchStr   = NULL;
  unsigned      constQBinsPerOctave = 0;  
  cmMidiByte_t  constQMinPitch      = 0;
  cmMidiByte_t  constQMaxPitch      = 0;
  cmReal_t      minDb               = 0;
  cmReal_t      floorThreshDb       = 0;
  cmJsonNode_t* featArrayNodePtr    = NULL;
  cmJsonNode_t* audioFnArrayNodePtr = NULL;
  _cmFt_t*      p                   = _cmFtHandleToPtr(h);
  unsigned      i,j;
  
  assert( cfgFn != NULL );

  // parse file
  if( cmJsonInitializeFromFile( &p->jsH, cfgFn, &p->ctx ) != kOkJsRC )
  {
    rc = _cmFtError( kCfgParseFailFtRC, p, "Cfg. file parse failed on: '%s'", cfgFn );
    goto errLabel;
  }

  // get the json cfg root
  if( (rootPtr = cmJsonRoot( p->jsH )) == NULL )
  {
    rc = _cmFtError( kCfgParseFailFtRC, p, "The cfg. file '%s' appears to be empty.", cfgFn );
    goto errLabel;
  }

  // read the cfg file header
  if((jsRC = cmJsonMemberValues( rootPtr, &errLabelPtr,
        "outDir",              kStringTId, &outDir,
        "wndMs",               kRealTId,   &wndMs,
        "hopFact",             kIntTId,    &hopFact,
        "normAudioFl",         kTrueTId,   &normAudioFl,
        "constQMinPitch",      kStringTId, &constQMinPitchStr,
        "constQMaxPitch",      kStringTId, &constQMaxPitchStr,
        "constQBinsPerOctave", kIntTId,    &constQBinsPerOctave,
        "minDb",               kRealTId,   &minDb,
        "floorThreshDb",       kRealTId,   &floorThreshDb,
        "featArray",           kArrayTId,  &featArrayNodePtr,
        "audioFnArray",        kArrayTId,  &audioFnArrayNodePtr,
        NULL )) != kOkJsRC )
  {
    if( jsRC == kNodeNotFoundJsRC )
      rc = _cmFtError( kCfgParseFailFtRC, p, "Cfg. field not found:'%s' in file:'%s'.",cmStringNullGuard(errLabelPtr),cmStringNullGuard(cfgFn));
    else
      rc = _cmFtError( kCfgParseFailFtRC, p, "Cfg. header parse failed '%s'.",cmStringNullGuard(cfgFn) );

    goto errLabel;

  }

  // convert the min const-q sci pitch string to a midi pitch value
  if( (constQMinPitch = cmSciPitchToMidi( constQMinPitchStr )) == kInvalidMidiPitch )
  {
    rc = _cmFtError( kCfgParseFailFtRC, p, "The const-Q min. pitch ('%s') is invalid.", cmStringNullGuard(constQMinPitchStr));
    goto errLabel;
  }

  // convert the max const-q sci pitch string to a midi pitch value
  if( (constQMaxPitch = cmSciPitchToMidi( constQMaxPitchStr )) == kInvalidMidiPitch )
  {
    rc = _cmFtError( kCfgParseFailFtRC, p, "The const-Q max. pitch ('%s') is invalid.", cmStringNullGuard(constQMaxPitchStr));
    goto errLabel;
  }

  unsigned parseAttrCnt = cmJsonChildCount( featArrayNodePtr );
  p->attrArray = cmMemAllocZ( cmFtAttr_t, parseAttrCnt );  


  // read the attribute array
  for(i=0,j=0; i<parseAttrCnt; ++i)
  {
    const char* featLabel;

    // set default values
    p->attrArray[j].cnt      = 0;
    p->attrArray[j].enableFl = true;

    if((jsRC = cmJsonMemberValues( cmJsonArrayElement(featArrayNodePtr,i), &errLabelPtr,
          "feat",     kStringTId,             &featLabel,
          "cnt",      kIntTId  | kOptArgJsFl, &p->attrArray[j].cnt,
          "normFl",   kTrueTId,               &p->attrArray[j].normFl,
          "enableFl", kTrueTId | kOptArgJsFl, &p->attrArray[j].enableFl,
          NULL )) != kOkJsRC )
    {
      if( jsRC == kNodeNotFoundJsRC )
        rc = _cmFtError( kCfgParseFailFtRC, p, "Cfg. feature attribute field:'%s' not found at index %i in file:'%s'.",cmStringNullGuard(errLabelPtr),i,cmStringNullGuard(cfgFn));
      else
        rc = _cmFtError( kCfgParseFailFtRC, p, "Cfg. feature attribute parse failed at index %i in '%s'.",i,cmStringNullGuard(cfgFn) );

      goto errLabel;
    }

    if( p->attrArray[j].enableFl )
    {

      // convert the feature label to an id
      if( (p->attrArray[j].id = cmFtFeatLabelToId( featLabel)) == kInvalidFtId )
      {
        rc = _cmFtError( kCfgParseFailFtRC, p, "Cfg. feature '%s' was not found at featArray index %i in '%s'.", featLabel, i, cmStringNullGuard(cfgFn));
        goto errLabel;
      }

      ++j;
    }
        
  }

  p->attrCnt    = j;

  p->paramCnt   = cmJsonChildCount( audioFnArrayNodePtr );
  p->paramArray = cmMemAllocZ( cmFtParam_t, p->paramCnt );

  // read the audio file array
  for(i=0; i<p->paramCnt; ++i)
  {

    cmJsonNode_t* skipArrayNodePtr = NULL;

    // read the audio file read
    if((jsRC = cmJsonMemberValues( cmJsonArrayElement(audioFnArrayNodePtr,i), &errLabelPtr,
          "audioFn",  kStringTId, &p->paramArray[i].audioFn,
          "featFn",   kStringTId, &p->paramArray[i].featFn,
          "skipArray",kArrayTId | kOptArgJsFl,  &skipArrayNodePtr,
          "chIdx",    kIntTId,    &p->paramArray[i].chIdx,
          NULL)) != kOkJsRC )
    {
      if( jsRC == kNodeNotFoundJsRC )
        rc = _cmFtError( kCfgParseFailFtRC, p, "Cfg. audio file field :'%s' not found at index %i in file:'%s'.",cmStringNullGuard(errLabelPtr),i,cmStringNullGuard(cfgFn));
      else
        rc = _cmFtError( kCfgParseFailFtRC, p, "Cfg. audio file parse failed at index %i in '%s'.",i,cmStringNullGuard(cfgFn) );

      goto errLabel;
    }

    p->paramArray[i].wndMs               = wndMs;
    p->paramArray[i].hopFact             = hopFact;
    p->paramArray[i].normAudioFl         = normAudioFl;
    p->paramArray[i].constQBinsPerOctave = constQBinsPerOctave;
    p->paramArray[i].constQMinPitch      = constQMinPitch;
    p->paramArray[i].constQMaxPitch      = constQMaxPitch;
    p->paramArray[i].minDb               = minDb;
    p->paramArray[i].floorThreshDb       = floorThreshDb;
    p->paramArray[i].attrArray           = p->attrArray;
    p->paramArray[i].attrCnt             = p->attrCnt;
    p->paramArray[i].skipCnt             = skipArrayNodePtr==NULL ? 0    : cmJsonChildCount( skipArrayNodePtr );
    p->paramArray[i].skipArray           = skipArrayNodePtr==NULL ? NULL : cmMemAllocZ( cmFtSkip_t, p->paramArray[i].skipCnt );
    
    
    // read the skip array in the audio file recd
    for(j=0; j<p->paramArray[i].skipCnt; ++j)
    {
      if((jsRC = cmJsonMemberValues( cmJsonArrayElement(skipArrayNodePtr,j), &errLabelPtr,
            "smpIdx",  kIntTId, &p->paramArray[i].skipArray[j].smpIdx,
            "smpCnt",  kIntTId, &p->paramArray[i].skipArray[j].smpCnt,
            NULL)) != kOkJsRC )
      {
        if( jsRC == kNodeNotFoundJsRC )
          rc = _cmFtError( kCfgParseFailFtRC, p, "Cfg. audio file skip field '%s' not found at index %i in file:'%s'.",cmStringNullGuard(errLabelPtr),j,cmStringNullGuard(cfgFn));
        else
          rc = _cmFtError( kCfgParseFailFtRC, p, "Cfg. audio file skip parse failed at index %i in '%s'.",j, cmStringNullGuard(cfgFn) );

        goto errLabel;
      }      

    }

    // if the audio file does not exist
    if( cmFsIsFile( p->paramArray[i].audioFn ) == false )
    {
      rc = _cmFtError( kFileNotFoundFtRC, p, "The audio file '%s' was not found.", p->paramArray[i].audioFn );
      goto errLabel;
    }

    // form the feature file name for this file
    if((p->paramArray[i].featFn = cmFsMakeFn( outDir, p->paramArray[i].featFn, NULL, NULL )) == NULL )
    {
      rc = _cmFtError( kFileSysFailFtRC, p, "The attempt to create the feature file name for '%s' failed.", cmStringNullGuard(p->paramArray[i].featFn));
      goto errLabel;
    }


  }
  
  // if the output directory does not exist then create it
  if( cmFsIsDir(outDir) == false )
    if( cmFsMkDir(outDir) != kOkFsRC )
    {
      rc = _cmFtError( kDirCreateFailFtRC, p, "The attempt to create the output directory '%s' failed.",outDir);
      goto errLabel;
    }


 errLabel:

    
  return rc;
  
}

bool _cmFtZeroSkipSamples( const cmFtParam_t* pp, cmSample_t* v, unsigned vn, unsigned begSmpIdx )
{
  unsigned          endSmpIdx = begSmpIdx + vn - 1;  
  bool              retFl     = false;
  unsigned          i         = 0;
  const cmFtSkip_t* sp        = pp->skipArray;
  
  // for each skipArray[] record
  for(i=0;  i<pp->skipCnt; ++sp,++i)
    if( sp->smpCnt !=  0 )
    {
      unsigned bi = 0; 
      unsigned ei = vn-1;

      unsigned sp_endIdx;

      // if sp->smpCnt is negative then skip to end of file
      if( sp->smpCnt == -1 )
        sp_endIdx = endSmpIdx;
      else
        sp_endIdx = sp->smpIdx + sp->smpCnt - 1;
      

      // begSmpIdx:endSmpIdx  indicate the index range of v[] 
      // sp->smpIdx:sp_endIdx indicate the skip index range


      // if the skip range is entirely before or after v[]
      if( sp_endIdx < begSmpIdx || sp->smpIdx > endSmpIdx )
        continue;

      // if sp->smpIdx is inside v[]
      if( sp->smpIdx > begSmpIdx )
        bi = sp->smpIdx - begSmpIdx;

      // if sp_endIdx is inside v[]
      if( sp_endIdx < endSmpIdx )
      {
        assert( endSmpIdx - sp_endIdx <= ei );
        ei -= endSmpIdx - sp_endIdx;
      }

      assert( bi <= ei );
      assert( bi < vn && ei < vn );

      // zero the samples which are inside the skip range
      cmVOS_Zero(v+bi,(ei-bi)+1);
      retFl = true;      
    }
  
  return retFl;
}


cmFtRC_t _cmFtProcInit( _cmFt_t* p, cmFtInfo_t* f, cmFtParam_t* pp, _cmFtAnl_t* anlArray )
{
  cmFtRC_t rc = kOkFtRC;
  unsigned i;


  // initialize the phase vocoder 
  if( cmPvAnlInit( p->pvocPtr, f->hopSmpCnt, f->srate, f->fftSmpCnt, f->hopSmpCnt, kNoCalcHzPvaFl ) != cmOkRC )
  {
    rc = _cmFtError(kDspProcFailFtRC,p," The phase vocoder initialization failed.");
    goto errLabel;
  }

  assert( f->binCnt == p->pvocPtr->binCnt ); 

  cmReal_t binHz = f->srate / f->fftSmpCnt;

  // initialize each requested feature extractor 
  for(i=0; i<pp->attrCnt; ++i)
  {
    _cmFtAnl_t*   a  = anlArray + i;

    assert( a->lp != NULL );

    switch( a->ap->id )
    {
      case kAmplFtId:
      case kDbAmplFtId:
      case kPowFtId:
      case kDbPowFtId:
      case kPhaseFtId:
        if( a->ap->cnt > f->binCnt )
        {
          rc = _cmFtError(kParamRangeFtRC,p,"The '%s' cnt value: %i must be less than the bin count: %i.",a->lp->label,a->ap->cnt,f->binCnt+1);
          goto errLabel;
        }

        if( a->ap->cnt == 0 )
          a->ap->cnt = f->binCnt;

        break;

      case kBfccFtId: // initialize the BFCC generator
        if( a->ap->cnt > kDefaultBarkBandCnt )
        {
          rc = _cmFtError(kParamRangeFtRC,p,"The BFCC feature vector length (%i) must be less than (%i).", a->ap->cnt, kDefaultBarkBandCnt+1 );
          goto errLabel;
        }

        if( a->ap->cnt == 0 )
          a->ap->cnt = kDefaultBarkBandCnt;
        
        if( cmBfccInit( p->bfccPtr, kDefaultBarkBandCnt, p->pvocPtr->binCnt, binHz ) != cmOkRC )
        {
          rc = _cmFtError(kDspProcFailFtRC,p," The BFCC generator initialization failed.");
          goto errLabel;
        }
        break;

      case kMfccFtId: // initialize the MFCC generator
        if( a->ap->cnt > kDefaultMelBandCnt )
        {
          rc = _cmFtError(kParamRangeFtRC,p,"The MFCC feature vector length (%i) must be less than (%i).", a->ap->cnt, kDefaultMelBandCnt+1 );
          goto errLabel;
        }

        if( a->ap->cnt == 0 )
          a->ap->cnt = kDefaultMelBandCnt;
        
        if( cmMfccInit( p->mfccPtr, f->srate, kDefaultMelBandCnt, a->ap->cnt, p->pvocPtr->binCnt ) != cmOkRC )
        {
          rc = _cmFtError(kDspProcFailFtRC,p," The MFCC generator initialization failed.");
          goto errLabel;
        }
        break;

      case kCepsFtId: // initialize the cepstrum generator

        if( a->ap->cnt > f->binCnt )
        {
          rc = _cmFtError(kParamRangeFtRC,p,"The '%s' cnt value: %i must be less than the bin count: %i.",a->lp->label,a->ap->cnt,f->binCnt+1);
          goto errLabel;
        }

        if( a->ap->cnt == 0 )
          a->ap->cnt = f->binCnt;
        
        if( cmCepsInit( p->cepsPtr, p->pvocPtr->binCnt, a->ap->cnt ) != cmOkRC )
        {
          rc = _cmFtError(kDspProcFailFtRC,p," The Cepstrum generator initialization failed.");
          goto errLabel;
        }
        break;

      case kConstQFtId:   // initialize the constant Q generator
      case kLogConstQFtId:

        if( cmConstQInit(p->constqPtr, f->srate, pp->constQMinPitch, pp->constQMaxPitch, pp->constQBinsPerOctave, kConstQThresh ) != cmOkRC )
        {
          rc = _cmFtError(kDspProcFailFtRC,p,"The constant-q generator initialization failed.");
          goto errLabel;
        }

        if( a->ap->cnt > p->constqPtr->constQBinCnt )
        {
          rc = _cmFtError(kParamRangeFtRC,p,"The '%s' cnt value: %i must be less than the bin count: %i.",a->lp->label,a->ap->cnt,p->constqPtr->constQBinCnt+1);
          goto errLabel;
        }

        if( a->ap->cnt == 0 )
          a->ap->cnt = p->constqPtr->constQBinCnt;
        break;

      case kRmsFtId:
      case kDbRmsFtId:
        a->ap->cnt = 1;  // scalars must have a cnt == 1
        break;

      case kD1AmplFtId:
      case kD1DbAmplFtId:
      case kD1PowFtId:
      case kD1DbPowFtId:
      case kD1PhaseFtId:
      case kD1BfccFtId:
      case kD1MfccFtId:
      case kD1CepsFtId:
      case kD1ConstQFtId:
      case kD1LogConstQFtId:
        if( a->ap->cnt == 0 )
          a->ap->cnt = a->sp->ap->cnt;
        break;

      case kD1RmsFtId:
      case kD1DbRmsFtId:
        a->ap->cnt = 1;
        break;
        
      default:
        { assert(0); }

    } // end switch


    // setup the feature label record and allocate the feature vector
    if( a->ap->cnt )
    {
      // 2==cvp and pvp + kStatRecdVectCnt==count of summary vectors
      unsigned nn    = a->ap->cnt * (2 + kStatRecdVectCnt);
      unsigned n     = 0;

      assert(a->v == NULL);
      a->v               = cmMemAllocZ( cmReal_t, nn );
      a->cvp             = a->v + n; n += a->ap->cnt;
      a->pvp             = a->v + n; n += a->ap->cnt;

      a->sr->cnt         = a->ap->cnt;

      a->sr->rawMinV     = a->v + n; n += a->ap->cnt;
      a->sr->rawMaxV     = a->v + n; n += a->ap->cnt;
      a->sr->rawAvgV     = a->v + n; n += a->ap->cnt;
      a->sr->rawSdvV     = a->v + n; n += a->ap->cnt;
      a->sr->rawMin      = cmReal_MAX;
      a->sr->rawMax      = -cmReal_MAX;
      cmVOR_Fill( a->sr->rawMinV, a->ap->cnt,  cmReal_MAX );
      cmVOR_Fill( a->sr->rawMaxV, a->ap->cnt, -cmReal_MAX );

      a->sr->normMinV    = a->v + n; n += a->ap->cnt;
      a->sr->normMaxV    = a->v + n; n += a->ap->cnt;
      a->sr->normAvgV    = a->v + n; n += a->ap->cnt;
      a->sr->normSdvV    = a->v + n; n += a->ap->cnt;
      a->sr->normMin     = cmReal_MAX;
      a->sr->normMax     = -cmReal_MAX;
      cmVOR_Fill( a->sr->normMinV, a->ap->cnt,  cmReal_MAX );
      cmVOR_Fill( a->sr->normMaxV, a->ap->cnt, -cmReal_MAX );
      
      assert(n == nn);
    }

    if( a->sp != NULL )
    {
      if( a->sp->ap->cnt > a->ap->cnt )
      {
        rc = _cmFtError( kParamRangeFtRC,p,"The feature element count '%i' for '%s' is greater than the source vector '%s' '%i'.", a->ap->cnt, a->lp->label, a->sp->lp->label, a->sp->ap->cnt );
        goto errLabel;
      }
    }

  }  // end for
 
 errLabel:
  return rc;
}

cmFtRC_t _cmFtProcExec( _cmFt_t* p, cmFtInfo_t* f, cmFtParam_t* pp, cmFrameFileH_t ffH, _cmFtAnl_t* anlArray, const cmSample_t* audV )
{
  cmFtRC_t rc = kOkFtRC;
  unsigned i;


  for(i=0; i < pp->attrCnt; ++i)
    {
      _cmFtAnl_t*     a    = anlArray + i;

      // swap current and previous pointer
      cmReal_t* tp = a->cvp; 
      a->cvp       = a->pvp;
      a->pvp       = tp;

      switch( a->lp->id )
      {
        case kAmplFtId:
          cmVOR_Copy(a->cvp, a->ap->cnt, p->pvocPtr->magV );
          break;

        case kDbAmplFtId:          
          cmVOR_AmplToDbVV( a->cvp, a->ap->cnt, p->pvocPtr->magV, pp->minDb );
          break;

        case kPowFtId:
          cmVOR_PowVVS( a->cvp, a->ap->cnt, p->pvocPtr->magV, 2.0 );
          break;

        case kDbPowFtId:
          cmVOR_PowToDbVV( a->cvp, a->ap->cnt, a->sp->cvp, pp->minDb );
          break;

        case kPhaseFtId:
          cmVOR_Copy( a->cvp, a->ap->cnt, p->pvocPtr->phsV );
          break;

        case kBfccFtId:
          {
            cmBfccExec( p->bfccPtr, p->pvocPtr->magV, p->pvocPtr->binCnt );
            cmVOR_Copy(a->cvp, a->ap->cnt, p->bfccPtr->outV );
          }
          break;

        case kMfccFtId:
          {
            cmMfccExecAmplitude( p->mfccPtr, p->pvocPtr->magV, p->pvocPtr->binCnt );
            cmVOR_Copy( a->cvp, a->ap->cnt, p->mfccPtr->outV );
          }
          break;

        case kCepsFtId:
          {
            cmCepsExec( p->cepsPtr, p->pvocPtr->magV, p->pvocPtr->phsV, p->pvocPtr->binCnt );
            cmVOR_Copy(a->cvp, a->ap->cnt, p->cepsPtr->outV );
          }
          break;

        case kConstQFtId:
          {
            // convert from float complex to double complex
            cmComplexR_t tmp0[ p->pvocPtr->binCnt ];
            unsigned j;
            for(j=0; j<p->pvocPtr->binCnt; ++j)
              tmp0[j] = p->pvocPtr->ft.complexV[j];

            cmConstQExec( p->constqPtr, tmp0, p->pvocPtr->binCnt );
            cmVOR_Copy( a->cvp, a->ap->cnt, p->constqPtr->magV );
          }
          break;

        case kLogConstQFtId:
          cmVOR_LogV( a->cvp, a->ap->cnt, p->constqPtr->magV );
          break;

        case kRmsFtId:
          a->cvp[0]  = cmVOS_RMS( audV, p->afRdPtr->outN, p->afRdPtr->outN ); 
          break;

        case kDbRmsFtId:
          cmVOR_AmplToDbVV( a->cvp, 1, a->sp->cvp, pp->minDb );
          break;

        case kD1AmplFtId:
        case kD1DbAmplFtId:
        case kD1PowFtId:
        case kD1DbPowFtId:
        case kD1PhaseFtId:
        case kD1BfccFtId:
        case kD1MfccFtId:
        case kD1CepsFtId:
        case kD1ConstQFtId:
        case kD1LogConstQFtId:
        case kD1RmsFtId:
        case kD1DbRmsFtId:
          cmVOR_SubVVV( a->cvp, a->ap->cnt, a->sp->pvp, a->sp->cvp );
          break;

        default:
          assert(0);
          break;

      } // end switch

      if( cmFrameFileWriteMtxReal(  ffH, a->lp->ffMtxId, a->lp->ffUnitsId, a->cvp, a->ap->cnt, 1 ) != kOkFfRC )
      {
        rc = _cmFtError( kFrameWriteFailFtRC, p, "Matrix write failed (feature:%s size:%i).",a->lp->label,a->ap->cnt);
        goto errLabel;
      }


    }

 errLabel:
  return rc;
  
}

unsigned _cmFtWriteField( char* buf, unsigned bufByteCnt, unsigned bufIdx, const void* s, unsigned srcByteCnt )
{
  assert( bufIdx + srcByteCnt <= bufByteCnt );
  memcpy(buf+bufIdx,s,srcByteCnt);
  return bufIdx + srcByteCnt;
}



cmFtRC_t _cmFtWriteFileHdr( _cmFt_t* p, cmFtInfo_t* f, cmFtParam_t* pp, cmFrameFileH_t ffH, cmFtSumm_t* summArray, bool updateFl )
{
  cmFtRC_t rc              = kOkFtRC;
  void*    buf;
  unsigned bufByteCnt;

  // serialize the file header
  if((rc = _cmFtSerializeFileHdr(p,f,pp,summArray,&buf,&bufByteCnt)) != kOkFtRC )
    goto errLabel;

  if( updateFl )
  {
    const cmFfMtx_t* mp = NULL;
    void* hdrPtr        = NULL;
    if( (hdrPtr = cmFrameFileMtxBlob(ffH, kDataMId, kNoUnitsUId, &mp )) == NULL )
    {
      rc = _cmFtError( kFrameFileFailFtRC, p, "Frame file header read before update failed.");
      goto errLabel;
    }

    assert( mp->rowCnt == bufByteCnt );
    memcpy( hdrPtr, buf, bufByteCnt );
  }
  else
  {
    if( cmFrameFileWriteMtxBlob( ffH, kDataMId, kNoUnitsUId, buf, bufByteCnt, 1 ) != kOkFfRC )
    {
      rc = _cmFtError( kFrameWriteFailFtRC, p, "Header write failed.");
      goto errLabel;
    }
  }

 errLabel:
  return rc;
  
}



// Interface to the _cmFtProcFile() user programmable process function.
// This function is called once per each feature vector in the feature file.
// v[fn] points to the feature file.
// Return true if the feature has been modified and should be written back to disk.
  typedef bool (*_cmFtProcFunc_t)( _cmFt_t* p, _cmFtAnl_t* a, cmReal_t* v, unsigned vn );

// Iterate through each frame and each frame matrix call procFunc().
cmFtRC_t _cmFtProcFile( _cmFt_t* p, cmFrameFileH_t ffH, cmFtParam_t* pp, _cmFtAnl_t* anlArray, _cmFtProcFunc_t procFunc )
{
  cmFtRC_t rc       = kOkFtRC;
  cmFfRC_t ffRC     = kOkFfRC;
  unsigned i,j;

  ++p->progPassIdx;
  p->progSmpIdx = 0;
  p->progSmpCnt = cmFrameFileDesc( ffH )->frameCnt;

  // rewind the frame file
  if( cmFrameFileRewind( ffH ) != kOkFfRC )
  {
    rc = _cmFtError(kFrameFileFailFtRC,p,"Normalize rewind failed on '%s'.", cmStringNullGuard(pp->featFn));
    goto errLabel;
  }

  // load the next data frame
  for(i=0; (ffRC=cmFrameFileFrameLoadNext(ffH,kFrameTypeFtId,kFrameStreamFtId,NULL)) == kOkFfRC; ++i,++p->progSmpIdx)
  {
    bool updateFl = false;

    // for each feature matrix
    for(j=0; j<pp->attrCnt; ++j)
    {
      unsigned         dn;
      cmReal_t*        dp;
      const cmFfMtx_t* mtxDescPtr = NULL;
      _cmFtAnl_t*      a          = anlArray + j;

      // get a pointer to the matrix data
      if((dp = cmFrameFileMtxReal( ffH, a->lp->ffMtxId, a->lp->ffUnitsId, &mtxDescPtr )) == NULL )
      {
        rc = _cmFtError(kFrameFileFailFtRC,p,"Data access failed during post processing on feature:'%s' in '%s'.", a->lp->label,cmStringNullGuard(pp->featFn));
        goto errLabel;
      }

      // get the lenth of the feature vector
      dn = mtxDescPtr->rowCnt*mtxDescPtr->colCnt;

      // processes this feature 
      if( procFunc(p,a,dp,dn) )
        updateFl = true;
    }    

    // write the frame back to disk
    if( updateFl )
      if( cmFrameFileFrameUpdate(ffH) != kOkFfRC )
      {
        rc = _cmFtError(kFrameFileFailFtRC,p,"Post procssing failed on record index %i in '%s'.", i, cmStringNullGuard(pp->featFn));
        goto errLabel;
      }
  }
  
  if( ffRC != kEofFfRC && ffRC != kOkFfRC )
  {
    rc = _cmFtError( kFrameFileFailFtRC,p,"Post processing iterationg failed on record index %i in '%s'.",i,cmStringNullGuard(pp->featFn));
    goto errLabel;
  }

 errLabel:
  return rc;
}


// Sum the feature vector into a->sr->rawAvg and track the global min/max value.
bool _cmFtProcRawMinMaxSum( _cmFt_t* p, _cmFtAnl_t* a, cmReal_t* v, unsigned vn )
{
  assert( vn == a->ap->cnt );

  cmVOR_AddVV( a->sr->rawAvgV, vn, v );                      // track vector sum for use in avg
 
  cmVOR_MinVV( a->sr->rawMinV, vn, v );                      // track min/max per vector dim
  cmVOR_MaxVV( a->sr->rawMaxV, vn, v );

  a->sr->rawMin = cmMin(a->sr->rawMin, cmVOR_Min(v,vn,1));   // track global min/max
  a->sr->rawMax = cmMax(a->sr->rawMax, cmVOR_Max(v,vn,1));
  return false;
}

// Sum the the squared diff. between feature value and feature avg into rawSdvV[]
bool  _cmFtProcRawStdDev( _cmFt_t* p, _cmFtAnl_t* a, cmReal_t* v, unsigned vn )
{
  cmReal_t t[ vn ];
  assert( vn == a->ap->cnt );

  cmVOR_SubVVV( t,              a->ap->cnt, v, a->sr->rawAvgV ); 
  cmVOR_PowVS(  t,              a->ap->cnt, 2.0 );
  cmVOR_AddVV(  a->sr->rawSdvV, a->ap->cnt, t );

  return false;
}

bool _cmFtProcNormMinMaxSum( _cmFt_t* p, _cmFtAnl_t* a, cmReal_t* v, unsigned vn )
{
  assert( a->ap->cnt == vn );

  if( a->ap->normFl == false ) 
  {
    cmVOR_Zero( a->sr->normMaxV, vn );
    cmVOR_Zero( a->sr->normMinV, vn );
    a->sr->normMin = 0;
    a->sr->normMax = 0;
  }
  else
  {

    if( a->lp->bipolarFl )
    {
      // subtract mean and divide by std-dev
      cmVOR_SubVV(v, vn, a->sr->rawAvgV );
      cmVOR_DivVVZ(v, vn, a->sr->rawSdvV );
    }
    else
    {
      // scale  feature into unit range based on file wide min/max
      cmVOR_SubVS(v, vn, a->sr->rawMin );

      if( a->sr->rawMax - a->sr->rawMin > 0 )
        cmVOR_DivVS(v, vn, a->sr->rawMax - a->sr->rawMin );
      else
        cmVOR_Zero(v,vn);

      // convert to unit total energy (UTE) 
      // (this makes the vector sum to one (like a prob. distrib))
      if( vn > 1 )
      {
        cmReal_t sum = cmVOR_Sum(v, vn );
        if( sum > 0 )
          cmVOR_DivVS(v, vn, sum );
        else
          cmVOR_Zero(v,vn);
      }

    }

    cmVOR_AddVV( a->sr->normAvgV, a->ap->cnt, v );              // track norm sum
    cmVOR_MinVV( a->sr->normMinV, vn, v );                      // track norm min/max per dim
    cmVOR_MaxVV( a->sr->normMaxV, vn, v );
    a->sr->normMin = cmMin(a->sr->normMin, cmVOR_Min(v,vn,1));  // track norm global min/max
    a->sr->normMax = cmMax(a->sr->normMax, cmVOR_Max(v,vn,1));
        
    return true;
  }

  return false;
}

// calc squared diff into a->sr->normSdv[]
bool  _cmFtNormStdDev( _cmFt_t* p, _cmFtAnl_t* a, cmReal_t* v, unsigned vn )
{
  if( a->ap->normFl )
  {
    assert( a->ap->cnt == vn );
    cmReal_t t[vn];

    cmVOR_SubVVV( t,               a->ap->cnt, v, a->sr->normAvgV ); 
    cmVOR_PowVS(  t,               a->ap->cnt, 2.0 );
    cmVOR_AddVV(  a->sr->normSdvV, a->ap->cnt, t );
  }

  return false;
}

// anlArray[] sorting function
int _cmFtAnlCompare( const void* pp0, const void* pp1 )
{ 
  const _cmFtAnl_t* p0 = (const _cmFtAnl_t*)pp0;
  const _cmFtAnl_t* p1 = (const _cmFtAnl_t*)pp1;

  assert( p0 != NULL && p0->lp !=NULL && p1!=NULL && p1->lp != NULL );
  return p0->lp->order - p1->lp->order; 
}


cmFtRC_t   _cmFtValidateAttrArray( _cmFt_t* p )
{
  cmFtRC_t rc = kOkFtRC;
  unsigned i,j;

  for(i=0; i<p->attrCnt; ++i)
  {
    _cmFtLabel_t* lp = _cmFtIdToLabelPtr(p->attrArray[i].id);

    assert( lp != NULL );

    // check for duplicate features
    for(j=0; j<p->attrCnt; ++j)
      if( i!=j && p->attrArray[i].id == p->attrArray[j].id )
      {
        rc = _cmFtError( kParamErrorFtRC, p, "The attribute '%s' has duplicate entries in the attribute array.", cmStringNullGuard(lp->label));
        goto errLabel;
      }

    // verify that the source id for this secondary feature was specified
    if( lp->srcId != kInvalidFtId )
    {
      for(j=0; j<p->attrCnt; ++j)
        if( p->attrArray[j].id == lp->srcId )
          break;

      if( j == p->attrCnt )
      {
        rc = _cmFtError( kParamErrorFtRC, p, "The primary feature '%s' must be specified in order to use the secondary feature '%s'.",cmStringNullGuard(_cmFtIdToLabelPtr(lp->srcId)->label),lp->label);
        goto errLabel;
      }              
    }
  }

 errLabel:
  return rc;
}

cmFtRC_t        cmFtAnalyzeFile( cmFtH_t h, cmFtParam_t* pp)
{
  cmFtRC_t        rc        = kOkFtRC;
  _cmFt_t*        p         = _cmFtHandleToPtr(h);
  cmSample_t      minSmp,maxSmp,meanSmp;
  cmReal_t        audioSigNormFact;
  cmFtInfo_t      f;
  unsigned        frameIdx,sampleIdx;
  cmFrameFileH_t  ffH       = cmFrameFileNullHandle;
  cmAudioFileInfo_t afInfo;
  _cmFtAnl_t*     anlArray  = NULL; 
  cmFtSumm_t*     summArray = NULL;
  unsigned        i;
  cmReal_t        floorThreshAmpl;

  if((rc = _cmFtValidateAttrArray(p)) != kOkFtRC )
    goto errLabel;

  cmVOR_DbToAmplVV(&floorThreshAmpl,1,&pp->floorThreshDb);

  // get the audio file header information
  if( cmAudioFileGetInfo(pp->audioFn, &afInfo, &p->ctx.rpt ) != kOkAfRC )
  {
    rc =  _cmFtError(kDspProcFailFtRC,p, "The audio file open failed on '%s'.",cmStringNullGuard(pp->audioFn));
    goto errLabel;
  }

  p->progSmpCnt     = afInfo.frameCnt;
  p->progSmpIdx     = 0;
  f.srate           = afInfo.srate;
  f.smpCnt          = afInfo.frameCnt;
  f.fftSmpCnt       = cmNearPowerOfTwo( (unsigned)floor( pp->wndMs * f.srate / 1000 ) );
  f.binCnt          = f.fftSmpCnt / 2 + 1;
  f.hopSmpCnt       = f.fftSmpCnt / pp->hopFact;
  f.frmCnt          = 0;
  f.skipFrmCnt      = 0;
  f.floorFrmCnt     = 0;


  // verify that the audio channel index is valid
  if( pp->chIdx >= afInfo.chCnt )
  {
    rc = _cmFtError(kChIdxInvalidFtRC,p,"The channel index (%i) specified for audio file '%s' is greater than the audio file channel count.",pp->chIdx,pp->audioFn,afInfo.chCnt );
    goto errLabel;
  }
  
  // initialize the audio file reader
  if( cmAudioFileRdOpen( p->afRdPtr, f.hopSmpCnt, pp->audioFn, pp->chIdx, 0, cmInvalidIdx ) != cmOkRC )
  {
    rc =  _cmFtError(kDspProcFailFtRC,p, "The audio file reader open failed.");
    goto errLabel;
  }

  // get the range of sample values from this audio file for later normalization
  if( cmAudioFileRdMinMaxMean( p->afRdPtr, pp->chIdx, &minSmp, &maxSmp, &meanSmp ) != cmOkRC )
  {
    rc = _cmFtError(kDspProcFailFtRC,p,"Audio file min/max/mean processing failed on the audio file:'%s'.",cmStringNullGuard(pp->audioFn));
    goto errLabel;
  }

  audioSigNormFact = cmMax( fabs(minSmp), fabs(maxSmp) );

  // allocate anlArray[]
  anlArray  = cmMemAllocZ(  _cmFtAnl_t,  pp->attrCnt );
  summArray = cmMemAllocZ( cmFtSumm_t, pp->attrCnt );

  // iniitalize anlArray[]
  for(i=0; i<pp->attrCnt; ++i)
  {
    _cmFtAnl_t*  a  = anlArray + i;

    a->ap           = pp->attrArray + i;
    a->lp           = _cmFtIdToLabelPtr(a->ap->id);
    a->sr           = summArray + i;
    a->sr->id       = a->lp->id;
  }

  // sort anlArray[] into init and exec order
  qsort( anlArray, pp->attrCnt, sizeof(anlArray[0]), _cmFtAnlCompare);

  // set the anlArray[i] source attribute pointer for secondary features (feat's based on other feat's) 
  for(i=0; i<pp->attrCnt; ++i)
    if( anlArray[i].lp->srcId != kInvalidFtId )
    {
      unsigned j;
      for(j=0; j<pp->attrCnt; ++j)
        if( i!=j && anlArray[j].lp->id == anlArray[i].lp->srcId )
        {
          anlArray[i].sp = anlArray + j;
          break;
        }

      assert( j != pp->attrCnt );
    }


  // initialize the feature extractors and allocate feature vector memory
  if((rc = _cmFtProcInit(p, &f, pp, anlArray)) != kOkFtRC )
    goto errLabel;

  // create the output frame file
  if( cmFrameFileCreate(&ffH, pp->featFn, f.srate, &p->ctx ) != kOkFfRC )
  {
    rc = _cmFtError( kFrameFileFailFtRC, p, "The feature file '%s' could not be created.",cmStringNullGuard(pp->featFn));
    goto errLabel;
  }
  
  // read the next block of samples from the audio file
  for(frameIdx=0,sampleIdx=0; cmAudioFileRdRead(p->afRdPtr) != cmEofRC; sampleIdx+=f.hopSmpCnt )
  {
    cmSample_t   aV[ p->afRdPtr->outN ];
    cmSample_t*  audV = aV;
    cmSample_t   rms;

    p->progSmpIdx = sampleIdx;

    // if this audio buffer is fully or paritally marked as 'skip'
    if( _cmFtZeroSkipSamples( pp, p->afRdPtr->outV, p->afRdPtr->outN, p->afRdPtr->curFrmIdx - p->afRdPtr->lastReadFrmCnt ) )
    {
      ++f.skipFrmCnt;
      continue;
    }

    // if the audio buffer is zero - skip it
    if((rms  = cmVOS_RMS( p->afRdPtr->outV, p->afRdPtr->outN, p->afRdPtr->outN )) < floorThreshAmpl )
    {
      ++f.floorFrmCnt;
      continue;
    }

    // normalize the audio
    if( pp->normAudioFl )
      cmVOS_MultVVS( audV, p->afRdPtr->outN,   p->afRdPtr->outV, audioSigNormFact );
    else
      audV = p->afRdPtr->outV;

    // execute the phase vocoder
    if( cmPvAnlExec(p->pvocPtr, audV, p->afRdPtr->outN )==false )
      continue;

    // create an empty frame
    if( cmFrameFileFrameCreate( ffH, kFrameTypeFtId, kFrameStreamFtId, sampleIdx, 0  ) != kOkFfRC )
    {
      rc = _cmFtError( kFrameFileFailFtRC, p, "Frame creation failed for frame index:%i on frame file:'%s'.",frameIdx,cmStringNullGuard(pp->featFn));
      goto errLabel;
    }

    // include the incomplete file header record in the first frame
    if( frameIdx == 0 )
      if((rc = _cmFtWriteFileHdr( p, &f, pp, ffH, summArray, false )) != kOkFtRC )
        goto errLabel;

    // execute each of the feature extractors and store the result
    if((rc = _cmFtProcExec(p, &f, pp, ffH, anlArray, audV )) != kOkFtRC )
      goto errLabel;

    // close and write the current frame 
    if( cmFrameFileFrameClose( ffH ) != kOkFfRC )
    {
      rc = _cmFtError( kFrameFileFailFtRC, p, "Frame write failed for frame index:%i on frame file:'%s'.",frameIdx,cmStringNullGuard(pp->featFn));
      goto errLabel;
    }

    ++frameIdx;
    
  }

  f.frmCnt = frameIdx;


  // update the rawAvgV[] for each feature
  if( f.frmCnt > 0 )
  {
    // sum feature value into a->sr->rawAvgV[]
    if(( rc = _cmFtProcFile(p,ffH,pp,anlArray, _cmFtProcRawMinMaxSum )) != kOkFtRC )
      goto errLabel;

    // complete the a->sr->rawAvgV[] calc
    for(i=0; i<pp->attrCnt; ++i)
      cmVOR_DivVS( anlArray[i].sr->rawAvgV, anlArray[i].ap->cnt, f.frmCnt );

    // calc sum of squared diff into a->sr->rawSdvV[]
    if(( rc = _cmFtProcFile(p,ffH,pp,anlArray, _cmFtProcRawStdDev )) != kOkFtRC )
      goto errLabel;

    // complete calc of std-dev
    for(i=0; i<pp->attrCnt; ++i)
    {
      _cmFtAnl_t* a = anlArray + i;
      cmVOR_DivVS( a->sr->rawSdvV, a->ap->cnt, f.frmCnt );
      cmVOR_PowVS( a->sr->rawSdvV, a->ap->cnt, 0.5 );
    }

    // make the initial normalized vector calculation (min/max/sum)
    if(( rc = _cmFtProcFile(p,ffH,pp,anlArray, _cmFtProcNormMinMaxSum )) != kOkFtRC )
      goto errLabel;


    // complete the a->sr->normAvgV[] calculation
    for(i=0; i<pp->attrCnt; ++i)
      cmVOR_DivVS( anlArray[i].sr->normAvgV, anlArray[i].ap->cnt, f.frmCnt );


    // calc squared of squared diff into a->sr->normSdvV[]
    if(( rc = _cmFtProcFile(p,ffH,pp,anlArray, _cmFtNormStdDev )) != kOkFtRC )
      goto errLabel;

    // complete the calc of norm std-dev
    for(i=0; i<pp->attrCnt; ++i)
    {
      _cmFtAnl_t* a = anlArray + i;
      cmVOR_DivVS( a->sr->normSdvV, a->ap->cnt, f.frmCnt );
      cmVOR_PowVS( a->sr->normSdvV, a->ap->cnt, 0.5 );

    }
  }
  
  //-------------------------------------------------------------------------
  //
  // rewrite the updated feature file header into the first frame
  //

  // rewind to the first frame
  if( cmFrameFileRewind( ffH ) != kOkFfRC )
  {
    rc = _cmFtError( kFrameFileFailFtRC, p, "Frame file rewind failed during header update on '%s'.", cmStringNullGuard(pp->featFn));
    goto errLabel;
  }

  // make the first frame current and load it into the cmFrameFiles current frame buffer
  if( cmFrameFileFrameLoadNext( ffH, kFrameTypeFtId, kFrameStreamFtId, NULL ) != kOkFfRC )
  {
    rc = _cmFtError( kFrameFileFailFtRC, p, "Frame file load next frme failed during header update on '%s'.", cmStringNullGuard(pp->featFn));
    goto errLabel;
  }
  
  // copy the update header record into the current frame buffer
  if((rc = _cmFtWriteFileHdr(p, &f, pp, ffH, summArray, true)) != kOkFtRC )
    goto errLabel;

  // write the updated  frame back to disk
  if( cmFrameFileFrameUpdate( ffH ) != kOkFfRC )
  {
    rc = _cmFtError( kFrameFileFailFtRC, p, "Frame file frame update failed during header update on '%s'.", cmStringNullGuard(pp->featFn));
    goto errLabel;
  }

 errLabel:

  if( anlArray != NULL )
    for(i=0; i<pp->attrCnt; ++i)
      cmMemPtrFree(&anlArray[i].v);

  cmMemPtrFree(&anlArray);
  cmMemPtrFree(&summArray);
  cmFrameFileClose(&ffH);

  return rc;
}

cmFtRC_t        cmFtAnalyze( cmFtH_t h )
{
  cmFtRC_t rc = kOkFtRC;
  _cmFt_t* p  = _cmFtHandleToPtr(h);
  unsigned i;

  for(i=0; i<p->paramCnt; ++i)
  {
    p->progParamIdx = i;
    p->progPassIdx  = 0;

    if((rc = cmFtAnalyzeFile(h,p->paramArray+i)) != kOkFtRC )
      break;
  }
  return rc;
}

const char* cmFtAnalyzeProgress( cmFtH_t h, unsigned* passPtr, cmReal_t* percentPtr )
{
  _cmFt_t* p = _cmFtHandleToPtr(h);


  if( percentPtr != NULL )
    *percentPtr = 0;

  if( passPtr != NULL)
    *passPtr = 0;

  if( p->progParamIdx == cmInvalidIdx )
    return NULL;

  if( percentPtr != NULL && p->progSmpCnt > 0 )
    *percentPtr = 100.0 * p->progSmpIdx / p->progSmpCnt;

  if( passPtr != NULL )
    *passPtr = p->progPassIdx;

  return p->paramArray[ p->progParamIdx ].audioFn;
}

cmFtRC_t _cmFtReaderClose( _cmFtFile_t* fp )
{
  cmFtRC_t rc = kOkFtRC;
  _cmFt_t* p  = _cmFtHandleToPtr(fp->h);
  /*
  unsigned i;

  if( cmPlviewIsValid( p->plvH ) )
    for(i=0; i<fp->info.param.attrCnt; ++i)
      if( cmPlviewFreeSource( p->plvH, fp->descArray[i].ap->id ) != kOkPlvRC )
      {
        rc = _cmFtError( kPlviewFailFtRC, p, "Plview source free failed on feature '%s'.",fp->descArray[i].lp->label);
        goto errLabel;
      }
  */

  if( cmFrameFileClose( &fp->ffH ) != kOkFfRC )
  {
    rc = _cmFtError( kFrameFileFailFtRC, p, "Frame file close failed.");
    goto errLabel;
  }

  cmMemPtrFree(&fp->descArray);
  cmMemPtrFree(&fp->info.summArray);
  cmMemPtrFree(&fp->info.param.skipArray);
  cmMemPtrFree(&fp->info.param.attrArray);
  cmMemPtrFree(&fp->hdrBuf);
  cmMemPtrFree(&fp);

 errLabel:
  return rc;
}

/*
// Fill buf[rowCnt,colCnt] with data from the source submatrix located at rowIdx,colIdx.
// Return the count of elements actually copied into buf[].
unsigned cmFtPlviewSrcFunc( void* userPtr, unsigned srcId, unsigned binIdx, unsigned frmIdx, cmReal_t* buf, unsigned binCnt, unsigned frmCnt )
{
  assert(userPtr != NULL );

  _cmFtFile_t*       fp         = (_cmFtFile_t*)userPtr;
  _cmFt_t*           p          = _cmFtHandleToPtr(fp->h);
  cmFfRC_t           rc         = kOkFfRC;
  const cmFfFrame_t* frmDescPtr = NULL;
  const cmFfMtx_t*   mtxDescPtr = NULL;
  unsigned           i;

  // seek to frmIdx
  if((rc = cmFrameFileSeek( fp->ffH, frmIdx )) != kOkFfRC )
  {
    rc = _cmFtError( kFrameFileFailFtRC, p, "Seek failed on plot data cmcess.");
    goto errLabel;
  }

  // load the frame
  for(i=0; i<frmCnt && (rc=cmFrameFileFrameLoadNext(fp->ffH, kFrameTypeFtId, kFrameStreamFtId, &frmDescPtr))==kOkFfRC; ++i)
  {
    const cmReal_t* dp;
    const _cmFtLabel_t* lp = _cmFtIdToLabelPtr(srcId);
    assert(lp != NULL);
    if((dp = cmFrameFileMtxReal( fp->ffH, lp->ffMtxId, lp->ffUnitsId, kRealFmtId, &mtxDescPtr)) == NULL )
    {
      rc = _cmFtError( kFrameFileFailFtRC, p, "Mtx data cmcess failed on plot data access.");
      goto errLabel;
    }

    cmVOR_Copy( buf + (i*binCnt), binCnt, dp );
    return binCnt;
  }
  
  errLabel:
  return 0;  
}
*/


cmFtRC_t cmFtReaderOpen(cmFtH_t h, cmFtFileH_t* hp, const char* featFn, const cmFtInfo_t** infoPtrPtr )
{
  cmFfRC_t           ffRC         = kOkFfRC;
  const cmFfFile_t*  fileDescPtr  = NULL;
  const cmFfFrame_t* frameDescPtr = NULL;
  cmFtRC_t           rc           = kOkFtRC;
  _cmFt_t*           p            = _cmFtHandleToPtr(h);
  _cmFtFile_t*       fp           = cmMemAllocZ( _cmFtFile_t, 1 );
  const cmFfMtx_t*   mp           = NULL;  
  void*              buf          = NULL;
  unsigned           i,j;
  //cmPlvSrc_t         plvSrc;


  if( infoPtrPtr != NULL )
    *infoPtrPtr = NULL;

  fp->h    = h;
  fp->ffH  = cmFrameFileNullHandle;


  // open the frame file
  if( cmFrameFileOpen(&fp->ffH, featFn, &p->ctx, &fileDescPtr ) != kOkFfRC )
  {
    rc = _cmFtError( kFrameFileFailFtRC, p, "Frame file open failed.");
    goto errLabel;
  }

  // load the first frame
  if((ffRC = cmFrameFileFrameLoadNext( fp->ffH, kFrameTypeFtId, kFrameStreamFtId, &frameDescPtr )) != kOkFfRC )
  {
    rc = _cmFtError( kFrameFileFailFtRC, p, "Frame file load failed.");
    goto errLabel;
  }  

  // read the file header
  if((buf = cmFrameFileMtxBlob(fp->ffH, kDataMId, kNoUnitsUId, &mp )) == NULL )
  {
    rc = _cmFtError( kFrameFileFailFtRC, p, "Frame file header read failed.");
    goto errLabel;
  }

  // parse the file header into fp->infoPtr
  if((rc = _cmDeserializeFileHdr( p, fp, buf, mp->rowCnt*mp->colCnt )) != kOkFtRC )
    goto errLabel;

  fp->descArray = cmMemAllocZ( _cmFtDesc_t, fp->info.param.attrCnt );

  // for each feature
  for(i=0; i<fp->info.param.attrCnt; ++i)
  {
    // setup the desc array
    fp->descArray[i].ap     = fp->info.param.attrArray + i;
    fp->descArray[i].lp     = _cmFtIdToLabelPtr( fp->descArray[i].ap->id );
    
    // sync descArray[] to summArray[] by matching the feature id's
    for(j=0; j<fp->info.param.attrCnt; ++j)
      if( fp->info.summArray[j].id == fp->descArray[i].lp->id )
      {
        fp->descArray[i].sr     = fp->info.summArray + j;
        break;
      }

    /*
    plvSrc.id               = fp->descArray[i].lp->id;
    plvSrc.label            = fp->descArray[i].lp->label;
    plvSrc.rn               = fp->descArray[i].ap->cnt;
    plvSrc.cn               = fp->info.frmCnt;
    plvSrc.userPtr          = fp;
    plvSrc.srcFuncPtr       = cmFtPlviewSrcFunc;
    plvSrc.worldExts.xMin   = 0;
    plvSrc.worldExts.xMax   = fp->info.frmCnt;
    plvSrc.worldExts.yMin   = fp->descArray[i].ap->cnt <= 1 ? fp->descArray[i].sr->rawMin : 0;
    plvSrc.worldExts.yMax   = fp->descArray[i].ap->cnt <= 1 ? fp->descArray[i].sr->rawMax : fp->descArray[i].ap->cnt;

    if( cmPlviewIsValid( p->plvH ) )
      if( cmPlviewAllocSource( p->plvH, &plvSrc ) != kOkPlvRC )
      {
        rc = _cmFtError( kPlviewFailFtRC, p, "Plview source allocattion failed for feature '%s'.",fp->descArray[i].lp->label);
        goto errLabel;
      }
    */
  }

  // rewind to the frame file
  if((ffRC = cmFrameFileRewind( fp->ffH )) != kOkFfRC )
  {
    rc = _cmFtError( kFrameFileFailFtRC, p, "Frame file rewind failed.");
    goto errLabel;
  }

  hp->h = fp;

  if( infoPtrPtr != NULL )
    *infoPtrPtr = &fp->info;

 errLabel:
  if( rc != kOkFtRC )
    _cmFtReaderClose(fp);
  return rc;
}

cmFtRC_t        cmFtReaderClose(   cmFtFileH_t* hp )
{
  cmFtRC_t rc = kOkFtRC;
  
  if( cmFtReaderIsValid(*hp) == false )
    return rc;

  _cmFtFile_t* fp = _cmFtFileHandleToPtr(*hp);
  
  if((rc = _cmFtReaderClose(fp)) != kOkFtRC )
    goto errLabel;

  hp->h = NULL;
  
 errLabel:
  return rc;
}

bool            cmFtReaderIsValid( cmFtFileH_t h )
{ return h.h != NULL; }

unsigned        cmFtReaderFeatCount( cmFtFileH_t h )
{
  _cmFtFile_t* fp = _cmFtFileHandleToPtr(h);
  return fp->info.param.attrCnt;  
}

unsigned        cmFtReaderFeatId( cmFtFileH_t h, unsigned index )
{
  _cmFtFile_t* fp = _cmFtFileHandleToPtr(h);
  assert( index < fp->info.param.attrCnt );
  return fp->descArray[index].lp->id;
}


cmFtRC_t        cmFtReaderRewind(  cmFtFileH_t h )
{
  cmFtRC_t     rc   = kOkFtRC;
  _cmFtFile_t* fp   = _cmFtFileHandleToPtr(h);
  
  if(cmFrameFileRewind( fp->ffH ) != kOkFfRC )
  {  
    _cmFt_t* p  = _cmFtHandleToPtr(fp->h);

    rc = _cmFtError( kFrameFileFailFtRC, p, "Frame file advance failed.");
    goto errLabel;
  }

 errLabel:
  return rc;
}

cmFtRC_t        cmFtReaderSeek( cmFtFileH_t h, unsigned frmIdx )
{
  cmFtRC_t     rc   = kOkFtRC;
  _cmFtFile_t* fp   = _cmFtFileHandleToPtr(h);

  if( cmFrameFileSeek( fp->ffH, kFrameStreamFtId, frmIdx ) != kOkFtRC )
  {
    _cmFt_t* p = _cmFtHandleToPtr(fp->h);

    rc = _cmFtError( kFrameFileFailFtRC, p, "Frame file seek failed.");
    goto errLabel;
  }

 errLabel:
  return rc;
}

cmFtRC_t        cmFtReaderAdvance( cmFtFileH_t h, cmFtFrameDesc_t* fdp )
{
  cmFfRC_t           ffRC         = kOkFfRC;
  const cmFfFrame_t* frameDescPtr = NULL;
  cmFtRC_t           rc           = kOkFtRC;
  _cmFtFile_t*       fp           = _cmFtFileHandleToPtr(h);
  _cmFt_t*           p            = _cmFtHandleToPtr(fp->h);


  if((ffRC = cmFrameFileFrameLoadNext( fp->ffH, kFrameTypeFtId, kFrameStreamFtId, &frameDescPtr )) != kOkFfRC )
  {  
    if( ffRC == kEofFfRC )
      rc = kEofFtRC;
    else
    {
      rc = _cmFtError( kFrameFileFailFtRC, p, "Frame file advance failed.");
      goto errLabel;
    }
  }

 errLabel:
  if( fdp != NULL )
  {
    if( rc == kOkFtRC )
    {
      fdp->smpIdx = frameDescPtr->time.sampleIdx;
      fdp->frmIdx = cmFrameFileFrameLoadedIndex(fp->ffH);
    }
    else
    {
      fdp->smpIdx = cmInvalidIdx;
      fdp->frmIdx = cmInvalidIdx;
    }
  }
  return rc;
}

cmReal_t* cmFtReaderData( cmFtFileH_t h, unsigned id, unsigned* cntPtr )
{
  _cmFtFile_t*     fp  = _cmFtFileHandleToPtr(h);
  cmReal_t*        dp  = NULL;
  _cmFtLabel_t*    lp  = _cmFtIdToLabelPtr(id);
  const cmFfMtx_t* mdp = NULL;

  assert( lp != NULL );

  if( cntPtr != NULL )
    *cntPtr = 0;

  if((dp = cmFrameFileMtxReal(fp->ffH,lp->ffMtxId,lp->ffUnitsId,&mdp)) == NULL )
    return NULL;

  if( cntPtr != NULL )
    *cntPtr = mdp->rowCnt * mdp->colCnt;

  return dp;
}

cmFtRC_t   cmFtReaderCopy(    cmFtFileH_t h, unsigned featId, unsigned frmIdx, cmReal_t* buf, unsigned frmCnt, unsigned elePerFrmCnt, unsigned* outEleCntPtr )
{
  cmFtRC_t      rc = kOkFtRC;
  _cmFtFile_t*  fp = _cmFtFileHandleToPtr(h);
  _cmFt_t*      p  = _cmFtHandleToPtr(fp->h);
  _cmFtLabel_t* lp = _cmFtIdToLabelPtr(featId);

  assert( lp != NULL );
  
  if( cmFrameFileMtxLoadReal( fp->ffH, kFrameStreamFtId, lp->ffMtxId, lp->ffUnitsId, frmIdx, frmCnt, buf, frmCnt*elePerFrmCnt, outEleCntPtr ) != kOkFfRC )
  {
    rc = _cmFtError( kFrameFileFailFtRC, p, "Frame load matrix failed.");
    goto errLabel;
  }  
  
 errLabel:
  return rc;
}


cmFtRC_t   cmFtReaderMultiSetup(  cmFtFileH_t h,  cmFtMulti_t* multiArray, unsigned multiCnt, unsigned* featVectEleCntPtr )
{
  cmFtRC_t     rc = kOkFtRC;
  _cmFtFile_t* fp = _cmFtFileHandleToPtr(h);
  _cmFt_t*     p  = _cmFtHandleToPtr(fp->h);
  unsigned     i,j;

  assert( featVectEleCntPtr != NULL );
  *featVectEleCntPtr = 0;

  for(i=0; i<multiCnt; ++i)
  {
    const _cmFtLabel_t* lp;

    // locate the static parameters assoc'd with this feature
    if((lp = _cmFtIdToLabelPtr( multiArray[i].featId )) == NULL )
    {
      rc = _cmFtError( kInvalidFeatIdFtRC, p, "Invalid feature id %i.",multiArray[i].featId);
      goto errLabel;
    }

    // locate the feature info assoc'd with this file
    for(j=0; j<fp->info.param.attrCnt; ++j)
    {
      if( fp->info.param.attrArray[j].id == multiArray[i].featId )
      {
        // if the multi ele cnt is -1 then use all avail ele's
        if( multiArray[i].cnt == -1 )
          multiArray[i].cnt = fp->info.param.attrArray[j].cnt;
        

        // verify the feature element count 
        if( fp->info.param.attrArray[j].cnt < multiArray[i].cnt )
        {
          rc = _cmFtError( kInvalidFeatIdFtRC, p, "The requested feature element count %i is greater than the actual feature count %i in feature file '%s'.",multiArray[i].cnt,fp->info.param.attrArray[j].cnt,fp->info.param.featFn);
          goto errLabel;
        }
        break;
      }
    }

    // verify that the feature attr recd was found
    if( j >= fp->info.param.attrCnt )
    {
      rc = _cmFtError( kInvalidFeatIdFtRC, p, "The feature %i was not used in the feature file '%s'.",multiArray[i].featId,fp->info.param.featFn);
      goto errLabel;
    }

    multiArray[i].id0 = lp->ffMtxId;
    multiArray[i].id1 = lp->ffUnitsId;

    *featVectEleCntPtr += multiArray[i].cnt;
  }
  
 errLabel:
  return rc;
}

cmFtRC_t  cmFtReaderMultiData(   cmFtFileH_t h, const cmFtMulti_t* multiArray, unsigned multiCnt, cmReal_t* outV, unsigned outN )
{
  cmFtRC_t         rc  = kOkFtRC;
  _cmFtFile_t*     fp  = _cmFtFileHandleToPtr(h);
  _cmFt_t*         p   = _cmFtHandleToPtr(fp->h);
  unsigned         i;
  unsigned         n = 0;

  for(i=0; i<multiCnt; ++i)
  {
    const cmFfMtx_t*   mdp = NULL;
    const cmFtMulti_t* m   = multiArray + i;
    cmReal_t*          dp  = NULL;

    if((dp = cmFrameFileMtxReal(fp->ffH,m->id0,m->id1,&mdp)) == NULL )
    {
      rc = _cmFtError( kFrameFileFailFtRC, p, "Matrix read failed on feature file '%s'.", fp->info.param.featFn);
      goto errLabel;
    }

    assert(m->cnt <= mdp->rowCnt*mdp->colCnt);
    assert(n + m->cnt <= outN );

    cmVOR_Copy(outV,  m->cnt, dp );
    outV += m->cnt;
    n    += m->cnt;

  }

 errLabel:
  return rc;

}

cmFtSumm_t* _cmFtReaderFindSummPtr( _cmFtFile_t* fp, unsigned featId )
{
  unsigned           i;
  const cmFtParam_t* pp = &fp->info.param;

  for(i=0; i<pp->attrCnt; ++i)
    if( fp->info.summArray[i].id == featId )
      return fp->info.summArray + i;
   return NULL;
}

cmFtRC_t        cmFtReaderReport( cmFtFileH_t h, unsigned featId )
{
  cmFtRC_t          rc = kOkFtRC;
  _cmFtFile_t*      fp = _cmFtFileHandleToPtr(h);  
  _cmFt_t*          p  = _cmFtHandleToPtr(fp->h);
  const cmFtInfo_t* ip = &fp->info;
  const cmFtParam_t* pp = &ip->param;
  unsigned           i;
  cmFtSumm_t*        s;

  _cmFtPrint(p,"ch:%i audio:%s\n",pp->chIdx,pp->audioFn);
  _cmFtPrint(p,"wndMs:%f hopFact:%i normAudioFl:%i \n",pp->wndMs,pp->hopFact,pp->normAudioFl);
 
  _cmFtPrint(p,"skip:\n");
  for(i=0; i<pp->skipCnt; ++i)
    _cmFtPrint(p,"idx:%10i cnt:%10i \n",pp->skipArray[i].smpIdx,pp->skipArray[i].smpCnt);

  _cmFtPrint(p,"attr:\n");
  for(i=0; i<pp->attrCnt; ++i)
    _cmFtPrint(p,"cnt:%4i normFl:%i raw min:%12f max:%12f norm min:%12f max:%12f %s\n",pp->attrArray[i].cnt,pp->attrArray[i].normFl,fp->descArray[i].sr->rawMin,fp->descArray[i].sr->rawMax,fp->descArray[i].sr->normMin,fp->descArray[i].sr->normMax,cmFtFeatIdToLabel(pp->attrArray[i].id));
  
  _cmFtPrint(p,"frmCnt:%i skipFrmCnt:%i floorFrmCnt:%i srate:%f fftSmpCnt:%i hopSmpCnt:%i binCnt:%i binHz:%f\n",ip->frmCnt,ip->skipFrmCnt,ip->floorFrmCnt,ip->srate,ip->fftSmpCnt,ip->hopSmpCnt,ip->binCnt,ip->srate/ip->fftSmpCnt);

  if( featId != kInvalidFtId )
  {
    if((s = _cmFtReaderFindSummPtr(fp,featId)) == NULL )
      return _cmFtError( kInvalidFeatIdFtRC, p, "The feature id %i is not valid.",featId);  

    _cmFtPrint(p,"feature:%s \n",_cmFtIdToLabelPtr(featId)->label);

    cmVOR_PrintLE("raw min: ", &p->ctx.rpt, 1, s->cnt, s->rawMinV );
    cmVOR_PrintLE("raw max: ", &p->ctx.rpt, 1, s->cnt, s->rawMaxV );
    cmVOR_PrintLE("raw avg: ", &p->ctx.rpt, 1, s->cnt, s->rawAvgV );
    cmVOR_PrintLE("raw sdv: ", &p->ctx.rpt, 1, s->cnt, s->rawSdvV );

    cmVOR_PrintLE("norm min:", &p->ctx.rpt, 1, s->cnt, s->normMinV );
    cmVOR_PrintLE("norm max:", &p->ctx.rpt, 1, s->cnt, s->normMaxV );
    cmVOR_PrintLE("norm avg:", &p->ctx.rpt, 1, s->cnt, s->normAvgV );
    cmVOR_PrintLE("norm sdv:", &p->ctx.rpt, 1, s->cnt, s->normSdvV );
  }

  return rc;
}

cmFtRC_t  cmFtReaderReportFn(  cmFtH_t h, const cmChar_t* fn, unsigned featId )
{
  cmFtRC_t rc0,rc1;
  cmFtFileH_t fh = cmFtFileNullHandle;
  if((rc0 = cmFtReaderOpen(h,&fh,fn,NULL)) != kOkFtRC )
    return rc0;

  rc0 = cmFtReaderReport(fh,featId);

  rc1 =  cmFtReaderClose(&fh);

  return rc0 != kOkFtRC ? rc0 : rc1;
}

cmFtRC_t  cmFtReaderReportFeature( cmFtFileH_t h, unsigned featId, unsigned frmIdx, unsigned frmCnt )
{
  cmFtRC_t        rc = kOkFtRC;
  _cmFtFile_t*    fp = _cmFtFileHandleToPtr(h);  
  _cmFt_t*        p  = _cmFtHandleToPtr(fp->h);
  unsigned        i;
  cmFtFrameDesc_t ftFrameDesc;

  if((rc = cmFtReaderSeek(h,frmIdx)) != kOkFtRC )
    return rc;

  for(i=0; i<frmCnt && (rc=cmFtReaderAdvance(h,&ftFrameDesc))==kOkFtRC; ++i)
  {
    cmReal_t* dp  = NULL;
    unsigned  cnt = 0;

    if(( dp = cmFtReaderData(h,featId,&cnt)) == NULL )
      break;

    // print first element 
    _cmFtPrint(p,"%f ",*dp);
  }
  

  return rc;
}

cmFtRC_t        cmFtReaderToBinary(cmFtFileH_t h, unsigned featId, unsigned frmIdx, unsigned frmCnt, const cmChar_t* outFn )
{
  cmFtRC_t        rc = kOkFtRC;
  _cmFtFile_t*    fp = _cmFtFileHandleToPtr(h);  
  _cmFt_t*        p  = _cmFtHandleToPtr(fp->h);
  unsigned        i;
  cmFtFrameDesc_t ftFrameDesc;
  cmFileH_t       fH;
  unsigned        hdr[] = {0,0,0};
  unsigned        maxCnt = 0;

  // create the output file
  if( cmFileOpen(&fH,outFn,kWriteFileFl,p->err.rpt) != kOkFileRC )
    return  _cmFtError( kFileFailFtRC, p, "Feature to binary file '%s' failed on output file creation.",outFn);

  // if frmCnt is not valid then set it to all frames past frmIdx
  if( frmCnt == cmInvalidCnt )
    frmCnt = cmFrameFileFrameCount(fp->ffH,kFrameStreamFtId);

  // validate frm idx
  if( frmIdx > frmCnt )
  {
    rc =  _cmFtError( kInvalidFrmIdxFtRC,p,"Frame index %i is invalid for frame count = %i.",frmIdx,frmCnt);
    goto errLabel;
  }

  // seek to the location first output frame
  if((rc = cmFtReaderSeek(h,frmIdx)) != kOkFtRC )
    goto errLabel;

  hdr[0] = frmCnt;  // count of frames
  hdr[1] = 0;       // count of elements per frame
  hdr[2] = sizeof(cmReal_t);

  // write the file header
  if( cmFileWrite(fH,hdr,sizeof(hdr)) != kOkFileRC )
  {
    rc = _cmFtError( kFileFailFtRC,p,"The output file header write failed.");
    goto errLabel;
  }

  // iterate through each frame
  for(i=0; i<frmCnt && (rc=cmFtReaderAdvance(h,&ftFrameDesc))==kOkFtRC; ++i)
  {
    cmReal_t* dp  = NULL;
    unsigned  cnt = 0;

    // get a pointer to the data for the requested feature
    if(( dp = cmFtReaderData(h,featId,&cnt)) == NULL )
      break;

    // write the count of elements in this frame
    if( cmFileWrite(fH,&cnt,sizeof(cnt)) != kOkFileRC )
    {
      rc = _cmFtError( kFileFailFtRC,p,"Output write failed on frame header at frame index %i.",i);
      goto errLabel;
    }

    // write the data 
    if( cmFileWrite(fH,dp,sizeof(*dp)*cnt) != kOkFileRC )
    {
      rc = _cmFtError( kFileFailFtRC,p,"Output data write failed on frame index %i.",i);
      goto errLabel;
    }

    if( cnt > maxCnt )
      maxCnt = cnt;
  }

  // rewind to the beginning of the file
  if( cmFileSeek(fH,kBeginFileFl,0) != kOkFileRC )
  {
    rc = _cmFtError( kFileFailFtRC,p,"Output file rewind failed.");
    goto errLabel;
  }

  // rewrite the header
  hdr[1] = maxCnt;
  if( cmFileWrite(fH,hdr,sizeof(hdr)) != kOkFileRC )
  {
    rc = _cmFtError( kFileFailFtRC,p,"The output file header re-write failed.");
    goto errLabel;
  }
  
 errLabel:

  if( cmFileIsValid(fH) )
    if( cmFileClose(&fH) != kOkFileRC )
      _cmFtError( kFileFailFtRC,p,"Output file close failed.");

  return rc;
}

cmFtRC_t        cmFtReaderToBinaryFn(cmFtH_t h, const cmChar_t* fn, unsigned featId, unsigned frmIdx, unsigned frmCnt, const cmChar_t* outFn )
{
  cmFtRC_t    rc = kOkFtRC;
  cmFtFileH_t fH = cmFtFileNullHandle;

  if((rc = cmFtReaderOpen(h,&fH,fn,NULL)) != kOkFtRC )
    return rc;
  
  rc = cmFtReaderToBinary(fH,featId,frmIdx,frmCnt,outFn);
    
  cmFtRC_t rc1 = cmFtReaderClose(&fH);

  return rc==kOkFtRC ? rc1 : rc;
}
