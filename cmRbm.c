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
#include "cmMath.h"
#include "cmFile.h"
#include "cmSymTbl.h"
#include "cmMidi.h"
#include "cmAudioFile.h"
#include "cmVectOpsTemplateMain.h"
#include "cmStack.h"
#include "cmProcObj.h"
#include "cmProcTemplateMain.h"
#include "cmVectOps.h"
#include "cmProc.h"
#include "cmProc2.h"
#include "cmRbm.h"


typedef struct
{
  double trainErr;
  double testErr;
} cmRbmMonitor_t;

  
cmRbmRC_t cmRbmWriteMonitorFile( cmCtx_t* c, cmStackH_t monH, const cmChar_t* fn )
{
  cmRbmRC_t rc  = kOkRbmRC;

  cmCtx*    ctx = cmCtxAlloc(NULL, c->err.rpt, cmLHeapNullHandle,  cmSymTblNullHandle );

  if( cmBinMtxFileWrite(fn, cmStackCount(monH), sizeof(cmRbmMonitor_t)/sizeof(double), NULL, cmStackFlatten(monH), ctx, c->err.rpt ) != cmOkRC )
  {
    rc = cmErrMsg(&c->err,kMonitorWrFailRbmRC,"Training monitor file '%s' write failed.",cmStringNullGuard(fn));
    goto errLabel;
  }

 errLabel:
  cmCtxFree(&ctx);
  return rc;
}

double* cmRbmReadDataFile( cmCtx_t* c, const char* fn, unsigned* dimNPtr, unsigned* pointCntPtr )
{
  unsigned rowCnt,colCnt,eleByteCnt;

  *dimNPtr = 0;
  *pointCntPtr = 0;

  if( cmBinMtxFileSize(c, fn, &rowCnt, &colCnt, &eleByteCnt ) != cmOkRC )
    return NULL;

  double* buf = cmMemAllocZ(double,rowCnt*colCnt);

  if( cmBinMtxFileRead(c, fn, rowCnt, colCnt, sizeof(double), buf,NULL) != cmOkRC )
  {
    cmMemFree(buf);
    return NULL;
  }

  *dimNPtr = rowCnt;
  *pointCntPtr = colCnt;

  return buf;

}


// Generate a matrix of 'pointsN' random binary valued column vectors of dimension dimN.
// The first i = {0...'dimN'-1} elements of each vector contain ones with prob probV[i] 
// (or zeros with prob 1 - probV[i].).  probV[i] in [0.0,1.0].
// The last element in each column is set to zero.
// The returned matrix m[ dimN+1, pointsN ] is in column major order and 
// must be deleted by the caller (e.g. cmMemFree(m)).
double* cmRbmGenBinaryTestData( cmCtx_t* c, const char* fn, const double* probV, unsigned dimN, unsigned pointsN )
{
  if( dimN == 0 || pointsN == 0 )
    return NULL;

  double* m = cmMemAllocZ( double, dimN*pointsN );
  unsigned i,j;
  for(i=0; i<pointsN; ++i)
    for(j=0; j<dimN; ++j)
      m[ i*dimN + j ] =  rand() < (probV[j] * RAND_MAX);

  if( fn != NULL )
    cmBinMtxFileWrite(fn,dimN,pointsN,NULL,m,NULL,c->err.rpt);

  return m;
}

typedef struct
{
  unsigned vN;
  double*  vs;  
  double*  vp;
  double*  vb;
  double*  vd; // std dev. (var = std_dev^2)

  unsigned hN;
  double*  hs;
  double*  hp;
  double*  hb;

  double*  W;   // W[vN,hN]

  cmStackH_t monH;
} cmRBM_t;  

