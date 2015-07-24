#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmGr.h"
#include "cmGrDevCtx.h"

// cmGr_t state flags
enum
{
  kDirtyGrFl = 0x01 // the cmGr object is dirty
};


typedef struct cmGrObj_str
{
  unsigned      id;          //
  cmGrObjFunc_t f;           // 
  cmGrVExt_t    wext;        // world coord's contained within this object (children of this object are contained by these extents)
  unsigned      wlimitFlags; // kLeftGrFl | kRightGrFl | kTopGrFl | kBottomGrFl
  cmGrVExt_t    wlimitExt;   // limit extents for wext (ext's with set flags always define the associated wext value)

  struct cmGrObj_str* parent;
  struct cmGrObj_str* children;
  struct cmGrObj_str* rsib;
  struct cmGrObj_str* lsib;

} cmGrObj_t;

typedef struct cmGrSync_str
{
  cmGrH_t              grH;
  unsigned             flags;  // kGrXXXSyncGrFl 
  struct cmGrSync_str* link;
} cmGrSync_t;

typedef struct cmGrColorMap_str
{
  const cmChar_t*        label;
  unsigned               id;
  cmGrColor_t*           map;
  unsigned               cnt;
  struct cmGrColorMap_str* link;
} cmGrColorMap_t;

typedef struct cmGrKeyMap_str
{
  unsigned        idx;          // index of this keymap entry (used for debugging)
  char            ascii;        // printible ascii character associated with this keycode or 0 if not printable
  cmGrKeyCodeId_t keycode;      // cmGr keycode
} cmGrKeyMap_t;

typedef struct
{
  cmCtx_t    ctx;
  cmErr_t    err;
  unsigned   id;                // user definable id
  cmLHeapH_t lhH;
  unsigned   cfgFlags;
  unsigned   stateFlags;

  cmGrObj_t* msDnObj;           // obj under last ms dn
  cmGrPPt_t  msDnPPt;           // last ms dn loc'n
  cmGrVPt_t  msDnVPt;           // last ms dn in same coords as msDnObj->vext (inside msDnObj->parent->wxt)
  cmGrVPt_t  msVPt;             // cur  ms pt in same coords as msDnObj->vext (inside msDnObj->parent->wxt)
  cmGrVSz_t  msDnVOffs;
  
  bool      selValidFl;
  cmGrVPt_t sel0Pt;
  cmGrVPt_t sel1Pt;

  cmGrVPt_t localPt;
  cmGrVPt_t globalPt;

  cmGrVExt_t      vext;          // view virtual extents
  cmGrPExt_t      pext;          // view physical extents
  cmGrObj_t*      objs;          // object tree
  cmGrObj_t*      rootObj;       // current root object
  unsigned char*  img;           // temporary image inversion buffer   
  cmGrCbFunc_t    cbFunc;        //
  void*           cbArg;         //
  cmGrSync_t*     syncs;         //
  cmGrColorMap_t* maps;          // color maps
} cmGr_t;

cmGrH_t    cmGrNullHandle    = cmSTATIC_NULL_HANDLE;
cmGrObjH_t cmGrObjNullHandle = cmSTATIC_NULL_HANDLE;


cmGrKeyMap_t _cmGrKeyMap[] =
{
  {  0, 0, 0 },
  {  1, 0, 0 },
  {  2, 0, 0 },
  {  3, 0, 0 },
  {  4, 0, 0 },
  {  5, 0, kHomeGrId},
  {  6, 0, kPageUpGrId},
  {  7, 0, kEndGrId},
  {  8, 8, kBackSpaceGrId },
  {  9, 9, kTabGrId },

  { 10, 0, kPageDownGrId},
  { 11, 0, kLeftGrId},
  { 12, 0, kUpGrId},
  { 13, 13, kEnterGrId },
  { 14, 0, kRightGrId},
  { 15, 0, kDownGrId},
  { 16, 0, kInsertGrId},
  { 17, 0, kPrintGrId},
  { 18, 0, kScrollLockGrId},
  { 19, 0, kPauseGrId},
  { 20, 0, kMenuGrId},

 
  { 21, 0, kLShiftGrId},
  { 22, 0, kRShiftGrId},
  { 23, 0, kLCtrlGrId},
  { 24, 0, kRCtrlGrId},
  { 25, 0, kLAltGrId},
  { 26, 0, kRAltGrId},
  { 27, 27, kEscapeGrId },
  { 28, 0, kLSuperGrId},
  { 29, 0, kRSuperGrId},
  { 30, 0, kNumLockGrId},
  { 31, 0, kCapsLockGrId},

  { 32, 32, kSpaceGrId },
  { 33, 33, kExclMarkGrId },
  { 34, 34, kDQuoteGrId },
  { 35, 35, kPoundGrId },
  { 36, 36, kDollarGrId },
  { 37, 37, kPercentGrId },
  { 38, 38, kAmpersandGrId },
  { 39, 39, kApostropheGrId },

  { 40, 40, kLParenGrId },
  { 41, 41, kRParenGrId },
  { 42, 42, kAsteriskGrId },
  { 43, 43, kPlusGrId },
  { 44, 44, kCommaGrId },
  { 45, 45, kHyphenGrId },
  { 46, 46, kPeriodGrId },
  { 47, 47, kForwardSlashGrId },
  { 48, 48, k0GrId },
  { 49, 49, k1GrId },

  { 50, 50, k2GrId },
  { 51, 51, k3GrId },
  { 52, 52, k4GrId },
  { 53, 53, k5GrId },
  { 54, 54, k6GrId },
  { 55, 55, k7GrId },
  { 56, 56, k8GrId },
  { 57, 57, k9GrId },
  { 58, 58, kColonGrId },
  { 59, 59, kSemiColonGrId },

  { 60, 60, kLesserGrId },
  { 61, 61, kEqualGrId },
  { 62, 62, kGreaterGrId },
  { 63, 63, kQMarkGrId },
  { 64, 64, kAtGrId },
  { 65, 65, kA_GrId },
  { 66, 66, kB_GrId },
  { 67, 67, kC_GrId },
  { 68, 68, kD_GrId },
  { 69, 69, kE_GrId },

  { 70, 70, kF_GrId },
  { 71, 71, kG_GrId },
  { 72, 72, kH_GrId },
  { 73, 73, kI_GrId },
  { 74, 74, kJ_GrId },
  { 75, 75, kK_GrId },
  { 76, 76, kL_GrId },
  { 77, 77, kM_GrId },
  { 78, 78, kN_GrId },
  { 79, 79, kO_GrId },

  { 80, 80, kP_GrId },
  { 81, 81, kQ_GrId },
  { 82, 82, kR_GrId },
  { 83, 83, kS_GrId },
  { 84, 84, kT_GrId },
  { 85, 85, kU_GrId },
  { 86, 86, kV_GrId },
  { 87, 87, kW_GrId },
  { 88, 88, kX_GrId },
  { 89, 89, kY_GrId },

  { 90, 90, kZ_GrId },
  { 91, 91, kLBracketGrId },
  { 92, 92, kBackSlashGrId },
  { 93, 93, kRBracketGrId },
  { 94, 94, kCaretGrId },
  { 95, 95, kUnderScoreGrId },
  { 96, 96, kAccentGrId },
  { 97, 97, ka_GrId },
  { 98, 98, kb_GrId },
  { 99, 99, kc_GrId },

  { 100, 100, kd_GrId },
  { 101, 101, ke_GrId },
  { 102, 102, kf_GrId },
  { 103, 103, kg_GrId },
  { 104, 104, kh_GrId },
  { 105, 105, ki_GrId },
  { 106, 106, kj_GrId },
  { 107, 107, kk_GrId },
  { 108, 108, kl_GrId },
  { 109, 109, km_GrId },

  { 110, 110, kn_GrId },
  { 111, 111, ko_GrId },
  { 112, 112, kp_GrId },
  { 113, 113, kq_GrId },
  { 114, 114, kr_GrId },
  { 115, 115, ks_GrId },
  { 116, 116, kt_GrId },
  { 117, 117, ku_GrId },
  { 118, 118, kv_GrId },
  { 119, 119, kw_GrId },

  { 120, 120, kx_GrId },
  { 121, 121, ky_GrId },
  { 122, 122, kz_GrId },
  { 123, 123, kLBraceGrId },
  { 124, 124, kPipeGrId },
  { 125, 125, kRBraceGrId },
  { 126, 126, kTildeGrId },
  { 127, 127, kDeleteGrId },

  { 128, 42, kNP_MultGrId },
  { 129, 43, kNP_PlusGrId },
  { 130, 45, kNP_MinusGrId },
  { 131, 46, kNP_DecPtGrId},
  { 132, 47, kNP_DivGrId},
  { 133, 48, kNP_0GrId},
  { 134, 49, kNP_1GrId},
  { 135, 50, kNP_2GrId},
  { 136, 51, kNP_3GrId},
  { 137, 52, kNP_4GrId},
  { 138, 53, kNP_5GrId},
  { 139, 54, kNP_6GrId},
  { 140, 55, kNP_7GrId},
  { 141, 56, kNP_8GrId},
  { 142, 57, kNP_9GrId},  
  { 143, 61, kNP_EqualGrId},
  { 144, 13, kNP_EnterGrId},
  { 145,  0, kFunc_1GrId},
  { 146,  0, kFunc_2GrId},
  { 147,  0, kFunc_3GrId},
  { 148,  0, kFunc_4GrId},
  { 149,  0, kFunc_5GrId},
  { 150,  0, kFunc_6GrId},
  { 151,  0, kFunc_7GrId},
  { 152,  0, kFunc_8GrId},
  { 153,  0, kFunc_9GrId},
  { 154,  0, kFunc_10GrId},
  { 155,  0, kFunc_11GrId},
  { 156,  0, kFunc_12GrId},
  { 157,  0, kBrightUpGrId},
  { 158,  0, kBrightDnGrId},
  { 159,  0, kAudio_PrevGrId},
  { 160,  0, kAudio_PlayGrId},
  { 161,  0, kAudio_NextGrId},
  { 162,  0, kAudio_MuteGrId},
  { 163,  0, kAudio_DnGrId },
  { 164,  0, kAudio_UpGrId },
  { 165,  0, kEjectGrId },
  { cmInvalidIdx, 0, cmInvalidId}
};

void _cmGrKeyMapValidate()
{
  unsigned i;
  for(i=0; _cmGrKeyMap[i].idx != cmInvalidIdx; ++i)
  {
    assert( _cmGrKeyMap[i].idx == i );
  }
}

cmGrKeyMap_t* _cmGrFindKeyMap( unsigned keycode )
{
  // printable ascii codes match their indexes
  if( 32 <= keycode && keycode <= 126 )
    return _cmGrKeyMap + keycode;

   unsigned i;
   for(i=0; i<32; ++i)
     if( _cmGrKeyMap[i].keycode == keycode )
       return _cmGrKeyMap + i;

   for(i=127; _cmGrKeyMap[i].idx != cmInvalidIdx; ++i)
     if( _cmGrKeyMap[i].keycode == keycode )
       return _cmGrKeyMap + i;

   assert(0);
   return NULL;
}



bool _cmGrSetViewExtents( cmGr_t* p, cmGrV_t minx, cmGrV_t miny, cmGrV_t maxx, cmGrV_t maxy );
bool  _cmGrSetViewExtentsE( cmGr_t* p, const cmGrVExt_t* e )
{ return _cmGrSetViewExtents(p, cmGrVExtMinX(e), cmGrVExtMinY(e), cmGrVExtMaxX(e), cmGrVExtMaxY(e) ); }

//====================================================================================================
// Expand cmGrVExt_t e0 to hold e1.
// Return true if e0 is actually changed.
bool cmGrVExtExpandToContain(cmGrVExt_t* e0, const cmGrVExt_t* e1)
{
  bool fl = false;

  if( cmGrVExtIsNullOrEmpty(e0) )
  {
    *e0 = *e1;
    return true;
  }

  assert( cmGrVExtIsNorm(e0) && cmGrVExtIsNorm(e1) );

  // min-x
  if( cmGrVExtMinX(e0) > cmGrVExtMinX(e1) )
  {
    cmGrVExtSetMinX(e0, cmGrVExtMinX(e1));
    fl = true;
  }

  // min-y
  if( cmGrVExtMinY(e0) > cmGrVExtMinY(e1) )
  {
    cmGrVExtSetMinY(e0, cmGrVExtMinY(e1));
    fl = true;
  }

  // max-y
  if( cmGrVExtMaxY(e0) < cmGrVExtMaxY(e1) )
  {
    cmGrVExtSetMaxY(e0, cmGrVExtMaxY(e1));
    fl = true;
  }

  // max-x
  if( cmGrVExtMaxX(e0) < cmGrVExtMaxX(e1) )
  {
    cmGrVExtSetMaxX(e0, cmGrVExtMaxX(e1));
    fl = true;
  }


  return fl;
}

