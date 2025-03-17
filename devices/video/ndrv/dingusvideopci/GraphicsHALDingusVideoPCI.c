/*
	File:		GraphicsHALTemplate.c

	Contains:	This file contains the items needed to implement the Graphics HAL
				for an arbitrary Template.
				
	The 'Template' graphic hardware is characterized as follows:
	
		Toynbee - Graphics Controller
		This is the arbitrary name of the graphics controller ASIC.
		Toynbee can be placed in a low power mode in which VRAM decays.
		It can support the following resolutions:


			• 512x384 at 60 Hz NTSC	(NTSC Safe Frame)
			• 512x384 at 60 Hz		(12" RGB)
			• 640x480 at 50 Hz PAL	(PAL Safe Frame)
			• 640x480 at 60 Hz NTSC	(NTSC Full Frame)
			• 640x480 at 60 Hz VGA	(Common variant of VGA timing)
			• 640x480 at 67 Hz 		(Standard Macintosh 'high resolution' display)
			• 640x870 at 75 Hz		(Portrait Display)
			• 768x576 at 50 Hz PAL	(PAL Full Frame)
			• 800x600 at 60 Hz VGA	(Common variant of VGA timing)
			• 800x600 at 72 Hz VGA	(Common variant of VGA timing)
			• 800x600 at 75 Hz VGA	(Common variant of VGA timing)
			• 832x624 at 75 Hz		(16" RGB Display)
			• 1024x768 at 60 Hz VGA	(Common variant of VGA timing)
			• 1024x768 at 72 Hz VGA	(Common variant of VGA timing)
			• 1024x768 at 75 Hz VGA	(Common variant of VGA timing)
			• 1024x768 at 75 Hz		(19" RGB)
			• 1152x870 at 75 Hz		(21" RGB)
			• 1280x960 at 75 Hz		(Vesa timing)
			• 1280x1024 at 75 Hz	(Vesa timing)

	
		There is support for hardware cursor, and putting the ASICs into a low power state.
					
		Spur - CLUT/DAC
		This is the arbitrary name of the CLUT/DAC, hereafter merely referred to as a CLUT.
		It is a simple 'triple 8x256' CLUT.  Spur has power saving features.  The PLL clock can
		be turned off and the CLUT can be turned off.  When the CLUT is off, the data decays.
		Spur implements the hardware cursor.
		
		MrSanAntonio - timing generator
		This is the arbitrary name of the timing generator.
		
		The conversion of this Template to a HAL for a specific hardware implementation can be
		started by doing a CASE SENSITIVE replacement of:
		
			"Template" with the "NameOfYourGraphicsArchitectureGoesHere"
			"template" with the "nameOfYourGraphicsArchitectureGoesHere"
			
			"Toynbee" with the "NameOfYourGraphicsControllerGoesHere"
			"toynbee" with the "nameOfYourGraphicsControllerGoesHere"
			
			"Spur" with the "NameOfYourCLUTGoesHere"
			"spur" with the "nameOfYourCLUTGoesHere"

			"MrSanAntonio" with the "NameOfYourTimingGeneratorGoesHere"
			"mrSanAntonio" with the "nameOfYourTimingGeneratorGoesHere"

	Written by:	Kevin Williams

	Copyright:	© 1994-1995 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

		 <2>	 7/17/95	SW		In GraphicsHAL GetModeTiming, now setting 'timingForamt =
		 							kDeclROMtables' prior to any error checking.  Also, in
									GraphicsHAL SetSync(), 'sitBitValidField' is now input only.
		 <1>	 4/15/95	SW		First Checked In
			
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

#include <cmath>
#include "vbe.h"


enum
{
	kHardwareCursorOffset = 16,			// Offset to base address to account for the hw cursor
	kRowBytesOffset = 32				// Offset to add to standard rowbytes, to account for hw cursor
};


enum
{
	kHardwareCursorImageSize = (16 * 16) >> 1,	// Size of hardware cursor image in bytes
	kNumHardwareCursorColors = 8				// Number of colors for hardware cursor
};


/*
//
// ToynbeeRegisters
//	This model reflects the version of the Toynbee cell that is in the Template ASIC.
//
typedef struct ToynbeeRegisters ToynbeeRegisters;
struct ToynbeeRegisters
{
	HWRegister32Bit deleteMe;
	HWRegister32Bit *baseAddress;	
	HWRegister32Bit *rowWords;
	HWRegister32Bit *clockConfiguration;
	HWRegister32Bit *frameBufferConfiguration1;
	HWRegister32Bit *frameBufferConfiguration2;
//	HWRegister32Bit *senseLineEnable;
	HWRegister32Bit *reset;
};
*/

/*
// following bit posiitions (in 68k ordering) describe the "logical registers"
// in Toynbee
enum
{
	kToynbeeFBCTwoMBitVRAMBit = 11,
	kToynbeeFBCFullVRAMBanksBit = 3,
	kToynbeeFBCFullVRAMBanksMask = 1 << kToynbeeFBCFullVRAMBanksBit
};
*/


// following enums tell how much VRAM is in the system
// typedef enum VRAMSize VRAMSize;
typedef enum VRAMSize
{
	k2MegVRAM,
	k4MegVRAM
} VRAMSize; 


//
// MrSanAntonioRegisters
//
typedef struct MrSanAntonioRegisters MrSanAntonioRegisters;
struct MrSanAntonioRegisters
{
	//	HWRegister32Bit *interruptMask;
	//	HWRegister32Bit *clearAnimationInterrupt;
	//	HWRegister32Bit *clearVBLInterrupt;
	//	HWRegister32Bit *cursorLine;
	//	HWRegister32Bit *animateLine;
	//	HWRegister32Bit *horizontalBurstGate;
	//	HWRegister32Bit *horizontalBackPorch;
	//	HWRegister32Bit *timingAdjust;
	HWRegister32Bit		ControlCUR_LINE			;UInt32 padding00[3];	//	HWRegister32Bit *currentLine;
	HWRegister32Bit		ControlVFPEQ			;UInt32 padding01[3];	//	HWRegister32Bit *verticalFrontPorchEqualization;
	HWRegister32Bit		ControlVFP				;UInt32 padding02[3];	//	HWRegister32Bit *verticalFrontPorch;
	HWRegister32Bit		ControlVAL				;UInt32 padding03[3];	//	HWRegister32Bit *verticalActiveLine;
	HWRegister32Bit		ControlVBP				;UInt32 padding04[3];	//	HWRegister32Bit *verticalBackPorch;
	HWRegister32Bit		ControlVBPEQ			;UInt32 padding05[3];	//	HWRegister32Bit *verticalBackPorchEqualization;
	HWRegister32Bit		ControlVSYNC			;UInt32 padding06[3];	//	HWRegister32Bit *verticalSync;
	HWRegister32Bit		ControlVHLINE			;UInt32 padding07[3];	//	HWRegister32Bit *verticalHalfLine;
	HWRegister32Bit		ControlPIPED			;UInt32 padding08[3];	//
	HWRegister32Bit		ControlHPIX				;UInt32 padding09[3];	//	HWRegister32Bit *horiztonalPixelCount;
	HWRegister32Bit		ControlHFP				;UInt32 padding10[3];	//	HWRegister32Bit *horizontalFrontPorch;
	HWRegister32Bit		ControlHAL				;UInt32 padding11[3];	//	HWRegister32Bit *horizontalActiveLine;
	HWRegister32Bit		ControlHBWAY			;UInt32 padding12[3];	//	HWRegister32Bit *horizontalBreezeway;
	HWRegister32Bit		ControlHSP				;UInt32 padding13[3];	//	HWRegister32Bit *horizontalSyncPulse;
	HWRegister32Bit		ControlHEQ				;UInt32 padding14[3];	//	HWRegister32Bit *horizontalEqualization;
	HWRegister32Bit		ControlHLFLN			;UInt32 padding15[3];	//	HWRegister32Bit *halfLine;
	HWRegister32Bit		ControlHSERR			;UInt32 padding16[3];	//	HWRegister32Bit *horizontalSerration;
	HWRegister32Bit		ControlCNTTST			;UInt32 padding17[3];	//	HWRegister32Bit *counterTest;

	HWRegister32Bit		ControlTEST				;UInt32 padding18[3];	//	toynbeeRunning
	HWRegister32Bit		ControlGBASE			;UInt32 padding19[3];
	HWRegister32Bit		ControlROW_WORDS		;UInt32 padding20[3];
	HWRegister32Bit		ControlMON_SENSE		;UInt32 padding21[3];	//	HWRegister32Bit *senseLineEnable; // Toynbee
	HWRegister32Bit		ControlENABLE			;UInt32 padding22[3];	//	HWRegister32Bit *mrSanAntonioMode;
	HWRegister32Bit		ControlGSC_DIVIDE		;UInt32 padding23[3];
	HWRegister32Bit		ControlREFRESH_COUNT	;UInt32 padding24[3];
	HWRegister32Bit		ControlINT_ENABLE		;UInt32 padding25[3];
	HWRegister32Bit		ControlINT_STATUS		;UInt32 padding26[3];	//	HWRegister32Bit *interruptStatus;
};

// following bit posiitions (in 68k ordering) describe the "logical registers"
// in MrSanAntonio
enum
{
/*
	// MrSanAntonioMode register
	kMrSanAntonioEnableBit = 4,		
	kMrSanAntonioEnableMask  = 1 << kMrSanAntonioEnableBit,

	kHActiveHighBit = 8,
	kHActiveHighMask  = 1 << kHActiveHighBit,
	kVActiveHighBit = 9,
	kVActiveHighMask  = 1 << kVActiveHighBit,
	kVHActiveHighMask = kVActiveHighMask | kHActiveHighMask,
	
	kCSyncEnableBit = 1,		
	kCSyncEnableMask  = 1 << kCSyncEnableBit,
	kHSyncEnableBit = 2,		
	kHSyncEnableMask  = 1 << kHSyncEnableBit,
	kVSyncEnableBit = 3,		
	kVSyncEnableMask  = 1 << kVSyncEnableBit,
	kMrSanAntonioSyncsMask = kCSyncEnableMask | kHSyncEnableMask | kVSyncEnableMask,

	kCBlankEnableBit = 9,
	kCBlankEnableMask = 1 << kCBlankEnableBit,
	
	kMrSanAntonioSyncsAndCBlankMask = kMrSanAntonioSyncsMask | kCBlankEnableMask,
	
	// interruptMask register
	kVBLInterruptEnableBit = 3,
	kVBLInterruptEnableMask = 1 << kVBLInterruptEnableBit,
	kAnimateInterruptEnableBit = 7,
	kAnimateInterruptEnableMask = 1 << kAnimateInterruptEnableBit,
	kCursorInterruptEnableBit = 9,
	kCursorInterruptEnableMask = 1 << kCursorInterruptEnableBit,

	// interruptStatus register
	kVBLInterruptStatusBit = 3,
	kVBLInterruptStatusMask = 1 << kVBLInterruptStatusBit,
	kAnimateInterruptStatusBit = 7,
	kAnimateInterruptStatusMask = 1 << kAnimateInterruptStatusBit,
*/
	kCursorInterruptStatusBit = 2,
	kCursorInterruptStatusMask = 1 << kCursorInterruptStatusBit
};


//
// SpurRegisters
//	General description of how Spur is programmed:  Two address lines going into
//	Spur are looked at to determine which external register is accessed.  Call these
//	address lines A[9:8]
//	There are 4 external registers
//	1) address				A[9:8] = 00
//	2) cursorPaletteRAM		A[9:8] = 01		Pallette for hardware cursor
//	3) multiport			A[9:8] = 10
//							generic name for all of the internal registers that are accessed when
//							A[9:8] = 10
//	4) colorPalletteRAM		A[9:8] = 11		CLUT Pallette
//
//	Accessing the registers in the multiport:
//	1) write the 8 bit address of the internal register to address
//	2) read/write the multiport register.  When Spur sees A[9:8] = 10, it looks at the 
//		contents of the address register and accesses the internal register whose internal address
//		matches the contents of the address register
//
//	Accessing the CLUT:
//	Spur is referred to as a "3x256x8" CLUT for the following reasons:
//	3		The CLUT has three channels: red, green and blue.
//	256		The CLUT has 256 physical addresses (0..FF).
//	8		For each channel at a given address, an 8-bit value can be stored.
//	To access a specfic CLUT entry:
//	1) write the 8 bit physical addresses of the CLUT entry to address.  (The CLUT's internal counter
//		resets to grab the red channel whenever the address register is hit.)
//	2) read/write the colorPalletteRAM register.  When Spur sees A[1:0] = 11, it access the 
//		CLUT entry that matches the contents of the address register.  The first read/write to the
//		colorPalletteRAM register reads/writes the red channel, the 2nd read/write to the 
//		colorPalletteRAM reads/writes the blue channel, the 3rd read/write to the colorPalletteRAM
//		reads/writes the green channel.
//		After 3 successive reads/writes to the colorPalletteRAM register, Spur will
//		autoincrement the address register.  The internal counter resets to grab the red channel.
//		Note:  Each time the address register is changed or auto increments, it is necessary to delay
//		to allow the CLUT to fetch the RGB data.  That is why there are DelayForHardware calls around
//		CLUT accesses.
//		
typedef struct SpurRegisters SpurRegisters;
struct SpurRegisters
{
	HWRegister8Bit *address;
	HWRegister8Bit *cursorPaletteRAM;
	HWRegister8Bit *multiPort;					// access port for multiple registers
	HWRegister8Bit *colorPaletteRAM;
};

// following register addresses are for Spurs's internal registers that are accessed through
// the multiport.
enum
{
	kSpurCursorXPositionHigh = 0x10,
	kSpurCursorXPositionLow = 0x11,

	kSpurControl = 0x20,
	kSpur0x21 = 0x21,
	kSpur0x22 = 0x22,
/*
	kSpurPhaseLockLoopControl = 0x66,
	kSpurMSetA = 0x43,
	kSpurPNSetA = 0x41,
	kSpurRISetA = 0x47,
	kSpurMSetB = 0x53,
	kSpurPNSetB = 0x51,
	kSpurRISetB = 0x57
*/
};

// following bit posiitions (in 68k ordering) describe the "logical registers"
// in Spur
enum
{
//	kSpurControl = 0x20,
	kSpurControlCursorEnableBit = 1,
	kSpurControlCursorEnableMask = 1 << kSpurControlCursorEnableBit,
	
//	kSpur0x21 = 0x21,
	kSpur0x21Value0 = 0,
	kSpur0x21Value1 = 1,

//	kSpur0x22 = 0x22,
	kSpur0x22Value0 = 0,

/*
//	kSpurPhaseLockLoopControl = 0x66,
	kSpurPhaseLockLoopControlSetSelectBit = 3,
	kSpurPhaseLockLoopControlSetSelectMask = 1 << kSpurPhaseLockLoopControlSetSelectBit,

	kSpurPhaseLockLoopControlCLKorPLLBit = 1,
	kSpurPhaseLockLoopControlCLKorPLLMask = 1 << kSpurPhaseLockLoopControlCLKorPLLBit,

	kSpurPhaseLockLoopControlCLKorPLLSetMask	= 	kSpurPhaseLockLoopControlCLKorPLLMask |
													kSpurPhaseLockLoopControlSetSelectMask,

	kSpurPhaseLockLoopControlPLLPowerBit = 0,
	kSpurPhaseLockLoopControlPLLPowerMask = 1 << kSpurPhaseLockLoopControlPLLPowerBit,

	kSpurPhaseLockLoopControlDACPowerBit = 2,
	kSpurPhaseLockLoopControlDACPowerMask = 1 << kSpurPhaseLockLoopControlDACPowerBit,

	kSpurPhaseLockLoopControlDACPLLPowerMask	= 	kSpurPhaseLockLoopControlDACPowerMask |
													kSpurPhaseLockLoopControlPLLPowerMask
*/
};


// Default delay for writing the CLUT
enum {
	kDefaultCLUTDelayHigh = 0,
	kDefaultCLUTDelayLow = 128
};



//	An explanation of interrupts for Template:
//	Template contains three interrupt sources: vbl, cursor, and animation.
//	vbl interrupts when enabled, generate an interrupt at every vertical blanking period
//	cusor interrupts when enabled, generate an interrupt when the video line specified in the
//		cursorLine register in mrSanAntonio is drawn.  This is programable
//	animation interrupts when enabled, generate an interrupt when "the line of output video" specified
//		in the animateLine register is drawn.  This is programmable.
//
//	All 3 Template interrupts get funneled to the same bit in the IO controller.  If all 3 Template interrupt
//	sources were enabled, the HAL would need to examine each interrupt source in Template to determine
//	what caused the interrupt.  There is no need to enable all 3 interrupt sources.  The HAL cares
//	about "vbl interrupts" i.e. generate an interrupt when the entire screen has been drawn.  If
//	vbl interrupts are enabled, the interrupt will automatically be generated every time this occurs.
//	However, the cursor interrupts (and probably animation interrupts) can be programmed to go off
//	when the last scan line is drawn.  This occurs slighly before the vbl interrupt would occur and
//	allows more time for "vbl tasks" to run.
//	The HAL uses the cursor interrupt.  The cursorLine register is programmed to be the last scan line
//	for the given displayModeID (height).
//
//	The cursor interrupt is always enabled inside Template.  This allows the driver to determine when 
//	a "vbl" occurs and program the hw at that time.  When interrupts are supposed to be off, the HAL
//	turns of the external interrupts at the controller (using the GraphicOSS functions).
//	This allows the HAL to program hw at the correct time while the system believes interrupts are off.
//



#include "GraphicsHALDingusVideoPCI.h"


