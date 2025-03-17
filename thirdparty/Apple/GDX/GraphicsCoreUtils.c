/*
	File:		GraphicsCoreUtils.c

	Contains:	Utilities that are used by the Core and the HALs

	Written by:	Sean Williams, Kevin Williams

	Copyright:	(c) 1994-1995 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

		 <2>	 7/17/95	SW		Added GraphicsUtil GetDefaultGammaTableID()
		 <1>	 4/15/95	SW		First Checked In

*/

#include "GraphicsPriv.h"
#include "GraphicsOSS.h"
#include "GraphicsCorePriv.h"
#include "GraphicsCoreUtils.h"
#include "GraphicsHAL.h"
#include <Types.h>
#include <Traps.h>



//=====================================================================================================
//
// GraphicsUtilCheckSetEntry()
//	This utility routine checks to make sure that a 'VDSetEntryRecord' provided to setting/getting
//	CLUT entries is valid.  The 'VDSetEntryRecord' is valid if its 'csTable' is not NULL, and that the
//	range of logical addresses that it indicates should set/got is appropriate for the indicated
//	absolute bit depth.
//
//	For this routine, the relevant fields indicated by 'VDSetEntryRecord' are:
//			->	csTable			pointer to array of 'ColorSpec'
//			->	csStart			(0 based) starting index, or -1.
//								If -1, then the 'value' field of each 'ColorSpec' entry indicates the
//								CLUT address to which the 'rgb' contents should be applied.
//			->	csCount			(0 based) number of 'ColorSpec' entries to set
//
//	The remaining parameters are described as follows:
//
//			-> bitsPerPixel		Absolute pixel depth
//			<- startPosition	(0 based) The starting position in the array of ColorSpecs to set/get.
//								This is the REAL start position (Not playing any of those -1 games.)
//			<- numberOfEntries	(0 based) This is the number of 'ColorSpec' entries to set/get.
//			<- sequential		'true' indicates that the logical address of the entry is obtained by
//								its array index in the 'csTable'.  'false' indicates that the logical
//								address of the entry should be determined by the 'value' field of the
//								ColorSpec
//=====================================================================================================
GDXErr GraphicsUtilCheckSetEntry(const VDSetEntryRecord *setEntry, UInt32 bitsPerPixel,
		SInt16 *startPosition, SInt16 *numberOfEntries, Boolean *sequential)
{
		
	UInt32 maxRange;												// Maximum logical address
	UInt32 i;														// Loop control variable

	GDXErr err = kGDXErrUnknownError;								// Assume failure
		
	// Make sure the ColorSpec table is valid
	if (NULL == setEntry->csTable)
	{
		err = kGDXErrInvalidColorSpecTable;
		goto ErrorExit;
	}

	// Find out what flavor this is: Sequential or Indexed. Indexed is indicated by
	// '-1 == setEntry->csStart'.  Additionally, if using the Indexed flavor, then the true
	// 'startPosition' in the provided 'csTable' is always 0.
	
	if (-1 == setEntry->csStart)
	{
		*sequential = false;
		*startPosition = 0;
	}
	else
	{
		*sequential = true;
		*startPosition = setEntry->csStart;
	}

	*numberOfEntries = setEntry->csCount;

	// Do range checking on the 'startPosition' and 'numberOfEntries'  The valid range depends
	// on the current absolute pixel depth:
	//
	//		if bitsPerPixel < 16, then maxRange = (2^^bpp) - 1
	//		"        "     == 16, then maxRange = 31
	//		"        "     == 32, then maxRange = 255
	//
	// For a detailed explanation of these values, please refer elsewhere... :-)
	
	if (16 > bitsPerPixel)			
		maxRange = (1 << bitsPerPixel) - 1;						// 1, 2, 4, or 8 bpp
	else if (16 == bitsPerPixel)
		maxRange = 31;											// 16 bpp
	else
		maxRange = 255;											// 32 bpp
		
	// Do generic range checking that is not dependent on whether this is Indexed or Sequential flavor.
	if ((*numberOfEntries > maxRange) || (*numberOfEntries < 0))
	{
		err = kGDXErrInvalidParameters;
		goto ErrorExit;
	}
	
	// Now do some flavor dependent range checking
	if (*sequential)
	{
		// For sequential flavor, make sure that we don't go beyond 'maxRange'
		if (*startPosition + *numberOfEntries > maxRange)
		{
				err = kGDXErrInvalidParameters;
				goto ErrorExit;
		}
	}
	else
	{
		// For indexed flavor, make sure none of the indexes exceed the maximum range
		for (i = 0; i <= *numberOfEntries; i++)
		{
			if (setEntry->csTable[i].value > maxRange)
			{
				err = kGDXErrInvalidParameters;
				goto ErrorExit;
			}
		}
	}
	
	err = kGDXErrNoError;											// Everything okay.
	
ErrorExit:
	return (err);
}



