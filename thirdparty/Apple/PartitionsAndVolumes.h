/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 The DingusPPC Development Team
          (See CREDITS.MD for more details)

(You may also contact divingkxt or powermax2286 on Discord)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/*
   This header contains types and constants defined in "File Manager
   Reference" and "Finder Interface Reference" both from 02/01/2003.

   These definitions can be found in HFSVolumes.h (or hfs_format.h) and
   AppleDiskPartitions.h as part of Mac OS X installed framework
   headers.

   Modifications by joevt for DingusPPC 08/19/2026
   Original license, description, and change history follows:
*/

/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */
/*
    File:       HFSVolumes.h

    Contains:   On-disk data structures for HFS and HFS Plus volumes.

    Version:    Mac OS 8.1

    Copyright:  © 1984-1999 by Apple Computer, Inc.  All rights reserved.

    File Ownership:

        DRI:               Mark Day

        Other Contacts:    Deric Horn

        Technology:        File Systems

    Writers:

        (ngk)   Nick Kledzik
        (gap)   george puckett
        (djb)   Don Brady
        (msd)   Mark Day
        (DSH)   Deric Horn

    Change History (most recent first):

    From HFSVolumes.i:

      <MOSXS>    5/28/99    DJB     Added on-disk B-tree structures.
      <MOSXS>     4/9/98    djb     Use a different kHFSPlusMountVersion for MacOS X.

         <2>     2/12/98    ngk     Clean up: add version field in this header, change Types.i to
                                    MacTypes.i, remove pragma comments, remove unnecesarry typedef
                                    struct Foo Foo statements.
         <1>      2/9/98    msd     first checked in
         <0>      2/5/98    msd     Imported from HFSVolumesPriv.i.  Added "HFS" to many names.
                                    Added reserved field to HFSPlusAttrInlineData.

    From HFSVolumesPriv.i:

      <CS19>    11/16/97    djb     Bump volume format version for Unicode changes.
      <CS18>    10/23/97    msd     Bug 1685113. Change kHFSPlusMountVersion to '8.10' to trigger
                                    MountCheck; it also happens to be a more correct value.
      <CS17>    10/19/97    msd     Bug 1684586. Rename modifyDate to attributeModDate. GetCatInfo
                                    and SetCatInfo use only contentModDate.
      <CS16>     9/19/97    msd     Fix bug 1679778. In the LargeCatalogFile structure, change the
                                    name of the linkCount field to "reserved1" since we no longer
                                    support hard links.
      <CS15>     9/10/97    msd     Make sure fork and extent information is 8-byte aligned in
                                    attribute records.
      <CS14>      9/4/97    msd     Attributes are now identified by a single Unicode string (flags
                                    and OSTypes are gone), and may have extents (allowing them to be
                                    very large). Removed C define of bitmapFile. Removed
                                    kVolumeHardLinksBit. Added field lastMountedVersion to
                                    VolumeHeader; moved unused space near start of structure to make
                                    following fields nicely aligned. LargeExtentKey's keyLength is
                                    now a UInt16, like all the rest of the HFS Plus B-Trees. Change
                                    kHFSPlusVersion to $0003.
      <CS13>      9/2/97    DSH     Moved the VolumeHeader to sector 2 of HFS+ partition, bumped
                                    kHFSPlusVersion to 0X0002.
      <CS12>      8/5/97    msd     Change kHFSPlusSigWord to $482B ('H+'). Add enum for
                                    kHFSPlusVersion (set to $0001).
      <CS11>     6/24/97    djb     Add link count to HFS Plus file record. Add kVolumeHardLinksBit.
      <CS10>     6/20/97    msd     Remove the define for contentModDate.
       <CS9>     6/12/97    djb     Removed rootFileCount and rootFolderCount. Added new dates,
                                    permissions, and encodingBitmap, changed signature to "D8".
       <CS8>      6/6/97    djb     Removed enum names and #pragma enumsalwaysint.
       <CS7>      6/4/97    gap     Removed ForBride conditional. Caused problem in
                                    HFSVolumesPriv.a.
       <CS6>      6/4/97    gap     Add #pragma enumsalwaysint so file will work with latest
                                    Interfacer tool in Bride build.
       <CS5>     5/30/97    djb     Add constants for minimum key lengths.
       <CS4>     5/28/97    msd     Add AttributeKey and associated enums.
       <CS3>     5/20/97    DSH     Keep sigWord in sync with last build, D5.
       <CS2>     5/15/97    msd     Key length and fork type for extent records should be unsigned.
       <CS1>     4/28/97    djb     first checked in

      <HFS7>      4/7/97    msd     Add FileID for attributes BTree.
      <HFS6>      4/4/97    djb     Major update to HFS Plus structures.
      <HFS5>     3/17/97    DSH     Adding kNumHFSPlusExtents, and kNumHFSExtents.
      <HFS4>     2/19/97    djb     Latest HFS Plus format changes. Removed B-Tree Manager data
                                    structures since they're defined in BTreesPrivate.h.
      <HFS3>     2/13/97    msd     Add unions for extent key and data.
      <HFS2>     1/16/97    djb     Changed kLargeExtentKeyMaximumLength to 11.
      <HFS1>      1/9/97    djb     first checked in
         <0>      1/7/97    djb     Converted from HFSVolumePriv.h file.
*/

