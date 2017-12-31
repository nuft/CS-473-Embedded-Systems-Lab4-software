#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <system.h>
#include <sys/alt_irq.h>
#include <io.h>
#include "i2c/i2c.h"

/* Settings */
#define CONFIG_TEST_PATTERN    1
#define CONFIG_MIRROR_ROW      0
#define CONFIG_MIRROR_COL      0
#define CONFIG_PIXCLK_DIV      0 // 0,1,2,4,8,16,32,64 half of effective divider

/* Camera Controller peripheral defines */
#define CAM_BASE CAM_CONTROLLER_0_BASE
#define CAM_CR  (0x00*4)
#define CAM_IMR (0x01*4)
#define CAM_ISR (0x02*4)
#define CAM_IAR (0x03*4)

#define CAM_CR_CON_EN_MASK  0x00000001
#define CAM_CR_CAM_EN_MASK  0x00000002

#define CAM_IMR_IRQ_MASK    0x00000001
#define CAM_ISR_IRQ_MASK    0x00000001

/* TRDB_D5M Camera defines */
#define TRDB_D5M_I2C_ADDRESS  (0xba)

/* Register Map */
#define REG_CHIP_VERSION                0x000   // default: 0x1801
#define REG_ROW_START                   0x001   // default: 0x0036 (54)
#define REG_COLUMN_STAR                 0x002   // default: 0x0010 (16)
#define REG_ROW_SIZE                    0x003   // default: 0x0797 (1943)
#define REG_COLUMN_SIZE                 0x004   // default: 0x0A1F (2591)
#define REG_HORIZONTAL_BLANK            0x005   // default: 0x0000 (0)
#define REG_VERTICAL_BLANK              0x006   // default: 0x0019 (25)
#define REG_OUTPUT_CONTROL              0x007   // default: 0x1F82
#define REG_SHUTTER_WIDTH_UPPER         0x008   // default: 0x0000
#define REG_SHUTTER_WIDTH_LOWER         0x009   // default: 0x0797
#define REG_PIXEL_CLOCK_CONTROL         0x00A   // default: 0x0000
#define REG_RESTART                     0x00B   // default: 0x0000
#define REG_SHUTTER_DELAY               0x00C   // default: 0x0000
#define REG_RESET                       0x00D   // default: 0x0000
#define REG_PLL_CONTROL                 0x010   // default: 0x0050
#define REG_PLL_CONFIG_1                0x011   // default: 0x6404
#define REG_PLL_CONFIG_2                0x012   // default: 0x0000
#define REG_READ_MODE_1                 0x01E   // default: 0x4006
#define REG_READ_MODE_2                 0x020   // default: 0x0007
#define REG_ROW_ADDRESS_MODE            0x022   // default: 0x8000
#define REG_COLUMN_ADDRESS_MODE         0x023   // default: 0x0007
#define REG_GREEN1_GAIN                 0x02B   // default: 0x0007
#define REG_BLUE_GAIN                   0x02C   // default: 0x0004
#define REG_RED_GAIN                    0x02D   // default: 0x0001
#define REG_GREEN2_GAIN                 0x02E   // default: 0x005A
#define REG_GLOBAL_GAIN                 0x035   // default: 0x231D
#define REG_ROW_BLACK_TARGET            0x049   // default: 0xA700
#define REG_ROW_BLACK_DEFAULT_OFFSET    0x04B   // default: 0x0C00
#define REG_TEST_PATTERN_CONTROL        0x0A0   // default: 0x0000
#define REG_TEST_PATTERN_GREEN          0x0A1   // default: 0x0000
#define REG_TEST_PATTERN_RED            0x0A2   // default: 0x0000
#define REG_TEST_PATTERN_BLUE           0x0A3   // default: 0x0000
#define REG_TEST_PATTERN_BAR_WIDTH      0x0A4   // default: 0x0000
#define REG_CHIP_VERSION_ALT            0x0FF   // default: 0x1801

/* Bit position and mask defines */
/* REG_ROW_ADDRESS_MODE */
#define ROW_BIN_POS         4
#define ROW_SKIP_POS        0
/* REG_COLUMN_ADDRESS_MODE */
#define COL_BIN_POS         4
#define COL_SKIP_POS        0
/* REG_OUTPUT_CONTROL */
#define CHIP_ENABLE_MASK    (1<<1)
/* REG_READ_MODE_1 */
#define SNAPSHOT_MASK       (1<<8)
/* REG_READ_MODE_2 */
#define MIRROR_ROW_MASK     (1<<15)
#define MIRROR_COL_MASK     (1<<14)
/* REG_PIXEL_CLOCK_CONTROL */
#define INVERT_PIXCLK_MASK  (1<<15)
#define DIVIDE_PIXCLK_POS   0
/* REG_TEST_PATTERN_CONTROL */
#define TEST_PATTERN_COLOR_FIELD 0
#define TEST_PATTERN_HORIZONTAL_GRADIENT 1
#define TEST_PATTERN_VERTICAL_GRADIENT 2
#define TEST_PATTERN_DIAGONAL 3
#define TEST_PATTERN_CLASSIC 4
#define TEST_PATTERN_MARCHING_1S 5
#define TEST_PATTERN_MONOCHROME_HORIZONTAL_BARS 6
#define TEST_PATTERN_MONOCHROME_VERTICAL_BARS 7
#define TEST_PATTERN_VERTICAL_COLOR_BARS 8

