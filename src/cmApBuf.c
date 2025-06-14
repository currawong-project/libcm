//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmTime.h"
#include "cmAudioPort.h"
#include "cmApBuf.h"
#include "cmThread.h"

/*
  This API is in general called by two types of threads:
  audio devices threads and the client thread.  There
  may be multiple devie threads however there is only
  one client thread.

  The audio device threads only call cmApBufUpdate().
  cmApBufUpdate() is never called by any other threads.
  A call from the audio update threads targets specific channels
  (cmApCh records).  The variables within each channels that
  it modifies are confined to:
    on input channels:   increments ii and increments fn (data is entering the ch. buffers)
    on output channels:  increments oi and decrements fn (data is leaving the ch. buffers)

  The client picks up incoming audio and provides outgoing audio via
  cmApBufGet(). It then informs the cmApBuf() that it has completed
  the audio data transfer by calling cmApBufAdvance().

  cmApBufAdvance() modifies the following internal variables:
    on input channels:  increments oi and decrements fn (data has left the ch buffer)
    on output channels: increments ii and increments fn (data has enterned the ch. buffer)

  Based on the above scenario the channel ii and oi variables are always thread-safe
  because they are only changed by a single thread. 

               ii       oi     fn
              ------   -----  ----
  input  ch:  audio    client both
  output ch:  client   audio  both
  
  The fn variable however is not thread-safe and therefore care must be taken as
  to how it is read and updated.  
 */

enum { kInApIdx=0, kOutApIdx=1, kIoApCnt=2 };

typedef struct
{
  unsigned      fl;   // kChApFl|kToneApFl|kMeterApFl ...
  cmApSample_t* b;    // b[n]
  unsigned      ii;   // next in
  unsigned      oi;   // next out
  unsigned      fn;   // full cnt  - count of samples currently in the buffer - incr'd by incoming, decr'd by outgoing
  unsigned      phs;  // tone phase
  double        hz;   // tone frequency 
  double        gain; // channel gain
  cmApSample_t* m;    // m[mn] meter sample sum  
  unsigned      mn;   // length of m[]
  unsigned      mi;   // next ele of m[] to rcv sum
  cmApSample_t  s0;   // buffered sample used for srate conversion
} cmApCh;

typedef struct
{
  unsigned     chCnt;      // Count of channels
  cmApCh*      chArray;    // chArray[chCnt] channel record array

  unsigned     n;          // Length of b[] (multiple of dspFrameCnt)  bufCnt*framesPerCycle
  double       srate;      // Device sample rate;
  int          srateMult;  // Internal sample rate multiplier (negative values for dividing). This srateMult*srate is the sample rate of
                           // signals held in the chApCh[] input and output buffers. Sample rate conversion to/from this rate
                           // occurs in cmApBufUpdate() as signals go from/to the audio device.

  unsigned     faultCnt;       // error count since start
  unsigned     framesPerCycle; // expected count of frames per channel to/from the audio device on each call to cmApBufUpdate()
  unsigned     dspFrameCnt;    // number of frames per channel in buffers returned by cmApBufGet().
  cmTimeSpec_t timeStamp;      // base (starting) time stamp for this device
  unsigned     ioFrameCnt;     // count of frames input or output for this device

} cmApIO;

typedef struct
{
  // ioArray[] always contains 2 elements - one for input the other for output.
  cmApIO     ioArray[kIoApCnt]; 
} cmApDev;

typedef struct
{
  cmApDev*   devArray;       
  unsigned   devCnt;
  unsigned   meterMs;

  cmApSample_t* zeroBuf;    // buffer of zeros 
  unsigned      zeroBufCnt; // max of all dspFrameCnt for all devices.


  unsigned     abufIdx;
  cmApSample_t abuf[ 16384 ];
} cmApBuf;

cmApBuf _cmApBuf;


// Copy the source channel (srcChIdx) to the destination buffer and apply up-sampling.
// 'src' is an interleaved buffer with 'srcN' samples per channel and 'srcChN' channels (total size in samples = srcN*srcChN)
// 'dst' is a non-interleaved (single channel) circular buffer of length 'dstN' where 'dstIdx' is the index of the first dst slot to receive a sample..
// Return the index into dst[] of the next location to receive an incoming sample.
unsigned  _cmApCopyInUpSample( const cmApSample_t* src, unsigned srcN, unsigned srcChN, unsigned srcChIdx, cmApSample_t* dst, unsigned dstN, unsigned dstIdx, unsigned mult, double gain, cmApSample_t* s0_Ref )
{
  cmApSample_t s0 = *s0_Ref;
  unsigned si,k,di=dstIdx;
  
  for(si=0; si<srcN; ++si)
  {
    cmApSample_t s1 = src[si*srcChN+srcChIdx];
    
    for(k=1; k<mult; ++k)
    {
      dst[di] = gain * (s0 + (s1 - s0) * k / mult );
      di = (di+1) % dstN;
    }

    dst[di] = gain * s1;
    di      = (di+1) % dstN;
    s0     = s1;    
  }
  
  *s0_Ref = s0;
  
  return di;
}

// Copy the source channel (srcChIdx) to the destination buffer and apply down-sampling.
// 'src' is an interleaved linear buffer with 'srcN' samples per channel and 'srcChN' channels,
// 'dst' is a non-interleaved circular buffer of length 'dstN' where 'dstIdx' is the index of the first dst slot to receive a sample..
// Return the index into dst[] of the next location to receive an incoming sample.
unsigned  _cmApCopyInDnSample( const cmApSample_t* src, unsigned srcN, unsigned srcChN, unsigned srcChIdx, cmApSample_t* dst, unsigned dstN, unsigned dstIdx, unsigned div, double gain )
{
  unsigned si,di=dstIdx;
  
  for(si=0; si<srcN; si+=div)
  {
    dst[di] = gain * src[si*srcChN+srcChIdx];
    di      = (di+1) % dstN;
  }
  return di;
}


