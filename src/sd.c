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

/** SD card interface.
The SPI2 instance is used to interface to the SD memory card. It is clocked by
the APB1 bus clock that provides a maximum of 36MHz. In identification mode the
SD card clock must not exceed 400kHz, this is achieved by a prescaler of 1/128
that yields about 280kHz at maximum system clock. Afterwards a clock up to 25MHz
is allowable. The maximum achievable bit rate to match this is 18MHz when using
a prescaler of 1/2:

    SPI_CR1_BR_x    Clock (APB1 @ 36MHz)
    2 1 0

    0 0 0           1/2     18MHz
    0 0 1           1/4      9MHz
    0 1 0           1/8      4.5MHz
    0 1 1           1/16     2.25MHz
    1 0 0           1/32     1.125MHz
    1 0 1           1/64       562.5kHz
    1 1 0           1/128      281.25kHz
    1 1 1           1/256      140.625kHz

Data bits are clocked on the rising edge of the clock, with the clock being in
low state when idle.

DMA1 Channel 4 is used for reception and DMA1 Channel 5 is used for transfer.
*/

#include "cmsis/stm32f10x.h"

#include "timeout.h"

#include "sd.h"

#if SD_INITIALIZATION_TIMEOUT <= 0
#   error Invalid initialization timeout
#endif
#if SD_SELECT_TIMEOUT <= 0
#   error Invalid select timeout
#endif
#if SD_READ_TIMEOUT <= 0
#   error Invalid read timeout
#endif
#if (SD_RETRIES < 0) || (SD_RETRIES > 255)
#   error Invalid number of retries (1 .. 255, 0 for infinite)
#endif



/* Max 400kHz */
#define BR_IDENT                (SPI_CR1_BR_2 | SPI_CR1_BR_1 | SPI_CR1_BR_0)

/* Max 25MHz */
#define BR_TRANS                (0)


/* Bit 0x80 is used to mark application commands, strip it thus */
#define COMMAND_TOKEN(x)        (0x40 | ((x) & 0x3F))

/* CMD0 --> R1
No arguments. */
#define CMD_GO_IDLE_STATE       0


/* CMD1 --> R1
*/
#define CMD_SEND_OP_COND        1

/* CMD8 --> R7
    11:8    supply voltage (VHS)
                0b0001      2.7V .. 3.6V
                0b0010      Low voltage range
    7:0     check pattern
            all other bits reserved ('0')
*/
#define CMD_SEND_IF_COND        8
#define IF_COND_VHS             0x00000F00
#define IF_COND_VHS_33V         0x00000100
#define IF_COND_VHS_LOW         0x00000200
#define IF_COND_CHECK           0x000000FF

/* CMD9 --> R1 + 16 bytes of CSD in a block transfer
No arguments.
*/
#define CMD_SEND_CSD            9

/* CMD12 --> R1b
No arguments
*/
#define CMD_STOP_TRANSMISSION   12

/* CMD16 --> R1
    31:0    block length
*/
#define CMD_SET_BLOCKLEN        16

/* CMD17 --> R1 + data in a block transfer
    32:0    address
*/
#define CMD_READ_SINGLE_BLOCK   17

/* CMD18 --> R1 + data in block transfers
    32:0    address
*/
#define CMD_READ_MULTIPLE_BLOCK 18

/* CMD24 --> R1
    32:0    address
*/
#define CMD_WRITE_BLOCK         24

/* CMD25 --> R1
    32:0    address
*/
#define CMD_WRITE_MULTIPLE_BLOCK    25

/* CMD55 --> R1
No arguments. */
#define CMD_APP_CMD             55


/* CMD58 --> R3
No arguments. */
#define CMD_READ_OCR            58

/* CMD59 --> R1
    0       CRC on ('1') or off ('0')
            all other bits reserved ('0')
*/
#define CMD_CRC_ON_OFF          59
#define CRC_ON                  0x01

/* ACMD41 --> R1
    30      host high capacity support (HCS)
            all other bits reserved ('0')
*/
#define ACMD_SD_SEND_OP_COND    (0x80 | 41)
#define OP_COND_HCS             0x40000000

/* ACMD42 --> R1
    0       Pull-up on CS enabled ('1') or disabled ('0')
*/
#define ACMD_SET_CLR_CARD_DETECT    (0x80 | 42)

