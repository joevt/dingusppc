/*
	File:		GraphicsOSS.c

	Contains:	This file implements the (weak) abstraction layer for Operating Systems Services (OSS)

	Written by:	Sean Williams, Kevin Williams

	Copyright:	(c) 1994-1995 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

		 <2>	 7/17/95	SW		Added GraphicsOSS DoVSLInterruptService() for hardware which 
		 							doesn't implement true hardware VBL interrupts.
		 <1>	 4/15/95	SW		First Checked In

*/

#include "GraphicsPriv.h"
#include "GraphicsOSS.h"
#include "GraphicsCore.h"
#include "GraphicsHAL.h"
#include "GraphicsCoreStatus.h"

#include <Devices.h>				// IOCommandID, etc
#include <DriverServices.h>			// for string functions
#include <Errors.h>
#include <Interrupts.h>				// handler stuff
#include <Files.h>					// CntrlParam
#include <Kernel.h>					// AddressSpace
#include <NameRegistry.h>			// IOCommandContents.initialinfo has RegEntryID
#include <Types.h>
#include <VideoServices.h>



//
// GraphicsNonVolatile
//	This structure describes the PropertyValue that is needed for the
//	GetPreferredConfiguration and SetPreferredConfiguration calls.  The DisplayCode is saved in
//	addition to the DisplayModeID and the DepthMode since the core looks at the displayCode from the
//	previous boot.  Since a device is limited to 8 bytes of NVRAM information, the GraphicsNonVolatile
//	stucture holds the compressed information that the GraphicsPreferredStructure expects.
//	4 bytes are reserved for the "Core", 4 bytes are reserved for the HAL
//
typedef struct GraphicsNonVolatile GraphicsNonVolatile;
struct GraphicsNonVolatile 
{
	UInt8 reserved;					// Reserved for future use by the core
    UInt8 mappedDisplayModeID;
    UInt8 mappedDepthMode;
	UInt8 mappedDisplayCode;		// Save the DisplayCode in case the user switches monitors
	UInt32 halData;					// The HAL gets 4 bytes of the NVRAM data
};


typedef UInt32 OSSMapDirection;
enum
{
	kOSSMapExpand = 0,
	kOSSMapCompress
};


typedef struct InterruptServiceInfo InterruptServiceInfo;
struct InterruptServiceInfo
{
	long unknown;
	InterruptServiceIDType vslServiceID;		// VSL service ID...run task for this id
	Boolean vslServiceIDValid;					// 'true' when OSS has a valid VSLServiceID
};

//
// OSSData (IPI)
//	This structure contains the 'globals' needed to maintain the necessary state
//	information regarding the graphics core.
//
typedef struct OSSData OSSData;
struct OSSData
{
	RegEntryID regEntryID;						// RegEntryID describing Graphics HW
	InterruptSetMember interruptSetMember;		// The 'driver-ist'
	InterruptServiceInfo VBLInteruptService;
	InterruptServiceInfo HBLInteruptService;
	InterruptServiceInfo FrameInteruptService;
	InterruptServiceInfo ConnectInteruptService;
	void *vblRefCon;							// Address of HAL's refCon for interrupt functions
	VBLHandler *halVBLHandler;					// Address of HAL's vbl handler
	VBLEnabler *halVBLEnabler;					// Address of HAL's vbl enabler
	VBLDisabler *halVBLDisabler;				// Address of HAL's vbl disabler
	void *defaultRefCon;						// Default refCon
	InterruptHandler defaultVBLHandler;			// Default vbl handler
	InterruptEnabler defaultVBLEnabler;			// Default vbl enabler
	InterruptDisabler defaultVBLDisabler;		// Default vbl disabler
	Boolean hasInterruptSetMember;				// Found kISTPropertyName
	Boolean chainDefault;						// 'true' if HAL wants default enabler/disabler vbl 
												// routines to be called after HAL routines
	Boolean installedHALVBLRoutines;			// 'true' if successfully installed HAL VBL routines
};



// OSS internal prototypes

static GDXErr GraphicsOSSInitialize(const RegEntryID *regEntryID);
static OSSData *GraphicsOSSGetOSSData(void);
static GDXErr OSSMapPreference(OSSMapDirection mapDirection, GraphicsNonVolatile *graphicsNonVolatile,
		GraphicsPreferred *graphicsPreferred);
static InterruptMemberNumber GraphicsOSSVBLInterruptHandler(InterruptSetMember interruptSetMember,
		void *vblRefCon, UInt32 theIntCount);
static void GraphicsOSSVBLInterruptEnabler(InterruptSetMember interruptSetMember, void *vblRefCon);
static Boolean GraphicsOSSVBLInterruptDisabler(InterruptSetMember interruptSetMember, 
			void *vblRefCon);

OSSData gOSSData;
Boolean gIsForMacOSX = false;


//=====================================================================================================
//
// DoDriverIO()
//	This is the entry point for the native Graphics Driver.
//
//=====================================================================================================
#pragma export on
OSErr DoDriverIO (AddressSpaceID spaceID, IOCommandID ioCommandID, IOCommandContents ioCommandContents, IOCommandCode ioCommandCode,
		IOCommandKind ioCommandKind)

{
	OSErr err = noErr;				// Assume success.  If we get an ioCommandCode that is not
									// supported just return noErr.
									

	switch (ioCommandCode)
	{
		case kInitializeCommand:
			err = GraphicsOSSInitialize(&ioCommandContents.initialInfo->deviceEntry);
			err = GraphicsInitialize(ioCommandContents.initialInfo->refNum, &ioCommandContents.initialInfo->deviceEntry, spaceID);
			break;

		case kReplaceCommand:
			err = GraphicsOSSInitialize(&ioCommandContents.initialInfo->deviceEntry);
			err = GraphicsReplace(ioCommandContents.initialInfo->refNum, &ioCommandContents.initialInfo->deviceEntry, spaceID);
			break;

		case kOpenCommand:
			err = GraphicsOpen();
			break;
			
		case kCloseCommand:
			err = GraphicsClose();
			break;
			
		case kControlCommand:
			err = GraphicsControl((CntrlParam *) ioCommandContents.pb);
			break;
			
		case kStatusCommand:
			err = GraphicsStatus((CntrlParam *) ioCommandContents.pb);
			break;
		
		case kFinalizeCommand:
			GraphicsOSSKillPrivateData();
			err = GraphicsFinalize(ioCommandContents.finalInfo->refNum, &ioCommandContents.finalInfo->deviceEntry);
			break;

		case kSupersededCommand:
			GraphicsOSSKillPrivateData();
			err = GraphicsSupersede(ioCommandContents.finalInfo->refNum, &ioCommandContents.finalInfo->deviceEntry);
			break;

		case kReadCommand:				// We do not support 'Read' or 'Write' commands
		case kWriteCommand:				
			err = noErr;
			break;		

		default:						// an error
			err = controlErr;
	}
	
	// If an immediate command, return error result right now, otherwise let everything be processed
	// by IOCommandIsComplete()  (Initialize, Finalize... are immediate)
	
	if (kImmediateIOCommandKind == ioCommandKind)
		return err;
	else
		return IOCommandIsComplete( ioCommandID, err );

}
#pragma export reset



