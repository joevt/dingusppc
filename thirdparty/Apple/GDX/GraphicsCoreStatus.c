/*
	File:		GraphicsCoreStatus.c

	Contains:	Implements the 'core' portions of the graphics Status calls.

	Written by:	Sean Williams, Kevin Williams

	Copyright:	(c) 1994-1995 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

		 <2>	 7/17/95	SW		In GraphicsCore GetConnection(), now reporting new constants for
		 							16, 19, and 21 inch color monitors.
		 <1>	 4/15/95	SW		First Checked In

*/

#include "GraphicsCoreStatus.h"
#include "GraphicsCorePriv.h"
#include "GraphicsPriv.h"
#include "GraphicsCoreUtils.h"
#include "GraphicsHAL.h"
#include "GraphicsOSS.h"

#include <DriverServices.h>
#include <Video.h>

// Forward declarations
static Boolean GammaTableApplicable(GammaTableID gammaTableID, DisplayCode displayCode);

typedef GDXErr RetrieveGammaFunction(ByteCount *size, char *name, GammaTbl *gammaTbl);
static RetrieveGammaFunction RetrieveGammaStandard;
static RetrieveGammaFunction RetrieveGammaPageWhite;
static RetrieveGammaFunction RetrieveGammaGray;
static RetrieveGammaFunction RetrieveGammaRubik;
static RetrieveGammaFunction RetrieveGammaNTSCPAL;
static RetrieveGammaFunction RetrieveGammaCSCTFT;



//=====================================================================================================
//
// GraphicsCoreGetMode()
//	This routine returns the current relative pixel depth, current page, and the base address of the
//	frame buffer for the current page.
//
//=====================================================================================================
GDXErr GraphicsCoreGetMode(VDPageInfo *pageInfo)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData() ;
	GDXErr err = kGDXErrUnknownError;								// Assume failure
	
	pageInfo->csMode = (SInt16) coreData->depthMode;
	pageInfo->csPage = coreData->currentPage;
	pageInfo->csBaseAddr = coreData->baseAddress;
	
	err = kGDXErrNoError ;											// Everything Okay
		
ErrorExit:

	return (err);
}



//=====================================================================================================
//
// GraphicsCoreGetEntries()
//	This routine must return the specified number of consecutive CLUT entries, starting with the
//	specified first entry.  If gamma table correction is used, the values returned may not be the same
//	as the values orignally passed by cscSetEntries.  If the value of csStart is 0 or positive, the
//	routine must return csCount entries starting at that position.  If it is -1, the routine must
//	access the contents of the 'value' fields in csTable to determine which entries are to be returned.
//	Both csStart and csCount are zero based; their values are l less than the desired amount.
//
//	Although direct video modes do not have logical color tables, the cscGetEntries status routine
//	should continue to return the current contents of the CLUT, just as it would in an indexed mode.
//
//=====================================================================================================
GDXErr GraphicsCoreGetEntries(VDSetEntryRecord *setEntry)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData() ;
	GDXErr err = kGDXErrUnknownError;								// Assume failure

	SInt16 startPosition;
	SInt16 numberOfEntries;
	Boolean sequential;

		
	// Make sure that 'setEntry' is pointing to a valid 'VDSetEntryRecord'.  Additionally, extract
	// the 'startPosition' and 'numberOfEntries' from the 'VDSetEntryRecord', and determine if this
	// is a 'Sequential' or 'Indexed' flavor.
	
	err = GraphicsUtilCheckSetEntry(setEntry, coreData->bitsPerPixel, &startPosition, &numberOfEntries, 
			&sequential);
	
	if (err)
		goto ErrorExit;

	err = GraphicsHALGetCLUT(setEntry->csTable, startPosition, numberOfEntries, sequential,
			coreData->depthMode);

	if (err)
		goto ErrorExit;

	err = kGDXErrNoError ;											// Everything Okay
		
ErrorExit:

	return (err);
}



//=====================================================================================================
//
// GraphicsCoreGetPages()
//	This returns the total number of graphics pages available in the specified relative pixel depth.
//	This is a counting number (not zero based).
//
//	For this routine, the relevant fields of the 'VDPageInfo' structure are as follows:
//			->	csMode			relative depth mode for which the page count is desired
//			<-	csPage			Number of pages supported at the specified relative depth mode
//
//=====================================================================================================
GDXErr GraphicsCoreGetPages(VDPageInfo *pageInfo)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData() ;
	GDXErr err = kGDXErrUnknownError;								// Assume failure

	SInt16 pageCount;
			
	err = GraphicsHALGetPages(coreData->displayModeID, pageInfo->csMode, &pageCount);
	if (err)
		goto ErrorExit;
	
	pageInfo->csPage = pageCount;									// Report the number of pages
	
	err = kGDXErrNoError ;											// Everything Okay
		
ErrorExit:

	return (err);
}



//=====================================================================================================
//
// GraphicsCoreGetBaseAddress()
//	This returns the base address of a specified page for the current DisplayModeID and DepthMode. 
//	This allows video pages to be written to even when not displayed.
//
//	For this routine, the relevant fields of the 'VDPageInfo' structure are as follows:
//			->	csPage			page number ( 0 based ).  Return the base address for this page
//			<-	csBaseAddr		base address of desired page
//
//=====================================================================================================
GDXErr GraphicsCoreGetBaseAddress(VDPageInfo *pageInfo)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData() ;
	GDXErr err = kGDXErrUnknownError;								// Assume failure
	
	pageInfo->csBaseAddr = NULL;									// Assume failure
	
	err = GraphicsHALGetBaseAddress( pageInfo->csPage, (char **)&pageInfo->csBaseAddr );
	if (err)
		goto ErrorExit;

	err = kGDXErrNoError ;											// Everything Okay
		
ErrorExit:

	return (err);
}



//=====================================================================================================
//
// GraphicsCoreGetGray()
//	This routine must returns a value indicating whether the cscSetEntries routine has been conditioned
//	to fill a card's CLUT with actual color or with the luminance-equivalent gray tones.  For actual
//	colors (the default case), the value return csMode is 0; for gray tones it is 1.
//
//=====================================================================================================
GDXErr GraphicsCoreGetGray(VDGrayRecord *gray)
{

	GDXErr err = kGDXErrUnknownError;								// Assume failure
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData() ;
	
	if (coreData->luminanceMapping)		
		gray->csMode = 1;
	else
		gray->csMode = 0;
		
	err = kGDXErrNoError ;											// Everything Okay
			
ErrorExit:

	return (err);
}



//=====================================================================================================
//
// GraphicsCoreGetInterrupt()
//	This returns 'csMode = 0' if VBL interrupts are enabled and a value of 1 if
//	VBL interrupts are disabled.
//
//=====================================================================================================
GDXErr GraphicsCoreGetInterrupt(VDFlagRecord *flag)
{

	GDXErr err = kGDXErrUnknownError;								// Assume failure
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData() ;
			
	if (coreData->interruptsEnabled)		
		flag->csMode = 0;											// Report interrupts enabled
	else
		flag->csMode = 1;											// Report interrupts disabled

	err = kGDXErrNoError ;											// Everything Okay
		
ErrorExit:

	return (err);
}



//=====================================================================================================
//
// GraphicsCoreGetGamma()
//	This returns a pointer to the current gamma table.  The calling application cannot preallocate
//	memory because of the unknown size requirements of the gamma data structure.
//
//=====================================================================================================
GDXErr GraphicsCoreGetGamma(VDGammaRecord *gamma)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData() ;
	GDXErr err = kGDXErrUnknownError;								// Assume failure

	if (NULL == coreData->gammaTable)
		gamma->csGTable = NULL;										// No gamma table in use
	else
		gamma->csGTable = (Ptr) coreData->gammaTable;				// Return Ptr to gamma table
		
	err = kGDXErrNoError ;											// Everything Okay
		
ErrorExit:

	return (err);
}



//=====================================================================================================
//
// GraphicsCoreGetCurrentMode()
//	This is used to gather information about the current display mode.
//	When called, this routine fills out a VDSwitchInfoRec with the appropriate data.
//
//	The Display Manager also uses the cscGetCurMode status request to determine whether a graphics
//	driver supports the Display Manager. If the driver returns the 'statusErr', system software assumes
//	that the driver does not support the Display Manager.
//
//	For this routine, the relevant fields of the 'VDSwitchInfoRec' structure are as follows:
//			<-	csMode			relative depthmode of the current pixel depth
//			<-	csData			DisplayModeID for the current resolution
//			<-	csPage			number of the current video page (0 based)
//			<-	csBaseAddr		base address of page specified in the csPage field
//
//=====================================================================================================
GDXErr GraphicsCoreGetCurrentMode(VDSwitchInfoRec *switchInfo)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData() ;
	GDXErr err = kGDXErrUnknownError;								// Assume failure
	
	// Return the current depth mode, display mode, page, and base address.
	
	switchInfo->csMode = (UInt16) coreData->depthMode;
	switchInfo->csData = (UInt32) coreData->displayModeID;
	switchInfo->csPage = (UInt16) coreData->currentPage;
	switchInfo->csBaseAddr = coreData->baseAddress;
	
	err = kGDXErrNoError ;											// Everything Okay
		
ErrorExit:

	return (err);
}