/* Single byte response:
    7:0     R1 bits
*/
#define R1_IDLE                 0x01
#define R1_ERASE_RESET          0x02
#define R1_ILLEGAL              0x04
#define R1_CRC                  0x08
#define R1_ERASE_SEQ            0x10
#define R1_ADDRESS              0x20
#define R1_PARAMETER            0x40

/* Preceeded by R1 response:
    15:8    R1 bits
    7:0     R2 bits
*/
#define R2_LOCKED               0x0001
#define R2_UNLOCK               0x0002
#define R2_SKIP                 0x0002
#define R2_ERROR                0x0004
#define R2_CC_ERROR             0x0008
#define R2_ECC                  0x0010
#define R2_WP                   0x0020
#define R2_PARAMETER            0x0040
#define R2_RANGE                0x0080

/* Preceeded by R1 response:
    39:32   R1 bits
    31:0    OCR bits
*/
#define R3_VHS_LOW              0x00000080
#define R3_VHS_27V              0x00008000
#define R3_VHS_28V              0x00010000
#define R3_VHS_29V              0x00020000
#define R3_VHS_30V              0x00040000
#define R3_VHS_31V              0x00080000
#define R3_VHS_32V              0x00100000
#define R3_VHS_33V              0x00200000
#define R3_VHS_34V              0x00400000
#define R3_VHS_35V              0x00800000
#define R3_S18A                 0x01000000
#define R3_UHS2                 0x20000000
#define R3_CCS                  0x40000000
#define R3_READY                0x80000000

/* Followed by operating conditions:
    15:8    R1 bits
    11:8    supply voltage (VHS)
                0b0001      2.7V .. 3.6V
                0b0010      Low voltage range
    7:0     check pattern
*/
#define R7_CHECK                IF_COND_CHECK
#define R7_VHS                  IF_COND_VHS
#define R7_VHS_33V              IF_COND_VHS_33V
#define R7_VHS_LOW              IF_COND_VHS_LOW


/* Data tokens */
#define DATA_TOKEN(x)           (0xFC | (x))
#define DATA_SINGLE_READ        0x02
#define DATA_MULTI_READ         0x02
#define DATA_SINGLE_WRITE       0x02
#define DATA_MULTI_WRITE        0x00
#define DATA_STOP_TRAN          0x01

#define DATA_RESP_TOKEN(x)      ((x) & 0x1F)
#define RESP_ACCEPTED           0x05
#define RESP_CRC_ERROR          0x0B
#define RESP_WRITE_ERROR        0x0D


static uint8_t card_type;
static bool high_density;
static bool crc_enabled;

