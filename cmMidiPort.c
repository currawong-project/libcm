#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmMidi.h"
#include "cmMidiPort.h"



//===================================================================================================
//
//

enum 
{ 
  kBufByteCnt = 1024, 

  kExpectStatusStId=0,       // 0
  kExpectDataStId,           // 1
  kExpectStatusOrDataStId,   // 2
  kExpectEOXStId             // 3
};

typedef struct cmMpParserCb_str
{
  cmMpCallback_t           cbFunc;
  void*                    cbDataPtr;
  struct cmMpParserCb_str* linkPtr;
} cmMpParserCb_t;

typedef struct
{
  cmErr_t        err;

  cmMpParserCb_t* cbChain;
  
  cmMidiPacket_t  pkt;

  unsigned      state;          // parser state id
  unsigned      errCnt;         // accumlated error count
  cmMidiByte_t  status;         // running status
  cmMidiByte_t  data0;          // data byte 0
  unsigned      dataCnt;        // data byte cnt for current status 
  unsigned      dataIdx;        // index (0 or 1) of next data byte
  cmMidiByte_t* buf;            // output buffer
  unsigned      bufByteCnt;     // output buffer byte cnt
  unsigned      bufIdx;         // next output buffer index
  unsigned      msgCnt;         // count of channel messages in the buffer
} cmMpParser_t;

cmMpParser_t* _cmMpParserFromHandle( cmMpParserH_t h )
{
  cmMpParser_t* p = (cmMpParser_t*)h.h;
  assert(p!=NULL);
  return p;
}

void _cmMpParserReport( cmMpParser_t* p )
{
  cmRptPrintf(p->err.rpt,"state:%i st:0x%x d0:%i dcnt:%i didx:%i buf[%i]->%i msg:%i err:%i\n",p->state,p->status,p->data0,p->dataCnt,p->dataIdx,p->bufByteCnt,p->bufIdx,p->msgCnt,p->errCnt);
}

void _cmMpParserDestroy( cmMpParser_t* p )
{
  cmMemPtrFree(&p->buf);

  cmMpParserCb_t* c = p->cbChain;
  while(c != NULL)
  {
    cmMpParserCb_t* nc = c->linkPtr;
    cmMemFree(c);
    c = nc;
  }

  cmMemPtrFree(&p);

}
 
cmMpParserH_t cmMpParserCreate( unsigned devIdx, unsigned portIdx, cmMpCallback_t cbFunc, void* cbDataPtr, unsigned bufByteCnt, cmRpt_t* rpt )
{
  cmMpRC_t      rc = kOkMpRC;
  cmMpParserH_t h;
  cmMpParser_t* p = cmMemAllocZ( cmMpParser_t, 1 );

  cmErrSetup(&p->err,rpt,"MIDI Parser");

  p->pkt.devIdx        = devIdx;
  p->pkt.portIdx       = portIdx;

  //p->cbChain           = cmMemAllocZ( cmMpParserCb_t, 1 );
  //p->cbChain->cbFunc    = cbFunc;
  //p->cbChain->cbDataPtr = cbDataPtr;
  //p->cbChain->linkPtr   = NULL;
  p->cbChain           = NULL;
  p->buf               = cmMemAllocZ( cmMidiByte_t, bufByteCnt );
  p->bufByteCnt        = bufByteCnt;
  p->bufIdx            = 0;
  p->msgCnt            = 0;
  p->state             = kExpectStatusStId;
  p->dataIdx           = cmInvalidIdx;
  p->dataCnt           = cmInvalidCnt;
  p->status            = kInvalidStatusMdId;

  h.h = p;

  if( cbFunc != NULL )
    rc = cmMpParserInstallCallback(h, cbFunc, cbDataPtr );
    

  if( rc != kOkMpRC )
  {
    h.h = NULL;
    _cmMpParserDestroy(p);
  }
  

  return h;
} 

void        cmMpParserDestroy(    cmMpParserH_t* hp )
{
  if( hp==NULL || hp->h == NULL )
    return;

  cmMpParser_t* p = _cmMpParserFromHandle(*hp);

  _cmMpParserDestroy(p);
  
  hp->h = NULL;

}

unsigned    cmMpParserErrorCount( cmMpParserH_t h )
{
  cmMpParser_t* p = _cmMpParserFromHandle(h);
  if( p == NULL )
    return 0;

  return p->errCnt;
}

void _cmMpParserCb( cmMpParser_t* p, cmMidiPacket_t* pkt, unsigned pktCnt )
{
  cmMpParserCb_t* c = p->cbChain;
  for(; c!=NULL; c=c->linkPtr)
  {
    pkt->cbDataPtr = c->cbDataPtr;
    c->cbFunc( pkt, pktCnt );
  }
}

