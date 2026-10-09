#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>

/* Set to 1 if clockwise rotation decreases the displayed angle. */
#define ENCODER_REVERSE 0

void Encoder_Init(void);
/* Called from the 1 ms SysTick interrupt, including while I2C is busy. */
void Encoder_Update(void);
int8_t Encoder_GetStep(void);
int32_t Encoder_GetCount(void);

#endif