// Copy samples from an interleaved src buffer to a non-interleaved dst buffer with sample rate conversion
unsigned  _cmApCopyInSamples( const cmApSample_t* src, unsigned srcN, unsigned srcChN, unsigned srcChIdx, cmApSample_t* dst, unsigned dstN, unsigned dstIdx, int srateMult, double gain, cmApSample_t* s0_Ref )
{
  if( srateMult < 1 )
    return _cmApCopyInDnSample( src, srcN, srcChN, srcChIdx, dst, dstN, dstIdx, -srateMult, gain );

  return _cmApCopyInUpSample( src, srcN, srcChN, srcChIdx, dst, dstN, dstIdx, srateMult, gain, s0_Ref );
}

void printBuf( const cmApSample_t* src, unsigned srcN, unsigned bi, unsigned n )
{
  unsigned i,j;
  for(i=bi,j=0; j<n; ++i,++j)
  {
    i = i % srcN;
    printf("(%i,%i,%f),\n",j,i,src[i]);
  }
  printf("-----\n");
}


// Copy 
// 'src' is a non-interleaved circular buf of total length 'srcN', with the first sample coming at 'srcIdx'.
// 'dst' is an interleaved buffer of length 'dstN' with 'dstChN' channels
// Return the index of the next src sample.
unsigned _cmApCopyOutUpSample( const cmApSample_t* src, unsigned srcN, unsigned srcIdx, cmApSample_t* dst, unsigned dstN, unsigned dstChN, unsigned dstChIdx, int mult, double gain, cmApSample_t* s0_Ref )
{
  unsigned di,si=srcIdx;
  cmApSample_t s0 = *s0_Ref;
  
  for(di=0; di<dstN; ++di)
  {
    cmApSample_t s1 = src[si];
    unsigned     k;
    for(k=1; k<mult && di<dstN; ++di,++k)
    {
      dst[di*dstChN+dstChIdx] = gain * (s0 + (s1 - s0) * k / mult );      
    }

    if( di < dstN )
    {
      dst[di*dstChN+dstChIdx] = gain * s1;
      ++di;
    }

    si = (si + 1) % srcN;
    
    s0 = s1;
  }

  *s0_Ref = s0;
  return si;
}

// 'src' is a non-interleaved (single channel) circular buf of total length 'srcN', with the first sample coming at 'srcIdx'.
// 'dst' is an interleaved buffer of length 'dstN' with 'dstChN' channels
// Return the index of the next src sample.
unsigned _cmApCopyOutDnSample( const cmApSample_t* src, unsigned srcN, unsigned srcIdx, cmApSample_t* dst, unsigned dstN, unsigned dstChN, unsigned dstChIdx, int div, double gain )
{
  unsigned di,si;

  // The total count of output samples is determined by 'dstN'
  // Downsampling is acheived by advancing the src index by 'div' samples.

  for(di=0,si=srcIdx; di<dstN; ++di)
  {
    dst[di*dstChN+dstChIdx] = gain * src[si];
    si = (si + div) % srcN;
  }

  return si;  
}


// Copy samples from a non-interleaved src buffer to an interleaved dst buffer with sample rate conversion
unsigned   _cmApCopyOutSamples( const cmApSample_t* src, unsigned srcN, unsigned srcIdx, cmApSample_t* dst, unsigned dstN, unsigned dstChN, unsigned dstChIdx, int srateMult, double gain, cmApSample_t* s0_Ref )
{
  if( srateMult > 0  )
    return _cmApCopyOutDnSample(src, srcN, srcIdx, dst, dstN, dstChN, dstChIdx, srateMult, gain );

  return _cmApCopyOutUpSample( src, srcN, srcIdx, dst, dstN, dstChN, dstChIdx, -srateMult, gain, s0_Ref );
}


cmApSample_t _cmApMeterValue( const cmApCh* cp )
{
  double sum = 0;
  unsigned i;
  for(i=0; i<cp->mn; ++i)
    sum += cp->m[i];

  return cp->mn==0 ? 0 : (cmApSample_t)sqrt(sum/cp->mn);
}

void _cmApSine0( cmApCh* cp, cmApSample_t* b0, unsigned n0, cmApSample_t* b1, unsigned n1, unsigned stride, float srate )
{
  unsigned i;

  for(i=0; i<n0; ++i,++cp->phs)
    b0[i*stride] = (float)(cp->gain * sin( 2.0 * M_PI * cp->hz * cp->phs / srate ));

  for(i=0; i<n1; ++i,++cp->phs)
    b1[i*stride] = (float)(cp->gain * sin( 2.0 * M_PI * cp->hz * cp->phs / srate ));
}

// Synthesize a sine signal of length sigN*srateMult starting at dst[dstidx]
// Assume dst[dstN] is a circular buffer
unsigned  _cmApSine( cmApCh* cp, cmApSample_t* dst, unsigned dstN, unsigned dstIdx,  unsigned dstChCnt, unsigned sigN, unsigned srateMult, float srate, double gain )
{
  unsigned i,di;
  
  sigN = srateMult < 0 ? sigN/abs(srateMult) : sigN * srateMult;
  for(i=0, di=dstIdx; i<sigN; ++i,++cp->phs)
  {
    dst[di] = (cmApSample_t)(gain * sin( 2.0 * M_PI * cp->hz * cp->phs / srate ));
    di = (di+dstChCnt) % dstN;
  }

  
  return sigN;
}


cmApSample_t _cmApMeter( const cmApSample_t* b, unsigned bn, unsigned stride )
{
  const cmApSample_t* ep  = b + bn;
  cmApSample_t        sum = 0;

  for(; b<ep; b+=stride)
    sum += *b * *b;

  return sum / bn;
}

void _cmApChFinalize( cmApCh* chPtr )
{
  cmMemPtrFree( &chPtr->b );
  cmMemPtrFree( &chPtr->m );
}

