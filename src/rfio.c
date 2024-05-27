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

/** RFIO.
*/

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "cmsis/stm32f10x.h"

#include "config.h"
#include "timeout.h"

#include "sx1231.h"
#include "rfio.h"

static uint8_t rssi = 0;
static uint8_t nodeid = 0;

/* Watchdog */
static timeout_t timeout;

static void select(bool s)
{
    /* Timing:
        /CS to first clock rising edge      t_ncs = 30ns
        last clock falling edge to CS       t_cs = 100ns
    */
    if (s) {
        GPIOA->BSRR = GPIO_BSRR_BR4;
        (void) SPI1->DR;
    }
    else {
        /* Wait for SPI to complete */
        while (!(SPI1->SR & SPI_SR_TXE));
        while (SPI1->SR & SPI_SR_BSY);
        GPIOA->BSRR = GPIO_BSRR_BS4;
    }
}

static void write(uint8_t address, uint8_t value)
{
    select(true);
    SPI1->DR = 0x80 | address;

    while (!(SPI1->SR & SPI_SR_TXE));
    SPI1->DR = value;

    select(false);
}

static void writefifo(const uint8_t *data, uint8_t n)
{
    select(true);
    SPI1->DR = 0x80 | RegFifo;

    while (n--) {
        while (!(SPI1->SR & SPI_SR_TXE));
        SPI1->DR = *data++;
    }

    select(false);
}

static uint8_t read(uint8_t address)
{
    select(true);
    SPI1->DR = address & 0x7F;
    while (!(SPI1->SR & SPI_SR_RXNE));
    (void) SPI1->DR;

    SPI1->DR = 0x00;
    while (!(SPI1->SR & SPI_SR_RXNE));
    uint8_t rx = SPI1->DR;

    select(false);
    return rx;
}

static void readfifo(uint8_t *data, uint8_t n)
{
    select(true);
    SPI1->DR = RegFifo & 0x7F;
    while (!(SPI1->SR & SPI_SR_RXNE));
    (void) SPI1->DR;

    /* Receive bytes one by one to avoid data overrun */
    while (n--) {
        SPI1->DR = 0x00;
        while (!(SPI1->SR & SPI_SR_RXNE));
        *data++ = SPI1->DR;
    }

    select(false);
}

static void flushfifo(void)
{
    /* Shortcut */
    write(RegIrqFlags2, IrqFlags2_FifoOverrun);
}

static void chmode(uint8_t mode)
{
    mode |= OpMode_ListenAbort;
    do {
        write(RegOpMode, mode);
        mode &= ~OpMode_ListenAbort;
        while ( !(read(RegIrqFlags1) & IrqFlags1_ModeReady) );
    } while ( (read(RegOpMode) & OpMode_Mode) != (mode & OpMode_Mode) );
}

void rf_calibrate(void)
{
    chmode(OpMode_Mode_Stdby);
    write(RegOsc1, RegOsc1_RcCalStart);
    while ( (read(RegOsc1) & RegOsc1_RcCalDone) != RegOsc1_RcCalDone);
}

void rf_frequency(uint32_t f)
{
    /* Frequency is set via RegFrfMsb..Lsb and to be calculated as follows.
    The frequency is updated upon writing the least significant byte to the
    RegFrfLsb register.
        F_step = F_xtal / 2**19     (61Hz for 32MHz XTAL)
        F_rf = Fstep * RegFrf

        RegFrf = Frf / F_step
               = F_rf / F_xtal * 2**19

    For Fxtal = 32MHz this gives nominal register values:
        315MHz          0x4EC000
        433MHz          0x6C4000
        868MHz          0xD90000
        915MHz          0xE4C000
    */
    uint8_t mode = read(RegOpMode);
    if ( (mode & OpMode_Mode) == OpMode_Mode_Tx )
        chmode(OpMode_Mode_Rx);

    if (f < 290000000)
        f = 290000000;
    else if (f > 1020000000)
        f = 1020000000;

    uint32_t reg = ((uint64_t) f << 19) / RF_XTAL;
    write(RegFrfMsb, reg >> 16);
    write(RegFrfMid, reg >> 8);
    write(RegFrfLsb, reg >> 0);

    if ( (mode & OpMode_Mode) == OpMode_Mode_Rx )
        chmode(OpMode_Mode_Fs);
    chmode(OpMode_Mode_Stdby);
}

