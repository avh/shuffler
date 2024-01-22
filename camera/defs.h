// (c)2024, Arthur van Hoff, Artfahrt Inc.
//
// Nikla Vision
//
// Double tap reset button to (re)enable uploads
// Use Examples/STM32H747_System/QSPIFormat to format wifi and user partitions
// Use Examples/STM32H747_System/QSPIFReadPartitions to list partitions
// Use Examples/STM32H747_System/WiFiFirmwareUpdater to install wifi firmware
//
#pragma once

#define SERIAL_BAUDRATE   115200

#define MOTOR_ADDR        0x3
#define MOTOR_CMD_EJECT   1
#define MOTOR_CMD_READY   2

#define EJECT_OK          1
#define EJECT_FAILED      2
#define HOPPER_EMPTY      3
#define HOPPER_STUCK      4
#define CMD_TIMEOUT       999

#define CARDSUIT_COL      8
#define CARDSUIT_ROW      9
#define CARDSUIT_NCOLS    14
#define CARDSUIT_NROWS    4

#define CARDSUIT_WIDTH        21
#define CARDSUIT_HEIGHT       62
#define CARD_WIDTH            CARDSUIT_WIDTH
#define CARD_HEIGHT           31
#define SUIT_WIDTH            CARDSUIT_WIDTH
#define SUIT_HEIGHT           25
#define SUIT_OFFSET           (CARDSUIT_HEIGHT - SUIT_HEIGHT)

#define CARD_MATCH_NONE       -1
#define CARD_MATCH_EMPTY      (4*13)
