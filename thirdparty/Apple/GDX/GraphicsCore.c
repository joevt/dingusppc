/*
	File:		GraphicsCore.c

	Contains:	This file implements the Intialize, Open, Close, Status, Control, and Finalize
				routines, as well as come other 'core' related functions.

	Written by:	Sean Williams, Kevin Williams

	Copyright:	© 1994-1995 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

		 <2>	 7/17/95	SW		In Graphics Open(), now apply the default gamma table instead of
		 							a linear one.  Also, do some slight changes to better support
									drivers which don't fully support kReplace/Supercede commands to
									eliminate the CLUT being incorrectly grayed out.
		 <1>	 4/15/95	SW		First Checked In

*/

#include "GraphicsPriv.h"
#include "GraphicsCore.h"
#include "GraphicsCorePriv.h"
#include "GraphicsCoreControl.h"
#include "GraphicsCoreStatus.h"
#include "GraphicsCoreUtils.h"
#include "GraphicsHAL.h"
#include "GraphicsOSS.h"

#include <Devices.h>
#include <DriverServices.h>
#include <NameRegistry.h>
#include <Errors.h>
#include <Video.h>


typedef	struct CoreReplacementDriverInfo CoreReplacementDriverInfo;
struct CoreReplacementDriverInfo 
{
	DisplayModeID displayModeID;	// Current display mode selector
	DepthMode depthMode;			// Relative bit depth
	short currentPage;				// Current graphics page (0 based)
	void *baseAddress;				// Base address of current page of frame buffer
};


// Forward delcarations of GraphicsCore.c routines that nobody else has to know about
static GDXErr GraphicsCoreInitPrivateData(DriverRefNum refNum, const RegEntryID* regEntryID,
			const AddressSpaceID spaceID);

void GraphicsCoreKillPrivateData(void);


GraphicsCoreData gCoreData;				// Persistant globals for the Core's private data.



//=====================================================================================================
//
// GraphicsCoreGetCoreData()
//	This the access method for the Core's private data.  ALWAYS uses this function to obtain
//	a pointer to the private data, as opposed to trying to access the data directly.
//	You have been warned...
//
//=====================================================================================================
GraphicsCoreData *GraphicsCoreGetCoreData(void)
{
	return &gCoreData;
}



//=====================================================================================================
//
// GraphicsCoreInitPrivateData()
//
//	This routine initializes the Core's private data to its proper state.
//
//=====================================================================================================
static GDXErr GraphicsCoreInitPrivateData(DriverRefNum refNum, const RegEntryID *regEntryID, const AddressSpaceID spaceID)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();
	GDXErr err = kGDXErrNoError;													// Assume success