static uint8_t bwflags(uint32_t bandwidth)
{
    /* For FSK the receiver bandwidth is calculated as
        B = F_xtal / (RxBwMant * 2**(RxBwExp + 2))
          = F_xtal / (RxBwMant * 2**RxBwExp * 4)

        RxBwMant * 2**RxBwExp = F_xtal / 4 / B

    The receiver bandwidth must be greater than half the bitrate. */
    if (bandwidth < 2600)
        bandwidth = 2600;
    else if (bandwidth > 500000)
        bandwidth = 500000;

    uint16_t div = RF_XTAL / 4 / bandwidth;

    /* Find exponent */
    uint8_t exp;
    for (exp = 0; div >= 16; exp++)
        div >>= 1;
    exp -= 1;
    div <<= 1;

    /* Find mantissa.
    The mantissa bits in RxBw align with the actual value:
                                Bits            Mantissa
        RxBw_RxBwMant_16        0b00000000      16 = 0b10000
        RxBw_RxBwMant_20        0x00001000      20 = 0b10100
        RxBw_RxBwMant_24        0x00010000      24 = 0b11000
    */
    uint8_t mant = (div & ~0x08) << 1;
    return (mant & RxBw_RxBwMant) | RxBw_RxBwExp_X(exp);
}

static void afcreset(void)
{
    write(RegAfcFei, read(RegAfcFei) | AfcFei_AfcClear);
    write(RegPacketConfig2, read(RegPacketConfig2) | PacketConfig2_RestartRx);
}

void rf_rxbw(uint32_t bandwidth)
{
    /* Use default DC canceller cut-off frequency at 4% of bandwidth */
    write(RegRxBw, RxBw_DccFreq_4 | bwflags(bandwidth));
}

void rf_afcbw(uint32_t bandwidth)
{
    /* Use default DC canceller cut-off frequency at 4% of bandwidth */
    write(RegAfcBw, RxBw_DccFreq_4 | bwflags(bandwidth));
}

void rf_fdev(uint32_t dev)
{
    /* Frequency deviation is determined in multiples of the PLL resolution:
        F_dev = F_xtal / 2**19 * Fdev
        Fdev = F_dev * 2**19 / F_xtal
    */

    uint32_t reg = ((uint64_t) dev << 19) / RF_XTAL;
    write(RegFdevMsb, reg >> 8);
    write(RegFdevLsb, reg);
}

void rf_bitrate(uint16_t rate)
{
    /* The bit rate in RegBitrateMsb..Lsb is directly derived from the crystal
    oscillator:
        F_bitrate = F_xtal / Bitrate

    Typical bit rates are:
        4.8k            0x1A0B

    The bitrate must be less than twice the receiver bandwidth. */
    uint16_t reg = RF_XTAL / rate;
    write(RegBitrateMsb, reg >> 8);
    write(RegBitrateLsb, reg);
}

void rf_power(int8_t power)
{
    /* There are three power amplifier configurations in the SX1231:
        PA0         -18 .. +13dBm
        PA1         -18 .. +13dBm
        PA1 + PA2    +2 .. +17dBm

    Both PA0 and PA1 deliver power according to
        L = -18dBm + OutputPower
        with OutputPower = 0 .. 31

    When both enabled PA1 and PA2 together deliver
        L = -14dBm + OutputPower
        with OutputPower = 16 .. 31
    */

    if (power > 13) {
#ifdef SX1231H
        /* Use PA1+PA2 above 13dBm */
        if (power > 17)
            power = 17;

        /* 2 .. 17  -->  16 .. 31 */
        write(RegPaLevel, PaLevel_Pa1On | PaLevel_Pa2On |
            ((power + 14) & PaLevel_OutputPower));
#else
        /* Only PA0 available */
        power = 13;
#endif
    }

    if (power >= -18)
        /* -18 .. 13  -->  0 .. 31 */
        write(RegPaLevel, PaLevel_Pa0On |
            ((power + 18) & PaLevel_OutputPower));
    else
        /* Shutdown */
        write(RegPaLevel, 0);
}

