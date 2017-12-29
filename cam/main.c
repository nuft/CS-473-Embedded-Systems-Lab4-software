#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <io.h>
#include <system.h>
#include "i2c/i2c.h"
#include "camera.h"

/* I2C defines */
#define I2C_FREQ    (50000000) /* Clock frequency driving the i2c core: 50 MHz in this example (ADAPT TO YOUR DESIGN) */
#define I2C_BASE    I2C_0_BASE

#define TEST 1

#define IMAGE_ADDR ((void *)HPS_0_BRIDGES_BASE)
//uint16_t image_buffer[IMAGE_HEIGHT*IMAGE_WIDTH];

void delay(uint64_t n)
{
    while (n-- > 0) {
        asm volatile ("nop");
    }
}

bool dump_image(const void *addr)
{
    const char* filename = "/mnt/host/image.ppm";
    FILE *outf = fopen(filename, "w");

    if (!outf) {
        printf("Error: could not open \"%s\" for writing\n", filename);
        return false;
    }

    // PPM header
    fprintf(outf, "P3\n320 240\n255\n");

    for (unsigned lin = 0; lin < 240; lin++) {
        for (unsigned col = 0; col < 320; col++) {
            uint16_t pixel = IORD_16DIRECT((uintptr_t)addr + 320 * lin + col, 0);
            uint8_t r = (uint8_t)((pixel >> 11) & 0b11111)<<3;
            uint8_t g = (uint8_t)((pixel >> 5) & 0b111111)<<2;
            uint8_t b = (uint8_t)(pixel & 0b11111)<<3;
            fprintf(outf, "%hhu %hhu %hhu  ", r, g, b);
        }
        fprintf(outf, "\n");
    }

    fclose(outf);
    return true;
}

int main(void)
{
	void *image_buffer = IMAGE_ADDR;

    printf("I2C init\n");
    i2c_dev i2c = i2c_inst((void *) I2C_BASE);
    i2c_init(&i2c, I2C_FREQ);

    /* clear image buffer */
    memset(image_buffer, 0, IMAGE_SIZE);

    /* Camera reset cycle */
    printf("Camera reset\n");
    camera_disable();
    delay(1000000);
    camera_enable();

#if TEST
    printf("Camera setup\n");
    camera_setup(&i2c, image_buffer, NULL, NULL);
    camera_dump_regs();
    camera_disable();

#else
    printf("Camera setup\n");
    camera_setup(&i2c, image_buffer, NULL, NULL);

    /* Wait until done*/
    printf("Camera wait for image... ");
    while(!camera_image_received());
    printf("DONE");
    camera_clear_irq_flag();
    camera_disable();
    printf("Camera disable\n");

    printf("Dump image\n");
    dump_image(image_buffer);
#endif

    while (1) {
        ;
    }
}
