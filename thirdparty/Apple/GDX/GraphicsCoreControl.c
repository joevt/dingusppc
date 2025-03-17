/*
	File:		GraphicsCoreControl.c

	Contains:	GDX Control Calls

	Written by:	Sean Williams, Kevin Williams

	Copyright:	(c) 1994-1995 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

		 <2>	 7/17/95	SW		For GraphicsHAL SetSync(), syncBitFieldValid is now input only.
		 <1>	 4/15/95	SW		First Checked In

*/

#include "GraphicsCoreControl.h"
#include "GraphicsCoreStatus.h"
#include "GraphicsCorePriv.h"
#include "GraphicsPriv.h"
#include "GraphicsCoreUtils.h"
#include "GraphicsHAL.h"
#include "GraphicsOSS.h"

#include <DriverServices.h>				// for PoolAllocate/Deallocate



//=====================================================================================================
//
// GraphicsCoreSetMode()
//	This is an antiquated control call that is only included for backward compatibility.  This routine
//	lets you change the pixel depth and/or the current graphics page BUT ONLY for the current
//	DisplayModeID.
//
//	The control call GraphicsCoreSwitchMode() is the newer, enhanced version of this and is used by
//	software that supports 'on-the-fly' resolution switching.
//
//	For this routine, the relevant fields indicated by 'VDPageInfo' are:
//			->	csMode			desired depth mode
//			->	csPage			desired display page
//			<-	csBaseAddr		base address of frame buffer
//
//=====================================================================================================
GDXErr GraphicsCoreSetMode(VDPageInfo *pageInfo)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();
	GDXErr err = kGDXErrUnknownError;									// Assume failure

	Boolean directColor;
	char *baseAddress;


	Boolean modePossible = false;
	
	DepthMode depthMode = (DepthMode) pageInfo->csMode;
	SInt16 page = pageInfo->csPage;

	
	// Make sure that the requested depth mode and page are valid
	err = GraphicsHALModePossible(coreData->displayModeID, depthMode, page, &modePossible);
	if (!modePossible || err)
	{
		err = kGDXErrRequestedModeNotPossible;
		goto ErrorExit;
	}

	if ( depthMode == coreData->depthMode && page != coreData->currentPage )
	{
		err = GraphicsHALProgramPage( page, &baseAddress );
		if (err)
			goto ErrorExit;
	}
	else
	{
		// Set the CLUT to 50% gray.  This is done because 50% gray looks the same at all pixel depths,
		// so no funny screen artifacts will be seen during mode switching.
		
		err = GraphicsHALGrayCLUT(coreData->gammaTable);
		if (err)
			goto ErrorExit;
			
		err = GraphicsHALProgramHardware(coreData->displayModeID, depthMode, page, &directColor,
				&baseAddress);
		if (err)
			goto ErrorExit;
	}

	// Request has been successfully completed, so update the coreData to relfect the current state.
	
	coreData->depthMode = depthMode;
	coreData->currentPage = page;
	coreData->baseAddress = baseAddress;
	coreData->directColor = directColor;
	
	// Return the new base address in pageInfo->csBaseAddr
	pageInfo->csBaseAddr = (Ptr) baseAddress;
	
	// Map the relative bit depth (DepthMode) to abosolute bit depth
	err = GraphicsHALMapDepthModeToBPP(coreData->depthMode, &coreData->bitsPerPixel);
	
	if (err)
		goto ErrorExit;
	
	err = kGDXErrNoError;												// Everything okay
		
ErrorExit:

	return err;
}



//=====================================================================================================
//
// GraphicsCoreSetEntries()
//	This should change the contents of the graphics hardware's CLUT for indexed devices.  If the
//	graphics hardware is in a direct color mode, the call should never be received, but if it is, then
//	an error should be returned.
//
//=====================================================================================================
GDXErr GraphicsCoreSetEntries(const VDSetEntryRecord *setEntry)
{
	GraphicsCoreData *coreData  = GraphicsCoreGetCoreData() ;
	GDXErr err = kGDXErrUnknownError;									// Assume failure
	
	// Make sure we are on an indexed device and not a direct device.
	if (coreData->directColor)
	{
		err = kGDXErrInvalidForIndexedDevice;
		goto ErrorExit;
	}
		
	err = GraphicsUtilSetEntries(setEntry, coreData->gammaTable, coreData->depthMode,
			coreData->bitsPerPixel, coreData->luminanceMapping, coreData->directColor);
		
ErrorExit:

	return err;
}



