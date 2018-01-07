#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#include <io.h>
#include <system.h>
#include "i2c/i2c.h"
#include "camera.h"

/* I2C defines */
#define I2C_FREQ    (50000000) /* Clock frequency driving the i2c core: 50 MHz in this example (ADAPT TO YOUR DESIGN) */
#define I2C_BASE    I2C_0_BASE

#define TEST 0

#define IMAGE_ADDR HPS_0_BRIDGES_BASE
#define IMAGE1 IMAGE_ADDR
#define IMAGE2 (IMAGE1 + IMAGE_SIZE)
#define IMAGE3 (IMAGE2 + IMAGE_SIZE)

void delay(uint64_t n)
{
    while (n-- > 0) {
        asm volatile ("nop");
    }
}

bool dump_image(const uint16_t *addr)
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
            uint16_t pixel = IORD_16DIRECT(addr, 2*(320 * lin + col));
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

bool compare_image_to_default(uint16_t *image, uint16_t default_value)
{
    /* compare image buffer */
    for (unsigned i = 0; i < IMAGE_SIZE/2; i++) {
        uint16_t pix = IORD_16DIRECT(image, 2*i);
        if (pix != default_value) {
            printf("difference found at image[%u] = %x\n", i, pix);
            return true;
        }
    }
    return false;
}

void clear_image_buffer(uint16_t *addr, uint16_t fill)
{
    for (unsigned i = 0; i < IMAGE_SIZE/2; i++) {
        IOWR_16DIRECT(addr, 2*i, fill);
    }
}

uint16_t get_pixel_xy(uint16_t *image, unsigned x, unsigned y)
{
    return IORD_16DIRECT(image, 2*(x + IMAGE_WIDTH*y));
}

void print_image_xy(uint16_t *image, unsigned x0, unsigned y0, unsigned dx, unsigned dy)
{
    for (unsigned y = 0; y < dy; y++) {
        for (unsigned x = 0; x < dx; x++) {
            printf("%04x ", get_pixel_xy(image, x0+x, y0+y));
        }
        printf("\n");
    }
}

uint16_t *next_image = NULL;
uint16_t *last_image = NULL;
volatile bool image_received = false;
void camera_interrupt(void *arg)
{
    (void) arg;
    camera_disable_receive();

    last_image = camera_get_frame_buffer();
    camera_set_frame_buffer(next_image);
    image_received = true;
    camera_clear_irq_flag();

    printf("\nCAMERA INTERRUPT\n");
}

int main(void)
{
    printf("I2C init\n");
    i2c_dev i2c = i2c_inst((void *) I2C_BASE);
    i2c_init(&i2c, I2C_FREQ);

    uint16_t *image1 = (uint16_t *)IMAGE1;
    uint16_t *image2 = (uint16_t *)IMAGE2;

    /* Point somewhere else during camera setup */
    camera_set_frame_buffer(image1);
    camera_disable_receive();

    /* Camera reset cycle */
    printf("Camera reset\n");
    camera_disable();
    delay(1000000);
    camera_enable();
    delay(1000000);


    printf("Camera setup\n");
    camera_setup(&i2c, image1, camera_interrupt, NULL);

    camera_dump_regs();

    while (1) {
        clear_image_buffer(image1, IMAGE_DEFAULT_VAL);

        next_image = image1;
        camera_enable_receive();

        /* Wait until done*/
        printf("Camera wait for image... ");
        while(!image_received);
        image_received = false;
        printf("DONE\n");

        compare_image_to_default(last_image, IMAGE_DEFAULT_VAL);

        /* debug info */
        print_image_xy(image1, 0, 0, 32, 2);
    }
}