// n=buf sample cnt mn=meter buf smp cnt
void _cmApChInitialize( cmApCh* chPtr, unsigned n, unsigned mn )
{
  _cmApChFinalize(chPtr);

  chPtr->b    = n==0 ? NULL : cmMemAllocZ( cmApSample_t, n );
  chPtr->ii   = 0;
  chPtr->oi   = 0;
  chPtr->fn   = 0;
  chPtr->fl   = (n!=0 ? kChApFl : 0);
  chPtr->hz   = 440;
  chPtr->gain = 0.8; // use reduced gain to prevent clipping
  chPtr->mn   = mn;
  chPtr->m    = cmMemAllocZ(cmApSample_t,mn);
  chPtr->mi   = 0;

}

void _cmApIoFinalize( cmApIO* ioPtr )
{
  unsigned i;
  for(i=0; i<ioPtr->chCnt; ++i)
    _cmApChFinalize( ioPtr->chArray + i );

  cmMemPtrFree(&ioPtr->chArray);
  ioPtr->chCnt = 0;
  ioPtr->n     = 0;
}

void _cmApIoInitialize( cmApIO* ioPtr, double srate, unsigned framesPerCycle, unsigned chCnt, unsigned n, unsigned meterBufN, unsigned dspFrameCnt, int srateMult )
{
  unsigned i;

  if( srateMult == 0 )
    srateMult = 1;

  _cmApIoFinalize(ioPtr);

  n += (n % dspFrameCnt); // force buffer size to be a multiple of dspFrameCnt
  
  ioPtr->chArray           = chCnt==0 ? NULL : cmMemAllocZ( cmApCh, chCnt );
  ioPtr->chCnt             = chCnt;
  ioPtr->n                 = srateMult<0 ?  n : n*srateMult;
  ioPtr->faultCnt          = 0;
  ioPtr->framesPerCycle    = framesPerCycle;
  ioPtr->srate             = srate;
  ioPtr->srateMult         = srateMult;
  ioPtr->dspFrameCnt       = dspFrameCnt;
  ioPtr->timeStamp.tv_sec  = 0;
  ioPtr->timeStamp.tv_nsec = 0;
  ioPtr->ioFrameCnt        = 0;

  for(i=0; i<chCnt; ++i )
    _cmApChInitialize( ioPtr->chArray + i, ioPtr->n, meterBufN );

}

void _cmApDevFinalize( cmApDev* dp )
{
  unsigned i;
  for(i=0; i<kIoApCnt; ++i)
    _cmApIoFinalize( dp->ioArray+i);
}

void _cmApDevInitialize( cmApDev* dp, double srate, unsigned iFpC, unsigned iChCnt, unsigned iBufN, unsigned oFpC, unsigned oChCnt, unsigned oBufN, unsigned meterBufN, unsigned dspFrameCnt, int srateMult )
{
  unsigned i;

  _cmApDevFinalize(dp);

  for(i=0; i<kIoApCnt; ++i)
  {
    unsigned chCnt = i==kInApIdx ? iChCnt     : oChCnt;
    unsigned bufN  = i==kInApIdx ? iBufN      : oBufN;
    unsigned fpc   = i==kInApIdx ? iFpC       : oFpC;
    _cmApIoInitialize( dp->ioArray+i, srate, fpc, chCnt, bufN, meterBufN, dspFrameCnt, srateMult );

  }
      
} 

cmAbRC_t cmApBufInitialize( unsigned devCnt, unsigned meterMs )
{
  cmAbRC_t rc;

  if((rc = cmApBufFinalize()) != kOkAbRC )
    return rc;

  _cmApBuf.devArray        = cmMemAllocZ( cmApDev, devCnt );
  _cmApBuf.devCnt          = devCnt;
  cmApBufSetMeterMs(meterMs);

  return kOkAbRC;
}

cmAbRC_t cmApBufFinalize()
{
  unsigned i;
  for(i=0; i<_cmApBuf.devCnt; ++i)    
    _cmApDevFinalize(_cmApBuf.devArray + i);

  cmMemPtrFree( &_cmApBuf.devArray );
  cmMemPtrFree( &_cmApBuf.zeroBuf );
  
  _cmApBuf.devCnt          = 0;

  FILE* fp;
  if(_cmApBuf.abufIdx>0 )
  {
    if((fp = fopen("/home/kevin/temp/temp.csv","wt")) != NULL )
    {
      int i;
      for(i=0; i<_cmApBuf.abufIdx; ++i)
      {
        fprintf(fp,"%f,\n",_cmApBuf.abuf[i]);
      }
      fclose(fp);
    }
  }
  

  
  return kOkAbRC;
}

cmAbRC_t cmApBufSetup( 
  unsigned devIdx,
  double   srate,
  unsigned dspFrameCnt,
  unsigned bufCnt,
  unsigned inChCnt, 
  unsigned inFramesPerCycle,
  unsigned outChCnt, 
  unsigned outFramesPerCycle,
  int      srateMult)
{
  cmApDev* devPtr    = _cmApBuf.devArray + devIdx;
  unsigned iBufN     = bufCnt * inFramesPerCycle;
  unsigned oBufN     = bufCnt * outFramesPerCycle;
  unsigned meterBufN = cmMax(1,floor(srate * _cmApBuf.meterMs / (1000.0 * outFramesPerCycle)));

  _cmApDevInitialize( devPtr, srate, inFramesPerCycle, inChCnt, iBufN, outFramesPerCycle, outChCnt, oBufN, meterBufN, dspFrameCnt, srateMult );

  if( inFramesPerCycle > _cmApBuf.zeroBufCnt || outFramesPerCycle > _cmApBuf.zeroBufCnt )
  {
    _cmApBuf.zeroBufCnt = cmMax(inFramesPerCycle,outFramesPerCycle);
    _cmApBuf.zeroBuf    = cmMemResizeZ(cmApSample_t,_cmApBuf.zeroBuf,_cmApBuf.zeroBufCnt);
  }

  return kOkAbRC;  
}