//=====================================================================================================
//
// GraphicsCoreSetGamma()
//	For a detailed description of gamma tables and how they are used in in the GDX Model, please see
//	"Designing PCI Card and Drivers", Chapter 11.
//
//=====================================================================================================
GDXErr GraphicsCoreSetGamma(const VDGammaRecord *gamma)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();
	GDXErr err = kGDXErrUnknownError;								// Assume failure
	
	GammaTbl *clientGamma = (GammaTbl *) gamma->csGTable;
	GammaTbl *gammaTable = coreData->gammaTable;
	

	// If the client passed in NULL as the gamma table pointer, that indicates that we should build
	// a linear ramp. Otherwise, the client has supplied a gamma table, and we should make a copy of
	// it and make it our current one.
	
	if (NULL == clientGamma)
	{	
		// Client  passed in NULL, so just build a linear ramp. The size of a linear ramp is:
		//
		//	linearRampSize  = sizeof(GammaTbl)		-- fixed size header
		//		+ 0							-- 0 == gFormulaSize
		//		+ 1 * 256 * 1				-- (# of channels) * (entries/channel) * (bytes/entry)
		//		- 2							-- accounts for gFormulaData[0] is last element of GammaTbl 
		//
		
		ByteCount linearRampSize = sizeof(GammaTbl) + 0 + (1 * 256 * 1) - 2;	
		UInt8 *correctionData;
		UInt32 i;
				
		// Only allocate new gamma table if existing gamma table is smaller than required.
		if (linearRampSize > coreData->maxGammaTableSize) 
		{
			coreData->maxGammaTableSize = 0;

			if (NULL != coreData->gammaTable)					// Deallocate previous gamma table
			{
				PoolDeallocate(coreData->gammaTable);
				coreData->gammaTable = NULL;
			}
			
			coreData->gammaTable = PoolAllocateResident(linearRampSize, true);
			if (NULL == coreData->gammaTable)
			{
				err = kGDXErrUnableToAllocateGammaTable;
				goto ErrorExit;
			}
		
			coreData->maxGammaTableSize = linearRampSize;
			
		}
		
		gammaTable = coreData->gammaTable;	// Dereference for clarity
		
		gammaTable->gVersion = 0;			// A version 0 style of the GammaTbl structure
		gammaTable->gType = 0;				// Frame buffer hardware invariant
		gammaTable->gFormulaSize = 0;		// No formula data, just correction data
		gammaTable->gChanCnt = 1;			// Apply same correction to Red, Green, & Blue
		gammaTable->gDataCnt = 256;			// gDataCnt == 2^^gDataWidth
		gammaTable->gDataWidth = 8;			// 8 bits of significant data per entry
		
		// Find the starting address of the correction data.  This can be computed by starting at
		// the address of gFormula[0] and adding the gFormulaSize.
		correctionData = (UInt8 *) ( (unsigned long) &gammaTable->gFormulaData[0]
				+ gammaTable->gFormulaSize);
				
		// Build the linear ramp.  Normally, for a linear ramp, the 'correction data == index'
		// However, for NTSC & PAL monitors, special consideration must be taken.  This is because
		// CCIR601  colors only range from 16-235, not 0-255.  
	
		if (kDisplayCodeNTSC == coreData->displayCode || kDisplayCodePAL == coreData->displayCode)
		{
			// NTSC of PAL, so build a 16-235 ramp to be CCIR601 compliant.
			for (i = 0; i < gammaTable->gDataCnt ; i++)
				*correctionData++ = (i * 220 / 256) + 16;

		} 
		else 
		{
			// Just build a normal 0-255 ramp.
			for (i = 0; i < gammaTable->gDataCnt ; i++)
				*correctionData++ = i;		
		}

	}
	else
	{
	
		// User supplied a gamma table, so make sure it is a valid one.
		
		ByteCount tableSize;
		UInt32 formulaLoop;
		UInt32 channelLoop;
		UInt32 entryLoop;
		
		UInt8 *clientData;
		UInt8 *newData;


		err = kGDXErrInvalidGammaTable;			// Assume gamma table is invalid
		
		if (0 != clientGamma->gVersion)			// Only support version 0 of the GammaTbl structure
			goto ErrorExit;
		
		if (0 != clientGamma->gType)			// Only support frame buffer invariant gamma tables
			goto ErrorExit;
			
		if ((1 != clientGamma->gChanCnt) && (3 != clientGamma->gChanCnt))  // Only 1 or 3 channels
			goto ErrorExit;
			
		if (8 < clientGamma->gDataWidth)		// Only support 8 bits or less of correction data/entry
			goto ErrorExit;
			
		if (clientGamma->gDataCnt != (1 << clientGamma->gDataWidth)	)// gDataCnt must = 2^^gDataWidth
			goto ErrorExit;
		
		// The client supplied gamma table is valid, so allocate enough memory to copy it.

		tableSize = sizeof(GammaTbl)							// fixed size header
				+ clientGamma->gFormulaSize						// add formula size
				+ clientGamma->gChanCnt * clientGamma->gDataCnt	// assume 1 byte/entry
				- 2; 											// correct gFormulaData[0] counted twice
	

		if (tableSize > coreData->maxGammaTableSize) 
		{
			coreData->maxGammaTableSize = 0;
			
			if (NULL != coreData->gammaTable)					// Deallocate previous gamma table
			{
				PoolDeallocate(coreData->gammaTable);
				coreData->gammaTable = NULL;
			}
			
			coreData->gammaTable = PoolAllocateResident(tableSize, true);
			if (NULL == coreData->gammaTable)
			{
				err = kGDXErrUnableToAllocateGammaTable;
				goto ErrorExit;
			}

			coreData->maxGammaTableSize = tableSize;
			
		}

		gammaTable = coreData->gammaTable;						// Dereference for clarity


		// Copy the client supplied gamma table into our freshly allocated one. This consists of 3
		// stages:  copying the fixed sized header, the formula data, and the correction data.
		
		*gammaTable = *clientGamma;								// Copy the fixed sized header		
		
		newData = (UInt8 *) &gammaTable->gFormulaData[0];		// Point to newGamma's formula data
		clientData = (UInt8 *) &clientGamma->gFormulaData[0];	// Point to clientGamma's formula data
		
		// Copy the formula data (if any)
		for (formulaLoop = 0 ; formulaLoop < gammaTable->gFormulaSize ; formulaLoop++)
			*newData++ = *clientData++;

		// Copy the correction data.  Convientiently, after copying the formula data, the 'newData'
		// pointer and the 'clientData' pointer are pointing to the their respective starting points
		// of their correction data.
		for (channelLoop = 0 ; channelLoop < gammaTable->gChanCnt ; channelLoop++)
		{
			for (entryLoop = 0 ; entryLoop < gammaTable->gDataCnt ; entryLoop++)		
				*newData++ = *clientData++;
		}
	}


	if ( coreData->supportsHardwareCursor )
	{
		err = GraphicsHALTransformHWCursorColors( gammaTable, coreData->luminanceMapping && !coreData->directColor );
		if (err)
			goto ErrorExit;
	}


	// Check to see if we are in a direct color mode.  If we are, then we need to build a 
	// black-to-white ramp in RGB and apply it to the hardware.

	if (coreData->directColor)
	{
		err = GraphicsUtilBlackToWhiteRamp(gammaTable, coreData->depthMode, coreData->bitsPerPixel, 
				coreData->luminanceMapping, coreData->directColor);
		if (err)
			goto ErrorExit;
	}
	
	
	err = kGDXErrNoError;								// Everything okay