void _cmRbmPrint( cmRBM_t* r, cmRpt_t* rpt )
{
  cmVOD_PrintL("hb", rpt, 1, r->hN, r->hb );
  cmVOD_PrintL("hp", rpt, 1, r->hN, r->hp );
  cmVOD_PrintL("hs", rpt, 1, r->hN, r->hs );

  cmVOD_PrintL("vb", rpt, 1, r->vN, r->vb );
  cmVOD_PrintL("vp", rpt, 1, r->vN, r->vp );
  cmVOD_PrintL("vs", rpt, 1, r->vN, r->vs );

  cmVOD_PrintL("W", rpt, r->vN, r->hN, r->W );
 
}

void _cmRbmRelease( cmRBM_t* r )
{
  cmStackFree(&r->monH);
  cmMemFree(r);
}

// Adjust the layer geometry to force all sizes to be a multiple of 16 bytes.
// This assumes that all data will be 8 byte doubles.
void _cmRbmAdjustSizes( unsigned* vNp, unsigned* hNp, unsigned* dNp )
{
  *vNp =  *vNp + (cmIsOddU(*vNp) ? 1 : 0);
  *hNp =  *hNp + (cmIsOddU(*hNp) ? 1 : 0);

  if( dNp != NULL )
    *dNp =  *dNp + (cmIsOddU(*dNp) ? 1 : 0);
}


cmRBM_t* _cmRbmAlloc( cmCtx_t* ctx, unsigned vN, unsigned hN )
{
  unsigned monInitCnt   = 1000;
  unsigned monExpandCnt = 1000;

  // adjust sizes to force base array addresses to be a multiple of 16 bytes.
  unsigned vn           = vN;
  unsigned hn           = hN;
  _cmRbmAdjustSizes(&vn,&hn,NULL);

  unsigned rn = sizeof(cmRBM_t);

  // force record to be a multiple of 16
  if( rn % 16 )
    rn += 16 - (rn % 16);     

  unsigned dn = 4*vn + 3*hn + vn*hn;
  unsigned bn = rn + dn*sizeof(double);

  char*    cp = cmMemAllocZ(char,bn);
  cmRBM_t*  r = (cmRBM_t*)cp;

  r->vs = (double*)(cp+rn);
  r->vp = r->vs + vn;
  r->vb = r->vp + vn;
  r->vd = r->vb + vn;
  r->hs = r->vd + vn;
  r->hp = r->hs + hn;
  r->hb = r->hp + hn;
  r->W  = r->hb + hn;
  r->vN = vN;
  r->hN = hN;

  assert(cp+bn == (char*)(r->W + vn*hn));

  if( cmStackAlloc(ctx, &r->monH, monInitCnt, monExpandCnt, sizeof(cmRbmMonitor_t)) != kOkStRC )
  {
    cmErrMsg(&ctx->err,kStackFailRbmRC,"Stack allocation failed for the training monitor data array.");
    goto errLabel;
  }

  return r;

 errLabel:
  _cmRbmRelease(r);
  return NULL;
  
}

