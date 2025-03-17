/*
	File:		GraphicsPriv.h

	Contains:	This file is private to GDX items.  It contains declarations for items that
				are strictly internal to the GDX model.

	Written by:	Sean Williams, Kevin Williams

	Copyright:	(c) 1994-1995 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

		 <1>	 4/15/95	SW		First Checked In

*/

#ifndef __GRAPHICSPRIV__
#define __GRAPHICSPRIV__

#include <MacTypes.h>
#include <Video.h>


// When writing to hardware in the C world, register address need to be declared
// as volatile.  Why?  Often a register is written to, and then read immediately.
// C, optimizing away programmers intentions, will not do the read.  
typedef volatile  UInt8		HWRegister8Bit;
typedef volatile  UInt16	HWRegister16Bit;
typedef volatile  UInt32	HWRegister32Bit;



//
// GDXErr
//	These error codes are private to the GDX model, and are not seen by any of the
//	Graphics driver clients.  They are used for debugging purposes, so the driver
//	writer will have more resolution than is provided by 'controlErr' or 'statusErr'.
//
typedef SInt32 GDXErr;
enum
{
	kGDXErrUnknownError = -4096,				// Sloppy programming if ever seen

	kGDXErrNoError = 0,				
	kGDXErrInvalidParameters = -1,				// Invalid parameters were provided
	
	kGDXErrDriverAlreadyOpen = -2,				// Driver is already open and got another open command
	kGDXErrDriverAlreadyClosed = -3,			// Driver is closed and got another close command
	kGDXErrRequestedModeNotPossible = -4,		// Invalid depth mode and/or display page requested
	kGDXErrDisplayModeIDUnsupported = -5,		// Frame buffer cannot support requested DisplayModeID
	kGDXErrMonitorUnsupported = -6,				// The connected monitor type is not supported
	kGDXErrDepthModeUnsupported = -7,			// Frame buffer cannot support requested DepthMode
	kGDXErrInvalidForIndexedDevice = -8,		// An invalid operation was attempted for a indexed device
	kGDXErrInvalidForDirectDevice = -9,			// An invalid operation was attempted for a direct device
	kGDXErrUnableToMapDepthModeToBPP = -10,		// Cannot relative pixel depth to absolute pixel depth
	kGDXErrInvalidColorSpecTable = -11,			// Client supplied invalid ColorSpec table
	kGDXErrUnableToAllocateColorSpecTable = -12,// Unable to allocate memory for a ColorSpec table
	kGDXErrUnableToAllocateGammaTable = -13,	// Unable to allocate memory for a gamma table
	kGDXErrUnableToAllocateCoreData = -14,		// Unable to allocate memory for a Core data
	kGDXErrUnableToAllocateHALData = -15,		// Unable to allocate memory for a HAL data
	kGDXErrUnableToAllocateData = -16,			// Unable to allocate misc data
	kGDXErrInvalidGammaTable = -17,				// Client supplied an invalid gamma table.
	kGDXErrInvalidResolutionForMonitor = -18,	// Monitor doesn't support requested resolution
	kGDXErrNoConnectedMonitor = -19,			// No monitor connected
	kGDXErrOSSPropertyNameLengthTooLong = -20,	// PropertyName exceeded the maximum length
	kGDXErrOSSPropertyStorageInvalid = -21,		// OSS PropertyStorage invalid
	kGDXErrOSSUnableToSavePropertyStorage= -22,	// OSS PropertyStorage could not be saved
	kGDXErrOSSNoProperyNameAndValue = -23,		// Unable to find the propertyName and propertyValue
	kGDXErrOSSUnableToGetPropertyValue = -24,	// Property exists but unable to Get the propertyValue
	kGDXErrOSSPropertySizeExceedsBuffer = -25,	// PropertySize exceeds the buffer size
	kGDXErrOSSUnexpectedPropertySize = -26,		// PropertySize didn't match.  propetyValue is bad
	kGDXErrOSSUnableToSetPropertyValue = -27,	// Unable to set a propertyValue for a property we know exists
	kGDXErrOSSUnableToCreateProperty = -28,		// Unable to create a property
	kGDXErrOSSUknownMappedDisplayModeID = -29,	// Mapped displayModeID (grpf) is unknown
	kGDXErrOSSUknownMappedDepthMode = -30,		// Mapped depthMode (grpf) is unknown
	kGDXErrOSSUknownMappedDisplayCode = -31,	// Mapped displayCode (grpf) is unknown
	kGDXErrOSSNoISTProperty = -32,				// No kISTPropertyName for the RegEntryID
	kGDXErrOSSNoDefaultVBLRoutines = -33,		// GetInterruptFunctions (Interrupts.h) failed
	kGDXErrOSSUnableToInstallVBLRoutines = -34,	// InstallInterruptFunctions (Interrupts.h) failed
	kGDXErrOSSUnableToInstallVSLService = -35,	// VSLNewInterruptService (VideoServices.h) failed
	kGDXErrOSSInterruptSourceStillActive = -36,	// Can't dispose of VSL services since interrupts ON
	kGDXErrOSSUnableToDisposeVSLService = -37,	// VSLDisposeInterruptService (VideoServices.h) failed
	kGDXErrUnableToSizeVRAM = -38,				// Failed VRAM sizing
	kGDXErrNoHardwareCursorSet = -39,			// No hardware cursor has been set
	kGDXErrCannotRenderCursorImage = -40,		// Specific cursor image cannot be rendered by hardware
	kGDXErrUnsupportedFunctionality = -41,		// Functionally (such as hw cursor, power saving) not supported
	kGDXErrDriverCannotRun = -42,				// Driver can't (or shouldn't) be run.
	