ErrorExit:

	return err;
}



//=====================================================================================================
//
// GraphicsCoreGrayPage()
//	This fills the specified video page with a dithered gray pattern in the absolute bit depth.
//
// For maximum speed, we will do this by filling each row of the frame buffer a 'long' (4 bytes) at a
// time, and then write out whatever bytes are remaining.
//
// 		                    (# of pixels) * (# of bits/pixel)
//    # of longs =          ---------------------------------
//                             (8 bits/byte) (4 bytes/long)
//
//
// 		                    (# of pixels) * (# of bits/pixel)
//    remaining bytes =     ---------------------------------    MOD 4
//                                   (8 bits/byte)
//
// We need four pieces of information in order to successfully gray the requested page:
//
//		base address of the page  -- this can be found by calling GraphicsCoreGetBaseAddress()
//		rowBytes 				  -- this can be found in the .vpRowBytes field of the VPBlock
//		numberOfRows			  -- this can be found in the .vpBounds.bottom of the VPBlock
//		pixelsPerRow			  -- this can be found in the .vpBounds.right of the VPBlock
//
//=====================================================================================================
GDXErr GraphicsCoreGrayPage(const VDPageInfo *pageInfo)
{

	// Define a new type which maps the absolute bit depth to the appropriate pattern to produce
	// a dithered gray.  A dithered gray is produced by having a black-white-black-white pixel pattern.
	
	typedef struct BPPToGrayPatternMap BPPToGrayPatternMap;
	struct BPPToGrayPatternMap
	{
		UInt32 bitsPerPixel;
		UInt32 grayPattern;
	};
	
	enum  {kMapSize = 6};
	
	BPPToGrayPatternMap bppMap[kMapSize] =
	{
		{1, 0xaaaaaaaa},			// Represents 32 pixels @ 1 bpp
		{2, 0xcccccccc},			//     "      16   "    @ 2 bpp
		{4, 0xf0f0f0f0},			//     "       8   "    @ 4 bpp
		{8, 0xff00ff00},			//     "       4   "    @ 8 bpp
		{16, 0xffff0000},			//     "       2   "    @ 16 bpp
		{32, 0xffffffff}			//     "       1   "    @ 32 bpp (Invert to get next pixel)
	};

	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();
	GDXErr err = kGDXErrUnknownError;				// Assume failure

	VDPageInfo baseAddressPageInfo ;				// To get address of page to gray
	VDVideoParametersInfoRec parametersInfo;		// To fill out the 'VPBlock' structure
	VPBlock vpBlock;								// To get 'vpRowBytes' & 'vpBounds.bottom & .right'
	
	Ptr rowStart;									// Start address of row
	SInt16 rowBytes;								// Byte offset between each row 
	SInt16 numberOfRows;							// # of rows on the screen (vpBlock.vpBounds.bottom)
	SInt16 pixelsPerRow;							// # of pixels/row (vpBlock.vpBounds.right)
	UInt32 longWrites;								// # of long (4 byte) writes per row
	UInt32 byteWrites;								// # of remaining bytes (if any)
	
	UInt32 row;										// loop control variable
	UInt32 i;										// loop control variable
	
	UInt32 *fillPtr;								// Location in frame buffer to fill w/ gray pattern
	UInt8 *bytePtr;									// Location in frame buffer to fill w/ gray pattern
	
	UInt32 grayPattern;								// Fill pattern
	UInt32 scratchPattern;							// "Working Version" of grayPattern

	Boolean modePossible;							// HAL sets if resolution, depthMode and page are valid
	
	SInt16 page = pageInfo->csPage;
	
	
	// Make sure that the requested page to gray is valid for current DisplayModeID and DepthMode
	err = GraphicsHALModePossible(coreData->displayModeID, coreData->depthMode, page, &modePossible);
	if (err || !modePossible)
	{
		err = kGDXErrRequestedModeNotPossible;
		goto ErrorExit;
	}

	// Call GraphicsCoreGetBaseAddress() to determine the base address of the page to gray.
	
	baseAddressPageInfo.csPage = page;								// Page we want base address of
	
	err = GraphicsCoreGetBaseAddress(&baseAddressPageInfo);			// Get the base address
	if (err)
		goto ErrorExit;
	
	// Call GraphicsCoreGetVideoParams() to obtain the VPBlock structure for current DisplayModeID at
	// the current DepthMode.
	
	parametersInfo.csDisplayModeID = coreData->displayModeID;		
	parametersInfo.csDepthMode = coreData->depthMode;
	parametersInfo.csVPBlockPtr = &vpBlock;
	
	err = GraphicsCoreGetVideoParams(&parametersInfo);				// Get the VPBlock structure
	if (err)
		goto ErrorExit;
			
	rowBytes = vpBlock.vpRowBytes;									// Byte offset between each row 	
	numberOfRows = vpBlock.vpBounds.bottom;							// Number of rows on the screen
	pixelsPerRow = vpBlock.vpBounds.right;							// Number of pixels/row
	
	longWrites = pixelsPerRow * coreData->bitsPerPixel / 8 / 4;		// # of longs per row
	byteWrites = (pixelsPerRow * coreData->bitsPerPixel / 8) % 4;	// Remaining bytes per row
	
	rowStart = baseAddressPageInfo.csBaseAddr;		// 1st row starts at the base address of this page
	
	if (coreData->useGrayPatterns)
	{
		switch (coreData->bitsPerPixel)
		{
			default:
				grayPattern = coreData->grayPattern8bpp;						
				break;
			case 16:
				grayPattern = coreData->grayPattern16bpp;
				break;
			case 32:
				grayPattern = coreData->grayPattern32bpp;
				break;
		}
	}
	else
	{
		// Scan the 'BPPToGrayPatternMap' to find the correct gray pattern for the absolute bit depth.
		for (i = 0 ; i < kMapSize ; i++)
		{
			if (bppMap[i].bitsPerPixel == coreData->bitsPerPixel)
			{
				grayPattern = bppMap[i].grayPattern;
				break;
			}
		}
	}

	// Fill the page with the dithered gray pattern.
	
	for (row = 0 ; row < numberOfRows ; row++)
	{
		scratchPattern = grayPattern;				// Get the gray pattern for the start of this row
		fillPtr = (UInt32 *) rowStart;				// Point to the beginning of row
		
		// Write out the requisite number of longs
		for (i = 0 ; i < longWrites ; i++)
		{
			*fillPtr = scratchPattern;
			fillPtr++;
			
			if (!coreData->useGrayPatterns)			
				if (32 == coreData->bitsPerPixel)		// In 32 bpp, you need to invert the pattern so a
					scratchPattern = ~scratchPattern;	// FFFFFFFF 00000000 FFFFFFFF etc is produced.
		}
		
		bytePtr = (UInt8 *) fillPtr;

		// Now see if there are any byte writes to do.
		for (i = 0 ; i < byteWrites ; i++)
		{
			*bytePtr = (UInt8) (scratchPattern >> 24) ;					// Use most significant byte
			bytePtr++;													// Use most significant byte
			scratchPattern = scratchPattern << 8;						// Get next byte in proper place
		}
		
		rowStart += rowBytes;						// Point rowStart to beginning of next row
		if (!coreData->useGrayPatterns)
			grayPattern = ~grayPattern;				// Invert pattern so next row will start w/opposite
	}

	// Check to see if we are in a direct color mode.  If we are, then we need to build a 
	// black-to-white ramp in RGB and apply it to the hardware.
	
	if ((coreData->directColor) && (NULL != coreData->gammaTable))
	{
		err = GraphicsUtilBlackToWhiteRamp(coreData->gammaTable, coreData->depthMode,
				coreData->bitsPerPixel, coreData->luminanceMapping, coreData->directColor);
		if (err)
			goto ErrorExit;
	}

	err = kGDXErrNoError;									// Everything okay
	
ErrorExit:

	return err;
}



