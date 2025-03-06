/*
     File:       HFSVolumes.h
 
     Contains:   On-disk data structures for HFS and HFS Plus volumes.
 
     Version:    Technology: Mac OS 8.1
                 Release:    Universal Interfaces 3.4.2
 
     Copyright:  © 1984-2002 by Apple Computer, Inc.  All rights reserved.
 
     Bugs?:      For bug reports, consult the following page on
                 the World Wide Web:
 
                     http://developer.apple.com/bugreporter/
 
*/
#ifndef __HFSVOLUMES__
#define __HFSVOLUMES__

#include <cinttypes>

#pragma pack(push, 1)

typedef unsigned long                   FourCharCode;
typedef FourCharCode                    OSType;

typedef uint16_t                          UniChar;

struct Rect {
  short               top;
  short               left;
  short               bottom;
  short               right;
};
typedef struct Rect                     Rect;
struct Point {
  short               v;
  short               h;
};
typedef struct Point                    Point;


#define FOUR_CHAR_CODE(x) (x)
struct HFSUniStr255 {
  uint16_t              length;                 /* number of unicode characters */
  UniChar               unicode[255];           /* unicode characters */
};
typedef struct HFSUniStr255             HFSUniStr255;
typedef const HFSUniStr255 *            ConstHFSUniStr255Param;

typedef char Str31[32];
typedef char Str27[28];

/* File info */
/*
     IMPORTANT:
     In MacOS 8, the fdFldr field has become reserved for the Finder.
*/
struct FInfo {
  OSType              fdType;                 /* The type of the file */
  OSType              fdCreator;              /* The file's creator */
  uint16_t            fdFlags;                /* Flags ex. kHasBundle, kIsInvisible, etc. */
  Point               fdLocation;             /* File's location in folder. */
                                              /* If set to {0, 0}, the Finder will place the item automatically */
  int16_t             fdFldr;                 /* Reserved (set to 0) */
};
typedef struct FInfo                    FInfo;
/* Extended file info */
/*
     IMPORTANT:
     In MacOS 8, the fdIconID and fdComment fields were changed
     to become reserved fields for the Finder.
     The fdScript has become an extended flag.
*/
struct FXInfo {
  int16_t             fdIconID;               /* Reserved (set to 0) */
  int16_t             fdReserved[3];          /* Reserved (set to 0) */
  int8_t              fdScript;               /* Extended flags. Script code if high-bit is set */
  int8_t              fdXFlags;               /* Extended flags */
  int16_t             fdComment;              /* Reserved (set to 0). Comment ID if high-bit is clear */
  int32_t             fdPutAway;              /* Put away folder ID */
};
typedef struct FXInfo                   FXInfo;
/* Folder info */
/*
     IMPORTANT:
     In MacOS 8, the frView field was changed to become reserved
     field for the Finder.
*/
struct DInfo {
  Rect                frRect;                 /* Folder's window bounds */
  uint16_t            frFlags;                /* Flags ex. kIsInvisible, kNameLocked, etc.*/
  Point               frLocation;             /* Folder's location in parent folder */
                                              /* If set to {0, 0}, the Finder will place the item automatically */
  int16_t             frView;                 /* Reserved (set to 0) */
};
typedef struct DInfo                    DInfo;
/* Extended folder info */
/*
     IMPORTANT:
     In MacOS 8, the frOpenChain and frComment fields were changed
     to become reserved fields for the Finder.
     The frScript has become an extended flag.
*/
struct DXInfo {
  Point               frScroll;               /* Scroll position */
  int32_t             frOpenChain;            /* Reserved (set to 0) */
  int8_t              frScript;               /* Extended flags. Script code if high-bit is set */
  int8_t              frXFlags;               /* Extended flags */
  int16_t             frComment;              /* Reserved (set to 0). Comment ID if high-bit is clear */
  int32_t             frPutAway;              /* Put away folder ID */
};
typedef struct DXInfo                   DXInfo;


/* Signatures used to differentiate between HFS and HFS Plus volumes */
enum {
  kMFSSigWord                   = 0xD2D7, /* 'RW' in High ASCII */
  kHFSSigWord                   = 0x4244, /* 'BD' in ASCII */
  kHFSPlusSigWord               = 0x482B, /* 'H+' in ASCII */
  kHFSXSigWord                  = 0x4858, /* 'HX' in ASCII */
  kHFSPlusVersion               = 0x0004, /* will change as format changes (version 4 shipped with Mac OS 8.1) */
  kHFSXVersion                  = 0x0005,
  kHFSPlusMountVersion          = FOUR_CHAR_CODE('8.10') /* will change as implementations change ('8.10' in Mac OS 8.1) */
};


