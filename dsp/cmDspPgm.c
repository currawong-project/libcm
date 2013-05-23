#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmFileSys.h"
#include "cmSymTbl.h"
#include "cmJson.h"
#include "cmPrefs.h"
#include "cmDspValue.h"
#include "cmMsgProtocol.h"
#include "cmThread.h"
#include "cmUdpPort.h"
#include "cmUdpNet.h"
#include "cmAudioSys.h"
#include "cmProcObj.h"
#include "cmDspCtx.h"
#include "cmDspClass.h"
#include "cmDspSys.h"
#include "cmDspPgm.h"
#include "cmDspPgmPP.h"
#include "cmDspPgmKr.h"

cmDspRC_t _cmDspSysPgm_Test_Midi( cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t rc = kOkDspRC;

  cmDspInst_t* sendBtn = cmDspSysAllocInst( h,"Button", "Send",    2, kButtonDuiId, 0.0 );
  cmDspInst_t* status  = cmDspSysAllocInst( h,"Scalar", "Status",  5, kNumberDuiId, 0.0,  127.0, 1.0,  144.0);
  cmDspInst_t* d0      = cmDspSysAllocInst( h,"Scalar", "D0",      5, kNumberDuiId, 0.0,  127.0, 1.0,  64.0);
  cmDspInst_t* d1      = cmDspSysAllocInst( h,"Scalar", "D1",      5, kNumberDuiId, 0.0,  127.0, 1.0,  64.0);
  cmDspInst_t* midiOut = cmDspSysAllocInst( h,"MidiOut", NULL,     2, "Fastlane", "Fastlane MIDI A");
  cmDspInst_t* midiIn  = cmDspSysAllocInst( h,"MidiIn",  NULL,     0 );
  cmDspInst_t* printer = cmDspSysAllocInst( h,"Printer", NULL,     1, ">" );

  // check for allocation errors
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    goto errLabel;

  cmDspSysInstallCb(   h, sendBtn, "out", d1,      "send", NULL);
  cmDspSysInstallCb(   h, sendBtn, "out", d0,      "send", NULL);
  cmDspSysInstallCb(   h, sendBtn, "out", status,  "send", NULL);

  cmDspSysInstallCb(   h, status,  "val", midiOut, "status",NULL);
  cmDspSysInstallCb(   h, d0,      "val", midiOut, "d0",    NULL);
  cmDspSysInstallCb(   h, d1,      "val", midiOut, "d1",    NULL);

  cmDspSysInstallCb(   h, midiIn,  "status", printer, "in", NULL);
  cmDspSysInstallCb(   h, midiIn,  "d0",     printer, "in", NULL);
  cmDspSysInstallCb(   h, midiIn,  "d1",     printer, "in", NULL);
  cmDspSysInstallCb(   h, midiIn,  "smpidx", printer, "in", NULL);

 errLabel:
  return rc;
  
}

cmDspRC_t _cmDspSysPgm_Stereo_Through( cmDspSysH_t h, void** userPtrPtr )
{
  bool useBuiltInFl = true;

  cmDspInst_t* ignp = cmDspSysAllocInst( h,"Scalar", "In Gain",  5, kNumberDuiId, 0.0,  4.0, 0.01,  1.0);
  //cmDspInst_t* ognp = cmDspSysAllocInst( h,"Scalar", "Out Gain", 5, kNumberDuiId, 0.0,  4.0, 0.01,  1.0);
  cmDspInst_t* hzp  =  cmDspSysAllocInst(h,"Scalar", "Hz",       5, kNumberDuiId, 0.0, 10.0, 0.001, 1.0);

  cmDspInst_t* php =  cmDspSysAllocInst(h,"Phasor",   NULL,  0 );
  cmDspInst_t* wtp =  cmDspSysAllocInst(h,"WaveTable",NULL,  2, cmDspSysSampleRate(h), 2 );

  cmDspInst_t* ai0p = cmDspSysAllocInst(h,"AudioIn", NULL,   1, useBuiltInFl ? 0 : 2 );
  cmDspInst_t* ai1p = cmDspSysAllocInst(h,"AudioIn", NULL,   1, useBuiltInFl ? 1 : 3 );
   
  // MOTU Traveler: Use channels 2&3 (out plugs:3&4) because 0&1 do not show up on plugs 1&2.
  cmDspInst_t* ao0p = cmDspSysAllocInst(h,"AudioOut",NULL,   1, useBuiltInFl ? 0 : 2 );
  cmDspInst_t* ao1p = cmDspSysAllocInst(h,"AudioOut",NULL,   1, useBuiltInFl ? 1 : 3 );

  cmDspInst_t* im0p = cmDspSysAllocInst(h,"AMeter","In 0",  0);
  cmDspInst_t* im1p = cmDspSysAllocInst(h,"AMeter","In 1", 0);

  cmDspInst_t* om0p = cmDspSysAllocInst(h,"AMeter","Out 0", 0);
  cmDspInst_t* om1p = cmDspSysAllocInst(h,"AMeter","Out 1",0);

  //cmDspSysConnectAudio(h, ai0p, "out", ao0p, "in"); 
  //cmDspSysConnectAudio(h, ai1p, "out", ao1p, "in"); 

  cmDspSysConnectAudio(h, ai0p, "out", im0p, "in");         //ain0 -> imtr0
  cmDspSysConnectAudio(h, ai1p, "out", im1p, "in");         //ain1 -> imtr1
  
  cmDspSysInstallCb(   h, hzp, "out", php, "mult", NULL);   // hz  -> phs
  cmDspSysConnectAudio(h, php, "out", wtp, "phs" );         // phs -> wt
  cmDspSysConnectAudio(h, wtp, "out", ao0p, "in"  );        // wt  -> aout0
  cmDspSysConnectAudio(h, wtp, "out", om0p, "in" );         // wt  -> omtr0

  cmDspSysInstallCb(   h, ignp,"val", ai1p, "gain", NULL);  // igain -> ain1
  cmDspSysConnectAudio(h, ai1p,"out", ao1p, "in" );         // ain1  -> aout1 
  cmDspSysConnectAudio(h, ai1p,"out", om1p, "in" );         // ain1  -> omtr1 
  return kOkDspRC;
}

cmDspRC_t _cmDspSysPgm_Stereo_Fx( cmDspSysH_t h, void** userPtrPtr )
{
  bool useBuiltInFl = true;

  cmDspInst_t* ignp = cmDspSysAllocInst( h,"Scalar", "In Gain",  5, kNumberDuiId, 0.0,  4.0, 0.01,  1.0);
  cmDspInst_t* ognp = cmDspSysAllocInst( h,"Scalar", "Out Gain", 5, kNumberDuiId, 0.0,  4.0, 0.01,  1.0);

  //cmDspInst_t* fb0p = cmDspSysAllocInst( h,"Scalar", "Feeback 0",  5, kNumberDuiId, 0.0,  1.0, 0.001,  0.0);
  //cmDspInst_t* fb1p = cmDspSysAllocInst( h,"Scalar", "Feedback 1", 5, kNumberDuiId, 0.0,  1.0, 0.001,  0.0);

  //cmDspInst_t* tm0p = cmDspSysAllocInst( h,"Scalar", "Time 0",  5, kNumberDuiId, 0.0,  1000.0, 0.0001,  10.0);
  //cmDspInst_t* tm1p = cmDspSysAllocInst( h,"Scalar", "Time 1",  5, kNumberDuiId, 0.0,  1000.0, 0.0001,  10.0);

  cmDspInst_t* rt0p = cmDspSysAllocInst( h,"Scalar", "Ratio 0",  5, kNumberDuiId, 0.01,  10.0, 0.01,  1.0);
  cmDspInst_t* rt1p = cmDspSysAllocInst( h,"Scalar", "Ratio 1",  5, kNumberDuiId, 0.01,  10.0, 0.01,  1.0);

  cmDspInst_t* ai0p = cmDspSysAllocInst(h,"AudioIn", NULL,   1, useBuiltInFl ? 0 : 2 );
  cmDspInst_t* ai1p = cmDspSysAllocInst(h,"AudioIn", NULL,   1, useBuiltInFl ? 1 : 3 );
  
  cmDspInst_t* om0p = cmDspSysAllocInst(h,"AMeter","Out Left", 0);
  cmDspInst_t* om1p = cmDspSysAllocInst(h,"AMeter","Out Right",0);

  //cmDspInst_t* dy0p = cmDspSysAllocInst(h,"Delay", NULL, 2, 1000.0, 0.5 );
  //cmDspInst_t* dy1p = cmDspSysAllocInst(h,"Delay", NULL, 2, 2000.0, 0.7 );

  cmDspInst_t* ps0p = cmDspSysAllocInst(h,"PShift", NULL, 0 );
  cmDspInst_t* ps1p = cmDspSysAllocInst(h,"PShift", NULL, 0 );
 
  // MOTU Traveler: Use channels 2&3 (out plugs:3&4) because 0&1 do not show up on plugs 1&2.
  cmDspInst_t* ao0p = cmDspSysAllocInst(h,"AudioOut",NULL,   1, useBuiltInFl ? 0 : 2 );
  cmDspInst_t* ao1p = cmDspSysAllocInst(h,"AudioOut",NULL,   1, useBuiltInFl ? 1 : 3 );



  cmDspSysConnectAudio(h, ai0p,"out", om0p, "in" );  // input meter connection
  cmDspSysConnectAudio(h, ai1p,"out", om1p, "in" );

  //cmDspSysConnectAudio(h, ai0p,"out", dy0p, "in" );  // input -> delay
  //cmDspSysConnectAudio(h, ai1p,"out", dy1p, "in" );


  cmDspSysConnectAudio(h, ai0p,"out", ps0p, "in" );  // delay -> pshift
  cmDspSysConnectAudio(h, ai1p,"out", ps1p, "in" );

  cmDspSysConnectAudio(h, ps0p,"out", ao0p, "in" ); // pshift -> output
  cmDspSysConnectAudio(h, ps1p,"out", ao1p, "in" );

  //cmDspSysConnectAudio(h, dy0p,"out", ao0p, "in" ); // delay -> output
  //cmDspSysConnectAudio(h, dy1p,"out", ao1p, "in" );

  cmDspSysInstallCb(   h, ignp,"val", ai0p, "gain", NULL);  // input gain
  cmDspSysInstallCb(   h, ignp,"val", ai1p, "gain", NULL);

  cmDspSysInstallCb(   h, ognp, "val", ao0p, "gain", NULL);  // output gain
  cmDspSysInstallCb(   h, ognp, "val", ao1p, "gain", NULL);  

  //cmDspSysInstallCb(   h, fb0p, "val", dy0p, "fb", NULL);  // feedback
  //cmDspSysInstallCb(   h, fb1p, "val", dy1p, "fb", NULL);  

  //cmDspSysInstallCb(   h, tm0p, "val", dy0p, "time", NULL);  // delay time
  //cmDspSysInstallCb(   h, tm1p, "val", dy1p, "time", NULL);  

  cmDspSysInstallCb(   h, rt0p, "val", ps0p, "ratio", NULL);  // pitch ratio
  cmDspSysInstallCb(   h, rt1p, "val", ps1p, "ratio", NULL);  

  return kOkDspRC;
}

cmDspRC_t _cmDspSysPgm_PlaySine( cmDspSysH_t h, void** userPtrPtr )
{
  bool useBuiltInFl = true;
  double frqHz = 440.0;

  cmDspInst_t* chp = cmDspSysAllocInst( h,"Scalar", "Channel",  5, kNumberDuiId, 0.0,  100.0, 1.0,  0.0);
  cmDspInst_t* php =  cmDspSysAllocInst(h,"Phasor",    NULL,   2, cmDspSysSampleRate(h), frqHz );
  cmDspInst_t* wtp =  cmDspSysAllocInst(h,"WaveTable", NULL,   2, ((int)cmDspSysSampleRate(h)), 4);
  cmDspInst_t* ao0p = cmDspSysAllocInst(h,"AudioOut",  NULL,   1, useBuiltInFl ? 0 : 2 );
  cmDspInst_t* ao1p = cmDspSysAllocInst(h,"AudioOut",  NULL,   1, useBuiltInFl ? 1 : 3 );
  cmDspInst_t* om0p = cmDspSysAllocInst(h,"AMeter","Out", 0);
  
  

  cmDspSysConnectAudio(h, php, "out", wtp,  "phs" );  // phasor -> wave table
  cmDspSysConnectAudio(h, wtp, "out", ao0p, "in" );   // wave table -> audio out
  cmDspSysConnectAudio(h, wtp, "out", ao1p, "in" );   // 
  cmDspSysConnectAudio(h, wtp, "out", om0p, "in" );

  cmDspSysInstallCb( h, chp, "val", ao0p, "ch", NULL);
  return kOkDspRC;
}

cmDspRC_t _cmDspSysPgm_PlayFile( cmDspSysH_t h, void** userPtrPtr )
{
  bool            useBuiltInFl = true;
  const char*     fn0          = "media/audio/20110723-Kriesberg/Audio Files/Piano 3_01.wav";
  //int             beg          = 6900826;
  //int             end          = 13512262;
  const cmChar_t* fn           = cmFsMakeFn(cmFsUserDir(),fn0,NULL,NULL );

  cmDspInst_t* ofp =  cmDspSysAllocInst(h,"Scalar", "Offset",  5, kNumberDuiId, 0.0,  cmDspSysSampleRate(h)*600.0, 1.0,  0.0);
  cmDspInst_t* fnp =  cmDspSysAllocInst(h,"Fname",    NULL,  3, false,"Audio Files (*.wav,*.aiff,*.aif)\tAudio Files (*.{wav,aiff,aif})",fn);
  cmDspInst_t* php =  cmDspSysAllocInst(h,"Phasor",   NULL,  0 );
  cmDspInst_t* wtp =  cmDspSysAllocInst(h,"WaveTable",NULL,  2, ((int)cmDspSysSampleRate(h)), 1 );
  //cmDspInst_t* afp =  cmDspSysAllocInst(h,"AudioFileOut",NULL,2,"/home/kevin/temp/record0.aif",1);
  
  cmDspInst_t* ao0p = cmDspSysAllocInst(h,"AudioOut",NULL,   1, useBuiltInFl ? 0 : 2 );
  cmDspInst_t* ao1p = cmDspSysAllocInst(h,"AudioOut",NULL,   1, useBuiltInFl ? 1 : 3 );

  
  cmDspSysConnectAudio(h, php, "out", wtp,  "phs" );  // phasor -> wave table
  cmDspSysConnectAudio(h, wtp, "out", ao0p, "in" );   // wave table -> audio out
  cmDspSysConnectAudio(h, wtp, "out", ao1p, "in" );   // 

  //cmDspSysConnectAudio(h, wtp, "out", afp,  "in0"); 

  cmDspSysInstallCb(h, ofp, "val", wtp, "beg", NULL ); 
  cmDspSysInstallCb(h, fnp, "out", wtp, "fn", NULL);    
  return kOkDspRC;
}


cmDspRC_t _cmDspSysPgm_GateDetect( cmDspSysH_t h, void** userPtrPtr )
{
  bool            useBuiltInFl = true;
  //const char*     fn0          = "media/audio/20110723-Kriesberg/Audio Files/Piano 3_01.wav";
  //int             beg          = 6900826;
  //int             end          = 13512262;
  //const char*     fn0          = "media/audio/McGill-3/1 Audio Track.aiff";
  const char*     fn0          = "temp/gate_detect0.aif";
  int             beg          = 0;
  int             end          = -1;
  const cmChar_t* fn           = cmFsMakeFn(cmFsUserDir(),fn0,NULL,NULL );
  const cmChar_t* tfn          = "/home/kevin/temp/test0.txt";

  cmDspInst_t* ofp =  cmDspSysAllocInst(h,"Scalar", "Offset",  5, kNumberDuiId, 0.0,  cmDspSysSampleRate(h)*600.0, 1.0,  0.0);
  //cmDspInst_t* fnp =  cmDspSysAllocInst(h,"Fname",     NULL,  3, false,"Audio Files (*.wav,*.aiff,*.aif)\tAudio Files (*.{wav,aiff,aif})",fn);
  cmDspInst_t* php =  cmDspSysAllocInst(h,"Phasor",    NULL,  0 );
  cmDspInst_t* wtp =  cmDspSysAllocInst(h,"WaveTable", NULL,  6, ((int)cmDspSysSampleRate(h)), 1, fn, -1, beg, end );
  cmDspInst_t* gdp =  cmDspSysAllocInst(h,"GateDetect",NULL,  1, 20.0);
  cmDspInst_t* gmp =  cmDspSysAllocInst(h,"Meter",     NULL,  3,    0.0,    0.0, 1.0);
  cmDspInst_t* rmp =  cmDspSysAllocInst(h,"Meter",     NULL,  3, -100.0, -100.0, 0.0);
  cmDspInst_t* txp =  cmDspSysAllocInst(h,"TextFile",  NULL,  2, 3, tfn);

  cmDspInst_t* ao0p = cmDspSysAllocInst(h,"AudioOut",NULL,   1, useBuiltInFl ? 0 : 2 );
  cmDspInst_t* ao1p = cmDspSysAllocInst(h,"AudioOut",NULL,   1, useBuiltInFl ? 1 : 3 );

  
  cmDspSysConnectAudio(h, php, "out", wtp,  "phs" );  // phasor -> wave table
  cmDspSysConnectAudio(h, wtp, "out", ao0p, "in" );   // wave table -> audio out
  cmDspSysConnectAudio(h, wtp, "out", ao1p, "in" );   // 
  cmDspSysConnectAudio(h, wtp, "out", gdp,  "in" );

  cmDspSysInstallCb(h, ofp, "val", wtp, "beg", NULL ); 
  cmDspSysInstallCb(h, gdp, "gate", gmp, "in", NULL );
  cmDspSysInstallCb(h, gdp, "rms",  rmp, "in", NULL );
  cmDspSysInstallCb(h, gdp, "gate", txp, "in-0", NULL);
  cmDspSysInstallCb(h, gdp, "rms",  txp, "in-1", NULL);
  cmDspSysInstallCb(h, gdp, "mean", txp, "in-2", NULL);
  return kOkDspRC;
}

cmDspRC_t _cmDspSysPgm_Record(cmDspSysH_t h, void** userPtrPtr)
{
  int          chCnt = 8;
  cmDspInst_t* a[chCnt];
  int i;
  for(i=0; i<chCnt; ++i)
    a[i]   = cmDspSysAllocInst( h, "AudioIn", NULL,  1, i );
  
  cmDspInst_t* mxp   = cmDspSysAllocInst( h, "AMix",         NULL,       chCnt+1, chCnt, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 );   
  cmDspInst_t* afp   = cmDspSysAllocInst( h, "AudioFileOut", NULL,       2,"/home/kevin/temp/gate_detect.aif",1);
  cmDspInst_t* aop   = cmDspSysAllocInst( h, "AudioOut",     NULL,       1, 0 );
  cmDspInst_t* txp   = cmDspSysAllocInst( h, "TextFile",     NULL,       2, 1, "/home/kevin/temp/gate_detect.txt");
  cmDspInst_t* chp   = cmDspSysAllocInst( h, "Scalar",       "Channel",  5, kNumberDuiId, 0.0,  7.0, 1.0,  0.0);
  
  for(i=0; i<chCnt; ++i)
  {
    cmChar_t lbl[15];
    snprintf(lbl,15,"in-%i",i);
    cmDspSysConnectAudio(h,a[i], "out", mxp, lbl);
  }

  cmDspSysConnectAudio(h, mxp, "out", afp,  "in0" );
  cmDspSysConnectAudio(h, mxp, "out", aop,  "in" );

  cmDspSysInstallCb(h, chp, "out", txp, "in-0", NULL);
  
  return kOkDspRC;
}