cmAbRC_t cmApBufPrimeOutput( unsigned devIdx, unsigned audioCycleCnt )
{
  cmApIO*  iop = _cmApBuf.devArray[devIdx].ioArray + kOutApIdx;
  unsigned i;
  
  for(i=0; i<iop->chCnt; ++i)
  {
    cmApCh*  cp = iop->chArray + i;
    unsigned bn = iop->n * sizeof(cmApSample_t);
    memset(cp->b,0,bn);
    cp->oi = 0;
    cp->ii = iop->framesPerCycle * audioCycleCnt;
    cp->fn = iop->framesPerCycle * audioCycleCnt;
  }

  return kOkAbRC;
}

void cmApBufOnPortEnable( unsigned devIdx, bool enableFl )
{
  if( devIdx == cmInvalidIdx || enableFl==false)
    return;
   
  cmApIO*  iop = _cmApBuf.devArray[devIdx].ioArray + kOutApIdx;
  iop->timeStamp.tv_sec = 0;
  iop->timeStamp.tv_nsec = 0;
  iop->ioFrameCnt = 0;

  iop = _cmApBuf.devArray[devIdx].ioArray + kInApIdx;
  iop->timeStamp.tv_sec = 0;
  iop->timeStamp.tv_nsec = 0;
  iop->ioFrameCnt = 0;


}

cmAbRC_t cmApBufUpdate(
  cmApAudioPacket_t*  inPktArray, 
  unsigned           inPktCnt, 
  cmApAudioPacket_t* outPktArray, 
  unsigned           outPktCnt )
{
  unsigned i,j;

  // copy samples from the packet to the buffer
  if( inPktArray != NULL )
  {
    for(i=0; i<inPktCnt; ++i)
    {
      cmApAudioPacket_t*  pp  = inPktArray + i;           
      cmApIO*             ip  = _cmApBuf.devArray[pp->devIdx].ioArray + kInApIdx; // dest io recd

      // if the base time stamp has not yet been set - then set it
      if( ip->timeStamp.tv_sec==0 && ip->timeStamp.tv_nsec==0 )
        ip->timeStamp = pp->timeStamp;

      // for each source packet channel and enabled dest channel
      for(j=0; j<pp->chCnt; ++j)
      {
        cmApCh*  cp = ip->chArray + pp->begChIdx +j; // dest ch

        assert(pp->begChIdx + j < ip->chCnt );

        // if the incoming samples  would overflow the buffer then ignore them
        if( cp->fn + pp->audioFramesCnt > ip->n )
        {
          ++ip->faultCnt; // record input overflow 
          continue;
        }

        // if the incoming samples would go off the end of the buffer then 
        // copy in the samples in two segments (one at the end and another at begin of dest channel)
        
        bool                enaFl = cmIsFlag(cp->fl,kChApFl) && cmIsFlag(cp->fl,kMuteApFl)==false;
        const cmApSample_t* sp    = enaFl ? ((cmApSample_t*)pp->audioBytesPtr) + j : _cmApBuf.zeroBuf;
        //unsigned            ssn   = enaFl ? pp->chCnt : 1;  // stride (packet samples are interleaved)
        //cmApSample_t*       dp    = cp->b + cp->ii;
        //const cmApSample_t* ep    = dp    + n0;


        // update the meter
        if( cmIsFlag(cp->fl,kMeterApFl) )
        {
          cp->m[cp->mi] = _cmApMeter(sp,pp->audioFramesCnt,pp->chCnt);
          cp->mi = (cp->mi + 1) % cp->mn;
        }

        unsigned incrSmpN = 0;

        // if the test tone is enabled on this input channel
        if( enaFl && cmIsFlag(cp->fl,kToneApFl) )
        {
          //_cmApSine(cp, dp, n0, cp->b, n1, 1, ip->srate );
          incrSmpN =  _cmApSine( cp, cp->b, ip->n, cp->ii,  1, pp->audioFramesCnt, ip->srateMult, ip->srate, cp->gain );

          
        }
        else // otherwise copy samples from the packet to the buffer
        {

          unsigned pi = cp->ii;
          
          cp->ii =  _cmApCopyInSamples( (cmApSample_t*)pp->audioBytesPtr, pp->audioFramesCnt, pp->chCnt, j, cp->b, ip->n, cp->ii, ip->srateMult, cp->gain, &cp->s0 );

          if( false )
            if( j == 2 && _cmApBuf.abufIdx < 16384 )
            {
              int ii;
              for(ii=pi; ii!=cp->ii && _cmApBuf.abufIdx<16384; _cmApBuf.abufIdx++)
              {
                _cmApBuf.abuf[ _cmApBuf.abufIdx ] = cp->b[ii];
                ii = (ii + 1) % ip->n;
              }

            }


          incrSmpN = cp->ii > pi ? cp->ii-pi : (ip->n-pi) + cp->ii;
                    
        }

        cmThUIntIncr(&cp->fn,incrSmpN);
      }
    }
  }

  // copy samples from the buffer to the packet
  if( outPktArray != NULL )
  {
    for(i=0; i<outPktCnt; ++i)
    {
      cmApAudioPacket_t* pp = outPktArray + i;           
      cmApIO*            op = _cmApBuf.devArray[pp->devIdx].ioArray + kOutApIdx; // dest io recd

      // if the base timestamp has not yet been set then set it.
      if( op->timeStamp.tv_sec==0 && op->timeStamp.tv_nsec==0 )
        op->timeStamp = pp->timeStamp;

      // for each dest packet channel and enabled source channel
      for(j=0; j<pp->chCnt; ++j)
      {
        cmApCh*  cp = op->chArray + pp->begChIdx + j; // dest ch
        //unsigned n0 = op->n - cp->oi;                 // first src segment
        //unsigned n1 = 0;                              // second src segment
        volatile unsigned fn = cp->fn; // store fn because it may be changed by the client thread

        // if the outgoing samples  will underflow the buffer 
        if( pp->audioFramesCnt > fn )
        {
          ++op->faultCnt;         // record an output underflow

          // if the buffer is empty - zero the packet and return
          if( fn == 0 )
          {
            memset( pp->audioBytesPtr, 0, pp->audioFramesCnt*sizeof(cmApSample_t));
            continue;
          }

          // ... otherwise decrease the count of returned samples
          pp->audioFramesCnt = fn;
        }

        
        // if the outgong segments would go off the end of the buffer then 
        //// arrange to wrap to the begining of the buffer
        //if( n0 < pp->audioFramesCnt )
        //  n1 = pp->audioFramesCnt-n0;
        //else
        //  n0 = pp->audioFramesCnt;

        //cmApSample_t* bpp      = ((cmApSample_t*)pp->audioBytesPtr) + j;
        //cmApSample_t* dp     = bpp;
        bool          enaFl    = cmIsFlag(cp->fl,kChApFl) && cmIsFlag(cp->fl,kMuteApFl)==false;
        unsigned      decrSmpN = 0;
        
        // if the tone is enabled on this channel
        if( enaFl && cmIsFlag(cp->fl,kToneApFl) )
        {
          //_cmApSine(cp, dp, n0, dp + n0*pp->chCnt, n1, pp->chCnt, op->srate );
          decrSmpN =  _cmApSine( cp, (cmApSample_t*)pp->audioBytesPtr, pp->audioFramesCnt * pp->chCnt, j,  pp->chCnt, pp->audioFramesCnt, op->srateMult, op->srate, cp->gain );
        }
        else    // otherwise copy samples from the output buffer to the packet
        {
          //const cmApSample_t* sp = enaFl ? cp->b + cp->oi : _cmApBuf.zeroBuf;
          //const cmApSample_t* ep = sp + n0;

          unsigned pi = cp->oi;
          cp->oi = _cmApCopyOutSamples( enaFl ? cp->b : _cmApBuf.zeroBuf, op->n, cp->oi, (cmApSample_t*)pp->audioBytesPtr, pp->audioFramesCnt, pp->chCnt, j, op->srateMult, cp->gain, &cp->s0 );

          decrSmpN = cp->oi>pi ? cp->oi-pi : (op->n-pi) + cp->oi;

          /*
          if( false )
            if( j == 2 && _cmApBuf.abufIdx < 16384 )
            {
              int ii;
              for(ii=pi; ii!=cp->oi && _cmApBuf.abufIdx<16384; _cmApBuf.abufIdx++)
              {
                _cmApBuf.abuf[ _cmApBuf.abufIdx ] = cp->b[ii];
                ii = (ii + 1) % op->n;
              }

            }
          */
          /*
          // copy the first segment
          for(; sp < ep; dp += pp->chCnt )
            *dp = cp->gain * *sp++;
          
          // if there is a second segment
          if( n1 > 0 )
          {
            // copy the second segment
            sp    = enaFl ? cp->b : _cmApBuf.zeroBuf;
            ep    = sp + n1;
            for(; sp<ep; dp += pp->chCnt )
              *dp = cp->gain * *sp++;

          }
          */
        }

        // update the meter
        if( cmIsFlag(cp->fl,kMeterApFl) )
        {
          cp->m[cp->mi] = _cmApMeter(((cmApSample_t*)pp->audioBytesPtr)+j,pp->audioFramesCnt,pp->chCnt);
          cp->mi        = (cp->mi + 1) % cp->mn;
        }

        // advance the output channel buffer
        /*
        cp->oi  = n1>0 ? n1 : cp->oi + n0;
        cmThUIntDecr(&cp->fn,pp->audioFramesCnt);
        */
        
        //cp->fn -= pp->audioFramesCnt;
        cmThUIntDecr(&cp->fn,decrSmpN);
      }
    }
  }
  return kOkAbRC;
}

