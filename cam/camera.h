#ifndef CAMERA_H
#define CAMERA_H

#include <stdint.h>
#include "i2c/i2c.h"

#define IMAGE_HEIGHT    240
#define IMAGE_WIDTH     320
#define IMAGE_SIZE 		(2*IMAGE_HEIGHT*IMAGE_WIDTH)

void camera_setup(i2c_dev *i2c, uint16_t *buf, void (*isr)(void *), void *isr_arg);
void camera_enable(void);
void camera_disable(void);
void camera_enable_receive(void);
void camera_disable_receive(void);
void camera_enable_interrupt(void);
void camera_disable_interrupt(void);
bool camera_image_received(void);
void camera_clear_irq_flag(void);
void camera_set_frame_buffer(uint16_t *buf);
uint16_t *camera_get_frame_buffer(void);
void camera_dump_regs(void);

#endif /* CAMERA_H */