//=====================================================================================================
//
// GraphicsUtilSetEntries()
//	This is the meat of the code that changes the contents of the graphics hardware's CLUT.
//
//	This call has two flavors.  In the Sequence flavor, indicated by 'csStart >= 0', 'csCount' entries
//	are changed in the CLUT, starting at 'csStart'  In this flavor, the 'value' field of the
//	'ColorSpec' is ignored.
//	In the Index flavor, indicated by 'csStart = -1', 'csCount' entries are installed into the CLUT at
//	the address specified by their respective 'value' field of the 'ColorSpec' entry.
//
//	For this routine, the relevant fields indicated by 'VDSetEntryRecord' are:
//			->	csTable			pointer to array of 'ColorSpec'
//			->	csStart			(0 based) starting index, or -1.
//								If -1, then the 'value' field of each 'ColorSpec' entry indicates the
//								CLUT address to which the 'rgb' contents should be applied.
//			->	csCount			(0 based) number of 'ColorSpec' entries to set
//
//	The remaining parameters are described as follows:
//
//			-> gamma			Use this gamma table to apply gamma correction
//			-> depthMode		The current relative pixel depth
//			-> bitsPerPixel		The current absolute pixel depth
//			-> luminanceMapping	'true' if luminanance mapping should occur
//			-> directColor		'true' if in a direct color mode (16 or 32 bits/pixel)
//
//=====================================================================================================
GDXErr GraphicsUtilSetEntries(const VDSetEntryRecord *setEntry, const GammaTbl *gamma, 
		DepthMode depthMode, UInt32 bitsPerPixel, Boolean luminanceMapping, Boolean directColor)
{
	SInt16 startPosition;
	SInt16 numberOfEntries;
	Boolean sequential;

	SInt16 dataWidth;
	UInt32 redIndex;
	UInt32 greenIndex;
	UInt32 blueIndex;
	
	UInt8 *redCorrection;									// Corrected red data in gamma
	UInt8 *greenCorrection;									// Corrected green data in gamma
	UInt8 *blueCorrection;									// Corrected blue data in gamma

	UInt32 i;												// Generic loop iterator
	
	GDXErr err = kGDXErrUnknownError;						// Assume failure
	
	ColorSpec *originalCSTable = setEntry->csTable;
	ColorSpec correctedCSTable[256];						// Allocate array big enough for 8, 32 bpp
		
	// Make sure that 'setEntry' is pointing to a valid 'VDSetEntryRecord'
	err = GraphicsUtilCheckSetEntry(setEntry, bitsPerPixel, &startPosition, &numberOfEntries, 
			&sequential);
	
	if (err)
		goto ErrorExit;

	// Make a copy of the client supplied 'originalCSTable' so luminance mapping (if applicable) 
	// and gamma correction can take place without altering the original.

	for (i = startPosition ; i <= (startPosition + numberOfEntries) ; i++)
	{
		UInt32 j = i - startPosition;					// 'originalCSTable' might not be 256 entries

		correctedCSTable[i].value = originalCSTable[j].value;
		correctedCSTable[i].rgb.red = originalCSTable[j].rgb.red;
		correctedCSTable[i].rgb.green = originalCSTable[j].rgb.green;
		correctedCSTable[i].rgb.blue = originalCSTable[j].rgb.blue;
	}
	
	// Check to see if luminance mapping should occur.  This should only happen if luminanceMapping
	// is enabled and the device is NOT in a direct color mode.
	if (luminanceMapping  && !directColor)
	{
		// Convert the RGB colors into luminance mapped gray scale.
		// For those familiar with color space theory, 
		// 		
		//		Luminance = .299Red + .587Green + .114Blue
		//		("Video Demystified" by Keith Jack, page 28)
		//
		// Conveniently, on the PowerPC architechture, floating point math is FASTER
		// than integer math, so outright floating point will be done.
		
		double redPortion;					// Luminance portion from red component
		double greenPortion;				// Luminance portion from green component
		double bluePortion;					// Luminance portion from blue component
		double luminance;					// Resulting luminosity
		
		for (i = startPosition ; i <= (startPosition + numberOfEntries) ; i++)
		{
			UInt32 j = i-startPosition;					// originalCSTable might not be 256 entries
			
			redPortion = 0.299 * originalCSTable[j].rgb.red;
			greenPortion = 0.587 * originalCSTable[j].rgb.green;
			bluePortion = 0.114 * originalCSTable[j].rgb.blue;
			
			luminance = redPortion + greenPortion + bluePortion;
			
			correctedCSTable[i].rgb.red = luminance;
			correctedCSTable[i].rgb.green = luminance;
			correctedCSTable[i].rgb.blue = luminance;
			
		}
	}
	
	// Apply gamma correction to the 'correctedCSTable'
	
	dataWidth = gamma->gDataWidth;
	redCorrection = (UInt8 *) &gamma->gFormulaData + gamma->gFormulaSize;
	
	if (1 == gamma->gChanCnt)
	{
		// Use same correction data for all three channels
		greenCorrection = redCorrection;
		blueCorrection = redCorrection;
	}
	else
	{
		// Each channel has its own correction data
		greenCorrection = redCorrection + gamma->gDataCnt;
		blueCorrection = redCorrection + ( 2 * gamma->gDataCnt);
	}
	
	for (i = startPosition ; i <= (startPosition + numberOfEntries) ; i++)
	{                                                                         
		// Extract the most significant 'dataWidth' amount of bits from each color
		// to use as the index into the correction data
		
		redIndex = correctedCSTable[i].rgb.red >> (16 - dataWidth);
		greenIndex = correctedCSTable[i].rgb.green >> (16 - dataWidth);
		blueIndex = correctedCSTable[i].rgb.blue >> (16 - dataWidth);
		
		correctedCSTable[i].rgb.red = *(redCorrection + redIndex);
		correctedCSTable[i].rgb.green = *(greenCorrection + greenIndex);
		correctedCSTable[i].rgb.blue = *(blueCorrection + blueIndex);
	}
	
	// Now program the CLUT
	
	err = GraphicsHALSetCLUT(originalCSTable, correctedCSTable, startPosition, numberOfEntries,
			sequential, depthMode);
			
	if (err)
		goto ErrorExit;	
	
	err = kGDXErrNoError ;									// Everything Okay
		
ErrorExit:

	return (err);
}