DisplayModeInfo	gDisplayModeInfo[] =
{
//                                                                                                                                                                                                                                                                                                               |----------------- 2 MB ------------------||----------------- 4 MB ------------------|
//                                                                                                                                                                                                                  interlaced                                                                nsCLUTAddrRegDelay |    8 bit    |   16 bit    |   32 bit    ||    8 bit    |   16 bit    |   32 bit    |                  
//                                                                                                                                                                                                                     TEST_4   cSyncDisable                                                                   v |GSC-DIVIDE   |GSC-DIVIDE   |GSC-DIVIDE   ||GSC-DIVIDE   |GSC-DIVIDE   |GSC-DIVIDE   |filler       
//                                                                                                                                                                                                         P2Mux          hSyncPol HSERR     HEQ      HBWAY    HFP       VHLINE      VBPEQ   VAL                 |   PIPED     |   PIPED     |   PIPED     ||   PIPED     |   PIPED     |   PIPED     |   filler    
//                                                                                                                                           2 MB max      4 MB max    width height   hertz       N2   D2        notInterlac vSyncPol   HLFLN    HSP      HAL       HPIX       VSYNC     VBP     VFP   VFPEQ     |        spur |        spur |        spur ||        spur |        spur |        spur |      filler 
//                                                                                                                                                                                                                                                                                                               |             |             |             ||             |             |             |
	100, kDMSModeReady, kDisplay512x384At60HzNTSC,          kDisplay512x384At60HzNTSC, timingAppleNTSC_ST,        0x003BF080 /* 59.94 Hz */, kDepthMode3, kDepthMode3,  512,  384, /*  60 NTSC */  47, 30,    1, 0, 1, 0, 0, 0, 0, 181, 195, 15, 389, 28,  86, 342, 388,  525,  523,  4, 43, 82,  466,  495, 512, 2,  77, 0x10, 1,  83, 0x14, 0,  85, 0x18,  3,  73, 0x20, 2,  81, 0x24, 1,  85, 0x28, 0, 0, 0,
	101, kDMSModeReady, kDisplay512x384At60Hz,              kDisplay512x384At60Hz,     timingApple_512x384_60hz,  0x003C0000 /* 60 Hz */,    kDepthMode3, kDepthMode3,  512,  384, /*  60      */  27, 14, 0x62, 1, 0, 0, 0, 0, 0, 304, 160,  8, 319, 15,  49, 305, 318,  814,  812,  4, 23, 42,  810,  811, 512, 2,  40, 0x10, 1,  46, 0x14, 0,  48, 0x18,  3,  36, 0x20, 2,  44, 0x24, 1,  48, 0x28, 0, 0, 0,
	102, kDMSModeReady, kDisplay640x480At50HzPAL,           kDisplay640x480At50HzPAL,  timingApplePAL_ST,         0x00320000 /* 50 Hz */,    kDepthMode3, kDepthMode3,  640,  480, /*  50 PAL  */  66, 35,    1, 0, 1, 0, 0, 0, 0, 219, 236, 18, 471, 34, 102, 422, 470,  625,  623,  3, 45, 86,  566,  595, 512, 2,  93, 0x10, 1,  99, 0x14, 0, 101, 0x18,  3,  89, 0x20, 2,  97, 0x24, 1, 101, 0x28, 0, 0, 0,
	103, kDMSModeReady, kDisplay640x480At60HzNTSC,          kDisplay640x480At60HzNTSC, timingAppleNTSC_FF,        0x003BF080 /* 59.94 Hz */, kDepthMode3, kDepthMode3,  640,  480, /*  60 NTSC */  47, 30,    1, 0, 1, 0, 0, 0, 0, 181, 195, 15, 389, 28,  54, 374, 388,  525,  524,  4,  9, 34,  514,  519, 512, 2,  45, 0x10, 1,  51, 0x14, 0,  53, 0x18,  3,  41, 0x20, 2,  49, 0x24, 1,  53, 0x28, 0, 0, 0,
	104, kDMSModeReady, kDisplay640x480At60HzVGA,           kDisplay640x480At60HzVGA,  timingVESA_640x480_60hz,   0x003BF080 /* 59.94 Hz */, kDepthMode3, kDepthMode3,  640,  480, /*  60 VGA  */  37, 23,    2, 1, 0, 0, 0, 0, 0, 352, 200, 24, 399, 47,  65, 385, 398, 1050, 1048,  2, 34, 66, 1026, 1037, 256, 2,  56, 0x10, 1,  62, 0x14, 0,  64, 0x18,  3,  52, 0x20, 2,  60, 0x24, 1,  64, 0x28, 0, 0, 0,
	105, kDMSModeReady, kDisplay640x480At67Hz,              kDisplay640x480At67Hz,     timingApple_640x480_67hz,  0x0042AA80 /* 66.67 Hz */, kDepthMode3, kDepthMode3,  640,  480, /*  67      */  27, 14,    2, 1, 0, 0, 0, 0, 0, 400, 216, 16, 431, 31,  73, 393, 430, 1050, 1048,  4, 43, 82, 1042, 1045, 128, 2,  64, 0x10, 1,  70, 0x14, 0,  72, 0x18,  3,  60, 0x20, 2,  68, 0x24, 1,  72, 0x28, 0, 0, 0,
	106, kDMSModeReady, kDisplay640x480At120Hz,             kDisplay640x480At120Hz,    timingGTF_640x480_120hz,   0x00780000 /* 120 Hz */,   kDepthMode3, kDepthMode3,  640,  480, /* 120      */ 127, 76,    3, 1, 0, 0, 0, 1, 1, 392, 212, 16, 423, 31,  77, 397, 422, 1030, 1028,  4, 35, 66, 1026, 1027, 128, 2,  68, 0x10, 1,  74, 0x14, 0,  76, 0x18,  3,  64, 0x20, 2,  72, 0x24, 1,  76, 0x28, 0, 0, 0,
	107, kDMSModeReady, kDisplay640x870At75Hz,              kDisplay640x870At75Hz,     timingApple_640x870_75hz,  0x004B0000 /* 75 Hz */,    kDepthMode2, kDepthMode3,  640,  870, /*  75      */  42, 23,    3, 1, 0, 0, 0, 0, 0, 376, 208, 20, 415, 39,  73, 393, 414, 1836, 1834,  4, 46, 88, 1828, 1831, 128, 2,  64, 0x10, 1,  70, 0x14, 0,   0,    0,  3,  60, 0x20, 2,  68, 0x24, 1,  72, 0x28, 0, 0, 0,
	108, kDMSModeReady, kDisplay768x576At50HzPAL,           kDisplay768x576At50HzPAL,  timingApplePAL_FF,         0x00320000 /* 50 Hz */,    kDepthMode3, kDepthMode3,  768,  576, /*  50 PAL  */  66, 35,    1, 0, 1, 0, 0, 0, 0, 219, 236, 18, 471, 34,  70, 454, 470,  625,  623,  3, 21, 38,  614,  619, 512, 2,  61, 0x10, 1,  67, 0x14, 0,  69, 0x18,  3,  57, 0x20, 2,  65, 0x24, 1,  69, 0x28, 0, 0, 0,
	109, kDMSModeReady, kDisplay800x600At60HzVGA,           kDisplay800x600At60HzVGA,  timingVESA_800x600_60hz,   0x003C0000 /* 60 Hz */,    kDepthMode3, kDepthMode3,  800,  600, /*  60 VGA  */  28, 11,    2, 1, 0, 0, 1, 1, 1, 464, 264, 32, 527, 63, 101, 501, 526, 1256, 1254,  6, 29, 52, 1252, 1253, 128, 2,  92, 0x10, 1,  98, 0x14, 0, 100, 0x18,  3,  88, 0x20, 2,  96, 0x24, 1, 100, 0x28, 0, 0, 0,
	110, kDMSModeReady, kDisplay800x600At72HzVGA,           kDisplay800x600At72HzVGA,  timingVESA_800x600_72hz,   0x00480000 /* 72 Hz */,    kDepthMode3, kDepthMode3,  800,  600, /*  72 VGA  */  27, 14, 0x53, 1, 0, 0, 1, 1, 1, 460, 260, 30, 519, 59,  85, 485, 518, 1332, 1330, 10, 33, 56, 1256, 1293, 128, 2,  76, 0x10, 1,  82, 0x14, 0,  84, 0x18,  3,  72, 0x20, 2,  80, 0x24, 1,  84, 0x28, 0, 0, 0,
	111, kDMSModeReady, kDisplay800x600At75HzVGA,           kDisplay800x600At75HzVGA,  timingVESA_800x600_75hz,   0x004b0000 /* 75 Hz */,    kDepthMode3, kDepthMode3,  800,  600, /*  75 VGA  */  22,  7,    2, 1, 0, 0, 1, 1, 1, 488, 264, 20, 527, 39, 113, 513, 526, 1250, 1248,  4, 25, 46, 1246, 1247, 128, 2, 104, 0x10, 1, 110, 0x14, 0, 112, 0x18,  3, 100, 0x20, 2, 108, 0x24, 1, 112, 0x28, 0, 0, 0,
	112, kDMSModeReady, kDisplay832x624At75Hz,              kDisplay832x624At75Hz,     timingApple_832x624_75hz,  0x004B0000 /* 75 Hz */,    kDepthMode3, kDepthMode3,  832,  624, /*  75      */  42, 23,    3, 1, 0, 0, 0, 0, 0, 544, 288, 16, 575, 31, 137, 553, 574, 1334, 1332,  4, 43, 82, 1330, 1331, 128, 2, 128, 0x10, 1, 134, 0x14, 0, 136, 0x18,  3, 124, 0x20, 2, 132, 0x24, 1, 136, 0x28, 0, 0, 0,
	113, kDMSModeReady, kDisplay1024x768At60HzVGA,          kDisplay1024x768At60HzVGA, timingVESA_1024x768_60hz,  0x003C0000 /* 60 Hz */,    kDepthMode2, kDepthMode3, 1024,  768, /*  60 VGA  */  31, 15,    3, 1, 0, 0, 0, 0, 1, 604, 336, 34, 671, 67, 141, 653, 670, 1612, 1610, 10, 39, 68, 1604, 1607, 128, 2, 132, 0x10, 1, 138, 0x14, 0,   0,    0,  3, 128, 0x20, 2, 136, 0x24, 1, 140, 0x28, 0, 0, 0,
	114, kDMSModeReady, kDisplay1024x768At75HzVGA,          kDisplay1024x768At75HzVGA, timingVESA_1024x768_75hz,  0x004B0000 /* 75 Hz */,    kDepthMode2, kDepthMode3, 1024,  768, /*  75 VGA  */  78, 31,    3, 1, 0, 0, 1, 1, 1, 608, 328, 24, 655, 47, 129, 641, 654, 1600, 1598,  4, 32, 60, 1596, 1597, 128, 2, 120, 0x10, 1, 126, 0x14, 0,   0,    0,  3, 116, 0x20, 2, 124, 0x24, 1, 128, 0x28, 0, 0, 0,
	115, kDMSModeReady, kDisplay1024x768At75Hz,             kDisplay1024x768At75Hz,    timingApple_1024x768_75hz, 0x004B0000 /* 75 Hz */,    kDepthMode2, kDepthMode3, 1024,  768, /*  75      */  28, 11,    3, 1, 0, 0, 0, 0, 0, 616, 332, 24, 663, 47, 129, 641, 662, 1608, 1606,  4, 34, 64, 1600, 1603, 128, 2, 120, 0x10, 1, 126, 0x14, 0,   0,    0,  3, 116, 0x20, 2, 124, 0x24, 1, 128, 0x28, 0, 0, 0,
	116, kDMSModeReady, kDisplay1152x870At75Hz,             kDisplay1152x870At75Hz,    timingApple_1152x870_75hz, 0x004B0000 /* 75 Hz */,    kDepthMode2, kDepthMode3, 1152,  870, /*  75      */  61, 19,    3, 1, 0, 0, 0, 0, 0, 664, 364, 32, 727, 63, 129, 705, 726, 1830, 1828,  4, 43, 82, 1822, 1825, 128, 2, 120, 0x10, 1, 126, 0x14, 0,   0,    0,  3, 116, 0x20, 2, 124, 0x24, 1, 128, 0x28, 0, 0, 0,
	117, kDMSModeReady, kDisplay1280x960At75Hz,             kDisplay1280x960At75Hz,    timingVESA_1280x960_75hz,  0x004B0000 /* 75 Hz */,    kDepthMode1, kDepthMode2, 1280,  960, /*  75      */ 125, 31,    3, 1, 0, 0, 1, 1, 0, 384, 210, 18, 419, 35,  89, 409, 418, 2000, 1998,  4, 40, 76, 1996, 1997, 128, 1,  86, 0x50, 0,   0,    0, 0,   0,    0,  2,  84, 0x60, 1,  88, 0x64, 0,   0,    0, 0, 0, 0,
	118, kDMSModeReady, kDisplay1280x1024At75Hz,            kDisplay1280x1024At75Hz,   timingVESA_1280x1024_75hz, 0x004B0000 /* 75 Hz */,    kDepthMode1, kDepthMode2, 1280, 1024, /*  75      */  56, 13,    3, 1, 0, 0, 1, 1, 0, 386, 211, 18, 421, 35,  91, 411, 420, 2132, 2130,  4, 42, 80, 2128, 2129, 128, 1,  88, 0x50, 0,   0,    0, 0,   0,    0,  2,  86, 0x60, 1,  90, 0x64, 0,   0,    0, 0, 0, 0,
// ••• If you add any more pre programmed modes here then change kFirstProgrammableModeInfo below. Don’t exceed kFirstProgrammableDisplayMode.
	0,   kDMSModeFree,  kFirstProgrammableDisplayMode +  0, kDisplayModeIDInvalid,     0,                         0,                         0,           0,              0,    0,                  0,  0,    0, 0, 0, 0, 0, 0, 0,   0,   0,  0,   0,  0,   0,   0,   0,    0,    0,  0,  0,  0,    0,    0,   0, 0,   0,    0, 0,   0,    0, 0,   0,    0,  0,   0,    0, 0,   0,    0, 0,   0,    0, 0, 0, 0,
	0,   kDMSModeFree,  kFirstProgrammableDisplayMode +  1, kDisplayModeIDInvalid,     0,                         0,                         0,           0,              0,    0,                  0,  0,    0, 0, 0, 0, 0, 0, 0,   0,   0,  0,   0,  0,   0,   0,   0,    0,    0,  0,  0,  0,    0,    0,   0, 0,   0,    0, 0,   0,    0, 0,   0,    0,  0,   0,    0, 0,   0,    0, 0,   0,    0, 0, 0, 0,
	0,   kDMSModeFree,  kFirstProgrammableDisplayMode +  2, kDisplayModeIDInvalid,     0,                         0,                         0,           0,              0,    0,                  0,  0,    0, 0, 0, 0, 0, 0, 0,   0,   0,  0,   0,  0,   0,   0,   0,    0,    0,  0,  0,  0,    0,    0,   0, 0,   0,    0, 0,   0,    0, 0,   0,    0,  0,   0,    0, 0,   0,    0, 0,   0,    0, 0, 0, 0,
	0,   kDMSModeFree,  kFirstProgrammableDisplayMode +  3, kDisplayModeIDInvalid,     0,                         0,                         0,           0,              0,    0,                  0,  0,    0, 0, 0, 0, 0, 0, 0,   0,   0,  0,   0,  0,   0,   0,   0,    0,    0,  0,  0,  0,    0,    0,   0, 0,   0,    0, 0,   0,    0, 0,   0,    0,  0,   0,    0, 0,   0,    0, 0,   0,    0, 0, 0, 0,
	0,   kDMSModeFree,  kFirstProgrammableDisplayMode +  4, kDisplayModeIDInvalid,     0,                         0,                         0,           0,              0,    0,                  0,  0,    0, 0, 0, 0, 0, 0, 0,   0,   0,  0,   0,  0,   0,   0,   0,    0,    0,  0,  0,  0,    0,    0,   0, 0,   0,    0, 0,   0,    0, 0,   0,    0,  0,   0,    0, 0,   0,    0, 0,   0,    0, 0, 0, 0,
	0,   kDMSModeFree,  kFirstProgrammableDisplayMode +  5, kDisplayModeIDInvalid,     0,                         0,                         0,           0,              0,    0,                  0,  0,    0, 0, 0, 0, 0, 0, 0,   0,   0,  0,   0,  0,   0,   0,   0,    0,    0,  0,  0,  0,    0,    0,   0, 0,   0,    0, 0,   0,    0, 0,   0,    0,  0,   0,    0, 0,   0,    0, 0,   0,    0, 0, 0, 0,
	0,   kDMSModeFree,  kFirstProgrammableDisplayMode +  6, kDisplayModeIDInvalid,     0,                         0,                         0,           0,              0,    0,                  0,  0,    0, 0, 0, 0, 0, 0, 0,   0,   0,  0,   0,  0,   0,   0,   0,    0,    0,  0,  0,  0,    0,    0,   0, 0,   0,    0, 0,   0,    0, 0,   0,    0,  0,   0,    0, 0,   0,    0, 0,   0,    0, 0, 0, 0,
	0,   kDMSModeFree,  kFirstProgrammableDisplayMode +  7, kDisplayModeIDInvalid,     0,                         0,                         0,           0,              0,    0,                  0,  0,    0, 0, 0, 0, 0, 0, 0,   0,   0,  0,   0,  0,   0,   0,   0,    0,    0,  0,  0,  0,    0,    0,   0, 0,   0,    0, 0,   0,    0, 0,   0,    0,  0,   0,    0, 0,   0,    0, 0,   0,    0, 0, 0, 0,
	0,   kDMSModeFree,  kFirstProgrammableDisplayMode +  8, kDisplayModeIDInvalid,     0,                         0,                         0,           0,              0,    0,                  0,  0,    0, 0, 0, 0, 0, 0, 0,   0,   0,  0,   0,  0,   0,   0,   0,    0,    0,  0,  0,  0,    0,    0,   0, 0,   0,    0, 0,   0,    0, 0,   0,    0,  0,   0,    0, 0,   0,    0, 0,   0,    0, 0, 0, 0,
	0,   kDMSModeFree,  kFirstProgrammableDisplayMode +  9, kDisplayModeIDInvalid,     0,                         0,                         0,           0,              0,    0,                  0,  0,    0, 0, 0, 0, 0, 0, 0,   0,   0,  0,   0,  0,   0,   0,   0,    0,    0,  0,  0,  0,    0,    0,   0, 0,   0,    0, 0,   0,    0, 0,   0,    0,  0,   0,    0, 0,   0,    0, 0,   0,    0, 0, 0, 0,
	0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 10, kDisplayModeIDInvalid,     0,                         0,                         0,           0,              0,    0,                  0,  0,    0, 0, 0, 0, 0, 0, 0,   0,   0,  0,   0,  0,   0,   0,   0,    0,    0,  0,  0,  0,    0,    0,   0, 0,   0,    0, 0,   0,    0, 0,   0,    0,  0,   0,    0, 0,   0,    0, 0,   0,    0, 0, 0, 0,
	0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 11, kDisplayModeIDInvalid,     0,                         0,                         0,           0,              0,    0,                  0,  0,    0, 0, 0, 0, 0, 0, 0,   0,   0,  0,   0,  0,   0,   0,   0,    0,    0,  0,  0,  0,    0,    0,   0, 0,   0,    0, 0,   0,    0, 0,   0,    0,  0,   0,    0, 0,   0,    0, 0,   0,    0, 0, 0, 0,
	0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 12, kDisplayModeIDInvalid,     0,                         0,                         0,           0,              0,    0,                  0,  0,    0, 0, 0, 0, 0, 0, 0,   0,   0,  0,   0,  0,   0,   0,   0,    0,    0,  0,  0,  0,    0,    0,   0, 0,   0,    0, 0,   0,    0, 0,   0,    0,  0,   0,    0, 0,   0,    0, 0,   0,    0, 0, 0, 0,
	0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 13, kDisplayModeIDInvalid,     0,                         0,                         0,           0,              0,    0,                  0,  0,    0, 0, 0, 0, 0, 0, 0,   0,   0,  0,   0,  0,   0,   0,   0,    0,    0,  0,  0,  0,    0,    0,   0, 0,   0,    0, 0,   0,    0, 0,   0,    0,  0,   0,    0, 0,   0,    0, 0,   0,    0, 0, 0, 0,
	0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 14, kDisplayModeIDInvalid,     0,                         0,                         0,           0,              0,    0,                  0,  0,    0, 0, 0, 0, 0, 0, 0,   0,   0,  0,   0,  0,   0,   0,   0,    0,    0,  0,  0,  0,    0,    0,   0, 0,   0,    0, 0,   0,    0, 0,   0,    0,  0,   0,    0, 0,   0,    0, 0,   0,    0, 0, 0, 0,
	0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 15, kDisplayModeIDInvalid,     0,                         0,                         0,           0,              0,    0,                  0,  0,    0, 0, 0, 0, 0, 0, 0,   0,   0,  0,   0,  0,   0,   0,   0,    0,    0,  0,  0,  0,    0,    0,   0, 0,   0,    0, 0,   0,    0, 0,   0,    0,  0,   0,    0, 0,   0,    0, 0,   0,    0, 0, 0, 0,
	0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 16, kDisplayModeIDInvalid,     0,                         0,                         0,           0,              0,    0,                  0,  0,    0, 0, 0, 0, 0, 0, 0,   0,   0,  0,   0,  0,   0,   0,   0,    0,    0,  0,  0,  0,    0,    0,   0, 0,   0,    0, 0,   0,    0, 0,   0,    0,  0,   0,    0, 0,   0,    0, 0,   0,    0, 0, 0, 0,
	0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 17, kDisplayModeIDInvalid,     0,                         0,                         0,           0,              0,    0,                  0,  0,    0, 0, 0, 0, 0, 0, 0,   0,   0,  0,   0,  0,   0,   0,   0,    0,    0,  0,  0,  0,    0,    0,   0, 0,   0,    0, 0,   0,    0, 0,   0,    0,  0,   0,    0, 0,   0,    0, 0,   0,    0, 0, 0, 0,
	0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 18, kDisplayModeIDInvalid,     0,                         0,                         0,           0,              0,    0,                  0,  0,    0, 0, 0, 0, 0, 0, 0,   0,   0,  0,   0,  0,   0,   0,   0,    0,    0,  0,  0,  0,    0,    0,   0, 0,   0,    0, 0,   0,    0, 0,   0,    0,  0,   0,    0, 0,   0,    0, 0,   0,    0, 0, 0, 0,
	0,   kDMSModeFree,  kFirstProgrammableDisplayMode + 19, kDisplayModeIDInvalid,     0,                         0,                         0,           0,              0,    0,                  0,  0,    0, 0, 0, 0, 0, 0, 0,   0,   0,  0,   0,  0,   0,   0,   0,    0,    0,  0,  0,  0,    0,    0,   0, 0,   0,    0, 0,   0,    0, 0,   0,    0,  0,   0,    0, 0,   0,    0, 0,   0,    0, 0, 0, 0,
};
const int kNumModeInfos = sizeof(gDisplayModeInfo) / sizeof(DisplayModeInfo);
const int kFirstProgrammableModeInfo = 19;

typedef enum RegFieldControl {
	/* see comments in GraphicsHALInitPrivateData for info */
	kRegFieldControlCUR_LINE,
	kRegFieldControlVFPEQ,
	kRegFieldControlVFP,
	kRegFieldControlVAL,
	kRegFieldControlVBP,
	kRegFieldControlVBPEQ,
	kRegFieldControlVSYNC,
	kRegFieldControlVHLINE,
	kRegFieldControlPIPED,
	kRegFieldControlHPIX,
	kRegFieldControlHFP,
	kRegFieldControlHAL,
	kRegFieldControlHBWAY,
	kRegFieldControlHSP,
	kRegFieldControlHEQ,
	kRegFieldControlHLFLN,
	kRegFieldControlHSERR,
	kRegFieldControlCNTTST,
	kRegFieldControlTEST_All,
	kRegFieldControlTEST_1_DisableTiming,
	kRegFieldControlTEST_2,
	kRegFieldControlInterlaced, // TEST_3
	kRegFieldControlTEST_4,
	kRegFieldControlHSyncPolarity, // TEST_5
	kRegFieldControlTEST_6,
	kRegFieldControlTEST_7,
	kRegFieldControlTEST_8_ResetTiming,
	kRegFieldControlVSyncPolarity, // TEST_9
	kRegFieldControlTEST_10,
	kRegFieldControlTEST_11,
	kRegFieldControlGBASE,
	kRegFieldControlROW_WORDS,
	kRegFieldControlMON_SENSE_All,
	kRegFieldControlMON_SENSE_1,
	kRegFieldControlMON_SENSE_2,
	kRegFieldControlMON_SENSE_3,
	kRegFieldControlENABLE_All,
	kRegFieldControlCBlankDisable, // ENABLE_1
	kRegFieldControlCSyncDisable, // ENABLE_2
	kRegFieldControlHSyncDisable, // ENABLE_3
	kRegFieldControlVSyncDisable, // ENABLE_4
	kRegFieldControl50or33MHz,
	kRegFieldControlWide,
	kRegFieldControlDetectPageHits,
	kRegFieldControlShiftClock,
	kRegFieldControlStandardBankDisable,
	kRegFieldControlDoubleBufferingEnable, // ENABLE_10
	kRegFieldControlLittleEndian,
	kRegFieldControlNotInterlaced, // ENABLE_12
	kRegFieldControlGSC_DIVIDE,
	kRegFieldControlREFRESH_COUNT,
	kRegFieldControlINT_ENABLE_All,
	kRegFieldControlINT_ENABLE_1,
	kRegFieldControlINT_ENABLE_2,
	kRegFieldControlINT_ENABLE_3,
	kRegFieldControlINT_ENABLE_4,
	kRegFieldControlINT_STATUS_All,
	kRegFieldControlINT_STATUS_1,
	kRegFieldControlINT_STATUS_2,
	kRegFieldControlINT_STATUS_3,
	kNumRegFieldsControl
} RegFieldControl;

