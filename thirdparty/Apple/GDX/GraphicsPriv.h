/*
	File:		GraphicsPriv.h

	Contains:	This file is private to GDX items.  It contains declarations for items that
				are strictly internal to the GDX model.

	Written by:	Sean Williams, Kevin Williams

	Copyright:	© 1994-1995 by Apple Computer, Inc., all rights reserved.

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
				kDisplayFirst = 1,
// from gdx
/* 01	*/		kDisplay512x384At60HzNTSC = kDisplayFirst,		// NTSC Safe Title
/* 02	*/		kDisplay512x384At60Hz,				// Apple's 12" RGB
/* 03	*/		kDisplay640x480At50HzPAL,			// PAL Safe Title
/* 04	*/		kDisplay640x480At60HzNTSC,			// NTSC Full Frame
/* 05	*/		kDisplay640x480At60HzVGA,			// Typical VGA monitor
/* 06	*/		kDisplay640x480At67Hz,				// Apple's 13" & 14" RGB

/* 07	*/		kDisplay640x870At75Hz, 				// Apple's Full Page Display (Portrait)

/* 08	*/		kDisplay768x576At50HzPAL,			// PAL Full Frame
/* 09	*/		kDisplay800x600At56HzVGA,			// SVGA	(VESA Standard)
/* 10	*/		kDisplay800x600At60HzVGA,			// SVGA	(VESA Standard)
/* 11	*/		kDisplay800x600At72HzVGA,			// SVGA	(VESA Standard)
/* 12	*/		kDisplay800x600At75HzVGA,			// SVGA at a higher refresh rate
/* 13	*/		kDisplay832x624At75Hz,				// Apple's 16" RGB
/* 14	*/		kDisplay1024x768At60HzVGA,			// VESA 1K-60Hz

/* 15	*/		kDisplay1024x768At72HzVGA,			// 72Hz (So it records well on TV)
/* 16	*/		kDisplay1024x768At75HzVGA,			// VESA 1K-75Hz....higher refresh rate
/* 17	*/		kDisplay1024x768At75Hz,				// Apple's 19" RGB

/* 18	*/		kDisplay1152x870At75Hz,				// Apple's 21" RGB

/* 19	*/		kDisplay1280x960At75Hz,				// Square Pixel version of 1280x1024
/* 20	*/		kDisplay1280x1024At75Hz,			// 

/* 21	*/		kDisplay256x192At60HzNTSCZoom,		// NTSC Safe Title timing, but 1/4 pixels
/* 22	*/		kDisplay320x240At50HzPALZoom,		// PAL Safe Title timing, but 1/4 pixels
/* 23	*/		kDisplay320x240At60HzNTSCZoom,		// NTSC Full Frame timing, but 1/4 pixels
/* 24	*/		kDisplay384x288At50HzPALZoom,		// PAL Full Frame timing, but 1/4 pixels

// from control ndrv
/* 25	*/		kDisplay1600x1200At80Hz,
/* 26	*/		kDisplay1024x768At70Hz,
/* 27	*/		kDisplay1600x1200At60Hz,
/* 28	*/		kDisplay1600x1200At65Hz,
/* 29	*/		kDisplay1600x1200At70Hz,
/* 30	*/		kDisplay1600x1200At75Hz,
/* 31	*/		kDisplay832x624At60Hz,
/* 32	*/		kDisplay832x624At50Hz,
/* 33	*/		kDisplay832x624At48Hz,
/* 34	*/		kDisplay1024x768At60Hz,
/* 35	*/		kDisplay1024x768At50Hz,
/* 36	*/		kDisplay1024x768At48Hz,
/* 37	*/		kDisplay1600x1200At50Hz,
/* 38	*/		kDisplay640x480At120Hz,
/* 39	*/		kDisplay640x480LCD1,
/* 40	*/		kDisplay640x480LCD2,
/* 41	*/		kDisplay800x600LCD1,
/* 42	*/		kDisplay800x600LCD2,
/* 43	*/		kDisplay1024x768LCD1,
/* 44	*/		kDisplay852x480At60Hz,
/* 45	*/		kDisplay640x480At72Hz,
/* 46	*/		kDisplay640x480At75Hz,
/* 47	*/		kDisplay640x480At85Hz,
/* 48	*/		kDisplay800x600At85Hz,
/* 49	*/		kDisplay1024x768At85Hz,
/* 50	*/		kDisplay1280x960At60Hz,
/* 51	*/		kDisplay1280x960At85Hz,
/* 52	*/		kDisplay1280x1024At85Hz,
/* 53	*/		kDisplay704x480At60HzNTSC,
/* 54	*/		kDisplay704x576At50HzPAL,
/* 55	*/		kDisplay720x480At60HzNTSC,
/* 56	*/		kDisplay720x576At50HzPAL,
/* 57	*/		kDisplay400x300At60Hz,
/* 58	*/		kDisplay1280x1024At60Hz,

// from control ndrv Mac OS 9.1
/* 59	*/		kDisplayMode59,
/* 60	*/		kDisplayMode60,
/* 61	*/		kDisplayMode61,
/* 62	*/		kDisplay1152x768LCD,

/*
// more voodoo modes
	kDisplay512x384At70Hz,
	kDisplay512x384At72Hz,
	kDisplay512x384At75Hz,
	kDisplay512x384At80Hz,
	kDisplay512x384At85Hz,
	kDisplay512x384At90Hz,
	kDisplay512x384At100Hz,
	kDisplay512x384At120Hz,

	kDisplay640x400At60Hz,
	kDisplay640x400At70Hz,
	kDisplay640x400At72Hz,
	kDisplay640x400At75Hz,
	kDisplay640x400At80Hz,
	kDisplay640x400At85Hz,
	kDisplay640x400At90Hz,
	kDisplay640x400At100Hz,
	kDisplay640x400At120Hz,

	kDisplay640x480At70Hz,
	kDisplay640x480At80Hz,
	kDisplay640x480At90Hz,
	kDisplay640x480At100Hz,

	kDisplay800x600At70Hz,
	kDisplay800x600At80Hz,
	kDisplay800x600At90Hz,
	kDisplay800x600At100Hz,
	kDisplay800x600At120Hz,

	kDisplay856x480At60Hz,
	kDisplay856x480At70Hz,
	kDisplay856x480At72Hz,
	kDisplay856x480At75Hz,
	kDisplay856x480At80Hz,
	kDisplay856x480At85Hz,
	kDisplay856x480At90Hz,
	kDisplay856x480At100Hz,
	kDisplay856x480At120Hz,

	kDisplay960x720At60Hz,
	kDisplay960x720At70Hz,
	kDisplay960x720At72Hz,
	kDisplay960x720At75Hz,
	kDisplay960x720At80Hz,
	kDisplay960x720At85Hz,
	kDisplay960x720At90Hz,
	kDisplay960x720At100Hz,
	kDisplay960x720At120Hz,

	kDisplay1024x768At80Hz,
	kDisplay1024x768At90Hz,
	kDisplay1024x768At100Hz,
	kDisplay1024x768At120Hz,
*/
//
/*
	kOneGreaterThanLastDisplayModeID,										// Let this value be auto-incremented
	kMaxDisplayModeIDs = kOneGreaterThanLastDisplayModeID - kDisplayFirst,	// so this value is always correct.
*/
	kFirstProgrammableDisplayMode = 200,
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
