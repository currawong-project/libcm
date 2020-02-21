#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmText.h"
#include "cmFileSys.h"
#include "cmSymTbl.h"
#include "cmJson.h"
#include "cmPrefs.h"
#include "cmDspValue.h"
#include "cmMsgProtocol.h"
#include "cmThread.h"
#include "cmUdpPort.h"
#include "cmUdpNet.h"
#include "cmSerialPort.h"
#include "cmTime.h"
#include "cmAudioSys.h"
#include "cmProcObj.h"
#include "cmDspCtx.h"
#include "cmDspClass.h"
#include "cmDspSys.h"
#include "cmDspPgm.h"
#include "cmDspPgmPPMain.h"

#include "cmAudioFile.h"
#include "cmProcObj.h"
#include "cmProc.h"
#include "cmProc3.h"

#include "cmVectOpsTemplateMain.h"
#include "cmVectOps.h"

/*

  subnet [ aout ] = fx_chain( ain, hzV ) 
  {
    cmpThreshDb = -40.0;
    cmpRatio    = 4;
    cmpAtkMs    = 30.0;
    cmpRlsMs    = 100.0;
 
    dstInGain   = 1.0;
    dstDSrate   = 44100.0;
    dstBits     = 24;

    eqMode      = "LP";
    eqHz        = hzV[ _i ];

    dlyMaxMs    = 10000.0;
    dlyFb       = 0.8;

    loop = loopRecd( );
    ps   = pitchShift( );
    cmp  = compressor( cmpThreshDb, cmpRatio, cmpAtkMs, cmpRlsMs );
    dst  = distortion( dstInGain, dstDSrate, dstBits );
    eq   = eq( eqMode, eqHz );
    dly  = delay( dlyMaxMs, dlyFb );
    
    ain       -> in.loop.out -> in.ps.out -> in.ps.out
    loop.out -> ps.in;
    ps.out   -> cmp.in;
    cmp.out  -> dst.in;
    dst.out  -> eq.in;
    eq.out   -> dly.in;
    dly.out  -> aout
  }


  audInMapV  = [ 0, 1, 0, 1];
  audOutMapV = [ 0, 1 ];
  hzV        = [ 110 220 440 880 ];

  ain        = audioInput( audioInMap, 1.0 )[ chCnt ];
  ef         = envFollower( )[ chCnt ];
  mtr        = meter(0.0,1.0,0.0)[ chCnt ];
  fx         = fx_chain( ain )( hzV )[ chCnt ];
  aout       = audioOutput( audioOutMap, 1.0 )[ chCnt ];

  ain.out -> ef.in
  ef.rms  -> mtr.in
  

 */

