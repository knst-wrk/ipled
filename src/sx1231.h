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

#ifndef SX1231_H
#define SX1231_H

/* FIFO access */
#define RegFifo                                 0x00

/* FIFO threshold */
#define RegFifoThresh                           0x3C
#define RegFifoThresh_TxStartCondition_Level    0x00
#define RegFifoThresh_TxStartCondition_NotEmpty 0x80

#define RegFifoThresh_FifoThreshold             0x7F
#define RegFifoThresh_FifoThreshold_X(x)        ( (x) & RegFifoThresh_FifoThreshold )

/* Operating modes of the transceiver */
#define RegOpMode                               0x01
#define OpMode_SequencerOff                     0x80

#define OpMode_ListenOn                         0x40
#define OpMode_ListenAbort                      0x20

#define OpMode_Mode                             0x1C
#define OpMode_Mode_Sleep                       0x00
#define OpMode_Mode_Stdby                       0x04
#define OpMode_Mode_Fs                          0x08
#define OpMode_Mode_Tx                          0x0C
#define OpMode_Mode_Rx                          0x10

/* Automatic mode transition */
#define RegAutoModes                            0x3B

#define AutoModes_EnterCondition_None           0x00
#define AutoModes_EnterCondition_FifoNotEmpty   0x20
#define AutoModes_EnterCondition_FifoLevel      0x40
#define AutoModes_EnterCondition_CrcOk          0x60
#define AutoModes_EnterCondition_PayloadReady   0x80
#define AutoModes_EnterCondition_SyncAddress    0xA0
#define AutoModes_EnterCondition_PacketSent     0xC0
#define AutoModes_EnterCondition_FifoEmpty      0xE0

#define AutoModes_ExitCondition_None            0x00
#define AutoModes_ExitCondition_FifoEmpty       0x04
#define AutoModes_ExitCondition_FifoLevel       0x08
#define AutoModes_ExitCondition_CrcOk           0x0C
#define AutoModes_ExitCondition_PayloadReady    0x10
#define AutoModes_ExitCondition_SyncAddress     0x14
#define AutoModes_ExitCondition_PacketSent      0x18
#define AutoModes_ExitCondition_Timeout         0x1C

#define AutoModes_IntermediateMode              0x03
#define AutoModes_IntermediateMode_Sleep        0x00
#define AutoModes_IntermediateMode_Stdby        0x01
#define AutoModes_IntermediateMode_Rx           0x02
#define AutoModes_IntermediateMode_Tx           0x03


/* Data mode */
#define RegDataModul                            0x02
#define DataModul_DataMode                      0x60
#define DataModul_DataMode_Packet               0x00
#define DataModul_DataMode_ContSync             0x40
#define DataModul_DataMode_Cont                 0x60

#define DataModul_ModType                       0x18
#define DataModul_ModType_FSK                   0x00
#define DataModul_ModType_OOK                   0x08

#define DataModul_ModShape                      0x03
#define DataModul_ModShape_None                 0x00
#define DataModul_ModShape_BT10                 0x01
#define DataModul_ModShape_BT05                 0x02
#define DataModul_ModShape_BT02                 0x03

/* Bit rate */
#define RegBitrateMsb                           0x03
#define RegBitrateLsb                           0x04

/* Frequency deviation for FSK */
#define RegFdevMsb                              0x05
#define RegFdevLsb                              0x06

/* RF frequency (updated upon writing RegFrfLsb) */
#define RegFrfMsb                               0x07
#define RegFrfMid                               0x08
#define RegFrfLsb                               0x09

/* Oscillator calibration */
#define RegOsc1                                 0x0A
#define RegOsc1_RcCalStart                      0x80
#define RegOsc1_RcCalDone                       0x40

/* Supply voltage monitor */
#define RegLowBat                               0x0C
#define LowBat_LowBatMonitor                    0x10
#define LowBat_LowBatOn                         0x08
#define LowBat_LowBatTrim                       0x07
#define LowBat_LowBatTrim_X(x)                  ( (((x) - 1695) / 69) & LowBat_LowBatTrim )


/* Listen mode configuration */
#define RegListen1                              0x0D
#define Listen1_ListenResolIdle                 0xC0
#define Listen1_ListenResolIdle_64us            0x40
#define Listen1_ListenResolIdle_4ms             0x80
#define Listen1_ListenResolIdle_262ms           0xC0

#define Listen1_ListenResolRx                   0x30
#define Listen1_ListenResolRx_64us              0x10
#define Listen1_ListenResolRx_4ms               0x20
#define Listen1_ListenResolRx_262ms             0x30

#define Listen1_ListenCriteria_Sync             0x08

#define Listen1_ListenEnd                       0x06
#define Listen1_ListenEnd_Stop                  0x00
#define Listen1_ListenEnd_ModeStop              0x02
#define Listen1_ListenEnd_Resume                0x04

/* Listen mode idle coefficient */
#define RegListen2                              0x0E

/* Listen mode RX coefficient */
#define RegListen3                              0x0F


/* Chip and mask versions */
#define RegVersion                              0x10

#define Version_2a                              0x21
#define Version_2b                              0x22
#define Version_2c                              0x23