//=====================================================================================================
//
// GraphicsCoreGetSync()
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
//
//	For this routine, the relevant fields of the 'VDSyncInfoRec' structure are as follows:
//			<>	csMode		Report HW capability or current state.
//
//				If csMode == 0xFF, then report the cabability of the HW
//				When reporting the capability of the HW, set the appropriate bits of csMode:
//				kDisableHorizontalSyncBit		Set if HW can disable Horizontal Sync
//				kDisableVerticalSyncBit			Set if HW can disable Vertical Sync
//				kDisableCompositeSyncBit		Set if HW can disable Composite Sync
//				kSyncOnRedEnableBit				Set if HW can sync on Red
//				kSyncOnGreenEnableBit			Set if HW can sync on Green
//				kSyncOnBlueEnableBit			Set if HW can sync on Blue
//				kNoSeparateSyncControlBit		Set if HW CANNOT enable/disable H,V,C sync independently
//												Means that HW ONLY supports the VESA "OFF" state
//
//				If csMode == 0, then report the current state of sync lines and  if HW is
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
//				To report if HW is "syncing" on Red, Green or Blue:
//				kEnableSyncOnBlue				Set if HW is "syncing" on Blue
//				kEnableSyncOnGreen				Set if HW is "syncing" on Blue
//				kEnableSyncOnRed				Set if HW is "syncing" on Blue
//
//
//=====================================================================================================
GDXErr GraphicsCoreGetSync(VDSyncInfoRec *sync)
{
	GDXErr err = kGDXErrUnknownError;					// Assume failure
	Boolean getHardwareSyncCapability;					// True if HAL should report HW cabapability.

	switch (sync->csMode)
	{
		case 0xff:
			getHardwareSyncCapability = true;
			break;
		case 0:
			getHardwareSyncCapability = false;
			break;
		default:
			err = kGDXErrInvalidParameters;
			goto ErrorExit;
			break;
	}
	
	err = GraphicsHALGetSync(getHardwareSyncCapability, sync);						
		
ErrorExit:

	return (err);
}



//=====================================================================================================
//
// GraphicsCoreGetConnection()
//	This is used to gather information about the display capabilities of the display attached to the
//	graphics device.  The driver must return information in the VDDisplayConnectInfoRec.
//
//	For this routine, the relevant fields of the 'VDDisplayConnectInfoRec' structure are as follows:
//			<-	csDisplayCode	Type of connected display.
//			<-	csConnectFlags	Whether the display modes for display are required or optional.
//								default is 0.  Forces the DisplayMgr to call GetModeTiming for each
//								timing mode.
//
//=====================================================================================================
GDXErr GraphicsCoreGetConnection(VDDisplayConnectInfoRec *displayConnectInfo)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData() ;
	GDXErr err = kGDXErrUnknownError;								// Assume failure

	RawSenseCode rawSenseCode;
	ExtendedSenseCode extendedSenseCode;
	Boolean standardInterpretation;	
	
	// Default to all modes invalid.
	// Specifically, this means the 'kAllModesValid' and 'kAllModesSafe' bits of the 'csConnectFlags'
	// field are set to 0.
	// This forces the DisplayMgr to call GetModeTiming status calls for each timing mode, instead of
	// assuming they all have the same state.

	displayConnectInfo->csConnectFlags = 0;	

	switch (coreData->displayCode)
	{
		case (kDisplayCode21InchMono):
			displayConnectInfo->csDisplayType = kMonoTwoPageConnect;
			displayConnectInfo->csConnectFlags |= ( 1 << kIsMonoDev);
			break;
		case (kDisplayCodePortraitMono):
			displayConnectInfo->csDisplayType = kFullPageConnect;
			displayConnectInfo->csConnectFlags |= ( 1 << kIsMonoDev);
			break;
		case (kDisplayCodePortrait):
			displayConnectInfo->csDisplayType = kFullPageConnect;
			break;
		case (kDisplayCodeStandard):
			displayConnectInfo->csDisplayType = kHRConnect;
			break;
		case (kDisplayCodeVGA):
			displayConnectInfo->csDisplayType = kVGAConnect;
			break;
		case (kDisplayCodeNTSC):
			displayConnectInfo->csDisplayType = kNTSCConnect;
			break;
		case (kDisplayCodePAL):
			displayConnectInfo->csDisplayType = kPALConnect;
			break;
		case (kDisplayCodeMultiScanBand1):
			displayConnectInfo->csDisplayType = kMultiModeCRT1Connect;
			break;
		case (kDisplayCodeMultiScanBand2):
			displayConnectInfo->csDisplayType = kMultiModeCRT2Connect;
			break;
		case (kDisplayCodeMultiScanBand3):
			displayConnectInfo->csDisplayType = kMultiModeCRT3Connect;
			break;
		case (kDisplayCode16Inch):
			displayConnectInfo->csDisplayType = kColor16Connect;
			break;
		case (kDisplayCode19Inch):
			displayConnectInfo->csDisplayType = kColor19Connect;
			break;
		case (kDisplayCode21Inch):
			displayConnectInfo->csDisplayType = kColorTwoPageConnect;
			break;
		case (kDisplayCodePanel):
			displayConnectInfo->csDisplayType = kPanelConnect;
			break;
		case (kDisplayCodePanelFSTN):
			displayConnectInfo->csDisplayType = kPanelFSTNConnect;
			break;
		case (kDisplayCodeDDCC):
			displayConnectInfo->csDisplayType = kDDCConnect;
			break;
		default:
			displayConnectInfo->csDisplayType = kUnknownConnect;
			break;
	}
	

	// Report any tagging that is occurring.  Tagging is used by the DisplayMgr to help determine
	// which display is associated with a particular driver.  If possible, the DisplayMgr instructs
	// the display to 'wiggle' its sense lines, and then makes multiple GetConnection() calls to 
	// the various graphics drivers.  The DisplayMgr considers a 'tag' to have occurred if the
	// 'csConnectTaggedType' or 'csConnectTaggedData' fields change state.
	// A 'tag' is considered 'standard' if the 'csConnectTaggedType' and 'csConnectTaggedData' fields
	// correspond to the RawSenseCode and ExtendedSenseCode, respectively, of 'standard' sense line
	// hardware implementations.
	
	displayConnectInfo->csConnectFlags |= ( 1 << kReportsTagging);		// Driver can report tagging
	
	// Get the state of the sense lines, so the Display Manager can determine if they are 'wiggling'
	err = GraphicsHALGetSenseCodes(&rawSenseCode, &extendedSenseCode, &standardInterpretation);
	if (err)
		goto ErrorExit;
	
	displayConnectInfo->csConnectTaggedType = rawSenseCode;
	displayConnectInfo->csConnectTaggedData = extendedSenseCode;

	if (!standardInterpretation)
		displayConnectInfo->csConnectFlags |= ( 1 << kTaggingInfoNonStandard);

	if (coreData->reportsDDCConnection)
		displayConnectInfo->csConnectFlags |= ( 1 << kReportsDDCConnection);
	
	if (coreData->hasDDCConnection)
		displayConnectInfo->csConnectFlags |= ( 1 << kHasDDCConnection);

	if (coreData->builtInConnection)
		displayConnectInfo->csConnectFlags |= ( 1 << kBuiltInConnection);

	if (coreData->modifyConnection)
		err = (coreData->modifyConnection)( displayConnectInfo );

ErrorExit:
	return (err);
}



//=====================================================================================================
//
// GraphicsCoreGetModeTiming()
//	This is used to to gather scan timing information.  The Core fills out the csTimingData, the
//	mapping of DisplayModeIDs -> csTimingData is invariant for all HALs.  The HAL fills in the
//	csTimingFlags.
//
//	For this routine, the relevant fields of the 'VDTimingInfoRec' structure are as follows:
//			->	csTimingMode	DisplayModeID describing the display resolution and scan timing
//
//			<-	csTimingFormat	Format of info in csTimingData field (only 'kDeclROMTables' is valid)
//			<-	csTimingData	Scan timings for the DisplayModeID specified in csTimingMode
//
//			<-	csTimingFlags	Whether the DisplayModeID with these scan timings is required or 
//								optional.  The HAL fills this in.
//
//=====================================================================================================
GDXErr GraphicsCoreGetModeTiming(VDTimingInfoRec *timingInfo)
{
	GDXErr err = kGDXErrInvalidParameters;						// Assume failure	

	err = GraphicsHALGetModeTiming(timingInfo->csTimingMode, &timingInfo->csTimingData,
													&timingInfo->csTimingFormat, &timingInfo->csTimingFlags);

ErrorExit:

	return (err);
}



//=====================================================================================================
//
// GraphicsCoreGetPreferredConfiguration()
//	This call is the counterpart to the SetPreferredConfiguration control all.
//
//	For this routine, the relevant fields of the 'VDSwitchInfoRec' structure are as follows:
//		<-			csMode		DepthMode of preferred resolution
//		<-			csData		DisplayModeID of preferred resolution
//
//=====================================================================================================
GDXErr GraphicsCoreGetPreferredConfiguration(VDSwitchInfoRec *switchInfo)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData() ;
	GDXErr err = kGDXErrUnknownError;								// Assume failure

	GraphicsPreferred graphicsPreferred;


	// Find the preferred configuration property that has been saved and get
	// 		1) the preferred displayModeID
	// 		2) the preferred depthMode

	err = GraphicsOSSGetCorePref(&coreData->regEntryID, &graphicsPreferred);
	
	if (err)
		goto ErrorExit;

	switchInfo->csMode = graphicsPreferred.depthMode;
	switchInfo->csData = graphicsPreferred.displayModeID;
	
	err = kGDXErrNoError ;											// Everything Okay
		
ErrorExit:

	return (err);
}



