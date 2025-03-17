/*
	File:		GraphicsCoreUtils.h

	Contains:	Declarations for 'utility' routines that portions of the GDX model (core, OSS, HAL)
				might want to make use of.

	Written by:	Sean Williams, Kevin Williams

	Copyright:	(c) 1994-1995 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

		 <2>	 7/17/95	SW		Added declaration for GraphicsUtil GetDefaultGammaTableID()
		 <1>	 4/15/95	SW		First Checked In

*/

#ifndef __GRAPHICSCOREUTILS__
#define __GRAPHICSCOREUTILS__
#include "GraphicsPriv.h"

#include <Types.h>
#include <Video.h>


// The following are prototypes for utility functions that the Core uses.

GDXErr GraphicsUtilCheckSetEntry(const VDSetEntryRecord *setEntry, UInt32 bitsPerPixel,
		SInt16 *startPosition, SInt16 *numberOfEntries, Boolean *sequential);

GDXErr GraphicsUtilSetEntries(const VDSetEntryRecord *setEntry, const GammaTbl *gamma, 
		DepthMode depthMode, UInt32 bitsPerPixel, Boolean luminanceMapping, Boolean directColor);
		
GDXErr GraphicsUtilBlackToWhiteRamp(const GammaTbl *gamma, DepthMode depthMode,
		UInt32 bitsPerPixel, Boolean luminanceMapping, Boolean directColor);
		
GDXErr GraphicsUtilGetDefaultGammaTableID(DisplayCode displayCode, GammaTableID *gammaTableID);

GDXErr GraphicsUtilMapSenseCodesToDisplayCode(RawSenseCode rawSenseCode,
		ExtendedSenseCode extendedSenseCode, Boolean unknown, DisplayCode *displayCode);

void ReadXPRam(void *dest, UInt16 size, UInt16 srcAddress);

GDXErr GraphicsUtilMapXPRAMToDispCode(ResType XPRAMCode, DisplayCode *displayCode);

GDXErr GraphicsUtilSetSync_2(void);

Boolean FindNamedRegEntry(const char *propertyName, RegEntryID *regEntryID);

Boolean GetRegEntryParent(const RegEntryID *deviceEntry, RegEntryID *parentEntry);

void SetDisplayProperties( DisplayCode displayCode, Boolean blackAndWhite );

void SetDDCAndBuiltInFlags( Boolean hasDDCConnection, Boolean builtInConnection );

GDXErr GraphicsUtilGetDDCBlock(VDDDCBlockRec *vdDDCBlock);
GDXErr GraphicsUtilGetDDCBlock_2a(UInt32 theBool, UInt8* ddcBlockData);

GDXErr GraphicsUtilDDCproc94(void);

#endif	// __GRAPHICSCOREUTILS__