#if 0
	DisplayModeIDData *masterTable = (DisplayModeIDData *) &coreData->masterTable;
	UInt32 i;																		// Loop iterator

	DisplayModeIDData localTable[kMaxDisplayModeIDs] =
	{
		{kDisplay512x384At60HzNTSC,	512,	384,	timingAppleNTSC_ST,	0x003BF080},	//	59.94 Hz
		{kDisplay512x384At60Hz,	512,	384,	timingApple_512x384_60hz,	0x003C0000},	//	60 Hz
/*		{kDisplay512x384At70Hz,	512,	384,	timingInvalid,	70<<16},	//	70 Hz
		{kDisplay512x384At72Hz,	512,	384,	timingInvalid,	72<<16},	//	72 Hz
		{kDisplay512x384At75Hz,	512,	384,	timingInvalid,	75<<16},	//	75 Hz
		{kDisplay512x384At80Hz,	512,	384,	timingInvalid,	80<<16},	//	80 Hz
		{kDisplay512x384At85Hz,	512,	384,	timingInvalid,	85<<16},	//	85 Hz
		{kDisplay512x384At90Hz,	512,	384,	timingInvalid,	90<<16},	//	90 Hz
		{kDisplay512x384At100Hz,	512,	384,	timingInvalid,	100<<16},	//	100 Hz
		{kDisplay512x384At120Hz,	512,	384,	timingInvalid,	120<<16},	//	120 Hz

		{kDisplay640x400At60Hz,	640,	400,	timingInvalid,	60<<16},	//	60 Hz
		{kDisplay640x400At70Hz,	640,	400,	timingInvalid,	70<<16},	//	70 Hz
		{kDisplay640x400At72Hz,	640,	400,	timingInvalid,	72<<16},	//	72 Hz
		{kDisplay640x400At75Hz,	640,	400,	timingInvalid,	75<<16},	//	75 Hz
		{kDisplay640x400At80Hz,	640,	400,	timingInvalid,	80<<16},	//	80 Hz
		{kDisplay640x400At85Hz,	640,	400,	timingInvalid,	85<<16},	//	85 Hz
		{kDisplay640x400At90Hz,	640,	400,	timingInvalid,	90<<16},	//	90 Hz
		{kDisplay640x400At100Hz,	640,	400,	timingInvalid,	100<<16},	//	100 Hz
		{kDisplay640x400At120Hz,	640,	400,	timingInvalid,	120<<16},	//	120 Hz
*/
		{kDisplay640x480At50HzPAL,	640,	480,	timingApplePAL_ST,	0x00320000},	//	50 Hz
		{kDisplay640x480At60HzNTSC,	640,	480,	timingAppleNTSC_FF,	0x003BF080},	//	59.94 Hz
		{kDisplay640x480At60HzVGA,	640,	480,	timingVESA_640x480_60hz,	0x003BF080},	//	59.94 Hz
		{kDisplay640x480At67Hz,	640,	480,	timingApple_640x480_67hz,	0x0042AA80},	//	66.67 Hz
//	{kDisplay640x480At70Hz,	640,	480,	timingInvalid,	70<<16},	//	70 Hz

		{kDisplay640x480At72Hz,	640,	480,	timingVESA_640x480_72hz,	0x00480000},	//	72 Hz
		{kDisplay640x480At75Hz,	640,	480,	timingVESA_640x480_75hz,	0x004B0000},	//	75 Hz
//		{kDisplay640x480At80Hz,	640,	480,	timingInvalid,	80<<16},	//	80 Hz
		{kDisplay640x480At85Hz,	640,	480,	timingVESA_640x480_85hz,	0x00550000},	//	85 Hz
//	{kDisplay640x480At90Hz,	640,	480,	timingInvalid,	90<<16},	//	90 Hz
//		{kDisplay640x480At100Hz,	640,	480,	timingInvalid,	100<<16},	//	100 Hz
		{kDisplay640x480At120Hz,	640,	480,	timingGTF_640x480_120hz,	0x00780000},	//	120 Hz

		{kDisplay640x870At75Hz,	640,	870,	timingApple_640x870_75hz,	0x004B0000},	//	75 Hz

		{kDisplay704x480At60HzNTSC,	704,	480,	timingAppleNTSC_FF,	0x003BF080},	//	59.94 Hz
		{kDisplay704x576At50HzPAL,	704,	576,	timingApplePAL_FF,	0x00320000},
		{kDisplay720x480At60HzNTSC,	720,	480,	timingAppleNTSC_FF,	0x003BF080},	//	59.94 Hz
		{kDisplay720x576At50HzPAL,	720,	576,	timingApplePAL_FF,	0x00320000},
		{kDisplay768x576At50HzPAL,	768,	576,	timingApplePAL_FF,	0x00320000},	//	50 Hz

		{kDisplay800x600At56HzVGA,	800,	600,	timingVESA_800x600_56hz,	0x00380000},	//	56 Hz
		{kDisplay800x600At60HzVGA,	800,	600,	timingVESA_800x600_60hz,	0x003C0000},	//	60 Hz
//		{kDisplay800x600At70Hz,	800,	600,	timingInvalid,	70<<16},	//	70 Hz
		{kDisplay800x600At72HzVGA,	800,	600,	timingVESA_800x600_72hz,	0x00480000},	//	72 Hz
		{kDisplay800x600At75HzVGA,	800,	600,	timingVESA_800x600_75hz,	0x004b0000},	//	75 Hz
//		{kDisplay800x600At80Hz,	800,	600,	timingInvalid,	80<<16},	//	80 Hz
		{kDisplay800x600At85Hz,	800,	600,	timingVESA_800x600_85hz,	0x00550000},	//	85 Hz
//		{kDisplay800x600At90Hz,	800,	600,	timingInvalid,	90<<16},	//	90 Hz
//		{kDisplay800x600At100Hz,	800,	600,	timingInvalid,	100<<16},	//	100 Hz
//		{kDisplay800x600At120Hz,	800,	600,	timingInvalid,	120<<16},	//	120 Hz

		{kDisplay832x624At75Hz,	832,	624,	timingApple_832x624_75hz,	0x004B0000},	//	75 Hz
/*
		{kDisplay856x480At60Hz,	856,	480,	timingInvalid,	60<<16},	//	60 Hz
		{kDisplay856x480At70Hz,	856,	480,	timingInvalid,	70<<16},	//	70 Hz
		{kDisplay856x480At72Hz,	856,	480,	timingInvalid,	72<<16},	//	72 Hz
		{kDisplay856x480At75Hz,	856,	480,	timingInvalid,	75<<16},	//	75 Hz
		{kDisplay856x480At80Hz,	856,	480,	timingInvalid,	80<<16},	//	80 Hz
		{kDisplay856x480At85Hz,	856,	480,	timingInvalid,	85<<16},	//	85 Hz
		{kDisplay856x480At90Hz,	856,	480,	timingInvalid,	90<<16},	//	90 Hz
		{kDisplay856x480At100Hz,	856,	480,	timingInvalid,	100<<16},	//	100 Hz
		{kDisplay856x480At120Hz,	856,	480,	timingInvalid,	120<<16},	//	120 Hz

		{kDisplay960x720At60Hz,	960,	720,	timingInvalid,	60<<16},	//	60 Hz
		{kDisplay960x720At70Hz,	960,	720,	timingInvalid,	70<<16},	//	70 Hz
		{kDisplay960x720At72Hz,	960,	720,	timingInvalid,	72<<16},	//	72 Hz
		{kDisplay960x720At75Hz,	960,	720,	timingInvalid,	75<<16},	//	75 Hz
		{kDisplay960x720At80Hz,	960,	720,	timingInvalid,	80<<16},	//	80 Hz
		{kDisplay960x720At85Hz,	960,	720,	timingInvalid,	85<<16},	//	85 Hz
		{kDisplay960x720At90Hz,	960,	720,	timingInvalid,	90<<16},	//	90 Hz
		{kDisplay960x720At100Hz,	960,	720,	timingInvalid,	100<<16},	//	100 Hz
		{kDisplay960x720At120Hz,	960,	720,	timingInvalid,	120<<16},	//	120 Hz
*/
		{kDisplay1024x768At60HzVGA,	1024,	768,	timingVESA_1024x768_60hz,	0x003C0000},	//	60 Hz
		{kDisplay1024x768At70Hz,	1024,	768,	timingVESA_1024x768_70hz,	0x00460000},	//	70 Hz
		{kDisplay1024x768At72HzVGA,	1024,	768,	timingVESA_1024x768_70hz,	0x00480000},	//	72 Hz -- close enough mapping		or	0x00460000=70 Hz
		{kDisplay1024x768At75HzVGA,	1024,	768,	timingVESA_1024x768_75hz,	0x004B0000},	//	75 Hz
		{kDisplay1024x768At85Hz,	1024,	768,	timingVESA_1024x768_85hz,	0x00550000},	//	85 Hz
		{kDisplay1024x768At75Hz,	1024,	768,	timingApple_1024x768_75hz,	0x004B0000},	//	75 Hz
/*		{kDisplay1024x768At80Hz,	1024,	768,	timingInvalid,	80<<16},	//	80 Hz
		{kDisplay1024x768At90Hz,	1024,	768,	timingInvalid,	90<<16},	//	90 Hz
		{kDisplay1024x768At100Hz,	1024,	768,	timingInvalid,	100<<16},	//	100 Hz
		{kDisplay1024x768At120Hz,	1024,	768,	timingInvalid,	120<<16},	//	120 Hz
*/		
		{kDisplay1152x870At75Hz,	1152,	870,	timingApple_1152x870_75hz,	0x004B0000},	//	75 Hz

		{kDisplay1280x960At60Hz,	1280,	960,	timingVESA_1280x960_60hz,	0x003C0000},
		{kDisplay1280x960At75Hz,	1280,	960,	timingVESA_1280x960_75hz,	0x004B0000},	//	75 Hz
		{kDisplay1280x960At85Hz,	1280,	960,	timingVESA_1280x960_85hz,	0x00550000},

		{kDisplay1280x1024At60Hz,	1280,	1024,	timingVESA_1280x1024_60hz,	0x003C0000},
		{kDisplay1280x1024At75Hz,	1280,	1024,	timingVESA_1280x1024_75hz,	0x004B0000},	//	75 Hz
		{kDisplay1280x1024At85Hz,	1280,	1024,	timingVESA_1280x1024_85hz,	0x00550000},

		{kDisplay1600x1200At60Hz,	1600,	1200,	timingVESA_1600x1200_60hz,	0x003C0000},
		{kDisplay1600x1200At65Hz,	1600,	1200,	timingVESA_1600x1200_65hz,	0x00410000},
		{kDisplay1600x1200At70Hz,	1600,	1200,	timingVESA_1600x1200_70hz,	0x00460000},
		{kDisplay1600x1200At75Hz,	1600,	1200,	timingVESA_1600x1200_75hz,	0x004B0000},
		{kDisplay1600x1200At80Hz,	1600,	1200,	timingVESA_1600x1200_80hz,	0x00500000},

		{kDisplay832x624At60Hz,	832,	624,	timingInvalid,	0x003BF0A3},
		{kDisplay832x624At50Hz,	832,	624,	timingInvalid,	0x00320000},
		{kDisplay832x624At48Hz,	832,	624,	timingInvalid,	0x00300000},

		{kDisplay1024x768At60Hz,	1024,	768,	timingInvalid,	0x003BF0A3},
		{kDisplay1024x768At50Hz,	1024,	768,	timingInvalid,	0x00320000},
		{kDisplay1024x768At48Hz,	1024,	768,	timingInvalid,	0x00300000},

		{kDisplay1600x1200At50Hz,	1600,	1200,	timingInvalid,	0x00320000},
		{kDisplay852x480At60Hz,	852,	480,	timingInvalid,	0x003C0000},

		{kDisplay256x192At60HzNTSCZoom,	256,	192,	timingAppleNTSC_ST,	0x003BF080},	//	59.94 Hz	or	0x003BF0A3
		{kDisplay320x240At50HzPALZoom,	320,	240,	timingApplePAL_ST,	0x00320000},	//	50 Hz
		{kDisplay320x240At60HzNTSCZoom,	320,	240,	timingAppleNTSC_FF,	0x003BF080},	//	59.94 Hz = 59.939453	or	0x003BF0A3 = 59.939987
		{kDisplay384x288At50HzPALZoom,	384,	288,	timingApplePAL_FF,	0x00320000},	//	50 Hz

		{kDisplay640x480LCD1,	640,	480,	timingApple_FixedRateLCD,	0x00000000},
		{kDisplay640x480LCD2,	640,	480,	timingApple_FixedRateLCD,	0x00000000},
		{kDisplay800x600LCD1,	800,	600,	timingApple_FixedRateLCD,	0x00000000},
		{kDisplay800x600LCD2,	800,	600,	timingApple_FixedRateLCD,	0x00000000},
		{kDisplay1024x768LCD1,	1024,	768,	timingApple_FixedRateLCD,	0x00000000},
		{kDisplay1152x768LCD,	1152,	768,	timingApple_FixedRateLCD,	0x00000000}, // from 9.1
		{kDisplay400x300At60Hz,	400,	300,	timingInvalid,	0x003BF0A3},
		{0,	0,	0,	timingInvalid,	0x00000000},
		{0,	0,	0,	timingInvalid,	0x00000000},
		{0,	0,	0,	timingInvalid,	0x00000000},
	};

	for (i = 0; i < kMaxDisplayModeIDs ; i++)
		masterTable[i] = localTable[i];