// Use the resource int arrays 'pitchList' and 'groupList' to create
// the following resource int arrays:
// CDmidi  - MIDI pitch of all chord notes
// CD0midi - MIDI pitch of all low/high chord notes
// CD1midi - MIDI pitch of all middle chord notes
// NONmidi - MIDI pitch of all non-chord notes.
// CDchan  - channel index of all chord notes
// CD0chan - channel index of all low/high chord notes
// CD1chan - channel index of all middle chord notes
// NONchan - channel index of all non-chord notes.
// 
cmDspRC_t _cmDspPgm_Main_ProcRsrc( cmDspSysH_t h, cmErr_t* err )
{
  cmDspRC_t       rc;
  unsigned*       midiList           = NULL;
  unsigned        midiCnt            = 0;
  unsigned*       groupList          = NULL;
  unsigned        groupCnt           = 0;
  const cmChar_t* midiListRsrcLabel  = "midiList";
  const cmChar_t* groupListRsrcLabel = "groupList";
  cmJsonH_t       jsH                = cmDspSysPgmRsrcHandle(h);
  unsigned        i,j,k;

  if((rc = cmDspRsrcUIntArray(h,&midiCnt,&midiList,midiListRsrcLabel,NULL)) != kOkDspRC )
  {
    rc = cmErrMsg(err,kRsrcNotFoundDspRC,"The resource '%s' could not be read.",midiListRsrcLabel);
    goto errLabel;
  }

  if((rc = cmDspRsrcUIntArray(h,&groupCnt,&groupList,groupListRsrcLabel,NULL)) != kOkDspRC )
  {
    rc = cmErrMsg(err,kRsrcNotFoundDspRC,"The resource '%s' could not be read.",groupListRsrcLabel);
    goto errLabel;
  }

  if( midiCnt !=  groupCnt )
  {
    rc = cmErrMsg(err,kInvalidStateDspRC,"The resource arrays:%s and %s were not the same length.",midiListRsrcLabel,groupListRsrcLabel);
    goto errLabel;
  }

  for(i=0; i<4; ++i)
  {
    int             midi[  midiCnt ];
    int             chan[ midiCnt ];
    const cmChar_t* label = NULL;
    const cmChar_t* label1;

    for(j=0,k=0; j<midiCnt; ++j)
    {
      if(  (i<=2 && groupList[j] == i) || (i==3 && groupList[j]!=0) )
      {
        midi[k] = midiList[j];
        chan[k] = j;
        ++k;
      }
    }
    
    switch( i )
    {
      case 0:
        label = "NON";
        break;

      case 1:
        label = "CD0";
        break;

      case 2:
        label = "CD1";
        break;

      case 3:
        label = "CD";
        break;
    }

    if( cmJsonInsertPairIntArray( jsH, cmJsonRoot(jsH), label1 = cmTsPrintfS("%smidi",label), k, midi ) != kOkJsRC )
    {
      rc = cmErrMsg(err,kJsonFailDspRC,"The chord detector pitch resource '%s' could not be created.", cmStringNullGuard(label1));
      goto errLabel;
    }

    if( cmJsonInsertPairIntArray( jsH, cmJsonRoot(jsH), label1 = cmTsPrintfS("%schan",label), k, chan) != kOkJsRC )
    {
      rc = cmErrMsg(err,kJsonFailDspRC,"The chord detector channel index resource '%s' could not be created.", cmStringNullGuard(label1));
      goto errLabel;
    }

  }

 errLabel:
  return rc;
  
}

// Use the resource int arrays 'MidiList',"MachineList", "ChordList" and 'GroupList' to create
// the resource int arrays. XXXchan arrays contain indexes between 0 and 15.  When used with
// the chord detector this the 'local' channels are 0-15 and the remote channels are 16-31.

// NLCDmidi  - MIDI pitch of all local chord notes
// NLCD0midi - MIDI pitch of all local low single notes
// NLCD1midi - MIDI pitch of all local middle single notes
// NLEEmidi  - MIDI pitch of all local e-e single notes.

// NLCDchan  - channel index of all local chord notes
// NLCD0chan - channel index of all local high single notes
// NLCD1chan - channel index of all local middle single notes
// NLEEchan  - channel index of all local e-e single notes.

// NRCDmidi  - MIDI pitch of all remote chord notes
// NRCD0midi - MIDI pitch of all remote low single notes
// NRCD1midi - MIDI pitch of all remote middle single notes
// NREEmidi  - MIDI pitch of all remote e-e single notes.

// NRCDchan  - channel index of all remote chord notes
// NRCD0chan - channel index of all remote high single notes
// NRCD1chan - channel index of all remote middle single notes
// NREEchan  - channel index of all remote e-e single notes.