void rf_sensitivity(int16_t sens)
{
    /* RSSI threshold is computed according to
        Lrssi = -1dB * RssiValue / 2 */
    if (sens < -127)
        sens = -127;
    else if (sens > 0)
        sens = 0;
    write(RegRssiThresh, -2 * sens);
}

int16_t rf_rssi(void)
{
    /* RssiValue is resolved in steps of 0.5dBm. Sacrifice this resolution in
    favor of uniform interface. */
    return -(int16_t) rssi / 2;
}

int32_t rf_fei(void)
{
    int16_t fei = ((uint16_t) read(RegFeiMsb) << 8) | read(RegFeiLsb);
    return (int32_t) ((int16_t) fei) * 61;
}

void rf_meshid(uint16_t id)
{
    write(RegSyncValue(0), id >> 8);
    write(RegSyncValue(1), id);
}

void rf_nodeid(uint8_t id)
{
    nodeid = id;
    write(RegNodeAdrs, nodeid);
}

void rf_promiscuous(bool p)
{
    /* Align with rf_prepare() */
    uint8_t reg =
        PacketConfig1_PacketFormat_Variable |
        PacketConfig1_DcFree_Manchester |
        PacketConfig1_CrcOn;

    if (p)
        reg |= PacketConfig1_AddressFiltering_None;
    else
        reg |= PacketConfig1_AddressFiltering_NodeBC;

    write(RegPacketConfig1, reg);
}


void rf_listen(uint16_t idle, uint16_t rx)
{
    /* Listen mode timings are computed as the product of a coefficient and the
    resolution, the latter being one out of 64us, 4.1ms and 262ms. This gives
    the following duration:

        resolution   coefficient        duration
             64us       0 .. 255         64us   .. 16.32ms
            4.1ms       0 .. 255        4.1ms   ..  1.05s
            262ms       0 .. 255        262ms   .. 66.81s

    Listen mode works as follows:

        - RFIO stays in standby but is periodically woken up to RX for a while.

        - When in RX packet qualification takes place:
            - The transisiton to RX has cleared the FIFO.
            - When the RSSI is sufficient, reception begins.
            - If the sync word is detected, packet reception continues and the
              TimeoutRssiThresh begins. Otherwise the chip returns to sleep.

        - When a packet is qualified:
            - The payload is received and written to the empty FIFO.
            - The CRC is computed.
            - If the requested PayloadLength has been received and the CRC is
              correct the PayloadReady interrupt is asserted.

            - When PayloadReady is asserted the listen mode is resumed and the
              RFIO is sent back to sleep.
            - When the CRC is wrong the FIFO is discarded and listen mode is
              resumed in sleep mode.
            - If TimeoutRssiThresh is exceeded before the payload has been
              completely received listen mode is resumed in sleep mode.

    The above implies that if a payload is ready and it is not read before the
    next packet is qualified it will be lost.
    Also there may be no meaningful RSSI available as the next RSSI phase might
    have commenced already. */
    chmode(OpMode_Mode_Stdby);
    if (!idle || !rx)
        return;

    uint8_t idlecoef, rxcoef;
    uint8_t listen1 = Listen1_ListenCriteria_Sync |
                      Listen1_ListenEnd_Resume;

    if (idle <= 16) {
        if (idle == 0)
            idle = 1;
        listen1 |= Listen1_ListenResolIdle_64us;
        idlecoef = idle * 16;
    }
    else if (idle < 1050) {
        listen1 |= Listen1_ListenResolIdle_4ms;
        idlecoef = idle / 4;
    }
    else {
        listen1 |= Listen1_ListenResolIdle_262ms;
        idlecoef = idle / 262;
    }

    if (rx <= 16) {
        if (rx == 0)
            rx = 1;
        listen1 |= Listen1_ListenResolRx_64us;
        rxcoef = rx * 16;
    }
    else if (rx < 1050) {
        listen1 |= Listen1_ListenResolRx_4ms;
        rxcoef = rx / 4;
    }
    else {
        listen1 |= Listen1_ListenResolRx_262ms;
        rxcoef = rx / 262;
    }

    write(RegListen1, listen1);
    write(RegListen2, idlecoef);
    write(RegListen3, rxcoef);

    /* Enable event for wake up */
    EXTI->EMR |= EXTI_EMR_MR1;
    EXTI->IMR |= EXTI_IMR_MR1;
    EXTI->PR = EXTI_PR_PR1;

    /* Manually set ListenOn as listen mode will change modes, which may
    interfere with the loop in chmode(). */
    afcreset();
    write(RegOpMode, OpMode_Mode_Stdby | OpMode_ListenOn);
}