static uint8_t crc7(const uint8_t *data, uint16_t n)
{
    /* 7 bit CRC for commands.
    The used polynomial is
        0x89 = 0b10001001 = x**7 + x**3 + 1
    */
    static const uint8_t table[256] =
    {
        0x00, 0x09, 0x12, 0x1B, 0x24, 0x2D, 0x36, 0x3F,
        0x48, 0x41, 0x5A, 0x53, 0x6C, 0x65, 0x7E, 0x77,
        0x19, 0x10, 0x0B, 0x02, 0x3D, 0x34, 0x2F, 0x26,
        0x51, 0x58, 0x43, 0x4A, 0x75, 0x7C, 0x67, 0x6E,
        0x32, 0x3B, 0x20, 0x29, 0x16, 0x1F, 0x04, 0x0D,
        0x7A, 0x73, 0x68, 0x61, 0x5E, 0x57, 0x4C, 0x45,
        0x2B, 0x22, 0x39, 0x30, 0x0F, 0x06, 0x1D, 0x14,
        0x63, 0x6A, 0x71, 0x78, 0x47, 0x4E, 0x55, 0x5C,
        0x64, 0x6D, 0x76, 0x7F, 0x40, 0x49, 0x52, 0x5B,
        0x2C, 0x25, 0x3E, 0x37, 0x08, 0x01, 0x1A, 0x13,
        0x7D, 0x74, 0x6F, 0x66, 0x59, 0x50, 0x4B, 0x42,
        0x35, 0x3C, 0x27, 0x2E, 0x11, 0x18, 0x03, 0x0A,
        0x56, 0x5F, 0x44, 0x4D, 0x72, 0x7B, 0x60, 0x69,
        0x1E, 0x17, 0x0C, 0x05, 0x3A, 0x33, 0x28, 0x21,
        0x4F, 0x46, 0x5D, 0x54, 0x6B, 0x62, 0x79, 0x70,
        0x07, 0x0E, 0x15, 0x1C, 0x23, 0x2A, 0x31, 0x38,
        0x41, 0x48, 0x53, 0x5A, 0x65, 0x6C, 0x77, 0x7E,
        0x09, 0x00, 0x1B, 0x12, 0x2D, 0x24, 0x3F, 0x36,
        0x58, 0x51, 0x4A, 0x43, 0x7C, 0x75, 0x6E, 0x67,
        0x10, 0x19, 0x02, 0x0B, 0x34, 0x3D, 0x26, 0x2F,
        0x73, 0x7A, 0x61, 0x68, 0x57, 0x5E, 0x45, 0x4C,
        0x3B, 0x32, 0x29, 0x20, 0x1F, 0x16, 0x0D, 0x04,
        0x6A, 0x63, 0x78, 0x71, 0x4E, 0x47, 0x5C, 0x55,
        0x22, 0x2B, 0x30, 0x39, 0x06, 0x0F, 0x14, 0x1D,
        0x25, 0x2C, 0x37, 0x3E, 0x01, 0x08, 0x13, 0x1A,
        0x6D, 0x64, 0x7F, 0x76, 0x49, 0x40, 0x5B, 0x52,
        0x3C, 0x35, 0x2E, 0x27, 0x18, 0x11, 0x0A, 0x03,
        0x74, 0x7D, 0x66, 0x6F, 0x50, 0x59, 0x42, 0x4B,
        0x17, 0x1E, 0x05, 0x0C, 0x33, 0x3A, 0x21, 0x28,
        0x5F, 0x56, 0x4D, 0x44, 0x7B, 0x72, 0x69, 0x60,
        0x0E, 0x07, 0x1C, 0x15, 0x2A, 0x23, 0x38, 0x31,
        0x46, 0x4F, 0x54, 0x5D, 0x62, 0x6B, 0x70, 0x79,
    };

    uint8_t crc = 0;
    while (n--)
         crc = table[ (crc << 1) ^ *data++ ];
    return crc;
}


uint16_t crc16(const uint8_t *data, uint16_t n) {
    /* 16 bit CRC for block transfers.
    The used polynomial is
        0x1021 = 0b1000000100001 = x**16 + x**12 + x**5 + 1
    */
    uint16_t crc = 0;
    while (n--) {
        crc  = (crc >> 8) | (crc << 8);
        crc ^= *data++;
        crc ^= (crc >> 4) & 0x000F;
        crc ^= crc << 12;
        crc ^= (crc & 0x00FF) << 5;
    }

    return crc;
}


static void flush(void)
{
    /* Wait for SPI to complete */
    /* BUG NOTE TODO */
    while (!(SPI2->SR & SPI_SR_TXE));
    while (SPI2->SR & SPI_SR_BSY);
    (void) SPI2->DR;
}

static bool ready(uint32_t timeout)
{
    for (timeout_t t = tot_set(timeout); !tot_expired(t); ) {
        /* Send dummy 0xFF stream */
        while (!(SPI2->SR & SPI_SR_TXE));
        SPI2->DR = 0xFF;

        /* Receive until a steady high level is detected */
        while (!(SPI2->SR & SPI_SR_RXNE));
        uint8_t response = SPI2->DR;
        if (response == 0xFF)
            return true;
    }

    /* Timeout expired */
    return false;
}

static bool select(bool s)
{
    /* When selecting the card it may resume signalling busy (MISO low) one
    clock after asserting CS. When deselecting it another clock is necessary to
    make it release the MISO line. */
    flush();
    if (s) {
        /* Assert CS and discard first byte received */
        GPIOB->BSRR = GPIO_BSRR_BR12;
        if (ready(SD_SELECT_TIMEOUT))
            return true;

        select(false);
        return false;
    }
    else {
        /* Revoke CS and provide dummy clocks to release MISO line */
        GPIOB->BSRR = GPIO_BSRR_BS12;
        SPI2->DR = 0xFF;
        return true;
    }
}

/* Loop is faster than setting up DMA here */
static void txbuf(const uint8_t *data, uint16_t n)
{
    while (n--) {
        while (!(SPI2->SR & SPI_SR_TXE));
        SPI2->DR = *data++;
    }
}