cmDspRC_t _cmDspSysPgm_PitchShiftFile( cmDspSysH_t h, void** userPtrPtr )
{
  bool            useBuiltInFl = true;
  const cmChar_t* fn0          = "media/audio/20110723-Kriesberg/Audio Files/Piano 3_01.wav";
  const cmChar_t* fn1          = "temp/record0.aif";
  int             beg          = 6900826;
  int             end          = 13512262;
  const cmChar_t* fn           = cmFsMakeFn(cmFsUserDir(),fn0,NULL,NULL );
  const cmChar_t* ofn          = cmFsMakeFn(cmFsUserDir(),fn1,NULL,NULL );

  cmDspInst_t* rt0p = cmDspSysAllocInst(h,"Scalar", "Ratio 0",  5, kNumberDuiId, 0.01,  10.0, 0.01,  1.0);
  cmDspInst_t* rt1p = cmDspSysAllocInst(h,"Scalar", "Ratio 1",  5, kNumberDuiId, 0.01,  10.0, 0.01,  1.0);
  cmDspInst_t* ofp =  cmDspSysAllocInst(h,"Scalar", "Offset",  5, kNumberDuiId, 0.0,  cmDspSysSampleRate(h)*600.0, 1.0,  0.0);
  //cmDspInst_t* fnp =  cmDspSysAllocInst(h,"Fname",    NULL,  3, false,"Audio Files (*.wav,*.aiff,*.aif)\tAudio Files (*.{wav,aiff,aif})",fn);
  cmDspInst_t* php =  cmDspSysAllocInst(h,"Phasor",   NULL,  0 );
  cmDspInst_t* wtp =  cmDspSysAllocInst(h,"WaveTable",NULL,  6, ((int)cmDspSysSampleRate(h)), 1, fn, -1, beg, end );
  cmDspInst_t* afp =  cmDspSysAllocInst(h,"AudioFileOut",NULL,2,ofn,1);

  cmDspInst_t* ps0p = cmDspSysAllocInst(h,"PShift", NULL, 0 );
  cmDspInst_t* ps1p = cmDspSysAllocInst(h,"PShift", NULL, 0 );
  
  cmDspInst_t* ao0p = cmDspSysAllocInst(h,"AudioOut",NULL,   1, useBuiltInFl ? 0 : 2 );
  cmDspInst_t* ao1p = cmDspSysAllocInst(h,"AudioOut",NULL,   1, useBuiltInFl ? 1 : 3 );

  
  cmDspSysConnectAudio(h, php,  "out", wtp,  "phs" );  // phasor -> wave table
  cmDspSysConnectAudio(h, wtp,  "out", ps0p, "in" );   // wave table -> shift
  cmDspSysConnectAudio(h, wtp,  "out", ps1p, "in" );   // 

  cmDspSysConnectAudio(h, ps0p, "out", ao0p, "in" );   // shift -> audio out
  cmDspSysConnectAudio(h, ps1p, "out", ao1p, "in" );   // 
  cmDspSysConnectAudio(h, ps0p, "out", afp,  "in0");   // 

  cmDspSysInstallCb(h, ofp, "val", wtp, "beg", NULL ); 
  //cmDspSysInstallCb(h, fnp, "out", wtp, "fn", NULL);    

  cmDspSysInstallCb(h, rt0p, "val", ps0p, "ratio", NULL);    
  cmDspSysInstallCb(h, rt1p, "val", ps1p, "ratio", NULL);    
  return kOkDspRC;
}

cmDspRC_t _cmDspSysPgm_LoopRecd( cmDspSysH_t h, void** userPtrPtr )
{
  bool            useBuiltInFl = true;
  const cmChar_t* fn0          = "media/audio/20110723-Kriesberg/Audio Files/Piano 3_01.wav";
  const cmChar_t* fn           = cmFsMakeFn(cmFsUserDir(),fn0,NULL,NULL );
  int             beg          =  6900826;
  int             end          = 13512262;

  cmDspInst_t* rfl = cmDspSysAllocInst(h,"Button", "Recd",  2, kCheckDuiId, 0.0 );
  cmDspInst_t* pfl = cmDspSysAllocInst(h,"Button", "Play",  2, kButtonDuiId, 1.0 );
  cmDspInst_t* rtp = cmDspSysAllocInst(h,"Scalar", "Ratio",    5, kNumberDuiId, 0.0,  10.0, 0.01, 1.0 );
  cmDspInst_t* ofp =  cmDspSysAllocInst(h,"Scalar", "Offset",  5, kNumberDuiId, 0.0,  cmDspSysSampleRate(h)*600.0, 1.0,  0.0);
  //cmDspInst_t* fnp =  cmDspSysAllocInst(h,"Fname",    NULL,  3, false,"Audio Files (*.wav,*.aiff,*.aif)\tAudio Files (*.{wav,aiff,aif})",fn);
  cmDspInst_t* php =  cmDspSysAllocInst(h,"Phasor",   NULL,  0 );
  cmDspInst_t* wtp =  cmDspSysAllocInst(h,"WaveTable",NULL,  6, ((int)cmDspSysSampleRate(h)), 1, fn, -1, beg, end );
  //cmDspInst_t* afp =  cmDspSysAllocInst(h,"AudioFileOut",NULL,2,"/home/kevin/temp/record0.aif",1);
  cmDspInst_t* lrp = cmDspSysAllocInst(h,"LoopRecd", NULL, 0 );
  cmDspInst_t* ao0p = cmDspSysAllocInst(h,"AudioOut",NULL,   1, useBuiltInFl ? 0 : 2 );
  cmDspInst_t* ao1p = cmDspSysAllocInst(h,"AudioOut",NULL,   1, useBuiltInFl ? 1 : 3 );
  
  cmDspSysConnectAudio(h, php,  "out", wtp,  "phs" );  // phasor -> wave table
  cmDspSysConnectAudio(h, wtp,  "out", lrp, "in" );   // wave table -> shift
  cmDspSysConnectAudio(h, lrp,  "out", ao0p, "in" );   // shift -> audio out
  cmDspSysConnectAudio(h, wtp,  "out", ao1p, "in" );    

  cmDspSysInstallCb(h, ofp, "val", wtp, "beg", NULL ); 
  //cmDspSysInstallCb(h, fnp, "out", wtp, "fn", NULL);    

  cmDspSysInstallCb(h, rfl, "out", lrp, "recd", NULL);    
  cmDspSysInstallCb(h, pfl, "out", lrp, "play", NULL);    
  cmDspSysInstallCb(h, rtp, "val", lrp, "ratio",NULL);
  return kOkDspRC;
}



cmDspRC_t _cmDspSysPgm_UiTest(cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t rc        = kOkDspRC;

  cmDspInst_t* mdp = cmDspSysAllocInst(h,"Scalar", "Mode",      5, kNumberDuiId, 0.0, 4.0, 1.0, 1.0);
  cmDspInst_t* wsp = cmDspSysAllocInst(h,"MsgList","wndSmpCnt", 3, "wndSmpCnt", NULL, 2);
  cmDspInst_t* hfp = cmDspSysAllocInst(h,"MsgList","hopFact",   3, "hopFact",   NULL, 2);
  cmDspInst_t* thp = cmDspSysAllocInst(h,"Scalar", "threshold", 5, kNumberDuiId, 0.0, 1.0, 0.01, 0.5 );
  cmDspInst_t* trp = cmDspSysAllocInst(h,"Scalar", "target",    5, kNumberDuiId, 0.0, 1.0, 0.01, 0.5 );
  cmDspInst_t* btn = cmDspSysAllocInst(h,"Button", "btn",       2, kButtonDuiId, 12.3 );
  cmDspInst_t* chk = cmDspSysAllocInst(h,"Button", "check",     2, kCheckDuiId, 0 );
  cmDspInst_t* txt = cmDspSysAllocInst(h,"Text",   "text",      1, "Hello" );
  cmDspInst_t* prp = cmDspSysAllocInst(h,"Printer", NULL,   1, ">" );
  cmDspInst_t* mtp = cmDspSysAllocInst(h,"Meter", "meter",  3, 0.0,  0.0, 4.0);
  cmDspInst_t* ctp = cmDspSysAllocInst(h,"Counter", NULL,   3, 0.0, 10.0, 1.0 );
                     cmDspSysAllocInst(h,"Label",  "label1", 1, "label2");
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    return rc;

  cmDspSysInstallCb(h, mdp, "val", thp, "val", NULL );

  cmDspSysInstallCb(h, mdp, "val", prp, "in", NULL );
  cmDspSysInstallCb(h, wsp, "out", prp, "in", NULL );
  cmDspSysInstallCb(h, hfp, "out", prp, "in", NULL );
  cmDspSysInstallCb(h, trp, "val", prp, "in", NULL );
  cmDspSysInstallCb(h, btn, "out", prp, "in", NULL );
  cmDspSysInstallCb(h, chk, "out", prp, "in", NULL );
  cmDspSysInstallCb(h, ctp, "out", prp, "in", NULL );

  cmDspSysInstallCb(h, mdp, "val", btn, "in", NULL );
  cmDspSysInstallCb(h, thp, "val", mtp, "in", NULL );
  cmDspSysInstallCb(h, thp, "val", trp, "val", NULL );

  cmDspSysInstallCb(h, btn, "sym", prp, "in", NULL );
  cmDspSysInstallCb(h, btn, "out", ctp, "next", NULL );
  
  cmDspSysInstallCb(h, txt, "val", prp, "in", NULL );

  return rc;

}

cmDspRC_t _cmDspSysPgm_Xfade( cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t rc         = kOkDspRC;
  unsigned  leftChIdx  = 0;
  unsigned  rightChIdx = 1;
  unsigned  chCnt      = 2;
  double    xfadeMs    = 1000;  // cross fade time
  double    sgHz       = 500;
  unsigned  sgShapeId  = 1;
  double    sgGain     = 0.5;

  cmDspInst_t* sg  = cmDspSysAllocInst(   h, "SigGen", NULL, 3, sgHz, sgShapeId, sgGain );

  cmDspInst_t* xfp  = cmDspSysAllocInst(h,"Xfader", NULL,    2, chCnt, xfadeMs );
   
  cmDspInst_t* ao0p = cmDspSysAllocInst(h,"AudioOut",NULL,   1, leftChIdx );
  cmDspInst_t* ao1p = cmDspSysAllocInst(h,"AudioOut",NULL,   1, rightChIdx );

  cmDspInst_t* im0p = cmDspSysAllocInst(h,"AMeter","In 0",  0);
  cmDspInst_t* im1p = cmDspSysAllocInst(h,"AMeter","In 1",  0);

  cmDspInst_t* om0p = cmDspSysAllocInst(h,"AMeter","Out 0", 0);
  cmDspInst_t* om1p = cmDspSysAllocInst(h,"AMeter","Out 1", 0);

  cmDspInst_t* mstr = cmDspSysAllocInst(h,"Button", "Mstr",   2, kCheckDuiId, 0.0 );
  cmDspInst_t* btn0 = cmDspSysAllocInst(h,"Button", "Fade 0", 2, kCheckDuiId, 0.0 );
  cmDspInst_t* btn1 = cmDspSysAllocInst(h,"Button", "Fade 1", 2, kCheckDuiId, 1.0 );


  cmDspInst_t* gm0p = cmDspSysAllocInst(h,"Meter","Gain 0", 3, 0.0, 0.0, 1.0);
  cmDspInst_t* gm1p = cmDspSysAllocInst(h,"Meter","Gain 1", 3, 0.0, 0.0, 1.0);
  
  cmDspInst_t* pon = cmDspSysAllocInst(h,"Printer", NULL,   1, "on:" );
  cmDspInst_t* pof = cmDspSysAllocInst(h,"Printer", NULL,   1, "off:" );  

  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    return rc;

  cmDspSysConnectAudio(h, sg, "out", im0p, "in");  // ain -> meter
  cmDspSysConnectAudio(h, sg, "out", im1p, "in");  //
  
  cmDspSysConnectAudio(h, sg, "out", xfp, "in-0"); // ain -> xfader
  cmDspSysConnectAudio(h, sg, "out", xfp, "in-1");

  cmDspSysConnectAudio(h, xfp, "out-0", om0p, "in" );   // xfade -> meter
  cmDspSysConnectAudio(h, xfp, "out-1", om1p, "in" );   // 

  cmDspSysConnectAudio(h, xfp, "out-0", ao0p, "in" );   // xfade -> aout
  cmDspSysConnectAudio(h, xfp, "out-1", ao1p, "in" );   // 

  cmDspSysInstallCb(h, btn0, "out", xfp, "gate-0", NULL ); // check -> xfade gate
  cmDspSysInstallCb(h, btn1, "out", xfp, "gate-1", NULL );
  cmDspSysInstallCb(h, mstr, "out", xfp, "mgate",  NULL );

  cmDspSysInstallCb(h, xfp, "gain-0", gm0p, "in", NULL );
  cmDspSysInstallCb(h, xfp, "gain-1", gm1p, "in", NULL );

  cmDspSysInstallCb(h, xfp, "on", pon, "in", NULL );
  cmDspSysInstallCb(h, xfp, "off", pof, "in", NULL );

  return cmDspSysLastRC(h);
}

cmDspRC_t _cmDspSysPgm6( cmDspSysH_t h, void* audioDir )
{
  cmDspRC_t rc        = kOkDspRC;
  int       wndSmpCnt = 2048;
  int       hopFact   = 4;

  //const char* afDir = "/Volumes/LaTetra0/media/audio/20110723-Kriesberg/Audio Files";
  //const char* afDir = "/Users/administrator/Documents/kc";
  const char* afDir = "/home/kevin/media/audio/20110723-Kriesberg/Audio Files";

  cmDspInst_t* ph0p = cmDspSysAllocInst(h,"Phasor",  NULL,   0 );
  cmDspInst_t* wt0p = cmDspSysAllocInst(h,"WaveTable",NULL,  0  );
  cmDspInst_t* ro0p = cmDspSysAllocInst(h,"Reorder", NULL,   5, 3, 2, 0, 1, 2 );
  cmDspInst_t* kr0p = cmDspSysAllocInst(h,"Kr",      NULL,   2, wndSmpCnt, hopFact );
  cmDspInst_t* kr1p = cmDspSysAllocInst(h,"Kr",      NULL,   2, wndSmpCnt, hopFact );

  cmDspInst_t* prp = cmDspSysAllocInst(h,"Printer", NULL,   0 );

  bool useBuiltInFl = true;

  // MOTU Traveler: Use channels 2&3 (out plugs:3&4) because 0&1 do not show up on plugs 1&2.
  cmDspInst_t* ao0p = cmDspSysAllocInst(h,"AudioOut",NULL,   1, useBuiltInFl ? 0 : 2 );
  cmDspInst_t* ao1p = cmDspSysAllocInst(h,"AudioOut",NULL,   1, useBuiltInFl ? 1 : 3 );

  // BUG: If audio inputs are not connected a 'not an audio buffer' msg is generated.
  // This is a problem with type determination in the cmDspClass related code : see cmDspClass.c 833 
  // for a place that this error would be thrown in the following audio outputs were not connected
  // to audio sources.
  cmDspInst_t* ao2p = NULL;
  cmDspInst_t* ao3p = NULL;

  if( !useBuiltInFl )
  {
    ao2p = cmDspSysAllocInst(h,"AudioOut",NULL,   1, 4 );
    ao3p = cmDspSysAllocInst(h,"AudioOut",NULL,   1, 5 );
  }

  cmDspInst_t* al0p = cmDspSysAllocInst(h,"MsgList","audFiles", 2, "audFiles",NULL);
  cmDspInst_t* fl0p = cmDspSysAllocInst(h,"MsgList","audFragsA", 2, "audFrags",NULL);
  cmDspInst_t* fn0p = cmDspSysAllocInst(h,"Sprintf","filename", 1, "%s/%s_%02i.wav");

  cmDspInst_t* md0p = cmDspSysAllocInst(h,"Scalar", "Mode",      5, kNumberDuiId, 0.0, 4.0, 1.0, 1.0);
  cmDspInst_t* ws0p = cmDspSysAllocInst(h,"MsgList","wndSmpCnt", 3, "wndSmpCnt", NULL, 2);
  cmDspInst_t* hf0p = cmDspSysAllocInst(h,"MsgList","hopFact",   3, "hopFact",   NULL, 2);
  cmDspInst_t* th0p = cmDspSysAllocInst(h,"Scalar", "threshold", 5, kNumberDuiId, 0.0, 100.0, 1.0,  60.0 );
  cmDspInst_t* us0p = cmDspSysAllocInst(h,"Scalar", "upr slope", 5, kNumberDuiId, 0.0,  10.0, 0.01,  0.0 ); 
  cmDspInst_t* ls0p = cmDspSysAllocInst(h,"Scalar", "lwr slope", 5, kNumberDuiId, 0.3,  10.0, 0.01,  2.0 );
  cmDspInst_t* of0p = cmDspSysAllocInst(h,"Scalar", "offset",    5, kNumberDuiId, 0.0, 100.0, 0.01, 30.0 );
  cmDspInst_t* iv0p = cmDspSysAllocInst(h,"Scalar", "invert",    5, kNumberDuiId, 0.0,   1.0, 1.0,   0.0 );  
  cmDspSysNewColumn(h,0);

  //cmDspInst_t* al1p = cmDspSysAllocInst(h,"MsgList","audFiles", 2, "audFiles",NULL);
  //cmDspInst_t* fl1p = cmDspSysAllocInst(h,"MsgList","audFrags1", 2, "audFrags",NULL);
  //cmDspInst_t* fn1p = cmDspSysAllocInst(h,"Sprintf","filename", 1, "%s/%s_%02i.wav");
  cmDspInst_t* md1p = cmDspSysAllocInst(h,"Scalar", "Mode",      5, kNumberDuiId, 0.0, 4.0, 1.0, 1.0);
  cmDspInst_t* ws1p = cmDspSysAllocInst(h,"MsgList","wndSmpCnt", 3, "wndSmpCnt", NULL, 2);
  cmDspInst_t* hf1p = cmDspSysAllocInst(h,"MsgList","hopFact",   3, "hopFact",   NULL, 2);
  cmDspInst_t* th1p = cmDspSysAllocInst(h,"Scalar", "threshold", 5, kNumberDuiId, 0.0, 100.0, 1.0,  60.0 );
  cmDspInst_t* us1p = cmDspSysAllocInst(h,"Scalar", "upr slope", 5, kNumberDuiId, 0.0,  10.0, 0.01,  0.0 ); 
  cmDspInst_t* ls1p = cmDspSysAllocInst(h,"Scalar", "lwr slope", 5, kNumberDuiId, 0.3,  10.0, 0.01,  2.0 );
  cmDspInst_t* of1p = cmDspSysAllocInst(h,"Scalar", "offset",    5, kNumberDuiId, 0.0, 100.0, 0.01, 30.0 );
  cmDspInst_t* iv1p = cmDspSysAllocInst(h,"Scalar", "invert",    5, kNumberDuiId, 0.0,   1.0, 1.0,   0.0 );  

  cmDspSysNewColumn(h,0);
  cmDspInst_t* dnp = cmDspSysAllocInst(h,"Fname",  "audDir",   3, true,NULL,afDir);

  cmDspInst_t* mtp = cmDspSysAllocInst(h,"Meter","MyMeter", 3, 50.0, 0.0, 100.0);
  cmDspInst_t* atp = cmDspSysAllocInst(h,"AMeter","Audio Meter",0);


  if( (rc = cmDspSysLastRC(h)) != kOkDspRC )
    return rc;

  cmDspSysConnectAudio(h, ph0p, "out", wt0p, "phs"); // phasor->wt:phs
  cmDspSysConnectAudio(h, wt0p, "out", kr0p, "in");  // wt->kr
  cmDspSysConnectAudio(h, kr0p, "out", ao0p, "in");  // kr->aout- 0
  cmDspSysConnectAudio(h, wt0p, "out", kr1p, "in");  // wt->kr
  cmDspSysConnectAudio(h, kr1p, "out", ao1p, "in");  // kr->aout - 1

  if( ao2p != NULL )
    cmDspSysConnectAudio(h, wt0p,"out", ao2p, "in");  // wt->aout - 2

  if( ao3p != NULL )
    cmDspSysConnectAudio(h, wt0p,"out", ao3p, "in");  // wt->aout - 3

  cmDspSysConnectAudio(h, wt0p,"out", atp, "in");      // wt->meter

  cmDspSysInstallCb(h, ws0p, "out", kr0p, "wndn", NULL );   // wndSmpCnt->kr
  cmDspSysInstallCb(h, hf0p, "out", kr0p, "hopf", NULL );   // hopFact->kr
  cmDspSysInstallCb(h, md0p, "val", kr0p, "mode", NULL );   // mode->kr
  cmDspSysInstallCb(h, th0p, "val", kr0p, "thrh", NULL );   // thresh->kr
  cmDspSysInstallCb(h, ls0p, "val", kr0p, "lwrs", NULL );   // lwrSlope->kr
  cmDspSysInstallCb(h, us0p, "val", kr0p, "uprs", NULL );   // uprSlope->kr
  cmDspSysInstallCb(h, of0p, "val", kr0p, "offs", NULL );   // offset->kr
  cmDspSysInstallCb(h, iv0p, "val", kr0p, "invt", NULL );   // invert->kr

  cmDspSysInstallCb(h, th0p, "val", mtp, "in", NULL ); 

  cmDspSysInstallCb(h, ws1p, "out", kr1p, "wndn", NULL );   // wndSmpCnt->kr
  cmDspSysInstallCb(h, hf1p, "out", kr1p, "hopf", NULL );   // hopFact->kr
  cmDspSysInstallCb(h, md1p, "val", kr1p, "mode", NULL );   // mode->kr
  cmDspSysInstallCb(h, th1p, "val", kr1p, "thrh", NULL );   // thresh->kr
  cmDspSysInstallCb(h, ls1p, "val", kr1p, "lwrs", NULL );   // lwrSlope->kr
  cmDspSysInstallCb(h, us1p, "val", kr1p, "uprs", NULL );   // uprSlope->kr
  cmDspSysInstallCb(h, of1p, "val", kr1p, "offs", NULL );   // offset->kr
  cmDspSysInstallCb(h, iv1p, "val", kr1p, "invt", NULL );   // invert->kr


  cmDspSysInstallCb(h, dnp,  "out",  fn0p, "in-0", NULL);    // dir->fn:0
  cmDspSysInstallCb(h, al0p, "out",  fn0p, "in-1", NULL );   // fn->fn:1
  cmDspSysInstallCb(h, fl0p, "take", fn0p, "in-2", NULL );   // take->fn:2
  cmDspSysInstallCb(h, fn0p, "out",  ro0p, "in-0", NULL);    // fn->print

  cmDspSysInstallCb(h, fl0p, "beg",  ro0p, "in-1", NULL);  //   
  cmDspSysInstallCb(h, fl0p, "end",  ro0p, "in-2", NULL);  //

  cmDspSysInstallCb(h, ro0p, "out-0", wt0p, "fn", NULL );   // fn->wt:fn
  cmDspSysInstallCb(h, ro0p, "out-1", wt0p, "beg", NULL );  // beg->wt:beg
  cmDspSysInstallCb(h, ro0p, "out-2", wt0p, "end", NULL );  // end->wt:end

  cmDspSysInstallCb(h, ro0p, "out-1", prp, "in", NULL ); // 

  //cmDspSysInstallCb(h, dnp,  "out",  fn1p, "in-0", NULL);    // dir->fn:0
  //cmDspSysInstallCb(h, al1p, "out",  fn1p, "in-1", NULL );   // fn->fn:1
  //cmDspSysInstallCb(h, fl1p, "take", fn1p, "in-2", NULL );   // take->fn:2
  //cmDspSysInstallCb(h, fn1p, "out",  ro1p, "in-0", NULL);    // fn->print

  //cmDspSysInstallCb(h, fl1p, "beg",  ro1p, "in-1", NULL);  //   
  //cmDspSysInstallCb(h, fl1p, "end",  ro1p, "in-2", NULL);  //

  //cmDspSysInstallCb(h, ro1p, "out-0", wt1p, "fn", NULL );   // fn->wt:fn
  //cmDspSysInstallCb(h, ro1p, "out-1", wt1p, "beg", NULL );  // beg->wt:beg
  //cmDspSysInstallCb(h, ro1p, "out-2", wt1p, "end", NULL );  // end->wt:end


  return cmDspSysLastRC(h);
}