bool cmGrVExtContain( const cmGrVExt_t* e0, cmGrVExt_t* e1 )
{
  bool    fl  = false;

  assert( cmGrVExtIsNorm(e0) && cmGrVExtIsNorm(e1) );
  // e1 must be able to fit inside e0
  assert( e1->sz.w <= e0->sz.w && e1->sz.h <= e0->sz.h );

  // if left edge of e1 is to left of e0
  if( cmGrVExtMinX(e1) < cmGrVExtMinX(e0) )
  {
    cmGrVExtSetMinX(e1,cmGrVExtMinX(e0));
    fl = true;
  }

  // if right edge of e1 is to right of e0
  if( cmGrVExtMaxX(e1) > cmGrVExtMaxX(e0) )
  {
    cmGrVExtSetMaxX(e1,cmGrVExtMaxX(e0));
    fl = true;
  }

  // if the bottom edge of e1 is below the bottom edge of e0
  if( cmGrVExtMinY(e1) < cmGrVExtMinY(e0) )
  {
    cmGrVExtSetMinY(e1,cmGrVExtMinY(e0));
    fl = true;
  }

  // if top  edge of e1 is above the top edge of e0
  if( cmGrVExtMaxY(e1) > cmGrVExtMaxY(e0) )
  {
    cmGrVExtSetMaxY(e1,cmGrVExtMaxY(e0));
    fl = true;
  }

  return fl;
}

void cmGrPExtIntersect( cmGrPExt_t* r, const cmGrPExt_t* e0, const cmGrPExt_t* e1 )
{
  if(  cmGrPExtR(e0) < cmGrPExtL(e1) || cmGrPExtL(e0) > cmGrPExtR(e1)  
    || cmGrPExtB(e0) < cmGrPExtT(e1) || cmGrPExtT(e0) > cmGrPExtB(e1) )
  {
    cmGrPExtSetEmpty(r);
    return;
  }
  
  cmGrPExtSetD(r,
    cmMax( cmGrPExtL(e0), cmGrPExtL(e1) ),
    cmMax( cmGrPExtT(e0), cmGrPExtT(e1) ),
    cmMin( cmGrPExtR(e0), cmGrPExtR(e1) ),
    cmMin( cmGrPExtB(e0), cmGrPExtB(e1) ) );
  
}

void cmGrVExtIntersect( cmGrVExt_t* r, const cmGrVExt_t* e0, const cmGrVExt_t* e1 )
{
  if(  cmGrVExtMaxX(e0) < cmGrVExtMinX(e1) || cmGrVExtMinX(e0) > cmGrVExtMaxX(e1)  
    || cmGrVExtMaxY(e0) < cmGrVExtMinY(e1) || cmGrVExtMinY(e0) > cmGrVExtMaxY(e1) )
  {
    cmGrVExtSetEmpty(r);
    return;
  }
  
  cmGrVExtSetD(r,
    cmMax( cmGrVExtMinX(e0), cmGrVExtMinX(e1) ),
    cmMax( cmGrVExtMinY(e0), cmGrVExtMinY(e1) ),
    cmMin( cmGrVExtMaxX(e0), cmGrVExtMaxX(e1) ),
    cmMin( cmGrVExtMaxY(e0), cmGrVExtMaxY(e1) ) );  
}


//====================================================================================================
//====================================================================================================
cmGr_t* _cmGrHandleToPtr( cmGrH_t h )
{
  cmGr_t* p = (cmGr_t*)h.h;
  assert( p != NULL );
  return p;
}


cmGrObj_t* _cmGrObjHandleToPtr( cmGrObjH_t h )
{
  cmGrObj_t* p = (cmGrObj_t*)h.h;
  assert( p != NULL );
  return p;
}

//====================================================================================================
unsigned        cmGrColorMapCount( cmGrH_t grH )
{
  cmGr_t*               p   = _cmGrHandleToPtr(grH);
  cmGrColorMap_t*       cmp = p->maps;
  unsigned              n   = 0;
  for(; cmp!=NULL; cmp=cmp->link)
    ++n;
  return n;
}

cmGrColorMap_t* _cmGrColorMapFromIndex( cmGr_t* p, unsigned idx )
{
  unsigned      i   = 0;
  cmGrColorMap_t* cmp = p->maps;
  for(; cmp!=NULL; ++i,cmp=cmp->link)
    if( i == idx )
      break;
  return cmp;
}

cmGrColorMap_t* _cmGrColorMapFromId( cmGr_t* p, unsigned id )
{
  cmGrColorMap_t* cmp = p->maps;
  for(; cmp!=NULL; cmp=cmp->link)
    if( cmp->id == id )
      break;
  return cmp;
}

unsigned        cmGrColorMapId( cmGrH_t grH, unsigned mapIdx )
{
  cmGr_t*       p   = _cmGrHandleToPtr(grH);
  cmGrColorMap_t* cmp;
  if((cmp = _cmGrColorMapFromIndex(p,mapIdx)) == NULL )
    return cmInvalidId;
  return cmp->id;  
}

const cmChar_t* cmGrColorMapLabel( cmGrH_t grH, unsigned id )
{
  cmGr_t*       p  = _cmGrHandleToPtr(grH);
  cmGrColorMap_t* cmp;
  if((cmp = _cmGrColorMapFromId(p,id)) == NULL )
    return NULL;
  return cmp->label;  
}

unsigned _cmGrColorMapRegister( cmGr_t* p, cmChar_t* label, const cmGrColor_t* array, unsigned cnt )     
{
  // locate an available id
  unsigned id = 0;
  while( _cmGrColorMapFromId(p,id)!=NULL )
    ++id;
  
  cmGrColorMap_t* cmp = cmLhAllocZ(p->lhH,cmGrColorMap_t,1);
  cmp->label = cmLhAllocStr(p->lhH,label);
  cmp->id    = id;
  cmp->map   = cmLhAllocZ(p->lhH,cmGrColor_t,cnt);
  cmp->cnt   = cnt;
  cmp->link  = p->maps;
  p->maps    = cmp;
  
  memcpy(cmp->map,array,sizeof(cmGrColor_t)*cnt);

  return id;
}

unsigned        cmGrColorMapRegister( cmGrH_t grH, cmChar_t* label, const cmGrColor_t* array, unsigned cnt )     
{ return _cmGrColorMapRegister( _cmGrHandleToPtr(grH),label, array, cnt ); }

cmGrColor_t*     cmGrColorMap( cmGrH_t grH, unsigned mapId )
{
  cmGr_t*       p  = _cmGrHandleToPtr(grH);
  cmGrColorMap_t* cmp;
  if((cmp = _cmGrColorMapFromId(p,mapId)) == NULL )
    return NULL;
  return cmp->map;  
}

unsigned        cmGrColorMapEleCount( cmGrH_t grH, unsigned mapId )
{
  cmGr_t*       p  = _cmGrHandleToPtr(grH);
  cmGrColorMap_t* cmp;
  if((cmp = _cmGrColorMapFromId(p,mapId)) == NULL )
    return cmInvalidId;
  return cmp->cnt;  
}

void _cmGrRgbInitDefaultColorMap( cmGr_t* p )
{
  unsigned map[] =
  {
    0x000000, // black
    0x00008b, // dark blue
    0x0000ff, // blue
    0x008080, // teal
    0x00ffff, // cyan
    0x00ff7f, // spring green
    0x00ff00, // green
    0x7cfc00, // lawn green
    0xffff00, // yellow
    0xff7f7f, // pink
    0xff00ff, // magenta
    0xff007f, //
    0xff0000, // red
    0x7f0000, // 
    0xffffff  // white
  };

  unsigned  n = sizeof(map)/sizeof(unsigned);

  _cmGrColorMapRegister(p,"default",map,n);
}



//====================================================================================================
// Object Callback Functions
//====================================================================================================

void _cmGrObjSetupFuncArgs( cmGrObjFuncArgs_t* a, cmGr_t* p, cmGrObj_t* op )
{
  cmGrH_t    h;
  cmGrObjH_t oh;

  h.h  = p;
  oh.h = op;

  a->ctx        = &p->ctx;
  a->grH        = h;
  a->objH       = oh;
  a->msDnPPt    = p->msDnPPt;
  a->msDnVPt    = p->msDnVPt;
  a->msDnVOffs  = p->msDnVOffs;
  a->msVPt      = p->msVPt;

  oh.h        = p->msDnObj;
  a->msDnObjH = oh;
}

cmGrRC_t _cmGrObjCbCreate( cmGr_t* p, cmGrObj_t* op )
{
  cmGrRC_t rc = kOkGrRC;
  
  if( op->f.createCbFunc != NULL )
  {
    cmGrObjFuncArgs_t a;
    _cmGrObjSetupFuncArgs(&a,p,op);

    a.cbArg = op->f.createCbArg;
    if((rc = op->f.createCbFunc(&a)) != kOkGrRC )
      rc = cmErrMsg(&p->err,kAppErrGrRC,"An application object (id=%i) failed on 'create'",op->id);
  }

  return rc;
}

void _cmGrObjCbDestroy( cmGr_t* p, cmGrObj_t* op )
{
  if( op->f.destroyCbFunc != NULL )
  {
    cmGrObjFuncArgs_t a;

    _cmGrObjSetupFuncArgs(&a,p,op);
    a.cbArg = op->f.destroyCbArg;

    op->f.destroyCbFunc(&a);
  }
}

void _cmGrObjCbRender( cmGr_t* p, cmGrDcH_t dcH, const cmGrObj_t* op )
{
  if( op->f.renderCbFunc != NULL )
  {
    cmGrObjFuncArgs_t a;
    _cmGrObjSetupFuncArgs(&a,p,(cmGrObj_t*)op);
    a.cbArg = op->f.renderCbArg;

    op->f.renderCbFunc(&a, dcH );
  }  
}

int _cmGrObjCbDistance( cmGr_t* p, const cmGrObj_t* op, int px, int py )
{
  int d = INT_MAX;

  if( op->f.distanceCbFunc != NULL )
  {
    cmGrObjFuncArgs_t a;
    _cmGrObjSetupFuncArgs(&a,p,(cmGrObj_t*)op);
    a.cbArg = op->f.distanceCbArg;

    d = op->f.distanceCbFunc(&a,px,py);
  }  
  return d;
}

bool _cmGrObjCbEvent( cmGr_t* p, cmGrObj_t* op, unsigned flags, unsigned key, int px, int py )
{
  bool fl = false;
  if( op->f.eventCbFunc != NULL )
  {
    cmGrObjFuncArgs_t a;
    _cmGrObjSetupFuncArgs(&a,p,op);
    a.cbArg = op->f.eventCbArg;

    fl = op->f.eventCbFunc(&a,flags,key,px,py);
  }  
  return fl;
}

void _cmGrObjCbVExt( cmGr_t* p, const cmGrObj_t* op, cmGrVExt_t* vext )
{
  if( op->f.vextCbFunc == NULL )
    cmGrVExtSetEmpty(vext);
  else
  {
    cmGrObjFuncArgs_t a;
    _cmGrObjSetupFuncArgs(&a,p,(cmGrObj_t*)op);
    a.cbArg = op->f.vextCbArg;

    op->f.vextCbFunc(&a,vext);
  }  
}

bool _cmGrObjCbIsInside( cmGr_t* p, const cmGrObj_t* op, unsigned evtFlags, int px, int py, cmGrV_t vx, cmGrV_t vy )
{
  bool fl = false;
  if( op->f.isInsideCbFunc != NULL )
  {
    cmGrObjFuncArgs_t a;
    _cmGrObjSetupFuncArgs(&a,p,(cmGrObj_t*)op);
    a.cbArg = op->f.isInsideCbArg;
    fl = op->f.isInsideCbFunc(&a,evtFlags,px,py,vx,vy);
  }  
  return fl;
}

//====================================================================================================
// Object Private Functions
//====================================================================================================
// Return true if pp is an ancestor (parent,grand-parent,great-grand-parent,...) of cp.
bool _cmGrObjIsAncestor( cmGrObj_t* pp, cmGrObj_t* cp )
{
  cmGrObj_t* tp = cp->parent;
  for(; tp != NULL; tp=tp->parent )
    if( tp == pp )
      break;

  return tp!=NULL;
}

// Append 'op' as the right-most child of 'pp'.
void _cmGrObjAppendChild( cmGrObj_t* pp, cmGrObj_t* cp)
{
  cp->parent = pp;

  if( pp->children == NULL )
  {
    pp->children = cp;
    cp->lsib     = NULL;
    cp->rsib     = NULL;
  }
  else
  {
    cmGrObj_t* op = pp->children;
    while( op->rsib != NULL )
      op          = op->rsib;

    op->rsib = cp;
    cp->rsib = NULL;
    cp->lsib = op;
  }
}

// Insert 'op' on the left of 'rp'.
void _cmGrObjInsertOnLeft( cmGrObj_t* op, cmGrObj_t* rp )
{
  op->parent = rp->parent;
  op->rsib   = rp;
  op->lsib   = rp->lsib;

  if( rp->lsib == NULL )
  {
    assert( rp->parent == rp->parent->children);
    rp->parent->children = op;
  }
  else
  {
    rp->lsib->rsib = op;    
  }

  rp->lsib = op;
}