//=====================================================================================================
//
// GraphicsCoreSetGray()
//	This routine is used with indexed devices to determine whether the control
//	routine with cscSetEntries fills a card's CLUT with actual colors or with the luminance-equivalent
//	gray tones.  For actual colors, the control routine is passed a csMode value of 0; for gray tones
//	it is passed a csMode value of 1.  Luminance equivalence should be determined by converting each
//	RGB value into the hue-saturation-brightness system and then selecting a gray value of equal
//	brightness.  Mapping colors to lumimance-equivalent gray tones lets a color monitor emulate
//	a monochrome exactly.
//
//	If the cscSetGray call is issued to a direct device, it sets the internal mapping state flag 
//	and returns a CtlGood result but does not cause the color table to be luminance mapped.
//	Short of using the control routine/ cscDirectSetEntries, there is no way to preview
//	luminnace-mapped color images on the color display of a direct device.
//	(Cards & Drivers, p. 208)
//
//	Check the value of 'grayPtr->csMode'.  If it is 0, set 'luminanceMapping' to false ; 
//	if 1, set 'luminanceMapping' to true.
//	if directColor = true, subsequent calls to CoreSetEntries will NOT do luminance mapping
//
//=====================================================================================================
GDXErr GraphicsCoreSetGray(VDGrayRecord *grayPtr)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();
	GDXErr err = kGDXErrUnknownError;							// Assume failure
	
	if (0 == grayPtr->csMode)
		coreData->luminanceMapping = false;
	else
		coreData->luminanceMapping = true;

	if ( coreData->monoOnly == true )
	{
		// If a Mono Only device, always set luminance mapping to true.
		// Additionally, change 'csMode' to alert client that the connected display is 'monoOnly'
		coreData->luminanceMapping = true;
		grayPtr->csMode = 1;				
	}
	
	err = kGDXErrNoError;										// Everything okay
	
