// (c)2024, Arthur van Hoff, Artfahrt Inc.
//
// Nikla Vision
//
// Double tap reset button to (re)enable uploads
// Use Examples/STM32H747_System/QSPIFormat to format wifi and user partitions
// Use Examples/STM32H747_System/QSPIFReadPartitions to list partitions
// Use Examples/STM32H747_System/WiFiFirmwareUpdater to install wifi firmware
//

#define MOTOR_ADDR        0x3
#define MOTOR_CMD_EJECT   1

#define EJECT_OK          1
#define EJECT_FAILED      2
#define HOPPER_EMPTY      3
#define HOPPER_STUCK      4
#define CMD_TIMEOUT       999

#define CARDSUIT_COL      18
#define CARDSUIT_ROW      4
#define CARDSUIT_NCOLS    14
#define CARDSUIT_NROWS    4

#define CARDSUIT_WIDTH        25
#define CARDSUIT_HEIGHT       64

#define r565(v)     ((v) & 0x00F8)
#define g565(v)     ((((v) & 0x0007) << 5) | (((v) & 0xE000) >> 11))
#define b565(v)     (((v) & 0x1F00) >> 5)