void _cmMpTransmitChMsgs( cmMpParser_t* p )
{
  if( p->msgCnt > 0 )
  {
    p->pkt.msgArray = (cmMidiMsg*)p->buf;
    p->pkt.msgCnt   = p->msgCnt;
    p->pkt.sysExMsg = NULL;

    //p->cbFunc( &p->pkt, 1 );  
    _cmMpParserCb(p,&p->pkt,1);

    p->bufIdx = 0;
    p->msgCnt = 0;
  }
}

void _cmMpTransmitSysEx( cmMpParser_t* p )
{
  p->pkt.msgArray = NULL;
  p->pkt.sysExMsg = p->buf;
  p->pkt.msgCnt   = p->bufIdx;
  //p->cbFunc( &p->pkt, 1 );
  _cmMpParserCb(p,&p->pkt,1);
  p->bufIdx = 0;

}

void _cmMpParserStoreChMsg( cmMpParser_t* p, unsigned deltaMicroSecs,  cmMidiByte_t d )
{
  // if there is not enough room left in the buffer then transmit the current messages
  if( p->bufByteCnt - p->bufIdx < sizeof(cmMidiMsg) )
    _cmMpTransmitChMsgs(p);


  assert( p->bufByteCnt - p->bufIdx >= sizeof(cmMidiMsg) );

  // get a pointer to the next msg in the buffer
  cmMidiMsg* msgPtr = (cmMidiMsg*)(p->buf + p->bufIdx);

  // fill the buffer msg
  msgPtr->deltaUs = deltaMicroSecs;
  msgPtr->status  = p->status;

  switch( p->dataCnt )
  {
    case 0: 
      break;
    case 1:
      msgPtr->d0 = d;
      msgPtr->d1 = 0;
      break;

    case 2:
      msgPtr->d0 = p->data0;
      msgPtr->d1 = d;
      break;

    default:
      assert(0);
  }

  // update the msg count and next buffer 
  ++p->msgCnt;

  p->bufIdx += sizeof(cmMidiMsg);

}

void cmMpParseMidiData( cmMpParserH_t h, unsigned deltaMicroSecs, const cmMidiByte_t* iBuf, unsigned iByteCnt )
{
  
  cmMpParser_t* p = _cmMpParserFromHandle(h);

  if( p == NULL )
    return;
  
  const cmMidiByte_t* ip = iBuf;
  const cmMidiByte_t* ep  = iBuf + iByteCnt;

  for(; ip < ep; ++ip )
  {
    // if this byte is a status byte
    if( cmMidiIsStatus(*ip) )
    {
      if( p->state != kExpectStatusStId && p->state != kExpectStatusOrDataStId )
        ++p->errCnt;

      p->status  = *ip;
      p->dataCnt = cmMidiStatusToByteCount(*ip);

      switch( p->status )
      {
        case kSysExMdId: // if this is the start of a sys-ex msg ...
          // ... clear the buffer to prepare from sys-ex data
          _cmMpTransmitChMsgs(p); 

          p->state   = kExpectEOXStId;
          p->dataCnt = cmInvalidCnt;
          p->dataIdx = cmInvalidIdx;
          p->buf[ p->bufIdx++ ] =  kSysExMdId;
          break;

        case kSysComEoxMdId: // if this is the end of a sys-ex msg
          assert( p->bufIdx < p->bufByteCnt );
          p->buf[p->bufIdx++] = *ip; 
          _cmMpTransmitSysEx(p);
          p->state = kExpectStatusStId;
          break;

        default: // ... otherwise it is a 1,2, or 3 byte msg status
          if( p->dataCnt > 0 )
          {
            p->state   = kExpectDataStId;
            p->dataIdx = 0;
          }
          else
          {
            // this is a status only msg - store it
            _cmMpParserStoreChMsg(p,deltaMicroSecs,*ip);

            p->state   = kExpectStatusStId;
            p->dataIdx = cmInvalidIdx;
            p->dataCnt = cmInvalidCnt;
          }

      }

      continue;
    }

    // at this point the current byte (*ip) is a data byte

    switch(p->state)
    {

      case kExpectStatusOrDataStId:
        assert( p->dataIdx == 0 );
          
      case kExpectDataStId:
        switch( p->dataIdx )
        {
          case 0: // expecting data byte 0 ...
            
            switch( p->dataCnt )
            {
              case 1: // ... of a 1 byte msg - the msg is complete
                _cmMpParserStoreChMsg(p,deltaMicroSecs,*ip);
                p->state = kExpectStatusOrDataStId; 
                break;

              case 2: // ... of a 2 byte msg - prepare to recv the second data byte
                p->state   = kExpectDataStId;
                p->dataIdx = 1;
                p->data0   = *ip;
                break;

              default:
                assert(0);
            }
            break;

          case 1: // expecting data byte 1 of a two byte msg
            assert( p->dataCnt == 2 );
            assert( p->state == kExpectDataStId );

            _cmMpParserStoreChMsg(p,deltaMicroSecs,*ip);
            p->state   = kExpectStatusOrDataStId;
            p->dataIdx = 0;
            break;

          default:
            assert(0);            
            
        }
        break;

      case kExpectEOXStId:
          assert( p->bufIdx < p->bufByteCnt );

          p->buf[p->bufIdx++] = *ip;

          // if the buffer is full - then transmit it
          if( p->bufIdx == p->bufByteCnt )
            _cmMpTransmitSysEx(p);

        break;

    }

  } // ip loop

  _cmMpTransmitChMsgs(p);
 
}