void cmRbmBinaryTrain( 
  cmCtx_t*           ctx, 
  cmRBM_t*           r,
  cmRbmTrainParms_t* p, 
  unsigned           dMN,
  const double*      dM )
{
  cmRpt_t* rpt     = ctx->err.rpt;
  bool     stochFl = true;
  unsigned i,j,k,ei,di;

  unsigned vN   = r->vN;
  unsigned hN   = r->hN;

  // adjust the memory sizes to align all arrays on 16 byte boundaries
  unsigned vn   = vN;
  unsigned hn   = hN;
  unsigned dn   = p->batchCnt;
  _cmRbmAdjustSizes(&vn,&hn,&dn);

  unsigned mn   = (3 * hn * vn) + (1 * vn) + (1 * hn) + (3 * hn * dn) + (2 * vn * dn);
  double*  m    = cmMemAllocZ(double,mn);
  double*  vh0M = m;              // vh0M[ hN, vN ]
  double*  vh1M = vh0M + hn*vn;   // vh1M[ hN, vN ]
  double*  dwM  = vh1M + hn*vn;   // dwM[  hN, vN ]
  double*  vdbV = dwM  + hn*vn;   // vdbV[ vN ]
  double*  hdbV = vdbV + vn;      // hdbV[ hN ]
  double*  hp0M = hdbV + hn;      // hp0M[ hN, dN ]
  double*  hs0M = hp0M + dn * hn; // hs0M[ dN, hN ]
  double*  hp1M = hs0M + dn * hn; // hp1M[ hN, dN ]
  double*  vp1M = hp1M + dn * hn; // vp1M[ dN, vN ]
  double*  vs1M = vp1M + dn * vn; // vs1M[ vN, dN ]

  assert( vs1M + vn * dn == m + mn );

  // initilaize the weights with random values
  // W = p->initW * randn(vN,hN,0.0,1.0)
  for(i=0; i<vN; ++i)
    cmVOD_RandomGauss( r->W + i*hN,  hN, 0.0, 1.0 );
  cmVOD_MultVS( r->W, hN*vN, p->initW);

  if(0)
  {
    const cmChar_t* fn = "/home/kevin/temp/cmRbmWeight.mtx"; 
    //cmBinMtxFileWrite(fn,hN, vN,NULL,dM,NULL,ctx->err.rpt);
    cmBinMtxFileRead( ctx, fn, hN, vN, sizeof(double), r->W,NULL);
  }

  cmVOD_Zero( dwM,  vN*hN );
  cmVOD_Zero( vdbV, vN );
  cmVOD_Zero( hdbV, hN );

  for(ei=0; ei<p->epochCnt; ++ei)
  {
    unsigned dN = 0;
    double  err = 0;

    for(di=0; di<dMN; di+=dN)
    {
      dN = cmMin(p->batchCnt,dMN-di);
      const double* d = dM + di * vN;  // d[ vN, dN ]

      //
      // Update hidden layer from data
      //

      // hp0M[hN,dN] = W[hN,vN] * d[vN,dN]
      cmVOD_MultMMM(hp0M,hN,dN,r->W,d,vN);

      // calc hs0M[dN,hN]
      for(k=0; k<dN; ++k)
        for(j=0; j<hN; ++j)
        {
          hp0M[ k*hN + j ] = 1.0/(1.0 + exp(-(hp0M[ k*hN + j] + r->hb[j])));
          hs0M[ j*dN + k ] = rand() < hp0M[ k*hN + j ] * RAND_MAX;

          if( !stochFl )
            hs0M[ j*dN + k ] = hp0M[ k*hN + j ] > 0.5;
        }

      // 
      // Reconstruct visible layer from hidden
      //

      // vp1M[dN,vN] = hs0M[dN,hN] * W[hN,vN]
      cmVOD_MultMMM(vp1M,dN,vN,hs0M,r->W,hN);

      // calc vs1M[vN,dN]
      for(k=0; k<dN; ++k)
        for(i=0; i<vN; ++i)
        {
          vp1M[ i*dN + k ] = 1.0/(1.0 + exp(-( vp1M[ i*dN + k ] + r->vb[i]) ) );
          vs1M[ k*vN + i ] = rand() < vp1M[ i*dN + k ] * RAND_MAX;

          if( !stochFl )
            vs1M[ k*vN + i ] = vp1M[ i*dN + k ] > 0.5;

          // calc training error
          err += pow(d[ k*vN + i ] - vp1M[ i*dN + k ],2.0);
        }


      //
      // Update hidden layer from reconstruction
      //

      // hp1M[hN,dN] = W[hN,vN] * vs1[vN,dN]
      cmVOD_MultMMM(hp1M,hN,dN,r->W,vs1M,vN);

      // calc hp1M[hN,dN]
      for(k=0; k<dN; ++k)
        for(j=0; j<hN; ++j)
          hp1M[ k*hN + j ] = 1.0/(1.0 + exp( -hp1M[ k*hN + j ] - r->hb[j] ));

      if(0)
      {
        cmVOD_PrintL("hp0M",rpt,hN,dN,hp0M);
        cmVOD_PrintL("hs0M",rpt,dN,hN,hs0M);
        cmVOD_PrintL("vp1M",rpt,dN,vN,vp1M);
        cmVOD_PrintL("vs1M",rpt,vN,dN,vs1M);
        cmVOD_PrintL("hp1M",rpt,hN,dN,hp1M);
      }


      //
      //  Update Wieghts
      //

      // vh0M[hN,vN] = hp0M[hN,dN] * d[vN,dN]'
      cmVOD_MultMMMt(vh0M, hN, vN, hp0M, d,    dN );
      cmVOD_MultMMMt(vh1M, hN, vN, hp1M, vs1M, dN );

      for(i=0; i<hN*vN; ++i)
      {
        dwM[i]   = p->momentum * dwM[i] + p->eta * ( (vh0M[i] - vh1M[i]) / dN );
        r->W[i] += dwM[i];
      }

      //
      // Update hidden bias
      //

      // sum(hp0M - hp1M,2)   - sum the difference of rows of hp0M and hp1M
      cmVOD_SubVV(hp0M,hN*dN,hp1M); // hp0M -= hp1M 
      cmVOD_SumMN(hp0M,hN,dN,hp1M); // hp1M[1:hN] = sum(hp0M,2) (note: hp1M is rused as temp space)
      
      for(j=0; j<hN; ++j)
      {
        hdbV[j]   =  p->momentum * hdbV[j] + p->eta * (hp1M[j] / dN);
        r->hb[j] += hdbV[j];
      }

      //
      // Update visible bias
      //

      // sum(d - vs1M, 2) 
      cmVOD_SubVVV(vp1M,vN*dN,d,vs1M); // vp1M = d - vs1M          (vp1M is reused as temp space)
      cmVOD_SumMN(vp1M,vN,dN,vs1M);    // vs1M[1:vn] = sum(vp1M,2) (vs1M is reused as temp space)

      for(i=0; i<vN; ++i)
      {
        vdbV[i]   = p->momentum * vdbV[i] +  p->eta * (vs1M[i] / dN );
        r->vb[i] += vdbV[i];
      }

      if(0)
      {
        cmVOD_PrintL("dwM", rpt, vN, hN, dwM );
        cmVOD_PrintL("vdbV",rpt, 1,  vN, vdbV );
        cmVOD_PrintL("hdbV",rpt, 1,  hN, hdbV );
        cmVOD_PrintL("W",   rpt, vN, hN, r->W );
        cmVOD_PrintL("vb",  rpt, 1, vN, r->vb );
        cmVOD_PrintL("hb",  rpt, 1, hN, r->hb );
      }


    } // di

    cmRptPrintf(rpt,"err:%f\n",err);

    if( cmStackIsValid(r->monH))
    {
      cmRbmMonitor_t monErr;
      monErr.trainErr = err;
      cmStackPush(r->monH,&monErr,1);
    }
  } // ei


  cmRptPrintf(rpt,"eta:%f momentum:%f\n",p->eta,p->momentum);
  cmVOD_PrintL("dwM", rpt, vN, hN, dwM );
  cmVOD_PrintL("vdbV",rpt, 1,  vN, vdbV );
  cmVOD_PrintL("hdbV",rpt, 1,  hN, hdbV );
  cmVOD_PrintL("W",   rpt, vN, hN, r->W );
  cmVOD_PrintL("vb",  rpt, 1, vN, r->vb );
  cmVOD_PrintL("hb",  rpt, 1, hN, r->hb );

  cmMemFree(m);

}


