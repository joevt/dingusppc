/*
	File:		GraphicsHALDingusVideoPCI.c

	Contains:	This file contains the items needed to implement the Graphics HAL.

	The graphics hardware is characterized as follows:

		The graphics controller can support arbitrary resolutions.

		There is support for hardware cursor.

		The CLUT is a simple 'triple 8x256' CLUT.

		A hardware cursor is implemented, supporting indexed and direct color.

		The timing generator supports arbitrary pixel clocks.
*/

#include "GraphicsHAL.h"
#include "GraphicsPriv.h"
#include "GraphicsCoreUtils.h"		// for GraphicsUtilMapSenseCodesToDisplayCode()
#include "GraphicsOSS.h"			// for GraphicsOSS Set/Get HALPref
#include "GraphicsCoreControl.h"

#include <Devices.h>
#include <DriverServices.h>
#include <Errors.h>
#include <Files.h>					// for stages of development, alpha, beta, etc
#include <Kernel.h>					// for DelayForHardware()
#include <PCI.h>
#include <Traps.h>
#include <Types.h>
#include <Video.h>
#include <VideoServices.h>

#include "vbe.h"

#include "dingusvideoregs.h"

#include "GraphicsHALDingusVideoPCI.h"


enum
{
	kHardwareCursorImageSize = 256 * 256,		// Size of hardware cursor image in bytes
	kNumHardwareCursorColors = 254				// Number of colors for hardware cursor
};


//
// DingusVideoMetaRegisters
//
typedef struct DingusVideoRegisters DingusVideoRegisters;
struct DingusVideoRegisters
{
	HWRegister32Bit     regsDisplay[MAX_DISPLAY_REG];
};

typedef struct DingusVideoMetaRegisters DingusVideoMetaRegisters;
struct DingusVideoMetaRegisters
{
	HWRegister32Bit			regsMeta[MAX_META_REG];
	DingusVideoRegisters	displays[];
};



UInt32 EstablishedTimingsIandII[] = {
	kDisplay_720x400_70Hz          ,
	kDisplay_720x400_88Hz          ,
	kDisplay_640x480_60Hz_VGA      ,
	kDisplay_640x480_67Hz          ,
	kDisplay_640x480_72Hz          ,
	kDisplay_640x480_75Hz          ,
	kDisplay_800x600_56Hz_VGA      ,
	kDisplay_800x600_60Hz_VGA      ,
	kDisplay_800x600_72Hz_VGA      ,
	kDisplay_800x600_75Hz_VGA      ,
	kDisplay_832x624_75Hz          ,
	kDisplay_1024x768i_87Hz        ,
	kDisplay_1024x768_60Hz_VGA     ,
	kDisplay_1024x768_70Hz         ,
	kDisplay_1024x768_75Hz_VGA     ,
	kDisplay_1280x1024_75Hz        ,
	kDisplay_1152x870_75Hz         ,
};

UInt32 EstablishedTimingsIII[] = {
	kDisplay_640x350_85Hz          ,
	kDisplay_640x400_85Hz          ,
	kDisplay_720x400_85Hz          ,
	kDisplay_640x480_85Hz          ,
	kDisplay_848x480_60Hz          ,
	kDisplay_800x600_85Hz          ,
	kDisplay_1024x768_85Hz         ,
	kDisplay_1152x864_75Hz         ,
	kDisplay_1280x768_60Hz_RB      ,
	kDisplay_1280x768_59_87Hz      ,
	kDisplay_1280x768_75Hz         ,
	kDisplay_1280x768_85Hz         ,
	kDisplay_1280x960_60Hz         ,
	kDisplay_1280x960_85Hz         ,
	kDisplay_1280x1024_60Hz        ,
	kDisplay_1280x1024_85Hz        ,
	kDisplay_1360x768_60Hz         ,
	kDisplay_1440x900_60Hz_RB      ,
	kDisplay_1440x900_60Hz         ,
	kDisplay_1440x900_75Hz         ,
	kDisplay_1440x900_85Hz         ,
	kDisplay_1400x1050_60Hz_RB     ,
	kDisplay_1400x1050_60Hz        ,
	kDisplay_1400x1050_75Hz        ,
	kDisplay_1400x1050_85Hz        ,
	kDisplay_1680x1050_60Hz_RB     ,
	kDisplay_1680x1050_60Hz        ,
	kDisplay_1680x1050_75Hz        ,
	kDisplay_1680x1050_85Hz        ,
	kDisplay_1600x1200_60Hz        ,
	kDisplay_1600x1200_65Hz        ,
	kDisplay_1600x1200_70Hz        ,
	kDisplay_1600x1200_75Hz        ,
	kDisplay_1600x1200_85Hz        ,
	kDisplay_1792x1344_60Hz        ,
	kDisplay_1792x1344_75Hz        ,
	kDisplay_1856x1392_60Hz        ,
	kDisplay_1856x1392_75Hz        ,
	kDisplay_1920x1200_60Hz_RB     ,
	kDisplay_1920x1200_60Hz        ,
	kDisplay_1920x1200_75Hz        ,
	kDisplay_1920x1200_85Hz        ,
	kDisplay_1920x1440_60Hz        ,
	kDisplay_1920x1440_75Hz        ,
};

UInt32 DisplayMonitorTimings[] = {
	kDisplay_640x350_85Hz          ,
	kDisplay_640x400_85Hz          ,
	kDisplay_720x400_85Hz          ,
	kDisplay_640x480_60Hz_VGA      ,
	kDisplay_640x480_72Hz          ,
	kDisplay_640x480_75Hz          ,
	kDisplay_640x480_85Hz          ,
	kDisplay_800x600_56Hz_VGA      ,
	kDisplay_800x600_60Hz_VGA      ,
	kDisplay_800x600_72Hz_VGA      ,
	kDisplay_800x600_75Hz_VGA      ,
	kDisplay_800x600_85Hz          ,
	kDisplay_800x600_120Hz         ,
	kDisplay_848x480_60Hz          ,
	kDisplay_1024x768i_87Hz        ,
	kDisplay_1024x768_60Hz_VGA     ,
	kDisplay_1024x768_70Hz         ,
	kDisplay_1024x768_75Hz_VGA     ,
	kDisplay_1024x768_85Hz         ,
	kDisplay_1024x768_120Hz        ,
	kDisplay_1152x864_75Hz         ,
	kDisplay_1280x720_60Hz         ,
	kDisplay_1280x768_60Hz_RB      ,
	kDisplay_1280x768_59_87Hz      ,
	kDisplay_1280x768_75Hz         ,
	kDisplay_1280x768_85Hz         ,
	kDisplay_1280x768_120Hz        ,
	kDisplay_1280x800_60Hz_RB      ,
	kDisplay_1280x800_60Hz         ,
	kDisplay_1280x800_75Hz         ,
	kDisplay_1280x800_85Hz         ,
	kDisplay_1280x800_120Hz        ,
	kDisplay_1280x960_60Hz         ,
	kDisplay_1280x960_85Hz         ,
	kDisplay_1280x960_120Hz        ,
	kDisplay_1280x1024_60Hz        ,
	kDisplay_1280x1024_75Hz        ,
	kDisplay_1280x1024_85Hz        ,
	kDisplay_1280x1024_120Hz       ,
	kDisplay_1360x768_60Hz         ,
	kDisplay_1360x768_120Hz        ,
	kDisplay_1366x768_60Hz         ,
	kDisplay_1366x768_60Hz_RB      ,
	kDisplay_1400x1050_60Hz_RB     ,
	kDisplay_1400x1050_60Hz        ,
	kDisplay_1400x1050_75Hz        ,
	kDisplay_1400x1050_85Hz        ,
	kDisplay_1400x1050_120Hz       ,
	kDisplay_1440x900_60Hz_RB      ,
	kDisplay_1440x900_60Hz         ,
	kDisplay_1440x900_75Hz         ,
	kDisplay_1440x900_85Hz         ,
	kDisplay_1440x900_120Hz        ,
	kDisplay_1600x900_60Hz         ,
	kDisplay_1600x1200_60Hz        ,
	kDisplay_1600x1200_65Hz        ,
	kDisplay_1600x1200_70Hz        ,
	kDisplay_1600x1200_75Hz        ,
	kDisplay_1600x1200_85Hz        ,
	kDisplay_1600x1200_120Hz       ,
	kDisplay_1680x1050_60Hz_RB     ,
	kDisplay_1680x1050_60Hz        ,
	kDisplay_1680x1050_75Hz        ,
	kDisplay_1680x1050_85Hz        ,
	kDisplay_1680x1050_120Hz       ,
	kDisplay_1792x1344_60Hz        ,
	kDisplay_1792x1344_75Hz        ,
	kDisplay_1792x1344_120Hz       ,
	kDisplay_1856x1392_60Hz        ,
	kDisplay_1856x1392_75Hz        ,
	kDisplay_1856x1392_120Hz       ,
	kDisplay_1920x1080_60Hz        ,
	kDisplay_1920x1200_60Hz_RB     ,
	kDisplay_1920x1200_60Hz        ,
	kDisplay_1920x1200_75Hz        ,
	kDisplay_1920x1200_85Hz        ,
	kDisplay_1920x1200_120Hz       ,
	kDisplay_1920x1440_60Hz        ,
	kDisplay_1920x1440_75Hz        ,
	kDisplay_1920x1440_120Hz       ,
	kDisplay_2048x1152_60Hz        ,
	kDisplay_2560x1600_60Hz_RB     ,
	kDisplay_2560x1600_60Hz        ,
	kDisplay_2560x1600_75Hz        ,
	kDisplay_2560x1600_85Hz        ,
	kDisplay_2560x1600_120Hz       ,
	kDisplay_4096x2160_60Hz_RB     ,
	kDisplay_4096x2160_59_94Hz     ,
};

UInt32 VideoIdentificationCodes[219] = { 0 };


//	An explanation of interrupts for Template:
//	interrupt sources: vbl
//	When vbl interrupts are enabled, generate an interrupt at every vertical blanking period.
//
//	All interrupts get funneled to the same bit in the IO controller.  If more than one interrupt
//	source was enabled, the HAL would need to examine each interrupt source to determine
//	what caused the interrupt.  The HAL cares
//	about "vbl interrupts" i.e. generate an interrupt when the entire screen has been drawn.  If
//	vbl interrupts are enabled, the interrupt will automatically be generated every time this occurs.
//

#define DISP(x) kDMSModeReady, x, x, 0