unsigned cmApBufMeterMs()
{ return _cmApBuf.meterMs; }

void     cmApBufSetMeterMs( unsigned meterMs )
{
  _cmApBuf.meterMs = cmMin(1000,cmMax(10,meterMs));
}

unsigned cmApBufChannelCount( unsigned devIdx, unsigned flags )
{
  if( devIdx == cmInvalidIdx )
    return 0;

  unsigned idx      = flags & kInApFl     ? kInApIdx : kOutApIdx;
  return _cmApBuf.devArray[devIdx].ioArray[ idx ].chCnt; 
}

void cmApBufSetFlag( unsigned devIdx, unsigned chIdx, unsigned flags )
{
  if( devIdx == cmInvalidIdx )
    return;

  unsigned idx      = flags & kInApFl     ? kInApIdx : kOutApIdx;
  bool     enableFl = flags & kEnableApFl ? true : false; 
  unsigned i        = chIdx != -1 ? chIdx   : 0;
  unsigned n        = chIdx != -1 ? chIdx+1 : _cmApBuf.devArray[devIdx].ioArray[idx].chCnt;
  
  for(; i<n; ++i)
  {
    cmApCh*  cp  = _cmApBuf.devArray[devIdx].ioArray[idx].chArray + i;
    cp->fl = cmEnaFlag(cp->fl, flags & (kChApFl|kToneApFl|kMeterApFl|kMuteApFl|kPassApFl), enableFl );
  }
  
}  

bool cmApBufIsFlag( unsigned devIdx, unsigned chIdx, unsigned flags )
{
  if( devIdx == cmInvalidIdx )
    return false;

  unsigned idx      = flags & kInApFl ? kInApIdx : kOutApIdx;
  return cmIsFlag(_cmApBuf.devArray[devIdx].ioArray[idx].chArray[chIdx].fl,flags);
}


void  cmApBufEnableChannel(   unsigned devIdx, unsigned chIdx, unsigned flags )
{  cmApBufSetFlag(devIdx,chIdx,flags | kChApFl); }

bool  cmApBufIsChannelEnabled(   unsigned devIdx, unsigned chIdx, unsigned flags )
{ return cmApBufIsFlag(devIdx, chIdx, flags | kChApFl); }