//=====================================================================================================
//
// GraphicsCoreGetNextResolution()
//	This call is used to determine the set of resolutions that the current display and the hardware
//	supports.
//	The Core fills out the csHorizontalPixels, csVerticalLines, and csRefreshRate.  These don't vary
//	with hardware implementations.  The HAL just returns the next supported DisplayModeID and the 
//	maximum bit depth that is supported.
//
//	For this routine, the relevant fields of the 'VDResolutionInfoRec' structure are as follows:
//			->	csPreviousDiplayModeID
//			If csPreviousDiplayModeID = kDisplayModeIDFindFirstResolution, get the first supported
//			resolution.
//			If csPreviousDiplayModeID = kDisplayModeIDCurrent, fill in the VDResolutionInfoRec
//			structure with the current resolution's parameters.
//			Otherwise, csPreviousDiplayModeID contains the previous DisplayModeID (hence its name)
//			from the previous call.
//
//			<-	csDisplayModeID		ID of the next display mode following csPreviousDiplayModeID
//			set to kDisplayModeIDNoMoreResolutions once all supported display modes have been reported
//
//			<-	csHorizontalPixels	# of pixels in a horizontal line
//			<-	csVerticalLines		# of lines in a screen
//			<-	csRefreshRate		Vertical Refresh Rate of the Screen
//			<-	csMaxDepthMode		Maximum bit depth for the DisplayModeID (relative depth mode)
//
//=====================================================================================================
GDXErr GraphicsCoreGetNextResolution(VDResolutionInfoRec *resolutionInfo)
{

	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();
	GDXErr err = kGDXErrUnknownError;				// Assume failure	
	DisplayModeID displayModeID;					// Flled in with next supported resolution
	DepthMode maxDepthMode;							// Max depthmode hw supports for a resolution

	UInt32 horizontalPixels = 0;																			
	UInt32 verticalLines = 0;																			
	Fixed refreshRate = 0;																			

	if (kDisplayModeIDCurrent == resolutionInfo->csPreviousDisplayModeID)
	{
		// Return the information about the current state.
		displayModeID = coreData->displayModeID;	
		err = GraphicsHALGetMaxDepthMode(displayModeID, &maxDepthMode);		
	
		if (err)												// Should NEVER be error
			goto ErrorExit;
	}
	else
	{
		err = GraphicsHALGetNextResolution(resolutionInfo->csPreviousDisplayModeID,
				&displayModeID, &maxDepthMode);
				
		if (err)					// Most probably not a Valid Resolution in csPreviousDisplayModeID
			goto ErrorExit;
	}
	
	// displayModeID now contains the resolution of interest in and maxDepthMode = max depthmode
	// that the hw supports for that resolution.
	
	resolutionInfo->csDisplayModeID = displayModeID;
	resolutionInfo->csMaxDepthMode = maxDepthMode;
	
	if (kDisplayModeIDNoMoreResolutions != displayModeID)
	{
		UInt32 bitsPerPixel;
		SInt16 rowBytes;
		
		err = GraphicsHALGetVideoParams(displayModeID, kDepthMode1, &bitsPerPixel, &rowBytes, &horizontalPixels, &verticalLines, &refreshRate);
		if (err)
			goto ErrorExit;

		resolutionInfo->csHorizontalPixels = horizontalPixels;
		resolutionInfo->csVerticalLines = verticalLines;
		resolutionInfo->csRefreshRate = refreshRate;									
	}

ErrorExit:

	return (err);
}



//=====================================================================================================
//
// GraphicsCoreGetVideoParams()
//	This fills out a VDVideoParameteresInfoRec structure for the appropriate DisplayModeID. 
//	The core will fill out most of the data.  The HAL is asked to fill in the csPageCount,
//	map the depthMode to bpp, and figure out the rowbytes from the horizontal pixels and depthMode
//
//	For this routine, the relevant fields of the 'VDVideoParametersInfoRec' structure are:
//		-> csDisplayModeID	ID of the desired mode
//		-> csdepthMode		The relative bit depth for which the info is desired
//
//	For this routine, the relevant fields of the 'VPBlock' structure are as follows:
//		<- vpBaseOffset		Offset to page zero of video RAM
//		<- vpRowBytes		Number of bytes between the start of successive rows of video memory
//		<- vpBounds			BoundsRect for the video display (gives dimensions)
//		<- vpVersion		PixelMap version number
//		<- vpPackType		(Always 0)
//		<- pPackSize		(Always 0)
//		<- vpHRes			Horizontal resolution of the device (pixels per inch)
//		<- vpVRes			Vertical resolution of the device (pixels per inch)
//		<- vpPixelType		Defines the pixel type
//		<- vpPixelSize		Number of bits in pixel
//		<- vpCmpCount		Number of components in pixel
//		<- vpCmpSize		Number of bits per component
//		<- vpPlaneBytes		Offset from one plane to the next
//
//		<-	csPageCount		number of pages supported for resolution and bit depth
//		<-	csDeviceType	device type: direct or CLUT.
//
//=====================================================================================================
GDXErr GraphicsCoreGetVideoParams(VDVideoParametersInfoRec *videoParamatersInfo)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData() ;
	GDXErr err;

	DepthMode depthMode = videoParamatersInfo->csDepthMode;
	DisplayModeID displayModeID = videoParamatersInfo->csDisplayModeID;	
	
	UInt32 bitsPerPixel;				// Ask HAL to map DepthMode into bits per pixel
	SInt16 pageCount;					// For calling GraphicsHALGetPages to determine # of pages
	
	SInt16 rowBytes;					// Save rowBytes for the depthMode for the given resoltution

	UInt32 horizontalPixels = 0;
	UInt32 verticalLines = 0;
	Fixed refreshRate = 0;

	// The displayModeID is supported, so fill in the rest of the depth independent data.

	(videoParamatersInfo->csVPBlockPtr)->vpBaseOffset 		= 0;			// For us, it's always 0
	(videoParamatersInfo->csVPBlockPtr)->vpBounds.top 		= 0;			// Always 0
	(videoParamatersInfo->csVPBlockPtr)->vpBounds.left 		= 0;			// Always 0
	// bottom, and right filled out above
	(videoParamatersInfo->csVPBlockPtr)->vpVersion 			= 0;			// Always 0
	(videoParamatersInfo->csVPBlockPtr)->vpPackType 		= 0;			// Always 0
	(videoParamatersInfo->csVPBlockPtr)->vpPackSize 		= 0;			// Always 0
	(videoParamatersInfo->csVPBlockPtr)->vpHRes 			= 0x00480000;	// Hard coded to 72 dpi
	(videoParamatersInfo->csVPBlockPtr)->vpVRes 			= 0x00480000;	// Hard coded to 72 dpi
	(videoParamatersInfo->csVPBlockPtr)->vpPlaneBytes 		= 0;			// Always 0

	// Fill in the depth dependent data
	 err = GraphicsHALGetPages(displayModeID, depthMode, &pageCount);
	if (err)
		goto ErrorExit;

	videoParamatersInfo->csPageCount = pageCount;
	
	err = GraphicsHALGetVideoParams(displayModeID, depthMode, &bitsPerPixel, &rowBytes, &horizontalPixels, &verticalLines, &refreshRate);

	if (err)
		goto ErrorExit;

	if (horizontalPixels && verticalLines && refreshRate)
	{
		(videoParamatersInfo->csVPBlockPtr)->vpBounds.bottom = verticalLines;
		(videoParamatersInfo->csVPBlockPtr)->vpBounds.right = horizontalPixels;
	}

	// Rowbytes set by GraphicsHALGetVideoParams so just set that now.
	(videoParamatersInfo->csVPBlockPtr)->vpRowBytes = rowBytes;

	switch ( bitsPerPixel )
	{
		case 1:
			videoParamatersInfo->csDeviceType 						= clutType;
			(videoParamatersInfo->csVPBlockPtr)->vpPixelType 		= 0;
			(videoParamatersInfo->csVPBlockPtr)->vpPixelSize 		= 1;
			(videoParamatersInfo->csVPBlockPtr)->vpCmpCount 		= 1;
			(videoParamatersInfo->csVPBlockPtr)->vpCmpSize 			= 1;
			break;
		case 2:
			videoParamatersInfo->csDeviceType 						= clutType;
			(videoParamatersInfo->csVPBlockPtr)->vpPixelType 		= 0;
			(videoParamatersInfo->csVPBlockPtr)->vpPixelSize 		= 2;
			(videoParamatersInfo->csVPBlockPtr)->vpCmpCount 		= 1;
			(videoParamatersInfo->csVPBlockPtr)->vpCmpSize 			= 2;
			break;
		case 4:
			videoParamatersInfo->csDeviceType 						= clutType;
			(videoParamatersInfo->csVPBlockPtr)->vpPixelType 		= 0;
			(videoParamatersInfo->csVPBlockPtr)->vpPixelSize 		= 4;
			(videoParamatersInfo->csVPBlockPtr)->vpCmpCount 		= 1;
			(videoParamatersInfo->csVPBlockPtr)->vpCmpSize 			= 4;
			break;
		case 8:
			videoParamatersInfo->csDeviceType 						= clutType;
			(videoParamatersInfo->csVPBlockPtr)->vpPixelType 		= 0;
			(videoParamatersInfo->csVPBlockPtr)->vpPixelSize 		= 8;
			(videoParamatersInfo->csVPBlockPtr)->vpCmpCount 		= 1;
			(videoParamatersInfo->csVPBlockPtr)->vpCmpSize 			= 8;
			break;
		case 16:
			videoParamatersInfo->csDeviceType 						= directType;
			(videoParamatersInfo->csVPBlockPtr)->vpPixelType 		= 16;
			(videoParamatersInfo->csVPBlockPtr)->vpPixelSize 		= 16;
			(videoParamatersInfo->csVPBlockPtr)->vpCmpCount 		= 3;
			(videoParamatersInfo->csVPBlockPtr)->vpCmpSize 			= 5;
			(videoParamatersInfo->csVPBlockPtr)->vpPlaneBytes 		= 0;
			break;
		case 32:
			videoParamatersInfo->csDeviceType 						= directType;
			(videoParamatersInfo->csVPBlockPtr)->vpPixelType 		= 16;
			(videoParamatersInfo->csVPBlockPtr)->vpPixelSize 		= 32;
			(videoParamatersInfo->csVPBlockPtr)->vpCmpCount 		= 3;
			(videoParamatersInfo->csVPBlockPtr)->vpCmpSize 			= 8;
			(videoParamatersInfo->csVPBlockPtr)->vpPlaneBytes 		= 0;
			break;
		default:
			err = kGDXErrInvalidParameters;							// Invalid depth
			break;
	}

ErrorExit:

	return (err);
}