//=====================================================================================================
//
// GraphicsUtilBlackToWhiteRamp()
//	When in direct color mode, it is sometimes necessary to build a black-to-white ramp in RGB, and
//	apply it to the CLUT.
//
//	The will be accomplished by making a call to GraphicsUtilSetEntries().  First, a ramp will be built
//	from black-to-white with an array of ColorSpecs.  For 16 bpp, the ramp will have 32 steps, and for
//	32 bpp, the ramp will have 256.
//
//	The parameters are described as follows:
//
//			-> gamma			Use this gamma table to apply gamma correction
//			-> depthMode		The current relative pixel depth
//			-> bitsPerPixel		The current absolute pixel depth
//			-> luminanceMapping	'true' if luminanance mapping should occur
//			-> directColor		'true' if in a direct color mode (16 or 32 bits/pixel)
//
//=====================================================================================================
GDXErr GraphicsUtilBlackToWhiteRamp(const GammaTbl *gamma, DepthMode depthMode,
		UInt32 bitsPerPixel, Boolean luminanceMapping, Boolean directColor)
{

	UInt16 i ;												// Loop control variable
	UInt16 rampSteps ;										// # of steps to ramp from black to white 
	UInt16 rampIncrement;									// Increment between ramp stamps
	UInt16 rampValue ;
	
	VDSetEntryRecord setEntry;								// To make GraphicsUtilSetEntries() call

	GDXErr err = kGDXErrUnknownError;						// Assume failure
	ColorSpec rgbRampCSTable[256];							// 16 bpp = 32 ColorSpecs. 8, 32 bpp = 256
	
	if (16 == bitsPerPixel)
		rampSteps = 32;						// 16 bpp -- ramp from black-to-white in 32 steps
	else
		rampSteps = 256;					// 32 bpp -- ramp from black-to-white in 256 steps

	
	// Build the black-to-white ramp in the ColorSpec array.  ColorSpecs represent the red,
	// green, and blue components of a RGB color as unsigned 16 bit numbers.  Gamma tables only 
	// use the most significant gDataWidth # of bits of an RGB component, where gDataWidth <= 8.
	// Therefore, an adequate black-to-white ramp can be created by having the most significant
	// byte of each RGB component range from 0 - 255 in 'rampSteps' number of steps.
	
	rampIncrement = 256 / rampSteps;				// Increment between ramp steps
	rampValue = 0;									// Start ramp at 0
	
	for (i = 0 ; i < rampSteps ; i++)
	{
		rgbRampCSTable[i].rgb.red = rampValue << 8;		// Set the most significant byte to 'rampValue'
		rgbRampCSTable[i].rgb.green = rampValue << 8;	//  "   "    "       "        "   "      "
		rgbRampCSTable[i].rgb.blue = rampValue << 8;	//  "   "    "       "        "   "      "
		rampValue += rampIncrement;						// Increment the value for the next step
	}

	// Apply the black-to-white ramp to the hardware.  This can be done by calling 
	// GraphicsCoreDirectSetEntries(), so set up a 'VDSetEntryRecord' for the call.
	
	setEntry.csTable = rgbRampCSTable;				
	setEntry.csStart = 0;								// Start with index 0 of the ColorSpec array
	setEntry.csCount = rampSteps - 1;					// -1 since csCount is 0 based.
	
	// Apply the gamma corrected ramp to hardware
	err = GraphicsUtilSetEntries(&setEntry, gamma, depthMode, bitsPerPixel, luminanceMapping,
			directColor);
			
	if (err)
		goto ErrorExit;
		
	err = kGDXErrNoError ;									// Everything Okay

ErrorExit:

	return (err);
}



//=====================================================================================================
//
// GraphicsUtilGetDefaultGammaTableID()
//	This routine returns the default gamma table for the specified DisplayCode.  This is provided so
//	the driver can apply a proper gamma table during the boot sequence, prior to the DisplayMgr setting
//	the preferred one.
//
//		->	displayCode		Specifier of the display in question
//		<-	gammaTableID	Specifier of the default gamma table for the given display.			
//
//
//=====================================================================================================
GDXErr GraphicsUtilGetDefaultGammaTableID(DisplayCode displayCode, GammaTableID *gammaTableID)
{		
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();
	*gammaTableID = kGammaTableIDStandard;

	if (!coreData->onlySupportStandardGamma)
	{
		switch (displayCode)
		{
			case kDisplayCode12Inch :
				*gammaTableID = kGammaTableIDRubik;
				break;
				
			case kDisplayCodePortraitMono :	
			case kDisplayCode21InchMono :
				*gammaTableID = kGammaTableIDGray;
				break;
				
			case kDisplayCodeNTSC :	
			case kDisplayCodePAL :
				*gammaTableID = kGammaTableIDNTSCPAL;
				break;
				
			case kDisplayCodePanel :
			case kDisplayCodePanelFSTN :  
				*gammaTableID = kGammaTableIDCSCTFT;
				break;
		}
	}

	return (kGDXErrNoError);
}