	kGDXErrDDCError43 = -43, // ack timeout after transmit
	kGDXErrDDCError44 = -44,
	
	kGDXErrOSSBadInterruptServiceType = -46,
	kGDXErrPad = 0x7fffffff						// Pad to avoid warning of dangling ','
};



//
// GammaTableID
//	This abstract data type is used to reference gamma tables.
//	(The typedef is declared in the API, so just the enumerated constants appear here)
//
enum
{
	kGammaTableIDStandard = 2000,				// Mac Standard Gamma
	kGammaTableIDPageWhite = 2001,				// Page-White Gamma
	kGammaTableIDGray = 2002,					// Mac Gray Gamma
	kGammaTableIDRubik = 2004,					// Mac RGB Gamma
	kGammaTableIDNTSCPAL = 2005,				// NTSC/PAL Gamma
	kGammaTableIDCSCTFT = 2006					// Active Matrix Color LCD Gamma
};



//
// DisplayModeID
//	This abstract data type is used to reference display resolutions.
//	The examples of monitors listed do NOT constitute the full list for a given DisplayModeID.  
//	Instead, they are just intended only to be a represetative sample.
//
//	DisplayModeIDs shouldn't change arbitrarily, even though the are completely private to the driver.
//	This is because the DisplayModeID is stored in NVRAM, and it could result in confusion if
//	a ROM based driver interprets the NVRAM items one way, and a subsequent disk based driver 
//	interprets them differently.
//
enum
{
				kDisplay_First = 1,
// from gdx
/* 01	*/		kDisplay_512x384_60Hz_NTSC			//      NTSC Safe Title
					= kDisplay_First,
/* 02	*/		kDisplay_512x384_60Hz,				// +    Apple's 12" RGB
/* 03	*/		kDisplay_640x480_50Hz_PAL,			//      PAL Safe Title
/* 04	*/		kDisplay_640x480_60Hz_NTSC,			// +    NTSC Full Frame
/* 05	*/		kDisplay_640x480_60Hz_VGA,			// +    Typical VGA monitor
/* 06	*/		kDisplay_640x480_67Hz,				// +    Apple's 13" & 14" RGB

/* 07	*/		kDisplay_640x870_75Hz, 				// +    Apple's Full Page Display (Portrait)

/* 08	*/		kDisplay_768x576_50Hz_PAL,			//      PAL Full Frame
/* 09	*/		kDisplay_800x600_56Hz_VGA,			// +    SVGA	(VESA Standard)
/* 10	*/		kDisplay_800x600_60Hz_VGA,			// +    SVGA	(VESA Standard)
/* 11	*/		kDisplay_800x600_72Hz_VGA,			// +    SVGA	(VESA Standard)
/* 12	*/		kDisplay_800x600_75Hz_VGA,			// +    SVGA at a higher refresh rate
/* 13	*/		kDisplay_832x624_75Hz,				// +    Apple's 16" RGB
/* 14	*/		kDisplay_1024x768_60Hz_VGA,			// +    VESA 1K-60Hz

/* 15	*/		kDisplay_1024x768_72Hz_VGA,			//      72Hz (So it records well on TV)
/* 16	*/		kDisplay_1024x768_75Hz_VGA,			// +    VESA 1K-75Hz....higher refresh rate
/* 17	*/		kDisplay_1024x768_75Hz,				// +    Apple's 19" RGB

/* 18	*/		kDisplay_1152x870_75Hz,				// +    Apple's 21" RGB

/* 19	*/		kDisplay_1280x960_75Hz,				// +    Square Pixel version of 1280x1024
/* 20	*/		kDisplay_1280x1024_75Hz,			// +    

/* 21	*/		kDisplay_256x192_60Hz_NTSCZoom,		//      NTSC Safe Title timing, but 1/4 pixels
/* 22	*/		kDisplay_320x240_50Hz_PALZoom,		//      PAL Safe Title timing, but 1/4 pixels
/* 23	*/		kDisplay_320x240_60Hz_NTSCZoom,		//      NTSC Full Frame timing, but 1/4 pixels
/* 24	*/		kDisplay_384x288_50Hz_PALZoom,		//      PAL Full Frame timing, but 1/4 pixels

//				kDisplay_640x480_60Hz_NTSC,
				kDisplay_512x342_60Hz,
//				kDisplay_512x384_60Hz,
				kDisplay_560x384_60Hz,
				kDisplay_640x400_67Hz,
//				kDisplay_640x870_75Hz,
//				kDisplay_1024x768_75Hz,
//				kDisplay_1280x960_75Hz,