ErrorExit:

	return err;
}



//=====================================================================================================
//
// GraphicsCoreSetInterrupt()
//	This controls the generation of the VBL interrupts.  To enable interrupts, pass a csMode value
//	of 0; to disable interrupts, pass a csMode value of 1.
//
//	Note:  This does NOT install or remove interrupt handlers.  It merely calls the OSS to set the
//	hardware as appropriate.
//
//=====================================================================================================
GDXErr GraphicsCoreSetInterrupt(const VDFlagRecord *flag)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();
	GDXErr err = kGDXErrUnknownError;							// Assume failure

	Boolean enableInterrupts; 
	
	if (0 == flag->csMode)
		enableInterrupts = true;
	else
		enableInterrupts = false;
		
	(void) GraphicsOSSSetVBLInterrupt(enableInterrupts);
	
	coreData->interruptsEnabled = enableInterrupts;				// Save vbl interrupt state
	err = kGDXErrNoError;										// Everything okay
	
ErrorExit:

	return err;
}



//=====================================================================================================
//
// GraphicsCoreDirectSetEntries()
//	Normally, color table animation is not used on a direct device, but there are some special
//	circumstances under which an application may want to change the color table hardware.  This routine
//	provides the direct device with indexed mode functionality idential to the regular 'cscSetEntries'
//	call.  The 'cscDirectSetEntries' routine has exactly the same functions and parameters as the
//	regular 'cscSetEntries' routine, but it works only on a direct device.  If this call is issued to
//	an indexed device, it should return an error indication.
//
//=====================================================================================================
GDXErr GraphicsCoreDirectSetEntries(const VDSetEntryRecord *setEntry)
{
	GraphicsCoreData *coreData  = GraphicsCoreGetCoreData() ;
	GDXErr err = kGDXErrUnknownError;									// Assume failure

	// Check to make sure that were are in a direct, not indexed, mode.
	if ( !(coreData->directColor) )
	{
		err = kGDXErrInvalidForDirectDevice;
		goto ErrorExit;
	}
	
	err = GraphicsUtilSetEntries(setEntry, coreData->gammaTable, coreData->depthMode,
			coreData->bitsPerPixel, coreData->luminanceMapping, coreData->directColor);
	
ErrorExit:

	return err;
}