DisplayModeInfo	gDisplayModeInfo[] =
{
//                                                                                                                                                                          interlaced
//                                                                                                                                                                                 cSyncDisable
//                                                                                        height            hActive       hSyncEnd      vActive        vSyncEnd      hSyncPolarity
//   seed       displayModeID                     timingData                       width        pixelClock         hSyncBegin    hTotal         vSyncBegin    vTotal    vSyncPolarity      // hexpected   hactual    herror // vexpected   vactual    verror // reference
//

	{ 100, DISP(kDisplay_512x384i_60Hz_NTSC    ), timingAppleNTSC_ST             ,   512,   384,    654400,   512,   488,   528,   544,    384,   385,   388,   393, N, P, true , false }, // not accurate (calculated using GTF)
	{ 102, DISP(kDisplay_640x480i_50Hz_PAL     ), timingApplePAL_ST              ,   640,   480,    835000,   640,   600,   656,   672,    480,   481,   484,   489, N, P, true , false }, // not accurate (calculated using GTF)
//	{ 103, DISP(kDisplay_640x480i_60Hz_NTSC    ), timingAppleNTSC_FF             ,   640,   480,   1053900,   640,   616,   672,   704,    480,   481,   484,   490, N, P, true , false }, // not accurate (calculated using GTF)
	{ 108, DISP(kDisplay_768x576i_50Hz_PAL     ), timingApplePAL_FF              ,   768,   576,   1261400,   768,   744,   808,   848,    576,   577,   580,   586, N, P, true , false }, // not accurate (calculated using GTF)

	{ 106, DISP(kDisplay_640x480_120Hz         ), timingGTF_640x480_120hz        ,   640,   480,   5240600,   640,   680,   744,   848,    480,   481,   484,   515, N, P, false, false },

	{ 200, DISP(kDisplay_640x480i_60Hz_NTSC    ), timingAppleNTSC_FF             ,   640,   480,   1227000,   640,   664,   724,   780,    480,   486,   492,   525, 0, 0, true , false }, //  15700000  15730769     30769 //  60000000  59926739     73260 // Designing_Cards_and_Drivers_for_the_Macintosh_Family_2nd_Edition_1990.pdf
	{ 201, DISP(kDisplay_512x342_60Hz          ), timingInvalid                  ,   512,   342,   1566720,   512,   526,   529,   704,    342,   343,   347,   370, 0, 0, false, false }, //  22250000  22254545      4545 //  60150000  60147420      2579 // macintosh_classic_ii.pdf (I changed the hsync pulse width because it was longer than horizontal blanking and I changed vf from 0 to 1)
	{ 202, DISP(kDisplay_512x384_60Hz          ), timingApple_512x384_60hz       ,   512,   384,   1566720,   512,   528,   560,   640,    384,   385,   388,   407, 0, 0, false, false }, //  24480000  24480000         0 //  60150000  60147420      2579 // Valkyrie_AV2_ERS_Dec95.pdf, PDM_ERS.pdf, Mac_LC_630_Quadra_630.pdf, PowerMac_Computers.pdf, Mac_Color_Classic.pdf
	{ 203, DISP(kDisplay_560x384_60Hz          ), timingApple_560x384_60hz       ,   560,   384,   1723400,   560,   592,   624,   704,    384,   385,   388,   407, 0, 0, false, false }, //  24480000  24480113       113 //  60150000  60147699      2300 // Mac_LC_520.pdf, Mac_LC_475_Quadra_605.pdf, Mac_Color_Classic.pdf
//	{ 204, DISP(kDisplay_560x384_60Hz          ), timingApple_560x384_60hz       ,   560,   384,   1723400,   560,   576,   624,   704,    384,   385,   388,   407, 0, 0, false, false }, //  24480000  24480113       113 //  60150000  60147699      2300 // Mac_LC_III.pdf
	{ 205, DISP(kDisplay_640x400_67Hz          ), timingApple_640x400_67hz       ,   640,   400,   3024000,   640,   704,   768,   864,    400,   443,   446,   525, 0, 0, false, false }, //  35000000  35000000         0 //  66670000  66666666      3333 // Mac_LC_III.pdf
//	{ 206, DISP(kDisplay_640x480_60Hz_VGA      ), timingVESA_640x480_60hz        ,   640,   480,   2517500,   640,   656,   752,   800,    480,   490,   492,   525, 0, 0, false, false }, //  31469000  31468750       250 //  59940000  59940476       476 // Valkyrie_AV2_ERS_Dec95.pdf, PDM_ERS.pdf, multiple_scan_15av_display.pdf, colorsync_17_displays.pdf, applevision_1710av_display.pdf, multiple_scan_720_display.pdf, Mac_LC_630_Quadra_630.pdf, PowerMac_Computers.pdf, Mac_LC_475_Quadra_605.pdf, multiple_scan_14_display.pdf, Mac_LC_III.pdf
//	{ 207, DISP(kDisplay_640x480_67Hz          ), timingApple_640x480_67hz       ,   640,   480,   3024000,   640,   704,   768,   864,    480,   483,   486,   525, 0, 0, false, false }, //  35000000  35000000         0 //  66670000  66666666      3333 // Valkyrie_AV2_ERS_Dec95.pdf, Guide_to_Macintosh_Family_Hardware_2nd_Edition_1990.pdf, Designing_Cards_and_Drivers_for_the_Macintiosh_II_and_SE_1987.pdf, multiple_scan_15av_display.pdf, multiple_scan_720_display.pdf, lc_580.performa_580cd.pdf, Mac_LC_630_Quadra_630.pdf, Mac_LC_575.pdf, Mac_LC_520.pdf, Mac_LC_475_Quadra_605.pdf, multiple_scan_14_display.pdf, Mac_LC_III.pdf
//	{ 208, DISP(kDisplay_640x480_67Hz          ), timingApple_640x480_67hz       ,   640,   480,   3133440,   640,   720,   784,   896,    480,   483,   486,   525, 0, 0, false, false }, //  34975000  34971428      3571 //  66620000  66612244      7755 // PDM_ERS.pdf, PowerMac_Computers.pdf
//	{ 209, DISP(kDisplay_640x480_72Hz          ), timingVESA_640x480_72hz        ,   640,   480,   3150000,   640,   664,   704,   832,    480,   489,   492,   520, 0, 0, false, false }, //  37800000  37860576     60576 //  72000000  72808801    808801 // colorsync_20_displays.pdf
//	{ 210, DISP(kDisplay_640x480_75Hz          ), timingVESA_640x480_75hz        ,   640,   480,   3150000,   640,   656,   720,   840,    480,   481,   484,   500, 0, 0, false, false }, //  37500000  37500000         0 //  75000000  75000000         0 // colorsync_20_displays.pdf
//	{ 211, DISP(kDisplay_640x480_85Hz          ), timingVESA_640x480_85hz        ,   640,   480,   3600000,   640,   696,   752,   832,    480,   481,   484,   509, 0, 0, false, false }, //  43260000  43269230      9230 //  85000000  85008311      8311 // colorsync_20_displays.pdf, multiple_scan_720_display.pdf
//	{ 212, DISP(kDisplay_800x600_56Hz_VGA      ), timingVESA_800x600_56hz        ,   800,   600,   3600000,   800,   824,   896,  1024,    600,   601,   603,   625, 0, 0, false, false }, //  35150000  35156250      6250 //  56000000  56250000    250000 // colorsync_20_displays.pdf
//	{ 213, DISP(kDisplay_800x600_60Hz_VGA      ), timingVESA_800x600_60hz        ,   800,   600,   4000000,   800,   840,   968,  1056,    600,   601,   605,   628, 0, 0, false, false }, //  37880000  37878787      1212 //  60310000  60316541      6541 // Valkyrie_AV2_ERS_Dec95.pdf, colorsync_20_displays.pdf, multiple_scan_15av_display.pdf, multiple_scan_720_display.pdf, Mac_LC_630_Quadra_630.pdf, multiple_scan_14_display.pdf, (2 4 22 in colorsync_17_displays.pdf and applevision_1710av_display.pdf)
//	{ 214, DISP(kDisplay_800x600_72Hz_VGA      ), timingVESA_800x600_72hz        ,   800,   600,   5000000,   800,   856,   976,  1040,    600,   637,   643,   666, 0, 0, false, false }, //  48080000  48076923      3076 //  72180000  72187572      7572 // Valkyrie_AV2_ERS_Dec95.pdf, colorsync_20_displays.pdf, multiple_scan_15av_display.pdf, multiple_scan_720_display.pdf, Mac_LC_630_Quadra_630.pdf, multiple_scan_14_display.pdf
//	{ 215, DISP(kDisplay_800x600_75Hz_VGA      ), timingVESA_800x600_75hz        ,   800,   600,   4950000,   800,   816,   896,  1056,    600,   601,   604,   625, 0, 0, false, false }, //  46870000  46875000      5000 //  75000000  75000000         0 // colorsync_20_displays.pdf, colorsync_17_displays.pdf, applevision_1710av_display.pdf
//	{ 216, DISP(kDisplay_800x600_85Hz          ), timingVESA_800x600_85hz        ,   800,   600,   5625000,   800,   832,   896,  1048,    600,   601,   604,   631, 0, 0, false, false }, //  53670000  53673664      3664 //  85000000  85061274     61274 // colorsync_20_displays.pdf, multiple_scan_720_display.pdf
//	{ 217, DISP(kDisplay_640x870_75Hz          ), timingApple_640x870_75hz       ,   640,   870,   5728000,   640,   672,   752,   832,    870,   873,   876,   918, 0, 0, false, false }, //  68850000  68846153      3846 //  75000000  74995810      4189 // Guide_to_Macintosh_Family_Hardware_2nd_Edition_1990.pdf
	{ 218, DISP(kDisplay_640x870_75Hz          ), timingApple_640x870_75hz       ,   640,   870,   5728320,   640,   672,   752,   832,    870,   873,   876,   918, 0, 0, false, false }, //  68850000  68850000         0 //  75000000  75000000         0 // PDM_ERS.pdf, PowerMac_Computers.pdf, Mac_LC_475_Quadra_605.pdf, Mac_LC_III.pdf
//	{ 219, DISP(kDisplay_832x624_75Hz          ), timingApple_832x624_75hz       ,   832,   624,   5728320,   832,   864,   928,  1152,    624,   625,   628,   667, 0, 0, false, false }, //  49725000  49725000         0 //  74550000  74550224       224 // Valkyrie_AV2_ERS_Dec95.pdf, PDM_ERS.pdf, colorsync_20_displays.pdf, multiple_scan_720_display.pdf, PowerMac_Computers.pdf, Mac_LC_475_Quadra_605.pdf, Mac_LC_III.pdf (2 3 38 in colorsync_17_displays.pdf and applevision_1710av_display.pdf)
//	{ 220, DISP(kDisplay_832x624_75Hz          ), timingApple_832x624_75hz       ,   832,   624,   5728320,   832,   864,   928,  1152,    624,   627,   630,   669, 0, 0, false, false }, //  46751000  49725000   2974000 //  75000000  74327354    672645 // multiple_scan_14_display.pdf
//	{ 221, DISP(kDisplay_1024x768_60Hz_VGA     ), timingVESA_1024x768_60hz       ,  1024,   768,   6500000,  1024,  1048,  1184,  1344,    768,   771,   777,   806, 0, 0, false, false }, //  48360000  48363095      3095 //  60000000  60003840      3840 // Valkyrie_AV2_ERS_Dec95.pdf, colorsync_20_displays.pdf, colorsync_17_displays.pdf, applevision_1710av_display.pdf, multiple_scan_720_display.pdf
//	{ 222, DISP(kDisplay_1024x768_70Hz         ), timingVESA_1024x768_70hz       ,  1024,   768,   7500000,  1024,  1048,  1184,  1328,    768,   771,   777,   806, 0, 0, false, false }, //  56480000  56475903      4096 //  70070000  70069359       640 // Valkyrie_AV2_ERS_Dec95.pdf, colorsync_20_displays.pdf, multiple_scan_720_display.pdf
	{ 223, DISP(kDisplay_1024x768_75Hz         ), timingApple_1024x768_75hz      ,  1024,   768,   8000000,  1024,  1056,  1152,  1328,    768,   771,   774,   804, 0, 0, false, false }, //  60240000  60240963       963 //  74930000  74926571      3428 // colorsync_17_displays.pdf, applevision_1710av_display.pdf, Mac_LC_475_Quadra_605.pdf
//	{ 224, DISP(kDisplay_1024x768_75Hz_VGA     ), timingVESA_1024x768_75hz       ,  1024,   768,   7875000,  1024,  1040,  1136,  1312,    768,   769,   772,   800, 0, 0, false, false }, //  60023000  60022865       134 //  75000000  75028582     28582 // colorsync_20_displays.pdf, multiple_scan_720_display.pdf
//	{ 225, DISP(kDisplay_1024x768_85Hz         ), timingVESA_1024x768_85hz       ,  1024,   768,   9450000,  1024,  1072,  1168,  1376,    768,   769,   772,   808, 0, 0, false, false }, //  68677000  68677325       325 //  85000000  84996690      3309 // colorsync_20_displays.pdf, multiple_scan_720_display.pdf
//	{ 226, DISP(kDisplay_1024x768i_87Hz        ), timingInvalid                  ,  1024,   768,   4490000,  1024,  1032,  1208,  1264,    768,   769,   777,   817, 0, 0, true , false }, //  35522000  35522151       151 //  87000000  86957532     42467 // colorsync_20_displays.pdf
//	{ 227, DISP(kDisplay_1152x870_75Hz         ), timingApple_1152x870_75hz      ,  1152,   870,  10000000,  1152,  1184,  1312,  1456,    870,   873,   876,   915, 0, 0, false, false }, //  68680000  68681318      1318 //  75060000  75061550      1550 // Valkyrie_AV2_ERS_Dec95.pdf, colorsync_20_displays.pdf, colorsync_17_displays.pdf, multiple_scan_720_display.pdf, Mac_LC_475_Quadra_605.pdf
	{ 228, DISP(kDisplay_1280x960_75Hz         ), timingVESA_1280x960_75hz       ,  1280,   960,  12600000,  1280,  1296,  1440,  1680,    960,   961,   964,  1000, 0, 0, false, false }, //  75000000  75000000         0 //  75000000  75000000         0 // Valkyrie_AV2_ERS_Dec95.pdf
//	{ 229, DISP(kDisplay_1280x1024_60Hz        ), timingVESA_1280x1024_60hz      ,  1280,  1024,  10800000,  1280,  1328,  1440,  1688,   1024,  1025,  1028,  1066, 0, 0, false, false }, //  63800000  63981042    181042 //  60000000  60019739     19739 // multiple_scan_720_display.pdf
//	{ 230, DISP(kDisplay_1280x1024_60Hz        ), timingVESA_1280x1024_60hz      ,  1280,  1024,  11025000,  1280,  1296,  1408,  1680,   1024,  1025,  1028,  1067, 0, 0, false, false }, //  63800000  65625000   1825000 //  60000000  61504217   1504217 // colorsync_17_displays.pdf, applevision_1710av_display.pdf
//	{ 231, DISP(kDisplay_1280x1024_75Hz        ), timingVESA_1280x1024_75hz      ,  1280,  1024,  13500000,  1280,  1296,  1440,  1688,   1024,  1025,  1028,  1066, 0, 0, false, false }, //  79980000  79976303      3696 //  75000000  75024674     24674 // Valkyrie_AV2_ERS_Dec95.pdf, colorsync_20_displays.pdf, colorsync_17_displays.pdf, applevision_1710av_display.pdf
//	{ 232, DISP(kDisplay_1280x1024_85Hz        ), timingVESA_1280x1024_85hz      ,  1280,  1024,  15750000,  1280,  1344,  1504,  1728,   1024,  1025,  1028,  1072, 0, 0, false, false }, //  91146000  91145833       166 //  85000000  85024098     24098 // colorsync_20_displays.pdf
//	{ 233, DISP(kDisplay_1600x1200_75Hz        ), timingVESA_1600x1200_75hz      ,  1600,  1200,  20250000,  1600,  1664,  1856,  2160,   1200,  1201,  1204,  1250, 0, 0, false, false }, //  93750000  93750000         0 //  75000000  75000000         0 // colorsync_20_displays.pdf

	{ 300, DISP(kDisplay_720x400_70Hz          ), timingInvalid                  ,   720,   400,   2832000,   720,   738,   846,   900,    400,   421,   423,   449, N, P, false, false }, // Byte 0x23, Bit 7 // IBM
	{ 301, DISP(kDisplay_720x400_88Hz          ), timingInvalid                  ,   720,   400,   3550000,   720,   738,   846,   900,    400,   412,   414,   449, N, P, false, false }, // Byte 0x23, Bit 6 // IBM
//	{ 302, DISP(kDisplay_640x480_60Hz_VGA      ), timingVESA_640x480_60hz        ,   640,   480,   2517500,   640,   656,   752,   800,    480,   490,   492,   525, N, N, false, false }, // Byte 0x23, Bit 5 // DMT 0x04
	{ 303, DISP(kDisplay_640x480_67Hz          ), timingApple_640x480_67hz       ,   640,   480,   3024000,   640,   704,   768,   864,    480,   483,   486,   525, N, N, false, false }, // Byte 0x23, Bit 4 // Apple
//	{ 304, DISP(kDisplay_640x480_72Hz          ), timingVESA_640x480_72hz        ,   640,   480,   3150000,   640,   664,   704,   832,    480,   489,   492,   520, N, N, false, false }, // Byte 0x23, Bit 3 // DMT 0x05
//	{ 305, DISP(kDisplay_640x480_75Hz          ), timingVESA_640x480_75hz        ,   640,   480,   3150000,   640,   656,   720,   840,    480,   481,   484,   500, N, N, false, false }, // Byte 0x23, Bit 2 // DMT 0x06
//	{ 306, DISP(kDisplay_800x600_56Hz_VGA      ), timingVESA_800x600_56hz        ,   800,   600,   3600000,   800,   824,   896,  1024,    600,   601,   603,   625, P, P, false, false }, // Byte 0x23, Bit 1 // DMT 0x08
//	{ 307, DISP(kDisplay_800x600_60Hz_VGA      ), timingVESA_800x600_60hz        ,   800,   600,   4000000,   800,   840,   968,  1056,    600,   601,   605,   628, P, P, false, false }, // Byte 0x23, Bit 0 // DMT 0x09
//	{ 308, DISP(kDisplay_800x600_72Hz_VGA      ), timingVESA_800x600_72hz        ,   800,   600,   5000000,   800,   856,   976,  1040,    600,   637,   643,   666, P, P, false, false }, // Byte 0x24, Bit 7 // DMT 0x0a
//	{ 309, DISP(kDisplay_800x600_75Hz_VGA      ), timingVESA_800x600_75hz        ,   800,   600,   4950000,   800,   816,   896,  1056,    600,   601,   604,   625, P, P, false, false }, // Byte 0x24, Bit 6 // DMT 0x0b
	{ 310, DISP(kDisplay_832x624_75Hz          ), timingApple_832x624_75hz       ,   832,   624,   5728320,   832,   864,   928,  1152,    624,   625,   628,   667, N, N, false, false }, // Byte 0x24, Bit 5 // Apple
//	{ 311, DISP(kDisplay_1024x768i_87Hz        ), timingInvalid                  ,  1024,   768,   4490000,  1024,  1032,  1208,  1264,    768,   769,   777,   817, P, P, true , false }, // Byte 0x24, Bit 4 // DMT 0x0f // Vfront 0; Vback 20; Vfront +0.5 Odd Field; Vback +0.5 Even Field
//	{ 312, DISP(kDisplay_1024x768_60Hz_VGA     ), timingVESA_1024x768_60hz       ,  1024,   768,   6500000,  1024,  1048,  1184,  1344,    768,   771,   777,   806, N, N, false, false }, // Byte 0x24, Bit 3 // DMT 0x10
//	{ 313, DISP(kDisplay_1024x768_70Hz         ), timingVESA_1024x768_70hz       ,  1024,   768,   7500000,  1024,  1048,  1184,  1328,    768,   771,   777,   806, N, N, false, false }, // Byte 0x24, Bit 2 // DMT 0x11
//	{ 314, DISP(kDisplay_1024x768_75Hz_VGA     ), timingVESA_1024x768_75hz       ,  1024,   768,   7875000,  1024,  1040,  1136,  1312,    768,   769,   772,   800, P, P, false, false }, // Byte 0x24, Bit 1 // DMT 0x12
//	{ 315, DISP(kDisplay_1280x1024_75Hz        ), timingVESA_1280x1024_75hz      ,  1280,  1024,  13500000,  1280,  1296,  1440,  1688,   1024,  1025,  1028,  1066, P, P, false, false }, // Byte 0x24, Bit 0 // DMT 0x24
	{ 316, DISP(kDisplay_1152x870_75Hz         ), timingApple_1152x870_75hz      ,  1152,   870,  10000000,  1152,  1184,  1312,  1456,    870,   873,   876,   915, P, P, false, false }, // Byte 0x25, Bit 7 // Apple    // differs from linux and edid-decode = 1152 1200 1328 1456  870 873 876 915

//	{ 317, DISP(kDisplay_640x350_85Hz          ), timingInvalid                  ,   640,   350,   3150000,   640,   672,   736,   832,    350,   382,   385,   445, P, N, false, false }, // Byte 0x06, Bit 7 // DMT 0x01
//	{ 318, DISP(kDisplay_640x400_85Hz          ), timingInvalid                  ,   640,   400,   3150000,   640,   672,   736,   832,    400,   401,   404,   445, N, P, false, false }, // Byte 0x06, Bit 6 // DMT 0x02
//	{ 319, DISP(kDisplay_720x400_85Hz          ), timingInvalid                  ,   720,   400,   3550000,   720,   756,   828,   936,    400,   401,   404,   446, N, P, false, false }, // Byte 0x06, Bit 5 // DMT 0x03
//	{ 320, DISP(kDisplay_640x480_85Hz          ), timingVESA_640x480_85hz        ,   640,   480,   3600000,   640,   696,   752,   832,    480,   481,   484,   509, N, N, false, false }, // Byte 0x06, Bit 4 // DMT 0x07
//	{ 321, DISP(kDisplay_848x480_60Hz          ), timingInvalid                  ,   848,   480,   3375000,   848,   864,   976,  1088,    480,   486,   494,   517, P, P, false, false }, // Byte 0x06, Bit 3 // DMT 0x0e
//	{ 322, DISP(kDisplay_800x600_85Hz          ), timingVESA_800x600_85hz        ,   800,   600,   5625000,   800,   832,   896,  1048,    600,   601,   604,   631, P, P, false, false }, // Byte 0x06, Bit 2 // DMT 0x0c
//	{ 323, DISP(kDisplay_1024x768_85Hz         ), timingVESA_1024x768_85hz       ,  1024,   768,   9450000,  1024,  1072,  1168,  1376,    768,   769,   772,   808, P, P, false, false }, // Byte 0x06, Bit 1 // DMT 0x13
//	{ 324, DISP(kDisplay_1152x864_75Hz         ), timingInvalid                  ,  1152,   864,  10800000,  1152,  1216,  1344,  1600,    864,   865,   868,   900, P, P, false, false }, // Byte 0x06, Bit 0 // DMT 0x15
//	{ 325, DISP(kDisplay_1280x768_60Hz_RB      ), timingInvalid                  ,  1280,   768,   6825000,  1280,  1328,  1360,  1440,    768,   771,   778,   790, P, N, false, false }, // Byte 0x07, Bit 7 // DMT 0x16
//	{ 326, DISP(kDisplay_1280x768_59_87Hz      ), timingInvalid                  ,  1280,   768,   7950000,  1280,  1344,  1472,  1664,    768,   771,   778,   798, N, P, false, false }, // Byte 0x07, Bit 6 // DMT 0x17
//	{ 327, DISP(kDisplay_1280x768_75Hz         ), timingInvalid                  ,  1280,   768,  10225000,  1280,  1360,  1488,  1696,    768,   771,   778,   805, N, P, false, false }, // Byte 0x07, Bit 5 // DMT 0x18
//	{ 328, DISP(kDisplay_1280x768_85Hz         ), timingInvalid                  ,  1280,   768,  11750000,  1280,  1360,  1496,  1712,    768,   771,   778,   809, N, P, false, false }, // Byte 0x07, Bit 4 // DMT 0x19
//	{ 329, DISP(kDisplay_1280x960_60Hz         ), timingVESA_1280x960_60hz       ,  1280,   960,  10800000,  1280,  1376,  1488,  1800,    960,   961,   964,  1000, P, P, false, false }, // Byte 0x07, Bit 3 // DMT 0x20
//	{ 330, DISP(kDisplay_1280x960_85Hz         ), timingVESA_1280x960_85hz       ,  1280,   960,  14850000,  1280,  1344,  1504,  1728,    960,   961,   964,  1011, P, P, false, false }, // Byte 0x07, Bit 2 // DMT 0x21
//	{ 331, DISP(kDisplay_1280x1024_60Hz        ), timingVESA_1280x1024_60hz      ,  1280,  1024,  10800000,  1280,  1328,  1440,  1688,   1024,  1025,  1028,  1066, P, P, false, false }, // Byte 0x07, Bit 1 // DMT 0x23
//	{ 332, DISP(kDisplay_1280x1024_85Hz        ), timingVESA_1280x1024_85hz      ,  1280,  1024,  15750000,  1280,  1344,  1504,  1728,   1024,  1025,  1028,  1072, P, P, false, false }, // Byte 0x07, Bit 0 // DMT 0x25
//	{ 333, DISP(kDisplay_1360x768_60Hz         ), timingInvalid                  ,  1360,   768,   8550000,  1360,  1424,  1536,  1792,    768,   771,   777,   795, P, P, false, false }, // Byte 0x08, Bit 7 // DMT 0x27
//	{ 334, DISP(kDisplay_1440x900_60Hz_RB      ), timingInvalid                  ,  1440,   900,   8875000,  1440,  1488,  1520,  1600,    900,   903,   909,   926, P, N, false, false }, // Byte 0x08, Bit 6 // DMT 0x2e
//	{ 335, DISP(kDisplay_1440x900_60Hz         ), timingInvalid                  ,  1440,   900,  10650000,  1440,  1520,  1672,  1904,    900,   903,   909,   934, N, P, false, false }, // Byte 0x08, Bit 5 // DMT 0x2f
//	{ 336, DISP(kDisplay_1440x900_75Hz         ), timingInvalid                  ,  1440,   900,  13675000,  1440,  1536,  1688,  1936,    900,   903,   909,   942, N, P, false, false }, // Byte 0x08, Bit 4 // DMT 0x30
//	{ 337, DISP(kDisplay_1440x900_85Hz         ), timingInvalid                  ,  1440,   900,  15700000,  1440,  1544,  1696,  1952,    900,   903,   909,   948, N, P, false, false }, // Byte 0x08, Bit 3 // DMT 0x31
//	{ 338, DISP(kDisplay_1400x1050_60Hz_RB     ), timingInvalid                  ,  1400,  1050,  10100000,  1400,  1448,  1480,  1560,   1050,  1053,  1057,  1080, P, N, false, false }, // Byte 0x08, Bit 2 // DMT 0x29
//	{ 339, DISP(kDisplay_1400x1050_60Hz        ), timingInvalid                  ,  1400,  1050,  12175000,  1400,  1488,  1632,  1864,   1050,  1053,  1057,  1089, N, P, false, false }, // Byte 0x08, Bit 1 // DMT 0x2a
//	{ 340, DISP(kDisplay_1400x1050_75Hz        ), timingInvalid                  ,  1400,  1050,  15600000,  1400,  1504,  1648,  1896,   1050,  1053,  1057,  1099, N, P, false, false }, // Byte 0x08, Bit 0 // DMT 0x2b
//	{ 341, DISP(kDisplay_1400x1050_85Hz        ), timingInvalid                  ,  1400,  1050,  17950000,  1400,  1504,  1656,  1912,   1050,  1053,  1057,  1105, N, P, false, false }, // Byte 0x09, Bit 7 // DMT 0x2c
//	{ 342, DISP(kDisplay_1680x1050_60Hz_RB     ), timingInvalid                  ,  1680,  1050,  11900000,  1680,  1728,  1760,  1840,   1050,  1053,  1059,  1080, P, N, false, false }, // Byte 0x09, Bit 6 // DMT 0x39
//	{ 343, DISP(kDisplay_1680x1050_60Hz        ), timingInvalid                  ,  1680,  1050,  14625000,  1680,  1784,  1960,  2240,   1050,  1053,  1059,  1089, N, P, false, false }, // Byte 0x09, Bit 5 // DMT 0x3a
//	{ 344, DISP(kDisplay_1680x1050_75Hz        ), timingInvalid                  ,  1680,  1050,  18700000,  1680,  1800,  1976,  2272,   1050,  1053,  1059,  1099, N, P, false, false }, // Byte 0x09, Bit 4 // DMT 0x3b
//	{ 345, DISP(kDisplay_1680x1050_85Hz        ), timingInvalid                  ,  1680,  1050,  21475000,  1680,  1808,  1984,  2288,   1050,  1053,  1059,  1105, N, P, false, false }, // Byte 0x09, Bit 3 // DMT 0x3c
//	{ 346, DISP(kDisplay_1600x1200_60Hz        ), timingVESA_1600x1200_60hz      ,  1600,  1200,  16200000,  1600,  1664,  1856,  2160,   1200,  1201,  1204,  1250, P, P, false, false }, // Byte 0x09, Bit 2 // DMT 0x33
//	{ 347, DISP(kDisplay_1600x1200_65Hz        ), timingVESA_1600x1200_65hz      ,  1600,  1200,  17550000,  1600,  1664,  1856,  2160,   1200,  1201,  1204,  1250, P, P, false, false }, // Byte 0x09, Bit 1 // DMT 0x34
//	{ 348, DISP(kDisplay_1600x1200_70Hz        ), timingVESA_1600x1200_70hz      ,  1600,  1200,  18900000,  1600,  1664,  1856,  2160,   1200,  1201,  1204,  1250, P, P, false, false }, // Byte 0x09, Bit 0 // DMT 0x35
//	{ 349, DISP(kDisplay_1600x1200_75Hz        ), timingVESA_1600x1200_75hz      ,  1600,  1200,  20250000,  1600,  1664,  1856,  2160,   1200,  1201,  1204,  1250, P, P, false, false }, // Byte 0x0a, Bit 7 // DMT 0x36
//	{ 350, DISP(kDisplay_1600x1200_85Hz        ), timingVESA_1600x1200_85hz      ,  1600,  1200,  22950000,  1600,  1664,  1856,  2160,   1200,  1201,  1204,  1250, P, P, false, false }, // Byte 0x0a, Bit 6 // DMT 0x37
//	{ 351, DISP(kDisplay_1792x1344_60Hz        ), timingVESA_1792x1344_60hz      ,  1792,  1344,  20475000,  1792,  1920,  2120,  2448,   1344,  1345,  1348,  1394, N, P, false, false }, // Byte 0x0a, Bit 5 // DMT 0x3e
//	{ 352, DISP(kDisplay_1792x1344_75Hz        ), timingVESA_1792x1344_75hz      ,  1792,  1344,  26100000,  1792,  1888,  2104,  2456,   1344,  1345,  1348,  1417, N, P, false, false }, // Byte 0x0a, Bit 4 // DMT 0x3f
//	{ 353, DISP(kDisplay_1856x1392_60Hz        ), timingVESA_1856x1392_60hz      ,  1856,  1392,  21825000,  1856,  1952,  2176,  2528,   1392,  1393,  1396,  1439, N, P, false, false }, // Byte 0x0a, Bit 3 // DMT 0x41
//	{ 354, DISP(kDisplay_1856x1392_75Hz        ), timingVESA_1856x1392_75hz      ,  1856,  1392,  28800000,  1856,  1984,  2208,  2560,   1392,  1393,  1396,  1500, N, P, false, false }, // Byte 0x0a, Bit 2 // DMT 0x42
//	{ 355, DISP(kDisplay_1920x1200_60Hz_RB     ), timingInvalid                  ,  1920,  1200,  15400000,  1920,  1968,  2000,  2080,   1200,  1203,  1209,  1235, P, N, false, false }, // Byte 0x0a, Bit 1 // DMT 0x44
//	{ 356, DISP(kDisplay_1920x1200_60Hz        ), timingInvalid                  ,  1920,  1200,  19325000,  1920,  2056,  2256,  2592,   1200,  1203,  1209,  1245, N, P, false, false }, // Byte 0x0a, Bit 0 // DMT 0x45
//	{ 357, DISP(kDisplay_1920x1200_75Hz        ), timingInvalid                  ,  1920,  1200,  24525000,  1920,  2056,  2264,  2608,   1200,  1203,  1209,  1255, N, P, false, false }, // Byte 0x0b, Bit 7 // DMT 0x46
//	{ 358, DISP(kDisplay_1920x1200_85Hz        ), timingInvalid                  ,  1920,  1200,  28125000,  1920,  2064,  2272,  2624,   1200,  1203,  1209,  1262, N, P, false, false }, // Byte 0x0b, Bit 6 // DMT 0x47
//	{ 359, DISP(kDisplay_1920x1440_60Hz        ), timingVESA_1920x1440_60hz      ,  1920,  1440,  23400000,  1920,  2048,  2256,  2600,   1440,  1441,  1444,  1500, N, P, false, false }, // Byte 0x0b, Bit 5 // DMT 0x49
//	{ 360, DISP(kDisplay_1920x1440_75Hz        ), timingVESA_1920x1440_75hz      ,  1920,  1440,  29700000,  1920,  2064,  2288,  2640,   1440,  1441,  1444,  1500, N, P, false, false }, // Byte 0x0b, Bit 4 // DMT 0x4a

	{ 401, DISP(kDisplay_640x350_85Hz          ), timingInvalid                  ,   640,   350,   3150000,   640,   672,   736,   832,    350,   382,   385,   445, P, N, false, false }, // DMT 0x01
	{ 402, DISP(kDisplay_640x400_85Hz          ), timingInvalid                  ,   640,   400,   3150000,   640,   672,   736,   832,    400,   401,   404,   445, N, P, false, false }, // DMT 0x02 // STD: 0x31 0x19
	{ 403, DISP(kDisplay_720x400_85Hz          ), timingInvalid                  ,   720,   400,   3550000,   720,   756,   828,   936,    400,   401,   404,   446, N, P, false, false }, // DMT 0x03
	{ 404, DISP(kDisplay_640x480_60Hz_VGA      ), timingVESA_640x480_60hz        ,   640,   480,   2517500,   640,   656,   752,   800,    480,   490,   492,   525, N, N, false, false }, // DMT 0x04 // STD: 0x31 0x40 // VIC   1
	{ 405, DISP(kDisplay_640x480_72Hz          ), timingVESA_640x480_72hz        ,   640,   480,   3150000,   640,   664,   704,   832,    480,   489,   492,   520, N, N, false, false }, // DMT 0x05 // STD: 0x31 0x4c
	{ 406, DISP(kDisplay_640x480_75Hz          ), timingVESA_640x480_75hz        ,   640,   480,   3150000,   640,   656,   720,   840,    480,   481,   484,   500, N, N, false, false }, // DMT 0x06 // STD: 0x31 0x4f
	{ 407, DISP(kDisplay_640x480_85Hz          ), timingVESA_640x480_85hz        ,   640,   480,   3600000,   640,   696,   752,   832,    480,   481,   484,   509, N, N, false, false }, // DMT 0x07 // STD: 0x31 0x59
	{ 408, DISP(kDisplay_800x600_56Hz_VGA      ), timingVESA_800x600_56hz        ,   800,   600,   3600000,   800,   824,   896,  1024,    600,   601,   603,   625, P, P, false, false }, // DMT 0x08
	{ 409, DISP(kDisplay_800x600_60Hz_VGA      ), timingVESA_800x600_60hz        ,   800,   600,   4000000,   800,   840,   968,  1056,    600,   601,   605,   628, P, P, false, false }, // DMT 0x09 // STD: 0x45 0x40
	{ 410, DISP(kDisplay_800x600_72Hz_VGA      ), timingVESA_800x600_72hz        ,   800,   600,   5000000,   800,   856,   976,  1040,    600,   637,   643,   666, P, P, false, false }, // DMT 0x0a // STD: 0x45 0x4c
	{ 411, DISP(kDisplay_800x600_75Hz_VGA      ), timingVESA_800x600_75hz        ,   800,   600,   4950000,   800,   816,   896,  1056,    600,   601,   604,   625, P, P, false, false }, // DMT 0x0b // STD: 0x45 0x4f
	{ 412, DISP(kDisplay_800x600_85Hz          ), timingVESA_800x600_85hz        ,   800,   600,   5625000,   800,   832,   896,  1048,    600,   601,   604,   631, P, P, false, false }, // DMT 0x0c // STD: 0x45 0x59
	{ 413, DISP(kDisplay_800x600_120Hz         ), timingInvalid                  ,   800,   600,   7325000,   800,   848,   880,   960,    600,   603,   607,   636, P, N, false, false }, // DMT 0x0d // RB
	{ 414, DISP(kDisplay_848x480_60Hz          ), timingInvalid                  ,   848,   480,   3375000,   848,   864,   976,  1088,    480,   486,   494,   517, P, P, false, false }, // DMT 0x0e
	{ 415, DISP(kDisplay_1024x768i_87Hz        ), timingInvalid                  ,  1024,   768,   4490000,  1024,  1032,  1208,  1264,    768,   769,   777,   817, P, P, true , false }, // DMT 0x0f // Vfront 0; Vback 20; Vfront +0.5 Odd Field; Vback +0.5 Even Field
	{ 416, DISP(kDisplay_1024x768_60Hz_VGA     ), timingVESA_1024x768_60hz       ,  1024,   768,   6500000,  1024,  1048,  1184,  1344,    768,   771,   777,   806, N, N, false, false }, // DMT 0x10 // STD: 0x61 0x40
	{ 417, DISP(kDisplay_1024x768_70Hz         ), timingVESA_1024x768_70hz       ,  1024,   768,   7500000,  1024,  1048,  1184,  1328,    768,   771,   777,   806, N, N, false, false }, // DMT 0x11 // STD: 0x61 0x4c
	{ 418, DISP(kDisplay_1024x768_75Hz_VGA     ), timingVESA_1024x768_75hz       ,  1024,   768,   7875000,  1024,  1040,  1136,  1312,    768,   769,   772,   800, P, P, false, false }, // DMT 0x12 // STD: 0x61 0x4f
	{ 419, DISP(kDisplay_1024x768_85Hz         ), timingVESA_1024x768_85hz       ,  1024,   768,   9450000,  1024,  1072,  1168,  1376,    768,   769,   772,   808, P, P, false, false }, // DMT 0x13 // STD: 0x61 0x59
	{ 420, DISP(kDisplay_1024x768_120Hz        ), timingInvalid                  ,  1024,   768,  11550000,  1024,  1072,  1104,  1184,    768,   771,   775,   813, P, N, false, false }, // DMT 0x14 // RB
	{ 421, DISP(kDisplay_1152x864_75Hz         ), timingInvalid                  ,  1152,   864,  10800000,  1152,  1216,  1344,  1600,    864,   865,   868,   900, P, P, false, false }, // DMT 0x15 // STD: 0x71 0x4f
	{ 422, DISP(kDisplay_1280x720_60Hz         ), timingInvalid                  ,  1280,   720,   7425000,  1280,  1390,  1430,  1650,    720,   725,   730,   750, P, P, false, false }, // DMT 0x55 // STD: 0x81 0xc0 // VIC   4 // VIC  69
	{ 423, DISP(kDisplay_1280x768_60Hz_RB      ), timingInvalid                  ,  1280,   768,   6825000,  1280,  1328,  1360,  1440,    768,   771,   778,   790, P, N, false, false }, // DMT 0x16 // RB, CVT: 0x7f 0x1c 0x21
	{ 424, DISP(kDisplay_1280x768_59_87Hz      ), timingInvalid                  ,  1280,   768,   7950000,  1280,  1344,  1472,  1664,    768,   771,   778,   798, N, P, false, false }, // DMT 0x17 // CVT: 0x7f 0x1c 0x28
	{ 425, DISP(kDisplay_1280x768_75Hz         ), timingInvalid                  ,  1280,   768,  10225000,  1280,  1360,  1488,  1696,    768,   771,   778,   805, N, P, false, false }, // DMT 0x18 // CVT: 0x7f 0x1c 0x44
	{ 426, DISP(kDisplay_1280x768_85Hz         ), timingInvalid                  ,  1280,   768,  11750000,  1280,  1360,  1496,  1712,    768,   771,   778,   809, N, P, false, false }, // DMT 0x19 // CVT: 0x7f 0x1c 0x62
	{ 427, DISP(kDisplay_1280x768_120Hz        ), timingInvalid                  ,  1280,   768,  14025000,  1280,  1328,  1360,  1440,    768,   771,   778,   813, P, N, false, false }, // DMT 0x1a
	{ 428, DISP(kDisplay_1280x800_60Hz_RB      ), timingInvalid                  ,  1280,   800,   7100000,  1280,  1328,  1360,  1440,    800,   803,   809,   823, P, N, false, false }, // DMT 0x1b // RB, CVT: 0x8f 0x18 0x21
	{ 429, DISP(kDisplay_1280x800_60Hz         ), timingInvalid                  ,  1280,   800,   8350000,  1280,  1352,  1480,  1680,    800,   803,   809,   831, N, P, false, false }, // DMT 0x1c // STD: 0x81 0x00, CVT: 0x8f 0x18 0x28
	{ 430, DISP(kDisplay_1280x800_75Hz         ), timingInvalid                  ,  1280,   800,  10650000,  1280,  1360,  1488,  1696,    800,   803,   809,   838, N, P, false, false }, // DMT 0x1d // STD: 0x81 0x0f, CVT: 0x8f 0x18 0x44
	{ 431, DISP(kDisplay_1280x800_85Hz         ), timingInvalid                  ,  1280,   800,  12250000,  1280,  1360,  1496,  1712,    800,   803,   809,   843, N, P, false, false }, // DMT 0x1e // STD: 0x81 0x19, CVT: 0x8f 0x18 0x62
	{ 432, DISP(kDisplay_1280x800_120Hz        ), timingInvalid                  ,  1280,   800,  14625000,  1280,  1328,  1360,  1440,    800,   803,   809,   847, P, N, false, false }, // DMT 0x1f // RB
	{ 433, DISP(kDisplay_1280x960_60Hz         ), timingVESA_1280x960_60hz       ,  1280,   960,  10800000,  1280,  1376,  1488,  1800,    960,   961,   964,  1000, P, P, false, false }, // DMT 0x20 // STD: 0x81 0x40
	{ 434, DISP(kDisplay_1280x960_85Hz         ), timingVESA_1280x960_85hz       ,  1280,   960,  14850000,  1280,  1344,  1504,  1728,    960,   961,   964,  1011, P, P, false, false }, // DMT 0x21 // STD: 0x81 0x59
	{ 435, DISP(kDisplay_1280x960_120Hz        ), timingInvalid                  ,  1280,   960,  17550000,  1280,  1328,  1360,  1440,    960,   963,   967,  1017, P, N, false, false }, // DMT 0x22 // RB
	{ 436, DISP(kDisplay_1280x1024_60Hz        ), timingVESA_1280x1024_60hz      ,  1280,  1024,  10800000,  1280,  1328,  1440,  1688,   1024,  1025,  1028,  1066, P, P, false, false }, // DMT 0x23 // STD: 0x81 0x80
	{ 437, DISP(kDisplay_1280x1024_75Hz        ), timingVESA_1280x1024_75hz      ,  1280,  1024,  13500000,  1280,  1296,  1440,  1688,   1024,  1025,  1028,  1066, P, P, false, false }, // DMT 0x24 // STD: 0x81 0x8f
	{ 438, DISP(kDisplay_1280x1024_85Hz        ), timingVESA_1280x1024_85hz      ,  1280,  1024,  15750000,  1280,  1344,  1504,  1728,   1024,  1025,  1028,  1072, P, P, false, false }, // DMT 0x25 // STD: 0x81 0x99
	{ 439, DISP(kDisplay_1280x1024_120Hz       ), timingInvalid                  ,  1280,  1024,  18725000,  1280,  1328,  1360,  1440,   1024,  1027,  1034,  1084, P, N, false, false }, // DMT 0x26 // RB
	{ 440, DISP(kDisplay_1360x768_60Hz         ), timingInvalid                  ,  1360,   768,   8550000,  1360,  1424,  1536,  1792,    768,   771,   777,   795, P, P, false, false }, // DMT 0x27
	{ 441, DISP(kDisplay_1360x768_120Hz        ), timingInvalid                  ,  1360,   768,  14825000,  1360,  1408,  1440,  1520,    768,   771,   776,   813, P, N, false, false }, // DMT 0x28 // RB
	{ 442, DISP(kDisplay_1366x768_60Hz         ), timingInvalid                  ,  1366,   768,   8550000,  1366,  1436,  1579,  1792,    768,   771,   774,   798, P, P, false, false }, // DMT 0x51
	{ 443, DISP(kDisplay_1366x768_60Hz_RB      ), timingInvalid                  ,  1366,   768,   7200000,  1366,  1380,  1436,  1500,    768,   769,   772,   800, P, P, false, false }, // DMT 0x56 // RB
	{ 444, DISP(kDisplay_1400x1050_60Hz_RB     ), timingInvalid                  ,  1400,  1050,  10100000,  1400,  1448,  1480,  1560,   1050,  1053,  1057,  1080, P, N, false, false }, // DMT 0x29 // RB, CVT: 0x0c 0x20 0x21
	{ 445, DISP(kDisplay_1400x1050_60Hz        ), timingInvalid                  ,  1400,  1050,  12175000,  1400,  1488,  1632,  1864,   1050,  1053,  1057,  1089, N, P, false, false }, // DMT 0x2a // STD: 0x90 0x40, CVT: 0x0c 0x20 0x28
	{ 446, DISP(kDisplay_1400x1050_75Hz        ), timingInvalid                  ,  1400,  1050,  15600000,  1400,  1504,  1648,  1896,   1050,  1053,  1057,  1099, N, P, false, false }, // DMT 0x2b // STD: 0x90 0x4f, CVT: 0x0c 0x20 0x44
	{ 447, DISP(kDisplay_1400x1050_85Hz        ), timingInvalid                  ,  1400,  1050,  17950000,  1400,  1504,  1656,  1912,   1050,  1053,  1057,  1105, N, P, false, false }, // DMT 0x2c // STD: 0x90 0x59, CVT: 0x0c 0x20 0x62
	{ 448, DISP(kDisplay_1400x1050_120Hz       ), timingInvalid                  ,  1400,  1050,  20800000,  1400,  1448,  1480,  1560,   1050,  1053,  1057,  1112, P, N, false, false }, // DMT 0x2d // RB
	{ 449, DISP(kDisplay_1440x900_60Hz_RB      ), timingInvalid                  ,  1440,   900,   8875000,  1440,  1488,  1520,  1600,    900,   903,   909,   926, P, N, false, false }, // DMT 0x2e // RB, CVT: 0xc1 0x18 0x21
	{ 450, DISP(kDisplay_1440x900_60Hz         ), timingInvalid                  ,  1440,   900,  10650000,  1440,  1520,  1672,  1904,    900,   903,   909,   934, N, P, false, false }, // DMT 0x2f // STD: 0x95 0x00, CVT: 0xc1 0x18 0x28
	{ 451, DISP(kDisplay_1440x900_75Hz         ), timingInvalid                  ,  1440,   900,  13675000,  1440,  1536,  1688,  1936,    900,   903,   909,   942, N, P, false, false }, // DMT 0x30 // STD: 0x95 0x0f, CVT: 0xc1 0x18 0x44
	{ 452, DISP(kDisplay_1440x900_85Hz         ), timingInvalid                  ,  1440,   900,  15700000,  1440,  1544,  1696,  1952,    900,   903,   909,   948, N, P, false, false }, // DMT 0x31 // STD: 0x95 0x19, CVT: 0xc1 0x18 0x68
	{ 453, DISP(kDisplay_1440x900_120Hz        ), timingInvalid                  ,  1440,   900,  18275000,  1440,  1488,  1520,  1600,    900,   903,   909,   953, P, N, false, false }, // DMT 0x32 // RB
	{ 454, DISP(kDisplay_1600x900_60Hz         ), timingInvalid                  ,  1600,   900,  10800000,  1600,  1624,  1704,  1800,    900,   901,   904,  1000, P, P, false, false }, // DMT 0x53 // RB, STD: 0xa9 0xc0
	{ 455, DISP(kDisplay_1600x1200_60Hz        ), timingVESA_1600x1200_60hz      ,  1600,  1200,  16200000,  1600,  1664,  1856,  2160,   1200,  1201,  1204,  1250, P, P, false, false }, // DMT 0x33 // STD: 0xa9 0x40
	{ 456, DISP(kDisplay_1600x1200_65Hz        ), timingVESA_1600x1200_65hz      ,  1600,  1200,  17550000,  1600,  1664,  1856,  2160,   1200,  1201,  1204,  1250, P, P, false, false }, // DMT 0x34 // STD: 0xa9 0x45
	{ 457, DISP(kDisplay_1600x1200_70Hz        ), timingVESA_1600x1200_70hz      ,  1600,  1200,  18900000,  1600,  1664,  1856,  2160,   1200,  1201,  1204,  1250, P, P, false, false }, // DMT 0x35 // STD: 0xa9 0x4a
	{ 458, DISP(kDisplay_1600x1200_75Hz        ), timingVESA_1600x1200_75hz      ,  1600,  1200,  20250000,  1600,  1664,  1856,  2160,   1200,  1201,  1204,  1250, P, P, false, false }, // DMT 0x36 // STD: 0xa9 0x4f
	{ 459, DISP(kDisplay_1600x1200_85Hz        ), timingVESA_1600x1200_85hz      ,  1600,  1200,  22950000,  1600,  1664,  1856,  2160,   1200,  1201,  1204,  1250, P, P, false, false }, // DMT 0x37 // STD: 0xa9 0x59
	{ 460, DISP(kDisplay_1600x1200_120Hz       ), timingInvalid                  ,  1600,  1200,  26825000,  1600,  1648,  1680,  1760,   1200,  1203,  1207,  1271, P, N, false, false }, // DMT 0x38 // RB
	{ 461, DISP(kDisplay_1680x1050_60Hz_RB     ), timingInvalid                  ,  1680,  1050,  11900000,  1680,  1728,  1760,  1840,   1050,  1053,  1059,  1080, P, N, false, false }, // DMT 0x39 // RB, CVT: 0x0c 0x28 0x21
	{ 462, DISP(kDisplay_1680x1050_60Hz        ), timingInvalid                  ,  1680,  1050,  14625000,  1680,  1784,  1960,  2240,   1050,  1053,  1059,  1089, N, P, false, false }, // DMT 0x3a // STD: 0xb3 0x00, CVT: 0x0c 0x28 0x28
	{ 463, DISP(kDisplay_1680x1050_75Hz        ), timingInvalid                  ,  1680,  1050,  18700000,  1680,  1800,  1976,  2272,   1050,  1053,  1059,  1099, N, P, false, false }, // DMT 0x3b // STD: 0xb3 0x0f, CVT: 0x0c 0x28 0x44
	{ 464, DISP(kDisplay_1680x1050_85Hz        ), timingInvalid                  ,  1680,  1050,  21475000,  1680,  1808,  1984,  2288,   1050,  1053,  1059,  1105, N, P, false, false }, // DMT 0x3c // STD: 0xb3 0x19, CVT: 0x0c 0x28 0x68
	{ 465, DISP(kDisplay_1680x1050_120Hz       ), timingInvalid                  ,  1680,  1050,  24550000,  1680,  1728,  1760,  1840,   1050,  1053,  1059,  1112, P, N, false, false }, // DMT 0x3d // RB
	{ 466, DISP(kDisplay_1792x1344_60Hz        ), timingVESA_1792x1344_60hz      ,  1792,  1344,  20475000,  1792,  1920,  2120,  2448,   1344,  1345,  1348,  1394, N, P, false, false }, // DMT 0x3e // STD: 0xc1 0x40
	{ 467, DISP(kDisplay_1792x1344_75Hz        ), timingVESA_1792x1344_75hz      ,  1792,  1344,  26100000,  1792,  1888,  2104,  2456,   1344,  1345,  1348,  1417, N, P, false, false }, // DMT 0x3f // STD: 0xc1 0x4f
	{ 468, DISP(kDisplay_1792x1344_120Hz       ), timingInvalid                  ,  1792,  1344,  33325000,  1792,  1840,  1872,  1952,   1344,  1347,  1351,  1423, P, N, false, false }, // DMT 0x40 // RB
	{ 469, DISP(kDisplay_1856x1392_60Hz        ), timingVESA_1856x1392_60hz      ,  1856,  1392,  21825000,  1856,  1952,  2176,  2528,   1392,  1393,  1396,  1439, N, P, false, false }, // DMT 0x41 // STD: 0xc9 0x40
	{ 470, DISP(kDisplay_1856x1392_75Hz        ), timingVESA_1856x1392_75hz      ,  1856,  1392,  28800000,  1856,  1984,  2208,  2560,   1392,  1393,  1396,  1500, N, P, false, false }, // DMT 0x42 // STD: 0xc9 0x4f
	{ 471, DISP(kDisplay_1856x1392_120Hz       ), timingInvalid                  ,  1856,  1392,  35650000,  1856,  1904,  1936,  2016,   1392,  1395,  1399,  1473, P, N, false, false }, // DMT 0x43 // RB
	{ 472, DISP(kDisplay_1920x1080_60Hz        ), timingInvalid                  ,  1920,  1080,  14850000,  1920,  2008,  2052,  2200,   1080,  1084,  1089,  1125, P, P, false, false }, // DMT 0x52 // STD: 0xd1 0xc0 // VIC  16 // VIC  76
	{ 473, DISP(kDisplay_1920x1200_60Hz_RB     ), timingInvalid                  ,  1920,  1200,  15400000,  1920,  1968,  2000,  2080,   1200,  1203,  1209,  1235, P, N, false, false }, // DMT 0x44 // RB, CVT: 0x57 0x28 0x21
	{ 474, DISP(kDisplay_1920x1200_60Hz        ), timingInvalid                  ,  1920,  1200,  19325000,  1920,  2056,  2256,  2592,   1200,  1203,  1209,  1245, N, P, false, false }, // DMT 0x45 // STD: 0xd1 0x00, CVT: 0x57 0x28 0x28
	{ 475, DISP(kDisplay_1920x1200_75Hz        ), timingInvalid                  ,  1920,  1200,  24525000,  1920,  2056,  2264,  2608,   1200,  1203,  1209,  1255, N, P, false, false }, // DMT 0x46 // STD: 0xd1 0x0f, CVT: 0x57 0x28 0x44
	{ 476, DISP(kDisplay_1920x1200_85Hz        ), timingInvalid                  ,  1920,  1200,  28125000,  1920,  2064,  2272,  2624,   1200,  1203,  1209,  1262, N, P, false, false }, // DMT 0x47 // STD: 0xd1 0x19, CVT: 0x57 0x28 0x62
	{ 477, DISP(kDisplay_1920x1200_120Hz       ), timingInvalid                  ,  1920,  1200,  31700000,  1920,  1968,  2000,  2080,   1200,  1203,  1209,  1271, P, N, false, false }, // DMT 0x48 // RB
	{ 478, DISP(kDisplay_1920x1440_60Hz        ), timingVESA_1920x1440_60hz      ,  1920,  1440,  23400000,  1920,  2048,  2256,  2600,   1440,  1441,  1444,  1500, N, P, false, false }, // DMT 0x49 // STD: 0xd1 0x40
	{ 479, DISP(kDisplay_1920x1440_75Hz        ), timingVESA_1920x1440_75hz      ,  1920,  1440,  29700000,  1920,  2064,  2288,  2640,   1440,  1441,  1444,  1500, N, P, false, false }, // DMT 0x4a // STD: 0xd1 0x4f
	{ 480, DISP(kDisplay_1920x1440_120Hz       ), timingInvalid                  ,  1920,  1440,  38050000,  1920,  1968,  2000,  2080,   1440,  1442,  1445,  1523, P, N, false, false }, // DMT 0x4b // RB
	{ 481, DISP(kDisplay_2048x1152_60Hz        ), timingInvalid                  ,  2048,  1152,  16200000,  2048,  2074,  2154,  2250,   1152,  1153,  1156,  1200, P, P, false, false }, // DMT 0x54 // RB, STD: 0xe1 0xc0
	{ 482, DISP(kDisplay_2560x1600_60Hz_RB     ), timingInvalid                  ,  2560,  1600,  26850000,  2560,  2608,  2640,  2720,   1600,  1603,  1609,  1646, P, N, false, false }, // DMT 0x4c // RB, CVT: 0x1f 0x38 0x21
	{ 483, DISP(kDisplay_2560x1600_60Hz        ), timingInvalid                  ,  2560,  1600,  34850000,  2560,  2752,  3032,  3504,   1600,  1603,  1609,  1658, N, P, false, false }, // DMT 0x4d // CVT: 0x1f 0x38 0x28
	{ 484, DISP(kDisplay_2560x1600_75Hz        ), timingInvalid                  ,  2560,  1600,  44325000,  2560,  2768,  3048,  3536,   1600,  1603,  1609,  1672, N, P, false, false }, // DMT 0x4e // CVT: 0x1f 0x38 0x44
	{ 485, DISP(kDisplay_2560x1600_85Hz        ), timingInvalid                  ,  2560,  1600,  50525000,  2560,  2768,  3048,  3536,   1600,  1603,  1609,  1682, N, P, false, false }, // DMT 0x4f // CVT: 0x1f 0x38 0x62
	{ 486, DISP(kDisplay_2560x1600_120Hz       ), timingInvalid                  ,  2560,  1600,  55275000,  2560,  2608,  2640,  2720,   1600,  1603,  1609,  1694, P, N, false, false }, // DMT 0x50 // RB
	{ 487, DISP(kDisplay_4096x2160_60Hz_RB     ), timingInvalid                  ,  4096,  2160,  55674400,  4096,  4104,  4136,  4176,   2160,  2208,  2216,  2222, P, N, false, false }, // DMT 0x57 // RB
	{ 488, DISP(kDisplay_4096x2160_59_94Hz     ), timingInvalid                  ,  4096,  2160,  55618800,  4096,  4104,  4136,  4176,   2160,  2208,  2216,  2222, P, N, false, false }, // DMT 0x58 // RB

// We don't distinguish between duplicate timings that have different shaped pixels, i.e. anamorphic vs non-anamorphic, or 4:3 vs 16:9.

//	{ 501, DISP(kDisplay_640x480_60Hz_VGA      ), timingVESA_640x480_60hz        ,   640,   480,   2517500,   640,   656,   752,   800,    480,   490,   492,   525, N, N, false, false }, // VIC   1 // DMT 0x04 // STD: 0x31 0x40
	{ 502, DISP(kDisplay_720x480_60Hz          ), timingInvalid                  ,   720,   480,   2700000,   720,   736,   798,   858,    480,   489,   495,   525, N, N, false, false }, // VIC   2 // VIC   3
//	{ 503, DISP(kDisplay_720x480_60Hz          ), timingInvalid                  ,   720,   480,   2700000,   720,   736,   798,   858,    480,   489,   495,   525, N, N, false, false }, // VIC   3 // VIC   2
//	{ 504, DISP(kDisplay_1280x720_60Hz         ), timingInvalid                  ,  1280,   720,   7425000,  1280,  1390,  1430,  1650,    720,   725,   730,   750, P, P, false, false }, // VIC   4 // DMT 0x55 // STD: 0x81 0xc0 // VIC  69
	{ 505, DISP(kDisplay_1920x1080i_60Hz       ), timingInvalid                  ,  1920,  1080,   7425000,  1920,  2008,  2052,  2200,   1080,  1082,  1087,  1103, P, P, true , false }, // VIC   5
	{ 506, DISP(kDisplay_1440x480i_60Hz        ), timingInvalid                  ,  1440,   480,   2700000,  1440,  1478,  1602,  1716,    480,   484,   487,   503, N, N, true , false }, // VIC   6 // VIC   7
//	{ 507, DISP(kDisplay_1440x480i_60Hz        ), timingInvalid                  ,  1440,   480,   2700000,  1440,  1478,  1602,  1716,    480,   484,   487,   503, N, N, true , false }, // VIC   7 // VIC   6
	{ 508, DISP(kDisplay_1440x240_60Hz         ), timingInvalid                  ,  1440,   240,   2700000,  1440,  1478,  1602,  1716,    240,   244,   247,   262, N, N, false, false }, // VIC   8 // VIC   9
//	{ 509, DISP(kDisplay_1440x240_60Hz         ), timingInvalid                  ,  1440,   240,   2700000,  1440,  1478,  1602,  1716,    240,   244,   247,   262, N, N, false, false }, // VIC   9 // VIC   8
	{ 510, DISP(kDisplay_2880x480i_60Hz        ), timingInvalid                  ,  2880,   480,   5400000,  2880,  2956,  3204,  3432,    480,   484,   487,   503, N, N, true , false }, // VIC  10 // VIC  11
//	{ 511, DISP(kDisplay_2880x480i_60Hz        ), timingInvalid                  ,  2880,   480,   5400000,  2880,  2956,  3204,  3432,    480,   484,   487,   503, N, N, true , false }, // VIC  11 // VIC  10
	{ 512, DISP(kDisplay_2880x240_60Hz         ), timingInvalid                  ,  2880,   240,   5400000,  2880,  2956,  3204,  3432,    240,   244,   247,   262, N, N, false, false }, // VIC  12 // VIC  13
//	{ 513, DISP(kDisplay_2880x240_60Hz         ), timingInvalid                  ,  2880,   240,   5400000,  2880,  2956,  3204,  3432,    240,   244,   247,   262, N, N, false, false }, // VIC  13 // VIC  12
	{ 514, DISP(kDisplay_1440x480_60Hz         ), timingInvalid                  ,  1440,   480,   5400000,  1440,  1472,  1596,  1716,    480,   489,   495,   525, N, N, false, false }, // VIC  14 // VIC  15
//	{ 515, DISP(kDisplay_1440x480_60Hz         ), timingInvalid                  ,  1440,   480,   5400000,  1440,  1472,  1596,  1716,    480,   489,   495,   525, N, N, false, false }, // VIC  15 // VIC  14
//	{ 516, DISP(kDisplay_1920x1080_60Hz        ), timingInvalid                  ,  1920,  1080,  14850000,  1920,  2008,  2052,  2200,   1080,  1084,  1089,  1125, P, P, false, false }, // VIC  16 // DMT 0x52 // STD: 0xd1 0xc0 // VIC  16
	{ 517, DISP(kDisplay_720x576_50Hz          ), timingInvalid                  ,   720,   576,   2700000,   720,   732,   796,   864,    576,   581,   586,   625, N, N, false, false }, // VIC  17 // VIC  18
//	{ 518, DISP(kDisplay_720x576_50Hz          ), timingInvalid                  ,   720,   576,   2700000,   720,   732,   796,   864,    576,   581,   586,   625, N, N, false, false }, // VIC  18 // VIC  17
	{ 519, DISP(kDisplay_1280x720_50Hz         ), timingInvalid                  ,  1280,   720,   7425000,  1280,  1720,  1760,  1980,    720,   725,   730,   750, P, P, false, false }, // VIC  19 // VIC  68
	{ 520, DISP(kDisplay_1920x1080i_50Hz       ), timingInvalid                  ,  1920,  1080,   7425000,  1920,  2448,  2492,  2640,   1080,  1082,  1087,  1103, P, P, true , false }, // VIC  20
	{ 521, DISP(kDisplay_1440x576i_50Hz        ), timingInvalid                  ,  1440,   576,   2700000,  1440,  1464,  1590,  1728,    576,   578,   581,   601, N, N, true , false }, // VIC  21 // VIC  22
//	{ 522, DISP(kDisplay_1440x576i_50Hz        ), timingInvalid                  ,  1440,   576,   2700000,  1440,  1464,  1590,  1728,    576,   578,   581,   601, N, N, true , false }, // VIC  22 // VIC  21
	{ 523, DISP(kDisplay_1440x288_50Hz         ), timingInvalid                  ,  1440,   288,   2700000,  1440,  1464,  1590,  1728,    288,   290,   293,   312, N, N, false, false }, // VIC  23 // VIC  24
//	{ 524, DISP(kDisplay_1440x288_50Hz         ), timingInvalid                  ,  1440,   288,   2700000,  1440,  1464,  1590,  1728,    288,   290,   293,   312, N, N, false, false }, // VIC  24 // VIC  23
	{ 525, DISP(kDisplay_2880x576i_50Hz        ), timingInvalid                  ,  2880,   576,   5400000,  2880,  2928,  3180,  3456,    576,   578,   581,   601, N, N, true , false }, // VIC  25 // VIC  26
//	{ 526, DISP(kDisplay_2880x576i_50Hz        ), timingInvalid                  ,  2880,   576,   5400000,  2880,  2928,  3180,  3456,    576,   578,   581,   601, N, N, true , false }, // VIC  26 // VIC  25
	{ 527, DISP(kDisplay_2880x288_50Hz         ), timingInvalid                  ,  2880,   288,   5400000,  2880,  2928,  3180,  3456,    288,   290,   293,   312, N, N, false, false }, // VIC  27 // VIC  28
//	{ 528, DISP(kDisplay_2880x288_50Hz         ), timingInvalid                  ,  2880,   288,   5400000,  2880,  2928,  3180,  3456,    288,   290,   293,   312, N, N, false, false }, // VIC  28 // VIC  27
	{ 529, DISP(kDisplay_1440x576_50Hz         ), timingInvalid                  ,  1440,   576,   5400000,  1440,  1464,  1592,  1728,    576,   581,   586,   625, N, N, false, false }, // VIC  29 // VIC  30
//	{ 530, DISP(kDisplay_1440x576_50Hz         ), timingInvalid                  ,  1440,   576,   5400000,  1440,  1464,  1592,  1728,    576,   581,   586,   625, N, N, false, false }, // VIC  30 // VIC  29
	{ 531, DISP(kDisplay_1920x1080_50Hz        ), timingInvalid                  ,  1920,  1080,  14850000,  1920,  2448,  2492,  2640,   1080,  1084,  1089,  1125, P, P, false, false }, // VIC  31 // VIC  75
	{ 532, DISP(kDisplay_1920x1080_24Hz        ), timingInvalid                  ,  1920,  1080,   7425000,  1920,  2558,  2602,  2750,   1080,  1084,  1089,  1125, P, P, false, false }, // VIC  32 // VIC  72
	{ 533, DISP(kDisplay_1920x1080_25Hz        ), timingInvalid                  ,  1920,  1080,   7425000,  1920,  2448,  2492,  2640,   1080,  1084,  1089,  1125, P, P, false, false }, // VIC  33 // VIC  73
	{ 534, DISP(kDisplay_1920x1080_30Hz        ), timingInvalid                  ,  1920,  1080,   7425000,  1920,  2008,  2052,  2200,   1080,  1084,  1089,  1125, P, P, false, false }, // VIC  34 // VIC  74
	{ 535, DISP(kDisplay_2880x480_60Hz         ), timingInvalid                  ,  2880,   480,  10800000,  2880,  2944,  3192,  3432,    480,   489,   495,   525, N, N, false, false }, // VIC  35 // VIC  36
//	{ 536, DISP(kDisplay_2880x480_60Hz         ), timingInvalid                  ,  2880,   480,  10800000,  2880,  2944,  3192,  3432,    480,   489,   495,   525, N, N, false, false }, // VIC  36 // VIC  35
	{ 537, DISP(kDisplay_2880x576_50Hz         ), timingInvalid                  ,  2880,   576,  10800000,  2880,  2928,  3184,  3456,    576,   581,   586,   625, N, N, false, false }, // VIC  37 // VIC  38
//	{ 538, DISP(kDisplay_2880x576_50Hz         ), timingInvalid                  ,  2880,   576,  10800000,  2880,  2928,  3184,  3456,    576,   581,   586,   625, N, N, false, false }, // VIC  38 // VIC  37
	{ 539, DISP(kDisplay_1920x1080i_50Hz_72MHz ), timingInvalid                  ,  1920,  1080,   7200000,  1920,  1952,  2120,  2304,   1080,  1103,  1108,  1165, P, N, true , false }, // VIC  39
	{ 540, DISP(kDisplay_1920x1080i_100Hz      ), timingInvalid                  ,  1920,  1080,  14850000,  1920,  2448,  2492,  2640,   1080,  1082,  1087,  1103, P, P, true , false }, // VIC  40
	{ 541, DISP(kDisplay_1280x720_100Hz        ), timingInvalid                  ,  1280,   720,  14850000,  1280,  1720,  1760,  1980,    720,   725,   730,   750, P, P, false, false }, // VIC  41 // VIC  70
	{ 542, DISP(kDisplay_720x576_100Hz         ), timingInvalid                  ,   720,   576,   5400000,   720,   732,   796,   864,    576,   581,   586,   625, N, N, false, false }, // VIC  42 // VIC  43
//	{ 543, DISP(kDisplay_720x576_100Hz         ), timingInvalid                  ,   720,   576,   5400000,   720,   732,   796,   864,    576,   581,   586,   625, N, N, false, false }, // VIC  43 // VIC  42
	{ 544, DISP(kDisplay_1440x576i_100Hz       ), timingInvalid                  ,  1440,   576,   5400000,  1440,  1464,  1590,  1728,    576,   578,   581,   601, N, N, true , false }, // VIC  44 // VIC  45
//	{ 545, DISP(kDisplay_1440x576i_100Hz       ), timingInvalid                  ,  1440,   576,   5400000,  1440,  1464,  1590,  1728,    576,   578,   581,   601, N, N, true , false }, // VIC  45 // VIC  44
	{ 546, DISP(kDisplay_1920x1080i_120Hz      ), timingInvalid                  ,  1920,  1080,  14850000,  1920,  2008,  2052,  2200,   1080,  1082,  1087,  1103, P, P, true , false }, // VIC  46
	{ 547, DISP(kDisplay_1280x720_120Hz        ), timingInvalid                  ,  1280,   720,  14850000,  1280,  1390,  1430,  1650,    720,   725,   730,   750, P, P, false, false }, // VIC  47 // VIC  71
	{ 548, DISP(kDisplay_720x480_120Hz         ), timingInvalid                  ,   720,   480,   5400000,   720,   736,   798,   858,    480,   489,   495,   525, N, N, false, false }, // VIC  48 // VIC  49
//	{ 549, DISP(kDisplay_720x480_120Hz         ), timingInvalid                  ,   720,   480,   5400000,   720,   736,   798,   858,    480,   489,   495,   525, N, N, false, false }, // VIC  49 // VIC  48
	{ 550, DISP(kDisplay_1440x480i_120Hz       ), timingInvalid                  ,  1440,   480,   5400000,  1440,  1478,  1602,  1716,    480,   484,   487,   503, N, N, true , false }, // VIC  50 // VIC  51
//	{ 551, DISP(kDisplay_1440x480i_120Hz       ), timingInvalid                  ,  1440,   480,   5400000,  1440,  1478,  1602,  1716,    480,   484,   487,   503, N, N, true , false }, // VIC  51 // VIC  50
	{ 552, DISP(kDisplay_720x576_200Hz         ), timingInvalid                  ,   720,   576,  10800000,   720,   732,   796,   864,    576,   581,   586,   625, N, N, false, false }, // VIC  52 // VIC  53
//	{ 553, DISP(kDisplay_720x576_200Hz         ), timingInvalid                  ,   720,   576,  10800000,   720,   732,   796,   864,    576,   581,   586,   625, N, N, false, false }, // VIC  53 // VIC  52
	{ 554, DISP(kDisplay_1440x576i_200Hz       ), timingInvalid                  ,  1440,   576,  10800000,  1440,  1464,  1590,  1728,    576,   578,   581,   601, N, N, true , false }, // VIC  54 // VIC  55
//	{ 555, DISP(kDisplay_1440x576i_200Hz       ), timingInvalid                  ,  1440,   576,  10800000,  1440,  1464,  1590,  1728,    576,   578,   581,   601, N, N, true , false }, // VIC  55 // VIC  54
	{ 556, DISP(kDisplay_720x480_240Hz         ), timingInvalid                  ,   720,   480,  10800000,   720,   736,   798,   858,    480,   489,   495,   525, N, N, false, false }, // VIC  56 // VIC  57
//	{ 557, DISP(kDisplay_720x480_240Hz         ), timingInvalid                  ,   720,   480,  10800000,   720,   736,   798,   858,    480,   489,   495,   525, N, N, false, false }, // VIC  57 // VIC  56
	{ 558, DISP(kDisplay_1440x480i_240Hz       ), timingInvalid                  ,  1440,   480,  10800000,  1440,  1478,  1602,  1716,    480,   484,   487,   503, N, N, true , false }, // VIC  58 // VIC  59
//	{ 559, DISP(kDisplay_1440x480i_240Hz       ), timingInvalid                  ,  1440,   480,  10800000,  1440,  1478,  1602,  1716,    480,   484,   487,   503, N, N, true , false }, // VIC  59 // VIC  58
	{ 560, DISP(kDisplay_1280x720_24Hz         ), timingInvalid                  ,  1280,   720,   5940000,  1280,  3040,  3080,  3300,    720,   725,   730,   750, P, P, false, false }, // VIC  60 // VIC  65
	{ 561, DISP(kDisplay_1280x720_25Hz         ), timingInvalid                  ,  1280,   720,   7425000,  1280,  3700,  3740,  3960,    720,   725,   730,   750, P, P, false, false }, // VIC  61 // VIC  66
	{ 562, DISP(kDisplay_1280x720_30Hz         ), timingInvalid                  ,  1280,   720,   7425000,  1280,  3040,  3080,  3300,    720,   725,   730,   750, P, P, false, false }, // VIC  62 // VIC  67
	{ 563, DISP(kDisplay_1920x1080_120Hz       ), timingInvalid                  ,  1920,  1080,  29700000,  1920,  2008,  2052,  2200,   1080,  1084,  1089,  1125, P, P, false, false }, // VIC  63 // VIC  78
	{ 564, DISP(kDisplay_1920x1080_100Hz       ), timingInvalid                  ,  1920,  1080,  29700000,  1920,  2448,  2492,  2640,   1080,  1084,  1089,  1125, P, P, false, false }, // VIC  64 // VIC  77
//	{ 565, DISP(kDisplay_1280x720_24Hz         ), timingInvalid                  ,  1280,   720,   5940000,  1280,  3040,  3080,  3300,    720,   725,   730,   750, P, P, false, false }, // VIC  65 // VIC  60
//	{ 566, DISP(kDisplay_1280x720_25Hz         ), timingInvalid                  ,  1280,   720,   7425000,  1280,  3700,  3740,  3960,    720,   725,   730,   750, P, P, false, false }, // VIC  66 // VIC  61
//	{ 567, DISP(kDisplay_1280x720_30Hz         ), timingInvalid                  ,  1280,   720,   7425000,  1280,  3040,  3080,  3300,    720,   725,   730,   750, P, P, false, false }, // VIC  67 // VIC  62
//	{ 568, DISP(kDisplay_1280x720_50Hz         ), timingInvalid                  ,  1280,   720,   7425000,  1280,  1720,  1760,  1980,    720,   725,   730,   750, P, P, false, false }, // VIC  68 // VIC  19
//	{ 569, DISP(kDisplay_1280x720_60Hz         ), timingInvalid                  ,  1280,   720,   7425000,  1280,  1390,  1430,  1650,    720,   725,   730,   750, P, P, false, false }, // VIC  69 // DMT 0x55 // STD: 0x81 0xc0 // VIC   4
//	{ 570, DISP(kDisplay_1280x720_100Hz        ), timingInvalid                  ,  1280,   720,  14850000,  1280,  1720,  1760,  1980,    720,   725,   730,   750, P, P, false, false }, // VIC  70 // VIC  41
//	{ 571, DISP(kDisplay_1280x720_120Hz        ), timingInvalid                  ,  1280,   720,  14850000,  1280,  1390,  1430,  1650,    720,   725,   730,   750, P, P, false, false }, // VIC  71 // VIC  47
//	{ 572, DISP(kDisplay_1920x1080_24Hz        ), timingInvalid                  ,  1920,  1080,   7425000,  1920,  2558,  2602,  2750,   1080,  1084,  1089,  1125, P, P, false, false }, // VIC  72 // VIC  32
//	{ 573, DISP(kDisplay_1920x1080_25Hz        ), timingInvalid                  ,  1920,  1080,   7425000,  1920,  2448,  2492,  2640,   1080,  1084,  1089,  1125, P, P, false, false }, // VIC  73 // VIC  33
//	{ 574, DISP(kDisplay_1920x1080_30Hz        ), timingInvalid                  ,  1920,  1080,   7425000,  1920,  2008,  2052,  2200,   1080,  1084,  1089,  1125, P, P, false, false }, // VIC  74 // VIC  34
//	{ 575, DISP(kDisplay_1920x1080_50Hz        ), timingInvalid                  ,  1920,  1080,  14850000,  1920,  2448,  2492,  2640,   1080,  1084,  1089,  1125, P, P, false, false }, // VIC  75 // VIC  31
//	{ 576, DISP(kDisplay_1920x1080_60Hz        ), timingInvalid                  ,  1920,  1080,  14850000,  1920,  2008,  2052,  2200,   1080,  1084,  1089,  1125, P, P, false, false }, // VIC  76 // DMT 0x52 // STD: 0xd1 0xc0 // VIC  16
//	{ 577, DISP(kDisplay_1920x1080_100Hz       ), timingInvalid                  ,  1920,  1080,  29700000,  1920,  2448,  2492,  2640,   1080,  1084,  1089,  1125, P, P, false, false }, // VIC  77 // VIC  64
//	{ 578, DISP(kDisplay_1920x1080_120Hz       ), timingInvalid                  ,  1920,  1080,  29700000,  1920,  2008,  2052,  2200,   1080,  1084,  1089,  1125, P, P, false, false }, // VIC  78 // VIC  63
	{ 579, DISP(kDisplay_1680x720_24Hz         ), timingInvalid                  ,  1680,   720,   5940000,  1680,  3040,  3080,  3300,    720,   725,   730,   750, P, P, false, false }, // VIC  79
	{ 580, DISP(kDisplay_1680x720_25Hz         ), timingInvalid                  ,  1680,   720,   5940000,  1680,  2908,  2948,  3168,    720,   725,   730,   750, P, P, false, false }, // VIC  80
	{ 581, DISP(kDisplay_1680x720_30Hz         ), timingInvalid                  ,  1680,   720,   5940000,  1680,  2380,  2420,  2640,    720,   725,   730,   750, P, P, false, false }, // VIC  81
	{ 582, DISP(kDisplay_1680x720_50Hz         ), timingInvalid                  ,  1680,   720,   8250000,  1680,  1940,  1980,  2200,    720,   725,   730,   750, P, P, false, false }, // VIC  82
	{ 583, DISP(kDisplay_1680x720_60Hz         ), timingInvalid                  ,  1680,   720,   9900000,  1680,  1940,  1980,  2200,    720,   725,   730,   750, P, P, false, false }, // VIC  83
	{ 584, DISP(kDisplay_1680x720_100Hz        ), timingInvalid                  ,  1680,   720,  16500000,  1680,  1740,  1780,  2000,    720,   725,   730,   825, P, P, false, false }, // VIC  84
	{ 585, DISP(kDisplay_1680x720_120Hz        ), timingInvalid                  ,  1680,   720,  19800000,  1680,  1740,  1780,  2000,    720,   725,   730,   825, P, P, false, false }, // VIC  85
	{ 586, DISP(kDisplay_2560x1080_24Hz        ), timingInvalid                  ,  2560,  1080,   9900000,  2560,  3558,  3602,  3750,   1080,  1084,  1089,  1100, P, P, false, false }, // VIC  86
	{ 587, DISP(kDisplay_2560x1080_25Hz        ), timingInvalid                  ,  2560,  1080,   9000000,  2560,  3008,  3052,  3200,   1080,  1084,  1089,  1125, P, P, false, false }, // VIC  87
	{ 588, DISP(kDisplay_2560x1080_30Hz        ), timingInvalid                  ,  2560,  1080,  11880000,  2560,  3328,  3372,  3520,   1080,  1084,  1089,  1125, P, P, false, false }, // VIC  88
	{ 589, DISP(kDisplay_2560x1080_50Hz        ), timingInvalid                  ,  2560,  1080,  18562500,  2560,  3108,  3152,  3300,   1080,  1084,  1089,  1125, P, P, false, false }, // VIC  89
	{ 590, DISP(kDisplay_2560x1080_60Hz        ), timingInvalid                  ,  2560,  1080,  19800000,  2560,  2808,  2852,  3000,   1080,  1084,  1089,  1100, P, P, false, false }, // VIC  90
	{ 591, DISP(kDisplay_2560x1080_100Hz       ), timingInvalid                  ,  2560,  1080,  37125000,  2560,  2778,  2822,  2970,   1080,  1084,  1089,  1250, P, P, false, false }, // VIC  91
	{ 592, DISP(kDisplay_2560x1080_120Hz       ), timingInvalid                  ,  2560,  1080,  49500000,  2560,  3108,  3152,  3300,   1080,  1084,  1089,  1250, P, P, false, false }, // VIC  92
	{ 593, DISP(kDisplay_3840x2160_24Hz        ), timingInvalid                  ,  3840,  2160,  29700000,  3840,  5116,  5204,  5500,   2160,  2168,  2178,  2250, P, P, false, false }, // VIC  93 // VIC 103
	{ 594, DISP(kDisplay_3840x2160_25Hz        ), timingInvalid                  ,  3840,  2160,  29700000,  3840,  4896,  4984,  5280,   2160,  2168,  2178,  2250, P, P, false, false }, // VIC  94 // VIC 104
	{ 595, DISP(kDisplay_3840x2160_30Hz        ), timingInvalid                  ,  3840,  2160,  29700000,  3840,  4016,  4104,  4400,   2160,  2168,  2178,  2250, P, P, false, false }, // VIC  95 // VIC 105
	{ 596, DISP(kDisplay_3840x2160_50Hz        ), timingInvalid                  ,  3840,  2160,  59400000,  3840,  4896,  4984,  5280,   2160,  2168,  2178,  2250, P, P, false, false }, // VIC  96 // VIC 106
	{ 597, DISP(kDisplay_3840x2160_60Hz        ), timingInvalid                  ,  3840,  2160,  59400000,  3840,  4016,  4104,  4400,   2160,  2168,  2178,  2250, P, P, false, false }, // VIC  97 // VIC 107
	{ 598, DISP(kDisplay_4096x2160_24Hz        ), timingInvalid                  ,  4096,  2160,  29700000,  4096,  5116,  5204,  5500,   2160,  2168,  2178,  2250, P, P, false, false }, // VIC  98
	{ 599, DISP(kDisplay_4096x2160_25Hz        ), timingInvalid                  ,  4096,  2160,  29700000,  4096,  5064,  5152,  5280,   2160,  2168,  2178,  2250, P, P, false, false }, // VIC  99
	{ 600, DISP(kDisplay_4096x2160_30Hz        ), timingInvalid                  ,  4096,  2160,  29700000,  4096,  4184,  4272,  4400,   2160,  2168,  2178,  2250, P, P, false, false }, // VIC 100
	{ 601, DISP(kDisplay_4096x2160_50Hz        ), timingInvalid                  ,  4096,  2160,  59400000,  4096,  5064,  5152,  5280,   2160,  2168,  2178,  2250, P, P, false, false }, // VIC 101
	{ 602, DISP(kDisplay_4096x2160_60Hz        ), timingInvalid                  ,  4096,  2160,  59400000,  4096,  4184,  4272,  4400,   2160,  2168,  2178,  2250, P, P, false, false }, // VIC 102
//	{ 603, DISP(kDisplay_3840x2160_24Hz        ), timingInvalid                  ,  3840,  2160,  29700000,  3840,  5116,  5204,  5500,   2160,  2168,  2178,  2250, P, P, false, false }, // VIC 103 // VIC  93
//	{ 604, DISP(kDisplay_3840x2160_25Hz        ), timingInvalid                  ,  3840,  2160,  29700000,  3840,  4896,  4984,  5280,   2160,  2168,  2178,  2250, P, P, false, false }, // VIC 104 // VIC  94
//	{ 605, DISP(kDisplay_3840x2160_30Hz        ), timingInvalid                  ,  3840,  2160,  29700000,  3840,  4016,  4104,  4400,   2160,  2168,  2178,  2250, P, P, false, false }, // VIC 105 // VIC  95
//	{ 606, DISP(kDisplay_3840x2160_50Hz        ), timingInvalid                  ,  3840,  2160,  59400000,  3840,  4896,  4984,  5280,   2160,  2168,  2178,  2250, P, P, false, false }, // VIC 106 // VIC  96
//	{ 607, DISP(kDisplay_3840x2160_60Hz        ), timingInvalid                  ,  3840,  2160,  59400000,  3840,  4016,  4104,  4400,   2160,  2168,  2178,  2250, P, P, false, false }, // VIC 107 // VIC  97
	{ 608, DISP(kDisplay_1280x720_48Hz         ), timingInvalid                  ,  1280,   720,   9000000,  1280,  2240,  2280,  2500,    720,   725,   730,   750, P, P, false, false }, // VIC 108 // VIC 109
//	{ 609, DISP(kDisplay_1280x720_48Hz         ), timingInvalid                  ,  1280,   720,   9000000,  1280,  2240,  2280,  2500,    720,   725,   730,   750, P, P, false, false }, // VIC 109 // VIC 108
	{ 610, DISP(kDisplay_1680x720_48Hz         ), timingInvalid                  ,  1680,   720,   9900000,  1680,  2490,  2530,  2750,    720,   725,   730,   750, P, P, false, false }, // VIC 110
	{ 611, DISP(kDisplay_1920x1080_48Hz        ), timingInvalid                  ,  1920,  1080,  14850000,  1920,  2558,  2602,  2750,   1080,  1084,  1089,  1125, P, P, false, false }, // VIC 111 // VIC 112
//	{ 612, DISP(kDisplay_1920x1080_48Hz        ), timingInvalid                  ,  1920,  1080,  14850000,  1920,  2558,  2602,  2750,   1080,  1084,  1089,  1125, P, P, false, false }, // VIC 112 // VIC 111
	{ 613, DISP(kDisplay_2560x1080_48Hz        ), timingInvalid                  ,  2560,  1080,  19800000,  2560,  3558,  3602,  3750,   1080,  1084,  1089,  1100, P, P, false, false }, // VIC 113
	{ 614, DISP(kDisplay_3840x2160_48Hz        ), timingInvalid                  ,  3840,  2160,  59400000,  3840,  5116,  5204,  5500,   2160,  2168,  2178,  2250, P, P, false, false }, // VIC 114 // VIC 116
	{ 615, DISP(kDisplay_4096x2160_48Hz        ), timingInvalid                  ,  4096,  2160,  59400000,  4096,  5116,  5204,  5500,   2160,  2168,  2178,  2250, P, P, false, false }, // VIC 115
//	{ 616, DISP(kDisplay_3840x2160_48Hz        ), timingInvalid                  ,  3840,  2160,  59400000,  3840,  5116,  5204,  5500,   2160,  2168,  2178,  2250, P, P, false, false }, // VIC 116 // VIC 114
	{ 617, DISP(kDisplay_3840x2160_100Hz       ), timingInvalid                  ,  3840,  2160, 118800000,  3840,  4896,  4984,  5280,   2160,  2168,  2178,  2250, P, P, false, false }, // VIC 117 // VIC 119
	{ 618, DISP(kDisplay_3840x2160_120Hz       ), timingInvalid                  ,  3840,  2160, 118800000,  3840,  4016,  4104,  4400,   2160,  2168,  2178,  2250, P, P, false, false }, // VIC 118 // VIC 120
//	{ 619, DISP(kDisplay_3840x2160_100Hz       ), timingInvalid                  ,  3840,  2160, 118800000,  3840,  4896,  4984,  5280,   2160,  2168,  2178,  2250, P, P, false, false }, // VIC 119 // VIC 117
//	{ 620, DISP(kDisplay_3840x2160_120Hz       ), timingInvalid                  ,  3840,  2160, 118800000,  3840,  4016,  4104,  4400,   2160,  2168,  2178,  2250, P, P, false, false }, // VIC 120 // VIC 118
	{ 621, DISP(kDisplay_5120x2160_24Hz        ), timingInvalid                  ,  5120,  2160,  39600000,  5120,  7116,  7204,  7500,   2160,  2168,  2178,  2200, P, P, false, false }, // VIC 121
	{ 622, DISP(kDisplay_5120x2160_25Hz        ), timingInvalid                  ,  5120,  2160,  39600000,  5120,  6816,  6904,  7200,   2160,  2168,  2178,  2200, P, P, false, false }, // VIC 122
	{ 623, DISP(kDisplay_5120x2160_30Hz        ), timingInvalid                  ,  5120,  2160,  39600000,  5120,  5784,  5872,  6000,   2160,  2168,  2178,  2200, P, P, false, false }, // VIC 123
	{ 624, DISP(kDisplay_5120x2160_48Hz        ), timingInvalid                  ,  5120,  2160,  74250000,  5120,  5866,  5954,  6250,   2160,  2168,  2178,  2475, P, P, false, false }, // VIC 124
	{ 625, DISP(kDisplay_5120x2160_50Hz        ), timingInvalid                  ,  5120,  2160,  74250000,  5120,  6216,  6304,  6600,   2160,  2168,  2178,  2250, P, P, false, false }, // VIC 125
	{ 626, DISP(kDisplay_5120x2160_60Hz        ), timingInvalid                  ,  5120,  2160,  74250000,  5120,  5284,  5372,  5500,   2160,  2168,  2178,  2250, P, P, false, false }, // VIC 126
	{ 627, DISP(kDisplay_5120x2160_100Hz       ), timingInvalid                  ,  5120,  2160, 148500000,  5120,  6216,  6304,  6600,   2160,  2168,  2178,  2250, P, P, false, false }, // VIC 127
	{ 628, DISP(kDisplay_5120x2160_120Hz       ), timingInvalid                  ,  5120,  2160, 148500000,  5120,  5284,  5372,  5500,   2160,  2168,  2178,  2250, P, P, false, false }, // VIC 193
	{ 629, DISP(kDisplay_7680x4320_24Hz        ), timingInvalid                  ,  7680,  4320, 118800000,  7680, 10232, 10408, 11000,   4320,  4336,  4356,  4500, P, P, false, false }, // VIC 194 // VIC 202
	{ 630, DISP(kDisplay_7680x4320_25Hz        ), timingInvalid                  ,  7680,  4320, 118800000,  7680, 10032, 10208, 10800,   4320,  4336,  4356,  4400, P, P, false, false }, // VIC 195 // VIC 203
	{ 631, DISP(kDisplay_7680x4320_30Hz        ), timingInvalid                  ,  7680,  4320, 118800000,  7680,  8232,  8408,  9000,   4320,  4336,  4356,  4400, P, P, false, false }, // VIC 196 // VIC 204
	{ 632, DISP(kDisplay_7680x4320_48Hz        ), timingInvalid                  ,  7680,  4320, 237600000,  7680, 10232, 10408, 11000,   4320,  4336,  4356,  4500, P, P, false, false }, // VIC 197 // VIC 205
	{ 633, DISP(kDisplay_7680x4320_50Hz        ), timingInvalid                  ,  7680,  4320, 237600000,  7680, 10032, 10208, 10800,   4320,  4336,  4356,  4400, P, P, false, false }, // VIC 198 // VIC 206
	{ 634, DISP(kDisplay_7680x4320_60Hz        ), timingInvalid                  ,  7680,  4320, 237600000,  7680,  8232,  8408,  9000,   4320,  4336,  4356,  4400, P, P, false, false }, // VIC 199 // VIC 207
	{ 635, DISP(kDisplay_7680x4320_100Hz       ), timingInvalid                  ,  7680,  4320, 475200000,  7680,  9792,  9968, 10560,   4320,  4336,  4356,  4500, P, P, false, false }, // VIC 200 // VIC 208
	{ 636, DISP(kDisplay_7680x4320_120Hz       ), timingInvalid                  ,  7680,  4320, 475200000,  7680,  8032,  8208,  8800,   4320,  4336,  4356,  4500, P, P, false, false }, // VIC 201 // VIC 209
//	{ 637, DISP(kDisplay_7680x4320_24Hz        ), timingInvalid                  ,  7680,  4320, 118800000,  7680, 10232, 10408, 11000,   4320,  4336,  4356,  4500, P, P, false, false }, // VIC 202 // VIC 194
//	{ 638, DISP(kDisplay_7680x4320_25Hz        ), timingInvalid                  ,  7680,  4320, 118800000,  7680, 10032, 10208, 10800,   4320,  4336,  4356,  4400, P, P, false, false }, // VIC 203 // VIC 195
//	{ 639, DISP(kDisplay_7680x4320_30Hz        ), timingInvalid                  ,  7680,  4320, 118800000,  7680,  8232,  8408,  9000,   4320,  4336,  4356,  4400, P, P, false, false }, // VIC 204 // VIC 196
//	{ 640, DISP(kDisplay_7680x4320_48Hz        ), timingInvalid                  ,  7680,  4320, 237600000,  7680, 10232, 10408, 11000,   4320,  4336,  4356,  4500, P, P, false, false }, // VIC 205 // VIC 197
//	{ 641, DISP(kDisplay_7680x4320_50Hz        ), timingInvalid                  ,  7680,  4320, 237600000,  7680, 10032, 10208, 10800,   4320,  4336,  4356,  4400, P, P, false, false }, // VIC 206 // VIC 198
//	{ 642, DISP(kDisplay_7680x4320_60Hz        ), timingInvalid                  ,  7680,  4320, 237600000,  7680,  8232,  8408,  9000,   4320,  4336,  4356,  4400, P, P, false, false }, // VIC 207 // VIC 199
//	{ 643, DISP(kDisplay_7680x4320_100Hz       ), timingInvalid                  ,  7680,  4320, 475200000,  7680,  9792,  9968, 10560,   4320,  4336,  4356,  4500, P, P, false, false }, // VIC 208 // VIC 200
//	{ 644, DISP(kDisplay_7680x4320_120Hz       ), timingInvalid                  ,  7680,  4320, 475200000,  7680,  8032,  8208,  8800,   4320,  4336,  4356,  4500, P, P, false, false }, // VIC 209 // VIC 201
	{ 645, DISP(kDisplay_10240x4320_24Hz       ), timingInvalid                  , 10240,  4320, 148500000, 10240, 11732, 11908, 12500,   4320,  4336,  4356,  4950, P, P, false, false }, // VIC 210
	{ 646, DISP(kDisplay_10240x4320_25Hz       ), timingInvalid                  , 10240,  4320, 148500000, 10240, 12732, 12908, 13500,   4320,  4336,  4356,  4400, P, P, false, false }, // VIC 211
	{ 647, DISP(kDisplay_10240x4320_30Hz       ), timingInvalid                  , 10240,  4320, 148500000, 10240, 10528, 10704, 11000,   4320,  4336,  4356,  4500, P, P, false, false }, // VIC 212
	{ 648, DISP(kDisplay_10240x4320_48Hz       ), timingInvalid                  , 10240,  4320, 297000000, 10240, 11732, 11908, 12500,   4320,  4336,  4356,  4950, P, P, false, false }, // VIC 213
	{ 649, DISP(kDisplay_10240x4320_50Hz       ), timingInvalid                  , 10240,  4320, 297000000, 10240, 12732, 12908, 13500,   4320,  4336,  4356,  4400, P, P, false, false }, // VIC 214
	{ 650, DISP(kDisplay_10240x4320_60Hz       ), timingInvalid                  , 10240,  4320, 297000000, 10240, 10528, 10704, 11000,   4320,  4336,  4356,  4500, P, P, false, false }, // VIC 215
	{ 651, DISP(kDisplay_10240x4320_100Hz      ), timingInvalid                  , 10240,  4320, 594000000, 10240, 12432, 12608, 13200,   4320,  4336,  4356,  4500, P, P, false, false }, // VIC 216
	{ 652, DISP(kDisplay_10240x4320_120Hz      ), timingInvalid                  , 10240,  4320, 594000000, 10240, 10528, 10704, 11000,   4320,  4336,  4356,  4500, P, P, false, false }, // VIC 217
	{ 653, DISP(kDisplay_4096x2160_100Hz       ), timingInvalid                  ,  4096,  2160, 118800000,  4096,  4896,  4984,  5280,   2160,  2168,  2178,  2250, P, P, false, false }, // VIC 218
	{ 654, DISP(kDisplay_4096x2160_120Hz       ), timingInvalid                  ,  4096,  2160, 118800000,  4096,  4184,  4272,  4400,   2160,  2168,  2178,  2250, P, P, false, false }, // VIC 219

// *** If you add any more pre programmed modes here then change kFirstProgrammableModeInfo below. Don't exceed kFirstProgrammableDisplayMode.
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode +  0, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode +  1, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode +  2, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode +  3, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode +  4, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode +  5, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode +  6, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode +  7, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode +  8, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode +  9, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 10, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 11, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 12, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 13, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 14, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 15, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 16, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 17, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 18, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 19, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 20, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 21, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 22, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 23, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 24, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 25, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 26, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 27, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 28, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 29, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 30, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 31, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 32, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 33, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 34, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 35, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 36, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 37, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 38, kDisplayModeIDInvalid      },
	{ 0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 39, kDisplayModeIDInvalid      },
};
const int kNumModeInfos = sizeof(gDisplayModeInfo) / sizeof(DisplayModeInfo);
const int kFirstProgrammableModeInfo = 19;