// Insert 'op' on the right of 'lp'.
// 'pp' is the parent of 'lp'. 'pp' must be given explicitely to cover
// the case where lp is NULL - in which case the new parent for op
// cannot be determined from lp.
void _cmGrObjInsertOnRight( cmGrObj_t* op, cmGrObj_t* lp, cmGrObj_t* pp )
{
  op->parent = pp;

  if( lp == NULL )
  {
    assert( pp != NULL && pp->children==NULL );
    pp->children = op;
    op->lsib = NULL;
    op->rsib = NULL;
    return;
  }

  assert( lp->parent == pp );

  op->lsib   = lp;
  op->rsib   = lp->rsib;
  

  if( lp->rsib != NULL )
    lp->rsib->lsib = op;

  lp->rsib = op;

}

// Unlink 'op' from the tree but leave it's children attached.
void _cmGrObjUnlink( cmGrObj_t * op )
{
  if( op->parent != NULL && op->parent->children == op )
    op->parent->children = op->parent->children->rsib;

  if( op->rsib != NULL )
    op->rsib->lsib = op->lsib;

  if( op->lsib != NULL )
    op->lsib->rsib = op->rsib;

  op->parent = NULL;
  op->rsib   = NULL;
  op->lsib   = NULL;
}

// Free 'op' and all of its children. 
// 'op' must be unlinked before calling this function
cmGrRC_t  _cmGrObjFree( cmGr_t* p, cmGrObj_t* op )
{
  cmGrRC_t rc = kOkGrRC;

  // go to the deepest child
  if( op->children != NULL )
    if((rc = _cmGrObjFree(p,op->children)) != kOkGrRC )
      return rc;

  // go right
  if( op->rsib != NULL )
    if((rc = _cmGrObjFree(p,op->rsib)) != kOkGrRC )
      return rc;

  // inform the application that we are destroying this object
  _cmGrObjCbDestroy(p,op);

  _cmGrObjUnlink(op);

  cmMemFree(op);

  return rc;
}

cmGrRC_t _cmGrObjUnlinkAndFree( cmGr_t* p, cmGrObj_t* op )
{
  cmGrRC_t   rc   = kOkGrRC;
  cmGrObj_t* rsib = op->rsib;
  cmGrObj_t* par  = op->parent;

  _cmGrObjUnlink(op);

  // if the free fails  ...
  if((rc = _cmGrObjFree(p,op)) != kOkGrRC )
  {
    // ... then restore the objects position
    if( rsib == NULL )
      _cmGrObjInsertOnLeft(op,rsib);
    else
      _cmGrObjAppendChild(par,op);
  }

  return rc;
}

// Return kL,T,R,BGrFl indicating which directions of wext are in violation of op->wlimitExt.
// Return's 0 if no limits are in violation
unsigned _cmGrObjWorldLimitsTestViolation( const cmGrObj_t* op, const cmGrVExt_t* wext )
{
  unsigned violFlags = 0;

  if( cmIsFlag( op->wlimitFlags, kLeftGrFl) )
    if( cmGrVExtMinX(wext) < cmGrVExtMinX(&op->wlimitExt) )
      violFlags = kLeftGrFl;

  if( cmIsFlag( op->wlimitFlags, kTopGrFl) )
    if( cmGrVExtMaxY(wext) < cmGrVExtMaxY(&op->wlimitExt) )
      violFlags = kTopGrFl;

  if( cmIsFlag( op->wlimitFlags, kRightGrFl) )
    if( cmGrVExtMaxX(wext) > cmGrVExtMaxX(&op->wlimitExt) )
      violFlags = kRightGrFl;

  if( cmIsFlag( op->wlimitFlags, kBottomGrFl) )
    if( cmGrVExtMinY(wext) > cmGrVExtMinY(&op->wlimitExt) )
      violFlags = kBottomGrFl;

  return violFlags;
}

// If op has world extent limits then apply them to 'ext'.
// Extent directions in 'ext' which do not have limits in 'op' are unchanged.
// Extent directions in 'ext' which have limits are set to the limit.
void _cmGrObjApplyExtLimits( cmGrObj_t* op, cmGrVExt_t* ext )
{
  if( cmIsFlag(op->wlimitFlags,kLeftGrFl) )
    cmGrVExtSetMinX(ext,cmGrVExtMinX(&op->wlimitExt));

  if( cmIsFlag(op->wlimitFlags,kBottomGrFl) )
    cmGrVExtSetMinY(ext,cmGrVExtMinY(&op->wlimitExt));   

  if( cmIsFlag(op->wlimitFlags,kTopGrFl) )
    cmGrVExtSetMaxY(ext,cmGrVExtMaxY(&op->wlimitExt));

  if( cmIsFlag(op->wlimitFlags,kRightGrFl) )
    cmGrVExtSetMaxX(ext,cmGrVExtMaxX(&op->wlimitExt));

}


// Return the outside extents of the children of 'op'.
// Returns false if there are no children and leaves wext set to NULL.
bool _cmGrObjChildExts( cmGr_t* p, const cmGrObj_t* op, cmGrVExt_t* ext )
{
  cmGrVExtSetNull(ext);

  op = op->children;

  if( op == NULL )
    return false;

  _cmGrObjCbVExt(p,op,ext);

  for(op=op->rsib; op!=NULL; op=op->rsib)
  {
    cmGrVExt_t e;
    _cmGrObjCbVExt(p,op,&e);
    cmGrVExtExpandToContain(ext,&e);
  }

  return true;
}

cmGrRC_t _cmGrObjSetWorldExt( cmGr_t* p, cmGrObj_t* op, const cmGrVExt_t* wext );

// The world extents changed to 'ref_wext' on an object.
// Examine all 'sync'ed' objects and expand them to be contained by 'ref_wext'.
cmGrRC_t  _cmGrSyncWorldExtentsExpand( cmGr_t* p, const cmGrVExt_t* ref_wext )
{
  cmGrRC_t rc = kOkGrRC;

  // apply changes to synch targets
  cmGrSync_t* sp = p->syncs;
  for(; sp!=NULL; sp=sp->link)
    if( cmIsFlag(sp->flags,kWorldSyncGrFl) )
    {
      // get the target ROOT object
      cmGrObj_t* top = _cmGrObjHandleToPtr(cmGrRootObjH(sp->grH));
      bool       fl  = false;
      cmGrVExt_t top_wext = top->wext;

      //printf("sync %i ",top->id);
      //cmGrVExtPrint("top_wext",&top_wext);
      //cmGrVExtPrint("ref_wext",ref_wext);

      // if horz sync was requested ...
      if( !fl && cmIsFlag(sp->flags,kHorzSyncGrFl) )
      {
        if( cmGrVExtIsNullOrEmpty(&top_wext) )
        {
          cmGrVExtSetMinX(&top_wext,cmGrVExtMinX(ref_wext));
          cmGrVExtSetMaxX(&top_wext,cmGrVExtMaxX(ref_wext));
          fl = true;
        }
        else
        {
          // ... and the target needs to expand and can expand
          if( cmGrVExtMinX(&top_wext) > cmGrVExtMinX(ref_wext) && cmIsNotFlag(top->wlimitFlags,kLeftGrFl) )   
          {
            cmGrVExtSetMinX(&top_wext,cmGrVExtMinX(ref_wext)); // .. expand the view
            fl      = true;
          }

          if( cmGrVExtMaxX(&top_wext) < cmGrVExtMaxX(ref_wext) && cmIsNotFlag(top->wlimitFlags,kRightGrFl) )   
          {
            cmGrVExtSetMaxX(&top_wext,cmGrVExtMaxX(ref_wext));
            fl      = true;
          }
        }
      }

      // if vert sync was requested ...
      if( !fl && cmIsFlag(sp->flags,kVertSyncGrFl) )
      {
        if( cmGrVExtIsNullOrEmpty(&top_wext) )
        {
          cmGrVExtSetMinY(&top_wext,cmGrVExtMinY(ref_wext));
          cmGrVExtSetMaxY(&top_wext,cmGrVExtMaxY(ref_wext));          
          fl = true;
        }
        else
        {
          if( cmGrVExtMinY(&top_wext) > cmGrVExtMinY(ref_wext) && cmIsNotFlag(top->wlimitFlags,kBottomGrFl))   
          {
            cmGrVExtSetMinY(&top_wext,cmGrVExtMinY(ref_wext));
            fl      = true;
          }

          if( cmGrVExtMaxY(&top_wext) < cmGrVExtMaxY(ref_wext) && cmIsNotFlag(top->wlimitFlags,kTopGrFl) )   
          {
            cmGrVExtSetMaxY(&top_wext,cmGrVExtMaxY(ref_wext));
            fl      = true;
          }

        }
      }

      // If fl is set then top_wext contains an expanded world view
      if( fl )
      {
        //cmGrVExtPrint("out top_wext",&top_wext);
        // this call may result in a recursion back into this function
        if((rc = _cmGrObjSetWorldExt( _cmGrHandleToPtr(sp->grH), top, &top_wext )) != kOkGrRC )
          goto errLabel;
      }
    }

 errLabel:
  return rc;
}

cmGrRC_t _cmGrObjSetWorldExt( cmGr_t* p, cmGrObj_t* op, const cmGrVExt_t* wext )
{
  cmGrRC_t   rc = kOkGrRC;
  cmGrVExt_t ce;
  cmGrVExt_t we = *wext; // make a copy of the new extents to override the 'const' on 'wext'.

  // apply the world ext limits to 'we'.
  _cmGrObjApplyExtLimits(op,&we);

  assert(cmGrVExtIsNorm(&we));   // assert w/h are positive

  // get the extents around all children
  if( _cmGrObjChildExts(p, op, &ce ) )
  {
    // if 'ce' is not entirely inside 'we'
    if( cmGrVExtIsExtInside(&we,&ce) == false )
      return cmErrMsg(&p->err,kExtsErrGrRC,"The change in world extents would have resulted in child objects outside the requested world extents.");
  }

  // if world extents are actually changing
  if( !cmGrVExtIsEqual(&op->wext,&we) )
  {
    // update the world extents for this object
    op->wext       = we;

    //op->stateFlags = cmSetFlag(op->stateFlags,kDirtyObjFl);

    //cmGrVExtPrint(cmTsPrintf("set w: %i ",op->id),&we);

    // if this is the root object
    if( p->rootObj == op )
    {
      // this call may result in recursion back into this function
      // if two cmGr's are mutual sync targets - an infinite loop
      // should be avoided by the cmGrVExtIsEqual() test above.
      rc = _cmGrSyncWorldExtentsExpand(p, wext );
    }
  }

  return rc;
}

void  _cmGrObjReport( cmGr_t* p, cmGrObj_t* op, cmRpt_t* rpt )
{
  cmGrVExt_t vext;
  cmRptPrintf(rpt,"id:0x%x \n",op->id);

  _cmGrObjCbVExt( p, op, &vext);  
  cmGrVExtRpt(&vext,rpt);
  cmRptPrintf(rpt,"\n");
}

void  _cmGrObjReportR( cmGr_t* p, cmGrObj_t* op, cmRpt_t* rpt)
{
  _cmGrObjReport(p, op,rpt);

  if( op->children != NULL )
    _cmGrObjReport(p,op->children,rpt);

  if( op->rsib != NULL )
    _cmGrObjReport(p,op->rsib,rpt);
}