static void rxbuf(uint8_t *data, uint16_t n)
{
    flush();

    /* Receive bytes one by one to avoid data overrun */
    while (n--) {
        SPI2->DR = 0xFF;
        while (!(SPI2->SR & SPI_SR_RXNE));
        *data++ = SPI2->DR;
    }
}

static uint8_t rxtoken(uint32_t timeout)
{
    flush();
    for (timeout_t t = tot_set(timeout); !tot_expired(t); ) {
        /* Send dummy 0xFF */
        SPI2->DR = 0xFF;

        /* Receive token */
        while (!(SPI2->SR & SPI_SR_RXNE));
        uint8_t response = SPI2->DR;
        if (response != 0xFF)
            return response;
    }

    /* Timeout */
    return 0xFF;
}

static void txtoken(uint8_t token)
{
    flush();

    /* Send dummy 0xFF to account for Twr */
    SPI2->DR = 0xFF;
    while (!(SPI2->SR & SPI_SR_TXE));

    SPI2->DR = token;
    while (!(SPI2->SR & SPI_SR_TXE));
}

/* n is half the size of data, i.e. number of words */
static inline void reorder(uint8_t *data, uint16_t n)
{
    /* Reorder received data as DMA has written it endian-swapped */
    uint8_t *p = data;
    while (n--) {
        uint8_t byte = *p++;
        p[-1] = *p;
        *p++ = byte;
    }
}

/* Block length n must be a multiple of 2. */
static bool rxblock(uint8_t *data, uint16_t n)
{
#ifdef SD_HARDWARE_CRC
    /* Switch to 16bit mode with CRC */
    SPI2->CR1 &= ~SPI_CR1_SPE;
    SPI2->SR = ~SPI_SR_CRCERR;
    SPI2->CR1 |= SPI_CR1_DFF | SPI_CR1_CRCEN | SPI_CR1_SPE;
    n /= 2;

    /* Disconnect MOSI pin from SPI peripheral to generate constant '1' in
    order to transfer 0xFF stuff bytes. Otherwise the SPI will automatically
    transfer a CRC that is accidentally interpreted as a new command by the
    SD card. */
    GPIOB->CRH &= ~GPIO_CRH_CNF15_1;
#endif

    /* Set up DMA transfers */
    uint16_t dummy = 0xFFFF;
    DMA1_Channel5->CMAR = (uint32_t) &dummy;
    DMA1_Channel5->CNDTR = n;
    DMA1_Channel4->CMAR = (uint32_t) data;
    DMA1_Channel4->CNDTR = n;
    __DSB();

#ifdef SD_HARDWARE_CRC
    DMA1_Channel5->CCR =
        DMA_CCR5_MSIZE_0 |
        DMA_CCR5_PSIZE_0 |
        DMA_CCR5_DIR |
        DMA_CCR5_EN;
    DMA1_Channel4->CCR =
        DMA_CCR4_MSIZE_0 |
        DMA_CCR4_PSIZE_0 |
        DMA_CCR4_MINC |
        DMA_CCR4_EN;
#else
    DMA1_Channel5->CCR =
        DMA_CCR5_DIR |
        DMA_CCR5_EN;
    DMA1_Channel4->CCR =
        DMA_CCR4_MINC |
        DMA_CCR4_EN;
#endif

    /* Transfer */
    DMA1->IFCR = DMA_IFCR_CTCIF5 | DMA_IFCR_CTCIF4;
    SPI2->CR2 |= SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN;

    /* Wait for DMA to complete */
    while ( (DMA1->ISR & (DMA_ISR_TCIF5 | DMA_ISR_TCIF4)) != (DMA_ISR_TCIF5 | DMA_ISR_TCIF4) );

    /* Wait for SPI to complete and halt DMA */
    while (!(SPI2->SR & SPI_SR_TXE));
    while (SPI2->SR & SPI_SR_BSY);
    SPI2->CR2 &= ~(SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN);
    DMA1_Channel5->CCR = 0;
    DMA1_Channel4->CCR = 0;
    __DSB();

#ifdef SD_HARDWARE_CRC
    /* DMA will take care of CRCNEXT */
    const uint16_t sr = SPI2->SR;

    /* Switch back to 8bit mode */
    SPI2->CR1 &= ~SPI_CR1_SPE;
    SPI2->CR1 &= ~(SPI_CR1_DFF | SPI_CR1_CRCEN);
    SPI2->CR1 |= SPI_CR1_SPE;

    /* Re-connect MOSI to SPI peripheral */
    GPIOB->CRH |= GPIO_CRH_CNF15_1;

    reorder(data, n);
    return !(sr & SPI_SR_CRCERR);
#else
    uint8_t crcbuf[2];
    rxbuf(crcbuf, 2);
    uint16_t sdcrc = (crcbuf[0] << 8) | crcbuf[1];
    return sdcrc == crc16(data, n);
#endif
}