#endif
	
	RegistryEntryIDCopy(regEntryID, &coreData->regEntryID);
	coreData->driverRefNum = refNum;
	coreData->spaceID = spaceID;

	coreData->delay20microsecs = DurationToAbsolute(20 * durationMicrosecond);
	coreData->delay40microsecs = DurationToAbsolute(40 * durationMicrosecond);
	coreData->delay100microsecs = DurationToAbsolute(100 * durationMicrosecond);
	coreData->delay200microsecs = DurationToAbsolute(200 * durationMicrosecond);
	coreData->delay1millisecs = DurationToAbsolute(1 * durationMillisecond);
	coreData->delay5secs = DurationToAbsolute(5 * durationSecond);

	coreData->graphicsCoreInited = true;
	coreData->onlySupportStandardGamma = false;

	coreData->processErrorProc = NULL;
	coreData->getMonoOnly = NULL;
	coreData->modifyConnection = NULL;

	coreData->useGrayPatterns = false;

ErrorExit:

	return err;
}


#if 0
//=====================================================================================================
//
// GraphicsCoreGetResolutionAndFrequency()
//
//=====================================================================================================
GDXErr GraphicsCoreGetResolutionAndFrequency( DisplayModeID displayModeID, UInt32 *horizontalPixels,
												UInt32 *verticalLines, Fixed *refreshRate )
{
	return GraphicsHALGetResolutionAndFrequency( displayModeID, horizontalPixels, verticalLines, refreshRate );

/*
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();
	
	DisplayModeIDData *masterTable = (DisplayModeIDData*)&coreData->masterTable;
	UInt32 i;

	for (i = 0 ; i < kMaxDisplayModeIDs ; i++)
	{
		if (masterTable[i].displayModeID == displayModeID)
		{
			*verticalLines = masterTable[i].verticalLines;
			*horizontalPixels = masterTable[i].horizontalPixels;
			*refreshRate = masterTable[i].refreshRate;
			return kGDXErrNoError;
		}
	}
	return kGDXErrDisplayModeIDUnsupported;
*/
}
#endif


//=====================================================================================================
//
// GraphicsCoreKillPrivateData()
//	Return any resources that the Core has reserved back to the system.
//
//
//=====================================================================================================
void GraphicsCoreKillPrivateData(void)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();
		
	if (NULL != coreData->gammaTable)
	{
		PoolDeallocate(coreData->gammaTable);
		coreData->gammaTable = NULL;
	}

	RegistryEntryIDDispose(&coreData->regEntryID);
}



//=====================================================================================================
//
// GraphicsIntialize()
//	This routine is called after getting a 'kInitializeCommand' in DoDriverIO().  This is similar
//	to getting a 'kReplaceCommand', but with subtle differences.  A 'kInitializeCommand' is issued
//	if no version of this driver has been previously loaded, whereas a 'kReplaceCommand' is
//	issued if a previous version of the driver has been loaded.
//
//	This routine must perform the following operations:
//		• Initialize the Core's private data
//		• Initialize the HAL's private data
//		• Install VBL interrupts
//	
//=====================================================================================================
OSErr GraphicsInitialize(DriverRefNum refNum, const RegEntryID* regEntryID, const AddressSpaceID spaceID)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();
	GDXErr err = kGDXErrUnknownError;			// Assume failure

	Boolean alwaysFalse = false;				// Tell HAL to do full initialize, not a replacement.
	
	coreData->replacingDriver = false;			// Doing a kInitializeCommand, not a kReplaceCommand.

	coreData->graphicsCoreInited = true;
	coreData->doDisposeVBLandFBConnectInterruptService = true;
	

	// In the event that there was a 'CoreReplacementDriverInfo' property in the NameRegistry,
	// delete it to avoid confusion since a 'kInitializeCommand' is occurring.
	(void) GraphicsOSSDeleteProperty(&coreData->regEntryID, "CoreReplacementInfo");
	
	err = GraphicsCoreInitPrivateData(refNum, regEntryID, spaceID);
	if (err)
		goto ErrorExit;

	err = GraphicsHALInitPrivateData(regEntryID, &alwaysFalse);
	if (err)
		goto ErrorExit;
		
	err = GraphicsOSSInstallVBLInterrupts(regEntryID);
	if (err)
		goto ErrorExit;


ErrorExit:

	if (err)		
	{
		// Opps...got an internal error and jumped here.  Kill everything.
		GraphicsOSSKillPrivateData();
		GraphicsCoreKillPrivateData();
		GraphicsHALKillPrivateData();
		err = openErr;
	}

	return (OSErr) err;	
}