bool rf_trip(void)
{
    /* NOTE Listen mode -> PayloadReady vanishes */
    return (EXTI->PR & EXTI_PR_PR1);
}


static void recover_auto_mode()
{
    /* Recover from deadlock in auto mode.
    When auto mode is configured the intermediate mode cannot be changed by
    writing RegOpMode as long as the AutoMode bit in RegIrqFlags1 is set. The
    work around is to synthesize a condition to leave the intermediate mode. */
    uint8_t tmp[FIFOSIZE];
    write(RegAutoModes, AutoModes_EnterCondition_FifoNotEmpty |
                        AutoModes_ExitCondition_FifoEmpty |
                        AutoModes_IntermediateMode_Stdby);
    write(RegFifo, 0xFF);
    readfifo(tmp, FIFOSIZE);
    write(RegAutoModes, 0);
}

void rf_sendto(uint8_t to, const uint8_t *msg, uint8_t length)
{
    /* Disable receiver to prevent overwriting FIFO */
    chmode(OpMode_Mode_Stdby);
    flushfifo();

    /* Set up variable packet length */
    if (length > MAXPACK)
        length = MAXPACK;

    write(RegFifoThresh,
        RegFifoThresh_TxStartCondition_Level |
        RegFifoThresh_FifoThreshold_X(length + 1));

    /* Use auto mode to reduce time spent in TX mode to the minimum.
    This is guarded by a timeout in case the auto mode is stuck. */
    write(RegAutoModes,
        AutoModes_IntermediateMode_Tx |
        AutoModes_EnterCondition_FifoLevel |
        AutoModes_ExitCondition_PacketSent);

    /* Send */
    write(RegFifo, length + 2);
    write(RegFifo, to);
    writefifo(msg, length);
    timeout = tot_set(RF_TX_TIMEOUT);
}

bool rf_sent(void)
{
    if ( (read(RegIrqFlags1) & IrqFlags1_AutoMode) ||
         (read(RegIrqFlags2) & IrqFlags2_FifoNotEmpty) ) {
        if (!tot_expired(timeout))
            return false;
        else
            recover_auto_mode();
    }

    write(RegAutoModes, 0);
    chmode(OpMode_Mode_Rx);
    timeout = tot_set(RF_AFC_TIMEOUT);
    return true;
}

uint8_t rf_receive(uint8_t *msg, uint8_t *length)
{
    /* The next RSSI phase will only be entered after the entire packet has been
    read from the FIFO. So conserve the current RSSI. */
    rssi = read(RegRssiValue);

    uint8_t n = read(RegFifo);
    uint8_t rcpt = read(RegFifo);

    if (n < 2) {
        /* Should not happen - malformed packet */
        *length = 0;
    }
    else {
        n -= 2;
        if (n < *length)
            *length = n;
        readfifo(msg, *length);
    }

    timeout = tot_set(RF_AFC_TIMEOUT);
    chmode(OpMode_Mode_Rx);
    return rcpt;
}

bool rf_received(void)
{
    if (GPIOB->IDR & GPIO_IDR_IDR1) {
        if (read(RegIrqFlags2) & IrqFlags2_PayloadReady) {
            chmode(OpMode_Mode_Stdby);
            timeout = tot_set(RF_AFC_TIMEOUT);
            return true;
        }
    }
    else {
        if (tot_expired(timeout)) {
            /* Reset possibly runaway AFC */
            afcreset();
            timeout = tot_set(RF_AFC_TIMEOUT);
        }
    }

    return false;
}