//=====================================================================================================
//
// GraphicsCoreGetGammaInfoList()
//	This status routine is called to iterate over the set of gamma tables that are applicable
//	to the connected display.  Ideally, this information should not be stored in the 
//	graphics driver at all, but until the system makes use of 'Display Modules' it has been
//	deemed that the graphics driver should carry this around.
//
//		->	csPreviousGammaTableID
//		Usually, this field represents a GammaTableID and indicates that information for the
//		NEXT applicable gamma table should be returned.  However, there are two special cases:
//
//				kGammaTableIDFindFirst == csPreviousGammaTableID
//				This indicates that the iteration should start from the beginning, and the
//				information corresponding to the FIRST applicable gamma table should be returned.
//
//				kGammaTableIDSpecific == csPreviousGammaTableID
//				This is a special case that indicates the csGammaTableID field is acting as an 
//				INPUT.  Specifically, csGammaTableID has a GammaTableID of the gamma table for which
//				the csGammaTableSize and csGammaTableName fields should be filled out.
//
//		<->	csGammaTableID
//		Usually, this is acting as an output.  Under these circumstances, this has the GammaTableID
//		of the gamma table which the csGammaTableSize and csGammaTableName fields are describing.
//
//		In the event that there are no more applicable gamma tables, then kGammaTableIDNoMoreTables
//		should be returned (NOTE: the csGammaTableSize and csGammaTableName are undefined in this case)
//
//		This acts as an input when 'kGammaTableIDSpecific == csPreviousGammaTableID' as described above.
//
//		<-	csGammaTableSize			Size of the gamma table in bytes
//		<-	csGammaTableName			Gamma table name (c-string)
//
//=====================================================================================================
GDXErr GraphicsCoreGetGammaInfoList(VDGetGammaListRec *getGammaList)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();
	GDXErr err = kGDXErrUnknownError;								// Assume failure

	GammaTableID gammaTableID;
	Boolean gammaTableApplicable;
	
	long kNumberOfGammaTableIDs;
	
	enum {
		kFullGammaList = 7,
		kStandardOnlyGammaList = 2
	};
	
	GammaTableID *gammaList;

	GammaTableID fullGammaList[kFullGammaList] =
	{		
		kGammaTableIDFindFirst, 				// Not REALLY a gamma table, but indicates list start
		
		kGammaTableIDStandard,					// Mac Standard Gamma
		kGammaTableIDPageWhite,					// Page-White Gamma
		kGammaTableIDGray,						// Mac Gray Gamma
		kGammaTableIDRubik,						// Mac RGB Gamma
		kGammaTableIDNTSCPAL,					// NTSC/PAL Gamma
		kGammaTableIDCSCTFT						// Active Matrix Color LCD Gamma
	};


	GammaTableID standardOnlyGammaList[kStandardOnlyGammaList] =
	{		
		kGammaTableIDFindFirst, 				// Not REALLY a gamma table, but indicates list start
		kGammaTableIDStandard					// Mac Standard Gamma
	};


	if (coreData->onlySupportStandardGamma)
	{
		gammaList = standardOnlyGammaList;
		kNumberOfGammaTableIDs = kStandardOnlyGammaList;
	}
	else
	{
		gammaList = fullGammaList;
		kNumberOfGammaTableIDs = kFullGammaList;
	}


	if (kGammaTableIDSpecific == getGammaList->csPreviousGammaTableID)
	{
	
		// Information on a specific gamma table has been requested, so make sure that it
		// is applicable to the the display that is connected.

		gammaTableID = getGammaList->csGammaTableID;
		gammaTableApplicable = GammaTableApplicable(gammaTableID, coreData->displayCode);
		
		if (!gammaTableApplicable)
		{
			// The provided GammaTableID specified a gamma table which was not applicable to the
			// connected display, so indicated a parameter error.
			err = kGDXErrInvalidParameters;
			goto ErrorExit;
		}
	} 
	else 
	{
	
		// Information on the next applicable gamma table has been been requested.  Therefore, find out
		// what position the 'csPreviousGammaTableID' was in the the list, and locate the next
		// applicable gamma table.
		
		UInt32 i;												// Loop control variable
		UInt32 previousIDPosition = 0;							// Position of 'csPreviousGammaTableID'
		
		gammaTableID = kGammaTableIDNoMoreTables;				// Assume no more matches (end-of-list)

		// Scan the 'gammaList' to find out the position of 'csPreviousGammaTableID'.  This is
		// neccessary so that the 'next' applicable gamma table can be found.

		for (i = 0 ; i < kNumberOfGammaTableIDs ; i++)
		{
			previousIDPosition += 1;
			if (getGammaList->csPreviousGammaTableID == gammaList[i])
				break;
		}
		

		// Continue scanning the list from the 'previousIDPosition' until an applicable gamma table is
		// found.
		// NOTE:  Since this loop terminates at "i < kNumberOfGammaTableIDs", it will never
		// actually be entered if the 'csPreviousGammaTableID' was the LAST in the list.

		for (i = previousIDPosition ; i < kNumberOfGammaTableIDs; i++)
		{
			gammaTableApplicable = GammaTableApplicable(gammaList[i], coreData->displayCode);
			if (gammaTableApplicable)
			{
				gammaTableID = gammaList[i];
				break;
			}
		}
	}
	
	getGammaList->csGammaTableID = gammaTableID;					// Return the GammaTableID
	
	// Call the appropriate RetriveGammaXXX function to get the size and name of the specified gamma
	// table. Note that the gamma table itself is NOT retrieved, since a NULL is passed in as its
	// destination pointer.
	
	switch (gammaTableID)
	{
		case kGammaTableIDNoMoreTables :							// At the end of the list
			err = kGDXErrNoError;
			break;
			
		case kGammaTableIDStandard :
			err = RetrieveGammaStandard(&getGammaList->csGammaTableSize,
					getGammaList->csGammaTableName, NULL);
			break;
			
		case kGammaTableIDPageWhite :
			err = RetrieveGammaPageWhite(&getGammaList->csGammaTableSize,
					getGammaList->csGammaTableName, NULL);
			break;
			
		case kGammaTableIDGray :
			err = RetrieveGammaGray(&getGammaList->csGammaTableSize,
					getGammaList->csGammaTableName, NULL);
			break;
			
		case kGammaTableIDRubik :
			err = RetrieveGammaRubik(&getGammaList->csGammaTableSize,
					getGammaList->csGammaTableName, NULL);
			break;
			
		case kGammaTableIDNTSCPAL :
			err = RetrieveGammaNTSCPAL(&getGammaList->csGammaTableSize,
					getGammaList->csGammaTableName, NULL);
			break;
			
		case kGammaTableIDCSCTFT :
			err = RetrieveGammaCSCTFT(&getGammaList->csGammaTableSize,
					getGammaList->csGammaTableName, NULL);
			break;
	}


ErrorExit:
	return(err);
}



//=====================================================================================================
//
// GraphicsCoreRetrieveGammaTable()
//	This routines copies the specified gamma table to the indicated location.  It is the responsibilty
//	of the caller to make sure that space has been allocated to hold the gamma table.
//		->	csGammaGammaTableID		The specifier of the desired gamma table.
//		<-	csGammaTablePtr			Desired gamma table is copied to this location.
//
//=====================================================================================================
GDXErr GraphicsCoreRetrieveGammaTable(VDRetrieveGammaRec *retrieveGamma)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData() ;
	GDXErr err = kGDXErrUnknownError;								// Assume failure
	

	if (NULL == retrieveGamma->csGammaTablePtr)
	{
		err = kGDXErrInvalidParameters;								// Can't copy table into NULL !
		goto ErrorExit;
	}
	
	// Call the appropriate RetriveGammaXXX function to get the specified gamma table.
	// Note that ONLY the gamma table itself is retrieved, since a NULL is passed in as the
	// destination pointers for the gamma table size and name.
	
	switch (retrieveGamma->csGammaTableID)
	{
		case kGammaTableIDStandard :
			err = RetrieveGammaStandard(NULL, NULL, retrieveGamma->csGammaTablePtr);
			break;
			
		case kGammaTableIDPageWhite :
			err = RetrieveGammaPageWhite(NULL, NULL, retrieveGamma->csGammaTablePtr);
			break;
			
		case kGammaTableIDGray :
			err = RetrieveGammaGray(NULL, NULL, retrieveGamma->csGammaTablePtr);
			break;
		case kGammaTableIDRubik :
			err = RetrieveGammaRubik(NULL, NULL, retrieveGamma->csGammaTablePtr);
			break;
			
		case kGammaTableIDNTSCPAL :
			err = RetrieveGammaNTSCPAL(NULL, NULL, retrieveGamma->csGammaTablePtr);
			break;
			
		case kGammaTableIDCSCTFT :
			err = RetrieveGammaCSCTFT(NULL, NULL, retrieveGamma->csGammaTablePtr);
			break;
			
		default :
			err = kGDXErrInvalidParameters;
	}


ErrorExit:
	return(err);
}



//=====================================================================================================
//
// GraphicsCoreSupportsHardwareCursor()
//	This call is used to determine if a hardware cursor is supported.
//
//=====================================================================================================
GDXErr GraphicsCoreSupportsHardwareCursor(VDSupportsHardwareCursorRec *supportsHardwareCursor)
{
	GDXErr err = kGDXErrUnknownError;								// Assume failure
	Boolean hardwareCursorCapable;

	supportsHardwareCursor->csReserved1 = 0;
	supportsHardwareCursor->csReserved2 = 0;
	
	err = GraphicsHALSupportsHardwareCursor(&hardwareCursorCapable);
	if (err)
		goto ErrorExit;
	
	if (hardwareCursorCapable)
		supportsHardwareCursor->csSupportsHardwareCursor = 1;
	else
		supportsHardwareCursor->csSupportsHardwareCursor = 0;
	
ErrorExit:
	return(err);
}



//=====================================================================================================
//
// GraphicsCoreGetHardwareCursorDrawState()
//	This is used to determine the state of the hardware cursor.  Just pass the call to the HAL.
//
//=====================================================================================================
GDXErr GraphicsCoreGetHardwareCursorDrawState(VDHardwareCursorDrawStateRec *cursorDrawState)
{
	GDXErr err = kGDXErrUnknownError;								// Assume failure

	cursorDrawState->csReserved1 = 0;
	cursorDrawState->csReserved2 = 0;

	err = GraphicsHALGetHardwareCursorDrawState (&(cursorDrawState->csCursorX),
			&(cursorDrawState->csCursorY), &(cursorDrawState->csCursorVisible),
			&(cursorDrawState->csCursorSet));

ErrorExit:
	return(err);
}



