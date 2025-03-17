/*
	File:		GraphicsCoreStatus.h

	Contains:	Declarations for the the routines that implement the driver's 'Status' calls.

	Written by:	Sean Williams, Kevin Williams

	Copyright:	(c) 1994-1995 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

		 <1>	 4/15/95	SW		First Checked In

*/

#ifndef __GRAPHICSCORESTATUS__
#define __GRAPHICSCORESTATUS__

#include "GraphicsPriv.h"
#include <Video.h>

// Declaration of Core driver calls
// These are the declarations for the meat of the driver Control routines.

#ifdef __cplusplus
extern "C" {
#endif

GDXErr GraphicsCoreGetMode(VDPageInfo *pageInfo);										// csCode = 2
GDXErr GraphicsCoreGetEntries(VDSetEntryRecord *setEntry);								// csCode = 3
GDXErr GraphicsCoreGetPages(VDPageInfo *pageInfo);										// csCode = 4
GDXErr GraphicsCoreGetBaseAddress(VDPageInfo *pageInfo);								// csCode = 5
GDXErr GraphicsCoreGetGray(VDGrayRecord *gray);											// csCode = 6
GDXErr GraphicsCoreGetInterrupt(VDFlagRecord *flag);									// csCode = 7
GDXErr GraphicsCoreGetGamma(VDGammaRecord *gamma);										// csCode = 8
GDXErr GraphicsCoreGetCurrentMode(VDSwitchInfoRec *switchInfo);							// csCode = 10
GDXErr GraphicsCoreGetSync(VDSyncInfoRec *sync);										// csCode = 11
GDXErr GraphicsCoreGetConnection(VDDisplayConnectInfoRec *displayConnectInfo); 			// csCode = 12
GDXErr GraphicsCoreGetModeTiming(VDTimingInfoRec *timingInfo);							// csCode = 13
GDXErr GraphicsCoreGetPreferredConfiguration(VDSwitchInfoRec *switchInfo);				// csCode = 16
GDXErr GraphicsCoreGetNextResolution(VDResolutionInfoRec *resolutionInfo);				// csCode = 17
GDXErr GraphicsCoreGetVideoParams(VDVideoParametersInfoRec *videoParamatersInfo);		// csCode = 18
GDXErr GraphicsCoreGetGammaInfoList(VDGetGammaListRec *getGammaList);					// csCode = 20
GDXErr GraphicsCoreRetrieveGammaTable(VDRetrieveGammaRec *getGammaList);				// csCode = 21
GDXErr GraphicsCoreSupportsHardwareCursor(VDSupportsHardwareCursorRec *supportsHardwareCursor);	// csCode = 22
GDXErr GraphicsCoreGetHardwareCursorDrawState(VDHardwareCursorDrawStateRec *cursorDrawState);	// csCode = 23
GDXErr GraphicsCoreGetPowerState(VDPowerStateRec *vdPowerState);						// csCode = 25
GDXErr GraphicsCoreGetDDCBlock(VDDDCBlockRec *vdDDCBlock);								// csCode = 27
GDXErr GraphicsCoreGetClutBehavior(VDClutBehavior *vdClutBehavior);						// csCode = 29
GDXErr GraphicsCoreGetTimingRanges(VDDisplayTimingRangeRec *vdDisplayTimingRange);		// csCode = 30
GDXErr GraphicsCoreGetDetailedTiming(VDDetailedTimingRec *vdDetailedTiming);			// csCode = 31


#ifdef __cplusplus
}
#endif

#endif	// __GRAPHICSCORESTATUS__