//=====================================================================================================
//
// GraphicsUtilMapSenseCodesToDisplayCode()
//	This routine will map RawSenseCode/ExtendendSenseCode pairs to their corresponding DisplayCode
//	for frame buffer controllers which either have 'standard' sense code hardware (or...can coerce
//	their raw/extended sense codes to appear standard).
//
//	This functionality is provided as a utility routine in the Core, because a large number of
//	frame buffer controllers have support for 'standard' sensing.
//
//	The parameters are described as follows:
//
//			-> rawSenseCode			Result from reading sense lines
//			-> extendedSenseCode	Result from applying extended sense algorithm to sense lines
//			<- displayCode			DisplayCode which the RawSenseCode/ExtendedSenseCode map to.
//
//=====================================================================================================
GDXErr GraphicsUtilMapSenseCodesToDisplayCode(RawSenseCode rawSenseCode,
		ExtendedSenseCode extendedSenseCode, Boolean unknown, DisplayCode *displayCode)
{

	GDXErr err = kGDXErrNoError;					// Assume success
	UInt32 i;										// Loop control variable
	
	// Define a type which maps a RawSenseCode/ExtendedSenseCode pair to its corresponding DisplayCode.
	// This type is defined locally since nothing outside the scope of this function needs to 
	// be aware of it.
	
	typedef struct SenseCodesToDisplayCodeMap SenseCodesToDisplayCodeMap;
	struct SenseCodesToDisplayCodeMap
	{
		RawSenseCode rawSenseCode;
		ExtendedSenseCode extendedSenseCode;
		DisplayCode displayCode;
	};
	
	
	enum { kMapSize = 21 };
	
	SenseCodesToDisplayCodeMap map[kMapSize] =
	{
		{kRSCZero,	kESCZero21Inch,				kDisplayCode21Inch},
		
		{kRSCOne,	kESCOnePortraitMono,		kDisplayCodePortraitMono},

		{kRSCTwo,	kESCTwo12Inch,				kDisplayCode12Inch},
		
		{kRSCThree,	kESCThree21InchRadius,		kDisplayCode21Inch},
		{kRSCThree,	kESCThree21InchMonoRadius,	kDisplayCode21InchMono},
		{kRSCThree,	kESCThree21InchMono,		kDisplayCode21InchMono},
				
		{kRSCFour,	kESCFourNTSC,				kDisplayCodeNTSC},
		
		{kRSCFive,	kESCFivePortrait,			kDisplayCodePortrait},
		
		{kRSCSix,	kESCSixMSB1,				kDisplayCodeMultiScanBand1},
		{kRSCSix,	kESCSixMSB2, 				kDisplayCodeMultiScanBand2},
		{kRSCSix,	kESCSixMSB3,				kDisplayCodeMultiScanBand3},
		{kRSCSix,	kESCSixStandard,			kDisplayCodeStandard},
	
		{kRSCSeven,	kESCSevenPAL,				kDisplayCodePAL},
		{kRSCSeven, kESCSevenNTSC,				kDisplayCodeNTSC},
		{kRSCSeven, kESCSevenVGA,				kDisplayCodeVGA},
		{kRSCSeven, kESCSeven16Inch,			kDisplayCode16Inch},
		{kRSCSeven, kESCSevenPALAlternate,		kDisplayCodePAL},
		{kRSCSeven, kESCSeven19Inch, 			kDisplayCode19Inch},
		{kRSCSeven, kESCSevenNoDisplay, 		kDisplayCodeNoDisplay},
		{kRSCSeven, 0x3E,						kDisplayCodeDDCC},
		{kRSCSeven, 0x3B,						kDisplayCode16},
	};


	*displayCode = kDisplayCodeUnknown;		// Assume unknown type of display attached
	
	for (i = 0 ; i < kMapSize ; i++)
	{
		if ((map[i].rawSenseCode == rawSenseCode) && (map[i].extendedSenseCode == extendedSenseCode))
		{
			*displayCode = map[i].displayCode;
			break;
		}
	}

	if (!unknown && *displayCode == kDisplayCodeDDCC )
		*displayCode = kDisplayCodeUnknown;

	return (err);
}


//=====================================================================================================
void ReadXPRam(void *dest, UInt16 size, UInt16 srcAddress)
{
	const UniversalProcPtr* ReadXPRam = (UniversalProcPtr*)0x0544;

	ProcInfoType uppReadXPRamProcInfo = kRegisterBased
		 | REGISTER_ROUTINE_PARAMETER(1, kRegisterD1, SIZE_CODE(sizeof(UInt16)))	// A-Trap
		 | REGISTER_ROUTINE_PARAMETER(2, kRegisterA0, SIZE_CODE(sizeof(UInt32)))	// dest
		 | REGISTER_ROUTINE_PARAMETER(3, kRegisterD0, SIZE_CODE(sizeof(UInt32)));	// size:address

// = 0x00733002

	UniversalProcPtr theProc;
	if (gIsForMacOSX)
		// Don't dereference low memory in Mac OS X.
		// The OS X implementation of CallUniversalProc in IONDRVLibraries.cpp will just return error -40.
		theProc = (UniversalProcPtr)ReadXPRam;
	else
		theProc = *ReadXPRam;

	CallUniversalProc(theProc, uppReadXPRamProcInfo, _ReadXPRam, dest, (size<<16) + srcAddress);

/*
  				movea.l 	(sp)+,A0
  				move.l  	#$size:srcAddress,D0
  				_ReadXPRam
*/
}