#ifndef __PARTITONSANDVOLUME__
#define __PARTITONSANDVOLUME__

#include <cinttypes>

#pragma pack(push, 1)

// =====================================================================
// Finder Interface Reference

typedef unsigned long FourCharCode;
typedef FourCharCode OSType;

typedef uint16_t UniChar;

struct Rect {
    int16_t top;
    int16_t left;
    int16_t bottom;
    int16_t right;
};
typedef struct Rect Rect;
struct Point {
    int16_t v;
    int16_t h;
};
typedef struct Point Point;

#define FOUR_CHAR_CODE(x) (x)

struct HFSUniStr255 {
    uint16_t length;
    UniChar  unicode[255];
};
typedef struct HFSUniStr255 HFSUniStr255;

typedef const HFSUniStr255 *ConstHFSUniStr255Param;

typedef char Str31[32];
typedef char Str27[28];

struct FInfo {
    OSType   dType;
    OSType   dCreator;
    uint16_t dFlags;
    Point    dLocation;
    int16_t  dFldr;
};
typedef struct FInfo FInfo;

struct FXInfo {
    int16_t fdIconID;
    int16_t fdReserved[3];
    int8_t  fdScript;
    int8_t  fdXFlags;
    int16_t fdComment;
    int32_t fdPutAway;
};
typedef struct FXInfo FXInfo;

struct DInfo {
    Rect     frRect;
    uint16_t frFlags;
    Point    frLocation;
    int16_t  frView;
};
typedef struct DInfo DInfo;

struct DXInfo {
    Point   frScroll;
    int32_t frOpenChain;
    int8_t  frScript;
    int8_t  frXFlags;
    int16_t frComment;
    int32_t frPutAway;
};
typedef struct DXInfo DXInfo;

// =====================================================================
// File Manager Reference
// HFSVolumes.h or hfs_format.h

enum {
    kMFSSigWord          = 0xD2D7,
    kHFSSigWord          = 0x4244,
    kHFSPlusSigWord      = 0x482B,
    kHFSXSigWord         = 0x4858,
    kHFSPlusVersion      = 0x0004,
    kHFSXVersion         = 0x0005,
#if 1 // TARGET_API_MACOS_X
    kHFSPlusMountVersion = FOUR_CHAR_CODE('8.10')
#else
    kHFSPlusMountVersion = FOUR_CHAR_CODE('9.00')
#endif
};

typedef uint32_t HFSCatalogNodeID;

enum {
    kHFSMaxVolumeNameChars   = 27,
    kHFSMaxFileNameChars     = 31,
    kHFSPlusMaxFileNameChars = 255
};

struct HFSExtentKey {
    uint8_t          keyLength;
    uint8_t          forkType;
    HFSCatalogNodeID fileID;
    uint16_t         startBlock;
};
typedef struct HFSExtentKey HFSExtentKey;