//=====================================================================================================
//
// GraphicsCoreGetPowerState()
//	The graphics hw might have the ability to to go into some kind of power saving mode.  Just
//	pass the call to the HAL.
//
//=====================================================================================================
GDXErr GraphicsCoreGetPowerState(VDPowerStateRec *vdPowerState)
{

	GDXErr err = kGDXErrUnknownError;								// Assume failure
	
	err = GraphicsHALGetPowerState(vdPowerState);
		
ErrorExit:

	return err;
}



//=====================================================================================================
//
// GraphicsCoreSetClutBehavior()
//
//=====================================================================================================
GDXErr GraphicsCoreGetDDCBlock(VDDDCBlockRec *vdDDCBlock)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData() ;
	GDXErr err;

	if (coreData->reportsDDCConnection == false)
	{
		err = kGDXErrUnsupportedFunctionality;
		goto ErrorExit;
	}
	if (coreData->triStateSyncOn == false)
	{
		err = kGDXErrUnsupportedFunctionality;
		goto ErrorExit;
	}

	err = GraphicsUtilGetDDCBlock(vdDDCBlock);

ErrorExit:
	return err;
}



//=====================================================================================================
//
// GraphicsCoreGetClutBehavior()
//
//=====================================================================================================
GDXErr GraphicsCoreGetClutBehavior(VDClutBehavior *vdClutBehavior)
{
	GDXErr err;
	err = GraphicsHALGetClutBehavior( vdClutBehavior );
	return err;
}



//=====================================================================================================
//
// GraphicsCoreGetTimingRanges()
//
//=====================================================================================================
GDXErr GraphicsCoreGetTimingRanges(VDDisplayTimingRangeRec *vdDisplayTimingRange)
{
	GDXErr err;
	err = GraphicsHALGetTimingRanges( vdDisplayTimingRange );
	return err;
}



//=====================================================================================================
//
// GraphicsCoreGetDetailedTiming()
//
//=====================================================================================================
GDXErr GraphicsCoreGetDetailedTiming(VDDetailedTimingRec *vdDetailedTiming)
{
	GDXErr err;
	err = GraphicsHALGetDetailedTiming( vdDetailedTiming );
	return err;
}



//=====================================================================================================
//
// GammaTableApplicable()
//	This private routine merely checks to see if a given gamma table is applicable to a given display.
//
//		->	gammaTableID	Specifier of the gamma table in question.			
//		->	displayCode		Specifier of the display in question
//
//		<-	Boolean			'true' if the specified gamma table is applicable to the specified display. 
//
//=====================================================================================================
static Boolean GammaTableApplicable(GammaTableID gammaTableID, DisplayCode displayCode)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData() ;	

	// Enumerate some arbitrary bit positions (and their masks) which correspond to different gamma
	// tables.  Setting a bit postion to '1' indicates that the gamma table is applicable for the
	// display in question.
	enum
	{
		kGammaTableStandardBit 			= 0,
		kGammaTablePageWhiteBit 		= 1,
		kGammaTableGrayBit 				= 2,
		kGammaTableRubikBit 			= 3,
		kGammaTableNTSCPALBit 			= 4,
		kGammaTableCSCTFTBit 			= 5,

		kGammaTableStandardMask 		= 0x01,
		kGammaTablePageWhiteMask 		= 0x02,
		kGammaTableGrayMask 			= 0x04,
		kGammaTableRubikMask 			= 0x08,
		kGammaTableNTSCPALMask 			= 0x10,
		kGammaTableCSCTFTMask 			= 0x20
	};
	
	UInt32 applicableTables;
	Boolean gammaTableApplicable;
	
	
	// Set the appropriate bits in 'applicableTables' based on the display.
	// NOTE:  for 'kDisplayCodeUnknown', the only table marked as applicable is the 'Standard' one.
	switch (displayCode)
	{
		case kDisplayCode12Inch :
			applicableTables = kGammaTableRubikMask;
			break;
			
		case kDisplayCodeUnknown :
		case kDisplayCodeStandard :  
		case kDisplayCodeVGA :
		case kDisplayCode20 :
		case kDisplayCode21 :
			applicableTables = kGammaTableStandardMask;	
			break;
			
		case kDisplayCodePortraitMono :	
		case kDisplayCode21InchMono :
			applicableTables = kGammaTableGrayMask;
			break;
			
		case kDisplayCodePortrait :	
		case kDisplayCode16Inch :
		case kDisplayCode19Inch :	
		case kDisplayCode21Inch :
		case kDisplayCodeMultiScanBand1 :
		case kDisplayCodeMultiScanBand2 :
		case kDisplayCodeMultiScanBand3 :
		case kDisplayCodeDDCC :
			applicableTables = (kGammaTableStandardMask | kGammaTablePageWhiteMask);
			break;
		
		case kDisplayCodeNTSC :	
		case kDisplayCodePAL :
			applicableTables = kGammaTableNTSCPALMask;
			break;

		case kDisplayCode16 :	
		case kDisplayCode17 :
			applicableTables = kGammaTableStandardMask | kGammaTablePageWhiteMask | kGammaTableGrayMask 
								| kGammaTableRubikMask | kGammaTableNTSCPALMask | kGammaTableCSCTFTMask;
			break;
		
		case kDisplayCodePanel :	
		case kDisplayCodePanelFSTN :
			applicableTables = kGammaTableCSCTFTMask;
			break;
	}


	if (coreData->onlySupportStandardGamma)
		applicableTables = kGammaTableStandardMask;


	// Check to see if the correct bit position in 'applicableTables' is set for a given GammaTableID.
	switch (gammaTableID)
	{
		case kGammaTableIDStandard :
			gammaTableApplicable = (0 != ( applicableTables & kGammaTableStandardMask ) );
			break;

		case kGammaTableIDPageWhite :
			gammaTableApplicable = (0 != ( applicableTables & kGammaTablePageWhiteMask ) );
			break;

		case kGammaTableIDGray :
			gammaTableApplicable = (0 != ( applicableTables & kGammaTableGrayMask ) );
			break;

		case kGammaTableIDRubik :
			gammaTableApplicable = (0 != ( applicableTables & kGammaTableRubikMask ) );
			break;

		case kGammaTableIDNTSCPAL :
			gammaTableApplicable = (0 != ( applicableTables & kGammaTableNTSCPALMask ) );
			break;

		case kGammaTableIDCSCTFT :
			gammaTableApplicable = (0 != ( applicableTables & kGammaTableCSCTFTMask ) );
			break;
			
		default:
			gammaTableApplicable = false;
	}

	return (gammaTableApplicable);
}



// ************  This is the temporary location of the gamma tables the Core knows about ***********
// Real-Soon-Now, this might be moved into a code fragment that is shared amoung the various graphics
// drivers.  It will be one of those funky real cool fragments that shares its DATA, as well as its
// code, so only one copy of the gamma tables will need to be in memory, regardless of how many
// drivers are loaded.  At that time, the gamma table data will become static data in the fragments
// heap, as opposed being allocated on the stack as it is here.
// This is left for the industrious byte-counter to implement.



//=====================================================================================================
//
// RetrieveGammaStandard()
//	This function retrieves the size, name, and gamma table of the 'Standard' gamma table.
//	Any combination of these can be retrieved by passing in a non-NULL pointer in the appropriate field.
//	
//		<- size		If non-NULL, then the size (in bytes) will be copied to the indicated location.
//		<- name		If non-NULL, the the name (c-string) will be copied to the indicated location.
//		<- gammaTbl	If non-NULL, then the GammaTbl will be copied to the indicated location.
//
//=====================================================================================================
static GDXErr RetrieveGammaStandard(ByteCount *size, char *name, GammaTbl *gammaTbl)
{
	ByteCount gammaTableSize;
	char gammaTableName[32] = "Mac Standard Gamma";
	
	GammaTbl gammaTable =
	{
		0,					// 0 == gVersion
		0,					// 0 == gType, so gamma table is display derived, not CLUT derived
		0,					// No formula data
		1,					// 1 == gChanCnt, so apply same correction to R, G, & B channels
		256,				// 256 entries per channel
		8,					// 8 bits of significance per entry
		NULL				// 0 == gFormulaData[0], since it will be filled in with correction data
	};
	
	enum {kNumberOfElements = 256};
	
	UInt8 correctionData[kNumberOfElements] =
	{
		0x00, 0x05, 0x09, 0x0B, 0x0E, 0x10, 0x13, 0x15, 0x17, 0x19, 0x1B, 0x1D, 0x1E, 0x20, 0x22, 0x24,
		0x25, 0x27, 0x28, 0x2A, 0x2C, 0x2D, 0x2F, 0x30, 0x31, 0x33, 0x34, 0x36, 0x37, 0x38, 0x3A, 0x3B, 
		0x3C, 0x3E, 0x3F, 0x40, 0x42, 0x43, 0x44, 0x45, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4D, 0x4E, 0x4F, 
		0x50, 0x51, 0x52, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5E, 0x5F, 0x60, 0x61, 
		0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 
		0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 
		0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8C, 0x8D, 0x8E, 0x8F, 
		0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9B, 0x9C, 0x9D, 
		0x9E, 0x9F, 0xA0, 0xA1, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 
		0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 
		0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC0, 0xC1, 0xC2, 0xC3, 0xC3, 0xC4, 
		0xC5, 0xC6, 0xC7, 0xC7, 0xC8, 0xC9, 0xCA, 0xCA, 0xCB, 0xCC, 0xCD, 0xCD, 0xCE, 0xCF, 0xD0, 0xD0, 
		0xD1, 0xD2, 0xD3, 0xD3, 0xD4, 0xD5, 0xD6, 0xD6, 0xD7, 0xD8, 0xD9, 0xD9, 0xDA, 0xDB, 0xDC, 0xDC, 
		0xDD, 0xDE, 0xDF, 0xDF, 0xE0, 0xE1, 0xE1, 0xE2, 0xE3, 0xE4, 0xE4, 0xE5, 0xE6, 0xE7, 0xE7, 0xE8, 
		0xE9, 0xE9, 0xEA, 0xEB, 0xEC, 0xEC, 0xED, 0xEE, 0xEE, 0xEF, 0xF0, 0xF1, 0xF1, 0xF2, 0xF3, 0xF3, 
		0xF4, 0xF5, 0xF5, 0xF6, 0xF7, 0xF8, 0xF8, 0xF9, 0xFA, 0xFA, 0xFB, 0xFC, 0xFC, 0xFD, 0xFE, 0xFF 
	};
		
	gammaTableSize = sizeof(GammaTbl)						// Fixed sized header
			+ gammaTable.gFormulaSize						// Add formula size
			+ (gammaTable.gChanCnt * gammaTable.gDataCnt)	// Assume 1 byte/entry
			- 2;											// Correct for gFormulaData[0] counted twice
			
	if (NULL != size)
		*size = gammaTableSize;
		
	if (NULL != name)
		CStrCopy(name, gammaTableName);
		
	if (NULL != gammaTbl)
	{
		UInt8 *destinationCorrectionData;			// Copy the correction data TO here
		UInt32 i;									// Loop control variable
		
		*gammaTbl = gammaTable;						// Copy the fixed sized header of the gamma table

		destinationCorrectionData = (UInt8 *) &(gammaTbl->gFormulaData);
		
		for (i = 0 ; i < kNumberOfElements ; i++)
			destinationCorrectionData[i] = correctionData[i];
	}
	
	return (kGDXErrNoError);
}



