#include "cmGlobal.h"
#include "cmRtSysMsg.h"
#include "cmUiDrvr.h"

void cmUiDriverArgSetup( cmUiDriverArg_t* a, 
  unsigned  rtSubIdx,
  unsigned  selId,
  cmUiDId_t dId,
  unsigned  appId,
  unsigned  usrId,
  unsigned  panelId,
  cmUiCId_t cId,
  unsigned  flags,
  int       ival,
  double    fval,
  const cmChar_t* sval,
  int       x,
  int       y,
  int       w,
  int       h
 )
{
  
  flags = cmEnaFlag(flags,kSvalUiFl,sval!=NULL);
  a->hdr.rtSubIdx= rtSubIdx;
  a->hdr.selId   = selId;
  a->dId     = dId;
  a->appId   = appId;
  a->usrId   = usrId;
  a->panelId = panelId;
  a->cId     = cId;
  a->flags   = flags;
  a->ival    = ival;
  a->fval    = fval;
  a->sval    = sval;
  a->x       = x;
  a->y       = y;
  a->w       = w;
  a->h       = h;
}

unsigned cmUiDriverArgSerializeBufByteCount( const cmUiDriverArg_t* a )
{  
  unsigned n = sizeof(*a);
  if( cmIsFlag(a->flags,kSvalUiFl) && a->sval != NULL )
    n += strlen(a->sval) + 1;
  return n;
}

cmUiRC_t cmUiDriverArgSerialize( const cmUiDriverArg_t* a, void* buf, unsigned bufByteCnt )
{
  if( bufByteCnt < cmUiDriverArgSerializeBufByteCount(a)) 
    return kBufTooSmallUiRC;

  memcpy(buf,a,sizeof(*a));

  if( cmIsFlag(a->flags,kSvalUiFl) && a->sval != NULL )
    strcpy( (char*)buf + sizeof(*a), a->sval );

  return kOkUiRC;  
}

cmUiRC_t cmUiDriverArgDeserialize( cmUiDriverArg_t* a, const void* buf, unsigned bufByteCnt )
{
  bool fl = bufByteCnt >= sizeof(*a);
  assert( fl );
  if( !fl )
    return kBufTooSmallUiRC;

  memcpy(a,buf,sizeof(*a));
  
  fl = a->cId < kMaxUiCId && a->dId < kMaxDId;

  assert(fl );
  if( !fl )
    return kBufCorruptUiRC;

  if( cmIsFlag(a->flags,kSvalUiFl) && a->sval != NULL )
    a->sval = (char*)buf + sizeof(*a);

  return kOkUiRC;
}

int             cmUiDriverArgGetInt(    const cmUiDriverArg_t* a )
{
  if( a->flags & kIvalUiFl )
    return a->ival;

  if( a->flags & kFvalUiFl )
    return round(a->fval);

  assert(0);
  return -1;
}

double          cmUiDriverArgGetDouble( const cmUiDriverArg_t* a )
{
  if( a->flags & kIvalUiFl )
    return a->ival;

  if( a->flags & kFvalUiFl )
    return a->fval;

  assert(0);
  return -1;
}

const cmChar_t* cmUiDriverArgGetString( const cmUiDriverArg_t* a )
{
  return a->flags & kSvalUiFl ? a->sval : NULL;
}