//=====================================================================================================
//
// GraphicsReplace()
//	This routine is called after getting a 'kReplaceCommand' in DoDriverIO().  This is similar
//	to getting a 'kInitializeCommand', but with subtle differences.  A 'kInitializeCommand' is issued
//	if no version of this driver has been previously loaded, whereas a 'kReplaceCommand' is
//	issued if a previous version of the driver has been loaded.
//
//	In order to minimize visual artifacts of the new driver 'replacing' the previous one, the Core
//	will attempt to retrieve some information from the NameRegistry which the 'superseded' version
//	of the driver left behind.
//
//	This routine must perform the following operations:
//		• Initialize the Core's private data
//		• Initialize the HAL's private data
//		• Install VBL interrupts
//	
//=====================================================================================================
OSErr GraphicsReplace(DriverRefNum refNum, const RegEntryID *regEntryID, const AddressSpaceID spaceID)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();
	GDXErr err = kGDXErrUnknownError;										// Assume failure
	CoreReplacementDriverInfo replacementDriverInfo;

	
	err = GraphicsCoreInitPrivateData(refNum, regEntryID, spaceID);
	if (err)
		goto ErrorExit;

	coreData->replacingDriver = true;			// Doing a kReplaceCommand, not a kInitializeCommand.						

	coreData->graphicsCoreInited = true;

	// Since a 'replacement' is being attempt, attempt to grab the Core data that was left behind by
	// the 'superseded' driver.  If it isn't found, set 'replacingDriver = false', and operation will
	// continue as if a 'kInitializeCommand' had been issued.
	
	err = GraphicsOSSGetProperty(&coreData->regEntryID, "CoreReplacementInfo",
			&replacementDriverInfo, sizeof(CoreReplacementDriverInfo));
	if (!err)
	{
		// Success!! Found the data left behind by the superseded driver.
		coreData->displayModeID = replacementDriverInfo.displayModeID;
		coreData->depthMode = replacementDriverInfo.depthMode;
		coreData->currentPage = replacementDriverInfo.currentPage;
		coreData->baseAddress = replacementDriverInfo.baseAddress;
	}
	else
	{
		// Opps...didn't find the data, so continue as if an 'kInitializeCommand' had been issued.
		coreData->replacingDriver = false;
	}

	// Always try to delete the CoreReplacementDriverInfo in the NameRegistry, so it will not confuse
	// subsequent operations.
	(void) GraphicsOSSDeleteProperty(&coreData->regEntryID, "CoreReplacementInfo");

	err = GraphicsHALInitPrivateData(regEntryID, &coreData->replacingDriver);
	if (err)
		goto ErrorExit;

	if ( !coreData->replacingDriver )
	{
		UInt32 needFullInitData = 1;
		(void) GraphicsOSSSaveProperty( regEntryID, "needFullInit", &needFullInitData,
										 sizeof( needFullInitData ), kOSSPropertyVolatile );
	}

	err = GraphicsOSSInstallVBLInterrupts(regEntryID);
	if (err)
		goto ErrorExit;

	
ErrorExit:

	if (err)
	{
		// Opps...got an internal error and jumped here.  Kill everything.
		GraphicsOSSKillPrivateData();
		GraphicsCoreKillPrivateData();
		GraphicsHALKillPrivateData();
		err = openErr;
	}

	return (OSErr) err;	

}



static void CheckPlatinumControlFatman(Boolean replacingDriver, DepthMode depthMode, Boolean *gotPlatinumOrControlAndNotFatman)
{
	RegEntryID regEntryID;
	Boolean	gotPlatinum = false;
	Boolean	gotControlAndNotFatman = false;
	GDXErr err;
	VDDisplayConnectInfoRec displayConnectInfo;
	Boolean didFind;

	*gotPlatinumOrControlAndNotFatman = false;


	if (depthMode != kDepthMode1)
		goto ErrorExit;

	if (!replacingDriver)
		goto ErrorExit;

	err = GraphicsCoreGetConnection(&displayConnectInfo);

	if (err)
		goto ErrorExit;

	switch (displayConnectInfo.csDisplayType)
	{
		case kMonoTwoPageConnect:
		case kFullPageConnect:
		case kUnknownConnect:
		case kHRConnect:
		case kColor16Connect:
		case kColorTwoPageConnect:
		case kColor19Connect:
			break;
		default:
			goto ErrorExit;
	}

	didFind = FindNamedRegEntry("platinum", &regEntryID);
	RegistryEntryIDDispose(&regEntryID);

	if (didFind)
		gotPlatinum = true;
	else
	{
		didFind = FindNamedRegEntry("control", &regEntryID);
		RegistryEntryIDDispose(&regEntryID);

		if (didFind)
		{
			didFind = FindNamedRegEntry("fatman", &regEntryID);
			RegistryEntryIDDispose(&regEntryID);

			if (!didFind)
				gotControlAndNotFatman = true;
		}
	}

	if (gotPlatinum || gotControlAndNotFatman)	// platinum || (control && !fatman)
		*gotPlatinumOrControlAndNotFatman = true;

ErrorExit:
	;
}


//=====================================================================================================
//
// GraphicsOpen()
//	This is called after getting a 'kOpenCommand' in DoDriverIO().
// 	This routine must perform the following:
//		• Instruct the HAL to open, putting the hardware in known state.
//		• Create a new interrupt service.
//		• Determine the type of display connected.
//		• Build a default gamma table.
//
//=====================================================================================================
OSErr GraphicsOpen(void)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();
	GDXErr err = kGDXErrUnknownError;			// Assume failure

	DisplayCode displayCode;					// Connected display
	DisplayCode savedDisplayCode;				// DisplayCode saved from previous boot 
	DisplayModeID displayModeID;				// Saved DisplayModeID from previous boot
	DepthMode depthMode;						// DepthMode to program hardware with 
	VDGammaRecord defaultGamma;					// Make GraphicsCoreSetGamma apply gamma table
	GammaTableID gammaTableID;					// ID of the default gamma table.