/* CatalogNodeID is used to track catalog objects */
typedef uint32_t                          HFSCatalogNodeID;
enum {
  kHFSMaxVolumeNameChars        = 27,
  kHFSMaxFileNameChars          = 31,
  kHFSPlusMaxFileNameChars      = 255
};


/* Extent overflow file data structures */
/* HFS Extent key */
struct HFSExtentKey {
  uint8_t               keyLength;              /* length of key, excluding this field */
  uint8_t               forkType;               /* 0 = data fork, FF = resource fork */
  HFSCatalogNodeID      fileID;                 /* file ID */
  uint16_t              startBlock;             /* first file allocation block number in this extent */
};
typedef struct HFSExtentKey             HFSExtentKey;
/* HFS Plus Extent key */
struct HFSPlusExtentKey {
  uint16_t              keyLength;              /* length of key, excluding this field */
  uint8_t               forkType;               /* 0 = data fork, FF = resource fork */
  uint8_t               pad;                    /* make the other fields align on 32-bit boundary */
  HFSCatalogNodeID      fileID;                 /* file ID */
  uint32_t              startBlock;             /* first file allocation block number in this extent */
};
typedef struct HFSPlusExtentKey         HFSPlusExtentKey;
/* Number of extent descriptors per extent record */
enum {
  kHFSExtentDensity             = 3,
  kHFSPlusExtentDensity         = 8
};

/* HFS extent descriptor */
struct HFSExtentDescriptor {
  uint16_t              startBlock;             /* first allocation block */
  uint16_t              blockCount;             /* number of allocation blocks */
};
typedef struct HFSExtentDescriptor      HFSExtentDescriptor;
/* HFS Plus extent descriptor */
struct HFSPlusExtentDescriptor {
  uint32_t              startBlock;             /* first allocation block */
  uint32_t              blockCount;             /* number of allocation blocks */
};
typedef struct HFSPlusExtentDescriptor  HFSPlusExtentDescriptor;
/* HFS extent record */

typedef HFSExtentDescriptor             HFSExtentRecord[3];
/* HFS Plus extent record */
typedef HFSPlusExtentDescriptor         HFSPlusExtentRecord[8];

/* Fork data info (HFS Plus only) - 80 bytes */
struct HFSPlusForkData {
  uint64_t              logicalSize;            /* fork's logical size in bytes */
  uint32_t              clumpSize;              /* fork's clump size in bytes */
  uint32_t              totalBlocks;            /* total blocks used by this fork */
  HFSPlusExtentRecord   extents;               /* initial set of extents */
};
typedef struct HFSPlusForkData          HFSPlusForkData;
/* Permissions info (HFS Plus only) - 16 bytes */
struct HFSPlusPermissions {
  uint32_t              ownerID;                /* user or group ID of file/folder owner */
  uint32_t              groupID;                /* additional user of group ID */
  uint32_t              permissions;            /* permissions (bytes: unused, owner, group, everyone) */
  uint32_t              specialDevice;          /* UNIX: device for character or block special file */
};
typedef struct HFSPlusPermissions       HFSPlusPermissions;
/* Catalog file data structures */
enum {
  kHFSRootParentID              = 1,    /* Parent ID of the root folder */
  kHFSRootFolderID              = 2,    /* Folder ID of the root folder */
  kHFSExtentsFileID             = 3,    /* File ID of the extents file */
  kHFSCatalogFileID             = 4,    /* File ID of the catalog file */
  kHFSBadBlockFileID            = 5,    /* File ID of the bad allocation block file */
  kHFSAllocationFileID          = 6,    /* File ID of the allocation file (HFS Plus only) */
  kHFSStartupFileID             = 7,    /* File ID of the startup file (HFS Plus only) */
  kHFSAttributesFileID          = 8,    /* File ID of the attribute file (HFS Plus only) */
  kHFSBogusExtentFileID         = 15,   /* Used for exchanging extents in extents file */
  kHFSFirstUserCatalogNodeID    = 16
};