				kDisplay_720x400_70Hz,
				kDisplay_720x400_88Hz,
//				kDisplay_640x480_67Hz,
//				kDisplay_832x624_75Hz,
//				kDisplay_1152x870_75Hz,

				kDisplay_640x350_85Hz,
				kDisplay_640x400_85Hz,
				kDisplay_720x400_85Hz,
//				kDisplay_640x480_60Hz_VGA,
				kDisplay_640x480_72Hz,
				kDisplay_640x480_75Hz,
				kDisplay_640x480_85Hz,
//				kDisplay_800x600_56Hz_VGA,
//				kDisplay_800x600_60Hz_VGA,
//				kDisplay_800x600_72Hz_VGA,
//				kDisplay_800x600_75Hz_VGA,
				kDisplay_800x600_85Hz,
				kDisplay_800x600_120Hz,
				kDisplay_848x480_60Hz,
				kDisplay_1024x768i_87Hz,
//				kDisplay_1024x768_60Hz_VGA,
				kDisplay_1024x768_70Hz,
//				kDisplay_1024x768_75Hz_VGA,
				kDisplay_1024x768_85Hz,
				kDisplay_1024x768_120Hz,
				kDisplay_1152x864_75Hz,
				kDisplay_1280x720_60Hz,
				kDisplay_1280x768_60Hz_RB,
				kDisplay_1280x768_59_87Hz,
				kDisplay_1280x768_75Hz,
				kDisplay_1280x768_85Hz,
				kDisplay_1280x768_120Hz,
				kDisplay_1280x800_60Hz_RB,
				kDisplay_1280x800_60Hz,
				kDisplay_1280x800_75Hz,
				kDisplay_1280x800_85Hz,
				kDisplay_1280x800_120Hz,
				kDisplay_1280x960_60Hz,
				kDisplay_1280x960_85Hz,
				kDisplay_1280x960_120Hz,
				kDisplay_1280x1024_60Hz,
//				kDisplay_1280x1024_75Hz,
				kDisplay_1280x1024_85Hz,
				kDisplay_1280x1024_120Hz,
				kDisplay_1360x768_60Hz,
				kDisplay_1360x768_120Hz,
				kDisplay_1366x768_60Hz,
				kDisplay_1366x768_60Hz_RB,
				kDisplay_1400x1050_60Hz_RB,
				kDisplay_1400x1050_60Hz,
				kDisplay_1400x1050_75Hz,
				kDisplay_1400x1050_85Hz,
				kDisplay_1400x1050_120Hz,
				kDisplay_1440x900_60Hz_RB,
				kDisplay_1440x900_60Hz,
				kDisplay_1440x900_75Hz,
				kDisplay_1440x900_85Hz,
				kDisplay_1440x900_120Hz,
				kDisplay_1600x900_60Hz,
				kDisplay_1600x1200_60Hz,
				kDisplay_1600x1200_65Hz,
				kDisplay_1600x1200_70Hz,
				kDisplay_1600x1200_75Hz,
				kDisplay_1600x1200_85Hz,
				kDisplay_1600x1200_120Hz,
				kDisplay_1680x1050_60Hz_RB,
				kDisplay_1680x1050_60Hz,
				kDisplay_1680x1050_75Hz,
				kDisplay_1680x1050_85Hz,
				kDisplay_1680x1050_120Hz,
				kDisplay_1792x1344_60Hz,
				kDisplay_1792x1344_75Hz,
				kDisplay_1792x1344_120Hz,
				kDisplay_1856x1392_60Hz,
				kDisplay_1856x1392_75Hz,
				kDisplay_1856x1392_120Hz,
				kDisplay_1920x1080_60Hz,
				kDisplay_1920x1200_60Hz_RB,
				kDisplay_1920x1200_60Hz,
				kDisplay_1920x1200_75Hz,
				kDisplay_1920x1200_85Hz,
				kDisplay_1920x1200_120Hz,
				kDisplay_1920x1440_60Hz,
				kDisplay_1920x1440_75Hz,
				kDisplay_1920x1440_120Hz,
				kDisplay_2048x1152_60Hz,
				kDisplay_2560x1600_60Hz_RB,
				kDisplay_2560x1600_60Hz,
				kDisplay_2560x1600_75Hz,
				kDisplay_2560x1600_85Hz,
				kDisplay_2560x1600_120Hz,
				kDisplay_4096x2160_60Hz_RB,
				kDisplay_4096x2160_59_94Hz,

