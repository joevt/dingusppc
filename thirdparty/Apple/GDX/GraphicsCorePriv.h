/*
	File:		GraphicsCorePriv.h

	Contains:	This has declarations that are private to the graphics 'core.'  Neither the HALs nor
				the OSS need any of this.

	Written by:	Sean Williams, Kevin Williams

	Copyright:	(c) 1994-1995 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

		 <1>	 4/15/95	SW		First Checked In

*/

#ifndef __GRAPHICSCOREPRIV__
#define __GRAPHICSCOREPRIV__

#include "GraphicsPriv.h"
#include "vbe.h"

#include <Types.h>
#include <Devices.h>
#include <NameRegistry.h>
#include <Video.h>


typedef GDXErr (* ErrProc )(GDXErr err);
typedef GDXErr (* DisplayConnectionProc )(VDDisplayConnectInfoRec *displayConnectInfo);

//
// GraphicsCoreData
//	This structure contains the 'globals' needed to maintain the necessary state
//	information regarding the graphics core.
//
typedef struct GraphicsCoreData GraphicsCoreData;
struct GraphicsCoreData
{
	RegEntryID regEntryID;			// RegEntryID describing Graphics HW
	AddressSpaceID spaceID;			// grabbed in Initialize 
	DriverRefNum driverRefNum;		// Reference number of driver
	DepthMode depthMode;			// Relative bit depth
	UInt32 bitsPerPixel;			// Absolute bit depth
	DisplayModeID displayModeID;	// Current display mode selector
	DisplayCode displayCode;		// DisplayCode for the connected display

	SInt16 currentPage;				// Current graphics page (0 based)
	void *baseAddress;				// Base address of current page of frame buffer
	GammaTbl *gammaTable;			// Current gamma table
	unsigned long maxGammaTableSize;// Biggest gamma table allocated..reuse existing gamma if can fit

	AbsoluteTime			delay20microsecs;
	AbsoluteTime			delay40microsecs;
	AbsoluteTime			delay100microsecs;
	AbsoluteTime			delay200microsecs;
	AbsoluteTime			delay1millisecs;
	AbsoluteTime			delay5secs;
	AbsoluteTime			time5secondsAfterOpen;
	BooleanProc				readSenseLine2Proc;
	BooleanProc				readSenseLine1Proc;
	VoidProc				senseLine2SetProc;
	VoidProc				senseLine2ClearProc;
	VoidProc				senseLine1SetProc;
	VoidProc				senseLine1ClearProc;
	VoidProc				senseLine2ResetProc;
	VoidProc				senseLine1ResetProc;
	VoidProc				senseLine2and1ResetProc;
	VoidProc				resetSenseLinesProc;
	RawSenseCodeProc		readSenseLinesProc;
	DDCPostProcessBlockProc	setDDCInfoProc;
	struct vbe_edid1_info	ddcBlockData;
//	UInt8					ddcBlockNumber;
//	Boolean					ddcBlockData.checksum;
	ErrProc					processErrorProc;
	BooleanProc				getMonoOnly;
	DisplayConnectionProc	modifyConnection;
	Boolean					reportsDDCConnection;
	Boolean					hasDDCConnection;
	Boolean					ddcTimedout;
	Boolean					hasTriStateSync;
	Boolean					triStateSyncOn;

	Boolean luminanceMapping;		// True if using luminance mapping
	Boolean	monoOnly;				// True if attached display only support Monochrome
	Boolean directColor;			// True for direct color, false for	indexed color
	Boolean interruptsEnabled;		// True when VBL interrupts are enabled
	Boolean driverOpen;				// True the driver is opened
	Boolean replacingDriver;		// True if got a kReplaceCommand instead of kInitializeCommand

	Boolean					graphicsCoreInited;
	Boolean					onlySupportStandardGamma;
	Boolean					supportsHardwareCursor;
	Boolean					doDisposeVBLandFBConnectInterruptService;	//			TRUE
	Boolean					builtInConnection;
	Boolean					useGrayPatterns;
	UInt32					grayPattern8bpp;
	UInt32					grayPattern16bpp;
	UInt32					grayPattern32bpp;
};



// Prototype for access function to core data
GraphicsCoreData *GraphicsCoreGetCoreData(void);


#endif	// __GRAPHICSCOREPRIV__