//=====================================================================================================
//
// GraphicsCoreSwitchMode()
//	This routine is quite similar to GraphicsCoreSetMode(), except that it supports "on-the-fly"
//	resolution swithing.
//	This routine lets you change the DepthMode and/or the current graphics page and/or the current
//	DisplayModeID. 
//
//	For this routine, the relevant fields indicated by 'VDSwitchInfoRec' are:
//			->	csMode			desired depth mode
//			->	csData			desired DisplayModeID
//			->	csPage			desired display page
//			<-	csBaseAddr		base address of desired page
//
//=====================================================================================================
GDXErr GraphicsCoreSwitchMode(VDSwitchInfoRec *switchInfo)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();
	GDXErr err = kGDXErrUnknownError;									// Assume failure

	Boolean directColor;
	void *baseAddress;
	Boolean modePossible = false;
	
	DisplayModeID displayModeID = (DisplayModeID) switchInfo->csData;
	DepthMode depthMode = (DepthMode) switchInfo->csMode;
	SInt16 page = switchInfo->csPage;

	// Make sure that the requested 'DisplayModeID', 'DepthMode' and page are valid
	err = GraphicsHALModePossible(displayModeID, depthMode, page, &modePossible);
	if (!modePossible || err)
	{
		err = kGDXErrRequestedModeNotPossible;
		goto ErrorExit;
	}
	
	// Set the CLUT to 50% gray.  This is done because 50% gray looks the same at all pixel depths,
	// so no funny screen artifacts will be seen during mode switching.
	
	err = GraphicsHALGrayCLUT(coreData->gammaTable);
	if (err)
		goto ErrorExit;
		
	err = GraphicsHALProgramHardware(displayModeID, depthMode, page, &directColor, &(char*)baseAddress);
	if (err)
		goto ErrorExit;
			
	// Request has been successfully completed, so update the coreData to relfect the current state.
	
	coreData->displayModeID = displayModeID;
	coreData->depthMode = depthMode;
	coreData->currentPage = page;
	coreData->baseAddress = baseAddress;
	coreData->directColor = directColor;



	if ( NULL != coreData->getMonoOnly )
	{
		Boolean oldMonoOnly = coreData->monoOnly;

		coreData->monoOnly = (coreData->getMonoOnly)();

		if ( oldMonoOnly != coreData->monoOnly )
		{
			VDGrayRecord gray;

			if ( coreData->monoOnly )
				gray.csMode = 1;
			else
				gray.csMode = 0;

			GraphicsCoreSetGray( &gray );
		}
	}



	// Return the new base address in switchInfo->csBaseAddr
	switchInfo->csBaseAddr = baseAddress;
	
	// Map the relative bit depth (DepthMode) to absolute bit depth
	err = GraphicsHALMapDepthModeToBPP(coreData->depthMode, &coreData->bitsPerPixel);
	
	if (err)
		goto ErrorExit;
	
	err = kGDXErrNoError;												// Everything okay
		