cmDspRC_t _cmDspSysPgmGuitar( cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t rc        = kOkDspRC;
  int       wndSmpCnt = 2048;
  int       hopFact   = 4;

  const char* afDir = "/Volumes/LaTetra0/media/audio/20110723-Kriesberg/Audio Files";
  //const char* afDir = "/Users/administrator/Documents/kc";

  cmDspInst_t* ph0p = cmDspSysAllocInst(h,"Phasor",  NULL,   0 );
  cmDspInst_t* wt0p = cmDspSysAllocInst(h,"WaveTable",NULL,  0  );
  cmDspInst_t* ro0p = cmDspSysAllocInst(h,"Reorder", NULL,   5, 3, 2, 0, 1, 2 );
  cmDspInst_t* kr0p = cmDspSysAllocInst(h,"Kr",      NULL,   2, wndSmpCnt, hopFact );
  cmDspInst_t* kr1p = cmDspSysAllocInst(h,"Kr",      NULL,   2, wndSmpCnt, hopFact );

  cmDspInst_t* prp = cmDspSysAllocInst(h,"Printer", NULL,   0 );

  bool useBuiltInFl = true;

  // MOTU Traveler: Use channels 2&3 (out plugs:3&4) because 0&1 do not show up on plugs 1&2.
  cmDspInst_t* ao0p = cmDspSysAllocInst(h,"AudioOut",NULL,   1, useBuiltInFl ? 0 : 2 );
  cmDspInst_t* ao1p = cmDspSysAllocInst(h,"AudioOut",NULL,   1, useBuiltInFl ? 1 : 3 );

  // BUG: If audio inputs are not connected a 'not an audio buffer' msg is generated.
  // This is a problem with type determination in the cmDspClass related code : see cmDspClass.c 833 
  // for a place that this error would be thrown in the following audio outputs were not connected
  // to audio sources.
  cmDspInst_t* ao2p = NULL;
  cmDspInst_t* ao3p = NULL;

  if( !useBuiltInFl )
  {
    ao2p = cmDspSysAllocInst(h,"AudioOut",NULL,   1, 4 );
    ao3p = cmDspSysAllocInst(h,"AudioOut",NULL,   1, 5 );
  }

  cmDspInst_t* al0p = cmDspSysAllocInst(h,"MsgList","audFiles", 2, "audFiles",NULL);
  cmDspInst_t* fl0p = cmDspSysAllocInst(h,"MsgList","audFragsA", 2, "audFrags",NULL);
  cmDspInst_t* fn0p = cmDspSysAllocInst(h,"Sprintf","filename", 1, "%s/%s_%02i.wav");

  cmDspInst_t* md0p = cmDspSysAllocInst(h,"Scalar", "Mode",      5, kNumberDuiId, 0.0, 4.0, 1.0, 1.0);
  cmDspInst_t* ws0p = cmDspSysAllocInst(h,"MsgList","wndSmpCnt", 3, "wndSmpCnt", NULL, 2);
  cmDspInst_t* hf0p = cmDspSysAllocInst(h,"MsgList","hopFact",   3, "hopFact",   NULL, 2);
  cmDspInst_t* th0p = cmDspSysAllocInst(h,"Scalar", "threshold", 5, kNumberDuiId, 0.0, 100.0, 1.0,  60.0 );
  cmDspInst_t* us0p = cmDspSysAllocInst(h,"Scalar", "upr slope", 5, kNumberDuiId, 0.0,  10.0, 0.01,  0.0 ); 
  cmDspInst_t* ls0p = cmDspSysAllocInst(h,"Scalar", "lwr slope", 5, kNumberDuiId, 0.3,  10.0, 0.01,  2.0 );
  cmDspInst_t* of0p = cmDspSysAllocInst(h,"Scalar", "offset",    5, kNumberDuiId, 0.0, 100.0, 0.01, 30.0 );
  cmDspInst_t* iv0p = cmDspSysAllocInst(h,"Scalar", "invert",    5, kNumberDuiId, 0.0,   1.0, 1.0,   0.0 );  
  cmDspSysNewColumn(h,0);

  //cmDspInst_t* al1p = cmDspSysAllocInst(h,"MsgList","audFiles", 2, "audFiles",NULL);
  //cmDspInst_t* fl1p = cmDspSysAllocInst(h,"MsgList","audFrags1", 2, "audFrags",NULL);
  //cmDspInst_t* fn1p = cmDspSysAllocInst(h,"Sprintf","filename", 1, "%s/%s_%02i.wav");
  cmDspInst_t* md1p = cmDspSysAllocInst(h,"Scalar", "Mode",      5, kNumberDuiId, 0.0, 4.0, 1.0, 1.0);
  cmDspInst_t* ws1p = cmDspSysAllocInst(h,"MsgList","wndSmpCnt", 3, "wndSmpCnt", NULL, 2);
  cmDspInst_t* hf1p = cmDspSysAllocInst(h,"MsgList","hopFact",   3, "hopFact",   NULL, 2);
  cmDspInst_t* th1p = cmDspSysAllocInst(h,"Scalar", "threshold", 5, kNumberDuiId, 0.0, 100.0, 1.0,  60.0 );
  cmDspInst_t* us1p = cmDspSysAllocInst(h,"Scalar", "upr slope", 5, kNumberDuiId, 0.0,  10.0, 0.01,  0.0 ); 
  cmDspInst_t* ls1p = cmDspSysAllocInst(h,"Scalar", "lwr slope", 5, kNumberDuiId, 0.3,  10.0, 0.01,  2.0 );
  cmDspInst_t* of1p = cmDspSysAllocInst(h,"Scalar", "offset",    5, kNumberDuiId, 0.0, 100.0, 0.01, 30.0 );
  cmDspInst_t* iv1p = cmDspSysAllocInst(h,"Scalar", "invert",    5, kNumberDuiId, 0.0,   1.0, 1.0,   0.0 );  

  cmDspSysNewColumn(h,0);
  cmDspInst_t* dnp = cmDspSysAllocInst(h,"Fname",  "audDir",   3, true,NULL,afDir);

  cmDspInst_t* mtp = cmDspSysAllocInst(h,"Meter","MyMeter", 3, 50.0, 0.0, 100.0);
  cmDspInst_t* atp = cmDspSysAllocInst(h,"AMeter","Audio Meter",0);


  if( (rc = cmDspSysLastRC(h)) != kOkDspRC )
    return rc;

  cmDspSysConnectAudio(h, ph0p, "out", wt0p, "phs"); // phasor->wt:phs
  cmDspSysConnectAudio(h, wt0p, "out", kr0p, "in");  // wt->kr
  cmDspSysConnectAudio(h, kr0p, "out", ao0p, "in");  // kr->aout- 0
  cmDspSysConnectAudio(h, wt0p, "out", kr1p, "in");  // wt->kr
  cmDspSysConnectAudio(h, kr1p, "out", ao1p, "in");  // kr->aout - 1

  if( ao2p != NULL )
    cmDspSysConnectAudio(h, wt0p,"out", ao2p, "in");  // wt->aout - 2

  if( ao3p != NULL )
    cmDspSysConnectAudio(h, wt0p,"out", ao3p, "in");  // wt->aout - 3

  cmDspSysConnectAudio(h, wt0p,"out", atp, "in");      // wt->meter

  cmDspSysInstallCb(h, ws0p, "out", kr0p, "wndn", NULL );   // wndSmpCnt->kr
  cmDspSysInstallCb(h, hf0p, "out", kr0p, "hopf", NULL );   // hopFact->kr
  cmDspSysInstallCb(h, md0p, "val", kr0p, "mode", NULL );   // mode->kr
  cmDspSysInstallCb(h, th0p, "val", kr0p, "thrh", NULL );   // thresh->kr
  cmDspSysInstallCb(h, ls0p, "val", kr0p, "lwrs", NULL );   // lwrSlope->kr
  cmDspSysInstallCb(h, us0p, "val", kr0p, "uprs", NULL );   // uprSlope->kr
  cmDspSysInstallCb(h, of0p, "val", kr0p, "offs", NULL );   // offset->kr
  cmDspSysInstallCb(h, iv0p, "val", kr0p, "invt", NULL );   // invert->kr

  cmDspSysInstallCb(h, th0p, "val", mtp, "in", NULL ); 

  cmDspSysInstallCb(h, ws1p, "out", kr1p, "wndn", NULL );   // wndSmpCnt->kr
  cmDspSysInstallCb(h, hf1p, "out", kr1p, "hopf", NULL );   // hopFact->kr
  cmDspSysInstallCb(h, md1p, "val", kr1p, "mode", NULL );   // mode->kr
  cmDspSysInstallCb(h, th1p, "val", kr1p, "thrh", NULL );   // thresh->kr
  cmDspSysInstallCb(h, ls1p, "val", kr1p, "lwrs", NULL );   // lwrSlope->kr
  cmDspSysInstallCb(h, us1p, "val", kr1p, "uprs", NULL );   // uprSlope->kr
  cmDspSysInstallCb(h, of1p, "val", kr1p, "offs", NULL );   // offset->kr
  cmDspSysInstallCb(h, iv1p, "val", kr1p, "invt", NULL );   // invert->kr


  cmDspSysInstallCb(h, dnp,  "out",  fn0p, "in-0", NULL);    // dir->fn:0
  cmDspSysInstallCb(h, al0p, "out",  fn0p, "in-1", NULL );   // fn->fn:1
  cmDspSysInstallCb(h, fl0p, "take", fn0p, "in-2", NULL );   // take->fn:2
  cmDspSysInstallCb(h, fn0p, "out",  ro0p, "in-0", NULL);    // fn->print

  cmDspSysInstallCb(h, fl0p, "beg",  ro0p, "in-1", NULL);  //   
  cmDspSysInstallCb(h, fl0p, "end",  ro0p, "in-2", NULL);  //

  cmDspSysInstallCb(h, ro0p, "out-0", wt0p, "fn", NULL );   // fn->wt:fn
  cmDspSysInstallCb(h, ro0p, "out-1", wt0p, "beg", NULL );  // beg->wt:beg
  cmDspSysInstallCb(h, ro0p, "out-2", wt0p, "end", NULL );  // end->wt:end

  cmDspSysInstallCb(h, ro0p, "out-1", prp, "in", NULL ); // 

  //cmDspSysInstallCb(h, dnp,  "out",  fn1p, "in-0", NULL);    // dir->fn:0
  //cmDspSysInstallCb(h, al1p, "out",  fn1p, "in-1", NULL );   // fn->fn:1
  //cmDspSysInstallCb(h, fl1p, "take", fn1p, "in-2", NULL );   // take->fn:2
  //cmDspSysInstallCb(h, fn1p, "out",  ro1p, "in-0", NULL);    // fn->print

  //cmDspSysInstallCb(h, fl1p, "beg",  ro1p, "in-1", NULL);  //   
  //cmDspSysInstallCb(h, fl1p, "end",  ro1p, "in-2", NULL);  //

  //cmDspSysInstallCb(h, ro1p, "out-0", wt1p, "fn", NULL );   // fn->wt:fn
  //cmDspSysInstallCb(h, ro1p, "out-1", wt1p, "beg", NULL );  // beg->wt:beg
  //cmDspSysInstallCb(h, ro1p, "out-2", wt1p, "end", NULL );  // end->wt:end


  return cmDspSysLastRC(h);
}




