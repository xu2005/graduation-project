#ifndef KEY_H
#define KEY_H

#include <stdbool.h>

void Key_Init(void);
/* Called from the 1 ms SysTick interrupt. */
void Key_Update(void);
bool Key_WasPressed(void);

#endif