//====================================================================================================
cmGrRC_t cmGrObjCreate(  cmGrH_t h, cmGrObjH_t* ohp, cmGrObjH_t parentH, cmGrObjFunc_t* f, unsigned id, unsigned flags, const cmGrVExt_t* wext )
{
  cmGrRC_t rc;

  if((rc = cmGrObjDestroy(h,ohp)) != kOkGrRC )
    return rc;

  cmGr_t* p = _cmGrHandleToPtr(h);

  // allocate the new object
  cmGrObj_t* op = cmMemAllocZ(cmGrObj_t,1);

  op->id         = id;
  op->f          = *f;
  
  if( wext != NULL )
  {
    op->wext = *wext;

    if( cmGrVExtIsNotNull(wext) )
      cmGrVExtNorm(&op->wext);
  }

  // if an explicit parent was not provided
  // then assign the root object as the parent
  if( cmGrObjIsValid(h,parentH) == false )
    parentH.h = p->rootObj;
  
  // insert the object into the tree
  if( cmGrObjIsValid(h,parentH) )
    _cmGrObjAppendChild(_cmGrObjHandleToPtr(parentH),op);
  else
  {
    // no root object exits - so make this obj the root
    assert(p->objs == NULL );
    p->objs       = op; 
  }

  ohp->h = op;

  // Notify the application that an object was created.
  if((rc = _cmGrObjCbCreate(p,op)) != kOkGrRC )
    goto errLabel;


  if( f->vextCbFunc != NULL )
  {
    cmGrVExt_t vext;
    cmGrVExtSetEmpty(&vext);

    // get the local virtual extents for the new object
    cmGrObjLocalVExt(h, *ohp, &vext );

    if( cmGrVExtIsNotNullOrEmpty(&vext) )
    {      
      // expand the parents world to contain the new object
      if( op->parent != NULL )
      {
        cmGrVExt_t parent_wext = op->parent->wext;
        if( cmGrVExtExpandToContain(&parent_wext,&vext) )
          _cmGrObjSetWorldExt(p,op->parent,&parent_wext);

        assert( cmGrVExtIsExtInside(&op->parent->wext,&vext) );
      }

      // if cfg'd to expand the view to contain new objects then do so here
      if( op->parent!=NULL && op->parent->parent==NULL && cmIsFlag(p->cfgFlags,kExpandViewGrFl) && cmGrVExtIsExtInside(&p->vext,&vext) == false )
      {
        cmGrVExt_t v = p->vext;
        if( cmGrVExtExpandToContain(&v,&vext) )
          _cmGrSetViewExtentsE(p,&v);        

        assert( cmGrVExtIsExtInside(&op->parent->wext,&p->vext));
      }

      // if the new object is inside the view extents then mark
      // the object as dirty
      //if( cmGrVExtIsExtInside(&p->vext,&vext) )
      //  op->stateFlags = cmSetFlag(op->stateFlags,kDirtyObjFl);
    }
  }

 errLabel:
  if( rc != kOkGrRC )
    cmGrObjDestroy(h,ohp);
  
  return rc;
}

cmGrRC_t _cmGrObjDestroy( cmGr_t* p, cmGrObj_t* op )
{
  if( op == NULL )
    return kOkGrRC;

  return _cmGrObjUnlinkAndFree( p, op);  
}

cmGrRC_t cmGrObjDestroy( cmGrH_t h, cmGrObjH_t* ohp )
{
  cmGrRC_t rc = kOkGrRC;

  if( ohp==NULL || cmGrObjIsValid(h,*ohp) == false )
    return kOkGrRC;

  cmGrObj_t* op = _cmGrObjHandleToPtr(*ohp);
  
  if((rc = _cmGrObjDestroy(_cmGrHandleToPtr(h),op)) != kOkGrRC )
    return rc;

  ohp->h = NULL;

  return rc;
}

cmGrRC_t cmGrObjIsValid( cmGrH_t h, cmGrObjH_t  oh )
{ return h.h!=NULL && oh.h != NULL; }


unsigned   cmGrObjId(        cmGrObjH_t oh )
{ return _cmGrObjHandleToPtr(oh)->id; }

void      cmGrObjSetId(     cmGrObjH_t oh, unsigned id )
{
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  op->id        = id;
}

cmGrObjH_t cmGrObjParent(    cmGrObjH_t oh )
{ 
  cmGrObjH_t poh;
  poh.h = _cmGrObjHandleToPtr(oh)->parent;
  return poh;
}


cmGrRC_t cmGrObjSetWorldExt( cmGrH_t h, cmGrObjH_t oh, const cmGrVExt_t* wext )
{
  cmGr_t*     p = _cmGrHandleToPtr(h);
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  return _cmGrObjSetWorldExt(p,op,wext);
}

void cmGrObjWorldExt( cmGrObjH_t oh, cmGrVExt_t* wext )
{
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  *wext = op->wext;
}

cmGrRC_t cmGrObjSetWorldLimitExt( cmGrH_t h, cmGrObjH_t oh, const cmGrVExt_t* vext, unsigned limitFlags )
{
  cmGrRC_t rc = kOkGrRC;
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);

  // store the current world extents
  cmGrVExt_t lext = op->wlimitExt;
  unsigned   lfl  = op->wlimitFlags;
  
  // set the new limits
  op->wlimitExt   = *vext;
  op->wlimitFlags = limitFlags;

  cmGrVExtNorm(&op->wlimitExt);

  // attempt to apply the current world extents with the new limits
  // (this may fail if their are child objects out of range of the new extents)
  if( cmGrVExtIsNotNull(&op->wext ) )
  {
    if((rc = cmGrObjSetWorldExt(h,oh,&op->wext)) != kOkGrRC )
    {
      // we failed - restore the old limits
      op->wlimitExt = lext; 
      op->wlimitFlags = lfl;
    }
  }
   
  return rc;
}

void cmGrObjWorldLimitExt( cmGrObjH_t oh, cmGrVExt_t* vext, unsigned* limitFlags )
{
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  if( vext != NULL )
    *vext = op->wlimitExt;

  if( limitFlags != NULL )
    *limitFlags = op->wlimitFlags;
}


void cmGrObjSetCreateCb(   cmGrObjH_t oh, cmGrCreateObjCb_t   cbFunc, void* cbArg )
{
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  op->f.createCbFunc = cbFunc;
  op->f.createCbArg  = cbArg;
}

void cmGrObjSetDestroyCb(  cmGrObjH_t oh, cmGrDestroyObjCb_t  cbFunc, void* cbArg )
{
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  op->f.destroyCbFunc = cbFunc;
  op->f.destroyCbArg  = cbArg;
}

void cmGrObjSetRenderCb(   cmGrObjH_t oh, cmGrRenderObjCb_t   cbFunc, void* cbArg )
{
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  op->f.renderCbFunc = cbFunc;
  op->f.renderCbArg  = cbArg;
}

void cmGrObjSetDistanceCb( cmGrObjH_t oh, cmGrDistanceObjCb_t cbFunc, void* cbArg )
{
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  op->f.distanceCbFunc = cbFunc;
  op->f.distanceCbArg  = cbArg;
}

void cmGrObjSetEventCb(    cmGrObjH_t oh, cmGrEventObjCb_t    cbFunc, void* cbArg )
{
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  op->f.eventCbFunc = cbFunc;
  op->f.eventCbArg  = cbArg;
}

void cmGrObjSetVExtCb(     cmGrObjH_t oh, cmGrVExtObjCb_t     cbFunc, void* cbArg )
{
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  op->f.vextCbFunc = cbFunc;
  op->f.vextCbArg  = cbArg;
}

void cmGrObjSetIsInsideCb( cmGrObjH_t oh, cmGrIsInsideObjCb_t cbFunc, void* cbArg )
{
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  op->f.isInsideCbFunc = cbFunc;
  op->f.isInsideCbArg  = cbArg;
}

cmGrCreateObjCb_t   cmGrObjCreateCbFunc(   cmGrObjH_t oh )
{
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  return op->f.createCbFunc;
}

cmGrDestroyObjCb_t  cmGrObjDestroyCbFunc(  cmGrObjH_t oh )
{
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  return op->f.destroyCbFunc;
}

cmGrRenderObjCb_t   cmGrObjRenderCbFunc(   cmGrObjH_t oh )
{
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  return op->f.renderCbFunc;
}

cmGrDistanceObjCb_t cmGrObjDistanceCbFunc( cmGrObjH_t oh )
{
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  return op->f.distanceCbFunc;
}

cmGrEventObjCb_t    cmGrObjEventCbFunc(    cmGrObjH_t oh )
{
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  return op->f.eventCbFunc;
}

cmGrVExtObjCb_t     cmGrObjVExtCbFunc(     cmGrObjH_t oh )
{
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  return op->f.vextCbFunc;
}

cmGrIsInsideObjCb_t cmGrObjIsInsideCbFunc( cmGrObjH_t oh )
{
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  return op->f.isInsideCbFunc;
}

void* cmGrObjCreateCbArg(   cmGrObjH_t oh )
{
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  return op->f.createCbArg;
}

void* cmGrObjDestroyCbArg(  cmGrObjH_t oh )
{
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  return op->f.createCbArg;
}

void* cmGrObjRenderCbArg(   cmGrObjH_t oh )
{
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  return op->f.destroyCbArg;
}

void* cmGrObjDistanceCbArg( cmGrObjH_t oh )
{
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  return op->f.distanceCbArg;
}

void* cmGrObjEventCbArg(    cmGrObjH_t oh )
{
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  return op->f.eventCbArg;
}

void* cmGrObjVExtCbArg(     cmGrObjH_t oh )
{
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  return op->f.vextCbArg;
}

void* cmGrObjIsInsideCbArg( cmGrObjH_t oh )
{
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  return op->f.isInsideCbArg;
}

void       cmGrObjLocalVExt( cmGrH_t h, cmGrObjH_t oh, cmGrVExt_t* vext )
{
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  return _cmGrObjCbVExt( _cmGrHandleToPtr(h), op, vext);  
}

cmGrObj_t* _cmGrObjIdToHandle( cmGrObj_t* op, unsigned id )
{
  cmGrObj_t* rp = NULL;
  if( op->id == id )
    return op;

  if( op->children != NULL )
    if((rp = _cmGrObjIdToHandle(op->children,id)) != NULL )
      return rp;

  if( op->rsib != NULL )
    if((rp = _cmGrObjIdToHandle(op->rsib,id)) != NULL )
      return rp;

  return NULL;
}

cmGrObjH_t cmGrObjIdToHandle( cmGrH_t h, unsigned id )
{
  cmGr_t*    p  = _cmGrHandleToPtr(h);
  cmGrObjH_t oh = cmGrObjNullHandle;
  cmGrObj_t* op;
  
  if((op = _cmGrObjIdToHandle(p->objs,id)) != NULL )
    oh.h                                    = op;
  return oh;
}

// Move 'aoH' such that it is above 'boH' in the z-order.
// This means that 'boH' must be drawn before 'aoH'.
// This algorithm is designed to not break object hierarchies
// when moving objects.  It achieves this by only moving
// the ancestor objects of boH and aoH at the level where
// they do not share a common ancestor.
void cmGrObjDrawAbove( cmGrObjH_t boH, cmGrObjH_t aoH )
{
  cmGrObj_t* bp = _cmGrObjHandleToPtr(boH);
  cmGrObj_t* ap = _cmGrObjHandleToPtr(aoH);
  cmGrObj_t* rp = bp;

  // set rp to the root object
  while( rp->parent != NULL )
    rp=rp->parent;

  while(1)
  {

    cmGrObj_t* a[] = {NULL,NULL};
    cmGrObj_t* bpp = NULL;
    cmGrObj_t* app = NULL;
    unsigned   i   = 0;

    rp = rp->children;
    for(; rp!=NULL; rp=rp->rsib)
    {
      if( bp==rp || _cmGrObjIsAncestor(rp,bp) )
      {
        assert( a[i]==NULL );
        bpp    = rp;
        a[i++] = rp;

        if( i==2 )
          break;
      }

      if( ap==rp || _cmGrObjIsAncestor(rp,ap) )
      {
        assert( a[i]==NULL );
        app    = rp;
        a[i++] = rp;

        if( i==2 )
          break;
      }
    }

    assert( rp != NULL && i==2 );
    
    // bpp and app share the same ancestor - keep looking
    // for the level where the they do not share same ancestor
    if( bpp == app )
      rp = bpp;
    else
    {
      // if bp is not already being drawn before ap
      if( a[0] != bpp )
      {
        _cmGrObjUnlink(app);
        _cmGrObjInsertOnRight(app,bpp,bpp->parent);
      }
      return;
    }
    
  }
  
}

void       cmGrObjReport(     cmGrH_t h, cmGrObjH_t oh, cmRpt_t* rpt )
{
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  _cmGrObjReport(_cmGrHandleToPtr(h),op,rpt);
}

void       cmGrObjReportR(    cmGrH_t h, cmGrObjH_t oh, cmRpt_t* rpt )
{
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  _cmGrObjReportR(_cmGrHandleToPtr(h),op,rpt);
}


//====================================================================================================
//====================================================================================================
#define _cmGr_X_VperP(p) (p->vext.sz.w     / (p->pext.sz.w-1) )
#define _cmGr_Y_VperP(p) (p->vext.sz.h     / (p->pext.sz.h-1) )
#define _cmGr_X_PperV(p) ((p->pext.sz.w-1) / p->vext.sz.w )
#define _cmGr_Y_PperV(p) ((p->pext.sz.h-1) / p->vext.sz.h )

int _cmGr_X_VtoP( cmGr_t* p, cmGrV_t vx )
{  return p->pext.loc.x + lround( (vx - p->vext.loc.x) * _cmGr_X_PperV(p)); }

int _cmGr_Y_VtoP(cmGr_t* p, cmGrV_t vy )
{ return p->pext.loc.y + (p->pext.sz.h-1) + lround(-(vy - p->vext.loc.y) * _cmGr_Y_PperV(p) ); }

cmGrV_t  _cmGr_X_PtoV( cmGr_t* p, int px )
{ return p->vext.loc.x + (px - p->pext.loc.x) * _cmGr_X_VperP(p); }

cmGrV_t _cmGr_Y_PtoV( cmGr_t* p, int py )
{ return  p->vext.loc.y + (p->pext.loc.y + (p->pext.sz.h-1) - py) * _cmGr_Y_VperP(p); }