/* Transmitter amplifier power and ramp */
#define RegPaLevel                              0x11
#define PaLevel_Pa0On                           0x80
#define PaLevel_Pa1On                           0x40
#define PaLevel_Pa2On                           0x20
#define PaLevel_OutputPower                     0x1F

#define RegPaRamp                               0x12
#define PaRamp_PaRamp                           0x0F
#define PaRamp_PaRamp_3_4ms                     0x00
#define PaRamp_PaRamp_2ms                       0x01
#define PaRamp_PaRamp_1ms                       0x02
#define PaRamp_PaRamp_500us                     0x03
#define PaRamp_PaRamp_250us                     0x04
#define PaRamp_PaRamp_125us                     0x05
#define PaRamp_PaRamp_100us                     0x06
#define PaRamp_PaRamp_62us                      0x07
#define PaRamp_PaRamp_50us                      0x08
#define PaRamp_PaRamp_40us                      0x09
#define PaRamp_PaRamp_31us                      0x0A
#define PaRamp_PaRamp_25us                      0x0B
#define PaRamp_PaRamp_20us                      0x0C
#define PaRamp_PaRamp_15us                      0x0D
#define PaRamp_PaRamp_12us                      0x0E
#define PaRamp_PaRamp_10us                      0x0F

/* Power amplifier current protection */
#define RegOcp                                  0x13
#define Ocp_OcpOn                               0x10
#define Ocp_OcpTrim                             0x0F
#define Ocp_OcpTrim_X(x)                        ( (((x) - 45) / 5) & Ocp_OcpTrim )

/* Low noise amplifier block */
#define RegLna                                  0x18
#define Lna_LnaZin                              0x80
#define Lna_LnaCurrentGain                      0x38

#define Lna_LnaGainSelect                       0x07
#define Lna_LnaGainSelect_Agc                   0x00
#define Lna_LnaGainSelect_Max                   0x01
#define Lna_LnaGainSelect_6dB                   0x02
#define Lna_LnaGainSelect_12dB                  0x03
#define Lna_LnaGainSelect_24dB                  0x04
#define Lna_LnaGainSelect_36dB                  0x05
#define Lna_LnaGainSelect_48dB                  0x06

/* Low noise amplifier sensitivity boost */
#define RegTestLna                              0x58
#define TestLna_SensitivityBoost                0x2D
#define TestLna_SensitivityNormal               0x1B

/* Fading margin improvement */
#define RegTestDagc                             0x6F
#define TestDagc_ContinuousDagc_Normal          0x00
#define TestDagc_ContinuousDagc_HiBeta          0x30
#define TestDagc_ContinuousDagc_LoBeta          0x20


/* Receiver bandwith */
#define RegRxBw                                 0x19
#define RxBw_DccFreq                            0xE0
#define RxBw_DccFreq_16                         0x00
#define RxBw_DccFreq_8                          0x20
#define RxBw_DccFreq_4                          0x40
#define RxBw_DccFreq_2                          0x60
#define RxBw_DccFreq_1                          0x80
#define RxBw_DccFreq_05                         0xA0
#define RxBw_DccFreq_025                        0xC0
#define RxBw_DccFreq_0125                       0xE0

#define RxBw_RxBwMant                           0x18
#define RxBw_RxBwMant_16                        0x00
#define RxBw_RxBwMant_20                        0x08
#define RxBw_RxBwMant_24                        0x10

#define RxBw_RxBwExp                            0x07
#define RxBw_RxBwExp_X(x)                       ( (x) & RxBw_RxBwExp )

/* Alternate receiver bandwidth for automatic frequency correction */
#define RegAfcBw                                0x1A
/* Same bits as in RegRxBw */

/* Frequency error measurement */
#define RegAfcFei                               0x1E
#define AfcFei_AfcStart                         0x01
#define AfcFei_AfcClear                         0x02
#define AfcFei_AfcAutoOn                        0x04
#define AfcFei_AfcAutoclearOn                   0x08
#define AfcFei_AfcDone                          0x10
#define AfcFei_FeiStart                         0x20
#define AfcFei_FeiDone                          0x40

/* Automatic frequency correction */
#define RegAfcCtrl                              0x0B
#define AfcCtrl_AfcLowBetaOn                    0x20

/* Frequency correction offset */
#define RegTestAfc                              0x71

/* Frequency error indicator */
#define RegFeiMsb                               0x21
#define RegFeiLsb                               0x22

/* Automatic frequency correction offset */
#define RegAfcMsb                               0x1F
#define RegAfcLsb                               0x20

/* Received signal strength indicator */
#define RegRssiConfig                           0x23
#define RssiConfig_RssiStart                    0x01
#define RssiConfig_RssiDone                     0x02

#define RegRssiValue                            0x24
#define RegRssiThresh                           0x29
#define RegRssiThresh_X(x)                      ( - (x) * 2 )

/* Mapping of pins DIO0 to DIO3 */
#define RegDioMapping1                          0x25
#define DioMapping1_Dio0                        0xC0
#define DioMapping1_Dio0_1                      0x80
#define DioMapping1_Dio0_0                      0x40