static void txblock(const uint8_t *data, uint16_t n)
{
    /* Set up DMA transfers */
    DMA1_Channel5->CMAR = (uint32_t) data;
    DMA1_Channel5->CNDTR = n;
    __DSB();

    DMA1_Channel5->CCR =
        DMA_CCR4_MINC |
        DMA_CCR5_DIR |
        DMA_CCR5_EN;

    /* Transfer */
    DMA1->IFCR = DMA_IFCR_CTCIF5;
    SPI2->CR2 |= SPI_CR2_TXDMAEN;

    /* Wait for DMA to complete */
    while ( (DMA1->ISR & DMA_ISR_TCIF5) != (DMA_ISR_TCIF5) );

    /* Wait for SPI to complete and halt DMA */
    while (!(SPI2->SR & SPI_SR_TXE));
    while (SPI2->SR & SPI_SR_BSY);
    SPI2->CR2 &= ~SPI_CR2_TXDMAEN;
    DMA1_Channel5->CCR = 0;
    __DSB();

    /* Cannot use hardware CRC because the const data array must not be
    permutated. */
    uint16_t crc = crc16(data, n);
    uint8_t crcbuf[2] = {
        (crc >> 8) & 0xFF,
        (crc >> 0) & 0xFF,
    };
    txbuf(crcbuf, 2);
}

