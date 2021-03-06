/*
 * FAT32 Formatting. This file is part of Rufusl.
 *
 * Copyright (C) 2016 Ognjen Galic
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
*/

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <string.h>

#include "fat32.h"
#include "log.h"
#include "definitions.h"

int format_fat32(const uint32_t *part_fd, uint8_t cluster_size, char *label) {

    /* This portion declares the FAT32 BPB. Why I did it this way is because
     * constructing a BPB on-the-fly is redundant as most of the fields in the
     * BPB are constants for FAT32, and only a couple of bytes (around 20) need
     * actual modifying to construct a valid FAT32 file system that mounts fine
     * on Windows and passes fsck.fat with one error, and that is that the FSInfo
     * field is left uninitialized, but that is the way Windows 7 leaves it and
     * when the fs gets mounted on Linux it gets initialized. After an initial
     * mount on Linux, fsck.fat reports no errors on the fs. Also, all bootable
     * and executable code is left blank, so if someone boots from this partition
     * by accident, they will not get a message. FAT32 partitions are not bootable
     * directly from the code in the BPB.
     *
     *  Legend:
     *
     *   0 BPB BIOS Jump to nothing
     *   3 OEM String: RUFUSL
     *  11 BPB_BytsPerSec = 512B constant for compat - uint16_t
     *  13 BPB_SecPerClus - Empty for populating - char
     *  14 BPB_RsvdSecCnt = 32 constant for FAT32 - uint16_t
     *  16 BPB_NumFATs - Constant 2 per FAT32 spec - uint8_t
     *  17 BPB_RootEntCnt = 0 for FAT32 - uint16_t
     *  19 BPB_TotSec16 = 0 for FAT32 - uint16_t
     *  21 BPB_Media - Constant F8 for removable/solid media - uint8_t
     *  22 BPB_FATSz16 = 0 for FAT32 - uint16_t
     *  24 BPB_SecPerTrk = Obsolete - uint16_t
     *  26 BPB_NumHeads = Obsolete - uint16_t
     *  28 BPB_HiddSec = Obsolete - uint32_t
     *  32 BPB_TotSec32 - Empty for populating - uint32_t
     *  36 BPB_FATSz32 - Empty for populating - uint32_t
     *  40 BPB_ExtFlags = 0 - No special flags - uint16_t
     *  42 BPB_FSVer = 0 - Version 0 - uint16_t
     *  44 BPB_RootClus = 2 for FAT32 - uint32_t
     *  48 BPB_FSInfo = 1 for FAT32 - uint16_t
     *  50 BPB_BkBootSec = 6 for FAT32 Microsoft recommended - uint16_t
     *  52 BPB_Reserved - Reserved 12 bytes for nothing - char[12]
     *  64 BS_DrvNum = 0x80, Windows 7 default - uint8_t
     *  65 BS_Reserved1 = Byte reserved for nothing - uint8_t
     *  66 BS_BootSig = 0x29, BOOT signature - uint8_t
     *  67 BS_VolID = Volume ID - char[4]
     *  71 BS_VolLab - Volume Label - char[11]
     *  82 BS_FilSysType - "FAT32   " - char[8]
     *  90 Dummy boot code
     * 510 MBR Signature
     * 512 Total
    */

    unsigned char fat32_bpb[512] = {

    /*  0 */ 0xEB, 0x00, 0x90,
    /*  3 */ 0x52, 0x55, 0x46, 0x55, 0x53, 0x4c, 0x00, 0x00,
    /* 11 */ 0x00, 0x02,
    /* 13 */ 0x00,
    /* 14 */ 0x20, 0x00,
    /* 16 */ 0x02,
    /* 17 */ 0x00, 0x00,
    /* 19 */ 0x00, 0x00,
    /* 21 */ 0xF8,
    /* 22 */ 0x00, 0x00,
    /* 24 */ 0xFF, 0xFF,
    /* 26 */ 0xFF, 0xFF,
    /* 28 */ 0x00, 0x00, 0x00, 0x00,
    /* 32 */ 0x00, 0x00, 0x00, 0x00,
    /* 36 */ 0x00, 0x00, 0x00, 0x00,
    /* 40 */ 0x00, 0x00,
    /* 42 */ 0x00, 0x00,
    /* 44 */ 0x02, 0x00, 0x00, 0x00,
    /* 48 */ 0x01, 0x00,
    /* 50 */ 0x06, 0x00,
    /* 52 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 64 */ 0x80,
    /* 65 */ 0x00,
    /* 66 */ 0x29,
    /* 67 */ 0xBE, 0xBA, 0xFE, 0xCA,
    /* 71 */ 0x4E, 0x4F, 0x20, 0x4E, 0x41, 0x4D,
             0x45, 0x20, 0x20, 0x20, 0x20,
    /* 82 */ 0x46, 0x41, 0x54, 0x33, 0x32, 0x20, 0x20, 0x20,
    /* 90 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   /* 510 */ 0x55, 0xAA
   /* 512 Total */

    };

    unsigned char fat32_fsi[512] = {

    /*  0 */ 0x52, 0x52, 0x61, 0x41, /* FSI_LeadSig */
    /*  4 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, /* Empty space */
   /* 484 */ 0x72, 0x72, 0x41, 0x61, /* FSI_StrucSig */
   /* 488 */ 0xFF, 0xFF, 0xFF, 0xFF, /* FSI_Free_Count - Not known - Windows 7 default */
   /* 492 */ 0xFF, 0xFF, 0xFF, 0xFF, /* FSI_Next_Free  - Not known - Windows 7 default */
   /* 496 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Reserved space */
   /* 508 */ 0x00, 0x00, 0x55, 0xAA /* MBR Signature */
   /* 512 Total */

    };

    /* This is an empty FAT Table, with its 8 byte
       magic number and an EOC to declare that it is
       empty */

    unsigned char fat32_fat[12] = {
        0xF8,0xFF,0xFF,0x0F, /* Media Descriptor byte 0xF8 */
        0xFF,0xFF,0xFF,0x0F, /* Root EOC */
        0xFF,0xFF,0xFF,0x0F, /* Blank FAT EOC */
    };

    const uint16_t BPB_ResvdSecCnt = 32;

    uint8_t BPB_SecPerClus;
    uint64_t DskSize;

    const uint8_t BPB_NumFATs = 2;

    if (ioctl(*part_fd, BLKGETSIZE, &DskSize) < 0) {
        perror("ioctl");
        return -1;
    }

    if (DskSize > UINT32_MAX - 1) {
        r_printf("Volume to big for FAT32!\n");
        return -1;
    }

    switch (cluster_size) {
      case BS_512B:
        BPB_SecPerClus = 1;
        break;
      case BS_1024B:
        BPB_SecPerClus = 2;
        break;
      case BS_2048B:
        BPB_SecPerClus = 4;
        break;
      case BS_4096B:
        BPB_SecPerClus = 8;
        break;
      case BS_8192B:
        BPB_SecPerClus = 16;
        break;
      case BS_16384B:
        BPB_SecPerClus = 32;
        break;
      case BS_32768B:
        BPB_SecPerClus = 64;
        break;
      default:

        r_printf("Autosetting cluster size.\n");

        if (DskSize < 66600) {
          r_printf("ERROR: Volume is too small!\n");
          return -1;
        } else if (DskSize < 532480) {
          BPB_SecPerClus = 1;
        } else if (DskSize < 16777216) {
          BPB_SecPerClus = 8;
        } else if (DskSize < 33554432) {
          BPB_SecPerClus = 16;
        } else if (DskSize < 67108864) {
          BPB_SecPerClus = 32;
        } else if (DskSize < 0xFFFFFFFF) {
          BPB_SecPerClus = 64;
        }

    }

    uint32_t BPB_TotSec32 = (uint32_t) DskSize; /* Transition to 32-bit */
    uint32_t BPB_FATSz32;

    /* This snippet of code is from the FAT32 specification */

    uint32_t TmpVal1 = BPB_TotSec32 - BPB_ResvdSecCnt;
    uint32_t TmpVal2 = (256 * BPB_SecPerClus) + BPB_NumFATs;
    TmpVal2 = TmpVal2 / 2;
    BPB_FATSz32 = (TmpVal1 + (TmpVal2 - 1)) / TmpVal2;

    /* Some debug informatio */

    r_printf("Device fd: %d\n", *part_fd);
    r_printf("Label: %s\n",label);
    r_printf("Sectors per cluter: %d\n", BPB_SecPerClus);
    r_printf("Total sectors: %d\n", BPB_TotSec32);
    r_printf("FAT32 FAT Size: %d\n", BPB_FATSz32);
    r_printf("BPB Size: %ld\n", sizeof(fat32_bpb));
    r_printf("FSI Size: %ld\n", sizeof(fat32_fsi));
    r_printf("FAT Size: %ld\n", sizeof(fat32_fat));

    /* STEP 1/4: Populate: PBP_SecPerClus */

    fat32_bpb[BPB_SEC_PER_CLUS_OFFSET] = BPB_SecPerClus;

    /* The following snippet dissects integers into bytes,
       and writes them into the BPB array whilst flipping
       their endianess to little endian on the fly */

    /* STEP 2/4: Populate: BPB_TotSec32 */

    fat32_bpb[BPB_TOT_SEC_32_OFFSET + 3] = (BPB_TotSec32 >> 24) & 0xFF;
    fat32_bpb[BPB_TOT_SEC_32_OFFSET + 2] = (BPB_TotSec32 >> 16) & 0xFF;
    fat32_bpb[BPB_TOT_SEC_32_OFFSET + 1] = (BPB_TotSec32 >> 8) & 0xFF;
    fat32_bpb[BPB_TOT_SEC_32_OFFSET    ] = BPB_TotSec32 & 0xFF;

    /* STEP 3/4: Populate: BPB_FATSz32 */

    fat32_bpb[BPB_FAT_SZ_32_OFFSET + 3] = (BPB_FATSz32 >> 24) & 0xFF;
    fat32_bpb[BPB_FAT_SZ_32_OFFSET + 2] = (BPB_FATSz32 >> 16) & 0xFF;
    fat32_bpb[BPB_FAT_SZ_32_OFFSET + 1] = (BPB_FATSz32 >> 8) & 0xFF;
    fat32_bpb[BPB_FAT_SZ_32_OFFSET    ] = BPB_FATSz32 & 0xFF;

    /* STEP 4/4: Populate: BS_VolLab */

    if (strlen(label) > 11) {
        r_printf("WARNING: Label is larger than allowed 11 chars. Will truncate.");
    }

    for (int i = 0; i < 11; i++) {
        fat32_bpb[BPB_LABEL_OFFSET + i] = *(label+i);
        if (*(label+i) == 0x00) {
            for (; i < 11; i++) {
                fat32_bpb[BPB_LABEL_OFFSET + i] = 0x00;
            }
        }
    }

    r_printf("File descriptor: %d\n", *part_fd);

    /* See the macro on the beginning of the file */

    SEEKNWRITE(*part_fd, 0, fat32_bpb, 512);                                     /* Write first BPB */
    SEEKNWRITE(*part_fd, 512, fat32_fsi, 512);                                   /* Write first FSI */
    SEEKNWRITE(*part_fd, 3072, fat32_bpb, 512);                                  /* Write second BPB */
    SEEKNWRITE(*part_fd, 3584, fat32_fsi, 512);                                  /* Write second FSI */
    SEEKNWRITE(*part_fd, BPB_ResvdSecCnt * 512, fat32_fat, 12);                  /* Write first FAT */
    SEEKNWRITE(*part_fd, (BPB_ResvdSecCnt + BPB_FATSz32) * 512, fat32_fat, 12);  /* Write second FAT */

    sync();

    return 0;
}