typedef enum RegFieldControl {
	/* see comments in GraphicsHALInitPrivateData for info */
	kRegFieldControlHACTIVE,
	kRegFieldControlHSYNCBEGIN,
	kRegFieldControlHSYNCEND,
	kRegFieldControlHTOTAL,
	kRegFieldControlVACTIVE,
	kRegFieldControlVSYNCBEGIN,
	kRegFieldControlVSYNCEND,
	kRegFieldControlVTOTAL,

	kRegFieldControlMON_SENSE_All,
	kRegFieldControlMON_SENSE_1,
	kRegFieldControlMON_SENSE_2,
	kRegFieldControlMON_SENSE_3,

	kRegFieldControlIMMEDIATE_FLAGS,
	kRegFieldControlPIXEL_CLOCK,
	kRegFieldControlPIXEL_DEPTH,
	kRegFieldControlFRAMEBUFFER_BASE,
	kRegFieldControlFRAMEBUFFER_ROWBYTES,
	kRegFieldControlHWCURSOR_BASE,
	kRegFieldControlHWCURSOR_WIDTH,
	kRegFieldControlHWCURSOR_POS,
	kRegFieldControlCOLOR_INDEX,
	kRegFieldControlCOLOR_DATA,

	kRegFieldControlTIMING_FLAGS_All,
	kRegFieldControlINTERLACED,
	kRegFieldControlVSYNC_POLARITY,
	kRegFieldControlHSYNC_POLARITY,
	kRegFieldControlHWCURSOR_24,


	kRegFieldControlIMMEDIATE_FLAGS_All,
	kRegFieldControlHWCURSOR_ENABLE,
	kRegFieldControlDISABLE_TIMING,
	kRegFieldControlVSYNC_DISABLE,
	kRegFieldControlHSYNC_DISABLE,
	kRegFieldControlCSYNC_DISABLE,
	kRegFieldControlBLANK_DISABLE,
	kRegFieldControlDO_LATCH,

	kRegFieldControlINT_ENABLE_All,
	kRegFieldControlVBL_IRQ_CLR,
	kRegFieldControlVBL_IRQ_EN,

	kRegFieldControlINT_STATUS_All,
	kRegFieldControlVBL_IRQ_STAT,
	kNumRegFieldsControl
} RegFieldControl;

typedef struct ControlRegSpec {
	UInt16 controlAddressOffset;
	UInt16 bitFieldStart;
	UInt32 mask;
	Boolean isBitField;
} ControlRegSpec;



// Template saves the clut when GraphicsHALSetPowerState is told to enter the low power mode kAVPowerOff.
// This stucture is 1/4 the size of a csTable
typedef struct RGB RGB;
struct RGB
{
	UInt8 red;
	UInt8 green;
	UInt8 blue;
	UInt8 filler;
};

//
// HardwareCursorData
//  This structure contains the data needed to maintain a hardware cursor.
//

typedef UInt32 SpurCursorImage[kHardwareCursorImageSize];
typedef ColorSpec SpurColorMap[kNumHardwareCursorColors];
typedef UInt32 SpurColorEncodings[kNumHardwareCursorColors];
typedef struct SpurHardwareCursorData SpurHardwareCursorData;
struct SpurHardwareCursorData
{
	HardwareCursorDescriptorRec cursorDescriptor;		// Record describing hardware cursor implementation
	SpurColorEncodings colorEncodings;					// Table of pixel encodings for each color
	SpurCursorImage cursorImage;						// Image data of hardware cursor
	SInt32 x, y;										// Current coordinates of cursor used to erase cursor
	Boolean cursorSet;									// Flag indicating that the cursor has been set up
	Boolean cursorRendered;								// Flag indicating that cursor is rendered in cursor buffer
														// Used to determine if erasing is necessary
	Boolean cursorCleared;								// True if HWCursor buffer has been cleared
	Boolean cursorVisible;								// True if cursor is visible on screen
	SInt32 deferredMove;								// True if we're deferring cursor movement to next VBL
														// Must be long and long aligned for CompareAndSwap
	SInt32 deferredX, deferredY;						// Coordinates to move cursor to on deferred move

	SpurColorMap colorMap;								// Color map for hardware cursor
	SpurColorMap transformedColorMap;					// luminance mapped and gamma corrected
};