//=====================================================================================================
//
// GraphicsOSSInitialize()
//
//	The OSS saves the 'regEntryID'.  More importantly, the OSS looks for the kISTPropertyName property
//	and saves the the 'InterruptSetMember' pertaining to the 'kISTChipInterruptSource'
//
//	Note: OSS assumes that a driver's "driver-ist" only pertains to VBL's
//
//=====================================================================================================
static GDXErr GraphicsOSSInitialize(const RegEntryID *regEntryID)
{
	ISTProperty istProperty;
	InterruptSetMember oneMember;
	RegPropertyValueSize valueSize;
	
	OSSData *ossData = GraphicsOSSGetOSSData();

	GDXErr err = kGDXErrUnknownError;							// Assume failure
	
	ossData->hasInterruptSetMember = false;						// Don't yet have interrtuptSetMember
	ossData->VBLInteruptService.vslServiceIDValid = false;							// No VSLServiceID yet
	ossData->HBLInteruptService.vslServiceIDValid = false;							// No VSLServiceID yet
	ossData->FrameInteruptService.vslServiceIDValid = false;							// No VSLServiceID yet
	ossData->ConnectInteruptService.vslServiceIDValid = false;							// No VSLServiceID yet

	RegistryEntryIDCopy(regEntryID, &ossData->regEntryID);		// Save the regEntryID,

	// Find out if we are running on Mac OS X

	#ifdef _MSC_VER
		#pragma message("Test this in Mac OS 9 and Mac OS X")
	#else
		#warning "Test this in Mac OS 9 and Mac OS X"
	#endif
	gIsForMacOSX = noErr == RegistryPropertyGetSize(regEntryID, "AAPL,iokit-ndrv", &valueSize);

	// Retrieve the ISTProperty.
	// The ISTProperty is an array consisting of several InterruptSetMembers.
	// The only InterruptSetMember of interest is the 'kISTChipInterruptSource', since
	// the others relate to DMA, which is not applicable here.

	err = GraphicsOSSGetProperty(regEntryID, kISTPropertyName, istProperty, sizeof(ISTProperty));
	if (!err)
	{
		ossData->interruptSetMember = istProperty[kISTChipInterruptSource];
		ossData->hasInterruptSetMember = true;						// Have valid interrtuptSetMember
	}
	else
	{
		err = GraphicsOSSGetProperty(regEntryID, kISTPropertyName, &oneMember, sizeof(InterruptSetMember));
		if (!err)
		{
			ossData->interruptSetMember = oneMember;
			ossData->hasInterruptSetMember = true;						// Have valid interrtuptSetMember
		}
	}

ErrorExit:

	return err;
}	



//=====================================================================================================
//
// GraphicsOSSKillPrivateData()
//
//	Dispose of the OSS's private data, remove interrupt handlers, and remove driver from VSL queue
//
//=====================================================================================================
void GraphicsOSSKillPrivateData()
{
	OSSData *ossData = GraphicsOSSGetOSSData();

	RegistryEntryIDDispose(&ossData->regEntryID);
	// Disable the external interrupts before removing the interrupt functions
	
	if (ossData->installedHALVBLRoutines)
	{
		// If the HAL installed a disabler function call it.  Otherwise call the default enabler
		if (NULL != ossData->halVBLDisabler)
			(void) GraphicsOSSVBLInterruptDisabler(ossData->interruptSetMember, ossData->vblRefCon);
		else
			(void) GraphicsOSSVBLDefaultDisabler();
		
		// Interrupts are disabled, so reinstall the default enablers
		(void) InstallInterruptFunctions(ossData->interruptSetMember.setID, 
				ossData->interruptSetMember.member, ossData->defaultRefCon, 
				ossData->defaultVBLHandler, ossData->defaultVBLEnabler, ossData->defaultVBLDisabler);
	}
	
	if (ossData->VBLInteruptService.vslServiceIDValid)
	{
		(void) VSLDisposeInterruptService(ossData->VBLInteruptService.vslServiceID);
		ossData->VBLInteruptService.vslServiceIDValid = false;
	}
	
	if (ossData->HBLInteruptService.vslServiceIDValid)
	{
		(void) VSLDisposeInterruptService(ossData->HBLInteruptService.vslServiceID);
		ossData->HBLInteruptService.vslServiceIDValid = false;
	}
	
	if (ossData->FrameInteruptService.vslServiceIDValid)
	{
		(void) VSLDisposeInterruptService(ossData->FrameInteruptService.vslServiceID);
		ossData->FrameInteruptService.vslServiceIDValid = false;
	}
	
	if (ossData->ConnectInteruptService.vslServiceIDValid)
	{
		(void) VSLDisposeInterruptService(ossData->ConnectInteruptService.vslServiceID);
		ossData->ConnectInteruptService.vslServiceIDValid = false;
	}
	
}



//=====================================================================================================
//
// GraphicsOSSGetOSSData()
//
//	Return the pointer to the global OSS data.
//	(Yes...you guessed it...it just returns the address of the global.  However, use your
//	access functions in the event the way-coolness of CFM is ever lost.  You have been warned.)
//
//=====================================================================================================
static OSSData *GraphicsOSSGetOSSData(void)
{
	return (&gOSSData);
}



