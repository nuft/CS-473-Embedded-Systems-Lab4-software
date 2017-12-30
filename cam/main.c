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

#define TEST 0

#define IMAGE_ADDR HPS_0_BRIDGES_BASE

void delay(uint64_t n)
{
    while (n-- > 0) {
        asm volatile ("nop");
    }
}

bool dump_image(const uintptr_t addr)
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
    	printf(".");
        for (unsigned col = 0; col < 320; col++) {
            uint16_t pixel = IORD_16DIRECT(addr + 2*(320 * lin + col), 0);
            uint8_t r = (uint8_t)((pixel >> 11) & 0b11111)<<3;
            uint8_t g = (uint8_t)((pixel >> 5) & 0b111111)<<2;
            uint8_t b = (uint8_t)(pixel & 0b11111)<<3;
            fprintf(outf, "%hhu %hhu %hhu  ", r, g, b);
        }
        fprintf(outf, "\n");
    }
    printf("\n");
    fclose(outf);
    return true;
}

#define IMAGE_DEFAULT_VAL 0xdead

void compare_image_to_default(uint16_t default_value)
{
    /* compare image buffer */
    for (unsigned i = 0; i < IMAGE_SIZE/2; i++) {
    	uint16_t pix = IORD_16DIRECT(IMAGE_ADDR, 2*i);
    	if (pix != default_value) {
    		printf("difference found at image[%u] = %x\n", i, pix);
    		return;
    	}
    }
}

int main(void)
{
    printf("I2C init\n");
    i2c_dev i2c = i2c_inst((void *) I2C_BASE);
    i2c_init(&i2c, I2C_FREQ);

    /* clear image buffer */
    for (unsigned i = 0; i < IMAGE_SIZE/2; i++) {
    	IOWR_16DIRECT(IMAGE_ADDR, 2*i, IMAGE_DEFAULT_VAL);
    }

    /* Camera reset cycle */
    printf("Camera reset\n");
    camera_disable();
    delay(1000000);
    camera_enable();
    delay(1000000);

#if TEST
    printf("Camera setup\n");
    camera_setup(&i2c, (void *)IMAGE_ADDR, NULL, NULL);
    camera_dump_regs();
    camera_disable();

#else
    printf("Camera setup\n");
    camera_setup(&i2c, (void *)IMAGE_ADDR, NULL, NULL);

	while (1) {
	    /* Wait until done*/
	    printf("Camera wait for image... ");
	    while(!camera_image_received());
	    printf("DONE\n");
	    camera_clear_irq_flag();
	    // camera_disable();
	    // printf("Camera disable\n");

	    compare_image_to_default(IMAGE_DEFAULT_VAL);
	    printf("pixel[0]: %x\n", IORD_16DIRECT(IMAGE_ADDR, 0));
	    printf("pixel[1]: %x\n", IORD_16DIRECT(IMAGE_ADDR + 2, 0));

	    // printf("some pixel: %x\n", IORD_16DIRECT(IMAGE_ADDR + 2*(320 * 100 + 100), 0));

	    // printf("Dump image...\n");
	    // dump_image(IMAGE_ADDR);
	    // printf("DONE\n");
	}
#endif

    while (1) {
        ;
    }
}
