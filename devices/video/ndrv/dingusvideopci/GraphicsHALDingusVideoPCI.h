#ifndef __GRAPHICSHALTEMPLATE__
#define __GRAPHICSHALTEMPLATE__


#include <Video.h>


#ifdef __cplusplus
extern "C" {
#endif


enum								// index for the maxDepthMode for a displayModeID given available 
{									// VRAM
	k2MegVRAMIndex,					// maxDepthMode when: 2 meg of VRAM
	k4MegVRAMIndex,					// maxDepthMode when: 4 meg of VRAM
	kVRAMNumIndexes
};

enum
{
	kDepthMode1Index,
	kDepthMode2Index,
	kDepthMode3Index,
	kDepthModeNumIndexes
};

enum
{
	vw32Index = 0,
	vw64Index = 1
};


enum
{
	kvw32d8Index = 0,
	kvw32d16Index = 1,
	kvw32d32Index = 2,
	kvw64d8Index = 3,
	kvw64d16Index = 4,
	kvw64d32Index = 5
};

/*
// The TemplateGetXXX functions contain the programming information for all registers based on the
// 1) the amount of VRAM 2) VRAM Width.  When information about a DisplayModeID, is desired
// the caller calls TemplateMapToIndicies which returns the proper index for each type of data based on the
// inputs.  The indices are then passed into the TemplateGetXXX function and the proper data is returned.
typedef struct InfoIndicies InfoIndicies;
struct InfoIndicies
{	
	UInt16 dIndex;
	UInt16 vwIndex;
	UInt16 vwdIndex;
	UInt16 maxDepthIndex;
};
*/



//
// BitDepthIndependentData
//	For each DisplayModeID, certain parameters are constant, regardless of the
//	bit depth.  This structure is used for those parameters.
//


typedef struct BitDepthIndependentData BitDepthIndependentData;
struct BitDepthIndependentData
{
	// ATHENS
	UInt8		N2;
	UInt8		D2;
	UInt8		P2Mux;
	UInt8		notInterlaced;
	// CONTROL
	UInt16		interlaced;
	UInt16		ControlTEST_4;
	UInt16		hSyncPolarity;
	UInt16		vSyncPolarity;
	UInt16		cSyncDisable;

	UInt16		horizontalSerration;			// ControlHSERR		// Horizontal MrSanAntonio parameters (bpp invariant)	
	UInt16		halfLine;						// ControlHLFLN 	//      "        "        "           "	
	UInt16		horizontalEqualization;			// ControlHEQ   	//      "        "        "           "	
	UInt16		horizontalSyncPulse;			// ControlHSP   	//      "        "        "           "	
	UInt16		horizontalBreezeway;			// ControlHBWAY 	//      "        "        "           "	
	UInt16		horizontalActiveLine;			// ControlHAL   	//      "        "        "           "	
	UInt16		horizontalFrontPorch;			// ControlHFP   	//      "        "        "           "	
	UInt16		horiztonalPixelCount;			// ControlHPIX  	//      "        "        "           "	
																	
	UInt16		verticalHalfLine;				// ControlVHLINE	// Vertical MrSanAntonio parameters (bpp invariant)
	UInt16		verticalSync;					// ControlVSYNC 	//    "       "       "              "	
	UInt16		verticalBackPorchEqualization;	// ControlVBPEQ 	//    "       "       "              "	
	UInt16		verticalBackPorch;				// ControlVBP   	//    "       "       "              "	
	UInt16		verticalActiveLine;				// ControlVAL   	//    "       "       "              "	
	UInt16		verticalFrontPorch;				// ControlVFP   	//    "       "       "              "	
	UInt16		verticalFrontPorchEqualization;	// ControlVFPEQ 	//    "       "       "              "	

	UInt32		nsCLUTAddrRegDelay;

/*
√	UInt32 horizontalSerration;				// Horizontal MrSanAntonio parameters (bpp invariant)
√	UInt32 halfLine;						//      "        "        "           "
√	UInt32 horizontalEqualization;			//      "        "        "           "
√	UInt32 horizontalSyncPulse;				//      "        "        "           "
√	UInt32 horizontalBreezeway;				//      "        "        "           "
	UInt32 horizontalBurstGate;				//      "        "        "           "
	UInt32 horizontalBackPorch;				//      "        "        "           "
√	UInt32 horizontalActiveLine;			//      "        "        "           "
√	UInt32 horizontalFrontPorch;			//      "        "        "           "
√	UInt32 horiztonalPixelCount;			//      "        "        "           "

	UInt16 baseAddress;						// FB base Address of VRAM.  hw cursor starts here

	UInt8 frankM;							// PLL inside Spur
	UInt8 frankPN;							// Parameters P = 3 ms bits, N = 5 ls bits
	UInt8 frankRI;							// Parameters R = 4 ms bits, I = 4 ls bits
	
	UInt8 vhSyncPolarity;					// 0x0c if active high, 0 if active low
*/
};



//
// WidthAndDepthDependentData
//	For DisplayModeID, certain parameters change according to the bit depth.
//	This structure is used for those parameters.
//
typedef struct WidthAndDepthDependentData WidthAndDepthDependentData;
struct WidthAndDepthDependentData
{
	UInt8 clockConfiguration;				// FB controller parameter; ControlGSC_DIVIDE
	UInt8 timingAdjust;						// Timing Adjust register = adj1, adj2 and pipeDelay; ControlPIPED
	UInt8 spurControl;						// CLUT parameter; low order 2 bits of spur register kSpurControl
};


//
// DisplayInfo
//	This structure will describe the capabilities of the frame buffer given the configuration of the
//	system.  
//
typedef struct DisplayInfo DisplayInfo;
struct DisplayInfo
{
	UInt32 displayModeSeed;
	UInt32 displayModeState;
	UInt32 dinfo_displayModeID;
	UInt32 dinfo_displayModeAlias;
	UInt32 timingData;
	Fixed refreshRate;
	DepthMode maxDepthMode[kVRAMNumIndexes];
	SInt16 width;
	SInt16 height;
};



typedef struct DisplayModeInfo DisplayModeInfo;
struct DisplayModeInfo
{
	DisplayInfo info;
	BitDepthIndependentData bdiData;
	WidthAndDepthDependentData vwdData[kVRAMNumIndexes][kDepthModeNumIndexes];
	UInt16 filler[3];	
};


extern DisplayModeInfo	gDisplayModeInfo[];

#ifdef __cplusplus
}
#endif

#endif	// __GRAPHICSHALTEMPLATE__