#define _cmGrParentToLocalX( op, vext, x ) (op)->wext.loc.x + ((x) - (vext).loc.x) * (op)->wext.sz.w / (vext).sz.w
#define _cmGrParentToLocalY( op, vext, y ) (op)->wext.loc.y + ((y) - (vext).loc.y) * (op)->wext.sz.h / (vext).sz.h

#define _cmGrLocalToParentX( op, vext, x ) (vext).loc.x + ((x) - (op)->wext.loc.x) * (vext).sz.w / (op)->wext.sz.w
#define _cmGrLocalToParentY( op, vext, y ) (vext).loc.y + ((y) - (op)->wext.loc.y) * (vext).sz.h / (op)->wext.sz.h 


// On input x,y are in the same coord's as op->vext.
// On output pt is converted to op's internal coord system (i.e. the pt is inside op->wext)
// Using pt as the src and dst is always safe. (i.e. _cmGrLocalToParent(p,op,pt->x,pt->y,pt) is safe.)
bool _cmGrParentToLocal( cmGr_t* p, cmGrObj_t* op, cmGrV_t x, cmGrV_t y, cmGrVPt_t* pt )
{
  cmGrVExt_t vext;
  _cmGrObjCbVExt(p,op,&vext);

  if( cmGrVExtIsNullOrEmpty(&vext) )
    return false;
    
  pt->x = _cmGrParentToLocalX( op, vext, x);
  pt->y = _cmGrParentToLocalY( op, vext, y );

  //pt->x = op->wext.loc.x + (x - vext.loc.x) * op->wext.sz.w / vext.sz.w;
  //pt->y = op->wext.loc.y + (y - vext.loc.y) * op->wext.sz.h / vext.sz.h;
  return true;
}


// On input x,y are in the same coords as op->wext.
// On output pt is converted to be in the same coord's as op->vext (i.e.the pt is inside op->parent->wext)
// Using pt as the src and dst is always safe. (i.e. _cmGrLocalToParent(p,op,pt->x,pt->y,pt) is safe.)
void _cmGrLocalToParent( cmGr_t* p, cmGrObj_t* op, cmGrV_t x, cmGrV_t y, cmGrVPt_t* pt )
{
  cmGrVExt_t vext;
  _cmGrObjCbVExt(p,op,&vext);

  pt->x = _cmGrLocalToParentX(op,vext, x);
  pt->y = _cmGrLocalToParentY(op,vext, y);

  //pt->x = vext.loc.x + (x - op->wext.loc.x) * vext.sz.w / op->wext.sz.w;
  //pt->y = vext.loc.y + (y - op->wext.loc.y) * vext.sz.h / op->wext.sz.h; 
}

// On input x is in coord's inside op->wext.
// Return is in phys coord's
int _cmGrX_VtoP( cmGr_t* p, cmGrObj_t* op, cmGrV_t x )
{
  cmGrVExt_t vext;

  for(; op->parent != NULL; op=op->parent )
  {
    _cmGrObjCbVExt(p,op,&vext);
    x = _cmGrLocalToParentX(op->parent,vext,x);
  }

  return _cmGr_X_VtoP(p,x);
}

// On input y is in coord's inside op->wext.
// Return is in phys coord's
int _cmGrY_VtoP( cmGr_t* p, cmGrObj_t* op, cmGrV_t y )
{
  cmGrVExt_t vext;

  for(; op->parent != NULL; op=op->parent )
  {
    _cmGrObjCbVExt(p,op,&vext);
    y = _cmGrLocalToParentY(op->parent,vext,y);
  }

  return _cmGr_Y_VtoP(p,y);
}

// On input x,y are coord's inside op->wext.
// On output rp is in physical coord's
void  _cmGrXY_VtoP( cmGr_t* p, cmGrObj_t* op, cmGrV_t x, cmGrV_t y, cmGrPPt_t* rp )
{
  cmGrVPt_t pt;
  cmGrVPtSet(&pt,x,y);

  for(; op->parent != NULL; op=op->parent )
    _cmGrLocalToParent(p,op->parent,pt.x,pt.y,&pt);

  rp->x = _cmGr_X_VtoP(p,pt.x);
  rp->y = _cmGr_Y_VtoP(p,pt.y);
}

// pt is converted from the root obj coord system to be in the same coord's
// as op->vext (inside of op->parent->wext)
void _cmGrXY_GlobalToLocal( cmGr_t* p, cmGrObj_t* op, cmGrVPt_t* pt )
{
  if( op->parent != NULL )
    _cmGrXY_GlobalToLocal(p,op->parent,pt);

  // convert from parent coord to child coords
  _cmGrParentToLocal(p,op,pt->x,pt->y,pt);
  
}


// On input x,y are in physical coordindates.
// On output rp is inside op->parent->wext (the same coord's as op->vext)
void _cmGrXY_PtoV( cmGr_t* p, cmGrObj_t* op, int px, int py, cmGrVPt_t* rp )
{
  // convert the phys points to points in the root coord system
  rp->x = _cmGr_X_PtoV(p,px);
  rp->y = _cmGr_Y_PtoV(p,py);
  
  _cmGrXY_GlobalToLocal(p, op, rp ); 
}

int  cmGrX_VtoP(     cmGrH_t hh, cmGrObjH_t oh, cmGrV_t x )
{
  cmGr_t*    p  = _cmGrHandleToPtr(hh);
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  return _cmGrX_VtoP(p,op,x);
}

int  cmGrY_VtoP(     cmGrH_t hh, cmGrObjH_t oh, cmGrV_t y )
{
  cmGr_t*    p  = _cmGrHandleToPtr(hh);
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  return _cmGrY_VtoP(p,op,y);
}


void     cmGrXY_VtoP( cmGrH_t h, cmGrObjH_t oh, cmGrV_t x, cmGrV_t y, cmGrPPt_t* rp )
{ _cmGrXY_VtoP(_cmGrHandleToPtr(h), _cmGrObjHandleToPtr(oh), x, y, rp ); }

void     cmGrXYWH_VtoP(  cmGrH_t hh, cmGrObjH_t oh, cmGrV_t x, cmGrV_t y, cmGrV_t w, cmGrV_t h, cmGrPExt_t* pext )
{
  cmGr_t*    p  = _cmGrHandleToPtr(hh);
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  cmGrPPt_t  pt0,pt1;
  _cmGrXY_VtoP(p,op,x,  y,  &pt0);
  _cmGrXY_VtoP(p,op,x+w,y+h,&pt1);
  cmGrPExtSetD(pext,pt0.x,pt0.y,pt1.x,pt1.y);
}

void     cmGrVExt_VtoP(  cmGrH_t hh, cmGrObjH_t oh, const cmGrVExt_t* vext, cmGrPExt_t* pext )
{ cmGrXYWH_VtoP(hh,oh,vext->loc.x,vext->loc.y,vext->sz.w,vext->sz.h,pext); }

void     cmGrXY_PtoV( cmGrH_t h, cmGrObjH_t oh, int x, int y, cmGrVPt_t* rp )
{ _cmGrXY_PtoV(_cmGrHandleToPtr(h), _cmGrObjHandleToPtr(oh), x, y, rp ); }

void     cmGrXYWH_PtoV(  cmGrH_t hh, cmGrObjH_t oh, int x, int y, int w, int h, cmGrVExt_t* vext )
{
  cmGr_t*    p  = _cmGrHandleToPtr(hh);
  cmGrObj_t* op = _cmGrObjHandleToPtr(oh);
  cmGrVPt_t  pt0,pt1;
  _cmGrXY_PtoV(p,op,x,y,&pt0);
  _cmGrXY_PtoV(p,op,x+w-1,y+h-1,&pt1);
  cmGrVExtSetD(vext,pt0.x,pt0.y,pt1.x,pt1.y);
}

void     cmGrPExt_PtoV(  cmGrH_t hh, cmGrObjH_t oh, const cmGrPExt_t* pext, cmGrVExt_t* vext )
{ cmGrXYWH_PtoV(hh,oh,pext->loc.x,pext->loc.y,pext->sz.w,pext->sz.h,vext); }


  void     cmGrDrawVLine( cmGrH_t h, cmGrDcH_t dcH, cmGrObjH_t oh, cmGrV_t x0, cmGrV_t y0, cmGrV_t x1, cmGrV_t y1 )
{
  cmGrPPt_t loc0;
  cmGrPPt_t loc1;

  cmGrXY_VtoP(h,oh,x0,y0,&loc0);
  cmGrXY_VtoP(h,oh,x1,y1,&loc1);
  cmGrDcDrawLine(dcH,loc0.x,loc0.y,loc1.x,loc1.y);
}

void    cmGrDrawVRect( cmGrH_t hh, cmGrDcH_t dcH, cmGrObjH_t oh, cmGrV_t x, cmGrV_t y, cmGrV_t w, cmGrV_t h )
{
  cmGrDrawVLine(hh,dcH,oh,x,  y,  x,  y+h);
  cmGrDrawVLine(hh,dcH,oh,x,  y+h,x+w,y+h);
  cmGrDrawVLine(hh,dcH,oh,x+w,y+h,x+w,y);
  cmGrDrawVLine(hh,dcH,oh,x+w,y,  x,  y);
}


//====================================================================================================
//====================================================================================================

cmGrRC_t _cmGrDestroy( cmGr_t* p )
{
  cmGrRC_t rc = kOkGrRC;

  if( p->objs != NULL )
  {
    // destroy the root object and all of it's children
    if((rc = _cmGrObjDestroy(p,p->objs)) != kOkGrRC )
      return rc;
  }

  cmGrSync_t* sp = p->syncs;
  while( sp != NULL )
  {
    cmGrH_t  h;
    h.h  = p;

    cmGrSync_t* np = sp->link;
    // break sync with any mutually sync'ed gr's to prevent 
    // attempted calls to this gr.
    cmGrSetSync( sp->grH, h, 0 );
    cmMemFree(sp);
    sp = np;
  }

  p->syncs = NULL;

  cmLHeapDestroy(&p->lhH);

  cmMemPtrFree(&p->img);

  cmMemFree(p);

  return rc;
}

void     _cmGrObjRootVExt( cmGrObjFuncArgs_t* args, cmGrVExt_t* vext )
{
  // the root object's virtual extent is the same as it's world extent
  cmGrObjWorldExt( args->objH, vext );
}

bool _cmGrObjRootIsInside( cmGrObjFuncArgs_t* args, unsigned evtFlags, int px, int py, cmGrV_t vx, cmGrV_t vy )
{
  cmGrVExt_t vext;
  _cmGrObjRootVExt(args,&vext);
  return cmGrVExtIsXyInside(&vext,vx,vy);
}

void _cmGrSetCfgFlags( cmGr_t* p, unsigned flags )
{
  p->cfgFlags = flags;

  // kSelectHorzFl and kSelectVertFl are mutually exclusive
  if( cmIsFlag(p->cfgFlags,kSelectHorzGrFl) )
    p->cfgFlags = cmClrFlag(p->cfgFlags,kSelectVertGrFl);

}

void _cmGrCallback( cmGr_t* p, cmGrCbId_t id, unsigned eventFlags, cmGrKeyCodeId_t keycode )
{
  if( p->cbFunc != NULL )
  {
    cmGrH_t h;
    h.h = p;
    p->cbFunc(p->cbArg,h,id,eventFlags,keycode);
  }
}


cmGrRC_t cmGrCreate( cmCtx_t* ctx, cmGrH_t* hp, unsigned id, unsigned cfgFlags, cmGrCbFunc_t cbFunc, void* cbArg, const cmGrVExt_t* wext )
{
  cmGrRC_t rc                 = kOkGrRC;
  cmGrVExt_t null_ext;
  cmGrObjH_t rootObjH = cmGrObjNullHandle;

  if(( rc = cmGrDestroy(hp)) != kOkGrRC )
    return rc;

  // create the cmGr_t
  cmGr_t* p     = cmMemAllocZ(cmGr_t,1);
  p->ctx        = *ctx;
  p->id         = id;
  p->stateFlags = kDirtyGrFl;
  p->cbFunc     = cbFunc;
  p->cbArg      = cbArg;

  _cmGrSetCfgFlags(p,cfgFlags);

  if( wext == NULL )
  {
    cmGrVExtSetNull(&null_ext);
    wext = &null_ext;
  }

  cmErrSetup(&p->err,&ctx->rpt,"cmGr");

  if( cmLHeapIsValid( p->lhH = cmLHeapCreate(8192,ctx)) == false )
  {
    rc = cmErrMsg(&p->err,kLHeapFailGrRC,"Linked heap create failed.");
    goto errLabel;
  }

  cmGrVExtSetEmpty(&p->vext);
  cmGrPExtSetEmpty(&p->pext);
  
  hp->h = p;

  // create the root object  
  cmGrObjFunc_t funcs;
  memset(&funcs,0,sizeof(funcs));
  funcs.vextCbFunc     = _cmGrObjRootVExt;
  funcs.isInsideCbFunc = _cmGrObjRootIsInside;

  if((rc = cmGrObjCreate(*hp, &rootObjH, cmGrObjNullHandle, &funcs, cmInvalidId, 0, wext )) != kOkGrRC )
  {
    rc = cmErrMsg(&p->err,kRootObjCreateFailGrRC,"The root object creation failed.");
    goto errLabel;
  }

  // create the default color map
  _cmGrRgbInitDefaultColorMap(p);

  p->objs    = _cmGrObjHandleToPtr(rootObjH);
  p->rootObj = p->objs;

  _cmGrCallback(p,kCreateCbGrId,0,kInvalidKeyCodeGrId);

 errLabel:
  if( rc != kOkGrRC )
  {
    _cmGrDestroy(p);
    hp->h = NULL;
  }

  return rc;
}