//
// TemplateHALData
//	This structure contains the necessary items for the Template HAL to maintain its
//	state information.
//
//	This is COMPLETELY private to the Template implementation of the Graphics HAL.
//
typedef struct TemplateHALData TemplateHALData;
struct TemplateHALData
{
	RegEntryID regEntryIDPCI;					// save RegEntryID of the PCI device
	RegEntryID regEntryID;						// save our RegEntryID in case we need anything
	UInt32 hwBaseAddress;						// VRAM base address read from Base Register 0
	UInt32 vramSize;							// Amount of VRAM 1, 2, ..., 2048 megs

	DingusVideoMetaRegisters *dingusVideoMeta;
	DingusVideoRegisters *dingusVideo;			// description of DingusVideo registers

	UInt32 numDisplays;
	UInt32 ramPerDisplay;

	Ptr baseAddressPageCurrent;
	Ptr baseAddressPage[2];						// current FrameBufferBaseAddress of VRAM reported QD
	Ptr baseAddress;							// current FrameBufferBaseAddress of VRAM reported QD
	HWRegister32Bit*					senseLineEnable;

	DisplayModeID displayModeID;				// current displayModeID
	RGB savedClut[256];							// save the clut if DAC and PLL are powered down
	DisplayCode displayCode;					// class of the connected display
	DepthMode depthMode;						// current depthMode
	UInt16								currentPage;
	SInt16 width;								// current width for displayModeId
	SInt16 height;								// current height displayModeId
	UInt16 rowBytes;							// current rowbytes for displayModeId and depthMode
	UInt16 cvhSyncDisabled;						// c,v,h Bits : if set, sync is disabled
	UInt16								numPages;
	SInt16								startPosition;
	SInt16								endPosition;

	Boolean								interlaced;

	Boolean monoOnly;							// True if attached display only support Monochrome
	Boolean								compositSyncDisabled;
	Boolean								setClutAtVBL;
	Boolean								clutBusy;
	Boolean								setClutEntriesPending;
	Boolean								setCursorClutEntriesPending;
	Boolean								cursorClutTransformed;
	Boolean								isDDCC;
	Boolean								hardwareIsProgrammed;
	Boolean								needsEnableCBlank;

	Boolean								supports_640x480_60Hz;
	Boolean								supports_640x480_67Hz;
	Boolean								supports_800x600_60Hz;
	Boolean								supports_800x600_72Hz;
	Boolean								supports_800x600_75Hz;
	Boolean								supports_832x624_75Hz;
	Boolean								supports_1024x768_60Hz;
	Boolean								supports_1024x768_70Hz;
	Boolean								supports_1024x768_75Hz;
	Boolean								supports_1152x870_75Hz;
	Boolean								supports_1280x1024_75Hz;

	UInt8								ddcChecksum;

	RawSenseCode						rawSenseCode;
	ExtendedSenseCode					extendedSenseCode;

	Boolean								monitorIsBlanked;
	ControlRegSpec						regSpecs[kNumRegFieldsControl];
	SpurHardwareCursorData				hardwareCursorData;	// Record of data for hardware cursor
};


//
// HALReplacementDriverInfo
//	In the event that the driver is being superseded, the HAL will try and save this information so
//	that the driver's HAL which replaces it can attempt to come up in the same state, if possible.
//	This will help to minimize the visual artifacts that a users sees.
//
typedef struct HALReplacementDriverInfo HALReplacementDriverInfo;
struct HALReplacementDriverInfo
{
	Ptr					baseAddressPageCurrent;
	Ptr					baseAddressPage[2];
	DisplayModeID		displayModeID;
	DepthMode			depthMode;
	UInt16				currentPage;
	SInt16				width;
	SInt16				height;

	DisplayCode			displayCode;		// class of the connected display
	UInt16				cvhSyncDisabled;	// c,v,h Bits : if set, sync is disabled
	UInt16				numPages;
	Boolean				interlaced;
	Boolean				monoOnly;				// True if attached display only support Monochrome
	Boolean				compositSyncDisabled;
/*
	Ptr baseAddress;				// current FrameBufferBaseAddress...I hate using ptr
	UInt32 vdPowerState;			// current state of hw...on, off..etc
	SInt16 width;					// current width for displayModeId
	SInt16 height;					// current height displayModeId
	UInt16 rowBytes;				// current rowbytes for displayModeId and depthMode
	Boolean clutOff;				// 'true' if DAC and PLL are powered down
	Boolean toynbeeRunning;			// 'true' swatch is on and interrupts are firing
*/
};

TemplateHALData* GraphicsHALGetHALData(void);

// Naming conventions for functions:
//		* GraphicsHALxxx - functions which all HALs must implement.  These have external scope
//		* {Template}xxx  - functions which are strictly private to a specific HAL



static GDXErr TemplateMapDepthModeToCLUTAttributes(DepthMode depthMode,
				UInt32 *startAddress, UInt32 *entryOffset);

// This routine handles interrupts and is passed back as handler in GraphicsHAL GetVBLInterruptRoutines
static VBLHandler TemplateClearInternalVBLInterrupts;

static void TemplateWaitForVBL(void);
static GDXErr TemplateAssertVideoReset(void);

static GDXErr TemplateSetupClockGenerator(BitDepthIndependentData* bdiData);

static GDXErr TemplateSetPage();

static GDXErr TemplateSetupFBController(DepthMode depthMode, const DisplayInfo *info,
		const BitDepthIndependentData* bdiData, const WidthAndDepthDependentData *vwdData);

static GDXErr TemplateReleaseVideoReset(void);

static ExtendedSenseCode TemplateGetExtendedSenseCode(void);
static void TemplateResetSenseLines(void);
static void TemplateDriveSenseLines(SenseLine senseLine);
static RawSenseCode TemplateReadSenseLines(void);


static Boolean GraphicsHALCallbackReadSenseLine2(void);
static Boolean GraphicsHALCallbackReadSenseLine1(void);
static void GraphicsHALCallbackSenseLine2Set(void);
static void GraphicsHALCallbackSenseLine2Clear(void);
static void GraphicsHALCallbackSenseLine1Set(void);
static void GraphicsHALCallbackSenseLine1Clear(void);
static void GraphicsHALCallbackResetSenseLine2(void);
static void GraphicsHALCallbackResetSenseLine1(void);
static void GraphicsHALCallbackResetSenseLine2and1(void);
static void GraphicsHALCallbackSetDDCInfo(UInt8* ddcBlockData, UInt32 ddcBlockNumber);

static void GraphicsHALDetermineDisplayCo_2(RawSenseCode rawSenseCode, ExtendedSenseCode extendedSenseCode,
											Boolean *bool1, Boolean *bool2);


//****************************************************************************************************************



static GDXErr TemplateGetDisplayData(Boolean ignoreNotReady, DisplayModeID displayModeID, DepthMode depthMode,
	BitDepthIndependentData* bdiData, WidthAndDepthDependentData* vwdData, DisplayInfo* info);

static void DeferredMoveHardwareCursor(void);


// Here is typedef for a function used to retrieve BitDepthIndependentData & BitDepthDependentData
typedef GDXErr GetBDIAndBDDDataFunction(short index, DepthMode depthMode,
	BitDepthIndependentData* bdiData, WidthAndDepthDependentData* vwdData, DisplayInfo* info);

static GetBDIAndBDDDataFunction TemplateGet;



// HAL global, persistant data is stored in gTemplateHALData
TemplateHALData	gTemplateHALData;	// Persistant Template specific data storage


// The version of Template Graphics driver
enum
{
	kmajorRev = 1,
	kminorAndBugRev = 0x01,
	kstage = finalStage,				//developStage, alphaStage, betaStage, finalStage
	knonRelRev = 1

};


#pragma export on
// TheDriverDescription
// 	This structure describes the native driver.  Since This is a Graphics driver, the driver should
//	be loaded by the DisplayMgr's expert loader.  This is indicated by the driverOSRuntimeInfo
//	with the kdriverIsUnderExpertControl flag set.
DriverDescription TheDriverDescription =
{
	// driverDescSignature
		kTheDescriptionSignature,						// signature of DriverDescription

	// driverDescVersion
		kInitialDriverDescriptor,						// Version of this data structure

	// driverType
	{
		"\pDingusVideo",									// device name must match in Devicetree
		kmajorRev, kminorAndBugRev, kstage, knonRelRev,		// Major, Minor, Stage, Rev
	},


	// driverOSRuntimeInfo								// OS Runtime Requirements of Driver
		{
			kDriverIsOpenedUponLoad + kDriverIsUnderExpertControl,	// Runtime Options
			"\p.Display_Video_Dingus_Video",
		},


	// driverServices									// Apple Service API Membership

		// nServices
		1,												// Number of Services Supported

		// serviceCategory								// Service Category Name
		kServiceCategoryNdrvDriver,						// We support the 'ndrv' category

		// serviceType									// Type within Category
		kNdrvTypeIsVideo,								// Video type

		// serviceVersion								// Version of service
		1,0,0,0											// majorRev, minorAndBugRev, stage, nonRelRev

};
#pragma export reset

//=====================================================================================================
//
// GraphicsHALGetHALData()
//	Return the pointer to the global Core data.  Yes...you guessed it...it just returning
//	the address of the global, but use the accessor function anyway to isolate yourself
//	in the event CFM coolness is ever lost.
//
//=====================================================================================================
TemplateHALData  *GraphicsHALGetHALData(void)
{
	return (&gTemplateHALData);
}



//=====================================================================================================
//
// DoInitOneControlRegField()
//
//=====================================================================================================
static void DoInitOneControlRegField( UInt16 logicalRegNdx, UInt16 controlAddressOffset, UInt16 bitFieldSize, UInt16 bitFieldStart, Boolean isBitField, ControlRegSpec* regSpecs)
{
	regSpecs[logicalRegNdx].controlAddressOffset = controlAddressOffset;
	regSpecs[logicalRegNdx].mask = bitFieldSize == 32 ? -1 : (1 << bitFieldSize) - 1;
	regSpecs[logicalRegNdx].bitFieldStart = bitFieldStart;
	regSpecs[logicalRegNdx].isBitField = isBitField;
}



//=====================================================================================================
//
// ControlWriteRegister()
//
//=====================================================================================================
static void ControlWriteRegister( UInt32 logicalRegNdx, UInt32 value )
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	DingusVideoRegisters *dingusVideo = templateHALData->dingusVideo;

	UInt32 mask = templateHALData->regSpecs[logicalRegNdx].mask;
	HWRegister32Bit *regAddress = (HWRegister32Bit*)((UInt8*)dingusVideo + templateHALData->regSpecs[logicalRegNdx].controlAddressOffset);

	value &= mask;
	if (templateHALData->regSpecs[logicalRegNdx].isBitField)
	{
		*regAddress = (EndianSwap32Bit(*regAddress) &
			~(mask << templateHALData->regSpecs[logicalRegNdx].bitFieldStart)) |
			(value << templateHALData->regSpecs[logicalRegNdx].bitFieldStart);
	}
	else
		*regAddress = EndianSwap32Bit(value);
}



//=====================================================================================================
//
// CalcRowBytes()
//
//=====================================================================================================
static SInt16 CalcRowBytes(UInt32 width, DepthMode depthMode)
{
	UInt32 bitsPerPixel;
	if (noErr == GraphicsHALMapDepthModeToBPP(depthMode, &bitsPerPixel)) {
		UInt32 pixelsPer8Bytes = 64 / bitsPerPixel;
		UInt32 rowBytes = (width + pixelsPer8Bytes - 1) / pixelsPer8Bytes * 8;
		if (rowBytes <= 32767)
			return rowBytes;
	}
	return 0;
}



static SInt16 CalcFrameBytes(UInt32 width, UInt32 height, DepthMode depthMode)
{
	UInt32 rowBytes = CalcRowBytes(width, depthMode);
	UInt32 frameBytes = (rowBytes * height + 255) & ~255;
	return frameBytes;
}



static Ptr CalcPageBaseAddress(UInt32 Page)
{
	return NULL;
}


static void InitControlFields(void)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	// fill in the addresses for dingusVideo
	#define InitOneControlRegField(logicalRegNdx, controlAddressOffset, bitFieldSize, bitFieldStart, isBitField) \
		DoInitOneControlRegField(logicalRegNdx, controlAddressOffset * sizeof(UInt32), bitFieldSize, \
								bitFieldStart, isBitField, templateHALData->regSpecs)

	//                      kRegField enum								offset					    bits    pos     isBitField

	InitOneControlRegField( kRegFieldControlHACTIVE                 ,	HACTIVE             	,	32	,	0	,	false	);
	InitOneControlRegField( kRegFieldControlHSYNCBEGIN              ,	HSYNCBEGIN             	,	32	,	0	,	false	);
	InitOneControlRegField( kRegFieldControlHSYNCEND                ,	HSYNCEND               	,	32	,	0	,	false	);
	InitOneControlRegField( kRegFieldControlHTOTAL                  ,	HTOTAL              	,	32	,	0	,	false	);
	InitOneControlRegField( kRegFieldControlVACTIVE                 ,	VACTIVE             	,	32	,	0	,	false	);
	InitOneControlRegField( kRegFieldControlVSYNCBEGIN              ,	VSYNCBEGIN            	,	32	,	0	,	false	);
	InitOneControlRegField( kRegFieldControlVSYNCEND                ,	VSYNCEND               	,	32	,	0	,	false	);
	InitOneControlRegField( kRegFieldControlVTOTAL                  ,	VTOTAL              	,	32	,	0	,	false	);

	InitOneControlRegField( kRegFieldControlMON_SENSE_All			,	MON_SENSE				,	9	,	0	,	false	);	//	MON_Sens: includes information reflecting the state of the Monitor ID pins and whether particular Monitor outputs are enabled.
	InitOneControlRegField( kRegFieldControlMON_SENSE_1				,	MON_SENSE				,	3	,	6	,	true	);
	InitOneControlRegField( kRegFieldControlMON_SENSE_2				,	MON_SENSE				,	3	,	3	,	true	);
	InitOneControlRegField( kRegFieldControlMON_SENSE_3				,	MON_SENSE				,	3	,	0	,	true	);

	InitOneControlRegField( kRegFieldControlIMMEDIATE_FLAGS         ,	IMMEDIATE_FLAGS     	,	32	,	0	,	false	);
	InitOneControlRegField( kRegFieldControlPIXEL_CLOCK             ,	PIXEL_CLOCK         	,	32	,	0	,	false	);
	InitOneControlRegField( kRegFieldControlPIXEL_DEPTH             ,	PIXEL_DEPTH         	,	32	,	0	,	false	);
	InitOneControlRegField( kRegFieldControlFRAMEBUFFER_BASE        ,	FRAMEBUFFER_BASE    	,	32	,	0	,	false	);
	InitOneControlRegField( kRegFieldControlFRAMEBUFFER_ROWBYTES    ,	FRAMEBUFFER_ROWBYTES	,	32	,	0	,	false	);
	InitOneControlRegField( kRegFieldControlHWCURSOR_BASE           ,	HWCURSOR_BASE       	,	32	,	0	,	false	);
	InitOneControlRegField( kRegFieldControlHWCURSOR_WIDTH          ,	HWCURSOR_WIDTH      	,	32	,	0	,	false	);
	InitOneControlRegField( kRegFieldControlHWCURSOR_POS            ,	HWCURSOR_POS        	,	32	,	0	,	false	);
	InitOneControlRegField( kRegFieldControlCOLOR_INDEX             ,	COLOR_INDEX         	,	32	,	0	,	false	);
	InitOneControlRegField( kRegFieldControlCOLOR_DATA              ,	COLOR_DATA          	,	32	,	0	,	false	);

	InitOneControlRegField( kRegFieldControlTIMING_FLAGS_All		,	TIMING_FLAGS			,	32	,	0	,	false	);
	InitOneControlRegField( kRegFieldControlINTERLACED				,	TIMING_FLAGS			,	1	,	0	,	true	);
	InitOneControlRegField( kRegFieldControlVSYNC_POLARITY			,	TIMING_FLAGS			,	1	,	1	,	true	);
	InitOneControlRegField( kRegFieldControlHSYNC_POLARITY			,	TIMING_FLAGS			,	1	,	2	,	true	);
	InitOneControlRegField( kRegFieldControlHWCURSOR_24				,	TIMING_FLAGS			,	1	,	3	,	true	);


	InitOneControlRegField( kRegFieldControlIMMEDIATE_FLAGS_All		,	IMMEDIATE_FLAGS			,	32	,	0	,	false	);
	InitOneControlRegField( kRegFieldControlHWCURSOR_ENABLE			,	IMMEDIATE_FLAGS			,	1	,	0	,	true	);
	InitOneControlRegField( kRegFieldControlDISABLE_TIMING 			,	IMMEDIATE_FLAGS			,	1	,	1	,	true	);
	InitOneControlRegField( kRegFieldControlVSYNC_DISABLE  			,	IMMEDIATE_FLAGS			,	1	,	2	,	true	);
	InitOneControlRegField( kRegFieldControlHSYNC_DISABLE  			,	IMMEDIATE_FLAGS			,	1	,	3	,	true	);
	InitOneControlRegField( kRegFieldControlCSYNC_DISABLE  			,	IMMEDIATE_FLAGS			,	1	,	4	,	true	);
	InitOneControlRegField( kRegFieldControlBLANK_DISABLE  			,	IMMEDIATE_FLAGS			,	1	,	5	,	true	);
	InitOneControlRegField( kRegFieldControlDO_LATCH       			,	IMMEDIATE_FLAGS			,	1	,	6	,	true	);

	InitOneControlRegField( kRegFieldControlINT_ENABLE_All			,	INT_ENABLE				,	32	,	0	,	false	);	//	Interrupt Enable: enables/disables/clears a number of interrupts to the primary processor, including vertical blank interrupt; system bus write error interrupt; and expansion bus write error interrupt.
	InitOneControlRegField( kRegFieldControlVBL_IRQ_CLR				,	INT_ENABLE				,	1	,	0	,	true	);	//
	InitOneControlRegField( kRegFieldControlVBL_IRQ_EN				,	INT_ENABLE				,	1	,	1	,	true	);	//

	InitOneControlRegField( kRegFieldControlINT_STATUS_All			,	INT_STATUS				,	32	,	0	,	false	);	//	Interrupt Status: indicates which interrupts have occurred
	InitOneControlRegField( kRegFieldControlVBL_IRQ_STAT			,	INT_STATUS				,	1	,	0	,	true	);	//	kCursorInterruptStatusBit
	#undef InitOneControlRegField
}

static void InitVICs()
{
#define VIC(n, t) VideoIdentificationCodes[n] = t;
VIC(  1, kDisplay_640x480_60Hz_VGA      )
VIC(  2, kDisplay_720x480_60Hz          )
VIC(  3, kDisplay_720x480_60Hz          )
VIC(  4, kDisplay_1280x720_60Hz         )
VIC(  5, kDisplay_1920x1080i_60Hz       )
VIC(  6, kDisplay_1440x480i_60Hz        )
VIC(  7, kDisplay_1440x480i_60Hz        )
VIC(  8, kDisplay_1440x240_60Hz         )
VIC(  9, kDisplay_1440x240_60Hz         )
VIC( 10, kDisplay_2880x480i_60Hz        )
VIC( 11, kDisplay_2880x480i_60Hz        )
VIC( 12, kDisplay_2880x240_60Hz         )
VIC( 13, kDisplay_2880x240_60Hz         )
VIC( 14, kDisplay_1440x480_60Hz         )
VIC( 15, kDisplay_1440x480_60Hz         )
VIC( 16, kDisplay_1920x1080_60Hz        )
VIC( 17, kDisplay_720x576_50Hz          )
VIC( 18, kDisplay_720x576_50Hz          )
VIC( 19, kDisplay_1280x720_50Hz         )
VIC( 20, kDisplay_1920x1080i_50Hz       )
VIC( 21, kDisplay_1440x576i_50Hz        )
VIC( 22, kDisplay_1440x576i_50Hz        )
VIC( 23, kDisplay_1440x288_50Hz         )
VIC( 24, kDisplay_1440x288_50Hz         )
VIC( 25, kDisplay_2880x576i_50Hz        )
VIC( 26, kDisplay_2880x576i_50Hz        )
VIC( 27, kDisplay_2880x288_50Hz         )
VIC( 28, kDisplay_2880x288_50Hz         )
VIC( 29, kDisplay_1440x576_50Hz         )
VIC( 30, kDisplay_1440x576_50Hz         )
VIC( 31, kDisplay_1920x1080_50Hz        )
VIC( 32, kDisplay_1920x1080_24Hz        )
VIC( 33, kDisplay_1920x1080_25Hz        )
VIC( 34, kDisplay_1920x1080_30Hz        )
VIC( 35, kDisplay_2880x480_60Hz         )
VIC( 36, kDisplay_2880x480_60Hz         )
VIC( 37, kDisplay_2880x576_50Hz         )
VIC( 38, kDisplay_2880x576_50Hz         )
VIC( 39, kDisplay_1920x1080i_50Hz_72MHz )
VIC( 40, kDisplay_1920x1080i_100Hz      )
VIC( 41, kDisplay_1280x720_100Hz        )
VIC( 42, kDisplay_720x576_100Hz         )
VIC( 43, kDisplay_720x576_100Hz         )
VIC( 44, kDisplay_1440x576i_100Hz       )
VIC( 45, kDisplay_1440x576i_100Hz       )
VIC( 46, kDisplay_1920x1080i_120Hz      )
VIC( 47, kDisplay_1280x720_120Hz        )
VIC( 48, kDisplay_720x480_120Hz         )
VIC( 49, kDisplay_720x480_120Hz         )
VIC( 50, kDisplay_1440x480i_120Hz       )
VIC( 51, kDisplay_1440x480i_120Hz       )
VIC( 52, kDisplay_720x576_200Hz         )
VIC( 53, kDisplay_720x576_200Hz         )
VIC( 54, kDisplay_1440x576i_200Hz       )
VIC( 55, kDisplay_1440x576i_200Hz       )
VIC( 56, kDisplay_720x480_240Hz         )
VIC( 57, kDisplay_720x480_240Hz         )
VIC( 58, kDisplay_1440x480i_240Hz       )
VIC( 59, kDisplay_1440x480i_240Hz       )
VIC( 60, kDisplay_1280x720_24Hz         )
VIC( 61, kDisplay_1280x720_25Hz         )
VIC( 62, kDisplay_1280x720_30Hz         )
VIC( 63, kDisplay_1920x1080_120Hz       )
VIC( 64, kDisplay_1920x1080_100Hz       )
VIC( 65, kDisplay_1280x720_24Hz         )
VIC( 66, kDisplay_1280x720_25Hz         )
VIC( 67, kDisplay_1280x720_30Hz         )
VIC( 68, kDisplay_1280x720_50Hz         )
VIC( 69, kDisplay_1280x720_60Hz         )
VIC( 70, kDisplay_1280x720_100Hz        )
VIC( 71, kDisplay_1280x720_120Hz        )
VIC( 72, kDisplay_1920x1080_24Hz        )
VIC( 73, kDisplay_1920x1080_25Hz        )
VIC( 74, kDisplay_1920x1080_30Hz        )
VIC( 75, kDisplay_1920x1080_50Hz        )
VIC( 76, kDisplay_1920x1080_60Hz        )
VIC( 77, kDisplay_1920x1080_100Hz       )
VIC( 78, kDisplay_1920x1080_120Hz       )
VIC( 79, kDisplay_1680x720_24Hz         )
VIC( 80, kDisplay_1680x720_25Hz         )
VIC( 81, kDisplay_1680x720_30Hz         )
VIC( 82, kDisplay_1680x720_50Hz         )
VIC( 83, kDisplay_1680x720_60Hz         )
VIC( 84, kDisplay_1680x720_100Hz        )
VIC( 85, kDisplay_1680x720_120Hz        )
VIC( 86, kDisplay_2560x1080_24Hz        )
VIC( 87, kDisplay_2560x1080_25Hz        )
VIC( 88, kDisplay_2560x1080_30Hz        )
VIC( 89, kDisplay_2560x1080_50Hz        )
VIC( 90, kDisplay_2560x1080_60Hz        )
VIC( 91, kDisplay_2560x1080_100Hz       )
VIC( 92, kDisplay_2560x1080_120Hz       )
VIC( 93, kDisplay_3840x2160_24Hz        )
VIC( 94, kDisplay_3840x2160_25Hz        )
VIC( 95, kDisplay_3840x2160_30Hz        )
VIC( 96, kDisplay_3840x2160_50Hz        )
VIC( 97, kDisplay_3840x2160_60Hz        )
VIC( 98, kDisplay_4096x2160_24Hz        )
VIC( 99, kDisplay_4096x2160_25Hz        )
VIC(100, kDisplay_4096x2160_30Hz        )
VIC(101, kDisplay_4096x2160_50Hz        )
VIC(102, kDisplay_4096x2160_60Hz        )
VIC(103, kDisplay_3840x2160_24Hz        )
VIC(104, kDisplay_3840x2160_25Hz        )
VIC(105, kDisplay_3840x2160_30Hz        )
VIC(106, kDisplay_3840x2160_50Hz        )
VIC(107, kDisplay_3840x2160_60Hz        )
VIC(108, kDisplay_1280x720_48Hz         )
VIC(109, kDisplay_1280x720_48Hz         )
VIC(110, kDisplay_1680x720_48Hz         )
VIC(111, kDisplay_1920x1080_48Hz        )
VIC(112, kDisplay_1920x1080_48Hz        )
VIC(113, kDisplay_2560x1080_48Hz        )
VIC(114, kDisplay_3840x2160_48Hz        )
VIC(115, kDisplay_4096x2160_48Hz        )
VIC(116, kDisplay_3840x2160_48Hz        )
VIC(117, kDisplay_3840x2160_100Hz       )
VIC(118, kDisplay_3840x2160_120Hz       )
VIC(119, kDisplay_3840x2160_100Hz       )
VIC(120, kDisplay_3840x2160_120Hz       )
VIC(121, kDisplay_5120x2160_24Hz        )
VIC(122, kDisplay_5120x2160_25Hz        )
VIC(123, kDisplay_5120x2160_30Hz        )
VIC(124, kDisplay_5120x2160_48Hz        )
VIC(125, kDisplay_5120x2160_50Hz        )
VIC(126, kDisplay_5120x2160_60Hz        )
VIC(127, kDisplay_5120x2160_100Hz       )
VIC(193, kDisplay_5120x2160_120Hz       )
VIC(194, kDisplay_7680x4320_24Hz        )
VIC(195, kDisplay_7680x4320_25Hz        )
VIC(196, kDisplay_7680x4320_30Hz        )
VIC(197, kDisplay_7680x4320_48Hz        )
VIC(198, kDisplay_7680x4320_50Hz        )
VIC(199, kDisplay_7680x4320_60Hz        )
VIC(200, kDisplay_7680x4320_100Hz       )
VIC(201, kDisplay_7680x4320_120Hz       )
VIC(202, kDisplay_7680x4320_24Hz        )
VIC(203, kDisplay_7680x4320_25Hz        )
VIC(204, kDisplay_7680x4320_30Hz        )
VIC(205, kDisplay_7680x4320_48Hz        )
VIC(206, kDisplay_7680x4320_50Hz        )
VIC(207, kDisplay_7680x4320_60Hz        )
VIC(208, kDisplay_7680x4320_100Hz       )
VIC(209, kDisplay_7680x4320_120Hz       )
VIC(210, kDisplay_10240x4320_24Hz       )
VIC(211, kDisplay_10240x4320_25Hz       )
VIC(212, kDisplay_10240x4320_30Hz       )
VIC(213, kDisplay_10240x4320_48Hz       )
VIC(214, kDisplay_10240x4320_50Hz       )
VIC(215, kDisplay_10240x4320_60Hz       )
VIC(216, kDisplay_10240x4320_100Hz      )
VIC(217, kDisplay_10240x4320_120Hz      )
VIC(218, kDisplay_4096x2160_100Hz       )
VIC(219, kDisplay_4096x2160_120Hz       )
#undef VIC
}

//=====================================================================================================
//
// GraphicsHALInitPrivateData()
//	Allocate and initialize the HAL private data.
//
//		-> regEntryID	The NameRegistry ID for this device.
//
//		<> replacingDriver
//		On input, this indicates whether the Core got a 'kInitializeCommand' or a 'kReplaceCommand'.
//		These commands are similar, but with subtle differences.  A 'kInitializeCommand' is issued
//		if no version of this driver has been previously loaded, whereas a 'kReplaceCommand' is
//		issued if a previous version of the driver has been loaded, but subsequently superseded.
//
//		If 'false', then a 'kInitializeCommand' had been recieved by the Core, and the HAL should
//		do a full hardware initialization.
//
//		If 'true', then a 'kReplaceCommand' had been received by the Core, and the HAL can attempt
//		to configure itself to its state prior to it being superceded.
//
//		On output, this allows the HAL to override the Core's default behavior if it chooses to do so.
//
//		If 'false' then the HAL is signinaling the Core that it is unable to re-configure itself to
//		its state prior to being superseded, and the core will continue as if a 'kInitializeCommand'
//		had occurred.
//
//		If 'true', then the HAL was able to re-configure itself in the event of being replaced,
//		and the Core will proceeded accordingly.
//
//=====================================================================================================


GDXErr GraphicsHALInitPrivateData(const RegEntryID *regEntryID, Boolean *replacingDriver)
{


	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	HardwareCursorDescriptorPtr cursorDescriptor;
	UInt32 i;

	UInt32 applAddress[1];								// one logical addresses
	PCIAssignedAddress assignedAddresses[1];			// one "phys-addr size" pairs

#if 0
	UInt32 halPreferences;								// look at HAL data in nvram
#endif
	UInt16 commandRegister;								// enable Template's memory space if everything ok

	OSErr osErr;
	GDXErr err = kGDXErrUnableToAllocateHALData;		// assume failure

	// save the regEntryID
	RegistryEntryIDCopy(regEntryID, &templateHALData->regEntryID);

	// save the regEntryID of the parent which is the PCI device
	GetRegEntryParent(regEntryID, &templateHALData->regEntryIDPCI);

	// Open Firmware creates an "assigned-address" property that describes the Base Register size and physical address.
	err = GraphicsOSSGetProperty(&templateHALData->regEntryIDPCI, kPCIAssignedAddressProperty, assignedAddresses,
		sizeof(assignedAddresses));

	// if there is an error, that means Open Firmware was unable to allocate the memory space that was
	// requested.  Hence, Template is unusable at this time and the driver should quit.
	if (err)
		goto ErrorExit;

	// The order of the "phys-addr size" pairs in the "assigned-address" property match the order
	// of the logical addresses in the "AAPL,address" property.
	err = GraphicsOSSGetProperty(&templateHALData->regEntryIDPCI, "AAPL,address", applAddress, sizeof(applAddress));
	if (err)							// should NEVER be an error if gotten this far
		goto ErrorExit;

	templateHALData->hwBaseAddress = applAddress[0];
	templateHALData->vramSize = assignedAddresses[0].size.lo;

	// have successfully found all base addresses.
	// Enable Template's memory space.  Always do a read-modify-write when hitting configuration registers
	osErr = ExpMgrConfigReadWord(&templateHALData->regEntryIDPCI, (LogicalAddress) kPCIConfigCommand, &commandRegister);

	if (osErr)							// shouldn't be an error
		goto ErrorExit;

	commandRegister|= 1 << 1;			// enable the Memory space which is Bit 1

	osErr = ExpMgrConfigWriteWord(&templateHALData->regEntryIDPCI, (LogicalAddress) kPCIConfigCommand, commandRegister);
	if (err)							// shouldn't be an error
		goto ErrorExit;

	InitControlFields();
	InitVICs();

	templateHALData->dingusVideoMeta = (DingusVideoMetaRegisters *)templateHALData->hwBaseAddress;
	templateHALData->numDisplays = templateHALData->dingusVideoMeta->regsMeta[NUM_DISPLAYS];

	{
		UInt32 thisDisplay;
		RegCStrEntryNameBuf name;
		err = GraphicsOSSGetProperty(&templateHALData->regEntryID, "Name", &name, sizeof(name));
		if (err)							// it is possible to error out if Open Firmware couldn't
			goto ErrorExit;					// allocate the requested memory space.
		thisDisplay = name[12] - 'A';
		templateHALData->dingusVideo = &templateHALData->dingusVideoMeta->displays[thisDisplay];
	}

	{
		UInt32 regsSize = ((sizeof(DingusVideoMetaRegisters) + templateHALData->numDisplays * sizeof(DingusVideoRegisters)) + 4095) & ~4095;
		UInt32 ram = templateHALData->vramSize;
		ram -= regsSize;
		ram /= templateHALData->numDisplays;
		templateHALData->ramPerDisplay = ram & ~4095;
	}

	templateHALData->senseLineEnable = &templateHALData->dingusVideo->regsDisplay[MON_SENSE];

	// initialize hardwareCursorData
	cursorDescriptor = &(templateHALData->hardwareCursorData.cursorDescriptor);
	cursorDescriptor->majorVersion = 0;
	cursorDescriptor->minorVersion = 0;
	cursorDescriptor->height = 32;
	cursorDescriptor->width = 32;
	cursorDescriptor->bitDepth = 8;
	cursorDescriptor->maskBitDepth = 0;
	cursorDescriptor->numColors = kNumHardwareCursorColors;
	cursorDescriptor->colorEncodings = (UInt32 *) &(templateHALData->hardwareCursorData.colorEncodings);
	for (i = 0; i < kNumHardwareCursorColors; i++)
		cursorDescriptor->colorEncodings[i] = 2 + i;
	cursorDescriptor->flags = 0;
	cursorDescriptor->supportedSpecialEncodings =
			kTransparentEncodedPixel | kInvertingEncodedPixel;
	cursorDescriptor->specialEncodings[kTransparentEncoding] = 0;
	cursorDescriptor->specialEncodings[kInvertingEncoding] = 1;


	templateHALData->setCursorClutEntriesPending = false;
	templateHALData->cursorClutTransformed = false;


	if (*replacingDriver)
	{
		// Important Implementation Note:  The exact behavior of what should happen during driver
		// replacement is largely dependent of the implementation of the driver, and the reason for
		// it being replaced.
		// Under the ideal circumstances, a driver should be able to be replaced without having to
		// shut down the raster or reprogram the hardware.  This will prevent visible flashes on the
		// display during the replacement process.
		//
		// When a driver is being updated to incorporate new features (as opposed to bug fixes), then
		// replacement can usually occur without flashes.  This is because by default, the
		// Driver Loader Library (DLL) does not let QuickDraw know that a new driver got loaded, so
		// QuickDraw doesn't issue commands to gray the screen, etc. which result in 'flashes.'
		//
		// However, this default behavior is only acceptable if the new driver comes up in the same
		// bit depth and has the same 'base address' and 'rowbytes' as the driver it replaced.  If
		// this is not the case, then the DLL needs to be informed that a full initialization of all
		// the QuickDraw variables is required.  The DLL can be informed by setting uncommenting
		// the following lines:
		//
		// UInt32 needFullInit  = 1;
		// err = GraphicsOSSSaveProperty(&templateHALData->regEntryID, "needFullInit",
		//			&needFullInit, sizeof(needFullInit), kOSSPropertyVolatile);

		HALReplacementDriverInfo replacementDriverInfo;

		err = GraphicsOSSGetProperty(&templateHALData->regEntryID, "HALReplacementInfo",
				&replacementDriverInfo, sizeof(HALReplacementDriverInfo));

		if (!err)
		{
			templateHALData->depthMode = replacementDriverInfo.depthMode;
			templateHALData->baseAddressPageCurrent = replacementDriverInfo.baseAddressPageCurrent;
			templateHALData->baseAddressPage[0] = replacementDriverInfo.baseAddressPage[0];
			templateHALData->baseAddressPage[1] = replacementDriverInfo.baseAddressPage[1];
			templateHALData->displayModeID = replacementDriverInfo.displayModeID;
			templateHALData->currentPage = replacementDriverInfo.currentPage;
			templateHALData->width = replacementDriverInfo.width;
			templateHALData->height = replacementDriverInfo.height;
			templateHALData->displayCode = replacementDriverInfo.displayCode;
			templateHALData->cvhSyncDisabled = replacementDriverInfo.cvhSyncDisabled;
			templateHALData->numPages = replacementDriverInfo.numPages;
			templateHALData->interlaced = replacementDriverInfo.interlaced;
			templateHALData->monoOnly = replacementDriverInfo.monoOnly;
			templateHALData->compositSyncDisabled = replacementDriverInfo.compositSyncDisabled;

			templateHALData->rowBytes = CalcRowBytes(templateHALData->width, templateHALData->depthMode);

			templateHALData->hardwareIsProgrammed = true;

			// Have now grabbed all the old state information.  The new driver knows what bugs the old
			// driver had.  Examine the state information to see if it is necessary to do a full hw
			// initialization.  For example, one displayModeID was known to be bad...the hw was programmed
			// incorrectly for that resolution.
			// if ( brokenDisplayModeID == templateHALData->displayModeID)
			//		*replacingDriver = false;

			// The Template PCI card has power saving features, the clut can be turned off, VRAM refresh
			// can be turned off.  While it is supremely unlikely that a driver would be replaced while it
			// was in a low power state, do a sanity check to make sure the vdPowerState == kAVPowerOn.
			// Force a full initialization if in a low power state
			// if ( kAVPowerOn != templateHALData->vdPowerState)
			//		*replacingDriver = false;		// report that a Full initialization is necessary

		}
		else
		{
			*replacingDriver = false;					// report that a Full initialization is necessary
			err = kGDXErrNoError;
		}


	}	// end if replacingDriver = true


	if (*replacingDriver)
	{
		if (templateHALData->displayModeID == kDisplay_800x600_60Hz_VGA)
		{
			*replacingDriver = false;
			// a bogus displayModeID so that DingusVideo will always be hit on first call to ProgramHardware
			templateHALData->displayModeID = kDisplayModeIDInvalid;
		}
	}

	if (!*replacingDriver)
	{
		// If the driver is replacing an old driver, the raster was left on.  Don't really need to hit
		// hw since the old driver left all the necessary state information as shown above.


		// Template supports 3 VRAM configurations:
		// 1) 1 MB VRAM arranged as a 32-bit wide half bank (this is the base configuration on Template)
		// 2) 2 MB VRAM arranged as a 64-bit wide full bank
		// 4) 4 MB VRAM arranged as two 64-bit wide full banks

		// size your VRAM here....left as an exercise for the reader....odd numbered answers are in appendix C

		templateHALData->hardwareIsProgrammed = false;
		templateHALData->needsEnableCBlank = false;
		templateHALData->cvhSyncDisabled = kDPMSSyncOn; // 0
		templateHALData->numPages = 1;
	}

	// always try to delete the HALReplacementDriverInfo in the nameRegistry
	(void) GraphicsOSSDeleteProperty(&templateHALData->regEntryID, "HALReplacementInfo");


	if (*replacingDriver)
	{
		BitDepthIndependentData bdiData;
		UInt32 numberOfEntries;
		err = TemplateGetDisplayData(true, templateHALData->displayModeID, templateHALData->depthMode, &bdiData, nil, nil);

		switch (templateHALData->depthMode) {
			case kDepthMode1: numberOfEntries =   2; break;
			case kDepthMode2: numberOfEntries =   4; break;
			case kDepthMode3: numberOfEntries =  16; break;
			case kDepthMode4: numberOfEntries = 256; break;
			case kDepthMode5: numberOfEntries =  32; break;
			case kDepthMode6: numberOfEntries = 256; break;
		}

		templateHALData->dingusVideo->regsDisplay[COLOR_INDEX] = EndianSwap32Bit(CLUT_FRAMEBUFFER);	// Start at CLUT entry 0
		for (i = 0; i < numberOfEntries; i++)
		{
			UInt32 color = EndianSwap32Bit(templateHALData->dingusVideo->regsDisplay[COLOR_DATA]);
			templateHALData->savedClut[i].red   = (color >> 16) & 255;
			templateHALData->savedClut[i].green = (color >>  8) & 255;
			templateHALData->savedClut[i].blue  = (color >>  0) & 255;
		}
	}

	templateHALData->setClutEntriesPending = false;
	templateHALData->startPosition = 255;
	templateHALData->endPosition = 0;

	templateHALData->setClutAtVBL = true;
	templateHALData->clutBusy = false;


	// Template might want to look at the HAL specfic data stored in NVRAM.
	// The last 4 bytes (each device is allowed 8 bytes of nvram) is private to each HAL.
	// GraphicsOSSGetHALPref automatically fetches the last 4 bytes for the HAL
#if 0
	err = GraphicsOSSGetHALPref(&templateHALData->regEntryID, &halPreferences);
	if (!err)
	{
		// examine the haldata stored in nvram.  The HAL portion of the nvram is private to the HAL
		// (the core never examines or messes with it) and can have any meaning defined by the HAL.
		// If the HAL wishes to save data, it can use the routine GraphicsOSSSetHALPref
		// NOTE: the HAL should do a read-modify-write if it wishes to change the data.  This allows disk
		// based drivers that replace the ROM based drivers to define new HAL data.  Ensures that the ROM
		// based driver (always run at startup before the disk based driver) doesn't smash the data that the
		// disk based driver saved.
	}
#endif


	return kGDXErrNoError;								// everything ok

ErrorExit:
	RegistryEntryIDDispose(&templateHALData->regEntryID);
	return osErr;
}



