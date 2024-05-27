/** This file is part of ipled - a versatile LED strip controller.
Copyright (C) 2024 Sven Pauli <sven@knst-wrk.de>

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

#include "../sd.h"

#include "diskio.h"


DSTATUS disk_status (BYTE pdrv)
{
    if (pdrv != 0)
        return STA_NOINIT;

    if (!sd_type())
        return STA_NOINIT;

    if (0)
        return STA_PROTECT;

    return 0;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv == 0) {
        if (!sd_identify())
            return STA_NOINIT;
    }

    return disk_status(pdrv);
}

DRESULT disk_read (BYTE pdrv, BYTE* buff, LBA_t sector, UINT count)
{
    if (pdrv != 0)
        return RES_NOTRDY;
    else if (!sd_type())
        return RES_NOTRDY;

    if (!count)
        return RES_PARERR;

    if (!sd_read(sector, buff, count))
        return RES_ERROR;
    else
        return RES_OK;
}

DRESULT disk_write (BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count)
{
    if (pdrv != 0)
        return RES_NOTRDY;
    else if (!sd_type())
        return RES_NOTRDY;

    if (!count)
        return RES_PARERR;

    if (!sd_write(sector, buff, count))
        return RES_ERROR;
    else
        return RES_OK;
}

DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void* buff)
{
    if (pdrv != 0)
        return RES_PARERR;

    if (sd_type() == SD_NONE)
        return RES_NOTRDY;

    switch (cmd) {
    case CTRL_SYNC:
        if (sd_sync())
            return RES_OK;
        break;

    case GET_SECTOR_COUNT: {
        DWORD *count = (DWORD *) buff;
        uint8_t type = sd_type();
        uint8_t csd[16];
        if (!sd_csd(csd))
            break;

        uint8_t structure = csd[0] >> 6;
        if (type == SD_MMC || structure == 0x00) {
            /* CSD version 1.0 (standard capacity card).
            Capacity is encoded as follows:
                Capacity = BlockCount * BlockLength

            with
                BlockCount = (C_SIZE + 1) * Multiplier
                BlockLength = 2 ** READ_BL_LEN
                Multiplier = 2 ** (C_SIZE_MULT + 2)

            Substituting yields
                Capacity = (C_SIZE + 1) * Multiplier * BlockLength

            The Multiplier and BlockLength can be combined to yield
                M = Multiplier * BlockLength
                  = (2 ** (C_SIZE_MULT + 2)) * (2 ** READ_BL_LEN)
                  =  2 ** ((C_SIZE_MULT + 2) + READ_BL_LEN)

            C_SIZE is found in bits 73:62, READ_BL_LEN is found in bits 83:80
            and C_SIZE_MULT is given in bits 49:47.

                127:120     byte 0
                119:112     byte 1
                111:104     byte 2
                103:96      byte 3
                95:88       byte 4
                87:80       byte 5          READ_BL_LEN, partially
                79:72       byte 6          C_SIZE, partially
                71:64       byte 7          C_SIZE
                63:56       byte 8          C_SIZE, partially
                55:48       byte 9          C_SIZE_MULT, partially
                47:40       byte 10         C_SIZE_MULT, partially
                39:32       byte 11
                31:24       byte 12
                23:16       byte 13
                15:8        byte 14
                7:0         byte 15
            */
            uint32_t c_size =
                  ((uint32_t) (csd[6] & 0x03) << 10)    /* 2 bits */
                | ((uint32_t) (csd[7] & 0xFF) << 2)     /* 8 bits */
                | ((uint32_t) (csd[8] & 0xC0) >> 6);    /* 2 bits */


            uint8_t read_bl_len = csd[5] & 0x0F;
            uint8_t c_size_mult =
                  ((uint8_t) (csd[9] & 0x03) << 1)      /* 2 bits */
                | ((uint8_t) (csd[10] & 0x80) >> 7);    /* 1 bit */

            /* Size is returned in multiples of the sector size of 512 bytes,
            which is accounted for in the shift width. */
            uint16_t shift = (c_size_mult + 2) + read_bl_len;
            if (shift < 9)
                /* Reject pathologic cards of less than 512 bytes of capacity */
                return RES_ERROR;

            *count = (c_size + 1) << (shift - 9);
            return RES_OK;
        }
        else if (structure == 0x01) {
            /* CSD version 2.0 (high/extended capacity card).
            Capacity in multiples of 512KiB encoded in bits 69:48:
                Capacity = (C_SIZE + 1) * 512KiB

                127:120     byte 0
                119:112     byte 1
                111:104     byte 2
                103:96      byte 3
                95:88       byte 4
                87:80       byte 5
                79:72       byte 6
                71:64       byte 7          C_SIZE, partially
                63:56       byte 8          C_SIZE
                55:48       byte 9          C_SIZE
                47:40       byte 10
                39:32       byte 11
                31:24       byte 12
                23:16       byte 13
                15:8        byte 14
                7:0         byte 15
            */
            uint32_t c_size =
                  ((uint32_t) (csd[7] & 0x3F) << 16)    /* 6 bits */
                | ((uint32_t) (csd[8] & 0xFF) << 8)     /* 8 bits */
                | ((uint32_t) (csd[9] & 0xFF) << 0);    /* 8 bits */

            /* Size is returned in number of sectors of 512 bytes each. The
            above equation hence becomes
                count = (C_SIZE + 1) * 512KiB / 512B
                      = (C_SIZE + 1) * 1Ki
            */
            *count = (c_size + 1) * 1024;
            return RES_OK;
        }
        } break;

    case GET_SECTOR_SIZE: {
        /* Sector size is fixed to 512 for SD/MMC cards */
        WORD *size = (WORD *) buff;
        *size = 512;
        return RES_OK;
        };


    default:
        return RES_PARERR;
    }

    return RES_ERROR;
}