cmDspRC_t _cmDspSysPgm_Pickups0( cmDspSysH_t h, void** userPtrPtr )
{
  unsigned i;
  unsigned chCnt = 8;

  double delayFb    = 0.0;
  double delayMaxMs = 1000.0;
  cmDspInst_t* chArray[chCnt];

  cmDspInst_t* m_mxWet  = cmDspSysAllocInst(h,"Scalar", "Mstr Wet",      5, kNumberDuiId, 0.0,  2.0, 0.01, 1.0 ); 
  cmDspInst_t* m_mxDry  = cmDspSysAllocInst(h,"Scalar", "Mstr Dry",      5, kNumberDuiId, 0.0,  2.0, 0.01, 1.0 );
  cmDspSysNewColumn(h,0);
  cmDspInst_t* m_lpByp  = cmDspSysAllocInst(h,"Button", "Mstr Lp Byps",  2, kCheckDuiId, 0.0 );
  cmDspInst_t* m_lpRcd  = cmDspSysAllocInst(h,"Button", "Mstr Lp Recd",  2, kCheckDuiId, 0.0 );
  cmDspInst_t* m_lpRat  = cmDspSysAllocInst(h,"Scalar", "Mstr Lp Ratio", 5, kNumberDuiId, 0.0,  10.0, 0.01, 1.0 );
  cmDspSysNewColumn(h,0);
  cmDspInst_t* m_dlyByp = cmDspSysAllocInst(h,"Button", "Mstr Dly Byps", 2, kCheckDuiId, 0.0 );
  cmDspInst_t* m_dlyMs  = cmDspSysAllocInst(h,"Scalar", "Mstr Dly Time", 5, kNumberDuiId, 0.0,  delayMaxMs, 1.0,  0.0 );
  cmDspInst_t* m_dlyFb  = cmDspSysAllocInst(h,"Scalar", "Mstr Dly Fb",   5, kNumberDuiId, 0.0,  0.999,      0.01, 0.0 );
  cmDspSysNewColumn(h,0);
  cmDspInst_t* m_psByp = cmDspSysAllocInst(h,"Button", "Mstr PS Byps",   2, kCheckDuiId, 0.0 );
  cmDspInst_t* m_psRat = cmDspSysAllocInst(h,"Scalar", "Mstr PS Ratio",  5, kNumberDuiId, 0.0,  10.0, 0.01, 1.0 );
  cmDspSysNewColumn(h,0);
  cmDspInst_t* m_rcByp = cmDspSysAllocInst(h,"Button", "Mstr Rect Byps", 2, kCheckDuiId, 0.0 );
  
  cmDspSysInsertHorzBorder(h);

  for(i=0; i<chCnt; ++i)
  {
    int    chIdx      = i;

    cmDspInst_t* mxWet  = cmDspSysAllocInst(h,"Scalar", "Wet",        5, kNumberDuiId, 0.0,  2.0, 0.01, 1.0 ); 
    cmDspInst_t* mxDry  = cmDspSysAllocInst(h,"Scalar", "Dry",        5, kNumberDuiId, 0.0,  2.0, 0.01, 1.0 );
    cmDspInst_t* lpByp  = cmDspSysAllocInst(h,"Button", "Lp Byps",    2, kCheckDuiId,  0.0 );
    cmDspInst_t* lpRcd  = cmDspSysAllocInst(h,"Button", "Lp Recd",    2, kCheckDuiId,  0.0 );
    cmDspInst_t* lpRat  = cmDspSysAllocInst(h,"Scalar", "Lp Ratio",   5, kNumberDuiId, 0.0,  10.0, 0.01, 1.0 );
    cmDspInst_t* dlyByp = cmDspSysAllocInst(h,"Button", "Delay Byps", 2, kCheckDuiId,  0.0 );
    cmDspInst_t* dlyMs  = cmDspSysAllocInst(h,"Scalar", "Delay Time", 5, kNumberDuiId, 0.0,  delayMaxMs, 1.0,  0.0 );
    cmDspInst_t* dlyFb  = cmDspSysAllocInst(h,"Scalar", "Delay Fb",   5, kNumberDuiId, 0.0,  0.999,      0.01, 0.0 );
    cmDspInst_t* psByp  = cmDspSysAllocInst(h,"Button", "PS Byps",    2, kCheckDuiId, 0.0 );
    cmDspInst_t* psRat  = cmDspSysAllocInst(h,"Scalar", "PS Ratio",   5, kNumberDuiId, 0.0,  10.0, 0.01, 1.0 );
    cmDspInst_t* rcByp  = cmDspSysAllocInst(h,"Button", "Rect Byps",  2, kCheckDuiId, 0.0 );

    cmDspInst_t* ain  = cmDspSysAllocInst(h, "AudioIn",   NULL, 1, chIdx );
    cmDspInst_t* loop = cmDspSysAllocInst(h, "LoopRecd",  NULL, 0 );
    cmDspInst_t* dely = cmDspSysAllocInst(h, "Delay",     NULL, 2, delayMaxMs, delayFb );
    cmDspInst_t* pshf = cmDspSysAllocInst(h, "PShift",    NULL, 0 );
    cmDspInst_t* rect = cmDspSysAllocInst(h, "Rectify",   NULL, 0 );
    cmDspInst_t* amix = cmDspSysAllocInst(h, "AMix",      NULL, 1, 2 );
    //cmDspInst_t* aout = cmDspSysAllocInst(h, "AudioOut", NULL, 1, chIdx );

    chArray[i] = amix;

    cmDspSysNewColumn(h,0);
    
    cmDspSysConnectAudio(h, ain,  "out", loop, "in");          // ain   -> loop
    cmDspSysConnectAudio(h, loop, "out", dely, "in");          // loop  -> delay
    cmDspSysConnectAudio(h, dely, "out", pshf, "in");          // delay -> pshf
    cmDspSysConnectAudio(h, pshf, "out", rect, "in");          // pshf  -> rect
    cmDspSysConnectAudio(h, rect, "out", amix, "in-0");        // rect  -> mix_wet
    cmDspSysConnectAudio(h, ain,  "out", amix, "in-1");        // ain   -> mix_dry
    //cmDspSysConnectAudio(h, amix, "out", aout, "in");          // mix   -> out 

    cmDspSysInstallCb(h, mxWet, "val", amix, "gain-0", NULL );
    cmDspSysInstallCb(h, mxDry, "val", amix, "gain-1", NULL );
    cmDspSysInstallCb(h, lpByp, "out", loop, "bypass", NULL );
    cmDspSysInstallCb(h, lpRcd, "out", loop, "recd",   NULL );
    cmDspSysInstallCb(h, lpRat, "val", loop, "ratio",  NULL );
    cmDspSysInstallCb(h, dlyByp,"val", dely, "bypass", NULL );
    cmDspSysInstallCb(h, dlyMs, "val", dely, "time",   NULL );
    cmDspSysInstallCb(h, dlyFb, "out", dely, "fb",     NULL );
    cmDspSysInstallCb(h, psByp, "out", pshf, "bypass", NULL );
    cmDspSysInstallCb(h, psRat, "val", pshf, "ratio",  NULL );
    cmDspSysInstallCb(h, rcByp, "out", rect, "bypass", NULL );

    cmDspSysInstallCb(h, m_mxWet, "val", mxWet, "in",  NULL );
    cmDspSysInstallCb(h, m_mxDry, "val", mxDry, "in",  NULL );
    cmDspSysInstallCb(h, m_lpByp, "out", lpByp, "in",  NULL );
    cmDspSysInstallCb(h, m_lpRcd, "out", lpRcd, "in",  NULL );
    cmDspSysInstallCb(h, m_lpRat, "val", lpRat, "in",  NULL ); 
    cmDspSysInstallCb(h, m_dlyByp,"out", dlyByp,"in",  NULL );
    cmDspSysInstallCb(h, m_dlyMs, "val", dlyMs, "in",  NULL );
    cmDspSysInstallCb(h, m_dlyFb, "val", dlyFb, "in",  NULL );    
    cmDspSysInstallCb(h, m_psByp, "out", psByp, "in",  NULL );
    cmDspSysInstallCb(h, m_psRat, "val", psRat, "in",  NULL );   
    cmDspSysInstallCb(h, m_rcByp, "out", rcByp, "in",  NULL );
    
  }

  double dfltGain = 0.5;
  cmDspInst_t* omix = cmDspSysAllocInst(h,  "AMix",     NULL, 9, 8, dfltGain, dfltGain, dfltGain, dfltGain, dfltGain, dfltGain, dfltGain, dfltGain );
  cmDspInst_t* aout0 = cmDspSysAllocInst(h, "AudioOut", NULL, 1, 0 );
  cmDspInst_t* aout1 = cmDspSysAllocInst(h, "AudioOut", NULL, 1, 1 );
  cmDspInst_t* aout2 = cmDspSysAllocInst(h, "AudioOut", NULL, 1, 2 );
  cmDspInst_t* aout3 = cmDspSysAllocInst(h, "AudioOut", NULL, 1, 3 );


  for(i=0; i<chCnt; ++i)
  {
    char label[32];
    snprintf(label,32,"in-%i",i);
    cmDspSysConnectAudio(h, chArray[i], "out", omix, label);
  }

  cmDspSysConnectAudio(h, omix, "out", aout0, "in");
  cmDspSysConnectAudio(h, omix, "out", aout1, "in");
  cmDspSysConnectAudio(h, omix, "out", aout2, "in");
  cmDspSysConnectAudio(h, omix, "out", aout3, "in");

  return kOkDspRC;
}

#include "cmAudioFile.h"
#include "cmProcObj.h"
#include "cmProc.h"
#include "cmProc3.h"

  // Usage:
  // 1) Push 'start'.
  // 2) Select the first element in the Ch Cfg List UI.
  // 3) Play several examples of the note.
  // 4) Select the next element in the Ch Cfg List UI.
  // 5) Go to 3) until all ch's have been played.
  // 6) Push 'proc'. A new set of gains will be calc'd and sent to the audio input channels.
  // Note that if a mistake is made while playing a set of notes in 3) then 
  // push select the same element from the list again and replay.
  // The order the notes are played in does not make any difference.

cmDspRC_t _cmDspSysPgm_AutoGain( cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t       rc              = kOkDspRC;
  unsigned        i;
  //unsigned        j;
  cmErr_t         err;  
  const cmChar_t* errLabelPtr     = NULL;
  cmCtx_t*        cmCtx           = cmDspSysPgmCtx(h);
  unsigned        chCnt           = 0;
  unsigned        nsChCnt         = 0;
  const cmChar_t* chCfgFn         = NULL;
  const cmChar_t* chCfgPath       = NULL;
  unsigned        agMedCnt        = 5;
  unsigned        agAvgCnt        = 9;
  unsigned        agSuprCnt       = 3;
  unsigned        agOffCnt        = 3;
  cmReal_t        agSuprCoeff     = 1.4;
  cmReal_t        agOnThreshDb    = -53.0;
  cmReal_t        agOffThreshDb   = -80.0;
  cmReal_t        agHopMs         = 25;
  //cmReal_t        cdMaxTimeSpanMs = 50;
  //cmReal_t        cdMinNoteCnt    = 3;
  unsigned        labelCharCnt    = 31;
  char            label0[ labelCharCnt + 1];


  cmErrSetup(&err,&cmCtx->rpt,"Auto-gain");

  // get the name of channel cfg file
  if( cmJsonPathValues( cmDspSysPgmRsrcHandle(h),"cfg/",NULL,&errLabelPtr,
      "chCfgFn",             kStringTId, &chCfgFn,
      "agParms/hopMs",       kRealTId,   &agHopMs,
      "agParms/medCnt",      kIntTId,    &agMedCnt,
      "agParms/avgCnt",      kIntTId,    &agAvgCnt,
      "agParms/suprCnt",     kIntTId,    &agSuprCnt,
      "agParms/offCnt",      kIntTId,    &agOffCnt,
      "agParms/suprCoeff",   kRealTId,   &agSuprCoeff,
      "agParms/onThreshDb",  kRealTId,   &agOnThreshDb,
      "agParms/offThreshDb", kRealTId,   &agOffThreshDb,
      NULL) != kOkJsRC )
  {
    rc = cmErrMsg(&err,kPgmCfgFailDspRC,"An error occurred while reading the required auto-tune JSON parameters.");
    goto errLabel;
  }

  // get the count of channels from the ch. cfg. array
  if(( chCnt = cmChCfgChannelCount(cmCtx,chCfgFn,&nsChCnt)) == 0 )
  {
    rc = cmErrMsg(&err,kPgmCfgFailDspRC,"Unable to obtain the channel count from '%s'.",cmStringNullGuard(chCfgFn));
    goto errLabel;
  }

  // prepend the prefs directory to the ch. cfg filename 
  chCfgPath = cmFsMakeFn(cmFsPrefsDir(),chCfgFn,NULL,NULL);
  
  if( rc == kOkDspRC )
  {
    //mDspInst_t* ain[chCnt];
    cmDspInst_t* ef[chCnt];
    cmDspInst_t* mtr[chCnt];
    cmDspInst_t* amtr[chCnt];
    cmDspInst_t* on[chCnt];
    cmDspInst_t* off[chCnt];
    cmDspInst_t* rms[chCnt];
    ///cmDspInst_t* fdr0[chCnt];
    ///cmDspInst_t* fdr1[chCnt];
    ///cmDspInst_t* fdr2[chCnt];

    // allocate the audio inputs
    //for(i=0; i<chCnt; ++i)
    //  ain[i] = cmDspSysAllocAudioIn(h,i,1.0);
    unsigned inChCnt;
    cmDspInst_t** ain = cmDspSysAllocAudioInAR( h, "audioInMap", 1.0, &inChCnt );

    // allocate the auto-gain calibration object
    cmDspInst_t* ag     = cmDspSysAllocInst( h, "AutoGain",     NULL,     9, chCnt, agHopMs,agMedCnt,agAvgCnt,agSuprCnt,agOffCnt,agSuprCoeff,agOnThreshDb,agOffThreshDb );

    // allocate the command buttons
    cmDspInst_t* start  = cmDspSysAllocButton(h, "start",  0);
    cmDspInst_t* proc   = cmDspSysAllocButton(h, "proc",   0);
    cmDspInst_t* cancel = cmDspSysAllocButton(h, "cancel", 0);
    cmDspInst_t* write  = cmDspSysAllocButton(h, "write",  0);
    cmDspInst_t* print  = cmDspSysAllocButton(h, "print",  0);
    cmDspInst_t* chMenu = cmDspSysAllocMsgList(h, chCfgPath, "ch_array", 0 );
    cmDspInst_t* onThr  = cmDspSysAllocScalar(h,"On  Thresh",-100.0,0.0,0.1,-45.0);
    cmDspInst_t* offThr = cmDspSysAllocScalar(h,"Off Thresh",-100.0,0.0,0.1,-80.0);
    //cmDspInst_t* prt    = cmDspSysAllocInst(h,"Printer",NULL,1,"ag>");
    cmDspInst_t* sub    = cmDspSysAllocInst(h,"ScalarOp",NULL,6,2,"+","in-0",0.0,"in-1",-1.0);

    cmDspSysNewColumn(h,200);
    cmDspSysAllocLabel(h,"EF Gate",kLeftAlignDuiId);

    // allocate the envelope followers and meters
    for(i=0; i<chCnt; ++i )
    {
      snprintf(label0,labelCharCnt,"%2i",i);
      ef[i]   = cmDspSysAllocInst( h, "EnvFollow", NULL, 0 );
      mtr[i]  = cmDspSysAllocInst( h, "Meter",label0, 3, 0.0, 0.0, 1.0 );
    }

    cmDspSysNewColumn(h,200);
    cmDspSysAllocLabel(h,"Audio",kLeftAlignDuiId);

    // allocate the envelope followers and meters
    for(i=0; i<chCnt; ++i )
    {
      amtr[i]  = cmDspSysAllocInst( h, "AMeter", NULL, 0 );
    }

    // chord detector, note selector, mix0, mix1, mix2
    ///cmDspInst_t* cdp = cmDspSysAllocInst(h, "ChordDetect", NULL, 1, "cdSel" );
    ///cmDspInst_t* nsp = cmDspSysAllocInst(h, "NoteSelect",  NULL, 1, chCnt );

    // onset count display
    cmDspSysNewColumn(h,150);
    cmDspSysAllocLabel(h,"Onsets",kLeftAlignDuiId);
    for(i=0; i<chCnt; ++i)
      on[i] = cmDspSysAllocScalar(h,NULL,0,1,0,0);
    
    // offset count display
    cmDspSysNewColumn(h,150);
    cmDspSysAllocLabel(h,"Offsets",kLeftAlignDuiId);
    for(i=0; i<chCnt; ++i)
      off[i] = cmDspSysAllocScalar(h,NULL,0,1,0,0);

    // offset count display
    cmDspSysNewColumn(h,150);
    cmDspSysAllocLabel(h,"RMS",kLeftAlignDuiId);
    for(i=0; i<chCnt; ++i)
      rms[i] = cmDspSysAllocScalar(h,NULL,0,1,0,0);

    /*
    // note select gate meters
    for(j=0; j<3; ++j)
    {
      snprintf(label0,labelCharCnt,"Set %i",j);
      cmDspSysNewColumn(h,50);
      cmDspSysAllocLabel(h,label0,kLeftAlignDuiId );

      
      for(i=0; i<chCnt; ++i)
      {
        cmDspInst_t* m = cmDspSysAllocInst(h, "Meter", NULL, 3, 0.0, 0.0, 1.0 );
        switch(j)
        {
          case 0: fdr0[i] = m; break;
          case 1: fdr1[i] = m; break;
          case 2: fdr2[i] = m; break;
            
        }
      }
    }
    
    // chord detector parameters
    cmDspSysNewColumn(h,150);
    cmDspSysAllocLabel(h,"Chord Detector",kLeftAlignDuiId);
    cmDspInst_t* cdSpanMs  = cmDspSysAllocScalar(h,"Span Ms", 10.0,1000.0,1.0,cdMaxTimeSpanMs);
    cmDspInst_t* cdNoteCnt = cmDspSysAllocScalar(h,"Note Cnt", 1.0, 100.0,1.0,cdMinNoteCnt );
    cmDspInst_t* cdCount   = cmDspSysAllocScalar(h,"Ch. Count",    0,1,0,0);

    */

    // allocate an audio mixer and two audio output channels
    cmDspInst_t* amix   = cmDspSysAllocInst( h, "AMix", NULL, 1, chCnt);
    cmDspInst_t* ao0    = cmDspSysAllocAudioOut(h,0,1.0);
    cmDspInst_t* ao1    = cmDspSysAllocAudioOut(h,1,1.0);

    // alloc chCfg last so that it's default outputs are applied to connected objects
    cmDspInst_t* chCfg  = cmDspSysAllocInst( h, "ChCfg",     NULL,     1, chCfgFn );

    if((rc = cmDspSysLastRC(h)) != kOkDspRC )
      goto errLabel;

    cmDspSysConnectAudioN11N(h,ain, "out", ag,   "in", chCnt); // ain -> auto gain
    cmDspSysConnectAudioN11N(h,ain, "out", amix, "in", chCnt); // ain -> amix
    cmDspSysConnectAudioN1N1(h,ain, "out", ef,   "in", chCnt); // ain -> EF
    cmDspSysConnectAudioN1N1(h, ain,  "out", amtr, "in",chCnt);

    cmDspSysInstallCb1N1N(h, ag,    "gain",  chCfg, "gain", chCnt ); // ag     -> chCfg (gain)
    cmDspSysInstallCb1NN1(h, chCfg, "gain",  ain,   "gain", chCnt ); // cgCfg  -> ain   (gain)
    cmDspSysInstallCb1NN1(h, chCfg, "ch",    ain,   "ch",   chCnt ); // cgCfg  -> ain   (ch)
    cmDspSysInstallCbN1N1(h, ef,    "gate",  mtr,   "in",   chCnt ); // EF gate -> meter

    cmDspSysInstallCb11N1(h, onThr,  "val",  ef,    "ondb",  chCnt ); // 
    cmDspSysInstallCb11N1(h, offThr, "val",  ef,    "offdb", chCnt ); //
    cmDspSysInstallCbN1N1(h, ef,     "ons",  on,    "val",   chCnt ); // EF -> onset count
    cmDspSysInstallCbN1N1(h, ef,     "offs", off,   "val",   chCnt ); // EF -> offset count
    cmDspSysInstallCbN1N1(h, ef,     "rms",  rms,   "val",   chCnt );

    ///cmDspSysInstallCbN11N(h, ef,     "gate", cdp,   "gate", chCnt ); // EF -> CD gate
    ///cmDspSysInstallCbN11N(h, ef,     "rms",  cdp,   "rms",  chCnt ); // EF -> CD rms

    ///cmDspSysInstallCb1N1N(h, cdp,    "gate", nsp,   "gate", chCnt ); // CD -> NS gate
    ///cmDspSysInstallCb1N1N(h, cdp,    "rms",  nsp,   "rms",  chCnt ); // CD -> NS rms
    ///cmDspSysInstallCb1NN1(h, nsp,    "gate-0",fdr0,  "in",   chCnt ); // NS -> Fader 0 gate
    ///cmDspSysInstallCb1NN1(h, nsp,    "gate-1",fdr1,  "in",   chCnt ); // NS -> Fader 1 gate
    ///cmDspSysInstallCb1NN1(h, nsp,    "gate-2",fdr2,  "in",   chCnt ); // NS -> Fader 2 gate   


    cmDspSysConnectAudio(h, amix, "out", ao0, "in");  // amix -> aout 0
    cmDspSysConnectAudio(h, amix, "out", ao1, "in");  // amix -> aout 1

    
    //cmDspSysInstallCb(h, chMenu, "ch",  ag,    "id", NULL );
    cmDspSysInstallCb(h, chMenu, "sel", sub, "in-0", NULL );
    cmDspSysInstallCb(h, sub,    "out", ag, "id", NULL );
    
    cmDspSysInstallCb(h, start,  "sym", ag,    "sel", NULL );
    cmDspSysInstallCb(h, proc,   "sym", ag,    "sel", NULL );
    cmDspSysInstallCb(h, cancel, "sym", ag,    "sel", NULL );
    cmDspSysInstallCb(h, print,  "sym", ag,    "sel", NULL );  
    cmDspSysInstallCb(h, print,  "sym", chCfg, "sel", NULL );
    cmDspSysInstallCb(h, write,  "sym", chCfg, "sel", NULL );

    /*
    cmDspSysInstallCb(h, cdSpanMs,  "val",    cdp,     "span",  NULL );
    cmDspSysInstallCb(h, cdNoteCnt, "val",    cdp,     "notes", NULL );
    cmDspSysInstallCb(h, cdp,       "count",  cdCount, "val",   NULL );
    cmDspSysInstallCb(h, cdp,       "detect", nsp,     "trig",  NULL );
    */
  }
 errLabel:

  cmFsFreeFn(chCfgPath);

  return rc;
  
}