struct HFSPlusExtentKey {
    uint16_t         keyLength;
    uint8_t          forkType;
    uint8_t          pad;
    HFSCatalogNodeID fileID;
    uint32_t         startBlock;
};
typedef struct HFSPlusExtentKey HFSPlusExtentKey;

enum {
    kHFSExtentDensity     = 3,
    kHFSPlusExtentDensity = 8
};

struct HFSExtentDescriptor {
    uint16_t startBlock;
    uint16_t blockCount;
};
typedef struct HFSExtentDescriptor HFSExtentDescriptor;

struct HFSPlusExtentDescriptor {
    uint32_t startBlock;
    uint32_t blockCount;
};
typedef struct HFSPlusExtentDescriptor HFSPlusExtentDescriptor;

typedef HFSExtentDescriptor HFSExtentRecord[3];

typedef HFSPlusExtentDescriptor HFSPlusExtentRecord[8];

struct HFSPlusForkData {
    uint64_t            logicalSize;
    uint32_t            clumpSize;
    uint32_t            totalBlocks;
    HFSPlusExtentRecord extents;
};
typedef struct HFSPlusForkData HFSPlusForkData;

struct HFSPlusPermissions {
    uint32_t ownerID;
    uint32_t groupID;
    uint32_t permissions;
    uint32_t specialDevice;
};
typedef struct HFSPlusPermissions HFSPlusPermissions;

enum {
    kHFSRootParentID           = 1,
    kHFSRootFolderID           = 2,
    kHFSExtentsFileID          = 3,
    kHFSCatalogFileID          = 4,
    kHFSBadBlockFileID         = 5,
    kHFSAllocationFileID       = 6,
    kHFSStartupFileID          = 7,
    kHFSAttributesFileID       = 8,
    kHFSBogusExtentFileID      = 15,
    kHFSFirstUserCatalogNodeID = 16
};

struct HFSCatalogKey {
    uint8_t          keyLength;
    uint8_t          reserved;
    HFSCatalogNodeID parentID;
    Str31            nodeName;
};
typedef struct HFSCatalogKey HFSCatalogKey;

struct HFSPlusCatalogKey {
    uint16_t         keyLength;
    HFSCatalogNodeID parentID;
    HFSUniStr255     nodeName;
};
typedef struct HFSPlusCatalogKey HFSPlusCatalogKey;

enum {
    kHFSFolderRecord           = 0x0100,
    kHFSFileRecord             = 0x0200,
    kHFSFolderThreadRecord     = 0x0300,
    kHFSFileThreadRecord       = 0x0400,
    kHFSPlusFolderRecord       = 1,
    kHFSPlusFileRecord         = 2,
    kHFSPlusFolderThreadRecord = 3,
    kHFSPlusFileThreadRecord   = 4
};

enum {
    kHFSFileLockedBit    = 0x0000,
    kHFSFileLockedMask   = 0x0001,
    kHFSThreadExistsBit  = 0x0001,
    kHFSThreadExistsMask = 0x0002
};

struct HFSCatalogFolder {
    int16_t          recordType;
    uint16_t         flags;
    uint16_t         valence;
    HFSCatalogNodeID folderID;
    uint32_t         createDate;
    uint32_t         modifyDate;
    uint32_t         backupDate;
    DInfo            userInfo;
    DXInfo           finderInfo;
    uint32_t         reserved[4];
};
typedef struct HFSCatalogFolder HFSCatalogFolder;

struct HFSPlusCatalogFolder {
    int16_t            recordType;
    uint16_t           flags;
    uint32_t           valence;
    HFSCatalogNodeID   folderID;
    uint32_t           createDate;
    uint32_t           contentModDate;
    uint32_t           attributeModDate;
    uint32_t           accessDate;
    uint32_t           backupDate;
    HFSPlusPermissions permissions;
    DInfo              userInfo;
    DXInfo             finderInfo;
    uint32_t           textEncoding;
    uint32_t           reserved;
};
typedef struct HFSPlusCatalogFolder HFSPlusCatalogFolder;