//=====================================================================================================
//
// GraphicsUtilMapXPRAMToDispCode()
//
//=====================================================================================================
GDXErr GraphicsUtilMapXPRAMToDispCode(ResType XPRAMCode, DisplayCode *displayCode)
{
	UInt32 i;										// Loop control variable
	GDXErr err = kGDXErrNoError;					// Assume success
	
	typedef struct XPRAMCodesToDisplayCodeMap XPRAMCodesToDisplayCodeMap;
	struct XPRAMCodesToDisplayCodeMap
	{
		ResType xpramCode;
		DisplayCode displayCode;
	};


	XPRAMCodesToDisplayCodeMap map[] =
	{
		{ 'RNIN', kDisplayCodeStandard },
		{ 'SRNN', kDisplayCodeStandard },
		{ 'RN12', kDisplayCode12Inch },
		{ 'RN13', kDisplayCodeStandard },
		{ 'RN15', kDisplayCodePortrait },
		{ 'RN16', kDisplayCode16Inch },
		{ 'RN19', kDisplayCode19Inch },
		{ 'RN21', kDisplayCode21Inch },
	};

	
	*displayCode = kDisplayCodeNoDisplay;
	
	for (i = 0 ; i < sizeof( map ) / sizeof( XPRAMCodesToDisplayCodeMap )  ; i++)
	{
		if (map[i].xpramCode == XPRAMCode)
		{
			*displayCode = map[i].displayCode;
			break;
		}
	}
	
	return (err);
}



//=====================================================================================================
//
// GraphicsUtilDDCTransmitBit()
//
//=====================================================================================================
static void GraphicsUtilDDCTransmitBit(int theBit)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();
	if (theBit == 0)
		(*coreData->senseLine2ClearProc)();
	else
		(*coreData->senseLine2SetProc)();
}



//=====================================================================================================
//
// GraphicsUtilDDCTransmitByte()
//
//=====================================================================================================
static void GraphicsUtilDDCTransmitByte(UInt8 theByte)
{
	UInt32 i;
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();
	for (i = 0; i < 8; i++)
	{
		int theBit = (theByte >> (7 - i)) & 1;
		DelayForHardware(coreData->delay100microsecs);
		GraphicsUtilDDCTransmitBit(theBit);

		DelayForHardware(coreData->delay40microsecs);
		(*coreData->senseLine1SetProc)();
		DelayForHardware(coreData->delay200microsecs);
		(*coreData->senseLine1ClearProc)();
		DelayForHardware(coreData->delay40microsecs);
	}
	(*coreData->senseLine2ResetProc)();
}



//=====================================================================================================
//
// GraphicsUtilDDCWaitForAck()
//
//=====================================================================================================
static GDXErr GraphicsUtilDDCWaitForAck(void)
{
	GDXErr err = kGDXErrNoError;

	Boolean done = false;
	AbsoluteTime endTime;
	
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();

	DelayForHardware(coreData->delay40microsecs);
	endTime = AddDurationToAbsolute(durationMillisecond, UpTime());
	coreData->ddcTimedout = false;
	while (!done)
	{
		if (!(*coreData->readSenseLine2Proc)() || coreData->ddcTimedout)
			done = true;
		if (AbsoluteDeltaToDuration(UpTime(), endTime) != 0)
			coreData->ddcTimedout = true;
	}
	DelayForHardware(coreData->delay40microsecs);
	(*coreData->senseLine1SetProc)();
	DelayForHardware(coreData->delay200microsecs);
	(*coreData->senseLine1ClearProc)();
	DelayForHardware(coreData->delay40microsecs);
	if (coreData->ddcTimedout)
	{
		err = kGDXErrDDCError43;
		goto ErrorExit;
	}
ErrorExit:
	return err;
}



//=====================================================================================================
//
// GraphicsUtilGetDDCBlock_2a1()
//
//=====================================================================================================
static void GraphicsUtilGetDDCBlock_2a1(void)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();
	DelayForHardware(coreData->delay200microsecs);
	(*coreData->senseLine2ClearProc)();
	(*coreData->senseLine1ClearProc)();
	DelayForHardware(coreData->delay100microsecs);
	(*coreData->senseLine1SetProc)();
	DelayForHardware(coreData->delay200microsecs);
	(*coreData->senseLine2SetProc)();
	DelayForHardware(coreData->delay100microsecs);
	(*coreData->senseLine2and1ResetProc)();
}



//=====================================================================================================
//
// GraphicsUtilDDCDoSomething()
//
//=====================================================================================================
static void GraphicsUtilDDCDoSomething(void)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();
	(*coreData->senseLine2SetProc)();
	(*coreData->senseLine1SetProc)();
	DelayForHardware(coreData->delay100microsecs);
	(*coreData->senseLine2ClearProc)();
	DelayForHardware(coreData->delay100microsecs);
	(*coreData->senseLine1ClearProc)();
	DelayForHardware(coreData->delay100microsecs);
}



//=====================================================================================================
//
// GraphicsUtilDDCDoSomething2()
//
//=====================================================================================================
static void GraphicsUtilDDCDoSomething2(void)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();
	(*coreData->senseLine1ResetProc)();
	(*coreData->senseLine2ResetProc)();
	DelayForHardware(coreData->delay200microsecs);
}