/* HFS catalog key */
struct HFSCatalogKey {
  uint8_t               keyLength;              /* key length (in bytes) */
  uint8_t               reserved;               /* reserved (set to zero) */
  HFSCatalogNodeID      parentID;               /* parent folder ID */
  Str31                 nodeName;               /* catalog node name */
};
typedef struct HFSCatalogKey            HFSCatalogKey;
/* HFS Plus catalog key */
struct HFSPlusCatalogKey {
  uint16_t              keyLength;              /* key length (in bytes) */
  HFSCatalogNodeID      parentID;               /* parent folder ID */
  HFSUniStr255          nodeName;               /* catalog node name */
};
typedef struct HFSPlusCatalogKey        HFSPlusCatalogKey;

/* Catalog record types */
enum {
                                        /* HFS Catalog Records */
  kHFSFolderRecord              = 0x0100, /* Folder record */
  kHFSFileRecord                = 0x0200, /* File record */
  kHFSFolderThreadRecord        = 0x0300, /* Folder thread record */
  kHFSFileThreadRecord          = 0x0400, /* File thread record */
                                        /* HFS Plus Catalog Records */
  kHFSPlusFolderRecord          = 1,    /* Folder record */
  kHFSPlusFileRecord            = 2,    /* File record */
  kHFSPlusFolderThreadRecord    = 3,    /* Folder thread record */
  kHFSPlusFileThreadRecord      = 4     /* File thread record */
};


/* Catalog file record flags */
enum {
  kHFSFileLockedBit             = 0x0000, /* file is locked and cannot be written to */
  kHFSFileLockedMask            = 0x0001,
  kHFSThreadExistsBit           = 0x0001, /* a file thread record exists for this file */
  kHFSThreadExistsMask          = 0x0002
};


/* HFS catalog folder record - 70 bytes */
struct HFSCatalogFolder {
  int16_t               recordType;             /* record type */
  uint16_t              flags;                  /* folder flags */
  uint16_t              valence;                /* folder valence */
  HFSCatalogNodeID      folderID;               /* folder ID */
  uint32_t              createDate;             /* date and time of creation */
  uint32_t              modifyDate;             /* date and time of last modification */
  uint32_t              backupDate;             /* date and time of last backup */
  DInfo                 userInfo;               /* Finder information */
  DXInfo                finderInfo;             /* additional Finder information */
  uint32_t              reserved[4];            /* reserved - set to zero */
};
typedef struct HFSCatalogFolder         HFSCatalogFolder;
/* HFS Plus catalog folder record - 88 bytes */
struct HFSPlusCatalogFolder {
  int16_t               recordType;             /* record type = HFS Plus folder record */
  uint16_t              flags;                  /* file flags */
  uint32_t              valence;                /* folder's valence (limited to 2^16 in Mac OS) */
  HFSCatalogNodeID      folderID;               /* folder ID */
  uint32_t              createDate;             /* date and time of creation */
  uint32_t              contentModDate;         /* date and time of last content modification */
  uint32_t              attributeModDate;       /* date and time of last attribute modification */
  uint32_t              accessDate;             /* date and time of last access (Rhapsody only) */
  uint32_t              backupDate;             /* date and time of last backup */
  HFSPlusPermissions    permissions;            /* permissions (for Rhapsody) */
  DInfo                 userInfo;               /* Finder information */
  DXInfo                finderInfo;             /* additional Finder information */
  uint32_t              textEncoding;           /* hint for name conversions */
  uint32_t              reserved;               /* reserved - set to zero */
};
typedef struct HFSPlusCatalogFolder     HFSPlusCatalogFolder;
/* HFS catalog file record - 102 bytes */
struct HFSCatalogFile {
  int16_t               recordType;             /* record type */
  uint8_t               flags;                  /* file flags */
  int8_t                fileType;               /* file type (unused ?) */
  FInfo                 userInfo;               /* Finder information */
  HFSCatalogNodeID      fileID;                 /* file ID */
  uint16_t              dataStartBlock;         /* not used - set to zero */
  int32_t               dataLogicalSize;        /* logical EOF of data fork */
  int32_t               dataPhysicalSize;       /* physical EOF of data fork */
  uint16_t              rsrcStartBlock;         /* not used - set to zero */
  int32_t               rsrcLogicalSize;        /* logical EOF of resource fork */
  int32_t               rsrcPhysicalSize;       /* physical EOF of resource fork */
  uint32_t              createDate;             /* date and time of creation */
  uint32_t              modifyDate;             /* date and time of last modification */
  uint32_t              backupDate;             /* date and time of last backup */
  FXInfo                finderInfo;             /* additional Finder information */
  uint16_t              clumpSize;              /* file clump size (not used) */
  HFSExtentRecord       dataExtents;            /* first data fork extent record */
  HFSExtentRecord       rsrcExtents;            /* first resource fork extent record */
  uint32_t              reserved;               /* reserved - set to zero */
};
typedef struct HFSCatalogFile           HFSCatalogFile;
/* HFS Plus catalog file record - 248 bytes */
struct HFSPlusCatalogFile {
  int16_t               recordType;             /* record type = HFS Plus file record */
  uint16_t              flags;                  /* file flags */
  uint32_t              reserved1;              /* reserved - set to zero */
  HFSCatalogNodeID      fileID;                 /* file ID */
  uint32_t              createDate;             /* date and time of creation */
  uint32_t              contentModDate;         /* date and time of last content modification */
  uint32_t              attributeModDate;       /* date and time of last attribute modification */
  uint32_t              accessDate;             /* date and time of last access (Rhapsody only) */
  uint32_t              backupDate;             /* date and time of last backup */
  HFSPlusPermissions    permissions;            /* permissions (for Rhapsody) */
  FInfo                 userInfo;               /* Finder information */
  FXInfo                finderInfo;             /* additional Finder information */
  uint32_t              textEncoding;           /* hint for name conversions */
  uint32_t              reserved2;              /* reserved - set to zero */