cmDspRC_t _cmDspSysPgm_PickupFxFile( cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t       rc        = kOkDspRC;
  cmErr_t         err;
  cmCtx_t*        cmCtx     = cmDspSysPgmCtx(h);
  unsigned        chCnt     = 0;
  unsigned        nsChCnt   = 0;
  const cmChar_t* chCfgPath = NULL;
  const cmChar_t* chCfgFn   = "pick_chs8.js";
  double          cfMinHz   = 20.0;
  double          cfAlpha   = 0.9;
  bool            cfFbFl    = true;
  unsigned        sgShapeId = 2;
  const cmChar_t* afn       = "/home/kevin/media/audio/gate_detect/gate_detect0.aif";
  unsigned        abeg[]    = { 9.842046, 18.838291, 27.007957, 35.562079, 45.461793, 52.920218, 60.436312, 68.913543};
  unsigned        aend[]    = {11.399088, 20.645229, 28.891786, 37.311349, 47.287954, 54.131251, 62.473923, 72.142964};
  bool            cfBypassFl  = false;
  unsigned        i;
  

  cmErrSetup(&err,&cmCtx->rpt,"Pickup Effects");

  // prepend the prefs directory to the ch. cfg filename 
  chCfgPath = cmFsMakeFn(cmFsPrefsDir(),chCfgFn,NULL,NULL);

  // get the count of channels from the ch. cfg. array
  if(( chCnt = cmChCfgChannelCount(cmCtx,chCfgFn,&nsChCnt)) == 0 )
  {
    rc = cmErrMsg(&err,kPgmCfgFailDspRC,"Unable to obtain the channel count from '%s'.",cmStringNullGuard(chCfgFn));
    goto errLabel;
  }

  if( rc == kOkDspRC )
  {
    cmDspInst_t* af[chCnt];
    //cmDspInst_t* aout[chCnt];
    cmDspInst_t* ef[chCnt];
    cmDspInst_t* cf[chCnt];
    cmDspInst_t* sg[chCnt];
    cmDspInst_t* mtr[chCnt];
    cmDspInst_t* mute[chCnt];

    cmDspInst_t* phs =  cmDspSysAllocInst(h,"Phasor",   NULL,  0 );
    
    // allocate the audio inputs
    for(i=0; i<chCnt; ++i)
    {
      unsigned labelCharCnt = 31;
      cmChar_t label[labelCharCnt+1];
      snprintf(label,labelCharCnt,"%i",i);

      int sbeg = floor(abeg[i] * cmDspSysSampleRate(h));
      int send = floor(aend[i] * cmDspSysSampleRate(h));

      //ain[i]  = cmDspSysAllocAudioIn( h,i,1.0);
      af[i]   = cmDspSysAllocInst(h,"WaveTable",NULL,  6, ((int)cmDspSysSampleRate(h)), 1, afn, -1, sbeg, send );
      //aout[i] = cmDspSysAllocAudioOut(h,i,1.0);      
      ef[i]   = cmDspSysAllocInst(h, "EnvFollow", NULL, 0 );
      sg[i]   = cmDspSysAllocInst(h, "SigGen",    NULL, 2, 1000.0, sgShapeId );
      cf[i]   = cmDspSysAllocInst(h, "CombFilt",  NULL, 5, cfBypassFl, cfMinHz, cfFbFl, cfMinHz, cfAlpha );
      mtr[i]  = cmDspSysAllocInst( h, "Meter",label, 3, 0.0, 0.0, 1.0 );
    }

    // allocate the ch cfg last so that it's default outputs initialize connected objects
    cmDspInst_t* chCfg  = cmDspSysAllocInst( h, "ChCfg",     NULL,     1, chCfgFn );  
    cmDspInst_t* mix    = cmDspSysAllocInst( h, "AMix",     NULL,      1, chCnt );

    cmDspSysNewColumn(h,50);
    for(i=0; i<chCnt; ++i)
      mute[i] = cmDspSysAllocCheck(h,"",0);
    
    // checkk for allocation errors
    if((rc = cmDspSysLastRC(h)) != kOkDspRC )
      goto errLabel;

    // 
    //cmDspSysConnectAudioN1N1(h, ain,   "out",  aout,  "in",   chCnt );
    cmDspSysConnectAudio11N1(h, phs,   "out",  af,    "phs",  chCnt ); 
    cmDspSysConnectAudioN1N1(h, af,    "out",  ef,    "in",   chCnt ); // af  -> EF 
    cmDspSysConnectAudioN1N1(h, sg,    "out",  cf,    "in",   chCnt ); // sg  -> CF
    cmDspSysConnectAudioN11N(h, cf,    "out",  mix,   "in",   chCnt ); // CF  -> mix
    //cmDspSysInstallCb1NN1(   h, chCfg, "gain", ain,   "gain", chCnt ); // chCfg -> ain gain
    cmDspSysInstallCb1NN1(   h, chCfg, "hz",   cf,    "hz",   chCnt ); // chCfg -> CF Hz
    //cmDspSysInstallCbN1N1(   h, ef,    "rms",  aout,  "gain", chCnt ); // EF  -> aout gain
    cmDspSysInstallCbN1N1(   h, ef,    "rms",  mtr,   "in",   chCnt ); // EF  -> meter RMS
    cmDspSysInstallCbN11N(   h, ef,    "rms",  mix,   "gain", chCnt );
    cmDspSysInstallCbN11N(   h, mute,  "out",  mix,   "mute", chCnt ); // mute -> mix
  }

 errLabel:
  cmFsFreeFn(chCfgPath);

  return rc;
}

cmDspRC_t _cmDspSysPgm_NoiseTails( cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t       rc        = kOkDspRC;
  cmErr_t         err;
  cmCtx_t*        cmCtx     = cmDspSysPgmCtx(h);
  unsigned        chCnt     = 0;
  unsigned        nsChCnt   = 0;
  const cmChar_t* chCfgPath = NULL;
  const cmChar_t* chCfgFn   = "pick_chs8.js";
  double          cfMinHz   = 20.0;
  double          cfHz      = 500;
  double          cfAlpha   = 0.9;
  bool            cfFbFl    = true;
  bool            cfBypassFl= false;
  unsigned        sgShapeId = 3;
  double          dfltDelayMs = 100;
  unsigned        i;
  

  cmErrSetup(&err,&cmCtx->rpt,"Noise Tails");

  // prepend the prefs directory to the ch. cfg filename 
  chCfgPath = cmFsMakeFn(cmFsPrefsDir(),chCfgFn,NULL,NULL);

  // get the count of channels from the ch. cfg. array
  if(( chCnt = cmChCfgChannelCount(cmCtx,chCfgFn,&nsChCnt)) == 0 )
  {
    rc = cmErrMsg(&err,kPgmCfgFailDspRC,"Unable to obtain the channel count from '%s'.",cmStringNullGuard(chCfgFn));
    goto errLabel;
  }

  if( rc == kOkDspRC )
  {
    cmDspInst_t* ain[chCnt];
    cmDspInst_t* ef[chCnt];
    cmDspInst_t* cf[chCnt];
    cmDspInst_t* sg[chCnt];
    cmDspInst_t* mtr[chCnt];
    cmDspInst_t* add[chCnt];
    cmDspInst_t* mul[chCnt];
    cmDspInst_t* dly[chCnt];

    // allocate the audio inputs
    for(i=0; i<chCnt; ++i)
    {
      unsigned labelCharCnt = 31;
      cmChar_t label[labelCharCnt+1];
      snprintf(label,labelCharCnt,"%i",i);

      ain[i]  = cmDspSysAllocAudioIn(  h, i, 1.0);
      ef[i]   = cmDspSysAllocInst( h, "EnvFollow", NULL, 0 );
      sg[i]   = cmDspSysAllocInst( h, "SigGen",    NULL, 2, 1000.0, sgShapeId );
      cf[i]   = cmDspSysAllocInst( h, "CombFilt",  NULL, 5, cfBypassFl, cfMinHz, cfFbFl, cfHz, cfAlpha );
      mtr[i]  = cmDspSysAllocInst( h, "Meter",label, 3, 0.0, 0.0, 1.0 );
      add[i]  = cmDspSysAllocInst( h, "ScalarOp", NULL, 6, 2, "+", "in-0", 0.0, "in-1", 0.0);
      mul[i]  = cmDspSysAllocInst( h, "ScalarOp", NULL, 6, 2, "*", "in-0", 0.0, "in-1", 0.99);
      dly[i]  = cmDspSysAllocInst( h, "MsgDelay", NULL, 2, 1000, dfltDelayMs );
    }

    // allocate the ch cfg last so that it's default outputs initialize connected objects
    cmDspInst_t* chCfg  = cmDspSysAllocInst( h, "ChCfg",    NULL,     1, chCfgFn );  
    cmDspInst_t* mix    = cmDspSysAllocInst( h, "AMix",     NULL,     1, chCnt );
    cmDspInst_t* ao0    = cmDspSysAllocAudioOut( h, 0, 1.0 );
    cmDspInst_t* ao1    = cmDspSysAllocAudioOut( h, 1, 1.0 );
    cmDspInst_t* alpha  = cmDspSysAllocScalar(   h, "alpha", -1.0, 1.0, 0.001, cfAlpha );
    cmDspInst_t* decay  = cmDspSysAllocScalar(   h, "decay", -1.0, 1.0, 0.001, 0.5);
    cmDspInst_t* delay  = cmDspSysAllocScalar(   h, "delay", 0.0,  1000.0, 1.0, dfltDelayMs );
    cmDspInst_t* zero   = cmDspSysAllocScalar(   h, "zero",  0.0,  0.0, 0.0, 0.0);

    // check for allocation errors
    if((rc = cmDspSysLastRC(h)) != kOkDspRC )
      goto errLabel;


    cmDspSysConnectAudioN1N1( h, ain, "out",  ef,  "in",   chCnt ); // ain -> EF 
    cmDspSysConnectAudioN1N1( h, sg,  "out",  cf,  "in",   chCnt ); // sg  -> CF
    cmDspSysConnectAudioN11N( h, cf,  "out",  mix, "in",  chCnt );  // cf  -> mix
    cmDspSysConnectAudio(     h, mix, "out",  ao0, "in");           // mix -> aout L  
    cmDspSysConnectAudio(     h, mix, "out",  ao1, "in");           // mix -> aout R

    cmDspSysInstallCb1NN1(   h, chCfg, "gain", ain,  "gain", chCnt ); // chCfg -> ain gain
    cmDspSysInstallCb1NN1(   h, chCfg, "hz",   cf,   "hz",   chCnt );   // chCfg -> CF Hz
    cmDspSysInstallCbN1N1(   h, ef,    "rms",  add,  "in-0", chCnt );   // EF  -> mul 0
    cmDspSysInstallCbN1N1(   h, mul,   "out",  add,  "in-1", chCnt );   // mul -> add
    cmDspSysInstallCbN1N1(   h, add,   "out",  dly,  "in",   chCnt );   // add -> delay
    cmDspSysInstallCbN11N(   h, dly,   "out",  mix,  "gain", chCnt );   // delay -> mix gain
    cmDspSysInstallCbN1N1(   h, add,   "out",  mul,  "in-0", chCnt);    // add -> mul (feedback)
    cmDspSysInstallCb11N1(   h, decay, "val",  mul,  "in-1", chCnt );   // decay ctl
    cmDspSysInstallCbN1N1(   h, ef,    "gate", mtr,  "in",   chCnt );   // EF  -> meter RMS
    cmDspSysInstallCb11N1(   h, alpha, "val",  cf,   "alpha",chCnt );   // CF alpha
    cmDspSysInstallCb11N1(   h, delay, "val",  dly,  "delay",chCnt );   // Delay ctl
    cmDspSysInstallCb111N(   h, zero,  "val",  mix,  "in",   chCnt );  //
  }

 errLabel:
  cmFsFreeFn(chCfgPath);

  return rc;
}

cmDspRC_t _cmDspSysPgm_NoiseTails2( cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t       rc           = kOkDspRC;
  cmErr_t         err;
  cmCtx_t*        cmCtx        = cmDspSysPgmCtx(h);
  unsigned        chCnt        = 0;
  unsigned        nsChCnt      = 0;
  const cmChar_t* chCfgPath    = NULL;
  const cmChar_t* chCfgFn      = "pick_chs8.js";
  double          cfMinHz      = 20.0;

  double          cfHz         = 500;
  double          cfAlpha      = 0.9;
  bool            cfFbFl       = true;
  bool            cfBypassFl   = false;
  unsigned        sgShapeId    = 3;

  double          adsrMaxMs    = 10000;
  double          adsrMinMs    = 0;
  double          adsrIncMs    = 1;
  double          adsrMaxLevel = 100.0;
  double          adsrSusLevel = 100.0;
  double          adsrMinLevel =   0.0;
  double          adsrIncLevel = 0.001;

  bool            eqBypassFl = false;
  unsigned        eqModeSymId = cmSymTblRegisterStaticSymbol(cmDspSysSymbolTable(h),"LP");
  double          eqF0hz      = 250;
  double          eqQ         = 1.0;
  double          eqFgain     = 1.0;
  
  bool            mtBypassFl = false;
  double          mtTimeScale= 1.0;
  double          mtFeedback = 0.0;

  unsigned        i;
  

  cmErrSetup(&err,&cmCtx->rpt,"Noise Tails");

  // prepend the prefs directory to the ch. cfg filename 
  chCfgPath = cmFsMakeFn(cmFsPrefsDir(),chCfgFn,NULL,NULL);

  // get the count of channels from the ch. cfg. array
  if(( chCnt = cmChCfgChannelCount(cmCtx,chCfgFn,&nsChCnt)) == 0 )
  {
    rc = cmErrMsg(&err,kPgmCfgFailDspRC,"Unable to obtain the channel count from '%s'.",cmStringNullGuard(chCfgFn));
    goto errLabel;
  }

  if( rc == kOkDspRC )
  {
    cmDspInst_t* ain[chCnt];
    cmDspInst_t* ef[chCnt];
    cmDspInst_t* cf[chCnt];
    cmDspInst_t* sg[chCnt];
    cmDspInst_t* mtr[chCnt];
    cmDspInst_t* add[chCnt];
    cmDspInst_t* mul[chCnt];
    cmDspInst_t* env[chCnt];
    cmDspInst_t* d2l[chCnt];
    cmDspInst_t* eq[chCnt];
    cmDspInst_t* mt[chCnt];

    // allocate the audio inputs
    for(i=0; i<chCnt; ++i)
    {
      unsigned labelCharCnt = 31;
      cmChar_t label[labelCharCnt+1];
      snprintf(label,labelCharCnt,"%i",i);

      ain[i]  = cmDspSysAllocAudioIn(  h, i, 1.0);
      ef[i]   = cmDspSysAllocInst( h, "EnvFollow", NULL, 0 );
      sg[i]   = cmDspSysAllocInst( h, "SigGen",    NULL, 2, 1000.0, sgShapeId );
      cf[i]   = cmDspSysAllocInst( h, "CombFilt",  NULL, 5, cfBypassFl, cfMinHz, cfFbFl, cfHz, cfAlpha );
      mtr[i]  = cmDspSysAllocInst( h, "Meter",label, 3, 0.0, 0.0, 1.0 );
      env[i]  = cmDspSysAllocInst( h, "Adsr",     NULL, 2, true, adsrMinLevel );
      d2l[i]  = cmDspSysAllocInst( h, "DbToLin",  NULL, 0 );
      add[i]  = cmDspSysAllocInst( h, "ScalarOp", NULL, 6, 2, "+", "in-0", 0.0, "in-1", 0.0);
      mul[i]  = cmDspSysAllocInst( h, "ScalarOp", NULL, 6, 2, "*", "in-0", 0.0, "in-1", 0.99);
      eq[i]   = cmDspSysAllocInst( h, "BiQuadEq", NULL, 5, eqBypassFl, eqModeSymId, eqF0hz, eqQ, eqFgain ); 
      mt[i]   = cmDspSysAllocInst( h, "MtDelay",  NULL, 9, mtBypassFl, mtTimeScale, mtFeedback,   20.0, 0.8,   15.0, 0.9,    12.0, 0.9 );  
    }

    // allocate the ch cfg last so that it's default outputs initialize connected objects
    cmDspInst_t* chCfg  = cmDspSysAllocInst(     h, "ChCfg",    NULL,     1, chCfgFn );  
    cmDspInst_t* mix    = cmDspSysAllocInst(     h, "AMix",     NULL,     1, chCnt );
    cmDspInst_t* ao0    = cmDspSysAllocAudioOut( h, 0, 1.0 );
    cmDspInst_t* ao1    = cmDspSysAllocAudioOut( h, 1, 1.0 );
    cmDspInst_t* alpha  = cmDspSysAllocScalar(   h, "alpha", -1.0, 1.0, 0.001, cfAlpha );
    cmDspInst_t* decay  = cmDspSysAllocScalar(   h, "decay", -1.0, 1.0, 0.001, 0.5);
    cmDspInst_t* zero   = cmDspSysAllocScalar(   h, "zero",   0.0, 0.0, 0.0,   0.0);
    // cmDspInst_t* print  = cmDspSysAllocButton(   h, "print", 0 );

    cmDspSysNewColumn(h,200);
    cmDspInst_t* dly   = cmDspSysAllocScalar( h, "Dly Ms",  adsrMinMs,   adsrMaxMs,   adsrIncMs, 0.0);
    cmDspInst_t* atk   = cmDspSysAllocScalar( h, "Atk Ms",  adsrMinMs,   adsrMaxMs,   adsrIncMs, 2000.0);
    cmDspInst_t* dcy   = cmDspSysAllocScalar( h, "Dcy Ms",  adsrMinMs,   adsrMaxMs,   adsrIncMs, 100.0);
    cmDspInst_t* hold  = cmDspSysAllocScalar( h, "Hold Ms", adsrMinMs,   adsrMaxMs,   adsrIncMs, 100.0);
    cmDspInst_t* rls   = cmDspSysAllocScalar( h, "Rls Ms",  adsrMinMs,   adsrMaxMs,   adsrIncMs, 4000.0);
    cmDspInst_t* alvl  = cmDspSysAllocScalar( h, "AdsrMax", adsrMinLevel,adsrMaxLevel,adsrIncLevel, adsrMaxLevel);  
    cmDspInst_t* sus   = cmDspSysAllocScalar( h, "Sustain", adsrMinLevel,adsrMaxLevel,adsrIncLevel, adsrSusLevel );  

    cmDspSysNewColumn(h,200);
    cmDspInst_t* onThr  = cmDspSysAllocScalar(h,"On  Thresh",-100.0,0.0,0.1,-55.0);
    cmDspInst_t* offThr = cmDspSysAllocScalar(h,"Off Thresh",-100.0,0.0,0.1,-80.0);

    cmDspInst_t* eqbyp   = cmDspSysAllocCheck(  h, "Eq Byp", 0 );
    cmDspInst_t* eqmode  = cmDspSysAllocInst(   h, "MsgList","Mode", 1, "biQuadEqMode");
    cmDspInst_t* eqq     = cmDspSysAllocScalar( h, "Q",          -100.0, 100.0, 0.1, eqQ);
    cmDspInst_t* eqfgn   = cmDspSysAllocScalar( h, "Filt Gain",   0.0,     1.0, 0.1, eqFgain);

    cmDspInst_t* mtfb   = cmDspSysAllocScalar( h, "Mt Feedback",   0.0,     1.0, 0.01, mtFeedback);
    cmDspInst_t* mtscale= cmDspSysAllocScalar( h, "Mt Time Scale",   0.01,   10.0, 0.01, mtTimeScale);
 
    // check for allocation errors
    if((rc = cmDspSysLastRC(h)) != kOkDspRC )
      goto errLabel;


    cmDspSysConnectAudioN1N1( h, ain, "out",  ef,  "in",   chCnt ); // ain -> EF 
    cmDspSysConnectAudioN1N1( h, sg,  "out",  cf,  "in",   chCnt ); // sg  -> CF
    cmDspSysConnectAudioN1N1( h, cf,  "out",  eq,  "in",   chCnt );
    cmDspSysConnectAudioN1N1( h, eq,  "out",  mt,  "in",   chCnt );
    cmDspSysConnectAudioN11N( h, mt,  "out",  mix, "in",  chCnt );  // cf  -> mix
    cmDspSysConnectAudio(     h, mix, "out",  ao0, "in");           // mix -> aout L  
    cmDspSysConnectAudio(     h, mix, "out",  ao1, "in");           // mix -> aout R

    cmDspSysInstallCb1NN1(   h, chCfg, "gain", ain,  "gain", chCnt );   // chCfg -> ain gain
    cmDspSysInstallCb1NN1(   h, chCfg, "hz",   cf,   "hz",   chCnt );   // chCfg -> CF Hz
    cmDspSysInstallCb1NN1(   h, chCfg, "hz",   eq,   "f0",   chCnt );   // chCfg -> Eq Hz
    cmDspSysInstallCbN1N1(   h, mul,   "out",  add,  "in-1", chCnt );   // mul -> add
    cmDspSysInstallCbN1N1(   h, ef,    "gate", env,  "gate", chCnt );   // EF  -> adsr  (gate)
    cmDspSysInstallCbN1N1(   h, ef,    "rms",  env,  "rms",  chCnt );   // EF ->adsr (rms)
    //cmDspSysInstallCb11N1(   h, print,   "out",  env,  "cmd",  chCnt );
    cmDspSysInstallCbN1N1(   h, env,   "out",  d2l,  "in", chCnt );
    cmDspSysInstallCbN11N(   h, d2l,   "out",  mix,  "gain", chCnt ); 
    cmDspSysInstallCbN1N1(   h, add,   "out",  mul,  "in-0", chCnt );   // add -> mul (feedback)
    cmDspSysInstallCb11N1(   h, decay, "val",  mul,  "in-1", chCnt );   // decay ctl
    cmDspSysInstallCbN1N1(   h, ef,    "gate", mtr,  "in",   chCnt );   // EF  -> meter RMS
    cmDspSysInstallCb11N1(   h, alpha, "val",  cf,   "alpha",chCnt );   // CF alpha
    cmDspSysInstallCb111N(   h, zero,  "val",  mix,  "in",   chCnt );  //

    cmDspSysInstallCb11N1( h, dly,  "val", env, "dly",  chCnt );
    cmDspSysInstallCb11N1( h, atk,  "val", env, "atk",  chCnt );
    cmDspSysInstallCb11N1( h, dcy,  "val", env, "dcy",  chCnt );
    cmDspSysInstallCb11N1( h, hold, "val", env, "hold", chCnt );
    cmDspSysInstallCb11N1( h, rls,  "val", env, "rls",  chCnt );
    cmDspSysInstallCb11N1( h, alvl, "val", env, "alvl", chCnt );
    cmDspSysInstallCb11N1( h, sus,  "val", env, "sus",  chCnt );

    cmDspSysInstallCb11N1(   h, onThr,  "val",  ef,    "ondb",  chCnt ); // 
    cmDspSysInstallCb11N1(   h, offThr, "val",  ef,    "offdb", chCnt ); //

    cmDspSysInstallCb11N1(   h, eqbyp,  "out",  eq,    "bypass", chCnt );
    cmDspSysInstallCb11N1(   h, eqmode, "mode", eq,    "mode",   chCnt );
    cmDspSysInstallCb11N1(   h, eqq,    "val",  eq,    "Q",      chCnt );
    cmDspSysInstallCb11N1(   h, eqfgn,   "val",  eq,    "gain",   chCnt );

    cmDspSysInstallCb11N1(   h, mtfb,    "val",  mt,    "fb",      chCnt );
    cmDspSysInstallCb11N1(   h, mtscale, "val",  mt,    "scale",   chCnt );

  }

 errLabel:
  cmFsFreeFn(chCfgPath);

  return rc;
}