//=====================================================================================================
//
// GraphicsOSSSaveProperty()
//	The OSS calls the Name Registry to save information.  The OSS doesn't care about the content.
//
//			->	regEntryID			Name Registry RegEntryID that should have the propertyName
//			->	propertyName		c string property name
//			->	propertyValue		void * to buffer containing the data
//			->	propertySize		size of the data
//			->	ossPropertyStorage	GDX internal flags that describe how the property should get saved
//					kOSSPropertyAvailableAtBoot, available at boot (saved across boots)
//					kOSSPropertyAvailableAtDisk, available once disk is around (saved across boots)
//					kOSSPropertyVolatile, property not saved across boots
//
//=====================================================================================================
GDXErr GraphicsOSSSaveProperty(const RegEntryID *regEntryID, const char *propertyName,
		const void *propertyValue, ByteCount propertySize, OSSPropertyStorage ossPropertyStorage)
{

	RegPropertyModifiers modifiers;			// Set modifers based on ossPropertyStorage
											
	RegPropertyValueSize valueSize;			
	OSErr nameRegistryErr;					// In the event of NameRegistry errors
	UInt32 propertyNameLength;				// Make sure proprtyName is not too long
	
	RegModifiers savedToNVRAMAndDiskMask = (kRegPropertyValueIsSavedToNVRAM | 
											kRegPropertyValueIsSavedToDisk);	

	GDXErr err = kGDXErrUnknownError;								// Assume failure
	

	// Is the 'kRegMaximumPropertyNameLength < propertyName length'?
	propertyNameLength = CStrLen(propertyName);
	if (kRegMaximumPropertyNameLength < propertyNameLength)
	{
		err = kGDXErrOSSPropertyNameLengthTooLong;
		goto ErrorExit;
	}
	
	// Is the OSSPropertyStorage valid?
	if ( 	(kOSSPropertyAvailableAtBoot != ossPropertyStorage) &&
			(kOSSPropertyAvailableAtDisk != ossPropertyStorage) &&
			(kOSSPropertyVolatile != ossPropertyStorage) )
	{
		err = kGDXErrOSSPropertyStorageInvalid;
		goto ErrorExit;
	}
			
	// Try to get the size of the propertyName to make sure that it exists
	nameRegistryErr = RegistryPropertyGetSize(regEntryID, propertyName, &valueSize);
	if (nameRegistryErr)
	{
		// Create the property since it didn't exist
		nameRegistryErr = RegistryPropertyCreate(regEntryID, propertyName, propertyValue, propertySize);
		
		if (nameRegistryErr)
		{
			err = kGDXErrOSSUnableToCreateProperty;
			goto ErrorExit;	
		}
		
	}
	else
	{		
		// Property already exists so just set the property to the new value
		nameRegistryErr = RegistryPropertySet(regEntryID, propertyName, propertyValue, propertySize);
	
		if (nameRegistryErr)
		{
			err = kGDXErrOSSUnableToSetPropertyValue;
			goto ErrorExit;
		}
	}
	
	// The property exists and has been set to the correct value.  Get  the modifiers so the can
	// be altered appropriately.
	
	nameRegistryErr = RegistryPropertyGetMod(regEntryID, propertyName, &modifiers);

	if (nameRegistryErr)
	{
		err = kGDXErrOSSUnableToSavePropertyStorage;
		goto ErrorExit;
	}
	
	// Clear the bits corresponding to where to store property, in case they had a different
	// previous value.
	
	modifiers &= ~savedToNVRAMAndDiskMask;
	
	if (kOSSPropertyAvailableAtBoot == ossPropertyStorage)
		modifiers |= kRegPropertyValueIsSavedToNVRAM;				// Property saved to NVRAM
	else if (kOSSPropertyAvailableAtDisk == ossPropertyStorage)
		modifiers |=  kRegPropertyValueIsSavedToDisk;				// Property saved to disk

	nameRegistryErr = RegistryPropertySetMod(regEntryID, propertyName, modifiers);

	if (nameRegistryErr)
	{
		err = kGDXErrOSSUnableToSavePropertyStorage;
		goto ErrorExit;
	}

	err = kGDXErrNoError;
	
ErrorExit:

	return err;

}



//=====================================================================================================
//
// GraphicsOSSGetProperty()
//	The OSS calls the Name Registry to get information.  The OSS doesn't care about the content.
//	If the property doesn't exist...so it goes, that's an error
//
//			->	regEntryID			Name Registry RegEntryID that should have the propertyName
//			->	propertyName		c string property name
//			->	propertyValue		void* to buffer to hold the data
//			->	propertySize		expected size of the data
//
//=====================================================================================================
GDXErr GraphicsOSSGetProperty(const RegEntryID *regEntryID, const char *propertyName,
		void *propertyValue, ByteCount propertySize)
{

	RegPropertyValueSize valueSize;							// Actual value size of propertyName
	OSErr nameRegistryErr;									// In the event of NameRegistry errors
	
	GDXErr err = kGDXErrUnknownError;						// Assume failure
	
	// Does the PropertyName exist? Is the storage provided by the caller big enough to hold data?
	nameRegistryErr = RegistryPropertyGetSize(regEntryID, propertyName, &valueSize);
	if (nameRegistryErr)
	{
		err = kGDXErrOSSNoProperyNameAndValue;
		goto ErrorExit;
	}

	if (valueSize > propertySize)
	{
		err = kGDXErrOSSPropertySizeExceedsBuffer;
		goto ErrorExit;
	}

	// The propertyValue exists and can be held in the buffer 'propertyValue'
	nameRegistryErr = RegistryPropertyGet(regEntryID, propertyName, propertyValue, &valueSize);
	if (nameRegistryErr)
	{
		err = kGDXErrOSSUnableToGetPropertyValue;
		goto ErrorExit;
	}

	if (valueSize != propertySize)
	{
		err = kGDXErrOSSUnexpectedPropertySize;
		goto ErrorExit;
	}

	err = kGDXErrNoError;
	
ErrorExit:

	return err;

}



//=====================================================================================================
//
// GraphicsOSSDeleteProperty()
//	The OSS calls the System Registry to delete Properties.  It doesn't care what property
//	
//
//			->	regEntryID		the System Registry RegEntryID that should have the propertyName
//					sure, the OSS should already have this and the Core and HAL shouldn't need to know 
//					about the existance of a RegEntryID but since the OSS has no other data...
//					let it slide.
//								
//			->	propertyName		c string property name
//
//=====================================================================================================
GDXErr GraphicsOSSDeleteProperty(const RegEntryID *regEntryID, const char *propertyName)
{

//	RegPropertyValueSize valueSize;		// actual value size of propertyName
	OSErr nameRegistryErr;				// record System Registry Errors
	
	GDXErr err = kGDXErrUnknownError;								// Assume failure
	
	nameRegistryErr = RegistryPropertyDelete(regEntryID, propertyName);
	if (nameRegistryErr)
	{
		err = kGDXErrOSSNoProperyNameAndValue;
		goto ErrorExit;
	}


	err = kGDXErrNoError;
	
ErrorExit:

	return err;

}