                                                /* start on double long (64 bit) boundry */
  HFSPlusForkData       dataFork;               /* size and block data for data fork */
  HFSPlusForkData       resourceFork;           /* size and block data for resource fork */
};
typedef struct HFSPlusCatalogFile       HFSPlusCatalogFile;
/* HFS catalog thread record - 46 bytes */
struct HFSCatalogThread {
  int16_t              recordType;             /* record type */
  int32_t              reserved[2];            /* reserved - set to zero */
  HFSCatalogNodeID     parentID;               /* parent ID for this catalog node */
  Str31                nodeName;               /* name of this catalog node */
};
typedef struct HFSCatalogThread         HFSCatalogThread;
/* HFS Plus catalog thread record -- maximum 520 bytes */
struct HFSPlusCatalogThread {
  int16_t              recordType;             /* record type */
  int16_t              reserved;               /* reserved - set to zero */
  HFSCatalogNodeID     parentID;               /* parent ID for this catalog node */
  HFSUniStr255         nodeName;               /* name of this catalog node (variable length) */
};
typedef struct HFSPlusCatalogThread     HFSPlusCatalogThread;

/*
    These are the types of records in the attribute B-tree.  The values were chosen
    so that they wouldn't conflict with the catalog record types.
*/
enum {
  kHFSPlusAttrInlineData        = 0x10, /* if size <  kAttrOverflowSize */
  kHFSPlusAttrForkData          = 0x20, /* if size >= kAttrOverflowSize */
  kHFSPlusAttrExtents           = 0x30  /* overflow extents for large attributes */
};


/*
    HFSPlusAttrInlineData
    For small attributes, whose entire value is stored within this one
    B-tree record.
    There would not be any other records for this attribute.
*/
struct HFSPlusAttrInlineData {
  uint32_t              recordType;             /*    = kHFSPlusAttrInlineData*/
  uint32_t              reserved;
  uint32_t              logicalSize;            /*    size in bytes of userData*/
  uint8_t               userData[2];            /*    variable length; space allocated is a multiple of 2 bytes*/
};
typedef struct HFSPlusAttrInlineData    HFSPlusAttrInlineData;
/*
    HFSPlusAttrForkData
    For larger attributes, whose value is stored in allocation blocks.
    If the attribute has more than 8 extents, there will be additonal
    records (of type HFSPlusAttrExtents) for this attribute.
*/
struct HFSPlusAttrForkData {
  uint32_t              recordType;             /*    = kHFSPlusAttrForkData*/
  uint32_t              reserved;
  HFSPlusForkData       theFork;                /*    size and first extents of value*/
};
typedef struct HFSPlusAttrForkData      HFSPlusAttrForkData;
/*
    HFSPlusAttrExtents
    This record contains information about overflow extents for large,
    fragmented attributes.
*/
struct HFSPlusAttrExtents {
  uint32_t              recordType;             /*    = kHFSPlusAttrExtents*/
  uint32_t              reserved;
  HFSPlusExtentRecord   extents;               /*    additional extents*/
};
typedef struct HFSPlusAttrExtents       HFSPlusAttrExtents;
/*  A generic Attribute Record*/
union HFSPlusAttrRecord {
  uint32_t               recordType;
  HFSPlusAttrInlineData  inlineData;
  HFSPlusAttrForkData    forkData;
  HFSPlusAttrExtents     overflowExtents;
};
typedef union HFSPlusAttrRecord         HFSPlusAttrRecord;
/* Key and node lengths */
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