void cmRbmRealTrain( 
  cmCtx_t*           ctx, 
  cmRBM_t*           r,
  cmRbmTrainParms_t* p, 
  unsigned           dMN,
  const double*      dM )
{
  cmRpt_t* rpt     = ctx->err.rpt;
  unsigned i,j,k,ei,di;

  unsigned vN   = r->vN;
  unsigned hN   = r->hN;

  // adjust the memory sizes to align all arrays on 16 byte boundaries
  unsigned vn   = vN;
  unsigned hn   = hN;
  unsigned dn   = p->batchCnt;
  _cmRbmAdjustSizes(&vn,&hn,&dn);

  unsigned mn   = (3 * hn * vn) + (1 * vn) + (1 * hn) + (3 * hn * dn) + (2 * vn * dn);
  double*  m    = cmMemAllocZ(double,mn);
  double*  vh0M = m;              // vh0M[ hN, vN ]
  double*  vh1M = vh0M + hn*vn;   // vh1M[ hN, vN ]
  double*  dwM  = vh1M + hn*vn;   // dwM[  hN, vN ]
  double*  vdbV = dwM  + hn*vn;   // vdbV[ vN ]
  double*  hdbV = vdbV + vn;      // hdbV[ hN ]
  double*  hp0M = hdbV + hn;      // hp0M[ hN, dN ]
  double*  hs0M = hp0M + dn * hn; // hs0M[ dN, hN ]
  double*  hp1M = hs0M + dn * hn; // hp1M[ hN, dN ]
  double*  vp1M = hp1M + dn * hn; // vp1M[ dN, vN ]
  double*  vs1M = vp1M + dn * vn; // vs1M[ vN, dN ]

  assert( vs1M + vn * dn == m + mn );

  //
  // Initilaize the weights with small random values
  // W = p->initW * randn(vN,hN,0.0,1.0)
  for(i=0; i<vN; ++i)
    cmVOD_RandomGauss( r->W + i*hN,  hN, 0.0, 1.0 );
  cmVOD_MultVS( r->W, hN*vN, p->initW);

  if(0)
  {
    const cmChar_t* fn = "/home/kevin/temp/cmRbmWeight.mtx"; 
    //cmBinMtxFileWrite(fn,hN, vN,NULL,dM,NULL,ctx->err.rpt);
    cmBinMtxFileRead( ctx, fn, hN, vN, sizeof(double), r->W,NULL);
  }

  cmVOD_Zero( dwM,  vN*hN );
  cmVOD_Zero( vdbV, vN );
  cmVOD_Zero( hdbV, hN );

  for(ei=0; ei<p->epochCnt; ++ei)
  {
    unsigned dN = 0;
    double  err = 0;

    for(di=0; di<dMN; di+=dN)
    {
      dN = cmMin(p->batchCnt,dMN-di);
      const double* d = dM + di * vN;  // d[ vN, dN ]

      //
      // Update hidden layer from data
      //

      // hp0M[hN,dN] = W[hN,vN] * d[vN,dN]
      cmVOD_MultMMM(hp0M,hN,dN,r->W,d,vN);

      // calc hs0M[dN,hN]
      for(k=0; k<dN; ++k)
        for(j=0; j<hN; ++j)
        {
          hp0M[ k*hN + j ] = 1.0/(1.0 + exp(-(hp0M[ k*hN + j] + r->hb[j])));
          hs0M[ j*dN + k ] = rand() < hp0M[ k*hN + j ] * RAND_MAX;
        }

      // 
      // Reconstruct visible layer from hidden
      //

      // vp1M[dN,vN] = hs0M[dN,hN] * W[hN,vN]
      cmVOD_MultMMM(vp1M,dN,vN,hs0M,r->W,hN);

      // calc vs1M[vN,dN]
      for(k=0; k<dN; ++k)
        for(i=0; i<vN; ++i)
        {
          vp1M[ i*dN + k ] =  r->vd[i] * vp1M[ i*dN + k ] + r->vb[i];

          cmVOD_GaussPDF(vs1M + k*vN + i, 1, vp1M + i*dN + k,  r->vb[i], r->vd[i] );

          // calc training error
          err += pow(d[ k*vN + i ] - vp1M[ i*dN + k ],2.0);
        }


      //
      // Update hidden layer from reconstruction
      //

      // hp1M[hN,dN] = W[hN,vN] * vs1[vN,dN]
      cmVOD_MultMMM(hp1M,hN,dN,r->W,vs1M,vN);

      // calc hp1M[hN,dN]
      for(k=0; k<dN; ++k)
        for(j=0; j<hN; ++j)
          hp1M[ k*hN + j ] = 1.0/(1.0 + exp( -hp1M[ k*hN + j ] - r->hb[j] ));

      if(0)
      {
        cmVOD_PrintL("hp0M",rpt,hN,dN,hp0M);
        cmVOD_PrintL("hs0M",rpt,dN,hN,hs0M);
        cmVOD_PrintL("vp1M",rpt,dN,vN,vp1M);
        cmVOD_PrintL("vs1M",rpt,vN,dN,vs1M);
        cmVOD_PrintL("hp1M",rpt,hN,dN,hp1M);
      }


      //
      //  Update Wieghts
      //

      // vh0M[hN,vN] = hp0M[hN,dN] * d[vN,dN]'
      cmVOD_MultMMMt(vh0M, hN, vN, hp0M, d,    dN );
      cmVOD_MultMMMt(vh1M, hN, vN, hp1M, vs1M, dN );

      for(i=0,k=0; i<vN; ++i)
        for(j=0; j<hN; ++j,++k)
        {
          dwM[k]   = p->momentum * dwM[k] + p->eta  * ( (vh0M[k] - vh1M[k]) / (dN * r->vd[i]) );
          r->W[k] += dwM[k];
        }

      //
      // Update hidden bias
      //

      // sum(hp0M - hp1M,2)   - sum the difference of rows of hp0M and hp1M
      cmVOD_SubVV(hp0M,hN*dN,hp1M); // hp0M -= hp1M 
      cmVOD_SumMN(hp0M,hN,dN,hp1M); // hp1M[1:hN] = sum(hp0M,2) (note: hp1M is rused as temp space)
      
      for(j=0; j<hN; ++j)
      {
        hdbV[j]   =  p->momentum * hdbV[j] + p->eta * (hp1M[j] / (dN * r->vd[i] * r->vd[i]));
        r->hb[j] += hdbV[j];
      }

      //
      // Update visible bias
      //

      // sum(d - vs1M, 2) 
      cmVOD_SubVVV(vp1M,vN*dN,d,vs1M); // vp1M = d - vs1M          (vp1M is reused as temp space)
      cmVOD_SumMN(vp1M,vN,dN,vs1M);    // vs1M[1:vn] = sum(vp1M,2) (vs1M is reused as temp space)

      for(i=0; i<vN; ++i)
      {
        vdbV[i]   = p->momentum * vdbV[i] +  p->eta * (vs1M[i] / dN );
        r->vb[i] += vdbV[i];
      }

      for(i=0; i<vN; ++i)
      {
        for(j=0; j<hN; ++j)
        {
          double sum_d = 0;
          double sum_m = 0;
          for(k=0; k<dN; ++k)
          {
            sum_d += hs0M[ j*dN + k ] * r->W[ i*hN + j ];
            sum_m += hp1M[ k*hN + j ] * r->W[ i*hN + j ];
          }
        }

        
      }


      if(0)
      {
        cmVOD_PrintL("dwM", rpt, vN, hN, dwM );
        cmVOD_PrintL("vdbV",rpt, 1,  vN, vdbV );
        cmVOD_PrintL("hdbV",rpt, 1,  hN, hdbV );
        cmVOD_PrintL("W",   rpt, vN, hN, r->W );
        cmVOD_PrintL("vb",  rpt, 1, vN, r->vb );
        cmVOD_PrintL("hb",  rpt, 1, hN, r->hb );
      }


    } // di

    cmRptPrintf(rpt,"err:%f\n",err);

    if( cmStackIsValid(r->monH))
    {
      cmRbmMonitor_t monErr;
      monErr.trainErr = err;
      cmStackPush(r->monH,&monErr,1);
    }
  } // ei


  cmRptPrintf(rpt,"eta:%f momentum:%f\n",p->eta,p->momentum);
  cmVOD_PrintL("dwM", rpt, vN, hN, dwM );
  cmVOD_PrintL("vdbV",rpt, 1,  vN, vdbV );
  cmVOD_PrintL("hdbV",rpt, 1,  hN, hdbV );
  cmVOD_PrintL("W",   rpt, vN, hN, r->W );
  cmVOD_PrintL("vb",  rpt, 1, vN, r->vb );
  cmVOD_PrintL("hb",  rpt, 1, hN, r->hb );

  cmMemFree(m);

}