//=====================================================================================================
//
// RetrieveGammaPageWhite()
//	This function retrieves the size, name, and gamma table of the 'Page-White' gamma table.
//	Any combination of these can be retrieved by passing in a non-NULL pointer in the appropriate field.
//	
//		<- size		If non-NULL, then the size (in bytes) will be copied to the indicated location.
//		<- name		If non-NULL, the the name (c-string) will be copied to the indicated location.
//		<- gammaTbl	If non-NULL, then the GammaTbl will be copied to the indicated location.
//
//=====================================================================================================
static GDXErr RetrieveGammaPageWhite(ByteCount *size, char *name, GammaTbl *gammaTbl)
{
	ByteCount gammaTableSize;
	char gammaTableName[32] = "Page-White Gamma";
	
	GammaTbl gammaTable =
	{
		0,					// 0 == gVersion
		0,					// 0 == gType, so gamma table is display derived, not CLUT derived
		0,					// No formula data
		3,					// 3 == gChanCnt, so apply individual correction to R, G, & B channels
		256,				// 256 entries per channel
		8,					// 8 bits of significance per entry
		NULL				// 0 == gFormulaData[0], since it will be filled in with correction data
	};
	
	enum {kNumberOfElements = 768};		
	
	UInt8 correctionData[kNumberOfElements] =
	{
		// Red channel
		0x00, 0x03, 0x06, 0x09, 0x0C, 0x10, 0x10, 0x12, 0x13, 0x15, 0x16, 0x16, 0x18, 0x1B, 0x1C, 0x1E,	
		0x1F, 0x22, 0x23, 0x26, 0x28, 0x2B, 0x2C, 0x2F, 0x32, 0x34, 0x37, 0x3A, 0x3C, 0x3F, 0x40, 0x41,
		0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x47, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51,
		0x52, 0x53, 0x54, 0x54, 0x56, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 
		0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 
		0x71, 0x72, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 
		0x7F, 0x81, 0x82, 0x83, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8A, 0x8B, 0x8C, 0x8D, 
		0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x98, 0x99, 0x9A, 0x9B, 
		0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA1, 0xA2, 0xA3, 0xA4, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA8, 
		0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB2, 0xB3, 0xB4, 0xB5, 0xB5, 
		0xB6, 0xB7, 0xB8, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC0, 0xC1, 0xC2, 
		0xC3, 0xC3, 0xC4, 0xC5, 0xC6, 0xC6, 0xC7, 0xC8, 0xC9, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCD, 0xCE, 
		0xCF, 0xD0, 0xD1, 0xD1, 0xD2, 0xD3, 0xD4, 0xD4, 0xD5, 0xD6, 0xD7, 0xD7, 0xD8, 0xD9, 0xDA, 0xDA, 
		0xDB, 0xDC, 0xDD, 0xDE, 0xDE, 0xDF, 0xE0, 0xE1, 0xE1, 0xE2, 0xE3, 0xE4, 0xE4, 0xE5, 0xE6, 0xE7, 
		0xE7, 0xE8, 0xE9, 0xEA, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEE, 0xEF, 0xF0, 0xF1, 0xF1, 0xF2, 0xF3, 
		0xF4, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF8, 0xF9, 0xFA, 0xFB, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF, 0xFF, 
		
		// Green channel
		0x00, 0x03, 0x06, 0x09, 0x0C, 0x10, 0x10, 0x18, 0x20, 0x20, 0x22, 0x23, 0x24, 0x25, 0x27, 0x28,
		0x29, 0x2C, 0x2D, 0x2E, 0x30, 0x32, 0x34, 0x37, 0x38, 0x3A, 0x3D, 0x3F, 0x40, 0x41, 0x42, 0x42, 
		0x43, 0x44, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 
		0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 
		0x62, 0x63, 0x64, 0x65, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 
		0x71, 0x71, 0x72, 0x73, 0x74, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x79, 0x7A, 0x7B, 0x7C, 0x7D,
		0x7E, 0x7F, 0x80, 0x81, 0x82, 0x83, 0x84, 0x84, 0x85, 0x86, 0x87, 0x88, 0x88, 0x89, 0x8A, 0x8B, 
		0x8C, 0x8D, 0x8E, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x93, 0x94, 0x95, 0x96, 0x96, 0x97, 0x98, 
		0x99, 0x9A, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA2, 0xA3, 0xA4, 0xA5, 
		0xA5, 0xA6, 0xA7, 0xA8, 0xA8, 0xA9, 0xAA, 0xAB, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xAF, 0xB0, 0xB1, 
		0xB2, 0xB2, 0xB3, 0xB4, 0xB5, 0xB5, 0xB6, 0xB7, 0xB7, 0xB8, 0xB9, 0xBA, 0xBA, 0xBB, 0xBC, 0xBD, 
		0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC1, 0xC2, 0xC3, 0xC3, 0xC4, 0xC5, 0xC6, 0xC6, 0xC7, 0xC8, 0xC9, 
		0xC9, 0xCA, 0xCB, 0xCC, 0xCC, 0xCD, 0xCE, 0xCF, 0xCF, 0xD0, 0xD1, 0xD2, 0xD2, 0xD3, 0xD4, 0xD4, 
		0xD5, 0xD6, 0xD6, 0xD7, 0xD8, 0xD9, 0xD9, 0xDA, 0xDB, 0xDC, 0xDC, 0xDD, 0xDE, 0xDE, 0xDF, 0xE0, 
		0xE1, 0xE1, 0xE2, 0xE3, 0xE4, 0xE4, 0xE5, 0xE6, 0xE6, 0xE7, 0xE8, 0xE9, 0xE9, 0xEA, 0xEB, 0xEC, 
		0xEC, 0xED, 0xEE, 0xEF, 0xEF, 0xF0, 0xF1, 0xF2, 0xF2, 0xF3, 0xF4, 0xF4, 0xF5, 0xF6, 0xF7, 0xF7, 
		
		// Blue channel
		0x00, 0x02, 0x05, 0x08, 0x0A, 0x0D, 0x10, 0x10, 0x10, 0x20, 0x20, 0x22, 0x23, 0x23, 0x24, 0x25,
		0x25, 0x27, 0x28, 0x29, 0x2A, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x32, 0x33, 0x34, 0x36, 0x37, 0x38, 
		0x3A, 0x3C, 0x3D, 0x3F, 0x40, 0x41, 0x41, 0x42, 0x42, 0x43, 0x44, 0x44, 0x45, 0x45, 0x46, 0x47, 
		0x47, 0x48, 0x49, 0x4A, 0x4A, 0x4B, 0x4C, 0x4D, 0x4D, 0x4E, 0x4F, 0x4F, 0x51, 0x51, 0x52, 0x53, 
		0x54, 0x55, 0x56, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x60, 0x61, 
		0x62, 0x62, 0x63, 0x64, 0x64, 0x65, 0x66, 0x66, 0x67, 0x68, 0x69, 0x69, 0x6A, 0x6B, 0x6C, 0x6C, 
		0x6D, 0x6E, 0x6F, 0x6F, 0x70, 0x71, 0x72, 0x72, 0x73, 0x74, 0x74, 0x75, 0x76, 0x77, 0x77, 0x78, 
		0x79, 0x79, 0x7A, 0x7B, 0x7C, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82, 0x82, 0x83, 0x84, 0x84, 
		0x85, 0x86, 0x86, 0x87, 0x88, 0x88, 0x89, 0x8A, 0x8A, 0x8B, 0x8C, 0x8D, 0x8D, 0x8E, 0x8F, 0x90, 
		0x90, 0x91, 0x91, 0x92, 0x93, 0x93, 0x94, 0x95, 0x95, 0x96, 0x97, 0x97, 0x98, 0x99, 0x99, 0x9A, 
		0x9B, 0x9B, 0x9C, 0x9D, 0x9D, 0x9E, 0x9F, 0xA0, 0xA0, 0xA1, 0xA1, 0xA2, 0xA3, 0xA3, 0xA4, 0xA4, 
		0xA5, 0xA6, 0xA6, 0xA7, 0xA7, 0xA8, 0xA9, 0xA9, 0xAA, 0xAB, 0xAB, 0xAC, 0xAD, 0xAD, 0xAE, 0xAF, 
		0xAF, 0xB0, 0xB0, 0xB1, 0xB2, 0xB2, 0xB3, 0xB3, 0xB4, 0xB5, 0xB5, 0xB6, 0xB6, 0xB7, 0xB8, 0xB8, 
		0xB9, 0xBA, 0xBA, 0xBB, 0xBB, 0xBC, 0xBD, 0xBD, 0xBE, 0xBF, 0xBF, 0xC0, 0xC0, 0xC1, 0xC2, 0xC2, 
		0xC3, 0xC3, 0xC4, 0xC5, 0xC5, 0xC6, 0xC6, 0xC7, 0xC8, 0xC8, 0xC9, 0xC9, 0xCA, 0xCB, 0xCB, 0xCC, 
		0xCC, 0xCD, 0xCE, 0xCE, 0xCF, 0xD0, 0xD0, 0xD1, 0xD1, 0xD2, 0xD3, 0xD3, 0xD4, 0xD4, 0xD5, 0xD6 
	};
		
	gammaTableSize = sizeof(GammaTbl)						// Fixed sized header
			+ gammaTable.gFormulaSize						// Add formula size
			+ (gammaTable.gChanCnt * gammaTable.gDataCnt)	// Assume 1 byte/entry
			- 2;											// Correct for gFormulaData[0] counted twice
			
	if (NULL != size)
		*size = gammaTableSize;
		
	if (NULL != name)
		CStrCopy(name, gammaTableName);
		
	if (NULL != gammaTbl)
	{
		UInt8 *destinationCorrectionData;			// Copy the correction data TO here
		UInt32 i;									// Loop control variable
		
		*gammaTbl = gammaTable;						// Copy the fixed sized header of the gamma table

		destinationCorrectionData = (UInt8 *) &(gammaTbl->gFormulaData);
		
		for (i = 0 ; i < kNumberOfElements ; i++)
			destinationCorrectionData[i] = correctionData[i];
	}
	
	return (kGDXErrNoError);
}