				kDisplay_720x480_60Hz,
				kDisplay_1920x1080i_60Hz,
				kDisplay_1440x480i_60Hz,
				kDisplay_1440x240_60Hz,
				kDisplay_2880x480i_60Hz,
				kDisplay_2880x240_60Hz,
				kDisplay_1440x480_60Hz,
				kDisplay_720x576_50Hz,
				kDisplay_1280x720_50Hz,
				kDisplay_1920x1080i_50Hz,
				kDisplay_1440x576i_50Hz,
				kDisplay_1440x288_50Hz,
				kDisplay_2880x576i_50Hz,
				kDisplay_2880x288_50Hz,
				kDisplay_1440x576_50Hz,
				kDisplay_1920x1080_50Hz,
				kDisplay_1920x1080_24Hz,
				kDisplay_1920x1080_25Hz,
				kDisplay_1920x1080_30Hz,
				kDisplay_2880x480_60Hz,
				kDisplay_2880x576_50Hz,
				kDisplay_1920x1080i_50Hz_72MHz,
				kDisplay_1920x1080i_100Hz,
				kDisplay_1280x720_100Hz,
				kDisplay_720x576_100Hz,
				kDisplay_1440x576i_100Hz,
				kDisplay_1920x1080i_120Hz,
				kDisplay_1280x720_120Hz,
				kDisplay_720x480_120Hz,
				kDisplay_1440x480i_120Hz,
				kDisplay_720x576_200Hz,
				kDisplay_1440x576i_200Hz,
				kDisplay_720x480_240Hz,
				kDisplay_1440x480i_240Hz,
				kDisplay_1280x720_24Hz,
				kDisplay_1280x720_25Hz,
				kDisplay_1280x720_30Hz,
				kDisplay_1920x1080_120Hz,
				kDisplay_1920x1080_100Hz,
				kDisplay_1680x720_24Hz,
				kDisplay_1680x720_25Hz,
				kDisplay_1680x720_30Hz,
				kDisplay_1680x720_50Hz,
				kDisplay_1680x720_60Hz,
				kDisplay_1680x720_100Hz,
				kDisplay_1680x720_120Hz,
				kDisplay_2560x1080_24Hz,
				kDisplay_2560x1080_25Hz,
				kDisplay_2560x1080_30Hz,
				kDisplay_2560x1080_50Hz,
				kDisplay_2560x1080_60Hz,
				kDisplay_2560x1080_100Hz,
				kDisplay_2560x1080_120Hz,
				kDisplay_3840x2160_24Hz,
				kDisplay_3840x2160_25Hz,
				kDisplay_3840x2160_30Hz,
				kDisplay_3840x2160_50Hz,
				kDisplay_3840x2160_60Hz,
				kDisplay_4096x2160_24Hz,
				kDisplay_4096x2160_25Hz,
				kDisplay_4096x2160_30Hz,
				kDisplay_4096x2160_50Hz,
				kDisplay_4096x2160_60Hz,
				kDisplay_1280x720_48Hz,
				kDisplay_1680x720_48Hz,
				kDisplay_1920x1080_48Hz,
				kDisplay_2560x1080_48Hz,
				kDisplay_3840x2160_48Hz,
				kDisplay_4096x2160_48Hz,
				kDisplay_3840x2160_100Hz,
				kDisplay_3840x2160_120Hz,
				kDisplay_5120x2160_24Hz,
				kDisplay_5120x2160_25Hz,
				kDisplay_5120x2160_30Hz,
				kDisplay_5120x2160_48Hz,
				kDisplay_5120x2160_50Hz,
				kDisplay_5120x2160_60Hz,
				kDisplay_5120x2160_100Hz,
				kDisplay_5120x2160_120Hz,
				kDisplay_7680x4320_24Hz,
				kDisplay_7680x4320_25Hz,
				kDisplay_7680x4320_30Hz,
				kDisplay_7680x4320_48Hz,
				kDisplay_7680x4320_50Hz,
				kDisplay_7680x4320_60Hz,
				kDisplay_7680x4320_100Hz,
				kDisplay_7680x4320_120Hz,
				kDisplay_10240x4320_24Hz,
				kDisplay_10240x4320_25Hz,
				kDisplay_10240x4320_30Hz,
				kDisplay_10240x4320_48Hz,
				kDisplay_10240x4320_50Hz,
				kDisplay_10240x4320_60Hz,
				kDisplay_10240x4320_100Hz,
				kDisplay_10240x4320_120Hz,
				kDisplay_4096x2160_100Hz,
				kDisplay_4096x2160_120Hz,

/*
	kOneGreaterThanLastDisplayModeID,										// Let this value be auto-incremented
	kMaxDisplayModeIDs = kOneGreaterThanLastDisplayModeID - kDisplay_First,	// so this value is always correct.
*/
	kFirstProgrammableDisplayMode = 300,
};