void cmRbmBinaryTest( cmCtx_t* ctx )
{

  const char* monitorFn     = "/home/kevin/temp/cmRbmMonitor0.mtx";
  const char* dataFn        = "/home/kevin/temp/cmRbmData0.mtx";
  unsigned pointsN          = 1000;
  unsigned dimN             = 4;
  unsigned vN               = dimN;
  unsigned hN               = 32;
  //double   probV[]          = {0.1,0.2,0.8,0.7};
  cmRbmTrainParms_t r;
  cmRBM_t* rbm;

  r.maxX        = 1.0;
  r.minX        = 0.0;
  r.initW       = 0.1;
  r.eta         = 0.01;
  r.holdOutFrac = 0.1;
  r.epochCnt    = 10;
  r.momentum    = 0.5;
  r.batchCnt    = 10;


  if(0)
  {
    vN      = 4;
    hN      = 6;

    double d[] = {
      0,   1,   1,   1,
      1,   1,   1,   1,
      0,   0,   1,   0,
      0,   0,   1,   0,
      0,   1,   1,   0,
      0,   1,   1,   1,
      1,   1,   1,   1,
      0,   0,   1,   0,
      0,   0,   1,   0,
      0,   1,   1,   0
    };

    if( (rbm = _cmRbmAlloc(ctx, vN, hN )) == NULL )
      return;

    pointsN = sizeof(d) / (sizeof(d[0]) * vN);
    cmRbmBinaryTrain(ctx,rbm,&r,pointsN,d);
    return;
  }

  if( (rbm = _cmRbmAlloc(ctx, vN, hN )) == NULL )
    return;    
 
  //double* data0M = cmRbmGenBinaryTestData(ctx,dataFn,probV,dimN,pointsN);
  double* data0M = cmRbmReadDataFile(ctx,dataFn,&dimN,&pointsN);

  double t[ vN ];
  // Sum the columns of sp[srn,scn] into dp[scn]. 
  // dp[] is zeroed prior to computing the sum.
  cmVOD_SumMN(data0M, dimN, pointsN, t );
  
  cmVOD_Print( &ctx->rpt, 1, dimN, t );

  if(0)
  {
    // 
    // Standardize data (subtract mean and divide by standard deviation)
    // then set the visible layers initial standard deviation to 1.0.
    //
    cmVOD_StandardizeRows( data0M, rbm->vN, pointsN, NULL, NULL );
    cmVOD_Fill( rbm->vd, rbm->vN, 1.0 );
    cmRbmRealTrain(ctx,rbm,&r,pointsN,data0M);
  }


  cmRbmBinaryTrain(ctx,rbm,&r,pointsN,data0M);

  cmRbmWriteMonitorFile(ctx, rbm->monH, monitorFn );

  cmMemFree(data0M);

  _cmRbmRelease(rbm);

}