#define TEST_PATTERN_CONTROL_POS 3
#define ENABLE_TEST_PATTERN_MASK (1<<0)


static i2c_dev *_i2c;

static bool write_reg(uint8_t register_offset, uint16_t data)
{
    int success;
    uint8_t byte_data[2] = {(data >> 8) & 0xff, data & 0xff};

    success = i2c_write_array(_i2c, TRDB_D5M_I2C_ADDRESS, register_offset, byte_data, sizeof(byte_data));

    if (success != I2C_SUCCESS) {
        return false;
    } else {
        return true;
    }
}

static uint16_t read_reg(uint8_t register_offset)
{
    int success;
    uint8_t byte_data[2] = {0, 0};

    success = i2c_read_array(_i2c, TRDB_D5M_I2C_ADDRESS, register_offset, byte_data, sizeof(byte_data));

    if (success != I2C_SUCCESS) {
    	printf("ERROR: I2C read\n");
    }

    return ((uint16_t) byte_data[0] << 8) + byte_data[1];
}


void camera_enable(void)
{
    IOWR_32DIRECT(CAM_BASE, CAM_CR, CAM_CR_CAM_EN_MASK | CAM_CR_CON_EN_MASK);
}

void camera_disable(void)
{
    IOWR_32DIRECT(CAM_BASE, CAM_CR, 0);
}

// #define CAM_IC_ID CAM_CONTROLLER_0_IRQ_INTERRUPT_CONTROLLER_ID
// #define CAM_IRQ CAM_CONTROLLER_0_IRQ

#define CAM_IC_ID 0
#define CAM_IRQ 1
/* Setup the camera
 * @note isr can be NULL to disable the interrupt
 */
void camera_setup(i2c_dev *i2c, void *buf, void (*isr)(void *), void *isr_arg)
{
    uint16_t reg;

    _i2c = i2c;

    if (isr != NULL) {
        // ic_id = <MY_IP>_IRQ_INTERRUPT_CONTROLLER_ID
        // irq = <MY_IP>_IRQ
    	alt_ic_isr_register(CAM_IC_ID, CAM_IRQ, isr, isr_arg, NULL);
    	alt_ic_irq_enable(CAM_IC_ID, CAM_IRQ);
        // Enable interrupt
        IOWR_32DIRECT(CAM_BASE, CAM_IMR, CAM_IMR_IRQ_MASK);
    } else {
        // Disable interrupt
        IOWR_32DIRECT(CAM_BASE, CAM_IMR, 0);
    }
    camera_set_frame_buffer(buf);

    // ROW_SIZE = 1919 (R0x03)
    write_reg(REG_ROW_SIZE, 1919);
    // COLUMN_SIZE = 2559 (R0x04)
    write_reg(REG_COLUMN_SIZE, 2559);
    // SHUTTER_WIDTH_LOWER = 3 (R0x09)
    write_reg(REG_SHUTTER_WIDTH_LOWER, 3);
    write_reg(REG_SHUTTER_WIDTH_UPPER, 0);
    // ROW_BIN = 3 (R0x22 [5:4])
    // ROW_SKIP = 3 (R0x22 [2:0])
    write_reg(REG_ROW_ADDRESS_MODE, (3<<ROW_BIN_POS) | (3<<ROW_SKIP_POS));
    // COLUMN_BIN = 3 (R0x23 [5:4])
    // COLUMN_SKIP = 3 (R0x23 [2:0])
    write_reg(REG_COLUMN_ADDRESS_MODE, (3<<COL_BIN_POS) | (3<<COL_SKIP_POS));

    // clear the bit Snapshot in register Read Mode 1 (bit 8 in R0x1E)
    reg = read_reg(REG_READ_MODE_1);
    write_reg(REG_READ_MODE_1, reg & ~SNAPSHOT_MASK);

    // mirror image
    reg = read_reg(REG_READ_MODE_2);
    reg &= ~(MIRROR_ROW_MASK | MIRROR_COL_MASK);
#if CONFIG_MIRROR_ROW
    reg |= MIRROR_ROW_MASK;
#endif
#if CONFIG_MIRROR_COL
    reg |= MIRROR_COL_MASK;
#endif
    write_reg(REG_READ_MODE_2, reg);

    // invert clock
    write_reg(REG_PIXEL_CLOCK_CONTROL, INVERT_PIXCLK_MASK | (CONFIG_PIXCLK_DIV<<DIVIDE_PIXCLK_POS));

#if CONFIG_TEST_PATTERN
    // Test_Pattern_Mode
    write_reg(REG_TEST_PATTERN_CONTROL, ENABLE_TEST_PATTERN_MASK | (TEST_PATTERN_VERTICAL_COLOR_BARS<<TEST_PATTERN_CONTROL_POS));
    write_reg(REG_TEST_PATTERN_RED, 0x080);
    write_reg(REG_TEST_PATTERN_GREEN, 0xfff);
    write_reg(REG_TEST_PATTERN_BLUE, 0xA80);
    write_reg(REG_TEST_PATTERN_BAR_WIDTH, 8);
#endif

    // Chip Enable=1 in Output Control register (bit 2 in R0x07)
    reg = read_reg(REG_OUTPUT_CONTROL);
    write_reg(REG_OUTPUT_CONTROL, reg | CHIP_ENABLE_MASK);
}