//	VDSwitchInfoRec switchInfo;					// Call GraphicsCoreSwitchMode to setup hw
	VDPageInfo pageInfo;						// Used to gray the screen
	VDFlagRecord flag;							// Used to turn on interrupts
	Boolean modePossible;						// Get preferred config, if DisplayCode hasn't changed,
												// make sure there is enough VRAM for the DepthMode 
												// and DisplayModeID.
												
	GraphicsPreferred graphicsPreferred;		// Saved DisplayModeID, DisplayModeID, & DisplayCode
	Boolean savePreferred = false;				// If the preferred configuartion from previous boot 
												// is no longer valid, save the new preferred config
	Boolean doFullInit = false;
	Boolean	gotPlatinumOrControlAndNotFatman = false;
	VDSyncInfoRec sync;
	

	// Be safe and init the 'defaultGamma.csGTable = NULL' so that it isn't mistakenly deallocated
	// in the event of an error.
	
	defaultGamma.csGTable = NULL;
	
	// If 'coreData->replacingDriver' is true, that means that the Core got a 'kReplaceCommand'
	// instead of a 'kInitialzeCommand'.  The HAL has already been told that a driver replacement
	// is being attempted. If the HAL managed to get all the information necessary, it would indicate
	// success by leaving 'coreData->replacingDriver = true'.  The HAL has an option to change that
	// variable if it requires that a full initialze take place.

	if (coreData->driverOpen)
	{
		err = kGDXErrDriverAlreadyOpen;
		goto ErrorExit;
	}
	
	coreData->gammaTable = NULL;				// No gamma table yet,
	coreData->maxGammaTableSize = 0;			// and likewise, gamma table buffer size is 0
	
	
	err = GraphicsHALSupportsHardwareCursor( &coreData->supportsHardwareCursor );
	if (err)
		goto ErrorExit;

	err = GraphicsHALOpen(coreData->spaceID, coreData->replacingDriver);	// Set hw in a known state
	if (err)
		goto ErrorExit;

	err = GraphicsOSSNewInterruptService( kVBLInterruptServiceType );
	if (err)
		goto ErrorExit;
	
	err = GraphicsOSSNewInterruptService( kFBConnectInterruptServiceType );
	if (err)
		goto ErrorExit;
	
	// Interrupt routines have been successfully installed.  Turn on interrupts by calling the
	// Core which will turn on interrupts and record the state of interrupts.
	// To enable interrupts, pass a 'csMode' value of 0
	
	flag.csMode = 0;
	err = GraphicsCoreSetInterrupt(&flag);
	if (err)
		goto ErrorExit;
	
	err = GraphicsHALGetUnknownRoutines( &coreData->reportsDDCConnection, 
	&coreData->readSenseLine2Proc, &coreData->readSenseLine1Proc, &coreData->senseLine2SetProc, &coreData->senseLine2ClearProc, &coreData->senseLine1SetProc, &coreData->senseLine1ClearProc,
	&coreData->senseLine2ResetProc, &coreData->senseLine1ResetProc, &coreData->senseLine2and1ResetProc, &coreData->resetSenseLinesProc, &coreData->readSenseLinesProc, &coreData->setDDCInfoProc );
	if (err)
		coreData->reportsDDCConnection = false;


	// Read the sense lines and determine what type of monitor is connected.
	err = GraphicsHALDetermineDisplayCode(&displayCode, &coreData->hasDDCConnection, &coreData->builtInConnection);
	if (err)
		goto ErrorExit;
		
	
	if (kDisplayCodeNoDisplay == displayCode)
	{
		ResType	ResTypeXPRAMCode;

		ReadXPRam(&ResTypeXPRAMCode, 4, 0x00FC);

		err = GraphicsUtilMapXPRAMToDispCode(ResTypeXPRAMCode, &displayCode);
		if (err)
			goto ErrorExit;

		if (kDisplayCodeNoDisplay == displayCode)
		{
			// Exit if no display is connected, but first update the 'preferredConfiguration' to 
			// to show that no monitor is attached.
			
			graphicsPreferred.ddcChecksum = 0;
			graphicsPreferred.displayModeID = kDisplay640x480At67Hz;
			graphicsPreferred.depthMode = kDepthMode1;
			graphicsPreferred.displayCode = kDisplayCodeNoDisplay;

			(void) GraphicsOSSSetCorePref(&coreData->regEntryID, &graphicsPreferred);

			err = kGDXErrNoConnectedMonitor;
			goto ErrorExit;
		}
	}
	coreData->displayCode = displayCode; 				// Save the DisplayCode in the Core's data
	
	if ((kDisplayCode21InchMono == displayCode) || (kDisplayCodePortraitMono == displayCode))
		 coreData->monoOnly = true;
	else
		coreData->monoOnly = false;				

	// Find the 'DisplayModeID' and the 'DepthMode' from the previous boot.

	err = GraphicsOSSGetCorePref(&coreData->regEntryID, &graphicsPreferred);
	savedDisplayCode = graphicsPreferred.displayCode;



	if ( coreData->hasDDCConnection )
	{
		if (coreData->ddcBlockData.checksum != graphicsPreferred.ddcChecksum)
		{
			savedDisplayCode = ~displayCode;
			if (coreData->replacingDriver)
			{
				doFullInit = true;
				coreData->replacingDriver = false;
			}
		}
	}
	else
	{
		if (!coreData->builtInConnection)
		{
			if (graphicsPreferred.ddcChecksum)
				savePreferred = true;
		}
	}



	if (err || (savedDisplayCode != displayCode))	
	{
		// Got an error, meaning something is wrong with the saved info, OR the 'DisplayCode' has 
		// changed from the previous boot, so get the best settings for the connected display.
	
		// Set flag to save the preferred configuration since current data stored is inaccurate.
		savePreferred = true;
		
		err	= GraphicsHALGetDefaultDisplayModeID(displayCode, &displayModeID, &depthMode);
	
		if (err)
			goto ErrorExit;
			
	}
	else
	{
		// Successfully retrieved the saved info and 'savedDisplayCode == displayCode'
		if (coreData->replacingDriver)
		{
			// Driver replacement is occurring, but the preferred settings don't match those of
			// the driver which was superseded.  Therefore, to avoid excessive visual artifacts,
			// use the settings from the superseded driver.
			
			savePreferred = true;
			if (graphicsPreferred.displayModeID != coreData->displayModeID)
				displayModeID = coreData->displayModeID;	// DisplayModeID from closed driver
			else
				displayModeID = graphicsPreferred.displayModeID;

			if (graphicsPreferred.depthMode != coreData->depthMode)
				depthMode = coreData->depthMode;			// DepthMode from closed driver
			else
				depthMode = graphicsPreferred.depthMode;
		}
		else
		{
			// Driver replacement WAS NOT occurring, so use the settings retrieved from the 
			// 'graphicsPreferred' property.

			displayModeID = graphicsPreferred.displayModeID;
			depthMode = graphicsPreferred.depthMode;
		}
	}	

	if (!coreData->replacingDriver)
		coreData->currentPage = 0;					// Hard code current page to zero
		
	
	err = GraphicsHALModePossible(displayModeID, depthMode, coreData->currentPage, &modePossible);

	if (err || !modePossible)
	{	
		// The HAL did not believe it could drive the specified mode, so try one last time to
		// determine a configuration that the HAL can support for this display.
		
		if (coreData->replacingDriver)
		{
			// Since the HAL can't handle the specified mode, mark 'coreData->replacingDriver' as
			// false, to insure that FULL hardware initialization occurrs.  Additionally, indicate
			// that new preferred settings should be saved.
			
			coreData->replacingDriver = false;
			doFullInit = true;
			savePreferred = true;
		}
			
		err	= GraphicsHALGetDefaultDisplayModeID(displayCode, &displayModeID, &depthMode);
		if (err)
			goto ErrorExit;
	}
	
	// Fill in the Core data.
	coreData->displayCode = displayCode;			
	coreData->displayModeID = displayModeID;		
	coreData->depthMode = depthMode;				
	coreData->luminanceMapping = false;					// Default to no luminance mapping

	err = GraphicsHALMapDepthModeToBPP(depthMode, &coreData->bitsPerPixel);
	if (err)
		goto ErrorExit;

	
	if (16 <= coreData->bitsPerPixel)
		coreData->directColor = true;			// Direct color when 16, 32 bpp
	else
		coreData->directColor = false;			// Not direct color when < 16 bpp

			
	if (!coreData->replacingDriver && coreData->graphicsCoreInited)
	{
		// If 'coreData->replacingDriver' was true, it would mean a valid raster and is being driven.
		// However, it currently isn't, so the hardware needs to be programmed to the specified
		// mode and the screen needs to be grayed.  This can be accomplished by calling 
		// GraphicsHALProgramHardware() followed by a call to GraphicsCoreGrayPage().

		void *baseAddress;
		Boolean directColor;
		
		err = GraphicsHALProgramHardware(displayModeID, depthMode, 0, &directColor, &(char*)baseAddress);

		if (err)
			goto ErrorExit;

		// Request has been successfully completed, so update the coreData to relfect the current state.
		
		coreData->displayModeID = displayModeID;
		coreData->depthMode = depthMode;
		coreData->currentPage = 0;
		coreData->baseAddress = baseAddress;
		coreData->directColor = directColor;

		// Gray the screen
		pageInfo.csPage = 0;								// Again, hard code for page 0
		err = GraphicsCoreGrayPage(&pageInfo);
		
		if (err)
			goto ErrorExit;
	}

	coreData->time5secondsAfterOpen = AddAbsoluteToAbsolute(UpTime(), coreData->delay5secs);
	CheckPlatinumControlFatman(coreData->replacingDriver, coreData->depthMode, &gotPlatinumOrControlAndNotFatman);



	// Attempt to retrieve a default gamma table for the connected display, but if that fails, then
	// apply a linear ramp.
	// Conviently, GraphicsCoreSetGamma will do all the low level stuff, AND IT WILL set
	// coreData->gammaTable appropriately.

	err = GraphicsUtilGetDefaultGammaTableID(coreData->displayCode, &gammaTableID);
	
	if (!err && !gotPlatinumOrControlAndNotFatman)
	{
		// Successfully got the ID of the default gamma table.  So retrieve it and then apply it.

		VDRetrieveGammaRec retrieveGamma;
		VDGetGammaListRec getGammaList;
		char gammaTableName[32];										// Spot for C-String name
		
		getGammaList.csPreviousGammaTableID = kGammaTableIDSpecific;	// Want info on specific ID
		getGammaList.csGammaTableID = gammaTableID;
		getGammaList.csGammaTableName = (char*)&gammaTableName;
		
		err = GraphicsCoreGetGammaInfoList(&getGammaList);
		if (err)
			goto ErrorExit;

		defaultGamma.csGTable = PoolAllocateResident(getGammaList.csGammaTableSize, true);
		if (NULL == defaultGamma.csGTable)
		{
			err = kGDXErrUnableToAllocateGammaTable;
			goto ErrorExit;
		}
		
		retrieveGamma.csGammaTableID = gammaTableID;
		retrieveGamma.csGammaTablePtr = (GammaTbl*)defaultGamma.csGTable;		// Point to the storage
		
		
		err = GraphicsCoreRetrieveGammaTable(&retrieveGamma);
		if (err)
			goto ErrorExit;
					
		err = GraphicsCoreSetGamma(&defaultGamma);
		if (err)
			goto ErrorExit;
	}
	else
	{
		// Unable to find the ID of a default gamma table, so just apply a linear ramp.
		// Conviently, GraphicsCoreSetGamma will build a default gamma table if we pass it
		// a VDGammaRecord with the csGTable field set to NULL.  AND IT WILL set coreData->gammaTable.

		defaultGamma.csGTable = NULL;
		err = GraphicsCoreSetGamma(&defaultGamma);
		
		if (err)						
			goto ErrorExit;
	}
	

	if (savePreferred)
	{
		graphicsPreferred.displayModeID = displayModeID;
		graphicsPreferred.depthMode = depthMode;
		graphicsPreferred.displayCode = displayCode;
		

		if (coreData->hasDDCConnection)
			graphicsPreferred.ddcChecksum = coreData->ddcBlockData.checksum;
		else if (!coreData->builtInConnection)
			graphicsPreferred.ddcChecksum = 0;


		err = GraphicsOSSSetCorePref(&coreData->regEntryID, &graphicsPreferred);
	}
	
	if (coreData->processErrorProc)
	{
		err = (coreData->processErrorProc)(err);
		if (err)
			goto ErrorExit;
	}

	coreData->triStateSyncOn = false;
	coreData->driverOpen = true;						// Driver successfully opened
	coreData->hasTriStateSync = false;

	err = GraphicsHALGetSync( true, &sync );

	if (err)
		err = noErr;	// If we couldn't save the prefs, don't bail out
	else
	{
		if (sync.csMode & kTriStateSyncMask)
			coreData->hasTriStateSync = true;
	}


	if (doFullInit)
	{
		UInt32 needFullInitData = 1;

		(void) GraphicsOSSSaveProperty(&coreData->regEntryID, "needFullInit",
			&needFullInitData, sizeof(needFullInitData), kOSSPropertyVolatile);
	}

