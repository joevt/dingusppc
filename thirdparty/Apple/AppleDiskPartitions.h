/*
     File:       AppleDiskPartitions.h
 
     Contains:   The Apple disk partition scheme as defined in Inside Macintosh: Volume V.
 
     Version:    Technology: Mac OS 9
                 Release:    Universal Interfaces 3.4.2
 
     Copyright:  © 2000-2002 by Apple Computer, Inc., all rights reserved
 
     Bugs?:      For bug reports, consult the following page on
                 the World Wide Web:
 
                     http://developer.apple.com/bugreporter/
 
*/
#ifndef __APPLEDISKPARTITIONS__
#define __APPLEDISKPARTITIONS__

#include <thirdparty/Apple/HFSVolumes.h>
#include <cinttypes>

#pragma pack(push, 1)

/* Block 0 Definitions */
enum {
  sbSIGWord                     = 0x4552, /* signature word for Block 0 ('ER') */
  sbMac                         = 1     /* system type for Mac */
};

/* Partition Map Signatures */
enum {
  pMapSIG                       = 0x504D, /* partition map signature ('PM') */
  pdSigWord                     = 0x5453, /* partition map signature ('TS') */
  oldPMSigWord                  = pdSigWord,
  newPMSigWord                  = pMapSIG
};


/* Driver Descriptor Map */
struct Block0 {
  uint16_t              sbSig;                  /* 00 unique value for SCSI block 0 */
  uint16_t              sbBlkSize;              /* 02 block size of device */
  uint32_t              sbBlkCount;             /* 04 number of blocks on device */
  uint16_t              sbDevType;              /* 08 device type */
  uint16_t              sbDevId;                /* 0A device id */
  uint32_t              sbData;                 /* 0C not used */
  uint16_t              sbDrvrCount;            /* 10 driver descriptor count */
  uint32_t              ddBlock;                /* 12 1st driver's starting block */
  uint16_t              ddSize;                 /* 16 size of 1st driver (512-byte blks) */
  uint16_t              ddType;                 /* 18 system type (1 for Mac+) */
  uint16_t              ddPad[243];             /* 1A ARRAY[0..242] OF INTEGER; not used */
};

typedef struct Block0                   Block0;

static_assert(sizeof(Block0) == 0x200);

/* Driver descriptor */
struct DDMap {
  uint32_t              ddBlock;                /* 1st driver's starting block */
  uint16_t              ddSize;                 /* size of 1st driver (512-byte blks) */
  uint16_t              ddType;                 /* system type (1 for Mac+) */
};
typedef struct DDMap                    DDMap;
/* Constants for the ddType field of the DDMap structure. */
enum {
  kDriverTypeMacSCSI            = 0x0001,
  kDriverTypeMacATA             = 0x0701,
  kDriverTypeMacSCSIChained     = 0xFFFF,
  kDriverTypeMacATAChained      = 0xF8FF
};

/* Partition Map Entry */
struct Partition {
  uint16_t              pmSig;                  /*     0 unique value for map entry blk */
  uint16_t              pmSigPad;               /*     2 currently unused */
  uint32_t              pmMapBlkCnt;            /*     4 # of blks in partition map */
  uint32_t              pmPyPartStart;          /*     8 physical start blk of partition */
  uint32_t              pmPartBlkCnt;           /*     C # of blks in this partition */
  uint8_t               pmPartName[32];         /*  0x10 ASCII partition name */
  uint8_t               pmParType[32];          /*  0x30 ASCII partition type */
  uint32_t              pmLgDataStart;          /*  0x50 log. # of partition's 1st data blk */
  uint32_t              pmDataCnt;              /*  0x54 # of blks in partition's data area */
  uint32_t              pmPartStatus;           /*  0x58 bit field for partition status */
  uint32_t              pmLgBootStart;          /*  0x5C log. blk of partition's boot code */
  uint32_t              pmBootSize;             /*  0x60 number of bytes in boot code */
  uint32_t              pmBootAddr;             /*  0x64 memory load address of boot code */
  uint32_t              pmBootAddr2;            /*  0x68 currently unused */
  uint32_t              pmBootEntry;            /*  0x6C entry point of boot code */
  uint32_t              pmBootEntry2;           /*  0x70 currently unused */
  uint32_t              pmBootCksum;            /*  0x74 checksum of boot code */
  uint8_t               pmProcessor[16];        /*  0x78 ASCII for the processor type */
  uint16_t              pmPad[188];             /*  0x88 ARRAY[0..187] OF INTEGER; not used */
};                                              /* 0x200 */
typedef struct Partition                Partition;

/* Flags for the pmPartStatus field of the Partition data structure. */
enum {
  kPartitionAUXIsValid          = 0x00000001,
  kPartitionAUXIsAllocated      = 0x00000002,
  kPartitionAUXIsInUse          = 0x00000004,
  kPartitionAUXIsBootValid      = 0x00000008,
  kPartitionAUXIsReadable       = 0x00000010,
  kPartitionAUXIsWriteable      = 0x00000020,
  kPartitionAUXIsBootCodePositionIndependent = 0x00000040,
  kPartitionIsWriteable         = 0x00000020,
  kPartitionIsMountedAtStartup  = 0x40000000,
  kPartitionIsStartup           = (long)0x80000000,
  kPartitionIsChainCompatible   = 0x00000100,
  kPartitionIsRealDeviceDriver  = 0x00000200,
  kPartitionCanChainToNext      = 0x00000400
};




/* Well known driver signatures, stored in the first four byte of pmPad. */
enum {
  kPatchDriverSignature         = FOUR_CHAR_CODE('ptDR'), /* SCSI and ATA[PI] patch driver    */
  kSCSIDriverSignature          = 0x00010600, /* SCSI  hard disk driver           */
  kATADriverSignature           = FOUR_CHAR_CODE('wiki'), /* ATA   hard disk driver           */
  kSCSICDDriverSignature        = FOUR_CHAR_CODE('CDvr'), /* SCSI  CD-ROM    driver           */
  kATAPIDriverSignature         = FOUR_CHAR_CODE('ATPI'), /* ATAPI CD-ROM    driver           */
  kDriveSetupHFSSignature       = FOUR_CHAR_CODE('DSU1') /* Drive Setup HFS partition        */
};

#pragma pack(pop)

#endif /* __APPLEDISKPARTITIONS__ */