//=====================================================================================================
//
// RetrieveGammaGray()
//	This function retrieves the size, name, and gamma table of the 'Gray' gamma table.
//	Any combination of these can be retrieved by passing in a non-NULL pointer in the appropriate field.
//	
//		<- size		If non-NULL, then the size (in bytes) will be copied to the indicated location.
//		<- name		If non-NULL, the the name (c-string) will be copied to the indicated location.
//		<- gammaTbl	If non-NULL, then the GammaTbl will be copied to the indicated location.
//
//=====================================================================================================
static GDXErr RetrieveGammaGray(ByteCount *size, char *name, GammaTbl *gammaTbl)
{
	ByteCount gammaTableSize;
	char gammaTableName[32] = "Mac Gray Gamma";
	
	GammaTbl gammaTable =
	{
		0,					// 0 == gVersion
		0,					// 0 == gType, so gamma table is display derived, not CLUT derived
		0,					// No formula data
		1,					// 1 == gChanCnt, so apply same correction to R, G, & B channels
		256,				// 256 entries per channel
		8,					// 8 bits of significance per entry
		NULL				// 0 == gFormulaData[0], since it will be filled in with correction data
	};
	
	enum {kNumberOfElements = 256};
	
	UInt8 correctionData[kNumberOfElements] =
	{
		0x05, 0x07, 0x08, 0x09, 0x0B, 0x0C, 0x0D, 0x0F, 0x10, 0x11, 0x12, 0x14, 0x15, 0x16, 0x18, 0x19,
		0x1A, 0x1C, 0x1D, 0x1E, 0x20, 0x21, 0x22, 0x23, 0x24, 0x26, 0x28, 0x29, 0x2A, 0x2C, 0x2D, 0x2F,
		0x30, 0x31, 0x33, 0x34, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3C, 0x3D, 0x3E, 0x40, 0x41, 0x42, 0x43,
		0x44, 0x45, 0x46, 0x48, 0x49, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
		0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x63, 0x63, 0x65, 0x65, 0x67,
		0x67, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
		0x78, 0x79, 0x7A, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x81, 0x82, 0x83, 0x83, 0x84, 0x85, 0x86,
		0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8E, 0x90, 0x90, 0x91, 0x92, 0x93, 0x93, 0x94,
		0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA0, 0xA1, 0xA2, 0xA3,
		0xA4, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAA, 0xAC, 0xAD, 0xAD, 0xAE, 0xAE, 0xB0, 0xB1,
		0xB2, 0xB3, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
		0xBF, 0xC0, 0xC1, 0xC2, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD,
		0xCD, 0xCE, 0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD3, 0xD4, 0xD5, 0xD6, 0xD6, 0xD7, 0xD8, 0xD8,
		0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDE, 0xDF, 0xE0, 0xE1, 0xE1, 0xE2, 0xE3, 0xE4, 0xE4, 0xE5,
		0xE6, 0xE7, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEE, 0xEF, 0xEF, 0xF0, 0xF1, 0xF2,
		0xF3, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF8, 0xF9, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
	};
		
	gammaTableSize = sizeof(GammaTbl)						// Fixed sized header
			+ gammaTable.gFormulaSize						// Add formula size
			+ (gammaTable.gChanCnt * gammaTable.gDataCnt)	// Assume 1 byte/entry
			- 2;											// Correct for gFormulaData[0] counted twice
			
	if (NULL != size)
		*size = gammaTableSize;
		
	if (NULL != name)
		CStrCopy(name, gammaTableName);
		
	if (NULL != gammaTbl)
	{
		UInt8 *destinationCorrectionData;			// Copy the correction data TO here
		UInt32 i;									// Loop control variable
		
		*gammaTbl = gammaTable;						// Copy the fixed sized header of the gamma table

		destinationCorrectionData = (UInt8 *) &(gammaTbl->gFormulaData);
		
		for (i = 0 ; i < kNumberOfElements ; i++)
			destinationCorrectionData[i] = correctionData[i];
	}
	
	return (kGDXErrNoError);
}



//=====================================================================================================
//
// RetrieveGammaRubik()
//	This function retrieves the size, name, and gamma table of the 'Rubik' gamma table.
//	Any combination of these can be retrieved by passing in a non-NULL pointer in the appropriate field.
//	
//		<- size		If non-NULL, then the size (in bytes) will be copied to the indicated location.
//		<- name		If non-NULL, the the name (c-string) will be copied to the indicated location.
//		<- gammaTbl	If non-NULL, then the GammaTbl will be copied to the indicated location.
//
//=====================================================================================================
static GDXErr RetrieveGammaRubik(ByteCount *size, char *name, GammaTbl *gammaTbl)
{
	ByteCount gammaTableSize;
	char gammaTableName[32] = "Mac RGB Gamma";
	
	GammaTbl gammaTable =
	{
		0,					// 0 == gVersion
		0,					// 0 == gType, so gamma table is display derived, not CLUT derived
		0,					// No formula data
		1,					// 1 == gChanCnt, so apply same correction to R, G, & B channels
		256,				// 256 entries per channel
		8,					// 8 bits of significance per entry
		NULL				// 0 == gFormulaData[0], since it will be filled in with correction data
	};
	
	enum {kNumberOfElements = 256};
	
	UInt8 correctionData[kNumberOfElements] =
	{
		0x05, 0x07, 0x08, 0x09, 0x0B, 0x0C, 0x0D, 0x0F, 0x10, 0x11, 0x12, 0x14, 0x15, 0x16, 0x18, 0x19,
		0x1A, 0x1C, 0x1D, 0x1E, 0x20, 0x21, 0x22, 0x23, 0x24, 0x26, 0x28, 0x29, 0x2A, 0x2C, 0x2D, 0x2F,
		0x30, 0x31, 0x33, 0x34, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3C, 0x3D, 0x3E, 0x40, 0x41, 0x42, 0x43,
		0x44, 0x45, 0x46, 0x48, 0x49, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
		0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x63, 0x63, 0x65, 0x65, 0x67,
		0x67, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
		0x78, 0x79, 0x7A, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x81, 0x82, 0x83, 0x83, 0x84, 0x85, 0x86,
		0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8E, 0x90, 0x90, 0x91, 0x92, 0x93, 0x93, 0x94,
		0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA0, 0xA1, 0xA2, 0xA3,
		0xA4, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAA, 0xAC, 0xAD, 0xAD, 0xAE, 0xAE, 0xB0, 0xB1,
		0xB2, 0xB3, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
		0xBF, 0xC0, 0xC1, 0xC2, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD,
		0xCD, 0xCE, 0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD3, 0xD4, 0xD5, 0xD6, 0xD6, 0xD7, 0xD8, 0xD8,
		0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDE, 0xDF, 0xE0, 0xE1, 0xE1, 0xE2, 0xE3, 0xE4, 0xE4, 0xE5,
		0xE6, 0xE7, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEE, 0xEF, 0xEF, 0xF0, 0xF1, 0xF2,
		0xF3, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF8, 0xF9, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
	};
		
	gammaTableSize = sizeof(GammaTbl)						// Fixed sized header
			+ gammaTable.gFormulaSize						// Add formula size
			+ (gammaTable.gChanCnt * gammaTable.gDataCnt)	// Assume 1 byte/entry
			- 2;											// Correct for gFormulaData[0] counted twice
			
	if (NULL != size)
		*size = gammaTableSize;
		
	if (NULL != name)
		CStrCopy(name, gammaTableName);
		
	if (NULL != gammaTbl)
	{
		UInt8 *destinationCorrectionData;			// Copy the correction data TO here
		UInt32 i;									// Loop control variable
		
		*gammaTbl = gammaTable;						// Copy the fixed sized header of the gamma table

		destinationCorrectionData = (UInt8 *) &(gammaTbl->gFormulaData);
		
		for (i = 0 ; i < kNumberOfElements ; i++)
			destinationCorrectionData[i] = correctionData[i];
	}
	
	return (kGDXErrNoError);
}