void rf_enable(bool enable)
{
    if (enable) {
        chmode(OpMode_Mode_Rx);
        timeout = tot_set(RF_AFC_TIMEOUT);

        /* Enable event for wake up */
        EXTI->EMR |= EXTI_EMR_MR1;

        /* Enable interrupt to have sticky pending bit */
        EXTI->IMR |= EXTI_IMR_MR1;
        EXTI->PR = EXTI_PR_PR1;
    }
    else {
        chmode(OpMode_Mode_Stdby);
        EXTI->EMR &= ~(EXTI_EMR_MR1 | EXTI_EMR_MR0);
        EXTI->IMR &= ~(EXTI_IMR_MR1 | EXTI_IMR_MR0);
        EXTI->PR = EXTI_PR_PR1 | EXTI_PR_PR0;
    }
}

static void clkout(void)
{
    /* Leave auto mode */
    recover_auto_mode();

    /* Possibly end listen mode */
    write(RegOpMode, OpMode_Mode_Stdby | OpMode_ListenAbort);

    /* Wake up chip */
    chmode(OpMode_Mode_Stdby);

    /* Except when in sleep mode the RFIO chip's GPIO5 pin will always be the
    clock output. */
    do {
        write(RegDioMapping2, DioMapping2_ClkOut_4);
    } while ( (read(RegDioMapping2) & DioMapping2_ClkOut) != DioMapping2_ClkOut_4 );
}

static void init()
{
    const struct {
        uint8_t reg;
        uint8_t val;
    } defaults[] = {
        { RegOpMode,            OpMode_Mode_Stdby },
        { RegAutoModes,         0 },
        { RegDataModul,         DataModul_DataMode_Packet |
                                DataModul_ModType_FSK |
                                DataModul_ModShape_None },

        /* Output current protection limit is calculated according to
            Ilimit = 45mA + 5mA * OcpTrim

        The default is 95mA. */
        { RegOcp,               Ocp_OcpOn |
                                Ocp_OcpTrim_X(95) },

        { RegPaRamp,            PaRamp_PaRamp_40us },

        /* The voltage monitor can be trimmed between 1,695V and 2185V in steps of
        approximately 69mV:
            Utrim = 1695mV + LowBatTrim * 69mV

        By default the monitor is disabled. */
        { RegLowBat,            0 },

        /* Modulation index is computed as
            beta = 2 * Fdev / Bitrate

        Assume a modulation index above 2 and adjust accordingly.
        */
        { RegAfcCtrl,           0 },
        { RegAfcFei,            AfcFei_AfcAutoOn |
                                AfcFei_AfcAutoclearOn },
        { RegTestDagc,          TestDagc_ContinuousDagc_HiBeta },
        { RegLna,               Lna_LnaGainSelect_Agc },
        { RegTestLna,           TestLna_SensitivityNormal },


        { RegPreambleMsb,       0 },
        { RegPreambleLsb,       10 },
        { RegSyncConfig,        SyncConfig_SyncOn |
                                SyncConfig_SyncSize_X(2) |
                                SyncConfig_SyncTol_X(0) },
        { RegSyncValue(0),      0xAA },
        { RegSyncValue(1),      0xAA },
        { RegPacketConfig1,     PacketConfig1_PacketFormat_Variable |
                                PacketConfig1_DcFree_Manchester |
                                PacketConfig1_CrcOn |
                                PacketConfig1_AddressFiltering_NodeBC },
        { RegPacketConfig2,     PacketConfig2_InterPacketRxDelay_X(4) |
                                PacketConfig2_AutoRxRestartOn },
        { RegNodeAdrs,          0 },
        { RegBroadcastAdrs,     0xFF },
        { RegPayloadLength,     MAXPACK + 2 },
        { RegFifoThresh,        RegFifoThresh_TxStartCondition_Level |
                                RegFifoThresh_FifoThreshold_X(MAXPACK + 1) },
        { RegRxTimeout1,        0 },
        { RegRxTimeout2,        MAXPACK * 2 + 5 },

        /* Listen mode */
        { RegListen1,           Listen1_ListenResolIdle_64us |
                                Listen1_ListenResolRx_64us |
                                Listen1_ListenCriteria_Sync |
                                Listen1_ListenEnd_Resume },
        { RegListen2,           1 },
        { RegListen3,           1 },

        /* PayloadReady on DIO0 = PB1 */
        { RegDioMapping1,       DioMapping1_Dio0_0 },
    };

    for (size_t i = 0; i < sizeof(defaults)/sizeof(*defaults); i++)
        write(defaults[i].reg, defaults[i].val);
}