void  cmApBufEnableTone(   unsigned devIdx, unsigned chIdx, unsigned flags )
{ cmApBufSetFlag(devIdx,chIdx,flags | kToneApFl); }

bool  cmApBufIsToneEnabled(   unsigned devIdx, unsigned chIdx, unsigned flags )
{ return cmApBufIsFlag(devIdx,chIdx,flags | kToneApFl); }

void  cmApBufEnableMute(   unsigned devIdx, unsigned chIdx, unsigned flags )
{ cmApBufSetFlag(devIdx,chIdx,flags | kMuteApFl); }

bool  cmApBufIsMuteEnabled(   unsigned devIdx, unsigned chIdx, unsigned flags )
{ return cmApBufIsFlag(devIdx,chIdx,flags | kMuteApFl); }

void  cmApBufEnablePass(   unsigned devIdx, unsigned chIdx, unsigned flags )
{ cmApBufSetFlag(devIdx,chIdx,flags | kPassApFl); }

bool  cmApBufIsPassEnabled(   unsigned devIdx, unsigned chIdx, unsigned flags )
{ return cmApBufIsFlag(devIdx,chIdx,flags | kPassApFl); }

void  cmApBufEnableMeter(   unsigned devIdx, unsigned chIdx, unsigned flags )
{ cmApBufSetFlag(devIdx,chIdx,flags | kMeterApFl); }

bool  cmApBufIsMeterEnabled(unsigned devIdx, unsigned chIdx, unsigned flags )
{ return cmApBufIsFlag(devIdx,chIdx,flags | kMeterApFl); }

cmApSample_t cmApBufMeter(unsigned devIdx, unsigned chIdx, unsigned flags )
{
  if( devIdx == cmInvalidIdx )
    return 0;

  unsigned            idx = flags & kInApFl  ? kInApIdx : kOutApIdx;
  const cmApCh*       cp  = _cmApBuf.devArray[devIdx].ioArray[idx].chArray + chIdx;
  return _cmApMeterValue(cp);  
} 

void cmApBufSetGain( unsigned devIdx, unsigned chIdx, unsigned flags, double gain )
{
  if( devIdx == cmInvalidIdx )
    return;

  unsigned idx      = flags & kInApFl     ? kInApIdx : kOutApIdx;
  unsigned i        = chIdx != -1 ? chIdx : 0;
  unsigned n        = i + (chIdx != -1 ? 1     : _cmApBuf.devArray[devIdx].ioArray[idx].chCnt);

  for(; i<n; ++i)
    _cmApBuf.devArray[devIdx].ioArray[idx].chArray[i].gain = gain;
}

double cmApBufGain( unsigned devIdx, unsigned chIdx, unsigned flags )
{
  if( devIdx == cmInvalidIdx )
    return 0;

  unsigned idx   = flags & kInApFl  ? kInApIdx : kOutApIdx;
  return  _cmApBuf.devArray[devIdx].ioArray[idx].chArray[chIdx].gain;  
}


unsigned cmApBufGetStatus( unsigned devIdx, unsigned flags, double* meterArray, unsigned meterCnt, unsigned* faultCntPtr )
{
  if( devIdx == cmInvalidIdx )
    return 0;

  unsigned ioIdx = cmIsFlag(flags,kInApFl) ? kInApIdx : kOutApIdx;
  cmApIO*  iop   = _cmApBuf.devArray[devIdx].ioArray + ioIdx;
  unsigned chCnt = cmMin(iop->chCnt, meterCnt );
  unsigned i;

  if( faultCntPtr != NULL )
    *faultCntPtr = iop->faultCnt;

  for(i=0; i<chCnt; ++i)
    meterArray[i] = _cmApMeterValue(iop->chArray + i);        
  return chCnt;
}


bool cmApBufIsDeviceReady( unsigned devIdx, unsigned flags )
{
  //bool     iFl = true;
  //bool     oFl = true;
  unsigned i   = 0;

  if( devIdx == cmInvalidIdx )
    return false;

  if( flags & kInApFl )
  {
    const cmApIO* ioPtr = _cmApBuf.devArray[devIdx].ioArray + kInApIdx;
    for(i=0; i<ioPtr->chCnt; ++i)
      if( ioPtr->chArray[i].fn < ioPtr->dspFrameCnt )
        return false;

    //iFl = ioPtr->fn > ioPtr->dspFrameCnt;
  }

  if( flags & kOutApFl )
  {
    const cmApIO* ioPtr = _cmApBuf.devArray[devIdx].ioArray + kOutApIdx;

    for(i=0; i<ioPtr->chCnt; ++i)
      if( (ioPtr->n - ioPtr->chArray[i].fn) < ioPtr->dspFrameCnt )
        return false;


    //oFl = (ioPtr->n - ioPtr->fn) > ioPtr->dspFrameCnt;
  } 

  return true;
  //return iFl & oFl;  
}


// Note that his function returns audio samples but does NOT
// change any internal states.
void cmApBufGet( unsigned devIdx, unsigned flags, cmApSample_t* bufArray[], unsigned bufChCnt )
{
  unsigned i;
  if( devIdx == cmInvalidIdx )
  {
    for(i=0; i<bufChCnt; ++i)
      bufArray[i] = NULL;
    return;
  }

  unsigned      idx   = flags & kInApFl ? kInApIdx : kOutApIdx;
  const cmApIO* ioPtr = _cmApBuf.devArray[devIdx].ioArray + idx;
  unsigned      n     = bufChCnt < ioPtr->chCnt ? bufChCnt : ioPtr->chCnt;
  cmApCh*       cp    = ioPtr->chArray;

  for(i=0; i<n; ++i,++cp)
  {
    unsigned offs = flags & kInApFl ? cp->oi : cp->ii;
    bufArray[i] = cmIsFlag(cp->fl,kChApFl) ? cp->b + offs : NULL;
  }
  
}