//=====================================================================================================
//
// GraphicsHALKillPrivateData()
//
//	Disposes of the HAL's private data
//
//=====================================================================================================
void GraphicsHALKillPrivateData()
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	RegistryEntryIDDispose(&templateHALData->regEntryID);
}



//=====================================================================================================
//
// GraphicsHALOpen_2()
//
//=====================================================================================================

static void GraphicsHALOpen_2(void)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	DingusVideoRegisters *dingusVideo = templateHALData->dingusVideo;

/*
	// it is possible that the driver is replacing a ROM driver and the machine was
	// in low power mode, in which case the clut would be off.  Turn on the DAC and PLL

	*templateHALData->dingusVideo.address = kSpurPhaseLockLoopControl;	// DAC and PLL address

	*templateHALData->dingusVideo.multiPort = 1 << kSpurPhaseLockLoopControlCLKorPLLBit;
*/

	// turn off the hw cursor.  Other control routines will turn it on as necessary
	ControlWriteRegister(kRegFieldControlHWCURSOR_ENABLE, 0);

	ControlWriteRegister(kRegFieldControlHWCURSOR_POS, 0);

	dingusVideo->regsDisplay[COLOR_INDEX] = EndianSwap32Bit(CLUT_CURSOR); // Start at CLUT entry 0
	dingusVideo->regsDisplay[COLOR_DATA] = RGB_COLOR(0, 0, 0);
	dingusVideo->regsDisplay[COLOR_DATA] = RGB_COLOR(255, 0, 0);
	dingusVideo->regsDisplay[COLOR_DATA] = RGB_COLOR(0, 255, 0);
	dingusVideo->regsDisplay[COLOR_DATA] = RGB_COLOR(0, 0, 255);
	dingusVideo->regsDisplay[COLOR_DATA] = RGB_COLOR(255, 255, 0);
	dingusVideo->regsDisplay[COLOR_DATA] = RGB_COLOR(0, 255, 255);
	dingusVideo->regsDisplay[COLOR_DATA] = RGB_COLOR(255, 0, 255);
	dingusVideo->regsDisplay[COLOR_DATA] = RGB_COLOR(255, 255, 255);
}



//=====================================================================================================
//
// GraphicsHALOpen()
//	It is possible for the driver to be opened and closed many times.
//	The HAL private data has been setup, so just initialize the hardware into
//	the state that is should be in on startup.  This should be the complete initialization
//	to get the HW into the desired state for the amount of VRAM that is in the system and
//	any other hardware specfic stuff that the HAL cares about. The sense lines, for example, should
//	be able to be read and tweaked to determine the type of the connected monitor.
//	No programming to set up the raster for a givin 'DisplayModeID' or 'DepthMode' is necessary at
//	this point.
//
//		-> spaceID			The AddressSpaceID for this device.
//		-> replacingDriver	'true' if the HAL should behave as if the driver is being replaced,
//							'false' otherwise.
//
//=====================================================================================================
GDXErr GraphicsHALOpen(const AddressSpaceID /*spaceID*/, Boolean replacingDriver)
{

	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	DingusVideoRegisters *dingusVideo = templateHALData->dingusVideo;

	GDXErr err = kGDXErrNoError;

	if (!replacingDriver)
	{
		// default state
		GraphicsHALOpen_2();

		ControlWriteRegister(kRegFieldControlHACTIVE, 0);
		ControlWriteRegister(kRegFieldControlHSYNCBEGIN, 0);
		ControlWriteRegister(kRegFieldControlHSYNCEND, 0);
		ControlWriteRegister(kRegFieldControlHTOTAL, 0);
		ControlWriteRegister(kRegFieldControlVACTIVE, 0);
		ControlWriteRegister(kRegFieldControlVSYNCBEGIN, 0);
		ControlWriteRegister(kRegFieldControlVSYNCEND, 0);
		ControlWriteRegister(kRegFieldControlVTOTAL, 0);

		ControlWriteRegister(kRegFieldControlMON_SENSE_All, 7 << 3);
		ControlWriteRegister(kRegFieldControlMON_SENSE_1, 0);
		ControlWriteRegister(kRegFieldControlMON_SENSE_2, 0);
		ControlWriteRegister(kRegFieldControlMON_SENSE_3, 0);

		ControlWriteRegister(kRegFieldControlIMMEDIATE_FLAGS, 0);
		ControlWriteRegister(kRegFieldControlPIXEL_CLOCK, 0);
		ControlWriteRegister(kRegFieldControlPIXEL_DEPTH, 0);
		ControlWriteRegister(kRegFieldControlFRAMEBUFFER_BASE, 0);
		ControlWriteRegister(kRegFieldControlFRAMEBUFFER_ROWBYTES, 0);
		ControlWriteRegister(kRegFieldControlHWCURSOR_BASE, 0);
		ControlWriteRegister(kRegFieldControlHWCURSOR_WIDTH, 0);
		ControlWriteRegister(kRegFieldControlHWCURSOR_POS, 0);
		ControlWriteRegister(kRegFieldControlCOLOR_INDEX, 0);
		ControlWriteRegister(kRegFieldControlCOLOR_DATA, 0);

		ControlWriteRegister(kRegFieldControlTIMING_FLAGS_All, 0);

		ControlWriteRegister(kRegFieldControlIMMEDIATE_FLAGS_All, 0);

		ControlWriteRegister(kRegFieldControlINT_ENABLE_All, 0);

		ControlWriteRegister(kRegFieldControlINT_STATUS_All, 0);
	}

/*
	// internal interrupts in Template should be on so that HAL programs hw during vbl
	*dingusVideo->interruptMask = kCursorInterruptEnableMask;	// cursor interrupts on, vbl & animate off


	//	The phaseLockLoopControl register determines which of the two frequency register sets the PLL
	//	is actively using.  Bit kSpurPhaseLockLoopControlSetSelectBit selects bewteen set A and B.
	// 0 = set A, 1 = set B.  On reset, set A is selected as the active set.
	//	Since the non active set is always used to reprogram the clock, mark that Set A is active.

	templateHALData->usingClockSetA = true;

	// when opened, hw is up and running....vdPowerState is always kAVPowerOn as shown in Initialize
	templateHALData->vdPowerState = kAVPowerOn;
*/


	if (!replacingDriver)
	{
		// write some registers
	}

	// Reset cursor state.
	templateHALData->hardwareCursorData.deferredMove = false;
	templateHALData->hardwareCursorData.cursorSet = false;
	templateHALData->hardwareCursorData.cursorRendered = false;
	templateHALData->hardwareCursorData.cursorCleared = false;
	templateHALData->hardwareCursorData.cursorVisible = false;

	ControlWriteRegister(kRegFieldControlVBL_IRQ_CLR, 1);

	err = kGDXErrNoError;

ErrorExit:
	return err;
}



//=====================================================================================================
//
// GraphicsHALClose()
//	Upon close, there are no major requirements, since the majority of the work will be handled
//	elsewhere.
//
//=====================================================================================================
GDXErr GraphicsHALClose(const AddressSpaceID /*spaceID*/)
{
	return kGDXErrNoError;
}



//=====================================================================================================
//
// GraphicsHALTerminate()
//
//		-> superseded
//		'true' if current driver is going to be superseded by another driver, 'false' otherwise.
//		If 'true', the current driver can choose to save any state that the replacement driver
//		may need..if it wants to keep raster going.
//
//		'false' no driver is going to replace it.  In that event, it should stop the raster and
//		leave hardware in a 'polite' state.
//
//=====================================================================================================
GDXErr GraphicsHALTerminate(Boolean superseded)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	GDXErr err = kGDXErrNoError;

	if (superseded)
	{
		// Driver is being superseded, so save state information for the replacement driver.

		HALReplacementDriverInfo replacementDriverInfo;

/*
		replacementDriverInfo.vdPowerState = templateHALData->vdPowerState;
		replacementDriverInfo.rowBytes = templateHALData->rowBytes;
		replacementDriverInfo.clutOff = templateHALData->clutOff;
		replacementDriverInfo.toynbeeRunning = templateHALData->toynbeeRunning;
*/
		replacementDriverInfo.baseAddressPage[0] = templateHALData->baseAddressPage[0];
		replacementDriverInfo.baseAddressPage[1] = templateHALData->baseAddressPage[1];
		replacementDriverInfo.displayModeID = templateHALData->displayModeID;
		replacementDriverInfo.baseAddressPageCurrent = templateHALData->baseAddressPageCurrent;
		replacementDriverInfo.depthMode = templateHALData->depthMode;
		replacementDriverInfo.currentPage = templateHALData->currentPage;
		replacementDriverInfo.width = templateHALData->width;
		replacementDriverInfo.height = templateHALData->height;
		replacementDriverInfo.displayCode = templateHALData->displayCode;
		replacementDriverInfo.cvhSyncDisabled = templateHALData->cvhSyncDisabled;
		replacementDriverInfo.numPages = templateHALData->numPages;
		replacementDriverInfo.interlaced = templateHALData->interlaced;
		replacementDriverInfo.monoOnly = templateHALData->monoOnly;
		replacementDriverInfo.compositSyncDisabled = templateHALData->compositSyncDisabled;

		err = GraphicsOSSSaveProperty(&templateHALData->regEntryID, "HALReplacementInfo",
				&replacementDriverInfo, sizeof(HALReplacementDriverInfo), kOSSPropertyVolatile);

		if (err)
			err = TemplateAssertVideoReset();
	}
	else
	{
		// Driver is going away for good, so blank turn off state machines.
		err = TemplateAssertVideoReset();
	}

	return err;
}


//=====================================================================================================
//
// GraphicsHALGetVBLInterruptRoutines()
//	The OSS encapsulates how interrupts are handled by the system.  This routine supplies
//	that OSS with the HAL's interrupt routines that follow the OSS conventions.  Hopefully,
//	if the OS changes, only the OSS will need to change.
//
//		<- installVBLInterrupts
//		'true' 	if the HAL's interrupt scheme can match the OSS's scheme. i.e. the HAL lets the OSS
//		handle most of the interrupt functions.
//		'false'	if the HAL's interrupt scheme is radically different than the OSS's scheme.  The
//		HAL is responsible for knowing how the OS handles interrupts.  Obviously, this is the
//		escape mechanism for a poor OSS design.  The HAL needs a radically different interrupt handling
//		design so the OSS will not handle any interrupt services.  If this is false, all other
//		paramters are ignored
//
//		<- chainDefault
//		If 'halVBLEnabler' or 'halVBLDisabler' = NULL, this is ignored by the OSS for the respective
//		function since the default enabler/disabler supplied by the OS is used.
//		If 'chainDefault = true', if the halVBLEnabler or halVBLDisabler != NULL, the OSS will call
//		the default OS enabler/disbler after the HAL's enabler/disabler is called
//		If 'chainDefault = false', if the halVBLEnabler or halVBLDisabler != NULL, the OSS will NOT call
//		the default OS enabler/disbler after the HAL's enabler/disabler is called.  The HAL assumes
//		the responsibility for enabling/disabling the interrupt source.  (Dangerous!)
//
//		<- halVBLHandler
//		The HAL's VBL handler which should clear the internal interrupt source.
//
//		<- halVBLEnabler
//		If 'halVBLEnabler = NULL', the default OS enabler will be called and the HAL can ignore things.
//		If 'halVBLEnabler != NULL' and 'hainDefault = true', the HAL needs to enable the internal
//		interrupt source and the OSS calls the default OS enable routine to enable external interrupts.
//		If 'halVBLEnabler != NULL' and 'chainDefault = false', the HAL needs to enable the internal
//		and external interrupt source.  (Dangerous!)
//
//		<- halVBLDisabler
//		If 'halVBLDisabler = NULL', the default OS enabler will be called and the HAL can ignore things.
//		If 'halVBLDisabler != NULL' and 'chainDefault = true', the HAL can choose to disable the internal
//		interrupt source and the OSS calls the default OS disable routine to disable external interrupts.
//		If 'halVBLDisabler != NULL' and 'chainDefault = false', the HAL can choose to disable the internal
//		and must disable the external interrupt source.  (Dangerous!)
//
//		<- vblRefCon
//		If the HAL needs some data for the interrupt routines, allocate some structure that
//		is pointed to by vblRefCon.
//
//
//	Template, always has the internal interrupt source enabled so it knows when vbls occur.  Hence,
//	halVBLEnabler and halVBLDisabler = nil.  The default OS enable/disable routines are used to
//	control what interrupts the OS sees.
//
//=====================================================================================================
GDXErr GraphicsHALGetVBLInterruptRoutines(Boolean *installVBLInterrupts, Boolean *chainDefault,
		VBLHandler **halVBLHandler, VBLEnabler **halVBLEnabler, VBLDisabler **halVBLDisabler,
		void **vblRefCon)
{
	// Template is always going to have its internal interrupt source enabled for VBLs.  Hence,
	// 'halVBLEnabler' and 'halVBLDisabler' are NULL.  The internal interrupt will be
	// allowed/stopped to propagate by opening/closing the external interrupt 'gateway' via the
	// default OS enabler/disabler.

	*installVBLInterrupts = true;		// This HAL supports the OSS's interrupt scheme.
	*chainDefault = false;				// Ignored by OSS since HAL's enabler/disablers are NULL
	*halVBLHandler = TemplateClearInternalVBLInterrupts;
	*halVBLEnabler = NULL;
	*halVBLDisabler = NULL;
	*vblRefCon = NULL;					// No private refCon needed.

	return kGDXErrNoError;
}



//=====================================================================================================
//
// GraphicsHALGetUnknownRoutines()
//
//=====================================================================================================
GDXErr GraphicsHALGetUnknownRoutines(
		Boolean *reportsDDCConnection,
		BooleanProc *readSenseLine2Proc,
		BooleanProc *readSenseLine1Proc,
		VoidProc *senseLine2SetProc,
		VoidProc *senseLine2ClearProc,
		VoidProc *senseLine1SetProc,
		VoidProc *senseLine1ClearProc,
		VoidProc *senseLine2ResetProc,
		VoidProc *senseLine1ResetProc,
		VoidProc *senseLine2and1ResetProc,
		VoidProc *resetSenseLinesProc,
		RawSenseCodeProc *readSenseLinesProc,
		DDCPostProcessBlockProc *setDDCInfoProc
)
{
	GDXErr err = kGDXErrNoError;

	*reportsDDCConnection = true;

	*readSenseLine2Proc      = &GraphicsHALCallbackReadSenseLine2;
	*readSenseLine1Proc      = &GraphicsHALCallbackReadSenseLine1;
	*senseLine2SetProc       = &GraphicsHALCallbackSenseLine2Set;
	*senseLine2ClearProc     = &GraphicsHALCallbackSenseLine2Clear;
	*senseLine1SetProc       = &GraphicsHALCallbackSenseLine1Set;
	*senseLine1ClearProc     = &GraphicsHALCallbackSenseLine1Clear;
	*senseLine2ResetProc     = &GraphicsHALCallbackResetSenseLine2;
	*senseLine1ResetProc     = &GraphicsHALCallbackResetSenseLine1;
	*senseLine2and1ResetProc = &GraphicsHALCallbackResetSenseLine2and1;
	*resetSenseLinesProc     = &TemplateResetSenseLines;
	*readSenseLinesProc      = &TemplateReadSenseLines;
	*setDDCInfoProc          = &GraphicsHALCallbackSetDDCInfo;

	return err;
}



//=====================================================================================================
//
// GraphicsHALGetSupportedTimings()
//
//=====================================================================================================
static void GraphicsHALGetSupportedTimings(struct vbe_edid1_info* ddcBlockData)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	templateHALData->supports_640x480_60Hz   = ddcBlockData->established_timings.timing_640x480_60;
	templateHALData->supports_640x480_67Hz   = ddcBlockData->established_timings.timing_640x480_67;
	templateHALData->supports_800x600_60Hz   = ddcBlockData->established_timings.timing_800x600_60;
	templateHALData->supports_800x600_72Hz   = ddcBlockData->established_timings.timing_800x600_72;
	templateHALData->supports_800x600_75Hz   = ddcBlockData->established_timings.timing_800x600_75;
	templateHALData->supports_832x624_75Hz   = ddcBlockData->established_timings.timing_832x624_75;
	templateHALData->supports_1024x768_60Hz  = ddcBlockData->established_timings.timing_1024x768_60;
	templateHALData->supports_1024x768_70Hz  = ddcBlockData->established_timings.timing_1024x768_70;
	templateHALData->supports_1024x768_75Hz  = ddcBlockData->established_timings.timing_1024x768_75;
	templateHALData->supports_1152x870_75Hz  = ddcBlockData->manufacturer_timings.timing_1152x870_75;
	templateHALData->supports_1280x1024_75Hz = ddcBlockData->established_timings.timing_1280x1024_75;
	templateHALData->ddcChecksum = ddcBlockData->checksum;
}



//=====================================================================================================
//
// GraphicsHALGrayCLUT()
//
//	This routine sets all the CLUT entries to 50% gray (with gamma correction).
//	This is useful so that the pixel depth can be subsequently changed without
//	introducing screen anonmalies.
//	The 50% gray value will be obtained by using the midpoint value of the supplied
//	gamma table.
//
//	NOTE:  this assumes that the gamma correction data size is 1 byte,
//	as stated in the core.
//
//=====================================================================================================
GDXErr GraphicsHALGrayCLUT(const GammaTbl *gamma)
{

	// gray all 256 entries...it doesn't matter what depthMode we are in
	enum { kClutSize = 256 };

	GDXErr err = kGDXErrUnknownError;					// Assume failure

	UInt8 *midPointRed;									// Midpoint of red correction data
	UInt8 *midPointGreen;								//     "    "  green    "       "
	UInt8 *midPointBlue;								//     "    "  blue     "       "

	SInt16 channelCount = gamma->gChanCnt;
	SInt16 entriesPerChannel = gamma->gDataCnt;

	DingusVideoRegisters *dingusVideo;

	Boolean vblInterruptsEnabled = false;				// default to not enabled;

	UInt32 i;											// utterly famous iterator


	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	// Flush out any deferred cursor moving.
	DeferredMoveHardwareCursor();

	dingusVideo = templateHALData->dingusVideo;

	// Get the midpoint of the red correction data.  This is found by starting at the begining of the
	// correction data, which can be found at '&gFormulaData[0]', adding the 'gFormulaSize'.
	// Then go halfway into the table as determined by 'entriesPerChannel / 2'

	midPointRed = (UInt8 *) ((UInt32) &gamma->gFormulaData[0] + gamma->gFormulaSize +
			(entriesPerChannel / 2));

	// If there is only 1 channel of correction data, it means the same correction is applied to the
	// red, green, and blue channels.  If there are 3 channels then each color has its own correction
	// data.

	if (1 == channelCount)
	{
		midPointGreen = midPointRed;
		midPointBlue = midPointRed;
	} else {
		midPointGreen = midPointRed + entriesPerChannel;
		midPointBlue = midPointRed + (entriesPerChannel * 2);
	}


	// Turn off VBL interrupts so we can adjust the CLUT during VBL time, without having to worry
	// about the VBL interrupt handler stealing time from us.

	vblInterruptsEnabled = GraphicsOSSSetVBLInterrupt(false);
	TemplateWaitForVBL();

	dingusVideo->regsDisplay[COLOR_INDEX] = EndianSwap32Bit(CLUT_FRAMEBUFFER); // Start at CLUT entry 0

	// gray the clut.  Each time the address register is hit, delay 800 ns since the
	// clut is retrieving data at the new address.  The address register autoincrements
	// after blue is written, hence the delay
	for (i = 0 ; i < kClutSize ; i++)
	{
		dingusVideo->regsDisplay[COLOR_DATA] = RGB_COLOR(*midPointRed, *midPointGreen, *midPointBlue);
	}

	if (vblInterruptsEnabled)
		GraphicsOSSSetVBLInterrupt(true);

	err = kGDXErrNoError;								// Everything okay

ErrorExit:

	return err;
}



//=====================================================================================================
//
// GraphicsHALSetCLUT()
//
//	This routine will program the CLUT with the specified array of 'ColorSpecs'.
//	Two such arrays are provided, the original, and a second that has been luminance mapped (if
//	appropriate) and gamma corrected. It is up to the HAL implementation to decide which array should
//	be applied to the hardware. Most hardware will use the corrected version.
//
//	It is important to note that the positions of the entries refers to logical positions, not physical
//	ones. In 4-bits-per-pixel mode, for example, the entry positions could range from 0, 1, 3, ..., 15,
//	even though the physical positions may not have this number sequence.
//
//	No range checking is required, because the caller has allready done so.
//
//		-> originalCSTable
//		This is a pointer to the array of ColorSpecs provided by the caller. This is only provided in
//		the event that the hardware should not use the orrectedCSTable. If any adjuments need to made
//		to it, then they should be done to a copy. Don't throw away the const!
//
//		-> correctedCSTable
//		This is essentially a copy of originalCSTable, except that it has been luminance mapped (if
//		appropriate) and gamma corrected. Most hardware will use this information to set the CLUT.
//		Though it is unlikely that you will need to change this information, it is not marked as
//		'const' in case you need to build a special version from the originalCSTable.  In that event,
//		you can alter the array as you see fit.  (Note:  regardless of the size of the originalCSTable,
//		correctedCSTable points to an array of ColorSpecs with 256 entries.
//
//		-> startPosition
//		(0 based) Starting point in the array of ColorSpecs
//
//		-> numberOfEntries
//		(0 based) This is the number of entries to be set.
//
//		-> sequential
//		If 'false', then the 'value' field of the ColorSpec should be inspected to see what logical
//		position should be set. If 'true', then the array index indicates what logical position should
//		be set.
//
//		-> depthMode
//		The relative bit depth. This is provided so that the HAL can decide how to map the logical
//		entry positions to the physcial entry postions.
//
//=====================================================================================================
GDXErr GraphicsHALSetCLUT(const ColorSpec *originalCSTable, ColorSpec *correctedCSTable,
				SInt16 startPosition, SInt16 numberOfEntries, Boolean sequential, DepthMode depthMode)
{
	#pragma unused( originalCSTable, depthMode )

	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	DingusVideoRegisters *dingusVideo;

	Boolean vblInterruptsEnabled = false;				// default to not enabled;
	UInt32 logicalAddress;

	UInt32 i;									// Loop control variable

	SInt16 endPosition;

	UInt8 maxAddress;
	UInt8 minAddress;

	GDXErr err = kGDXErrUnknownError;			// Assume failure.

	templateHALData->clutBusy = true;

	if (templateHALData->cursorClutTransformed)
	{
		templateHALData->cursorClutTransformed = false;
		templateHALData->setCursorClutEntriesPending = true;
	}

	if (templateHALData->setClutAtVBL)
	{
		// save to the "virtual" CLUT
		if (sequential)
		{
			if ( templateHALData->setClutEntriesPending )
			{
				if ( templateHALData->startPosition > startPosition )
					templateHALData->startPosition = startPosition;
				endPosition = startPosition + numberOfEntries;
				if ( templateHALData->endPosition < endPosition )
					templateHALData->endPosition = endPosition;
			}
			else
			{
				templateHALData->startPosition = startPosition;
				endPosition = startPosition + numberOfEntries;
				templateHALData->endPosition = endPosition;
			}
			for (i = startPosition ; i <= endPosition; i++)
			{
				// correctedCSTable had the ms byte of an original 16 bit red, blue or green in its ls byte
				templateHALData->savedClut[i].red = correctedCSTable[i].rgb.red;
				templateHALData->savedClut[i].green = correctedCSTable[i].rgb.green;
				templateHALData->savedClut[i].blue = correctedCSTable[i].rgb.blue;
			}
		}
		else
		{
			maxAddress = 0;
			minAddress = 255;
			endPosition = startPosition + numberOfEntries;
			if (startPosition <= endPosition)
			{
				for (i = startPosition ; i <= endPosition; i++)
				{
					logicalAddress = correctedCSTable[i].value;
					if (logicalAddress < minAddress)
						minAddress = logicalAddress;
					if (logicalAddress > maxAddress)
						maxAddress = logicalAddress;
					// correctedCSTable had the ms byte of an original 16 bit red, blue or green in its ls byte
					templateHALData->savedClut[logicalAddress].red = correctedCSTable[i].rgb.red;
					templateHALData->savedClut[logicalAddress].green = correctedCSTable[i].rgb.green;
					templateHALData->savedClut[logicalAddress].blue = correctedCSTable[i].rgb.blue;
				}
			}
			if (templateHALData->endPosition < maxAddress)
				templateHALData->endPosition = maxAddress;
			if (templateHALData->startPosition > minAddress)
				templateHALData->startPosition = minAddress;
		}
		templateHALData->setClutEntriesPending = true;
	}
	else
	{
	// Turn off VBL interrupts so we can adjust the CLUT during VBL time, without having to worry
	// about the VBL interrupt handler stealing time from us.

		vblInterruptsEnabled = GraphicsOSSSetVBLInterrupt(false);
		TemplateWaitForVBL();

		// Flush out any deferred cursor moving.
		DeferredMoveHardwareCursor();

		dingusVideo = templateHALData->dingusVideo;

		if (sequential)
		{
			dingusVideo->regsDisplay[COLOR_INDEX] = EndianSwap32Bit(CLUT_FRAMEBUFFER + startPosition);

			endPosition = startPosition + numberOfEntries;

			if (startPosition <= endPosition)
			{
				// Program the CLUT entries.  For our hardware, use the correctedCSTable
				for (i = startPosition ; i <= endPosition; i++)
				{
					templateHALData->savedClut[i].red = correctedCSTable[i].rgb.red;
					templateHALData->savedClut[i].green = correctedCSTable[i].rgb.green;
					templateHALData->savedClut[i].blue = correctedCSTable[i].rgb.blue;
					dingusVideo->regsDisplay[COLOR_DATA] = RGB_COLOR(correctedCSTable[i].rgb.red, correctedCSTable[i].rgb.green, correctedCSTable[i].rgb.blue);
				}
			}
		}
		else
		{
			endPosition = startPosition + numberOfEntries;
			if (startPosition <= endPosition)
			{
				// Program the CLUT entries.  For our hardware, use the correctedCSTable
				for (i = startPosition ; i <= endPosition; i++)
				{
					logicalAddress = correctedCSTable[i].value;
					dingusVideo->regsDisplay[COLOR_INDEX] = EndianSwap32Bit(CLUT_FRAMEBUFFER + logicalAddress);

					templateHALData->savedClut[logicalAddress].red = correctedCSTable[i].rgb.red;
					templateHALData->savedClut[logicalAddress].green = correctedCSTable[i].rgb.green;
					templateHALData->savedClut[logicalAddress].blue = correctedCSTable[i].rgb.blue;
					dingusVideo->regsDisplay[COLOR_DATA] = RGB_COLOR(correctedCSTable[i].rgb.red, correctedCSTable[i].rgb.green, correctedCSTable[i].rgb.blue);
				}
			}
		}
		if (vblInterruptsEnabled)
			GraphicsOSSSetVBLInterrupt(true);
	}
	templateHALData->clutBusy = false;

	err = kGDXErrNoError;							// Everything okay

ErroExit:

	return err;
}



//=====================================================================================================
//
// GraphicsHALGetCLUT()
//
//	This routine will fill out the specified array of ColorSpecs with the contents of the CLUT.
//	The 'RGBColor' structure in each 'ColorSpec' uses 16-bits for each channel (red, green, and blue),
//	whereas most CLUTs only use 8-bits. Therefore, when filling in the 'RGBColor' structure, the most
//	significant byte for each channel should be filled with the 8-bits extracted from its respective
//	channel in the CLUT. Moreover, to maintain the same behavior as the reference model, the 8-bits
//	from the CLUT should also be written to the least significant byte for each RGBColor.
//
//	It is important to note that the positions of the entries refer to logical positions, not physical
//	ones. At 4 bpp, for example, the entry positions could range from 0, 1, 2, ..., 15, even though the
//	physical positions may not have this number sequence.
//
//	No range checking is required, because the caller has already done so.
//
//		<> csTable
//		Pointer to array of 'ColorSpecs' provided by the caller to be filled with the CLUT contents.
//
//		-> startPosition	(0 based) Starting point in the array to fill.
//		-> numberOfEntries	(0 based) The number of entries to be get.
//
//		-> sequential
//		If 'false', then the value field of the 'ColorSpec' should be inspected to see what logical
//		position should be retrieved. If 'true', then the array index indicates what logical position
//		should be read.
//
//		-> depthMode
//		The relative bit depth. This is provided so that the HAL can decide how to map the logical
//		entry positions to the physical entry postions.
//
//=====================================================================================================
GDXErr GraphicsHALGetCLUT(ColorSpec *csTable, SInt16 startPosition, SInt16 numberOfEntries,
		Boolean sequential, DepthMode depthMode)
{
	#pragma unused( depthMode )

	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	UInt32 logicalAddress;

	UInt32 i;											// Loop control variable

	GDXErr err = kGDXErrUnknownError;					// Assume failure.

	// return the "virtual" clut entries
	for (i = startPosition ; i <= (startPosition + numberOfEntries); i++)
	{
		if (sequential)
			logicalAddress = i;
		else
			logicalAddress = csTable[i].value;

		csTable[i].rgb.red = templateHALData->savedClut[logicalAddress].red;		//		(xxrr)
		csTable[i].rgb.red |= (csTable[i].rgb.red << 8);	// Copy it to most sig. byte 	(rrrr)

		csTable[i].rgb.green = templateHALData->savedClut[logicalAddress].green;	//		(xxgg)
		csTable[i].rgb.green |= (csTable[i].rgb.green << 8);// Copy it to most sig. byte 	(gggg)

		csTable[i].rgb.blue = templateHALData->savedClut[logicalAddress].blue;		//		(xxbb)
		csTable[i].rgb.blue |= (csTable[i].rgb.blue << 8);	// Copy it to most sig. byte 	(bbbb)
	}

	err = kGDXErrNoError;										// Everything okay

ErrorExit:

	return err;
}



//=====================================================================================================
//
// GraphicsHALGetPages()
//
//	This routine reports the number of graphics pages supported for the specified 'DisplayModeID' at
//	the specified 'DepthMode'.
//	No attempt should be made to determine whether or not a display capable of being driven with
//	a raster of type 'DisplayModeID' is physically connected.
//
//		-> displaymodeID	The DisplayModeID for which the page count is desired.
//		-> depthMode		The relative bit depth for which the page count is desired.
//
//		<- pageCount
//		# of pages supported at the specified 'DisplayModeID' and 'DepthMode'.  In the event of an
//		error, 'pageCount' is undefined.  This is a counting number, so it is NOT zero based.
//
//=====================================================================================================
GDXErr GraphicsHALGetPages(DisplayModeID displayModeID, DepthMode depthMode, SInt16 *pageCount)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	WidthAndDepthDependentData vwdData;			// dependent on VRAM width and depthMode

	GDXErr err = TemplateGetDisplayData(true, displayModeID, depthMode,
		nil, &vwdData, nil);
	if (err)										// unknown displayModeID OR vramSize
	{
		// Opps...caller specified an invalid 'DisplayModeID' or 'DepthMode'
		err = kGDXErrInvalidParameters;
		goto ErrorExit;
	}

	*pageCount = vwdData.pages;

	err = kGDXErrNoError;									// Everything okay

ErrorExit:

	return err;
}



//=====================================================================================================
//
// GraphicsHALGetBaseAddress()
//
//	This returns the base address of a specified page in the current mode.
//	This allows video pages to be written to even when not displayed
//
//			->	page			page number ( 0 based ).  Return the base address for this page
//			<-	baseAddress		base address of desired page
//
//=====================================================================================================
GDXErr GraphicsHALGetBaseAddress(SInt16 page, char **baseAddress)
{


	GDXErr err = kGDXErrUnknownError;			// Assume failure.
	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	if (page < 0 || page > 1)
	{
		// caller asked for baseAddress of invalid page
		err = kGDXErrInvalidParameters;
		goto ErrorExit;
	}

	// the base address saved in the HAL data is the base address reported to Quickdraw
	// the base address in TemplateGetXXX routines is the base address that Toynbee is programmed with
	// Relationship: QD base address = Toynbee base address
	if (page == 0)
		*baseAddress = (char *)(templateHALData->baseAddressPage[0]);
	else
		*baseAddress = (char *)(templateHALData->baseAddressPage[1]);

	err = kGDXErrNoError;									// Everything okay

ErrorExit:

	return err;
}