//=====================================================================================================
//
// GraphicsOSSSetCorePref()
//	The OSS calls the System Registry to get information.  The "Core" preferences are saved in the
//	first 4 bytes of the "gprf" nvram property.  The data is saved in a compressed format.  Hence the
//	OSS will translate the grpf data into the GraphicsPreferred data that the core expects.  The
//	4 bytes reserved for the HAL's is ignored.
//	
//
//			->	regEntryID			the System Registry RegEntryID that should have the propertyName
//			->	graphicsPreferred	buffer to hold the GraphicsPreferred data
//								
//	For this routine, the relevant fields of the 'GraphicsPreferred' structure are as follows:
//
//  		->  displayModeID	should be the current displayModeID and will be used on next boot
//  		->  depthMode		should be the current depthMode and will be used on the next boot
//  		->	displayCode		the type of display attached.  If the display changes between boots
//								the displayModeID and the depthMode will be ignored.
//
//=====================================================================================================
GDXErr GraphicsOSSSetCorePref(const RegEntryID *regEntryID, GraphicsPreferred *graphicsPreferred)
{

	RegPropertyModifiers modifiers;			// set modifers based on ossPropertyStorage
	RegModifiers savedToNVRAMAndDiskMask =  (kRegPropertyValueIsSavedToNVRAM |
											kRegPropertyValueIsSavedToDisk);
											
	RegPropertyValueSize valueSize;		// actual value size of kPreferredConfigurationName property
	RegPropertyValueSize expectedValueSize = sizeof(GraphicsNonVolatile);
	GraphicsNonVolatile graphicsNonVolatile;		// actual data stored in nvram
	OSErr nameRegistryErr;				// record System Registry Errors
	
	GDXErr err = kGDXErrUnknownError;								// Assume failure

	err = OSSMapPreference(kOSSMapCompress, &graphicsNonVolatile, graphicsPreferred);
	if (err)
		goto ErrorExit;
		
	// Does the propertyName exists and is the storage provided by the caller big enough to hold data
	nameRegistryErr = RegistryPropertyGetSize(regEntryID, kPreferredConfigurationName, &valueSize);
	if (nameRegistryErr)
	{
	
		// Create the property since it didn't exist
//		graphicsNonVolatile.reserved = 0;	// not set in real driver
		graphicsNonVolatile.halData = 0;

		nameRegistryErr = RegistryPropertyCreate(regEntryID,kPreferredConfigurationName, 
			&graphicsNonVolatile,sizeof(GraphicsNonVolatile));

		if (nameRegistryErr)
		{
			err = kGDXErrOSSUnableToCreateProperty;
			goto ErrorExit;	
		}
		// The property exists and has been set to the correct value.  Now set the modifiers
		nameRegistryErr = RegistryPropertyGetMod(regEntryID, kPreferredConfigurationName, &modifiers);
	
		if (nameRegistryErr)
		{
			err = kGDXErrOSSUnableToSavePropertyStorage;
			goto ErrorExit;
		}
		
		modifiers &= ~savedToNVRAMAndDiskMask;					// Be paranoid and clear these bits
		modifiers |= kRegPropertyValueIsSavedToNVRAM;			// Set them to store in NVRAM
	
		nameRegistryErr = RegistryPropertySetMod(regEntryID, kPreferredConfigurationName, modifiers);
	
		if (nameRegistryErr)
		{
			err = kGDXErrOSSUnableToSavePropertyStorage;
			goto ErrorExit;
		}


	}
	else
	{
		// Property already exists so just set the property to the new value
		if (valueSize > expectedValueSize)
		{
			// Hmmmm...for some reason, the valueSize is bigger than expected.  Rather than returning
			// an error, kill  the old property and create a new property of the correct size
			
			graphicsNonVolatile.mappedDisplayModeID = 0;
			graphicsNonVolatile.mappedDepthMode = 0;
			graphicsNonVolatile.mappedDisplayCode = 0;
			graphicsNonVolatile.reserved = 0;
			graphicsNonVolatile.halData = 0;
			
			nameRegistryErr = RegistryPropertySet(regEntryID, kPreferredConfigurationName, 
				&graphicsNonVolatile,sizeof(GraphicsNonVolatile));
		
			if (nameRegistryErr)
			{
				err = kGDXErrOSSUnableToSetPropertyValue;
				goto ErrorExit;
			}
			
		// Continue on like nothing happend
		
		}
	
		// The propertyValue exists and can be held in the buffer 'graphicsNonVolatile'

		nameRegistryErr = RegistryPropertyGet(regEntryID, kPreferredConfigurationName,
				&graphicsNonVolatile, &valueSize);
	
		if (nameRegistryErr)
		{
			err = kGDXErrOSSUnableToGetPropertyValue;
			goto ErrorExit;
		}
		
		if (valueSize != expectedValueSize)
		{
			err = kGDXErrOSSUnexpectedPropertySize;
			goto ErrorExit;
		}
	
		// The 'kPreferredConfigurationName' property was found.  Translate the GraphicsPreferred 
		// data into the compressed format
		
		err = OSSMapPreference(kOSSMapCompress, &graphicsNonVolatile, graphicsPreferred);
		if (err)
			goto ErrorExit;
			
		nameRegistryErr = RegistryPropertySet(regEntryID, kPreferredConfigurationName, 
				&graphicsNonVolatile,sizeof(GraphicsNonVolatile));

		if (nameRegistryErr)
		{
			err = kGDXErrOSSUnableToSetPropertyValue;
			goto ErrorExit;
		}
	}

	err = kGDXErrNoError;
	
ErrorExit:

	return err;

}



//=====================================================================================================
//
// GraphicsOSSGetCorePref()
//	The OSS calls the Name Registry to get information.  The "Core" preferences are saved in the
//	first 4 bytes of the 'gprf' NVRAM property.  The data is saved in a compressed format.  Hence the
//	OSS will translate the 'grpf' data into the GraphicsPreferred data that the core expects.  The
//	4 bytes reserved for the HAL's is ignored.
//	
//
//			->	regEntryID			Name Registry RegEntryID that should have the propertyName
//			<-	graphicsPreferred	buffer to hold the GraphicsPreferred data
//								
//	For this routine, the relevant fields of the 'GraphicsPreferred' structure are as follows:
//
//  		<-  displayModeID	should be the current displayModeID and will be used on next boot
//  		<-  depthMode		should be the current depthMode and will be used on the next boot
//  		<-  displayCode		the type of display attached.  If the display changes between boots
//								the displayModeID and the depthMode will be ignored.
//
//=====================================================================================================
GDXErr GraphicsOSSGetCorePref(const RegEntryID *regEntryID, GraphicsPreferred *graphicsPreferred)
{

	RegPropertyValueSize valueSize;			// Value size of kPreferredConfigurationName property
	RegPropertyValueSize expectedValueSize = sizeof(GraphicsNonVolatile);
	GraphicsNonVolatile graphicsNonVolatile;				// Actual data stored in NVRAM
	OSErr nameRegistryErr;									// In the event of NameRegistry errors
	
	GDXErr err = kGDXErrUnknownError;						// Assume failure
	
	// Does the propertyName exist? Is the storage provided by the caller big enough to hold data?

	nameRegistryErr = RegistryPropertyGetSize(regEntryID, kPreferredConfigurationName, &valueSize);
	if (nameRegistryErr)
	{
		err = kGDXErrOSSNoProperyNameAndValue;
		goto ErrorExit;
	}

	if (valueSize > expectedValueSize)
	{
		err = kGDXErrOSSPropertySizeExceedsBuffer;
		goto ErrorExit;
	}

	// The propertyValue exists and can be held in the buffer "graphicsNonVolatile"
	nameRegistryErr = RegistryPropertyGet(regEntryID, kPreferredConfigurationName, &graphicsNonVolatile,
			&valueSize);
			
	if (nameRegistryErr)
	{
		err = kGDXErrOSSUnableToGetPropertyValue;
		goto ErrorExit;
	}
	
	if (valueSize != expectedValueSize)
	{
		err = kGDXErrOSSUnexpectedPropertySize;
		goto ErrorExit;
	}

	// The kPreferredConfigurationName property was found, so translate the compressed data into
	// the GraphicsPreferred format
	
	err = OSSMapPreference(kOSSMapExpand, &graphicsNonVolatile, graphicsPreferred);
	if (err)
		goto ErrorExit;
	
	err = kGDXErrNoError;
	
ErrorExit:

	return err;

}