void _cmApBufCalcTimeStamp( double srate, const cmTimeSpec_t* baseTimeStamp, unsigned frmCnt, cmTimeSpec_t* retTimeStamp )
{
  if( retTimeStamp==NULL )
    return;

  double   secs         = frmCnt / srate;
  unsigned int_secs     = floor(secs);
  double   frac_secs    = secs - int_secs;

  retTimeStamp->tv_nsec = floor(baseTimeStamp->tv_nsec + frac_secs * 1000000000);
  retTimeStamp->tv_sec  = baseTimeStamp->tv_sec  + int_secs;

  if( retTimeStamp->tv_nsec > 1000000000 )
  {
    retTimeStamp->tv_nsec -= 1000000000;
    retTimeStamp->tv_sec  += 1;
  }
}

void cmApBufGetIO(   unsigned iDevIdx, cmApSample_t* iBufArray[], unsigned iBufChCnt, cmTimeSpec_t* iTimeStampPtr, unsigned oDevIdx, cmApSample_t* oBufArray[], unsigned oBufChCnt, cmTimeSpec_t* oTimeStampPtr )
{
  cmApBufGet( iDevIdx, kInApFl, iBufArray, iBufChCnt );
  cmApBufGet( oDevIdx, kOutApFl,oBufArray, oBufChCnt );

  unsigned i       = 0;

  if( iDevIdx != cmInvalidIdx && oDevIdx != cmInvalidIdx )
  {
    const cmApIO* ip       = _cmApBuf.devArray[iDevIdx].ioArray + kInApIdx;
    const cmApIO* op       = _cmApBuf.devArray[oDevIdx].ioArray + kOutApIdx;
    unsigned      minChCnt = cmMin(iBufChCnt,oBufChCnt);  
    unsigned      frmCnt   = cmMin(ip->dspFrameCnt,op->dspFrameCnt);
    unsigned      byteCnt  = frmCnt * sizeof(cmApSample_t);
    
    _cmApBufCalcTimeStamp(ip->srate, &ip->timeStamp, ip->ioFrameCnt, iTimeStampPtr );
    _cmApBufCalcTimeStamp(op->srate, &op->timeStamp, op->ioFrameCnt, oTimeStampPtr );

    for(i=0; i<minChCnt; ++i)
    {
      cmApCh* ocp = op->chArray + i;
      cmApCh* icp = ip->chArray + i;

      if( oBufArray[i] != NULL )
      {
        // if either the input or output channel is marked for pass-through
        if( cmAllFlags(ocp->fl,kPassApFl)  || cmAllFlags(icp->fl,kPassApFl) )
        {
          memcpy( oBufArray[i], iBufArray[i], byteCnt );

          // set the output buffer to NULL to prevent it being over written by the client
          oBufArray[i] = NULL;
        }
        else
        {
          // zero the output buffer
          memset(oBufArray[i],0,byteCnt);
        }
      }
    }
  }

  if( oDevIdx != cmInvalidIdx )
  {
    const cmApIO* op  = _cmApBuf.devArray[oDevIdx].ioArray + kOutApIdx;
    unsigned byteCnt  = op->dspFrameCnt * sizeof(cmApSample_t);

    _cmApBufCalcTimeStamp(op->srate, &op->timeStamp, op->ioFrameCnt, oTimeStampPtr );

    for(; i<oBufChCnt; ++i)
      if( oBufArray[i] != NULL )
        memset( oBufArray[i], 0, byteCnt );
  }

}

void cmApBufAdvance( unsigned devIdx, unsigned flags )
{
  unsigned i;

  if( devIdx == cmInvalidIdx )
    return;

  if( flags & kInApFl )
  {
    cmApIO* ioPtr = _cmApBuf.devArray[devIdx].ioArray + kInApIdx;

    for(i=0; i<ioPtr->chCnt; ++i)
    {
      cmApCh* cp = ioPtr->chArray + i;
      cp->oi     = (cp->oi + ioPtr->dspFrameCnt) % ioPtr->n;
      cmThUIntDecr(&cp->fn,ioPtr->dspFrameCnt);
    }

    // count the number of samples input from this device
    if( ioPtr->timeStamp.tv_sec!=0 && ioPtr->timeStamp.tv_nsec!=0 )
      cmThUIntIncr(&ioPtr->ioFrameCnt,ioPtr->dspFrameCnt);

  }
  
  if( flags & kOutApFl )
  {
    cmApIO* ioPtr = _cmApBuf.devArray[devIdx].ioArray + kOutApIdx;
    for(i=0; i<ioPtr->chCnt; ++i)
    {
      cmApCh* cp = ioPtr->chArray + i;
      cp->ii     = (cp->ii + ioPtr->dspFrameCnt) % ioPtr->n;
      cmThUIntIncr(&cp->fn,ioPtr->dspFrameCnt);
    }

    // count the number of samples output from this device
    if( ioPtr->timeStamp.tv_sec!=0 && ioPtr->timeStamp.tv_nsec!=0 )
      cmThUIntIncr(&ioPtr->ioFrameCnt,ioPtr->dspFrameCnt);
  }
}


void cmApBufInputToOutput( unsigned iDevIdx, unsigned oDevIdx )
{
  if( iDevIdx == cmInvalidIdx || oDevIdx == cmInvalidIdx )
    return;

  unsigned    iChCnt   = cmApBufChannelCount( iDevIdx, kInApFl  );
  unsigned    oChCnt   = cmApBufChannelCount( oDevIdx, kOutApFl );
  unsigned    chCnt    = iChCnt < oChCnt ? iChCnt : oChCnt;
  
  unsigned    i;

  cmApSample_t* iBufPtrArray[ iChCnt ];
  cmApSample_t* oBufPtrArray[ oChCnt ];


  while( cmApBufIsDeviceReady( iDevIdx, kInApFl ) && cmApBufIsDeviceReady( oDevIdx, kOutApFl ) )
  {
    cmApBufGet( iDevIdx, kInApFl,  iBufPtrArray, iChCnt );
    cmApBufGet( oDevIdx, kOutApFl, oBufPtrArray, oChCnt );

    // Warning: buffer pointers to disabled channels will be set to NULL

    for(i=0; i<chCnt; ++i)
    {
      cmApIO* ip = _cmApBuf.devArray[iDevIdx ].ioArray + kInApIdx;
      cmApIO* op = _cmApBuf.devArray[oDevIdx].ioArray + kOutApIdx;

      assert( ip->dspFrameCnt == op->dspFrameCnt );

      unsigned    byteCnt  = ip->dspFrameCnt * sizeof(cmApSample_t);


      if( oBufPtrArray[i] != NULL )
      {      
        // the input channel is not disabled
        if( iBufPtrArray[i]!=NULL )
          memcpy(oBufPtrArray[i],iBufPtrArray[i],byteCnt);    
        else
          // the input channel is disabled but the output is not - so fill the output with zeros
          memset(oBufPtrArray[i],0,byteCnt);
      }
    }

    cmApBufAdvance( iDevIdx, kInApFl );
    cmApBufAdvance( oDevIdx, kOutApFl );
  }
  
}