//=====================================================================================================
//
// GraphicsHALGetSync()
//
//	Multipurpose call to
//	1) Report the capabilities of the frame buffer in controlling the sync lines and if HW can
//	"sync" on Red, Green or Blue
//	2) Report the current status of the sync lines and if HW is "syncing" on Red, Green or Blue
//
//	If the display supported the VESA Device Power Management Standard (DPMS), it would respond
//	to HSync and VSync in the following manner:
//	The VESA Standards are:
//
//	State       	Vert Sync		Hor Sync		Video
//	-----			--------		---------		------
//	Active         	Pulses      	Pulses      	Active
//	Standby			Pulses   		No Pulses      	Blanked
//	Idle			No Pulses      	Pulses			Blanked
//	Off			 	No Pulses		No Pulses		Blanked
//
//
//			->	getHardwareSyncCapability	true if reporting HW capability
//											false if reporting current status
//
//	For this routine, the relevant fields of the 'VDSyncInfoRec' structure are as follows:
//			<-	sync		report HW capability or current state.
//
//				if getHardwareSyncCapability then report the cabability of the HW
//				When reporting the capability of the HW, set the appropriate bits of csMode:
//				kDisableHorizontalSyncBit		set if HW can disable Horizontal Sync
//				kDisableVerticalSyncBit			set if HW can disable Vertical Sync
//				kDisableCompositeSyncBit		set if HW can disable Composite Sync
//				kSyncOnRedEnableBit				set if HW can sync on Red
//				kSyncOnGreenEnableBit			set if HW can sync on Green
//				kSyncOnBlueEnableBit			set if HW can sync on Blue
//				kNoSeparateSyncControlBit		set if HW CANNOT enable/disable H,V,C sync independently
//												Means that HW ONLY supports the VESA DPMS "OFF" state
//
//				if !getHardwareSyncCapability then report the current state of sync lines and  if HW is
//				"syncing" on Red, Green or Blue
//				Reporting the "current state of the sync lines" means: "Report the State of the Display"
//				To report the current state of the display, the kDisableHorizontalSyncBit and
//				the kDisableVerticalSyncBit have the following values:
//
//				State       	kDisableVerticalSyncBit		kDisableHorizontalSyncBit		Video
//				-----			-----------------------		-------------------------		------
//				Active       		  	0      						0      					Active
//				Standby					0   						1		   			   	Blanked
//				Idle					1     			 			0						Blanked
//				Off			 			1							1						Blanked
//
//				to report if HW is "syncing" on Red, Green or Blue:
//				kSyncOnRedEnableBit				set if HW is "syncing" on Blue
//				kSyncOnGreenEnableBit			set if HW is "syncing" on Green
//				kSyncOnBlueEnableBit			set if HW is "syncing" on Blue
//
//
//=====================================================================================================
GDXErr GraphicsHALGetSync(Boolean getHardwareSyncCapability, VDSyncInfoRec *sync)
{
	if (getHardwareSyncCapability)
	{
		// report capability of the hardware.  Template CAN control H,V,C sync independently and
		// is UNABLE to sync on Red/Green/Blue
		sync->csMode = 0
			| (1 << kDisableHorizontalSyncBit)
			| (1 << kDisableVerticalSyncBit  )
			| (1 << kDisableCompositeSyncBit )
			;
	}
	else
	{
		TemplateHALData *templateHALData = GraphicsHALGetHALData();
		// Report the current status.  Since Template does not support
		// the sync on Red/Green/Blue bits, just return the DPMS status
		sync->csMode = (UInt8)templateHALData->cvhSyncDisabled;
	}

ErrorExit:

	return kGDXErrNoError;
}



//=====================================================================================================
//
// GraphicsHALSetSync_2()
//
//=====================================================================================================
static Boolean GraphicsHALSetSync_2(void)
{
//	possible GraphicsHALDetermineDisplayCode ?
	Boolean monitorConnected;
	RawSenseCode rawSenseCode;						// Result from reading sense lines after reset
	ExtendedSenseCode extendedSenseCode;			// Result from applying extended sense algorithm
	DisplayCode displayCode;
	struct vbe_edid1_info ddcBlockData;

	GDXErr err = kGDXErrNoError;					// Assume sucess

	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	templateHALData->monitorIsBlanked = false;
	monitorConnected = false;

	TemplateResetSenseLines();
	rawSenseCode = TemplateReadSenseLines();
	extendedSenseCode = TemplateGetExtendedSenseCode();
	if (rawSenseCode == kRSCSeven && extendedSenseCode == kESCSevenNoDisplay)
		return(false);

	err = GraphicsUtilMapSenseCodesToDisplayCode(rawSenseCode, extendedSenseCode, false, &displayCode);
	if (rawSenseCode != templateHALData->rawSenseCode || extendedSenseCode != templateHALData->extendedSenseCode)
	{
		templateHALData->isDDCC = false;
		monitorConnected = true;
		SetDDCAndBuiltInFlags(false, false);
	}
	if ((rawSenseCode == kRSCSix && extendedSenseCode == kESCSixStandard) ||
		(rawSenseCode == kRSCSeven && extendedSenseCode == kESCSevenDDC))
	{
		ControlWriteRegister(kRegFieldControlBLANK_DISABLE, 1);	// disable CBlank
		ControlWriteRegister(kRegFieldControlVSYNC_DISABLE, 0);	// enable vertical sync pulses
		ControlWriteRegister(kRegFieldControlHSYNC_DISABLE, 0);	// enable horizontal sync pulses
		GraphicsUtilSetSync_2();
		if (GraphicsUtilGetDDCBlock_2a(1,(UInt8*)&ddcBlockData))
		{
			if (templateHALData->isDDCC)
			{
				templateHALData->isDDCC = false;
				monitorConnected = true;
				SetDDCAndBuiltInFlags(false, false);
				templateHALData->ddcChecksum = 0;
			}
		}
		else
		{
			if (!templateHALData->isDDCC)
			{
				templateHALData->isDDCC = true;
				monitorConnected = true;
				SetDDCAndBuiltInFlags(true, false);
				GraphicsHALGetSupportedTimings(&ddcBlockData);
			}
			else if (ddcBlockData.checksum != templateHALData->ddcChecksum)
			{
				monitorConnected = true;
				SetDDCAndBuiltInFlags(true, false);
				GraphicsHALGetSupportedTimings(&ddcBlockData);
			}
		}
	}
	if (monitorConnected)
	{
		if ( (kDisplayCode21InchMono == displayCode) || (kDisplayCodePortraitMono == displayCode) )
			templateHALData->monoOnly = true;
		else
			templateHALData->monoOnly = false;
		templateHALData->displayCode = displayCode;
		SetDisplayProperties(displayCode, templateHALData->monoOnly);
		templateHALData->rawSenseCode = rawSenseCode;
		templateHALData->extendedSenseCode = extendedSenseCode;
	}
	return monitorConnected;
}



//=====================================================================================================
//
// GraphicsHALSetSync()
//	If the display supported the VESA Device Power Management Standard (DPMS), it would respond
//	to HSync and VSync in the following manner:
//
//	The VESA Standards are:
//
//	State       	Vert Sync		Hor Sync		Video
//	-----			--------		---------		------
//	Active         	Pulses      	Pulses      	Active
//	Standby			Pulses   		No Pulses      	Blanked
//	Idle			No Pulses      	Pulses			Blanked
//	Off			 	No Pulses		No Pulses		Blanked
//
//
//			->	syncBitField		bit field of the sync bits that need to be disabled/enabled
//
//				kDisableHorizontalSyncBit		set if HW should disable Horizontal Sync (No Pulses)
//				kDisableVerticalSyncBit			set if HW should disable Vertical Sync (No Pulses)
//				kDisableCompositeSyncBit		set if HW should disable Composite Sync (No Pulses)
//				kSyncOnRedEnableBit				set if HW should sync on Red
//				kSyncOnGreenEnableBit			set if HW should sync on Green
//				kSyncOnBlueEnableBit			set if HW should sync on Blue
//
//
//			->	syncBitFieldValid	mask of the bits that are valid in the csMode bit field
//
//
//=====================================================================================================
GDXErr GraphicsHALSetSync(UInt8 syncBitField, UInt8 syncBitFieldValid)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	UInt16 cvhSyncDisabled = templateHALData->cvhSyncDisabled;
	UInt16 disableCompositeSync;
	Boolean monitorConnected;

	GDXErr err = kGDXErrUnknownError;								// Assume failure

	// Template is UNABLE to sync on Red/Green/Blue and reported that on the GetSync status call.
	// if any of the kEnableSyncOnBlue, kEnableSyncOnGreen or kEnableSyncOnRed bits in csFlags
	// are set than return an error.

	if ((kSyncOnMask | kTriStateSyncMask) & syncBitFieldValid)
	{
		err = kGDXErrInvalidParameters;
		goto ErrorExit;
	}

	disableCompositeSync = (syncBitField & kCompositeSyncMask) >> kDisableCompositeSyncBit;
	if (templateHALData->compositSyncDisabled)
		disableCompositeSync = 1;

	// if HW did not support the disabling of c,v,h sync independently, the HAL should check:
	// that bits kDisableCompositeSyncBit, kDisableVerticalSyncBit, kDisableHorizontalSyncBit in
	// 1) syncBitFieldValid are ALL marked as valid
	// 2) syncBitField are ALL the same state


	if (kCompositeSyncMask & syncBitFieldValid)
	{
		// save state of compositeSync
		cvhSyncDisabled = (cvhSyncDisabled & ~kCompositeSyncMask) |
							 (disableCompositeSync << kDisableCompositeSyncBit);
	}
	if (kVerticalSyncMask & syncBitFieldValid)
	{
		// save state of verticalSync
		cvhSyncDisabled =	(cvhSyncDisabled & ~kVerticalSyncMask) |
						(syncBitField & kVerticalSyncMask);
	}
	if (kHorizontalSyncMask & syncBitFieldValid)
	{
		// save state of horizontalSync
		cvhSyncDisabled =	(cvhSyncDisabled & ~kHorizontalSyncMask) |
			(syncBitField & kHorizontalSyncMask);
	}

	monitorConnected = false;
	if (0 == (cvhSyncDisabled & (kHorizontalSyncMask | kVerticalSyncMask)))
	{																	// if both syncs are on (display is active)
		if (templateHALData->monitorIsBlanked)
			monitorConnected = GraphicsHALSetSync_2();
		ControlWriteRegister(kRegFieldControlBLANK_DISABLE, 0);			// then enable CBlank
	}
	else
	{
		ControlWriteRegister(kRegFieldControlBLANK_DISABLE, 1);			// else disable CBlank
	}

	// Template enables a sync by writing a 0 to the sync.  So the value of each sync is cvh bit in
	// in cvhSyncDisabled.  For each sync in DingusVideo, 0 = enabled, 1 = disabled
	ControlWriteRegister(kRegFieldControlCSYNC_DISABLE, (cvhSyncDisabled & kCompositeSyncMask ) >> kDisableCompositeSyncBit); 		// composite pulses
	ControlWriteRegister(kRegFieldControlVSYNC_DISABLE, (cvhSyncDisabled & kVerticalSyncMask  ) >> kDisableVerticalSyncBit); 		// vertical pulses
	ControlWriteRegister(kRegFieldControlHSYNC_DISABLE, (cvhSyncDisabled & kHorizontalSyncMask) >> kDisableHorizontalSyncBit); 		// horizontal pulses

	templateHALData->cvhSyncDisabled = cvhSyncDisabled;

	if (monitorConnected)
	{
		GraphicsOSSInterruptHandler(kConnectInterruptServiceType);
		cvhSyncDisabled = templateHALData->cvhSyncDisabled;
	}

	if ((cvhSyncDisabled & (kHorizontalSyncMask | kVerticalSyncMask)) == (kHorizontalSyncMask | kVerticalSyncMask))
	{
		// if both syncs are off (display is Blanked)
		templateHALData->monitorIsBlanked = true;
	}

	err = kGDXErrNoError;								// Everything okay

ErrorExit:

	return err;
}



//=====================================================================================================
//
// GraphicsHALGetModeTiming()
//
//	This is used to to gather scan timing information.  For the specified displayModeID, report
//	the timingFlags for the connected display.  If the HAL doesn't know if the display supports the
//	displayModeID, timingFlags should be 0 (kModeValid and kModeSafe bits should NOT be set).  That
//	indicates to the Display Mgr that the driver doesn't think the display can handle the resolution,
//	but it will let the Display Mgr make the final decision.
//
//			->	displayModeID	Describes the display resolution and scan timing
//
//			<-	timingFormat	Format of the info in 'timingData' field, only 'kDeclROMtables' is valid.
//
//			<-	timingFlags		Whether the display mode with these scan timings is required or optional.
//								If a 'displayModeID' is not thought to be valid for given display, set
//								timingFlags to 0 (invalid and unsafe)...the DisplayMgr will ask
//								display modules if the mode is valid.
//
//=====================================================================================================
GDXErr GraphicsHALGetModeTiming(DisplayModeID displayModeID, UInt32 *timingData, UInt32 *timingFormat,
			UInt32 *timingFlags)
{


	// Define a new type which is a table of supported 'DisplayModeIDs' and their corresponding
	// timingdata and their timing flags

	typedef struct DisplayModeTimingTable DisplayModeTimingTable;
	struct DisplayModeTimingTable
	{
		DisplayCode displayCode;
		DisplayModeID displayModeID;
		UInt32 timingFlags;
	};

	enum
	{
		notValid = 0,
		valid = (1 << kModeValid),
		validAndSafe = (1 << kModeValid | 1 << kModeSafe),
		validAndSafeAndDefault = ( (1 << kModeValid) | (1 << kModeSafe) | (1 << kModeDefault) )
	};

	DisplayModeTimingTable theDisplayModeTimingTable[] =
	{
		{kDisplayCode12Inch, kDisplay_512x384_60Hz, validAndSafe},
		{kDisplayCodeStandard, kDisplay_640x480_67Hz, validAndSafe},
		{kDisplayCodePortrait, kDisplay_640x870_75Hz, validAndSafe},
		{kDisplayCodePortraitMono, kDisplay_640x870_75Hz, validAndSafe},
		{kDisplayCode16Inch, kDisplay_832x624_75Hz, validAndSafe},
		{kDisplayCode19Inch, kDisplay_1024x768_75Hz, validAndSafe},
		{kDisplayCode21Inch, kDisplay_1152x870_75Hz, validAndSafe},
		{kDisplayCode21InchMono, kDisplay_1152x870_75Hz, validAndSafe},

		{kDisplayCodeVGA, kDisplay_640x480_60Hz_VGA, validAndSafeAndDefault},
		{kDisplayCodeVGA, kDisplay_640x480_120Hz, valid},
		{kDisplayCodeVGA, kDisplay_800x600_60Hz_VGA, valid},
		{kDisplayCodeVGA, kDisplay_800x600_72Hz_VGA, valid},
		{kDisplayCodeVGA, kDisplay_800x600_75Hz_VGA, valid},
		{kDisplayCodeVGA, kDisplay_1024x768_60Hz_VGA, valid},
		{kDisplayCodeVGA, kDisplay_1024x768_72Hz_VGA, valid},
		{kDisplayCodeVGA, kDisplay_1024x768_75Hz_VGA, valid},
		{kDisplayCodeVGA, kDisplay_1280x960_75Hz, valid},
		{kDisplayCodeVGA, kDisplay_1280x1024_75Hz, valid},

		{kDisplayCodeNTSC, kDisplay_512x384i_60Hz_NTSC, validAndSafe},
		{kDisplayCodeNTSC, kDisplay_640x480i_60Hz_NTSC, validAndSafeAndDefault},

		{kDisplayCodePAL, kDisplay_640x480i_50Hz_PAL, validAndSafe},
		{kDisplayCodePAL, kDisplay_768x576i_50Hz_PAL, validAndSafeAndDefault},

		{kDisplayCodeMultiScanBand1, kDisplay_640x480_67Hz, validAndSafeAndDefault},
		{kDisplayCodeMultiScanBand1, kDisplay_800x600_60Hz_VGA, valid},
		{kDisplayCodeMultiScanBand1, kDisplay_800x600_72Hz_VGA, valid},
		{kDisplayCodeMultiScanBand1, kDisplay_832x624_75Hz, validAndSafe},

		{kDisplayCodeMultiScanBand2, kDisplay_640x480_67Hz, validAndSafe},
		{kDisplayCodeMultiScanBand2, kDisplay_832x624_75Hz, validAndSafeAndDefault},
		{kDisplayCodeMultiScanBand2, kDisplay_1024x768_60Hz_VGA, valid},
		{kDisplayCodeMultiScanBand2, kDisplay_1024x768_72Hz_VGA, valid},
		{kDisplayCodeMultiScanBand2, kDisplay_1024x768_75Hz, validAndSafe},

		{kDisplayCodeMultiScanBand3, kDisplay_640x480_67Hz, validAndSafe},
		{kDisplayCodeMultiScanBand3, kDisplay_640x480_120Hz, valid},
		{kDisplayCodeMultiScanBand3, kDisplay_832x624_75Hz, validAndSafe},
		{kDisplayCodeMultiScanBand3, kDisplay_1024x768_60Hz_VGA, valid},
		{kDisplayCodeMultiScanBand3, kDisplay_1024x768_72Hz_VGA, valid},
		{kDisplayCodeMultiScanBand3, kDisplay_1024x768_75Hz, validAndSafe},
		{kDisplayCodeMultiScanBand3, kDisplay_1152x870_75Hz, validAndSafeAndDefault},
		{kDisplayCodeMultiScanBand3, kDisplay_1280x960_75Hz, valid},
		{kDisplayCodeMultiScanBand3, kDisplay_1280x1024_75Hz, valid},

		{kDisplayCodeDDCC, kDisplay_640x480_120Hz, notValid},
		{kDisplayCodeDDCC, kDisplay_640x480_60Hz_VGA, notValid},
	};

	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	DisplayCode displayCode = templateHALData->displayCode;	// class of connected display

	UInt32 i;			// omnipresent loop iterator

	GDXErr err = kGDXErrNoError;			// Never fails.  timingFlags = 0 if we don't think
											// monitor supports the displayModeID
	DepthMode maxDepthMode;
	*timingFlags = 0;				// Default to invalid and unsafe.  If HAL doesn't know if a display
									// supports a displayModeID, the OS will ask a display module

	if (displayModeID < kFirstProgrammableDisplayMode)
		*timingFormat = kDeclROMtables;	// Default to kDeclROMtables -- the only valid timingFormat
	else
		*timingFormat = kDetailedTimingFormat;

	*timingData = timingInvalid;

	// Prior to doing anything else, do some paranoid error checking, and make sure that Toynbee
	// can drive the indicated 'displayModeID.'

	err = GraphicsHALGetMaxDepthMode(displayModeID, &maxDepthMode);
	if (err)
	{
		// Opps...caller specified an invalid 'DisplayModeID' or 'DepthMode'
		goto ErrorExit;
	}

	if (templateHALData->isDDCC)
		displayCode = kDisplayCodeDDCC;


	// Scan the theDisplayModeTimingTable to see if the connected monitor supports the displayModeID

	for (i = 0; i < sizeof(theDisplayModeTimingTable) / sizeof(theDisplayModeTimingTable[0]); i++)
	{
		if ((theDisplayModeTimingTable[i].displayCode == displayCode) &&
			(theDisplayModeTimingTable[i].displayModeID == displayModeID))
		{
			*timingFlags = theDisplayModeTimingTable[i].timingFlags;
			break;
		}
	}

	{
		DisplayInfo info;
		err = TemplateGetDisplayData( true, displayModeID, kDepthMode1,
										 nil, nil, &info );
		if (!err)
		{
			*timingData = info.timingData;
		}
	}

	if (displayCode == kDisplayCode16)
		*timingFlags = valid;

ErrorExit:

	return err;
}



//=====================================================================================================
//
// GraphicsHALGetResolutionAndFrequency()
//
//=====================================================================================================
static GDXErr GraphicsHALGetResolutionAndFrequency( DisplayModeID displayModeID, UInt32 *horizontalPixels,
												UInt32 *verticalLines, Fixed *refreshRate )
{
	DisplayInfo info;
	BitDepthIndependentData bdiData;
	GDXErr err = TemplateGetDisplayData( true, displayModeID, kDepthMode1,
									 &bdiData, nil, &info );
	if (!err)
	{
		*verticalLines = info.height;
		*horizontalPixels = info.width;
		*refreshRate = (UInt64)bdiData.pixelClock * 10 * 65536 / (bdiData.hTotal * bdiData.vTotal);
	}

	return err;
}



//=====================================================================================================
//
// GraphicsHALGetNextResolution()
//
//	This call will take a 'previousDisplayModeID' and return the next supported display mode.
//	The Core takes care of most of the work.  The HAL just needs to look at the
//	'previousDisplayModeID' and return the next supported 'displayModeID' and the max depthMode
//	that the HW supports.
//	The Core has already checked to see if previousDisplayModeID = -1, in which case it has
//	returned the current resolution's information.
//	The HAL returns all DisplayModeIDs that it supports.  Pay No attention to the monitor behind
//	the sense lines.
//
//			->	previousDisplayModeID
//			'previousDiplayModeID = kDisplayModeIDCurrent' never happens.  Core will handle that case.
//			If 'previousDiplayModeID = kDisplayModeIDFindFirstResolution', get the first supported
//			resolution for the monitor.
//			Otherwise, 'previousDiplayModeID' contains the previous displayModeID (hence its name)
//			from the previous call.
//
//			<-	displayModeID	ID of the next display mode following 'csPreviousDiplayModeID'
//			Set to 'kDisplayModeIDNoMoreResolutions' once all supported display modes have been reported.
//
//			<-	maxDepthMode	Maximum relative bit depth for the 'displayModeID'
//
//=====================================================================================================
GDXErr GraphicsHALGetNextResolution(DisplayModeID previousDisplayModeID,
		DisplayModeID *displayModeID, DepthMode *maxDepthMode)
{
	GDXErr err = kGDXErrUnknownError;								// Assume failure

	UInt32 i;							// omnipresent loop iterator
	UInt32 displayModeIDIndex;			// index for the returned DisplayModeID
	UInt32 lastIndex;

	// if previousDisplayModeID = kDisplayModeIDFindFirstResolution, return the first resolution
	// that HW supports

	if ( previousDisplayModeID == kDisplayModeIDFindFirstResolution )
	{
		i = -1;
		lastIndex = kFirstProgrammableModeInfo;
	}
	else if ( previousDisplayModeID == kDisplayModeIDFindFirstProgrammable )
	{
		i = kFirstProgrammableModeInfo - 1;
		lastIndex = kNumModeInfos;
	}
	else
	{
		if ( previousDisplayModeID < kFirstProgrammableDisplayMode )
			lastIndex = kFirstProgrammableModeInfo;
		else
			lastIndex = kNumModeInfos;
		for ( i = 0; (i < lastIndex) && (gDisplayModeInfo[i].info.dinfo_displayModeAlias != previousDisplayModeID); i++ )
			;
	}

	// Check if i has exceeded lastIndex.  This means the
	// previousDisplayModeID was not valid.

	if ( lastIndex == i )
	{
		err = kGDXErrInvalidParameters;
		goto ErrorExit;
	}

	displayModeIDIndex = ++i;		// index to next supported resolution. (or no more)

	// displayModeIndex now points to the nextresolution.
	// Make sure 'displayModeIndex != lastIndex'... if it does, all resolutions have been reported.
	if (displayModeIDIndex < lastIndex)
	{
		*displayModeID = gDisplayModeInfo[displayModeIDIndex].info.dinfo_displayModeAlias;
		err = GraphicsHALGetMaxDepthMode(*displayModeID, maxDepthMode);
	}
	else
	{
		*displayModeID = kDisplayModeIDNoMoreResolutions;		// no more supported resolutions
		err = kGDXErrNoError;
	}

ErrorExit:
	return err;
}



//=====================================================================================================
//
// GraphicsHALGetVideoParams()
//
//	The HAL only needs to return the absolute bits per pixel and the rowBytes for a
//	given depthMode and displayModeID.  The displayModeID is passed in since rowBytes might depend
//	on the resolution.
//
//		-> displayModeID	the displayModeID for which the info is desired
//		-> depthMode		The relative bit depth for which the info is desired
//		<- bitsPerPixel		absolute bit depth for the given depthMode
//		<- rowBytes			width of each row of video memory for the given depthMode
//		<- horizontalPixels	the horizontal pixels for the dispalyModeID
//		<- verticalLines	the vertical lines for the dispalyModeID
//		<- refreshRate		the refresh rate for the dispalyModeID
//
//=====================================================================================================
GDXErr GraphicsHALGetVideoParams(DisplayModeID displayModeID, DepthMode depthMode,
		UInt32 *bitsPerPixel, SInt16 *rowBytes,  UInt32 *horizontalPixels, UInt32 *verticalLines, Fixed *refreshRate)
{
	GDXErr err;

	err = GraphicsHALGetResolutionAndFrequency( displayModeID, horizontalPixels, verticalLines, refreshRate );
	if (err)
		goto ErrorExit;

	err = GraphicsHALMapDepthModeToBPP(depthMode, bitsPerPixel);
	if (err)
		goto ErrorExit;

	*rowBytes = CalcRowBytes(*horizontalPixels, depthMode);

ErrorExit:

	return err;
}



//=====================================================================================================
//
// GraphicsHALGetMaxDepthMode()
//
// This takes a 'displayModeID' and returns the maximum depthMode that is supported by the
// hardware for that 'displayModeID'.  NO check is made to determine if the 'displayModeID' is
// valid for the connected monitor.  The HAL should return an error if the 'displayModeID' is
// not supported or there is not enough VRAM to support the 'displayModeID'
//
// This function DETERMINES if the HW supports a given displayModeID
//
//			->	displayModeID		get the information for this display mode
//			<-	maxDepthMode		Maximum bit depth for the DisplayModeID (relative depth mode)
//
//=====================================================================================================
GDXErr GraphicsHALGetMaxDepthMode(DisplayModeID displayModeID, DepthMode *maxDepthMode)
{
	DisplayInfo info;

	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	GDXErr err = TemplateGetDisplayData(true, displayModeID, kDepthMode1,
									 nil, nil, &info);

	if (err)										// unknown displayModeID OR vramSize
		goto ErrorExit;

	*maxDepthMode = info.maxDepthMode;

	if (templateHALData->monoOnly && *maxDepthMode > kDepthMode4)
		*maxDepthMode = kDepthMode4;	// if on a mono only display, never do more than 256 colors

	err = kGDXErrNoError;

ErrorExit:

	return err;
}



//=====================================================================================================
//
//  GraphicsHALMapDepthModeToBPP()
//	This routine maps a relative pixel depth (DepthMode) to an absolute pixel depth (bits per pixel)
//
//		-> depthMode		The relative pixel depth
//
//		<- bitsPerPixel		Corresponding abosolute pixel depth.
//
//=====================================================================================================
GDXErr GraphicsHALMapDepthModeToBPP(DepthMode depthMode, UInt32 *bitsPerPixel)
{
	GDXErr err = kGDXErrUnknownError;			// Assume failure.

	switch (depthMode)
	{
		case kDepthMode1: *bitsPerPixel =  1; break;
		case kDepthMode2: *bitsPerPixel =  2; break;
		case kDepthMode3: *bitsPerPixel =  4; break;
		case kDepthMode4: *bitsPerPixel =  8; break;
		case kDepthMode5: *bitsPerPixel = 16; break;
		case kDepthMode6: *bitsPerPixel = 32; break;
		default:								// someone passed in bogus depthMode...say it is 8
			*bitsPerPixel = 8;
			err = kGDXErrUnableToMapDepthModeToBPP;
			goto ErrorExit;
	}

	err = kGDXErrNoError;

ErrorExit:

	return err;
}



//=====================================================================================================
//
// GraphicsHALModePossible()
//
//	This routine checks to see if the frame buffer is cabable of driving the given 'displayModeID' at
//	the indicated 'depthMode' and 'page'. This DOES NOT check to see that the 'displayModeID' is valid
//	for the display type that is physically connected to the frame buffer.
//
//	IMPORTANT NOTE: The GDXErr return value DOES NOT indicate whether the mode is possible or not.
//	It only signifies whether or not the value returned in 'modePossible' was correctly determined.  In
//	the event of an error, 'modePossible' does not contain valid information.
//
//		-> displaymodeID		The desired DisplayModeID.
//		-> depthMode			The desired relative bit depth.
//		-> page					The desired page.
//
//		<- modePossible
//		This will be 'true' if the frame buffer can support the desired items, 'false' otherwise. In
//		the event of an error, 'modePossible' is undefined.
//
//=====================================================================================================
GDXErr GraphicsHALModePossible(DisplayModeID displayModeID, DepthMode depthMode, SInt16 page,
		Boolean *modePossible)
{
	GDXErr err;
	SInt16 pageCount;
	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	*modePossible = false;						// Assume mode not possible

	err = GraphicsHALGetPages(displayModeID, depthMode, &pageCount);

	// Check to see if the requested page is valid.

	if (err || page < 0 || page >= pageCount)
		goto ErrorExit;

	*modePossible = true;

	err = kGDXErrNoError;

ErrorExit:

	return err;
}



//=====================================================================================================
//
// GraphicsHALDetermineDisplayCode()
//
//	This routine is called whenever it is necessary to determine the type of display that is
//	connected to the frame buffer controller.
//	When this routine is called, the following actions should occur:
//
//		* Perform required steps to determine what display is connected (e.g., read sense lines)
//		* Update the HAL's state information regarding the type of display connected (if HAL
//		  implementation maintains that state information)
//
//	In the event that the HAL is does not the specific type of display attached, it should set
//	'*displayCode = kDisplayCodeUnknown'
//
//			<- displayCode	DisplayCode for the attached display.
//
//=====================================================================================================
GDXErr GraphicsHALDetermineDisplayCode(DisplayCode *displayCode, Boolean *hasDDCConnection, Boolean *builtInConnection)
{
	RawSenseCode rawSenseCode;						// Result from reading sense lines after reset
	ExtendedSenseCode extendedSenseCode;			// Result from applying extended sense algorithm

	GDXErr err = kGDXErrNoError;					// Assume sucess
	Boolean bool1 = false;
	Boolean bool2 = false;

	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	templateHALData->isDDCC = false;
	*builtInConnection = false;


	// The rawSenseCode is obtained by making sure the sense lines are reset (i.e., none of the lines
	// are being actively driven by the frame buffer controller) and then reading the state of the
	// lines.


	TemplateResetSenseLines();
	rawSenseCode = TemplateReadSenseLines();

	extendedSenseCode = TemplateGetExtendedSenseCode();

	GraphicsHALDetermineDisplayCo_2(rawSenseCode, extendedSenseCode, &bool1, &bool2);

	if (templateHALData->hardwareIsProgrammed)
	{
		if (bool1)
			GraphicsUtilSetSync_2();
		else if (bool2)
			GraphicsUtilDDCproc94();
	}

	if (bool2)
	{
		struct vbe_edid1_info ddcBlockData;

		if (!templateHALData->hardwareIsProgrammed)
		{
			Boolean directColor;
			char* baseAddress;

			ControlWriteRegister(kRegFieldControlBLANK_DISABLE, 1); // disable CBlank
			GraphicsHALProgramHardware(kDisplay_640x480_67Hz, kDepthMode1, 0, &directColor, &baseAddress);
			templateHALData->displayModeID = kDisplayModeIDInvalid;
			templateHALData->hardwareIsProgrammed = true;
			templateHALData->needsEnableCBlank = true;
			if (bool1)
				GraphicsUtilSetSync_2();
			else if (bool2)
				GraphicsUtilDDCproc94();
		}

		if (GraphicsUtilGetDDCBlock_2a(true, (UInt8*)&ddcBlockData) == kGDXErrNoError)
		{
			templateHALData->isDDCC = true;
			GraphicsHALGetSupportedTimings(&ddcBlockData);
		}
	}

	*hasDDCConnection = templateHALData->isDDCC;

	if (!templateHALData->isDDCC && !bool1 && bool2)
		*builtInConnection = true;


	// The rawSenseCode is obtained by making sure the sense lines are reset (i.e., none of the lines
	// are being actively driven by the frame buffer controller) and then reading the state of the
	// lines.


	TemplateResetSenseLines();
	rawSenseCode = TemplateReadSenseLines();

	extendedSenseCode = TemplateGetExtendedSenseCode();

	templateHALData->rawSenseCode = rawSenseCode;
	templateHALData->extendedSenseCode = extendedSenseCode;

	// Since the Template architecture uses 'standard' sense codes, call the utility function
	// that maps the raw and extendended sense codes to a 'displayCode.'

	err = GraphicsUtilMapSenseCodesToDisplayCode(rawSenseCode, extendedSenseCode, false, displayCode);
	if (err)
		goto ErrorExit;

	if (*displayCode == kDisplayCodeNoDisplay)
	{
		ResType	ResTypeXPRAMCode;

		ReadXPRam(&ResTypeXPRAMCode, 4, 0x00FC);
		err = GraphicsUtilMapXPRAMToDispCode(ResTypeXPRAMCode, displayCode);
	}

	templateHALData->displayCode = *displayCode;		// Update HAL information

	if ( (kDisplayCode21InchMono == *displayCode) || (kDisplayCodePortraitMono == *displayCode) )
		templateHALData->monoOnly = true;
	else
		templateHALData->monoOnly = false;

	if (*displayCode == kDisplayCodeNoDisplay)
	{
		// write some registers
	}
	else {
		// write some registers
	}

ErrorExit:

	return err;
}



//=====================================================================================================
//
// GraphicsHALGetSenseCodes()
//
//	This routine is called whenever the state of the sense codes need to be reported.
//	NOTE:  This should only report the the sense code information.  No attempt should be made to
//	determine what type of display is attached here.  Moreover, the sense codes should be
//	determined EVERY time this call is made, and not make use of any previously saved values.
//
//		<- rawSenseCode
//		For 'standard' sense code hardware, this value is found by instructing the frame buffer
//		controller NOT to actively drive any of the monitor sense lines, and then reading the
//		state of the the monitor sense lines 2, 1, and 0.  (2 is the MSB, 0 the LSB)
//
//		<- extendedSenseCode
//		For 'standard' sense code hardware, the extended sense code algorithm is as follows:
//		(Note:  as described here, sense line 'A' corresponds to '2', 'B' to '1', and 'C' to '0')
//			* Drive sense line 'A' low and read the values of 'B' and 'C'.
//			* Drive sense line 'B' low and read the values of 'A' and 'C'.
//			* Drive sense line 'C' low and read the values of 'A' and 'B'.
//
//		In this way, a six-bit number of the form BC/AC/AB is generated.
//
//		<- standardInterpretation
//		If 'standard' sense code hardware is implemented (or the values are coerced to appear
//		'standard' then set this to TRUE.  Otherwise, set it to FALSE, and the interpretation for
//		'rawSenseCode' and 'extendedSenseCode' will be considered private.
//
//=====================================================================================================
GDXErr GraphicsHALGetSenseCodes(RawSenseCode *rawSenseCode, ExtendedSenseCode *extendedSenseCode,
		Boolean *standardInterpretation)
{
	GDXErr err = kGDXErrNoError;							// Assume sucess
	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	// The rawSenseCode is obtained by making sure the sense lines are reset (i.e., none of the lines
	// are being actively driven by the frame buffer controller) and then reading the state of the
	// lines.

	TemplateResetSenseLines();
	*rawSenseCode = TemplateReadSenseLines();

	*extendedSenseCode = TemplateGetExtendedSenseCode();

	*standardInterpretation = true;						// Template has 'standard' sense lines

ErrorExit:
	return err;
}



//=====================================================================================================
//
// GraphicsHALCallbackReadSenseLine2()
//
//=====================================================================================================
static Boolean GraphicsHALCallbackReadSenseLine2(void)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	long theValue = EndianSwap32Bit(*templateHALData->senseLineEnable);
	if (theValue & 0x100)
		return true;
	else
		return false;
}



//=====================================================================================================
//
// GraphicsHALCallbackReadSenseLine1()
//
//=====================================================================================================
static Boolean GraphicsHALCallbackReadSenseLine1(void)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	long theValue = EndianSwap32Bit(*templateHALData->senseLineEnable);
	if (theValue & 0x80)
		return true;
	else
		return false;
}



//=====================================================================================================
//
// GraphicsHALCallbackSenseLine2Set()
//
//=====================================================================================================
static void GraphicsHALCallbackSenseLine2Set(void)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	long oldValue = EndianSwap32Bit(*templateHALData->senseLineEnable);
	long newValue = (oldValue & ~0x20) | 4;
	*templateHALData->senseLineEnable = EndianSwap32Bit(newValue);
}



//=====================================================================================================
//
// GraphicsHALCallbackSenseLine2Clear()
//
//=====================================================================================================
static void GraphicsHALCallbackSenseLine2Clear(void)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	long oldValue = EndianSwap32Bit(*templateHALData->senseLineEnable);
	long newValue = oldValue & ~24;
	*templateHALData->senseLineEnable = EndianSwap32Bit(newValue);
}



//=====================================================================================================
//
// GraphicsHALCallbackSenseLine1Set()
//
//=====================================================================================================
static void GraphicsHALCallbackSenseLine1Set(void)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	long oldValue = EndianSwap32Bit(*templateHALData->senseLineEnable);
	long newValue = (oldValue & ~0x10) | 2;
	*templateHALData->senseLineEnable = EndianSwap32Bit(newValue);
}



//=====================================================================================================
//
// GraphicsHALCallbackSenseLine1Clear()
//
//=====================================================================================================
static void GraphicsHALCallbackSenseLine1Clear(void)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	long oldValue = EndianSwap32Bit(*templateHALData->senseLineEnable);
	long newValue = oldValue & ~0x12;
	*templateHALData->senseLineEnable = EndianSwap32Bit(newValue);
}



//=====================================================================================================
//
// GraphicsHALCallbackResetSenseLine2()
//
//=====================================================================================================
static void GraphicsHALCallbackResetSenseLine2(void)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	long oldValue = EndianSwap32Bit(*templateHALData->senseLineEnable);
	long newValue = (oldValue & ~4) | 0x20;
	*templateHALData->senseLineEnable = EndianSwap32Bit(newValue);
}



//=====================================================================================================
//
// GraphicsHALCallbackResetSenseLine1()
//
//=====================================================================================================
static void GraphicsHALCallbackResetSenseLine1(void)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	long oldValue = EndianSwap32Bit(*templateHALData->senseLineEnable);
	long newValue = (oldValue & ~2) | 0x10;
	*templateHALData->senseLineEnable = EndianSwap32Bit(newValue);
}