void rf_configure(void)
{
    /* When adjusting RF parameters the following set of constraints must be
    adhered to:

        1.  Modulation index:
                beta = 2 * Fdev / Bitrate
                0.5 < beta < 10

        2.  Frequency deviation:
                600Hz < Fdev < 500kHz - Bitrate/2

        2.  Receiver bandwidths:
                B    > Fdev + Bitrate/2
                Bafc > Fdev + Bitrate / 2 + Foffs
    */
    rf_frequency(config.rf.frequency);
    rf_bitrate(config.rf.bitrate);
    rf_afcbw(config.rf.afcbw);
    rf_rxbw(config.rf.rxbw);
    rf_fdev(config.rf.fdev);
    rf_power(config.rf.power);
    rf_sensitivity(config.rf.sensitivity);

    rf_meshid(config.rf.mesh);
    rf_nodeid(config.rf.node);
}

void rf_prepare(void)
{
    /* Port */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    __DSB();
    GPIOA->ODR |= GPIO_ODR_ODR4;
    GPIOA->CRL &= ~GPIO_CRL_CNF4;                       /* NSS */
    GPIOA->CRL |= GPIO_CRL_MODE4_0 | GPIO_CRL_MODE4_1;

    GPIOA->CRL &= ~GPIO_CRL_CNF5;                       /* SCK */
    GPIOA->CRL |= GPIO_CRL_CNF5_1 | GPIO_CRL_MODE5_0 | GPIO_CRL_MODE5_1;

    GPIOA->CRL &= ~(GPIO_CRL_CNF6_0 | GPIO_CRL_MODE6);  /* MISO */
    GPIOA->CRL |= GPIO_CRL_CNF6_1;
    GPIOA->ODR |= GPIO_ODR_ODR6;

    GPIOA->CRL &= ~GPIO_CRL_CNF7;                       /* MOSI */
    GPIOA->CRL |= GPIO_CRL_CNF7_1 | GPIO_CRL_MODE7_0 | GPIO_CRL_MODE7_1;

    /* Pull-down to prevent floating lines in standby mode */
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    __DSB();
    GPIOB->CRL &= ~(
        GPIO_CRL_CNF0_0 | GPIO_CRL_MODE0 |
        GPIO_CRL_CNF1_0 | GPIO_CRL_MODE1);              /* DIO0..1 */
    GPIOB->CRL |= GPIO_CRL_CNF0_1 | GPIO_CRL_CNF1_1;
    GPIOB->ODR &= ~(GPIO_ODR_ODR0 | GPIO_ODR_ODR1);

    /* Rising-edge interrupts */
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
    __DSB();
    NVIC_DisableIRQ(EXTI0_IRQn);                        /* EXTI0 = PB0 = DIO1 */
    NVIC_DisableIRQ(EXTI1_IRQn);                        /* EXTI1 = PB1 = DIO0 */
    AFIO->EXTICR[0] &= ~(AFIO_EXTICR1_EXTI1 | AFIO_EXTICR1_EXTI0);
    AFIO->EXTICR[0] |= AFIO_EXTICR1_EXTI1_PB | AFIO_EXTICR1_EXTI0_PB;
    EXTI->RTSR |= EXTI_RTSR_TR1 | EXTI_RTSR_TR0;
    EXTI->FTSR &= ~(EXTI_FTSR_TR1 | EXTI_FTSR_TR0);
    EXTI->IMR &= ~(EXTI_IMR_MR1 | EXTI_IMR_MR0);
    EXTI->EMR &= ~(EXTI_EMR_MR1 | EXTI_EMR_MR0);

    /* By now system clock is 8MHz from the HSI. Yet configure the SPI to not
    exceed the RFIO chip's maximum SPI frequency of 10MHz when the system clock
    is switched to HSE. This requires a prescaler of at least 1/16 as SPI1 is
    clocked by the high speed APB2's 72MHz. An SPI frequency of 4.5MHz is
    achievable. */
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    __DSB();
    NVIC_DisableIRQ(SPI1_IRQn);
    SPI1->CR2 = 0;
    SPI1->CR1 =
        SPI_CR1_SSM |
        SPI_CR1_SSI |
        SPI_CR1_SPE |
        SPI_CR1_BR_1 | SPI_CR1_BR_0 |
        SPI_CR1_MSTR;

    clkout();
    init();

    rf_configure();
}