//=====================================================================================================
//
// GraphicsOSSSetHALPref()
//	The OSS calls the Name Registry to get information.  The 'HAL' preferences are saved in the
//	last 4 bytes of the 'gprf' nvram property.  Unlike the SetCorePref, if the NVRAM data isn't the
//	expected size, the code bails without trying to modify the NVRAM structure
//
//			->	regEntryID			Name Registry RegEntryID that should have the propertyName
//			->	halData				4 bytes the HAL wishes to save
//								
//=====================================================================================================
GDXErr GraphicsOSSSetHALPref(const RegEntryID *regEntryID, UInt32 halData)
{

	RegPropertyValueSize valueSize;		// Value size of kPreferredConfigurationName property
	RegPropertyValueSize expectedValueSize = sizeof(GraphicsNonVolatile);
	GraphicsNonVolatile graphicsNonVolatile;				// Actual data stored in NVRAM
	OSErr nameRegistryErr;									// In the event of NameRegistry errors
	
	GDXErr err = kGDXErrUnknownError;						// Assume failure

		
	// Does the propertyName exist? Is the storage provided by the caller big enough to hold data?

	nameRegistryErr = RegistryPropertyGetSize(regEntryID, kPreferredConfigurationName, &valueSize);
	if (nameRegistryErr)
	{	
		err = kGDXErrOSSNoProperyNameAndValue;
		goto ErrorExit;
	}

	if (valueSize != expectedValueSize)
	{
		err = kGDXErrOSSUnexpectedPropertySize;
		goto ErrorExit;
	}

	// The propertyValue exists and can be held in the buffer 'graphicsNonVolatile'
	nameRegistryErr = RegistryPropertyGet(regEntryID, kPreferredConfigurationName,
			&graphicsNonVolatile, &valueSize);

	if (nameRegistryErr)
	{
		err = kGDXErrOSSUnableToGetPropertyValue;
		goto ErrorExit;
	}
	
	if (valueSize != expectedValueSize)
	{
		err = kGDXErrOSSUnexpectedPropertySize;
		goto ErrorExit;
	}

	// The kPreferredConfigurationName property was found, so set the HAL data

	graphicsNonVolatile.halData = halData;
	
		
	nameRegistryErr = RegistryPropertySet(regEntryID, kPreferredConfigurationName, 
		&graphicsNonVolatile,sizeof(GraphicsNonVolatile));

	if (nameRegistryErr)
	{
		err = kGDXErrOSSUnableToSetPropertyValue;
		goto ErrorExit;
	}

	err = kGDXErrNoError;
	
ErrorExit:

	return err;

}


	
//=====================================================================================================
//
// GraphicsOSSGetHALPref()
//	The OSS calls the Name Registry to get information.  The 'HAL' preferences are saved in the
//	last 4 bytes of the 'gprf' nvram property.  The OSS just passes the data back to the HAL.
//
//			->	regEntryID			Name Registry RegEntryID that should have the propertyName
//			<-	halData				buffer to hold the hal private data....data can be anything HAL
//									wants it to be
//								
//=====================================================================================================
GDXErr GraphicsOSSGetHALPref(const RegEntryID *regEntryID, UInt32 *halData)
{

	RegPropertyValueSize valueSize;				// Value size of kPreferredConfigurationName property
	RegPropertyValueSize expectedValueSize = sizeof(GraphicsNonVolatile);
	GraphicsNonVolatile graphicsNonVolatile;				// Actual data stored in nvram
	OSErr nameRegistryErr;									// In the event of NameRegistry errors
	
	GDXErr err = kGDXErrUnknownError;						// Assume failure
	
	// Does the propertyName exist? Is the storage provided by the caller big enough to hold data?
	
	nameRegistryErr = RegistryPropertyGetSize(regEntryID, kPreferredConfigurationName, &valueSize);
	if (nameRegistryErr)
	{
		err = kGDXErrOSSNoProperyNameAndValue;
		goto ErrorExit;
	}

	if (valueSize > expectedValueSize)
	{
		err = kGDXErrOSSPropertySizeExceedsBuffer;
		goto ErrorExit;
	}

	// The propertyValue exists and can be held in the buffer 'graphicsNonVolatile'
	nameRegistryErr = RegistryPropertyGet(regEntryID, kPreferredConfigurationName, &graphicsNonVolatile,
						&valueSize);

	if (nameRegistryErr)
	{
		err = kGDXErrOSSUnableToGetPropertyValue;
		goto ErrorExit;
	}
	
	if (valueSize != expectedValueSize)
	{
		err = kGDXErrOSSUnexpectedPropertySize;
		goto ErrorExit;
	}

	// The kPreferredConfigurationName property was found.  Copy the HAL data to halPreferred
	*halData = graphicsNonVolatile.halData;
		
	err = kGDXErrNoError;
	
ErrorExit:

	return err;

}