cmDspRC_t _cmDspSysPgm_CombFilt( cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t rc        = kOkDspRC;
  double    cfMinHz   = 20.0;
  double    cfAlpha   = 0.9;
  bool      cfFbFl    = true;
  bool      cfBypassFl= false;
  unsigned  sgShapeId = 2;

  cmDspInst_t* ao  = cmDspSysAllocAudioOut( h, 0, 1.0 );
  cmDspInst_t* sg  = cmDspSysAllocInst(   h, "SigGen",    NULL, 2, 1000.0, sgShapeId );
  cmDspInst_t* cf  = cmDspSysAllocInst(   h, "CombFilt",  NULL, 5, cfBypassFl, cfMinHz, cfFbFl, cfMinHz, cfAlpha );
  cmDspInst_t* hz  = cmDspSysAllocScalar( h, "Hz", 25, 10000, 1, 1000 );
  cmDspInst_t* a   = cmDspSysAllocScalar( h, "Alpha", 0.0, 2.0, 0.001, cfAlpha);

  // check for allocation errors
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    goto errLabel;

  cmDspSysConnectAudio(h, sg, "out", cf, "in");
  cmDspSysConnectAudio(h, cf, "out", ao, "in");
  cmDspSysInstallCb(   h, hz, "val", cf, "hz", NULL);
  cmDspSysInstallCb(   h, a,  "val", cf, "alpha", NULL);
 errLabel:
  return rc;  
}

cmDspRC_t _cmDspSysPgm_ScalarOp( cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t rc;

  cmDspInst_t* add   = cmDspSysAllocInst(   h, "ScalarOp", NULL,  6, 2, "+", "in-0", 0.0, "in-1", 0.0 );
  cmDspInst_t* mul0  = cmDspSysAllocInst(   h, "ScalarOp", NULL,  6, 2, "*", "in-0", 0.0, "in-1", 0.0 );
  cmDspInst_t* mul1  = cmDspSysAllocInst(   h, "ScalarOp", NULL,  6, 2, "*", "in-0", 0.0, "in-1", 0.0 );
  cmDspInst_t* in    = cmDspSysAllocScalar( h, "Input",      0.0, 10.0, 0.001, 0.0);
  cmDspInst_t* in_m  = cmDspSysAllocScalar( h, "Input_M",    0.0, 10.0, 0.001, 0.0);
  cmDspInst_t* fb    = cmDspSysAllocScalar( h, "Feedback",   0.0, 10.0, 0.001, 0.0);
  cmDspInst_t* fb_m  = cmDspSysAllocScalar( h, "Feedback_M", 0.0, 10.0, 0.001, 0.0);
  cmDspInst_t* out   = cmDspSysAllocScalar( h, "Out",        0.0, 10.0, 0.001, 0.0);

    // check for allocation errors
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    goto errLabel;

  cmDspSysInstallCb( h, in,    "val", mul0, "in-0", NULL );
  cmDspSysInstallCb( h, in_m,  "val", mul0, "in-1", NULL );
  cmDspSysInstallCb( h, fb,    "val", mul1, "in-0", NULL );
  cmDspSysInstallCb( h, fb_m,  "val", mul1, "in-1", NULL );
  cmDspSysInstallCb( h, mul0,  "out", add,  "in-0", NULL );
  cmDspSysInstallCb( h, mul1,  "out", add,  "in-1", NULL );
  cmDspSysInstallCb( h, add,   "out", fb,   "val",  NULL );
  cmDspSysInstallCb( h, add,   "out", out,  "val",  NULL );

    errLabel:
  return rc;
}


cmDspRC_t _cmDspSysPgm_RingMod( cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t       rc          = kOkDspRC;
  cmErr_t         err;
  cmCtx_t*        cmCtx       = cmDspSysPgmCtx(h);
  unsigned        chCnt       = 0;
  unsigned        nsChCnt     = 0;
  const cmChar_t* chCfgPath   = NULL;
  const cmChar_t* chCfgFn     = "pick_chs8.js";
  unsigned        groupCnt    = 3;
  unsigned        chsPerGroup = 2;
  cmReal_t        fadeTimeMs  = 25;
  unsigned        i,j,k;
  

  cmErrSetup(&err,&cmCtx->rpt,"Pickup Effects");

  // prepend the prefs directory to the ch. cfg filename 
  chCfgPath = cmFsMakeFn(cmFsPrefsDir(),chCfgFn,NULL,NULL);

  // get the count of channels from the ch. cfg. array
  if(( chCnt = cmChCfgChannelCount(cmCtx,chCfgFn,&nsChCnt)) == 0 )
  {
    rc = cmErrMsg(&err,kPgmCfgFailDspRC,"Unable to obtain the channel count from '%s'.",cmStringNullGuard(chCfgFn));
    goto errLabel;
  }

  if( rc == kOkDspRC )
  {
    cmDspInst_t* ain[chCnt];
    cmDspInst_t* ef[chCnt];
    cmDspInst_t* mtr[chCnt];
    cmDspInst_t* nom[groupCnt];
    cmDspInst_t* rm[groupCnt];
    cmDspInst_t* nm_mtr[groupCnt*chsPerGroup];
    unsigned labelCharCnt = 31;
    cmChar_t label[labelCharCnt+1];

    // allocate the audio inputs and envelope followers
    for(i=0; i<chCnt; ++i)
    {
      snprintf(label,labelCharCnt,"%i",i);

      ain[i]  = cmDspSysAllocAudioIn(  h, i, 1.0);
      ef[i]   = cmDspSysAllocInst( h, "EnvFollow", NULL, 0 );
      mtr[i]  = cmDspSysAllocInst( h, "Meter",     label, 3, 0.0, 0.0, 1.0 );
    }
    
    cmDspInst_t* gs = cmDspSysAllocInst( h, "GroupSel", NULL, 3, chCnt, groupCnt, chsPerGroup );

    for(i=0; i<groupCnt; ++i)
    {
      nom[i] = cmDspSysAllocInst(h, "AudioNofM", NULL, 3, chCnt, chsPerGroup, fadeTimeMs );
      rm[i]  = cmDspSysAllocInst(h, "RingMod",   NULL, 1, chsPerGroup );
    }

    for(i=0,k=0; i<groupCnt; ++i)
    {
      cmDspSysNewColumn(h,50);
      snprintf(label,labelCharCnt,"%i",i);
      cmDspSysAllocLabel(h,label,kLeftAlignDuiId );

      for(j=0; j<chsPerGroup; ++j,++k)
      {        
        snprintf(label,labelCharCnt,"%i",j);
        nm_mtr[k] = cmDspSysAllocInst(h, "Meter", label, 3, 0.0, 0.0, 1.0 );
      } 
    }
    assert(k==groupCnt*chsPerGroup);

    // allocate the ch cfg last so that it's default outputs initialize connected objects
    //cmDspInst_t* chCfg  = cmDspSysAllocInst( h, "ChCfg",    NULL,     1, chCfgFn );  
    cmDspInst_t* mix    = cmDspSysAllocInst( h, "AMix",     NULL,     1, groupCnt );
    cmDspInst_t* ao0    = cmDspSysAllocAudioOut( h, 0, 1.0 );
    cmDspInst_t* ao1    = cmDspSysAllocAudioOut( h, 1, 1.0 );

    // check for allocation errors
    if((rc = cmDspSysLastRC(h)) != kOkDspRC )
      goto errLabel;

    cmDspSysConnectAudioN1N1( h, ain, "out",  ef,  "in",   chCnt ); // ain -> EF 

    for(i=0; i<groupCnt; ++i)
    {
      cmDspSysConnectAudioN11N( h, ain,    "out", nom[i], "in", chCnt );
      cmDspSysConnectAudio1N1N( h, nom[i], "out", rm[i],  "in", chsPerGroup);

      snprintf(label,labelCharCnt,"gate-%i",i);
      cmDspSysInstallCb1N1N( h, gs, label, nom[i], "gate", chCnt );

      cmDspSysInstallCb1NN1( h, nom[i], "gain", nm_mtr + i * chsPerGroup, "in", chsPerGroup);
    }


    cmDspSysConnectAudioN11N( h, rm,  "out",  mix, "in", groupCnt );
    cmDspSysConnectAudio(     h, mix, "out",  ao0, "in");           // mix -> aout L  
    cmDspSysConnectAudio(     h, mix, "out",  ao1, "in");           // mix -> aout R

    cmDspSysInstallCbN11N(   h, ef, "gate",  gs,   "gate",   chCnt );   // EF -> grp_sel gate
    cmDspSysInstallCbN11N(   h, ef, "rms",   gs,   "rms",    chCnt );   // EF -> grp_sel RMS
    cmDspSysInstallCbN1N1(   h, ef, "gate",  mtr,  "in",     chCnt );
  }

 errLabel:
  cmFsFreeFn(chCfgPath);

  return rc;
}

cmDspRC_t _cmDspSysPgm_RingMod2( cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t       rc          = kOkDspRC;
  cmErr_t         err;
  cmCtx_t*        cmCtx       = cmDspSysPgmCtx(h);
  unsigned        iChCnt       = 0;
  unsigned        oChCnt       = 0;
  unsigned        nsChCnt      = 0;
  const cmChar_t* chCfgPath   = NULL;
  const cmChar_t* chCfgFn     = "pick_chs8.js";
  unsigned        i;
  
  cmErrSetup(&err,&cmCtx->rpt,"Pickup Effects");

  // prepend the prefs directory to the ch. cfg filename 
  chCfgPath = cmFsMakeFn(cmFsPrefsDir(),chCfgFn,NULL,NULL);

  // get the count of channels from the ch. cfg. array
  if(( iChCnt = cmChCfgChannelCount(cmCtx,chCfgFn,&nsChCnt)) == 0 )
  {
    rc = cmErrMsg(&err,kPgmCfgFailDspRC,"Unable to obtain the channel count from '%s'.",cmStringNullGuard(chCfgFn));
    goto errLabel;
  }

  if( rc == kOkDspRC && iChCnt > 0 )
  {
    if( iChCnt % 2 )
      iChCnt -= 1;

    oChCnt = iChCnt/2;

    cmDspInst_t* ain[iChCnt];
    cmDspInst_t* ef[iChCnt];
    cmDspInst_t* mtr[iChCnt];
    cmDspInst_t* rm[oChCnt];
    unsigned     labelCharCnt = 31;
    cmChar_t     label[labelCharCnt+1];

    // allocate the audio inputs and envelope followers
    for(i=0; i<iChCnt; ++i)
    {
      snprintf(label,labelCharCnt,"%i",i);

      ain[i]  = cmDspSysAllocAudioIn(  h, i, 1.0);
      ef[i]   = cmDspSysAllocInst( h, "EnvFollow", NULL, 0 );
      mtr[i]  = cmDspSysAllocInst( h, "Meter",     label, 3, 0.0, 0.0, 1.0 );
    }

    for(i=0; i<oChCnt; ++i)
    {
      rm[i]   = cmDspSysAllocInst(   h, "RingMod", NULL, 1, 2 );
    }

    cmDspInst_t* gain   = cmDspSysAllocScalar( h, "RM Gain", 0.0, 10.0, 0.001, 1.0);
    cmDspInst_t* mix    = cmDspSysAllocInst( h, "AMix",     NULL,     1, oChCnt );
    cmDspInst_t* ao0    = cmDspSysAllocAudioOut( h, 0, 1.0 );
    cmDspInst_t* ao1    = cmDspSysAllocAudioOut( h, 1, 1.0 );

    // check for allocation errors
    if((rc = cmDspSysLastRC(h)) != kOkDspRC )
      goto errLabel;

    cmDspSysConnectAudioN1N1( h, ain, "out",  ef,  "in",   iChCnt ); // ain -> EF 
    cmDspSysInstallCbN1N1(    h, ef,  "gate", mtr, "in",   iChCnt ); // EF -> mtr (gate)
    
    for(i=0; i<oChCnt; ++i)
    {
      cmDspSysConnectAudio( h, ain[i*2+0], "out", rm[i], "in-0");   // ain -> rm 0
      cmDspSysConnectAudio( h, ain[i*2+1], "out", rm[i], "in-1");   // ain -> rm 1

      snprintf(label,labelCharCnt,"in-%i",i);              
      cmDspSysConnectAudio( h, rm[i],      "out", mix,   label);   // rm -> mix

      cmDspSysInstallCb(h, gain, "val", rm[i], "gain", NULL );     // gain -> rm gain
    }


    cmDspSysConnectAudio(     h, mix, "out",  ao0, "in");           // mix -> aout L  
    cmDspSysConnectAudio(     h, mix, "out",  ao1, "in");           // mix -> aout R


  }

 errLabel:
  cmFsFreeFn(chCfgPath);

  return rc;
}

cmDspRC_t _cmDspSysPgm_MsgDelay( cmDspSysH_t h, void** userPtrPtr )
{

  cmDspRC_t rc              = kOkDspRC;
  double    dfltDelayTimeMs = 100.0;
  double    maxDelayTimeMs  = 10000.0;

  cmDspInst_t* ctl   = cmDspSysAllocScalar( h, "Delay", 0.0, maxDelayTimeMs, 1.0, dfltDelayTimeMs );
  cmDspInst_t* dly   = cmDspSysAllocInst(   h, "MsgDelay", NULL, 2, 1000, dfltDelayTimeMs );
  cmDspInst_t* print = cmDspSysAllocInst(   h, "Printer",  NULL, 1, ">");

  if( (rc = cmDspSysLastRC(h)) != kOkDspRC )
    return rc;

  cmDspSysInstallCb( h, ctl, "val",  dly,   "delay", NULL );
  cmDspSysInstallCb( h, ctl, "val",  dly,   "in",    NULL );
  cmDspSysInstallCb( h, dly,  "out", print, "in",    NULL );

  return rc;
}


cmDspRC_t _cmDspSysPgm_Adsr( cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t rc;
  bool   trigModeFl = true;
  double adsrMaxMs = 5000;
  double adsrMinMs = 0;
  double adsrIncMs = 1;
  double adsrMaxLevel = 100.0; //1.0;
  double adsrSusLevel =  80.0; //0.8;
  double adsrMinLevel =  0.0; //0.0;

  double adsrIncLevel = 0.001;
  const cmChar_t* fn = "/home/kevin/temp/adsr1.bin";

  cmDspInst_t* adsr = cmDspSysAllocInst(h, "Adsr", NULL, 2, trigModeFl, adsrMinLevel );
  cmDspInst_t* chck = cmDspSysAllocCheck(h,"Gate",0);
  cmDspInst_t* mtr   = cmDspSysAllocInst(h,"Meter","Out", 3, adsrMinLevel, adsrMinLevel, adsrMaxLevel );
  cmDspInst_t* bmf   = cmDspSysAllocInst(h,"BinMtxFile", NULL, 2, 1, fn );

  cmDspInst_t* dly   = cmDspSysAllocScalar( h, "Dly Ms",  adsrMinMs,   adsrMaxMs,   adsrIncMs, 0.0);
  cmDspInst_t* atk   = cmDspSysAllocScalar( h, "Atk Ms",  adsrMinMs,   adsrMaxMs,   adsrIncMs, 1000.0);
  cmDspInst_t* dcy   = cmDspSysAllocScalar( h, "Dcy Ms",  adsrMinMs,   adsrMaxMs,   adsrIncMs, 300.0);
  cmDspInst_t* hold  = cmDspSysAllocScalar( h, "Hold Ms", adsrMinMs,   adsrMaxMs,   adsrIncMs, 500.0);
  cmDspInst_t* rls   = cmDspSysAllocScalar( h, "Rls Ms",  adsrMinMs,   adsrMaxMs,   adsrIncMs, 1000.0);
  cmDspInst_t* alvl  = cmDspSysAllocScalar( h, "AdsrMax", adsrMinLevel,adsrMaxLevel,adsrIncLevel, adsrMaxLevel);  
  cmDspInst_t* sus   = cmDspSysAllocScalar( h, "Sustain", adsrMinLevel,adsrMaxLevel,adsrIncLevel, adsrSusLevel);  

    // check for allocation errors
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    goto errLabel;

  cmDspSysInstallCb( h, dly,  "val", adsr, "dly",  NULL );
  cmDspSysInstallCb( h, atk,  "val", adsr, "atk",  NULL );
  cmDspSysInstallCb( h, dcy,  "val", adsr, "dcy",  NULL );
  cmDspSysInstallCb( h, hold, "val", adsr, "hold",  NULL );
  cmDspSysInstallCb( h, rls,  "val", adsr, "rls",  NULL );
  cmDspSysInstallCb( h, alvl, "val", adsr, "alvl", NULL );
  cmDspSysInstallCb( h, sus,  "val", adsr, "sus",  NULL );

  cmDspSysInstallCb( h, chck, "out", adsr, "gate", NULL );
  cmDspSysInstallCb( h, adsr, "out", mtr, "in",   NULL );
  cmDspSysInstallCb( h, adsr, "out", bmf,  "in", NULL );

 errLabel:
  return rc;
  
}