ErrorExit:

	if (err)		
	{
		// Opps...got an internal error and jumped here.  Kill everything
		GraphicsOSSKillPrivateData();
		GraphicsCoreKillPrivateData();
		GraphicsHALKillPrivateData();
		err = openErr;
	}

	if (NULL != defaultGamma.csGTable)					// Deallocate default gamma table
		PoolDeallocate(defaultGamma.csGTable);


	// Always set replacingDriver to false when leaving.  Only a 'kReplaceCommand' should set that flag
	// to true and it should be reset to false after each open.
	
	coreData->replacingDriver = false;

	return (OSErr)err;	
}



//=====================================================================================================
//
// GraphicsClose()
//	This is called after getting a 'kCloseCommand' in DoDriverIO().
// 	This routine must perform the following:
//		• Disable VBL interrupts (HAL).
//		• Remove the interrupt handler installed by the driver, restoring any vectors it changed during
//		  installation (OSS).
//		• Instruct the HAL to close.
// 
//=====================================================================================================
OSErr GraphicsClose(void)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();
	GDXErr err = kGDXErrUnknownError;						// Assume failure.
	VDFlagRecord flag;										// Used to turn off interrupts.

	if (false == coreData->driverOpen)
	{
		err = kGDXErrNoError;								// Shouldn't happen but...
		goto ErrorExit;
	}
	
	flag.csMode = 1;										// Disable interrupts.
	err = GraphicsCoreSetInterrupt(&flag);
	if (err)
		goto ErrorExit;

	if (NULL != coreData->gammaTable)						// Deallocate gamma table
	{
		PoolDeallocate(coreData->gammaTable);
		coreData->gammaTable = NULL;
		coreData->maxGammaTableSize = 0;
	}

	if (coreData->doDisposeVBLandFBConnectInterruptService)
	{
		err = GraphicsOSSDisposeInterruptService( kVBLInterruptServiceType );
		if (err)
			goto ErrorExit;

		err = GraphicsOSSDisposeInterruptService( kFBConnectInterruptServiceType );
		if (err)
			goto ErrorExit;
	}

	err = GraphicsHALClose(coreData->spaceID);
	if (err)
		goto ErrorExit;
	
	coreData->driverOpen = false;							// Driver successfully closed.