//=====================================================================================================
//
// OSSMapPreference()
//	Each device is allocated 8 bytes of NVRAM to store non volatile data in the NameRegistry.
//	Since the GDX driver wishes to store more than 8 bytes of data, it is necessary to map the actual
//	data into a format that fits into 8 bytes.  This routine maps in both direction based on the
//	mapDirection.
//
//	The mapping algorithim is as follows:
//	1) mappedDisplayModeID....Direct mapping occurs, since DisplayModeIDs currently have 1 byte 
//		of significance in the LSB, thus mapping can be avoided.
//	2) mappedDepthMode, 0 = kDepthMode1, 1 = kDepthMode2 etc..this is done by substracting kDepthMode1
//		from the depthMode when compressing the data.  kDepthMode1 is added to the mappedDepthMode when
//		the expanding the data
//	3) mappedDisplayCode....Direct mapping occurs.  DisplayCodes should not be greater than 255
//	
//
//			->	mapDirection			
//			'kOSSMapExpand' 	if GraphicsNonVolatile data is being converted into GraphicsPreferred
//			'kOSSMapCompress'	if GraphicsPreferred is being converted into GraphicsNonVolatile
//
//			<>	graphicsNonVolatile
//			if 'kOSSMapExpand', graphicsNonVolatile will be converted/stored in graphicsPreferred
//			if 'kOSSMapCompress', graphicsPreferred will be converted/storedin graphicsNonVolatile
//
//			<>	graphicsPreferred
//			if 'kOSSMapExpand', graphicsNonVolatile will be converted/stored in graphicsPreferred
//			if 'kOSSMapCompress', graphicsPreferred will be converted/stored in graphicsNonVolatile
//								
//=====================================================================================================
static GDXErr OSSMapPreference(OSSMapDirection mapDirection, GraphicsNonVolatile *graphicsNonVolatile,
		GraphicsPreferred *graphicsPreferred)
{

	GDXErr err = kGDXErrNoError;								// Assume success

	
	if (kOSSMapExpand == mapDirection)			
	{
		// Map GraphicsNonVolatile to GraphicsPreferred
		
		graphicsPreferred->ddcChecksum = graphicsNonVolatile->reserved;
		graphicsPreferred->displayModeID = graphicsNonVolatile->mappedDisplayModeID;
		graphicsPreferred->depthMode = graphicsNonVolatile->mappedDepthMode + kDepthMode1;
		graphicsPreferred->displayCode = graphicsNonVolatile->mappedDisplayCode;

	}
	else
	{										
		// Map GraphicsPreferred to GraphicsNonVolatile
		
		graphicsNonVolatile->reserved = graphicsPreferred->ddcChecksum;
		graphicsNonVolatile->mappedDisplayModeID = graphicsPreferred->displayModeID;
		graphicsNonVolatile->mappedDepthMode = graphicsPreferred->depthMode - kDepthMode1;
		graphicsNonVolatile->mappedDisplayCode = graphicsPreferred->displayCode;

	}
	
ErrorExit:
	
	return err;
}



//=====================================================================================================
//
// GraphicsOSSInstallVBLInterrupts()
//	This routine is specific for VBL interrupts and assumes that a driver's 'driver-ist' only
//	pertains to VBL's.  The Video Services Library (VSL) kHBLService and kFrameService are ignored.
//	Interrogate the HAL to get the interrupt handler, the interrupt enabler, the interrupt disabler,
//	and the VBL 'refCon' (private HAL data that the HAL might need to carry out the functions).
//	This version of GDX interrupt handling isolates the HAL from knowing stuff about how the 
//	handlers are installed.  Conventions interrupt routines must follow:
//		interrupt handler:		clear the hw interrupt source
//		interrupt enabler:		enable the hw interrupt source
//		interrupt disabler:		disable the hw interrupt source and :
//								return true if interrupts were enabled before the call
//								return false if interrupts were disabled before the call
//								
//	
//			->	regEntryID		Name Registry RegEntryID that should have the propertyName
//								
//=====================================================================================================
GDXErr GraphicsOSSInstallVBLInterrupts(const RegEntryID *regEntryID)
{
	#pragma unused( regEntryID )
	
	Boolean installVBLInterrupts = false;			// Ask HAL if OSS should install VBL routines
	InterruptEnabler enabler = NULL;				// Use default enabler if HAL's enabler is NULL
	InterruptDisabler disabler = NULL;				// Use default disabler if HAL's disabler is NULL
	
	OSSData *ossData = GraphicsOSSGetOSSData();
	
	GDXErr err = kGDXErrUnknownError;				// Assume failure
	
	
	err = GraphicsHALGetVBLInterruptRoutines(&installVBLInterrupts, &ossData->chainDefault,
			&ossData->halVBLHandler, &ossData->halVBLEnabler, &ossData->halVBLDisabler,
			&ossData->vblRefCon);
	
	if (err)
		goto ErrorExit;
		
	if (installVBLInterrupts)			// The OSS should handle the HAL's vbl interrupts
	{
		// It is possible that OSS should install the routines but doesn't have the InterruptSetMember
		if (ossData->hasInterruptSetMember)
		{
			OSStatus osStatusErr;
//			OSErr osErr;
			
			osStatusErr = GetInterruptFunctions(ossData->interruptSetMember.setID,
					ossData->interruptSetMember.member,
					&ossData->defaultRefCon,
					&ossData->defaultVBLHandler,
					&ossData->defaultVBLEnabler,
					&ossData->defaultVBLDisabler );
					
			if (osStatusErr)
			{
				err = kGDXErrOSSNoDefaultVBLRoutines;
				goto ErrorExit;
			}

			// If the HAL's enabler/disabler functions are NULL, that indicates that the HAL
			// can use the default enabler/disabler function.  Otherwise, the OSS enabler/disabler
			// functions will be installed.
			// NOTE:  Making a call to InstallInterruptFunctions() with NULL as the enabler/disabler,
			// than the enablers currently installed (the 'default enablers') are used.
			
			if (NULL != ossData->halVBLEnabler)
				enabler = GraphicsOSSVBLInterruptEnabler;
				
			if (NULL != ossData->halVBLDisabler)
				disabler = GraphicsOSSVBLInterruptDisabler;
				
			osStatusErr = InstallInterruptFunctions(	ossData->interruptSetMember.setID, 
														ossData->interruptSetMember.member, 
														ossData->vblRefCon,
														(InterruptHandler) GraphicsOSSVBLInterruptHandler, 
														enabler, 
														disabler);
			
			if (osStatusErr)
			{
				err = kGDXErrOSSUnableToInstallVBLRoutines;
				goto ErrorExit;
			}

			// Successfully installed the routines.
			ossData->installedHALVBLRoutines = true;
			
		}
		else
		{
			err = kGDXErrOSSNoISTProperty;
			goto ErrorExit;
		}
	
	}

ErrorExit:
	
	return err;
}



//=====================================================================================================
//
// GraphicsOSSNewInterruptService()
//	It is necessary to register with the Video Service Library to allow the VSL to run tasks in the
//	vbl queue when interrupts occur.  The service is installed at each Open and removed on each Close.
//	Both the interrupt handlers and the VSL must be installed before interrupt handling can occur.
//	
//=====================================================================================================
GDXErr GraphicsOSSNewInterruptService(InterruptServiceType theServiceType)
{
	OSSData *ossData = GraphicsOSSGetOSSData();
	OSErr osErr = noErr;
	InterruptServiceInfo *theInterruptService;

	GDXErr err = kGDXErrNoError;

	switch (theServiceType)
	{
		case kVBLInterruptServiceType:
			theInterruptService = &ossData->VBLInteruptService;
			break;
		case kHBLInterruptServiceType:
			theInterruptService = &ossData->HBLInteruptService;
			break;
		case kFrameInterruptServiceType:
			theInterruptService = &ossData->FrameInteruptService;
			break;
		case kFBConnectInterruptServiceType:
			theInterruptService = &ossData->ConnectInteruptService;
			break;
		default:
			err = kGDXErrOSSUnableToInstallVSLService;
	}
	if (err)
		goto ErrorExit;


	if (true == theInterruptService->vslServiceIDValid)			// VSL services already installed
	{
		err = kGDXErrNoError;
		goto ErrorExit;
	}

	// Try to register with the VideoService lib
	if ( ossData->regEntryID.contents[0] | ossData->regEntryID.contents[1] | ossData->regEntryID.contents[2] | ossData->regEntryID.contents[3] )
	{
		osErr = VSLNewInterruptService(&ossData->regEntryID, theServiceType, 
				&theInterruptService->vslServiceID);
		// Everything is working and VBLs should be able to run
		if ( !osErr )
			theInterruptService->vslServiceIDValid = true;						// Have a valid VSLServiceID
	}

	if (osErr)
	{
		err = kGDXErrOSSUnableToInstallVSLService;
		goto ErrorExit;
	}


	err = kGDXErrNoError;

ErrorExit:

	return err;

}