typedef struct ControlRegSpec {
	UInt16 controlAddressOffset;
	UInt16 bitFieldSize;
	UInt16 bitFieldStart;
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

typedef UInt8 SpurCursorImage[kHardwareCursorImageSize];
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
	RegEntryID regEntryID;						// save our RegEntryID in case we need anything
	RegEntryID regEntryID_sixty6;
	Ptr baseAddressPageCurrent;
	Ptr baseAddressPage0;						// current FrameBufferBaseAddress of VRAM reported QD
	Ptr baseAddressPage1;						// current FrameBufferBaseAddress of VRAM reported QD
	MrSanAntonioRegisters *mrSanAntonio;		// description of MrSanAntonio registers
	UInt32 vramBaseAddress;						// VRAM base address read from Base Register 1
	HWRegister32Bit*					senseLineEnable;
	DisplayModeID displayModeID;				// current displayModeID
	AbsoluteTime absCLUTAddrRegDelay;			// 800ns in absolute time used for hitting clut
	AbsoluteTime						senseLineAndVideoDelay5ms;
	SpurRegisters spur;							// Addresses of Spur registers
	UInt32								vramUsageMode; // something to do with available banks and pages and sixty6 (4 modes: 0,1,2,3)
	RGB savedClut[256];							// save the clut if DAC and PLL are powered down
	DisplayCode displayCode;					// class of the connected display
	DepthMode depthMode;						// current depthMode
	UInt16								currentPage;
	SInt16 width;								// current width for displayModeId
	SInt16 height;								// current height displayModeId
	UInt16 rowBytes;							// current rowbytes for displayModeId and depthMode
	UInt16 cvhSyncDisabled;						// c,v,h Bits if set, sync is disabled
	UInt16								numPages;
	SInt16								startPosition;
	SInt16								endPosition;
	UInt16					unused1;
	VRAMSize vramSize;							// Amount of VRAM 1, 2 or 4 megs
	UInt8					unused2;
	UInt16					unused3;
	UInt16					unused4;
	Boolean								interlaced;
	Boolean								fVRAMBank1;
	Boolean								fVRAMBank2;
	Boolean								hasSixty6;
	Boolean								hasDeaconb;
	Boolean								hasFatman;			
	Boolean monoOnly;							// True if attached display only support Monochrome
	Boolean								compositSyncDisabled;				
	Boolean								setClutAtVBL;
	Boolean								clutBusy;
	Boolean								setClutEntriesPending;
	Boolean								setCursorClutEntriesPending;
	Boolean								cursorClutTransformed;
	Boolean								usingCustomClutDelay;
	Boolean								isDDCC;
	Boolean								hardwareIsProgrammed;
	Boolean								needsEnableCBlank;

	Boolean								supports640x480At60Hz;
	Boolean								supports640x480At67Hz;
	Boolean								supports800x600At60Hz;
	Boolean								supports800x600At72Hz;
	Boolean								supports800x600At75Hz;
	Boolean								supports832x624At75Hz;
	Boolean								supports1024x768At60Hz;
	Boolean								supports1024x768At70Hz;
	Boolean								supports1024x768At75Hz;
	Boolean								supports1152x870At75Hz;
	Boolean								supports1280x1024At75Hz;

	UInt8								ddcChecksum;

	RawSenseCode						rawSenseCode;
	ExtendedSenseCode					extendedSenseCode;

	Boolean								monitorIsBlanked;
	ControlRegSpec						regSpecs[kNumRegFieldsControl];
	UInt16								filler;
	SpurHardwareCursorData hardwareCursorData;	// Record of data for hardware cursor
/*
	ToynbeeRegisters toynbee;					// description of Toynbee registers
	UInt32 vdPowerState;						// current state of hw...on, off..etc
	Boolean usingClockSetA;						// 'true' if using Set A of the Frankenstien clock
	Boolean vramWidth32;						// 'true' if vram width is 32 bits wide
	Boolean clutOff;							// 'true' if DAC and PLL are powered down
	Boolean toynbeeRunning;						// 'true' mrSanAntonio is on and interrupts are firing
*/
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
	Ptr					baseAddressPage0;
	Ptr					baseAddressPage1;
	DisplayModeID displayModeID;
	UInt32				vramUsageMode;
	DepthMode depthMode;
	UInt16				currentPage;
	SInt16				width;
	SInt16				height;
	UInt32				filler1;

	DisplayCode displayCode;		// class of the connected display
	UInt16 cvhSyncDisabled;			// c,v,h Bits if set, sync is disabled
	UInt16				numPages;
	Boolean				interlaced;
	Boolean				fVRAMBank1;
	Boolean				fVRAMBank2;
	Boolean monoOnly;				// True if attached display only support Monochrome
	Boolean				compositSyncDisabled;
	Boolean				filler2;
	short				filler3;
/*
	Ptr baseAddress;				// current FrameBufferBaseAddress...I hate using ptr
	UInt32 vdPowerState;			// current state of hw...on, off..etc
	VRAMSize vramSize;				// Amount of VRAM 1, 2 or 4 megs
	SInt16 width;					// current width for displayModeId
	SInt16 height;					// current height displayModeId
	UInt16 rowBytes;				// current rowbytes for displayModeId and depthMode
	Boolean vramWidth32;			// 'true' if vram width is 32 bits wide
	Boolean clutOff;				// 'true' if DAC and PLL are powered down
	Boolean toynbeeRunning;			// 'true' swatch is on and interrupts are firing
*/
};

// enum { kDepthModeInvalid = kDepthMode1 -1 };	// invalid depthMode is ALWAYS less than all valid depthModes

TemplateHALData* GraphicsHALGetHALData(void);

// Naming conventions for functions:
//		• GraphicsHALxxx	- functions which all HALs must implement.  These have external scope
//		• {Template}xxx	- functions which are strictly private to a specific HAL



static GDXErr TemplateMapDepthModeToCLUTAttributes(DepthMode depthMode,
				UInt32 *startAddress, UInt32 *entryOffset);

// This routine handles interrupts and is passed back as handler in GraphicsHAL GetVBLInterruptRoutines
static VBLHandler TemplateClearInternalVBLInterrupts;

static void TemplateWaitForVBL(void);
static GDXErr TemplateAssertVideoReset(void);

static GDXErr TemplateSetupClockGenerator(BitDepthIndependentData* bdiData);

static GDXErr TemplateSetupCLUT(const SpurRegisters *spur, const WidthAndDepthDependentData *vwdData);

static GDXErr TemplateSetupFBController_2(DepthMode depthMode, const DisplayInfo *info, const WidthAndDepthDependentData *vwdData);
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


//••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••



static GDXErr TemplateGetDisplayData(Boolean ignoreNotReady, DisplayModeID displayModeID, DepthMode depthMode, VRAMSize vramSize,
	BitDepthIndependentData* bdiData, WidthAndDepthDependentData* vwdData, DisplayInfo* info);	

static void DeferredMoveHardwareCursor(void);


// Here is typedef for a function used to retrieve BitDepthIndependentData & BitDepthDependentData
typedef GDXErr GetBDIAndBDDDataFunction(short index, DepthMode depthMode, VRAMSize vramSize,
	BitDepthIndependentData* bdiData, WidthAndDepthDependentData* vwdData, DisplayInfo* info);

static GetBDIAndBDDDataFunction TemplateGet;



// HAL global, persistant data is stored in gTemplateHALData
TemplateHALData	gTemplateHALData;	// Persistant Template specific data storage


// The version of Template Graphics driver
enum
{
	kmajorRev = 1, 
	kminorAndBugRev = 0x05,
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
		"\pcontrol",										// device name must match in Devicetree
		kmajorRev, kminorAndBugRev, kstage, knonRelRev,		// Major, Minor, Stage, Rev
	},

	
	// driverOSRuntimeInfo								// OS Runtime Requirements of Driver
		{
			kDriverIsOpenedUponLoad + kDriverIsUnderExpertControl,	// Runtime Options
			"\p.Display_Video_Apple_Control",
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
// Patches to fix OS X compatibility problems
//
//=====================================================================================================

#if 1 // isForMacOSX

static OSStatus GetPCICardBaseAddress( RegEntryID *theID, UInt32 *baseRegAddress,
								UInt8 offsetValue, UInt32 *spaceAllocated )
{
	OSStatus				osStatus;
	PCIAssignedAddress		assignedArray[2];
	UInt32					virtualArray[2];
	RegPropertyValueSize	propertySize;
	UInt32					numberOfElements;
	Boolean					foundMatch;
	UInt16					index;

	*baseRegAddress = NULL;
	foundMatch = false;

	propertySize = sizeof(assignedArray);
	osStatus = RegistryPropertyGet(theID, kPCIAssignedAddressProperty, assignedArray, &propertySize);

	if ((osStatus == kOTNoError) && propertySize)
	{
		numberOfElements = propertySize/sizeof(PCIAssignedAddress);

		propertySize = sizeof(virtualArray);
		osStatus = RegistryPropertyGet(theID, kAAPLDeviceLogicalAddress, virtualArray, &propertySize);

		if ((osStatus == kOTNoError) && propertySize)
		{
			/* search through the assigned addresses property looking for base register */

			for (index = 0; (index != numberOfElements) && !foundMatch; ++index)
			{
				if (assignedArray[index].registerNumber == offsetValue)
				{
					if (spaceAllocated)
					{
						*spaceAllocated = assignedArray[index].size.lo;
					}
					*baseRegAddress = virtualArray[index];
					foundMatch = true;
				}
			}
		}
		else
			osStatus = kENXIOErr;

	}
	else
		osStatus = kENXIOErr;

	return osStatus;
}


// this routine will get logical addresses
static OSErr MyExpMgrConfigReadLong(
	RegEntryIDPtr		node,
	LogicalAddress		configAddr,
	UInt32 *			valuePtr)
{
	return GetPCICardBaseAddress( node, valuePtr, (UInt8)configAddr, NULL );
}

#else

// this routine only gets physical addresses which equal logical addresses only in Mac OS 9 and early versions of Mac OS X (≤ Jaguar)
#define MyExpMgrConfigReadLong ExpMgrConfigReadLong

#endif



//=====================================================================================================
//
// IsSixty6DisplayConnected()
//
//=====================================================================================================
static Boolean IsSixty6DisplayConnected(void)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	UInt32 gcAddressProperty;
	RegPropertyValueSize gcAddressPropertySize = sizeof(gcAddressProperty);
	Boolean hasSixty6 = true;
	RegEntryID regEntryID_gc;
	FindNamedRegEntry("gc", &regEntryID_gc);
	RegistryPropertyGet(&regEntryID_gc, "AAPL,address", &gcAddressProperty, &gcAddressPropertySize);
	if (!templateHALData->hasFatman)
	{
		UInt16 regValue = *(UInt16*)(gcAddressProperty + 0x1E000);
		if ((regValue & 0x0C000) == 0x0C000)
			hasSixty6 = false;
	}
	else
	{
		UInt32 regValue = *(UInt32*)(gcAddressProperty + 0x34);
		if ((regValue & 0x300000) == 0x300000)
			hasSixty6 = false;
	}
	RegistryEntryIDDispose(&regEntryID_gc);
	return hasSixty6;
}