cmGrRC_t cmGrDestroy( cmGrH_t* hp )
{
  cmGrRC_t rc                       = kOkGrRC;
  if( hp==NULL || cmGrIsValid(*hp) == false )
    return kOkGrRC;

  cmGr_t* p = _cmGrHandleToPtr(*hp);
  
  if((rc = _cmGrDestroy(p)) != kOkGrRC )
    goto errLabel;

  hp->h = NULL;
  
 errLabel:
  return rc;
}

// Destroy all objects (except the root object)
cmGrRC_t   _cmGrObjDestroyAll( cmGr_t* p )
{
  cmGrObj_t* op = p->objs;
  if( op != NULL )
  {
    op = op->children;
    while( op != NULL )
    {
      cmGrObj_t* np = op->rsib;
      _cmGrObjUnlinkAndFree(p,op);
      op            = np;
    }
  }
  return kOkGrRC;
}


cmGrRC_t cmGrClear( cmGrH_t h )
{
  cmGrRC_t rc;
  cmGr_t*    p = _cmGrHandleToPtr(h);

  if((rc = _cmGrObjDestroyAll(p)) != kOkGrRC )
    return rc;
  
  p->rootObj    = p->objs;
  p->stateFlags = kDirtyGrFl;

  cmGrVExtSetEmpty(&p->vext);

  p->msDnObj = NULL;
  cmGrPPtSet(&p->msDnPPt,0,0);
  cmGrVPtSet(&p->msDnVPt,0,0);
  cmGrVPtSet(&p->msVPt,0,0);
  cmGrVSzSet(&p->msDnVOffs,0,0);
  
  p->selValidFl = false;
  cmGrVPtSet(&p->sel0Pt,0,0);
  cmGrVPtSet(&p->sel1Pt,0,0);

  cmGrVPtSet(&p->localPt,0,0);
  cmGrVPtSet(&p->globalPt,0,0);

  cmGrVExtSetNull(&p->rootObj->wext);
  return rc;
}


cmGrObjH_t cmGrRootObjH( cmGrH_t h )
{
  cmGr_t*    p = _cmGrHandleToPtr(h);
  cmGrObjH_t oh;
  oh.h = p->rootObj;
  return oh;
}

unsigned cmGrCfgFlags( cmGrH_t h )
{
  cmGr_t* p = _cmGrHandleToPtr(h);
  return p->cfgFlags;
}

void     cmGrSetCfgFlags( cmGrH_t h, unsigned cfgFlags )
{
  cmGr_t* p = _cmGrHandleToPtr(h);
  _cmGrSetCfgFlags(p,cfgFlags);
}


void _cmGrObjDraw( cmGr_t* p, cmGrObj_t* op, cmGrDcH_t dcH )
{
  _cmGrObjCbRender(p,dcH,op);

  if( op->children != NULL )
    _cmGrObjDraw(p,op->children,dcH);

  if( op->rsib != NULL )
    _cmGrObjDraw(p,op->rsib,dcH);
  
}

void _cmInvertImage( cmGr_t* p, cmGrDcH_t dcH, const cmGrPExt_t* pext )
{
  if( cmGrPExtIsNullOrEmpty(pext) )
    return;

  cmGrPExt_t  r;
  cmGrPExtIntersect( &r, pext, &p->pext );

  unsigned       n   = r.sz.w * r.sz.h * 3;

  if( n > 0 )
  {
    unsigned       i;
    p->img = cmMemResizeZ(unsigned char,p->img,n);

    //p->dd.read_image(p->ddUser, p->img, r.loc.x, r.loc.y, r.sz.w, r.sz.h );
    cmGrDcReadImage( dcH, p->img, &r );

    for(i=0; i<n; ++i)
      p->img[i] = 255 - p->img[i];

    //p->dd.draw_image(p->ddUser, p->img, r.loc.x, r.loc.y, r.sz.w, r.sz.h);
    cmGrDcDrawImage( dcH, p->img, &r);
  }
}

cmGrRC_t cmGrDraw( cmGrH_t h, cmGrDcH_t dcH )
{
  cmGr_t*           p = _cmGrHandleToPtr(h);
  _cmGrObjDraw(p,p->rootObj,dcH);

  cmGrPExt_t pext;
  cmGrVExt_t vext;
  cmGrSelectExtents(h,&vext,&pext);

  if( cmGrPExtIsNotNull(&pext))
  {
  
    if( pext.sz.w<=1 && pext.sz.h<=1 )
    {
      cmGrPExt_t ipext;

      if( !cmIsFlag(p->cfgFlags,kSelectHorzGrFl) )
      {
        cmGrPExtSet(&ipext,p->pext.loc.x,pext.loc.y,p->pext.sz.w,1);
        _cmInvertImage(p,dcH,&ipext);
      }
      
      if( !cmIsFlag(p->cfgFlags,kSelectVertGrFl) )
      {
        cmGrPExtSet(&ipext,pext.loc.x,p->pext.loc.y,1,p->pext.sz.h);
        _cmInvertImage(p,dcH,&ipext);
      }
    }
    else
    {
      _cmInvertImage(p,dcH,&pext);
    }
  }

  return kOkGrRC;
}

bool     _cmGrSetViewExtents( cmGr_t* p, cmGrV_t minx, cmGrV_t miny, cmGrV_t maxx, cmGrV_t maxy )
{
  cmGrVExt_t vext;

  // load a vext with the new view extents
  cmGrVExtSetD(&vext,minx,miny,maxx,maxy);

  // if the view ext is not changing
  if( cmGrVExtIsEqual(&p->vext,&vext) )
    return false;

  //  the root object must exist
  assert( p->rootObj != NULL );

  // the view extents must be in the world extents
  if( cmGrVExtIsExtInside(&p->rootObj->wext,&vext)==false )
  {
    cmErrMsg(&p->err,kInvalidCoordsGrRC,"View extent is not inside the world extents.");
    return false;
  }

  //cmGrVExtPrint("set vw:",&vext);

  p->vext       =  vext;
  p->stateFlags = cmSetFlag(p->stateFlags,kDirtyGrFl);

  _cmGrCallback(p, kViewExtCbGrId,0,kInvalidKeyCodeGrId );

  // apply changes to synch target
  cmGrSync_t* sp = p->syncs;
  for(; sp!=NULL; sp=sp->link)
    if( cmIsFlag(sp->flags,kViewSyncGrFl) )
    {
      cmGrViewExtents( sp->grH, &vext );
      bool fl = false;

      if( cmIsFlag(sp->flags,kHorzSyncGrFl) )
      {
        fl         = true;
        vext.loc.x = p->vext.loc.x;
        vext.sz.w  = p->vext.sz.w;
      }


      if( cmIsFlag(sp->flags,kVertSyncGrFl) )
      {
        fl         = true;
        vext.loc.y = p->vext.loc.y;
        vext.sz.h  = p->vext.sz.h;
      }
      
      if( fl )
        cmGrSetViewExtentsE( sp->grH, &vext );
    }
  //printf("s:%p %f %f %f %f\n",p,p->vext.loc.x, p->vext.loc.y, p->vext.sz.w, p->vext.sz.h );

  return true;
}

bool      _cmGrSetScrollH( cmGr_t* p, int x )
{
  assert( p->rootObj != NULL );
  cmGrV_t vx = p->rootObj->wext.loc.x + (x * p->vext.sz.w / p->pext.sz.w);
  
  return  _cmGrSetViewExtents(p,vx, p->vext.loc.y, vx+p->vext.sz.w, p->vext.loc.y+p->vext.sz.h);
}

int _cmGrScrollH( cmGr_t* p )
{ return round((p->vext.loc.x - p->rootObj->wext.loc.x) * p->pext.sz.w / p->vext.sz.w); }

bool      _cmGrSetScrollV( cmGr_t* p, int y )
{
  assert( p->rootObj != NULL );
  cmGrV_t vy = p->rootObj->wext.loc.y + (y * p->vext.sz.h / p->pext.sz.h);
  
  return  _cmGrSetViewExtents(p, p->vext.loc.x, vy, p->vext.loc.x+p->vext.sz.w, vy + p->vext.sz.h);
}

int _cmGrScrollV( cmGr_t* p )
{ return round((p->vext.loc.y - p->rootObj->wext.loc.y) * p->pext.sz.h / p->vext.sz.h); }

void  _cmGrSetSelectPoints(cmGr_t* p, const cmGrVPt_t* pt0, const cmGrVPt_t* pt1)
{
  bool deltaFl = false;

  p->selValidFl = true;

  if( pt0 != NULL )
  {
    if( cmGrVPtIsNotEqual(&p->sel0Pt,pt0) )
    {
      p->sel0Pt = *pt0;
      deltaFl   = true;
    }

    if( pt1 == NULL && cmGrVPtIsNotEqual(&p->sel1Pt,pt0) )
    {
      p->sel1Pt = *pt0;
      deltaFl   = true;
    }
  }

  if( pt1 != NULL && cmGrVPtIsNotEqual(&p->sel1Pt,pt1) )
  {
    p->sel1Pt = *pt1;
    deltaFl   = true;
  }

  if( !deltaFl )
    return;

  _cmGrCallback(p,kSelectExtCbGrId,0,kInvalidKeyCodeGrId);

  // apply changes to synch target
  cmGrSync_t* sp = p->syncs;
  for(; sp!=NULL; sp=sp->link)
    if( cmIsFlag(sp->flags,kSelectSyncGrFl) )
    {
      cmGrVPt_t pt0, pt1;
      cmGrSelectPoints(sp->grH, &pt0, &pt1 );


      bool fl = false;

      if( cmIsFlag(sp->flags,kHorzSyncGrFl) )
      {
        fl         = true;
        pt0.x = p->sel0Pt.x;
        pt1.x = p->sel1Pt.x;
      }

      if( cmIsFlag(sp->flags,kVertSyncGrFl) )
      {
        fl         = true;
        pt0.y = p->sel0Pt.y;
        pt1.y = p->sel1Pt.y;
      }
      
      if( fl )
        cmGrSetSelectPoints( sp->grH, &pt0, &pt1 );
    }

  
}

void      _cmGrScrollExtents( cmGr_t* p, cmGrPSz_t* tot, cmGrPSz_t* vis, cmGrPSz_t* max, cmGrPPt_t* pos )
{

  assert( p->rootObj != NULL );

  if( tot != NULL )
  {
    tot->w = round(p->rootObj->wext.sz.w * p->pext.sz.w / p->vext.sz.w);
    tot->h = round(p->rootObj->wext.sz.h * p->pext.sz.h / p->vext.sz.h);
  }

  if( vis != NULL )
  {
    vis->w = round(p->vext.sz.w * p->pext.sz.w / p->vext.sz.w);
    vis->h = round(p->vext.sz.h * p->pext.sz.h / p->vext.sz.h);
  }

  if( max != NULL )
  {
    max->w = tot->w - vis->w;
    max->h = tot->h - vis->h;
  }

  if( pos != NULL )
  {
    pos->x = _cmGrScrollH(p);
    pos->y = _cmGrScrollV(p);
  }
}

// vx,vy are in the same coord's as op->vext
cmGrObj_t* _cmGrFindObjRec( cmGr_t* p, cmGrObj_t* op, unsigned evtFlags, int px, int py, cmGrV_t vx, cmGrV_t vy )
{
  cmGrObj_t* rop = NULL;
  cmGrObj_t* top;
  cmGrVExt_t vext;

  // get the location of op inside op->parent->wext
  _cmGrObjCbVExt(p,op,&vext);

  // is vx,vy inside op - this is equiv to: cmGrVExtIsXyInside(&vext,vx,vy)
  if( _cmGrObjCbIsInside(p,op,evtFlags,px,py,vx,vy) )
    rop = op;

  if( op->children != NULL )
  {
    cmGrVPt_t pt;
    if( _cmGrParentToLocal(p,op,vx,vy,&pt) )
      if((top = _cmGrFindObjRec(p,op->children,evtFlags,px,py,vx,vy)) != NULL )
        rop = top;
  }

  if( op->rsib != NULL )
    if((top = _cmGrFindObjRec(p,op->rsib,evtFlags,px,py,vx,vy)) != NULL )
      rop = top;


  return rop;
}

