#ifndef __GRAPHICSHALDINGUSVIDEOPCI__
#define __GRAPHICSHALDINGUSVIDEOPCI__


#include <Video.h>


#ifdef __cplusplus
extern "C" {
#endif


enum
{
	kDepthMode1Index,
	kDepthMode2Index,
	kDepthMode3Index,
	kDepthMode4Index,
	kDepthMode5Index,
	kDepthMode6Index,
	kDepthModeNumIndexes
};

enum
{
	N = false,
	P = true
};

//
// BitDepthIndependentData
//	For each DisplayModeID, certain parameters are constant, regardless of the
//	bit depth. This structure is used for those parameters.
//


typedef struct BitDepthIndependentData BitDepthIndependentData;
struct BitDepthIndependentData
{
	UInt32 pixelClock;

	UInt32 hActive;
	UInt32 hSyncBegin;
	UInt32 hSyncEnd;
	UInt32 hTotal;

	UInt32 vActive;
	UInt32 vSyncBegin;
	UInt32 vSyncEnd;
	UInt32 vTotal;

	Boolean hSyncPolarity;
	Boolean vSyncPolarity;
	Boolean interlaced;
	Boolean cSyncDisable;
};



//
// WidthAndDepthDependentData
//	For DisplayModeID, certain parameters change according to the bit depth.
//	This structure is used for those parameters.
//
typedef struct WidthAndDepthDependentData WidthAndDepthDependentData;
struct WidthAndDepthDependentData
{
	UInt16 pixelDepth;
	UInt16 pages;
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
	DepthMode maxDepthMode;
	UInt32 timingData;
	UInt16 width;
	UInt16 height;
};



typedef struct DisplayModeInfo DisplayModeInfo;
struct DisplayModeInfo
{
	DisplayInfo info;
	BitDepthIndependentData bdiData;
	WidthAndDepthDependentData vwdData[kDepthModeNumIndexes];
};


extern DisplayModeInfo	gDisplayModeInfo[];

#ifdef __cplusplus
}
#endif

#endif	// __GRAPHICSHALDINGUSVIDEOPCI__