/* HFS and HFS Plus volume attribute bits */
enum {
                                        /* Bits 0-6 are reserved (always cleared by MountVol call) */
  kHFSVolumeHardwareLockBit     = 7,    /* volume is locked by hardware */
  kHFSVolumeUnmountedBit        = 8,    /* volume was successfully unmounted */
  kHFSVolumeSparedBlocksBit     = 9,    /* volume has bad blocks spared */
  kHFSVolumeNoCacheRequiredBit  = 10,   /* don't cache volume blocks (i.e. RAM or ROM disk) */
  kHFSBootVolumeInconsistentBit = 11,   /* boot volume is inconsistent (System 7.6 and later) */
                                        /* Bits 12-14 are reserved for future use */
  kHFSVolumeSoftwareLockBit     = 15,   /* volume is locked by software */
  kHFSVolumeHardwareLockMask    = 1 << kHFSVolumeHardwareLockBit,
  kHFSVolumeUnmountedMask       = 1 << kHFSVolumeUnmountedBit,
  kHFSVolumeSparedBlocksMask    = 1 << kHFSVolumeSparedBlocksBit,
  kHFSVolumeNoCacheRequiredMask = 1 << kHFSVolumeNoCacheRequiredBit,
  kHFSBootVolumeInconsistentMask = 1 << kHFSBootVolumeInconsistentBit,
  kHFSVolumeSoftwareLockMask    = 1 << kHFSVolumeSoftwareLockBit,
  kHFSMDBAttributesMask         = 0x8380
};

enum {
  kHFSCatalogNodeIDsReusedBit   = 12,   /* nextCatalogID wrapped around */
  kHFSCatalogNodeIDsReusedMask  = 1 << kHFSCatalogNodeIDsReusedBit
};

/* Master Directory Block (HFS only) - 162 bytes */
/* Stored at sector #2 (3rd sector) */
struct HFSMasterDirectoryBlock {

                                                /* These first fields are also used by MFS */

  uint16_t              drSigWord;              /* volume signature */
  uint32_t              drCrDate;               /* date and time of volume creation */
  uint32_t              drLsMod;                /* date and time of last modification */
  uint16_t              drAtrb;                 /* volume attributes */
  uint16_t              drNmFls;                /* number of files in root folder */
  uint16_t              drVBMSt;                /* first block of volume bitmap */
  uint16_t              drAllocPtr;             /* start of next allocation search */
  uint16_t              drNmAlBlks;             /* number of allocation blocks in volume */
  uint32_t              drAlBlkSiz;             /* size (in bytes) of allocation blocks */
  uint32_t              drClpSiz;               /* default clump size */
  uint16_t              drAlBlSt;               /* first allocation block in volume */
  uint32_t              drNxtCNID;              /* next unused catalog node ID */
  uint16_t              drFreeBks;              /* number of unused allocation blocks */
  Str27                 drVN;                   /* volume name */

                                                /* Master Directory Block extensions for HFS */

  uint32_t              drVolBkUp;              /* date and time of last backup */
  uint16_t              drVSeqNum;              /* volume backup sequence number */
  uint32_t              drWrCnt;                /* volume write count */
  uint32_t              drXTClpSiz;             /* clump size for extents overflow file */
  uint32_t              drCTClpSiz;             /* clump size for catalog file */
  uint16_t              drNmRtDirs;             /* number of directories in root folder */
  uint32_t              drFilCnt;               /* number of files in volume */
  uint32_t              drDirCnt;               /* number of directories in volume */
  int32_t               drFndrInfo[8];          /* information used by the Finder */
  uint16_t              drEmbedSigWord;         /* embedded volume signature (formerly drVCSize) */
  HFSExtentDescriptor   drEmbedExtent;          /* embedded volume location and size (formerly drVBMCSize and drCtlCSize) */
  uint32_t              drXTFlSize;             /* size of extents overflow file */
  HFSExtentRecord       drXTExtRec;             /* extent record for extents overflow file */
  uint32_t              drCTFlSize;             /* size of catalog file */
  HFSExtentRecord       drCTExtRec;             /* extent record for catalog file */
};
typedef struct HFSMasterDirectoryBlock  HFSMasterDirectoryBlock;
/* HFSPlusVolumeHeader (HFS Plus only) - 512 bytes */
/* Stored at sector #2 (3rd sector) and second-to-last sector. */
struct HFSPlusVolumeHeader {
  uint16_t              signature;              /* volume signature == 'H+' */
  uint16_t              version;                /* current version is kHFSPlusVersion */
  uint32_t              attributes;             /* volume attributes */
  uint32_t              lastMountedVersion;     /* implementation version which last mounted volume */
  uint32_t              reserved;               /* reserved - set to zero */