//
// DisplayCode
//	This abstract data type is used to represent what type of display is physically connected to 
//	the frame buffer controller.  It is important to note that NO ATTEMPT should be made to directly
//	map a DisplayCode from raw or exteneded sense codes.
//	All monitors are considered to be RGB, unless explicitly marked otherwise.  
//	The examples of displays listed in the comments represent only a some instances of that type, not
//	necessarily the full set.
//
//	WARNING! since the the DisplayCode is saved into NVRAM (allocated 1 byte of space), NO DisplayCode
//	should be greater than 255.
//
//	Again, just as with DisplayModeIDs, these values should not be arbitrarily changed, thus avoid
//	confusion between ROM based and disk based drivers.
// 
typedef SInt32 DisplayCode;
enum
{
	kDisplayCodeNoDisplay 			= 0,			// No display attached
	kDisplayCodeUnknown				= 1,			// A display is present, but it's type is unknown 
	kDisplayCode12Inch				= 2,			// 12" RGB (Rubik)
	kDisplayCodeStandard			= 3,			// 13"/14" RGB or 12" Monochrome
	kDisplayCodePortrait 			= 4,			// 15" Portait RGB (Manufactured by Radius)
	kDisplayCodePortraitMono 		= 5,			// 15" Portrait Monochrome
	kDisplayCode16Inch				= 6,			// 16" RGB (GoldFish)
	kDisplayCode19Inch				= 7,			// 19" RGB (Third Party)
	kDisplayCode21Inch				= 8,			// 21" RGB (Vesuvio, Radius 21" RGB)
	kDisplayCode21InchMono			= 9,			// 21" Monochrome (Kong, Radius 21" Mono)
	kDisplayCodeVGA 				= 10,			// VGA
	kDisplayCodeNTSC		 		= 11,			// NTSC
	kDisplayCodePAL 				= 12,			// PAL
	kDisplayCodeMultiScanBand1		= 13,			// MultiScan Band-1 (12" thru 16" resolutions)
	kDisplayCodeMultiScanBand2		= 14,			// MultiScan Band-3 (13" thru 19" resolutions)
	kDisplayCodeMultiScanBand3		= 15,			// MultiScan Band-3 (13" thru 21" resolutions)
	kDisplayCode16					= 16,
	kDisplayCode17					= 17,
	kDisplayCodePanel				= 18,
	kDisplayCodePanelFSTN			= 19,
	kDisplayCode20					= 20,
	kDisplayCode21					= 21,
	kDisplayCodeDDCC				= 22
};