cmDspRC_t _cmDspSysPgm_Compressor( cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t rc;
  const cmChar_t* ofn       = "/home/kevin/temp/adsr0.bin";
  const char*     afn0      = "media/audio/20110723-Kriesberg/Audio Files/Piano 3_01.wav";
  int             beg       = 6900826;
  int             end       = 13512262;
  const cmChar_t* afn       = cmFsMakeFn(cmFsUserDir(),afn0,NULL,NULL );
  bool            bypassFl  = false;
  double          inGain    = 1.0;
  double          threshDb  = -40.0;
  double          ratio_num = 4.0;
  double          atkMs     = 100.0;
  double          rlsMs     = 100.0;
  double          makeup    = 1.0;
  double          wndMaxMs  = 1000.0;
  double          wndMs     = 200.0;

  cmDspInst_t* off =  cmDspSysAllocInst(h,"Scalar", "Offset",  5, kNumberDuiId, 0.0,  cmDspSysSampleRate(h)*600.0, 1.0,  0.0);
  cmDspInst_t* phs =  cmDspSysAllocInst(h,"Phasor",   NULL,  0 );
  cmDspInst_t* wt  =  cmDspSysAllocInst(h,"WaveTable",NULL,  6, ((int)cmDspSysSampleRate(h)), 1, afn, -1, beg, end );
  cmDspInst_t* cmp =  cmDspSysAllocInst(h,"Compressor",NULL, 8, bypassFl, threshDb, ratio_num, atkMs, rlsMs, makeup, wndMs, wndMaxMs ); 

  cmDspInst_t* ao0   = cmDspSysAllocInst(h,"AudioOut",NULL,   1, 0 );
  cmDspInst_t* ao1   = cmDspSysAllocInst(h,"AudioOut",NULL,   1, 1 );
  cmDspInst_t* bmf   = cmDspSysAllocInst(h,"BinMtxFile", NULL, 2, 1, ofn );
  cmDspInst_t* mtr   = cmDspSysAllocInst(h,"Meter","Env", 3, 0.0, 0.0, 1.0);

  cmDspInst_t* igain = cmDspSysAllocScalar( h, "In Gain",  0.0,   10.0, 0.1, inGain);
  cmDspInst_t* thr   = cmDspSysAllocScalar( h, "ThreshDb", -100.0, 0.0, 0.1, threshDb);
  cmDspInst_t* rat   = cmDspSysAllocScalar( h, "Ratio",    0.1, 100, 0.1, ratio_num);
  cmDspInst_t* atk   = cmDspSysAllocScalar( h, "Atk Ms",   0.0, 1000.0, 0.1, atkMs);
  cmDspInst_t* rls   = cmDspSysAllocScalar( h, "Rls Ms",   0.0, 1000.0, 0.1, rlsMs);
  cmDspInst_t* mkup  = cmDspSysAllocScalar( h, "Makeup",   0.0, 10.0,   0.01, makeup);
  cmDspInst_t* wnd   = cmDspSysAllocScalar( h, "Wnd Ms",   1.0, wndMaxMs, 1.0, wndMs );

    // check for allocation errors
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    goto errLabel;

  cmDspSysConnectAudio(h, phs, "out", wt,  "phs" );  // phasor -> wave table
  cmDspSysConnectAudio(h, wt,  "out", cmp, "in" );   // wave table -> cmp in
  cmDspSysConnectAudio(h, cmp, "out", ao0, "in" );   // comp -> aout
  cmDspSysConnectAudio(h, cmp, "out", ao1, "in" );   // 


  cmDspSysInstallCb(h, off,  "val", wt, "beg", NULL ); 
  cmDspSysInstallCb(h, igain,"val", cmp, "igain", NULL );
  cmDspSysInstallCb(h, thr,  "val", cmp, "thr", NULL );
  cmDspSysInstallCb(h, rat,  "val", cmp, "ratio", NULL );
  cmDspSysInstallCb(h, atk,  "val", cmp, "atk", NULL );
  cmDspSysInstallCb(h, rls,  "val", cmp, "rls", NULL );
  cmDspSysInstallCb(h, mkup, "val", cmp, "ogain", NULL );
  cmDspSysInstallCb(h, wnd,  "val", cmp, "wnd", NULL );
  cmDspSysInstallCb(h, cmp,  "env", bmf, "in", NULL );
  cmDspSysInstallCb(h, cmp,  "env", mtr, "in", NULL );
 errLabel:
  return rc;
  
}


cmDspRC_t _cmDspSysPgm_BiQuadEq( cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t       rc        = kOkDspRC;
  const char*     afn0      = "media/audio/20110723-Kriesberg/Audio Files/Piano 3_01.wav";
  int             beg       = 6900826;
  int             end       = 13512262;
  const cmChar_t* afn       = cmFsMakeFn(cmFsUserDir(),afn0,NULL,NULL );
  bool            bypassFl  = false;
  unsigned        modeSymId = cmSymTblRegisterStaticSymbol(cmDspSysSymbolTable(h),"LP");
  double          f0hz      = 264.0;
  double          Q         = 1.0;
  double          fgain     = 1.0;

  cmDspInst_t* off =  cmDspSysAllocInst(h,"Scalar", "Offset",  5, kNumberDuiId, 0.0,  cmDspSysSampleRate(h)*600.0, 1.0,  0.0);
  cmDspInst_t* phs =  cmDspSysAllocInst(h,"Phasor",   NULL,  0 );
  cmDspInst_t* wt  =  cmDspSysAllocInst(h,"WaveTable",NULL,  6, ((int)cmDspSysSampleRate(h)), 1, afn, -1, beg, end );
  cmDspInst_t* flt =  cmDspSysAllocInst(h,"BiQuadEq",NULL,   5, bypassFl, modeSymId,f0hz, Q, fgain  ); 

  cmDspInst_t* ao0   = cmDspSysAllocInst(h,"AudioOut",NULL,   1, 0 );
  cmDspInst_t* ao1   = cmDspSysAllocInst(h,"AudioOut",NULL,   1, 1 );

  cmDspInst_t* mode  = cmDspSysAllocInst(   h, "MsgList","Mode", 1, "biQuadEqMode");
  cmDspInst_t* fhz   = cmDspSysAllocScalar( h, "Fc Hz",       0.0, 15000.0, 0.1, f0hz);
  cmDspInst_t* q     = cmDspSysAllocScalar( h, "Q",          -100.0, 100, 0.1,   Q);
  cmDspInst_t* fgn   = cmDspSysAllocScalar( h, "Filt Gain",   0.0, 1.0, 0.1, fgain);

    // check for allocation errors
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    goto errLabel;

  cmDspSysConnectAudio(h, phs, "out", wt,  "phs" );  // phasor -> wave table
  cmDspSysConnectAudio(h, wt,  "out", flt, "in" );   // wave table -> cmp in
  cmDspSysConnectAudio(h, flt, "out", ao0, "in" );   // filter -> aout
  cmDspSysConnectAudio(h, flt, "out", ao1, "in" );   // 


  cmDspSysInstallCb(h, off, "val", wt,  "beg", NULL ); 
  cmDspSysInstallCb(h, mode,"mode",flt, "mode", NULL );
  cmDspSysInstallCb(h, fhz, "val", flt, "f0", NULL );
  cmDspSysInstallCb(h, q,   "val", flt, "Q", NULL );
  cmDspSysInstallCb(h, fgn, "val", flt, "gain", NULL );

 errLabel:
  return rc;
  
}


cmDspRC_t _cmDspSysPgm_DistDs( cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t       rc        = kOkDspRC;
  const char*     afn0      = "media/audio/20110723-Kriesberg/Audio Files/Piano 3_01.wav";
  int             beg       = 6900826;
  int             end       = 13512262;
  const cmChar_t* afn       = cmFsMakeFn(cmFsUserDir(),afn0,NULL,NULL );
  bool            bypassFl  = false;
  double          inGain    = 1.0;
  double          dsrate    = 44100.0;
  double          bits      = 24.0;
  bool            rectifyFl = false;
  bool            fullRectFl = false;
  double          clipDb     = -10.0;
  

  cmDspInst_t* off =  cmDspSysAllocInst(h,"Scalar", "Offset",  5, kNumberDuiId, 0.0,  cmDspSysSampleRate(h)*600.0, 1.0,  0.0);
  cmDspInst_t* phs =  cmDspSysAllocInst(h,"Phasor",   NULL,  0 );
  cmDspInst_t* wt  =  cmDspSysAllocInst(h,"WaveTable",NULL,  6, ((int)cmDspSysSampleRate(h)), 1, afn, -1, beg, end );
  cmDspInst_t* dst =  cmDspSysAllocInst(h,"DistDs",NULL,   3, bypassFl, inGain, dsrate, bits  ); 

  cmDspInst_t* ao0   = cmDspSysAllocInst(h,"AudioOut",NULL,   1, 0 );
  cmDspInst_t* ao1   = cmDspSysAllocInst(h,"AudioOut",NULL,   1, 1 );

  cmDspInst_t* ign   = cmDspSysAllocScalar( h, "In Gain",      0.0, 10.0, 0.01, 1.0);
  cmDspInst_t* rct   = cmDspSysAllocCheck(  h, "Rectify",   rectifyFl);
  cmDspInst_t* ful   = cmDspSysAllocCheck(  h, "Full/Half", fullRectFl);
  cmDspInst_t* dsr   = cmDspSysAllocScalar( h, "Srate",        0.0, 96000, 1.0, dsrate);
  cmDspInst_t* dbt   = cmDspSysAllocScalar( h, "bits",         2.0,  32.0, 1.0, bits);
  cmDspInst_t* clip  = cmDspSysAllocScalar( h, "Clip dB",   -100.0,   0.0, 0.1, clipDb);
  cmDspInst_t* ogn   = cmDspSysAllocScalar( h, "Out Gain",    0.0, 10.0, 0.01, 1.0);

    // check for allocation errors
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    goto errLabel;

  cmDspSysConnectAudio(h, phs, "out", wt,  "phs" );  // phasor -> wave table
  cmDspSysConnectAudio(h, wt,  "out", dst, "in" );   // wave table -> cmp in
  cmDspSysConnectAudio(h, dst, "out", ao0, "in" );   // filter -> aout
  cmDspSysConnectAudio(h, dst, "out", ao1, "in" );   // 


  cmDspSysInstallCb(h, off,  "val", wt,  "beg", NULL ); 
  cmDspSysInstallCb(h, ign,  "val", dst, "igain", NULL );
  cmDspSysInstallCb(h, dsr,  "val", dst, "srate", NULL );
  cmDspSysInstallCb(h, dbt,  "val", dst, "bits", NULL );
  cmDspSysInstallCb(h, rct,  "out", dst, "rect", NULL );
  cmDspSysInstallCb(h, ful,  "out", dst, "full", NULL );
  cmDspSysInstallCb(h, clip, "val", dst, "clip", NULL );
  cmDspSysInstallCb(h, ogn,  "val", dst, "ogain", NULL );

 errLabel:
  return rc;
  
}

cmDspRC_t _cmDspSysPgm_Seq( cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t rc;
  double min = 1.0;
  double max = 10.0;
  double incr = 1.0;

  cmDspInst_t* btn = cmDspSysAllocButton( h, "smack", 0);
  cmDspInst_t* cnt = cmDspSysAllocInst(   h, "Counter", NULL,   3, min, max, incr );
  cmDspInst_t* lst = cmDspSysAllocInst(   h, "MsgList","Seq", 1, "seqTest");
  cmDspInst_t* prt = cmDspSysAllocInst(   h, "Printer", NULL, 1, ">");

  cmDspSysInstallCb(h, lst, "cnt",  cnt, "max",  NULL );
  cmDspSysInstallCb(h, btn, "out",  cnt, "next", NULL );
  cmDspSysInstallCb(h, cnt, "out",  lst, "sel",  NULL );
  cmDspSysInstallCb(h, lst, "midi", prt, "in", NULL );
  

    // check for allocation errors
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    goto errLabel;

 errLabel:
  return rc;
}

cmDspRC_t _cmDspSysPgm_ThunkNet( cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t rc;
  cmDspInst_t* add   = cmDspSysAllocInst(   h, "ScalarOp", "adder-0",  6, 2, "+", "in-0", 0.0, "in-1", 0.0 );
  cmDspInst_t* in    = cmDspSysAllocScalar( h, "Input",      0.0, 10.0, 0.001, 0.0);
  cmDspInst_t* out   = cmDspSysAllocScalar( h, "Out",        0.0, 10.0, 0.001, 0.0);

    // check for allocation errors
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    goto errLabel;

  cmDspSysInstallCb( h, in,    "val", add,  "in-1", NULL );
  cmDspSysInstallCb( h, add,   "out", out,  "val",  NULL );

 errLabel:
  return rc;
}

cmDspRC_t _cmDspSysPgm_WhirlNet( cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t rc;

  cmDspInst_t* in    = cmDspSysAllocScalar( h, "Input",      0.0, 10.0, 0.001, 0.0);
  
    // check for allocation errors
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    goto errLabel;

  cmDspSysInstallNetCb( h, in,    "val", "thunk",  "adder-0", "in-0" );

 errLabel:
  return rc;
  
}

cmDspRC_t _cmDspSysPgm_NofM( cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t rc;
  unsigned iChCnt = 3;
  unsigned oChCnt = 2;
  unsigned sgShapeId = 0;
  double  sgGain = 0.4;
  double  xfadeMs = 1000;

  cmDspInst_t* onBtn = cmDspSysAllocButton( h, "on", 0 );
  cmDspInst_t* offBtn = cmDspSysAllocButton(h, "off", 0 );

  cmDspInst_t* sg0  = cmDspSysAllocInst(   h, "SigGen",    NULL, 3, 500.0, sgShapeId, sgGain );
  cmDspInst_t* sg1  = cmDspSysAllocInst(   h, "SigGen",    NULL, 3, 1000.0, sgShapeId, sgGain );
  cmDspInst_t* sg2  = cmDspSysAllocInst(   h, "SigGen",    NULL, 3, 2000.0, sgShapeId, sgGain );

  cmDspInst_t* nom  = cmDspSysAllocInst(   h,"NofM",     NULL,   3, iChCnt, oChCnt, xfadeMs );
  cmDspInst_t* mix  = cmDspSysAllocInst(   h, "AMix",    NULL, 1, oChCnt  );
  cmDspInst_t* ao   = cmDspSysAllocAudioOut(h, 0, 1.0 ); 


  cmDspInst_t* btn = cmDspSysAllocButton( h, "cfg", 0);


  cmDspInst_t* sel0 = cmDspSysAllocCheck(h,"Sel-0",0);
  cmDspInst_t* sel1 = cmDspSysAllocCheck(h,"Sel-1",0);
  cmDspInst_t* sel2 = cmDspSysAllocCheck(h,"Sel-2",0);

  cmDspInst_t* in0  = cmDspSysAllocScalar( h, "In-0",      0.0, 10.0, 0.001, 0.0);
  cmDspInst_t* in1  = cmDspSysAllocScalar( h, "In-1",      0.0, 10.0, 0.001, 0.0);
  cmDspInst_t* in2  = cmDspSysAllocScalar( h, "In-2",      0.0, 10.0, 0.001, 0.0);  

  cmDspInst_t* out0  = cmDspSysAllocScalar( h, "Out-0",      0.0, 10.0, 0.001, 0.0);
  cmDspInst_t* out1  = cmDspSysAllocScalar( h, "Out-1",      0.0, 10.0, 0.001, 0.0);


  // check for allocation errors
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    goto errLabel;


  cmDspSysConnectAudio( h, sg0, "out", nom, "a-in-0" );
  cmDspSysConnectAudio( h, sg1, "out", nom, "a-in-1" );
  cmDspSysConnectAudio( h, sg2, "out", nom, "a-in-2" );
  cmDspSysConnectAudio( h, nom, "a-out-0", mix, "in-0" );
  cmDspSysConnectAudio( h, nom, "a-out-1", mix, "in-1" );
  cmDspSysConnectAudio( h, mix, "out",     ao,  "in" );

  cmDspSysInstallCb( h, btn,  "sym", nom, "cmd", NULL );

  cmDspSysInstallCb( h, sel0, "out", nom, "sel-0", NULL );
  cmDspSysInstallCb( h, sel1, "out", nom, "sel-1", NULL );
  cmDspSysInstallCb( h, sel2, "out", nom, "sel-2", NULL );

  cmDspSysInstallCb( h, in0, "val", nom, "f-in-0", NULL );
  cmDspSysInstallCb( h, in1, "val", nom, "f-in-1", NULL );
  cmDspSysInstallCb( h, in2, "val", nom, "f-in-2", NULL );

  cmDspSysInstallCb( h, nom, "f-out-0", out0, "val", NULL );
  cmDspSysInstallCb( h, nom, "f-out-1", out1, "val", NULL );

  cmDspSysInstallCb( h, onBtn, "sym", nom, "cmd", NULL );
  cmDspSysInstallCb( h, offBtn, "sym", nom, "cmd", NULL );

 errLabel:
  return rc;
}

cmDspRC_t _cmDspSysPgm_1ofN( cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t rc;
  unsigned inCnt = 3;
  unsigned initSel = 0;

  cmDspInst_t* oom = cmDspSysAllocInst(h,"1ofN", NULL,   2, inCnt, initSel );

  cmDspInst_t* sel  = cmDspSysAllocScalar( h, "Sel",      0, inCnt-1, 0.001, 0.0);

  cmDspInst_t* in0  = cmDspSysAllocScalar( h, "In-0",      0.0, 10.0, 0.001, 0.0);
  cmDspInst_t* in1  = cmDspSysAllocScalar( h, "In-1",      0.0, 10.0, 0.001, 0.0);
  cmDspInst_t* in2  = cmDspSysAllocScalar( h, "In-2",      0.0, 10.0, 0.001, 0.0);  

  cmDspInst_t* out  = cmDspSysAllocScalar( h, "Out",      0.0, 10.0, 0.001, 0.0);


  // check for allocation errors
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    goto errLabel;


  cmDspSysInstallCb( h, sel, "val", oom, "chidx", NULL );

  cmDspSysInstallCb( h, in0, "val", oom, "f-in-0", NULL );
  cmDspSysInstallCb( h, in1, "val", oom, "f-in-1", NULL );
  cmDspSysInstallCb( h, in2, "val", oom, "f-in-2", NULL );

  cmDspSysInstallCb( h, oom, "f-out", out, "val", NULL );


 errLabel:
  return rc;
}

