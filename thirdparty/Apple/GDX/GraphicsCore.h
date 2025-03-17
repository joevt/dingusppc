/*
	File:		GraphicsCore.h

	Contains:	Function declarations of the 'Core' graphics routines.

	Written by:	Sean Williams, Kevin Williams

	Copyright:	(c) 1994-1995 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

		 <1>	 4/15/95	SW		First Checked In

*/

#ifndef __GRAPHICSCORE__
#define __GRAPHICSCORE__

#include <Devices.h>
#include <Types.h>
#include <Files.h>					// CntrlParam
#include <Kernel.h>					// AddressSpaceID

// Declarations for GraphicsCore routines.
OSErr GraphicsInitialize(DriverRefNum refNum, const RegEntryID *regEntryID, const AddressSpaceID spaceID);
OSErr GraphicsReplace(DriverRefNum refNum, const RegEntryID *regEntryID, const AddressSpaceID spaceID);
OSErr GraphicsOpen(void);
OSErr GraphicsClose(void);
OSErr GraphicsControl(CntrlParam *pb);
OSErr GraphicsStatus(CntrlParam	*pb);
OSErr GraphicsFinalize(DriverRefNum refNum, const RegEntryID *regEntryID);
OSErr GraphicsSupersede(DriverRefNum refNum, const RegEntryID* regEntryID);
#endif	// __GRAPHICSCORE__