cmGrObj_t*  _cmGrEventMsDown( cmGr_t* p, unsigned evtFlags, cmGrKeyCodeId_t key, int px, int py, cmGrV_t gx, cmGrV_t gy )
{
  // store the phys loc. of the ms dn event
  cmGrPPtSet(&p->msDnPPt,px,py);   

  // get a pointer to an object 
  cmGrObj_t* op =  _cmGrFindObjRec(p, p->rootObj, evtFlags, px, py, gx, gy );

  // if the mouse did not go down in an object that accepts mouse down events
  // or the object was a root object
  if( op == NULL || op->parent == NULL )
    return NULL;

  // store the object and coord's where the mouse went down.
  cmGrVExt_t vext;
  p->msDnObj = op; // set the msDnObj

  // convert the phys ms dn point to the virt point inside op->parent.wext
  _cmGrXY_PtoV(p, op->parent, px, py, &p->msDnVPt );

  p->msVPt    = p->msDnVPt;    // set the current ms virt pt
  p->localPt  = p->msDnVPt;    // set the current local pt

  // notifiy the app of the local coord's
  _cmGrCallback(p,kLocalPtCbGrId,0,kInvalidKeyCodeGrId);

  // get the ms down obj virt extents
  _cmGrObjCbVExt(p,op,&vext);

  // store the offset from the lower left to the ms dn point
  p->msDnVOffs.w = p->msDnVPt.x - vext.loc.x; 
  p->msDnVOffs.h = p->msDnVPt.y - vext.loc.y;

  // notify the object that the mouse went down
  if(_cmGrObjCbEvent(p,op,evtFlags,key,px,py))
    return op;
  return NULL;
}

cmGrObj_t* _cmGrEventMsUp( cmGr_t* p, unsigned evtFlags, cmGrKeyCodeId_t key, int px, int py, cmGrV_t gx, cmGrV_t gy )
{
  bool       fl = false;
  cmGrObj_t* op = NULL;

  cmGrPPt_t upPPt;
  cmGrPPtSet(&upPPt,px,py);
  
  // if a click occured ...
  if( cmGrPPtIsEqual(&p->msDnPPt,&upPPt )  )
  {
    // ... and the click is in a non-root object which accepts click events ...
    if( p->msDnObj!= NULL && p->msDnObj->parent!=NULL && _cmGrObjCbIsInside(p,p->msDnObj,evtFlags | kMsClickGrFl, px, py, gx, gy) )
    {
      // ... then generate a click event
      unsigned newEvtFlags = cmClrFlag(evtFlags,kMsUpGrFl) | kMsClickGrFl;
      fl = _cmGrObjCbEvent(p,p->msDnObj,newEvtFlags,key,px,py);
      op = p->msDnObj;
    }
    else // ... else if the click occurred in the root object
      //if( p->msDnObj==NULL || p->msDnObj->parent==NULL)
      {
        cmGrVPt_t pt;
        cmGrVPtSet(&pt,gx,gy);

        bool shFl = cmIsFlag(evtFlags,kShiftKeyGrFl);
        _cmGrSetSelectPoints( p, shFl ? NULL : &pt, shFl ? &pt : NULL );

      }
  }

  // if op is NULL then there was no-mouse down object to match with 
  // this mouse-up event - find an object to match with the mouse-up event
  if( op == NULL )
    op = _cmGrFindObjRec(p, p->rootObj, evtFlags, px, py, gx, gy );

  // if a mouse-up object was found then
  if( op != NULL && op->parent != NULL)
  {
    
    // notify the object under the mouse that the mouse went up
    if( _cmGrObjCbEvent(p,op,evtFlags,key,px,py) )
      fl = true;

    // convert the phys ms dn point to the virt point inside op->parent.wext
    _cmGrXY_PtoV(p, op->parent, px, py, &p->msVPt );

  }

  _cmGrCallback(p,kFocusCbGrId,0,kInvalidKeyCodeGrId);

  p->msDnObj = NULL;

  return fl ? op : NULL;
}

cmGrObj_t*  _cmGrEventMsMove( cmGr_t* p, unsigned evtFlags, cmGrKeyCodeId_t key, int px, int py, cmGrV_t gx, cmGrV_t gy )
{
  bool       fl = false;
  cmGrObj_t* op = _cmGrFindObjRec(p, p->rootObj, evtFlags, px, py, gx, gy );

  if( op != NULL && op->parent != NULL )
  {
    
    fl = _cmGrObjCbEvent(p,op,evtFlags,key,px,py);
  }

  return fl ? op : NULL;
}

cmGrObj_t*  _cmGrEventMsDrag( cmGr_t* p, unsigned evtFlags, cmGrKeyCodeId_t key, int px, int py, cmGrV_t gx, cmGrV_t gy )
{
  bool       fl = false;
  cmGrObj_t* op = _cmGrFindObjRec(p, p->rootObj, evtFlags, px, py, gx, gy );

  if( op != NULL && p->msDnObj != NULL && p->msDnObj->parent != NULL )
  {
    // set the current virtual point
    _cmGrXY_PtoV(p, p->msDnObj->parent, px, py, &p->msVPt );

    // callback the dragged object
    fl = _cmGrObjCbEvent(p,p->msDnObj,evtFlags,key,px,py);

    // if the px,py is outside the root phys extents then scroll
    if( !cmGrPExtIsXyInside(&p->pext,px,py) )
    {
      bool      hFl = false, vFl=false;
      cmGrPSz_t tot,vis,max;
      cmGrPPt_t pos;
      _cmGrScrollExtents(p, &tot, &vis, &max, &pos );

      if( px < cmGrPExtL(&p->pext) )
        hFl = _cmGrSetScrollH(p,   cmMax(0,     _cmGrScrollH(p) - (cmGrPExtL(&p->pext) - px)));
      else
        if( px > cmGrPExtR(&p->pext) )
          hFl = _cmGrSetScrollH(p, cmMin(max.w, _cmGrScrollH(p) + (px - cmGrPExtR(&p->pext))));            

      if( py < cmGrPExtT(&p->pext) )
        vFl = _cmGrSetScrollV(p,   cmMax(0,     _cmGrScrollV(p) - (cmGrPExtT(&p->pext) - py)));
      else
        if( py > cmGrPExtB(&p->pext) )
          vFl = _cmGrSetScrollV(p, cmMin(max.h, _cmGrScrollV(p) + (py - cmGrPExtB(&p->pext))));            
              

      fl = fl || vFl || hFl;
    }
  }
  return fl ? op : NULL;
}

bool     cmGrEvent( cmGrH_t h, unsigned evtFlags, cmGrKeyCodeId_t key, int px, int py )
{
  bool              fl = false;
  cmGrObj_t*        op = NULL;
  cmGr_t*           p = _cmGrHandleToPtr(h);
  cmGrVPtSet(&p->localPt,0,0);

  // convert the phys points to points in the root coord system
  cmGrV_t gx = _cmGr_X_PtoV(p,px);
  cmGrV_t gy = _cmGr_Y_PtoV(p,py);

  // set the global point
  cmGrVPtSet(&p->globalPt,gx,gy);

  // inform the app of the possibly new global point
  _cmGrCallback(p,kGlobalPtCbGrId,0,kInvalidKeyCodeGrId);
  
  switch( evtFlags & kEvtMask)
  {
    case kKeyUpGrFl:
      _cmGrCallback(p,kKeyUpCbGrId,evtFlags,key);
      break;

    case kKeyDnGrFl:
      _cmGrCallback(p,kKeyDnCbGrId,evtFlags,key);
      break;

    case kMsDownGrFl:      
      op = _cmGrEventMsDown(p,evtFlags,key,px,py,gx,gy);
      break;

    case kMsUpGrFl: // handle mouse-up, mouse-click, and focus events
      op = _cmGrEventMsUp(p,evtFlags,key,px,py,gx,gy);
      break;

    case kMsMoveGrFl:
      op = _cmGrEventMsMove(p,evtFlags,key,px,py,gx,gy);
      break;

    case kMsDragGrFl:
      op = _cmGrEventMsDrag(p,evtFlags,key,px,py,gx,gy);
      break;

  }

  if( op != NULL )
  {
    // convert gx,gy to be inside op->wext
    cmGrVPtSet(&p->localPt,gx,gy);
    _cmGrXY_GlobalToLocal(p,op,&p->localPt);

    _cmGrCallback(p,kLocalPtCbGrId,0,kInvalidKeyCodeGrId);

    fl = true;
  }

  return fl;
}

bool     cmGrEvent1( cmGrH_t h, unsigned flags, cmGrKeyCodeId_t key, int px, int py )
{
  bool              fl = false;
  cmGr_t*           p = _cmGrHandleToPtr(h);
  cmGrObj_t*        op;
  cmGrVPtSet(&p->localPt,0,0);

  // convert the phys points to points in the root coord system
  cmGrV_t gx = _cmGr_X_PtoV(p,px);
  cmGrV_t gy = _cmGr_Y_PtoV(p,py);

  cmGrVPtSet(&p->globalPt,gx,gy);

  _cmGrCallback(p,kGlobalPtCbGrId,0,kInvalidKeyCodeGrId);

  // find the obj under the mouse
  if((op = _cmGrFindObjRec(p,p->rootObj,flags&kEvtMask,px,py,gx,gy)) != NULL )
  {          
    // convert gx,gy to be inside op->wext
    cmGrVPtSet(&p->localPt,gx,gy);
    _cmGrXY_GlobalToLocal(p,op,&p->localPt);

    _cmGrCallback(p,kLocalPtCbGrId,0,kInvalidKeyCodeGrId);

  }
  
  if( (flags&kEvtMask)==kKeyUpGrFl )
    _cmGrCallback(p,kKeyUpCbGrId,flags,key);

  if( (flags&kEvtMask)==kKeyDnGrFl )
    _cmGrCallback(p,kKeyDnCbGrId,flags,key);

  if( op != NULL )
  {
    switch( flags & kEvtMask )
    {
      case kMsDownGrFl:

        // store the phys loc. of the ms dn event
        cmGrPPtSet(&p->msDnPPt,px,py);   

        if( op != NULL  )
        {
          // if the click was in not in the root object
          if( op->parent != NULL )
          {
            // store the object and coord's where the mouse went down.

            cmGrVExt_t vext;
            p->msDnObj = op; // set the msDnObj

            // convert the phys ms dn point to the virt point inside op->parent.wext
            _cmGrXY_PtoV(p, op->parent, px, py, &p->msDnVPt );

            // set the current ms virt pt
            p->msVPt = p->msDnVPt;       

            // get the ms down obj virt extents
            _cmGrObjCbVExt(p,op,&vext);

            // store the offset from the lower left to the ms dn point
            p->msDnVOffs.w = p->msDnVPt.x - vext.loc.x; 
            p->msDnVOffs.h = p->msDnVPt.y - vext.loc.y;

            // notify the object that the mouse went down
            fl = _cmGrObjCbEvent(p,op,flags,key,px,py);
          }
        }
        break;
        
      case kMsUpGrFl:
        {
          cmGrPPt_t upPPt;
          cmGrPPtSet(&upPPt,px,py);

          bool clickFl = cmGrPPtIsEqual(&p->msDnPPt,&upPPt );

          // if a click occured ...
          if( clickFl  )
          {
            // ... and the click is in a non-root object ...
            if( p->msDnObj!= NULL && p->msDnObj->parent!=NULL )
            {
              // ... then generate a click event
              unsigned evtFlags = cmClrFlag(flags,kMsUpGrFl) | kMsClickGrFl;
              fl = _cmGrObjCbEvent(p,op,evtFlags,key,px,py);
            }
            else // ... else if the click occurred in the root object
            {
              cmGrVPt_t pt;
              cmGrVPtSet(&pt,gx,gy);

              bool shFl = cmIsFlag(flags,kShiftKeyGrFl);
              _cmGrSetSelectPoints( p, shFl ? NULL : &pt, shFl ? &pt : NULL );

            }

          }

          // notify the object under the mouse that the mouse went up
          if( _cmGrObjCbEvent(p,op,flags,key,px,py) )
            fl = true;

          _cmGrCallback(p,kFocusCbGrId,0,kInvalidKeyCodeGrId);

          p->msDnObj = NULL;
        }
        break;
        
      case kMsMoveGrFl:
        fl = _cmGrObjCbEvent(p,op,flags,key,px,py);
        break;
        
      case kMsWheelGrFl:
        break;
        
      case kMsDragGrFl:
        if( p->msDnObj != NULL  )
        {
          // set the current virtual point
          _cmGrXY_PtoV(p, p->msDnObj->parent, px, py, &p->msVPt );

          // callback the dragged object
          fl = _cmGrObjCbEvent(p,p->msDnObj,flags,key,px,py);

          // if the px,py is outside the root phys extents then scroll
          if( !cmGrPExtIsXyInside(&p->pext,px,py) )
          {
            bool      hFl = false, vFl=false;
            cmGrPSz_t tot,vis,max;
            cmGrPPt_t pos;
            cmGrScrollExtents(h, &tot, &vis, &max, &pos );

            if( px < cmGrPExtL(&p->pext) )
              hFl = _cmGrSetScrollH(p,   cmMax(0,     _cmGrScrollH(p) - (cmGrPExtL(&p->pext) - px)));
            else
              if( px > cmGrPExtR(&p->pext) )
                hFl = _cmGrSetScrollH(p, cmMin(max.w, _cmGrScrollH(p) + (px - cmGrPExtR(&p->pext))));            

            if( py < cmGrPExtT(&p->pext) )
              vFl = _cmGrSetScrollV(p,   cmMax(0,     _cmGrScrollV(p) - (cmGrPExtT(&p->pext) - py)));
            else
              if( py > cmGrPExtB(&p->pext) )
                vFl = _cmGrSetScrollV(p, cmMin(max.h, _cmGrScrollV(p) + (py - cmGrPExtB(&p->pext))));            
              

            fl = fl || vFl || hFl;
          }

        }
        break;

      case kKeyDnGrFl:
      case kKeyUpGrFl:
        //fl = _cmGrObjCbEvent(p,p->msDnObj,flags,key,px,py);
        break;
    }
  }

  return fl;
}