ErrorExit:

	return  noErr;

}



//=====================================================================================================
//
// GraphicsControl()
//	This is called after getting a 'kControlCommand' in DoDriverIO(). This is the bottleneck for
//	control calls.
//
//=====================================================================================================
OSErr GraphicsControl(CntrlParam *pb)
{

	OSErr returnErr = noErr;					// Return REAL external error when not debugging
	GDXErr err = kGDXErrNoError;				// Holds internal errors
	void *genericPtr;
	
	// The 'csParam' field of the 'CntrlParam' stucture is defined as 'short csParam[11]'.  This is
	// meant for 'operation defined parameters.' For the graphics driver, only the first 4 bytes are
	// used.  They are used as a pointer to another structure.
	// To help code readability, the pointer will be extracted as a generic 'void *' and then cast as
	// appropriate.

	genericPtr = (void *) *((UInt32 *) &(pb->csParam[0]));

	switch (pb->csCode)				// The control call being made
	{
		
	   	case cscReset:				// Old obsolete call..return a 'controlErr'
			returnErr = controlErr;
			goto ErrorExit;
			break;
			
	   	case cscKillIO:				// Old obsolete call..do nothing
			err = kGDXErrNoError;
			break;

	   	case cscSetMode:
			err = GraphicsCoreSetMode((VDPageInfo *) genericPtr);
			break;
			
	   	case cscSetEntries:
			err = GraphicsCoreSetEntries((VDSetEntryRecord *) genericPtr);
			break;
			
	   	case cscSetGamma:
			err = GraphicsCoreSetGamma((VDGammaRecord *) genericPtr);
			break;
			
	   	case cscGrayPage:
			err = GraphicsCoreGrayPage((VDPageInfo *) genericPtr);
			break;
			
	   	case cscSetGray:
			err = GraphicsCoreSetGray((VDGrayRecord *) genericPtr);
			break;
			
	   	case cscSetInterrupt:
			err = GraphicsCoreSetInterrupt((VDFlagRecord *) genericPtr);
			break;
			
	   	case cscDirectSetEntries:
			err = GraphicsCoreDirectSetEntries((VDSetEntryRecord *) genericPtr);
			break;
			
	   	case cscSetDefaultMode:			// Old call that doesn't work in Slot Mgr Independent (SMI) API
			returnErr = controlErr;
			goto ErrorExit;
			break;
			
	   	case cscSwitchMode:
			err = GraphicsCoreSwitchMode((VDSwitchInfoRec *) genericPtr);
			break;
			
	   	case cscSetSync:
			err = GraphicsCoreSetSync((VDSyncInfoRec *) genericPtr);
			break;
		
		case cscSavePreferredConfiguration:
			err = GraphicsCoreSetPreferredConfiguration((VDSwitchInfoRec *) genericPtr);
			break;

	   	case cscSetHardwareCursor:
			err = GraphicsCoreSetHardwareCursor((VDSetHardwareCursorRec *) genericPtr);
			break;

	   	case cscDrawHardwareCursor:
			err = GraphicsCoreDrawHardwareCursor((VDDrawHardwareCursorRec *) genericPtr);
			break;

	   	case cscSetPowerState:
			err = GraphicsCoreSetPowerState((VDPowerStateRec *) genericPtr);
			break;

	   	case cscSetClutBehavior:
			err = GraphicsCoreSetClutBehavior((VDClutBehavior *) genericPtr);
			break;

		case cscSetDetailedTiming:
			err = GraphicsCoreSetDetailedTiming((VDDetailedTimingRec *) genericPtr);
			break;

		default:							// This will return the appropriate error code
			returnErr = GraphicsHALPrivateControl(genericPtr, pb->csCode);
			goto ErrorExit;
	}
	
	if (err == kGDXErrUnsupportedFunctionality)
		returnErr = controlErr;
	else
		if (err)
			returnErr = paramErr;
		else
			returnErr = noErr;

ErrorExit:

	return returnErr;

}	


//=====================================================================================================
//
// GraphicsStatus()
//	This is called after getting a 'kStatusCommand' in DoDriverIO(). This is the bottleneck for
//	status calls.
// 
//=====================================================================================================
OSErr GraphicsStatus(CntrlParam	*pb)
{

	OSErr returnErr = noErr;					// return REAL external error when not debugging
	GDXErr err = kGDXErrNoError;				// holds internal errors
//	DisplayCode unusedDisplayCode;				// Get Preferred returns the display type also for
												// internal use
	void *genericPtr;
	
	// The 'csParam' field of the 'CntrlParam' stucture is defined as 'short csParam[11]'.  This is
	// meant for 'operation defined parameters.' For the graphics driver, only the first 4 bytes are
	// used.  They are used as a pointer to another structure.
	// To help code readability, the pointer will be extracted as a generic 'void *' and then cast as
	// appropriate.

	genericPtr = (void *) *((UInt32 *) &(pb->csParam[0]));
	
	switch (pb->csCode)					// The status call being made
	{
		
	   	case cscGetMode:
			err = GraphicsCoreGetMode((VDPageInfo *) genericPtr);
			break;
			
	   	case cscGetEntries:
			err = GraphicsCoreGetEntries((VDSetEntryRecord *) genericPtr);
			break;
			
	   	case cscGetPages:
			err = GraphicsCoreGetPages((VDPageInfo *) genericPtr);
			break;
			
		case cscGetBaseAddr:
			err = GraphicsCoreGetBaseAddress((VDPageInfo *) genericPtr);
			break;
			
	   	case cscGetGray:
			err = GraphicsCoreGetGray((VDGrayRecord *) genericPtr);
			break;
			
	   	case cscGetInterrupt:
			err = GraphicsCoreGetInterrupt((VDFlagRecord *) genericPtr);
			break;
			
	   	case cscGetGamma:
			err = GraphicsCoreGetGamma((VDGammaRecord *) genericPtr);
			break;
			
	   	case cscGetDefaultMode:			// Old call that doesn't work in Slot Mgr Independent (SMI) API
			err = kGDXErrUnknownError;
			break;
			
	   	case cscGetCurMode:
			err = GraphicsCoreGetCurrentMode((VDSwitchInfoRec *) genericPtr);
			break;
						
	   	case cscGetSync:
			err = GraphicsCoreGetSync((VDSyncInfoRec *) genericPtr);
			break;
						
	   	case cscGetConnection:
			err = GraphicsCoreGetConnection((VDDisplayConnectInfoRec *) genericPtr);
			break;
						
	   	case cscGetModeTiming:
			err = GraphicsCoreGetModeTiming((VDTimingInfoRec *) genericPtr);
			break;
						
		case cscGetPreferredConfiguration:
			err = GraphicsCoreGetPreferredConfiguration((VDSwitchInfoRec *) genericPtr);
			break;

		case cscGetNextResolution:
			err = GraphicsCoreGetNextResolution((VDResolutionInfoRec *) genericPtr);
			break;

	   	case cscGetVideoParameters:
			err = GraphicsCoreGetVideoParams((VDVideoParametersInfoRec *) genericPtr);
			break;
	   
	   	case cscGetGammaInfoList:
			err = GraphicsCoreGetGammaInfoList((VDGetGammaListRec *) genericPtr);
			break;

	   	case cscRetrieveGammaTable:
			err = GraphicsCoreRetrieveGammaTable((VDRetrieveGammaRec *) genericPtr);
			break;

		case cscSupportsHardwareCursor:
			err = GraphicsCoreSupportsHardwareCursor((VDSupportsHardwareCursorRec *) genericPtr);
			break;

		case cscGetHardwareCursorDrawState:
			err = GraphicsCoreGetHardwareCursorDrawState((VDHardwareCursorDrawStateRec *) genericPtr);
			break;

	   	case cscGetPowerState:
			err = GraphicsCoreGetPowerState((VDPowerStateRec *) genericPtr);
			break;

		case cscGetDDCBlock:
			err = GraphicsCoreGetDDCBlock((VDDDCBlockRec *) genericPtr);
			break;
		
		case cscGetClutBehavior:
			err = GraphicsCoreGetClutBehavior((VDClutBehavior *) genericPtr);
			break;

		case cscGetTimingRanges:
			err = GraphicsCoreGetTimingRanges((VDDisplayTimingRangeRec *) genericPtr);
			break;

		case cscGetDetailedTiming:
			err = GraphicsCoreGetDetailedTiming((VDDetailedTimingRec *) genericPtr);
			break;

		default:							// This will return the appropriate error code
			returnErr = GraphicsHALPrivateStatus(genericPtr, pb->csCode);
			goto ErrorExit;
			
	}

	if (err)
		returnErr = paramErr;
	else
		returnErr = noErr;
	
ErrorExit:
	
	return returnErr;
}



