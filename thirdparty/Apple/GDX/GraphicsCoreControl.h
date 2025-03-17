/*
	File:		GraphicsCoreControl.h

	Contains:	Declarations for the the routines that implement the driver's 'Control' calls.

	Written by:	Sean Williams, Kevin Williams

	Copyright:	(c) 1994-1995 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

		 <1>	 	 4/15/95	SW		First Checked In

*/

#ifndef __GRAPHICSCORECONTROL__
#define __GRAPHICSCORECONTROL__

#include "GraphicsPriv.h"
#include <Video.h>


// Declaration of Core driver calls
// These are the declarations for the meat of the driver Control routines.

#ifdef __cplusplus
extern "C" {
#endif

GDXErr GraphicsCoreSetMode(VDPageInfo *pageInfo);										// csCode = 2
GDXErr GraphicsCoreSetEntries(const VDSetEntryRecord *setEntry);						// csCode = 3
GDXErr GraphicsCoreSetGamma(const VDGammaRecord *gamma);								// csCode = 4
GDXErr GraphicsCoreGrayPage(const VDPageInfo *pageInfo);								// csCode = 5
GDXErr GraphicsCoreSetGray(VDGrayRecord *grayPtr);										// csCode = 6
GDXErr GraphicsCoreSetInterrupt(const VDFlagRecord *flag);								// csCode = 7
GDXErr GraphicsCoreDirectSetEntries(const VDSetEntryRecord *setEntry);					// csCode = 8
GDXErr GraphicsCoreSwitchMode(VDSwitchInfoRec *switchInfo);								// csCode = 10
GDXErr GraphicsCoreSetSync(VDSyncInfoRec *sync);										// csCode = 11
GDXErr GraphicsCoreSetPreferredConfiguration(const VDSwitchInfoRec *switchInfo);		// csCode = 16
GDXErr GraphicsCoreSetHardwareCursor(const VDSetHardwareCursorRec *setHardwareCursorParams);	// csCode = 22
GDXErr GraphicsCoreDrawHardwareCursor(const VDDrawHardwareCursorRec *drawHardwareCursorParams);	// csCode = 23
GDXErr GraphicsCoreSetPowerState(VDPowerStateRec *vdPowerState);						// csCode = 25
GDXErr GraphicsCoreSetClutBehavior(VDClutBehavior *vdClutBehavior);						// csCode = 29
GDXErr GraphicsCoreSetDetailedTiming(VDDetailedTimingRec *vdDetailedTiming);			// csCode = 31

#ifdef __cplusplus
}
#endif

#endif	// __GRAPHICSCORECONTROL__