#ifdef DEBUG
#include "server.h"

void rf_debug(void)
{
    uint8_t reg;

    reg = read(RegOpMode);
    srv_printf("OpMode: 0x%x - ", reg);
    switch (reg & OpMode_Mode) {
        case OpMode_Mode_Sleep: srv_printf("sleep"); break;
        case OpMode_Mode_Stdby: srv_printf("standby"); break;
        case OpMode_Mode_Fs: srv_printf("freq syn"); break;
        case OpMode_Mode_Rx: srv_printf("rx"); break;
        case OpMode_Mode_Tx: srv_printf("tx"); break;
        default: srv_printf("?");
    }
    srv_printf( (reg & OpMode_SequencerOff) ? ", sequencer off" : ", sequencer on" );
    srv_printf( (reg & OpMode_ListenOn) ? ", listen on" : "" );

    uint16_t w = ((uint16_t) read(RegFeiMsb) << 8) | read(RegFeiLsb);
    srv_printf("\nFei: %i = %i", w, (int16_t) w * 61);

    w = ((uint16_t) read(RegAfcMsb) << 8) | read(RegAfcLsb);
    srv_printf("\nAfc: %i = %i", w, (int16_t) w * 61);

    srv_printf("\nRssi: %i", rf_rssi());
    srv_printf("\nRssi threshold: -%i", read(RegRssiThresh) / 2);

    reg = read(RegIrqFlags1);
    srv_printf("\nIRQ flags: 0x%x 0x%x", read(RegIrqFlags1), read(RegIrqFlags2));
    if (reg & IrqFlags1_ModeReady) srv_printf("\n+ mode ready");
    if (reg & IrqFlags1_RxReady) srv_printf("\n+ RX ready");
    if (reg & IrqFlags1_TxReady) srv_printf("\n+ TX ready");
    if (reg & IrqFlags1_PllLock) srv_printf("\n+ PLL lock");
    if (reg & IrqFlags1_Rssi) srv_printf("\n+ Rssi");
    if (reg & IrqFlags1_Timeout) srv_printf("\n+ Timeout");
    if (reg & IrqFlags1_AutoMode) srv_printf("\n+ Auto mode");
    if (reg & IrqFlags1_SyncAddressMatch) srv_printf("\n+ Sync match");

    reg = read(RegIrqFlags2);
    if (reg & IrqFlags2_FifoFull) srv_printf("\n+ Fifo full");
    if (reg & IrqFlags2_FifoNotEmpty) srv_printf("\n+ Fifo not empty");
    if (reg & IrqFlags2_FifoLevel) srv_printf("\n+ Fifo level");
    if (reg & IrqFlags2_FifoOverrun) srv_printf("\n+ Fifo overrun");
    if (reg & IrqFlags2_PacketSent) srv_printf("\n+ Packet sent");
    if (reg & IrqFlags2_PayloadReady) srv_printf("\n+ Payload ready");
    if (reg & IrqFlags2_CrcOk) srv_printf("\n+ Crc ok");
    if (reg & IrqFlags2_LowBat) srv_printf("\n+ Low bat");

    srv_printf("\n");
}

#endif