//
// SenseLine
//	This abstract data type is used to reference the various monitor sense lines used by frame buffer
//	controllers which can implement Apple's Sense Line Protocol.
//
typedef UInt32 SenseLine;
enum
{
	kSenseLineA = 0,			// Corresponds to Monitor Sense Line 2 (pin 10 on a DB-15 connector)
	kSenseLineB = 1,			// Corresponds to Monitor Sense Line 1 (pin 7 on a DB-15 connector)
	kSenseLineC = 2				// Corresponds to Monitor Sense Line 0 (pin 4 on a DB-15 connector)
};



//
// OSSPropertyStorage
//	This abstract data type specifies how the driver can save information.  The information can be
//	NonVolatile (kOSSPropertyAvailableAtBoot, kOSSPropertyAvailableAtDisk) or Volatile.
//	The OSS insulates the rest of the driver from whatever storage mechanism is used by the OS
//
typedef UInt32 OSSPropertyStorage;
enum
{
	kOSSPropertyAvailableAtBoot,
	kOSSPropertyAvailableAtDisk,
	kOSSPropertyVolatile
};



//
// GraphicsPreferred
//	This structure describes the PropertyValue that is needed for the
//	GetPreferredConfiguration and SetPreferredConfiguration calls.  The DisplayCode is saved in
//	addition to the DisplayModeID and the DepthMode since the core looks at the DisplayCode from the
//	previous boot.  Since only 8 bytes of NVRAM are available to store the data, the OSS will map
//	the GraphicsPreferred structure into a GraphicsNonVolatile structure (defined in GraphicsOSS.c)
//
typedef struct GraphicsPreferred GraphicsPreferred;
struct GraphicsPreferred 
{
	UInt8 ddcChecksum;
    DisplayModeID displayModeID;
    DepthMode depthMode;
	DisplayCode displayCode;				// Save the DisplayCode in case user switches monitors
};


// The PropertyName for the PreferredConfiguration
#define kPreferredConfigurationName "gprf"

//
// GraphicsHAL VBL handler prototypes
//	Function definitions for the HALs to handle real vbl's
//
typedef void VBLHandler(void* vblRefCon);
typedef void VBLEnabler(void *vblRefcon);
typedef Boolean VBLDisabler(void *vblRefcon);



//
// DisplayModeIDData
//	All the DisplayModeIDs need to be examined by the Core/OSS for GetNextResolution,
//	GetVideoParamenters and when the OSS compresses the 'gprf' data to be in 8 bytes.
//	This table combines all the information into one table
//
typedef struct DisplayModeIDData DisplayModeIDData;
struct DisplayModeIDData 
{
    DisplayModeID displayModeID;			// The DisplayModeID in question

	UInt16 horizontalPixels; 				// GetNextResolution/GetVideoParams data
	UInt16 verticalLines;					// GetNextResolution/GetVideoParams data
	UInt32 timingData;
	Fixed refreshRate;						// GetNextResolution/GetVideoParams data
};


//
// procedure types
//
typedef Boolean (* BooleanProc )(void);
typedef void (* VoidProc )(void);
typedef RawSenseCode (* RawSenseCodeProc )(void);
typedef void (* DDCPostProcessBlockProc)(UInt8 *ddcBlockData, UInt32 ddcBlockNumber);

#endif	// __GRAPHICSPRIV__