static uint8_t command(uint8_t index, uint32_t arg)
{
    ready(SD_SELECT_TIMEOUT);

    if (index & 0x80) {
        /* Announce application command if needed */
        index &= ~0x80;
        if (command(CMD_APP_CMD, 0) != R1_IDLE)
            return 0xFF;
    }

    /* Compose command */
    uint8_t buf[6] = {
        COMMAND_TOKEN(index),           /* start bit | command index */
        arg >> 24,                      /* arguments */
        arg >> 16,
        arg >> 8,
        arg >> 0,
        0x01                            /* stop bit */
    };

    /* Add 7 bit CRC to stop bit */
    buf[5] |= (crc7(buf, sizeof(buf)/sizeof(*buf) - 1) << 1);

#if SD_RETRIES
        for (uint8_t retry = SD_RETRIES; retry > 0; retry--) {
#else
        for (;;) {
#endif
        /* Transfer command and wait for completion */
        txbuf(buf, sizeof(buf)/sizeof(*buf));

        if (index == CMD_STOP_TRANSMISSION) {
            /* Discard last byte of transmission */
            while (!(SPI2->SR & SPI_SR_TXE));
            SPI2->DR = 0xFF;
        }

        /* Receive response after at most 10 bytes (timing Ncr) */
        flush();
        uint8_t response;
        for (uint8_t i = 0; i < 10; i++) {
            /* Send dummy 0xFF */
            SPI2->DR = 0xFF;

            /* Receive response */
            while (!(SPI2->SR & SPI_SR_RXNE));
            response = SPI2->DR;
            if (response & 0x80)
                continue;

            /* Retry on CRC error */
            if (response & R1_CRC)
                break;

            return response;
        }

        /* Re-select and retry */
        select(false);
        select(true);
    }

    return 0xFF;
}

static uint32_t response(void)
{
    uint8_t buf[4];
    rxbuf(buf, sizeof(buf)/sizeof(*buf));
    return
        ((uint32_t) buf[0] << 24) |
        ((uint32_t) buf[1] << 16) |
        ((uint32_t) buf[2] << 8) |
        ((uint32_t) buf[3] << 0);
}


bool sd_read(uint32_t sector, uint8_t *data, uint16_t n)
{
    if (!n)
        return true;

    if (!high_density)
        sector *= 512;

    if (!select(true))
        return false;

    if (n > 1) {
        /* Multiple sector read */
#if SD_RETRIES
        for (uint8_t retry = SD_RETRIES; retry > 0; retry--) {
#else
        for (;;) {
#endif
            if (command(CMD_READ_MULTIPLE_BLOCK, sector) != 0)
                break;

            uint8_t *p = data;
            for (uint16_t i = 0; i < n; i++) {
                if (rxtoken(SD_READ_TIMEOUT) == DATA_TOKEN(DATA_SINGLE_READ)) {
                    if (rxblock(p, 512)) {
                        p += 512;
                        continue;
                    }
                }

                p = 0;
                break;
            }

            if (command(CMD_STOP_TRANSMISSION, 0) != 0)
                break;

            if (p) {
                select(false);
                return true;
            }

            /* Retry */
            select(false);
            select(true);
        }
    }
    else {
        /* Single sector read */
#if SD_RETRIES
        for (uint8_t retry = SD_RETRIES; retry > 0; retry--) {
#else
        for (;;) {
#endif
            if (command(CMD_READ_SINGLE_BLOCK, sector) != 0)
                break;

            if (rxtoken(SD_READ_TIMEOUT) == DATA_TOKEN(DATA_SINGLE_READ)) {
                if (rxblock(data, 512)) {
                    select(false);
                    return true;
                }
            }

            /* Retry */
            select(false);
            select(true);
        }
    }

    /* Abort */
    select(false);
    return false;
}

bool sd_write(uint32_t sector, const uint8_t *data, uint16_t n)
{
    if (!n)
        return true;

    if (!high_density)
        sector *= 512;

    if (!select(true))
        return false;

    if (n > 1) {
        /* Multiple sector write */
#if SD_RETRIES
        for (uint8_t retry = SD_RETRIES; retry > 0; retry--) {
#else
        for (;;) {
#endif
            if (command(CMD_WRITE_MULTIPLE_BLOCK, sector) != 0)
                break;

            const uint8_t *p = data;
            for (uint16_t i = 0; i < n; i++) {
                if (ready(SD_WRITE_TIMEOUT)) {
                    txtoken(DATA_TOKEN(DATA_MULTI_WRITE));
                    txblock(p, 512);
                    if (DATA_RESP_TOKEN(rxtoken(SD_WRITE_TIMEOUT)) == RESP_ACCEPTED) {
                        p += 512;
                        continue;
                    }
                }

                p = 0;
                break;
            }

            if (p) {
                if (ready(SD_WRITE_TIMEOUT)) {
                    txtoken(DATA_TOKEN(DATA_STOP_TRAN));
                    if (ready(SD_WRITE_TIMEOUT)) {
                        select(false);
                        return true;
                    }
                }
            }

            /* Retry */
            select(false);
            select(true);
        }
    }
    else {
        /* Single sector write */
#if SD_RETRIES
        for (uint8_t retry = SD_RETRIES; retry > 0; retry--) {
#else
        for (;;) {
#endif
            if (command(CMD_WRITE_BLOCK, sector) != 0)
                break;

            if (ready(SD_WRITE_TIMEOUT)) {
                txtoken(DATA_TOKEN(DATA_SINGLE_WRITE));
                txblock(data, 512);
                if (DATA_RESP_TOKEN(rxtoken(SD_WRITE_TIMEOUT)) == RESP_ACCEPTED) {
                    if (ready(SD_WRITE_TIMEOUT)) {
                        select(false);
                        return true;
                    }
                }
            }

            /* Retry */
            select(false);
            select(true);
        }
    }

    /* Abort */
    select(false);
    return false;
}

bool sd_ocr(uint32_t *ocr)
{
    if (!card_type)
        return false;

    if (!select(true))
        return false;

    uint8_t result = command(CMD_READ_OCR, 0);
    if (result != 0) {
        select(false);
        return false;
    }

    *ocr = response();
    select(false);
    return true;
}

bool sd_csd(uint8_t *csd)
{
    if (!card_type)
        return false;

    if (!select(true))
        return false;

    uint8_t result = command(CMD_SEND_CSD, 0);
    if (result != 0) {
        select(false);
        return false;
    }

    if (rxblock(csd, 16)) {
        select(false);
        return true;
    }
    else {
        select(false);
        return false;
    }
}

bool sd_sync(void)
{
    if (!select(true))
        return false;

    select(false);
    return true;
}

uint8_t sd_type(void)
{
    return card_type;
}

void sd_enable(bool enable)
{
    if (enable) {
        /* Turn on power */
        GPIOA->BSRR = GPIO_BSRR_BR8;

        /* Set up SPI interface */
        GPIOB->ODR |= GPIO_ODR_ODR12;
        GPIOB->CRH &= ~GPIO_CRH_CNF12;                      /* NSS */
        GPIOB->CRH |= GPIO_CRH_MODE12_0 | GPIO_CRH_MODE12_1;

        GPIOB->CRH &= ~GPIO_CRH_CNF13;                      /* SCK */
        GPIOB->CRH |= GPIO_CRH_CNF13_1 | GPIO_CRH_MODE13_0 | GPIO_CRH_MODE13_1;

        GPIOB->CRH &= ~(GPIO_CRH_CNF14 | GPIO_CRH_MODE14);  /* MISO */
        GPIOB->CRH |= GPIO_CRH_CNF14_1;
        GPIOB->ODR |= GPIO_ODR_ODR14;

        GPIOB->CRH &= ~GPIO_CRH_CNF15;                      /* MOSI */
        GPIOB->CRH |= GPIO_CRH_CNF15_1 | GPIO_CRH_MODE15_0 | GPIO_CRH_MODE15_1;

        /* Set MOSI.
        The MOSI pin is normally connected to the SPI peripheral as alternate
        function. When using the SPI's CRC, however, it is disconnected and
        operated as GPIO instead to output a constant stream of '1' bits during
        the transmission of the stuff bytes.
        Otherwise the SPI peripheral would finally transmit the computed CRC
        that could then be mistaken for a command by the SD card. */
        GPIOB->BSRR = GPIO_BSRR_BS15;
    }
    else {
        flush();

        /* Switch interface to inputs with pull-down */
        GPIOB->CRH &= ~(GPIO_CRH_MODE12 | GPIO_CRH_MODE13 | GPIO_CRH_MODE14 | GPIO_CRH_MODE15);
        GPIOB->BSRR = GPIO_BSRR_BR12 | GPIO_BSRR_BR13 | GPIO_BSRR_BR14 | GPIO_BSRR_BR15;
        GPIOB->CRH &= ~(GPIO_CRH_CNF12_0 | GPIO_CRH_CNF13_0 | GPIO_CRH_CNF14_0 | GPIO_CRH_CNF15_0);
        GPIOB->CRH |= GPIO_CRH_CNF12_1 | GPIO_CRH_CNF13_1 | GPIO_CRH_CNF14_1 | GPIO_CRH_CNF15_1;

        /* Switch off power supply */
        GPIOA->BSRR = GPIO_BSRR_BS8;
    }
}

bool sd_identify(void)
{
    uint8_t result;
    card_type = SD_NONE;
    crc_enabled = false;
    high_density = false;

    /* Wait for SPI to finish and switch to slow clock */
    flush();
    SPI2->CR1 &= ~SPI_CR1_SPE;
    SPI2->CR1 &= ~SPI_CR1_BR;
    SPI2->CR1 |= SPI_CR1_SPE | BR_IDENT;

    /* Send >= 80 dummy clocks */
    for (uint8_t i = 0; i < 10; i++) {
        while ( !(SPI2->SR & SPI_SR_TXE) );
        SPI2->DR = 0xFF;
    }

    /* Reset card */
    select(true);
    result = command(CMD_GO_IDLE_STATE, 0);
    if (result != R1_IDLE)
        goto no_card;

    /* Try to turn on CRC */
    result = command(CMD_CRC_ON_OFF, CRC_ON);
    if ( !(result & ~(R1_ILLEGAL | R1_IDLE)) ) {
        if (result == R1_IDLE)
            crc_enabled = true;
    }
    else {
        goto no_card;
    }

    /* Test for SD physical layer v2.00 */
    result = command(CMD_SEND_IF_COND, IF_COND_VHS_33V | 0xBC);
    if (result == R1_IDLE) {
        uint32_t r7 = response();
        if ((r7 & R7_CHECK) != 0xBC)
            /* Invalid check pattern */
            goto no_card;
        else if ((r7 & R7_VHS) != R7_VHS_33V)
            /* Voltage range not supported */
            goto no_card;

        card_type = SD_SDv2;
    }

    /* Verify operating conditions */
    result = command(CMD_READ_OCR, 0);
    if (result == R1_IDLE) {
        uint32_t ocr = response();
        if (!(ocr & R3_VHS_33V))
            /* Voltage range not supported */
            goto no_card;
    }
    else {
        goto no_card;
    }

    /* Initialize card */
    if (card_type == SD_SDv2) {
        timeout_t t = tot_set(SD_INITIALIZATION_TIMEOUT);
        while ((result = command(ACMD_SD_SEND_OP_COND, OP_COND_HCS)) != 0) {
            if (tot_expired(t)) {
                /* Might have been wrongly identified as SDv2 */
                card_type = SD_NONE;
                break;
            }
        }
    }

    if (card_type == SD_SDv2) {
        /* Negotiate density with v2 SD card */
        if (command(CMD_READ_OCR, 0) != 0)
            goto no_card;

        uint32_t ocr = response();
        if (ocr & R3_CCS)
            high_density = true;
    }
    else {
        result = command(ACMD_SD_SEND_OP_COND, 0);
        if (result == R1_IDLE) {
            /* Initialize v1 SD card */
            timeout_t t = tot_set(SD_INITIALIZATION_TIMEOUT);
            while (command(ACMD_SD_SEND_OP_COND, 0) != 0) {
                if (tot_expired(t))
                    goto no_card;
            }

            card_type = SD_SDv1;
        }
        else if (result & R1_ILLEGAL) {
            /* Try to initialize MMC */
            timeout_t t = tot_set(SD_INITIALIZATION_TIMEOUT);
            while (command(CMD_SEND_OP_COND, 0) != 0) {
                if (tot_expired(t))
                    goto no_card;
            }

            card_type = SD_MMC;
        }
        else if (result != 0) {
            /* Unsupported card */
            goto no_card;
        }

        /* Set block length */
        result = command(CMD_SET_BLOCKLEN, 512);
        if (result != 0)
            goto no_card;
    }

    select(false);

    /* Wait for SPI to finish and switch to fast clock */
    flush();
    SPI2->CR1 &= ~SPI_CR1_SPE;
    SPI2->CR1 &= ~SPI_CR1_BR;
    SPI2->CR1 |= BR_TRANS;
    SPI2->CR1 |= SPI_CR1_SPE;
    SPI2->DR = 0xFF;
    return true;

no_card:
    card_type = 0;
    select(false);
    return false;
}



void sd_prepare(void)
{
    card_type = SD_NONE;


    /* Port */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    __DSB();
    GPIOA->ODR |= GPIO_ODR_ODR8;                        /* /SDEN */
    GPIOA->CRH &= ~(GPIO_CRH_CNF8 | GPIO_CRH_MODE8);
    GPIOA->CRH |= GPIO_CRH_MODE8_1;

    /* NSS, MISO, MOSI, SCK with pull-down */
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    __DSB();
    GPIOB->CRH &= ~(GPIO_CRH_MODE12 | GPIO_CRH_MODE13 | GPIO_CRH_MODE14 | GPIO_CRH_MODE15);
    GPIOB->BSRR = GPIO_BSRR_BR12 | GPIO_BSRR_BR13 | GPIO_BSRR_BR14 | GPIO_BSRR_BR15;
    GPIOB->CRH &= ~(GPIO_CRH_CNF12_0 | GPIO_CRH_CNF13_0 | GPIO_CRH_CNF14_0 | GPIO_CRH_CNF15_0);
    GPIOB->CRH |= GPIO_CRH_CNF12_1 | GPIO_CRH_CNF13_1 | GPIO_CRH_CNF14_1 | GPIO_CRH_CNF15_1;

    /* SPI */
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
    __DSB();
    NVIC_DisableIRQ(SPI2_IRQn);
    SPI2->CRCPR = 0x1021;
    SPI2->CR2 = 0;
    SPI2->CR1 =
        SPI_CR1_SSM |
        SPI_CR1_SSI |
        SPI_CR1_SPE |
        BR_IDENT |
        SPI_CR1_MSTR;

    /* DMA */
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    __DSB();
    NVIC_DisableIRQ(DMA1_Channel4_IRQn);
    NVIC_DisableIRQ(DMA1_Channel5_IRQn);
    DMA1_Channel4->CPAR = (uint32_t) &SPI2->DR;
    DMA1_Channel4->CCR = 0;
    DMA1_Channel5->CPAR = (uint32_t) &SPI2->DR;
    DMA1_Channel5->CCR = DMA_CCR5_DIR;
}