struct HFSCatalogFile {
    int16_t          recordType;
    uint8_t          flags;
    int8_t           fileType;
    FInfo            userInfo;
    HFSCatalogNodeID fileID;
    uint16_t         dataStartBlock;
    int32_t          dataLogicalSize;
    int32_t          dataPhysicalSize;
    uint16_t         rsrcStartBlock;
    int32_t          rsrcLogicalSize;
    int32_t          rsrcPhysicalSize;
    uint32_t         createDate;
    uint32_t         modifyDate;
    uint32_t         backupDate;
    FXInfo           finderInfo;
    uint16_t         clumpSize;
    HFSExtentRecord  dataExtents;
    HFSExtentRecord  rsrcExtents;
    uint32_t         reserved;
};
typedef struct HFSCatalogFile HFSCatalogFile;

struct HFSPlusCatalogFile {
    int16_t            recordType;
    uint16_t           flags;
    uint32_t           linkCount;
    HFSCatalogNodeID   fileID;
    uint32_t           createDate;
    uint32_t           contentModDate;
    uint32_t           attributeModDate;
    uint32_t           accessDate;
    uint32_t           backupDate;
    HFSPlusPermissions permissions;
    FInfo              userInfo;
    FXInfo             finderInfo;
    uint32_t           textEncoding;
    uint32_t           reserved2;
    HFSPlusForkData    dataFork;
    HFSPlusForkData    resourceFork;
};
typedef struct HFSPlusCatalogFile HFSPlusCatalogFile;

struct HFSCatalogThread {
    int16_t          recordType;
    int32_t          reserved[2];
    HFSCatalogNodeID parentID;
    Str31            nodeName;
};
typedef struct HFSCatalogThread HFSCatalogThread;

struct HFSPlusCatalogThread {
    int16_t          recordType;
    int16_t          reserved;
    HFSCatalogNodeID parentID;
    HFSUniStr255     nodeName;
};
typedef struct HFSPlusCatalogThread HFSPlusCatalogThread;

enum {
    kHFSPlusAttrInlineData = 0x10,
    kHFSPlusAttrForkData   = 0x20,
    kHFSPlusAttrExtents    = 0x30
};

struct HFSPlusAttrInlineData {
    uint32_t recordType;
    uint32_t reserved;
    uint32_t logicalSize;
    uint8_t  userData[2];
};
typedef struct HFSPlusAttrInlineData HFSPlusAttrInlineData;

struct HFSPlusAttrForkData {
    uint32_t        recordType;
    uint32_t        reserved;
    HFSPlusForkData theFork;
};
typedef struct HFSPlusAttrForkData HFSPlusAttrForkData;

struct HFSPlusAttrExtents {
    uint32_t            recordType;
    uint32_t            reserved;
    HFSPlusExtentRecord extents;
};
typedef struct HFSPlusAttrExtents HFSPlusAttrExtents;

union HFSPlusAttrRecord {
    uint32_t              recordType;
    HFSPlusAttrInlineData inlineData;
    HFSPlusAttrForkData   forkData;
    HFSPlusAttrExtents    overflowExtents;
};
typedef union HFSPlusAttrRecord HFSPlusAttrRecord;

enum {
    kHFSPlusExtentKeyMaximumLength  = sizeof(HFSPlusExtentKey) - sizeof(uint16_t),
    kHFSExtentKeyMaximumLength      = sizeof(HFSExtentKey) - sizeof(uint8_t),
    kHFSPlusCatalogKeyMaximumLength = sizeof(HFSPlusCatalogKey) - sizeof(uint16_t),
    kHFSPlusCatalogKeyMinimumLength = kHFSPlusCatalogKeyMaximumLength - sizeof(HFSUniStr255) + sizeof(uint16_t),
    kHFSCatalogKeyMaximumLength     = sizeof(HFSCatalogKey) - sizeof(uint8_t),
    kHFSCatalogKeyMinimumLength     = kHFSCatalogKeyMaximumLength - sizeof(Str31) + sizeof(uint8_t),
    kHFSPlusCatalogMinNodeSize      = 4096,
    kHFSPlusExtentMinNodeSize       = 512,
    kHFSPlusAttrMinNodeSize         = 4096
};