// 'local' refers to the machine running the chord detector
// 'remote' refers to the machine not running the chord detector 
cmDspRC_t _cmDspPgm_Main_NetProcRsrc( cmDspSysH_t h, cmErr_t* err )
{
  cmDspRC_t       rc                 = kOkDspRC;
  const cmChar_t* midiListRsrcLabel  = "MidiList";
  unsigned*       midiList           = NULL;
  unsigned        midiCnt            = 0;
  const cmChar_t* machListRsrcLabel  = "MachineList";
  unsigned*       machList           = NULL;
  unsigned        machCnt            = 0;
  const cmChar_t* groupListRsrcLabel = "GroupList";
  unsigned*       groupList          = NULL;
  unsigned        groupCnt           = 0;
  const cmChar_t* chordListRsrcLabel = "ChordList";
  unsigned*       chordList          = NULL;
  unsigned        chordCnt           = 0;
  cmJsonH_t       jsH                = cmDspSysPgmRsrcHandle(h);
  const cmChar_t* localNetNodeLabel  = NULL;
  unsigned        localNetNodeId     = cmInvalidId;
  bool            verboseFl          = false;
  unsigned        i,j,k,m;


  // get the network node id of this machine
  if(( cmDspRsrcString(h, &localNetNodeLabel, "cdLocalNetNode", NULL )) != kOkDspRC )
  {
    rc = cmErrMsg(err,kRsrcNotFoundDspRC,"The resource 'cdLocalNetNode' could not be read.");
    goto errLabel;
  }

  if(( localNetNodeId = cmDspSysNetNodeLabelToId(h,localNetNodeLabel)) == cmInvalidId )
  {
    cmErrMsg(err,kInvalidArgDspRC,"The network node label '%s' is not valid.",cmStringNullGuard(localNetNodeLabel));
    goto errLabel;
  }

  // get the pitch assigned to each channel
  if((rc = cmDspRsrcUIntArray(h,&midiCnt,&midiList,midiListRsrcLabel,NULL)) != kOkDspRC )
  {
    rc = cmErrMsg(err,kRsrcNotFoundDspRC,"The resource '%s' could not be read.",midiListRsrcLabel);
    goto errLabel;
  }

  // get the network node id of each channel
  if((rc = cmDspRsrcUIntArray(h,&machCnt,&machList,machListRsrcLabel,NULL)) != kOkDspRC )
  {
    rc = cmErrMsg(err,kRsrcNotFoundDspRC,"The resource '%s' could not be read.",machListRsrcLabel);
    goto errLabel;
  }

  // get the group id of each channel
  if((rc = cmDspRsrcUIntArray(h,&groupCnt,&groupList,groupListRsrcLabel,NULL)) != kOkDspRC )
  {
    rc = cmErrMsg(err,kRsrcNotFoundDspRC,"The resource '%s' could not be read.",groupListRsrcLabel);
    goto errLabel;
  }

  // get the chord detector flag for each channel
  if((rc = cmDspRsrcUIntArray(h,&chordCnt,&chordList,chordListRsrcLabel,NULL)) != kOkDspRC )
  {
    rc = cmErrMsg(err,kRsrcNotFoundDspRC,"The resource '%s' could not be read.",chordListRsrcLabel);
    goto errLabel;
  }

  if( midiCnt !=  groupCnt || midiCnt != machCnt)
  {
    rc = cmErrMsg(err,kInvalidStateDspRC,"The resource arrays:%s, %s and %s were not all the same length.",midiListRsrcLabel,machListRsrcLabel,groupListRsrcLabel);
    goto errLabel;
  }

  if( 1 )
  {
    const cmChar_t* label1;
          
    unsigned ten = 10;
    int nsChSelChV[ midiCnt ];     // note selector chord port [0, 5]
    int nsChSelChIdxV[ midiCnt ];  // index on note selector port
    int nsNcSelChV[ midiCnt ];     // note selector single-note port [ 2, 3, 4,   7,8,9 ]
    int nsNcSelChIdxV[ midiCnt ];  // index on note selector single-note port
    unsigned chIdxCntV[ ten ];     // next index for each note selector port

    cmVOU_Fill(chIdxCntV,ten,0);

    for(i=0; i<midiCnt; ++i)
    {
      unsigned localFl      = i<16;
      unsigned selIdx       = localFl ? 0 : 5;    // chord port
      unsigned chordChFl    = chordList[i] != 0;  // is this a chord channel

      nsChSelChV[i]    = cmInvalidIdx;
      nsChSelChIdxV[i] = cmInvalidIdx;
      if( chordChFl )
      {
        nsChSelChV[ i ]      =  selIdx;   
        nsChSelChIdxV[ i ]   =  chIdxCntV[ selIdx ];
        chIdxCntV[ selIdx ] +=  1;
      }

      switch( groupList[i] )
      {
        case 0:   selIdx = localFl ? 4 : 9; break;  // e-e group
        case 1:   selIdx = localFl ? 2 : 7; break;  // hi/lo group
        case 2:   selIdx = localFl ? 3 : 8; break;  // mid group
        default:
          { assert(0); }
      }

      assert( selIdx < ten );

      nsNcSelChV[ i ]    = selIdx;
      nsNcSelChIdxV[ i ] = chIdxCntV[ selIdx ];
      chIdxCntV[ selIdx ] += 1;

    }

    if( verboseFl )
    {
      cmVOI_PrintL("nsChSelChV",    err->rpt, 1, midiCnt, nsChSelChV );
      cmVOI_PrintL("nsChSelChIdxV", err->rpt, 1, midiCnt, nsChSelChIdxV );
      cmVOI_PrintL("nsNcSelChV",    err->rpt, 1, midiCnt, nsNcSelChV );
      cmVOI_PrintL("nsNcSelChIdxV", err->rpt, 1, midiCnt, nsNcSelChIdxV );
    }

    if( cmJsonInsertPairIntArray( jsH, cmJsonRoot(jsH), label1 = "nsChSelChV", midiCnt, nsChSelChV ) != kOkJsRC )
    {
      rc = cmErrMsg(err,kJsonFailDspRC,"The note selector resource '%s' could not be created.", cmStringNullGuard(label1));
      goto errLabel;
    }

    if( cmJsonInsertPairIntArray( jsH, cmJsonRoot(jsH), label1 = "nsChSelChIdxV", midiCnt, nsChSelChIdxV ) != kOkJsRC )
    {
      rc = cmErrMsg(err,kJsonFailDspRC,"The note selector resource '%s' could not be created.", cmStringNullGuard(label1));
      goto errLabel;
    }

    if( cmJsonInsertPairIntArray( jsH, cmJsonRoot(jsH), label1 = "nsNcSelChV", midiCnt, nsNcSelChV ) != kOkJsRC )
    {
      rc = cmErrMsg(err,kJsonFailDspRC,"The note selector resource '%s' could not be created.", cmStringNullGuard(label1));
      goto errLabel;
    }

    if( cmJsonInsertPairIntArray( jsH, cmJsonRoot(jsH), label1 = "nsNcSelChIdxV", midiCnt, nsNcSelChIdxV ) != kOkJsRC )
    {
      rc = cmErrMsg(err,kJsonFailDspRC,"The note selector resource '%s' could not be created.", cmStringNullGuard(label1));
      goto errLabel;
    }

    for(i=0; i<ten; ++i)
    {
      int midi[ midiCnt ];
      int chan[ midiCnt ];

      for(j=0,k=0; j<midiCnt; ++j)
      {
        m = i;
        if( m == 1 || m == 6 )
          m -= 1;

        if( nsChSelChV[j] == m )
        {
          midi[k] = midiList[j];
          chan[k] = j>15 ? j-16 : j;
          assert( k == nsChSelChIdxV[j] );
          ++k;
        }
        
        if( nsNcSelChV[j] == m )
        {
          midi[k] = midiList[j];
          chan[k] = j>15 ? j-16 : j;
          assert( k == nsNcSelChIdxV[j] );
          ++k;
        }        
      }

      if( cmJsonInsertPairIntArray( jsH, cmJsonRoot(jsH), label1 = cmTsPrintfS("nsMidi-%i",i), k, midi ) != kOkJsRC )
      {
        rc = cmErrMsg(err,kJsonFailDspRC,"The note selector resource '%s' could not be created.", cmStringNullGuard(label1));
        goto errLabel;
      }

      if( verboseFl )
        cmVOI_PrintL(label1, err->rpt, 1, k, midi );

      if( cmJsonInsertPairIntArray( jsH, cmJsonRoot(jsH), label1 = cmTsPrintfS("nsChan-%i",i), k, chan ) != kOkJsRC )
      {
        rc = cmErrMsg(err,kJsonFailDspRC,"The note selector resource '%s' could not be created.", cmStringNullGuard(label1));
        goto errLabel;
      }

      if( verboseFl )
        cmVOI_PrintL(label1, err->rpt, 1, k, chan );

    }

  }
 errLabel:
  return rc;
  
}

