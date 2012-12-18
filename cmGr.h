#ifndef cmGr_h
#define cmGr_h

#ifdef __cplusplus
extern "C" {
#endif

  enum 
  {
    kAliceBlueGrId            = 0xf0f8ff,
    kAntiqueWhiteGrId         = 0xfaebd7,
    kAquaGrId                 = 0x00ffff,
    kAquamarineGrId           = 0x7fffd4,
    kAzureGrId                = 0xf0ffff,
    kBeigeGrId                = 0xf5f5dc,
    kBisqueGrId               = 0xffe4c4,
    kBlackGrId                = 0x000000,
    kBlanchedAlmondGrId       = 0xffebcd,
    kBlueGrId                 = 0x0000ff,
    kBlueVioletGrId           = 0x8a2be2,
    kBrownGrId                = 0xa52a2a,
    kBurlyWoodGrId            = 0xdeb887,
    kCadetBlueGrId            = 0x5f9ea0,
    kChartreuseGrId           = 0x7fff00,
    kChocolateGrId            = 0xd2691e,
    kCoralGrId                = 0xff7f50,
    kCornflowerBlueGrId       = 0x6495ed,
    kCornsilkGrId             = 0xfff8dc,
    kCrimsonGrId              = 0xdc143c,
    kCyanGrId                 = 0x00ffff,
    kDarkBlueGrId             = 0x00008b,
    kDarkCyanGrId             = 0x008b8b,
    kDarkGoldenRodGrId        = 0xb8860b,
    kDarkGrayGrId             = 0xa9a9a9,
    kDarkGreyGrId             = 0xa9a9a9,
    kDarkGreenGrId            = 0x006400,
    kDarkKhakiGrId            = 0xbdb76b,
    kDarkMagentaGrId          = 0x8b008b,
    kDarkOliveGreenGrId       = 0x556b2f,
    kDarkorangeGrId           = 0xff8c00,
    kDarkOrchidGrId           = 0x9932cc,
    kDarkRedGrId              = 0x8b0000,
    kDarkSalmonGrId           = 0xe9967a,
    kDarkSeaGreenGrId         = 0x8fbc8f,
    kDarkSlateBlueGrId        = 0x483d8b,
    kDarkSlateGrayGrId        = 0x2f4f4f,
    kDarkSlateGreyGrId        = 0x2f4f4f,
    kDarkTurquoiseGrId        = 0x00ced1,
    kDarkVioletGrId           = 0x9400d3,
    kDeepPinkGrId             = 0xff1493,
    kDeepSkyBlueGrId          = 0x00bfff,
    kDimGrayGrId              = 0x696969,
    kDimGreyGrId              = 0x696969,
    kDodgerBlueGrId           = 0x1e90ff,
    kFireBrickGrId            = 0xb22222,
    kFloralWhiteGrId          = 0xfffaf0,
    kForestGreenGrId          = 0x228b22,
    kFuchsiaGrId              = 0xff00ff,
    kGainsboroGrId            = 0xdcdcdc,
    kGhostWhiteGrId           = 0xf8f8ff,
    kGoldGrId                 = 0xffd700,
    kGoldenRodGrId            = 0xdaa520,
    kGrayGrId                 = 0x808080,
    kGreyGrId                 = 0x808080,
    kGreenGrId                = 0x008000,
    kGreenYellowGrId          = 0xadff2f,
    kHoneyDewGrId             = 0xf0fff0,
    kHotPinkGrId              = 0xff69b4,
    kIndianRedGrId            = 0xcd5c5c,
    kIndigoGrId               = 0x4b0082,
    kIvoryGrId                = 0xfffff0,
    kKhakiGrId                = 0xf0e68c,
    kLavenderGrId             = 0xe6e6fa,
    kLavenderBlushGrId        = 0xfff0f5,
    kLawnGreenGrId            = 0x7cfc00,
    kLemonChiffonGrId         = 0xfffacd,
    kLightBlueGrId            = 0xadd8e6,
    kLightCoralGrId           = 0xf08080,
    kLightCyanGrId            = 0xe0ffff,
    kLightGoldenRodYellowGrId = 0xfafad2,
    kLightGrayGrId            = 0xd3d3d3,
    kLightGreyGrId            = 0xd3d3d3,
    kLightGreenGrId           = 0x90ee90,
    kLightPinkGrId            = 0xffb6c1,
    kLightSalmonGrId          = 0xffa07a,
    kLightSeaGreenGrId        = 0x20b2aa,
    kLightSkyBlueGrId         = 0x87cefa,
    kLightSlateGrayGrId       = 0x778899,
    kLightSlateGreyGrId       = 0x778899,
    kLightSteelBlueGrId       = 0xb0c4de,
    kLightYellowGrId          = 0xffffe0,
    kLimeGrId                 = 0x00ff00,
    kLimeGreenGrId            = 0x32cd32,
    kLinenGrId                = 0xfaf0e6,
    kMagentaGrId              = 0xff00ff,
    kMaroonGrId               = 0x800000,
    kMediumAquaMarineGrId     = 0x66cdaa,
    kMediumBlueGrId           = 0x0000cd,
    kMediumOrchidGrId         = 0xba55d3,
    kMediumPurpleGrId         = 0x9370d8,
    kMediumSeaGreenGrId       = 0x3cb371,
    kMediumSlateBlueGrId      = 0x7b68ee,
    kMediumSpringGreenGrId    = 0x00fa9a,
    kMediumTurquoiseGrId      = 0x48d1cc,
    kMediumVioletRedGrId      = 0xc71585,
    kMidnightBlueGrId         = 0x191970,
    kMintCreamGrId            = 0xf5fffa,
    kMistyRoseGrId            = 0xffe4e1,
    kMoccasinGrId             = 0xffe4b5,
    kNavajoWhiteGrId          = 0xffdead,
    kNavyGrId                 = 0x000080,
    kOldLaceGrId              = 0xfdf5e6,
    kOliveGrId                = 0x808000,
    kOliveDrabGrId            = 0x6b8e23,
    kOrangeGrId               = 0xffa500,
    kOrangeRedGrId            = 0xff4500,
    kOrchidGrId               = 0xda70d6,
    kPaleGoldenRodGrId        = 0xeee8aa,
    kPaleGreenGrId            = 0x98fb98,
    kPaleTurquoiseGrId        = 0xafeeee,
    kPaleVioletRedGrId        = 0xd87093,
    kPapayaWhipGrId           = 0xffefd5,
    kPeachPuffGrId            = 0xffdab9,
    kPeruGrId                 = 0xcd853f,
    kPinkGrId                 = 0xffc0cb,
    kPlumGrId                 = 0xdda0dd,
    kPowderBlueGrId           = 0xb0e0e6,
    kPurpleGrId               = 0x800080,
    kRedGrId                  = 0xff0000,
    kRosyBrownGrId            = 0xbc8f8f,
    kRoyalBlueGrId            = 0x4169e1,
    kSaddleBrownGrId          = 0x8b4513,
    kSalmonGrId               = 0xfa8072,
    kSandyBrownGrId           = 0xf4a460,
    kSeaGreenGrId             = 0x2e8b57,
    kSeaShellGrId             = 0xfff5ee,
    kSiennaGrId               = 0xa0522d,
    kSilverGrId               = 0xc0c0c0,
    kSkyBlueGrId              = 0x87ceeb,
    kSlateBlueGrId            = 0x6a5acd,
    kSlateGrayGrId            = 0x708090,
    kSlateGreyGrId            = 0x708090,
    kSnowGrId                 = 0xfffafa,
    kSpringGreenGrId          = 0x00ff7f,
    kSteelBlueGrId            = 0x4682b4,
    kTanGrId                  = 0xd2b48c,
    kTealGrId                 = 0x008080,
    kThistleGrId              = 0xd8bfd8,
    kTomatoGrId               = 0xff6347,
    kTurquoiseGrId            = 0x40e0d0,
    kVioletGrId               = 0xee82ee,
    kWheatGrId                = 0xf5deb3,
    kWhiteGrId                = 0xffffff,
    kWhiteSmokeGrId           = 0xf5f5f5,
    kYellowGrId               = 0xffff00,
    kYellowGreenGrId          = 0x9acd32
  };

 
  typedef enum
  {
    kHomeGrId = 5,       // 5
    kPageUpGrId,         // 6
    kEndGrId,            // 7
    kBackSpaceGrId = 8,  // 8
    kTabGrId       = 9,  // 9 
    kPageDownGrId,       // 10
    kLeftGrId,           // 11
    kUpGrId,             // 12
    kEnterGrId  = 13,    // 13
    kRightGrId,          // 14
    kDownGrId,           // 15
    kInsertGrId,         // 16
    kPrintGrId,          // 17
    kScrollLockGrId,     // 18
    kPauseGrId,          // 19
    kMenuGrId,           // 20
    kLShiftGrId,         // 21
    kRShiftGrId,         // 22
    kLCtrlGrId,          // 23
    kRCtrlGrId,          // 24
    kLAltGrId,           // 25
    kRAltGrId,           // 26
    kEscapeGrId = 27,    // 27
    kLSuperGrId,         // 28
    kRSuperGrId,         // 29
    kNumLockGrId,        // 30
    kCapsLockGrId,       // 31
    kSpaceGrId     = 32, // 32  Min. printable ASCII
    kExclMarkGrId,       // 33
    kDQuoteGrId,         // 34
    kPoundGrId,          // 35
    kDollarGrId,         // 36
    kPercentGrId,        // 37
    kAmpersandGrId,      // 38
    kApostropheGrId,     // 39
    kLParenGrId,         // 40
    kRParenGrId,         // 41
    kAsteriskGrId,       // 42  
    kPlusGrId,           // 43
    kCommaGrId,          // 44
    kHyphenGrId,         // 45
    kPeriodGrId,         // 46
    kForwardSlashGrId,   // 47
    k0GrId,              // 48
    k1GrId,              // 49
    k2GrId,              // 50
    k3GrId,              // 51
    k4GrId,              // 52
    k5GrId,              // 53 
    k6GrId,              // 54
    k7GrId,              // 55
    k8GrId,              // 56
    k9GrId,              // 57
    kColonGrId,          // 58 
    kSemiColonGrId,      // 59
    kLesserGrId,         // 60
    kEqualGrId,          // 61
    kGreaterGrId,        // 62
    kQMarkGrId,          // 63
    kAtGrId,             // 64
    kA_GrId,             // 65
    kB_GrId,             // 66
    kC_GrId,             // 67
    kD_GrId,             // 68
    kE_GrId,             // 69
    kF_GrId,             // 70
    kG_GrId,             // 71
    kH_GrId,             // 72
    kI_GrId,             // 73
    kJ_GrId,             // 74
    kK_GrId,             // 75
    kL_GrId,             // 76
    kM_GrId,             // 77
    kN_GrId,             // 78
    kO_GrId,             // 79
    kP_GrId,             // 80
    kQ_GrId,             // 81
    kR_GrId,             // 82
    kS_GrId,             // 83
    kT_GrId,             // 84
    kU_GrId,             // 85
    kV_GrId,             // 86
    kW_GrId,             // 87
    kX_GrId,             // 88
    kY_GrId,             // 89
    kZ_GrId,             // 90
    kLBracketGrId,       // 91
    kBackSlashGrId,      // 92
    kRBracketGrId,       // 93
    kCaretGrId,          // 94
    kUnderScoreGrId,     // 95
    kAccentGrId,         // 96
    ka_GrId,             // 97
    kb_GrId,             // 98 
    kc_GrId,             // 99 
    kd_GrId,             // 100
    ke_GrId,             // 101
    kf_GrId,             // 102
    kg_GrId,             // 103
    kh_GrId,             // 104
    ki_GrId,             // 105
    kj_GrId,             // 106
    kk_GrId,             // 107
    kl_GrId,             // 108
    km_GrId,             // 109
    kn_GrId,             // 110
    ko_GrId,             // 111
    kp_GrId,             // 112
    kq_GrId,             // 113
    kr_GrId,             // 114
    ks_GrId,             // 115
    kt_GrId,             // 116
    ku_GrId,             // 117
    kv_GrId,             // 118
    kw_GrId,             // 119
    kx_GrId,             // 120
    ky_GrId,             // 121
    kz_GrId,             // 122
    kLBraceGrId,         // 123
    kPipeGrId,           // 124
    kRBraceGrId,         // 125
    kTildeGrId,          // 126
    kDeleteGrId,         // 127
    kNP_MultGrId,        // 128
    kNP_PlusGrId,        // 129
    kNP_MinusGrId,       // 130
    kNP_DecPtGrId,       // 131
    kNP_DivGrId,         // 132
    kNP_0GrId,           // 133
    kNP_1GrId,           // 134
    kNP_2GrId,           // 135
    kNP_3GrId,           // 136
    kNP_4GrId,           // 137
    kNP_5GrId,           // 138
    kNP_6GrId,           // 139
    kNP_7GrId,           // 140
    kNP_8GrId,           // 141
    kNP_9GrId,           // 142
    kNP_EqualGrId,       // 143
    kNP_EnterGrId,       // 144
    kFunc_1GrId,         // 145
    kFunc_2GrId,         // 146
    kFunc_3GrId,         // 147
    kFunc_4GrId,         // 148 
    kFunc_5GrId,         // 149
    kFunc_6GrId,         // 150
    kFunc_7GrId,         // 151
    kFunc_8GrId,         // 152
    kFunc_9GrId,         // 153
    kFunc_10GrId,        // 154
    kFunc_11GrId,        // 155
    kFunc_12GrId,        // 156
    kBrightUpGrId,       // 157
    kBrightDnGrId,       // 158
    kAudio_PrevGrId,     // 159
    kAudio_PlayGrId,     // 160
    kAudio_NextGrId,     // 161
    kAudio_MuteGrId,     // 162
    kAudio_UpGrId,       // 163
    kAudio_DnGrId,       // 164
    kEjectGrId,          // 165
    kInvalidKeyCodeGrId
  } cmGrKeyCodeId_t;

  enum
  {
    kMinAsciiGrId = kSpaceGrId,
    kMaxAsciiGrId = kDeleteGrId
  };

  enum
  {
    kOkGrRC,
    kLHeapFailGrRC,
    kAppErrGrRC,
    kRootObjCreateFailGrRC,
    kInvalidCoordsGrRC,
    kExtsErrGrRC
  };

  enum
  {
    kLeftGrFl   = 0x01,
    kTopGrFl    = 0x02,
    kRightGrFl  = 0x04,
    kBottomGrFl = 0x08,
  };

  typedef enum
  {
    kLeftGrIdx   = 0,   // min-x
    kTopGrIdx    = 1,   // max-y
    kRightGrIdx  = 2,   // max-x
    kBottomGrIdx = 3,   // min-y
    kAxisGrCnt   = 4
  } cmGrAxisIdx_t;


  typedef cmHandle_t cmGrH_t;
  typedef cmHandle_t cmGrObjH_t;
  typedef cmHandle_t cmGrDcH_t;
  typedef unsigned   cmGrRC_t;

  extern cmGrH_t     cmGrNullHandle;
  extern cmGrObjH_t  cmGrObjNullHandle;

  typedef cmReal_t cmGrV_t;

  //====================================================================================================
  
  // Calculate the width and height between two pixels.
  // This implies that the first and last pixel are inside the valid range.
#define cmGrXtoW(x0,x1)        (abs((x1)-(x0))+1)
#define cmGrWtoX(x0,w)         (((x0)+(w))-1)

#define cmGrYtoH(y0,y1)        (abs((y1)-(y0))+1)
#define cmGrHtoY(y0,h)         (((y0)+(h))-1)

#define cmGrPIsXInRange(x,x0,w) ((x0)<=(x)&&(x)<=cmGrWtoX((x0),(w)))
#define cmGrPIsYInRange(y,y0,h) ((y0)<=(y)&&(y)<=cmGrHtoY((y0),(h)))

#define cmGrVIsXInRange(x,x0,w) ((x0)<=(x)&&(x)<=((x0)+(w)))
#define cmGrVIsYInRange(y,y0,h) ((y0)<=(y)&&(y)<=((y0)+(h)))

  typedef struct
  {
    int x;
    int y;
  } cmGrPPt_t;

#define cmGrPPtSet( p, xx, yy ) do{ (p)->x=(xx); (p)->y=(yy); }while(0)
#define cmGrPPtIsEqual(p0,p1) ((p0)->x==(p1)->x && (p0)->y==(p1)->y)
#define cmGrPPtPrint(lbl,p) printf("%s x=%i y=%i\n",(lbl),(p)->x,(p)->y)

  //====================================================================================================
  typedef struct
  {
    int w;
    int h;
  } cmGrPSz_t;

#define   cmGrPSzSet(      s, ww, hh )          do{ (s)->w=(ww); (s)->h=(hh);}while(0)
#define   cmGrPSzSetD(     s, x0, y0, x1, y1 )  cmGrPSzSet(cmGrXtoW(x0,x1),cmGrYtoH(y0,y1))

#define   cmGrPSzSetEmpty( s )  ((s)->w = (s)->h =  0)
#define   cmGrPSzSetNull(  s )  ((s)->w = (s)->h = -1)
#define   cmGrPSzIsEmpty(  s )  ((s)->w== 0 && (s)->h== 0)
#define   cmGrPSzIsNull(   s )  ((s)->w==-1 && (s)->h==-1)
#define   cmGrPSzIsEqual(s0,s1) ((s0)->w==(s1)->w && (s0)->h==(s1)->h)
#define   cmGrPSzPrint(lbl,s) printf("%s w=%i h=%i\n",(lbl),(s)->w,(s)->h)

  //====================================================================================================
  typedef struct
  {
    cmGrPPt_t loc;
    cmGrPSz_t sz;
  } cmGrPExt_t;

#define   cmGrPExtSet( e, x, y, w, h )    do{ cmGrPPtSet(&(e)->loc,(x),(y)); cmGrPSzSet(&(e)->sz,(w),(h)); }while(0)
#define   cmGrPExtSetD(e, x0, y0, x1, y1) cmGrPExtSet(e,cmMin(x0,x1),cmMin(y0,y1),cmGrXtoW(x0,x1),cmGrYtoH(y0,y1))

#define   cmGrPExtL(e) ((e)->loc.x)
#define   cmGrPExtT(e) ((e)->loc.y)
#define   cmGrPExtR(e) (cmGrWtoX((e)->loc.x,(e)->sz.w))
#define   cmGrPExtB(e) (cmGrHtoY((e)->loc.y,(e)->sz.h))
#define   cmGrPExtW(e) ((e)->sz.w)
#define   cmGrPExtH(e) ((e)->sz.h)

#define   cmGrPExtSetL(e,v) ((e)->loc.x = (v))
#define   cmGrPExtSetT(e,v) ((e)->loc.y = (v))
#define   cmGrPExtSetR(e,v) cmGrPExtSetW(e,cmGrXtoW((e)->loc.x,(v)))
#define   cmGrPExtSetB(e,v) cmGrPExtSetH(e,cmGrYtoH((e)->loc.y,(v)))
#define   cmGrPExtSetW(e,v) ((e)->sz.w = (v))
#define   cmGrPExtSetH(e,v) ((e)->sz.h = (v))

#define   cmGrPExtCtrX(e)   ((e)->loc.x + (e)->sz.w / 2)
#define   cmGrPExtCtrY(e)   ((e)->loc.y + (e)->sz.h / 2)
#define   cmGrPExtCtr(e,pt) do{ (pt)->x=cmGrPExtCtrX(e); (pt)->y=cmGrPExtCtrY(e); }while(0)

#define   cmGrPExtSetEmpty( e ) do{ cmGrPSzSetEmpty(&(e)->sz); cmGrPPtSet(&(e)->loc,0,0); }while(0)
#define   cmGrPExtSetNull(  e ) do{ cmGrPSzSetNull( &(e)->sz); cmGrPPtSet(&(e)->loc,0,0); }while(0)
#define   cmGrPExtIsEmpty(  e ) cmGrPSzIsEmpty( &(e)->sz )
#define   cmGrPExtIsNull(   e ) cmGrPSzIsNull(  &(e)->sz )
#define   cmGrPExtIsNullOrEmpty(e) (cmGrPExtIsNull(e)||cmGrPExtIsEmpty(e))
#define   cmGrPExtIsNotEmpty(e) (!cmGrPExtIsEmpty(e))
#define   cmGrPExtIsNotNull(e)  (!cmGrPExtIsNull(e))
#define   cmGrPExtIsNotNullOrEmpty(e) (cmGrPExtIsNotNull(e)||cmGrPExtIsNoEmpty(e))

#define   cmGrPExtIsEqual( e0, e1 ) (cmGrPPtIsEqual(&(e0)->loc,&(e1)->loc) && cmGrPSzIsEqual(&(e0)->sz, &(e1)->sz))

#define   cmGrPExtIsXyInside( e, xx, yy) (cmGrPIsXInRange((xx),(e)->loc.x,(e)->sz.w) && cmGrPIsYInRange((yy), (e)->loc.y, (e)->sz.h) )
#define   cmGrPExtIsPtInside( e, pt )  (cmGrPExtIsXyInside((e),(pt)->x,(pt)->y))
#define   cmGrPExtIsExtInside(e0, e1)  (cmGrPExtIsPtInside((e0),&((e1)->loc)) && cmGrPExtIsXyInside((e0), cmGrWtoX((e1)->loc.x,(e1)->sz.w), cmGrHtoY((e1)->loc.y,(e1)->sz.h)))

#define   cmGrPExtExpand(e,l,t,r,b) do{(e)->loc.x+=(l); (e)->loc.y+=(t); (e)->sz.w+=(abs(l)+abs(r)); (e)->sz.h+=(abs(t)+abs(b));}while(0)

#define   cmGrPExtRpt(e,rpt) cmRptPrintf(rpt,"x:%i y:%i w:%i h:%i",(e)->loc.x,(e)->loc.y,(e)->sz.w,(e)->sz.h)
#define   cmGrPExtPrint(lbl,e) printf("%s %i %i %i %i\n",lbl,(e)->loc.x,(e)->loc.y,(e)->sz.w,(e)->sz.h)

  void      cmGrPExtIntersect( cmGrPExt_t* r, const cmGrPExt_t* e0, const cmGrPExt_t* e1 );
   

  //====================================================================================================

  typedef struct
  {
    cmGrV_t x;
    cmGrV_t y;
  } cmGrVPt_t;

#define cmGrVPtSet( p, xx, yy ) do{ (p)->x=(xx); (p)->y=(yy); }while(0)
#define cmGrVPtIsEqual(p0,p1) ((p0)->x==(p1)->x && (p0)->y==(p1)->y)
#define cmGrVPtIsNotEqual(p0,p1) (!cmGrVPtIsEqual(p0,p1))

  //====================================================================================================
  typedef struct
  {
    cmGrV_t w;
    cmGrV_t h;
  } cmGrVSz_t;

#define   cmGrVSzSet(      s, ww, hh )          do{ (s)->w=(ww); (s)->h=(hh);}while(0)
#define   cmGrVSzSetD(     s, x0, y0, x1, y1 )  cmGrVSzSet((x1)-(x0),(y1)-(y0))

#define   cmGrVSzSetEmpty( s ) ((s)->w = (s)->h =  0)
#define   cmGrVSzSetNull(  s ) ((s)->w = (s)->h = -1)
#define   cmGrVSzIsEmpty(  s ) ((s)->w== 0 && (s)->h== 0)
#define   cmGrVSzIsNull(   s ) ((s)->w==-1 || (s)->h==-1)
#define   cmGrVSzIsEqual(s0,s1) ((s0)->w==(s1)->w && (s0)->h==(s1)->h)

  //====================================================================================================
  typedef struct
  {
    cmGrVPt_t loc;  
    cmGrVSz_t sz;
  } cmGrVExt_t;

#define   cmGrVExtIsNorm( e )             ((e)->sz.w>=0 && (e)->sz.h>=0)
#define   cmGrVExtNorm(   e )             do{ if( cmGrVExtIsNotNull(e) ){ if((e)->sz.w<0){(e)->loc.x += (e)->sz.w; (e)->sz.w*=-1;} if((e)->sz.h<0){(e)->loc.y += (e)->sz.h; (e)->sz.h*=-1;}} }while(0)
#define   cmGrVExtSet( e, x, y, w, h )    do{ cmGrVPtSet(&(e)->loc,(x),(y)); cmGrVSzSet(&(e)->sz,(w),(h)); cmGrVExtNorm(e); }while(0)
#define   cmGrVExtSetD(e, x0, y0, x1, y1) cmGrVExtSet((e),(x0),(y0),(x1)-(x0),(y1)-(y0))

  // 
  //  l,t      minx,maxy   
  //      r,b            maxx,miny
  //
#define   cmGrVExtMinX(e) ((e)->loc.x)
#define   cmGrVExtMinY(e) ((e)->loc.y)
#define   cmGrVExtMaxX(e) ((e)->loc.x + (e)->sz.w)
#define   cmGrVExtMaxY(e) ((e)->loc.y + (e)->sz.h)
#define   cmGrVExtW(e) ((e)->sz.w)
#define   cmGrVExtH(e) ((e)->sz.h)

#define   cmGrVExtSetMinX(e,v) ((e)->loc.x = (v))
#define   cmGrVExtSetMinY(e,v) ((e)->loc.y = (v))

  // Beware: setting maxx and maxy depends on the current value of minx and miny.
  // If both minx and maxx are being changed then be sure to set minx first.
  // If both miny and maxy are being changed then be sure to set miny first.
#define   cmGrVExtSetMaxX(e,v) ((e)->sz.w  = (v) - cmGrVExtMinX(e))
#define   cmGrVExtSetMaxY(e,v) ((e)->sz.h  = (v) - cmGrVExtMinY(e))
#define   cmGrVExtSetW(e,v)    ((e)->sz.w  = (v))
#define   cmGrVExtSetH(e,v)    ((e)->sz.h  = (v))

#define   cmGrVExtSetEmpty( e )       do{ cmGrVSzSetEmpty(&(e)->sz); cmGrVPtSet(&(e)->loc,0,0); }while(0)
#define   cmGrVExtSetNull(  e )       do{ cmGrVSzSetNull(&(e)->sz);  cmGrVPtSet(&(e)->loc,0,0); }while(0)
#define   cmGrVExtIsEmpty(  e )       cmGrVSzIsEmpty(&(e)->sz)
#define   cmGrVExtIsNull(   e )       cmGrVSzIsNull( &(e)->sz)
#define   cmGrVExtIsNullOrEmpty(e)    (cmGrVExtIsNull(e)||cmGrVExtIsEmpty(e))
#define   cmGrVExtIsNotEmpty(e)       (!cmGrVExtIsEmpty(e))
#define   cmGrVExtIsNotNull(e)        (!cmGrVExtIsNull(e))
#define   cmGrVExtIsNotNullOrEmpty(e) (cmGrVExtIsNotNull(e)&&cmGrVExtIsNotEmpty(e))
#define   cmGrVExtIsEqual( e0, e1 )   (cmGrVPtIsEqual(&(e0)->loc,&(e1)->loc) && cmGrVSzIsEqual(&(e0)->sz, &(e1)->sz))


#define   cmGrVExtIsXyInside( e, xx, yy) (cmGrVIsXInRange((xx),(e)->loc.x,(e)->sz.w) && cmGrVIsYInRange((yy),(e)->loc.y,(e)->sz.h))
#define   cmGrVExtIsPtInside( e, pt )    (cmGrVExtIsXyInside((e),(pt)->x,(pt)->y))

  // e1 is inside e0
#define   cmGrVExtIsExtInside(e0, e1)    (cmGrVExtIsXyInside((e0),cmGrVExtMinX(e1),cmGrVExtMinY(e1)) && cmGrVExtIsXyInside((e0), cmGrVExtMaxX(e1), cmGrVExtMaxY(e1)))

#define   cmGrVExtRpt(e,rpt) cmRptPrintf(rpt,"x:%f y:%f w:%f h:%f",(e)->loc.x,(e)->loc.y,(e)->sz.w,(e)->sz.h)
#define   cmGrVExtPrint(lbl,e) printf("%s %f %f %f %f\n",lbl,(e)->loc.x,(e)->loc.y,(e)->sz.w,(e)->sz.h)


  // Shift and expand e0 to contain e1.  Return true if e0 actually changes.
  bool    cmGrVExtExpandToContain( cmGrVExt_t* e0, const cmGrVExt_t* e1 );

  // Force e1 to be contained by e0 by shifting e1's location. This function
  // will never change the width or height of e1. Return true if e1 is changed.
  bool    cmGrVExtContain( const cmGrVExt_t* e0, cmGrVExt_t* e1 );

  // Return the intersection of 'e0' with 'e1' in 'r'. 
  void    cmGrVExtIntersect( cmGrVExt_t* r, const cmGrVExt_t* e0, const cmGrVExt_t* e1 );

  //====================================================================================================
  

#define cmGrRgbToColor( r, g, b )  (((r) << 16) + ((g) << 8) + (b))
#define cmGrColorToR(   c )        (((c) >> 16) & 0x000000ff) 
#define cmGrColorToG(   c )        (((c) >>  8) & 0x000000ff) 
#define cmGrColorToB(   c )        (((c)      ) & 0x000000ff) 

  typedef unsigned cmGrColor_t;
  enum { kGrDefaultColorMapIdx = 0, kGrDefaultColorMapId=0 };

  unsigned        cmGrColorMapCount(    cmGrH_t grH );
  unsigned        cmGrColorMapId(       cmGrH_t grH, unsigned mapIdx );  
  const cmChar_t* cmGrColorMapLabel(    cmGrH_t grH, unsigned id );
  unsigned        cmGrColorMapRegister( cmGrH_t grH, cmChar_t* label, const cmGrColor_t* array, unsigned cnt );     
  cmGrColor_t*    cmGrColorMap(         cmGrH_t grH, unsigned mapId );
  unsigned        cmGrColorMapEleCount( cmGrH_t grH, unsigned mapId );

  //====================================================================================================
  typedef struct
  {
    cmCtx_t*   ctx;          // application context
    cmGrH_t    grH;          // graphics system handle to which this graphic object belongs
    cmGrObjH_t objH;         // this graphics object handle
    void*      cbArg;        // user callback arg

    cmGrPPt_t  msDnPPt;      // mouse down phys point
    cmGrVPt_t  msDnVPt;      // mouse down virt point inside op->parent->wext
    cmGrVSz_t  msDnVOffs;    // virtual offset from mouse down point to msDnObj->vext
    cmGrObjH_t msDnObjH;     // handle of object which recv'd mouse down
    cmGrVPt_t  msVPt;        // cur ms virtual point
    
  } cmGrObjFuncArgs_t;


  typedef cmGrRC_t (*cmGrCreateObjCb_t)(   cmGrObjFuncArgs_t* args );
  typedef void     (*cmGrDestroyObjCb_t)(  cmGrObjFuncArgs_t* args );
  typedef bool     (*cmGrRenderObjCb_t)(   cmGrObjFuncArgs_t* args, cmGrDcH_t dcH );
  typedef int      (*cmGrDistanceObjCb_t)( cmGrObjFuncArgs_t* args, int x, int y );
  typedef bool     (*cmGrEventObjCb_t)(    cmGrObjFuncArgs_t* args, unsigned flags, unsigned key, int px, int py  );  
  typedef void     (*cmGrVExtObjCb_t)(     cmGrObjFuncArgs_t* args, cmGrVExt_t* vext );
  typedef bool     (*cmGrIsInsideObjCb_t)( cmGrObjFuncArgs_t* args, unsigned evtFlags, int px, int py, cmGrV_t vx, cmGrV_t vy );

  typedef struct cmGrObjFunc_str
  {
    // User defined constructor.
    cmGrCreateObjCb_t  createCbFunc;
    void*              createCbArg;

    // User defined destructor.
    cmGrDestroyObjCb_t destroyCbFunc;
    void*              destroyCbArg;

    // Draw the object by calling back to the cmGrDrawXXX() functions
    cmGrRenderObjCb_t  renderCbFunc;
    void*              renderCbArg;

    // Return the physical distance from a physical view location to the object.
    // (NOT USED)
    cmGrDistanceObjCb_t distanceCbFunc;
    void*               distanceCbArg;

    // Handle an event. gx,gy are in the same coord's as args.objH.vext (they are inside args.objH.parent.wext).
    // Return true if the event objects dirty flag should be set.
    cmGrEventObjCb_t    eventCbFunc;
    void*               eventCbArg;

    // Return the objects location and size inside op->parent->wext
    cmGrVExtObjCb_t     vextCbFunc;
    void*               vextCbArg;

    // Called to determine which object is under the mouse and whether the event can
    // handle the event as described by the 'evtFlags' args.
    // Return true if the point is inside this obj.  vx,vy is in the the same coord's 
    // as op->vext (i.e. vx,vy is inside op->parent->wext) and the object will accept
    // the event implied by the 'evtFlags' argument.  
    // The simple answer to this call is cmGrVExtIsXyInside( *vext, vx, vy ).
    cmGrIsInsideObjCb_t isInsideCbFunc;
    void*               isInsideCbArg;
  } cmGrObjFunc_t;


  // Create a graphic object. This function calls the user defined (*create)() function.
  cmGrRC_t   cmGrObjCreate(     cmGrH_t h, cmGrObjH_t* hp, cmGrObjH_t parentH, cmGrObjFunc_t* f, unsigned id, unsigned flags, const cmGrVExt_t* wext );

  // Destroy a graphic object and all of it's children. 
  // This function calls the user defined (*destroy)() function.
  cmGrRC_t   cmGrObjDestroy(    cmGrH_t h, cmGrObjH_t* hp );

  // Return true if 'oh' is a valid handle.
  cmGrRC_t   cmGrObjIsValid(    cmGrH_t h, cmGrObjH_t  oh );

  // Return the user id associated with this object.
  unsigned   cmGrObjId(        cmGrObjH_t oh );
  void       cmGrObjSetId(     cmGrObjH_t oh, unsigned id );

  // Return the handle to the parent object.
  cmGrObjH_t cmGrObjParent(    cmGrObjH_t oh );

  // An object world coord's are used to place child objects.
  cmGrRC_t   cmGrObjSetWorldExt( cmGrH_t h, cmGrObjH_t oh, const cmGrVExt_t* vext );
  void       cmGrObjWorldExt( cmGrObjH_t oh, cmGrVExt_t* vext );

  cmGrRC_t   cmGrObjSetWorldLimitExt( cmGrH_t h, cmGrObjH_t oh, const cmGrVExt_t* vext, unsigned limitFlags );
  void       cmGrObjWorldLimitExt(  cmGrObjH_t oh, cmGrVExt_t* vext, unsigned* limitFlags );

  void                cmGrObjSetCreateCb(   cmGrObjH_t oh, cmGrCreateObjCb_t   cbFunc, void* cbArg );
  void                cmGrObjSetDestroyCb(  cmGrObjH_t oh, cmGrDestroyObjCb_t  cbFunc, void* cbArg );
  void                cmGrObjSetRenderCb(   cmGrObjH_t oh, cmGrRenderObjCb_t   cbFunc, void* cbArg );
  void                cmGrObjSetDistanceCb( cmGrObjH_t oh, cmGrDistanceObjCb_t cbFunc, void* cbArg );
  void                cmGrObjSetEventCb(    cmGrObjH_t oh, cmGrEventObjCb_t    cbFunc, void* cbArg );
  void                cmGrObjSetVExtCb(     cmGrObjH_t oh, cmGrVExtObjCb_t     cbFunc, void* cbArg );
  void                cmGrObjSetIsInsideCb( cmGrObjH_t oh, cmGrIsInsideObjCb_t cbFunc, void* cbArg );

  cmGrCreateObjCb_t   cmGrObjCreateCbFunc(   cmGrObjH_t oh );
  cmGrDestroyObjCb_t  cmGrObjDestroyCbFunc(  cmGrObjH_t oh );
  cmGrRenderObjCb_t   cmGrObjRenderCbFunc(   cmGrObjH_t oh );
  cmGrDistanceObjCb_t cmGrObjDistanceCbFunc( cmGrObjH_t oh );
  cmGrEventObjCb_t    cmGrObjEventCbFunc(    cmGrObjH_t oh );
  cmGrVExtObjCb_t     cmGrObjVExtCbFunc(     cmGrObjH_t oh );
  cmGrIsInsideObjCb_t cmGrObjIsInsideCbFunc( cmGrObjH_t oh );

  void*               cmGrObjCreateCbArg(   cmGrObjH_t oh );
  void*               cmGrObjDestroyCbArg(  cmGrObjH_t oh );
  void*               cmGrObjRenderCbArg(   cmGrObjH_t oh );
  void*               cmGrObjDistanceCbArg( cmGrObjH_t oh );
  void*               cmGrObjEventCbArg(    cmGrObjH_t oh );
  void*               cmGrObjVExtCbArg(     cmGrObjH_t oh );
  void*               cmGrObjIsInsideCbArg( cmGrObjH_t oh );
  

  // Same as call to user defined (*vect)(). 
  void       cmGrObjLocalVExt( cmGrH_t h, cmGrObjH_t oh, cmGrVExt_t* vext );  

  // Given an objects id return it's handle.
  cmGrObjH_t cmGrObjIdToHandle( cmGrH_t h, unsigned id );

  // Move 'aoH' such that it is drawn above 'boH' in the z-order.
  // This means that 'boH' will be drawn before 'aoH'.
  void       cmGrObjDrawAbove( cmGrObjH_t boH, cmGrObjH_t aoH );

  void       cmGrObjReport(     cmGrH_t h, cmGrObjH_t oh, cmRpt_t* rpt ); 
  void       cmGrObjReportR(    cmGrH_t h, cmGrObjH_t oh, cmRpt_t* rpt ); // print children


  //====================================================================================================
  // Drawing Functions - called by objects to draw themselves

  int      cmGrX_VtoP(     cmGrH_t hh, cmGrObjH_t oh, cmGrV_t y );
  int      cmGrY_VtoP(     cmGrH_t hh, cmGrObjH_t oh, cmGrV_t x );

  void     cmGrXY_VtoP(    cmGrH_t hh, cmGrObjH_t oh, cmGrV_t x, cmGrV_t y, cmGrPPt_t* rp );
  void     cmGrXYWH_VtoP(  cmGrH_t hh, cmGrObjH_t oh, cmGrV_t x, cmGrV_t y, cmGrV_t w, cmGrV_t h, cmGrPExt_t* pext ); 
  void     cmGrVExt_VtoP(  cmGrH_t hh, cmGrObjH_t oh, const cmGrVExt_t* vext, cmGrPExt_t* pext );

  void     cmGrXY_PtoV(    cmGrH_t hh, cmGrObjH_t oh, int x, int y, cmGrVPt_t* rp );
  void     cmGrXYWH_PtoV(  cmGrH_t hh, cmGrObjH_t oh, int x, int y, int w, int h, cmGrVExt_t* vext );
  void     cmGrPExt_PtoV(  cmGrH_t hh, cmGrObjH_t oh, const cmGrPExt_t* pext, cmGrVExt_t* vext );
  
  void     cmGrDrawVLine( cmGrH_t hh, cmGrDcH_t dcH, cmGrObjH_t oh, cmGrV_t x0, cmGrV_t y0, cmGrV_t x1, cmGrV_t y1 );
  void     cmGrDrawVRect( cmGrH_t hh, cmGrDcH_t dcH, cmGrObjH_t oh, cmGrV_t x,  cmGrV_t y,  cmGrV_t w, cmGrV_t h );

  //====================================================================================================

  // Callback identifiers
  typedef enum
  {
    kCreateCbGrId,
    kDestroyCbGrId,
    kLocalPtCbGrId,
    kGlobalPtCbGrId,
    kPhysExtCbGrId,
    kViewExtCbGrId,
    kSelectExtCbGrId,
    kFocusCbGrId,
    kKeyUpCbGrId,
    kKeyDnCbGrId
  } cmGrCbId_t;

  // Callback function associated with this canvas.
  typedef void (*cmGrCbFunc_t)( void* arg, cmGrH_t grH, cmGrCbId_t id, unsigned evtFlags, cmGrKeyCodeId_t keycode ); 

  // Configuration Flags
  enum
  {
    kExpandViewGrFl = 0x01,  // expand the view to show new objects
    kSelectHorzGrFl = 0x02,  // select along x-axis only
    kSelectVertGrFl = 0x04   // select along y-axis only
  };

  // 'wext' is optional. 
  // 'id' is an arbitrary user definable identifier - although it is used
  // as the view index by cmGrPage().
  cmGrRC_t cmGrCreate( 
    cmCtx_t*          ctx, 
    cmGrH_t*          hp, 
    unsigned          id, 
    unsigned          cfgFlags, 
    cmGrCbFunc_t      cbFunc,
    void*             cbArg,  
    const cmGrVExt_t* wext  );   // Optional internal world extents for this object

  // Destroy this canvas.
  cmGrRC_t cmGrDestroy( cmGrH_t* hp );

  // Remove all objects from the root object and restore the canvas to it's default state.
  cmGrRC_t cmGrClear( cmGrH_t h );

  // Get the root object handle
  cmGrObjH_t cmGrRootObjH( cmGrH_t h );

  // Get and set the configuration flags (e.g. kExpandViewGrFl | kSelectHorzGrFl | kSelectVertHorzGrFl )
  unsigned cmGrCfgFlags( cmGrH_t h );
  void     cmGrSetCfgFlags( cmGrH_t h, unsigned cfgFlags );

  // Draw the objects on the canvas.
  cmGrRC_t cmGrDraw( cmGrH_t h, cmGrDcH_t dcH );

  // event flags
  enum
  {
    kMsDownGrFl = 0x0001,
    kMsUpGrFl   = 0x0002,
    kMsMoveGrFl = 0x0004,
    kMsWheelGrFl= 0x0008,
    kMsDragGrFl = 0x0010,
    kMsClickGrFl= 0x0020,
    kKeyDnGrFl  = 0x0040,
    kKeyUpGrFl  = 0x0080,

    kMsEvtMask  = 0x02f,
    kEvtMask    = 0x00ff,

    kMsLBtnGrFl = 0x0100,
    kMsCBtnGrFl = 0x0200,
    kMsRBtnGrFl = 0x0400,
    
    kShiftKeyGrFl = 0x0800,
    kAltKeyGrFl   = 0x1000,
    kCtlKeyGrFl   = 0x2000,
  };

  // Receive a UI event.
  bool     cmGrEvent(  cmGrH_t h, unsigned flags, cmGrKeyCodeId_t key, int x, int y );

  // Return true if 'h' is valid.
  bool     cmGrIsValid( cmGrH_t h );

  // Return the user defined 'id' set in cmGrCreate()
  unsigned cmGrId( cmGrH_t h );

  // Return the last mouse location in root object coordinates.
  const cmGrVPt_t* cmGrGlobalPt( cmGrH_t h );

  // Return the last mouse location in coordinates of the object the mouse was over.
  const cmGrVPt_t* cmGrLocalPt( cmGrH_t h );
  

  // The new view extents must fit inside the world extents.
  // Return true if the view extents actually changed.
  bool     cmGrSetViewExtents( cmGrH_t hh, cmGrV_t minx, cmGrV_t miny, cmGrV_t maxx, cmGrV_t maxy );
  bool     cmGrSetViewExtentsE(cmGrH_t h,  const cmGrVExt_t* ext );
  void     cmGrViewExtents( cmGrH_t h, cmGrVExt_t* exts );
  
  // View Location
  // Return true if the phys extents actually changed.
  bool     cmGrSetPhysExtents( cmGrH_t hh, int x, int y, int w, int h );
  bool     cmGrSetPhysExtentsE(cmGrH_t h, const cmGrPExt_t* ext );
  void     cmGrPhysExtents( cmGrH_t h, cmGrPExt_t* exts );

  // Return some scroll bar values for this canvas.
  // tot=world pixels, vis=vis pixels, max=max scroll pos  pos=cur scroll pos
  // All return values are optional.
  void      cmGrScrollExtents( cmGrH_t h, cmGrPSz_t* tot, cmGrPSz_t* vis, cmGrPSz_t* max, cmGrPPt_t* pos );

  // Return true if the view location actually changed.
  bool      cmGrSetScrollH( cmGrH_t h, int x );
  int       cmGrScrollH(    cmGrH_t h );
  bool      cmGrSetScrollV( cmGrH_t h, int y );
  int       cmGrScrollV(    cmGrH_t h );

  // Get the current selection extents.
  // If the selection extents are not valid then the function returns false
  // and sets the return extents to their null state.
  bool      cmGrSelectExtents( cmGrH_t h, cmGrVExt_t* vext, cmGrPExt_t* pext );

  // Both pts are optional
  void      cmGrSetSelectPoints(cmGrH_t h, const cmGrVPt_t* pt0, const cmGrVPt_t* pt1 );
  void      cmGrSelectPoints(   cmGrH_t h, cmGrVPt_t* pt0, cmGrVPt_t* pt1 );

  enum { kZoomInGrFl=0x01, kXAxisGrFl=0x02, kYAxisGrFl=0x04, kSelectGrFl=0x08, kShowAllGrFl=0x10 };

  // 1) If kSelectGrFl is not set then the center 1/3 of the current view
  // becomes the new view.
  // 2) If kSelectGrFl is set then the selection area becomes the view.
  // 3) If kSelectGrFl is set but no selection area exists then 
  // option 1) is selected used and using the selection point as center.
  void      cmGrZoom( cmGrH_t h, unsigned flags );

  // Synchronize the 'syncGrH'  horz. and/or verical, world,view,select extents to
  // this gr's  extents. Changes to this gr's extents will be automatically
  // applied to 'syncGrH'.
  // If 'syncGrH' was used in a previous call to this function then flags will
  // modify the previously set flags value. 
  // Clear the kHorzSyncFl and kVertSyncFl to disable the synchronization.
  // Set flags to 0 to prevent future sync calls.
  enum { kWorldSyncGrFl=0x01, kViewSyncGrFl=0x02, kSelectSyncGrFl=0x04, kHorzSyncGrFl=0x08, kVertSyncGrFl=0x10 };
  void cmGrSetSync( cmGrH_t h, cmGrH_t syncGrH, unsigned flags );
  

  void     cmGrReport( cmGrH_t h, cmRpt_t* rpt );

  
#ifdef __cplusplus
}
#endif

#endif