//=====================================================================================================
//
// GraphicsHALCallbackResetSenseLine2and1()
//
//=====================================================================================================
static void GraphicsHALCallbackResetSenseLine2and1(void)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	long oldValue = EndianSwap32Bit(*templateHALData->senseLineEnable);
	long newValue = (oldValue & ~6) | 0x30;
	*templateHALData->senseLineEnable = EndianSwap32Bit(newValue);
}



//=====================================================================================================
//
// GraphicsHALCallbackSetDDCInfo()
//
//=====================================================================================================
static void GraphicsHALCallbackSetDDCInfo(UInt8* ddcBlockData, UInt32 ddcBlockNumber)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	templateHALData->isDDCC = true;
	if (ddcBlockNumber == 1)
		GraphicsHALGetSupportedTimings((struct vbe_edid1_info*)ddcBlockData);
}



//=====================================================================================================
//
// GraphicsHALGetDefaultDisplayModeID()
//
//	The 'displayCode' is passed in and the HAL returns the default 'displayModeID' and the
//	'depthMode'.  This routine gets called when a new monitor is connected to the computer.  The
//	HAL knows how much VRAM is available and whether it can handle a given 'displayModeID' for a
//	monitor.
//	For example, the 'kIndexedMultiScanBand3' has a default 'displayModeID' of kDisplay_1152x870_75Hz.
//	If there is not enough VRAM available, the HAL is unable to switch into that resolution, hence
//	the HAL will switch into the next best resolution.
//	The HAL will also return the max 'depthMode' that can be supported for the resolution.  This
//	enables a switch from a 13" monitor at millions of colors to be switched with a 21" monitor
//	and, if there is enough VRAM, the depthMode will still be at millions of colors
//
//		-> displayCode			the connected display type
//		<- displaymodeID		The default DisplayModeID for the connected monitor type
//		<- depthMode			The default depthMode
//
//=====================================================================================================
GDXErr GraphicsHALGetDefaultDisplayModeID(DisplayCode displayCode, DisplayModeID *displayModeID,
		DepthMode *depthMode)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	// Define a new type which is a table of default 'DisplayModeIDs' for the
	// connected monitor.

	typedef struct DefaultResolutionTable DefaultResolutionTable;
	struct DefaultResolutionTable
	{
		DisplayCode displayCode;
		DisplayModeID displayModeID;
	};

	DefaultResolutionTable defaultResolutionTable[] =
	{
		{kDisplayCodeUnknown, kDisplay_640x480_67Hz},
		{kDisplayCode12Inch, kDisplay_512x384_60Hz},
		{kDisplayCodeStandard, kDisplay_640x480_67Hz},
		{kDisplayCodePortrait, kDisplay_640x870_75Hz},
		{kDisplayCodePortraitMono, kDisplay_640x870_75Hz},
		{kDisplayCode16Inch, kDisplay_832x624_75Hz},
		{kDisplayCode19Inch, kDisplay_1024x768_75Hz},
		{kDisplayCode21Inch, kDisplay_1152x870_75Hz},
		{kDisplayCode21InchMono, kDisplay_1152x870_75Hz},
		{kDisplayCodeVGA, kDisplay_640x480_60Hz_VGA},
		{kDisplayCodeNTSC, kDisplay_640x480i_60Hz_NTSC},
		{kDisplayCodePAL, kDisplay_768x576i_50Hz_PAL},
		{kDisplayCodeMultiScanBand1, kDisplay_640x480_67Hz},
		{kDisplayCodeMultiScanBand2, kDisplay_832x624_75Hz},
		{kDisplayCodeMultiScanBand3, kDisplay_1152x870_75Hz},
		{kDisplayCodeDDCC, kDisplay_640x480_60Hz_VGA},
		{kDisplayCode16, kDisplay_640x480_67Hz},
	};
	enum { maxDefaultTableEntries = sizeof(defaultResolutionTable) / sizeof(defaultResolutionTable[0]) };

	UInt32 i;								// loop iterator
	GDXErr err = kGDXErrMonitorUnsupported;			// Assume failure


	// Scan the defaultResolutionTable to find the the connected monitor

	for (i = 0; i < maxDefaultTableEntries; i++)
	{
		if (defaultResolutionTable[i].displayCode == displayCode)
		{
			*displayModeID = defaultResolutionTable[i].displayModeID;
			err = kGDXErrNoError;
			break;					// found displayCode
		}
	}

	if (err)											// didn't find displayCode
		goto ErrorExit;

	if (templateHALData->isDDCC) // DDC data will override displayCode
	{
		if (templateHALData->supports_640x480_67Hz)
			*displayModeID = kDisplay_640x480_67Hz;
		else if (templateHALData->supports_832x624_75Hz)
			*displayModeID = kDisplay_832x624_75Hz;
		else if (templateHALData->supports_800x600_75Hz)
			*displayModeID = kDisplay_800x600_75Hz_VGA;
		else if (templateHALData->supports_800x600_60Hz)
			*displayModeID = kDisplay_800x600_60Hz_VGA;
		else if (templateHALData->supports_800x600_72Hz)
			*displayModeID = kDisplay_800x600_72Hz_VGA;
		else if (templateHALData->supports_1024x768_75Hz)
			*displayModeID = kDisplay_1024x768_75Hz_VGA;
		else if (templateHALData->supports_1024x768_70Hz)
			*displayModeID = kDisplay_1024x768_70Hz;
		else if (templateHALData->supports_1024x768_60Hz)
			*displayModeID = kDisplay_1024x768_60Hz_VGA;
		else if (templateHALData->supports_1152x870_75Hz)
			*displayModeID = kDisplay_1152x870_75Hz;
	}

	err = GraphicsHALGetMaxDepthMode(*displayModeID, depthMode);


ErrorExit:

	return err;
}




//=====================================================================================================
//
// GraphicsHALDetermineDisplayCo_2()
//
//=====================================================================================================
static void GraphicsHALDetermineDisplayCo_2( RawSenseCode rawSenseCode, ExtendedSenseCode extendedSenseCode,
											Boolean *bool1, Boolean *bool2)
{
	GDXErr err;
	GraphicsPreferred graphicsPreferred;
	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	*bool1 = false;
	*bool2 = false;

	err = GraphicsOSSGetCorePref(&templateHALData->regEntryID, &graphicsPreferred);

	if (err)
	{
		if (
				(
					( rawSenseCode == kRSCTwo || rawSenseCode == kRSCSix ) &&
					(
						extendedSenseCode == kESCTwo12Inch   || // 0x21 = 10  00  01
						extendedSenseCode == 0x22            || //      = 10  00  10 // not possible
						extendedSenseCode == kESCSixStandard || // 0x2B = 10  10  11
						extendedSenseCode == 0x29			 ||	//      = 10  10  01 // not possible
						0
					)
				)
				||
				(
					( rawSenseCode == kRSCThree || rawSenseCode == kRSCSeven ) &&
					(
						extendedSenseCode == kESCThree21InchMonoRadius	|| // 0x34 = 11  01  00 // also possible with rawSenseCode = seven
						extendedSenseCode == 0x36						|| //      = 11  01  10 // not possible
						extendedSenseCode == kESCSevenDDC             	|| // 0x3E = 11  11  10 // only possible with rawSenseCode = seven
						extendedSenseCode == 0x3D						|| //      = 11  11  01 // only possible with rawSenseCode = seven
						0
					)
				)
			)
			*bool1 = true;
			*bool2 = true;
	}
	else
	{
		if ( rawSenseCode == kRSCSix || rawSenseCode == kRSCTwo )
		{
			if (
				extendedSenseCode == kESCTwo12Inch	 ||
				extendedSenseCode == 0x22			 ||
				extendedSenseCode == kESCSixStandard ||
				extendedSenseCode == 0x29			 ||
				0
			)
			{
				*bool2 = true;
				if (
					graphicsPreferred.displayCode == kDisplayCodeStandard ||
					graphicsPreferred.displayCode == kDisplayCode12Inch
				)
					*bool1 = false;
				else
					*bool1 = true;
			}
		}
		else if ( rawSenseCode == kRSCSeven || rawSenseCode == kRSCThree )
		{
			if (
				extendedSenseCode == kESCSevenDDC				||
				extendedSenseCode == kESCThree21InchMonoRadius	||
				extendedSenseCode == 0x36						||
				extendedSenseCode == 0x3D						||
				0
			)
			{
				*bool2 = true;
				if (
					graphicsPreferred.displayCode == kDisplayCodeStandard	||
					graphicsPreferred.displayCode == kDisplayCode21InchMono ||
					graphicsPreferred.displayCode == kDisplayCodeUnknown
				)
					*bool1 = false;
				else
					*bool1 = true;
			}
		}
	}
}



//=====================================================================================================
//
// TemplateCalcPageBaseAddress()
//
//=====================================================================================================
static GDXErr TemplateCalcPageBaseAddress(Ptr* baseAddress)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	templateHALData->baseAddressPage[0] = CalcPageBaseAddress(0);
	templateHALData->baseAddressPage[1] = CalcPageBaseAddress(1);
	*baseAddress = templateHALData->baseAddressPage[templateHALData->currentPage];

	return kGDXErrNoError;
}



//=====================================================================================================
//
// GraphicsHALProgramHardware()
//
//	This routine attempts to program the graphics hardware to the desired 'displayModeID', 'depthMode',
//	and 'page'.  It is not required to specicifically check to see if the inputs are valid, since it
//	can assume that the checking has been done elsewhere.
//
//		-> displaymodeID		The desired DisplayModeID.
//		-> depthMode			The desired relative bit depth.
//		-> page					The desired page.
//
//		<- directColor
//		This is 'true' if the desired depthMode results in the hardware being in a direct color mode,
//		otherwise it is 'false'. In the event on an error, it is undefined.
//
//		<- baseAddress
//		The resulting base address of the frame buffer's ram. In the event of an error, it is undefined.
//
//=====================================================================================================
GDXErr GraphicsHALProgramHardware(DisplayModeID displayModeID, DepthMode depthMode, SInt16 page,
									Boolean *directColor, char **baseAddress)
{
	Boolean resolutionUnchanged = false;			// don't hit timing registers if same resolution
	Boolean vblInterruptsEnabled = false;		// default to not enabled;
	BitDepthIndependentData bdiData;			// dependent on resolution
	WidthAndDepthDependentData vwdData;			// dependent on VRAM width and depthMode
	DisplayInfo info;							// describes cabilities of display

	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	GDXErr err = kGDXErrUnknownError;								// Assume failure

	// Flush out any deferred cursor moving without actually redrawing the cursor.
	if (false != templateHALData->hardwareCursorData.deferredMove)
	{
		templateHALData->hardwareCursorData.x =
			templateHALData->hardwareCursorData.deferredX;
		templateHALData->hardwareCursorData.y =
			templateHALData->hardwareCursorData.deferredY;

		templateHALData->hardwareCursorData.deferredMove = false;
	}

	if (displayModeID == templateHALData->displayModeID)
		resolutionUnchanged = true;

	// Get 'bdiData', 'bddData', 'vwData', 'vwdData' and 'info' for requested 'DisplayModeID' and 'DepthMode'
	err = TemplateGetDisplayData(true, displayModeID, depthMode, &bdiData, &vwdData, &info);
	if (err)
	{
		err = kGDXErrDisplayModeIDUnsupported;
		goto ErrorExit;
	}

	// Check if data is valid: check that the maxDepthMode is greater than or equal to the requested DepthMode
	if (vwdData.pixelDepth == 0)
	{
		err = kGDXErrDepthModeUnsupported;
		goto ErrorExit;
	}

	// Save any HAL data that might have changed
	templateHALData->depthMode = depthMode;
	templateHALData->width = info.width;
	templateHALData->height = info.height;
	templateHALData->displayModeID = displayModeID;
	templateHALData->currentPage = page;

	switch (depthMode) {
		case kDepthMode1: templateHALData->rowBytes = (templateHALData->width + 63) / 64 * 8; break;
		case kDepthMode2: templateHALData->rowBytes = (templateHALData->width + 31) / 32 * 8; break;
		case kDepthMode3: templateHALData->rowBytes = (templateHALData->width + 15) / 16 * 8; break;
		case kDepthMode4: templateHALData->rowBytes = (templateHALData->width +  7) /  8 * 8; break;
		case kDepthMode5: templateHALData->rowBytes = (templateHALData->width +  3) /  4 * 8; break;
		case kDepthMode6: templateHALData->rowBytes = (templateHALData->width +  1) /  2 * 8; break;
	}

	// Start programing the hardware to the desired state.  For all these function calls, we are
	// explicitly ignoring any errors, since we have passed 'the point of no return'

	vblInterruptsEnabled = GraphicsOSSSetVBLInterrupt(false);

	// turn hardware cursor off
	ControlWriteRegister(kRegFieldControlHWCURSOR_ENABLE, 0);

	templateHALData->hardwareCursorData.cursorRendered = false;
	templateHALData->hardwareCursorData.cursorVisible = false;

	(void) TemplateAssertVideoReset();
	(void) TemplateSetupClockGenerator(&bdiData);
	(void) TemplateSetPage(templateHALData->dingusVideo);
	(void) TemplateSetupFBController(depthMode, &info, &bdiData, &vwdData);
	(void) TemplateReleaseVideoReset();

	if (templateHALData->needsEnableCBlank)
	{
		ControlWriteRegister(kRegFieldControlBLANK_DISABLE, 0); // enable CBlank
		templateHALData->needsEnableCBlank = false;
	}

	if (vblInterruptsEnabled)
		GraphicsOSSSetVBLInterrupt(true);

	ControlWriteRegister(kRegFieldControlVBL_IRQ_CLR, 0);
	ControlWriteRegister(kRegFieldControlVBL_IRQ_CLR, 1);

	// Base address that is reported to QD
	TemplateCalcPageBaseAddress(&templateHALData->baseAddressPageCurrent);

	*baseAddress = templateHALData->baseAddressPageCurrent;
	switch (depthMode) {
		case kDepthMode1: *directColor = false; break;
		case kDepthMode2: *directColor = false; break;
		case kDepthMode3: *directColor = false; break;
		case kDepthMode4: *directColor = false; break;
		case kDepthMode5: *directColor = true ; break;
		case kDepthMode6: *directColor = true ; break;
	}

	// hardware cursor buffer is no longer cleared
	templateHALData->hardwareCursorData.cursorCleared = false;

	err = kGDXErrNoError;										// Everything Okay

ErrorExit:

	return err;
}


//=====================================================================================================
//
// GraphicsHALDrawHardwareCursor()
//	This routine sets the cursor's X and Y coordinates and its visible state.  If the cursor was set
//	successfully by a previous call to GraphicsHALSetHardwareCursor(), then the HAL must program the
//	hardware with the given X, Y and visible state.  If the previous call to
//	GraphicsHALSetHardwareCursor() failed, then an error should be returned.
//
//		-> x		X coordinate
//		-> y		Y coordinate
//		-> visible	'true' if the cursor must be visible.
//
//=====================================================================================================
GDXErr  GraphicsHALDrawHardwareCursor(SInt32 x, SInt32 y, Boolean visible)
{
	TemplateHALData				*templateHALData = GraphicsHALGetHALData();
	SpurHardwareCursorData		*pHardwareCursorData;
	UInt32						cursorRowBytes;
	UInt32						*pCursorImage;
	SInt32						i, j;

	#ifdef _MSC_VER
		#pragma message("fix me")
	#else
		#warning fix me
	#endif

	// Get data for hardware cursor
	pHardwareCursorData = &(templateHALData->hardwareCursorData);

	// Check if the hardware cursor buffer must be cleared.
	if (false == pHardwareCursorData->cursorCleared)
	{
		// set hardware cursor buffer area to transparent
		pCursorImage = (UInt32 *) (((Ptr) (templateHALData->baseAddressPageCurrent)));
		cursorRowBytes = templateHALData->rowBytes;
		for (i = 0; i < templateHALData->height; i++)
		{
			for (j = 0; j < 4; j++)
				*pCursorImage++ = 0;
			pCursorImage = (UInt32 *) ((UInt32) pCursorImage + cursorRowBytes);
		}
		pHardwareCursorData->cursorCleared = true;
	}

	if (false == pHardwareCursorData->cursorSet)
	{
		pHardwareCursorData->cursorVisible = false;
		return (kGDXErrNoHardwareCursorSet);
	}

	if (false == visible)
	{
		pHardwareCursorData->cursorVisible = false;
		return (kGDXErrNoError);
	}

	pHardwareCursorData->deferredX = x;
	pHardwareCursorData->deferredY = y;
	pHardwareCursorData->deferredMove = true;
	pHardwareCursorData->cursorVisible = true;
	return (kGDXErrNoError);
}



//=====================================================================================================
//
// DoLuminanceMappingForHWCursor()
//  Convert the RGB colors into luminance mapped gray scale.
//  For those of us familiar with color space theory,
//
//		Luminance = .299Red + .587Green + .114Blue
//		("Video Demystified" by Keith Jack, page 28)
//
//  Conveniently, on the PowerPC architechture, floating point math is FASTER
//  than integer math, so we will do outright floating point and not even
//  think about playing games with Fixed Point math.
//
//=====================================================================================================
static void DoLuminanceMappingForHWCursor(SpurColorMap colorMap, SpurColorMap luminanceMap)
{
	UInt32						i;

	double redPortion;					// Luminance portion from red component
	double greenPortion;				// Luminance portion from green component
	double bluePortion;					// Luminance portion from blue component
	double luminance;					// Resulting luminosity


	for (i = 0 ; i < 8 ; i++)
	{
		redPortion = 0.299 * colorMap[i].rgb.red;
		greenPortion = 0.587 * colorMap[i].rgb.green;
		bluePortion = 0.114 * colorMap[i].rgb.blue;

		luminance = redPortion + greenPortion + bluePortion;

		luminanceMap[i].rgb.red = luminance;
		luminanceMap[i].rgb.green = luminance;
		luminanceMap[i].rgb.blue = luminance;
	}
}



//=====================================================================================================
//
// ApplyGammaToHWCursor()
//  Apply gamma correction to the 'colorMap.ctTable'
//
//=====================================================================================================
static void ApplyGammaToHWCursor(const GammaTbl *gamma, SpurColorMap colorMap)
{
	SInt16 						dataWidth;
	UInt32 						redIndex;
	UInt32 						greenIndex;
	UInt32 						blueIndex;

	UInt8 						*redCorrection;			// corrected red data in gamma
	UInt8 						*greenCorrection;		// corrected green data in gamma
	UInt8 						*blueCorrection;		// corrected blue data in gamma

	UInt32						i;

	dataWidth = gamma->gDataWidth;
	redCorrection = (UInt8 *) &gamma->gFormulaData + gamma->gFormulaSize;
	if (1 == gamma->gChanCnt)
	{
		// Use same correction data for all three channels
		greenCorrection = redCorrection;
		blueCorrection = redCorrection;
	} else {
		// Each channel has its own correction data
		greenCorrection = redCorrection + gamma->gDataCnt;
		blueCorrection = redCorrection + ( 2 * gamma->gDataCnt);
	}
	for (i = 0 ; i < 8 ; i++)
	{
		// Extract the most significant 'dataWidth' amount of bits from each color
		// to use as the index into the correction data

		redIndex = colorMap[i].rgb.red >> (16 - dataWidth);
		greenIndex = colorMap[i].rgb.green >> (16 - dataWidth);
		blueIndex = colorMap[i].rgb.blue >> (16 - dataWidth);

		colorMap[i].rgb.red = *(redCorrection + redIndex);
		colorMap[i].rgb.green = *(greenCorrection + greenIndex);
		colorMap[i].rgb.blue = *(blueCorrection + blueIndex);
	}
}




//=====================================================================================================

#pragma options align=mac68k

struct HWCursorColorTable {
	SInt32			ctSeed;
	SInt16			ctFlags;
	SInt16			ctSize;
	ColorSpec		ctTable[kNumHardwareCursorColors];
};
typedef struct HWCursorColorTable HWCursorColorTable;

#pragma options align=reset



//=====================================================================================================
//
// GraphicsHALSetHardwareCursor()
//	This routine is called to setup the hardware cursor and determine if whether the hardware can
//	support it.  The HAL should remember whether this call was successful for subsequent
//	GetHardwareCursorDrawState() or DrawHardwareCursor() calls, but should NOT change the cursor's
//	X or Y coordinates, nor its visible state.
//
//		-> gamma
//		Current gamma table to correct cursor colors with, if the HAL can apply gamma correction.
//
//		-> luminanceMapping
//		This will be true if the Core had luminance mapping enabled AND it was in an indexed color
//		mode.  If 'true', the HAL should luminance map the cursor CLUT EVEN if the hardware cursor is
//		a super-duper cursor capable of direct color. This is because the hardware cursor should look
//		like the software cursor it is replacing.
//
//		-> cursorRef
//		Opaque data to be handed to VSLPrepareCursorForHardwareCursor().
//
//=====================================================================================================
GDXErr GraphicsHALSetHardwareCursor(const GammaTbl *gamma, Boolean luminanceMapping, void *cursorRef)
{
	TemplateHALData				*templateHALData = GraphicsHALGetHALData();
	HardwareCursorInfoRec		hardwareCursorInfo;
	HWCursorColorTable			colorMap;
	SpurHardwareCursorData		*pHardwareCursorData;
	Ptr							hardwareCursorImage;
	SInt32						cursorX, cursorY;
	UInt32						i;

	pHardwareCursorData = &(templateHALData->hardwareCursorData);
	hardwareCursorImage = (Ptr) &(pHardwareCursorData->cursorImage);

	// Get cursor state.
	cursorX = pHardwareCursorData->x;
	cursorY = pHardwareCursorData->y;

	hardwareCursorInfo.majorVersion = 0;
	hardwareCursorInfo.minorVersion = 0;
	hardwareCursorInfo.cursorHeight = 0;
	hardwareCursorInfo.cursorWidth = 0;
	hardwareCursorInfo.colorMap = (CTabPtr) &colorMap;
	hardwareCursorInfo.hardwareCursor = hardwareCursorImage;
	for (i = 0; i < 6; i++)
		hardwareCursorInfo.reserved[i] = 0;

	pHardwareCursorData->cursorSet =
		VSLPrepareCursorForHardwareCursor
		(cursorRef, &(pHardwareCursorData->cursorDescriptor), &hardwareCursorInfo);

	if (false == pHardwareCursorData->cursorSet)
	{
		GraphicsHALDrawHardwareCursor (cursorX, cursorY, false);
		return (kGDXErrCannotRenderCursorImage);
	}

	for (i = 0 ; i < 8 ; i++)
		pHardwareCursorData->colorMap[i] = colorMap.ctTable[i];

	return GraphicsHALTransformHWCursorColors(gamma, luminanceMapping);
}



//=====================================================================================================
//
// GraphicsHALTransformHWCursorColors()
//
//=====================================================================================================
GDXErr GraphicsHALTransformHWCursorColors(const GammaTbl *gamma, Boolean luminanceMapping)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	SpurHardwareCursorData *pHardwareCursorData = &(templateHALData->hardwareCursorData);
	UInt32						i;

	templateHALData->setCursorClutEntriesPending = false;

	if (luminanceMapping)
		DoLuminanceMappingForHWCursor(pHardwareCursorData->colorMap, pHardwareCursorData->transformedColorMap);
	else
		for (i = 0 ; i < 8 ; i++)
			pHardwareCursorData->transformedColorMap[i] = pHardwareCursorData->colorMap[i];

	ApplyGammaToHWCursor(gamma, pHardwareCursorData->transformedColorMap);

	templateHALData->cursorClutTransformed = true;

	return (kGDXErrNoError);
}





//=====================================================================================================
//
// GraphicsHALSupportsHardwareCursor()
//	This call is used to determine if the HAL supports a hardware cursor.
//
//		<- supportsHardwareCursor	'true' if supports a hardware cursor, 'false' otherwise.
//
//=====================================================================================================
GDXErr GraphicsHALSupportsHardwareCursor(Boolean *supportsHardwareCursor)
{
  *supportsHardwareCursor = true;

  return (kGDXErrNoError);
}



//=====================================================================================================
//
// GraphicsHALGetHardwareCursorDrawState()
//	This routine is used to determine the state of the hardware cursor.  After HAL initialization
//	the cursor's visible state and set state should be false. After a mode change the cursor should be
//	made invisible but the set state should remain unchanged.
//
//		<- csCursorX		X coordinate from last DrawHardwareCursor call
//		<- csCursorY		Y coordinate from last DrawHardwareCursor call
//		<- csCursorVisible	'true' if the cursor is visible
//		<- csCursorSet		'true' if last GraphicsHALSetHardwareCursor() call was successful.
//
//=====================================================================================================
GDXErr GraphicsHALGetHardwareCursorDrawState(SInt32  *pCursorX, SInt32  *pCursorY,
											 UInt32  *pCursorVisible, UInt32  *pCursorSet)
{
	TemplateHALData				*templateHALData = GraphicsHALGetHALData();
	SpurHardwareCursorData	*pHardwareCursorData;

	// Get data for hardware cursor
	pHardwareCursorData = &(templateHALData->hardwareCursorData);

	// Check if cursor is set
	if (pHardwareCursorData->cursorSet)
	{
		*pCursorX = pHardwareCursorData->x;
		*pCursorY = pHardwareCursorData->y;
		*pCursorVisible = pHardwareCursorData->cursorVisible;
		*pCursorSet = true;
	}
	else
	{
		*pCursorX = 0;
		*pCursorY = 0;
		*pCursorVisible = false;
		*pCursorSet = false;
	}

	return (kGDXErrNoError);
}



//=====================================================================================================
//
// DeferredMoveHardwareCursor()
//	This is mainly called by the VBL interrupt handler to minimize the latency with moving
// the cursor.  This minimizes the effect of moving the cursor when it intersects the
// scan line.  Since this procedure writes to the Spur address register, this procedure
// must be called by any routine that wants to use the Spur address register so that the
// routine doesn't get interrupted by DeferredMoveHardwareCursor.  The only routine that
// defers drawing is GraphicsHALDrawHardwareCursor.
//
//		-> void
//
//=====================================================================================================
static void  DeferredMoveHardwareCursor(void)
{
	TemplateHALData				*templateHALData = GraphicsHALGetHALData();
	UInt32						screenWidth, screenHeight;
	SpurHardwareCursorData		*pHardwareCursorData;
	Ptr							hardwareCursorImage;
	Ptr							pHardwareCursorFrameBuffer;
	UInt32						screenRowBytes, cursorRowBytes;
	Ptr							pScreenRowBase, pCursorRowBase;
	UInt32						*pCursorImage, *pScreenImage;
	UInt32						screenLine, upperCursorLine, lowerCursorLine, endMask;
	UInt32						upperLineShift, lowerLineShift;
	SInt32						width, height;
	UInt32						cursorStartX, cursorStartY;
	SInt32						currentY;
	SInt32						x, y;
	SInt32						rowLongs;
	SInt32						deferredMove;
	SInt32						i, j;
	DingusVideoRegisters		*dingusVideo = templateHALData->dingusVideo;

	// Get data for hardware cursor.
	pHardwareCursorData = &(templateHALData->hardwareCursorData);

	if ( !pHardwareCursorData->cursorSet )
	{
		ControlWriteRegister(kRegFieldControlHWCURSOR_ENABLE, 0);
	}
	else
	{
		if ( !pHardwareCursorData->cursorVisible )
		{
			ControlWriteRegister(kRegFieldControlHWCURSOR_ENABLE, 0);
		}
	}
	if (templateHALData->setCursorClutEntriesPending)
	{
		dingusVideo->regsDisplay[COLOR_INDEX] = EndianSwap32Bit(CLUT_CURSOR); // Start at CLUT entry 0
		for ( i = 0; i < kNumHardwareCursorColors; i++ )
		{
			dingusVideo->regsDisplay[COLOR_DATA] = RGB_COLOR(
				pHardwareCursorData->transformedColorMap[i].rgb.red,
				pHardwareCursorData->transformedColorMap[i].rgb.green,
				pHardwareCursorData->transformedColorMap[i].rgb.blue
			);
		}
		templateHALData->setCursorClutEntriesPending = false;
	}


	// Check if we should move or not.
	deferredMove = pHardwareCursorData->deferredMove;
	while (!CompareAndSwap (deferredMove, false, (UInt32 *) &(pHardwareCursorData->deferredMove)))
		deferredMove = pHardwareCursorData->deferredMove;
	if (false == deferredMove)
		return;

	currentY = pHardwareCursorData->y;

	x = pHardwareCursorData->deferredX;
	y = pHardwareCursorData->deferredY;

	// Calculate proper currentY and height.
	screenWidth = templateHALData->width;
	screenHeight = templateHALData->height;
	height = 16;

	if (currentY < 0)
	{
		height -= -currentY;
		currentY = 0;
	}
	if ((currentY + height) > screenHeight)
		height = screenHeight - currentY;

	hardwareCursorImage = (Ptr) &(pHardwareCursorData->cursorImage);
	#ifdef _MSC_VER
		#pragma message("fix me")
	#else
		#warning fix me
	#endif

	pHardwareCursorFrameBuffer = templateHALData->baseAddressPageCurrent;
	screenRowBytes = templateHALData->rowBytes;

	pScreenRowBase = (Ptr) pHardwareCursorFrameBuffer + currentY*screenRowBytes;
	for (i = 0; i < height; i++)
	{
		pScreenImage = (UInt32 *) pScreenRowBase;
		for (j = 0; j < 4; j++)
			*pScreenImage++ = 0;
		pScreenRowBase += screenRowBytes;
	}
	pHardwareCursorData->x = x;
	pHardwareCursorData->y = y;

	// Calculate proper width, height, cursorStartX, and cursorStartY.
	width = 16;
	height = 16;
	cursorStartX = 0;
	cursorStartY = 0;

	if (x < 0)
	{
		width -= -x;
		cursorStartX = -x;
		x = 0;
	}
	if ((x + width) > screenWidth)
		width = screenWidth - x;

	if (y < 0)
	{
		height -= -y;
		cursorStartY = -y;
		y = 0;
	}
	if ((y + height) > screenHeight)
		height = screenHeight - y;

	pScreenRowBase = (Ptr) pHardwareCursorFrameBuffer + y*screenRowBytes;
	cursorRowBytes = 8;
	pCursorRowBase = (Ptr) hardwareCursorImage + cursorStartY*cursorRowBytes +
									((cursorStartX >> 1) & ~4);
	rowLongs = (width + (cursorStartX & 0x07) - 1) >> 3;
	upperLineShift = (cursorStartX & 0x07) << 2;
	lowerLineShift = (32 - upperLineShift);
	endMask = (0xFFFFFFFF << (32 - ((((width - 1) & 0x07) + 1) << 2))) >> upperLineShift;
	for (i = 0; i < height; i++)
	{
		pScreenImage = (UInt32 *) pScreenRowBase;
		pCursorImage = (UInt32 *) pCursorRowBase;
		upperCursorLine = *pCursorImage++;
		for (j = 0; j < rowLongs; j++)
		{
			lowerCursorLine = *pCursorImage++;
			screenLine = (upperCursorLine << upperLineShift) |
							(lowerCursorLine >> lowerLineShift);
			*pScreenImage++ = screenLine;
			upperCursorLine = lowerCursorLine;
		}
		*pScreenImage++ = (upperCursorLine & endMask) << upperLineShift;
		pScreenRowBase += screenRowBytes;
		pCursorRowBase += cursorRowBytes;
	}

	ControlWriteRegister(kRegFieldControlHWCURSOR_POS, (x << 16) | y);

	pHardwareCursorData->deferredMove = false;
	if ( pHardwareCursorData->cursorVisible )
	{
		ControlWriteRegister(kRegFieldControlHWCURSOR_ENABLE, 1);
		pHardwareCursorData->cursorVisible = true;
		pHardwareCursorData->deferredMove = false;
	}
}



//=====================================================================================================
//
// GraphicsHALSetPowerState()
//	Turn off VRAM refresh in Toynbee and turn off the PLL and the CLUT in Spur if going to kAVPowerOff
//
//	For this routine, the relevant fields indicated by 'VDPowerStateRec' are:
//			->	powerState		desired power mode: kAVPowerOff, kAVPowerStandby, kAVPowerSuspend,
//								kAVPowerOn
//
//			<- powerFlags		kPowerStateNeedsRefresh bit set if hw needs to be refreshed after
//								coming out of the designated power state.
//
//=====================================================================================================
GDXErr GraphicsHALSetPowerState(VDPowerStateRec *vdPowerState)
{
	#pragma unused( vdPowerState )
	return kGDXErrUnsupportedFunctionality;
}



//=====================================================================================================
//
// GraphicsHALGetPowerState()
//	The graphics hw might have the ability to to go into some kind of power saving mode.  Just
//	pass the call to the HAL
//
//	For this routine, the relevant fields indicated by 'VDPowerStateRec' are:
//			<- powerState		current power mode: kAVPowerOff, kAVPowerStandby, kAVPowerSuspend,
//								kAVPowerOn
//
//			<- powerFlags		kPowerStateNeedsRefresh bit set if hw needs to be refreshed after
//								coming out of power state
//
//=====================================================================================================
GDXErr GraphicsHALGetPowerState(VDPowerStateRec *vdPowerState)
{
	#pragma unused( vdPowerState )
	return kGDXErrUnsupportedFunctionality;
}




//=====================================================================================================
//
// TemplateSetCursorColors()
//
//=====================================================================================================
static void TemplateSetCursorColors(void)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	UInt8 startPosition = templateHALData->startPosition;
	UInt8 endPosition = templateHALData->endPosition;
	DingusVideoRegisters *dingusVideo = templateHALData->dingusVideo;
	UInt32 i;

	dingusVideo->regsDisplay[COLOR_INDEX] = EndianSwap32Bit(CLUT_FRAMEBUFFER + startPosition);

	for (i = startPosition; i <= endPosition; i++)
	{
		dingusVideo->regsDisplay[COLOR_DATA] = RGB_COLOR(
			templateHALData->savedClut[i].red,
			templateHALData->savedClut[i].green,
			templateHALData->savedClut[i].blue
		);
	}

	templateHALData->setClutEntriesPending = false;
	templateHALData->startPosition = 255;
	templateHALData->endPosition = 0;
}



//=====================================================================================================
//
// GraphicsHALSetClutBehavior()
//
//=====================================================================================================
GDXErr GraphicsHALSetClutBehavior(VDClutBehavior *vdClutBehavior)
{
	GDXErr err = kGDXErrInvalidParameters;

	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	Boolean oldSetClutAtVBL = templateHALData->setClutAtVBL;
	Boolean newSetClutAtVBL;

	if (*vdClutBehavior != kSetClutAtSetEntries && *vdClutBehavior != kSetClutAtVBL)
		goto ErrorExit;

	if (*vdClutBehavior != kSetClutAtSetEntries)
	{
		templateHALData->setClutAtVBL = true;
		newSetClutAtVBL = true;
	}
	else
	{
		templateHALData->setClutAtVBL = false;
		newSetClutAtVBL = false;
	}

	if (oldSetClutAtVBL & !newSetClutAtVBL && templateHALData->setClutEntriesPending)
	{
		Boolean vblInterruptsEnabled = GraphicsOSSSetVBLInterrupt(false);
		if (templateHALData->setClutEntriesPending)
		{
			TemplateSetCursorColors();
		}
		if ( vblInterruptsEnabled )
			GraphicsOSSSetVBLInterrupt( true );
	}

	err = kGDXErrNoError;

ErrorExit:

	return err;
}



//=====================================================================================================
//
// GraphicsHALGetClutBehavior()
//
//=====================================================================================================
GDXErr GraphicsHALGetClutBehavior(VDClutBehavior *vdClutBehavior)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	if (templateHALData->setClutAtVBL)
		*vdClutBehavior = kSetClutAtVBL;
	else
		*vdClutBehavior = kSetClutAtSetEntries;

	return kGDXErrNoError;
}


//=====================================================================================================
//
// GraphicsHALProgramPage()
//
//=====================================================================================================
GDXErr GraphicsHALProgramPage(SInt16 page, Ptr* baseAddress)
{
	GDXErr err;
	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	SpurHardwareCursorData *pHardwareCursorData = &(templateHALData->hardwareCursorData);
	DingusVideoRegisters *dingusVideo = templateHALData->dingusVideo;
	Boolean vblInterruptsEnabled;

	if (page < 0 || page > 1)
	{
		err = kGDXErrInvalidParameters;
		goto ErrorExit;
	}

	if (page == templateHALData->currentPage)
	{
		err = kGDXErrNoError;
		goto ErrorExit;
	}

	templateHALData->currentPage = page;

	ControlWriteRegister(kRegFieldControlHWCURSOR_ENABLE, 0);

	pHardwareCursorData->cursorRendered = false;
	pHardwareCursorData->cursorVisible = false;

	vblInterruptsEnabled = GraphicsOSSSetVBLInterrupt(false);
	TemplateWaitForVBL();

	TemplateSetPage();

	if (vblInterruptsEnabled)
		GraphicsOSSSetVBLInterrupt(true);
	TemplateCalcPageBaseAddress(&templateHALData->baseAddressPageCurrent);
	pHardwareCursorData->cursorCleared = false;
	*baseAddress = templateHALData->baseAddressPageCurrent;

	err = kGDXErrNoError;
ErrorExit:
	return err;
}