//=====================================================================================================
//
// GraphicsOSSDisposeInterruptService()
//	Each time the driver is closed, it is necessary to 'unregister' with the VSL so that tasks
//	in the VBL queue for the driver can be moved to the the 'system' VBL task queue.
//	NOTE!!! The interrupt handler DOES NOT CHECK to ensure that there is a service registered with the
//	VSL before calling VSLDoInterruptService.  That means that interrupts must be turned off before
//	this routine is called.  As a sanity check, the OSS will make sure interrupts are off.
//	
//=====================================================================================================
GDXErr GraphicsOSSDisposeInterruptService(InterruptServiceType theServiceType)
{

	VDFlagRecord flag;									// Check status of interrupts before disposing
	OSSData *ossData = GraphicsOSSGetOSSData();
	OSErr osErr;
	InterruptServiceInfo *theInterruptService;
	
	GDXErr err = kGDXErrUnknownError;					// Assume failure

	switch (theServiceType)
	{
		case kVBLInterruptServiceType:
			theInterruptService = &ossData->VBLInteruptService;
			break;
		case kHBLInterruptServiceType:
			theInterruptService = &ossData->HBLInteruptService;
			break;
		case kFrameInterruptServiceType:
			theInterruptService = &ossData->FrameInteruptService;
			break;
		case kFBConnectInterruptServiceType:
			theInterruptService = &ossData->ConnectInteruptService;
			break;
		default:
			err = kGDXErrOSSBadInterruptServiceType;
			goto ErrorExit;
	}


	if (false == theInterruptService->vslServiceIDValid)						// VSL services already disposed
	{
		err = kGDXErrNoError;
		goto ErrorExit;
	}



	if (theServiceType == kVBLInterruptServiceType)
	{
		// Make sure interrupts are disabled.  It would be bad to remove the VSL service and then get
		// an interrupt.
		
		err = GraphicsCoreGetInterrupt(&flag);

		if (0 == flag.csMode)			 
		{
			// Can't remove VSL service because interrupts are still occuring
			err = kGDXErrOSSInterruptSourceStillActive;
			goto ErrorExit;
		}
	}

	// Mark ID as invalid prior to disposal, in order to avoid the unlikely race condition that
	// a HAL which only uses software timer interrupts and didn't properly kill it is set off.
	
	theInterruptService->vslServiceIDValid = false;						

	osErr = VSLDisposeInterruptService(theInterruptService->vslServiceID);

	if (osErr)
	{
		err = kGDXErrOSSUnableToDisposeVSLService;
		theInterruptService->vslServiceIDValid = true;			// Mark ID as valid, since disposal unsuccessful
		goto ErrorExit;
	}

	err = kGDXErrNoError;

ErrorExit:

	return err;

}



//=====================================================================================================
//
// GraphicsOSSSetVBLInterrupt()
//	This routine is used to enable/disable VBL interrupts.  The OSS obtained all the information
// 	regarding how interrupts should be handled via the GraphicsHALGetVBLInterruptRoutines() call.
//	Therefore, it knows whether to call default enablers/disablers, or the ones provided by the HAL.
//
//		-> enableInterrupts		'true' if interrupts should be enabled, 'false' otherwise.
//
//		<- Boolean				If disabling interrupts, then this is 'true' if interrupts were
//								previously enabled, false otherwise.
//								It is UNDEFINED when enabling interrupts.
//
//=====================================================================================================
Boolean GraphicsOSSSetVBLInterrupt(Boolean enableInterrupts)
{
	OSSData *ossData = GraphicsOSSGetOSSData();
//	InterruptSourceState interruptSourceState;
	Boolean originallyEnabled;
	
	if (enableInterrupts)
	{
		if (NULL == ossData->halVBLEnabler)
			GraphicsOSSVBLDefaultEnabler();
		else
			GraphicsOSSVBLInterruptEnabler(ossData->interruptSetMember, ossData->vblRefCon);
			
	}
	else
	{
		if (NULL == ossData->halVBLDisabler)
			originallyEnabled = GraphicsOSSVBLDefaultDisabler();
		else
			originallyEnabled = GraphicsOSSVBLInterruptDisabler(ossData->interruptSetMember,
					ossData->vblRefCon);
	}
	

	return originallyEnabled;
}


//=====================================================================================================
//
// GraphicsOSSVBLInterruptHandler()
//
//	This routine is specific for VBL interrupts and assumes that a driver's "driver-ist" only
//	pertains to vbl's.
//	This routine is the entry point to for the HAL's vbl interrupt handler.  It assumes that
//	once the vbl interrupt source has been cleared by the HAL, it is okay to call the videoservice lib
//	to run tasks in the vbl queue.  It assume that it can return kIsrIsComplete to indicate there are
//	no more interrupt set members that care about the interrupt and that the interrupt processing is
//	finished.
//
//			->	interruptSetMember	interrupt occurred for this InterruptSetMember
//
//	For this routine, the relevant fields of the 'InterruptSetMember' structure are as follows:
//
//			-> setID
//			Interrupt occured in this set.  OSS assumes there is only one relevant set per driver, i.e,
//			OSS is only handling VBL interrupts
//
//			-> member
//			Member for which the interrupt occured.  The OS never looks at the member and since the OSS
//			assumes there is only 1 relvant member per driver, it is being ignored.
//
//			-> vblRefCon
//			The refCon that a HAL might need to handle interrupts.  The OS never looks at the refCon
//			and the OSS just passes it to the HAL
//
//			-> theIntCount			don't care
//
//=====================================================================================================
static InterruptMemberNumber GraphicsOSSVBLInterruptHandler(InterruptSetMember interruptSetMember,
			void *vblRefCon, UInt32 theIntCount)
{
	#pragma unused( interruptSetMember, theIntCount )
	
	OSSData *ossData = GraphicsOSSGetOSSData();

	if ( ossData->halVBLHandler )
		(*ossData->halVBLHandler)(vblRefCon);				// Call the HALs Handler

	(void) VSLDoInterruptService(ossData->VBLInteruptService.vslServiceID);

	return kIsrIsComplete;
}