enum {
    kHFSVolumeHardwareLockBit      = 7,
    kHFSVolumeUnmountedBit         = 8,
    kHFSVolumeSparedBlocksBit      = 9,
    kHFSVolumeNoCacheRequiredBit   = 10,
    kHFSBootVolumeInconsistentBit  = 11,
    kHFSVolumeSoftwareLockBit      = 15,
    kHFSVolumeHardwareLockMask     = 1 << kHFSVolumeHardwareLockBit,
    kHFSVolumeUnmountedMask        = 1 << kHFSVolumeUnmountedBit,
    kHFSVolumeSparedBlocksMask     = 1 << kHFSVolumeSparedBlocksBit,
    kHFSVolumeNoCacheRequiredMask  = 1 << kHFSVolumeNoCacheRequiredBit,
    kHFSBootVolumeInconsistentMask = 1 << kHFSBootVolumeInconsistentBit,
    kHFSVolumeSoftwareLockMask     = 1 << kHFSVolumeSoftwareLockBit,
    kHFSMDBAttributesMask          = 0x8380
};

enum {
    kHFSCatalogNodeIDsReusedBit  = 12,
    kHFSCatalogNodeIDsReusedMask = 1 << kHFSCatalogNodeIDsReusedBit
};

struct HFSMasterDirectoryBlock {
    uint16_t            drSigWord;
    uint32_t            drCrDate;
    uint32_t            drLsMod;
    uint16_t            drAtrb;
    uint16_t            drNmFls;
    uint16_t            drVBMSt;
    uint16_t            drAllocPtr;
    uint16_t            drNmAlBlks;
    uint32_t            drAlBlkSiz;
    uint32_t            drClpSiz;
    uint16_t            drAlBlSt;
    uint32_t            drNxtCNID;
    uint16_t            drFreeBks;
    Str27               drVN;
    uint32_t            drVolBkUp;
    uint16_t            drVSeqNum;
    uint32_t            drWrCnt;
    uint32_t            drXTClpSiz;
    uint32_t            drCTClpSiz;
    uint16_t            drNmRtDirs;
    uint32_t            drFilCnt;
    uint32_t            drDirCnt;
    uint32_t            drFndrInfo[8];
    uint16_t            drEmbedSigWord;
    HFSExtentDescriptor drEmbedExtent;
    uint32_t            drXTFlSize;
    HFSExtentRecord     drXTExtRec;
    uint32_t            drCTFlSize;
    HFSExtentRecord     drCTExtRec;
};
typedef struct HFSMasterDirectoryBlock  HFSMasterDirectoryBlock;

struct HFSPlusVolumeHeader {
    uint16_t         signature;
    uint16_t         version;
    uint32_t         attributes;
    uint32_t         lastMountedVersion;
    uint32_t         reserved;
    uint32_t         createDate;
    uint32_t         modifyDate;
    uint32_t         backupDate;
    uint32_t         checkedDate;
    uint32_t         fileCount;
    uint32_t         folderCount;
    uint32_t         blockSize;
    uint32_t         totalBlocks;
    uint32_t         freeBlocks;
    uint32_t         nextAllocation;
    uint32_t         rsrcClumpSize;
    uint32_t         dataClumpSize;
    HFSCatalogNodeID nextCatalogID;
    uint32_t         writeCount;
    uint64_t         encodingsBitmap;
    uint8_t          finderInfo[32];
    HFSPlusForkData  allocationFile;
    HFSPlusForkData  extentsFile;
    HFSPlusForkData  catalogFile;
    HFSPlusForkData  attributesFile;
    HFSPlusForkData  startupFile;
};
typedef struct HFSPlusVolumeHeader HFSPlusVolumeHeader;

struct BTNodeDescriptor {
    uint32_t fLink;
    uint32_t bLink;
    int8_t   kind;
    uint8_t  height;
    uint16_t numRecords;
    uint16_t reserved;
};
typedef struct BTNodeDescriptor BTNodeDescriptor;

enum {
    kBTLeafNode   = -1,
    kBTIndexNode  = 0,
    kBTHeaderNode = 1,
    kBTMapNode    = 2
};