//=====================================================================================================
//
// DoInitOneControlRegField()
//
//=====================================================================================================
static void DoInitOneControlRegField( UInt16 logicalRegNdx, UInt16 controlAddressOffset, UInt16 bitFieldSize, UInt16 bitFieldStart, Boolean isBitField, ControlRegSpec* regSpecs)
{
	regSpecs[logicalRegNdx].controlAddressOffset = controlAddressOffset;
	regSpecs[logicalRegNdx].bitFieldSize = bitFieldSize;
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
	MrSanAntonioRegisters *mrSanAntonio = templateHALData->mrSanAntonio;

	UInt32 mask = (1 << templateHALData->regSpecs[logicalRegNdx].bitFieldSize) - 1;
	HWRegister32Bit *regAddress = (HWRegister32Bit*)((UInt8*)mrSanAntonio + templateHALData->regSpecs[logicalRegNdx].controlAddressOffset);

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
#if 0
	UInt32 halPreferences;								// look at HAL data in nvram
#endif
	Nanoseconds nanoseconds;							// convert 800ns into Absolute Time
#if 0
	PCIAssignedAddress assignedAddresses[2];			// there should be two "phys-addr size" pairs
	UInt32 applAddress[2];								// there should be two logical addresses
	UInt32 hwBaseAddress;								// get hwBaseAddress from Base Register 0
	UInt32 baseRegister0Index;							// which entry in APPL,address is for Base Register 0
	UInt32 baseRegister1Index;							// which entry in APPL,address is for Base Register 1
	UInt16 commandRegister;								// enable Template's memory space if everything ok
#endif	


	RegEntryID regEntryDeaconb;
	RegEntryID regEntryFatman;

	OSErr osErr;
	GDXErr err = kGDXErrUnableToAllocateHALData;		// assume failure

	// save the regEntryID,
	RegistryEntryIDCopy(regEntryID, &templateHALData->regEntryID);

#if 0
	// Template is on a PCI card.  Template contains the Toynbee framebuffer, the Spur CLUT,
	// and the MrSanAntonio timing generator. In this sample code, it is assumed that the
	// Template PCI card uses 2 Base Registers.  This illustrates how the "assigned-address" property
	// must be examined to determine which "AAPL,address" 32 bit entry corresponds with each Base Register.
	// Base Register 0 (offset at 0x10 from the configuration registers) is the base address for the hardware.
	// Base Register 1 (offset at 0x14 from the configuration registers) is the base address for the VRAM.
	// Template's Open Firmware startup code created a "reg" property that described
	// 1) the configuration registers
	// 2) Base Register 0
	// 3) Base Register 1
	// Open Firmware creates an "assigned-address" property that describes Base Register 0 and Base Register 1

	err = GraphicsOSSGetProperty(&templateHALData->regEntryID, kPCIAssignedAddressProperty, assignedAddresses,
		sizeof(PCIAssignedAddress)*2);		// times 2 since there should be 2 assigned-addresses

	// if there is an error, that means Open Firmware was unable to allocate the memory space that was
	// requested.  Hence, Template is unusable at this time and the driver should quit.
	if (err)
		goto ErrorExit;

	// Since the entries of the "assigned-address" property does not necessarily match the order of the
	// entries in the "reg" property, examine the rrrrrrrr bits in each physHi to determine which
	// "phys-addr size" pair matches each Base Register.
	// Base Register 0 (offset at 0x10 from the configuration registers) has rrrrrrrr = 0x10 etc...
	if ( 0x10 == assignedAddresses[0].registerNumber )
	{
		baseRegister0Index = 0;
		baseRegister1Index = 1;
	}
	else
	{
		baseRegister0Index = 1;
		baseRegister1Index = 0;
	}

	// The order of the "phys-addr size" pairs in the "assigned-address" property match the order
	// of the logical addresses in the "AAPL,address" property.  Go find the logical addresses.
	err = GraphicsOSSGetProperty(&templateHALData->regEntryID, "AAPL,address", applAddress, 8);

	if (err)							// should NEVER be an error if gotten this far
		goto ErrorExit;

	hwBaseAddress = applAddress[baseRegister0Index];
	templateHALData->vramBaseAddress = applAddress[baseRegister1Index];

	// have successfully found all base addresses.
	// Enable Template's memory space.  Always do a read-modify-write when hitting configuration registers
	osErr = ExpMgrConfigReadWord(&templateHALData->regEntryID, (LogicalAddress) 0x4, &commandRegister);

	if (osErr)							// shouldn't be an error
		goto ErrorExit;

	commandRegister|= 1 << 1;			// enable the Memory space which is Bit 1

	osErr = ExpMgrConfigWriteWord(&templateHALData->regEntryID, (LogicalAddress) 0x4, commandRegister);
	if (err)							// shouldn't be an error
		goto ErrorExit;


	// Note: More examples
	// 1) a PCI card only uses 1 BaseRegister to access hardware and VRAM.  If the card's
	// Open Firmware startup code created a "reg" property for just the 1 Base Register that it uses,
	// the "AAPL,address" property will contain the single logical address.  So if the "AAPL,address" property
	// exists and is the right size (4), then the PCI card knows Open Firmware was able to allocate the
	// requested memory.  Such a card would only have to do the following to find the logical address.
	//
	//	err = GraphicsOSSGetProperty(&templateHALData->regEntryID, "AAPL,address", &hwBaseAddress, 4);
	//	if (err)							// it is possible to error out if Open Firmware couldn't
	//		goto ErrorExit;					// allocate the requested memory space.
	//
	// 2) a PCI card that doesn't have Open Firmware startup code (BECAUSE THE CARD IS IN DEVELOPMENT)
	// will have a "reg" property automatically created for it by Open Firmware.
	// This is described in chapter 4 of Designing PCI Cards&Drivers.
	// The PCI Graphics card should implement the Expansion ROM Base Register (offset 0x30 from the configuration
	// registers)  because it will eventually be a startup device. Open Firmware will automatically add
	// a "phys-addr size" pair to the "assigned-address" property for the Expansion ROM Base Register.
	// Likewise, an additional entry will be created in the "AAPL,address" property.
	// The driver, during development, needs to allocate additional space in the buffers used to fetch each
	// property.  The entries of the "assigned-address" property need to be exaimined to determine the order
	// as shown above.
	// (All PCI graphics cards should be a startup device and need the Expansion ROM Base Register.  YOU ARE
	// LAME IF YOU DON'T HAVE ONE)
#endif

	templateHALData->hasDeaconb = FindNamedRegEntry("deaconb", &regEntryDeaconb);
	RegistryEntryIDDispose(&regEntryDeaconb);
	templateHALData->hasFatman = FindNamedRegEntry("fatman", &regEntryFatman);
	RegistryEntryIDDispose(&regEntryFatman);
	templateHALData->hasSixty6 = FindNamedRegEntry("sixty6", &templateHALData->regEntryID_sixty6);

	if (templateHALData->hasSixty6)
	{
		templateHALData->hasSixty6 = IsSixty6DisplayConnected();
		if (templateHALData->hasFatman && !templateHALData->hasDeaconb)
			templateHALData->hasSixty6 = false;
	}

	// spur is the CLUT used in Template.
	// 1) 32 or 64 bit pixel port for spur.
	// 2) spur has a programmable PLL clock generator


#if 1 // isForMacOSX
	{
		// this code gets logical addresses necessary for Mac OS X compatibility
		UInt32					baseAddr;
		RegEntryID				regEntryID;
		RegPropertyValueSize	propertySize = sizeof( baseAddr );

		if (templateHALData->hasFatman)
			baseAddr = 0xC8000000; // physical address
		else
			baseAddr = 0xF3000000; // physical address
		if (FindNamedRegEntry("gc", &regEntryID) || FindNamedRegEntry("fatman", &regEntryID))
		{
			#warning // I don't know if the correct logical address is obtained by this code for hardware that contains fatman
			RegistryPropertyGet(&regEntryID, kAAPLDeviceLogicalAddress, &baseAddr, &propertySize);
			RegistryEntryIDDispose(&regEntryID);
		}

		baseAddr += 0x1B000; // RADACAL (see "Control2.c" in BootX source code)

		templateHALData->spur.address = (HWRegister8Bit*)baseAddr + 0x00;
		templateHALData->spur.cursorPaletteRAM = (HWRegister8Bit*)baseAddr + 0x10;
		templateHALData->spur.multiPort = (HWRegister8Bit*)baseAddr + 0x20;
		templateHALData->spur.colorPaletteRAM = (HWRegister8Bit*)baseAddr + 0x30;
	}
#else
	{
		// this code gets physical addresses which are equivelent to logical addresses only in Mac OS 9 and early versions of Mac OS X (≤ Jaguar)

		if (templateHALData->hasFatman)
		{
			templateHALData->spur.address = (HWRegister8Bit*) 0xC801B000;
			templateHALData->spur.cursorPaletteRAM = (HWRegister8Bit*) 0xC801B010;
			templateHALData->spur.multiPort = (HWRegister8Bit*) 0xC801B020;
			templateHALData->spur.colorPaletteRAM = (HWRegister8Bit*) 0xC801B030;
		}
		else
		{
			templateHALData->spur.address = (HWRegister8Bit*) 0xF301B000;
			templateHALData->spur.cursorPaletteRAM = (HWRegister8Bit*) 0xF301B010;
			templateHALData->spur.multiPort = (HWRegister8Bit*) 0xF301B020;
			templateHALData->spur.colorPaletteRAM = (HWRegister8Bit*) 0xF301B030;
		}
	}
#endif

	// fill in the addresses for mrSanAntonio
	#define InitOneControlRegField(logicalRegNdx, controlAddressOffset, bitFieldSize, bitFieldStart, isBitField) \
		DoInitOneControlRegField(logicalRegNdx, offsetof(MrSanAntonioRegisters, controlAddressOffset), bitFieldSize, \
								bitFieldStart, isBitField, templateHALData->regSpecs)

/*

VRAM used in the PowerMac 8600 DIMMs:
Part Number = TC528257J70
Description = Video Dynamic RAM - 512x8 SAM,mask write,fast page
Manufacturer = Toshiba
Number of Words = 256k
Bits Per Word = 8
t(acc) Max. (S) = 70n
tW Min (S) = 130n
Output Config = 3-State
P(D) Max.(W) Power Dissipation = 1
Nom. Supp (V) = 5
Package = SOJ
Pins = 40
Military = N
Technology = CMOS


DIMMs:
4 TC528257J70 chips per DIMM (4 bytes = 32bits), 2 DIMMs per bank (8 bytes = 64bits) = 4MB total (16 bytes = 128 bits)

tSCC = 25 ns = 40 MHz
40 MHz * 4 bytes per DIMM = 8 bit: 160 MHz per DIMM
						  = 16 bit: 80 MHz per DIMM (not good enough for 1152x870@75Hz 100 MHz - proof that the RAMDAC reads from multiple DIMMs)
						  = 32 bit: 40 MHz per DIMM (not good enough for 832x624@75Hz 57 MHz - ditto)

40 MHz * 8 bytes per bank = 8 bit: 320 MHz per bank
						  = 16 bit: 160 MHz per bank
						  = 32 bit: 80 MHz per bank (not good enough for 1152x870@75Hz 100 MHz - proof that the RAMDAC reads from multiple banks)

40 MHz * 16 bytes = 8 bit: 640 MHz
					16 bit: 320 MHz
					32 bit: 160 MHz

From Patent 5793996:

Enables: Controls a variety of features within the bridge/graphics controller. For example, 
- various monitor sync and blank signals (generated by the video timing logic 715) can be enabled or disabled.
Also controls:
- whether graphics memory is 128 bits wide or 64 bits wide

- whether the VRAM state machines generate 50 MHz or 33 MHz waveforms
- whether the VRAM state machines 719 detect page hits on the system bus 501 to frame buffer 517 single beat writes

- whether the shift clock is to be generated
- and whether data transfers are to be performed for the standard bank of VRAM
- whether double buffering is to be enabled, whereby the same data transfers are generated for both the standard bank of VRAM 559-1 and the optional bank 559-2
- whether the system 500 is in big-endian or little-endian mode
- whether the monitor being controlled is Progressive or Interlaced. 
*/




/*

From 74ACT715 data sheet (a hardware example - the 74ACT715 is not used by the chaos/control hardware):

	SYSCK
	   _   _   _   _   _   _   _   _   _   _   _   _   _   _   _   _   _   _   _ 
	|_| |_| |_| |_| |_| |_| |_| |_| |_| |_| |_| |_| |_| |_| |_| |_| |_| |_| |_| 
	                                                                                                                                                                                                                                                               
	    |<                             HMAX                            >|
	    |<    HBLANK   >|                                                                                                                                                                                                                                                               
	____|               |_______________________________________________                                                                                                                                                                                                                                                                                                          
	    \_______________/                                               \________                                                                                                                                                                                       
	    |          >|   |< HBP                                                                                                                                                                                                                                              
	    |  >| HSYNC |<                                                                                                                                                                                                                                                               
	________|        _______________________________________________________                                                                                                                                                                                                                                                                                                          
	    |   \_______/                                                       \____                                                                                                                                                                                       
	   >|   |< HFP                                                                                                                                                                                                                                                     
	    |  >|   |< HEQP                                                                                                                                                                                                                                                              
	________|    ___________________________     ___________________________                                                                                                                                                                                                                                                                                                          
	    |   \___/                           \___/                           \___/                                                                                                                                                                                       
	    |   |                               |                                                                                                                                                                                                                       
	    |   |                        >|     |<- HSERRP
	   _____|                          _____                           _____                                                                                                                                                                                                  
	__/     \_________________________/     \_________________________/     \____
	        |
	        |<         HMAX/2              >|                                                                                                                                                                                                                                                       
	                                                                                                                                                                                                                                                               
                                                                                                                                                                                                                                                               
	HBLANK
	   ___   ___   ___   ___   ___   ___   ___   ___   ___   ___   ___   ___   ___   ___   ___   ___
	|_|   |_|   |_|   |_|   |_|   |_|   |_|   |_|   |_|   |_|   |_|   |_|   |_|   |_|   |_|   |_|   
	                                                                                                                                                                                                                                                               
	                                                                                                                                                                                                                                                               
	      |         VBLANK        |                                                                                                                                                                                                                                                                                                                    
	______|                       |_____________________________________________________      
	      \_______________________/                                                     \___________
	      |     |           |     |                                                                                                                                                                                                                                
	      |     |   VSYNC   |     |                                                                                                                                                                                                                                                       
	____________|           |_________________________________________________________________      
	      |     \___________/                                                                 \_____
	 VFP >|     |<         >|     |< VBP                                                                                                                                                                                                                                     
	                                                                                                                                                                                                                                                               
	                                                                                                                                                                                                                                                               

	                   |<               VFP               >|<              VSYNC              >|
	                   |                                   |                                   |
	                   | 1           3           5         | 7           9           11        | 13          15          17          19          
	            _______|    _______     _______     _______|    _______     _______     _______|    _______     _______     _______     _______     _______
	HBLANK |___|       |___|       |___|       |___|       |___|       |___|       |___|       |___|       |___|       |___|       |___|       |___|
	                   |                                   |                                   |                                   |                                                                                                                                       
	                   |                                   |                                   |                                   |                                                                                                                                       
	       _   _________   ___   ___   ___   ___   ___   ___     _     _     _     _     _     _   ___   ___   ___   ___   ___   ___   _________   ________
	CSYNC   |_|         |_|   |_|   |_|   |_|   |_|   |_|   |___| |___| |___| |___| |___| |___| |_|   |_|   |_|   |_|   |_|   |_|   |_|         |_|
	                   |                                   |                                   |                                   |                                                                                                                                       
	                   |                                   |                                   |                                   |                                                                                                                                       
	       ________________________________________________                                     ___________________________________________________________
	VSYNC                                                  |___________________________________| 
	                   |                                   |                                   |                                   |                                                                                                                                       
	       ____________                                                                                                             _______________________
	                   |___________________________________________________________________________________________________________|
	                   |                                                                                                           |
	                   |<                                     EQUALIZATION SERRATION INTERVAL                                     >|



chaos/control registers:
	  _   _   _   _   _   _   _   _   _   _   _   _   _   _   _   _ 
	_| |_| |_| |_| |_| |_| |_| |_| |_| |_| |_| |_| |_| |_| |_| |_| |
	                                                                                                                                                                                   
	           | HAL                                           | HFP                                                                                                                           
	           |_______________________________________________|                                                                                                                                                                                                                                         
	___________/                                               \____                                                                                                                           
	                                                                                                                                                                                   
	       | HBWAY                                                 | HSP                                                                                                                          
	       |_______________________________________________________|                                                                                                                                                                                                                                          
	_______/                                                       \                                                                                                                           
	                                                                                                                                                                                   
	   | HEQ                       | HLFLN                                                                                                                                                       
	   |___________________________|    ___________________________                                                                                                                                                                                                                                          
	___/                           \___/                           \                                                                                                                           
	                               |                                                                                                                                                    
	                               |                         | HSERR                                                                                                                        
	                          _____|                         |_____                                                                                                                                  
	_________________________/     \_________________________/     \


                                                                                                                                                                                                                                                               
*/
	//                      kRegField enum								offset					    bits    pos     isBitField
	InitOneControlRegField( kRegFieldControlCUR_LINE				,	ControlCUR_LINE			,	12	,	0	,	false	);	//	currentLine: indicates which line of video is currently being displayed. 

	InitOneControlRegField( kRegFieldControlVFPEQ					,	ControlVFPEQ			,	12	,	0	,	false	);	//	verticalFrontPorchEqualization: Controls the Graphics Vertical Front Porch Equalization starting point. 
	InitOneControlRegField( kRegFieldControlVFP						,	ControlVFP				,	12	,	0	,	false	);	//	verticalFrontPorch: Controls the Vertical Front Porch starting point. 
	InitOneControlRegField( kRegFieldControlVAL						,	ControlVAL				,	12	,	0	,	false	);	//	verticalActiveLine: Controls the Graphics Vertical Active Area starting point.
	InitOneControlRegField( kRegFieldControlVBP						,	ControlVBP				,	12	,	0	,	false	);	//	verticalBackPorch: Controls the Vertical Back Porch starting point (without equalization). 
	InitOneControlRegField( kRegFieldControlVBPEQ					,	ControlVBPEQ			,	12	,	0	,	false	);	//	verticalBackPorchEqualization: Controls the Vertical Back Porch starting point (with equalization). 
	InitOneControlRegField( kRegFieldControlVSYNC					,	ControlVSYNC			,	12	,	0	,	false	);	//	verticalSync: Controls the Vertical Sync starting point. 
	InitOneControlRegField( kRegFieldControlVHLINE					,	ControlVHLINE			,	12	,	0	,	false	);	//	verticalHalfLine: Controls the Half Lines in a Field. 

	InitOneControlRegField( kRegFieldControlPIPED					,	ControlPIPED			,	12	,	0	,	false	);	//	PIPED: Controls the Early Hblank point. 

	InitOneControlRegField( kRegFieldControlHPIX					,	ControlHPIX				,	12	,	0	,	false	);	//	horiztonalPixelCount: Controls the Horizontal Pixels Count. 

	InitOneControlRegField( kRegFieldControlHFP						,	ControlHFP				,	12	,	0	,	false	);	//	horizontalFrontPorch: Controls the Graphics Horizontal Front Porch starting point. 
	InitOneControlRegField( kRegFieldControlHAL						,	ControlHAL				,	12	,	0	,	false	);	//	horizontalActiveLine: Controls the Graphics Horizontal Active starting point. 
	InitOneControlRegField( kRegFieldControlHBWAY					,	ControlHBWAY			,	12	,	0	,	false	);	//	horizontalBreezeway: Controls the Graphics Horizontal Breezeway starting point. 
	InitOneControlRegField( kRegFieldControlHSP						,	ControlHSP				,	12	,	0	,	false	);	//	horizontalSyncPulse: Controls the Graphics Horizontal Sync starting point. 
	InitOneControlRegField( kRegFieldControlHEQ						,	ControlHEQ				,	12	,	0	,	false	);	//	horizontalEqualization: Controls the Horizontal Equalizations starting point. 

	InitOneControlRegField( kRegFieldControlHLFLN					,	ControlHLFLN			,	12	,	0	,	false	);	//	halfLine: Controls the Half Line point of Active Video. 
	InitOneControlRegField( kRegFieldControlHSERR					,	ControlHSERR			,	12	,	0	,	false	);	//	horizontalSerration: Controls the Horizontal Serration's starting point. 

	InitOneControlRegField( kRegFieldControlCNTTST					,	ControlCNTTST			,	12	,	0	,	false	);	//	counterTest

	InitOneControlRegField( kRegFieldControlTEST_All				,	ControlTEST				,	11	,	0	,	false	);
	InitOneControlRegField( kRegFieldControlTEST_1_DisableTiming	,	ControlTEST				,	1	,	10	,	true	);	//	1 assert reset, 0 release reset
	InitOneControlRegField( kRegFieldControlTEST_2					,	ControlTEST				,	1	,	9	,	true	);	//			unused always 0
	InitOneControlRegField( kRegFieldControlInterlaced				,	ControlTEST				,	1	,	8	,	true	);	//	0 for progressive, 1 for interlaced
	InitOneControlRegField( kRegFieldControlTEST_4					,	ControlTEST				,	1	,	7	,	true	);	//			always 0
	InitOneControlRegField( kRegFieldControlHSyncPolarity			,	ControlTEST				,	1	,	6	,	true	);	//	h sync polarity 0 = negative 1 = positive
	InitOneControlRegField( kRegFieldControlTEST_6					,	ControlTEST				,	1	,	5	,	true	);	//			always 1
	InitOneControlRegField( kRegFieldControlTEST_7					,	ControlTEST				,	1	,	4	,	true	);	//			always 1
	InitOneControlRegField( kRegFieldControlTEST_8_ResetTiming		,	ControlTEST				,	1	,	3	,	true	);	//	toynbeeRunning
	InitOneControlRegField( kRegFieldControlVSyncPolarity			,	ControlTEST				,	1	,	2	,	true	);	//	v sync polarity 0 = negative 1 = positive
	InitOneControlRegField( kRegFieldControlTEST_10					,	ControlTEST				,	1	,	1	,	true	);	//			always 1
	InitOneControlRegField( kRegFieldControlTEST_11					,	ControlTEST				,	1	,	0	,	true	);	//			always 1

	InitOneControlRegField( kRegFieldControlGBASE					,	ControlGBASE			,	22	,	0	,	false	);	//	GBASE: Contains the Graphics Base Address ›21:5! value. This address indicates where the first pixel of graphics memory is located in VRAM 559. 
	InitOneControlRegField( kRegFieldControlROW_WORDS				,	ControlROW_WORDS		,	15	,	0	,	false	);	//	Row_Words: Contains the Graphics Rowwords value. Rowwords is the number added to the address of the first pixel on any line of Graphics to find the first pixel on the very next line of Graphics. 

	InitOneControlRegField( kRegFieldControlMON_SENSE_All			,	ControlMON_SENSE		,	9	,	0	,	false	);	//	MON_Sens: includes information reflecting the state of the Monitor ID pins and whether particular Monitor outputs are enabled. 
	InitOneControlRegField( kRegFieldControlMON_SENSE_1				,	ControlMON_SENSE		,	3	,	6	,	true	);
	InitOneControlRegField( kRegFieldControlMON_SENSE_2				,	ControlMON_SENSE		,	3	,	3	,	true	);
	InitOneControlRegField( kRegFieldControlMON_SENSE_3				,	ControlMON_SENSE		,	3	,	0	,	true	);

	InitOneControlRegField( kRegFieldControlENABLE_All				,	ControlENABLE			,	12	,	0	,	false	);
	InitOneControlRegField( kRegFieldControlCBlankDisable			,	ControlENABLE			,	1	,	11	,	true	);	// disable CBlank
	InitOneControlRegField( kRegFieldControlCSyncDisable			,	ControlENABLE			,	1	,	10	,	true	);	// disable Composite Sync
	InitOneControlRegField( kRegFieldControlHSyncDisable			,	ControlENABLE			,	1	,	9	,	true	);	// disable Horizontal Sync
	InitOneControlRegField( kRegFieldControlVSyncDisable			,	ControlENABLE			,	1	,	8	,	true	);	// disable Vertical Sync
	InitOneControlRegField( kRegFieldControl50or33MHz				,	ControlENABLE			,	1	,	7	,	true	);	//			always 0	- possibly whether the VRAM state machines generate 50 MHz or 33 MHz waveforms
	InitOneControlRegField( kRegFieldControlWide					,	ControlENABLE			,	1	,	6	,	true	);	//						- possibly whether graphics memory is 128 bits wide or 64 bits wide
	InitOneControlRegField( kRegFieldControlDetectPageHits			,	ControlENABLE			,	1	,	5	,	true	);	//						- possibly whether the VRAM state machines detect page hits on the system bus to frame buffer single beat writes
	InitOneControlRegField( kRegFieldControlShiftClock				,	ControlENABLE			,	1	,	4	,	true	);	//						- possibly whether the shift clock is to be generated
	InitOneControlRegField( kRegFieldControlStandardBankDisable		,	ControlENABLE			,	1	,	3	,	true	);	// 						- possibly whether data transfers are to be performed for the standard bank of VRAM
	InitOneControlRegField( kRegFieldControlDoubleBufferingEnable	,	ControlENABLE			,	1	,	2	,	true	);	// 						- possibly whether double buffering is to be enabled, whereby the same data transfers are generated for both the standard bank of VRAM and the optional bank
	InitOneControlRegField( kRegFieldControlLittleEndian			,	ControlENABLE			,	1	,	1	,	true	);	//			always 0	- possibly endian mode: 0 = big endian
	InitOneControlRegField( kRegFieldControlNotInterlaced			,	ControlENABLE			,	1	,	0	,	true	);	// 1 for progressive, 0 for interlaced

	InitOneControlRegField( kRegFieldControlGSC_DIVIDE				,	ControlGSC_DIVIDE		,	2	,	0	,	false	);	//	GSCDivide: Controls the Graphics clock divide count, which determines the rate at which data is shifted out of VRAM 559 and loaded into the RAMDAC 523. 

	InitOneControlRegField( kRegFieldControlREFRESH_COUNT			,	ControlREFRESH_COUNT	,	10	,	0	,	false	);	//	RefreshCount: Contains the count of system bus clocks used to generate refresh cycles to the VRAMs 559. In a preferred embodiment, the VRAMs need to be refreshed every 15.6 microseconds. 

	InitOneControlRegField( kRegFieldControlINT_ENABLE_All			,	ControlINT_ENABLE		,	4	,	0	,	false	);	//	Interrupt Enable: enables/disables/clears a number of interrupts to the primary processor, including vertical blank interrupt; system bus write error interrupt; and expansion bus write error interrupt. 
	InitOneControlRegField( kRegFieldControlINT_ENABLE_1			,	ControlINT_ENABLE		,	1	,	3	,	true	);	//	clearCursorInterrupt
	InitOneControlRegField( kRegFieldControlINT_ENABLE_2			,	ControlINT_ENABLE		,	1	,	2	,	true	);	//	clearCursorInterrupt
	InitOneControlRegField( kRegFieldControlINT_ENABLE_3			,	ControlINT_ENABLE		,	1	,	1	,	true	);	//			always 0
	InitOneControlRegField( kRegFieldControlINT_ENABLE_4			,	ControlINT_ENABLE		,	1	,	0	,	true	);	//			always 0

	InitOneControlRegField( kRegFieldControlINT_STATUS_All			,	ControlINT_STATUS		,	3	,	0	,	false	);	//	Interrupt Status: indicates which interrupts have occurred
	InitOneControlRegField( kRegFieldControlINT_STATUS_1			,	ControlINT_STATUS		,	1	,	2	,	true	);	//	kCursorInterruptStatusBit
	InitOneControlRegField( kRegFieldControlINT_STATUS_2			,	ControlINT_STATUS		,	1	,	1	,	true	);	//			unused
	InitOneControlRegField( kRegFieldControlINT_STATUS_3			,	ControlINT_STATUS		,	1	,	0	,	true	);	//			unused

	osErr = MyExpMgrConfigReadLong(&templateHALData->regEntryID, (LogicalAddress)kPCIConfigBaseAddress1, (UInt32*)&templateHALData->mrSanAntonio);
	if (osErr)
		goto ErrorExit;

	// fill in the addresses for the toynbee framebuffer.
	osErr = MyExpMgrConfigReadLong(&templateHALData->regEntryID, (LogicalAddress)kPCIConfigBaseAddress2, &templateHALData->vramBaseAddress);
	if (osErr)
		goto ErrorExit;


	templateHALData->senseLineEnable = &templateHALData->mrSanAntonio->ControlMON_SENSE;

	// initialize hardwareCursorData
	cursorDescriptor = &(templateHALData->hardwareCursorData.cursorDescriptor);
	cursorDescriptor->majorVersion = 0;
	cursorDescriptor->minorVersion = 0;
	cursorDescriptor->height = 32;
	cursorDescriptor->width = 32;
	cursorDescriptor->bitDepth = 4;
	cursorDescriptor->maskBitDepth = 0;
	cursorDescriptor->numColors = kNumHardwareCursorColors;
	cursorDescriptor->colorEncodings = (UInt32 *) &(templateHALData->hardwareCursorData.colorEncodings);
	for (i = 0; i < kNumHardwareCursorColors; i++)
		cursorDescriptor->colorEncodings[i] = 8 + i;
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
		// err = GraphicsOSSSaveProperty(&sixty6HALData->regEntryID, "needFullInit",
		//			&needFullInit, sizeof(needFullInit), kOSSPropertyVolatile);

		HALReplacementDriverInfo replacementDriverInfo;

		err = GraphicsOSSGetProperty(&templateHALData->regEntryID, "HALReplacementInfo",
				&replacementDriverInfo, sizeof(HALReplacementDriverInfo));

		if (!err)
		{
			SInt16 width;
	
			templateHALData->depthMode = replacementDriverInfo.depthMode;
			templateHALData->baseAddressPageCurrent = replacementDriverInfo.baseAddressPageCurrent;
			templateHALData->baseAddressPage0 = replacementDriverInfo.baseAddressPage0;
			templateHALData->baseAddressPage1 = replacementDriverInfo.baseAddressPage1;
			templateHALData->displayModeID = replacementDriverInfo.displayModeID;
			templateHALData->vramUsageMode = replacementDriverInfo.vramUsageMode;
			templateHALData->currentPage = replacementDriverInfo.currentPage;
			templateHALData->width = replacementDriverInfo.width;
			templateHALData->height = replacementDriverInfo.height;
			templateHALData->displayCode = replacementDriverInfo.displayCode;
			templateHALData->cvhSyncDisabled = replacementDriverInfo.cvhSyncDisabled;
			templateHALData->numPages = replacementDriverInfo.numPages;
			templateHALData->interlaced = replacementDriverInfo.interlaced;
			templateHALData->fVRAMBank1 = replacementDriverInfo.fVRAMBank1;
			templateHALData->fVRAMBank2 = replacementDriverInfo.fVRAMBank2;
			templateHALData->monoOnly = replacementDriverInfo.monoOnly;
			templateHALData->compositSyncDisabled = replacementDriverInfo.compositSyncDisabled;
//			templateHALData->vdPowerState = replacementDriverInfo.vdPowerState;
//			templateHALData->vramSize = replacementDriverInfo.vramSize;
//			templateHALData->vramWidth32 = replacementDriverInfo.vramWidth32;
//			templateHALData->clutOff = replacementDriverInfo.clutOff;
//			templateHALData->toynbeeRunning = replacementDriverInfo.toynbeeRunning;

			width = (replacementDriverInfo.width + 31) & ~31;

			if (replacementDriverInfo.depthMode == kDepthMode1)
				templateHALData->rowBytes = width + kRowBytesOffset;
			else if (replacementDriverInfo.depthMode == kDepthMode2)
				templateHALData->rowBytes = width * 2 + kRowBytesOffset;
			else
				templateHALData->rowBytes = width * 4 + kRowBytesOffset;

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
		if (templateHALData->displayModeID == kDisplay800x600At60HzVGA)
		{
			*replacingDriver = false;
			// a bogus displayModeID so that MrSanAntonio will always be hit on first call to ProgramHardware
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
		templateHALData->cvhSyncDisabled = 0;
		templateHALData->numPages = 1;
	}

	// always try to delete the HALReplacementDriverInfo in the nameRegistry
	(void) GraphicsOSSDeleteProperty(&templateHALData->regEntryID, "HALReplacementInfo");


	// Spur clut must sometimes wait for stuff to happen, e.g. when writing rgb to the clut,
	// it is necessary to wait ≈800 ns before writing the next rgb.  Since this is done many times,
	// convert 800 ns to absolute time and save in the hal data.
	nanoseconds.hi = kDefaultCLUTDelayHigh;
	nanoseconds.lo = kDefaultCLUTDelayLow;
	templateHALData->absCLUTAddrRegDelay = NanosecondsToAbsolute(nanoseconds);

	templateHALData->senseLineAndVideoDelay5ms = DurationToAbsolute(5*durationMillisecond);

	templateHALData->usingCustomClutDelay = false;

	if (*replacingDriver)
	{
		BitDepthIndependentData bdiData;
		UInt32 numberOfEntries;						// clut will have 31 or 255 entries (0 based)
		err = TemplateGetDisplayData(true, templateHALData->displayModeID, templateHALData->depthMode, k4MegVRAM, &bdiData, nil, nil);
		if (!err)
		{
			nanoseconds.lo = bdiData.nsCLUTAddrRegDelay;
			nanoseconds.hi = 0;
			templateHALData->absCLUTAddrRegDelay = NanosecondsToAbsolute(nanoseconds);
		}
		if (templateHALData->depthMode == kDepthMode2)
			numberOfEntries = 31; // for 5 bit per channel
		else
			numberOfEntries = 255; // for 8 bit indexed and 8 bit per channel
		templateHALData->spur.address = 0;								// Start at CLUT entry 0
		SynchronizeIO();
		for (i = 0; i <= numberOfEntries; i++)
		{
			DelayForHardware(templateHALData->absCLUTAddrRegDelay);
			templateHALData->savedClut[i].red = *templateHALData->spur.colorPaletteRAM;
			SynchronizeIO();
			templateHALData->savedClut[i].green = *templateHALData->spur.colorPaletteRAM;
			SynchronizeIO();
			templateHALData->savedClut[i].blue = *templateHALData->spur.colorPaletteRAM;
			SynchronizeIO();
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
	RegistryEntryIDDispose(&templateHALData->regEntryID_sixty6);
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
	RegistryEntryIDDispose(&templateHALData->regEntryID_sixty6);
}



//=====================================================================================================
//
// GraphicsHALOpen_2()
//
//=====================================================================================================
static void GraphicsHALOpen_2(void)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	SpurRegisters *spur = &templateHALData->spur;

/*
	// it is possible that the driver is replacing a ROM driver and the machine was
	// in low power mode, in which case the clut would be off.  Turn on the DAC and PLL
	
	*templateHALData->spur.address = kSpurPhaseLockLoopControl;	// DAC and PLL address	
	SynchronizeIO();

	*templateHALData->spur.multiPort = 1 << kSpurPhaseLockLoopControlCLKorPLLBit;
*/

	// turn off the hw cursor.  Other control routines will turn it on as necessary
	*spur->address = kSpurControl;			// Setup the address register for 'control'
	SynchronizeIO();
	*spur->multiPort = 0;	// turn off hw cursor (clear kSpurControlCursorEnableBit)

	*spur->address = kSpur0x21;
	SynchronizeIO();
	*spur->multiPort = kSpur0x21Value0;	// clear kSpur0x21Bit0Bit

	*spur->address = kSpurCursorXPositionLow;
	SynchronizeIO();
	*spur->multiPort = 0;

	*spur->address = kSpurCursorXPositionHigh;
	SynchronizeIO();
	*spur->multiPort = 0;

	*spur->address = kSpur0x22;
	SynchronizeIO();
	*spur->multiPort = kSpur0x22Value0;

	*spur->address = 0;								// Start at CLUT entry 0

	*spur->cursorPaletteRAM = 0;
	*spur->cursorPaletteRAM = 0;
	*spur->cursorPaletteRAM = 0;

	*spur->cursorPaletteRAM = 255;
	*spur->cursorPaletteRAM = 0;
	*spur->cursorPaletteRAM = 0;

	*spur->cursorPaletteRAM = 0;
	*spur->cursorPaletteRAM = 255;
	*spur->cursorPaletteRAM = 0;

	*spur->cursorPaletteRAM = 0;
	*spur->cursorPaletteRAM = 0;
	*spur->cursorPaletteRAM = 255;

	*spur->cursorPaletteRAM = 255;
	*spur->cursorPaletteRAM = 255;
	*spur->cursorPaletteRAM = 0;

	*spur->cursorPaletteRAM = 0;
	*spur->cursorPaletteRAM = 255;
	*spur->cursorPaletteRAM = 255;

	*spur->cursorPaletteRAM = 255;
	*spur->cursorPaletteRAM = 0;
	*spur->cursorPaletteRAM = 255;

	*spur->cursorPaletteRAM = 255;
	*spur->cursorPaletteRAM = 255;
	*spur->cursorPaletteRAM = 255;
}



//=====================================================================================================
//
// TemplateDetectVRAM()
//	The section "VRAM state machines 719" in patent 5793996 explaining VRAM addressing, banks,
//	pages, endian modes, etc may apply to the hardware that this driver is responsible for.
//
//=====================================================================================================
static void TemplateDetectVRAM(void)
{
	HWRegister32Bit* testAddr;
	UInt32 test0;
	UInt32 test2;

	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	templateHALData->fVRAMBank1 = false;

	testAddr = (HWRegister32Bit*)(templateHALData->vramBaseAddress + 0x00800000);

	ControlWriteRegister(kRegFieldControlWide, 1);
	ControlWriteRegister(kRegFieldControlStandardBankDisable, 0);
	ControlWriteRegister(kRegFieldControlDoubleBufferingEnable, 0);

	testAddr[0] = 'Nano';
	SynchronizeIO();

	testAddr[1] = -1;
	SynchronizeIO();

	test0 = testAddr[0];
	SynchronizeIO();

	test2 = testAddr[2];
	SynchronizeIO();

	if (test0 == 'Nano')
	{
		templateHALData->fVRAMBank1 = true;
		if (test2 != 'Nano')
			templateHALData->fVRAMBank2 = true;
	}
	else
	{
		testAddr[2] = 'Nano';
		SynchronizeIO();

		testAddr[3] = -1;
		SynchronizeIO();

		test2 = testAddr[2];
		if (test2 == 'Nano')
			templateHALData->fVRAMBank2 = true;
	}
}



//=====================================================================================================
//
// TemplateSetSixty6CanRun()
//
//=====================================================================================================
static void TemplateSetSixty6CanRun(RegEntryID* regEntryID, Boolean canRun)
{
	char propertyValue[4];
	RegPropertyValueSize propertySize = sizeof(propertyValue);
	if (canRun)
	{
		if (RegistryPropertyGet(regEntryID, "canRun", &propertyValue, &propertySize) != noErr)
			RegistryPropertyCreate(regEntryID, "canRun", "yes", 4);
		else
			RegistryPropertySet(regEntryID, "canRun", "yes", 4);
	}
	else
		RegistryPropertyDelete(regEntryID, "canRun");
}



//=====================================================================================================
//
// TemplateSetSomeRegisters()
//
//=====================================================================================================
static void TemplateSetSomeRegisters(UInt32 vramUsageMode)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	switch (vramUsageMode)
	{
		case 3:
			ControlWriteRegister(kRegFieldControlStandardBankDisable, 0);
			ControlWriteRegister(kRegFieldControlDoubleBufferingEnable, 1);
			ControlWriteRegister(kRegFieldControlWide, 0);
			break;
		case 0:
			if (templateHALData->vramSize == k4MegVRAM)
				ControlWriteRegister(kRegFieldControlWide, 1);
			else
				ControlWriteRegister(kRegFieldControlWide, 0);
			ControlWriteRegister(kRegFieldControlStandardBankDisable, 0);
			ControlWriteRegister(kRegFieldControlDoubleBufferingEnable, 0);
			break;
		case 1:
		case 2:
			ControlWriteRegister(kRegFieldControlStandardBankDisable, 1);
			ControlWriteRegister(kRegFieldControlDoubleBufferingEnable, 0);
			ControlWriteRegister(kRegFieldControlWide, 0);
			break;
	}
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
GDXErr GraphicsHALOpen(const AddressSpaceID spaceID, Boolean replacingDriver)
{

	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	MrSanAntonioRegisters *mrSanAntonio = templateHALData->mrSanAntonio;
	SpurRegisters *spur = &templateHALData->spur;

	RegEntryID regEntryID_devicetree;
	
	GDXErr err = kGDXErrNoError;
	
	if (!replacingDriver)
	{
		// default state
		GraphicsHALOpen_2();
		ControlWriteRegister(kRegFieldControlVFPEQ, 0);
		ControlWriteRegister(kRegFieldControlVFP, 0);
		ControlWriteRegister(kRegFieldControlVAL, 0);
		ControlWriteRegister(kRegFieldControlVBP, 0);
		ControlWriteRegister(kRegFieldControlVBPEQ, 0);
		ControlWriteRegister(kRegFieldControlVSYNC, 0);
		ControlWriteRegister(kRegFieldControlVHLINE, 0);
		ControlWriteRegister(kRegFieldControlPIPED, 0);
		ControlWriteRegister(kRegFieldControlHPIX, 0);
		ControlWriteRegister(kRegFieldControlHFP, 0);
		ControlWriteRegister(kRegFieldControlHAL, 0);
		ControlWriteRegister(kRegFieldControlHBWAY, 0);
		ControlWriteRegister(kRegFieldControlHSP, 0);
		ControlWriteRegister(kRegFieldControlHEQ, 0);
		ControlWriteRegister(kRegFieldControlHLFLN, 0);
		ControlWriteRegister(kRegFieldControlHSERR, 0);
		ControlWriteRegister(kRegFieldControlCNTTST, 0);
		ControlWriteRegister(kRegFieldControlTEST_All, 0);
		ControlWriteRegister(kRegFieldControlGBASE, 0);
		ControlWriteRegister(kRegFieldControlROW_WORDS, 0);
		ControlWriteRegister(kRegFieldControlMON_SENSE_All, 7 << 3);
		ControlWriteRegister(kRegFieldControlENABLE_All, 1 << 6); // kRegFieldControlWide
		ControlWriteRegister(kRegFieldControlGSC_DIVIDE, 1);
		ControlWriteRegister(kRegFieldControlShiftClock, 1);
		FindNamedRegEntry("device-tree", &regEntryID_devicetree);

		{
			UInt32 clockFrequency;
			RegPropertyValueSize clockFrequencySize = sizeof(UInt32); /* this was added by me */
			err = RegistryPropertyGet(&regEntryID_devicetree, "clock-frequency", &clockFrequency, &clockFrequencySize);
			
			// the VRAMs need to be refreshed every 15.6 microseconds. 
			if (!err)
			{
				clockFrequency /= 10000000; // (50 million clocks per second) ÷ (10 million tenths of a microsecond per second) = 5 clocks per tenth of a microsecond
				
				ControlWriteRegister(kRegFieldControlREFRESH_COUNT, clockFrequency*156); // 5 clocks per tenth of a microsecond * 156 tenths of a microsecond per refresh = 5 * 156 clocks per refresh
			}
			else
			{
				ControlWriteRegister(kRegFieldControlREFRESH_COUNT, 5*156); // 15.6 microseconds
			}
		}
		ControlWriteRegister(kRegFieldControlINT_ENABLE_All, 0);
		TemplateDetectVRAM();
	}

/*
	// internal interrupts in Template should be on so that HAL programs hw during vbl
	*mrSanAntonio->interruptMask = kCursorInterruptEnableMask;	// cursor interrupts on, vbl & animate off


	//	The phaseLockLoopControl register determines which of the two frequency register sets the PLL
	//	is actively using.  Bit kSpurPhaseLockLoopControlSetSelectBit selects bewteen set A and B.
	// 0 = set A, 1 = set B.  On reset, set A is selected as the active set.
	//	Since the non active set is always used to reprogram the clock, mark that Set A is active.
		
	templateHALData->usingClockSetA = true;

	// when opened, hw is up and running....vdPowerState is always kAVPowerOn as shown in Initialize
	templateHALData->vdPowerState = kAVPowerOn;
*/


	if (templateHALData->fVRAMBank1)
		templateHALData->vramUsageMode = 0;
	else if (templateHALData->fVRAMBank2)
		templateHALData->vramUsageMode = 2;


	if (!templateHALData->fVRAMBank1 && !templateHALData->fVRAMBank2)
	{
		err = kGDXErrUnknownError;
		goto ErrorExit;
	}


	if (templateHALData->fVRAMBank2 && templateHALData->hasSixty6)
	{
		templateHALData->vramUsageMode = 2;
		TemplateSetSixty6CanRun(&templateHALData->regEntryID_sixty6, true);
	}
	else
	{
		TemplateSetSixty6CanRun(&templateHALData->regEntryID_sixty6, false);
	}


	switch(templateHALData->vramUsageMode)
	{
		default:
			err = kGDXErrInvalidParameters;
			goto ErrorExit;
		case 0:
			if (templateHALData->fVRAMBank1)
				if (templateHALData->fVRAMBank2)
					templateHALData->vramSize = k4MegVRAM;
				else
					templateHALData->vramSize = k2MegVRAM;
			break;
		case 1:
		case 2:
			if (templateHALData->fVRAMBank2)
				templateHALData->vramSize = k2MegVRAM;
			break;
		case 3:
			if (templateHALData->fVRAMBank2)
				templateHALData->vramSize = k2MegVRAM;
			break;
	}


	if (!replacingDriver)
	{
		TemplateSetSomeRegisters(templateHALData->vramUsageMode);
		if (templateHALData->vramSize == k2MegVRAM)
			ControlWriteRegister(kRegFieldControlStandardBankDisable, 1);
	}


	{
		Boolean setProcessorCache = true;
		
		/* patch the following crap */

		// The original ndrv for control dereferences this address twice - I have no idea what it is but I think the resulting number is the PVR
		// In the Mac OS 8 version of the control ndrv, the address was 0x5FFFEFD8.

		if (gIsForMacOSX)
		{
			#warning // verify if the OS 9 code is actually getting the PVR and if so then add PVR getting code here
			// the Mac OS X control.ndrv replaced "pvrValueMaybe = **weirdAddress" with "pvrValueMaybe = 0"
		}
		else
		{
			volatile UInt32** weirdAddress = (volatile UInt32**)0x68FFEFD8;
			UInt32 pvrValueMaybe;

			pvrValueMaybe = **weirdAddress;
			if ((pvrValueMaybe >> 16) == 4)
			{
				pvrValueMaybe = **weirdAddress;
				if ((pvrValueMaybe & 0x0FFFF) < 0x0303)
					setProcessorCache = false;
			}

			/*
			604 processor versions:
				3.01	0x00040300
				3.1		0x00040301
				3.2		0x00040302
		
				3.3		0x00040303
				3.4		0x00040304
				3.5		0x00040305
				3.6		0x00040306
				3.7		0x00040307
				4		0x00040400
				5		0x00040500
				5.1		0x00040501
				6.1		0x00040601
			*/
			
		}
		if (setProcessorCache)
			SetProcessorCacheMode(spaceID, (ConstLogicalAddress)(templateHALData->vramBaseAddress + 0x00800000), 0x00800000, kProcessorCacheModeWriteThrough);
	}

	// Reset cursor state.
	templateHALData->hardwareCursorData.deferredMove = false;
	templateHALData->hardwareCursorData.cursorSet = false;
	templateHALData->hardwareCursorData.cursorRendered = false;
	templateHALData->hardwareCursorData.cursorCleared = false;
	templateHALData->hardwareCursorData.cursorVisible = false;

	ControlWriteRegister(kRegFieldControlINT_ENABLE_2, 1);

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
GDXErr GraphicsHALClose(const AddressSpaceID spaceID)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	SetProcessorCacheMode(spaceID, (ConstLogicalAddress)(templateHALData->vramBaseAddress + 0x00800000), 0x00800000, kProcessorCacheModeDefault);
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
		replacementDriverInfo.vramSize = templateHALData->vramSize;
		replacementDriverInfo.rowBytes = templateHALData->rowBytes;
		replacementDriverInfo.vramWidth32 = templateHALData->vramWidth32;
		replacementDriverInfo.clutOff = templateHALData->clutOff;
		replacementDriverInfo.toynbeeRunning = templateHALData->toynbeeRunning;
*/
		replacementDriverInfo.baseAddressPage0 = templateHALData->baseAddressPage0;
		replacementDriverInfo.baseAddressPage1 = templateHALData->baseAddressPage1;
		replacementDriverInfo.displayModeID = templateHALData->displayModeID;
		replacementDriverInfo.baseAddressPageCurrent = templateHALData->baseAddressPageCurrent;
		replacementDriverInfo.vramUsageMode = templateHALData->vramUsageMode;
		replacementDriverInfo.depthMode = templateHALData->depthMode;
		replacementDriverInfo.currentPage = templateHALData->currentPage;
		replacementDriverInfo.width = templateHALData->width;
		replacementDriverInfo.height = templateHALData->height;
		replacementDriverInfo.displayCode = templateHALData->displayCode;
		replacementDriverInfo.cvhSyncDisabled = templateHALData->cvhSyncDisabled;
		replacementDriverInfo.numPages = templateHALData->numPages;
		replacementDriverInfo.interlaced = templateHALData->interlaced;
		replacementDriverInfo.fVRAMBank1 = templateHALData->fVRAMBank1;
		replacementDriverInfo.fVRAMBank2 = templateHALData->fVRAMBank2;
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
GDXErr GraphicsHALGetUnknownRoutines( Boolean *reportsDDCConnection, 
		BooleanProc *readSenseLine2Proc, BooleanProc *readSenseLine1Proc, VoidProc *senseLine2SetProc, VoidProc *senseLine2ClearProc, VoidProc *senseLine1SetProc, VoidProc *senseLine1ClearProc,
		VoidProc *senseLine2ResetProc, VoidProc *senseLine1ResetProc, VoidProc *senseLine2and1ResetProc, VoidProc *resetSenseLinesProc, RawSenseCodeProc *readSenseLinesProc, DDCPostProcessBlockProc *setDDCInfoProc )
{
	GDXErr err = kGDXErrNoError;

	*reportsDDCConnection = true;

	*readSenseLine2Proc = &GraphicsHALCallbackReadSenseLine2;
	*readSenseLine1Proc = &GraphicsHALCallbackReadSenseLine1;
	*senseLine2SetProc = &GraphicsHALCallbackSenseLine2Set;
	*senseLine2ClearProc = &GraphicsHALCallbackSenseLine2Clear;
	*senseLine1SetProc = &GraphicsHALCallbackSenseLine1Set;
	*senseLine1ClearProc = &GraphicsHALCallbackSenseLine1Clear;
	*senseLine2ResetProc = &GraphicsHALCallbackResetSenseLine2;
	*senseLine1ResetProc = &GraphicsHALCallbackResetSenseLine1;
	*senseLine2and1ResetProc = &GraphicsHALCallbackResetSenseLine2and1;
	*resetSenseLinesProc = &TemplateResetSenseLines;
	*readSenseLinesProc = &TemplateReadSenseLines;
	*setDDCInfoProc = &GraphicsHALCallbackSetDDCInfo;

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
	
	templateHALData->supports640x480At60Hz = ddcBlockData->established_timings.timing_640x480_60;
	templateHALData->supports640x480At67Hz = ddcBlockData->established_timings.timing_640x480_67;
	templateHALData->supports800x600At60Hz = ddcBlockData->established_timings.timing_800x600_60;
	templateHALData->supports800x600At72Hz = ddcBlockData->established_timings.timing_800x600_72;
	templateHALData->supports800x600At75Hz = ddcBlockData->established_timings.timing_800x600_75;
	templateHALData->supports832x624At75Hz = ddcBlockData->established_timings.timing_832x624_75;
	templateHALData->supports1024x768At60Hz = ddcBlockData->established_timings.timing_1024x768_60;
	templateHALData->supports1024x768At70Hz = ddcBlockData->established_timings.timing_1024x768_70;
	templateHALData->supports1024x768At75Hz = ddcBlockData->established_timings.timing_1024x768_75;
	templateHALData->supports1152x870At75Hz = ddcBlockData->manufacturer_timings.timing_1152x870_75;
	templateHALData->supports1280x1024At75Hz = ddcBlockData->established_timings.timing_1280x1024_75;
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

	SpurRegisters *spur;

	Boolean vblInterruptsEnabled = false;				// default to not enabled;

	UInt32 i;											// utterly famous iterator


	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	AbsoluteTime absCLUTAddrRegDelay = templateHALData->absCLUTAddrRegDelay;				// 128ns in absolute time to hit clut

	// Flush out any deferred cursor moving.
	DeferredMoveHardwareCursor();

	spur = &templateHALData->spur;

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

	*spur->address = 0;								// Start at CLUT entry 0
	SynchronizeIO();

	if (absCLUTAddrRegDelay.lo != 0)
		DelayForHardware(absCLUTAddrRegDelay);

	// gray the clut.  Each time the address register is hit, delay 800 ns since the
	// clut is retrieving data at the new address.  The address register autoincrements
	// after blue is written, hence the delay
	for (i = 0 ; i < kClutSize ; i++)
	{

		(void) DelayForHardware(absCLUTAddrRegDelay);

		*spur->colorPaletteRAM = *midPointRed;		// Red
		SynchronizeIO();
		*spur->colorPaletteRAM = *midPointGreen;	// Green
		SynchronizeIO();
		*spur->colorPaletteRAM = *midPointBlue;		// Blue
		SynchronizeIO();

		if (absCLUTAddrRegDelay.lo != 0)
			DelayForHardware(absCLUTAddrRegDelay);
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
//	ones. In 4-bits-per-pixel mode, for example, the entry positions could range from 0, 1, 3,…, 15,
//	even though the physical positions may not have this number sequence.
//
//	No range checking is required, because the caller has allready done so.	
//
//		-> originalCSTable		
//		This is a pointer to the array of ColorSpecs provided by the caller. This is only provided in
//		the event that the hardware should not use the orrectedCSTable. If any adjuments need to made
//		to it, then they should be done to a copy. Don’t throw away the const!
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
	AbsoluteTime absCLUTAddrRegDelay = templateHALData->absCLUTAddrRegDelay;				// 128ns in absolute time to hit clut
	SpurRegisters *spur;

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

		spur = &templateHALData->spur;

		if (sequential)
		{
			*spur->address = startPosition;
			SynchronizeIO();

			if (absCLUTAddrRegDelay.lo != 0)
				(void) DelayForHardware(absCLUTAddrRegDelay);

			endPosition = startPosition + numberOfEntries;

			if (startPosition <= endPosition)
			{
				// Program the CLUT entries.  For our hardware, use the correctedCSTable				
				for (i = startPosition ; i <= endPosition; i++)
				{
					templateHALData->savedClut[i].red = correctedCSTable[i].rgb.red;
					*spur->colorPaletteRAM = correctedCSTable[i].rgb.red;
					SynchronizeIO();

					templateHALData->savedClut[i].green = correctedCSTable[i].rgb.green;
					*spur->colorPaletteRAM = correctedCSTable[i].rgb.green;
					SynchronizeIO();

					templateHALData->savedClut[i].blue = correctedCSTable[i].rgb.blue;
					*spur->colorPaletteRAM = correctedCSTable[i].rgb.blue;
					SynchronizeIO();

					if (absCLUTAddrRegDelay.lo != 0)
						(void) DelayForHardware(absCLUTAddrRegDelay);	// need 128ns after the address register is hit
																		// it is fetching the clut data at the address
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
					*spur->address = logicalAddress;
					SynchronizeIO();
					if (absCLUTAddrRegDelay.lo != 0)
						(void) DelayForHardware(absCLUTAddrRegDelay);

					templateHALData->savedClut[logicalAddress].red = correctedCSTable[i].rgb.red;
					*spur->colorPaletteRAM = correctedCSTable[i].rgb.red;
					SynchronizeIO();

					templateHALData->savedClut[logicalAddress].green = correctedCSTable[i].rgb.green;
					*spur->colorPaletteRAM = correctedCSTable[i].rgb.green;
					SynchronizeIO();

					templateHALData->savedClut[logicalAddress].blue = correctedCSTable[i].rgb.blue;
					*spur->colorPaletteRAM = correctedCSTable[i].rgb.blue;
					SynchronizeIO();

					if (absCLUTAddrRegDelay.lo != 0)
						(void) DelayForHardware(absCLUTAddrRegDelay);	// need 128ns after the address register is hit
																		// it is fetching the clut data at the address
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
//	ones. At 4 bpp, for example, the entry positions could range from 0, 1, 2,…, 15, even though the
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
	
	GDXErr err = kGDXErrUnknownError;							// Assume failure.
	Boolean	modePossible = false;
	
	// For the Template graphics hardware, the 'pageCount' is always 1 for any supported 
	// 'DisplayModeID'  Therefore, the validity of the  the specified 'DisplayModeID' and 'DepthMode'
	// will be tested by calling GraphicsHALModePossible(), passing in '0' for the page #.
	
	err = GraphicsHALModePossible(displayModeID, depthMode, 0, &modePossible);
	if (!modePossible || err)
	{
		// Opps...caller specified an invalid 'DisplayModeID' or 'DepthMode'
		err = kGDXErrInvalidParameters;
		goto ErrorExit;
	}
	
	// Template graphics architecture only supports 1 page.

	*pageCount = templateHALData->numPages;

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
	// Relationship: QD base address = Toynbee base address + kHardwareCursorOffset
	if (page == 0)
		*baseAddress = (char *)(templateHALData->baseAddressPage0);
	else
		*baseAddress = (char *)(templateHALData->baseAddressPage1);

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
		sync->csMode = 0	|
						  1 << kDisableHorizontalSyncBit |
						  1 << kDisableVerticalSyncBit |
						  1 << kDisableCompositeSyncBit;
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
		ControlWriteRegister(kRegFieldControlCBlankDisable, 1);	// disable CBlank
		ControlWriteRegister(kRegFieldControlVSyncDisable, 0);	// enable vertical sync pulses
		ControlWriteRegister(kRegFieldControlHSyncDisable, 0);	// enable horizontal sync pulses
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
		ControlWriteRegister(kRegFieldControlCBlankDisable, 0);			// then enable CBlank
	}
	else
	{
		ControlWriteRegister(kRegFieldControlCBlankDisable, 1);			// else disable CBlank
	}

	// Template enables a sync by writing a 0 to the sync.  So the value of each sync is cvh bit in
	// in cvhSyncDisabled.  For each sync in MrSanAntonio, 0 = enabled, 1 = disabled
	ControlWriteRegister(kRegFieldControlCSyncDisable, (cvhSyncDisabled & kCompositeSyncMask) >> kDisableCompositeSyncBit); 		// composite pulses
	ControlWriteRegister(kRegFieldControlVSyncDisable, (cvhSyncDisabled & kVerticalSyncMask) >> kDisableVerticalSyncBit); 			// vertical pulses
	ControlWriteRegister(kRegFieldControlHSyncDisable, (cvhSyncDisabled & kHorizontalSyncMask) >> kDisableHorizontalSyncBit); 		// horizontal pulses

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
		maxTimingTableEntries = 34,
		notValid = 0,
		valid = (1 << kModeValid),
		validAndSafe = (1 << kModeValid | 1 << kModeSafe),
		validAndSafeAndDefault = ( (1 << kModeValid) | (1 << kModeSafe) | (1 << kModeDefault) )
	};

	DisplayModeTimingTable theDisplayModeTimingTable[maxTimingTableEntries] =
	{
		{kDisplayCode12Inch, kDisplay512x384At60Hz, validAndSafe},
		{kDisplayCodeStandard, kDisplay640x480At67Hz, validAndSafe},
		{kDisplayCodePortrait, kDisplay640x870At75Hz, validAndSafe},
		{kDisplayCodePortraitMono, kDisplay640x870At75Hz, validAndSafe},
		{kDisplayCode16Inch, kDisplay832x624At75Hz, validAndSafe},
		{kDisplayCode19Inch, kDisplay1024x768At75Hz, validAndSafe},
		{kDisplayCode21Inch, kDisplay1152x870At75Hz, validAndSafe},
		{kDisplayCode21InchMono, kDisplay1152x870At75Hz, validAndSafe},

		{kDisplayCodeVGA, kDisplay640x480At60HzVGA, validAndSafeAndDefault},
		{kDisplayCodeVGA, kDisplay640x480At120Hz, valid},
		{kDisplayCodeVGA, kDisplay800x600At60HzVGA, valid},
		{kDisplayCodeVGA, kDisplay800x600At72HzVGA, valid},
		{kDisplayCodeVGA, kDisplay800x600At75HzVGA, valid},
		{kDisplayCodeVGA, kDisplay1024x768At60HzVGA, valid},
//		{kDisplayCodeVGA, kDisplay1024x768At72HzVGA, valid},
		{kDisplayCodeVGA, kDisplay1024x768At75HzVGA, valid},
		{kDisplayCodeVGA, kDisplay1280x960At75Hz, valid},
		{kDisplayCodeVGA, kDisplay1280x1024At75Hz, valid},

		{kDisplayCodeNTSC, kDisplay512x384At60HzNTSC, validAndSafe},
		{kDisplayCodeNTSC, kDisplay640x480At60HzNTSC, validAndSafeAndDefault},

		{kDisplayCodePAL, kDisplay640x480At50HzPAL, validAndSafe},
		{kDisplayCodePAL, kDisplay768x576At50HzPAL, validAndSafeAndDefault},

		{kDisplayCodeMultiScanBand1, kDisplay640x480At67Hz, validAndSafeAndDefault},
//		{kDisplayCodeMultiScanBand1, kDisplay800x600At60HzVGA, valid},
//		{kDisplayCodeMultiScanBand1, kDisplay800x600At72HzVGA, valid},
		{kDisplayCodeMultiScanBand1, kDisplay832x624At75Hz, validAndSafe},

		{kDisplayCodeMultiScanBand2, kDisplay640x480At67Hz, validAndSafe},
		{kDisplayCodeMultiScanBand2, kDisplay832x624At75Hz, validAndSafeAndDefault},
//		{kDisplayCodeMultiScanBand2, kDisplay1024x768At60HzVGA, valid},
//		{kDisplayCodeMultiScanBand2, kDisplay1024x768At72HzVGA, valid},
		{kDisplayCodeMultiScanBand2, kDisplay1024x768At75Hz, validAndSafe},

		{kDisplayCodeMultiScanBand3, kDisplay640x480At67Hz, validAndSafe},
		{kDisplayCodeMultiScanBand3, kDisplay640x480At120Hz, valid},
		{kDisplayCodeMultiScanBand3, kDisplay832x624At75Hz, validAndSafe},
//		{kDisplayCodeMultiScanBand3, kDisplay1024x768At60HzVGA, valid},
//		{kDisplayCodeMultiScanBand3, kDisplay1024x768At72HzVGA, valid},
		{kDisplayCodeMultiScanBand3, kDisplay1024x768At75Hz, validAndSafe},
		{kDisplayCodeMultiScanBand3, kDisplay1152x870At75Hz, validAndSafeAndDefault},
		{kDisplayCodeMultiScanBand3, kDisplay1280x960At75Hz, valid},
		{kDisplayCodeMultiScanBand3, kDisplay1280x1024At75Hz, valid},

		{kDisplayCodeDDCC, kDisplay640x480At120Hz, notValid}
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

	for (i = 0; i < maxTimingTableEntries; i++)
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
		err = TemplateGetDisplayData( true, displayModeID, kDepthMode1, k4MegVRAM,
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
	GDXErr err = TemplateGetDisplayData( true, displayModeID, kDepthMode1, k4MegVRAM,
									 nil, nil, &info );
	if (!err)
	{
		*verticalLines = info.height;
		*horizontalPixels = info.width;
		*refreshRate = info.refreshRate;
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

	#warning "test this"

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
//		<> rowBytes			on input, rowbytes contains the horizontal pixels for the dispalyModeID
//							on output, width of each row of video memory for the given depthMode
//
//=====================================================================================================
GDXErr GraphicsHALGetVideoParams(DisplayModeID displayModeID, DepthMode depthMode,
		UInt32 *bitsPerPixel, SInt16 *rowBytes,  UInt32 *horizontalPixels, UInt32 *verticalLines, Fixed *refreshRate)
{
	GDXErr err;
	
	err = GraphicsHALGetResolutionAndFrequency( displayModeID, horizontalPixels, verticalLines, refreshRate );
	if (err)
		goto ErrorExit;
	*rowBytes = *horizontalPixels;

	err = GraphicsHALMapDepthModeToBPP(depthMode, bitsPerPixel);
	if (err)
		goto ErrorExit;

	switch (*bitsPerPixel)
	{
		case 8:
			 *rowBytes = *rowBytes + kRowBytesOffset;
			break;
		case 16:
			*rowBytes = (*rowBytes * 2) + kRowBytesOffset;
			break;
		case 32:
			*rowBytes = (*rowBytes * 4) + kRowBytesOffset;
			break;
		default:
			err = kGDXErrInvalidParameters;				// invalid depth...shouldn't happen
	}

ErrorExit:

	return err;
}



//=====================================================================================================
//
// GraphicsHALGetMaxDepthMode()
//
// This takes a 'displayModeID' and returns the maximum depthMode that is supported by the
// hardware for that 'displayModeID'  NO check is made to determine if the 'displayModeID' is
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

	GDXErr err = TemplateGetDisplayData(true, displayModeID, kDepthMode1, templateHALData->vramSize,
									 nil, nil, &info);

	if (err)										// unknown displayModeID OR vramSize, vramWidth32 are
		goto ErrorExit;								// are really screwed up

	if (templateHALData->vramSize == k4MegVRAM)
		*maxDepthMode = info.maxDepthMode[k4MegVRAMIndex];
	else
		*maxDepthMode = info.maxDepthMode[k2MegVRAMIndex];

	if ( templateHALData->monoOnly )
		*maxDepthMode = kDepthMode1;	// if on a mono only display, never do more than 256 colors

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
		case kDepthMode1:
			*bitsPerPixel = 8;
			break;
		case kDepthMode2:
			*bitsPerPixel = 16;
			break;
		case kDepthMode3:
			*bitsPerPixel = 32;
			break;
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
	GDXErr err = kGDXErrUnknownError;			// Assume failure.
	
	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	
	DepthMode maxDepthMode;						// the max depthMode for the DisplayModeID

	*modePossible = false;						// Assume mode not possible

	// Check to see if the requested page is valid.
	
	if (page < 0 || page > 1)
	{
		err = kGDXErrInvalidParameters;
		goto ErrorExit;
	}


	if (page != templateHALData->currentPage &&
		templateHALData->vramUsageMode != 3)
	{
		err = kGDXErrInvalidParameters;
		goto ErrorExit;
	}


	// Find the maximum depthMode for the displayModeID
	// If the displayModeID is not supportted by hardware, GetMaxDepthMode() returns
	// kGDXErrDisplayModeIDUnsupported 

	err = GraphicsHALGetMaxDepthMode(displayModeID, &maxDepthMode);

	if (err)
		goto ErrorExit;


	if (maxDepthMode >= depthMode)
	{
		*modePossible = true;
	}

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
//		• Perform required steps to determine what display is connected (e.g., read sense lines)
//		• Update the HAL's state information regarding the type of display connected (if HAL
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
	VRAMSize vramSize;

	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	if (!templateHALData->fVRAMBank1 || !templateHALData->fVRAMBank2)
		vramSize = k2MegVRAM;
	else
		vramSize = k4MegVRAM;

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

			ControlWriteRegister(kRegFieldControlCBlankDisable, 1); // disable CBlank
			if ((vramSize == k2MegVRAM) && templateHALData->hasSixty6)
				ControlWriteRegister(kRegFieldControlStandardBankDisable, 1);
			GraphicsHALProgramHardware(kDisplay640x480At67Hz, kDepthMode1, 0, &directColor, &baseAddress);
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
		TemplateSetSomeRegisters(2);
		if (templateHALData->fVRAMBank1 && templateHALData->hasSixty6)
		{
			TemplateSetSixty6CanRun(&templateHALData->regEntryID_sixty6, true);
			TemplateAssertVideoReset();
		}
	}
	else
		TemplateSetSomeRegisters(templateHALData->vramUsageMode);

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
//			• Drive sense line 'A' low and read the values of 'B' and 'C'.  
//			• Drive sense line 'B' low and read the values of 'A' and 'C'.
//			• Drive sense line 'C' low and read the values of 'A' and 'B'.
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
	SynchronizeIO();
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
	SynchronizeIO();
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
	SynchronizeIO();
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
	SynchronizeIO();
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
	SynchronizeIO();
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
	SynchronizeIO();
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
	SynchronizeIO();
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
//	For example, the 'kIndexedMultiScanBand3' has a default 'displayModeID' of kDisplay1152x870At75Hz.
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
		{kDisplayCodeUnknown, kDisplay640x480At67Hz},
		{kDisplayCode12Inch, kDisplay512x384At60Hz},
		{kDisplayCodeStandard, kDisplay640x480At67Hz},
		{kDisplayCodePortrait, kDisplay640x870At75Hz},
		{kDisplayCodePortraitMono, kDisplay640x870At75Hz},
		{kDisplayCode16Inch, kDisplay832x624At75Hz},
		{kDisplayCode19Inch, kDisplay1024x768At75Hz},
		{kDisplayCode21Inch, kDisplay1152x870At75Hz},
		{kDisplayCode21InchMono, kDisplay1152x870At75Hz},
		{kDisplayCodeVGA, kDisplay640x480At60HzVGA},
		{kDisplayCodeNTSC, kDisplay640x480At60HzNTSC},
		{kDisplayCodePAL, kDisplay768x576At50HzPAL},
		{kDisplayCodeMultiScanBand1, kDisplay640x480At67Hz},
		{kDisplayCodeMultiScanBand2, kDisplay832x624At75Hz},
		{kDisplayCodeMultiScanBand3, kDisplay1152x870At75Hz},
		{kDisplayCodeDDCC, kDisplay640x480At60HzVGA},
		{kDisplayCode16, kDisplay640x480At67Hz},
	};
	enum { maxDefaultTableEntries = sizeof(defaultResolutionTable) / sizeof(DefaultResolutionTable) };

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
		if (templateHALData->supports640x480At67Hz)
			*displayModeID = kDisplay640x480At67Hz;
		else if (templateHALData->supports832x624At75Hz)
			*displayModeID = kDisplay832x624At75Hz;
		else if (templateHALData->supports800x600At75Hz)
			*displayModeID = kDisplay800x600At75HzVGA;
		else if (templateHALData->supports800x600At60Hz)
			*displayModeID = kDisplay800x600At60HzVGA;
		else if (templateHALData->supports800x600At72Hz)
			*displayModeID = kDisplay800x600At72HzVGA;
		else if (templateHALData->supports1024x768At75Hz)
			*displayModeID = kDisplay1024x768At75HzVGA;
		else if (templateHALData->supports1024x768At70Hz)
			*displayModeID = kDisplay1024x768At70Hz;
		else if (templateHALData->supports1024x768At60Hz)
			*displayModeID = kDisplay1024x768At60HzVGA;
		else if (templateHALData->supports1152x870At75Hz)
			*displayModeID = kDisplay1152x870At75Hz;
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
	#warning "figure out bool1 and bool2"

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
					( rawSenseCode == kRSCSix || rawSenseCode == kRSCTwo ) &&
					(
						extendedSenseCode == kESCSixStandard || // 0x2B
						extendedSenseCode == kESCTwo12Inch || // 0x21
						extendedSenseCode == 0x22 ||
						extendedSenseCode == 0x29
					)
				)
				||
				(
					( rawSenseCode == kRSCSeven || rawSenseCode == kRSCThree ) &&
					(
						extendedSenseCode == kESCSevenDDC || // 0x3E
						extendedSenseCode == kESCThree21InchMonoRadius || // 0x34
						extendedSenseCode == 0x36 ||
						extendedSenseCode == 0x3D
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
			if ( extendedSenseCode == 0x29 ||
				extendedSenseCode == kESCTwo12Inch ||
				extendedSenseCode == 0x22 ||
				extendedSenseCode == kESCSixStandard )
			{
				*bool2 = true;
				if (graphicsPreferred.displayCode == kDisplayCodeStandard ||
						graphicsPreferred.displayCode == kDisplayCode12Inch)
					*bool1 = false;
				else
					*bool1 = true;
			}
		}
		else if ( rawSenseCode == kRSCSeven || rawSenseCode == kRSCThree )
		{
			if ( extendedSenseCode == kESCSevenDDC ||
				extendedSenseCode == kESCThree21InchMonoRadius ||
				extendedSenseCode == 0x36 ||
				extendedSenseCode == 0x3D )
			{
				*bool2 = true;
				if (graphicsPreferred.displayCode == kDisplayCodeStandard ||
						graphicsPreferred.displayCode == kDisplayCode21InchMono ||
						graphicsPreferred.displayCode == kDisplayCodeUnknown)
					*bool1 = false;
				else
					*bool1 = true;
			}
		}
	}
}



//=====================================================================================================
//
// WriteToCuda_addr_reg_data()
//
//=====================================================================================================

// See AppleCudaCommands.h
#define ADB_PACKET_PSEUDO			1
#define ADB_PSEUDOCMD_GET_SET_IIC   0x22

// See technote 1079
typedef struct {
	UInt8		pbCmdType;		// Command Type, always 'pseudoPkt'
	UInt8		pbCmd;			// Command
	union {						// parameter to pass
		UInt8	pByte[4];
		UInt16	pWord[2];
		UInt32	pLong;
	} pbParam;
	UInt16		pbByteCnt;		// Number of bytes passed in buffer
	UInt8		*pbBufPtr;		// Pointer to a buffer.
	UInt8		pbFlags;		// Flags returned by Cuda
	UInt8		pbSpare;		// reserved
	SInt16		pbResult;		// Result code returned by Cuda
	ProcPtr		pbCompletion;	// Routine to be called on completion
} CudaPB, *CudaPbPtr;

static OSErr Cuda_CudaPB(CudaPB *cudaPB);



static OSErr WriteToCuda_addr_reg_data(UInt8* addr, UInt8* reg, UInt8* somebyte)
{
	CudaPB cudaPB;
	UInt8 theBuffer[2];
	
	theBuffer[0] = 1;
	theBuffer[1] = *somebyte;

	cudaPB.pbCmdType = ADB_PACKET_PSEUDO; // pseudoPkt;
	cudaPB.pbCmd = ADB_PSEUDOCMD_GET_SET_IIC;
	cudaPB.pbParam.pByte[0] = *addr;
	cudaPB.pbParam.pByte[1] = *reg;
	cudaPB.pbParam.pByte[2] = 0;
	cudaPB.pbParam.pByte[3] = 0;
	cudaPB.pbByteCnt = 2;
	cudaPB.pbBufPtr = theBuffer;
	cudaPB.pbFlags = 0;
	cudaPB.pbResult = 0;
	return Cuda_CudaPB(&cudaPB);
}



//=====================================================================================================
//
// TemplateCalcPageBaseAddress()
//
//=====================================================================================================
static GDXErr TemplateCalcPageBaseAddress(Ptr* baseAddress)
{
	UInt32 apertureOffset;
	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	switch (templateHALData->vramUsageMode)
	{
		case 0:
			if (templateHALData->vramSize == k4MegVRAM)
				apertureOffset = 0x00800000;
			else
				apertureOffset = 0x00C00000;
			break;
		case 1:
			apertureOffset = 0x00800000;
			break;
		case 2:
			apertureOffset = 0x00E00000;
			break;
		case 3:
			if (templateHALData->currentPage == 0)
				apertureOffset = 0x00C00000;
			else
				apertureOffset = 0x00E00000;
			break;
		default:
			apertureOffset = 0x00800000;
	}
	
	
	{

/*
#if isForMacOSX
#else
		// The original control ndrv and the one patched for the original version of OS X used the "|" operator
		// instead of "+" in the calculations below. "|" does not work in OS versions where the logical and
		// phyiscal addresses are not the same (versions ≥ Panther)   
#endif
*/
		*baseAddress = (Ptr)(templateHALData->vramBaseAddress + apertureOffset + 0x210);
		if (templateHALData->vramUsageMode == 3)
		{
			templateHALData->baseAddressPage0 = (Ptr)(templateHALData->vramBaseAddress + 0x00C00000 + 0x210);
			templateHALData->baseAddressPage1 = (Ptr)(templateHALData->vramBaseAddress + 0x00E00000 + 0x210);
		}
		else
		{
			templateHALData->baseAddressPage1 = nil;
			templateHALData->baseAddressPage0 = (Ptr)(templateHALData->vramBaseAddress + apertureOffset + 0x210);
		}
	}

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
//		The resulting base address of the frame buffer’s ram. In the event of an error, it is undefined.
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
	
	UInt16 width;

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
	err = TemplateGetDisplayData(true, displayModeID, depthMode, templateHALData->vramSize, &bdiData, &vwdData, &info);
	if (err)
	{
		err = kGDXErrDisplayModeIDUnsupported;
		goto ErrorExit;
	}

	// Check if data is valid: check that the maxDepthMode is greater than or equal to the requested DepthMode
	if (vwdData.clockConfiguration == 0 && vwdData.timingAdjust == 0 && vwdData.spurControl == 0)
	{
		err = kGDXErrDepthModeUnsupported;
		goto ErrorExit;
	}

	// Save any HAL data that might have changed
	templateHALData->depthMode = depthMode;
	width = info.width;
	templateHALData->width = width;
	templateHALData->displayModeID = displayModeID;
	templateHALData->currentPage = page;
	templateHALData->height = info.height;

	if (!templateHALData->usingCustomClutDelay)
	{
		Nanoseconds nanoseconds;
		nanoseconds.lo = bdiData.nsCLUTAddrRegDelay;
		nanoseconds.hi = 0;
		templateHALData->absCLUTAddrRegDelay = NanosecondsToAbsolute(nanoseconds);
	}

	width = (width + 31) & ~31;
	if (depthMode == kDepthMode1)
		templateHALData->rowBytes = width + kRowBytesOffset;
	else if (depthMode == kDepthMode2)
		templateHALData->rowBytes = width * 2 + kRowBytesOffset;
	else
		templateHALData->rowBytes = width * 4 + kRowBytesOffset;

	// Start programing the hardware to the desired state.  For all these function calls, we are
	// explicitly ignoring any errors, since we have passed 'the point of no return'
	
	vblInterruptsEnabled = GraphicsOSSSetVBLInterrupt(false);

	// turn hardware cursor off
	*templateHALData->spur.address = kSpurControl;
	SynchronizeIO();
	*templateHALData->spur.multiPort &= ~kSpurControlCursorEnableMask;
	SynchronizeIO();
	templateHALData->hardwareCursorData.cursorRendered = false;
	templateHALData->hardwareCursorData.cursorVisible = false;

	// When only the depthMode changes, there is no need to put Toynbee in reset.  That would
	// cause the screen to go black.  When the resolution changes, however, Toynbee must be reset
	// or the display won't work.  If the clut is off, however, never assert reset nor release
	// reset(since releasing reset would use more power).  Toynbee will
	// be taken out of reset when the monitor is turned back on.

	if (resolutionUnchanged)
	{
		(void) TemplateSetupCLUT(&templateHALData->spur, &vwdData);
		TemplateSetupFBController_2(depthMode, &info, &vwdData);
	}
	else
	{
		(void) TemplateAssertVideoReset();
		(void) TemplateSetupClockGenerator(&bdiData);
		(void) TemplateSetupCLUT(&templateHALData->spur, &vwdData);
		(void) TemplateSetupFBController(depthMode, &info, &bdiData, &vwdData);
		(void) TemplateReleaseVideoReset();
	}

	if (templateHALData->needsEnableCBlank)
	{
		ControlWriteRegister(kRegFieldControlCBlankDisable, 0); // enable CBlank
		templateHALData->needsEnableCBlank = false;
	}

	if (vblInterruptsEnabled)
		GraphicsOSSSetVBLInterrupt(true);

	ControlWriteRegister(kRegFieldControlINT_ENABLE_1, 0);
	SynchronizeIO();
	ControlWriteRegister(kRegFieldControlINT_ENABLE_1, 1);

	// Base address that is reported to QD
	TemplateCalcPageBaseAddress(&templateHALData->baseAddressPageCurrent);

	*baseAddress = templateHALData->baseAddressPageCurrent;
	if (kDepthMode1 == depthMode)
		*directColor = false;								// 8 bpp
	else
		*directColor = true;								// 16 or 32 bpp

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

	// Get data for hardware cursor
	pHardwareCursorData = &(templateHALData->hardwareCursorData);

	// Check if the hardware cursor buffer must be cleared.
	if (false == pHardwareCursorData->cursorCleared)
	{
		// set hardware cursor buffer area to transparent
		pCursorImage = (UInt32 *) (((Ptr) (templateHALData->baseAddressPageCurrent)) - kHardwareCursorOffset);
		cursorRowBytes = templateHALData->rowBytes;
		for (i = 0; i < templateHALData->height; i++)
		{
			for (j = 0; j < 4; j++)
				*pCursorImage++ = 0;
			pCursorImage = (UInt32 *) ((UInt32) pCursorImage + cursorRowBytes - kHardwareCursorOffset);
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
	AbsoluteTime absCLUTAddrRegDelay = templateHALData->absCLUTAddrRegDelay;				// 800ns in absolute time to hit clut
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
//	the cursor’s visible state and set state should be false. After a mode change the cursor should be
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
	SpurRegisters				*spur = &templateHALData->spur;

	// Get data for hardware cursor.
	pHardwareCursorData = &(templateHALData->hardwareCursorData);

	if ( !pHardwareCursorData->cursorSet )
	{
		*spur->address = kSpurControl;
		SynchronizeIO();
		*spur->multiPort &= ~kSpurControlCursorEnableMask;
		SynchronizeIO();
	}
	else
	{
		if ( !pHardwareCursorData->cursorVisible )
		{
			*spur->address = kSpurControl;
			SynchronizeIO();
			*spur->multiPort &= ~kSpurControlCursorEnableMask;
			SynchronizeIO();
		}
	}
	if (templateHALData->setCursorClutEntriesPending)
	{
		*spur->address = 0;								// Start at CLUT entry 0
		SynchronizeIO();
		for ( i = 0; i < 8; i++ )
		{
			DelayForHardware( templateHALData->absCLUTAddrRegDelay );
			*spur->cursorPaletteRAM = pHardwareCursorData->transformedColorMap[i].rgb.red;
			SynchronizeIO();
			*spur->cursorPaletteRAM = pHardwareCursorData->transformedColorMap[i].rgb.green;
			SynchronizeIO();
			*spur->cursorPaletteRAM = pHardwareCursorData->transformedColorMap[i].rgb.blue;
			SynchronizeIO();
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
	pHardwareCursorFrameBuffer = templateHALData->baseAddressPageCurrent - kHardwareCursorOffset;
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

	*(templateHALData->spur.address) = kSpurCursorXPositionLow;
	SynchronizeIO();
	*(templateHALData->spur.multiPort) = x & 0xff;
	SynchronizeIO();
	*(templateHALData->spur.address) = kSpurCursorXPositionHigh;
	SynchronizeIO();
	*(templateHALData->spur.multiPort) = (x & 0x0f00) >> 8;
	SynchronizeIO();

	pHardwareCursorData->deferredMove = false;
	if ( pHardwareCursorData->cursorVisible )
	{
		*spur->address = kSpurControl;
		SynchronizeIO();
		*spur->multiPort |= kSpurControlCursorEnableMask;
		SynchronizeIO();
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
	AbsoluteTime absCLUTAddrRegDelay = templateHALData->absCLUTAddrRegDelay;
	SpurRegisters *spur = &templateHALData->spur;
	UInt32 i;

	*spur->address = startPosition;
	SynchronizeIO();
	if (absCLUTAddrRegDelay.lo != 0)
		(void) DelayForHardware(absCLUTAddrRegDelay);

	for (i = startPosition; i <= endPosition; i++)
	{
		*spur->colorPaletteRAM = templateHALData->savedClut[i].red;
		SynchronizeIO();
		*spur->colorPaletteRAM = templateHALData->savedClut[i].green;
		SynchronizeIO();
		*spur->colorPaletteRAM = templateHALData->savedClut[i].blue;
		SynchronizeIO();
		if (absCLUTAddrRegDelay.lo != 0)
			(void) DelayForHardware(absCLUTAddrRegDelay);
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
	SpurRegisters *spur = &templateHALData->spur;
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

	*spur->address = kSpurControl;
	SynchronizeIO();

	*spur->multiPort &= ~kSpurControlCursorEnableMask;
	SynchronizeIO();

	pHardwareCursorData->cursorRendered = false;
	pHardwareCursorData->cursorVisible = false;

	vblInterruptsEnabled = GraphicsOSSSetVBLInterrupt(false);
	TemplateWaitForVBL();

	*spur->address = kSpur0x21;
	SynchronizeIO();
	switch (templateHALData->vramUsageMode)
	{
		case 0:
			*spur->multiPort = kSpur0x21Value1;
			break;
		case 3:
			if (templateHALData->currentPage == 0)
				*spur->multiPort = kSpur0x21Value1;
			else
				*spur->multiPort = kSpur0x21Value0;
			break;
		case 1:
		case 2:
			*spur->multiPort = kSpur0x21Value0;
			break;
	}

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

	vdDisplayTimingRange->csMinPixelClock =  11737500; // Hz = 31.3MHz ÷ 4 * 1.5; min in OS 9 driver is 12.27 MHz
	vdDisplayTimingRange->csMaxPixelClock = 156000000; // Hz = 31.3MHz ÷ 1 * 5.0; max in OS 9 driver is 135 MHz

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
	#warning "figure out how to do interlaced"

	BitDepthIndependentData bdiData;
	WidthAndDepthDependentData vwdData;
	DisplayInfo info;
	UInt64 sourceClock;
	int horizontalShift;

	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	GDXErr err = kGDXErrUnknownError;								// Assume failure
	
	if ( vdDetailedTiming->csTimingSize < sizeof(VDDetailedTimingRec) ) // Reserved in Mac OS X
	{
		err = kGDXErrInvalidParameters;
		goto ErrorExit;
	}

	err = TemplateGetDisplayData( false, vdDetailedTiming->csDisplayModeID, kDepthMode1, k4MegVRAM,
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

	switch (bdiData.P2Mux >> 4)
	{
		case 0:		sourceClock = 31300000; break;
		case 5:		sourceClock = 25925926; break;
		case 6:		sourceClock = 16247467; break;
		default:	sourceClock = 31300000; break;
	}
	vdDetailedTiming->csPixelClock = sourceClock * bdiData.N2 / ((UInt32)bdiData.D2 << (3-(bdiData.P2Mux & 3)));    /* Hz*/

	vdDetailedTiming->csMinPixelClock = vdDetailedTiming->csPixelClock;        /* Hz - With error what is slowest actual clock */
	vdDetailedTiming->csMaxPixelClock = vdDetailedTiming->csPixelClock;        /* Hz - With error what is fasted actual clock */

	horizontalShift = (vwdData.spurControl >> 6) + 1;
	vdDetailedTiming->csHorizontalActive = (bdiData.horizontalFrontPorch - bdiData.horizontalActiveLine) << horizontalShift;     /* Pixels*/
	vdDetailedTiming->csHorizontalBlanking = (bdiData.horizontalSyncPulse - bdiData.horizontalFrontPorch + // fp
											  bdiData.horizontalBreezeway + 1 + // sync
											  bdiData.horizontalActiveLine - bdiData.horizontalBreezeway // bp
											  ) << horizontalShift;   /* Pixels*/
	vdDetailedTiming->csHorizontalSyncOffset = (bdiData.horizontalSyncPulse - bdiData.horizontalFrontPorch - 6) << horizontalShift; /* Pixels*/
	vdDetailedTiming->csHorizontalSyncPulseWidth = (bdiData.horizontalBreezeway + 1) << horizontalShift; /* Pixels*/

	vdDetailedTiming->csVerticalActive = (bdiData.verticalFrontPorch - bdiData.verticalActiveLine) >> bdiData.notInterlaced;       /* Lines*/
	vdDetailedTiming->csVerticalBlanking = (bdiData.verticalSync - bdiData.verticalFrontPorch + // fp
											  bdiData.verticalHalfLine - bdiData.verticalSync + bdiData.verticalBackPorchEqualization + // sync
											  bdiData.verticalActiveLine - bdiData.verticalBackPorchEqualization // bp
											  ) >> 1;     /* Lines*/
	vdDetailedTiming->csVerticalSyncOffset = (bdiData.verticalSync - bdiData.verticalFrontPorch) >> 1;   /* Lines*/
	vdDetailedTiming->csVerticalSyncPulseWidth = (bdiData.verticalHalfLine - bdiData.verticalSync + bdiData.verticalBackPorchEqualization) >> 1; /* Lines*/

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
static DepthMode CalcMaxDepthMode( UInt64 pixelClock, int horizontalDivide, UInt32 ramMB, UInt32 width, UInt32 height )
{
	DepthMode maxDepthModeRam;
	DepthMode maxDepthModeClock;

	UInt32 frameBytes = ( (width + 31) & ~31 ) * height;
	switch ( ramMB * 1024 * 1024 / frameBytes )
	{
		case 0:		maxDepthModeRam = 0; break;
		case 1:		maxDepthModeRam = kDepthMode1; break;
		case 2:
		case 3:		maxDepthModeRam = kDepthMode2; break;
		default:	maxDepthModeRam = kDepthMode3; break;
	}


	switch ( ramMB * 160000000 / pixelClock ) // see description of ram chips (TC528257J70) above
	{
		case 0:		maxDepthModeClock = 0; break;
		case 1:		maxDepthModeClock = kDepthMode1; break;
		case 2:
		case 3:		maxDepthModeClock = kDepthMode2; break;
		default:	maxDepthModeClock = kDepthMode3; break;
	}
	
	if ( maxDepthModeRam > maxDepthModeClock )
		maxDepthModeRam = maxDepthModeClock;

	if ( maxDepthModeRam == kDepthMode3 && horizontalDivide > 2 )
		maxDepthModeRam = kDepthMode2;

	return maxDepthModeRam;
}



static void CalculateBestRatio( UInt64 wantedDotClockHz, UInt8* bestn2p, UInt8* bestd2p, UInt8* bestp2muxp )
{
	const double srcClockHz = 31300000.0;
	const double minratio = 1.6;

	Boolean gotclockdiff;
	double bestclockdiff;
	int n2, d2, bestn2, bestd2;

	double wantedratio = wantedDotClockHz / srcClockHz;
	int bestp2mux;
	for ( bestp2mux = 3; bestp2mux > 1 && wantedratio < minratio; bestp2mux--)
		wantedratio *= 2;

	gotclockdiff = false;

	for (n2 = 1; n2 <= 127; n2++)
	{
		for (d2 = 1; d2 <= n2; d2++)
		{
			double dotclock = srcClockHz * n2 / d2 / (1 << (3-bestp2mux));
			double clockdiff = fabs( dotclock - wantedDotClockHz );

			if (gotclockdiff == false || clockdiff < bestclockdiff)
			{
				gotclockdiff = true;
				bestclockdiff = clockdiff;
				bestn2 = n2;
				bestd2 = d2;
			}
		} // for d2
	} // for n2

	*bestn2p = bestn2;
	*bestd2p = bestd2;
	*bestp2muxp = bestp2mux;
}



GDXErr GraphicsHALSetDetailedTiming(VDDetailedTimingRec *vdDetailedTiming)
{
	#warning "figure out how to do interlaced using SwitchRes or something"
	
	BitDepthIndependentData bdiData;
	WidthAndDepthDependentData vwdData[kVRAMNumIndexes][kDepthModeNumIndexes];
	DisplayInfo info;
	int horizontalDivide;
	int i;
	int vramSizeNdx;
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
	info.refreshRate = ( vdDetailedTiming->csPixelClock << 16 ) / ( (UInt64)( vdDetailedTiming->csHorizontalActive + vdDetailedTiming->csHorizontalBlanking ) * ( vdDetailedTiming->csVerticalActive + vdDetailedTiming->csVerticalBlanking ) );

	info.maxDepthMode[k2MegVRAMIndex] = 0;
	info.maxDepthMode[k4MegVRAMIndex] = 0;

	info.width = vdDetailedTiming->csHorizontalActive;
	info.height = vdDetailedTiming->csVerticalActive;


	// ATHENS
	CalculateBestRatio( vdDetailedTiming->csPixelClock, &bdiData.N2, &bdiData.D2, &bdiData.P2Mux );
	bdiData.notInterlaced = true;

	// CONTROL
	bdiData.interlaced = false;
	bdiData.ControlTEST_4 = false;
	bdiData.hSyncPolarity = ( vdDetailedTiming->csHorizontalSyncConfig & kSyncPositivePolarityMask ) != 0;
	bdiData.vSyncPolarity = ( vdDetailedTiming->csVerticalSyncConfig & kSyncPositivePolarityMask ) != 0;
	bdiData.cSyncDisable = true;

	horizontalDivide = 2;
	#warning "maybe take pixel clock and ram width into account here?"

	for ( ; horizontalDivide <= 4; horizontalDivide += 2 )
	{
		bdiData.horizontalBreezeway = vdDetailedTiming->csHorizontalSyncPulseWidth / horizontalDivide - 1;
		bdiData.horizontalActiveLine = vdDetailedTiming->csHorizontalBlanking / horizontalDivide - vdDetailedTiming->csHorizontalSyncOffset / horizontalDivide - 6 - 1;
		bdiData.horizontalFrontPorch = vdDetailedTiming->csHorizontalActive / horizontalDivide + bdiData.horizontalActiveLine;
		bdiData.horizontalSyncPulse = vdDetailedTiming->csHorizontalSyncOffset / horizontalDivide + 6 + bdiData.horizontalFrontPorch;
		bdiData.horiztonalPixelCount = bdiData.horizontalSyncPulse - 1;
		bdiData.horizontalSerration = ( vdDetailedTiming->csHorizontalActive + vdDetailedTiming->csHorizontalBlanking - vdDetailedTiming->csHorizontalSyncPulseWidth ) / horizontalDivide;
		bdiData.halfLine = ( vdDetailedTiming->csHorizontalActive + vdDetailedTiming->csHorizontalBlanking ) / (2 * horizontalDivide);
		bdiData.horizontalEqualization = ( bdiData.horizontalBreezeway + 1 ) / 2;
		
		#warning "find the real max"
		if ( bdiData.horizontalSyncPulse /* < (1 << 12) */ /* <= 727 */ < 839 ) // max allowed is maybe in the range [727..839)   727 is used by 1152x870. 839 would be used by 1280x960 if it used a horizontalDivide of 2. 1<<12=4096
			break;
	}

	if ( bdiData.horizontalSyncPulse >= (1 << 12) )
	{
		err = kGDXErrInvalidParameters;
		goto ErrorExit;
	}

	for ( vramSizeNdx = k2MegVRAMIndex; vramSizeNdx < kVRAMNumIndexes; vramSizeNdx++ )
		info.maxDepthMode[vramSizeNdx] = CalcMaxDepthMode( vdDetailedTiming->csPixelClock, horizontalDivide, (vramSizeNdx + 1) * 2, vdDetailedTiming->csHorizontalActive, vdDetailedTiming->csVerticalActive );

	if ( info.maxDepthMode[k4MegVRAMIndex] == 0 )
	{
		err = kGDXErrInvalidParameters;
		goto ErrorExit;
	}

	bdiData.verticalBackPorchEqualization = ( vdDetailedTiming->csVerticalSyncPulseWidth - 1 ) * 2;
	bdiData.verticalActiveLine = vdDetailedTiming->csVerticalBlanking * 2 + bdiData.verticalBackPorchEqualization - vdDetailedTiming->csVerticalSyncPulseWidth * 2 - vdDetailedTiming->csVerticalSyncOffset * 2;
	bdiData.verticalFrontPorch = vdDetailedTiming->csVerticalActive * 2 + bdiData.verticalActiveLine;
	bdiData.verticalSync = vdDetailedTiming->csVerticalSyncOffset * 2 + bdiData.verticalFrontPorch;
	bdiData.verticalHalfLine = ( vdDetailedTiming->csVerticalBlanking + vdDetailedTiming->csVerticalActive ) * 2;
	bdiData.verticalBackPorch = bdiData.verticalActiveLine/2 + ( vdDetailedTiming->csVerticalSyncPulseWidth - 1 );
	bdiData.verticalFrontPorchEqualization = bdiData.verticalSync - vdDetailedTiming->csVerticalSyncOffset;

	if ( bdiData.verticalHalfLine >= (1 << 12) )
	{
		err = kGDXErrInvalidParameters;
		goto ErrorExit;
	}


	if ( vdDetailedTiming->csPixelClock < 25000000 ) // 12.270000…15.667200
		bdiData.nsCLUTAddrRegDelay = 512;
	else if ( vdDetailedTiming->csPixelClock < 30000000 ) // 25.174500
		bdiData.nsCLUTAddrRegDelay = 256;
	else
		bdiData.nsCLUTAddrRegDelay = 128; // 30.241000…135.000000


	for ( depthModeNdx = kDepthMode1Index; depthModeNdx < kDepthModeNumIndexes; depthModeNdx ++ )
	{
		int dividerThingy = 2 - depthModeNdx - (horizontalDivide > 2);
		int pow3;
		switch (dividerThingy) {
			case 0: pow3 = 1; break;
			case 1: pow3 = 3; break;
			case 2: pow3 = 9; break;
		}

		for ( vramSizeNdx = k2MegVRAMIndex; vramSizeNdx < kVRAMNumIndexes; vramSizeNdx++ )
		{
			if ( depthModeNdx + kDepthMode1 <= info.maxDepthMode[vramSizeNdx] )
			{
				vwdData[vramSizeNdx][depthModeNdx].clockConfiguration = vramSizeNdx + dividerThingy;
				vwdData[vramSizeNdx][depthModeNdx].timingAdjust = bdiData.horizontalActiveLine - pow3 - vramSizeNdx*2*dividerThingy;
				vwdData[vramSizeNdx][depthModeNdx].spurControl = ((horizontalDivide > 2) << 6) | ((vramSizeNdx + 1) << 4) | (depthModeNdx << 2);
			}
			else
			{
				vwdData[vramSizeNdx][depthModeNdx].clockConfiguration = 0;
				vwdData[vramSizeNdx][depthModeNdx].timingAdjust = 0;
				vwdData[vramSizeNdx][depthModeNdx].spurControl = 0;
			}
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
// GraphicsHALControlCode140()
//
//=====================================================================================================
typedef struct {
	SInt32 vramUsageMode;
	Ptr baseAddressPage0;
	Ptr baseAddressPage1;
} Control140Struct;


static GDXErr GraphicsHALControlCode140(Control140Struct *genericPtr)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	GDXErr err = kGDXErrNoError;
	Boolean modePossible;

	UInt32 saveVramUsageMode = templateHALData->vramUsageMode;

	VRAMSize saveVramSize = templateHALData->vramSize;
	VRAMSize theVRAMSize;

	if (genericPtr->vramUsageMode < 0 || genericPtr->vramUsageMode > 3)
	{
			err = kGDXErrInvalidParameters;
			goto ErrorExit;
	}

	if (!templateHALData->fVRAMBank1 || !templateHALData->fVRAMBank2)
		theVRAMSize = k2MegVRAM;
	else
		theVRAMSize = k4MegVRAM;

	switch (genericPtr->vramUsageMode)
	{
		default:
			err = kGDXErrInvalidParameters;
			goto ErrorExit;

		case 0:
			if (templateHALData->fVRAMBank1)
			{
				templateHALData->numPages = 1;
				templateHALData->vramSize = theVRAMSize;
			}
			else
				err = kGDXErrInvalidParameters;
			break;

		case 1:
			if (theVRAMSize == k4MegVRAM)
			{
				templateHALData->numPages = 1;
				templateHALData->vramSize = k2MegVRAM;
			}
			else
				err = kGDXErrInvalidParameters;
			break;

		case 2:
			if (templateHALData->fVRAMBank2)
			{
				templateHALData->numPages = 1;
				templateHALData->vramSize = k2MegVRAM;
			}
			else
				err = kGDXErrInvalidParameters;
			break;

		case 3:
			if (theVRAMSize == k4MegVRAM)
			{
				templateHALData->numPages = 2;
				templateHALData->vramSize = k2MegVRAM;
			}
			else
				err = kGDXErrInvalidParameters;
			break;
	}

	if (err)
		goto ErrorExit;

	err = GraphicsHALModePossible(templateHALData->displayModeID, templateHALData->depthMode, templateHALData->currentPage, &modePossible);
	if (err)
		goto ErrorExit;

	if (!modePossible)
		goto ErrorExit;

	if (templateHALData->baseAddressPageCurrent)
	{
		VDPageInfo pageInfo;
		pageInfo.csPage = templateHALData->currentPage;
		err = GraphicsCoreGrayPage(&pageInfo);
		if (err)
			goto ErrorExit;
	}

	templateHALData->vramUsageMode = genericPtr->vramUsageMode;
	TemplateSetSomeRegisters(genericPtr->vramUsageMode);

	{
		VDSwitchInfoRec	switchInfo;
		switchInfo.csMode = templateHALData->depthMode;
		switchInfo.csData = templateHALData->displayModeID;
		switchInfo.csPage = templateHALData->currentPage;

		err = GraphicsCoreSwitchMode(&switchInfo);
	}
	
	if (!err)
	{
		genericPtr->baseAddressPage0 = templateHALData->baseAddressPage0;
		genericPtr->baseAddressPage1 = templateHALData->baseAddressPage1;
	}
	else
	{
ErrorExit:
		templateHALData->vramSize = saveVramSize;
		templateHALData->vramUsageMode = saveVramUsageMode;
		TemplateSetSomeRegisters(saveVramUsageMode);
	}

	return err;
}



//=====================================================================================================
//
// GraphicsHALControlCode141()
//
//=====================================================================================================
typedef struct {
	UInt32 _141_0L;
	UInt32 whatFieldsToSetOrGet;
	Nanoseconds nsCLUTAddrRegDelay;
} Control141Struct;


static GDXErr GraphicsHALControlCode141(Control141Struct *genericPtr)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	Nanoseconds nanoseconds;

	if (genericPtr->whatFieldsToSetOrGet > 3)
		return kGDXErrInvalidParameters;

	if (genericPtr->whatFieldsToSetOrGet & 1)
	{
		Boolean oldSetClutAtVBL = templateHALData->setClutAtVBL;
		Boolean newSetClutAtVBL = (genericPtr->_141_0L & 1) != 0;

		templateHALData->setClutAtVBL = newSetClutAtVBL;
		if (oldSetClutAtVBL && !newSetClutAtVBL && templateHALData->setClutEntriesPending)
		{
			Boolean vblInterruptsEnabled = GraphicsOSSSetVBLInterrupt(false);
			if (templateHALData->setClutEntriesPending)
			{
				TemplateSetCursorColors();
			}
			if (vblInterruptsEnabled)
				GraphicsOSSSetVBLInterrupt(true);
		}
	}

	if (genericPtr->whatFieldsToSetOrGet & 2)
	{
		if (genericPtr->_141_0L & 2)
		{
			nanoseconds = genericPtr->nsCLUTAddrRegDelay;
			templateHALData->absCLUTAddrRegDelay = NanosecondsToAbsolute(nanoseconds);
			templateHALData->usingCustomClutDelay = true;
		}
		else
		{
			BitDepthIndependentData bdiData;			// dependent on resolution

			GDXErr err;
			nanoseconds.hi = 0;
			templateHALData->usingCustomClutDelay = false;
			nanoseconds.lo = 128;
			templateHALData->absCLUTAddrRegDelay = NanosecondsToAbsolute(nanoseconds);
			err = TemplateGetDisplayData( true, templateHALData->displayModeID, templateHALData->depthMode, k4MegVRAM, &bdiData, nil, nil);
			if (!err)
			{
				nanoseconds.lo = bdiData.nsCLUTAddrRegDelay;
				nanoseconds.hi = 0;
				templateHALData->absCLUTAddrRegDelay = NanosecondsToAbsolute(nanoseconds);
			}
		}
	}

	return kGDXErrNoError;
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
OSErr GraphicsHALPrivateControl(void * genericPtr, SInt16 privateControlCode)
{


	GDXErr err = kGDXErrUnknownError;				// Assume failure.
	OSErr returnErr = controlErr;					// Assume cs code is invalid

	switch (privateControlCode)
	{
		default:
			goto ErrorExit;							// csCode not supported

		case 140:
			if (GraphicsHALControlCode140((Control140Struct *)genericPtr) != kGDXErrNoError)
			{
				returnErr = paramErr;
				goto ErrorExit;
			}
			break;

		case 141:
			if (GraphicsHALControlCode141((Control141Struct *)genericPtr) != kGDXErrNoError)
			{
				returnErr = paramErr;
				goto ErrorExit;
			}
			break;
	}
	returnErr = noErr;

ErrorExit:
	return returnErr;
}



//=====================================================================================================
//
// GraphicsHALStatusCode140()
//
//=====================================================================================================
static GDXErr GraphicsHALStatusCode140(Control140Struct* genericPtr)
{
	GDXErr err;
	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	VRAMSize vramSize;
	
	if (!templateHALData->fVRAMBank1 || !templateHALData->fVRAMBank2)
		vramSize = k2MegVRAM;
	else
		vramSize = k4MegVRAM;

	genericPtr->baseAddressPage0 = NULL;
	genericPtr->baseAddressPage1 = NULL;

	switch (genericPtr->vramUsageMode)
	{
		default:
			err = kGDXErrInvalidParameters;
			goto ErrorExit;

		case -1:
			genericPtr->vramUsageMode = 0;

			if (templateHALData->fVRAMBank1)
				genericPtr->vramUsageMode = 1;

			if (templateHALData->fVRAMBank2)
				genericPtr->vramUsageMode |= 4;

			if (vramSize == k4MegVRAM)
				genericPtr->vramUsageMode |= 0xA;

			break;

		case 0:
			genericPtr->vramUsageMode = templateHALData->vramUsageMode;
			genericPtr->baseAddressPage0 = templateHALData->baseAddressPage0;
			genericPtr->baseAddressPage1 = templateHALData->baseAddressPage1;
			break;
	}

	err = kGDXErrNoError;

ErrorExit:
	return err;
}



//=====================================================================================================
//
// GraphicsHALStatusCode141()
//
//=====================================================================================================
static GDXErr GraphicsHALStatusCode141(Control141Struct *genericPtr)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	if (genericPtr->whatFieldsToSetOrGet > 3)
		return kGDXErrInvalidParameters;

	if (genericPtr->whatFieldsToSetOrGet & 1)
		genericPtr->_141_0L = (genericPtr->_141_0L & ~1) | templateHALData->setClutAtVBL;

	if (genericPtr->whatFieldsToSetOrGet & 2)
		genericPtr->nsCLUTAddrRegDelay = AbsoluteToNanoseconds(templateHALData->absCLUTAddrRegDelay);

	return kGDXErrNoError;
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
OSErr GraphicsHALPrivateStatus(void * genericPtr, SInt16 privateStatusCode)
{



	GDXErr err = kGDXErrUnknownError;				// Assume failure.
	OSErr returnErr = controlErr;					// Assume cs code is invalid

	switch (privateStatusCode)
	{
		default:
			goto ErrorExit;							// csCode not supported

		case 140:
			if (GraphicsHALStatusCode140((Control140Struct *)genericPtr) != kGDXErrNoError)
			{
				returnErr = paramErr;
				goto ErrorExit;
			}
			break;

		case 141:
			if (GraphicsHALStatusCode141((Control141Struct *)genericPtr) != kGDXErrNoError)
			{
				returnErr = paramErr;
				goto ErrorExit;
			}
			break;
	}
	returnErr = noErr;

ErrorExit:
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
	MrSanAntonioRegisters *mrSanAntonio = templateHALData->mrSanAntonio;
	UInt32 status;

	if (EndianSwap32Bit(mrSanAntonio->ControlTEST) & 8) // toynbeeRunning
	{
		HWRegister32Bit *interruptStatus = &templateHALData->mrSanAntonio->ControlINT_STATUS;

		// clearCursorInterrupt
		mrSanAntonio->ControlINT_ENABLE = EndianSwap32Bit(0x04);
		SynchronizeIO();
		mrSanAntonio->ControlINT_ENABLE = EndianSwap32Bit(0x0C);
		SynchronizeIO();

		while (true)
		{
			status = EndianSwap32Bit(*interruptStatus);
			if (status & kCursorInterruptStatusMask)
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
	MrSanAntonioRegisters *mrSanAntonio = templateHALData->mrSanAntonio;

	if (templateHALData->setClutEntriesPending && !templateHALData->clutBusy)
	{
		TemplateSetCursorColors();
	}

	mrSanAntonio->ControlINT_ENABLE = 0x04000000;					// clear Template interrupt
	SynchronizeIO();
	mrSanAntonio->ControlINT_ENABLE = 0x0C000000;
	SynchronizeIO();

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
	AbsoluteTime delay = templateHALData->senseLineAndVideoDelay5ms;

	ControlWriteRegister(kRegFieldControlTEST_1_DisableTiming, 1);
	SynchronizeIO();

	ControlWriteRegister(kRegFieldControlHSyncPolarity, 0);	// set polarity of H Sync Negative
	ControlWriteRegister(kRegFieldControlVSyncPolarity, 0);	// set polarity of V Sync Negative
	ControlWriteRegister(kRegFieldControlTEST_6, 1);
	ControlWriteRegister(kRegFieldControlTEST_7, 1);
	ControlWriteRegister(kRegFieldControlTEST_10, 1);
	ControlWriteRegister(kRegFieldControlTEST_11, 1);

	ControlWriteRegister(kRegFieldControlTEST_8_ResetTiming, 0);
	SynchronizeIO();
	DelayForHardware(delay);

	ControlWriteRegister(kRegFieldControlTEST_8_ResetTiming, 1);
	SynchronizeIO();
	DelayForHardware(delay);

	ControlWriteRegister(kRegFieldControlTEST_8_ResetTiming, 0);
	SynchronizeIO();
	DelayForHardware(delay);

	ControlWriteRegister(kRegFieldControlTEST_8_ResetTiming, 1);
	SynchronizeIO();
	DelayForHardware(delay);

	return (kGDXErrNoError);
}



//=====================================================================================================
//
// TemplateSetupClockGenerator()
//	This will program the video clock generator for the Template graphics architecture.
//
//		-> spur			Pointer to structure of register addresses for Spur
//		-> bdiData		The hardware programming parameters that DO NOT vary with bit depth
//
//		<> usingClockSetA
//		As an input, this is 'true' if the current active set of the Frankenstein clock generator is
//		Set A, 'false' if Set B is in use.
//		As an output, this is changed to reflect the current set in use.
//
// static GDXErr TemplateSetupClockGenerator(const SpurRegisters *spur, const BitDepthIndependentData *bdiData,
//		Boolean *usingClockSetA);
//
//=====================================================================================================
static GDXErr TemplateSetupClockGenerator(BitDepthIndependentData* bdiData)
{
	UInt8 addr = 0x50;
	UInt8 reg, somebyte;
	
	reg = 0x01;
	somebyte = bdiData->D2;
	WriteToCuda_addr_reg_data(&addr, &reg, &somebyte);

	reg = 0x02;
	somebyte = bdiData->N2;
	WriteToCuda_addr_reg_data(&addr, &reg, &somebyte);

	reg = 0x03;
	somebyte = bdiData->P2Mux;

	if (bdiData->P2Mux & 0x40)
	{
		UInt8 somebyte2 = bdiData->P2Mux & 0x33;
		WriteToCuda_addr_reg_data(&addr, &reg, &somebyte2);

		SynchronizeIO();
		WriteToCuda_addr_reg_data(&addr, &reg, &somebyte);
	}
	else
		WriteToCuda_addr_reg_data(&addr, &reg, &somebyte);
	return kGDXErrNoError;
}



//=====================================================================================================
//
// TemplateSetupCLUT()
//	This will program the CLUT for the Template graphics architecture.
//
//		-> spur				Pointer to structure of register addresses for Spur
//		-> bddData			The hardware programming parameters that DO vary with bit depth
//
//=====================================================================================================
static GDXErr TemplateSetupCLUT(const SpurRegisters *spur, const WidthAndDepthDependentData *vwdData)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	UInt8 control;										// don't hit cursor enable
	UInt8 newControl = vwdData->spurControl;			// control for new depthMode. 

	newControl &= ~(kSpurControlCursorEnableMask | 1);	// make sure bddData doesn't hit cursor enable
	
	*spur->address = kSpurControl;						// Setup the address register for 'control'
	SynchronizeIO();

	control = *spur->multiPort;
	control &= (kSpurControlCursorEnableMask | 1);		// clear everything except cursor enable
	
	control |= newControl;						
	*spur->multiPort = control;
	SynchronizeIO();

	*spur->address = kSpur0x21;
	SynchronizeIO();
	switch (templateHALData->vramUsageMode)
	{
		case 0:
			*spur->multiPort = kSpur0x21Value1;
			break;
		case 3:
			if (templateHALData->currentPage == 0)
				*spur->multiPort = kSpur0x21Value1;
			else
				*spur->multiPort = kSpur0x21Value0;
			break;
		case 1:
		case 2:
			*spur->multiPort = kSpur0x21Value0;
			break;
	}

	return kGDXErrNoError;
}



//=====================================================================================================
//
// TemplateSetupFBController_2()
//
//=====================================================================================================
static GDXErr TemplateSetupFBController_2(DepthMode depthMode, const DisplayInfo *info, const WidthAndDepthDependentData *vwdData)
{
	#pragma unused( depthMode, info )

	UInt16 rowBytesMultiplier;
	TemplateHALData *templateHALData = GraphicsHALGetHALData();

	// Program MrSanAntonio with the 'WidthAndDepthDependentData' 
	ControlWriteRegister(kRegFieldControlPIPED, vwdData->timingAdjust);

	// Program Toynbee with the 'WidthAndDepthDependentData' 
	ControlWriteRegister(kRegFieldControlGSC_DIVIDE, vwdData->clockConfiguration);

	if (templateHALData->interlaced)
		rowBytesMultiplier = 2;
	else
		rowBytesMultiplier = 1;

	// Program Toynbee with the 'BitDepthDependentData' 
	ControlWriteRegister(kRegFieldControlROW_WORDS, templateHALData->rowBytes * rowBytesMultiplier);

	// Program Toynbee with the 'BitDepthIndependentData'... (could change...always hit)
	ControlWriteRegister(kRegFieldControlGBASE, 0x200);

	return kGDXErrNoError;
}



//=====================================================================================================
//
// TemplateSetupFBController()
//	This will program the frame buffer for the Template graphics architecture.
//
//		-> resolutionChange	true if bdi registers should be hit
//		-> toynbee			pointer to structure of register address for Toynbee
//		-> mrSanAntonio		pointer to structure of register address for MrSanAntonio
//		-> bdiData			The hardware programming parameters that DO NOT vary with bit depth
//		-> bddData			The hardware programming parameters that DO vary with bit depth
//		-> vwData			The hardware programming parameters that DO vary VRAM width
//		-> vwdData			The hardware programming parameters that DO vary VRAM width AND bit depth
//
//=====================================================================================================
static GDXErr TemplateSetupFBController(DepthMode depthMode, const DisplayInfo *info,
		const BitDepthIndependentData* bdiData, const WidthAndDepthDependentData *vwdData)
{
	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	
	// Program MrSanAntonio with the 'BitDepthIndependentData' 
	ControlWriteRegister(kRegFieldControlVFPEQ, bdiData->verticalFrontPorchEqualization);
	ControlWriteRegister(kRegFieldControlVFP, bdiData->verticalFrontPorch);
	ControlWriteRegister(kRegFieldControlVAL, bdiData->verticalActiveLine);
	ControlWriteRegister(kRegFieldControlVBP, bdiData->verticalBackPorch);
	ControlWriteRegister(kRegFieldControlVBPEQ, bdiData->verticalBackPorchEqualization);
	ControlWriteRegister(kRegFieldControlVSYNC, bdiData->verticalSync);
	ControlWriteRegister(kRegFieldControlVHLINE, bdiData->verticalHalfLine);

	ControlWriteRegister(kRegFieldControlHPIX, bdiData->horiztonalPixelCount);
	ControlWriteRegister(kRegFieldControlHFP, bdiData->horizontalFrontPorch);
	ControlWriteRegister(kRegFieldControlHAL, bdiData->horizontalActiveLine);
	ControlWriteRegister(kRegFieldControlHBWAY, bdiData->horizontalBreezeway);
	ControlWriteRegister(kRegFieldControlHSP, bdiData->horizontalSyncPulse);
	ControlWriteRegister(kRegFieldControlHEQ, bdiData->horizontalEqualization);
	ControlWriteRegister(kRegFieldControlHLFLN, bdiData->halfLine);
	ControlWriteRegister(kRegFieldControlHSERR, bdiData->horizontalSerration);
	ControlWriteRegister(kRegFieldControlNotInterlaced, bdiData->notInterlaced);
	ControlWriteRegister(kRegFieldControlInterlaced, bdiData->interlaced);
	ControlWriteRegister(kRegFieldControlTEST_4, bdiData->ControlTEST_4);

	ControlWriteRegister(kRegFieldControlHSyncPolarity, bdiData->hSyncPolarity);	// set polarity of H Sync
	ControlWriteRegister(kRegFieldControlVSyncPolarity, bdiData->vSyncPolarity);	// set polarity of V Sync

	ControlWriteRegister(kRegFieldControlHSyncDisable, 0); // enable horizontal sync pulses
	ControlWriteRegister(kRegFieldControlVSyncDisable, 0); // enable vertical sync pulses

	if (bdiData->cSyncDisable)
	{
		templateHALData->compositSyncDisabled = true; /* composite sync disabled */
		ControlWriteRegister(kRegFieldControlCSyncDisable, 1);
	}
	else
	{
		templateHALData->compositSyncDisabled = false; /* composite sync enabled */
		ControlWriteRegister(kRegFieldControlCSyncDisable, 0);
	}

	if (!templateHALData->compositSyncDisabled)
		templateHALData->cvhSyncDisabled &= 4; /* composite sync enabled */
	else
		templateHALData->cvhSyncDisabled |= 4; /* composite sync disabled */

	if (bdiData->notInterlaced == false)
		templateHALData->interlaced = true;
	else
		templateHALData->interlaced = false;

	TemplateSetupFBController_2(depthMode, info, vwdData);

	if (templateHALData->vramSize == k4MegVRAM)
		ControlWriteRegister(kRegFieldControlWide, 1);
	else
		ControlWriteRegister(kRegFieldControlWide, 0);

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

	ControlWriteRegister(kRegFieldControlTEST_8_ResetTiming, 0);

	ControlWriteRegister(kRegFieldControlTEST_1_DisableTiming, 0);

	ControlWriteRegister(kRegFieldControlTEST_8_ResetTiming, 0);
	SynchronizeIO();
	DelayForHardware(templateHALData->senseLineAndVideoDelay5ms);

	ControlWriteRegister(kRegFieldControlTEST_8_ResetTiming, 1);
	SynchronizeIO();
	DelayForHardware(templateHALData->senseLineAndVideoDelay5ms);

	ControlWriteRegister(kRegFieldControlTEST_8_ResetTiming, 0);
	SynchronizeIO();
	DelayForHardware(templateHALData->senseLineAndVideoDelay5ms);

	ControlWriteRegister(kRegFieldControlTEST_8_ResetTiming, 1);
	SynchronizeIO();
	DelayForHardware(templateHALData->senseLineAndVideoDelay5ms);

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
//		-> vramSize			VRAM in the system (1, 2 or 4 megs)
//		-> vramWidth32		boolean set to true if VRAM width is 32 bits wide
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
static GDXErr TemplateGetDisplayData(Boolean ignoreNotReady, DisplayModeID displayModeID, DepthMode depthMode, VRAMSize vramSize,
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
					
				err = TemplateGet(i, depthMode, vramSize, bdiData, vwdData, info);
				if (bdiData)
					if (displayModeID == kDisplay800x600At72HzVGA)
					{
						TemplateHALData *templateHALData = GraphicsHALGetHALData();
						if (templateHALData->hasFatman)
						{
							bdiData->N2 = 35;
							bdiData->D2 = 11;
							bdiData->P2Mux = 2;
						}
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
//		-> maxDepthIndex	index for maxDepthRay			( k1MegVRAMIndex...k4MegVRAMIndex )
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
static GDXErr TemplateGet(short index, DepthMode depthMode, VRAMSize vramSize,
	BitDepthIndependentData* bdiData, WidthAndDepthDependentData* vwdData, DisplayInfo* info)
{
	GDXErr err = kGDXErrNoError;

	if (info)
		*info = gDisplayModeInfo[ index ].info;
	if (bdiData)
		*bdiData = gDisplayModeInfo[ index ].bdiData;
	if (vwdData)
		*vwdData = gDisplayModeInfo[ index ].vwdData[vramSize][depthMode - kDepthMode1];

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

	MrSanAntonioRegisters *mrSanAntonio = templateHALData->mrSanAntonio;
	mrSanAntonio->ControlMON_SENSE = EndianSwap32Bit(7 << 3);						//  Tristate sense lines

	SynchronizeIO();
	DelayForHardware(templateHALData->senseLineAndVideoDelay5ms);		// wait some amount of time for lines to stabilize
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
	MrSanAntonioRegisters *mrSanAntonio = templateHALData->mrSanAntonio;

	UInt32 senseLineValue;

	senseLineValue = EndianSwap32Bit(mrSanAntonio->ControlMON_SENSE);	// read sense line

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
	kDriveAValue = 0x00000003 << 3, // 011
	kDriveBValue = 0x00000005 << 3, // 101
	kDriveCValue = 0x00000006 << 3  // 110
};

	TemplateHALData *templateHALData = GraphicsHALGetHALData();
	MrSanAntonioRegisters *mrSanAntonio = templateHALData->mrSanAntonio;

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
	
	mrSanAntonio->ControlMON_SENSE = EndianSwap32Bit( senseLineValue );				//  Drive appropriate sense line

	DelayForHardware(templateHALData->senseLineAndVideoDelay5ms);		// wait some amount of time for lines to stabilize

}



//=====================================================================================================
//
// TemplateGetExtendedSenseCode()
//	This routine applies the 'standard' extended sense code algorithm to the sense lines to determine
//	their ExtendedSenseCode.
//
// 	For 'standard' sense line hardware, the extended sense code algorithm is as follows:
//	(Note:  as described here, sense line 'A' corresponds to '2', 'B' to '1', and 'C' to '0')
//		• Drive sense line 'A' low and read the values of 'B' and 'C'.
//		• Drive sense line 'B' low and read the values of 'A' and 'C'.
//		• Drive sense line 'C' low and read the values of 'A' and 'B'.
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
	extendedBC = (extendedBC << 4) & 0x30;						// BC 00 00

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



//=====================================================================================================
//
// Cuda_CudaPB()
//
//=====================================================================================================

static OSErr Cuda_CudaPB(CudaPB *cudaPB)
{
	const UniversalProcPtr* EgretDispatch = (UniversalProcPtr*)0x0648;

	enum {
		uppEgretDispatchProcInfo = kRegisterBased
			| RESULT_SIZE(SIZE_CODE(sizeof(OSErr)))
			| REGISTER_RESULT_LOCATION(kRegisterD0)
			| REGISTER_ROUTINE_PARAMETER(1, kRegisterD1, SIZE_CODE(sizeof(UInt32)))		// A-Trap
			| REGISTER_ROUTINE_PARAMETER(2, kRegisterA0, SIZE_CODE(sizeof(CudaPB*)))	// CudaPB*
	};

	UniversalProcPtr theProc;
	if (gIsForMacOSX)
		// Don't dereference low memory in Mac OS X.
		// The OS X implementation of CallOSTrapUniversalProc in IONDRVLibraries.cpp
		// doesn't use the UniversalProcPtr anyway - it uses the A-Trap and ProcInfo instead.
		theProc = (UniversalProcPtr)EgretDispatch;
	else
		theProc = *EgretDispatch;

	return CallOSTrapUniversalProc(theProc, uppEgretDispatchProcInfo, _EgretDispatch, cudaPB);
}