void GraphicsOSSInterruptHandler(InterruptServiceType theServiceType)
{
	InterruptServiceInfo *theInterruptService;

	OSSData *ossData = GraphicsOSSGetOSSData();
	switch (theServiceType)
	{
		case kHBLInterruptServiceType:
			theInterruptService = &ossData->HBLInteruptService;
			break;
		case kFrameInterruptServiceType:
			theInterruptService = &ossData->FrameInteruptService;
			break;
		case kFBConnectInterruptServiceType:
			theInterruptService = &ossData->ConnectInteruptService;
			break;
		default:
			theInterruptService = &ossData->VBLInteruptService;
	}

	if (theInterruptService->vslServiceIDValid)
		(void) VSLDoInterruptService(theInterruptService->vslServiceID);
}


//=====================================================================================================
//
// GraphicsOSSVBLInterruptEnabler()
//
//	This routine is specific for VBL interrupts and assumes that a driver's 'driver-ist' only
//	pertains to VBL's.
//	This routine is the entry point to for the HAL's VBL interrupt enabler.  It just calls the
//	HAL's VBL interrupt enabler.  If the HAL's vbl interrupt enabler was NULL, this routine
//	is not installed since the default enabler and disabler routines are used.
//
//			->	interruptSetMember	interrupt occurred for this InterruptSetMember
//
//	For this routine, the relevant fields of the 'InterruptSetMember' structure are as follows:
//
//			-> setID
//			Disable the interrupt source for set.  OSS assumes there is only one relevant set per 
//			driver, i.e., the OSS is only handling VBL interrupts.
//
//			-> member
//			Member for which the interrupt occured.  The OS never looks at the member and since the OSS
//			assumes there is only 1 relvant member per driver, it is being ignored.
//
//			-> vblRefCon
//			The refCon that a HAL might need to handle interrupts.  The OS never looks at the refCon
//			and the OSS just passes it to the HAL.
//
//=====================================================================================================
static void GraphicsOSSVBLInterruptEnabler(InterruptSetMember interruptSetMember, void *vblRefCon)
{
	#pragma unused( interruptSetMember )
	OSSData *ossData = GraphicsOSSGetOSSData();
	if ( ossData->halVBLEnabler )
		(*ossData->halVBLEnabler)(vblRefCon);					// HAL VBL enabler

	// Call default enabler if HAL instructed the OSS to 'chain' the defaults
	if (ossData->chainDefault)								
		GraphicsOSSVBLDefaultEnabler();	
}



//=====================================================================================================
//
// GraphicsOSSVBLInterruptDisabler()
//
//	This routine is specific for VBL interrupts and assumes that a driver's "driver-ist" only
//	pertains to vbl's.  If the HAL's vbl interrupt enabler was NULL, this routine is not installed
//	since the default enabler and disabler routines are used.
//
//	This routine is the entry point to for the HAL's vbl interrupt disabler.  It just calls the
//	HAL's vbl interrupt enabler.  The HAL should:
//								return true if interrupts were enabled before the call
//								return false if interrupts were disabled before the call
//
//			->	interruptSetMember	interrupt occurred for this InterruptSetMember
//
//	For this routine, the relevant fields of the 'InterruptSetMember' structure are as follows:
//
//			-> setID
//			Disable the interrupt source for set.  OSS assumes there is only one relevant set per 
//			driver, i.e., the OSS is only handling VBL interrupts.
//
//			-> member
//			Member for which the interrupt occured.  The OS never looks at the member and since the OSS
//			assumes there is only 1 relvant member per driver, it is being ignored.
//
//			-> vblRefCon
//			The refCon that a HAL might need to handle interrupts.  The OS never looks at the refCon
//			and the OSS just passes it to the HAL.
//
//=====================================================================================================
static Boolean GraphicsOSSVBLInterruptDisabler(InterruptSetMember interruptSetMember, 
			void *vblRefCon)
{
	#pragma unused( interruptSetMember )
	
	OSSData *ossData = GraphicsOSSGetOSSData();
	InterruptSourceState interruptSourceState = kSourceWasDisabled;
	Boolean originallyEnabled;

	if ( ossData->halVBLDisabler )
		interruptSourceState =  (*ossData->halVBLDisabler)(vblRefCon);

	if (ossData->chainDefault)									// Call default disabler if HAl didn't
		interruptSourceState = GraphicsOSSVBLDefaultDisabler();	// disable the external interrupt source
	
	if (kSourceWasEnabled == interruptSourceState)
		originallyEnabled = true;
	else
		originallyEnabled = false;

	return originallyEnabled;
}



//=====================================================================================================
//
// GraphicsOSSVBLDefaultEnabler()
//	This routine is specific for VBL interrupts and assumes that a driver's 'driver-ist' only
//	pertains to VBLs.  If the HAL doesn't know about the external interrupt source, it can
//	call this routine after hitting its own hardware to reenable interrupts.
//
//=====================================================================================================
void GraphicsOSSVBLDefaultEnabler(void)
{
	OSSData *ossData = GraphicsOSSGetOSSData();
	(*ossData->defaultVBLEnabler)(ossData->interruptSetMember, NULL);
}



//=====================================================================================================
//
// GraphicsOSSVBLDefaultDisabler()
//
//	This routine is specific for VBL interrupts and assumes that a driver's 'driver-ist' only
//	pertains to VBLs.  If the HAL doesn't know about the external interrupt source, it can
//	call this routine before hitting hardware to disable interrupts.
//
//	returns		'true' if the external interrupts was originally enabled, 'false' otherwise.
//
//=====================================================================================================
Boolean GraphicsOSSVBLDefaultDisabler(void)
{
	OSSData *ossData = GraphicsOSSGetOSSData();
	InterruptSourceState interruptSourceState = kSourceWasDisabled;
	Boolean originallyEnabled;

	if ( ossData->defaultVBLDisabler )
		interruptSourceState =  (*ossData->defaultVBLDisabler)(ossData->interruptSetMember,nil);

	if (kSourceWasEnabled == interruptSourceState)
		originallyEnabled = true;
	else
		originallyEnabled = false;

	return originallyEnabled;
}

//=====================================================================================================
//
// GraphicsOSSDoVSLInterruptService()
//
//	This routine simply calls the VSL to service the VBL tasks associated with this graphics device.
//
//	Normally, a HAL implementaion would NEVER have to call this routine.  However, in the rare event
//	that the HAL's hardware does not support true hardware interrupts, then the HAL should call this
//	during its 'simulated VBL' routine to give allow the OS to serice items in its VBL task queue.
//
//=====================================================================================================
void GraphicsOSSDoVSLInterruptService(void)
{
	
	OSSData *ossData = GraphicsOSSGetOSSData();

	if (ossData->VBLInteruptService.vslServiceIDValid)
		(void) VSLDoInterruptService(ossData->VBLInteruptService.vslServiceID);
}