//=====================================================================================================
//
// GraphicsUtilDDCTransmitBuffer()
//
//=====================================================================================================
static GDXErr GraphicsUtilDDCTransmitBuffer(UInt8 theByte, UInt8 numBytes, UInt8* buffer)
{
	UInt32 i;
	GDXErr err;

	GraphicsUtilDDCDoSomething2();
	GraphicsUtilDDCDoSomething();

	GraphicsUtilDDCTransmitByte(theByte);
	err = GraphicsUtilDDCWaitForAck();
	if (!err)
	{
		for (i = 0; i < numBytes; i++)
		{
			GraphicsUtilDDCTransmitByte(buffer[i]);
			err = GraphicsUtilDDCWaitForAck();
			if (err)
				break;
		}
	}
	if (err)
		GraphicsUtilGetDDCBlock_2a1();
	return err;
}



//=====================================================================================================
//
// GraphicsUtilDDCReceiveByte()
//
//=====================================================================================================
static GDXErr GraphicsUtilDDCReceiveByte(UInt8* theByte)
{
	UInt32 i;
	GDXErr err;

	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();
	coreData->ddcTimedout = false;
	(*coreData->senseLine1ClearProc)();
	(*coreData->senseLine2ResetProc)();
	for (i = 0; i < 7; i++)
	{
		AbsoluteTime endTime;
		Boolean done;
		int theBit;
		
		DelayForHardware(coreData->delay100microsecs);
		(*coreData->senseLine1ResetProc)();
		endTime = AddDurationToAbsolute(2*durationMillisecond, UpTime());
		while (!done)
		{
			if (!(*coreData->readSenseLine1Proc)() || coreData->ddcTimedout)
				done = true;
			if (AbsoluteDeltaToDuration(UpTime(), endTime) != 0)
				coreData->ddcTimedout = true;
		}
		theBit = (*coreData->readSenseLine2Proc)();
		*theByte |= theBit << (7 - i);
		DelayForHardware(coreData->delay200microsecs);
		(*coreData->senseLine1ClearProc)();
		DelayForHardware(coreData->delay40microsecs);
		if (coreData->ddcTimedout)
		{
			err = kGDXErrDDCError43;
			goto ErrorExit;
		}
		err = kGDXErrNoError;
	}
ErrorExit:
	return err;
}



//=====================================================================================================
//
// GraphicsUtilDDCproc93()
//
//=====================================================================================================
static void GraphicsUtilDDCproc93(void)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();
	(*coreData->senseLine1ClearProc)();
	DelayForHardware(coreData->delay20microsecs);
	DelayForHardware(coreData->delay40microsecs);

	(*coreData->senseLine2SetProc)();

	DelayForHardware(coreData->delay100microsecs);
	(*coreData->senseLine1SetProc)();
	DelayForHardware(coreData->delay200microsecs);
	(*coreData->senseLine1ClearProc)();
	DelayForHardware(coreData->delay40microsecs);
	(*coreData->senseLine2ResetProc)();

	DelayForHardware(coreData->delay100microsecs);
	DelayForHardware(coreData->delay100microsecs);
}



//=====================================================================================================
//
// GraphicsUtilDDCproc93sortof()
//
//=====================================================================================================
static void GraphicsUtilDDCkindoflikeproc93(void)
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();
	(*coreData->senseLine1ClearProc)();
	DelayForHardware(coreData->delay20microsecs);
	DelayForHardware(coreData->delay40microsecs);

	(*coreData->senseLine2ClearProc)();

	DelayForHardware(coreData->delay100microsecs);
	(*coreData->senseLine1SetProc)();
	DelayForHardware(coreData->delay200microsecs);
	(*coreData->senseLine1ClearProc)();
	DelayForHardware(coreData->delay40microsecs);
	(*coreData->senseLine2ResetProc)();
}



//=====================================================================================================
//
// GraphicsUtilDDCproc94()
//
//=====================================================================================================
GDXErr GraphicsUtilDDCproc94(void)
{
	UInt32 i;
	GDXErr err;
	UInt8 buffer;
	UInt8 receivedByte;

	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();
	(*coreData->senseLine1ClearProc)();
	for (i = 0; i < 34; i++)
	{
		DelayForHardware(coreData->delay1millisecs);
	}
	buffer = 0;
	err = GraphicsUtilDDCTransmitBuffer(0xA0, sizeof(buffer), &buffer);
	if (err == kGDXErrNoError)
	{
		GraphicsUtilDDCDoSomething();

		GraphicsUtilDDCTransmitByte(0xA1);
		err = GraphicsUtilDDCWaitForAck();
		if (err == kGDXErrNoError)
			err = GraphicsUtilDDCReceiveByte(&receivedByte);
	}
	GraphicsUtilDDCproc93();
	GraphicsUtilGetDDCBlock_2a1();
	return err;
}



//=====================================================================================================
//
// GraphicsUtilDDCReceiveBuffer()
//
//=====================================================================================================
static GDXErr GraphicsUtilDDCReceiveBuffer(UInt8 theByte, UInt8 numBytes, UInt8* ddcBlockData)
{
	UInt32 i;
	GDXErr err;

	GraphicsUtilDDCDoSomething2();
	GraphicsUtilDDCDoSomething();

	{
		GraphicsUtilDDCTransmitByte(theByte | 1);
		err = GraphicsUtilDDCWaitForAck();
		if (err)
			goto ErrorExit;
		for (i = 0; i < numBytes - 1; i++)
		{
			ddcBlockData[i] = 0;
			err = GraphicsUtilDDCReceiveByte(&ddcBlockData[i]);
			if (err)
				goto ErrorExit;
			GraphicsUtilDDCkindoflikeproc93();
		}
		ddcBlockData[numBytes - 1] = 0;
		err = GraphicsUtilDDCReceiveByte(&ddcBlockData[numBytes - 1]);
	ErrorExit:
		err; //		internal return err;
	}

	GraphicsUtilDDCproc93();
	GraphicsUtilGetDDCBlock_2a1();
	return err;
}