  uint32_t              createDate;             /* date and time of volume creation */
  uint32_t              modifyDate;             /* date and time of last modification */
  uint32_t              backupDate;             /* date and time of last backup */
  uint32_t              checkedDate;            /* date and time of last disk check */

  uint32_t              fileCount;              /* number of files in volume */
  uint32_t              folderCount;            /* number of directories in volume */

  uint32_t              blockSize;              /* size (in bytes) of allocation blocks */
  uint32_t              totalBlocks;            /* number of allocation blocks in volume (includes this header and VBM*/
  uint32_t              freeBlocks;             /* number of unused allocation blocks */

  uint32_t              nextAllocation;         /* start of next allocation search */
  uint32_t              rsrcClumpSize;          /* default resource fork clump size */
  uint32_t              dataClumpSize;          /* default data fork clump size */
  HFSCatalogNodeID      nextCatalogID;          /* next unused catalog node ID */

  uint32_t              writeCount;             /* volume write count */
  uint64_t              encodingsBitmap;        /* which encodings have been use  on this volume */

  uint8_t               finderInfo[32];         /* information used by the Finder */

  HFSPlusForkData       allocationFile;         /* allocation bitmap file */
  HFSPlusForkData       extentsFile;            /* extents B-tree file */
  HFSPlusForkData       catalogFile;            /* catalog B-tree file */
  HFSPlusForkData       attributesFile;         /* extended attributes B-tree file */
  HFSPlusForkData       startupFile;            /* boot file */
};
typedef struct HFSPlusVolumeHeader      HFSPlusVolumeHeader;
/* ---------- HFS and HFS Plus B-tree structures ---------- */
/* BTNodeDescriptor -- Every B-tree node starts with these fields. */
struct BTNodeDescriptor {
  uint32_t              fLink;                  /*    next node at this level*/
  uint32_t              bLink;                  /*    previous node at this level*/
  int8_t                kind;                   /*    kind of node (leaf, index, header, map)*/
  uint8_t               height;                 /*    zero for header, map; child is one more than parent*/
  uint16_t              numRecords;             /*    number of records in this node*/
  uint16_t              reserved;               /*    reserved; set to zero*/
};
typedef struct BTNodeDescriptor         BTNodeDescriptor;
/* Constants for BTNodeDescriptor kind */
enum {
  kBTLeafNode                   = -1,
  kBTIndexNode                  = 0,
  kBTHeaderNode                 = 1,
  kBTMapNode                    = 2
};

/* BTHeaderRec -- The first record of a B-tree header node */
struct BTHeaderRec {
  uint16_t              treeDepth;              /*    maximum height (usually leaf nodes)*/
  uint32_t              rootNode;               /*    node number of root node*/
  uint32_t              leafRecords;            /*    number of leaf records in all leaf nodes*/
  uint32_t              firstLeafNode;          /*    node number of first leaf node*/
  uint32_t              lastLeafNode;           /*    node number of last leaf node*/
  uint16_t              nodeSize;               /*    size of a node, in bytes*/
  uint16_t              maxKeyLength;           /*    reserved*/
  uint32_t              totalNodes;             /*    total number of nodes in tree*/
  uint32_t              freeNodes;              /*    number of unused (free) nodes in tree*/
  uint16_t              reserved1;              /*    unused*/
  uint32_t              clumpSize;              /*    reserved*/
  uint8_t               btreeType;              /*    reserved*/
  uint8_t               reserved2;              /*    reserved*/
  uint32_t              attributes;             /*    persistent attributes about the tree*/
  uint32_t              reserved3[16];          /*    reserved*/
};
typedef struct BTHeaderRec              BTHeaderRec;
/* Constants for BTHeaderRec attributes */
enum {
  kBTBadCloseMask               = 0x00000001, /*    reserved*/
  kBTBigKeysMask                = 0x00000002, /*    key length field is 16 bits*/
  kBTVariableIndexKeysMask      = 0x00000004  /*    keys in index nodes are variable length*/
};

#pragma pack(pop)

#endif /* __HFSVOLUMES__ */