struct BTHeaderRec {
    uint16_t treeDepth;
    uint32_t rootNode;
    uint32_t leafRecords;
    uint32_t firstLeafNode;
    uint32_t lastLeafNode;
    uint16_t nodeSize;
    uint16_t maxKeyLength;
    uint32_t totalNodes;
    uint32_t freeNodes;
    uint16_t reserved1;
    uint32_t clumpSize;
    uint8_t  btreeType;
    uint8_t  reserved2;
    uint32_t attributes;
    uint32_t reserved3[16];
};
typedef struct BTHeaderRec BTHeaderRec;

enum {
    kBTBadCloseMask          = 0x00000001,
    kBTBigKeysMask           = 0x00000002,
    kBTVariableIndexKeysMask = 0x00000004
};

// =====================================================================
// File Manager Reference
// OSServices/AppleDiskPartitions.h

enum {
    sbSIGWord = 0x4552,
    sbMac     = 1
};

enum {
    pMapSIG      = 0x504D,
    pdSigWord    = 0x5453,
    oldPMSigWord = pdSigWord,
    newPMSigWord = pMapSIG
};

struct Block0 {
    uint16_t sbSig;
    uint16_t sbBlkSize;
    uint32_t sbBlkCount;
    uint16_t sbDevType;
    uint16_t sbDevId;
    uint32_t sbData;
    uint16_t sbDrvrCount;
    uint32_t ddBlock;
    uint16_t ddSize;
    uint16_t ddType;
    uint16_t ddPad[243];
};

typedef struct Block0 Block0;

static_assert(sizeof(Block0) == 0x200);

struct DDMap {
    uint32_t ddBlock;
    uint16_t ddSize;
    uint16_t ddType;
};
typedef struct DDMap DDMap;

enum {
    kDriverTypeMacSCSI        = 0x0001,
    kDriverTypeMacATA         = 0x0701,
    kDriverTypeMacSCSIChained = 0xFFFF,
    kDriverTypeMacATAChained  = 0xF8FF
};

struct Partition {
    uint16_t pmSig;
    uint16_t pmSigPad;
    uint32_t pmMapBlkCnt;
    uint32_t pmPyPartStart;
    uint32_t pmPartBlkCnt;
    uint8_t  pmPartName[32];
    uint8_t  pmParType[32];
    uint32_t pmLgDataStart;
    uint32_t pmDataCnt;
    uint32_t pmPartStatus;
    uint32_t pmLgBootStart;
    uint32_t pmBootSize;
    uint32_t pmBootAddr;
    uint32_t pmBootAddr2;
    uint32_t pmBootEntry;
    uint32_t pmBootEntry2;
    uint32_t pmBootCksum;
    uint8_t  pmProcessor[16];
    uint16_t pmPad[188];
};
typedef struct Partition Partition;

enum : uint32_t {
    kPartitionAUXIsValid                       = 0x00000001,
    kPartitionAUXIsAllocated                   = 0x00000002,
    kPartitionAUXIsInUse                       = 0x00000004,
    kPartitionAUXIsBootValid                   = 0x00000008,
    kPartitionAUXIsReadable                    = 0x00000010,
    kPartitionAUXIsWriteable                   = 0x00000020,
    kPartitionAUXIsBootCodePositionIndependent = 0x00000040,
    kPartitionIsWriteable                      = 0x00000020,
    kPartitionIsMountedAtStartup               = 0x40000000,
    kPartitionIsStartup                        = 0x80000000,
    kPartitionIsChainCompatible                = 0x00000100,
    kPartitionIsRealDeviceDriver               = 0x00000200,
    kPartitionCanChainToNext                   = 0x00000400
};

enum {
    kPatchDriverSignature   = FOUR_CHAR_CODE('ptDR'),
    kSCSIDriverSignature    = 0x00010600,
    kATADriverSignature     = FOUR_CHAR_CODE('wiki'),
    kSCSICDDriverSignature  = FOUR_CHAR_CODE('CDvr'),
    kATAPIDriverSignature   = FOUR_CHAR_CODE('ATPI'),
    kDriveSetupHFSSignature = FOUR_CHAR_CODE('DSU1')
};

// =====================================================================

#pragma pack(pop)

#endif