//=====================================================================================================
//
// GraphicsUtilDDCReceiveDDCBLock()
//
//=====================================================================================================
static GDXErr GraphicsUtilDDCReceiveDDCBLock(UInt8 theByte, UInt8 theByte2, UInt8* ddcBlockData)
{
	UInt32 i;
	GDXErr err;
	UInt8 buffer;
	UInt8 checksum = 0;
	
	buffer = theByte2;
	err = GraphicsUtilDDCTransmitBuffer(theByte, sizeof(buffer), &buffer);
	if (err)
		goto ErrorExit;
	err = GraphicsUtilDDCReceiveBuffer(theByte, kDDCBlockSize, ddcBlockData);
	if (err)
		goto ErrorExit;
	for (i = 0; i < kDDCBlockSize - 1; i++)
		checksum += ddcBlockData[i];
	if (0x100 - checksum != ddcBlockData[kDDCBlockSize-1])
		err = kGDXErrDDCError44;
ErrorExit:
	return err;
}



//=====================================================================================================
//
// GraphicsUtilDo9_2a1()
//
//=====================================================================================================
static void GraphicsUtilDo9_2a1(void)
{
	GraphicsUtilGetDDCBlock_2a1();
	GraphicsUtilGetDDCBlock_2a1();
	GraphicsUtilGetDDCBlock_2a1();
	GraphicsUtilGetDDCBlock_2a1();
	GraphicsUtilGetDDCBlock_2a1();
	GraphicsUtilGetDDCBlock_2a1();
	GraphicsUtilGetDDCBlock_2a1();
	GraphicsUtilGetDDCBlock_2a1();
	GraphicsUtilGetDDCBlock_2a1();
}


//=====================================================================================================
//
// GraphicsUtilGetDDCBlock_2a()
//
//=====================================================================================================
GDXErr GraphicsUtilGetDDCBlock_2a(UInt32 theBool, UInt8* ddcBlockData)
{
	UInt32 i;
	GDXErr err;
	UInt8 theByte;
	
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();
	GraphicsUtilDo9_2a1();
	theByte = (theBool << 7) + 0x80;
	err = GraphicsUtilDDCReceiveDDCBLock(0xA0, theByte, ddcBlockData);
	if (err)
	{
		GraphicsUtilDo9_2a1();
		if (err == kGDXErrDDCError43)
		{
			err = GraphicsUtilDDCReceiveDDCBLock(0xA0,theByte, ddcBlockData);
			if (err == kGDXErrDDCError43)
			{
				GraphicsUtilDo9_2a1();
			}
			else if (err == kGDXErrDDCError44)
			{
				GraphicsUtilDo9_2a1();
				err = GraphicsUtilDDCReceiveDDCBLock(0xA0,theByte, ddcBlockData);
				if (err)
				{
					GraphicsUtilDo9_2a1();
				}
			}
		}
		else if (err == kGDXErrDDCError44)
		{
			GraphicsUtilDo9_2a1();
			err = GraphicsUtilDDCReceiveDDCBLock(0xA0,theByte, ddcBlockData);
			if (err)
			{
				GraphicsUtilDo9_2a1();
				err = GraphicsUtilDDCReceiveDDCBLock(0xA0,theByte, ddcBlockData);
				if (err)
				{
					GraphicsUtilDo9_2a1();
					err = GraphicsUtilDDCReceiveDDCBLock(0xA0,theByte, ddcBlockData);
					if (err)
					{
						GraphicsUtilDo9_2a1();
					}
				}
			}
		}
		if (err)
			goto ErrorExit;
	}
	if (theBool)
	{
		for (i = 0; i < kDDCBlockSize; i++)
			((UInt8*)&coreData->ddcBlockData)[i] = ddcBlockData[i];
	}
ErrorExit:
	return err;
}