bool camera_image_received(void)
{
    if (IORD_32DIRECT(CAM_BASE, CAM_ISR) & CAM_ISR_IRQ_MASK) {
        return true;
    } else {
        return false;
    }
}

void camera_clear_irq_flag(void)
{
    IOWR_32DIRECT(CAM_BASE, CAM_ISR, CAM_ISR_IRQ_MASK);
}

void camera_set_frame_buffer(void *buf)
{
    IOWR_32DIRECT(CAM_BASE, CAM_IAR, (uint32_t)buf);
}

uintptr_t camera_get_frame_buffer(void)
{
    return IORD_32DIRECT(CAM_BASE, CAM_IAR);
}

void camera_dump_regs(void)
{
    printf("CHIP_VERSION = %4hx\n", read_reg(REG_CHIP_VERSION));
    printf("ROW_START = %4hx\n", read_reg(REG_ROW_START));
    printf("COLUMN_STAR = %4hx\n", read_reg(REG_COLUMN_STAR));
    printf("ROW_SIZE = %4hx\n", read_reg(REG_ROW_SIZE));
    printf("COLUMN_SIZE = %4hx\n", read_reg(REG_COLUMN_SIZE));
    printf("HORIZONTAL_BLANK = %4hx\n", read_reg(REG_HORIZONTAL_BLANK));
    printf("VERTICAL_BLANK = %4hx\n", read_reg(REG_VERTICAL_BLANK));
    printf("OUTPUT_CONTROL = %4hx\n", read_reg(REG_OUTPUT_CONTROL));
    printf("SHUTTER_WIDTH_UPPER = %4hx\n", read_reg(REG_SHUTTER_WIDTH_UPPER));
    printf("SHUTTER_WIDTH_LOWER = %4hx\n", read_reg(REG_SHUTTER_WIDTH_LOWER));
    printf("PIXEL_CLOCK_CONTROL = %4hx\n", read_reg(REG_PIXEL_CLOCK_CONTROL));
    printf("RESTART = %4hx\n", read_reg(REG_RESTART));
    printf("SHUTTER_DELAY = %4hx\n", read_reg(REG_SHUTTER_DELAY));
    printf("RESET = %4hx\n", read_reg(REG_RESET));
    printf("PLL_CONTROL = %4hx\n", read_reg(REG_PLL_CONTROL));
    printf("PLL_CONFIG_1 = %4hx\n", read_reg(REG_PLL_CONFIG_1));
    printf("PLL_CONFIG_2 = %4hx\n", read_reg(REG_PLL_CONFIG_2));
    printf("READ_MODE_1 = %4hx\n", read_reg(REG_READ_MODE_1));
    printf("READ_MODE_2 = %4hx\n", read_reg(REG_READ_MODE_2));
    printf("ROW_ADDRESS_MODE = %4hx\n", read_reg(REG_ROW_ADDRESS_MODE));
    printf("COLUMN_ADDRESS_MODE = %4hx\n", read_reg(REG_COLUMN_ADDRESS_MODE));
    printf("GREEN1_GAIN = %4hx\n", read_reg(REG_GREEN1_GAIN));
    printf("BLUE_GAIN = %4hx\n", read_reg(REG_BLUE_GAIN));
    printf("RED_GAIN = %4hx\n", read_reg(REG_RED_GAIN));
    printf("GREEN2_GAIN = %4hx\n", read_reg(REG_GREEN2_GAIN));
    printf("GLOBAL_GAIN = %4hx\n", read_reg(REG_GLOBAL_GAIN));
    printf("ROW_BLACK_TARGET = %4hx\n", read_reg(REG_ROW_BLACK_TARGET));
    printf("ROW_BLACK_DEFAULT_OFFSET = %4hx\n", read_reg(REG_ROW_BLACK_DEFAULT_OFFSET));
    printf("TEST_PATTERN_CONTROL = %4hx\n", read_reg(REG_TEST_PATTERN_CONTROL));
    printf("TEST_PATTERN_GREEN = %4hx\n", read_reg(REG_TEST_PATTERN_GREEN));
    printf("TEST_PATTERN_RED = %4hx\n", read_reg(REG_TEST_PATTERN_RED));
    printf("TEST_PATTERN_BLUE = %4hx\n", read_reg(REG_TEST_PATTERN_BLUE));
    printf("TEST_PATTERN_BAR_WIDTH = %4hx\n", read_reg(REG_TEST_PATTERN_BAR_WIDTH));
    printf("CHIP_VERSION_ALT = %4hx\n", read_reg(REG_CHIP_VERSION_ALT));
}