ErrorExit:

	return err;
}



//=====================================================================================================
//
// GraphicsCoreSetSync()
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
//	For this routine, the relevant fields of the 'VDSyncInfoRec' structure are as follows:
//			->	csMode		bit field of the sync bits that need to be disabled/enabled
//
//				kDisableHorizontalSyncBit		set if HW should disable Horizontal Sync (No Pulses)
//				kDisableVerticalSyncBit			set if HW should disable Vertical Sync (No Pulses)
//				kDisableCompositeSyncBit		set if HW should disable Composite Sync (No Pulses)
//				kSyncOnRedEnableBit				set if HW should sync on Red
//				kSyncOnGreenEnableBit			set if HW should sync on Green
//				kSyncOnBlueEnableBit			set if HW should sync on Blue
//
//
//			->	csFlags			Mask of the bits that are valid in the csMode bit field
//
//	Note! for compatibility with the Energy Saver cdev:
//		To put a display into the Active state, is passes in csMode = 0x00, csFlags = 0x00
//			map to csMode = 0x00, csFlags = 0x07
//
//	Somebody (3rd Party Screen Savers) sets csMode = 0xFF, csFlags = 0xFF to turn off a display...
//			map to csMode = 0x07, csFlags = 0x07
//
//	For NEW callers, If v or h sync gets hit, csFlags should be 0x03.  This ensures that both v and h 
//	sync get set to the correct state.
//	
//=====================================================================================================
GDXErr GraphicsCoreSetSync(VDSyncInfoRec *sync)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();
	GDXErr err = kGDXErrUnknownError;							// Assume failure

	UInt8 syncBitField = sync->csMode;							// Bit field for syncs to hit
	UInt8 syncBitFieldValid = sync->csFlags;					// Mask for valid bits in syncBitField
	UInt8 rgbSyncField;


	// If csMode = 0x00, csFlags = 0x00, map to csMode = 0x00, csFlags = 0x07
	if ( (0x00 == syncBitField) && (0x00 == syncBitFieldValid) )
	{
		syncBitField = 0;
		syncBitFieldValid = kDPMSSyncMask;
	}

	// If csMode = 0xFF, csFlags = 0xFF, map to csMode = 0x07, csFlags = 0x07
	if ( (0xFF == syncBitField) && (0xFF == syncBitFieldValid) )
	{
		syncBitField = 0x07;
		syncBitFieldValid = kDPMSSyncMask;
	}
	
	// Error checking for "compatability" is done.
	
	// Make sure only 1 (if any) of the kSyncOn RGB bits is set
	rgbSyncField = syncBitFieldValid & kSyncOnMask;
	if ( rgbSyncField != 0 && rgbSyncField != kSyncOnRedMask && rgbSyncField != kSyncOnGreenMask && rgbSyncField != kSyncOnBlueMask )
	{
		err = kGDXErrInvalidParameters;
		goto ErrorExit;
	}
	
	if ((syncBitFieldValid & kTriStateSyncMask) && (syncBitField & kTriStateSyncMask) && !coreData->hasTriStateSync)
	{
		err = kGDXErrInvalidParameters;
		goto ErrorExit;
	}


	err = GraphicsHALSetSync(syncBitField, syncBitFieldValid);


	if (syncBitFieldValid & kTriStateSyncMask)
	{
		if (syncBitField & kTriStateSyncMask)
			coreData->triStateSyncOn = true;
		else
			coreData->triStateSyncOn = false;
	}

	if (coreData->hasDDCConnection)
	{
		VDSyncInfoRec currentSync;
		err = GraphicsHALGetSync(false, &currentSync);

		if (!coreData->triStateSyncOn && !(currentSync.csMode & kVerticalSyncMask))
			GraphicsUtilSetSync_2();
	}

	sync->csFlags = syncBitFieldValid;								// Return HAL's flags to caller

ErrorExit:

	return err;
}