cmDspRC_t _cmDspSysPgm_Router( cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t rc;
  unsigned inCnt = 3;
  unsigned initSel = 0;

  cmDspInst_t* rtr = cmDspSysAllocInst(h,"Router", NULL,   2, inCnt, initSel );

  cmDspInst_t* sel  = cmDspSysAllocScalar( h, "Sel",      0, inCnt-1, 1.0, 0.0);

  cmDspInst_t* in  = cmDspSysAllocScalar( h, "In",      0.0, 10.0, 0.001, 0.0);

  cmDspInst_t* out0  = cmDspSysAllocScalar( h, "Out-0",      0.0, 10.0, 0.001, 0.0);
  cmDspInst_t* out1  = cmDspSysAllocScalar( h, "Out-1",      0.0, 10.0, 0.001, 0.0);
  cmDspInst_t* out2  = cmDspSysAllocScalar( h, "Out-2",      0.0, 10.0, 0.001, 0.0);  



  // check for allocation errors
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    goto errLabel;


  cmDspSysInstallCb( h, sel, "val", rtr, "sel", NULL );

  cmDspSysInstallCb( h, in, "val", rtr, "f-in", NULL );

  cmDspSysInstallCb( h, rtr, "f-out-0", out0, "val", NULL );
  cmDspSysInstallCb( h, rtr, "f-out-1", out1, "val", NULL );
  cmDspSysInstallCb( h, rtr, "f-out-2", out2, "val", NULL );




 errLabel:
  return rc;
}

cmDspRC_t _cmDspSysPgm_Preset( cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t rc;

  unsigned        sgShapeId = 0;
  double          sgHz      = 500;
  double          sgGain    = 0.02;
  unsigned        grpSymId  = cmDspSysPresetRegisterGroup(h,"test");
  const cmChar_t* prefixLabel    = NULL;

  cmDspInst_t* sg  = cmDspSysAllocInst(   h, "SigGen",    NULL, 3, sgHz, sgShapeId, sgGain );
  cmDspInst_t* ao  =  cmDspSysAllocAudioOut( h, 0, 1.0 );
  
  cmDspInst_t* shapes = cmDspSysAllocMsgListP(h, grpSymId, prefixLabel, "Shape", NULL, "shapeList", 0 );
  cmDspInst_t* gains  = cmDspSysAllocMsgListP(h, grpSymId, prefixLabel, "Gains", NULL, "gainMenu", 0 );
  cmDspInst_t* hz     = cmDspSysAllocScalarP( h, grpSymId, prefixLabel, "Hz",        0.0, 10000.0, 0.01, sgHz);
  cmDspInst_t* gain   = cmDspSysAllocScalarP( h, grpSymId, prefixLabel, "Gain",      0.0, 1.0,     0.01, sgGain);
  
  cmDspInst_t* preset    = cmDspSysAllocInst(   h, "Preset", NULL, 1, grpSymId );
  cmDspInst_t* presetLbl = cmDspSysAllocInst(   h, "Text",   "Preset",      1, "" );
  cmDspInst_t* storeBtn  = cmDspSysAllocButton( h, "store",  0);
  cmDspInst_t* recallBtn = cmDspSysAllocButton( h, "recall", 0);

  // check for allocation errors
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    goto errLabel;

  cmDspSysConnectAudio(h, sg,    "out", ao, "in" );
  cmDspSysInstallCb(   h, shapes,"out", sg, "shape", NULL );
  cmDspSysInstallCb(   h, hz,    "val", sg, "hz",    NULL );
  cmDspSysInstallCb(   h, gain,  "val", sg, "gain",  NULL );
  cmDspSysInstallCb(   h, gains, "out", gain, "val",  NULL );
  
  cmDspSysInstallCb(   h, presetLbl, "val", preset, "label",NULL);
  cmDspSysInstallCb(   h, storeBtn,  "sym", preset, "cmd", NULL );
  cmDspSysInstallCb(   h, recallBtn, "sym", preset, "cmd", NULL );
 errLabel:
  return rc;
}

cmDspRC_t _cmDspSysPgm_RsrcWr( cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t    rc      = kOkDspRC;

  const cmChar_t* lbl1 = "rsrc1";
  const cmChar_t* lbl2 = "rsrc2";
  const cmChar_t* str1 = NULL;
  const cmChar_t* str2 = NULL;

  if( cmDspRsrcString(h,&str1, lbl1, NULL ) != kOkDspRC )
    str1 = "1";

  if( cmDspRsrcString(h,&str2, lbl2, NULL ) != kOkDspRC )
    str2 = "2";

  cmDspInst_t* rsrcWr  = cmDspSysAllocInst( h, "RsrcWr", NULL,    2, lbl1, lbl2 );
  cmDspInst_t* textUi0 = cmDspSysAllocInst( h, "Text",   "Value1",      1, str1 );
  cmDspInst_t* textUi1 = cmDspSysAllocInst( h, "Text",   "Value2",      1, str2 );

  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    goto errLabel;

  cmDspSysInstallCb( h, textUi0, "val", rsrcWr, "rsrc1", NULL );
  cmDspSysInstallCb( h, textUi1, "val", rsrcWr, "rsrc2", NULL );

 errLabel:
  return rc;
}

cmDspRC_t _cmDspSysPgm_1Up( cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t rc;
  unsigned chCnt = 3;
  double   maxIdx = chCnt - 1;
  unsigned initIdx = 2;

  cmDspInst_t* sel  = cmDspSysAllocScalar( h, "Chan", 0.0, maxIdx, 1.0, 0.0 );
  cmDspInst_t* up   = cmDspSysAllocInst( h, "1Up", NULL, 2, chCnt, initIdx );

  cmDspInst_t* pr0  = cmDspSysAllocInst( h, "Printer", NULL, 1, "0:" );
  cmDspInst_t* pr1  = cmDspSysAllocInst( h, "Printer", NULL, 1, "1:" );
  cmDspInst_t* pr2  = cmDspSysAllocInst( h, "Printer", NULL, 1, "2:" );

  // check for allocation errors
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    goto errLabel;
  
  cmDspSysInstallCb(   h, sel, "val", up, "sel",NULL);
  cmDspSysInstallCb(   h, up,  "out-0", pr0, "in", NULL );
  cmDspSysInstallCb(   h, up,  "out-1", pr1, "in", NULL );
  cmDspSysInstallCb(   h, up,  "out-2", pr2, "in", NULL );

 errLabel:
  return rc;
}

cmDspRC_t _cmDspSysPgm_PortToSym( cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t rc = kOkDspRC;

  cmDspInst_t* btn0  = cmDspSysAllocButton( h, "Btn0", 0.0 );
  cmDspInst_t* btn1  = cmDspSysAllocButton( h, "Btn1", 0.0 );
  cmDspInst_t* btn2  = cmDspSysAllocButton( h, "Btn2", 0.0 );  

  cmDspInst_t* pts   = cmDspSysAllocInst( h, "PortToSym", NULL, 3, "one", "two", "three");

  cmDspInst_t* pr0  = cmDspSysAllocInst( h, "Printer", NULL, 1, "0:" );
  cmDspInst_t* pr1  = cmDspSysAllocInst( h, "Printer", NULL, 1, "1:" );

  // check for allocation errors
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    goto errLabel;
  
  cmDspSysInstallCb(   h, btn0, "out", pts, "one",NULL);
  cmDspSysInstallCb(   h, btn1, "out", pts, "two",NULL);
  cmDspSysInstallCb(   h, btn2, "out", pts, "three",NULL);

  cmDspSysInstallCb(   h, btn0, "out", pr1, "in",NULL);
  cmDspSysInstallCb(   h, btn1, "out", pr1, "in",NULL);
  cmDspSysInstallCb(   h, btn2, "out", pr1, "in",NULL);

  cmDspSysInstallCb(   h, pts,  "one",   pr0, "in", NULL );
  cmDspSysInstallCb(   h, pts,  "two",   pr0, "in", NULL );
  cmDspSysInstallCb(   h, pts,  "three", pr0, "in", NULL );


 errLabel:
  return rc;
}

cmDspRC_t _cmDspSysPgm_Line( cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t rc = kOkDspRC;

  cmDspInst_t* beg  = cmDspSysAllocScalar( h, "beg", -10.0, 10.0, 1.0, 0.0 );
  cmDspInst_t* end  = cmDspSysAllocScalar( h, "end", -10.0, 10.0, 1.0, 1.0 );
  cmDspInst_t* dur  = cmDspSysAllocScalar( h, "dur", 0.0, 10000.0, 1.0, 0.0 );  
  cmDspInst_t* reset = cmDspSysAllocButton(h, "reset", 0.0 );

  cmDspInst_t* line   = cmDspSysAllocInst( h, "Line", NULL, 3, 0.0, 10.0, 1000.0 );

  cmDspInst_t* mtr  = cmDspSysAllocInst( h, "Meter", NULL, 3, -10.0, 10.0, 0.0  );

  cmDspInst_t* pr1  = cmDspSysAllocInst( h, "Printer", NULL, 1, ">" );

  // check for allocation errors
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    goto errLabel;
  
  cmDspSysInstallCb(   h, beg, "val", line, "beg",NULL);
  cmDspSysInstallCb(   h, end, "val", line, "end",NULL);
  cmDspSysInstallCb(   h, dur, "val", line, "dur",NULL);


  cmDspSysInstallCb(   h, line,  "out", mtr, "in", NULL );
  cmDspSysInstallCb(   h, reset, "sym", line, "cmd", NULL );
  cmDspSysInstallCb(   h, line, "out", pr1, "in", NULL );
 errLabel:
  return rc;
}

cmDspRC_t _cmDspSysPgm_Array( cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t       rc           = kOkDspRC;
  unsigned        cnt          = 0;
  const cmChar_t* rsrcLabelStr = "test";

  if( cmDspRsrcArrayCount( h, &cnt, rsrcLabelStr, NULL ) != kOkDspRC )
    return cmErrMsg(&cmDspSysPgmCtx(h)->err,kPgmCfgFailDspRC,"The resource '%s' could not be read.",rsrcLabelStr);

  cmDspInst_t*  printBtn = cmDspSysAllocButton(    h, "print", 0.0 );
  cmDspInst_t*  sendBtn  = cmDspSysAllocButton(    h, "send",  0.0 );
  cmDspInst_t*  cntBtn   = cmDspSysAllocButton(    h, "count", 0.0 );
  cmDspInst_t*  offsCtl  = cmDspSysAllocScalar(    h, "offset",0.0, 128.0, 1.0, 60.0 );
  cmDspInst_t*  array    = cmDspSysAllocInst(      h, "Array",    NULL, 1, rsrcLabelStr );
  cmDspInst_t** pcvt     = cmDspSysAllocInstArray( h, cnt, "PitchCvt", NULL, NULL,  0 );
  cmDspInst_t*  printer  = cmDspSysAllocInst(      h, "Printer",  NULL, 1, ">" );
  
  cmDspSysInstallCb(     h, printBtn, "sym",  array,   "cmd", NULL );
  cmDspSysInstallCb(     h, sendBtn,  "sym",  array,   "cmd", NULL );
  cmDspSysInstallCb(     h, cntBtn,   "sym",  array,   "cmd", NULL );
  cmDspSysInstallCb11N1( h, offsCtl,  "val",  pcvt,    "offs", cnt );
  cmDspSysInstallCb1NN1( h, array,    "out",  pcvt,    "midi", cnt );
  cmDspSysInstallCbN111( h, pcvt,     "midi", printer, "in",  cnt );
  cmDspSysInstallCb(     h, array,    "cnt",  printer, "in",  NULL );
  return rc;
}

cmDspRC_t _cmDspSysPgm_SegLine( cmDspSysH_t h, void** userPtrPtr )
{
  cmDspRC_t rc = kOkDspRC;

  cmDspInst_t*  btn     = cmDspSysAllocButton(    h, "Trig", 0.0 );
  cmDspInst_t*  sline   = cmDspSysAllocInst(      h, "SegLine", NULL, 1, "array" );
  cmDspInst_t*  printer = cmDspSysAllocInst(      h, "Printer",  NULL, 1, ">" );
  
  // check for allocation errors
  if((rc = cmDspSysLastRC(h)) != kOkDspRC )
    goto errLabel;

  cmDspSysInstallCb( h, btn,   "sym",  sline,   "trig", NULL );
  cmDspSysInstallCb( h, sline, "out", printer, "in",   NULL );

 errLabel:
  return rc;
}

cmDspRC_t _cmDspSysPgm_AvailCh( cmDspSysH_t h, void** userPtrPtr )
{
  double   frqHz       = 440.0;
  unsigned xfadeChCnt  = 2;
  double   xfadeMs     = 250.0;
  bool     xfadeInitFl = false;

  const char*  fn    = "/home/kevin/media/audio/20110723-Kriesberg/Audio Files/Piano 3_01.wav";
  
  cmDspInst_t* chk0   = cmDspSysAllocInst(h,"Button", "0",  2, kButtonDuiId, 0.0 );
  //cmDspInst_t* chk1   = cmDspSysAllocInst(h,"Button", "1",  2, kCheckDuiId, 0.0 );

  cmDspInst_t* achp  = cmDspSysAllocInst( h, "AvailCh", NULL, 1, xfadeChCnt );
  
  cmDspInst_t* sphp  =  cmDspSysAllocInst( h, "Phasor",    NULL,   2, cmDspSysSampleRate(h), frqHz );
  cmDspInst_t* swtp  =  cmDspSysAllocInst( h, "WaveTable", NULL,   2, ((int)cmDspSysSampleRate(h)), 4);
  cmDspInst_t* fphp  =  cmDspSysAllocInst( h, "Phasor",    NULL,   1, cmDspSysSampleRate(h) );
  cmDspInst_t* fwtp  =  cmDspSysAllocInst( h, "WaveTable", NULL,   5, ((int)cmDspSysSampleRate(h)), 1, fn, -1, 7000000 );
  cmDspInst_t* fad0  =  cmDspSysAllocInst( h, "Xfader",    NULL,   3, xfadeChCnt,  xfadeMs, xfadeInitFl ); 

  //cmDspInst_t*  prp  = cmDspSysAllocInst(  h, "Printer",  NULL, 1, ">" );

  cmDspInst_t* ao0p = cmDspSysAllocInst(h,"AudioOut",  NULL,   1, 0 );
  cmDspInst_t* ao1p = cmDspSysAllocInst(h,"AudioOut",  NULL,   1, 1 );
    

  // phasor->sine->fad-0->aout
  cmDspSysConnectAudio(h, sphp, "out",   swtp, "phs" );
  cmDspSysConnectAudio(h, swtp, "out",   fad0, "in-0" ); 
  cmDspSysConnectAudio(h, fad0, "out-0", ao0p, "in" );

  // phasor->file->fad-1->aout
  cmDspSysConnectAudio(h, fphp, "out",   fwtp, "phs" );
  cmDspSysConnectAudio(h, fwtp, "out",   fad0, "in-1" ); 
  cmDspSysConnectAudio(h, fad0, "out-1", ao1p, "in" ); 

  //cmDspSysInstallCb(h, chk0, "out", fad0, "gate-0", NULL);
  //cmDspSysInstallCb(h, chk1, "out", fad0, "gate-1", NULL);

  cmDspSysInstallCb(h, chk0, "sym",     achp, "trig",   NULL);
  cmDspSysInstallCb(h, achp, "gate-0",  fad0, "gate-0", NULL );
  cmDspSysInstallCb(h, fad0, "state-0", achp, "dis-0",   NULL );

  cmDspSysInstallCb(h, achp, "gate-1",  fad0, "gate-1", NULL );
  cmDspSysInstallCb(h, fad0, "state-1", achp, "dis-1",   NULL );


  return kOkDspRC;

}

_cmDspSysPgm_t _cmDspSysPgmArray[] = 
{
  { "time_line",     _cmDspSysPgm_TimeLine,     NULL, NULL },
  { "switcher",      _cmDspSysPgm_Switcher,     NULL, NULL },
  { "main",          _cmDspSysPgm_Main,         NULL, NULL },
  { "array",         _cmDspSysPgm_Array,        NULL, NULL },
  { "line",          _cmDspSysPgm_Line,         NULL, NULL },
  { "1Up",           _cmDspSysPgm_1Up,          NULL, NULL },
  { "PortToSym",     _cmDspSysPgm_PortToSym,    NULL, NULL },
  { "preset",        _cmDspSysPgm_Preset,       NULL, NULL },
  { "rsrcWr",        _cmDspSysPgm_RsrcWr,       NULL, NULL },
  { "router",        _cmDspSysPgm_Router,       NULL, NULL },
  { "1ofN",          _cmDspSysPgm_1ofN,         NULL, NULL },
  { "NofM",          _cmDspSysPgm_NofM,         NULL, NULL },
  { "whirl_net",     _cmDspSysPgm_WhirlNet,     NULL, NULL },
  { "thunk_net",     _cmDspSysPgm_ThunkNet,     NULL, NULL },
  { "seq",           _cmDspSysPgm_Seq,          NULL, NULL },
  { "dist_ds",       _cmDspSysPgm_DistDs,       NULL, NULL },
  { "bi_quad_eq",    _cmDspSysPgm_BiQuadEq,     NULL, NULL },
  { "compressor",    _cmDspSysPgm_Compressor,   NULL, NULL },
  { "adsr",          _cmDspSysPgm_Adsr,         NULL, NULL },
  { "msg delay",     _cmDspSysPgm_MsgDelay,     NULL, NULL },
  { "pickup rmod2",  _cmDspSysPgm_RingMod2,     NULL, NULL },
  { "pickup rmod",  _cmDspSysPgm_RingMod,       NULL, NULL },
  { "pickup tails", _cmDspSysPgm_NoiseTails,    NULL, NULL },
  { "tails_2",      _cmDspSysPgm_NoiseTails2,   NULL, NULL },
  { "pickups",     _cmDspSysPgm_Pickups0,       NULL, NULL },
  { "midi_test",   _cmDspSysPgm_Test_Midi,      NULL, NULL },
  { "2_thru",      _cmDspSysPgm_Stereo_Through, NULL, NULL },
  { "guitar",      _cmDspSysPgmGuitar,          NULL, NULL },
  { "2_fx",        _cmDspSysPgm_Stereo_Fx,      NULL, NULL },
  { "sine",        _cmDspSysPgm_PlaySine,       NULL, NULL },
  { "file",        _cmDspSysPgm_PlayFile,       NULL, NULL },
  { "gate_detect", _cmDspSysPgm_GateDetect,     NULL, NULL },
  { "record",      _cmDspSysPgm_Record,         NULL, NULL },
  { "pitch_shift", _cmDspSysPgm_PitchShiftFile, NULL, NULL },
  { "loop_recd",   _cmDspSysPgm_LoopRecd,       NULL, NULL },
  { "ui_test",     _cmDspSysPgm_UiTest,         NULL, NULL },
  { "xfade_test",  _cmDspSysPgm_Xfade,          NULL, NULL },
  { "auto_gain",   _cmDspSysPgm_AutoGain,       NULL, NULL },
  { "comb filt",   _cmDspSysPgm_CombFilt,       NULL, NULL },
  { "scalar op",   _cmDspSysPgm_ScalarOp,       NULL, NULL },
  { "seg_line",    _cmDspSysPgm_SegLine,        NULL, NULL },
  { "avail_ch",    _cmDspSysPgm_AvailCh,        NULL, NULL },
  { NULL , NULL, NULL, NULL }
};

_cmDspSysPgm_t* _cmDspSysPgmArrayBase()
{
  return _cmDspSysPgmArray;
}