#define DioMapping1_Dio1                        0x30
#define DioMapping1_Dio1_1                      0x20
#define DioMapping1_Dio1_0                      0x10

#define DioMapping1_Dio2                        0x0C
#define DioMapping1_Dio2_1                      0x08
#define DioMapping1_Dio2_0                      0x04

#define DioMapping1_Dio3                        0x03
#define DioMapping1_Dio3_1                      0x02
#define DioMapping1_Dio3_0                      0x01

/* Mapping of pins DIO4 and DIO5, ClkOut frequency */
#define RegDioMapping2                          0x26
#define DioMapping2_Dio4                        0xC0
#define DioMapping2_Dio4_1                      0x80
#define DioMapping2_Dio4_0                      0x40

#define DioMapping2_Dio5                        0x30
#define DioMapping2_Dio5_1                      0x20
#define DioMapping2_Dio5_0                      0x10

#define DioMapping2_ClkOut                      0x03
#define DioMapping2_ClkOut_1                    0x00
#define DioMapping2_ClkOut_2                    0x01
#define DioMapping2_ClkOut_4                    0x02
#define DioMapping2_ClkOut_8                    0x03
#define DioMapping2_ClkOut_16                   0x04
#define DioMapping2_ClkOut_32                   0x05
#define DioMapping2_ClkOut_RC                   0x06
#define DioMapping2_ClkOut_Off                  0x07

/* Status register #1 */
#define RegIrqFlags1                            0x27
#define IrqFlags1_ModeReady                     0x80
#define IrqFlags1_RxReady                       0x40
#define IrqFlags1_TxReady                       0x20
#define IrqFlags1_PllLock                       0x10
#define IrqFlags1_Rssi                          0x08
#define IrqFlags1_Timeout                       0x04
#define IrqFlags1_AutoMode                      0x02
#define IrqFlags1_SyncAddressMatch              0x01

/* Status register #2 */
#define RegIrqFlags2                            0x28
#define IrqFlags2_FifoFull                      0x80
#define IrqFlags2_FifoNotEmpty                  0x40
#define IrqFlags2_FifoLevel                     0x20
#define IrqFlags2_FifoOverrun                   0x10
#define IrqFlags2_PacketSent                    0x08
#define IrqFlags2_PayloadReady                  0x04
#define IrqFlags2_CrcOk                         0x02
#define IrqFlags2_LowBat                        0x01

/* Preamble length */
#define RegPreambleMsb                          0x2C
#define RegPreambleLsb                          0x2D

/* Synchronization word */
#define RegSyncConfig                           0x2E
#define SyncConfig_SyncOn                       0x80
#define SyncConfig_FifoFillCondition            0x40
#define SyncConfig_SyncSize                     0x38
#define SyncConfig_SyncSize_X(x)                ( ((x) << 3) & SyncConfig_SyncSize )
#define SyncConfig_SyncTol                      0x03
#define SyncConfig_SyncTol_X(x)                 ( (x) & SyncConfig_SyncTol )

#define RegSyncValue(n)                         ( 0x2F + ((n) & 0x07) )

/* Packet frame configuration #1 */
#define RegPacketConfig1                        0x37
#define PacketConfig1_PacketFormat_Fixed        0x00
#define PacketConfig1_PacketFormat_Variable     0x80

#define PacketConfig1_DcFree                    0x60
#define PacketConfig1_DcFree_None               0x00
#define PacketConfig1_DcFree_Manchester         0x20
#define PacketConfig1_DcFree_Whitening          0x40

#define PacketConfig1_CrcOn                     0x10
#define PacketConfig1_CrcAutoClearOff           0x08

#define PacketConfig1_AddressFiltering          0x06
#define PacketConfig1_AddressFiltering_None     0x00
#define PacketConfig1_AddressFiltering_Node     0x02
#define PacketConfig1_AddressFiltering_NodeBC   0x04

/* Packet frame configuration #2 */
#define RegPacketConfig2                        0x3D
#define PacketConfig2_InterPacketRxDelay        0xF0
#define PacketConfig2_InterPacketRxDelay_X(x)   ( ((x) << 4) & PacketConfig2_InterPacketRxDelay )

#define PacketConfig2_RestartRx                 0x04
#define PacketConfig2_AutoRxRestartOn           0x02
#define PacketConfig2_AesOn                     0x01

/* Packet node and broadcast addresses */
#define RegNodeAdrs                             0x39
#define RegBroadcastAdrs                        0x3A

/* Payload length, including address byte */
#define RegPayloadLength                        0x38

/* Timeout if RSSI is not reached after enabling receiver */
#define RegRxTimeout1                           0x2A

/* Timeout if payload is not ready after RSSI interrupt */
#define RegRxTimeout2                           0x2B


#define RegAesKey(n)            (0x3E + (n) & 0x0F)

#define RegTemp1                0x4E
#define RegTemp2                0x4F


#define RegTestTcxo             0x59
#define RegTestPLLBw            0x5F



#define RegOokPeak              0x1B
#define RegOokAvg               0x1C
#define RegOokFix               0x1D

#define FIFOSIZE 66

#endif