//=====================================================================================================
//
// GraphicsFinalize()
//	This routine is called after getting a 'kFinalizeCommand' in DoDriverIO().  This is similar
//	to getting a 'kSupersededCommand', but with subtle differences.  A 'kFinalizeCommand' is issued
//	if the driver is going away for good, whereas as a 'kSupersededCommand' is issued if the driver
//	will be replaced with a later, greater version.
//	This will do the following:
//		• Instruct the HAL to terminate, turning off its raster
//		• Instruct the Core to kill its private data
//		• Instruct the HAL to kill its private data
//		(NOTE: the OSS already killed its private data, when it recieved the 'kFinalizeCommand')
//
//=====================================================================================================
OSErr GraphicsFinalize(DriverRefNum refNum, const RegEntryID* regEntryID)
{
	#pragma unused( refNum, regEntryID )
	// Release LOCAL Storage
		
	(void) GraphicsHALTerminate(false);		// The 'false' indicates this is a 'kFinalizeCommand', and
											// not a 'kSupersededCommand'.
	GraphicsCoreKillPrivateData();
	GraphicsHALKillPrivateData();

	return noErr;

}



//=====================================================================================================
//
// GraphicsSupersede()
//	This routine is called after getting a 'kSupersededCommand' in DoDriverIO().  This is similar
//	to getting a 'kFinalizeCommand', but with subtle differences.  A 'kFinalizeCommand' is issued
//	if the driver is going away for good, whereas as a 'kSupersededCommand' is issued if the driver
//	will be replaced with a later, greater version.
//	This will do the following:
//		• Save key portions of the Core data, so that the replacement driver can attempt to come
//		  come up in the same state.
//		• Instruct the HAL to terminate, but leave its raster on.
//		• Instruct the Core to kill its private data
//		• Instruct the HAL to kill its private data
//		(NOTE: the OSS already killed its private data, when it recieved the 'kSupersededCommand')
//	
//=====================================================================================================
OSErr GraphicsSupersede(DriverRefNum refNum, const RegEntryID* regEntryID)
{
	#pragma unused( refNum, regEntryID )

	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();
	CoreReplacementDriverInfo replacementDriverInfo;

	replacementDriverInfo.displayModeID = coreData->displayModeID;
	replacementDriverInfo.depthMode = coreData->depthMode;
	replacementDriverInfo.currentPage = coreData->currentPage;
	replacementDriverInfo.baseAddress = coreData->baseAddress;

	
	(void) GraphicsOSSSaveProperty(&coreData->regEntryID, "CoreReplacementInfo",
			&replacementDriverInfo, sizeof(CoreReplacementDriverInfo), kOSSPropertyVolatile);
		
	(void) GraphicsHALTerminate(true);		// The 'true' indicates this is a 'kSupersededCommand', and
											// not a 'kFinalizeCommand'.
	GraphicsCoreKillPrivateData();
	GraphicsHALKillPrivateData();

	return noErr;

}


#if 0

Unimplemented control and status codes

√ = implemented by Joe van Tunen


enum {
                                        /* Control Codes */
	cscPrivateControlCall         = 26,   /* Takes a VDPrivateSelectorDataPtr */
	cscSetMultiConnect            = 28,   /* Takes a VDMultiConnectInfoPtr */

√	cscSetDetailedTiming          = 31,   /* Takes a VDDetailedTimingPtr */
	cscDoCommunication            = 33,   /* Takes a VDCommunicationPtr */
	cscProbeConnection            = 34,   /* Takes nil pointer (may generate a kFBConnectInterruptServiceType service interrupt)*/

	cscSetScaler                  = 36,   /* Takes a VDScalerPtr*/
	cscSetMirror                  = 37,   /* Takes a VDMirrorPtr*/
	cscSetFeatureConfiguration	= 38,	/* Takes a VDConfigurationPtr*/

};

enum {
                                        /* Status Codes */
	cscGetModeBaseAddress         = 14,   /* Return base address information about a particular mode */
	cscGetScanProc                = 15,   /* QuickTime scan chasing routine */
	cscGetConvolution             = 24,   /* Takes a VDConvolutionInfoPtr */
	cscPrivateStatusCall          = 26,   /* Takes a VDPrivateSelectorDataPtr */
	cscGetMultiConnect            = 28,   /* Takes a VDMultiConnectInfoPtr */
√	cscGetTimingRanges            = 30,   /* Takes a VDDisplayTimingRangePtr */
√	cscGetDetailedTiming          = 31,   /* Takes a VDDetailedTimingPtr */
	cscGetCommunicationInfo       = 32,   /* Takes a VDCommunicationInfoPtr */
	cscGetScalerInfo              = 35,   /* Takes a VDScalerInfoPtr */
	cscGetScaler                  = 36,   /* Takes a VDScalerPtr*/
	cscGetMirror                  = 37,   /* Takes a VDMirrorPtr*/
	cscGetFeatureConfiguration    = 38    /* Takes a VDConfigurationPtr*/
};

#endif