//=====================================================================================================
//
// RetrieveGammaNTSCPAL()
//	This function retrieves the size, name, and gamma table of the 'NTSC/PAL' gamma table.
//	Any combination of these can be retrieved by passing in a non-NULL pointer in the appropriate field.
//	
//		<- size		If non-NULL, then the size (in bytes) will be copied to the indicated location.
//		<- name		If non-NULL, the the name (c-string) will be copied to the indicated location.
//		<- gammaTbl	If non-NULL, then the GammaTbl will be copied to the indicated location.
//
//	This gamma table serves two functions:  it scales the full range (0-255) graphics values to CCIR601
//	range (16-235) and adds gamma correction factor of 1.4.  The values
//	were derived knowing that:
//	
//		255^(1/1.4) * (scale factor) + 16 = 235
//		
//	Therefore, the scale factor = 4.183.  Hence, the values for the CLUT are as follows:
//		
//		value = N^(1/1.4) * 4.183 + 16
//	
//	where N is the 8 bit linear input from VRAM for the Red, Green, or Blue component.
//		
//=====================================================================================================
static GDXErr RetrieveGammaNTSCPAL(ByteCount *size, char *name, GammaTbl *gammaTbl)
{
	ByteCount gammaTableSize;
	char gammaTableName[32] = "NTSC/PAL Gamma";
	
	GammaTbl gammaTable =
	{
		0,					// 0 == gVersion
		0,					// 0 == gType, so gamma table is display derived, not CLUT derived
		0,					// No formula data
		1,					// 1 == gChanCnt, so apply same correction to R, G, & B channels
		256,				// 256 entries per channel
		8,					// 8 bits of significance per entry
		NULL				// 0 == gFormulaData[0], since it will be filled in with correction data
	};
	
	enum {kNumberOfElements = 256};		
	
	UInt8 correctionData[kNumberOfElements] =
	{
		0x10, 0x14, 0x17, 0x19, 0x1B, 0x1D, 0x1F, 0x21, 0x22, 0x24, 0x26, 0x27, 0x29, 0x2A, 0x2C, 0x2D,
		0x2E, 0x30, 0x31, 0x32, 0x34, 0x35, 0x36, 0x37, 0x38, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x41, 
		0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 
		0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 
		0x62, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 
		0x70, 0x71, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7B, 0x7C, 
		0x7D, 0x7E, 0x7F, 0x7F, 0x80, 0x81, 0x82, 0x83, 0x83, 0x84, 0x85, 0x86, 0x87, 0x87, 0x88, 0x89, 
		0x8A, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8E, 0x8F, 0x90, 0x91, 0x91, 0x92, 0x93, 0x94, 0x94, 0x95, 
		0x96, 0x97, 0x97, 0x98, 0x99, 0x9A, 0x9A, 0x9B, 0x9C, 0x9D, 0x9D, 0x9E, 0x9F, 0x9F, 0xA0, 0xA1, 
		0xA2, 0xA2, 0xA3, 0xA4, 0xA4, 0xA5, 0xA6, 0xA7, 0xA7, 0xA8, 0xA9, 0xA9, 0xAA, 0xAB, 0xAC, 0xAC, 
		0xAD, 0xAE, 0xAE, 0xAF, 0xB0, 0xB0, 0xB1, 0xB2, 0xB3, 0xB3, 0xB4, 0xB5, 0xB5, 0xB6, 0xB7, 0xB7, 
		0xB8, 0xB9, 0xB9, 0xBA, 0xBB, 0xBB, 0xBC, 0xBD, 0xBD, 0xBE, 0xBF, 0xBF, 0xC0, 0xC1, 0xC1, 0xC2, 
		0xC3, 0xC3, 0xC4, 0xC5, 0xC5, 0xC6, 0xC7, 0xC7, 0xC8, 0xC9, 0xC9, 0xCA, 0xCB, 0xCB, 0xCC, 0xCD, 
		0xCD, 0xCE, 0xCF, 0xCF, 0xD0, 0xD1, 0xD1, 0xD2, 0xD3, 0xD3, 0xD4, 0xD4, 0xD5, 0xD6, 0xD6, 0xD7, 
		0xD8, 0xD8, 0xD9, 0xDA, 0xDA, 0xDB, 0xDB, 0xDC, 0xDD, 0xDD, 0xDE, 0xDF, 0xDF, 0xE0, 0xE0, 0xE1, 
		0xE2, 0xE2, 0xE3, 0xE4, 0xE4, 0xE5, 0xE5, 0xE6, 0xE7, 0xE7, 0xE8, 0xE9, 0xE9, 0xEA, 0xEA, 0xEB
	};
		
	gammaTableSize = sizeof(GammaTbl)						// Fixed sized header
			+ gammaTable.gFormulaSize						// Add formula size
			+ (gammaTable.gChanCnt * gammaTable.gDataCnt)	// Assume 1 byte/entry
			- 2;											// Correct for gFormulaData[0] counted twice
			
	if (NULL != size)
		*size = gammaTableSize;
		
	if (NULL != name)
		CStrCopy(name, gammaTableName);
		
	if (NULL != gammaTbl)
	{
		UInt8 *destinationCorrectionData;			// Copy the correction data TO here
		UInt32 i;									// Loop control variable
		
		*gammaTbl = gammaTable;						// Copy the fixed sized header of the gamma table

		destinationCorrectionData = (UInt8 *) &(gammaTbl->gFormulaData);
		
		for (i = 0 ; i < kNumberOfElements ; i++)
			destinationCorrectionData[i] = correctionData[i];
	}
	
	return (kGDXErrNoError);
}



//=====================================================================================================
//
// RetrieveGammaCSCTFT()
//	This function retrieves the size, name, and gamma table of the 'CSCTFT' gamma table.
//	Any combination of these can be retrieved by passing in a non-NULL pointer in the appropriate field.
//	
//		<- size		If non-NULL, then the size (in bytes) will be copied to the indicated location.
//		<- name		If non-NULL, the the name (c-string) will be copied to the indicated location.
//		<- gammaTbl	If non-NULL, then the GammaTbl will be copied to the indicated location.
//
//=====================================================================================================
static GDXErr RetrieveGammaCSCTFT(ByteCount *size, char *name, GammaTbl *gammaTbl)
{
	ByteCount gammaTableSize;
	char gammaTableName[32] = "Active Color LCD Gamma";
	
	GammaTbl gammaTable =
	{
		0,					// 0 == gVersion
		0,					// 0 == gType, so gamma table is display derived, not CLUT derived
		0,					// No formula data
		1,					// 1 == gChanCnt, so apply same correction to R, G, & B channels
		256,				// 256 entries per channel
		8,					// 8 bits of significance per entry
		NULL				// 0 == gFormulaData[0], since it will be filled in with correction data
	};
	
	enum {kNumberOfElements = 256};
	
	UInt8 correctionData[kNumberOfElements] =
	{
		0x00, 0x05, 0x09, 0x0B, 0x0E, 0x10, 0x13, 0x15, 0x17, 0x19, 0x1B, 0x1D, 0x1E, 0x20, 0x22, 0x24, 
		0x25, 0x28, 0x28, 0x2A, 0x2C, 0x2D, 0x2F, 0x30, 0x31, 0x33, 0x34, 0x36, 0x37, 0x38, 0x3A, 0x3B, 
		0x3C, 0x3E, 0x3F, 0x42, 0x44, 0x48, 0x49, 0x4A, 0x4B, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x54, 
		0x55, 0x56, 0x57, 0x58, 0x59, 0x59, 0x5A, 0x5A, 0x5B, 0x5C, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 
		0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 
		0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x7A, 0x7C, 0x7E, 0x80, 0x82, 0x84, 0x86, 0x87, 0x88, 0x8A, 
		0x8C, 0x8E, 0x90, 0x92, 0x94, 0x96, 0x98, 0x99, 0x9A, 0x9B, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 
		0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xA9, 0xAA, 0xAA, 0xAB, 0xAB, 0xAB, 0xAC, 
		0xAC, 0xAD, 0xAD, 0xAE, 0xAE, 0xAF, 0xAF, 0xB0, 0xB0, 0xB0, 0xB1, 0xB1, 0xB2, 0xB2, 0xB3, 0xB3, 
		0xB4, 0xB4, 0xB4, 0xB5, 0xB5, 0xB6, 0xB6, 0xB7, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBC, 0xBD, 
		0xBE, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC8, 0xC9, 0xC9, 0xCA, 0xCA, 
		0xCA, 0xCB, 0xCB, 0xCC, 0xCD, 0xCD, 0xCD, 0xCE, 0xCE, 0xCF, 0xCF, 0xD0, 0xD0, 0xD1, 0xD2, 0xD3, 
		0xD3, 0xD4, 0xD4, 0xD5, 0xD5, 0xD6, 0xD6, 0xD7, 0xD7, 0xD8, 0xD9, 0xD9, 0xDA, 0xDB, 0xDC, 0xDC, 
		0xDD, 0xDE, 0xDF, 0xDF, 0xE0, 0xE1, 0xE1, 0xE2, 0xE3, 0xE4, 0xE4, 0xE5, 0xE6, 0xE7, 0xE7, 0xE8, 
		0xE9, 0xE9, 0xEA, 0xEA, 0xEB, 0xEB, 0xEC, 0xEC, 0xED, 0xED, 0xEE, 0xEE, 0xEE, 0xEF, 0xEF, 0xF0, 
		0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFC, 0xFD, 0xFE, 0xFF
	};
		
	gammaTableSize = sizeof(GammaTbl)						// Fixed sized header
			+ gammaTable.gFormulaSize						// Add formula size
			+ (gammaTable.gChanCnt * gammaTable.gDataCnt)	// Assume 1 byte/entry
			- 2;											// Correct for gFormulaData[0] counted twice
			
	if (NULL != size)
		*size = gammaTableSize;
		
	if (NULL != name)
		CStrCopy(name, gammaTableName);
		
	if (NULL != gammaTbl)
	{
		UInt8 *destinationCorrectionData;			// Copy the correction data TO here
		UInt32 i;									// Loop control variable
		
		*gammaTbl = gammaTable;						// Copy the fixed sized header of the gamma table

		destinationCorrectionData = (UInt8 *) &(gammaTbl->gFormulaData);
		
		for (i = 0 ; i < kNumberOfElements ; i++)
			destinationCorrectionData[i] = correctionData[i];
	}
	
	return (kGDXErrNoError);
}