//=====================================================================================================
//
// GraphicsHALGetTimingRanges()
//
//=====================================================================================================
GDXErr GraphicsHALGetTimingRanges(VDDisplayTimingRangeRec *vdDisplayTimingRange)
{
	if ( vdDisplayTimingRange->csRangeSize < sizeof(VDDisplayTimingRangeRec) )
		return kGDXErrInvalidParameters;

	vdDisplayTimingRange->csRangeSize = sizeof(VDDisplayTimingRangeRec);  /* Init to sizeof(VDDisplayTimingRangeRec) */ // Reserved in Mac OS X
	vdDisplayTimingRange->csRangeType = 0;            /* Init to 0 */ // Reserved in Mac OS X
	vdDisplayTimingRange->csRangeVersion = 0;         /* Init to 0 */
	vdDisplayTimingRange->csRangeReserved = 0;        /* Init to 0 */ // Reserved in Mac OS X

	vdDisplayTimingRange->csRangeBlockIndex = 0;      /* Requested block (first index is 0)*/ // Reserved in Mac OS X
	vdDisplayTimingRange->csRangeGroup = 0;           /* set to 0 */ // Reserved in Mac OS X
	vdDisplayTimingRange->csRangeBlockCount = 0;      /* # blocks */ // Reserved in Mac OS X
	vdDisplayTimingRange->csRangeFlags = 0;           /* dependent video */ // Reserved in Mac OS X

	vdDisplayTimingRange->csMinPixelClock =    6000000;
	vdDisplayTimingRange->csMaxPixelClock = 6000000000;

	vdDisplayTimingRange->csMaxPixelError =    500000; // Hz; Max dot clock error
	vdDisplayTimingRange->csTimingRangeSyncFlags = kRangeSupportsSeperateSyncsMask | kRangeSupportsCompositeSyncMask; // I have not tested if kIORangeSupportsVSyncSerration is valid for the control hardware - joevt
	vdDisplayTimingRange->csTimingRangeSignalLevels = kAnalogSignalLevel_0700_0300 | kAnalogSignalLevel_0714_0286 | kAnalogSignalLevel_1000_0400 | kAnalogSignalLevel_0700_0000; // I haven't tested which of these is true - joevt
	vdDisplayTimingRange->csReserved0 = 0;

	vdDisplayTimingRange->csMinFrameRate =    40; // Hz; min in OS 9 driver is 50 Hz
	vdDisplayTimingRange->csMaxFrameRate =   200; // Hz; max in OS 9 driver is 120 Hz
	vdDisplayTimingRange->csMinLineRate =  10000; // Hz; min in OS 9 driver is 15.625 kHz
	vdDisplayTimingRange->csMaxLineRate = 100000; // Hz; max in OS 9 driver is 80 kHz


	vdDisplayTimingRange->csMaxHorizontalTotal = 3000;   /* Clocks - Maximum total (active + blanking) */
	vdDisplayTimingRange->csMaxVerticalTotal = 3000;     /* Clocks - Maximum total (active + blanking) */
	vdDisplayTimingRange->csMaxTotalReserved1 = 0;    /* Reserved */
	vdDisplayTimingRange->csMaxTotalReserved2 = 0;    /* Reserved */



	                      /* Some cards require that some timing elements*/
	                      /* be multiples of a "character size" (often 8*/
	                      /* clocks).  The "xxxxCharSize" fields document*/
	                      /* those requirements.*/

	// lower resolutions on chaos/control can use a charsize of 2 but higher resolutions require a char size of 4

	vdDisplayTimingRange->csCharSizeHorizontalActive = 32;
	vdDisplayTimingRange->csCharSizeHorizontalBlanking = 4;
	vdDisplayTimingRange->csCharSizeHorizontalSyncOffset = 4;
	vdDisplayTimingRange->csCharSizeHorizontalSyncPulse = 4;

	vdDisplayTimingRange->csCharSizeVerticalActive = 1;
	vdDisplayTimingRange->csCharSizeVerticalBlanking = 1;
	vdDisplayTimingRange->csCharSizeVerticalSyncOffset = 1;
	vdDisplayTimingRange->csCharSizeVerticalSyncPulse = 1;

	vdDisplayTimingRange->csCharSizeHorizontalBorderLeft = 4;
	vdDisplayTimingRange->csCharSizeHorizontalBorderRight = 4;
	vdDisplayTimingRange->csCharSizeVerticalBorderTop = 1;
	vdDisplayTimingRange->csCharSizeVerticalBorderBottom = 1;

	vdDisplayTimingRange->csCharSizeHorizontalTotal = 4; // Character size for active + blanking
	vdDisplayTimingRange->csCharSizeVerticalTotal = 1; // Character size for active + blanking
	vdDisplayTimingRange->csCharSizeReserved1 = 0;    /* Reserved (Init to 0) */

	vdDisplayTimingRange->csMinHorizontalActiveClocks = 0;
	vdDisplayTimingRange->csMaxHorizontalActiveClocks = 3000;
	vdDisplayTimingRange->csMinHorizontalBlankingClocks = 0;
	vdDisplayTimingRange->csMaxHorizontalBlankingClocks = 3000;

	vdDisplayTimingRange->csMinHorizontalSyncOffsetClocks = 0;
	vdDisplayTimingRange->csMaxHorizontalSyncOffsetClocks = 3000;
	vdDisplayTimingRange->csMinHorizontalPulseWidthClocks = 0;
	vdDisplayTimingRange->csMaxHorizontalPulseWidthClocks = 3000;

	vdDisplayTimingRange->csMinVerticalActiveClocks = 0;
	vdDisplayTimingRange->csMaxVerticalActiveClocks = 3000 * 3000;
	vdDisplayTimingRange->csMinVerticalBlankingClocks = 0;
	vdDisplayTimingRange->csMaxVerticalBlankingClocks = 3000 * 3000;

	vdDisplayTimingRange->csMinVerticalSyncOffsetClocks = 0;
	vdDisplayTimingRange->csMaxVerticalSyncOffsetClocks = 3000 * 3000;
	vdDisplayTimingRange->csMinVerticalPulseWidthClocks = 0;
	vdDisplayTimingRange->csMaxVerticalPulseWidthClocks = 3000 * 3000;

	vdDisplayTimingRange->csMinHorizontalBorderLeft = 0;
	vdDisplayTimingRange->csMaxHorizontalBorderLeft = 0;
	vdDisplayTimingRange->csMinHorizontalBorderRight = 0;
	vdDisplayTimingRange->csMaxHorizontalBorderRight = 0;

	vdDisplayTimingRange->csMinVerticalBorderTop = 0;
	vdDisplayTimingRange->csMaxVerticalBorderTop = 0;
	vdDisplayTimingRange->csMinVerticalBorderBottom = 0;
	vdDisplayTimingRange->csMaxVerticalBorderBottom = 0;

	vdDisplayTimingRange->csReserved1 = 0;            /* Reserved (Init to 0)*/
	vdDisplayTimingRange->csReserved2 = 0;            /* Reserved (Init to 0)*/
	vdDisplayTimingRange->csReserved3 = 0;            /* Reserved (Init to 0)*/
	vdDisplayTimingRange->csReserved4 = 0;            /* Reserved (Init to 0)*/

	vdDisplayTimingRange->csReserved5 = 0;            /* Reserved (Init to 0)*/
	vdDisplayTimingRange->csReserved6 = 0;            /* Reserved (Init to 0)*/
	vdDisplayTimingRange->csReserved7 = 0;            /* Reserved (Init to 0)*/
	vdDisplayTimingRange->csReserved8 = 0;            /* Reserved (Init to 0)*/

	return kGDXErrNoError;
}



//=====================================================================================================
//
// GraphicsHALGetDetailedTiming()
//
//=====================================================================================================
GDXErr GraphicsHALGetDetailedTiming(VDDetailedTimingRec *vdDetailedTiming)
{
	BitDepthIndependentData bdiData;
	WidthAndDepthDependentData vwdData;
	DisplayInfo info;

	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	GDXErr err = kGDXErrUnknownError;								// Assume failure

	if ( vdDetailedTiming->csTimingSize < sizeof(VDDetailedTimingRec) ) // Reserved in Mac OS X
	{
		err = kGDXErrInvalidParameters;
		goto ErrorExit;
	}

	err = TemplateGetDisplayData( false, vdDetailedTiming->csDisplayModeID, kDepthMode1,
									 &bdiData, &vwdData, &info );
	if (err)
	{
		err = kGDXErrDisplayModeIDUnsupported;
		goto ErrorExit;
	}

	vdDetailedTiming->csTimingSize = sizeof(VDDetailedTimingRec);
	vdDetailedTiming->csTimingType = 0;           /* Init to 0*/ // Reserved in Mac OS X
	vdDetailedTiming->csTimingVersion = 0;        /* Init to 0*/ // Reserved in Mac OS X
	vdDetailedTiming->csTimingReserved = 0;       /* Init to 0*/ // Reserved in Mac OS X

	vdDetailedTiming->csDisplayModeSeed = info.displayModeSeed;      /* */ // scalerFlags in Mac OS X
	vdDetailedTiming->csDisplayModeState = info.displayModeState;     /* Display Mode state*/ // horizontalScaled in Mac OS X
	vdDetailedTiming->csDisplayModeAlias = info.dinfo_displayModeAlias;     /* Mode to use when programmed.*/ // verticalScaled in Mac OS X

	vdDetailedTiming->csSignalConfig = kAnalogSetupExpectedMask; // this is in the EDID - /chaos/control can't control this
	vdDetailedTiming->csSignalLevels = kAnalogSignalLevel_0700_0300; // this in the EDID - /chaos/control can't control this

	vdDetailedTiming->csPixelClock = (UInt64)bdiData.pixelClock * 10;    /* Hz*/

	vdDetailedTiming->csMinPixelClock = vdDetailedTiming->csPixelClock;        /* Hz - With error what is slowest actual clock */
	vdDetailedTiming->csMaxPixelClock = vdDetailedTiming->csPixelClock;        /* Hz - With error what is fasted actual clock */

	vdDetailedTiming->csHorizontalActive = bdiData.hActive; /* Pixels*/
	vdDetailedTiming->csHorizontalBlanking = bdiData.hTotal - bdiData.hActive; /* Pixels*/
	vdDetailedTiming->csHorizontalSyncOffset = bdiData.hSyncBegin - bdiData.hActive; /* Pixels*/
	vdDetailedTiming->csHorizontalSyncPulseWidth = bdiData.hSyncEnd - bdiData.hSyncBegin; /* Pixels*/

	vdDetailedTiming->csVerticalActive = bdiData.vActive; //  >> !!(bdiData.timingFlags & INTERLACED) /* Lines*/
	vdDetailedTiming->csVerticalBlanking = bdiData.vTotal - bdiData.vActive; /* Lines*/
	vdDetailedTiming->csVerticalSyncOffset = bdiData.vSyncBegin - bdiData.vActive; /* Lines*/
	vdDetailedTiming->csVerticalSyncPulseWidth = bdiData.vSyncEnd - bdiData.vSyncBegin; /* Lines*/

	vdDetailedTiming->csHorizontalBorderLeft = 0; /* Pixels*/
	vdDetailedTiming->csHorizontalBorderRight = 0; /* Pixels*/
	vdDetailedTiming->csVerticalBorderTop = 0;    /* Lines*/
	vdDetailedTiming->csVerticalBorderBottom = 0; /* Lines*/

	vdDetailedTiming->csHorizontalSyncConfig = kSyncPositivePolarityMask * bdiData.hSyncPolarity;
	vdDetailedTiming->csHorizontalSyncLevel = 0;  /* Future use (init to 0)*/
	vdDetailedTiming->csVerticalSyncConfig = kSyncPositivePolarityMask * bdiData.vSyncPolarity;
	vdDetailedTiming->csVerticalSyncLevel = 0;    /* Future use (init to 0)*/

	vdDetailedTiming->csReserved1 = 0;            /* Init to 0*/
	vdDetailedTiming->csReserved2 = 0;            /* Init to 0*/
	vdDetailedTiming->csReserved3 = 0;            /* Init to 0*/
	vdDetailedTiming->csReserved4 = 0;            /* Init to 0*/

	vdDetailedTiming->csReserved5 = 0;            /* Init to 0*/
	vdDetailedTiming->csReserved6 = 0;            /* Init to 0*/
	vdDetailedTiming->csReserved7 = 0;            /* Init to 0*/
	vdDetailedTiming->csReserved8 = 0;            /* Init to 0*/

	err = kGDXErrNoError;
ErrorExit:
	return err;
}



//=====================================================================================================
//
// GraphicsHALSetDetailedTiming()
//
//=====================================================================================================
static DepthMode CalcMaxDepthMode( UInt32 width, UInt32 height )
{
	DepthMode maxDepthMode = 0;
	DepthMode depthMode;

	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	for (depthMode = kDepthMode1; depthMode < kDepthMode6; depthMode++) {
		UInt32 frameBytes = CalcFrameBytes(width, height, maxDepthMode);
		if (frameBytes + kHardwareCursorImageSize > templateHALData->ramPerDisplay)
			break;
		maxDepthMode = depthMode;
	}
	return maxDepthMode;
}



GDXErr GraphicsHALSetDetailedTiming(VDDetailedTimingRec *vdDetailedTiming)
{
	BitDepthIndependentData bdiData;
	WidthAndDepthDependentData vwdData[kDepthModeNumIndexes];
	DisplayInfo info;
	int i;
	int depthModeNdx;

	GDXErr err;

	if ( vdDetailedTiming->csTimingSize < sizeof(VDDetailedTimingRec) || // Reserved in Mac OS X
		vdDetailedTiming->csSignalConfig != kAnalogSetupExpectedMask || // this is in the EDID - /chaos/control can't control this
		vdDetailedTiming->csSignalLevels != kAnalogSignalLevel_0700_0300 || // this in the EDID - /chaos/control can't control this
		vdDetailedTiming->csHorizontalBorderLeft != 0 || /* Pixels*/
		vdDetailedTiming->csHorizontalBorderRight != 0 || /* Pixels*/
		vdDetailedTiming->csVerticalBorderTop != 0 ||    /* Lines*/
		vdDetailedTiming->csVerticalBorderBottom != 0 /* Lines*/
		)
	{
		err = kGDXErrInvalidParameters;
		goto ErrorExit;
	}

	err = kGDXErrDisplayModeIDUnsupported;					// Assume failure

	// Get the 'bdiData' and 'bddData' for the requested 'DisplayModeID'
	for (i = kFirstProgrammableModeInfo; i < kNumModeInfos; i++)
	{
		if (vdDetailedTiming->csDisplayModeID == gDisplayModeInfo[i].info.dinfo_displayModeID)
		{
			err = kGDXErrNoError;
			break;
		}
	}

	if (err)
	{
		goto ErrorExit;
	}

	info.dinfo_displayModeID = vdDetailedTiming->csDisplayModeID;			/* Init to 0*/
	info.displayModeSeed = vdDetailedTiming->csDisplayModeSeed;				/* */ // scalerFlags in Mac OS X
	info.displayModeState = kDMSModeNotReady;								/* Display Mode state*/ // horizontalScaled in Mac OS X
	info.dinfo_displayModeAlias = vdDetailedTiming->csDisplayModeAlias;		/* Mode to use when programmed.*/ // verticalScaled in Mac OS X

	info.timingData = timingInvalid;

	info.maxDepthMode = 0;

	info.width = vdDetailedTiming->csHorizontalActive;
	info.height = vdDetailedTiming->csVerticalActive;


	// CONTROL
	bdiData.interlaced = false;
	bdiData.hSyncPolarity = ( vdDetailedTiming->csHorizontalSyncConfig & kSyncPositivePolarityMask ) != 0;
	bdiData.vSyncPolarity = ( vdDetailedTiming->csVerticalSyncConfig & kSyncPositivePolarityMask ) != 0;
	bdiData.cSyncDisable = true;

	bdiData.hActive = vdDetailedTiming->csHorizontalActive;
	bdiData.hSyncBegin = bdiData.hActive + vdDetailedTiming->csHorizontalSyncOffset;
	bdiData.hSyncEnd = bdiData.hSyncBegin + vdDetailedTiming->csHorizontalSyncPulseWidth;
	bdiData.hTotal = bdiData.hActive + vdDetailedTiming->csHorizontalBlanking;

	bdiData.vActive = vdDetailedTiming->csVerticalActive;
	bdiData.vSyncBegin = bdiData.vActive + vdDetailedTiming->csVerticalSyncOffset;
	bdiData.vSyncEnd = bdiData.vSyncBegin + vdDetailedTiming->csVerticalSyncPulseWidth;
	bdiData.vTotal = bdiData.vActive + vdDetailedTiming->csVerticalBlanking;

	info.maxDepthMode = CalcMaxDepthMode(vdDetailedTiming->csHorizontalActive, vdDetailedTiming->csVerticalActive);

	if ( info.maxDepthMode == 0 )
	{
		err = kGDXErrInvalidParameters;
		goto ErrorExit;
	}

	for ( depthModeNdx = kDepthMode1Index; depthModeNdx < kDepthModeNumIndexes; depthModeNdx ++ )
	{
		if ( depthModeNdx + kDepthMode1 <= info.maxDepthMode )
		{
			vwdData[depthModeNdx].pixelDepth = depthModeNdx;
		}
		else
		{
			vwdData[depthModeNdx].pixelDepth = 0;
		}
	}

	gDisplayModeInfo[i].info.displayModeState = kDMSModeNotReady;
	gDisplayModeInfo[i].info = info;
	gDisplayModeInfo[i].bdiData = bdiData;
	BlockMoveData( vwdData, gDisplayModeInfo[i].vwdData, sizeof( vwdData ) );

	gDisplayModeInfo[i].info.displayModeState = vdDetailedTiming->csDisplayModeState;     /* Display Mode state*/ // horizontalScaled in Mac OS X

	err = kGDXErrNoError;
ErrorExit:
	return err;
}



//=====================================================================================================
//
// GraphicsHALPrivateControl()
//	Routine accepts private control calls.  The core passes all control codes that it doesn't
//	understand to this routine.  If the HAL knows what to do with the code, it hands it off
//	accordingly.
//
//		-> genericPtr			Points to the data structure that the HAL needs for this control
//								call.  Should be cast to appropriate data type if internal routine is
//								invoked.
//		-> privateControlCode	The private csCode that the HAL might know what to do with.
//
//=====================================================================================================
OSErr GraphicsHALPrivateControl(void * /*genericPtr*/, SInt16 /*privateControlCode*/)
{
	OSErr returnErr = controlErr;					// Assume cs code is invalid
	return returnErr;
}



//=====================================================================================================
//
// GraphicsHALPrivateStatus()
//	Routine accepts private status calls.  The core passes all status codes that it doesn't
//	understand to this routine.  If the HAL knows what to do with the code, it hands it off
//	accordingly.
//
//		-> genericPtr			Points to the data structure that the HAL needs for this status
//								call.  Should be cast to appropriate data type if internal routine is
//								invoked.
//		-> privateStatusCode	The private csCode that the HAL might know what to do with.
//
//=====================================================================================================
OSErr GraphicsHALPrivateStatus(void * /*genericPtr*/, SInt16 /*privateStatusCode*/)
{
	OSErr returnErr = statusErr;					// Assume cs code is invalid
	return returnErr;
}



//=====================================================================================================
//
// TemplateMapDepthModeToCLUTAttributes()
//
//	This simple routine maps a 'DepthMode' to the corresponding CLUT attritributes associate with it.
//
//		-> depthMode	The 'DepthMode' for which the CLUT attributes are desired.
//		<- startAddress	The physical address that corresponds to logical address 0.
//		<- entryOffset	The physical offset between each logical address.
//
//=====================================================================================================
static GDXErr TemplateMapDepthModeToCLUTAttributes(DepthMode depthMode,
				UInt32 *startAddress, UInt32 *entryOffset)
{
	// Define a new type which maps a 'DepthMode' to the corresponding physical attributes of a CLUT.
	// These attributes are needed because the logical addresses when in 1, 2, or 4 bpp do not
	// correspond to the physical address.  Instead, they are distributed evenly through out the CLUT
	// address space.

	typedef struct DepthModeToCLUTAttributesMap DepthModeToCLUTAttributesMap;
	struct DepthModeToCLUTAttributesMap
	{
		DepthMode depthMode;	// DepthMode for which these attributes apply
		UInt32 startAddress;	// Physical address of logical address 0
		UInt32 entryOffset;		// physical offset between each logical address
	};

	#define kMapSize 3

	DepthModeToCLUTAttributesMap depthModeMap[kMapSize] =
	{
		{kDepthMode1, 0x00, 0x01},						// 8 bpp
		{kDepthMode2, 0x00, 0x01},						// 16 bpp
		{kDepthMode3, 0x00, 0x01}						// 32 bpp
	};

	UInt32 i;									// Loop control variable

	GDXErr err = kGDXErrDepthModeUnsupported;					// Assume invalid depthmode
	// Scan the 'DepthModeToCLUTAttributesMap' to find CLUT attributes for the given depth mode.


	for (i = 0 ; i < kMapSize ; i++)
	{
		if (depthModeMap[i].depthMode == depthMode)
		{
			*startAddress = depthModeMap[i].startAddress;
			*entryOffset = depthModeMap[i].entryOffset;
			err = kGDXErrNoError;
			break;
		}
	}

ErrorExit:

	return err;
}



//=====================================================================================================
//
// TemplateWaitForVBL()
//	This routine does not return until it senses that a VBL has occurred.
//	For the Template class of machines, VBL information is in 3 registers
//	interruptMask, interruptStatus, clearCursorInterrupt.
//	For the Template of machines the hardware operates in the follow way:
//
//	Internal Template interrupts are always on.  Just watch the interruptStatus register
//	and see when the kCursorInterruptStatusBit is set.
//
//	This routine does not make any assumptions about whether external VBL interrupts are
//	enabled or disabled prior to its calling.  However, it is recommended that the
//	external VBL interrupts be disabled prior to calling this if any type of accurate waiting
//	takes place.
//
//=====================================================================================================
static void TemplateWaitForVBL(void)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	DingusVideoRegisters *dingusVideo = templateHALData->dingusVideo;
	UInt32 status;

	if (EndianSwap32Bit(dingusVideo->regsDisplay[IMMEDIATE_FLAGS]) & DISABLE_TIMING == 0) // toynbeeRunning
	{
		HWRegister32Bit *interruptStatus = &templateHALData->dingusVideo->regsDisplay[INT_STATUS];

		// clearCursorInterrupt
		dingusVideo->regsDisplay[INT_ENABLE] = EndianSwap32Bit(VBL_IRQ_CLR); // 0x04
		dingusVideo->regsDisplay[INT_ENABLE] = EndianSwap32Bit(VBL_IRQ_EN | VBL_IRQ_CLR); // 0x0C

		while (true)
		{
			status = EndianSwap32Bit(*interruptStatus);
			if (status & VBL_IRQ_STAT)
				break;
		}
	}
}



//=====================================================================================================
//
// TemplateClearInternalVBLInterrupts()
//	This routine clears the external interrupt source in GrandCentral.  This routine
//	is the interrupt Handler that is passed to the OSS.
//	Register clearCursorInterrupt in Template is accessed to clear the interrupt.
//
//=====================================================================================================
static void TemplateClearInternalVBLInterrupts(void* vblRefCon)
{
	#pragma unused( vblRefCon )
	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	DingusVideoRegisters *dingusVideo = templateHALData->dingusVideo;

	if (templateHALData->setClutEntriesPending && !templateHALData->clutBusy)
	{
		TemplateSetCursorColors();
	}

	dingusVideo->regsDisplay[INT_ENABLE] = EndianSwap32Bit(VBL_IRQ_CLR);					// clear Template interrupt
	dingusVideo->regsDisplay[INT_ENABLE] = EndianSwap32Bit(VBL_IRQ_EN | VBL_IRQ_CLR);

	// Flush out any deferred cursor moving.
	DeferredMoveHardwareCursor();
}



//=====================================================================================================
//
// TemplateAssertVideoReset()
//	In the Template graphics architecture, the frame buffer controller (Toynbee) needs to be put into reset
// prior to programing.
//
//		-> toynbee	pointer to structure of register address for Toynbee
//
//=====================================================================================================
static GDXErr TemplateAssertVideoReset(void)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	return (kGDXErrNoError);
}



//=====================================================================================================
//
// TemplateSetupClockGenerator()
//	This will program the video clock generator for the Template graphics architecture.
//
//		-> bdiData		The hardware programming parameters that DO NOT vary with bit depth
//
//		<> usingClockSetA
//		As an input, this is 'true' if the current active set of the Frankenstein clock generator is
//		Set A, 'false' if Set B is in use.
//		As an output, this is changed to reflect the current set in use.
//
//=====================================================================================================
static GDXErr TemplateSetupClockGenerator(BitDepthIndependentData* bdiData)
{
	ControlWriteRegister(kRegFieldControlPIXEL_CLOCK, bdiData->pixelClock);
	return kGDXErrNoError;
}



//=====================================================================================================
//
// TemplateSetupClockGenerator()
//
//=====================================================================================================
static GDXErr TemplateSetPage()
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	return kGDXErrNoError;
}



//=====================================================================================================
//
// TemplateSetupFBController()
//	This will program the frame buffer for the Template graphics architecture.
//
//		-> resolutionChange	true if bdi registers should be hit
//		-> toynbee			pointer to structure of register address for Toynbee
//		-> dingusVideo		pointer to structure of register address for DingusVideo
//		-> bdiData			The hardware programming parameters that DO NOT vary with bit depth
//		-> bddData			The hardware programming parameters that DO vary with bit depth
//		-> vwData			The hardware programming parameters that DO vary VRAM width
//		-> vwdData			The hardware programming parameters that DO vary VRAM width AND bit depth
//
//=====================================================================================================
static GDXErr TemplateSetupFBController(DepthMode /*depthMode*/, const DisplayInfo */*info*/,
		const BitDepthIndependentData* bdiData, const WidthAndDepthDependentData *vwdData)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	// Program DingusVideo with the 'BitDepthIndependentData'

	ControlWriteRegister(kRegFieldControlHACTIVE         , bdiData->hActive      );
	ControlWriteRegister(kRegFieldControlHSYNCBEGIN      , bdiData->hSyncBegin   );
	ControlWriteRegister(kRegFieldControlHSYNCEND        , bdiData->hSyncEnd     );
	ControlWriteRegister(kRegFieldControlHTOTAL          , bdiData->hTotal       );
	ControlWriteRegister(kRegFieldControlVACTIVE         , bdiData->vActive      );
	ControlWriteRegister(kRegFieldControlVSYNCBEGIN      , bdiData->vSyncBegin   );
	ControlWriteRegister(kRegFieldControlVSYNCEND        , bdiData->vSyncEnd     );
	ControlWriteRegister(kRegFieldControlVTOTAL          , bdiData->vTotal       );
	ControlWriteRegister(kRegFieldControlPIXEL_CLOCK     , bdiData->pixelClock   );
	ControlWriteRegister(kRegFieldControlINTERLACED      , bdiData->interlaced   );
	ControlWriteRegister(kRegFieldControlVSYNC_POLARITY  , bdiData->vSyncPolarity);
	ControlWriteRegister(kRegFieldControlHSYNC_POLARITY  , bdiData->hSyncPolarity);

	ControlWriteRegister(kRegFieldControlHSYNC_DISABLE, 0); // enable horizontal sync pulses
	ControlWriteRegister(kRegFieldControlVSYNC_DISABLE, 0); // enable vertical sync pulses

	if (bdiData->cSyncDisable)
	{
		templateHALData->compositSyncDisabled = true; /* composite sync disabled */
		ControlWriteRegister(kRegFieldControlCSYNC_DISABLE, 1);
	}
	else
	{
		templateHALData->compositSyncDisabled = false; /* composite sync enabled */
		ControlWriteRegister(kRegFieldControlCSYNC_DISABLE, 0);
	}

	if (!templateHALData->compositSyncDisabled)
		templateHALData->cvhSyncDisabled &= ~kCompositeSyncMask; /* composite sync enabled */
	else
		templateHALData->cvhSyncDisabled |= kCompositeSyncMask; /* composite sync disabled */

	templateHALData->interlaced = bdiData->interlaced;

	// Program DingusVideo with the 'WidthAndDepthDependentData'
	ControlWriteRegister(kRegFieldControlPIXEL_DEPTH, vwdData->pixelDepth);


	// Program Toynbee with the 'WidthAndDepthDependentData'

	// Program Toynbee with the 'BitDepthDependentData'
	ControlWriteRegister(kRegFieldControlFRAMEBUFFER_ROWBYTES, templateHALData->rowBytes);

	// Program Toynbee with the 'BitDepthIndependentData'... (could change...always hit)
	#ifdef _MSC_VER
		#pragma message("fix me")
	#else
		#warning fix me
	#endif
//	ControlWriteRegister(kRegFieldControlFRAMEBUFFER_BASE, templateHALData->currentPageAddress);

	return kGDXErrNoError;
}



//=====================================================================================================
// TemplateReleaseVideoReset()
//	In the Template graphics architecture, the frame buffer controller (Toynbee) needs to be put through a
//	release-reset-release process.  Starting from a fully reset state, the steps are as follows:
//
//		release Toynbee from reset
//		reassert Toynbee reset
//		release Toynbee from reset
//		release VRAM state machine from reset
//		release video refresh state machine from reset
//
//		-> toynbee	pointer to structure of register address for Toynbee
//
//=====================================================================================================
static GDXErr TemplateReleaseVideoReset(void)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	return (kGDXErrNoError);
}



//=====================================================================================================
//
// TemplateGetDisplayData()
//	This routine returns all the data necessary to program the graphics hardware to the desired
//	'displayModeID', 'depthMode', and 'page', based on the the system configuration passed in.
//
//		-> displayModeID	get information for this resolution
//		-> depthMode		get information for this depthMode
//
//		<- bdiData		The hardware programming parameters that DO NOT vary with bit depth
//		<- bddData		The hardware programming parameters that DO vary with bit depth
//		<- vwData		The hardware programming parameters that DO vary VRAM width
//		<- vwdData		The hardware programming parameters that DO vary VRAM width AND bit depth
//		<- info			various tidbits of information:
//			the relevant fields of the 'DisplayInfo' structure are as follows:
//
//		<- width			horizontal pixels for the given displayModeID
//		<- height			vertical pixels for the given displayModeID
//		<- maxDepthMode		max depthMode based on inputs.  if inputs can't be supported,
//							maxDepthMode = kDepthModeInvalid
//		<- rowBytes			rowBytes reported in get video parameters
//
//=====================================================================================================
static GDXErr TemplateGetDisplayData(Boolean ignoreNotReady, DisplayModeID displayModeID, DepthMode depthMode,
	BitDepthIndependentData* bdiData, WidthAndDepthDependentData* vwdData, DisplayInfo* info)
{
	GDXErr err = kGDXErrDisplayModeIDUnsupported;					// Assume failure

	int i;

	// Get the 'bdiData' and 'bddData' for the requested 'DisplayModeID' and 'DepthMode'
	if ( displayModeID != kDisplayModeIDInvalid )
	{
		for (i = 0; i < kNumModeInfos; i++)
		{
			if (displayModeID == gDisplayModeInfo[i].info.dinfo_displayModeAlias)
			{
				if (ignoreNotReady)
					if (gDisplayModeInfo[i].info.displayModeState != kDMSModeReady)
						continue;

				err = TemplateGet(i, depthMode, bdiData, vwdData, info);
				if (bdiData)
					if (displayModeID == kDisplay_800x600_72Hz_VGA)
					{
						TemplateHALData *templateHALData = GraphicsHALGetHALData();
						// update bdiData if necessary
					}
				break;
			}
		}
	}

ErrorExit:

	return err;
}



//=====================================================================================================
//
// TemplateGetXXX()
//	All of the TemplateGetXXX parameters are described here.  For the given indicies, the proper
//	data is returned.
//
//		-> InfoIndicies		the proper index for each type of data
//			the relevant fields of the 'InfoIndicies' structure are as follows:
//
//		-> dIndex			index for bddData in dRay		( kDepthMode1Index...kDepthMode3Index )
//		-> vwIndex			index for vwData in vwRay		( vw32Index, vw32Index )
//		-> vwdIndex			index for vwdData in vwdRay		( kvw32d8Index...kvw64d32Index )
//
//		<- bdiData		The hardware programming parameters that DO NOT vary with bit depth
//		<- bddData		The hardware programming parameters that DO vary with bit depth
//		<- vwData		The hardware programming parameters that DO vary VRAM width
//		<- vwdData		The hardware programming parameters that DO vary VRAM width AND bit depth
//		<- info			various tidbits of information:
//			the relevant fields of the 'DisplayInfo' structure are as follows:
//
//		<- width			horizontal pixels for the given displayModeID
//		<- height			vertical pixels for the given displayModeID
//		<- maxDepthMode		max depthMode based on inputs.  if inputs can't be supported,
//							maxDepthMode = kDepthModeInvalid
//		<- rowBytes			rowBytes reported in get video parameters
//=====================================================================================================
static GDXErr TemplateGet(short index, DepthMode depthMode,
	BitDepthIndependentData* bdiData, WidthAndDepthDependentData* vwdData, DisplayInfo* info)
{
	GDXErr err = kGDXErrNoError;

	if (info)
		*info = gDisplayModeInfo[ index ].info;
	if (bdiData)
		*bdiData = gDisplayModeInfo[ index ].bdiData;
	if (vwdData)
		*vwdData = gDisplayModeInfo[ index ].vwdData[depthMode - kDepthMode1];

ErrorExit:
	return err;
}



//=====================================================================================================
//
// TemplateResetSenseLines()
//	Before reading the Sense Lines to determine what monitor is connected, the sense lines
//	need to be reset.  Puts sense lines in a known state so the sensing process can start.
//
//=====================================================================================================
static void TemplateResetSenseLines(void)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	DingusVideoRegisters *dingusVideo = templateHALData->dingusVideo;
	ControlWriteRegister(kRegFieldControlMON_SENSE_All, 7 << 3); //  Tristate sense lines
}



//=====================================================================================================
//
// TemplateReadSenseLines()
//
//		<- senseLineValue		read value of the sense lines
//
//=====================================================================================================
static RawSenseCode TemplateReadSenseLines(void)
{

enum { kTemplateSenseLineMask = 0x00000007 };

	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	DingusVideoRegisters *dingusVideo = templateHALData->dingusVideo;

	UInt32 senseLineValue;

	senseLineValue = EndianSwap32Bit(dingusVideo->regsDisplay[MON_SENSE]);	// read sense line

	senseLineValue = (senseLineValue >> 6) & kTemplateSenseLineMask;	// Mask off the sense pins


	// Reset the lines:

	TemplateResetSenseLines();								// reset the sense lines
	return(senseLineValue);
}



//=====================================================================================================
//
// TemplateDriveSenseLines()
//	Drive the approprite sense line so that the two other sense lines can be read.
//
//		-> senseLine		the sense line to drive.
//
//=====================================================================================================
static void TemplateDriveSenseLines(SenseLine senseLine)
{
enum
{
	kDriveAValue = 0b011 << 3,
	kDriveBValue = 0b101 << 3,
	kDriveCValue = 0b110 << 3,
};

	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	DingusVideoRegisters *dingusVideo = templateHALData->dingusVideo;

	UInt32 senseLineValue;


	switch (senseLine)
	{
		case kSenseLineA:
			senseLineValue = kDriveAValue;
			break;
		case kSenseLineB:
			senseLineValue = kDriveBValue;
			break;
		case kSenseLineC:
			senseLineValue = kDriveCValue;
			break;
	}

	ControlWriteRegister(kRegFieldControlMON_SENSE_All, senseLineValue); //  Drive appropriate sense line
}



//=====================================================================================================
//
// TemplateGetExtendedSenseCode()
//	This routine applies the 'standard' extended sense code algorithm to the sense lines to determine
//	their ExtendedSenseCode.
//
// 	For 'standard' sense line hardware, the extended sense code algorithm is as follows:
//	(Note:  as described here, sense line 'A' corresponds to '2', 'B' to '1', and 'C' to '0')
//		* Drive sense line 'A' low and read the values of 'B' and 'C'.
//		* Drive sense line 'B' low and read the values of 'A' and 'C'.
//		* Drive sense line 'C' low and read the values of 'A' and 'B'.
//
//	In this way, a six-bit number of the form BC/AC/AB is generated.
//
//=====================================================================================================
static ExtendedSenseCode TemplateGetExtendedSenseCode()
{
	ExtendedSenseCode extendedBC;								// Result from driving line A
	ExtendedSenseCode extendedAC;								// Result from driving line B
	ExtendedSenseCode extendedAB;								// Result from driving line C

	ExtendedSenseCode extendedSenseCode;						// Final value.

	// Obtain the extendedBC value by driving sense line A
	TemplateDriveSenseLines(kSenseLineA);
	extendedBC = TemplateReadSenseLines();						// xx xx BC
	extendedBC = (extendedBC << 4) & 0b110000;					// BC 00 00

	// Obtain the extendedAC value by driving sense line B
	TemplateDriveSenseLines(kSenseLineB);
	extendedAC = TemplateReadSenseLines();						// xx xA xC
	extendedAC = ((extendedAC >> 2) << 3) |						// (00 A0 00) | (00 0C 00) = 00 AC 00
				 ((extendedAC & 0x1) << 2);

	// Obtain the extendedAB value by driving sense line C
	TemplateDriveSenseLines(kSenseLineC);
	extendedAB = TemplateReadSenseLines();						// xx xA Bx
	extendedAB = (extendedAB >> 1) & 0x3;						// 00 00 AB

	extendedSenseCode = extendedBC | extendedAC | extendedAB;

	return(extendedSenseCode);
}