const cmChar_t* _cmDspPP_SelCheckTitle( cmDspSysH_t h, unsigned i )
{ return _cmDspPP_CircuitDesc(i)->title; }

cmDspRC_t _cmDspSysPgm_Main( cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t               rc         = kOkDspRC;
  const cmChar_t*         chCfgFn    = NULL;
  cmCtx_t*                cmCtx      = cmDspSysPgmCtx(h);
  double                  efOnThrDb  = -40;
  double                  efOffThrDb = -70;
  double                  efMaxDb    = -15;
  unsigned                circuitCnt = _cmDspPP_CircuitDescCount();
  unsigned                resetSymId = cmDspSysRegisterStaticSymbol(h,"_reset");
  bool                    inFileFl   = false;
  cmErr_t                 err;  
  _cmDspPP_Ctx_t          ctx;
  cmDspPP_CircuitSwitch_t sw; 
  memset(&sw,0,sizeof(sw));


  // set up the global context record
  memset(&ctx,0,sizeof(ctx));
  ctx.oChCnt = 2;
  ctx.err    = &err;

  cmErrSetup(&err,&cmCtx->rpt,"PP Main");

  // create the individual non-networked chord detector resource arrays from the base resource arrays
  if((rc =_cmDspPgm_Main_ProcRsrc(h,&err)) != kOkDspRC )
    goto errLabel;

  // create the individual networked chord detector resource arrays from the base resource arrays
  if((rc = _cmDspPgm_Main_NetProcRsrc(h,&err)) != kOkDspRC )
    goto errLabel;

  // get the channel cfg configuration file name
  if( cmJsonPathToString( cmDspSysPgmRsrcHandle(h), NULL, NULL, "chCfgFn", &chCfgFn ) != kOkJsRC )
  {
    rc = cmErrMsg(&err,kRsrcNotFoundDspRC,"The 'chCfgFn' resource was not found.");
    goto errLabel;
  }

  // get the count of channels from the ch. cfg. array
  if(( ctx.iChCnt = cmChCfgChannelCount(cmCtx,chCfgFn,&ctx.nsCnt)) == 0 )
  {
    rc = cmErrMsg(&err,kPgmCfgFailDspRC,"Unable to obtain the channel count from '%s'.",cmStringNullGuard(chCfgFn));
    goto errLabel;
  }

  if( rc == kOkDspRC )
  {
    unsigned inChCnt,outChCnt;
    // channel cfg 
    ctx.chCfg  = cmDspSysAllocInst(     h, "ChCfg",    NULL,  1, chCfgFn );  

    // global printer
    ctx.print  = cmDspSysAllocInst(     h, "Printer",  NULL,  1, ">" );

    

    cmDspInst_t** ain;
    cmDspInst_t*  phs;

    // audio input
    if( inFileFl )
    {
      unsigned        wtSmpCnt = (unsigned)cmDspSysSampleRate(h);
      unsigned        wtMode   = 1; //file mode
      const cmChar_t* fn       = "/home/kevin/media/audio/gate_detect/gate_detect2.aif";

      phs  =  cmDspSysAllocInst(h,"Phasor",   NULL,  0 );
      ain  =  cmDspSysAllocInstArray(h, ctx.iChCnt, "WaveTable", NULL,  NULL, 3, wtSmpCnt, wtMode, fn );
      inChCnt = ctx.iChCnt;
    }
    else
    {
      ain = cmDspSysAllocAudioInAR( h, "audioInMap", 1.0, &inChCnt );
    }
    

    // envelope followers and RMS meters
    cmDspSysAllocLabel(h,"EF Gates",kLeftAlignDuiId );
    cmDspInst_t** ef  = cmDspSysAllocInstArray( h, ctx.iChCnt,"EnvFollow", NULL, NULL, 0 );
    cmDspInst_t** mtr = cmDspSysAllocInstArray( h, ctx.iChCnt,"Meter",    "mtr", NULL, 3, 0.0, 0.0, 1.0 );

    
    // Level meters
    cmDspSysNewColumn(h,200);
    cmDspSysAllocLabel(h,"Level",kLeftAlignDuiId );
    cmDspInst_t** lvl = cmDspSysAllocInstArray( h, ctx.iChCnt,"Meter",    "lvl", NULL, 3, 0.0, 0.0, 1.0 );
    assert( inChCnt == ctx.iChCnt );

    // Onset count displays
    cmDspSysNewColumn(h,200);
    cmDspInst_t** onn = cmDspSysAllocInstArray( h, ctx.iChCnt,"Scalar", "on", NULL, 5, kNumberDuiId, 0.0, 10000.0, 1.0, 0.0 );

    cmDspSysNewColumn(h,200);

    // program reset button
    cmDspInst_t* resetBtn = cmDspSysAllocButton( h, "reset", 0 );  
    cmDspSysAssignInstAttrSymbol(h,resetBtn, resetSymId );

    // circuit selection check-boxes
    cmDspInst_t** csel = cmDspSysAllocInstArray( h, circuitCnt, "Button", NULL, _cmDspPP_SelCheckTitle, 2, kCheckDuiId, 0.0 );

    cmDspSysNewColumn(h,200);
    cmDspSysAllocLabel(h,"Out Gains",kLeftAlignDuiId );

    // output gain controls
    cmDspInst_t** ogain  = cmDspSysAllocInstArray( h, ctx.oChCnt,"Scalar", "mgain", NULL, 5, kNumberDuiId,  0.0, 10.0, 0.01, 1.0);
    
    // envelope follower parameters
    cmDspInst_t*  onThr  = cmDspSysAllocScalar( h, "On  Thresh",-100.0, 0.0, 0.1, efOnThrDb); 
    cmDspInst_t*  offThr = cmDspSysAllocScalar( h, "Off Thresh",-100.0, 0.0, 0.1, efOffThrDb);
    cmDspInst_t*  maxDb  = cmDspSysAllocScalar( h, "Max Db",    -100.0, 0.0, 0.1, efMaxDb);
  
    // switcher and circuits
    _cmDspPP_CircuitSwitchAlloc(h, &ctx, &sw, resetBtn, csel, ain, ef);


    // audio output
    cmDspInst_t** aout = cmDspSysAllocAudioOutAR( h, "audioOutMap", 1.0, &outChCnt );
    assert( outChCnt == ctx.oChCnt );

    //cmDspInst_t*  prt  = cmDspSysAllocInst( h, "Printer", NULL, 2, "", 250 );

    // check for errors
    if((rc = cmDspSysLastRC(h)) != kOkDspRC )
      goto doneLabel;

    if( inFileFl )
    {
      cmDspSysConnectAudio11N1(   h, phs, "out", ain, "phs", ctx.iChCnt );  // phs -> wt
      cmDspSysConnectAudioN1N1(   h, ain, "out",  ef, "in", ctx.iChCnt );   // wt  -> EF
    }
    else
    {
      cmDspSysConnectAudioN1N1( h, ain,     "out",    ef,      "in",       ctx.iChCnt );     // ain -> EF
    }

    cmDspSysInstallCb1NN1(    h, ctx.chCfg, "gain",   ain,     "gain",     ctx.iChCnt );   // chCfg -> ain (gain)

    cmDspSysConnectAudioN1N1( h, sw.omix, "out",      aout,    "in",       ctx.oChCnt );   // Sw.mix -> aout
    cmDspSysInstallCbN1N1(    h, ogain,   "val",      aout,    "gain",     ctx.oChCnt );   // gain -> aout

    cmDspSysInstallCb(        h, resetBtn,"sym",      ctx.chCfg, "sel",    NULL );         // reset -> chCfg    

    cmDspSysInstallCbN1N1(    h, ef,      "gate",     mtr,     "in",       ctx.iChCnt );   // EF -> meter (gate)
    cmDspSysInstallCbN1N1(    h, ef,      "level",    lvl,     "in",       ctx.iChCnt );   // EF -> meter (level)
    cmDspSysInstallCbN1N1(    h, ef,      "ons",      onn,     "val",      ctx.iChCnt );

    cmDspSysInstallCb11N1(    h, onThr,   "val",      ef,      "ondb",     ctx.iChCnt );    // EF sensivity settings
    cmDspSysInstallCb11N1(    h, offThr,  "val",      ef,      "offdb",    ctx.iChCnt );    //
    cmDspSysInstallCb11N1(    h, maxDb,   "val",      ef,      "maxdb",    ctx.iChCnt );    //

    //cmDspSysInstallCbN111(    h, ef,      "level",    prt,     "in",       ctx.iChCnt );

  doneLabel:
    _cmDspPP_CircuitSwitchFree(h, &sw );
    
  }
 errLabel:

  if( rc != kOkDspRC )
    cmErrMsg(&err,rc,"'main' construction failed.");

  return rc;
}