bool     cmGrIsValid( cmGrH_t h )
{ return h.h != NULL; }

unsigned cmGrId( cmGrH_t h )
{
  cmGr_t*    p    = _cmGrHandleToPtr(h);
  return p->id;
}

const cmGrVPt_t* cmGrGlobalPt( cmGrH_t h )
{
  cmGr_t*    p    = _cmGrHandleToPtr(h);
  return &p->globalPt;
}

const cmGrVPt_t* cmGrLocalPt( cmGrH_t h )
{
  cmGr_t*    p    = _cmGrHandleToPtr(h);
  return &p->localPt;
}


bool     cmGrSetViewExtents( cmGrH_t h, cmGrV_t minx, cmGrV_t miny, cmGrV_t maxx, cmGrV_t maxy )
{ return _cmGrSetViewExtents( _cmGrHandleToPtr(h), minx,miny,maxx,maxy); }

bool     cmGrSetViewExtentsE( cmGrH_t h, const cmGrVExt_t* e )
{ return cmGrSetViewExtents( h, cmGrVExtMinX(e), cmGrVExtMinY(e), cmGrVExtMaxX(e), cmGrVExtMaxY(e) ); }

void     cmGrViewExtents( cmGrH_t h, cmGrVExt_t* vext )
{
  cmGr_t* p = _cmGrHandleToPtr(h);
  *vext = p->vext;

  //printf("g0:%p %f %f %f %f\n",p,p->vext.loc.x, p->vext.loc.y, p->vext.sz.w, p->vext.sz.h );
  //printf("g1:%p %f %f %f %f\n",p,vext->loc.x, vext->loc.y, vext->sz.w, vext->sz.h );
}


bool     cmGrSetPhysExtents( cmGrH_t hh, int x, int y, int w, int h )
{
  cmGr_t*    p    = _cmGrHandleToPtr(hh);
  cmGrPExt_t pext;
  cmGrPExtSet(&pext,x,y,w,h);

  assert( cmGrPExtIsNull(&pext)==false );

  if( cmGrPExtIsEqual(&pext,&p->pext) )
    return false;

  p->pext = pext;
  p->stateFlags = cmSetFlag(p->stateFlags,kDirtyGrFl);

  _cmGrCallback(p,kPhysExtCbGrId,0,kInvalidKeyCodeGrId);

  //cmGrPExtPrint("pext",&p->pext);
  return true;
}

bool     cmGrSetPhysExtentsE(cmGrH_t h, const cmGrPExt_t* e )
{ return cmGrSetPhysExtents(h, cmGrPExtL(e), cmGrPExtT(e), cmGrPExtW(e), cmGrPExtH(e) );  }

void     cmGrPhysExtents( cmGrH_t h, cmGrPExt_t* exts )
{
  cmGr_t* p = _cmGrHandleToPtr(h);
  *exts = p->pext;
}
void      cmGrScrollExtents( cmGrH_t h, cmGrPSz_t* tot, cmGrPSz_t* vis, cmGrPSz_t* max, cmGrPPt_t* pos )
{
  cmGr_t* p = _cmGrHandleToPtr(h);
  _cmGrScrollExtents(p,tot,vis,max,pos);
}


bool      cmGrSetScrollH( cmGrH_t h, int x )
{ return _cmGrSetScrollH( _cmGrHandleToPtr(h), x ); }

int       cmGrScrollH( cmGrH_t h )
{ return _cmGrScrollH( _cmGrHandleToPtr(h) ); }

bool      cmGrSetScrollV( cmGrH_t h, int y )
{ return _cmGrSetScrollV( _cmGrHandleToPtr(h), y ); }

int       cmGrScrollV( cmGrH_t h )
{ return _cmGrScrollV( _cmGrHandleToPtr(h) ); }

bool  cmGrSelectExtents( cmGrH_t h, cmGrVExt_t* vext, cmGrPExt_t* pext )
{
  cmGrPPt_t pt0,pt1;
  cmGr_t* p = _cmGrHandleToPtr(h);

  if( !p->selValidFl )
  {
    cmGrVExtSetNull(vext);
    cmGrPExtSetNull(pext);
    return false;
  }

  if( p->sel0Pt.x == p->sel1Pt.x && p->sel0Pt.y == p->sel1Pt.y )
    cmGrVExtSet(vext, p->sel0Pt.x, p->sel0Pt.y, 0, 0);
  else
  {
    cmGrV_t gx0=p->sel0Pt.x, gy0=p->sel0Pt.y;
    cmGrV_t gx1=p->sel1Pt.x, gy1=p->sel1Pt.y;

    if( cmIsFlag(p->cfgFlags,kSelectHorzGrFl) )
    {
      gy0 = cmGrVExtMaxY(&p->rootObj->wext);
      gy1 = cmGrVExtMinY(&p->rootObj->wext);
    }
    else
      if( cmIsFlag(p->cfgFlags,kSelectVertGrFl ) )
      {
        gx0 = cmGrVExtMinX(&p->rootObj->wext);
        gx1 = cmGrVExtMaxX(&p->rootObj->wext);
      }

    cmGrVExtSetD(vext,cmMin(gx0,gx1),cmMin(gy0,gy1),cmMax(gx0,gx1),cmMax(gy0,gy1));

  }
  
  _cmGrXY_VtoP(p, p->rootObj, vext->loc.x, vext->loc.y, &pt0 );
  _cmGrXY_VtoP(p, p->rootObj, vext->loc.x + vext->sz.w, vext->loc.y + vext->sz.h, &pt1 );

  cmGrPExtSetD(pext,cmMin(pt0.x,pt1.x),cmMin(pt0.y,pt1.y),cmMax(pt0.x,pt1.x),cmMax(pt0.y,pt1.y));

  //printf("%f %f %f %f\n",vext->loc.x,vext->loc.y,vext->sz.w,vext->sz.h);
  //printf("%i %i %i %i\n",pext->loc.x,pext->loc.y,pext->sz.w,pext->sz.h);
  return true;
}

void  cmGrSetSelectPoints(  cmGrH_t h, const cmGrVPt_t* pt0, const cmGrVPt_t* pt1 )
{
  cmGr_t* p = _cmGrHandleToPtr(h);
  _cmGrSetSelectPoints(p,pt0,pt1);
}

void   cmGrSelectPoints(  cmGrH_t h, cmGrVPt_t* pt0, cmGrVPt_t* pt1 )
{
  cmGr_t* p = _cmGrHandleToPtr(h);

  if( pt0 != NULL )
    *pt0 = p->sel0Pt;

  if( pt1 != NULL )
    *pt1 = p->sel1Pt;
}


void  cmGrZoom( cmGrH_t h, unsigned flags )
{
  cmGr_t*    p     = _cmGrHandleToPtr(h);
  bool       hfl   = cmIsFlag(flags,kXAxisGrFl);
  bool       vfl   = cmIsFlag(flags,kYAxisGrFl);
  bool       inFl  = cmIsFlag(flags,kZoomInGrFl);
  bool       allFl = cmIsFlag(flags,kShowAllGrFl);
  double     magn  = 3.0;
  double     ratio = inFl ? 1.0/magn : magn;
  cmGrVPt_t  c;
  cmGrVExt_t z;
  cmGrVExt_t v;
  cmGrVExt_t w;

  cmGrVExtSetNull(&z);
  cmGrViewExtents( h,&v);  // get the current view extents
  cmGrObjWorldExt( cmGrRootObjH(h),&w);  // get the current world extents
  

  // if show all was requested ...
  if( allFl )
  {

    // .. then the world ext's become the view extents.
    cmGrVExtSetD(&z, 
      hfl ? cmGrVExtMinX(&w) : cmGrVExtMinX(&v),
      vfl ? cmGrVExtMinY(&w) : cmGrVExtMinY(&v),
      hfl ? cmGrVExtMaxX(&w) : cmGrVExtMaxX(&v), 
      vfl ? cmGrVExtMaxY(&w) : cmGrVExtMaxY(&v));
  }
  else 
  {
  
    // if the selection flag is not set or the selection pt/area is not valid
    if( cmIsNotFlag(flags,kSelectGrFl) || p->selValidFl==false )
    {
      // center the zoom on the current view
      c.x = v.loc.x + v.sz.w/2;
      c.y = v.loc.y + v.sz.h/2;
    }
    else
    {
      // if the selection area is a point
      if( p->sel0Pt.x == p->sel1Pt.x && p->sel0Pt.y == p->sel1Pt.y )
      {
        // center the zoom on the current view
        c.x = p->sel0Pt.x;
        c.y = p->sel1Pt.y;
      }
      else
      {
        cmGrPExt_t dum;
        
        // The selection area exist - make it the new view area
        // Note that f the selection area is greater than the 
        // current view area then this may not be a magnification.
        cmGrSelectExtents(h,&z,&dum);
      }
    }

    // if no zoom area has been created then create
    // one centered on 'c'.
    if( cmGrVExtIsNull(&z) )
    {
      cmGrVExt_t t;
      cmGrV_t zw = v.sz.w * ratio / 2;
      cmGrV_t zh = v.sz.h * ratio / 2;

      cmGrVExtSetD(&t, 
        hfl ? c.x-zw : cmGrVExtMinX(&v),
        vfl ? c.y-zh : cmGrVExtMinY(&v),
        hfl ? c.x+zw : cmGrVExtMaxX(&v), 
        vfl ? c.y+zh : cmGrVExtMaxY(&v));
      
      cmGrVExtIntersect(&z,&t,&w);
      
    }
  }

  //cmGrVExtPrint("w:",&w);
  //cmGrVExtPrint("z:",&z);
  //cmGrVExtPrint("v:",&v);
  assert( cmGrVExtIsNorm(&z));

  cmGrSetViewExtentsE(h,&z);
}

void cmGrSetSync( cmGrH_t h, cmGrH_t syncGrH, unsigned flags )
{
  cmGr_t* p = _cmGrHandleToPtr(h);

  // attempt to locate syncGrH on the sync list
  cmGrSync_t* pp = NULL;
  cmGrSync_t* sp = p->syncs;
  for(; sp != NULL; sp=sp->link)
  {
    if( cmHandlesAreEqual(sp->grH,syncGrH) )
      break;

    pp = sp;
  }

  // if the handle was not found ...
  if( sp == NULL )
  {
    // ... and a valid flags value was given ...
    if( flags != 0 )
    {
      // ... then create a new sync target.
      sp        = cmMemAllocZ(cmGrSync_t,1);
      sp->grH   = syncGrH;
      sp->flags = flags;
      sp->link  = p->syncs;
      p->syncs  = sp;
    }
  }
  else // ... otherwise syncGrH is already a sync target
  {
    // if flags is non-zero then update the target sync flags
    if( flags != 0 )
      sp->flags = flags;
    else
    {
      // otherwise delete the sync recd assoc'd with syncGrH
      if( pp == NULL )
        p->syncs = sp->link;
      else
        pp->link = sp->link;

      cmMemFree(sp);
    }
  }
}

void     cmGrReport( cmGrH_t h,cmRpt_t* r )
{
  cmGr_t*  p   = _cmGrHandleToPtr(h);
  cmRpt_t* rpt = r==NULL ? p->err.rpt : r;

  cmRptPrintf(rpt,"cfg:0x%x state:0x%x\n",p->cfgFlags, p->stateFlags);
  //cmRptPrintf(rpt,"World: "); cmGrVExtMaxXpt(&p->wext,rpt);  cmRptPrintf(rpt,"\n");
  cmRptPrintf(rpt,"View:  "); cmGrVExtRpt(&p->vext,rpt);  cmRptPrintf(rpt,"\n");
  cmRptPrintf(rpt,"Phys:  "); cmGrPExtRpt(&p->pext,rpt);  cmRptPrintf(rpt,"\n");

  _cmGrObjReportR(p,p->rootObj,rpt);
}