//=====================================================================================================
//
// GraphicsUtilGetDDCBlock()
//
//=====================================================================================================
GDXErr GraphicsUtilGetDDCBlock(VDDDCBlockRec *vdDDCBlock)
{
	UInt32 i;
	GDXErr err;
	RawSenseCode rawSenseCode;
	Boolean someBoolean;
	Byte* ddcBlockDataSrc;
	Byte ddcBlockData[128];

	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();
	if (vdDDCBlock->ddcBlockNumber == 0 || vdDDCBlock->ddcBlockType != kDDCBlockTypeEDID)
	{
		err = kGDXErrInvalidParameters;
		goto ErrorExit;
	}
	if ((vdDDCBlock->ddcFlags & kDDCForceReadMask) && vdDDCBlock->ddcBlockNumber == 1)
	{
		(*coreData->resetSenseLinesProc)();
		GraphicsUtilDDCproc94();
		(*coreData->resetSenseLinesProc)();
		rawSenseCode = (*coreData->readSenseLinesProc)();
		if (coreData->builtInConnection)
		{
			AbsoluteTime currentTime = UpTime();
			if (*(UInt64*)(&currentTime) > *(UInt64*)(&coreData->time5secondsAfterOpen))
				coreData->builtInConnection = false;
		}
		if (rawSenseCode >= kRSCSix)
		{
			someBoolean = GraphicsUtilGetDDCBlock_2a(true, ddcBlockData);
			(*coreData->resetSenseLinesProc)();
			if (!someBoolean)
				coreData->builtInConnection = false;
			else
			{
				err = kGDXErrDDCError44;
				goto ErrorExit;
			}
		}
		else
		{
			err = kGDXErrDDCError44;
			goto ErrorExit;
		}
		coreData->hasDDCConnection = true;
	}
	if (!coreData->hasDDCConnection)
	{
		err = kGDXErrDDCError44;
		goto ErrorExit;
	}
	if (vdDDCBlock->ddcBlockNumber > coreData->ddcBlockData.extension_flag + 1)
	{
		err = kGDXErrInvalidParameters;
		goto ErrorExit;
	}
	if (vdDDCBlock->ddcBlockNumber == 1)
		ddcBlockDataSrc = (UInt8*)&coreData->ddcBlockData;
	else
	{
		(*coreData->resetSenseLinesProc)();
		someBoolean = GraphicsUtilGetDDCBlock_2a(vdDDCBlock->ddcBlockNumber, ddcBlockData);
		(*coreData->resetSenseLinesProc)();
		if (someBoolean)
		{
			err = kGDXErrDDCError44;
			goto ErrorExit;
		}
		ddcBlockDataSrc = ddcBlockData;
	}
	for (i = 0 ; i < kDDCBlockSize ; i++)
		vdDDCBlock->ddcBlockData[i] = ddcBlockDataSrc[i];
	if (coreData->setDDCInfoProc)
		(*coreData->setDDCInfoProc)(vdDDCBlock->ddcBlockData, vdDDCBlock->ddcBlockNumber);
	err = kGDXErrNoError;
ErrorExit:
		return err;
}



//=====================================================================================================
//
// GraphicsUtilSetSync_2a()
//
//=====================================================================================================
static void GraphicsUtilSetSync_2a(Duration duration)
{
	AbsoluteTime endTime;
	UInt32 first;
	UInt32 second;
	
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();
	(*coreData->senseLine2ResetProc)();
	endTime = AddDurationToAbsolute(duration, UpTime());
	first = (*coreData->readSenseLine2Proc)();
	do {
		second = (*coreData->readSenseLine2Proc)();
		if (second != first)
			break;
	}
	while (AbsoluteDeltaToDuration(UpTime(), endTime) == 0);
}



//=====================================================================================================
//
// GraphicsUtilSetSync_2()
//
//=====================================================================================================
GDXErr GraphicsUtilSetSync_2(void)
{
	GDXErr err;

	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();

	err = GraphicsUtilDDCproc94();
	if (!err)
		return err;

	GraphicsUtilSetSync_2a(1700);
	err = GraphicsUtilDDCproc94();
	if (!err)
		return err;

	GraphicsUtilSetSync_2a(1700);
	err = GraphicsUtilDDCproc94();
	if (!err)
		return err;
	
	
	GraphicsUtilSetSync_2a(1700);
	err = GraphicsUtilDDCproc94();
	return err;
}



//=====================================================================================================
//
// FindNamedRegEntry()
//
//=====================================================================================================
Boolean FindNamedRegEntry(const char *propertyName, RegEntryID *regEntryID)
{
	Boolean didFind = false;
	RegEntryIter cookie;
	RegCStrEntryNameBuf nameComponent;
	RegEntryID parentEntry;
	OSStatus osStatusErr;
	
	if (noErr == RegistryEntryIterateCreate(&cookie))
	{
		while (1)
		{
			Boolean done, done2 = false;
			RegistryEntryIterate( &cookie, kRegIterContinue, regEntryID, &done);
			if (done)
				break;
			nameComponent[0] = 0;
			osStatusErr = RegistryCStrEntryToName( regEntryID, &parentEntry, nameComponent, &done2 );
			if (noErr == osStatusErr)
			{
				if (CStrCmp(nameComponent, propertyName) == 0)
				{
					didFind = true;
					break;
				}
			}
			if (done || osStatusErr)
				break;
		}
	}
	RegistryEntryIterateDispose( &cookie );
	return didFind;
}



//=====================================================================================================
//
// LocateParentDevice()
//
//=====================================================================================================
Boolean GetRegEntryParent(const RegEntryID *deviceEntry, RegEntryID *parentEntry)
{
	RegCStrEntryName deviceNameBuf[kRegCStrMaxEntryNameLength + 1];
	Boolean done;
	OSStatus err = RegistryCStrEntryToName(deviceEntry, parentEntry, &deviceNameBuf[0], &done);
	return (err == noErr) && !done;
}



//=====================================================================================================
//
// SetDisplayProperties()
//
//=====================================================================================================
void SetDisplayProperties( DisplayCode displayCode, Boolean blackAndWhite )
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();

	coreData->displayCode = displayCode;
	coreData->monoOnly = blackAndWhite;
	coreData->luminanceMapping = blackAndWhite;
}



//=====================================================================================================
//
// SetDDCAndBuiltInFlags()
//
//=====================================================================================================
void SetDDCAndBuiltInFlags( Boolean hasDDCConnection, Boolean builtInConnection )
{
	GraphicsCoreData *coreData = GraphicsCoreGetCoreData();

	coreData->hasDDCConnection = hasDDCConnection;
	coreData->builtInConnection = builtInConnection;
	if (builtInConnection)
		coreData->delay5secs = DurationToAbsolute(5000);
}