//=====================================================================================================
//
// GraphicsCoreSetPreferredConfiguration()
//	This call is the counterpart to the GetPreferredConfiguration status call. This call will be used
//	by clients to set the preferred DepthMode and DisplayModeID.  This means that the card should save
//	this information in non-volatile RAM so that it persists accross reboots.
//
//	For this routine, the relevant fields of the 'VDSwitchInfoRec' structure are as follows:
//		->			csMode					Depth of preferred resolution
//		->			csData					DisplayModeID of preferred resolution
//
//=====================================================================================================
GDXErr GraphicsCoreSetPreferredConfiguration(const VDSwitchInfoRec *switchInfo)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData() ;
	GDXErr err = kGDXErrUnknownError;								// Assume failure
	GraphicsPreferred graphicsPreferred;
	
	
	if (coreData->hasDDCConnection)
		graphicsPreferred.ddcChecksum = coreData->ddcBlockData.checksum;
	else if (coreData->builtInConnection)
	{
		GraphicsPreferred oldPref;
		err = GraphicsOSSGetCorePref(&coreData->regEntryID, &oldPref);
		if (err)
			graphicsPreferred.ddcChecksum = 0;
		else
			graphicsPreferred.ddcChecksum = oldPref.ddcChecksum;
	}
	else
		graphicsPreferred.ddcChecksum = 0;


	graphicsPreferred.depthMode = switchInfo->csMode;
	graphicsPreferred.displayModeID = switchInfo->csData;
	graphicsPreferred.displayCode = coreData->displayCode;
	
	err = GraphicsOSSSetCorePref(&coreData->regEntryID, &graphicsPreferred);

ErrorExit:

	return err;
}




//=====================================================================================================
//
// GraphicsCoreSetHardwareCursor()
//	SetHardwareCursor is a required routine for drivers that support hardware cursors. QuickDraw uses
//	the SetHardwareCursor control call to set up the hardware cursor and determine whether the hardware
//	can support it. The driver must determine whether it can support the given cursor and, if so,
//	program the hardware cursor frame buffer (or equivalent), set up the CLUT, and return noErr.
//	If the driver cannot support the cursor it must return an error. The driver must remember whether
//	this call was successful for subsequent GetHardwareCursorDrawState() or DrawHardwareCursor() calls,
//	but should not change the cursor's x or y coordinates or its visible state.
//
//=====================================================================================================
GDXErr GraphicsCoreSetHardwareCursor(const VDSetHardwareCursorRec  *setHardwareCursor)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData() ;
	GDXErr err = kGDXErrUnknownError;								// Assume failure

	err = GraphicsHALSetHardwareCursor(coreData->gammaTable, 
			(coreData->luminanceMapping && (!coreData->directColor)),
			(void *) setHardwareCursor->csCursorRef);

ErrorExit:

	return err;
}



//=====================================================================================================
//
// GraphicsCoreDrawHardwareCursor()
//	This routines is called to set the hardware cursor's X and Y posisition, and its current visibility
//	state.
//
//=====================================================================================================
GDXErr GraphicsCoreDrawHardwareCursor(const VDDrawHardwareCursorRec  *drawHardwareCursor)
{
	GDXErr err = kGDXErrUnknownError;								// Assume failure

	err = GraphicsHALDrawHardwareCursor(drawHardwareCursor->csCursorX,
			drawHardwareCursor->csCursorY, drawHardwareCursor->csCursorVisible);

ErrorExit:

	return err;
}



//=====================================================================================================
//
// GraphicsCoreSetPowerState()
//	The graphics hw might have the ability to to go into some kind of power saving mode.  Just
//	pass the call to the HAL.
//
//=====================================================================================================
GDXErr GraphicsCoreSetPowerState(VDPowerStateRec *vdPowerState)
{

	GDXErr err = kGDXErrUnknownError;								// Assume failure
	
	err = GraphicsHALSetPowerState(vdPowerState);
		
ErrorExit:

	return err;
}



//=====================================================================================================
//
// GraphicsCoreSetClutBehavior()
//
//=====================================================================================================
GDXErr GraphicsCoreSetClutBehavior(VDClutBehavior *vdClutBehavior)
{
	GDXErr err;
	if ( *vdClutBehavior != kSetClutAtSetEntries && *vdClutBehavior != kSetClutAtVBL )
	{
		err = kGDXErrInvalidParameters;
		goto ErrorExit;
	}
	err = GraphicsHALSetClutBehavior( vdClutBehavior );

ErrorExit:
	return err;
}



//=====================================================================================================
//
// GraphicsCoreSetDetailedTiming()
//
//=====================================================================================================
GDXErr GraphicsCoreSetDetailedTiming(VDDetailedTimingRec *vdDetailedTiming)
{
	GDXErr err;
	err = GraphicsHALSetDetailedTiming( vdDetailedTiming );
	return err;
}