void cmApBufReport( cmRpt_t* rpt )
{
  unsigned i,j,k;
  for(i=0; i<_cmApBuf.devCnt; ++i)
  {
    cmRptPrintf(rpt,"%i ",i);

    for(j=0; j<kIoApCnt; ++j)
    {
      cmApIO* ip = _cmApBuf.devArray[i].ioArray + j;

      unsigned ii = 0;
      unsigned oi = 0;
      unsigned fn  = 0;
      cmApSample_t mtr = 0;
      for(k=0; k<ip->chCnt; ++k)
      {
        cmApCh* cp = ip->chArray + i;
        ii += cp->ii;
        oi += cp->oi;
        fn += cp->fn;
        mtr += _cmApMeterValue(cp);
      }

      cmRptPrintf(rpt,"%s - i:%7i o:%7i f:%7i n:%7i err %s:%7i  mtr:%5.4f ",
        j==0?"IN ":"OUT",
        ii,oi,fn,ip->n, (j==0?"over ":"under"), ip->faultCnt, mtr);
      
    }

    cmRptPrintf(rpt,"\n");
  }
}

//{ { label:cmApBufExample }
//(
// cmApBufTest() demonstrates the audio buffer usage.
//)

//(

void cmApBufTest( cmRpt_t* rpt )
{
  unsigned devIdx         = 0;
  unsigned devCnt         = 1 ;
  unsigned dspFrameCnt    = 10;
  unsigned cycleCnt       = 3;
  unsigned framesPerCycle = 25;
  unsigned inChCnt        = 2;
  unsigned outChCnt       = inChCnt;
  unsigned sigN           = cycleCnt*framesPerCycle*inChCnt;
  double   srate          = 44100.0;
  unsigned meterMs        = 50;
  int      srateMult      = 1;
  
  unsigned          bufChCnt= inChCnt;
  cmApSample_t*     inBufArray[ bufChCnt ];
  cmApSample_t*     outBufArray[ bufChCnt ];
  cmApSample_t      iSig[ sigN ];
  cmApSample_t      oSig[ sigN ];
  cmApSample_t*     os = oSig;
  cmApAudioPacket_t pkt;
  int        i,j;

  // create a simulated signal
  for(i=0; i<sigN; ++i)
  {
    iSig[i]   = i;
    oSig[i] = 0;
  }

  pkt.devIdx         = 0;
  pkt.begChIdx       = 0;
  pkt.chCnt          = inChCnt;
  pkt.audioFramesCnt = framesPerCycle;
  pkt.bitsPerSample  = 32;
  pkt.flags          = 0;
 

  // initialize a the audio buffer 
  cmApBufInitialize(devCnt,meterMs);

  // setup the buffer with the specific parameters to by used by the host audio ports
  cmApBufSetup(devIdx,srate,dspFrameCnt,cycleCnt,inChCnt,framesPerCycle,outChCnt,framesPerCycle, srateMult);

  // simulate cylcing through sigN buffer transactions
  for(i=0; i<sigN; i+=framesPerCycle*inChCnt)
  {
    // setup an incoming audio packet
    pkt.audioFramesCnt = framesPerCycle;
    pkt.audioBytesPtr  = iSig+i;

    // simulate a call from the audio port with incoming samples 
    // (fill the audio buffers internal input buffers)
    cmApBufUpdate(&pkt,1,NULL,0);

    // if all devices need to be serviced
    while( cmApBufIsDeviceReady( devIdx, kInApFl | kOutApFl ))
    {
      // get pointers to full internal input buffers
      cmApBufGet(devIdx, kInApFl, inBufArray, bufChCnt );
      
      // get pointers to empty internal output buffers
      cmApBufGet(devIdx, kOutApFl, outBufArray, bufChCnt );

      // Warning: pointers to disabled channels will be set to NULL.

      // simulate a play through by copying the incoming samples to the outgoing buffers.
      for(j=0; j<bufChCnt; ++j)
        if( outBufArray[j] != NULL )
        {
          // if the input is disabled - but the output is not then zero the output buffer
          if( inBufArray[j] == NULL )
            memset( outBufArray[j], 0, dspFrameCnt*sizeof(cmApSample_t));
          else
            // copy the input to the output
            memcpy( outBufArray[j], inBufArray[j], dspFrameCnt*sizeof(cmApSample_t));
        }

      // signal the buffer that this cycle is complete.
      // (marks current internal input/output buffer empty/full)
      cmApBufAdvance( devIdx, kInApFl | kOutApFl );
    }

    pkt.audioBytesPtr = os;
    
    // simulate a call from the audio port picking up outgoing samples
    // (empties the audio buffers internal output buffers)
    cmApBufUpdate(NULL,0,&pkt,1);

    os += pkt.audioFramesCnt * pkt.chCnt;
  }

  for(i=0; i<sigN; ++i)
    cmRptPrintf(rpt,"%f ",oSig[i]);
  cmRptPrintf(rpt,"\n");

  cmApBufFinalize();
}

//)
//}


