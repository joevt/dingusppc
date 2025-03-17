/*
	File:		GraphicsOSS.h

	Contains:	This file contains the declarations for the (weak) abstraction layer for Operating 
				Systems Services (OSS).

	Written by:	Sean Williams, Kevin Williams

	Copyright:	(c) 1994-1995 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

		 <2>	 7/17/95	SW		Added declaration for GraphicsOSS DoVSLService()
		 <1>	 4/15/95	SW		First Checked In

*/
#ifndef __GRAPHICSOSS__
#define __GRAPHICSOSS__


#include "GraphicsPriv.h"
#include <Devices.h>				// IOCommandID, etc
#include <Kernel.h>
#include <NameRegistry.h>
#include <VideoServices.h>

//
// DoDriverIO()
//	This is the entry point for the native Graphics Driver.
OSErr DoDriverIO (AddressSpaceID SpaceID, IOCommandID ioCommandID, IOCommandContents ioCommandcontents, IOCommandCode ioCommandCode,
		IOCommandKind ioCommandKind);

GDXErr GraphicsOSSSaveProperty(const RegEntryID *regEntryID, const char *propertyName,
		const void *propertyValue, ByteCount propertySize, OSSPropertyStorage ossPropertyStorage);
GDXErr GraphicsOSSGetProperty(const RegEntryID *regEntryID, const char *propertyName,
		void *propertyValue, ByteCount propertySize);
GDXErr GraphicsOSSDeleteProperty(const RegEntryID *regEntryID, const char *propertyName);

GDXErr GraphicsOSSSetCorePref(const RegEntryID *regEntryID, GraphicsPreferred *graphicsPreferred);
GDXErr GraphicsOSSGetCorePref(const RegEntryID *regEntryID, GraphicsPreferred *graphicsPreferred);
GDXErr GraphicsOSSSetHALPref(const RegEntryID *regEntryID, UInt32 halData);
GDXErr GraphicsOSSGetHALPref(const RegEntryID *regEntryID, UInt32 *halData);

void GraphicsOSSKillPrivateData();
GDXErr GraphicsOSSInstallVBLInterrupts(const RegEntryID *regEntryID);
GDXErr GraphicsOSSNewInterruptService(InterruptServiceType theServiceType);
GDXErr GraphicsOSSDisposeInterruptService(InterruptServiceType theServiceType);

Boolean GraphicsOSSSetVBLInterrupt(Boolean enableInterrupts);
void GraphicsOSSVBLDefaultEnabler(void);
Boolean GraphicsOSSVBLDefaultDisabler(void);

void GraphicsOSSDoVSLInterruptService(void);

void GraphicsOSSInterruptHandler(InterruptServiceType theServiceType);

extern Boolean gIsForMacOSX;

#endif		// __GRAPHICSOSS__