cmMpRC_t      cmMpParserInstallCallback( cmMpParserH_t h, cmMpCallback_t  cbFunc, void* cbDataPtr )
{
  cmMpParser_t*   p        = _cmMpParserFromHandle(h);
  cmMpParserCb_t* newCbPtr = cmMemAllocZ( cmMpParserCb_t, 1 );
  cmMpParserCb_t* c        = p->cbChain;
  
  newCbPtr->cbFunc    = cbFunc;
  newCbPtr->cbDataPtr = cbDataPtr;
  newCbPtr->linkPtr   = NULL;

  if( p->cbChain == NULL )
    p->cbChain = newCbPtr;
  else  
  {
    while( c->linkPtr != NULL )
      c = c->linkPtr;

    c->linkPtr = newCbPtr;
  }
  
  return kOkMpRC;
}

cmMpRC_t      cmMpParserRemoveCallback(  cmMpParserH_t h, cmMpCallback_t cbFunc, void* cbDataPtr )
{
  cmMpParser_t*   p = _cmMpParserFromHandle(h);
  cmMpParserCb_t* c1 = p->cbChain;  // target link
  cmMpParserCb_t* c0 = NULL;        // link pointing to target

  // search for the cbFunc to remove
  for(; c1!=NULL; c1=c1->linkPtr)
  {
    if( c1->cbFunc == cbFunc && c1->cbDataPtr == cbDataPtr)
      break;

    c0 = c1;
  }

  // if the cbFunc was not found
  if( c1 == NULL )
    return cmErrMsg(&p->err,kCbNotFoundMpRC,"Unable to locate the callback function %p for removal.",cbFunc);

  // the cbFunc to remove was found

  // if it was the first cb in the chain
  if( c0 == NULL )
    p->cbChain = c1->linkPtr;
  else
    c0->linkPtr = c1->linkPtr;

  cmMemFree(c1);
  
  return kOkMpRC;
}

bool cmMpParserHasCallback( cmMpParserH_t h, cmMpCallback_t cbFunc, void* cbDataPtr )
{
  cmMpParser_t*   p = _cmMpParserFromHandle(h);
  cmMpParserCb_t* c = p->cbChain;  // target link

  // search for the cbFunc to remove
  for(; c!=NULL; c=c->linkPtr)
    if( c->cbFunc == cbFunc && c->cbDataPtr == cbDataPtr )
      return true;

  return false;
}

//====================================================================================================
//
//

void cmMpTestPrint( void* userDataPtr, const char* fmt, va_list vl )
{
  vprintf(fmt,vl);
}

void cmMpTestCb( const cmMidiPacket_t* pktArray, unsigned pktCnt )
{
  unsigned i,j;
  for(i=0; i<pktCnt; ++i)
  {
    const cmMidiPacket_t* p = pktArray + i;

    for(j=0; j<p->msgCnt; ++j)
      if( p->msgArray != NULL )
        printf("%8i 0x%x %i %i\n", p->msgArray[j].deltaUs/1000, p->msgArray[j].status,p->msgArray[j].d0, p->msgArray[j].d1);
      else
        printf("0x%x ",p->sysExMsg[j]);

  }
}

void cmMpTest( cmRpt_t* rpt )
{
  char ch;
  unsigned parserBufByteCnt = 1024;
  cmMpInitialize(cmMpTestCb,NULL,parserBufByteCnt,"app",rpt);
  cmMpReport(rpt);

  
  cmRptPrintf(rpt,"<return> to continue\n");

  while((ch = getchar()) != 'q')
  {
    cmMpDeviceSend(0,0,0x90,60,60);
  }

  cmMpFinalize();
}
